/*
 * Sony CXD8530AQ/CXD8530BQ/CXD8530CQ/CXD8661R
 *
 * PSX CPU emulator for the MAME project written by smf
 * Thanks to Farfetch'd for information on the delay slot bug
 *
 * The PSX CPU is a custom r3000a with a built in
 * geometry transform engine, no mmu & no data cache.
 *
 * There is a stall circuit for load delays, but
 * it doesn't work if the load occurs in a branch
 * delay slot.
 *
 */

#include <stdio.h>
#include "cpuintrf.h"
#include "memory.h"
#include "mamedbg.h"
#include "psx.h"
#include "state.h"
#include "usrintrf.h"

#define EXC_INT ( 0 )
#define EXC_ADEL ( 4 )
#define EXC_ADES ( 5 )
#define EXC_SYS ( 8 )
#define EXC_BP ( 9 )
#define EXC_RI ( 10 )
#define EXC_CPU ( 11 )
#define EXC_OVF ( 12 )

#define CP0_RANDOM ( 1 )
#define CP0_BADVADDR ( 8 )
#define CP0_SR ( 12 )
#define CP0_CAUSE ( 13 )
#define CP0_EPC ( 14 )
#define CP0_PRID ( 15 )

#define SR_IEC ( 1L << 0 )
#define SR_KUC ( 1L << 1 )
#define SR_ISC ( 1L << 16 )
#define SR_SWC ( 1L << 17 )
#define SR_TS  ( 1L << 21 )
#define SR_BEV ( 1L << 22 )
#define SR_RE ( 1L << 25 )
#define SR_CU0 ( 1L << 28 )
#define SR_CU1 ( 1L << 29 )
#define SR_CU2 ( 1L << 30 )
#define SR_CU3 ( 1L << 31 )

#define CAUSE_EXC ( 31L << 2 )
#define CAUSE_IP ( 255L << 8 )
#define CAUSE_IP2 ( 1L << 10 )
#define CAUSE_IP3 ( 1L << 11 )
#define CAUSE_IP4 ( 1L << 12 )
#define CAUSE_IP5 ( 1L << 13 )
#define CAUSE_IP6 ( 1L << 14 )
#define CAUSE_IP7 ( 1L << 15 )
#define CAUSE_CE ( 3L << 28 )
#define CAUSE_CE0 ( 0L << 28 )
#define CAUSE_CE1 ( 1L << 28 )
#define CAUSE_CE2 ( 2L << 28 )
#define CAUSE_BD ( 1L << 31 )

static UINT8 mips_reg_layout[] =
{
	MIPS_PC, -1,
	MIPS_DELAYV, MIPS_DELAYR, -1,
	MIPS_HI, MIPS_LO, -1,
	-1,
	MIPS_R0, MIPS_R1, -1,
	MIPS_R2, MIPS_R3, -1,
	MIPS_R4, MIPS_R5, -1,
	MIPS_R6, MIPS_R7, -1,
	MIPS_R8, MIPS_R9, -1,
	MIPS_R10, MIPS_R11, -1,
	MIPS_R12, MIPS_R13, -1,
	MIPS_R14, MIPS_R15, -1,
	MIPS_R16, MIPS_R17, -1,
	MIPS_R18, MIPS_R19, -1,
	MIPS_R20, MIPS_R21, -1,
	MIPS_R22, MIPS_R23, -1,
	MIPS_R24, MIPS_R25, -1,
	MIPS_R26, MIPS_R27, -1,
	MIPS_R28, MIPS_R29, -1,
	MIPS_R30, MIPS_R31, -1,
	-1,
	MIPS_CP0R0, MIPS_CP0R1, -1,
	MIPS_CP0R2, MIPS_CP0R3, -1,
	MIPS_CP0R4, MIPS_CP0R5, -1,
	MIPS_CP0R6, MIPS_CP0R7, -1,
	MIPS_CP0R8, MIPS_CP0R9, -1,
	MIPS_CP0R10, MIPS_CP0R11, -1,
	MIPS_CP0R12, MIPS_CP0R13, -1,
	MIPS_CP0R14, MIPS_CP0R15, -1,
	MIPS_CP0R16, MIPS_CP0R17, -1,
	MIPS_CP0R18, MIPS_CP0R19, -1,
	MIPS_CP0R20, MIPS_CP0R21, -1,
	MIPS_CP0R22, MIPS_CP0R23, -1,
	MIPS_CP0R24, MIPS_CP0R25, -1,
	MIPS_CP0R26, MIPS_CP0R27, -1,
	MIPS_CP0R28, MIPS_CP0R29, -1,
	MIPS_CP0R30, MIPS_CP0R31, -1,
	-1,
	MIPS_CP2DR0, MIPS_CP2DR1, -1,
	MIPS_CP2DR2, MIPS_CP2DR3, -1,
	MIPS_CP2DR4, MIPS_CP2DR5, -1,
	MIPS_CP2DR6, MIPS_CP2DR7, -1,
	MIPS_CP2DR8, MIPS_CP2DR9, -1,
	MIPS_CP2DR10, MIPS_CP2DR11, -1,
	MIPS_CP2DR12, MIPS_CP2DR13, -1,
	MIPS_CP2DR14, MIPS_CP2DR15, -1,
	MIPS_CP2DR16, MIPS_CP2DR17, -1,
	MIPS_CP2DR18, MIPS_CP2DR19, -1,
	MIPS_CP2DR20, MIPS_CP2DR21, -1,
	MIPS_CP2DR22, MIPS_CP2DR23, -1,
	MIPS_CP2DR24, MIPS_CP2DR25, -1,
	MIPS_CP2DR26, MIPS_CP2DR27, -1,
	MIPS_CP2DR28, MIPS_CP2DR29, -1,
	MIPS_CP2DR30, MIPS_CP2DR31, -1,
	-1,
	MIPS_CP2CR0, MIPS_CP2CR1, -1,
	MIPS_CP2CR2, MIPS_CP2CR3, -1,
	MIPS_CP2CR4, MIPS_CP2CR5, -1,
	MIPS_CP2CR6, MIPS_CP2CR7, -1,
	MIPS_CP2CR8, MIPS_CP2CR9, -1,
	MIPS_CP2CR10, MIPS_CP2CR11, -1,
	MIPS_CP2CR12, MIPS_CP2CR13, -1,
	MIPS_CP2CR14, MIPS_CP2CR15, -1,
	MIPS_CP2CR16, MIPS_CP2CR17, -1,
	MIPS_CP2CR18, MIPS_CP2CR19, -1,
	MIPS_CP2CR20, MIPS_CP2CR21, -1,
	MIPS_CP2CR22, MIPS_CP2CR23, -1,
	MIPS_CP2CR24, MIPS_CP2CR25, -1,
#if 0
/* the debugger interface is limited to 127 & the kludge no longer works */
	MIPS_CP2CR26, MIPS_CP2CR27, -1,
	MIPS_CP2CR28, MIPS_CP2CR29, -1,
	MIPS_CP2CR30, MIPS_CP2CR31,
#endif
	0
};

static UINT8 mips_win_layout[] = {
	45, 0,35,13,	/* register window (top right) */
	 0, 0,44,13,	/* disassembler window (left, upper) */
	 0,14,44, 8,	/* memory #1 window (left, middle) */
	45,14,35, 8,	/* memory #2 window (lower) */
	 0,23,80, 1 	/* command line window (bottom rows) */
};

static const char *delayn[] =
{
	"pc", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
	"pc"
};

#define REGPC ( 32 )

typedef struct
{
	UINT32 op;
	UINT32 pc;
	UINT32 delayv;
	UINT32 delayr;
	UINT32 hi;
	UINT32 lo;
	UINT32 r[ 32 ];
	UINT32 cp0r[ 32 ];
	PAIR cp2cr[ 32 ];
	PAIR cp2dr[ 32 ];
	int (*irq_callback)(int irqline);
} mips_cpu_context;

mips_cpu_context mipscpu;

int mips_ICount = 0;


/***************************************************************************
    Fast RAM

    Loads and stores to plain RAM regions registered by the driver
    (psxcpu_add_fastram) use a direct pointer instead of the memory
    system. Off unless a driver registers regions. Little-endian hosts
    only (the regions are kept in host order).
***************************************************************************/

#define PSX_MAX_FASTRAM		8

struct psx_fastram
{
	offs_t		start;
	offs_t		end;
	int			readonly;
	UINT8 *		base;
};

static struct psx_fastram fastram[ PSX_MAX_FASTRAM ];
static int fastram_count;
static struct psx_fastram *fastram_last = &fastram[ 0 ];

static INLINE void psx_jit_check_write( const void *host );

/* set by any access outside fast RAM (see psxloop.c) */
static int psx_slow_access;

#ifdef PSXJIT_VERIFY
/* debug: stores logged while the recompiler is checked against the interpreter */
#define PSXJIT_VLOG		262144
struct psx_vlog { UINT8 *p; int size; UINT32 old; };
static struct psx_vlog jit_vlog[ PSXJIT_VLOG ], interp_vlog[ PSXJIT_VLOG ];
static int jit_vlog_count, interp_vlog_count, jit_verify_logging;
static void psx_verify_log( UINT8 *p, int size )
{
	if( jit_verify_logging && interp_vlog_count < PSXJIT_VLOG )
	{
		interp_vlog[ interp_vlog_count ].p = p;
		interp_vlog[ interp_vlog_count ].size = size;
		memcpy( &interp_vlog[ interp_vlog_count ].old, p, size );
		interp_vlog_count++;
	}
}
#define PSX_VERIFY_LOG( p, size )	psx_verify_log( p, size )
#else
#define PSX_VERIFY_LOG( p, size )
#endif

void psxcpu_add_fastram( offs_t start, offs_t end, int readonly, void *base )
{
#ifndef MSB_FIRST
	int i;
	for( i = 0; i < fastram_count; i++ )
	{
		if( fastram[ i ].start == start )
		{
			break;
		}
	}
	if( i == PSX_MAX_FASTRAM )
	{
		return;
	}
	fastram[ i ].start = start;
	fastram[ i ].end = end;
	fastram[ i ].readonly = readonly;
	fastram[ i ].base = (UINT8 *)base;
	if( i == fastram_count )
	{
		fastram_count++;
	}
#endif
}

/* the region holding all 'size' bytes at 'address', or NULL */
static INLINE struct psx_fastram *find_fastram( offs_t address, int size )
{
	struct psx_fastram *r = fastram_last;
	int i;

	if( fastram_count == 0 )
	{
		return NULL;
	}
	if( address - r->start <= r->end - r->start && address + size - 1 <= r->end )
	{
		return r;
	}
	for( i = 0; i < fastram_count; i++ )
	{
		r = &fastram[ i ];
		if( address - r->start <= r->end - r->start && address + size - 1 <= r->end )
		{
			fastram_last = r;
			return r;
		}
	}
	return NULL;
}

