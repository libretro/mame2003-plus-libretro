/***************************************************************************

    Sega 315-5838 / 317-0229 compression and encryption
    (DecAthlete graphics on ST-V, Dead or Alive startup string on Model 2)

    Translated to C from current MAME (sega/315-5838_317-0229_comp.cpp,
    license BSD-3-Clause): a fixed word substitution followed by a
    programmable prefix decompression. The four variants differ only in the
    substitution (routing, tables and affine masks).

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "machine/sega315_5838.h"

#define BITN(x,n) (((x)>>(n))&1)

static UINT16 (*comp_source_r)(UINT32 offset);
static int comp_variant = SEGA315_5838_VARIANT_0229;
static UINT16 m_tree[ 24 ];
static UINT8 m_dictionary[ 256 ];
static int m_tree_words, m_dictionary_bytes, m_upload_dictionary;
static UINT32 m_source;
static UINT16 m_word, m_output;
static int m_bits, m_abort;

static UINT16 decipher( UINT16 c )
{
	/* routing, substitution tables and affine masks of 315-5838, 317-0229, 317-0230, 317-0231 */
	static const UINT8 all_routing[ 4 ][ 4 ] = { { 0, 1, 2, 3 }, { 3, 2, 0, 1 }, { 2, 3, 1, 0 }, { 1, 0, 3, 2 } };
	static const UINT8 all_sboxes[ 4 ][ 4 ][ 8 ] =
	{
		{ { 6, 7, 2, 0, 5, 1, 3, 4 }, { 5, 3, 7, 2, 1, 6, 4, 0 },
		  { 2, 0, 6, 1, 3, 7, 4, 5 }, { 6, 5, 7, 4, 1, 3, 0, 2 } },
		{ { 6, 0, 5, 4, 7, 2, 1, 3 }, { 5, 6, 1, 3, 7, 0, 4, 2 },
		  { 2, 6, 0, 1, 3, 4, 7, 5 }, { 6, 5, 7, 4, 1, 3, 0, 2 } },
		{ { 6, 2, 7, 0, 5, 3, 1, 4 }, { 5, 3, 7, 2, 1, 6, 4, 0 },
		  { 2, 1, 3, 5, 6, 0, 4, 7 }, { 6, 1, 3, 5, 7, 0, 2, 4 } },
		{ { 6, 0, 5, 4, 7, 2, 1, 3 }, { 5, 1, 6, 3, 7, 4, 0, 2 },
		  { 2, 1, 3, 5, 6, 0, 4, 7 }, { 6, 3, 1, 5, 7, 2, 0, 4 } }
	};
	static const UINT8 all_affine[ 4 ][ 4 ] =
	{
		{ 0x14, 0x18, 0x02, 0x15 }, { 0x12, 0x11, 0x04, 0x1a },
		{ 0x14, 0x18, 0x02, 0x15 }, { 0x12, 0x11, 0x04, 0x1a }
	};
	const UINT8 *routing = all_routing[ comp_variant ];
	const UINT8 (*sboxes)[ 8 ] = all_sboxes[ comp_variant ];
	const UINT8 *affine_masks = all_affine[ comp_variant ];
	static const UINT8 output_bits[ 4 ][ 3 ] = { { 0, 1, 7 }, { 2, 5, 9 }, { 4, 10, 13 }, { 6, 11, 15 } };
	static const UINT8 affine_bits[ 4 ] = { 3, 8, 12, 14 };
	UINT8 lanes[ 4 ];
	UINT8 affine;
	UINT16 result = 0;
	int group, bit;

	lanes[ 0 ] = ( BITN( c, 5 ) ^ BITN( c, 12 ) ) | ( ( BITN( c, 7 ) ^ BITN( c, 9 ) ) << 1 ) | ( BITN( c, 14 ) << 2 );
	lanes[ 1 ] = BITN( c, 6 ) | ( BITN( c, 8 ) << 1 ) | ( BITN( c, 15 ) << 2 );
	lanes[ 2 ] = ( BITN( c, 1 ) ^ BITN( c, 8 ) ) | ( BITN( c, 7 ) << 1 ) | ( BITN( c, 12 ) << 2 );
	lanes[ 3 ] = BITN( c, 3 ) | ( BITN( c, 4 ) << 1 ) | ( BITN( c, 10 ) << 2 );
	affine = BITN( c, 0 ) | ( ( BITN( c, 0 ) ^ BITN( c, 2 ) ) << 1 ) | ( ( BITN( c, 11 ) ^ BITN( c, 14 ) ) << 2 ) |
		( ( BITN( c, 4 ) ^ BITN( c, 13 ) ) << 3 ) | 16;

	for( group = 0; group < 4; group++ )
	{
		UINT8 value = sboxes[ group ][ lanes[ routing[ group ] ] ];
		UINT8 parity;
		for( bit = 0; bit < 3; bit++ )
			result |= BITN( value, bit ) << output_bits[ group ][ bit ];
		parity = affine & affine_masks[ group ];
		parity ^= parity >> 4;
		parity ^= parity >> 2;
		parity ^= parity >> 1;
		result |= ( parity & 1 ) << affine_bits[ group ];
	}
	return result;
}

