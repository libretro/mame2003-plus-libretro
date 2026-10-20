/***************************************************************************

    Fujitsu MB86235 "TGPx4", from current MAME
    (src/devices/cpu/mb86235/mb86235.cpp and mb86235ops.cpp; copyright
    Angelo Salese, ElSemi, Ville Linde, Matthew Daniels, BSD-3-Clause),
    the interpreter (MAME runs it, its DRC is disabled) in C for MAME 2003.

    A FIFO read on an empty input FIFO stalls: the instruction still runs
    with no ALU operation, no address register update and no PR write
    pointer step, and is executed again when the CPU runs next (the driver
    suspends it until the i960 pushes a value).

***************************************************************************/

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "cpuintrf.h"
#include "state.h"
#include "mamedbg.h"
#include "mb86235.h"

#define AD  0x00000001
#define AU  0x00000002
#define AV  0x00000004
#define AZ  0x00000008
#define AN  0x00000010
#define ZC  0x00000100
#define IL  0x00000200
#define NR  0x00000400
#define ZD  0x00000800
#define RP  0x00004000
#define LP  0x00008000
#define MD  0x00010000
#define MU  0x00020000
#define MV  0x00040000
#define MZ  0x00080000
#define MN  0x00100000
#define OFF 0x01000000
#define OFE 0x02000000
#define IFF 0x04000000
#define IFE 0x08000000
#define F0  0x10000000
#define F1  0x20000000
#define F2  0x40000000

typedef struct
{
	UINT32 pc, delay_pc, ppc;
	UINT32 aa[ 8 ], ab[ 8 ], ma[ 8 ], mb[ 8 ], ar[ 8 ];
	UINT32 sp, eb, eo, rpc, lpc;
	UINT32 prp, pwp, pr[ 24 ];
	UINT32 mod, st;
	UINT32 pcp, pcs[ 4 ];
	UINT32 pdr, ddr;
	int delay_slot;
	UINT32 fifo_pc;
	int has_stalled;
	UINT32 ram_a[ 0x400 ], ram_b[ 0x400 ];
	struct mb86235_config *cfg;
} mb86235_regs;

static mb86235_regs m;
int mb86235_icount;

#define FSET( f )	( m.st |= ( f ) )
#define FCLR( f )	( m.st &= ~( f ) )
#define BIT64( x, n )	( (int)( ( ( x ) >> ( n ) ) & 1 ) )

static INLINE float u2f( UINT32 v ) { union { float f; UINT32 u; } x; x.u = v; return x.f; }
static INLINE UINT32 f2u( float v ) { union { float f; UINT32 u; } x; x.f = v; return x.u; }

static void fatal_op( const char *what, UINT32 v )
{
	logerror( "TGPx4: illegal %s %02x at pc=%03x\n", what, v, m.ppc );
}


/***************************************************************************
    Helpers
***************************************************************************/

static void handle_single_step_execution( void )
{
	if( m.has_stalled )
		return;

	if( m.st & RP )		/* repeat opcode */
	{
		--m.rpc;
		if( m.rpc == 1 )
			FCLR( RP );
	}
	else
		m.pc++;
}

static int check_previous_op_stall( void )
{
	return m.has_stalled && ( m.st & RP ) == 0;
}

static void increment_pwp( void )
{
	m.pwp++;
	if( m.pwp >= 24 )
		m.pwp = 0;
}

static void increment_prp( void )
{
	m.prp++;
	if( m.prp >= 24 )
		m.prp = 0;
}

static void decrement_prp( void )
{
	if( m.prp == 0 )
		m.prp = 24;
	m.prp--;
}

static UINT32 ext_r( UINT32 a )				{ return m.cfg->ext_r( a & 0xffffff ); }
static void ext_w( UINT32 a, UINT32 d )		{ m.cfg->ext_w( a & 0xffffff, d ); }

static UINT32 decode_ea( UINT8 mode, UINT8 rx, UINT8 ry, UINT16 disp, int isbbus )
{
	UINT32 res;

	switch( mode )
	{
	case 0x00:	/* ARx */
		return m.ar[ rx ];
	case 0x01:	/* ARx++ */
		res = m.ar[ rx ];
		if( m.has_stalled ) return res;
		m.ar[ rx ] = ( m.ar[ rx ] + 1 ) & 0x3fff;
		return res;
	case 0x02:	/* ARx-- */
		res = m.ar[ rx ];
		if( m.has_stalled ) return res;
		m.ar[ rx ] = ( m.ar[ rx ] - 1 ) & 0x3fff;
		return res;
	case 0x03:	/* ARx++ disp14 */
		res = m.ar[ rx ];
		if( m.has_stalled ) return res;
		m.ar[ rx ] = ( m.ar[ rx ] + disp ) & 0x3fff;
		return res;
	case 0x04:	/* ARx + ARy */
		return m.ar[ rx ] + m.ar[ ry ];
	case 0x05:	/* ARx + ARy++ */
		res = m.ar[ ry ];
		if( m.has_stalled ) return m.ar[ rx ] + res;
		m.ar[ ry ] = ( m.ar[ ry ] + 1 ) & 0x3fff;
		return m.ar[ rx ] + res;
	case 0x06:	/* ARx + ARy-- */
		res = m.ar[ ry ];
		if( m.has_stalled ) return m.ar[ rx ] + res;
		m.ar[ ry ] = ( m.ar[ ry ] - 1 ) & 0x3fff;
		return m.ar[ rx ] + res;
	case 0x07:	/* ARx + (ARy++ disp14) */
		res = m.ar[ ry ];
		if( m.has_stalled ) return m.ar[ rx ] + res;
		m.ar[ ry ] = ( m.ar[ ry ] + disp ) & 0x3fff;
		return m.ar[ rx ] + res;
	case 0x08:	/* ARx + ARyU/ARyL (A bus/B bus) */
		return m.ar[ rx ] + ( isbbus ? ( m.ar[ ry ] & 0x7f ) : ( m.ar[ ry ] >> 7 ) );
	case 0x09:	/* ARx + ARyL/ARyU (A bus/B bus) */
		return m.ar[ rx ] + ( isbbus ? ( m.ar[ ry ] >> 7 ) : ( m.ar[ ry ] & 0x7f ) );
	case 0x0a:	/* ARx + disp14 */
		return m.ar[ rx ] + disp;
	case 0x0b:	/* ARx + ARy + disp14 */
		return m.ar[ rx ] + m.ar[ ry ] + disp;
	case 0x0d:	/* ARx + [ARy++] */
	case 0x0e:	/* ARx + [ARy--] */
	case 0x0f:	/* ARx + [ARy++ disp14] */
		res = m.ar[ ry ] & ( 0x1ff >> ( 7 - ( isbbus ? ( ( m.mod >> 8 ) & 7 ) : ( ( m.mod >> 12 ) & 7 ) ) ) );
		if( m.has_stalled ) return m.ar[ rx ] + res;
		if( mode == 0x0d )		m.ar[ ry ]++;
		else if( mode == 0x0e )	m.ar[ ry ]--;
		else					m.ar[ ry ] += disp;
		m.ar[ ry ] &= 0x3fff;
		return m.ar[ rx ] + res;
	}

	fatal_op( "decode_ea type", mode );
	return 0;
}

