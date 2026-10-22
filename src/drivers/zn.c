/***************************************************************************

  Sony ZN1/ZN2 - Arcade PSX Hardware
  ==================================
  Driver by smf

  QSound emulation based on information from the cps1/cps2 driver, Amuse & Miguel Angel Horna
  Taito FX1a sound emulation based on information from the Taito F2 driver

  The ZN1/ZN2 boards are standard boards from Sony. Extra sound h/w is
  included on the rom board,

  The BIOS is protected in all the games that run on this hardware.
  During boot up the BIOS decrypts parts of itself into ram using
  a device connected to the PSX controller port. As this is not
  emulated yet you only get the boot up colours.


  Capcom ZN1 ( coh-1000 ) / ZN2 ( coh-3002c )
  -------------------------------------------

  The QSound hardware is different to cps2 as it uses an i/o port and
  nmi's instead of shared memory. The driver uses 8bit i/o addresses
  but the real ZN1 hardware may use 16bit i/o addresses as the code
  always accesses port 0xf100. The ZN2 code however seems to vary the
  top eight bits of the address, this may or may not be important but
  it is currently ignored.

  The Gallop Racer dump came from a Capcom ZN1 QSound board but there
  were no QSound program/sample roms & the game doesn't appear to use
  it. Tecmo went on to produce games on their own boards.

    Key
    ---
    CP01 - ZN-1 motherboard
    CP02 - Battle Arena Toshinden 2
    CP03 - Star Gladiator
    CP04 - Street Fighter EX / EX Plus
    CP05 - Gallop Racer
    CP06 - Rival Schools / Justice Gakuen
    CP07 - Plasma Sword / Star Gladiator 2
    CP08 - Street Fighter EX2
    CP09 - Tech Romancer / Kikaioh
    CP10 - ZN-2 motherboard
    CP11 - Tetris The Grand Master
    CP12 - Street Fighter EX2 Plus
    CP13 - Strider 2 / Strider Hiryu 2


  Taito FX1a
  ----------

  The extra sound h/w is the same as taito f2 ( z80-a + ym2610b ).

    Key
    ---
    TT02 - Super Football Champ
    TT06 - Magical Date


  Taito FX1b
  ----------

  The extra sound h/w isn't emulated and consists of an mn1020012a +
  zoom zsg-2 + tms57002dpha.

  mn1020012a is a 16bit panasonic mpu & the zoom zsg-2 is a sound generator
  lsi produced by zoom corp in 1995.

    Key
    ---
    TT04 - Ray Storm
    TT05 - Fighter's Impact Ace
    TT07 - G-Darius

  Beastorizer bootleg runs on a ray storm motherboard, but doesn't have fx1b sound h/w.

  PS Arcade 95 ( coh-1002e )
  --------------------------
Main Board: Sony ZN-1  (1-659-709-12  COH-1002E)
 Sub Board: Raizing RA9701 SUB

    Key
    ---
    ET01 - Main board
    ET02 - Beastorizer

TMP68HC00N-16 (Toshiba 68K 12MHz)
OSC: 12.0000MHz (Kyocera KX-01-1)
Mach211 (x2) Labled "Main_IF2" & "SUB_IF2"
ST M628032-15E1 (x2) Ram??
Yamaha YMF271-F
GAL16V8B
CAT702 103090-ET02 (the "ET02" is a printed sticker/label) ??


  Acclaim PSX ( coh-1000 )
  ------------------------

    Key
    ---
    AC01 - Motherboard
    AC02 - NBA Jam Extreme

Sound: Analog Devices ADSP-2181

  Video System
  ------------

    Key
    ---
    KN01 - Motherboard
    KN02 - Sonic Wings Limited

  Atlus ( coh-1001l )
  -------------------

    Key
    ---
    AT01 - Motherboard
    AT02 - Heaven's Gate

  Tecmo ( coh-1002m )
  -------------------

    Key
    ---
    MG01 - Motherboard
    MG02 - Gallop Racer 2
    MG05 - Dead or Alive ++
    MG09 - Tondemo Crisis

  Atari / Time Warner ( coh-1000w )
  ---------------------------------

    Key
    ---
    TW01 - Motherboard
    TW02 - Primal Rage II

  ***************************************************************************/

#define PROTECTION_HACK ( 1 )

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/mips/psx.h"
#include "cpu/z80/z80.h"
#include "sndhrdw/taitosnd.h"
#include "sndhrdw/taitozoom.h"
#include "cpu/mn10200/mn10200.h"
#include "includes/psx.h"
#include "state.h"
#include "sound/psx.h"
#include "machine/idectrl.h"
#include "cpu/adsp2100/adsp2100.h"

static int zn_bank_secsel;	/* the game ROM bank follows this register (Raizing) */
static UINT32 tecmo_bank;
static void tecmo_bank_update( void );

#if PROTECTION_HACK


/* decodetaito.c */
static unsigned char tt_protection_hack[] =
{
	/* unknown */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,

	/* first test, this byte is ignored */
	0xef,

	/* checked for explicitly ( skips red screen ) */
	0xd7, 0x5f, 0xb7, 0xea, 0x51, 0x15, 0x96, 0x02,
	0x0b, 0xf8, 0xf5, 0x54, 0xc5, 0xf9, 0x5d, 0x5d,
	0xea, 0x13, 0xd0, 0x63, 0x95, 0xd4, 0x9f, 0xee,
	0xeb, 0x3f, 0x94, 0x36, 0x42, 0x45, 0x81, 0xb0,
	0x80, 0xc4, 0x3f, 0x42, 0x39, 0x2a, 0xa4, 0x28,
	0x28, 0x20, 0xac, 0x00, 0x20, 0x8a, 0x28, 0x92,
	0xa8,

	/* unknown */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* second test, this byte is ignored */
	0x00,

	/* checks the result of a calculation ( skips blue screen ) */
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,

	/* default value */
	0x00,
};

static unsigned short n_sio0_ptr;
static unsigned char *p_sio0_ret;
static unsigned short n_sio0_ret_size;

#endif

static READ32_HANDLER( sio0_r )
{
	unsigned short n_return;

	n_return = 0;
	switch( offset )
	{
	case 0x00:
		/* data */
#if PROTECTION_HACK
		if( p_sio0_ret != NULL )
		{
			n_return = p_sio0_ret[ n_sio0_ptr ];
			if( n_sio0_ptr < n_sio0_ret_size - 1 )
			{
				n_sio0_ptr++;
			}
		}
#endif
		break;
	case 0x01:
		/* status */
#if PROTECTION_HACK
		if( p_sio0_ret != NULL )
		{
			n_return = 2;
		}
#endif
		break;
	case 0x02:
		/* mode */
		break;
	case 0x03:
		/* control */
		break;
	case 0x04:
		/* baud */
		break;
	}
	log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: sio0_r( %04x ) %04x\n", activecpu_get_pc(), offset, n_return );
	return n_return;
}

static WRITE32_HANDLER( sio0_w )
{
	int n_char;
	char s_char[ 5 ];

	s_char[ 0 ] = 0;
	switch( offset )
	{
	case 0x00:
		/* data */
		n_char = ( data & 0xff );
		if( n_char >= 0x20 && n_char <= 0x7f )
		{
			sprintf( s_char, " '%c'", n_char );
		}
		else
		{
			sprintf( s_char, " $%02x", n_char );
		}
		break;
	case 0x01:
		/* status */
		break;
	case 0x02:
		/* mode */
		break;
	case 0x03:
		/* control */
		break;
	case 0x04:
		/* baud */
		break;
	}
	log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: sio0_w( %04x, %08x )%s\n", activecpu_get_pc(), offset, data, s_char );
}

static UINT8 qsound_bank;

static void qsound_bank_update( void )
{
	cpu_setbank( 10, memory_region( REGION_CPU2 ) + 0x10000 + ( qsound_bank * 0x4000 ) );
}

static WRITE_HANDLER( qsound_bankswitch_w )
{
	qsound_bank = data & 0x0f;
	qsound_bank_update();
}

static MEMORY_READ32_START( zn_readmem )
	{ 0x00000000, 0x003fffff, MRA32_RAM },		/* ram */
	{ 0x1f000000, 0x1f3fffff, MRA32_BANK1 },	/* game rom */
	{ 0x1f800000, 0x1f8003ff, MRA32_BANK3 },	/* scratchpad */
	{ 0x1f801040, 0x1f80104f, sio0_r },
	{ 0x1f801810, 0x1f801817, psx_gpu_r },
	{ 0x1fb80000, 0x1fbbffff, MRA32_BANK6 },	/* country rom */
	{ 0x80000000, 0x803fffff, MRA32_BANK4 },	/* ram mirror */
	{ 0xa0000000, 0xa03fffff, MRA32_BANK5 },	/* ram mirror */
	{ 0xbfc00000, 0xbfc7ffff, MRA32_BANK7 },	/* bios */
MEMORY_END

static MEMORY_WRITE32_START( zn_writemem )
	{ 0x00000000, 0x003fffff, MWA32_RAM },		/* ram */
	{ 0x1f000000, 0x1f3fffff, MWA32_ROM },		/* game rom */
	{ 0x1f800000, 0x1f8003ff, MWA32_BANK3 },	/* scratchpad */
	{ 0x1f801040, 0x1f80104f, sio0_w },
	{ 0x1f801810, 0x1f801817, psx_gpu_w },
	{ 0x1fb80000, 0x1fbbffff, MWA32_ROM },		/* country rom */
	{ 0x80000000, 0x803fffff, MWA32_BANK4 },	/* ram mirror */
	{ 0xa0000000, 0xa03fffff, MWA32_BANK5 },	/* ram mirror */
	{ 0xbfc00000, 0xbfc7ffff, MWA32_ROM },		/* bios */
MEMORY_END

static MEMORY_READ_START( qsound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK10 },	/* banked (contains music data) */
	{ 0xd007, 0xd007, qsound_status_r },
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( qsound_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xd000, 0xd000, qsound_data_h_w },
	{ 0xd001, 0xd001, qsound_data_l_w },
	{ 0xd002, 0xd002, qsound_cmd_w },
	{ 0xd003, 0xd003, qsound_bankswitch_w },
	{ 0xf000, 0xffff, MWA_RAM },
MEMORY_END

static PORT_READ_START( qsound_readport )
	{ 0x00, 0x00, soundlatch_r },
PORT_END

static DRIVER_INIT( zn )
{
	zn_bank_secsel = 0;
	cpu_setbank( 1, memory_region( REGION_USER1 ) ); /* game rom */
	cpu_setbank( 3, memory_region( REGION_CPU1 ) + 0x0480000 ); /* scratch pad */
	cpu_setbank( 4, memory_region( REGION_CPU1 ) ); /* ram mirror */
	cpu_setbank( 5, memory_region( REGION_CPU1 ) ); /* ram mirror */
	cpu_setbank( 6, memory_region( REGION_USER2 ) ); /* capcom country rom */
	cpu_setbank( 7, memory_region( REGION_CPU1 ) + 0x0400000 ); /* bios */

#if PROTECTION_HACK
	if( strcmp( Machine->gamedrv->name, "ftimpcta" ) == 0 ||
		strcmp( Machine->gamedrv->name, "gdarius" ) == 0 ||
		strcmp( Machine->gamedrv->name, "gdarius2" ) == 0 ||
		strcmp( Machine->gamedrv->name, "mgcldate" ) == 0 ||
		strcmp( Machine->gamedrv->name, "psyforce" ) == 0 ||
		strcmp( Machine->gamedrv->name, "raystorm" ) == 0 ||
		strcmp( Machine->gamedrv->name, "sfchamp" ) == 0 ||
		strcmp( Machine->gamedrv->name, "beastrzb" ) == 0 )
	{
		p_sio0_ret = tt_protection_hack;
		n_sio0_ret_size = sizeof( tt_protection_hack );
	}
	else
	{
		p_sio0_ret = NULL;
		n_sio0_ret_size = 0;
	}
#endif
	psx_driver_init();
}

static MACHINE_INIT( zn )
{
#if PROTECTION_HACK
	n_sio0_ptr = 0;
#endif
	psx_machine_init();
}

static MACHINE_DRIVER_START( zn )
	/* basic machine hardware */
	MDRV_CPU_ADD(PSXCPU, 33868800) /* 33MHz ?? */
	MDRV_CPU_MEMORY(zn_readmem,zn_writemem)
	MDRV_CPU_VBLANK_INT(psx_vblank,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(0)

	MDRV_MACHINE_INIT(zn)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(640, 480)
	MDRV_VISIBLE_AREA(0, 479, 0, 479)
	MDRV_PALETTE_LENGTH(65536)

	MDRV_PALETTE_INIT(psx)
	MDRV_VIDEO_START(psx_type2_1024x1024)
	MDRV_VIDEO_UPDATE(psx)
	MDRV_VIDEO_STOP(psx)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
MACHINE_DRIVER_END

/***************************************************************************

  ZN-1 / ZN-2 main board, backported from MAME 0.135 (2009) zn.c, psx.c
  and znsec.c, for the Capcom games (COH-1000C, COH-1002C, COH-3002C)

  The main board reads the inputs, the DIP switches and the two CAT702
  security chips over the PSX serial port 0, one bit per clock. The PSX
  serial port of the shared PSX code works on whole bytes, so the board
  has its own copy of port 0 here, as MAME 0.135 had it, clocked bit by
  bit. Port 1 stays with the shared PSX code.

***************************************************************************/

/* CAT702 security chip (MAME 0.135 znsec.c)

  A serial magic latch: an 8-bit state, set to 0xfc when the chip is
  selected; each clock sends one bit of the state and, for an input bit
  of 0, transforms the state with a chip-dependent linear sbox. One chip
  is on the main board, one on the ROM board; their 8-byte dumps are in
  the ROM sets (REGION_USER4 and REGION_USER5). */

static struct
{
	const UINT8 *transform;
	UINT8 state;
	UINT8 bit;
} znsec[ 2 ];

/* the sbox xor mask for a select bit and an input bit */
static UINT8 znsec_sbox_coef( int chip, int sel, int bit )
{
	UINT8 r;
	if( !sel )
		return znsec[ chip ].transform[ bit ];
	r = znsec_sbox_coef( chip, ( sel - 1 ) & 7, ( bit - 1 ) & 7 );
	r = ( r << 1 ) | ( ( ( r >> 7 ) ^ ( r >> 6 ) ) & 1 );
	if( bit != 7 )
		return r;
	return r ^ znsec_sbox_coef( chip, sel, 0 );
}

static UINT8 znsec_apply_bit_sbox( int chip, UINT8 state, int sel )
{
	int i;
	UINT8 r = 0;
	for( i = 0; i < 8; i++ )
		if( state & ( 1 << i ) )
			r ^= znsec_sbox_coef( chip, sel, i );
	return r;
}

static UINT8 znsec_apply_sbox( UINT8 state, const UINT8 *sbox )
{
	int i;
	UINT8 r = 0;
	for( i = 0; i < 8; i++ )
		if( state & ( 1 << i ) )
			r ^= sbox[ i ];
	return r;
}

static void znsec_init( int chip, const UINT8 *transform )
{
	znsec[ chip ].transform = transform;
	znsec[ chip ].state = 0xfc;
	znsec[ chip ].bit = 0;
}

static void znsec_start( int chip )
{
	znsec[ chip ].state = 0xfc;
	znsec[ chip ].bit = 0;
}

static UINT8 znsec_step( int chip, UINT8 input )
{
	static const UINT8 initial_sbox[ 8 ] = { 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x7f };
	UINT8 res;

	if( znsec[ chip ].transform == NULL )
		return 1;

	if( znsec[ chip ].bit == 0 )
		znsec[ chip ].state = znsec_apply_sbox( znsec[ chip ].state, initial_sbox );

	res = ( znsec[ chip ].state >> znsec[ chip ].bit ) & 1;
	if( ( input & 1 ) == 0 )
		znsec[ chip ].state = znsec_apply_bit_sbox( chip, znsec[ chip ].state, znsec[ chip ].bit );

	znsec[ chip ].bit = ( znsec[ chip ].bit + 1 ) & 7;
	return res;
}

/* PSX serial port 0, bit by bit (MAME 0.135 psx.c) */

#define ZN_SIO_OUT_DATA  ( 1 )	/* COMMAND */
#define ZN_SIO_OUT_DTR   ( 2 )	/* ATT */
#define ZN_SIO_OUT_CLOCK ( 8 )	/* CLOCK */
#define ZN_SIO_IN_DATA   ( 1 )	/* DATA */
#define ZN_SIO_IN_DSR    ( 2 )	/* ACK */

#define SIO_STATUS_TX_RDY    ( 1 << 0 )
#define SIO_STATUS_RX_RDY    ( 1 << 1 )
#define SIO_STATUS_TX_EMPTY  ( 1 << 2 )
#define SIO_STATUS_OVERRUN   ( 1 << 4 )
#define SIO_STATUS_DSR       ( 1 << 7 )
#define SIO_STATUS_IRQ       ( 1 << 9 )

#define SIO_CONTROL_TX_ENA   ( 1 << 0 )
#define SIO_CONTROL_IACK     ( 1 << 4 )
#define SIO_CONTROL_RESET    ( 1 << 6 )
#define SIO_CONTROL_TX_IENA  ( 1 << 10 )
#define SIO_CONTROL_RX_IENA  ( 1 << 11 )
#define SIO_CONTROL_DSR_IENA ( 1 << 12 )
#define SIO_CONTROL_DTR      ( 1 << 13 )

typedef void ( *zn_sio_handler )( int n_data );

static UINT32 zn_sio_status, zn_sio_mode, zn_sio_control, zn_sio_baud;
static UINT32 zn_sio_tx, zn_sio_rx, zn_sio_tx_prev, zn_sio_rx_prev;
static UINT32 zn_sio_tx_data, zn_sio_rx_data;
static UINT32 zn_sio_tx_shift, zn_sio_rx_shift, zn_sio_tx_bits, zn_sio_rx_bits;
static mame_timer *zn_sio_timer;
static zn_sio_handler zn_sio_fn;

static void zn_sio_interrupt( void )
{
	zn_sio_status |= SIO_STATUS_IRQ;
	psx_irq_set( 0x80 );
}

static void zn_sio_timer_adjust( void )
{
	double n_time = TIME_NEVER;
	if( ( zn_sio_status & SIO_STATUS_TX_EMPTY ) == 0 || zn_sio_tx_bits != 0 )
	{
		int n_prescaler;
		switch( zn_sio_mode & 3 )
		{
		case 1: n_prescaler = 1; break;
		case 2: n_prescaler = 16; break;
		case 3: n_prescaler = 64; break;
		default: n_prescaler = 0; break;
		}
		if( zn_sio_baud != 0 && n_prescaler != 0 )
			n_time = TIME_IN_HZ( 33868800 ) * n_prescaler * zn_sio_baud;
	}
	timer_adjust( zn_sio_timer, n_time, 0, 0 );
}

static void zn_sio_clock( int param )
{
	if( zn_sio_tx_bits == 0 &&
		( zn_sio_control & SIO_CONTROL_TX_ENA ) != 0 &&
		( zn_sio_status & SIO_STATUS_TX_EMPTY ) == 0 )
	{
		zn_sio_tx_bits = 8;
		zn_sio_tx_shift = zn_sio_tx_data;
		zn_sio_rx_bits = 8;
		zn_sio_rx_shift = 0;
		zn_sio_status |= SIO_STATUS_TX_EMPTY | SIO_STATUS_TX_RDY;
	}

	if( zn_sio_tx_bits != 0 )
	{
		zn_sio_tx = ( zn_sio_tx & ~ZN_SIO_OUT_DATA ) | ( ( zn_sio_tx_shift & 1 ) * ZN_SIO_OUT_DATA );
		zn_sio_tx_shift >>= 1;
		zn_sio_tx_bits--;

		if( zn_sio_fn != NULL )
		{
			zn_sio_tx &= ~ZN_SIO_OUT_CLOCK;
			zn_sio_fn( zn_sio_tx );
			zn_sio_tx |= ZN_SIO_OUT_CLOCK;
			zn_sio_fn( zn_sio_tx );
		}

		if( zn_sio_tx_bits == 0 && ( zn_sio_control & SIO_CONTROL_TX_IENA ) != 0 )
			zn_sio_interrupt();
	}

	if( zn_sio_rx_bits != 0 )
	{
		zn_sio_rx_shift = ( zn_sio_rx_shift >> 1 ) | ( ( zn_sio_rx & ZN_SIO_IN_DATA ) << 7 );
		zn_sio_rx_bits--;

		if( zn_sio_rx_bits == 0 )
		{
			if( ( zn_sio_status & SIO_STATUS_RX_RDY ) != 0 )
				zn_sio_status |= SIO_STATUS_OVERRUN;
			else
			{
				zn_sio_rx_data = zn_sio_rx_shift;
				zn_sio_status |= SIO_STATUS_RX_RDY;
			}
			if( ( zn_sio_control & SIO_CONTROL_RX_IENA ) != 0 )
				zn_sio_interrupt();
		}
	}

	zn_sio_timer_adjust();
}

static void zn_sio_input( int n_mask, int n_data )
{
	zn_sio_rx = ( zn_sio_rx & ~n_mask ) | ( n_data & n_mask );

	if( ( zn_sio_rx & ZN_SIO_IN_DSR ) != 0 )
	{
		zn_sio_status |= SIO_STATUS_DSR;
		if( ( zn_sio_rx_prev & ZN_SIO_IN_DSR ) == 0 &&
			( zn_sio_control & SIO_CONTROL_DSR_IENA ) != 0 )
			zn_sio_interrupt();
	}
	else
		zn_sio_status &= ~SIO_STATUS_DSR;
	zn_sio_rx_prev = zn_sio_rx;
}

static WRITE32_HANDLER( zn_sio_w )
{
	switch( offset )
	{
	case 0:
		zn_sio_tx_data = data;
		zn_sio_status &= ~( SIO_STATUS_TX_RDY | SIO_STATUS_TX_EMPTY );
		zn_sio_timer_adjust();
		break;
	case 2:
		if( ACCESSING_LSW32 )
			zn_sio_mode = data & 0xffff;
		if( ACCESSING_MSW32 )
		{
			zn_sio_control = data >> 16;
			if( ( zn_sio_control & SIO_CONTROL_RESET ) != 0 )
			{
				zn_sio_status |= SIO_STATUS_TX_EMPTY | SIO_STATUS_TX_RDY;
				zn_sio_status &= ~( SIO_STATUS_RX_RDY | SIO_STATUS_OVERRUN | SIO_STATUS_IRQ );
			}
			if( ( zn_sio_control & SIO_CONTROL_IACK ) != 0 )
			{
				zn_sio_status &= ~SIO_STATUS_IRQ;
				zn_sio_control &= ~SIO_CONTROL_IACK;
			}
			if( ( zn_sio_control & SIO_CONTROL_DTR ) != 0 )
				zn_sio_tx |= ZN_SIO_OUT_DTR;
			else
				zn_sio_tx &= ~ZN_SIO_OUT_DTR;

			if( ( ( zn_sio_tx ^ zn_sio_tx_prev ) & ZN_SIO_OUT_DTR ) != 0 && zn_sio_fn != NULL )
				zn_sio_fn( zn_sio_tx );
			zn_sio_tx_prev = zn_sio_tx;
		}
		break;
	case 3:
		if( ACCESSING_MSW32 )
			zn_sio_baud = data >> 16;
		break;
	}
}

static READ32_HANDLER( zn_sio_r )
{
	data32_t data = 0;
	switch( offset )
	{
	case 0:
		data = zn_sio_rx_data;
		zn_sio_status &= ~SIO_STATUS_RX_RDY;
		zn_sio_rx_data = 0xff;
		break;
	case 1:
		data = zn_sio_status;
		break;
	case 2:
		data = ( zn_sio_control << 16 ) | zn_sio_mode;
		break;
	case 3:
		data = zn_sio_baud << 16;
		break;
	}
	return data;
}

/* what is on the other end of port 0: selected by znsecsel */

static UINT32 m_n_znsecsel;
static int m_n_dip_bit;
static int m_b_lastclock;
static mame_timer *zn_dip_timer;

static void sio_znsec0_handler( int n_data )
{
	if( ( n_data & ZN_SIO_OUT_CLOCK ) == 0 )
	{
		if( m_b_lastclock )
			zn_sio_input( ZN_SIO_IN_DATA, ( znsec_step( 0, ( n_data & ZN_SIO_OUT_DATA ) != 0 ) != 0 ) * ZN_SIO_IN_DATA );
		m_b_lastclock = 0;
	}
	else
		m_b_lastclock = 1;
}

static void sio_znsec1_handler( int n_data )
{
	if( ( n_data & ZN_SIO_OUT_CLOCK ) == 0 )
	{
		if( m_b_lastclock )
			zn_sio_input( ZN_SIO_IN_DATA, ( znsec_step( 1, ( n_data & ZN_SIO_OUT_DATA ) != 0 ) != 0 ) * ZN_SIO_IN_DATA );
		m_b_lastclock = 0;
	}
	else
		m_b_lastclock = 1;
}

static void sio_pad_handler( int n_data )
{
	zn_sio_input( ZN_SIO_IN_DATA | ZN_SIO_IN_DSR, ZN_SIO_IN_DATA | ZN_SIO_IN_DSR );
}

#define ZN_PORT_DSW ( 6 )

static void sio_dip_handler( int n_data )
{
	if( ( n_data & ZN_SIO_OUT_CLOCK ) == 0 )
	{
		if( m_b_lastclock )
		{
			int bit = ( readinputport( ZN_PORT_DSW ) >> m_n_dip_bit ) & 1;
			zn_sio_input( ZN_SIO_IN_DATA, bit * ZN_SIO_IN_DATA );
			m_n_dip_bit = ( m_n_dip_bit + 1 ) & 7;
		}
		m_b_lastclock = 0;
	}
	else
		m_b_lastclock = 1;
}

static void zn_dip_timer_fired( int param )
{
	zn_sio_input( ZN_SIO_IN_DSR, param * ZN_SIO_IN_DSR );
	if( param )
		timer_adjust( zn_dip_timer, TIME_IN_CYCLES( 50, 0 ), 0, 0 );
}

static READ32_HANDLER( znsecsel_r )
{
	return m_n_znsecsel;
}

static WRITE32_HANDLER( znsecsel_w )
{
	COMBINE_DATA( &m_n_znsecsel );

	if( zn_bank_secsel )
	{
		tecmo_bank = m_n_znsecsel & 3;
		tecmo_bank_update();
	}

	if( ( m_n_znsecsel & 0x80 ) == 0 )
	{
		zn_sio_fn = sio_pad_handler;
		zn_sio_input( ZN_SIO_IN_DSR, 0 );
	}
	else if( ( m_n_znsecsel & 0x08 ) == 0 )
	{
		znsec_start( 1 );
		zn_sio_fn = sio_znsec1_handler;
		zn_sio_input( ZN_SIO_IN_DSR, 0 );
	}
	else if( ( m_n_znsecsel & 0x04 ) == 0 )
	{
		znsec_start( 0 );
		zn_sio_fn = sio_znsec0_handler;
		zn_sio_input( ZN_SIO_IN_DSR, 0 );
	}
	else
	{
		m_n_dip_bit = 0;
		m_b_lastclock = 1;
		zn_sio_fn = sio_dip_handler;
		zn_sio_input( ZN_SIO_IN_DSR, 0 );
		timer_adjust( zn_dip_timer, TIME_IN_CYCLES( 100, 0 ), 1, 0 );
	}
}

/* board registers */

static int zn_vram_2mb;

static READ32_HANDLER( boardconfig_r )
{
	/* --00/01 mem=4M, 10 mem=8M, 11 mem=16M; -1-- smem=2M; 1--- vmem=2M;
	   011----- rev=1 */
	int n_config = zn_vram_2mb ? 64|32|8 : 64|32;
	if( memory_region_length( REGION_CPU1 ) == 0x800000 )
		n_config |= 2;	/* 8MB (Atari) */
	return n_config;
}

static READ32_HANDLER( zn_unknown_r )
{
	return 0xffffffff;
}

static int zn_gun_mux = -1;	/* Acclaim light guns: which gun the GPU holds, on SERVICE bit 3 */

static READ32_HANDLER( zn_inputs_r )	/* P1, P2, SERVICE, SYSTEM at 0x1fa00000 + 0x100 each */
{
	int n_port = ( offset >> 6 ) & 3;
	UINT32 n_data = readinputport( n_port ) | 0xffffff00;
	if( n_port == 2 && zn_gun_mux >= 0 )
		n_data = ( n_data & ~8 ) | ( zn_gun_mux ? 8 : 0 );
	return n_data;
}

static READ32_HANDLER( zn_inputs2_r )	/* P3, P4 at 0x1fa10000 + 0x100 each */
{
	return readinputport( 4 + ( ( offset >> 6 ) & 1 ) ) | 0xffffff00;
}

static int zn_coin_tecmo;

static WRITE32_HANDLER( zn_coin_w )
{
	if( zn_coin_tecmo )
	{
		/* Tecmo: four coin counters, no lockouts */
		coin_counter_w( 0, data & 0x01 );
		coin_counter_w( 2, data & 0x04 );
		coin_counter_w( 1, data & 0x10 );
		coin_counter_w( 3, data & 0x40 );
		return;
	}
	/* 0x01 counter, 0x02 coin lock 1, 0x20 coin lock 2 */
	coin_counter_w( 0, data & 0x01 );
	coin_counter_w( 1, data & 0x10 );
}

/* AT28C16 EEPROM (2KB, settings) on all four byte lanes, kept in the NVRAM */
static data32_t *zn_eeprom;
static int zn_eeprom_default;	/* region with the factory contents, or 0 */

static READ32_HANDLER( zn_eeprom_r )
{
	return zn_eeprom[ offset ];
}

static WRITE32_HANDLER( zn_eeprom_w )
{
	COMBINE_DATA( &zn_eeprom[ offset ] );
}

static NVRAM_HANDLER( zn )
{
	if( read_or_write )
		mame_fwrite( file, zn_eeprom, 0x800 );
	else if( file )
		mame_fread( file, zn_eeprom, 0x800 );
	else
	{
		memset( zn_eeprom, 0xff, 0x800 );
		if( zn_eeprom_default != 0 && memory_region( zn_eeprom_default ) != NULL &&
			memory_region_length( zn_eeprom_default ) == 0x800 )
		{
			/* byte n of the chip is byte lane n & 3 of word n >> 2 */
			const UINT8 *src = memory_region( zn_eeprom_default );
			int i;
			memset( zn_eeprom, 0, 0x800 );
			for( i = 0; i < 0x800; i++ )
				zn_eeprom[ i >> 2 ] |= (data32_t)src[ i ] << ( ( i & 3 ) * 8 );
		}
	}
}

/* main board memory map: the PSX peripherals as on System 11/12 (shared
   PSX code), the board I/O at 0x1fa00000. The memory system offsets a bank
   by the start of the first range that uses it, so every mirror of the RAM
   and of the BIOS has a bank of its own */
#define ZN_MAIN_READ \
	{ 0x00000000, 0x003fffff, MRA32_RAM },    /* ram */ \
	{ 0x00400000, 0x007fffff, MRA32_BANK4 },  /* ram mirror */ \
	{ 0x1f800000, 0x1f8003ff, MRA32_BANK3 },  /* scratchpad */ \
	{ 0x1f801000, 0x1f80100f, MRA32_RAM }, \
	{ 0x1f801010, 0x1f801013, MRA32_NOP }, \
	{ 0x1f801014, 0x1f801017, psx_spu_delay_r }, \
	{ 0x1f801018, 0x1f80102f, MRA32_NOP }, \
	{ 0x1f801040, 0x1f80104f, zn_sio_r }, \
	{ 0x1f801050, 0x1f80105f, MRA32_NOP }, \
	{ 0x1f801060, 0x1f80106f, MRA32_RAM }, \
	{ 0x1f801070, 0x1f801077, psx_irq_r }, \
	{ 0x1f801080, 0x1f8010ff, psx_dma_r }, \
	{ 0x1f801100, 0x1f80113f, psx_counter_r }, \
	{ 0x1f801810, 0x1f801817, psx_gpu_r }, \
	{ 0x1f801820, 0x1f801827, psx_mdec_r }, \
	{ 0x1f801c00, 0x1f801dff, psx_spu_r }, \
	{ 0x1f802020, 0x1f802033, MRA32_RAM }, \
	{ 0x1fa00000, 0x1fa003ff, zn_inputs_r }, \
	{ 0x1fa10000, 0x1fa101ff, zn_inputs2_r }, \
	{ 0x1fa10200, 0x1fa10203, boardconfig_r }, \
	{ 0x1fa10300, 0x1fa10303, znsecsel_r }, \
	{ 0x1fa30000, 0x1fa30003, MRA32_NOP }, \
	{ 0x1fa40000, 0x1fa40003, MRA32_NOP }, \
	{ 0x1fa60000, 0x1fa60003, MRA32_NOP }, \
	{ 0x1faf0000, 0x1faf07ff, zn_eeprom_r }, \
	{ 0x1fb20000, 0x1fb20007, zn_unknown_r }, \
	{ 0x1fc00000, 0x1fc7ffff, MRA32_BANK11 }, /* bios */ \
	{ 0x80000000, 0x803fffff, MRA32_BANK5 },  /* ram mirror */ \
	{ 0x80400000, 0x807fffff, MRA32_BANK8 }, \
	{ 0x9fc00000, 0x9fc7ffff, MRA32_BANK12 }, /* bios mirror */ \
	{ 0xa0000000, 0xa03fffff, MRA32_BANK9 },  /* ram mirror */ \
	{ 0xbfc00000, 0xbfc7ffff, MRA32_BANK7 },  /* bios mirror */

#define ZN_MAIN_WRITE \
	{ 0x00000000, 0x003fffff, MWA32_RAM }, \
	{ 0x00400000, 0x007fffff, MWA32_BANK4 }, \
	{ 0x1f800000, 0x1f8003ff, MWA32_BANK3 }, \
	{ 0x1f801000, 0x1f80100f, MWA32_RAM }, \
	{ 0x1f801010, 0x1f801013, MWA32_NOP }, \
	{ 0x1f801014, 0x1f801017, psx_spu_delay_w }, \
	{ 0x1f801018, 0x1f80102f, MWA32_NOP }, \
	{ 0x1f801040, 0x1f80104f, zn_sio_w }, \
	{ 0x1f801050, 0x1f80105f, MWA32_NOP }, \
	{ 0x1f801060, 0x1f80106f, MWA32_RAM }, \
	{ 0x1f801070, 0x1f801077, psx_irq_w }, \
	{ 0x1f801080, 0x1f8010ff, psx_dma_w }, \
	{ 0x1f801100, 0x1f80113f, psx_counter_w }, \
	{ 0x1f801810, 0x1f801817, psx_gpu_w }, \
	{ 0x1f801820, 0x1f801827, psx_mdec_w }, \
	{ 0x1f801c00, 0x1f801dff, psx_spu_w }, \
	{ 0x1f802020, 0x1f802033, MWA32_RAM }, \
	{ 0x1f802040, 0x1f802043, MWA32_NOP }, \
	{ 0x1fa10300, 0x1fa10303, znsecsel_w }, \
	{ 0x1fa20000, 0x1fa20003, zn_coin_w }, \
	{ 0x1fa30000, 0x1fa30003, MWA32_NOP }, \
	{ 0x1faf0000, 0x1faf07ff, zn_eeprom_w }, \
	{ 0x1fc00000, 0x1fc7ffff, MWA32_ROM }, \
	{ 0x80000000, 0x803fffff, MWA32_BANK5 }, \
	{ 0x80400000, 0x807fffff, MWA32_BANK8 }, \
	{ 0x9fc00000, 0x9fc7ffff, MWA32_ROM }, \
	{ 0xa0000000, 0xa03fffff, MWA32_BANK9 }, \
	{ 0xbfc00000, 0xbfc7ffff, MWA32_ROM }, \
	{ 0xfffe0130, 0xfffe0133, MWA32_NOP },

static data32_t *zn_scratchpad;

/* direct access to RAM, scratchpad and BIOS for the PSX CPU core, and the
   recompiler (core option, as on System 11/12). Called at machine init,
   after the cpu init that clears them. The RAM is 4 MB (8 MB on the
   Atari board), the BIOS is in REGION_USER3, so psx_machine_cpu_speedups()
   does not fit */
static void zn_cpu_speedups( void )
{
	static const offs_t ram_base[] = { 0x00000000, 0x80000000, 0xa0000000 };
	static const offs_t bios_base[] = { 0x1fc00000, 0x9fc00000, 0xbfc00000 };
	UINT32 n_ram = memory_region_length( REGION_CPU1 );
	UINT32 n_bios = memory_region_length( REGION_USER3 );
	int i;

	if( n_ram > 0x00800000 ) n_ram = 0x00800000;
	for( i = 0; i < 3; i++ )
	{
		psxcpu_add_fastram( ram_base[ i ], ram_base[ i ] + n_ram - 1, 0, memory_region( REGION_CPU1 ) );
	}
	psxcpu_add_fastram( 0x1f800000, 0x1f8003ff, 0, zn_scratchpad );
	for( i = 0; i < 3; i++ )
	{
		psxcpu_add_fastram( bios_base[ i ], bios_base[ i ] + n_bios - 1, 1, memory_region( REGION_USER3 ) );
	}

	if( options.mips_dynarec )
	{
		psxcpu_set_jit( 1 );
	}
}

/* the shared parts of the main board init (zn_driver_init in MAME 0.135) */
static void zn_board_init( void )
{
	zn_bank_secsel = 0;
	cpu_setbank( 3, zn_scratchpad );
	cpu_setbank( 4, memory_region( REGION_CPU1 ) );
	cpu_setbank( 5, memory_region( REGION_CPU1 ) );
	cpu_setbank( 8, memory_region( REGION_CPU1 ) );
	cpu_setbank( 9, memory_region( REGION_CPU1 ) );
	cpu_setbank( 7, memory_region( REGION_USER3 ) );
	cpu_setbank( 11, memory_region( REGION_USER3 ) );
	cpu_setbank( 12, memory_region( REGION_USER3 ) );

	znsec_init( 0, NULL );
	znsec_init( 1, NULL );

	zn_sio_timer = timer_alloc( zn_sio_clock );
	zn_dip_timer = timer_alloc( zn_dip_timer_fired );

	state_save_register_UINT32( "zn", 0, "znsecsel", &m_n_znsecsel, 1 );
	state_save_register_int( "zn", 0, "dip_bit", &m_n_dip_bit );
	state_save_register_int( "zn", 0, "lastclock", &m_b_lastclock );
	state_save_register_UINT32( "zn", 0, "sio_status", &zn_sio_status, 1 );
	state_save_register_UINT32( "zn", 0, "sio_mode", &zn_sio_mode, 1 );
	state_save_register_UINT32( "zn", 0, "sio_control", &zn_sio_control, 1 );
	state_save_register_UINT32( "zn", 0, "sio_baud", &zn_sio_baud, 1 );
	state_save_register_UINT32( "zn", 0, "sio_tx", &zn_sio_tx, 1 );
	state_save_register_UINT32( "zn", 0, "sio_rx", &zn_sio_rx, 1 );
	state_save_register_UINT32( "zn", 0, "sio_tx_prev", &zn_sio_tx_prev, 1 );
	state_save_register_UINT32( "zn", 0, "sio_rx_prev", &zn_sio_rx_prev, 1 );
	state_save_register_UINT32( "zn", 0, "sio_tx_data", &zn_sio_tx_data, 1 );
	state_save_register_UINT32( "zn", 0, "sio_rx_data", &zn_sio_rx_data, 1 );
	state_save_register_UINT32( "zn", 0, "sio_tx_shift", &zn_sio_tx_shift, 1 );
	state_save_register_UINT32( "zn", 0, "sio_rx_shift", &zn_sio_rx_shift, 1 );
	state_save_register_UINT32( "zn", 0, "sio_tx_bits", &zn_sio_tx_bits, 1 );
	state_save_register_UINT32( "zn", 0, "sio_rx_bits", &zn_sio_rx_bits, 1 );
	state_save_register_UINT8( "zn", 0, "znsec0_state", &znsec[ 0 ].state, 1 );
	state_save_register_UINT8( "zn", 0, "znsec0_bit", &znsec[ 0 ].bit, 1 );
	state_save_register_UINT8( "zn", 0, "znsec1_state", &znsec[ 1 ].state, 1 );
	state_save_register_UINT8( "zn", 0, "znsec1_bit", &znsec[ 1 ].bit, 1 );

	psx_driver_init();
}

static void zn_board_reset( void )
{
	zn_gun_mux = -1;
	m_n_znsecsel = 0;
	m_n_dip_bit = 0;
	m_b_lastclock = 1;
	zn_sio_status = SIO_STATUS_TX_EMPTY | SIO_STATUS_TX_RDY;
	zn_sio_mode = zn_sio_control = zn_sio_baud = 0;
	zn_sio_tx = zn_sio_rx = zn_sio_tx_prev = zn_sio_rx_prev = 0;
	zn_sio_tx_data = 0;
	zn_sio_rx_data = 0xff;
	zn_sio_tx_shift = zn_sio_rx_shift = zn_sio_tx_bits = zn_sio_rx_bits = 0;
	zn_sio_fn = sio_pad_handler;
	timer_adjust( zn_sio_timer, TIME_NEVER, 0, 0 );
	zn_cpu_speedups(); /* after the cpu init, which clears them */
	timer_adjust( zn_dip_timer, TIME_NEVER, 0, 0 );
	psx_machine_init();
}

/***************************************************************************

  Capcom ZN-1 (COH-1000C / COH-1002C) and ZN-2 (COH-3002C)

  As in current MAME (capcom_zn_state). The CAT702 on the main board is
  CP01 (ZN-1) or CP10 (ZN-2), the one on the ROM board CPnn. Game ROM: the
  first 4MB fixed at 0x1f000000, then 16 4MB banks at 0x1f400000 (the
  byte written at 0x1fb00000 selects the bank). Country ROM at 0x1fb80000.
  Sound: QSound with a Z80 at 8MHz, commands through a latch and NMI, and
  a 250Hz interrupt.

***************************************************************************/

static UINT32 capcom_bank;

static void capcom_bank_update( void )
{
	UINT32 n_len = memory_region_length( REGION_USER1 );
	UINT32 n_offs = 0x400000 + capcom_bank * 0x400000;
	if( n_len <= 0x400000 )
		n_offs = 0;
	else if( n_offs + 0x400000 > n_len )
		n_offs = 0x400000 + ( n_offs - 0x400000 ) % ( n_len - 0x400000 );
	cpu_setbank( 2, memory_region( REGION_USER1 ) + n_offs );
}

static WRITE32_HANDLER( capcom_bank_w )
{
	if( ACCESSING_LSB32 )
	{
		capcom_bank = data & 0x0f;
		capcom_bank_update();
	}
}

static READ32_HANDLER( capcom_kickharness_r )
{
	/* required for buttons 4,5&6 */
	return 0xffffffff;
}

static WRITE32_HANDLER( capcom_qsound_w )
{
	soundlatch_w( 0, data & 0xff );
	cpu_set_irq_line( 1, IRQ_LINE_NMI, PULSE_LINE );
}

static MEMORY_READ32_START( capcom_zn_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f3fffff, MRA32_BANK1 },  /* fixed game rom */
	{ 0x1f400000, 0x1f7fffff, MRA32_BANK2 },  /* banked game rom */
	{ 0x1fb40010, 0x1fb40013, capcom_kickharness_r },
	{ 0x1fb40020, 0x1fb40023, capcom_kickharness_r },
	{ 0x1fb80000, 0x1fbfffff, MRA32_BANK6 },  /* country rom */
MEMORY_END

static MEMORY_WRITE32_START( capcom_zn_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb00000, 0x1fb00003, capcom_bank_w },
	{ 0x1fb60000, 0x1fb60003, capcom_qsound_w },
	{ 0x1fb80000, 0x1fbfffff, MWA32_ROM },
MEMORY_END

/* the 250Hz interrupt of the Z80 (measured on CPS2): one of every 6
   slots of 1/1500s, so that its phase is a counter in the save state */
static UINT8 capcom_qsound_slot;

static INTERRUPT_GEN( capcom_qsound_interrupt )
{
	if( ++capcom_qsound_slot >= 6 )
	{
		capcom_qsound_slot = 0;
		cpu_set_irq_line( 1, 0, HOLD_LINE );
	}
}

static void capcom_postload( void )
{
	capcom_bank_update();
	qsound_bank_update();
}

static DRIVER_INIT( capcom_zn )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	cpu_setbank( 6, memory_region( REGION_USER2 ) );

	zn_board_init();
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* CP01 / CP10 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* CPnn */

	state_save_register_UINT32( "zn", 0, "capcom_bank", &capcom_bank, 1 );
	state_save_register_UINT8( "zn", 0, "qsound_bank", &qsound_bank, 1 );
	state_save_register_UINT8( "zn", 0, "qsound_slot", &capcom_qsound_slot, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( capcom_postload );
}

static MACHINE_INIT( capcom_zn )
{
	capcom_bank = 0;
	capcom_bank_update();
	qsound_bank = 0;
	qsound_bank_update();
	capcom_qsound_slot = 0;
	zn_board_reset();
}

/* the PSX SPU of the main board (0.35 in MAME) */
static struct PSXSPUinterface zn_spu_interface =
{
	35
};

static struct QSound_interface capcom_qsound_interface =
{
	QSOUND_CLOCK,
	REGION_SOUND1,
	{ 100,100 }
};

/* ZN-1, 1MB VRAM (COH-1000C) */
static MACHINE_DRIVER_START( coh1000c )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( capcom_zn_readmem, capcom_zn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_CPU_ADD( Z80, 8000000 )	/* 8MHz ?? */
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( qsound_readmem, qsound_writemem )
	MDRV_CPU_PORTS( qsound_readport, 0 )
	MDRV_CPU_VBLANK_INT( capcom_qsound_interrupt, 25 )	/* 1500Hz slots, 250Hz */

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 40 )

	MDRV_MACHINE_INIT( capcom_zn )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x512 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
	MDRV_SOUND_ADD( QSOUND, capcom_qsound_interface )
MACHINE_DRIVER_END

/* ZN-1, 2MB VRAM (COH-1002C) */
static MACHINE_DRIVER_START( coh1002c )
	MDRV_IMPORT_FROM( coh1000c )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
MACHINE_DRIVER_END

/* ZN-2 (COH-3002C) */
static MACHINE_DRIVER_START( coh3002c )
	MDRV_IMPORT_FROM( coh1000c )
	MDRV_CPU_REPLACE( "main", PSXCPU, 100000000 / 4 )	/* CXD8661R, 100MHz XTAL */
	MDRV_VIDEO_START( psx_type2_1024x1024 )
MACHINE_DRIVER_END

static DRIVER_INIT( coh1000c )
{
	zn_vram_2mb = 0;
	init_capcom_zn();
}

static DRIVER_INIT( coh1002c )
{
	zn_vram_2mb = 1;
	init_capcom_zn();
}

static DRIVER_INIT( coh3002c )
{
	init_coh1002c();
}

/* Gallop Racer: the QSound sockets are empty, the Z80 is held in reset */
static DRIVER_INIT( glpracr )
{
	init_coh1000c();
	timer_suspendcpu( 1, 1, SUSPEND_REASON_DISABLE );
}

/***************************************************************************

  Tecmo TPS (COH-1002M), as in current MAME

  ZN-1 main board with 2MB VRAM and the Tecmo BIOS. The CAT702 on the main
  board is MG01, the one on the ROM board MGnn; both are read from the ROM
  set. Game ROM: 8MB banks at 0x1f000000, the bank is the byte written at
  0x1fb00006. Sound: the PSX SPU of the main board.

  Cool Boarders Arcade Jam adds a sound board: a Z80 and a YMZ280B, with
  two IDT7202 FIFOs (1024 bytes) between the main CPU and the Z80.

  Gallop Racer 2 Link HW has a Z80 link board, not connected to anything
  in MAME either: its ROM is in the set, the CPU is not emulated.

***************************************************************************/


static void tecmo_bank_update( void )
{
	UINT32 n_banks = memory_region_length( REGION_USER1 ) / 0x800000;
	UINT32 n_bank = tecmo_bank;
	if( n_banks == 0 )
		n_banks = 1;
	if( n_bank >= n_banks )
		n_bank %= n_banks;
	cpu_setbank( 1, memory_region( REGION_USER1 ) + n_bank * 0x800000 );
}

static WRITE32_HANDLER( tecmo_bank_w )	/* byte at 0x1fb00006 */
{
	if( ( mem_mask & 0x00ff0000 ) == 0 )
	{
		tecmo_bank = ( data >> 16 ) & 0xff;
		tecmo_bank_update();
	}
}

static MEMORY_READ32_START( tecmo_zn_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f7fffff, MRA32_BANK1 },  /* banked game rom */
MEMORY_END

static MEMORY_WRITE32_START( tecmo_zn_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb00004, 0x1fb00007, tecmo_bank_w },
MEMORY_END

/* Cool Boarders Arcade Jam sound board */

#define CBAJ_FIFO_SIZE ( 1024 )

static struct
{
	UINT8 data[ CBAJ_FIFO_SIZE ];
	UINT32 read;
	UINT32 count;
} cbaj_fifo[ 2 ];	/* 0: main CPU to Z80, 1: Z80 to main CPU */

static UINT8 cbaj_fifo_read( int n )
{
	UINT8 data;
	if( cbaj_fifo[ n ].count == 0 )
		return 0xff;	/* underflow: the 9 data lines float high */
	data = cbaj_fifo[ n ].data[ cbaj_fifo[ n ].read ];
	cbaj_fifo[ n ].read = ( cbaj_fifo[ n ].read + 1 ) % CBAJ_FIFO_SIZE;
	cbaj_fifo[ n ].count--;
	return data;
}

static void cbaj_fifo_write( int n, UINT8 data )
{
	if( cbaj_fifo[ n ].count == CBAJ_FIFO_SIZE )
		return;	/* overflow: the write is lost */
	cbaj_fifo[ n ].data[ ( cbaj_fifo[ n ].read + cbaj_fifo[ n ].count ) % CBAJ_FIFO_SIZE ] = data;
	cbaj_fifo[ n ].count++;
}

static READ32_HANDLER( cbaj_sound_main_r )
{
	data32_t data = 0;
	if( ACCESSING_LSB32 )
		data |= cbaj_fifo_read( 1 );
	if( ACCESSING_MSB32 )	/* 0x1fb00003, d1: data from the Z80 */
		data |= ( cbaj_fifo[ 1 ].count != 0 ? 0x02 : 0x00 ) << 24;
	return data;
}

static WRITE32_HANDLER( cbaj_sound_main_w )
{
	if( ACCESSING_LSB32 )
		cbaj_fifo_write( 0, data & 0xff );
}

static MEMORY_READ32_START( cbaj_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f7fffff, MRA32_BANK1 },  /* banked game rom */
	{ 0x1fb00000, 0x1fb00003, cbaj_sound_main_r },
MEMORY_END

static MEMORY_WRITE32_START( cbaj_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb00000, 0x1fb00003, cbaj_sound_main_w },
	{ 0x1fb00004, 0x1fb00007, tecmo_bank_w },
MEMORY_END

static READ_HANDLER( cbaj_z80_fifo_r )
{
	return cbaj_fifo_read( 0 );
}

static WRITE_HANDLER( cbaj_z80_fifo_w )
{
	cbaj_fifo_write( 1, data );
}

static READ_HANDLER( cbaj_z80_status_r )
{
	/* d1: data from the main CPU */
	return cbaj_fifo[ 0 ].count != 0 ? 0x02 : 0x00;
}

static READ_HANDLER( cbaj_ymz280b_ext_r )
{
	return 0xff;
}

static MEMORY_READ_START( cbaj_z80_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( cbaj_z80_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xffff, MWA_RAM },
MEMORY_END

static PORT_READ_START( cbaj_z80_readport )
	{ 0x84, 0x84, cbaj_ymz280b_ext_r },
	{ 0x85, 0x85, YMZ280B_status_0_r },
	{ 0x90, 0x90, cbaj_z80_fifo_r },
	{ 0x91, 0x91, cbaj_z80_status_r },
PORT_END

static PORT_WRITE_START( cbaj_z80_writeport )
	{ 0x84, 0x84, YMZ280B_register_0_w },
	{ 0x85, 0x85, YMZ280B_data_0_w },
	{ 0x90, 0x90, cbaj_z80_fifo_w },
PORT_END

static struct YMZ280Binterface cbaj_ymz280b_interface =
{
	1,
	{ 16934400 },
	{ REGION_SOUND1 },
	{ YM3012_VOL( 35, MIXER_PAN_LEFT, 35, MIXER_PAN_RIGHT ) },
	{ 0 }
};

static void tecmo_bank_postload( void )
{
	tecmo_bank_update();
}

static DRIVER_INIT( coh1002m )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 1;
	zn_eeprom_default = REGION_USER6;	/* factory settings, when the set has them */

	zn_board_init();
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* MG01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* MGnn */

	memset( cbaj_fifo, 0, sizeof( cbaj_fifo ) );

	state_save_register_UINT32( "zn", 0, "tecmo_bank", &tecmo_bank, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_UINT8( "zn", 0, "cbaj_fifo0", cbaj_fifo[ 0 ].data, CBAJ_FIFO_SIZE );
	state_save_register_UINT32( "zn", 0, "cbaj_fifo0_read", &cbaj_fifo[ 0 ].read, 1 );
	state_save_register_UINT32( "zn", 0, "cbaj_fifo0_count", &cbaj_fifo[ 0 ].count, 1 );
	state_save_register_UINT8( "zn", 0, "cbaj_fifo1", cbaj_fifo[ 1 ].data, CBAJ_FIFO_SIZE );
	state_save_register_UINT32( "zn", 0, "cbaj_fifo1_read", &cbaj_fifo[ 1 ].read, 1 );
	state_save_register_UINT32( "zn", 0, "cbaj_fifo1_count", &cbaj_fifo[ 1 ].count, 1 );
	state_save_register_func_postload( tecmo_bank_postload );
}

static MACHINE_INIT( tecmo_zn )
{
	tecmo_bank = 0;
	tecmo_bank_update();
	memset( cbaj_fifo, 0, sizeof( cbaj_fifo ) );
	zn_board_reset();
}

/* ZN-1, 2MB VRAM (COH-1002M) */
static MACHINE_DRIVER_START( coh1002m )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( tecmo_zn_readmem, tecmo_zn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( tecmo_zn )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( cbaj )
	MDRV_IMPORT_FROM( coh1002m )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_MEMORY( cbaj_readmem, cbaj_writemem )

	MDRV_CPU_ADD( Z80, 32000000 / 8 )
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( cbaj_z80_readmem, cbaj_z80_writemem )
	MDRV_CPU_PORTS( cbaj_z80_readport, cbaj_z80_writeport )

	MDRV_INTERLEAVE( 100 )	/* 6000Hz in MAME */

	MDRV_SOUND_ADD( YMZ280B, cbaj_ymz280b_interface )
MACHINE_DRIVER_END

/***************************************************************************

  Taito FX-1A (COH-1000T), as in current MAME

  ZN-1 main board with 1MB VRAM and the Taito BIOS; CAT702 TT01 on the
  main board, TTnn on the ROM board. Game ROM: 8MB banks at 0x1f000000,
  bits 0-1 of the byte written at 0x1fb40000 (bit 5 kicks the MB3773
  watchdog, not emulated). Sound: a Z80 with a YM2610B, commands through
  a TC0140SYT at 0x1fb80000, and the PSX SPU of the main board.

***************************************************************************/

static WRITE32_HANDLER( taito_fx1_bank_w )	/* byte at 0x1fb40000 */
{
	if( ACCESSING_LSB32 )
	{
		tecmo_bank = data & 3;
		tecmo_bank_update();
	}
}

static READ32_HANDLER( taito_fx1a_tc0140syt_r )
{
	/* 0x1fb80002: comm */
	if( !( mem_mask & 0x00ff0000 ) )
		return taitosound_comm_r( 0 ) << 16;
	return 0;
}

static WRITE32_HANDLER( taito_fx1a_tc0140syt_w )
{
	if( ACCESSING_LSB32 )					/* 0x1fb80000: port */
		taitosound_port_w( 0, data & 0xff );
	if( !( mem_mask & 0x00ff0000 ) )		/* 0x1fb80002: comm */
		taitosound_comm_w( 0, ( data >> 16 ) & 0xff );
}

static MEMORY_READ32_START( taito_fx1a_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f7fffff, MRA32_BANK1 },  /* banked game rom */
	{ 0x1fb80000, 0x1fb80003, taito_fx1a_tc0140syt_r },
MEMORY_END

static MEMORY_WRITE32_START( taito_fx1a_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb40000, 0x1fb40003, taito_fx1_bank_w },
	{ 0x1fb80000, 0x1fb80003, taito_fx1a_tc0140syt_w },
MEMORY_END

/* the Z80 program is at 0x10000 in REGION_CPU2 (its first 16KB copied to
   0x0000 at init): 16KB fixed, then 8 banks of 16KB at 0x4000 */
static UINT8 taito_fx1a_soundbank;

static void taito_fx1a_soundbank_update( void )
{
	cpu_setbank( 10, memory_region( REGION_CPU2 ) + 0x10000 + ( taito_fx1a_soundbank & 7 ) * 0x4000 );
}

static WRITE_HANDLER( taito_fx1a_soundbank_w )
{
	taito_fx1a_soundbank = data & 7;
	taito_fx1a_soundbank_update();
}

static MEMORY_READ_START( taito_fx1a_sound_readmem )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x7fff, MRA_BANK10 },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe000, YM2610_status_port_0_A_r },
	{ 0xe001, 0xe001, YM2610_read_port_0_r },
	{ 0xe002, 0xe002, YM2610_status_port_0_B_r },
	{ 0xe200, 0xe200, MRA_NOP },
	{ 0xe201, 0xe201, taitosound_slave_comm_r },
	{ 0xee00, 0xee00, MRA_NOP },
MEMORY_END

static MEMORY_WRITE_START( taito_fx1a_sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe000, YM2610_control_port_0_A_w },
	{ 0xe001, 0xe001, YM2610_data_port_0_A_w },
	{ 0xe002, 0xe002, YM2610_control_port_0_B_w },
	{ 0xe003, 0xe003, YM2610_data_port_0_B_w },
	{ 0xe200, 0xe200, taitosound_slave_port_w },
	{ 0xe201, 0xe201, taitosound_slave_comm_w },
	{ 0xe400, 0xe403, MWA_NOP },	/* pan */
	{ 0xee00, 0xee00, MWA_NOP },	/* ? */
	{ 0xf000, 0xf000, MWA_NOP },	/* ? */
	{ 0xf200, 0xf200, taito_fx1a_soundbank_w },
MEMORY_END

static void taito_fx1a_ym_irq( int irq )
{
	cpu_set_irq_line( 1, 0, irq ? ASSERT_LINE : CLEAR_LINE );
}

static struct YM2610interface taito_fx1a_ym2610b_interface =
{
	1,
	16000000 / 2,
	{ 75 },		/* SSG, as MAME */
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ taito_fx1a_ym_irq },
	{ REGION_SOUND1 },	/* no Delta-T samples */
	{ REGION_SOUND1 },	/* ADPCM-A */
	{ YM3012_VOL( 100, MIXER_PAN_LEFT, 100, MIXER_PAN_RIGHT ) }
};

