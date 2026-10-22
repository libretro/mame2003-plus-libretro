/***************************************************************************

 mn10200.c : Panasonic MN10200 (MN1020012A) emulator

 Backported to the MAME 0.78 CPU interface (mame2003-plus) from current
 MAME src/devices/cpu/mn10200/mn10200.cpp, written by Olivier Galibert,
 peripherals and improvements by R. Belmont and hap. The opcode and
 peripheral code is MAME's, only translated to C.

 0.78 timers are not saved in save states: the 8-bit timers are kept as
 "armed, time left" and set again after a load.

****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "driver.h"
#include "state.h"
#include "mamedbg.h"
#include "mn10200.h"

#define MN10200_NUM_PRESCALERS		2
#define MN10200_NUM_TIMERS_8BIT		10
#define MN10200_NUM_IRQ_GROUPS		31

enum
{
	FLAG_ZF  = 0x0001, /* zero flag */
	FLAG_NF  = 0x0002, /* negative flag */
	FLAG_CF  = 0x0004, /* carry flag */
	FLAG_VF  = 0x0008, /* overflow flag */
	FLAG_ZX  = 0x0010, /* extended zero flag */
	FLAG_NX  = 0x0020, /* extended negative flag */
	FLAG_CX  = 0x0040, /* extended carry flag */
	FLAG_VX  = 0x0080, /* extended overflow flag */
	FLAG_IM0 = 0x0100, /* interrupt mask */
	FLAG_IM1 = 0x0200,
	FLAG_IM2 = 0x0400,
	FLAG_IE  = 0x0800, /* interrupt enable */
	FLAG_S0  = 0x1000, /* software bit */
	FLAG_S1  = 0x2000,
	FLAG_D14 = 0x4000,
	FLAG_D15 = 0x8000
};

typedef struct
{
	UINT8 mode;
	UINT8 base;
	UINT8 cur;
} mn10200_counter;

typedef struct
{
	/* the UINT32s are really UINT24 */
	UINT32 pc;
	UINT32 d[4];
	UINT32 a[4];
	UINT16 psw;
	UINT16 mdr;

	/* interrupts */
	UINT8 icrl[MN10200_NUM_IRQ_GROUPS];
	UINT8 icrh[MN10200_NUM_IRQ_GROUPS];
	UINT8 nmicr;
	UINT8 iagr;
	UINT8 extmdl;
	UINT8 extmdh;
	int possible_irq;

	/* timers */
	double sysclock_period;
	mn10200_counter simple_timer[MN10200_NUM_TIMERS_8BIT];
	mn10200_counter prescaler[MN10200_NUM_PRESCALERS];

	/* dma */
	struct
	{
		UINT32 adr;
		UINT32 count;
		UINT16 iadr;
		UINT8 ctrll;
		UINT8 ctrlh;
		UINT8 irq;
	} dma[8];

	/* serial */
	struct
	{
		UINT8 ctrll;
		UINT8 ctrlh;
		UINT8 buf;
		UINT8 recv;
	} serial[2];

	/* ports */
	UINT8 pplul;
	UINT8 ppluh;
	UINT8 p3md;
	UINT8 p4;
	struct
	{
		UINT8 out;
		UINT8 dir;
	} port[4];
} mn10200_state;

static mn10200_state mn;
int mn10200_ICount;

static void *timer_timers[MN10200_NUM_TIMERS_8BIT];
static UINT8 timer_on[MN10200_NUM_TIMERS_8BIT];		/* armed (for save states) */
static double timer_left[MN10200_NUM_TIMERS_8BIT];

/*
	A timer whose underflows nobody sees (its irq disabled with the request
	already up, no timer cascaded on it) is left asleep instead of firing:
	used as a baud clock it would underflow 240000 times a second, and every
	timer end cuts the timeslices of all the CPUs. It keeps its phase, and
	wakes up as soon as one of those conditions changes.
*/
static UINT8 timer_asleep[MN10200_NUM_TIMERS_8BIT];
static double timer_t0[MN10200_NUM_TIMERS_8BIT];		/* time of the last underflow */
static double timer_period[MN10200_NUM_TIMERS_8BIT];

static mn10200_port_read_func port_read_fn;
static mn10200_port_write_func port_write_fn;

void mn10200_set_port_handlers(mn10200_port_read_func readfn, mn10200_port_write_func writefn)
{
	port_read_fn = readfn;
	port_write_fn = writefn;
}

static UINT8 port_r(int port)
{
	return port_read_fn ? port_read_fn( port ) : 0xff;
}

static void port_w(int port, UINT8 data)
{
	if( port_write_fn )
		port_write_fn( port, data );
}

static void io_control_w(offs_t offset, UINT8 data);
static UINT8 io_control_r(offs_t offset);

/* memory: the on-chip peripherals at 0xfc00-0xffff, the rest on the bus */
#define IS_IO(a)	( ( (a) & 0xfffc00 ) == 0x00fc00 )

static INLINE UINT8 read_mem8(UINT32 address)
{
	address &= 0xffffff;
	if( IS_IO( address ) )
		return io_control_r( address - 0xfc00 );
	return cpu_readmem24lew( address );
}

static INLINE UINT16 read_mem16(UINT32 address)
{
	address &= 0xfffffe;
	if( IS_IO( address ) )
		return io_control_r( address - 0xfc00 ) | ( io_control_r( address - 0xfc00 + 1 ) << 8 );
	return cpu_readmem24lew_word( address );
}

static INLINE UINT32 read_mem24(UINT32 address)
{
	address &= 0xfffffe;
	return read_mem16( address ) | ( read_mem8( address + 2 ) << 16 );
}

static INLINE void write_mem8(UINT32 address, UINT8 data)
{
	address &= 0xffffff;
	if( IS_IO( address ) )
		io_control_w( address - 0xfc00, data );
	else
		cpu_writemem24lew( address, data );
}

static INLINE void write_mem16(UINT32 address, UINT16 data)
{
	address &= 0xfffffe;
	if( IS_IO( address ) )
	{
		io_control_w( address - 0xfc00, data & 0xff );
		io_control_w( address - 0xfc00 + 1, data >> 8 );
	}
	else
		cpu_writemem24lew_word( address, data );
}

static INLINE void write_mem24(UINT32 address, UINT32 data)
{
	address &= 0xfffffe;
	write_mem16( address, data );
	write_mem8( address + 2, data >> 16 );
}

/* instruction arguments: byte reads, any alignment */
static INLINE UINT8 read_arg8(UINT32 address)
{
	return read_mem8( address );
}

static INLINE UINT16 read_arg16(UINT32 address)
{
	return read_mem8( address ) | ( read_mem8( address + 1 ) << 8 );
}

static INLINE UINT32 read_arg24(UINT32 address)
{
	return read_mem8( address ) | ( read_mem8( address + 1 ) << 8 ) | ( read_mem8( address + 2 ) << 16 );
}

static INLINE void change_pc(UINT32 pc)
{
	mn.pc = pc & 0xffffff;
}

static void refresh_timer(int tmr);

/*************************************************************************/
/* from MAME mn10200.cpp */

static void take_irq(int level, int group)
{
	mn10200_ICount -= 7;

	write_mem24(mn.a[3] - 4, mn.pc);
	write_mem16(mn.a[3] - 6, mn.psw);
	mn.a[3] -= 6;
	change_pc(0x80008);
	mn.psw = (mn.psw & 0xf0ff) | (level << 8);
	mn.iagr = group;
}

static void check_irq(void)
{
	int level, group, i;

	if (!mn.nmicr && !(mn.psw & FLAG_IE))
		return;

	level = mn.psw >> 8 & 7;
	group = 0;

	/* find highest valid level */
	for (i = 1; i < 11; i++)
	{
		if (mn.icrl[i] >> 4 & mn.icrh[i] & 0xf && (mn.icrh[i] >> 4 & 7) < level)
		{
			level = mn.icrh[i] >> 4 & 7;
			group = i;
		}
	}

	/* take interrupt */
	if (mn.nmicr && group)
		take_irq(level, 0);
	else if (mn.nmicr)
		take_irq(0, 0);
	else if (group)
		take_irq(level, group);

	return;
}

static void check_ext_irq(void)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		/* active irq at low or high? (not edge triggered) */
		if ((mn.p4 >> i & 1) == (mn.extmdl >> (i * 2) & 3))
			mn.icrl[8] |= (1 << (4 + i));
	}

	mn.possible_irq = 1;
}

void mn10200_set_irq_line(int irqnum, int state)
{
	/* take an external IRQ */
	int pin = state ? 0 : 1;
	int old, active = 0;

	if ((UINT32)irqnum >= MN10200_MAX_EXT_IRQ)
		return;
	old = mn.p4 >> irqnum & 1;

	switch (mn.extmdl >> (irqnum * 2) & 3)
	{
		/* 'L' level */
		case 0:
			active = (pin == 0);
			break;

		/* 'H' level */
		case 1:
			active = (pin == 1);
			break;

		/* falling edge */
		case 2:
			active = (pin == 0 && old == 1);
			break;

		/* rising edge */
		case 3:
			active = (pin == 1 && old == 0);
			break;
	}

	mn.p4 &= ~(1 << irqnum);
	mn.p4 |= pin << irqnum;

	if (active)
	{
		mn.icrl[8] |= (1 << (4 + irqnum));
		mn.possible_irq = 1;
	}
}

