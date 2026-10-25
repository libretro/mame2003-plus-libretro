/****************************************************************************

    NEC V25 (and V25+) emulator

    license:BSD-3-Clause
    copyright-holders:Bryan McPhail, Alex W. Jackson

    Port of MAME 0.264's src/devices/cpu/nec/v25.cpp and v25sfr.cpp to the
    MAME 0.78 CPU interface, in C. The instructions (v25ops.h) are the ones
    of MAME's necinstr.hxx and v25instr.hxx, converted by a script.

    A V20 core with:
    - the registers in 8 banks of the internal RAM (IDA, 256 bytes at IDB),
      with the special function registers after it (ports, timers,
      interrupt controller, clock);
    - the extra V25 instructions (register banks, BRKN/BRKS, FINT...);
    - the opcode decryption table of the "secure" V25 variants (Toaplan).

    As in MAME: the macro service, DMA, serial ports and the polling of
    the timers are not emulated, and the cycle counts are the V20 ones.

    The CPU clock of the machine driver is the input clock (the crystal):
    the CPU runs at clock / PCK, PCK being set by the PRC register (8 after
    reset), and the timers count input clocks.

****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "driver.h"
#include "state.h"
#include "mamedbg.h"
#include "v25.h"

typedef UINT8 BOOLEAN;
typedef UINT8 BYTE;
typedef UINT16 WORD;
typedef UINT32 DWORD;

/* Cpu types, steps of 8 to help the cycle count calculation */
#define V33_TYPE 0
#define V30_TYPE 8
#define V20_TYPE 16

/* interrupt vectors */
enum
{
	NEC_DIVIDE_VECTOR   = 0,
	NEC_TRAP_VECTOR     = 1,
	NEC_NMI_VECTOR      = 2,
	NEC_BRKV_VECTOR     = 4,
	NEC_CHKIND_VECTOR   = 5,
	NEC_IBRK_VECTOR     = 19,
	NEC_INTTU0_VECTOR   = 28,
	NEC_INTTU1_VECTOR   = 29,
	NEC_INTTU2_VECTOR   = 30,
	NEC_INTD0_VECTOR    = 20,
	NEC_INTD1_VECTOR    = 21,
	NEC_INTP0_VECTOR    = 24,
	NEC_INTP1_VECTOR    = 25,
	NEC_INTP2_VECTOR    = 26,
	NEC_INTSER0_VECTOR  = 12,
	NEC_INTSR0_VECTOR   = 13,
	NEC_INTST0_VECTOR   = 14,
	NEC_INTSER1_VECTOR  = 16,
	NEC_INTSR1_VECTOR   = 17,
	NEC_INTST1_VECTOR   = 18,
	NEC_INTTB_VECTOR    = 31
};

/* interrupt sources */
#define BRK     0
#define INT_IRQ 1
#define NMI_IRQ (1 << 1)
#define INTTU0  (1 << 2)
#define INTTU1  (1 << 3)
#define INTTU2  (1 << 4)
#define INTD0   (1 << 5)
#define INTD1   (1 << 6)
#define INTP0   (1 << 7)
#define INTP1   (1 << 8)
#define INTP2   (1 << 9)
#define INTSER0 (1 << 10)
#define INTSR0  (1 << 11)
#define INTST0  (1 << 12)
#define INTSER1 (1 << 13)
#define INTSR1  (1 << 14)
#define INTST1  (1 << 15)
#define INTTB   (1 << 16)
#define BRKN    (1 << 17)
#define BRKS    (1 << 18)

enum {
	VECTOR_PC = 0x02/2,
	PSW_SAVE  = 0x04/2,
	PC_SAVE   = 0x06/2
};

typedef enum {
	DS1 = 0x0E/2,
	PS  = 0x0C/2,
	SS  = 0x0A/2,
	DS0 = 0x08/2
} SREGS;

typedef enum {
	AW = 0x1E/2,
	CW = 0x1C/2,
	DW = 0x1A/2,
	BW = 0x18/2,
	SP = 0x16/2,
	BP = 0x14/2,
	IX = 0x12/2,
	IY = 0x10/2
} WREGS;

#ifdef MSB_FIRST
typedef enum {
	AL = 0x1F, AH = 0x1E, CL = 0x1D, CH = 0x1C,
	DL = 0x1B, DH = 0x1A, BL = 0x19, BH = 0x18
} BREGS;
#else
typedef enum {
	AL = 0x1E, AH = 0x1F, CL = 0x1C, CH = 0x1D,
	DL = 0x1A, DH = 0x1B, BL = 0x18, BH = 0x19
} BREGS;
#endif


/***************************************************************************
    CPU state
***************************************************************************/

typedef struct
{
	/* internal RAM and register banks */
	UINT16	internal_ram[0x80];

	UINT16	ip;
	UINT16	prev_ip;

	/* PSW flags */
	INT32	SignVal;
	UINT32	AuxVal, OverVal, ZeroVal, CarryVal, ParityVal;	/* 0 or non-0 valued flags */
	UINT8	IBRK, F0, F1, TF, IF, DF, MF;	/* 0 or 1 valued flags */
	UINT8	RBW, RBB;	/* current register bank base, preshifted for word and byte registers */

	/* interrupt related */
	UINT32	pending_irq;
	UINT32	unmasked_irq;
	UINT32	macro_service;
	UINT32	bankswitch_irq;
	UINT8	priority_inttu, priority_intd, priority_intp, priority_ints0, priority_ints1;
	UINT8	ems[3];
	UINT8	srms[2];
	UINT8	stms[2];
	UINT8	tmms[3];
	UINT8	IRQS, ISPR;
	UINT32	nmi_state;
	UINT32	irq_state;
	UINT32	poll_state;
	UINT32	mode_state;
	UINT32	intp_state[3];
	UINT8	intm;
	UINT8	no_interrupt;
	UINT8	halted;

	/* timer related */
	UINT16	TM0, MD0, TM1, MD1;
	UINT8	TMC0, TMC1;

	/* system control */
	UINT8	RAMEN, TB, PCK;	/* PRC register */
	UINT8	RFM;
	UINT16	WTC;
	UINT32	IDB;

	UINT8	prefetch_size;
	UINT8	prefetch_cycles;
	INT8	prefetch_count;
	UINT8	prefetch_reset;
	UINT32	chip_type;

	UINT32	prefix_base;	/* base address of the latest prefix segment */
	UINT8	seg_prefix;		/* prefix segment indicator */

	UINT32	EA;
	UINT16	EO;
	UINT16	E16;

	/* not saved: configuration */
	int		cpunum;
	const UINT8 *decryptiontable;
	int		(*irq_callback)(int irqline);
	mame_timer *timers[4];
} v25_state;