static UINT32 read_abus( UINT32 addr )
{
	if( ( addr & 0x3fff ) >= 0x400 )
		return ext_r( ( addr & 0x3fff ) + ( m.eb & 0xffc000 ) );
	return m.ram_a[ addr & 0x3ff ];
}

static UINT32 read_bbus( UINT32 addr )
{
	return m.ram_b[ addr & 0x3ff ];
}

static void write_abus( UINT32 addr, UINT32 data )
{
	if( ( addr & 0x3fff ) >= 0x400 )
		ext_w( ( addr & 0x3fff ) + ( m.eb & 0xffc000 ), data );
	else
		m.ram_a[ addr & 0x3ff ] = data;
}

static void write_bbus( UINT32 addr, UINT32 data )
{
	m.ram_b[ addr & 0x3ff ] = data;
}


/***************************************************************************
    ALU
***************************************************************************/

static void set_alu_flagsd( UINT32 val )
{
	FCLR( AN|AZ );
	if( val & 0x80000000 ) FSET( AN );
	if( val == 0 ) FSET( AZ );
}

static void set_alu_flagsf( float val )
{
	FCLR( AN|AZ );
	if( val < 0.0F ) FSET( AN );
	if( val == 0.0F ) FSET( AZ );
}

static void set_alu_flagsi( INT32 val )
{
	FCLR( AN|AZ );
	if( val < 0 ) FSET( AN );
	if( val == 0 ) FSET( AZ );
}

static UINT32 get_prx( UINT8 which )
{
	UINT32 res = m.pr[ m.prp ];
	switch( which & 7 )
	{
	case 0: break;
	case 1: increment_prp(); break;
	case 2: decrement_prp(); break;
	case 3: m.prp = 0; break;
	default: fatal_op( "get_prx", which & 7 ); break;
	}
	return res;
}

static UINT32 get_constfloat( UINT8 which )
{
	static const float float_table[ 8 ] = { -1.0F, 0.0F, 0.5F, 1.0F, 1.5F, 2.0F, 3.0F, 5.0F };
	return f2u( float_table[ which & 7 ] );
}

static UINT32 get_constint( UINT8 which )
{
	switch( which & 7 )
	{
	case 0: return 0;
	case 1: return 1;
	case 2: return 0xffffffff;
	}
	fatal_op( "get_constint", which );
	return 0;
}

static UINT32 get_alureg( UINT8 which, int isfloatop )
{
	switch( which >> 3 )
	{
	case 0: return m.aa[ which & 7 ];
	case 1: return m.ab[ which & 7 ];
	case 2: return get_prx( which & 7 );
	case 3: return isfloatop ? get_constfloat( which & 7 ) : get_constint( which & 7 );
	}
	fatal_op( "get_alureg", which );
	return 0;
}

static UINT32 get_mulreg( UINT8 which, int isfloatop )
{
	switch( which >> 3 )
	{
	case 0: return m.ma[ which & 7 ];
	case 1: return m.mb[ which & 7 ];
	case 2: return get_prx( which & 7 );
	case 3: return isfloatop ? get_constfloat( which & 7 ) : get_constint( which & 7 );
	}
	fatal_op( "get_mulreg", which );
	return 0;
}

static void set_alureg( UINT8 which, UINT32 value )
{
	switch( which >> 3 )
	{
	case 0: m.ma[ which & 7 ] = value; break;
	case 1: m.mb[ which & 7 ] = value; break;
	case 2: m.aa[ which & 7 ] = value; break;
	case 3: m.ab[ which & 7 ] = value; break;
	}
}

static INT32 float_to_int( float f )
{
	/* as a C cast where it is defined; clamped outside the int range */
	if( f != f ) return (INT32)0x80000000;
	if( f >= 2147483648.0F ) return 0x7fffffff;
	if( f < -2147483648.0F ) return (INT32)0x80000000;
	return (INT32)f;
}

