/***************************************************************************

  Namco System 12 - Arcade PSX Hardware
  =====================================
  Driver by smf
  H8/3002 and Golgo13 support by R. Belmont based on work by The_Author and DynaChicken
  H8/3002 + C352 sound backported to mame2003-plus (coh700s, Tekken 3 only for now)

  Issues:
    on coh700 the sound cpu is only simulated enough for inputs to work, credit handling is not perfect.
    coh700s runs the real H8/3002 sound/I-O program and the C352 (sound, inputs, coins, RTC).
    not all games work due to either banking, dma or protection issues.
    graphics are glitchy in some games.

Known Dumps
-----------

Game       Description                             CPU board           Mother board             Daughter board          Keycus

tekken3    Tekken 3 (TET2/VER.E1)                  COH 700             ?                        SYSTEM12 M8F2           ?
soulclbr   Soul Calibur (SOC12/VER.A2)             COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M5F2           KC020
ehrgeiz    Ehrgeiz (EG2/VER.A)                     COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M4F6           KC021
mdhorse    Derby Quiz My Dream Horse (MDH1/VER.A2) COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M10X64         KC035
fgtlayer   Fighting Layer (FTL3/VER.A)             COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M5F4           KC037
pacapp     Paca Paca Passion (PPP1/VER.A2)         COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M8F2           KC038
sws99      Super World Stadium '99 (SS91/VER.A3)   COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M5F4           KC043
tektagt    Tekken Tag Tournament (TEG2/VER.C1)     COH 700             SYSTEM12 MOTHER(B)       SYSTEM12 M8F4           KC044
mrdrillr   Mr. Driller (DRI3/VER.A2)               COH 700             SYSTEM12 MOTHER(C)       SYSTEM12 M8F2           KC048
aquarush   Aqua Rush (AQ1/VER.A1)                  COH 700             SYSTEM12 MOTHER(C)       SYSTEM12 M5F2           KC053
golgo13    Golgo 13 (GLG1/VER.A)                   COH 700             ?                        SYSTEM12 M8F6           ?

ROM sets and file names follow current MAME (parents and the clones listed in the GAME table).

COH 700

SONY CXD8661R L9A0088 WE19960 NNM 9738
SONY CXD8654Q
MOTOROLA MC44200FT (Triple 8-Bit Video DAC)
RAM: SEC KM4132G271Q-10 (x2), SEC KM416V1204BT-L5 (x2, note there are 2 additional unpopulated posions for this RAM also)
X1  : V100.00 KDS 745 (near CXD8661R)
X101: M53.693 KDS 745 (near CXD8654Q)

*/

#include "driver.h"
#include "state.h"
#include "cpu/mips/psx.h"
#include "includes/psx.h"
#include "cpu/h83002/h83002.h"
#include "sound/c352.h"
#include <time.h>

#define VERBOSE_LEVEL ( 0 )

static INLINE void verboselog( int n_level, const char *s_fmt, ... )
{
	if( VERBOSE_LEVEL >= n_level )
	{
		va_list v;
		char buf[ 32768 ];
		va_start( v, s_fmt );
		vsprintf( buf, s_fmt, v );
		va_end( v );
		log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: %s", activecpu_get_pc(), buf );
	}
}

static data32_t *namcos12_sharedram;

static WRITE32_HANDLER( sharedram_w )
{
	verboselog( 1, "sharedram_w( %08x, %08x, %08x )\n", ( offset * 4 ), data, mem_mask );
	COMBINE_DATA( &namcos12_sharedram[ offset ] );
}

static READ32_HANDLER( sharedram_r )
{
	verboselog( 1, "sharedram_r( %08x, %08x ) %08x\n", ( offset * 4 ), mem_mask, namcos12_sharedram[ offset ] );
	return namcos12_sharedram[ offset ];
}

#define SHRAM( x ) namcos12_sharedram[ ( x ) / 4 ]

/* true if the running game is "name" or a clone of it */
static int namcos12_is( const char *name )
{
	return strcmp( Machine->gamedrv->name, name ) == 0 ||
		( Machine->gamedrv->clone_of != NULL && strcmp( Machine->gamedrv->clone_of->name, name ) == 0 );
}

static INTERRUPT_GEN( namcos12_vblank )
{
	UINT16 n_coin;
	static UINT16 n_oldcoin = 0;

	SHRAM( 0x3000 ) = ( SHRAM( 0x3000 ) & 0x0000ffff ) | 0x76010000;
	SHRAM( 0x30f0 ) = ( SHRAM( 0x30f0 ) & 0xffff0000 ) | 0x00000000;
	SHRAM( 0x305c ) = ( SHRAM( 0x305c ) & 0x0000ffff ) | 0x00000000;
	SHRAM( 0x3068 ) = ( SHRAM( 0x3068 ) & 0x0000ffff ) | 0x00000000;
	SHRAM( 0x3078 ) = ( SHRAM( 0x3078 ) & 0xffff0000 ) | 0x00000000;
	SHRAM( 0x3240 ) = ( SHRAM( 0x3240 ) & 0xffff0000 ) | 0x00000000;
	SHRAM( 0x3940 ) = ( SHRAM( 0x3940 ) & 0xffff0000 ) | 0x00000000;

	SHRAM( 0x3380 ) = readinputport( 0 );

	SHRAM( 0x3180 ) = SHRAM( 0x3140 );
	SHRAM( 0x3140 ) = readinputport( 1 ) | ( readinputport( 2 ) << 16 );
	n_coin = readinputport( 3 );
	if( ( n_coin & n_oldcoin & 0x01 ) != 0 )
	{
		SHRAM( 0x32c0 ) = ( SHRAM( 0x32c0 ) & 0xffff0000 ) | ( ( SHRAM( 0x32c0 ) + 0x00000001 ) & 0x0000ffff );
	}
	if( ( n_coin & n_oldcoin & 0x02 ) != 0 )
	{
		SHRAM( 0x32c0 ) = ( SHRAM( 0x32c0 ) & 0x0000ffff ) | ( ( SHRAM( 0x32c0 ) + 0x00010000 ) & 0xffff0000 );
	}
	if( ( SHRAM( 0x3140 ) & ~SHRAM( 0x3180 ) & 0x00004000 ) != 0 )
	{
		SHRAM( 0x3200 ) = ( SHRAM( 0x3200 ) & 0xffff0000 ) | ( ( SHRAM( 0x3200 ) + 0x00000001 ) & 0x0000ffff );
	}
	if( ( SHRAM( 0x3140 ) & ~SHRAM( 0x3180 ) & 0x40000000 ) != 0 )
	{
		SHRAM( 0x3200 ) = ( SHRAM( 0x3200 ) & 0x0000ffff ) | ( ( SHRAM( 0x3200 ) + 0x00010000 ) & 0xffff0000 );
	}
	SHRAM( 0x3900 ) = SHRAM( 0x3200 ) + SHRAM( 0x32c0 );

	n_oldcoin = ~n_coin;

	psx_vblank();

	/* kludge: protection hacks */
	if( namcos12_is( "fgtlayer" ) )
	{
		data8_t *RAM = memory_region( REGION_CPU1 );
		if( *( (data32_t *)&RAM[ 0x2ac494 ] ) == 0x080ab125 )
		{
			*( (data32_t *)&RAM[ 0x2ac494 ] ) = 0;
			psxcpu_ram_written( &RAM[ 0x2ac494 ], 4 );
		}
	}
	else if( namcos12_is( "pacapp" ) )
	{
		data8_t *RAM = memory_region( REGION_CPU1 );
		if( *( (data32_t *)&RAM[ 0x16d50 ] ) == 0x08005b54 )
		{
			*( (data32_t *)&RAM[ 0x16d50 ] ) = 0;
			psxcpu_ram_written( &RAM[ 0x16d50 ], 4 );
		}
	}
}


/***************************************************************************

  H8/3002 sound & I/O CPU and C352 (backported from MAME 0.152 / current MAME)

  The H8/3002 runs the sound program from SOUND.11S, drives the C352 and
  handles the inputs, coins, RTC and video settings, talking to the main
  CPU through the shared RAM.  Machines using it (coh700s) do not need the
  input/coin simulation done in namcos12_vblank.

***************************************************************************/

static data16_t *namcos12_h8ram;	/* H8 side of 0x080000-0x08ffff not seen by the PSX */

/* the PSX sees the first 16KB (0x1f080000-0x1f083fff) as 32-bit LE, the H8 as 16-bit words */
#define SHRAM16( x ) ( ( (data16_t *)namcos12_sharedram )[ WORD_XOR_LE( ( x ) * 2 ) / 2 ] )

static READ16_HANDLER( s12_h8_sharedram_r )
{
	if( offset < 0x2000 )
	{
		return SHRAM16( offset );
	}
	return namcos12_h8ram[ offset - 0x2000 ];
}

static WRITE16_HANDLER( s12_h8_sharedram_w )
{
	if( offset < 0x2000 )
	{
		COMBINE_DATA( &SHRAM16( offset ) );
	}
	else
	{
		COMBINE_DATA( &namcos12_h8ram[ offset - 0x2000 ] );
	}
}

static READ16_HANDLER( s12_h8_in0_r )
{
	return readinputport( 1 );
}

static READ16_HANDLER( s12_h8_in1_r )
{
	return readinputport( 2 );
}

static MEMORY_READ16_START( s12h8_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x080000, 0x08ffff, s12_h8_sharedram_r },
	{ 0x280000, 0x287fff, c352_0_r },
	{ 0x300000, 0x300001, s12_h8_in0_r },
	{ 0x300002, 0x300003, s12_h8_in1_r },
	{ 0x300010, 0x300011, MRA16_NOP },	/* golgo13 writes here a lot, possibly also a wait state generator? */
	{ 0x300030, 0x300031, MRA16_NOP },	/* most S12 bioses write here simply to generate a wait state.  there is no deeper meaning. */
MEMORY_END

static MEMORY_WRITE16_START( s12h8_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x080000, 0x08ffff, s12_h8_sharedram_w },
	{ 0x280000, 0x287fff, c352_0_w },
	{ 0x300010, 0x300011, MWA16_NOP },
	{ 0x300030, 0x300031, MWA16_NOP },
MEMORY_END