static v25_state V;
int v25_ICount;

/* per-CPU configuration, set by the driver before the CPU starts */
static const UINT8 *v25_config_table[MAX_CPU];
static read8_handler v25_config_in[MAX_CPU][4];		/* PT, P0, P1, P2 */
static write8_handler v25_config_out[MAX_CPU][3];	/* P0, P1, P2 */

static UINT8 parity_table[256];


/***************************************************************************
    macros (MAME's v25priv.ipp)
***************************************************************************/

#define SetRB(x)        do { V.RBW = (x) << 4; V.RBB = (x) << 5; } while (0)

#define Sreg(x)         V.internal_ram[V.RBW + (x)]
#define Wreg(x)         V.internal_ram[V.RBW + (x)]
#define Breg(x)         ((UINT8 *)V.internal_ram)[V.RBB + (x)]

#define PC()       ((Sreg(PS)<<4)+V.ip)

#define CF      (V.CarryVal!=0)
#define SF      (V.SignVal<0)
#define ZF      (V.ZeroVal==0)
#define PF      parity_table[(BYTE)V.ParityVal]
#define AF      (V.AuxVal!=0)
#define OF      (V.OverVal!=0)
#define RB      (V.RBW >> 4)

#define read_mem_byte(a)            v25_read_byte((a))
#define read_mem_word(a)            v25_read_word((a))
#define write_mem_byte(a,d)         v25_write_byte((a),(d))
#define write_mem_word(a,d)         v25_write_word((a),(d))

#define read_port_byte(a)       cpu_readport16((a) & 0xffff)
#define read_port_word(a)       (cpu_readport16((a) & 0xffff) | (cpu_readport16(((a) + 1) & 0xffff) << 8))
#define write_port_byte(a,d)    cpu_writeport16((a) & 0xffff, (d))
#define write_port_word(a,d)    do { cpu_writeport16((a) & 0xffff, (d) & 0xff); cpu_writeport16(((a) + 1) & 0xffff, ((d) >> 8) & 0xff); } while (0)

#define CHANGE_PC do { EMPTY_PREfetch(); } while (0)

#define SegBase(Seg) (Sreg(Seg) << 4)

#define DefaultBase(Seg) ((V.seg_prefix && (Seg==DS0 || Seg==SS)) ? V.prefix_base : Sreg(Seg) << 4)

#define GetMemB(Seg,Off) (read_mem_byte(DefaultBase(Seg) + (Off)))
#define GetMemW(Seg,Off) (read_mem_word(DefaultBase(Seg) + (Off)))

#define PutMemB(Seg,Off,x) { write_mem_byte(DefaultBase(Seg) + (Off), (x)); }
#define PutMemW(Seg,Off,x) { write_mem_word(DefaultBase(Seg) + (Off), (x)); }

/* prefetch timing */
#define EMPTY_PREfetch()    V.prefetch_reset = 1

#define PUSH(val) { Wreg(SP) -= 2; write_mem_word(((Sreg(SS)<<4)+Wreg(SP)), val); }
#define POP(var) { Wreg(SP) += 2; var = read_mem_word(((Sreg(SS)<<4) + ((Wreg(SP)-2) & 0xffff))); }

#define BRKXA(xa) { logerror("%06x: %sXA instruction is V33 exclusive\n", PC(), xa ? "BRK" : "RET"); }
#define BRKEM

#define GetModRM UINT32 ModRM=fetch()

#define CLK(all) v25_ICount-=all
#define CLKS(v20,v30,v33) { const UINT32 ccount=(v20<<16)|(v30<<8)|v33; v25_ICount-=(ccount>>V.chip_type)&0x7f; }
#define CLKW(v20o,v30o,v33o,v20e,v30e,v33e,addr) { const UINT32 ocount=(v20o<<16)|(v30o<<8)|v33o, ecount=(v20e<<16)|(v30e<<8)|v33e; v25_ICount-=(addr&1)?((ocount>>V.chip_type)&0x7f):((ecount>>V.chip_type)&0x7f); }
#define CLKM(v20,v30,v33,v20m,v30m,v33m) { const UINT32 ccount=(v20<<16)|(v30<<8)|v33, mcount=(v20m<<16)|(v30m<<8)|v33m; v25_ICount-=( ModRM >=0xc0 )?((ccount>>V.chip_type)&0x7f):((mcount>>V.chip_type)&0x7f); }
#define CLKR(v20o,v30o,v33o,v20e,v30e,v33e,vall,addr) { const UINT32 ocount=(v20o<<16)|(v30o<<8)|v33o, ecount=(v20e<<16)|(v30e<<8)|v33e; if (ModRM >=0xc0) v25_ICount-=vall; else v25_ICount-=(addr&1)?((ocount>>V.chip_type)&0x7f):((ecount>>V.chip_type)&0x7f); }

#define CompressFlags() (WORD)(CF | (V.IBRK << 1) | (PF << 2) | (V.F0 << 3) | (AF << 4) \
				| (V.F1 << 5) | (ZF << 6) | (SF << 7) | (V.TF << 8) | (V.IF << 9) \
				| (V.DF << 10) | (OF << 11) | (RB << 12) | (V.MF << 15))

#define ExpandFlags(f) \
{ \
	V.CarryVal = (f) & 0x0001; \
	V.IBRK = ((f) & 0x0002) == 0x0002; \
	V.ParityVal = !((f) & 0x0004); \
	V.F0 = ((f) & 0x0008) == 0x0008; \
	V.AuxVal = (f) & 0x0010; \
	V.F1 = ((f) & 0x0020) == 0x0020; \
	V.ZeroVal = !((f) & 0x0040); \
	V.SignVal = (f) & 0x0080 ? -1 : 0; \
	V.TF = ((f) & 0x0100) == 0x0100; \
	V.IF = ((f) & 0x0200) == 0x0200; \
	V.DF = ((f) & 0x0400) == 0x0400; \
	V.OverVal = (f) & 0x0800; \
	/* RB only changes on BRKCS/RETRBI/TSKSW, so skip it */ \
	V.MF = ((f) & 0x8000) == 0x8000; \
}


/***************************************************************************
    special function registers and internal data area (v25sfr.cpp)
***************************************************************************/

static void v25_timer_callback(int param)
{
	cpuintrf_push_context(param >> 24);
	V.pending_irq |= param & 0xffffff;
	cpuintrf_pop_context();
}

/* time of n input clocks */
static double v25_clocks_to_time(unsigned clocks)
{
	return (double)clocks * TIME_IN_HZ(Machine->drv->cpu[V.cpunum].cpu_clock);
}

