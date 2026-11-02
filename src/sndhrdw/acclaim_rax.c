/***************************************************************************

  Acclaim RAX sound board, as in current MAME (shared/rax.cpp)

  An ADSP-2181 at 16.67MHz with its ROM (banks of 4MB) read by the byte DMA
  (BDMA), 4 banks of 8K words of external data RAM behind the data memory
  overlay, and two DACs fed by SPORT0 in autobuffer mode. The host writes
  16-bit commands, which the ADSP reads at I/O 3 (that clears its IRQL0).

  Used by NBA Jam Extreme (ZN-1) and Batman Forever (ST-V).

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "cpu/adsp2100/adsp2100.h"
#include "sndhrdw/acclaim_rax.h"

/* the ADSP-2181 control registers, data memory 0x3fe0-0x3fff */
enum
{
	RAX_IDMA_CONTROL_REG = 0,	/* 3fe0 */
	RAX_BDMA_INT_ADDR_REG,		/* 3fe1 */
	RAX_BDMA_EXT_ADDR_REG,		/* 3fe2 */
	RAX_BDMA_CONTROL_REG,		/* 3fe3 */
	RAX_BDMA_WORD_COUNT_REG,	/* 3fe4 */
	RAX_PROG_FLAG_DATA_REG,		/* 3fe5 */
	RAX_PROG_FLAG_CONTROL_REG,	/* 3fe6 */

	RAX_S1_AUTOBUF_REG = 15,	/* 3fef */
	RAX_S1_RFSDIV_REG,			/* 3ff0 */
	RAX_S1_SCLKDIV_REG,			/* 3ff1 */
	RAX_S1_CONTROL_REG,			/* 3ff2 */
	RAX_S0_AUTOBUF_REG,			/* 3ff3 */
	RAX_S0_RFSDIV_REG,			/* 3ff4 */
	RAX_S0_SCLKDIV_REG,			/* 3ff5 */
	RAX_S0_CONTROL_REG,			/* 3ff6 */
	RAX_S0_MCTXLO_REG,			/* 3ff7 */
	RAX_S0_MCTXHI_REG,			/* 3ff8 */
	RAX_S0_MCRXLO_REG,			/* 3ff9 */
	RAX_S0_MCRXHI_REG,			/* 3ffa */
	RAX_TIMER_SCALE_REG,		/* 3ffb */
	RAX_TIMER_COUNT_REG,		/* 3ffc */
	RAX_TIMER_PERIOD_REG,		/* 3ffd */
	RAX_WAITSTATES_REG,			/* 3ffe */
	RAX_SYSCONTROL_REG			/* 3fff */
};

#define RAX_BUFFER_SIZE		0x4000		/* DAC samples per channel */
#define RAX_BUFFER_MASK		( RAX_BUFFER_SIZE - 1 )

static int rax_cpunum;
static UINT8 *rax_rom;
static UINT32 rax_rom_banks;			/* the banks of 4MB in the ROM region */
static UINT16 *rax_banked_ram;			/* 5 x 8K words: internal, then external 0-3 */
static UINT16 rax_control_regs[ 32 ];
static UINT16 rax_data_in;
static UINT16 rax_data_out;
static UINT8 rax_pf0;
static UINT16 rax_data_bank;
static UINT16 rax_rom_bank;
static UINT32 rax_dmovlay;
static UINT16 rax_size;
static UINT16 rax_incs;
static INT32 rax_ireg;
static UINT16 rax_ireg_base;
static UINT8 rax_dac_enabled[ 2 ];

/* the timers are not saved by the core: their time left is */
static void *rax_reg_timer;
static void *rax_dma_timer;
static double rax_reg_timer_period;
static double rax_reg_timer_left;
static double rax_dma_timer_left;
static UINT32 rax_dma_param;

/* the DACs */
static int rax_stream;
static INT16 *rax_buffer[ 2 ];
static UINT32 rax_buffer_in[ 2 ];
static UINT32 rax_buffer_out[ 2 ];
static UINT32 rax_sample_rate;
static UINT32 rax_sample_step;
static UINT32 rax_sample_position;

