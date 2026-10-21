/***************************************************************************

    Sega Model 1: TGP (geometrizer) coprocessor.

    From MAME 0.215 (src/mame/machine/model1.cpp, the last version with
    the high level TGP; copyright Olivier Galibert, BSD-3-Clause) and
    current MAME (sega/model1_m.cpp: copro tables and data ROM access),
    in C for MAME 2003.

    The TGP runs in C (HLE): each function runs as soon as the V60 has
    pushed its arguments. For the 315-5573 and 315-5711 games every function
    they call was checked call by call against the real program (see
    docs/sega-model1.md) and gives the same results bit for bit.

    Development builds (-DMODEL1_TGP_DEV) also run the real MB86233 on the
    dumped program, selected with the environment variable MODEL1_TGP:

    - lle: the real TGP only.

    - oracle: the game runs on the real TGP, the HLE gets the same input
      and every call is compared, with the function, its arguments and the
      results of both logged when they differ.

***************************************************************************/

#include <math.h>

/* no fused multiply-add: the TGP rounds every product and sum, and x86 (the
   reference) does not fuse them either. GCC and clang fuse a*b+c by default
   on CPUs that have the instruction (PowerPC, ARM64) */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif
#include "driver.h"
#include "state.h"
#include "cpu/mb86233/mb86233.h"
#include "cpu/v60/v60.h"
#include "includes/model1.h"

#define CPU_MAIN	0

/* C89: sqrtf is C99 (MSVC 2005 has none). The square root of a float done
   in double and rounded to float is the same value */
static float tgp_sqrt( float x )
{
	return (float)sqrt( (double)x );
}

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#if 0
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

#define DS				0x2000		/* copro RAM, 32-bit words */
#define MAT_STACK_SIZE	32

int model1_tgp_mode;		/* MODEL1_TGP_HLE / _LLE / _ORACLE */
int model1_tgp_table;		/* MODEL1_TGP_TABLE_VR, _SWA or _VF */
int model1_tgp_cpu = -1;	/* CPU number of the MB86233, -1 without it */


/*************************************
 *  float <-> bits
 *************************************/

static INLINE UINT32 f2u( float f )
{
	union { float f; UINT32 u; } v;
	v.f = f;
	return v.u;
}

static INLINE float u2f( UINT32 u )
{
	union { float f; UINT32 u; } v;
	v.u = u;
	return v.f;
}


/*************************************
 *  FIFOs between the V60 and the TGP

    As MAME's generic_fifo: a push is never lost, it may go past the
    size and then the writer waits; a pop from an empty FIFO makes the
    reader repeat the access and wait for a push. With the HLE the TGP
    side is synchronous, so only the V60 ever waits.
 *************************************/

#define FIFO_SIZE	16
#define FIFO_ALLOC	512

struct m1_fifo
{
	UINT32 data[ FIFO_ALLOC ];
	int head, count;
	int reader, writer;			/* CPU numbers, -1 for the HLE */
	int reader_waits, writer_waits;
};

static struct m1_fifo fifo_in;		/* V60 -> LLE TGP */
static struct m1_fifo fifo_out;		/* LLE TGP -> V60 */
static struct m1_fifo hle_in;		/* V60 -> HLE TGP */
static struct m1_fifo hle_out;		/* HLE TGP -> V60 (or oracle compare) */

static void fifo_clear( struct m1_fifo *f )
{
	f->head = f->count = 0;
	if( f->reader_waits && f->reader >= 0 )
		cpunum_resume( f->reader, SUSPEND_REASON_SPIN );
	if( f->writer_waits && f->writer >= 0 )
		cpunum_resume( f->writer, SUSPEND_REASON_SPIN );
	f->reader_waits = f->writer_waits = 0;
}

static void fifo_push( struct m1_fifo *f, UINT32 v )
{
	if( f->count >= FIFO_ALLOC )
		return;
	f->data[ ( f->head + f->count ) % FIFO_ALLOC ] = v;
	f->count++;
	if( f->reader_waits )
	{
		f->reader_waits = 0;
		if( f->reader >= 0 )
			cpunum_resume( f->reader, SUSPEND_REASON_SPIN );
	}
	if( f->writer >= 0 && f->count > FIFO_SIZE && !f->writer_waits )
	{
		f->writer_waits = 1;
		cpunum_suspend( f->writer, SUSPEND_REASON_SPIN, 1 );
		if( cpu_getactivecpu() == f->writer )
			activecpu_adjust_icount( -activecpu_get_icount() - 1 );
	}
}

/* returns 0 and sets *ok = 0 when empty: the reader must repeat */
static UINT32 fifo_pop( struct m1_fifo *f, int *ok )
{
	UINT32 v;
	if( f->count == 0 )
	{
		*ok = 0;
		if( !f->reader_waits && f->reader >= 0 )
		{
			f->reader_waits = 1;
			cpunum_suspend( f->reader, SUSPEND_REASON_SPIN, 1 );
		}
		return 0;
	}
	*ok = 1;
	v = f->data[ f->head ];
	f->head = ( f->head + 1 ) % FIFO_ALLOC;
	f->count--;
	if( f->writer_waits && f->count <= FIFO_SIZE )
	{
		f->writer_waits = 0;
		if( f->writer >= 0 )
			cpunum_resume( f->writer, SUSPEND_REASON_SPIN );
	}
	return v;
}

static INLINE UINT32 fifo_peek( struct m1_fifo *f, int n )
{
	return f->data[ ( f->head + n ) % FIFO_ALLOC ];
}


/*************************************
 *  TGP state
 *************************************/

static UINT32 tgp_data_ram[ 0x400 ];	/* the real TGP's data RAM */
static UINT32 *copro_ram_lle;		/* RAM seen by the V60 and the real TGP */
static UINT32 *copro_ram_hle;		/* RAM of the HLE (same buffer unless oracle) */
#define m_copro_ram_data	copro_ram_hle

static UINT32 *copro_tables;		/* opr14742/14743 */
static UINT32 *copro_data;			/* per-game data ROM (Virtua Racing tracks...) */
static UINT32 copro_data_mask;
static UINT32 *copro_program;		/* 2048 words */

static UINT16 m_v60_copro_ram_adr;
static UINT16 m_v60_copro_ram_latch[2];
static UINT32 m_v60_copro_fifo_r, m_v60_copro_fifo_w;
static UINT32 m_copro_ram_adr[4];
static UINT32 m_copro_sincos_base, m_copro_inv_base, m_copro_isqrt_base;
static UINT32 m_copro_atan_base[4];
static UINT32 m_copro_data_base;

static offs_t m_pushpc;
static UINT32 m_copro_hle_active_list_pos, m_copro_hle_active_list_length;
static UINT32 m_copro_hle_list_length;
static UINT32 m_copro_hle_ram_scan_adr;

/* the copro RAM address registers step by 4 with bit 18 set: the TGP walks
   one of the 4 words (x, y, z, radius) of its vertex records, as current
   MAME */
static UINT32 ram_adr_next( UINT32 adr )
{
	return adr + ( ( adr & 0x40000 ) ? 4 : 1 );
}

static UINT32 ram_scan( void )
{
	UINT32 a = m_copro_hle_ram_scan_adr;
	m_copro_hle_ram_scan_adr = ram_adr_next( a );
	return a & ( DS - 1 );
}

/* the matrix stack as the TGP keeps it: slots in its RAM, the current
   matrix is the slot at the base, a depth counter beside it */
/* the TGP's data RAM 0x200-0x3ff as the programs use it: 315-5573 and
   315-5711 keep 12 word matrix slots from 0x330, 315-5711 its accumulator
   stack at 0x390-0x39f (deep matrix stacks run into it, as on the real
   TGP); 315-5724 keeps 16 word slots from 0x230 and its saved matrices
   from 0x2b0 */
#define DRAM_MAT	0x130
#define DRAM_ACC	0x190
#define DRAM_MAT_VF	0x030
#define DRAM_VEC_VF	0x0b0
static float  m_dram[ 0x200 ];
static int    m_mat_base = DRAM_MAT, m_mat_stride = 12;
#define MAT_SLOT( k )	( &m_dram[ m_mat_base + m_mat_stride * ( k ) ] )
#define MAT_SLOTS	( ( 0x200 - m_mat_base ) / m_mat_stride )
static float *m_cmat = &m_dram[ DRAM_MAT ];
static UINT32 m_vf_r0[ 0x100 ];		/* 315-5724: its variables in the TGP's RAM 0x000-0x0ff */
static UINT32 m_vf_list_adr[ 2 ];	/* 315-5724 f98: where the words of its list go */
static int    m_list_sink;			/* who takes the words of a list: 0 none, 1 f98_vf */
static INT32  m_mat_depth;
#define CMAT_SIZE	( 12 * sizeof( float ) )
static float  m_mat_vector[21][12];
static INT32  m_mat_stack_pos;
static float  m_acc;
static UINT32 m_acc_sp;
static float  m_swa_view[ 6 ];		/* 315-5711: f49 parameters (0x265-0x26a) */
static float  m_tgp_vf_xmin, m_tgp_vf_xmax, m_tgp_vf_zmin, m_tgp_vf_zmax;
static float  m_tgp_vf_ygnd, m_tgp_vf_yflr, m_tgp_vf_yjmp;
static float  m_tgp_vr_circx, m_tgp_vr_circy, m_tgp_vr_circrad;
static float  m_tgp_vr_cbox[12];
static int    m_tgp_vr_select;
static float  m_tgp_int_px, m_tgp_int_py, m_tgp_int_pz;
static UINT32 m_tgp_int_adr;
static UINT32 m_t71;			/* 315-5711: a scratch word ($71) normalize reads by mistake */
static UINT32 m_t11;			/* 315-5711: the scratch word $11, which f52 reads */
static float  m_int_n[ 4 ];		/* 315-5711: plane of the last polygon tested ($44-$47) */
static float  m_int_p[ 3 ];		/* its intersection point ($4b-$4d) */
static float  m_int_sv[ 6 ];	/* intercept: the nearest one ($275-$27a) */
static float  m_tgp_vr_base[4];
static int    m_ccount;

struct tgp_function
{
	void (*cb)( void );
	int count;
};

static UINT32 copro_data_u32( UINT32 offset )
{
	return copro_data ? copro_data[ offset & copro_data_mask ] : 0;
}

/*************************************
 *  HLE FIFO access
 *************************************/

static void fifoout_push( UINT32 data )
{
	fifo_push( &hle_out, data );
}

static void fifoout_push_f( float data )
{
	fifo_push( &hle_out, f2u( data ) );
}

static UINT32 fifoin_pop( void )
{
	int ok;
	return fifo_pop( &hle_in, &ok );
}

static float fifoin_pop_f( void )
{
	int ok;
	return u2f( fifo_pop( &hle_in, &ok ) );
}


/*************************************
 *  TGP functions (MAME 0.215)
 *************************************/

/* sine and cosine as the TGP gets them: the quarter wave table (opr14742/3)
   behind its I/O ports 0x20-0x23, a 16 bit angle */
static UINT32 table_sincos( UINT32 ang )
{
	UINT32 index = ang & 0x3fff;
	UINT32 result;
	if( ang & 0x4000 )	/* sin(90 + x) = sin(90 - x), as current MAME */
		index = index ? 0x4000 - index : 0x3fff;
	result = copro_tables[ index ];
	if( ang & 0x8000 )
		result ^= 0x80000000;
	return result;
}

/* the other table ports, as functions of what was written to them */
static UINT32 tgp_inv_r( UINT32 base, int offset )
{
	UINT32 index = ( ( base >> 9 ) & 0x3ffe ) | ( offset & 1 );
	UINT32 result = copro_tables[ index | 0x8000 ];
	UINT8 bexp = ( base >> 23 ) & 0xff;
	UINT8 exp = ( result >> 23 ) + ( 0x7f - bexp );
	result = ( result & 0x807fffff ) | ( (UINT32)exp << 23 );
	if( base & 0x80000000 )
		result ^= 0x80000000;
	return result;
}

static UINT32 tgp_isqrt_r( UINT32 base, int offset )
{
	UINT32 index = 0x2000 ^ ( ( ( base >> 10 ) & 0x3ffe ) | ( offset & 1 ) );
	UINT32 result = copro_tables[ index | 0xc000 ];
	UINT8 bexp = ( base >> 24 ) & 0x7f;
	UINT8 exp = ( result >> 23 ) + ( 0x3f - bexp );
	result = ( result & 0x807fffff ) | ( (UINT32)exp << 23 );
	if( !( offset & 1 ) )
		result &= 0x7fffffff;
	return result;
}

static UINT32 tgp_atan_r( UINT32 b0, UINT32 b1, UINT32 b2, UINT32 b3 )
{
	UINT32 idx = b3 & 0xffff;
	UINT32 result;
	UINT16 dt;
	int s0 = ( b0 & 0x80000000 ) != 0;
	int s1 = ( b1 & 0x80000000 ) != 0;
	int s2 = ( b2 & 0x80000000 ) != 0;
	if( idx & 0xc000 )
		idx = 0x3fff;
	result = copro_tables[ idx | 0x4000 ];
	/* as current MAME: the hardware corrects the table's low digits */
	dt = ( result >> 16 ) + result;
	if( dt & 0x001 )
		result -= ( ( result & 0x00f ) == 0x00e ) ? 0x00000001 : 0x00010000;
	if( dt & 0x010 )
		result -= ( ( result & 0x0f0 ) == 0x0e0 ) ? 0x00000010 : 0x00100000;
	if( dt & 0x100 )
		result -= ( ( result & 0xf00 ) == 0xe00 ) ? 0x00000100 : 0x01000000;
	if( s0 ^ s1 ^ s2 )
		result >>= 16;
	if( s2 )
		result += 0x4000;
	if( ( s0 && !s2 ) || ( s1 && s2 ) )
		result += 0x8000;
	return result & 0xffff;
}

/* 1/x the way the TGP program gets it: the table pair at port 0x28/0x29,
   one multiply-subtract with the mantissa of x */
static float tgp_recip( float x )
{
	UINT32 xb = f2u( x );
	float xn = u2f( ( xb & 0x807fffff ) | ( 0x7f << 23 ) );
	float i0 = u2f( tgp_inv_r( xb, 0 ) & 0x7fffffff );
	float i1 = u2f( tgp_inv_r( xb, 1 ) );
	float p = xn * i0;
	return i1 - p;
}

/* 1/sqrt(x) the same way, with the pair at port 0x2a/0x2b */
static float tgp_rsqrt( float x )
{
	UINT32 xb = f2u( x );
	float xn = u2f( ( xb & 0x807fffff ) | ( 0x7f << 23 ) );
	float i0 = u2f( tgp_isqrt_r( xb, 0 ) );
	float i1 = u2f( tgp_isqrt_r( xb, 1 ) );
	float p = xn * i0;
	return i1 - p;
}

/* a word of the data ROM through the TGP's 32K window: the base goes to
   port 0x2e, consecutive reads stay in the window */
static UINT32 window_word( UINT32 base, int k )
{
	return copro_data_u32( ( base & ~0x7fff ) | ( ( ( base & 0x7fff ) + k ) & 0x7fff ) );
}

static UINT32 m_tgp_fword;	/* function word of the running HLE function */

/* a word of track entry i, as the TGP fetches it (VR program 0x7d3): the
   entry address goes to the data ROM base port, the words are read through
   the 32K window */
static UINT32 track_word( UINT32 i, int k )
{
	return window_word( ( i << 4 ) + copro_data_u32( 0x20 + m_tgp_vr_select ), k );
}

static float track_f( UINT32 i, int k )
{
	return u2f( track_word( i, k ) );
}

/* is (px, pz) inside the quad p (4 points x, y, z) seen from above: the
   cross product sign for each edge, as the TGP program (VR 0x43b, 0x4b9) */
static int quad_inside( const float *p, float px, float pz )
{
	int i;
	for( i = 0; i < 4; i++ )
	{
		const float *p0 = &p[ 3 * i ];
		const float *p1 = &p[ 3 * ( ( i + 1 ) & 3 ) ];
		float d = pz * p0[ 0 ];
		d = d - px * p0[ 2 ];
		d = d - pz * p1[ 0 ];
		d = d + px * p1[ 2 ];
		d = d - p1[ 2 ] * p0[ 0 ];
		d = d + p1[ 0 ] * p0[ 2 ];
		if( ( f2u( d ) & 0x80000000 ) && ( f2u( d ) & 0x7fffffff ) )	/* negative, not -0 */
			return 0;
	}
	return 1;
}

/* the ALU's negate: 0 stays 0, -0 becomes 0 */
static float tgp_neg( float x )
{
	UINT32 u = f2u( x );
	return u2f( u ? u ^ 0x80000000 : 0 );
}

/* the ALU's d < 0 test (the sign flag): -0 is not negative */
static int tgp_lt( float d )
{
	UINT32 u = f2u( d );
	return ( u & 0x80000000 ) && ( u & 0x7fffffff );
}

/* the ALU's d <= 0 test: -0 counts as zero */
static int tgp_led( float d )
{
	UINT32 u = f2u( d );
	return !( u & 0x7fffffff ) || ( u & 0x80000000 );
}

/* angle of (x, y), 16 bits: the TGP subroutine (VR program 0x7a6) */
static UINT32 tgp_atan( float x, float y )
{
	float ax = u2f( f2u( x ) & 0x7fffffff );
	float ay = u2f( f2u( y ) & 0x7fffffff );
	float d = ax - ay;
	float num, den, r, p, q;
	if( !tgp_led( d ) )
	{
		num = ay;
		den = ax;
	}
	else
	{
		num = ax;
		den = ay;
	}
	r = tgp_recip( den );
	p = num * 16384.0f;
	q = r * p;
	return tgp_atan_r( f2u( x ), f2u( y ), f2u( d ), (UINT32)(INT32)q );
}

static float tcos(INT16 a)
{
	return u2f( table_sincos( (UINT16)a + 0x4000 ) );
}

static float tsin(INT16 a)
{
	return u2f( table_sincos( (UINT16)a ) );
}

static UINT32 ram_get_i(void)
{
	return m_copro_ram_data[ ram_scan() ];
}

