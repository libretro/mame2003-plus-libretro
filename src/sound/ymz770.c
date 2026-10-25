/***************************************************************************

	Yamaha YMZ770C "AMMS-A" (Cave CV1000)

	From current MAME (src/devices/sound/ymz770.cpp: R. Belmont and
	MetalliC, AMM decoding by Olivier Galibert). 16 channels of AMM (an
	MPEG layer II variant) read from the sound flashes, 8 sequencers.

	The CV1000 only connects the right output: the stream is mono.

	Differences from MAME: the decoder state of each channel is in the
	save states (MAME leaves it out); the sample rate is the one of the
	chip clock (MAME changes it to the rate of the AMM header, the CV1000
	samples have the same rate).

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "mpeg_audio.h"
#include "includes/cv1k.h"

#define YMZ770_CLOCK	16384000
#define YMZ770_DIVIDER	1024

typedef struct
{
	UINT8 pan, pan_delay, pan1, pan1_delay, volume_target, volume_delay, volume2;
} ymz_voice_params;

typedef struct
{
	ymz_voice_params current, latch;
	UINT16 phrase;
	INT32 volume;
	UINT8 loop;
	UINT8 pending, is_playing, is_paused, last_block;
	struct mpeg_audio decoder;
	INT16 output_data[0x1000];
	INT32 output_remaining, output_ptr, atbl, pptr;
} ymz_channel;

typedef struct
{
	UINT32 delay;
	UINT16 sequence, timer, stopchan;
	UINT8 loop;
	UINT32 offset;
	UINT8 bank, is_playing, is_paused;
} ymz_sequence;

static ymz_channel *ymz_channels;		/* 16 */
static ymz_sequence ymz_sequences[8];
static UINT8 ymz_cur_reg, ymz_mute, ymz_doen, ymz_vlma, ymz_vlma1, ymz_bsl, ymz_cpl;
static UINT32 ymz_sclock;
static UINT8 *ymz_rom;
static UINT32 ymz_rom_size;
static int ymz_stream;

static UINT8 get_rom_byte(UINT32 offset)
{
	return ymz_rom[offset % ymz_rom_size];
}

static UINT32 get_phrase_offs(int phrase)
{
	return ymz_rom[(4 * phrase) + 1] << 16 | ymz_rom[(4 * phrase) + 2] << 8 | ymz_rom[(4 * phrase) + 3];
}

static UINT32 get_seq_offs(int sqn)
{
	return ymz_rom[(4 * sqn) + 1 + 0x400] << 16 | ymz_rom[(4 * sqn) + 2 + 0x400] << 8 | ymz_rom[(4 * sqn) + 3 + 0x400];
}

static void internal_reg_write(UINT8 reg, UINT8 data);

static void sequencer(void)
{
	int s, ch;

	for (s = 0; s < 8; s++)
	{
		ymz_sequence *sequence = &ymz_sequences[s];

		if (!sequence->is_playing)
			continue;
		if (sequence->delay > 0)
			sequence->delay--;
		else
		{
			int reg = get_rom_byte(sequence->offset++);
			UINT8 data = get_rom_byte(sequence->offset++);
			switch (reg)
			{
				case 0x0f:
					for (ch = 0; ch < 8; ch++)
						if (sequence->stopchan & (1 << ch))
							ymz_channels[ch].is_playing = 0;
					if (sequence->loop)
						sequence->offset = get_seq_offs(sequence->sequence);
					else
						sequence->is_playing = 0;
					break;
				case 0x0e:
					sequence->delay = sequence->timer * 32 + 32 - 1;
					break;
				default:
					internal_reg_write(reg, data);
					break;
			}
		}
	}
}