static void taito_fx1a_postload( void )
{
	tecmo_bank_update();
	taito_fx1a_soundbank_update();
}

static DRIVER_INIT( coh1000ta )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 0;
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	zn_board_init();
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* TT01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* TTnn */

	/* the fixed 16KB of the Z80 program */
	memcpy( memory_region( REGION_CPU2 ), memory_region( REGION_CPU2 ) + 0x10000, 0x4000 );

	state_save_register_UINT32( "zn", 0, "taito_bank", &tecmo_bank, 1 );
	state_save_register_UINT8( "zn", 0, "taito_soundbank", &taito_fx1a_soundbank, 1 );
	taitosound_state_register();
	YM2610_timer_state_register();
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( taito_fx1a_postload );
}

static MACHINE_INIT( taito_fx1a )
{
	tecmo_bank = 0;
	tecmo_bank_update();
	taito_fx1a_soundbank = 1;
	taito_fx1a_soundbank_update();
	taitosound_reset();
	zn_board_reset();
}

/* ZN-1, 1MB VRAM (COH-1000T), FX-1A sound */
static MACHINE_DRIVER_START( coh1000ta )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( taito_fx1a_readmem, taito_fx1a_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_CPU_ADD( Z80, 16000000 / 4 )
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( taito_fx1a_sound_readmem, taito_fx1a_sound_writemem )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 100 )

	MDRV_MACHINE_INIT( taito_fx1a )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x512 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
	MDRV_SOUND_ADD( YM2610B, taito_fx1a_ym2610b_interface )
MACHINE_DRIVER_END

/***************************************************************************

  Taito FX-1B (COH-1000T / COH-1002T with the ZROM board), as current MAME

  The FX-1A main board (1MB VRAM; G-Darius has 2MB) with the Taito Zoom
  sound board: an MN10200 sound CPU, a ZSG-2 PCM chip and a TMS57002 DSP.
  The main CPU talks to it through two registers at 0x1fb80000 (address
  at +2, data at +0), a sound IRQ at 0x1fba0000 and 256 bytes of shared
  RAM at 0x1fbe0000 (lanes 0 and 2). 512 bytes of FRAM at 0x1fb00000
  (lanes 0 and 2) are kept in the NVRAM with the EEPROM.

***************************************************************************/

static UINT8 taito_fx1b_fram[ 0x200 ];
/* a byte at lanes 0 and 2 of each word: byte offset * 2 + ( lane 2 ) */
static READ32_HANDLER( taito_fx1b_fram_r )
{
	return taito_fx1b_fram[ offset * 2 ] | ( taito_fx1b_fram[ offset * 2 + 1 ] << 16 );
}

static WRITE32_HANDLER( taito_fx1b_fram_w )
{
	if( ACCESSING_LSB32 )
		taito_fx1b_fram[ offset * 2 ] = data & 0xff;
	if( !( mem_mask & 0x00ff0000 ) )
		taito_fx1b_fram[ offset * 2 + 1 ] = ( data >> 16 ) & 0xff;
}

/* the SPU at 0.3, as MAME on this board */
static struct PSXSPUinterface taito_fx1b_spu_interface =
{
	30
};

static MEMORY_READ32_START( taito_fx1b_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f7fffff, MRA32_BANK1 },  /* banked game rom */
	{ 0x1fb00000, 0x1fb003ff, taito_fx1b_fram_r },
	{ 0x1fbc0000, 0x1fbc0003, taitozoom_sound_irq_r },
	{ 0x1fbe0000, 0x1fbe01ff, taitozoom_shared_r },
MEMORY_END

static MEMORY_WRITE32_START( taito_fx1b_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb00000, 0x1fb003ff, taito_fx1b_fram_w },
	{ 0x1fb40000, 0x1fb40003, taito_fx1_bank_w },
	{ 0x1fb80000, 0x1fb80003, taitozoom_reg_w },
	{ 0x1fba0000, 0x1fba0003, taitozoom_sound_irq_w },
	{ 0x1fbe0000, 0x1fbe01ff, taitozoom_shared_w },
MEMORY_END

static NVRAM_HANDLER( taito_fx1b )
{
	nvram_handler_zn( file, read_or_write );
	if( read_or_write )
		mame_fwrite( file, taito_fx1b_fram, sizeof( taito_fx1b_fram ) );
	else if( file )
		mame_fread( file, taito_fx1b_fram, sizeof( taito_fx1b_fram ) );
	else
		memset( taito_fx1b_fram, 0xff, sizeof( taito_fx1b_fram ) );
}

static void taito_fx1b_postload( void )
{
	tecmo_bank_update();
}

static void taito_fx1b_init( int vram_2mb )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = vram_2mb;
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	zn_board_init();
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* TT01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* TTnn */

	state_save_register_UINT32( "zn", 0, "taito_bank", &tecmo_bank, 1 );
	state_save_register_UINT8( "zn", 0, "taito_fram", taito_fx1b_fram, sizeof( taito_fx1b_fram ) );
	taitozoom_init();
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( taito_fx1b_postload );
}

static DRIVER_INIT( coh1000tb )
{
	taito_fx1b_init( 0 );
}

static DRIVER_INIT( coh1002tb )
{
	taito_fx1b_init( 1 );
}

static MACHINE_INIT( taito_fx1b )
{
	tecmo_bank = 0;
	tecmo_bank_update();
	taitozoom_reset();
	zn_board_reset();
}

/* ZN-1, 1MB VRAM (COH-1000T), FX-1B (Taito Zoom) sound */
static MACHINE_DRIVER_START( coh1000tb )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( taito_fx1b_readmem, taito_fx1b_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_CPU_ADD( MN10200, 25000000 / 2 / 2 )	/* MN1020012A, 12.5MHz, internal / 2 */
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( taitozoom_mn_readmem, taitozoom_mn_writemem )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 100 )

	MDRV_MACHINE_INIT( taito_fx1b )
	MDRV_NVRAM_HANDLER( taito_fx1b )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x512 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, taito_fx1b_spu_interface )
	MDRV_SOUND_ADD( CUSTOM, taitozoom_custom_interface )
MACHINE_DRIVER_END

/* ZN-1, 2MB VRAM (COH-1002T), FX-1B sound: G-Darius */
static MACHINE_DRIVER_START( coh1002tb )
	MDRV_IMPORT_FROM( coh1000tb )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
MACHINE_DRIVER_END


/***************************************************************************

  Eighting / Raizing (COH-1002E), as in current MAME

  ZN-1 main board with 2MB VRAM and the Raizing BIOS (Brave Blade: the
  Tecmo board and BIOS); CAT702 ET01 (MG01) on the main board, ETnn (MG11)
  on the ROM board. Game ROM: 8MB banks at 0x1f000000, bits 0-1 of the
  CAT702 / DIP select register. Sound: a 68000 with a YMF271, commands
  through a latch at 0x1fb00000, IRQ 2 at 0x1fb00004; and the PSX SPU.

***************************************************************************/

static WRITE32_HANDLER( raizing_soundlatch_w )	/* byte at 0x1fb00000 */
{
	if( ACCESSING_LSB32 )
		soundlatch_w( 0, data & 0xff );
}

static WRITE32_HANDLER( raizing_sound_irq_w )	/* byte at 0x1fb00004 */
{
	if( ACCESSING_LSB32 )
		cpu_set_irq_line( 1, 2, HOLD_LINE );
}

static MEMORY_READ32_START( raizing_zn_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f7fffff, MRA32_BANK1 },  /* banked game rom */
MEMORY_END

static MEMORY_WRITE32_START( raizing_zn_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb00000, 0x1fb00003, raizing_soundlatch_w },
	{ 0x1fb00004, 0x1fb00007, raizing_sound_irq_w },
MEMORY_END

/* the YMF271 on the low byte of each word */
static READ16_HANDLER( raizing_ymf271_r )
{
	return YMF271_0_r( offset );
}

static WRITE16_HANDLER( raizing_ymf271_w )
{
	if( ACCESSING_LSB16 )
		YMF271_0_w( offset, data & 0xff );
}

static READ16_HANDLER( raizing_soundlatch_r )
{
	return soundlatch_r( 0 );
}

static MEMORY_READ16_START( raizing_sound_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x080000, 0x0fffff, MRA16_RAM },
	{ 0x100000, 0x10001f, raizing_ymf271_r },
	{ 0x180008, 0x180009, raizing_soundlatch_r },
MEMORY_END

static MEMORY_WRITE16_START( raizing_sound_writemem )
	{ 0x000000, 0x07ffff, MWA16_NOP },
	{ 0x080000, 0x0fffff, MWA16_RAM },
	{ 0x100000, 0x10001f, raizing_ymf271_w },
	{ 0x100020, 0xffffff, MWA16_NOP },
MEMORY_END

static struct YMF271interface raizing_ymf271_interface =
{
	1,
	{ REGION_SOUND1 },
	{ YM3012_VOL( 100, MIXER_PAN_LEFT, 100, MIXER_PAN_RIGHT ) },
	{ 0 },
	{ 0 },
	{ 0 }
};

static void raizing_postload( void )
{
	tecmo_bank_update();
}

static void raizing_zn_init( void )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 0;
	zn_eeprom_default = REGION_USER6;	/* factory settings, when the set has them */

	zn_board_init();
	zn_bank_secsel = 1;
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* ET01 / MG01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* ETnn / MG11 */

	state_save_register_UINT32( "zn", 0, "raizing_bank", &tecmo_bank, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( raizing_postload );
}

static DRIVER_INIT( coh1002e )
{
	raizing_zn_init();
}

static MACHINE_INIT( raizing_zn )
{
	tecmo_bank = 0;
	tecmo_bank_update();
	zn_board_reset();
}

/* ZN-1, 2MB VRAM (COH-1002E), 68000 + YMF271 sound */
static MACHINE_DRIVER_START( coh1002e )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( raizing_zn_readmem, raizing_zn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_CPU_ADD( M68000, 12000000 )
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( raizing_sound_readmem, raizing_sound_writemem )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 100 )

	MDRV_MACHINE_INIT( raizing_zn )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
	MDRV_SOUND_ADD( YMF271, raizing_ymf271_interface )
MACHINE_DRIVER_END


/***************************************************************************

  Atari / Time Warner (COH-1000W), as in current MAME

  ZN-1 main board with 2MB VRAM, 8MB of RAM and the Time Warner BIOS;
  CAT702 TW01 on the main board, TW02 on the ROM board. Game ROM: 2MB at
  0x1f000000. A VT83C461 IDE controller with the hard disk: its registers
  at 0x1f7e4000, a 32-bit data port at 0x1f7f4000 (the high half is
  latched), an interrupt enable at 0x1f7e8000 (bit 0), the interrupt on
  PSX IRQ 10 and the sector data on PSX DMA channel 5. Sound: the PSX SPU
  only.

***************************************************************************/

static UINT16 atari_irqctrl;
static UINT16 atari_latch;
static UINT8 atari_ide_irq;

static void atari_ide_interrupt( int state )
{
	atari_ide_irq = ( state != CLEAR_LINE );
	if( atari_ide_irq && ( atari_irqctrl & 1 ) )
		psx_irq_set( 0x400 );
}

static struct ide_interface atari_ide_intf =
{
	atari_ide_interrupt
};

static READ32_HANDLER( atari_irqctrl_r )
{
	return atari_irqctrl;
}

static WRITE32_HANDLER( atari_irqctrl_w )
{
	UINT32 n_ctrl = atari_irqctrl;
	COMBINE_DATA( &n_ctrl );
	atari_irqctrl = n_ctrl & 0xffff;
}

/* the 32-bit data port: a read of 0x1f0 returns the low half and latches
   the high half, which a read of 0x1f2 returns */
static READ32_HANDLER( atari_ide32_r )
{
	UINT32 n_data = 0xffffffff;
	if( offset == 0x1f0 / 4 )
	{
		if( ( mem_mask & 0x0000ffff ) == 0 )
		{
			UINT32 n_data32 = ide_controller32_0_r( 0x1f0 / 4, 0 );
			atari_latch = n_data32 >> 16;
			n_data = ( n_data & 0xffff0000 ) | ( n_data32 & 0xffff );
		}
		if( ( mem_mask & 0xffff0000 ) == 0 )
			n_data = ( n_data & 0x0000ffff ) | ( (UINT32)atari_latch << 16 );
	}
	return n_data;
}

/* the sector data on DMA channel 5, 16 bits at a time */
static void atari_dma_read( UINT32 n_address, INT32 n_size )
{
	UINT8 *p_ram = memory_region( REGION_CPU1 );
	UINT32 n_mask = memory_region_length( REGION_CPU1 ) - 1;

	n_size <<= 1;
	while( n_size > 0 )
	{
		UINT16 n_word = ide_controller32_0_r( 0x1f0 / 4, 0xffff0000 ) & 0xffff;
		*( (UINT16 *)( p_ram + WORD_XOR_LE( n_address & n_mask ) ) ) = n_word;
		n_address += 2;
		n_size--;
	}
}

static void atari_dma_write( UINT32 n_address, INT32 n_size )
{
}

static MEMORY_READ32_START( atari_zn_readmem )
	{ 0x00000000, 0x007fffff, MRA32_RAM },    /* ram, 8MB */
	{ 0x80000000, 0x807fffff, MRA32_BANK5 },  /* ram mirror */
	{ 0xa0000000, 0xa07fffff, MRA32_BANK9 },  /* ram mirror */
	{ 0x1f000000, 0x1f1fffff, MRA32_BANK1 },  /* game rom */
	{ 0x1f7e4000, 0x1f7e4fff, ide_controller32_0_r },
	{ 0x1f7e8000, 0x1f7e8003, atari_irqctrl_r },
	{ 0x1f7f4000, 0x1f7f4fff, atari_ide32_r },
	ZN_MAIN_READ
MEMORY_END

static MEMORY_WRITE32_START( atari_zn_writemem )
	{ 0x00000000, 0x007fffff, MWA32_RAM },
	{ 0x80000000, 0x807fffff, MWA32_BANK5 },
	{ 0xa0000000, 0xa07fffff, MWA32_BANK9 },
	{ 0x1f000000, 0x1f000003, MWA32_NOP },    /* watchdog (DS1232) */
	{ 0x1f000004, 0x1f1fffff, MWA32_ROM },
	{ 0x1f7e4000, 0x1f7e4fff, ide_controller32_0_w },
	{ 0x1f7e8000, 0x1f7e8003, atari_irqctrl_w },
	{ 0x1f7f4000, 0x1f7f4fff, MWA32_NOP },
	ZN_MAIN_WRITE
MEMORY_END

static DRIVER_INIT( coh1000w )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 0;
	zn_eeprom_default = REGION_USER6;	/* factory settings */

	zn_board_init();
	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* TW01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* TW02 */

	psx_dma_install_read_handler( 5, atari_dma_read );
	psx_dma_install_write_handler( 5, atari_dma_write );
	ide_controller_init( 0, &atari_ide_intf );

	state_save_register_UINT16( "zn", 0, "atari_irqctrl", &atari_irqctrl, 1 );
	state_save_register_UINT16( "zn", 0, "atari_latch", &atari_latch, 1 );
	state_save_register_UINT8( "zn", 0, "atari_ide_irq", &atari_ide_irq, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
}

static MACHINE_INIT( atari_zn )
{
	atari_irqctrl = 0;
	atari_latch = 0;
	atari_ide_irq = 0;
	ide_controller_reset( 0 );
	zn_board_reset();
}

