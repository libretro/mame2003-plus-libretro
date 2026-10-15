/***************************************************************************

 h83002.c: Hitachi H8/3002 microcontroller emulator

 Original by The_Author & DynaChicken for the ZiNc emulator.
 MAME changes by R. Belmont, Luca Elia, and Tomasz Slanina.

 TS 20060412 Added exts.l, sub.l, divxs.w (buggy), jsr @reg, rotxl.l reg, mov.l @(adr, reg), reg
 LE 20070903 Added divxu.b  shal.l  extu.w  dec.l #Imm,Rd  subx.b
 LE 20080202 Separated 3002/3044/3007, Added or.l  shal.l  rotl.l  not.l  neg.l  exts.w
             sub/or/xor.l #Imm:32,ERd  bset/bnot/bclr.b Rn,@ERd  bst/bist.b #Imm:3,@ERd  bnot.b #Imm:3,@ERd
 LE 20090128 Added mov.l ers,@aa:16;  bild #xx:3,rd;  eepmov.b;  bnot #xx:3,@aa:8

 mame2003-plus: backported from the legacy H8/3xx core of MAME 0.152
 (h8_16.c, h8ops.h, h8periph.c) to the MAME 0.78 CPU interface.
 Only the H8/3002 model is kept.  The on-chip RAM and peripheral
 registers are handled inside the core, the on-chip I/O ports are
 routed to the driver through h8_3002_set_io_handlers().

 This core keeps a single global context: get/set_context are no-ops,
 so only one H8/3002 can exist in a machine (System 12 uses one).

***************************************************************************/

#include <stdio.h>
#include <string.h>
#include "cpuintrf.h"
#include "memory.h"
#include "mamedbg.h"
#include "state.h"
#include "timer.h"
#include "mame.h"
#include "driver.h"
#include "h83002.h"

#define H8_LOG(...)	do { } while (0)

#define H8_SP	(7)

/* on-chip resources of the H8/3002 */
#define H8_IRAM_START	0xfffd10
#define H8_IRAM_END		0xffff0f
#define H8_REG_START	0xffff10

typedef struct
{
	/* main CPU stuff */
	UINT32 h8err;
	UINT32 regs[8];
	UINT32 pc, ppc;

	UINT32 irq_req[3];
	INT32 cyccnt;

	UINT8  ccr, exr;
	UINT8  h8nflag, h8vflag, h8cflag, h8zflag, h8iflag, h8hflag;
	UINT8  h8uflag, h8uiflag;
	UINT8  incheckirqs;

	UINT8  has_exr;
	int    mode_8bit;

	int (*irq_cb)(int irqline);

	/* onboard peripherals stuff */
	UINT8 per_regs[0x100];
	UINT8 iram[H8_IRAM_END - H8_IRAM_START + 1];

	UINT16 h8TCNT[5];
	UINT8 h8TSTR;

	mame_timer *timer[5];
	int clock;
} h83xx_state;

static h83xx_state h8state;
int h8_3002_ICount;

static h8_io_read_func  h8_io_read  = NULL;
static h8_io_write_func h8_io_write = NULL;

void h8_3002_set_io_handlers(h8_io_read_func readfn, h8_io_write_func writefn)
{
	h8_io_read = readfn;
	h8_io_write = writefn;
}

static data8_t h8_io_read_byte(int port)
{
	return h8_io_read ? h8_io_read(port) : 0;
}

static void h8_io_write_byte(int port, data8_t data)
{
	if (h8_io_write)
		h8_io_write(port, data);
}

static UINT8 h8_register_read8(h83xx_state *h8, UINT32 address);
static void h8_register_write8(h83xx_state *h8, UINT32 address, UINT8 val);
void h8_3002_InterruptRequest(h83xx_state *h8, UINT8 source, UINT8 state);

/***************************************************************************
    memory access (external bus via the 0.78 memory system, on-chip
    RAM/registers handled here)
***************************************************************************/

static UINT8 h8_read8(h83xx_state *h8, UINT32 address)
{
	address &= 0xffffff;
	if (address >= H8_IRAM_START)
	{
		if (address <= H8_IRAM_END)
			return h8->iram[address - H8_IRAM_START];
		return h8_register_read8(h8, address);
	}
	return cpu_readmem24bew(address);
}

