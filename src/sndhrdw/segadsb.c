/***************************************************************************

    Sega Digital Sound Boards (DSB): MPEG audio music boards of the Model
    1/2/3 era, from current MAME (src/mame/sega/dsbz80.cpp and dsb2.cpp;
    copyright R. Belmont, Olivier Galibert, BSD-3-Clause), in C for MAME
    2003.

    - DSB (Z80): the Z80 gets the game's sound commands on an i8251 (the
      same MIDI line as the SCSP) and drives the MPEG decoder through I/O
      ports: start/end addresses, play/loop/stop, volume, stereo mode.
    - DSB2 (68000): same idea, the decoder is driven through a command
      FIFO; a 1 kHz timer interrupt paces the 68000.

    The decoded audio (32 kHz stereo) is a stream of its own, mixed with
    the SCSP.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "sound/mpeg_audio.h"
#include "includes/segadsb.h"

#define RXQ_SIZE	256

static int dsb_type;		/* SEGADSB_NONE, SEGADSB_Z80, SEGADSB_68K */
static int dsb_cpu;
static struct mpeg_audio *dsb_decoder;
static UINT8 *dsb_mpeg_rom;
static UINT32 dsb_mpeg_len;
static int dsb_stream = -1;

/* i8251: received bytes waiting for the board CPU */
static UINT8 dsb_rxq[ RXQ_SIZE ];
static int dsb_rxq_head, dsb_rxq_count;

/* MPEG player */
static UINT32 mp_start, mp_end, lp_start, lp_end, reg_start, reg_end;
static int mp_vol, mp_pan, mp_state;	/* mp_state: 0 stop, 1 play, 2 play and loop */
static int mp_pos;						/* in bits */
static INT16 audio_buf[ 1152 * 2 ];
static int audio_pos, audio_avail;

/* DSB2 FIFO command state */
enum { CMD_IDLE, CMD_START_HI, CMD_START_MD, CMD_START_LO, CMD_END_HI, CMD_END_MD, CMD_END_LO };
static int dsb2_command;
static UINT16 *dsb2_ram;
static void *dsb2_timer;


/*************************************
 *  Serial input
 *************************************/

static void dsb_update_irq( void )
{
	if( dsb_type == SEGADSB_Z80 )
		cpu_set_irq_line( dsb_cpu, 0, dsb_rxq_count ? ASSERT_LINE : CLEAR_LINE );
	else if( dsb_type == SEGADSB_68K )
		cpu_set_irq_line( dsb_cpu, 1, dsb_rxq_count ? ASSERT_LINE : CLEAR_LINE );
}

/* a byte the i960 sent on the sound line (a full queue drops it, as an
   overrun of the real i8251 would) */
void segadsb_midi_in( UINT8 data )
{
	if( dsb_type == SEGADSB_NONE || dsb_rxq_count >= RXQ_SIZE )
		return;
	dsb_rxq[ ( dsb_rxq_head + dsb_rxq_count ) % RXQ_SIZE ] = data;
	dsb_rxq_count++;
	dsb_update_irq();
}

/* i8251: data (offset 0) and status (offset 1) */
static UINT8 uart_r( int offset )
{
	if( offset & 1 )
		return 0x85 | ( dsb_rxq_count ? 0x02 : 0 );	/* DSR, TxE, TxRDY, RxRDY */
	if( dsb_rxq_count )
	{
		UINT8 data = dsb_rxq[ dsb_rxq_head ];
		dsb_rxq_head = ( dsb_rxq_head + 1 ) % RXQ_SIZE;
		dsb_rxq_count--;
		dsb_update_irq();
		return data;
	}
	return 0;
}


/*************************************
 *  MPEG player
 *************************************/

static void mpeg_stop( void )
{
	mp_state = 0;
	audio_pos = audio_avail = 0;
}

static void mpeg_play( UINT32 start, UINT32 end, int state )
{
	/* never start a window outside the ROM (as MAME's DSB2 guard) */
	if( start >= dsb_mpeg_len || end > dsb_mpeg_len || end <= start )
	{
		mpeg_stop();
		return;
	}
	mp_start = start;
	mp_end = end;
	mp_pos = (int)( start * 8 );
	mp_state = state;
}

