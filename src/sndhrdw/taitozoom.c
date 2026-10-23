/***************************************************************************

  Taito Zoom ZSG-2 sound board (Taito FX-1B)

  Backported from current MAME src/mame/sony/taito_zm.cpp (Olivier
  Galibert, hap, superctr, cam900):

  Panasonic MN1020012A sound CPU, 12.5MHz (25MHz / 2)
  Zoom Corp. ZSG-2 PCM chip, 25MHz: 48 voices at 25MHz / 768
  Texas Instruments TMS57002 DSP, 12.5MHz: reverb / chorus

  MN10200 map: program ROM 0x080000-0x0fffff, RAM 0x400000-0x41ffff,
  ZSG-2 0x800000-0x8007ff, TMS57002 host port 0xc00000, shared RAM with
  the main CPU 0xe00000-0xe000ff (M66220FP). Port 1 drives the TMS57002
  program / coefficient load lines.

  The ZSG-2 has four outputs: reverb and chorus sends to the DSP, and the
  direct left / right. The two main board registers 4 / 5 set the global
  volume left / right, at the DSP output.

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "cpu/mn10200/mn10200.h"
#include "sound/zsg2.h"
#include "cpu/tms57002/tms57002.h"
#include "taitozoom.h"

#define ZSG2_RATE		( 25000000 / 768 )
#define TMS_CYCLES_PER_SAMPLE	( ( 25000000 / 2 ) / ZSG2_RATE )	/* 384 */
#define ZOOM_CPU		1

static UINT8 zoom_shared[ 0x100 ];
static data16_t zoom_ram[ 0x20000 / 2 ];
static UINT16 zoom_reg_address;
static UINT8 zoom_tms_ctrl;
static UINT8 zoom_gain[ 2 ];			/* global volume left / right, 0-63 */
static int zoom_stream = -1;

/* the TMS57002 "update FIFO empty" line goes, inverted, to the MN10200 IRQ 1 */
static int zoom_tms_line = -1;		/* last state given to the line (-1: unknown) */

static void zoom_tms_empty( int empty )
{
	/* the DSP gives it at every FIFO access: only a change goes to the CPU, */
	/* each one costs a timeslice */
	if( empty != zoom_tms_line )
	{
		zoom_tms_line = empty;
		cpu_set_irq_line( ZOOM_CPU, MN10200_IRQ1, empty ? CLEAR_LINE : ASSERT_LINE );
	}
}

/*
	The DSP runs from the stream, so it only advances when the stream is
	brought up to date. It takes the coefficient updates written by the
	MN10200 within one sample period: while the FIFO is not empty, bring the
	stream up to date every sample, so the IRQ 1 line drops as on the board
	(else it would stay up until the next stream update).
*/
static void *zoom_dsp_timer;
static int zoom_dsp_pending;
static double zoom_dsp_left;		/* time left on the kick, for save states (-1: none) */

static void zoom_dsp_kick( int param )
{
	int before = tms57002dsp_fifo_count();
	zoom_dsp_pending = 0;
	stream_update( zoom_stream, 0 );
	/* again while the DSP takes them; a DSP not reading them (idle, or not
	   the address) would only cost a timer per sample */
	if( !tms57002dsp_empty_r() && tms57002dsp_fifo_count() < before )
	{
		zoom_dsp_pending = 1;
		timer_adjust( zoom_dsp_timer, TIME_IN_HZ( ZSG2_RATE ), 0, 0 );
	}
}

static void zoom_dsp_arm( void )
{
	if( !tms57002dsp_empty_r() && !zoom_dsp_pending )
	{
		zoom_dsp_pending = 1;
		timer_adjust( zoom_dsp_timer, TIME_IN_HZ( ZSG2_RATE ), 0, 0 );
	}
}

/* MN10200 port 1: the TMS57002 control */
static data8_t zoom_port_r( int port )
{
	return port == 1 ? zoom_tms_ctrl : 0xff;
}

static void zoom_port_w( int port, data8_t data )
{
	if( port != 1 )
		return;
	/* bit 1: CLOAD, bit 0: PLOAD (other bits unknown, 0x9f in most games) */
	stream_update( zoom_stream, 0 );
	tms57002dsp_cload_w( data & 2 );
	tms57002dsp_pload_w( data & 1 );
	zoom_tms_ctrl = data;
}

/* MN10200 map */

static READ16_HANDLER( zoom_ram_r )
{
	return zoom_ram[ offset ];
}

static WRITE16_HANDLER( zoom_ram_w )
{
	COMBINE_DATA( &zoom_ram[ offset ] );
}

static READ16_HANDLER( zoom_zsg2_r )
{
	stream_update( zoom_stream, 0 );
	return zsg2_read( offset );
}

