/***************************************************************************

  Intel / Sharp 16-bit flash memories, the Intel command set

  Backported from current MAME src/devices/machine/intelfsh.cpp. A block
  erase takes the time of MAME (1 second for a 64KB block, 300 / 600 ms
  for the E28F400B boot blocks); the status reads busy until then.

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "intelfsh.h"

enum
{
	FM_NORMAL = 0,		/* read array */
	FM_READID,
	FM_READSTATUS,
	FM_WRITEPART1,		/* next write: the word to program */
	FM_CLEARPART1,		/* next write: the erase confirm */
	FM_SETMASTER,		/* next write: the master lock confirm */
	FM_WRITEBUFFER1,	/* next write: the word count of a buffered program */
	FM_WRITEBUFFER2		/* the words of a buffered program, then the confirm */
};

struct intelfsh_chip
{
	int type;
	UINT16 *data;
	UINT32 words;
	UINT8 maker_id;
	UINT16 device_id;
	UINT8 mode;
	UINT8 status;
	UINT8 master_lock;
	mame_timer *timer;
	double timer_left;	/* time left on the timer, for save states (-1: none) */
	/* the write buffer of the StrataFlash (28F640J5), 16 words */
	UINT16 buffer[ 16 ];
	UINT32 buffer_start;
	UINT8 buffer_count;
	UINT8 buffer_used;
};

static struct intelfsh_chip chips[ INTELFSH_CHIPS_MAX ];

static void intelfsh_delay( int chip )
{
	struct intelfsh_chip *c = &chips[ chip ];

	/* the erase is done */
	if( c->mode == FM_READSTATUS )
		c->status = 0x80;
}

/* 0.78 does not save its timers: an erase goes on with the time it had left */
static void intelfsh_presave( void )
{
	int i;

	for( i = 0; i < INTELFSH_CHIPS_MAX; i++ )
	{
		if( chips[ i ].data != NULL )
		{
			chips[ i ].timer_left = ( chips[ i ].status & 0x80 ) ? -1 : timer_timeleft( chips[ i ].timer );
			/* a timer due in this timeslice can be a little past its time */
			if( !( chips[ i ].status & 0x80 ) && chips[ i ].timer_left < 0 )
				chips[ i ].timer_left = 0;
		}
	}
}

static void intelfsh_postload( void )
{
	int i;

	for( i = 0; i < INTELFSH_CHIPS_MAX; i++ )
	{
		if( chips[ i ].data != NULL )
		{
			if( chips[ i ].timer_left >= 0 )
				timer_adjust( chips[ i ].timer, chips[ i ].timer_left, i, 0 );
			else
				timer_adjust( chips[ i ].timer, TIME_NEVER, 0, 0 );
		}
	}
}

void intelfsh_init( int chip, int type, UINT16 *data )
{
	struct intelfsh_chip *c = &chips[ chip ];
	UINT32 bytes;

	/* chip 0 comes first: forget the chips of the game before */
	if( chip == 0 )
		memset( chips, 0, sizeof( chips ) );
	memset( c, 0, sizeof( *c ) );
	c->type = type;
	switch( type )
	{
	case INTELFSH_INTEL_E28F400B:
		bytes = 0x80000;
		c->maker_id = 0x89;		/* Intel */
		c->device_id = 0x4471;
		break;

	case INTELFSH_INTEL_28F640J5:
		bytes = 0x800000;
		c->maker_id = 0x89;		/* Intel */
		c->device_id = 0x15;
		break;

	default:
	case INTELFSH_INTEL_TE28F160:
		bytes = 0x200000;
		c->maker_id = 0xb0;		/* Sharp */
		c->device_id = 0xd0;
		break;
	}
	c->words = bytes / 2;
	c->data = data != NULL ? data : auto_malloc( bytes );
	memset( c->data, 0xff, bytes );
	c->mode = FM_NORMAL;
	c->status = 0x80;
	c->timer = timer_alloc( intelfsh_delay );
	c->timer_left = -1;

	state_save_register_UINT16( "intelfsh", chip, "data", c->data, c->words );
	state_save_register_UINT8( "intelfsh", chip, "mode", &c->mode, 1 );
	state_save_register_UINT8( "intelfsh", chip, "status", &c->status, 1 );
	state_save_register_UINT8( "intelfsh", chip, "master_lock", &c->master_lock, 1 );
	state_save_register_double( "intelfsh", chip, "timer_left", &c->timer_left, 1 );
	if( type == INTELFSH_INTEL_28F640J5 )
	{
		state_save_register_UINT16( "intelfsh", chip, "buffer", c->buffer, 16 );
		state_save_register_UINT32( "intelfsh", chip, "buffer_start", &c->buffer_start, 1 );
		state_save_register_UINT8( "intelfsh", chip, "buffer_count", &c->buffer_count, 1 );
		state_save_register_UINT8( "intelfsh", chip, "buffer_used", &c->buffer_used, 1 );
	}
	if( chip == 0 )
	{
		state_save_register_func_presave( intelfsh_presave );
		state_save_register_func_postload( intelfsh_postload );
	}
}