static INLINE data8_t psx_read8( offs_t address )
{
	struct psx_fastram *r = find_fastram( address, 1 );
	if( r ) return r->base[ address - r->start ];
	psx_slow_access = 1;
	return cpu_readmem32ledw( address );
}

static INLINE data16_t psx_read16( offs_t address )
{
	struct psx_fastram *r = find_fastram( address, 2 );
	if( r ) return *(data16_t *)&r->base[ address - r->start ];
	psx_slow_access = 1;
	return cpu_readmem32ledw_word( address );
}

static INLINE data32_t psx_read32( offs_t address )
{
	struct psx_fastram *r = find_fastram( address, 4 );
	if( r ) return *(data32_t *)&r->base[ address - r->start ];
	psx_slow_access = 1;
	return cpu_readmem32ledw_dword( address );
}

static INLINE void psx_write8( offs_t address, data8_t data )
{
	struct psx_fastram *r = find_fastram( address, 1 );
	if( r && !r->readonly )
	{
		psx_jit_check_write( &r->base[ address - r->start ] );
		PSX_VERIFY_LOG( &r->base[ address - r->start ], 1 );
		r->base[ address - r->start ] = data;
		return;
	}
	psx_slow_access = 1;
	cpu_writemem32ledw( address, data );
}

static INLINE void psx_write16( offs_t address, data16_t data )
{
	struct psx_fastram *r = find_fastram( address, 2 );
	if( r && !r->readonly )
	{
		psx_jit_check_write( &r->base[ address - r->start ] );
		PSX_VERIFY_LOG( &r->base[ address - r->start ], 2 );
		*(data16_t *)&r->base[ address - r->start ] = data;
		return;
	}
	psx_slow_access = 1;
	cpu_writemem32ledw_word( address, data );
}

static INLINE void psx_write32( offs_t address, data32_t data )
{
	struct psx_fastram *r = find_fastram( address, 4 );
	if( r && !r->readonly )
	{
		psx_jit_check_write( &r->base[ address - r->start ] );
		PSX_VERIFY_LOG( &r->base[ address - r->start ], 4 );
		*(data32_t *)&r->base[ address - r->start ] = data;
		return;
	}
	psx_slow_access = 1;
	cpu_writemem32ledw_dword( address, data );
}

static void loop_arrive( UINT32 l, UINT32 from );
static int loop_kind( UINT32 l );
static int loop_pure_armed;
#define LOOP_NONE		0

#include "psxjit.c"
#include "psxloop.c"


static UINT32 mips_mtc0_writemask[]=
{
	0xffffffff, /* INDEX */
	0x00000000, /* RANDOM */
	0xffffff00, /* ENTRYLO */
	0x00000000,
	0xffe00000, /* CONTEXT */
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000, /* BADVADDR */
	0x00000000,
	0xffffffc0, /* ENTRYHI */
	0x00000000,
	0xf27fff3f, /* SR */
	0x00000300, /* CAUSE */
	0x00000000, /* EPC */
	0x00000000, /* PRID */
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000
};

/* GTE register and command trace (very verbose), off by default */
#ifdef PSX_GTE_TRACE
#define GTELOG( args )	log_cb args
#else
#define GTELOG( args )	do { } while( 0 )
#endif

static UINT32 getcp2dr( int n_reg );
static void setcp2dr( int n_reg, UINT32 n_value );
static UINT32 getcp2cr( int n_reg );
static void setcp2cr( int n_reg, UINT32 n_value );
static void docop2( int gteop );
static void mips_exception( int exception );

void mips_stop( void )
{
#ifdef MAME_DEBUG
	extern int debug_key_pressed;
	debug_key_pressed = 1;
	CALL_MAME_DEBUG;
#endif
}

static INLINE void mips_set_cp0r( int reg, UINT32 value )
{
	mipscpu.cp0r[ reg ] = value;
	if( reg == CP0_SR || reg == CP0_CAUSE )
	{
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_IEC ) != 0 && ( mipscpu.cp0r[ CP0_SR ] & mipscpu.cp0r[ CP0_CAUSE ] & CAUSE_IP ) != 0 )
		{
			mips_exception( EXC_INT );
		}
		else if( mipscpu.delayr != REGPC && ( mipscpu.pc & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 3 ) ) != 0 )
		{
			mips_exception( EXC_ADEL );
			mips_set_cp0r( CP0_BADVADDR, mipscpu.pc );
		}
	}
}

static INLINE void mips_commit_delayed_load( void )
{
	if( mipscpu.delayr != 0 )
	{
		mipscpu.r[ mipscpu.delayr ] = mipscpu.delayv;
		mipscpu.delayr = 0;
		mipscpu.delayv = 0;
	}
}

static INLINE void mips_delayed_branch( UINT32 n_adr )
{
	if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 3 ) ) != 0 )
	{
		mips_exception( EXC_ADEL );
		mips_set_cp0r( CP0_BADVADDR, n_adr );
	}
	else
	{
		mips_commit_delayed_load();
		mipscpu.delayr = REGPC;
		mipscpu.delayv = n_adr;
		mipscpu.pc += 4;
	}
}

static INLINE void mips_set_pc( unsigned val )
{
	mipscpu.pc = val;
	change_pc32ledw( val );
	mipscpu.delayr = 0;
	mipscpu.delayv = 0;
}

static INLINE void mips_advance_pc( void )
{
	if( mipscpu.delayr == REGPC )
	{
		mips_set_pc( mipscpu.delayv );
	}
	else
	{
		mips_commit_delayed_load();
		mipscpu.pc += 4;
	}
}

static INLINE void mips_load( UINT32 n_r, UINT32 n_v )
{
	mips_advance_pc();
	if( n_r != 0 )
	{
		mipscpu.r[ n_r ] = n_v;
	}
}

static INLINE void mips_delayed_load( UINT32 n_r, UINT32 n_v )
{
	if( mipscpu.delayr == REGPC )
	{
		mips_set_pc( mipscpu.delayv );
		mipscpu.delayr = n_r;
		mipscpu.delayv = n_v;
	}
	else
	{
		mips_commit_delayed_load();
		mipscpu.pc += 4;
		if( n_r != 0 )
		{
			mipscpu.r[ n_r ] = n_v;
		}
	}
}

static void mips_exception( int exception )
{
	mips_set_cp0r( CP0_SR, ( mipscpu.cp0r[ CP0_SR ] & ~0x3f ) | ( ( mipscpu.cp0r[ CP0_SR ] << 2 ) & 0x3f ) );
	if( mipscpu.delayr == REGPC )
	{
		mips_set_cp0r( CP0_EPC, mipscpu.pc - 4 );
		mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~CAUSE_EXC ) | CAUSE_BD | ( exception << 2 ) );
	}
	else
	{
		mips_commit_delayed_load();
		mips_set_cp0r( CP0_EPC, mipscpu.pc );
		mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~( CAUSE_EXC | CAUSE_BD ) ) | ( exception << 2 ) );
	}
	if( mipscpu.cp0r[ CP0_SR ] & SR_BEV )
	{
		mips_set_pc( 0xbfc00180 );
	}
	else
	{
		mips_set_pc( 0x80000080 );
	}
}

void mips_init( void )
{
	int cpu = cpu_getactivecpu();

	fastram_count = 0;
	fastram_last = &fastram[ 0 ];
	psxcpu_set_jit( 0 );

	state_save_register_UINT32( "psxcpu", cpu, "op", &mipscpu.op, 1 );
	state_save_register_UINT32( "psxcpu", cpu, "pc", &mipscpu.pc, 1 );
	state_save_register_UINT32( "psxcpu", cpu, "delayv", &mipscpu.delayv, 1 );
	state_save_register_UINT32( "psxcpu", cpu, "delayr", &mipscpu.delayr, 1 );
	state_save_register_UINT32( "psxcpu", cpu, "hi", &mipscpu.hi, 1 );
	state_save_register_UINT32( "psxcpu", cpu, "lo", &mipscpu.lo, 1 );
	state_save_register_UINT32( "psxcpu", cpu, "r", &mipscpu.r[ 0 ], 32 );
	state_save_register_UINT32( "psxcpu", cpu, "cp0r", &mipscpu.cp0r[ 0 ], 32 );
	state_save_register_UINT32( "psxcpu", cpu, "cp2cr", &mipscpu.cp2cr[ 0 ].d, 32 );
	state_save_register_UINT32( "psxcpu", cpu, "cp2dr", &mipscpu.cp2dr[ 0 ].d, 32 );
}

void mips_reset( void *param )
{
	mips_set_cp0r( CP0_SR, ( mipscpu.cp0r[ CP0_SR ] & ~( SR_TS | SR_SWC | SR_KUC | SR_IEC ) ) | SR_BEV );
	mips_set_cp0r( CP0_RANDOM, 63 ); /* todo: */
	mips_set_cp0r( CP0_PRID, 0x00000200 ); /* todo: */
	mips_set_pc( 0xbfc00000 );
}

void mips_exit( void )
{
	psxcpu_set_jit( 0 );
}