static float ram_get_f(void)
{
	return u2f( m_copro_ram_data[ ram_scan() ] );
}

static void fadd(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float r = a+b;
	LOG(("TGP fadd %f+%f=%f (%x)\n", a, b, r, m_pushpc));
	fifoout_push_f(r);
}

static void fsub(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float r = a-b;
	LOG(("TGP fsub %f-%f=%f (%x)\n", a, b, r, m_pushpc));
	fifoout_push_f(r);
}

static void fmul(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float r = a*b;
	LOG(("TGP fmul %f*%f=%f (%x)\n", a, b, r, m_pushpc));
	fifoout_push_f(r);
}

static void fdiv(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float r = a * tgp_recip( b );	/* as the TGP program (VR 0x0aa) */
	LOG(("TGP fdiv %f/%f=%f (%x)\n", a, b, r, m_pushpc));
	fifoout_push_f(r);
}

static void mat_push_copy( void )
{
	if( m_mat_stack_pos + 1 < MAT_SLOTS )
	{
		memcpy( MAT_SLOT( m_mat_stack_pos + 1 ), m_cmat, CMAT_SIZE );
		m_mat_stack_pos++;
		m_cmat = MAT_SLOT( m_mat_stack_pos );
	}
}

/* 7 levels; past them only the depth counts (VR program 0x0cc) */
static void matrix_push(void)
{
	if( m_mat_depth < 7 )
		mat_push_copy();
	m_mat_depth++;
	LOG(("TGP matrix_push (depth=%d, pc=%x)\n", m_mat_depth, m_pushpc));
}

/* 315-5711 (0x108): only exactly at 7 levels does the push just count */
static void matrix_push_swa(void)
{
	if( m_mat_depth != 7 )
	{
		m_t11 = f2u( m_cmat[ 1 ] );	/* copied through $10-$1b */
		mat_push_copy();
	}
	m_mat_depth++;
	LOG(("TGP matrix_push_swa (depth=%d, pc=%x)\n", m_mat_depth, m_pushpc));
}

static void matrix_pop(void)
{
	if( m_mat_depth <= 0 )
		return;
	if( m_mat_depth < 8 && m_mat_stack_pos > 0 )
	{
		m_mat_stack_pos--;
		m_cmat = MAT_SLOT( m_mat_stack_pos );
	}
	m_mat_depth--;
	LOG(("TGP matrix_pop (depth=%d, pc=%x)\n", m_mat_depth, m_pushpc));
}

static void matrix_write(void)
{
	int i;
	for(i=0; i<12; i++)
		m_cmat[i] = fifoin_pop_f();
	LOG(("TGP matrix_write %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f) (%x)\n",
				m_cmat[0], m_cmat[1], m_cmat[2], m_cmat[3], m_cmat[4], m_cmat[5], m_cmat[6], m_cmat[7], m_cmat[8], m_cmat[9], m_cmat[10], m_cmat[11],
				m_pushpc));
}

static void clear_stack(void)
{
	LOG(("TGP clear_stack (%x)\n", m_pushpc));
	m_mat_stack_pos = 0;
	m_cmat = MAT_SLOT( 0 );
	m_mat_depth = 0;
}

static void matrix_mul(void)
{
	float m[12];
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	float e = fifoin_pop_f();
	float f = fifoin_pop_f();
	float g = fifoin_pop_f();
	float h = fifoin_pop_f();
	float i = fifoin_pop_f();
	float j = fifoin_pop_f();
	float k = fifoin_pop_f();
	float l = fifoin_pop_f();
	LOG(("TGP matrix_mul %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f (%x)\n", a, b, c, d, e, f, g, h, i, j, k, l, m_pushpc));
	m_t11 = f2u( b );	/* 315-5711 keeps the arguments in $10-$1b */
	m[0]  = a*m_cmat[0] + b*m_cmat[3] + c*m_cmat[6];
	m[1]  = a*m_cmat[1] + b*m_cmat[4] + c*m_cmat[7];
	m[2]  = a*m_cmat[2] + b*m_cmat[5] + c*m_cmat[8];
	m[3]  = d*m_cmat[0] + e*m_cmat[3] + f*m_cmat[6];
	m[4]  = d*m_cmat[1] + e*m_cmat[4] + f*m_cmat[7];
	m[5]  = d*m_cmat[2] + e*m_cmat[5] + f*m_cmat[8];
	m[6]  = g*m_cmat[0] + h*m_cmat[3] + i*m_cmat[6];
	m[7]  = g*m_cmat[1] + h*m_cmat[4] + i*m_cmat[7];
	m[8]  = g*m_cmat[2] + h*m_cmat[5] + i*m_cmat[8];
	/* the TGP program (VR 0x0f8) adds the translation before the third term
	   of the first two rows, after it in the last one */
	m[9]  = (j*m_cmat[0] + k*m_cmat[3] + m_cmat[9]) + l*m_cmat[6];
	m[10] = (j*m_cmat[1] + k*m_cmat[4] + m_cmat[10]) + l*m_cmat[7];
	m[11] = j*m_cmat[2] + k*m_cmat[5] + l*m_cmat[8] + m_cmat[11];

	memcpy(m_cmat, m, sizeof(m));
}

static void anglev(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	LOG(("TGP anglev %f, %f (%x)\n", a, b, m_pushpc));
	fifoout_push( (INT16)tgp_atan( a, b ) );	/* as the TGP program (VR 0x13f) */
}

static void triangle_normal(void)
{
	float p1x = fifoin_pop_f();
	float p1y = fifoin_pop_f();
	float p1z = fifoin_pop_f();
	float p2x = fifoin_pop_f();
	float p2y = fifoin_pop_f();
	float p2z = fifoin_pop_f();
	float p3x = fifoin_pop_f();
	float p3y = fifoin_pop_f();
	float p3z = fifoin_pop_f();

	float nx = (p1y-p2y)*(p3z-p2z) - (p3y-p2y)*(p1z-p2z);
	float ny = (p1z-p2z)*(p3x-p2x) - (p3z-p2z)*(p1x-p2x);
	float nz = (p1x-p2x)*(p3y-p2y) - (p3x-p2x)*(p1y-p2y);

	float nn = tgp_sqrt(nx*nx + ny*ny + nz*nz);
	if(nn < 1e-3)
		nn = 0;
	else
		nn = 1/nn;

	nx *= nn;
	ny *= nn;
	nz *= nn;

	LOG(("TGP triangle_normal %f, %f, %f, %f, %f, %f, %f, %f, %f (%x)\n", p1x, p1y, p1z, p2x, p2y,p2z, p3x, p3y, p3z, m_pushpc));
	fifoout_push_f(nx);
	fifoout_push_f(ny);
	fifoout_push_f(nz);
}

/* 315-5711 (0x1c6): the program never loads y and uses the scratch word
   at $71 in its place (and drops x from the length) */
static void normalize_swa(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float o = u2f( m_t71 ), s, r;
	LOG(("TGP normalize_swa %f, %f, %f (%x)\n", a, b, c, m_pushpc));
	s = b*o + o*b;
	s = s + c*c;
	r = tgp_rsqrt( s );
	fifoout_push_f(a*r);
	fifoout_push_f(o*r);
	fifoout_push_f(c*r);
}

/* 315-5711: the accumulator has a 16 entry stack (0x459, 0x462, 0x425, 0x46b) */
static void acc_push( float v )
{
	m_acc_sp--;
	m_dram[ DRAM_ACC + ( m_acc_sp & 15 ) ] = m_acc;
	m_acc = v;
}

static float acc_pop( void )
{
	float v = m_dram[ DRAM_ACC + ( m_acc_sp & 15 ) ];
	m_acc_sp++;
	return v;
}

static void acc_set_swa(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP acc_set_swa %f (%x)\n", a, m_pushpc));
	acc_push( a );
}

static void acc_seti_swa(void)
{
	INT32 a = fifoin_pop();
	LOG(("TGP acc_seti_swa %d (%x)\n", a, m_pushpc));
	acc_push( (float)a );
}

static void acc_get_swa(void)
{
	LOG(("TGP acc_get_swa (%x)\n", m_pushpc));
	fifoout_push_f( m_acc );
	m_acc = acc_pop();
}

/* the stacked value minus the accumulator, popped */
static void acc_rsub_swa(void)
{
	LOG(("TGP acc_rsub_swa (%x)\n", m_pushpc));
	m_acc = acc_pop() - m_acc;
}

static void track_select(void)
{
	INT32 a = fifoin_pop();
	LOG(("TGP track_select %d (%x)\n", a, m_pushpc));
	m_tgp_vr_select = a;
}

static void load_base(void)
{
	m_tgp_vr_base[0] = fifoin_pop_f();
	m_tgp_vr_base[1] = fifoin_pop_f();
	m_tgp_vr_base[2] = fifoin_pop_f();
	m_tgp_vr_base[3] = fifoin_pop_f();

	LOG(("TGP load_base %f, %f, %f, %f (%x)\n", m_tgp_vr_base[0], m_tgp_vr_base[1], m_tgp_vr_base[2], m_tgp_vr_base[3], m_pushpc));
}

static void transpose(void)
{
	float t;
	LOG(("TGP transpose (%x)\n", m_pushpc));

	t = m_cmat[1]; m_cmat[1] = m_cmat[3]; m_cmat[3] = t;
	t = m_cmat[2]; m_cmat[2] = m_cmat[6]; m_cmat[6] = t;
	t = m_cmat[5]; m_cmat[5] = m_cmat[7]; m_cmat[7] = t;
}

static void anglep(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	LOG(("TGP anglep %f, %f, %f, %f (%x)\n", a, b, c, d, m_pushpc));
	fifoout_push( (INT16)tgp_atan( a - c, b - d ) );
}

static void matrix_ident(void)
{
	LOG(("TGP matrix_ident (%x)\n", m_pushpc));
	memset(m_cmat, 0, CMAT_SIZE);
	m_cmat[0] = 1.0;
	m_cmat[4] = 1.0;
	m_cmat[8] = 1.0;
}

static void matrix_read(void)
{
	int i;
	LOG(("TGP matrix_read (%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f) (%x)\n",
				m_cmat[0], m_cmat[1], m_cmat[2], m_cmat[3], m_cmat[4], m_cmat[5], m_cmat[6], m_cmat[7], m_cmat[8], m_cmat[9], m_cmat[10], m_cmat[11], m_pushpc));
	for(i=0; i<12; i++)
		fifoout_push_f(m_cmat[i]);
	m_t11 = f2u( m_cmat[ 1 ] );	/* 315-5711 copies it through $10-$1b */
}

static void matrix_trans(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();

	LOG(("TCP matrix_trans %f %f %f (%x)\n", a, b, c, m_pushpc));

	/* sums in the TGP program's order (VR 0x1d1) */
	m_cmat[ 9] = ((m_cmat[ 9] + a*m_cmat[0]) + b*m_cmat[3]) + c*m_cmat[6];
	m_cmat[10] = ((m_cmat[10] + a*m_cmat[1]) + b*m_cmat[4]) + c*m_cmat[7];
	m_cmat[11] = ((m_cmat[11] + a*m_cmat[2]) + b*m_cmat[5]) + c*m_cmat[8];
}

static void matrix_scale(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	LOG(("TGP matrix_scale %f, %f, %f (%x)\n", a, b, c, m_pushpc));
	m_cmat[0] *= a;
	m_cmat[1] *= a;
	m_cmat[2] *= a;
	m_cmat[3] *= b;
	m_cmat[4] *= b;
	m_cmat[5] *= b;
	m_cmat[6] *= c;
	m_cmat[7] *= c;
	m_cmat[8] *= c;
}

static void matrix_rotx(void)
{
	INT16 a = fifoin_pop();
	float s = tsin(a);
	float c = tcos(a);
	float t1, t2;
	LOG(("TGP matrix_rotx %d (%x)\n", a, m_pushpc));
	t1 = m_cmat[3];
	t2 = m_cmat[6];
	m_cmat[3] = c*t1-s*t2;
	m_cmat[6] = s*t1+c*t2;
	t1 = m_cmat[4];
	t2 = m_cmat[7];
	m_cmat[4] = c*t1-s*t2;
	m_cmat[7] = s*t1+c*t2;
	t1 = m_cmat[5];
	t2 = m_cmat[8];
	m_cmat[5] = c*t1-s*t2;
	m_cmat[8] = s*t1+c*t2;
}

static void matrix_roty(void)
{
	INT16 a = fifoin_pop();
	float s = tsin(a);
	float c = tcos(a);
	float t1, t2;

	LOG(("TGP matrix_roty %d (%x)\n", a, m_pushpc));
	t1 = m_cmat[6];
	t2 = m_cmat[0];
	m_cmat[6] = c*t1-s*t2;
	m_cmat[0] = s*t1+c*t2;
	t1 = m_cmat[7];
	t2 = m_cmat[1];
	m_cmat[7] = c*t1-s*t2;
	m_cmat[1] = s*t1+c*t2;
	t1 = m_cmat[8];
	t2 = m_cmat[2];
	m_cmat[8] = c*t1-s*t2;
	m_cmat[2] = s*t1+c*t2;
}

/* 315-5711 (0x240): the first sum starts from the d register, which still
   holds the dispatch address (0x6c, a denormal) */
static void matrix_roty_swa(void)
{
	INT16 a = fifoin_pop();
	float s = tsin(a);
	float c = tcos(a);
	float t1, t2;

	LOG(("TGP matrix_roty_swa %d (%x)\n", a, m_pushpc));
	t1 = m_cmat[6];
	t2 = m_cmat[0];
	m_cmat[6] = c*t1-s*t2;
	m_cmat[0] = (u2f(0x6c) + c*t2) + s*t1;
	t1 = m_cmat[7];
	t2 = m_cmat[1];
	m_cmat[7] = c*t1-s*t2;
	m_cmat[1] = s*t1+c*t2;
	t1 = m_cmat[8];
	t2 = m_cmat[2];
	m_cmat[8] = c*t1-s*t2;
	m_cmat[2] = s*t1+c*t2;
}

static void matrix_rotz(void)
{
	INT16 a = fifoin_pop();
	float s = tsin(a);
	float c = tcos(a);
	float t1, t2;

	LOG(("TGP matrix_rotz %d (%x)\n", a, m_pushpc));
	t1 = m_cmat[0];
	t2 = m_cmat[3];
	m_cmat[0] = c*t1-s*t2;
	m_cmat[3] = s*t1+c*t2;
	t1 = m_cmat[1];
	t2 = m_cmat[4];
	m_cmat[1] = c*t1-s*t2;
	m_cmat[4] = s*t1+c*t2;
	t1 = m_cmat[2];
	t2 = m_cmat[5];
	m_cmat[2] = c*t1-s*t2;
	m_cmat[5] = s*t1+c*t2;
}

static void track_read_quad(void)
{
	UINT32 a = fifoin_pop();
	int offd;

	LOG(("TGP track_read_quad %d (%x)\n", a, m_pushpc));

	offd = copro_data_u32(0x20+m_tgp_vr_select) + 16*a;
	fifoout_push(copro_data_u32(offd));
	fifoout_push(copro_data_u32(offd+1));
	fifoout_push(copro_data_u32(offd+2));
	fifoout_push(copro_data_u32(offd+3));
	fifoout_push(copro_data_u32(offd+4));
	fifoout_push(copro_data_u32(offd+5));
	fifoout_push(copro_data_u32(offd+6));
	fifoout_push(copro_data_u32(offd+7));
	fifoout_push(copro_data_u32(offd+8));
	fifoout_push(copro_data_u32(offd+9));
	fifoout_push(copro_data_u32(offd+10));
	fifoout_push(copro_data_u32(offd+11));
}

/* 315-5711 polygon lists (Wing War, NetMerc collisions): the segment
   P (P + D) against polygon j of list g, as the subroutine at 0x686.
   0 hit, 1 outside the polygon, 2 parallel, 3 before P, 4 after P + D;
   *t gets the position on the segment */
static UINT32 poly_list_word( UINT32 g, int k )
{
	return window_word( 2 * g + copro_data_u32( 0x10 + m_tgp_vr_select ), k );
}

static int poly_test( const float *pp, const float *dd, UINT32 base, int j, float *tt )
{
	float v[ 12 ], dn, pn, t, ia, ib;
	UINT32 type;
	int k, a, b;

	for( k = 0; k < 12; k++ )
		v[ k ] = u2f( window_word( base, 17 * j + k ) );
	for( k = 0; k < 4; k++ )
		m_int_n[ k ] = u2f( window_word( base, 17 * j + 12 + k ) );
	type = window_word( base, 17 * j + 16 );

	dn = dd[0]*m_int_n[0] + dd[1]*m_int_n[1];
	dn = dn + dd[2]*m_int_n[2];
	if( !( f2u( dn ) & 0x7fffffff ) )
		return 2;
	pn = pp[0]*m_int_n[0] + pp[1]*m_int_n[1];
	pn = pn + pp[2]*m_int_n[2];
	pn = pn + m_int_n[3];
	t = tgp_neg( pn / dn );
	if( tgp_lt( t ) )
		return 3;
	if( !tgp_led( t - 1.0f ) )
		return 4;
	*tt = t;
	m_int_p[ 0 ] = t*dd[0] + pp[0];
	m_int_p[ 1 ] = t*dd[1] + pp[1];
	m_int_p[ 2 ] = pp[2] + t*dd[2];

	/* inside test in the plane the type gives: (A, B) = (z, y), (x, z), (y, x) */
	switch( type )
	{
	case 0:		a = 2; b = 1; break;
	case 1:		a = 0; b = 2; break;
	default:	a = 1; b = 0; break;
	}
	ia = m_int_p[ a ];
	ib = m_int_p[ b ];
	for( k = 0; k < 4; k++ )
	{
		const float *vi = &v[ 3 * k ], *vn = &v[ 3 * ( ( k + 1 ) & 3 ) ];
		float d = vi[a]*ib;
		d = d - vi[b]*ia;
		d = d - vn[a]*ib;
		d = d + vn[b]*ia;
		d = d + vi[b]*vn[a];
		d = d - vn[b]*vi[a];
		if( tgp_lt( d ) )
			return 1;
	}
	return 0;
}