static void rax_update_data_ram_bank( void )
{
	int n_entry = ( rax_dmovlay == 0 ) ? 0 : 1 + rax_data_bank;
	cpu_setbank( 14, rax_banked_ram + n_entry * 0x2000 );
}

/* data memory as the ADSP sees it, outside its context (DMA, DACs) */
static UINT16 *rax_data_ptr( UINT32 n_addr )
{
	n_addr &= 0x3fff;
	if( n_addr < 0x2000 )
		return rax_banked_ram + ( ( rax_dmovlay == 0 ) ? 0 : 1 + rax_data_bank ) * 0x2000 + n_addr;
	if( n_addr < 0x3fe0 )
		return (UINT16 *)( memory_region( REGION_CPU1 + rax_cpunum ) + ADSP2181_DATA_OFFSET + ( n_addr << 1 ) );
	return NULL;
}

static void rax_dma_done( int param )
{
	/* update the external address and the page */
	rax_control_regs[ RAX_BDMA_WORD_COUNT_REG ] = 0;
	rax_control_regs[ RAX_BDMA_EXT_ADDR_REG ] = param & 0x3fff;
	rax_control_regs[ RAX_BDMA_CONTROL_REG ] &= ~0xff00;
	rax_control_regs[ RAX_BDMA_CONTROL_REG ] |= ( ( param >> 14 ) & 0xff ) << 8;

	if( rax_control_regs[ RAX_BDMA_CONTROL_REG ] & 8 )
		cpu_set_reset_line( rax_cpunum, PULSE_LINE );
	else
		cpu_set_irq_line( rax_cpunum, ADSP2181_BDMA, PULSE_LINE );

	timer_adjust( rax_dma_timer, TIME_NEVER, 0, 0 );
}

/* the byte DMA: from the ROM to program or data memory, at once; the end
   (its interrupt, or the reset of a boot load) after a cycle per word */
static void rax_bdma( UINT32 n_count_reg )
{
	const UINT8 *adsp_rom = rax_rom + ( rax_rom_bank % rax_rom_banks ) * 0x400000;
	UINT32 page = ( rax_control_regs[ RAX_BDMA_CONTROL_REG ] >> 8 ) & 0xff;
	UINT32 dir = ( rax_control_regs[ RAX_BDMA_CONTROL_REG ] >> 2 ) & 1;
	UINT32 type = rax_control_regs[ RAX_BDMA_CONTROL_REG ] & 3;
	UINT32 src_addr = ( page << 14 ) | rax_control_regs[ RAX_BDMA_EXT_ADDR_REG ];
	UINT32 count = rax_control_regs[ RAX_BDMA_WORD_COUNT_REG ];

	if( !dir )
	{
		while( count )
		{
			UINT32 n_addr = rax_control_regs[ RAX_BDMA_INT_ADDR_REG ];
			if( type == 0 )
			{
				/* 24-bit words to program memory */
				UINT32 n_word = ( adsp_rom[ ( src_addr + 0 ) & 0x3fffff ] << 16 ) |
					( adsp_rom[ ( src_addr + 1 ) & 0x3fffff ] << 8 ) | adsp_rom[ ( src_addr + 2 ) & 0x3fffff ];
				UINT32 *pgm = (UINT32 *)( memory_region( REGION_CPU1 + rax_cpunum ) + ADSP2181_PGM_OFFSET );
				pgm[ n_addr & 0x3fff ] = n_word;
				src_addr += 3;
			}
			else
			{
				/* 16-bit words, or bytes (MSB or LSB aligned) to data memory */
				UINT16 n_word;
				UINT16 *p;
				if( type == 1 )
				{
					n_word = ( adsp_rom[ src_addr & 0x3fffff ] << 8 ) | adsp_rom[ ( src_addr + 1 ) & 0x3fffff ];
					src_addr += 2;
				}
				else
				{
					n_word = adsp_rom[ src_addr & 0x3fffff ] << ( ( type == 2 ) ? 8 : 0 );
					src_addr++;
				}
				p = rax_data_ptr( n_addr );
				if( p )
					*p = n_word;
			}
			rax_control_regs[ RAX_BDMA_INT_ADDR_REG ] = ( n_addr + 1 ) & 0x3fff;
			count--;
		}
	}

	rax_dma_param = src_addr;
	timer_adjust( rax_dma_timer, TIME_IN_HZ( RAX_CLOCK ) * ( n_count_reg & 0x3fff ), rax_dma_param, 0 );
}