UINT16 *intelfsh_data( int chip )
{
	return chips[ chip ].data;
}

UINT32 intelfsh_size( int chip )
{
	return chips[ chip ].words * 2;
}

void intelfsh_reset( int chip )
{
	struct intelfsh_chip *c = &chips[ chip ];

	/* a hardware reset ends any operation and goes back to read array */
	c->mode = FM_NORMAL;
	c->status = 0x80;
	timer_adjust( c->timer, TIME_NEVER, 0, 0 );
}

UINT16 intelfsh_read( int chip, UINT32 offset )
{
	struct intelfsh_chip *c = &chips[ chip ];

	switch( c->mode )
	{
	default:
	case FM_NORMAL:
		return c->data[ offset % c->words ];

	case FM_READSTATUS:
	case FM_WRITEBUFFER2:
		return c->status;

	case FM_WRITEBUFFER1:
		return 0x80;	/* the extended status: a write buffer is free */

	case FM_READID:
		switch( offset & 0xff )
		{
		case 0: return c->maker_id;
		case 1: return c->device_id;
		case 2: return 0;	/* block lock config */
		case 3: return c->master_lock;
		}
		return 0;
	}
}

static void intelfsh_erase( int chip, UINT32 offset )
{
	struct intelfsh_chip *c = &chips[ chip ];
	UINT32 base, size;
	double duration;

	if( c->type == INTELFSH_INTEL_E28F400B )
	{
		/* 16KB, 2 x 8KB, 96KB, then 128KB blocks */
		base = ( offset & 0x3ffff ) * 2;
		if( base < 0x4000 )
		{
			base = 0;
			size = 0x4000;
			duration = TIME_IN_MSEC( 300 );
		}
		else if( base < 0x8000 )
		{
			base &= 0x6000;
			size = 0x2000;
			duration = TIME_IN_MSEC( 300 );
		}
		else if( base < 0x20000 )
		{
			base = 0x8000;
			size = 0x18000;
			duration = TIME_IN_MSEC( 600 );
		}
		else
		{
			base &= 0x60000;
			size = 0x20000;
			duration = TIME_IN_MSEC( 600 );
		}
	}
	else
	{
		base = ( ( offset % c->words ) * 2 ) & ~0xffff;
		size = 0x10000;
		duration = TIME_IN_SEC( 1 );
	}
	memset( c->data + base / 2, 0xff, size );
	timer_adjust( c->timer, duration, chip, 0 );
}

