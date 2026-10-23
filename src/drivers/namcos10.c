/***************************************************************************

  Namco System 10 - Arcade PSX Hardware
  =====================================

  Backported from current MAME src/mame/namco/namcos10.cpp (smf, windyfairy
  and others), ns10crypt.cpp (Andreas Naive, windyfairy) and the NAND flash
  of src/devices/machine/nandflash.cpp.

  The main board is a PSone: CXD8606BQ CPU, CXD8561CQ GPU with 2MB of VRAM,
  the SPU, 4MB of RAM. It has no BIOS: the boot loader and the game come
  from the ROM board, which also decrypts them:

  MEM(M) (Mr. Driller 2): an Intel 28F640J5 flash with the program and two
  16MB mask ROMs, read through a 1MB window at 0x1f400000 with a bank
  register at 0x1fb40000. The type 1 decrypter.

  MEM(N): two to five Samsung K9F2808U0B NAND flashes (16MB and 512KB of
  spare area), read a page at a time through registers at 0x1f400000, by
  the CPU or by DMA channel 5. The type 2 decrypter. Block 1 of the first
  NAND holds the settings and the bookkeeping, the only part the board
  writes.

  Both scramble the data that is not decrypted: an xor with 0xaaaa and a
  bitswap that changes with the game. The start of the program (copied
  into the BIOS space when the board starts) is scrambled that way.

  Memory map (mirrored at 0x9f000000 and 0xbf000000):
  0x1f300000  MEM(M)/(N): decrypter on (bit 15, IV in bits 0-3) / off
  0x1f400000  MEM(M): 1MB window; MEM(N): NAND registers
  0x1f500000  MEM(N): 1MB of RAM (the NAND block table)
  0x1fb40000  MEM(M): bank register
  0x1fba0000  DIP switches, EXIO ident, inputs, I2C, interrupt status
  0x1fe00000  EXIO(G) (Point Blank 3, Gunbalina): the light gun counters

  The I/O interrupt (IRQ 10) comes at 100Hz, as in MAME. The GPU linked
  lists end 500 system clocks after they start, as in MAME (the movies of
  Point Blank 3 wait for the DrawSync callback).

  Not here: the games that need the TMP95C061 of the EXIO (Gahaha
  Ippatsudou 1 and 2: their analog sticks), the MEMP3 board (its MCU and
  MP3 decoder), the CD-ROM drive (Shamisen Brothers) and the sets that do
  not work in MAME.

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "cpu/mips/psx.h"
#include "includes/psx.h"
#include "sound/psx.h"
#include "machine/intelfsh.h"

enum
{
	NS10_MEMM = 0,
	NS10_MEMN
};

static int ns10_board;

/***************************************************************************

  The scramble of the data that is not decrypted

***************************************************************************/

/* the output bit 15 - n is bit swap[ n ] of the input, after the xor */
static UINT16 unscramble_lo[ 256 ];
static UINT16 unscramble_hi[ 256 ];

static UINT16 bitswap16_tab( UINT16 data, const UINT8 *swap )
{
	UINT16 r = 0;
	int i;

	for( i = 0; i < 16; i++ )
	{
		if( data & ( 1 << swap[ i ] ) )
			r |= 1 << ( 15 - i );
	}
	return r;
}

static void unscramble_setup( const UINT8 *swap )
{
	int i;

	/* the bitswap is linear: one table per byte, the xor folded in the low one */
	for( i = 0; i < 256; i++ )
	{
		unscramble_lo[ i ] = bitswap16_tab( i, swap ) ^ bitswap16_tab( 0xaaaa, swap );
		unscramble_hi[ i ] = bitswap16_tab( i << 8, swap );
	}
}

static INLINE UINT16 unscramble( UINT16 data )
{
	return unscramble_lo[ data & 0xff ] ^ unscramble_hi[ data >> 8 ];
}

/***************************************************************************

  The decrypters (ns10crypt.cpp)

  Type 1 (Mr. Driller 2): a mask from the cipher and plain words and a
  counter. Type 2: the mask of the next word is linear in the last three
  cipher and plain words (48 bits each), plus a nonlinear term that changes
  with the game (in Point Blank 3, a bit from a table). The linear part is
  in tables of 256 entries for each byte of the two histories.

***************************************************************************/

struct ns10_history
{
	UINT32 clo, chi;	/* the last three cipher words: bits 0-31, 32-47 */
	UINT32 plo, phi;	/* the last three plain words */
	int bit;			/* the bit from the nonlinear table */
};

struct ns10_crypt
{
	const UINT8 *bitswap;			/* the scramble */
	const UINT32 ( *emask )[ 2 ];	/* type 2: the masks of the cipher words (bits 32-47, 0-31) */
	const UINT32 ( *dmask )[ 2 ];	/* and of the plain words */
	UINT16 xmask;
	UINT16 ( *nonlinear )( const struct ns10_history *h );
	const UINT16 *iv;				/* NULL: the S-box of MAME */
	int nonlinear_table;			/* the table bits are in REGION_USER5 */
};

static const struct ns10_crypt *crypt;
static UINT8 crypt_active;
static UINT16 crypt_mask;
static UINT32 crypt_counter;		/* type 1 */
static struct ns10_history crypt_h;	/* type 2 */
static UINT32 crypt_nonlinear_count;
static UINT16 crypt_etab[ 6 ][ 256 ];
static UINT16 crypt_dtab[ 6 ][ 256 ];

static int par32( UINT32 x )
{
	x ^= x >> 16;
	x ^= x >> 8;
	x ^= x >> 4;
	return ( 0x6996 >> ( x & 0xf ) ) & 1;
}

/* the parity of the bits of a 48-bit history in a mask */
#define PAR48( mhi, mlo, hi, lo )	par32( ( ( mhi ) & ( hi ) ) ^ ( ( mlo ) & ( lo ) ) )
/* bit n of the xor of the two histories */
#define HBIT( h, n )	( ( n ) < 32 ? ( ( ( h )->clo ^ ( h )->plo ) >> ( n ) ) & 1 : ( ( ( h )->chi ^ ( h )->phi ) >> ( ( n ) - 32 ) ) & 1 )

static void crypt_tables( const UINT32 ( *mask )[ 2 ], UINT16 ( *tab )[ 256 ] )
{
	int k, v, j;

	for( k = 0; k < 6; k++ )
	{
		for( v = 0; v < 256; v++ )
		{
			UINT16 r = 0;
			for( j = 0; j < 16; j++ )
			{
				UINT32 m = k < 4 ? mask[ j ][ 1 ] >> ( k * 8 ) : mask[ j ][ 0 ] >> ( ( k - 4 ) * 8 );
				r |= par32( m & v & 0xff ) << j;
			}
			tab[ k ][ v ] = r;
		}
	}
}

static void crypt_init( const struct ns10_crypt *c )
{
	crypt = c;
	unscramble_setup( c->bitswap );
	if( c->emask != NULL )
	{
		crypt_tables( c->emask, crypt_etab );
		crypt_tables( c->dmask, crypt_dtab );
	}
}

static void crypt_reset( void )
{
	crypt_active = 0;
	crypt_mask = 0;
	crypt_counter = 0;
	memset( &crypt_h, 0, sizeof( crypt_h ) );
	crypt_nonlinear_count = 0;
}

static void crypt_activate( int iv )
{
	static const int type1_sbox[ 16 ] = { 16, 16, 16, 0, 4, 9, 16, 16, 16, 8, 16, 1, 16, 9, 16, 5 };
	static const int type2_sbox[ 16 ] = { 0, 12, 13, 6, 2, 4, 9, 8, 11, 1, 7, 15, 10, 5, 14, 3 };

	if( crypt->emask == NULL )
	{
		/* 16 is an unknown entry in MAME, used as it is */
		crypt_mask = type1_sbox[ iv ];
		crypt_counter = 0;
	}
	else
	{
		int s = type2_sbox[ iv ];
		if( crypt->iv != NULL )
			crypt_h.clo = crypt->iv[ iv ];
		else
			crypt_h.clo = ( ( s >> 3 ) & 1 ) << 15 | ( ( s >> 2 ) & 1 ) << 12 | ( ( s >> 1 ) & 1 ) << 11 | ( s & 1 ) << 8;
		crypt_h.chi = 0;
		crypt_h.plo = 0;
		crypt_h.phi = 0;
		crypt_mask = 0;
	}
	crypt_active = 1;
}

static UINT16 crypt_type1( UINT16 cipherword )
{
	static const UINT8 swap_plain[ 16 ] = { 9, 13, 15, 7, 14, 8, 6, 10, 11, 12, 3, 5, 0, 1, 4, 2 };
	static const UINT8 swap_c[ 16 ] = { 6, 11, 3, 1, 13, 5, 15, 10, 2, 9, 8, 4, 0, 12, 7, 14 };
	static const UINT8 swap_p[ 16 ] = { 9, 7, 5, 2, 14, 4, 13, 8, 0, 15, 10, 1, 3, 6, 12, 11 };
	UINT16 plainword = crypt_mask ^ bitswap16_tab( cipherword, swap_plain );
	UINT32 n = crypt_counter;
	UINT16 nbs =
		( BIT( n, 4 ) << 15 ) ^
		( ( BIT( cipherword, 2 ) ^ BIT( cipherword, 5 ) ) << 14 ) ^
		( BIT( cipherword, 0 ) << 13 ) ^
		( ( BIT( cipherword, 4 ) | BIT( cipherword, 5 ) ) << 12 ) ^
		( BIT( n, 0 ) << 11 ) ^
		( BIT( cipherword, 6 ) << 10 ) ^
		( ( BIT( cipherword, 4 ) & BIT( n, 1 ) ) << 8 ) ^
		( BIT( n, 3 ) << 6 ) ^
		( ( BIT( cipherword, 3 ) | BIT( n, 7 ) ) << 5 ) ^
		( ( BIT( cipherword, 2 ) ^ BIT( n, 3 ) ) << 4 ) ^
		( BIT( n, 2 ) << 3 ) ^
		( ( BIT( cipherword, 7 ) & BIT( n, 7 ) ) << 2 ) ^
		( BIT( n, 5 ) << 1 ) ^
		( BIT( cipherword, 7 ) | BIT( n, 1 ) );

	crypt_mask = nbs ^ bitswap16_tab( cipherword, swap_c ) ^ bitswap16_tab( plainword, swap_p ) ^ 0xecbe;
	crypt_counter = ( crypt_counter + 1 ) & 0xff;	/* an 8-bit counter in MAME */
	return plainword;
}

static UINT16 crypt_type2( UINT16 cipherword )
{
	UINT16 plainword = cipherword ^ crypt_mask;
	struct ns10_history *h = &crypt_h;
	UINT16 m;

	h->chi = h->clo >> 16;
	h->clo = ( h->clo << 16 ) | cipherword;
	h->phi = h->plo >> 16;
	h->plo = ( h->plo << 16 ) | plainword;

	m = crypt_etab[ 0 ][ h->clo & 0xff ] ^ crypt_etab[ 1 ][ ( h->clo >> 8 ) & 0xff ] ^
		crypt_etab[ 2 ][ ( h->clo >> 16 ) & 0xff ] ^ crypt_etab[ 3 ][ h->clo >> 24 ] ^
		crypt_etab[ 4 ][ h->chi & 0xff ] ^ crypt_etab[ 5 ][ h->chi >> 8 ] ^
		crypt_dtab[ 0 ][ h->plo & 0xff ] ^ crypt_dtab[ 1 ][ ( h->plo >> 8 ) & 0xff ] ^
		crypt_dtab[ 2 ][ ( h->plo >> 16 ) & 0xff ] ^ crypt_dtab[ 3 ][ h->plo >> 24 ] ^
		crypt_dtab[ 4 ][ h->phi & 0xff ] ^ crypt_dtab[ 5 ][ h->phi >> 8 ];
	m ^= crypt->xmask;

	if( crypt->nonlinear_table )
	{
		const UINT8 *t = memory_region( REGION_USER5 );
		UINT32 bits = memory_region_length( REGION_USER5 ) * 8;
		h->bit = t != NULL ? ( t[ crypt_nonlinear_count / 8 ] >> ( 7 - ( crypt_nonlinear_count % 8 ) ) ) & 1 : 0;
		crypt_nonlinear_count++;
		if( crypt_nonlinear_count >= bits )
			crypt_nonlinear_count = 0;
	}
	m ^= crypt->nonlinear( h );
	crypt_mask = m;
	return plainword;
}

static INLINE UINT16 crypt_decrypt( UINT16 data )
{
	return crypt->emask == NULL ? crypt_type1( data ) : crypt_type2( data );
}

