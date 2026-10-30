/***************************************************************************

    Sega Model 2: TGP coprocessor link and sound board link.

    From current MAME (src/mame/sega/model2.cpp; copyright R. Belmont,
    Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels,
    BSD-3-Clause), in C for MAME 2003.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "cpu/i960/i960.h"
#include "cpu/mb86233/mb86233.h"
#include "cpu/sharc/sharc.h"
#include "cpu/mb86235/mb86235.h"
#include "sound/scsp.h"
#include "includes/model2.h"
#include "includes/segadsb.h"

#define CPU_MAIN	0
#define CPU_TGP		1


/*************************************
 *  FIFOs between the i960 and the TGP

    As MAME's generic_fifo: a push is never lost, it may go past the
    size and then the writer waits; a pop from an empty FIFO makes the
    reader repeat the access and wait for a push.
 *************************************/

#define FIFO_SIZE	8
#define FIFO_ALLOC	256

struct m2_fifo
{
	UINT32 data[ FIFO_ALLOC ];
	int head, count;
	int reader, writer;			/* CPU numbers */
	int reader_waits, writer_waits;
	int size;
};

int model2_copro_type;	/* MODEL2_COPRO_TGP or MODEL2_COPRO_SHARC */

static struct m2_fifo fifo_in;
static void sharc_flags_update( void );

static struct m2_fifo fifo_out;	/* TGP -> i960 */

static void sharc_flags_update( void )
{
	if( model2_copro_type != MODEL2_COPRO_SHARC )
		return;
	/* FLAG0: FIFOIN empty, FLAG1: FIFOIN full (as current MAME) */
	sharc_set_flag_input( 0, fifo_in.count == 0 );
	sharc_set_flag_input( 1, fifo_in.count >= fifo_in.size );
}

static void fifo_clear( struct m2_fifo *f )
{
	f->head = f->count = 0;
	if( f == &fifo_in ) sharc_flags_update();
	if( f->reader_waits )
		cpunum_resume( f->reader, SUSPEND_REASON_SPIN );
	if( f->writer_waits )
		cpunum_resume( f->writer, SUSPEND_REASON_SPIN );
	f->reader_waits = f->writer_waits = 0;
}

static void fifo_push( struct m2_fifo *f, UINT32 v )
{
	if( f->count >= FIFO_ALLOC )
		return;	/* cannot happen: the writer waits first */
	f->data[ ( f->head + f->count ) % FIFO_ALLOC ] = v;
	f->count++;
	if( f == &fifo_in ) sharc_flags_update();
	if( f->reader_waits )
	{
		f->reader_waits = 0;
		cpunum_resume( f->reader, SUSPEND_REASON_SPIN );
	}
	if( f->count > f->size && !f->writer_waits )
	{
		/* the writer waits for the reader to make room */
		f->writer_waits = 1;
		cpunum_suspend( f->writer, SUSPEND_REASON_SPIN, 1 );
		if( cpu_getactivecpu() == f->writer )
			activecpu_adjust_icount( -activecpu_get_icount() - 1 );
	}
}