/* emulation of the Epson R4543 real time clock
   in System 12, bit 0 of H8/3002 port A is connected to it's chip enable
   the actual I/O takes place through the H8/3002's serial port B. */
static int s12_porta = 0, s12_rtcstate = 0;
static int s12_boot_hack = 0;
static int s12_lastpB = 0x50, s12_setstate = 0, s12_setnum = 0, s12_settings[ 8 ];

static data8_t make_bcd( data8_t data )
{
	return ( ( data / 10 ) << 4 ) | ( data % 10 );
}

static data8_t s12_mcu_rtc_r( void )
{
	/* the R4543 shifts out 52 bits LSB first: seconds, minutes, hours (8 bits each),
	   day of the week (4 bits), day, month, year (8 bits each).  The H8 reads them
	   8 bits at a time, so everything after the day of the week is nibble-shifted. */
	time_t now = time( NULL );
	struct tm *t = localtime( &now );
	UINT64 bits = 0;
	data8_t ret;

	if( t != NULL )
	{
		static const int weekday[ 7 ] = { 7, 1, 2, 3, 4, 5, 6 };	/* 1 = Monday, 7 = Sunday */

		bits  = (UINT64)make_bcd( t->tm_sec );					/* seconds (BCD, 0-59), bit 7 = battery low */
		bits |= (UINT64)make_bcd( t->tm_min ) << 8;				/* minutes (BCD, 0-59) */
		bits |= (UINT64)make_bcd( t->tm_hour ) << 16;			/* hour (BCD, 0-23) */
		bits |= (UINT64)( weekday[ t->tm_wday ] & 0x0f ) << 24;	/* day of the week */
		bits |= (UINT64)make_bcd( t->tm_mday ) << 28;			/* day (BCD, 1-31) */
		bits |= (UINT64)make_bcd( t->tm_mon + 1 ) << 36;		/* month (BCD, 1-12) */
		bits |= (UINT64)make_bcd( t->tm_year % 100 ) << 44;		/* year (BCD, 0-99) */
	}

	ret = ( s12_rtcstate < 7 ) ? (data8_t)( bits >> ( s12_rtcstate * 8 ) ) : 0;
	s12_rtcstate++;
	return ret;
}

static void s12_mcu_settings_w( data8_t data )
{
	if( s12_setstate )
	{
		/* data */
		s12_settings[ s12_setnum & 7 ] = data;
	}
	else
	{
		/* setting number */
		s12_setnum = ( data >> 4 ) - 1;
	}

	s12_setstate ^= 1;
}

static data8_t s12_h8_io_r( int port )
{
	switch( port )
	{
	case H8_PORT_7:
		return readinputport( 0 ) & 0xff;	/* DIP switches */
	case H8_PORT_8:
		return 0x02;
	case H8_PORT_A:
		return s12_porta;
	case H8_PORT_B:
		/* golgo13 won't boot if this doesn't toggle every read */
		s12_lastpB ^= 0x80;
		return s12_lastpB;
	case H8_SERIAL_1:
		return s12_mcu_rtc_r();
	}
	return 0;
}

static void s12_h8_io_w( int port, data8_t data )
{
	switch( port )
	{
	case H8_PORT_A:
		/* bit 0 = chip enable for the RTC, reset the state on the rising edge of the bit */
		if( ( !( s12_porta & 1 ) ) && ( data & 1 ) )
		{
			s12_rtcstate = 0;
		}
		s12_porta = data;
		break;
	case H8_PORT_B:
		/* bit 7 = chip enable for the video settings controller */
		if( data & 0x80 )
		{
			s12_setstate = 0;
		}
		s12_lastpB = data;
		break;
	case H8_SERIAL_1:
		s12_mcu_settings_w( data );
		break;
	}
}

static INTERRUPT_GEN( namcos12_vblank_h8 )
{
	/* inputs, coins and sound are handled by the H8/3002 */
	psx_vblank();
}

static struct C352interface c352_interface =
{
	25401600,		/* clock */
	288,			/* divider: 88.2kHz output */
	REGION_SOUND1,
	100
};

static data32_t m_n_bankoffset;

static WRITE32_HANDLER( bankoffset_w )
{
	m_n_bankoffset = data;

	cpu_setbank( 7, memory_region( REGION_USER3 ) + ( m_n_bankoffset * 0x200000 ) );

	verboselog( 1, "bankoffset_w( %08x, %08x, %08x ) %08x\n", offset, data, mem_mask, m_n_bankoffset );
}

static data32_t m_n_dmaoffset;

static WRITE32_HANDLER( dmaoffset_w )
{
	if( ACCESSING_LSW32 )
	{
		m_n_dmaoffset = ( offset * 4 ) + ( data << 16 );
	}
	verboselog( 1, "dmaoffset_w( %08x, %08x, %08x ) %08x\n", offset, data, mem_mask, m_n_dmaoffset );
}

static int m_has_tektagt_dma;
static data32_t m_n_tektagdmaoffset;

static void namcos12_rom_read( UINT32 n_address, INT32 n_size )
{
	INT32 n_left;
	UINT32 *p_n_src;
	UINT32 *p_n_dst;
	data32_t n_offset = m_has_tektagt_dma ? m_n_tektagdmaoffset : m_n_dmaoffset;

	verboselog( 1, "namcos12_rom_read( %08x, %08x )\n", n_address, n_size );

	m_has_tektagt_dma = 0;

	if( n_offset >= 0x80000000 )
	{
		p_n_src = (data32_t *)( memory_region( REGION_USER2 ) + ( n_offset & 0x003fffff ) );
	}
	else
	{
		n_offset &= 0x7fffffff;
		if( n_offset >= memory_region_length( REGION_USER3 ) )
		{
			n_offset = 0;
		}
		p_n_src = (data32_t *)( memory_region( REGION_USER3 ) + n_offset );
		n_left = ( memory_region_length( REGION_USER3 ) - n_offset ) / 4;
		if( n_size > n_left )
		{
			n_size = n_left;
		}
	}

	p_n_dst = (data32_t *)( memory_region( REGION_CPU1 ) + n_address );

	n_left = ( memory_region_length( REGION_CPU1 ) - n_address ) / 4;
	if( n_size > n_left )
	{
		n_size = n_left;
	}
	while( n_size > 0 )
	{
		*( p_n_dst++ ) = *( p_n_src++ );
		n_size--;
	}
}

static MEMORY_WRITE32_START( namcos12_writemem )
	{ 0x00000000, 0x003fffff, MWA32_RAM },    /* ram */
	{ 0x1f000000, 0x1f000003, bankoffset_w }, /* banking */
	{ 0x1f008000, 0x1f008003, MWA32_NOP },    /* ?? */
	{ 0x1f010000, 0x1f010003, MWA32_NOP },    /* ?? */
	{ 0x1f018000, 0x1f018003, MWA32_NOP },    /* ?? */
	{ 0x1f080000, 0x1f083fff, sharedram_w, &namcos12_sharedram }, /* shared ram?? */
	{ 0x1f140000, 0x1f141fff, MWA32_RAM, (data32_t **)&generic_nvram, &generic_nvram_size }, /* flash */
	{ 0x1f1bff08, 0x1f1bff0f, MWA32_NOP },    /* ?? */
	{ 0x1f700000, 0x1f70ffff, dmaoffset_w },  /* dma */
	{ 0x1f800000, 0x1f8003ff, MWA32_BANK1 },  /* scratchpad */
	{ 0x1f801000, 0x1f801007, MWA32_NOP },
	{ 0x1f801008, 0x1f80100b, MWA32_RAM },    /* ?? */
	{ 0x1f80100c, 0x1f80102f, MWA32_NOP },
	{ 0x1f801040, 0x1f80105f, psx_sio_w },
	{ 0x1f801060, 0x1f80106f, MWA32_NOP },
	{ 0x1f801070, 0x1f801077, psx_irq_w },
	{ 0x1f801080, 0x1f8010ff, psx_dma_w },
	{ 0x1f801100, 0x1f80113f, psx_counter_w },
	{ 0x1f801810, 0x1f801817, psx_gpu_w },
	{ 0x1f801820, 0x1f801827, psx_mdec_w },
	{ 0x1f801c00, 0x1f801dff, MWA32_NOP },
	{ 0x1f802020, 0x1f802033, MWA32_RAM },
	{ 0x1f802040, 0x1f802043, MWA32_NOP },
	{ 0x1fc00000, 0x1fffffff, MWA32_ROM },    /* bios */
	{ 0x80000000, 0x803fffff, MWA32_BANK3 },  /* ram mirror */
	{ 0x9fc00000, 0x9fffffff, MWA32_ROM },    /* bios */
	{ 0xa0000000, 0xa03fffff, MWA32_BANK5 },  /* ram mirror */
	{ 0xbfc00000, 0xbfffffff, MWA32_ROM },    /* bios */
	{ 0xfffe0130, 0xfffe0133, MWA32_NOP },
MEMORY_END

static MEMORY_READ32_START( namcos12_readmem )
	{ 0x00000000, 0x003fffff, MRA32_RAM },    /* ram */
	{ 0x1f000000, 0x1f000003, MRA32_NOP },    /* banking */
	{ 0x1f080000, 0x1f083fff, sharedram_r },  /* shared ram */
	{ 0x1f140000, 0x1f141fff, MRA32_RAM },    /* flash */
	{ 0x1f800000, 0x1f8003ff, MRA32_BANK1 },  /* scratchpad */
	{ 0x1f801008, 0x1f80100b, MRA32_RAM },    /* ?? */
	{ 0x1f801010, 0x1f801013, MRA32_NOP },
	{ 0x1f801014, 0x1f801017, MRA32_NOP },
	{ 0x1f801040, 0x1f80105f, psx_sio_r },
	{ 0x1f801070, 0x1f801077, psx_irq_r },
	{ 0x1f801080, 0x1f8010ff, psx_dma_r },
	{ 0x1f801100, 0x1f80113f, psx_counter_r },
	{ 0x1f801810, 0x1f801817, psx_gpu_r },
	{ 0x1f801820, 0x1f801827, psx_mdec_r },
	{ 0x1f801c00, 0x1f801dff, MRA32_NOP },
	{ 0x1f802020, 0x1f802033, MRA32_RAM },
	{ 0x1fa00000, 0x1fbfffff, MRA32_BANK7 },  /* banked roms */
	{ 0x1fc00000, 0x1fffffff, MRA32_BANK2 },  /* bios mirror */
	{ 0x80000000, 0x803fffff, MRA32_BANK3 },  /* ram mirror */
	{ 0x9fc00000, 0x9fffffff, MRA32_BANK4 },  /* bios mirror */
	{ 0xa0000000, 0xa03fffff, MRA32_BANK5 },  /* ram mirror */
	{ 0xbfc00000, 0xbfffffff, MRA32_BANK6 },  /* bios */
