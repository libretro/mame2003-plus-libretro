/***************************************************************************

    Namco C71 (TMS320C25 with Namco internal ROM) - fast DSP core

    The instructions, flags, repeat mode, interrupts and timer follow the
    TMS320C2x core of current MAME (Tony La Porta, Quench and others,
    BSD-3-Clause), cycle for cycle. What changes is how it is run:

    - the memory is a table of 256-word pages per space, set up by the
      driver: RAM and ROM pages are read and written directly, only the
      pages without a pointer go through a handler;
    - the internal RAM blocks B0/B1/B2 and the memory mapped registers
      live in the core, B0 moves between data and program space with
      CNFD/CNFP by swapping two page pointers;
    - I/O ports, BIO and the serial port are plain function pointers;
    - one switch on the opcode, the operands decoded once.

    The HOLD pin is not wired on System 22 (MAME returns 0 for it), so
    the hold logic is left out.

***************************************************************************/

#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "state.h"
#include "c71.h"

#define CLK 4	/* 1 machine cycle equals 4 clock ticks: the icount is in clocks */

/* STR0 */
#define ARP_REG		0xe000
#define OV_FLAG		0x1000
#define OVM_FLAG	0x0800
#define INTM_FLAG	0x0200
#define DP_REG		0x01ff
/* STR1 */
#define ARB_REG		0xe000
#define CNF0_REG	0x1000
#define TC_FLAG		0x0800
#define SXM_FLAG	0x0400
#define C_FLAG		0x0200
#define HM_FLAG		0x0040
#define FSM_FLAG	0x0020
#define XF_FLAG		0x0010
#define FO_FLAG		0x0008
#define TXM_FLAG	0x0004
#define PM_REG		0x0003

#define FIXED_STR1	0x0180		/* TMS320C25 */
#define STACK_LIMIT	7			/* 8 levels */

typedef struct
{
	/* page tables: base pointer of the page, or NULL */
	data16_t *drd[256], *dwr[256];
	data16_t *prd[256], *pwr[256];
	data16_t *prd_cnf0, *pwr_cnf0;		/* program page 0xff when CNF0 = 0 */
	c71_read_fn dread;
	c71_write_fn dwrite;
	c71_read_fn ioread[16];
	c71_write_fn iowrite[16];
	int (*bio)(void);
	data16_t (*dr_in)(void);
	void (*dx_out)(data16_t);
	UINT16 idle_start[4], idle_end[4];
	int idle_slots;

	/* on-chip memory */
	data16_t b0[256];
	data16_t b1[256];
	data16_t b2[32];
	data16_t irom[0x1000];
	int configured;
} c71_bus;

typedef struct
{
	UINT16	PREVPC;
	UINT16	PC;
	UINT16	PFC;
	UINT16	STR0, STR1;
	UINT8	IFR;
	UINT8	RPTC;
	UINT32	ACC;
	UINT32	Preg;
	UINT16	Treg;
	UINT16	AR[8];
	UINT16	STACK[8];
	UINT32	ALU;
	UINT32	oldacc;
	UINT16	opcode;
	UINT16	drr, dxr, tim, prd, imr, greg;
	INT32	timerover;
	INT32	memaccess;
	INT32	idle;
	INT32	init_load_addr;
	INT32	external_mem_access;
	int		(*irq_callback)(int irqline);
	c71_bus	*bus;
} c71_regs;

static c71_regs R;
static c71_bus c71_buses[MAX_CPU];
static int dec_cycles;

int c71_icount;

static UINT8 c71_reg_layout[] = {
	C71_PC,  C71_STR0, C71_STR1, C71_IFR, C71_RPTC, -1,
	C71_ACC, C71_PREG, C71_TREG, C71_AR0, C71_AR1, -1,
	C71_AR2, C71_AR3, C71_AR4, C71_AR5, C71_AR6, C71_AR7, -1,
	C71_STK0, C71_STK1, C71_STK2, C71_STK3, C71_STK4, C71_STK5, C71_STK6, C71_STK7, -1,
	C71_DRR, C71_DXR, C71_TIM, C71_PRD, C71_IMR, C71_GREG, 0
};

static UINT8 c71_win_layout[] = {
	 0, 0,80, 5,
	 0, 6,29,15,
	30, 6,50, 1,
	30, 8,50,13,
	 0,22,80, 1,
};


/***************************************************************************
    Configuration (from the driver)
***************************************************************************/

static c71_bus *bus_for(int cpunum)
{
	c71_bus *b = &c71_buses[cpunum];
	if (!b->configured)
	{
		memset(b, 0, sizeof(*b));
		b->configured = 1;
		/* internal ROM at 0x0000-0x0fff (microcomputer mode) */
		{
			int p;
			for (p = 0; p < 0x10; p++)
				b->prd[p] = &b->irom[p << 8];
		}
		/* B0 in data space, B1 at 0x300 */
		b->drd[2] = b->dwr[2] = b->b0;
		b->drd[3] = b->dwr[3] = b->b1;
	}
	return b;
}

void c71_set_internal_rom(int cpunum, const data16_t *rom)
{
	c71_bus *b = bus_for(cpunum);
	memcpy(b->irom, rom, sizeof(b->irom));
}

void c71_map_data(int cpunum, offs_t start, offs_t end, data16_t *base, int writable)
{
	c71_bus *b = bus_for(cpunum);
	offs_t p;
	for (p = start >> 8; p <= (end >> 8); p++)
	{
		b->drd[p] = base ? base + ((p << 8) - start) : NULL;
		b->dwr[p] = (base && writable) ? base + ((p << 8) - start) : NULL;
	}
}

void c71_map_program(int cpunum, offs_t start, offs_t end, data16_t *base, int writable)
{
	c71_bus *b = bus_for(cpunum);
	offs_t p;
	for (p = start >> 8; p <= (end >> 8); p++)
	{
		b->prd[p] = base ? base + ((p << 8) - start) : NULL;
		b->pwr[p] = (base && writable) ? base + ((p << 8) - start) : NULL;
	}
}

void c71_set_data_handlers(int cpunum, c71_read_fn rd, c71_write_fn wr)
{
	c71_bus *b = bus_for(cpunum);
	b->dread = rd;
	b->dwrite = wr;
}

void c71_set_io(int cpunum, int port, c71_read_fn rd, c71_write_fn wr)
{
	c71_bus *b = bus_for(cpunum);
	b->ioread[port & 15] = rd;
	b->iowrite[port & 15] = wr;
}

void c71_set_bio(int cpunum, int (*bio)(void))
{
	bus_for(cpunum)->bio = bio;
}

void c71_set_serial(int cpunum, data16_t (*dr_in)(void), void (*dx_out)(data16_t))
{
	c71_bus *b = bus_for(cpunum);
	b->dr_in = dr_in;
	b->dx_out = dx_out;
}

void c71_set_idle_loop(int cpunum, int slot, UINT16 pc_start, UINT16 pc_end)
{
	c71_bus *b = bus_for(cpunum);
	if (slot < 0 || slot >= 4)
		return;
	b->idle_start[slot] = pc_start;
	b->idle_end[slot] = pc_end;
	if (slot >= b->idle_slots)
		b->idle_slots = slot + 1;
}


/***************************************************************************
    Memory access
***************************************************************************/

