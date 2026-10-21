/***************************************************************************

    Sega Model 1

    From MAME (sega/model1.cpp, copyright Olivier Galibert, BSD-3-Clause):
    the main board as current MAME (interrupt controller with its mask,
    the two interval timers, the UART to the sound board), the TGP as
    MAME 0.215 (high level, see machine/model1_machine.c).

    V60 16 MHz, TGP (MB86233) 16 MHz, Model 1 sound board (68000 10 MHz,
    2 MultiPCM, YM3438), Digital Sound Board (Z80 + MPEG audio) on
    Star Wars Arcade, Model 1 I/O board behind a 2 KB dual port RAM.

    The I/O board firmware (model1io / model1io2 BIOS) is simulated: the
    inputs and the analog channels are written where the games read
    them and the backup commands work on a 256 byte NVRAM. The link
    board (Virtua Racing, Virtua Formula, Wing War) is not emulated: the
    games see it with its CPU held in reset and run stand-alone. The R360
    motion base (Wing War R360) reports the game states back as the real
    one does. The Polhemus tracker of the NetMerc visor is not emulated:
    the visor pose is fixed, looking ahead; the gun follows the stick.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/segaic24.h"
#include "cpu/v60/v60.h"
#include "cpu/mb86233/mb86233.h"
#include "sound/multipcm.h"
#include "sound/2612intf.h"
#include "includes/segadsb.h"
#include "includes/model1.h"

#define CPU_MAIN	0
#define CPU_TGP		1
#define CPU_SOUND	2
#define CPU_DSB		3

#define V60_CLOCK	16000000


/*************************************
 *  Interrupts

    E00000: acknowledge (0x10 all, 0x20 the last one taken)
    E00002: per-level mask, active low (bit n set = level n masked)
    level 0: interval timers (Star Wars Arcade pumps its sound queue here)
    level 1: vblank
    level 3: UART ready (Virtua Fighter pumps its sound queue here)
 *************************************/

static UINT8 irq_status, last_irq, irq_mask;

static void irq_update( void )
{
	cpu_set_irq_line( CPU_MAIN, 0, irq_status ? ASSERT_LINE : CLEAR_LINE );
}

static void irq_raise( int level )
{
	irq_status |= 1 << level;
	irq_update();
}

static int irq_callback( int irqline )
{
	int i;
	for( i = 0; i < 8; i++ )
		if( ( irq_status >> i ) & 1 )
		{
			last_irq = i;
			break;
		}
	return last_irq;
}

static WRITE16_HANDLER( irq_control_w )
{
	if( !ACCESSING_LSB )
		return;
	switch( data & 0xff )
	{
	case 0x10:
		irq_status = 0;
		break;
	case 0x20:
		irq_status &= ~( 1 << last_irq );
		break;
	}
	irq_update();
}

static void sound_ready_check( void );

static READ16_HANDLER( irq_mask_r )
{
	return irq_mask;
}

static WRITE16_HANDLER( irq_mask_w )
{
	if( !ACCESSING_LSB )
		return;
	irq_mask = data & 0xff;
	sound_ready_check();
}

static INTERRUPT_GEN( model1_vblank )
{
	model1_video_vblank_start();
	if( !( irq_mask & 0x02 ) )
		irq_raise( 1 );
	model1_tgp_oracle_frame();
}


/*************************************
 *  Interval timers (level 0), counting V60 clocks / 0x800
 *************************************/

static UINT16 timer_mode, timer_period[ 2 ], timer_val[ 2 ];
static void *irq0_timer[ 2 ];

static double timer_time( int i )
{
	return TIME_IN_HZ( (double)V60_CLOCK / ( 0x800 * (double)timer_period[ i ] ) );
}

static void irq0_timer_tick( int param )
{
	if( !( irq_mask & 0x01 ) )
		irq_raise( 0 );
	if( timer_period[ param ] )
		timer_adjust( irq0_timer[ param ], timer_time( param ), param, 0 );
}

static WRITE16_HANDLER( timer_mode_w )
{
	COMBINE_DATA( &timer_mode );
}

static WRITE16_HANDLER( timer_period_w )
{
	COMBINE_DATA( &timer_period[ offset ] );
	if( timer_period[ offset ] )
		timer_adjust( irq0_timer[ offset ], timer_time( offset ), offset, 0 );
	else
		timer_adjust( irq0_timer[ offset ], TIME_NEVER, 0, 0 );
}

static READ16_HANDLER( timer_r )
{
	if( timer_period[ offset ] )
	{
		double left = timer_timeleft( irq0_timer[ offset ] );
		if( left > 0 && left < TIME_NEVER )
			timer_val[ offset ] = (UINT16)( (UINT32)( left * V60_CLOCK ) / 0x800 );
	}
	return timer_val[ offset ];
}


/*************************************
 *  Banking
 *************************************/

static UINT8 rom_bank;

static void set_rom_bank( void )
{
	/* 100000-1fffff data roms */
	UINT32 base = 0x1000000 + 0x100000 * rom_bank;
	if( base + 0x100000 > memory_region_length( REGION_CPU1 ) )
		base = 0x1000000;
	cpu_setbank( 1, memory_region( REGION_CPU1 ) + base );
}

static WRITE16_HANDLER( bank_w )
{
	if( ACCESSING_LSB && ( data & 0xf ) == 0x1 )
	{
		/* netmerc has bit 0x80 set, not a bank bit */
		rom_bank = ( data >> 4 ) & 0x7;
		set_rom_bank();
	}
}


/*************************************
 *  Sound link: uPD71051 (i8251) UART to the Model 1 sound board

    31250 baud, 320 us a byte each way. The V60 side is polled and
    interrupts on level 3 when it can send or has a byte (TxRDY, RxRDY);
    the sound board's 68000 takes a byte on IRQ 2. What the sound board
    sends goes to the V60 and to the Digital Sound Board (Star Wars).
 *************************************/

#define UART_Q		64
#define MIDI_BYTE_TIME	TIME_IN_USEC( 320 )

static UINT8 tx_buffer, tx_shift;
static int tx_full, tx_busy;
static void *tx_timer;

static UINT8 rx_q[ UART_Q ];
static int rx_head, rx_count;

/* bytes waiting for the sound board's 68000: the real UART holds one and
   loses the next while the 68000 is busy, which here happens at boot (the
   V60 sends its first commands before the 68000 has finished its init,
   as the simulated I/O board answers at once); a queue keeps them */
static UINT8 snd_rx_q[ UART_Q ];
static int snd_rx_head, snd_rx_count;

static int uart_txrdy( void )	{ return !tx_full; }
static int uart_rxrdy( void )	{ return rx_count > 0; }

static void sound_ready_check( void )
{
	if( ( uart_txrdy() || uart_rxrdy() ) && !( irq_mask & 0x08 ) )
		irq_raise( 3 );
}

static void uart_tx_start( UINT8 data )
{
	tx_shift = data;
	tx_busy = 1;
	timer_adjust( tx_timer, MIDI_BYTE_TIME, 0, 0 );
}

static void uart_tx_cb( int param )
{
	/* the byte reaches the sound board (an unread one is overrun) */
	if( snd_rx_count < UART_Q )
	{
		snd_rx_q[ ( snd_rx_head + snd_rx_count ) % UART_Q ] = tx_shift;
		snd_rx_count++;
	}
	cpu_set_irq_line( CPU_SOUND, 2, ASSERT_LINE );

	tx_busy = 0;
	if( tx_full )
	{
		tx_full = 0;
		uart_tx_start( tx_buffer );
		sound_ready_check();	/* TxRDY up again */
	}
}

static READ16_HANDLER( uart_r )
{
	if( offset == 0 )
	{
		UINT8 data = 0;
		if( rx_count > 0 )
		{
			data = rx_q[ rx_head ];
			rx_head = ( rx_head + 1 ) % UART_Q;
			rx_count--;
		}
		return data;
	}
	/* DSR, TxEMPTY, RxRDY, TxRDY */
	return 0x80 | ( ( !tx_busy && !tx_full ) ? 0x04 : 0 ) | ( uart_rxrdy() ? 0x02 : 0 ) | ( uart_txrdy() ? 0x01 : 0 );
}

static WRITE16_HANDLER( uart_w )
{
	if( offset != 0 || !ACCESSING_LSB )
		return;	/* mode / command */
	if( !tx_busy )
	{
		uart_tx_start( data & 0xff );
		sound_ready_check();	/* the buffer emptied at once */
	}
	else
	{
		tx_buffer = data & 0xff;
		tx_full = 1;
	}
}

/* the sound board's UART, seen from its 68000 */
static READ16_HANDLER( snd_uart_r )
{
	if( offset == 0 )
	{
		UINT8 data = snd_rx_q[ snd_rx_head ];
		if( snd_rx_count )
		{
			snd_rx_head = ( snd_rx_head + 1 ) % UART_Q;
			snd_rx_count--;
			if( !snd_rx_count )
				cpu_set_irq_line( CPU_SOUND, 2, CLEAR_LINE );
		}
		return data;
	}
	return 0x85 | ( snd_rx_count ? 0x02 : 0x00 );
}

static int model1_has_dsb;

static WRITE16_HANDLER( snd_uart_w )
{
	if( offset != 0 || !ACCESSING_LSB )
		return;
	if( rx_count < UART_Q )
	{
		rx_q[ ( rx_head + rx_count ) % UART_Q ] = data & 0xff;
		rx_count++;
	}
	if( model1_has_dsb )
		segadsb_midi_in( data & 0xff );
	sound_ready_check();
}


/*************************************
 *  Model 1 I/O board (simulated firmware)

    dual port RAM, as the games use it:
    00-07  analog channels
    08-0a  switch inputs IN0 - IN2 (active low)
    0b-0d  DIP switches DSW1 - DSW3 (active low, all off)
    0e     board switches (active low)
    0f     outputs (lamps, coin counters), written by the game
    11     drive board / motion base output, written by the game
    20     command to the board, cleared by the board when taken:
           1 = refresh inputs, 2 = store 0x100-0x1ff into the backup,
           3 = load the backup into 0x100-0x1ff
    21     board status, 0x40 = ready
 *************************************/

static UINT8 dpram[ 0x800 ];
static UINT8 io_backup[ 0x100 ];
static INT8 io_analog_port[ 8 ];	/* input port of each channel, -1 none */
static int io_in2_port;				/* input port of IN2, -1 none */
static int io_r360;					/* Wing War R360: IN2 is the motion base */
static int io_hmd;					/* NetMerc: the visor pose at 0x80 */
static UINT8 nvram_fill;			/* a new backup RAM */
static UINT8 r360_state;

static UINT8 dpram_read( int a )
{
	if( a < 8 )
	{
		return io_analog_port[ a ] >= 0 ? readinputport( io_analog_port[ a ] ) : 0;
	}
	switch( a )
	{
	case 0x08:	return readinputport( 0 );
	case 0x09:	return readinputport( 1 );
	case 0x0a:
		if( io_r360 )
			return r360_state;
		return io_in2_port >= 0 ? readinputport( io_in2_port ) : 0xff;
	case 0x0b: case 0x0c: case 0x0d:	/* DSW1-3, all off */
	case 0x0e:	return 0xff;
	}
	return dpram[ a ];
}