/* one instruction at mipscpu.pc (opcode in mipscpu.op), without the cycle */
static INLINE void mips_step( void )
{
	UINT32 n_res;

	switch( INS_OP( mipscpu.op ) )
	{
	case OP_SPECIAL:
		switch( INS_FUNCT( mipscpu.op ) )
		{
		case FUNCT_SLL:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RT( mipscpu.op ) ] << INS_SHAMT( mipscpu.op ) );
			break;
		case FUNCT_SRL:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RT( mipscpu.op ) ] >> INS_SHAMT( mipscpu.op ) );
			break;
		case FUNCT_SRA:
			mips_load( INS_RD( mipscpu.op ), (INT32)mipscpu.r[ INS_RT( mipscpu.op ) ] >> INS_SHAMT( mipscpu.op ) );
			break;
		case FUNCT_SLLV:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RT( mipscpu.op ) ] << ( mipscpu.r[ INS_RS( mipscpu.op ) ] & 31 ) );
			break;
		case FUNCT_SRLV:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RT( mipscpu.op ) ] >> ( mipscpu.r[ INS_RS( mipscpu.op ) ] & 31 ) );
			break;
		case FUNCT_SRAV:
			mips_load( INS_RD( mipscpu.op ), (INT32)mipscpu.r[ INS_RT( mipscpu.op ) ] >> ( mipscpu.r[ INS_RS( mipscpu.op ) ] & 31 ) );
			break;
		case FUNCT_JR:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				mips_delayed_branch( mipscpu.r[ INS_RS( mipscpu.op ) ] );
			}
			break;
		case FUNCT_JALR:
			n_res = mipscpu.pc + 8;
			mips_delayed_branch( mipscpu.r[ INS_RS( mipscpu.op ) ] );
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mipscpu.r[ INS_RD( mipscpu.op ) ] = n_res;
			}
			break;
		case FUNCT_SYSCALL:
			mips_exception( EXC_SYS );
			break;
		case FUNCT_BREAK:
			mips_exception( EXC_BP );
			break;
		case FUNCT_MFHI:
			mips_load( INS_RD( mipscpu.op ), mipscpu.hi );
			break;
		case FUNCT_MTHI:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				mips_advance_pc();
				mipscpu.hi = mipscpu.r[ INS_RS( mipscpu.op ) ];
			}
			break;
		case FUNCT_MFLO:
			mips_load( INS_RD( mipscpu.op ),  mipscpu.lo );
			break;
		case FUNCT_MTLO:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				mips_advance_pc();
				mipscpu.lo = mipscpu.r[ INS_RS( mipscpu.op ) ];
			}
			break;
		case FUNCT_MULT:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				INT64 n_res64;
				n_res64 = MUL_64_32_32( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ], (INT32)mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
				mipscpu.lo = LO32_32_64( n_res64 );
				mipscpu.hi = HI32_32_64( n_res64 );
			}
			break;
		case FUNCT_MULTU:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				UINT64 n_res64;
				n_res64 = MUL_U64_U32_U32( mipscpu.r[ INS_RS( mipscpu.op ) ], mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
				mipscpu.lo = LO32_U32_U64( n_res64 );
				mipscpu.hi = HI32_U32_U64( n_res64 );
			}
			break;
		case FUNCT_DIV:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				UINT32 n_div;
				UINT32 n_mod;
				if( mipscpu.r[ INS_RT( mipscpu.op ) ] != 0 )
				{
					n_div = (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] / (INT32)mipscpu.r[ INS_RT( mipscpu.op ) ];
					n_mod = (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] % (INT32)mipscpu.r[ INS_RT( mipscpu.op ) ];
					mips_advance_pc();
					mipscpu.lo = n_div;
					mipscpu.hi = n_mod;
				}
				else
				{
					mips_advance_pc();
				}
			}
			break;
		case FUNCT_DIVU:
			if( INS_RD( mipscpu.op ) != 0 )
			{
				mips_exception( EXC_RI );
			}
			else
			{
				UINT32 n_div;
				UINT32 n_mod;
				if( mipscpu.r[ INS_RT( mipscpu.op ) ] != 0 )
				{
					n_div = mipscpu.r[ INS_RS( mipscpu.op ) ] / mipscpu.r[ INS_RT( mipscpu.op ) ];
					n_mod = mipscpu.r[ INS_RS( mipscpu.op ) ] % mipscpu.r[ INS_RT( mipscpu.op ) ];
					mips_advance_pc();
					mipscpu.lo = n_div;
					mipscpu.hi = n_mod;
				}
				else
				{
					mips_advance_pc();
				}
			}
			break;
		case FUNCT_ADD:
			{
				n_res = mipscpu.r[ INS_RS( mipscpu.op ) ] + mipscpu.r[ INS_RT( mipscpu.op ) ];
				if( (INT32)( ~( mipscpu.r[ INS_RS( mipscpu.op ) ] ^ mipscpu.r[ INS_RT( mipscpu.op ) ] ) & ( mipscpu.r[ INS_RS( mipscpu.op ) ] ^ n_res ) ) < 0 )
				{
					mips_exception( EXC_OVF );
				}
				else
				{
					mips_load( INS_RD( mipscpu.op ), n_res );
				}
			}
			break;
		case FUNCT_ADDU:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] + mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		case FUNCT_SUB:
			n_res = mipscpu.r[ INS_RS( mipscpu.op ) ] - mipscpu.r[ INS_RT( mipscpu.op ) ];
			if( (INT32)( ( mipscpu.r[ INS_RS( mipscpu.op ) ] ^ mipscpu.r[ INS_RT( mipscpu.op ) ] ) & ( mipscpu.r[ INS_RS( mipscpu.op ) ] ^ n_res ) ) < 0 )
			{
				mips_exception( EXC_OVF );
			}
			else
			{
				mips_load( INS_RD( mipscpu.op ), n_res );
			}
			break;
		case FUNCT_SUBU:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] - mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		case FUNCT_AND:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] & mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		case FUNCT_OR:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] | mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		case FUNCT_XOR:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] ^ mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		case FUNCT_NOR:
			mips_load( INS_RD( mipscpu.op ), ~( mipscpu.r[ INS_RS( mipscpu.op ) ] | mipscpu.r[ INS_RT( mipscpu.op ) ] ) );
			break;
		case FUNCT_SLT:
			mips_load( INS_RD( mipscpu.op ), (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] < (INT32)mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		case FUNCT_SLTU:
			mips_load( INS_RD( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] < mipscpu.r[ INS_RT( mipscpu.op ) ] );
			break;
		default:
			mips_exception( EXC_RI );
			break;
		}
		break;
	case OP_REGIMM:
		switch( INS_RT( mipscpu.op ) )
		{
		case RT_BLTZ:
			if( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] < 0 )
			{
				mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			break;
		case RT_BGEZ:
			if( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] >= 0 )
			{
				mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			break;
		case RT_BLTZAL:
			n_res = mipscpu.pc + 8;
			if( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] < 0 )
			{
				mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			mipscpu.r[ 31 ] = n_res;
			break;
		case RT_BGEZAL:
			n_res = mipscpu.pc + 8;
			if( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] >= 0 )
			{
				mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
			}
			else
			{
				mips_advance_pc();
			}
			mipscpu.r[ 31 ] = n_res;
			break;
		}
		break;
	case OP_J:
		mips_delayed_branch( ( ( mipscpu.pc + 4 ) & 0xf0000000 ) + ( INS_TARGET( mipscpu.op ) << 2 ) );
		break;
	case OP_JAL:
		n_res = mipscpu.pc + 8;
		mips_delayed_branch( ( ( mipscpu.pc + 4 ) & 0xf0000000 ) + ( INS_TARGET( mipscpu.op ) << 2 ) );
		mipscpu.r[ 31 ] = n_res;
		break;
	case OP_BEQ:
		if( mipscpu.r[ INS_RS( mipscpu.op ) ] == mipscpu.r[ INS_RT( mipscpu.op ) ] )
		{
			mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
		}
		else
		{
			mips_advance_pc();
		}
		break;
	case OP_BNE:
		if( mipscpu.r[ INS_RS( mipscpu.op ) ] != mipscpu.r[ INS_RT( mipscpu.op ) ] )
		{
			mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
		}
		else
		{
			mips_advance_pc();
		}
		break;
	case OP_BLEZ:
		if( INS_RT( mipscpu.op ) != 0 )
		{
			mips_exception( EXC_RI );
		}
		else if( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] <= 0 )
		{
			mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
		}
		else
		{
			mips_advance_pc();
		}
		break;
	case OP_BGTZ:
		if( INS_RT( mipscpu.op ) != 0 )
		{
			mips_exception( EXC_RI );
		}
		else if( (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] > 0 )
		{
			mips_delayed_branch( mipscpu.pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) << 2 ) );
		}
		else
		{
			mips_advance_pc();
		}
		break;
	case OP_ADDI:
		{
			UINT32 n_imm;
			n_imm = MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			n_res = mipscpu.r[ INS_RS( mipscpu.op ) ] + n_imm;
			if( (INT32)( ~( mipscpu.r[ INS_RS( mipscpu.op ) ] ^ n_imm ) & ( mipscpu.r[ INS_RS( mipscpu.op ) ] ^ n_res ) ) < 0 )
			{
				mips_exception( EXC_OVF );
			}
			else
			{
				mips_load( INS_RT( mipscpu.op ), n_res );
			}
		}
		break;
	case OP_ADDIU:
		mips_load( INS_RT( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) );
		break;
	case OP_SLTI:
		mips_load( INS_RT( mipscpu.op ), (INT32)mipscpu.r[ INS_RS( mipscpu.op ) ] < MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) );
		break;
	case OP_SLTIU:
		mips_load( INS_RT( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] < (UINT32)MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) ) );
		break;
	case OP_ANDI:
		mips_load( INS_RT( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] & INS_IMMEDIATE( mipscpu.op ) );
		break;
	case OP_ORI:
		mips_load( INS_RT( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] | INS_IMMEDIATE( mipscpu.op ) );
		break;
	case OP_XORI:
		mips_load( INS_RT( mipscpu.op ), mipscpu.r[ INS_RS( mipscpu.op ) ] ^ INS_IMMEDIATE( mipscpu.op ) );
		break;
	case OP_LUI:
		mips_load( INS_RT( mipscpu.op ), INS_IMMEDIATE( mipscpu.op ) << 16 );
		break;
	case OP_COP0:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) != 0 && ( mipscpu.cp0r[ CP0_SR ] & SR_CU0 ) == 0 )
		{
			mips_exception( EXC_CPU );
			mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~CAUSE_CE ) | CAUSE_CE0 );
		}
		else
		{
			switch( INS_RS( mipscpu.op ) )
			{
			case RS_MFC:
				mips_delayed_load( INS_RT( mipscpu.op ), mipscpu.cp0r[ INS_RD( mipscpu.op ) ] );
				break;
			case RS_CFC:
				/* todo: */
				log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 CFC not supported\n", mipscpu.pc );
				mips_stop();
				mips_advance_pc();
				break;
			case RS_MTC:
				n_res = ( mipscpu.cp0r[ INS_RD( mipscpu.op ) ] & ~mips_mtc0_writemask[ INS_RD( mipscpu.op ) ] ) |
					( mipscpu.r[ INS_RT( mipscpu.op ) ] & mips_mtc0_writemask[ INS_RD( mipscpu.op ) ] );
				mips_advance_pc();
				mips_set_cp0r( INS_RD( mipscpu.op ), n_res );
				break;
			case RS_CTC:
				/* todo: */
				log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 CTC not supported\n", mipscpu.pc );
				mips_stop();
				mips_advance_pc();
				break;
			case RS_BC:
				switch( INS_RT( mipscpu.op ) )
				{
				case RT_BCF:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 BCF not supported\n", mipscpu.pc );
					mips_stop();
					mips_advance_pc();
					break;
				case RT_BCT:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 BCT not supported\n", mipscpu.pc );
					mips_stop();
					mips_advance_pc();
					break;
				default:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			default:
				switch( INS_CO( mipscpu.op ) )
				{
				case 1:
					switch( INS_CF( mipscpu.op ) )
					{
					case CF_RFE:
						mips_advance_pc();
						mips_set_cp0r( CP0_SR, ( mipscpu.cp0r[ CP0_SR ] & ~0xf ) | ( ( mipscpu.cp0r[ CP0_SR ] >> 2 ) & 0xf ) );
						break;
					default:
						/* todo: */
						log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 unknown command %08x\n", mipscpu.pc, mipscpu.op );
						mips_stop();
						mips_advance_pc();
						break;
					}
					break;
				default:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP0 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			}
		}
		break;
	case OP_COP1:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_CU1 ) == 0 )
		{
			mips_exception( EXC_CPU );
			mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~CAUSE_CE ) | CAUSE_CE1 );
		}
		else
		{
			switch( INS_RS( mipscpu.op ) )
			{
			case RS_MFC:
				/* todo: */
				log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 BCT not supported\n", mipscpu.pc );
				mips_stop();
				mips_advance_pc();
				break;
			case RS_CFC:
				/* todo: */
				log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 CFC not supported\n", mipscpu.pc );
				mips_stop();
				mips_advance_pc();
				break;
			case RS_MTC:
				/* todo: */
				log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 MTC not supported\n", mipscpu.pc );
				mips_stop();
				mips_advance_pc();
				break;
			case RS_CTC:
				/* todo: */
				log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 CTC not supported\n", mipscpu.pc );
				mips_stop();
				mips_advance_pc();
				break;
			case RS_BC:
				switch( INS_RT( mipscpu.op ) )
				{
				case RT_BCF:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 BCF not supported\n", mipscpu.pc );
					mips_stop();
					mips_advance_pc();
					break;
				case RT_BCT:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 BCT not supported\n", mipscpu.pc );
					mips_stop();
					mips_advance_pc();
					break;
				default:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			default:
				switch( INS_CO( mipscpu.op ) )
				{
				case 1:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				default:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			}
		}
		break;
	case OP_COP2:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_CU2 ) == 0 )
		{
			mips_exception( EXC_CPU );
			mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~CAUSE_CE ) | CAUSE_CE2 );
		}
		else
		{
			switch( INS_RS( mipscpu.op ) )
			{
			case RS_MFC:
				mips_delayed_load( INS_RT( mipscpu.op ), getcp2dr( INS_RD( mipscpu.op ) ) );
				break;
			case RS_CFC:
				mips_delayed_load( INS_RT( mipscpu.op ), getcp2cr( INS_RD( mipscpu.op ) ) );
				break;
			case RS_MTC:
				setcp2dr( INS_RD( mipscpu.op ), mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
				break;
			case RS_CTC:
				setcp2cr( INS_RD( mipscpu.op ), mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
				break;
			case RS_BC:
				switch( INS_RT( mipscpu.op ) )
				{
				case RT_BCF:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP2 BCF not supported\n", mipscpu.pc );
					mips_stop();
					mips_advance_pc();
					break;
				case RT_BCT:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP2 BCT not supported\n", mipscpu.pc );
					mips_stop();
					mips_advance_pc();
					break;
				default:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP2 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			default:
				switch( INS_CO( mipscpu.op ) )
				{
				case 1:
					docop2( INS_COFUN( mipscpu.op ) );
					mips_advance_pc();
					break;
				default:
					/* todo: */
					log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP2 unknown command %08x\n", mipscpu.pc, mipscpu.op );
					mips_stop();
					mips_advance_pc();
					break;
				}
				break;
			}
		}
		break;
	case OP_LB:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LB SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), MIPS_BYTE_EXTEND( psx_read8( n_adr ^ 3 ) ) );
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), MIPS_BYTE_EXTEND( psx_read8( n_adr ) ) );
			}
		}
		break;
	case OP_LH:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LH SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 1 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), MIPS_WORD_EXTEND( psx_read16( n_adr ^ 2 ) ) );
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 1 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), MIPS_WORD_EXTEND( psx_read16( n_adr ) ) );
			}
		}
		break;
	case OP_LWL:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LWL SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 0:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0x00ffffff ) | ( (UINT32)psx_read8( n_adr + 3 ) << 24 );
					break;
				case 1:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0x0000ffff ) | ( (UINT32)psx_read16( n_adr + 1 ) << 16 );
					break;
				case 2:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0x000000ff ) | ( (UINT32)psx_read8( n_adr - 1 ) << 8 ) | ( (UINT32)psx_read16( n_adr ) << 16 );
					break;
				default:
					n_res = psx_read32( n_adr - 3 );
					break;
				}
				mips_delayed_load( INS_RT( mipscpu.op ), n_res );
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 0:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0x00ffffff ) | ( (UINT32)psx_read8( n_adr ) << 24 );
					break;
				case 1:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0x0000ffff ) | ( (UINT32)psx_read16( n_adr - 1 ) << 16 );
					break;
				case 2:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0x000000ff ) | ( (UINT32)psx_read16( n_adr - 2 ) << 8 ) | ( (UINT32)psx_read8( n_adr ) << 24 );
					break;
				default:
					n_res = psx_read32( n_adr - 3 );
					break;
				}
				mips_delayed_load( INS_RT( mipscpu.op ), n_res );
			}
		}
		break;
	case OP_LW:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LW SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 3 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), psx_read32( n_adr ) );
			}
		}
		break;
	case OP_LBU:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LBU SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), psx_read8( n_adr ^ 3 ) );
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), psx_read8( n_adr ) );
			}
		}
		break;
	case OP_LHU:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LHU SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 1 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), psx_read16( n_adr ^ 2 ) );
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 1 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				mips_delayed_load( INS_RT( mipscpu.op ), psx_read16( n_adr ) );
			}
		}
		break;
	case OP_LWR:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LWR SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 3:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0xffffff00 ) | psx_read8( n_adr - 3 );
					break;
				case 2:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0xffff0000 ) | psx_read16( n_adr - 2 );
					break;
				case 1:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0xff000000 ) | psx_read16( n_adr - 1 ) | ( (UINT32)psx_read8( n_adr + 1 ) << 16 );
					break;
				default:
					n_res = psx_read32( n_adr );
					break;
				}
				mips_delayed_load( INS_RT( mipscpu.op ), n_res );
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 3:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0xffffff00 ) | psx_read8( n_adr );
					break;
				case 2:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0xffff0000 ) | psx_read16( n_adr );
					break;
				case 1:
					n_res = ( mipscpu.r[ INS_RT( mipscpu.op ) ] & 0xff000000 ) | psx_read8( n_adr ) | ( (UINT32)psx_read16( n_adr + 1 ) << 8 );
					break;
				default:
					n_res = psx_read32( n_adr );
					break;
				}
				mips_delayed_load( INS_RT( mipscpu.op ), n_res );
			}
		}
		break;
	case OP_SB:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: SB SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				psx_write8( n_adr ^ 3, mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				psx_write8( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
			}
		}
		break;
	case OP_SH:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: SH SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 1 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				psx_write16( n_adr ^ 2, mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 1 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				psx_write16( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
			}
		}
		break;
	case OP_SWL:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: SWL SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 0:
					psx_write8( n_adr + 3, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 24 );
					break;
				case 1:
					psx_write16( n_adr + 1, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 16 );
					break;
				case 2:
					psx_write8( n_adr - 1, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 8 );
					psx_write16( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 16 );
					break;
				case 3:
					psx_write32( n_adr - 3, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				}
				mips_advance_pc();
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 0:
					psx_write8( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 24 );
					break;
				case 1:
					psx_write16( n_adr - 1, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 16 );
					break;
				case 2:
					psx_write16( n_adr - 2, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 8 );
					psx_write8( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 24 );
					break;
				case 3:
					psx_write32( n_adr - 3, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				}
				mips_advance_pc();
			}
		}
		break;
	case OP_SW:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