/* the CPU runs at the input clock / PCK */
static void v25_update_clock(void)
{
	cpunum_set_clockscale(V.cpunum, 1.0 / V.PCK);
}

static UINT8 read_irqcontrol(UINT32 source, UINT8 priority)
{
	return  (((V.pending_irq & source)     ? 0x80 : 0x00)
			| ((V.unmasked_irq & source)   ? 0x00 : 0x40)
			| ((V.macro_service & source)  ? 0x20 : 0x00)
			| ((V.bankswitch_irq & source) ? 0x10 : 0x00)
			| priority);
}

static void write_irqcontrol(UINT32 source, UINT8 d)
{
	if (d & 0x80)
		V.pending_irq |= source;
	else
		V.pending_irq &= ~source;

	if (d & 0x40)
		V.unmasked_irq &= ~source;
	else
		V.unmasked_irq |= source;

	if (d & 0x20)
		V.macro_service |= source;
	else
		V.macro_service &= ~source;

	if (d & 0x10)
		V.bankswitch_irq |= source;
	else
		V.bankswitch_irq &= ~source;
}

static UINT8 port_in(int n)
{
	read8_handler h = v25_config_in[V.cpunum][n];
	return h ? h(0) : 0xff;
}

static void port_out(int n, UINT8 d)
{
	write8_handler h = v25_config_out[V.cpunum][n];
	if (h)
		h(0, d);
}

static void tmc0_w(UINT8 d)
{
	V.TMC0 = d;
	if (d & 0x01)	/* oneshot mode */
	{
		if (d & 0x80)
			timer_adjust(V.timers[0], v25_clocks_to_time(V.PCK * V.TM0 * ((d & 0x40) ? 128 : 12)), (V.cpunum << 24) | INTTU0, 0);
		else
			timer_adjust(V.timers[0], TIME_NEVER, 0, 0);

		if (d & 0x20)
			timer_adjust(V.timers[1], v25_clocks_to_time(V.PCK * V.MD0 * ((d & 0x10) ? 128 : 12)), (V.cpunum << 24) | INTTU1, 0);
		else
			timer_adjust(V.timers[1], TIME_NEVER, 0, 0);
	}
	else	/* interval mode */
	{
		if (d & 0x80)
		{
			double time = v25_clocks_to_time(V.PCK * V.MD0 * ((d & 0x40) ? 128 : 6));
			timer_adjust(V.timers[0], time, (V.cpunum << 24) | INTTU0, time);
			timer_adjust(V.timers[1], TIME_NEVER, 0, 0);
			V.TM0 = V.MD0;
		}
		else
		{
			timer_adjust(V.timers[0], TIME_NEVER, 0, 0);
			timer_adjust(V.timers[1], TIME_NEVER, 0, 0);
		}
	}
}

static void tmc1_w(UINT8 d)
{
	V.TMC1 = d & 0xC0;
	if (d & 0x80)
	{
		double time = v25_clocks_to_time(V.PCK * V.MD1 * ((d & 0x40) ? 128 : 6));
		timer_adjust(V.timers[2], time, (V.cpunum << 24) | INTTU2, time);
		V.TM1 = V.MD1;
	}
	else
		timer_adjust(V.timers[2], TIME_NEVER, 0, 0);
}

static void timebase_start(void)
{
	double time = v25_clocks_to_time(V.PCK << V.TB);
	timer_adjust(V.timers[3], time, (V.cpunum << 24) | INTTB, time);
}

static UINT8 prc_r(void)
{
	UINT8 ret = V.RAMEN ? 0x40 : 0;

	switch (V.TB)
	{
		case 10: break;
		case 13: ret |= 0x04; break;
		case 16: ret |= 0x08; break;
		case 20: ret |= 0x0C; break;
	}

	switch (V.PCK)
	{
		case 2: break;
		case 4: ret |= 0x01; break;
		case 8: ret |= 0x02; break;
	}

	return ret;
}

static void prc_w(UINT8 d)
{
	static const int timebases[4] = { 10, 13, 16, 20 };
	static const int clocks[4] = { 2, 4, 8, 0 };

	V.RAMEN = ((d & 0x40) == 0x40);
	V.TB = timebases[(d & 0x0C) >> 2];
	V.PCK = clocks[d & 0x03];
	if (V.PCK == 0)
		V.PCK = 8;

	timebase_start();
	v25_update_clock();
}

/* byte read of the special function registers (IDA 100-1ff) */
static UINT8 sfr_read(unsigned a)
{
	switch (a)
	{
		case 0x100: return port_in(1);	/* P0 */
		case 0x108:	/* P1 is combined with the interrupt lines */
			return ((port_in(2) & 0xf0)
					| (V.nmi_state     ? 0x00 : 0x01)
					| (V.intp_state[0] ? 0x00 : 0x02)
					| (V.intp_state[1] ? 0x00 : 0x04)
					| (V.intp_state[2] ? 0x00 : 0x08));
		case 0x110: return port_in(3);	/* P2 */
		case 0x138: return port_in(0);	/* PT */
		case 0x140: return V.intm;
		case 0x144: case 0x145: case 0x146: return V.ems[a - 0x144];
		case 0x14c: return read_irqcontrol(INTP0, V.priority_intp);
		case 0x14d: return read_irqcontrol(INTP1, 7);
		case 0x14e: return read_irqcontrol(INTP2, 7);
		case 0x165: return V.srms[0];
		case 0x166: return V.stms[0];
		case 0x16c: return read_irqcontrol(INTSER0, V.priority_ints0);
		case 0x16d: return read_irqcontrol(INTSR0, V.priority_ints0);
		case 0x16e: return read_irqcontrol(INTST0, V.priority_ints0);
		case 0x175: return V.srms[1];
		case 0x176: return V.stms[1];
		case 0x17c: return read_irqcontrol(INTSER1, V.priority_ints1);
		case 0x17d: return read_irqcontrol(INTSR1, V.priority_ints1);
		case 0x17e: return read_irqcontrol(INTST1, V.priority_ints1);
		case 0x180: return V.TM0 & 0xff;
		case 0x181: return V.TM0 >> 8;
		case 0x182: return V.MD0 & 0xff;
		case 0x183: return V.MD0 >> 8;
		case 0x188: return V.TM1 & 0xff;
		case 0x189: return V.TM1 >> 8;
		case 0x18a: return V.MD1 & 0xff;
		case 0x18b: return V.MD1 >> 8;
		case 0x194: case 0x195: case 0x196: return V.tmms[a - 0x194];
		case 0x19c: return read_irqcontrol(INTTU0, V.priority_inttu);
		case 0x19d: return read_irqcontrol(INTTU1, 7);
		case 0x19e: return read_irqcontrol(INTTU2, 7);
		case 0x1e1: return V.RFM;
		case 0x1e8: return V.WTC & 0xff;
		case 0x1e9: return V.WTC >> 8;
		case 0x1ea: return (V.F0 << 3) | (V.F1 << 5);
		case 0x1eb: return prc_r();
		case 0x1ec: return read_irqcontrol(INTTB, 7);
		case 0x1ef: return V.IRQS;
		case 0x1fc: return V.ISPR;
		case 0x1ff: return V.IDB >> 12;
	}
	return 0xff;
}

