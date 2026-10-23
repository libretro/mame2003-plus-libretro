/***************************************************************************

  ATA flash PC card behind a Ricoh RF5C296 PC Card controller (Taito G-NET)

  Backported from current MAME:
  - src/devices/machine/rf5c296.cpp: the controller. Its I/O space holds
    the ATA registers of the card, its memory space the attribute memory
    of the card (CIS, configuration registers). Index / data registers at
    0x3e0 / 0x3e1; register 3 bit 6 low resets the card.
  - src/devices/bus/pccard/ataflash.cpp: the ATA flash card and the three
    Taito ones, locked until the game gives the key of the CHD:
    PC card type 1: the key bytes written at 0x280-0x288 of the attribute
      memory (read back at 0x201, 0 when unlocked);
    PC card type 2: command 0xfe, then command 0xfc and a sector with the
      key at bytes 2-6;
    Compact Flash: command 0x0f, the key in the feature, sector count,
      sector number and cylinder registers.
  - src/devices/machine/atahle.cpp, atastorage.cpp: the ATA device (a
    Compact Flash: no seek time), PIO only. The card has no interrupt and
    no DMA lines on this board.

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "harddisk.h"
#include "ataflash.h"

/* ATA registers */
enum
{
	CS0_DATA = 0,
	CS0_ERROR_FEATURE,
	CS0_SECTOR_COUNT,
	CS0_SECTOR_NUMBER,
	CS0_CYLINDER_LOW,
	CS0_CYLINDER_HIGH,
	CS0_DEVICE_HEAD,
	CS0_STATUS_COMMAND
};

#define CS1_ALTSTATUS_CONTROL		( 6 )
#define CS1_ACTIVE_STATUS			( 7 )

#define STATUS_ERR		0x01
#define STATUS_IDX		0x02
#define STATUS_DRQ		0x08
#define STATUS_DSC		0x10
#define STATUS_DRDY		0x40
#define STATUS_BSY		0x80

#define ERROR_NONE			0x00
#define ERROR_DIAG_PASSED	0x01
#define ERROR_ABRT			0x04
#define ERROR_BAD_SECTOR	0x80

#define CONTROL_SRST		0x04

#define HEAD_HS				0x0f
#define HEAD_DRV			0x10
#define HEAD_L				0x40

#define CMD_RECALIBRATE				0x10
#define CMD_READ_SECTORS			0x20
#define CMD_READ_SECTORS_NORETRY	0x21
#define CMD_READ_SECTORS_EXT		0x24
#define CMD_READ_DMA_EXT			0x25
#define CMD_READ_MULTIPLE_EXT		0x29
#define CMD_WRITE_SECTORS			0x30
#define CMD_WRITE_SECTORS_NORETRY	0x31
#define CMD_WRITE_SECTORS_EXT		0x34
#define CMD_WRITE_DMA_EXT			0x35
#define CMD_WRITE_MULTIPLE_EXT		0x39
#define CMD_VERIFY_SECTORS			0x40
#define CMD_VERIFY_SECTORS_NORETRY	0x41
#define CMD_VERIFY_SECTORS_EXT		0x42
#define CMD_SEEK					0x70
#define CMD_DIAGNOSTIC				0x90
#define CMD_SET_CONFIG				0x91
#define CMD_READ_MULTIPLE			0xc4
#define CMD_WRITE_MULTIPLE			0xc5
#define CMD_SET_BLOCK_COUNT			0xc6
#define CMD_READ_DMA				0xc8
#define CMD_WRITE_DMA				0xca
#define CMD_IDLE_IMMEDIATE			0xe1
#define CMD_IDLE					0xe3
#define CMD_READ_BUFFER				0xe4
#define CMD_CACHE_FLUSH				0xe7
#define CMD_WRITE_BUFFER			0xe8
#define CMD_IDENTIFY_DEVICE			0xec
#define CMD_SET_FEATURES			0xef
#define CMD_SECURITY_UNLOCK			0xf2
#define CMD_SECURITY_DISABLE		0xf6
#define CMD_READ_NATIVE_MAX			0xf8
#define CMD_SET_MAX					0xf9
#define CMD_TAITO_CF_UNLOCK			0x0f
#define CMD_TAITO_GNET_UNLOCK_1		0xfe
#define CMD_TAITO_GNET_UNLOCK_2		0xfc

/* what the busy timer does when it ends */
enum
{
	PARAM_DETECT_DEVICE1 = 0,
	PARAM_DIAGNOSTIC,
	PARAM_COMMAND,
	PARAM_RESET
};

#define MINIMUM_COMMAND_TIME	TIME_IN_USEC( 10 )
#define DIAGNOSTIC_TIME			TIME_IN_MSEC( 2 )
#define DETECT_DEVICE1_TIME		TIME_IN_MSEC( 2 )
#define TIME_BETWEEN_SECTORS	TIME_IN_NSEC( 400 )
#define TIME_PER_SECTOR_WRITE	TIME_IN_USEC( 100 )
#define TIME_PER_ROTATION		( 1.0 / ( 5400 / 60 ) )