static void crypt_switch_w( UINT16 data )
{
	if( data & 0x8000 )
		crypt_activate( data & 0xf );
	else
		crypt_active = 0;
}

/***************************************************************************

  The game data of the decrypters and scrambles (current MAME)

***************************************************************************/

/* the masks are 48 bits: { bits 32-47, bits 0-31 } */

/* chocovdr */
static const UINT8 chocovdr_bitswap[ 16 ] = { 13, 12, 14, 15, 9, 8, 10, 11, 4, 7, 6, 5, 0, 3, 2, 1 };
static const UINT32 chocovdr_emask[ 16 ][ 2 ] =
{
	{ 0x5239, 0x351ec1da }, { 0x0000, 0x00008090 }, { 0x0000, 0x48264808 }, { 0x0000, 0x00004820 },
	{ 0x0000, 0x00000500 }, { 0x0000, 0x58ff5a54 }, { 0x0000, 0xd8220208 }, { 0x5239, 0x351e91d3 },
	{ 0x0000, 0x9a1dfaff }, { 0x0000, 0x90040001 }, { 0x0000, 0x00000100 }, { 0x0000, 0x00001408 },
	{ 0x0000, 0x32efd3f1 }, { 0x0000, 0x000000d0 }, { 0x0000, 0x32efd2d7 }, { 0x0000, 0x00000840 },
};
static const UINT32 chocovdr_dmask[ 16 ][ 2 ] =
{
	{ 0x2000, 0x410485da }, { 0x0000, 0x00008081 }, { 0x0000, 0x08044088 }, { 0x0000, 0x00004802 },
	{ 0x0000, 0x00000500 }, { 0x0000, 0x430cda54 }, { 0x0000, 0x10000028 }, { 0x2000, 0x410491db },
	{ 0x0000, 0x1100fafe }, { 0x0000, 0x18040001 }, { 0x0000, 0x00000010 }, { 0x0000, 0x00000508 },
	{ 0x0000, 0x6800d3f5 }, { 0x0000, 0x00000058 }, { 0x0000, 0x6800d2d5 }, { 0x0000, 0x00001840 },
};
#define chocovdr_xmask ( 0x5b22 )

/* gamshara */
static const UINT8 gamshara_bitswap[ 16 ] = { 13, 12, 15, 14, 8, 9, 11, 10, 5, 7, 4, 6, 0, 1, 2, 3 };
static const UINT32 gamshara_emask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x00000028 }, { 0xcae8, 0x3f389fd9 }, { 0x0000, 0x00001000 }, { 0x0000, 0x42823402 },
	{ 0xcae8, 0x736a0592 }, { 0xcae8, 0x736a8596 }, { 0x0000, 0x8b4095b9 }, { 0x0000, 0x00002100 },
	{ 0x0000, 0x04018228 }, { 0x0000, 0x00000042 }, { 0x0000, 0x00000818 }, { 0x0000, 0x00004010 },
	{ 0x0000, 0x8b4099f1 }, { 0x0000, 0x044bce08 }, { 0x0000, 0x000000c1 }, { 0x0000, 0x42823002 },
};
static const UINT32 gamshara_dmask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x00000028 }, { 0x0904, 0xc2048dd9 }, { 0x0000, 0x00008000 }, { 0x0000, 0x54021002 },
	{ 0x0904, 0xe0078592 }, { 0x0904, 0xe00785b2 }, { 0x0000, 0x440097f9 }, { 0x0000, 0x00002104 },
	{ 0x0000, 0x29018308 }, { 0x0000, 0x00000042 }, { 0x0000, 0x00000850 }, { 0x0000, 0x00004012 },
	{ 0x0000, 0x4400d1f1 }, { 0x0000, 0x6001ce08 }, { 0x0000, 0x000000c8 }, { 0x0000, 0x54023002 },
};
#define gamshara_xmask ( 0x25ab )

/* gjspace */
static const UINT8 gjspace_bitswap[ 16 ] = { 8, 10, 6, 5, 7, 14, 4, 15, 13, 9, 1, 0, 2, 11, 12, 3 };
static const UINT32 gjspace_emask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x00000240 }, { 0xd617, 0xeb0f1ab1 }, { 0x0000, 0x451111c0 }, { 0x0000, 0x013b1f44 },
	{ 0xaab0, 0xb356abce }, { 0x7ca7, 0x6b89602a }, { 0x0000, 0x00001800 }, { 0x0000, 0x031d1303 },
	{ 0x0000, 0x00000801 }, { 0x0000, 0x30111160 }, { 0x0000, 0x01ab3978 }, { 0x0000, 0xc131b160 },
	{ 0x0000, 0x00001110 }, { 0x0000, 0x00008002 }, { 0x0000, 0xe1113540 }, { 0xd617, 0xfdce8bfc },
};
static const UINT32 gjspace_dmask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x00008240 }, { 0x0000, 0x2f301ab1 }, { 0x0000, 0x050011c0 }, { 0x0000, 0x412817c4 },
	{ 0x0004, 0xc338abc6 }, { 0x0004, 0x6108602a }, { 0x0000, 0x00005800 }, { 0x0000, 0xc3081347 },
	{ 0x0000, 0x00000801 }, { 0x0000, 0x61001160 }, { 0x0000, 0x61183978 }, { 0x0000, 0xe520b142 },
	{ 0x0000, 0x00001101 }, { 0x0000, 0x0000a002 }, { 0x0000, 0x29001740 }, { 0x0000, 0xa4309bfc },
};
#define gjspace_xmask ( 0x2e7f )

/* knpuzzle */
static const UINT8 knpuzzle_bitswap[ 16 ] = { 14, 15, 12, 13, 10, 8, 11, 9, 4, 5, 6, 7, 1, 3, 0, 2 };
static const UINT32 knpuzzle_emask[ 16 ][ 2 ] =
{
	{ 0x0000, 0xc0a4208c }, { 0x0000, 0x204100a8 }, { 0x0000, 0x0c0306a0 }, { 0x0000, 0x0819e944 },
	{ 0x0000, 0x00001400 }, { 0x0000, 0x00000061 }, { 0x0000, 0x0141401c }, { 0x0000, 0x00000020 },
	{ 0x0000, 0x01418010 }, { 0x8d6a, 0x1eb690cf }, { 0x8d6a, 0x4d3b90ce }, { 0x0000, 0x00004201 },
	{ 0x0000, 0x012c00a2 }, { 0x0000, 0x0c0304a4 }, { 0x0000, 0x00000500 }, { 0x0000, 0x00000980 },
};
static const UINT32 knpuzzle_dmask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x2a22608c }, { 0x0000, 0x002300a8 }, { 0x0000, 0x00390ea0 }, { 0x0000, 0x0100a9c4 },
	{ 0x0000, 0x00001400 }, { 0x0000, 0x00000041 }, { 0x0000, 0x03014014 }, { 0x0000, 0x00000022 },
	{ 0x0000, 0x03010110 }, { 0x0800, 0x031a80cf }, { 0x0800, 0x003398de }, { 0x0000, 0x00004200 },
	{ 0x0000, 0x012a04a2 }, { 0x0000, 0x003984a4 }, { 0x0000, 0x00000700 }, { 0x0000, 0x00000882 },
};
#define knpuzzle_xmask ( 0x01e2 )

/* konotako */
static const UINT8 konotako_bitswap[ 16 ] = { 14, 15, 12, 13, 8, 9, 11, 10, 5, 4, 7, 6, 0, 1, 3, 2 };
static const UINT32 konotako_emask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x0000004c }, { 0x0000, 0xd39e3d3d }, { 0x0000, 0x00001110 }, { 0x0000, 0x00002200 },
	{ 0x0000, 0x3680c008 }, { 0x0000, 0x00000281 }, { 0x0000, 0x00005002 }, { 0x2a73, 0x71895a47 },
	{ 0x0000, 0x00000003 }, { 0x2a73, 0x71897a4e }, { 0x2a73, 0xaea17a41 }, { 0x2a73, 0xfd895a4f },
	{ 0x0000, 0x5328200a }, { 0x0000, 0x00000010 }, { 0x0000, 0x00000040 }, { 0x0000, 0x00000200 },
};
static const UINT32 konotako_dmask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x0000008c }, { 0x0000, 0x53003d25 }, { 0x0000, 0x00001120 }, { 0x0000, 0x00002200 },
	{ 0x0000, 0x37004008 }, { 0x0000, 0x00000282 }, { 0x0000, 0x00006002 }, { 0x0600, 0x35005a47 },
	{ 0x0000, 0x00000003 }, { 0x0600, 0x35001a4e }, { 0x0600, 0x25007a41 }, { 0x0600, 0xb5005a2f },
	{ 0x0000, 0x9000200b }, { 0x0000, 0x00000310 }, { 0x0000, 0x00001840 }, { 0x0000, 0x00000400 },
};
#define konotako_xmask ( 0x0748 )

/* mrdrilrg */
static const UINT8 mrdrilrg_bitswap[ 16 ] = { 14, 12, 15, 13, 10, 9, 8, 11, 4, 5, 6, 7, 0, 1, 3, 2 };
static const UINT32 mrdrilrg_emask[ 16 ][ 2 ] =
{
	{ 0x115c, 0x68620401 }, { 0x0081, 0x02802094 }, { 0x0081, 0xa65270a8 }, { 0x0000, 0x00810010 },
	{ 0x4c2c, 0x080f3059 }, { 0x0081, 0x470a9500 }, { 0x0081, 0x6a123009 }, { 0x0081, 0xa2b23038 },
	{ 0x2d18, 0xa06b9000 }, { 0x0118, 0x6c02000c }, { 0x0081, 0xa2b2704c }, { 0x7028, 0x11016302 },
	{ 0x0081, 0x470a1122 }, { 0x0000, 0x00006200 }, { 0x9170, 0xac403013 }, { 0x0000, 0x00001044 },
};
static const UINT32 mrdrilrg_dmask[ 16 ][ 2 ] =
{
	{ 0x115c, 0x68620401 }, { 0x0081, 0x02802094 }, { 0x0081, 0xa65270a8 }, { 0x0000, 0x00810010 },
	{ 0x4c2c, 0x080f3059 }, { 0x0081, 0x470a9500 }, { 0x0081, 0x6a123009 }, { 0x0081, 0xa2b23038 },
	{ 0x2d18, 0xa06b9000 }, { 0x0118, 0x6c02000c }, { 0x0081, 0xa2b2704c }, { 0x7028, 0x11016302 },
	{ 0x0081, 0x470a1122 }, { 0x0000, 0x00006200 }, { 0x9170, 0xac403013 }, { 0x0000, 0x00001044 },
};
#define mrdrilrg_xmask ( 0x0000 )

/* panikuru */
static const UINT8 panikuru_bitswap[ 16 ] = { 14, 12, 15, 13, 8, 9, 10, 11, 4, 7, 6, 5, 1, 0, 3, 2 };
static const UINT32 panikuru_emask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x00000130 }, { 0x0000, 0x18802004 }, { 0xb04a, 0x04781081 }, { 0x0000, 0x06802000 },
	{ 0x0000, 0x00000c02 }, { 0x0000, 0x42040020 }, { 0x0000, 0x00000003 }, { 0xb040, 0xc00c0940 },
	{ 0x0000, 0x00000009 }, { 0x1020, 0xa0010400 }, { 0x0000, 0x10204104 }, { 0x0000, 0x5a840004 },
	{ 0x0000, 0x00000290 }, { 0x0000, 0x10204380 }, { 0x0000, 0x00001041 }, { 0x0000, 0x00006100 },
};
static const UINT32 panikuru_dmask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x00000120 }, { 0x0000, 0x28002004 }, { 0x0108, 0x00480001 }, { 0x0000, 0x06040100 },
	{ 0x0000, 0x00000c00 }, { 0x0000, 0x42000820 }, { 0x0000, 0x00001003 }, { 0x9102, 0x12040100 },
	{ 0x0000, 0x00008009 }, { 0x9000, 0x21018000 }, { 0x0000, 0x90004104 }, { 0x0000, 0x6a004004 },
	{ 0x0000, 0x00000214 }, { 0x0000, 0x90004190 }, { 0x0000, 0x00001040 }, { 0x0000, 0x00006008 },
};
#define panikuru_xmask ( 0x7c29 )