static void decode_aluop( UINT8 opcode, UINT32 src1, UINT32 src2, UINT8 imm, UINT8 dst_which )
{
	switch( opcode )
	{
	/* floating point ops */
	case 0x00:	/* FADD */
	case 0x01:	/* FADDZ */
	case 0x02:	/* FSUB */
	case 0x03:	/* FSUBZ */
	{
		float f1 = u2f( src1 ), f2 = u2f( src2 ), d;
		if( opcode & 2 )
			d = f2 - f1;
		else
			d = f1 + f2;
		if( opcode & 1 )
		{
			FCLR( ZC );
			if( d < 0.0F )
			{
				FSET( ZC );
				d = 0.0F;
			}
		}
		set_alu_flagsf( d );
		set_alureg( dst_which, f2u( d ) );
		break;
	}

	case 0x04:	/* FCMP */
	case 0x06:	/* FABC */
	{
		float f1 = u2f( src1 ), f2 = u2f( src2 ), d;
		if( opcode & 2 )
			d = (float)fabs( f2 ) - (float)fabs( f1 );
		else
			d = f2 - f1;
		set_alu_flagsf( d );
		break;
	}

	case 0x05:	/* FABS */
	{
		float d = (float)fabs( u2f( src1 ) );
		set_alu_flagsf( d );
		set_alureg( dst_which, f2u( d ) );
		break;
	}

	case 0x07:	/* NOP */
		break;

	case 0x08:	/* FEA */
	case 0x09:	/* FES */
	{
		/* the exponent field changes, sign and mantissa stay (MAME keeps
		   only the exponent field, dropping them) */
		UINT32 e = ( src1 >> 23 ) & 0xff;
		src1 &= ~0x7f800000;
		if( opcode & 1 )
			e -= imm;
		else
			e += imm;
		e &= 0xff;
		src1 |= e << 23;
		set_alu_flagsd( src1 );
		set_alureg( dst_which, src1 );
		break;
	}

	case 0x0a:	/* FRCP */
	{
		float f = u2f( src1 );
		FCLR( ZD );
		if( f == 0.0F )
			FSET( ZD );
		f = 1.0F / f;
		set_alu_flagsf( f );
		set_alureg( dst_which, f2u( f ) );
		break;
	}

	case 0x0b:	/* FRSQ */
	{
		float f = u2f( src1 );
		FCLR( NR );
		if( f <= 0.0F )
			FSET( NR );
		f = 1.0F / (float)sqrt( f );
		set_alu_flagsf( f );
		set_alureg( dst_which, f2u( f ) );
		break;
	}

	case 0x0c:	/* FLOG */
	{
		float f = u2f( src1 );
		FCLR( IL );
		if( f <= 0.0F )
			FSET( IL );
		f = (float)log( f ) / 0.301030F;
		set_alu_flagsf( f );
		set_alureg( dst_which, f2u( f ) );
		break;
	}

	case 0x0d:	/* CIF */
	{
		float f = (float)(INT32)src1;
		set_alu_flagsf( f );
		set_alureg( dst_which, f2u( f ) );
		break;
	}

	case 0x0e:	/* CFI */
	{
		INT32 v = float_to_int( u2f( src1 ) );
		set_alu_flagsi( v );
		set_alureg( dst_which, (UINT32)v );
		break;
	}

	case 0x0f:	/* CFIB */
	{
		float f = u2f( src1 );
		UINT32 res;
		if( f < 0.0F )
		{
			FSET( AU );
			res = 0;
		}
		else if( f >= 4294967296.0F )
			res = 0xffffffff;
		else
			res = (UINT32)f;
		FCLR( AZ );
		if( res == 0 )
			FSET( AZ );
		if( res > 0xff )
		{
			FSET( AV );
			res = 0xff;
		}
		set_alureg( dst_which, res );
		break;
	}

	/* integer ops */
	case 0x10:	/* ADD */
	case 0x11:	/* ADDZ */
	case 0x12:	/* SUB */
	case 0x13:	/* SUBZ */
	{
		INT32 res;
		if( opcode & 2 )
			res = (INT32)( src2 - src1 );
		else
			res = (INT32)( src1 + src2 );
		if( opcode & 1 )
		{
			FCLR( ZC );
			if( res < 0 )
			{
				FSET( ZC );
				res = 0;
			}
		}
		set_alu_flagsi( res );
		set_alureg( dst_which, (UINT32)res );
		break;
	}

	case 0x14:	/* CMP */
		set_alu_flagsi( (INT32)( src2 - src1 ) );
		break;

	case 0x15:	/* ABS */
		src1 &= 0x7fffffff;
		set_alu_flagsd( src1 );
		set_alureg( dst_which, src1 );
		break;

	case 0x16:	/* ATR */
	case 0x17:	/* ATRZ */
		if( opcode & 1 )
		{
			FCLR( ZC );
			if( src1 & 0x80000000 )
			{
				FSET( ZC );
				src1 = 0;
			}
		}
		set_alureg( dst_which, src1 );
		break;

	/* logical ops */
	case 0x18:	/* AND */
		set_alu_flagsd( src1 & src2 );
		set_alureg( dst_which, src1 & src2 );
		break;
	case 0x19:	/* OR */
		set_alu_flagsd( src1 | src2 );
		set_alureg( dst_which, src1 | src2 );
		break;
	case 0x1a:	/* XOR */
		set_alu_flagsd( src1 ^ src2 );
		set_alureg( dst_which, src1 ^ src2 );
		break;
	case 0x1b:	/* NOT */
		set_alu_flagsd( ~src1 );
		set_alureg( dst_which, ~src1 );
		break;
	case 0x1c:	/* LSR */
	{
		UINT32 res = imm >= 32 ? 0 : src1 >> imm;
		set_alu_flagsd( res );
		set_alureg( dst_which, res );
		break;
	}
	case 0x1d:	/* LSL */
	{
		UINT32 res = imm >= 32 ? 0 : src1 << imm;
		set_alu_flagsd( res );
		set_alureg( dst_which, res );
		break;
	}
	case 0x1e:	/* ASR */
	{
		INT32 res = (INT32)src1 >> ( imm >= 32 ? 31 : imm );
		set_alu_flagsi( res );
		set_alureg( dst_which, (UINT32)res );
		break;
	}
	case 0x1f:	/* ASL */
	{
		INT32 res = (INT32)( imm >= 32 ? 0 : src1 << imm );
		set_alu_flagsi( res );
		set_alureg( dst_which, (UINT32)res );
		break;
	}

	default:
		fatal_op( "decode_aluop", opcode );
		break;
	}
}

static void decode_mulop( int isfmul, UINT32 src1, UINT32 src2, UINT8 dst_which )
{
	if( isfmul )	/* FMUL */
	{
		float res = u2f( src1 ) * u2f( src2 );
		FCLR( MN|MZ|MD );	/* MV and MU are not reset */
		if( res < 0.0F ) FSET( MN );
		if( res == 0.0F ) FSET( MZ );
		if( res == res && ( res - res ) != ( res - res ) ) FSET( MV );	/* infinite */
		if( fabs( res ) < FLT_MIN ) FSET( MU );
		if( res != res ) FSET( MD );	/* NaN */
		set_alureg( dst_which, f2u( res ) );
	}
	else	/* MUL */
	{
		INT32 res = (INT32)( src1 * src2 );
		FCLR( MN|MZ );
		if( res < 0 ) FSET( MN );
		if( res == 0 ) FSET( MZ );
		set_alureg( dst_which, (UINT32)res );
	}
}