static int timer_tick_simple(int tmr)
{
	int next = tmr + 1;

	/* is it a cascaded timer, and enabled? */
	if (next < MN10200_NUM_TIMERS_8BIT && mn.simple_timer[next].mode & 0x83 && (mn.simple_timer[next].mode & 0x83) == 0x81)
	{
		/* did it underflow? */
		if (--mn.simple_timer[next].cur == 0xff)
		{
			/* cascaded underflow? */
			if (timer_tick_simple(next) != 2)
			{
				mn.simple_timer[next].cur = mn.simple_timer[next].base;
				return 1;
			}
		}

		return 2;
	}
	else
	{
		/* trigger irq */
		mn.icrl[1 + (tmr >> 2)] |= (1 << (4 + (tmr & 3)));
		mn.possible_irq = 1;

		return 0;
	}
}

static void refresh_timer(int tmr)
{
	/* 0: external pin */
	/* 1: cascaded (handled elsewhere) */
	/* 2: prescaler 0 */
	/* 3: prescaler 1 */
	int p = mn.simple_timer[tmr].mode & 1;

	/* enabled, and source is prescaler? */
	timer_asleep[tmr] = 0;
	if ((mn.simple_timer[tmr].mode & 0x82) == 0x82 && mn.prescaler[p].mode & 0x80)
	{
		double period = mn.sysclock_period * (mn.prescaler[p].base + 1) * (mn.simple_timer[tmr].cur + 1);
		timer_adjust(timer_timers[tmr], period, tmr, 0);
		timer_on[tmr] = 1;
	}
	else
	{
		timer_adjust(timer_timers[tmr], TIME_NEVER, tmr, 0);
		timer_on[tmr] = 0;
	}
}

static void refresh_all_timers(void)
{
	int tmr;
	for (tmr = 0; tmr < MN10200_NUM_TIMERS_8BIT; tmr++)
		refresh_timer(tmr);
}

/* nothing would see its next underflows */
static int timer_unseen(int tmr)
{
	int g = 1 + (tmr >> 2), bit = 1 << (tmr & 3), next = tmr + 1;

	if (mn.icrh[g] & bit || !(mn.icrl[g] >> 4 & bit))
		return 0;
	if (next < MN10200_NUM_TIMERS_8BIT && (mn.simple_timer[next].mode & 0x83) == 0x81)
		return 0;
	return 1;
}

/* time to the next underflow of a sleeping timer */
static double timer_asleep_left(int tmr)
{
	double elapsed = timer_get_time() - timer_t0[tmr];
	double left = timer_period[tmr] - fmod(elapsed, timer_period[tmr]);

	return left > 0 ? left : timer_period[tmr];
}

static void timer_wake(int tmr)
{
	if (timer_asleep[tmr])
	{
		timer_asleep[tmr] = 0;
		timer_adjust(timer_timers[tmr], timer_asleep_left(tmr), tmr, 0);
	}
}

/* the irq control of a group changed */
static void timer_wake_group(int g)
{
	int tmr;

	for (tmr = (g - 1) * 4; tmr < (g - 1) * 4 + 4 && tmr < MN10200_NUM_TIMERS_8BIT; tmr++)
		if (tmr >= 0)
			timer_wake(tmr);
}

static void simple_timer_cb(int param)
{
	int tmr = param;

	/* handle our expiring and also tick our cascaded children */
	if (timer_tick_simple(tmr) == 2)
		mn.simple_timer[tmr].cur = 0xff; /* cascaded and no underflow occurred */
	else
		mn.simple_timer[tmr].cur = mn.simple_timer[tmr].base;

	/* refresh this timer */
	refresh_timer(tmr);

	if (timer_on[tmr] && timer_unseen(tmr))
	{
		int p = mn.simple_timer[tmr].mode & 1;

		timer_asleep[tmr] = 1;
		timer_t0[tmr] = timer_get_time();
		timer_period[tmr] = mn.sysclock_period * (mn.prescaler[p].base + 1) * (mn.simple_timer[tmr].cur + 1);
		timer_adjust(timer_timers[tmr], TIME_NEVER, tmr, 0);
	}
}

static void illegal(UINT8 prefix, UINT8 op)
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: illegal opcode %x %x @ PC=%x\n", prefix, op, mn.pc);
	mn.nmicr |= 2;
}

static UINT32 do_add3(UINT32 a, UINT32 b, UINT32 c)
{
	UINT32 r = (a & 0xffffff) + (b & 0xffffff) + c;

	mn.psw &= 0xff00;
	if ((a^r) & (b^r) & 0x00800000)
		mn.psw |= FLAG_VX;
	if (r & 0x01000000)
		mn.psw |= FLAG_CX;
	if (r & 0x00800000)
		mn.psw |= FLAG_NX;
	if ((r & 0x00ffffff) == 0)
		mn.psw |= FLAG_ZX;
	if ((a^r) & (b^r) & 0x00008000)
		mn.psw |= FLAG_VF;
	if (((a & 0xffff) + (b & 0xffff) + c) & 0x00010000)
		mn.psw |= FLAG_CF;
	if (r & 0x00008000)
		mn.psw |= FLAG_NF;
	if ((r & 0x0000ffff) == 0)
		mn.psw |= FLAG_ZF;

	return r;
}

static UINT32 do_sub3(UINT32 a, UINT32 b, UINT32 c)
{
	UINT32 r = (a & 0xffffff) - (b & 0xffffff) - c;

	mn.psw &= 0xff00;
	if ((a^b) & (a^r) & 0x00800000)
		mn.psw |= FLAG_VX;
	if (r & 0x01000000)
		mn.psw |= FLAG_CX;
	if (r & 0x00800000)
		mn.psw |= FLAG_NX;
	if ((r & 0x00ffffff) == 0)
		mn.psw |= FLAG_ZX;
	if ((a^b) & (a^r) & 0x00008000)
		mn.psw |= FLAG_VF;
	if (((a & 0xffff) - (b & 0xffff) - c) & 0x00010000)
		mn.psw |= FLAG_CF;
	if (r & 0x00008000)
		mn.psw |= FLAG_NF;
	if ((r & 0x0000ffff) == 0)
		mn.psw |= FLAG_ZF;

	return r;
}

static void test_nz16(UINT16 v)
{
	mn.psw &= 0xfff0;
	if (v & 0x8000)
		mn.psw |= FLAG_NF;
	if (v == 0)
		mn.psw |= FLAG_ZF;
}

static void do_jsr(UINT32 to, UINT32 ret)
{
	mn.a[3] -= 4;
	write_mem24(mn.a[3], ret);
	change_pc(to);
}

static void do_branch(int condition)
{
	if (condition)
	{
		mn10200_ICount -= 1;
		change_pc(mn.pc + (INT8)read_arg8(mn.pc));
	}
}