MEMORY_END

/***************************************************************************

  Tekken Tag Tournament protection and second DMA offset (from current MAME)

***************************************************************************/

static int m_ttt_cnt;
static data32_t m_ttt_val[ 2 ];

static void tektagt_protection_1_w16( int offset, data16_t data )
{
	/* second dma offset or protection ref values write */
	switch( offset )
	{
	case 0:
		m_n_tektagdmaoffset = data;
		if( m_ttt_cnt != 2 )
			m_ttt_val[ m_ttt_cnt ] = data;
		break;
	case 1:
		m_n_tektagdmaoffset |= data << 16;
		if( m_ttt_cnt != 2 )
			m_ttt_val[ m_ttt_cnt++ ] |= data << 16;
		break;
	}
}

static WRITE32_HANDLER( tektagt_protection_1_w )
{
	if( ACCESSING_LSW32 )
		tektagt_protection_1_w16( 0, data & 0xffff );
	if( ACCESSING_MSW32 )
		tektagt_protection_1_w16( 1, data >> 16 );
}

static READ32_HANDLER( tektagt_protection_1_r )
{
	/* reads are either ignored or bit 15 is tested for a busy flag */
	return 0x8000;
}

static WRITE32_HANDLER( tektagt_protection_2_w )
{
	/* writes are 0 or rand(), only used as a "start prot value write" trigger */
	if( ACCESSING_LSW32 )
		m_ttt_cnt = 0;
}

static data16_t tektagt_protection_2_r16( int offset )
{
	if( m_ttt_cnt == 2 )
	{
		if( m_ttt_val[ 0 ] == 0x806d2c24 && m_ttt_val[ 1 ] == 0xd5545715 )
		{
			if( offset == 0 )
				return 0x36e2;
		}
		else if( m_ttt_val[ 0 ] == 0x804c2c84 && m_ttt_val[ 1 ] == 0xd5545615 )
		{
			if( offset == 0 )
				return 0x2651;
		}
		else if( m_ttt_val[ 0 ] == 0x2aaba8e6 && m_ttt_val[ 1 ] == 0x00820040 )
		{
			if( offset == 1 )
				return 0x4186;
		}
		else if( m_ttt_val[ 0 ] == 0x2aaba592 && m_ttt_val[ 1 ] == 0x01780544 )
		{
			if( offset == 1 )
				return 0x3c7d;
		}
		else
		{
			if( ( ( m_ttt_val[ 1 ] >> 16 ) & 0xff ) == 0xa9 )
				return 0x552e;
		}
	}
	return 0;
}

static READ32_HANDLER( tektagt_protection_2_r )
{
	data32_t ret = 0;
	if( ACCESSING_LSW32 )
		ret |= tektagt_protection_2_r16( 0 );
	if( ACCESSING_MSW32 )
		ret |= (data32_t)tektagt_protection_2_r16( 1 ) << 16;
	return ret;
}

static READ32_HANDLER( tektagt_protection_3_r )
{
	m_has_tektagt_dma = 1;
	/* always ignored */
	return 0;
}

static MEMORY_READ32_START( tektagt_readmem )
	{ 0x1f700000, 0x1f700003, tektagt_protection_3_r },
	{ 0x1ff00000, 0x1ff00003, tektagt_protection_3_r },
	{ 0x1fb00000, 0x1fb00003, tektagt_protection_1_r },
	{ 0x1fb80000, 0x1fb80003, tektagt_protection_2_r },
	{ 0x00000000, 0x003fffff, MRA32_RAM },    /* ram */
	{ 0x1f000000, 0x1f000003, MRA32_NOP },    /* banking */
	{ 0x1f080000, 0x1f083fff, sharedram_r },  /* shared ram */
	{ 0x1f140000, 0x1f141fff, MRA32_RAM },    /* flash */
	{ 0x1f800000, 0x1f8003ff, MRA32_BANK1 },  /* scratchpad */
	{ 0x1f801008, 0x1f80100b, MRA32_RAM },    /* ?? */
	{ 0x1f801010, 0x1f801013, MRA32_NOP },
	{ 0x1f801014, 0x1f801017, MRA32_NOP },
	{ 0x1f801040, 0x1f80105f, psx_sio_r },
	{ 0x1f801070, 0x1f801077, psx_irq_r },
	{ 0x1f801080, 0x1f8010ff, psx_dma_r },
	{ 0x1f801100, 0x1f80113f, psx_counter_r },
	{ 0x1f801810, 0x1f801817, psx_gpu_r },
	{ 0x1f801820, 0x1f801827, psx_mdec_r },
	{ 0x1f801c00, 0x1f801dff, MRA32_NOP },
	{ 0x1f802020, 0x1f802033, MRA32_RAM },
	{ 0x1fa00000, 0x1fbfffff, MRA32_BANK7 },  /* banked roms */
	{ 0x1fc00000, 0x1fffffff, MRA32_BANK2 },  /* bios mirror */
	{ 0x80000000, 0x803fffff, MRA32_BANK3 },  /* ram mirror */
	{ 0x9fc00000, 0x9fffffff, MRA32_BANK4 },  /* bios mirror */
	{ 0xa0000000, 0xa03fffff, MRA32_BANK5 },  /* ram mirror */
	{ 0xbfc00000, 0xbfffffff, MRA32_BANK6 },  /* bios */
MEMORY_END

static MEMORY_WRITE32_START( tektagt_writemem )
	{ 0x1fb00000, 0x1fb00003, tektagt_protection_1_w },
	{ 0x1fb80000, 0x1fb80003, tektagt_protection_2_w },
	{ 0x00000000, 0x003fffff, MWA32_RAM },    /* ram */
	{ 0x1f000000, 0x1f000003, bankoffset_w }, /* banking */
	{ 0x1f008000, 0x1f008003, MWA32_NOP },    /* ?? */
	{ 0x1f010000, 0x1f010003, MWA32_NOP },    /* ?? */
	{ 0x1f018000, 0x1f018003, MWA32_NOP },    /* ?? */
	{ 0x1f080000, 0x1f083fff, sharedram_w, &namcos12_sharedram }, /* shared ram?? */
	{ 0x1f140000, 0x1f141fff, MWA32_RAM, (data32_t **)&generic_nvram, &generic_nvram_size }, /* flash */
	{ 0x1f1bff08, 0x1f1bff0f, MWA32_NOP },    /* ?? */
	{ 0x1f700000, 0x1f70ffff, dmaoffset_w },  /* dma */
	{ 0x1f800000, 0x1f8003ff, MWA32_BANK1 },  /* scratchpad */
	{ 0x1f801000, 0x1f801007, MWA32_NOP },
	{ 0x1f801008, 0x1f80100b, MWA32_RAM },    /* ?? */
	{ 0x1f80100c, 0x1f80102f, MWA32_NOP },
	{ 0x1f801040, 0x1f80105f, psx_sio_w },
	{ 0x1f801060, 0x1f80106f, MWA32_NOP },
	{ 0x1f801070, 0x1f801077, psx_irq_w },
	{ 0x1f801080, 0x1f8010ff, psx_dma_w },
	{ 0x1f801100, 0x1f80113f, psx_counter_w },
	{ 0x1f801810, 0x1f801817, psx_gpu_w },
	{ 0x1f801820, 0x1f801827, psx_mdec_w },
	{ 0x1f801c00, 0x1f801dff, MWA32_NOP },
	{ 0x1f802020, 0x1f802033, MWA32_RAM },
	{ 0x1f802040, 0x1f802043, MWA32_NOP },
	{ 0x1fc00000, 0x1fffffff, MWA32_ROM },    /* bios */
	{ 0x80000000, 0x803fffff, MWA32_BANK3 },  /* ram mirror */
	{ 0x9fc00000, 0x9fffffff, MWA32_ROM },    /* bios */
	{ 0xa0000000, 0xa03fffff, MWA32_BANK5 },  /* ram mirror */
	{ 0xbfc00000, 0xbfffffff, MWA32_ROM },    /* bios */
	{ 0xfffe0130, 0xfffe0133, MWA32_NOP },
MEMORY_END

static DRIVER_INIT( namcos12 )
{
	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	cpu_setbank( 2, memory_region( REGION_USER2 ) );
	cpu_setbank( 3, memory_region( REGION_CPU1 ) );
	cpu_setbank( 4, memory_region( REGION_USER2 ) );
	cpu_setbank( 5, memory_region( REGION_CPU1 ) );
	cpu_setbank( 6, memory_region( REGION_USER2 ) );

	psx_driver_init();

	psx_dma_install_read_handler( 5, namcos12_rom_read );

	state_save_register_UINT32( "namcos12", 0, "m_n_dmaoffset", &m_n_dmaoffset, 1 );
	state_save_register_UINT32( "namcos12", 0, "m_n_bankoffset", &m_n_bankoffset, 1 );

	/* kludge: some kind of protection? */
	if( namcos12_is( "golgo13" ) ||
		namcos12_is( "pacapp" ) )
	{
		data8_t *RAM = memory_region( REGION_USER2 );

		*( (data32_t *)&RAM[ 0x20280 ] ) = 0;
		*( (data32_t *)&RAM[ 0x20284 ] ) = 0;
		*( (data32_t *)&RAM[ 0x20288 ] ) = 0;
	}
}