static WRITE16_HANDLER( zoom_zsg2_w )
{
	/* the ZSG-2 only takes full 16-bit accesses */
	if( mem_mask != 0 )
		return;
	stream_update( zoom_stream, 0 );
	zsg2_write( offset, data );
}

static READ16_HANDLER( zoom_tms_r )
{
	stream_update( zoom_stream, 0 );
	return 0xff00 | tms57002dsp_data_r();
}

static WRITE16_HANDLER( zoom_tms_w )
{
	if( ACCESSING_LSB )
	{
		stream_update( zoom_stream, 0 );
		tms57002dsp_data_w( data & 0xff );
		zoom_dsp_arm();
	}
}

static READ16_HANDLER( zoom_shared16_r )
{
	return zoom_shared[ offset * 2 ] | ( zoom_shared[ offset * 2 + 1 ] << 8 );
}

static WRITE16_HANDLER( zoom_shared16_w )
{
	if( ACCESSING_LSB )
		zoom_shared[ offset * 2 ] = data & 0xff;
	if( ACCESSING_MSB )
		zoom_shared[ offset * 2 + 1 ] = data >> 8;
}

MEMORY_READ16_START( taitozoom_mn_readmem )
	{ 0x080000, 0x0fffff, MRA16_ROM },
	{ 0x400000, 0x41ffff, zoom_ram_r },
	{ 0x800000, 0x8007ff, zoom_zsg2_r },
	{ 0xc00000, 0xc00001, zoom_tms_r },
	{ 0xe00000, 0xe000ff, zoom_shared16_r },
MEMORY_END

/* G-NET: the samples in the flashes */
static UINT32 (*zoom_flash_samples_r)( UINT32 offset );

void taitozoom_set_flash( UINT32 (*samples_r)( UINT32 offset ) )
{
	zoom_flash_samples_r = samples_r;
}

MEMORY_WRITE16_START( taitozoom_mn_writemem )
	{ 0x080000, 0x0fffff, MWA16_ROM },
	{ 0x400000, 0x41ffff, zoom_ram_w },
	{ 0x800000, 0x8007ff, zoom_zsg2_w },
	{ 0xc00000, 0xc00001, zoom_tms_w },
	{ 0xe00000, 0xe000ff, zoom_shared16_w },
MEMORY_END

/* main CPU side */

WRITE32_HANDLER( taitozoom_reg_w )
{
	if( !( mem_mask & 0xffff0000 ) )		/* 0x1fb80002: register address */
		zoom_reg_address = ( data >> 16 ) & 0xff;
	if( ACCESSING_LSW32 )					/* 0x1fb80000: register data */
	{
		switch( zoom_reg_address )
		{
		case 0x04:	/* zsg2+dsp global volume left */
			stream_update( zoom_stream, 0 );
			zoom_gain[ 0 ] = data & 0x3f;
			break;
		case 0x05:	/* zsg2+dsp global volume right */
			stream_update( zoom_stream, 0 );
			zoom_gain[ 1 ] = data & 0x3f;
			break;
		}
	}
}

WRITE32_HANDLER( taitozoom_sound_irq_w )
{
	cpu_set_irq_line( ZOOM_CPU, MN10200_IRQ0, ASSERT_LINE );
	cpu_set_irq_line( ZOOM_CPU, MN10200_IRQ0, CLEAR_LINE );
}

READ32_HANDLER( taitozoom_sound_irq_r )
{
	return 0;	/* read before writing the irq, bit 0 = busy? (as MAME) */
}

READ32_HANDLER( taitozoom_shared_r )
{
	return zoom_shared[ ( offset * 2 ) & 0xff ] | ( zoom_shared[ ( offset * 2 + 1 ) & 0xff ] << 16 );
}

WRITE32_HANDLER( taitozoom_shared_w )
{
	if( ACCESSING_LSB32 )
		zoom_shared[ ( offset * 2 ) & 0xff ] = data & 0xff;
	if( !( mem_mask & 0x00ff0000 ) )
		zoom_shared[ ( offset * 2 + 1 ) & 0xff ] = ( data >> 16 ) & 0xff;
}

/* sound */