/* used by bootstrap
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: SW SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
*/
			mips_advance_pc();
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 3 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				psx_write32( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
				mips_advance_pc();
			}
		}
		break;
	case OP_SWR:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: SWR SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & ( SR_RE | SR_KUC ) ) == ( SR_RE | SR_KUC ) )
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 0:
					psx_write32( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				case 1:
					psx_write16( n_adr - 1, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					psx_write8( n_adr + 1, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 16 );
					break;
				case 2:
					psx_write16( n_adr - 2, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				case 3:
					psx_write8( n_adr - 3, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				}
				mips_advance_pc();
			}
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				switch( n_adr & 3 )
				{
				case 0:
					psx_write32( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				case 1:
					psx_write8( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					psx_write16( n_adr + 1, mipscpu.r[ INS_RT( mipscpu.op ) ] >> 8 );
					break;
				case 2:
					psx_write16( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				case 3:
					psx_write8( n_adr, mipscpu.r[ INS_RT( mipscpu.op ) ] );
					break;
				}
				mips_advance_pc();
			}
		}
		break;
	case OP_LWC1:
		/* todo: */
		log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 LWC not supported\n", mipscpu.pc );
		mips_stop();
		mips_advance_pc();
		break;
	case OP_LWC2:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_CU2 ) == 0 )
		{
			mips_exception( EXC_CPU );
			mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~CAUSE_CE ) | CAUSE_CE2 );
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: LWC2 SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 3 ) ) != 0 )
			{
				mips_exception( EXC_ADEL );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				/* todo: delay? */
				setcp2dr( INS_RT( mipscpu.op ), psx_read32( n_adr ) );
				mips_advance_pc();
			}
		}
		break;
	case OP_SWC1:
		/* todo: */
		log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: COP1 SWC not supported\n", mipscpu.pc );
		mips_stop();
		mips_advance_pc();
		break;
	case OP_SWC2:
		if( ( mipscpu.cp0r[ CP0_SR ] & SR_CU2 ) == 0 )
		{
			mips_exception( EXC_CPU );
			mips_set_cp0r( CP0_CAUSE, ( mipscpu.cp0r[ CP0_CAUSE ] & ~CAUSE_CE ) | CAUSE_CE2 );
		}
		else if( ( mipscpu.cp0r[ CP0_SR ] & SR_ISC ) != 0 )
		{
			/* todo: */
			log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: SWC2 SR_ISC not supported\n", mipscpu.pc );
			mips_stop();
			mips_advance_pc();
		}
		else
		{
			UINT32 n_adr;
			n_adr = mipscpu.r[ INS_RS( mipscpu.op ) ] + MIPS_WORD_EXTEND( INS_IMMEDIATE( mipscpu.op ) );
			if( ( n_adr & ( ( ( mipscpu.cp0r[ CP0_SR ] & SR_KUC ) << 30 ) | 3 ) ) != 0 )
			{
				mips_exception( EXC_ADES );
				mips_set_cp0r( CP0_BADVADDR, n_adr );
			}
			else
			{
				psx_write32( n_adr, getcp2dr( INS_RT( mipscpu.op ) ) );
				mips_advance_pc();
			}
		}
		break;
	default:
		log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: unknown opcode %08x\n", mipscpu.pc, mipscpu.op );
		mips_stop();
		mips_exception( EXC_RI );
		break;
	}
}