#define GETAOP( x )		( (UINT8)( ( ( x ) >> 56 ) & 0x1f ) )
#define GETAI1( x )		( (UINT8)( ( ( x ) >> 52 ) & 0x0f ) )
#define GETAI2( x )		( (UINT8)( ( ( x ) >> 47 ) & 0x1f ) )
#define GETAO( x )		( (UINT8)( ( ( x ) >> 42 ) & 0x1f ) )

#define GETMOP( x )		( (UINT8)( ( ( x ) >> 41 ) & 0x01 ) )
#define GETMI1( x )		( (UINT8)( ( ( x ) >> 37 ) & 0x0f ) )
#define GETMI2( x )		( (UINT8)( ( ( x ) >> 32 ) & 0x1f ) )
#define GETMO( x )		( (UINT8)( ( ( x ) >> 27 ) & 0x1f ) )

static int get_alu_second_src( UINT8 which )
{
	if( ( which & 0x1c ) == 0x1c ) return 0;	/* logical ops */
	if( ( which & 0x1e ) == 0x16 ) return 0;	/* ATRx */
	if( ( which & 0x0f ) == 0x05 ) return 0;	/* ABS/FABS */
	if( ( which & 0x18 ) == 0x08 ) return 0;	/* floating point ops */
	return 1;
}

static void do_alu1( UINT64 op )
{
	if( m.has_stalled )
		return;

	if( BIT64( op, 41 ) )	/* ALU */
	{
		UINT8 aluop = GETAOP( op );
		UINT32 alusrc1 = get_alureg( GETAI1( op ), 0 );
		UINT32 alusrc2 = get_alu_second_src( aluop ) ? get_alureg( GETAI2( op ), ( aluop & 0x10 ) == 0 ) : 0;
		decode_aluop( aluop, alusrc1, alusrc2, GETAI2( op ), GETAO( op ) );
	}
	else	/* MUL */
	{
		int isfmul = GETAOP( op ) != 0;
		UINT32 mulsrc1 = get_mulreg( GETAI1( op ), 0 );
		UINT32 mulsrc2 = get_mulreg( GETAI2( op ), isfmul );
		decode_mulop( isfmul, mulsrc1, mulsrc2, GETAO( op ) );
	}
}

static void do_alu2( UINT64 op )
{
	UINT8 aluop;
	UINT32 alusrc1, alusrc2, mulsrc1, mulsrc2;
	int isfmul;

	if( m.has_stalled )
		return;

	aluop = GETAOP( op );
	alusrc1 = get_alureg( GETAI1( op ), 0 );
	alusrc2 = get_alu_second_src( aluop ) ? get_alureg( GETAI2( op ), ( aluop & 0x10 ) == 0 ) : 0;

	isfmul = GETMOP( op ) != 0;
	mulsrc1 = get_mulreg( GETMI1( op ), 0 );
	mulsrc2 = get_mulreg( GETMI2( op ), isfmul );

	decode_aluop( aluop, alusrc1, alusrc2, GETAI2( op ), GETAO( op ) );
	decode_mulop( isfmul, mulsrc1, mulsrc2, GETMO( op ) );
}


/***************************************************************************
    Transfers
***************************************************************************/

static UINT32 get_transfer_reg( UINT8 which )
{
	switch( which >> 3 )
	{
	case 0: return m.ma[ which & 7 ];
	case 1: return m.aa[ which & 7 ];
	case 2:
		switch( which & 7 )
		{
		case 0: return m.eb;
		case 1: return m.eb >> 14;
		case 2: return m.eb & 0x3fff;
		case 3: return m.eo;
		case 4: return m.sp;
		case 5: return m.st;
		case 6: return m.mod;
		case 7: return m.lpc;
		}
		break;
	case 3: return m.ar[ which & 7 ];
	case 4: return m.mb[ which & 7 ];
	case 5: return m.ab[ which & 7 ];
	case 6:
		switch( which & 7 )
		{
		case 0:	/* PRx */
			return m.pr[ m.prp ];
		case 1:	/* FI */
		{
			UINT32 res = m.cfg->fifoin_r();
			if( m.has_stalled && ( m.st & RP ) == 0 )
				m.fifo_pc = m.ppc;
			return res;
		}
		case 4: return m.pdr;
		case 5: return m.ddr;
		case 6: return m.prp;
		case 7: return m.pwp;
		}
		break;
	}

	fatal_op( "get_transfer_reg", which );
	return 0;
}

static void set_transfer_reg( UINT8 which, UINT32 value )
{
	switch( which >> 3 )
	{
	case 0: m.ma[ which & 7 ] = value; break;
	case 1: m.aa[ which & 7 ] = value; break;
	case 2:
		switch( which & 7 )
		{
		case 0: m.eb = value; break;
		case 1: m.eb = ( m.eb & 0x3fff ) | ( value << 14 ); break;
		/* EBL, the low 14 bits (as read above; MAME shifts the value
		   up as for EBU here, a copy and paste slip) */
		case 2: m.eb = ( m.eb & 0xffc000 ) | ( value & 0x3fff ); break;
		case 3: m.eo = value; break;
		case 4: m.sp = value; break;
		case 5: m.st = value; break;
		case 6: m.mod = value; break;
		case 7: m.lpc = value; break;
		}
		break;
	case 3: m.ar[ which & 7 ] = value & 0x3fff; break;
	case 4: m.mb[ which & 7 ] = value; break;
	case 5: m.ab[ which & 7 ] = value; break;
	case 6:
		switch( which & 7 )
		{
		case 0:
			m.pr[ m.pwp ] = value;
			if( !m.has_stalled )
				increment_pwp();
			break;
		case 2:	/* FO0 */
		case 3:	/* FO1 (sets the 33rd bit; not used by Model 2) */
			m.cfg->fifoout_w( value );
			break;
		case 4: m.pdr = value; break;
		case 5: m.ddr = value; break;
		case 6:
			if( value >= 24 ) fatal_op( "prp value", value );
			else m.prp = value;
			break;
		case 7:
			if( value >= 24 ) fatal_op( "pwp value", value );
			else m.pwp = value;
			break;
		default:
			fatal_op( "set_transfer_reg", which );
			break;
		}
		break;
	default:
		fatal_op( "set_transfer_reg dst", which );
		break;
	}
}