/* the polygons of list g: count, then their address */
static int poly_list( UINT32 g, UINT32 *base )
{
	int n = poly_list_word( g, 0 ) & 0xff;
	*base = poly_list_word( g, 1 );
	return n ? n : 256;
}

/* nearest polygon of list g hit by the segment a-b: 0 hit (point and
   normal for int_point / int_normal), 1 none. 315-5711 program (0x472) */
static void intercept(void)
{
	float pp[ 3 ], qq[ 3 ], dd[ 3 ], best = 1.0f, t = 0;
	UINT32 g, base, res = 1;
	int i, n;

	pp[0] = fifoin_pop_f(); pp[1] = fifoin_pop_f(); pp[2] = fifoin_pop_f();
	qq[0] = fifoin_pop_f(); qq[1] = fifoin_pop_f(); qq[2] = fifoin_pop_f();
	g = fifoin_pop();
	LOG(("TGP intercept %f, %f, %f, %f, %f, %f, %x (%x)\n", pp[0], pp[1], pp[2], qq[0], qq[1], qq[2], g, m_pushpc));

	dd[0] = qq[0] - pp[0];
	dd[1] = qq[1] - pp[1];
	dd[2] = qq[2] - pp[2];
	n = poly_list( g, &base );
	for( i = 0; i < n; i++ )
		if( poly_test( pp, dd, base, i, &t ) == 0 && tgp_lt( t - best ) )
		{
			best = t;
			res = 0;
			memcpy( &m_int_sv[ 0 ], m_int_p, 3 * sizeof( float ) );
			memcpy( &m_int_sv[ 3 ], m_int_n, 3 * sizeof( float ) );
		}
	memcpy( m_int_p, &m_int_sv[ 0 ], 3 * sizeof( float ) );
	memcpy( m_int_n, &m_int_sv[ 3 ], 3 * sizeof( float ) );
	fifoout_push( res );
}

static void transform_point(void)
{
	float x = fifoin_pop_f();
	float y = fifoin_pop_f();
	float z = fifoin_pop_f();
	LOG(("TGP transform_point %f, %f, %f -> %f, %f, %f (%x)\n", x, y, z,
			 m_cmat[0]*x+m_cmat[3]*y+m_cmat[6]*z+m_cmat[9],
			 m_cmat[1]*x+m_cmat[4]*y+m_cmat[7]*z+m_cmat[10],
			 m_cmat[2]*x+m_cmat[5]*y+m_cmat[8]*z+m_cmat[11],
			 m_pushpc));
	/* sums in the TGP program's order (VR 0x2b4) */
	fifoout_push_f(((m_cmat[ 9] + x*m_cmat[0]) + y*m_cmat[3]) + z*m_cmat[6]);
	fifoout_push_f(((m_cmat[10] + x*m_cmat[1]) + y*m_cmat[4]) + z*m_cmat[7]);
	fifoout_push_f(((m_cmat[11] + x*m_cmat[2]) + y*m_cmat[5]) + z*m_cmat[8]);
}

static void fcos_m1(void)
{
	INT16 a = fifoin_pop();
	LOG(("TGP fcos %d (%x)\n", a, m_pushpc));
	fifoout_push_f(tcos(a));
}

static void fsin_m1(void)
{
	INT16 a = fifoin_pop();
	LOG(("TGP fsin %d (%x)\n", a, m_pushpc));
	fifoout_push_f(tsin(a));
}

static void fcosm_m1(void)
{
	INT16 a = fifoin_pop();
	float b = fifoin_pop_f();
	LOG(("TGP fcosm %d, %f (%x)\n", a, b, m_pushpc));
	fifoout_push_f(b*tcos(a));
}

static void fsinm_m1(void)
{
	INT16 a = fifoin_pop();
	float b = fifoin_pop_f();
	LOG(("TGP fsinm %d, %f (%x)\n", a, b, m_pushpc));
	fifoout_push_f(b*tsin(a));
}

static void distance3(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	float e = fifoin_pop_f();
	float f = fifoin_pop_f();
	LOG(("TGP distance3 (%f, %f, %f), (%f, %f, %f) (%x)\n", a, b, c, d, e, f, m_pushpc));
	/* as the TGP program (VR 0x2fb) */
	a -= d;
	b -= e;
	c -= f;
	d = a*a + b*b;
	d = d + c*c;
	fifoout_push_f(d * tgp_rsqrt(d));
}

static void ftoi(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP ftoi %f (%x)\n", a, m_pushpc));
	fifoout_push((int)a);
}

static void itof(void)
{
	INT32 a = fifoin_pop();
	LOG(("TGP itof %d (%x)\n", a, m_pushpc));
	fifoout_push_f(a);
}

static void acc_set(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP acc_set %f (%x)\n", a, m_pushpc));
	m_acc = a;
}

static void acc_get(void)
{
	LOG(("TGP acc_get (%x)\n", m_pushpc));
	fifoout_push_f(m_acc);
}

static void acc_add(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP acc_add %f (%x)\n", a, m_pushpc));
	m_acc += a;
}

static void acc_sub(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP acc_sub %f (%x)\n", a, m_pushpc));
	m_acc -= a;
}

static void acc_mul(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP acc_mul %f (%x)\n", a, m_pushpc));
	m_acc *= a;
}

static void acc_div(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP acc_div %f (%x)\n", a, m_pushpc));
	m_acc = m_acc * tgp_recip(a);	/* as the TGP program (VR 0x33d) */
}

/* Virtua Racing: position against the two edges of track entry a, the
   entry rotated by its angle (word 15); as the TGP program (0x34b) */
static void f42(void)
{
	UINT32 a = fifoin_pop();
	float x = fifoin_pop_f();
	float z = fifoin_pop_f();
	UINT32 ang = track_word( a, 15 );
	float s = u2f( table_sincos( ang ) );
	float c = u2f( table_sincos( ang + 0x4000 ) );
	float ms = u2f( table_sincos( ang + 0x8000 ) );
	float t, o0, o1, o3, o4;
	LOG(("TGP f42 %d, %f, %f (%x)\n", a, x, z, m_pushpc));

	t = s * ( track_f( a, 2 ) - z );
	o0 = t + c * ( track_f( a, 0 ) - x );
	t = s * ( track_f( a, 5 ) - z );
	o1 = t + c * ( track_f( a, 3 ) - x );
	fifoout_push_f( o0 );
	fifoout_push_f( o1 );
	fifoout_push_f( ( -1.0f * ( o1 + o0 ) ) * 0.5f );

	t = ms * ( track_f( a, 3 ) - x );
	o3 = t + c * ( track_f( a, 5 ) - z );
	t = ms * ( track_f( a, 6 ) - x );
	o4 = t + c * ( track_f( a, 8 ) - z );
	fifoout_push_f( o3 );
	fifoout_push_f( o4 );
	fifoout_push_f( ( -1.0f * ( o4 + o3 ) ) * 0.5f );
}


/* r = (x2 + y2 + z2)1/2,     f = tan-1(y/(x2+z2)1/2),     q = tan-1(z/x) */

/* r = (x2 + y2 + z2)1/2,     f = tan-1(y/(x2+z2)1/2),     q = tan-1(z/x)
   as the 315-5711 program (0x4c1) */
static void xyz2rqf(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float a2, s, xz2;
	LOG(("TGP xyz2rqf %f, %f, %f (%x)\n", a, b, c, m_pushpc));
	a2 = a*a;
	s = a2 + b*b;
	s = s + c*c;
	fifoout_push_f(s * tgp_rsqrt(s));
	fifoout_push((INT16)tgp_atan(a, c));
	xz2 = a2 + c*c;
	fifoout_push((INT16)tgp_atan(xz2 * tgp_rsqrt(xz2), b));
	m_t71 = f2u( b );
}

/* Virtua Racing: track entry a against the point (x, y, z) and the angle
   e, all over n steps: the centre of the entry's quad minus the point, and
   its heading minus e. As the TGP program (VR 0x38f) */
static void f43(void)
{
	UINT32 a = fifoin_pop();
	float x = fifoin_pop_f();
	float y = fifoin_pop_f();
	float z = fifoin_pop_f();
	INT32 e = fifoin_pop();
	INT32 n = fifoin_pop();
	float fn = (float)n, fe = (float)e, r = tgp_recip( fn );
	float p[ 3 ];
	float ang;
	int i;
	LOG(("TGP f43 %d, %f, %f, %f, %d, %d (%x)\n", a, x, y, z, e, n, m_pushpc));

	p[ 0 ] = x;
	p[ 1 ] = y;
	p[ 2 ] = z;
	for( i = 0; i < 3; i++ )
	{
		float sum = ( ( track_f( a, i ) + track_f( a, i + 3 ) ) + track_f( a, i + 6 ) ) + track_f( a, i + 9 );
		fifoout_push_f( ( 0.25f * sum - p[ i ] ) * r );
	}
	ang = (float)(INT16)tgp_atan( track_f( a, 3 ) - track_f( a, 0 ), track_f( a, 5 ) - track_f( a, 2 ) );
	fifoout_push( (UINT32)(INT32)( ( ang - fe ) * r ) );
}

/* a vector of length a in the direction of angles c (around y) and b,
   as the 315-5711 program (0x4f2) */
static void f43_swa(void)
{
	float a = fifoin_pop_f();
	UINT32 b = fifoin_pop();
	UINT32 c = fifoin_pop();
	float acc;
	LOG(("TGP f43_swa %f, %d, %d (%x)\n", a, b, c, m_pushpc));
	acc = u2f( table_sincos( c + 0x4000 ) ) * a;
	fifoout_push_f( u2f( table_sincos( b + 0x4000 ) ) * acc );
	fifoout_push_f( u2f( table_sincos( c ) ) * a );
	fifoout_push_f( u2f( table_sincos( b ) ) * acc );
	m_t71 = b;
}

static void track_read_tri(void)
{
	UINT32 a = fifoin_pop();
	int offd;

	LOG(("TGP track_read_tri %d (%x)\n", a, m_pushpc));

	offd = copro_data_u32(0x20+m_tgp_vr_select) + 16*a;
	fifoout_push(copro_data_u32(offd+12));
	fifoout_push(copro_data_u32(offd+13));
	fifoout_push(copro_data_u32(offd+14));
}

/* rotations of the current matrix by a sine / cosine pair, as the
   subroutines of the 315-5711 program (0x7c5, 0x7da) */
static void rot_y_sc( float s, float c )
{
	int i;
	for( i = 0; i < 3; i++ )
	{
		float t0 = m_cmat[ i ], t6 = m_cmat[ i + 6 ];
		m_cmat[ i ] = c*t0 - s*t6;
		m_cmat[ i + 6 ] = c*t6 + s*t0;
	}
}

static void rot_x_sc( float s, float c )
{
	int i;
	for( i = 3; i < 6; i++ )
	{
		float t3 = m_cmat[ i ], t6 = m_cmat[ i + 3 ];
		m_cmat[ i ] = c*t3 - s*t6;
		m_cmat[ i + 3 ] = s*t3 + c*t6;
	}
}

/* point the matrix along (a, b, c): as the 315-5711 program (0x507) */
static void matrix_sdir(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float xz2, s, r1, r2, l;
	LOG(("TGP matrix_sdir %f, %f, %f (%x)\n", a, b, c, m_pushpc));

	xz2 = a*a + c*c;
	s = xz2 + b*b;
	r1 = tgp_rsqrt( xz2 );
	l = xz2 * r1;
	rot_y_sc( a * r1, c * r1 );
	r2 = tgp_rsqrt( s );
	rot_x_sc( r2 * b, r2 * l );
	m_t71 = f2u( b );
}

static void fsqrt(void)
{
	float a = fifoin_pop_f();
	LOG(("TGP fsqrt %f (%x)\n", a, m_pushpc));
	fifoout_push_f(tgp_sqrt(a));
}

/* 315-5711 (0x305): the table pair without the mantissa step, so not a
   square root for most values; as the program does */
static void fsqrt_swa(void)
{
	float a = fifoin_pop_f();
	float p;
	LOG(("TGP fsqrt_swa %f (%x)\n", a, m_pushpc));
	p = a * u2f( tgp_isqrt_r( f2u( a ), 0 ) );
	fifoout_push_f( u2f( tgp_isqrt_r( f2u( a ), 1 ) ) - p );
}

static void vlength(void)
{
	float x = fifoin_pop_f();
	float y = fifoin_pop_f();
	float z = fifoin_pop_f();
	float dx, dy, dz, d;
	LOG(("TGP vlength %f, %f, %f (%x)\n", x, y, z, m_pushpc));

	/* as the TGP program (VR 0x3f2) */
	dx = m_tgp_vr_base[0] - x;
	dy = m_tgp_vr_base[1] - y;
	dz = m_tgp_vr_base[2] - z;
	d = dx*dx + dy*dy;
	d = d + dz*dz;
	fifoout_push_f((0.0f + d * tgp_rsqrt(d)) - m_tgp_vr_base[3]);
	m_t71 = f2u( m_tgp_vr_base[3] );
}

/* distance from the base point less its radius, and the distance: as the
   315-5711 program (0x32d), whose sum starts from the z difference */
static void f47(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float dx, dy, dz, s, l;
	LOG(("TGP f47 %f, %f, %f (%x)\n", a, b, c, m_pushpc));
	dx = m_tgp_vr_base[0] - a;
	dy = m_tgp_vr_base[1] - b;
	dz = m_tgp_vr_base[2] - c;
	s = dz + dx*dx;
	s = s + dy*dy;
	s = s + dz*dz;
	l = 0.0f + s * tgp_rsqrt(s);
	fifoout_push_f(l - m_tgp_vr_base[3]);
	fifoout_push_f(l);
	m_t71 = f2u( m_tgp_vr_base[3] );
}

static void track_read_info(void)
{
	UINT16 a = fifoin_pop();
	int offd;

	LOG(("TGP track_read_info %d (%x)\n", a, m_pushpc));

	offd = copro_data_u32(0x20+m_tgp_vr_select) + 16*a;
	fifoout_push(copro_data_u32(offd+15));
}

static void colbox_set(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	float e = fifoin_pop_f();
	float f = fifoin_pop_f();
	float g = fifoin_pop_f();
	float h = fifoin_pop_f();
	float i = fifoin_pop_f();
	float j = fifoin_pop_f();
	float k = fifoin_pop_f();
	float l = fifoin_pop_f();
	LOG(("TGP colbox_set %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f (%x)\n", a, b, c, d, e, f, g, h, i, j, k, l, m_pushpc));
	m_tgp_vr_cbox[ 0] = a;
	m_tgp_vr_cbox[ 1] = b;
	m_tgp_vr_cbox[ 2] = c;
	m_tgp_vr_cbox[ 3] = d;
	m_tgp_vr_cbox[ 4] = e;
	m_tgp_vr_cbox[ 5] = f;
	m_tgp_vr_cbox[ 6] = g;
	m_tgp_vr_cbox[ 7] = h;
	m_tgp_vr_cbox[ 8] = i;
	m_tgp_vr_cbox[ 9] = j;
	m_tgp_vr_cbox[10] = k;
	m_tgp_vr_cbox[11] = l;
}

/* Virtua Racing: is the point inside the quad set by colbox_set (seen
   from above)? As the TGP program (0x43b): the cross product sign for each
   edge, -1 (all bits set) inside, 0 outside */
static void colbox_test(void)
{
	float px = fifoin_pop_f();
	float py = fifoin_pop_f();
	float pz = fifoin_pop_f();
	(void)py;
	LOG(("TGP colbox_test %f, %f, %f (%x)\n", px, py, pz, m_pushpc));
	/* -1 (all bits set) inside, 0 outside */
	fifoout_push( quad_inside( m_tgp_vr_cbox, px, pz ) ? 0xffffffff : 0 );
}

/* the view for f50_swa: projection scales and screen limits, as the
   315-5711 program (0x538) */
static void f49_swa(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	LOG(("TGP f49_swa %f, %f (%x)\n", a, b, m_pushpc));
	m_swa_view[ 0 ] = a * 4.0f;
	m_swa_view[ 1 ] = b * 4.0f;
	m_swa_view[ 2 ] = fifoin_pop_f();
	m_swa_view[ 3 ] = fifoin_pop_f();
	m_swa_view[ 4 ] = fifoin_pop_f();
	m_swa_view[ 5 ] = fifoin_pop_f();
}

/* is the sphere (x, y, z) radius r on screen: its depth when it is, -1
   when it is out of the sides, -2 behind. As the 315-5711 program (0x546) */
static void f50_swa(void)
{
	float x = fifoin_pop_f();
	float y = fifoin_pop_f();
	float z = fifoin_pop_f();
	float r = fifoin_pop_f();
	float tx, ty, tz, rz, k1, k2, sx, sy;
	LOG(("TGP f50_swa %f, %f, %f, %f (%x)\n", x, y, z, r, m_pushpc));

	tx = ((m_cmat[ 9] + x*m_cmat[0]) + y*m_cmat[3]) + z*m_cmat[6];
	ty = ((m_cmat[10] + x*m_cmat[1]) + y*m_cmat[4]) + z*m_cmat[7];
	tz = ((m_cmat[11] + x*m_cmat[2]) + y*m_cmat[5]) + z*m_cmat[8];
	if( ( f2u( tz ) & 0x80000000 ) && ( f2u( tz ) & 0x7fffffff ) )
	{
		fifoout_push_f( -2.0f );
		return;
	}
	rz = tgp_recip( tz );
	k1 = ( r * m_swa_view[ 0 ] ) * rz;
	m_t71 = f2u( k1 );
	k2 = ( r * m_swa_view[ 1 ] ) * rz;
	sx = ( tx * m_swa_view[ 0 ] ) * rz;
	sy = ( ty * m_swa_view[ 1 ] ) * rz;
	if( !tgp_lt( ( sy - k2 ) - m_swa_view[ 4 ] ) ||
		tgp_led( ( sy + k2 ) - m_swa_view[ 5 ] ) ||
		!tgp_lt( ( sx - k1 ) - m_swa_view[ 2 ] ) ||
		tgp_led( ( sx + k1 ) - m_swa_view[ 3 ] ) )
		fifoout_push_f( -1.0f );
	else
		fifoout_push_f( tz );
}