/* Wing War R360: the game states on the drive board line, as MAME */
static void r360_w( UINT8 data )
{
	switch( data )
	{
	case 0xbf: case 0xbe:	r360_state = ~0x40; break;
	case 0xbd:				r360_state = ~0x44; break;
	case 0xbc:				r360_state = ~0x45; break;
	case 0xbb:				r360_state = ~0x46; break;
	case 0xba: case 0xb9:	r360_state = ~0x40; break;
	}
}

static void dpram_write( int a, UINT8 data )
{
	dpram[ a ] = data;
	if( a == 0x11 && io_r360 )	/* drive board output */
		r360_w( data );
	if( a == 0x20 && data != 0 )
	{
		switch( data )
		{
		case 2:	memcpy( io_backup, &dpram[ 0x100 ], 0x100 ); break;
		case 3:	memcpy( &dpram[ 0x100 ], io_backup, 0x100 ); break;
		}
		dpram[ 0x20 ] = 0;
		dpram[ 0x21 ] = 0x40;
	}
}

static READ16_HANDLER( dpram_r )
{
	activecpu_adjust_icount( -1 );	/* wait state */
	return dpram_read( offset & 0x7ff );
}

static WRITE16_HANDLER( dpram_w )
{
	if( ACCESSING_LSB )
		dpram_write( offset & 0x7ff, data & 0xff );
}


/*************************************
 *  Link board, held in reset: the shared RAM and the CN / FG flags
 *************************************/

static UINT16 *comm_ram;
static UINT8 comm_cn, comm_fg;

static READ16_HANDLER( comm_r )
{
	if( offset == 0 )
		return comm_cn | 0xfe;
	return comm_fg | 0x80 | 0x7e;
}

static WRITE16_HANDLER( comm_w )
{
	if( !ACCESSING_LSB )
		return;
	if( offset == 0 )
		comm_cn = data & 1;
	else if( comm_cn )
		comm_fg = data & 1;
}


/*************************************
 *  Video glue
 *************************************/

static WRITE16_HANDLER( md0_w )	{ COMBINE_DATA( model1_display_list0 + offset ); }
static WRITE16_HANDLER( md1_w )	{ COMBINE_DATA( model1_display_list1 + offset ); }


static UINT16 *model1_nvram;


/*************************************
 *  Memory maps
 *************************************/

/* the TGP registers repeat over their 128 KB blocks */
static READ16_HANDLER( copro_adr_r )	{ return model1_tgp_copro_adr_r( 0, mem_mask ); }
static WRITE16_HANDLER( copro_adr_w )	{ model1_tgp_copro_adr_w( 0, data, mem_mask ); }
static READ16_HANDLER( copro_ram_r )	{ return model1_tgp_copro_ram_r( offset & 1, mem_mask ); }
static WRITE16_HANDLER( copro_ram_w )	{ model1_tgp_copro_ram_w( offset & 1, data, mem_mask ); }
static READ16_HANDLER( copro_fifo_r )	{ return model1_tgp_copro_r( offset & 1, mem_mask ); }
static WRITE16_HANDLER( copro_fifo_w )	{ model1_tgp_copro_w( offset & 1, data, mem_mask ); }

static MEMORY_READ16_START( model1_readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM },
	{ 0x100000, 0x1fffff, MRA16_BANK1 },
	{ 0x200000, 0x2fffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, MRA16_RAM },
	{ 0x500000, 0x53ffff, MRA16_RAM },
	{ 0x600000, 0x60ffff, MRA16_RAM },
	{ 0x610000, 0x61ffff, MRA16_RAM },
	{ 0x680000, 0x680003, model1_listctl_r },
	{ 0x700000, 0x70ffff, sys24_tile_r },
	{ 0x780000, 0x7fffff, sys24_char_r },
	{ 0x900000, 0x903fff, MRA16_RAM },
	{ 0x910000, 0x91bfff, MRA16_RAM },
	{ 0xb00000, 0xb00fff, MRA16_RAM },
	{ 0xb01000, 0xb01003, comm_r },
	{ 0xc00000, 0xc00fff, dpram_r },
	{ 0xc40000, 0xc40003, uart_r },
	{ 0xd00000, 0xd1ffff, copro_adr_r },
	{ 0xdc0000, 0xddffff, model1_fifoin_status_r },
	{ 0xe00002, 0xe00003, irq_mask_r },
	{ 0xe0000c, 0xe0000f, timer_r },
	{ 0xf80000, 0xffffff, MRA16_ROM },
MEMORY_END

static MEMORY_WRITE16_START( model1_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },
	{ 0x100000, 0x1fffff, MWA16_ROM },
	{ 0x200000, 0x2fffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, MWA16_RAM, &model1_nvram },
	{ 0x500000, 0x53ffff, MWA16_RAM },
	{ 0x600000, 0x60ffff, md0_w, &model1_display_list0 },
	{ 0x610000, 0x61ffff, md1_w, &model1_display_list1 },
	{ 0x680000, 0x680003, model1_listctl_w },
	{ 0x700000, 0x70ffff, sys24_tile_w },
	{ 0x720000, 0x720001, MWA16_NOP },	/* unknown, always 0 */
	{ 0x740000, 0x740001, MWA16_NOP },	/* horizontal synchronization */
	{ 0x760000, 0x760001, MWA16_NOP },	/* vertical synchronization */
	{ 0x770000, 0x770001, MWA16_NOP },	/* video synchronization switch */
	{ 0x780000, 0x7fffff, sys24_char_w },
	{ 0x900000, 0x903fff, model1_paletteram_w, &paletteram16 },
	{ 0x910000, 0x91bfff, MWA16_RAM, &model1_color_xlat },
	{ 0xb00000, 0xb00fff, MWA16_RAM, &comm_ram },
	{ 0xb01000, 0xb01003, comm_w },
	{ 0xc00000, 0xc00fff, dpram_w },
	{ 0xc40000, 0xc40003, uart_w },
	{ 0xd00000, 0xd1ffff, copro_adr_w },
	{ 0xd20000, 0xd3ffff, copro_ram_w },
	{ 0xd80000, 0xd9ffff, copro_fifo_w },
	{ 0xe00000, 0xe00001, irq_control_w },
	{ 0xe00002, 0xe00003, irq_mask_w },
	{ 0xe00004, 0xe00005, bank_w },
	{ 0xe00006, 0xe00007, timer_mode_w },
	{ 0xe00008, 0xe0000b, timer_period_w },
	{ 0xe0000c, 0xe0000f, MWA16_NOP },	/* vf/swa clear the counts at init */
	{ 0xf80000, 0xffffff, MWA16_ROM },
MEMORY_END

/* the TGP is also in the I/O space, as in memory: the games read its
   results and RAM with IN instructions (they can wait), and Wing War
   sends its boot command (8, clear the matrix stack) with OUT */
static PORT_READ16_START( model1_readport )
	{ 0xd00000, 0xd1ffff, copro_adr_r },
	{ 0xd20000, 0xd3ffff, copro_ram_r },
	{ 0xd80000, 0xd9ffff, copro_fifo_r },
	{ 0xdc0000, 0xddffff, model1_fifoin_status_r },
PORT_END

static PORT_WRITE16_START( model1_writeport )
	{ 0xd00000, 0xd1ffff, copro_adr_w },
	{ 0xd20000, 0xd3ffff, copro_ram_w },
	{ 0xd80000, 0xd9ffff, copro_fifo_w },
PORT_END

static MEMORY_READ32_START( tgp_readmem )
	{ 0x00000000, 0x00000003, MRA32_NOP },
MEMORY_END

static MEMORY_WRITE32_START( tgp_writemem )
	{ 0x00000000, 0x00000003, MWA32_NOP },
MEMORY_END


/* Model 1 sound board: 68000, 2 MultiPCM, YM3438 */

static READ16_HANDLER( m1snd_mpcm0_r )	{ return MultiPCM_reg_0_r( 0 ); }
static READ16_HANDLER( m1snd_mpcm1_r )	{ return MultiPCM_reg_1_r( 0 ); }
static WRITE16_HANDLER( m1snd_mpcm0_w )	{ MultiPCM_reg_0_w( offset, data ); }
static WRITE16_HANDLER( m1snd_mpcm1_w )	{ MultiPCM_reg_1_w( offset, data ); }
static WRITE16_HANDLER( m1snd_mpcm0_bnk_w )	{ multipcm_set_bank( 0, 0x100000 * ( data & 3 ), 0x100000 * ( data & 3 ) ); }
static WRITE16_HANDLER( m1snd_mpcm1_bnk_w )	{ multipcm_set_bank( 1, 0x100000 * ( data & 3 ), 0x100000 * ( data & 3 ) ); }
static READ16_HANDLER( m1snd_ym_r )	{ return YM2612_status_port_0_A_r( 0 ); }

static WRITE16_HANDLER( m1snd_ym_w )
{
	switch( offset )
	{
	case 0:	YM2612_control_port_0_A_w( 0, data ); break;
	case 1:	YM2612_data_port_0_A_w( 0, data ); break;
	case 2:	YM2612_control_port_0_B_w( 0, data ); break;
	case 3:	YM2612_data_port_0_B_w( 0, data ); break;
	}
}

