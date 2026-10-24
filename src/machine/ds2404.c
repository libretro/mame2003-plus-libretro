/***************************************************************************

  Dallas DS2404 EconoRAM Time Chip (3-wire interface)

  Backported from current MAME src/devices/machine/ds2404.cpp: the SRAM
  through the scratchpad, and the 40-bit real time clock that counts in
  1/256 seconds from a reference date.

***************************************************************************/

#include <string.h>
#include <time.h>
#include "driver.h"
#include "state.h"
#include "ds2404.h"

enum
{
	DS2404_IDLE = 0,
	DS2404_COMMAND,
	DS2404_ADDRESS1,
	DS2404_ADDRESS2,
	DS2404_OFFSET,
	DS2404_INIT_COMMAND,
	DS2404_READ_MEMORY,
	DS2404_WRITE_SCRATCHPAD,
	DS2404_READ_SCRATCHPAD,
	DS2404_COPY_SCRATCHPAD
};

static UINT16 ds2404_address;
static UINT16 ds2404_offset;
static UINT16 ds2404_end_offset;
static UINT8 ds2404_a1, ds2404_a2;
static UINT8 ds2404_sram[ 512 ];	/* 4096 bits */
static UINT8 ds2404_ram[ 32 ];		/* the scratchpad: 256 bits */
static UINT8 ds2404_rtc[ 5 ];		/* 40-bit RTC */
static UINT8 ds2404_state[ 8 ];
static UINT8 ds2404_state_ptr;
static mame_timer *ds2404_timer;
static double ds2404_timer_left;	/* time to the next tick, for save states */

static void ds2404_tick( int param )
{
	int i;

	for( i = 0; i < 5; i++ )
	{
		ds2404_rtc[ i ]++;
		if( ds2404_rtc[ i ] != 0 )
			break;
	}
}

/* 0.78 does not save its timers: the tick goes on with the time it had left */
static void ds2404_presave( void )
{
	ds2404_timer_left = timer_timeleft( ds2404_timer );
	if( ds2404_timer_left < 0 )
		ds2404_timer_left = 0;
}

static void ds2404_postload( void )
{
	timer_adjust( ds2404_timer, ds2404_timer_left, 0, TIME_IN_HZ( 256 ) );
}

void ds2404_init( int ref_year, int ref_month, int ref_day )
{
	struct tm ref_tm;
	time_t ref_time, current_time;

	memset( &ref_tm, 0, sizeof( ref_tm ) );
	ref_tm.tm_year = ref_year - 1900;
	ref_tm.tm_mon = ref_month - 1;
	ref_tm.tm_mday = ref_day;
	ref_time = mktime( &ref_tm );

	time( &current_time );
	current_time -= ref_time;

	ds2404_rtc[ 0 ] = 0;
	ds2404_rtc[ 1 ] = ( current_time >> 0 ) & 0xff;
	ds2404_rtc[ 2 ] = ( current_time >> 8 ) & 0xff;
	ds2404_rtc[ 3 ] = ( current_time >> 16 ) & 0xff;
	ds2404_rtc[ 4 ] = ( current_time >> 24 ) & 0xff;

	memset( ds2404_sram, 0, sizeof( ds2404_sram ) );
	memset( ds2404_ram, 0, sizeof( ds2404_ram ) );
	memset( ds2404_state, DS2404_IDLE, sizeof( ds2404_state ) );
	ds2404_state_ptr = 0;
	ds2404_address = 0;
	ds2404_offset = 0;
	ds2404_end_offset = 0;
	ds2404_a1 = ds2404_a2 = 0;

	ds2404_timer = timer_alloc( ds2404_tick );
	timer_adjust( ds2404_timer, TIME_IN_HZ( 256 ), 0, TIME_IN_HZ( 256 ) );
	ds2404_timer_left = 0;

	state_save_register_UINT16( "ds2404", 0, "address", &ds2404_address, 1 );
	state_save_register_UINT16( "ds2404", 0, "offset", &ds2404_offset, 1 );
	state_save_register_UINT16( "ds2404", 0, "end_offset", &ds2404_end_offset, 1 );
	state_save_register_UINT8( "ds2404", 0, "a1", &ds2404_a1, 1 );
	state_save_register_UINT8( "ds2404", 0, "a2", &ds2404_a2, 1 );
	state_save_register_UINT8( "ds2404", 0, "sram", ds2404_sram, sizeof( ds2404_sram ) );
	state_save_register_UINT8( "ds2404", 0, "ram", ds2404_ram, sizeof( ds2404_ram ) );
	state_save_register_UINT8( "ds2404", 0, "rtc", ds2404_rtc, sizeof( ds2404_rtc ) );
	state_save_register_UINT8( "ds2404", 0, "state", ds2404_state, sizeof( ds2404_state ) );
	state_save_register_UINT8( "ds2404", 0, "state_ptr", &ds2404_state_ptr, 1 );
	state_save_register_double( "ds2404", 0, "timer_left", &ds2404_timer_left, 1 );
	state_save_register_func_presave( ds2404_presave );
	state_save_register_func_postload( ds2404_postload );
}

static void ds2404_rom_cmd( UINT8 cmd )
{
	switch( cmd )
	{
	case 0xcc:	/* skip ROM */
		ds2404_state[ 0 ] = DS2404_COMMAND;
		ds2404_state_ptr = 0;
		break;

	default:
		logerror( "DS2404: unknown ROM command %02X\n", cmd );
		break;
	}
}