/* returns 0 and sets *ok = 0 when empty: the reader must repeat */
static UINT32 fifo_pop( struct m2_fifo *f, int *ok )
{
	UINT32 v;
	if( f->count == 0 )
	{
		*ok = 0;
		if( !f->reader_waits )
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
	if( f == &fifo_in ) sharc_flags_update();
	if( f->writer_waits && f->count <= f->size )
	{
		f->writer_waits = 0;
		cpunum_resume( f->writer, SUSPEND_REASON_SPIN );
	}
	return v;
}


/*************************************
 *  i960 side
 *************************************/

static UINT32 m_coproctl, m_coprocnt;
static UINT32 *copro_tgp_program;
static UINT32 *copro_tgpx4_program;	/* 64-bit words as low, high dword pairs */
static int tgp_booted;

READ32_HANDLER( model2_copro_ctl1_r )
{
	return m_coproctl;
}

static void copro_halt( void )
{
	if( model2_copro_type != MODEL2_COPRO_TGP )
		return;	/* the SHARC and the TGPx4 keep running while the host sends a new program, as MAME */
	tgp_booted = 0;
	cpunum_suspend( CPU_TGP, SUSPEND_REASON_HALT, 1 );
}

static void copro_boot( void )
{
	if( model2_copro_type == MODEL2_COPRO_SHARC )
	{
		tgp_booted = 1;
		cpunum_resume( CPU_TGP, SUSPEND_REASON_HALT );
		return;
	}
	if( model2_copro_type == MODEL2_COPRO_TGPX4 )
	{
		/* out of halt, no reset (as MAME) */
		tgp_booted = 1;
		cpunum_resume( CPU_TGP, SUSPEND_REASON_HALT );
		return;
	}
	tgp_booted = 1;
	cpunum_resume( CPU_TGP, SUSPEND_REASON_HALT );
	cpunum_reset( CPU_TGP, NULL, NULL );
}

WRITE32_HANDLER( model2_copro_ctl1_w )
{
	/* did hi bit change? */
	if( ( data ^ m_coproctl ) == 0x80000000 )
	{
		if( data & 0x80000000 )
		{
			m_coprocnt = 0;
			copro_halt();
		}
		else
			copro_boot();
	}
	COMBINE_DATA( &m_coproctl );
}

READ32_HANDLER( model2_copro_fifo_r )
{
	int ok;
	UINT32 v = fifo_pop( &fifo_out, &ok );
	if( !ok )
		i960_stall();
	return v;
}

WRITE32_HANDLER( model2_copro_fifo_w )
{
	if( m_coproctl & 0x80000000 )
	{
		if( model2_copro_type == MODEL2_COPRO_SHARC )
			sharc_external_dma_write( m_coprocnt, data & 0xffff );
		else if( model2_copro_type == MODEL2_COPRO_TGPX4 )
			copro_tgpx4_program[ m_coprocnt & 0x1fff ] = data;
		else
			copro_tgp_program[ m_coprocnt & 0xfff ] = data;
		m_coprocnt++;
	}
	else
		fifo_push( &fifo_in, data );
}

WRITE32_HANDLER( model2_copro_function_port_w )
{
	UINT32 d = data & 0x800fffff;
	UINT32 a = ( offset >> 2 ) & 0xff;
	d |= a << 23;
	fifo_push( &fifo_in, d );
}

READ32_HANDLER( model2_fifo_control_r )
{
	return fifo_out.count == 0 ? 1 : 0;
}


/*************************************
 *  TGP side (MB86234)
 *************************************/

static UINT32 *copro_tgp_data;		/* 0x000-0x0ff and 0x200-0x3ff */
static UINT32 *copro_tgp_tables;
static UINT32 copro_tgp_bank_reg;
static UINT32 copro_sincos_base, copro_inv_base, copro_isqrt_base, copro_atan_base[ 4 ];

static UINT32 tgp_prog_r( UINT32 offset )
{
	return copro_tgp_program[ offset & 0xfff ];
}

static UINT32 tgp_data_r( UINT32 offset )
{
	offset &= 0xffff;
	if( offset < 0x100 ) return copro_tgp_data[ offset ];
	if( offset >= 0x200 && offset < 0x400 ) return copro_tgp_data[ offset - 0x100 ];
	return 0;
}

static void tgp_data_w( UINT32 offset, UINT32 data )
{
	offset &= 0xffff;
	if( offset < 0x100 ) copro_tgp_data[ offset ] = data;
	else if( offset >= 0x200 && offset < 0x400 ) copro_tgp_data[ offset - 0x100 ] = data;
}

static UINT32 copro_sincos_r( int offset )
{
	UINT32 ang = copro_sincos_base + offset * 0x4000;
	UINT32 index = ang & 0x3fff;
	UINT32 result;
	if( ang & 0x4000 )
	{
		int i = 0x4000 - (int)index;
		index = i < 0x3fff ? i : 0x3fff;
	}
	result = copro_tgp_tables[ index ];
	if( ang & 0x8000 )
		result ^= 0x80000000;
	return result;
}

static UINT32 copro_inv_r( int offset )
{
	UINT32 index = ( ( copro_inv_base >> 9 ) & 0x3ffe ) | ( offset & 1 );
	UINT32 result = copro_tgp_tables[ index | 0x8000 ];
	UINT8 bexp = ( copro_inv_base >> 23 ) & 0xff;
	UINT8 exp = ( result >> 23 ) + ( 0x7f - bexp );
	result = ( result & 0x007fffff ) | ( exp << 23 );
	if( ( copro_inv_base & 0x80000000 ) && offset )
		result |= 0x80000000;
	return result;
}

static UINT32 copro_isqrt_r( int offset )
{
	UINT32 index = 0x2000 ^ ( ( ( copro_isqrt_base >> 10 ) & 0x3ffe ) | ( offset & 1 ) );
	UINT32 result = copro_tgp_tables[ index | 0xc000 ];
	UINT8 bexp = ( copro_isqrt_base >> 24 ) & 0x7f;
	UINT8 exp = ( result >> 23 ) + ( 0x3f - bexp );
	result = ( result & 0x807fffff ) | ( exp << 23 );
	if( !( offset & 1 ) )
		result &= 0x7fffffff;
	return result;
}

static UINT32 copro_atan_r( void )
{
	UINT8 ie = 0x88 - ( copro_atan_base[ 3 ] >> 23 );
	int s0 = ( copro_atan_base[ 0 ] & 0x80000000 ) != 0;
	int s1 = ( copro_atan_base[ 1 ] & 0x80000000 ) != 0;
	int s2 = ( copro_atan_base[ 0 ] & 0x7fffffff ) <= ( copro_atan_base[ 1 ] & 0x7fffffff );
	UINT32 im = copro_atan_base[ 3 ] & 0x7fffff;
	UINT32 index = ie <= 0x17 ? ( im | 0x800000 ) >> ie : 0;
	UINT32 result;

	if( index == 0x4000 )
		index = 0x3fff;
	result = copro_tgp_tables[ index | 0x4000 ];
	if( s0 ^ s1 ^ s2 )
		result >>= 16;
	if( s2 )
		result += 0x4000;
	if( ( s0 && !s2 ) || ( s1 && s2 ) )
		result += 0x8000;
	return result & 0xffff;
}

/* as MAME's map: with the bank enabled its view covers the whole space,
   table registers included */
static UINT32 tgp_io_r( UINT32 offset )
{
	offset &= 0xffff;
	if( !( copro_tgp_bank_reg & 0xc00000 ) )
	switch( offset )
	{
	case 0x20: case 0x21: case 0x22: case 0x23:	return copro_sincos_r( offset - 0x20 );
	case 0x24: case 0x25: case 0x26: case 0x27:	return copro_atan_r();
	case 0x28: case 0x29:						return copro_inv_r( offset - 0x28 );
	case 0x2a: case 0x2b:						return copro_isqrt_r( offset - 0x2a );
	}
	if( copro_tgp_bank_reg & 0xc00000 )
	{
		UINT32 adr = ( copro_tgp_bank_reg & 0xff0000 ) | offset;
		if( adr & 0x800000 )
		{
			adr &= ( memory_region_length( REGION_USER2 ) >> 2 ) - 1;
			return ( (UINT32 *)memory_region( REGION_USER2 ) )[ adr ];
		}
		if( adr & 0x400000 )
			return model2_bufferram[ adr & 0x7fff ];
	}
	return 0;
}

static void tgp_io_w( UINT32 offset, UINT32 data )
{
	offset &= 0xffff;
	if( !( copro_tgp_bank_reg & 0xc00000 ) )
	switch( offset )
	{
	case 0x20: case 0x21: case 0x22: case 0x23:
		copro_sincos_base = data;
		return;
	case 0x24: case 0x25: case 0x26: case 0x27:
		copro_atan_base[ offset - 0x24 ] = data;
		mb86233_gpio_w( 0, ( copro_atan_base[ 0 ] & 0x7fffffff ) <= ( copro_atan_base[ 1 ] & 0x7fffffff ) );
		return;
	case 0x28: case 0x29:
		copro_inv_base = data;
		return;
	case 0x2a: case 0x2b:
		copro_isqrt_base = data;
		return;
	}
	if( copro_tgp_bank_reg & 0xc00000 )
	{
		UINT32 adr = ( copro_tgp_bank_reg & 0xff0000 ) | offset;
		if( adr & 0x400000 )
			model2_bufferram[ adr & 0x7fff ] = data;
	}
}

static UINT32 tgp_rf_r( UINT32 offset )
{
	if( ( offset & 0xf ) == 1 )
	{
		int ok;
		UINT32 v = fifo_pop( &fifo_in, &ok );
		if( !ok )
			mb86233_stall();
		return v;
	}
	return 0;
}

static void tgp_rf_w( UINT32 offset, UINT32 data )
{
	switch( offset & 0xf )
	{
	case 2:
		fifo_push( &fifo_out, data );
		break;
	case 3:
		copro_tgp_bank_reg = data;
		break;
	}
}

struct mb86233_config model2_tgp_config =
{
	tgp_prog_r, tgp_data_r, tgp_data_w, tgp_io_r, tgp_io_w, tgp_rf_r, tgp_rf_w
};



/*************************************
 *  SHARC side (ADSP-21062, Model 2B)
 *************************************/

static UINT32 sharc_data_r( UINT32 address )
{
	if( address >= 0x0400000 && address < 0x0c00000 )
	{
		int ok;
		UINT32 v = fifo_pop( &fifo_in, &ok );
		if( !ok )
			sharc_stall();
		return v;
	}
	if( address >= 0x1400000 && address < 0x1c00000 )
		return model2_bufferram[ address & 0x7fff ];
	if( address >= 0x1c00000 && address < 0x1e00000 )
	{
		UINT32 a = ( address - 0x1c00000 ) & ( ( memory_region_length( REGION_USER2 ) >> 2 ) - 1 );
		return ( (UINT32 *)memory_region( REGION_USER2 ) )[ a ];
	}
	return 0;
}

static void sharc_data_w( UINT32 address, UINT32 data )
{
	if( address >= 0x0c00000 && address < 0x1400000 )
		fifo_push( &fifo_out, data );
	else if( address >= 0x1400000 && address < 0x1c00000 )
		model2_bufferram[ address & 0x7fff ] = data;
}

struct sharc_config model2_sharc_config =
{
	SHARC_BOOT_MODE_HOST,
	sharc_data_r,
	sharc_data_w,
	NULL, NULL, NULL
};

/*************************************
 *  TGPx4 side (MB86235, Model 2C)
 *************************************/

static UINT64 tgpx4_prog_r( UINT32 offset )
{
	offset &= 0xfff;
	return ( (UINT64)copro_tgpx4_program[ offset * 2 + 1 ] << 32 ) | copro_tgpx4_program[ offset * 2 ];
}

/* external bus: buffer RAM at 0x400000 (mirrored up to 0x7fffff), the
   copro data ROM at 0x800000 */
static UINT32 tgpx4_ext_r( UINT32 offset )
{
	if( offset >= 0x400000 && offset < 0x800000 )
		return model2_bufferram[ offset & 0x7fff ];
	if( offset >= 0x800000 && offset < 0xa00000 )
	{
		UINT32 a = ( offset - 0x800000 ) & ( ( memory_region_length( REGION_USER2 ) >> 2 ) - 1 );
		return ( (UINT32 *)memory_region( REGION_USER2 ) )[ a ];
	}
	return 0;
}

static void tgpx4_ext_w( UINT32 offset, UINT32 data )
{
	if( offset >= 0x400000 && offset < 0x800000 )
		model2_bufferram[ offset & 0x7fff ] = data;
}

static UINT32 tgpx4_fifoin_r( void )
{
	int ok;
	UINT32 v = fifo_pop( &fifo_in, &ok );
	if( !ok )
		mb86235_stall();
	return v;
}

static void tgpx4_fifoout_w( UINT32 data )
{
	fifo_push( &fifo_out, data );
}

static int tgpx4_fifo_flag( int which )
{
	switch( which )
	{
	case MB86235_FIFO_IFF: return fifo_in.count >= fifo_in.size;
	case MB86235_FIFO_IFE: return fifo_in.count == 0;
	case MB86235_FIFO_OFF: return fifo_out.count >= fifo_out.size;
	case MB86235_FIFO_OFE: return fifo_out.count == 0;
	}
	return 0;
}

static void tgpx4_fifo_clear( int which )
{
	fifo_clear( which ? &fifo_out : &fifo_in );
}

struct mb86235_config model2_tgpx4_config =
{
	tgpx4_prog_r,
	tgpx4_ext_r,
	tgpx4_ext_w,
	tgpx4_fifoin_r,
	tgpx4_fifoout_w,
	tgpx4_fifo_flag,
	tgpx4_fifo_clear
};

WRITE32_HANDLER( model2_copro_iop_w )
{
	if( model2_copro_type == MODEL2_COPRO_SHARC )
		sharc_external_iop_write( offset, data );
}

READ32_HANDLER( model2_copro_status_r )
{
	return m_coprocnt == 0 ? 0xffffffff : 0;
}


/*************************************
 *  Init / reset / save
 *************************************/

static UINT32 fifo_in_save[ FIFO_ALLOC ], fifo_out_save[ FIFO_ALLOC ];
static int fifo_in_count, fifo_out_count;

static INT32 fifo_waits_save[ 4 ];

static void fifo_wait_load( struct m2_fifo *f, int reader_waits, int writer_waits )
{
	f->reader_waits = reader_waits ? 1 : 0;
	f->writer_waits = writer_waits ? 1 : 0;
	if( f->reader_waits )
		cpunum_suspend( f->reader, SUSPEND_REASON_SPIN, 1 );
	if( f->writer_waits )
		cpunum_suspend( f->writer, SUSPEND_REASON_SPIN, 1 );
}

static void copro_presave( void )
{
	int i;
	fifo_in_count = fifo_in.count;
	fifo_out_count = fifo_out.count;
	fifo_waits_save[ 0 ] = fifo_in.reader_waits;
	fifo_waits_save[ 1 ] = fifo_in.writer_waits;
	fifo_waits_save[ 2 ] = fifo_out.reader_waits;
	fifo_waits_save[ 3 ] = fifo_out.writer_waits;
	for( i = 0; i < fifo_in.count; i++ )
		fifo_in_save[ i ] = fifo_in.data[ ( fifo_in.head + i ) % FIFO_ALLOC ];
	for( i = 0; i < fifo_out.count; i++ )
		fifo_out_save[ i ] = fifo_out.data[ ( fifo_out.head + i ) % FIFO_ALLOC ];
}

static void copro_postload( void )
{
	int i;
	fifo_clear( &fifo_in );
	fifo_clear( &fifo_out );
	for( i = 0; i < fifo_in_count; i++ )
		fifo_in.data[ i ] = fifo_in_save[ i ];
	for( i = 0; i < fifo_out_count; i++ )
		fifo_out.data[ i ] = fifo_out_save[ i ];
	fifo_in.count = fifo_in_count;
	fifo_out.count = fifo_out_count;
	if( model2_copro_type == MODEL2_COPRO_SHARC )
		sharc_flags_update();
	/* the CPUs waiting on a FIFO at the save wait again, so that the
	   access that ends the wait resumes them */
	fifo_wait_load( &fifo_in, fifo_waits_save[ 0 ], fifo_waits_save[ 1 ] );
	fifo_wait_load( &fifo_out, fifo_waits_save[ 2 ], fifo_waits_save[ 3 ] );
	if( tgp_booted )
		cpunum_resume( CPU_TGP, SUSPEND_REASON_HALT );
	else
		cpunum_suspend( CPU_TGP, SUSPEND_REASON_HALT, 1 );
}

void model2_copro_init( void )
{
	copro_tgp_program = auto_malloc( 0x1000 * 4 );
	copro_tgp_data = auto_malloc( 0x300 * 4 );
	copro_tgpx4_program = auto_malloc( 0x2000 * 4 );
	memset( copro_tgpx4_program, 0, 0x2000 * 4 );
	memset( copro_tgp_program, 0, 0x1000 * 4 );
	memset( copro_tgp_data, 0, 0x300 * 4 );
	copro_tgp_tables = (UINT32 *)memory_region( REGION_USER5 );
	fifo_in.size = fifo_out.size = ( model2_copro_type == MODEL2_COPRO_SHARC ) ? 16 : FIFO_SIZE;

	fifo_in.reader = CPU_TGP;
	fifo_in.writer = CPU_MAIN;
	fifo_out.reader = CPU_MAIN;
	fifo_out.writer = CPU_TGP;

	state_save_register_UINT32( "model2tgp", 0, "program", copro_tgp_program, 0x1000 );
	state_save_register_UINT32( "model2tgp", 0, "data", copro_tgp_data, 0x300 );
	state_save_register_UINT32( "model2tgp", 0, "tgpx4_program", copro_tgpx4_program, 0x2000 );
	state_save_register_UINT32( "model2tgp", 0, "coproctl", &m_coproctl, 1 );
	state_save_register_UINT32( "model2tgp", 0, "coprocnt", &m_coprocnt, 1 );
	state_save_register_UINT32( "model2tgp", 0, "bank", &copro_tgp_bank_reg, 1 );
	state_save_register_UINT32( "model2tgp", 0, "sincos", &copro_sincos_base, 1 );
	state_save_register_UINT32( "model2tgp", 0, "inv", &copro_inv_base, 1 );
	state_save_register_UINT32( "model2tgp", 0, "isqrt", &copro_isqrt_base, 1 );
	state_save_register_UINT32( "model2tgp", 0, "atan", copro_atan_base, 4 );
	state_save_register_int( "model2tgp", 0, "booted", &tgp_booted );
	state_save_register_UINT32( "model2tgp", 0, "fifo_in", fifo_in_save, FIFO_ALLOC );
	state_save_register_UINT32( "model2tgp", 0, "fifo_out", fifo_out_save, FIFO_ALLOC );
	state_save_register_int( "model2tgp", 0, "fifo_in_count", &fifo_in_count );
	state_save_register_int( "model2tgp", 0, "fifo_out_count", &fifo_out_count );
	state_save_register_INT32( "model2tgp", 0, "fifo_waits", fifo_waits_save, 4 );
	state_save_register_func_presave( copro_presave );
	state_save_register_func_postload( copro_postload );
}

void model2_copro_reset( void )
{
	m_coproctl = 0;
	m_coprocnt = 0;
	copro_tgp_bank_reg = 0;
	fifo_clear( &fifo_in );
	fifo_clear( &fifo_out );
	if( model2_copro_type == MODEL2_COPRO_SHARC )
	{
		/* the SHARC waits in halt for its program, DMAed by the i960 */
		tgp_booted = 0;
		cpunum_suspend( CPU_TGP, SUSPEND_REASON_HALT, 1 );
		sharc_set_flag_input( 0, 1 );
		sharc_set_flag_input( 1, 0 );
		return;
	}
	if( model2_copro_type == MODEL2_COPRO_TGPX4 )
	{
		/* the TGPx4 waits in halt for its program, as MAME */
		tgp_booted = 0;
		cpunum_suspend( CPU_TGP, SUSPEND_REASON_HALT, 1 );
		return;
	}
	/* hold TGP in halt until we have code */
	copro_halt();
}


/*************************************
 *  Sound link: i8251 UART (uPD71051)

    As current MAME: 31250 baud MIDI (320 us a byte); the sound
    interrupt (line 10) is up while the UART can send (TxRDY) or has a
    byte from the sound board (RxRDY).

    Model 2A and later: the UART talks to the SCSP MIDI port.
    Original Model 2: it talks to the i8251 on the Model 1 sound board
    (68000 + 2 MultiPCM + YM3438), whose RxRDY is the 68000 IRQ 2.
 *************************************/

#define UART_TXQ	64
#define MIDI_BYTE_TIME	TIME_IN_USEC( 320 )

int model2_sound_board;	/* MODEL2_SND_SCSP or MODEL2_SND_M1 */
int model2_m1_extra;	/* Model 2A with a Model 1 sound board too (Manx TT DX) */

/* CPU of the Model 1 sound board: the sound CPU on the original Model 2, the
   fourth CPU when it is an extra board next to the SCSP */
#define M1SND_CPU	( model2_m1_extra ? 3 : 2 )

static UINT8 uart_txq[ UART_TXQ ];
static int uart_txq_head, uart_txq_count;
static void *uart_timer;

/* Model 1 sound board side */
static UINT8 m1_rx_data, m1_rxq[ UART_TXQ ];
static int m1_rx_full, m1_rxq_head, m1_rxq_count;

static int uart_txrdy( void )	{ return uart_txq_count < 2; }

static int uart_rxrdy( void )
{
	if( model2_sound_board == MODEL2_SND_M1 )
		return m1_rxq_count > 0;
	return SCSP_MidiOutAvail() > 0;
}

void model2_sound_irq_check( void )
{
	if( uart_txrdy() || uart_rxrdy() )
		model2_raise_irq( 1 << 10 );
}

static int uart_sink_free( void )
{
	if( model2_sound_board == MODEL2_SND_M1 )
		return !m1_rx_full;
	/* with an extra Model 1 sound board, a byte goes to both boards */
	if( model2_m1_extra && m1_rx_full )
		return 0;
	return SCSP_MidiInFree() > 0;
}

static void uart_sink_put( UINT8 data )
{
	if( model2_sound_board == MODEL2_SND_M1 || model2_m1_extra )
	{
		m1_rx_data = data;
		m1_rx_full = 1;
		cpu_set_irq_line( M1SND_CPU, 2, ASSERT_LINE );
	}
	if( model2_sound_board == MODEL2_SND_SCSP )
	{
		SCSP_MidiIn( 0, data, 0 );
		segadsb_midi_in( data );	/* the music board on the same line (2C) */
	}
}

static void uart_tx_cb( int param )
{
	if( uart_txq_count > 0 && uart_sink_free() )
	{
		uart_sink_put( uart_txq[ uart_txq_head ] );
		uart_txq_head = ( uart_txq_head + 1 ) % UART_TXQ;
		uart_txq_count--;
	}
	if( uart_txq_count > 0 )
		timer_adjust( uart_timer, MIDI_BYTE_TIME, 0, 0 );
	model2_sound_irq_check();
}

/* data at byte 0, status / command at byte 2 of the dword (umask16 0x00ff) */
READ32_HANDLER( model2_uart_r )
{
	data32_t data = 0;
	if( !( mem_mask & 0x000000ff ) )
	{
		if( model2_sound_board == MODEL2_SND_M1 )
		{
			if( m1_rxq_count > 0 )
			{
				data |= m1_rxq[ m1_rxq_head ];
				m1_rxq_head = ( m1_rxq_head + 1 ) % UART_TXQ;
				m1_rxq_count--;
			}
		}
		else
			data |= SCSP_MidiOutR( 0, 0 ) & 0xff;
	}
	if( !( mem_mask & 0x00ff0000 ) )
	{
		UINT8 status = 0x80;	/* DSR */
		if( uart_txrdy() ) status |= 0x01;
		if( uart_rxrdy() ) status |= 0x02;
		if( uart_txq_count == 0 ) status |= 0x04;
		data |= status << 16;
	}
	return data;
}

WRITE32_HANDLER( model2_uart_w )
{
	if( !( mem_mask & 0x000000ff ) )
	{
		if( uart_txq_count < UART_TXQ )
		{
			uart_txq[ ( uart_txq_head + uart_txq_count ) % UART_TXQ ] = data & 0xff;
			uart_txq_count++;
			if( uart_txq_count == 1 )
				timer_adjust( uart_timer, MIDI_BYTE_TIME, 0, 0 );
		}
	}
	/* command / mode writes (byte 2) need nothing here */
}

/* Model 1 sound board i8251, seen from its 68000 (umask16 0x00ff) */
READ16_HANDLER( model2_m1snd_uart_r )
{
	if( offset == 0 )
	{
		UINT8 data = m1_rx_data;
		if( m1_rx_full )
		{
			m1_rx_full = 0;
			cpu_set_irq_line( M1SND_CPU, 2, CLEAR_LINE );
		}
		return data;
	}
	/* DSR, TxEMPTY, TxRDY and RxRDY */
	return 0x85 | ( m1_rx_full ? 0x02 : 0x00 );
}

WRITE16_HANDLER( model2_m1snd_uart_w )
{
	/* the extra board of Manx TT DX only listens: the i960 reads the SCSP */
	if( model2_m1_extra )
		return;
	if( offset == 0 && !( mem_mask & 0x00ff ) )
	{
		if( m1_rxq_count < UART_TXQ )
		{
			m1_rxq[ ( m1_rxq_head + m1_rxq_count ) % UART_TXQ ] = data & 0xff;
			m1_rxq_count++;
		}
		model2_sound_irq_check();
	}
}

void model2_sound_init( void )
{
	/* SCSP board: copy the 68k vector table into RAM */
	if( model2_sound_board == MODEL2_SND_SCSP )
		memcpy( memory_region( REGION_CPU3 ), memory_region( REGION_USER8 ), 16 );

	uart_timer = timer_alloc( uart_tx_cb );
	state_save_register_UINT8( "model2uart", 0, "txq", uart_txq, UART_TXQ );
	state_save_register_int( "model2uart", 0, "txq_head", &uart_txq_head );
	state_save_register_int( "model2uart", 0, "txq_count", &uart_txq_count );
	state_save_register_UINT8( "model2uart", 0, "m1_rx_data", &m1_rx_data, 1 );
	state_save_register_int( "model2uart", 0, "m1_rx_full", &m1_rx_full );
	state_save_register_UINT8( "model2uart", 0, "m1_rxq", m1_rxq, UART_TXQ );
	state_save_register_int( "model2uart", 0, "m1_rxq_head", &m1_rxq_head );
	state_save_register_int( "model2uart", 0, "m1_rxq_count", &m1_rxq_count );
}

void model2_sound_reset( void )
{
	uart_txq_head = uart_txq_count = 0;
	m1_rx_full = 0;
	m1_rxq_head = m1_rxq_count = 0;
	timer_adjust( uart_timer, TIME_NEVER, 0, 0 );

	/* the 68k vector table is copied into RAM in model2_sound_init (before the
	   CPUs are reset) and stays there */
}

void model2_sound_postload( void )
{
	if( uart_txq_count > 0 )
		timer_adjust( uart_timer, MIDI_BYTE_TIME, 0, 0 );
}