static void h8_write8(h83xx_state *h8, UINT32 address, UINT8 data)
{
	address &= 0xffffff;
	if (address >= H8_IRAM_START)
	{
		if (address <= H8_IRAM_END)
			h8->iram[address - H8_IRAM_START] = data;
		else
			h8_register_write8(h8, address, data);
		return;
	}
	cpu_writemem24bew(address, data);
}

static UINT16 h8_read16(h83xx_state *h8, UINT32 address)
{
	address &= 0xfffffe;
	if (address >= H8_IRAM_START)
		return (h8_read8(h8, address) << 8) | h8_read8(h8, address + 1);
	return cpu_readmem24bew_word(address);
}

static void h8_write16(h83xx_state *h8, UINT32 address, UINT16 data)
{
	address &= 0xfffffe;
	if (address >= H8_IRAM_START)
	{
		h8_write8(h8, address, data >> 8);
		h8_write8(h8, address + 1, data & 0xff);
		return;
	}
	cpu_writemem24bew_word(address, data);
}

static UINT16 h8_readop16(h83xx_state *h8, UINT32 address)
{
	address &= 0xfffffe;
	if (address >= H8_IRAM_START)
		return h8_read16(h8, address);
	return cpu_readop16(address);
}

#define h8_mem_read8(x)          h8_read8(h8, x)
#define h8_mem_read16(z, x)      h8_read16(h8, x)
#define h8_mem_write8(x, y)      h8_write8(h8, x, y)
#define h8_mem_write16(z, x, y)  h8_write16(h8, x, y)

/* timing macros
   note: we assume a system 12 - type setup where external access is 3+1 states
   timing will be off somewhat for other configurations. */
#define H8_IFETCH_TIMING(x) h8->cyccnt -= (x) * 4;
#define H8_BRANCH_TIMING(x) h8->cyccnt -= (x) * 4;
#define H8_STACK_TIMING(x)  h8->cyccnt -= (x) * 4;
#define H8_BYTE_TIMING(x, adr)  if (address24 >= 0xffff10) h8->cyccnt -= (x) * 3; else h8->cyccnt -= (x) * 4;
#define H8_WORD_TIMING(x, adr)  if (address24 >= 0xffff10) h8->cyccnt -= (x) * 3; else h8->cyccnt -= (x) * 4;
#define H8_IOP_TIMING(x)    h8->cyccnt -= (x);

static UINT32 h8_mem_read32(h83xx_state *h8, UINT32 address)
{
	UINT32 result = h8_read16(h8, address) << 16;
	return result | h8_read16(h8, address + 2);
}

static void h8_mem_write32(h83xx_state *h8, UINT32 address, UINT32 data)
{
	h8_write16(h8, address, data >> 16);
	h8_write16(h8, address + 2, data);
}

static void h8_check_irqs(h83xx_state *h8);

/***************************************************************************
    implementation
***************************************************************************/

void h8_3002_InterruptRequest(h83xx_state *h8, UINT8 source, UINT8 state)
{
	int request = source / 32;
	int bit = source % 32;

	/* don't allow clear on external interrupts */
	if ((source <= 17) && (state == 0)) return;

	if (state)
		h8->irq_req[request] |= (1<<bit);
	else
		h8->irq_req[request] &= ~(1<<bit);
}

static UINT8 h8_get_ccr(h83xx_state *h8)
{
	h8->ccr = 0;
	if(h8->h8nflag)h8->ccr |= H8_NFLAG;
	if(h8->h8zflag)h8->ccr |= H8_ZFLAG;
	if(h8->h8vflag)h8->ccr |= H8_VFLAG;
	if(h8->h8cflag)h8->ccr |= H8_CFLAG;
	if(h8->h8uflag)h8->ccr |= H8_UFLAG;
	if(h8->h8hflag)h8->ccr |= H8_HFLAG;
	if(h8->h8uiflag)h8->ccr |= H8_UIFLAG;
	if(h8->h8iflag)h8->ccr |= H8_IFLAG;
	return h8->ccr;
}

static UINT8 h8_get_exr(h83xx_state *h8)
{
	return h8->exr;
}