/* byte write of the special function registers */
static void sfr_write(unsigned a, UINT8 d)
{
	switch (a)
	{
		case 0x100: port_out(0, d); break;			/* P0 */
		case 0x108: port_out(1, d & 0xf0); break;	/* only the upper four bits of P1 are outputs */
		case 0x110: port_out(2, d); break;			/* P2 */
		case 0x140: V.intm = d & 0x55; break;
		case 0x144: case 0x145: case 0x146: V.ems[a - 0x144] = d & 0xf7; break;
		case 0x14c: write_irqcontrol(INTP0, d); V.priority_intp = d & 0x7; break;
		case 0x14d: write_irqcontrol(INTP1, d); break;
		case 0x14e: write_irqcontrol(INTP2, d); break;
		case 0x165: V.srms[0] = d & 0xf7; break;
		case 0x166: V.stms[0] = d & 0xf7; break;
		case 0x16c: write_irqcontrol(INTSER0, d & 0xd0); V.priority_ints0 = d & 0x7; break;	/* no macro service for error interrupt */
		case 0x16d: write_irqcontrol(INTSR0, d); V.priority_ints0 = d & 0x7; break;
		case 0x16e: write_irqcontrol(INTST0, d); V.priority_ints0 = d & 0x7; break;
		case 0x175: V.srms[1] = d & 0xf7; break;
		case 0x176: V.stms[1] = d & 0xf7; break;
		case 0x17c: write_irqcontrol(INTSER1, d & 0xd0); V.priority_ints1 = d & 0x7; break;
		case 0x17d: write_irqcontrol(INTSR1, d); V.priority_ints1 = d & 0x7; break;
		case 0x17e: write_irqcontrol(INTST1, d); V.priority_ints1 = d & 0x7; break;
		case 0x180: V.TM0 = (V.TM0 & 0xff00) | d; break;
		case 0x181: V.TM0 = (V.TM0 & 0x00ff) | (d << 8); break;
		case 0x182: V.MD0 = (V.MD0 & 0xff00) | d; break;
		case 0x183: V.MD0 = (V.MD0 & 0x00ff) | (d << 8); break;
		case 0x188: V.TM1 = (V.TM1 & 0xff00) | d; break;
		case 0x189: V.TM1 = (V.TM1 & 0x00ff) | (d << 8); break;
		case 0x18a: V.MD1 = (V.MD1 & 0xff00) | d; break;
		case 0x18b: V.MD1 = (V.MD1 & 0x00ff) | (d << 8); break;
		case 0x190: tmc0_w(d); break;
		case 0x191: tmc1_w(d); break;
		case 0x194: case 0x195: case 0x196: V.tmms[a - 0x194] = d & 0xf7; break;
		case 0x19c: write_irqcontrol(INTTU0, d); V.priority_inttu = d & 0x7; break;
		case 0x19d: write_irqcontrol(INTTU1, d); break;
		case 0x19e: write_irqcontrol(INTTU2, d); break;
		case 0x1e1: V.RFM = d; break;
		case 0x1e8: V.WTC = (V.WTC & 0xff00) | d; break;
		case 0x1e9: V.WTC = (V.WTC & 0x00ff) | (d << 8); break;
		case 0x1ea: V.F0 = (d >> 3) & 1; V.F1 = (d >> 5) & 1; break;
		case 0x1eb: prc_w(d); break;
		case 0x1ec: write_irqcontrol(INTTB, d & 0xC0); break;	/* no macro service, bank switching or priority */
		case 0x1ff: V.IDB = (d << 12) | 0xe00; break;
	}
}

/* the internal data area: RAM at 000-0ff, the registers at 100-1ff */
static UINT8 ida_read_byte(unsigned a)
{
	a &= 0x1ff;
	if (a < 0x100)
		return ((UINT8 *)V.internal_ram)[BYTE_XOR_LE(a)];
	return sfr_read(a);
}

static void ida_write_byte(unsigned a, UINT8 d)
{
	a &= 0x1ff;
	if (a < 0x100)
		((UINT8 *)V.internal_ram)[BYTE_XOR_LE(a)] = d;
	else
		sfr_write(a, d);
}

#define IN_IDA(a)	(((a) & 0xffe00) == V.IDB && (V.RAMEN || ((a) & 0x100)))

static UINT8 v25_read_byte(unsigned a)
{
	a &= 0xfffff;
	if (IN_IDA(a) || a == 0xfffff)
		return ida_read_byte(a);
	return cpu_readmem20(a);
}

static UINT16 v25_read_word(unsigned a)
{
	a &= 0xfffff;
	if (a & 1)
		return (v25_read_byte(a) | (v25_read_byte(a + 1) << 8));

	/* not sure about this - manual says FFFFC-FFFFE are "reserved" */
	if (a == 0xffffe)
		return (cpu_readmem20(a) | (ida_read_byte(0x1ff) << 8));
	else if (IN_IDA(a))
	{
		if ((a & 0x1ff) < 0x100)
			return V.internal_ram[(a & 0xff) >> 1];
		return ida_read_byte(a) | (ida_read_byte(a + 1) << 8);
	}
	return cpu_readmem20(a) | (cpu_readmem20(a + 1) << 8);
}

static void v25_write_byte(unsigned a, UINT8 d)
{
	a &= 0xfffff;
	if (IN_IDA(a) || a == 0xfffff)
		ida_write_byte(a, d);
	else
		cpu_writemem20(a, d);
}

static void v25_write_word(unsigned a, UINT16 d)
{
	a &= 0xfffff;
	if (a & 1)
	{
		v25_write_byte(a, d);
		v25_write_byte(a + 1, d >> 8);
		return;
	}

	/* not sure about this - manual says FFFFC-FFFFE are "reserved" */
	if (a == 0xffffe)
	{
		cpu_writemem20(a, d & 0xff);
		ida_write_byte(0x1ff, d >> 8);
	}
	else if (IN_IDA(a))
	{
		if ((a & 0x1ff) < 0x100)
			V.internal_ram[(a & 0xff) >> 1] = d;
		else
		{
			ida_write_byte(a, d & 0xff);
			ida_write_byte(a + 1, d >> 8);
		}
	}
	else
	{
		cpu_writemem20(a, d & 0xff);
		cpu_writemem20(a + 1, d >> 8);
	}
}


