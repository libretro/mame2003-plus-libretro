/***************************************************************************

  Namco System 11 - Arcade PSX Hardware
  =====================================
  Driver by smf & Ryan Holtz
  Board notes by The Guru & Brian A. Troha
  Thanks to R Belmont & The Zinc Team.

  Issues:
   All games run the real C76 (Mitsubishi M37702 with its internal BIOS, c76.bin from
   namcoc76.zip) and the C352: sound, inputs and coins (backported from MAME 0.121 / current
   MAME). Keycus protection, light guns and ROM sets as in current MAME.
   Random draw list corruption in soul edge v2 & dunkmania.
   missing memcard support in soul edge, dunk mania & prime goal ex 
   Most games try to run too fast, lack of root counters and sound timers are likely to blame.

Known Dumps
-----------

Game       Description                             CPU board           Mother board             Daughter board          Keycus   ROM0L

tekken     Tekken (TE4/VER.C)                      COH 100             SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       none     5
tekkena    Tekken (TE2/VER.B)                      COH 100             "                        "                       "        "
tekkenb    Tekken (TE1/VER.B)                      COH 100             "                        "                       "        "
tekken2    Tekken 2 Ver.B (TES3/VER.B)             COH 100             SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       C406     6
tekken2a   Tekken 2 Ver.B (TES2/VER.B)             COH 100             "                        "                       "        "
tekken2b   Tekken 2 (TES2/VER.A)                   COH 100             "                        "                       "        "
souledge   Soul Edge Ver. II (SO4/VER.C)           COH 100 / COH 110   SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       C409     6
souledga   Soul Edge (SO3/VER.A)                   COH 100 / COH 110   "                        "                       "        "
souledgb   Soul Edge (SO1/VER.A)                   COH 100 / COH 110   "                        "                       "        "
dunkmnia   Dunk Mania (DM1/VER.C)                  COH 100 / COH 110   SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       C410     5
xevi3dg    Xevious 3D/G (XV31/VER.A)               COH 100 / COH 110   SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       C430     5
primglex   Prime Goal EX (PG1/VER.A)               COH 100 / COH 110   SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       C411     6
danceyes   Dancing Eyes (DC1/VER.A)                COH 100 / COH 110   SYSTEM11 MOTHER PCB      SYSTEM11 ROM8 PCB       C431     5
starswep   Star Sweep (STP1/VER.A)                 COH 100 / COH 110   SYSTEM11 MOTHER(B) PCB                           C442     -
myangel3   Kosodate Quiz My Angel 3 (KQT1/VER.A)   COH 110             SYSTEM11 MOTHER(B) PCB   SYSTEM11 ROM8(64) PCB   C443     ?

PCB Layout
----------


SYSTEM11 MOTHER PCB 8645960103 (8645970103)
|------------------------------------------|
|                         (CONN1)          |
|               C384     KEYCUS   S11MOT3A |
|                        AT28C16           |
|                       S11MOT2A           |
|                                   PRG.2L |
|J     WAVE.8K                      PRG.2K |
|A                                  PRG.2J |
|M                                  PRG.2F |
|M                                         |
|A                                         |
|                                          |
|       C76     SPROG.6D    C195  61C256   |
|                                 61C256   |
|               S11MOT1A                   |
|                                          |
|     DSW(2)                               |
|                                          |
|             (CONN2)                      |
|                                          |
|------------------------------------------|


Notes:
      S11MOT* = Standard System 11 PALs
      WAVE ROM is 42 pin DIP either 16MBit or if 32MBit it's Byte Mode.
      SPROG ROM type is Intel PA28F200BX 2MBit Flash
      PRG ROM type is Intel E28F008SA 8MBit Flash
      CONN1 is for connection of the ROM Board
      CONN2 is for connection of the CPU board
      There is a REV B Mother board (SYSTEM11 MOTHER(B) PCB) with slightly different
      component placings. It's not known if it has different components also.


ROM Board
---------

SYSTEM11 ROM8 PCB 8645960202 (8645970202)
|----------------------------------------|
|               (CONN1)                  |
|ROM3L.9               74F139    ROM3U.1 |
|                                        |
|ROM1L.8     ROM0L.6       ROM2U.4       |
|      ROM2L.7      ROM0U.5       ROM1U.3|
|                                        |
|----------------------------------------|


Notes:
      This ROM board is wired to accept 8Bit x 32MBit SOP44 MASK ROMs.
      There is another ROM board (ROM8(64) PCB) which is wired to accept 64MBit SOP44 MASK ROMs
      and the IC locations are slightly different also.


CPU Board
---------

GP-11  COH-100  S-L1B 1-655-543-12
|-------------------------------------|
|         SONY           KM48V514BJ-6 |
|         CXD8530AQ      KM48V514BJ-6 |
|              67.737MHz KM48V514BJ-6 |
|                        KM48V514BJ-6 |
|                        KM48V514BJ-6 |
|                        KM48V514BJ-6 |
|         SONY           KM48V514BJ-6 |
|(CONN2)  CXD8538Q       KM48V514BJ-6 |
|              53.69MHz               |
|                                     |
|                                     |
|                       D482445LGW-A70|
|  SONY     D482445LGW-A70            |
|  CXD2923AR            D482445LGW-A70|
|           D482445LGW-A70            |
|-------------------------------------|

CXD8530BQ may be used instead of CXD8530AQ


GP-13  COH-110  S-XMB 1-660-276-11
|-------------------------------------|
|         SONY           KM48V514BJ-6 |
|         CXD8530CQ      KM48V514BJ-6 |
|              67.737MHz KM48V514BJ-6 |
|                        KM48V514BJ-6 |
|                        KM48V514BJ-6 |
|                        KM48V514BJ-6 |
|         SONY           KM48V514BJ-6 |
|(CONN2)  CXD8561Q       KM48V514BJ-6 |
|              53.69MHz               |
|                                     |
|                         Motorola    |
|                         XC44200FUD  |
|    32Meg  32Meg                     |
|                                     |
|                                     |
|-------------------------------------|

32Meg = KM4132G271Q-12


***************************************************************************/

#include "driver.h"
#include "state.h"
#include "cpu/mips/psx.h"
#include "includes/psx.h"
#include "cpu/m37710/m37710.h"
#include "sound/c352.h"

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

static data32_t *namcos11_sharedram;
static data32_t *namcos11_keycus;
static size_t namcos11_keycus_size;

static WRITE32_HANDLER( keycus_w )
{
	verboselog( 1, "keycus_w( %08x, %08x, %08x )\n", offset, data, mem_mask );
	COMBINE_DATA( &namcos11_keycus[ offset ] );
}