static INT32 disp6( UINT32 v )
{
	v &= 0x3f;
	return ( v & 0x20 ) ? (INT32)v - 0x40 : (INT32)v;
}

/* double transfer type 1 */
static void do_alu2_trans2_1( UINT64 op )
{
	UINT8 sd = (UINT8)( ( op >> 25 ) & 3 );
	UINT32 ares = 0, bres = 0, addr;

	switch( sd )
	{
	case 0:
	case 1:
		ares = get_transfer_reg( (UINT8)( ( op >> 20 ) & 0x1f ) );
		bres = get_transfer_reg( (UINT8)( ( ( op >> 10 ) & 0xf ) | 0x20 ) );
		break;
	case 2:
		addr = decode_ea( (UINT8)( op & 0xf ), (UINT8)( ( op >> 17 ) & 7 ), (UINT8)( ( op >> 14 ) & 7 ), 0, 0 );
		ares = read_abus( addr );
		addr = decode_ea( (UINT8)( op & 0xf ), (UINT8)( ( op >> 7 ) & 7 ), (UINT8)( ( op >> 4 ) & 7 ), 0, 1 );
		bres = read_bbus( addr );
		break;
	default:
		fatal_op( "trans2_1 SD", sd );
		break;
	}

	do_alu2( op );

	switch( sd )
	{
	case 0:
	case 2:
		set_transfer_reg( (UINT8)( ( op >> 20 ) & 0x1f ), ares );
		set_transfer_reg( (UINT8)( ( ( op >> 10 ) & 0xf ) | 0x20 ), bres );
		break;
	case 1:
		addr = decode_ea( (UINT8)( op & 0xf ), (UINT8)( ( op >> 17 ) & 7 ), (UINT8)( ( op >> 14 ) & 7 ), 0, 0 );
		write_abus( addr, ares );
		addr = decode_ea( (UINT8)( op & 0xf ), (UINT8)( ( op >> 7 ) & 7 ), (UINT8)( ( op >> 4 ) & 7 ), 0, 1 );
		write_bbus( addr, bres );
		break;
	}
}

/* transfer type 1 */
static void do_alu2_trans1_1( UINT64 op )
{
	UINT8 sr, dr;
	UINT32 res, addr;
	int isbbus;

	if( BIT64( op, 26 ) )	/* external transfer */
	{
		if( BIT64( op, 25 ) )	/* ext -> int */
		{
			res = ext_r( m.eb + m.eo );

			do_alu2( op );

			dr = (UINT8)( ( op >> 12 ) & 0x7f );
			if( dr & 0x40 )
			{
				isbbus = ( dr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), dr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x1f ), isbbus ) & 0x3ff;
				if( isbbus )
					write_bbus( addr, res );
				else
					write_abus( addr, res );
			}
			else
				set_transfer_reg( dr, res );

			m.eo += disp6( (UINT32)( op >> 19 ) );
		}
		else	/* int -> ext */
		{
			sr = (UINT8)( ( op >> 12 ) & 0x7f );
			if( sr & 0x40 )
			{
				isbbus = ( sr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), sr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x1f ), isbbus ) & 0x3ff;
				res = isbbus ? read_bbus( addr ) : read_abus( addr );
			}
			else
				res = get_transfer_reg( sr );

			do_alu2( op );

			ext_w( m.eb + m.eo, res );
			m.eo += disp6( (UINT32)( op >> 19 ) );
		}
	}
	else
	{
		sr = (UINT8)( ( op >> 19 ) & 0x7f );
		if( sr & 0x40 )
		{
			if( sr == 0x58 )
				res = (UINT32)( op & 0xfff );
			else
			{
				isbbus = ( sr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), sr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x1f ), isbbus );
				res = isbbus ? read_bbus( addr ) : read_abus( addr );
			}
		}
		else
			res = get_transfer_reg( sr );

		do_alu2( op );

		dr = (UINT8)( ( op >> 12 ) & 0x7f );
		if( dr & 0x40 )
		{
			if( dr == 0x58 )
				fatal_op( "do_alu2_trans1_1 dr", dr );
			else
			{
				isbbus = ( dr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), dr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x1f ), isbbus );
				if( isbbus )
					write_bbus( addr, res );
				else
					write_abus( addr, res );
			}
		}
		else
			set_transfer_reg( dr, res );
	}
}

/* double transfer type 2 */
static void do_alu1_trans2_2( UINT64 op )
{
	UINT8 sda = (UINT8)( ( op >> 38 ) & 3 );
	UINT8 sdb = (UINT8)( ( op >> 18 ) & 3 );
	UINT32 ares = 0, bres = 0, addr;

	switch( sda )	/* A bus read */
	{
	case 0:
	case 1:
		ares = get_transfer_reg( (UINT8)( ( op >> 33 ) & 0x1f ) );
		break;
	case 2:
		addr = decode_ea( (UINT8)( ( op >> 20 ) & 0xf ), (UINT8)( ( op >> 30 ) & 7 ), (UINT8)( ( op >> 27 ) & 7 ), (UINT16)( ( op >> 24 ) & 7 ), 0 );
		ares = read_abus( addr );
		break;
	default:
		fatal_op( "trans2_2 SDA", sda );
		break;
	}

	switch( sdb )	/* B bus read */
	{
	case 0:
	case 1:
		bres = get_transfer_reg( (UINT8)( ( ( op >> 13 ) & 0x1f ) | 0x20 ) );
		break;
	case 2:
		addr = decode_ea( (UINT8)( op & 0xf ), (UINT8)( ( op >> 10 ) & 7 ), (UINT8)( ( op >> 7 ) & 7 ), (UINT16)( ( op >> 4 ) & 7 ), 1 );
		bres = read_bbus( addr );
		break;
	default:
		fatal_op( "trans2_2 SDB", sdb );
		break;
	}

	do_alu1( op );

	switch( sda )	/* A bus write */
	{
	case 0:
	case 2:
		set_transfer_reg( (UINT8)( ( op >> 28 ) & 0x1f ), ares );
		break;
	case 1:
		addr = decode_ea( (UINT8)( ( op >> 20 ) & 0xf ), (UINT8)( ( op >> 30 ) & 7 ), (UINT8)( ( op >> 27 ) & 7 ), (UINT16)( ( op >> 24 ) & 7 ), 0 );
		write_abus( addr, ares );
		break;
	}

	switch( sdb )	/* B bus write */
	{
	case 0:
	case 2:
		set_transfer_reg( (UINT8)( ( ( op >> 8 ) & 0x1f ) | 0x20 ), bres );
		break;
	case 1:
		addr = decode_ea( (UINT8)( op & 0xf ), (UINT8)( ( op >> 10 ) & 7 ), (UINT8)( ( op >> 7 ) & 7 ), (UINT16)( ( op >> 4 ) & 7 ), 1 );
		write_bbus( addr, bres );
		break;
	}
}