static READ16_HANDLER( rax_control_r )
{
	if( offset == RAX_PROG_FLAG_DATA_REG )
		return rax_pf0;
	return rax_control_regs[ offset ];
}

static WRITE16_HANDLER( rax_control_w )
{
	rax_control_regs[ offset ] = data;

	switch( offset )
	{
		case RAX_BDMA_INT_ADDR_REG:
			rax_control_regs[ offset ] = data & 0x3fff;
			break;
		case RAX_BDMA_EXT_ADDR_REG:
			rax_control_regs[ offset ] = data & 0x3fff;
			break;
		case RAX_BDMA_CONTROL_REG:
			rax_control_regs[ offset ] = data & 0xff0f;
			break;
		case RAX_BDMA_WORD_COUNT_REG:
			rax_control_regs[ offset ] = data & 0x3fff;
			rax_bdma( data );
			break;

		case RAX_S1_AUTOBUF_REG:
			/* autobuffer off: the right DAC stops */
			if( !( data & 2 ) )
			{
				stream_update( rax_stream, 0 );
				rax_dac_enabled[ 1 ] = 0;
				rax_buffer_in[ 1 ] = rax_buffer_out[ 1 ] = 0;
			}
			break;

		case RAX_S0_AUTOBUF_REG:
			/* autobuffer off: the left DAC and the transfers stop */
			if( !( data & 2 ) )
			{
				stream_update( rax_stream, 0 );
				rax_dac_enabled[ 0 ] = 0;
				rax_buffer_in[ 0 ] = rax_buffer_out[ 0 ] = 0;
				timer_adjust( rax_reg_timer, TIME_NEVER, 0, 0 );
				rax_reg_timer_period = 0;
			}
			break;
	}
}

/* the I/O space: data RAM bank, ROM bank, and the latches */
static UINT16 rax_io_r( UINT32 offset )
{
	if( offset == 3 )
	{
		cpu_set_irq_line( rax_cpunum, ADSP2181_IRQL0, CLEAR_LINE );
		return rax_data_in;
	}
	return 0xffff;
}

static void rax_io_w( UINT32 offset, UINT16 data )
{
	switch( offset )
	{
		case 0:
			/* the board has two more RAM sockets, empty */
			rax_data_bank = data & 3;
			rax_update_data_ram_bank();
			break;
		case 1:
			rax_rom_bank = data;
			break;
		case 3:
			rax_data_out = data;
			rax_pf0 = 0;
			break;
	}
}

static void rax_dmovlay_w( UINT32 data )
{
	rax_dmovlay = ( data != 0 );
	rax_update_data_ram_bank();
}

/* a quarter of the autobuffer, to the DACs (left, right interleaved) */
static void rax_irq( int param )
{
	int reg = cpunum_get_reg( rax_cpunum, ADSP2100_I0 + rax_ireg );
	int count = rax_size / ( 4 * ( rax_incs ? rax_incs : 1 ) );
	int i;

	stream_update( rax_stream, 0 );

	for( i = 0; i < ( count & ~1 ); i++ )
	{
		UINT16 *p = rax_data_ptr( reg );
		int n_channel = i & 1;
		if( rax_dac_enabled[ n_channel ] )
		{
			rax_buffer[ n_channel ][ rax_buffer_in[ n_channel ] & RAX_BUFFER_MASK ] = p ? (INT16)*p : 0;
			if( rax_buffer_in[ n_channel ] - rax_buffer_out[ n_channel ] < RAX_BUFFER_SIZE - 1 )
				rax_buffer_in[ n_channel ]++;
		}
		reg += rax_incs;
	}

	/* check for wrapping */
	if( reg >= rax_ireg_base + rax_size )
		reg = rax_ireg_base;

	cpunum_set_reg( rax_cpunum, ADSP2100_I0 + rax_ireg, reg );
}