/* page 0: memory mapped registers and B2; everything else unmapped */
static data16_t page0_r(offs_t a)
{
	switch (a)
	{
		case 0: return R.drr;
		case 1: return R.dxr;
		case 2: return R.tim;
		case 3: return R.prd;
		case 4: return R.imr;
		case 5: return R.greg;
	}
	if (a >= 0x60 && a < 0x80)
		return R.bus->b2[a - 0x60];
	return 0;
}

static void page0_w(offs_t a, data16_t d)
{
	switch (a)
	{
		case 0: R.drr = d; return;
		case 1: R.dxr = d; return;
		case 2: R.tim = d; return;
		case 3: R.prd = d; return;
		case 4: R.imr = d; return;
		case 5: R.greg = d; return;
	}
	if (a >= 0x60 && a < 0x80)
		R.bus->b2[a - 0x60] = d;
}

static data16_t dread_slow(offs_t a)
{
	if (a < 0x100)
		return page0_r(a);
	if (R.bus->dread)
		return R.bus->dread(a);
	return 0;
}

static void dwrite_slow(offs_t a, data16_t d)
{
	if (a < 0x100)
		page0_w(a, d);
	else if (R.bus->dwrite)
		R.bus->dwrite(a, d);
}

#define DREAD(a)	c71_dread((a) & 0xffff)
#define DWRITE(a,d)	c71_dwrite((a) & 0xffff, (d))

static INLINE data16_t c71_dread(offs_t a)
{
	const data16_t *p = R.bus->drd[a >> 8];
	if (p)
		return p[a & 0xff];
	return dread_slow(a);
}

static INLINE void c71_dwrite(offs_t a, data16_t d)
{
	data16_t *p = R.bus->dwr[a >> 8];
	if (p)
		p[a & 0xff] = d;
	else
		dwrite_slow(a, d);
}

static INLINE data16_t PREAD(offs_t a)
{
	const data16_t *p;
	a &= 0xffff;
	p = R.bus->prd[a >> 8];
	return p ? p[a & 0xff] : 0;
}

static INLINE void PWRITE(offs_t a, data16_t d)
{
	data16_t *p;
	a &= 0xffff;
	p = R.bus->pwr[a >> 8];
	if (p)
		p[a & 0xff] = d;
}

static INLINE data16_t IOREAD(int port)
{
	c71_read_fn f = R.bus->ioread[port];
	return f ? f(port) : 0;
}

static INLINE void IOWRITE(int port, data16_t d)
{
	c71_write_fn f = R.bus->iowrite[port];
	if (f)
		f(port, d);
}


/***************************************************************************
    Flags and helpers (as MAME tms320c2x.cpp)
***************************************************************************/

#define OP_L	(R.opcode & 0xff)
#define OP_H	(R.opcode >> 8)

#define OV		(R.STR0 & OV_FLAG)
#define OVM		(R.STR0 & OVM_FLAG)
#define INTM	(R.STR0 & INTM_FLAG)
#define ARP		((R.STR0 & ARP_REG) >> 13)
#define DP		((R.STR0 & DP_REG) << 7)
#define CNF0	(R.STR1 & CNF0_REG)
#define TC		(R.STR1 & TC_FLAG)
#define SXM		(R.STR1 & SXM_FLAG)
#define CARRY	(R.STR1 & C_FLAG)
#define HM		(R.STR1 & HM_FLAG)
#define PM		(R.STR1 & PM_REG)

#define DMA		(DP | (OP_L & 0x7f))
#define DMApg0	(OP_L & 0x7f)
#define IND		R.AR[ARP]

#define CLR0(flag)	(R.STR0 = (R.STR0 & ~(flag)) | 0x0400)
#define SET0(flag)	(R.STR0 |= (flag) | 0x0400)
#define CLR1(flag)	(R.STR1 = (R.STR1 & ~(flag)) | FIXED_STR1)
#define SET1(flag)	(R.STR1 |= (flag) | FIXED_STR1)

#define ACC_H	((UINT16)(R.ACC >> 16))
#define ACC_L	((UINT16)R.ACC)
#define SET_ACC_H(v)	(R.ACC = (R.ACC & 0x0000ffff) | ((UINT32)(UINT16)(v) << 16))
#define SET_ACC_L(v)	(R.ACC = (R.ACC & 0xffff0000) | (UINT16)(v))
#define ALU_L	((UINT16)R.ALU)
#define ALU_H	((UINT16)(R.ALU >> 16))
#define OLD_H	((UINT16)(R.oldacc >> 16))

static INLINE void MODIFY_DP(int data)
{
	R.STR0 = (R.STR0 & ~DP_REG) | (data & DP_REG) | 0x0400;
}

static INLINE void MODIFY_PM(int data)
{
	R.STR1 = (R.STR1 & ~PM_REG) | (data & PM_REG) | FIXED_STR1;
}

static INLINE void MODIFY_ARP(int data)
{
	R.STR1 = (R.STR1 & ~ARB_REG) | (R.STR0 & ARP_REG) | FIXED_STR1;
	R.STR0 = (R.STR0 & ~ARP_REG) | ((data << 13) & ARP_REG) | 0x0400;
}

static UINT16 reverse_carry_add(UINT16 arg0, UINT16 arg1)
{
	UINT16 result = 0;
	int carry = 0;
	int count;
	for (count = 0; count < 16; count++)
	{
		int sum = (arg0 >> 15) + (arg1 >> 15) + carry;
		result = (result << 1) | (sum & 1);
		carry = sum >> 1;
		arg0 <<= 1;
		arg1 <<= 1;
	}
	return result;
}

static INLINE void MODIFY_AR_ARP_NOLOAD(void)
{
	UINT16 *ar = &R.AR[ARP];
	switch (OP_L & 0x70)
	{
		case 0x00: break;
		case 0x10: (*ar)--; break;
		case 0x20: (*ar)++; break;
		case 0x30: break;
		case 0x40: *ar = reverse_carry_add(*ar, (UINT16)(-R.AR[0])); break;
		case 0x50: *ar -= R.AR[0]; break;
		case 0x60: *ar += R.AR[0]; break;
		case 0x70: *ar += reverse_carry_add(*ar, R.AR[0]); break;
	}
}

static INLINE void MODIFY_AR_ARP(void)
{
	MODIFY_AR_ARP_NOLOAD();
	if (OP_L & 8)
		MODIFY_ARP(OP_L & 7);
}

#define CALCULATE_ADD_CARRY() \
	do { if (R.oldacc > R.ACC) SET1(C_FLAG); else CLR1(C_FLAG); } while (0)

#define CALCULATE_SUB_CARRY() \
	do { if (R.oldacc < R.ACC) CLR1(C_FLAG); else SET1(C_FLAG); } while (0)

static INLINE void CALCULATE_ADD_OVERFLOW(INT32 addval)
{
	if ((INT32)((R.ACC ^ addval) & (R.oldacc ^ R.ACC)) < 0)
	{
		SET0(OV_FLAG);
		if (OVM)
			R.ACC = ((INT32)R.oldacc < 0) ? 0x80000000 : 0x7fffffff;
	}
}

static INLINE void CALCULATE_SUB_OVERFLOW(INT32 subval)
{
	if ((INT32)((R.oldacc ^ subval) & (R.oldacc ^ R.ACC)) < 0)
	{
		SET0(OV_FLAG);
		if (OVM)
			R.ACC = ((INT32)R.oldacc < 0) ? 0x80000000 : 0x7fffffff;
	}
}