/***************************************************************************
    fetch and prefetch
***************************************************************************/

static void prefetch(void)
{
	V.prefetch_count--;
}

static void do_prefetch(int previous_ICount)
{
	int diff = previous_ICount - (int) v25_ICount;

	/* The implementation is not accurate, but comes close.
	 * It does not respect that the V30 will fetch two bytes
	 * at once directly, but instead uses only 2 cycles instead
	 * of 4. There are however only very few sources publicly
	 * available and they are vague.
	 */
	while (V.prefetch_count<0)
	{
		V.prefetch_count++;
		if (diff>V.prefetch_cycles)
			diff -= V.prefetch_cycles;
		else
			v25_ICount -= V.prefetch_cycles;
	}

	if (V.prefetch_reset)
	{
		V.prefetch_count = 0;
		V.prefetch_reset = 0;
		return;
	}

	while (diff>=V.prefetch_cycles && V.prefetch_count < V.prefetch_size)
	{
		diff -= V.prefetch_cycles;
		V.prefetch_count++;
	}
}

static UINT8 fetch(void)
{
	prefetch();
	return cpu_readmem20(((Sreg(PS)<<4)+V.ip++) & 0xfffff);
}

static UINT16 fetchword(void)
{
	UINT16 r = fetch();
	r |= (fetch()<<8);
	return r;
}

static UINT8 fetchop(void)
{
	UINT8 ret;

	prefetch();
	ret = cpu_readmem20(((Sreg(PS)<<4)+V.ip++) & 0xfffff);

	if (V.MF == 0)
		if (V.decryptiontable)
			ret = V.decryptiontable[ret];
	return ret;
}

static void nec_interrupt(unsigned int_num, int source);
static void nec_bankswitch(unsigned bank_num);
static void nec_trap(void);


/***************************************************************************
    instructions
***************************************************************************/

#include "v25ops.h"


/***************************************************************************
    interrupts
***************************************************************************/

static void nec_interrupt(unsigned int_num, int source)
{
	UINT32 dest_seg, dest_off;

	i_pushf();
	V.TF = V.IF = 0;
	V.MF = V.mode_state;

	switch(source)
	{
		case BRKN:	/* force native mode */
			V.MF = 1;
			break;
		case BRKS:	/* force secure mode */
			if (V.decryptiontable)
				V.MF = 0;
			break;
		case INT_IRQ:	/* get vector */
			int_num = V.irq_callback ? (*V.irq_callback)(0) : 0xff;
			break;
		default:
			break;
	}

	dest_off = read_mem_word(int_num*4);
	dest_seg = read_mem_word(int_num*4+2);

	PUSH(Sreg(PS));
	PUSH(V.ip);
	V.prev_ip = V.ip = (WORD)dest_off;
	Sreg(PS) = (WORD)dest_seg;
	CHANGE_PC;
}

static void nec_bankswitch(unsigned bank_num)
{
	int tmp = CompressFlags();

	V.TF = V.IF = 0;
	V.MF = V.mode_state;

	SetRB(bank_num);

	Wreg(PSW_SAVE) = tmp;
	Wreg(PC_SAVE) = V.ip;
	V.prev_ip = V.ip = Wreg(VECTOR_PC);
	CHANGE_PC;
}

static void nec_trap(void)
{
	v25_instruction[fetchop()]();
	nec_interrupt(NEC_TRAP_VECTOR, BRK);
}

static void external_int(void)
{
	/* interrupt sources subject to priority control */
	const UINT32 SOURCES = INTTU0 | INTTU1 | INTTU2 | INTD0 | INTD1 | INTP0 | INTP1 | INTP2
				| INTSER0 | INTSR0 | INTST0 | INTSER1 | INTSR1 | INTST1 | INTTB;

	int pending = V.pending_irq & V.unmasked_irq;

	if (pending & NMI_IRQ)
	{
		nec_interrupt(NEC_NMI_VECTOR, NMI_IRQ);
		V.pending_irq &= ~NMI_IRQ;
	}
	else if (pending & SOURCES)
	{
		int i = -1;
		UINT32 source = 0;
		UINT8 vector = 0;

		while (++i < 8)
		{
			if (V.ISPR & (1 << i)) break;

			if (V.priority_inttu == i)
			{
				if (pending & INTTU0) { source = INTTU0; vector = NEC_INTTU0_VECTOR; break; }
				if (pending & INTTU1) { source = INTTU1; vector = NEC_INTTU1_VECTOR; break; }
				if (pending & INTTU2) { source = INTTU2; vector = NEC_INTTU2_VECTOR; break; }
			}

			if (V.priority_intd == i)
			{
				if (pending & INTD0) { source = INTD0; vector = NEC_INTD0_VECTOR; break; }
				if (pending & INTD1) { source = INTD1; vector = NEC_INTD1_VECTOR; break; }
			}

			if (V.priority_intp == i)
			{
				if (pending & INTP0) { source = INTP0; vector = NEC_INTP0_VECTOR; break; }
				if (pending & INTP1) { source = INTP1; vector = NEC_INTP1_VECTOR; break; }
				if (pending & INTP2) { source = INTP2; vector = NEC_INTP2_VECTOR; break; }
			}

			if (V.priority_ints0 == i)
			{
				if (pending & INTSER0) { source = INTSER0; vector = NEC_INTSER0_VECTOR; break; }
				if (pending & INTSR0)  { source = INTSR0;  vector = NEC_INTSR0_VECTOR;  break; }
				if (pending & INTST0)  { source = INTST0;  vector = NEC_INTST0_VECTOR;  break; }
			}

			if (V.priority_ints1 == i)
			{
				if (pending & INTSER1) { source = INTSER1; vector = NEC_INTSER1_VECTOR; break; }
				if (pending & INTSR1)  { source = INTSR1;  vector = NEC_INTSR1_VECTOR;  break; }
				if (pending & INTST1)  { source = INTST1;  vector = NEC_INTST1_VECTOR;  break; }
			}

			if (i == 7 && (pending & INTTB))
			{
				source = INTTB;
				vector = NEC_INTTB_VECTOR;
				break;
			}
		}

		if (source != 0)
		{
			V.pending_irq &= ~source;
			if (V.macro_service & source)
			{
				/* macro service not emulated (as in MAME) */
			}
			else
			{
				V.IRQS = vector;
				V.ISPR |= (1 << i);
				if (V.bankswitch_irq & source)
					nec_bankswitch(i);
				else
					nec_interrupt(vector, source);
			}
		}
	}
	else if (pending & INT_IRQ)
	{
		/* the actual vector is retrieved after pushing flags */
		/* and clearing the IF */
		nec_interrupt((UINT32)-1, INT_IRQ);
		V.irq_state = CLEAR_LINE;
		V.pending_irq &= ~INT_IRQ;
	}
}