/* transfer type 2 */
static void do_alu1_trans1_2( UINT64 op )
{
	UINT8 sr, dr;
	UINT32 res, addr;
	int isbbus;

	if( BIT64( op, 38 ) )	/* external transfer */
	{
		if( BIT64( op, 37 ) )	/* ext -> int */
		{
			res = ext_r( m.eb + m.eo );

			do_alu1( op );

			dr = (UINT8)( ( op >> 24 ) & 0x7f );
			if( dr & 0x40 )
			{
				isbbus = ( dr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), dr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x3fff ), isbbus ) & 0x3ff;
				if( isbbus )
					write_bbus( addr, res );
				else
					write_abus( addr, res );
			}
			else
				set_transfer_reg( dr, res );

			m.eo += disp6( (UINT32)( op >> 31 ) );
		}
		else	/* int -> ext */
		{
			sr = (UINT8)( ( op >> 24 ) & 0x7f );
			if( sr & 0x40 )
			{
				if( sr == 0x58 )
					res = (UINT32)( op & 0xffffff );
				else
				{
					isbbus = ( sr & 0x20 ) == 0x20;
					addr = decode_ea( (UINT8)( op & 0xf ), sr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x3fff ), isbbus ) & 0x3ff;
					res = isbbus ? read_bbus( addr ) : read_abus( addr );
				}
			}
			else
				res = get_transfer_reg( sr );

			do_alu1( op );

			ext_w( m.eb + m.eo, res );
			m.eo += disp6( (UINT32)( op >> 31 ) );
		}
	}
	else
	{
		sr = (UINT8)( ( op >> 31 ) & 0x7f );
		if( sr & 0x40 )
		{
			if( sr == 0x58 )
				res = (UINT32)( op & 0xffffff );
			else
			{
				isbbus = ( sr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), sr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x3fff ), isbbus );
				res = isbbus ? read_bbus( addr ) : read_abus( addr );
			}
		}
		else
			res = get_transfer_reg( sr );

		do_alu1( op );

		dr = (UINT8)( ( op >> 24 ) & 0x7f );
		if( dr & 0x40 )
		{
			if( dr == 0x58 )
				fatal_op( "do_alu1_trans1_2 dr", dr );
			else
			{
				isbbus = ( dr & 0x20 ) == 0x20;
				addr = decode_ea( (UINT8)( op & 0xf ), dr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0x3fff ), isbbus );
				if( isbbus )
					write_bbus( addr, res );
				else
					write_abus( addr, res );
			}
		}
		else
			set_transfer_reg( dr, res );
	}
}

/* transfer type 3 */
static void do_trans1_3( UINT64 op )
{
	UINT8 dr = (UINT8)( ( op >> 19 ) & 0x7f );
	UINT32 imm = (UINT32)( op >> 27 );

	if( dr & 0x40 )
	{
		int isbbus = ( dr & 0x20 ) == 0x20;
		UINT32 addr = decode_ea( (UINT8)( op & 0xf ), dr & 7, (UINT8)( ( op >> 4 ) & 7 ), (UINT16)( ( op >> 7 ) & 0xfff ), isbbus );
		if( isbbus )
			write_bbus( addr, imm );
		else
			write_abus( addr, imm );
	}
	else
		set_transfer_reg( dr, imm );
}


/***************************************************************************
    Control
***************************************************************************/

static void push_pc( UINT32 pcval )
{
	m.pcs[ m.pcp++ ] = pcval;
	m.pcp &= 3;
}

static UINT32 pop_pc( void )
{
	m.pcp = ( m.pcp - 1 ) & 3;
	return m.pcs[ m.pcp ];
}

static UINT32 do_control_dst( UINT64 op )
{
	UINT32 addr = 0;
	int r = (int)( ( op >> 6 ) & 7 );

	switch( ( op >> 13 ) & 7 )
	{
	case 0: addr = (UINT32)( op & 0xfff ); break;	/* immediate */
	case 1: addr = m.ar[ r ]; break;
	case 2: addr = BIT64( op, 11 ) ? m.ab[ r ] : m.aa[ r ]; break;
	case 3: addr = BIT64( op, 11 ) ? m.mb[ r ] : m.ma[ r ]; break;
	case 4: addr = read_abus( (UINT32)( op & 0x3ff ) ); break;
	case 5: addr = read_bbus( (UINT32)( op & 0x3ff ) ); break;
	case 6: addr = read_abus( m.ar[ r ] ); break;
	case 7: addr = read_bbus( m.ar[ r ] ); break;
	}

	if( BIT64( op, 12 ) )
	{
		mb86235_icount--;
		return ( m.pc + addr ) & 0xfff;
	}
	return addr & 0xfff;
}

static void set_mod( UINT16 mod1, UINT16 mod2 )
{
	m.mod &= ~mod1;
	m.mod |= mod2;
}