static MEMORY_READ16_START( m1snd_readmem )
	{ 0x000000, 0x0bffff, MRA16_ROM },	/* 080000-09ffff: the upper socket again */
	{ 0xc20000, 0xc20003, snd_uart_r },
	{ 0xc40000, 0xc40007, m1snd_mpcm0_r },
	{ 0xc60000, 0xc60007, m1snd_mpcm1_r },
	{ 0xd00000, 0xd00007, m1snd_ym_r },
	{ 0xf00000, 0xf0ffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( m1snd_writemem )
	{ 0x000000, 0x0bffff, MWA16_ROM },
	{ 0xc20000, 0xc20003, snd_uart_w },
	{ 0xc40000, 0xc40007, m1snd_mpcm0_w },
	{ 0xc40012, 0xc40013, MWA16_NOP },
	{ 0xc50000, 0xc50001, m1snd_mpcm0_bnk_w },
	{ 0xc60000, 0xc60007, m1snd_mpcm1_w },
	{ 0xc70000, 0xc70001, m1snd_mpcm1_bnk_w },
	{ 0xd00000, 0xd00007, m1snd_ym_w },
	{ 0xf00000, 0xf0ffff, MWA16_RAM },
MEMORY_END

static struct MultiPCM_interface m1snd_multipcm_interface =
{
	2,
	{ 8000000, 8000000 },
	{ REGION_SOUND1, REGION_SOUND2 },
	{ YM3012_VOL( 100, MIXER_PAN_LEFT, 100, MIXER_PAN_RIGHT ), YM3012_VOL( 100, MIXER_PAN_LEFT, 100, MIXER_PAN_RIGHT ) }
};

static struct YM2612interface m1snd_ym3438_interface =
{
	1,
	8000000,
	{ 60, 60 },
	{ 0 }, { 0 }, { 0 }, { 0 }
};


/* Digital Sound Board (Star Wars Arcade) */

static MEMORY_READ_START( dsbz80_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( dsbz80_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xffff, MWA_RAM },
MEMORY_END

static PORT_READ_START( dsbz80_readport )
	{ 0x00, 0xff, segadsb_z80_port_r },
PORT_END

static PORT_WRITE_START( dsbz80_writeport )
	{ 0x00, 0xff, segadsb_z80_port_w },
PORT_END


/*************************************
 *  Machine
 *************************************/

/* the save state keeps neither the ROM bank pointer nor the timers: their
   time left is saved here and they are set again on load */
static double timer_left[ 3 ];

static double time_left( void *t )
{
	double left = timer_timeleft( t );
	return ( left > 0 && left < TIME_NEVER ) ? left : -1;
}

static void model1_presave( void )
{
	timer_left[ 0 ] = time_left( irq0_timer[ 0 ] );
	timer_left[ 1 ] = time_left( irq0_timer[ 1 ] );
	timer_left[ 2 ] = time_left( tx_timer );
}

static void model1_postload( void )
{
	int i;
	set_rom_bank();
	for( i = 0; i < 2; i++ )
		timer_adjust( irq0_timer[ i ], timer_left[ i ] >= 0 ? timer_left[ i ] : TIME_NEVER, i, 0 );
	timer_adjust( tx_timer, timer_left[ 2 ] >= 0 ? timer_left[ 2 ] : TIME_NEVER, 0, 0 );
}

static MACHINE_INIT( model1 )
{
	rom_bank = 0;
	set_rom_bank();

	irq_status = 0;
	last_irq = 0;
	irq_mask = 0xff;
	cpu_set_irq_line( CPU_MAIN, 0, CLEAR_LINE );
	cpu_set_irq_callback( CPU_MAIN, irq_callback );

	timer_mode = 0;
	timer_period[ 0 ] = timer_period[ 1 ] = 0;
	timer_val[ 0 ] = timer_val[ 1 ] = 0;
	timer_adjust( irq0_timer[ 0 ], TIME_NEVER, 0, 0 );
	timer_adjust( irq0_timer[ 1 ], TIME_NEVER, 1, 0 );

	tx_full = tx_busy = 0;
	timer_adjust( tx_timer, TIME_NEVER, 0, 0 );
	rx_head = rx_count = 0;
	snd_rx_head = snd_rx_count = 0;
	cpu_set_irq_line( CPU_SOUND, 2, CLEAR_LINE );

	memset( dpram, 0, 0x100 );
	dpram[ 0x21 ] = 0x40;	/* board ready */
	if( io_hmd )
	{
		/* NetMerc: the Polhemus tracker of the visor is not emulated; as
		   current MAME, its pose (x, y, z, then three angles in radians
		   * 8192) is set once so that the camera looks ahead */
		static const INT16 pose[ 6 ] = { 0, 0, 0, 12868, 25736, 12868 };
		int i;
		for( i = 0; i < 6; i++ )
		{
			dpram[ 0x80 + i * 2 ] = pose[ i ] & 0xff;
			dpram[ 0x81 + i * 2 ] = ( pose[ i ] >> 8 ) & 0xff;
		}
	}
	r360_state = ~0x40;
	comm_cn = comm_fg = 0;

	model1_tgp_reset();
	if( model1_tgp_mode == MODEL1_TGP_HLE )
		cpunum_suspend( CPU_TGP, SUSPEND_REASON_DISABLE, 1 );

	if( model1_has_dsb )
		segadsb_reset();
}

static NVRAM_HANDLER( model1 )
{
	if( read_or_write )
	{
		mame_fwrite( file, model1_nvram, 0x10000 );
		mame_fwrite( file, io_backup, 0x100 );
	}
	else if( file )
	{
		mame_fread( file, model1_nvram, 0x10000 );
		mame_fread( file, io_backup, 0x100 );
	}
	else
	{
		memset( model1_nvram, nvram_fill, 0x10000 );
		memset( io_backup, 0, 0x100 );
	}
}

static void model1_common_init( int table, int dsb )
{
	int i;

	model1_tgp_table = table;
	model1_tgp_cpu = CPU_TGP;
#ifdef MODEL1_TGP_DEV
	{
		/* development builds: MODEL1_TGP=lle / oracle */
		const char *env = getenv( "MODEL1_TGP" );
		model1_tgp_mode = MODEL1_TGP_HLE;
		if( env && !strcmp( env, "lle" ) )
			model1_tgp_mode = MODEL1_TGP_LLE;
		if( env && !strcmp( env, "oracle" ) )
			model1_tgp_mode = MODEL1_TGP_ORACLE;
	}
#else
	model1_tgp_mode = MODEL1_TGP_HLE;
#endif
	model1_tgp_init();

	for( i = 0; i < 8; i++ )
		io_analog_port[ i ] = -1;
	io_in2_port = -1;
	io_r360 = 0;
	io_hmd = 0;
	nvram_fill = 0xff;	/* vf needs the RAM 1-filled for its rankings */

	model1_has_dsb = dsb;
	if( dsb )
	{
		segadsb_mpeg_region = REGION_SOUND3;
		segadsb_init( SEGADSB_Z80, CPU_DSB );
	}
	else
		segadsb_init( SEGADSB_NONE, 0 );

	irq0_timer[ 0 ] = timer_alloc( irq0_timer_tick );
	irq0_timer[ 1 ] = timer_alloc( irq0_timer_tick );
	tx_timer = timer_alloc( uart_tx_cb );

	state_save_register_UINT8( "model1", 0, "irq_status", &irq_status, 1 );
	state_save_register_UINT8( "model1", 0, "rom_bank", &rom_bank, 1 );
	state_save_register_double( "model1", 0, "timer_left", timer_left, 3 );
	state_save_register_func_presave( model1_presave );
	state_save_register_func_postload( model1_postload );
	state_save_register_UINT8( "model1", 0, "last_irq", &last_irq, 1 );
	state_save_register_UINT8( "model1", 0, "irq_mask", &irq_mask, 1 );
	state_save_register_UINT16( "model1", 0, "timer_mode", &timer_mode, 1 );
	state_save_register_UINT16( "model1", 0, "timer_period", timer_period, 2 );
	state_save_register_UINT16( "model1", 0, "timer_val", timer_val, 2 );
	state_save_register_UINT8( "model1", 0, "tx_buffer", &tx_buffer, 1 );
	state_save_register_UINT8( "model1", 0, "tx_shift", &tx_shift, 1 );
	state_save_register_int( "model1", 0, "tx_full", &tx_full );
	state_save_register_int( "model1", 0, "tx_busy", &tx_busy );
	state_save_register_UINT8( "model1", 0, "rx_q", rx_q, UART_Q );
	state_save_register_int( "model1", 0, "rx_head", &rx_head );
	state_save_register_int( "model1", 0, "rx_count", &rx_count );
	state_save_register_UINT8( "model1", 0, "snd_rx_q", snd_rx_q, UART_Q );
	state_save_register_int( "model1", 0, "snd_rx_head", &snd_rx_head );
	state_save_register_int( "model1", 0, "snd_rx_count", &snd_rx_count );
	state_save_register_UINT8( "model1", 0, "dpram", dpram, 0x800 );
	state_save_register_UINT8( "model1", 0, "io_backup", io_backup, 0x100 );
	state_save_register_UINT8( "model1", 0, "r360", &r360_state, 1 );
	state_save_register_UINT8( "model1", 0, "comm_cn", &comm_cn, 1 );
	state_save_register_UINT8( "model1", 0, "comm_fg", &comm_fg, 1 );
}

static void set_analog( int a0, int a1, int a2, int a3, int a4, int a5 )
{
	io_analog_port[ 0 ] = a0;
	io_analog_port[ 1 ] = a1;
	io_analog_port[ 2 ] = a2;
	io_analog_port[ 3 ] = a3;
	io_analog_port[ 4 ] = a4;
	io_analog_port[ 5 ] = a5;
}

/* input ports: 0 IN0, 1 IN1, 2 IN2 or the first analog channel, ... */
static DRIVER_INIT( vf )
{
	model1_common_init( MODEL1_TGP_TABLE_VF, 0 );
	io_in2_port = 2;
}

static DRIVER_INIT( vr )
{
	model1_common_init( MODEL1_TGP_TABLE_VR, 0 );
	io_in2_port = 2;
	set_analog( 3, 4, 5, -1, -1, -1 );
}

static DRIVER_INIT( swa )
{
	model1_common_init( MODEL1_TGP_TABLE_SWA, 1 );
	io_in2_port = 2;
	set_analog( 3, 4, 5, -1, 6, 7 );
}

static DRIVER_INIT( wingwar )
{
	model1_common_init( MODEL1_TGP_TABLE_SWA, 0 );
	set_analog( 2, 3, 4, -1, -1, -1 );
}

static DRIVER_INIT( wingwar360 )
{
	model1_common_init( MODEL1_TGP_TABLE_SWA, 0 );
	set_analog( 2, 3, -1, -1, -1, -1 );
	io_r360 = 1;
}

static DRIVER_INIT( netmerc )
{
	model1_common_init( MODEL1_TGP_TABLE_SWA, 0 );
	set_analog( 2, -1, 3, -1, -1, -1 );
	io_hmd = 1;
	/* a new RAM must be 0-filled: a zero gun mount calibration makes the
	   game set its factory one (1s are kept and make it divide by zero) */
	nvram_fill = 0x00;
}

static MACHINE_DRIVER_START( model1 )
	MDRV_CPU_ADD_TAG( "main", V60, V60_CLOCK )
	MDRV_CPU_MEMORY( model1_readmem, model1_writemem )
	MDRV_CPU_PORTS( model1_readport, model1_writeport )
	MDRV_CPU_VBLANK_INT( model1_vblank, 1 )

	MDRV_CPU_ADD_TAG( "tgp", MB86233, 16000000 )
	MDRV_CPU_CONFIG( model1_tgp_config )
	MDRV_CPU_MEMORY( tgp_readmem, tgp_writemem )

	MDRV_CPU_ADD_TAG( "sound", M68000, 10000000 )	/* 20 MHz XTAL / 2 */
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( m1snd_readmem, m1snd_writemem )

	/* 16 MHz pixel clock, 656 x 424 total, vblank from line 384 */
	MDRV_FRAMES_PER_SECOND( 16000000.0 / ( 656 * 424 ) )
	MDRV_VBLANK_DURATION( 1000000.0 * 40 / ( 16000000.0 / 656 ) )
	MDRV_INTERLEAVE( 10 )

	MDRV_MACHINE_INIT( model1 )
	MDRV_NVRAM_HANDLER( model1 )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 496, 384 )
	MDRV_VISIBLE_AREA( 0, 495, 0, 383 )
	MDRV_PALETTE_LENGTH( 8192 )

	MDRV_VIDEO_START( model1 )
	MDRV_VIDEO_UPDATE( model1 )
	MDRV_VIDEO_EOF( model1 )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( YM3438, m1snd_ym3438_interface )
	MDRV_SOUND_ADD( MULTIPCM, m1snd_multipcm_interface )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( swa )
	MDRV_IMPORT_FROM( model1 )

	MDRV_CPU_ADD_TAG( "dsb", Z80, 4000000 )	/* clock unknown, as MAME */
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( dsbz80_readmem, dsbz80_writemem )
	MDRV_CPU_PORTS( dsbz80_readport, dsbz80_writeport )

	MDRV_SOUND_ADD( CUSTOM, segadsb_custom_interface )
MACHINE_DRIVER_END


/*************************************
 *  Input ports
 *************************************/

#define MODEL1_SYSTEM \
	PORT_START	/* IN0 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )

INPUT_PORTS_START( vf )
	MODEL1_SYSTEM
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( vr )
	MODEL1_SYSTEM
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* VR1 (red) */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* VR2 (blue) */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )	/* VR3 (yellow) */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )	/* VR4 (green) */
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON7 | IPF_PLAYER1 )	/* shift down */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON8 | IPF_PLAYER1 )	/* shift up */
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2: from the drive board */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* wheel */
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_CENTER, 100, 3, 0, 0xff )

	PORT_START	/* accelerator */
	PORT_ANALOG( 0xff, 0x30, IPT_PEDAL, 100, 16, 1, 0xff )

	PORT_START	/* brake */
	PORT_ANALOG( 0xff, 0x30, IPT_PEDAL2, 100, 16, 1, 0xff )