static void h8_set_ccr(h83xx_state *h8, UINT8 data)
{
	h8->ccr = data;

	h8->h8nflag = (data & H8_NFLAG) ? 1 : 0;
	h8->h8zflag = (data & H8_ZFLAG) ? 1 : 0;
	h8->h8vflag = (data & H8_VFLAG) ? 1 : 0;
	h8->h8cflag = (data & H8_CFLAG) ? 1 : 0;
	h8->h8hflag = (data & H8_HFLAG) ? 1 : 0;
	h8->h8iflag = (data & H8_IFLAG) ? 1 : 0;
	h8->h8uflag = (data & H8_UFLAG) ? 1 : 0;
	h8->h8uiflag = (data & H8_UIFLAG) ? 1 : 0;

	if (!h8->incheckirqs) h8_check_irqs(h8);
}

static void h8_set_exr(h83xx_state *h8, UINT8 data)
{
	h8->exr = data;
}

static INT16 h8_getreg16(h83xx_state *h8, UINT8 reg)
{
	if(reg > 7)
		return h8->regs[reg-8]>>16;
	else
		return h8->regs[reg];
}

static void h8_setreg16(h83xx_state *h8, UINT8 reg, UINT16 data)
{
	if(reg > 7)
	{
		h8->regs[reg-8] &= 0xffff;
		h8->regs[reg-8] |= data<<16;
	}
	else
	{
		h8->regs[reg] &= 0xffff0000;
		h8->regs[reg] |= data;
	}
}

static UINT8 h8_getreg8(h83xx_state *h8, UINT8 reg)
{
	if(reg > 7)
		return h8->regs[reg-8];
	else
		return h8->regs[reg]>>8;
}

static void h8_setreg8(h83xx_state *h8, UINT8 reg, UINT8 data)
{
	if(reg > 7)
	{
		h8->regs[reg-8] &= 0xffffff00;
		h8->regs[reg-8] |= data;
	}
	else
	{
		h8->regs[reg] &= 0xffff00ff;
		h8->regs[reg] |= data<<8;
	}
}

static UINT32 h8_getreg32(h83xx_state *h8, UINT8 reg)
{
	return h8->regs[reg];
}

static void h8_setreg32(h83xx_state *h8, UINT8 reg, UINT32 data)
{
	h8->regs[reg] = data;
}

static void h8_GenException(h83xx_state *h8, UINT8 vectornr)
{
	/* push PC on stack (extended mode stack push!) */
	h8_setreg32(h8, H8_SP, h8_getreg32(h8, H8_SP)-4);
	h8_mem_write32(h8, h8_getreg32(h8, H8_SP), h8->pc | (h8_get_ccr(h8)<<24));

	/* generate address from vector */
	h8_set_ccr(h8, h8_get_ccr(h8) | 0x80);
	if (h8->h8uiflag == 0)
		h8_set_ccr(h8, h8_get_ccr(h8) | 0x40);
	h8->pc = h8_mem_read32(h8, vectornr * 4) & 0xffffff;

	/* I couldn't find timing info for exceptions, so this is a guess (based on JSR/BSR) */
	H8_IFETCH_TIMING(2);
	H8_STACK_TIMING(2);
}

static int h8_get_priority(h83xx_state *h8, UINT8 bit)
{
	int res = 0;
	switch(bit)
	{
	case 12: /* IRQ0 */
		if (h8->per_regs[0xF8]&0x80) res = 1;
		break;
	case 13: /* IRQ1 */
		if (h8->per_regs[0xF8]&0x40) res = 1;
		break;
	case 14: /* IRQ2 */
	case 15: /* IRQ3 */
		if (h8->per_regs[0xF8]&0x20) res = 1;
		break;
	case 16: /* IRQ4 */
	case 17: /* IRQ5 */
		if (h8->per_regs[0xF8]&0x10) res = 1;
		break;
	case 53: /* SCI0 Rx */
		if (!(h8->per_regs[0xB2]&0x40)) res = -2;
		else if (h8->per_regs[0xF9]&0x08) res = 1;
		break;
	case 54: /* SCI0 Tx Empty */
		if (!(h8->per_regs[0xB2]&0x80)) res = -2;
		else if (h8->per_regs[0xF9]&0x08) res = 1;
		break;
	case 55: /* SCI0 Tx End */
		if (!(h8->per_regs[0xB2]&0x04)) res = -2;
		else if (h8->per_regs[0xF9]&0x08) res = 1;
		break;
	case 57: /* SCI1 Rx */
		if (!(h8->per_regs[0xBA]&0x40)) res = -2;
		else if (h8->per_regs[0xF9]&0x04) res = 1;
		break;
	case 58: /* SCI1 Tx Empty */
		if (!(h8->per_regs[0xBA]&0x80)) res = -2;
		else if (h8->per_regs[0xF9]&0x04) res = 1;
		break;
	case 59: /* SCI1 Tx End */
		if (!(h8->per_regs[0xBA]&0x04)) res = -2;
		else if (h8->per_regs[0xF9]&0x04) res = 1;
		break;
	}
	return res;
}