int mips_execute( int cycles )
{
	mips_ICount = cycles;
	/* a polling loop's pass is only trusted within one timeslice (psxloop.c): */
	/* RAM may change between timeslices, and after a state load or a reset */
	loop_pure_armed = 0;
	do
	{
		CALL_MAME_DEBUG;

		if( jit_enabled && jit_run() )
		{
			continue;
		}
		{
			UINT32 from = mipscpu.pc;
			mipscpu.op = cpu_readop32( from );
			mips_step();
			mips_ICount--;
			/* back to an earlier address: maybe a wait loop to skip (psxloop.c) */
			if( mipscpu.pc < from && mipscpu.delayr == 0 )
			{
				loop_arrive( mipscpu.pc, from );
			}
		}
	} while( mips_ICount > 0 );

	return cycles - mips_ICount;
}

unsigned mips_get_context( void *dst )
{
	if( dst )
	{
		*(mips_cpu_context *)dst = mipscpu;
	}
	return sizeof( mips_cpu_context );
}

void mips_set_context( void *src )
{
	if( src )
	{
		mipscpu = *(mips_cpu_context *)src;
		change_pc32ledw( mipscpu.pc );
	}
}

unsigned mips_get_reg( int regnum )
{
	switch( regnum )
	{
	case REG_PC:		return mipscpu.pc;
	case MIPS_PC:		return mipscpu.pc;
	case REG_SP:
		/* because there is no hardware stack and the pipeline causes the cpu to execute the
		instruction after a subroutine call before the subroutine is executed there is little
		chance of cmd_step_over() in mamedbg.c working. */
						return 0;
	case MIPS_DELAYV:	return mipscpu.delayv;
	case MIPS_DELAYR:	return mipscpu.delayr;
	case MIPS_HI:		return mipscpu.hi;
	case MIPS_LO:		return mipscpu.lo;
	case MIPS_R0:		return mipscpu.r[ 0 ];
	case MIPS_R1:		return mipscpu.r[ 1 ];
	case MIPS_R2:		return mipscpu.r[ 2 ];
	case MIPS_R3:		return mipscpu.r[ 3 ];
	case MIPS_R4:		return mipscpu.r[ 4 ];
	case MIPS_R5:		return mipscpu.r[ 5 ];
	case MIPS_R6:		return mipscpu.r[ 6 ];
	case MIPS_R7:		return mipscpu.r[ 7 ];
	case MIPS_R8:		return mipscpu.r[ 8 ];
	case MIPS_R9:		return mipscpu.r[ 9 ];
	case MIPS_R10:		return mipscpu.r[ 10 ];
	case MIPS_R11:		return mipscpu.r[ 11 ];
	case MIPS_R12:		return mipscpu.r[ 12 ];
	case MIPS_R13:		return mipscpu.r[ 13 ];
	case MIPS_R14:		return mipscpu.r[ 14 ];
	case MIPS_R15:		return mipscpu.r[ 15 ];
	case MIPS_R16:		return mipscpu.r[ 16 ];
	case MIPS_R17:		return mipscpu.r[ 17 ];
	case MIPS_R18:		return mipscpu.r[ 18 ];
	case MIPS_R19:		return mipscpu.r[ 19 ];
	case MIPS_R20:		return mipscpu.r[ 20 ];
	case MIPS_R21:		return mipscpu.r[ 21 ];
	case MIPS_R22:		return mipscpu.r[ 22 ];
	case MIPS_R23:		return mipscpu.r[ 23 ];
	case MIPS_R24:		return mipscpu.r[ 24 ];
	case MIPS_R25:		return mipscpu.r[ 25 ];
	case MIPS_R26:		return mipscpu.r[ 26 ];
	case MIPS_R27:		return mipscpu.r[ 27 ];
	case MIPS_R28:		return mipscpu.r[ 28 ];
	case MIPS_R29:		return mipscpu.r[ 29 ];
	case MIPS_R30:		return mipscpu.r[ 30 ];
	case MIPS_R31:		return mipscpu.r[ 31 ];
	case MIPS_CP0R0:	return mipscpu.cp0r[ 0 ];
	case MIPS_CP0R1:	return mipscpu.cp0r[ 1 ];
	case MIPS_CP0R2:	return mipscpu.cp0r[ 2 ];
	case MIPS_CP0R3:	return mipscpu.cp0r[ 3 ];
	case MIPS_CP0R4:	return mipscpu.cp0r[ 4 ];
	case MIPS_CP0R5:	return mipscpu.cp0r[ 5 ];
	case MIPS_CP0R6:	return mipscpu.cp0r[ 6 ];
	case MIPS_CP0R7:	return mipscpu.cp0r[ 7 ];
	case MIPS_CP0R8:	return mipscpu.cp0r[ 8 ];
	case MIPS_CP0R9:	return mipscpu.cp0r[ 9 ];
	case MIPS_CP0R10:	return mipscpu.cp0r[ 10 ];
	case MIPS_CP0R11:	return mipscpu.cp0r[ 11 ];
	case MIPS_CP0R12:	return mipscpu.cp0r[ 12 ];
	case MIPS_CP0R13:	return mipscpu.cp0r[ 13 ];
	case MIPS_CP0R14:	return mipscpu.cp0r[ 14 ];
	case MIPS_CP0R15:	return mipscpu.cp0r[ 15 ];
	case MIPS_CP0R16:	return mipscpu.cp0r[ 16 ];
	case MIPS_CP0R17:	return mipscpu.cp0r[ 17 ];
	case MIPS_CP0R18:	return mipscpu.cp0r[ 18 ];
	case MIPS_CP0R19:	return mipscpu.cp0r[ 19 ];
	case MIPS_CP0R20:	return mipscpu.cp0r[ 20 ];
	case MIPS_CP0R21:	return mipscpu.cp0r[ 21 ];
	case MIPS_CP0R22:	return mipscpu.cp0r[ 22 ];
	case MIPS_CP0R23:	return mipscpu.cp0r[ 23 ];
	case MIPS_CP0R24:	return mipscpu.cp0r[ 24 ];
	case MIPS_CP0R25:	return mipscpu.cp0r[ 25 ];
	case MIPS_CP0R26:	return mipscpu.cp0r[ 26 ];
	case MIPS_CP0R27:	return mipscpu.cp0r[ 27 ];
	case MIPS_CP0R28:	return mipscpu.cp0r[ 28 ];
	case MIPS_CP0R29:	return mipscpu.cp0r[ 29 ];
	case MIPS_CP0R30:	return mipscpu.cp0r[ 30 ];
	case MIPS_CP0R31:	return mipscpu.cp0r[ 31 ];
	case MIPS_CP2DR0:	return mipscpu.cp2dr[ 0 ].d;
	case MIPS_CP2DR1:	return mipscpu.cp2dr[ 1 ].d;
	case MIPS_CP2DR2:	return mipscpu.cp2dr[ 2 ].d;
	case MIPS_CP2DR3:	return mipscpu.cp2dr[ 3 ].d;
	case MIPS_CP2DR4:	return mipscpu.cp2dr[ 4 ].d;
	case MIPS_CP2DR5:	return mipscpu.cp2dr[ 5 ].d;
	case MIPS_CP2DR6:	return mipscpu.cp2dr[ 6 ].d;
	case MIPS_CP2DR7:	return mipscpu.cp2dr[ 7 ].d;
	case MIPS_CP2DR8:	return mipscpu.cp2dr[ 8 ].d;
	case MIPS_CP2DR9:	return mipscpu.cp2dr[ 9 ].d;
	case MIPS_CP2DR10:	return mipscpu.cp2dr[ 10 ].d;
	case MIPS_CP2DR11:	return mipscpu.cp2dr[ 11 ].d;
	case MIPS_CP2DR12:	return mipscpu.cp2dr[ 12 ].d;
	case MIPS_CP2DR13:	return mipscpu.cp2dr[ 13 ].d;
	case MIPS_CP2DR14:	return mipscpu.cp2dr[ 14 ].d;
	case MIPS_CP2DR15:	return mipscpu.cp2dr[ 15 ].d;
	case MIPS_CP2DR16:	return mipscpu.cp2dr[ 16 ].d;
	case MIPS_CP2DR17:	return mipscpu.cp2dr[ 17 ].d;
	case MIPS_CP2DR18:	return mipscpu.cp2dr[ 18 ].d;
	case MIPS_CP2DR19:	return mipscpu.cp2dr[ 19 ].d;
	case MIPS_CP2DR20:	return mipscpu.cp2dr[ 20 ].d;
	case MIPS_CP2DR21:	return mipscpu.cp2dr[ 21 ].d;
	case MIPS_CP2DR22:	return mipscpu.cp2dr[ 22 ].d;
	case MIPS_CP2DR23:	return mipscpu.cp2dr[ 23 ].d;
	case MIPS_CP2DR24:	return mipscpu.cp2dr[ 24 ].d;
	case MIPS_CP2DR25:	return mipscpu.cp2dr[ 25 ].d;
	case MIPS_CP2DR26:	return mipscpu.cp2dr[ 26 ].d;
	case MIPS_CP2DR27:	return mipscpu.cp2dr[ 27 ].d;
	case MIPS_CP2DR28:	return mipscpu.cp2dr[ 28 ].d;
	case MIPS_CP2DR29:	return mipscpu.cp2dr[ 29 ].d;
	case MIPS_CP2DR30:	return mipscpu.cp2dr[ 30 ].d;
	case MIPS_CP2DR31:	return mipscpu.cp2dr[ 31 ].d;
	case MIPS_CP2CR0:	return mipscpu.cp2cr[ 0 ].d;
	case MIPS_CP2CR1:	return mipscpu.cp2cr[ 1 ].d;
	case MIPS_CP2CR2:	return mipscpu.cp2cr[ 2 ].d;
	case MIPS_CP2CR3:	return mipscpu.cp2cr[ 3 ].d;
	case MIPS_CP2CR4:	return mipscpu.cp2cr[ 4 ].d;
	case MIPS_CP2CR5:	return mipscpu.cp2cr[ 5 ].d;
	case MIPS_CP2CR6:	return mipscpu.cp2cr[ 6 ].d;
	case MIPS_CP2CR7:	return mipscpu.cp2cr[ 7 ].d;
	case MIPS_CP2CR8:	return mipscpu.cp2cr[ 8 ].d;
	case MIPS_CP2CR9:	return mipscpu.cp2cr[ 9 ].d;
	case MIPS_CP2CR10:	return mipscpu.cp2cr[ 10 ].d;
	case MIPS_CP2CR11:	return mipscpu.cp2cr[ 11 ].d;
	case MIPS_CP2CR12:	return mipscpu.cp2cr[ 12 ].d;
	case MIPS_CP2CR13:	return mipscpu.cp2cr[ 13 ].d;
	case MIPS_CP2CR14:	return mipscpu.cp2cr[ 14 ].d;
	case MIPS_CP2CR15:	return mipscpu.cp2cr[ 15 ].d;
	case MIPS_CP2CR16:	return mipscpu.cp2cr[ 16 ].d;
	case MIPS_CP2CR17:	return mipscpu.cp2cr[ 17 ].d;
	case MIPS_CP2CR18:	return mipscpu.cp2cr[ 18 ].d;
	case MIPS_CP2CR19:	return mipscpu.cp2cr[ 19 ].d;
	case MIPS_CP2CR20:	return mipscpu.cp2cr[ 20 ].d;
	case MIPS_CP2CR21:	return mipscpu.cp2cr[ 21 ].d;
	case MIPS_CP2CR22:	return mipscpu.cp2cr[ 22 ].d;
	case MIPS_CP2CR23:	return mipscpu.cp2cr[ 23 ].d;
	case MIPS_CP2CR24:	return mipscpu.cp2cr[ 24 ].d;
	case MIPS_CP2CR25:	return mipscpu.cp2cr[ 25 ].d;
	case MIPS_CP2CR26:	return mipscpu.cp2cr[ 26 ].d;
	case MIPS_CP2CR27:	return mipscpu.cp2cr[ 27 ].d;
	case MIPS_CP2CR28:	return mipscpu.cp2cr[ 28 ].d;
	case MIPS_CP2CR29:	return mipscpu.cp2cr[ 29 ].d;
	case MIPS_CP2CR30:	return mipscpu.cp2cr[ 30 ].d;
	case MIPS_CP2CR31:	return mipscpu.cp2cr[ 31 ].d;
	}
	return 0;
}