static void execute_run(void)
{
	UINT8 op, v;
	UINT16 mask0;
	UINT32 d, res, n, q, r;

	while (mn10200_ICount > 0)
	{
	/* internal peripheral, external pin, or prev instruction may have changed irq state */
	while (mn.possible_irq)
	{
		mn.possible_irq = 0;
		check_irq();
	}

	CALL_MAME_DEBUG;

	mn10200_ICount -= 1;
	op = read_arg8(mn.pc);
	mn.pc += 1;

	/* main opcodes */
	switch (op)
	{
		/* mov dm, (an) */
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			write_mem16(mn.a[op>>2&3], mn.d[op&3]);
			break;

		/* movb dm, (an) */
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			write_mem8(mn.a[op>>2&3], mn.d[op&3]); /* note: error in manual */
			break;

		/* mov (an), dm */
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			mn.d[op&3] = (INT16)read_mem16(mn.a[op>>2&3]);
			break;

		/* movbu (an), dm */
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
			mn.d[op&3] = read_mem8(mn.a[op>>2&3]);
			break;

		/* mov dm, (d8, an) */
		case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			write_mem16((mn.a[op>>2&3] + (INT8)read_arg8(mn.pc)), mn.d[op&3]);
			mn.pc += 1;
			break;

		/* mov am, (d8, an) */
		case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			mn10200_ICount -= 1;
			write_mem24((mn.a[op>>2&3] + (INT8)read_arg8(mn.pc)), mn.a[op&3]);
			mn.pc += 1;
			break;

		/* mov (d8, an), dm */
		case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			mn.d[op&3] = (INT16)read_mem16(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc));
			mn.pc += 1;
			break;

		/* mov (d8, an), am */
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			mn10200_ICount -= 1;
			mn.a[op&3] = read_mem24(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc));
			mn.pc += 1;
			break;

		/* mov dn, dm */
		case 0x81: case 0x82: case 0x83: case 0x84: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8b: case 0x8c: case 0x8d: case 0x8e:
			mn.d[op&3] = mn.d[op>>2&3];
			break;

		/* mov imm8, dn */
		case 0x80: case 0x85: case 0x8a: case 0x8f:
			mn.d[op&3] = (INT8)read_arg8(mn.pc);
			mn.pc += 1;
			break;

		/* add dn, dm */
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			mn.d[op&3] = do_add3(mn.d[op&3], mn.d[op>>2&3], 0);
			break;

		/* sub dn, dm */
		case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
			mn.d[op&3] = do_sub3(mn.d[op&3], mn.d[op>>2&3], 0);
			break;

		/* extx dn */
		case 0xb0: case 0xb1: case 0xb2: case 0xb3:
			mn.d[op&3] = (INT16)mn.d[op&3];
			break;

		/* extxu dn */
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
			mn.d[op&3] = (UINT16)mn.d[op&3];
			break;

		/* extxb dn */
		case 0xb8: case 0xb9: case 0xba: case 0xbb:
			mn.d[op&3] = (INT8)mn.d[op&3];
			break;

		/* extxbu dn */
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			mn.d[op&3] = (UINT8)mn.d[op&3];
			break;

		/* mov dn, (abs16) */
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
			write_mem16(read_arg16(mn.pc), mn.d[op&3]);
			mn.pc += 2;
			break;

		/* movb dn, (abs16) */
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
			write_mem8(read_arg16(mn.pc), mn.d[op&3]);
			mn.pc += 2;
			break;

		/* mov (abs16), dn */
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
			mn.d[op&3] = (INT16)read_mem16(read_arg16(mn.pc));
			mn.pc += 2;
			break;

		/* movbu (abs16), dn */
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
			mn.d[op&3] = read_mem8(read_arg16(mn.pc));
			mn.pc += 2;
			break;

		/* add imm8, an */
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
			mn.a[op&3] = do_add3(mn.a[op&3], (INT8)read_arg8(mn.pc), 0);
			mn.pc += 1;
			break;

		/* add imm8, dn */
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
			mn.d[op&3] = do_add3(mn.d[op&3], (INT8)read_arg8(mn.pc), 0);
			mn.pc += 1;
			break;

		/* cmp imm8, dn */
		case 0xd8: case 0xd9: case 0xda: case 0xdb:
			do_sub3(mn.d[op&3], (INT8)read_arg8(mn.pc), 0);
			mn.pc += 1;
			break;

		/* mov imm16, an */
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
			mn.a[op&3] = read_arg16(mn.pc);
			mn.pc += 2;
			break;

		/* blt label8 */
		case 0xe0:
			do_branch(((mn.psw & (FLAG_NF|FLAG_VF)) == FLAG_NF) || ((mn.psw & (FLAG_NF|FLAG_VF)) == FLAG_VF)); /* (VF^NF)=1 */
			mn.pc += 1;
			break;

		/* bgt label8 */
		case 0xe1:
			do_branch(((mn.psw & (FLAG_ZF|FLAG_NF|FLAG_VF)) == 0) || ((mn.psw & (FLAG_ZF|FLAG_NF|FLAG_VF)) == (FLAG_NF|FLAG_VF))); /* ((VF^NF)|ZF)=0 */
			mn.pc += 1;
			break;

		/* bge label8 */
		case 0xe2:
			do_branch(((mn.psw & (FLAG_NF|FLAG_VF)) == 0) || ((mn.psw & (FLAG_NF|FLAG_VF)) == (FLAG_NF|FLAG_VF))); /* (VF^NF)=0 */
			mn.pc += 1;
			break;

		/* ble label8 */
		case 0xe3:
			do_branch((mn.psw & FLAG_ZF) || ((mn.psw & (FLAG_NF|FLAG_VF)) == FLAG_NF) || ((mn.psw & (FLAG_NF|FLAG_VF)) == FLAG_VF)); /* ((VF^NF)|ZF)=1 */
			mn.pc += 1;
			break;

		/* bcs label8 */
		case 0xe4:
			do_branch(mn.psw & FLAG_CF); /* CF=1 */
			mn.pc += 1;
			break;

		/* bhi label8 */
		case 0xe5:
			do_branch(!(mn.psw & (FLAG_ZF|FLAG_CF))); /* (CF|ZF)=0 */
			mn.pc += 1;
			break;

		/* bcc label8 */
		case 0xe6:
			do_branch(!(mn.psw & FLAG_CF)); /* CF=0 */
			mn.pc += 1;
			break;

		/* bls label8 */
		case 0xe7:
			do_branch(mn.psw & (FLAG_ZF|FLAG_CF)); /* (CF|ZF)=1 */
			mn.pc += 1;
			break;

		/* beq label8 */
		case 0xe8:
			do_branch(mn.psw & FLAG_ZF); /* ZF=1 */
			mn.pc += 1;
			break;

		/* bne label8 */
		case 0xe9:
			do_branch(!(mn.psw & FLAG_ZF)); /* ZF=0 */
			mn.pc += 1;
			break;

		/* bra label8 */
		case 0xea:
			do_branch(1);
			mn.pc += 1;
			break;

		/* rti */
		case 0xeb:
			mn10200_ICount -= 5;
			mn.psw = read_mem16(mn.a[3]);
			change_pc(read_mem24(mn.a[3] + 2));
			mn.a[3] += 6;
			mn.possible_irq = 1;
			break;

		/* cmp imm16, an */
		case 0xec: case 0xed: case 0xee: case 0xef:
			do_sub3(mn.a[op&3], read_arg16(mn.pc), 0);
			mn.pc += 2;
			break;

		/* nop */
		case 0xf6:
			break;

		/* mov imm16, dn */
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
			mn.d[op&3] = (INT16)read_arg16(mn.pc);
			mn.pc += 2;
			break;

		/* jmp label16 */
		case 0xfc:
			mn10200_ICount -= 1;
			change_pc(mn.pc + 2 + (INT16)read_arg16(mn.pc));
			break;

		/* jsr label16 */
		case 0xfd:
			mn10200_ICount -= 3;
			do_jsr(mn.pc + 2 + (INT16)read_arg16(mn.pc), mn.pc + 2);
			break;

		/* rts */
		case 0xfe:
			mn10200_ICount -= 4;
			change_pc(read_mem24(mn.a[3]));
			mn.a[3] += 4;
			break;

		default:
			illegal(0, op);
			mn.possible_irq = 1;
			break;



		/* extended code f0 (2 bytes) */
		case 0xf0:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;

		switch (op)
		{
		/* jmp (an) */
		case 0x00: case 0x04: case 0x08: case 0x0c:
			mn10200_ICount -= 1;
			change_pc(mn.a[op>>2&3]);
			break;

		/* jsr (an) */
		case 0x01: case 0x05: case 0x09: case 0x0d:
			mn10200_ICount -= 3;
			do_jsr(mn.a[op>>2&3], mn.pc);
			break;

		/* bset dm, (an) */
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
		{
			mn10200_ICount -= 3;
			v = read_mem8(mn.a[op>>2&3]);
			test_nz16(v & mn.d[op&3]);
			write_mem8(mn.a[op>>2&3], v | mn.d[op&3]);
			break;
		}

		/* bclr dm, (an) */
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
		{
			mn10200_ICount -= 3;
			v = read_mem8(mn.a[op>>2&3]);
			test_nz16(v & mn.d[op&3]);
			write_mem8(mn.a[op>>2&3], v & ~mn.d[op&3]);
			break;
		}

		/* movb (di, an), dm */
		case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			mn.d[op&3] = (INT8)read_mem8(mn.a[op>>2&3] + mn.d[op>>4&3]);
			break;

		/* movbu (di, an), dm */
		case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
		case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			mn.d[op&3] = read_mem8(mn.a[op>>2&3] + mn.d[op>>4&3]);
			break;

		/* movb dm, (di, an) */
		case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf:
		case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb: case 0xec: case 0xed: case 0xee: case 0xef:
		case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
			write_mem8(mn.a[op>>2&3] + mn.d[op>>4&3], mn.d[op&3]);
			break;

		default:
			illegal(0xf0, op);
			mn.possible_irq = 1;
			break;
		}
		break;



		/* extended code f1 (2 bytes) */
		case 0xf1:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;

		switch (op & 0xc0)
		{
		/* mov (di, an), am */
		case 0x00:
			mn10200_ICount -= 1;
			mn.a[op&3] = read_mem24(mn.a[op>>2&3] + mn.d[op>>4&3]);
			break;

		/* mov (di, an), dm */
		case 0x40:
			mn.d[op&3] = (INT16)read_mem16(mn.a[op>>2&3] + mn.d[op>>4&3]);
			break;

		/* mov am, (di, an) */
		case 0x80:
			mn10200_ICount -= 1;
			write_mem24(mn.a[op>>2&3] + mn.d[op>>4&3], mn.a[op&3]);
			break;

		/* mov dm, (di, an) */
		case 0xc0:
			write_mem16(mn.a[op>>2&3] + mn.d[op>>4&3], mn.d[op&3]);
			break;
		}
		break;



		/* extended code f2 (2 bytes) */
		case 0xf2:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;

		switch (op & 0xf0)
		{
		/* add dm, an */
		case 0x00:
			mn.a[op&3] = do_add3(mn.a[op&3], mn.d[op>>2&3], 0);
			break;

		/* sub dm, an */
		case 0x10:
			mn.a[op&3] = do_sub3(mn.a[op&3], mn.d[op>>2&3], 0);
			break;

		/* cmp dm, an */
		case 0x20:
			do_sub3(mn.a[op&3], mn.d[op>>2&3], 0);
			break;

		/* mov dm, an */
		case 0x30:
			mn.a[op&3] = mn.d[op>>2&3];
			break;

		/* add an, am */
		case 0x40:
			mn.a[op&3] = do_add3(mn.a[op&3], mn.a[op>>2&3], 0);
			break;

		/* sub an, am */
		case 0x50:
			mn.a[op&3] = do_sub3(mn.a[op&3], mn.a[op>>2&3], 0);
			break;

		/* cmp an, am */
		case 0x60:
			do_sub3(mn.a[op&3], mn.a[op>>2&3], 0);
			break;

		/* mov an, am */
		case 0x70:
			mn.a[op&3] = mn.a[op>>2&3];
			break;

		/* addc dn, dm */
		case 0x80:
		{
			mask0 = ~FLAG_ZF | (mn.psw & FLAG_ZF);
			mn.d[op&3] = do_add3(mn.d[op&3], mn.d[op>>2&3], (mn.psw & FLAG_CF) ? 1 : 0);
			mn.psw &= mask0; /* ZF can only be set if it was set before the operation */
			break;
		}

		/* subc dn, dm */
		case 0x90:
		{
			mask0 = ~FLAG_ZF | (mn.psw & FLAG_ZF);
			mn.d[op&3] = do_sub3(mn.d[op&3], mn.d[op>>2&3], (mn.psw & FLAG_CF) ? 1 : 0);
			mn.psw &= mask0; /* ZF can only be set if it was set before the operation */
			break;
		}

		/* add an, dm */
		case 0xc0:
			mn.d[op&3] = do_add3(mn.d[op&3], mn.a[op>>2&3], 0);
			break;

		/* sub an, dm */
		case 0xd0:
			mn.d[op&3] = do_sub3(mn.d[op&3], mn.a[op>>2&3], 0);
			break;

		/* cmp an, dm */
		case 0xe0:
			do_sub3(mn.d[op&3], mn.a[op>>2&3], 0);
			break;

		/* mov an, dm */
		case 0xf0:
			mn.d[op&3] = mn.a[op>>2&3];
			break;

		default:
			illegal(0xf2, op);
			mn.possible_irq = 1;
			break;
		}
		break;



		/* extended code f3 (2 bytes) */
		case 0xf3:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;

		switch (op)
		{
		/* and dn, dm */
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			test_nz16(mn.d[op&3] &= 0xff0000 | mn.d[op>>2&3]);
			break;

		/* or dn, dm */
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			test_nz16(mn.d[op&3] |= 0x00ffff & mn.d[op>>2&3]);
			break;

		/* xor dn, dm */
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			test_nz16(mn.d[op&3] ^= 0x00ffff & mn.d[op>>2&3]);
			break;

		/* rol dn */
		case 0x30: case 0x31: case 0x32: case 0x33:
		{
			d = mn.d[op&3];
			test_nz16(mn.d[op&3] = (d & 0xff0000) | ((d << 1) & 0x00fffe) | ((mn.psw & FLAG_CF) ? 1 : 0));
			if (d & 0x8000)
				mn.psw |= FLAG_CF;
			break;
		}

		/* ror dn */
		case 0x34: case 0x35: case 0x36: case 0x37:
		{
			d = mn.d[op&3];
			test_nz16(mn.d[op&3] = (d & 0xff0000) | ((d >> 1) & 0x007fff) | ((mn.psw & FLAG_CF) ? 0x8000 : 0));
			if (d & 1)
				mn.psw |= FLAG_CF;
			break;
		}

		/* asr dn */
		case 0x38: case 0x39: case 0x3a: case 0x3b:
		{
			d = mn.d[op&3];
			test_nz16(mn.d[op&3] = (d & 0xff8000) | ((d >> 1) & 0x007fff));
			if (d & 1)
				mn.psw |= FLAG_CF;
			break;
		}

		/* lsr dn */
		case 0x3c: case 0x3d: case 0x3e: case 0x3f:
		{
			d = mn.d[op&3];
			test_nz16(mn.d[op&3] = (d & 0xff0000) | ((d >> 1) & 0x007fff));
			if (d & 1)
				mn.psw |= FLAG_CF;
			break;
		}

		/* mul dn, dm */
		case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		{
			mn10200_ICount -= 10;
			res = ((INT16)mn.d[op&3]) * ((INT16)mn.d[op>>2&3]);
			mn.d[op&3] = res & 0xffffff;
			mn.psw &= 0xff00; /* f4 is undefined */
			if (res & 0x80000000)
				mn.psw |= FLAG_NF;
			else if (res == 0)
				mn.psw |= FLAG_ZF;
			mn.mdr = res >> 16;
			break;
		}

		/* mulu dn, dm */
		case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		{
			mn10200_ICount -= 10;
			res = ((UINT16)mn.d[op&3]) * ((UINT16)mn.d[op>>2&3]);
			mn.d[op&3] = res & 0xffffff;
			mn.psw &= 0xff00; /* f4 is undefined */
			if (res & 0x80000000)
				mn.psw |= FLAG_NF;
			else if (res == 0)
				mn.psw |= FLAG_ZF;
			mn.mdr = res >> 16;
			break;
		}

		/* divu dn, dm */
		case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
		{
			mn10200_ICount -= 11;
			mn.psw &= 0xff00; /* f7 may be undefined */

			n = (mn.mdr << 16) | (UINT16)mn.d[op&3];
			d = (UINT16)mn.d[op>>2&3];
			if (d == 0)
			{
				/* divide by 0 */
				mn.psw |= FLAG_VF;
				break;
			}
			q = n / d;
			r = n % d;
			if (q >= 0x10000)
			{
				/* overflow (Dm and MDR are undefined) */
				mn.psw |= FLAG_VF;
				break;
			}
			mn.d[op&3] = q;
			mn.mdr = r;
			if (q == 0)
				mn.psw |= FLAG_ZF | FLAG_ZX;
			if (q & 0x8000)
				mn.psw |= FLAG_NF;
			break;
		}

		/* cmp dn, dm */
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			do_sub3(mn.d[op&3], mn.d[op>>2&3], 0);
			break;

		/* mov dn, mdr */
		case 0xc0: case 0xc4: case 0xc8: case 0xcc:
			mn.mdr = mn.d[op>>2&3];
			break;

		/* ext dn */
		case 0xc1: case 0xc5: case 0xc9: case 0xcd:
			mn10200_ICount -= 1;
			mn.mdr = (mn.d[op>>2&3] & 0x8000) ? 0xffff : 0x0000;
			break;

		/* mov dn, psw */
		case 0xd0: case 0xd4: case 0xd8: case 0xdc:
			mn10200_ICount -= 1;
			mn.psw = mn.d[op>>2&3];
			mn.possible_irq = 1;
			break;

		/* mov mdr, dn */
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
			mn.d[op&3] = mn.mdr;
			break;

		/* not dn */
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
			test_nz16(mn.d[op&3] ^= 0x00ffff);
			break;

		/* mov psw, dn */
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
			mn.d[op&3] = mn.psw;
			break;

		default:
			illegal(0xf3, op);
			mn.possible_irq = 1;
			break;
		}
		break;



		/* extended code f4 (5 bytes) */
		case 0xf4:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;
			mn10200_ICount -= 1;

		switch (op)
		{
		/* mov dm, (d24, an) */
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			write_mem16(read_arg24(mn.pc) + mn.a[op>>2&3], mn.d[op&3]);
			break;

		/* mov am, (d24, an) */
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			mn10200_ICount -= 1;
			write_mem24(read_arg24(mn.pc) + mn.a[op>>2&3], mn.a[op&3]);
			break;

		/* movb dm, (d24, an) */
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			write_mem8(read_arg24(mn.pc) + mn.a[op>>2&3], mn.d[op&3]);
			break;

		/* movx dm, (d24, an) */
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
			mn10200_ICount -= 1;
			write_mem24(read_arg24(mn.pc) + mn.a[op>>2&3], mn.d[op&3]);
			break;

		/* mov dn, (abs24) */
		case 0x40: case 0x41: case 0x42: case 0x43:
			write_mem16(read_arg24(mn.pc), mn.d[op&3]);
			break;

		/* movb dn, (abs24) */
		case 0x44: case 0x45: case 0x46: case 0x47:
			write_mem8(read_arg24(mn.pc), mn.d[op&3]);
			break;

		/* mov an, (abs24) */
		case 0x50: case 0x51: case 0x52: case 0x53:
			mn10200_ICount -= 1;
			write_mem24(read_arg24(mn.pc), mn.a[op&3]);
			break;

		/* add imm24, dn */
		case 0x60: case 0x61: case 0x62: case 0x63:
			mn.d[op&3] = do_add3(mn.d[op&3], read_arg24(mn.pc), 0);
			break;

		/* add imm24, an */
		case 0x64: case 0x65: case 0x66: case 0x67:
			mn.a[op&3] = do_add3(mn.a[op&3], read_arg24(mn.pc), 0);
			break;

		/* sub imm24, dn */
		case 0x68: case 0x69: case 0x6a: case 0x6b:
			mn.d[op&3] = do_sub3(mn.d[op&3], read_arg24(mn.pc), 0);
			break;

		/* sub imm24, an */
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			mn.a[op&3] = do_sub3(mn.a[op&3], read_arg24(mn.pc), 0);
			break;

		/* mov imm24, dn */
		case 0x70: case 0x71: case 0x72: case 0x73:
			mn.d[op&3] = read_arg24(mn.pc);
			break;

		/* mov imm24, an */
		case 0x74: case 0x75: case 0x76: case 0x77:
			mn.a[op&3] = read_arg24(mn.pc);
			break;

		/* cmp imm24, dn */
		case 0x78: case 0x79: case 0x7a: case 0x7b:
			do_sub3(mn.d[op&3], read_arg24(mn.pc), 0);
			break;

		/* cmp imm24, an */
		case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			do_sub3(mn.a[op&3], read_arg24(mn.pc), 0);
			break;

		/* mov (d24, an), dm */
		case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
			mn.d[op&3] = (INT16)read_mem16(mn.a[op>>2&3] + read_arg24(mn.pc));
			break;

		/* movbu (d24, an), dm */
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			mn.d[op&3] = read_mem8(mn.a[op>>2&3] + read_arg24(mn.pc));
			break;

		/* movb (d24, an), dm */
		case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
			mn.d[op&3] = (INT8)read_mem8(mn.a[op>>2&3] + read_arg24(mn.pc));
			break;

		/* movx (d24, an), dm */
		case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			mn10200_ICount -= 1;
			mn.d[op&3] = read_mem24(mn.a[op>>2&3] + read_arg24(mn.pc));
			break;

		/* mov (abs24), dn */
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
			mn.d[op&3] = (INT16)read_mem16(read_arg24(mn.pc));
			break;

		/* movb (abs24), dn */
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
			mn.d[op&3] = (INT8)read_mem8(read_arg24(mn.pc));
			break;

		/* movbu (abs24), dn */
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
			mn.d[op&3] = read_mem8(read_arg24(mn.pc));
			break;

		/* mov (abs24), an */
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
			mn10200_ICount -= 1;
			mn.a[op&3] = read_mem24(read_arg24(mn.pc));
			break;

		/* jmp label24 */
		case 0xe0:
			mn10200_ICount -= 1;
			change_pc(mn.pc + read_arg24(mn.pc));
			break;

		/* jsr label24 */
		case 0xe1:
			mn10200_ICount -= 2;
			do_jsr(mn.pc + read_arg24(mn.pc), mn.pc + 3);
			break;

		/* mov (d24, an), am */
		case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
			mn10200_ICount -= 1;
			mn.a[op&3] = read_mem24(mn.a[op>>2&3] + read_arg24(mn.pc));
			break;

		default:
			illegal(0xf4, op);
			mn.possible_irq = 1;
			break;
		}
		mn.pc += 3;
		break;



		/* extended code f5 (3 bytes) */
		case 0xf5:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;

		switch (op)
		{
		/* and imm8, dn */
		case 0x00: case 0x01: case 0x02: case 0x03:
			test_nz16(mn.d[op&3] &= 0xff0000 | read_arg8(mn.pc));
			break;

		/* btst imm8, dn */
		case 0x04: case 0x05: case 0x06: case 0x07:
			test_nz16(mn.d[op&3] & read_arg8(mn.pc));
			break;

		/* or imm8, dn */
		case 0x08: case 0x09: case 0x0a: case 0x0b:
			test_nz16(mn.d[op&3] |= read_arg8(mn.pc));
			break;

		/* addnf imm8, an */
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			mn.a[op&3] = mn.a[op&3] + (INT8)read_arg8(mn.pc);
			break;

		/* movb dm, (d8, an) */
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			write_mem8(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc), mn.d[op&3]);
			break;

		/* movb (d8, an), dm */
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			mn.d[op&3] = (INT8)read_mem8(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc));
			break;

		/* movbu (d8, an), dm */
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
			mn.d[op&3] = read_mem8(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc));
			break;

		/* movx dm, (d8, an) */
		case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			mn10200_ICount -= 1;
			write_mem24(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc), mn.d[op&3]);
			break;

		/* movx (d8, an), dm */
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			mn10200_ICount -= 1;
			mn.d[op&3] = read_mem24(mn.a[op>>2&3] + (INT8)read_arg8(mn.pc));
			break;

		/* bltx label8 */
		case 0xe0:
			do_branch(((mn.psw & (FLAG_NX|FLAG_VX)) == FLAG_NX) || ((mn.psw & (FLAG_NX|FLAG_VX)) == FLAG_VX)); /* (VX^NX)=1 */
			break;

		/* bgtx label8 */
		case 0xe1:
			do_branch(((mn.psw & (FLAG_ZX|FLAG_NX|FLAG_VX)) == 0) || ((mn.psw & (FLAG_ZX|FLAG_NX|FLAG_VX)) == (FLAG_NX|FLAG_VX))); /* ((VX^NX)|ZX)=0 */
			break;

		/* bgex label8 */
		case 0xe2:
			do_branch(((mn.psw & (FLAG_NX|FLAG_VX)) == 0) || ((mn.psw & (FLAG_NX|FLAG_VX)) == (FLAG_NX|FLAG_VX))); /* (VX^NX)=0 */
			break;

		/* blex label8 */
		case 0xe3:
			do_branch((mn.psw & FLAG_ZX) || ((mn.psw & (FLAG_NX|FLAG_VX)) == FLAG_NX) || ((mn.psw & (FLAG_NX|FLAG_VX)) == FLAG_VX)); /* ((VX^NX)|ZX)=1 */
			break;

		/* bcsx label8 */
		case 0xe4:
			do_branch(mn.psw & FLAG_CX); /* CX=1 */
			break;

		/* bhix label8 */
		case 0xe5:
			do_branch(!(mn.psw & (FLAG_ZX|FLAG_CX))); /* (CX|ZX)=0 */
			break;

		/* bccx label8 */
		case 0xe6:
			do_branch(!(mn.psw & FLAG_CX)); /* CX=0 */
			break;

		/* blsx label8 */
		case 0xe7:
			do_branch(mn.psw & (FLAG_ZX|FLAG_CX)); /* (CX|ZX)=1 */
			break;

		/* beqx label8 */
		case 0xe8:
			do_branch(mn.psw & FLAG_ZX); /* ZX=1 */
			break;

		/* bnex label8 */
		case 0xe9:
			do_branch(!(mn.psw & FLAG_ZX)); /* ZX=0 */
			break;

		/* bvcx label8 */
		case 0xec:
			do_branch(!(mn.psw & FLAG_VX)); /* VX=0 */
			break;

		/* bvsx label8 */
		case 0xed:
			do_branch(mn.psw & FLAG_VX); /* VX=1 */
			break;

		/* bncx label8 */
		case 0xee:
			do_branch(!(mn.psw & FLAG_NX)); /* NX=0 */
			break;

		/* bnsx label8 */
		case 0xef:
			do_branch(mn.psw & FLAG_NX); /* NX=1 */
			break;

		/* bvc label8 */
		case 0xfc:
			do_branch(!(mn.psw & FLAG_VF)); /* VF=0 */
			break;

		/* bvs label8 */
		case 0xfd:
			do_branch(mn.psw & FLAG_VF); /* VF=1 */
			break;

		/* bnc label8 */
		case 0xfe:
			do_branch(!(mn.psw & FLAG_NF)); /* NF=0 */
			break;

		/* bns label8 */
		case 0xff:
			do_branch(mn.psw & FLAG_NF); /* NF=1 */
			break;

		default:
			illegal(0xf5, op);
			mn.possible_irq = 1;
			break;
		}
		mn.pc += 1;
		break;



		/* extended code f7 (4 bytes) */
		case 0xf7:
			mn10200_ICount -= 1;
			op = read_arg8(mn.pc);
			mn.pc += 1;

		switch (op)
		{
		/* and imm16, dn */
		case 0x00: case 0x01: case 0x02: case 0x03:
			test_nz16(mn.d[op&3] &= 0xff0000 | read_arg16(mn.pc));
			break;

		/* btst imm16, dn */
		case 0x04: case 0x05: case 0x06: case 0x07:
			test_nz16(mn.d[op&3] & read_arg16(mn.pc));
			break;

		/* add imm16, an */
		case 0x08: case 0x09: case 0x0a: case 0x0b:
			mn.a[op&3] = do_add3(mn.a[op&3], (INT16)read_arg16(mn.pc), 0);
			break;

		/* sub imm16, an */
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			mn.a[op&3] = do_sub3(mn.a[op&3], (INT16)read_arg16(mn.pc), 0);
			break;

		/* and imm16, psw */
		case 0x10:
			mn10200_ICount -= 1;
			mn.psw &= read_arg16(mn.pc);
			break;

		/* or imm16, psw */
		case 0x14:
			mn10200_ICount -= 1;
			mn.psw |= read_arg16(mn.pc);
			mn.possible_irq = 1;
			break;

		/* add imm16, dn */
		case 0x18: case 0x19: case 0x1a: case 0x1b:
			mn.d[op&3] = do_add3(mn.d[op&3], (INT16)read_arg16(mn.pc), 0);
			break;

		/* sub imm16, dn */
		case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			mn.d[op&3] = do_sub3(mn.d[op&3], (INT16)read_arg16(mn.pc), 0);
			break;

		/* mov an, (abs16) */
		case 0x20: case 0x21: case 0x22: case 0x23:
			mn10200_ICount -= 1;
			write_mem24(read_arg16(mn.pc), mn.a[op&3]);
			break;

		/* mov (abs16), an */
		case 0x30: case 0x31: case 0x32: case 0x33:
			mn10200_ICount -= 1;
			mn.a[op&3] = read_mem24(read_arg16(mn.pc));
			break;

		/* or imm16, dn */
		case 0x40: case 0x41: case 0x42: case 0x43:
			test_nz16(mn.d[op&3] |= read_arg16(mn.pc));
			break;

		/* cmp imm16, dn */
		case 0x48: case 0x49: case 0x4a: case 0x4b:
			do_sub3(mn.d[op&3], (INT16)read_arg16(mn.pc), 0);
			break;

		/* xor imm16, dn */
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			test_nz16(mn.d[op&3] ^= read_arg16(mn.pc));
			break;

		/* movbu (d16, an), dm */
		case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			mn.d[op&3] = read_mem8(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc));
			break;

		/* movx dm, (d16, an) */
		case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			mn10200_ICount -= 1;
			write_mem24(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc), mn.d[op&3]);
			break;

		/* movx (d16, an), dm */
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			mn10200_ICount -= 1;
			mn.d[op&3] = read_mem24(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc));
			break;

		/* mov dm, (d16, an) */
		case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
			write_mem16(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc), mn.d[op&3]);
			break;

		/* movb dm, (d16, an) */
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			write_mem8(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc), mn.d[op&3]);
			break;

		/* mov am, (d16, an) */
		case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
			mn10200_ICount -= 1;
			write_mem24(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc), mn.a[op&3]);
			break;

		/* mov (d16, an), am */
		case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			mn10200_ICount -= 1;
			mn.a[op&3] = read_mem24(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc));
			break;

		/* mov (d16, an), dm */
		case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf:
			mn.d[op&3] = (INT16)read_mem16(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc));
			break;

		/* movb (d16, an), dm */
		case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
			mn.d[op&3] = (INT8)read_mem8(mn.a[op>>2&3] + (INT16)read_arg16(mn.pc));
			break;

		default:
			illegal(0xf7, op);
			mn.possible_irq = 1;
			break;
		}
		mn.pc += 2;
		break;

	} /* end main switch */

	} /* end loop */
}