static void h8_check_irqs(h83xx_state *h8)
{
	int lv = -1;

	h8->incheckirqs = 1;

	if (h8->has_exr)
	{
		lv = (h8->exr & 7);
	}
	else
	{
		if (h8->h8iflag == 0)
		{
			lv = 0;
		}
		else
		{
			if ((h8->per_regs[0xF2]&0x08)/*SYSCR*/ == 0)
			{
				if (h8->h8uiflag == 0)
					lv = 1;
			}
		}
	}

	/* any interrupts wanted and can accept ? */
	if(((h8->irq_req[0] != 0) || (h8->irq_req[1]!= 0) || (h8->irq_req[2] != 0)) && (lv >= 0))
	{
		UINT8 bit, source;
		/* which one ? */
		for(bit = 0, source = 0xff; source == 0xff && bit < 32; bit++)
		{
			if( h8->irq_req[0] & (1<<bit) )
			{
				if (h8_get_priority(h8, bit) >= lv)
				{
					h8->irq_req[0] &= ~(1<<bit);
					source = bit;
				}
			}
		}
		for(bit = 0; source == 0xff && bit < 32; bit++)
		{
			if( h8->irq_req[1] & (1<<bit) )
			{
				if (h8_get_priority(h8, bit + 32) >= lv)
				{
					h8->irq_req[1] &= ~(1<<bit);
					source = bit + 32;
				}
			}
		}
		for(bit = 0; source == 0xff && bit < 32; bit++)
		{
			if( h8->irq_req[2] & (1<<bit) )
			{
				if (h8_get_priority(h8, bit + 64) >= lv)
				{
					h8->irq_req[2] &= ~(1<<bit);
					source = bit + 64;
				}
			}
		}

		/* call the MAME callback if it's one of the 6 external IRQs */
		if (source >= 12 && source <= 17)
		{
			if (h8->irq_cb)
				(*h8->irq_cb)(source - 12 + H8_IRQ0);
		}

		if (source != 0xff)
			h8_GenException(h8, source);
	}

	h8->incheckirqs = 0;
}

#define U64(x) x##ULL
#define H8_ADDR_MASK 0xffffff
#include "h8ops.h"

/***************************************************************************
    on-chip peripherals (ITU timers, ports, serial, ADC)
***************************************************************************/

/* timer registers */
#define TSTR    (0x60)
#define TIER0   (0x66)
#define TIER1   (0x70)
#define TIER2   (0x7a)
#define TIER3   (0x84)
#define TIER4   (0x94)
#define TSR0    (0x67)
#define TSR1    (0x71)
#define TSR2    (0x7b)
#define TSR3    (0x85)
#define TSR4    (0x95)
#define TCR0    (0x64)
#define TCR1    (0x6e)
#define TCR2    (0x78)
#define TCR3    (0x82)
#define TCR4    (0x92)

static const UINT8 tsr[5] = { TSR0, TSR1, TSR2, TSR3, TSR4 };
static const UINT8 tier[5] = { TIER0, TIER1, TIER2, TIER3, TIER4 };
static const UINT8 tcr[5] = { TCR0, TCR1, TCR2, TCR3, TCR4 };
static const int tscales[4] = { 1, 2, 4, 8 };