static INLINE UINT16 POP_STACK(void)
{
	UINT16 data = R.STACK[STACK_LIMIT];
	int i;
	for (i = STACK_LIMIT; i > 0; i--)
		R.STACK[i] = R.STACK[i - 1];
	return data;
}

static INLINE void PUSH_STACK(UINT16 data)
{
	int i;
	for (i = 0; i < STACK_LIMIT; i++)
		R.STACK[i] = R.STACK[i + 1];
	R.STACK[STACK_LIMIT] = data;
}

static INLINE void SHIFT_Preg_TO_ALU(void)
{
	switch (PM)
	{
		case 0: R.ALU = R.Preg; break;
		case 1: R.ALU = R.Preg << 1; break;
		case 2: R.ALU = R.Preg << 4; break;
		case 3: R.ALU = R.Preg >> 6; if (R.Preg & 0x80000000) R.ALU |= 0xfc000000; break;
	}
}

static INLINE void GETDATA(int shift, int signext)
{
	if (OP_L & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMA;
	R.external_mem_access = (R.memaccess >= 0x800);

	R.ALU = DREAD(R.memaccess);
	if (signext)
		R.ALU = (UINT32)(INT32)(INT16)R.ALU;
	R.ALU <<= shift;

	if (OP_L & 0x80)
		MODIFY_AR_ARP();
}

/* LST/LST1: the ARP is not loaded by the indirect field */
static INLINE void GETDATA_NOARP(void)
{
	if (OP_L & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMA;
	R.external_mem_access = (R.memaccess >= 0x800);

	R.ALU = DREAD(R.memaccess);

	if (OP_L & 0x80)
		MODIFY_AR_ARP_NOLOAD();
}

static INLINE void PUTDATA(UINT16 data)
{
	R.external_mem_access = (R.memaccess >= 0x800);
	if (OP_L & 0x80)
	{
		DWRITE(IND, data);
		MODIFY_AR_ARP();
	}
	else
		DWRITE(DMA, data);
}

static INLINE void PUTDATA_SST(UINT16 data)
{
	if (OP_L & 0x80)
		R.memaccess = IND;
	else
		R.memaccess = DMApg0;
	R.external_mem_access = (R.memaccess >= 0x800);

	if (OP_L & 0x80)
	{
		R.opcode &= 0xfff7;		/* stop ARP changes */
		MODIFY_AR_ARP();
	}
	DWRITE(R.memaccess, data);
}

/* CNFD / CNFP: B0 moves between data 0x200-0x2ff and program 0xff00-0xffff */
static void set_cnf0(int on)
{
	c71_bus *b = R.bus;
	if (on)
	{
		b->prd[0xff] = b->pwr[0xff] = b->b0;
		b->drd[2] = b->dwr[2] = NULL;
	}
	else
	{
		b->prd[0xff] = b->pwr[0xff] = NULL;
		b->drd[2] = b->dwr[2] = b->b0;
	}
}


/***************************************************************************
    Instructions
***************************************************************************/

#define BRANCH_IF(cond) \
	do { if (cond) R.PC = PREAD(R.PC); else R.PC++; MODIFY_AR_ARP(); } while (0)

static void op_ce(void)
{
	switch (OP_L)
	{
		case 0x00: CLR0(INTM_FLAG); break;							/* EINT */
		case 0x01: SET0(INTM_FLAG); break;							/* DINT */
		case 0x02: CLR0(OVM_FLAG); break;							/* ROVM */
		case 0x03: SET0(OVM_FLAG); break;							/* SOVM */
		case 0x04:													/* CNFD */
			if (R.STR1 & CNF0_REG) { set_cnf0(0); CLR1(CNF0_REG); }
			break;
		case 0x05:													/* CNFP */
			if (!(R.STR1 & CNF0_REG)) { set_cnf0(1); SET1(CNF0_REG); }
			break;
		case 0x06: CLR1(SXM_FLAG); break;							/* RSXM */
		case 0x07: SET1(SXM_FLAG); break;							/* SSXM */
		case 0x08: case 0x09: case 0x0a: case 0x0b:					/* SPM */
			MODIFY_PM(OP_L & 3);
			break;
		case 0x0c: CLR1(XF_FLAG); break;							/* RXF (XF pin not wired) */
		case 0x0d: SET1(XF_FLAG); break;							/* SXF */
		case 0x0e: case 0x0f:										/* FORT */
			if (OP_L & 1) SET1(FO_FLAG); else CLR1(FO_FLAG);
			break;
		case 0x14: SHIFT_Preg_TO_ALU(); R.ACC = R.ALU; break;		/* PAC */
		case 0x15:													/* APAC */
			R.oldacc = R.ACC;
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;
		case 0x16:													/* SPAC */
			R.oldacc = R.ACC;
			SHIFT_Preg_TO_ALU();
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;
		case 0x18:													/* SFL */
			R.ALU = R.ACC;
			R.ACC <<= 1;
			if (R.ALU & 0x80000000) SET1(C_FLAG); else CLR1(C_FLAG);
			break;
		case 0x19:													/* SFR */
			R.ALU = R.ACC;
			R.ACC >>= 1;
			if (SXM && (R.ALU & 0x80000000)) R.ACC |= 0x80000000;
			if (R.ALU & 1) SET1(C_FLAG); else CLR1(C_FLAG);
			break;
		case 0x1b:													/* ABS */
			if ((INT32)R.ACC < 0)
			{
				R.ACC = -R.ACC;
				if (R.ACC == 0x80000000)
				{
					SET0(OV_FLAG);
					if (OVM) R.ACC--;
				}
			}
			CLR1(C_FLAG);
			break;
		case 0x1c: PUSH_STACK(ACC_L); break;						/* PUSH */
		case 0x1d: R.ACC = POP_STACK(); break;						/* POP */
		case 0x1e: PUSH_STACK(R.PC); R.PC = 0x001e; break;			/* TRAP */
		case 0x1f: CLR0(INTM_FLAG); R.idle = 1; break;				/* IDLE */
		case 0x20: CLR1(TXM_FLAG); break;							/* RTXM */
		case 0x21: SET1(TXM_FLAG); break;							/* STXM */
		case 0x23:													/* NEG */
			if (R.ACC == 0x80000000)
			{
				SET0(OV_FLAG);
				if (OVM) R.ACC = 0x7fffffff;
			}
			else R.ACC = -R.ACC;
			if (R.ACC) CLR1(C_FLAG); else SET1(C_FLAG);
			break;
		case 0x24: PUSH_STACK(R.PC); R.PC = ACC_L; break;			/* CALA */
		case 0x25: R.PC = ACC_L; break;								/* BACC */
		case 0x26: R.PC = POP_STACK(); break;						/* RET */
		case 0x27: R.ACC = ~R.ACC; break;							/* CMPL */
		case 0x30: CLR1(C_FLAG); break;								/* RC */
		case 0x31: SET1(C_FLAG); break;								/* SC */
		case 0x32: CLR1(TC_FLAG); break;							/* RTC */
		case 0x33: SET1(TC_FLAG); break;							/* STC */
		case 0x34:													/* ROL */
			R.ALU = R.ACC;
			R.ACC <<= 1;
			if (CARRY) R.ACC |= 1;
			if (R.ALU & 0x80000000) SET1(C_FLAG); else CLR1(C_FLAG);
			break;
		case 0x35:													/* ROR */
			R.ALU = R.ACC;
			R.ACC >>= 1;
			if (CARRY) R.ACC |= 0x80000000;
			if (R.ALU & 1) SET1(C_FLAG); else CLR1(C_FLAG);
			break;
		case 0x36: CLR1(FSM_FLAG); break;							/* RFSM */
		case 0x37: SET1(FSM_FLAG); break;							/* SFSM */
		case 0x38: CLR1(HM_FLAG); break;							/* RHM */
		case 0x39: SET1(HM_FLAG); break;							/* SHM */
		case 0x3c: case 0x3d: case 0x3e: case 0x3f: break;			/* CONF: not on the C25 */
		case 0x50: case 0x51: case 0x52: case 0x53:					/* CMPR */
		{
			UINT16 a = R.AR[ARP], b = R.AR[0];
			int t = 0;
			switch (OP_L & 3)
			{
				case 0: t = (a == b); break;
				case 1: t = (a <  b); break;
				case 2: t = (a >  b); break;
				case 3: t = (a != b); break;
			}
			if (t) SET1(TC_FLAG); else CLR1(TC_FLAG);
			break;
		}
		case 0x82: case 0x92: case 0xa2: case 0xb2:					/* NORM */
		case 0xc2: case 0xd2: case 0xe2: case 0xf2:
			if (R.ACC != 0 && (INT32)(R.ACC ^ (R.ACC << 1)) >= 0)
			{
				CLR1(TC_FLAG);
				R.ACC <<= 1;
				MODIFY_AR_ARP();
			}
			else SET1(TC_FLAG);
			break;
		default:
			logerror("C71:  PC = %04x,  Illegal opcode = %04x\n", (R.PC - 1) & 0xffff, R.opcode);
			break;
	}
}

static const UINT8 ce_cycles[256] =
{
/*00*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*10*/ 0,0,0,0,1,1,1,0, 1,1,0,1,1,1,2,3,
/*20*/ 1,1,0,1,2,2,2,1, 0,0,0,0,0,0,0,0,
/*30*/ 1,1,1,1,1,1,1,1, 1,1,0,0,1,1,1,1,
/*40*/ 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*50*/ 1,1,1,1,0,0,0,0, 0,0,0,0,0,0,0,0,
/*60*/ 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*70*/ 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*80*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*90*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*a0*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*b0*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*c0*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*d0*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*e0*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,
/*f0*/ 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0
};

/* main opcode cycles, by high byte (0xce and the Dx long immediates apart) */
static const UINT8 main_cycles[256] =
{
/*00*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*10*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*20*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*30*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*40*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*50*/ 1,1,1,1,1,1,1,1, 3,2,1,1,2,2,2,2,
/*60*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*70*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*80*/ 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
/*90*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*a0*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*b0*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*c0*/ 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
/*d0*/ 1,1,1,1,1,1,1,0, 1,1,1,1,1,1,1,1,
/*e0*/ 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
/*f0*/ 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2
};

/* Dx long immediates (opcode & 0xf0f8) == 0xd000 */
static void op_dx(void)
{
	switch (OP_L)
	{
		case 0:														/* LRLK */
			R.ALU = (UINT16)PREAD(R.PC);
			R.PC++;
			R.AR[OP_H & 7] = ALU_L;
			break;
		case 1:														/* LALK */
			if (SXM) R.ALU = (UINT32)(INT32)(INT16)PREAD(R.PC);
			else     R.ALU = (UINT16)PREAD(R.PC);
			R.PC++;
			R.ALU <<= (OP_H & 0xf);
			R.ACC = R.ALU;
			break;
		case 2:														/* ADLK */
			R.oldacc = R.ACC;
			if (SXM) R.ALU = (UINT32)(INT32)(INT16)PREAD(R.PC);
			else     R.ALU = (UINT16)PREAD(R.PC);
			R.PC++;
			R.ALU <<= (OP_H & 0xf);
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;
		case 3:														/* SBLK */
			R.oldacc = R.ACC;
			if (SXM) R.ALU = (UINT32)(INT32)(INT16)PREAD(R.PC);
			else     R.ALU = (UINT16)PREAD(R.PC);
			R.PC++;
			R.ALU <<= (OP_H & 0xf);
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;
		case 4:														/* ANDK */
			R.oldacc = R.ACC;
			R.ALU = (UINT16)PREAD(R.PC);
			R.PC++;
			R.ALU <<= (OP_H & 0xf);
			R.ACC &= R.ALU;
			break;
		case 5:														/* ORK */
			R.ALU = (UINT16)PREAD(R.PC);
			R.PC++;
			R.ALU <<= (OP_H & 0xf);
			R.ACC |= R.ALU;
			break;
		case 6:														/* XORK */
			R.ALU = PREAD(R.PC);
			R.PC++;
			R.ALU <<= (OP_H & 0xf);
			R.ACC ^= R.ALU;
			break;
		default:
			logerror("C71:  PC = %04x,  Illegal opcode = %04x\n", (R.PC - 1) & 0xffff, R.opcode);
			break;
	}
}

/* every opcode but 0xCExx and the Dx long immediates */
static void op_main(void)
{
	switch (OP_H)
	{
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:	/* ADD */
			R.oldacc = R.ACC;
			GETDATA(OP_H & 0xf, SXM);
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;

		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:	/* SUB */
			R.oldacc = R.ACC;
			GETDATA(OP_H & 0xf, SXM);
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;

		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:	/* LAC */
			GETDATA(OP_H & 0xf, SXM);
			R.ACC = R.ALU;
			break;

		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:	/* LAR */
			GETDATA(0, 0);
			R.AR[OP_H & 7] = ALU_L;
			break;

		case 0x38:																			/* MPY */
			GETDATA(0, 0);
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)R.Treg);
			break;
		case 0x39:																			/* SQRA */
			R.oldacc = R.ACC;
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			GETDATA(0, 0);
			R.Treg = ALU_L;
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)ALU_L);
			break;
		case 0x3a:																			/* MPYA */
			R.oldacc = R.ACC;
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			GETDATA(0, 0);
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)R.Treg);
			break;
		case 0x3b:																			/* MPYS */
			R.oldacc = R.ACC;
			SHIFT_Preg_TO_ALU();
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			GETDATA(0, 0);
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)R.Treg);
			break;
		case 0x3c:																			/* LT */
			GETDATA(0, 0);
			R.Treg = ALU_L;
			break;
		case 0x3d:																			/* LTA */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			R.Treg = ALU_L;
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;
		case 0x3e:																			/* LTP */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			R.Treg = ALU_L;
			SHIFT_Preg_TO_ALU();
			R.ACC = R.ALU;
			break;
		case 0x3f:																			/* LTD */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			R.Treg = ALU_L;
			DWRITE(R.memaccess + 1, ALU_L);
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;

		case 0x40:																			/* ZALH */
			GETDATA(0, 0);
			R.ACC = (UINT32)ALU_L << 16;
			break;
		case 0x41:																			/* ZALS */
			GETDATA(0, 0);
			R.ACC = ALU_L;
			break;
		case 0x42:																			/* LACT */
			GETDATA(R.Treg & 0xf, SXM);
			R.ACC = R.ALU;
			break;
		case 0x43:																			/* ADDC */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			if (CARRY) R.ACC++;
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			if (R.ACC != R.oldacc)
				CALCULATE_ADD_CARRY();
			break;
		case 0x44:																			/* SUBH */
		{
			UINT16 h;
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			h = (UINT16)(ACC_H - ALU_L);
			SET_ACC_H(h);
			if (OLD_H < h)
				CLR1(C_FLAG);
			if ((INT16)((OLD_H ^ ALU_L) & (OLD_H ^ h)) < 0)
			{
				SET0(OV_FLAG);
				if (OVM) SET_ACC_H(((INT16)OLD_H < 0) ? 0x8000 : 0x7fff);
			}
			break;
		}
		case 0x45:																			/* SUBS */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			R.ACC -= ALU_L;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;
		case 0x46:																			/* SUBT */
			R.oldacc = R.ACC;
			GETDATA(R.Treg & 0xf, SXM);
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;
		case 0x47:																			/* SUBC */
			R.oldacc = R.ACC;
			GETDATA(15, SXM);
			R.ACC -= R.ALU;
			if ((INT32)((R.oldacc ^ R.ALU) & (R.oldacc ^ R.ACC)) < 0)
				SET0(OV_FLAG);
			CALCULATE_SUB_CARRY();
			if (R.oldacc >= R.ALU)
			{
				R.ALU = R.ACC;
				R.ACC = (R.ACC << 1) | 1;
			}
			else
			{
				R.ALU = R.ACC;
				R.ACC = R.oldacc << 1;
			}
			break;
		case 0x48:																			/* ADDH */
		{
			UINT16 h;
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			h = (UINT16)(ACC_H + ALU_L);
			SET_ACC_H(h);
			if (OLD_H > h)
				SET1(C_FLAG);
			if ((INT16)((h ^ ALU_L) & (OLD_H ^ h)) < 0)
			{
				SET0(OV_FLAG);
				if (OVM) SET_ACC_H(((INT16)OLD_H < 0) ? 0x8000 : 0x7fff);
			}
			break;
		}
		case 0x49:																			/* ADDS */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;
		case 0x4a:																			/* ADDT */
			R.oldacc = R.ACC;
			GETDATA(R.Treg & 0xf, SXM);
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;
		case 0x4b:																			/* RPT */
			GETDATA(0, 0);
			R.RPTC = (UINT8)R.ALU;
			R.init_load_addr = 2;
			break;
		case 0x4c:																			/* XOR */
			GETDATA(0, 0);
			SET_ACC_L(ACC_L ^ ALU_L);
			break;
		case 0x4d:																			/* OR */
			GETDATA(0, 0);
			SET_ACC_L(ACC_L | ALU_L);
			break;
		case 0x4e:																			/* AND */
			GETDATA(0, 0);
			R.ACC &= R.ALU;
			break;
		case 0x4f:																			/* SUBB */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			if (CARRY == 0) R.ACC--;
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			if (R.ACC != R.oldacc)
				CALCULATE_SUB_CARRY();
			break;

		case 0x50:																			/* LST */
			GETDATA_NOARP();
			R.ALU = (R.ALU & 0xffff0000) | (ALU_L & ~INTM_FLAG);
			R.STR0 &= INTM_FLAG;
			R.STR0 |= ALU_L;
			R.STR0 |= 0x0400;
			break;
		case 0x51:																			/* LST1 */
			GETDATA_NOARP();
			R.STR1 = ALU_L | FIXED_STR1;
			R.STR0 &= ~ARP_REG;
			R.STR0 |= (R.STR1 & ARB_REG);
			break;
		case 0x52:																			/* LDP */
			GETDATA(0, 0);
			MODIFY_DP(R.ALU & 0x1ff);
			break;
		case 0x53:																			/* LPH */
			GETDATA(0, 0);
			R.Preg = (R.Preg & 0x0000ffff) | ((UINT32)ALU_L << 16);
			break;
		case 0x54:																			/* PSHD */
			GETDATA(0, 0);
			PUSH_STACK(ALU_L);
			break;
		case 0x55:																			/* MAR, LARP, NOP */
			if (OP_L & 0x80) MODIFY_AR_ARP();
			break;
		case 0x56:																			/* DMOV */
			GETDATA(0, 0);
			DWRITE(R.memaccess + 1, ALU_L);
			break;
		case 0x57:																			/* BITT */
			GETDATA(0, 0);
			if (R.ALU & (0x8000 >> (R.Treg & 0xf))) SET1(TC_FLAG); else CLR1(TC_FLAG);
			break;
		case 0x58:																			/* TBLR */
			if (R.init_load_addr)
				R.PFC = ACC_L;
			R.ALU = (R.ALU & 0xffff0000) | PREAD(R.PFC);
			if (!(CNF0 && R.PFC >= 0xff00))
				dec_cycles += (1*CLK);
			PUTDATA(ALU_L);
			R.PFC++;
			break;
		case 0x59:																			/* TBLW */
			if (R.init_load_addr)
				R.PFC = ACC_L;
			dec_cycles += (1*CLK);
			GETDATA(0, 0);
			if (R.external_mem_access) dec_cycles += (1*CLK);
			PWRITE(R.PFC, ALU_L);
			R.PFC++;
			break;
		case 0x5a:																			/* SQRS */
			R.oldacc = R.ACC;
			SHIFT_Preg_TO_ALU();
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			GETDATA(0, 0);
			R.Treg = ALU_L;
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)ALU_L);
			break;
		case 0x5b:																			/* LTS */
			R.oldacc = R.ACC;
			GETDATA(0, 0);
			R.Treg = ALU_L;
			SHIFT_Preg_TO_ALU();
			R.ACC -= R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;
		case 0x5c:																			/* MACD */
			R.oldacc = R.ACC;
			if (R.init_load_addr)
			{
				R.PFC = PREAD(R.PC);
				R.PC++;
			}
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			GETDATA(0, 0);
			if ((OP_L & 0x80) || R.init_load_addr)
				DWRITE(R.memaccess + 1, ALU_L);
			R.Treg = ALU_L;
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)PREAD(R.PFC));
			R.PFC++;
			dec_cycles += (2*CLK);
			break;
		case 0x5d:																			/* MAC */
			R.oldacc = R.ACC;
			if (R.init_load_addr)
			{
				R.PFC = PREAD(R.PC);
				R.PC++;
			}
			SHIFT_Preg_TO_ALU();
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			GETDATA(0, 0);
			R.Treg = ALU_L;
			R.Preg = (UINT32)((INT32)(INT16)ALU_L * (INT32)(INT16)PREAD(R.PFC));
			R.PFC++;
			dec_cycles += (2*CLK);
			break;
		case 0x5e: BRANCH_IF(CARRY); break;													/* BC */
		case 0x5f: BRANCH_IF(CARRY == 0); break;											/* BNC */

		case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:	/* SACL */
			R.ALU = R.ACC << (OP_H & 7);
			PUTDATA(ALU_L);
			break;
		case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:	/* SACH */
			R.ALU = R.ACC << (OP_H & 7);
			PUTDATA(ALU_H);
			break;
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:	/* SAR */
			PUTDATA(R.AR[OP_H & 7]);
			break;
		case 0x78: PUTDATA_SST(R.STR0); break;												/* SST */
		case 0x79: PUTDATA_SST(R.STR1); break;												/* SST1 */
		case 0x7a:																			/* POPD */
			R.ALU = (UINT16)POP_STACK();
			PUTDATA(ALU_L);
			break;
		case 0x7b:																			/* ZALR */
			GETDATA(0, 0);
			R.ACC = ((UINT32)ALU_L << 16) | 0x8000;
			break;
		case 0x7c:																			/* SPL */
			SHIFT_Preg_TO_ALU();
			PUTDATA(ALU_L);
			break;
		case 0x7d:																			/* SPH */
			SHIFT_Preg_TO_ALU();
			PUTDATA(ALU_H);
			break;
		case 0x7e: R.AR[ARP] += OP_L; break;												/* ADRK */
		case 0x7f: R.AR[ARP] -= OP_L; break;												/* SBRK */

		case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:	/* IN */
			R.ALU = (R.ALU & 0xffff0000) | IOREAD(OP_H & 0xf);
			PUTDATA(ALU_L);
			break;

		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:	/* BIT */
			GETDATA(0, 0);
			if (R.ALU & (0x8000 >> (OP_H & 0xf))) SET1(TC_FLAG); else CLR1(TC_FLAG);
			break;

		case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
		case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:	/* MPYK */
			R.Preg = (UINT32)((INT32)(INT16)R.Treg * (INT32)((INT16)(R.opcode << 3) >> 3));
			break;

		case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6: case 0xc7:	/* LARK */
			R.AR[OP_H & 7] = OP_L;
			break;
		case 0xc8: case 0xc9: MODIFY_DP(R.opcode & 0x1ff); break;							/* LDPK */
		case 0xca: R.ACC = OP_L; break;														/* LACK, ZAC */
		case 0xcb:																			/* RPTK */
			R.RPTC = OP_L;
			R.init_load_addr = 2;
			break;
		case 0xcc:																			/* ADDK */
			R.oldacc = R.ACC;
			R.ALU = OP_L;
			R.ACC += R.ALU;
			CALCULATE_ADD_OVERFLOW(R.ALU);
			CALCULATE_ADD_CARRY();
			break;
		case 0xcd:																			/* SUBK */
			R.oldacc = R.ACC;
			R.ALU = OP_L;
			R.ACC -= (UINT8)R.ALU;
			CALCULATE_SUB_OVERFLOW(R.ALU);
			CALCULATE_SUB_CARRY();
			break;
		case 0xce: op_ce(); break;
		case 0xcf:																			/* MPYU */
			GETDATA(0, 0);
			R.Preg = (UINT32)ALU_L * (UINT32)R.Treg;
			break;

		case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb: case 0xec: case 0xed: case 0xee: case 0xef:	/* OUT */
			GETDATA(0, 0);
			IOWRITE(OP_H & 0xf, ALU_L);
			break;

		case 0xf0:																			/* BV */
			if (OV) { R.PC = PREAD(R.PC); CLR0(OV_FLAG); }
			else R.PC++;
			MODIFY_AR_ARP();
			break;
		case 0xf1: BRANCH_IF((INT32)R.ACC > 0); break;										/* BGZ */
		case 0xf2: BRANCH_IF((INT32)R.ACC <= 0); break;										/* BLEZ */
		case 0xf3: BRANCH_IF((INT32)R.ACC < 0); break;										/* BLZ */
		case 0xf4: BRANCH_IF((INT32)R.ACC >= 0); break;										/* BGEZ */
		case 0xf5: BRANCH_IF(R.ACC != 0); break;											/* BNZ */
		case 0xf6: BRANCH_IF(R.ACC == 0); break;											/* BZ */
		case 0xf7:																			/* BNV */
			if (OV == 0) R.PC = PREAD(R.PC);
			else { R.PC++; CLR0(OV_FLAG); }
			MODIFY_AR_ARP();
			break;
		case 0xf8: BRANCH_IF(TC == 0); break;												/* BBZ */
		case 0xf9: BRANCH_IF(TC); break;													/* BBNZ */
		case 0xfa: BRANCH_IF(R.bus->bio ? R.bus->bio() : 1);	/* unwired BIO reads high */ break;							/* BIOZ */
		case 0xfb: BRANCH_IF(R.AR[ARP]); break;												/* BANZ */
		case 0xfc:																			/* BLKP */
			if (R.init_load_addr)
			{
				R.PFC = PREAD(R.PC);
				R.PC++;
			}
			R.ALU = PREAD(R.PFC);
			PUTDATA(ALU_L);
			R.PFC++;
			dec_cycles += (2*CLK);
			break;
		case 0xfd:																			/* BLKD */
			if (R.init_load_addr)
			{
				R.PFC = PREAD(R.PC);
				R.PC++;
			}
			R.ALU = DREAD(R.PFC);
			PUTDATA(ALU_L);
			R.PFC++;
			dec_cycles += (1*CLK);
			break;
		case 0xfe:																			/* CALL */
			R.PC++;
			PUSH_STACK(R.PC);
			R.PC = PREAD(R.PC - 1);
			MODIFY_AR_ARP();
			break;
		case 0xff:																			/* B */
			R.PC = PREAD(R.PC);
			MODIFY_AR_ARP();
			break;

		default:	/* 0xd0-0xdf outside the long immediates */
			logerror("C71:  PC = %04x,  Illegal opcode = %04x\n", (R.PC - 1) & 0xffff, R.opcode);
			break;
	}
}