INPUT_PORTS_END

INPUT_PORTS_START( swa )
	MODEL1_SYSTEM
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* stick 1 X */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_PLAYER1 | IPF_REVERSE, 100, 4, 27, 227 )

	PORT_START	/* stick 1 Y */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 4, 27, 227 )

	PORT_START	/* throttle */
	PORT_ANALOG( 0xff, 0x7f, IPT_PEDAL | IPF_PLAYER1 | IPF_REVERSE, 100, 16, 28, 200 )

	PORT_START	/* stick 2 X */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_PLAYER2 | IPF_REVERSE, 100, 4, 27, 227 )

	PORT_START	/* stick 2 Y */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 4, 27, 227 )
INPUT_PORTS_END

INPUT_PORTS_START( wingwar )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* view 1 */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )	/* view 2 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )	/* view 3 */

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON7 | IPF_PLAYER1 )	/* view 4 */
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* machine gun */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* missile */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* smoke */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* stick X */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1 | IPF_REVERSE, 100, 4, 0, 0xff )

	PORT_START	/* stick Y */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 4, 0, 0xff )

	PORT_START	/* throttle */
	PORT_ANALOG( 0xff, 0x01, IPT_PEDAL | IPF_PLAYER1, 100, 16, 1, 0xff )
INPUT_PORTS_END

INPUT_PORTS_START( wingwar360 )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* machine gun */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* missile */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* smoke */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* stick X */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 4, 0, 0xff )

	PORT_START	/* stick Y */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1 | IPF_REVERSE, 100, 4, 0, 0xff )
INPUT_PORTS_END

INPUT_PORTS_START( netmerc )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* trigger */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* thumb */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* MVD holder */
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNUSED )

	/* the gun mount ends, as the factory calibration of the game */
	PORT_START	/* gun left / right */
	PORT_ANALOG( 0xff, 0x85, IPT_AD_STICK_X | IPF_PLAYER1, 100, 4, 0x46, 0xc4 )

	PORT_START	/* gun up / down */
	PORT_ANALOG( 0xff, 0x45, IPT_AD_STICK_Y | IPF_PLAYER1 | IPF_REVERSE, 100, 4, 0x21, 0x69 )
INPUT_PORTS_END


/*************************************
 *  ROM definitions (current MAME)
 *************************************/

#define MODEL1_CPU_BOARD \
	ROM_REGION( 0x1000, REGION_CPU2, ROMREGION_ERASE00 ) /* TGP: address spaces are callbacks */ \
	ROM_REGION32_LE( 0x40000, REGION_USER2, 0 ) /* TGP tables */ \
	ROM_LOAD32_WORD( "opr14742.bin", 0x000000, 0x20000, CRC(446a1085) SHA1(51b3f4d3a35a36087ea0ba4e26d6e7d17b6418e2) ) \
	ROM_LOAD32_WORD( "opr14743.bin", 0x000002, 0x20000, CRC(e8953554) SHA1(1499f8e30ac15affc66e6f04ae031bb8680d9260) )