static void ymz770_update(int param, INT16 *buffer, int length)
{
	const INT32 clip_max3 = 32768 * 75 / 100;
	const INT32 clip_max2 = 32768 * 875 / 1000;
	int i, c;

	for (i = 0; i < length; i++)
	{
		INT32 mixr = 0;

		sequencer();

		for (c = 0; c < 16; c++)
		{
			ymz_channel *channel = &ymz_channels[c];

			if (channel->output_remaining == 0 && channel->is_playing && !channel->is_paused)
			{
				for (;;)
				{
					if (channel->last_block)
					{
						if (channel->loop)
						{
							if (channel->loop != 255)
								--channel->loop;
							channel->pending = 1;
						}
						else
						{
							channel->is_playing = 0;
							channel->output_remaining = 0;
							mpeg_audio_clear(&channel->decoder);
						}
					}
					if (channel->is_playing)
					{
						int sample_rate = ymz_sclock, channel_count;
						int pos;

						/* new phrase */
						if (channel->pending)
						{
							int phrase = channel->phrase;
							channel->atbl = ymz_rom[4 * phrase] >> 4 & 7;
							channel->pptr = 8 * get_phrase_offs(phrase);
							/* the AMM buffer 'contamination' should not be noticeable */
							if (!channel->last_block && channel->latch.volume2 != channel->current.volume2)
								mpeg_audio_clear(&channel->decoder);
							channel->pending = 0;
						}

						/* next block */
						pos = channel->pptr;
						if (!mpeg_audio_decode_buffer_atbl(&channel->decoder, &pos, ymz_rom_size * 8, channel->output_data,
								&channel->output_remaining, &sample_rate, &channel_count, channel->atbl) || channel->output_remaining == 0)
						{
							channel->pptr = pos;
							channel->is_playing = !channel->last_block;	/* no endless retry */
							channel->last_block = 1;
							channel->output_remaining = 0;
							continue;
						}
						channel->pptr = pos;
						channel->last_block = channel->output_remaining < 1152;
						channel->output_ptr = 0;
						channel->current = channel->latch;
					}
					break;
				}
			}

			if (channel->output_remaining > 0)
			{
				/* finish the current block */
				INT32 smpl = channel->output_data[channel->output_ptr++];
				/* volume is linear, 0 - 128 (100%) */
				smpl = (smpl * (channel->volume >> 17)) >> 7;
				smpl = (smpl * channel->current.volume2) >> 7;
				/* pan is linear, 0 - 128: 0 = 100% left, 128 = 100% right */
				mixr += (smpl * channel->current.pan) >> 7;
				channel->output_remaining--;
				if (channel->output_remaining == 0 && !channel->is_playing)
					mpeg_audio_clear(&channel->decoder);
			}
		}

		if (ymz_mute)
			mixr = 0;
		else
		{
			/* main volume is linear, 0 - 255, 128 = 100% */
			mixr *= ymz_vlma;
			mixr >>= 7 - ymz_bsl;
			/* clip limiter: 0 off, 1 6.02 dB (100%), 2 4.86 dB (87.5%), 3 3.52 dB (75%) */
			switch (ymz_cpl)
			{
				case 3: if (mixr < -clip_max3) mixr = -clip_max3; else if (mixr > clip_max3) mixr = clip_max3; break;
				case 2: if (mixr < -clip_max2) mixr = -clip_max2; else if (mixr > clip_max2) mixr = clip_max2; break;
				case 1: if (mixr < -32768) mixr = -32768; else if (mixr > 32767) mixr = 32767; break;
			}
		}
		buffer[i] = mixr < -32768 ? -32768 : mixr > 32767 ? 32767 : mixr;
	}
}