/* SPORT0 transmit: the autobuffer starts */
static void rax_tx_callback( int port, INT32 data )
{
	if( port != 0 )
		return;

	/* SPORT0 enabled */
	if( rax_control_regs[ RAX_SYSCONTROL_REG ] & 0x1000 )
	{
		/* autobuffer only, which is what this board uses */
		if( rax_control_regs[ RAX_S0_AUTOBUF_REG ] & 0x0002 )
		{
			int mreg, lreg;
			UINT16 source;

			stream_update( rax_stream, 0 );

			rax_ireg = ( rax_control_regs[ RAX_S0_AUTOBUF_REG ] >> 9 ) & 7;
			mreg = ( rax_control_regs[ RAX_S0_AUTOBUF_REG ] >> 7 ) & 3;
			mreg |= rax_ireg & 0x04;	/* msb comes from ireg */
			lreg = rax_ireg;

			source = cpunum_get_reg( rax_cpunum, ADSP2100_I0 + rax_ireg );
			rax_incs = cpunum_get_reg( rax_cpunum, ADSP2100_M0 + mreg );
			rax_size = cpunum_get_reg( rax_cpunum, ADSP2100_L0 + lreg );

			/* the base value, kept for the wrapping; back one, so that the
			   first sample is not lost */
			source -= rax_incs;
			cpunum_set_reg( rax_cpunum, ADSP2100_I0 + rax_ireg, source );
			rax_ireg_base = source;

			/* a sample every 16 serial clocks; the DACs at that rate */
			rax_sample_rate = RAX_CLOCK / ( 16 * ( rax_control_regs[ RAX_S0_SCLKDIV_REG ] + 1 ) );
			rax_sample_step = (UINT32)( (double)rax_sample_rate * 65536.0 / (double)Machine->sample_rate );
			rax_dac_enabled[ 0 ] = rax_dac_enabled[ 1 ] = 1;

			/* a timer every quarter of the buffer (two samples per pair) */
			if( rax_incs )
			{
				rax_reg_timer_period = TIME_IN_HZ( rax_sample_rate ) * rax_size / ( 4 * 2 * rax_incs );
				timer_adjust( rax_reg_timer, rax_reg_timer_period, 0, rax_reg_timer_period );
			}
			return;
		}
	}

	/* anything else: no sound */
	stream_update( rax_stream, 0 );
	rax_dac_enabled[ 0 ] = rax_dac_enabled[ 1 ] = 0;
	rax_buffer_in[ 0 ] = rax_buffer_out[ 0 ] = rax_buffer_in[ 1 ] = rax_buffer_out[ 1 ] = 0;
	timer_adjust( rax_reg_timer, TIME_NEVER, 0, 0 );
	rax_reg_timer_period = 0;
}

/* both DACs, resampled to the output rate; silence when one runs out */
static void rax_dac_update( int num, INT16 **buffer, int length )
{
	UINT32 n_end = rax_sample_position;
	int n_channel;

	for( n_channel = 0; n_channel < 2; n_channel++ )
	{
		INT16 *dest = buffer[ n_channel ];
		UINT32 current = rax_sample_position;
		UINT32 n_used;
		int i;

		for( i = 0; i < length; i++ )
		{
			UINT32 n_index = rax_buffer_out[ n_channel ] + ( current >> 16 );
			if( !rax_dac_enabled[ n_channel ] || n_index >= rax_buffer_in[ n_channel ] )
				break;
			dest[ i ] = rax_buffer[ n_channel ][ n_index & RAX_BUFFER_MASK ];
			current += rax_sample_step;
		}
		for( ; i < length; i++ )
			dest[ i ] = 0;

		/* drop the samples played, keep the fraction */
		n_used = current >> 16;
		if( n_used > rax_buffer_in[ n_channel ] - rax_buffer_out[ n_channel ] )
			n_used = rax_buffer_in[ n_channel ] - rax_buffer_out[ n_channel ];
		rax_buffer_out[ n_channel ] += n_used;
		if( n_channel == 0 )
			n_end = current & 0xffff;
	}
	rax_sample_position = n_end;
}