void mips_set_reg( int regnum, unsigned val )
{
	switch( regnum )
	{
	case REG_PC:		mips_set_pc( val );				break;
	case MIPS_PC:		mips_set_pc( val );				break;
	case REG_SP:		/* no stack */					break;
	case MIPS_DELAYV:	mipscpu.delayv = val;			break;
	case MIPS_DELAYR:	if( val <= REGPC ) mipscpu.delayr = val; break;
	case MIPS_HI:		mipscpu.hi = val;				break;
	case MIPS_LO:		mipscpu.lo = val;				break;
	case MIPS_R0:		mipscpu.r[ 0 ] = val;			break;
	case MIPS_R1:		mipscpu.r[ 1 ] = val;			break;
	case MIPS_R2:		mipscpu.r[ 2 ] = val;			break;
	case MIPS_R3:		mipscpu.r[ 3 ] = val;			break;
	case MIPS_R4:		mipscpu.r[ 4 ] = val;			break;
	case MIPS_R5:		mipscpu.r[ 5 ] = val;			break;
	case MIPS_R6:		mipscpu.r[ 6 ] = val;			break;
	case MIPS_R7:		mipscpu.r[ 7 ] = val;			break;
	case MIPS_R8:		mipscpu.r[ 8 ] = val;			break;
	case MIPS_R9:		mipscpu.r[ 9 ] = val;			break;
	case MIPS_R10:		mipscpu.r[ 10 ] = val;			break;
	case MIPS_R11:		mipscpu.r[ 11 ] = val;			break;
	case MIPS_R12:		mipscpu.r[ 12 ] = val;			break;
	case MIPS_R13:		mipscpu.r[ 13 ] = val;			break;
	case MIPS_R14:		mipscpu.r[ 14 ] = val;			break;
	case MIPS_R15:		mipscpu.r[ 15 ] = val;			break;
	case MIPS_R16:		mipscpu.r[ 16 ] = val;			break;
	case MIPS_R17:		mipscpu.r[ 17 ] = val;			break;
	case MIPS_R18:		mipscpu.r[ 18 ] = val;			break;
	case MIPS_R19:		mipscpu.r[ 19 ] = val;			break;
	case MIPS_R20:		mipscpu.r[ 20 ] = val;			break;
	case MIPS_R21:		mipscpu.r[ 21 ] = val;			break;
	case MIPS_R22:		mipscpu.r[ 22 ] = val;			break;
	case MIPS_R23:		mipscpu.r[ 23 ] = val;			break;
	case MIPS_R24:		mipscpu.r[ 24 ] = val;			break;
	case MIPS_R25:		mipscpu.r[ 25 ] = val;			break;
	case MIPS_R26:		mipscpu.r[ 26 ] = val;			break;
	case MIPS_R27:		mipscpu.r[ 27 ] = val;			break;
	case MIPS_R28:		mipscpu.r[ 28 ] = val;			break;
	case MIPS_R29:		mipscpu.r[ 29 ] = val;			break;
	case MIPS_R30:		mipscpu.r[ 30 ] = val;			break;
	case MIPS_R31:		mipscpu.r[ 31 ] = val;			break;
	case MIPS_CP0R0:	mips_set_cp0r( 0, val );		break;
	case MIPS_CP0R1:	mips_set_cp0r( 1, val );		break;
	case MIPS_CP0R2:	mips_set_cp0r( 2, val );		break;
	case MIPS_CP0R3:	mips_set_cp0r( 3, val );		break;
	case MIPS_CP0R4:	mips_set_cp0r( 4, val );		break;
	case MIPS_CP0R5:	mips_set_cp0r( 5, val );		break;
	case MIPS_CP0R6:	mips_set_cp0r( 6, val );		break;
	case MIPS_CP0R7:	mips_set_cp0r( 7, val );		break;
	case MIPS_CP0R8:	mips_set_cp0r( 8, val );		break;
	case MIPS_CP0R9:	mips_set_cp0r( 9, val );		break;
	case MIPS_CP0R10:	mips_set_cp0r( 10, val );		break;
	case MIPS_CP0R11:	mips_set_cp0r( 11, val );		break;
	case MIPS_CP0R12:	mips_set_cp0r( 12, val );		break;
	case MIPS_CP0R13:	mips_set_cp0r( 13, val );		break;
	case MIPS_CP0R14:	mips_set_cp0r( 14, val );		break;
	case MIPS_CP0R15:	mips_set_cp0r( 15, val );		break;
	case MIPS_CP0R16:	mips_set_cp0r( 16, val );		break;
	case MIPS_CP0R17:	mips_set_cp0r( 17, val );		break;
	case MIPS_CP0R18:	mips_set_cp0r( 18, val );		break;
	case MIPS_CP0R19:	mips_set_cp0r( 19, val );		break;
	case MIPS_CP0R20:	mips_set_cp0r( 20, val );		break;
	case MIPS_CP0R21:	mips_set_cp0r( 21, val );		break;
	case MIPS_CP0R22:	mips_set_cp0r( 22, val );		break;
	case MIPS_CP0R23:	mips_set_cp0r( 23, val );		break;
	case MIPS_CP0R24:	mips_set_cp0r( 24, val );		break;
	case MIPS_CP0R25:	mips_set_cp0r( 25, val );		break;
	case MIPS_CP0R26:	mips_set_cp0r( 26, val );		break;
	case MIPS_CP0R27:	mips_set_cp0r( 27, val );		break;
	case MIPS_CP0R28:	mips_set_cp0r( 28, val );		break;
	case MIPS_CP0R29:	mips_set_cp0r( 29, val );		break;
	case MIPS_CP0R30:	mips_set_cp0r( 30, val );		break;
	case MIPS_CP0R31:	mips_set_cp0r( 31, val );		break;
	case MIPS_CP2DR0:	mipscpu.cp2dr[ 0 ].d = val;		break;
	case MIPS_CP2DR1:	mipscpu.cp2dr[ 1 ].d = val;		break;
	case MIPS_CP2DR2:	mipscpu.cp2dr[ 2 ].d = val;		break;
	case MIPS_CP2DR3:	mipscpu.cp2dr[ 3 ].d = val;		break;
	case MIPS_CP2DR4:	mipscpu.cp2dr[ 4 ].d = val;		break;
	case MIPS_CP2DR5:	mipscpu.cp2dr[ 5 ].d = val;		break;
	case MIPS_CP2DR6:	mipscpu.cp2dr[ 6 ].d = val;		break;
	case MIPS_CP2DR7:	mipscpu.cp2dr[ 7 ].d = val;		break;
	case MIPS_CP2DR8:	mipscpu.cp2dr[ 8 ].d = val;		break;
	case MIPS_CP2DR9:	mipscpu.cp2dr[ 9 ].d = val;		break;
	case MIPS_CP2DR10:	mipscpu.cp2dr[ 10 ].d = val;	break;
	case MIPS_CP2DR11:	mipscpu.cp2dr[ 11 ].d = val;	break;
	case MIPS_CP2DR12:	mipscpu.cp2dr[ 12 ].d = val;	break;
	case MIPS_CP2DR13:	mipscpu.cp2dr[ 13 ].d = val;	break;
	case MIPS_CP2DR14:	mipscpu.cp2dr[ 14 ].d = val;	break;
	case MIPS_CP2DR15:	mipscpu.cp2dr[ 15 ].d = val;	break;
	case MIPS_CP2DR16:	mipscpu.cp2dr[ 16 ].d = val;	break;
	case MIPS_CP2DR17:	mipscpu.cp2dr[ 17 ].d = val;	break;
	case MIPS_CP2DR18:	mipscpu.cp2dr[ 18 ].d = val;	break;
	case MIPS_CP2DR19:	mipscpu.cp2dr[ 19 ].d = val;	break;
	case MIPS_CP2DR20:	mipscpu.cp2dr[ 20 ].d = val;	break;
	case MIPS_CP2DR21:	mipscpu.cp2dr[ 21 ].d = val;	break;
	case MIPS_CP2DR22:	mipscpu.cp2dr[ 22 ].d = val;	break;
	case MIPS_CP2DR23:	mipscpu.cp2dr[ 23 ].d = val;	break;
	case MIPS_CP2DR24:	mipscpu.cp2dr[ 24 ].d = val;	break;
	case MIPS_CP2DR25:	mipscpu.cp2dr[ 25 ].d = val;	break;
	case MIPS_CP2DR26:	mipscpu.cp2dr[ 26 ].d = val;	break;
	case MIPS_CP2DR27:	mipscpu.cp2dr[ 27 ].d = val;	break;
	case MIPS_CP2DR28:	mipscpu.cp2dr[ 28 ].d = val;	break;
	case MIPS_CP2DR29:	mipscpu.cp2dr[ 29 ].d = val;	break;
	case MIPS_CP2DR30:	mipscpu.cp2dr[ 30 ].d = val;	break;
	case MIPS_CP2DR31:	mipscpu.cp2dr[ 31 ].d = val;	break;
	case MIPS_CP2CR0:	mipscpu.cp2cr[ 0 ].d = val;		break;
	case MIPS_CP2CR1:	mipscpu.cp2cr[ 1 ].d = val;		break;
	case MIPS_CP2CR2:	mipscpu.cp2cr[ 2 ].d = val;		break;
	case MIPS_CP2CR3:	mipscpu.cp2cr[ 3 ].d = val;		break;
	case MIPS_CP2CR4:	mipscpu.cp2cr[ 4 ].d = val;		break;
	case MIPS_CP2CR5:	mipscpu.cp2cr[ 5 ].d = val;		break;
	case MIPS_CP2CR6:	mipscpu.cp2cr[ 6 ].d = val;		break;
	case MIPS_CP2CR7:	mipscpu.cp2cr[ 7 ].d = val;		break;
	case MIPS_CP2CR8:	mipscpu.cp2cr[ 8 ].d = val;		break;
	case MIPS_CP2CR9:	mipscpu.cp2cr[ 9 ].d = val;		break;
	case MIPS_CP2CR10:	mipscpu.cp2cr[ 10 ].d = val;	break;
	case MIPS_CP2CR11:	mipscpu.cp2cr[ 11 ].d = val;	break;
	case MIPS_CP2CR12:	mipscpu.cp2cr[ 12 ].d = val;	break;
	case MIPS_CP2CR13:	mipscpu.cp2cr[ 13 ].d = val;	break;
	case MIPS_CP2CR14:	mipscpu.cp2cr[ 14 ].d = val;	break;
	case MIPS_CP2CR15:	mipscpu.cp2cr[ 15 ].d = val;	break;
	case MIPS_CP2CR16:	mipscpu.cp2cr[ 16 ].d = val;	break;
	case MIPS_CP2CR17:	mipscpu.cp2cr[ 17 ].d = val;	break;
	case MIPS_CP2CR18:	mipscpu.cp2cr[ 18 ].d = val;	break;
	case MIPS_CP2CR19:	mipscpu.cp2cr[ 19 ].d = val;	break;
	case MIPS_CP2CR20:	mipscpu.cp2cr[ 20 ].d = val;	break;
	case MIPS_CP2CR21:	mipscpu.cp2cr[ 21 ].d = val;	break;
	case MIPS_CP2CR22:	mipscpu.cp2cr[ 22 ].d = val;	break;
	case MIPS_CP2CR23:	mipscpu.cp2cr[ 23 ].d = val;	break;
	case MIPS_CP2CR24:	mipscpu.cp2cr[ 24 ].d = val;	break;
	case MIPS_CP2CR25:	mipscpu.cp2cr[ 25 ].d = val;	break;
	case MIPS_CP2CR26:	mipscpu.cp2cr[ 26 ].d = val;	break;
	case MIPS_CP2CR27:	mipscpu.cp2cr[ 27 ].d = val;	break;
	case MIPS_CP2CR28:	mipscpu.cp2cr[ 28 ].d = val;	break;
	case MIPS_CP2CR29:	mipscpu.cp2cr[ 29 ].d = val;	break;
	case MIPS_CP2CR30:	mipscpu.cp2cr[ 30 ].d = val;	break;
	case MIPS_CP2CR31:	mipscpu.cp2cr[ 31 ].d = val;	break;
	}
}

