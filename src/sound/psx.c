/***************************************************************************

	PSX SPU

	CXD2922BQ/CXD2925Q

	preliminary version by smf; backported from MAME 0.135 (2009): voices
	are decoded and played (no envelopes, no reverb), the SPU interrupt is
	raised when a voice reaches the IRQ address.

***************************************************************************/

#include "driver.h"
#include "includes/psx.h"
#include "state.h"

#define MAX_CHANNEL ( 24 )
#define SPU_RAM_SIZE ( 512 * 1024 )
#define SAMPLES_PER_BLOCK ( 28 )
#define PITCH_SHIFT ( 12 )
#define SPU_RATE ( 44100 )

enum { e_attack = 0, e_decay, e_sustain, e_sustainEnd, e_release, e_releaseend };

static UINT32 *m_p_n_psxram;
static int m_n_stream;

static data16_t m_n_mainvolumeleft;
static data16_t m_n_mainvolumeright;
static data16_t m_n_reverberationdepthleft;
static data16_t m_n_reverberationdepthright;
static data32_t m_n_voiceon;
static data32_t m_n_voiceoff;
static data32_t m_n_modulationmode;
static data32_t m_n_noisemode;
static data32_t m_n_reverbmode;
static data32_t m_n_channelonoff;
static data16_t m_n_reverbworkareastart;
static data16_t m_n_irqaddress;
static data32_t m_n_spuoffset;
static data16_t m_n_spudata;
static data16_t m_n_spucontrol;
static data32_t m_n_spustatus;
static data16_t m_n_cdvolumeleft;
static data16_t m_n_cdvolumeright;
static data16_t m_n_externalvolumeleft;
static data16_t m_n_externalvolumeright;
static data16_t m_p_n_volumeleft[ MAX_CHANNEL ];
static data16_t m_p_n_volumeright[ MAX_CHANNEL ];
static data16_t m_p_n_pitch[ MAX_CHANNEL ];
static data16_t m_p_n_address[ MAX_CHANNEL ];
static data16_t m_p_n_envelopestate[ MAX_CHANNEL ];
static data16_t m_p_n_attackdecaysustain[ MAX_CHANNEL ];
static data16_t m_p_n_sustainrelease[ MAX_CHANNEL ];
static data16_t m_p_n_adsrvolume[ MAX_CHANNEL ];
static data16_t m_p_n_repeataddress[ MAX_CHANNEL ];
static data32_t m_p_n_effect[ 16 ];
static data16_t *m_p_n_spuram;
static data32_t m_p_n_blockaddress[ MAX_CHANNEL ];
static data32_t m_p_n_blockoffset[ MAX_CHANNEL ];
static data32_t m_p_n_blockstatus[ MAX_CHANNEL ];
static INT16 m_p_n_blockbuffer[ MAX_CHANNEL * SAMPLES_PER_BLOCK ];
static INT16 m_p_n_s1[ MAX_CHANNEL ];
static INT16 m_p_n_s2[ MAX_CHANNEL ];
static data32_t m_n_loop[ MAX_CHANNEL ];
static data32_t m_n_spu_delay;

#define SPU_REG( a ) ( ( a - 0xc00 ) / 4 )
#define SPU_CHANNEL_REG( a ) ( a / 4 )

static INLINE int spu_volume( data16_t n_volume )
{
	int v;
	if( ( n_volume & 0x8000 ) != 0 )
		v = ( n_volume & 0x7f ) * 0x80;
	else if( ( n_volume & 0x4000 ) != 0 )
		v = -( n_volume & 0x3fff );
	else
		v = n_volume;
	return v;
}

static INLINE int spu_limit( int v )
{
	if( v < -32768 )
		return -32768;
	if( v > 32767 )
		return 32767;
	return v;
}