/* 0x34 (0x59c): inverts the current matrix, by cofactors, as the 315-5711
   program does it: in its order, with an OR of two elements where a
   subtract is expected, and $11 (left by an earlier function) in the
   second element. $70 / $71 keep two of the terms */
static void f52(void)
{
	float *m = m_cmat;
	float e20 = m[4] - m[3] * m[1];
	float r20 = 1.0f / e20;
	float e21 = ( m[5] - m[3] * m[2] ) * r20;
	float e23 = m[7] - m[6] * m[1];
	float r70 = 1.0f / ( u2f( f2u( m[8] ) | f2u( m[6] ) ) - e23 * e21 );
	float e71 = ( m[3] + m[3] ) * r20;
	float e26 = m[10] - m[9] * m[1];
	float e27 = ( m[11] - m[9] * m[2] ) - e21 * e26;
	float e1e = m[2] - m[1] * e21;
	float e16 = ( tgp_neg( m[6] ) - e23 * e71 ) * r70;
	float e17 = ( e23 * tgp_neg( r20 ) ) * r70;
	float e1d = tgp_neg( m[9] ) - e26 * e71;
	float r[12];

	LOG(("TGP f52 (%x)\n", m_pushpc));
	r[0]  = ( 1.0f - m[1] * e71 ) - e1e * e16;
	r[1]  = u2f( m_t11 ) - e1e * e17;
	r[2]  = e1e * tgp_neg( r70 );
	r[3]  = e71 - e20 * e16;
	r[4]  = r20 - e21 * e17;
	r[5]  = e21 * tgp_neg( r70 );
	r[6]  = e16;
	r[7]  = e17;
	r[8]  = r70;
	r[9]  = e1d - e27 * e16;
	r[10] = e26 * tgp_neg( r20 ) - e27 * e17;
	r[11] = e27 * tgp_neg( r70 );
	memcpy( m_cmat, r, sizeof( r ) );
	m_t11 = f2u( r[ 1 ] );
	m_t71 = f2u( e71 );
}

/* the inverse of matrix_sdir: as the 315-5711 program (0x61d) */
static void matrix_rdir(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float xz2, s, r1, r2, l;
	LOG(("TGP matrix_rdir %f, %f, %f (%x)\n", a, b, c, m_pushpc));

	xz2 = a*a + c*c;
	s = xz2 + b*b;
	r1 = tgp_rsqrt( xz2 );
	l = xz2 * r1;
	r2 = tgp_rsqrt( s );
	rot_x_sc( tgp_neg( r2 * b ), r2 * l );
	rot_y_sc( tgp_neg( a * r1 ), c * r1 );
}
/* height of the plane through p0, p1, p2 at (x, z): VR program 0x73a */
static float plane_height( const float *p0, const float *p1, const float *p2, float x, float z )
{
	float nx, ny, nz, k;
	nx = (p1[1] - p0[1]) * (p2[2] - p0[2]) - (p1[2] - p0[2]) * (p2[1] - p0[1]);
	ny = (p1[2] - p0[2]) * (p2[0] - p0[0]) - (p1[0] - p0[0]) * (p2[2] - p0[2]);
	nz = (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0]);
	if( !( f2u( ny ) & 0x7fffffff ) )
		return 0;	/* the program leaves a stale value here */
	k = nx*x + nz*z;
	k = k - nx*p0[0];
	k = k - ny*p0[1];
	k = k - nz*p0[2];
	return -1.0f * ( k * tgp_recip( ny ) );
}

/* height of a track quad at (x, z): the triangle p0 p1 p2 or, when (x, z)
   is outside it, p0 p2 p3. VR program 0x693 */
static float quad_height( const float *p, float x, float z )
{
	const float *p0 = &p[ 0 ], *p1 = &p[ 3 ], *p2 = &p[ 6 ];
	float dx12 = p1[0] - p2[0], dz12 = p1[2] - p2[2];
	float dx02 = p0[0] - p2[0], dz02 = p0[2] - p2[2];
	float dx = x - p2[0], dz = z - p2[2];
	float ta, tb, den, num;

	if( f2u( 0.0f + dz02 ) & 0x7fffffff )
	{
		den = dz02 * dx12 - dz12 * dx02;
		num = dz02 * dx - dz * dx02;
		ta = num * tgp_recip( den );
	}
	else
		ta = dz * tgp_recip( dz12 );

	if( f2u( 0.0f + dx02 ) & 0x7fffffff )
		tb = ( dx - dx12 * ta ) * tgp_recip( dx02 );
	else
	{
		num = dz * dx12 - dx * dz12;
		tb = num * tgp_recip( dz02 * dx12 );
	}

	{
		/* both >= 0: the sign of their bits ORed, as a float plus 0 */
		UINT32 o = f2u( 0.0f + u2f( f2u( ta ) | f2u( tb ) ) );
		if( ( o & 0x80000000 ) && ( o & 0x7fffffff ) )
		{
			p1 = &p[ 6 ];
			p2 = &p[ 9 ];
		}
	}
	return plane_height( p0, p1, p2, x, z );
}

/* Virtua Racing: the track quad under (x, z) from list b of the selected
   track, the one nearest height a. Results in the TGP RAM at the address
   in the function word: [0] 0 when done, [1] height (count when none),
   [2] entry. As the TGP program (VR 0x482) */
static void track_lookup(void)
{
	float a = fifoin_pop_f();
	UINT32 b = fifoin_pop();
	float x = fifoin_pop_f();
	float z = fifoin_pop_f();
	UINT32 adr = m_tgp_fword & 0x80000000 ? m_tgp_fword | 0x7f800000 : m_tgp_fword & 0x807fffff;
	UINT32 offi = copro_data_u32( 0x10 + m_tgp_vr_select ) + b;
	UINT32 len = window_word( offi, 0 ), i;
	UINT32 count = 0, best_entry = 0;
	float best_h = 0, best_d = 0;

	LOG(("TGP track_lookup %f, 0x%x, %f, %f (%x)\n", a, b, x, z, m_pushpc));

	m_copro_ram_data[ adr & ( DS - 1 ) ] = 0xffffffff;	/* still searching */

	for( i = 0; i < len; i++ )
	{
		UINT32 bpos = window_word( offi, 1 + i );
		float pts[ 12 ];
		int k;
		for( k = 0; k < 12; k++ )
			pts[ k ] = track_f( bpos, k );
		if( quad_inside( pts, x, z ) )
		{
			float h = quad_height( pts, x, z );
			float dd = u2f( f2u( a - u2f( f2u( h ) & 0x7fffffff ) ) & 0x7fffffff );
			/* the first one, then any as near or nearer */
			if( !count || tgp_led( dd - best_d ) )
			{
				best_d = dd;
				best_h = h;
				best_entry = bpos;
			}
			count++;
		}
	}

	if( count )
	{
		m_copro_ram_data[ ( adr + 1 ) & ( DS - 1 ) ] = f2u( best_h );
		m_copro_ram_data[ ( adr + 2 ) & ( DS - 1 ) ] = best_entry;
	}
	else
		m_copro_ram_data[ ( adr + 1 ) & ( DS - 1 ) ] = 0;
	m_copro_ram_data[ adr & ( DS - 1 ) ] = 0;
}

/* the segment against the polygons of list g, up to the first one hit:
   0 hit, else the last polygon's result. 315-5711 program (0x64b) */
static void f56(void)
{
	float pp[ 3 ], qq[ 3 ], dd[ 3 ], t;
	UINT32 g, base;
	int i, n, r = 0;

	pp[0] = fifoin_pop_f(); pp[1] = fifoin_pop_f(); pp[2] = fifoin_pop_f();
	qq[0] = fifoin_pop_f(); qq[1] = fifoin_pop_f(); qq[2] = fifoin_pop_f();
	g = fifoin_pop();
	LOG(("TGP f56 %f, %f, %f, %f, %f, %f, %d (%x)\n", pp[0], pp[1], pp[2], qq[0], qq[1], qq[2], g, m_pushpc));

	dd[0] = qq[0] - pp[0];
	dd[1] = qq[1] - pp[1];
	dd[2] = qq[2] - pp[2];
	n = poly_list( g, &base );
	for( i = 0; i < n; i++ )
	{
		r = poly_test( pp, dd, base, i, &t );
		if( r == 0 )
			break;
	}
	fifoout_push( r );
}

/* 315-5711 functions no game was seen calling, with the arguments and
   results the program takes and gives so the command stream stays in step */

/* 0x19 (0x272): the matrix stack depth */
static void stack_depth_swa(void)
{
	fifoout_push( m_mat_depth );
}

/* 0x20 (0x444): the accumulator times the popped stack value */
static void acc_mulpop_swa(void)
{
	m_acc = m_acc * acc_pop();
}

/* 0x17 (0x43d): stacked value plus accumulator, ANDed bitwise with it */
static void acc_addand_swa(void)
{
	float v = acc_pop() + m_acc;
	m_acc = u2f( f2u( v ) & f2u( m_acc ) );
}

/* 0x30 (0x52f): takes an angle and returns to the command loop at once
   (the next word is read as a command) */
static void f30_swa(void)
{
	fifoin_pop();
}

/* 0x04 (0x0f5), 0x3e (0x3bb): self tests, the results are not emulated */
static void f04_swa(void)
{
	fifoin_pop();
	fifoin_pop();
	fifoout_push( 0 );
}

static void romsum_swa(void)
{
	fifoin_pop();
	fifoin_pop();
	fifoin_pop();
	fifoout_push( 0 );
	fifoout_push( 0 );
}

/* 0x3d (0x34d): table ROM check */
static void tabsum_swa(void)
{
	fifoout_push( 0 );
	fifoout_push( 0 );
}

/* 0x3f (0x3e2) restarts the command loop; 0x42 (0x726) pops and multiplies
   the matrix stack (with bugs that overwrite it); 0x36/0x37 do nothing */
static void nop_swa(void)
{
}

/* 315-5711 (0x710): selects the polygon lists */
static void select_swa(void)
{
	m_tgp_vr_select = fifoin_pop();
	LOG(("TGP select_swa %d (%x)\n", m_tgp_vr_select, m_pushpc));
}

static void int_normal(void)
{
	LOG(("TGP int_normal (%x)\n", m_pushpc));
	fifoout_push_f(m_int_n[0]);
	fifoout_push_f(m_int_n[1]);
	fifoout_push_f(m_int_n[2]);
}

static void matrix_readt(void)
{
	LOG(("TGP matrix_readt (%x)\n", m_pushpc));
	fifoout_push_f(m_cmat[9]);
	fifoout_push_f(m_cmat[10]);
	fifoout_push_f(m_cmat[11]);
}

static void acc_geti(void)
{
	LOG(("TGP acc_geti (%x)\n", m_pushpc));
	fifoout_push((int)m_acc);
}

static void int_point(void)
{
	LOG(("TGP int_point (%x)\n", m_pushpc));
	fifoout_push_f(m_int_p[0]);
	fifoout_push_f(m_int_p[1]);
	fifoout_push_f(m_int_p[2]);
}

static void col_setcirc(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	LOG(("TGP col_setcirc %f, %f, %f (%x)\n", a, b, c, m_pushpc));
	m_tgp_vr_circx = a;
	m_tgp_vr_circy = b;
	m_tgp_vr_circrad = c;
}

static void col_testpt(void)
{
	float x, y, d;
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	LOG(("TGP col_testpt %f, %f (%x)\n", a, b, m_pushpc));
	/* as the TGP program (VR 0x603) */
	x = m_tgp_vr_circx - a;
	y = m_tgp_vr_circy - b;
	d = x*x + y*y;
	fifoout_push_f((0.0f + d * tgp_rsqrt(d)) - m_tgp_vr_circrad);
}

/* the same, with the distance too (VR 0x61e) */
static void col_testpt2(void)
{
	float x, y, d, l;
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	LOG(("TGP col_testpt2 %f, %f (%x)\n", a, b, m_pushpc));
	x = m_tgp_vr_circx - a;
	y = m_tgp_vr_circy - b;
	d = x*x + y*y;
	l = 0.0f + d * tgp_rsqrt(d);
	fifoout_push_f(l - m_tgp_vr_circrad);
	fifoout_push_f(l);
}

/* 315-5711 (0x713): an identity in a new slot. Past 7 levels the program
   jumps into the middle of another function; nothing done here */
static void push_and_ident(void)
{
	LOG(("TGP push_and_ident (depth=%d, pc=%x)\n", m_mat_depth, m_pushpc));
	if( m_mat_depth >= 7 || m_mat_stack_pos + 1 >= MAT_SLOTS )
		return;
	m_mat_stack_pos++;
	m_cmat = MAT_SLOT( m_mat_stack_pos );
	memset(m_cmat, 0, CMAT_SIZE);
	m_cmat[0] = 1.0;
	m_cmat[4] = 1.0;
	m_cmat[8] = 1.0;
	m_mat_depth++;
}

static void catmull_rom(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	float e = fifoin_pop_f();
	float f = fifoin_pop_f();
	float g = fifoin_pop_f();
	float h = fifoin_pop_f();
	float i = fifoin_pop_f();
	float j = fifoin_pop_f();
	float k = fifoin_pop_f();
	float l = fifoin_pop_f();
	float m = fifoin_pop_f();
	float m2, m3;
	float w1, w2, w3, w4;

	LOG(("TGP catmull_rom %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f (%x)\n", a, b, c, d, e, f, g, h, i, j, k, l, m, m_pushpc));

	m2 = m*m;
	m3 = m*m*m;

	w1 = 0.5f*(-m3+2*m2-m);
	w2 = 0.5f*(3*m3-5*m2+2);
	w3 = 0.5f*(-3*m3+4*m2+m);
	w4 = 0.5f*(m3-m2);

	fifoout_push_f(a*w1+d*w2+g*w3+j*w4);
	fifoout_push_f(b*w1+e*w2+h*w3+k*w4);
	fifoout_push_f(c*w1+f*w2+i*w3+l*w4);
}

static void distance(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	float s;
	LOG(("TGP distance (%f, %f), (%f, %f) (%x)\n", a, b, c, d, m_pushpc));
	/* as the TGP program (VR 0x639, 0x683) */
	c = a - c;
	d = b - d;
	s = c*c + d*d;
	fifoout_push_f(s * tgp_rsqrt(s));
}

static void car_move(void)
{
	INT16 a = fifoin_pop();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	float dx, dy;
	LOG(("TGP car_move (%d, %f), (%f, %f) (%x)\n", a, b, c, d, m_pushpc));

	dx = b*tsin(a);
	dy = b*tcos(a);

	fifoout_push_f(dx);
	fifoout_push_f(dy);
	fifoout_push_f(c+dx);
	fifoout_push_f(d+dy);
}

static void matrix_rtrans(void)
{
	LOG(("TGP matrix_rtrans (%x)\n", m_pushpc));
	fifoout_push_f(m_cmat[ 9]);
	fifoout_push_f(m_cmat[10]);
	fifoout_push_f(m_cmat[11]);
}

static void matrix_unrot(void)
{
	LOG(("TGP matrix_unrot (%x)\n", m_pushpc));
	memset(m_cmat, 0, 9*sizeof(m_cmat[0]));
	m_cmat[0] = 1.0;
	m_cmat[4] = 1.0;
	m_cmat[8] = 1.0;
}

static void ram_setadr(void)
{
	m_copro_hle_ram_scan_adr = fifoin_pop();
	LOG(("TGP f0 ram_setadr 0x%x (%x)\n", m_copro_hle_ram_scan_adr, m_pushpc));
}

/* Addresses in daytona's and VR's TGP programs */
/*************************************
 *  315-5724 (Virtua Fighter): the functions its program does its own way.
 *  Its matrix stack has 16 word slots from 0x230 (depth $30, current
 *  matrix $31), its saved matrices 16 word slots from 0x2b0
 *************************************/

/* the 1/sqrt(x) pair at port 0x2a/0x2b, the first term taken absolute
   (0x1d7); x times it is the square root (0x1df) */
static float tgp_rsqrt_vf( float x )
{
	UINT32 xb = f2u( x );
	float xn = u2f( ( xb & 0x807fffff ) | ( 0x7f << 23 ) );
	float i0 = u2f( tgp_isqrt_r( xb, 0 ) & 0x7fffffff );
	float i1 = u2f( tgp_isqrt_r( xb, 1 ) );
	return i1 - xn * i0;
}

static float tgp_sqrt_vf( float x )
{
	return tgp_rsqrt_vf( x ) * x;
}

/* a point through matrix m, the translation added last (0x1c3) */
static void vf_transform( const float *m, float x, float y, float z, float *r )
{
	int i;
	for( i = 0; i < 3; i++ )
		r[ i ] = ( ( m[ i ] * x + m[ i + 3 ] * y ) + m[ i + 6 ] * z ) + m[ i + 9 ];
}

/* a times m (0x218): the translation goes in after the second product of
   each row */
static void vf_mul( const float *a, const float *m, float *r )
{
	int i, j;
	for( i = 0; i < 3; i++ )
		for( j = 0; j < 3; j++ )
			r[ 3 * i + j ] = ( a[ 3 * i ] * m[ j ] + a[ 3 * i + 1 ] * m[ j + 3 ] ) + a[ 3 * i + 2 ] * m[ j + 6 ];
	for( j = 0; j < 3; j++ )
		r[ 9 + j ] = ( ( a[ 9 ] * m[ j ] + a[ 10 ] * m[ j + 3 ] ) + m[ j + 9 ] ) + a[ 11 ] * m[ j + 6 ];
}

/* (a.b) in the program's order (0x24e) */
static float vf_dot( const float *a, const float *b )
{
	return ( a[ 0 ] * b[ 0 ] + a[ 1 ] * b[ 1 ] ) + a[ 2 ] * b[ 2 ];
}

/* saved matrix n (0x2b0 + 16 n), NULL past the RAM; vmat_save and
   vmat_load move the first 16 */
static float *vf_vec( UINT32 n )
{
	return n < 21 ? &m_dram[ DRAM_VEC_VF + 16 * n ] : NULL;
}

static void fdiv_vf(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	fifoout_push_f( a / b );	/* the DSP's divide (0x0b5) */
}

