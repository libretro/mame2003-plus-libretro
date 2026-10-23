/***************************************************************************

  Namco System 12 - Arcade PSX Hardware
  =====================================
  Driver by smf
  H8/3002 and Golgo13 support by R. Belmont based on work by The_Author and DynaChicken
  H8/3002 + C352 sound backported to mame2003-plus (coh700s)

  Issues:
    on coh700 the sound cpu is only simulated enough for inputs to work, credit handling is not perfect.
    coh700s runs the real H8/3002 sound/I-O program and the C352 (sound, inputs, coins, RTC).
    graphics are glitchy in some games.

  Not here yet (current MAME): the games on the Namco JVS I/O boards
  (aplarail, technodr), on the CD-XA board (truckk, ujlnow, questff) and
  on the COH-716 CPU board (sws2001).

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
static int s12_has_adc = 0;
static data16_t s12_adc( int n_channel );
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
	case H8_ADC_0_H:
	case H8_ADC_1_H:
	case H8_ADC_2_H:
	case H8_ADC_3_H:
		return s12_adc( ( port - H8_ADC_0_H ) / 2 ) >> 2;
	case H8_ADC_0_L:
	case H8_ADC_1_L:
	case H8_ADC_2_L:
	case H8_ADC_3_L:
		return ( s12_adc( ( port - H8_ADC_0_L ) / 2 ) & 3 ) << 6;
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

/***************************************************************************

  Light guns (current MAME)

  Point Blank 2, Gunbarl, Ghoul Panic, Oh! Bakyuuun (ptblank2_state): the
  System 11 gun board, read at 0x1f780000 (X, Y of each gun, one 16-bit
  value per 32-bit word); lamps and recoil at 0x1f788000.
  Golgo 13 (golgo13_state): the gun goes to the H8/3002 ADC, X on channel 1
  and Y on channel 2.

***************************************************************************/

#define S12_PORT_GUN	3	/* the gun ports follow DSW, IN0, IN1 */

static data16_t s12_gun_r( int n_gun )
{
	data16_t data = readinputport( S12_PORT_GUN + n_gun );

	if( ( data & 3 ) == 3 )
	{
		data++; /* yend */
	}
	return data;
}

static READ32_HANDLER( ptblank2_gun_r )
{
	data16_t data = s12_gun_r( offset & 3 );

	return data | ( data << 16 );
}

/* gun port ranges, for the crosshairs (PORT_CROSSHAIR( X/Y, 1.0, 0.0, 0 )) */
static int s12_gun_min[ 2 ], s12_gun_max[ 2 ];
static int s12_gun_reverse_y;
static int s12_n_guns;

static VIDEO_UPDATE( namcos12_gun )
{
	const struct rectangle *visible = &Machine->visible_area;
	int n_gun;

	video_update_psx( bitmap, cliprect );

	for( n_gun = 0; n_gun < s12_n_guns; n_gun++ )
	{
		int n_x = readinputport( S12_PORT_GUN + n_gun * 2 ) - s12_gun_min[ 0 ];
		int n_y = readinputport( S12_PORT_GUN + n_gun * 2 + 1 ) - s12_gun_min[ 1 ];
		n_x = visible->min_x + n_x * ( visible->max_x - visible->min_x ) / ( s12_gun_max[ 0 ] - s12_gun_min[ 0 ] );
		n_y = visible->min_y + n_y * ( visible->max_y - visible->min_y ) / ( s12_gun_max[ 1 ] - s12_gun_min[ 1 ] );
		if( s12_gun_reverse_y )
		{
			n_y = visible->max_y - ( n_y - visible->min_y );
		}
		draw_crosshair( n_gun + 1, bitmap, n_x, n_y, cliprect );
	}
}

/* the H8/3002 ADC: the result is 10 bits, left justified in the data register.
   Golgo 13: the gun on channels 1 (X) and 2 (Y); Kart Duel: brake, gas and
   steering wheel on channels 0, 1 and 2 */