void mips_set_nmi_line( int state )
{
	/* no nmi */
}

void mips_set_irq_line( int irqline, int state )
{
	UINT32 ip;

	switch( irqline )
	{
	case 0:
		ip = CAUSE_IP2;
		break;
	case 1:
		ip = CAUSE_IP3;
		break;
	case 2:
		ip = CAUSE_IP4;
		break;
	case 3:
		ip = CAUSE_IP5;
		break;
	case 4:
		ip = CAUSE_IP6;
		break;
	case 5:
		ip = CAUSE_IP7;
		break;
	default:
		return;
	}

	switch( state )
	{
	case CLEAR_LINE:
		mips_set_cp0r( CP0_CAUSE, mipscpu.cp0r[ CP0_CAUSE ] & ~ip );
		break;
	case ASSERT_LINE:
		mips_set_cp0r( CP0_CAUSE, mipscpu.cp0r[ CP0_CAUSE ] |= ip );
		if( mipscpu.irq_callback )
		{
			/* HOLD_LINE interrupts are not supported by the architecture.
			By acknowledging the interupt here they are treated like PULSE_LINE
			interrupts, so if the interrupt isn't enabled it will be ignored.
			There is also a problem with PULSE_LINE interrupts as the interrupt
			pending bits aren't latched the emulated code won't know what caused
			the interrupt. */
			(*mipscpu.irq_callback)( irqline );
		}
		break;
	}
}

void mips_set_irq_callback( int (*callback)(int irqline) )
{
	mipscpu.irq_callback = callback;
}

/****************************************************************************
 * Return a formatted string for a register
 ****************************************************************************/