#define SECTOR_SIZE		( 512 )

#define METADATA_TAG( a, b, c, d )	( ( (UINT32)( a ) << 24 ) | ( (UINT32)( b ) << 16 ) | ( (UINT32)( c ) << 8 ) | (UINT32)( d ) )

static int card_type;
static struct hard_disk_file *card_disk;
static UINT32 num_cylinders, num_heads, num_sectors;
static UINT8 can_identify;
static UINT16 identify_buffer[ 256 ];
static UINT8 cis[ 512 ];
static UINT8 key[ 5 ];
static UINT8 has_key;

/* card state */
static UINT8 buffer[ SECTOR_SIZE ];
static UINT16 buffer_offset, buffer_size;
static UINT8 reg_error, reg_feature, reg_sector_count, reg_sector_number;
static UINT8 reg_cylinder_low, reg_cylinder_high, reg_device_head;
static UINT8 reg_status, reg_command, reg_device_control;
static UINT8 transfers_8bit, single_device, resetting;
static UINT16 block_count, sectors_until_int;
static UINT32 sectors_remaining, cur_lba;
static UINT16 locked;
static UINT8 config_option, config_status, pin_replacement;
static UINT8 rf5c296_reg;

static mame_timer *busy_timer;
static INT32 busy_param;
static double busy_left;		/* time left on the busy timer, for save states (-1: none) */
static double idx_time;			/* when the index bit was last given */

static void soft_reset( void );
static void finished_command( void );
static void fill_buffer( void );
static void finished_read( void );
static void finished_write( void );

static int device_selected( void )
{
	return ( reg_device_head & HEAD_DRV ) == 0;	/* the card is device 0 */
}

static int is_ready( void )
{
	return locked == 0;
}

static void start_busy( double time, int param )
{
	reg_status |= STATUS_BSY;
	busy_param = param;
	timer_adjust( busy_timer, time, param, 0 );
}

static void stop_busy( void )
{
	reg_status &= ~STATUS_BSY;
	timer_adjust( busy_timer, TIME_NEVER, 0, 0 );
}

static void signature( void )
{
	reg_sector_count = 1;
	reg_sector_number = 1;
	reg_cylinder_low = 0;
	reg_cylinder_high = 0;
	reg_device_head = 0;
}

static void finished_diagnostic( void )
{
	resetting = 0;
	if( is_ready() )
		reg_status |= STATUS_DRDY;
	signature();
}

static void start_diagnostic( void )
{
	reg_error = can_identify ? ERROR_DIAG_PASSED : ERROR_NONE;
	/* device 0 alone on the bus: no device 1 to wait for */
	finished_diagnostic();
}

static void busy_tick( int param )
{
	reg_status &= ~STATUS_BSY;
	switch( param )
	{
	case PARAM_DETECT_DEVICE1:
		single_device = 1;
		soft_reset();
		break;
	case PARAM_DIAGNOSTIC:
		start_diagnostic();
		break;
	case PARAM_COMMAND:
		finished_command();
		break;
	}
}

static void soft_reset( void )
{
	buffer_offset = 0;
	buffer_size = 0;
	reg_status = 0;
	start_busy( DIAGNOSTIC_TIME, PARAM_DIAGNOSTIC );
	cur_lba = 0;
	reg_status |= STATUS_DSC;
}

static UINT8 calculate_status( void )
{
	UINT8 result = reg_status;
	double now = timer_get_time();

	if( now - idx_time > TIME_PER_ROTATION )
	{
		result |= STATUS_IDX;
		idx_time = now;
	}
	return result;
}

/* sectors */

static UINT32 lba_address( void )
{
	if( reg_device_head & HEAD_L )
		return ( ( reg_device_head & HEAD_HS ) << 24 ) | ( reg_cylinder_high << 16 ) | ( reg_cylinder_low << 8 ) | reg_sector_number;
	return ( ( ( ( ( reg_cylinder_high << 8 ) | reg_cylinder_low ) * num_heads ) + ( reg_device_head & HEAD_HS ) ) * num_sectors ) + reg_sector_number - 1;
}

static UINT32 count_remaining( void )
{
	return reg_sector_count ? reg_sector_count : 0x100;
}

static void next_sector( void )
{
	UINT8 cur_head = reg_device_head & HEAD_HS;

	if( reg_device_head & HEAD_L )
	{
		reg_sector_number++;
		if( reg_sector_number == 0 )
		{
			reg_cylinder_low++;
			if( reg_cylinder_low == 0 )
			{
				reg_cylinder_high++;
				if( reg_cylinder_high == 0 )
					cur_head++;
			}
		}
	}
	else
	{
		/* sectors are 1-based, heads 0-based */
		reg_sector_number++;
		if( reg_sector_number > num_sectors )
		{
			reg_sector_number = 1;
			cur_head++;
			if( cur_head >= num_heads )
			{
				cur_head = 0;
				reg_cylinder_low++;
				if( reg_cylinder_low == 0 )
					reg_cylinder_high++;
			}
		}
	}
	reg_device_head = ( reg_device_head & ~HEAD_HS ) | ( cur_head & HEAD_HS );
	cur_lba = lba_address();
}