static void h8itu_timer_expire(int which)
{
	h83xx_state *h8 = &h8state;

	timer_adjust(h8->timer[which], TIME_NEVER, 0, 0);
	h8->h8TCNT[which] = 0;
	h8->per_regs[tsr[which]] |= 4;
	/* interrupt on overflow ? */
	if(h8->per_regs[tier[which]] & 4)
	{
		h8_3002_InterruptRequest(h8, 26 + 4*which, 1);
	}
}

static void h8_itu_refresh_timer(h83xx_state *h8, int tnum)
{
	int ourTCR = h8->per_regs[tcr[tnum]];
	int ourTVAL = h8->h8TCNT[tnum];
	double period;

	period = TIME_IN_HZ(h8->clock) * tscales[ourTCR & 3] * (65536 - ourTVAL);

	if (ourTCR & 4)
		H8_LOG("H8/3002: Timer %d is using an external clock.  Unsupported!\n", tnum);

	timer_adjust(h8->timer[tnum], period, tnum, 0);
}

static void h8_itu_sync_timers(h83xx_state *h8, int tnum)
{
	int ourTCR = h8->per_regs[tcr[tnum]];
	double cycle_time, cur;

	/* get the time per unit */
	cycle_time = TIME_IN_HZ(h8->clock) * tscales[ourTCR & 3];
	cur = timer_timeelapsed(h8->timer[tnum]);

	h8->h8TCNT[tnum] = (UINT16)(cur / cycle_time);
}

static UINT8 h8_itu_read8(h83xx_state *h8, UINT8 reg)
{
	switch(reg)
	{
	case 0x60: return h8->h8TSTR;
	case 0x68: h8_itu_sync_timers(h8, 0); return h8->h8TCNT[0]>>8;
	case 0x69: h8_itu_sync_timers(h8, 0); return h8->h8TCNT[0]&0xff;
	case 0x72: h8_itu_sync_timers(h8, 1); return h8->h8TCNT[1]>>8;
	case 0x73: h8_itu_sync_timers(h8, 1); return h8->h8TCNT[1]&0xff;
	case 0x7c: h8_itu_sync_timers(h8, 2); return h8->h8TCNT[2]>>8;
	case 0x7d: h8_itu_sync_timers(h8, 2); return h8->h8TCNT[2]&0xff;
	case 0x86: h8_itu_sync_timers(h8, 3); return h8->h8TCNT[3]>>8;
	case 0x87: h8_itu_sync_timers(h8, 3); return h8->h8TCNT[3]&0xff;
	default:   return h8->per_regs[reg];
	}
}

static void h8_itu_write8(h83xx_state *h8, UINT8 reg, UINT8 val)
{
	int t;

	h8->per_regs[reg] = val;
	switch(reg)
	{
	case 0x60:
		for (t = 0; t < 5; t++)
			if ((val & (1 << t)) && !(h8->h8TSTR & (1 << t)))
				h8_itu_refresh_timer(h8, t);
		h8->h8TSTR = val;
		break;
	case 0x68: h8->h8TCNT[0] = (val<<8) | (h8->h8TCNT[0] & 0xff); if (h8->h8TSTR & 0x01) h8_itu_refresh_timer(h8, 0); break;
	case 0x69: h8->h8TCNT[0] = (val) | (h8->h8TCNT[0] & 0xff00);  if (h8->h8TSTR & 0x01) h8_itu_refresh_timer(h8, 0); break;
	case 0x72: h8->h8TCNT[1] = (val<<8) | (h8->h8TCNT[1] & 0xff); if (h8->h8TSTR & 0x02) h8_itu_refresh_timer(h8, 1); break;
	case 0x73: h8->h8TCNT[1] = (val) | (h8->h8TCNT[1] & 0xff00);  if (h8->h8TSTR & 0x02) h8_itu_refresh_timer(h8, 1); break;
	case 0x7c: h8->h8TCNT[2] = (val<<8) | (h8->h8TCNT[2] & 0xff); if (h8->h8TSTR & 0x04) h8_itu_refresh_timer(h8, 2); break;
	case 0x7d: h8->h8TCNT[2] = (val) | (h8->h8TCNT[2] & 0xff00);  if (h8->h8TSTR & 0x04) h8_itu_refresh_timer(h8, 2); break;
	case 0x86: h8->h8TCNT[3] = (val<<8) | (h8->h8TCNT[3] & 0xff); if (h8->h8TSTR & 0x08) h8_itu_refresh_timer(h8, 3); break;
	case 0x87: h8->h8TCNT[3] = (val) | (h8->h8TCNT[3] & 0xff00);  if (h8->h8TSTR & 0x08) h8_itu_refresh_timer(h8, 3); break;
	case 0x96: h8->h8TCNT[4] = (val<<8) | (h8->h8TCNT[4] & 0xff); if (h8->h8TSTR & 0x10) h8_itu_refresh_timer(h8, 4); break;
	case 0x97: h8->h8TCNT[4] = (val) | (h8->h8TCNT[4] & 0xff00);  if (h8->h8TSTR & 0x10) h8_itu_refresh_timer(h8, 4); break;
	default: break;
	}
}