/* ZN-1, 2MB VRAM, 8MB RAM (COH-1000W), SPU sound */
static MACHINE_DRIVER_START( coh1000w )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( atari_zn_readmem, atari_zn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( atari_zn )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
MACHINE_DRIVER_END


/***************************************************************************

  Acclaim (COH-1000A): Judge Dredd, as in current MAME

  ZN-1 main board with 2MB VRAM and the Acclaim BIOS; CAT702 AC01 on the
  main board, AC02 on the ROM board. Game ROM: 2MB at 0x1f000000. An IDE
  hard disk: the command block at 0x1fbfff90, the control block at
  0x1fbfff80, 16 bits per register; its interrupt on PSX IRQ 10. Two light
  guns: at each vblank the board switches gun (SERVICE bit 3) and the GPU
  latches its coordinates. Sound: the PSX SPU only.

***************************************************************************/

static UINT8 acclaim_gun_mux;

static void acclaim_ide_interrupt( int state )
{
	if( state != CLEAR_LINE )
		psx_irq_set( 0x400 );
}

static struct ide_interface acclaim_ide_intf =
{
	acclaim_ide_interrupt
};

/* the IDE registers of a block (0x1f0 or 0x3f0), one per 16-bit word */
static UINT32 acclaim_ide_read( int n_base, offs_t offset, UINT32 mem_mask )
{
	UINT32 n_data = 0;
	int n_half;
	for( n_half = 0; n_half < 2; n_half++ )
	{
		UINT32 n_halfmask = 0xffff << ( n_half * 16 );
		if( ( mem_mask & n_halfmask ) != n_halfmask )
		{
			int n_reg = n_base + offset * 2 + n_half;
			int n_lane = n_reg & 3;
			UINT32 n_mask = ( n_reg == 0x1f0 ) ? 0xffff0000 : ~( 0xff << ( n_lane * 8 ) );
			UINT32 n_value = ide_controller32_0_r( n_reg / 4, n_mask ) >> ( n_lane * 8 );
			n_value &= ( n_reg == 0x1f0 ) ? 0xffff : 0xff;
			n_data |= n_value << ( n_half * 16 );
		}
	}
	return n_data;
}

static void acclaim_ide_write( int n_base, offs_t offset, UINT32 data, UINT32 mem_mask )
{
	int n_half;
	for( n_half = 0; n_half < 2; n_half++ )
	{
		UINT32 n_halfmask = 0xffff << ( n_half * 16 );
		if( ( mem_mask & n_halfmask ) != n_halfmask )
		{
			int n_reg = n_base + offset * 2 + n_half;
			int n_lane = n_reg & 3;
			UINT32 n_value = ( data >> ( n_half * 16 ) ) & ( ( n_reg == 0x1f0 ) ? 0xffff : 0xff );
			UINT32 n_mask = ( n_reg == 0x1f0 ) ? 0xffff0000 : ~( 0xff << ( n_lane * 8 ) );
			ide_controller32_0_w( n_reg / 4, n_value << ( n_lane * 8 ), n_mask );
		}
	}
}

static READ32_HANDLER( acclaim_ide_cs0_r ) { return acclaim_ide_read( 0x1f0, offset, mem_mask ); }
static WRITE32_HANDLER( acclaim_ide_cs0_w ) { acclaim_ide_write( 0x1f0, offset, data, mem_mask ); }
static READ32_HANDLER( acclaim_ide_cs1_r ) { return acclaim_ide_read( 0x3f0, offset, mem_mask ); }
static WRITE32_HANDLER( acclaim_ide_cs1_w ) { acclaim_ide_write( 0x3f0, offset, data, mem_mask ); }

static MEMORY_READ32_START( jdredd_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f1fffff, MRA32_BANK1 },  /* game rom */
	{ 0x1fbfff80, 0x1fbfff8f, acclaim_ide_cs1_r },
	{ 0x1fbfff90, 0x1fbfff9f, acclaim_ide_cs0_r },
MEMORY_END

static MEMORY_WRITE32_START( jdredd_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f1fffff, MWA32_ROM },
	{ 0x1fbfff00, 0x1fbfff03, MWA32_NOP },    /* bank (no effect) */
	{ 0x1fbfff10, 0x1fbfff13, MWA32_NOP },
	{ 0x1fbfff80, 0x1fbfff8f, acclaim_ide_cs1_w },
	{ 0x1fbfff90, 0x1fbfff9f, acclaim_ide_cs0_w },
MEMORY_END

/* at each vblank: the other gun, and its coordinates to the GPU when it
   points at the screen */
static INTERRUPT_GEN( jdredd_vblank )
{
	int n_x, n_y;

	acclaim_gun_mux = !acclaim_gun_mux;
	zn_gun_mux = acclaim_gun_mux;
	n_x = readinputport( 7 + acclaim_gun_mux * 2 );
	n_y = readinputport( 8 + acclaim_gun_mux * 2 );
	if( n_x > 0x393 && n_x < 0xcb2 && n_y > 0x02d && n_y < 0x217 )
		psx_gpu_lightgun_set( n_x, n_y );
	psx_vblank();
}

/* the crosshairs: the gun ports span the whole screen, as
   PORT_CROSSHAIR( X/Y, 1.0, 0.0, 0 ) in MAME */
static VIDEO_UPDATE( jdredd )
{
	const struct rectangle *visible = &Machine->visible_area;
	int n_gun;

	video_update_psx( bitmap, cliprect );

	for( n_gun = 0; n_gun < 2; n_gun++ )
	{
		int n_x = readinputport( 7 + n_gun * 2 ) - 0x392;
		int n_y = readinputport( 8 + n_gun * 2 ) - 0x02c;
		n_x = visible->min_x + n_x * ( visible->max_x - visible->min_x ) / ( 0xcb3 - 0x392 );
		n_y = visible->min_y + n_y * ( visible->max_y - visible->min_y ) / ( 0x218 - 0x02c );
		draw_crosshair( n_gun + 1, bitmap, n_x, n_y, cliprect );
	}
}

static void jdredd_postload( void )
{
	zn_gun_mux = acclaim_gun_mux;
}

static DRIVER_INIT( jdredd )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	zn_board_init();
	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* AC01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* AC02 */

	ide_controller_init( 0, &acclaim_ide_intf );

	state_save_register_UINT8( "zn", 0, "acclaim_gun_mux", &acclaim_gun_mux, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( jdredd_postload );
}

static MACHINE_INIT( jdredd )
{
	ide_controller_reset( 0 );
	zn_board_reset();
	acclaim_gun_mux = 0;
	zn_gun_mux = 0;
}

/* ZN-1, 2MB VRAM (COH-1000A), hard disk, SPU sound */
static MACHINE_DRIVER_START( jdredd )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( jdredd_readmem, jdredd_writemem )
	MDRV_CPU_VBLANK_INT( jdredd_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( jdredd )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( jdredd )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
MACHINE_DRIVER_END


/***************************************************************************

  Acclaim (COH-1000A): NBA Jam Extreme, as in current MAME

  The Acclaim ZN-1 board of Judge Dredd, without the hard disk: 32MB of
  game ROM in 16 banks of 2MB and 32KB of battery backed RAM (71256), and
  the RAX sound board.

  Game ROM at 0x1f000000: the first 2MB window holds the bank written in
  the low half of 0x1fbfff00. When that byte is 0, the backup RAM is at
  0x1f200000 (8 bits per byte address); otherwise the 6MB from 0x1f200000
  read the ROM from the bank written in the high half (inverted bit 4),
  and the backup RAM is still written there.

  RAX sound board (MAME shared/rax.cpp): an ADSP-2181 at 16.67MHz with
  8MB of ROM (two banks of 4MB) read by its byte DMA (BDMA), 4 banks of
  8K words of external data RAM behind the data memory overlay, and two
  DACs fed by SPORT0 in autobuffer mode. The main CPU writes 16-bit
  commands to 0x1fbfff80; the ADSP reads them at I/O 3, which clears its
  IRQL0.

***************************************************************************/

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

#define RAX_CLOCK			16670000
#define RAX_BUFFER_SIZE		0x4000		/* DAC samples per channel */
#define RAX_BUFFER_MASK		( RAX_BUFFER_SIZE - 1 )

static int rax_cpunum;
static UINT8 *rax_rom;
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
	const UINT8 *adsp_rom = rax_rom + ( rax_rom_bank & 1 ) * 0x400000;
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

static struct CustomSound_interface rax_custom_interface =
{
	rax_custom_start, 0, 0
};

static MEMORY_READ16_START( rax_readmem )
	{ ADSP_DATA_ADDR_RANGE( 0x0000, 0x1fff ), MRA16_BANK14 },	/* internal RAM, or external through DMOVLAY */
	{ ADSP_DATA_ADDR_RANGE( 0x2000, 0x3fdf ), MRA16_RAM },		/* internal RAM */
	{ ADSP_DATA_ADDR_RANGE( 0x3fe0, 0x3fff ), rax_control_r },	/* control registers */
	{ ADSP_PGM_ADDR_RANGE( 0x0000, 0x3fff ), MRA16_RAM },		/* internal program RAM */
MEMORY_END

static MEMORY_WRITE16_START( rax_writemem )
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

static void rax_init( int cpunum )
{
	rax_cpunum = cpunum;
	rax_rom = memory_region( REGION_SOUND1 );
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

static void rax_reset( void )
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
static void rax_data_w( UINT16 data )
{
	rax_data_in = data;
	cpu_set_irq_line( rax_cpunum, ADSP2181_IRQL0, ASSERT_LINE );
	cpu_boost_interleave( 0, TIME_IN_USEC( 5 ) );
}


/* the main board side */

static UINT8 *nbajamex_sram;			/* 71256 */
static UINT32 nbajamex_rombank[ 2 ];
static UINT8 nbajamex_bankmap;			/* 0: backup RAM at 0x1f200000, 1: ROM bank 1 there */

static void nbajamex_update_banks( void )
{
	UINT32 n_bank0 = ( ( nbajamex_rombank[ 0 ] >> 4 ) & 1 ) | ( ( nbajamex_rombank[ 0 ] & 7 ) << 1 );
	cpu_setbank( 1, memory_region( REGION_USER1 ) + n_bank0 * 0x200000 );
}

static READ32_HANDLER( nbajamex_rom1_r )
{
	UINT32 n_address = offset * 4;	/* from 0x1f200000 */

	if( !nbajamex_bankmap )
	{
		/* the backup RAM, bytes at byte addresses */
		if( n_address < 0x8000 )
			return nbajamex_sram[ n_address ] | ( nbajamex_sram[ n_address + 1 ] << 8 ) |
				( nbajamex_sram[ n_address + 2 ] << 16 ) | ( nbajamex_sram[ n_address + 3 ] << 24 );
		return 0;
	}
	else
	{
		UINT32 n_bank1 = ( ( ~nbajamex_rombank[ 1 ] >> 4 ) & 1 ) | ( ( nbajamex_rombank[ 1 ] & 7 ) << 1 );
		UINT32 n_rom = n_bank1 * 0x200000 + n_address;
		if( n_rom + 4 <= memory_region_length( REGION_USER1 ) )
			return *(UINT32 *)( memory_region( REGION_USER1 ) + n_rom );
		return 0;
	}
}

static WRITE32_HANDLER( nbajamex_backup_w )
{
	UINT32 n_address = offset * 4;	/* from 0x1f200000 */
	int n_lane;

	for( n_lane = 0; n_lane < 4; n_lane++ )
		if( !( mem_mask & ( 0xff << ( n_lane * 8 ) ) ) )
			nbajamex_sram[ n_address + n_lane ] = data >> ( n_lane * 8 );
}

static WRITE32_HANDLER( nbajamex_bank_w )
{
	/* a 16-bit register in each half */
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
		nbajamex_rombank[ 0 ] = data & 0xffff;
	if( ( mem_mask & 0xffff0000 ) != 0xffff0000 )
		nbajamex_rombank[ 1 ] = data >> 16;
	nbajamex_bankmap = ( nbajamex_rombank[ 0 ] != 0 );
	nbajamex_update_banks();
}

static READ32_HANDLER( nbajamex_sound_08_r )
{
	return 0x400;	/* sound related */
}

static READ32_HANDLER( nbajamex_sound_80_r )
{
	return 0xffff;
}

static WRITE32_HANDLER( nbajamex_sound_80_w )
{
	/* all 16 bits go to the latch of the sound board (see the sound test) */
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
		rax_data_w( data & 0xffff );
}

static MEMORY_READ32_START( nbajamex_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f1fffff, MRA32_BANK1 },  /* game rom, bank 0 */
	{ 0x1f200000, 0x1f7fffff, nbajamex_rom1_r },
	{ 0x1fbfff08, 0x1fbfff0b, nbajamex_sound_08_r },
	{ 0x1fbfff80, 0x1fbfff83, nbajamex_sound_80_r },
MEMORY_END

static MEMORY_WRITE32_START( nbajamex_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f1fffff, MWA32_ROM },
	{ 0x1f200000, 0x1f207fff, nbajamex_backup_w },
	{ 0x1f208000, 0x1f7fffff, MWA32_NOP },
	{ 0x1fbfff00, 0x1fbfff03, nbajamex_bank_w },
	{ 0x1fbfff10, 0x1fbfff13, MWA32_NOP },
	{ 0x1fbfff80, 0x1fbfff83, nbajamex_sound_80_w },
MEMORY_END

static NVRAM_HANDLER( nbajamex )
{
	nvram_handler_zn( file, read_or_write );
	if( read_or_write )
		mame_fwrite( file, nbajamex_sram, 0x8000 );
	else if( file )
		mame_fread( file, nbajamex_sram, 0x8000 );
	else
		memset( nbajamex_sram, 0xff, 0x8000 );
}

static void nbajamex_postload( void )
{
	nbajamex_update_banks();
}

static DRIVER_INIT( nbajamex )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	nbajamex_sram = auto_malloc( 0x8000 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );
	memset( nbajamex_sram, 0xff, 0x8000 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	zn_board_init();
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* AC01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* AC02 */

	rax_init( mame_find_cpu_index( "rax" ) );

	state_save_register_UINT8( "zn", 0, "nbajamex_sram", nbajamex_sram, 0x8000 );
	state_save_register_UINT32( "zn", 0, "nbajamex_rombank", nbajamex_rombank, 2 );
	state_save_register_UINT8( "zn", 0, "nbajamex_bankmap", &nbajamex_bankmap, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( nbajamex_postload );
}

static MACHINE_INIT( nbajamex )
{
	zn_board_reset();
	nbajamex_rombank[ 0 ] = nbajamex_rombank[ 1 ] = 0;
	nbajamex_bankmap = 0;
	nbajamex_update_banks();
	rax_reset();
}

/* ZN-1, 2MB VRAM (COH-1000A), SPU and the RAX sound board */
static MACHINE_DRIVER_START( nbajamex )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( nbajamex_readmem, nbajamex_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_CPU_ADD_TAG( "rax", ADSP2181, RAX_CLOCK )
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( rax_readmem, rax_writemem )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( nbajamex )
	MDRV_NVRAM_HANDLER( nbajamex )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
	MDRV_SOUND_ADD( CUSTOM, rax_custom_interface )
MACHINE_DRIVER_END


/***************************************************************************

  Video System (COH-1002V), as in current MAME

  ZN-1 main board with 2MB VRAM and the Video System BIOS; CAT702 KN01 on
  the main board, KN02 on the ROM board. Game ROM: 2.5MB fixed at
  0x1f000000, and 24 banks of 1MB at 0x1fb00000, selected by the byte
  written at 0x1fb00000. Sound: the PSX SPU only.

***************************************************************************/

static UINT32 visco_bank;

static void visco_bank_update( void )
{
	UINT32 n_banks = memory_region_length( REGION_USER2 ) / 0x100000;
	cpu_setbank( 13, memory_region( REGION_USER2 ) + ( visco_bank % n_banks ) * 0x100000 );
}

static WRITE32_HANDLER( visco_bank_w )	/* byte at 0x1fb00000 */
{
	if( ACCESSING_LSB32 )
	{
		visco_bank = data & 0xff;
		visco_bank_update();
	}
}

static MEMORY_READ32_START( visco_zn_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f27ffff, MRA32_BANK1 },  /* fixed game rom */
	{ 0x1fb00000, 0x1fbfffff, MRA32_BANK13 }, /* banked game rom */
MEMORY_END

static MEMORY_WRITE32_START( visco_zn_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f27ffff, MWA32_ROM },
	{ 0x1fb00000, 0x1fb00003, visco_bank_w },
MEMORY_END

static void visco_postload( void )
{
	visco_bank_update();
}

static DRIVER_INIT( coh1002v )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	zn_board_init();
	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* KN01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* KN02 */

	state_save_register_UINT32( "zn", 0, "visco_bank", &visco_bank, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( visco_postload );
}

static MACHINE_INIT( visco_zn )
{
	visco_bank = 0;
	visco_bank_update();
	zn_board_reset();
}

/* ZN-1, 2MB VRAM (COH-1002V), SPU sound */
static MACHINE_DRIVER_START( coh1002v )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( visco_zn_readmem, visco_zn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( visco_zn )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, zn_spu_interface )
MACHINE_DRIVER_END


/***************************************************************************

  Atlus (COH-1001L), as in current MAME

  ZN-1 main board with 2MB VRAM and the Atlus BIOS; CAT702 AT01 on the
  main board, AT02 on the ROM board. Game ROM: 4 banks of 8MB at
  0x1f000000, selected by the byte written at 0x1fb00002. Sound: a 68000
  at 10MHz with a YMZ280B, and the PSX SPU. The main CPU writes 16-bit
  commands to a latch at 0x1fb00000; a pending command holds 68000 IRQ 3,
  which the read of the latch clears. The YMZ280B interrupts on IRQ 2.

***************************************************************************/

static UINT16 atlus_latch;
static UINT8 atlus_latch_pending;

static void atlus_latch_irq_update( void )
{
	cpu_set_irq_line( 1, 3, atlus_latch_pending ? ASSERT_LINE : CLEAR_LINE );
}

static void atlus_latch_callback( int param )
{
	atlus_latch = param;
	atlus_latch_pending = 1;
	atlus_latch_irq_update();
}

static WRITE32_HANDLER( atlus_zn_w )	/* latch at 0x1fb00000, bank at 0x1fb00002 */
{
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
	{
		UINT16 n_latch = atlus_latch;
		n_latch = ( n_latch & ( mem_mask & 0xffff ) ) | ( data & ~mem_mask & 0xffff );
		/* the CPUs synchronize before the 68000 sees the command */
		timer_set( TIME_NOW, n_latch, atlus_latch_callback );
	}
	if( ( mem_mask & 0x00ff0000 ) == 0 )
	{
		tecmo_bank = ( data >> 16 ) & 3;
		tecmo_bank_update();
	}
}

static MEMORY_READ32_START( atlus_zn_readmem )
	ZN_MAIN_READ
	{ 0x1f000000, 0x1f7fffff, MRA32_BANK1 },  /* banked game rom */
MEMORY_END

static MEMORY_WRITE32_START( atlus_zn_writemem )
	ZN_MAIN_WRITE
	{ 0x1f000000, 0x1f7fffff, MWA32_ROM },
	{ 0x1fb00000, 0x1fb00003, atlus_zn_w },
MEMORY_END

static READ16_HANDLER( atlus_latch_r )
{
	atlus_latch_pending = 0;
	atlus_latch_irq_update();
	return atlus_latch;
}

/* the YMZ280B on the low byte of each word: register, then data / status */
static READ16_HANDLER( atlus_ymz280b_r )
{
	return offset ? YMZ280B_status_0_r( 0 ) : 0xff;
}

static WRITE16_HANDLER( atlus_ymz280b_w )
{
	if( ACCESSING_LSB16 )
	{
		if( offset )
			YMZ280B_data_0_w( 0, data & 0xff );
		else
			YMZ280B_register_0_w( 0, data & 0xff );
	}
}

static MEMORY_READ16_START( atlus_sound_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x100000, 0x100001, atlus_latch_r },
	{ 0x200000, 0x200003, atlus_ymz280b_r },
	{ 0x700000, 0x70ffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( atlus_sound_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x100000, 0x100001, MWA16_NOP },	/* irq ack maybe */
	{ 0x200000, 0x200003, atlus_ymz280b_w },
	{ 0x700000, 0x70ffff, MWA16_RAM },
MEMORY_END

static void atlus_ymz280b_irq( int state )
{
	cpu_set_irq_line( 1, 2, state ? ASSERT_LINE : CLEAR_LINE );
}

static struct YMZ280Binterface atlus_ymz280b_interface =
{
	1,
	{ 16934400 },
	{ REGION_SOUND1 },
	{ YM3012_VOL( 18, MIXER_PAN_LEFT, 18, MIXER_PAN_RIGHT ) },	/* 0.35 in MAME, whose YMZ280B is half as loud */
	{ atlus_ymz280b_irq }
};

/* the PSX SPU of the Atlus board (0.175 in MAME) */
static struct PSXSPUinterface atlus_spu_interface =
{
	18
};

static void atlus_postload( void )
{
	tecmo_bank_update();
	atlus_latch_irq_update();
}

static DRIVER_INIT( coh1001l )
{
	zn_scratchpad = auto_malloc( 0x400 );
	zn_eeprom = auto_malloc( 0x800 );
	memset( zn_scratchpad, 0, 0x400 );
	memset( zn_eeprom, 0xff, 0x800 );

	zn_vram_2mb = 1;
	zn_coin_tecmo = 0;
	zn_eeprom_default = 0;

	zn_board_init();
	znsec_init( 0, memory_region( REGION_USER4 ) );	/* AT01 */
	znsec_init( 1, memory_region( REGION_USER5 ) );	/* AT02 */

	state_save_register_UINT32( "zn", 0, "atlus_bank", &tecmo_bank, 1 );
	state_save_register_UINT16( "zn", 0, "atlus_latch", &atlus_latch, 1 );
	state_save_register_UINT8( "zn", 0, "atlus_latch_pending", &atlus_latch_pending, 1 );
	state_save_register_UINT32( "zn", 0, "scratchpad", zn_scratchpad, 0x100 );
	state_save_register_func_postload( atlus_postload );
}

static MACHINE_INIT( atlus_zn )
{
	tecmo_bank = 0;
	tecmo_bank_update();
	atlus_latch = 0;
	atlus_latch_pending = 0;
	zn_board_reset();
}

/* ZN-1, 2MB VRAM (COH-1001L), 68000 + YMZ280B sound */
static MACHINE_DRIVER_START( coh1001l )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 )	/* CXD8530CQ, 67.7376MHz XTAL; as System 11 */
	MDRV_CPU_MEMORY( atlus_zn_readmem, atlus_zn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_CPU_ADD( M68000, 10000000 )
	MDRV_CPU_FLAGS( CPU_AUDIO_CPU )
	MDRV_CPU_MEMORY( atlus_sound_readmem, atlus_sound_writemem )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 100 )

	MDRV_MACHINE_INIT( atlus_zn )
	MDRV_NVRAM_HANDLER( zn )

	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, atlus_spu_interface )
	MDRV_SOUND_ADD( YMZ280B, atlus_ymz280b_interface )
MACHINE_DRIVER_END


INPUT_PORTS_START( zn )
	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE )	/* pause */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE	)	/* pause */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2  )

	PORT_START		/* DSWA */
	PORT_DIPNAME( 0xff, 0xff, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0xff, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START		/* DSWB */
	PORT_DIPNAME( 0xff, 0xff, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0xff, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START		/* DSWC */
	PORT_DIPNAME( 0xff, 0xff, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0xff, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START		/* Player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )

	PORT_START		/* Player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
INPUT_PORTS_END


/* the main board inputs: P1, P2, SERVICE, SYSTEM, P3, P4, DSW */
#define ZN_DSW_PORT \
	PORT_START	/* DSW */ \
	PORT_DIPNAME( 0x01, 0x01, "Freeze" ) \
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Service_Mode ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

/* Tecmo TPS: the inputs of current MAME (zn, zn2p, zn1p, glpracr2) */
#define TPS_PLAYER_PORT( n, b3, b4 ) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER##n ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER##n ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, b3 ? ( IPT_BUTTON3 | IPF_PLAYER##n ) : IPT_UNUSED ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, b4 ? ( IPT_BUTTON4 | IPF_PLAYER##n ) : IPT_UNUSED )

#define TPS_UNUSED_PORT \
	PORT_START \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

#define TPS_SERVICE_PORT( b5 ) \
	PORT_START	/* SERVICE */ \
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, b5 ? ( IPT_BUTTON5 | IPF_PLAYER1 ) : IPT_UNUSED ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, b5 ? ( IPT_BUTTON5 | IPF_PLAYER2 ) : IPT_UNUSED ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, b5 ? ( IPT_BUTTON5 | IPF_PLAYER3 ) : IPT_UNUSED ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, b5 ? ( IPT_BUTTON5 | IPF_PLAYER4 ) : IPT_UNUSED )

#define TPS_SYSTEM_PORT( start2, coin2, four ) \
	PORT_START	/* SYSTEM */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, start2 ? IPT_START2 : IPT_UNUSED ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, four ? IPT_START3 : IPT_UNUSED ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, four ? IPT_START4 : IPT_UNUSED ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, coin2 ? IPT_COIN2 : IPT_UNUSED ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, four ? IPT_COIN3 : IPT_UNUSED ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, four ? IPT_COIN4 : IPT_UNUSED )

/* four players, four buttons */
INPUT_PORTS_START( tps )
	TPS_PLAYER_PORT( 1, 1, 1 )
	TPS_PLAYER_PORT( 2, 1, 1 )
	TPS_SERVICE_PORT( 1 )
	TPS_SYSTEM_PORT( 1, 1, 1 )
	TPS_PLAYER_PORT( 3, 1, 1 )
	TPS_PLAYER_PORT( 4, 1, 1 )
	ZN_DSW_PORT
INPUT_PORTS_END

/* two players, three buttons */
INPUT_PORTS_START( tps2p )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_PLAYER_PORT( 2, 1, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
INPUT_PORTS_END

/* one player, three buttons */
INPUT_PORTS_START( tps1p )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 0, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
INPUT_PORTS_END

/* Capcom: the inputs of current MAME (capcom6b, capcom4b, glpracr,
   plsmaswd, strider2, tgmj). The buttons 4 to 6 of each player are on
   the P3 and P4 inputs. */
#define CAPCOM_EXTRA_PORT( n, b1, b2, b3 ) \
	PORT_START \
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, b1 ? ( b1 | IPF_PLAYER##n ) : IPT_UNUSED ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, b2 ? ( b2 | IPF_PLAYER##n ) : IPT_UNUSED ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, b3 ? ( b3 | IPF_PLAYER##n ) : IPT_UNUSED ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

/* two players, six buttons */
INPUT_PORTS_START( capcom6b )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_PLAYER_PORT( 2, 1, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	CAPCOM_EXTRA_PORT( 1, IPT_BUTTON4, IPT_BUTTON5, IPT_BUTTON6 )
	CAPCOM_EXTRA_PORT( 2, IPT_BUTTON4, IPT_BUTTON5, IPT_BUTTON6 )
	ZN_DSW_PORT
INPUT_PORTS_END

/* two players, four buttons */
INPUT_PORTS_START( capcom4b )
	TPS_PLAYER_PORT( 1, 0, 0 )
	TPS_PLAYER_PORT( 2, 0, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	CAPCOM_EXTRA_PORT( 1, IPT_BUTTON3, IPT_BUTTON4, 0 )
	CAPCOM_EXTRA_PORT( 2, IPT_BUTTON3, IPT_BUTTON4, 0 )
	ZN_DSW_PORT
INPUT_PORTS_END

/* Gallop Racer: one player, two buttons */
INPUT_PORTS_START( glpracr )
	TPS_PLAYER_PORT( 1, 0, 0 )
	TPS_UNUSED_PORT
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 0, 0, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
INPUT_PORTS_END

/* two players, four buttons */
INPUT_PORTS_START( plsmaswd )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_PLAYER_PORT( 2, 1, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	CAPCOM_EXTRA_PORT( 1, IPT_BUTTON4, 0, 0 )
	CAPCOM_EXTRA_PORT( 2, IPT_BUTTON4, 0, 0 )
	ZN_DSW_PORT
INPUT_PORTS_END

/* Strider 2: one player, three buttons */
INPUT_PORTS_START( strider2 )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 0, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
INPUT_PORTS_END

/* Tetris The Grand Master: two players, 4-way joysticks, three buttons */
#define TGMJ_PLAYER_PORT( n ) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER##n ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER##n ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER##n ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER##n ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

INPUT_PORTS_START( tgmj )
	TGMJ_PLAYER_PORT( 1 )
	TGMJ_PLAYER_PORT( 2 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
INPUT_PORTS_END

/* Primal Rage 2: two players, six buttons; buttons 4 to 6 of both players
   on the P4 inputs */
INPUT_PORTS_START( primrag2 )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_PLAYER_PORT( 2, 1, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	PORT_START	/* P4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	ZN_DSW_PORT
INPUT_PORTS_END

/* Judge Dredd: two light guns, a trigger and a button each */
INPUT_PORTS_START( jdredd )
	PORT_START	/* P1 */
	PORT_BIT( 0xef, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_START	/* P2 */
	PORT_BIT( 0xef, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_START	/* SERVICE */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )	/* gun mux, from the driver */
	PORT_BIT( 0x30, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
	PORT_START	/* GUN1X */
	PORT_ANALOG( 0xffff, 0x0822, IPT_LIGHTGUN_X | IPF_PLAYER1, 100, 50, 0x392, 0xcb3 )
	PORT_START	/* GUN1Y */
	PORT_ANALOG( 0xffff, 0x0122, IPT_LIGHTGUN_Y | IPF_PLAYER1, 100, 15, 0x02c, 0x218 )
	PORT_START	/* GUN2X */
	PORT_ANALOG( 0xffff, 0x0822, IPT_LIGHTGUN_X | IPF_PLAYER2, 100, 50, 0x392, 0xcb3 )
	PORT_START	/* GUN2Y */
	PORT_ANALOG( 0xffff, 0x0122, IPT_LIGHTGUN_Y | IPF_PLAYER2, 100, 15, 0x02c, 0x218 )
INPUT_PORTS_END

/* four players, four buttons */
INPUT_PORTS_START( nbajamex )
	TPS_PLAYER_PORT( 1, 1, 1 )
	TPS_PLAYER_PORT( 2, 1, 1 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 1 )
	TPS_PLAYER_PORT( 3, 1, 1 )
	TPS_PLAYER_PORT( 4, 1, 1 )
	ZN_DSW_PORT
INPUT_PORTS_END

/* Taito FX: DSW 4 is the test mode; the 4 bits not fitted read as 0 */
#define TAITO_FX_DSW_PORT \
	PORT_START	/* DSW */ \
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Service_Mode ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x08, 0x08, "Test Mode" ) \
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

/* Aero Fighters Special: two players, two buttons; DSW 3 test mode, 4 save */
INPUT_PORTS_START( aerofgts )
	TPS_PLAYER_PORT( 1, 0, 0 )
	TPS_PLAYER_PORT( 2, 0, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Test Mode" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Save" )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

/* Bloody Roar 2: two players, four buttons; the fourth on the P3 port */
INPUT_PORTS_START( bldyror2 )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_PLAYER_PORT( 2, 1, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	PORT_START	/* P3 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
	TPS_UNUSED_PORT
	TAITO_FX_DSW_PORT
INPUT_PORTS_END

/* Taito FX-1A: two players, three buttons */
INPUT_PORTS_START( znt2p )
	TPS_PLAYER_PORT( 1, 1, 0 )
	TPS_PLAYER_PORT( 2, 1, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	TAITO_FX_DSW_PORT
INPUT_PORTS_END

/* Taito FX-1B RayStorm: two players, two buttons */
INPUT_PORTS_START( raystorm )
	TPS_PLAYER_PORT( 1, 0, 0 )
	TPS_PLAYER_PORT( 2, 0, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 1, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	TAITO_FX_DSW_PORT
INPUT_PORTS_END

/* two players, two buttons, one coin slot */
INPUT_PORTS_START( glpracr2 )
	TPS_PLAYER_PORT( 1, 0, 0 )
	TPS_PLAYER_PORT( 2, 0, 0 )
	TPS_SERVICE_PORT( 0 )
	TPS_SYSTEM_PORT( 1, 0, 0 )
	TPS_UNUSED_PORT
	TPS_UNUSED_PORT
	ZN_DSW_PORT
INPUT_PORTS_END


/* Capcom ZN-1 and ZN-2, as in current MAME: the BIOS, its CAT702 (CP01 or
   CP10) and the game sets with their CAT702 (CPnn) */

#define CAPCOM_ZN1_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m534002c-13.ic353", 0x000000, 0x080000, CRC(50033af6) SHA1(486d92ff6c7f1e54f8e0ef41cd9116eca0e10e1a) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "cp01.ic652", 0x000000, 0x000008, CRC(bb7ba033) SHA1(966f3aee5d00e1312c455f855915d621e6cb0672) )

#define CAPCOM_ZN2_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m534002c-59.ic353", 0x000000, 0x080000, CRC(e860ea8b) SHA1(66e7e1d4e426466b8f48a2ba055a91b475569504) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "cp10.ic652", 0x000000, 0x000008, CRC(7808fcef) SHA1(3c3fb8227b05dace042dc1429b9f2355b1a15638) )

ROM_START( coh1000c )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, ROMREGION_ERASEFF )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, ROMREGION_ERASEFF )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASEFF )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_COPY( REGION_USER4, 0x000000, 0x000000, 0x000008 )
ROM_END

ROM_START( ts2 )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ts2e_04.2h", 0x0000000, 0x080000, CRC(7a3b535f) SHA1(f17f0dad81e50a7f4ccd042a26af1d472df4785f) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ts2-05m.3h", 0x0000000, 0x400000, CRC(7f4228e2) SHA1(3690a76d19d97e55bc7b05a8456328697cfd7a77) )
	ROM_LOAD( "ts2-06m.4h", 0x0400000, 0x200000, CRC(67be6797) SHA1(521f69439ba7666f54d1008a291f3391f3a16499) )
	ROM_LOAD( "ts2-07m.5h", 0x0600000, 0x200000, CRC(db73e2b8) SHA1(239243f44c41df765789c14cc4036bb02e2ab373) )
	ROM_LOAD( "ts2-08m.2k", 0x0800000, 0x200000, CRC(01a48431) SHA1(6e395af726da91909e07dac25bb9b70b3ccebd4e) )
	ROM_LOAD( "ts2-09m.3k", 0x0a00000, 0x200000, CRC(83f408de) SHA1(415787c4dca604dd5611e16936a0ffa981dedf78) )
	ROM_LOAD( "ts2-10m.4k", 0x0c00000, 0x200000, CRC(ad90679a) SHA1(19dd30764f892ee7f89c78ccbccdaf4d6b0e6e09) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ts2_02.2e", 0x000000, 0x008000, CRC(2f45c461) SHA1(513b6b9b5a2f7c567c30c958e0e13834cd9bd266) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ts2-01m.3b", 0x000000, 0x400000, CRC(d7a505e0) SHA1(f1b0cdea712101f695bd326eccd753eb79a07490) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp02", 0x000000, 0x000008, CRC(40b0b697) SHA1(a8ee10fc9c0257de0a9ed0db29ea08afe900405f) )
ROM_END

ROM_START( ts2u )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ts2u_04.2h", 0x0000000, 0x080000, CRC(ddb52e7c) SHA1(e77891abae7681d911ef6eba2e0920d81433ebe6) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ts2-05m.3h", 0x0000000, 0x400000, CRC(7f4228e2) SHA1(3690a76d19d97e55bc7b05a8456328697cfd7a77) )
	ROM_LOAD( "ts2-06m.4h", 0x0400000, 0x200000, CRC(67be6797) SHA1(521f69439ba7666f54d1008a291f3391f3a16499) )
	ROM_LOAD( "ts2-07m.5h", 0x0600000, 0x200000, CRC(db73e2b8) SHA1(239243f44c41df765789c14cc4036bb02e2ab373) )
	ROM_LOAD( "ts2-08m.2k", 0x0800000, 0x200000, CRC(01a48431) SHA1(6e395af726da91909e07dac25bb9b70b3ccebd4e) )
	ROM_LOAD( "ts2-09m.3k", 0x0a00000, 0x200000, CRC(83f408de) SHA1(415787c4dca604dd5611e16936a0ffa981dedf78) )
	ROM_LOAD( "ts2-10m.4k", 0x0c00000, 0x200000, CRC(ad90679a) SHA1(19dd30764f892ee7f89c78ccbccdaf4d6b0e6e09) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ts2_02.2e", 0x000000, 0x008000, CRC(2f45c461) SHA1(513b6b9b5a2f7c567c30c958e0e13834cd9bd266) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ts2-01m.3b", 0x000000, 0x400000, CRC(d7a505e0) SHA1(f1b0cdea712101f695bd326eccd753eb79a07490) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp02", 0x000000, 0x000008, CRC(40b0b697) SHA1(a8ee10fc9c0257de0a9ed0db29ea08afe900405f) )
ROM_END

ROM_START( ts2ua )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ts2u_04.2h", 0x0000000, 0x080000, CRC(ddb52e7c) SHA1(e77891abae7681d911ef6eba2e0920d81433ebe6) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ts2-05m.3h", 0x0000000, 0x400000, CRC(7f4228e2) SHA1(3690a76d19d97e55bc7b05a8456328697cfd7a77) )
	ROM_LOAD( "ts2-06m",    0x0400000, 0x400000, CRC(cd7e0a27) SHA1(325b5f2e653cdea07cddc9d20d12b5ab50dca949) )
	ROM_LOAD( "ts2-08m",    0x0800000, 0x400000, CRC(b1f7f115) SHA1(3f416d2aac07aa73a99593b5a21b047da60cea6a) )
	ROM_LOAD( "ts2-10m.4k", 0x0c00000, 0x200000, CRC(ad90679a) SHA1(19dd30764f892ee7f89c78ccbccdaf4d6b0e6e09) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ts2_02.2e", 0x000000, 0x008000, CRC(2f45c461) SHA1(513b6b9b5a2f7c567c30c958e0e13834cd9bd266) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ts2-01m.3b", 0x000000, 0x400000, CRC(d7a505e0) SHA1(f1b0cdea712101f695bd326eccd753eb79a07490) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp02", 0x000000, 0x000008, CRC(40b0b697) SHA1(a8ee10fc9c0257de0a9ed0db29ea08afe900405f) )
ROM_END

ROM_START( ts2j )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ts2j_04.2h", 0x0000000, 0x080000, CRC(4aba8c5e) SHA1(a56001bf50bfc1b03036e88ae1febd1aac8c63c0) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ts2-05m.3h", 0x0000000, 0x400000, CRC(7f4228e2) SHA1(3690a76d19d97e55bc7b05a8456328697cfd7a77) )
	ROM_LOAD( "ts2-06m.4h", 0x0400000, 0x200000, CRC(67be6797) SHA1(521f69439ba7666f54d1008a291f3391f3a16499) )
	ROM_LOAD( "ts2-07m.5h", 0x0600000, 0x200000, CRC(db73e2b8) SHA1(239243f44c41df765789c14cc4036bb02e2ab373) )
	ROM_LOAD( "ts2-08m.2k", 0x0800000, 0x200000, CRC(01a48431) SHA1(6e395af726da91909e07dac25bb9b70b3ccebd4e) )
	ROM_LOAD( "ts2-09m.3k", 0x0a00000, 0x200000, CRC(83f408de) SHA1(415787c4dca604dd5611e16936a0ffa981dedf78) )
	ROM_LOAD( "ts2-10m.4k", 0x0c00000, 0x200000, CRC(ad90679a) SHA1(19dd30764f892ee7f89c78ccbccdaf4d6b0e6e09) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ts2_02.2e", 0x000000, 0x008000, CRC(2f45c461) SHA1(513b6b9b5a2f7c567c30c958e0e13834cd9bd266) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ts2-01m.3b", 0x000000, 0x400000, CRC(d7a505e0) SHA1(f1b0cdea712101f695bd326eccd753eb79a07490) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp02", 0x000000, 0x000008, CRC(40b0b697) SHA1(a8ee10fc9c0257de0a9ed0db29ea08afe900405f) )
ROM_END

ROM_START( ts2ja )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ts2j_04.2h", 0x0000000, 0x080000, CRC(4aba8c5e) SHA1(a56001bf50bfc1b03036e88ae1febd1aac8c63c0) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ts2-05m.3h", 0x0000000, 0x400000, CRC(7f4228e2) SHA1(3690a76d19d97e55bc7b05a8456328697cfd7a77) )
	ROM_LOAD( "ts2-06m",    0x0400000, 0x400000, CRC(cd7e0a27) SHA1(325b5f2e653cdea07cddc9d20d12b5ab50dca949) )
	ROM_LOAD( "ts2-08m",    0x0800000, 0x400000, CRC(b1f7f115) SHA1(3f416d2aac07aa73a99593b5a21b047da60cea6a) )
	ROM_LOAD( "ts2-10m.4k", 0x0c00000, 0x200000, CRC(ad90679a) SHA1(19dd30764f892ee7f89c78ccbccdaf4d6b0e6e09) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ts2_02.2e", 0x000000, 0x008000, CRC(2f45c461) SHA1(513b6b9b5a2f7c567c30c958e0e13834cd9bd266) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ts2-01m.3b", 0x000000, 0x400000, CRC(d7a505e0) SHA1(f1b0cdea712101f695bd326eccd753eb79a07490) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp02", 0x000000, 0x000008, CRC(40b0b697) SHA1(a8ee10fc9c0257de0a9ed0db29ea08afe900405f) )
ROM_END

ROM_START( starglad )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ps1u_04.2h", 0x0000000, 0x080000, CRC(121fb234) SHA1(697d18d37afd95f302b40a5a6a78d8c92a41ea73) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ps1-05m.3h", 0x0000000, 0x400000, CRC(8ad72c4f) SHA1(c848c37eb5365000b4d4720b5c08d89ddd8e2c33) )
	ROM_LOAD( "ps1-06m.4h", 0x0400000, 0x400000, CRC(95d8ed61) SHA1(e9f259d589dc38a8321a6fea1f5dac741cadc0ff) )
	ROM_LOAD( "ps1-07m.5h", 0x0800000, 0x400000, CRC(c06752db) SHA1(0884b308e9cd9dde8660b422bc8fec9a362bcb52) )
	ROM_LOAD( "ps1-08m.2k", 0x0c00000, 0x400000, CRC(381f9ded) SHA1(b7878a90740f5b3c5881ac7d46e2b84b18727337) )
	ROM_LOAD( "ps1-09m.3k", 0x1000000, 0x400000, CRC(bd894812) SHA1(9f0c3365e685a53ae793f4a256a6c177a843a424) )
	ROM_LOAD( "ps1-10m.4k", 0x1400000, 0x400000, CRC(ff80c18a) SHA1(8d01717eed6ec1f508fe7c445da941fb84ef7d22) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ps1_02a.2e", 0x000000, 0x008000, CRC(b854df92) SHA1(ea71a613b5b19ec7e9c6e342e7743d320582a6bb) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "ps1_03a.3e", 0x028000, 0x020000, CRC(a2562fbb) SHA1(3de02a4aa7ea620961ca2a5c331f38134033db79) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ps1-01m.3b", 0x000000, 0x400000, CRC(0bfb17aa) SHA1(cf4482785a2a33ad814c8b1461c5bc8e8e027895) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp03", 0x000000, 0x000008, CRC(27970400) SHA1(b240e3587d39315840c7471a0e97d7cd8275886c) )
ROM_END

ROM_START( stargladj )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ps1j_04.2h", 0x0000000, 0x080000, CRC(f865006d) SHA1(6abb476777a4309b1a60f12e6313c051db195808) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "ps1-05m.3h", 0x0000000, 0x400000, CRC(8ad72c4f) SHA1(c848c37eb5365000b4d4720b5c08d89ddd8e2c33) )
	ROM_LOAD( "ps1-06m.4h", 0x0400000, 0x400000, CRC(95d8ed61) SHA1(e9f259d589dc38a8321a6fea1f5dac741cadc0ff) )
	ROM_LOAD( "ps1-07m.5h", 0x0800000, 0x400000, CRC(c06752db) SHA1(0884b308e9cd9dde8660b422bc8fec9a362bcb52) )
	ROM_LOAD( "ps1-08m.2k", 0x0c00000, 0x400000, CRC(381f9ded) SHA1(b7878a90740f5b3c5881ac7d46e2b84b18727337) )
	ROM_LOAD( "ps1-09m.3k", 0x1000000, 0x400000, CRC(bd894812) SHA1(9f0c3365e685a53ae793f4a256a6c177a843a424) )
	ROM_LOAD( "ps1-10m.4k", 0x1400000, 0x400000, CRC(ff80c18a) SHA1(8d01717eed6ec1f508fe7c445da941fb84ef7d22) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ps1_02a.2e", 0x000000, 0x008000, CRC(b854df92) SHA1(ea71a613b5b19ec7e9c6e342e7743d320582a6bb) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "ps1_03a.3e", 0x028000, 0x020000, CRC(a2562fbb) SHA1(3de02a4aa7ea620961ca2a5c331f38134033db79) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ps1-01m.3b", 0x000000, 0x400000, CRC(0bfb17aa) SHA1(cf4482785a2a33ad814c8b1461c5bc8e8e027895) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp03", 0x000000, 0x000008, CRC(27970400) SHA1(b240e3587d39315840c7471a0e97d7cd8275886c) )
ROM_END

ROM_START( glpracr )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "gra_ep-eng.2h", 0x0000000, 0x080000, CRC(f3ab9c85) SHA1(ce9d5d0406a6854975d5c71935fe917706334429) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "gra-05m.3h", 0x0000000, 0x400000, CRC(78053700) SHA1(38727c8cc34bb57b7b7e73041e382fb0361f184e) )
	ROM_LOAD( "gra-06m.4h", 0x0400000, 0x400000, CRC(d73b392b) SHA1(241ddf474cea035e81a2abc580d3c0395ee925bb) )
	ROM_LOAD( "gra-07m.5h", 0x0800000, 0x400000, CRC(acaefe3a) SHA1(32d596b0f975e1558fa7929c3166d8dad40a1c80) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASEFF )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp05", 0x000000, 0x000008, CRC(ae930900) SHA1(01fd03487943e21e3634285ef50107c6c2c4f6ca) )
ROM_END

ROM_START( glpracrj )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "gpaj_04.2h", 0x0000000, 0x080000, CRC(53bf551c) SHA1(320632b5010630cee4c5ccb1578d5ee6d2754632) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "gra-05m.3h", 0x0000000, 0x400000, CRC(78053700) SHA1(38727c8cc34bb57b7b7e73041e382fb0361f184e) )
	ROM_LOAD( "gra-06m.4h", 0x0400000, 0x400000, CRC(d73b392b) SHA1(241ddf474cea035e81a2abc580d3c0395ee925bb) )
	ROM_LOAD( "gra-07m.5h", 0x0800000, 0x400000, CRC(acaefe3a) SHA1(32d596b0f975e1558fa7929c3166d8dad40a1c80) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASEFF )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp05", 0x000000, 0x000008, CRC(ae930900) SHA1(01fd03487943e21e3634285ef50107c6c2c4f6ca) )
ROM_END

ROM_START( sfex )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfee_04a.2h", 0x0000000, 0x080000, CRC(092cfa2e) SHA1(8af38a3f4f89f661233995a672faf486e71b79bc) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfe-05m.3h", 0x0000000, 0x400000, CRC(eab781fe) SHA1(205476cb72c8dac915e140fb32243dfc5d209ba4) )
	ROM_LOAD( "sfe-06m.4h", 0x0400000, 0x400000, CRC(999de60c) SHA1(092882698c411fc5c3bcb43105bf1886f94b8e40) )
	ROM_LOAD( "sfe-07m.5h", 0x0800000, 0x400000, CRC(76117b0a) SHA1(027233199170fa6e5b32f28da2031638c6d3d14a) )
	ROM_LOAD( "sfe-08m.2k", 0x0c00000, 0x400000, CRC(a36bbec5) SHA1(fa22ea50d4d8bed2ded97a346f61b2f5f68769b9) )
	ROM_LOAD( "sfe-09m.3k", 0x1000000, 0x400000, CRC(62c424cc) SHA1(ea19c49b486473b150dbf8541286e225655496db) )
	ROM_LOAD( "sfe-10m.4k", 0x1400000, 0x400000, CRC(83791a8b) SHA1(534969797640834ca692c11d0ce7c3a060fc7e4b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexu )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfeu_04b.2h", 0x0000000, 0x080000, CRC(de02bd29) SHA1(62a88a30f73db661f5b98fc7e2d34d51acb965cc) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfe-05m.3h", 0x0000000, 0x400000, CRC(eab781fe) SHA1(205476cb72c8dac915e140fb32243dfc5d209ba4) )
	ROM_LOAD( "sfe-06m.4h", 0x0400000, 0x400000, CRC(999de60c) SHA1(092882698c411fc5c3bcb43105bf1886f94b8e40) )
	ROM_LOAD( "sfe-07m.5h", 0x0800000, 0x400000, CRC(76117b0a) SHA1(027233199170fa6e5b32f28da2031638c6d3d14a) )
	ROM_LOAD( "sfe-08m.2k", 0x0c00000, 0x400000, CRC(a36bbec5) SHA1(fa22ea50d4d8bed2ded97a346f61b2f5f68769b9) )
	ROM_LOAD( "sfe-09m.3k", 0x1000000, 0x400000, CRC(62c424cc) SHA1(ea19c49b486473b150dbf8541286e225655496db) )
	ROM_LOAD( "sfe-10m.4k", 0x1400000, 0x400000, CRC(83791a8b) SHA1(534969797640834ca692c11d0ce7c3a060fc7e4b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexa )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfea_04a.2h", 0x0000000, 0x080000, CRC(08247bd4) SHA1(07f356ef2827b3fbd0bfaf2010915315d9d60ef1) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfe-05m.3h", 0x0000000, 0x400000, CRC(eab781fe) SHA1(205476cb72c8dac915e140fb32243dfc5d209ba4) )
	ROM_LOAD( "sfe-06m.4h", 0x0400000, 0x400000, CRC(999de60c) SHA1(092882698c411fc5c3bcb43105bf1886f94b8e40) )
	ROM_LOAD( "sfe-07m.5h", 0x0800000, 0x400000, CRC(76117b0a) SHA1(027233199170fa6e5b32f28da2031638c6d3d14a) )
	ROM_LOAD( "sfe-08m.2k", 0x0c00000, 0x400000, CRC(a36bbec5) SHA1(fa22ea50d4d8bed2ded97a346f61b2f5f68769b9) )
	ROM_LOAD( "sfe-09m.3k", 0x1000000, 0x400000, CRC(62c424cc) SHA1(ea19c49b486473b150dbf8541286e225655496db) )
	ROM_LOAD( "sfe-10m.4k", 0x1400000, 0x400000, CRC(83791a8b) SHA1(534969797640834ca692c11d0ce7c3a060fc7e4b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexj )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfej_04.2h", 0x0000000, 0x080000, CRC(ea100607) SHA1(27ef8c619804999d32d14fcc5ec783c057b4dc73) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfe-05m.3h", 0x0000000, 0x400000, CRC(eab781fe) SHA1(205476cb72c8dac915e140fb32243dfc5d209ba4) )
	ROM_LOAD( "sfe-06m.4h", 0x0400000, 0x400000, CRC(999de60c) SHA1(092882698c411fc5c3bcb43105bf1886f94b8e40) )
	ROM_LOAD( "sfe-07m.5h", 0x0800000, 0x400000, CRC(76117b0a) SHA1(027233199170fa6e5b32f28da2031638c6d3d14a) )
	ROM_LOAD( "sfe-08m.2k", 0x0c00000, 0x400000, CRC(a36bbec5) SHA1(fa22ea50d4d8bed2ded97a346f61b2f5f68769b9) )
	ROM_LOAD( "sfe-09m.3k", 0x1000000, 0x400000, CRC(62c424cc) SHA1(ea19c49b486473b150dbf8541286e225655496db) )
	ROM_LOAD( "sfe-10m.4k", 0x1400000, 0x400000, CRC(83791a8b) SHA1(534969797640834ca692c11d0ce7c3a060fc7e4b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexp )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfpu_04a.2h", 0x0000000, 0x080000, CRC(4617adc2) SHA1(200307904349ad7e5d7d76d8c904b6b10424c7ef) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfp-05m.3h", 0x0000000, 0x400000, CRC(ac7dcc5e) SHA1(216de2de691a9bd7982d5d6b5b1e3e35ff381a2f) )
	ROM_LOAD( "sfp-06m.4h", 0x0400000, 0x400000, CRC(1d504758) SHA1(bd56141aba35dbb5b318445ba5db12eff7442221) )
	ROM_LOAD( "sfp-07m.5h", 0x0800000, 0x400000, CRC(0f585f30) SHA1(24ffdbc360f8eddb702905c99d315614327861a7) )
	ROM_LOAD( "sfp-08m.2k", 0x0c00000, 0x400000, CRC(65eabc61) SHA1(bbeb3bcd8dd8f7f88ed82412a81134a3d6f6ffd9) )
	ROM_LOAD( "sfp-09m.3k", 0x1000000, 0x400000, CRC(15f8b71e) SHA1(efb28fbe750f443550ee9718385355aae7e858c9) )
	ROM_LOAD( "sfp-10m.4k", 0x1400000, 0x400000, CRC(c1ecf652) SHA1(616e14ff63d38272730c810b933a6b3412e2da17) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexpu1 )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfpu_04.2h", 0x0000000, 0x080000, CRC(305e4ec0) SHA1(0df9572d7fc1bbc7131483960771d016fa5487a5) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfp-05m.3h", 0x0000000, 0x400000, CRC(ac7dcc5e) SHA1(216de2de691a9bd7982d5d6b5b1e3e35ff381a2f) )
	ROM_LOAD( "sfp-06m.4h", 0x0400000, 0x400000, CRC(1d504758) SHA1(bd56141aba35dbb5b318445ba5db12eff7442221) )
	ROM_LOAD( "sfp-07m.5h", 0x0800000, 0x400000, CRC(0f585f30) SHA1(24ffdbc360f8eddb702905c99d315614327861a7) )
	ROM_LOAD( "sfp-08m.2k", 0x0c00000, 0x400000, CRC(65eabc61) SHA1(bbeb3bcd8dd8f7f88ed82412a81134a3d6f6ffd9) )
	ROM_LOAD( "sfp-09m.3k", 0x1000000, 0x400000, CRC(15f8b71e) SHA1(efb28fbe750f443550ee9718385355aae7e858c9) )
	ROM_LOAD( "sfp-10m.4k", 0x1400000, 0x400000, CRC(c1ecf652) SHA1(616e14ff63d38272730c810b933a6b3412e2da17) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexpj )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfpj_04a.2h", 0x0000000, 0x080000, CRC(6e99a0f7) SHA1(8f22bc545dd0e3eff24ab62ce5af1998d48d3770) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfp-05m.3h", 0x0000000, 0x400000, CRC(ac7dcc5e) SHA1(216de2de691a9bd7982d5d6b5b1e3e35ff381a2f) )
	ROM_LOAD( "sfp-06m.4h", 0x0400000, 0x400000, CRC(1d504758) SHA1(bd56141aba35dbb5b318445ba5db12eff7442221) )
	ROM_LOAD( "sfp-07m.5h", 0x0800000, 0x400000, CRC(0f585f30) SHA1(24ffdbc360f8eddb702905c99d315614327861a7) )
	ROM_LOAD( "sfp-08m.2k", 0x0c00000, 0x400000, CRC(65eabc61) SHA1(bbeb3bcd8dd8f7f88ed82412a81134a3d6f6ffd9) )
	ROM_LOAD( "sfp-09m.3k", 0x1000000, 0x400000, CRC(15f8b71e) SHA1(efb28fbe750f443550ee9718385355aae7e858c9) )
	ROM_LOAD( "sfp-10m.4k", 0x1400000, 0x400000, CRC(c1ecf652) SHA1(616e14ff63d38272730c810b933a6b3412e2da17) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( sfexpj1 )
	CAPCOM_ZN1_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sfpj_04.2h", 0x0000000, 0x080000, CRC(18d043f5) SHA1(9e6e24a722d13888fbfd391ddb1a5045b162488c) )

	ROM_REGION32_LE( 0x2400000, REGION_USER1, 0 )
	ROM_LOAD( "sfp-05m.3h", 0x0000000, 0x400000, CRC(ac7dcc5e) SHA1(216de2de691a9bd7982d5d6b5b1e3e35ff381a2f) )
	ROM_LOAD( "sfp-06m.4h", 0x0400000, 0x400000, CRC(1d504758) SHA1(bd56141aba35dbb5b318445ba5db12eff7442221) )
	ROM_LOAD( "sfp-07m.5h", 0x0800000, 0x400000, CRC(0f585f30) SHA1(24ffdbc360f8eddb702905c99d315614327861a7) )
	ROM_LOAD( "sfp-08m.2k", 0x0c00000, 0x400000, CRC(65eabc61) SHA1(bbeb3bcd8dd8f7f88ed82412a81134a3d6f6ffd9) )
	ROM_LOAD( "sfp-09m.3k", 0x1000000, 0x400000, CRC(15f8b71e) SHA1(efb28fbe750f443550ee9718385355aae7e858c9) )
	ROM_LOAD( "sfp-10m.4k", 0x1400000, 0x400000, CRC(c1ecf652) SHA1(616e14ff63d38272730c810b933a6b3412e2da17) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sfe_02.2e", 0x000000, 0x008000, CRC(1908475c) SHA1(99f68cff2d92f5697eec0846201f6fb317d5dc08) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sfe_03.3e", 0x028000, 0x020000, CRC(95c1e2e0) SHA1(383bbe9613798a3ac6944d18768280a840994e40) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sfe-01m.3b", 0x000000, 0x400000, CRC(f5afff0d) SHA1(7f9ac32ba0a3d9c6fef367e36a92d47c9ac1feb3) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp04", 0x000000, 0x000008, CRC(e0dc24ae) SHA1(17d6e3dc11308195f4c46a6cd8093db9eaf584e5) )
ROM_END

ROM_START( coh3002c )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, ROMREGION_ERASEFF )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, ROMREGION_ERASEFF )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASEFF )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_COPY( REGION_USER4, 0x000000, 0x000000, 0x000008 )
ROM_END

ROM_START( rvschool )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "jste_04.2h", 0x0000000, 0x080000, CRC(1567555a) SHA1(3b93235076ab3c06914c83becf0da8e810b8917a) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "jst-05m.3h", 0x0000000, 0x400000, CRC(723372b8) SHA1(2a7c95d1f9a3f58c469dfc28ead1fd192eaaebd1) )
	ROM_LOAD( "jst-06m.4h", 0x0400000, 0x400000, CRC(4248988e) SHA1(4bdf7cac17d70ea85aa2002fc6b21a64d05e6e5a) )
	ROM_LOAD( "jst-07m.5h", 0x0800000, 0x400000, CRC(c84c5a16) SHA1(5c0ca7454189c766f1ca7305504ff1867007c8e6) )
	ROM_LOAD( "jst-08m.2k", 0x0c00000, 0x400000, CRC(791b57f3) SHA1(4ea12a0f7a7110d7dcbc55b3f02aa9a92dea4b12) )
	ROM_LOAD( "jst-09m.3k", 0x1000000, 0x400000, CRC(6df42048) SHA1(9e2b4a424de3918e5e54bc87fd9dcceff8d162be) )
	ROM_LOAD( "jst-10m.4k", 0x1400000, 0x400000, CRC(d7e22769) SHA1(733f96dce2586fc0a8af3cec18153085750c9a4d) )
	ROM_LOAD( "jst-11m.5k", 0x1800000, 0x400000, CRC(0a033ac5) SHA1(218b33cb51db99d3e9ee180da6a74460f4444fc6) )
	ROM_LOAD( "jst-12m.6k", 0x1c00000, 0x400000, CRC(43bd2ddd) SHA1(7f2976e394362cb648f620e430b3bf11b71485a6) )
	ROM_LOAD( "jst-13m.7k", 0x2000000, 0x400000, CRC(6b443235) SHA1(c764d8b742aa1c46bc8d37f36e864ef50a1ff4e4) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "jst_02.2e", 0x000000, 0x008000, CRC(7809e2c3) SHA1(0216a665f7978bc8db3f7fdab038e1c7aa120844) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "jst_03.3e", 0x028000, 0x020000, CRC(860ff24d) SHA1(eea72fa5eaf407a112a5b3daf60f7ac8ad191cc7) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "jst-01m.3b", 0x000000, 0x400000, CRC(9a7c98f9) SHA1(764c6c4f41047e1f36d2dceac4aa9b943a9d529a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp06", 0x000000, 0x000008, CRC(99b22128) SHA1(9a773927ead72ed4ded44d53d89ecb123e1d3f17) )
ROM_END

ROM_START( rvschoolu )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "jstu_04.2h", 0x0000000, 0x080000, CRC(d83724ae) SHA1(0890c0164116606acc600f646e82972d0d4f79b4) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "jst-05m.3h", 0x0000000, 0x400000, CRC(723372b8) SHA1(2a7c95d1f9a3f58c469dfc28ead1fd192eaaebd1) )
	ROM_LOAD( "jst-06m.4h", 0x0400000, 0x400000, CRC(4248988e) SHA1(4bdf7cac17d70ea85aa2002fc6b21a64d05e6e5a) )
	ROM_LOAD( "jst-07m.5h", 0x0800000, 0x400000, CRC(c84c5a16) SHA1(5c0ca7454189c766f1ca7305504ff1867007c8e6) )
	ROM_LOAD( "jst-08m.2k", 0x0c00000, 0x400000, CRC(791b57f3) SHA1(4ea12a0f7a7110d7dcbc55b3f02aa9a92dea4b12) )
	ROM_LOAD( "jst-09m.3k", 0x1000000, 0x400000, CRC(6df42048) SHA1(9e2b4a424de3918e5e54bc87fd9dcceff8d162be) )
	ROM_LOAD( "jst-10m.4k", 0x1400000, 0x400000, CRC(d7e22769) SHA1(733f96dce2586fc0a8af3cec18153085750c9a4d) )
	ROM_LOAD( "jst-11m.5k", 0x1800000, 0x400000, CRC(0a033ac5) SHA1(218b33cb51db99d3e9ee180da6a74460f4444fc6) )
	ROM_LOAD( "jst-12m.6k", 0x1c00000, 0x400000, CRC(43bd2ddd) SHA1(7f2976e394362cb648f620e430b3bf11b71485a6) )
	ROM_LOAD( "jst-13m.7k", 0x2000000, 0x400000, CRC(6b443235) SHA1(c764d8b742aa1c46bc8d37f36e864ef50a1ff4e4) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "jst_02.2e", 0x000000, 0x008000, CRC(7809e2c3) SHA1(0216a665f7978bc8db3f7fdab038e1c7aa120844) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "jst_03.3e", 0x028000, 0x020000, CRC(860ff24d) SHA1(eea72fa5eaf407a112a5b3daf60f7ac8ad191cc7) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "jst-01m.3b", 0x000000, 0x400000, CRC(9a7c98f9) SHA1(764c6c4f41047e1f36d2dceac4aa9b943a9d529a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp06", 0x000000, 0x000008, CRC(99b22128) SHA1(9a773927ead72ed4ded44d53d89ecb123e1d3f17) )
ROM_END

ROM_START( rvschoola )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "jsta_04.2h", 0x0000000, 0x080000, CRC(034b1011) SHA1(6773246be242ee336503d21d7d44a3884832eb1e) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "jst-05m.3h", 0x0000000, 0x400000, CRC(723372b8) SHA1(2a7c95d1f9a3f58c469dfc28ead1fd192eaaebd1) )
	ROM_LOAD( "jst-06m.4h", 0x0400000, 0x400000, CRC(4248988e) SHA1(4bdf7cac17d70ea85aa2002fc6b21a64d05e6e5a) )
	ROM_LOAD( "jst-07m.5h", 0x0800000, 0x400000, CRC(c84c5a16) SHA1(5c0ca7454189c766f1ca7305504ff1867007c8e6) )
	ROM_LOAD( "jst-08m.2k", 0x0c00000, 0x400000, CRC(791b57f3) SHA1(4ea12a0f7a7110d7dcbc55b3f02aa9a92dea4b12) )
	ROM_LOAD( "jst-09m.3k", 0x1000000, 0x400000, CRC(6df42048) SHA1(9e2b4a424de3918e5e54bc87fd9dcceff8d162be) )
	ROM_LOAD( "jst-10m.4k", 0x1400000, 0x400000, CRC(d7e22769) SHA1(733f96dce2586fc0a8af3cec18153085750c9a4d) )
	ROM_LOAD( "jst-11m.5k", 0x1800000, 0x400000, CRC(0a033ac5) SHA1(218b33cb51db99d3e9ee180da6a74460f4444fc6) )
	ROM_LOAD( "jst-12m.6k", 0x1c00000, 0x400000, CRC(43bd2ddd) SHA1(7f2976e394362cb648f620e430b3bf11b71485a6) )
	ROM_LOAD( "jst-13m.7k", 0x2000000, 0x400000, CRC(6b443235) SHA1(c764d8b742aa1c46bc8d37f36e864ef50a1ff4e4) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "jst_02.2e", 0x000000, 0x008000, CRC(7809e2c3) SHA1(0216a665f7978bc8db3f7fdab038e1c7aa120844) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "jst_03.3e", 0x028000, 0x020000, CRC(860ff24d) SHA1(eea72fa5eaf407a112a5b3daf60f7ac8ad191cc7) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "jst-01m.3b", 0x000000, 0x400000, CRC(9a7c98f9) SHA1(764c6c4f41047e1f36d2dceac4aa9b943a9d529a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp06", 0x000000, 0x000008, CRC(99b22128) SHA1(9a773927ead72ed4ded44d53d89ecb123e1d3f17) )
ROM_END

ROM_START( jgakuen )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "jstj_04a.2h", 0x0000000, 0x080000, CRC(91b36f38) SHA1(423dbf0f9f08458a866e9acb2a5b657c736b179f) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "jst-05m.3h", 0x0000000, 0x400000, CRC(723372b8) SHA1(2a7c95d1f9a3f58c469dfc28ead1fd192eaaebd1) )
	ROM_LOAD( "jst-06m.4h", 0x0400000, 0x400000, CRC(4248988e) SHA1(4bdf7cac17d70ea85aa2002fc6b21a64d05e6e5a) )
	ROM_LOAD( "jst-07m.5h", 0x0800000, 0x400000, CRC(c84c5a16) SHA1(5c0ca7454189c766f1ca7305504ff1867007c8e6) )
	ROM_LOAD( "jst-08m.2k", 0x0c00000, 0x400000, CRC(791b57f3) SHA1(4ea12a0f7a7110d7dcbc55b3f02aa9a92dea4b12) )
	ROM_LOAD( "jst-09m.3k", 0x1000000, 0x400000, CRC(6df42048) SHA1(9e2b4a424de3918e5e54bc87fd9dcceff8d162be) )
	ROM_LOAD( "jst-10m.4k", 0x1400000, 0x400000, CRC(d7e22769) SHA1(733f96dce2586fc0a8af3cec18153085750c9a4d) )
	ROM_LOAD( "jst-11m.5k", 0x1800000, 0x400000, CRC(0a033ac5) SHA1(218b33cb51db99d3e9ee180da6a74460f4444fc6) )
	ROM_LOAD( "jst-12m.6k", 0x1c00000, 0x400000, CRC(43bd2ddd) SHA1(7f2976e394362cb648f620e430b3bf11b71485a6) )
	ROM_LOAD( "jst-13m.7k", 0x2000000, 0x400000, CRC(6b443235) SHA1(c764d8b742aa1c46bc8d37f36e864ef50a1ff4e4) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "jst_02.2e", 0x000000, 0x008000, CRC(7809e2c3) SHA1(0216a665f7978bc8db3f7fdab038e1c7aa120844) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "jst_03.3e", 0x028000, 0x020000, CRC(860ff24d) SHA1(eea72fa5eaf407a112a5b3daf60f7ac8ad191cc7) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "jst-01m.3b", 0x000000, 0x400000, CRC(9a7c98f9) SHA1(764c6c4f41047e1f36d2dceac4aa9b943a9d529a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp06", 0x000000, 0x000008, CRC(99b22128) SHA1(9a773927ead72ed4ded44d53d89ecb123e1d3f17) )
ROM_END

ROM_START( jgakuen1 )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "jstj_04.2h", 0x0000000, 0x080000, CRC(28b8000a) SHA1(9ebf74b453d775cadca9c2d7d8e2c7eb57bb9a38) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "jst-05m.3h", 0x0000000, 0x400000, CRC(723372b8) SHA1(2a7c95d1f9a3f58c469dfc28ead1fd192eaaebd1) )
	ROM_LOAD( "jst-06m.4h", 0x0400000, 0x400000, CRC(4248988e) SHA1(4bdf7cac17d70ea85aa2002fc6b21a64d05e6e5a) )
	ROM_LOAD( "jst-07m.5h", 0x0800000, 0x400000, CRC(c84c5a16) SHA1(5c0ca7454189c766f1ca7305504ff1867007c8e6) )
	ROM_LOAD( "jst-08m.2k", 0x0c00000, 0x400000, CRC(791b57f3) SHA1(4ea12a0f7a7110d7dcbc55b3f02aa9a92dea4b12) )
	ROM_LOAD( "jst-09m.3k", 0x1000000, 0x400000, CRC(6df42048) SHA1(9e2b4a424de3918e5e54bc87fd9dcceff8d162be) )
	ROM_LOAD( "jst-10m.4k", 0x1400000, 0x400000, CRC(d7e22769) SHA1(733f96dce2586fc0a8af3cec18153085750c9a4d) )
	ROM_LOAD( "jst-11m.5k", 0x1800000, 0x400000, CRC(0a033ac5) SHA1(218b33cb51db99d3e9ee180da6a74460f4444fc6) )
	ROM_LOAD( "jst-12m.6k", 0x1c00000, 0x400000, CRC(43bd2ddd) SHA1(7f2976e394362cb648f620e430b3bf11b71485a6) )
	ROM_LOAD( "jst-13m.7k", 0x2000000, 0x400000, CRC(6b443235) SHA1(c764d8b742aa1c46bc8d37f36e864ef50a1ff4e4) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "jst_02.2e", 0x000000, 0x008000, CRC(7809e2c3) SHA1(0216a665f7978bc8db3f7fdab038e1c7aa120844) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "jst_03.3e", 0x028000, 0x020000, CRC(860ff24d) SHA1(eea72fa5eaf407a112a5b3daf60f7ac8ad191cc7) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "jst-01m.3b", 0x000000, 0x400000, CRC(9a7c98f9) SHA1(764c6c4f41047e1f36d2dceac4aa9b943a9d529a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp06", 0x000000, 0x000008, CRC(99b22128) SHA1(9a773927ead72ed4ded44d53d89ecb123e1d3f17) )
ROM_END

ROM_START( sfex2 )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ex2e_04.2h", 0x0000000, 0x080000, CRC(77e1622b) SHA1(626a6718e7e843b09075b652d4fcbd9c86eea02b) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ex2-05m.3h", 0x0000000, 0x800000, CRC(78726b17) SHA1(2da449df335ef133ebc3997bbad73ef4137f4771) )
	ROM_LOAD( "ex2-06m.4h", 0x0800000, 0x800000, CRC(be1075ed) SHA1(36dc673372f30f8b3ff5689ae568c5cd01fe2c07) )
	ROM_LOAD( "ex2-07m.5h", 0x1000000, 0x800000, CRC(6496c6ed) SHA1(054bcecbb04033abea14d9ffe6634b2bd11ca88b) )
	ROM_LOAD( "ex2-08m.2k", 0x1800000, 0x800000, CRC(3194132e) SHA1(d1324fcf0a8528fc683791d6342697a7e08674f4) )
	ROM_LOAD( "ex2-09m.3k", 0x2000000, 0x400000, CRC(075ae585) SHA1(6b88851db618fc3e96f1d740c46c1bc5be0ee21b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ex2_02.2e", 0x000000, 0x008000, CRC(9489875e) SHA1(1fc9985ff98232c63ea8d05a69f7d77cdf72919f) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ex2-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp08", 0x000000, 0x000008, CRC(a63d6fa6) SHA1(68995438a1e90ff9aa59090e7e031d51c68c4d73) )
ROM_END

ROM_START( sfex2u )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ex2u_04a.2h", 0x0000000, 0x080000, CRC(8dc5317f) SHA1(c35224caf70662a0e45a74cbead294a51f9b9e16) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ex2-05m.3h", 0x0000000, 0x800000, CRC(78726b17) SHA1(2da449df335ef133ebc3997bbad73ef4137f4771) )
	ROM_LOAD( "ex2-06m.4h", 0x0800000, 0x800000, CRC(be1075ed) SHA1(36dc673372f30f8b3ff5689ae568c5cd01fe2c07) )
	ROM_LOAD( "ex2-07m.5h", 0x1000000, 0x800000, CRC(6496c6ed) SHA1(054bcecbb04033abea14d9ffe6634b2bd11ca88b) )
	ROM_LOAD( "ex2-08m.2k", 0x1800000, 0x800000, CRC(3194132e) SHA1(d1324fcf0a8528fc683791d6342697a7e08674f4) )
	ROM_LOAD( "ex2-09m.3k", 0x2000000, 0x400000, CRC(075ae585) SHA1(6b88851db618fc3e96f1d740c46c1bc5be0ee21b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ex2_02.2e", 0x000000, 0x008000, CRC(9489875e) SHA1(1fc9985ff98232c63ea8d05a69f7d77cdf72919f) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ex2-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp08", 0x000000, 0x000008, CRC(a63d6fa6) SHA1(68995438a1e90ff9aa59090e7e031d51c68c4d73) )
ROM_END

ROM_START( sfex2u1 )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ex2u_04.2h", 0x0000000, 0x080000, CRC(eb417c2c) SHA1(bd47d0a1d911af4457c795ec348c847c2cf9e721) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ex2-05m.3h", 0x0000000, 0x800000, CRC(78726b17) SHA1(2da449df335ef133ebc3997bbad73ef4137f4771) )
	ROM_LOAD( "ex2-06m.4h", 0x0800000, 0x800000, CRC(be1075ed) SHA1(36dc673372f30f8b3ff5689ae568c5cd01fe2c07) )
	ROM_LOAD( "ex2-07m.5h", 0x1000000, 0x800000, CRC(6496c6ed) SHA1(054bcecbb04033abea14d9ffe6634b2bd11ca88b) )
	ROM_LOAD( "ex2-08m.2k", 0x1800000, 0x800000, CRC(3194132e) SHA1(d1324fcf0a8528fc683791d6342697a7e08674f4) )
	ROM_LOAD( "ex2-09m.3k", 0x2000000, 0x400000, CRC(075ae585) SHA1(6b88851db618fc3e96f1d740c46c1bc5be0ee21b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ex2_02.2e", 0x000000, 0x008000, CRC(9489875e) SHA1(1fc9985ff98232c63ea8d05a69f7d77cdf72919f) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ex2-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp08", 0x000000, 0x000008, CRC(a63d6fa6) SHA1(68995438a1e90ff9aa59090e7e031d51c68c4d73) )
ROM_END

ROM_START( sfex2a )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ex2a_04.2h", 0x0000000, 0x080000, CRC(ac9a872d) SHA1(4e237f5e2e5de58e587e9abc5767509b8d750004) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ex2-05m.3h", 0x0000000, 0x800000, CRC(78726b17) SHA1(2da449df335ef133ebc3997bbad73ef4137f4771) )
	ROM_LOAD( "ex2-06m.4h", 0x0800000, 0x800000, CRC(be1075ed) SHA1(36dc673372f30f8b3ff5689ae568c5cd01fe2c07) )
	ROM_LOAD( "ex2-07m.5h", 0x1000000, 0x800000, CRC(6496c6ed) SHA1(054bcecbb04033abea14d9ffe6634b2bd11ca88b) )
	ROM_LOAD( "ex2-08m.2k", 0x1800000, 0x800000, CRC(3194132e) SHA1(d1324fcf0a8528fc683791d6342697a7e08674f4) )
	ROM_LOAD( "ex2-09m.3k", 0x2000000, 0x400000, CRC(075ae585) SHA1(6b88851db618fc3e96f1d740c46c1bc5be0ee21b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ex2_02.2e", 0x000000, 0x008000, CRC(9489875e) SHA1(1fc9985ff98232c63ea8d05a69f7d77cdf72919f) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ex2-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp08", 0x000000, 0x000008, CRC(a63d6fa6) SHA1(68995438a1e90ff9aa59090e7e031d51c68c4d73) )
ROM_END

ROM_START( sfex2h )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ex2h_04.2h", 0x0000000, 0x080000, CRC(68f2ef80) SHA1(212bb3c0d935c64f5e3b20e427e06d97404709d8) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ex2-05m.3h", 0x0000000, 0x800000, CRC(78726b17) SHA1(2da449df335ef133ebc3997bbad73ef4137f4771) )
	ROM_LOAD( "ex2-06m.4h", 0x0800000, 0x800000, CRC(be1075ed) SHA1(36dc673372f30f8b3ff5689ae568c5cd01fe2c07) )
	ROM_LOAD( "ex2-07m.5h", 0x1000000, 0x800000, CRC(6496c6ed) SHA1(054bcecbb04033abea14d9ffe6634b2bd11ca88b) )
	ROM_LOAD( "ex2-08m.2k", 0x1800000, 0x800000, CRC(3194132e) SHA1(d1324fcf0a8528fc683791d6342697a7e08674f4) )
	ROM_LOAD( "ex2-09m.3k", 0x2000000, 0x400000, CRC(075ae585) SHA1(6b88851db618fc3e96f1d740c46c1bc5be0ee21b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ex2_02.2e", 0x000000, 0x008000, CRC(9489875e) SHA1(1fc9985ff98232c63ea8d05a69f7d77cdf72919f) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ex2-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp08", 0x000000, 0x000008, CRC(a63d6fa6) SHA1(68995438a1e90ff9aa59090e7e031d51c68c4d73) )
ROM_END

ROM_START( sfex2j )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "ex2j_04.2h", 0x0000000, 0x080000, CRC(5d603586) SHA1(ff546d3bd011d6441e9672b88bab763d3cd89be2) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ex2-05m.3h", 0x0000000, 0x800000, CRC(78726b17) SHA1(2da449df335ef133ebc3997bbad73ef4137f4771) )
	ROM_LOAD( "ex2-06m.4h", 0x0800000, 0x800000, CRC(be1075ed) SHA1(36dc673372f30f8b3ff5689ae568c5cd01fe2c07) )
	ROM_LOAD( "ex2-07m.5h", 0x1000000, 0x800000, CRC(6496c6ed) SHA1(054bcecbb04033abea14d9ffe6634b2bd11ca88b) )
	ROM_LOAD( "ex2-08m.2k", 0x1800000, 0x800000, CRC(3194132e) SHA1(d1324fcf0a8528fc683791d6342697a7e08674f4) )
	ROM_LOAD( "ex2-09m.3k", 0x2000000, 0x400000, CRC(075ae585) SHA1(6b88851db618fc3e96f1d740c46c1bc5be0ee21b) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ex2_02.2e", 0x000000, 0x008000, CRC(9489875e) SHA1(1fc9985ff98232c63ea8d05a69f7d77cdf72919f) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ex2-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp08", 0x000000, 0x000008, CRC(a63d6fa6) SHA1(68995438a1e90ff9aa59090e7e031d51c68c4d73) )
ROM_END

ROM_START( plsmaswd )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sg2u_04.2h", 0x0000000, 0x080000, CRC(154187c0) SHA1(58cc0e9d32786b1c1d64ecee4667190456b36ef6) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "sg2-05m.3h", 0x0000000, 0x800000, CRC(f1759236) SHA1(fbe3a820a8c571dfb186eae68346e6461168ed48) )
	ROM_LOAD( "sg2-06m.4h", 0x0800000, 0x800000, CRC(33de4f72) SHA1(ab32af76b5682e3d9f67dadbaed35abc043912b4) )
	ROM_LOAD( "sg2-07m.5h", 0x1000000, 0x800000, CRC(72f724ba) SHA1(e6658b495d308d1de6710f87b5b9d346008b0c5a) )
	ROM_LOAD( "sg2-08m.2k", 0x1800000, 0x800000, CRC(9e169eee) SHA1(6141b1a7863fdfb200ca35d2893979a34dcc3f6c) )
	ROM_LOAD( "sg2-09m.3k", 0x2000000, 0x400000, CRC(33f73d4c) SHA1(954695a43e77b58585409678bd87c76adac1d855) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sg2_02.2e", 0x000000, 0x008000, CRC(415ee138) SHA1(626083c8705f012552691c450f95401ddc88065b) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sg2_03.3e", 0x028000, 0x020000, CRC(43806735) SHA1(88d389bcc79cbd4fa1f4b62008e171a897e77652) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sg2-01m.3a", 0x000000, 0x400000, CRC(643ea27b) SHA1(40747432d5cfebac54d3824b6a6f26b5e7742fc1) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp07", 0x000000, 0x000008, CRC(4c1acdc2) SHA1(8c0b4ae95d6ba89e4bdf9fce5e2605c2728ae67f) )
ROM_END

ROM_START( plsmaswda )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sg2a_04.2h", 0x0000000, 0x080000, CRC(66e5dada) SHA1(f2e50ee963b8a6aadf25a17b3ff6dcb428b8bdb2) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "sg2-05m.3h", 0x0000000, 0x800000, CRC(f1759236) SHA1(fbe3a820a8c571dfb186eae68346e6461168ed48) )
	ROM_LOAD( "sg2-06m.4h", 0x0800000, 0x800000, CRC(33de4f72) SHA1(ab32af76b5682e3d9f67dadbaed35abc043912b4) )
	ROM_LOAD( "sg2-07m.5h", 0x1000000, 0x800000, CRC(72f724ba) SHA1(e6658b495d308d1de6710f87b5b9d346008b0c5a) )
	ROM_LOAD( "sg2-08m.2k", 0x1800000, 0x800000, CRC(9e169eee) SHA1(6141b1a7863fdfb200ca35d2893979a34dcc3f6c) )
	ROM_LOAD( "sg2-09m.3k", 0x2000000, 0x400000, CRC(33f73d4c) SHA1(954695a43e77b58585409678bd87c76adac1d855) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sg2_02.2e", 0x000000, 0x008000, CRC(415ee138) SHA1(626083c8705f012552691c450f95401ddc88065b) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sg2_03.3e", 0x028000, 0x020000, CRC(43806735) SHA1(88d389bcc79cbd4fa1f4b62008e171a897e77652) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sg2-01m.3a", 0x000000, 0x400000, CRC(643ea27b) SHA1(40747432d5cfebac54d3824b6a6f26b5e7742fc1) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp07", 0x000000, 0x000008, CRC(4c1acdc2) SHA1(8c0b4ae95d6ba89e4bdf9fce5e2605c2728ae67f) )
ROM_END

ROM_START( stargld2 )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "sg2j_04.2h", 0x0000000, 0x080000, CRC(cf4ce6ac) SHA1(52b6f61d79671c9c108b3dfbd3c2ac333285412c) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "sg2-05m.3h", 0x0000000, 0x800000, CRC(f1759236) SHA1(fbe3a820a8c571dfb186eae68346e6461168ed48) )
	ROM_LOAD( "sg2-06m.4h", 0x0800000, 0x800000, CRC(33de4f72) SHA1(ab32af76b5682e3d9f67dadbaed35abc043912b4) )
	ROM_LOAD( "sg2-07m.5h", 0x1000000, 0x800000, CRC(72f724ba) SHA1(e6658b495d308d1de6710f87b5b9d346008b0c5a) )
	ROM_LOAD( "sg2-08m.2k", 0x1800000, 0x800000, CRC(9e169eee) SHA1(6141b1a7863fdfb200ca35d2893979a34dcc3f6c) )
	ROM_LOAD( "sg2-09m.3k", 0x2000000, 0x400000, CRC(33f73d4c) SHA1(954695a43e77b58585409678bd87c76adac1d855) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "sg2_02.2e", 0x000000, 0x008000, CRC(415ee138) SHA1(626083c8705f012552691c450f95401ddc88065b) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "sg2_03.3e", 0x028000, 0x020000, CRC(43806735) SHA1(88d389bcc79cbd4fa1f4b62008e171a897e77652) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "sg2-01m.3a", 0x000000, 0x400000, CRC(643ea27b) SHA1(40747432d5cfebac54d3824b6a6f26b5e7742fc1) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp07", 0x000000, 0x000008, CRC(4c1acdc2) SHA1(8c0b4ae95d6ba89e4bdf9fce5e2605c2728ae67f) )
ROM_END

ROM_START( tgmj )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "atej_04.2h", 0x0000000, 0x080000, CRC(bb4bbb96) SHA1(808f4b29493e74efd661d561d11cbec2f4afd1c8) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "ate-05m.3h", 0x0000000, 0x400000, CRC(50977f5a) SHA1(78c2b1965957ff1756c25b76e549f11fc0001153) )
	ROM_LOAD( "ate-06m.4h", 0x0400000, 0x400000, CRC(05973f16) SHA1(c9262e8de14c4a9489f7050316012913c1caf0ff) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "ate_02.2e", 0x000000, 0x008000, CRC(f4f6e82f) SHA1(ad6c49197a60f456367c9f78353741fb847819a1) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "ate-01m.3a", 0x000000, 0x400000, CRC(a21c6521) SHA1(560e4855f6e00def5277bdd12064b49e55c3b46b) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp11", 0x000000, 0x000008, CRC(22d77c59) SHA1(8e837a2f1b0c146ff835f20f4b9f6f9eb7e5ff04) )
ROM_END

ROM_START( techromn )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "kioe_04.2h", 0x0000000, 0x080000, CRC(ebd33b09) SHA1(3f0226d275efc7b97c8d3431211f948aa1271d34) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "kio-05m.3h", 0x0000000, 0x800000, CRC(98e9eb24) SHA1(144773296c213ab09d626c915f90bb74e24487f0) )
	ROM_LOAD( "kio-06m.4h", 0x0800000, 0x800000, CRC(be8d7d73) SHA1(bcbbbd0b83503f2ed32527444e0da3afd774d3f7) )
	ROM_LOAD( "kio-07m.5h", 0x1000000, 0x800000, CRC(ffd81f18) SHA1(f8387a9d45e79f97ccdffabe755638a60f80ccf5) )
	ROM_LOAD( "kio-08m.2k", 0x1800000, 0x800000, CRC(17302226) SHA1(976ba7f48c9a52d24388cd63d02be08627cf2e30) )
	ROM_LOAD( "kio-09m.3k", 0x2000000, 0x800000, CRC(a34f2119) SHA1(50fa992eba5324a173fcc0923227c13cad4f97e5) )
	ROM_LOAD( "kio-10m.4k", 0x2800000, 0x800000, CRC(7400037a) SHA1(d58641e1d6bf1c6ca04f6c98d6809edaa7df75d3) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "kio_02.2e", 0x000000, 0x008000, CRC(174309b3) SHA1(b35b9c3905d2fabaa8410f70f7b382e916c89733) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "kio_03.3e", 0x028000, 0x020000, CRC(0b313ae5) SHA1(0ea39305ca30f376930e39b134fd1a52200624fa) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "kio-01m.3a", 0x000000, 0x400000, CRC(6dc5bd07) SHA1(e1755a48465f741691ea0fa1166cb2dc09210ed9) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp09", 0x000000, 0x000008, CRC(02939f83) SHA1(a2a4d5218609c28da8175647cfb5114064d3265e) )
ROM_END

ROM_START( techromnu )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "kiou_04.2h", 0x0000000, 0x080000, CRC(08aca34a) SHA1(768a37f719af5d96993db5592b6505b013e0d6f4) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "kio-05m.3h", 0x0000000, 0x800000, CRC(98e9eb24) SHA1(144773296c213ab09d626c915f90bb74e24487f0) )
	ROM_LOAD( "kio-06m.4h", 0x0800000, 0x800000, CRC(be8d7d73) SHA1(bcbbbd0b83503f2ed32527444e0da3afd774d3f7) )
	ROM_LOAD( "kio-07m.5h", 0x1000000, 0x800000, CRC(ffd81f18) SHA1(f8387a9d45e79f97ccdffabe755638a60f80ccf5) )
	ROM_LOAD( "kio-08m.2k", 0x1800000, 0x800000, CRC(17302226) SHA1(976ba7f48c9a52d24388cd63d02be08627cf2e30) )
	ROM_LOAD( "kio-09m.3k", 0x2000000, 0x800000, CRC(a34f2119) SHA1(50fa992eba5324a173fcc0923227c13cad4f97e5) )
	ROM_LOAD( "kio-10m.4k", 0x2800000, 0x800000, CRC(7400037a) SHA1(d58641e1d6bf1c6ca04f6c98d6809edaa7df75d3) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "kio_02.2e", 0x000000, 0x008000, CRC(174309b3) SHA1(b35b9c3905d2fabaa8410f70f7b382e916c89733) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "kio_03.3e", 0x028000, 0x020000, CRC(0b313ae5) SHA1(0ea39305ca30f376930e39b134fd1a52200624fa) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "kio-01m.3a", 0x000000, 0x400000, CRC(6dc5bd07) SHA1(e1755a48465f741691ea0fa1166cb2dc09210ed9) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp09", 0x000000, 0x000008, CRC(02939f83) SHA1(a2a4d5218609c28da8175647cfb5114064d3265e) )
ROM_END

ROM_START( techromna )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "kioa_04.2h", 0x0000000, 0x080000, CRC(8ff43381) SHA1(7ce81e70fdc593b01a56e5e8b7d1de8000fe9453) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "kio-05m.3h", 0x0000000, 0x800000, CRC(98e9eb24) SHA1(144773296c213ab09d626c915f90bb74e24487f0) )
	ROM_LOAD( "kio-06m.4h", 0x0800000, 0x800000, CRC(be8d7d73) SHA1(bcbbbd0b83503f2ed32527444e0da3afd774d3f7) )
	ROM_LOAD( "kio-07m.5h", 0x1000000, 0x800000, CRC(ffd81f18) SHA1(f8387a9d45e79f97ccdffabe755638a60f80ccf5) )
	ROM_LOAD( "kio-08m.2k", 0x1800000, 0x800000, CRC(17302226) SHA1(976ba7f48c9a52d24388cd63d02be08627cf2e30) )
	ROM_LOAD( "kio-09m.3k", 0x2000000, 0x800000, CRC(a34f2119) SHA1(50fa992eba5324a173fcc0923227c13cad4f97e5) )
	ROM_LOAD( "kio-10m.4k", 0x2800000, 0x800000, CRC(7400037a) SHA1(d58641e1d6bf1c6ca04f6c98d6809edaa7df75d3) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "kio_02.2e", 0x000000, 0x008000, CRC(174309b3) SHA1(b35b9c3905d2fabaa8410f70f7b382e916c89733) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "kio_03.3e", 0x028000, 0x020000, CRC(0b313ae5) SHA1(0ea39305ca30f376930e39b134fd1a52200624fa) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "kio-01m.3a", 0x000000, 0x400000, CRC(6dc5bd07) SHA1(e1755a48465f741691ea0fa1166cb2dc09210ed9) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp09", 0x000000, 0x000008, CRC(02939f83) SHA1(a2a4d5218609c28da8175647cfb5114064d3265e) )
ROM_END

ROM_START( kikaioh )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "kioj_04.2h", 0x0000000, 0x080000, CRC(3a2a3bc8) SHA1(3c4ae3cfe00a7f60ab2196ae042dab4a8eb6f597) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "kio-05m.3h", 0x0000000, 0x800000, CRC(98e9eb24) SHA1(144773296c213ab09d626c915f90bb74e24487f0) )
	ROM_LOAD( "kio-06m.4h", 0x0800000, 0x800000, CRC(be8d7d73) SHA1(bcbbbd0b83503f2ed32527444e0da3afd774d3f7) )
	ROM_LOAD( "kio-07m.5h", 0x1000000, 0x800000, CRC(ffd81f18) SHA1(f8387a9d45e79f97ccdffabe755638a60f80ccf5) )
	ROM_LOAD( "kio-08m.2k", 0x1800000, 0x800000, CRC(17302226) SHA1(976ba7f48c9a52d24388cd63d02be08627cf2e30) )
	ROM_LOAD( "kio-09m.3k", 0x2000000, 0x800000, CRC(a34f2119) SHA1(50fa992eba5324a173fcc0923227c13cad4f97e5) )
	ROM_LOAD( "kio-10m.4k", 0x2800000, 0x800000, CRC(7400037a) SHA1(d58641e1d6bf1c6ca04f6c98d6809edaa7df75d3) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "kio_02.2e", 0x000000, 0x008000, CRC(174309b3) SHA1(b35b9c3905d2fabaa8410f70f7b382e916c89733) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "kio_03.3e", 0x028000, 0x020000, CRC(0b313ae5) SHA1(0ea39305ca30f376930e39b134fd1a52200624fa) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "kio-01m.3a", 0x000000, 0x400000, CRC(6dc5bd07) SHA1(e1755a48465f741691ea0fa1166cb2dc09210ed9) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp09", 0x000000, 0x000008, CRC(02939f83) SHA1(a2a4d5218609c28da8175647cfb5114064d3265e) )
ROM_END

ROM_START( sfex2p )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "x2pe_04.2h", 0x0000000, 0x080000, CRC(2cbd44ff) SHA1(9f32e447b811df8a8fa8d28d11204fc8ca7f93f8) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "x2p-05m.3h", 0x0000000, 0x800000, CRC(4ee3110f) SHA1(704f8dca7d0b698659af9e3271ea5072dfd42b8b) )
	ROM_LOAD( "x2p-06m.4h", 0x0800000, 0x800000, CRC(4cd53a45) SHA1(39499ea6c9aa51c71f4fe44cc02f93d5a39e14ec) )
	ROM_LOAD( "x2p-07m.5h", 0x1000000, 0x800000, CRC(11207c2a) SHA1(0182652819f1c3a36e7b42e34ef86d2455a2dd90) )
	ROM_LOAD( "x2p-08m.2k", 0x1800000, 0x800000, CRC(3560c2cc) SHA1(8b0ce22d954387f7bb032b5220d1014ef68741e8) )
	ROM_LOAD( "x2p-09m.3k", 0x2000000, 0x800000, CRC(344aa227) SHA1(69dc6f511939bf7fa25c2531ecf307a7565fe7a8) )
	ROM_LOAD( "x2p-10m.4k", 0x2800000, 0x800000, CRC(2eef5931) SHA1(e5227529fb68eeb1b2f25813694173a75d906b52) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "x2p_02.2e", 0x000000, 0x008000, CRC(3705de5e) SHA1(847007ca271da64bf13ffbf496d4291429eee27a) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "x2p_03.3e", 0x028000, 0x020000, CRC(6ae828f6) SHA1(41c54165e87b846a845da581f408b96979288158) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "x2p-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp12", 0x000000, 0x000008, CRC(7cc2ed68) SHA1(a409ae837665700bdc4e3aa7c41a418d5b792940) )
ROM_END

ROM_START( sfex2pu )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "x2pu_04.2h", 0x0000000, 0x080000, CRC(2938118c) SHA1(4bdeeb9aa3dd54ef44aa3fc73d78d65297b1ed25) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "x2p-05m.3h", 0x0000000, 0x800000, CRC(4ee3110f) SHA1(704f8dca7d0b698659af9e3271ea5072dfd42b8b) )
	ROM_LOAD( "x2p-06m.4h", 0x0800000, 0x800000, CRC(4cd53a45) SHA1(39499ea6c9aa51c71f4fe44cc02f93d5a39e14ec) )
	ROM_LOAD( "x2p-07m.5h", 0x1000000, 0x800000, CRC(11207c2a) SHA1(0182652819f1c3a36e7b42e34ef86d2455a2dd90) )
	ROM_LOAD( "x2p-08m.2k", 0x1800000, 0x800000, CRC(3560c2cc) SHA1(8b0ce22d954387f7bb032b5220d1014ef68741e8) )
	ROM_LOAD( "x2p-09m.3k", 0x2000000, 0x800000, CRC(344aa227) SHA1(69dc6f511939bf7fa25c2531ecf307a7565fe7a8) )
	ROM_LOAD( "x2p-10m.4k", 0x2800000, 0x800000, CRC(2eef5931) SHA1(e5227529fb68eeb1b2f25813694173a75d906b52) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "x2p_02.2e", 0x000000, 0x008000, CRC(3705de5e) SHA1(847007ca271da64bf13ffbf496d4291429eee27a) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "x2p_03.3e", 0x028000, 0x020000, CRC(6ae828f6) SHA1(41c54165e87b846a845da581f408b96979288158) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "x2p-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp12", 0x000000, 0x000008, CRC(7cc2ed68) SHA1(a409ae837665700bdc4e3aa7c41a418d5b792940) )
ROM_END

ROM_START( sfex2pa )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "x2pa_04.2h", 0x0000000, 0x080000, CRC(c437d602) SHA1(150f0dfd9f2e4f9adc11f8960da1e6be250456b1) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "x2p-05m.3h", 0x0000000, 0x800000, CRC(4ee3110f) SHA1(704f8dca7d0b698659af9e3271ea5072dfd42b8b) )
	ROM_LOAD( "x2p-06m.4h", 0x0800000, 0x800000, CRC(4cd53a45) SHA1(39499ea6c9aa51c71f4fe44cc02f93d5a39e14ec) )
	ROM_LOAD( "x2p-07m.5h", 0x1000000, 0x800000, CRC(11207c2a) SHA1(0182652819f1c3a36e7b42e34ef86d2455a2dd90) )
	ROM_LOAD( "x2p-08m.2k", 0x1800000, 0x800000, CRC(3560c2cc) SHA1(8b0ce22d954387f7bb032b5220d1014ef68741e8) )
	ROM_LOAD( "x2p-09m.3k", 0x2000000, 0x800000, CRC(344aa227) SHA1(69dc6f511939bf7fa25c2531ecf307a7565fe7a8) )
	ROM_LOAD( "x2p-10m.4k", 0x2800000, 0x800000, CRC(2eef5931) SHA1(e5227529fb68eeb1b2f25813694173a75d906b52) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "x2p_02.2e", 0x000000, 0x008000, CRC(3705de5e) SHA1(847007ca271da64bf13ffbf496d4291429eee27a) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "x2p_03.3e", 0x028000, 0x020000, CRC(6ae828f6) SHA1(41c54165e87b846a845da581f408b96979288158) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "x2p-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp12", 0x000000, 0x000008, CRC(7cc2ed68) SHA1(a409ae837665700bdc4e3aa7c41a418d5b792940) )
ROM_END

ROM_START( sfex2ph )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "x2ph_04.2h", 0x0000000, 0x080000, CRC(a4f07439) SHA1(630be537062134da2d01866b9587e5c119c198bb) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "x2p-05m.3h", 0x0000000, 0x800000, CRC(4ee3110f) SHA1(704f8dca7d0b698659af9e3271ea5072dfd42b8b) )
	ROM_LOAD( "x2p-06m.4h", 0x0800000, 0x800000, CRC(4cd53a45) SHA1(39499ea6c9aa51c71f4fe44cc02f93d5a39e14ec) )
	ROM_LOAD( "x2p-07m.5h", 0x1000000, 0x800000, CRC(11207c2a) SHA1(0182652819f1c3a36e7b42e34ef86d2455a2dd90) )
	ROM_LOAD( "x2p-08m.2k", 0x1800000, 0x800000, CRC(3560c2cc) SHA1(8b0ce22d954387f7bb032b5220d1014ef68741e8) )
	ROM_LOAD( "x2p-09m.3k", 0x2000000, 0x800000, CRC(344aa227) SHA1(69dc6f511939bf7fa25c2531ecf307a7565fe7a8) )
	ROM_LOAD( "x2p-10m.4k", 0x2800000, 0x800000, CRC(2eef5931) SHA1(e5227529fb68eeb1b2f25813694173a75d906b52) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "x2p_02.2e", 0x000000, 0x008000, CRC(3705de5e) SHA1(847007ca271da64bf13ffbf496d4291429eee27a) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "x2p_03.3e", 0x028000, 0x020000, CRC(6ae828f6) SHA1(41c54165e87b846a845da581f408b96979288158) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "x2p-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp12", 0x000000, 0x000008, CRC(7cc2ed68) SHA1(a409ae837665700bdc4e3aa7c41a418d5b792940) )
ROM_END

ROM_START( sfex2pj )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "x2pj_04.2h", 0x0000000, 0x080000, CRC(c6d0aea3) SHA1(f48ee889dd743109f830063da3eb0f687db2d86c) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "x2p-05m.3h", 0x0000000, 0x800000, CRC(4ee3110f) SHA1(704f8dca7d0b698659af9e3271ea5072dfd42b8b) )
	ROM_LOAD( "x2p-06m.4h", 0x0800000, 0x800000, CRC(4cd53a45) SHA1(39499ea6c9aa51c71f4fe44cc02f93d5a39e14ec) )
	ROM_LOAD( "x2p-07m.5h", 0x1000000, 0x800000, CRC(11207c2a) SHA1(0182652819f1c3a36e7b42e34ef86d2455a2dd90) )
	ROM_LOAD( "x2p-08m.2k", 0x1800000, 0x800000, CRC(3560c2cc) SHA1(8b0ce22d954387f7bb032b5220d1014ef68741e8) )
	ROM_LOAD( "x2p-09m.3k", 0x2000000, 0x800000, CRC(344aa227) SHA1(69dc6f511939bf7fa25c2531ecf307a7565fe7a8) )
	ROM_LOAD( "x2p-10m.4k", 0x2800000, 0x800000, CRC(2eef5931) SHA1(e5227529fb68eeb1b2f25813694173a75d906b52) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "x2p_02.2e", 0x000000, 0x008000, CRC(3705de5e) SHA1(847007ca271da64bf13ffbf496d4291429eee27a) )
	ROM_CONTINUE(         0x010000, 0x018000 )
	ROM_LOAD( "x2p_03.3e", 0x028000, 0x020000, CRC(6ae828f6) SHA1(41c54165e87b846a845da581f408b96979288158) )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "x2p-01m.3a", 0x000000, 0x400000, CRC(14a5bb0e) SHA1(dfe3c3a53bd4c58743d8039b5344d3afbe2a9c24) )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp12", 0x000000, 0x000008, CRC(7cc2ed68) SHA1(a409ae837665700bdc4e3aa7c41a418d5b792940) )
ROM_END

ROM_START( strider2 )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "hr2e_04.2h", 0x0000000, 0x080000, CRC(32d89b1c) SHA1(77d4d9de768cb195d1290b89892feaef6d8705e8) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "hr2-05m.3h", 0x0000000, 0x800000, CRC(18716fe8) SHA1(bb923f18120086054cd6fd91f77d27a190c1eed4) )
	ROM_LOAD( "hr2-06m.4h", 0x0800000, 0x800000, CRC(6f13b69c) SHA1(9a14ecc72631bc44053af71fe7e3934bedf1a71e) )
	ROM_LOAD( "hr2-07m.5h", 0x1000000, 0x800000, CRC(3925701b) SHA1(d93218d2b97cc0fc6c30221bd6b5e955520fbc46) )
	ROM_LOAD( "hr2-08m.2k", 0x1800000, 0x800000, CRC(d844c0dc) SHA1(6010cfbf4dc42fda182884d78e12dcb63df00249) )
	ROM_LOAD( "hr2-09m.3k", 0x2000000, 0x800000, CRC(cdd43e6b) SHA1(346a83deadecd56428276acefc2ce95249a49921) )
	ROM_LOAD( "hr2-10m.4k", 0x2800000, 0x400000, CRC(d95b3f37) SHA1(b6566c1184718f6c0986d13060894c0fb400c201) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "hr2_02.2e", 0x000000, 0x008000, CRC(acd8d385) SHA1(5edb61c3d66d2d09a28a71db52eee3a9f7db8c9d) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "hr2-01m.3a", 0x000000, 0x200000, CRC(510a16d1) SHA1(05f10c2921a4d3b1fab4d0a4ea06351809bdbb07) )
	ROM_RELOAD(                         0x200000, 0x200000 )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp13", 0x000000, 0x000008, CRC(606edf50) SHA1(db711a9da4ecaf74e0b37deb8eb94e8b8fbae041) )
ROM_END

ROM_START( strider2u )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "hr2u_04.2h", 0x0000000, 0x080000, CRC(b28b01c6) SHA1(ad40f550ce14f09cf34d51b9b0b7154c31c8936e) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "hr2-05m.3h", 0x0000000, 0x800000, CRC(18716fe8) SHA1(bb923f18120086054cd6fd91f77d27a190c1eed4) )
	ROM_LOAD( "hr2-06m.4h", 0x0800000, 0x800000, CRC(6f13b69c) SHA1(9a14ecc72631bc44053af71fe7e3934bedf1a71e) )
	ROM_LOAD( "hr2-07m.5h", 0x1000000, 0x800000, CRC(3925701b) SHA1(d93218d2b97cc0fc6c30221bd6b5e955520fbc46) )
	ROM_LOAD( "hr2-08m.2k", 0x1800000, 0x800000, CRC(d844c0dc) SHA1(6010cfbf4dc42fda182884d78e12dcb63df00249) )
	ROM_LOAD( "hr2-09m.3k", 0x2000000, 0x800000, CRC(cdd43e6b) SHA1(346a83deadecd56428276acefc2ce95249a49921) )
	ROM_LOAD( "hr2-10m.4k", 0x2800000, 0x400000, CRC(d95b3f37) SHA1(b6566c1184718f6c0986d13060894c0fb400c201) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "hr2_02.2e", 0x000000, 0x008000, CRC(acd8d385) SHA1(5edb61c3d66d2d09a28a71db52eee3a9f7db8c9d) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "hr2-01m.3a", 0x000000, 0x200000, CRC(510a16d1) SHA1(05f10c2921a4d3b1fab4d0a4ea06351809bdbb07) )
	ROM_RELOAD(                         0x200000, 0x200000 )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp13", 0x000000, 0x000008, CRC(606edf50) SHA1(db711a9da4ecaf74e0b37deb8eb94e8b8fbae041) )
ROM_END

ROM_START( strider2a )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "hr2a_04.2h", 0x0000000, 0x080000, CRC(56ff9394) SHA1(fe8417965d945210ac098c6678c02f1c678bd13b) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "hr2-05m.3h", 0x0000000, 0x800000, CRC(18716fe8) SHA1(bb923f18120086054cd6fd91f77d27a190c1eed4) )
	ROM_LOAD( "hr2-06m.4h", 0x0800000, 0x800000, CRC(6f13b69c) SHA1(9a14ecc72631bc44053af71fe7e3934bedf1a71e) )
	ROM_LOAD( "hr2-07m.5h", 0x1000000, 0x800000, CRC(3925701b) SHA1(d93218d2b97cc0fc6c30221bd6b5e955520fbc46) )
	ROM_LOAD( "hr2-08m.2k", 0x1800000, 0x800000, CRC(d844c0dc) SHA1(6010cfbf4dc42fda182884d78e12dcb63df00249) )
	ROM_LOAD( "hr2-09m.3k", 0x2000000, 0x800000, CRC(cdd43e6b) SHA1(346a83deadecd56428276acefc2ce95249a49921) )
	ROM_LOAD( "hr2-10m.4k", 0x2800000, 0x400000, CRC(d95b3f37) SHA1(b6566c1184718f6c0986d13060894c0fb400c201) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "hr2_02.2e", 0x000000, 0x008000, CRC(acd8d385) SHA1(5edb61c3d66d2d09a28a71db52eee3a9f7db8c9d) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "hr2-01m.3a", 0x000000, 0x200000, CRC(510a16d1) SHA1(05f10c2921a4d3b1fab4d0a4ea06351809bdbb07) )
	ROM_RELOAD(                         0x200000, 0x200000 )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp13", 0x000000, 0x000008, CRC(606edf50) SHA1(db711a9da4ecaf74e0b37deb8eb94e8b8fbae041) )