static int read_sector( UINT32 lba )
{
	if( card_disk == NULL )
		return 0;
	return hard_disk_read( card_disk, lba, 1, buffer ) == 1;
}

static int write_sector( UINT32 lba )
{
	if( card_disk == NULL )
		return 0;
	return hard_disk_write( card_disk, lba, 1, buffer ) == 1;
}

/* identify */

static void identify_string( int word_offset, int word_length, const char *s )
{
	int i, p = 0, len = strlen( s );

	for( i = 0; i < word_length; i++ )
	{
		identify_buffer[ word_offset + i ] = ( p < len ? s[ p++ ] : ' ' ) << 8;
		identify_buffer[ word_offset + i ] |= p < len ? s[ p++ ] : ' ';
	}
}

/* cf_device_base::ide_build_identify_device */
static void build_identify( void )
{
	UINT32 total_sectors = num_cylinders * num_heads * num_sectors;

	memset( identify_buffer, 0, sizeof( identify_buffer ) );
	identify_buffer[ 0 ] = 0x848a;			/* configuration bits */
	identify_buffer[ 1 ] = num_cylinders;
	identify_buffer[ 3 ] = num_heads;
	identify_buffer[ 6 ] = num_sectors;
	identify_buffer[ 7 ] = total_sectors >> 16;
	identify_buffer[ 8 ] = total_sectors & 0xffff;
	identify_string( 10, 10, "00000000000000000000" );	/* serial number */
	identify_buffer[ 22 ] = 4;
	identify_string( 23, 4, "1.0" );		/* firmware revision */
	identify_string( 27, 20, "MAME    Virtual CompactFlash" );
	identify_buffer[ 47 ] = 0x0001;			/* read/write multiple */
	identify_buffer[ 49 ] = 0x0200;			/* capabilities */
	identify_buffer[ 51 ] = 0x0200;			/* PIO timing */
	identify_buffer[ 53 ] = 0x0003;
	identify_buffer[ 54 ] = num_cylinders;
	identify_buffer[ 55 ] = num_heads;
	identify_buffer[ 56 ] = num_sectors;
	identify_buffer[ 57 ] = total_sectors & 0xffff;
	identify_buffer[ 58 ] = total_sectors >> 16;
	identify_buffer[ 60 ] = total_sectors & 0xffff;
	identify_buffer[ 61 ] = total_sectors >> 16;
}

/* the default CIS of MAME, for a CHD without one */
static const UINT8 default_cis[] =
{
	0x01, 0x04, 0xdf, 0x4a, 0x01, 0xff,				/* CISTPL_DEVICE */
	0x20, 0x04, 0x00, 0x00, 0x00, 0x00,				/* CISTPL_MANFID */
	0x21, 0x02, 0x04, 0x01,							/* CISTPL_FUNCID */
	0x1a, 0x05, 0x01, 0x03, 0x00, 0x02, 0x0f,		/* CISTPL_CONFIG */
	0x15, 0x14, 0x05, 0x01,							/* CISTPL_VERS_1 */
	'M', 'A', 'M', 'E', 0, 'A', 'T', ' ', 'F', 'L', 'A', 'S', 'H', 0, '1', '.', '0', 0,
	0xff, 0xff										/* CISTPL_END */
};

/* features */

static int set_dma_mode( int word )
{
	if( ( identify_buffer[ word ] >> ( reg_sector_count & 7 ) ) & 1 )
	{
		identify_buffer[ 62 ] &= 0xff;
		identify_buffer[ 63 ] &= 0xff;
		identify_buffer[ 88 ] &= 0xff;
		identify_buffer[ word ] |= 0x100 << ( reg_sector_count & 7 );
		return 1;
	}
	return 0;
}

static int set_features( void )
{
	switch( reg_feature )
	{
	case 0x01:	/* enable 8-bit data transfers */
		transfers_8bit = 1;
		return 1;
	case 0x03:	/* transfer mode */
		switch( reg_sector_count & 0xf8 )
		{
		case 0x00:	/* PIO default */
			return ( reg_sector_count & 7 ) <= 1;
		case 0x08:	/* PIO flow control */
			if( ( reg_sector_count & 7 ) <= 2 )
				return 1;
			if( ( identify_buffer[ 64 ] >> ( ( reg_sector_count & 7 ) - 3 ) ) & 1 )
			{
				identify_buffer[ 62 ] &= 0xff;
				identify_buffer[ 63 ] &= 0xff;
				identify_buffer[ 88 ] &= 0xff;
				return 1;
			}
			return 0;
		case 0x10:
			return set_dma_mode( 62 );
		case 0x20:
			return set_dma_mode( 63 );
		case 0x40:
			return set_dma_mode( 88 );
		}
		return 0;
	case 0x66:	/* disable reverting to power on defaults */
	case 0xcc:	/* enable reverting */
		return 1;
	case 0x81:	/* disable 8-bit data transfers */
		transfers_8bit = 0;
		return 1;
	case 0x88:	/* ECC */
	case 0x99:	/* retries */
	case 0xaa:	/* read look ahead */
		return 1;
	}
	return 0;
}