static UINT8 h8_ISR_r(h83xx_state *h8)
{
	UINT8 res = 0;
	int i;
	for (i = 0; i < 6; i++)
		if (h8->irq_req[0] & (1 << (12+i))) res |= (1 << i);
	return res;
}

static void h8_ISR_w(h83xx_state *h8, UINT8 val)
{
	int i;
	for (i = 0; i < 6; i++)
		if ((~val) & (1 << i))  h8->irq_req[0] &= ~(1 << (12+i));
}

static UINT8 h8_register_read8(h83xx_state *h8, UINT32 address)
{
	UINT8 val;
	UINT8 reg = address & 0xff;

	if(reg >= 0x60 && reg <= 0x9f)
		return h8_itu_read8(h8, reg);

	switch(reg)
	{
	case 0xb4: /* serial port A status */
		val = h8->per_regs[reg];
		val |= 0xc4;        /* transmit finished, receive ready, no errors */
		break;
	case 0xb5: val = h8_io_read_byte(H8_SERIAL_0); break;	/* serial port A receive */
	case 0xbc: /* serial port B status */
		val = h8->per_regs[reg];
		val |= 0xc4;        /* transmit finished, receive ready, no errors */
		break;
	case 0xbd: val = h8_io_read_byte(H8_SERIAL_1); break;	/* serial port B receive */
	case 0xe0: val = h8_io_read_byte(H8_ADC_0_H); break;
	case 0xe1: val = h8_io_read_byte(H8_ADC_0_L); break;
	case 0xe2: val = h8_io_read_byte(H8_ADC_1_H); break;
	case 0xe3: val = h8_io_read_byte(H8_ADC_1_L); break;
	case 0xe4: val = h8_io_read_byte(H8_ADC_2_H); break;
	case 0xe5: val = h8_io_read_byte(H8_ADC_2_L); break;
	case 0xe6: val = h8_io_read_byte(H8_ADC_3_H); break;
	case 0xe7: val = h8_io_read_byte(H8_ADC_3_L); break;
	case 0xe8: val = 0x80; break;							/* adc status */
	case 0xc7: val = h8_io_read_byte(H8_PORT_4); break;	/* port 4 data */
	case 0xcb: val = h8_io_read_byte(H8_PORT_6); break;	/* port 6 data */
	case 0xce: val = h8_io_read_byte(H8_PORT_7); break;	/* port 7 data */
	case 0xcf: val = h8_io_read_byte(H8_PORT_8); break;	/* port 8 data */
	case 0xd2: val = h8_io_read_byte(H8_PORT_9); break;	/* port 9 data */
	case 0xd3: val = h8_io_read_byte(H8_PORT_A); break;	/* port a data */
	case 0xd6: val = h8_io_read_byte(H8_PORT_B); break;	/* port b data */
	case 0xf6: val = h8_ISR_r(h8); break;
	default:   val = h8->per_regs[reg]; break;
	}

	return val;
}