static void internal_reg_write(UINT8 reg, UINT8 data)
{
	if (reg < 0x40)
	{
		/* global registers */
		switch (reg)
		{
			case 0x00:
				ymz_mute = data & 1;
				ymz_doen = data >> 1 & 1;
				break;
			case 0x01:
				ymz_vlma = data;
				break;
			case 0x02:
				ymz_bsl = data & 7;
				ymz_cpl = data >> 4 & 7;
				break;
		}
	}
	else if (reg < 0x60)
	{
		/* playback registers */
		int ch = reg >> 2 & 0x07;
		switch (reg & 0x03)
		{
			case 0:
				ymz_channels[ch].phrase = data;
				break;
			case 1:
				ymz_channels[ch].volume = 128 << 17;
				ymz_channels[ch].latch.volume2 = data;
				break;
			case 2:
				ymz_channels[ch].latch.pan = data << 3;
				break;
			case 3:
				/* both KON bits 1: keep playing, do not restart the channel */
				if ((data & 6) == 2 || ((data & 6) == 6 && !ymz_channels[ch].is_playing))
				{
					ymz_channels[ch].pending = 1;
					ymz_channels[ch].is_playing = 1;
					ymz_channels[ch].last_block = 0;
				}
				else if ((data & 6) == 0)
					ymz_channels[ch].is_playing = 0;
				ymz_channels[ch].loop = (data & 1) ? 255 : 0;
				break;
		}
	}
	else if (reg >= 0x80)
	{
		/* sequencer registers */
		int ch = reg >> 4 & 0x07, i;
		switch (reg & 0x0f)
		{
			case 0:	/* SQSN */
				ymz_sequences[ch].sequence = data;
				break;
			case 1:	/* SQON SQLP */
				if ((data & 6) == 2 || ((data & 6) == 6 && !ymz_sequences[ch].is_playing))
				{
					ymz_sequences[ch].offset = get_seq_offs(ymz_sequences[ch].sequence);
					ymz_sequences[ch].delay = 0;
					ymz_sequences[ch].is_playing = 1;
				}
				else if ((data & 6) == 0 && ymz_sequences[ch].is_playing)
				{
					ymz_sequences[ch].is_playing = 0;
					for (i = 0; i < 8; i++)
						if (ymz_sequences[ch].stopchan & (1 << i))
							ymz_channels[i].is_playing = 0;
				}
				ymz_sequences[ch].loop = data & 1;
				break;
			case 2:	/* TMRH */
				ymz_sequences[ch].timer = (ymz_sequences[ch].timer & 0x00ff) | (data << 8);
				break;
			case 3:	/* TMRL */
				ymz_sequences[ch].timer = (ymz_sequences[ch].timer & 0xff00) | data;
				break;
			case 6:	/* SQOF */
				ymz_sequences[ch].stopchan = data;
				break;
		}
	}
}

void ymz770_write(int offset, data8_t data)
{
	if (offset & 1)
	{
		stream_update(ymz_stream, 0);
		internal_reg_write(ymz_cur_reg, data);
	}
	else
		ymz_cur_reg = data;
}

static void ymz770_reset(void)
{
	int i;

	for (i = 0; i < 16; i++)
	{
		ymz_channel *channel = &ymz_channels[i];
		channel->latch.pan = 64;
		channel->latch.pan_delay = 0;
		channel->latch.pan1 = 64;
		channel->latch.pan1_delay = 0;
		channel->latch.volume_target = 0;
		channel->latch.volume_delay = 0;
		channel->latch.volume2 = 0;
		channel->current = channel->latch;
		channel->phrase = 0;
		channel->volume = 0;
		channel->loop = 0;
		channel->pending = 0;
		channel->is_playing = 0;
		channel->is_paused = 0;
		channel->last_block = 0;
		channel->output_remaining = 0;
		mpeg_audio_clear(&channel->decoder);
	}
	for (i = 0; i < 8; i++)
	{
		ymz_sequence *sequence = &ymz_sequences[i];
		memset(sequence, 0, sizeof(*sequence));
	}
}