/* the angle as the table port gives it, 0-0xffff (0x0f4) */
static void anglev_vf(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	fifoout_push( tgp_atan( a, b ) );
}

static void anglep_vf(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float d = fifoin_pop_f();
	fifoout_push( tgp_atan( a - c, b - d ) );
}

/* 0x0ce: past 7 levels the program takes 8 off the depth (the register
   it subtracts was meant to hold 1) */
static void matrix_pop_vf(void)
{
	if( m_mat_depth >= 8 )
		m_mat_depth -= 8;
	else if( m_mat_depth > 0 )
	{
		if( m_mat_stack_pos > 0 )
			m_mat_stack_pos--;
		m_cmat = MAT_SLOT( m_mat_stack_pos );
		m_mat_depth--;
	}
}

static void matrix_mul_vf(void)
{
	float a[ 12 ], r[ 12 ];
	int i;
	for( i = 0; i < 12; i++ )
		a[ i ] = fifoin_pop_f();
	vf_mul( a, m_cmat, r );
	memcpy( m_cmat, r, sizeof( r ) );
}

static void matrix_trans_vf(void)
{
	float x = fifoin_pop_f();
	float y = fifoin_pop_f();
	float z = fifoin_pop_f();
	float r[ 3 ];
	vf_transform( m_cmat, x, y, z, r );
	memcpy( &m_cmat[ 9 ], r, sizeof( r ) );
}

static void transform_point_vf(void)
{
	float x = fifoin_pop_f();
	float y = fifoin_pop_f();
	float z = fifoin_pop_f();
	float r[ 3 ];
	vf_transform( m_cmat, x, y, z, r );
	fifoout_push_f( r[ 0 ] );
	fifoout_push_f( r[ 1 ] );
	fifoout_push_f( r[ 2 ] );
}

/* 0x199: the table pair alone, 1/sqrt(x) */
static void fsqrt_vf(void)
{
	fifoout_push_f( tgp_rsqrt_vf( fifoin_pop_f() ) );
}

/* 0x2fd: the translation set to 0 */
static void f80_vf(void)
{
	m_cmat[ 9 ] = m_cmat[ 10 ] = m_cmat[ 11 ] = 0;
}

static void vmat_store_vf(void)
{
	float *v = vf_vec( fifoin_pop() );
	if( v )
		memcpy( v, m_cmat, CMAT_SIZE );
}

static void vmat_restore_vf(void)
{
	float *v = vf_vec( fifoin_pop() );
	if( v )
		memcpy( m_cmat, v, CMAT_SIZE );
}

static void vmat_mul_vf(void)
{
	float *a = vf_vec( fifoin_pop() );
	float *b = vf_vec( fifoin_pop() );
	float r[ 12 ];
	if( a && b )
	{
		vf_mul( a, m_cmat, r );
		memcpy( b, r, sizeof( r ) );
	}
}

static void vmat_read_vf(void)
{
	float *v = vf_vec( fifoin_pop() );
	int i;
	for( i = 0; i < 12; i++ )
		fifoout_push_f( v ? v[ i ] : 0 );
}

/* all 16 slots, 256 words, to and from the copro RAM, through the RAM
   address port ram_setadr also uses */
static void vmat_save_vf(void)
{
	int i;
	m_copro_hle_ram_scan_adr = ( fifoin_pop() & 0xffff ) | 0x10000;
	for( i = 0; i < 256; i++ )
		memcpy( &m_copro_ram_data[ ram_scan() ], &m_dram[ DRAM_VEC_VF + i ], 4 );
}

static void vmat_load_vf(void)
{
	int i;
	m_copro_hle_ram_scan_adr = ( fifoin_pop() & 0xffff ) | 0x10000;
	for( i = 0; i < 256; i++ )
		memcpy( &m_dram[ DRAM_VEC_VF + i ], &m_copro_ram_data[ ram_scan() ], 4 );
}

static void vmat_load1_vf(void)
{
	int i;
	m_copro_hle_ram_scan_adr = ( fifoin_pop() & 0xffff ) | 0x10000;
	for( i = 0; i < 12; i++ )
		memcpy( &m_cmat[ i ], &m_copro_ram_data[ ram_scan() ], 4 );
}

/* every saved matrix times the current one (0x474) */
static void vmat_flatten_vf(void)
{
	float r[ 12 ];
	int i;
	for( i = 15; i >= 0; i-- )
	{
		vf_mul( vf_vec( i ), m_cmat, r );
		memcpy( vf_vec( i ), r, sizeof( r ) );
	}
}

static void ram_trans_vf(void)
{
	float x = ram_get_f();
	float y = ram_get_f();
	float z = ram_get_f();
	float r[ 3 ];
	vf_transform( m_cmat, x, y, z, r );
	memcpy( &m_cmat[ 9 ], r, sizeof( r ) );
}

/* 0x264: the nearest the two points get, the second moving by dv against
   the first: w = a0 - a2, u = a1 - a3 per axis, dv = u - w */
static void cpa_vf(void)
{
	float w[ 3 ], u[ 3 ], dv[ 3 ], q[ 3 ], n, dd, t, r;
	int k;
	for( k = 0; k < 3; k++ )
	{
		float a0 = fifoin_pop_f();
		float a1 = fifoin_pop_f();
		float a2 = fifoin_pop_f();
		float a3 = fifoin_pop_f();
		w[ k ] = a0 - a2;
		u[ k ] = a1 - a3;
		dv[ k ] = u[ k ] - w[ k ];
	}
	n = tgp_neg( vf_dot( w, dv ) );
	if( tgp_led( n ) )
		r = tgp_sqrt_vf( vf_dot( w, w ) );
	else
	{
		dd = vf_dot( dv, dv );
		if( tgp_led( dd - n ) )
			r = tgp_sqrt_vf( vf_dot( u, u ) );
		else
		{
			t = n / dd;
			for( k = 0; k < 3; k++ )
				q[ k ] = w[ k ] + dv[ k ] * t;
			r = tgp_sqrt_vf( vf_dot( q, q ) );
		}
	}
	fifoout_push_f( r );
}

/* 0x5ff: the box in $40-$46: floor, ground and jump heights, x max and
   min, z max and min */
static void groundbox_set_vf(void)
{
	int i;
	for( i = 0; i < 7; i++ )
		m_vf_r0[ 0x40 + i ] = fifoin_pop();
}

/* 0x36e: the point goes to the copro RAM, the ground height comes back
   from the word after it; three flags: under the ground outside the box,
   under the floor inside it, under the jump height */
static void groundbox_test_vf(void)
{
	float a = fifoin_pop_f();
	float b = fifoin_pop_f();
	float c = fifoin_pop_f();
	float p[ 3 ], h, y;
	UINT32 out = 0, in = 0, jump = 0;
	vf_transform( m_cmat, a, b, c, p );
	m_copro_ram_data[ ram_scan() ] = f2u( p[ 0 ] );
	m_copro_ram_data[ ram_scan() ] = f2u( p[ 1 ] );
	m_copro_ram_data[ ram_scan() ] = f2u( p[ 2 ] );
	h = ram_get_f();
	y = p[ 1 ] - h;
	if( !tgp_led( u2f( m_vf_r0[ 0x43 ] ) - p[ 0 ] ) && tgp_lt( u2f( m_vf_r0[ 0x44 ] ) - p[ 0 ] ) &&
		!tgp_led( u2f( m_vf_r0[ 0x45 ] ) - p[ 2 ] ) && tgp_lt( u2f( m_vf_r0[ 0x46 ] ) - p[ 2 ] ) )
	{
		if( tgp_lt( y - u2f( m_vf_r0[ 0x40 ] ) ) )
			in = 1;
	}
	else if( tgp_lt( y - u2f( m_vf_r0[ 0x41 ] ) ) )
		out = 1;
	if( tgp_lt( y - u2f( m_vf_r0[ 0x42 ] ) ) )
		jump = 1;
	fifoout_push( out );
	fifoout_push( in );
	fifoout_push( jump );
}


/* 315-5724's variables at $34-$73 (list bases, view and camera) */
static float vf_r( int x )
{
	return u2f( m_vf_r0[ x ] );
}

static void vf_w( int x, float v )
{
	m_vf_r0[ x ] = f2u( v );
}

/* a word of the TGP's RAM 0x200-0x3ff, kept as the bits it holds */
static UINT32 vf_dram_r( UINT32 i )
{
	UINT32 v;
	memcpy( &v, &m_dram[ i & 0x1ff ], 4 );
	return v;
}

static void vf_dram_w( UINT32 i, UINT32 v )
{
	memcpy( &m_dram[ i & 0x1ff ], &v, 4 );
}

/* rows ra and rb of the current matrix turned by the angle (0x20c) */
static void vf_rot( int ra, int rb, UINT32 ang )
{
	float s = u2f( table_sincos( ang ) );
	float c = u2f( table_sincos( ang + 0x4000 ) );
	int j;
	for( j = 0; j < 3; j++ )
	{
		float a = m_cmat[ ra + j ], b = m_cmat[ rb + j ];
		m_cmat[ ra + j ] = a * c - b * s;
		m_cmat[ rb + j ] = a * s + b * c;
	}
}

/* |v| as x * 1/sqrt(x) */
static float vf_len( float x, float y, float z )
{
	return tgp_sqrt_vf( ( x * x + y * y ) + z * z );
}

/* 0x3c5: the bases of the two sphere lists (records of x, y, z, radius
   in the copro RAM, page 4 so the address steps by 4), the offset of the
   previous positions, the length */
static void f89_vf(void)
{
	int i;
	for( i = 0; i < 4; i++ )
		m_vf_r0[ 0x34 + i ] = fifoin_pop();
}

/* 0x400: $38-$3b; $3b becomes $3a - $3b, $3c and $3d (flags) cleared */
static void f92_vf(void)
{
	int i;
	for( i = 0; i < 4; i++ )
		m_vf_r0[ 0x38 + i ] = fifoin_pop();
	vf_w( 0x3b, vf_r( 0x3a ) - vf_r( 0x3b ) );
	m_vf_r0[ 0x3c ] = m_vf_r0[ 0x3d ] = 0;
}

/* 0x40c: the current matrix from the angles $38, $39 and a */
static void f93_vf(void)
{
	UINT32 a = fifoin_pop();
	float fa = u2f( a );
	float k = u2f( table_sincos( m_vf_r0[ 0x38 ] + 0x8000 ) ) / u2f( table_sincos( m_vf_r0[ 0x38 ] + 0x4000 ) );
	float s = u2f( table_sincos( m_vf_r0[ 0x39 ] ) );
	float c = u2f( table_sincos( m_vf_r0[ 0x39 ] + 0x4000 ) );
	memset( m_cmat, 0, CMAT_SIZE );
	m_cmat[ 0 ] = 1.0f;
	m_cmat[ 8 ] = 1.0f;
	memcpy( &m_cmat[ 10 ], &a, 4 );
	m_cmat[ 3 ] = s * k;
	m_cmat[ 5 ] = c * k;
	m_cmat[ 9 ] = tgp_neg( fa * m_cmat[ 3 ] );
	m_cmat[ 11 ] = tgp_neg( fa * m_cmat[ 5 ] );
}

/* 0x42b: each saved matrix above the ground (y > 0) times the matrix at
   0x3c0, or the one at 0x3d0 when its translation leaves the box ($3a or
   $3b); a bit per matrix in the flags at $3c + n records which */
static void f94_vf(void)
{
	UINT32 n = fifoin_pop();
	UINT32 fl = m_vf_r0[ ( 0x3c + n ) & 0xff ], mask = 1;
	float m10[ 12 ], m50[ 12 ], r[ 12 ];
	int i;
	memcpy( m10, &m_dram[ 0x1c0 ], sizeof( m10 ) );
	memcpy( m50, &m_dram[ 0x1d0 ], sizeof( m50 ) );
	memcpy( &m_vf_r0[ 0x10 ], m10, sizeof( m10 ) );	/* the program's copies */
	memcpy( &m_vf_r0[ 0x50 ], m50, sizeof( m50 ) );
	for( i = 15; i >= 0; i-- )
	{
		float *v = vf_vec( i );
		int far = 1;
		if( tgp_lt( tgp_neg( v[ 10 ] ) ) )
		{
			float t = vf_r( ( n & mask ) ? 0x3b : 0x3a );
			vf_mul( v, m10, r );
			if( tgp_lt( u2f( f2u( r[ 9 ] ) & 0x7fffffff ) - t ) && tgp_lt( u2f( f2u( r[ 11 ] ) & 0x7fffffff ) - t ) )
			{
				fl &= ~mask;
				far = 0;
			}
		}
		if( far )
		{
			fl |= mask;
			vf_mul( v, m50, r );
		}
		memcpy( v, r, sizeof( r ) );
		mask <<= 1;
	}
	m_vf_r0[ ( 0x3c + n ) & 0xff ] = fl;
}

/* 0x4a0: the radius list follows, $37 words: they go to 0x2b0 and to the
   radius word of the list records, current and previous */
static void f98_word( UINT32 v, UINT32 i )
{
	if( 0xb0 + i < 0x200 )
		vf_dram_w( 0xb0 + i, v );
	m_copro_ram_data[ m_vf_list_adr[ 0 ] & ( DS - 1 ) ] = v;
	m_vf_list_adr[ 0 ] = ram_adr_next( m_vf_list_adr[ 0 ] );
	m_copro_ram_data[ m_vf_list_adr[ 1 ] & ( DS - 1 ) ] = v;
	m_vf_list_adr[ 1 ] = ram_adr_next( m_vf_list_adr[ 1 ] );
	m_copro_hle_ram_scan_adr = m_vf_list_adr[ 0 ];
}

static void f98_vf(void)
{
	UINT32 a = fifoin_pop();
	m_vf_list_adr[ 0 ] = m_vf_r0[ a ? 0x35 : 0x34 ] + 0x30003;
	m_vf_list_adr[ 1 ] = m_vf_list_adr[ 0 ] + m_vf_r0[ 0x36 ];
	m_copro_hle_ram_scan_adr = m_vf_list_adr[ 0 ];
	m_copro_hle_active_list_length = m_vf_r0[ 0x37 ];
	m_copro_hle_active_list_pos = 0;
	m_list_sink = 1;
}

/* 0x5e1: the cells (bits 16-23 of the float) each sphere covers on one
   axis between its previous position (e) and its current one (c) */
static UINT32 vf_cell( float v )
{
	return ( f2u( v ) >> 16 ) & 0xff;
}

static void vf_list_read( UINT32 base, UINT32 axis, UINT32 n )
{
	UINT32 i;
	m_copro_hle_ram_scan_adr = base + 0x30003;
	for( i = 0; i < n; i++ )
		m_vf_r0[ ( 0x10 + i ) & 0xff ] = ram_get_i();
	m_copro_hle_ram_scan_adr = base + 0x30000 + axis;
	for( i = 0; i < n; i++ )
		m_vf_r0[ ( 0xc0 + i ) & 0xff ] = ram_get_i();
	m_copro_hle_ram_scan_adr = base + m_vf_r0[ 0x36 ] + 0x30000 + axis;
	for( i = 0; i < n; i++ )
		m_vf_r0[ ( 0xe0 + i ) & 0xff ] = ram_get_i();
	for( i = 0; i < n; i++ )
	{
		float c = vf_r( ( 0xc0 + i ) & 0xff ), e = vf_r( ( 0xe0 + i ) & 0xff ), r = vf_r( ( 0x10 + i ) & 0xff );
		UINT32 lo, hi;
		if( tgp_led( c - e ) )
		{
			lo = vf_cell( c - r );
			hi = vf_cell( e + r );
		}
		else
		{
			hi = vf_cell( c + r );
			lo = vf_cell( e - r );
		}
			m_vf_r0[ ( 0xc0 + i ) & 0xff ] = lo;
		m_vf_r0[ ( 0xe0 + i ) & 0xff ] = hi;
	}
}

/* 0x4bb: which spheres of the second list each one of the first list may
   touch, axis by axis: cell bitmaps of the second list at 0x2b0 (lowest
   cell, then or-ed upwards) and 0x330 (highest cell, or-ed downwards),
   a bit per sphere; the three axes and-ed into $60 + i */
static void f99_vf(void)
{
	UINT32 n = m_vf_r0[ 0x37 ], i, k, axis;
	for( axis = 0; axis < 3; axis++ )
	{
		UINT32 mask = 1, acc;
		vf_list_read( m_vf_r0[ 0x35 ], axis, n );
		for( k = 0; k < 0x100; k++ )
			vf_dram_w( 0xb0 + k, 0 );
		for( i = 0; i < n; i++ )
		{
			UINT32 lo = 0xb0 + ( m_vf_r0[ ( 0xc0 + i ) & 0xff ] & 0x7f );
			UINT32 hi = 0x130 + ( m_vf_r0[ ( 0xe0 + i ) & 0xff ] & 0x7f );
			vf_dram_w( lo, vf_dram_r( lo ) | mask );
			vf_dram_w( hi, vf_dram_r( hi ) | mask );
			mask <<= 1;
		}
		acc = 0;
		for( k = 0; k < 0x80; k++ )
		{
			acc |= vf_dram_r( 0xb0 + k );
			vf_dram_w( 0xb0 + k, acc );
		}
		acc = 0;
		for( k = 0; k < 0x80; k++ )
		{
			acc |= vf_dram_r( 0x1af - k );
			vf_dram_w( 0x1af - k, acc );
		}
		vf_list_read( m_vf_r0[ 0x34 ], axis, n );
		for( i = 0; i < n; i++ )
		{
			m_vf_r0[ ( 0xc0 + i ) & 0xff ] = vf_dram_r( 0x130 + ( m_vf_r0[ ( 0xc0 + i ) & 0xff ] & 0x7f ) );
			m_vf_r0[ ( 0xe0 + i ) & 0xff ] = vf_dram_r( 0xb0 + ( m_vf_r0[ ( 0xe0 + i ) & 0xff ] & 0x7f ) );
		}
		for( i = 0; i < n; i++ )
			m_vf_r0[ ( 0x60 + 0x20 * axis + i ) & 0xff ] = m_vf_r0[ ( 0xc0 + i ) & 0xff ] & m_vf_r0[ ( 0xe0 + i ) & 0xff ];
	}
	for( i = 0; i < n; i++ )
		m_vf_r0[ ( 0x60 + i ) & 0xff ] &= m_vf_r0[ ( 0x80 + i ) & 0xff ] & m_vf_r0[ ( 0xa0 + i ) & 0xff ];
}