static void h8_register_write8(h83xx_state *h8, UINT32 address, UINT8 val)
{
	UINT8 reg = address & 0xff;

	if(reg >= 0x60 && reg <= 0x9f)
		h8_itu_write8(h8, reg, val);

	switch (reg)
	{
	case 0xb3:  /* serial 0 send */
		h8_io_write_byte(H8_SERIAL_0, val);
		h8_3002_InterruptRequest(h8, 54, 1);
		h8_3002_InterruptRequest(h8, 55, 1);
		break;
	case 0xbb:  /* serial 1 send */
		h8_io_write_byte(H8_SERIAL_1, val);
		h8_3002_InterruptRequest(h8, 58, 1);
		h8_3002_InterruptRequest(h8, 59, 1);
		break;
	case 0xc7: h8_io_write_byte(H8_PORT_4, val); break;
	case 0xcb: h8_io_write_byte(H8_PORT_6, val); break;
	case 0xce: h8_io_write_byte(H8_PORT_7, val); break;
	case 0xcf: h8_io_write_byte(H8_PORT_8, val); break;
	case 0xd2: h8_io_write_byte(H8_PORT_9, val); break;
	case 0xd3: h8_io_write_byte(H8_PORT_A, val); break;
	case 0xd6: h8_io_write_byte(H8_PORT_B, val); break;
	case 0xf6: h8_ISR_w(h8, val); break;
	}

	h8->per_regs[reg] = val;
}

/***************************************************************************
    MAME 0.78 interface
***************************************************************************/

static void h8_onstateload(void)
{
	h8_set_ccr(&h8state, h8state.ccr);
}

void h8_3002_init(void)
{
	h83xx_state *h8 = &h8state;
	int cpu = cpu_getactivecpu();
	int i;

	memset(h8, 0, sizeof(*h8));
	h8->h8iflag = 1;
	h8->mode_8bit = 0;

	for (i = 0; i < 5; i++)
		h8->timer[i] = timer_alloc(h8itu_timer_expire);

	state_save_register_UINT32("h8_3002", cpu, "h8err",    &h8->h8err, 1);
	state_save_register_UINT32("h8_3002", cpu, "regs",     h8->regs, 8);
	state_save_register_UINT32("h8_3002", cpu, "pc",       &h8->pc, 1);
	state_save_register_UINT32("h8_3002", cpu, "ppc",      &h8->ppc, 1);
	state_save_register_UINT32("h8_3002", cpu, "irq_req",  h8->irq_req, 3);
	state_save_register_UINT8 ("h8_3002", cpu, "ccr",      &h8->ccr, 1);
	state_save_register_UINT8 ("h8_3002", cpu, "per_regs", h8->per_regs, 0x100);
	state_save_register_UINT8 ("h8_3002", cpu, "iram",     h8->iram, sizeof(h8->iram));
	state_save_register_UINT8 ("h8_3002", cpu, "h8TSTR",   &h8->h8TSTR, 1);
	state_save_register_UINT16("h8_3002", cpu, "h8TCNT",   h8->h8TCNT, 5);
	state_save_register_func_postload(h8_onstateload);
}

void h8_3002_reset(void *param)
{
	h83xx_state *h8 = &h8state;
	int i;

	h8->clock = Machine->drv->cpu[cpu_getactivecpu()].cpu_clock;

	h8->h8err = 0;
	memset(h8->regs, 0, sizeof(h8->regs));
	memset(h8->irq_req, 0, sizeof(h8->irq_req));
	memset(h8->per_regs, 0, sizeof(h8->per_regs));
	h8->incheckirqs = 0;
	h8->exr = 0;
	h8->has_exr = 0;
	h8_set_ccr(h8, H8_IFLAG);

	h8->pc = h8_mem_read32(h8, 0) & 0xffffff;
	h8->ppc = h8->pc;

	/* disable timers */
	h8->h8TSTR = 0;
	for (i = 0; i < 5; i++)
	{
		h8->h8TCNT[i] = 0;
		timer_adjust(h8->timer[i], TIME_NEVER, 0, 0);
	}
}

void h8_3002_exit(void)
{
}

/* single instance: the state lives in h8state, contexts are not swapped */
unsigned h8_3002_get_context(void *dst)
{
	return 1;
}

void h8_3002_set_context(void *src)
{
}