static void io_control_w(offs_t offset, UINT8 data)
{
	switch (offset)
	{
		case 0x000:
			if(data & 12)
			{
				/* (log) */;
			}
			break;

		case 0x001:
			/* (log) */;
			break;

		case 0x002: case 0x003: /* Memory control */
			break;

		case 0x030: case 0x031: /* Memory mode reg 0 */
		case 0x032: case 0x033: /* Memory mode reg 1 */
		case 0x034: case 0x035: /* Memory mode reg 2 */
		case 0x036: case 0x037: /* Memory mode reg 3 */
			break;

		/* irq control */
		/* group 0, NMI control */
		case 0x040:
			mn.nmicr &= data; /* nmi ack */
			break;
		case 0x041:
			break;

		/* group 1-10, maskable irq control */
		case 0x042: case 0x044: case 0x046: case 0x048: case 0x04a:
		case 0x04c: case 0x04e: case 0x050: case 0x052: case 0x054:
			mn.icrl[(offset & 0x3f) >> 1] &= data; /* irq ack */
			timer_wake_group((offset & 0x3f) >> 1);

			/* group 8, external irqs might still be active */
			if (offset == 0x050)
				check_ext_irq();
			break;

		case 0x043: case 0x045: case 0x047: case 0x049: case 0x04b:
		case 0x04d: case 0x04f: case 0x051: case 0x053: case 0x055:
			mn.icrh[(offset & 0x3f) >> 1] = data;
			mn.possible_irq = 1;
			timer_wake_group((offset & 0x3f) >> 1);
			break;

		/* external irq control */
		case 0x056:
			mn.extmdl = data;
			check_ext_irq();
			break;
		case 0x057:
			mn.extmdh = data & 3;
			break;

		case 0x100: case 0x101: /* DRAM control reg */
		case 0x102: case 0x103: /* Refresh counter */
			break;

		case 0x180: case 0x190:
		{
			int ser = (offset-0x180) >> 4;
			/* const char *parity[8] = { "no", "1", "2", "3", "l", "h", "even", "odd" }; */
			/* const char *source[4] = { "sbt0", "timer 8", "2", "timer 9" }; */
			mn.serial[ser].ctrll = data;

			/* log_event("MN102", "Serial %d length=%c, parity=%s, stop=%c, source=%s", */
			/*     ser, */
			/*     data & 0x80 ? '8' : '7', parity[(data >> 4) & 7], */
			/*     data & 8 ? '2' : '1', source[data & 3]); */
			break;
		}

		case 0x181: case 0x191:
		{
			int ser = (offset-0x180) >> 4;
			mn.serial[ser].ctrlh = data;
			/* log_event("MN102", "Serial %d transmit=%s, receive=%s, break=%s, proto=%s, order=%s", */
			/*     ser, */
			/*     data & 0x80 ? "on" : "off", data & 0x40 ? "on" : "off", */
			/*     data & 0x20 ? "on" : "off", data & 8 ? "sync" : "async", */
			/*     data & 2 ? "msb" : "lsb"); */
			break;
		}

		case 0x182: case 0x192:
		{
			int ser = (offset-0x180) >> 4;
			mn.serial[ser].buf = data;
			/* (log) */;
			break;
		}

		case 0x1a0:
			/* (log) */;
			break;

		case 0x1a1:
			/* (log) */;
			break;

		/* 8-bit timers */
		/* timer base */
		case 0x210: case 0x211: case 0x212: case 0x213: case 0x214:
		case 0x215: case 0x216: case 0x217: case 0x218: case 0x219:
			mn.simple_timer[offset & 0xf].base = data;
			break;

		/* prescaler base */
		case 0x21a: case 0x21b:
			mn.prescaler[offset & 1].base = data;
			break;

		/* timer mode */
		case 0x220: case 0x221: case 0x222: case 0x223: case 0x224:
		case 0x225: case 0x226: case 0x227: case 0x228: case 0x229:
			/* a timer cascaded on the one below makes its underflows seen */
			if ((offset & 0xf) > 0)
				timer_wake((offset & 0xf) - 1);
			mn.simple_timer[offset & 0xf].mode = data & 0xc3;

			/* reload */
			if (data & 0x40)
				mn.simple_timer[offset & 0xf].cur = mn.simple_timer[offset & 0xf].base;

			refresh_timer(offset & 0xf);
			break;

		/* prescaler mode */
		case 0x22a: case 0x22b:
			mn.prescaler[offset & 1].mode = data & 0xc0;

			/* reload */
			if (data & 0x40)
				mn.prescaler[offset & 1].cur = mn.prescaler[offset & 1].base;

			refresh_all_timers();
			break;

		case 0x230: case 0x240: case 0x250:
		{
			/* const char *modes[4] = { "single", "double", "ioa", "iob" }; */
			/* const char *sources[8] = { "pres.0", "pres.1", "iob", "sysclk", "*4", "*1", "6", "7" }; */
			/* printf("MN10200: Timer %d comp=%s on_1=%s on_match=%s phase=%s source=%s\n", */
			/*     10 + ((offset-0x230) >> 4), */
			/*     modes[data >> 6], data & 0x20 ? "cleared" : "not cleared",  data & 0x10 ? "cleared" : "not cleared", */
			/*     data & 8 ? "tff" : "rsff", sources[data & 7]); */
			break;
		}

		case 0x231: case 0x241: case 0x251:
		{
			/* const char *modes[4] = { "up", "down", "up on ioa", "up on iob" }; */
			/* printf("MN10200: Timer %d %s ff=%s op=%s ext_trig=%s %s\n", */
			/* 10 + ((offset-0x230) >> 4), */
			/* data & 0x80 ? "enable" : "disable", data & 0x40 ? "operate" : "clear", */
			/* modes[(data >> 4) & 3], data & 2 ? "on" : "off", data & 1 ? "one-shot" : "repeat"); */

			break;
		}

		case 0x234: case 0x244: case 0x254:
			/* (log) */;
			break;

		case 0x235: case 0x245: case 0x255:
			/* (log) */;
			break;

		case 0x236: case 0x246: case 0x256:
			/* (log) */;
			break;

		case 0x237: case 0x247: case 0x257:
			break;

		case 0x238: case 0x248: case 0x258:
			/* (log) */;
			break;

		case 0x239: case 0x249: case 0x259:
			/* (log) */;
			break;

		case 0x23a: case 0x24a: case 0x25a:
			/* (log) */;
			break;

		case 0x23b: case 0x24b: case 0x25b:
			break;

		case 0x260: case 0x261:
		{
			/* const char *mode[4] = { "sysbuf", "4-phase", "4-phase 1/2", "3" }; */
			/* log_event("MN102", "Sync Output %c timing=%s out=%s dir=%s mode=%s", */
			/*     offset == 0x261 ? 'B' : 'A', */
			/*     data & 0x10 ? "12A" : "1", data & 8 ? "sync a" :"P13-10", */
			/*     data & 4 ? "ccw" : "cw", mode[data & 3]); */
			break;
		}

		case 0x262:
			/* (log) */;
			break;

		case 0x280: case 0x290: case 0x2a0: case 0x2b0: case 0x2c0: case 0x2d0: case 0x2e0: case 0x2f0:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].adr = (mn.dma[dma].adr & 0x00ffff00) | data;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d adr=%06x\n", dma, mn.dma[dma].adr);
			break;
		}

		case 0x281: case 0x291: case 0x2a1: case 0x2b1: case 0x2c1: case 0x2d1: case 0x2e1: case 0x2f1:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].adr = (mn.dma[dma].adr & 0x00ff00ff) | (data << 8);
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d adr=%06x\n", dma, mn.dma[dma].adr);
			break;
		}

		case 0x282: case 0x292: case 0x2a2: case 0x2b2: case 0x2c2: case 0x2d2: case 0x2e2: case 0x2f2:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].adr = (mn.dma[dma].adr & 0x0000ffff) | (data << 16);
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d adr=%06x\n", dma, mn.dma[dma].adr);
			break;
		}

		case 0x283: case 0x293: case 0x2a3: case 0x2b3: case 0x2c3: case 0x2d3: case 0x2e3: case 0x2f3:
			break;

		case 0x284: case 0x294: case 0x2a4: case 0x2b4: case 0x2c4: case 0x2d4: case 0x2e4: case 0x2f4:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].count = (mn.dma[dma].count & 0x00ffff00) | data;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d count=%06x\n", dma, mn.dma[dma].count);
			break;
		}

		case 0x285: case 0x295: case 0x2a5: case 0x2b5: case 0x2c5: case 0x2d5: case 0x2e5: case 0x2f5:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].count = (mn.dma[dma].count & 0x00ff00ff) | (data << 8);
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d count=%06x\n", dma, mn.dma[dma].count);
			break;
		}

		case 0x286: case 0x296: case 0x2a6: case 0x2b6: case 0x2c6: case 0x2d6: case 0x2e6: case 0x2f6:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].count = (mn.dma[dma].count & 0x0000ffff) | (data << 16);
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d count=%06x\n", dma, mn.dma[dma].count);
			break;
		}

		case 0x287: case 0x297: case 0x2a7: case 0x2b7: case 0x2c7: case 0x2d7: case 0x2e7: case 0x2f7:
			break;

		case 0x288: case 0x298: case 0x2a8: case 0x2b8: case 0x2c8: case 0x2d8: case 0x2e8: case 0x2f8:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].iadr = (mn.dma[dma].iadr & 0xff00) | data;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d iadr=%03x\n", dma, mn.dma[dma].iadr);
			break;
		}

		case 0x289: case 0x299: case 0x2a9: case 0x2b9: case 0x2c9: case 0x2d9: case 0x2e9: case 0x2f9:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].iadr = (mn.dma[dma].iadr & 0x00ff) | ((data & 3) << 8);
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d iadr=%03x\n", dma, mn.dma[dma].iadr);
			break;
		}

		case 0x28a: case 0x29a: case 0x2aa: case 0x2ba: case 0x2ca: case 0x2da: case 0x2ea: case 0x2fa:
		{
			static const char *const trans[4] = { "M-IO", "M-M", "M-X1", "m-X2" };
			static const char *const start[32] =
			{
				"soft", "a/d", "ser0tx", "set0rx", "ser1tx", "ser1rx",
				"timer0", "timer1", "timer2", "timer3", "timer4", "timer5", "timer6", "timer7", "timer8", "timer9",
				"timer10u", "timer10a", "timer10b",
				"timer11u", "timer11a", "timer12b",
				"timer12a", "timer12b",
				"irq0", "irq1", "irq2", "irq3",
				"X0e", "X1e", "X0l", "X1l"
			};

			int dma = (offset-0x280) >> 4;
			mn.dma[dma].ctrll = data;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d control ack=%s, trans=%s, start=%s\n",
				dma,
				data & 0x80 ? "level" : "pulse",
				trans[(data >> 5) & 3],
				start[data & 31]);

			break;
		}

		case 0x28b: case 0x29b: case 0x2ab: case 0x2bb: case 0x2cb: case 0x2db: case 0x2eb: case 0x2fb:
		{
			static const char *const tradr[4] = { "inc", "dec", "fixed", "reserved" };
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].ctrlh = data;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d control %s irq=%s %s %s dir=%s %s %s\n",
				dma,
				data & 0x80 ? "enable" : "disable",
				data & 0x40 ? "off" : "on",
				data & 0x20 ? "byte" : "word",
				data & 0x10 ? "burst" : "single",
				data & 0x08 ? "dst" : "src",
				data & 0x04 ? "continue" : "normal",
				tradr[data & 3]);

			break;
		}

		case 0x28c: case 0x29c: case 0x2ac: case 0x2bc: case 0x2cc: case 0x2dc: case 0x2ec: case 0x2fc:
		{
			int dma = (offset-0x280) >> 4;
			mn.dma[dma].irq = data & 7;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "MN10200: DMA %d irq=%d\n", dma, data & 7);
			break;
		}

		case 0x28d: case 0x29d: case 0x2ad: case 0x2bd: case 0x2cd: case 0x2dd: case 0x2ed: case 0x2fd:
			break;

		case 0x3b2:
			/* (log) */;
			break;

		case 0x3b3:
			/* (log) */;
			break;

		/* ports */
		/* pull-up control */
		case 0x3b0:
			mn.pplul = data & 0x7f;
			break;
		case 0x3b1:
			mn.ppluh = data & 0x3f;
			break;

		/* outputs */
		case 0x3c0:
			mn.port[0].out = data;
			port_w(0, mn.port[0].out | (mn.port[0].dir ^ 0xff));
			break;
		case 0x264:
			mn.port[1].out = data;
			port_w(1, mn.port[1].out | (mn.port[1].dir ^ 0xff));
			break;
		case 0x3c2:
			mn.port[2].out = data & 0x0f;
			port_w(2, mn.port[2].out | (mn.port[2].dir ^ 0x0f));
			break;
		case 0x3c3:
			mn.port[3].out = data & 0x1f;
			port_w(3, mn.port[3].out | (mn.port[3].dir ^ 0x1f));
			break;

		/* directions (0=input, 1=output) */
		case 0x3e0:
			mn.port[0].dir = data;
			break;
		case 0x3e1:
			mn.port[1].dir = data;
			break;
		case 0x3e2:
			mn.port[2].dir = data & 0x0f;
			break;
		case 0x3e3:
			mn.port[3].dir = data & 0x1f;
			break;

		/* port 3 output mode */
		case 0x3f3:
			mn.p3md = data & 0x7f;
			break;


		default:
			/* (log) */;
			break;
	}
}