/* 0x5f9: the result, $37 words from $60 */
static void f100_vf(void)
{
	UINT32 i;
	for( i = 0; i < m_vf_r0[ 0x37 ]; i++ )
		fifoout_push( m_vf_r0[ ( 0x60 + i ) & 0xff ] );
}

/* 0x711: the camera parameters, 25 words from the copro RAM to $50-$68 */
static void f103_vf(void)
{
	int i;
	m_copro_hle_ram_scan_adr = fifoin_pop();
	for( i = 0; i < 25; i++ )
		m_vf_r0[ 0x50 + i ] = ram_get_i();
}

/* 0x605: the camera. Its position $69-$6b moves by the given step, stays
   above the floor ($56) and out of a sphere ($57-$5a, or $5f-$62 when
   far from $5b-$5e), then keeps the distance a0 from the target $66-$68;
   the view matrix turns to look at the target, the target follows the
   position. Results: the position and how far it moved ($6f-$71) */
static void f102_vf(void)
{
	float v[ 3 ], l, k, t;
	UINT32 ang1, ang2;
	int i;
	m_vf_r0[ 0x72 ] = fifoin_pop();
	m_vf_r0[ 0x73 ] = fifoin_pop();
	m_vf_r0[ 0x69 ] = fifoin_pop();
	m_vf_r0[ 0x6a ] = fifoin_pop();
	m_vf_r0[ 0x6b ] = fifoin_pop();
	m_vf_r0[ 0x6f ] = fifoin_pop();
	m_vf_r0[ 0x70 ] = fifoin_pop();
	m_vf_r0[ 0x71 ] = fifoin_pop();
	for( i = 0; i < 3; i++ )
	{
		m_vf_r0[ 0x6c + i ] = m_vf_r0[ 0x69 + i ];
		vf_w( 0x69 + i, vf_r( 0x6f + i ) + vf_r( 0x69 + i ) );
	}
	if( tgp_lt( vf_r( 0x6a ) - vf_r( 0x56 ) ) )
		m_vf_r0[ 0x6a ] = m_vf_r0[ 0x56 ];
	else
	{
		int c = 0x5a;
		for( i = 0; i < 3; i++ )
			v[ i ] = vf_r( 0x69 + i ) - vf_r( 0x5b + i );
		if( !tgp_lt( vf_len( v[ 0 ], v[ 1 ], v[ 2 ] ) - vf_r( 0x5e ) ) )
		{
			for( i = 0; i < 3; i++ )
				v[ i ] = vf_r( 0x69 + i ) - vf_r( 0x5f + i );
			c = 0x62;
		}
		else
			for( i = 0; i < 3; i++ )
				v[ i ] = vf_r( 0x69 + i ) - vf_r( 0x57 + i );
		l = vf_len( v[ 0 ], v[ 1 ], v[ 2 ] );
		t = tgp_neg( l - vf_r( c ) );
		if( !tgp_led( t ) )
		{
			k = t / l;
			for( i = 0; i < 3; i++ )
				vf_w( 0x69 + i, vf_r( 0x69 + i ) + v[ i ] * k );
		}
	}
	for( i = 0; i < 3; i++ )
		v[ i ] = vf_r( 0x69 + i ) - vf_r( 0x66 + i );
	k = vf_r( 0x72 ) / vf_len( v[ 0 ], v[ 1 ], v[ 2 ] );
	for( i = 0; i < 3; i++ )
	{
		v[ i ] = v[ i ] * k;
		vf_w( 0x69 + i, v[ i ] + vf_r( 0x66 + i ) );
	}
	{
		float s = u2f( table_sincos( m_vf_r0[ 0x65 ] ) );
		float c = u2f( table_sincos( m_vf_r0[ 0x65 ] + 0x4000 ) );
		float x = c * v[ 0 ] - s * v[ 2 ];
		float z = c * v[ 2 ] + s * v[ 0 ];
		ang1 = tgp_atan( x, v[ 1 ] ) + 0x4000;
		ang2 = tgp_atan( tgp_sqrt_vf( x * x + v[ 1 ] * v[ 1 ] ), z );
	}
	{
		float ay = u2f( f2u( v[ 1 ] ) & 0x7fffffff );
		float e = vf_r( 0x52 ) * ay;
		float f = vf_r( 0x53 ) * ay;
		float s = u2f( table_sincos( m_vf_r0[ 0x64 ] ) );
		float c = u2f( table_sincos( m_vf_r0[ 0x64 ] + 0x4000 ) );
		float a1 = vf_r( 0x73 );
		vf_w( 0x6f, vf_r( 0x55 ) * ( ( ( c * a1 + vf_r( 0x69 ) ) - vf_r( 0x6c ) ) + e ) );
		vf_w( 0x70, vf_r( 0x54 ) * ( ( ( vf_r( 0x51 ) + vf_r( 0x6a ) ) - vf_r( 0x6d ) ) + vf_r( 0 ) ) );
		vf_w( 0x71, vf_r( 0x55 ) * ( ( ( s * a1 + vf_r( 0x6b ) ) - vf_r( 0x6e ) ) + f ) );
	}
	{
		float s = u2f( table_sincos( m_vf_r0[ 0x63 ] ) );
		float c = u2f( table_sincos( m_vf_r0[ 0x63 ] + 0x4000 ) );
		float x = c * vf_r( 0x66 ) - s * vf_r( 0x68 );
		float z = c * vf_r( 0x68 ) + s * vf_r( 0x66 );
		float r[ 3 ];
		vf_transform( m_cmat, x, vf_r( 0x67 ), z, r );
		memcpy( &m_cmat[ 9 ], r, sizeof( r ) );
	}
	vf_rot( 3, 6, ang1 );
	vf_rot( 0, 3, ang2 );
	for( i = 0; i < 3; i++ )
		m_vf_r0[ 0x66 + i ] = m_vf_r0[ 0x69 + i ];
	fifoout_push( m_vf_r0[ 0x69 ] );
	fifoout_push( m_vf_r0[ 0x6a ] );
	fifoout_push( m_vf_r0[ 0x6b ] );
	fifoout_push( m_vf_r0[ 0x6f ] );
	fifoout_push( m_vf_r0[ 0x70 ] );
	fifoout_push( m_vf_r0[ 0x71 ] );
}


/* a function the program leaves at once */
static void nop_tgp(void)
{
}

/* 0x0fb: the vector over its length */
static void normalize_vf(void)
{
	float v[ 3 ], l;
	int i;
	for( i = 0; i < 3; i++ )
		v[ i ] = fifoin_pop_f();
	l = tgp_sqrt_vf( vf_dot( v, v ) );
	for( i = 0; i < 3; i++ )
		fifoout_push_f( v[ i ] / l );
}

/* 0x193 */
static void acc_div_vf(void)
{
	m_acc = m_acc / fifoin_pop_f();
}

/* 0x302: the translation of the current matrix */
static void matrix_settrans_vf(void)
{
	m_cmat[ 9 ] = fifoin_pop_f();
	m_cmat[ 10 ] = fifoin_pop_f();
	m_cmat[ 11 ] = fifoin_pop_f();
}


static const struct tgp_function ftab_vr[] = {
	{ fadd,            2 }, /* 0x00 */ /* 0b5 09b */
	{ fsub,            2 },            /* 0ba 0a0 */
	{ fmul,            2 },            /* 0bf 0a5 */
	{ fdiv,            2 },            /* 0c4 0aa */
	{ NULL,                        0 },            /* 0d2 0b9 */
	{ matrix_push,     0 },            /* 0e3 0cc */
	{ matrix_pop,      0 },            /* 0f7 0e0 */
	{ matrix_write,   12 },            /* 106 0ef */
	{ clear_stack,     0 },            /* 10a 0f4 */
	{ matrix_mul,     12 },            /* 10e 0f8 */
	{ anglev,          2 },            /* 154 13f */
	{ NULL,                        0 },            /* 15d 149 */
	{ NULL,                        0 },            /* 19f 18a */
	{ track_select,    1 },            /* 1b8 1a5 */
	{ load_base,       4 },            /* 1bb 1a8 */
	{ anglep,          4 },            /* 1c1 1ae */

	{ matrix_ident,    0 }, /* 0x10 */ /* 1d2 1c1 */
	{ matrix_read,     0 },            /* 1d9 1c8 */
	{ matrix_trans,    3 },            /* 1dd 1d1 */
	{ matrix_scale,    3 },            /* 1f3 1e8 */
	{ matrix_rotx,     1 },            /* 20a 200 */
	{ matrix_roty,     1 },            /* 223 21a */
	{ matrix_rotz,     1 },            /* 23b 233 */
	{ NULL,                        0 },            /* 253 24c */
	{ track_read_quad, 1 },            /* 2aa 2a8 */
	{ NULL,                        0 },            /* 2b0 2b0 */
	{ transform_point, 3 },            /* 2b2 2b4 */
	{ fsin_m1,         1 },            /* 2cb 2d7 */
	{ fcos_m1,         1 },            /* 2ce 2df */
	{ fsinm_m1,        2 },            /* 2d1 2e7 */
	{ fcosm_m1,        2 },            /* 2d7 2f1 */
	{ distance3,       6 },            /* 2dd 2fb */

	{ NULL,                        0 }, /* 0x20 */ /* 2ff 31e */
	{ NULL,                        0 },            /* 2ff 31e */
	{ NULL,                        0 },            /* 300 31f */
	{ NULL,                        0 },            /* 304 323 */
	{ acc_set,         1 },            /* 308 327 */
	{ acc_get,         0 },            /* 30a 32a */
	{ acc_add,         1 },            /* 30c 32e */
	{ acc_sub,         1 },            /* 311 333 */
	{ acc_mul,         1 },            /* 316 338 */
	{ acc_div,         1 },            /* 31b 33d */
	{ f42,             3 },            /* 329 34b */
	{ f43,             6 },            /* 36c 38f */
	{ track_read_tri,  1 },            /* 3c2 3e2 */
	{ fsqrt,           1 },            /* 3c7 3e9 */
	{ vlength,         3 },            /* 3cf 3f2 */
	{ NULL,                        0 },            /* 3ef 411 */

	{ track_read_info, 1 }, /* 0x30 */ /* 410 431 */
	{ colbox_set,     12 },            /* 413 436 */
	{ colbox_test,     3 },            /* 417 43b */
	{ NULL,                        0 },            /* 43b 463 */
	{ NULL,                        0 },            /* 44d 478 */
	{ NULL,                        0 },            /* 452 47d */
	{ track_lookup,    4 },            /* 457 482 */
	{ NULL,                        0 },            /* 51a 522 */
	{ NULL,                        0 },            /* 521 527 */
	{ NULL,                        0 },            /* 52f 535 */
	{ NULL,                        0 },            /* 53d 543 */
	{ NULL,                        0 },            /* 545 54c */
	{ NULL,                        0 },            /* 558 55e */
	{ NULL,                        0 },            /* 559 55f */
	{ NULL,                        0 },            /* 5c6 5cd */
	{ NULL,                        0 },            /* 5e9 5f4 */

	{ col_setcirc,     3 }, /* 0x40 */ /* 5f3 5fe */
	{ col_testpt,      2 },            /* 5fa 603 */
	{ col_testpt2,     2 },            /* 615 61e */
	{ distance,        4 },            /* 631 639 */
	{ NULL,                        0 },            /* 63f 648 */
	{ NULL,                        0 },            /* 643 64e */
	{ NULL,                        0 },            /* 64b 657 */
	{ car_move,        4 },            /* 661 66e */
};

/* 315-5724 (Virtua Fighter): its own versions where its program differs
   from 315-5573 */
static const struct tgp_function ftab_vf[] = {
	{ fadd,            2 }, /* 0x00 */
	{ fsub,            2 },
	{ fmul,            2 },
	{ fdiv_vf,         2 },
	{ nop_tgp,         0 },
	{ matrix_push,     0 },
	{ matrix_pop_vf,   0 },
	{ matrix_write,   12 },
	{ clear_stack,     0 },
	{ matrix_mul_vf,  12 },
	{ anglev_vf,       2 },
	{ nop_tgp,         0 },
	{ normalize_vf,    3 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ anglep_vf,       4 },

	{ matrix_ident,    0 }, /* 0x10 */
	{ matrix_read,     0 },
	{ matrix_trans_vf, 3 },
	{ matrix_scale,    3 },
	{ matrix_rotx,     1 },
	{ matrix_roty,     1 },
	{ matrix_rotz,     1 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ transform_point_vf, 3 },
	{ fsin_m1,         1 },
	{ fcos_m1,         1 },
	{ fsinm_m1,        2 },
	{ fcosm_m1,        2 },
	{ distance3,       6 },

	{ nop_tgp,         0 }, /* 0x20 */
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ acc_set,         1 },
	{ acc_get,         0 },
	{ acc_add,         1 },
	{ acc_sub,         1 },
	{ acc_mul,         1 },
	{ acc_div_vf,      1 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ fsqrt_vf,        1 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },

	{ nop_tgp,         0 }, /* 0x30 */
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },

	{ nop_tgp,         0 }, /* 0x40 */
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ distance,        4 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ nop_tgp,         0 },
	{ cpa_vf,         12 },
	{ nop_tgp,         0 },
	{ vmat_store_vf,   1 },
	{ vmat_restore_vf, 1 },
	{ NULL,                        0 },
	{ vmat_mul_vf,     2 },
	{ vmat_read_vf,    1 },
	{ matrix_unrot,    0 },

	{ f80_vf,          0 }, /* 0x50 */
	{ matrix_settrans_vf, 3 },
	{ matrix_rtrans,   0 },
	{ NULL,                        0 },
	{ vmat_save_vf,    1 },
	{ vmat_load_vf,    1 },
	{ ram_setadr,      1 },
	{ groundbox_test_vf, 3 },
	{ NULL,                        0 },
	{ f89_vf,          4 },
	{ NULL,                        0 },
	{ NULL,                        0 },
	{ f92_vf,          4 },
	{ f93_vf,          1 },
	{ f94_vf,          1 },
	{ vmat_flatten_vf, 0 },

	{ vmat_load1_vf,   1 }, /* 0x60 */
	{ ram_trans_vf,    0 },
	{ f98_vf,          1 },
	{ f99_vf,          0 },
	{ f100_vf,         0 },
	{ groundbox_set_vf, 7 },
	{ f102_vf,         8 },
	{ f103_vf,         1 }
};

/* Used in swa scene 1 and unemulated: */
/*   f49_swa */

static const struct tgp_function ftab_swa[] = {
	{ fadd,            2 },  /* 0x00 */
	{ fsub,            2 },
	{ fmul,            2 },
	{ fdiv,            2 },
	{ f04_swa,         2 },
	{ matrix_push_swa, 0 },
	{ matrix_pop,      0 },
	{ matrix_write,   12 },
	{ clear_stack,     0 },
	{ matrix_mul,     12 },
	{ anglev,          2 },
	{ triangle_normal, 9 },
	{ normalize_swa,   3 },
	{ acc_seti_swa,    1 },
	{ load_base,       4 },
	{ transpose,       0 },

	{ matrix_ident,    0 }, /* 0x10 */
	{ matrix_read,     0 },
	{ matrix_trans,    3 },
	{ matrix_scale,    3 },
	{ matrix_rotx,     1 },
	{ matrix_roty_swa, 1 },
	{ matrix_rotz,     1 },
	{ acc_addand_swa,  0 },
	{ intercept,       7 },
	{ stack_depth_swa, 0 },
	{ transform_point, 3 },
	{ fsin_m1,         1 },
	{ fcos_m1,         1 },
	{ fsinm_m1,        2 },
	{ fcosm_m1,        2 },
	{ distance3,       6 },

	{ acc_mulpop_swa,  0 }, /* 0x20 */
	{ NULL,                        0 },
	{ ftoi,            1 },
	{ itof,            1 },
	{ acc_set_swa,     1 },
	{ acc_get_swa,     0 },
	{ acc_add,         1 },
	{ acc_sub,         1 },
	{ acc_mul,         1 },
	{ acc_div,         1 }, /* not used ? */
	{ xyz2rqf,         3 },
	{ f43_swa,         3 },
	{ matrix_sdir,     3 },
	{ fsqrt_swa,       1 },
	{ vlength,         3 },
	{ f47,             3 },

	{ f30_swa,         1 }, /* 0x30 */
	{ f49_swa,         6 },
	{ f50_swa,         4 },
	{ acc_rsub_swa,    0 },
	{ f52,             0 },
	{ matrix_rdir,     3 },
	{ nop_swa,         0 },
	{ nop_swa,         0 },
	{ f56,             7 },
	{ int_point,       0 },
	{ matrix_readt,    0 },
	{ acc_geti,        0 },
	{ int_normal,      0 },
	{ tabsum_swa,      0 },
	{ romsum_swa,      3 },
	{ nop_swa,         0 },
	{ select_swa,      1 },

	{ push_and_ident,  0 }, /* 0x40 */
	{ nop_swa,         0 },
	{ catmull_rom,    13 }
};



/*************************************
 *  Oracle: the HLE checked against the real TGP

    Both get every word the V60 pushes. The HLE runs a function as soon
    as its arguments are in and keeps a record of it: where its function
    word sits in the input stream, its arguments and its results. The
    DSP takes its input in order: when it takes the function word of the
    next record, the results it pushed since the previous one belong to
    the previous call and are compared with the record. So the pairing
    holds even when a function gives a wrong number of results.
 *************************************/

#define ORACLE_CALLS	256
#define ORACLE_ARGS		16
#define ORACLE_WORDS	64

struct oracle_call
{
	UINT32 fn;					/* function number */
	UINT32 start;				/* input index of its function word */
	UINT32 args[ ORACLE_ARGS ];
	int nargs;
	UINT32 out[ ORACLE_WORDS ];
	int nout;
	offs_t pc;
	int unimpl;
#ifdef MODEL1_TGP_DEV
	UINT32 ram[ 0x200 ];		/* 315-5724: the HLE's TGP RAM after the call */
	UINT32 r0[ 0x100 ];
	INT32 depth, pos;
#endif
};