static void ds2404_cmd( UINT8 cmd )
{
	switch( cmd )
	{
	case 0x0f:	/* write scratchpad */
		ds2404_state[ 0 ] = DS2404_ADDRESS1;
		ds2404_state[ 1 ] = DS2404_ADDRESS2;
		ds2404_state[ 2 ] = DS2404_INIT_COMMAND;
		ds2404_state[ 3 ] = DS2404_WRITE_SCRATCHPAD;
		ds2404_state_ptr = 0;
		break;

	case 0x55:	/* copy scratchpad */
		ds2404_state[ 0 ] = DS2404_ADDRESS1;
		ds2404_state[ 1 ] = DS2404_ADDRESS2;
		ds2404_state[ 2 ] = DS2404_OFFSET;
		ds2404_state[ 3 ] = DS2404_INIT_COMMAND;
		ds2404_state[ 4 ] = DS2404_COPY_SCRATCHPAD;
		ds2404_state_ptr = 0;
		break;

	case 0xf0:	/* read memory */
		ds2404_state[ 0 ] = DS2404_ADDRESS1;
		ds2404_state[ 1 ] = DS2404_ADDRESS2;
		ds2404_state[ 2 ] = DS2404_INIT_COMMAND;
		ds2404_state[ 3 ] = DS2404_READ_MEMORY;
		ds2404_state_ptr = 0;
		break;

	default:
		logerror( "DS2404: unknown command %02X\n", cmd );
		break;
	}
}

static UINT8 ds2404_readmem( void )
{
	if( ds2404_address < 0x200 )
		return ds2404_sram[ ds2404_address ];
	else if( ds2404_address >= 0x202 && ds2404_address <= 0x206 )
		return ds2404_rtc[ ds2404_address - 0x202 ];
	return 0;
}

static void ds2404_writemem( UINT8 value )
{
	if( ds2404_address < 0x200 )
		ds2404_sram[ ds2404_address ] = value;
	else if( ds2404_address >= 0x202 && ds2404_address <= 0x206 )
		ds2404_rtc[ ds2404_address - 0x202 ] = value;
}

WRITE_HANDLER( ds2404_1w_reset_w )
{
	ds2404_state[ 0 ] = DS2404_IDLE;
	ds2404_state_ptr = 0;
}

WRITE_HANDLER( ds2404_3w_reset_w )
{
	ds2404_state[ 0 ] = DS2404_COMMAND;
	ds2404_state_ptr = 0;
}

READ_HANDLER( ds2404_data_r )
{
	UINT8 value = 0;

	switch( ds2404_state[ ds2404_state_ptr ] )
	{
	case DS2404_READ_MEMORY:
		value = ds2404_readmem();
		break;

	case DS2404_READ_SCRATCHPAD:
		if( ds2404_offset < 0x20 )
		{
			value = ds2404_ram[ ds2404_offset ];
			ds2404_offset++;
		}
		break;
	}
	return value;
}

WRITE_HANDLER( ds2404_data_w )
{
	int i;

	switch( ds2404_state[ ds2404_state_ptr ] )
	{
	case DS2404_IDLE:
		ds2404_rom_cmd( data & 0xff );
		break;

	case DS2404_COMMAND:
		ds2404_cmd( data & 0xff );
		break;

	case DS2404_ADDRESS1:
		ds2404_a1 = data & 0xff;
		ds2404_state_ptr++;
		break;

	case DS2404_ADDRESS2:
		ds2404_a2 = data & 0xff;
		ds2404_state_ptr++;
		break;

	case DS2404_OFFSET:
		ds2404_end_offset = data & 0xff;
		ds2404_state_ptr++;
		break;

	case DS2404_WRITE_SCRATCHPAD:
		if( ds2404_offset < 0x20 )
		{
			ds2404_ram[ ds2404_offset ] = data & 0xff;
			ds2404_offset++;
		}
		break;
	}

	if( ds2404_state[ ds2404_state_ptr ] == DS2404_INIT_COMMAND )
	{
		switch( ds2404_state[ ds2404_state_ptr + 1 ] )
		{
		case DS2404_READ_MEMORY:
			ds2404_address = ( ds2404_a2 << 8 ) | ds2404_a1;
			ds2404_address -= 1;
			break;

		case DS2404_WRITE_SCRATCHPAD:
		case DS2404_READ_SCRATCHPAD:
			ds2404_address = ( ds2404_a2 << 8 ) | ds2404_a1;
			ds2404_offset = ds2404_address & 0x1f;
			break;

		case DS2404_COPY_SCRATCHPAD:
			ds2404_address = ( ds2404_a2 << 8 ) | ds2404_a1;
			for( i = 0; i <= ds2404_end_offset; i++ )
			{
				/* MAME reads past the 32 bytes of the scratchpad here: stay in it */
				ds2404_writemem( ds2404_ram[ i & 0x1f ] );
				ds2404_address++;
			}
			break;
		}
		ds2404_state_ptr++;
	}
}

WRITE_HANDLER( ds2404_clk_w )
{
	if( ds2404_state[ ds2404_state_ptr ] == DS2404_READ_MEMORY )
		ds2404_address++;
}

void ds2404_nvram_load( mame_file *file )
{
	mame_fread( file, ds2404_sram, sizeof( ds2404_sram ) );
}

void ds2404_nvram_save( mame_file *file )
{
	mame_fwrite( file, ds2404_sram, sizeof( ds2404_sram ) );
}