static void spu_update( int param, INT16 **outputs, int samples )
{
	static const int f[ 5 ][ 2 ] =
	{
		{   0,   0 },
		{  60,   0 },
		{ 115, -52 },
		{  98, -55 },
		{ 122, -60 }
	};
	int n_channel;

	memset( outputs[ 0 ], 0, samples * sizeof( *outputs[ 0 ] ) );
	memset( outputs[ 1 ], 0, samples * sizeof( *outputs[ 1 ] ) );

	if( m_p_n_spuram == NULL )
		return;

	for( n_channel = 0; n_channel < MAX_CHANNEL; n_channel++ )
	{
		int voll, volr, n_sample;

		/* the envelopes are not emulated: a voice in release is silenced, so
		   that looped sounds don't play forever */
		if( m_p_n_envelopestate[ n_channel ] == e_release )
		{
			voll = 0;
			volr = 0;
		}
		else
		{
			voll = spu_volume( m_p_n_volumeleft[ n_channel ] );
			volr = spu_volume( m_p_n_volumeright[ n_channel ] );
		}

		for( n_sample = 0; n_sample < samples; n_sample++ )
		{
			int v;
			if( m_p_n_blockoffset[ n_channel ] >= ( SAMPLES_PER_BLOCK << PITCH_SHIFT ) )
			{
				int n_shift, n_predict, n_flags, n_word;
				if( !m_p_n_blockstatus[ n_channel ] )
					break;
				if( ( m_n_spucontrol & 0x40 ) != 0 &&
					( m_n_irqaddress * 4 ) >= m_p_n_blockaddress[ n_channel ] &&
					( m_n_irqaddress * 4 ) <= m_p_n_blockaddress[ n_channel ] + 7 )
				{
					psx_irq_set( 0x0200 );
				}

				n_shift =   ( m_p_n_spuram[ m_p_n_blockaddress[ n_channel ] ] >> 0 ) & 0x0f;
				n_predict = ( m_p_n_spuram[ m_p_n_blockaddress[ n_channel ] ] >> 4 ) & 0x0f;
				n_flags =   ( m_p_n_spuram[ m_p_n_blockaddress[ n_channel ] ] >> 8 ) & 0xff;
				if( n_predict > 4 )
					n_predict = 0;
				if( ( n_flags & 4 ) != 0 )
					m_n_loop[ n_channel ] = m_p_n_blockaddress[ n_channel ];
				m_p_n_blockaddress[ n_channel ] = ( m_p_n_blockaddress[ n_channel ] + 1 ) % ( SPU_RAM_SIZE / 2 );

				for( n_word = 0; n_word < 7; n_word++ )
				{
					int n_nibble;
					int n_packed = m_p_n_spuram[ m_p_n_blockaddress[ n_channel ] ];
					m_p_n_blockaddress[ n_channel ] = ( m_p_n_blockaddress[ n_channel ] + 1 ) % ( SPU_RAM_SIZE / 2 );

					for( n_nibble = 0; n_nibble < 4; n_nibble++ )
					{
						int n_unpacked = ( ( n_packed & 0xf ) << 12 );
						if( ( n_unpacked & 0x8000 ) != 0 )
							n_unpacked |= ~0xffff;
						n_unpacked = ( n_unpacked >> n_shift ) +
							( ( m_p_n_s1[ n_channel ] * f[ n_predict ][ 0 ] ) >> 6 ) +
							( ( m_p_n_s2[ n_channel ] * f[ n_predict ][ 1 ] ) >> 6 );
						m_p_n_s2[ n_channel ] = m_p_n_s1[ n_channel ];
						m_p_n_s1[ n_channel ] = n_unpacked;

						m_p_n_blockbuffer[ ( n_channel * SAMPLES_PER_BLOCK ) + ( n_word * 4 ) + n_nibble ] = n_unpacked;
						n_packed >>= 4;
					}
				}
				if( ( n_flags & 1 ) != 0 )
				{
					if( n_flags != 3 )
						m_p_n_blockstatus[ n_channel ] = 0;
					else
						m_p_n_blockaddress[ n_channel ] = m_n_loop[ n_channel ];
				}
				m_p_n_blockoffset[ n_channel ] %= ( SAMPLES_PER_BLOCK << PITCH_SHIFT );
			}
			v = m_p_n_blockbuffer[ ( n_channel * SAMPLES_PER_BLOCK ) + ( m_p_n_blockoffset[ n_channel ] >> PITCH_SHIFT ) ];
			m_p_n_blockoffset[ n_channel ] += m_p_n_pitch[ n_channel ];
			outputs[ 0 ][ n_sample ] = spu_limit( outputs[ 0 ][ n_sample ] + ( ( v * voll ) / 0x4000 ) );
			outputs[ 1 ][ n_sample ] = spu_limit( outputs[ 1 ][ n_sample ] + ( ( v * volr ) / 0x4000 ) );
		}
	}
}