/***************************************************************************
    CPU interface
***************************************************************************/

void v25_set_decryption_table(int cpunum, const UINT8 *table)
{
	v25_config_table[cpunum] = table;
}

void v25_set_port_handlers(int cpunum,
	read8_handler pt_in, read8_handler p0_in, read8_handler p1_in, read8_handler p2_in,
	write8_handler p0_out, write8_handler p1_out, write8_handler p2_out)
{
	v25_config_in[cpunum][0] = pt_in;
	v25_config_in[cpunum][1] = p0_in;
	v25_config_in[cpunum][2] = p1_in;
	v25_config_in[cpunum][3] = p2_in;
	v25_config_out[cpunum][0] = p0_out;
	v25_config_out[cpunum][1] = p1_out;
	v25_config_out[cpunum][2] = p2_out;
}

static void v25_postload(void);

void v25_init(void)
{
	static const WREGS wreg_name[8]={ AW, CW, DW, BW, SP, BP, IX, IY };
	static const BREGS breg_name[8]={ AL, CL, DL, BL, AH, CH, DH, BH };
	int cpu = cpu_getactivecpu();
	unsigned int i, j, c;
	const char *type = "v25";

	for (i = 0; i < 256; i++)
	{
		for (j = i, c = 0; j > 0; j >>= 1)
			if (j & 1) c++;
		parity_table[i] = !(c & 1);
	}

	for (i = 0; i < 256; i++)
	{
		Mod_RM.reg.b[i] = breg_name[(i & 0x38) >> 3];
		Mod_RM.reg.w[i] = wreg_name[(i & 0x38) >> 3];
	}

	for (i = 0xc0; i < 0x100; i++)
	{
		Mod_RM.RM.w[i] = wreg_name[i & 7];
		Mod_RM.RM.b[i] = breg_name[i & 7];
	}

	memset(&V, 0, sizeof(V));
	V.cpunum = cpu;
	V.prefetch_size = 4;
	V.prefetch_cycles = 4;
	V.chip_type = V20_TYPE;
	V.PCK = 8;

	for (i = 0; i < 4; i++)
		V.timers[i] = timer_alloc(v25_timer_callback);

	state_save_register_UINT16(type, cpu, "IRAM",      V.internal_ram, 0x80);
	state_save_register_UINT16(type, cpu, "IP",        &V.ip, 1);
	state_save_register_UINT16(type, cpu, "PREV_IP",   &V.prev_ip, 1);
	state_save_register_INT32 (type, cpu, "SIGNVAL",   &V.SignVal, 1);
	state_save_register_UINT32(type, cpu, "AUXVAL",    &V.AuxVal, 1);
	state_save_register_UINT32(type, cpu, "OVERVAL",   &V.OverVal, 1);
	state_save_register_UINT32(type, cpu, "ZEROVAL",   &V.ZeroVal, 1);
	state_save_register_UINT32(type, cpu, "CARRYVAL",  &V.CarryVal, 1);
	state_save_register_UINT32(type, cpu, "PARITYVAL", &V.ParityVal, 1);
	state_save_register_UINT8 (type, cpu, "IBRK",      &V.IBRK, 1);
	state_save_register_UINT8 (type, cpu, "F0",        &V.F0, 1);
	state_save_register_UINT8 (type, cpu, "F1",        &V.F1, 1);
	state_save_register_UINT8 (type, cpu, "TF",        &V.TF, 1);
	state_save_register_UINT8 (type, cpu, "IF",        &V.IF, 1);
	state_save_register_UINT8 (type, cpu, "DF",        &V.DF, 1);
	state_save_register_UINT8 (type, cpu, "MF",        &V.MF, 1);
	state_save_register_UINT8 (type, cpu, "RBW",       &V.RBW, 1);
	state_save_register_UINT8 (type, cpu, "RBB",       &V.RBB, 1);
	state_save_register_UINT32(type, cpu, "PENDING",   &V.pending_irq, 1);
	state_save_register_UINT32(type, cpu, "UNMASKED",  &V.unmasked_irq, 1);
	state_save_register_UINT32(type, cpu, "MACRO",     &V.macro_service, 1);
	state_save_register_UINT32(type, cpu, "BANKSW",    &V.bankswitch_irq, 1);
	state_save_register_UINT8 (type, cpu, "PRI_TU",    &V.priority_inttu, 1);
	state_save_register_UINT8 (type, cpu, "PRI_D",     &V.priority_intd, 1);
	state_save_register_UINT8 (type, cpu, "PRI_P",     &V.priority_intp, 1);
	state_save_register_UINT8 (type, cpu, "PRI_S0",    &V.priority_ints0, 1);
	state_save_register_UINT8 (type, cpu, "PRI_S1",    &V.priority_ints1, 1);
	state_save_register_UINT8 (type, cpu, "EMS",       V.ems, 3);
	state_save_register_UINT8 (type, cpu, "SRMS",      V.srms, 2);
	state_save_register_UINT8 (type, cpu, "STMS",      V.stms, 2);
	state_save_register_UINT8 (type, cpu, "TMMS",      V.tmms, 3);
	state_save_register_UINT8 (type, cpu, "IRQS",      &V.IRQS, 1);
	state_save_register_UINT8 (type, cpu, "ISPR",      &V.ISPR, 1);
	state_save_register_UINT32(type, cpu, "NMI_STATE", &V.nmi_state, 1);
	state_save_register_UINT32(type, cpu, "IRQ_STATE", &V.irq_state, 1);
	state_save_register_UINT32(type, cpu, "POLL",      &V.poll_state, 1);
	state_save_register_UINT32(type, cpu, "MODE",      &V.mode_state, 1);
	state_save_register_UINT32(type, cpu, "INTP",      V.intp_state, 3);
	state_save_register_UINT8 (type, cpu, "INTM",      &V.intm, 1);
	state_save_register_UINT8 (type, cpu, "NO_INT",    &V.no_interrupt, 1);
	state_save_register_UINT8 (type, cpu, "HALTED",    &V.halted, 1);
	state_save_register_UINT16(type, cpu, "TM0",       &V.TM0, 1);
	state_save_register_UINT16(type, cpu, "MD0",       &V.MD0, 1);
	state_save_register_UINT16(type, cpu, "TM1",       &V.TM1, 1);
	state_save_register_UINT16(type, cpu, "MD1",       &V.MD1, 1);
	state_save_register_UINT8 (type, cpu, "TMC0",      &V.TMC0, 1);
	state_save_register_UINT8 (type, cpu, "TMC1",      &V.TMC1, 1);
	state_save_register_UINT8 (type, cpu, "RAMEN",     &V.RAMEN, 1);
	state_save_register_UINT8 (type, cpu, "TB",        &V.TB, 1);
	state_save_register_UINT8 (type, cpu, "PCK",       &V.PCK, 1);
	state_save_register_UINT8 (type, cpu, "RFM",       &V.RFM, 1);
	state_save_register_UINT16(type, cpu, "WTC",       &V.WTC, 1);
	state_save_register_UINT32(type, cpu, "IDB",       &V.IDB, 1);
	state_save_register_INT8  (type, cpu, "PF_COUNT",  &V.prefetch_count, 1);
	state_save_register_UINT8 (type, cpu, "PF_RESET",  &V.prefetch_reset, 1);
	state_save_register_UINT32(type, cpu, "PREFIX",    &V.prefix_base, 1);
	state_save_register_UINT8 (type, cpu, "SEGPREFIX", &V.seg_prefix, 1);
	state_save_register_UINT32(type, cpu, "EA",        &V.EA, 1);
	state_save_register_UINT16(type, cpu, "EO",        &V.EO, 1);
	state_save_register_UINT16(type, cpu, "E16",       &V.E16, 1);
	state_save_register_func_postload(v25_postload);
}