static data16_t s12_adc( int n_channel )
{
	switch( s12_has_adc )
	{
	case 1:	/* golgo13 */
		switch( n_channel )
		{
		case 1:
			return ( ( readinputport( S12_PORT_GUN ) * 0x240 ) / 0x1ff ) + 0x7a;
		case 2:
			return ( ( readinputport( S12_PORT_GUN + 1 ) * 0x1bf ) / 0x1ff ) + 0x1f;
		}
		break;
	case 2:	/* kartduel */
		if( n_channel < 3 )
		{
			return readinputport( S12_PORT_GUN + n_channel ) & 0x3ff;
		}
		break;
	}
	return 0;
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
static int m_alt_bank;	/* mdhorse, kaiunqz: "alt bank" method of current MAME */

static void bankoffset_update( void )
{
	UINT32 n_banks = memory_region_length( REGION_USER3 ) / 0x200000;
	UINT32 n_bank = m_n_bankoffset;

	if( n_banks == 0 )
	{
		return;
	}
	if( n_bank >= n_banks )
	{
		n_bank %= n_banks;
	}
	cpu_setbank( 7, memory_region( REGION_USER3 ) + ( n_bank * 0x200000 ) );
}

static WRITE32_HANDLER( bankoffset_w )
{
	/* only the low half (0x1f000000-1) is the bank register (current MAME) */
	if( !ACCESSING_LSW32 )
	{
		return;
	}
	data &= 0xffff;

	if( m_alt_bank )
	{
		/* bit 3 set: upper bits ((data - 8) << 2), clear: lower 3 bits */
		if( ( data & 8 ) != 0 )
		{
			m_n_bankoffset = ( data - 8 ) << 2;
		}
		else
		{
			m_n_bankoffset = ( m_n_bankoffset & ~7 ) | ( data & 7 );
		}
	}
	else
	{
		m_n_bankoffset = data;
	}

	bankoffset_update();

	verboselog( 1, "bankoffset_w( %08x, %08x, %08x ) %08x\n", offset, data, mem_mask, m_n_bankoffset );
}

static data32_t m_n_dmaoffset;

/* PSX expansion 1 base address (0x1f801000).  Some games (mdhorse, kaiunqz)
   point it at 0x1f300000 to DMA from the program ROM without setting bit 31
   of the DMA offset (current MAME) */
static data32_t m_n_expbase;

static WRITE32_HANDLER( expbase_w )
{
	COMBINE_DATA( &m_n_expbase );
}

static WRITE32_HANDLER( dmaoffset_w )
{
	/* a 16-bit register in current MAME: the address of the write gives the low
	   half of the offset, the data the high half; the upper half of a 32-bit
	   word gives an offset ending in 2 */
	if( ACCESSING_LSW32 )
	{
		m_n_dmaoffset = ( offset * 4 ) | ( ( data & 0xffff ) << 16 );
	}
	if( ACCESSING_MSW32 )
	{
		m_n_dmaoffset = ( offset * 4 + 2 ) | ( data & 0xffff0000 );
	}
	verboselog( 1, "dmaoffset_w( %08x, %08x, %08x ) %08x\n", offset, data, mem_mask, m_n_dmaoffset );
}

static int m_has_tektagt_dma;
static data32_t m_n_tektagdmaoffset;

static void namcos12_rom_read( UINT32 n_address, INT32 n_size )
{
	INT32 n_left;
	data16_t *p_n_src;
	UINT32 *p_n_dst;
	data32_t n_offset = m_has_tektagt_dma ? m_n_tektagdmaoffset : m_n_dmaoffset;

	verboselog( 1, "namcos12_rom_read( %08x, %08x )\n", n_address, n_size );

	m_has_tektagt_dma = 0;

	if( n_offset >= 0x80000000 || m_n_expbase == 0x1f300000 )
	{
		/* the boot loaders of some games (kaiunqz) copy a little past the end
		   of the program ROM on purpose (current MAME masks with 0xffffff) */
		n_offset &= 0x00ffffff;
		if( n_offset >= memory_region_length( REGION_USER2 ) )
		{
			return; /* past the end: nothing is copied (current MAME) */
		}
		p_n_src = (data16_t *)( memory_region( REGION_USER2 ) + ( n_offset & ~1 ) );
		n_left = ( memory_region_length( REGION_USER2 ) - n_offset ) / 4;
		if( n_size > n_left )
		{
			n_size = n_left;
		}
	}
	else
	{
		n_offset &= 0x7fffffff;
		if( m_alt_bank )
		{
			/* the alt bank method keeps the upper bits of the offset in the bank,
			   reduced as for the CPU window (bankoffset_update): with the 31-bit
			   offset the sum stays below 2^32 */
			UINT32 n_banks = memory_region_length( REGION_USER3 ) / 0x200000;
			if( n_banks == 0 )
			{
				return;
			}
			n_offset += ( m_n_bankoffset % n_banks ) * 0x200000;
		}
		if( n_offset >= memory_region_length( REGION_USER3 ) )
		{
			return; /* past the end: nothing is copied (current MAME) */
		}
		p_n_src = (data16_t *)( memory_region( REGION_USER3 ) + ( n_offset & ~1 ) );
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
	/* in 16-bit units: the offset can end in 2 (dmaoffset_w) */
	while( n_size > 0 )
	{
		*( p_n_dst++ ) = p_n_src[ 0 ] | ( p_n_src[ 1 ] << 16 );
		p_n_src += 2;
		n_size--;
	}
}

/* the AT28C16 EEPROM (2KB) at 0x1f140000, one byte in each 16-bit half
   (current MAME: umask 0x00ff00ff). The core keeps the 8KB window as RAM,
   so that the NVRAM files of older versions still load. A new one starts
   as an erased AT28C16 reads in MAME: 0xff in the EEPROM bytes, 0 in the
   other ones, or the factory contents when the set has them (REGION_USER4) */
static NVRAM_HANDLER( namcos12 )
{
	if( read_or_write )
	{
		mame_fwrite( file, generic_nvram, generic_nvram_size );
	}
	else if( file )
	{
		mame_fread( file, generic_nvram, generic_nvram_size );
	}
	else
	{
		data32_t *nv = (data32_t *)generic_nvram;
		data8_t *factory = memory_region( REGION_USER4 );
		UINT32 n_factory = memory_region_length( REGION_USER4 );
		UINT32 i;

		memset( generic_nvram, 0, generic_nvram_size );
		for( i = 0; i < 0x400 && i * 4 < generic_nvram_size; i++ )
		{
			if( factory != NULL && i * 2 + 1 < n_factory )
			{
				nv[ i ] = factory[ i * 2 ] | ( factory[ i * 2 + 1 ] << 16 );
			}
			else
			{
				nv[ i ] = 0x00ff00ff;
			}
		}
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

	m_alt_bank = 0;
	m_n_bankoffset = 0;
	m_n_expbase = 0;
	install_mem_write32_handler( 0, 0x1f801000, 0x1f801003, expbase_w );

	psx_driver_init();

	psx_dma_install_read_handler( 5, namcos12_rom_read );

	state_save_register_UINT32( "namcos12", 0, "m_n_dmaoffset", &m_n_dmaoffset, 1 );
	state_save_register_UINT32( "namcos12", 0, "m_n_bankoffset", &m_n_bankoffset, 1 );
	state_save_register_UINT32( "namcos12", 0, "m_n_expbase", &m_n_expbase, 1 );
	/* the PSX scratchpad (as the ZN drivers do) */
	state_save_register_UINT32( "namcos12", 0, "scratchpad", (data32_t *)memory_region( REGION_USER1 ), 0x400 / 4 );
	/* the RAM shared with the H8/3002 (its side above 16KB is h8ram) */
	state_save_register_UINT32( "namcos12", 0, "sharedram", namcos12_sharedram, 0x4000 / 4 );
	/* the AT28C16 EEPROM (the game settings and bookkeeping), as MAME */
	if( generic_nvram != NULL && generic_nvram_size != 0 )
	{
		state_save_register_UINT8( "namcos12", 0, "eeprom", generic_nvram, generic_nvram_size );
	}
	else
	{
		log_cb( RETRO_LOG_WARN, LOGPRE "namcos12: EEPROM not in the save state\n" );
	}
	state_save_register_func_postload( bankoffset_update );
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
	s12_has_adc = 0;
	s12_n_guns = 0;
}

static DRIVER_INIT( namcos12_h8b )
{
	init_namcos12_h8();
	s12_boot_hack = 1;
}

static DRIVER_INIT( namcos12_h8alt )
{
	init_namcos12_h8();
	m_alt_bank = 1;
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

static void s12_gun_setup( int n_guns, int xmin, int xmax, int ymin, int ymax, int reverse_y )
{
	s12_n_guns = n_guns;
	s12_gun_min[ 0 ] = xmin; s12_gun_max[ 0 ] = xmax;
	s12_gun_min[ 1 ] = ymin; s12_gun_max[ 1 ] = ymax;
	s12_gun_reverse_y = reverse_y;
}

/* Point Blank 2, Ghoul Panic: coh700b with the System 11 gun board */
static DRIVER_INIT( ptblank2 )
{
	init_namcos12_h8b();

	install_mem_read32_handler( 0, 0x1f780000, 0x1f78000f, ptblank2_gun_r );
	install_mem_read32_handler( 0, 0x1ff80000, 0x1ff8000f, ptblank2_gun_r );
	install_mem_write32_handler( 0, 0x1f788000, 0x1f788003, MWA32_NOP );	/* lamps and recoil */
	install_mem_write32_handler( 0, 0x1ff88000, 0x1ff88003, MWA32_NOP );

	/* HACK (current MAME): patch out the wait for DMA 5 to complete */
	*( (data32_t *)( memory_region( REGION_USER2 ) + 0x331c4 ) ) = 0;

	if( namcos12_is( "ghlpanic" ) )
	{
		s12_gun_setup( 2, 0xbe, 0x35f, 0x1a, 0x109, 0 );
	}
	else
	{
		s12_gun_setup( 2, 0xdc, 0x387, 0x28, 0x11b, 0 );
	}
}

/* Golgo 13: alt bank, coh700b, the gun on the H8/3002 ADC */
static DRIVER_INIT( golgo13 )
{
	init_namcos12_h8alt();
	s12_boot_hack = 1;
	s12_has_adc = 1;
	s12_gun_setup( 1, 0x000, 0x1ff, 0x000, 0x1ff, 1 );
}

/* Kart Duel: the link board (current MAME kartduel_state). Its H8/3002 runs
   a program the game uploads to the shared RAM and stays in reset without
   the link (MAME: MACHINE_NODEVICE_LAN), so only the RAM is there */
static data32_t *kartduel_link_ram;
static data32_t kartduel_link_regs[ 0x10 ];

static READ32_HANDLER( kartduel_link_ram_r ) { return kartduel_link_ram[ offset ]; }
static WRITE32_HANDLER( kartduel_link_ram_w ) { COMBINE_DATA( &kartduel_link_ram[ offset ] ); }
static READ32_HANDLER( kartduel_link_regs_r ) { return kartduel_link_regs[ offset & 0x0f ]; }
static WRITE32_HANDLER( kartduel_link_regs_w ) { COMBINE_DATA( &kartduel_link_regs[ offset & 0x0f ] ); }

static DRIVER_INIT( kartduel )
{
	init_namcos12_h8b();
	s12_has_adc = 2;

	kartduel_link_ram = auto_malloc( 0x10000 );
	memset( kartduel_link_ram, 0, 0x10000 );
	memset( kartduel_link_regs, 0, sizeof( kartduel_link_regs ) );
	state_save_register_UINT32( "kartduel", 0, "link_ram", kartduel_link_ram, 0x10000 / 4 );
	state_save_register_UINT32( "kartduel", 0, "link_regs", kartduel_link_regs, 0x10 );

	install_mem_read32_handler( 0, 0x1f780000, 0x1f78ffff, kartduel_link_ram_r );
	install_mem_write32_handler( 0, 0x1f780000, 0x1f78ffff, kartduel_link_ram_w );
	install_mem_write32_handler( 0, 0x1f796000, 0x1f796003, MWA32_NOP );	/* link cpu reset */
	install_mem_read32_handler( 0, 0x1f796004, 0x1f79600b, kartduel_link_regs_r );
	install_mem_write32_handler( 0, 0x1f796004, 0x1f79600b, kartduel_link_regs_w );
	install_mem_read32_handler( 0, 0x1f796014, 0x1f796017, kartduel_link_regs_r );
	install_mem_write32_handler( 0, 0x1f796014, 0x1f796017, kartduel_link_regs_w );
	install_mem_read32_handler( 0, 0x1f796020, 0x1f796023, kartduel_link_regs_r );
	install_mem_write32_handler( 0, 0x1f796020, 0x1f796023, kartduel_link_regs_w );
	install_mem_read32_handler( 0, 0x1f797000, 0x1f797003, kartduel_link_regs_r );
	install_mem_write32_handler( 0, 0x1f797000, 0x1f797003, kartduel_link_regs_w );
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
	MDRV_CPU_ADD( PSXCPU, 100000000 / 4 ) /* CXD8661R, 100MHz XTAL; one cycle per four clocks as in current MAME */
	MDRV_CPU_MEMORY( namcos12_readmem, namcos12_writemem )
	MDRV_CPU_VBLANK_INT( namcos12_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( namcos12 )
	MDRV_NVRAM_HANDLER( namcos12 )

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
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 100000000 / 4 ) /* CXD8661R, 100MHz XTAL; one cycle per four clocks as in current MAME */
	MDRV_CPU_MEMORY( namcos12_readmem, namcos12_writemem )
	MDRV_CPU_VBLANK_INT( namcos12_vblank_h8, 1 )

	MDRV_CPU_ADD( H83002, 16934400 ) /* frequency based on research (superctr) */
	MDRV_CPU_MEMORY( s12h8_readmem, s12h8_writemem )
	MDRV_CPU_VBLANK_INT( irq1_line_pulse, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 40 )

	MDRV_MACHINE_INIT( namcos12_h8 )
	MDRV_NVRAM_HANDLER( namcos12 )

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

/* the light gun games: coh700s with the crosshairs */
static MACHINE_DRIVER_START( coh700g )
	MDRV_IMPORT_FROM( coh700s )
	MDRV_VIDEO_UPDATE( namcos12_gun )
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

/* ptblank2 (current MAME): the trigger on button 1, two light guns */
INPUT_PORTS_START( ptblank2 )
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
	PORT_BIT( 0x006f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x6f00, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )

	/* GUN1X, GUN1Y, GUN2X, GUN2Y */
	PORT_START
	PORT_ANALOG( 0xffff, 0x0231, IPT_LIGHTGUN_X | IPF_PLAYER1, 100, 15, 0x0dc, 0x387 )
	PORT_START
	PORT_ANALOG( 0xffff, 0x00a1, IPT_LIGHTGUN_Y | IPF_PLAYER1, 50, 15, 0x028, 0x11b )
	PORT_START
	PORT_ANALOG( 0xffff, 0x0231, IPT_LIGHTGUN_X | IPF_PLAYER2, 100, 15, 0x0dc, 0x387 )
	PORT_START
	PORT_ANALOG( 0xffff, 0x00a1, IPT_LIGHTGUN_Y | IPF_PLAYER2, 50, 15, 0x028, 0x11b )
INPUT_PORTS_END

/* ghlpanic (current MAME): the trigger on button 1, two light guns */
INPUT_PORTS_START( ghlpanic )
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
	PORT_BIT( 0x006f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x6f00, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )

	/* GUN1X, GUN1Y, GUN2X, GUN2Y */
	PORT_START
	PORT_ANALOG( 0xffff, 0x020e, IPT_LIGHTGUN_X | IPF_PLAYER1, 100, 15, 0x0be, 0x35f )
	PORT_START
	PORT_ANALOG( 0xffff, 0x0091, IPT_LIGHTGUN_Y | IPF_PLAYER1, 50, 15, 0x01a, 0x109 )
	PORT_START
	PORT_ANALOG( 0xffff, 0x020e, IPT_LIGHTGUN_X | IPF_PLAYER2, 100, 15, 0x0be, 0x35f )
	PORT_START
	PORT_ANALOG( 0xffff, 0x0091, IPT_LIGHTGUN_Y | IPF_PLAYER2, 50, 15, 0x01a, 0x109 )
INPUT_PORTS_END

/* Golgo 13 (current MAME): up / down select, enter, trigger, the sensor of the gun stand */
INPUT_PORTS_START( golgo13 )
	/* DSW (H8 port 7) */
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN0 */
	PORT_START
	PORT_BIT( 0x0003, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* enter */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* trigger */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x07ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* sensor */
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )

	/* GUNX, GUNY (the Y port is reversed) */
	PORT_START
	PORT_ANALOG( 0xffff, 0x0100, IPT_LIGHTGUN_X | IPF_PLAYER1, 100, 15, 0x000, 0x1ff )
	PORT_START
	PORT_ANALOG( 0xffff, 0x0100, IPT_LIGHTGUN_Y | IPF_PLAYER1 | IPF_REVERSE, 100, 15, 0x000, 0x1ff )
INPUT_PORTS_END

/* Kart Duel (current MAME): up / down select, enter, the pedals and the wheel on the ADC */
INPUT_PORTS_START( kartduel )
	/* DSW (H8 port 7) */
	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	/* IN0 */
	PORT_START
	PORT_BIT( 0x0003, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* enter */
	PORT_BIT( 0x0060, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	/* IN1 */
	PORT_START
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )

	/* ADC0: brake, ADC1: gas, ADC2: steering wheel */
	PORT_START
	PORT_ANALOG( 0x03ff, 0x00ad, IPT_PEDAL2 | IPF_PLAYER1, 100, 40, 0x000, 0x15a )
	PORT_START
	PORT_ANALOG( 0x03ff, 0x00ad, IPT_PEDAL | IPF_PLAYER1, 100, 40, 0x000, 0x15a )
	PORT_START
	PORT_ANALOG( 0x03ff, 0x0200, IPT_PADDLE | IPF_PLAYER1, 100, 40, 0x0e0, 0x320 )
INPUT_PORTS_END

/* Paca Paca Passion (current MAME): four coloured buttons per player on the joystick lines, one coin slot */
INPUT_PORTS_START( pacapp )
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
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
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
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BITX(0x4000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )
INPUT_PORTS_END

/* Paca Paca Passion 2 / Special: as pacapp, two coin slots */
INPUT_PORTS_START( pacapp2 )
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
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
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

/* My Dream Horse, Kaiun Quiz (current MAME): four answer buttons per player on the joystick lines, one coin slot */
INPUT_PORTS_START( mdhorse )
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
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
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
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
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

ROM_START( kaiunqz )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "kw1vera.2l",        0x0000000, 0x200000, CRC(fd1d2324) SHA1(eedb4627dfa17e9aac2c99592628f1fa7060edb5) )
	ROM_LOAD16_BYTE( "kw1vera.2p",        0x0000001, 0x200000, CRC(d8bdea6b) SHA1(d32118846a1f43eecff7f56dcda03adf04975784) )

	ROM_REGION32_LE( 0x3000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "kw1rom0l.12",       0x0000000, 0x800000, CRC(e0768f0b) SHA1(3ffec7c789a95fcff7f06432723f155b99a20c42) )
	ROM_LOAD16_BYTE( "kw1rom0u.11",       0x0000001, 0x800000, CRC(04101a16) SHA1(c2d79333ee382e2fea229bba23918fb5f88a25d4) )
	ROM_LOAD16_BYTE( "kw1rom1l.10",       0x1000000, 0x800000, CRC(6a4edfa6) SHA1(a59fa28446b28db28b38368c794af71e673f69a2) )
	ROM_LOAD16_BYTE( "kw1rom1u.9",        0x1000001, 0x800000, CRC(e6150b0d) SHA1(7dd1ba524246aea007e7bd6d3e7d457033b23a09) )
	ROM_LOAD16_BYTE( "kw1rom2l.7",        0x2000000, 0x800000, CRC(21c376e4) SHA1(4b82494c1e13ebcea3c4e97803c0667be73bf6ae) )
	ROM_LOAD16_BYTE( "kw1rom2u.8",        0x2000001, 0x800000, CRC(d850c222) SHA1(802c1782b323802eae871e2b9ccc5eae34277861) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "kw1vera.11s",  0x0000000, 0x080000, CRC(d863fafa) SHA1(3c2bb38c24165e3a1a4d1d257fcfc019b63d5199) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "kw1wave0.2",               0x0000000, 0x800000, CRC(060e52ae) SHA1(7ea95cae9d3c648163b225d0c7d365644be90241) )
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

ROM_START( pacapp2 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "pks1vera.2l",       0x0000000, 0x200000, CRC(aec428d3) SHA1(c13aecc6a367d6da501dce66fecbab5458ecac53) )
	ROM_LOAD16_BYTE( "pks1vera.2p",       0x0000001, 0x200000, CRC(289e6e8a) SHA1(b8197355bee5660e8ff78a1c427c6d2b94a12b9d) )

	ROM_REGION32_LE( 0x1800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "pks1rom0.ic7",      0x0000000, 0x800000, CRC(91f151fc) SHA1(84b4c9b2a9ae5687e1cba5f0a0db96495956cbfe) )
	ROM_LOAD(        "pks1rom1.ic8",      0x0800000, 0x800000, CRC(8195f981) SHA1(1b9c2289e0975606ff52b9974778f82c9c29e955) )
	ROM_LOAD(        "pks1rom2.ic9",      0x1000000, 0x800000, CRC(fea93ddc) SHA1(5f9c3f899a11797746d9c1077960f918aaca95f4) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "pks1vera.11s", 0x0000000, 0x080000, CRC(4942b588) SHA1(b28eb64a7347259619e32fdd396770983e25a707) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "pks1wave0.ic2",            0x0000000, 0x800000, CRC(3ebd27c3) SHA1(af874c3fb4cf1651075f307daefe7e3434769ecb) )
	ROM_LOAD( "pks1wave1.ic1",            0x0800000, 0x800000, CRC(07e426b7) SHA1(6dd038d4f2c95470f1ca09cfead83802e9480d02) )
ROM_END

ROM_START( pacappsp )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "psp1vera.2l",       0x0000000, 0x200000, CRC(4b6943af) SHA1(63b21794719bc1fc075e9cc4f1c1783442860036) )
	ROM_LOAD16_BYTE( "psp1vera.2p",       0x0000001, 0x200000, CRC(91397f04) SHA1(db3dd59edcdec10eb2fee74450c024a7ecffe1c9) )

	ROM_REGION32_LE( 0x1800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD(        "psp1rom0.ic7",      0x0000000, 0x800000, CRC(7c26ff47) SHA1(f9d366ae9eb11e14875b611112bc82f7c7d391e0) )
	ROM_LOAD(        "psp1rom1.ic8",      0x0800000, 0x800000, CRC(4b764fc3) SHA1(92a7f0f1537f2f4d5a8b0218720978b576f78dad) )
	ROM_LOAD(        "psp1rom2.ic9",      0x1000000, 0x800000, CRC(a3fa6730) SHA1(0126a77661c3add0bfec9f6ef15021e602b7a614) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "psp1vera.11s", 0x0000000, 0x080000, CRC(47b9ccab) SHA1(15d4c5ac9ae7202df2d0ef84092679d190f2c45a) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "psp1wave0.ic2",            0x0000000, 0x800000, CRC(44b9a327) SHA1(ab20d5371f7a8c3a7fbc21fcf54a93c1cd007cd2) )
	ROM_LOAD( "psp1wave1.ic1",            0x0800000, 0x800000, CRC(c045b09c) SHA1(1cfd7003771576f6574d91d862af1f76ceaed64f) )
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

ROM_START( tekken3a )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet2vera.2e",       0x0000000, 0x200000, CRC(7270f157) SHA1(e73c5970e58f9e8c5696f4e3b15908fbec6c21ce) )
	ROM_LOAD16_BYTE( "tet2vera.2j",       0x0000001, 0x200000, CRC(94ceb446) SHA1(c730eb5c770991ae3ae0b9ba63681ce037e46746) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1verafl3l.12",   0x1800000, 0x200000, CRC(64fa1f83) SHA1(5bc8b4657826ead19827f63d04f67597970d5337) )
	ROM_LOAD16_BYTE( "tet1verafl3u.13",   0x1800001, 0x200000, CRC(32a2516b) SHA1(6f4e4fc1b11d17a867d3e7bdfdd351438390a5a2) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1vera.11s", 0x0000000, 0x080000, CRC(a74dfe7f) SHA1(854096a6f12ee9073fb1f38e41c8e6e0725a7521) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3b )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet2verb.2e",       0x0000000, 0x200000, CRC(a6cbc434) SHA1(859d84e6e9a52c2cdd54a2a0bb8104169eb19c07) )
	ROM_LOAD16_BYTE( "tet2verb.2j",       0x0000001, 0x200000, CRC(c8f95ec5) SHA1(7f34c42e1fbc35118e8476cdb78fbdb9564001de) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",       0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",       0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1verb.11s", 0x0000000, 0x080000, CRC(b13d88a9) SHA1(944d35203bbb7155a296ff37daa426b42f8e2b6b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3c )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet2verc.2e",       0x0000000, 0x200000, CRC(4483d76e) SHA1(c43b7e445acb77ddeb8a647bef1fc509ced0412e) )
	ROM_LOAD16_BYTE( "tet2verc.2j",       0x0000001, 0x200000, CRC(3b4fee42) SHA1(b71b2969b0416e6fd348297876fc55d44e14ebef) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",       0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",       0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1verb.11s", 0x0000000, 0x080000, CRC(b13d88a9) SHA1(944d35203bbb7155a296ff37daa426b42f8e2b6b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3d )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet2verd.2e",       0x0000000, 0x200000, CRC(ff269bcd) SHA1(f118b69ffe3ee1ad785c115c39d5166f3c546554) )
	ROM_LOAD16_BYTE( "tet2verd.2j",       0x0000001, 0x200000, CRC(46f9205c) SHA1(662b8f910e4ccc1a0e9f3fef0992a92abbebebd0) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",       0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",       0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1verb.11s", 0x0000000, 0x080000, CRC(b13d88a9) SHA1(944d35203bbb7155a296ff37daa426b42f8e2b6b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3ua )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet3vera.2e",       0x0000000, 0x200000, CRC(2fb6fac2) SHA1(518f74f09fa879cc1507c6afa2dd922b38cecd55) )
	ROM_LOAD16_BYTE( "tet3vera.2j",       0x0000001, 0x200000, CRC(968af792) SHA1(6187128d5ca07fd394f674b8dda0c190e6cd7f9d) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1verafl3l.12",   0x1800000, 0x200000, CRC(64fa1f83) SHA1(5bc8b4657826ead19827f63d04f67597970d5337) )
	ROM_LOAD16_BYTE( "tet1verafl3u.13",   0x1800001, 0x200000, CRC(32a2516b) SHA1(6f4e4fc1b11d17a867d3e7bdfdd351438390a5a2) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1vera.11s", 0x0000000, 0x080000, CRC(a74dfe7f) SHA1(854096a6f12ee9073fb1f38e41c8e6e0725a7521) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3ud )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet3verd.2e",       0x0000000, 0x200000, CRC(9056a8d1) SHA1(08269de80361672f1a193e5cdcd0d4571b746a85) )
	ROM_LOAD16_BYTE( "tet3verd.2j",       0x0000001, 0x200000, CRC(60ae06f4) SHA1(898355cc6bae4745b6b9913e34d50fe2a00f1c2c) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",       0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",       0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1verb.11s", 0x0000000, 0x080000, CRC(b13d88a9) SHA1(944d35203bbb7155a296ff37daa426b42f8e2b6b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3ja )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet1vera.2e",       0x0000000, 0x200000, CRC(98fe53b4) SHA1(0d3380f368908a21cb1e4cea353687d3f6295d79) )
	ROM_LOAD16_BYTE( "tet1vera.2j",       0x0000001, 0x200000, CRC(4dc6bb4a) SHA1(1a0ae22410fb6f7757d21fdaf713893ca4e177fe) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1verafl3l.12",   0x1800000, 0x200000, CRC(64fa1f83) SHA1(5bc8b4657826ead19827f63d04f67597970d5337) )
	ROM_LOAD16_BYTE( "tet1verafl3u.13",   0x1800001, 0x200000, CRC(32a2516b) SHA1(6f4e4fc1b11d17a867d3e7bdfdd351438390a5a2) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1vera.11s", 0x0000000, 0x080000, CRC(a74dfe7f) SHA1(854096a6f12ee9073fb1f38e41c8e6e0725a7521) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tekken3jd )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tet1verd.2e",      0x0000000, 0x200000, CRC(8a2e5ed5) SHA1(f06aa6eceffa30359c93811bc70e34eb3d85b3c5) )
	ROM_LOAD16_BYTE( "tet1verd.2j",      0x0000001, 0x200000, CRC(fd2128f7) SHA1(c8be7101538c26b6eb1a8fe901549457be8d05b9) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tet1rom0l.6",       0x0000000, 0x400000, CRC(2886bb32) SHA1(08ad9da2df25ad8c933a812ac238c81135072929) )
	ROM_LOAD16_BYTE( "tet1rom0u.9",       0x0000001, 0x400000, CRC(c5705b92) SHA1(20df20c8d18eb4712d565a3df9a8d9270dee6aaa) )
	ROM_LOAD16_BYTE( "tet1rom1l.7",       0x0800000, 0x400000, CRC(0397d283) SHA1(ebafcd14cdb2601214129a84fc6830846f5cd274) )
	ROM_LOAD16_BYTE( "tet1rom1u.10",      0x0800001, 0x400000, CRC(502ba5cd) SHA1(19c1282245c6dbfc945c0bd0f3918968c3e5c3ed) )
	ROM_LOAD16_BYTE( "tet1rom2l.8",       0x1000000, 0x400000, CRC(e03b1c24) SHA1(8579b95a8fd06b7d2893ff88b228fd794162dff1) )
	ROM_LOAD16_BYTE( "tet1rom2u.11",      0x1000001, 0x400000, CRC(75eb2ab3) SHA1(dee43884e542391903f6aaae2c166e7921a86fb4) )
	ROM_LOAD16_BYTE( "tet1fl3l.12",       0x1800000, 0x200000, CRC(45513073) SHA1(8a36f58ee2d292b50e00c6bf275f9def358032d8) )
	ROM_LOAD16_BYTE( "tet1fl3u.13",       0x1800001, 0x200000, CRC(1917d993) SHA1(cabc44514a3e62a18a7f8f883603241447d6948b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tet1verb.11s", 0x0000000, 0x080000, CRC(b13d88a9) SHA1(944d35203bbb7155a296ff37daa426b42f8e2b6b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tet1wave0.5",              0x0000000, 0x400000, CRC(77ba7975) SHA1(fe9434dcf0fb232c85efaaae1b4b13d36099620a) )
	ROM_LOAD( "tet1wave1.4",              0x0400000, 0x400000, CRC(ffeba79f) SHA1(941412bbe9d0305d9a23c224c1bb774c4321f6df) )
ROM_END

ROM_START( tektagtc1a )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg2ver_c1.2e",     0x0000000, 0x200000, CRC(c0800960) SHA1(80fc8910ebb2399b3be3c9ea87cc1d9283b42676) )
	ROM_LOAD16_BYTE( "teg2ver_c1.2j",     0x0000001, 0x200000, CRC(c0476713) SHA1(e51e4f3cd20ad6838fb05aaede0ab288e145e7a2) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg2roml0",         0x0000000, 0x400000, CRC(cf984e85) SHA1(4d8296998b5f225b3f20bfdcb092a64b6e1fa76e) )
	ROM_LOAD32_WORD( "teg2roml00.ic13",   0x0000002, 0x400000, CRC(927723a5) SHA1(075aaca1700c9134496eb9a73cbe325ddc74eae1) )
	ROM_LOAD32_WORD( "teg2romh0",         0x0800000, 0x400000, CRC(ea088657) SHA1(fbacca78832f68a310bafd06188d645e3424e0ed) )
	ROM_LOAD32_WORD( "teg2romh00",        0x0800002, 0x400000, CRC(a85aa306) SHA1(07318133c7c48cb61590ed0eceb519a8cc1ca781) )
	ROM_LOAD32_WORD( "teg2roml1",         0x1000000, 0x400000, CRC(8552f0ef) SHA1(45ed0dacdad94e708260a98b9cd47861543d2b7a) )
	ROM_LOAD32_WORD( "teg2roml10.ic14",   0x1000002, 0x400000, CRC(13eb424b) SHA1(59d6c77136050de1e7250e6627636d5b178bb8a8) )
	ROM_LOAD32_WORD( "teg2romh1",         0x1800000, 0x400000, CRC(86eb5abe) SHA1(66a3807a18d81e8291baf56533694f60e31a6367) )
	ROM_LOAD32_WORD( "teg2romh10",        0x1800002, 0x400000, CRC(25131a87) SHA1(77fa236c4879a59323bc2dac26c195b7c47f812c) )
	ROM_LOAD32_WORD( "teg2roml2",         0x2000000, 0x400000, CRC(abdfc6ca) SHA1(70033becd29ea15611be2fc099a43712181bcbec) )
	ROM_LOAD32_WORD( "teg2roml20.ic15",   0x2000002, 0x400000, CRC(ed5ec7f7) SHA1(c91085a64ced95ee6618303987de61893e069742) )
	ROM_LOAD32_WORD( "teg2romh2",         0x2800000, 0x400000, CRC(8c03b301) SHA1(f395a4685e2deb8f7e9afc18ce15fa0d392b0cfa) )
	ROM_LOAD32_WORD( "teg2romh20",        0x2800002, 0x400000, CRC(c873c362) SHA1(d95451a7477b716c0d75ba04e1d8df3d8a4e10dd) )
	ROM_LOAD32_WORD( "teg2rf4",           0x3000000, 0x400000, CRC(612f6a37) SHA1(bd9597ef802e3c46f891f0ef523a3e0b0fe22672) )
	ROM_LOAD32_WORD( "teg2rf6",           0x3000002, 0x400000, CRC(0c9292ce) SHA1(664c6ccf18042ed8ebc29b1781ae7454acb05ba1) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s",     0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg2wavel0.ic12",          0x0000000, 0x400000, CRC(1865336f) SHA1(deda6d129daf709b0316f6260e67613fbf849e57) )
	ROM_LOAD( "teg2wavel1",               0x0400000, 0x400000, CRC(f5ab70e6) SHA1(cdb62d8eb92ca961be684caf768d1961d98ece22) )
	ROM_LOAD( "teg2waveh0",               0x0800000, 0x400000, CRC(bc0325bf) SHA1(77059c300a9d53d0e97bb898a06c58076078d7d2) )
	ROM_LOAD( "teg2waveh1",               0x0c00000, 0x400000, CRC(6c654921) SHA1(3849157b30b4b459956b823511551abda2e4b6e5) )
ROM_END

ROM_START( tektagtja3 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg1vera.2e",       0x0000000, 0x200000, BAD_DUMP CRC(17c4bf36) SHA1(abf2dfb3e35344cf4449ade6e63b36c590d9c131) )
	ROM_LOAD16_BYTE( "teg1vera.2j",       0x0000001, 0x200000, BAD_DUMP CRC(97cd9524) SHA1(8031cb465db378a6d9db9b132cf1169b94cba7dc) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg1_rom0e.ic9",    0x0000000, 0x800000, CRC(c962a373) SHA1(d662dbd89ef62c5ac3150a018fc2d35ef2ee94ac) )
	ROM_LOAD32_WORD( "teg1_rom0o.ic13",   0x0000002, 0x800000, CRC(badb7dcf) SHA1(8c0bf7f6351c5a2a0996df371a901cf90c68cd8c) )
	ROM_LOAD32_WORD( "teg1_rom1e.ic10",   0x1000000, 0x800000, CRC(b3d56124) SHA1(4df20c74ba63f7362caf15e9b8949fab655704fb) )
	ROM_LOAD32_WORD( "teg1_rom1o.ic14",   0x1000002, 0x800000, CRC(2434ceb6) SHA1(f19f1599acbd6fd48793a2ee5a500ca817d9df56) )
	ROM_LOAD32_WORD( "teg1_rom2e.ic11",   0x2000000, 0x800000, CRC(6e5c3428) SHA1(e3cdb60a4445406877b2e273385f34bfb0974220) )
	ROM_LOAD32_WORD( "teg1_rom2o.ic15",   0x2000002, 0x800000, CRC(21ce9dfa) SHA1(f27e8210ee236c327aa3e1ce4dd408abc6580a1b) )
	ROM_LOAD32_BYTE( "teg1flel.4",        0x3000000, 0x200000, NO_DUMP ) /* It's NOT been verified these should be the same, like the Tekken 3 Ver A  */
	ROM_LOAD32_BYTE( "teg1fleu.5",        0x3000001, 0x200000, NO_DUMP ) /* sets above, these Ver A set(s) might require a specific set of flash ROMs */
	ROM_LOAD32_BYTE( "teg1flol.6",        0x3000002, 0x200000, NO_DUMP )
	ROM_LOAD32_BYTE( "teg1flou.7",        0x3000003, 0x200000, NO_DUMP )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s",     0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg1_wave0.ic1",           0x0000000, 0x800000, CRC(4bd99104) SHA1(f76b0576cc28fe49d3c1c402988b933933e52e15) )
	ROM_LOAD( "teg1_wave1.ic12",          0x0800000, 0x800000, CRC(dbc74fff) SHA1(601b7e7361ea744b34e3fa1fc39d88641de7f4c6) )
ROM_END

ROM_START( tektagtjb )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "teg1verb.2e",       0x0000000, 0x200000, BAD_DUMP CRC(ca6c305f) SHA1(264a85566b74f544fe63a01332d92c65d23b6608) )
	ROM_LOAD16_BYTE( "teg1verb.2j",       0x0000001, 0x200000, BAD_DUMP CRC(5413e2ed) SHA1(d453f7932654d8258c67eb7fe3639d71db7e414c) )

	ROM_REGION32_LE( 0x3800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD32_WORD( "teg1_rom0e.ic9",    0x0000000, 0x800000, CRC(c962a373) SHA1(d662dbd89ef62c5ac3150a018fc2d35ef2ee94ac) )
	ROM_LOAD32_WORD( "teg1_rom0o.ic13",   0x0000002, 0x800000, CRC(badb7dcf) SHA1(8c0bf7f6351c5a2a0996df371a901cf90c68cd8c) )
	ROM_LOAD32_WORD( "teg1_rom1e.ic10",   0x1000000, 0x800000, CRC(b3d56124) SHA1(4df20c74ba63f7362caf15e9b8949fab655704fb) )
	ROM_LOAD32_WORD( "teg1_rom1o.ic14",   0x1000002, 0x800000, CRC(2434ceb6) SHA1(f19f1599acbd6fd48793a2ee5a500ca817d9df56) )
	ROM_LOAD32_WORD( "teg1_rom2e.ic11",   0x2000000, 0x800000, CRC(6e5c3428) SHA1(e3cdb60a4445406877b2e273385f34bfb0974220) )
	ROM_LOAD32_WORD( "teg1_rom2o.ic15",   0x2000002, 0x800000, CRC(21ce9dfa) SHA1(f27e8210ee236c327aa3e1ce4dd408abc6580a1b) )
	ROM_LOAD32_BYTE( "flel.ic4",          0x3000000, 0x200000, CRC(88b3823c) SHA1(6f31acb642c57daccbfdb87b790037e261c8c73c) )
	ROM_LOAD32_BYTE( "fleu.ic5",          0x3000001, 0x200000, CRC(36df0867) SHA1(6bec8560ad4c122dc909daa83aa9089ba5b281f7) )
	ROM_LOAD32_BYTE( "flol.ic6",          0x3000002, 0x200000, CRC(03a76765) SHA1(ae35ae28375f2a3e52d72b77ec09750c326cc269) )
	ROM_LOAD32_BYTE( "flou.ic7",          0x3000003, 0x200000, CRC(6d6947d1) SHA1(2f307bc4070fadb510c0473bc91d917b2d845ca5) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "teg1.11s",     0x0000000, 0x080000, CRC(67d0c469) SHA1(da164702fc21b9f46a9e32c89e7b1d36070ddf79) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "teg1_wave0.ic1",           0x0000000, 0x800000, CRC(4bd99104) SHA1(f76b0576cc28fe49d3c1c402988b933933e52e15) )
	ROM_LOAD( "teg1_wave1.ic12",          0x0800000, 0x800000, CRC(dbc74fff) SHA1(601b7e7361ea744b34e3fa1fc39d88641de7f4c6) )
ROM_END

ROM_START( lbgrande )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "lg2vera.2l",        0x0000000, 0x200000, CRC(5ed6b152) SHA1(fdab457862bd6e0a3178c9329bd0978b6aa3ae2f) )
	ROM_LOAD16_BYTE( "lg2vera.2p",        0x0000001, 0x200000, CRC(97c57149) SHA1(bb9bc1ba3ea826eb1c987b11218c0afa0fc54bdc) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "lg1rom0l.6",        0x0000000, 0x400000, CRC(c5df7f27) SHA1(07c596efb2533b9adc579874b7e8ef7fcc7f73c3) )
	ROM_LOAD16_BYTE( "lg1rom0u.9",        0x0000001, 0x400000, CRC(74607817) SHA1(448a9213fa566fdbab5d789df064da7dc946ba2c) )
	ROM_LOAD16_BYTE( "lg1fl3l.12",        0x1800000, 0x200000, CRC(c9947d3e) SHA1(239b1f81ffac6a54b438082664124b6cf51a9b1c) )
	ROM_LOAD16_BYTE( "lg1fl3u.13",        0x1800001, 0x200000, CRC(f3d69f45) SHA1(546f588f144e1a75eee4a6d0a6cef8a3f79f0238) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "lg1vera.11s",  0x0000000, 0x080000, CRC(de717a09) SHA1(78f26ff630c50632916fa17fa870dcde7f13781d) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "lg1wave0.5",               0x0000000, 0x400000, CRC(4647fada) SHA1(99f5e9ded0c83f1a0d3670f6380bc15c1380671e) )
ROM_END

ROM_START( toukon3 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tr1vera.2e",        0x0000000, 0x200000, CRC(126ebb73) SHA1(de429e335e03f2b5116fc50f556a5507475a0535) )
	ROM_LOAD16_BYTE( "tr1vera.2j",        0x0000001, 0x200000, CRC(2edb3ad2) SHA1(d1d2d78b781c7f6fb5a201785295daa825ad057e) )

	ROM_REGION32_LE( 0x0800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tr1rom0l.6",        0x0000000, 0x400000, CRC(42946d26) SHA1(dc7944cb6daceda41ecee7a4e3f549cba916ab87) )
	ROM_LOAD16_BYTE( "tr1rom0u.9",        0x0000001, 0x400000, CRC(e3cd0be0) SHA1(f27d14cd086b5961be33e635662aca89a5e8c857) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tr1vera.11s",  0x0000000, 0x080000, CRC(f9ecbd19) SHA1(ad64f95ef85ee5c45dcf15217ea5adff61fddba6) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tr1wave0.5",               0x0000000, 0x400000, CRC(07d10e55) SHA1(3af8f7019a221981984741181b6be98e2315d441) )
	ROM_LOAD( "tr1wave1.4",               0x0400000, 0x400000, CRC(34539cdd) SHA1(afb7079c0f447fbda285a5b97a37c04baf26db75) )
ROM_END

ROM_START( sws98 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ss81vera.2l",       0x0000000, 0x200000, CRC(94b1f34c) SHA1(0c8491fda366b5b2874e5f49959dccd11d372e46) )
	ROM_LOAD16_BYTE( "ss81vera.2p",       0x0000001, 0x200000, CRC(7d0ed33d) SHA1(34342ce57b29ee15c6279c099bb145ac7ad262f3) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "ss81fl1l.9",        0x1000000, 0x200000, CRC(b0b5dc77) SHA1(abce4e6ae60858b7c7408d10975b7a1e0d183115) )
	ROM_LOAD16_BYTE( "ss81fl1u.10",       0x1000001, 0x200000, CRC(e526dba5) SHA1(889fdfba17282eb05a3e254af81ee15c3e16acc4) )
	ROM_LOAD16_BYTE( "ss81fl2l.7",        0x1400000, 0x200000, CRC(2dc6f6b5) SHA1(e6c7bb804d7d027acca5adb15b0bb95321905ff3) )
	ROM_LOAD16_BYTE( "ss81fl2u.8",        0x1400001, 0x200000, CRC(c8341c9f) SHA1(b7c5a960a238e963c7ec5bdb0751a8730d3b09f7) )
	ROM_LOAD16_BYTE( "ss81fl3l.5",        0x1800000, 0x200000, CRC(ce5d2a96) SHA1(08f3955e1ce949ce436f6f2f19fd42204682ae8c) )
	ROM_LOAD16_BYTE( "ss81fl3u.6",        0x1800001, 0x200000, CRC(02df4df8) SHA1(01fd66c905cde9ca073dc4b6dbd826c5fe96b79b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ss81vera.11s", 0x0000000, 0x080000, CRC(c6bc5c31) SHA1(c6ef46c3fa8a7749618126d360e614ea6c8d9c54) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ss81wave0.2",              0x0000000, 0x800000, CRC(1c5e2ff1) SHA1(4f29dfd49f6b5c3ca3b758823d368051354bd673) )
	ROM_LOAD( "ss81wave1.1",              0x0800000, 0x800000, CRC(5f4c8861) SHA1(baee7182c32bc064d805de5a16948faf78941ac4) )
ROM_END

ROM_START( tenkomor )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tkm2vera.2e",       0x0000000, 0x200000, CRC(a9b81653) SHA1(9199505019234140b0d89e199f0db307d5bcca02) )
	ROM_LOAD16_BYTE( "tkm2vera.2j",       0x0000001, 0x200000, CRC(28cff9ee) SHA1(d1996d45cca3a9bbd6a7f39721b2ec9f3d052422) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tkm1rom0l.12",      0x0000000, 0x800000, CRC(dddebb39) SHA1(44169b0c6be4d387e7b6087ce723476ee96b09b4) )
	ROM_LOAD16_BYTE( "tkm1rom0u.11",      0x0000001, 0x800000, CRC(dbcc3838) SHA1(c4e11800c5e8122044914152227b8a29e9446c9d) )
	ROM_LOAD16_BYTE( "tkm1fl1l.9",        0x1000000, 0x200000, CRC(071ef722) SHA1(7c0317b3bca2763dbbac5454901cd48a0b195edd) )
	ROM_LOAD16_BYTE( "tkm1fl1u.10",       0x1000001, 0x200000, CRC(580f8391) SHA1(2205c80721631ea8016efbe6d79f5ea7d1924278) )
	ROM_LOAD16_BYTE( "tkm1fl2l.7",        0x1400000, 0x200000, CRC(bd54efe3) SHA1(ff3573066ad2498a33ceabf378a3c69af11ee7db) )
	ROM_LOAD16_BYTE( "tkm1fl2u.8",        0x1400001, 0x200000, CRC(6e4e6320) SHA1(8d220b0028cfc2f02eb34df41a5dbb23be3e7908) )
	ROM_LOAD16_BYTE( "tkm1fl3l.5",        0x1800000, 0x200000, CRC(a31ffb10) SHA1(4f8e41e263611a7f9a63bd53ebee4e68ad0095e6) )
	ROM_LOAD16_BYTE( "tkm1fl3u.6",        0x1800001, 0x200000, CRC(bc566162) SHA1(db40d28c57dbf1351adfa2dd740edbe0b897307f) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tkm1vera.11s", 0x0000000, 0x080000, CRC(0b414dae) SHA1(a8f77ae7ee0dc516cd9aaf944431a9a0c9dc7def) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tkm1wave0.2",              0x0000000, 0x800000, CRC(6085387d) SHA1(a2a55f6ebe9de2d5415a1c4f3ec7975af95b45b4) )
	ROM_LOAD( "tkm1wave1.1",              0x0800000, 0x800000, CRC(7567796b) SHA1(99e4b867477da2ccddfa9bebc4be84adc5cba53c) )
ROM_END

ROM_START( tenkomorja1 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tkm1vera.2e",       0x0000000, 0x200000, CRC(d4c89229) SHA1(aba6686eef924868b3bd2142fd073303fe9c4042) )
	ROM_LOAD16_BYTE( "tkm1vera.2j",       0x0000001, 0x200000, CRC(a6bfcaf4) SHA1(55dfa65e07a63a413f6eb47084e60b4fc32bcde5) )

	ROM_REGION32_LE( 0x1c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tkm1rom0l.12",      0x0000000, 0x800000, CRC(dddebb39) SHA1(44169b0c6be4d387e7b6087ce723476ee96b09b4) )
	ROM_LOAD16_BYTE( "tkm1rom0u.11",      0x0000001, 0x800000, CRC(dbcc3838) SHA1(c4e11800c5e8122044914152227b8a29e9446c9d) )
	ROM_LOAD16_BYTE( "tkm1fl1l.9",        0x1000000, 0x200000, CRC(071ef722) SHA1(7c0317b3bca2763dbbac5454901cd48a0b195edd) )
	ROM_LOAD16_BYTE( "tkm1fl1u.10",       0x1000001, 0x200000, CRC(580f8391) SHA1(2205c80721631ea8016efbe6d79f5ea7d1924278) )
	ROM_LOAD16_BYTE( "tkm1fl2l.7",        0x1400000, 0x200000, CRC(bd54efe3) SHA1(ff3573066ad2498a33ceabf378a3c69af11ee7db) )
	ROM_LOAD16_BYTE( "tkm1fl2u.8",        0x1400001, 0x200000, CRC(6e4e6320) SHA1(8d220b0028cfc2f02eb34df41a5dbb23be3e7908) )
	ROM_LOAD16_BYTE( "tkm1fl3l.5",        0x1800000, 0x200000, CRC(a31ffb10) SHA1(4f8e41e263611a7f9a63bd53ebee4e68ad0095e6) )
	ROM_LOAD16_BYTE( "tkm1fl3u.6",        0x1800001, 0x200000, CRC(bc566162) SHA1(db40d28c57dbf1351adfa2dd740edbe0b897307f) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "tkm1vera.11s", 0x0000000, 0x080000, CRC(0b414dae) SHA1(a8f77ae7ee0dc516cd9aaf944431a9a0c9dc7def) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tkm1wave0.2",              0x0000000, 0x800000, CRC(6085387d) SHA1(a2a55f6ebe9de2d5415a1c4f3ec7975af95b45b4) )
	ROM_LOAD( "tkm1wave1.1",              0x0800000, 0x800000, CRC(7567796b) SHA1(99e4b867477da2ccddfa9bebc4be84adc5cba53c) )
ROM_END

ROM_START( sws2000 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ss01vera.2l",       0x0000000, 0x200000, CRC(6ddbdcaa) SHA1(cff31d75e7780851b2c2c025ee34fd8990e2f502) )
	ROM_LOAD16_BYTE( "ss01vera.2p",       0x0000001, 0x200000, CRC(6ade7d28) SHA1(3e8d7bc9a284324c4de0ac265872b613d108cb57) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD( "ss01rom0.9",        0x0000000, 0x800000, CRC(c08cc59c) SHA1(f2d8064491c98acbb40260d7f13d0f9b394c1383) )
	ROM_LOAD( "ss01rom1.10",       0x0800000, 0x800000, CRC(d4aa1dc6) SHA1(68fcb60aadf35668d7d746da1e29d5d985e40aec) )
	ROM_LOAD( "ss01rom2.11",       0x1000000, 0x800000, CRC(3371904a) SHA1(58bbf98a44560bc021bac4bc513fe89deea85b50) )
	ROM_LOAD16_BYTE( "ss01fl3l.7",        0x1800000, 0x200000, CRC(35bad813) SHA1(19ceffb78695c8c22b30659aad56ac237ffb56ac) )
	ROM_LOAD16_BYTE( "ss01fl3h.8",        0x1800001, 0x200000, CRC(a14828fc) SHA1(fd8a052451f74f420f4a4a2b902e0c2e83e3239b) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ss01vera.11s", 0x0000000, 0x080000, CRC(641e6584) SHA1(dfc0ac21bea5b19dbce5d50fb681854889d756dc) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ss01wave0.2",              0x0000000, 0x800000, CRC(1c5e2ff1) SHA1(4f29dfd49f6b5c3ca3b758823d368051354bd673) )
	ROM_LOAD( "ss01wave1.1",              0x0800000, 0x800000, CRC(5f4c8861) SHA1(baee7182c32bc064d805de5a16948faf78941ac4) )
ROM_END

ROM_START( ptblank2 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gnb5vera.2l",       0x0000000, 0x200000, CRC(4d0ef3b7) SHA1(6c4077316fa90b734c4a4e0aa3eadd26e97bd6ce) )
	ROM_LOAD16_BYTE( "gnb5vera.2p",       0x0000001, 0x200000, CRC(5d1d19ff) SHA1(4aa8ba7233d7f9bac759c98f53e637c1f3659c3f) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb1prg0l.ic12",    0x0000000, 0x800000, CRC(78746037) SHA1(d130ca1153a730e3c967945248f00662f9fab304) )
	ROM_LOAD16_BYTE( "gnb1prg0u.ic11",    0x0000001, 0x800000, CRC(697d3279) SHA1(40302780f7494d9413888b2d1da38bd14a9a444f) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "gnb_vera.11s", 0x0000000, 0x080000, CRC(d45a53eb) SHA1(01eb4f659b29671d417c5d3684f15a5876f76009) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1waveb.ic2",            0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )
ROM_END

ROM_START( gunbarl )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gnb4vera.2l",       0x0000000, 0x200000, CRC(88c05cde) SHA1(80d210b06c8eda19e37430fb34492885d9eec671) )
	ROM_LOAD16_BYTE( "gnb4vera.2p",       0x0000001, 0x200000, CRC(7d57437a) SHA1(b4fc960b11e7dc9d3bde567f534635f264276e53) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb1prg0l.ic12",    0x0000000, 0x800000, CRC(78746037) SHA1(d130ca1153a730e3c967945248f00662f9fab304) )
	ROM_LOAD16_BYTE( "gnb1prg0u.ic11",    0x0000001, 0x800000, CRC(697d3279) SHA1(40302780f7494d9413888b2d1da38bd14a9a444f) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "gnb_vera.11s", 0x0000000, 0x080000, CRC(d45a53eb) SHA1(01eb4f659b29671d417c5d3684f15a5876f76009) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1waveb.ic2",            0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )
ROM_END

ROM_START( ghlpanic )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ob2vera.2e",        0x0000000, 0x200000, CRC(77162ae0) SHA1(cdc0833756037562b49bb2ae02931b3b24d27329) )
	ROM_LOAD16_BYTE( "ob2vera.2j",        0x0000001, 0x200000, CRC(628f0830) SHA1(a547880674d95b84acc9c05413cba4fd3a81e0cf) )

	ROM_REGION32_LE( 0x0800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "ob1rom0l.ic12",     0x0000000, 0x400000, CRC(f8b6a599) SHA1(2af3186242a8cbf14ab7532496a91041300527e9) )
	ROM_LOAD16_BYTE( "ob1rom0u.ic11",     0x0000001, 0x400000, CRC(0625db92) SHA1(b63be6e41b2c6e2194f02f0c31da1f30c4e08232) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ob1vera.11s",  0x0000000, 0x080000, CRC(f8c459f2) SHA1(681520c891f5c8a0f321652d8834910310c88d1a) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ob1wave.ic2",              0x0000000, 0x800000, CRC(e7bc7202) SHA1(f0f598304866ebe62642eaac6b7d8709baa14fe1) )
ROM_END

ROM_START( ohbakyuun )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ob1vera.2l",        0x0000000, 0x200000, CRC(4cb3c25a) SHA1(18679b9ae54f2b001316d9a55065c859df292c4c) )
	ROM_LOAD16_BYTE( "ob1vera.2p",        0x0000001, 0x200000, CRC(85abbe30) SHA1(0edfa34c45bd95882639a7d3e43bc9df3bc23586) )

	ROM_REGION32_LE( 0x0800000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "ob1rom0l.ic12",     0x0000000, 0x400000, CRC(f8b6a599) SHA1(2af3186242a8cbf14ab7532496a91041300527e9) )
	ROM_LOAD16_BYTE( "ob1rom0u.ic11",     0x0000001, 0x400000, CRC(0625db92) SHA1(b63be6e41b2c6e2194f02f0c31da1f30c4e08232) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ob1vera.11s",  0x0000000, 0x080000, CRC(f8c459f2) SHA1(681520c891f5c8a0f321652d8834910310c88d1a) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ob1wave.ic2",              0x0000000, 0x800000, CRC(e7bc7202) SHA1(f0f598304866ebe62642eaac6b7d8709baa14fe1) )
ROM_END

ROM_START( g13knd )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gls1vera.2e",       0x0000000, 0x200000, CRC(904c39a7) SHA1(e62a518657b639d31e390b8d1a36eee8a46ab179) )
	ROM_LOAD16_BYTE( "gls1vera.2j",       0x0000001, 0x200000, CRC(f8f9d6d2) SHA1(ec02192f3874fea289d123fd6d828148c77fbf6d) )

	ROM_REGION32_LE( 0x3c00000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gls1rom0l.ic10",    0x0000000, 0x800000, CRC(91e04e7e) SHA1(70603ec5c17d9074f7176e7214f7507a3e408578) )
	ROM_LOAD16_BYTE( "gls1rom0u.ic14",    0x0000001, 0x800000, CRC(29f67094) SHA1(d17baa723bca5cadb483cd777d0ef25c29367426) )
	ROM_LOAD16_BYTE( "gls1rom1l.ic11",    0x1000000, 0x800000, CRC(fc69578f) SHA1(20bcd687aeb3741893588b492f93531e58405c3e) )
	ROM_LOAD16_BYTE( "gls1rom1u.ic15",    0x1000001, 0x800000, CRC(db46d626) SHA1(a623097fd17e99ee31c22ed62cb88aa9dc25721c) )
	ROM_LOAD16_BYTE( "gls1rom2l.ic12",    0x2000000, 0x800000, CRC(707cec76) SHA1(59257acc0b5a7f86a895873fc679ea184d2c0a50) )
	ROM_LOAD16_BYTE( "gls1rom2u.ic16",    0x2000001, 0x800000, CRC(970abad0) SHA1(fb41c379d28df2115baee967cc9645da869d2e2d) )
	ROM_LOAD16_BYTE( "gls1fl3l.ic4",      0x3000000, 0x200000, CRC(3a0ffd17) SHA1(03c704012040b9b9c24453baa8dbfbb3aec82854) )
	ROM_LOAD16_BYTE( "gls1fl3u.ic5",      0x3000001, 0x200000, CRC(565567ff) SHA1(36a26ed8fbba95dccbde6300363acb45d9c029b3) )
	ROM_LOAD16_BYTE( "gls1fl4l.ic6",      0x3400000, 0x200000, CRC(b4591cc0) SHA1(8da1defb74957bd02324d5b2d3af406dc1f7061b) )
	ROM_LOAD16_BYTE( "gls1fl4u.ic7",      0x3400001, 0x200000, CRC(679d545c) SHA1(b0e04cb123b4a15d935d773856e9cc954e81ba21) )
	ROM_LOAD16_BYTE( "gls1fl5l.ic8",      0x3800000, 0x200000, CRC(a5581533) SHA1(58721d3acc1a740e616b721b400c6912f21d516f) )
	ROM_LOAD16_BYTE( "gls1fl5u.ic9",      0x3800001, 0x200000, CRC(a8b6832a) SHA1(c0e392b7aa51dfcfce602fb0a46ccbc18074385c) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "gls1vera.11s", 0x0000000, 0x080000, CRC(a1e68d02) SHA1(bcacea12531539776d5ad5d34b64f7f4a10a0b97) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gls1wave0.ic1",            0x0000000, 0x800000, CRC(9de10ef5) SHA1(c391998ed3d882d49d6dbe7079414a2bfd180b49) )
	ROM_LOAD( "gls1wave1.ic13",           0x0800000, 0x800000, CRC(4c92b6bf) SHA1(a463b5825f635efe402a6ea6c1ed8ecdc3d466e0) )
ROM_END

ROM_START( kartduel )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ktd2vera.2l",       0x0000000, 0x200000, CRC(7e7d8454) SHA1(4e0db56e433015cf1b9fd4cad35df99f1730e567) )
	ROM_LOAD16_BYTE( "ktd2vera.2p",       0x0000001, 0x200000, CRC(6c8af7e7) SHA1(602975500180de98a0a7e22c079344e7a3ece25e) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "kdt1rom0l.ic12",    0x0000000, 0x800000, CRC(f8a79aef) SHA1(8c2a6aef5cc0ffe4b5e9f2a98ef5a29edd59eb4d) )
	ROM_LOAD16_BYTE( "kdt1rom0u.ic11",    0x0000001, 0x800000, CRC(bab0d328) SHA1(9a15bfb38c63b0012f29755b2be071e9c82d1c20) )

	ROM_REGION( 0x0000800, REGION_USER4, 0 ) /* factory contents of the AT28C16 */
	ROM_LOAD( "at28c16",                  0x0000000, 0x000800, CRC(1be18a1b) SHA1(3f7f95736dc7b03dfd06e2124518a1828d567303) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ktd1vera.11s", 0x0000000, 0x080000, CRC(c2ff1971) SHA1(32ee2afe08e92049d8139c9324a0ea1a3b7ee5a1) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ktd1wave0.ic2",            0x0000000, 0x800000, CRC(c54d5539) SHA1(17518dc76bb3627ca42bac665938bb11ea926396) )
ROM_END

ROM_START( kartduelja )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "ktd1vera.2l",       0x0000000, 0x200000, CRC(0c207249) SHA1(6c57de25d452226a25f658638d89b81257960741) )
	ROM_LOAD16_BYTE( "ktd1vera.2p",       0x0000001, 0x200000, CRC(f6e2581f) SHA1(06eb108c2775290590dba75f964f26443a585d70) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "kdt1rom0l.ic12",    0x0000000, 0x800000, CRC(f8a79aef) SHA1(8c2a6aef5cc0ffe4b5e9f2a98ef5a29edd59eb4d) )
	ROM_LOAD16_BYTE( "kdt1rom0u.ic11",    0x0000001, 0x800000, CRC(bab0d328) SHA1(9a15bfb38c63b0012f29755b2be071e9c82d1c20) )

	ROM_REGION( 0x0000800, REGION_USER4, 0 ) /* factory contents of the AT28C16 */
	ROM_LOAD( "at28c16",                  0x0000000, 0x000800, CRC(dd9aba8f) SHA1(477e2f319d3e1a61982b27b911bd0264a55634da) )

	ROM_REGION( 0x0080000, REGION_CPU2, 0 ) /* sound prg (H8/3002) */
	ROM_LOAD16_WORD_SWAP( "ktd1vera.11s", 0x0000000, 0x080000, CRC(c2ff1971) SHA1(32ee2afe08e92049d8139c9324a0ea1a3b7ee5a1) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "ktd1wave0.ic2",            0x0000000, 0x800000, CRC(c54d5539) SHA1(17518dc76bb3627ca42bac665938bb11ea926396) )
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
GAMEX( 1997, tekken3a, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (World, TET2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3b, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (World, TET2/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3c, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (World, TET2/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3d, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (World, TET2/VER.D)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3ua, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (US, TET3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3ud, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (US, TET3/VER.D)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3ja, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (Japan, TET1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3jd, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco", "Tekken 3 (Japan, TET1/VER.D)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, tekken3je1, tekken3, coh700s, tekken3, namcos12_h8, ROT0, "Namco",         "Tekken 3 (Japan, TET1/VER.E1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, lbgrande, 0, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco", "Libero Grande (World, LG2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, toukon3, 0, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco/Tomy", "Shin Nihon Pro Wrestling Toukon Retsuden 3 Arcade Edition (Japan, TR1/VER.A)", GAME_IMPERFECT_GRAPHICS )
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
GAMEX( 1998, sws98, 0, coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco", "Super World Stadium '98 (Japan, SS81/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, mdhorse,    0,        coh700s, mdhorse, namcos12_h8alt, ROT0, "MOSS/Namco",    "Derby Quiz My Dream Horse (Japan, MDH1/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, tenkomor, 0, coh700s, namcos12_h8, namcos12_h8b, ROT90, "Namco", "Tenkomori Shooting (World, TKM2/VER.A1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, tenkomorja1, tenkomor, coh700s, namcos12_h8, namcos12_h8b, ROT90, "Namco", "Tenkomori Shooting (Japan, TKM1/VER.A1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, fgtlayer,  0,        coh700s, namcos12_h8, namcos12_h8b, ROT0, "Arika/Namco",   "Fighting Layer (Asia, FTL3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, fgtlayerja, fgtlayer, coh700s, namcos12_h8, namcos12_h8b, ROT0, "Arika/Namco",   "Fighting Layer (Japan, FTL0/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, pacapp,     0,        coh700s, pacapp, namcos12_h8b, ROT0, "Produce/Namco", "Paca Paca Passion (Japan, PPP1/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ptblank2, namcoc76, coh700g, ptblank2, ptblank2, ROT0, "Namco", "Point Blank 2 (World, GNB5/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, gunbarl, ptblank2, coh700g, ptblank2, ptblank2, ROT0, "Namco", "Gunbarl (Japan, GNB4/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, sws99,      0,        coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Super World Stadium '99 (Japan, SS91/VER.A3)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING )
GAMEX( 1999, tektagt,    0,       tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (World, TEG2/VER.C1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtc1a, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco", "Tekken Tag Tournament (World, TEG2/VER.C1, set 2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtub,  tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (US, TEG3/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtuc1, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (US, TEG3/VER.C1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, tektagtja3, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco", "Tekken Tag Tournament (Japan, TEG1/VER.A3)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING )
GAMEX( 1999, tektagtjb, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco", "Tekken Tag Tournament (Japan, TEG1/VER.B)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING )
GAMEX( 1999, tektagtjc1, tektagt, tektagt, namcos12_h8, tektagt, ROT0, "Namco",         "Tekken Tag Tournament (Japan, TEG1/VER.C1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ghlpanic, 0, coh700g, ghlpanic, ptblank2, ROT0, "Eighting/Raizing/Namco", "Ghoul Panic (World, OB2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ohbakyuun, ghlpanic, coh700g, ghlpanic, ptblank2, ROT0, "Eighting/Raizing/Namco", "Oh! Bakyuuun (Japan, OB1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, pacapp2,    0,        coh700s, pacapp2, namcos12_h8b, ROT0, "Produce/Namco", "Paca Paca Passion 2 (Japan, PKS1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, mrdrillr,   0,        coh700s, mrdrillr, namcos12_h8b, ROT0, "Namco",         "Mr. Driller (US, DRI3/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, mrdrillrja2, mrdrillr, coh700s, mrdrillr, namcos12_h8b, ROT0, "Namco",        "Mr. Driller (Japan, DRI1/VER.A2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, kaiunqz,    0,        coh700s, mdhorse, namcos12_h8alt, ROT0, "Namco",         "Kaiun Quiz (Japan, KW1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, pacappsp,   0,        coh700s, pacapp2, namcos12_h8b, ROT0, "Produce/Namco", "Paca Paca Passion Special (Japan, PSP1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, aquarush,   0,        coh700s, namcos12_h8, namcos12_h8, ROT0, "Namco",         "Aqua Rush (Japan, AQ1/VER.A1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, golgo13, 0, coh700g, golgo13, golgo13, ROT0, "Eighting/Raizing/Namco", "Golgo 13 (Japan, GLG1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 2000, sws2000, 0, coh700s, namcos12_h8, namcos12_h8b, ROT0, "Namco", "Super World Stadium 2000 (Japan, SS01/VER.A)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING )
GAMEX( 2000, kartduel, 0, coh700s, kartduel, kartduel, ROT0, "Gaps/Namco", "Kart Duel (World, KTD2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 2000, kartduelja, kartduel, coh700s, kartduel, kartduel, ROT0, "Gaps/Namco", "Kart Duel (Japan, KTD1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 2000, g13knd, 0, coh700g, golgo13, golgo13, ROT0, "Eighting/Raizing/Namco", "Golgo 13 Kiseki no Dandou (Japan, GLS1/VER.A)", GAME_IMPERFECT_GRAPHICS )