/* startrgn */
static const UINT8 startrgn_bitswap[ 16 ] = { 14, 13, 12, 15, 9, 11, 8, 10, 4, 5, 6, 7, 0, 3, 2, 1 };
static const UINT32 startrgn_emask[ 16 ][ 2 ] =
{
	{ 0x3e4b, 0xfe92c6a9 }, { 0x0000, 0x0000010c }, { 0x3e4b, 0x7bd6c4aa }, { 0xb1a9, 0x04b8fab8 },
	{ 0x0000, 0x00000080 }, { 0x0000, 0x00008c00 }, { 0xb1a9, 0xb2f0b4cd }, { 0x0000, 0x6c100828 },
	{ 0x0000, 0x6c100838 }, { 0xb1a9, 0xd3913fcd }, { 0x0000, 0x6161aa00 }, { 0x0000, 0x00006040 },
	{ 0x0000, 0x00000420 }, { 0x0000, 0x00001801 }, { 0x3e4b, 0x7bd6deab }, { 0x0000, 0x00000105 },
};
static const UINT32 startrgn_dmask[ 16 ][ 2 ] =
{
	{ 0x1202, 0x1f00c6a8 }, { 0x0000, 0x00000008 }, { 0x1202, 0x0b1046aa }, { 0x1200, 0x1502fea8 },
	{ 0x0000, 0x00002000 }, { 0x0000, 0x00008800 }, { 0x1200, 0x1e02b4cd }, { 0x0000, 0x2c0008aa },
	{ 0x0000, 0x2c00083a }, { 0x1200, 0x3f027ecd }, { 0x0000, 0x21008a00 }, { 0x0000, 0x00002040 },
	{ 0x0000, 0x00000428 }, { 0x0000, 0x00001001 }, { 0x1202, 0x0b10ceab }, { 0x0000, 0x00000144 },
};
#define startrgn_xmask ( 0x8c46 )

/* ptblank3 */
static const UINT8 ptblank3_bitswap[ 16 ] = { 13, 12, 15, 14, 8, 9, 11, 10, 5, 7, 4, 6, 1, 0, 2, 3 };
static const UINT32 ptblank3_emask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x57001200 }, { 0x0000, 0x000000a4 }, { 0x0000, 0x00000150 }, { 0x0000, 0x00008004 },
	{ 0x0000, 0x57002204 }, { 0x0000, 0x09001000 }, { 0x0000, 0x00102000 }, { 0x0000, 0x64004888 },
	{ 0x0000, 0x00100008 }, { 0x00a0, 0xc0980600 }, { 0x0000, 0x1801c020 }, { 0x5810, 0x881a09c0 },
	{ 0x0004, 0x00980600 }, { 0x0000, 0x00000300 }, { 0x0000, 0x00000080 }, { 0x0000, 0x00a00c0a },
};
static const UINT32 ptblank3_dmask[ 16 ][ 2 ] =
{
	{ 0x0000, 0x41051000 }, { 0x0000, 0x00000024 }, { 0x0000, 0x00000050 }, { 0x0000, 0x00008004 },
	{ 0x0000, 0x41052200 }, { 0x0000, 0x09401100 }, { 0x0000, 0x00122000 }, { 0x0000, 0x04100898 },
	{ 0x0000, 0x00120088 }, { 0x0000, 0x40a80400 }, { 0x0000, 0x19004000 }, { 0x0185, 0x9830281d },
	{ 0x0024, 0x00880201 }, { 0x0000, 0x00000300 }, { 0x0000, 0x000000c0 }, { 0x0000, 0x00000408 },
};
#define ptblank3_xmask ( 0xe1b8 )

static UINT16 nl_zero( const struct ns10_history *h ) { return 0; }

static UINT16 nl_chocovdr( const struct ns10_history *h )
{
	return ( HBIT( h, 9 ) & ( PAR48( 0, 0x10065810, h->chi, h->clo ) ^ PAR48( 0, 0x21005810, h->phi, h->plo ) ) ) << 10;
}

static UINT16 nl_gamshara( const struct ns10_history *h )
{
	return ( HBIT( h, 7 ) & HBIT( h, 13 ) ) << 2;
}

static UINT16 nl_knpuzzle( const struct ns10_history *h )
{
	return ( HBIT( h, 19 ) & ( PAR48( 0, 0x14001290, h->chi, h->clo ) ^ PAR48( 0, 0x00021290, h->phi, h->plo ) ) ) << 1;
}

static UINT16 nl_konotako( const struct ns10_history *h )
{
	return ( HBIT( h, 7 ) & HBIT( h, 15 ) ) << 15;
}

static UINT16 nl_mrdrilrg( const struct ns10_history *h )
{
	UINT32 mlo = h->clo ^ h->plo, mhi = h->chi ^ h->phi;
	return ( PAR48( 0x1201, 0x00400000, mhi, mlo ) & PAR48( 0x0118, 0x00020000, mhi, mlo ) ) * 0x11;
}

static UINT16 nl_panikuru( const struct ns10_history *h )
{
	return ( ( PAR48( 0, 0x88300281, h->chi, h->clo ) ^ PAR48( 0, 0x04600281, h->phi, h->plo ) ) &
		( PAR48( 0xa131, 0x40090000, h->chi, h->clo ) ^ PAR48( 0x8062, 0x40090000, h->phi, h->plo ) ) ) << 2;
}

static UINT16 nl_startrgn( const struct ns10_history *h )
{
	return ( HBIT( h, 12 ) & HBIT( h, 14 ) ) << 4;
}

static UINT16 nl_ptblank3( const struct ns10_history *h )
{
	return h->bit << 11;
}

static const UINT16 mrdrilrg_iv[ 16 ] =
{
	0x0000, 0x0000, 0x0000, 0x2000, 0x801a, 0x6000, 0x0002, 0x0000,
	0x0000, 0x0018, 0x0000, 0x0420, 0x0000, 0x8800, 0x0000, 0xc000
};

static const UINT16 panikuru_iv[ 16 ] =
{
	0x0000, 0x020e, 0x0412, 0x0411, 0x0204, 0x001b, 0x0009, 0x0207,
	0x2040, 0x2440, 0x0012, 0x0001, 0x0614, 0x000a, 0x0214, 0x041b
};

static const UINT8 mrdrilr2_bitswap[ 16 ] = { 12, 13, 15, 14, 11, 10, 9, 8, 7, 6, 4, 1, 2, 5, 0, 3 };

#define NS10_CRYPT( name, nl, iv, table ) \
	static const struct ns10_crypt name##_crypt = { name##_bitswap, name##_emask, name##_dmask, name##_xmask, nl, iv, table };

static const struct ns10_crypt mrdrilr2_crypt = { mrdrilr2_bitswap, NULL, NULL, 0, NULL, NULL, 0 };
NS10_CRYPT( chocovdr, nl_chocovdr, NULL, 0 )
NS10_CRYPT( gamshara, nl_gamshara, NULL, 0 )
NS10_CRYPT( gjspace, nl_zero, NULL, 0 )
NS10_CRYPT( knpuzzle, nl_knpuzzle, NULL, 0 )
NS10_CRYPT( konotako, nl_konotako, NULL, 0 )
NS10_CRYPT( mrdrilrg, nl_mrdrilrg, mrdrilrg_iv, 0 )
NS10_CRYPT( panikuru, nl_panikuru, panikuru_iv, 0 )
NS10_CRYPT( startrgn, nl_startrgn, NULL, 0 )
NS10_CRYPT( ptblank3, nl_ptblank3, NULL, 1 )

/***************************************************************************

  The main board

***************************************************************************/

static UINT16 ns10_int;
static UINT32 exio_ident_bit;
static UINT32 exio_ident_byte;
static UINT8 exio_ident_code;

static mame_timer *ns10_io_timer;
static double ns10_io_timer_left;

static data32_t *ns10_sram;		/* 0x1f500000 (MEM(N)) */

/* the I2C bus: nothing answers but an acknowledge (as MAME) */
enum { I2CP_IDLE, I2CP_RECEIVE_BYTE, I2CP_RECEIVE_ACK_1, I2CP_RECEIVE_ACK_0 };
static UINT16 i2c_dev_clock, i2c_dev_data, i2c_host_clock, i2c_host_data, i2c_prev_clock, i2c_prev_data;
static UINT32 i2cp_mode, i2c_byte, i2c_bit;

static void i2c_update( void )
{
	UINT16 clock = i2c_dev_clock & i2c_host_clock & 1;
	UINT16 data = i2c_dev_data & i2c_host_data & 1;

	if( i2c_prev_data == data && i2c_prev_clock == clock )
		return;

	switch( i2cp_mode )
	{
	case I2CP_IDLE:
		if( clock && !data )
		{
			i2c_byte = 0;
			i2c_bit = 7;
			i2cp_mode = I2CP_RECEIVE_BYTE;
		}
		break;
	case I2CP_RECEIVE_BYTE:
		if( clock && data && !i2c_prev_data )
			i2cp_mode = I2CP_IDLE;
		else if( clock && !i2c_prev_clock )
		{
			i2c_byte |= data << i2c_bit;
			if( i2c_bit == 0 )
			{
				i2cp_mode = I2CP_RECEIVE_ACK_1;
				i2c_dev_data = 0;
				data = 0;
			}
			else
				i2c_bit--;
		}
		break;
	case I2CP_RECEIVE_ACK_1:
		if( clock && !i2c_prev_clock )
			i2cp_mode = I2CP_RECEIVE_ACK_0;
		break;
	case I2CP_RECEIVE_ACK_0:
		if( !clock && i2c_prev_clock )
		{
			i2c_dev_data = 1;
			data = i2c_host_data & 1;
			i2c_byte = 0;
			i2c_bit = 7;
			i2cp_mode = I2CP_RECEIVE_BYTE;
		}
		break;
	}
	i2c_prev_data = data;
	i2c_prev_clock = clock;
}

static UINT16 exio_ident_r( void )
{
	UINT16 res;
	int b = ( exio_ident_code >> exio_ident_bit ) & 1;

	res = exio_ident_byte >= 0x20 ? 3 : ( b ? 3 : 0 );
	if( exio_ident_bit == 0 )
	{
		exio_ident_bit = 7;
		exio_ident_byte++;
	}
	else
		exio_ident_bit--;
	return res;
}

static void ns10_io_interrupt( int param )
{
	ns10_int |= 8;
	psx_irq_set( 0x400 );
}

/* 0.78 does not save its timers: the I/O timer goes on with its phase */
static void ns10_presave( void )
{
	ns10_io_timer_left = timer_timeleft( ns10_io_timer );
	if( ns10_io_timer_left < 0 )
		ns10_io_timer_left = 0;
}

static void ns10_postload( void )
{
	timer_adjust( ns10_io_timer, ns10_io_timer_left, 0, TIME_IN_HZ( 100 ) );
}

/***************************************************************************

  MEM(M): the flash and the mask ROMs

***************************************************************************/

static UINT32 memm_bank_idx;
static UINT32 memm_direct;

static UINT16 memm_range_r( UINT32 offset )
{
	const UINT8 *base;
	UINT32 length;
	UINT32 a;
	UINT16 data;

	if( memm_direct == 0 )
		return intelfsh_read( 0, 0x100000 * memm_bank_idx + offset );

	/* the ROMs, not the flash */
	if( memm_bank_idx < 0x10 )
	{
		base = memory_region( REGION_USER3 );
		length = memory_region_length( REGION_USER3 );
		a = ( 0x100000 * memm_bank_idx + offset ) * 2;
	}
	else
	{
		base = memory_region( REGION_USER4 );
		length = memory_region_length( REGION_USER4 );
		a = ( 0x100000 * ( memm_bank_idx - 0x10 ) + offset ) * 2;
	}
	if( base == NULL || a + 1 >= length )
		return 0xffff;
	data = base[ a ] | ( base[ a + 1 ] << 8 );

	if( crypt_active )
		return crypt_decrypt( data );
	if( memm_bank_idx < 0x10 )
		data = unscramble( data );
	return data;
}

/***************************************************************************

  MEM(N): the Samsung K9F2808U0B NAND flashes (nandflash.cpp)

  512 + 16 byte pages, 32 pages in a block, 32768 pages; one column and
  two row address cycles; no sequential row read; reads take no time.

***************************************************************************/

#define NAND_PAGE_SIZE		( 0x210 )
#define NAND_PAGE_DATA		( 0x200 )
#define NAND_PAGES			( 0x8000 )
#define NAND_SIZE			( NAND_PAGE_SIZE * NAND_PAGES )
#define NAND_BLOCK_PAGES	( 32 )
#define NAND_MAX			( 16 )

/* the settings: block 1 of the first NAND */
#define NAND_NV_OFFSET		( NAND_BLOCK_PAGES * NAND_PAGE_SIZE )
#define NAND_NV_SIZE		( NAND_BLOCK_PAGES * NAND_PAGE_SIZE )