static struct oracle_call oracle_ring[ ORACLE_CALLS ];
static int oracle_head, oracle_tail;	/* records written by the HLE / not yet taken by the DSP */
static int oracle_cur;					/* record the DSP is running, -1 none */
static int oracle_cur_pending;			/* ... the HLE has not run it yet (arguments to come) */
static UINT32 oracle_pending_start;		/* input index of a function word waiting for its arguments */
static int oracle_pending_valid;
static UINT32 oracle_in_count;			/* words pushed to the TGP */
static UINT32 oracle_dsp_pops;			/* words the DSP took */
static UINT32 oracle_dsp_out[ ORACLE_WORDS ];
static int oracle_dsp_nout;

static UINT32 oracle_calls[ 256 ], oracle_exact[ 256 ], oracle_close[ 256 ], oracle_bad[ 256 ];
static UINT32 oracle_count_bad[ 256 ], oracle_unimpl[ 256 ];
#ifdef MODEL1_TGP_DEV
static UINT32 oracle_ram_bad[ 256 ];
static int oracle_ram_logged[ 256 ];
/* 315-5724: the variables of the TGP's RAM 0x000-0x0ff the HLE keeps */
static const UINT16 oracle_vf_r0_ranges[][ 2 ] = { { 0x34, 0x100 }, { 0, 0 } };
#endif
static int oracle_logged[ 256 ];

static void oracle_hle_call( UINT32 fn, int nargs, int unimpl, int out_before )
{
	struct oracle_call *c = &oracle_ring[ oracle_head ];
	int i;
	c->fn = fn & 0xff;
	c->start = oracle_in_count - hle_in.count;
	c->nargs = nargs < ORACLE_ARGS ? nargs : ORACLE_ARGS;
	for( i = 0; i < c->nargs; i++ )
		c->args[ i ] = fifo_peek( &hle_in, i + 1 );
	c->pc = m_pushpc;
	c->unimpl = unimpl;
	oracle_pending_valid = 0;
	c->nout = 0;
	oracle_calls[ c->fn ]++;
	if( unimpl )
		oracle_unimpl[ c->fn ]++;
	(void)out_before;
}

/* after the function ran: its results move from hle_out into the record */
static void oracle_hle_done( void )
{
	struct oracle_call *c = &oracle_ring[ oracle_head ];
	int ok;
#ifdef MODEL1_TGP_DEV
	if( model1_tgp_table == MODEL1_TGP_TABLE_VF )
	{
		int i;
		for( i = 0; i < 0x200; i++ )
			c->ram[ i ] = f2u( m_dram[ i ] );
		memcpy( c->r0, m_vf_r0, sizeof( c->r0 ) );
		c->depth = m_mat_depth;
		c->pos = m_mat_stack_pos;
	}
#endif
	while( hle_out.count )
	{
		UINT32 v = fifo_pop( &hle_out, &ok );
		if( c->nout < ORACLE_WORDS )
			c->out[ c->nout++ ] = v;
	}
	if( oracle_cur_pending && oracle_cur == oracle_head )
	{
		/* the DSP already started this one */
		oracle_cur_pending = 0;
		oracle_tail = ( oracle_head + 1 ) % ORACLE_CALLS;
	}
	oracle_head = ( oracle_head + 1 ) % ORACLE_CALLS;
	if( oracle_head == oracle_tail )
	{
		/* the DSP is far behind: drop the oldest record */
		oracle_tail = ( oracle_tail + 1 ) % ORACLE_CALLS;
		if( oracle_cur == oracle_tail )
			oracle_cur = -1;
	}
}

/* the call that took a list ends with its last word: its RAM again */
static void oracle_list_done( void )
{
#ifdef MODEL1_TGP_DEV
	struct oracle_call *c = &oracle_ring[ ( oracle_head + ORACLE_CALLS - 1 ) % ORACLE_CALLS ];
	if( model1_tgp_table == MODEL1_TGP_TABLE_VF )
	{
		int i;
		for( i = 0; i < 0x200; i++ )
			c->ram[ i ] = f2u( m_dram[ i ] );
		memcpy( c->r0, m_vf_r0, sizeof( c->r0 ) );
	}
#endif
}

static UINT32 ulp_diff( UINT32 a, UINT32 b )
{
	INT32 ia = ( a & 0x80000000 ) ? (INT32)( 0x80000000 - a ) : (INT32)a;
	INT32 ib = ( b & 0x80000000 ) ? (INT32)( 0x80000000 - b ) : (INT32)b;
	return ia > ib ? (UINT32)( ia - ib ) : (UINT32)( ib - ia );
}

/* nearly the same value: a rounding difference, not a different result */
static int oracle_near( UINT32 a, UINT32 b )
{
	double fa = u2f( a ), fb = u2f( b ), d = fabs( fa - fb ), m = fabs( fa ) > fabs( fb ) ? fabs( fa ) : fabs( fb );
	if( ulp_diff( a, b ) <= 16 )
		return 1;
	return d <= 1e-4 * m || d < 1e-6;
}

static void oracle_log( const char *what, const struct oracle_call *c )
{
	char buf[ 2048 ];
	int i, n = 0;
	n += sprintf( buf + n, "[TGP oracle] %s f%02x (pc %06x) args", what, c->fn, c->pc );
	for( i = 0; i < c->nargs; i++ )
		n += sprintf( buf + n, " %08x", c->args[ i ] );
	n += sprintf( buf + n, "\n[TGP oracle]     dsp(%d)", oracle_dsp_nout );
	for( i = 0; i < oracle_dsp_nout && i < 24; i++ )
		n += sprintf( buf + n, " %08x", oracle_dsp_out[ i ] );
	n += sprintf( buf + n, "\n[TGP oracle]     hle(%d)", c->nout );
	for( i = 0; i < c->nout && i < 24; i++ )
		n += sprintf( buf + n, " %08x", c->out[ i ] );
	log_cb( RETRO_LOG_INFO, "%s\n", buf );
}

/* the DSP finished the call it was running */
static void oracle_dsp_done( void )
{
	struct oracle_call *c;
	int i, close = 0, bad = 0;

	if( oracle_cur < 0 || oracle_cur_pending )
	{
		oracle_cur = -1;
		oracle_cur_pending = 0;
		return;
	}
	c = &oracle_ring[ oracle_cur ];
	if( c->nout != oracle_dsp_nout )
	{
		oracle_count_bad[ c->fn ]++;
		if( oracle_logged[ c->fn ] < 6 )
		{
			oracle_logged[ c->fn ]++;
			oracle_log( c->unimpl ? "unimplemented" : "count", c );
		}
	}
	else
	{
		for( i = 0; i < c->nout; i++ )
			if( c->out[ i ] != oracle_dsp_out[ i ] )
			{
				if( oracle_near( c->out[ i ], oracle_dsp_out[ i ] ) )
					close = 1;
				else
					bad = 1;
			}
		if( bad )
		{
#ifdef MODEL1_TGP_DEV
			{
				/* MODEL1_TGP_HIST=fn: the calls before its first mismatch */
				static int done;
				const char *h = getenv( "MODEL1_TGP_HIST" );
				if( h && !done && strtoul( h, NULL, 16 ) == c->fn )
				{
					int k;
					done = 1;
					log_cb( RETRO_LOG_INFO, "[TGP hist] HLE depth %d pos %d DSP $7 %x $9 %x\n", m_mat_depth, m_mat_stack_pos, tgp_data_ram[ 7 ], tgp_data_ram[ 9 ] );
					{ int q; for( q = 0; q < 4; q++ ) log_cb( RETRO_LOG_INFO, "[TGP hist] slot %d dsp %08x %08x %08x hle %08x %08x %08x\n", q, tgp_data_ram[0x330+12*q+9], tgp_data_ram[0x330+12*q+10], tgp_data_ram[0x330+12*q+11], f2u(MAT_SLOT(q)[9]), f2u(MAT_SLOT(q)[10]), f2u(MAT_SLOT(q)[11]) ); }
					for( k = 40; k > 0; k-- )
					{
						const struct oracle_call *o = &oracle_ring[ ( oracle_cur + ORACLE_CALLS - k ) % ORACLE_CALLS ];
						int j;
						char b[ 512 ];
						int n = sprintf( b, "[TGP hist] f%02x pc %06x args", o->fn, o->pc );
						for( j = 0; j < o->nargs && j < 8; j++ )
							n += sprintf( b + n, " %08x", o->args[ j ] );
						n += sprintf( b + n, " out" );
						for( j = 0; j < o->nout && j < 6; j++ )
							n += sprintf( b + n, " %08x", o->out[ j ] );
						log_cb( RETRO_LOG_INFO, "%s\n", b );
					}
				}
			}
#endif
			oracle_bad[ c->fn ]++;
			if( oracle_logged[ c->fn ] < 6 )
			{
				oracle_logged[ c->fn ]++;
				oracle_log( "mismatch", c );
			}
		}
		else if( close )
		{
			oracle_close[ c->fn ]++;
			if( oracle_logged[ c->fn ] < 6 )
			{
				oracle_logged[ c->fn ]++;
				oracle_log( "close", c );
			}
		}
		else
			oracle_exact[ c->fn ]++;
	}
#ifdef MODEL1_TGP_DEV
	/* 315-5724: the RAM the HLE keeps against the real TGP's, after the call */
	if( model1_tgp_table == MODEL1_TGP_TABLE_VF )
	{
		int i, k, first = -1, nd = 0, first0 = -1;
		for( i = 0x30; i < 0x1b0; i++ )
			if( c->ram[ i ] != tgp_data_ram[ 0x200 + i ] )
			{
				if( first < 0 ) first = 0x200 + i;
				nd++;
			}
		if( (UINT32)c->depth != tgp_data_ram[ 0x30 ] || (UINT32)( DRAM_MAT_VF + 16 * c->pos ) != tgp_data_ram[ 0x31 ] )
		{
			if( first < 0 ) first = 0x30;
			nd++;
		}
		for( k = 0; oracle_vf_r0_ranges[ k ][ 1 ]; k++ )
			for( i = oracle_vf_r0_ranges[ k ][ 0 ]; i < oracle_vf_r0_ranges[ k ][ 1 ]; i++ )
				if( c->r0[ i ] != tgp_data_ram[ i ] )
				{
					if( first < 0 ) first = i;
					if( first0 < 0 ) first0 = i;
					nd++;
				}
		if( nd )
		{
			oracle_ram_bad[ c->fn ]++;
			if( oracle_ram_logged[ c->fn ] < 4 )
			{
				oracle_ram_logged[ c->fn ]++;
				log_cb( RETRO_LOG_INFO, "[TGP oracle] ram f%02x (pc %06x): %d words differ, first %03x dsp %08x hle %08x (depth dsp %d hle %d, ptr dsp %x hle %x)\n",
					c->fn, c->pc, nd, first, tgp_data_ram[ first ],
					first >= 0x200 ? c->ram[ first - 0x200 ] : c->r0[ first ],
					tgp_data_ram[ 0x30 ], c->depth, tgp_data_ram[ 0x31 ], DRAM_MAT_VF + 16 * c->pos );
				if( first0 >= 0 )
					log_cb( RETRO_LOG_INFO, "[TGP oracle]     first in 000-0ff: %02x dsp %08x hle %08x\n", first0, tgp_data_ram[ first0 ], c->r0[ first0 ] );
				oracle_log( "ram", c );
			}
		}
	}
#endif
	oracle_cur = -1;
}

/* the DSP took an input word */
static void oracle_dsp_pop( void )
{
	UINT32 idx = oracle_dsp_pops++;
	while( oracle_tail != oracle_head && (INT32)( oracle_ring[ oracle_tail ].start - idx ) < 0 )
		oracle_tail = ( oracle_tail + 1 ) % ORACLE_CALLS;	/* a call the DSP never started */
	if( oracle_tail != oracle_head && oracle_ring[ oracle_tail ].start == idx )
	{
		oracle_dsp_done();
		oracle_cur = oracle_tail;
		oracle_tail = ( oracle_tail + 1 ) % ORACLE_CALLS;
		oracle_dsp_nout = 0;
	}
	else if( oracle_tail == oracle_head && oracle_pending_valid && oracle_pending_start == idx )
	{
		/* a function whose arguments are still coming */
		oracle_dsp_done();
		oracle_cur = oracle_head;
		oracle_cur_pending = 1;
		oracle_dsp_nout = 0;
	}
}

/* the DSP pushed a result */
static void oracle_dsp_push( UINT32 v )
{
	if( oracle_dsp_nout < ORACLE_WORDS )
		oracle_dsp_out[ oracle_dsp_nout++ ] = v;
}

void model1_tgp_oracle_report( void )
{
	int f;
	if( model1_tgp_mode != MODEL1_TGP_ORACLE )
		return;
	for( f = 0; f < 256; f++ )
		if( oracle_calls[ f ] )
		{
			UINT32 ram_bad = 0;
#ifdef MODEL1_TGP_DEV
			ram_bad = oracle_ram_bad[ f ];
#endif
			log_cb( RETRO_LOG_INFO, "[TGP oracle]   f%02x calls %u exact %u close %u wrong %u count %u ram %u%s\n",
				f, oracle_calls[ f ], oracle_exact[ f ], oracle_close[ f ], oracle_bad[ f ], oracle_count_bad[ f ],
				ram_bad, oracle_unimpl[ f ] ? " UNIMPLEMENTED" : "" );
		}
}

/* compare the RAM the two TGPs keep (called once a frame) */
void model1_tgp_oracle_frame( void )
{
	static int frames;
	int i, diff = 0, first = -1;
	if( model1_tgp_mode != MODEL1_TGP_ORACLE )
		return;
	for( i = 0; i < DS; i++ )
		if( copro_ram_lle[ i ] != copro_ram_hle[ i ] )
		{
			if( first < 0 ) first = i;
			diff++;
		}
#ifdef MODEL1_TGP_DEV
	if( diff )
	{
		static int shown;
		if( shown < 3 )
		{
			int n = 0;
			shown++;
			for( i = 0; i < DS && n < 16; i++ )
				if( copro_ram_lle[ i ] != copro_ram_hle[ i ] )
				{
					log_cb( RETRO_LOG_INFO, "[TGP oracle] copro RAM frame %d: %04x dsp %08x hle %08x\n", frames, i, copro_ram_lle[ i ], copro_ram_hle[ i ] );
					n++;
				}
		}
	}
#endif
	if( diff && ( frames % 300 ) == 0 )
		log_cb( RETRO_LOG_INFO, "[TGP oracle] copro RAM: %d words differ, first at %04x (dsp %08x hle %08x)\n",
			diff, first, copro_ram_lle[ first ], copro_ram_hle[ first ] );
	if( ( ++frames % 600 ) == 0 )
		model1_tgp_oracle_report();
}


/*************************************
 *  HLE dispatch
 *************************************/

static void hle_call( const struct tgp_function *tab, UINT32 f )
{
	int ok;
	if( model1_tgp_mode == MODEL1_TGP_ORACLE )
		oracle_hle_call( f, tab[ f ].count, 0, hle_out.count );
	m_tgp_fword = fifo_pop( &hle_in, &ok );			/* the function word */
	tab[ f ].cb();
	if( model1_tgp_mode == MODEL1_TGP_ORACLE )
		oracle_hle_done();
}

static void hle_unimplemented( UINT32 f )
{
	int ok;
	LOG(( "TGP function %d unimplemented (%x)\n", f, m_pushpc ));
	if( model1_tgp_mode == MODEL1_TGP_ORACLE )
		oracle_hle_call( f, 0, 1, hle_out.count );
	fifo_pop( &hle_in, &ok );
	if( model1_tgp_mode == MODEL1_TGP_ORACLE )
		oracle_hle_done();
}

static void copro_hle_table( const struct tgp_function *tab, UINT32 n )
{
	UINT32 f;
	int ok;

	if( m_copro_hle_active_list_length )
	{
		while( m_copro_hle_active_list_pos < m_copro_hle_active_list_length && hle_in.count )
		{
			UINT32 v = fifo_pop( &hle_in, &ok );
			if( m_list_sink == 1 )
				f98_word( v, m_copro_hle_active_list_pos );
			m_copro_hle_active_list_pos++;
		}
		if( m_copro_hle_active_list_pos == m_copro_hle_active_list_length )
		{
			m_copro_hle_active_list_pos = m_copro_hle_active_list_length = 0;
			m_list_sink = 0;
			if( model1_tgp_mode == MODEL1_TGP_ORACLE )
				oracle_list_done();
		}
		if( !hle_in.count )
			return;
	}

	while( hle_in.count )
	{
		f = fifo_peek( &hle_in, 0 ) >> 23;	/* the exponent of the ieee float */
		if( f < n && tab[ f ].cb != NULL )
		{
			if( hle_in.count < tab[ f ].count + 1 )
			{
				oracle_pending_start = oracle_in_count - hle_in.count;
				oracle_pending_valid = 1;
				return;
			}
			hle_call( tab, f );
		}
		else
			hle_unimplemented( f );
		if( m_copro_hle_active_list_length )
			return;
	}
}

static void copro_hle_swa( void )
{
	UINT32 f;

	while( hle_in.count )
	{
		f = fifo_peek( &hle_in, 0 );
		if( f < sizeof( ftab_swa ) / sizeof( ftab_swa[ 0 ] ) && ftab_swa[ f ].cb != NULL )
		{
			if( hle_in.count < ftab_swa[ f ].count + 1 )
			{
				oracle_pending_start = oracle_in_count - hle_in.count;
				oracle_pending_valid = 1;
				return;
			}
			hle_call( ftab_swa, f );
		}
		else
			hle_unimplemented( f );
	}
}

static void copro_hle_run( void )
{
	if( model1_tgp_table == MODEL1_TGP_TABLE_SWA )
		copro_hle_swa();
	else if( model1_tgp_table == MODEL1_TGP_TABLE_VF )
		copro_hle_table( ftab_vf, sizeof( ftab_vf ) / sizeof( ftab_vf[ 0 ] ) );
	else
		copro_hle_table( ftab_vr, sizeof( ftab_vr ) / sizeof( ftab_vr[ 0 ] ) );
}