void sega315_5838_set_variant( int variant )
{
	comp_variant = variant & 3;
}

void sega315_5838_init( UINT16 (*source_r)( UINT32 offset ) )
{
	comp_source_r = source_r;
	comp_variant = SEGA315_5838_VARIANT_0229;
	state_save_register_UINT16( "315_5838", 0, "tree", m_tree, 24 );
	state_save_register_UINT8( "315_5838", 0, "dictionary", m_dictionary, 256 );
	state_save_register_int( "315_5838", 0, "tree_words", &m_tree_words );
	state_save_register_int( "315_5838", 0, "dictionary_bytes", &m_dictionary_bytes );
	state_save_register_int( "315_5838", 0, "upload_dictionary", &m_upload_dictionary );
	state_save_register_UINT32( "315_5838", 0, "source", &m_source, 1 );
	state_save_register_UINT16( "315_5838", 0, "word", &m_word, 1 );
	state_save_register_UINT16( "315_5838", 0, "output", &m_output, 1 );
	state_save_register_int( "315_5838", 0, "bits", &m_bits );
	state_save_register_int( "315_5838", 0, "abort", &m_abort );
}

void sega315_5838_reset( void )
{
	memset( m_tree, 0, sizeof( m_tree ) );
	memset( m_dictionary, 0, sizeof( m_dictionary ) );
	m_tree_words = m_dictionary_bytes = 0;
	m_upload_dictionary = 0;
	m_source = 0;
	m_word = 0;
	m_bits = 0;
	m_output = 0xffff;
	m_abort = 0;
}

void sega315_5838_source_w( UINT32 data )
{
	m_source = data & 0x007fffff;
	m_word = 0;
	m_bits = 0;
	m_abort = 0;
}

void sega315_5838_table_w( int offset, UINT16 data )
{
	if( !( offset & 1 ) )
	{
		m_upload_dictionary = BITN( data, 7 );
		if( m_upload_dictionary )
			m_dictionary_bytes = 0;
		else
			m_tree_words = 0;
	}
	else if( m_upload_dictionary )
	{
		if( m_dictionary_bytes < 256 )
		{
			m_dictionary[ m_dictionary_bytes++ ] = data >> 8;
			m_dictionary[ m_dictionary_bytes++ ] = data & 0xff;
		}
	}
	else if( m_tree_words < 24 )
		m_tree[ m_tree_words++ ] = data;
}

static UINT8 decompress_byte( void )
{
	UINT16 code = 0;
	int length;
	unsigned entry, next;

	if( m_abort || m_tree_words != 24 || m_dictionary_bytes != 256 )
		return 0xff;

	for( length = 1; length <= 12; length++ )
	{
		if( !m_bits )
		{
			m_word = decipher( comp_source_r( m_source ) );
			m_source = ( m_source + 1 ) & 0x007fffff;
			m_bits = 16;
		}
		code = ( code << 1 ) | BITN( m_word, --m_bits );
		for( entry = 0; entry < 24; entry += 2 )
		{
			UINT16 first, limit;
			if( ( ( m_tree[ entry ] >> 8 ) & 0xff ) != length )
				continue;
			first = m_tree[ entry + 1 ] >> ( 12 - length );
			limit = 1 << length;
			for( next = entry + 2; next < 24; next += 2 )
			{
				if( m_tree[ next + 1 ] != m_tree[ entry + 1 ] )
				{
					limit = m_tree[ next + 1 ] >> ( 12 - length );
					break;
				}
			}
			if( code >= first && code < limit )
			{
				unsigned index = ( m_tree[ entry ] & 0xff ) + code - first;
				if( index < 256 )
					return m_dictionary[ index ];
			}
		}
	}
	/* invalid tables or data */
	m_abort = 1;
	return 0xff;
}

UINT16 sega315_5838_data_r( void )
{
	m_output = decompress_byte() << 8;
	m_output |= decompress_byte();
	return m_output;
}