static DRIVER_INIT( namcos12_h8 )
{
	init_namcos12();

	namcos12_h8ram = auto_malloc( 0xc000 );
	memset( namcos12_h8ram, 0, 0xc000 );
	state_save_register_UINT16( "namcos12", 0, "h8ram", namcos12_h8ram, 0xc000 / 2 );

	state_save_register_int( "namcos12", 0, "s12_porta", &s12_porta );
	state_save_register_int( "namcos12", 0, "s12_rtcstate", &s12_rtcstate );
	state_save_register_int( "namcos12", 0, "s12_lastpB", &s12_lastpB );
	state_save_register_int( "namcos12", 0, "s12_setstate", &s12_setstate );
	state_save_register_int( "namcos12", 0, "s12_setnum", &s12_setnum );

	h8_3002_set_io_handlers( s12_h8_io_r, s12_h8_io_w );
	/* the sound program polls the shared RAM in "mov.b @aa:24 / bne" loops */
	h8_3002_set_idle_poll( 0x080000, 0x08ffff );
	s12_boot_hack = 0;
}

static DRIVER_INIT( namcos12_h8b )
{
	init_namcos12_h8();
	s12_boot_hack = 1;
}

static DRIVER_INIT( tektagt )
{
	init_namcos12_h8b();

	m_has_tektagt_dma = 0;
	m_n_tektagdmaoffset = 0;
	m_ttt_cnt = 0;
	m_ttt_val[ 0 ] = m_ttt_val[ 1 ] = 0;
	state_save_register_int( "tektagt", 0, "has_dma", &m_has_tektagt_dma );
	state_save_register_UINT32( "tektagt", 0, "dmaoffset", &m_n_tektagdmaoffset, 1 );
	state_save_register_int( "tektagt", 0, "ttt_cnt", &m_ttt_cnt );
	state_save_register_UINT32( "tektagt", 0, "ttt_val", m_ttt_val, 2 );
}

MACHINE_INIT( namcos12 )
{
	psx_machine_init();
	psx_machine_cpu_speedups();
}

MACHINE_INIT( namcos12_h8 )
{
	psx_machine_init();
	psx_machine_cpu_speedups();

	s12_porta = 0;
	s12_rtcstate = 0;
	s12_lastpB = 0x50;
	s12_setstate = 0;
	s12_setnum = 0;

	if( s12_boot_hack )
	{
		/* 12 bytes in ram are checked early in the boot sequence, how they get
		   into memory is not currently known (from current MAME, coh700b) */
		memcpy( memory_region( REGION_CPU1 ) + 0x10000, memory_region( REGION_USER2 ) + 0x20280, 12 );
	}
}

static MACHINE_DRIVER_START( coh700 )
	/* basic machine hardware */
	MDRV_CPU_ADD( PSXCPU, 33868800 / 2 ) /* 33MHz ?? */
	MDRV_CPU_MEMORY( namcos12_readmem, namcos12_writemem )
	MDRV_CPU_VBLANK_INT( namcos12_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( namcos12 )
	MDRV_NVRAM_HANDLER( generic_0fill )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
#if defined( MAME_DEBUG )
	MDRV_SCREEN_SIZE( 1024, 1024 )
	MDRV_VISIBLE_AREA( 0, 1023, 0, 1023 )
#else
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
#endif
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
MACHINE_DRIVER_END

/* System 12 with the H8/3002 sound/I-O CPU and the C352 */
static MACHINE_DRIVER_START( coh700s )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 33868800 / 2 ) /* 33MHz ?? */
	MDRV_CPU_MEMORY( namcos12_readmem, namcos12_writemem )
	MDRV_CPU_VBLANK_INT( namcos12_vblank_h8, 1 )

	MDRV_CPU_ADD( H83002, 16934400 ) /* frequency based on research (superctr) */
	MDRV_CPU_MEMORY( s12h8_readmem, s12h8_writemem )
	MDRV_CPU_VBLANK_INT( irq1_line_pulse, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 40 )

	MDRV_MACHINE_INIT( namcos12_h8 )
	MDRV_NVRAM_HANDLER( generic_0fill )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
#if defined( MAME_DEBUG )
	MDRV_SCREEN_SIZE( 1024, 1024 )
	MDRV_VISIBLE_AREA( 0, 1023, 0, 1023 )
#else
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
#endif
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( C352, c352_interface )
MACHINE_DRIVER_END

/* Tekken Tag Tournament: coh700s + protection / second DMA offset */
static MACHINE_DRIVER_START( tektagt )
	MDRV_IMPORT_FROM( coh700s )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_MEMORY( tektagt_readmem, tektagt_writemem )
MACHINE_DRIVER_END

/* inputs read by the H8/3002 (port 7 and 0x300000/0x300002) */
INPUT_PORTS_START( namcos12_h8 )
	/* DSW (H8 port 7) */
	PORT_START
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Freeze" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0xff3f, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN0 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x0f00, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )
INPUT_PORTS_END

/* Mr. Driller (current MAME): 4-way joysticks, one button */
INPUT_PORTS_START( mrdrillr )
	/* DSW (H8 port 7) */
	PORT_START
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Freeze" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0xff3f, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN0 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0f00, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )
INPUT_PORTS_END

/* Tekken 3: LP/RP on button 1/2, LK/RK on the button 4/5 pins */
INPUT_PORTS_START( tekken3 )
	/* DSW (H8 port 7) */
	PORT_START
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Freeze" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0xff3f, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN0 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0f00, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )
INPUT_PORTS_END

INPUT_PORTS_START( namcos12 )
	/* IN 0 */
	PORT_START
	PORT_BITX( 0x8000, IP_ACTIVE_HIGH, 0, "Test Switch", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0040, 0x0000, "DIP1 (Test)" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0000, "DIP2 (Freeze)" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( On ) )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN 1 */
	PORT_START
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( On ) )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_BUTTON4 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON5 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON6 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN 2 */
	PORT_START
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( On ) )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_SERVICE2 )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN 3 */
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0xfffc, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