const char *mips_info( void *context, int regnum )
{
	static char buffer[ 64 ][ 47 + 1 ];
	static int which = 0;
	mips_cpu_context *r = context;

	which++;
	which %= 64;
	buffer[ which ][ 0 ] = '\0';
	if( !context )
	{
		static mips_cpu_context tmp;
		mips_get_context( &tmp );
		r = &tmp;
	}

	switch( regnum )
	{
	case CPU_INFO_REG + MIPS_PC:		sprintf( buffer[ which ], "pc      :%08x", r->pc );		break;
	case CPU_INFO_REG + MIPS_DELAYV:	sprintf( buffer[ which ], "delay   :%08x", r->delayv );			break;
	case CPU_INFO_REG + MIPS_DELAYR:	sprintf( buffer[ which ], "delay %s:%02x", delayn[ r->delayr ], r->delayr ); break;
	case CPU_INFO_REG + MIPS_HI:		sprintf( buffer[ which ], "hi      :%08x", r->hi );		break;
	case CPU_INFO_REG + MIPS_LO:		sprintf( buffer[ which ], "lo      :%08x", r->lo );		break;
	case CPU_INFO_REG + MIPS_R0:		sprintf( buffer[ which ], "zero    :%08x", r->r[ 0 ] );			break;
	case CPU_INFO_REG + MIPS_R1:		sprintf( buffer[ which ], "at      :%08x", r->r[ 1 ] );			break;
	case CPU_INFO_REG + MIPS_R2:		sprintf( buffer[ which ], "v0      :%08x", r->r[ 2 ] );			break;
	case CPU_INFO_REG + MIPS_R3:		sprintf( buffer[ which ], "v1      :%08x", r->r[ 3 ] );			break;
	case CPU_INFO_REG + MIPS_R4:		sprintf( buffer[ which ], "a0      :%08x", r->r[ 4 ] );			break;
	case CPU_INFO_REG + MIPS_R5:		sprintf( buffer[ which ], "a1      :%08x", r->r[ 5 ] );			break;
	case CPU_INFO_REG + MIPS_R6:		sprintf( buffer[ which ], "a2      :%08x", r->r[ 6 ] );			break;
	case CPU_INFO_REG + MIPS_R7:		sprintf( buffer[ which ], "a3      :%08x", r->r[ 7 ] );			break;
	case CPU_INFO_REG + MIPS_R8:		sprintf( buffer[ which ], "t0      :%08x", r->r[ 8 ] );			break;
	case CPU_INFO_REG + MIPS_R9:		sprintf( buffer[ which ], "t1      :%08x", r->r[ 9 ] );			break;
	case CPU_INFO_REG + MIPS_R10:		sprintf( buffer[ which ], "t2      :%08x", r->r[ 10 ] );		break;
	case CPU_INFO_REG + MIPS_R11:		sprintf( buffer[ which ], "t3      :%08x", r->r[ 11 ] );		break;
	case CPU_INFO_REG + MIPS_R12:		sprintf( buffer[ which ], "t4      :%08x", r->r[ 12 ] );		break;
	case CPU_INFO_REG + MIPS_R13:		sprintf( buffer[ which ], "t5      :%08x", r->r[ 13 ] );		break;
	case CPU_INFO_REG + MIPS_R14:		sprintf( buffer[ which ], "t6      :%08x", r->r[ 14 ] );		break;
	case CPU_INFO_REG + MIPS_R15:		sprintf( buffer[ which ], "t7      :%08x", r->r[ 15 ] );		break;
	case CPU_INFO_REG + MIPS_R16:		sprintf( buffer[ which ], "s0      :%08x", r->r[ 16 ] );		break;
	case CPU_INFO_REG + MIPS_R17:		sprintf( buffer[ which ], "s1      :%08x", r->r[ 17 ] );		break;
	case CPU_INFO_REG + MIPS_R18:		sprintf( buffer[ which ], "s2      :%08x", r->r[ 18 ] );		break;
	case CPU_INFO_REG + MIPS_R19:		sprintf( buffer[ which ], "s3      :%08x", r->r[ 19 ] );		break;
	case CPU_INFO_REG + MIPS_R20:		sprintf( buffer[ which ], "s4      :%08x", r->r[ 20 ] );		break;
	case CPU_INFO_REG + MIPS_R21:		sprintf( buffer[ which ], "s5      :%08x", r->r[ 21 ] );		break;
	case CPU_INFO_REG + MIPS_R22:		sprintf( buffer[ which ], "s6      :%08x", r->r[ 22 ] );		break;
	case CPU_INFO_REG + MIPS_R23:		sprintf( buffer[ which ], "s7      :%08x", r->r[ 23 ] );		break;
	case CPU_INFO_REG + MIPS_R24:		sprintf( buffer[ which ], "t8      :%08x", r->r[ 24 ] );		break;
	case CPU_INFO_REG + MIPS_R25:		sprintf( buffer[ which ], "t9      :%08x", r->r[ 25 ] );		break;
	case CPU_INFO_REG + MIPS_R26:		sprintf( buffer[ which ], "k0      :%08x", r->r[ 26 ] );		break;
	case CPU_INFO_REG + MIPS_R27:		sprintf( buffer[ which ], "k1      :%08x", r->r[ 27 ] );		break;
	case CPU_INFO_REG + MIPS_R28:		sprintf( buffer[ which ], "gp      :%08x", r->r[ 28 ] );		break;
	case CPU_INFO_REG + MIPS_R29:		sprintf( buffer[ which ], "sp      :%08x", r->r[ 29 ] );		break;
	case CPU_INFO_REG + MIPS_R30:		sprintf( buffer[ which ], "fp      :%08x", r->r[ 30 ] );		break;
	case CPU_INFO_REG + MIPS_R31:		sprintf( buffer[ which ], "ra      :%08x", r->r[ 31 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R0:		sprintf( buffer[ which ], "Index   :%08x", r->cp0r[ 0 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R1:		sprintf( buffer[ which ], "Random  :%08x", r->cp0r[ 1 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R2:		sprintf( buffer[ which ], "EntryLo :%08x", r->cp0r[ 2 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R3:		sprintf( buffer[ which ], "cp0r3   :%08x", r->cp0r[ 3 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R4:		sprintf( buffer[ which ], "Context :%08x", r->cp0r[ 4 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R5:		sprintf( buffer[ which ], "cp0r5   :%08x", r->cp0r[ 5 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R6:		sprintf( buffer[ which ], "cp0r6   :%08x", r->cp0r[ 6 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R7:		sprintf( buffer[ which ], "cp0r7   :%08x", r->cp0r[ 7 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R8:		sprintf( buffer[ which ], "BadVAddr:%08x", r->cp0r[ 8 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R9:		sprintf( buffer[ which ], "cp0r9   :%08x", r->cp0r[ 9 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R10:	sprintf( buffer[ which ], "EntryHi :%08x", r->cp0r[ 10 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R11:	sprintf( buffer[ which ], "cp0r11  :%08x", r->cp0r[ 11 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R12:	sprintf( buffer[ which ], "SR      :%08x", r->cp0r[ 12 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R13:	sprintf( buffer[ which ], "Cause   :%08x", r->cp0r[ 13 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R14:	sprintf( buffer[ which ], "EPC     :%08x", r->cp0r[ 14 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R15:	sprintf( buffer[ which ], "PRId    :%08x", r->cp0r[ 15 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R16:	sprintf( buffer[ which ], "cp0r16  :%08x", r->cp0r[ 16 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R17:	sprintf( buffer[ which ], "cp0r17  :%08x", r->cp0r[ 17 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R18:	sprintf( buffer[ which ], "cp0r18  :%08x", r->cp0r[ 18 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R19:	sprintf( buffer[ which ], "cp0r19  :%08x", r->cp0r[ 19 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R20:	sprintf( buffer[ which ], "cp0r20  :%08x", r->cp0r[ 20 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R21:	sprintf( buffer[ which ], "cp0r21  :%08x", r->cp0r[ 21 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R22:	sprintf( buffer[ which ], "cp0r22  :%08x", r->cp0r[ 22 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R23:	sprintf( buffer[ which ], "cp0r23  :%08x", r->cp0r[ 23 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R24:	sprintf( buffer[ which ], "cp0r24  :%08x", r->cp0r[ 24 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R25:	sprintf( buffer[ which ], "cp0r25  :%08x", r->cp0r[ 25 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R26:	sprintf( buffer[ which ], "cp0r26  :%08x", r->cp0r[ 26 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R27:	sprintf( buffer[ which ], "cp0r27  :%08x", r->cp0r[ 27 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R28:	sprintf( buffer[ which ], "cp0r28  :%08x", r->cp0r[ 28 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R29:	sprintf( buffer[ which ], "cp0r29  :%08x", r->cp0r[ 29 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R30:	sprintf( buffer[ which ], "cp0r30  :%08x", r->cp0r[ 30 ] );		break;
	case CPU_INFO_REG + MIPS_CP0R31:	sprintf( buffer[ which ], "cp0r31  :%08x", r->cp0r[ 31 ] );		break;
	case CPU_INFO_REG + MIPS_CP2DR0:	sprintf( buffer[ which ], "vxy0    :%08x", r->cp2dr[ 0 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR1:	sprintf( buffer[ which ], "vz0     :%08x", r->cp2dr[ 1 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR2:	sprintf( buffer[ which ], "vxy1    :%08x", r->cp2dr[ 2 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR3:	sprintf( buffer[ which ], "vz1     :%08x", r->cp2dr[ 3 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR4:	sprintf( buffer[ which ], "vxy2    :%08x", r->cp2dr[ 4 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR5:	sprintf( buffer[ which ], "vz2     :%08x", r->cp2dr[ 5 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR6:	sprintf( buffer[ which ], "rgb     :%08x", r->cp2dr[ 6 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR7:	sprintf( buffer[ which ], "otz     :%08x", r->cp2dr[ 7 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR8:	sprintf( buffer[ which ], "ir0     :%08x", r->cp2dr[ 8 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR9:	sprintf( buffer[ which ], "ir1     :%08x", r->cp2dr[ 9 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR10:	sprintf( buffer[ which ], "ir2     :%08x", r->cp2dr[ 10 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR11:	sprintf( buffer[ which ], "ir3     :%08x", r->cp2dr[ 11 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR12:	sprintf( buffer[ which ], "sxy0    :%08x", r->cp2dr[ 12 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR13:	sprintf( buffer[ which ], "sxy1    :%08x", r->cp2dr[ 13 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR14:	sprintf( buffer[ which ], "sxy2    :%08x", r->cp2dr[ 14 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR15:	sprintf( buffer[ which ], "sxyp    :%08x", r->cp2dr[ 15 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR16:	sprintf( buffer[ which ], "sz0     :%08x", r->cp2dr[ 16 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR17:	sprintf( buffer[ which ], "sz1     :%08x", r->cp2dr[ 17 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR18:	sprintf( buffer[ which ], "sz2     :%08x", r->cp2dr[ 18 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR19:	sprintf( buffer[ which ], "sz3     :%08x", r->cp2dr[ 19 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR20:	sprintf( buffer[ which ], "rgb0    :%08x", r->cp2dr[ 20 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR21:	sprintf( buffer[ which ], "rgb1    :%08x", r->cp2dr[ 21 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR22:	sprintf( buffer[ which ], "rgb2    :%08x", r->cp2dr[ 22 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR23:	sprintf( buffer[ which ], "res1    :%08x", r->cp2dr[ 23 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR24:	sprintf( buffer[ which ], "mac0    :%08x", r->cp2dr[ 24 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR25:	sprintf( buffer[ which ], "mac1    :%08x", r->cp2dr[ 25 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR26:	sprintf( buffer[ which ], "mac2    :%08x", r->cp2dr[ 26 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR27:	sprintf( buffer[ which ], "mac3    :%08x", r->cp2dr[ 27 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR28:	sprintf( buffer[ which ], "irgb    :%08x", r->cp2dr[ 28 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR29:	sprintf( buffer[ which ], "orgb    :%08x", r->cp2dr[ 29 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR30:	sprintf( buffer[ which ], "lzcs    :%08x", r->cp2dr[ 30 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2DR31:	sprintf( buffer[ which ], "lzcr    :%08x", r->cp2dr[ 31 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR0:	sprintf( buffer[ which ], "r11r12  :%08x", r->cp2cr[ 0 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR1:	sprintf( buffer[ which ], "r13r21  :%08x", r->cp2cr[ 1 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR2:	sprintf( buffer[ which ], "r22r23  :%08x", r->cp2cr[ 2 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR3:	sprintf( buffer[ which ], "r31r32  :%08x", r->cp2cr[ 3 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR4:	sprintf( buffer[ which ], "r33     :%08x", r->cp2cr[ 4 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR5:	sprintf( buffer[ which ], "trx     :%08x", r->cp2cr[ 5 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR6:	sprintf( buffer[ which ], "try     :%08x", r->cp2cr[ 6 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR7:	sprintf( buffer[ which ], "trz     :%08x", r->cp2cr[ 7 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR8:	sprintf( buffer[ which ], "l11l12  :%08x", r->cp2cr[ 8 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR9:	sprintf( buffer[ which ], "l13l21  :%08x", r->cp2cr[ 9 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR10:	sprintf( buffer[ which ], "l22l23  :%08x", r->cp2cr[ 10 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR11:	sprintf( buffer[ which ], "l31l32  :%08x", r->cp2cr[ 11 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR12:	sprintf( buffer[ which ], "l33     :%08x", r->cp2cr[ 12 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR13:	sprintf( buffer[ which ], "rbk     :%08x", r->cp2cr[ 13 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR14:	sprintf( buffer[ which ], "gbk     :%08x", r->cp2cr[ 14 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR15:	sprintf( buffer[ which ], "bbk     :%08x", r->cp2cr[ 15 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR16:	sprintf( buffer[ which ], "lr1lr2  :%08x", r->cp2cr[ 16 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR17:	sprintf( buffer[ which ], "lr31g1  :%08x", r->cp2cr[ 17 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR18:	sprintf( buffer[ which ], "lg2lg3  :%08x", r->cp2cr[ 18 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR19:	sprintf( buffer[ which ], "lb1lb2  :%08x", r->cp2cr[ 19 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR20:	sprintf( buffer[ which ], "lb3     :%08x", r->cp2cr[ 20 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR21:	sprintf( buffer[ which ], "rfc     :%08x", r->cp2cr[ 21 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR22:	sprintf( buffer[ which ], "gfc     :%08x", r->cp2cr[ 22 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR23:	sprintf( buffer[ which ], "bfc     :%08x", r->cp2cr[ 23 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR24:	sprintf( buffer[ which ], "ofx     :%08x", r->cp2cr[ 24 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR25:	sprintf( buffer[ which ], "ofy     :%08x", r->cp2cr[ 25 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR26:	sprintf( buffer[ which ], "h       :%08x", r->cp2cr[ 26 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR27:	sprintf( buffer[ which ], "dqa     :%08x", r->cp2cr[ 27 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR28:	sprintf( buffer[ which ], "dqb     :%08x", r->cp2cr[ 28 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR29:	sprintf( buffer[ which ], "zsf3    :%08x", r->cp2cr[ 29 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR30:	sprintf( buffer[ which ], "zsf4    :%08x", r->cp2cr[ 30 ].d );	break;
	case CPU_INFO_REG + MIPS_CP2CR31:	sprintf( buffer[ which ], "flag    :%08x", r->cp2cr[ 31 ].d );	break;
	case CPU_INFO_FLAGS:		return "";
	case CPU_INFO_NAME:			return "PSX CPU";
	case CPU_INFO_FAMILY:		return "mipscpu";
	case CPU_INFO_VERSION:		return "1.4";
	case CPU_INFO_FILE:			return __FILE__;
	case CPU_INFO_CREDITS:		return "Copyright 2003 smf";
	case CPU_INFO_REG_LAYOUT:	return (const char*)mips_reg_layout;
	case CPU_INFO_WIN_LAYOUT:	return (const char*)mips_win_layout;
	}
	return buffer[ which ];
}

unsigned mips_dasm( char *buffer, UINT32 pc )
{
	unsigned ret;
	change_pc32ledw( pc );
#ifdef MAME_DEBUG
	ret = DasmMIPS( buffer, pc );
#else
	sprintf( buffer, "$%08x", cpu_readop32( pc ) );
	ret = 4;
#endif
	change_pc32ledw( mipscpu.pc );
	return ret;
}

/* the GTE (current MAME) */
#include "psx_gte.c"

static void docop2( int gteop )
{
	if( !gte_docop2( gteop ) )
	{
		usrintf_showmessage_secs( 1, "unknown GTE op %08x", gteop );
		log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: unknown GTE op %08x\n", mipscpu.pc, gteop );
		mips_stop();
	}
}