static void zoom_stream_update( int param, INT16 **buffer, int length )
{
	static INT32 out[ 4 ][ 512 ];
	INT32 *outs[ 4 ];
	INT32 in[ 4 ], dsp[ 4 ];
	int i, pos = 0;

	outs[ 0 ] = out[ 0 ]; outs[ 1 ] = out[ 1 ]; outs[ 2 ] = out[ 2 ]; outs[ 3 ] = out[ 3 ];
	while( pos < length )
	{
		int n = length - pos;
		if( n > 512 )
			n = 512;
		zsg2_update( outs, n );
		for( i = 0; i < n; i++ )
		{
			INT32 l, r;
			/* ZSG-2 to the DSP: reverb and chorus sends at 0.5, direct at 1.0 */
			in[ 0 ] = out[ 0 ][ i ] / 2;
			in[ 1 ] = out[ 1 ][ i ] / 2;
			in[ 2 ] = out[ 2 ][ i ];
			in[ 3 ] = out[ 3 ][ i ];
			tms57002dsp_sample( in, dsp, TMS_CYCLES_PER_SAMPLE );
			/* DSP outputs 2 / 3 with the global volume */
			l = ( dsp[ 2 ] * zoom_gain[ 0 ] ) / 63;
			r = ( dsp[ 3 ] * zoom_gain[ 1 ] ) / 63;
			buffer[ 0 ][ pos + i ] = l < -32768 ? -32768 : l > 32767 ? 32767 : l;
			buffer[ 1 ][ pos + i ] = r < -32768 ? -32768 : r > 32767 ? 32767 : r;
		}
		pos += n;
	}
}

static int zoom_sh_start( const struct MachineSound *msound )
{
	const char *names[ 2 ] = { "Taito Zoom L", "Taito Zoom R" };
	int vols[ 2 ];
	vols[ 0 ] = MIXER( 100, MIXER_PAN_LEFT );
	vols[ 1 ] = MIXER( 100, MIXER_PAN_RIGHT );

	if( zoom_flash_samples_r != NULL )
	{
		/* G-NET: 6MB of samples in the three flashes */
		zsg2_init( NULL, 0x600000 );
		zsg2_set_ext_read( zoom_flash_samples_r );
	}
	else
		zsg2_init( memory_region( REGION_SOUND1 ), memory_region_length( REGION_SOUND1 ) );
	zsg2_reset();
	tms57002dsp_init();
	tms57002dsp_set_empty_callback( zoom_tms_empty );
	zoom_dsp_timer = timer_alloc( zoom_dsp_kick );
	tms57002dsp_reset();
	zoom_stream = stream_init_multi( 2, names, vols, ZSG2_RATE, 0, zoom_stream_update );
	return zoom_stream < 0;
}

struct CustomSound_interface taitozoom_custom_interface =
{
	zoom_sh_start, NULL, NULL
};

/* init */

/* 0.78 does not save its timers: the kick goes on with the time it had left */
static void taitozoom_presave( void )
{
	zoom_dsp_left = zoom_dsp_pending ? timer_timeleft( zoom_dsp_timer ) : -1;
}

static void taitozoom_postload( void )
{
	zoom_tms_line = -1;
	if( zoom_dsp_left >= 0 )
	{
		zoom_dsp_pending = 1;
		timer_adjust( zoom_dsp_timer, zoom_dsp_left, 0, 0 );
	}
	else
	{
		zoom_dsp_pending = 0;
		timer_adjust( zoom_dsp_timer, TIME_NEVER, 0, 0 );
		zoom_dsp_arm();
	}
}

void taitozoom_init( void )
{
	zoom_flash_samples_r = NULL;
	mn10200_set_port_handlers( zoom_port_r, zoom_port_w );
	memset( zoom_shared, 0, sizeof( zoom_shared ) );

	state_save_register_UINT8( "taitozoom", 0, "shared", zoom_shared, sizeof( zoom_shared ) );
	state_save_register_UINT16( "taitozoom", 0, "ram", zoom_ram, sizeof( zoom_ram ) / 2 );
	state_save_register_UINT16( "taitozoom", 0, "reg_address", &zoom_reg_address, 1 );
	state_save_register_UINT8( "taitozoom", 0, "tms_ctrl", &zoom_tms_ctrl, 1 );
	state_save_register_UINT8( "taitozoom", 0, "gain", zoom_gain, 2 );
	zsg2_state_register();
	tms57002dsp_state_register();
	state_save_register_double( "taitozoom", 0, "dsp_left", &zoom_dsp_left, 1 );
	state_save_register_func_presave( taitozoom_presave );
	state_save_register_func_postload( taitozoom_postload );
}

void taitozoom_device_reset( void )
{
	if( zoom_stream >= 0 )
		stream_update( zoom_stream, 0 );
	zoom_reg_address = 0;
	zsg2_reset();
	tms57002dsp_reset();
}

void taitozoom_reset( void )
{
	zoom_tms_line = -1;
	zoom_reg_address = 0;
	zoom_tms_ctrl = 0;
	zoom_gain[ 0 ] = zoom_gain[ 1 ] = 63;
	zsg2_reset();
	tms57002dsp_reset();
}