static UINT8 io_control_r(offs_t offset)
{
	switch (offset)
	{
		/* active irq group */
		case 0x00e:
			return mn.iagr << 1;
		case 0x00f:
			return 0;

		/* irq control */
		/* group 0, NMI control */
		case 0x040:
			return mn.nmicr;
		case 0x041:
			return 0;

		/* group 1-10, maskable irq control */
		case 0x042: case 0x044: case 0x046: case 0x048: case 0x04a:
		case 0x04c: case 0x04e: case 0x050: case 0x052: case 0x054:
			/* low 4 bits are a mask of IEN and IRF */
			return (mn.icrl[(offset & 0x3f) >> 1] & 0xf0) | (mn.icrl[(offset & 0x3f) >> 1] >> 4 & mn.icrh[(offset & 0x3f) >> 1]);

		case 0x043: case 0x045: case 0x047: case 0x049: case 0x04b:
		case 0x04d: case 0x04f: case 0x051: case 0x053: case 0x055:
			return mn.icrh[(offset & 0x3f) >> 1];

		/* external irq control */
		case 0x056:
			return mn.extmdl;
		case 0x057:
			return mn.extmdh | (mn.p4 << 4);

		case 0x180: case 0x190:
			return mn.serial[(offset-0x180) >> 4].ctrll;

		case 0x181: case 0x191:
			return mn.serial[(offset-0x180) >> 4].ctrlh;

		case 0x182: /*case 0x192: */
		{
			int ser = (offset-0x180) >> 4;
			return mn.serial[ser].recv++;
		}

		case 0x183: /*case 0x193: */
			return 0x10;

		/* 8-bit timers */
		/* timer counter (not accurate) */
		case 0x200: case 0x201: case 0x202: case 0x203: case 0x204:
		case 0x205: case 0x206: case 0x207: case 0x208: case 0x209:
			return mn.simple_timer[offset & 0xf].cur;

		/* prescaler counter (not accurate) */
		case 0x20a: case 0x20b:
			return mn.prescaler[offset & 1].cur;

		/* timer base */
		case 0x210: case 0x211: case 0x212: case 0x213: case 0x214:
		case 0x215: case 0x216: case 0x217: case 0x218: case 0x219:
			return mn.simple_timer[offset & 0xf].base;

		/* prescaler base */
		case 0x21a: case 0x21b:
			return mn.prescaler[offset & 1].base;

		/* timer mode */
		case 0x220: case 0x221: case 0x222: case 0x223: case 0x224:
		case 0x225: case 0x226: case 0x227: case 0x228: case 0x229:
			return mn.simple_timer[offset & 0xf].mode;

		/* prescaler mode */
		case 0x22a: case 0x22b:
			return mn.prescaler[offset & 1].mode;

		case 0x28c: case 0x29c: case 0x2ac: case 0x2bc: case 0x2cc: case 0x2dc: case 0x2ec: case 0x2fc:
		{
			int dma = (offset-0x280) >> 4;
			return mn.dma[dma].irq;
		}

		/* ports */
		/* pull-up control */
		case 0x3b0:
			return mn.pplul;
		case 0x3b1:
			return mn.ppluh;

		/* outputs */
		case 0x3c0:
			return mn.port[0].out;
		case 0x264:
			return mn.port[1].out;
		case 0x3c2:
			return mn.port[2].out;
		case 0x3c3:
			return mn.port[3].out;

		/* inputs */
		case 0x3d0:
			return port_r(0) | mn.port[0].dir;
		case 0x3d1:
			return port_r(1) | mn.port[1].dir;
		case 0x3d2:
			return (port_r(2) & 0x0f) | mn.port[2].dir;
		case 0x3d3:
			return (port_r(3) & 0x1f) | mn.port[3].dir;

		/* directions (0=input, 1=output) */
		case 0x3e0:
			return mn.port[0].dir;
		case 0x3e1:
			return mn.port[1].dir;
		case 0x3e2:
			return mn.port[2].dir;
		case 0x3e3:
			return mn.port[3].dir;

		/* port 3 output mode */
		case 0x3f3:
			return mn.p3md;


		default:
			/* (log) */;
			break;
	}

	return 0;
}