enum { SM_M_INIT, SM_M_READ, SM_M_PROGRAM, SM_M_ERASE, SM_M_READSTATUS, SM_M_READID, SM_M_30, SM_M_RANDOM_DATA_INPUT, SM_M_RANDOM_DATA_OUTPUT };
enum { SM_PM_A, SM_PM_B, SM_PM_C };

struct ns10_nand
{
	UINT8 *data;
	UINT32 mode;
	UINT32 pointer_mode;
	UINT32 page_addr;
	UINT32 byte_addr;
	UINT32 addr_load_ptr;
	UINT32 program_byte_count;
	UINT32 status;
	UINT32 accumulated_status;
	UINT32 mode_3065;
	UINT8 pagereg[ NAND_PAGE_SIZE ];
};

static struct ns10_nand nand[ NAND_MAX ];
static int nand_count;
static int nand_warned;

static UINT32 memn_ctrl_reg;
static UINT32 memn_device_idx;
static UINT32 memn_nand_address;

static void nand_reset( struct ns10_nand *n )
{
	n->mode = SM_M_INIT;
	n->pointer_mode = SM_PM_A;
	n->page_addr = 0;
	n->byte_addr = 0;
	n->addr_load_ptr = 0;
	n->program_byte_count = 0;
	n->accumulated_status = 0;
	n->mode_3065 = 0;
	n->status = 0xc0;
	memset( n->pagereg, 0, sizeof( n->pagereg ) );
}

/* a write outside the settings block is not kept in the NVRAM file */
static void nand_written( struct ns10_nand *n, UINT32 offset )
{
	if( ( n != &nand[ 0 ] || offset < NAND_NV_OFFSET || offset >= NAND_NV_OFFSET + NAND_NV_SIZE ) && !nand_warned )
	{
		nand_warned = 1;
		log_cb( RETRO_LOG_WARN, LOGPRE "namcos10: NAND %d written at %06x, outside the settings block\n", (int)( n - nand ), offset );
	}
}

static void nand_command_w( struct ns10_nand *n, UINT8 data )
{
	int i;

	switch( data )
	{
	case 0xff:	/* reset */
		n->mode = SM_M_INIT;
		n->pointer_mode = SM_PM_A;
		n->status = ( n->status & 0x80 ) | 0x40;
		n->accumulated_status = 0;
		n->mode_3065 = 0;
		break;
	case 0x00:	/* read */
		n->mode = SM_M_READ;
		n->pointer_mode = SM_PM_A;
		n->addr_load_ptr = 0;
		break;
	case 0x01:
		n->mode = SM_M_READ;
		n->pointer_mode = SM_PM_B;
		n->addr_load_ptr = 0;
		break;
	case 0x50:
		n->mode = SM_M_READ;
		n->pointer_mode = SM_PM_C;
		n->addr_load_ptr = 0;
		break;
	case 0x80:	/* page program */
		n->mode = SM_M_PROGRAM;
		n->addr_load_ptr = 0;
		n->program_byte_count = 0;
		memset( n->pagereg, 0xff, NAND_PAGE_SIZE );
		break;
	case 0x10:
	case 0x15:
		if( n->mode != SM_M_PROGRAM && n->mode != SM_M_RANDOM_DATA_INPUT )
			n->mode = SM_M_INIT;
		else
		{
			n->status = ( n->status & 0x80 ) | n->accumulated_status;
			if( n->page_addr < NAND_PAGES )
			{
				UINT8 *p = n->data + n->page_addr * NAND_PAGE_SIZE;
				for( i = 0; i < NAND_PAGE_SIZE; i++ )
					p[ i ] &= n->pagereg[ i ];
				nand_written( n, n->page_addr * NAND_PAGE_SIZE );
			}
			n->status |= 0x40;
			n->accumulated_status = data == 0x15 ? n->status & 0x1f : 0;
			n->mode = SM_M_INIT;
		}
		break;
	case 0x60:	/* block erase */
		n->mode = SM_M_ERASE;
		n->page_addr = 0;
		n->addr_load_ptr = 0;
		break;
	case 0xd0:
		if( n->mode != SM_M_ERASE )
			n->mode = SM_M_INIT;
		else
		{
			UINT32 page = n->page_addr & ~( NAND_BLOCK_PAGES - 1 );
			n->status &= 0x80;
			if( page < NAND_PAGES )
			{
				memset( n->data + page * NAND_PAGE_SIZE, 0xff, NAND_BLOCK_PAGES * NAND_PAGE_SIZE );
				nand_written( n, page * NAND_PAGE_SIZE );
			}
			n->status |= 0x40;
			n->mode = SM_M_INIT;
			if( n->pointer_mode == SM_PM_B )
				n->pointer_mode = SM_PM_A;
		}
		break;
	case 0x70:	/* read status */
		n->mode = SM_M_READSTATUS;
		break;
	case 0x90:	/* read ID */
		n->mode = SM_M_READID;
		n->addr_load_ptr = 0;
		break;
	case 0x30:	/* one column address cycle */
		n->mode = SM_M_30;
		break;
	case 0x65:
		if( n->mode != SM_M_30 )
			n->mode = SM_M_INIT;
		else
			n->mode_3065 = 1;
		break;
	case 0x05:	/* random data output */
		if( n->mode != SM_M_READ && n->mode != SM_M_RANDOM_DATA_OUTPUT )
			n->mode = SM_M_INIT;
		else
		{
			n->mode = SM_M_RANDOM_DATA_OUTPUT;
			n->addr_load_ptr = 0;
		}
		break;
	case 0xe0:
		if( n->mode != SM_M_RANDOM_DATA_OUTPUT )
			n->mode = SM_M_INIT;
		break;
	case 0x85:	/* random data input */
		if( n->mode != SM_M_PROGRAM && n->mode != SM_M_RANDOM_DATA_INPUT )
			n->mode = SM_M_INIT;
		else
		{
			n->mode = SM_M_RANDOM_DATA_INPUT;
			n->addr_load_ptr = 0;
			n->program_byte_count = 0;
		}
		break;
	default:
		logerror( "namcos10: NAND command %02x\n", data );
		n->mode = SM_M_INIT;
		break;
	}
}

static void nand_address_w( struct ns10_nand *n, UINT8 data )
{
	switch( n->mode )
	{
	case SM_M_READ:
	case SM_M_PROGRAM:
		if( n->addr_load_ptr == 0 )
		{
			n->page_addr = 0;
			switch( n->pointer_mode )
			{
			case SM_PM_A:
				n->byte_addr = data;
				break;
			case SM_PM_B:
				n->byte_addr = data + 256;
				n->pointer_mode = SM_PM_A;
				break;
			case SM_PM_C:
				n->byte_addr = ( data & 0x0f ) + ( n->mode_3065 ? 256 : NAND_PAGE_DATA );
				break;
			}
		}
		else if( n->addr_load_ptr < 3 )
		{
			int shift = ( n->addr_load_ptr - 1 ) * 8;
			n->page_addr = ( n->page_addr & ~( 0xff << shift ) ) | ( data << shift );
		}
		n->addr_load_ptr++;
		break;
	case SM_M_ERASE:
		if( n->addr_load_ptr < 2 )
		{
			int shift = n->addr_load_ptr * 8;
			n->page_addr = ( n->page_addr & ~( 0xff << shift ) ) | ( data << shift );
		}
		n->addr_load_ptr++;
		break;
	case SM_M_RANDOM_DATA_INPUT:
	case SM_M_RANDOM_DATA_OUTPUT:
		if( n->addr_load_ptr < 1 )
			n->byte_addr = data;
		n->addr_load_ptr++;
		break;
	case SM_M_READID:
		if( n->addr_load_ptr == 0 )
			n->byte_addr = data;
		n->addr_load_ptr++;
		break;
	default:
		break;
	}
}

static UINT8 nand_data_r8( struct ns10_nand *n )
{
	static const UINT8 id[ 2 ] = { 0xec, 0x73 };
	UINT8 reply = 0;

	switch( n->mode )
	{
	case SM_M_READ:
	case SM_M_RANDOM_DATA_OUTPUT:
		if( !n->mode_3065 && n->byte_addr < NAND_PAGE_SIZE && n->page_addr < NAND_PAGES )
			reply = n->data[ n->page_addr * NAND_PAGE_SIZE + n->byte_addr ];
		else
			reply = 0xff;
		n->byte_addr++;
		break;
	case SM_M_READSTATUS:
		reply = n->status & 0xc1;
		if( ( n->status & 0x40 ) == 0 )
			reply &= ~0x40;
		break;
	case SM_M_READID:
		reply = n->byte_addr < 2 ? id[ n->byte_addr ] : 0;
		n->byte_addr++;
		break;
	default:
		break;
	}
	return reply;
}

static void nand_data_w8( struct ns10_nand *n, UINT8 data )
{
	if( n->mode == SM_M_PROGRAM || n->mode == SM_M_RANDOM_DATA_INPUT )
	{
		if( n->program_byte_count++ < NAND_PAGE_SIZE && n->byte_addr < NAND_PAGE_SIZE )
			n->pagereg[ n->byte_addr ] = data;
		n->byte_addr++;
		if( n->byte_addr == NAND_PAGE_SIZE )
			n->byte_addr = n->pointer_mode != SM_PM_C ? 0 : NAND_PAGE_DATA;
	}
}

static UINT16 memn_nand_data_r( void )
{
	struct ns10_nand *n;
	UINT16 data;

	if( memn_device_idx >= nand_count )
		return 0xffff;
	n = &nand[ memn_device_idx ];

	data = nand_data_r8( n ) << 8;
	data |= nand_data_r8( n );

	if( crypt_active )
		return crypt_decrypt( data );

	/* blocks 0 (the bad block table) and 1 (the settings) of the first */
	/* NAND are plain */
	if( memn_device_idx == 0 && ( ( ( memn_nand_address >> 8 ) & 0xffff ) >> 5 ) < 2 )
		return data;

	return unscramble( data );
}

/* DMA channel 5: the NAND data, two words at a time */
static void memn_dma_read( UINT32 n_address, INT32 n_size )
{
	data32_t *p_n_ram = (data32_t *)memory_region( REGION_CPU1 );
	UINT32 n_ram = memory_region_length( REGION_CPU1 );

	n_address &= n_ram - 1;
	while( n_size > 0 )
	{
		UINT16 lo = memn_nand_data_r();
		UINT16 hi = memn_nand_data_r();
		p_n_ram[ n_address / 4 ] = lo | ( hi << 16 );
		n_address = ( n_address + 4 ) & ( n_ram - 1 );
		n_size--;
	}
}

/***************************************************************************

  EXIO (namcos10_exio.cpp): the I/O board of the gun games. Its TMP95C061
  is not populated on the EXIO(G) of Point Blank 3 and Gunbalina, so only
  the CPLD is here: the RAM of the MCU (bytes, one in each word), the
  control registers and the light gun counters. It is at 0x1fe00000, over
  the BIOS space: the rest of the window reads the BIOS. The ident is the
  one of an EXIO without CPU (0x32): with the 0x30 of MAME, the games load
  a program in the MCU and wait for it, which needs the MCU

***************************************************************************/

#define NS10_PORT_GUN		( 3 )
#define EXIO_RAM_SIZE		( 0x5000 )

static int ns10_has_exio;
static UINT8 *exio_ram;
static UINT16 exio_ctrl;
static UINT16 exio_bus_req;
static int ns10_gun_min[ 2 ], ns10_gun_max[ 2 ];

static UINT16 exio_r16( UINT32 a )
{
	UINT32 o = a - 0x1fe00000;

	if( o >= 0x06000 && o < 0x10000 )
		return exio_ram[ ( o - 0x06000 ) / 2 ];
	if( o >= 0x10000 && o < 0x10004 )
		return exio_ctrl;
	if( o >= 0x18000 && o < 0x18004 )
		return exio_bus_req == 1 ? 2 : 0;
	if( o >= 0x28000 && o < 0x28004 )
		return 0;	/* no MCU */
	if( o >= 0x50000 && o < 0x50004 )
		return 0xffff;	/* EXIO_IN1, EXIO_IN2: nothing */
	if( o >= 0x58000 && o < 0x58004 )
		return 0xffff;
	if( o >= 0x80000 && o < 0xc0000 && ( o & 0x7ffc ) == 0 )
	{
		/* the gun counters: bits 0-5, then 6-21 of the position */
		int n = ( o - 0x80000 ) >> 15;
		UINT32 val = readinputport( NS10_PORT_GUN + n / 2 );
		return ( n & 1 ) ? ( val >> 6 ) & 0xffff : val & 0x3f;
	}
	/* the BIOS space under the window */
	{
		data32_t w = ( (data32_t *)memory_region( REGION_USER2 ) )[ ( a - 0x1fc00000 ) / 4 ];
		return ( a & 2 ) ? w >> 16 : w & 0xffff;
	}
}