static int rax_custom_start( const struct MachineSound *msound )
{
	const char *names[] = { "RAX DAC L", "RAX DAC R" };
	int vols[] = { MIXER( 100, MIXER_PAN_LEFT ), MIXER( 100, MIXER_PAN_RIGHT ) };
	int i;

	rax_stream = stream_init_multi( 2, names, vols, Machine->sample_rate, 0, rax_dac_update );
	for( i = 0; i < 2; i++ )
	{
		rax_buffer[ i ] = auto_malloc( RAX_BUFFER_SIZE * sizeof( INT16 ) );
		if( !rax_buffer[ i ] )
			return 1;
		memset( rax_buffer[ i ], 0, RAX_BUFFER_SIZE * sizeof( INT16 ) );
	}
	state_save_register_INT16( "rax", 0, "buffer_l", rax_buffer[ 0 ], RAX_BUFFER_SIZE );
	state_save_register_INT16( "rax", 0, "buffer_r", rax_buffer[ 1 ], RAX_BUFFER_SIZE );
	return 0;
}

struct CustomSound_interface acclaim_rax_custom_interface =
{
	rax_custom_start, 0, 0
};

MEMORY_READ16_START( acclaim_rax_readmem )
	{ ADSP_DATA_ADDR_RANGE( 0x0000, 0x1fff ), MRA16_BANK14 },	/* internal RAM, or external through DMOVLAY */
	{ ADSP_DATA_ADDR_RANGE( 0x2000, 0x3fdf ), MRA16_RAM },		/* internal RAM */
	{ ADSP_DATA_ADDR_RANGE( 0x3fe0, 0x3fff ), rax_control_r },	/* control registers */
	{ ADSP_PGM_ADDR_RANGE( 0x0000, 0x3fff ), MRA16_RAM },		/* internal program RAM */
MEMORY_END

MEMORY_WRITE16_START( acclaim_rax_writemem )
	{ ADSP_DATA_ADDR_RANGE( 0x0000, 0x1fff ), MWA16_BANK14 },
	{ ADSP_DATA_ADDR_RANGE( 0x2000, 0x3fdf ), MWA16_RAM },
	{ ADSP_DATA_ADDR_RANGE( 0x3fe0, 0x3fff ), rax_control_w },
	{ ADSP_PGM_ADDR_RANGE( 0x0000, 0x3fff ), MWA16_RAM },
MEMORY_END

static void rax_presave( void )
{
	rax_reg_timer_left = timer_timeleft( rax_reg_timer );
	rax_dma_timer_left = timer_timeleft( rax_dma_timer );
}

static void rax_postload( void )
{
	rax_update_data_ram_bank();
	if( rax_reg_timer_period > 0 && rax_reg_timer_left < TIME_NEVER / 2 )
		timer_adjust( rax_reg_timer, ( rax_reg_timer_left > 0 ) ? rax_reg_timer_left : 0, 0, rax_reg_timer_period );
	else
		timer_adjust( rax_reg_timer, TIME_NEVER, 0, 0 );
	if( rax_dma_timer_left < TIME_NEVER / 2 )
		timer_adjust( rax_dma_timer, ( rax_dma_timer_left > 0 ) ? rax_dma_timer_left : 0, rax_dma_param, 0 );
	else
		timer_adjust( rax_dma_timer, TIME_NEVER, 0, 0 );
}