/* commands */

static void command_error( void )
{
	reg_status |= STATUS_ERR;
	reg_error = ERROR_ABRT;
}

static void read_first_sector( void )
{
	if( reg_command == CMD_READ_BUFFER )
		start_busy( TIME_BETWEEN_SECTORS, PARAM_COMMAND );
	else
		start_busy( 0, PARAM_COMMAND );	/* Compact Flash: no seek time */
}

static void read_buffer_empty( void )
{
	buffer_offset = 0;
	reg_status &= ~STATUS_DRQ;
	fill_buffer();
}

static void finished_read( void )
{
	int ok;

	if( reg_command == CMD_READ_BUFFER )
		ok = 1;
	else
		ok = read_sector( lba_address() );

	if( ok )
	{
		/* stop on the last sector */
		if( sectors_remaining != 1 )
			next_sector();

		sectors_until_int--;
		if( sectors_until_int == 0 || ( sectors_remaining == 1 && reg_command != CMD_READ_MULTIPLE ) )
			sectors_until_int = reg_command == CMD_READ_MULTIPLE ? block_count : 1;

		if( reg_command == CMD_VERIFY_SECTORS || reg_command == CMD_VERIFY_SECTORS_NORETRY )
			read_buffer_empty();
		else
			reg_status |= STATUS_DRQ;
	}
	else
	{
		reg_status |= STATUS_ERR;
		reg_error = ERROR_BAD_SECTOR;
	}
}

static void fill_buffer( void )
{
	switch( reg_command )
	{
	case CMD_IDENTIFY_DEVICE:
	case CMD_READ_BUFFER:
		break;

	case CMD_READ_MULTIPLE:
		if( sectors_remaining > 0 )
		{
			sectors_remaining--;
			reg_sector_count = sectors_remaining;
		}
		if( sectors_remaining > 0 )
			finished_read();
		break;

	default:
		if( sectors_remaining > 0 )
		{
			sectors_remaining--;
			reg_sector_count = sectors_remaining;
		}
		if( sectors_remaining > 0 )
			start_busy( TIME_BETWEEN_SECTORS, PARAM_COMMAND );
		break;
	}
}

static void finished_write( void )
{
	int count;

	if( reg_command == CMD_WRITE_BUFFER )
		count = 1;
	else
		count = write_sector( lba_address() );

	if( count == 1 )
	{
		if( sectors_remaining != 1 )
			next_sector();

		sectors_until_int--;
		if( sectors_until_int == 0 || sectors_remaining == 1 )
			sectors_until_int = reg_command == CMD_WRITE_MULTIPLE ? block_count : 1;

		if( sectors_remaining > 0 )
		{
			sectors_remaining--;
			reg_sector_count = sectors_remaining;
		}
		if( sectors_remaining > 0 )
			reg_status |= STATUS_DRQ;
	}
	else
	{
		reg_status |= STATUS_ERR;
		reg_error = ERROR_BAD_SECTOR;
	}
}

static void process_buffer( void )
{
	if( card_type == ATAFLASH_TAITO_PCCARD2 && reg_command == CMD_TAITO_GNET_UNLOCK_2 )
	{
		int i, bad = 0;

		for( i = 0; !bad && i < SECTOR_SIZE; i++ )
			bad = ( ( i < 2 || i >= 7 ) && buffer[ i ] ) || ( ( i >= 2 && i < 7 ) && buffer[ i ] != key[ i - 2 ] );
		if( bad )
		{
			reg_status |= STATUS_ERR;
			reg_error = ERROR_NONE;
		}
		else
			locked = 0;
		return;
	}

	switch( reg_command )
	{
	case CMD_SECURITY_UNLOCK:		/* no passwords on the card */
	case CMD_SECURITY_DISABLE:
	case CMD_WRITE_BUFFER:
		break;

	case CMD_WRITE_MULTIPLE:
		if( sectors_until_int != 1 )
			finished_write();
		else
			start_busy( TIME_PER_SECTOR_WRITE, PARAM_COMMAND );
		break;

	default:
		start_busy( TIME_PER_SECTOR_WRITE, PARAM_COMMAND );
		break;
	}
}

static void write_buffer_full( void )
{
	buffer_offset = 0;
	reg_status &= ~STATUS_DRQ;
	process_buffer();
}