void intelfsh_write( int chip, UINT32 offset, UINT16 data )
{
	struct intelfsh_chip *c = &chips[ chip ];

	switch( c->mode )
	{
	case FM_NORMAL:
	case FM_READSTATUS:
	case FM_READID:
		switch( data & 0xff )
		{
		case 0xf0:
		case 0xff:	/* read array */
			c->mode = FM_NORMAL;
			break;
		case 0x90:	/* read ID */
			c->mode = FM_READID;
			break;
		case 0x40:
		case 0x10:	/* program */
			c->mode = FM_WRITEPART1;
			break;
		case 0x50:	/* clear status */
			c->status = 0x80;
			c->mode = FM_READSTATUS;
			break;
		case 0x20:	/* block erase */
			c->mode = FM_CLEARPART1;
			break;
		case 0x60:	/* set master lock */
			c->mode = FM_SETMASTER;
			break;
		case 0x70:	/* read status */
			c->mode = FM_READSTATUS;
			break;
		case 0xe8:	/* write to buffer (StrataFlash) */
			if( c->type == INTELFSH_INTEL_28F640J5 )
				c->mode = FM_WRITEBUFFER1;
			else
				logerror( "intelfsh %d: unknown command %02x\n", chip, data & 0xff );
			break;
		default:
			logerror( "intelfsh %d: unknown command %02x\n", chip, data & 0xff );
			break;
		}
		break;

	case FM_WRITEPART1:
		c->data[ offset % c->words ] &= data;
		c->status = 0x80;
		c->mode = FM_READSTATUS;
		break;

	case FM_CLEARPART1:
		if( ( data & 0xff ) == 0xd0 )
		{
			intelfsh_erase( chip, offset );
			c->status = 0x00;
			c->mode = FM_READSTATUS;
		}
		else
			logerror( "intelfsh %d: unexpected %02x in block erase\n", chip, data & 0xff );
		break;

	case FM_WRITEBUFFER1:
		/* the number of words less one (MAME clamps it to the buffer) */
		c->buffer_count = ( data > 0xf ? 0xf : data ) + 1;
		c->buffer_used = 0;
		c->status = 0x80;
		c->mode = FM_WRITEBUFFER2;
		break;

	case FM_WRITEBUFFER2:
		if( c->buffer_used < c->buffer_count )
		{
			if( c->buffer_used == 0 )
				c->buffer_start = offset;
			if( offset >= c->buffer_start + c->buffer_count )
			{
				/* outside the block of words: error, abort */
				c->status = 0x30;
				c->mode = FM_READSTATUS;
			}
			else
				c->buffer[ c->buffer_used++ ] = data;
		}
		else
		{
			if( ( data & 0xff ) == 0xd0 )
			{
				/* the confirm: program the words, from the start address */
				UINT32 i;
				for( i = 0; i < c->buffer_count; i++ )
					c->data[ ( c->buffer_start + i ) % c->words ] &= c->buffer[ i ];
				c->status = 0x80;
			}
			else
				c->status = 0x30;	/* invalid sequence */
			c->mode = FM_READSTATUS;
		}
		break;

	case FM_SETMASTER:
		switch( data & 0xff )
		{
		case 0xf1:
			c->master_lock = 1;
			break;
		case 0xd0:
			c->master_lock = 0;
			break;
		default:
			logerror( "intelfsh %d: unexpected %02x in set master lock\n", chip, data & 0xff );
			break;
		}
		c->mode = FM_READSTATUS;
		break;
	}
}

void intelfsh_nvram_load( int chip, mame_file *file )
{
	struct intelfsh_chip *c = &chips[ chip ];
	UINT8 buffer[ 0x1000 ];
	UINT32 i, j;

	for( i = 0; i < c->words; i += sizeof( buffer ) / 2 )
	{
		mame_fread( file, buffer, sizeof( buffer ) );
		for( j = 0; j < sizeof( buffer ) / 2; j++ )
			c->data[ i + j ] = buffer[ j * 2 ] | ( buffer[ j * 2 + 1 ] << 8 );
	}
}

void intelfsh_nvram_save( int chip, mame_file *file )
{
	struct intelfsh_chip *c = &chips[ chip ];
	UINT8 buffer[ 0x1000 ];
	UINT32 i, j;

	for( i = 0; i < c->words; i += sizeof( buffer ) / 2 )
	{
		for( j = 0; j < sizeof( buffer ) / 2; j++ )
		{
			buffer[ j * 2 ] = c->data[ i + j ] & 0xff;
			buffer[ j * 2 + 1 ] = c->data[ i + j ] >> 8;
		}
		mame_fwrite( file, buffer, sizeof( buffer ) );
	}
}