/* DMA channel 4 */
static void spu_read( UINT32 n_address, INT32 n_size )
{
	while( n_size > 0 )
	{
		m_p_n_psxram[ n_address / 4 ] =
			( m_p_n_spuram[ m_n_spuoffset + 0 ] << 0 ) |
			( m_p_n_spuram[ m_n_spuoffset + 1 ] << 16 );
		m_n_spuoffset = ( m_n_spuoffset + 2 ) % ( SPU_RAM_SIZE / 2 );
		n_address += 4;
		n_size--;
	}
}

static void spu_write( UINT32 n_address, INT32 n_size )
{
	while( n_size > 0 )
	{
		m_p_n_spuram[ m_n_spuoffset + 0 ] = ( m_p_n_psxram[ n_address / 4 ] >> 0 );
		m_p_n_spuram[ m_n_spuoffset + 1 ] = ( m_p_n_psxram[ n_address / 4 ] >> 16 );
		m_n_spuoffset = ( m_n_spuoffset + 2 ) % ( SPU_RAM_SIZE / 2 );
		n_address += 4;
		n_size--;
	}
}

int PSX_sh_start( const struct MachineSound *msound )
{
	const struct PSXSPUinterface *intf = msound->sound_interface;
	char buf[ 2 ][ 40 ];
	const char *name[ 2 ];
	int vol[ 2 ];
	int n_channel;

	m_p_n_psxram = (UINT32 *)memory_region( REGION_CPU1 );

	m_n_mainvolumeleft = 0;
	m_n_mainvolumeright = 0;
	m_n_reverberationdepthleft = 0;
	m_n_reverberationdepthright = 0;
	m_n_voiceon = 0;
	m_n_voiceoff = 0;
	m_n_modulationmode = 0;
	m_n_noisemode = 0;
	m_n_reverbmode = 0;
	m_n_channelonoff = 0;
	m_n_reverbworkareastart = 0;
	m_n_irqaddress = 0;
	m_n_spuoffset = 0;
	m_n_spudata = 0;
	m_n_spucontrol = 0;
	m_n_spustatus = 0;
	m_n_cdvolumeleft = 0;
	m_n_cdvolumeright = 0;
	m_n_externalvolumeleft = 0;
	m_n_externalvolumeright = 0;
	m_n_spu_delay = 0;

	for( n_channel = 0; n_channel < MAX_CHANNEL; n_channel++ )
	{
		m_p_n_volumeleft[ n_channel ] = 0;
		m_p_n_volumeright[ n_channel ] = 0;
		m_p_n_pitch[ n_channel ] = 0;
		m_p_n_address[ n_channel ] = 0;
		m_p_n_envelopestate[ n_channel ] = 0;
		m_p_n_attackdecaysustain[ n_channel ] = 0;
		m_p_n_sustainrelease[ n_channel ] = 0;
		m_p_n_adsrvolume[ n_channel ] = 0;
		m_p_n_repeataddress[ n_channel ] = 0;
		m_p_n_blockaddress[ n_channel ] = 0;
		m_p_n_blockoffset[ n_channel ] = 0;
		m_p_n_blockstatus[ n_channel ] = 0;
		m_p_n_s1[ n_channel ] = 0;
		m_p_n_s2[ n_channel ] = 0;
		m_n_loop[ n_channel ] = 0;
	}
	memset( m_p_n_blockbuffer, 0, sizeof( m_p_n_blockbuffer ) );
	memset( m_p_n_effect, 0, sizeof( m_p_n_effect ) );

	m_p_n_spuram = auto_malloc( SPU_RAM_SIZE );
	if( m_p_n_spuram == NULL )
		return 1;
	memset( m_p_n_spuram, 0, SPU_RAM_SIZE );

	state_save_register_UINT16( "psxspu", 0, "mainvolumeleft", &m_n_mainvolumeleft, 1 );
	state_save_register_UINT16( "psxspu", 0, "mainvolumeright", &m_n_mainvolumeright, 1 );
	state_save_register_UINT16( "psxspu", 0, "reverberationdepthleft", &m_n_reverberationdepthleft, 1 );
	state_save_register_UINT16( "psxspu", 0, "reverberationdepthright", &m_n_reverberationdepthright, 1 );
	state_save_register_UINT32( "psxspu", 0, "voiceon", &m_n_voiceon, 1 );
	state_save_register_UINT32( "psxspu", 0, "voiceoff", &m_n_voiceoff, 1 );
	state_save_register_UINT32( "psxspu", 0, "modulationmode", &m_n_modulationmode, 1 );
	state_save_register_UINT32( "psxspu", 0, "noisemode", &m_n_noisemode, 1 );
	state_save_register_UINT32( "psxspu", 0, "reverbmode", &m_n_reverbmode, 1 );
	state_save_register_UINT32( "psxspu", 0, "channelonoff", &m_n_channelonoff, 1 );
	state_save_register_UINT16( "psxspu", 0, "reverbworkareastart", &m_n_reverbworkareastart, 1 );
	state_save_register_UINT16( "psxspu", 0, "irqaddress", &m_n_irqaddress, 1 );
	state_save_register_UINT32( "psxspu", 0, "spuoffset", &m_n_spuoffset, 1 );
	state_save_register_UINT16( "psxspu", 0, "spudata", &m_n_spudata, 1 );
	state_save_register_UINT16( "psxspu", 0, "spucontrol", &m_n_spucontrol, 1 );
	state_save_register_UINT32( "psxspu", 0, "spustatus", &m_n_spustatus, 1 );
	state_save_register_UINT16( "psxspu", 0, "cdvolumeleft", &m_n_cdvolumeleft, 1 );
	state_save_register_UINT16( "psxspu", 0, "cdvolumeright", &m_n_cdvolumeright, 1 );
	state_save_register_UINT16( "psxspu", 0, "externalvolumeleft", &m_n_externalvolumeleft, 1 );
	state_save_register_UINT16( "psxspu", 0, "externalvolumeright", &m_n_externalvolumeright, 1 );
	state_save_register_UINT16( "psxspu", 0, "volumeleft", m_p_n_volumeleft, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "volumeright", m_p_n_volumeright, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "pitch", m_p_n_pitch, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "address", m_p_n_address, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "envelopestate", m_p_n_envelopestate, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "attackdecaysustain", m_p_n_attackdecaysustain, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "sustainrelease", m_p_n_sustainrelease, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "adsrvolume", m_p_n_adsrvolume, MAX_CHANNEL );
	state_save_register_UINT16( "psxspu", 0, "repeataddress", m_p_n_repeataddress, MAX_CHANNEL );
	state_save_register_UINT32( "psxspu", 0, "effect", m_p_n_effect, 16 );
	state_save_register_UINT16( "psxspu", 0, "spuram", m_p_n_spuram, SPU_RAM_SIZE / 2 );
	state_save_register_UINT32( "psxspu", 0, "blockaddress", m_p_n_blockaddress, MAX_CHANNEL );
	state_save_register_UINT32( "psxspu", 0, "blockoffset", m_p_n_blockoffset, MAX_CHANNEL );
	state_save_register_UINT32( "psxspu", 0, "blockstatus", m_p_n_blockstatus, MAX_CHANNEL );
	state_save_register_INT16( "psxspu", 0, "blockbuffer", m_p_n_blockbuffer, MAX_CHANNEL * SAMPLES_PER_BLOCK );
	state_save_register_INT16( "psxspu", 0, "s1", m_p_n_s1, MAX_CHANNEL );
	state_save_register_INT16( "psxspu", 0, "s2", m_p_n_s2, MAX_CHANNEL );
	state_save_register_UINT32( "psxspu", 0, "loop", m_n_loop, MAX_CHANNEL );
	state_save_register_UINT32( "psxspu", 0, "spu_delay", &m_n_spu_delay, 1 );

	psx_dma_install_read_handler( 4, spu_read );
	psx_dma_install_write_handler( 4, spu_write );

	name[ 0 ] = buf[ 0 ];
	name[ 1 ] = buf[ 1 ];
	sprintf( buf[ 0 ], "%s L", sound_name( msound ) );
	sprintf( buf[ 1 ], "%s R", sound_name( msound ) );
	vol[ 0 ] = MIXER( intf->mixing_level, MIXER_PAN_LEFT );
	vol[ 1 ] = MIXER( intf->mixing_level, MIXER_PAN_RIGHT );
	m_n_stream = stream_init_multi( 2, name, vol, SPU_RATE, 0, spu_update );
	return 0;
}

