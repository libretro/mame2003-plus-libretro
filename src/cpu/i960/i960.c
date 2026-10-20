/***************************************************************************

    Intel 80960KA/KB, from current MAME (src/devices/cpu/i960/i960.cpp,
    copyright Farfetch'd, R. Belmont, BSD-3-Clause), in C for MAME 2003.

    Same instruction set, register cache, interrupt and stall handling
    as current MAME; the BURST memory flag is a driver callback
    (i960_set_burst_callback).

***************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "cpuintrf.h"
#include "state.h"
#include "mamedbg.h"
#include "log.h"
#include "i960.h"

#define I960_RCACHE_SIZE 4

#define SEXT( v, bits )		( (INT32)( (UINT32)( v ) << ( 32 - ( bits ) ) ) >> ( 32 - ( bits ) ) )
#define DWORD_ALIGNED( a )	( ( ( a ) & 3 ) == 0 )
#define WORD_ALIGNED( a )	( ( ( a ) & 1 ) == 0 )
#ifndef BIT
#define BIT( x, n )			( ( ( x ) >> ( n ) ) & 1 )
#endif

typedef struct
{
	UINT32 t1, t2;
	int index, size;
	int burst_mode;
	int iswriteop;
} i960_stall_state;

typedef struct
{
	UINT32 r[0x20];
	UINT32 rcache[I960_RCACHE_SIZE][0x10];
	UINT32 rcache_frame_addr[I960_RCACHE_SIZE];
	INT32 rcache_pos;
	double fp[4];
	UINT32 SAT, PRCB, PC, AC, IP, PIP, ICR;
	int immediate_irq, immediate_vector, immediate_pri;
	INT8 irq_line_state[4];
	int stalled;
	i960_stall_state stall_state;
} i960_regs;

static i960_regs i960;
int i960_icount;
static int (*i960_burst_cb)(UINT32 address);

#define m_r						i960.r
#define m_rcache				i960.rcache
#define m_rcache_frame_addr		i960.rcache_frame_addr
#define m_rcache_pos			i960.rcache_pos
#define m_fp					i960.fp
#define m_SAT					i960.SAT
#define m_PRCB					i960.PRCB
#define m_PC					i960.PC
#define m_AC					i960.AC
#define m_IP					i960.IP
#define m_PIP					i960.PIP
#define m_ICR					i960.ICR
#define m_immediate_irq			i960.immediate_irq
#define m_immediate_vector		i960.immediate_vector
#define m_immediate_pri			i960.immediate_pri
#define m_irq_line_state		i960.irq_line_state
#define m_stalled				i960.stalled
#define m_stall_state			i960.stall_state
#define m_icount				i960_icount

static INLINE int i960_is_burst( UINT32 address )
{
	return i960_burst_cb == NULL || i960_burst_cb( address );
}

static INLINE UINT32 i960_readop( UINT32 address )
{
	return cpu_readmem32ledw_dword( address );
}

static void i960_fatal( const char *fmt, ... )
{
	char buf[ 256 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( buf, fmt, va );
	va_end( va );
	log_cb( RETRO_LOG_ERROR, LOGPRE "%s", buf );
}

static INLINE float u2f( UINT32 v ) { union { float f; UINT32 u; } x; x.u = v; return x.f; }
static INLINE UINT32 f2u( float v ) { union { float f; UINT32 u; } x; x.f = v; return x.u; }
static INLINE double u2d( UINT64 v ) { union { double d; UINT64 u; } x; x.u = v; return x.d; }
static INLINE UINT64 d2u( double v ) { union { double d; UINT64 u; } x; x.d = v; return x.u; }
static INLINE UINT32 rotl32( UINT32 v, UINT32 n ) { n &= 31; return n ? ( v << n ) | ( v >> ( 32 - n ) ) : v; }

static void check_pending_irqs( void );
static void do_call( UINT32 adr, int type, UINT32 stack );

static UINT32 i960_read_dword_unaligned(UINT32 address)
{
	if (!DWORD_ALIGNED(address))
		return cpu_readmem32ledw(address) | cpu_readmem32ledw(address+1)<<8 | cpu_readmem32ledw(address+2)<<16 | cpu_readmem32ledw(address+3)<<24;
	else
		return cpu_readmem32ledw_dword(address);
}


static UINT16 i960_read_word_unaligned(UINT32 address)
{
	if (!WORD_ALIGNED(address))
		return cpu_readmem32ledw(address) | cpu_readmem32ledw(address+1)<<8;
	else
		return cpu_readmem32ledw_word(address);
}

static void i960_write_dword_unaligned(UINT32 address, UINT32 data)
{
	if (!DWORD_ALIGNED(address))
	{
		cpu_writemem32ledw(address, data & 0xff);
		cpu_writemem32ledw(address+1, (data>>8)&0xff);
		cpu_writemem32ledw(address+2, (data>>16)&0xff);
		cpu_writemem32ledw(address+3, (data>>24)&0xff);
	}
	else
	{
		cpu_writemem32ledw_dword(address, data);
	}
}


static void i960_write_word_unaligned(UINT32 address, UINT16 data)
{
	if (!WORD_ALIGNED(address))
	{
		cpu_writemem32ledw(address, data & 0xff);
		cpu_writemem32ledw(address+1, (data>>8)&0xff);
	}
	else
	{
		cpu_writemem32ledw_word(address, data);
	}
}

static void send_iac(UINT32 adr)
{
	UINT32 iac[4];
	iac[0] = cpu_readmem32ledw_dword(adr);
	iac[1] = cpu_readmem32ledw_dword(adr+4);
	iac[2] = cpu_readmem32ledw_dword(adr+8);
	iac[3] = cpu_readmem32ledw_dword(adr+12);

	switch(iac[0]>>24) {
	case 0x40:  /* generate irq */
		logerror("I960: %x: IAC %08x %08x %08x %08x (generate IRQ)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		break;
	case 0x41:  /* test for pending interrupts */
		logerror("I960: %x: IAC %08x %08x %08x %08x (test for pending interrupts)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		check_pending_irqs();
		break;
	case 0x80:  /* store SAT & PRCB in memory */
		cpu_writemem32ledw_dword(iac[1], m_SAT);
		cpu_writemem32ledw_dword(iac[1]+4, m_PRCB);
		break;
	case 0x89:  /* invalidate internal instruction cache */
		logerror("I960: %x: IAC %08x %08x %08x %08x (invalidate internal instruction cache)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		/* we do not emulate the instruction cache, so this is safe to ignore */
		break;
	case 0x8f:  /* enable/disable breakpoints */
		logerror("I960: %x: IAC %08x %08x %08x %08x (enable/disable breakpoints)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		/* processor breakpoints are not emulated, safe to ignore */
		break;
	case 0x91:  /* stop processor */
		logerror("I960: %x: IAC %08x %08x %08x %08x (stop processor)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		break;
	case 0x92:  /* continue initialization */
		logerror("I960: %x: IAC %08x %08x %08x %08x (continue initialization)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		break;
	case 0x93: /* reinit */
		m_SAT  = iac[1];
		m_PRCB = iac[2];
		m_IP   = iac[3];
		break;
	default:
		i960_fatal("I960: %x: IAC %08x %08x %08x %08x\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
	}
}

static UINT32 get_ea(UINT32 opcode)
{
	int abase = (opcode >> 14) & 0x1f;
	if(!(opcode & 0x00001000)) { /* MEMA */
		UINT32 offset = opcode & 0x1fff;
		if(!(opcode & 0x2000))
			return offset;
		else
			return m_r[abase]+offset;
	} else {                     /* MEMB */
		int index = opcode & 0x1f;
		int scale = (opcode >> 7) & 0x7;
		int mode  = (opcode >> 10) & 0xf;
		UINT32 ret;

		switch(mode) {
		case 0x4:
			return m_r[abase];

		case 0x5:   /* address of this instruction + the offset dword + 8 */
			/* which in reality is "address of next instruction + the offset dword" */
			ret = i960_readop(m_IP);
			m_IP += 4;
			ret += m_IP;
			return ret;

		case 0x7:
			return m_r[abase] + (m_r[index] << scale);

		case 0xc:
			ret = i960_readop(m_IP);
			m_IP += 4;
			return ret;

		case 0xd:
			ret = i960_readop(m_IP) + m_r[abase];
			m_IP += 4;
			return ret;

		case 0xe:
			ret = i960_readop(m_IP) + (m_r[index] << scale);
			m_IP += 4;
			return ret;

		case 0xf:
			ret = i960_readop(m_IP) + m_r[abase] + (m_r[index] << scale);
			m_IP += 4;
			return ret;

		default:
			i960_fatal("I960: %x: unhandled MEMB mode %x\n", m_PIP, mode);
			return 0;
		}
	}
}

/* i960 Extended-real register image: words 0/1 hold the 64-bit fraction with */
/* an explicit integer bit in bit 63, word 2 holds the sign in bit 15 and the */
/* 15-bit biased exponent (bias 16383) in bits 14:0, with the upper 16 bits zero. */
/*
	C99 math where the compiler has it (identical results), C89 fallbacks
	otherwise (MSVC before 2013, strict C89 builds).
*/
#if ( defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 199901L ) || ( defined( _MSC_VER ) && _MSC_VER >= 1800 )
#define i960_isnan( x )		isnan( x )
#define i960_isinf( x )		isinf( x )
#define i960_round( x )		round( x )
#define i960_trunc( x )		trunc( x )
#define i960_log2( x )		log2( x )
#define i960_logb( x )		logb( x )
#else
static int i960_isnan( double x ) { return x != x; }
static int i960_isinf( double x ) { return x == x && ( x - x ) != 0.0; }
/* half away from zero, as round() */
static double i960_round( double x ) { double a = fabs( x ), f = floor( a ); if( a - f >= 0.5 ) f += 1.0; return x < 0.0 ? -f : f; }
static double i960_trunc( double x ) { return x < 0.0 ? ceil( x ) : floor( x ); }
static double i960_log2( double x ) { return log( x ) / log( 2.0 ); }
/* the binary exponent, as logb(): -inf for 0, +inf for infinities, NaN */
/* unchanged; frexp() gives 0.5 <= m < 1, also for denormals */
static double i960_logb( double x )
{
	int e;
	if( i960_isnan( x ) ) return x;
	if( i960_isinf( x ) ) return fabs( x );
	if( x == 0.0 ) return -HUGE_VAL;
	frexp( x, &e );
	return (double)( e - 1 );
}
#endif

/* 64-bit constants without the LL suffix (not in C89 / MSVC 2003) */
#define I960_U64( hi, lo )	( ( (UINT64)( hi ) << 32 ) | (UINT32)( lo ) )

static void double_to_extended(double val, UINT32 *words)
{
	const UINT64 bits = d2u(val);
	const UINT32 sign = (UINT32)(bits >> 63) << 15;
	UINT64 frac;
	UINT32 e;

	if(i960_isnan(val) || i960_isinf(val))
	{
		frac = ((UINT64)1 << 63) | ((bits & I960_U64(0x000fffff, 0xffffffff)) << 11);
		e = 0x7fff;
	}
	else if(val == 0.0)
	{
		frac = 0;
		e = 0;
	}
	else
	{
		int exp2;
		const double m = frexp(fabs(val), &exp2);   /* 0.5 <= m < 1, exact for denormals too */
		frac = (UINT64)(ldexp(m, 64));              /* integer bit lands in bit 63 */
		e = exp2 - 1 + 16383;
	}

	words[0] = (UINT32)(frac);
	words[1] = (UINT32)(frac >> 32);
	words[2] = sign | e;
}

static double extended_to_double(const UINT32 *words)
{
	const UINT64 frac = words[0] | ((UINT64)(words[1]) << 32);
	const UINT32 e = words[2] & 0x7fff;
	double val;

	if(e == 0x7fff)
	{
		if(!(frac << 1))    /* integer bit only: infinity */
		{
			val = HUGE_VAL;
		}
		else                /* NaN: keep whatever payload fits, always quiet */
		{
			val = u2d(I960_U64(0x7ff80000, 0) | ((frac & I960_U64(0x7fffffff, 0xffffffff)) >> 11));
		}
	}
	else
	{
		/* value = fraction * 2^(exponent - bias - 63); e == 0 is a denormal (exponent 1) */
		val = ldexp((double)(frac), (int)( e ? e : 1 ) - 16383 - 63);
	}

	return BIT(words[2], 15) ? -val : val;
}

static UINT32 get_1_ri(UINT32 opcode)
{
	if(!(opcode & 0x00000800))
		return m_r[opcode & 0x1f];
	else
		return opcode & 0x1f;
}

static UINT32 get_2_ri(UINT32 opcode)
{
	if(!(opcode & 0x00001000))
		return m_r[(opcode>>14) & 0x1f];
	else
		return (opcode>>14) & 0x1f;
}

static UINT64 get_2_ri64(UINT32 opcode)
{
	if(!(opcode & 0x00001000))
		return m_r[(opcode>>14) & 0x1f] | ((UINT64)m_r[((opcode>>14) & 0x1f)+1]<<32);
	else
		return (opcode>>14) & 0x1f;
}

static void set_ri(UINT32 opcode, UINT32 val)
{
	if(!(opcode & 0x00002000))
		m_r[(opcode>>19) & 0x1f] = val;
	else {
		i960_fatal("I960: %x: set_ri on literal?\n", m_PIP);
	}
}

static void set_ri2(UINT32 opcode, UINT32 val, UINT32 val2)
{
	if(!(opcode & 0x00002000))
	{
		m_r[(opcode>>19) & 0x1f] = val;
		m_r[((opcode>>19) & 0x1f)+1] = val2;
	}
	else {
		i960_fatal("I960: %x: set_ri2 on literal?\n", m_PIP);
	}
}

static void set_ri64(UINT32 opcode, UINT64 val)
{
	if(!(opcode & 0x00002000)) {
		m_r[(opcode>>19) & 0x1f] = val;
		m_r[((opcode>>19) & 0x1f)+1] = val >> 32;
	} else
		i960_fatal("I960: %x: set_ri64 on literal?\n", m_PIP);
}

static double get_1_rif(UINT32 opcode)
{
	if(!(opcode & 0x00000800))
		return u2f(m_r[opcode & 0x1f]);
	else {
		int idx = opcode & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

static double get_2_rif(UINT32 opcode)
{
	if(!(opcode & 0x00001000))
		return u2f(m_r[(opcode>>14) & 0x1f]);
	else {
		int idx = (opcode>>14) & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

static void set_rif(UINT32 opcode, double val)
{
	if(!(opcode & 0x00002000))
		m_r[(opcode>>19) & 0x1f] = f2u(val);
	else if(!(opcode & 0x00e00000))
		m_fp[(opcode>>19) & 3] = val;
	else
		i960_fatal("I960: %x: set_rif on literal?\n", m_PIP);
}

static double get_1_rifl(UINT32 opcode)
{
	if(!(opcode & 0x00000800)) {
		UINT64 v = m_r[opcode & 0x1e];
		v |= ((UINT64)(m_r[(opcode & 0x1e)+1]))<<32;
		return u2d(v);
	} else {
		int idx = opcode & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

static double get_2_rifl(UINT32 opcode)
{
	if(!(opcode & 0x00001000)) {
		UINT64 v = m_r[(opcode >> 14) & 0x1e];
		v |= ((UINT64)(m_r[((opcode>>14) & 0x1e)+1]))<<32;
		return u2d(v);
	} else {
		int idx = (opcode>>14) & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

static void set_rifl(UINT32 opcode, double val)
{
	if(!(opcode & 0x00002000)) {
		UINT64 v = d2u(val);
		m_r[(opcode>>19) & 0x1e] = v;
		m_r[((opcode>>19) & 0x1e)+1] = v>>32;
	} else if(!(opcode & 0x00e00000))
		m_fp[(opcode>>19) & 3] = val;
	else
		i960_fatal("I960: %x: set_rifl on literal?\n", m_PIP);
}

static UINT32 get_1_ci(UINT32 opcode)
{
	if(!(opcode & 0x00002000))
		return m_r[(opcode >> 19) & 0x1f];
	else
		return (opcode >> 19) & 0x1f;
}

static UINT32 get_2_ci(UINT32 opcode)
{
	return m_r[(opcode >> 14) & 0x1f];
}

static UINT32 get_disp(UINT32 opcode)
{
	return SEXT(opcode, 24) - 4;
}

static UINT32 get_disp_s(UINT32 opcode)
{
	return SEXT(opcode, 13) - 4;
}

static void cmp_s(INT32 v1, INT32 v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

static void cmp_u(UINT32 v1, UINT32 v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

static void concmp_s(INT32 v1, INT32 v2)
{
	m_AC &= ~7;
	if(v1 <= v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

static void concmp_u(UINT32 v1, UINT32 v2)
{
	m_AC &= ~7;
	if(v1 <= v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

static void cmp_d(double v1, double v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else if(v1 > v2)
		m_AC |= 1;
}

static void bxx(UINT32 opcode, int mask)
{
	if(m_AC & mask) {
		m_IP += get_disp(opcode);
		m_IP &= ~3;
	}
}

static void fxx(UINT32 opcode, int mask)
{
	if(m_AC & mask) {
		i960_fatal("Taking the fault on a FAULT insn not yet supported\n");
	}
}

static void bxx_s(UINT32 opcode, int mask)
{
	if(m_AC & mask) {
		m_IP += get_disp_s(opcode);
		m_IP &= ~3;
	}
}

static void test(UINT32 opcode, int mask)
{
	if(m_AC & mask)
		m_r[(opcode>>19) & 0x1f] = 1;
	else
		m_r[(opcode>>19) & 0x1f] = 0;
}

static double round_to_int(double val)
{
	/* apply rounding mode */
	switch ((m_AC >> 30) & 3)
	{
	case 0: return i960_round(val);
	case 1: return floor(val);
	case 2: return ceil(val);
	default: return i960_trunc(val);
	}
}


/* interrupt dispatch */
static void take_interrupt(int vector, int lvl)
{
	int int_tab =  cpu_readmem32ledw_dword(m_PRCB+20);    /* interrupt table */
	int int_SP  =  cpu_readmem32ledw_dword(m_PRCB+24);    /* interrupt stack */
	int SP;
	UINT32 IRQV;

	IRQV = cpu_readmem32ledw_dword(int_tab + 36 + (vector-8)*4);

	/* start the process */
	if(!(m_PC & 0x2000))    /* if this is a nested interrupt, don't re-get int_SP */
	{
		SP = int_SP;
	}
	else
	{
		SP = m_r[I960_SP];
	}

	SP = (SP + 63) & ~63;
	SP += 64;   /* add padding to prevent buffer underflow when saving processor state */

	do_call(IRQV, 7, SP);

	/* save the processor state */
	cpu_writemem32ledw_dword(m_r[I960_FP]-16, m_PC);
	cpu_writemem32ledw_dword(m_r[I960_FP]-12, m_AC);
	/* store the vector */
	cpu_writemem32ledw_dword(m_r[I960_FP]-8, vector-8);

	m_PC &= ~0x001f0401;    /* clear priority (bits 16-20), trace-fault pending (bit 10) and trace enable (bit 0) */
	m_PC |= (lvl<<16);      /* set CPU level to current IRQ level */
	m_PC |= 0x2002;         /* set supervisor mode & interrupt flag */
}

static void check_immediate_irqs(void)
{
	int cpu_pri = (m_PC >> 16) & 0x1f;

	if ((m_immediate_irq) && ((cpu_pri < m_immediate_pri) || (m_immediate_pri == 31)))
	{
		take_interrupt(m_immediate_vector, m_immediate_pri);
		m_immediate_irq = 0;
	}
}

static void check_pending_irqs(void)
{
	int int_tab = cpu_readmem32ledw_dword(m_PRCB + 20);    /* interrupt table */
	int cpu_pri = (m_PC >> 16) & 0x1f;
	int pending_pri = cpu_readmem32ledw_dword(int_tab);    /* read pending priorities */
	int take = -1;
	static const UINT32 lvlmask[4] = { 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };
	int lvl;

	for (lvl = 31; lvl >= 0; lvl--) {
		if ((pending_pri & (1 << lvl)) && ((cpu_pri < lvl) || (lvl == 31))) {
			int word, wordl, wordh, vword, irq;

			/* figure out which word contains this level's priorities */
			word = ((lvl / 4) * 4) + 4; /* (lvl/4) = word address, *4 for byte address, +4 to skip pending priorities */
			wordl = (lvl % 4) * 8;
			wordh = (wordl + 8) - 1;

			vword = cpu_readmem32ledw_dword(int_tab + word);

			/* take the first vector we find for this level */
			for (irq = wordh; irq >= wordl; irq--) {
				if (vword & (1 << irq)) {
					/* clear pending bit */
					vword &= ~(1 << irq);
					cpu_writemem32ledw_dword(int_tab + word, vword);
					take = irq;
					break;
				}
			}

			/* if no vectors were found at our level, it's an error */
			if (take == -1) {
				logerror("i960: ERROR! no vector found for pending level %d\n", lvl);

				/* try to recover... */
				pending_pri &= ~(1 << lvl);
				cpu_writemem32ledw_dword(int_tab, pending_pri);
				return;
			}

			/* if no vectors are waiting for this level, clear the level bit */
			if (!(vword & lvlmask[lvl % 4])) {
				pending_pri &= ~(1 << lvl);
				cpu_writemem32ledw_dword(int_tab, pending_pri);
			}

			take += ((lvl / 4) * 32);

			take_interrupt(take, lvl);
			return;
		}
	}
}

static void do_call(UINT32 adr, int type, UINT32 stack)
{
	int i;
	UINT32 FP;

	/* call and callx take 9 cycles base */
	m_icount -= 9;

	/* set the new RIP */
	m_r[I960_RIP] = m_IP;
/*  osd_printf_debug("CALL (type %d): FP %x, %x => %x, stack %x, rcache_pos %d\n", type, m_r[I960_FP], m_r[I960_RIP], adr, stack, m_rcache_pos); */

	/* are we out of cache entries? */
	if (m_rcache_pos >= I960_RCACHE_SIZE) {
		/* flush the current register set to the current frame */
		FP = m_r[I960_FP] & ~0x3f;
		for (i = 0; i < 16; i++) {
			cpu_writemem32ledw_dword(FP + (i*4), m_r[i]);
		}
	}
	else    /* a cache entry is available, use it */
	{
		memcpy(&m_rcache[m_rcache_pos][0], m_r, 0x10 * sizeof(UINT32));
		m_rcache_frame_addr[m_rcache_pos] = m_r[I960_FP] & ~0x3f;
	}
	m_rcache_pos++;

	m_IP = adr;
	m_r[I960_PFP] = m_r[I960_FP] & ~7;
	m_r[I960_PFP] |= type;

	if(type == 7) { /* interrupts need special handling */
		/* set the stack to the passed-in value to properly handle nested interrupts */
		/* (can't set it externally or the original program's SP will be lost) */
		m_r[I960_SP] = stack;
	}

	m_r[I960_FP]  = (m_r[I960_SP] + 63) & ~63;
	m_r[I960_SP]  = m_r[I960_FP] + 64;
}

static void do_ret_0(void)
{
/*  int type = m_r[I960_PFP] & 7; */

	m_r[I960_FP] = m_r[I960_PFP] & ~0x3f;

	m_rcache_pos--;

	/* normal situation: if we're still above rcache size, we're not in cache. */
	/* abnormal situation (after the app does a FLUSHREG): rcache_pos will be 0 */
	/* coming in, but we must still treat it as a not-in-cache situation. */
	if ((m_rcache_pos >= I960_RCACHE_SIZE) || (m_rcache_pos < 0))
	{
		int i;
		for(i=0; i<0x10; i++)
			m_r[i] = cpu_readmem32ledw_dword(m_r[I960_FP]+4*i);

		if (m_rcache_pos < 0)
		{
			m_rcache_pos = 0;
		}
	}
	else
	{
		memcpy(m_r, m_rcache[m_rcache_pos], 0x10*sizeof(UINT32));
	}

/*  osd_printf_debug("RET (type %d): FP %x, %x => %x, rcache_pos %d\n", type, m_r[I960_FP], m_IP, m_r[I960_RIP], m_rcache_pos); */
	m_IP = m_r[I960_RIP];
}

static void do_ret(void)
{
	UINT32 x, y;
	m_icount -= 7;
	switch(m_r[I960_PFP] & 7) {
	case 0:
		do_ret_0();
		break;

	case 7:
		x = cpu_readmem32ledw_dword(m_r[I960_FP]-16);
		y = cpu_readmem32ledw_dword(m_r[I960_FP]-12);
		do_ret_0();
		m_AC = y;
		/* #### test supervisor */
		m_PC = x;

		/* check for another IRQ now that we're back */
		check_pending_irqs();
		break;

	default:
		i960_fatal("I960: %x: Unsupported return mode %d\n", m_PIP, m_r[I960_PFP] & 7);
	}
}

/* if last opcode was a multi dword burst read opcode save the data here */
/* i.e. Model 2 FIFO reads with ldl, ldt, ldq */
static void burst_stall_save(UINT32 t1, UINT32 t2, int index, int size, int iswriteop)
{
	m_stall_state.t1 = t1;
	m_stall_state.t2 = t2;
	m_stall_state.index = index;
	m_stall_state.size = size;
	m_stall_state.iswriteop = iswriteop;
	m_stall_state.burst_mode = 1;
}

/* resume from a burst stall opcode */
static void execute_burst_stall_op(UINT32 opcode)
{
	int i;
	/* in case opcode uses an operand call effective address function to fix IP register */
	(void)get_ea(opcode);

	/* check if our data is ready */
	for(i=m_stall_state.index ; i<m_stall_state.size ;i++)
	{
		/* count down 1 icount for every op */
		m_icount--;
		if(m_stall_state.iswriteop == 1)
			i960_write_dword_unaligned(m_stall_state.t1, m_r[m_stall_state.t2+i]);
		else
			m_r[m_stall_state.t2+i] = i960_read_dword_unaligned(m_stall_state.t1);

		/* if the host returned stall just save the index and try again on a later moment */
		if(m_stalled == 1)
		{
			m_stall_state.index = i;
			return;
		}
	}

	/* clear stall burst mode */
	m_stall_state.burst_mode = 0;
	/* now that we are done we might as well check if there's an irq too */
	check_immediate_irqs();
}

static void execute_op(UINT32 opcode)
{
	UINT32 t1, t2;
	double t1f, t2f;

	switch(opcode >> 24) {
		case 0x08: /* b */
			m_icount--;
			m_IP += get_disp(opcode);
			break;

		case 0x09: /* call */
			do_call(m_IP+get_disp(opcode), 0, m_r[I960_SP]);
			break;

		case 0x0a: /* ret */
			do_ret();
			break;

		case 0x0b: /* bal */
			m_icount -= 5;
			m_r[0x1e] = m_IP;
			m_IP += get_disp(opcode);
			break;

		case 0x10: /* bno */
			m_icount--;
			if(!(m_AC & 7)) {
				m_IP += get_disp(opcode);
			}
			break;

		case 0x11: /* bg */
			m_icount--;
			bxx(opcode, 1);
			break;

		case 0x12: /* be */
			m_icount--;
			bxx(opcode, 2);
			break;

		case 0x13: /* bge */
			m_icount--;
			bxx(opcode, 3);
			break;

		case 0x14: /* bl */
			m_icount--;
			bxx(opcode, 4);
			break;

		case 0x15: /* bne */
			m_icount--;
			bxx(opcode, 5);
			break;

		case 0x16: /* ble */
			m_icount--;
			bxx(opcode, 6);
			break;

		case 0x17: /* bo */
			m_icount--;
			bxx(opcode, 7);
			break;

		case 0x18: /* faultno */
			m_icount--;
			if(!(m_AC & 7)) {
				m_IP += get_disp(opcode);
			}
			break;

		case 0x19: /* faultg */
			m_icount--;
			fxx(opcode, 1);
			break;

		case 0x1a: /* faulte */
			m_icount--;
			fxx(opcode, 2);
			break;

		case 0x1b: /* faultge */
			m_icount--;
			fxx(opcode, 3);
			break;

		case 0x1c: /* faultl */
			m_icount--;
			fxx(opcode, 4);
			break;

		case 0x1d: /* faultne */
			m_icount--;
			fxx(opcode, 5);
			break;

		case 0x1e: /* faultle */
			m_icount--;
			fxx(opcode, 6);
			break;

		case 0x1f: /* faulto */
			m_icount--;
			fxx(opcode, 7);
			break;

		case 0x20: /* testno */
			m_icount--;
			if(!(m_AC & 7))
				m_r[(opcode>>19) & 0x1f] = 1;
			else
				m_r[(opcode>>19) & 0x1f] = 0;
			break;

		case 0x21: /* testg */
			m_icount--;
			test(opcode, 1);
			break;

		case 0x22: /* teste */
			m_icount--;
			test(opcode, 2);
			break;

		case 0x23: /* testge */
			m_icount--;
			test(opcode, 3);
			break;

		case 0x24: /* testl */
			m_icount--;
			test(opcode, 4);
			break;

		case 0x25: /* testne */
			m_icount--;
			test(opcode, 5);
			break;

		case 0x26: /* testle */
			m_icount--;
			test(opcode, 6);
			break;

		case 0x27: /* testo */
			m_icount--;
			test(opcode, 7);
			break;

		case 0x30: /* bbc */
			m_icount -= 4;
			t1 = get_1_ci(opcode) & 0x1f;
			t2 = get_2_ci(opcode);
			if(!(t2 & (1<<t1))) {
				m_AC = (m_AC & ~7) | 2;
				m_IP += get_disp_s(opcode);
			} else
				m_AC &= ~7;
			break;

		case 0x31: /* cmp0bg */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 1);
			break;

		case 0x32: /* cmpobe */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 2);
			break;

		case 0x33: /* cmpobge */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 3);
			break;

		case 0x34: /* cmpobl */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 4);
			break;

		case 0x35: /* cmpobne */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 5);
			break;

		case 0x36: /* cmpoble */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 6);
			break;

		case 0x37: /* bbs */
			m_icount -= 4;
			t1 = get_1_ci(opcode) & 0x1f;
			t2 = get_2_ci(opcode);
			if(t2 & (1<<t1)) {
				m_AC = (m_AC & ~7) | 2;
				m_IP += get_disp_s(opcode);
			} else
				m_AC &= ~7;
			break;

		case 0x39: /* cmpibg */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 1);
			break;

		case 0x3a: /* cmpibe */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 2);
			break;

		case 0x3b: /* cmpibge */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 3);
			break;

		case 0x3c: /* cmpibl */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 4);
			break;

		case 0x3d: /* cmpibne */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 5);
			break;

		case 0x3e: /* cmpible */
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 6);
			break;

		case 0x58:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* notbit */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 ^ (1<<(t1 & 31)));
				break;

			case 0x1: /* and */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & t1);
				break;

			case 0x2: /* andnot */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & ~t1);
				break;

			case 0x3: /* setbit */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | (1<<(t1 & 31)));
				break;

			case 0x4: /* notand */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (~t2) & t1);
				break;

			case 0x6: /* xor */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 ^ t1);
				break;

			case 0x7: /* or */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | t1);
				break;

			case 0x8: /* nor */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((~t2) & (~t1)));
				break;

			case 0x9: /* xnor */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ~(t2 ^ t1));
				break;

			case 0xa: /* not */
				m_icount--;
				t1 = get_1_ri(opcode);
				set_ri(opcode, ~t1);
				break;

			case 0xb: /* ornot */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | ~t1);
				break;

			case 0xc: /* clrbit */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & ~(1<<(t1 & 31)));
				break;

			case 0xd: /* notor */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (~t2) | t1);
				break;

			case 0xe: /* nand */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ~t2 | ~t1);
				break;

			case 0xf: /* alterbit */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(m_AC & 2)
					set_ri(opcode, t2 | (1<<(t1 & 31)));
				else
					set_ri(opcode, t2 & ~(1<<(t1 & 31)));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 58.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x59:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* addo */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2+t1);
				break;

			case 0x1: /* addi */
				/* #### overflow */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2+t1);
				break;

			case 0x2: /* subo */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2-t1);
				break;

			case 0x3: /* subi */
				/* #### overflow */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2-t1);
				break;

			case 0x8: /* shro */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t1 >= 32 ? 0 : t2>>t1);
				break;

			case 0xa: /* shrdi */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 >= 32)
					set_ri(opcode, 0);
				else if(((INT32)t2) < 0) {
					if(t2 & ((1<<t1)-1))
						set_ri(opcode, (((INT32)t2)>>t1)+1);
					else
						set_ri(opcode, ((INT32)t2)>>t1);
				} else
					set_ri(opcode, t2>>t1);
				break;

			case 0xb: /* shri */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 >= 32)
					set_ri(opcode, (INT32)t2 < 0 ? -1 : 0);
				else
					set_ri(opcode, ((INT32)t2)>>t1);
				break;

			case 0xc: /* shlo */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t1 >= 32 ? 0 : t2<<t1);
				break;

			case 0xd: /* rotate */
				m_icount--;
				t1 = get_1_ri(opcode) & 0x1f;
				t2 = get_2_ri(opcode);
				set_ri(opcode, rotl32(t2, t1));
				break;

			case 0xe: /* shli */
				/* missing overflow */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				/* TODO: on later models, sign is always preserved even upon overflow */
				set_ri(opcode, t1 >= 32 ? 0 : t2<<t1);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 59.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5a:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* cmpo */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				break;

			case 0x1: /* cmpi */
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				break;

			case 0x2: /* concmpo */
				m_icount--;
				if(!(m_AC & 0x4)) {
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					concmp_u(t1, t2);
				}
				break;

			case 0x3: /* concmpi */
				m_icount--;
				if(!(m_AC & 0x4)) {
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					concmp_s(t1, t2);
				}
				break;

			case 0x4: /* cmpinco */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				set_ri(opcode, t2+1);
				break;

			case 0x5: /* cmpinci */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				set_ri(opcode, t2+1);
				break;

			case 0x6: /* cmpdeco */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				set_ri(opcode, t2-1);
				break;

			case 0x7: /* cmpdeci */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				set_ri(opcode, t2-1);
				break;

			case 0xc: /* scanbyte */
				m_icount -= 2;
				m_AC &= ~7;     /* clear CC */
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if ((t1 & 0xff000000) == (t2 & 0xff000000) ||
					(t1 & 0x00ff0000) == (t2 & 0x00ff0000) ||
					(t1 & 0x0000ff00) == (t2 & 0x0000ff00) ||
					(t1 & 0x000000ff) == (t2 & 0x000000ff))
				{
					m_AC |= 2;
				}
				break;

			case 0xe: /* chkbit */
				m_icount -= 2;
				t1 = get_1_ri(opcode) & 0x1f;
				t2 = get_2_ri(opcode);
				if(t2 & (1<<t1))
					m_AC = (m_AC & ~7) | 2;
				else
					m_AC &= ~7;
				break;

			default:
				i960_fatal("I960: %x: Unhandled 5a.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5b:
			switch((opcode >> 7) & 0xf) {
			case 0x0:   /* addc */
				{
					UINT64 res;

					m_icount -= 2;
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					res = t2+(t1+((m_AC>>1)&1));
					set_ri(opcode, res&0xffffffff);

					m_AC &= ~0x3;   /* clear C and V */
					/* set carry */
					m_AC |= ((res) & (((UINT64)1) << 32)) ? 0x2 : 0;
					/* set overflow */
					m_AC |= (((res) ^ (t1)) & ((res) ^ (t2)) & 0x80000000) ? 1: 0;
				}
				break;

			case 0x2:   /* subc */
				{
					UINT64 res;

					m_icount -= 2;
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					/* dst = src2 - src1 - 1 + C, or src2 + ~src1 + C */
					res = (UINT64)t2 + (UINT64)(UINT32)~t1 + ((m_AC>>1)&1);
					set_ri(opcode, res&0xffffffff);

					m_AC &= ~0x7;   /* cc = 0CV */
					/* set carry */
					m_AC |= ((res) & (((UINT64)1) << 32)) ? 0x2 : 0;
					/* set overflow */
					m_AC |= (((t2) ^ (t1)) & ((t2) ^ (res)) & 0x80000000) ? 1 : 0;
				}
				break;

			default:
				i960_fatal("I960: %x: Unhandled 5b.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5c:
			switch((opcode >> 7) & 0xf) {
			case 0xc: /* mov */
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				set_ri(opcode, t1);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 5c.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5d:
			switch((opcode >> 7) & 0xf) {
			case 0xc: /* movl */
				m_icount -= 2;
				t2 = (opcode>>19) & 0x1e;
				if(opcode & 0x00000800) { /* litteral */
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 2*sizeof(UINT32));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 5d.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5e:
			switch((opcode >> 7) & 0xf) {
			case 0xc: /* movt */
				m_icount -= 3;
				t2 = (opcode>>19) & 0x1c;
				if(opcode & 0x00000800) { /* litteral */
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = m_r[t2+2]= t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 3*sizeof(UINT32));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 5e.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5f:
			switch((opcode >> 7) & 0xf) {
			case 0xc: /* movq */
				m_icount -= 4;
				t2 = (opcode>>19) & 0x1c;
				if(opcode & 0x00000800) { /* litteral */
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = m_r[t2+2] = m_r[t2+3] = t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 4*sizeof(UINT32));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 5f.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x60:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* synmov */
				m_icount -= 6;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				/* interrupt control register */
				if(t1 == 0xff000004)
					m_ICR = cpu_readmem32ledw_dword(t2);
				else
					cpu_writemem32ledw_dword(t1,    cpu_readmem32ledw_dword(t2));
				m_AC = (m_AC & ~7) | 2;
				break;

			case 0x2: /* synmovq */
				m_icount -= 12;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 == 0xff000010)
					send_iac(t2);
				else {
					cpu_writemem32ledw_dword(t1,    cpu_readmem32ledw_dword(t2));
					cpu_writemem32ledw_dword(t1+4,  cpu_readmem32ledw_dword(t2+4));
					cpu_writemem32ledw_dword(t1+8,  cpu_readmem32ledw_dword(t2+8));
					cpu_writemem32ledw_dword(t1+12, cpu_readmem32ledw_dword(t2+12));
				}
				m_AC = (m_AC & ~7) | 2;
				break;

			default:
				i960_fatal("I960: %x: Unhandled 60.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x64:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* spanbit */
				{
					UINT32 res = 0xffffffff;
					int i;

					m_icount -= 10;

					t1 = get_1_ri(opcode);
					m_AC &= ~7;

					for (i = 31; i >= 0; i--)
					{
						if (!(t1 & (1<<i)))
						{
							m_AC |= 2;
							res = i;
							break;
						}
					}

					set_ri(opcode, res);
				}
				break;

			case 0x1: /* scanbit */
				{
					UINT32 res = 0xffffffff;
					int i;

					m_icount -= 10;

					t1 = get_1_ri(opcode);
					m_AC &= ~7;

					for (i = 31; i >= 0; i--)
					{
						if (t1 & (1<<i))
						{
							m_AC |= 2;
							res = i;
							break;
						}
					}

					set_ri(opcode, res);
				}
				break;

			case 0x4: /* dmovt */
				/*
				    The dmovt instruction moves a 32-bit word from one register to another
				    and tests the least-significant byte of the operand to determine if it is a
				    valid ASCII-coded decimal digit (001100002 through 001110012,
				    corresponding to the decimal digits 0 through 9). For valid digits, the
				    condition code (CC) is set to 000; otherwise the condition code is set to
				    010.
				*/
				m_icount -= 7;
				t1 = get_1_ri(opcode);
				set_ri(opcode, t1);
				m_AC &= 0xfff8;
				if ((t1 & 0xff) < 0x30 || (t1 & 0xff) > 0x39)
					m_AC |= 2;
				break;

			case 0x5: /* modac */
				m_icount -= 10;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, m_AC);
				m_AC = (m_AC & ~t1) | (t2 & t1);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 64.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x65:
			switch((opcode >> 7) & 0xf) {
			case 0x5: /* modpc */
				m_icount -= 10;
				t1 = m_PC;
				t2 = get_2_ri(opcode);
				m_PC = (m_PC & ~t2) | (m_r[(opcode>>19) & 0x1f] & t2);
				set_ri(opcode, t1);
				if ((t1 >> 16 & 0x1f) > (m_PC >> 16 & 0x1f))
					check_pending_irqs();
				break;

			default:
				i960_fatal("I960: %x: Unhandled 65.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x66:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* calls */
				t1 = get_1_ri(opcode);
				t2 = cpu_readmem32ledw_dword(m_SAT + 152);    /* get pointer to system procedure table */
				t2 = cpu_readmem32ledw_dword(t2 + 48 + (t1 * 4));
				if ((t2 & 3) != 0)
				{
					i960_fatal("I960: system calls that jump into supervisor mode aren't yet supported\n");
				}
				do_call(t2, 0, m_r[I960_SP]);
				break;

			case 0xd: /* flushreg */
				if (m_rcache_pos > 4)
				{
					m_rcache_pos = 4;
				}
				for(t1=0; t1 < m_rcache_pos; t1++)
				{
					int i;

					for (i = 0; i < 0x10; i++)
					{
						cpu_writemem32ledw_dword(m_rcache_frame_addr[t1] + (i * sizeof(UINT32)), m_rcache[t1][i]);
					}
				}
				m_rcache_pos = 0;
				break;

			default:
				i960_fatal("I960: %x: Unhandled 66.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x67:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* emul */
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);

				set_ri64(opcode, ((UINT64)(t1) * (UINT64)(t2)));
				break;

			case 0x1: /* ediv */
				m_icount -= 37;
				{
					UINT64 src1, src2;

					src1 = get_1_ri(opcode);
					src2 = get_2_ri64(opcode);

					set_ri2(opcode, src2 % src1, src2 / src1);
				}
				break;

			case 0x4: /* cvtir */
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				set_rif(opcode, (double)(INT32)t1);
				break;

			case 0x5: /* cvtilr */
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				set_rifl(opcode, (double)(INT32)t1);
				break;

			case 0x6: /* scalerl */
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f * pow(2.0, (double)(INT32)t1));
				break;

			case 0x7: /* scaler */
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				t2f = get_2_rif(opcode);
			set_rif(opcode, t2f * pow(2.0, (double)(INT32)t1));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 67.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x68:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* atanr */
				m_icount -= 267;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, atan2(t2f, t1f));
				break;

			case 0x1: /* logepr */
				m_icount -= 400;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*i960_log2(t1f+1.0));
				break;

			case 0x2: /* logr */
				m_icount -= 438;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*i960_log2(t1f));
				break;

			case 0x3: /* remr */
				m_icount -= 67; /* (67 to 75878 depending on opcodes!!!) */
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, fmod(t2f, t1f));
				break;

			case 0x5: /* cmpr */
				m_icount -= 10;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				cmp_d(t1f, t2f);
				break;

			case 0x8: /* sqrtr */
				m_icount -= 104;
				t1f = get_1_rif(opcode);
				set_rif(opcode, sqrt(t1f));
				break;

			case 0x9: /* expr */
				m_icount -= 334; /* checkme */
				t1f = get_1_rif(opcode);
				set_rif(opcode, pow(2.0, t1f) - 1.0);
				break;

			case 0xa: /* logbnr */
				m_icount -= 37;
				t1f = get_1_rif(opcode);
				set_rif(opcode, i960_logb(t1f));
				break;

			case 0xb: /* roundr */
				m_icount -= 69;
				t1f = get_1_rif(opcode);
				set_rif(opcode, round_to_int(t1f));
				break;

			case 0xc: /* sinr */
				m_icount -= 406;
				t1f = get_1_rif(opcode);
				set_rif(opcode, sin(t1f));
				break;

			case 0xd: /* cosr */
				m_icount -= 406;
				t1f = get_1_rif(opcode);
				set_rif(opcode, cos(t1f));
				break;

			case 0xe: /* tanr */
				m_icount -= 293;
				t1f = get_1_rif(opcode);
				set_rif(opcode, tan(t1f));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 68.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x69:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* atanrl */
				m_icount -= 350;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, atan2(t2f, t1f));
				break;

			case 0x2: /* logrl */
				m_icount -= 438;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f*i960_log2(t1f));
				break;

			case 0x5: /* cmprl */
				m_icount -= 12;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				cmp_d(t1f, t2f);
				break;

			case 0x8: /* sqrtrl */
				m_icount -= 104;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, sqrt(t1f));
				break;

			case 0x9: /* exprl */
				m_icount -= 334;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, pow(2.0, t1f)-1.0);
				break;

			case 0xa: /* logbnrl */
				m_icount -= 37;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, i960_logb(t1f));
				break;

			case 0xb: /* roundrl */
				m_icount -= 70;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, round_to_int(t1f));
				break;

			case 0xc: /* sinrl */
				m_icount -= 441;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, sin(t1f));
				break;

			case 0xd: /* cosrl */
				m_icount -= 441;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, cos(t1f));
				break;

			case 0xe: /* tanrl */
				m_icount -= 323;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, tan(t1f));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 69.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6c:
			switch((opcode >> 7) & 0xf) {
			case 0x0: /* cvtri */
				m_icount -= 33;
				t1f = get_1_rif(opcode);
				set_ri(opcode, (INT32)round_to_int(t1f));
				break;

			case 0x1: /* cvtril */
				m_icount -= 35;
				t1f = get_1_rif(opcode);
				set_ri64(opcode, (INT64)round_to_int(t1f));
				break;

			case 0x2: /* cvtzri */
				m_icount -= 43;
				t1f = get_1_rif(opcode);
				set_ri(opcode, (INT32)t1f);
				break;

			case 0x3: /* cvtzril */
				m_icount -= 44;
				t1f = get_1_rif(opcode);
				set_ri64(opcode, (INT64)t1f);
				break;

			case 0x9: /* movr */
				m_icount -= 5;
				t1f = get_1_rif(opcode);
				set_rif(opcode, t1f);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 6c.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6d:
			switch((opcode >> 7) & 0xf) {
			case 0x9: /* movrl */
				m_icount -= 6;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, t1f);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 6d.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6e:
			switch((opcode >> 7) & 0xf) {
			case 0x1: /* movre (undocumented encoding, used by Dead or Alive to save/restore fp0-fp3) */
			case 0x9: /* movre */
				{
					UINT32 v[3];

					m_icount -= 8;

					/* source: three g/l registers (multiple of 4), a floating-point register or a literal */
					if(!(opcode & 0x00000800))
					{
						const int src = opcode & 0x1c;
						v[0] = m_r[src];
						v[1] = m_r[src+1];
						v[2] = m_r[src+2] & 0xffff;    /* upper 16 bits of the third word are truncated */
					}
					else
					{
						const int idx = opcode & 0x1f;
						if(idx < 4)
							double_to_extended(m_fp[idx], v);
						else
							double_to_extended((idx == 0x16) ? 1.0 : 0.0, v);
					}

					/* destination: three g/l registers (multiple of 4) or a floating-point register */
					if(!(opcode & 0x00002000))
					{
						const int dst = (opcode>>19) & 0x1c;
						m_r[dst] = v[0];
						m_r[dst+1] = v[1];
						m_r[dst+2] = v[2];
					}
					else if(!(opcode & 0x00e00000))
						m_fp[(opcode>>19) & 3] = extended_to_double(v);
					else
						i960_fatal("i960: %x: movre to literal?\n", m_PIP);
				}
				break;
			case 0x2: /* cpysre */
				m_icount -= 8;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);

				if (t2f >= 0.0)
					set_rifl(opcode, fabs(t1f));
				else
					set_rifl(opcode, -fabs(t1f));
				break;
			default:
				i960_fatal("I960: %x: Unhandled 6e.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x70:
			switch((opcode >> 7) & 0xf) {
			case 0x1: /* mulo */
				m_icount -= 18;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2*t1);
				break;

			case 0x8: /* remo */
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2%t1);
				break;

			case 0xb: /* divo */
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if (t1 == 0)    /* HACK! */
					set_ri(opcode, 0);
				else
					set_ri(opcode, t2/t1);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 70.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x74:
			switch((opcode >> 7) & 0xf) {
			case 0x1: /* muli */
				m_icount -= 18;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((INT32)t2)*((INT32)t1));
				break;

			case 0x8: /* remi */
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((INT32)t2)%((INT32)t1));
				break;

			case 0x9:{/* modi */
				INT32 src1, src2, dst;
				m_icount -= 37;
				src1 = (INT32)get_1_ri(opcode);
				src2 = (INT32)get_2_ri(opcode);
				dst = src2 - ((src2/src1)*src1);
				if(((src1 ^ src2) < 0) && (dst != 0))   /* operands of opposite sign (the product would overflow) */
					dst += src1;
				set_ri(opcode, dst);
				break;
			}

			case 0xb: /* divi */
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((INT32)t2)/((INT32)t1));
				break;

			default:
				i960_fatal("I960: %x: Unhandled 74.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x78:
			switch((opcode >> 7) & 0xf) {
			case 0xb: /* divr */
				m_icount -= 35;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f/t1f);
				break;

			case 0xc: /* mulr */
				m_icount -= 18;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*t1f);
				break;

			case 0xd: /* subr */
				m_icount -= 10;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f-t1f);
				break;

			case 0xf: /* addr */
				m_icount -= 10;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f+t1f);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 78.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x79:
			switch((opcode >> 7) & 0xf) {
			case 0xb: /* divrl */
				m_icount -= 77;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f/t1f);
				break;

			case 0xc: /* mulrl */
				m_icount -= 36;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f*t1f);
				break;

			case 0xd: /* subrl */
				m_icount -= 13;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f-t1f);
				break;

			case 0xf: /* addrl */
				m_icount -= 13;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f+t1f);
				break;

			default:
				i960_fatal("I960: %x: Unhandled 79.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x80: { /* ldob */
			UINT8 v;
			m_icount -= 4;
			v = cpu_readmem32ledw(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x82: /* stob */
			m_icount -= 2;
			cpu_writemem32ledw(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x84: /* bx */
			m_icount -= 3;
			m_IP = get_ea(opcode);
			break;

		case 0x85: /* balx */
			m_icount -= 5;
			t1 = get_ea(opcode);
			m_r[(opcode>>19)&0x1f] = m_IP;
			m_IP = t1;
			break;

		case 0x86: /* callx */
			t1 = get_ea(opcode);
			do_call(t1, 0, m_r[I960_SP]);
			break;

		case 0x88: { /* ldos */
			UINT16 v;
			m_icount -= 4;
			v = i960_read_word_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x8a: /* stos */
			m_icount -= 2;
			i960_write_word_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x8c: /* lda */
			m_icount--;
			m_r[(opcode>>19)&0x1f] = get_ea(opcode);
			break;

		case 0x90: { /* ld */
			UINT32 v;
			m_icount -= 4;
			v = i960_read_dword_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x92: /* st */
			m_icount -= 2;
			i960_write_dword_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x98:{/* ldl */
			int i;
			m_icount -= 5;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1e;
			for(i=0; i<2; i++) {
				UINT32 pack_first = i960_read_dword_unaligned(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,2,0);
					return;
				}
				m_r[t2+i] = pack_first;
				if(i960_is_burst(t1))
					t1 += 4;
			}
			break;
		}

		case 0x9a:{/* stl */
			int i;
			m_icount -= 3;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1e;
			for(i=0; i<2; i++) {
				i960_write_dword_unaligned(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,2,1);
					return;
				}
				if(i960_is_burst(t1))
					t1 += 4;
			}
			break;
		}

		case 0xa0:{/* ldt */
			int i;
			m_icount -= 6;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<3; i++) {
				UINT32 pack_first = i960_read_dword_unaligned(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,3,0);
					return;
				}
				m_r[t2+i] = pack_first;
				if(i960_is_burst(t1))
					t1 += 4;
			}
			break;
		}

		case 0xa2:{/* stt */
			int i;
			m_icount -= 4;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<3; i++) {
				i960_write_dword_unaligned(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,3,1);
					return;
				}
				if(i960_is_burst(t1))
					t1 += 4;
			}
			break;
		}

		case 0xb0:{/* ldq */
			int i;
			m_icount -= 7;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<4; i++) {
				UINT32 pack_first = i960_read_dword_unaligned(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,4,0);
					return;
				}
				m_r[t2+i] = pack_first;
				if(i960_is_burst(t1))
					t1 += 4;
			}
			break;
		}

		case 0xb2:{/* stq */
			int i;
			m_icount -= 5;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<4; i++) {
				i960_write_dword_unaligned(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,4,1);
					return;
				}
				if(i960_is_burst(t1))
					t1 += 4;
			}
			break;
		}

		case 0xc0: { /* ldib */
			INT8 v;
			m_icount -= 4;
			v = cpu_readmem32ledw(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0xc2: /* stib */
			m_icount -= 2;
			cpu_writemem32ledw(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0xc8: { /* ldis */
			INT16 v;
			m_icount -= 4;
			v = i960_read_word_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0xca: /* stis */
			m_icount -= 2;
			i960_write_word_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		default:
			i960_fatal("I960: %x: Unhandled %02x\n", m_PIP, opcode >> 24);
	}

}


/***************************************************************************
    CPU interface
***************************************************************************/

int i960_execute( int cycles )
{
	UINT32 opcode;

	i960_icount = cycles;

	/* delay checking irqs if we are in burst stall mode */
	if( m_stall_state.burst_mode == 0 )
		check_immediate_irqs();

	while( m_icount > 0 )
	{
		m_PIP = m_IP;
		CALL_MAME_DEBUG;

		opcode = i960_readop( m_IP );
		m_IP += 4;

		m_stalled = 0;

		if( m_stall_state.burst_mode == 1 )
			execute_burst_stall_op( opcode );
		else
			execute_op( opcode );

		/* stalled on a FIFO: give the other CPUs the rest of the timeslice */
		if( m_stalled )
			m_icount = 0;
	}

	return cycles - i960_icount;
}

void i960_stall( void )
{
	m_stalled = 1;
	m_IP = m_PIP;
}

void i960_set_burst_callback( int (*is_burst)(UINT32 address) )
{
	i960_burst_cb = is_burst;
}

void i960_set_irq_line( int irqline, int state )
{
	int int_tab, cpu_pri, vector = 0, priority;
	UINT32 pend, word, wordofs;

	if( irqline < 0 || irqline > 3 )
		return;
	if( m_irq_line_state[ irqline ] == state )
		return;

	m_irq_line_state[ irqline ] = state;

	int_tab = cpu_readmem32ledw_dword( m_PRCB + 20 );	/* interrupt table */
	cpu_pri = ( m_PC >> 16 ) & 0x1f;

	/* the 4 external IRQ lines in "normal" mode only, as current MAME */
	switch( irqline )
	{
	case I960_IRQ0: vector = m_ICR & 0xff; break;
	case I960_IRQ1: vector = ( m_ICR >> 8 ) & 0xff; break;
	case I960_IRQ2: vector = ( m_ICR >> 16 ) & 0xff; break;
	case I960_IRQ3: vector = ( m_ICR >> 24 ) & 0xff; break;
	}

	if( !vector )
	{
		logerror( "i960: interrupt line %d in IAC mode, unsupported!\n", irqline );
		return;
	}

	priority = vector / 8;

	if( state )
	{
		/* check if we can take this "right now" */
		if( ( ( cpu_pri < priority ) || ( priority == 31 ) ) && ( m_immediate_irq == 0 ) )
		{
			m_immediate_irq = 1;
			m_immediate_vector = vector;
			m_immediate_pri = priority;
		}
		else
		{
			/* store the interrupt in the "pending" table */
			pend = cpu_readmem32ledw_dword( int_tab );
			pend |= ( 1 << priority );
			cpu_writemem32ledw_dword( int_tab, pend );

			/* now bitfield-ize the vector */
			word = ( ( vector / 32 ) * 4 ) + 4;
			wordofs = vector % 32;
			pend = cpu_readmem32ledw_dword( int_tab + word );
			pend |= ( 1 << wordofs );
			cpu_writemem32ledw_dword( int_tab + word, pend );
		}
	}
}

void i960_set_irq_callback( int (*callback)(int irqline) )
{
}

void i960_init( void )
{
	int cpu = cpu_getactivecpu();

	memset( &i960, 0, sizeof( i960 ) );
	state_save_register_UINT32( "i960", cpu, "r", i960.r, 0x20 );
	state_save_register_UINT32( "i960", cpu, "rcache", &i960.rcache[ 0 ][ 0 ], I960_RCACHE_SIZE * 0x10 );
	state_save_register_UINT32( "i960", cpu, "rcache_frame_addr", i960.rcache_frame_addr, I960_RCACHE_SIZE );
	state_save_register_INT32( "i960", cpu, "rcache_pos", &i960.rcache_pos, 1 );
	state_save_register_double( "i960", cpu, "fp", i960.fp, 4 );
	state_save_register_UINT32( "i960", cpu, "SAT", &i960.SAT, 1 );
	state_save_register_UINT32( "i960", cpu, "PRCB", &i960.PRCB, 1 );
	state_save_register_UINT32( "i960", cpu, "PC", &i960.PC, 1 );
	state_save_register_UINT32( "i960", cpu, "AC", &i960.AC, 1 );
	state_save_register_UINT32( "i960", cpu, "IP", &i960.IP, 1 );
	state_save_register_UINT32( "i960", cpu, "PIP", &i960.PIP, 1 );
	state_save_register_UINT32( "i960", cpu, "ICR", &i960.ICR, 1 );
	state_save_register_int( "i960", cpu, "immediate_irq", &i960.immediate_irq );
	state_save_register_int( "i960", cpu, "immediate_vector", &i960.immediate_vector );
	state_save_register_int( "i960", cpu, "immediate_pri", &i960.immediate_pri );
	state_save_register_INT8( "i960", cpu, "irq_line_state", i960.irq_line_state, 4 );
	state_save_register_int( "i960", cpu, "stalled", &i960.stalled );
	state_save_register_UINT32( "i960", cpu, "stall_t1", &i960.stall_state.t1, 1 );
	state_save_register_UINT32( "i960", cpu, "stall_t2", &i960.stall_state.t2, 1 );
	state_save_register_int( "i960", cpu, "stall_index", &i960.stall_state.index );
	state_save_register_int( "i960", cpu, "stall_size", &i960.stall_state.size );
	state_save_register_int( "i960", cpu, "stall_burst", &i960.stall_state.burst_mode );
	state_save_register_int( "i960", cpu, "stall_write", &i960.stall_state.iswriteop );
}

void i960_reset( void *param )
{
	m_SAT		= cpu_readmem32ledw_dword( 0 );
	m_PRCB		= cpu_readmem32ledw_dword( 4 );
	m_IP		= cpu_readmem32ledw_dword( 12 );
	m_PC		= 0x001f2002;
	m_AC		= 0;
	m_ICR		= 0xff000000;
	m_immediate_irq = 0;
	m_immediate_vector = 0;
	m_immediate_pri = 0;

	memset( m_r, 0, sizeof( m_r ) );
	memset( m_rcache, 0, sizeof( m_rcache ) );
	memset( m_rcache_frame_addr, 0, sizeof( m_rcache_frame_addr ) );
	memset( m_fp, 0, sizeof( m_fp ) );
	memset( m_irq_line_state, 0, sizeof( m_irq_line_state ) );

	m_r[ I960_FP ] = cpu_readmem32ledw_dword( m_PRCB + 24 );
	m_r[ I960_SP ] = m_r[ I960_FP ] + 64;
	m_rcache_pos = 0;

	m_stalled = 0;
	m_stall_state.burst_mode = 0;
	m_PIP = m_IP;
}

void i960_exit( void )
{
}

unsigned i960_get_context( void *dst )
{
	if( dst )
		*(i960_regs *)dst = i960;
	return sizeof( i960_regs );
}

void i960_set_context( void *src )
{
	if( src )
		i960 = *(i960_regs *)src;
}

unsigned i960_get_reg( int regnum )
{
	switch( regnum )
	{
	case REG_PC:
	case I960_IPREG:	return m_IP;
	case REG_PREVIOUSPC:
	case I960_PIPREG:	return m_PIP;
	case REG_SP:		return m_r[ I960_SP ];
	case I960_SATREG:	return m_SAT;
	case I960_PRCBREG:	return m_PRCB;
	case I960_PCREG:	return m_PC;
	case I960_ACREG:	return m_AC;
	}
	if( regnum >= I960_R0REG && regnum < I960_R0REG + 32 )
		return m_r[ regnum - I960_R0REG ];
	return 0;
}

void i960_set_reg( int regnum, unsigned val )
{
	switch( regnum )
	{
	case REG_PC:
	case I960_IPREG:	m_IP = val; return;
	case REG_SP:		m_r[ I960_SP ] = val; return;
	case I960_SATREG:	m_SAT = val; return;
	case I960_PRCBREG:	m_PRCB = val; return;
	case I960_PCREG:	m_PC = val; return;
	case I960_ACREG:	m_AC = val; return;
	}
	if( regnum >= I960_R0REG && regnum < I960_R0REG + 32 )
		m_r[ regnum - I960_R0REG ] = val;
}

const char *i960_info( void *context, int regnum )
{
	static char buffer[ 16 ][ 47 + 1 ];
	static int which = 0;
	i960_regs *r = context ? (i960_regs *)context : &i960;

	which = ( which + 1 ) % 16;
	buffer[ which ][ 0 ] = '\0';

	switch( regnum )
	{
	case CPU_INFO_REG + I960_IPREG:		sprintf( buffer[ which ], "IP:%08X", r->IP ); break;
	case CPU_INFO_REG + I960_PCREG:		sprintf( buffer[ which ], "PC:%08X", r->PC ); break;
	case CPU_INFO_REG + I960_ACREG:		sprintf( buffer[ which ], "AC:%08X", r->AC ); break;
	case CPU_INFO_FLAGS:				sprintf( buffer[ which ], "%08X", r->AC ); break;
	case CPU_INFO_NAME:		return "i80960KB";
	case CPU_INFO_FAMILY:	return "Intel i960";
	case CPU_INFO_VERSION:	return "1.0";
	case CPU_INFO_FILE:		return __FILE__;
	case CPU_INFO_CREDITS:	return "Farfetch'd, R. Belmont (current MAME)";
	}
	return buffer[ which ];
}

unsigned i960_dasm( char *buffer, unsigned pc )
{
	sprintf( buffer, "$%08x", cpu_readmem32ledw_dword( pc ) );
	return 4;
}