/***************************************************************************
    Interrupts, timer, execution
***************************************************************************/

static int process_IRQs(void)
{
	/* IFR: XINT RINT TINT INT2 INT1 INT0. Not taken if masked, or after EINT */
	if (INTM == 0 && R.opcode != 0xce00 && (R.IFR & R.imr))
	{
		int pending = R.IFR & R.imr;
		PUSH_STACK(R.PC);
		R.idle = 0;
		SET0(INTM_FLAG);

		if (pending & 0x01) { if (R.irq_callback) (*R.irq_callback)(0); R.PC = 0x0002; R.IFR &= ~0x01; }
		else if (pending & 0x02) { if (R.irq_callback) (*R.irq_callback)(1); R.PC = 0x0004; R.IFR &= ~0x02; }
		else if (pending & 0x04) { if (R.irq_callback) (*R.irq_callback)(2); R.PC = 0x0006; R.IFR &= ~0x04; }
		else if (pending & 0x08) { R.PC = 0x0018; R.IFR &= ~0x08; }
		else if (pending & 0x10) { R.drr = R.bus->dr_in ? R.bus->dr_in() : 0xffff; R.PC = 0x001a; R.IFR &= ~0x10; }
		else { if (R.bus->dx_out) R.bus->dx_out(R.dxr); R.PC = 0x001c; R.IFR &= ~0x20; }
		return 3*CLK;
	}
	return 0;
}