/* After a state load (with the context of this CPU active): the clock */
/* divider is set again from the loaded registers. The timers are in the */
/* state; with a state without them (another build), they are set again from */
/* the registers and restart from the beginning of their period. */
static void v25_postload(void)
{
	v25_update_clock();
	if (timer_state_loaded())
		return;
	timebase_start();
	tmc0_w(V.TMC0);
	if (V.TMC1 & 0x80)
		tmc1_w(V.TMC1);
	else
		timer_adjust(V.timers[2], TIME_NEVER, 0, 0);
}

void v25_reset(void *param)
{
	int (*save_irqcallback)(int) = V.irq_callback;

	(void)param;
	V.irq_callback = save_irqcallback;
	V.decryptiontable = v25_config_table[V.cpunum];

	V.ip = 0;
	V.prev_ip = 0;
	V.IBRK = 1;
	V.F0 = 0;
	V.F1 = 0;
	V.TF = 0;
	V.IF = 0;
	V.DF = 0;
	V.SignVal = 0;
	V.AuxVal = 0;
	V.OverVal = 0;
	V.ZeroVal = 1;
	V.CarryVal = 0;
	V.ParityVal = 1;
	V.pending_irq = 0;
	V.unmasked_irq = INT_IRQ | NMI_IRQ;
	V.macro_service = 0;
	V.bankswitch_irq = 0;
	V.priority_inttu = 7;
	V.priority_intd = 7;
	V.priority_intp = 7;
	V.priority_ints0 = 7;
	V.priority_ints1 = 7;
	V.IRQS = V.ISPR = 0;
	V.nmi_state = 0;
	V.irq_state = 0;
	V.poll_state = 1;
	V.mode_state = V.MF = (V.decryptiontable) ? 0 : 1;
	V.intm = 0;
	V.halted = 0;
	V.no_interrupt = 0;
	V.prefetch_count = 0;
	V.prefetch_reset = 0;
	V.prefix_base = 0;
	V.seg_prefix = 0;

	V.TM0 = V.MD0 = V.TM1 = V.MD1 = 0;
	V.TMC0 = V.TMC1 = 0;

	V.RAMEN = 1;
	V.TB = 20;
	V.PCK = 8;
	V.RFM = 0xfc;
	V.WTC = 0xffff;
	V.IDB = 0xffe00;

	timebase_start();
	timer_adjust(V.timers[0], TIME_NEVER, 0, 0);
	timer_adjust(V.timers[1], TIME_NEVER, 0, 0);
	timer_adjust(V.timers[2], TIME_NEVER, 0, 0);
	v25_update_clock();

	SetRB(7);
	Sreg(PS) = 0xffff;
	Sreg(SS) = 0;
	Sreg(DS0) = 0;
	Sreg(DS1) = 0;

	CHANGE_PC;
}

void v25_exit(void)
{
}

int v25_execute(int cycles)
{
	int prev_ICount;
	int pending = V.pending_irq & V.unmasked_irq;
	int i;

	v25_ICount = cycles;

	if (V.halted && pending)
	{
		for (i = 0; i < 8; i++)
		{
			if (V.ISPR & (1 << i)) break;

			if (V.priority_inttu == i && (pending & (INTTU0|INTTU1|INTTU2)))
				V.halted = 0;

			if (V.priority_intd == i && (pending & (INTD0|INTD1)))
				V.halted = 0;

			if (V.priority_intp == i && (pending & (INTP0|INTP1|INTP2)))
				V.halted = 0;

			if (V.priority_ints0 == i && (pending & (INTSER0|INTSR0|INTST0)))
				V.halted = 0;

			if (V.priority_ints1 == i && (pending & (INTSER1|INTSR1|INTST1)))
				V.halted = 0;

			if (i == 7 && (pending & INTTB))
				V.halted = 0;
		}
	}

	if (V.halted)
		return cycles;

	while (v25_ICount > 0)
	{
		/* Dispatch IRQ */
		V.prev_ip = V.ip;
		if (V.no_interrupt == 0 && (V.pending_irq & V.unmasked_irq))
		{
			if (V.pending_irq & NMI_IRQ)
				external_int();
			else if (V.IF)
				external_int();
		}

		/* No interrupt allowed between last instruction and this one */
		if (V.no_interrupt)
			V.no_interrupt--;

		CALL_MAME_DEBUG;
		prev_ICount = v25_ICount;
		v25_instruction[fetchop()]();
		do_prefetch(prev_ICount);
	}

	return cycles - v25_ICount;
}

unsigned v25_get_context(void *dst)
{
	if (dst)
		*(v25_state *)dst = V;
	return sizeof(v25_state);
}

void v25_set_context(void *src)
{
	if (src)
		V = *(v25_state *)src;
}