void PSX_sh_stop( void )
{
	m_p_n_spuram = NULL;
}

void PSX_sh_update( void )
{
}

void PSX_sh_reset( void )
{
}

READ32_HANDLER( psx_spu_delay_r )
{
	return m_n_spu_delay;
}

WRITE32_HANDLER( psx_spu_delay_w )
{
	COMBINE_DATA( &m_n_spu_delay );
}

READ32_HANDLER( psx_spu_r )
{
	int n_channel = offset / 4;
	if( n_channel < MAX_CHANNEL )
	{
		switch( offset % 4 )
		{
		case SPU_CHANNEL_REG( 0x8 ):
			return ( m_p_n_sustainrelease[ n_channel ] << 16 ) | m_p_n_attackdecaysustain[ n_channel ];
		case SPU_CHANNEL_REG( 0xc ):
			return ( m_p_n_repeataddress[ n_channel ] << 16 ) | m_p_n_adsrvolume[ n_channel ];
		default:
			return 0;
		}
	}
	switch( offset )
	{
	case SPU_REG( 0xd88 ): return m_n_voiceon;
	case SPU_REG( 0xd8c ): return m_n_voiceoff;
	case SPU_REG( 0xd90 ): return m_n_modulationmode;
	case SPU_REG( 0xd94 ): return m_n_noisemode;
	case SPU_REG( 0xd98 ): return m_n_reverbmode;
	case SPU_REG( 0xda4 ): return ( ( m_n_spuoffset / 4 ) << 16 ) | m_n_irqaddress;
	case SPU_REG( 0xda8 ): return m_n_spudata | ( m_n_spucontrol << 16 );
	case SPU_REG( 0xdac ): return m_n_spustatus;
	}
	return 0;
}