static void finished_command( void )
{
	int i;

	switch( reg_command )
	{
	case CMD_IDENTIFY_DEVICE:
		if( can_identify )
		{
			for( i = 0; i < 256; i++ )
			{
				buffer[ i * 2 ] = identify_buffer[ i ] & 0xff;
				buffer[ i * 2 + 1 ] = identify_buffer[ i ] >> 8;
			}
			reg_status |= STATUS_DRQ;
		}
		else
		{
			reg_status |= STATUS_ERR;
			reg_error = ERROR_NONE;
		}
		break;

	case CMD_SET_CONFIG:
		num_sectors = reg_sector_count ? reg_sector_count : 0x100;
		num_heads = ( reg_device_head & HEAD_HS ) + 1;
		break;

	case CMD_READ_SECTORS:
	case CMD_READ_SECTORS_NORETRY:
	case CMD_READ_MULTIPLE:
	case CMD_VERIFY_SECTORS:
	case CMD_VERIFY_SECTORS_NORETRY:
	case CMD_READ_DMA:
	case CMD_READ_BUFFER:
		finished_read();
		break;

	case CMD_WRITE_SECTORS:
	case CMD_WRITE_SECTORS_NORETRY:
	case CMD_WRITE_MULTIPLE:
	case CMD_WRITE_DMA:
	case CMD_WRITE_BUFFER:
		finished_write();
		break;

	case CMD_READ_NATIVE_MAX:
		i = num_cylinders * num_heads * num_sectors;
		buffer[ 0 ] = i >> 24;
		buffer[ 1 ] = i >> 16;
		buffer[ 2 ] = i >> 8;
		buffer[ 3 ] = i;
		break;

	case CMD_DIAGNOSTIC:
		start_diagnostic();
		break;

	case CMD_SET_FEATURES:
		if( !set_features() )
			command_error();
		break;
	}
}

static void process_command( void )
{
	buffer_size = SECTOR_SIZE;

	/* the Taito cards: the unlock commands, and nothing else while locked */
	switch( card_type )
	{
	case ATAFLASH_TAITO_PCCARD2:
		if( reg_command == CMD_TAITO_GNET_UNLOCK_1 )
		{
			reg_sector_count = 1;
			reg_status |= STATUS_DRDY;
			return;
		}
		if( reg_command == CMD_TAITO_GNET_UNLOCK_2 )
		{
			reg_status |= STATUS_DRQ;
			return;
		}
		break;

	case ATAFLASH_TAITO_CF:
		if( reg_command == CMD_TAITO_CF_UNLOCK )
		{
			if( reg_feature != key[ 0 ] || reg_sector_count != key[ 1 ] || reg_sector_number != key[ 2 ] ||
				reg_cylinder_low != key[ 3 ] || reg_cylinder_high != key[ 4 ] )
				reg_status &= ~STATUS_DRDY;
			else
				locked = 0;
			return;
		}
		break;
	}
	if( card_type != ATAFLASH_GENERIC && locked != 0 )
	{
		reg_status |= STATUS_ERR;
		reg_error = ERROR_NONE;
		reg_status &= ~STATUS_DRDY;
		return;
	}

	sectors_until_int = 0;
	switch( reg_command )
	{
	case CMD_READ_SECTORS:
	case CMD_READ_SECTORS_NORETRY:
	case CMD_READ_MULTIPLE:
		sectors_remaining = count_remaining();
		sectors_until_int = 1;
		read_first_sector();
		break;

	case CMD_READ_BUFFER:
		sectors_remaining = count_remaining();
		sectors_until_int = 1;
		buffer_offset = 0;
		read_first_sector();
		break;

	case CMD_VERIFY_SECTORS:
	case CMD_VERIFY_SECTORS_NORETRY:
	case CMD_READ_DMA:
		sectors_remaining = count_remaining();
		sectors_until_int = sectors_remaining;
		read_first_sector();
		break;

	case CMD_WRITE_SECTORS:
	case CMD_WRITE_SECTORS_NORETRY:
		sectors_remaining = count_remaining();
		sectors_until_int = 1;
		reg_status |= STATUS_DRQ;
		break;

	case CMD_WRITE_BUFFER:
		sectors_remaining = count_remaining();
		sectors_until_int = 1;
		buffer_offset = 0;
		reg_status |= STATUS_DRQ;
		break;

	case CMD_WRITE_MULTIPLE:
		sectors_remaining = count_remaining();
		sectors_until_int = block_count;
		reg_status |= STATUS_DRQ;
		break;

	case CMD_WRITE_DMA:
		sectors_remaining = count_remaining();
		sectors_until_int = sectors_remaining;
		reg_status |= STATUS_DRQ;
		break;

	case CMD_SECURITY_UNLOCK:
	case CMD_SECURITY_DISABLE:
		reg_status |= STATUS_DRQ;
		break;

	case CMD_IDENTIFY_DEVICE:
	case CMD_RECALIBRATE:
	case CMD_SET_CONFIG:
	case CMD_READ_NATIVE_MAX:
	case CMD_SET_FEATURES:
	case CMD_CACHE_FLUSH:
		start_busy( MINIMUM_COMMAND_TIME, PARAM_COMMAND );
		break;

	case CMD_DIAGNOSTIC:
		start_busy( DIAGNOSTIC_TIME, PARAM_COMMAND );
		break;

	case CMD_IDLE:
	case CMD_SET_MAX:
	case CMD_SEEK:
	case CMD_IDLE_IMMEDIATE:
		break;

	case CMD_SET_BLOCK_COUNT:
		block_count = reg_sector_count ? reg_sector_count : 0x100;
		break;

	default:	/* the LBA48 commands too: the card has no LBA48 */
		logerror( "ataflash: unknown command %02x\n", reg_command );
		command_error();
		break;
	}
}