ROM_START( vf )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16082.14", 0x200000, 0x80000, CRC(b23f22ee) SHA1(9fd5b5a5974703a60a54de3d2bce4301bfc0e533) ) /* Rom board with Sega ID# 834-10170 */
	ROM_LOAD16_BYTE( "epr-16083.15", 0x200001, 0x80000, CRC(d12c77f8) SHA1(b4aeba8d5f1ab4aec024391407a2cb58ce2e94b0) )

	ROM_LOAD( "epr-16080.4", 0xfc0000, 0x20000, CRC(3662e1a5) SHA1(6bfceb1a7c1c7912679c907f2b7516ae9c7dda67) )
	ROM_LOAD( "epr-16081.5", 0xfe0000, 0x20000, CRC(6dec06ce) SHA1(7891544456bccd2fc647bccd058945ad50466636) )

	ROM_LOAD16_BYTE( "mpr-16084.6",  0x1000000, 0x80000, CRC(483f453b) SHA1(41a5527be73f5dd1c87b2a8113235bdd247ec049) )
	ROM_LOAD16_BYTE( "mpr-16085.7",  0x1000001, 0x80000, CRC(5fa01277) SHA1(dfa7ddff0a7daf29071431f26b93dd8e8e5793b6) )
	ROM_LOAD16_BYTE( "mpr-16086.8",  0x1100000, 0x80000, CRC(deac47a1) SHA1(3a8016124e4dc579d4aae745d4af1905ad0e4fbd) )
	ROM_LOAD16_BYTE( "mpr-16087.9",  0x1100001, 0x80000, CRC(7a64daac) SHA1(da6a9cad4b0cb2af4299e664c0889f3fbdc25530) )
	ROM_LOAD16_BYTE( "mpr-16088.10", 0x1200000, 0x80000, CRC(fcda2d1e) SHA1(0f7d0f604d429a1da0d1c3f31694520bada49680) )
	ROM_LOAD16_BYTE( "mpr-16089.11", 0x1200001, 0x80000, CRC(39befbe0) SHA1(362c493092cd0536fadee7326ecc7f973e23fb58) )
	ROM_LOAD16_BYTE( "mpr-16090.12", 0x1300000, 0x80000, CRC(90c76831) SHA1(5a3c25f2a131cfbb2ad067bef1ab7b1c95645d41) )
	ROM_LOAD16_BYTE( "mpr-16091.13", 0x1300001, 0x80000, CRC(53115448) SHA1(af798d5b1fcb720d7288a5ac48839d9ace16a2f2) )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5724.bin", 0, 0x2000, CRC(4b4f330e) SHA1(8809d93d47593f808faca55161999677ac7a3eb0) BAD_DUMP )

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-16120.7", 0x00000, 0x20000, CRC(2bff8378) SHA1(854b08ab983e4e98cb666f2f44de9a6829b1eb52) )
	ROM_LOAD16_WORD_SWAP( "epr-16121.8", 0x20000, 0x20000, CRC(ff6723f9) SHA1(53498b8c103745883657dfd6efe27edfd48b356f) )
	ROM_RELOAD( 0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16122.32", 0x000000, 0x200000, CRC(568bc64e) SHA1(31fd0ef8319efe258011b4621adebb790b620770) )
	ROM_LOAD( "mpr-16123.33", 0x200000, 0x200000, CRC(15d78844) SHA1(37c17e38604cf7004a951408024941cd06b1d93e) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16124.4", 0x000000, 0x200000, CRC(45520ba1) SHA1(c33e3c12639961016e5fa6b5025d0a67dff28907) )
	ROM_LOAD( "mpr-16125.5", 0x200000, 0x200000, CRC(9b4998b6) SHA1(0418d9b0acf79f35d0f7575c21f1be9a0ea343da) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16096.26", 0x000000, 0x200000, CRC(a92b0bf3) SHA1(fd3adff5f41f0b0be98df548c848eda04fc0da48) )
	ROM_LOAD32_WORD( "mpr-16097.27", 0x000002, 0x200000, CRC(0232955a) SHA1(df934fb6d022032620932571ff5ed176d5dcb017) )
	ROM_LOAD32_WORD( "mpr-16098.28", 0x400000, 0x200000, CRC(cf2e1b84) SHA1(f3d16c72344f7f218a792ce7f1dd7cad910a8c97) )
	ROM_LOAD32_WORD( "mpr-16099.29", 0x400002, 0x200000, CRC(20e46854) SHA1(423d3642bd2f14e68d29029c027b791de2c1ec53) )
	ROM_LOAD32_WORD( "mpr-16100.30", 0x800000, 0x200000, CRC(e13e983d) SHA1(120637caa2404ad4124b676fd6fcd721f30948df) )
	ROM_LOAD32_WORD( "mpr-16101.31", 0x800002, 0x200000, CRC(0dbed94d) SHA1(df1cddcc1d3976816bd786c2d6211a8563f6f690) )
	ROM_LOAD32_WORD( "mpr-16102.32", 0xc00000, 0x200000, CRC(4cb41fb6) SHA1(4a07bfad4f221508de8c931861424dcc5be3f46a) )
	ROM_LOAD32_WORD( "mpr-16103.33", 0xc00002, 0x200000, CRC(526d1c76) SHA1(edc8dafc9261cd0e970c3b50e3c1ca51a32a4cdf) )
ROM_END

ROM_START( vr )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-14882.14", 0x200000, 0x80000, CRC(547d75ad) SHA1(a57c11966886c37de1d7df131ad60457669231dd) )
	ROM_LOAD16_BYTE( "epr-14883.15", 0x200001, 0x80000, CRC(6bfad8b1) SHA1(c1f780e456b405abd42d92f4e03e40aad88f8c22) )

	ROM_LOAD( "epr-14878a.4", 0xfc0000, 0x20000, CRC(6d69e695) SHA1(12d3612d3dfd474b8020cdfb8ffc5dcc64e2e1a3) )
	ROM_LOAD( "epr-14879a.5", 0xfe0000, 0x20000, CRC(d45af9dd) SHA1(48f2bf940c78e3ae4273a56e9f32371d870e41e0) )

	ROM_LOAD16_BYTE( "mpr-14880.6",  0x1000000, 0x80000, CRC(adc7c208) SHA1(967b6f522011f17fd2821ccbe20bcb6d4680a4a0) )
	ROM_LOAD16_BYTE( "mpr-14881.7",  0x1000001, 0x80000, CRC(e5ab89df) SHA1(873dea86628457e69f732158e3efb05d133eaa44) )
	ROM_LOAD16_BYTE( "mpr-14884.8",  0x1100000, 0x80000, CRC(6cf9c026) SHA1(f4d717958dba6b6402f5ffe7638fe0bf353b61ed) )
	ROM_LOAD16_BYTE( "mpr-14885.9",  0x1100001, 0x80000, CRC(f65c9262) SHA1(511a22bcfaf199737bfc5a809fd66cb4439dd386) )
	ROM_LOAD16_BYTE( "mpr-14886.10", 0x1200000, 0x80000, CRC(92868734) SHA1(e1dfb134dc3ba7e0b1d40681621e09ac5a222518) )
	ROM_LOAD16_BYTE( "mpr-14887.11", 0x1200001, 0x80000, CRC(10c7c636) SHA1(c77d55460bba4354349e473e129f21afeed05e91) )
	ROM_LOAD16_BYTE( "mpr-14888.12", 0x1300000, 0x80000, CRC(04bfdc5b) SHA1(bb8788a761620d0440a62ae51c3b41f70a04b5e4) )
	ROM_LOAD16_BYTE( "mpr-14889.13", 0x1300001, 0x80000, CRC(c49f0486) SHA1(cc2bb9059c016ba2c4f6e7508bd1687df07b8b48) )

	ROM_REGION( 0x2000, REGION_USER4, 0 ) /* TGP program rom */
	ROM_LOAD( "315-5573.bin", 0, 0x2000, CRC(3335a19b) SHA1(72eedfcc799ec4c7534fd7415de6631087ff6731) )

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-14870a.7", 0x00000, 0x20000, CRC(919d9b75) SHA1(27be79881cc9a2b5cf37e18f1e2d87251426b428) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-14873.32", 0x000000, 0x200000, CRC(b1965190) SHA1(fc47e9ed4a44d48477bd9a35e42c26508c0f4a0c) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-14876.4", 0x000000, 0x200000, CRC(ba6b2327) SHA1(02285520624a4e612cb4b65510e3458b13b1c6ba) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-14890.26", 0x000000, 0x200000, CRC(dcbe006b) SHA1(195be7fabec405ca1b4e1338d3b8d7bb4a06dd73) )
	ROM_LOAD32_WORD( "mpr-14891.27", 0x000002, 0x200000, CRC(25832b38) SHA1(a8d74538149c92bae661334e327b031eaca2a8f2) )
	ROM_LOAD32_WORD( "mpr-14892.28", 0x400000, 0x200000, CRC(5136f3ba) SHA1(ce8312975764db09bbfa2068b99559a5c1798a36) )
	ROM_LOAD32_WORD( "mpr-14893.29", 0x400002, 0x200000, CRC(1c531ada) SHA1(8b373ac97a3649c64f48eb3d1dd95c5833f330b6) )
	ROM_LOAD32_WORD( "mpr-14894.30", 0x800000, 0x200000, CRC(830a71bc) SHA1(884378e8a5afeb819daf5285d0d205986d566340) )
	ROM_LOAD32_WORD( "mpr-14895.31", 0x800002, 0x200000, CRC(af027ac5) SHA1(523f03d90358ddb7d0e96fd06b9a65cebfc09f24) )
	ROM_LOAD32_WORD( "mpr-14896.32", 0xc00000, 0x200000, CRC(382091dc) SHA1(efa266f0f6bfe36ad1c365e588fff33b01e166dd) )
	ROM_LOAD32_WORD( "mpr-14897.33", 0xc00002, 0x200000, CRC(74873195) SHA1(80705ec577d14570f9bba77cc26766f831c41f42) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-14898.39", 0x000000, 0x80000, CRC(61da2bb6) SHA1(7a12ba522d64a1aeec1ca6f5a87ee063692131f9) )
	ROM_LOAD32_BYTE( "mpr-14899.40", 0x000001, 0x80000, CRC(2cd58bee) SHA1(73defec823de4244a387af55fea7210edc1b314f) )
	ROM_LOAD32_BYTE( "mpr-14900.41", 0x000002, 0x80000, CRC(aa7c017d) SHA1(0fa2b59a8bb5f5907b2b2567e69d11c73b398dc1) )
	ROM_LOAD32_BYTE( "mpr-14901.42", 0x000003, 0x80000, CRC(175b7a9a) SHA1(c86602e771cd49bab425b4ba7926d2f44858bd39) )

ROM_END

ROM_START( vformula )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-15638.14", 0x200000, 0x80000, CRC(b9db21a2) SHA1(db58c047977f5fc37f278afe7159a78e3fa6c015) )
	ROM_LOAD16_BYTE( "epr-15639.15", 0x200001, 0x80000, CRC(4c3796f5) SHA1(1bf312a4999a15fbc5d194627f9c0ad9dbc1f2c0) )

	ROM_LOAD( "epr-15623.4", 0xfc0000, 0x20000, CRC(155fa5be) SHA1(a0a3fd8980c52279adbc1c64aa22e42a0b196dd9) )
	ROM_LOAD( "epr-15622.5", 0xfe0000, 0x20000, CRC(18007f6f) SHA1(61573742627ec027abd2cc700e79f04da5215bfd) )

	ROM_LOAD16_BYTE( "epr-15641.6",  0x1000000, 0x80000, CRC(bf01e4d5) SHA1(53ad9ecd2de2ea1d7b446f9db61352e10a55ea05) )
	ROM_LOAD16_BYTE( "epr-15640.7",  0x1000001, 0x80000, CRC(3e6d83dc) SHA1(62aa552a38ee193e0dfab5d1261756fe237db42c) )
	ROM_LOAD16_BYTE( "mpr-14884.8",  0x1100000, 0x80000, CRC(6cf9c026) SHA1(f4d717958dba6b6402f5ffe7638fe0bf353b61ed) )
	ROM_LOAD16_BYTE( "mpr-14885.9",  0x1100001, 0x80000, CRC(f65c9262) SHA1(511a22bcfaf199737bfc5a809fd66cb4439dd386) )
	ROM_LOAD16_BYTE( "mpr-14886.10", 0x1200000, 0x80000, CRC(92868734) SHA1(e1dfb134dc3ba7e0b1d40681621e09ac5a222518) )
	ROM_LOAD16_BYTE( "mpr-14887.11", 0x1200001, 0x80000, CRC(10c7c636) SHA1(c77d55460bba4354349e473e129f21afeed05e91) )
	ROM_LOAD16_BYTE( "mpr-14888.12", 0x1300000, 0x80000, CRC(04bfdc5b) SHA1(bb8788a761620d0440a62ae51c3b41f70a04b5e4) )
	ROM_LOAD16_BYTE( "mpr-14889.13", 0x1300001, 0x80000, CRC(c49f0486) SHA1(cc2bb9059c016ba2c4f6e7508bd1687df07b8b48) )

	ROM_REGION( 0x2000, REGION_USER4, 0 ) /* TGP program rom */
	ROM_LOAD( "315-5573.bin", 0, 0x2000, CRC(3335a19b) SHA1(72eedfcc799ec4c7534fd7415de6631087ff6731) )

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-14870a.7", 0x00000, 0x20000, CRC(919d9b75) SHA1(27be79881cc9a2b5cf37e18f1e2d87251426b428) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-14873.32", 0x000000, 0x200000, CRC(b1965190) SHA1(fc47e9ed4a44d48477bd9a35e42c26508c0f4a0c) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-14876.4", 0x000000, 0x200000, CRC(ba6b2327) SHA1(02285520624a4e612cb4b65510e3458b13b1c6ba) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-14890.26", 0x000000, 0x200000, CRC(dcbe006b) SHA1(195be7fabec405ca1b4e1338d3b8d7bb4a06dd73) )
	ROM_LOAD32_WORD( "mpr-14891.27", 0x000002, 0x200000, CRC(25832b38) SHA1(a8d74538149c92bae661334e327b031eaca2a8f2) )
	ROM_LOAD32_WORD( "mpr-14892.28", 0x400000, 0x200000, CRC(5136f3ba) SHA1(ce8312975764db09bbfa2068b99559a5c1798a36) )
	ROM_LOAD32_WORD( "mpr-14893.29", 0x400002, 0x200000, CRC(1c531ada) SHA1(8b373ac97a3649c64f48eb3d1dd95c5833f330b6) )
	ROM_LOAD32_WORD( "mpr-14894.30", 0x800000, 0x200000, CRC(830a71bc) SHA1(884378e8a5afeb819daf5285d0d205986d566340) )
	ROM_LOAD32_WORD( "mpr-14895.31", 0x800002, 0x200000, CRC(af027ac5) SHA1(523f03d90358ddb7d0e96fd06b9a65cebfc09f24) )
	ROM_LOAD32_WORD( "mpr-14896.32", 0xc00000, 0x200000, CRC(382091dc) SHA1(efa266f0f6bfe36ad1c365e588fff33b01e166dd) )
	ROM_LOAD32_WORD( "mpr-14897.33", 0xc00002, 0x200000, CRC(74873195) SHA1(80705ec577d14570f9bba77cc26766f831c41f42) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-14898.39", 0x000000, 0x80000, CRC(61da2bb6) SHA1(7a12ba522d64a1aeec1ca6f5a87ee063692131f9) )
	ROM_LOAD32_BYTE( "mpr-14899.40", 0x000001, 0x80000, CRC(2cd58bee) SHA1(73defec823de4244a387af55fea7210edc1b314f) )
	ROM_LOAD32_BYTE( "mpr-14900.41", 0x000002, 0x80000, CRC(aa7c017d) SHA1(0fa2b59a8bb5f5907b2b2567e69d11c73b398dc1) )
	ROM_LOAD32_BYTE( "mpr-14901.42", 0x000003, 0x80000, CRC(175b7a9a) SHA1(c86602e771cd49bab425b4ba7926d2f44858bd39) )

ROM_END

ROM_START( swa )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16669.14", 0x200000, 0x80000, CRC(52e5e7e4) SHA1(0eb584299d821a61ed2c27c615356eaeb18b41c6) )
	ROM_LOAD16_BYTE( "epr-16670.15", 0x200001, 0x80000, CRC(1e7ecabd) SHA1(62e8dd5d3a053426a1f3d94d15b04621d36d6a2c) )

	ROM_LOAD( "epr-16668.5", 0xf80000, 0x80000, CRC(9e112425) SHA1(fc7be83b33b586780444e3426164e1cb5125f794) )
	ROM_RELOAD(              0x000000, 0x80000 )
	ROM_RELOAD(              0x080000, 0x80000 )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-16470.7", 0x000000, 0x020000, CRC(7da18cf7) SHA1(bd432d882d217277faee120e2577357a32eb4a6e) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16486.32", 0x000000, 0x200000, CRC(7df50533) SHA1(f2fb876738e37d70eb9005e5629a9ae89aa413a8) )
	ROM_LOAD( "mpr-16487.33", 0x200000, 0x200000, CRC(31b28dfa) SHA1(bd1ac11bf2f9161f61f8af3b9ff4c2709b7ee700) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16484.4", 0x000000, 0x200000, CRC(9d4c334d) SHA1(8b4d903f14559fed425d225bb23ccfe8da23cbd3) )
	ROM_LOAD( "mpr-16485.5", 0x200000, 0x200000, CRC(95aadcad) SHA1(4276db655db9834692c3843eb96a3e3a89cb7252) )

	ROM_REGION( 0x20000, REGION_CPU4, 0 ) /* Z80 DSB code */
	ROM_LOAD( "epr-16471.2", 0x000000, 0x020000, CRC(f4ee84a4) SHA1(f12b214e6f195b0e5f49ba9f41d8e54bfcea9acc) )

	ROM_REGION( 0x800000, REGION_SOUND3, 0 ) /* DSB MPEG data */
	ROM_LOAD( "mpr-16514.57", 0x000000, 0x200000, CRC(3175b0be) SHA1(63649d053c8c17ce1746d16d0cc8202be20c302f) )
	ROM_LOAD( "mpr-16515.58", 0x200000, 0x200000, CRC(3114d748) SHA1(9ef090623cdd2a1d06b5d1bc4b9a07ab4eff5b76) )

	ROM_REGION32_LE( 0xc00000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16476.26", 0x000000, 0x200000, CRC(d48609ae) SHA1(8c8686a5c9ca4837447a7f70ed194e2f1882b66d) )
	ROM_LOAD32_WORD( "mpr-16477.27", 0x000002, 0x200000, CRC(b979b082) SHA1(0c60d259093e987f3856730b57b43bde7e9562e3) )
	ROM_LOAD32_WORD( "mpr-16478.28", 0x400000, 0x200000, CRC(80c780f7) SHA1(2f57c5373b02765d302bcd81e24f7b7bc4181387) )
	ROM_LOAD32_WORD( "mpr-16479.29", 0x400002, 0x200000, CRC(e43183b3) SHA1(4e62c67cdf7a6fdac0ded86d5f9e81044b9dea8d) )
	ROM_LOAD32_WORD( "mpr-16480.30", 0x800000, 0x200000, CRC(3185547a) SHA1(9871937372c2c755717802117a3ad39e1a11410e) )
	ROM_LOAD32_WORD( "mpr-16481.31", 0x800002, 0x200000, CRC(ce8d76fe) SHA1(0406f0500d19d6707515627b4143f92a9a5db769) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16472.39", 0x000000, 0x80000, CRC(5a0d7553) SHA1(ba8e08e5a0c6b7fbc10084ad7ad3edf61efb0d70) )
	ROM_LOAD32_BYTE( "mpr-16473.40", 0x000001, 0x80000, CRC(876c5399) SHA1(be7e40c77a385600941f11c24852cd73c71696f0) )
	ROM_LOAD32_BYTE( "mpr-16474.41", 0x000002, 0x80000, CRC(5864a26f) SHA1(be0c22dfff37408f6b401b1970f7fcc6fc7fbcd2) )
	ROM_LOAD32_BYTE( "mpr-16475.42", 0x000003, 0x80000, CRC(b9266be9) SHA1(cf195cd89c9d191b9eb8c5299f8cc154c2b4bd82) )
ROM_END

ROM_START( swaj )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16468.14", 0x200000, 0x80000, CRC(681d03c0) SHA1(4d21e26ce211466d429b84bca69a8147ff31ec6c) )
	ROM_LOAD16_BYTE( "epr-16469.15", 0x200001, 0x80000, CRC(6f281f7c) SHA1(6a9179e48d14838bb2a1a3f63fdd3a68ed009e03) )

	ROM_LOAD( "epr-16467.5", 0xf80000, 0x80000, CRC(605068f5) SHA1(99d7e171ce3353477c282d7567dedb9947206f14) )
	ROM_RELOAD(              0x000000, 0x80000 )
	ROM_RELOAD(              0x080000, 0x80000 )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-16470.7", 0x000000, 0x020000, CRC(7da18cf7) SHA1(bd432d882d217277faee120e2577357a32eb4a6e) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16486.32", 0x000000, 0x200000, CRC(7df50533) SHA1(f2fb876738e37d70eb9005e5629a9ae89aa413a8) )
	ROM_LOAD( "mpr-16487.33", 0x200000, 0x200000, CRC(31b28dfa) SHA1(bd1ac11bf2f9161f61f8af3b9ff4c2709b7ee700) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16484.4", 0x000000, 0x200000, CRC(9d4c334d) SHA1(8b4d903f14559fed425d225bb23ccfe8da23cbd3) )
	ROM_LOAD( "mpr-16485.5", 0x200000, 0x200000, CRC(95aadcad) SHA1(4276db655db9834692c3843eb96a3e3a89cb7252) )

	ROM_REGION( 0x20000, REGION_CPU4, 0 ) /* Z80 DSB code */
	ROM_LOAD( "epr-16471.2", 0x000000, 0x020000, CRC(f4ee84a4) SHA1(f12b214e6f195b0e5f49ba9f41d8e54bfcea9acc) )

	ROM_REGION( 0x400000, REGION_SOUND3, 0 ) /* DSB MPEG data */
	ROM_LOAD( "mpr-16514.57", 0x000000, 0x200000, CRC(3175b0be) SHA1(63649d053c8c17ce1746d16d0cc8202be20c302f) )
	ROM_LOAD( "mpr-16515.58", 0x200000, 0x200000, CRC(3114d748) SHA1(9ef090623cdd2a1d06b5d1bc4b9a07ab4eff5b76) )

	ROM_REGION32_LE( 0xc00000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16476.26", 0x000000, 0x200000, CRC(d48609ae) SHA1(8c8686a5c9ca4837447a7f70ed194e2f1882b66d) )
	ROM_LOAD32_WORD( "mpr-16477.27", 0x000002, 0x200000, CRC(b979b082) SHA1(0c60d259093e987f3856730b57b43bde7e9562e3) )
	ROM_LOAD32_WORD( "mpr-16478.28", 0x400000, 0x200000, CRC(80c780f7) SHA1(2f57c5373b02765d302bcd81e24f7b7bc4181387) )
	ROM_LOAD32_WORD( "mpr-16479.29", 0x400002, 0x200000, CRC(e43183b3) SHA1(4e62c67cdf7a6fdac0ded86d5f9e81044b9dea8d) )
	ROM_LOAD32_WORD( "mpr-16480.30", 0x800000, 0x200000, CRC(3185547a) SHA1(9871937372c2c755717802117a3ad39e1a11410e) )
	ROM_LOAD32_WORD( "mpr-16481.31", 0x800002, 0x200000, CRC(ce8d76fe) SHA1(0406f0500d19d6707515627b4143f92a9a5db769) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16472.39", 0x000000, 0x80000, CRC(5a0d7553) SHA1(ba8e08e5a0c6b7fbc10084ad7ad3edf61efb0d70) )
	ROM_LOAD32_BYTE( "mpr-16473.40", 0x000001, 0x80000, CRC(876c5399) SHA1(be7e40c77a385600941f11c24852cd73c71696f0) )
	ROM_LOAD32_BYTE( "mpr-16474.41", 0x000002, 0x80000, CRC(5864a26f) SHA1(be0c22dfff37408f6b401b1970f7fcc6fc7fbcd2) )
	ROM_LOAD32_BYTE( "mpr-16475.42", 0x000003, 0x80000, CRC(b9266be9) SHA1(cf195cd89c9d191b9eb8c5299f8cc154c2b4bd82) )
ROM_END

ROM_START( wingwar )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16729.14", 0x200000, 0x80000, CRC(7edec2cc) SHA1(3e423a868ca7c8475fbb5bc1a10526e69d94d865) )
	ROM_LOAD16_BYTE( "epr-16730.15", 0x200001, 0x80000, CRC(bab24dee) SHA1(26c95139c1aa7f34b6a5cce39e5bd1dd2ef0dd49) )

	ROM_LOAD( "epr-16953.4", 0xfc0000, 0x20000, CRC(c821a920) SHA1(7fc9ea5d828aac664514fa6d38f708f1ffd26220) )
	ROM_RELOAD(              0x000000, 0x20000 )
	ROM_LOAD( "epr-16952.5", 0xfe0000, 0x20000, CRC(03a3ecc5) SHA1(5c4aa221302b0a0800e1af99a41ab46fe4325184) )
	ROM_RELOAD(              0x020000, 0x20000 )

	ROM_LOAD16_BYTE( "mpr-16738.6",  0x1000000, 0x80000, CRC(51518ffa) SHA1(e4674ddfed4205957b14e133c6fdf6454872f324) )
	ROM_LOAD16_BYTE( "mpr-16737.7",  0x1000001, 0x80000, CRC(37b1379c) SHA1(98620c324268e1dd906c077ac8a8cd903b9de1f7) )
	ROM_LOAD16_BYTE( "mpr-16736.8",  0x1100000, 0x80000, CRC(10b6a025) SHA1(7a4f624ceb7c0b92044a5db8ff55440562ef836b) )
	ROM_LOAD16_BYTE( "mpr-16735.9",  0x1100001, 0x80000, CRC(c82fd198) SHA1(d9e53ae1e14dfc8e84a14c0026ef0b904863bb1b) )
	ROM_LOAD16_BYTE( "mpr-16734.10", 0x1200000, 0x80000, CRC(f76371c1) SHA1(0ff082db3877383d0dd977dc60c932b725e3d164) )
	ROM_LOAD16_BYTE( "mpr-16733.11", 0x1200001, 0x80000, CRC(e105847b) SHA1(8489a6c91fd6d1e9ba81e8eaf36c514da30dccbe) )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-17126.7", 0x000000, 0x20000, CRC(50178e40) SHA1(fb01aecfbe4e90adc997de0d45a63c16ef353b37) )
	ROM_LOAD16_WORD_SWAP( "epr-16752.8", 0x020000, 0x20000, CRC(6541c48f) SHA1(9341eff160e31a8574b9545fafc1c4059323fa0c) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16753.32", 0x000000, 0x200000, CRC(324a8333) SHA1(960342e08db637c6f72615d49cffd9fb0889620b) )
	ROM_LOAD( "mpr-16754.33", 0x200000, 0x200000, CRC(144f3cf5) SHA1(d2f8cc9086affbbc5ef2195272200230f724e5d1) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16755.4", 0x000000, 0x200000, CRC(4baaf878) SHA1(661d4ea9be6a4952852d0ef94becee7ed42bf4a1) )
	ROM_LOAD( "mpr-16756.5", 0x200000, 0x200000, CRC(d9c40672) SHA1(83e6f1156b30888d3a00103f079dc74f4fca8446) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16743.26", 0x000000, 0x200000, CRC(a710d33c) SHA1(1d0184545b34789ed511caaa25d57db3cd9a8e2f) )
	ROM_LOAD32_WORD( "mpr-16744.27", 0x000002, 0x200000, CRC(de796e1f) SHA1(397efb86a21b178770f29d2464bacf5f893619a0) )
	ROM_LOAD32_WORD( "mpr-16745.28", 0x400000, 0x200000, CRC(905b689c) SHA1(685dec2a65d5b3a386bda0af1bb5ae7e2b73a01a) )
	ROM_LOAD32_WORD( "mpr-16746.29", 0x400002, 0x200000, CRC(163b312e) SHA1(6b45007d6a9d17c8a0b46d81ec84ce9bfefb1695) )
	ROM_LOAD32_WORD( "mpr-16747.30", 0x800000, 0x200000, CRC(7353bb12) SHA1(608c5d561e909b8ba31d53db18e6e199855eaaec) )
	ROM_LOAD32_WORD( "mpr-16748.31", 0x800002, 0x200000, CRC(8ce98d3a) SHA1(1978776a0e2ea817508e30ba232d5f8d9c158f70) )
	ROM_LOAD32_WORD( "mpr-16749.32", 0xc00000, 0x200000, CRC(0e36dc1a) SHA1(4939177a6ac51ca57d0acd118ff14af4f4e438bb) )
	ROM_LOAD32_WORD( "mpr-16750.33", 0xc00002, 0x200000, CRC(e4f0b98d) SHA1(e69de2e5ccea2834fb8326bdd61fc6b517bc60b7) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16741.39", 0x000000, 0x80000, CRC(84b2ffd8) SHA1(0eba3855d20b88567c6fa08046e12429664d87cb) )
	ROM_LOAD32_BYTE( "mpr-16742.40", 0x000001, 0x80000, CRC(e9cc12bb) SHA1(40c83c968be3b11fad193a00e7b760f074450683) )
	ROM_LOAD32_BYTE( "mpr-16739.41", 0x000002, 0x80000, CRC(6c73e98f) SHA1(7b31e62922ab6d0df97c3ecc52b78e6d086c8635) )
	ROM_LOAD32_BYTE( "mpr-16740.42", 0x000003, 0x80000, CRC(44b31007) SHA1(4bb265fea25a7bbcbb8ab080fdcf09849b18f1de) )
ROM_END

ROM_START( wingwaru )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16729.14", 0x200000, 0x80000, CRC(7edec2cc) SHA1(3e423a868ca7c8475fbb5bc1a10526e69d94d865) )
	ROM_LOAD16_BYTE( "epr-16730.15", 0x200001, 0x80000, CRC(bab24dee) SHA1(26c95139c1aa7f34b6a5cce39e5bd1dd2ef0dd49) )

	ROM_LOAD( "epr-16951.4", 0xfc0000, 0x20000, CRC(fbe12d15) SHA1(07e572d20babc47c14b20af51c567c36b1b9c7a9) )
	ROM_RELOAD(              0x000000, 0x20000 )
	ROM_LOAD( "epr-16950.5", 0xfe0000, 0x20000, CRC(841e2195) SHA1(66f465aaf71955496e6f83335f7b836ad1d8c724) )
	ROM_RELOAD(              0x020000, 0x20000 )

	ROM_LOAD16_BYTE( "mpr-16738.6",  0x1000000, 0x80000, CRC(51518ffa) SHA1(e4674ddfed4205957b14e133c6fdf6454872f324) )
	ROM_LOAD16_BYTE( "mpr-16737.7",  0x1000001, 0x80000, CRC(37b1379c) SHA1(98620c324268e1dd906c077ac8a8cd903b9de1f7) )
	ROM_LOAD16_BYTE( "mpr-16736.8",  0x1100000, 0x80000, CRC(10b6a025) SHA1(7a4f624ceb7c0b92044a5db8ff55440562ef836b) )
	ROM_LOAD16_BYTE( "mpr-16735.9",  0x1100001, 0x80000, CRC(c82fd198) SHA1(d9e53ae1e14dfc8e84a14c0026ef0b904863bb1b) )
	ROM_LOAD16_BYTE( "mpr-16734.10", 0x1200000, 0x80000, CRC(f76371c1) SHA1(0ff082db3877383d0dd977dc60c932b725e3d164) )
	ROM_LOAD16_BYTE( "mpr-16733.11", 0x1200001, 0x80000, CRC(e105847b) SHA1(8489a6c91fd6d1e9ba81e8eaf36c514da30dccbe) )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-16751.7", 0x000000, 0x20000, CRC(23ba5ebc) SHA1(b98aab546c5e980baeedbada4e7472eb4c588260) )
	ROM_LOAD16_WORD_SWAP( "epr-16752.8", 0x020000, 0x20000, CRC(6541c48f) SHA1(9341eff160e31a8574b9545fafc1c4059323fa0c) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16753.32", 0x000000, 0x200000, CRC(324a8333) SHA1(960342e08db637c6f72615d49cffd9fb0889620b) )
	ROM_LOAD( "mpr-16754.33", 0x200000, 0x200000, CRC(144f3cf5) SHA1(d2f8cc9086affbbc5ef2195272200230f724e5d1) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16755.4", 0x000000, 0x200000, CRC(4baaf878) SHA1(661d4ea9be6a4952852d0ef94becee7ed42bf4a1) )
	ROM_LOAD( "mpr-16756.5", 0x200000, 0x200000, CRC(d9c40672) SHA1(83e6f1156b30888d3a00103f079dc74f4fca8446) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16743.26", 0x000000, 0x200000, CRC(a710d33c) SHA1(1d0184545b34789ed511caaa25d57db3cd9a8e2f) )
	ROM_LOAD32_WORD( "mpr-16744.27", 0x000002, 0x200000, CRC(de796e1f) SHA1(397efb86a21b178770f29d2464bacf5f893619a0) )
	ROM_LOAD32_WORD( "mpr-16745.28", 0x400000, 0x200000, CRC(905b689c) SHA1(685dec2a65d5b3a386bda0af1bb5ae7e2b73a01a) )
	ROM_LOAD32_WORD( "mpr-16746.29", 0x400002, 0x200000, CRC(163b312e) SHA1(6b45007d6a9d17c8a0b46d81ec84ce9bfefb1695) )
	ROM_LOAD32_WORD( "mpr-16747.30", 0x800000, 0x200000, CRC(7353bb12) SHA1(608c5d561e909b8ba31d53db18e6e199855eaaec) )
	ROM_LOAD32_WORD( "mpr-16748.31", 0x800002, 0x200000, CRC(8ce98d3a) SHA1(1978776a0e2ea817508e30ba232d5f8d9c158f70) )
	ROM_LOAD32_WORD( "mpr-16749.32", 0xc00000, 0x200000, CRC(0e36dc1a) SHA1(4939177a6ac51ca57d0acd118ff14af4f4e438bb) )
	ROM_LOAD32_WORD( "mpr-16750.33", 0xc00002, 0x200000, CRC(e4f0b98d) SHA1(e69de2e5ccea2834fb8326bdd61fc6b517bc60b7) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16741.39", 0x000000, 0x80000, CRC(84b2ffd8) SHA1(0eba3855d20b88567c6fa08046e12429664d87cb) )
	ROM_LOAD32_BYTE( "mpr-16742.40", 0x000001, 0x80000, CRC(e9cc12bb) SHA1(40c83c968be3b11fad193a00e7b760f074450683) )
	ROM_LOAD32_BYTE( "mpr-16739.41", 0x000002, 0x80000, CRC(6c73e98f) SHA1(7b31e62922ab6d0df97c3ecc52b78e6d086c8635) )
	ROM_LOAD32_BYTE( "mpr-16740.42", 0x000003, 0x80000, CRC(44b31007) SHA1(4bb265fea25a7bbcbb8ab080fdcf09849b18f1de) )
ROM_END

ROM_START( wingwarj )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16729.14", 0x200000, 0x80000, CRC(7edec2cc) SHA1(3e423a868ca7c8475fbb5bc1a10526e69d94d865) )
	ROM_LOAD16_BYTE( "epr-16730.15", 0x200001, 0x80000, CRC(bab24dee) SHA1(26c95139c1aa7f34b6a5cce39e5bd1dd2ef0dd49) )

	ROM_LOAD( "epr-16728.4", 0xfc0000, 0x20000, CRC(194d58ff) SHA1(281201cbb9bb148a06fe88edeccf680f837dcc10) )
	ROM_RELOAD(              0x000000, 0x20000 )
	ROM_LOAD( "epr-16727.5", 0xfe0000, 0x20000, CRC(430380eb) SHA1(25bd81a69a9b4cdc644f912283b65b3eab988327) )
	ROM_RELOAD(              0x020000, 0x20000 )

	ROM_LOAD16_BYTE( "mpr-16738.6",  0x1000000, 0x80000, CRC(51518ffa) SHA1(e4674ddfed4205957b14e133c6fdf6454872f324) )
	ROM_LOAD16_BYTE( "mpr-16737.7",  0x1000001, 0x80000, CRC(37b1379c) SHA1(98620c324268e1dd906c077ac8a8cd903b9de1f7) )
	ROM_LOAD16_BYTE( "mpr-16736.8",  0x1100000, 0x80000, CRC(10b6a025) SHA1(7a4f624ceb7c0b92044a5db8ff55440562ef836b) )
	ROM_LOAD16_BYTE( "mpr-16735.9",  0x1100001, 0x80000, CRC(c82fd198) SHA1(d9e53ae1e14dfc8e84a14c0026ef0b904863bb1b) )
	ROM_LOAD16_BYTE( "mpr-16734.10", 0x1200000, 0x80000, CRC(f76371c1) SHA1(0ff082db3877383d0dd977dc60c932b725e3d164) )
	ROM_LOAD16_BYTE( "mpr-16733.11", 0x1200001, 0x80000, CRC(e105847b) SHA1(8489a6c91fd6d1e9ba81e8eaf36c514da30dccbe) )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-16751.7", 0x000000, 0x20000, CRC(23ba5ebc) SHA1(b98aab546c5e980baeedbada4e7472eb4c588260) )
	ROM_LOAD16_WORD_SWAP( "epr-16752.8", 0x020000, 0x20000, CRC(6541c48f) SHA1(9341eff160e31a8574b9545fafc1c4059323fa0c) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16753.32", 0x000000, 0x200000, CRC(324a8333) SHA1(960342e08db637c6f72615d49cffd9fb0889620b) )
	ROM_LOAD( "mpr-16754.33", 0x200000, 0x200000, CRC(144f3cf5) SHA1(d2f8cc9086affbbc5ef2195272200230f724e5d1) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16755.4", 0x000000, 0x200000, CRC(4baaf878) SHA1(661d4ea9be6a4952852d0ef94becee7ed42bf4a1) )
	ROM_LOAD( "mpr-16756.5", 0x200000, 0x200000, CRC(d9c40672) SHA1(83e6f1156b30888d3a00103f079dc74f4fca8446) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16743.26", 0x000000, 0x200000, CRC(a710d33c) SHA1(1d0184545b34789ed511caaa25d57db3cd9a8e2f) )
	ROM_LOAD32_WORD( "mpr-16744.27", 0x000002, 0x200000, CRC(de796e1f) SHA1(397efb86a21b178770f29d2464bacf5f893619a0) )
	ROM_LOAD32_WORD( "mpr-16745.28", 0x400000, 0x200000, CRC(905b689c) SHA1(685dec2a65d5b3a386bda0af1bb5ae7e2b73a01a) )
	ROM_LOAD32_WORD( "mpr-16746.29", 0x400002, 0x200000, CRC(163b312e) SHA1(6b45007d6a9d17c8a0b46d81ec84ce9bfefb1695) )
	ROM_LOAD32_WORD( "mpr-16747.30", 0x800000, 0x200000, CRC(7353bb12) SHA1(608c5d561e909b8ba31d53db18e6e199855eaaec) )
	ROM_LOAD32_WORD( "mpr-16748.31", 0x800002, 0x200000, CRC(8ce98d3a) SHA1(1978776a0e2ea817508e30ba232d5f8d9c158f70) )
	ROM_LOAD32_WORD( "mpr-16749.32", 0xc00000, 0x200000, CRC(0e36dc1a) SHA1(4939177a6ac51ca57d0acd118ff14af4f4e438bb) )
	ROM_LOAD32_WORD( "mpr-16750.33", 0xc00002, 0x200000, CRC(e4f0b98d) SHA1(e69de2e5ccea2834fb8326bdd61fc6b517bc60b7) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16741.39", 0x000000, 0x80000, CRC(84b2ffd8) SHA1(0eba3855d20b88567c6fa08046e12429664d87cb) )
	ROM_LOAD32_BYTE( "mpr-16742.40", 0x000001, 0x80000, CRC(e9cc12bb) SHA1(40c83c968be3b11fad193a00e7b760f074450683) )
	ROM_LOAD32_BYTE( "mpr-16739.41", 0x000002, 0x80000, CRC(6c73e98f) SHA1(7b31e62922ab6d0df97c3ecc52b78e6d086c8635) )
	ROM_LOAD32_BYTE( "mpr-16740.42", 0x000003, 0x80000, CRC(44b31007) SHA1(4bb265fea25a7bbcbb8ab080fdcf09849b18f1de) )
ROM_END

/* Dumper's note: As per other R360 games wingwar is just a slightly modified version with 4 roms different on the rom board. */

ROM_START( wingwar360 )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16729.14", 0x200000, 0x80000, CRC(7edec2cc) SHA1(3e423a868ca7c8475fbb5bc1a10526e69d94d865) )
	ROM_LOAD16_BYTE( "epr-16730.15", 0x200001, 0x80000, CRC(bab24dee) SHA1(26c95139c1aa7f34b6a5cce39e5bd1dd2ef0dd49) )

	ROM_LOAD( "epr-17052.4", 0xfc0000, 0x20000, CRC(0f4743e7) SHA1(cc47fd1d25808728ed05d95d510733b8bd011b41) )
	ROM_RELOAD(              0x000000, 0x20000 )
	ROM_LOAD( "epr-17053.5", 0xfe0000, 0x20000, CRC(83af2415) SHA1(46dfee9db95171a3942cd32c851ec75c3d9e03da) )
	ROM_RELOAD(              0x020000, 0x20000 )

	ROM_LOAD16_BYTE( "epr-17056.6",  0x1000000, 0x80000, CRC(5216de4d) SHA1(1463311d3f96ca7c46b8f676ee3963caddeec9e2) )
	ROM_LOAD16_BYTE( "epr-17057.7",  0x1000001, 0x80000, CRC(b21aa2db) SHA1(861e637b5ae9b2d7fe6d09a7e788a6a3ff09af6e) )
	ROM_LOAD16_BYTE( "mpr-16736.8",  0x1100000, 0x80000, CRC(10b6a025) SHA1(7a4f624ceb7c0b92044a5db8ff55440562ef836b) )
	ROM_LOAD16_BYTE( "mpr-16735.9",  0x1100001, 0x80000, CRC(c82fd198) SHA1(d9e53ae1e14dfc8e84a14c0026ef0b904863bb1b) )
	ROM_LOAD16_BYTE( "mpr-16734.10", 0x1200000, 0x80000, CRC(f76371c1) SHA1(0ff082db3877383d0dd977dc60c932b725e3d164) )
	ROM_LOAD16_BYTE( "mpr-16733.11", 0x1200001, 0x80000, CRC(e105847b) SHA1(8489a6c91fd6d1e9ba81e8eaf36c514da30dccbe) )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-17126.7", 0x000000, 0x20000, CRC(50178e40) SHA1(fb01aecfbe4e90adc997de0d45a63c16ef353b37) )
	ROM_LOAD16_WORD_SWAP( "epr-16752.8", 0x020000, 0x20000, CRC(6541c48f) SHA1(9341eff160e31a8574b9545fafc1c4059323fa0c) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16753.32", 0x000000, 0x200000, CRC(324a8333) SHA1(960342e08db637c6f72615d49cffd9fb0889620b) )
	ROM_LOAD( "mpr-16754.33", 0x200000, 0x200000, CRC(144f3cf5) SHA1(d2f8cc9086affbbc5ef2195272200230f724e5d1) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16755.4", 0x000000, 0x200000, CRC(4baaf878) SHA1(661d4ea9be6a4952852d0ef94becee7ed42bf4a1) )
	ROM_LOAD( "mpr-16756.5", 0x200000, 0x200000, CRC(d9c40672) SHA1(83e6f1156b30888d3a00103f079dc74f4fca8446) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16743.26", 0x000000, 0x200000, CRC(a710d33c) SHA1(1d0184545b34789ed511caaa25d57db3cd9a8e2f) )
	ROM_LOAD32_WORD( "mpr-16744.27", 0x000002, 0x200000, CRC(de796e1f) SHA1(397efb86a21b178770f29d2464bacf5f893619a0) )
	ROM_LOAD32_WORD( "mpr-16745.28", 0x400000, 0x200000, CRC(905b689c) SHA1(685dec2a65d5b3a386bda0af1bb5ae7e2b73a01a) )
	ROM_LOAD32_WORD( "mpr-16746.29", 0x400002, 0x200000, CRC(163b312e) SHA1(6b45007d6a9d17c8a0b46d81ec84ce9bfefb1695) )
	ROM_LOAD32_WORD( "mpr-16747.30", 0x800000, 0x200000, CRC(7353bb12) SHA1(608c5d561e909b8ba31d53db18e6e199855eaaec) )
	ROM_LOAD32_WORD( "mpr-16748.31", 0x800002, 0x200000, CRC(8ce98d3a) SHA1(1978776a0e2ea817508e30ba232d5f8d9c158f70) )
	ROM_LOAD32_WORD( "mpr-16749.32", 0xc00000, 0x200000, CRC(0e36dc1a) SHA1(4939177a6ac51ca57d0acd118ff14af4f4e438bb) )
	ROM_LOAD32_WORD( "mpr-16750.33", 0xc00002, 0x200000, CRC(e4f0b98d) SHA1(e69de2e5ccea2834fb8326bdd61fc6b517bc60b7) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16741.39", 0x000000, 0x80000, CRC(84b2ffd8) SHA1(0eba3855d20b88567c6fa08046e12429664d87cb) )
	ROM_LOAD32_BYTE( "mpr-16742.40", 0x000001, 0x80000, CRC(e9cc12bb) SHA1(40c83c968be3b11fad193a00e7b760f074450683) )
	ROM_LOAD32_BYTE( "mpr-16739.41", 0x000002, 0x80000, CRC(6c73e98f) SHA1(7b31e62922ab6d0df97c3ecc52b78e6d086c8635) )
	ROM_LOAD32_BYTE( "mpr-16740.42", 0x000003, 0x80000, CRC(44b31007) SHA1(4bb265fea25a7bbcbb8ab080fdcf09849b18f1de) )

	/* Dumper's note: Video & Drive is the control board in the attendants' tower, same hardware as G-Loc R360 with the two program roms being the only difference. */
ROM_END

ROM_START( netmerc )
	MODEL1_CPU_BOARD

	ROM_REGION( 0x1400000, REGION_CPU1, ROMREGION_ERASEFF ) /* v60 code */
	ROM_LOAD( "epr-18120.ic5", 0xf80000, 0x80000, CRC(de489762) SHA1(4e935f11e844489c5b5dc06439dc7902c4fddc9f) ) /* Rom board with Sega ID# ??? */
	ROM_RELOAD(          0x000000, 0x80000 )

	ROM_LOAD16_BYTE( "epr-18122.ic6",  0x1000000, 0x80000, CRC(10581ab3) SHA1(e3f5ec4ce14a1e1a1134c8b516c371b7655f3544) )
	ROM_LOAD16_BYTE( "epr-18123.ic7",  0x1000001, 0x80000, CRC(27e0d848) SHA1(117f99efd0643ba92b3340171d8952b6a1a568d4) )
	ROM_LOAD16_BYTE( "epr-18124.ic8",  0x1100000, 0x80000, CRC(5a0e5618) SHA1(a092d05a414532aa27d4bae908b053da4255462a) )
	ROM_LOAD16_BYTE( "epr-18125.ic9",  0x1100001, 0x80000, CRC(bd47284f) SHA1(ae9325e6789bb956ab664c7ad9c48b80056f3916) )
	ROM_LOAD16_BYTE( "epr-18126.ic10", 0x1200000, 0x80000, CRC(43dc5ca8) SHA1(249ed83b90b0237ceabbed814cd42dc60dc7a174) )
	ROM_LOAD16_BYTE( "epr-18127.ic11", 0x1200001, 0x80000, CRC(d307a4ca) SHA1(5555235f740c1b09f6e1587d0fceb35b23d4a8a8) )

	ROM_REGION32_LE( 0x2000, REGION_USER4, 0 )
	ROM_LOAD( "315-5711.bin", 0, 0x2000, CRC(6a21f304) SHA1(d5c61ea6e4744f10170ea556068c248bd43bb111) BAD_DUMP)

	ROM_REGION( 0xc0000, REGION_CPU3, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-18121.ic7", 0x00000, 0x80000, CRC(113285b5) SHA1(5d060cee41e8d6a4a918f890c2d169d87dbcad79) )
	ROM_RELOAD( 0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-18134.ic32", 0x000000, 0x200000, BAD_DUMP CRC(9a4109e5) SHA1(ba59caac5f5a80fc52c507d8a47f322a380aa9a1) )
	ROM_LOAD( "mpr-18135.ic33", 0x200000, 0x200000, CRC(291d17e4) SHA1(7465d5bde9ece5112932efa58b3c1c07f2b11543) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-18136.ic4", 0x000000, 0x200000, CRC(2a9c0aaf) SHA1(e8c52a6032ae968afcc41b3d8bfd15065057ea4a) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-18128.ic26", 0x000000, 0x200000, CRC(70d8028c) SHA1(6e55028be778cd246f67e9d8cd69b9f5366bc429) )
	ROM_LOAD32_WORD( "mpr-18129.ic27", 0x000002, 0x200000, CRC(134feea3) SHA1(afaf9fa08c5b0ab8c9d5fdef78d829e8582cc61a) )
	ROM_LOAD32_WORD( "mpr-18130.ic28", 0x400000, 0x200000, CRC(c7aeae3e) SHA1(4f636178f903cd03e1b9de73a11d1105b7cdb084) )
	ROM_LOAD32_WORD( "mpr-18131.ic29", 0x400002, 0x200000, CRC(32683b33) SHA1(daf808853d9d85dff87eba5c081f47b4af4ae0a2) )
	ROM_LOAD32_WORD( "mpr-18132.ic30", 0x800000, 0x200000, CRC(a17e3ac2) SHA1(19827c06ebc3e9de63668ef07675224e169d853e) )
	ROM_LOAD32_WORD( "mpr-18133.ic31", 0x800002, 0x200000, CRC(f56354dd) SHA1(2ef1fe8b4995a67b70b565adf8f0ea0ad6e10094) )

	ROM_REGION32_LE( 0x200000, REGION_USER3, ROMREGION_ERASE00 ) /* IC39-IC42 unpopulated */

ROM_END


/*    YEAR  NAME        PARENT   MACHINE  INPUT       INIT        MONITOR  COMPANY FULLNAME */
GAMEX( 1993, vf,         0,       model1,  vf,         vf,         ROT0,    "Sega", "Virtua Fighter", 0 )
GAMEX( 1992, vr,         0,       model1,  vr,         vr,         ROT0,    "Sega", "Virtua Racing", 0 )
GAMEX( 1993, vformula,   vr,      model1,  vr,         vr,         ROT0,    "Sega", "Virtua Formula", 0 )
GAMEX( 1994, swa,        0,       swa,     swa,        swa,        ROT0,    "Sega", "Star Wars Arcade (US)", 0 )
GAMEX( 1994, swaj,       swa,     swa,     swa,        swa,        ROT0,    "Sega", "Star Wars Arcade (Japan)", 0 )
GAMEX( 1994, wingwar,    0,       model1,  wingwar,    wingwar,    ROT0,    "Sega", "Wing War (World)", 0 )
GAMEX( 1994, wingwaru,   wingwar, model1,  wingwar,    wingwar,    ROT0,    "Sega", "Wing War (US)", 0 )
GAMEX( 1994, wingwarj,   wingwar, model1,  wingwar,    wingwar,    ROT0,    "Sega", "Wing War (Japan)", 0 )
GAMEX( 1994, wingwar360, wingwar, model1,  wingwar360, wingwar360, ROT0,    "Sega", "Wing War R360 (US)", 0 )
GAMEX( 1993, netmerc,    0,       model1,  netmerc,    netmerc,    ROT0,    "Sega", "Sega NetMerc", 0 )