/* tekken 2 */
static READ32_HANDLER( keycus_c406_r )
{
	/* todo: verify behaviour */
	data32_t data;

	data = namcos11_keycus[ offset ];
	switch( offset )
	{
	case 0:
		data = ( data & 0xffff0000 ) |
			( ( namcos11_keycus[ 0 ] >> 8 ) & 0xf000 ) |
			( ( namcos11_keycus[ 0 ] >> 16 ) & 0xf00 ) |
			( ( namcos11_keycus[ 1 ] >> 8 ) & 0xf0 ) |
			( ( namcos11_keycus[ 1 ] >> 8 ) & 0xf );
		break;
	}
	verboselog( 1, "keycus_c406_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* soul edge: the C409 blends two 15-bit colours, as in current MAME (ns11prot).
   Words 1 and 3 (p1, p2) and 7 (p3) are written; word 7 reads back p2 and p3
   mixed by p1 (1..0x20). In the 32-bit map, word 2n+1 is the high half of
   dword n. */
static READ32_HANDLER( keycus_c409_r )
{
	data32_t data;

	data = namcos11_keycus[ offset ];
	switch( offset )
	{
	case 3:
		{
			int p1 = namcos11_keycus[ 0 ] >> 16;
			int p2 = namcos11_keycus[ 1 ] >> 16;
			int p3 = namcos11_keycus[ 3 ] >> 16;
			int a2 = ( p1 - 0x01 ) & 0x1f;
			int a3 = ( 0x20 - p1 ) & 0x1f;
			int r = ( ( ( p2 & 0x1f ) * a2 ) + ( ( p3 & 0x1f ) * a3 ) ) / 0x1f;
			int g = ( ( ( ( p2 >> 5 ) & 0x1f ) * a2 ) + ( ( ( p3 >> 5 ) & 0x1f ) * a3 ) ) / 0x1f;
			int b = ( ( ( ( p2 >> 10 ) & 0x1f ) * a2 ) + ( ( ( p3 >> 10 ) & 0x1f ) * a3 ) ) / 0x1f;
			data = ( data & 0x0000ffff ) | ( ( r | ( g << 5 ) | ( b << 10 ) ) << 16 );
		}
		break;
	}
	verboselog( 1, "keycus_c409_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* dunk mania */
static READ32_HANDLER( keycus_c410_r )
{
	data32_t data;
	data32_t n_value;

	if( ( namcos11_keycus[ 0 ] & 0x0000ffff ) != 0xfffe )
	{
		n_value = namcos11_keycus[ 0 ] & 0x0000ffff;
	}
	else
	{
		n_value = 410;
	}

	data = namcos11_keycus[ offset ];
	switch( offset )
	{
	case 0:
		data = ( data & 0x0000ffff ) |
			( ( n_value / 1 ) % 10 );
		break;
	case 1:
		data =
			( ( ( n_value / 10 ) % 10 ) << 24 ) |
			( ( ( n_value / 100 ) % 10 ) << 0 ) |
			( ( ( n_value / 1000 ) % 10 ) << 8 ) |
			( ( ( n_value / 10000 ) % 10 ) << 16 );
		break;
	}
	verboselog( 1, "keycus_c410_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* prime goal ex */
static READ32_HANDLER( keycus_c411_r )
{
	data32_t data;
	data32_t n_value;

	data = namcos11_keycus[ offset ];
	if( ( namcos11_keycus[ 1 ] & 0x0000ffff ) == 0x00007256 )
	{
		n_value = namcos11_keycus[ 5 ] & 0x0000ffff;
	}
	else
	{
		n_value = 411;
	}

	switch( offset )
	{
	case 0:
		data = ( data & 0xffff0000 ) | ( ( ( n_value / 10 ) % 10 ) << 8 ) | ( ( n_value / 1 ) % 10 );
		break;
	case 1:
		data = ( data & 0xffff0000 ) | ( ( ( n_value / 1000 ) % 10 ) << 8 ) | ( ( n_value / 100 ) % 10 );
		break;
	case 4:
		data = ( data & 0xffff0000 ) | ( ( n_value / 10000 ) % 10 );
		break;
	}

	verboselog( 1, "keycus_c411_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* xevious 3d/g */
static READ32_HANDLER( keycus_c430_r )
{
	data32_t data;
	UINT16 n_value;

	if( ( namcos11_keycus[ 2 ] & 0x0000ffff ) == 0x0000e296 )
	{
		n_value = namcos11_keycus[ 0 ] & 0x0000ffff;
	}
	else
	{
		n_value = 430;
	}

	data = namcos11_keycus[ offset ];
	switch( offset )
	{
	case 0:
		data = ( data & 0x0000ffff ) |
			( ( ( n_value / 10000 ) % 10 ) << 16 );
		break;
	case 2:
		data =
			( ( ( n_value / 1000 ) % 10 ) << 8 ) |
			( ( ( n_value / 100 ) % 10 ) << 0 ) |
			( ( ( n_value / 10 ) % 10 ) << 24 ) |
			( ( ( n_value / 1 ) % 10 ) << 16 );
		break;
	}
	verboselog( 1, "keycus_c430_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* dancing eyes */
static READ32_HANDLER( keycus_c431_r )
{
	data32_t data;
	UINT16 n_value;

	if( ( namcos11_keycus[ 0 ] & 0x0000ffff ) == 0x00009e61 )
	{
		n_value = namcos11_keycus[ 6 ] & 0x0000ffff;
	}
	else
	{
		n_value = 431;
	}

	data = namcos11_keycus[ offset ];
	switch( offset )
	{
	case 0:
		data = ( data & 0xffff0000 ) | ( ( ( n_value / 10 ) % 10 ) << 8 ) | ( ( n_value / 1 ) % 10 );
		break;
	case 2:
		data = ( data & 0xffff0000 ) | ( ( ( n_value / 1000 ) % 10 ) << 8 ) | ( ( n_value / 100 ) % 10 );
		break;
	case 4:
		data = ( data & 0xffff0000 ) | ( ( n_value / 10000 ) % 10 );
		break;
	}
	verboselog( 1, "keycus_c431_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* star sweep */
/* pocket racer (current MAME ns11prot.cpp: p1 = word 0, p2 = word 1, p3 = word 3) */
static READ32_HANDLER( keycus_c432_r )
{
	data32_t data = namcos11_keycus[ offset ];
	UINT16 p1 = namcos11_keycus[ 0 ] & 0xffff;
	UINT16 p2 = namcos11_keycus[ 1 ] & 0xffff;
	UINT16 p3 = namcos11_keycus[ 3 ] & 0xffff;

	if( p1 == 0x0000 && ( ( ( p3 == 0x0000 || p3 == 0x00dc ) && p2 == 0xefff ) || p3 == 0x2f15 ) )
	{
		UINT32 n_value = ( p3 != 0x2f15 ) ? 432 : p2;

		switch( offset )
		{
		case 1:
			data = ( data & 0xffff0000 ) | ( ( n_value / 1 ) % 10 ) | ( ( ( n_value / 10 ) % 10 ) << 8 );
			break;
		case 2:
			data = ( data & 0xffff0000 ) | ( ( n_value / 100 ) % 10 ) | ( ( ( n_value / 1000 ) % 10 ) << 8 );
			break;
		case 3:
			data = ( data & 0xffff0000 ) | ( ( n_value / 10000 ) % 10 ) | ( ( ( n_value / 100000 ) % 10 ) << 8 );
			break;
		}
	}
	verboselog( 1, "keycus_c432_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

static READ32_HANDLER( keycus_c442_r )
{
	/* todo: verify behaviour */
	UINT32 data;

	data = namcos11_keycus[ offset ];

	switch( offset )
	{
	case 0:
		if( ( data & 0xffff0000 ) == 0x00210000 )
		{
			data = ( data & 0x0000ffff ) | 0xc4420000;
		}
		break;
	}
	verboselog( 1, "keycus_c442_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

/* kosodate quiz my angel 3 */
/* C443 (current MAME ns11prot.cpp): p1 = word 0, p2 = word 1; unknown states read random values
   (a local generator, saved with the state, keeps runs reproducible) */
static UINT32 keycus_rand_state = 0x12345678;

static UINT16 keycus_rand( void )
{
	keycus_rand_state = keycus_rand_state * 1103515245 + 12345;
	return keycus_rand_state >> 16;
}

static UINT16 keycus_c443_word( int n_word )
{
	UINT16 p1 = namcos11_keycus[ 0 ] & 0xffff;
	UINT16 p2 = namcos11_keycus[ 0 ] >> 16;

	if( n_word == 0 && p1 == 0x0020 && ( p2 == 0x0000 || p2 == 0xffff || p2 == 0xffe0 ) )
	{
		return 0x0020;
	}
	if( n_word == 1 && p1 == 0x0020 && p2 == 0xffdf )
	{
		return 0x0000;
	}
	if( n_word == 1 && p1 == 0x0020 && ( p2 == 0xffff || p2 == 0xffe0 ) )
	{
		return 0xc443;
	}
	return keycus_rand();
}

static READ32_HANDLER( keycus_c443_r )
{
	UINT32 data = 0;

	if( ACCESSING_LSW32 )
	{
		data |= keycus_c443_word( offset * 2 );
	}
	if( ACCESSING_MSW32 )
	{
		data |= keycus_c443_word( offset * 2 + 1 ) << 16;
	}
	verboselog( 1, "keycus_c443_r( %08x, %08x, %08x )\n", offset, data, mem_mask );
	return data;
}

static WRITE32_HANDLER( sharedram_w )
{
	verboselog( 1, "sharedram_w( %08x, %08x, %08x )\n", ( offset * 4 ) + 0x4000, data, mem_mask );
	COMBINE_DATA( &namcos11_sharedram[ offset ] );
}

static READ32_HANDLER( sharedram_r )
{
	verboselog( 1, "sharedram_r( %08x, %08x ) %08x\n", ( offset * 4 ) + 0x4000, mem_mask, namcos11_sharedram[ offset ] );
	return namcos11_sharedram[ offset ];
}

#define SHRAM( x ) namcos11_sharedram[ ( x - 0x4000 ) / 4 ]

static UINT32 m_n_bankoffset;
static UINT32 m_p_n_bankoffset[ 8 ];

static INLINE void bankswitch_update( int n_bank )
{
	verboselog( 1, "bankswitch_update( %d ) = %08x\n", n_bank, m_p_n_bankoffset[ n_bank ] );
	cpu_setbank( 7 + n_bank, memory_region( REGION_USER3 ) + m_p_n_bankoffset[ n_bank ] );
}

static void bankswitch_update_all( void )
{
	int n_bank;

	for( n_bank = 0; n_bank < 8; n_bank++ )
	{
		bankswitch_update( n_bank );
	}
}

static INLINE void bankswitch_rom8( int n_bank, int n_data )
{
	m_p_n_bankoffset[ n_bank ] = ( ( ( n_data & 0xc0 ) >> 4 ) + ( n_data & 0x03 ) ) * 1024 * 1024;
	bankswitch_update( n_bank );
}

static WRITE32_HANDLER( bankswitch_rom32_w )
{
	verboselog( 2, "bankswitch_rom32_w( %08x, %08x, %08x )\n", offset, data, mem_mask );

	if( ACCESSING_LSW32 )
	{
		bankswitch_rom8( ( offset * 2 ), data & 0xffff );
	}
	if( ACCESSING_MSW32 )
	{
		bankswitch_rom8( ( offset * 2 ) + 1, data >> 16 );
	}
}

static WRITE32_HANDLER( bankswitch_rom64_upper_w )
{
	verboselog( 2, "bankswitch_rom64_upper_w( %08x, %08x, %08x )\n", offset, data, mem_mask );

	if( ACCESSING_LSW32 )
	{
		m_n_bankoffset = 0;
	}
	if( ACCESSING_MSW32 )
	{
		m_n_bankoffset = 16;
	}
}

static INLINE void bankswitch_rom64( int n_bank, int n_data )
{
	/* todo: verify behaviour */
	m_p_n_bankoffset[ n_bank ] = ( ( ( ( n_data & 0xc0 ) >> 3 ) + ( n_data & 0x07 ) ) ^ m_n_bankoffset ) * 1024 * 1024;
	bankswitch_update( n_bank );
}

static WRITE32_HANDLER( bankswitch_rom64_w )
{
	verboselog( 2, "bankswitch_rom64_w( %08x, %08x, %08x )\n", offset, data, mem_mask );

	if( ACCESSING_LSW32 )
	{
		bankswitch_rom64( ( offset * 2 ), data & 0xffff );
	}
	if( ACCESSING_MSW32 )
	{
		bankswitch_rom64( ( offset * 2 ) + 1, data >> 16 );
	}
}

static MEMORY_WRITE32_START( namcos11_writemem )
	{ 0x00000000, 0x003fffff, MWA32_RAM },    /* ram */
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
	{ 0x1fa04000, 0x1fa0ffff, sharedram_w, &namcos11_sharedram }, /* shared ram */
	{ 0x1fa20000, 0x1fa2ffff, keycus_w, &namcos11_keycus, &namcos11_keycus_size }, /* keycus */
	{ 0x1fa30000, 0x1fa30fff, MWA32_RAM, (data32_t **)&generic_nvram, &generic_nvram_size }, /* flash */
	{ 0x1fb00000, 0x1fb00003, MWA32_NOP },    /* ?? */
	{ 0x1fbf6000, 0x1fbf6003, MWA32_NOP },    /* ?? */
	{ 0x1fc00000, 0x1fffffff, MWA32_ROM },    /* bios */
	{ 0x80000000, 0x803fffff, MWA32_BANK3 },  /* ram mirror */
	{ 0x9fc00000, 0x9fffffff, MWA32_ROM },    /* bios */
	{ 0xa0000000, 0xa03fffff, MWA32_BANK5 },  /* ram mirror */
	{ 0xbfc00000, 0xbfffffff, MWA32_ROM },    /* bios */
	{ 0xfffe0130, 0xfffe0133, MWA32_NOP },
MEMORY_END

static MEMORY_READ32_START( namcos11_readmem )
	{ 0x00000000, 0x003fffff, MRA32_RAM },    /* ram */
	{ 0x1f000000, 0x1f0fffff, MRA32_BANK7 },  /* banked roms */
	{ 0x1f100000, 0x1f1fffff, MRA32_BANK8 },
	{ 0x1f200000, 0x1f2fffff, MRA32_BANK9 },
	{ 0x1f300000, 0x1f3fffff, MRA32_BANK10 },
	{ 0x1f400000, 0x1f4fffff, MRA32_BANK11 },
	{ 0x1f500000, 0x1f5fffff, MRA32_BANK12 },
	{ 0x1f600000, 0x1f6fffff, MRA32_BANK13 },
	{ 0x1f700000, 0x1f7fffff, MRA32_BANK14 },
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
	{ 0x1fa04000, 0x1fa0ffff, sharedram_r },  /* shared ram */
	{ 0x1fa30000, 0x1fa30fff, MRA32_RAM },    /* flash */
	{ 0x1fc00000, 0x1fffffff, MRA32_BANK2 },  /* bios mirror */
	{ 0x80000000, 0x803fffff, MRA32_BANK3 },  /* ram mirror */
	{ 0x9fc00000, 0x9fffffff, MRA32_BANK4 },  /* bios mirror */
	{ 0xa0000000, 0xa03fffff, MRA32_BANK5 },  /* ram mirror */
	{ 0xbfc00000, 0xbfffffff, MRA32_BANK6 },  /* bios */
MEMORY_END

static struct
{
	const char *s_name;
	mem_read32_handler keycus_r;
	int n_daughterboard;
} namcos11_config_table[] =
{
	{ "tekken", NULL, 32 },
	{ "tekken2", keycus_c406_r, 32 },
	{ "souledge", keycus_c409_r, 32 },
	{ "dunkmnia", keycus_c410_r, 32 },
	{ "xevi3dg",  keycus_c430_r, 32 },
	{ "primglex", keycus_c411_r, 32 },
	{ "danceyes", keycus_c431_r, 32 },
	{ "starswep", keycus_c442_r, 0 },
	{ "myangel3", keycus_c443_r, 64 },
	{ "pocketrc", keycus_c432_r, 32 },
	{ "ptblank2", keycus_c443_r, 64 },
	{ NULL, NULL }
};

static DRIVER_INIT( namcos11 )
{
	int n_game;

	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	cpu_setbank( 2, memory_region( REGION_USER2 ) );
	cpu_setbank( 3, memory_region( REGION_CPU1 ) );
	cpu_setbank( 4, memory_region( REGION_USER2 ) );
	cpu_setbank( 5, memory_region( REGION_CPU1 ) );
	cpu_setbank( 6, memory_region( REGION_USER2 ) );

	n_game = 0;
	while( namcos11_config_table[ n_game ].s_name != NULL )
	{
		if( strcmp( Machine->gamedrv->name, namcos11_config_table[ n_game ].s_name ) == 0 ||
			( Machine->gamedrv->clone_of != NULL && strcmp( Machine->gamedrv->clone_of->name, namcos11_config_table[ n_game ].s_name ) == 0 ) )
		{
			if( namcos11_config_table[ n_game ].keycus_r != NULL )
			{
				install_mem_read32_handler( 0, 0x1fa20000, 0x1fa2ffff, namcos11_config_table[ n_game ].keycus_r );
			}
			if( namcos11_config_table[ n_game ].n_daughterboard != 0 )
			{
				int n_bank;

				m_n_bankoffset = 0;
				for( n_bank = 0; n_bank < 8; n_bank++ )
				{
					m_p_n_bankoffset[ n_bank ] = 0;
				}
				bankswitch_update_all();

				if( namcos11_config_table[ n_game ].n_daughterboard == 32 )
				{
					install_mem_write32_handler( 0, 0x1fa10020, 0x1fa1002f, bankswitch_rom32_w );
				}
				if( namcos11_config_table[ n_game ].n_daughterboard == 64 )
				{
					install_mem_write32_handler( 0, 0x1f080000, 0x1f080003, bankswitch_rom64_upper_w );
					install_mem_write32_handler( 0, 0x1fa10020, 0x1fa1002f, bankswitch_rom64_w );
					install_mem_read32_handler( 0, 0x1fa10020, 0x1fa1002f, MRA32_NOP );
				}
				state_save_register_UINT32( "namcos11", 0, "m_n_bankoffset", &m_n_bankoffset, 1 );
				state_save_register_UINT32( "namcos11", 0, "m_p_n_bankoffset", &m_p_n_bankoffset[ 0 ], 8 );
				state_save_register_func_postload( bankswitch_update_all );
			}
			else
			{
				install_mem_write32_handler( 0, 0x1fa10020, 0x1fa1002f, MWA32_NOP );
			}
			break;
		}
		n_game++;
	}

	state_save_register_UINT32( "namcos11", 0, "keycus_rand_state", &keycus_rand_state, 1 );
	psx_driver_init();
	psx_gpu_set_thread( options.psx_gpu_thread );	/* PSX GPU drawing thread (core option) */
}

MACHINE_INIT( namcos11 )
{
	memset( namcos11_keycus, 0, namcos11_keycus_size );
	keycus_rand_state = 0x12345678;
	psx_machine_init();
	psx_machine_cpu_speedups();
}


/***************************************************************************

  C76 (Mitsubishi M37702 with 16KB internal BIOS) and C352
  (backported from MAME 0.121 namcoc7x / current MAME namcos11)

  The C76 runs its internal BIOS (c76.bin) and the sound program
  (SPROG.6D): it drives the C352 and handles inputs and coins, talking
  to the main CPU through the 48KB shared RAM.

***************************************************************************/

/* the PSX sees the shared RAM (0x1fa04000-0x1fa0ffff) as 32-bit LE, the C76 as 16-bit words */
#define SHRAM16( x ) ( ( (data16_t *)namcos11_sharedram )[ WORD_XOR_LE( ( x ) * 2 ) / 2 ] )

static READ16_HANDLER( c76_shared_r )
{
	return SHRAM16( offset );
}

static WRITE16_HANDLER( c76_shared_w )
{
	COMBINE_DATA( &SHRAM16( offset ) );
}

static READ16_HANDLER( c76_rom_r )
{
	return ( (data16_t *)memory_region( REGION_CPU2 ) )[ offset & 0x3ffff ];
}

/* input port indices of INPUT_PORTS( tekken ) */
#define C76_PORT_SWITCH  0
#define C76_PORT_PLAYER1 1
#define C76_PORT_PLAYER2 2
#define C76_PORT_PLAYER4 3
#define C76_PORT_ADC0    4

static READ16_HANDLER( c76_player4_r ) { return readinputport( C76_PORT_PLAYER4 ) & 0xff; }
static READ16_HANDLER( c76_switch_r )  { return readinputport( C76_PORT_SWITCH ) & 0xff; }
static READ16_HANDLER( c76_player1_r ) { return readinputport( C76_PORT_PLAYER1 ) & 0xff; }
static READ16_HANDLER( c76_player2_r ) { return readinputport( C76_PORT_PLAYER2 ) & 0xff; }

static MEMORY_READ16_START( c76_readmem )
	{ 0x001000, 0x001001, c76_player4_r },
	{ 0x001002, 0x001003, c76_switch_r },
	{ 0x001004, 0x001005, c76_player1_r },
	{ 0x001006, 0x001007, c76_player2_r },
	{ 0x002000, 0x002fff, c352_0_r },
	{ 0x004000, 0x00bfff, c76_shared_r },
	{ 0x080000, 0x0fffff, c76_rom_r },
	{ 0x200000, 0x27ffff, c76_rom_r },
	{ 0x280000, 0x2fffff, c76_rom_r },
MEMORY_END

static MEMORY_WRITE16_START( c76_writemem )
	{ 0x002000, 0x002fff, c352_0_w },
	{ 0x004000, 0x00bfff, c76_shared_w },
	{ 0x300000, 0x300001, MWA16_NOP },
	{ 0x301000, 0x301001, MWA16_NOP },	/* watchdog? LEDs? */
	{ 0x308000, 0x308003, MWA16_NOP },	/* volume control IC? */
MEMORY_END

static data8_t c76_io_r( int port )
{
	if( port >= M37710_ADC0_L && port <= M37710_ADC7_H )
	{
		/* ADC inputs: 8-bit value in the low register */
		if( ( ( port - M37710_ADC0_L ) & 1 ) == 0 )
		{
			return readinputport( C76_PORT_ADC0 + ( ( port - M37710_ADC0_L ) >> 1 ) ) & 0xff;
		}
		return 0;
	}

	/* the on-chip ports are not wired: inputs float high */
	return 0xff;
}

static void c76_io_w( int port, data8_t data )
{
}

static INTERRUPT_GEN( c76_interrupt )
{
	if( cpu_getiloops() == 0 )
		cpu_set_irq_line( 1, M37710_LINE_IRQ0, HOLD_LINE );
	else
		cpu_set_irq_line( 1, M37710_LINE_IRQ2, HOLD_LINE );
}

static INTERRUPT_GEN( namcos11_vblank_c76 )
{
	/* inputs, coins and sound are handled by the C76 */
	psx_vblank();
}

static struct C352interface c76_c352_interface =
{
	25401600,		/* clock */
	288,			/* divider */
	REGION_SOUND1,
	100
};

static DRIVER_INIT( namcos11_c76 )
{
	init_namcos11();

	m37710_set_io_handlers( c76_io_r, c76_io_w );
	m37710_set_internal_rom( memory_region( REGION_USER4 ) );
	/* the C76 BIOS waits for its interrupts in "lda $83 / beq" (c151) */
	m37710_set_idle_loop( 0xc151, 2 );
}

/* Point Blank 2 / Gunbarl light guns (current MAME): X 0xd8-0x387, Y 0x2c-0x11b;
   the 8-bit input ports are scaled to that range */
#define C76_PORT_GUN1X   12

static UINT16 ptblank2_gun_x( int n_gun )
{
	return 0xd8 + ( readinputport( C76_PORT_GUN1X + n_gun * 2 ) & 0xff ) * ( 0x387 - 0xd8 ) / 0xff;
}

static UINT16 ptblank2_gun_y( int n_gun )
{
	return 0x2c + ( readinputport( C76_PORT_GUN1X + n_gun * 2 + 1 ) & 0xff ) * ( 0x11b - 0x2c ) / 0xff;
}

static READ32_HANDLER( ptblank2_lightgun_r )
{
	data32_t data = 0;

	switch( offset )
	{
	case 0: data = ptblank2_gun_x( 0 ); break;
	case 1: data = ptblank2_gun_y( 0 ) | ( ( ptblank2_gun_y( 0 ) + 1 ) << 16 ); break;
	case 2: data = ptblank2_gun_x( 1 ); break;
	case 3: data = ptblank2_gun_y( 1 ) | ( ( ptblank2_gun_y( 1 ) + 1 ) << 16 ); break;
	}
	return data;
}

static DRIVER_INIT( ptblank2 )
{
	init_namcos11_c76();

	install_mem_read32_handler( 0, 0x1f780000, 0x1f78000f, ptblank2_lightgun_r );
	install_mem_write32_handler( 0, 0x1f788000, 0x1f788003, MWA32_NOP ); /* lamps and recoil */
}

/* System 11 with the C76 sound/I-O MCU and the C352 */
static MACHINE_DRIVER_START( coh100_c76 )
	/* basic machine hardware */
	MDRV_CPU_ADD( PSXCPU, 33868800 / 2 ) /* 33MHz ?? */
	MDRV_CPU_MEMORY( namcos11_readmem, namcos11_writemem )
	MDRV_CPU_VBLANK_INT( namcos11_vblank_c76, 1 )

	MDRV_CPU_ADD( M37702, 16934400 )
	MDRV_CPU_MEMORY( c76_readmem, c76_writemem )
	MDRV_CPU_VBLANK_INT( c76_interrupt, 2 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )
	MDRV_INTERLEAVE( 40 )

	MDRV_MACHINE_INIT( namcos11 )
	MDRV_NVRAM_HANDLER( generic_0fill )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
#if defined( MAME_DEBUG )
	MDRV_SCREEN_SIZE( 1024, 1024 )
	MDRV_VISIBLE_AREA( 0, 1023, 0, 1023 )
#else
	MDRV_SCREEN_SIZE( 640, 240 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 239 )
#endif
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type1_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( C352, c76_c352_interface )
MACHINE_DRIVER_END

/* the same on the COH-110 motherboard (CXD8561Q GPU) */
static MACHINE_DRIVER_START( coh110_c76 )
	MDRV_IMPORT_FROM( coh100_c76 )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
MACHINE_DRIVER_END

INPUT_PORTS_START( tekken )
	/* SWITCH */
	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x02, 0x02, "DIP1 (Test)" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, "DIP2 (Freeze)" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* PLAYER1 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	/* PLAYER2 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	/* PLAYER4 (P2 kicks) */
	PORT_START
	PORT_BIT( 0xcf, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	/* ADC0 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC1 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	/* ADC2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	/* ADC3 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC5 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC6 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC7 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* Soul Edge on the C76 (current MAME layout): P1 B4 on ADC2, P2 B4 on PLAYER4 bit 4 */
INPUT_PORTS_START( souledge )
	/* SWITCH */
	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x02, 0x02, "DIP1 (Test)" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, "DIP2 (Freeze)" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* PLAYER1 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	/* PLAYER2 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	/* PLAYER4 (P2 button 4) */
	PORT_START
	PORT_BIT( 0xef, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	/* ADC0 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC1 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	/* ADC3 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC5 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC6 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC7 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* System 11 on the C76 (current MAME layout): player 3 on the ADC inputs, player 4 on PLAYER4 */
INPUT_PORTS_START( namcos11_c76 )
	/* SWITCH */
	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_DIPNAME( 0x02, 0x02, "DIP1 (Test)" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, "DIP2 (Freeze)" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* PLAYER1 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	/* PLAYER2 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	/* PLAYER4 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	/* ADC0 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	/* ADC1 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	/* ADC2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	/* ADC3 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	/* ADC4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER3 )
	/* ADC5 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER3 )
	/* ADC6 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER3 )
	/* ADC7 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

/* Pocket Racer on the C76 (current MAME layout): steering on ADC0, accelerator on ADC1 */
INPUT_PORTS_START( pocketrc )
	/* SWITCH */
	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x1c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x02, 0x02, "DIP1 (Test)" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, "DIP2 (Freeze)" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* PLAYER1 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	/* PLAYER2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	/* PLAYER4 */
	PORT_START
	PORT_BIT( 0xf7, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 ) /* toggle view */

	/* ADC0 */
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE | IPF_REVERSE | IPF_PLAYER1, 100, 10, 0x38, 0xc8 )
	/* ADC1 */
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_REVERSE | IPF_PLAYER1, 100, 10, 0x00, 0x7f )
	/* ADC2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC3 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC5 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC6 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC7 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* Point Blank 2 / Gunbarl on the C76 (current MAME layout): triggers on PLAYER1/2 bit 4,
   gun positions read by the main CPU at 0x1f780000 */
INPUT_PORTS_START( ptblank2 )
	/* SWITCH */
	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_DIPNAME( 0x02, 0x02, "DIP1 (Test)" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, "DIP2 (Freeze)" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* PLAYER1 */
	PORT_START
	PORT_BIT( 0x6f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	/* PLAYER2 */
	PORT_START
	PORT_BIT( 0x6f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	/* PLAYER4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	/* ADC0 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC1 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC3 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC5 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC6 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC7 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	/* GUN1X, GUN1Y, GUN2X, GUN2Y */
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 50, 15, 0, 0xff )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 50, 15, 0, 0xff )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 50, 15, 0, 0xff )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 50, 15, 0, 0xff )
INPUT_PORTS_END

/* My Angel 3 on the C76 (current MAME layout): four answer buttons per player */
INPUT_PORTS_START( myangel3 )
	/* SWITCH */
	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x02, 0x02, "DIP1 (Test)" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, "DIP2 (Freeze)" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* PLAYER1 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	/* PLAYER2 */
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x70, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	/* PLAYER4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	/* ADC0 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC1 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC2 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC3 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC4 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC5 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC6 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	/* ADC7 */
	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* C76 internal BIOS (Mitsubishi M37702 mask ROM), shared by all System 11 games,
   searched in namcoc76.zip like the current MAME device ROM */
#define NAMCO_C76_BIOS \
	ROM_REGION( 0x4000, REGION_USER4, 0 ) \
	ROM_LOAD( "c76.bin", 0x0000, 0x4000, CRC(399faac7) SHA1(ceb184ef0486caf715dd997101999785f67a40b8) )

ROM_START( namcoc76 )
	NAMCO_C76_BIOS
ROM_END

ROM_START( danceyes ) /* Marked as DC1/VER.A but the 1 was crossed out and a handwritten "2" was added */
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dc2verb.2l",   0x0000000, 0x100000, CRC(9f605dd9) SHA1(67a13a9195b766e66fe581099ea3d4615b8532b2) )
	ROM_LOAD16_BYTE( "dc2verb.2j",   0x0000001, 0x100000, CRC(210d7e22) SHA1(1aa76f5a9c2bbddeb7880df7e98287e29e8dea05) )
	ROM_LOAD16_BYTE( "dc1vera.2k",   0x0200000, 0x100000, CRC(bdd9484e) SHA1(2f85e0ac4b12b2cf4c9717fad745d2d68c27d39a) )
	ROM_LOAD16_BYTE( "dc1vera.2f",   0x0200001, 0x100000, CRC(25a2f06f) SHA1(400806a262681cf908ac16e039686b5a0d5fd58e) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dc1rom0l.ic5", 0x0000000, 0x200000, CRC(8b5b4b13) SHA1(9681e29ad95a2cc555d0fbb558a0865a87a77268) )
	ROM_LOAD16_BYTE( "dc1rom0u.ic6", 0x0000001, 0x200000, CRC(93ca9bd0) SHA1(c7d2ecbeb451bd57097fb39f21dc347b86a2b838) )
	ROM_LOAD16_BYTE( "dc1rom1l.ic3", 0x0400000, 0x200000, CRC(380e0282) SHA1(33389e3b15b93fb939991b8d7dc8b182ba88e78a) )
	ROM_LOAD16_BYTE( "dc1rom1u.ic8", 0x0400001, 0x200000, CRC(47d966a7) SHA1(3b6f004136bd9d75dbef846f417ca34a56893d61) )
	ROM_LOAD16_BYTE( "dc1rom2l.ic4", 0x0800000, 0x200000, CRC(8f130220) SHA1(7f086d502178c27511c1142254381a9a2a999e8d) )
	ROM_LOAD16_BYTE( "dc1rom2u.ic7", 0x0800001, 0x200000, CRC(24514dc6) SHA1(f9f7d6a45cbd51513cc038f6321a30c1a72b7a58) )
	ROM_LOAD16_BYTE( "dc1rom3l.ic1", 0x0c00000, 0x200000, CRC(a76bcd4c) SHA1(817abdc43158b7aaac329c3ea17782277acb36a4) )
	ROM_LOAD16_BYTE( "dc1rom3u.ic9", 0x0c00001, 0x200000, CRC(1405d123) SHA1(3d7be5558358740f5a0a3a3022543cf5aca4cf24) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "dc1sprog.6d",  0x0000000, 0x040000, CRC(96cd7788) SHA1(68a5a53a5fc50e2b6b684c99d27d81e3a8c56287) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dc1wave.8k",   0x000000, 0x400000, CRC(8ba0f6a7) SHA1(e9868debd808e92b196d1baeeeae9c4855356a01) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( danceyesu )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dc3verc.2l",   0x0000000, 0x100000, CRC(a7a00bc6) SHA1(9e51e28fee1f3f5bf212d1c2c07c674487a61ed3) )
	ROM_LOAD16_BYTE( "dc3verc.2j",   0x0000001, 0x100000, CRC(02fc2415) SHA1(cfe597302e2d322c1068a2a3b3ab27efd6154c72) )
	ROM_LOAD16_BYTE( "dc1vera.2k",   0x0200000, 0x100000, CRC(bdd9484e) SHA1(2f85e0ac4b12b2cf4c9717fad745d2d68c27d39a) )
	ROM_LOAD16_BYTE( "dc1vera.2f",   0x0200001, 0x100000, CRC(25a2f06f) SHA1(400806a262681cf908ac16e039686b5a0d5fd58e) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dc1rom0l.ic5", 0x0000000, 0x200000, CRC(8b5b4b13) SHA1(9681e29ad95a2cc555d0fbb558a0865a87a77268) )
	ROM_LOAD16_BYTE( "dc1rom0u.ic6", 0x0000001, 0x200000, CRC(93ca9bd0) SHA1(c7d2ecbeb451bd57097fb39f21dc347b86a2b838) )
	ROM_LOAD16_BYTE( "dc1rom1l.ic3", 0x0400000, 0x200000, CRC(380e0282) SHA1(33389e3b15b93fb939991b8d7dc8b182ba88e78a) )
	ROM_LOAD16_BYTE( "dc1rom1u.ic8", 0x0400001, 0x200000, CRC(47d966a7) SHA1(3b6f004136bd9d75dbef846f417ca34a56893d61) )
	ROM_LOAD16_BYTE( "dc1rom2l.ic4", 0x0800000, 0x200000, CRC(8f130220) SHA1(7f086d502178c27511c1142254381a9a2a999e8d) )
	ROM_LOAD16_BYTE( "dc1rom2u.ic7", 0x0800001, 0x200000, CRC(24514dc6) SHA1(f9f7d6a45cbd51513cc038f6321a30c1a72b7a58) )
	ROM_LOAD16_BYTE( "dc1rom3l.ic1", 0x0c00000, 0x200000, CRC(a76bcd4c) SHA1(817abdc43158b7aaac329c3ea17782277acb36a4) )
	ROM_LOAD16_BYTE( "dc1rom3u.ic9", 0x0c00001, 0x200000, CRC(1405d123) SHA1(3d7be5558358740f5a0a3a3022543cf5aca4cf24) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "dc1sprog.6d",  0x0000000, 0x040000, CRC(96cd7788) SHA1(68a5a53a5fc50e2b6b684c99d27d81e3a8c56287) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dc1wave.8k",   0x000000, 0x400000, CRC(8ba0f6a7) SHA1(e9868debd808e92b196d1baeeeae9c4855356a01) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( danceyesj )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dc1vera.2l",   0x0000000, 0x100000, CRC(b164ad67) SHA1(62a7f9bc7fb9c218e5031598512dbd1e013283b3) )
	ROM_LOAD16_BYTE( "dc1vera.2j",   0x0000001, 0x100000, CRC(28e4cb3d) SHA1(17923c66725da1f8e77b7c08d8017160bba53eb9) )
	ROM_LOAD16_BYTE( "dc1vera.2k",   0x0200000, 0x100000, CRC(bdd9484e) SHA1(2f85e0ac4b12b2cf4c9717fad745d2d68c27d39a) )
	ROM_LOAD16_BYTE( "dc1vera.2f",   0x0200001, 0x100000, CRC(25a2f06f) SHA1(400806a262681cf908ac16e039686b5a0d5fd58e) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dc1rom0l.ic5", 0x0000000, 0x200000, CRC(8b5b4b13) SHA1(9681e29ad95a2cc555d0fbb558a0865a87a77268) )
	ROM_LOAD16_BYTE( "dc1rom0u.ic6", 0x0000001, 0x200000, CRC(93ca9bd0) SHA1(c7d2ecbeb451bd57097fb39f21dc347b86a2b838) )
	ROM_LOAD16_BYTE( "dc1rom1l.ic3", 0x0400000, 0x200000, CRC(380e0282) SHA1(33389e3b15b93fb939991b8d7dc8b182ba88e78a) )
	ROM_LOAD16_BYTE( "dc1rom1u.ic8", 0x0400001, 0x200000, CRC(47d966a7) SHA1(3b6f004136bd9d75dbef846f417ca34a56893d61) )
	ROM_LOAD16_BYTE( "dc1rom2l.ic4", 0x0800000, 0x200000, CRC(8f130220) SHA1(7f086d502178c27511c1142254381a9a2a999e8d) )
	ROM_LOAD16_BYTE( "dc1rom2u.ic7", 0x0800001, 0x200000, CRC(24514dc6) SHA1(f9f7d6a45cbd51513cc038f6321a30c1a72b7a58) )
	ROM_LOAD16_BYTE( "dc1rom3l.ic1", 0x0c00000, 0x200000, CRC(a76bcd4c) SHA1(817abdc43158b7aaac329c3ea17782277acb36a4) )
	ROM_LOAD16_BYTE( "dc1rom3u.ic9", 0x0c00001, 0x200000, CRC(1405d123) SHA1(3d7be5558358740f5a0a3a3022543cf5aca4cf24) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "dc1sprog.6d",  0x0000000, 0x040000, CRC(96cd7788) SHA1(68a5a53a5fc50e2b6b684c99d27d81e3a8c56287) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dc1wave.8k",   0x000000, 0x400000, CRC(8ba0f6a7) SHA1(e9868debd808e92b196d1baeeeae9c4855356a01) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( dunkmnia )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dm2verc.2l",   0x0000000, 0x100000, CRC(f6a6c46e) SHA1(5552c3a8fb0ef18e6f0bca8d81ec68b904824a66) )
	ROM_LOAD16_BYTE( "dm2verc.2j",   0x0000001, 0x100000, CRC(1df539ce) SHA1(89ac436c685cf26899168c73fdaccee778fd7d2d) )
	ROM_LOAD16_BYTE( "dm1verc.2k",   0x0200000, 0x100000, CRC(c8d72f78) SHA1(30341301f0d1cdcb14f3e0672ec1165c0583fbc8) )
	ROM_LOAD16_BYTE( "dm1verc.2f",   0x0200001, 0x100000, CRC(d379dfa9) SHA1(142cb70b5ea060c961c5bc60a624643b5ec390df) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dm1rom0l.ic5", 0x0000000, 0x200000, CRC(4bb5d71d) SHA1(7d61211c7a6d1f6593604390fe99206a6a8cc7b3) )
	ROM_CONTINUE( 0x0000000, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "dm1rom0u.ic6", 0x0000001, 0x200000, CRC(c16b47c5) SHA1(0fb2c5bc4674b3366762127c6333fb3a837b4de2) )
	ROM_CONTINUE( 0x0000001, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "dm1rom1l.ic3", 0x0400000, 0x200000, CRC(20dd3294) SHA1(b2fd5075b6281ac7bfc2681fc282f9ebaa089af5) )
	ROM_CONTINUE( 0x0400000, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "dm1rom1u.ic8", 0x0400001, 0x200000, CRC(01e905d3) SHA1(430b2ae0c67265b6acc8aa4dd50f6144929993f8) )
	ROM_CONTINUE( 0x0400001, 0x200000 ) /* first & second half identical */

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "dm1sprog.6d",  0x0000000, 0x040000, CRC(de1cbc78) SHA1(855ebece1841f50ae324d7d6b8b18ab6f657d28e) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dm1wave.8k",   0x000000, 0x400000, CRC(883d7455) SHA1(d2129d7c8b981128c3d0ce0c56fd0d5d58d5d2d9) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( dunkmniajc )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "dm1verc.2l",   0x0000000, 0x100000, CRC(6c81654a) SHA1(00e84cc41b3dd49a8f0d3a364ea4a5d2662c45ff) )
	ROM_LOAD16_BYTE( "dm1verc.2j",   0x0000001, 0x100000, CRC(10329b7e) SHA1(d214764e90c7d79abea01580e79092e34a58b695) )
	ROM_LOAD16_BYTE( "dm1verc.2k",   0x0200000, 0x100000, CRC(c8d72f78) SHA1(30341301f0d1cdcb14f3e0672ec1165c0583fbc8) )
	ROM_LOAD16_BYTE( "dm1verc.2f",   0x0200001, 0x100000, CRC(d379dfa9) SHA1(142cb70b5ea060c961c5bc60a624643b5ec390df) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "dm1rom0l.ic5", 0x0000000, 0x200000, CRC(4bb5d71d) SHA1(7d61211c7a6d1f6593604390fe99206a6a8cc7b3) )
	ROM_CONTINUE( 0x0000000, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "dm1rom0u.ic6", 0x0000001, 0x200000, CRC(c16b47c5) SHA1(0fb2c5bc4674b3366762127c6333fb3a837b4de2) )
	ROM_CONTINUE( 0x0000001, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "dm1rom1l.ic3", 0x0400000, 0x200000, CRC(20dd3294) SHA1(b2fd5075b6281ac7bfc2681fc282f9ebaa089af5) )
	ROM_CONTINUE( 0x0400000, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "dm1rom1u.ic8", 0x0400001, 0x200000, CRC(01e905d3) SHA1(430b2ae0c67265b6acc8aa4dd50f6144929993f8) )
	ROM_CONTINUE( 0x0400001, 0x200000 ) /* first & second half identical */

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "dm1sprog.6d",  0x0000000, 0x040000, CRC(de1cbc78) SHA1(855ebece1841f50ae324d7d6b8b18ab6f657d28e) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "dm1wave.8k",   0x000000, 0x400000, CRC(883d7455) SHA1(d2129d7c8b981128c3d0ce0c56fd0d5d58d5d2d9) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( myangel3 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD( "kqt1vera.1j",         0x0000000, 0x200000, CRC(df7aef8a) SHA1(d4ff144bcdecc1d4a3b834d0b9c182609ad9b260) )
	ROM_LOAD( "kqt1vera.1l",         0x0200000, 0x200000, CRC(ffc51c01) SHA1(bba2c2c1ad31039c7dc7413e51e7fc317451e1e3) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "kqt1prg0l.ic2", 0x0000000, 0x800000, CRC(d67eee66) SHA1(1842e987ae406ac2cf4c173aaaa73b5f67f4fd3d) )
	ROM_LOAD16_BYTE( "kqt1prg0u.ic5", 0x0000001, 0x800000, CRC(4d1c7bf3) SHA1(6bf9712264a82920f7898896643d05a6b425e74a) )
	ROM_LOAD16_BYTE( "kqt1prg1l.ic3", 0x1000000, 0x800000, CRC(298d8eeb) SHA1(c421b1bdd5fd46c026a41e2cec47cafd1a69d33d) )
	ROM_LOAD16_BYTE( "kqt1prg1u.ic6", 0x1000001, 0x800000, CRC(911783db) SHA1(1005fc9b38e212844e397150a6f98f43ad88d4b9) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "kqt1sprog.7e", 0x0000000, 0x040000, CRC(bb1888a6) SHA1(4db07738079725413cdba7eb75252ee71ae50a66) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "kqt1wave.8k",  0x000000, 0x400000, CRC(92ca8e4f) SHA1(48d6bdfcc5de1c280afa36c3f0dd6d4177771355) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( pocketrc )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "pkr1verb.2l",  0x000000, 0x100000, CRC(300d906a) SHA1(f521858f78284d69e6a6ec2c35a468c87dd0628c) )
	ROM_LOAD16_BYTE( "pkr1verb.2j",  0x000001, 0x100000, CRC(d5f47526) SHA1(b1d5b6f714510d0cfcc9b32708287755f9f27ead) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "pkr1rom0l.ic5", 0x000000, 0x200000, CRC(6c9b074c) SHA1(885f342bd178e4146e1f75259206f6625c0b3c18) )
	ROM_LOAD16_BYTE( "pkr1rom0u.ic6", 0x000001, 0x200000, CRC(a55c0906) SHA1(3b6abfa877f88a4d96222d98af02498b0c777af6) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "pkr1verb.6d",  0x000000, 0x040000, CRC(9bf08992) SHA1(fca7943f7bcf0ee758fa63fbdef8f7456b9e46cb) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "pkr1wave.8k",  0x000000, 0x400000, CRC(72517c46) SHA1(d0dcc750fe8eca9e965e7c366ac39a42ffd76557) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

/* container for the Point Blank 2 sets (in current MAME the parent is the System 10 version) */
ROM_START( ptblank2 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */
	NAMCO_C76_BIOS
ROM_END

ROM_START( ptblank2a )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gnb2vera.2l",  0x0000000, 0x100000, CRC(4926599d) SHA1(acb5e37d5f5d9e9ade0e92c9574cccdd0f7388e0) )
	ROM_LOAD16_BYTE( "gnb2vera.2j",  0x0000001, 0x100000, CRC(2aba8c09) SHA1(18c31f4bde3b90ef7b3ca7cc07da4a3c146fa2c1) )
	ROM_LOAD16_BYTE( "gnb1vera.2k",  0x0200000, 0x100000, CRC(e6335e4e) SHA1(9067f05d848c1c8a88967a3c6552d2d24e80672b) )
	ROM_LOAD16_BYTE( "gnb1vera.2f",  0x0200001, 0x100000, CRC(2bb7eb6d) SHA1(d1b1e031a28443140ac8652dfd77a65a042b67fc) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb2prg.1",    0x0000000, 0x400000, CRC(8a8e77c3) SHA1(1a37e04a0acd1ab8c5fcbf807f24fd22f1d90a82) ) // == same data as the 64Mbit ROMs
	ROM_LOAD16_BYTE( "gnb2prg.2",    0x0000001, 0x400000, CRC(563edc3f) SHA1(d691560bded88fe7738de01b293f1e761ab9304c) )
	ROM_LOAD16_BYTE( "gnb2prg.3",    0x0800000, 0x400000, CRC(94fbe733) SHA1(74634c3680d22697c1cc3059c2bbe1703e77ddf1) )
	ROM_LOAD16_BYTE( "gnb2prg.4",    0x0800001, 0x400000, CRC(1cbe79a6) SHA1(46e9f72c121ece3457b2f66413489ce6568e5510) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "gnb1vera.6d",  0x0000000, 0x040000, CRC(6461ae77) SHA1(1377b716a69ef9d4d2e48083d23f22bd5c103c00) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1wave.8k",  0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( ptblank2b )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD( "gnb2vera.1j",         0x0000000, 0x200000, CRC(df0bc5f0) SHA1(ccd5cac2c8cef73dae971d256afec58d4b897430) ) // == gnb2vera.2l + gnb2vera.2j interleaved
	ROM_LOAD( "gnb2vera.1l",         0x0200000, 0x200000, CRC(8a274d96) SHA1(9ae0932e5dba2a052dc4977c76bde2e0c5f39d54) ) // == gnb1vera.2k + gnb1vera.2f interleaved

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb2prg.1",    0x0000000, 0x400000, CRC(8a8e77c3) SHA1(1a37e04a0acd1ab8c5fcbf807f24fd22f1d90a82) ) // == same data as the 64Mbit ROMs
	ROM_LOAD16_BYTE( "gnb2prg.2",    0x0000001, 0x400000, CRC(563edc3f) SHA1(d691560bded88fe7738de01b293f1e761ab9304c) )
	ROM_LOAD16_BYTE( "gnb2prg.3",    0x0800000, 0x400000, CRC(94fbe733) SHA1(74634c3680d22697c1cc3059c2bbe1703e77ddf1) )
	ROM_LOAD16_BYTE( "gnb2prg.4",    0x0800001, 0x400000, CRC(1cbe79a6) SHA1(46e9f72c121ece3457b2f66413489ce6568e5510) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "gnb1vera.7e",  0x0000000, 0x040000, CRC(6461ae77) SHA1(1377b716a69ef9d4d2e48083d23f22bd5c103c00) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1wave.8k",  0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( ptblank2c )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gnb2verx.2l",  0x0000000, 0x100000, CRC(c2a02ccf) SHA1(fa637ecacc3e72d432c91dbff7d59fe56f28bab2) )
	ROM_LOAD16_BYTE( "gnb2verx.2j",  0x0000001, 0x100000, CRC(96abd746) SHA1(13720d02ef9e7445422c07836296acde4db4aa50) )
	ROM_LOAD16_BYTE( "gnb1vera.2k",  0x0200000, 0x100000, CRC(e6335e4e) SHA1(9067f05d848c1c8a88967a3c6552d2d24e80672b) )
	ROM_LOAD16_BYTE( "gnb1vera.2f",  0x0200001, 0x100000, CRC(2bb7eb6d) SHA1(d1b1e031a28443140ac8652dfd77a65a042b67fc) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb2prg.1",    0x0000000, 0x400000, CRC(8a8e77c3) SHA1(1a37e04a0acd1ab8c5fcbf807f24fd22f1d90a82) ) // == same data as the 64Mbit ROMs
	ROM_LOAD16_BYTE( "gnb2prg.2",    0x0000001, 0x400000, CRC(563edc3f) SHA1(d691560bded88fe7738de01b293f1e761ab9304c) )
	ROM_LOAD16_BYTE( "gnb2prg.3",    0x0800000, 0x400000, CRC(94fbe733) SHA1(74634c3680d22697c1cc3059c2bbe1703e77ddf1) )
	ROM_LOAD16_BYTE( "gnb2prg.4",    0x0800001, 0x400000, CRC(1cbe79a6) SHA1(46e9f72c121ece3457b2f66413489ce6568e5510) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "gnb1vera.6d",  0x0000000, 0x040000, CRC(6461ae77) SHA1(1377b716a69ef9d4d2e48083d23f22bd5c103c00) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1wave.8k",  0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( ptblank2ua )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gnb3vera.2l",  0x0000000, 0x100000, CRC(57ad719a) SHA1(f22a02d33c7c23cccffb8ce2e3aca26b07ecac0a) )
	ROM_LOAD16_BYTE( "gnb3vera.2j",  0x0000001, 0x100000, CRC(0378af98) SHA1(601444b5a0935a4b69b5ada618aaf1bc6bb12a3b) )
	ROM_LOAD16_BYTE( "gnb1vera.2k",  0x0200000, 0x100000, CRC(e6335e4e) SHA1(9067f05d848c1c8a88967a3c6552d2d24e80672b) )
	ROM_LOAD16_BYTE( "gnb1vera.2f",  0x0200001, 0x100000, CRC(2bb7eb6d) SHA1(d1b1e031a28443140ac8652dfd77a65a042b67fc) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb1prg0l.ic2", 0x000000, 0x800000, CRC(78746037) SHA1(d130ca1153a730e3c967945248f00662f9fab304) )
	ROM_LOAD16_BYTE( "gnb1prg0u.ic5", 0x000001, 0x800000, CRC(697d3279) SHA1(40302780f7494d9413888b2d1da38bd14a9a444f) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "gnb1vera.6d",  0x0000000, 0x040000, CRC(6461ae77) SHA1(1377b716a69ef9d4d2e48083d23f22bd5c103c00) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1wave.8k",  0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( gunbarla )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "gnb1vera.2l",  0x0000000, 0x100000, CRC(405e2585) SHA1(dc05ce7692e9244db0276fba13800b00a68d2054) )
	ROM_LOAD16_BYTE( "gnb1vera.2j",  0x0000001, 0x100000, CRC(2d2af8cf) SHA1(e9ad303777d71e8c37f2a83b12fe61ee018dbd8e) )
	ROM_LOAD16_BYTE( "gnb1vera.2k",  0x0200000, 0x100000, CRC(e6335e4e) SHA1(9067f05d848c1c8a88967a3c6552d2d24e80672b) )
	ROM_LOAD16_BYTE( "gnb1vera.2f",  0x0200001, 0x100000, CRC(2bb7eb6d) SHA1(d1b1e031a28443140ac8652dfd77a65a042b67fc) )

	ROM_REGION32_LE( 0x2000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "gnb1prg0l.ic2", 0x000000, 0x800000, CRC(78746037) SHA1(d130ca1153a730e3c967945248f00662f9fab304) )
	ROM_LOAD16_BYTE( "gnb1prg0u.ic5", 0x000001, 0x800000, CRC(697d3279) SHA1(40302780f7494d9413888b2d1da38bd14a9a444f) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "gnb1vera.6d",  0x0000000, 0x040000, CRC(6461ae77) SHA1(1377b716a69ef9d4d2e48083d23f22bd5c103c00) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "gnb1wave.8k",  0x0000000, 0x400000, CRC(4e19d9d6) SHA1(0a92c987536999a789663a30c787950ab6995128) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( primglex )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "pg1vera.2l",   0x0000000, 0x100000, CRC(fc15fd1a) SHA1(6ca5ebdc096cab3296dc7c1f675d78dfc7c69a05) )
	ROM_LOAD16_BYTE( "pg1vera.2j",   0x0000001, 0x100000, CRC(79955553) SHA1(ad2dca38b06a835f8241fae0a5fa18d5874cebe4) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "pg1rom0u.ic5", 0x0000000, 0x200000, CRC(2a503f2f) SHA1(206b9c9204be22241d2a3e017b96c3a103f5a976) )
	ROM_CONTINUE( 0x0000000, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "pg1rom0l.ic6", 0x0000001, 0x200000, CRC(54cef992) SHA1(5ba81353b1dddc1c6640fc4c15df81535e7a6ae8) )
	ROM_CONTINUE( 0x0000001, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "pg1rom1u.ic3", 0x0400000, 0x200000, CRC(1ee41152) SHA1(d240e6ba820aa2aa4f12380c255f624f91aed564) )
	ROM_CONTINUE( 0x0400000, 0x200000 ) /* first & second half identical */
	ROM_LOAD16_BYTE( "pg1rom1l.ic8", 0x0400001, 0x200000, CRC(59b5a71c) SHA1(ddc1f0a5488466166c21fd0c84ab2b4cf04316bf) )
	ROM_CONTINUE( 0x0400001, 0x200000 ) /* first & second half identical */

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "pg1sprog.6d",  0x0000000, 0x040000, CRC(e7c3396d) SHA1(12bbb8ebcaab1b40462a12917dd9b58bd9ab8663) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD16_WORD_SWAP( "pg1wave.8k",   0x0000000, 0x400000, CRC(fc9ad9eb) SHA1(ce5bb2288ed8cf1348825c39423cbb99d9324b9c) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( souledge )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "so4verc.2l",   0x0000000, 0x100000, CRC(12b8ae0d) SHA1(31571023d5b77ebcd4103b8cac5ba710a3d570a0) )
	ROM_LOAD16_BYTE( "so4verc.2j",   0x0000001, 0x100000, CRC(938262b0) SHA1(e806883e32c473a3c2bb07849126631f6d66fa66) )
	ROM_LOAD16_BYTE( "so1verc.2k",   0x0200000, 0x100000, CRC(1789e399) SHA1(2e11869124cdfa7a66107b0e642bdc72ee0c306a) )
	ROM_LOAD16_BYTE( "so1verc.2f",   0x0200001, 0x100000, CRC(8cffe1c3) SHA1(d54a0b1d55f33db2890bfa70c411cca3e446fccf) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "so1rom0u.ic5", 0x0000000, 0x200000, CRC(e364d673) SHA1(70fd58fb912939c57e3a5fadd01c1467df08d4ac) )
	ROM_LOAD16_BYTE( "so1rom0l.ic6", 0x0000001, 0x200000, CRC(9c5b0858) SHA1(f3ac726f1167551beea7edc46e43b826b7baaf62) )
	ROM_LOAD16_BYTE( "so1rom1u.ic3", 0x0400000, 0x200000, CRC(8f9d8c5b) SHA1(ac1da70854eee344a645749f564366ceac571767) )
	ROM_LOAD16_BYTE( "so1rom1l.ic8", 0x0400001, 0x200000, CRC(4406ef16) SHA1(c86f199fdb1db23e5944ca51e6518b9cd0dafb71) )
	ROM_LOAD16_BYTE( "so1rom2u.ic4", 0x0800000, 0x200000, CRC(b4baa886) SHA1(0432692a4d71a3f1b47707efb6858927744940e4) )
	ROM_LOAD16_BYTE( "so1rom2l.ic7", 0x0800001, 0x200000, CRC(37c1f66e) SHA1(13a8a73fce142ea5ebe3f0c1050e44a027ab42a6) )
	ROM_LOAD16_BYTE( "so1rom3u.ic1", 0x0c00000, 0x200000, CRC(f11bd521) SHA1(baf936dec58cebfeef1c74f95e455b2fe74eb982) )
	ROM_LOAD16_BYTE( "so1rom3l.ic9", 0x0c00001, 0x200000, CRC(84465bcc) SHA1(d8be888d41cfe194c3a1853d9146d3a74ef7bab1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "so1sprogc.6d", 0x0000000, 0x040000, CRC(2bbc118c) SHA1(4168a9aa525f1f0ce6cf6e14cfe4c118c4c0d773) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "so1wave.8k",   0x000000, 0x400000, CRC(0e68836b) SHA1(c392b370a807803c7ab060105861253e1b407f49) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( souledgeuc )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "so3verc.2l",   0x0000000, 0x100000, CRC(c90e343b) SHA1(83bd2fc67493c34b408b13148f6a2feba9a44ed0) )
	ROM_LOAD16_BYTE( "so3verc.2j",   0x0000001, 0x100000, CRC(b7466db5) SHA1(c7a4c66e61812fb6de582fe555755293d78771c7) )
	ROM_LOAD16_BYTE( "so1verc.2k",   0x0200000, 0x100000, CRC(1789e399) SHA1(2e11869124cdfa7a66107b0e642bdc72ee0c306a) )
	ROM_LOAD16_BYTE( "so1verc.2f",   0x0200001, 0x100000, CRC(8cffe1c3) SHA1(d54a0b1d55f33db2890bfa70c411cca3e446fccf) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "so1rom0u.ic5", 0x0000000, 0x200000, CRC(e364d673) SHA1(70fd58fb912939c57e3a5fadd01c1467df08d4ac) )
	ROM_LOAD16_BYTE( "so1rom0l.ic6", 0x0000001, 0x200000, CRC(9c5b0858) SHA1(f3ac726f1167551beea7edc46e43b826b7baaf62) )
	ROM_LOAD16_BYTE( "so1rom1u.ic3", 0x0400000, 0x200000, CRC(8f9d8c5b) SHA1(ac1da70854eee344a645749f564366ceac571767) )
	ROM_LOAD16_BYTE( "so1rom1l.ic8", 0x0400001, 0x200000, CRC(4406ef16) SHA1(c86f199fdb1db23e5944ca51e6518b9cd0dafb71) )
	ROM_LOAD16_BYTE( "so1rom2u.ic4", 0x0800000, 0x200000, CRC(b4baa886) SHA1(0432692a4d71a3f1b47707efb6858927744940e4) )
	ROM_LOAD16_BYTE( "so1rom2l.ic7", 0x0800001, 0x200000, CRC(37c1f66e) SHA1(13a8a73fce142ea5ebe3f0c1050e44a027ab42a6) )
	ROM_LOAD16_BYTE( "so1rom3u.ic1", 0x0c00000, 0x200000, CRC(f11bd521) SHA1(baf936dec58cebfeef1c74f95e455b2fe74eb982) )
	ROM_LOAD16_BYTE( "so1rom3l.ic9", 0x0c00001, 0x200000, CRC(84465bcc) SHA1(d8be888d41cfe194c3a1853d9146d3a74ef7bab1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "so1sprogc.6d", 0x0000000, 0x040000, CRC(2bbc118c) SHA1(4168a9aa525f1f0ce6cf6e14cfe4c118c4c0d773) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "so1wave.8k",   0x000000, 0x400000, CRC(0e68836b) SHA1(c392b370a807803c7ab060105861253e1b407f49) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( souledgeua )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "so3vera.2l",   0x0000000, 0x100000, CRC(19b39096) SHA1(9188cd1fd9b15e5545eb41ea2768a8bd42113379) )
	ROM_LOAD16_BYTE( "so3vera.2j",   0x0000001, 0x100000, CRC(09eda46f) SHA1(24d04d2ba51af508ddc0656e8bb5e1335b08cc8a) )
	ROM_LOAD16_BYTE( "so1vera.2k",   0x0200000, 0x100000, CRC(29bdc6bb) SHA1(9047792c1a21a0001024939f1c6a8c3d86948973) )
	ROM_LOAD16_BYTE( "so1vera.2f",   0x0200001, 0x100000, CRC(c035b71b) SHA1(38719a75193774b124d845460c0c03d36849719d) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "so1rom0u.ic5", 0x0000000, 0x200000, CRC(e364d673) SHA1(70fd58fb912939c57e3a5fadd01c1467df08d4ac) )
	ROM_LOAD16_BYTE( "so1rom0l.ic6", 0x0000001, 0x200000, CRC(9c5b0858) SHA1(f3ac726f1167551beea7edc46e43b826b7baaf62) )
	ROM_LOAD16_BYTE( "so1rom1u.ic3", 0x0400000, 0x200000, CRC(8f9d8c5b) SHA1(ac1da70854eee344a645749f564366ceac571767) )
	ROM_LOAD16_BYTE( "so1rom1l.ic8", 0x0400001, 0x200000, CRC(4406ef16) SHA1(c86f199fdb1db23e5944ca51e6518b9cd0dafb71) )
	ROM_LOAD16_BYTE( "so1rom2u.ic4", 0x0800000, 0x200000, CRC(b4baa886) SHA1(0432692a4d71a3f1b47707efb6858927744940e4) )
	ROM_LOAD16_BYTE( "so1rom2l.ic7", 0x0800001, 0x200000, CRC(37c1f66e) SHA1(13a8a73fce142ea5ebe3f0c1050e44a027ab42a6) )
	ROM_LOAD16_BYTE( "so1rom3u.ic1", 0x0c00000, 0x200000, CRC(f11bd521) SHA1(baf936dec58cebfeef1c74f95e455b2fe74eb982) )
	ROM_LOAD16_BYTE( "so1rom3l.ic9", 0x0c00001, 0x200000, CRC(84465bcc) SHA1(d8be888d41cfe194c3a1853d9146d3a74ef7bab1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "so1sprog.6d",  0x0000000, 0x040000, CRC(f6f682b7) SHA1(a64e19be3f6e630b8c34f34b46b95aadfabd3f63) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "so1wave.8k",   0x000000, 0x400000, CRC(0e68836b) SHA1(c392b370a807803c7ab060105861253e1b407f49) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( souledgea )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "so2vera.2l",   0x0000000, 0x100000, CRC(0e9efc5c) SHA1(212b156f811174189fbf2ba908e67c6d76b9b4a6) )
	ROM_LOAD16_BYTE( "so2vera.2j",   0x0000001, 0x100000, CRC(fda023f5) SHA1(f0a5bb1c813a5ee8629cb590e473dd0f5f1f416e) )
	ROM_LOAD16_BYTE( "so2vera.2k",   0x0200000, 0x100000, CRC(29bdc6bb) SHA1(9047792c1a21a0001024939f1c6a8c3d86948973) )
	ROM_LOAD16_BYTE( "so2vera.2f",   0x0200001, 0x100000, CRC(c035b71b) SHA1(38719a75193774b124d845460c0c03d36849719d) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "so1rom0u.ic5", 0x0000000, 0x200000, CRC(e364d673) SHA1(70fd58fb912939c57e3a5fadd01c1467df08d4ac) )
	ROM_LOAD16_BYTE( "so1rom0l.ic6", 0x0000001, 0x200000, CRC(9c5b0858) SHA1(f3ac726f1167551beea7edc46e43b826b7baaf62) )
	ROM_LOAD16_BYTE( "so1rom1u.ic3", 0x0400000, 0x200000, CRC(8f9d8c5b) SHA1(ac1da70854eee344a645749f564366ceac571767) )
	ROM_LOAD16_BYTE( "so1rom1l.ic8", 0x0400001, 0x200000, CRC(4406ef16) SHA1(c86f199fdb1db23e5944ca51e6518b9cd0dafb71) )
	ROM_LOAD16_BYTE( "so1rom2u.ic4", 0x0800000, 0x200000, CRC(b4baa886) SHA1(0432692a4d71a3f1b47707efb6858927744940e4) )
	ROM_LOAD16_BYTE( "so1rom2l.ic7", 0x0800001, 0x200000, CRC(37c1f66e) SHA1(13a8a73fce142ea5ebe3f0c1050e44a027ab42a6) )
	ROM_LOAD16_BYTE( "so1rom3u.ic1", 0x0c00000, 0x200000, CRC(f11bd521) SHA1(baf936dec58cebfeef1c74f95e455b2fe74eb982) )
	ROM_LOAD16_BYTE( "so1rom3l.ic9", 0x0c00001, 0x200000, CRC(84465bcc) SHA1(d8be888d41cfe194c3a1853d9146d3a74ef7bab1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "so1sprog.6d",  0x0000000, 0x040000, CRC(f6f682b7) SHA1(a64e19be3f6e630b8c34f34b46b95aadfabd3f63) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "so1wave.8k",   0x000000, 0x400000, CRC(0e68836b) SHA1(c392b370a807803c7ab060105861253e1b407f49) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( souledgeja )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "so1vera.2l",   0x0000000, 0x100000, CRC(bafb94c8) SHA1(92461ce74a537a9801a024280e18fc3d0a5e6e5c) )
	ROM_LOAD16_BYTE( "so1vera.2j",   0x0000001, 0x100000, CRC(abe2d28e) SHA1(e9d858c8f8651b04bc72eb6de423da4925e94250) )
	ROM_LOAD16_BYTE( "so1vera.2k",   0x0200000, 0x100000, CRC(29bdc6bb) SHA1(9047792c1a21a0001024939f1c6a8c3d86948973) )
	ROM_LOAD16_BYTE( "so1vera.2f",   0x0200001, 0x100000, CRC(c035b71b) SHA1(38719a75193774b124d845460c0c03d36849719d) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "so1rom0u.ic5", 0x0000000, 0x200000, CRC(e364d673) SHA1(70fd58fb912939c57e3a5fadd01c1467df08d4ac) )
	ROM_LOAD16_BYTE( "so1rom0l.ic6", 0x0000001, 0x200000, CRC(9c5b0858) SHA1(f3ac726f1167551beea7edc46e43b826b7baaf62) )
	ROM_LOAD16_BYTE( "so1rom1u.ic3", 0x0400000, 0x200000, CRC(8f9d8c5b) SHA1(ac1da70854eee344a645749f564366ceac571767) )
	ROM_LOAD16_BYTE( "so1rom1l.ic8", 0x0400001, 0x200000, CRC(4406ef16) SHA1(c86f199fdb1db23e5944ca51e6518b9cd0dafb71) )
	ROM_LOAD16_BYTE( "so1rom2u.ic4", 0x0800000, 0x200000, CRC(b4baa886) SHA1(0432692a4d71a3f1b47707efb6858927744940e4) )
	ROM_LOAD16_BYTE( "so1rom2l.ic7", 0x0800001, 0x200000, CRC(37c1f66e) SHA1(13a8a73fce142ea5ebe3f0c1050e44a027ab42a6) )
	ROM_LOAD16_BYTE( "so1rom3u.ic1", 0x0c00000, 0x200000, CRC(f11bd521) SHA1(baf936dec58cebfeef1c74f95e455b2fe74eb982) )
	ROM_LOAD16_BYTE( "so1rom3l.ic9", 0x0c00001, 0x200000, CRC(84465bcc) SHA1(d8be888d41cfe194c3a1853d9146d3a74ef7bab1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "so1sprog.6d",  0x0000000, 0x040000, CRC(f6f682b7) SHA1(a64e19be3f6e630b8c34f34b46b95aadfabd3f63) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "so1wave.8k",   0x000000, 0x400000, CRC(0e68836b) SHA1(c392b370a807803c7ab060105861253e1b407f49) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( starswep ) /* reports as: 151500JUL151997 STP1/VER.A but is clearly an English (world) version */
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "stp2vera.2l",   0x0000000, 0x100000, CRC(a03f4bac) SHA1(a9b5e5e540105c325a4b8f7f1ae863b6fd6227f9) )
	ROM_LOAD16_BYTE( "stp2vera.2j",   0x0000001, 0x100000, CRC(590da032) SHA1(be0af7b68ab7cc666424ca07edebc17d813b2d03) )
	ROM_LOAD16_BYTE( "stp2vera.2k",   0x0200000, 0x100000, CRC(ac8717d5) SHA1(66dbc734de78041202e7d32ccceec306a6df34f8) )
	ROM_LOAD16_BYTE( "stp2vera.2f",   0x0200001, 0x100000, CRC(42733309) SHA1(d28cb6dc03ccf992e0b1c67db7f39b4fd8a38392) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "stp2vera.6d", 0x0000000, 0x040000, CRC(08aaaf6a) SHA1(51c913a39ff7c154aef8bb10139cc8b92eb4756a) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "stp1wave.8k",  0x000000, 0x400000, CRC(18f30e92) SHA1(b3819455856298527a7224495f541145aecf23dd) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( starswepj ) /* reports as: 151500JUL151997 STP1/VER.A */
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD( "stp1vera.1j",         0x0000000, 0x200000, CRC(ef83e126) SHA1(f721b43358cedad0f28af5d2b292b44043fd47a0) )
	ROM_LOAD( "stp1vera.1l",         0x0200000, 0x200000, CRC(0ee7fe1e) SHA1(8c2f5b0e7b49dbe0e8105bf55c493acd46a4f59d) ) /* == stp2vera.2k + stp2vera.2f interleaved */

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "stp1sprog.7e", 0x0000000, 0x040000, CRC(08aaaf6a) SHA1(51c913a39ff7c154aef8bb10139cc8b92eb4756a) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "stp1wave.8k",  0x000000, 0x400000, CRC(18f30e92) SHA1(b3819455856298527a7224495f541145aecf23dd) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "te2verc.2l",   0x0000000, 0x100000, CRC(a24c8c57) SHA1(bf69f17310201e38aa17fb4484cdb480f26d876e) )
	ROM_LOAD16_BYTE( "te2verc.2j",   0x0000001, 0x100000, CRC(3224c298) SHA1(8afe80af0eef52f96bf2ddb2482b06692a8f047d) )
	ROM_LOAD16_BYTE( "te1verb.2k",   0x0200000, 0x100000, CRC(b9860b29) SHA1(678889fc5c70bf66f0bd9864a20636ffb620ed0d) )
	ROM_LOAD16_BYTE( "te1verb.2f",   0x0200001, 0x100000, CRC(3dc01aad) SHA1(266f346fa575c42b635bc469798f5aade9821e20) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "te1rom0l.ic5", 0x0000000, 0x200000, CRC(03786e09) SHA1(de2b9e19ace43c424d100dc5d3207217f66d6479) )
	ROM_LOAD16_BYTE( "te1rom0u.ic6", 0x0000001, 0x200000, CRC(75d91051) SHA1(1c7958162315576c3881dcc684b85710f7f19cd6) )
	ROM_LOAD16_BYTE( "te1rom1l.ic3", 0x0400000, 0x200000, CRC(81416f8e) SHA1(b42ff08ee84491c57a7c87bb767db7e2ec7a26c2) )
	ROM_LOAD16_BYTE( "te1rom1u.ic8", 0x0400001, 0x200000, CRC(fa7ba433) SHA1(e222c4b1631c09e58546446a1e82c16fca936f1d) )
	ROM_LOAD16_BYTE( "te1rom2l.ic4", 0x0800000, 0x200000, CRC(41d77846) SHA1(eeab049135c02a255899fe37e225c1111b2fbb7d) )
	ROM_LOAD16_BYTE( "te1rom2u.ic7", 0x0800001, 0x200000, CRC(a678987e) SHA1(c62c00ce5cf4d001723c999b2bc3dbb90283def1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "te1sprog.6d",  0x0000000, 0x040000, CRC(849587e9) SHA1(94c6a757b24758a866a41bd8acd46aa46844f74b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "te1wave.8k",   0x0000000, 0x200000, CRC(fce6c57a) SHA1(7fb8c69452c92c59a940a2b69d0d73ef7aefcb82) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekkenac )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "te4verc.2l",   0x0000000, 0x100000, CRC(7ecb7892) SHA1(7837f2b3dbfc6b4a153ea652e8a3fd89c4daa26e) )
	ROM_LOAD16_BYTE( "te4verc.2j",   0x0000001, 0x100000, CRC(eea3365d) SHA1(d13df90833aac48f9d9d20cddefb81f90ebab249) )
	ROM_LOAD16_BYTE( "te1verb.2k",   0x0200000, 0x100000, CRC(b9860b29) SHA1(678889fc5c70bf66f0bd9864a20636ffb620ed0d) )
	ROM_LOAD16_BYTE( "te1verb.2f",   0x0200001, 0x100000, CRC(3dc01aad) SHA1(266f346fa575c42b635bc469798f5aade9821e20) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "te1rom0l.ic5", 0x0000000, 0x200000, CRC(03786e09) SHA1(de2b9e19ace43c424d100dc5d3207217f66d6479) )
	ROM_LOAD16_BYTE( "te1rom0u.ic6", 0x0000001, 0x200000, CRC(75d91051) SHA1(1c7958162315576c3881dcc684b85710f7f19cd6) )
	ROM_LOAD16_BYTE( "te1rom1l.ic3", 0x0400000, 0x200000, CRC(81416f8e) SHA1(b42ff08ee84491c57a7c87bb767db7e2ec7a26c2) )
	ROM_LOAD16_BYTE( "te1rom1u.ic8", 0x0400001, 0x200000, CRC(fa7ba433) SHA1(e222c4b1631c09e58546446a1e82c16fca936f1d) )
	ROM_LOAD16_BYTE( "te1rom2l.ic4", 0x0800000, 0x200000, CRC(41d77846) SHA1(eeab049135c02a255899fe37e225c1111b2fbb7d) )
	ROM_LOAD16_BYTE( "te1rom2u.ic7", 0x0800001, 0x200000, CRC(a678987e) SHA1(c62c00ce5cf4d001723c999b2bc3dbb90283def1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "te1sprog.6d",  0x0000000, 0x040000, CRC(849587e9) SHA1(94c6a757b24758a866a41bd8acd46aa46844f74b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "te1wave.8k",   0x0000000, 0x200000, CRC(fce6c57a) SHA1(7fb8c69452c92c59a940a2b69d0d73ef7aefcb82) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekkenb )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "te2verb.2l",   0x0000000, 0x100000, CRC(246cfbdd) SHA1(c1bffe3c463e9eff6dcaf9937da72bff8ff33e4e) )
	ROM_LOAD16_BYTE( "te2verb.2j",   0x0000001, 0x100000, CRC(dfa83e47) SHA1(a29d852c1b6a52c043248d7e5af04067dfa3eb40) )
	ROM_LOAD16_BYTE( "te1verb.2k",   0x0200000, 0x100000, CRC(b9860b29) SHA1(678889fc5c70bf66f0bd9864a20636ffb620ed0d) )
	ROM_LOAD16_BYTE( "te1verb.2f",   0x0200001, 0x100000, CRC(3dc01aad) SHA1(266f346fa575c42b635bc469798f5aade9821e20) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "te1rom0l.ic5", 0x0000000, 0x200000, CRC(03786e09) SHA1(de2b9e19ace43c424d100dc5d3207217f66d6479) )
	ROM_LOAD16_BYTE( "te1rom0u.ic6", 0x0000001, 0x200000, CRC(75d91051) SHA1(1c7958162315576c3881dcc684b85710f7f19cd6) )
	ROM_LOAD16_BYTE( "te1rom1l.ic3", 0x0400000, 0x200000, CRC(81416f8e) SHA1(b42ff08ee84491c57a7c87bb767db7e2ec7a26c2) )
	ROM_LOAD16_BYTE( "te1rom1u.ic8", 0x0400001, 0x200000, CRC(fa7ba433) SHA1(e222c4b1631c09e58546446a1e82c16fca936f1d) )
	ROM_LOAD16_BYTE( "te1rom2l.ic4", 0x0800000, 0x200000, CRC(41d77846) SHA1(eeab049135c02a255899fe37e225c1111b2fbb7d) )
	ROM_LOAD16_BYTE( "te1rom2u.ic7", 0x0800001, 0x200000, CRC(a678987e) SHA1(c62c00ce5cf4d001723c999b2bc3dbb90283def1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "te1sprog.6d",  0x0000000, 0x040000, CRC(849587e9) SHA1(94c6a757b24758a866a41bd8acd46aa46844f74b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "te1wave.8k",   0x0000000, 0x200000, CRC(fce6c57a) SHA1(7fb8c69452c92c59a940a2b69d0d73ef7aefcb82) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekkenjb )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "te1verb.2l",   0x0000000, 0x100000, CRC(4291afee) SHA1(2e04a6c786544176e2b7e22b5f469e3548896b19) )
	ROM_LOAD16_BYTE( "te1verb.2j",   0x0000001, 0x100000, CRC(5c534705) SHA1(2430b5c36de419822de0283c006c5af2e7cd95ef) )
	ROM_LOAD16_BYTE( "te1verb.2k",   0x0200000, 0x100000, CRC(b9860b29) SHA1(678889fc5c70bf66f0bd9864a20636ffb620ed0d) )
	ROM_LOAD16_BYTE( "te1verb.2f",   0x0200001, 0x100000, CRC(3dc01aad) SHA1(266f346fa575c42b635bc469798f5aade9821e20) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "te1rom0l.ic5", 0x0000000, 0x200000, CRC(03786e09) SHA1(de2b9e19ace43c424d100dc5d3207217f66d6479) )
	ROM_LOAD16_BYTE( "te1rom0u.ic6", 0x0000001, 0x200000, CRC(75d91051) SHA1(1c7958162315576c3881dcc684b85710f7f19cd6) )
	ROM_LOAD16_BYTE( "te1rom1l.ic3", 0x0400000, 0x200000, CRC(81416f8e) SHA1(b42ff08ee84491c57a7c87bb767db7e2ec7a26c2) )
	ROM_LOAD16_BYTE( "te1rom1u.ic8", 0x0400001, 0x200000, CRC(fa7ba433) SHA1(e222c4b1631c09e58546446a1e82c16fca936f1d) )
	ROM_LOAD16_BYTE( "te1rom2l.ic4", 0x0800000, 0x200000, CRC(41d77846) SHA1(eeab049135c02a255899fe37e225c1111b2fbb7d) )
	ROM_LOAD16_BYTE( "te1rom2u.ic7", 0x0800001, 0x200000, CRC(a678987e) SHA1(c62c00ce5cf4d001723c999b2bc3dbb90283def1) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "te1sprog.6d",  0x0000000, 0x040000, CRC(849587e9) SHA1(94c6a757b24758a866a41bd8acd46aa46844f74b) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "te1wave.8k",   0x0000000, 0x200000, CRC(fce6c57a) SHA1(7fb8c69452c92c59a940a2b69d0d73ef7aefcb82) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2 )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes2verd.2l",  0x0000000, 0x100000, CRC(1aef3da8) SHA1(1bb5359b28777fa26034d6916bcda1cc225a0574) )
	ROM_LOAD16_BYTE( "tes2verd.2j",  0x0000001, 0x100000, CRC(03787fd0) SHA1(c4971de47b61cae149d0c8a493ca0b3a2ee7a4ab) )
	ROM_LOAD16_BYTE( "tes1verd.2k",  0x0200000, 0x100000, CRC(846ace0a) SHA1(2016b415ba771d159d63b01f3a12a6cadf23451d) )
	ROM_LOAD16_BYTE( "tes1verd.2f",  0x0200001, 0x100000, CRC(7a0663b4) SHA1(f0036f5c2bf81471e63cd5cf86698dc0721ab9b4) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2b )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes2verb.2l",  0x0000000, 0x100000, CRC(aa9a4503) SHA1(e6fdd34216591510593bbda0232ddc0fcd80e80b) )
	ROM_LOAD16_BYTE( "tes2verb.2j",  0x0000001, 0x100000, CRC(63706d8c) SHA1(740cfa8b422fb663bcb412b3389da33c7f9f13be) )
	ROM_LOAD16_BYTE( "tes1verb.2k",  0x0200000, 0x100000, CRC(668ca712) SHA1(67100db4c6a3ca62d3f62f6fcef974ce017e2c9e) )
	ROM_LOAD16_BYTE( "tes1verb.2f",  0x0200001, 0x100000, CRC(c4f66a0a) SHA1(1b3dd33d7e6d9122826bf8be0dbbc088e4cc41e8) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2a )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes2vera.2l",  0x0000000, 0x100000, CRC(8bb82bf0) SHA1(ac4e0077dff4c46ea2435903c410590f91cafe7d) )
	ROM_LOAD16_BYTE( "tes2vera.2j",  0x0000001, 0x100000, CRC(4e02f921) SHA1(15339c2626033912947d33e5f59a109e607be0bf) )
	ROM_LOAD16_BYTE( "tes1vera.2k",  0x0200000, 0x100000, CRC(78e2ce1a) SHA1(fb242725dc72fa234bd7df81cec57fe010cf58f3) )
	ROM_LOAD16_BYTE( "tes1vera.2f",  0x0200001, 0x100000, CRC(fbb0b146) SHA1(08b11ac0fbfeed62910c5cb5ff7b5939ecbca142) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2ud )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes3verd.2l",  0x0000000, 0x100000, CRC(0768f36c) SHA1(fe299998649eeded77ab4bda23090997f4b39734) )
	ROM_LOAD16_BYTE( "tes3verd.2j",  0x0000001, 0x100000, CRC(d29a0545) SHA1(b969f388d141d2e5cdb021aa72024ea040f493c7) )
	ROM_LOAD16_BYTE( "tes1verd.2k",  0x0200000, 0x100000, CRC(846ace0a) SHA1(2016b415ba771d159d63b01f3a12a6cadf23451d) )
	ROM_LOAD16_BYTE( "tes1verd.2f",  0x0200001, 0x100000, CRC(7a0663b4) SHA1(f0036f5c2bf81471e63cd5cf86698dc0721ab9b4) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2ub )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes3verb.2l",  0x0000000, 0x100000, CRC(4692075f) SHA1(d048a92040ceb57ef7462bebc2c1112b964570ec) )
	ROM_LOAD16_BYTE( "tes3verb.2j",  0x0000001, 0x100000, CRC(db3ec640) SHA1(fc9f475232ea77abd2eb7e2e09314281264e9d38) )
	ROM_LOAD16_BYTE( "tes1verb.2k",  0x0200000, 0x100000, CRC(668ca712) SHA1(67100db4c6a3ca62d3f62f6fcef974ce017e2c9e) )
	ROM_LOAD16_BYTE( "tes1verb.2f",  0x0200001, 0x100000, CRC(c4f66a0a) SHA1(1b3dd33d7e6d9122826bf8be0dbbc088e4cc41e8) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2ua )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes3vera.2l",  0x0000000, 0x100000, CRC(0276680b) SHA1(508bb1c8a0ce929181508108ad3797e67ddd3df8) )
	ROM_LOAD16_BYTE( "tes3vera.2j",  0x0000001, 0x100000, CRC(80e9b5ec) SHA1(2f6313492f0d78b9745303418cd77347d14acc8c) )
	ROM_LOAD16_BYTE( "tes1vera.2k",  0x0200000, 0x100000, CRC(78e2ce1a) SHA1(fb242725dc72fa234bd7df81cec57fe010cf58f3) )
	ROM_LOAD16_BYTE( "tes1vera.2f",  0x0200001, 0x100000, CRC(fbb0b146) SHA1(08b11ac0fbfeed62910c5cb5ff7b5939ecbca142) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2jc )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes1verc.2l",  0x0000000, 0x100000, CRC(abcb4981) SHA1(5122b2107184a4465f2e9583556b7d3d6ae37444) )
	ROM_LOAD16_BYTE( "tes1verc.2j",  0x0000001, 0x100000, CRC(d936bf5f) SHA1(bb59cd294d0b7d30efa65b070cdc01b6f7a23739) )
	ROM_LOAD16_BYTE( "tes1verb.2k",  0x0200000, 0x100000, CRC(668ca712) SHA1(67100db4c6a3ca62d3f62f6fcef974ce017e2c9e) )
	ROM_LOAD16_BYTE( "tes1verb.2f",  0x0200001, 0x100000, CRC(c4f66a0a) SHA1(1b3dd33d7e6d9122826bf8be0dbbc088e4cc41e8) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( tekken2jb )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "tes1verb.2l",  0x0000000, 0x100000, CRC(9c333739) SHA1(d850746e5a9147951ede56a6cf115fe98fc9f70b) )
	ROM_LOAD16_BYTE( "tes1verb.2j",  0x0000001, 0x100000, CRC(dc8cfaea) SHA1(e66d4a1d37148f60874210c1431dc59bf944ebaf) )
	ROM_LOAD16_BYTE( "tes1verb.2k",  0x0200000, 0x100000, CRC(668ca712) SHA1(67100db4c6a3ca62d3f62f6fcef974ce017e2c9e) )
	ROM_LOAD16_BYTE( "tes1verb.2f",  0x0200001, 0x100000, CRC(c4f66a0a) SHA1(1b3dd33d7e6d9122826bf8be0dbbc088e4cc41e8) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "tes1rom0l.ic6", 0x0000000, 0x200000, CRC(fc904ede) SHA1(cea378ba86f94beadb3d67685f1b8c141f478abe) )
	ROM_LOAD16_BYTE( "tes1rom0u.ic5", 0x0000001, 0x200000, CRC(57b38f5d) SHA1(edb4beab47b1339a5b1bc8071086abfcba57722e) )
	ROM_LOAD16_BYTE( "tes1rom1l.ic8", 0x0400000, 0x200000, CRC(aa48f04b) SHA1(f7383d2b3a84c4e649a27c0ad1e6af4702ec0a17) )
	ROM_LOAD16_BYTE( "tes1rom1u.ic3", 0x0400001, 0x200000, CRC(b147c543) SHA1(c4b18c218999ec73d04c92e06fb3e6165ceebf2b) )
	ROM_LOAD16_BYTE( "tes1rom2l.ic7", 0x0800000, 0x200000, CRC(b08da52c) SHA1(31fe2021d0fe37c16555650dd10d26ed80d9b493) )
	ROM_LOAD16_BYTE( "tes1rom2u.ic4", 0x0800001, 0x200000, CRC(8a1561b8) SHA1(ebc02c9e7033d54aefb5034c97a3c8cd749b5600) )
	ROM_LOAD16_BYTE( "tes1rom3l.ic9", 0x0c00000, 0x200000, CRC(d5ac0f18) SHA1(342d063f7974bd1f90b5ca4832dfa4fbc9605453) )
	ROM_LOAD16_BYTE( "tes1rom3u.ic1", 0x0c00001, 0x200000, CRC(44ed509d) SHA1(27e26aaf5ce72ab686f3f05743b1d91b5334b4e0) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "tes1sprog.6d", 0x0000000, 0x040000, CRC(af18759f) SHA1(aabd7d1384925781d37f860605a5d4622e0fc2e4) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "tes1wave.8k",  0x800000, 0x400000, CRC(34a34eab) SHA1(8e83a579abdcd419dc5cff8aa4c1d7e6c3add773) )

	NAMCO_C76_BIOS
ROM_END

ROM_START( xevi3dg ) /* PCB was stickered as XV3 2 / VER.B */
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "xv32verb.2l",  0x0000000, 0x100000, CRC(6ffcceac) SHA1(b26b1c38680a818335748554eee50f6290ae23d2) )
	ROM_LOAD16_BYTE( "xv32verb.2j",  0x0000001, 0x100000, CRC(c096dd18) SHA1(195e080f833b8854d9011d5eb50b9e556954903f) )
	ROM_LOAD16_BYTE( "xv31vera.2k",  0x0200000, 0x100000, CRC(3d58138e) SHA1(9203d6bdc2d968de818d5f465523cc030217dcf8) )
	ROM_LOAD16_BYTE( "xv31vera.2f",  0x0200001, 0x100000, CRC(9e8780a2) SHA1(83148d55456b2b92969f7ac2bdb2d492bf969895) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "xv31rom0l.ic5", 0x0000000, 0x200000, CRC(24e1e262) SHA1(84df49b22a8a36284da771944a8390672a0c32bf) )
	ROM_LOAD16_BYTE( "xv31rom0u.ic6", 0x0000001, 0x200000, CRC(cae38ef3) SHA1(2dfe0b31969091975e8d8c8188ce7dd007e4a0f3) )
	ROM_LOAD16_BYTE( "xv31rom1l.ic3", 0x0400000, 0x200000, CRC(46b4cb72) SHA1(c3360c4fdb71ffcbccec3f4ad8d7963b08822e26) )
	ROM_LOAD16_BYTE( "xv31rom1u.ic8", 0x0400001, 0x200000, CRC(be0eb5d1) SHA1(f1a0adcb7c65bbba723fe09b07280b0b924d6b19) )
	ROM_LOAD16_BYTE( "xv31rom2l.ic4", 0x0800000, 0x200000, CRC(8403a277) SHA1(35193211351494a086d8422e3b0b71a8d3a262a6) )
	ROM_LOAD16_BYTE( "xv31rom2u.ic7", 0x0800001, 0x200000, CRC(ecf70432) SHA1(bec128a215e0aef66e9a8707bb0d1eb7b098a356) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "xv31sprog.6d", 0x0000000, 0x040000, CRC(e50b856a) SHA1(631da4f60c9ce08387fca26a70481a2fdacf9765) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "xv31wave.8k",  0x000000, 0x400000, CRC(14f25ddd) SHA1(4981cf1017432ff85b768ec88c36f535df30b783) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( xevi3dga )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "xv32vera.2l",  0x0000000, 0x100000, CRC(bb5c0f1b) SHA1(b021fce7237208e95c138d8ad1bdbdcbafd3796e) )
	ROM_LOAD16_BYTE( "xv32vera.2j",  0x0000001, 0x100000, CRC(21e20037) SHA1(f383150e9a0e5d0d96a0be661dfb5d3d09cb2518) )
	ROM_LOAD16_BYTE( "xv31vera.2k",  0x0200000, 0x100000, CRC(3d58138e) SHA1(9203d6bdc2d968de818d5f465523cc030217dcf8) )
	ROM_LOAD16_BYTE( "xv31vera.2f",  0x0200001, 0x100000, CRC(9e8780a2) SHA1(83148d55456b2b92969f7ac2bdb2d492bf969895) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "xv31rom0l.ic5", 0x0000000, 0x200000, CRC(24e1e262) SHA1(84df49b22a8a36284da771944a8390672a0c32bf) )
	ROM_LOAD16_BYTE( "xv31rom0u.ic6", 0x0000001, 0x200000, CRC(cae38ef3) SHA1(2dfe0b31969091975e8d8c8188ce7dd007e4a0f3) )
	ROM_LOAD16_BYTE( "xv31rom1l.ic3", 0x0400000, 0x200000, CRC(46b4cb72) SHA1(c3360c4fdb71ffcbccec3f4ad8d7963b08822e26) )
	ROM_LOAD16_BYTE( "xv31rom1u.ic8", 0x0400001, 0x200000, CRC(be0eb5d1) SHA1(f1a0adcb7c65bbba723fe09b07280b0b924d6b19) )
	ROM_LOAD16_BYTE( "xv31rom2l.ic4", 0x0800000, 0x200000, CRC(8403a277) SHA1(35193211351494a086d8422e3b0b71a8d3a262a6) )
	ROM_LOAD16_BYTE( "xv31rom2u.ic7", 0x0800001, 0x200000, CRC(ecf70432) SHA1(bec128a215e0aef66e9a8707bb0d1eb7b098a356) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "xv31sprog.6d", 0x000000, 0x040000, CRC(7e9fc6a0) SHA1(7f8f1bb9c55f0de0d21c20128806ee9e8a821504) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "xv31wave.8k",  0x000000, 0x400000, CRC(14f25ddd) SHA1(4981cf1017432ff85b768ec88c36f535df30b783) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

ROM_START( xevi3dgj )
	ROM_REGION( 0x0400000, REGION_CPU1, 0 ) /* main ram */
	ROM_REGION( 0x0000400, REGION_USER1, 0 ) /* scratchpad */

	ROM_REGION32_LE( 0x0400000, REGION_USER2, 0 ) /* main prg */
	ROM_LOAD16_BYTE( "xv31vera.2l",  0x0000000, 0x100000, CRC(419e0f13) SHA1(876ae81c9fde18cdec792f3dfee682761ac99730) )
	ROM_LOAD16_BYTE( "xv31vera.2j",  0x0000001, 0x100000, CRC(df95373a) SHA1(65cf85ad4dcbc8f0376ac46ee0c1037d79099843) )
	ROM_LOAD16_BYTE( "xv31vera.2k",  0x0200000, 0x100000, CRC(3d58138e) SHA1(9203d6bdc2d968de818d5f465523cc030217dcf8) )
	ROM_LOAD16_BYTE( "xv31vera.2f",  0x0200001, 0x100000, CRC(9e8780a2) SHA1(83148d55456b2b92969f7ac2bdb2d492bf969895) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 ) /* main data */
	ROM_LOAD16_BYTE( "xv31rom0l.ic5", 0x0000000, 0x200000, CRC(24e1e262) SHA1(84df49b22a8a36284da771944a8390672a0c32bf) )
	ROM_LOAD16_BYTE( "xv31rom0u.ic6", 0x0000001, 0x200000, CRC(cae38ef3) SHA1(2dfe0b31969091975e8d8c8188ce7dd007e4a0f3) )
	ROM_LOAD16_BYTE( "xv31rom1l.ic3", 0x0400000, 0x200000, CRC(46b4cb72) SHA1(c3360c4fdb71ffcbccec3f4ad8d7963b08822e26) )
	ROM_LOAD16_BYTE( "xv31rom1u.ic8", 0x0400001, 0x200000, CRC(be0eb5d1) SHA1(f1a0adcb7c65bbba723fe09b07280b0b924d6b19) )
	ROM_LOAD16_BYTE( "xv31rom2l.ic4", 0x0800000, 0x200000, CRC(8403a277) SHA1(35193211351494a086d8422e3b0b71a8d3a262a6) )
	ROM_LOAD16_BYTE( "xv31rom2u.ic7", 0x0800001, 0x200000, CRC(ecf70432) SHA1(bec128a215e0aef66e9a8707bb0d1eb7b098a356) )

	ROM_REGION16_LE( 0x80000, REGION_CPU2, 0 ) /* sound data (C76) */
	ROM_LOAD( "xv31sprog.6d", 0x0000000, 0x040000, CRC(e50b856a) SHA1(631da4f60c9ce08387fca26a70481a2fdacf9765) )

	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* samples (C352) */
	ROM_LOAD( "xv31wave.8k",  0x000000, 0x400000, CRC(14f25ddd) SHA1(4981cf1017432ff85b768ec88c36f535df30b783) )
	ROM_RELOAD( 0x800000, 0x400000 )

	NAMCO_C76_BIOS
ROM_END

GAMEX( 1994, namcoc76,   0,        coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Namco C76 BIOS", NOT_A_DRIVER )

GAMEX( 1994, tekken,     namcoc76, coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken (World, TE2/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, tekkenac,   tekken,   coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken (Asia, TE4/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, tekkenb,    tekken,   coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken (World, TE2/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, tekkenjb,   tekken,   coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken (Japan, TE1/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, tekken2,    namcoc76, coh110_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 Ver.B (World, TES2/VER.D)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tekken2b,   tekken2,  coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 Ver.B (World, TES2/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, tekken2ud,  tekken2,  coh110_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 Ver.B (US, TES3/VER.D)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tekken2ub,  tekken2,  coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 Ver.B (US, TES3/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tekken2jc,  tekken2,  coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 Ver.B (Japan, TES1/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tekken2jb,  tekken2,  coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 Ver.B (Japan, TES1/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tekken2a,   tekken2,  coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 (World, TES2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tekken2ua,  tekken2,  coh100_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Tekken 2 (US, TES3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, souledge,   namcoc76, coh110_c76,      souledge, namcos11_c76, ROT0, "Namco", "Soul Edge Ver. II (Asia, SO4/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, souledgeuc, souledge, coh110_c76,      souledge, namcos11_c76, ROT0, "Namco", "Soul Edge Ver. II (US, SO3/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, souledgea,  souledge, coh110_c76,      souledge, namcos11_c76, ROT0, "Namco", "Soul Edge (World, SO2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, souledgeua, souledge, coh110_c76,      souledge, namcos11_c76, ROT0, "Namco", "Soul Edge (US, SO3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, souledgeja, souledge, coh110_c76,      souledge, namcos11_c76, ROT0, "Namco", "Soul Edge (Japan, SO1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, dunkmnia,   namcoc76, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Dunk Mania (World, DM2/VER.C)", 0 )
GAMEX( 1995, dunkmniajc, dunkmnia, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Dunk Mania (Japan, DM1/VER.C)", 0 )
GAMEX( 1995, xevi3dg,    namcoc76, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Xevious 3D/G (World, XV32/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, xevi3dga,   xevi3dg,  coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Xevious 3D/G (World, XV32/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, xevi3dgj,   xevi3dg,  coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Xevious 3D/G (Japan, XV31/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, primglex,   namcoc76, coh110_c76,      tekken,   namcos11_c76, ROT0, "Namco", "Prime Goal EX (Japan, PG1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, danceyes,   namcoc76, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Dancing Eyes (World, DC2/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, danceyesu,  danceyes, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Dancing Eyes (US, DC3/VER.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, danceyesj,  danceyes, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Namco", "Dancing Eyes (Japan, DC1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, starswep,   namcoc76, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Axela / Namco", "Star Sweep (World, STP2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, starswepj,  starswep, coh110_c76,      namcos11_c76, namcos11_c76, ROT0, "Axela / Namco", "Star Sweep (Japan, STP1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, myangel3,   namcoc76, coh110_c76,      myangel3, namcos11_c76, ROT0, "MOSS / Namco", "Kosodate Quiz My Angel 3 (Japan, KQT1/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, pocketrc,   namcoc76, coh110_c76,      pocketrc, namcos11_c76, ROT0, "Namco", "Pocket Racer (Japan, PKR1/VER.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ptblank2,   namcoc76, coh110_c76,      ptblank2, ptblank2, ROT0, "Namco", "Point Blank 2 sets", NOT_A_DRIVER )
GAMEX( 1999, ptblank2a,  ptblank2, coh110_c76,      ptblank2, ptblank2, ROT0, "Namco", "Point Blank 2 (World, GNB2/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ptblank2b,  ptblank2, coh110_c76,      ptblank2, ptblank2, ROT0, "Namco", "Point Blank 2 (World, GNB2/VER.A alt)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ptblank2c,  ptblank2, coh110_c76,      ptblank2, ptblank2, ROT0, "Namco", "Point Blank 2 (World, unknown version)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, ptblank2ua, ptblank2, coh110_c76,      ptblank2, ptblank2, ROT0, "Namco", "Point Blank 2 (US, GNB3/VER.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, gunbarla,   ptblank2, coh110_c76,      ptblank2, ptblank2, ROT0, "Namco", "Gunbarl (Japan, GNB1/VER.A)", GAME_IMPERFECT_GRAPHICS )