static void dsb_stream_update( int param, INT16 **buffer, int length )
{
	INT16 *left = buffer[ 0 ], *right = buffer[ 1 ];
	int i = 0;

	if( dsb_stream >= 0 && Machine->sample_rate )
	for( ;; )
	{
		while( i < length && audio_pos < audio_avail )
		{
			int l = audio_buf[ audio_pos * 2 ], r = audio_buf[ audio_pos * 2 + 1 ];
			if( mp_pan == 1 ) r = l;	/* left on both channels */
			if( mp_pan == 2 ) l = r;	/* right on both channels */
			left[ i ] = (INT16)( l * mp_vol / 128 );
			right[ i ] = (INT16)( r * mp_vol / 128 );
			audio_pos++;
			i++;
		}
		if( i >= length || mp_state == 0 )
			break;

		{
			int samples, rate, channels;
			UINT32 end = mp_end < dsb_mpeg_len ? mp_end : dsb_mpeg_len;	/* never read past the ROM */
			if( mp_pos < 0 || mp_pos >= (int)( end * 8 ) )
				samples = 0;
			if( mp_pos >= 0 && mp_pos < (int)( end * 8 ) &&
				mpeg_audio_decode_buffer( dsb_decoder, &mp_pos, (int)( end * 8 ), audio_buf, &samples, &rate, &channels ) )
			{
				if( channels == 1 )
				{
					/* mono frame: both channels */
					int k;
					for( k = samples - 1; k >= 0; k-- )
						audio_buf[ k * 2 ] = audio_buf[ k * 2 + 1 ] = audio_buf[ k ];
				}
				audio_avail = samples;
				audio_pos = 0;
			}
			else if( mp_state == 2 )
			{
				/* the loop: from the latched start and end */
				if( mp_pos == (int)( lp_start * 8 ) )
					mp_state = 0;	/* looping on undecodable data: stop (as MAME) */
				mp_pos = (int)( lp_start * 8 );
				if( lp_end )
					mp_end = lp_end;
			}
			else
				mp_state = 0;
		}
	}

	for( ; i < length; i++ )
		left[ i ] = right[ i ] = 0;
}


/*************************************
 *  DSB (Z80)
 *************************************/

/* writes to start and end while a stream plays are latched for the loop */
static void dsb_addr_w( UINT32 *reg, int offset, UINT8 data, int is_start )
{
	int shift = ( 2 - offset ) * 8;
	*reg = ( *reg & ~( 0xff << shift ) ) | ( (UINT32)data << shift );
	if( offset != 2 )
		return;
	if( mp_state == 0 )
	{
		if( is_start ) mp_start = *reg;
		else mp_end = *reg;
	}
	else
	{
		if( is_start ) lp_start = *reg;
		else lp_end = *reg;
	}
}

READ_HANDLER( segadsb_z80_port_r )
{
	UINT32 prg = (UINT32)( mp_pos >> 3 );
	switch( offset & 0xff )
	{
	case 0xe2: return ( prg >> 16 ) & 0xff;
	case 0xe3: return ( prg >> 8 ) & 0xff;
	case 0xe4: return prg & 0xff;
	case 0xf0: return uart_r( 0 );
	case 0xf1: return uart_r( 1 );
	}
	return 0xff;
}

WRITE_HANDLER( segadsb_z80_port_w )
{
	stream_update( dsb_stream, 0 );
	switch( offset & 0xff )
	{
	case 0xe0:	/* 0 stop, 1 play, 2 play with loop */
		if( data == 0 )
			mpeg_stop();
		else
		{
			mp_state = data;
			mp_pos = (int)( mp_start * 8 );
		}
		break;
	case 0xe2: case 0xe3: case 0xe4:
		dsb_addr_w( &reg_start, ( offset & 0xff ) - 0xe2, data, 1 );
		break;
	case 0xe5: case 0xe6: case 0xe7:
		dsb_addr_w( &reg_end, ( offset & 0xff ) - 0xe5, data, 0 );
		break;
	case 0xe8:	/* volume (the top bit is unknown) */
		mp_vol = ~data & 0x7f;
		break;
	case 0xe9:	/* 0 stereo, 1 left on both, 2 right on both */
		mp_pan = data & 3;
		break;
	}
}