unsigned v25_get_reg(int regnum)
{
	switch (regnum)
	{
		case REG_PC:		return PC();
		case REG_PREVIOUSPC:	return (Sreg(PS)<<4) + V.prev_ip;
		case REG_SP:		return (Sreg(SS)<<4) + Wreg(SP);
		case V25_PC:		return PC();
		case V25_IP:		return V.ip;
		case V25_AW:		return Wreg(AW);
		case V25_CW:		return Wreg(CW);
		case V25_DW:		return Wreg(DW);
		case V25_BW:		return Wreg(BW);
		case V25_SP:		return Wreg(SP);
		case V25_BP:		return Wreg(BP);
		case V25_IX:		return Wreg(IX);
		case V25_IY:		return Wreg(IY);
		case V25_DS1:		return Sreg(DS1);
		case V25_PS:		return Sreg(PS);
		case V25_SS:		return Sreg(SS);
		case V25_DS0:		return Sreg(DS0);
		case V25_PSW:		return CompressFlags();
		case V25_IDB:		return V.IDB;
		case V25_PENDING:	return V.pending_irq;
	}
	return 0;
}

void v25_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
		case REG_PC:
		case V25_PC:
			if (val - (Sreg(PS)<<4) < 0x10000)
				V.ip = val - (Sreg(PS)<<4);
			else
			{
				Sreg(PS) = val >> 4;
				V.ip = val & 0x0000f;
			}
			break;
		case REG_SP:		Wreg(SP) = val - (Sreg(SS)<<4); break;
		case V25_IP:		V.ip = val; break;
		case V25_AW:		Wreg(AW) = val; break;
		case V25_CW:		Wreg(CW) = val; break;
		case V25_DW:		Wreg(DW) = val; break;
		case V25_BW:		Wreg(BW) = val; break;
		case V25_SP:		Wreg(SP) = val; break;
		case V25_BP:		Wreg(BP) = val; break;
		case V25_IX:		Wreg(IX) = val; break;
		case V25_IY:		Wreg(IY) = val; break;
		case V25_DS1:		Sreg(DS1) = val; break;
		case V25_PS:		Sreg(PS) = val; break;
		case V25_SS:		Sreg(SS) = val; break;
		case V25_DS0:		Sreg(DS0) = val; break;
		case V25_PSW:		ExpandFlags(val); break;
		case V25_IDB:		V.IDB = (val & 0xffe00) | 0xe00; break;
	}
}

void v25_set_irq_line(int irqline, int state)
{
	switch (irqline)
	{
		case 0:
			V.irq_state = state;
			if (state == CLEAR_LINE)
				V.pending_irq &= ~INT_IRQ;
			else
			{
				V.pending_irq |= INT_IRQ;
				V.halted = 0;
			}
			break;
		case IRQ_LINE_NMI:
			if (V.nmi_state == state) return;
			V.nmi_state = state;
			if (state != CLEAR_LINE)
			{
				V.pending_irq |= NMI_IRQ;
				V.halted = 0;
			}
			break;
		case V25_INPUT_LINE_INTP0:
		case V25_INPUT_LINE_INTP1:
		case V25_INPUT_LINE_INTP2:
			irqline -= V25_INPUT_LINE_INTP0;
			if (V.intp_state[irqline] == state) return;
			V.intp_state[irqline] = state;
			if (state != CLEAR_LINE)
				V.pending_irq |= (INTP0 << irqline);
			break;
		case V25_INPUT_LINE_POLL:
			V.poll_state = state;
			break;
	}
}

void v25_set_irq_callback(int (*callback)(int irqline))
{
	V.irq_callback = callback;
}

const char *v25_info(void *context, int regnum)
{
	static char buffer[16][47+1];
	static int which = 0;
	v25_state *r = context ? (v25_state *)context : &V;
	unsigned rbw = r->RBW;

	which = (which + 1) % 16;
	buffer[which][0] = '\0';

	switch (regnum)
	{
		case CPU_INFO_REG+V25_PC:	sprintf(buffer[which], "PC:%05X", ((r->internal_ram[rbw + PS] << 4) + r->ip) & 0xfffff); break;
		case CPU_INFO_REG+V25_IP:	sprintf(buffer[which], "IP:%04X", r->ip); break;
		case CPU_INFO_REG+V25_AW:	sprintf(buffer[which], "AW:%04X", r->internal_ram[rbw + AW]); break;
		case CPU_INFO_REG+V25_CW:	sprintf(buffer[which], "CW:%04X", r->internal_ram[rbw + CW]); break;
		case CPU_INFO_REG+V25_DW:	sprintf(buffer[which], "DW:%04X", r->internal_ram[rbw + DW]); break;
		case CPU_INFO_REG+V25_BW:	sprintf(buffer[which], "BW:%04X", r->internal_ram[rbw + BW]); break;
		case CPU_INFO_REG+V25_SP:	sprintf(buffer[which], "SP:%04X", r->internal_ram[rbw + SP]); break;
		case CPU_INFO_REG+V25_BP:	sprintf(buffer[which], "BP:%04X", r->internal_ram[rbw + BP]); break;
		case CPU_INFO_REG+V25_IX:	sprintf(buffer[which], "IX:%04X", r->internal_ram[rbw + IX]); break;
		case CPU_INFO_REG+V25_IY:	sprintf(buffer[which], "IY:%04X", r->internal_ram[rbw + IY]); break;
		case CPU_INFO_REG+V25_DS1:	sprintf(buffer[which], "DS1:%04X", r->internal_ram[rbw + DS1]); break;
		case CPU_INFO_REG+V25_PS:	sprintf(buffer[which], "PS:%04X", r->internal_ram[rbw + PS]); break;
		case CPU_INFO_REG+V25_SS:	sprintf(buffer[which], "SS:%04X", r->internal_ram[rbw + SS]); break;
		case CPU_INFO_REG+V25_DS0:	sprintf(buffer[which], "DS0:%04X", r->internal_ram[rbw + DS0]); break;
		case CPU_INFO_REG+V25_IDB:	sprintf(buffer[which], "IDB:%05X", r->IDB); break;
		case CPU_INFO_REG+V25_PENDING:	sprintf(buffer[which], "P:%05X", r->pending_irq); break;
		case CPU_INFO_FLAGS:		sprintf(buffer[which], "RB%d %c", rbw >> 4, r->MF ? 'N' : 'S'); break;
		case CPU_INFO_NAME:		return "V25";
		case CPU_INFO_FAMILY:	return "NEC V-Series";
		case CPU_INFO_VERSION:	return "MAME 0.264";
		case CPU_INFO_FILE:		return __FILE__;
		case CPU_INFO_CREDITS:	return "NEC V25/V35 emulator by Bryan McPhail, Alex W. Jackson";
	}
	return buffer[which];
}

unsigned v25_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%02X", cpu_readmem20(pc & 0xfffff));
	return 1;
}