static void process_timer(int clocks)
{
	for (;;)
	{
		int preclocks = CLK - R.timerover;
		int ticks;
		if (clocks < preclocks)
		{
			R.timerover += clocks;
			c71_icount -= clocks;
			return;
		}

		ticks = 1 + (clocks - preclocks) / CLK;
		if (ticks <= R.tim)
		{
			c71_icount -= clocks;
			R.timerover = clocks - (ticks - 1) * CLK - preclocks;
			R.tim -= ticks;
			return;
		}

		/* the timer underflows: TINT */
		c71_icount -= preclocks + CLK * R.tim;
		R.timerover = 0;
		R.tim = R.prd;
		R.IFR |= 0x08;
		clocks = process_IRQs();
	}
}

/* one opcode in repeat mode: 0xCExx or the main table (as MAME) */
static INLINE void exec_repeated(void)
{
	if (OP_H == 0xce)
		op_ce();
	else
		op_main();
}

static int in_idle_loop(UINT16 pc)
{
	const c71_bus *b = R.bus;
	int i;
	for (i = 0; i < b->idle_slots; i++)
		if (pc >= b->idle_start[i] && pc <= b->idle_end[i])
			return 1;
	return 0;
}

int c71_execute(int cycles)
{
	c71_icount = cycles;

	if (R.idle && R.IFR && c71_icount > 0)
		c71_icount -= process_IRQs();

	while (R.idle && c71_icount > 0)
		process_timer(c71_icount);

	while (c71_icount > 0)
	{
		dec_cycles = 0;

		if (R.IFR)
			dec_cycles += process_IRQs();

		R.PREVPC = R.PC;

		CALL_MAME_DEBUG;

		R.opcode = PREAD(R.PC);
		R.PC++;

		if (OP_H == 0xce)
		{
			dec_cycles += ce_cycles[OP_L] * CLK;
			op_ce();
		}
		else if ((R.opcode & 0xf0f8) == 0xd000)
		{
			dec_cycles += (OP_L == 7) ? 0 : 2*CLK;
			op_dx();
		}
		else
		{
			dec_cycles += main_cycles[OP_H] * CLK;
			op_main();
		}

		if (R.init_load_addr == 2)		/* repeat the next instruction */
		{
			R.PREVPC = R.PC;
			R.opcode = PREAD(R.PC);
			R.PC++;
			dec_cycles += (1*CLK);

			do {
				dec_cycles += (1*CLK);
				exec_repeated();
				R.init_load_addr = 0;
				R.RPTC--;
			} while (R.RPTC != 0xff);
			R.RPTC = 0;
			R.PFC = R.PC;
			R.init_load_addr = 1;
		}

		process_timer(dec_cycles);

		while (R.idle && c71_icount > 0)
			process_timer(c71_icount);

		/* a polling loop that only other CPUs can end: give up the timeslice */
		if (R.bus->idle_slots && R.PC <= R.PREVPC && in_idle_loop(R.PC) && c71_icount > 0)
			process_timer(c71_icount);
	}

	return cycles - c71_icount;
}