void acclaim_rax_init( int cpunum, int region )
{
	rax_cpunum = cpunum;
	rax_rom = memory_region( region );
	rax_rom_banks = memory_region_length( region ) / 0x400000;
	if( rax_rom_banks < 1 )
		rax_rom_banks = 1;
	rax_banked_ram = auto_malloc( 0x2000 * 5 * sizeof( UINT16 ) );
	memset( rax_banked_ram, 0, 0x2000 * 5 * sizeof( UINT16 ) );
	rax_reg_timer = timer_alloc( rax_irq );
	rax_dma_timer = timer_alloc( rax_dma_done );

	adsp2181_set_tx_callback( rax_tx_callback );
	adsp2181_set_io_callbacks( rax_io_r, rax_io_w );
	adsp2181_set_dmovlay_callback( rax_dmovlay_w );

	state_save_register_UINT16( "rax", 0, "banked_ram", rax_banked_ram, 0x2000 * 5 );
	state_save_register_UINT16( "rax", 0, "control_regs", rax_control_regs, 32 );
	state_save_register_UINT16( "rax", 0, "data_in", &rax_data_in, 1 );
	state_save_register_UINT16( "rax", 0, "data_out", &rax_data_out, 1 );
	state_save_register_UINT8( "rax", 0, "pf0", &rax_pf0, 1 );
	state_save_register_UINT16( "rax", 0, "data_bank", &rax_data_bank, 1 );
	state_save_register_UINT16( "rax", 0, "rom_bank", &rax_rom_bank, 1 );
	state_save_register_UINT32( "rax", 0, "dmovlay", &rax_dmovlay, 1 );
	state_save_register_UINT16( "rax", 0, "size", &rax_size, 1 );
	state_save_register_UINT16( "rax", 0, "incs", &rax_incs, 1 );
	state_save_register_INT32( "rax", 0, "ireg", &rax_ireg, 1 );
	state_save_register_UINT16( "rax", 0, "ireg_base", &rax_ireg_base, 1 );
	state_save_register_UINT8( "rax", 0, "dac_enabled", rax_dac_enabled, 2 );
	state_save_register_double( "rax", 0, "reg_timer_period", &rax_reg_timer_period, 1 );
	state_save_register_double( "rax", 0, "reg_timer_left", &rax_reg_timer_left, 1 );
	state_save_register_double( "rax", 0, "dma_timer_left", &rax_dma_timer_left, 1 );
	state_save_register_UINT32( "rax", 0, "dma_param", &rax_dma_param, 1 );
	state_save_register_UINT32( "rax", 0, "buffer_in", rax_buffer_in, 2 );
	state_save_register_UINT32( "rax", 0, "buffer_out", rax_buffer_out, 2 );
	state_save_register_UINT32( "rax", 0, "sample_rate", &rax_sample_rate, 1 );
	state_save_register_UINT32( "rax", 0, "sample_step", &rax_sample_step, 1 );
	state_save_register_UINT32( "rax", 0, "sample_position", &rax_sample_position, 1 );
	state_save_register_func_presave( rax_presave );
	state_save_register_func_postload( rax_postload );
}

void acclaim_rax_reset( void )
{
	UINT32 *pgm = (UINT32 *)( memory_region( REGION_CPU1 + rax_cpunum ) + ADSP2181_PGM_OFFSET );
	int i;

	/* the boot: 32 program words (96 bytes) from the ROM, as by BDMA */
	for( i = 0; i < 32; i++ )
		pgm[ i ] = ( rax_rom[ i * 3 + 0 ] << 16 ) | ( rax_rom[ i * 3 + 1 ] << 8 ) | rax_rom[ i * 3 + 2 ];

	rax_pf0 = 1;
	rax_rom_bank = 0;
	rax_size = 0;
	rax_incs = 0;
	rax_ireg = 0;
	memset( rax_control_regs, 0, sizeof( rax_control_regs ) );
	rax_dmovlay = 0;
	rax_data_bank = 0;
	rax_update_data_ram_bank();

	rax_dac_enabled[ 0 ] = rax_dac_enabled[ 1 ] = 0;
	rax_buffer_in[ 0 ] = rax_buffer_out[ 0 ] = rax_buffer_in[ 1 ] = rax_buffer_out[ 1 ] = 0;
	rax_sample_position = 0;
	rax_sample_step = 0x10000;
	rax_reg_timer_period = 0;
	timer_adjust( rax_reg_timer, TIME_NEVER, 0, 0 );
	timer_adjust( rax_dma_timer, TIME_NEVER, 0, 0 );
}

/* from the main CPU: a command, IRQL0 on the ADSP */
void acclaim_rax_data_w( UINT16 data )
{
	rax_data_in = data;
	cpu_set_irq_line( rax_cpunum, ADSP2181_IRQL0, ASSERT_LINE );
	cpu_boost_interleave( 0, TIME_IN_USEC( 5 ) );
}