static int decode_branch_jump( UINT8 which )
{
	static const UINT32 condition_table[ 19 ] =
	{
		MN, MZ, MV, MU, ZD, NR, IL, ZC, AN, AZ, AV, AU, MD, AD, 0, 0, F0, F1, F2
	};

	if( which < 19 )
		return ( m.st & condition_table[ which ] ) != 0;

	switch( which )
	{
	case 20: return m.cfg->fifo_flag( MB86235_FIFO_IFF );
	case 21: return m.cfg->fifo_flag( MB86235_FIFO_IFE );
	case 22: return m.cfg->fifo_flag( MB86235_FIFO_OFF );
	case 23: return m.cfg->fifo_flag( MB86235_FIFO_OFE );
	case 24: return 0;	/* bit 33 of the input FIFO; not used by Model 2 */
	}
	fatal_op( "decode_branch_jump mode", which );
	return 0;
}

static void do_alu_control( UINT64 op )
{
	UINT32 cop = (UINT32)( ( op >> 22 ) & 0x1f );
	UINT8 ef1 = (UINT8)( ( op >> 16 ) & 0x3f );
	UINT16 ef2 = (UINT16)( op & 0xffff );

	switch( cop )
	{
	case 0x00:	/* NOP */
		break;
	case 0x01:	/* REP */
		m.rpc = ( ef1 == 0x3f ) ? m.ar[ ( ef2 >> 13 ) & 7 ] : ef2;
		FSET( RP );
		break;
	case 0x02:	/* SETL */
		m.lpc = ( ef1 == 0x3f ) ? m.ar[ ( ef2 >> 13 ) & 7 ] : ef2;
		FSET( LP );
		break;
	case 0x03:	/* CLRF */
		if( ef1 & 1 ) m.cfg->fifo_clear( 0 );	/* CLRFI */
		if( ef1 & 2 ) m.cfg->fifo_clear( 1 );	/* CLRFO */
		break;
	case 0x04:	/* PUSH */
		m.sp = ( m.sp - 1 ) & 0x3ff;
		m.ram_b[ m.sp ] = get_transfer_reg( (UINT8)( ( ef2 >> 6 ) & 0x3f ) );
		break;
	case 0x05:	/* POP: after the ALU operation(s) */
		break;
	case 0x08: set_mod( 0xffff, ef2 ); break;	/* SETM */
	case 0x09: set_mod( 0x7000, ef2 ); break;	/* SETMCBSA */
	case 0x0a: set_mod( 0x0e00, ef2 ); break;	/* SETMCBSB */
	case 0x0b: set_mod( 0x0080, ef2 ); break;	/* SETMRF */
	case 0x0c: set_mod( 0x0010, ef2 ); break;	/* SETMRDY */
	case 0x0d: set_mod( 0x0007, ef2 ); break;	/* SETMWAIT */

	/* control flow */
	case 0x10:	/* DBcc */
		if( decode_branch_jump( ef1 ) )
		{
			m.delay_slot = 1;
			m.delay_pc = do_control_dst( op );
		}
		mb86235_icount--;
		break;
	case 0x11:	/* DBNcc */
		if( !decode_branch_jump( ef1 ) )
		{
			m.delay_slot = 1;
			m.delay_pc = do_control_dst( op );
		}
		mb86235_icount--;
		break;
	case 0x12:	/* DJMP */
		m.delay_slot = 1;
		m.delay_pc = do_control_dst( op );
		mb86235_icount--;
		break;
	case 0x13:	/* DBLP: relative addressing only */
		if( m.st & LP )
		{
			m.delay_slot = 1;
			m.delay_pc = ( m.pc + (UINT32)( op & 0xfff ) ) & 0xfff;
		}
		--m.lpc;
		if( m.lpc == 1 )
			FCLR( LP );
		break;
	case 0x14:	/* DBBC */
	case 0x15:	/* DBBS */
	{
		int result = ( m.ar[ ( op >> 13 ) & 7 ] & ( 1 << ( ( op >> 16 ) & 0xf ) ) ) != 0;
		if( result == ( cop == 0x15 ) )
		{
			m.delay_slot = 1;
			m.delay_pc = ( m.pc + (UINT32)( op & 0xfff ) ) & 0xfff;
		}
		mb86235_icount -= 2;
		break;
	}
	case 0x18:	/* DCcc */
		if( decode_branch_jump( ef1 ) )
		{
			m.delay_slot = 1;
			m.delay_pc = do_control_dst( op );
			push_pc( m.pc + 1 );
		}
		mb86235_icount--;
		break;
	case 0x19:	/* DCNcc */
		if( !decode_branch_jump( ef1 ) )
		{
			m.delay_slot = 1;
			m.delay_pc = do_control_dst( op );
			push_pc( m.pc + 1 );
		}
		mb86235_icount--;
		break;
	case 0x1a:	/* DCALL */
		m.delay_slot = 1;
		m.delay_pc = do_control_dst( op );
		push_pc( m.pc + 1 );
		break;
	case 0x1b:	/* DRET */
		m.delay_slot = 1;
		m.delay_pc = pop_pc();
		break;
	default:
		fatal_op( "control op", cop );
		break;
	}

	if( BIT64( op, 63 ) )
		do_alu1( op );
	else
		do_alu2( op );

	if( cop == 0x05 )	/* now we can safely pop */
	{
		set_transfer_reg( (UINT8)( ( ef2 >> 6 ) & 0x3f ), m.ram_b[ m.sp & 0x3ff ] );
		m.sp = ( m.sp + 1 ) & 0x3ff;
	}
}

static void execute_op( UINT64 op )
{
	switch( ( op >> 61 ) & 7 )
	{
	case 0: do_alu2_trans2_1( op ); break;
	case 1: do_alu2_trans1_1( op ); break;
	case 4: do_alu1_trans2_2( op ); break;
	case 5: do_alu1_trans1_2( op ); break;
	case 2:
	case 6: do_alu_control( op ); break;
	case 7: do_trans1_3( op ); break;
	default:
		fatal_op( "opcode type", (UINT32)( ( op >> 61 ) & 7 ) );
		break;
	}
}


/***************************************************************************
    CPU interface
***************************************************************************/