/* registers */

static UINT16 command_r( int offset, UINT16 mem_mask )
{
	UINT16 data = 0xffff;

	/* device 0 answers for a missing device 1 */
	if( !device_selected() && !single_device )
		return data;
	if( ( reg_status & STATUS_BSY ) && offset != CS0_DATA )
		return ( data & 0xff00 ) | ( device_selected() ? calculate_status() : 0 );

	switch( offset )
	{
	case CS0_DATA:
		if( device_selected() && !( reg_status & STATUS_BSY ) && ( reg_status & STATUS_DRQ ) )
		{
			int size = ( ( mem_mask & 0xff00 ) && !transfers_8bit ) ? 2 : 1;

			data = ( data & 0xff00 ) | buffer[ buffer_offset & ( SECTOR_SIZE - 1 ) ];
			if( size == 2 )
				data = ( data & 0x00ff ) | ( buffer[ ( buffer_offset + 1 ) & ( SECTOR_SIZE - 1 ) ] << 8 );
			buffer_offset += size;
			if( buffer_offset >= buffer_size )
				read_buffer_empty();
		}
		break;
	case CS0_ERROR_FEATURE:	data = ( data & 0xff00 ) | reg_error; break;
	case CS0_SECTOR_COUNT:	data = ( data & 0xff00 ) | reg_sector_count; break;
	case CS0_SECTOR_NUMBER:	data = ( data & 0xff00 ) | reg_sector_number; break;
	case CS0_CYLINDER_LOW:	data = ( data & 0xff00 ) | reg_cylinder_low; break;
	case CS0_CYLINDER_HIGH:	data = ( data & 0xff00 ) | reg_cylinder_high; break;
	case CS0_DEVICE_HEAD:	data = ( data & 0xff00 ) | reg_device_head; break;
	case CS0_STATUS_COMMAND:
		data = ( data & 0xff00 ) | ( device_selected() ? calculate_status() : 0 );
		break;
	}
	return data;
}

static UINT16 control_r( int offset )
{
	UINT16 data = 0xffff;

	if( !device_selected() && !single_device )
		return data;

	switch( offset )
	{
	case CS1_ALTSTATUS_CONTROL:
		data = ( data & 0xff00 ) | ( device_selected() ? calculate_status() : 0 );
		break;
	case CS1_ACTIVE_STATUS:
		data = ( data & 0xff00 ) | ( device_selected() ? 1 : 0 );
		break;
	}
	return data;
}

static void command_w( int offset, UINT16 data, UINT16 mem_mask )
{
	/* a register write during a data transfer aborts it */
	if( ( reg_status & STATUS_DRQ ) && offset != CS0_DATA && ( offset != CS0_DEVICE_HEAD || ( data & 0xff ) != reg_device_head ) )
	{
		stop_busy();
		reg_status &= ~STATUS_DRQ;
		command_error();
	}

	if( reg_status & STATUS_BSY )
		return;

	switch( offset )
	{
	case CS0_DATA:
		if( device_selected() && ( reg_status & STATUS_DRQ ) )
		{
			buffer[ buffer_offset++ & ( SECTOR_SIZE - 1 ) ] = data & 0xff;
			if( ( mem_mask & 0xff00 ) && !transfers_8bit )
				buffer[ buffer_offset++ & ( SECTOR_SIZE - 1 ) ] = data >> 8;
			if( buffer_offset >= buffer_size )
				write_buffer_full();
		}
		break;
	case CS0_ERROR_FEATURE:	reg_feature = data; break;
	case CS0_SECTOR_COUNT:	reg_sector_count = data; break;
	case CS0_SECTOR_NUMBER:	reg_sector_number = data; break;
	case CS0_CYLINDER_LOW:	reg_cylinder_low = data; break;
	case CS0_CYLINDER_HIGH:	reg_cylinder_high = data; break;
	case CS0_DEVICE_HEAD:	reg_device_head = data; break;
	case CS0_STATUS_COMMAND:
		if( device_selected() || reg_command == CMD_DIAGNOSTIC )
		{
			reg_command = data;
			buffer_offset = 0;
			reg_status &= ~STATUS_ERR;
			reg_error = ERROR_NONE;
			process_command();
		}
		break;
	}
}

static void control_w( int offset, UINT16 data )
{
	if( offset == CS1_ALTSTATUS_CONTROL )
	{
		UINT8 old = reg_device_control;

		reg_device_control = data;
		if( ( reg_device_control ^ old ) & CONTROL_SRST )
		{
			if( reg_device_control & CONTROL_SRST )
			{
				if( !resetting )
					start_busy( TIME_NEVER, PARAM_RESET );
			}
			else if( busy_param == PARAM_RESET )
				soft_reset();
		}
	}
}