/***************************************************************************
    Interface
***************************************************************************/

void c71_init(void)
{
	int cpu = cpu_getactivecpu();
	c71_bus *b = bus_for(cpu);

	memset(&R, 0, sizeof(R));
	R.bus = b;
	R.STR1 = FIXED_STR1;

	state_save_register_UINT16("c71", cpu, "PC", &R.PC, 1);
	state_save_register_UINT16("c71", cpu, "PREVPC", &R.PREVPC, 1);
	state_save_register_UINT16("c71", cpu, "PFC", &R.PFC, 1);
	state_save_register_UINT16("c71", cpu, "STR0", &R.STR0, 1);
	state_save_register_UINT16("c71", cpu, "STR1", &R.STR1, 1);
	state_save_register_UINT8("c71", cpu, "IFR", &R.IFR, 1);
	state_save_register_UINT8("c71", cpu, "RPTC", &R.RPTC, 1);
	state_save_register_UINT32("c71", cpu, "ACC", &R.ACC, 1);
	state_save_register_UINT32("c71", cpu, "Preg", &R.Preg, 1);
	state_save_register_UINT16("c71", cpu, "Treg", &R.Treg, 1);
	state_save_register_UINT16("c71", cpu, "AR", R.AR, 8);
	state_save_register_UINT16("c71", cpu, "STACK", R.STACK, 8);
	state_save_register_UINT32("c71", cpu, "ALU", &R.ALU, 1);
	state_save_register_UINT32("c71", cpu, "oldacc", &R.oldacc, 1);
	state_save_register_UINT16("c71", cpu, "opcode", &R.opcode, 1);
	state_save_register_UINT16("c71", cpu, "drr", &R.drr, 1);
	state_save_register_UINT16("c71", cpu, "dxr", &R.dxr, 1);
	state_save_register_UINT16("c71", cpu, "tim", &R.tim, 1);
	state_save_register_UINT16("c71", cpu, "prd", &R.prd, 1);
	state_save_register_UINT16("c71", cpu, "imr", &R.imr, 1);
	state_save_register_UINT16("c71", cpu, "greg", &R.greg, 1);
	state_save_register_INT32("c71", cpu, "timerover", &R.timerover, 1);
	state_save_register_INT32("c71", cpu, "memaccess", &R.memaccess, 1);
	state_save_register_INT32("c71", cpu, "idle", &R.idle, 1);
	state_save_register_INT32("c71", cpu, "init_load_addr", &R.init_load_addr, 1);
	state_save_register_INT32("c71", cpu, "external_mem_access", &R.external_mem_access, 1);
	state_save_register_UINT16("c71", cpu, "b0", b->b0, 256);
	state_save_register_UINT16("c71", cpu, "b1", b->b1, 256);
	state_save_register_UINT16("c71", cpu, "b2", b->b2, 32);
}