ROM_START( aquarush )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "aq1vera.2l",        0x0000000, 0x200000, CRC(91eb9258) SHA1(30e225eb551bfe1bed6b342dd6d597345d64b677) )
	ROM_LOAD16_BYTE( "aq1vera.2p",        0x0000001, 0x200000, CRC(a92f21aa) SHA1(bde33f1f66aaa55031c6b2972b042eef87047cce) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "aq1rom0.7",         0x0000000, 0x800000, CRC(3e64dd33) SHA1(dce3bb84c3069bc202d04f24d7702158dd3194d4) )
	ROM_LOAD(        "aq1rom1.8",         0x0800000, 0x800000, CRC(e4d415cf) SHA1(bbd244adaf704d7daf7341ff3b0a92162927a59b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "aq1vera.11s",  0x000000, 0x080000, CRC(78277e02) SHA1(577ebb6d7ab5e304fb1dc1e7fd5649762e0d1786) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "aq1wave0.2",               0x000000, 0x800000, CRC(0cf7278d) SHA1(aee31e4d9b3522f42325071768803c542aa6de09) )
ROM_END

ROM_START( ehrgeiz )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "eg2vera.2e",        0x0000000, 0x200000, CRC(9174ec90) SHA1(273bb9c9f0a7eb48470601a0eadf450908ac4d92) )
	ROM_LOAD16_BYTE( "eg2vera.2j",        0x0000001, 0x200000, CRC(a8388248) SHA1(89eeb6095cc8c7ad6cdc8480cff6f688f07f64d7) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "eg1rom0l.12",       0x0000000, 0x800000, CRC(fe11a48e) SHA1(bdb2af5949bb4a324dab01489e9488aa5b9bd129) )
	ROM_LOAD16_BYTE( "eg1rom0u.11",       0x0000001, 0x800000, CRC(7cb90c25) SHA1(0ff6f7bf8b7eb2e0706bd235fcb9a95ea639fae6) )
	ROM_LOAD16_BYTE( "eg1fl1l.9",         0x1000000, 0x200000, CRC(dd4cac2b) SHA1(f49d0055303b3d8639e94f93bce6bf160cc99913) )
	ROM_LOAD16_BYTE( "eg1fl1u.10",        0x1000001, 0x200000, CRC(e3348407) SHA1(829bf1be0f74fd385e325d774f1449e28aba1e4d) )
	ROM_LOAD16_BYTE( "eg1fl2l.7",         0x1400000, 0x200000, CRC(34493262) SHA1(9dd5b1b25e7232460bf5ee5b339d9f536ec26979) )
	ROM_LOAD16_BYTE( "eg1fl2u.8",         0x1400001, 0x200000, CRC(4fb84f1d) SHA1(714e0ef56871d7b4568bc6dda08bbb1c01dbba37) )
	ROM_LOAD16_BYTE( "eg1fl3l.5",         0x1800000, 0x200000, CRC(f9441f87) SHA1(70944160620ba2fc80b4fc3a7b61c33622298a8b) )
	ROM_LOAD16_BYTE( "eg1fl3u.6",         0x1800001, 0x200000, CRC(cea397de) SHA1(3791dbadb5699c805c27930e59c61af4d62f77f7) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "eg1vera.11s",  0x0000000, 0x080000, CRC(9e44645e) SHA1(374eb4a4c09d6b5b7af5ff0efec16b4d2aacbe2b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "eg1wave0.2",               0x0000000, 0x800000, CRC(961fe69f) SHA1(0189a061959a8d94b9d2db627911264faf9f28fd) )
ROM_END

ROM_START( ehrgeizua )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "eg3vera.2l",        0x0000000, 0x200000, CRC(64c00ff0) SHA1(fc7980bc8d98c810aed2eb6b3265d150784dfc15) )
	ROM_LOAD16_BYTE( "eg3vera.2p",        0x0000001, 0x200000, CRC(e722c030) SHA1(4669a7861c14d97048728989708a0fa3733f83a8) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "eg1rom0l.12",       0x0000000, 0x800000, CRC(fe11a48e) SHA1(bdb2af5949bb4a324dab01489e9488aa5b9bd129) )
	ROM_LOAD16_BYTE( "eg1rom0u.11",       0x0000001, 0x800000, CRC(7cb90c25) SHA1(0ff6f7bf8b7eb2e0706bd235fcb9a95ea639fae6) )
	ROM_LOAD16_BYTE( "eg1fl1l.9",         0x1000000, 0x200000, CRC(dd4cac2b) SHA1(f49d0055303b3d8639e94f93bce6bf160cc99913) )
	ROM_LOAD16_BYTE( "eg1fl1u.10",        0x1000001, 0x200000, CRC(e3348407) SHA1(829bf1be0f74fd385e325d774f1449e28aba1e4d) )
	ROM_LOAD16_BYTE( "eg1fl2l.7",         0x1400000, 0x200000, CRC(34493262) SHA1(9dd5b1b25e7232460bf5ee5b339d9f536ec26979) )
	ROM_LOAD16_BYTE( "eg1fl2u.8",         0x1400001, 0x200000, CRC(4fb84f1d) SHA1(714e0ef56871d7b4568bc6dda08bbb1c01dbba37) )
	ROM_LOAD16_BYTE( "eg1fl3l.5",         0x1800000, 0x200000, CRC(f9441f87) SHA1(70944160620ba2fc80b4fc3a7b61c33622298a8b) )
	ROM_LOAD16_BYTE( "eg1fl3u.6",         0x1800001, 0x200000, CRC(cea397de) SHA1(3791dbadb5699c805c27930e59c61af4d62f77f7) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "eg1vera.11s",  0x0000000, 0x080000, CRC(9e44645e) SHA1(374eb4a4c09d6b5b7af5ff0efec16b4d2aacbe2b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "eg1wave0.2",               0x0000000, 0x800000, CRC(961fe69f) SHA1(0189a061959a8d94b9d2db627911264faf9f28fd) )
ROM_END

ROM_START( ehrgeizja )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "eg1vera.2l",        0x0000000, 0x200000, CRC(302d62cf) SHA1(e2de280ae4475829398a6770aed8eab0ed35b1ce) )
	ROM_LOAD16_BYTE( "eg1vera.2p",        0x0000001, 0x200000, CRC(1d7fb3a1) SHA1(dc038a639f89d7c8daaf987b728fde175fe4dbec) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "eg1rom0l.12",       0x0000000, 0x800000, CRC(fe11a48e) SHA1(bdb2af5949bb4a324dab01489e9488aa5b9bd129) )
	ROM_LOAD16_BYTE( "eg1rom0u.11",       0x0000001, 0x800000, CRC(7cb90c25) SHA1(0ff6f7bf8b7eb2e0706bd235fcb9a95ea639fae6) )
	ROM_LOAD16_BYTE( "eg1fl1l.9",         0x1000000, 0x200000, CRC(dd4cac2b) SHA1(f49d0055303b3d8639e94f93bce6bf160cc99913) )
	ROM_LOAD16_BYTE( "eg1fl1u.10",        0x1000001, 0x200000, CRC(e3348407) SHA1(829bf1be0f74fd385e325d774f1449e28aba1e4d) )
	ROM_LOAD16_BYTE( "eg1fl2l.7",         0x1400000, 0x200000, CRC(34493262) SHA1(9dd5b1b25e7232460bf5ee5b339d9f536ec26979) )
	ROM_LOAD16_BYTE( "eg1fl2u.8",         0x1400001, 0x200000, CRC(4fb84f1d) SHA1(714e0ef56871d7b4568bc6dda08bbb1c01dbba37) )
	ROM_LOAD16_BYTE( "eg1fl3l.5",         0x1800000, 0x200000, CRC(f9441f87) SHA1(70944160620ba2fc80b4fc3a7b61c33622298a8b) )
	ROM_LOAD16_BYTE( "eg1fl3u.6",         0x1800001, 0x200000, CRC(cea397de) SHA1(3791dbadb5699c805c27930e59c61af4d62f77f7) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "eg1vera.11s",  0x0000000, 0x080000, CRC(9e44645e) SHA1(374eb4a4c09d6b5b7af5ff0efec16b4d2aacbe2b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "eg1wave0.2",               0x0000000, 0x800000, CRC(961fe69f) SHA1(0189a061959a8d94b9d2db627911264faf9f28fd) )
ROM_END

ROM_START( fgtlayer )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ftl3vera.2e", 0x0000000, 0x200000, CRC(17c23471) SHA1(22b5a25569c085f059c1c846cf628a1f140035e9) )
	ROM_LOAD16_BYTE( "ftl3vera.2j", 0x0000001, 0x200000, CRC(1626465d) SHA1(941e7f8fbbaa232aab10a52aba1c462b4c91bb71) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "ftl1rom0.9",  0x0000000, 0x800000, CRC(e33ce365) SHA1(f7e5b98d2e8e5f233265909477c84331f016ebfb) )
	ROM_LOAD(        "ftl1rom1.10", 0x0800000, 0x800000, CRC(a1ec7d08) SHA1(e693a0b16235c44d4165b2f53dc25d5288297c27) )
	ROM_LOAD(        "ftl1rom2.11", 0x1000000, 0x800000, CRC(204be858) SHA1(95b22b678b7d11b0ec907698c18cebd84437c656) )
	ROM_LOAD16_BYTE( "ftl1fl3l.7",  0x1800000, 0x200000, CRC(e4fb01e1) SHA1(c7883e86afb58412281a317bfdf62a21488421be) )
	ROM_LOAD16_BYTE( "ftl1fl3h.8",  0x1800001, 0x200000, CRC(67a5c56f) SHA1(6f0a4f93b4975b24efb26c3dd47b791c92c55fbf) )
	ROM_LOAD16_BYTE( "ftl1fl4l.5",  0x1c00000, 0x200000, CRC(8039242c) SHA1(9c3f9637d7cd0c004c0c39aee815eed228ebad20) )
	ROM_LOAD16_BYTE( "ftl1fl4h.6",  0x1c00001, 0x200000, CRC(5ad59726) SHA1(b3a68f7ba2052b99407a5423223202001f2a4f67) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ftl1vera.11s", 0x0000000, 0x080000, CRC(e3f957cd) SHA1(1c7f2033025fb9c40654cff26d78697baf697c59) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ftl1wave0.2",  0x0000000, 0x800000, CRC(ee009a2b) SHA1(c332bf59917b2673d7acb864bf92d25d74a350b6) )
	ROM_LOAD( "ftl1wave1.1",  0x0800000, 0x800000, CRC(a54a89cd) SHA1(543b47c6442f7a78e26992b041235a91d719bb89) )
ROM_END

ROM_START( fgtlayerja )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ftl0vera.2e", 0x0000000, 0x200000, CRC(f4156e79) SHA1(cedb917940be8c74fa4ddb48213ce6917444e306) )
	ROM_LOAD16_BYTE( "ftl0vera.2j", 0x0000001, 0x200000, CRC(c65b57c0) SHA1(0051aa46d09fbe9d896ae5f534e21955373f1d46) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "ftl1rom0.9",  0x0000000, 0x800000, CRC(e33ce365) SHA1(f7e5b98d2e8e5f233265909477c84331f016ebfb) )
	ROM_LOAD(        "ftl1rom1.10", 0x0800000, 0x800000, CRC(a1ec7d08) SHA1(e693a0b16235c44d4165b2f53dc25d5288297c27) )
	ROM_LOAD(        "ftl1rom2.11", 0x1000000, 0x800000, CRC(204be858) SHA1(95b22b678b7d11b0ec907698c18cebd84437c656) )
	ROM_LOAD16_BYTE( "ftl1fl3l.7",  0x1800000, 0x200000, CRC(e4fb01e1) SHA1(c7883e86afb58412281a317bfdf62a21488421be) )
	ROM_LOAD16_BYTE( "ftl1fl3h.8",  0x1800001, 0x200000, CRC(67a5c56f) SHA1(6f0a4f93b4975b24efb26c3dd47b791c92c55fbf) )
	ROM_LOAD16_BYTE( "ftl1fl4l.5",  0x1c00000, 0x200000, CRC(8039242c) SHA1(9c3f9637d7cd0c004c0c39aee815eed228ebad20) )
	ROM_LOAD16_BYTE( "ftl1fl4h.6",  0x1c00001, 0x200000, CRC(5ad59726) SHA1(b3a68f7ba2052b99407a5423223202001f2a4f67) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ftl1vera.11s", 0x0000000, 0x080000, CRC(e3f957cd) SHA1(1c7f2033025fb9c40654cff26d78697baf697c59) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ftl1wave0.2",  0x0000000, 0x800000, CRC(ee009a2b) SHA1(c332bf59917b2673d7acb864bf92d25d74a350b6) )
	ROM_LOAD( "ftl1wave1.1",  0x0800000, 0x800000, CRC(a54a89cd) SHA1(543b47c6442f7a78e26992b041235a91d719bb89) )
ROM_END

ROM_START( golgo13 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "glg1vera.2l",       0x0000000, 0x200000, CRC(aa15abfe) SHA1(e82b408746e01c50c5cb0dcef804974d1e97078a) )
	ROM_LOAD16_BYTE( "glg1vera.2p",       0x0000001, 0x200000, CRC(37a4cf90) SHA1(b5470d44036e9de8220b669f71b50bcec42d9a18) )

	ROM_REGION32_LE( 0x3c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "glg1rom0l.10",      0x0000000, 0x800000, CRC(2837524e) SHA1(81d811c2cf8121854ed6046dc421cda7fcd44014) )
	ROM_LOAD16_BYTE( "glg1rom0u.14",      0x0000001, 0x800000, CRC(4482deff) SHA1(03dcd12633b167d9af664ae826fc2c2ff096c0a6) )
	ROM_LOAD16_BYTE( "glg1rom1l.11",      0x1000000, 0x800000, CRC(080f3550) SHA1(40a5257baeb366e32ff51f571efb12a556f93430) )
	ROM_LOAD16_BYTE( "glg1rom1u.15",      0x1000001, 0x800000, CRC(b49e8613) SHA1(d25ede8046ce309d5a5515a2e44b6773fdd56333) )
	ROM_LOAD16_BYTE( "glg1rom2l.12",      0x2000000, 0x800000, CRC(e1a79a87) SHA1(c3f31b1bc6a3b51361df0d89d3ff0717875fd800) )
	ROM_LOAD16_BYTE( "glg1rom2u.16",      0x2000001, 0x800000, CRC(cda22852) SHA1(e609a55e0ec91b532469c3607dcc96456eec6a07) )
	ROM_LOAD16_BYTE( "glg1fl3l.4",        0x3000000, 0x200000, CRC(16ddc2cb) SHA1(71460bd4ad35b3488dc1326fe2f600df40988e06) )
	ROM_LOAD16_BYTE( "glg1fl3u.5",        0x3000001, 0x200000, CRC(d90d72d1) SHA1(0027389344862486c53cd0e1554b36eed65aa5b0) )
	ROM_LOAD16_BYTE( "glg1fl4l.6",        0x3400000, 0x200000, CRC(820d8133) SHA1(2d780612055a0fa5de756cfa46ce7f134139e550) )
	ROM_LOAD16_BYTE( "glg1fl4u.7",        0x3400001, 0x200000, CRC(02d78160) SHA1(2eafcbd3ba550acf7816074e6c7eee65f7d64859) )
	ROM_LOAD16_BYTE( "glg1fl5l.8",        0x3800000, 0x200000, CRC(090b2508) SHA1(c3587aeab71f6a9dcc90bf2af496303e9d20eac8) )
	ROM_LOAD16_BYTE( "glg1fl5u.9",        0x3800001, 0x200000, CRC(2231a07c) SHA1(480e17219101a0dae5cd64507e31cd7e711c95fa) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "glg1vera.11s", 0x0000000, 0x080000, CRC(5c33f240) SHA1(ec8fc8d83466b28dfa35b93e16d8164883513b19) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "glg1wave0.1",              0x0000000, 0x800000, CRC(672d4f7c) SHA1(16a7564a2c68840a438a33ac2381df4e70e1bb45) )
	ROM_LOAD( "glg1wave1.13",             0x0800000, 0x800000, CRC(480b0a1a) SHA1(341d5ec8ad0f3c0a121eeeec9466aaeec2bd1c74) )
ROM_END

ROM_START( mdhorse )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "mdh1vera.2l",       0x0000000, 0x200000, CRC(fbb567b2) SHA1(899dccdfbc8dcbcdaf9b5df93e249a36f8cbf999) )
	ROM_LOAD16_BYTE( "mdh1vera.2p",       0x0000001, 0x200000, CRC(a0f182ab) SHA1(70c789ea88248c1f810f9fdb3feaf808acbaa8cd) )

	ROM_REGION32_LE( 0x3000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "mdh1rom0l",         0x0000000, 0x800000, CRC(ca5bf806) SHA1(8c88b1f29c96a0696ac9428c411cde10faacce35) )
	ROM_LOAD16_BYTE( "mdh1rom0u",         0x0000001, 0x800000, CRC(315e9539) SHA1(340fcd196f53f64b3f56ef73101eddc9e142d907) )
	ROM_LOAD16_BYTE( "mdh1rom1l",         0x1000000, 0x800000, CRC(9f610211) SHA1(8459733c52d1c62033a4aeb9985b4a6e863a62d0) )
	ROM_LOAD16_BYTE( "mdh1rom1u",         0x1000001, 0x800000, CRC(a2e43560) SHA1(b0657a22701c6f2098f210d59e4c9bc88593a750) )
	ROM_LOAD16_BYTE( "mdh1rom2l",         0x2000000, 0x800000, CRC(84840fa9) SHA1(0f051db55b30f7dd473f4df9bb5e7d38c39dc785) )
	ROM_LOAD16_BYTE( "mdh1rom2u",         0x2000001, 0x800000, CRC(9490dafe) SHA1(c1bd9343535876eac5369a86105013b4a7d731b3) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "mdh1vera.11s", 0x0000000, 0x080000, CRC(20d7ba29) SHA1(95a056d1f1ac70dda8ced832b506076485348a33) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "mdh1wave0",                0x0000000, 0x800000, CRC(7b031123) SHA1(7cbc1f71d259405f9f1ef26026d51abcb255b057) )