static void ymz770_save_state(void)
{
	int i;

	state_save_register_UINT8("ymz770", 0, "cur_reg", &ymz_cur_reg, 1);
	state_save_register_UINT8("ymz770", 0, "mute", &ymz_mute, 1);
	state_save_register_UINT8("ymz770", 0, "doen", &ymz_doen, 1);
	state_save_register_UINT8("ymz770", 0, "vlma", &ymz_vlma, 1);
	state_save_register_UINT8("ymz770", 0, "vlma1", &ymz_vlma1, 1);
	state_save_register_UINT8("ymz770", 0, "bsl", &ymz_bsl, 1);
	state_save_register_UINT8("ymz770", 0, "cpl", &ymz_cpl, 1);
	state_save_register_UINT32("ymz770", 0, "sclock", &ymz_sclock, 1);

	for (i = 0; i < 16; i++)
	{
		ymz_channel *c = &ymz_channels[i];
		state_save_register_UINT8("ymz770_ch", i, "current", &c->current.pan, 7);
		state_save_register_UINT8("ymz770_ch", i, "latch", &c->latch.pan, 7);
		state_save_register_UINT16("ymz770_ch", i, "phrase", &c->phrase, 1);
		state_save_register_INT32("ymz770_ch", i, "volume", &c->volume, 1);
		state_save_register_UINT8("ymz770_ch", i, "loop", &c->loop, 1);
		state_save_register_UINT8("ymz770_ch", i, "pending", &c->pending, 1);
		state_save_register_UINT8("ymz770_ch", i, "is_playing", &c->is_playing, 1);
		state_save_register_UINT8("ymz770_ch", i, "is_paused", &c->is_paused, 1);
		state_save_register_UINT8("ymz770_ch", i, "last_block", &c->last_block, 1);
		state_save_register_INT16("ymz770_ch", i, "output_data", c->output_data, 0x1000);
		state_save_register_INT32("ymz770_ch", i, "output_remaining", &c->output_remaining, 1);
		state_save_register_INT32("ymz770_ch", i, "output_ptr", &c->output_ptr, 1);
		state_save_register_INT32("ymz770_ch", i, "atbl", &c->atbl, 1);
		state_save_register_INT32("ymz770_ch", i, "pptr", &c->pptr, 1);
		/* the synthesis history of the decoder */
		state_save_register_double("ymz770_ch", i, "audio_buffer", &c->decoder.audio_buffer[0][0], 2 * 32 * 32);
		state_save_register_int("ymz770_ch", i, "audio_buffer_pos0", &c->decoder.audio_buffer_pos[0]);
		state_save_register_int("ymz770_ch", i, "audio_buffer_pos1", &c->decoder.audio_buffer_pos[1]);
	}
	for (i = 0; i < 8; i++)
	{
		ymz_sequence *s = &ymz_sequences[i];
		state_save_register_UINT32("ymz770_seq", i, "delay", &s->delay, 1);
		state_save_register_UINT16("ymz770_seq", i, "sequence", &s->sequence, 1);
		state_save_register_UINT16("ymz770_seq", i, "timer", &s->timer, 1);
		state_save_register_UINT16("ymz770_seq", i, "stopchan", &s->stopchan, 1);
		state_save_register_UINT8("ymz770_seq", i, "loop", &s->loop, 1);
		state_save_register_UINT32("ymz770_seq", i, "offset", &s->offset, 1);
		state_save_register_UINT8("ymz770_seq", i, "bank", &s->bank, 1);
		state_save_register_UINT8("ymz770_seq", i, "is_playing", &s->is_playing, 1);
		state_save_register_UINT8("ymz770_seq", i, "is_paused", &s->is_paused, 1);
	}
}

static int ymz770_sh_start(const struct MachineSound *msound)
{
	int i;

	ymz_rom = memory_region(REGION_SOUND1);
	ymz_rom_size = memory_region_length(REGION_SOUND1);
	ymz_sclock = YMZ770_CLOCK / YMZ770_DIVIDER;

	ymz_channels = auto_malloc(16 * sizeof(ymz_channel));
	if (!ymz_channels)
		return 1;
	memset(ymz_channels, 0, 16 * sizeof(ymz_channel));
	for (i = 0; i < 16; i++)
		mpeg_audio_init(&ymz_channels[i].decoder, ymz_rom, MPEG_AUDIO_AMM);

	ymz_cur_reg = ymz_mute = ymz_doen = ymz_vlma = ymz_vlma1 = ymz_bsl = ymz_cpl = 0;
	ymz770_reset();
	ymz770_save_state();

	ymz_stream = stream_init("YMZ770", 100, ymz_sclock, 0, ymz770_update);
	return ymz_stream < 0;
}

/* the machine reset (the device reset of MAME) */
void ymz770_reset_chip(void)
{
	if (ymz_channels)
		ymz770_reset();
}

struct CustomSound_interface ymz770_custom_interface =
{
	ymz770_sh_start, NULL, NULL
};