/*************************************
 *  DSB2 (68000)
 *************************************/

static void dsb2_fifo_w( UINT8 data )
{
	switch( dsb2_command )
	{
	case CMD_START_HI: reg_start = ( reg_start & 0x00ffff ) | ( (UINT32)data << 16 ); dsb2_command = CMD_START_MD; return;
	case CMD_START_MD: reg_start = ( reg_start & 0xff00ff ) | ( (UINT32)data << 8 ); dsb2_command = CMD_START_LO; return;
	case CMD_START_LO: reg_start = ( reg_start & 0xffff00 ) | data; mp_start = reg_start; dsb2_command = CMD_IDLE; return;
	case CMD_END_HI: reg_end = ( reg_end & 0x00ffff ) | ( (UINT32)data << 16 ); dsb2_command = CMD_END_MD; return;
	case CMD_END_MD: reg_end = ( reg_end & 0xff00ff ) | ( (UINT32)data << 8 ); dsb2_command = CMD_END_LO; return;
	case CMD_END_LO: reg_end = ( reg_end & 0xffff00 ) | data; mp_end = reg_end; dsb2_command = CMD_IDLE; return;
	}

	/* the low bit is the player number */
	switch( data & 0xfe )
	{
	case 0x14: dsb2_command = CMD_START_HI; break;
	case 0x24: dsb2_command = CMD_END_HI; break;
	case 0x74: mpeg_play( mp_start & 0xffffff, mp_end & 0xffffff, 1 ); break;
	case 0x84: mpeg_stop(); break;
	}
}

READ16_HANDLER( segadsb_68k_r )
{
	UINT32 a = 0xc00000 + offset * 2;	/* the handler covers 0xc00000-0xe80001 */
	if( a >= 0xc00000 && a < 0xc00004 )
		return uart_r( ( a >> 1 ) & 1 );
	if( a == 0xe80000 )
		return 0x01;	/* MPEG status */
	return 0;
}

WRITE16_HANDLER( segadsb_68k_w )
{
	UINT32 a = 0xc00000 + offset * 2;	/* the handler covers 0xc00000-0xe80001 */
	if( mem_mask & 0xff )
		return;	/* the registers are on the low byte */
	if( a == 0xe00002 )
	{
		stream_update( dsb_stream, 0 );
		dsb2_fifo_w( data & 0xff );
	}
	/* 0xc00000: i8251 mode/command, 0xd00000: system control (MPEG ROM bank
	   above 16 MB, not used by Model 2) */
}

static void dsb2_timer_cb( int param )
{
	cpu_set_irq_line( dsb_cpu, 2, HOLD_LINE );
}


/*************************************
 *  Init, reset, sound start
 *************************************/

int segadsb_mpeg_region = REGION_SOUND2;