/*************************************************************************/
/* 0.78 interface */

static void mn10200_presave(void)
{
	int tmr;
	for( tmr = 0; tmr < MN10200_NUM_TIMERS_8BIT; tmr++ )
		timer_left[ tmr ] = !timer_on[ tmr ] ? 0 : timer_asleep[ tmr ] ? timer_asleep_left( tmr ) : timer_timeleft( timer_timers[ tmr ] );
}

static void mn10200_postload(void)
{
	int tmr;
	for( tmr = 0; tmr < MN10200_NUM_TIMERS_8BIT; tmr++ )
	{
		timer_asleep[ tmr ] = 0;
		if( timer_on[ tmr ] )
			timer_adjust( timer_timers[ tmr ], timer_left[ tmr ], tmr, 0 );
		else
			timer_adjust( timer_timers[ tmr ], TIME_NEVER, tmr, 0 );
	}
}

void mn10200_init(void)
{
	int cpu = cpu_getactivecpu();
	int tmr;

	memset( &mn, 0, sizeof( mn ) );
	/* the peripherals run at the instruction clock (the crystal / 2) */
	mn.sysclock_period = TIME_IN_HZ( Machine->drv->cpu[ cpu ].cpu_clock );
	mn.p4 = 0xf;

	for( tmr = 0; tmr < MN10200_NUM_TIMERS_8BIT; tmr++ )
	{
		timer_timers[ tmr ] = timer_alloc( simple_timer_cb );
		timer_adjust( timer_timers[ tmr ], TIME_NEVER, tmr, 0 );
		timer_on[ tmr ] = 0;
		timer_asleep[ tmr ] = 0;
	}

	state_save_register_UINT32( "mn10200", cpu, "pc", &mn.pc, 1 );
	state_save_register_UINT32( "mn10200", cpu, "d", mn.d, 4 );
	state_save_register_UINT32( "mn10200", cpu, "a", mn.a, 4 );
	state_save_register_UINT16( "mn10200", cpu, "psw", &mn.psw, 1 );
	state_save_register_UINT16( "mn10200", cpu, "mdr", &mn.mdr, 1 );
	state_save_register_UINT8( "mn10200", cpu, "icrl", mn.icrl, MN10200_NUM_IRQ_GROUPS );
	state_save_register_UINT8( "mn10200", cpu, "icrh", mn.icrh, MN10200_NUM_IRQ_GROUPS );
	state_save_register_UINT8( "mn10200", cpu, "nmicr", &mn.nmicr, 1 );
	state_save_register_UINT8( "mn10200", cpu, "iagr", &mn.iagr, 1 );
	state_save_register_UINT8( "mn10200", cpu, "extmdl", &mn.extmdl, 1 );
	state_save_register_UINT8( "mn10200", cpu, "extmdh", &mn.extmdh, 1 );
	state_save_register_int( "mn10200", cpu, "possible_irq", &mn.possible_irq );
	state_save_register_UINT8( "mn10200", cpu, "timers", (UINT8 *)mn.simple_timer, sizeof( mn.simple_timer ) );
	state_save_register_UINT8( "mn10200", cpu, "prescalers", (UINT8 *)mn.prescaler, sizeof( mn.prescaler ) );
	state_save_register_UINT8( "mn10200", cpu, "timer_on", timer_on, MN10200_NUM_TIMERS_8BIT );
	state_save_register_double( "mn10200", cpu, "timer_left", timer_left, MN10200_NUM_TIMERS_8BIT );
	state_save_register_UINT8( "mn10200", cpu, "pplul", &mn.pplul, 1 );
	state_save_register_UINT8( "mn10200", cpu, "ppluh", &mn.ppluh, 1 );
	state_save_register_UINT8( "mn10200", cpu, "p3md", &mn.p3md, 1 );
	state_save_register_UINT8( "mn10200", cpu, "p4", &mn.p4, 1 );
	state_save_register_UINT8( "mn10200", cpu, "port", (UINT8 *)mn.port, sizeof( mn.port ) );
	state_save_register_UINT8( "mn10200", cpu, "serial", (UINT8 *)mn.serial, sizeof( mn.serial ) );
	state_save_register_func_presave( mn10200_presave );
	state_save_register_func_postload( mn10200_postload );
}