/* card memory (the PC card I/O space): byte offsets */
static UINT16 card_io_r( offs_t offset, UINT16 mem_mask )
{
	if( offset < 8 )
		return command_r( offset, mem_mask );
	if( offset < 16 )
		return control_r( offset & 7 );
	return 0xffff;
}

static void card_io_w( offs_t offset, UINT16 data, UINT16 mem_mask )
{
	if( offset < 8 )
		command_w( offset, data, mem_mask );
	else if( offset < 16 )
		control_w( offset & 7, data );
}

/* card attribute memory: word offsets */
static UINT16 card_reg_r( offs_t offset )
{
	switch( offset )
	{
	case 0x100:
		return config_option;
	case 0x101:
		return config_status;
	case 0x102:
		return pin_replacement;
	case 0x201:
		if( card_type == ATAFLASH_TAITO_PCCARD1 )
			return locked != 0;
		break;
	default:
		if( offset < 0x100 )
			return cis[ offset ];
		break;
	}
	return 0xffff;
}

static void card_reg_w( offs_t offset, UINT16 data )
{
	if( card_type == ATAFLASH_TAITO_PCCARD1 && offset >= 0x280 && offset <= 0x288 )
	{
		int pos = offset - 0x280;
		UINT8 k = pos < 5 ? key[ pos ] : 0;

		if( ( data & 0xff ) == k )
			locked &= ~( 1 << pos );
		else
			locked |= 1 << pos;
		return;
	}

	switch( offset )
	{
	case 0x100:
		config_option = data;
		break;
	case 0x101:
		config_status = data;
		break;
	}
}

/* the card reset: device_reset and the ATA interface_post_reset of MAME */
static void card_reset( void )
{
	if( card_disk != NULL && !can_identify )
	{
		struct hard_disk_info *info = hard_disk_get_info( card_disk );
		UINT8 ident[ 512 ];
		int i;

		if( info->sectorbytes == SECTOR_SIZE )
		{
			num_cylinders = info->cylinders;
			num_heads = info->heads;
			num_sectors = info->sectors;
		}
		if( hard_disk_get_metadata( card_disk, METADATA_TAG( 'I', 'D', 'N', 'T' ), ident, sizeof( ident ) ) == 512 )
		{
			for( i = 0; i < 256; i++ )
				identify_buffer[ i ] = ident[ i * 2 ] | ( ident[ i * 2 + 1 ] << 8 );
		}
		else
			build_identify();
		can_identify = 1;
	}

	memset( cis, 0xff, sizeof( cis ) );
	if( card_disk == NULL || hard_disk_get_metadata( card_disk, METADATA_TAG( 'C', 'I', 'S', ' ' ), cis, sizeof( cis ) ) < 0 )
	{
		memset( cis, 0xff, sizeof( cis ) );
		memcpy( cis, default_cis, sizeof( default_cis ) );
	}
	config_option = 0;
	config_status = 0;
	pin_replacement = 0x2e;

	if( card_type != ATAFLASH_GENERIC && has_key )
		locked = card_type == ATAFLASH_TAITO_PCCARD1 ? 0x1ff : 1;

	/* the ATA interface */
	reg_status = 0;
	reg_device_control = 0;
	resetting = 1;
	start_busy( DETECT_DEVICE1_TIME, PARAM_DETECT_DEVICE1 );
}

/* the RF5C296 */

static void rf5c296_reg_w( UINT8 reg, UINT8 data )
{
	/* register 3 bit 6 low: the card reset */
	if( reg == 0x03 && !( data & 0x40 ) )
		card_reset();
}

UINT16 rf5c296_io_r( offs_t offset, UINT16 mem_mask )
{
	int shift = 0;
	UINT16 data;

	offset *= 2;
	if( mem_mask == 0xff00 )
	{
		shift = 8;
		mem_mask >>= 8;
		offset++;
	}

	switch( offset )
	{
	case 0x3e0:
		data = rf5c296_reg;
		break;
	case 0x3e1:
		data = 0x00;
		break;
	default:
		data = card_io_r( offset, mem_mask );
		break;
	}
	return data << shift;
}

void rf5c296_io_w( offs_t offset, UINT16 data, UINT16 mem_mask )
{
	offset *= 2;
	if( mem_mask == 0xff00 )
	{
		mem_mask >>= 8;
		data >>= 8;
		offset++;
	}

	switch( offset )
	{
	case 0x3e0:
		rf5c296_reg = data;
		break;
	case 0x3e1:
		rf5c296_reg_w( rf5c296_reg, data );
		break;
	default:
		card_io_w( offset, data, mem_mask );
		break;
	}
}

UINT16 rf5c296_mem_r( offs_t offset, UINT16 mem_mask )
{
	return card_reg_r( offset );
}