ROM_END

ROM_START( shiryu2 )
	CAPCOM_ZN2_BIOS

	ROM_REGION32_LE( 0x080000, REGION_USER2, 0 )
	ROM_LOAD( "hr2j_04.2h", 0x0000000, 0x080000, CRC(0824ee5f) SHA1(a296ffe03f0d947deb9803d05de3c240a26b52bb) )

	ROM_REGION32_LE( 0x3000000, REGION_USER1, 0 )
	ROM_LOAD( "hr2-05m.3h", 0x0000000, 0x800000, CRC(18716fe8) SHA1(bb923f18120086054cd6fd91f77d27a190c1eed4) )
	ROM_LOAD( "hr2-06m.4h", 0x0800000, 0x800000, CRC(6f13b69c) SHA1(9a14ecc72631bc44053af71fe7e3934bedf1a71e) )
	ROM_LOAD( "hr2-07m.5h", 0x1000000, 0x800000, CRC(3925701b) SHA1(d93218d2b97cc0fc6c30221bd6b5e955520fbc46) )
	ROM_LOAD( "hr2-08m.2k", 0x1800000, 0x800000, CRC(d844c0dc) SHA1(6010cfbf4dc42fda182884d78e12dcb63df00249) )
	ROM_LOAD( "hr2-09m.3k", 0x2000000, 0x800000, CRC(cdd43e6b) SHA1(346a83deadecd56428276acefc2ce95249a49921) )
	ROM_LOAD( "hr2-10m.4k", 0x2800000, 0x400000, CRC(d95b3f37) SHA1(b6566c1184718f6c0986d13060894c0fb400c201) )

	ROM_REGION( 0x050000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "hr2_02.2e", 0x000000, 0x008000, CRC(acd8d385) SHA1(5edb61c3d66d2d09a28a71db52eee3a9f7db8c9d) )
	ROM_CONTINUE(         0x010000, 0x018000 )

	ROM_REGION( 0x400000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD16_WORD_SWAP( "hr2-01m.3a", 0x000000, 0x200000, CRC(510a16d1) SHA1(05f10c2921a4d3b1fab4d0a4ea06351809bdbb07) )
	ROM_RELOAD(                         0x200000, 0x200000 )

	ROM_REGION( 0x000008, REGION_USER5, 0 )
	ROM_LOAD( "cp13", 0x000000, 0x000008, CRC(606edf50) SHA1(db711a9da4ecaf74e0b37deb8eb94e8b8fbae041) )
ROM_END


/* Tecmo TPS */

#define TPS_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m534002c-61.ic353", 0x0000000, 0x080000, CRC(69ffbcb4) SHA1(03eb2febfab3fcde716defff291babd9392de965) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "mg01.ic652", 0x000000, 0x000008, CRC(50dc8322) SHA1(cef4011c6c06d6fce546e1db5d9fbae155eabf57) )

/* TPS ROM addressing note: .216 *if present* goes at 0x400000, else nothing there.
   .217 goes at 0x800000, .218 at 0xc00000, .219 at 0x1000000, and so on. */

ROM_START( coh1002m )
	TPS_BIOS
ROM_END

ROM_START( glpracr2 )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "gallop2u.119", 0x0000001, 0x100000, CRC(9899911c) SHA1(f043fb97760c53422ad6aeb214474c0be00017ce) )
	ROM_LOAD16_BYTE( "gallop2u.120", 0x0000000, 0x100000, CRC(fd69bd4b) SHA1(26a183bdc3b2fb3d93bd7694e429a676106f4e58) )
	ROM_LOAD( "gra2-0.217",          0x0800000, 0x400000, CRC(a077ffa3) SHA1(73492ec2145246276bfe25b27d7de4f6393124f4) )
	ROM_LOAD( "gra2-1.218",          0x0c00000, 0x400000, CRC(28ce033c) SHA1(4dc53e5c82fde683efd72c66b397d56aa72d52b9) )
	ROM_LOAD( "gra2-2.219",          0x1000000, 0x400000, CRC(0c9cb7da) SHA1(af23c11e69428413ff4d1c2746adb786de927cb5) )
	ROM_LOAD( "gra2-3.220",          0x1400000, 0x400000, CRC(264e3a0c) SHA1(c1509b16d7192b9f61dbceb299290239219adefd) )
	ROM_LOAD( "gra2-4.221",          0x1800000, 0x400000, CRC(056bf022) SHA1(f73152268e09249bad2ffd4df5cfb4f0f4a494ae) )
	ROM_LOAD( "gra2-5.222",          0x1c00000, 0x400000, CRC(3eb97009) SHA1(e3ed4bdb0dcf581dc05b19306a3db0b989d00f75) )
	ROM_LOAD( "gra2-6.223",          0x2000000, 0x400000, CRC(8c6b4c4c) SHA1(0053f736dcd437c01da8cadd820e8af658ce6077) )
	ROM_LOAD( "gra2-7.323",          0x2400000, 0x400000, CRC(7dfb6c54) SHA1(6e9a9a4172f957ba354ddd82c30735a56c5934b1) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg02", 0x000000, 0x000008, CRC(ce1d5558) SHA1(1d704636b14bb11889c06b344043555803ad6e08) )
ROM_END

ROM_START( glpracr2j )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "1.119", 0x0000001, 0x100000, CRC(0fe2d2df) SHA1(031369f4e1138e2ee293c321e5ee418e560b3f06) )
	ROM_LOAD16_BYTE( "2.120", 0x0000000, 0x100000, CRC(8e3fb1c0) SHA1(2126c1e43bee7cd938e0f2a3ea841da8811223cd) )
	ROM_LOAD( "gra2-0.217",   0x0800000, 0x400000, CRC(a077ffa3) SHA1(73492ec2145246276bfe25b27d7de4f6393124f4) )
	ROM_LOAD( "gra2-1.218",   0x0c00000, 0x400000, CRC(28ce033c) SHA1(4dc53e5c82fde683efd72c66b397d56aa72d52b9) )
	ROM_LOAD( "gra2-2.219",   0x1000000, 0x400000, CRC(0c9cb7da) SHA1(af23c11e69428413ff4d1c2746adb786de927cb5) )
	ROM_LOAD( "gra2-3.220",   0x1400000, 0x400000, CRC(264e3a0c) SHA1(c1509b16d7192b9f61dbceb299290239219adefd) )
	ROM_LOAD( "gra2-4.221",   0x1800000, 0x400000, CRC(056bf022) SHA1(f73152268e09249bad2ffd4df5cfb4f0f4a494ae) )
	ROM_LOAD( "gra2-5.222",   0x1c00000, 0x400000, CRC(3eb97009) SHA1(e3ed4bdb0dcf581dc05b19306a3db0b989d00f75) )
	ROM_LOAD( "gra2-6.223",   0x2000000, 0x400000, CRC(8c6b4c4c) SHA1(0053f736dcd437c01da8cadd820e8af658ce6077) )
	ROM_LOAD( "gra2-7.323",   0x2400000, 0x400000, CRC(7dfb6c54) SHA1(6e9a9a4172f957ba354ddd82c30735a56c5934b1) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg02", 0x000000, 0x000008, CRC(ce1d5558) SHA1(1d704636b14bb11889c06b344043555803ad6e08) )
ROM_END

ROM_START( glpracr2l )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "gra2b.119", 0x0000001, 0x100000, CRC(43abee7c) SHA1(ea0afc820d8480c12c9af54057877ff11a8012fb) )
	ROM_LOAD16_BYTE( "gra2a.120", 0x0000000, 0x100000, CRC(f60096d4) SHA1(5349d780d41a5711b483cd7eb66cd4e496b4fbe4) )
	ROM_LOAD( "gra2-0.217",       0x0800000, 0x400000, CRC(a077ffa3) SHA1(73492ec2145246276bfe25b27d7de4f6393124f4) )
	ROM_LOAD( "gra2-1.218",       0x0c00000, 0x400000, CRC(28ce033c) SHA1(4dc53e5c82fde683efd72c66b397d56aa72d52b9) )
	ROM_LOAD( "gra2-2.219",       0x1000000, 0x400000, CRC(0c9cb7da) SHA1(af23c11e69428413ff4d1c2746adb786de927cb5) )
	ROM_LOAD( "gra2-3.220",       0x1400000, 0x400000, CRC(264e3a0c) SHA1(c1509b16d7192b9f61dbceb299290239219adefd) )
	ROM_LOAD( "gra2-4.221",       0x1800000, 0x400000, CRC(056bf022) SHA1(f73152268e09249bad2ffd4df5cfb4f0f4a494ae) )
	ROM_LOAD( "gra2-5.222",       0x1c00000, 0x400000, CRC(3eb97009) SHA1(e3ed4bdb0dcf581dc05b19306a3db0b989d00f75) )
	ROM_LOAD( "gra2-6.223",       0x2000000, 0x400000, CRC(8c6b4c4c) SHA1(0053f736dcd437c01da8cadd820e8af658ce6077) )
	ROM_LOAD( "gra2-7.323",       0x2400000, 0x400000, CRC(7dfb6c54) SHA1(6e9a9a4172f957ba354ddd82c30735a56c5934b1) )

	ROM_REGION( 0x040000, REGION_USER7, 0 ) /* Z80 link board, not emulated */
	ROM_LOAD( "link3118.bin", 0x000000, 0x040000, CRC(a4d4761e) SHA1(3fb25dfa5220d25093588d9501e0666214491100) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg02", 0x000000, 0x000008, CRC(ce1d5558) SHA1(1d704636b14bb11889c06b344043555803ad6e08) )
ROM_END

ROM_START( doapp )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "doapp119.bin", 0x0000001, 0x100000, CRC(bbe04cef) SHA1(f2dae4810ca78075fc3007a6001531a455235a2e) )
	ROM_LOAD16_BYTE( "doapp120.bin", 0x0000000, 0x100000, CRC(b614d7e6) SHA1(373756d9b88b45c677e987ee1e5cb2d5446ecfe8) )
	ROM_LOAD( "doapp-0.216",         0x0400000, 0x400000, CRC(acc6c539) SHA1(a744567a3d75634098b1749103307981be9acbdd) )
	ROM_LOAD( "doapp-1.217",         0x0800000, 0x400000, CRC(14b961c4) SHA1(3fae1fcb4665ba8bad391881b26c2d087718d42f) )
	ROM_LOAD( "doapp-2.218",         0x0c00000, 0x400000, CRC(134f698f) SHA1(6422972cf5d30a0f09f0c20f042691d5969207b4) )
	ROM_LOAD( "doapp-3.219",         0x1000000, 0x400000, CRC(1c6540f3) SHA1(8631fde93a1da6325d7b31c7edf12c964f0ac4fc) )
	ROM_LOAD( "doapp-4.220",         0x1400000, 0x400000, CRC(f83bacf7) SHA1(5bd66da993f0db966581dde80dd7e5b377754412) )
	ROM_LOAD( "doapp-5.221",         0x1800000, 0x400000, CRC(e11e8b71) SHA1(b1d1b9532b5f074ce216a603436d5674d136865d) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg05", 0x000000, 0x000008, CRC(5748a4ca) SHA1(c88d73f6a646a9ddefdfd84cba70d591759c069f) )
ROM_END

ROM_START( doappk )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "doapp_u0119.119", 0x0000001, 0x100000, CRC(9084704e) SHA1(616b3e65a32768767209ae77c48ad34d11b31754) )
	ROM_LOAD16_BYTE( "doapp_u0120.120", 0x0000000, 0x100000, CRC(720a6983) SHA1(d38783110440ffa00036b86f97bff5edb6a5673f) )
	ROM_LOAD( "doapp-0.216",            0x0400000, 0x400000, CRC(acc6c539) SHA1(a744567a3d75634098b1749103307981be9acbdd) )
	ROM_LOAD( "doapp-1.217",            0x0800000, 0x400000, CRC(14b961c4) SHA1(3fae1fcb4665ba8bad391881b26c2d087718d42f) )
	ROM_LOAD( "doapp-2.218",            0x0c00000, 0x400000, CRC(134f698f) SHA1(6422972cf5d30a0f09f0c20f042691d5969207b4) )
	ROM_LOAD( "doapp-3.219",            0x1000000, 0x400000, CRC(1c6540f3) SHA1(8631fde93a1da6325d7b31c7edf12c964f0ac4fc) )
	ROM_LOAD( "doapp-4.220",            0x1400000, 0x400000, CRC(f83bacf7) SHA1(5bd66da993f0db966581dde80dd7e5b377754412) )
	ROM_LOAD( "doapp-5.221",            0x1800000, 0x400000, CRC(e11e8b71) SHA1(b1d1b9532b5f074ce216a603436d5674d136865d) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg05", 0x000000, 0x000008, CRC(5748a4ca) SHA1(c88d73f6a646a9ddefdfd84cba70d591759c069f) )
ROM_END

ROM_START( cbaj )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "cbaj_1.119", 0x0000001, 0x080000, CRC(814f8b4b) SHA1(17966038a692d0701139660f25725d7c10a2a928) )
	ROM_LOAD16_BYTE( "cbaj_2.120", 0x0000000, 0x080000, CRC(89286229) SHA1(18a84ef648ec3b79707eb42b55563adf38dffd0d) )
	ROM_LOAD( "cb-00.216",         0x0400000, 0x400000, CRC(3db68bea) SHA1(77ab334e0c02e608b11d8fdb9505b2301f6f9afb) )
	ROM_LOAD( "cb-01.217",         0x0800000, 0x400000, CRC(481040bc) SHA1(c6fe575b77d1eb5f613691dec5ed08929b72b955) )
	ROM_LOAD( "cb-02.218",         0x0c00000, 0x400000, CRC(858f116c) SHA1(e3546862d367d2fe88913fea3185b23bc6a9777d) )
	ROM_LOAD( "cb-03.219",         0x1000000, 0x400000, CRC(3576ea2a) SHA1(a5ee7bb9f4650e99ee067eb1cc28c62d9099a6cf) )
	ROM_LOAD( "cb-04.220",         0x1400000, 0x400000, CRC(551c4b29) SHA1(c3f8508a006b475491c9ea20eb64c3bea6b35afb) )
	ROM_LOAD( "cb-05.221",         0x1800000, 0x400000, CRC(7da453da) SHA1(85b2c93b9453e8c7791b530b7e036e4ef6abc077) )
	ROM_LOAD( "cb-06.222",         0x1c00000, 0x400000, CRC(833cb18b) SHA1(dbc390e1dbf3e7815eb3d170c0890d3785d8002c) )
	ROM_LOAD( "cb-07.223",         0x2000000, 0x400000, CRC(3b64ce9e) SHA1(a137da126295736bb7643655d52bd570004e87fd) )
	ROM_LOAD( "cb-08.323",         0x2400000, 0x400000, CRC(57cc482e) SHA1(603c3d13a6cd796c209a97aa7e63b77bdbf71580) )

	ROM_REGION( 0x040000, REGION_CPU2, 0 ) /* Z80 */
	ROM_LOAD( "cbaj_z80.3118", 0x000000, 0x040000, CRC(92b02ad2) SHA1(f72317679ecbd8a0c3b081baaf9ff20a8c9ec00f) )

	ROM_REGION( 0x800000, REGION_SOUND1, 0 ) /* YMZ280B samples */
	ROM_LOAD( "cb-vo.5120", 0x000000, 0x400000, CRC(afb05d6d) SHA1(0c08010579813814fbf8a978cf4376bab18697a4) )
	ROM_LOAD( "cb-se.5121", 0x400000, 0x400000, CRC(f12b3db9) SHA1(d5231ad664603050bdca2081b114b07fc905ddc2) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg03", 0x000000, 0x000008, CRC(8be79633) SHA1(f38f881b6139eb8368e593904cb50b6e68bdf2e9) )
ROM_END

ROM_START( cbajbl )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "uo119.bin",  0x0000001, 0x080000, CRC(814f8b4b) SHA1(17966038a692d0701139660f25725d7c10a2a928) )
	ROM_LOAD16_BYTE( "uo120.bin",  0x0000000, 0x080000, CRC(89286229) SHA1(18a84ef648ec3b79707eb42b55563adf38dffd0d) )
	ROM_LOAD( "uo216.bin",         0x0400000, 0x200000, CRC(1b18e0e9) SHA1(306b4849f707c5b643992e3880a34b894fc9f0e1) )
	ROM_LOAD( "uo216h.bin",        0x0600000, 0x200000, CRC(bb86ee76) SHA1(9eead295d8fec3708c1e693d3d58346cc214580c) )
	ROM_LOAD( "uo217.bin",         0x0800000, 0x200000, CRC(02d8dfcd) SHA1(b83636374fcae6aa1c3a320fafcc7bd7890c0bc6) )
	ROM_LOAD( "uo217h.bin",        0x0a00000, 0x200000, CRC(6e0f34da) SHA1(e7384a3019dcc0f69335635056ae3fac89fb4b52) )
	ROM_LOAD( "uo218.bin",         0x0c00000, 0x200000, CRC(14eaaff6) SHA1(b38f541b9f0397edee071d883b2801fa40cba127) )
	ROM_LOAD( "uo218h.bin",        0x0e00000, 0x200000, CRC(9f6420dc) SHA1(3b354bdfc54ac5d69c229d897cabc807c21c61bc) )
	ROM_LOAD( "uo219.bin",         0x1000000, 0x200000, CRC(89e727b1) SHA1(09e69708c5b8244dbacd09dce6558cf917a58623) )
	ROM_LOAD( "uo219h.bin",        0x1200000, 0x200000, BAD_DUMP CRC(727b7ae5) SHA1(182b63b77965a25591bda71575af464f6e3b26f8) )
	ROM_LOAD( "uo220.bin",         0x1400000, 0x200000, BAD_DUMP CRC(19102f15) SHA1(b7964e0cb53b608b775d7a713b63f13307b1b532) )
	ROM_LOAD( "uo220h.bin",        0x1600000, 0x200000, BAD_DUMP CRC(e7b94df3) SHA1(07b5d85e1a337f10c25cebdb0f3f893e38a1db6c) )
	ROM_LOAD( "uo221.bin",         0x1800000, 0x200000, CRC(701d7c4e) SHA1(b06c2db7e06d453991f9872bcb62951eb62b5d56) )
	ROM_LOAD( "uo221h.bin",        0x1a00000, 0x200000, CRC(5e93c0d6) SHA1(2ec5afd53eee9e8e6e2ac91a269a5ec55cef4da1) )
	ROM_LOAD( "uo222.bin",         0x1c00000, 0x200000, CRC(76ad7801) SHA1(9cb38aff86f3f8151145f2661bf1261adbd2ec1c) )
	ROM_LOAD( "uo222h.bin",        0x1e00000, 0x200000, CRC(34561670) SHA1(2131fcbbc8bc3da31cd4ef027ec6688af212b7c1) )
	ROM_LOAD( "uo223.bin",         0x2000000, 0x200000, CRC(d4a80392) SHA1(d8d69e8a15910471b718dc6531e4470fc35365cc) )
	ROM_LOAD( "uo223h.bin",        0x2200000, 0x200000, CRC(eb5b6b3a) SHA1(ad595053b852a45b85f061dfa960fa04cf74eb9f) )
	ROM_LOAD( "uo323.bin",         0x2400000, 0x200000, CRC(e9140f6c) SHA1(a9e8c3b94de319a5fd5327111339c76c65371ba9) )

	ROM_REGION( 0x040000, REGION_CPU2, 0 ) /* Z80 */
	ROM_LOAD( "near_z80.bin",  0x000000,  0x020000, CRC(10b30c5b) SHA1(8c4af71dc14c43a2b32377737b6dabc393b3c141) )

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASE00 ) /* YMZ280B samples */
	ROM_LOAD( "uo5120.bin",   0x000000, 0x200000, CRC(973bc8fe) SHA1(ab37fdebff5daee469f51838a6f498541c1c9e07) )
	ROM_LOAD( "uo5120h.bin",  0x200000, 0x200000, CRC(01bede31) SHA1(ed7a2eaabe21127b6440cc20d3089a880ce428d5) )
	ROM_LOAD( "uo5121.bin",   0x400000, 0x200000, CRC(76112372) SHA1(f69860fbffd33aee6cdf475c091987467bca274a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg03", 0x000000, 0x000008, CRC(8be79633) SHA1(f38f881b6139eb8368e593904cb50b6e68bdf2e9) )
ROM_END

ROM_START( shngmtkb )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "shmj-b.119", 0x0000001, 0x080000, CRC(65522c67) SHA1(b5981e5859aab742a87d6742feb9c55a3e6ba13f) )
	ROM_LOAD16_BYTE( "shmj-a.120", 0x0000000, 0x080000, CRC(a789defa) SHA1(f8f0d1c9e3492cda652a9561ef1d549b92f73efd) )
	ROM_LOAD( "sh-00.217",         0x0800000, 0x400000, CRC(081fed1c) SHA1(fb18add9521b8b104329871b4c1b8ae5e0254f8b) )
	ROM_LOAD( "sh-01.218",         0x0c00000, 0x400000, CRC(5a84ea96) SHA1(af4972cc10706999361d7505b975f5f1e1fc6761) )
	ROM_LOAD( "sh-02.219",         0x1000000, 0x400000, CRC(c8f80d76) SHA1(51e4eac6cec8e37e5b8c0e7d341feea574add7da) )
	ROM_LOAD( "sh-03.220",         0x1400000, 0x400000, CRC(daaa4c73) SHA1(eb31d4cadd9eba3d3431f3f6ef880bb2effa0b9f) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg04", 0x000000, 0x000008, CRC(2711a75f) SHA1(400cd6f28826ba0ad1125eda21507e5ebf5caccf) )
ROM_END

ROM_START( tondemo )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "u0119.bin", 0x0000001, 0x100000, CRC(5711e301) SHA1(005375d32c1eda9bd39e46326880a62506d06389) )
	ROM_LOAD16_BYTE( "u0120.bin", 0x0000000, 0x100000, CRC(0b8312c6) SHA1(93e0e4b796cc953daf7ed2ff2f327aed07cf833a) )
	ROM_LOAD( "tca-0.217",        0x0800000, 0x400000, CRC(ef175910) SHA1(b77aa9016804172d433d97d5fdc242a1361e941c) )
	ROM_LOAD( "tca-1.218",        0x0c00000, 0x400000, CRC(c3474e8a) SHA1(46dd0ae7cd2e54c639fe39d6965ef71ce6a1b921) )
	ROM_LOAD( "tca-2.219",        0x1000000, 0x400000, CRC(89b8e1a8) SHA1(70c5f0f2d0a7869e29b62b32fa485f941b683678) )
	ROM_LOAD( "tca-3.220",        0x1400000, 0x400000, CRC(4fcf8032) SHA1(3ea815548c3bda32b1d4e88454c29e5025431b1c) )
	ROM_LOAD( "tca-4.221",        0x1800000, 0x400000, CRC(c9e23f25) SHA1(145d4e7f0cb67d2552559ce90305a56802a253f9) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg09", 0x000000, 0x000008, CRC(bb4a157c) SHA1(8ea729c3c2694bee12292b9654b86ff6a9e45f94) )
ROM_END

ROM_START( glpracr3 )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "1.119", 0x0000001, 0x100000, CRC(89bdf567) SHA1(916accbcad52e9ee4e3b28a339138fe2bfbecdfe) )
	ROM_LOAD16_BYTE( "2.120", 0x0000000, 0x100000, CRC(042273fb) SHA1(eb98c4e74f385ddc6545b9250df5858b39fe361d) )
	ROM_LOAD( "gra3-0.216",   0x0400000, 0x400000, CRC(b405ee65) SHA1(8ba9872e4c166e3b659a2802554bf1e964f64620) )
	ROM_LOAD( "gra3-1.217",   0x0800000, 0x400000, CRC(a06f05ac) SHA1(ade224533d75c64cb188b78bdca908c1fa882492) )
	ROM_LOAD( "gra3-2.218",   0x0c00000, 0x400000, CRC(31793f9b) SHA1(310c2dff84d17c9ed7f59e249b22e9394edcb444) )
	ROM_LOAD( "gra3-3.219",   0x1000000, 0x400000, CRC(d59fb3eb) SHA1(2db2cc1d4884d54c415531053319f2b2ad65361f) )
	ROM_LOAD( "gra3-4.220",   0x1400000, 0x400000, CRC(59a0a105) SHA1(6a585c9eaa8d9b5dad798d9d28d73f04bc838114) )
	ROM_LOAD( "gra3-5.221",   0x1800000, 0x400000, CRC(4994fb17) SHA1(59b3e6c333e55ca8b6b4b00cd52b51e3e59a5657) )
	ROM_LOAD( "gra3-6.222",   0x1c00000, 0x400000, CRC(1362c1af) SHA1(eae5b3cb11d361b3aa3f572e49800c0b2e3544ca) )
	ROM_LOAD( "gra3-7.223",   0x2000000, 0x400000, CRC(73565e1f) SHA1(74311ee94e3abc8428b4a8b1c6c3dacd883b5646) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg08", 0x000000, 0x000008, CRC(679367fe) SHA1(495b03e1cdad9d6aaf509b73d837340e2b1bb23b) )
ROM_END

ROM_START( glpracr3j )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "gra3u119.119", 0x0000001, 0x100000, CRC(aba69017) SHA1(670b895ee7d36bc5a00f6b0df7ce965517986617) )
	ROM_LOAD16_BYTE( "gra3u120.120", 0x0000000, 0x100000, CRC(8aa98d99) SHA1(9dc1ba89e37a5c2955ee027e4e5aa0ae71e09f9b) )
	ROM_LOAD( "gra3-0.216",          0x0400000, 0x400000, CRC(b405ee65) SHA1(8ba9872e4c166e3b659a2802554bf1e964f64620) )
	ROM_LOAD( "gra3-1.217",          0x0800000, 0x400000, CRC(a06f05ac) SHA1(ade224533d75c64cb188b78bdca908c1fa882492) )
	ROM_LOAD( "gra3-2.218",          0x0c00000, 0x400000, CRC(31793f9b) SHA1(310c2dff84d17c9ed7f59e249b22e9394edcb444) )
	ROM_LOAD( "gra3-3.219",          0x1000000, 0x400000, CRC(d59fb3eb) SHA1(2db2cc1d4884d54c415531053319f2b2ad65361f) )
	ROM_LOAD( "gra3-4.220",          0x1400000, 0x400000, CRC(59a0a105) SHA1(6a585c9eaa8d9b5dad798d9d28d73f04bc838114) )
	ROM_LOAD( "gra3-5.221",          0x1800000, 0x400000, CRC(4994fb17) SHA1(59b3e6c333e55ca8b6b4b00cd52b51e3e59a5657) )
	ROM_LOAD( "gra3-6.222",          0x1c00000, 0x400000, CRC(1362c1af) SHA1(eae5b3cb11d361b3aa3f572e49800c0b2e3544ca) )
	ROM_LOAD( "gra3-7.223",          0x2000000, 0x400000, CRC(73565e1f) SHA1(74311ee94e3abc8428b4a8b1c6c3dacd883b5646) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg08", 0x000000, 0x000008, CRC(679367fe) SHA1(495b03e1cdad9d6aaf509b73d837340e2b1bb23b) )
ROM_END

ROM_START( flamegun )
	TPS_BIOS

	ROM_REGION32_LE( 0x3800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "flamegun.119", 0x0000001, 0x100000, CRC(bc8e9e29) SHA1(02e4f079f0ed864dbc056d5f64d33a0522c034fd) )
	ROM_LOAD16_BYTE( "flamegun.120", 0x0000000, 0x100000, CRC(387f3070) SHA1(6b12765f1d3ec5f3d1cdfd961fba72a319d65ff4) )
	ROM_LOAD( "fg00.216",            0x0400000, 0x400000, CRC(f33736ca) SHA1(7b18f9fef1df913b7ed3a2c97e9c4925790d86c5) )
	ROM_LOAD( "fg01.217",            0x0800000, 0x400000, CRC(8980ff44) SHA1(f677bf5d279ad7731730b7e36ebf33d554903ce6) )
	ROM_LOAD( "fg02.218",            0x0c00000, 0x400000, CRC(97d1b032) SHA1(1d23cd40ced002ae1fe2fb009d2f31d8612b125a) )
	ROM_LOAD( "fg03.219",            0x1000000, 0x400000, CRC(cf84508c) SHA1(2a1173e3751f5a8b2400219a75f23d2450a0ebd6) )
	ROM_LOAD( "fg04.220",            0x1400000, 0x400000, CRC(5cc333fa) SHA1(fd81e811ef2026b245e65c104d24ae1679baa0f5) )
	ROM_LOAD( "fg05.221",            0x1800000, 0x400000, CRC(9490bc1b) SHA1(c5ea133de0a271793601f2701267d3ca82781f60) )
	ROM_LOAD( "fg06.222",            0x1c00000, 0x400000, CRC(8c29b754) SHA1(304086196321b7d9748456ed1af8965ac6192942) )
	ROM_LOAD( "fg07.223",            0x2000000, 0x400000, CRC(f62cefe8) SHA1(a59a3a41258e8474f0aeb0e3b8c2f73caf47ece4) )
	ROM_LOAD( "fg08.323",            0x2400000, 0x400000, CRC(855959b4) SHA1(994b7c8bd883a41470791802d5a07a98a553096e) )
	ROM_LOAD( "fg09.324",            0x2800000, 0x400000, CRC(82f129b4) SHA1(c0dcbc908c12f7cecbb051a671649edd20bac32c) )
	ROM_LOAD( "fg0a.325",            0x2c00000, 0x400000, CRC(f8d2b20c) SHA1(d0c9e413d72772ab8710e217d228e001b28667c8) )
	ROM_LOAD( "fb0b.326",            0x3000000, 0x400000, CRC(ad78ec79) SHA1(7e37a90a64c70f2c0024eaf30e1e4e028c0d858e) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg06", 0x000000, 0x000008, CRC(a10556fe) SHA1(5c2ecbb9319517137c0bbce1e38e75c6aa5e8b6f) )
ROM_END

ROM_START( flamegunj )
	TPS_BIOS

	ROM_REGION32_LE( 0x3800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "fg_1.119", 0x0000001, 0x100000, CRC(1f2aa527) SHA1(de3a20d9aeb745fe82cd1d87bde26876e088483a) )
	ROM_LOAD16_BYTE( "fg_2.120", 0x0000000, 0x100000, CRC(a2cd4cad) SHA1(bf542eeb6e768b3e86bacdc79ab04be394ce3e63) )
	ROM_LOAD( "fg00.216",        0x0400000, 0x400000, CRC(f33736ca) SHA1(7b18f9fef1df913b7ed3a2c97e9c4925790d86c5) )
	ROM_LOAD( "fg01.217",        0x0800000, 0x400000, CRC(8980ff44) SHA1(f677bf5d279ad7731730b7e36ebf33d554903ce6) )
	ROM_LOAD( "fg02.218",        0x0c00000, 0x400000, CRC(97d1b032) SHA1(1d23cd40ced002ae1fe2fb009d2f31d8612b125a) )
	ROM_LOAD( "fg03.219",        0x1000000, 0x400000, CRC(cf84508c) SHA1(2a1173e3751f5a8b2400219a75f23d2450a0ebd6) )
	ROM_LOAD( "fg04.220",        0x1400000, 0x400000, CRC(5cc333fa) SHA1(fd81e811ef2026b245e65c104d24ae1679baa0f5) )
	ROM_LOAD( "fg05.221",        0x1800000, 0x400000, CRC(9490bc1b) SHA1(c5ea133de0a271793601f2701267d3ca82781f60) )
	ROM_LOAD( "fg06.222",        0x1c00000, 0x400000, CRC(8c29b754) SHA1(304086196321b7d9748456ed1af8965ac6192942) )
	ROM_LOAD( "fg07.223",        0x2000000, 0x400000, CRC(f62cefe8) SHA1(a59a3a41258e8474f0aeb0e3b8c2f73caf47ece4) )
	ROM_LOAD( "fg08.323",        0x2400000, 0x400000, CRC(855959b4) SHA1(994b7c8bd883a41470791802d5a07a98a553096e) )
	ROM_LOAD( "fg09.324",        0x2800000, 0x400000, CRC(82f129b4) SHA1(c0dcbc908c12f7cecbb051a671649edd20bac32c) )
	ROM_LOAD( "fg0a.325",        0x2c00000, 0x400000, CRC(f8d2b20c) SHA1(d0c9e413d72772ab8710e217d228e001b28667c8) )
	ROM_LOAD( "fb0b.326",        0x3000000, 0x400000, CRC(ad78ec79) SHA1(7e37a90a64c70f2c0024eaf30e1e4e028c0d858e) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg06", 0x000000, 0x000008, CRC(a10556fe) SHA1(5c2ecbb9319517137c0bbce1e38e75c6aa5e8b6f) )
ROM_END

ROM_START( lpadv )
	TPS_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "lp_3.u0119", 0x000001, 0x100000, CRC(18cade44) SHA1(8a44156224c77c51f4f6ca61a0168e48dfcc6eda) )
	ROM_LOAD16_BYTE( "lp_4.u0120", 0x000000, 0x100000, CRC(12fffc02) SHA1(3294b65e4a0bbf501785565dd0c1f36f9bcea969) )
	ROM_LOAD( "rp00.u0216",        0x400000, 0x400000, CRC(d759d0d4) SHA1(47b009a5dfa81611276b1376bdab44dfad597e85) )
	ROM_LOAD( "rp01.u0217",        0x800000, 0x400000, CRC(5be576e1) SHA1(e24a96d179016d6d65205079874b35500760a642) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg07", 0x000000, 0x000008, CRC(afec0e8e) SHA1(dc3c696181dfb5b7426d882094be5eee2619e2a5) )
ROM_END

ROM_START( tblkkuzu )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "tbk.u119", 0x0000001, 0x100000, CRC(621b07e1) SHA1(30773aaa333fdee7ef55db2f8adde010688abce1) )
	ROM_LOAD16_BYTE( "tbk.u120", 0x0000000, 0x100000, CRC(bb390f7d) SHA1(6bce88448fbb5308952f8c221e786be8aa51ceff) )
	ROM_LOAD( "tbk.u0216",       0x0400000, 0x400000, CRC(41f8285f) SHA1(3326ab83d96d51ed31fb5c2f30630ff480d45282) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg12", 0x000000, 0x000008, CRC(584e6ea2) SHA1(f60fb556090d31f0f0fa1f8d87815eff6d2498a4) )
ROM_END

ROM_START( 1on1gov )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "1on1.u119", 0x0000001, 0x100000, CRC(10aecc19) SHA1(ad2fe6011551935907568cc3b4028f481034537c) )
	ROM_LOAD16_BYTE( "1on1.u120", 0x0000000, 0x100000, CRC(eea158bd) SHA1(2b2a56fcce46557201bbbab7b170ee64549ddafe) )
	ROM_LOAD( "oog-0.u0217",      0x0800000, 0x400000, CRC(8b42f365) SHA1(6035a370f477f0f33894f642717fa0b012540d36) )
	ROM_LOAD( "oog-1.u0218",      0x0c00000, 0x400000, CRC(65162f46) SHA1(db420a2f0d996b32cd4b6e9352d46a36fa31eaaa) )
	ROM_LOAD( "oog-2.u0219",      0x1000000, 0x400000, CRC(14cf3a84) SHA1(60175a1fb2c765e4c3d0e30e77961f84cfa8485c) )
	ROM_LOAD( "oog-3.u0220",      0x1400000, 0x400000, CRC(9a45f6ff) SHA1(e0ee90c545c821bf1d6b4709b1e40f93314c51a6) )
	ROM_LOAD( "oog-4.u0221",      0x1800000, 0x400000, CRC(ba20a1fd) SHA1(7893f50de730624b8447f39fc7e25e4e334df845) )
	ROM_LOAD( "oog-5.u0222",      0x1c00000, 0x400000, CRC(eed1953d) SHA1(8d3e738a07b9c6b6ca55be7b47444b5e3725065c) )
	ROM_LOAD( "oog-6.u0223",      0x2000000, 0x400000, CRC(f74f38b6) SHA1(ff7f0ebff85fc982f8d1c13d6649d4c7c5da6c45) )
	ROM_LOAD( "oog-7.u0323",      0x2400000, 0x400000, CRC(0e58777c) SHA1(9f8ee3c6d6d8b1482522500e18217577056d8c98) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16", 0x000000, 0x000800, CRC(fe992f29) SHA1(73c9c4d40abd8f3a95d4eb20f3c65f3a5cdd1203) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg10", 0x000000, 0x000008, CRC(8a73da40) SHA1(d6803a4c1d240dec2a4d8ac43aa6d7e9df947026) )
ROM_END

ROM_START( twcupmil )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "twm-ep.119", 0x0000001, 0x100000, CRC(5f2908fb) SHA1(fc7ac1f6e81543678705e6e510dbf786b1502444) )
	ROM_LOAD16_BYTE( "twm-ep.120", 0x0000000, 0x100000, CRC(1a0ef17a) SHA1(bb7123610d3791c08577b87c8be59a0dd2cc33f1) )
	ROM_LOAD( "twm-0.216",         0x0400000, 0x400000, CRC(39cbc56a) SHA1(931d0d729620ef20e5c4fd521bce45cdb1742127) )
	ROM_LOAD( "twm-1.217",         0x0800000, 0x400000, CRC(fae0687a) SHA1(383a86f55441be287075af046ebac6a5ab54e6cf) )
	ROM_LOAD( "twm-2.218",         0x0c00000, 0x400000, CRC(cb852264) SHA1(a7a2f3d6f723ddd80c57ac63522a1a0bf526a7b3) )
	ROM_LOAD( "twm-3.219",         0x1000000, 0x400000, CRC(7c9f6925) SHA1(32519a238810d02181eaf5c2344334c523fa77d1) )
	ROM_LOAD( "twm-4.220",         0x1400000, 0x400000, CRC(17cd0ec9) SHA1(37581530e974af692ab71471d0238801cd19c843) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg13", 0x000000, 0x000008, CRC(5778418e) SHA1(7c4d6922bbe726e0f25297f8528e94b41023581b) )
ROM_END

ROM_START( mfjump )
	TPS_BIOS

	ROM_REGION32_LE( 0x2800000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "mfj-o.119", 0x0000001, 0x100000, CRC(0d724dc5) SHA1(2ba388fe6254c0cf3847fd173a414ee5ca31f4f4) )
	ROM_LOAD16_BYTE( "mfj-e.120", 0x0000000, 0x100000, CRC(86292bca) SHA1(b6a25ab828da3d5c8f6d945336513485708f3f5b) )
	ROM_LOAD( "mfj.216",          0x0400000, 0x400000, CRC(0d518dba) SHA1(100cd4d0a1e678e660336027f067a9a1f5cbad3e) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg14", 0x000000, 0x000008, CRC(5fa9a8c2) SHA1(d927760279ebda185fecf879e62ceefe938acd81) )
ROM_END


/* Video System (COH-1002V) */

#define VIDEO_SYSTEM_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "coh-1002v.ic353", 0x0000000, 0x080000, CRC(5ff165f3) SHA1(8f59314c1093446b9bcb06d232244da6df78e206) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "kn01.ic652", 0x000000, 0x000008, CRC(1ce271df) SHA1(b01479dc75a4a8a86bee7ac7cd10af2bda332544) )

#define VIDEO_SYSTEM_BANKED_ROMS \
	ROM_REGION32_LE( 0x1800000, REGION_USER2, 0 ) /* banked game rom, 1MB banks */ \
	ROM_LOAD( "ic11.bin", 0x0000000, 0x400000, CRC(a93f6fee) SHA1(6f079643b50833f8fb497c49945ad23326cc9170) ) \
	ROM_LOAD( "ic12.bin", 0x0400000, 0x400000, CRC(9f584ef7) SHA1(12c04e198f17d1915f58e83aff45ca2e76773df8) ) \
	ROM_LOAD( "ic13.bin", 0x0800000, 0x400000, CRC(652e9c78) SHA1(a929b2944de72606338acb822c1031463e2b1cc5) ) \
	ROM_LOAD( "ic14.bin", 0x0c00000, 0x400000, CRC(c4ef1424) SHA1(1734a6ee6d0be94d24afefcf2a125b74747f53d0) ) \
	ROM_LOAD( "ic15.bin", 0x1000000, 0x400000, CRC(2551d816) SHA1(e1500d4bfa8cc55220c366a5852263ac2070da82) ) \
	ROM_LOAD( "ic16.bin", 0x1400000, 0x400000, CRC(21b401bc) SHA1(89374b80453c474aa1dd3a219422f557f95a262c) ) \
	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */ \
	ROM_LOAD( "kn02", 0x000000, 0x000008, CRC(0d2001b4) SHA1(740d7794a1b94a42351d81947cbfae138c56ecc9) )

ROM_START( coh1002v )
	VIDEO_SYSTEM_BIOS
ROM_END

ROM_START( aerofgts )
	VIDEO_SYSTEM_BIOS

	ROM_REGION32_LE( 0x0280000, REGION_USER1, 0 ) /* fixed game rom */
	ROM_LOAD( "1-8_prog0_usa_aa9e.ic5", 0x0000000, 0x080000, CRC(d15a4904) SHA1(4e329f56584df4b057b56e21f0230dda4ec46bcf) )
	ROM_LOAD( "1-8_prog1_usa_0a1e.ic6", 0x0080000, 0x080000, CRC(d6625176) SHA1(018bba263effda1a0b64023aa08b384f0590245b) )
	ROM_LOAD( "1-8_prog2_usa_c4b3.ic7", 0x0100000, 0x080000, CRC(df5ba2f7) SHA1(19153084e7cff632380b67a2fff800644a2fbf7d) )
	ROM_LOAD( "1-8_prog3_usa_e1d8.ic8", 0x0180000, 0x080000, CRC(8711574a) SHA1(d02a9dac78ee468985c2f2c8d3b9a588f6fc7c02) )
	ROM_LOAD( "1-8_prog4_usa_9127.ic9", 0x0200000, 0x080000, CRC(670d9b27) SHA1(a4bca91c36d06cb0663d2478096bbc1668e01d60) )

	VIDEO_SYSTEM_BANKED_ROMS
ROM_END

ROM_START( aerofgtst )
	VIDEO_SYSTEM_BIOS

	ROM_REGION32_LE( 0x0280000, REGION_USER1, 0 ) /* fixed game rom */
	ROM_LOAD( "vs.ic5", 0x0000000, 0x080000, CRC(453dd514) SHA1(189fe5598485de160f5c0add90235ed63ed92747) )
	ROM_LOAD( "vs.ic6", 0x0080000, 0x080000, CRC(24257295) SHA1(fe6d969c407f2c26c3af2a2a5015dc83b1675e9a) )
	ROM_LOAD( "vs.ic7", 0x0100000, 0x080000, CRC(df5ba2f7) SHA1(19153084e7cff632380b67a2fff800644a2fbf7d) )
	ROM_LOAD( "vs.ic8", 0x0180000, 0x080000, CRC(df638f92) SHA1(5704eed2ef798a91a0398948af0324955ec38534) )
	ROM_LOAD( "vs.ic9", 0x0200000, 0x080000, CRC(1294aa24) SHA1(07ca2cc3f117cfd48d2ca558d9435a3fc238c0fe) )

	VIDEO_SYSTEM_BANKED_ROMS
ROM_END

ROM_START( sncwgltd )
	VIDEO_SYSTEM_BIOS

	ROM_REGION32_LE( 0x0280000, REGION_USER1, 0 ) /* fixed game rom */
	ROM_LOAD( "ic5.bin", 0x0000000, 0x080000, CRC(458f14aa) SHA1(b4e50be60ffb9b7911561dd35b6a7e0df3432a3a) )
	ROM_LOAD( "ic6.bin", 0x0080000, 0x080000, CRC(8233dd1e) SHA1(1422b4530d671e3b8b471ec16c20ef7c819ab762) )
	ROM_LOAD( "ic7.bin", 0x0100000, 0x080000, CRC(df5ba2f7) SHA1(19153084e7cff632380b67a2fff800644a2fbf7d) )
	ROM_LOAD( "ic8.bin", 0x0180000, 0x080000, CRC(e8145f2b) SHA1(3a1cb189426998856dfeda47267fde64be34c6ec) )
	ROM_LOAD( "ic9.bin", 0x0200000, 0x080000, CRC(605c9370) SHA1(9734549cae3028c089f4c9f2336ee374b3f950f8) )

	VIDEO_SYSTEM_BANKED_ROMS
ROM_END


/* Atlus (COH-1001L), MAME names */

#define ATLUS_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m534002c-17.ic353", 0x000000, 0x080000, CRC(6721146b) SHA1(9511d24bfe25eb180fb2db0835b131cb4a12730e) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "at01.ic652", 0x000000, 0x000008, CRC(181668b3) SHA1(8d2fe1facb4473cc20b4b477aaf881f9280939cf) )

ROM_START( coh1001l )
	ATLUS_BIOS
ROM_END

ROM_START( hvnsgate )
	ATLUS_BIOS

	ROM_REGION32_LE( 0x2000000, REGION_USER1, ROMREGION_ERASEFF ) /* game rom, 4 banks of 8MB */
	ROM_LOAD16_BYTE( "athg-01b.18", 0x0000001, 0x080000, CRC(e820136f) SHA1(2bc3465928dd08060736a2a67d98864d634275d6) )
	ROM_LOAD16_BYTE( "athg-02b.17", 0x0000000, 0x080000, CRC(11bfa89b) SHA1(f23e4c9d8eb90bd3bb3327d9950edd7a467ce8da) )
	ROM_LOAD( "athg-07.027",        0x0100000, 0x400000, CRC(46411f67) SHA1(2e8f37c3d9d7f5f3c79fca8ffeaf4c2fd1634b91) )
	ROM_LOAD( "athg-08.028",        0x0500000, 0x400000, CRC(85289345) SHA1(6385fe27451b80f97e7bad823b3b59eff3efa541) )
	ROM_LOAD( "athg-09.210",        0x0900000, 0x400000, CRC(19e558b5) SHA1(c195bc7dc3cfe4f099d27afdebd6f9cfe064e1df) )
	ROM_LOAD( "athg-10.029",        0x0d00000, 0x400000, CRC(748f936e) SHA1(134e78ea71bb9646f36cc503c704496a2b622ee9) )
	ROM_LOAD( "athg-11.215",        0x1100000, 0x200000, CRC(ac8e53bd) SHA1(002c4be1aa57d810c5d810c475631d9f14e1d2ab) )

	ROM_REGION( 0x040000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "athg-04.21", 0x000001, 0x020000, CRC(18523e85) SHA1(0ecc2116760f05fca8e5366b0a97dfe26fa9bc0c) )
	ROM_LOAD16_BYTE( "athg-03.22", 0x000000, 0x020000, CRC(7eef7e68) SHA1(65b8ae18ef4ff636c548326a360b481aeb316869) )

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF ) /* YMZ280B samples */
	ROM_LOAD( "athg-05.4136", 0x400000, 0x200000, CRC(74469a15) SHA1(0faa883900d7fd2e5240f486db33b3d868f1f05f) )
	ROM_LOAD( "athg-06.4134", 0x600000, 0x200000, CRC(443ade73) SHA1(6ef6aa68c525b9749833125dcab929d1d65d3b90) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "at02", 0x000000, 0x000008, CRC(55fe469e) SHA1(a99d41e21bd1df67b387558314763864eaae0265) )
ROM_END

/* Taito FX-1A (COH-1000T), MAME names */

#define TAITO_FX1_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m534002c-14.ic353", 0x0000000, 0x080000, CRC(e3f23b6e) SHA1(e18907cf8c6ba54d96edba0a9a00487a90219e0d) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "tt01.ic652", 0x000000, 0x000008, CRC(daac13e7) SHA1(96d94f2b06fb7ecd91454969f4959a583ffb9d5a) )

ROM_START( coh1000t )
	TAITO_FX1_BIOS
ROM_END

ROM_START( sfchamp )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e18-12.2",  0x0000001, 0x080000, CRC(72304685) SHA1(2e6f645871e19a49fcdfbdca49c6be415471eadf) )
	ROM_LOAD16_BYTE( "e18-13.7",  0x0000000, 0x080000, CRC(fa4d01ee) SHA1(27efd8e2107d71213d35f2a58762ed8812f809d3) )
	ROM_LOAD( "e18-02.12",        0x0600000, 0x200000, CRC(c7b4fe29) SHA1(7f823bd61abf2b15d3ba62bca829a5b1acacfd09) )
	ROM_LOAD( "e18-03.16",        0x0800000, 0x200000, CRC(76392346) SHA1(2c5b70c4708208f866feea0472fcc72333061124) )
	ROM_LOAD( "e18-04.19",        0x0a00000, 0x200000, CRC(fc3731da) SHA1(58948aad8d7bb7a8449d2bf12e9d5e6d7b4426b5) )
	ROM_LOAD( "e18-05.21",        0x0c00000, 0x200000, CRC(2e984c50) SHA1(6d8255e38c67d68bf489c9885663ed2edf148188) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e18-09.22", 0x010000, 0x020000, CRC(bb5a5319) SHA1(0bb700cafc157d3af663cc9bebb8167487ff2852) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e18-01.15", 0x000000, 0x200000, CRC(dbd1408c) SHA1(ef81064f2f95e5ae25eb1f10d1e78f27f9e294f5) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt02", 0x000000, 0x000008, CRC(b1585ac7) SHA1(d967eea539de2dd7a8d13fa2785c3033065bed9c) )
ROM_END

ROM_START( sfchampo )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e18-11.2",  0x0000001, 0x080000, CRC(f5462f30) SHA1(44eb03a9b51e2d8dd14fe2ed36dbcf17035a22c7) )
	ROM_LOAD16_BYTE( "e18-08.7",  0x0000000, 0x080000, CRC(6a5558cd) SHA1(75b26bcaaa213283e7e0dace69ee58f305b4572d) )
	ROM_LOAD( "e18-02.12",        0x0600000, 0x200000, CRC(c7b4fe29) SHA1(7f823bd61abf2b15d3ba62bca829a5b1acacfd09) )
	ROM_LOAD( "e18-03.16",        0x0800000, 0x200000, CRC(76392346) SHA1(2c5b70c4708208f866feea0472fcc72333061124) )
	ROM_LOAD( "e18-04.19",        0x0a00000, 0x200000, CRC(fc3731da) SHA1(58948aad8d7bb7a8449d2bf12e9d5e6d7b4426b5) )
	ROM_LOAD( "e18-05.21",        0x0c00000, 0x200000, CRC(2e984c50) SHA1(6d8255e38c67d68bf489c9885663ed2edf148188) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e18-09.22", 0x010000, 0x020000, CRC(bb5a5319) SHA1(0bb700cafc157d3af663cc9bebb8167487ff2852) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e18-01.15", 0x000000, 0x200000, CRC(dbd1408c) SHA1(ef81064f2f95e5ae25eb1f10d1e78f27f9e294f5) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt02", 0x000000, 0x000008, CRC(b1585ac7) SHA1(d967eea539de2dd7a8d13fa2785c3033065bed9c) )
ROM_END

ROM_START( sfchampu )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e18-10.2",  0x0000001, 0x080000, CRC(82411fa6) SHA1(0aa1764b7ff68258ef76a41355c50d5067262d75) )
	ROM_LOAD16_BYTE( "e18-08.7",  0x0000000, 0x080000, CRC(6a5558cd) SHA1(75b26bcaaa213283e7e0dace69ee58f305b4572d) )
	ROM_LOAD( "e18-02.12",        0x0600000, 0x200000, CRC(c7b4fe29) SHA1(7f823bd61abf2b15d3ba62bca829a5b1acacfd09) )
	ROM_LOAD( "e18-03.16",        0x0800000, 0x200000, CRC(76392346) SHA1(2c5b70c4708208f866feea0472fcc72333061124) )
	ROM_LOAD( "e18-04.19",        0x0a00000, 0x200000, CRC(fc3731da) SHA1(58948aad8d7bb7a8449d2bf12e9d5e6d7b4426b5) )
	ROM_LOAD( "e18-05.21",        0x0c00000, 0x200000, CRC(2e984c50) SHA1(6d8255e38c67d68bf489c9885663ed2edf148188) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e18-09.22", 0x010000, 0x020000, CRC(bb5a5319) SHA1(0bb700cafc157d3af663cc9bebb8167487ff2852) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e18-01.15", 0x000000, 0x200000, CRC(dbd1408c) SHA1(ef81064f2f95e5ae25eb1f10d1e78f27f9e294f5) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt02", 0x000000, 0x000008, CRC(b1585ac7) SHA1(d967eea539de2dd7a8d13fa2785c3033065bed9c) )
ROM_END

ROM_START( sfchampj )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e18-07.2",  0x0000001, 0x080000, CRC(1b484e1c) SHA1(f29f40a9988475d8abbb126095b0716133c087a0) )
	ROM_LOAD16_BYTE( "e18-08.7",  0x0000000, 0x080000, CRC(6a5558cd) SHA1(75b26bcaaa213283e7e0dace69ee58f305b4572d) )
	ROM_LOAD( "e18-02.12",        0x0600000, 0x200000, CRC(c7b4fe29) SHA1(7f823bd61abf2b15d3ba62bca829a5b1acacfd09) )
	ROM_LOAD( "e18-03.16",        0x0800000, 0x200000, CRC(76392346) SHA1(2c5b70c4708208f866feea0472fcc72333061124) )
	ROM_LOAD( "e18-04.19",        0x0a00000, 0x200000, CRC(fc3731da) SHA1(58948aad8d7bb7a8449d2bf12e9d5e6d7b4426b5) )
	ROM_LOAD( "e18-05.21",        0x0c00000, 0x200000, CRC(2e984c50) SHA1(6d8255e38c67d68bf489c9885663ed2edf148188) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e18-09.22", 0x010000, 0x020000, CRC(bb5a5319) SHA1(0bb700cafc157d3af663cc9bebb8167487ff2852) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e18-01.15", 0x000000, 0x200000, CRC(dbd1408c) SHA1(ef81064f2f95e5ae25eb1f10d1e78f27f9e294f5) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt02", 0x000000, 0x000008, CRC(b1585ac7) SHA1(d967eea539de2dd7a8d13fa2785c3033065bed9c) )
ROM_END

ROM_START( psyforce )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e22-09_.2", 0x0000001, 0x080000, CRC(997e4500) SHA1(4a90b452c9a877ccec55a11f36c4cbc6df1f1f41) )
	ROM_LOAD16_BYTE( "e22-06_.7", 0x0000000, 0x080000, CRC(f6341d63) SHA1(99dc27aa694ae5951148054291912a486726e8c9) )
	ROM_LOAD( "e22-02.16",        0x0800000, 0x200000, CRC(03b50064) SHA1(0259537e86b266b3f34308c4fc0bcc04c037da71) )
	ROM_LOAD( "e22-03.19",        0x0a00000, 0x200000, CRC(8372f839) SHA1(646b3919b6be63412c11850ec1524685abececc0) )
	ROM_LOAD( "e22-04.21",        0x0c00000, 0x200000, CRC(397b71aa) SHA1(48743c362503c1d2dbeb3c8be4cb2aaaae015b88) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e22-07.22", 0x010000, 0x020000, CRC(739af589) SHA1(dbb4d1c6d824a99ccf27168e2c21644e19811523) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e22-01.15", 0x000000, 0x200000, CRC(808b8340) SHA1(d8bde850dd9b5b71e94ea707d2d728754f907977) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt03", 0x000000, 0x000008, CRC(5799d8c9) SHA1(72cf1ed0958959d085ca5f8ab8b4f883ab1cd162) )
ROM_END

ROM_START( psyforcej )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e22-05_.2", 0x0000001, 0x080000, CRC(7770242c) SHA1(dd37575d3d9ffdef60fe0e4cab6c9e42d087f714) )
	ROM_LOAD16_BYTE( "e22-10_.7", 0x0000000, 0x080000, CRC(f6341d63) SHA1(99dc27aa694ae5951148054291912a486726e8c9) )
	ROM_LOAD( "e22-02.16",        0x0800000, 0x200000, CRC(03b50064) SHA1(0259537e86b266b3f34308c4fc0bcc04c037da71) )
	ROM_LOAD( "e22-03.19",        0x0a00000, 0x200000, CRC(8372f839) SHA1(646b3919b6be63412c11850ec1524685abececc0) )
	ROM_LOAD( "e22-04.21",        0x0c00000, 0x200000, CRC(397b71aa) SHA1(48743c362503c1d2dbeb3c8be4cb2aaaae015b88) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e22-07.22", 0x010000, 0x020000, CRC(739af589) SHA1(dbb4d1c6d824a99ccf27168e2c21644e19811523) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e22-01.15", 0x000000, 0x200000, CRC(808b8340) SHA1(d8bde850dd9b5b71e94ea707d2d728754f907977) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt03", 0x000000, 0x000008, CRC(5799d8c9) SHA1(72cf1ed0958959d085ca5f8ab8b4f883ab1cd162) )
ROM_END

ROM_START( psyforcex )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e22-11.2",  0x0000001, 0x080000, CRC(29ebebc9) SHA1(48a3371460d69ae5ba58d3e24615f389bf43b2cc) )
	ROM_LOAD16_BYTE( "e22-12.7",  0x0000000, 0x080000, CRC(7426ffc5) SHA1(24b0132241e2e49109e585b082bf4ab67f86b294) )
	ROM_LOAD( "e22-02.16",        0x0800000, 0x200000, CRC(03b50064) SHA1(0259537e86b266b3f34308c4fc0bcc04c037da71) )
	ROM_LOAD( "e22-03.19",        0x0a00000, 0x200000, CRC(8372f839) SHA1(646b3919b6be63412c11850ec1524685abececc0) )
	ROM_LOAD( "e22-04.21",        0x0c00000, 0x200000, CRC(397b71aa) SHA1(48743c362503c1d2dbeb3c8be4cb2aaaae015b88) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e22-07.22", 0x010000, 0x020000, CRC(739af589) SHA1(dbb4d1c6d824a99ccf27168e2c21644e19811523) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e22-01.15", 0x000000, 0x200000, CRC(808b8340) SHA1(d8bde850dd9b5b71e94ea707d2d728754f907977) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt03", 0x000000, 0x000008, CRC(5799d8c9) SHA1(72cf1ed0958959d085ca5f8ab8b4f883ab1cd162) )
ROM_END

ROM_START( mgcldtex )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e32-08.2",  0x0000001, 0x100000, CRC(3d42cd28) SHA1(9017922e835a359ba5126c8a9e8c27380a5ce081) )
	ROM_LOAD16_BYTE( "e32-09.7",  0x0000000, 0x100000, CRC(db7ec115) SHA1(fa6f18de71ba997389d887d7ffe745aa25e24c20) )
	ROM_LOAD( "e32-01.1",         0x0400000, 0x400000, CRC(cf5f1d01) SHA1(5417f8aef5c8d0e9e63ba8c68efb5b3ef37b4693) )
	ROM_LOAD( "e32-02.6",         0x0800000, 0x400000, CRC(61c8438c) SHA1(bdbe6079cc634c0cd6580f76619eb2944c9a31d9) )
	ROM_LOAD( "e32-03.12",        0x0c00000, 0x200000, CRC(190d1618) SHA1(838a651d32752015baa7e8caea62fd739631b8be) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e32-10.22", 0x010000, 0x020000, CRC(adf3feb5) SHA1(bae5bc3fad99a92a3492be1b775dab861007eb3b) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e32-04.15", 0x000000, 0x400000, CRC(c72f9eea) SHA1(7ab8b412a8ed00a42016acb7d13d3b074155780a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt06", 0x000000, 0x000008, CRC(ce282fc9) SHA1(dfc1f679ec70f2ff32869cfeb452acf9ac154aef) )
ROM_END

ROM_START( mgcldate )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e32-05.2",  0x0000001, 0x080000, CRC(72fc7f7b) SHA1(50d9e84bc74fb63ec1900ab149051888bc3d03a5) )
	ROM_LOAD16_BYTE( "e32-06.7",  0x0000000, 0x080000, CRC(d11c3881) SHA1(f7046c5bed4818152edcf697a49664b0bcf12a1b) )
	ROM_LOAD( "e32-01.1",         0x0400000, 0x400000, CRC(cf5f1d01) SHA1(5417f8aef5c8d0e9e63ba8c68efb5b3ef37b4693) )
	ROM_LOAD( "e32-02.6",         0x0800000, 0x400000, CRC(61c8438c) SHA1(bdbe6079cc634c0cd6580f76619eb2944c9a31d9) )
	ROM_LOAD( "e32-03.12",        0x0c00000, 0x200000, CRC(190d1618) SHA1(838a651d32752015baa7e8caea62fd739631b8be) )

	ROM_REGION( 0x030000, REGION_CPU2, 0 ) /* z80, banked at 0x10000 */
	ROM_LOAD( "e32-07.22", 0x010000, 0x020000, CRC(adf3feb5) SHA1(bae5bc3fad99a92a3492be1b775dab861007eb3b) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ym2610b adpcm-a */
	ROM_LOAD( "e32-04.15", 0x000000, 0x400000, CRC(c72f9eea) SHA1(7ab8b412a8ed00a42016acb7d13d3b074155780a) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt06", 0x000000, 0x000008, CRC(ce282fc9) SHA1(dfc1f679ec70f2ff32869cfeb452acf9ac154aef) )
ROM_END

/* Taito FX-1B (COH-1000T + ZROM), MAME names */

ROM_START( raystorm )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e24-xx.ic4", 0x0000001, 0x080000, CRC(33f63638) SHA1(fdda33ffc9902b3605a3272fae5a614e93856a86) )
	ROM_LOAD16_BYTE( "e24-xx.ic3", 0x0000000, 0x080000, CRC(5eeed3b2) SHA1(d8bb1613d7285eabdc6f0a2d231d2eeeb52f307b) )
	ROM_LOAD( "e24-02.1",          0x0400000, 0x400000, CRC(9f70950d) SHA1(b3e4f925a61ae2e5dd4cc5d7ec3030a0d5c2c04d) )
	ROM_LOAD( "e24-03.2",          0x0800000, 0x400000, CRC(6c1f0a5d) SHA1(1aac37a7ff23e54021a4cec18c9bb93242337180) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e24-09.14", 0x080000, 0x080000, CRC(808589e1) SHA1(46ada4c6d68c2462186a0b962abb435ee740c0ba) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e24-04.27", 0x000000, 0x400000, CRC(f403493a) SHA1(3e49fd2a060a3893e26f14cc3cf47c4ba91e17d4) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt04", 0x000000, 0x000008, CRC(013363df) SHA1(f04d04c7c858a8e2eaff0c51e782ad76d6ebbed1) )
ROM_END

ROM_START( raystormo )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e24-08.4", 0x0000001, 0x080000, CRC(ae071b95) SHA1(0e1597220808d6e3998ef1e9d88779e0187ba0af) )
	ROM_LOAD16_BYTE( "e24-06.3", 0x0000000, 0x080000, CRC(d70cdf46) SHA1(da6163d69d3ea9c1e3f4b7961a548f1f9d8d9909) )
	ROM_LOAD( "e24-02.1",        0x0400000, 0x400000, CRC(9f70950d) SHA1(b3e4f925a61ae2e5dd4cc5d7ec3030a0d5c2c04d) )
	ROM_LOAD( "e24-03.2",        0x0800000, 0x400000, CRC(6c1f0a5d) SHA1(1aac37a7ff23e54021a4cec18c9bb93242337180) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e24-09.14", 0x080000, 0x080000, CRC(808589e1) SHA1(46ada4c6d68c2462186a0b962abb435ee740c0ba) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e24-04.27", 0x000000, 0x400000, CRC(f403493a) SHA1(3e49fd2a060a3893e26f14cc3cf47c4ba91e17d4) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt04", 0x000000, 0x000008, CRC(013363df) SHA1(f04d04c7c858a8e2eaff0c51e782ad76d6ebbed1) )
ROM_END

ROM_START( raystormu )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e24-07.4", 0x0000001, 0x080000, CRC(d9002b03) SHA1(bdb0aa88536c4c98c150ece87387930b3dbdd258) )
	ROM_LOAD16_BYTE( "e24-06.3", 0x0000000, 0x080000, CRC(d70cdf46) SHA1(da6163d69d3ea9c1e3f4b7961a548f1f9d8d9909) )
	ROM_LOAD( "e24-02.1",        0x0400000, 0x400000, CRC(9f70950d) SHA1(b3e4f925a61ae2e5dd4cc5d7ec3030a0d5c2c04d) )
	ROM_LOAD( "e24-03.2",        0x0800000, 0x400000, CRC(6c1f0a5d) SHA1(1aac37a7ff23e54021a4cec18c9bb93242337180) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e24-09.14", 0x080000, 0x080000, CRC(808589e1) SHA1(46ada4c6d68c2462186a0b962abb435ee740c0ba) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e24-04.27", 0x000000, 0x400000, CRC(f403493a) SHA1(3e49fd2a060a3893e26f14cc3cf47c4ba91e17d4) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt04", 0x000000, 0x000008, CRC(013363df) SHA1(f04d04c7c858a8e2eaff0c51e782ad76d6ebbed1) )
ROM_END

ROM_START( raystormj )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e24-05.4", 0x0000001, 0x080000, CRC(40097ab9) SHA1(67e73568b35515c2c5a9119e97ac4709baff8c5a) )
	ROM_LOAD16_BYTE( "e24-06.3", 0x0000000, 0x080000, CRC(d70cdf46) SHA1(da6163d69d3ea9c1e3f4b7961a548f1f9d8d9909) )
	ROM_LOAD( "e24-02.1",        0x0400000, 0x400000, CRC(9f70950d) SHA1(b3e4f925a61ae2e5dd4cc5d7ec3030a0d5c2c04d) )
	ROM_LOAD( "e24-03.2",        0x0800000, 0x400000, CRC(6c1f0a5d) SHA1(1aac37a7ff23e54021a4cec18c9bb93242337180) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e24-09.14", 0x080000, 0x080000, CRC(808589e1) SHA1(46ada4c6d68c2462186a0b962abb435ee740c0ba) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e24-04.27", 0x000000, 0x400000, CRC(f403493a) SHA1(3e49fd2a060a3893e26f14cc3cf47c4ba91e17d4) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt04", 0x000000, 0x000008, CRC(013363df) SHA1(f04d04c7c858a8e2eaff0c51e782ad76d6ebbed1) )
ROM_END

ROM_START( ftimpcta )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e25-13.4", 0x0000001, 0x100000, CRC(7f078d7b) SHA1(df9800dd6885dbc33736c5143d877b0847221061) )
	ROM_LOAD16_BYTE( "e25-14.3", 0x0000000, 0x100000, CRC(0c5f474f) SHA1(ce7031ba860297b99cddd6d0177f07e03520faeb) )
	ROM_LOAD( "e25-01.1",        0x0400000, 0x400000, CRC(8cc4be0c) SHA1(9ca15558a83b7e332e50accf1f7852444a7ce730) )
	ROM_LOAD( "e25-02.2",        0x0800000, 0x400000, CRC(8e8b4c82) SHA1(55c9d4d3a08fc3226a75ab3a674be433af83e289) )
	ROM_LOAD( "e25-03.12",       0x0c00000, 0x400000, CRC(43b1c085) SHA1(6e53550e9be0d2f415fc6b4f3b8a71185c5370b2) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e25-10.14", 0x080000, 0x080000, CRC(2b2ad1b1) SHA1(6d064d0b6805d43ce42929ac8f5645b56384f53c) )

	ROM_REGION32_LE( 0x600000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e25-04.27", 0x000000, 0x400000, CRC(09a66d35) SHA1(f0df24bc9bfc9eb0f5150dc035c19fc5b8a39bf9) )
	ROM_LOAD( "e25-05.28", 0x400000, 0x200000, CRC(3fb57636) SHA1(aa38bfac11ecf10fd55143cf4525a2a529be8bb6) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt05", 0x000000, 0x000008, CRC(66045c83) SHA1(b040fa87e855fd512fa99e80056d37aba895805b) )
ROM_END

ROM_START( ftimpact )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e25-09.4", 0x0000001, 0x080000, CRC(d457bfc7) SHA1(e974a9c3e7b0748ef89d78e76a7dbb763c42b6f7) )
	ROM_LOAD16_BYTE( "e25-07.3", 0x0000000, 0x080000, CRC(829be1cc) SHA1(64b139d7c3696ab2f0b9a4842c19a38fe6a8cede) )
	ROM_LOAD( "e25-01.1",        0x0400000, 0x400000, CRC(8cc4be0c) SHA1(9ca15558a83b7e332e50accf1f7852444a7ce730) )
	ROM_LOAD( "e25-02.2",        0x0800000, 0x400000, CRC(8e8b4c82) SHA1(55c9d4d3a08fc3226a75ab3a674be433af83e289) )
	ROM_LOAD( "e25-03.12",       0x0c00000, 0x400000, CRC(43b1c085) SHA1(6e53550e9be0d2f415fc6b4f3b8a71185c5370b2) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e25-10.14", 0x080000, 0x080000, CRC(2b2ad1b1) SHA1(6d064d0b6805d43ce42929ac8f5645b56384f53c) )

	ROM_REGION32_LE( 0x600000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e25-04.27", 0x000000, 0x400000, CRC(09a66d35) SHA1(f0df24bc9bfc9eb0f5150dc035c19fc5b8a39bf9) )
	ROM_LOAD( "e25-05.28", 0x400000, 0x200000, CRC(3fb57636) SHA1(aa38bfac11ecf10fd55143cf4525a2a529be8bb6) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt05", 0x000000, 0x000008, CRC(66045c83) SHA1(b040fa87e855fd512fa99e80056d37aba895805b) )
ROM_END

ROM_START( ftimpactu )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e25-08.4", 0x0000001, 0x080000, CRC(a3508f51) SHA1(fd4c3cc186e280497dc905ebda92472d5b72b1b4) )
	ROM_LOAD16_BYTE( "e25-07.3", 0x0000000, 0x080000, CRC(829be1cc) SHA1(64b139d7c3696ab2f0b9a4842c19a38fe6a8cede) )
	ROM_LOAD( "e25-01.1",        0x0400000, 0x400000, CRC(8cc4be0c) SHA1(9ca15558a83b7e332e50accf1f7852444a7ce730) )
	ROM_LOAD( "e25-02.2",        0x0800000, 0x400000, CRC(8e8b4c82) SHA1(55c9d4d3a08fc3226a75ab3a674be433af83e289) )
	ROM_LOAD( "e25-03.12",       0x0c00000, 0x400000, CRC(43b1c085) SHA1(6e53550e9be0d2f415fc6b4f3b8a71185c5370b2) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e25-10.14", 0x080000, 0x080000, CRC(2b2ad1b1) SHA1(6d064d0b6805d43ce42929ac8f5645b56384f53c) )

	ROM_REGION32_LE( 0x600000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e25-04.27", 0x000000, 0x400000, CRC(09a66d35) SHA1(f0df24bc9bfc9eb0f5150dc035c19fc5b8a39bf9) )
	ROM_LOAD( "e25-05.28", 0x400000, 0x200000, CRC(3fb57636) SHA1(aa38bfac11ecf10fd55143cf4525a2a529be8bb6) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt05", 0x000000, 0x000008, CRC(66045c83) SHA1(b040fa87e855fd512fa99e80056d37aba895805b) )
ROM_END

ROM_START( ftimpactj )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e25-06.4", 0x0000001, 0x080000, CRC(3a59deeb) SHA1(4377c5829fb5b6f5d0120caf992b1ee714897641) )
	ROM_LOAD16_BYTE( "e25-07.3", 0x0000000, 0x080000, CRC(829be1cc) SHA1(64b139d7c3696ab2f0b9a4842c19a38fe6a8cede) )
	ROM_LOAD( "e25-01.1",        0x0400000, 0x400000, CRC(8cc4be0c) SHA1(9ca15558a83b7e332e50accf1f7852444a7ce730) )
	ROM_LOAD( "e25-02.2",        0x0800000, 0x400000, CRC(8e8b4c82) SHA1(55c9d4d3a08fc3226a75ab3a674be433af83e289) )
	ROM_LOAD( "e25-03.12",       0x0c00000, 0x400000, CRC(43b1c085) SHA1(6e53550e9be0d2f415fc6b4f3b8a71185c5370b2) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e25-10.14", 0x080000, 0x080000, CRC(2b2ad1b1) SHA1(6d064d0b6805d43ce42929ac8f5645b56384f53c) )

	ROM_REGION32_LE( 0x600000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e25-04.27", 0x000000, 0x400000, CRC(09a66d35) SHA1(f0df24bc9bfc9eb0f5150dc035c19fc5b8a39bf9) )
	ROM_LOAD( "e25-05.28", 0x400000, 0x200000, CRC(3fb57636) SHA1(aa38bfac11ecf10fd55143cf4525a2a529be8bb6) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt05", 0x000000, 0x000008, CRC(66045c83) SHA1(b040fa87e855fd512fa99e80056d37aba895805b) )
ROM_END

ROM_START( gdarius2 )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e39-12.4", 0x0000001, 0x100000, CRC(b23266c3) SHA1(80aaddaaf10e40280ade4c7d11f45ddab47ee9a6) )
	ROM_LOAD16_BYTE( "e39-11.3", 0x0000000, 0x100000, CRC(766f73df) SHA1(9ce24c153920d259bc7fdef0778083eb6d639be3) )
	ROM_LOAD( "e39-01.1",        0x0400000, 0x400000, CRC(bdaaa251) SHA1(a42daa706ee859c2b66be179e08c0ad7990f919e) )
	ROM_LOAD( "e39-02.2",        0x0800000, 0x400000, CRC(a47aab5d) SHA1(64b58e47035ad9d8d6dcaf475cbcc3ad85f4d82f) )
	ROM_LOAD( "e39-03.12",       0x0c00000, 0x400000, CRC(a883b6a5) SHA1(b8d00d944c90f8cd9c2b076688f4c68b2e6d557a) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e39-07.14", 0x080000, 0x080000, CRC(2252c7c1) SHA1(92b9908e0d87cad6587f1acc0eef69eaae8c6a98) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e39-04.27", 0x000000, 0x400000, CRC(6ee35e68) SHA1(fdfe63203d8cecf84cb869039fb893d5b63cdd67) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt07", 0x000000, 0x000008, CRC(ccf2f332) SHA1(6eb07cd5fab29f5536fd94aa88a2b09f28cc3494) )
ROM_END

ROM_START( gdarius )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e39-09.ic4", 0x0000001, 0x100000, CRC(f457794d) SHA1(43571770b239298e3442d3da8aa4f4507380d639) )
	ROM_LOAD16_BYTE( "e39-10.ic3", 0x0000000, 0x100000, CRC(6ba4d941) SHA1(75f2d8c920d29102c09e041fc3198e32ad57dbaf) )
	ROM_LOAD( "e39-01.1",          0x0400000, 0x400000, CRC(bdaaa251) SHA1(a42daa706ee859c2b66be179e08c0ad7990f919e) )
	ROM_LOAD( "e39-02.2",          0x0800000, 0x400000, CRC(a47aab5d) SHA1(64b58e47035ad9d8d6dcaf475cbcc3ad85f4d82f) )
	ROM_LOAD( "e39-03.12",         0x0c00000, 0x400000, CRC(a883b6a5) SHA1(b8d00d944c90f8cd9c2b076688f4c68b2e6d557a) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e39-07.14", 0x080000, 0x080000, CRC(2252c7c1) SHA1(92b9908e0d87cad6587f1acc0eef69eaae8c6a98) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e39-04.27", 0x000000, 0x400000, CRC(6ee35e68) SHA1(fdfe63203d8cecf84cb869039fb893d5b63cdd67) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt07", 0x000000, 0x000008, CRC(ccf2f332) SHA1(6eb07cd5fab29f5536fd94aa88a2b09f28cc3494) )
ROM_END

ROM_START( gdariusu )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e39-08.ic4", 0x0000001, 0x100000, CRC(835049db) SHA1(2b230c8fd6c6ea4e30740fda28f631344b018b79) )
	ROM_LOAD16_BYTE( "e39-10.ic3", 0x0000000, 0x100000, CRC(6ba4d941) SHA1(75f2d8c920d29102c09e041fc3198e32ad57dbaf) )
	ROM_LOAD( "e39-01.1",          0x0400000, 0x400000, CRC(bdaaa251) SHA1(a42daa706ee859c2b66be179e08c0ad7990f919e) )
	ROM_LOAD( "e39-02.2",          0x0800000, 0x400000, CRC(a47aab5d) SHA1(64b58e47035ad9d8d6dcaf475cbcc3ad85f4d82f) )
	ROM_LOAD( "e39-03.12",         0x0c00000, 0x400000, CRC(a883b6a5) SHA1(b8d00d944c90f8cd9c2b076688f4c68b2e6d557a) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e39-07.14", 0x080000, 0x080000, CRC(2252c7c1) SHA1(92b9908e0d87cad6587f1acc0eef69eaae8c6a98) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e39-04.27", 0x000000, 0x400000, CRC(6ee35e68) SHA1(fdfe63203d8cecf84cb869039fb893d5b63cdd67) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt07", 0x000000, 0x000008, CRC(ccf2f332) SHA1(6eb07cd5fab29f5536fd94aa88a2b09f28cc3494) )
ROM_END

ROM_START( gdariusj )
	TAITO_FX1_BIOS

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "e39-06.4", 0x0000001, 0x100000, CRC(2980c30d) SHA1(597321642125c3ae37581c2d9abc2723c7909996) )
	ROM_LOAD16_BYTE( "e39-05.3", 0x0000000, 0x100000, CRC(750e5b13) SHA1(68fe9cbd7d506cfd587dccc40b6ae0b0b6ee7c29) )
	ROM_LOAD( "e39-01.1",        0x0400000, 0x400000, CRC(bdaaa251) SHA1(a42daa706ee859c2b66be179e08c0ad7990f919e) )
	ROM_LOAD( "e39-02.2",        0x0800000, 0x400000, CRC(a47aab5d) SHA1(64b58e47035ad9d8d6dcaf475cbcc3ad85f4d82f) )
	ROM_LOAD( "e39-03.12",       0x0c00000, 0x400000, CRC(a883b6a5) SHA1(b8d00d944c90f8cd9c2b076688f4c68b2e6d557a) )

	ROM_REGION16_LE( 0x100000, REGION_CPU2, 0 ) /* mn10200 */
	ROM_LOAD( "e39-07.14", 0x080000, 0x080000, CRC(2252c7c1) SHA1(92b9908e0d87cad6587f1acc0eef69eaae8c6a98) )

	ROM_REGION32_LE( 0x400000, REGION_SOUND1, 0 ) /* zsg2 */
	ROM_LOAD( "e39-04.27", 0x000000, 0x400000, CRC(6ee35e68) SHA1(fdfe63203d8cecf84cb869039fb893d5b63cdd67) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tt07", 0x000000, 0x000008, CRC(ccf2f332) SHA1(6eb07cd5fab29f5536fd94aa88a2b09f28cc3494) )
ROM_END

/* Taito FX1 old BIOS layout, for the Beastorizer bootleg (zn machine) */

#define TAITOFX1_BIOS \
	ROM_REGION( 0x480400, REGION_CPU1, 0 ) \
	ROM_LOAD( "coh1002t.bin", 0x0400000, 0x080000, CRC(e3f23b6e) SHA1(e18907cf8c6ba54d96edba0a9a00487a90219e0d) )

ROM_START( taitofx1 )
	TAITOFX1_BIOS
ROM_END

/* Eighting / Raizing (COH-1002E) */

#define RAIZING_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m27c402cz-54.ic353", 0x0000000, 0x080000, CRC(910f3a8b) SHA1(cd68532967a25f476a6d73473ec6b6f4df2e1689) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "et01.ic652", 0x000000, 0x000008, CRC(a7dd922e) SHA1(1069c1d9015028a51a1b314cfacb014ea90aa425) )

ROM_START( coh1002e )
	RAIZING_BIOS
ROM_END

ROM_START( beastrzr )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD16_BYTE( "b.roar_u0213.u0213",  0x000001, 0x080000, CRC(2c586534) SHA1(a38dfc3a45446d24a1caac89b0f560989d46ded5) )
	ROM_LOAD16_BYTE( "b.roar_u0212.u0212",  0x000000, 0x080000, CRC(1c85d7fb) SHA1(aa406a42c424cc16a9e5330c68dda9acf8760088) )
	ROM_LOAD16_BYTE( "b.roar_u0215.u0215",  0x100001, 0x080000, CRC(31c8e055) SHA1(2811789ab6221b972d1e3ffe98916587990f7564) )
	ROM_LOAD16_BYTE( "b.roar_u0214.u0214",  0x100000, 0x080000, CRC(1cdc450a) SHA1(9215e5fec52f7c5c0070feb621eb9c77f98e2362) )
	ROM_LOAD( "8ing_ra-b.roar_rom-1.u0217", 0x400000, 0x400000, CRC(11f1ba36) SHA1(d41ae686c2c607640cbadf906215c89134758050) )
	ROM_LOAD( "8ing_ra-b.roar_rom-2.u0216", 0x800000, 0x400000, CRC(d46d46b7) SHA1(1c42cb5dcda4b26c08c4ecf95efeadaf3a1d1dd2) )

	ROM_REGION( 0x080000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "b.roar_u046.u046", 0x000001, 0x040000, CRC(d4bb261a) SHA1(9a295b1354ef15f37ea09bb209cf0cb98437c462) )
	ROM_LOAD16_BYTE( "b.roar_u042.u042", 0x000000, 0x040000, CRC(4d537f88) SHA1(1760367d70a81606e29885ea315185d2c2a9409b) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "8ing_ra-b.roar_rom-3.u0326", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et02", 0x000000, 0x000008, CRC(187ce61a) SHA1(521122b0f7b3f278dd2a2b1d73c252b952b5f55d) )
ROM_END

ROM_START( beastrzra )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD16_BYTE( "b.roar_u0213.u0213",  0x000001, 0x080000, CRC(5c3ca4d3) SHA1(f246de057c2ea725e5e628dd32597f3bc7d22070) )
	ROM_LOAD16_BYTE( "b.roar_u0212.u0212",  0x000000, 0x080000, CRC(45f1269b) SHA1(4754bc97c261dfcb5e379fe765689ea737dd5e1d) )
	ROM_LOAD16_BYTE( "b.roar_u0215.u0215",  0x100001, 0x080000, CRC(31c8e055) SHA1(2811789ab6221b972d1e3ffe98916587990f7564) )
	ROM_LOAD16_BYTE( "b.roar_u0214.u0214",  0x100000, 0x080000, CRC(1cdc450a) SHA1(9215e5fec52f7c5c0070feb621eb9c77f98e2362) )
	ROM_LOAD( "8ing_ra-b.roar_rom-1.u0217", 0x400000, 0x400000, CRC(11f1ba36) SHA1(d41ae686c2c607640cbadf906215c89134758050) )
	ROM_LOAD( "8ing_ra-b.roar_rom-2.u0216", 0x800000, 0x400000, CRC(d46d46b7) SHA1(1c42cb5dcda4b26c08c4ecf95efeadaf3a1d1dd2) )

	ROM_REGION( 0x080000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "b.roar_u046.u046", 0x000001, 0x040000, CRC(d4bb261a) SHA1(9a295b1354ef15f37ea09bb209cf0cb98437c462) )
	ROM_LOAD16_BYTE( "b.roar_u042.u042", 0x000000, 0x040000, CRC(4d537f88) SHA1(1760367d70a81606e29885ea315185d2c2a9409b) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "8ing_ra-b.roar_rom-3.u0326", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et02", 0x000000, 0x000008, CRC(187ce61a) SHA1(521122b0f7b3f278dd2a2b1d73c252b952b5f55d) )
ROM_END

ROM_START( bldyroar )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD16_BYTE( "b.roar_u0213.u0213",  0x000001, 0x080000, CRC(63769342) SHA1(7231188073b997b039467db85ce7c85383daf591) )
	ROM_LOAD16_BYTE( "b.roar_u0212.u0212",  0x000000, 0x080000, CRC(966b7169) SHA1(63e025cacb84e89d30b40ed6cfa5c63d84c298c4) )
	ROM_LOAD16_BYTE( "b.roar_u0215.u0215",  0x100001, 0x080000, CRC(31c8e055) SHA1(2811789ab6221b972d1e3ffe98916587990f7564) )
	ROM_LOAD16_BYTE( "b.roar_u0214.u0214",  0x100000, 0x080000, CRC(1cdc450a) SHA1(9215e5fec52f7c5c0070feb621eb9c77f98e2362) )
	ROM_LOAD( "8ing_ra-b.roar_rom-1.u0217", 0x400000, 0x400000, CRC(11f1ba36) SHA1(d41ae686c2c607640cbadf906215c89134758050) )
	ROM_LOAD( "8ing_ra-b.roar_rom-2.u0216", 0x800000, 0x400000, CRC(d46d46b7) SHA1(1c42cb5dcda4b26c08c4ecf95efeadaf3a1d1dd2) )

	ROM_REGION( 0x080000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "b.roar_u046.u046", 0x000001, 0x040000, CRC(d4bb261a) SHA1(9a295b1354ef15f37ea09bb209cf0cb98437c462) )
	ROM_LOAD16_BYTE( "b.roar_u042.u042", 0x000000, 0x040000, CRC(4d537f88) SHA1(1760367d70a81606e29885ea315185d2c2a9409b) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "8ing_ra-b.roar_rom-3.u0326", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et02", 0x000000, 0x000008, CRC(187ce61a) SHA1(521122b0f7b3f278dd2a2b1d73c252b952b5f55d) )
ROM_END

ROM_START( bldyror2 )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021", 0x0000000, 0x200000, CRC(fa7602e1) SHA1(6fb6af09656fbb86d2abda35804b2ed4a4cd7461) )
	ROM_LOAD( "flash1.024", 0x0200000, 0x200000, CRC(03465a69) SHA1(7c29aff2bf19c379873d3927c260892c78281882) )
	ROM_LOAD( "rom-1a.028", 0x0800000, 0x400000, CRC(0e711461) SHA1(1d0bd80e6885432ef0623babde28e5760b714bfa) )
	ROM_LOAD( "rom-1b.29",  0x0c00000, 0x400000, CRC(0cf153f9) SHA1(53bb9f8642079f56d8e925792b069362df666819) )
	ROM_LOAD( "rom-2a.026", 0x1000000, 0x400000, CRC(b71d955d) SHA1(49fce452c70ceafc8a149fa9ff073589b7261882) )
	ROM_LOAD( "rom-2b.210", 0x1400000, 0x400000, CRC(89959dde) SHA1(99d54b9876f38f5e625334bbd1439618cdf01d56) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "br2_u0412.412", 0x000001, 0x080000, CRC(e254dd8a) SHA1(5b8fcafcf2176e0b55efcf37799d7c0d97e01bdc) )
	ROM_LOAD16_BYTE( "br2_u049.049",  0x000000, 0x080000, CRC(10dc855b) SHA1(4e6e3a71911c8976ae07c2b6cac5a36f98193def) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "rom-3.336", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_world", 0x000000, 0x000800, CRC(01b42397) SHA1(853553a38e81e64a17c040173b29c7bfd6f79f31) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et03", 0x000000, 0x000008, CRC(779b0bfd) SHA1(76a188c78083bbb2740379d53143e1efaf637b85) )
ROM_END

ROM_START( bldyror2u )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021", 0x0000000, 0x200000, CRC(fa7602e1) SHA1(6fb6af09656fbb86d2abda35804b2ed4a4cd7461) )
	ROM_LOAD( "flash1.024", 0x0200000, 0x200000, CRC(03465a69) SHA1(7c29aff2bf19c379873d3927c260892c78281882) )
	ROM_LOAD( "rom-1a.028", 0x0800000, 0x400000, CRC(0e711461) SHA1(1d0bd80e6885432ef0623babde28e5760b714bfa) )
	ROM_LOAD( "rom-1b.29",  0x0c00000, 0x400000, CRC(0cf153f9) SHA1(53bb9f8642079f56d8e925792b069362df666819) )
	ROM_LOAD( "rom-2a.026", 0x1000000, 0x400000, CRC(b71d955d) SHA1(49fce452c70ceafc8a149fa9ff073589b7261882) )
	ROM_LOAD( "rom-2b.210", 0x1400000, 0x400000, CRC(89959dde) SHA1(99d54b9876f38f5e625334bbd1439618cdf01d56) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "br2_u0412.412", 0x000001, 0x080000, CRC(e254dd8a) SHA1(5b8fcafcf2176e0b55efcf37799d7c0d97e01bdc) )
	ROM_LOAD16_BYTE( "br2_u049.049",  0x000000, 0x080000, CRC(10dc855b) SHA1(4e6e3a71911c8976ae07c2b6cac5a36f98193def) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "rom-3.336", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_usa", 0x000000, 0x000800, CRC(b78d6fc3) SHA1(49d8b6f44c31d74f36cba981af7f4c7e23dd9007) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et03", 0x000000, 0x000008, CRC(779b0bfd) SHA1(76a188c78083bbb2740379d53143e1efaf637b85) )
ROM_END

ROM_START( bldyror2a )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021", 0x0000000, 0x200000, CRC(fa7602e1) SHA1(6fb6af09656fbb86d2abda35804b2ed4a4cd7461) )
	ROM_LOAD( "flash1.024", 0x0200000, 0x200000, CRC(03465a69) SHA1(7c29aff2bf19c379873d3927c260892c78281882) )
	ROM_LOAD( "rom-1a.028", 0x0800000, 0x400000, CRC(0e711461) SHA1(1d0bd80e6885432ef0623babde28e5760b714bfa) )
	ROM_LOAD( "rom-1b.29",  0x0c00000, 0x400000, CRC(0cf153f9) SHA1(53bb9f8642079f56d8e925792b069362df666819) )
	ROM_LOAD( "rom-2a.026", 0x1000000, 0x400000, CRC(b71d955d) SHA1(49fce452c70ceafc8a149fa9ff073589b7261882) )
	ROM_LOAD( "rom-2b.210", 0x1400000, 0x400000, CRC(89959dde) SHA1(99d54b9876f38f5e625334bbd1439618cdf01d56) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "br2_u0412.412", 0x000001, 0x080000, CRC(e254dd8a) SHA1(5b8fcafcf2176e0b55efcf37799d7c0d97e01bdc) )
	ROM_LOAD16_BYTE( "br2_u049.049",  0x000000, 0x080000, CRC(10dc855b) SHA1(4e6e3a71911c8976ae07c2b6cac5a36f98193def) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "rom-3.336", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_asia", 0x000000, 0x000800, CRC(da8c1a64) SHA1(14cbb751f498c96d9d8fce3eea3781ebc45f6291) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et03", 0x000000, 0x000008, CRC(779b0bfd) SHA1(76a188c78083bbb2740379d53143e1efaf637b85) )
ROM_END

ROM_START( bldyror2j )
	RAIZING_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021", 0x0000000, 0x200000, CRC(fa7602e1) SHA1(6fb6af09656fbb86d2abda35804b2ed4a4cd7461) )
	ROM_LOAD( "flash1.024", 0x0200000, 0x200000, CRC(03465a69) SHA1(7c29aff2bf19c379873d3927c260892c78281882) )
	ROM_LOAD( "rom-1a.028", 0x0800000, 0x400000, CRC(0e711461) SHA1(1d0bd80e6885432ef0623babde28e5760b714bfa) )
	ROM_LOAD( "rom-1b.29",  0x0c00000, 0x400000, CRC(0cf153f9) SHA1(53bb9f8642079f56d8e925792b069362df666819) )
	ROM_LOAD( "rom-2a.026", 0x1000000, 0x400000, CRC(b71d955d) SHA1(49fce452c70ceafc8a149fa9ff073589b7261882) )
	ROM_LOAD( "rom-2b.210", 0x1400000, 0x400000, CRC(89959dde) SHA1(99d54b9876f38f5e625334bbd1439618cdf01d56) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "br2_u0412.412", 0x000001, 0x080000, CRC(e254dd8a) SHA1(5b8fcafcf2176e0b55efcf37799d7c0d97e01bdc) )
	ROM_LOAD16_BYTE( "br2_u049.049",  0x000000, 0x080000, CRC(10dc855b) SHA1(4e6e3a71911c8976ae07c2b6cac5a36f98193def) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "rom-3.336", 0x000000, 0x400000, CRC(b74cc4d1) SHA1(eb5485582a12959ae06927a2f1d8a7e63e0f956f) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_japan", 0x000000, 0x000800, CRC(6cb55630) SHA1(b840bc0339485dd82f7c9aa669faf90ae371218f) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "et03", 0x000000, 0x000008, CRC(779b0bfd) SHA1(76a188c78083bbb2740379d53143e1efaf637b85) )
ROM_END

ROM_START( brvblade )
	TPS_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021",      0x0000000, 0x200000, CRC(97e12c63) SHA1(382970617a363f6c98ee741f26be6a75c9752bdb) )
	ROM_LOAD( "flash1.024",      0x0200000, 0x200000, CRC(d9d40a34) SHA1(c91dbc6f85404e9397fa79a4bac28e8c3c1a5228) )
	ROM_LOAD( "ra-bbl_rom1.028", 0x0800000, 0x400000, CRC(418535e0) SHA1(7c443e651704f2cd552565c35f4a93f2dc250558) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "spu1u412.bin", 0x0000001, 0x080000, CRC(6408b5b2) SHA1(ba60aa1074df87e98fa260211e9ec99cea25023f) )
	ROM_LOAD16_BYTE( "spu0u049.bin", 0x0000000, 0x080000, CRC(c9df8ed9) SHA1(00a58522189091c48d781b6703e4378e04343c33) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "ra-bbl_rom2.336", 0x000000, 0x400000, CRC(cd052c02) SHA1(d955a70a89b3b1a0b505a05c0887c399fe7a2c68) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_world", 0x000000, 0x000800, CRC(fe7f7d34) SHA1(18f6ae14e57afa668b3eef821b4cf0a7599a21ac) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg11", 0x000000, 0x000008, CRC(56d917de) SHA1(3c1cdbf552265dbff6051853d933ac34047bcdae) )
ROM_END

ROM_START( brvbladeu )
	TPS_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021",      0x0000000, 0x200000, CRC(97e12c63) SHA1(382970617a363f6c98ee741f26be6a75c9752bdb) )
	ROM_LOAD( "flash1.024",      0x0200000, 0x200000, CRC(d9d40a34) SHA1(c91dbc6f85404e9397fa79a4bac28e8c3c1a5228) )
	ROM_LOAD( "ra-bbl_rom1.028", 0x0800000, 0x400000, CRC(418535e0) SHA1(7c443e651704f2cd552565c35f4a93f2dc250558) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "spu1u412.bin", 0x0000001, 0x080000, CRC(6408b5b2) SHA1(ba60aa1074df87e98fa260211e9ec99cea25023f) )
	ROM_LOAD16_BYTE( "spu0u049.bin", 0x0000000, 0x080000, CRC(c9df8ed9) SHA1(00a58522189091c48d781b6703e4378e04343c33) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "ra-bbl_rom2.336", 0x000000, 0x400000, CRC(cd052c02) SHA1(d955a70a89b3b1a0b505a05c0887c399fe7a2c68) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_usa", 0x000000, 0x000800, CRC(0a2c042f) SHA1(147651d2e55873a82295214b3b0bd6c46cdad239) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg11", 0x000000, 0x000008, CRC(56d917de) SHA1(3c1cdbf552265dbff6051853d933ac34047bcdae) )
ROM_END

ROM_START( brvbladea )
	TPS_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021",      0x0000000, 0x200000, CRC(97e12c63) SHA1(382970617a363f6c98ee741f26be6a75c9752bdb) )
	ROM_LOAD( "flash1.024",      0x0200000, 0x200000, CRC(d9d40a34) SHA1(c91dbc6f85404e9397fa79a4bac28e8c3c1a5228) )
	ROM_LOAD( "ra-bbl_rom1.028", 0x0800000, 0x400000, CRC(418535e0) SHA1(7c443e651704f2cd552565c35f4a93f2dc250558) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "spu1u412.bin", 0x0000001, 0x080000, CRC(6408b5b2) SHA1(ba60aa1074df87e98fa260211e9ec99cea25023f) )
	ROM_LOAD16_BYTE( "spu0u049.bin", 0x0000000, 0x080000, CRC(c9df8ed9) SHA1(00a58522189091c48d781b6703e4378e04343c33) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "ra-bbl_rom2.336", 0x000000, 0x400000, CRC(cd052c02) SHA1(d955a70a89b3b1a0b505a05c0887c399fe7a2c68) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_asia", 0x000000, 0x000800, CRC(adb1aa3d) SHA1(118a17b8a15108666e4f9f2d1798031b3b893536) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg11", 0x000000, 0x000008, CRC(56d917de) SHA1(3c1cdbf552265dbff6051853d933ac34047bcdae) )
ROM_END

ROM_START( brvbladej )
	TPS_BIOS

	ROM_REGION32_LE( 0x1800000, REGION_USER1, 0 ) /* game rom, 8MB banks */
	ROM_LOAD( "flash0.021",      0x0000000, 0x200000, CRC(97e12c63) SHA1(382970617a363f6c98ee741f26be6a75c9752bdb) )
	ROM_LOAD( "flash1.024",      0x0200000, 0x200000, CRC(d9d40a34) SHA1(c91dbc6f85404e9397fa79a4bac28e8c3c1a5228) )
	ROM_LOAD( "ra-bbl_rom1.028", 0x0800000, 0x400000, CRC(418535e0) SHA1(7c443e651704f2cd552565c35f4a93f2dc250558) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 68000 */
	ROM_LOAD16_BYTE( "spu1u412.bin", 0x0000001, 0x080000, CRC(6408b5b2) SHA1(ba60aa1074df87e98fa260211e9ec99cea25023f) )
	ROM_LOAD16_BYTE( "spu0u049.bin", 0x0000000, 0x080000, CRC(c9df8ed9) SHA1(00a58522189091c48d781b6703e4378e04343c33) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* ymf271 */
	ROM_LOAD( "ra-bbl_rom2.336", 0x000000, 0x400000, CRC(cd052c02) SHA1(d955a70a89b3b1a0b505a05c0887c399fe7a2c68) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* factory settings */
	ROM_LOAD( "at28c16_japan", 0x000000, 0x000800, CRC(59e2d326) SHA1(53f48b6ad7243aa92b54863515f104a7a54b2810) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "mg11", 0x000000, 0x000008, CRC(56d917de) SHA1(3c1cdbf552265dbff6051853d933ac34047bcdae) )
ROM_END

ROM_START( beastrzb )
	TAITOFX1_BIOS

	ROM_REGION32_LE( 0x0900000, REGION_USER1, 0 )
	ROM_LOAD( "27c160.1",     0x0000000, 0x200000, CRC(820855e2) SHA1(18bdd4d0b4a92ae4fde457e1f37c813be6eece71) )
	ROM_LOAD( "27c160.2",     0x0200000, 0x200000, CRC(1712af34) SHA1(3a78997a2ad0fec1b09828b47150a4be611cd9ad) )
	ROM_LOAD( "4",            0x0400000, 0x200000, CRC(bff21f44) SHA1(2dffc518c069f0692a3b75e10091658d9c10ecb5) )
	ROM_LOAD( "27c160.5",     0x0600000, 0x200000, CRC(10fe6f4d) SHA1(9faee2faa6d741e1caf25edd093644be5723aa5c) )
	ROM_LOAD( "27c800.3",     0x0800000, 0x100000, CRC(7192eb4e) SHA1(bb276a38261099d91080d8613dc7500322f6fcab) )

	ROM_REGION( 0x180000, REGION_CPU2, 0 )
	ROM_LOAD( "27c4096.1",    0x0000000, 0x080000, CRC(217734a1) SHA1(de4f519215123c09b3b5f27509b4d74604b5e03d) )
	ROM_LOAD( "27c4096.2",    0x0080000, 0x080000, CRC(d1f2a9b2) SHA1(d1475a453ce4e3b9f2ff59abedf0f57ba3c408fe) )
	ROM_LOAD( "27c240.3",     0x0100000, 0x080000, CRC(509cdc8b) SHA1(8b92b79be09de56e7d40c2d02fcbeca92bb60226) )
ROM_END


/* Atari / Time Warner (COH-1000W), MAME names */

#define TIME_WARNER_BIOS \
	ROM_REGION32_LE( 0x800000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram, 8MB */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "msm27c402zb.ic353", 0x000000, 0x080000, CRC(45e8a4b4) SHA1(815488d8563c85f97fbc3384ff21f08e4c88b7b7) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "tw01.ic652", 0x000000, 0x000008, CRC(1cac067e) SHA1(8e87941bfb374172628c8a37647e3a66b7166aa8) )

ROM_START( coh1000w )
	TIME_WARNER_BIOS
ROM_END

ROM_START( primrag2 )
	TIME_WARNER_BIOS

	ROM_REGION32_LE( 0x200000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "pr2_036.u16",  0x000001, 0x080000, CRC(3ee39e4f) SHA1(dbd859b54fb9be33effc14eb847dcd829024eea3) )
	ROM_LOAD16_BYTE( "pr2_036.u14",  0x000000, 0x080000, CRC(c86450cd) SHA1(19c3c50d839a9efb6ffa9ada8a072f56697c1abb) )
	ROM_LOAD16_BYTE( "pr2_036.u17",  0x100001, 0x080000, CRC(3681516c) SHA1(714f73ea4ac190c36a6eb2308616a4aecabc4e69) )
	ROM_LOAD16_BYTE( "pr2_036.u15",  0x100000, 0x080000, CRC(4b24bd54) SHA1(7f27cd524d10e5869aab6d4dc6a4217d049c475d) )

	ROM_REGION( 0x000800, REGION_USER6, 0 ) /* at28c16, factory settings */
	ROM_LOAD( "at28c16", 0x000000, 0x000800, CRC(62c21283) SHA1(6013d831fb97c025682603a1bb9d62bd93e1b893) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "tw02", 0x000000, 0x000008, CRC(583e8e1c) SHA1(0af99e3feb3fa684883a5daefba2c38a442f3884) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "primrag2", 0, SHA1(bc615068ddf4fd967f770ee01c02f285c052c4c5) )
ROM_END

#define ACCLAIM_BIOS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* ram */ \
	ROM_REGION32_LE( 0x080000, REGION_USER3, 0 ) /* bios */ \
	ROM_LOAD( "m534002c-15.ic353", 0x000000, 0x080000, CRC(8d8d0764) SHA1(7ee83d317190bb1cef2f8f01c81eaaae47150ebb) ) \
	ROM_REGION( 0x000008, REGION_USER4, 0 ) /* CAT702, main board */ \
	ROM_LOAD( "ac01.ic652", 0x000000, 0x000008, CRC(c425eaf5) SHA1(decf77ab50f7f58c641c3214a11647924f3a8998) )

ROM_START( coh1000a )
	ACCLAIM_BIOS
ROM_END

ROM_START( jdredd )
	ACCLAIM_BIOS

	ROM_REGION32_LE( 0x200000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "9e54_01-16-98_1566_u_36.u36",  0x000001, 0x020000, CRC(37addbf9) SHA1(a4061a1ba9e230f080f0bfea69bf77efe9264a92) )
	ROM_LOAD16_BYTE( "79d3_01-16-98_1565_u_35.u35",  0x000000, 0x020000, CRC(c1e17191) SHA1(82901439b1a51b9aadb4df4b9d944f26697a1460) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "ac02", 0x000000, 0x000008, CRC(1412d475) SHA1(c2f62232a261870f58353d09dc0d6ce2ad17a729) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "jdreddc", 0, SHA1(eee205f83e5f590f8baf36452c873d7063156bd0) )	/* 1576 Rev.C */
ROM_END

ROM_START( jdreddb )
	ACCLAIM_BIOS

	ROM_REGION32_LE( 0x200000, REGION_USER1, 0 ) /* game rom */
	ROM_LOAD16_BYTE( "9e54_11-21-97_1566_u_36.u36",  0x000001, 0x020000, CRC(37addbf9) SHA1(a4061a1ba9e230f080f0bfea69bf77efe9264a92) )
	ROM_LOAD16_BYTE( "79d3_11-21-97_1565_u_35.u35",  0x000000, 0x020000, CRC(c1e17191) SHA1(82901439b1a51b9aadb4df4b9d944f26697a1460) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "ac02", 0x000000, 0x000008, CRC(1412d475) SHA1(c2f62232a261870f58353d09dc0d6ce2ad17a729) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "jdreddb", 0, SHA1(20f696fa6e1fbf97793bac2a794631c5dd4fb39a) )	/* 1576 Rev.B */
ROM_END

ROM_START( nbajamex )
	ACCLAIM_BIOS

	ROM_REGION32_LE( 0x2000000, REGION_USER1, 0 ) /* game rom, 16 banks of 2MB */
	ROM_LOAD16_BYTE( "nba_jam_110i_odd_cs_cc48.u36",  0x0000001, 0x100000, CRC(533041e7) SHA1(691a11a031e61d892db74c4c29b6a999917c341b) )	/* labeled  NBA JAM 1.10I  ODD  CS = CC48 */
	ROM_LOAD16_BYTE( "nba_jam_110i_even_cs_9567.u35", 0x0000000, 0x100000, CRC(63d7f15d) SHA1(23525e875d0e61763301e95f70964bc9df3a2624) )	/* labeled  NBA JAM 1.10I  EVEN  CS = 9567 */
	ROM_LOAD16_BYTE( "nba0o.u28",   0x0200001, 0x200000, CRC(be13c5af) SHA1(eee5c9d985384ecfe4f00fae27d66fbefc15b28e) )
	ROM_LOAD16_BYTE( "nba0e.u41",   0x0200000, 0x200000, CRC(077f4355) SHA1(63c52bb82943b52bb0906d114acd5ea8643068b6) )
	ROM_LOAD16_BYTE( "nba1o.u29",   0x0600001, 0x200000, CRC(3650e85b) SHA1(a36bfa235c8e3bb516e178f54d3c5e3955c7e918) )
	ROM_LOAD16_BYTE( "nba1e.u42",   0x0600000, 0x200000, CRC(f1212cf9) SHA1(b2f80af3ec4d559056e86f695d89d1d32b500f50) )
	ROM_LOAD16_BYTE( "nba2o.u30",   0x0a00001, 0x200000, CRC(ccbb6125) SHA1(998eda99182b984f88f5fc58095cb35bf232a26b) )
	ROM_LOAD16_BYTE( "nba2e.u43",   0x0a00000, 0x200000, CRC(c20ab628) SHA1(7ffe5005e1913da56770452ae2f907a4a270ab24) )
	ROM_LOAD16_BYTE( "nba3o.u31",   0x0e00001, 0x200000, CRC(d5238edf) SHA1(d1eb30ec65dd6cfa8cbb2b36af3a83820d1de99a) )
	ROM_LOAD16_BYTE( "nba3e.u44",   0x0e00000, 0x200000, CRC(07ba00a3) SHA1(c14bffd35ee715b07d6065b454b0443438ab6536) )
	ROM_LOAD16_BYTE( "nba4o.u3",    0x1200001, 0x200000, CRC(1cf16a34) SHA1(a7e984a2db846854f1c4a9a2fdefd0d17ce3108c) )
	ROM_LOAD16_BYTE( "nba4e.u17",   0x1200000, 0x200000, CRC(b5977765) SHA1(08acdfe413a5a9182ca117f44b7acac9dac9ecea) )
	ROM_LOAD16_BYTE( "nba5o.u4",    0x1600001, 0x200000, CRC(5272754b) SHA1(c35ba5377eb812991e4bf0d954a34af90b986341) )
	ROM_LOAD16_BYTE( "nba5e.u18",   0x1600000, 0x200000, CRC(0eb917da) SHA1(d6c8991ba7cd492668757658ee64078d0e82b596) )
	ROM_LOAD16_BYTE( "nba6o.u5",    0x1a00001, 0x200000, CRC(b1dfb42e) SHA1(fb9627e228bf2a744842eb44afbca4a6232cadb2) )
	ROM_LOAD16_BYTE( "nba6e.u19",   0x1a00000, 0x200000, CRC(6f17d8c1) SHA1(22cf263efb64cf62030e02b641c485debe75944d) )

	ROM_REGION( ADSP2100_SIZE, REGION_CPU2, ROMREGION_ERASE00 )	/* ADSP-2181 internal RAM */

	ROM_REGION( 0x800000, REGION_SOUND1, 0 )	/* RAX: two banks of 4MB */
	ROM_LOAD( "360snda1.u52", 0x000000, 0x080000, CRC(36d8a628) SHA1(944a01c9128f5e90c7dba3557a3ecb2c5ca90831) )
	ROM_LOAD( "sound0.u48",   0x400000, 0x200000, CRC(38873b67) SHA1(b2f8d32270ae604c099a1b9b71d2e06468c7d4a9) )
	ROM_LOAD( "sound1.u49",   0x600000, 0x200000, CRC(57014589) SHA1(d360ff1c52424bd91a5a8d1a2a9c10bf7abb0602) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "ac02", 0x000000, 0x000008, CRC(1412d475) SHA1(c2f62232a261870f58353d09dc0d6ce2ad17a729) )
ROM_END

ROM_START( nbajamexa )
	ACCLAIM_BIOS

	ROM_REGION32_LE( 0x2000000, REGION_USER1, 0 ) /* game rom, 16 banks of 2MB */
	ROM_LOAD16_BYTE( "360-mp-a1_odd_cs_e0fe.u36",  0x0000001, 0x100000, CRC(c433e827) SHA1(1d2a5a6990a1b1864e63ce3ba7306d48ebbd4775) )	/* labeled  360-MP-A1  ODD  CS = E0FE */
	ROM_LOAD16_BYTE( "360-mp-a1_even_cs_295a.u35", 0x0000000, 0x100000, CRC(d8f5b2f7) SHA1(e38609d314721b8b612e047406e2888395917b0d) )	/* labeled  360-MP-A1  EVEN  CS = 295A */
	ROM_LOAD16_BYTE( "nba0o.u28",   0x0200001, 0x200000, CRC(be13c5af) SHA1(eee5c9d985384ecfe4f00fae27d66fbefc15b28e) )
	ROM_LOAD16_BYTE( "nba0e.u41",   0x0200000, 0x200000, CRC(077f4355) SHA1(63c52bb82943b52bb0906d114acd5ea8643068b6) )
	ROM_LOAD16_BYTE( "nba1o.u29",   0x0600001, 0x200000, CRC(3650e85b) SHA1(a36bfa235c8e3bb516e178f54d3c5e3955c7e918) )
	ROM_LOAD16_BYTE( "nba1e.u42",   0x0600000, 0x200000, CRC(f1212cf9) SHA1(b2f80af3ec4d559056e86f695d89d1d32b500f50) )
	ROM_LOAD16_BYTE( "nba2o.u30",   0x0a00001, 0x200000, CRC(ccbb6125) SHA1(998eda99182b984f88f5fc58095cb35bf232a26b) )
	ROM_LOAD16_BYTE( "nba2e.u43",   0x0a00000, 0x200000, CRC(c20ab628) SHA1(7ffe5005e1913da56770452ae2f907a4a270ab24) )
	ROM_LOAD16_BYTE( "nba3o.u31",   0x0e00001, 0x200000, CRC(d5238edf) SHA1(d1eb30ec65dd6cfa8cbb2b36af3a83820d1de99a) )
	ROM_LOAD16_BYTE( "nba3e.u44",   0x0e00000, 0x200000, CRC(07ba00a3) SHA1(c14bffd35ee715b07d6065b454b0443438ab6536) )
	ROM_LOAD16_BYTE( "nba4o.u3",    0x1200001, 0x200000, CRC(1cf16a34) SHA1(a7e984a2db846854f1c4a9a2fdefd0d17ce3108c) )
	ROM_LOAD16_BYTE( "nba4e.u17",   0x1200000, 0x200000, CRC(b5977765) SHA1(08acdfe413a5a9182ca117f44b7acac9dac9ecea) )
	ROM_LOAD16_BYTE( "nba5o.u4",    0x1600001, 0x200000, CRC(5272754b) SHA1(c35ba5377eb812991e4bf0d954a34af90b986341) )
	ROM_LOAD16_BYTE( "nba5e.u18",   0x1600000, 0x200000, CRC(0eb917da) SHA1(d6c8991ba7cd492668757658ee64078d0e82b596) )
	ROM_LOAD16_BYTE( "nba6o.u5",    0x1a00001, 0x200000, CRC(b1dfb42e) SHA1(fb9627e228bf2a744842eb44afbca4a6232cadb2) )
	ROM_LOAD16_BYTE( "nba6e.u19",   0x1a00000, 0x200000, CRC(6f17d8c1) SHA1(22cf263efb64cf62030e02b641c485debe75944d) )

	ROM_REGION( ADSP2100_SIZE, REGION_CPU2, ROMREGION_ERASE00 )	/* ADSP-2181 internal RAM */

	ROM_REGION( 0x800000, REGION_SOUND1, 0 )	/* RAX: two banks of 4MB */
	ROM_LOAD( "360snda1.u52", 0x000000, 0x080000, CRC(36d8a628) SHA1(944a01c9128f5e90c7dba3557a3ecb2c5ca90831) )
	ROM_LOAD( "sound0.u48",   0x400000, 0x200000, CRC(38873b67) SHA1(b2f8d32270ae604c099a1b9b71d2e06468c7d4a9) )
	ROM_LOAD( "sound1.u49",   0x600000, 0x200000, CRC(57014589) SHA1(d360ff1c52424bd91a5a8d1a2a9c10bf7abb0602) )

	ROM_REGION( 0x000008, REGION_USER5, 0 ) /* CAT702, rom board */
	ROM_LOAD( "ac02", 0x000000, 0x000008, CRC(1412d475) SHA1(c2f62232a261870f58353d09dc0d6ce2ad17a729) )
ROM_END

/* Capcom ZN-1 */

GAMEX( 1995, coh1000c, 0,        coh1000c, capcom6b, coh1000c, ROT0, "Capcom", "Capcom ZN-1", NOT_A_DRIVER )
GAMEX( 1995, ts2,      coh1000c, coh1000c, capcom6b, coh1000c, ROT0, "Capcom / Takara", "Battle Arena Toshinden 2 (Europe 951124)", 0 )
GAMEX( 1995, ts2u,     ts2,      coh1000c, capcom6b, coh1000c, ROT0, "Capcom / Takara", "Battle Arena Toshinden 2 (USA 951124)", 0 )
GAMEX( 1995, ts2ua,    ts2,      coh1000c, capcom6b, coh1000c, ROT0, "Capcom / Takara", "Battle Arena Toshinden 2 (USA 951124, 32Mb mask ROMs)", 0 )
GAMEX( 1995, ts2j,     ts2,      coh1000c, capcom6b, coh1000c, ROT0, "Capcom / Takara", "Battle Arena Toshinden 2 (Japan 951124)", 0 )
GAMEX( 1995, ts2ja,    ts2,      coh1000c, capcom6b, coh1000c, ROT0, "Capcom / Takara", "Battle Arena Toshinden 2 (Japan 951124, 32Mb mask ROMs)", 0 )
GAMEX( 1996, starglad, coh1000c, coh1000c, capcom4b, coh1000c, ROT0, "Capcom", "Star Gladiator Episode I: Final Crusade (USA 960627)", 0 )
GAMEX( 1996, stargladj, starglad, coh1000c, capcom4b, coh1000c, ROT0, "Capcom", "Star Gladiator Episode I: Final Crusade (Japan 960627)", 0 )
GAMEX( 1996, glpracr,  coh1000c, coh1000c, glpracr, glpracr, ROT0, "Tecmo", "Gallop Racer (English Ver 10.17.K)", 0 )
GAMEX( 1996, sfex,     coh1000c, coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX (Europe 961219)", 0 )
GAMEX( 1996, sfexu,    sfex,     coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX (USA 961219)", 0 )
GAMEX( 1996, sfexa,    sfex,     coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX (Asia 961219)", 0 )
GAMEX( 1996, sfexj,    sfex,     coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX (Japan 961130)", 0 )
GAMEX( 1997, sfexp,    coh1000c, coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX Plus (USA 970407)", 0 )
GAMEX( 1997, sfexpu1,  sfexp,    coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX Plus (USA 970311)", 0 )
GAMEX( 1997, sfexpj,   sfexp,    coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX Plus (Japan 970407)", 0 )
GAMEX( 1997, sfexpj1,  sfexp,    coh1002c, capcom6b, coh1002c, ROT0, "Capcom / Arika", "Street Fighter EX Plus (Japan 970311)", 0 )

/* Capcom ZN-2 */

GAMEX( 1996, glpracrj, glpracr,  coh1000c, glpracr, glpracr, ROT0, "Tecmo", "Gallop Racer (Japanese Ver 9.01.12)", 0 )
GAMEX( 1997, coh3002c, 0,        coh3002c, capcom6b, coh3002c, ROT0, "Capcom", "Capcom ZN-2", NOT_A_DRIVER )
GAMEX( 1997, rvschool, coh3002c, coh3002c, capcom4b, coh3002c, ROT0, "Capcom", "Rival Schools: United By Fate (Europe 971117)", 0 )
GAMEX( 1997, rvschoolu, rvschool, coh3002c, capcom4b, coh3002c, ROT0, "Capcom", "Rival Schools: United By Fate (USA 971117)", 0 )
GAMEX( 1997, rvschoola, rvschool, coh3002c, capcom4b, coh3002c, ROT0, "Capcom", "Rival Schools: United By Fate (Asia 971117)", 0 )
GAMEX( 1997, jgakuen,  rvschool, coh3002c, capcom4b, coh3002c, ROT0, "Capcom", "Shiritsu Justice Gakuen: Legion of Heroes (Japan 971216)", 0 )
GAMEX( 1997, jgakuen1, rvschool, coh3002c, capcom4b, coh3002c, ROT0, "Capcom", "Shiritsu Justice Gakuen: Legion of Heroes (Japan 971117)", 0 )
GAMEX( 1998, sfex2,    coh3002c, coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 (Europe 980312)", 0 )
GAMEX( 1998, sfex2u,   sfex2,    coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 (USA 980526)", 0 )
GAMEX( 1998, sfex2u1,  sfex2,    coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 (USA 980312)", 0 )
GAMEX( 1998, sfex2a,   sfex2,    coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 (Asia 980312)", 0 )
GAMEX( 1998, sfex2h,   sfex2,    coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 (Hispanic 980312)", 0 )
GAMEX( 1998, sfex2j,   sfex2,    coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 (Japan 980312)", 0 )
GAMEX( 1998, plsmaswd, coh3002c, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Plasma Sword: Nightmare of Bilstein (USA 980316)", 0 )
GAMEX( 1998, plsmaswda, plsmaswd, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Plasma Sword: Nightmare of Bilstein (Asia 980316)", 0 )
GAMEX( 1998, stargld2, plsmaswd, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Star Gladiator 2: Nightmare of Bilstein (Japan 980316)", 0 )
GAMEX( 1998, tgmj,     coh3002c, coh3002c, tgmj,    coh3002c, ROT0, "Arika / Capcom", "Tetris: The Grand Master (Japan 980710)", 0 )
GAMEX( 1998, techromn, coh3002c, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Tech Romancer (Europe 980914)", 0 )
GAMEX( 1998, techromnu, techromn, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Tech Romancer (USA 980914)", 0 )
GAMEX( 1998, techromna, techromn, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Tech Romancer (Asia 980914)", 0 )
GAMEX( 1998, kikaioh,  techromn, coh3002c, plsmaswd, coh3002c, ROT0, "Capcom", "Choukou Senki Kikaioh (Japan 980914)", 0 )
GAMEX( 1999, sfex2p,   coh3002c, coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 Plus (Europe 990611)", 0 )
GAMEX( 1999, sfex2pu,  sfex2p,   coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 Plus (USA 990611)", 0 )
GAMEX( 1999, sfex2pa,  sfex2p,   coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 Plus (Asia 990611)", 0 )
GAMEX( 1999, sfex2ph,  sfex2p,   coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 Plus (Hispanic 990611)", 0 )
GAMEX( 1999, sfex2pj,  sfex2p,   coh3002c, capcom6b, coh3002c, ROT0, "Capcom / Arika", "Street Fighter EX2 Plus (Japan 990611)", 0 )
GAMEX( 1999, strider2, coh3002c, coh3002c, strider2, coh3002c, ROT0, "Capcom", "Strider 2 (Europe 991213)", 0 )
GAMEX( 1999, strider2u, strider2, coh3002c, strider2, coh3002c, ROT0, "Capcom", "Strider 2 (USA 991213)", 0 )
GAMEX( 1999, strider2a, strider2, coh3002c, strider2, coh3002c, ROT0, "Capcom", "Strider 2 (Asia 991213)", 0 )
GAMEX( 1999, shiryu2,  strider2, coh3002c, strider2, coh3002c, ROT0, "Capcom", "Strider Hiryu 2 (Japan 991213)", 0 )

/* Atari */

GAMEX( 1995, coh1000w,  0,        coh1000w, tps2p,    coh1000w, ROT0,   "Atari", "Time Warner ZN-1", NOT_A_DRIVER )
GAMEX( 1997, primrag2,  coh1000w, coh1000w, primrag2, coh1000w, ROT0,   "Atari", "Primal Rage II (Test Version 0.36a)", 0 )

/* Acclaim */

/* A dummy driver, so that the bios can be debugged, and to serve as */
/* parent for the coh1000a.bin file, so that we do not have to include */
/* it in every zip file */



GAMEX( 1995, coh1000a,  0,        jdredd,   jdredd,   jdredd,   ROT0,   "Acclaim", "Acclaim ZN-1", NOT_A_DRIVER )
GAMEX( 1996, jdredd,    coh1000a, jdredd,   jdredd,   jdredd,   ROT0,   "Acclaim", "Judge Dredd (Rev C Dec. 17 1997)", 0 )
GAMEX( 1996, jdreddb,   jdredd,   jdredd,   jdredd,   jdredd,   ROT0,   "Acclaim", "Judge Dredd (Rev B Nov. 26 1997)", 0 )
GAMEX( 1996, nbajamex,  coh1000a, nbajamex, nbajamex, nbajamex, ROT0,   "Acclaim", "NBA Jam Extreme (ver. 1.10I)", 0 )
GAMEX( 1996, nbajamexa, nbajamex, nbajamex, nbajamex, nbajamex, ROT0,   "Acclaim", "NBA Jam Extreme (ver. 1.04)", 0 )

/* Tecmo TPS */

/* the BIOS and the main board CAT702 (MG01) are in coh1002m.zip */
GAMEX( 1997, coh1002m,  0,        coh1002m, tps,      coh1002m, ROT0, "Tecmo", "Tecmo TPS System", NOT_A_DRIVER )

GAMEX( 1997, glpracr2,  coh1002m, coh1002m, glpracr2, coh1002m, ROT0, "Tecmo", "Gallop Racer 2 (Export)", 0 )
GAMEX( 1997, glpracr2j, glpracr2, coh1002m, glpracr2, coh1002m, ROT0, "Tecmo", "Gallop Racer 2 (Japan)", 0 )
GAMEX( 1997, glpracr2l, glpracr2, coh1002m, glpracr2, coh1002m, ROT0, "Tecmo", "Gallop Racer 2 Link HW (Japan)", 0 )
GAMEX( 1998, doapp,     coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Tecmo", "Dead Or Alive ++ (Japan/USA/Export)", 0 )
GAMEX( 1998, doappk,    doapp,    coh1002m, tps2p,    coh1002m, ROT0, "Tecmo", "Dead Or Alive ++ (Korea)", 0 )
GAMEX( 1998, cbaj,      coh1002m, cbaj,     tps1p,    coh1002m, ROT0, "UEP Systems", "Cool Boarders Arcade Jam (Export)", 0 )
GAMEX( 1998, cbajbl,    cbaj,     cbaj,     tps1p,    coh1002m, ROT0, "bootleg", "Cool Boarders Arcade Jam (Export bootleg)", 0 )
GAMEX( 1998, shngmtkb,  coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Sunsoft / Activision", "Shanghai Matekibuyuu (Japan)", 0 )
GAMEX( 1999, tondemo,   coh1002m, coh1002m, tps1p,    coh1002m, ROT0, "Tecmo", "Tondemo Crisis (Japan)", 0 )
GAMEX( 1999, glpracr3,  coh1002m, coh1002m, glpracr2, coh1002m, ROT0, "Tecmo", "Gallop Racer 3 (Export)", 0 )
GAMEX( 1999, glpracr3j, glpracr3, coh1002m, glpracr2, coh1002m, ROT0, "Tecmo", "Gallop Racer 3 (Japan)", 0 )
GAMEX( 1999, flamegun,  coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Gaps Inc.", "Flame Gunner (Export)", 0 )
GAMEX( 1999, flamegunj, flamegun, coh1002m, tps2p,    coh1002m, ROT0, "Gaps Inc.", "Flame Gunner (Japan)", 0 )
GAMEX( 1999, lpadv,     coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Amuse World", "Logic Pro Adventure (Japan)", 0 )
GAMEX( 2000, tblkkuzu,  coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Tamsoft / D3 Publisher", "The Block Kuzushi (Japan)", 0 )
GAMEX( 2000, 1on1gov,   coh1002m, coh1002m, tps,      coh1002m, ROT0, "Tecmo", "1 on 1 Government (Japan)", 0 )
GAMEX( 2000, twcupmil,  coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Tecmo", "Tecmo World Cup Millennium (Japan)", 0 )
GAMEX( 2001, mfjump,    coh1002m, coh1002m, tps2p,    coh1002m, ROT0, "Tecmo", "Monster Farm Jump (Japan)", 0 )

/* Video System */

GAMEX( 1996, coh1002v,  0,        coh1002v, zn,       coh1002v, ROT0,   "Video System Co.", "Video System ZN-1", NOT_A_DRIVER )
GAMEX( 1996, aerofgts,  coh1002v, coh1002v, aerofgts, coh1002v, ROT270, "McO'River, Inc.", "Aero Fighters Special (USA)", 0 )
GAMEX( 1996, aerofgtst, aerofgts, coh1002v, aerofgts, coh1002v, ROT270, "Video System Co.", "Aero Fighters Special (Taiwan)", 0 )
GAMEX( 1996, sncwgltd,  aerofgts, coh1002v, aerofgts, coh1002v, ROT270, "Video System Co.", "Sonic Wings Limited (Japan)", 0 )

/* Atlus */

GAMEX( 1995, coh1001l,  0,        coh1001l, tps2p,    coh1001l, ROT0,   "Atlus", "Atlus ZN-1", NOT_A_DRIVER )
GAMEX( 1996, hvnsgate,  coh1001l, coh1001l, tps2p,    coh1001l, ROT0,   "Racdym / Atlus", "Heaven's Gate", 0 )

/* Taito FX-1A */

GAMEX( 1995, coh1000t,  0,        coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Taito FX-1", NOT_A_DRIVER )

GAMEX( 1995, sfchamp,   coh1000t, coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Super Football Champ (Ver 2.5O)", 0 )
GAMEX( 1995, sfchampo,  sfchamp,  coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Super Football Champ (Ver 2.4O)", 0 )
GAMEX( 1995, sfchampu,  sfchamp,  coh1000ta, znt2p, coh1000ta, ROT0, "Taito America", "Super Football Champ (Ver 2.4A)", 0 )
GAMEX( 1995, sfchampj,  sfchamp,  coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Super Football Champ (Ver 2.4J)", 0 )
GAMEX( 1995, psyforce,  coh1000t, coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Psychic Force (Ver 2.4O)", 0 )
GAMEX( 1995, psyforcej, psyforce, coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Psychic Force (Ver 2.4J)", 0 )
GAMEX( 1995, psyforcex, psyforce, coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Psychic Force EX (Ver 2.0J)", 0 )
GAMEX( 1997, mgcldtex,  coh1000t, coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Magical Date EX / Magical Date - Sotsugyou Kokuhaku Daisakusen (Ver 2.01J)", 0 )
GAMEX( 1996, mgcldate,  mgcldtex, coh1000ta, znt2p, coh1000ta, ROT0, "Taito", "Magical Date / Magical Date - Dokidoki Kokuhaku Daisakusen (Ver 2.02J)", 0 )

/* Taito FX-1B */

GAMEX( 1996, raystorm,  coh1000t, coh1000tb, raystorm, coh1000tb, ROT0, "Taito", "RayStorm (Ver 2.06A)", 0 )
GAMEX( 1996, raystormo, raystorm, coh1000tb, raystorm, coh1000tb, ROT0, "Taito", "RayStorm (Ver 2.05O)", 0 )
GAMEX( 1996, raystormu, raystorm, coh1000tb, raystorm, coh1000tb, ROT0, "Taito America", "RayStorm (Ver 2.05A)", 0 )
GAMEX( 1996, raystormj, raystorm, coh1000tb, raystorm, coh1000tb, ROT0, "Taito", "RayStorm (Ver 2.05J)", 0 )
GAMEX( 1997, ftimpcta,  coh1000t, coh1000tb, znt2p,    coh1000tb, ROT0, "Taito", "Fighters' Impact A (Ver 2.00J)", 0 )
GAMEX( 1996, ftimpact,  ftimpcta, coh1000tb, znt2p,    coh1000tb, ROT0, "Taito", "Fighters' Impact (Ver 2.02O)", 0 )
GAMEX( 1996, ftimpactu, ftimpcta, coh1000tb, znt2p,    coh1000tb, ROT0, "Taito America", "Fighters' Impact (Ver 2.02A)", 0 )
GAMEX( 1996, ftimpactj, ftimpcta, coh1000tb, znt2p,    coh1000tb, ROT0, "Taito", "Fighters' Impact (Ver 2.02J)", 0 )
GAMEX( 1997, gdarius2,  coh1000t, coh1002tb, znt2p,    coh1002tb, ROT0, "Taito", "G-Darius Ver.2 (Ver 2.03J)", 0 )
GAMEX( 1997, gdarius,   gdarius2, coh1002tb, znt2p,    coh1002tb, ROT0, "Taito", "G-Darius (Ver 2.02O)", 0 )
GAMEX( 1997, gdariusu,  gdarius2, coh1002tb, znt2p,    coh1002tb, ROT0, "Taito America", "G-Darius (Ver 2.02A)", 0 )
GAMEX( 1997, gdariusj,  gdarius2, coh1002tb, znt2p,    coh1002tb, ROT0, "Taito", "G-Darius (Ver 2.01J)", 0 )

/* Taito FX1, old BIOS layout: parent of the Beastorizer bootleg only */

GAMEX( 1995, taitofx1, 0,        zn,       zn, zn,   ROT0, "Sony/Taito", "Taito FX1", NOT_A_DRIVER )

/* Eighting / Raizing (COH-1002E) */

GAMEX( 1996, coh1002e,  0,        coh1002e, znt2p,    coh1002e, ROT0,   "Eighting / Raizing", "Eighting / Raizing ZN-1", NOT_A_DRIVER )
GAMEX( 1997, beastrzr,  coh1002e, coh1002e, znt2p,    coh1002e, ROT0,   "Eighting / Raizing", "Beastorizer (USA, Sep 22 1997)", 0 )
GAMEX( 1997, beastrzra, beastrzr, coh1002e, znt2p,    coh1002e, ROT0,   "Eighting / Raizing", "Beastorizer (USA, Jun 21 1997)", 0 )
GAMEX( 1997, bldyroar,  beastrzr, coh1002e, znt2p,    coh1002e, ROT0,   "Eighting / Raizing", "Bloody Roar (Japan, Jun 21 1997)", 0 )
GAMEX( 1997, beastrzb,  taitofx1, zn,       zn,       zn,       ROT0,   "Eighting/Raizing", "Beastorizer (bootleg)", GAME_UNEMULATED_PROTECTION | GAME_NO_SOUND | GAME_NOT_WORKING )
GAMEX( 1998, bldyror2,  coh1002e, coh1002e, bldyror2, coh1002e, ROT0,   "Eighting / Raizing", "Bloody Roar 2 (World)", 0 )
GAMEX( 1998, bldyror2u, bldyror2, coh1002e, bldyror2, coh1002e, ROT0,   "Eighting / Raizing", "Bloody Roar 2 (USA)", 0 )
GAMEX( 1998, bldyror2a, bldyror2, coh1002e, bldyror2, coh1002e, ROT0,   "Eighting / Raizing", "Bloody Roar 2 (Asia)", 0 )
GAMEX( 1998, bldyror2j, bldyror2, coh1002e, bldyror2, coh1002e, ROT0,   "Eighting / Raizing", "Bloody Roar 2 (Japan)", 0 )
GAMEX( 2000, brvblade,  coh1002m, coh1002e, znt2p,    coh1002e, ROT270, "Eighting / Raizing", "Brave Blade (World)", 0 )
GAMEX( 2000, brvbladeu, brvblade, coh1002e, znt2p,    coh1002e, ROT270, "Eighting / Raizing", "Brave Blade (USA)", 0 )
GAMEX( 2000, brvbladea, brvblade, coh1002e, znt2p,    coh1002e, ROT270, "Eighting / Raizing", "Brave Blade (Asia)", 0 )
GAMEX( 2000, brvbladej, brvblade, coh1002e, znt2p,    coh1002e, ROT270, "Eighting / Raizing", "Brave Blade (Japan)", 0 )