void mn10200_reset(void *param)
{
	int i, address;

	change_pc( 0x80000 );
	mn.psw = 0;

	/* note: officially, these registers are reset to 'undefined' */
	memset( mn.d, 0, sizeof( mn.d ) );
	memset( mn.a, 0, sizeof( mn.a ) );
	mn.mdr = 0;

	/* reset internal peripherals */
	mn.nmicr = 0;
	mn.iagr = 0;
	write_mem16( 0xfc00, 0x8000 );
	write_mem16( 0xfc02, 0x0737 );

	/* need to clear them twice since some rely on the value of others */
	for( i = 0; i < 2; i++ )
		for( address = 0xfc04; address < 0x10000; address++ )
			write_mem16( address, 0 );

	mn.possible_irq = 0;
}

void mn10200_exit(void)
{
}

int mn10200_execute(int cycles)
{
	mn10200_ICount = cycles;
	execute_run();
	return cycles - mn10200_ICount;
}

unsigned mn10200_get_context(void *dst)
{
	if( dst )
		*(mn10200_state *)dst = mn;
	return sizeof( mn10200_state );
}

void mn10200_set_context(void *src)
{
	if( src )
		mn = *(mn10200_state *)src;
}

unsigned mn10200_get_reg(int regnum)
{
	switch( regnum )
	{
		case REG_PC: case MN10200_PC: return mn.pc;
		case REG_SP: case MN10200_A3: return mn.a[ 3 ];
		case MN10200_PSW: return mn.psw;
		case MN10200_MDR: return mn.mdr;
		case MN10200_D0: return mn.d[ 0 ];
		case MN10200_D1: return mn.d[ 1 ];
		case MN10200_D2: return mn.d[ 2 ];
		case MN10200_D3: return mn.d[ 3 ];
		case MN10200_A0: return mn.a[ 0 ];
		case MN10200_A1: return mn.a[ 1 ];
		case MN10200_A2: return mn.a[ 2 ];
		case MN10200_NMICR: return mn.nmicr;
		case MN10200_IAGR: return mn.iagr;
		case REG_PREVIOUSPC: return mn.pc;
	}
	return 0;
}