void c71_reset(void *param)
{
	/* B0 back in data space */
	if (R.STR1 & CNF0_REG)
		set_cnf0(0);

	R.PC = 0;
	R.STR0 |= 0x0600;					/* INTM and unused bit set to 1 */
	R.STR0 &= 0xefff;					/* OV cleared */
	R.STR1 |= 0x0670 | FIXED_STR1;		/* SXM, C, HM, FSM, XF set */
	R.STR1 &= 0xeff0;					/* CNF, FO, TXM, PM cleared */
	R.RPTC = 0;
	R.IFR = 0;

	R.greg = 0;
	R.tim  = 0xffff;
	R.prd  = 0xffff;
	R.imr  = 0xffc0;

	R.idle = 0;
	R.init_load_addr = 1;
}

void c71_exit(void)
{
}

unsigned c71_get_context(void *dst)
{
	if (dst)
		*(c71_regs *)dst = R;
	return sizeof(c71_regs);
}

void c71_set_context(void *src)
{
	if (src)
	{
		R = *(c71_regs *)src;
		/* the CNF0 page swap lives in the bus: keep it in step after a state load */
		if (((R.STR1 & CNF0_REG) != 0) != (R.bus->prd[0xff] == R.bus->b0))
			set_cnf0((R.STR1 & CNF0_REG) != 0);
	}
}