void segadsb_init( int type, int cpu )
{
	dsb_type = type;
	dsb_cpu = cpu;
	dsb_stream = -1;
	if( type == SEGADSB_NONE )
	{
		segadsb_mpeg_region = REGION_SOUND2;
		return;
	}

	dsb_mpeg_rom = memory_region( segadsb_mpeg_region );
	dsb_mpeg_len = memory_region_length( segadsb_mpeg_region );
	segadsb_mpeg_region = REGION_SOUND2;	/* back to the default for the next game */
	dsb_decoder = auto_malloc( sizeof( struct mpeg_audio ) );
	mpeg_audio_init( dsb_decoder, dsb_mpeg_rom, MPEG_AUDIO_L2 );

	if( type == SEGADSB_68K )
	{
		dsb2_ram = auto_malloc( 0x20000 );
		memset( dsb2_ram, 0, 0x20000 );
		cpu_setbank( 16, dsb2_ram );
		dsb2_timer = timer_alloc( dsb2_timer_cb );
		state_save_register_UINT16( "segadsb", 0, "ram", dsb2_ram, 0x10000 );
	}

	state_save_register_UINT8( "segadsb", 0, "rxq", dsb_rxq, RXQ_SIZE );
	state_save_register_int( "segadsb", 0, "rxq_head", &dsb_rxq_head );
	state_save_register_int( "segadsb", 0, "rxq_count", &dsb_rxq_count );
	state_save_register_UINT32( "segadsb", 0, "mp_start", &mp_start, 1 );
	state_save_register_UINT32( "segadsb", 0, "mp_end", &mp_end, 1 );
	state_save_register_UINT32( "segadsb", 0, "lp_start", &lp_start, 1 );
	state_save_register_UINT32( "segadsb", 0, "lp_end", &lp_end, 1 );
	state_save_register_UINT32( "segadsb", 0, "start", &reg_start, 1 );
	state_save_register_UINT32( "segadsb", 0, "end", &reg_end, 1 );
	state_save_register_int( "segadsb", 0, "vol", &mp_vol );
	state_save_register_int( "segadsb", 0, "pan", &mp_pan );
	state_save_register_int( "segadsb", 0, "state", &mp_state );
	state_save_register_int( "segadsb", 0, "pos", &mp_pos );
	state_save_register_INT16( "segadsb", 0, "audio_buf", audio_buf, 1152 * 2 );
	state_save_register_int( "segadsb", 0, "audio_pos", &audio_pos );
	state_save_register_int( "segadsb", 0, "audio_avail", &audio_avail );
	state_save_register_int( "segadsb", 0, "command", &dsb2_command );
	state_save_register_double( "segadsb", 0, "synth", &dsb_decoder->audio_buffer[ 0 ][ 0 ], 2 * 32 * 32 );
	state_save_register_int( "segadsb", 0, "synth_pos0", &dsb_decoder->audio_buffer_pos[ 0 ] );
	state_save_register_int( "segadsb", 0, "synth_pos1", &dsb_decoder->audio_buffer_pos[ 1 ] );
}

void segadsb_reset( void )
{
	if( dsb_type == SEGADSB_NONE )
		return;
	dsb_rxq_head = dsb_rxq_count = 0;
	mp_start = mp_end = lp_start = lp_end = reg_start = reg_end = 0;
	mp_vol = 0x7f;
	mp_pan = 0;
	mpeg_stop();
	mp_pos = 0;
	dsb2_command = CMD_IDLE;
	memset( audio_buf, 0, sizeof( audio_buf ) );
	mpeg_audio_clear( dsb_decoder );
	if( dsb_type == SEGADSB_Z80 )
		memset( memory_region( REGION_CPU1 + dsb_cpu ) + 0x8000, 0, 0x8000 );	/* RAM */
	if( dsb_type == SEGADSB_68K )
	{
		cpu_setbank( 16, dsb2_ram );
		timer_adjust( dsb2_timer, TIME_IN_HZ( 1000 ), 0, TIME_IN_HZ( 1000 ) );
	}
	dsb_update_irq();
}

static int segadsb_sh_start( const struct MachineSound *msound )
{
	const char *names[] = { "DSB left", "DSB right" };
	/* mixed 9 dB under the SCSP's full level: the music against the sound
	   effects measured on two recordings of Top Skater cabinets (attract
	   mode, aligned on the music) came out 9 dB lower than at full level */
	int vols[] = { MIXER( 35, MIXER_PAN_LEFT ), MIXER( 35, MIXER_PAN_RIGHT ) };
	if( dsb_type == SEGADSB_NONE )
		return 0;
	dsb_stream = stream_init_multi( 2, names, vols, 32000, 0, dsb_stream_update );
	return 0;
}

struct CustomSound_interface segadsb_custom_interface =
{
	segadsb_sh_start, NULL, NULL
};