static void exio_w16( UINT32 a, UINT16 data )
{
	UINT32 o = a - 0x1fe00000;

	if( o >= 0x06000 && o < 0x10000 )
		exio_ram[ ( o - 0x06000 ) / 2 ] = data & 0xff;
	else if( o >= 0x10000 && o < 0x10004 )
		exio_ctrl = data;
	else if( o >= 0x18000 && o < 0x18004 )
		exio_bus_req = data;
}

static data32_t exio_r32( UINT32 a, data32_t mem_mask )
{
	data32_t data = 0;

	a &= 0x1fffffff;
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
		data |= exio_r16( a );
	if( ( mem_mask & 0xffff0000 ) != 0xffff0000 )
		data |= (data32_t)exio_r16( a + 2 ) << 16;
	return data;
}

static void exio_w32( UINT32 a, data32_t data, data32_t mem_mask )
{
	a &= 0x1fffffff;
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
		exio_w16( a, data & 0xffff );
	if( ( mem_mask & 0xffff0000 ) != 0xffff0000 )
		exio_w16( a + 2, data >> 16 );
}

#define EXIO_HANDLERS( tag, base ) \
	static READ32_HANDLER( exio_##tag##_r ) { return exio_r32( ( base ) + offset * 4, mem_mask ); } \
	static WRITE32_HANDLER( exio_##tag##_w ) { exio_w32( ( base ) + offset * 4, data, mem_mask ); }

EXIO_HANDLERS( 1f, 0x1fe00000 )
EXIO_HANDLERS( 9f, 0x9fe00000 )
EXIO_HANDLERS( bf, 0xbfe00000 )

static VIDEO_UPDATE( namcos10_gun )
{
	const struct rectangle *visible = &Machine->visible_area;
	int n_gun;

	video_update_psx( bitmap, cliprect );

	for( n_gun = 0; n_gun < 2; n_gun++ )
	{
		int n_x = readinputport( NS10_PORT_GUN + n_gun * 2 ) - ns10_gun_min[ 0 ];
		int n_y = readinputport( NS10_PORT_GUN + n_gun * 2 + 1 ) - ns10_gun_min[ 1 ];
		n_x = visible->min_x + n_x * ( visible->max_x - visible->min_x ) / ( ns10_gun_max[ 0 ] - ns10_gun_min[ 0 ] );
		n_y = visible->min_y + n_y * ( visible->max_y - visible->min_y ) / ( ns10_gun_max[ 1 ] - ns10_gun_min[ 1 ] );
		draw_crosshair( n_gun + 1, bitmap, n_x, n_y, cliprect );
	}
}

/***************************************************************************

  The registers, 16 bits wide

***************************************************************************/

static UINT16 ns10_r16( UINT32 a )
{
	if( a >= 0x1fb00000 )
	{
		if( ns10_board == NS10_MEMM && a >= 0x1fb40000 && a < 0x1fb60000 )
		{
			/* a guess of MAME that works */
			memm_direct = 1;
			return 0;
		}
		switch( a )
		{
		case 0x1fba0000: return readinputport( 0 );
		case 0x1fba0002: return exio_ident_r();
		case 0x1fba0004: return readinputport( 1 );
		case 0x1fba0006: return readinputport( 2 );
		case 0x1fba0008: return i2c_dev_clock & i2c_host_clock & 1;
		case 0x1fba000a: return i2c_dev_data & i2c_host_data & 1;
		case 0x1fba001a: return ns10_int;
		}
		if( a >= 0x1fbf0000 && a < 0x1fbf0010 )
			return 1;	/* the CD-ROM drive is there? */
		return 0;
	}

	if( ns10_board == NS10_MEMM )
	{
		if( a >= 0x1f400000 )
			return memm_range_r( ( a - 0x1f400000 ) / 2 );
		return 0;
	}

	switch( a )
	{
	case 0x1f400000: return 0;	/* NAND ready / busy: never busy */
	case 0x1f450000: return memn_nand_data_r();
	case 0x1f470000: return memn_ctrl_reg;
	}
	return 0;
}

static void ns10_w16( UINT32 a, UINT16 data, UINT16 mask )
{
	if( a >= 0x1fb00000 )
	{
		if( ns10_board == NS10_MEMM && a >= 0x1fb40000 && a < 0x1fb60000 )
		{
			memm_bank_idx = ( a - 0x1fb40000 ) / 2;
			memm_direct = memm_bank_idx >= 0x10;
			return;
		}
		switch( a )
		{
		case 0x1fba0002:
			exio_ident_bit = 7;
			exio_ident_byte = 0;
			return;
		case 0x1fba0008:
			i2c_host_clock = ( i2c_host_clock & ~mask ) | ( data & mask );
			i2c_update();
			return;
		case 0x1fba000a:
			i2c_host_data = ( i2c_host_data & ~mask ) | ( data & mask );
			i2c_update();
			return;
		case 0x1fba0012:
		case 0x1fba0014:
		case 0x1fba0016:
		case 0x1fba0018:
			ns10_int &= ~( 1 << ( ( a - 0x1fba0012 ) / 2 ) );
			return;
		}
		return;
	}

	if( a == 0x1f300000 || ( ns10_board == NS10_MEMN && a == 0x1f380000 ) )
	{
		crypt_switch_w( data );
		return;
	}

	if( ns10_board == NS10_MEMM )
	{
		/* the flash, at the offset in the window (the bank is not used) */
		if( a >= 0x1f400000 && memm_direct == 0 )
			intelfsh_write( 0, ( a - 0x1f400000 ) / 2, data );
		return;
	}

	switch( a )
	{
	case 0x1f410000:
	case 0x1f420000:
	case 0x1f430000:
	case 0x1f440000:
		/* 8-bit registers, on the low byte */
		if( ( mask & 0xff ) == 0 )
			return;
		data &= 0xff;
		if( a == 0x1f410000 )
		{
			if( memn_device_idx < nand_count )
				nand_command_w( &nand[ memn_device_idx ], data );
			return;
		}
		if( memn_device_idx < nand_count )
			nand_address_w( &nand[ memn_device_idx ], data );
		if( a == 0x1f420000 )
			memn_nand_address = ( memn_nand_address & 0xffff00 ) | data;
		else if( a == 0x1f430000 )
			memn_nand_address = ( memn_nand_address & 0xff00ff ) | ( data << 8 );
		else
			memn_nand_address = ( memn_nand_address & 0x00ffff ) | ( data << 16 );
		return;
	case 0x1f450000:
		if( memn_device_idx < nand_count )
		{
			nand_data_w8( &nand[ memn_device_idx ], data >> 8 );
			nand_data_w8( &nand[ memn_device_idx ], data & 0xff );
		}
		return;
	case 0x1f460000:
		if( ( memn_ctrl_reg & 4 ) == 0 )
			memn_device_idx = data;
		return;
	case 0x1f470000:
		memn_ctrl_reg = data;
		return;
	}
}

static data32_t ns10_r32( UINT32 a, data32_t mem_mask )
{
	data32_t data = 0;

	a &= 0x1fffffff;
	/* the low word first, for the decrypters; a byte read reads its word */
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
		data |= ns10_r16( a );
	if( ( mem_mask & 0xffff0000 ) != 0xffff0000 )
		data |= (data32_t)ns10_r16( a + 2 ) << 16;
	return data;
}

static void ns10_w32( UINT32 a, data32_t data, data32_t mem_mask )
{
	a &= 0x1fffffff;
	if( ( mem_mask & 0x0000ffff ) != 0x0000ffff )
		ns10_w16( a, data & 0xffff, ~mem_mask & 0xffff );
	if( ( mem_mask & 0xffff0000 ) != 0xffff0000 )
		ns10_w16( a + 2, data >> 16, ( ~mem_mask >> 16 ) & 0xffff );
}

/* the handlers of each mirror: the offset is from the start of the range */
#define NS10_HANDLERS( tag, base ) \
	static READ32_HANDLER( ns10_##tag##_r ) { return ns10_r32( ( base ) + offset * 4, mem_mask ); } \
	static WRITE32_HANDLER( ns10_##tag##_w ) { ns10_w32( ( base ) + offset * 4, data, mem_mask ); }

NS10_HANDLERS( rom1f, 0x1f300000 )
NS10_HANDLERS( rom9f, 0x9f300000 )
NS10_HANDLERS( rombf, 0xbf300000 )
NS10_HANDLERS( io1f, 0x1fb00000 )
NS10_HANDLERS( io9f, 0x9fb00000 )
NS10_HANDLERS( iobf, 0xbfb00000 )

/***************************************************************************

  Memory maps: the PSX peripherals, the registers of the board (and their
  mirrors at 0x9f000000 and 0xbf000000). Every mirror of a RAM has a bank
  of its own

***************************************************************************/

#define NS10_READ \
	{ 0x00000000, 0x003fffff, MRA32_RAM },    /* ram */ \
	{ 0x00400000, 0x007fffff, MRA32_BANK3 },  /* ram mirror */ \
	{ 0x1f800000, 0x1f8003ff, MRA32_BANK1 },  /* scratchpad */ \
	{ 0x1f801000, 0x1f80100f, MRA32_RAM }, \
	{ 0x1f801010, 0x1f801013, MRA32_NOP }, \
	{ 0x1f801014, 0x1f801017, psx_spu_delay_r }, \
	{ 0x1f801018, 0x1f80102f, MRA32_NOP }, \
	{ 0x1f801040, 0x1f80105f, psx_sio_r }, \
	{ 0x1f801060, 0x1f80106f, MRA32_RAM }, \
	{ 0x1f801070, 0x1f801077, psx_irq_r }, \
	{ 0x1f801080, 0x1f8010ff, psx_dma_r }, \
	{ 0x1f801100, 0x1f80113f, psx_counter_r }, \
	{ 0x1f801810, 0x1f801817, psx_gpu_r }, \
	{ 0x1f801820, 0x1f801827, psx_mdec_r }, \
	{ 0x1f801c00, 0x1f801dff, psx_spu_r }, \
	{ 0x1f802020, 0x1f802033, MRA32_RAM }, \
	{ 0x1fb00000, 0x1fbfffff, ns10_io1f_r }, \
	{ 0x1fc00000, 0x1fffffff, MRA32_BANK2 },  /* bios */ \
	{ 0x80000000, 0x803fffff, MRA32_BANK4 },  /* ram mirror */ \
	{ 0x80400000, 0x807fffff, MRA32_BANK5 }, \
	{ 0x9fb00000, 0x9fbfffff, ns10_io9f_r }, \
	{ 0x9fc00000, 0x9fffffff, MRA32_BANK6 },  /* bios mirror */ \
	{ 0xa0000000, 0xa03fffff, MRA32_BANK7 },  /* ram mirror */ \
	{ 0xbfb00000, 0xbfbfffff, ns10_iobf_r }, \
	{ 0xbfc00000, 0xbfffffff, MRA32_BANK8 },  /* bios mirror */

#define NS10_WRITE \
	{ 0x00000000, 0x003fffff, MWA32_RAM }, \
	{ 0x00400000, 0x007fffff, MWA32_BANK3 }, \
	{ 0x1f800000, 0x1f8003ff, MWA32_BANK1 }, \
	{ 0x1f801000, 0x1f80100f, MWA32_RAM }, \
	{ 0x1f801010, 0x1f801013, MWA32_NOP }, \
	{ 0x1f801014, 0x1f801017, psx_spu_delay_w }, \
	{ 0x1f801018, 0x1f80102f, MWA32_NOP }, \
	{ 0x1f801040, 0x1f80105f, psx_sio_w }, \
	{ 0x1f801060, 0x1f80106f, MWA32_RAM }, \
	{ 0x1f801070, 0x1f801077, psx_irq_w }, \
	{ 0x1f801080, 0x1f8010ff, psx_dma_w }, \
	{ 0x1f801100, 0x1f80113f, psx_counter_w }, \
	{ 0x1f801810, 0x1f801817, psx_gpu_w }, \
	{ 0x1f801820, 0x1f801827, psx_mdec_w }, \
	{ 0x1f801c00, 0x1f801dff, psx_spu_w }, \
	{ 0x1f802020, 0x1f802033, MWA32_RAM }, \
	{ 0x1f802040, 0x1f802043, MWA32_NOP }, \
	{ 0x1fb00000, 0x1fbfffff, ns10_io1f_w }, \
	{ 0x1fc00000, 0x1fffffff, MWA32_ROM }, \
	{ 0x80000000, 0x803fffff, MWA32_BANK4 }, \
	{ 0x80400000, 0x807fffff, MWA32_BANK5 }, \
	{ 0x9fb00000, 0x9fbfffff, ns10_io9f_w }, \
	{ 0x9fc00000, 0x9fffffff, MWA32_ROM }, \
	{ 0xa0000000, 0xa03fffff, MWA32_BANK7 }, \
	{ 0xbfb00000, 0xbfbfffff, ns10_iobf_w }, \
	{ 0xbfc00000, 0xbfffffff, MWA32_ROM }, \
	{ 0xfffe0130, 0xfffe0133, MWA32_NOP },

static MEMORY_READ32_START( memm_readmem )
	{ 0x1f300000, 0x1f5fffff, ns10_rom1f_r },
	{ 0x9f300000, 0x9f5fffff, ns10_rom9f_r },
	{ 0xbf300000, 0xbf5fffff, ns10_rombf_r },
	NS10_READ
MEMORY_END

static MEMORY_WRITE32_START( memm_writemem )
	{ 0x1f300000, 0x1f5fffff, ns10_rom1f_w },
	{ 0x9f300000, 0x9f5fffff, ns10_rom9f_w },
	{ 0xbf300000, 0xbf5fffff, ns10_rombf_w },
	NS10_WRITE
MEMORY_END

static MEMORY_READ32_START( memn_readmem )
	{ 0x1f300000, 0x1f4fffff, ns10_rom1f_r },
	{ 0x1f500000, 0x1f5fffff, MRA32_BANK9 },	/* ram */
	{ 0x1fe40000, 0x1fe40003, MRA32_NOP },
	{ 0x1fe48000, 0x1fe48003, MRA32_NOP },
	{ 0x9f300000, 0x9f4fffff, ns10_rom9f_r },
	{ 0x9f500000, 0x9f5fffff, MRA32_BANK10 },
	{ 0xbf300000, 0xbf4fffff, ns10_rombf_r },
	{ 0xbf500000, 0xbf5fffff, MRA32_BANK11 },
	NS10_READ
MEMORY_END

static MEMORY_WRITE32_START( memn_writemem )
	{ 0x1f300000, 0x1f4fffff, ns10_rom1f_w },
	{ 0x1f500000, 0x1f5fffff, MWA32_BANK9 },
	{ 0x1fe40000, 0x1fe40003, MWA32_NOP },
	{ 0x1fe48000, 0x1fe48003, MWA32_NOP },
	{ 0x9f300000, 0x9f4fffff, ns10_rom9f_w },
	{ 0x9f500000, 0x9f5fffff, MWA32_BANK10 },
	{ 0xbf300000, 0xbf4fffff, ns10_rombf_w },
	{ 0xbf500000, 0xbf5fffff, MWA32_BANK11 },
	NS10_WRITE
MEMORY_END

/***************************************************************************

  Init

***************************************************************************/

static void ns10_driver_init( int board )
{
	data32_t *bios = (data32_t *)memory_region( REGION_USER2 );
	UINT32 i;

	ns10_board = board;

	/* the BIOS space reads 0x55 where nothing is copied (as MAME) */
	for( i = 0; i < memory_region_length( REGION_USER2 ) / 4; i++ )
		bios[ i ] = 0x55555555;

	cpu_setbank( 1, memory_region( REGION_USER1 ) );
	cpu_setbank( 2, memory_region( REGION_USER2 ) );
	cpu_setbank( 3, memory_region( REGION_CPU1 ) );
	cpu_setbank( 4, memory_region( REGION_CPU1 ) );
	cpu_setbank( 5, memory_region( REGION_CPU1 ) );
	cpu_setbank( 6, memory_region( REGION_USER2 ) );
	cpu_setbank( 7, memory_region( REGION_CPU1 ) );
	cpu_setbank( 8, memory_region( REGION_USER2 ) );

	psx_driver_init();
	/* the movies of Point Blank 3 show only with a DrawSync callback after */
	/* the linked list, as MAME */
	psx_dma_set_gpu_delay();

	exio_ident_code = 0xff;	/* no EXIO board */
	ns10_has_exio = 0;
	ns10_io_timer = timer_alloc( ns10_io_interrupt );

	state_save_register_UINT32( "namcos10", 0, "scratchpad", (data32_t *)memory_region( REGION_USER1 ), 0x400 / 4 );
	state_save_register_UINT16( "namcos10", 0, "int", &ns10_int, 1 );
	state_save_register_UINT32( "namcos10", 0, "exio_ident_bit", &exio_ident_bit, 1 );
	state_save_register_UINT32( "namcos10", 0, "exio_ident_byte", &exio_ident_byte, 1 );
	state_save_register_double( "namcos10", 0, "io_timer_left", &ns10_io_timer_left, 1 );
	state_save_register_UINT16( "namcos10", 0, "i2c_dev_clock", &i2c_dev_clock, 1 );
	state_save_register_UINT16( "namcos10", 0, "i2c_dev_data", &i2c_dev_data, 1 );
	state_save_register_UINT16( "namcos10", 0, "i2c_host_clock", &i2c_host_clock, 1 );
	state_save_register_UINT16( "namcos10", 0, "i2c_host_data", &i2c_host_data, 1 );
	state_save_register_UINT16( "namcos10", 0, "i2c_prev_clock", &i2c_prev_clock, 1 );
	state_save_register_UINT16( "namcos10", 0, "i2c_prev_data", &i2c_prev_data, 1 );
	state_save_register_UINT32( "namcos10", 0, "i2cp_mode", &i2cp_mode, 1 );
	state_save_register_UINT32( "namcos10", 0, "i2c_byte", &i2c_byte, 1 );
	state_save_register_UINT32( "namcos10", 0, "i2c_bit", &i2c_bit, 1 );
	state_save_register_UINT8( "namcos10", 0, "crypt_active", &crypt_active, 1 );
	state_save_register_UINT16( "namcos10", 0, "crypt_mask", &crypt_mask, 1 );
	state_save_register_UINT32( "namcos10", 0, "crypt_counter", &crypt_counter, 1 );
	state_save_register_UINT32( "namcos10", 0, "crypt_clo", &crypt_h.clo, 1 );
	state_save_register_UINT32( "namcos10", 0, "crypt_chi", &crypt_h.chi, 1 );
	state_save_register_UINT32( "namcos10", 0, "crypt_plo", &crypt_h.plo, 1 );
	state_save_register_UINT32( "namcos10", 0, "crypt_phi", &crypt_h.phi, 1 );
	state_save_register_UINT32( "namcos10", 0, "crypt_nonlinear_count", &crypt_nonlinear_count, 1 );
	state_save_register_func_presave( ns10_presave );
	state_save_register_func_postload( ns10_postload );
}

/* MEM(M): the program is at the start of the flash, scrambled */
static void memm_driver_init( const struct ns10_crypt *c )
{
	const UINT8 *rom = memory_region( REGION_USER3 );
	data32_t *bios = (data32_t *)memory_region( REGION_USER2 );
	UINT16 *flash;
	UINT32 i;

	ns10_driver_init( NS10_MEMM );
	crypt_init( c );

	/* the flash starts with the contents of the ROM, as host order words */
	intelfsh_init( 0, INTELFSH_INTEL_28F640J5, NULL );
	flash = intelfsh_data( 0 );
	for( i = 0; i < intelfsh_size( 0 ) / 2 && i * 2 + 1 < memory_region_length( REGION_USER3 ); i++ )
		flash[ i ] = rom[ i * 2 ] | ( rom[ i * 2 + 1 ] << 8 );

	for( i = 0; i < 0x62000 / 4; i++ )
		bios[ i ] = unscramble( flash[ i * 2 ] ) | ( unscramble( flash[ i * 2 + 1 ] ) << 16 );

	state_save_register_UINT32( "memm", 0, "bank_idx", &memm_bank_idx, 1 );
	state_save_register_UINT32( "memm", 0, "direct", &memm_direct, 1 );
}

/* MEM(N): the boot loader and the program are in pages 0x40 to 0x11f and */
/* 0x120 to 0x201f of the first NAND, big endian words */
static void memn_nand_copy( const UINT8 *src, data32_t *dst, UINT32 start_page, UINT32 pages )
{
	UINT32 page, i;

	for( page = start_page; page < start_page + pages; page++ )
	{
		const UINT8 *p = src + page * NAND_PAGE_SIZE;
		for( i = 0; i < NAND_PAGE_DATA; i += 4 )
		{
			UINT16 lo = unscramble( ( p[ i ] << 8 ) | p[ i + 1 ] );
			UINT16 hi = unscramble( ( p[ i + 2 ] << 8 ) | p[ i + 3 ] );
			*( dst++ ) = lo | ( hi << 16 );
		}
	}
}

static void memn_driver_init( const struct ns10_crypt *c )
{
	UINT8 *nand_base = memory_region( REGION_USER3 );
	data32_t *bios = (data32_t *)memory_region( REGION_USER2 );
	int i;

	ns10_driver_init( NS10_MEMN );
	crypt_init( c );

	nand_count = memory_region_length( REGION_USER3 ) / NAND_SIZE;
	if( nand_count > NAND_MAX )
		nand_count = NAND_MAX;
	memset( nand, 0, sizeof( nand ) );
	for( i = 0; i < nand_count; i++ )
		nand[ i ].data = nand_base + i * NAND_SIZE;
	nand_warned = 0;

	memn_nand_copy( nand_base, bios, 0x40, 0xe0 );
	memn_nand_copy( nand_base, bios + 0x20000 / 4, 0x120, 0x1f00 );

	ns10_sram = auto_malloc( 0x100000 );
	memset( ns10_sram, 0, 0x100000 );
	cpu_setbank( 9, ns10_sram );
	cpu_setbank( 10, ns10_sram );
	cpu_setbank( 11, ns10_sram );

	psx_dma_install_read_handler( 5, memn_dma_read );

	state_save_register_UINT32( "memn", 0, "sram", ns10_sram, 0x100000 / 4 );
	state_save_register_UINT32( "memn", 0, "ctrl_reg", &memn_ctrl_reg, 1 );
	state_save_register_UINT32( "memn", 0, "device_idx", &memn_device_idx, 1 );
	state_save_register_UINT32( "memn", 0, "nand_address", &memn_nand_address, 1 );
	/* the settings block of the first NAND, the only one the board writes */
	state_save_register_UINT8( "memn", 0, "nand_nv", nand_base + NAND_NV_OFFSET, NAND_NV_SIZE );
	for( i = 0; i < nand_count; i++ )
	{
		state_save_register_UINT32( "nand", i, "mode", &nand[ i ].mode, 1 );
		state_save_register_UINT32( "nand", i, "pointer_mode", &nand[ i ].pointer_mode, 1 );
		state_save_register_UINT32( "nand", i, "page_addr", &nand[ i ].page_addr, 1 );
		state_save_register_UINT32( "nand", i, "byte_addr", &nand[ i ].byte_addr, 1 );
		state_save_register_UINT32( "nand", i, "addr_load_ptr", &nand[ i ].addr_load_ptr, 1 );
		state_save_register_UINT32( "nand", i, "program_byte_count", &nand[ i ].program_byte_count, 1 );
		state_save_register_UINT32( "nand", i, "status", &nand[ i ].status, 1 );
		state_save_register_UINT32( "nand", i, "accumulated_status", &nand[ i ].accumulated_status, 1 );
		state_save_register_UINT32( "nand", i, "mode_3065", &nand[ i ].mode_3065, 1 );
		state_save_register_UINT8( "nand", i, "pagereg", nand[ i ].pagereg, NAND_PAGE_SIZE );
	}
}

static DRIVER_INIT( mrdrilr2 ) { memm_driver_init( &mrdrilr2_crypt ); }
static DRIVER_INIT( chocovdr ) { memn_driver_init( &chocovdr_crypt ); }
static DRIVER_INIT( gamshara ) { memn_driver_init( &gamshara_crypt ); }
static DRIVER_INIT( gjspace ) { memn_driver_init( &gjspace_crypt ); }
static DRIVER_INIT( knpuzzle ) { memn_driver_init( &knpuzzle_crypt ); }
static DRIVER_INIT( konotako ) { memn_driver_init( &konotako_crypt ); }
static DRIVER_INIT( mrdrilrg ) { memn_driver_init( &mrdrilrg_crypt ); }
static DRIVER_INIT( panikuru ) { memn_driver_init( &panikuru_crypt ); }
static DRIVER_INIT( startrgn ) { memn_driver_init( &startrgn_crypt ); }

/* Point Blank 3, Gunbalina: the EXIO(G) with the guns */
static DRIVER_INIT( ptblank3 )
{
	memn_driver_init( &ptblank3_crypt );

	ns10_has_exio = 1;
	exio_ident_code = 0x32;	/* EXIO without CPU */
	exio_ram = auto_malloc( EXIO_RAM_SIZE );
	memset( exio_ram, 0, EXIO_RAM_SIZE );
	ns10_gun_min[ 0 ] = 0; ns10_gun_max[ 0 ] = 0x13f;
	ns10_gun_min[ 1 ] = 0; ns10_gun_max[ 1 ] = 0xef;

	install_mem_read32_handler( 0, 0x1fe00000, 0x1fefffff, exio_1f_r );
	install_mem_write32_handler( 0, 0x1fe00000, 0x1fefffff, exio_1f_w );
	install_mem_read32_handler( 0, 0x9fe00000, 0x9fefffff, exio_9f_r );
	install_mem_write32_handler( 0, 0x9fe00000, 0x9fefffff, exio_9f_w );
	install_mem_read32_handler( 0, 0xbfe00000, 0xbfefffff, exio_bf_r );
	install_mem_write32_handler( 0, 0xbfe00000, 0xbfefffff, exio_bf_w );

	state_save_register_UINT8( "exio", 0, "ram", exio_ram, EXIO_RAM_SIZE );
	state_save_register_UINT16( "exio", 0, "ctrl", &exio_ctrl, 1 );
	state_save_register_UINT16( "exio", 0, "bus_req", &exio_bus_req, 1 );
}

static MACHINE_INIT( namcos10 )
{
	int i;

	psx_machine_init();
	psx_machine_cpu_speedups();
	if( ns10_has_exio )
	{
		/* the EXIO window at 0x1fe00000 is not BIOS: the CPU core reads */
		/* the BIOS space below it directly, the rest through the handlers */
		static const offs_t bios_base[] = { 0x1fc00000, 0x9fc00000, 0xbfc00000 };
		for( i = 0; i < 3; i++ )
			psxcpu_add_fastram( bios_base[ i ], bios_base[ i ] + 0x1fffff, 1, memory_region( REGION_USER2 ) );
		exio_ctrl = 0;
		exio_bus_req = 0;
	}

	ns10_int = 0;
	exio_ident_bit = 0;
	exio_ident_byte = 0;
	i2c_dev_clock = i2c_dev_data = 1;
	i2c_host_clock = i2c_host_data = 1;
	i2c_prev_clock = i2c_prev_data = 1;
	i2cp_mode = I2CP_IDLE;
	i2c_byte = 0;
	i2c_bit = 0;
	crypt_reset();
	timer_adjust( ns10_io_timer, TIME_IN_HZ( 100 ), 0, TIME_IN_HZ( 100 ) );

	if( ns10_board == NS10_MEMM )
	{
		memm_bank_idx = 0;
		memm_direct = 0;
		intelfsh_reset( 0 );
	}
	else
	{
		memn_ctrl_reg = 0;
		memn_device_idx = 0;
		memn_nand_address = 0;
		for( i = 0; i < nand_count; i++ )
			nand_reset( &nand[ i ] );
	}
}

/* MEM(M): the flash (8MB, little endian words); MEM(N): the settings */
/* block of the first NAND. No file: the contents of the ROMs */
static NVRAM_HANDLER( namcos10 )
{
	if( ns10_board == NS10_MEMM )
	{
		if( read_or_write )
			intelfsh_nvram_save( 0, file );
		else if( file )
			intelfsh_nvram_load( 0, file );
	}
	else
	{
		UINT8 *nv = memory_region( REGION_USER3 ) + NAND_NV_OFFSET;
		if( read_or_write )
			mame_fwrite( file, nv, NAND_NV_SIZE );
		else if( file )
			mame_fread( file, nv, NAND_NV_SIZE );
	}
}

/* the PSX SPU (0.75 in MAME) */
static struct PSXSPUinterface ns10_spu_interface =
{
	75
};

static MACHINE_DRIVER_START( ns10_memn )
	MDRV_CPU_ADD_TAG( "main", PSXCPU, 101491200 / 4 )	/* CXD8606BQ, 101.4912MHz XTAL; one cycle per four clocks as in current MAME */
	MDRV_CPU_MEMORY( memn_readmem, memn_writemem )
	MDRV_CPU_VBLANK_INT( psx_vblank, 1 )

	MDRV_FRAMES_PER_SECOND( 60 )
	MDRV_VBLANK_DURATION( 0 )

	MDRV_MACHINE_INIT( namcos10 )
	MDRV_NVRAM_HANDLER( namcos10 )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES( VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT )
	MDRV_SCREEN_SIZE( 640, 480 )
	MDRV_VISIBLE_AREA( 0, 639, 0, 479 )
	MDRV_PALETTE_LENGTH( 65536 )

	MDRV_PALETTE_INIT( psx )
	MDRV_VIDEO_START( psx_type2_1024x1024 )
	MDRV_VIDEO_UPDATE( psx )
	MDRV_VIDEO_STOP( psx )

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES( SOUND_SUPPORTS_STEREO )
	MDRV_SOUND_ADD( PSXSPU, ns10_spu_interface )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ns10_memm )
	MDRV_IMPORT_FROM( ns10_memn )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_MEMORY( memm_readmem, memm_writemem )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ns10_gun )
	MDRV_IMPORT_FROM( ns10_memn )
	MDRV_VIDEO_UPDATE( namcos10_gun )
MACHINE_DRIVER_END

/***************************************************************************

  Inputs. 0x1fba0000: the DIP switches (SW1, active low). 0x1fba0004: the
  32-bit JAMMA inputs, in two ports here (bits 0-15, 16-31). The test
  switch is a slide switch, as most games expect

***************************************************************************/

#define NS10_SYSTEM_UNKNOWN( mask ) \
	PORT_DIPNAME( mask, mask, DEF_STR( Unknown ) ) \
	PORT_DIPSETTING(    mask, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

#define NS10_PLAYER( p, way ) \
	PORT_BIT( 0x01 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | way | IPF_PLAYER##p ) \
	PORT_BIT( 0x02 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | way | IPF_PLAYER##p ) \
	PORT_BIT( 0x04 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | way | IPF_PLAYER##p ) \
	PORT_BIT( 0x08 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_JOYSTICK_UP | way | IPF_PLAYER##p ) \
	PORT_BIT( 0x10 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER##p ) \
	PORT_BIT( 0x20 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER##p ) \
	PORT_BIT( 0x40 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER##p ) \
	PORT_BIT( 0x80 << ( ( p - 1 ) * 8 ), IP_ACTIVE_LOW, IPT_START##p )

/* bits 16-31: buttons 4-6, the coins and the service switches */
#define NS10_IN1_HIGH \
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 ) \
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 ) \
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 ) \
	PORT_BIT( 0x0f11, IP_ACTIVE_LOW, IPT_UNUSED ) \
	NS10_IN1_COINS

#define NS10_IN1_COINS \
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW ) \
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )

#define NS10_SYSTEM_DIPS_LOW \
	NS10_SYSTEM_UNKNOWN( 0x0001 ) \
	NS10_SYSTEM_UNKNOWN( 0x0002 ) \
	NS10_SYSTEM_UNKNOWN( 0x0004 ) \
	NS10_SYSTEM_UNKNOWN( 0x0008 ) \
	NS10_SYSTEM_UNKNOWN( 0x0010 ) \
	NS10_SYSTEM_UNKNOWN( 0x0020 ) \
	NS10_SYSTEM_UNKNOWN( 0x0040 )

#define NS10_SERVICE_DIP( mask ) \
	PORT_DIPNAME( mask, mask, DEF_STR( Service_Mode ) ) \
	PORT_DIPSETTING(    mask, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_START( namcos10 )
	PORT_START	/* SYSTEM */
	NS10_SYSTEM_DIPS_LOW
	NS10_SYSTEM_UNKNOWN( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15 */
	NS10_PLAYER( 1, IPF_8WAY )
	NS10_PLAYER( 2, IPF_8WAY )

	PORT_START	/* IN1, bits 16-31 */
	NS10_IN1_HIGH
INPUT_PORTS_END

INPUT_PORTS_START( mrdrilr2 )
	PORT_START	/* SYSTEM */
	NS10_SYSTEM_DIPS_LOW
	NS10_SYSTEM_UNKNOWN( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15 */
	NS10_PLAYER( 1, IPF_4WAY )
	NS10_PLAYER( 2, IPF_4WAY )

	PORT_START	/* IN1, bits 16-31 */
	NS10_IN1_HIGH
INPUT_PORTS_END

INPUT_PORTS_START( gamshara )
	PORT_START	/* SYSTEM */
	/* only the exact value 0x6e gives the World region, the rest Japan */
	PORT_DIPNAME( 0x007f, 0x007f, "Region" )
	PORT_DIPSETTING(      0x007f, "Japan" )
	PORT_DIPSETTING(      0x006e, "World" )
	NS10_SERVICE_DIP( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15 */
	NS10_PLAYER( 1, IPF_8WAY )
	NS10_PLAYER( 2, IPF_8WAY )

	PORT_START	/* IN1, bits 16-31 */
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	NS10_IN1_COINS
INPUT_PORTS_END

INPUT_PORTS_START( gjspace )
	PORT_START	/* SYSTEM */
	NS10_SYSTEM_DIPS_LOW
	NS10_SERVICE_DIP( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15 */
	NS10_PLAYER( 1, IPF_8WAY )
	NS10_PLAYER( 2, IPF_8WAY )

	PORT_START	/* IN1, bits 16-31 */
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	NS10_IN1_COINS
INPUT_PORTS_END

/* one button and start, no player 2 */
INPUT_PORTS_START( startrgn )
	PORT_START	/* SYSTEM */
	NS10_SYSTEM_DIPS_LOW
	NS10_SYSTEM_UNKNOWN( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15 */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xff6f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 16-31 */
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	NS10_IN1_COINS
INPUT_PORTS_END

INPUT_PORTS_START( konotako )
	PORT_START	/* SYSTEM */
	PORT_DIPNAME( 0x0001, 0x0001, "Region" )
	PORT_DIPSETTING(      0x0000, "Export (Cristaltec license)" )
	PORT_DIPSETTING(      0x0001, "Japan" )
	NS10_SYSTEM_UNKNOWN( 0x0002 )
	NS10_SYSTEM_UNKNOWN( 0x0004 )
	NS10_SYSTEM_UNKNOWN( 0x0008 )
	NS10_SYSTEM_UNKNOWN( 0x0010 )
	NS10_SYSTEM_UNKNOWN( 0x0020 )
	NS10_SYSTEM_UNKNOWN( 0x0040 )
	NS10_SYSTEM_UNKNOWN( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15: no button 3 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x4040, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 16-31: one coin */
	PORT_BIT( 0x1fff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )
INPUT_PORTS_END

/* the triggers on button 1, the guns as current MAME (X 0-0x13f, Y 0-0xef) */
INPUT_PORTS_START( ptblank3 )
	PORT_START	/* SYSTEM */
	NS10_SYSTEM_DIPS_LOW
	NS10_SERVICE_DIP( 0x0080 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 0-15 */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x6f6f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1, bits 16-31 */
	PORT_BIT( 0x0fff, IP_ACTIVE_LOW, IPT_UNUSED )
	NS10_IN1_COINS

	PORT_START	/* GUN1X, GUN1Y, GUN2X, GUN2Y */
	PORT_ANALOG( 0xffff, 0x00a0, IPT_LIGHTGUN_X | IPF_PLAYER1, 100, 5, 0x000, 0x13f )
	PORT_START
	PORT_ANALOG( 0xffff, 0x0078, IPT_LIGHTGUN_Y | IPF_PLAYER1, 50, 5, 0x000, 0x0ef )
	PORT_START
	PORT_ANALOG( 0xffff, 0x00a0, IPT_LIGHTGUN_X | IPF_PLAYER2, 100, 5, 0x000, 0x13f )
	PORT_START
	PORT_ANALOG( 0xffff, 0x0078, IPT_LIGHTGUN_Y | IPF_PLAYER2, 50, 5, 0x000, 0x0ef )
INPUT_PORTS_END

/***************************************************************************

  ROMs. REGION_USER2: the BIOS space, filled at init. REGION_USER3: the
  flash (MEM(M)) or the NANDs one after the other (MEM(N)), REGION_USER4:
  the mask ROMs (MEM(M))

***************************************************************************/

#define NS10_BASE_REGIONS \
	ROM_REGION32_LE( 0x400000, REGION_CPU1, 0 ) /* main ram */ \
	ROM_REGION32_LE( 0x000400, REGION_USER1, 0 ) /* scratchpad */ \
	ROM_REGION32_LE( 0x400000, REGION_USER2, 0 ) /* bios space */

ROM_START( mrdrilr2 )
	NS10_BASE_REGIONS
	ROM_REGION( 0x800000, REGION_USER3, 0 ) /* flash */
	ROM_LOAD( "dr22vera.1a", 0x000000, 0x800000, CRC(140eafb6) SHA1(b68f901ff18d052bc21bb548159dcbc7dd731e5c) )

	ROM_REGION( 0x2000000, REGION_USER4, 0 ) /* mask roms */
	ROM_LOAD( "dr21ma1.1d", 0x0000000, 0x1000000, CRC(26dc6f55) SHA1(a9cedf547fa7a4d5850b9b3b867d46e577a035e0) )
	ROM_LOAD( "dr21ma2.2d", 0x1000000, 0x1000000, CRC(702556ff) SHA1(c95defd5fd6a9b406fc8d8f28ecfab732ef1ff42) )
ROM_END

ROM_START( mrdrilr2j )
	NS10_BASE_REGIONS
	ROM_REGION( 0x800000, REGION_USER3, 0 ) /* flash */
	ROM_LOAD( "dr21vera.1a", 0x000000, 0x800000, CRC(03e0241e) SHA1(de26054657cc129dd581e2672d5c81d26daeb5e6) )

	ROM_REGION( 0x2000000, REGION_USER4, 0 ) /* mask roms */
	ROM_LOAD( "dr21ma1.1d", 0x0000000, 0x1000000, CRC(26dc6f55) SHA1(a9cedf547fa7a4d5850b9b3b867d46e577a035e0) )
	ROM_LOAD( "dr21ma2.2d", 0x1000000, 0x1000000, CRC(702556ff) SHA1(c95defd5fd6a9b406fc8d8f28ecfab732ef1ff42) )
ROM_END

ROM_START( mrdrilr2u )
	NS10_BASE_REGIONS
	ROM_REGION( 0x800000, REGION_USER3, 0 ) /* flash */
	ROM_LOAD( "dr23vera.1a", 0x000000, 0x800000, CRC(5f32b6fb) SHA1(ecb94d8317946f278a2cb0bc55abea2b89fa2e71) )

	ROM_REGION( 0x2000000, REGION_USER4, 0 ) /* mask roms */
	ROM_LOAD( "dr21ma1.1d", 0x0000000, 0x1000000, CRC(26dc6f55) SHA1(a9cedf547fa7a4d5850b9b3b867d46e577a035e0) )
	ROM_LOAD( "dr21ma2.2d", 0x1000000, 0x1000000, CRC(702556ff) SHA1(c95defd5fd6a9b406fc8d8f28ecfab732ef1ff42) )
ROM_END

ROM_START( gjspace )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 4, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "10011.8e", 0x0000000, 0x1080000, CRC(c581bca7) SHA1(3042fa8383b9f18509768feb08b400ac194831b0) )
	ROM_LOAD( "10011.8d", 0x1080000, 0x1080000, CRC(7fb4a3fe) SHA1(677ba88af00d448c72a89af145acc9a9d85c3d76) )
	ROM_LOAD( "10011.7e", 0x2100000, 0x1080000, CRC(31bfc843) SHA1(398017d1a4e26d63feef395fdfac56d6c9f0ed99) )
	ROM_LOAD( "10011.7d", 0x3180000, 0x1080000, CRC(15d83649) SHA1(8ecbf6e294182459fbe2cc098e2406767851c911) )
ROM_END

ROM_START( knpuzzle )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 3, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "kpm1vera_0.8e", 0x0000000, 0x1080000, CRC(4b4255da) SHA1(c8ec575e53596a167a07db97076fd69e6646d0f5) )
	ROM_LOAD( "kpm1vera_1.8d", 0x1080000, 0x1080000, CRC(644595a6) SHA1(4b60008ca5cac894a815fe6aaa980296a83f673f) )
	ROM_LOAD( "kpm1vera_2.7e", 0x2100000, 0x1080000, CRC(6bf164e5) SHA1(b4a2e6eb18c09220b0c8ec80159d13d0e439a559) )
ROM_END

ROM_START( mrdrilrg )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 3, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "0.8e", 0x0000000, 0x1080000, CRC(4b61ac62) SHA1(b91076ce3d99fbe1f9247c66a577af3c3d468b20) )
	ROM_LOAD( "1.8d", 0x1080000, 0x1080000, CRC(c87b5e86) SHA1(b034210da30e1f2f7d04f77e00bf7724437e2024) )
	ROM_LOAD( "2.7e", 0x2100000, 0x1080000, CRC(e0a9339f) SHA1(4284e7233876cfaf8021440d78ccc8c70d00cc00) )
ROM_END

ROM_START( chocovdr )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 5, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "0.8e", 0x0000000, 0x1080000, CRC(330d86d3) SHA1(0d7ea7593510531ef7350dd7ee957208681708da) )
	ROM_LOAD( "1.8d", 0x1080000, 0x1080000, CRC(4aecd6fc) SHA1(31fe8f36e38020a92f15c44fd1a4b486636b40ce) )
	ROM_LOAD( "2.7e", 0x2100000, 0x1080000, CRC(ac212e5a) SHA1(f2d2e65a3249992730b8b90561b9bcf5eaaafb88) )
	ROM_LOAD( "3.7d", 0x3180000, 0x1080000, CRC(907d3d15) SHA1(20519d1f8bd9c6bc45b65e2d7444d588e922611d) )
	ROM_LOAD( "4.6e", 0x4200000, 0x1080000, CRC(1ed957dd) SHA1(bc8ce9f249fe496c130c6fe67b2260c4d0734ab9) )
ROM_END

ROM_START( gamshara )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 2, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "10021a_e.8e", 0x0000000, 0x1080000, CRC(1413b7d2) SHA1(595c77dfcba8074fff84b859870818aace530ed8) )
	ROM_LOAD( "10021a.8d",   0x1080000, 0x1080000, CRC(73669ff7) SHA1(eb8bbf931f1f8a049208d081d040512a3ffa9c00) )
ROM_END

ROM_START( gamsharao )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 2, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "10021a.8e", 0x0000000, 0x1080000, CRC(6c0361fc) SHA1(7debf1f2e6bed31d59fb224a78a17a94fc573785) )
	ROM_LOAD( "10021a.8d", 0x1080000, 0x1080000, CRC(73669ff7) SHA1(eb8bbf931f1f8a049208d081d040512a3ffa9c00) )
ROM_END

ROM_START( panikuru )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 3, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "0.8e", 0x0000000, 0x1080000, CRC(a68b0965) SHA1(037e41e80a62675ffe1c820060c8bb414fb84c30) )
	ROM_LOAD( "1.8d", 0x1080000, 0x1080000, CRC(18e5135d) SHA1(a7b1533a1df71be5498718e301d1c9c548551fb4) )
	ROM_LOAD( "2.7e", 0x2100000, 0x1080000, CRC(cd3b25e0) SHA1(39dfebc59e71b8f1c28e718ee71032620f11440c) )
ROM_END

ROM_START( startrgn )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 2, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "stt1a_0.bin", 0x0000000, 0x1080000, CRC(1e090644) SHA1(a7a293e2bd9eea2eb64a492a47272d9d9ee2c724) )
	ROM_LOAD( "stt1a_1.bin", 0x1080000, 0x1080000, CRC(aa527694) SHA1(a25dcbeca58a1443070848b3487a24d51d41a34b) )
ROM_END

ROM_START( konotako )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 2, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "0.8e", 0x0000000, 0x1080000, CRC(63d23a0c) SHA1(31b54119f20827ff13ecf0cd87803a5e27eaafe7) )
	ROM_LOAD( "1.8d", 0x1080000, 0x1080000, CRC(bdbed53c) SHA1(5773069c43642e6f334cee185a6fb6908eedcf4a) )
ROM_END

ROM_START( ptblank3 )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 2, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "gnn2vera_0.8e", 0x0000000, 0x1080000, CRC(5ae03e23) SHA1(2ad9e04080a99427e3c59e0b1567b8d98109938f) )
	ROM_LOAD( "gnn2vera_1.8d", 0x1080000, 0x1080000, CRC(82d2cfb5) SHA1(4b5e713a55e74a7b32b1b9b5811892df2df86256) )

	ROM_REGION( 0x2ec00, REGION_USER5, 0 ) /* the nonlinear bits of the decrypter */
	ROM_LOAD( "ptblank3_nonlinear", 0x00000, 0x2ec00, CRC(5997d7dd) SHA1(ab3f32fd92ee20ca3c7642686d9d8039d0b6cbc5) )
ROM_END

ROM_START( gunbalina )
	NS10_BASE_REGIONS
	ROM_REGION( 0x1080000 * 2, REGION_USER3, 0 ) /* nands */
	ROM_LOAD( "gnn1a.8e", 0x0000000, 0x1080000, CRC(2371aaba) SHA1(fc43a2172a6325da9c0a9f4d9cf65ab8d3883f22) )
	ROM_LOAD( "gnn1a.8d", 0x1080000, 0x1080000, CRC(6cd343e0) SHA1(dcec44abae1504025895f42fe574549e5010f7d5) )

	ROM_REGION( 0x2ec00, REGION_USER5, 0 ) /* the nonlinear bits of the decrypter */
	ROM_LOAD( "ptblank3_nonlinear", 0x00000, 0x2ec00, CRC(5997d7dd) SHA1(ab3f32fd92ee20ca3c7642686d9d8039d0b6cbc5) )
ROM_END

/* MEM(M) */
GAMEX( 2000, mrdrilr2,  0,        ns10_memm, mrdrilr2, mrdrilr2, ROT0, "Namco", "Mr. Driller 2 (World, DR22 Ver.A)", 0 )
GAMEX( 2000, mrdrilr2j, mrdrilr2, ns10_memm, mrdrilr2, mrdrilr2, ROT0, "Namco", "Mr. Driller 2 (Japan, DR21 Ver.A)", 0 )
GAMEX( 2000, mrdrilr2u, mrdrilr2, ns10_memm, mrdrilr2, mrdrilr2, ROT0, "Namco", "Mr. Driller 2 (US, DR23 Ver.A)", 0 )

/* MEM(N) */
GAMEX( 2001, gjspace,   0,        ns10_memn, gjspace,  gjspace,  ROT0, "Namco / Metro", "GekiToride-Jong Space (10011 Ver.A)", 0 )
GAMEX( 2001, knpuzzle,  0,        ns10_memn, namcos10, knpuzzle, ROT0, "Namco", "Kotoba no Puzzle Mojipittan (Japan, KPM1 Ver.A)", 0 )
GAMEX( 2001, mrdrilrg,  0,        ns10_memn, mrdrilr2, mrdrilrg, ROT0, "Namco", "Mr. Driller G (Japan, DRG1 Ver.A)", 0 )
GAMEX( 2002, chocovdr,  0,        ns10_memn, namcos10, chocovdr, ROT0, "Namco", "Uchuu Daisakusen: Chocovader Contactee (Japan, CVC1 Ver.A)", 0 )
GAMEX( 2002, gamshara,  0,        ns10_memn, gamshara, gamshara, ROT0, "Mitchell", "Gamshara (20020912A / 10021 Ver.A)", 0 )
GAMEX( 2002, gamsharao, gamshara, ns10_memn, gamshara, gamshara, ROT0, "Mitchell", "Gamshara (20020716A / 10021 Ver.A)", 0 )
GAMEX( 2002, panikuru,  0,        ns10_memn, namcos10, panikuru, ROT0, "Namco", "Panikuru Panekuru (Japan, PPA1 Ver.A)", 0 )
GAMEX( 2002, startrgn,  0,        ns10_memn, startrgn, startrgn, ROT0, "Namco", "Star Trigon (Japan, STT1 Ver.A)", 0 )
GAMEX( 2003, konotako,  0,        ns10_memn, konotako, konotako, ROT0, "Mitchell", "Kono e Tako (10021 Ver.A)", 0 )

/* MEM(N) and EXIO(G) */
GAMEX( 2000, ptblank3,  0,        ns10_gun,  ptblank3, ptblank3, ROT0, "Namco", "Point Blank 3 (World, GNN2 Ver.A)", 0 )
GAMEX( 2000, gunbalina, ptblank3, ns10_gun,  ptblank3, ptblank3, ROT0, "Namco", "Gunbalina (Japan, GNN1 Ver.A)", 0 )