unsigned h8_3002_get_reg(int regnum)
{
	h83xx_state *h8 = &h8state;

	switch (regnum)
	{
		case REG_PC:
		case H8_PC:   return h8->pc;
		case REG_PREVIOUSPC: return h8->ppc;
		case REG_SP:  return h8->regs[7];
		case H8_CCR:  return h8_get_ccr(h8);
		case H8_E0: case H8_E1: case H8_E2: case H8_E3:
		case H8_E4: case H8_E5: case H8_E6: case H8_E7:
			return h8->regs[regnum - H8_E0];
	}
	return 0;
}

void h8_3002_set_reg(int regnum, unsigned val)
{
	h83xx_state *h8 = &h8state;

	switch (regnum)
	{
		case REG_PC:
		case H8_PC:   h8->pc = val & 0xffffff; break;
		case REG_SP:  h8->regs[7] = val; break;
		case H8_CCR:  h8_set_ccr(h8, val); break;
		case H8_E0: case H8_E1: case H8_E2: case H8_E3:
		case H8_E4: case H8_E5: case H8_E6: case H8_E7:
			h8->regs[regnum - H8_E0] = val; break;
	}
}

void h8_3002_set_irq_line(int irqline, int state)
{
	h83xx_state *h8 = &h8state;
	int asserted = (state != CLEAR_LINE && state != INTERNAL_CLEAR_LINE);

	switch (irqline)
	{
		case H8_IRQ0: case H8_IRQ1: case H8_IRQ2:
		case H8_IRQ3: case H8_IRQ4: case H8_IRQ5:
			h8_3002_InterruptRequest(h8, 12 + irqline - H8_IRQ0, asserted);
			break;
		case H8_SCI_0_RX: h8_3002_InterruptRequest(h8, 53, asserted); break;
		case H8_SCI_1_RX: h8_3002_InterruptRequest(h8, 57, asserted); break;
	}
}

void h8_3002_set_irq_callback(int (*callback)(int irqline))
{
	h8state.irq_cb = callback;
}

const char *h8_3002_info(void *context, int regnum)
{
	static char buffer[16][47+1];
	static int which = 0;
	h83xx_state *h8 = &h8state;

	which = (which+1) % 16;
	buffer[which][0] = '\0';

	switch (regnum)
	{
		case CPU_INFO_REG+H8_PC:  sprintf(buffer[which], "PC  :%06X", h8->pc); break;
		case CPU_INFO_REG+H8_CCR: sprintf(buffer[which], "CCR :%02X", h8_get_ccr(h8)); break;
		case CPU_INFO_REG+H8_E0:  sprintf(buffer[which], "ER0 :%08X", h8->regs[0]); break;
		case CPU_INFO_REG+H8_E1:  sprintf(buffer[which], "ER1 :%08X", h8->regs[1]); break;
		case CPU_INFO_REG+H8_E2:  sprintf(buffer[which], "ER2 :%08X", h8->regs[2]); break;
		case CPU_INFO_REG+H8_E3:  sprintf(buffer[which], "ER3 :%08X", h8->regs[3]); break;
		case CPU_INFO_REG+H8_E4:  sprintf(buffer[which], "ER4 :%08X", h8->regs[4]); break;
		case CPU_INFO_REG+H8_E5:  sprintf(buffer[which], "ER5 :%08X", h8->regs[5]); break;
		case CPU_INFO_REG+H8_E6:  sprintf(buffer[which], "ER6 :%08X", h8->regs[6]); break;
		case CPU_INFO_REG+H8_E7:  sprintf(buffer[which], " SP :%08X", h8->regs[7]); break;
		case CPU_INFO_FLAGS:
			sprintf(buffer[which], "%c%c%c%c%c%c%c%c",
				h8->h8iflag ? 'I':'i', h8->h8uiflag ? 'U':'u',
				h8->h8hflag ? 'H':'h', h8->h8uflag ? 'U':'u',
				h8->h8nflag ? 'N':'n', h8->h8zflag ? 'Z':'z',
				h8->h8vflag ? 'V':'v', h8->h8cflag ? 'C':'c');
			break;
		case CPU_INFO_NAME: return "H8/3002";
		case CPU_INFO_FAMILY: return "Hitachi H8/300";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "The_Author & DynaChicken (ZiNc), R. Belmont, Luca Elia, Tomasz Slanina";
	}
	return buffer[which];
}

unsigned h8_3002_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%04X", h8_read16(&h8state, pc));
	return 2;
}