WRITE32_HANDLER( psx_spu_w )
{
	int n_channel = offset / 4;

	stream_update( m_n_stream, 0 );

	if( n_channel < MAX_CHANNEL )
	{
		switch( offset % 4 )
		{
		case SPU_CHANNEL_REG( 0x0 ):
			if( ACCESSING_LSW32 ) m_p_n_volumeleft[ n_channel ] = data & 0xffff;
			if( ACCESSING_MSW32 ) m_p_n_volumeright[ n_channel ] = data >> 16;
			break;
		case SPU_CHANNEL_REG( 0x4 ):
			if( ACCESSING_LSW32 ) m_p_n_pitch[ n_channel ] = data & 0xffff;
			if( ACCESSING_MSW32 ) m_p_n_address[ n_channel ] = data >> 16;
			break;
		case SPU_CHANNEL_REG( 0x8 ):
			if( ACCESSING_LSW32 ) m_p_n_attackdecaysustain[ n_channel ] = data & 0xffff;
			if( ACCESSING_MSW32 ) m_p_n_sustainrelease[ n_channel ] = data >> 16;
			break;
		case SPU_CHANNEL_REG( 0xc ):
			if( ACCESSING_LSW32 ) m_p_n_adsrvolume[ n_channel ] = data & 0xffff;
			if( ACCESSING_MSW32 ) m_p_n_repeataddress[ n_channel ] = data >> 16;
			break;
		}
		return;
	}

	switch( offset )
	{
	case SPU_REG( 0xd80 ):
		if( ACCESSING_LSW32 ) m_n_mainvolumeleft = data & 0xffff;
		if( ACCESSING_MSW32 ) m_n_mainvolumeright = data >> 16;
		break;
	case SPU_REG( 0xd84 ):
		if( ACCESSING_LSW32 ) m_n_reverberationdepthleft = data & 0xffff;
		if( ACCESSING_MSW32 ) m_n_reverberationdepthright = data >> 16;
		break;
	case SPU_REG( 0xd88 ):
		m_n_voiceon = 0;
		COMBINE_DATA( &m_n_voiceon );
		for( n_channel = 0; n_channel < MAX_CHANNEL; n_channel++ )
		{
			if( ( m_n_voiceon & ( 1 << n_channel ) ) != 0 )
			{
				m_p_n_blockaddress[ n_channel ] = ( m_p_n_address[ n_channel ] * 4 ) % ( SPU_RAM_SIZE / 2 );
				m_p_n_blockoffset[ n_channel ] = ( SAMPLES_PER_BLOCK << PITCH_SHIFT );
				m_p_n_s1[ n_channel ] = 0;
				m_p_n_s2[ n_channel ] = 0;
				m_p_n_blockstatus[ n_channel ] = 1;
				m_p_n_envelopestate[ n_channel ] = e_attack;
			}
		}
		break;
	case SPU_REG( 0xd8c ):
		m_n_voiceoff = 0;
		COMBINE_DATA( &m_n_voiceoff );
		for( n_channel = 0; n_channel < MAX_CHANNEL; n_channel++ )
		{
			/* key off: the envelope goes to the release state */
			if( ( m_n_voiceoff & ( 1 << n_channel ) ) != 0 && m_p_n_envelopestate[ n_channel ] < e_release )
				m_p_n_envelopestate[ n_channel ] = e_release;
		}
		break;
	case SPU_REG( 0xd90 ):
		COMBINE_DATA( &m_n_modulationmode );
		break;
	case SPU_REG( 0xd94 ):
		COMBINE_DATA( &m_n_noisemode );
		break;
	case SPU_REG( 0xd98 ):
		COMBINE_DATA( &m_n_reverbmode );
		break;
	case SPU_REG( 0xd9c ):
		COMBINE_DATA( &m_n_channelonoff );
		break;
	case SPU_REG( 0xda0 ):
		if( ACCESSING_MSW32 ) m_n_reverbworkareastart = data >> 16;
		break;
	case SPU_REG( 0xda4 ):
		if( ACCESSING_LSW32 ) m_n_irqaddress = data & 0xffff;
		if( ACCESSING_MSW32 ) m_n_spuoffset = ( ( data >> 16 ) * 4 ) % ( SPU_RAM_SIZE / 2 );
		break;
	case SPU_REG( 0xda8 ):
		if( ACCESSING_LSW32 )
		{
			m_n_spudata = data & 0xffff;
			m_p_n_spuram[ m_n_spuoffset ] = m_n_spudata;
			m_n_spuoffset = ( m_n_spuoffset + 1 ) % ( SPU_RAM_SIZE / 2 );
		}
		if( ACCESSING_MSW32 ) m_n_spucontrol = data >> 16;
		break;
	case SPU_REG( 0xdac ):
		COMBINE_DATA( &m_n_spustatus );
		m_n_spustatus &= 0xf800ffff;
		break;
	case SPU_REG( 0xdb0 ):
		if( ACCESSING_LSW32 ) m_n_cdvolumeleft = data & 0xffff;
		if( ACCESSING_MSW32 ) m_n_cdvolumeright = data >> 16;
		break;
	case SPU_REG( 0xdb4 ):
		if( ACCESSING_LSW32 ) m_n_externalvolumeleft = data & 0xffff;
		if( ACCESSING_MSW32 ) m_n_externalvolumeright = data >> 16;
		break;
	default:
		if( offset >= SPU_REG( 0xdc0 ) && offset <= SPU_REG( 0xdfc ) )
			COMBINE_DATA( &m_p_n_effect[ offset & 0x0f ] );
		break;
	}
}