ROM_END

ROM_START( mrdrillr )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dri3vera2.2l",      0x0000000, 0x200000, CRC(36b9eeab) SHA1(85f5d57d4fc6160bfed49a08ad31a2cc2b4a38c5) )
	ROM_LOAD16_BYTE( "dri3vera2.2p",      0x0000001, 0x200000, CRC(811c00d5) SHA1(627685078fceb023e8af1b7af5269f9b01a06dba) )

	ROM_REGION32_LE( 0x0800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dri1rom0l.6",       0x0000000, 0x400000, CRC(021bb2fa) SHA1(bfe3e46e9728d5b5a692f432515267ff8b8255e7) )
	ROM_LOAD16_BYTE( "dri1rom0u.9",       0x0000001, 0x400000, CRC(5aae85ea) SHA1(a54dcc050c12ed3d77efc328e366e99c392eb139) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "dri1vera.11s", 0x0000000, 0x080000, CRC(33ea9c0e) SHA1(5018d7a1a45ec3133cd928435db8804f66321924) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dri1wave0.5",              0x0000000, 0x800000, CRC(32928df1) SHA1(79af92a2d24a0e3d5bfe1785776b0f86a93882ce) )
ROM_END

ROM_START( mrdrillrja2 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dri1vera2.2l",      0x0000000, 0x200000, CRC(751ca21d) SHA1(1c271bba83d387c797ce8daa43885bcb6e1a51a6) )
	ROM_LOAD16_BYTE( "dri1vera2.2p",      0x0000001, 0x200000, CRC(2a2b0704) SHA1(5a8b40c6cf0adc43ca2ee0c576ec82f314aacd2c) )

	ROM_REGION32_LE( 0x0800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dri1rom0l.6",       0x0000000, 0x400000, CRC(021bb2fa) SHA1(bfe3e46e9728d5b5a692f432515267ff8b8255e7) )
	ROM_LOAD16_BYTE( "dri1rom0u.9",       0x0000001, 0x400000, CRC(5aae85ea) SHA1(a54dcc050c12ed3d77efc328e366e99c392eb139) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "dri1vera.11s", 0x0000000, 0x080000, CRC(33ea9c0e) SHA1(5018d7a1a45ec3133cd928435db8804f66321924) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dri1wave0.5",              0x0000000, 0x800000, CRC(32928df1) SHA1(79af92a2d24a0e3d5bfe1785776b0f86a93882ce) )
ROM_END

ROM_START( pacapp )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ppp1vera.2l",       0x0000000, 0x200000, CRC(6e74bd05) SHA1(41a2e06538cea3bced2992f5858a3f0cd1c0b4aa) )
	ROM_LOAD16_BYTE( "ppp1vera.2p",       0x0000001, 0x200000, CRC(b7a2f724) SHA1(820ae04ec416b8394a1d919279748bde3460cb96) )

	ROM_REGION32_LE( 0x1800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "ppp1rom0l.6",       0x0000000, 0x400000, CRC(b152fdd8) SHA1(23c5c07680a62e941a7b1a28897f986dd9399801) )
	ROM_LOAD16_BYTE( "ppp1rom0u.9",       0x0000001, 0x400000, CRC(c615c26e) SHA1(db1aae37ebee2a74636415e4b1b0b17790a6a67e) )
	ROM_LOAD16_BYTE( "ppp1rom1l.7",       0x0800000, 0x400000, CRC(46eaedbd) SHA1(afe3c966fcf083d89b45d44d871bed1b8caa3014) )
	ROM_LOAD16_BYTE( "ppp1rom1u.10",      0x0800001, 0x400000, CRC(32f27dce) SHA1(06de870b83c972403d96b8b9a8ee5a192a99451d) )
	ROM_LOAD16_BYTE( "ppp1rom2l.8",       0x1000000, 0x400000, CRC(dca7e5ed) SHA1(b43b6e086912009295ceae11d2d540733353d7b6) )
	ROM_LOAD16_BYTE( "ppp1rom2u.11",      0x1000001, 0x400000, CRC(cada7a0d) SHA1(e8d927a4680911b77de1d906b5e0140697e9c67b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ppp1vera.11s", 0x0000000, 0x080000, CRC(22242317) SHA1(e46bf3c594136168faedebbd59f53ff9a6ecf3c1) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ppp1wave0.5",              0x0000000, 0x800000, CRC(184ccc7d) SHA1(b74638cebef209638c625aac7e8c5b924f56a8bb) )
	ROM_LOAD( "ppp1wave1.4",              0x0800000, 0x800000, CRC(cbcf74c5) SHA1(a089277c9befc87b5bbe0d4e5b8187a4ad5ef143) )
ROM_END

ROM_START( soulclbr )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc12vera.2l",      0x0000000, 0x200000, CRC(ceadcc9a) SHA1(68cd3d15f28178fcea8aada6067b7060edfb2913) )
	ROM_LOAD16_BYTE( "soc12vera.2p",      0x0000001, 0x200000, CRC(65a74cf0) SHA1(1129c078925bd02f4e9cef64cd7761968be53537) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbrja2 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc1vera.2l",       0x0000000, 0x200000, CRC(37e0a203) SHA1(3915b5e530c8e70a07aa8ccedeb66633ae5f670e) )
	ROM_LOAD16_BYTE( "soc1vera.2p",       0x0000001, 0x200000, CRC(7cd87a35) SHA1(5a4837b6f6a49c88126a0ddbb8059a4da77127bc) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbrab )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc14verb.2l",      0x0000000, 0x200000, CRC(6af5c5f6) SHA1(51d1e7d78d95cfc765cd219ed07b405cd920044b) )
	ROM_LOAD16_BYTE( "soc14verb.2p",      0x0000001, 0x200000, CRC(23e7a4c4) SHA1(a97f36cafdeff9e26fbd24e54ab8ac8080763761) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbrac )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc14verc.2e",      0x0000000, 0x200000, CRC(c40e9614) SHA1(dc20469f0d657423e472fdf5897852ab9fb8bb73) )
	ROM_LOAD16_BYTE( "soc14verc.2j",      0x0000001, 0x200000, CRC(80c41446) SHA1(e5620a4f0ffba913169a779df73384b7ca8780b9) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbrub )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc13verb.2e",      0x0000000, 0x200000, CRC(ad7cfb1e) SHA1(7d1e7fd0024e31780335690906846e91ba063003) )
	ROM_LOAD16_BYTE( "soc13verb.2j",      0x0000001, 0x200000, CRC(7449c045) SHA1(1c7a8b659d0f12dded2a00bc83baeb392fd7a719) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbruc )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc13verc.2l",      0x0000000, 0x200000, CRC(4ba962fb) SHA1(e2b5e543d92a4157788482f3ab7c6b0e5ff30367) )
	ROM_LOAD16_BYTE( "soc13verc.2p",      0x0000001, 0x200000, CRC(140c40de) SHA1(352faec0fff5a8422ee7c8db2e0c946b139be03f) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbrjb )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc11verb.2e",      0x0000000, 0x200000, CRC(9660d996) SHA1(6361abfd8b0d29848aabad6a5c517ba0d336359a) )
	ROM_LOAD16_BYTE( "soc11verb.2j",      0x0000001, 0x200000, CRC(49939880) SHA1(a53fb8ecd71c8d59b0e08d6233ea658ae083bc6d) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( soulclbrjc )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "soc11verc.2l",      0x0000000, 0x200000, CRC(f5e3679c) SHA1(b426cfc7707a6772e6aabbaf4a19b7f008324d55) )
	ROM_LOAD16_BYTE( "soc11verc.2p",      0x0000001, 0x200000, CRC(7537719c) SHA1(d83d4c762fa7fcfd5d84de550568e92999e5bdfb) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "soc1rom0.7",        0x0000000, 0x800000, CRC(cdc47b55) SHA1(315ea6b819de5c4883aa400f1b9f4172637757bf) )
	ROM_LOAD(        "soc1rom1.8",        0x0800000, 0x800000, CRC(30d2dd5a) SHA1(1c0c467ba339e0241efb8d5c3b025a046b2ca676) )
	ROM_LOAD(        "soc1rom2.9",        0x1000000, 0x800000, CRC(dbb93955) SHA1(624cd8ad94e8ae53206f798bff81784afe95e5f1) )
	ROM_LOAD(        "soc1fl3.6",         0x1800000, 0x400000, CRC(24d94c38) SHA1(0f9b9ab11dd4e02086d7b9104ce2f5d4e93cd696) )
	ROM_LOAD(        "soc1fl4.5",         0x1c00000, 0x400000, CRC(6212090e) SHA1(ed5e50771180935a0c2e760e7369673098722201) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "soc1vera.11s", 0x0000000, 0x080000, CRC(52aa206a) SHA1(5abe9d6f800fa1b9623aa08b16e9b959b840e50b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "soc1wave0.2",              0x0000000, 0x800000, CRC(c100618d) SHA1(b87f88ee42ad9c5affa674e5f816d902143fed99) )
ROM_END

ROM_START( sws99 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ss91vera.2e",       0x0000000, 0x200000, CRC(4dd928d7) SHA1(d76c0f52d1a2cd101a6879e6ff57ed1c52b5e228) )
	ROM_LOAD16_BYTE( "ss91vera.2j",       0x0000001, 0x200000, CRC(40777a48) SHA1(6e3052ddbe3943eb2418cd50102cead88b850240) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "ss91rom0.9",        0x0000000, 0x800000, CRC(db5bc50d) SHA1(b8b59f6db3a374277871c39b3657cb193525e558) )
	ROM_LOAD(        "ss91rom1.10",       0x0800000, 0x800000, CRC(4d71f29f) SHA1(3ccc8410d383bfd9fde44574ebb9c24a235cc734) )
	ROM_FILL(                             0x1000000, 0x800000, 0x00 )
	ROM_LOAD16_BYTE( "ss91fl3l.7",        0x1800000, 0x200000, CRC(61efd65b) SHA1(1e97d5cd51bf778995c3e568ac7e1d3514264d48) )
	ROM_LOAD16_BYTE( "ss91fl3h.8",        0x1800001, 0x200000, CRC(7f3c8c54) SHA1(b473a503cc5d532922780139ab3cab974d3df65b) )
	ROM_LOAD16_BYTE( "ss91fl4l.5",        0x1c00000, 0x200000, CRC(a6af9511) SHA1(63f14f4fc2b210348e41cdfa552a7a7c86fb1b99) )
	ROM_LOAD16_BYTE( "ss91fl4h.6",        0x1c00001, 0x200000, CRC(be3730a4) SHA1(48802229d31c1eef7f4173eb05060a328b702336) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ss91vera.11s", 0x0000000, 0x080000, CRC(c6bc5c31) SHA1(c6ef46c3fa8a7749618126d360e614ea6c8d9c54) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ss91wave0.2",              0x0000000, 0x800000, CRC(1c5e2ff1) SHA1(4f29dfd49f6b5c3ca3b758823d368051354bd673) )
	ROM_LOAD( "ss91wave1.1",              0x0800000, 0x800000, CRC(5f4c8861) SHA1(baee7182c32bc064d805de5a16948faf78941ac4) )
ROM_END

ROM_START( tekken3 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet2vere1.2e", 0x0000000, 0x200000, CRC(7ded5461) SHA1(3a5638c6ad40bfde6e12fdfd6d469f6ea5e9f4fb) )
	ROM_LOAD16_BYTE( "tet2vere1.2j", 0x0000001, 0x200000, CRC(25c96e1e) SHA1(554d1e42886d6a6c9c5857e9cbd5d7c37d7a6e67) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",  0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",  0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",  0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10", 0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",  0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11", 0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",  0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",  0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1vere.11s", 0x0000000, 0x080000, CRC(c92b98d1) SHA1(8ae6fba8c5b6b9a2ab9541eac8553b282f35750d) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",  0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",  0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3je1 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet1vere1.2e", 0x0000000, 0x200000, CRC(8b01113b) SHA1(45fdfd58293641ed16bc59c633a85a9cf64ccbaf) )
	ROM_LOAD16_BYTE( "tet1vere1.2j", 0x0000001, 0x200000, CRC(df4c96fb) SHA1(2e223045bf5b80ccf615106e869760c5b7aa8d44) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",  0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",  0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",  0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10", 0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",  0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11", 0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",  0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",  0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1vere.11s", 0x0000000, 0x080000, CRC(c92b98d1) SHA1(8ae6fba8c5b6b9a2ab9541eac8553b282f35750d) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",  0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",  0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tektagt )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg2verc1.2e", 0x0000000, 0x200000, CRC(c6da0717) SHA1(9e01ae64710d85eb9899d6fa6fd0a2152aee8c11) )
	ROM_LOAD16_BYTE( "teg2verc1.2j", 0x0000001, 0x200000, CRC(25a1d2ff) SHA1(529a11a1bbb8655534d7ec371f1c09e9e387ed11) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg1_rom0e.ic9",  0x0000000, 0x800000, CRC(c962a373) SHA1(d662dbd89ef62c5ac3150a018fc2d35ef2ee94ac) )
	ROM_LOAD32_WORD( "teg1_rom0o.ic13", 0x0000002, 0x800000, CRC(badb7dcf) SHA1(8c0bf7f6351c5a2a0996df371a901cf90c68cd8c) )
	ROM_LOAD32_WORD( "teg1_rom1e.ic10", 0x1000000, 0x800000, CRC(b3d56124) SHA1(4df20c74ba63f7362caf15e9b8949fab655704fb) )
	ROM_LOAD32_WORD( "teg1_rom1o.ic14", 0x1000002, 0x800000, CRC(2434ceb6) SHA1(f19f1599acbd6fd48793a2ee5a500ca817d9df56) )
	ROM_LOAD32_WORD( "teg1_rom2e.ic11", 0x2000000, 0x800000, CRC(6e5c3428) SHA1(e3cdb60a4445406877b2e273385f34bfb0974220) )
	ROM_LOAD32_WORD( "teg1_rom2o.ic15", 0x2000002, 0x800000, CRC(21ce9dfa) SHA1(f27e8210ee236c327aa3e1ce4dd408abc6580a1b) )
	ROM_LOAD32_BYTE( "flel.ic4",        0x3000000, 0x200000, CRC(88b3823c) SHA1(6f31acb642c57daccbfdb87b790037e261c8c73c) )
	ROM_LOAD32_BYTE( "fleu.ic5",        0x3000001, 0x200000, CRC(36df0867) SHA1(6bec8560ad4c122dc909daa83aa9089ba5b281f7) )
	ROM_LOAD32_BYTE( "flol.ic6",        0x3000002, 0x200000, CRC(03a76765) SHA1(ae35ae28375f2a3e52d72b77ec09750c326cc269) )
	ROM_LOAD32_BYTE( "flou.ic7",        0x3000003, 0x200000, CRC(6d6947d1) SHA1(2f307bc4070fadb510c0473bc91d917b2d845ca5) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s", 0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg1_wave0.ic1",  0x0000000, 0x800000, CRC(4bd99104) SHA1(f76b0576cc28fe49d3c1c402988b933933e52e15) )
	ROM_LOAD( "teg1_wave1.ic12", 0x0800000, 0x800000, CRC(dbc74fff) SHA1(601b7e7361ea744b34e3fa1fc39d88641de7f4c6) )
ROM_END

ROM_START( tektagtub )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg3verb.2l", 0x0000000, 0x200000, CRC(97df2855) SHA1(c1b61df8e79348424f4bd2660ab5179ef21bdb07) )
	ROM_LOAD16_BYTE( "teg3verb.2p", 0x0000001, 0x200000, CRC(1dbe7591) SHA1(af464caa03fdd12024ad482e9c853a36510bfba7) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg1_rom0e.ic9",  0x0000000, 0x800000, CRC(c962a373) SHA1(d662dbd89ef62c5ac3150a018fc2d35ef2ee94ac) )
	ROM_LOAD32_WORD( "teg1_rom0o.ic13", 0x0000002, 0x800000, CRC(badb7dcf) SHA1(8c0bf7f6351c5a2a0996df371a901cf90c68cd8c) )
	ROM_LOAD32_WORD( "teg1_rom1e.ic10", 0x1000000, 0x800000, CRC(b3d56124) SHA1(4df20c74ba63f7362caf15e9b8949fab655704fb) )
	ROM_LOAD32_WORD( "teg1_rom1o.ic14", 0x1000002, 0x800000, CRC(2434ceb6) SHA1(f19f1599acbd6fd48793a2ee5a500ca817d9df56) )
	ROM_LOAD32_WORD( "teg1_rom2e.ic11", 0x2000000, 0x800000, CRC(6e5c3428) SHA1(e3cdb60a4445406877b2e273385f34bfb0974220) )
	ROM_LOAD32_WORD( "teg1_rom2o.ic15", 0x2000002, 0x800000, CRC(21ce9dfa) SHA1(f27e8210ee236c327aa3e1ce4dd408abc6580a1b) )
	ROM_LOAD32_BYTE( "flel.ic4",        0x3000000, 0x200000, CRC(88b3823c) SHA1(6f31acb642c57daccbfdb87b790037e261c8c73c) )
	ROM_LOAD32_BYTE( "fleu.ic5",        0x3000001, 0x200000, CRC(36df0867) SHA1(6bec8560ad4c122dc909daa83aa9089ba5b281f7) )
	ROM_LOAD32_BYTE( "flol.ic6",        0x3000002, 0x200000, CRC(03a76765) SHA1(ae35ae28375f2a3e52d72b77ec09750c326cc269) )
	ROM_LOAD32_BYTE( "flou.ic7",        0x3000003, 0x200000, CRC(6d6947d1) SHA1(2f307bc4070fadb510c0473bc91d917b2d845ca5) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s", 0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg1_wave0.ic1",  0x0000000, 0x800000, CRC(4bd99104) SHA1(f76b0576cc28fe49d3c1c402988b933933e52e15) )
	ROM_LOAD( "teg1_wave1.ic12", 0x0800000, 0x800000, CRC(dbc74fff) SHA1(601b7e7361ea744b34e3fa1fc39d88641de7f4c6) )
ROM_END

ROM_START( tektagtuc1 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg3verc1.2l", 0x0000000, 0x200000, CRC(1efb7b85) SHA1(0623bb6571caf046ff7b4f83f11ee84a92c4b462) )
	ROM_LOAD16_BYTE( "teg3verc1.2p", 0x0000001, 0x200000, CRC(7caef9b2) SHA1(5c56d69ba2f723d0a4fbe4902196efc6ba9d5094) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg1_rom0e.ic9",  0x0000000, 0x800000, CRC(c962a373) SHA1(d662dbd89ef62c5ac3150a018fc2d35ef2ee94ac) )
	ROM_LOAD32_WORD( "teg1_rom0o.ic13", 0x0000002, 0x800000, CRC(badb7dcf) SHA1(8c0bf7f6351c5a2a0996df371a901cf90c68cd8c) )
	ROM_LOAD32_WORD( "teg1_rom1e.ic10", 0x1000000, 0x800000, CRC(b3d56124) SHA1(4df20c74ba63f7362caf15e9b8949fab655704fb) )
	ROM_LOAD32_WORD( "teg1_rom1o.ic14", 0x1000002, 0x800000, CRC(2434ceb6) SHA1(f19f1599acbd6fd48793a2ee5a500ca817d9df56) )
	ROM_LOAD32_WORD( "teg1_rom2e.ic11", 0x2000000, 0x800000, CRC(6e5c3428) SHA1(e3cdb60a4445406877b2e273385f34bfb0974220) )
	ROM_LOAD32_WORD( "teg1_rom2o.ic15", 0x2000002, 0x800000, CRC(21ce9dfa) SHA1(f27e8210ee236c327aa3e1ce4dd408abc6580a1b) )
	ROM_LOAD32_BYTE( "flel.ic4",        0x3000000, 0x200000, CRC(88b3823c) SHA1(6f31acb642c57daccbfdb87b790037e261c8c73c) )
	ROM_LOAD32_BYTE( "fleu.ic5",        0x3000001, 0x200000, CRC(36df0867) SHA1(6bec8560ad4c122dc909daa83aa9089ba5b281f7) )
	ROM_LOAD32_BYTE( "flol.ic6",        0x3000002, 0x200000, CRC(03a76765) SHA1(ae35ae28375f2a3e52d72b77ec09750c326cc269) )
	ROM_LOAD32_BYTE( "flou.ic7",        0x3000003, 0x200000, CRC(6d6947d1) SHA1(2f307bc4070fadb510c0473bc91d917b2d845ca5) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s", 0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg1_wave0.ic1",  0x0000000, 0x800000, CRC(4bd99104) SHA1(f76b0576cc28fe49d3c1c402988b933933e52e15) )
	ROM_LOAD( "teg1_wave1.ic12", 0x0800000, 0x800000, CRC(dbc74fff) SHA1(601b7e7361ea744b34e3fa1fc39d88641de7f4c6) )
ROM_END

ROM_START( tektagtjc1 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x00400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg1verc1.2e", 0x0000000, 0x200000, CRC(adbdfc2e) SHA1(e18458efb0f9b341970132b512d2e5104a942c6e) )
	ROM_LOAD16_BYTE( "teg1verc1.2j", 0x0000001, 0x200000, CRC(2fa33418) SHA1(e15f4c8cc4309ebb90d8d015b1f17f75ce5c8879) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg1_rom0e.ic9",  0x0000000, 0x800000, CRC(c962a373) SHA1(d662dbd89ef62c5ac3150a018fc2d35ef2ee94ac) )
	ROM_LOAD32_WORD( "teg1_rom0o.ic13", 0x0000002, 0x800000, CRC(badb7dcf) SHA1(8c0bf7f6351c5a2a0996df371a901cf90c68cd8c) )
	ROM_LOAD32_WORD( "teg1_rom1e.ic10", 0x1000000, 0x800000, CRC(b3d56124) SHA1(4df20c74ba63f7362caf15e9b8949fab655704fb) )
	ROM_LOAD32_WORD( "teg1_rom1o.ic14", 0x1000002, 0x800000, CRC(2434ceb6) SHA1(f19f1599acbd6fd48793a2ee5a500ca817d9df56) )
	ROM_LOAD32_WORD( "teg1_rom2e.ic11", 0x2000000, 0x800000, CRC(6e5c3428) SHA1(e3cdb60a4445406877b2e273385f34bfb0974220) )
	ROM_LOAD32_WORD( "teg1_rom2o.ic15", 0x2000002, 0x800000, CRC(21ce9dfa) SHA1(f27e8210ee236c327aa3e1ce4dd408abc6580a1b) )
	ROM_LOAD32_BYTE( "flel.ic4",        0x3000000, 0x200000, CRC(88b3823c) SHA1(6f31acb642c57daccbfdb87b790037e261c8c73c) )
	ROM_LOAD32_BYTE( "fleu.ic5",        0x3000001, 0x200000, CRC(36df0867) SHA1(6bec8560ad4c122dc909daa83aa9089ba5b281f7) )
	ROM_LOAD32_BYTE( "flol.ic6",        0x3000002, 0x200000, CRC(03a76765) SHA1(ae35ae28375f2a3e52d72b77ec09750c326cc269) )
	ROM_LOAD32_BYTE( "flou.ic7",        0x3000003, 0x200000, CRC(6d6947d1) SHA1(2f307bc4070fadb510c0473bc91d917b2d845ca5) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s", 0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg1_wave0.ic1",  0x0000000, 0x800000, CRC(4bd99104) SHA1(f76b0576cc28fe49d3c1c402988b933933e52e15) )
	ROM_LOAD( "teg1_wave1.ic12", 0x0800000, 0x800000, CRC(dbc74fff) SHA1(601b7e7361ea744b34e3fa1fc39d88641de7f4c6) )
ROM_END

GAMEX( 1997, tekken3,   0,        coh700s, tekken3, namcos12_h8, ROT0, "Namco",         "Tekken 3 (World, TET2/VER.E1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3je1, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco",         "Tekken 3 (Japan, TET1/VER.E1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbr,   0,        coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (World, SOC12/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbrab, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (Asia, SOC14/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbrac, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (Asia, SOC14/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbrub, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (US, SOC13/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbruc, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (US, SOC13/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbrja2, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",        "Soul Calibur (Japan, SOC11/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbrjb, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (Japan, SOC11/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, soulclbrjc, soulclbr, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Soul Calibur (Japan, SOC11/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, ehrgeiz,    0,        coh700s, namcos12_h8, namcos12_h8, ROT0, "Square/Namco",  "Ehrgeiz (World, EG2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, ehrgeizua,  ehrgeiz,  coh700s, namcos12_h8, namcos12_h8, ROT0, "Square/Namco",  "Ehrgeiz (US, EG3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, ehrgeizja,  ehrgeiz,  coh700s, namcos12_h8, namcos12_h8, ROT0, "Square/Namco",  "Ehrgeiz (Japan, EG1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, mdhorse,    0,        coh700, namcos12, namcos12, ROT0, "MOSS/Namco",    "Derby Quiz My Dream Horse (Japan, MDH1/VER.A2)", GAME_NOT_WORKING | GAME_NO_SOUND )
GAMEX( 1998, fgtlayer,  0,        coh700s, namcos12_h8, namcos12_h8b, ROT0, "Arika/Namco",   "Fighting Layer (Asia, FTL3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, fgtlayerja, fgtlayer, coh700s, namcos12_h8, namcos12_h8b, ROT0, "Arika/Namco",   "Fighting Layer (Japan, FTL0/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, pacapp,     0,        coh700, namcos12, namcos12, ROT0, "Produce/Namco", "Paca Paca Passion (Japan, PPP1/VER.A2)", GAME_NOT_WORKING | GAME_NO_SOUND )
GAMEX( 1999, sws99,      0,        coh700, namcos12, namcos12, ROT0, "Namco",         "Super World Stadium '99 (Japan, SS91/VER.A3)", GAME_NOT_WORKING | GAME_NO_SOUND )
GAMEX( 1999, tektagt,    0,       tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (World, TEG2/VER.C1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtub,  tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (US, TEG3/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtuc1, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (US, TEG3/VER.C1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtjc1, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (Japan, TEG1/VER.C1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, mrdrillr,   0,        coh700s, mrdrillr, namcos12_h8b, ROT0, "Namco",         "Mr. Driller (US, DRI3/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, mrdrillrja2, mrdrillr, coh700s, mrdrillr, namcos12_h8b, ROT0, "Namco",        "Mr. Driller (Japan, DRI1/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, aquarush,   0,        coh700, namcos12, namcos12, ROT0, "Namco",         "Aqua Rush (Japan, AQ1/VER.A1)", GAME_IMPERFECT_GRAPHICS | GAME_NO_SOUND )
GAMEX( 1999, golgo13,    0,        coh700, namcos12, namcos12, ROT0, "Eighting/Raizing/Namco", "Golgo 13 (Japan, GLG1/VER.A)", GAME_NOT_WORKING | GAME_NO_SOUND )