unsigned c71_get_reg(int regnum)
{
	switch (regnum)
	{
		case REG_PC:
		case C71_PC:   return R.PC;
		case REG_SP:
		case C71_STK7: return R.STACK[7];
		case C71_STK6: return R.STACK[6];
		case C71_STK5: return R.STACK[5];
		case C71_STK4: return R.STACK[4];
		case C71_STK3: return R.STACK[3];
		case C71_STK2: return R.STACK[2];
		case C71_STK1: return R.STACK[1];
		case C71_STK0: return R.STACK[0];
		case C71_PFC:  return R.PFC;
		case C71_STR0: return R.STR0;
		case C71_STR1: return R.STR1;
		case C71_IFR:  return R.IFR;
		case C71_RPTC: return R.RPTC;
		case C71_ACC:  return R.ACC;
		case C71_PREG: return R.Preg;
		case C71_TREG: return R.Treg;
		case C71_AR0: case C71_AR1: case C71_AR2: case C71_AR3:
		case C71_AR4: case C71_AR5: case C71_AR6: case C71_AR7:
			return R.AR[regnum - C71_AR0];
		case C71_DRR:  return R.drr;
		case C71_DXR:  return R.dxr;
		case C71_TIM:  return R.tim;
		case C71_PRD:  return R.prd;
		case C71_IMR:  return R.imr;
		case C71_GREG: return R.greg;
		case REG_PREVIOUSPC: return R.PREVPC;
	}
	return 0;
}

void c71_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
		case REG_PC:
		case C71_PC:   R.PC = val; break;
		case REG_SP:
		case C71_STK7: R.STACK[7] = val; break;
		case C71_STK6: R.STACK[6] = val; break;
		case C71_STK5: R.STACK[5] = val; break;
		case C71_STK4: R.STACK[4] = val; break;
		case C71_STK3: R.STACK[3] = val; break;
		case C71_STK2: R.STACK[2] = val; break;
		case C71_STK1: R.STACK[1] = val; break;
		case C71_STK0: R.STACK[0] = val; break;
		case C71_PFC:  R.PFC = val; break;
		case C71_STR0: R.STR0 = val; break;
		case C71_STR1: R.STR1 = val; break;
		case C71_IFR:  R.IFR = val; break;
		case C71_RPTC: R.RPTC = val; break;
		case C71_ACC:  R.ACC = val; break;
		case C71_PREG: R.Preg = val; break;
		case C71_TREG: R.Treg = val; break;
		case C71_AR0: case C71_AR1: case C71_AR2: case C71_AR3:
		case C71_AR4: case C71_AR5: case C71_AR6: case C71_AR7:
			R.AR[regnum - C71_AR0] = val; break;
		case C71_DRR:  R.drr = val; break;
		case C71_DXR:  R.dxr = val; break;
		case C71_TIM:  R.tim = val; break;
		case C71_PRD:  R.prd = val; break;
		case C71_IMR:  R.imr = val; break;
		case C71_GREG: R.greg = val; break;
	}
}

void c71_set_irq_line(int irqline, int state)
{
	/* pending interrupts cannot be cleared */
	if (state != CLEAR_LINE && irqline >= 0 && irqline < 6)
		R.IFR |= (1 << irqline);
}

void c71_set_irq_callback(int (*callback)(int irqline))
{
	R.irq_callback = callback;
}

const char *c71_info(void *context, int regnum)
{
	static char buffer[16][47+1];
	static int which = 0;
	c71_regs *r = context;

	which = (which + 1) % 16;
	buffer[which][0] = '\0';
	if (!context)
		r = &R;

	switch (regnum)
	{
		case CPU_INFO_REG+C71_PC:   sprintf(buffer[which], "PC:%04X", r->PC); break;
		case CPU_INFO_REG+C71_STR0: sprintf(buffer[which], "STR0:%04X", r->STR0); break;
		case CPU_INFO_REG+C71_STR1: sprintf(buffer[which], "STR1:%04X", r->STR1); break;
		case CPU_INFO_REG+C71_IFR:  sprintf(buffer[which], "IFR:%04X", r->IFR); break;
		case CPU_INFO_REG+C71_RPTC: sprintf(buffer[which], "RPTC:%02X", r->RPTC); break;
		case CPU_INFO_REG+C71_ACC:  sprintf(buffer[which], "ACC:%08X", r->ACC); break;
		case CPU_INFO_REG+C71_PREG: sprintf(buffer[which], "P:%08X", r->Preg); break;
		case CPU_INFO_REG+C71_TREG: sprintf(buffer[which], "T:%04X", r->Treg); break;
		case CPU_INFO_REG+C71_AR0: case CPU_INFO_REG+C71_AR1: case CPU_INFO_REG+C71_AR2: case CPU_INFO_REG+C71_AR3:
		case CPU_INFO_REG+C71_AR4: case CPU_INFO_REG+C71_AR5: case CPU_INFO_REG+C71_AR6: case CPU_INFO_REG+C71_AR7:
			sprintf(buffer[which], "AR%d:%04X", regnum - CPU_INFO_REG - C71_AR0, r->AR[regnum - CPU_INFO_REG - C71_AR0]);
			break;
		case CPU_INFO_REG+C71_STK0: case CPU_INFO_REG+C71_STK1: case CPU_INFO_REG+C71_STK2: case CPU_INFO_REG+C71_STK3:
		case CPU_INFO_REG+C71_STK4: case CPU_INFO_REG+C71_STK5: case CPU_INFO_REG+C71_STK6: case CPU_INFO_REG+C71_STK7:
			sprintf(buffer[which], "STK%d:%04X", regnum - CPU_INFO_REG - C71_STK0, r->STACK[regnum - CPU_INFO_REG - C71_STK0]);
			break;
		case CPU_INFO_REG+C71_DRR:  sprintf(buffer[which], "DRR:%04X", r->drr); break;
		case CPU_INFO_REG+C71_DXR:  sprintf(buffer[which], "DXR:%04X", r->dxr); break;
		case CPU_INFO_REG+C71_TIM:  sprintf(buffer[which], "TIM:%04X", r->tim); break;
		case CPU_INFO_REG+C71_PRD:  sprintf(buffer[which], "PRD:%04X", r->prd); break;
		case CPU_INFO_REG+C71_IMR:  sprintf(buffer[which], "IMR:%04X", r->imr); break;
		case CPU_INFO_REG+C71_GREG: sprintf(buffer[which], "GREG:%04X", r->greg); break;
		case CPU_INFO_FLAGS:
			sprintf(buffer[which], "arp%d%c%c%cdp%03x arb%d%c%c%c%cpm%d",
				(r->STR0 & 0xe000) >> 13,
				r->STR0 & 0x1000 ? 'O':'.',
				r->STR0 & 0x0800 ? 'M':'.',
				r->STR0 & 0x0200 ? 'I':'.',
				(r->STR0 & 0x01ff),
				(r->STR1 & 0xe000) >> 13,
				r->STR1 & 0x1000 ? 'P':'D',
				r->STR1 & 0x0800 ? 'T':'.',
				r->STR1 & 0x0400 ? 'S':'.',
				r->STR1 & 0x0200 ? 'C':'.',
				(r->STR1 & 0x0003));
			break;
		case CPU_INFO_NAME: return "C71";
		case CPU_INFO_FAMILY: return "Namco C71 (TMS320C25)";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Based on the MAME TMS320C2x core (Tony La Porta, Quench and others)";
		case CPU_INFO_REG_LAYOUT: return (const char *)c71_reg_layout;
		case CPU_INFO_WIN_LAYOUT: return (const char *)c71_win_layout;
		default: return "";
	}
	return buffer[which];
}

unsigned c71_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%04X", PREAD(pc));
	return 1;
}