void mn10200_set_reg(int regnum, unsigned val)
{
	switch( regnum )
	{
		case REG_PC: case MN10200_PC: change_pc( val ); break;
		case REG_SP: case MN10200_A3: mn.a[ 3 ] = val & 0xffffff; break;
		case MN10200_PSW: mn.psw = val; break;
		case MN10200_MDR: mn.mdr = val; break;
		case MN10200_D0: mn.d[ 0 ] = val; break;
		case MN10200_D1: mn.d[ 1 ] = val; break;
		case MN10200_D2: mn.d[ 2 ] = val; break;
		case MN10200_D3: mn.d[ 3 ] = val; break;
		case MN10200_A0: mn.a[ 0 ] = val; break;
		case MN10200_A1: mn.a[ 1 ] = val; break;
		case MN10200_A2: mn.a[ 2 ] = val; break;
	}
}

void mn10200_set_irq_callback(int (*callback)(int irqline))
{
}

const char *mn10200_info(void *context, int regnum)
{
	static char buffer[ 16 ][ 47 + 1 ];
	static int which = 0;
	mn10200_state *r = context ? (mn10200_state *)context : &mn;

	which = ( which + 1 ) % 16;
	buffer[ which ][ 0 ] = '\0';

	switch( regnum )
	{
		case CPU_INFO_REG+MN10200_PC: sprintf( buffer[ which ], "PC :%06X", r->pc ); break;
		case CPU_INFO_REG+MN10200_PSW: sprintf( buffer[ which ], "PSW:%04X", r->psw ); break;
		case CPU_INFO_REG+MN10200_MDR: sprintf( buffer[ which ], "MDR:%04X", r->mdr ); break;
		case CPU_INFO_REG+MN10200_D0: sprintf( buffer[ which ], "D0 :%06X", r->d[ 0 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_D1: sprintf( buffer[ which ], "D1 :%06X", r->d[ 1 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_D2: sprintf( buffer[ which ], "D2 :%06X", r->d[ 2 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_D3: sprintf( buffer[ which ], "D3 :%06X", r->d[ 3 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_A0: sprintf( buffer[ which ], "A0 :%06X", r->a[ 0 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_A1: sprintf( buffer[ which ], "A1 :%06X", r->a[ 1 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_A2: sprintf( buffer[ which ], "A2 :%06X", r->a[ 2 ] & 0xffffff ); break;
		case CPU_INFO_REG+MN10200_A3: sprintf( buffer[ which ], "A3 :%06X", r->a[ 3 ] & 0xffffff ); break;
		case CPU_INFO_FLAGS:
			sprintf( buffer[ which ], "%c%c%c%c%c%c%c%c",
				r->psw & FLAG_VX ? 'V' : '-', r->psw & FLAG_CX ? 'C' : '-',
				r->psw & FLAG_NX ? 'N' : '-', r->psw & FLAG_ZX ? 'Z' : '-',
				r->psw & FLAG_VF ? 'v' : '-', r->psw & FLAG_CF ? 'c' : '-',
				r->psw & FLAG_NF ? 'n' : '-', r->psw & FLAG_ZF ? 'z' : '-' );
			break;
		case CPU_INFO_NAME: return "MN10200";
		case CPU_INFO_FAMILY: return "Panasonic MN10200";
		case CPU_INFO_VERSION: return "1.0";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Olivier Galibert, R. Belmont, hap";
	}
	return buffer[ which ];
}

unsigned mn10200_dasm(char *buffer, unsigned pc)
{
	sprintf( buffer, "$%02X", read_mem8( pc ) );
	return 1;
}