/*************************************
 *  V60 side
 *************************************/

READ16_HANDLER( model1_tgp_copro_adr_r )
{
	return m_v60_copro_ram_adr;
}

WRITE16_HANDLER( model1_tgp_copro_adr_w )
{
	COMBINE_DATA( &m_v60_copro_ram_adr );
}

READ16_HANDLER( model1_tgp_copro_ram_r )
{
	UINT16 r;
	if( !offset )
		r = copro_ram_lle[ m_v60_copro_ram_adr & ( DS - 1 ) ];
	else
	{
		r = copro_ram_lle[ m_v60_copro_ram_adr & ( DS - 1 ) ] >> 16;
		if( m_v60_copro_ram_adr & 0x8000 )
			m_v60_copro_ram_adr++;
	}
	return r;
}

WRITE16_HANDLER( model1_tgp_copro_ram_w )
{
	COMBINE_DATA( m_v60_copro_ram_latch + offset );
	if( offset )
	{
		UINT32 v = m_v60_copro_ram_latch[ 0 ] | ( m_v60_copro_ram_latch[ 1 ] << 16 );
		copro_ram_lle[ m_v60_copro_ram_adr & ( DS - 1 ) ] = v;
		copro_ram_hle[ m_v60_copro_ram_adr & ( DS - 1 ) ] = v;
		if( m_v60_copro_ram_adr & 0x8000 )
			m_v60_copro_ram_adr++;
	}
}

READ16_HANDLER( model1_tgp_copro_r )
{
	if( !offset )
	{
		int ok;
		struct m1_fifo *f = ( model1_tgp_mode == MODEL1_TGP_HLE ) ? &hle_out : &fifo_out;
		m_v60_copro_fifo_r = fifo_pop( f, &ok );
		if( !ok )
		{
			if( f == &hle_out )
				LOG(( "TGP: V60 read with no HLE result (%x)\n", activecpu_get_pc() ));
			else
				v60_stall();
		}
		return m_v60_copro_fifo_r;
	}
	return m_v60_copro_fifo_r >> 16;
}

WRITE16_HANDLER( model1_tgp_copro_w )
{
	if( offset )
	{
		m_v60_copro_fifo_w = ( m_v60_copro_fifo_w & 0x0000ffff ) | ( data << 16 );
		m_pushpc = activecpu_get_pc();
		if( model1_tgp_mode != MODEL1_TGP_HLE )
			fifo_push( &fifo_in, m_v60_copro_fifo_w );
		if( model1_tgp_mode != MODEL1_TGP_LLE )
		{
			oracle_in_count++;
			fifo_push( &hle_in, m_v60_copro_fifo_w );
			copro_hle_run();
		}
	}
	else
		m_v60_copro_fifo_w = ( m_v60_copro_fifo_w & 0xffff0000 ) | ( data & 0xffff );
}

READ16_HANDLER( model1_fifoin_status_r )
{
	return 0xffff;
}


/*************************************
 *  Real TGP side (MB86233, as current MAME's copro maps)
 *************************************/


static UINT32 tgp_prog_r( UINT32 offset )
{
	return copro_program[ offset & 0x7ff ];
}

static UINT32 tgp_data_r( UINT32 offset )
{
	offset &= 0xffff;
	if( offset == 0x100 )
	{
		int ok;
		UINT32 v = fifo_pop( &fifo_in, &ok );
		if( !ok )
			mb86233_stall();
		else if( model1_tgp_mode == MODEL1_TGP_ORACLE )
			oracle_dsp_pop();
		return v;
	}
	if( offset < 0x400 )
		return tgp_data_ram[ offset ];
	return 0;
}

static void tgp_data_w( UINT32 offset, UINT32 data )
{
	offset &= 0xffff;
	if( offset == 0x400 )
	{
		fifo_push( &fifo_out, data );
		if( model1_tgp_mode == MODEL1_TGP_ORACLE )
			oracle_dsp_push( data );
		return;
	}
	if( offset < 0x400 && offset != 0x100 )
		tgp_data_ram[ offset ] = data;
}

/* the table ports of the real TGP: the same functions as the HLE's */
static UINT32 copro_sincos_r( UINT32 offset )
{
	return table_sincos( m_copro_sincos_base + offset * 0x4000 );
}

static UINT32 copro_inv_r( UINT32 offset )
{
	return tgp_inv_r( m_copro_inv_base, offset );
}

static UINT32 copro_isqrt_r( UINT32 offset )
{
	return tgp_isqrt_r( m_copro_isqrt_base, offset );
}

static UINT32 copro_atan_r( void )
{
	return tgp_atan_r( m_copro_atan_base[ 0 ], m_copro_atan_base[ 1 ], m_copro_atan_base[ 2 ], m_copro_atan_base[ 3 ] );
}

static UINT32 tgp_io_r( UINT32 offset )
{
	offset &= 0xffff;
	if( offset >= 0x8000 )
		return copro_data_u32( ( m_copro_data_base & ~0x7fff ) | ( offset & 0x7fff ) );
	if( ( offset & ~0x18 ) == 0 )
		return m_copro_ram_adr[ ( offset >> 3 ) & 3 ];
	if( ( offset & ~0x18 ) == 1 )
	{
		int i = ( offset >> 3 ) & 3;
		UINT32 v = copro_ram_lle[ m_copro_ram_adr[ i ] & ( DS - 1 ) ];
		m_copro_ram_adr[ i ] = ram_adr_next( m_copro_ram_adr[ i ] );
		return v;
	}
	if( offset >= 0x20 && offset <= 0x23 )
		return copro_sincos_r( offset - 0x20 );
	if( offset >= 0x24 && offset <= 0x27 )
		return copro_atan_r();
	if( offset >= 0x28 && offset <= 0x29 )
		return copro_inv_r( offset - 0x28 );
	if( offset >= 0x2a && offset <= 0x2b )
		return copro_isqrt_r( offset - 0x2a );
	return 0;
}

static void tgp_io_w( UINT32 offset, UINT32 data )
{
	offset &= 0xffff;
	if( ( offset & ~0x18 ) == 0 )
		m_copro_ram_adr[ ( offset >> 3 ) & 3 ] = data;
	else if( ( offset & ~0x18 ) == 1 )
	{
		int i = ( offset >> 3 ) & 3;
		copro_ram_lle[ m_copro_ram_adr[ i ] & ( DS - 1 ) ] = data;
		m_copro_ram_adr[ i ] = ram_adr_next( m_copro_ram_adr[ i ] );
	}
	else if( offset >= 0x20 && offset <= 0x23 )
		m_copro_sincos_base = data;
	else if( offset >= 0x24 && offset <= 0x27 )
		m_copro_atan_base[ offset - 0x24 ] = data;
	else if( offset >= 0x28 && offset <= 0x29 )
		m_copro_inv_base = data;
	else if( offset >= 0x2a && offset <= 0x2b )
		m_copro_isqrt_base = data;
	else if( offset == 0x2e )
		m_copro_data_base = data;
}

static UINT32 tgp_rf_r( UINT32 offset )
{
	return 0;
}

static void tgp_rf_w( UINT32 offset, UINT32 data )
{
	/* leds? busy flag? */
}

struct mb86233_config model1_tgp_config =
{
	tgp_prog_r, tgp_data_r, tgp_data_w, tgp_io_r, tgp_io_w, tgp_rf_r, tgp_rf_w
};


/*************************************
 *  Init / reset
 *************************************/

static UINT32 *ram_buf_a, *ram_buf_b;

/* a save state can fall between the words of a command or before the V60
   read the results: the FIFOs are part of the state */
static void fifo_register( struct m1_fifo *f, const char *name )
{
	state_save_register_UINT32( "model1tgp_fifo", 0, name, f->data, FIFO_ALLOC );
	state_save_register_int( "model1tgp_fifo_head", 0, name, &f->head );
	state_save_register_int( "model1tgp_fifo_count", 0, name, &f->count );
	state_save_register_int( "model1tgp_fifo_rw", 0, name, &f->reader_waits );
	state_save_register_int( "model1tgp_fifo_ww", 0, name, &f->writer_waits );
}

static void mat_postload( void )
{
	if( m_mat_stack_pos < 0 || m_mat_stack_pos >= MAT_SLOTS )
		m_mat_stack_pos = 0;
	m_cmat = MAT_SLOT( m_mat_stack_pos );
}

void model1_tgp_init( void )
{
	if( !ram_buf_a )
	{
		ram_buf_a = auto_malloc( ( DS + 16 ) * sizeof( UINT32 ) );
		ram_buf_b = auto_malloc( ( DS + 16 ) * sizeof( UINT32 ) );
	}
	memset( ram_buf_a, 0, ( DS + 16 ) * sizeof( UINT32 ) );
	memset( ram_buf_b, 0, ( DS + 16 ) * sizeof( UINT32 ) );
	copro_ram_lle = ram_buf_a;
	copro_ram_hle = ( model1_tgp_mode == MODEL1_TGP_ORACLE ) ? ram_buf_b : ram_buf_a;

	copro_tables = (UINT32 *)memory_region( REGION_USER2 );
	copro_data = (UINT32 *)memory_region( REGION_USER3 );
	copro_data_mask = copro_data ? ( memory_region_length( REGION_USER3 ) >> 2 ) - 1 : 0;
	copro_program = (UINT32 *)memory_region( REGION_USER4 );

	state_save_register_UINT32( "model1tgp", 0, "ram", copro_ram_lle, DS );
	if( copro_ram_hle != copro_ram_lle )
		state_save_register_UINT32( "model1tgp", 0, "ram_hle", copro_ram_hle, DS );
	state_save_register_UINT32( "model1tgp", 0, "data_ram", tgp_data_ram, 0x400 );
	state_save_register_UINT16( "model1tgp", 0, "v60_ram_adr", &m_v60_copro_ram_adr, 1 );
	state_save_register_UINT16( "model1tgp", 0, "v60_ram_latch", m_v60_copro_ram_latch, 2 );
	state_save_register_UINT32( "model1tgp", 0, "v60_fifo_r", &m_v60_copro_fifo_r, 1 );
	state_save_register_UINT32( "model1tgp", 0, "v60_fifo_w", &m_v60_copro_fifo_w, 1 );
	state_save_register_UINT32( "model1tgp", 0, "ram_adr", m_copro_ram_adr, 4 );
	state_save_register_UINT32( "model1tgp", 0, "sincos", &m_copro_sincos_base, 1 );
	state_save_register_UINT32( "model1tgp", 0, "inv", &m_copro_inv_base, 1 );
	state_save_register_UINT32( "model1tgp", 0, "isqrt", &m_copro_isqrt_base, 1 );
	state_save_register_UINT32( "model1tgp", 0, "atan", m_copro_atan_base, 4 );
	state_save_register_UINT32( "model1tgp", 0, "data_base", &m_copro_data_base, 1 );
	state_save_register_UINT32( "model1tgp", 0, "scan_adr", &m_copro_hle_ram_scan_adr, 1 );
	state_save_register_float( "model1tgp", 0, "dram", m_dram, 0x200 );
	state_save_register_UINT32( "model1tgp", 0, "vf_r0", m_vf_r0, 0x100 );
	state_save_register_UINT32( "model1tgp", 0, "vf_list_adr", m_vf_list_adr, 2 );
	state_save_register_int( "model1tgp", 0, "list_sink", &m_list_sink );
	state_save_register_INT32( "model1tgp", 0, "mat_depth", &m_mat_depth, 1 );
	state_save_register_func_postload( mat_postload );
	state_save_register_float( "model1tgp", 0, "mat_vector", &m_mat_vector[ 0 ][ 0 ], 21 * 12 );
	state_save_register_INT32( "model1tgp", 0, "mat_stack_pos", &m_mat_stack_pos, 1 );
	state_save_register_float( "model1tgp", 0, "acc", &m_acc, 1 );
	state_save_register_UINT32( "model1tgp", 0, "acc_sp", &m_acc_sp, 1 );
	state_save_register_float( "model1tgp", 0, "swa_view", m_swa_view, 6 );
	state_save_register_UINT32( "model1tgp", 0, "list_length", &m_copro_hle_list_length, 1 );
	state_save_register_UINT32( "model1tgp", 0, "active_list_length", &m_copro_hle_active_list_length, 1 );
	state_save_register_UINT32( "model1tgp", 0, "active_list_pos", &m_copro_hle_active_list_pos, 1 );
	state_save_register_float( "model1tgp", 0, "vf_bounds1", &m_tgp_vf_xmin, 1 );
	state_save_register_float( "model1tgp", 0, "vf_bounds2", &m_tgp_vf_xmax, 1 );
	state_save_register_float( "model1tgp", 0, "vf_bounds3", &m_tgp_vf_zmin, 1 );
	state_save_register_float( "model1tgp", 0, "vf_bounds4", &m_tgp_vf_zmax, 1 );
	state_save_register_float( "model1tgp", 0, "vf_ygnd", &m_tgp_vf_ygnd, 1 );
	state_save_register_float( "model1tgp", 0, "vf_yflr", &m_tgp_vf_yflr, 1 );
	state_save_register_float( "model1tgp", 0, "vf_yjmp", &m_tgp_vf_yjmp, 1 );
	state_save_register_float( "model1tgp", 0, "vr_circ", &m_tgp_vr_circx, 1 );
	state_save_register_float( "model1tgp", 0, "vr_circy", &m_tgp_vr_circy, 1 );
	state_save_register_float( "model1tgp", 0, "vr_circrad", &m_tgp_vr_circrad, 1 );
	state_save_register_float( "model1tgp", 0, "vr_cbox", m_tgp_vr_cbox, 12 );
	state_save_register_int( "model1tgp", 0, "vr_select", &m_tgp_vr_select );
	state_save_register_float( "model1tgp", 0, "int_px", &m_tgp_int_px, 1 );
	state_save_register_float( "model1tgp", 0, "int_py", &m_tgp_int_py, 1 );
	state_save_register_float( "model1tgp", 0, "int_pz", &m_tgp_int_pz, 1 );
	state_save_register_UINT32( "model1tgp", 0, "int_adr", &m_tgp_int_adr, 1 );
	state_save_register_float( "model1tgp", 0, "int_n", m_int_n, 4 );
	state_save_register_UINT32( "model1tgp", 0, "t71", &m_t71, 1 );
	state_save_register_UINT32( "model1tgp", 0, "t11", &m_t11, 1 );
	state_save_register_float( "model1tgp", 0, "int_p", m_int_p, 3 );
	state_save_register_float( "model1tgp", 0, "int_sv", m_int_sv, 6 );
	state_save_register_float( "model1tgp", 0, "vr_base", m_tgp_vr_base, 4 );
	state_save_register_int( "model1tgp", 0, "ccount", &m_ccount );
	fifo_register( &fifo_in, "in" );
	fifo_register( &fifo_out, "out" );
	fifo_register( &hle_in, "hle_in" );
	fifo_register( &hle_out, "hle_out" );
}

void model1_tgp_reset( void )
{
	m_v60_copro_ram_adr = 0;
	m_v60_copro_ram_latch[ 0 ] = m_v60_copro_ram_latch[ 1 ] = 0;
	memset( m_copro_ram_adr, 0, sizeof( m_copro_ram_adr ) );
	m_copro_sincos_base = m_copro_inv_base = m_copro_isqrt_base = 0;
	memset( m_copro_atan_base, 0, sizeof( m_copro_atan_base ) );
	m_copro_data_base = 0;
	memset( copro_ram_lle, 0, DS * sizeof( UINT32 ) );
	if( copro_ram_hle != copro_ram_lle )
		memset( copro_ram_hle, 0, DS * sizeof( UINT32 ) );
	memset( tgp_data_ram, 0, sizeof( tgp_data_ram ) );

	m_acc = 0;
	memset( m_dram, 0, sizeof( m_dram ) );
	m_acc_sp = 0;
	m_t71 = 0;
	m_t11 = 0;
	memset( m_int_n, 0, sizeof( m_int_n ) );
	memset( m_int_p, 0, sizeof( m_int_p ) );
	memset( m_int_sv, 0, sizeof( m_int_sv ) );
	memset( m_swa_view, 0, sizeof( m_swa_view ) );
	memset( m_vf_r0, 0, sizeof( m_vf_r0 ) );
	m_vf_list_adr[ 0 ] = m_vf_list_adr[ 1 ] = 0;
	m_list_sink = 0;
	m_mat_base = ( model1_tgp_table == MODEL1_TGP_TABLE_VF ) ? DRAM_MAT_VF : DRAM_MAT;
	m_mat_stride = ( model1_tgp_table == MODEL1_TGP_TABLE_VF ) ? 16 : 12;
	m_mat_stack_pos = 0;
	m_mat_depth = 0;
	m_cmat = MAT_SLOT( 0 );
	m_copro_hle_active_list_length = 0;
	m_copro_hle_active_list_pos = 0;
	m_copro_hle_list_length = 0;
	/* the 315-5711 and 315-5724 games use the matrix the TGP's RAM powers
	   up with (zeros here, as the real TGP emulated) before setting it;
	   MAME's HLE starts with an identity, kept for Virtua Racing */
	if( model1_tgp_table == MODEL1_TGP_TABLE_VR )
	{
		m_cmat[ 0 ] = 1.0;
		m_cmat[ 4 ] = 1.0;
		m_cmat[ 8 ] = 1.0;
	}

	fifo_in.reader = model1_tgp_cpu;	fifo_in.writer = CPU_MAIN;
	fifo_out.reader = CPU_MAIN;			fifo_out.writer = model1_tgp_cpu;
	hle_in.reader = -1;					hle_in.writer = -1;
	/* an HLE function that pushed too few results must not hang the V60:
	   it reads 0 instead (logged) */
	hle_out.reader = -1;
	hle_out.writer = -1;
	fifo_clear( &fifo_in );
	fifo_clear( &fifo_out );
	fifo_clear( &hle_in );
	fifo_clear( &hle_out );

	oracle_head = oracle_tail = 0;
	oracle_cur = -1;
	oracle_cur_pending = oracle_pending_valid = 0;
	oracle_in_count = oracle_dsp_pops = 0;
	oracle_dsp_nout = 0;
}