void rf5c296_mem_w( offs_t offset, UINT16 data, UINT16 mem_mask )
{
	card_reg_w( offset, data );
}

/* init */

/* 0.78 does not save its timers: the busy time goes on with what it had left */
static void ataflash_presave( void )
{
	busy_left = -1;
	if( ( reg_status & STATUS_BSY ) && busy_param != PARAM_RESET )
	{
		/* a timer due in this timeslice can be a little past its time */
		busy_left = timer_timeleft( busy_timer );
		if( busy_left < 0 )
			busy_left = 0;
	}
}

static void ataflash_postload( void )
{
	if( busy_left >= 0 )
		timer_adjust( busy_timer, busy_left, busy_param, 0 );
	else
		timer_adjust( busy_timer, TIME_NEVER, 0, 0 );
}

void ataflash_init( int type, struct hard_disk_file *disk )
{
	card_type = type;
	card_disk = disk;
	num_cylinders = num_heads = num_sectors = 0;
	can_identify = 0;
	memset( identify_buffer, 0, sizeof( identify_buffer ) );
	memset( key, 0, sizeof( key ) );
	has_key = 0;
	locked = 0;
	if( disk != NULL && type != ATAFLASH_GENERIC &&
		hard_disk_get_metadata( disk, METADATA_TAG( 'K', 'E', 'Y', ' ' ), key, sizeof( key ) ) == 5 )
		has_key = 1;

	busy_timer = timer_alloc( busy_tick );
	busy_param = PARAM_COMMAND;
	busy_left = -1;
	idx_time = 0;

	state_save_register_UINT8( "ataflash", 0, "buffer", buffer, SECTOR_SIZE );
	state_save_register_UINT16( "ataflash", 0, "buffer_offset", &buffer_offset, 1 );
	state_save_register_UINT16( "ataflash", 0, "buffer_size", &buffer_size, 1 );
	state_save_register_UINT8( "ataflash", 0, "error", &reg_error, 1 );
	state_save_register_UINT8( "ataflash", 0, "feature", &reg_feature, 1 );
	state_save_register_UINT8( "ataflash", 0, "sector_count", &reg_sector_count, 1 );
	state_save_register_UINT8( "ataflash", 0, "sector_number", &reg_sector_number, 1 );
	state_save_register_UINT8( "ataflash", 0, "cylinder_low", &reg_cylinder_low, 1 );
	state_save_register_UINT8( "ataflash", 0, "cylinder_high", &reg_cylinder_high, 1 );
	state_save_register_UINT8( "ataflash", 0, "device_head", &reg_device_head, 1 );
	state_save_register_UINT8( "ataflash", 0, "status", &reg_status, 1 );
	state_save_register_UINT8( "ataflash", 0, "command", &reg_command, 1 );
	state_save_register_UINT8( "ataflash", 0, "device_control", &reg_device_control, 1 );
	state_save_register_UINT8( "ataflash", 0, "transfers_8bit", &transfers_8bit, 1 );
	state_save_register_UINT8( "ataflash", 0, "single_device", &single_device, 1 );
	state_save_register_UINT8( "ataflash", 0, "resetting", &resetting, 1 );
	state_save_register_UINT16( "ataflash", 0, "block_count", &block_count, 1 );
	state_save_register_UINT16( "ataflash", 0, "sectors_until_int", &sectors_until_int, 1 );
	state_save_register_UINT32( "ataflash", 0, "sectors_remaining", &sectors_remaining, 1 );
	state_save_register_UINT32( "ataflash", 0, "cur_lba", &cur_lba, 1 );
	state_save_register_UINT32( "ataflash", 0, "num_heads", &num_heads, 1 );
	state_save_register_UINT32( "ataflash", 0, "num_sectors", &num_sectors, 1 );
	state_save_register_UINT16( "ataflash", 0, "identify", identify_buffer, 256 );
	state_save_register_UINT16( "ataflash", 0, "locked", &locked, 1 );
	state_save_register_UINT8( "ataflash", 0, "config_option", &config_option, 1 );
	state_save_register_UINT8( "ataflash", 0, "config_status", &config_status, 1 );
	state_save_register_UINT8( "ataflash", 0, "rf5c296_reg", &rf5c296_reg, 1 );
	state_save_register_INT32( "ataflash", 0, "busy_param", &busy_param, 1 );
	state_save_register_double( "ataflash", 0, "busy_left", &busy_left, 1 );
	state_save_register_double( "ataflash", 0, "idx_time", &idx_time, 1 );
	state_save_register_func_presave( ataflash_presave );
	state_save_register_func_postload( ataflash_postload );
}

void ataflash_reset( void )
{
	reg_error = reg_feature = reg_sector_count = reg_sector_number = 0;
	reg_cylinder_low = reg_cylinder_high = reg_device_head = 0;
	reg_command = 0;
	transfers_8bit = 0;
	single_device = 0;
	block_count = 0;
	sectors_until_int = 0;
	sectors_remaining = 0;
	rf5c296_reg = 0;
	idx_time = timer_get_time();
	card_reset();
}