int mb86235_execute( int cycles )
{
	mb86235_icount = cycles;

	while( mb86235_icount > 0 )
	{
		UINT32 curpc = check_previous_op_stall() ? m.fifo_pc : m.pc;
		UINT64 opcode;

		m.ppc = curpc;
		CALL_MAME_DEBUG;
		opcode = m.cfg->prog_r( curpc & 0xfff );

		if( m.delay_slot )
		{
			m.pc = m.delay_pc;
			m.delay_slot = 0;
		}
		else
			handle_single_step_execution();

		m.has_stalled = 0;

		execute_op( opcode );

		mb86235_icount--;

		/* waiting for its input FIFO: the driver suspends it, give up the timeslice */
		if( m.has_stalled )
			mb86235_icount = 0;
	}
	return cycles - mb86235_icount;
}

void mb86235_stall( void )
{
	m.has_stalled = 1;
}

void mb86235_init( void )
{
	int cpu = cpu_getactivecpu();

	memset( &m, 0, sizeof( m ) );
	state_save_register_UINT32( "mb86235", cpu, "pc", &m.pc, 1 );
	state_save_register_UINT32( "mb86235", cpu, "delay_pc", &m.delay_pc, 1 );
	state_save_register_UINT32( "mb86235", cpu, "ppc", &m.ppc, 1 );
	state_save_register_UINT32( "mb86235", cpu, "aa", m.aa, 8 );
	state_save_register_UINT32( "mb86235", cpu, "ab", m.ab, 8 );
	state_save_register_UINT32( "mb86235", cpu, "ma", m.ma, 8 );
	state_save_register_UINT32( "mb86235", cpu, "mb", m.mb, 8 );
	state_save_register_UINT32( "mb86235", cpu, "ar", m.ar, 8 );
	state_save_register_UINT32( "mb86235", cpu, "sp", &m.sp, 1 );
	state_save_register_UINT32( "mb86235", cpu, "eb", &m.eb, 1 );
	state_save_register_UINT32( "mb86235", cpu, "eo", &m.eo, 1 );
	state_save_register_UINT32( "mb86235", cpu, "rpc", &m.rpc, 1 );
	state_save_register_UINT32( "mb86235", cpu, "lpc", &m.lpc, 1 );
	state_save_register_UINT32( "mb86235", cpu, "prp", &m.prp, 1 );
	state_save_register_UINT32( "mb86235", cpu, "pwp", &m.pwp, 1 );
	state_save_register_UINT32( "mb86235", cpu, "pr", m.pr, 24 );
	state_save_register_UINT32( "mb86235", cpu, "mod", &m.mod, 1 );
	state_save_register_UINT32( "mb86235", cpu, "st", &m.st, 1 );
	state_save_register_UINT32( "mb86235", cpu, "pcp", &m.pcp, 1 );
	state_save_register_UINT32( "mb86235", cpu, "pcs", m.pcs, 4 );
	state_save_register_UINT32( "mb86235", cpu, "pdr", &m.pdr, 1 );
	state_save_register_UINT32( "mb86235", cpu, "ddr", &m.ddr, 1 );
	state_save_register_int( "mb86235", cpu, "delay_slot", &m.delay_slot );
	state_save_register_UINT32( "mb86235", cpu, "fifo_pc", &m.fifo_pc, 1 );
	state_save_register_int( "mb86235", cpu, "has_stalled", &m.has_stalled );
	state_save_register_UINT32( "mb86235", cpu, "ram_a", m.ram_a, 0x400 );
	state_save_register_UINT32( "mb86235", cpu, "ram_b", m.ram_b, 0x400 );
}

/* as MAME: only the program counter and the delay slot are reset */
void mb86235_reset( void *param )
{
	if( param )
		m.cfg = (struct mb86235_config *)param;
	m.pc = 0;
	m.delay_pc = 0;
	m.ppc = 0;
	m.delay_slot = 0;
	m.has_stalled = 0;
}

void mb86235_exit( void )
{
}

unsigned mb86235_get_context( void *dst )
{
	if( dst )
		*(mb86235_regs *)dst = m;
	return sizeof( mb86235_regs );
}

void mb86235_set_context( void *src )
{
	if( src )
		m = *(mb86235_regs *)src;
}

unsigned mb86235_get_reg( int regnum )
{
	switch( regnum )
	{
	case REG_PC:			return m.pc;
	case REG_PREVIOUSPC:	return m.ppc;
	case REG_SP:			return m.sp;
	}
	return 0;
}

void mb86235_set_reg( int regnum, unsigned val )
{
	switch( regnum )
	{
	case REG_PC:	m.pc = val & 0xfff; break;
	case REG_SP:	m.sp = val & 0x3ff; break;
	}
}

void mb86235_set_irq_line( int irqline, int state )
{
	/* interrupts are not used (as current MAME) */
}

void mb86235_set_irq_callback( int (*callback)(int irqline) )
{
}

const char *mb86235_info( void *context, int regnum )
{
	static char buffer[ 8 ][ 47 + 1 ];
	static int which = 0;
	mb86235_regs *r = context ? (mb86235_regs *)context : &m;

	which = ( which + 1 ) % 8;
	buffer[ which ][ 0 ] = '\0';

	switch( regnum )
	{
	case CPU_INFO_REG + 1:	sprintf( buffer[ which ], "PC:%03X", r->pc ); break;
	case CPU_INFO_FLAGS:	sprintf( buffer[ which ], "%08X", r->st ); break;
	case CPU_INFO_NAME:		return "MB86235";
	case CPU_INFO_FAMILY:	return "Fujitsu TGPx4";
	case CPU_INFO_VERSION:	return "1.0";
	case CPU_INFO_FILE:		return __FILE__;
	case CPU_INFO_CREDITS:	return "Angelo Salese, ElSemi, Ville Linde, Matthew Daniels (current MAME)";
	}
	return buffer[ which ];
}

unsigned mb86235_dasm( char *buffer, unsigned pc )
{
	UINT64 op = m.cfg ? m.cfg->prog_r( pc & 0xfff ) : 0;
	sprintf( buffer, "$%08x%08x", (UINT32)( op >> 32 ), (UINT32)op );
	return 1;
}
