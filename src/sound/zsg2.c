/***************************************************************************

  ZOOM ZSG-2 custom wavetable synthesizer

  Backported to mame2003-plus from current MAME src/devices/sound/zsg2.cpp:
  written by Olivier Galibert, MAME conversion by R. Belmont, working
  emulation by hap, properly working emulation by superctr. See the MAME
  file for the register map and the sample format.

  The decoded sample cache is not saved: it only depends on the ROM.

***************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "state.h"
#include "zsg2.h"

#define STATUS_ACTIVE	0x8000

#define EMPHASIS_INITIAL_BIAS	0
/* Adjusts the cutoff constant of the filter by right-shifting the filter state.
   The current value gives a -6dB cutoff frequency at about 81.5 Hz, assuming
   sample playback at 32.552 kHz. */
#define EMPHASIS_FILTER_SHIFT	(16-10)
#define EMPHASIS_ROUNDING		0x20
/* Adjusts the output amplitude by right-shifting the filtered output. */
#define EMPHASIS_OUTPUT_SHIFT	1

/* 16 registers per channel, 48 channels */
struct zchan
{
	UINT16 v[16];
	UINT16 status;
	UINT32 cur_pos;
	UINT32 step_ptr;
	UINT32 step;
	UINT32 start_pos;
	UINT32 end_pos;
	UINT32 loop_pos;
	UINT32 page;

	UINT16 vol;
	UINT16 vol_initial;
	UINT16 vol_target;
	INT16 vol_delta;

	UINT16 output_cutoff;
	UINT16 output_cutoff_initial;
	UINT16 output_cutoff_target;
	INT16 output_cutoff_delta;

	INT32 emphasis_filter_state;
	INT32 output_filter_state;

	UINT8 output_gain[4];	/* attenuation for output channels */
	INT16 samples[5];		/* +1 history */
};

static UINT16 gain_tab[256];
static UINT16 zsg2_reg[32];
static struct zchan chan[48];
static UINT32 sample_count;

static const UINT8 *mem_base;
static UINT32 mem_blocks;
static UINT32 read_address;
static UINT32 *mem_copy;
static INT16 *full_samples;

static UINT32 read_memory(UINT32 offset)
{
	const UINT8 *p;
	if (offset >= mem_blocks)
		return 0;
	p = mem_base + offset * 4;
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
}

static INT16 *prepare_samples(UINT32 offset)
{
	UINT32 block = read_memory(offset);
	UINT8 shift;
	UINT32 i;

	if (block == 0)
		return &full_samples[mem_blocks * 4]; /* overflow or 0 */

	if (block == mem_copy[offset])
		return &full_samples[offset * 4]; /* cached */

	mem_copy[offset] = block;
	offset *= 4;

	/* decompress 32 bit block to 4 16-bit samples */
	/* 42222222 51111111 60000000 ssss3333 */
	full_samples[offset|0] = block >> 8 & 0x7f;
	full_samples[offset|1] = block >> 16 & 0x7f;
	full_samples[offset|2] = block >> 24 & 0x7f;
	full_samples[offset|3] = (block >> (8+1) & 0x40) | (block >> (16+2) & 0x20) | (block >> (24+3) & 0x10) | (block & 0xf);

	/* sign-extend and shift */
	shift = block >> 4 & 0xf;
	for (i = offset; i < (offset + 4); i++)
	{
		full_samples[i] <<= 9;
		full_samples[i] >>= shift;
	}

	return &full_samples[offset];
}

/* fill the buffer with filtered samples */
static void filter_samples(struct zchan *ch)
{
	INT16 *raw_samples = prepare_samples(ch->page | ch->cur_pos);
	int i;
	ch->samples[0] = ch->samples[4]; /* we want to remember the last sample */

	for (i = 0; i < 4; i++)
	{
		INT32 sample;
		ch->emphasis_filter_state += raw_samples[i]-((ch->emphasis_filter_state+EMPHASIS_ROUNDING)>>EMPHASIS_FILTER_SHIFT);

		sample = ch->emphasis_filter_state >> EMPHASIS_OUTPUT_SHIFT;
		if (sample < -32768) sample = -32768;
		if (sample > 32767) sample = 32767;
		ch->samples[i+1] = sample;
	}
}

static UINT16 ramp(UINT16 current, UINT16 target, INT16 delta)
{
	INT32 rampval = current + delta;
	if (delta < 0 && rampval < target)
		rampval = target;
	else if (delta >= 0 && rampval > target)
		rampval = target;

	return rampval;
}

void zsg2_update(INT32 **outputs, int samples)
{
	int i, c, output;

	for (i = 0; i < samples; i++)
	{
		INT32 mix[4];
		mix[0] = mix[1] = mix[2] = mix[3] = 0;

		/* loop over all channels */
		for (c = 0; c < 48; c++)
		{
			struct zchan *elem = &chan[c];
			UINT8 sample_pos;
			INT32 sample;

			if (~elem->status & STATUS_ACTIVE)
				continue;

			elem->step_ptr += elem->step;
			if (elem->step_ptr & 0xffff0000)
			{
				if (++elem->cur_pos >= elem->end_pos)
				{
					/* loop sample */
					elem->cur_pos = elem->loop_pos;
					if ((elem->cur_pos + 1) >= elem->end_pos)
					{
						/* end of sample */
						elem->vol = 0; /* this should help the channel allocation just a bit */
						elem->status &= ~STATUS_ACTIVE;
						continue;
					}
				}

				if (elem->cur_pos == elem->start_pos)
					elem->emphasis_filter_state = EMPHASIS_INITIAL_BIAS;

				elem->step_ptr &= 0xffff;
				filter_samples(elem);
			}

			sample_pos = elem->step_ptr >> 14 & 3;
			sample = elem->samples[sample_pos];

			/* linear interpolation (hardware certainly does something similar) */
			sample += ((UINT16)(elem->step_ptr << 2 & 0xffff) * (INT16)(elem->samples[sample_pos+1] - sample)) >> 16;

			/* another filter... */
			elem->output_filter_state += (sample - (elem->output_filter_state >> 16)) * elem->output_cutoff;
			sample = elem->output_filter_state >> 16;

			/* to prevent DC bias, we need to slowly discharge the filter when the output filter cutoff is 0 */
			if (!elem->output_cutoff)
				elem->output_filter_state >>= 1;

			sample = (sample * elem->vol) >> 16;

			for (output = 0; output < 4; output++)
			{
				int output_gain = elem->output_gain[output] & 0x1f; /* left / right */
				INT32 output_sample = sample;

				if (elem->output_gain[output] & 0x80) /* perhaps ? */
					output_sample = -output_sample;

				mix[output] += (output_sample * gain_tab[output_gain & 0x1f]) >> 16;
			}

			/* apply ramping every other update */
			/* it's possible key on is handled on the other sample */
			if (sample_count & 1)
			{
				elem->vol = ramp(elem->vol, elem->vol_target, elem->vol_delta);
				elem->output_cutoff = ramp(elem->output_cutoff, elem->output_cutoff_target, elem->output_cutoff_delta);
			}
		}

		for (output = 0; output < 4; output++)
		{
			INT32 m = mix[output];
			if (m < -32768) m = -32768;
			if (m > 32767) m = 32767;
			outputs[output][i] = m;
		}
	}
	sample_count++;
}

/* Convert ramping register value to something more usable.
   Upper 4 bits is a shift amount, lower 4 bits is a 2's complement value.
   Get ramp amount by sign extending the low 4 bits, XOR by 8, then
   shifting it by the upper 4 bits. */
static INT16 get_ramp(UINT8 val)
{
	INT16 frac = val << 12; /* sign extend */
	frac = ((frac >> 12) ^ 8) << (val >> 4);
	return (frac >> 4);
}

static void chan_w(int ch, int reg, UINT16 data)
{
	switch (reg)
	{
		case 0x0:
			/* lo byte: unknown, 0 on most games; hi byte: start address low */
			chan[ch].start_pos = (chan[ch].start_pos & 0xff00) | (data >> 8 & 0xff);
			break;

		case 0x1:
			/* lo byte: start address high; hi byte: address page */
			chan[ch].start_pos = (chan[ch].start_pos & 0x00ff) | (data << 8 & 0xff00);
			chan[ch].page = (UINT32)data << 8 & 0xff0000;
			break;

		case 0x2:
			/* no function? always 0 */
			break;

		case 0x3:
			/* unknown, always 0x0400. is this a flag? */
			chan[ch].status &= 0x8000;
			chan[ch].status |= data & 0x7fff;
			break;

		case 0x4:
			/* frequency */
			chan[ch].step = data + 1;
			break;

		case 0x5:
			/* lo byte: loop address low; hi byte: right output gain (direct) */
			chan[ch].loop_pos = (chan[ch].loop_pos & 0xff00) | (data & 0xff);
			chan[ch].output_gain[3] = data >> 8;
			break;

		case 0x6:
			/* end address */
			chan[ch].end_pos = data;
			break;

		case 0x7:
			/* lo byte: loop address high; hi byte: left output gain (direct) */
			chan[ch].loop_pos = (chan[ch].loop_pos & 0x00ff) | (data << 8 & 0xff00);
			chan[ch].output_gain[2] = data >> 8;
			break;

		case 0x8:
			/* IIR lowpass time constant (initial, latched on key on) */
			chan[ch].output_cutoff_initial = data;
			break;

		case 0x9:
			/* writes 0 at key on */
			chan[ch].output_cutoff = data;
			break;

		case 0xa:
			/* volume (initial, latched on key on) */
			chan[ch].vol_initial = data;
			break;

		case 0xb:
			/* writes 0 at key on */
			chan[ch].vol = data;
			break;

		case 0xc:
			/* IIR lowpass time constant (target) */
			chan[ch].output_cutoff_target = data;
			break;

		case 0xd:
			/* hi byte: DSP channel 1 (chorus) gain; lo byte: filter ramping speed */
			chan[ch].output_gain[1] = data >> 8;
			chan[ch].output_cutoff_delta = get_ramp(data & 0xff);
			break;

		case 0xe:
			/* volume target */
			chan[ch].vol_target = data;
			break;

		case 0xf:
			/* hi byte: DSP channel 0 (reverb) gain; lo byte: volume ramping speed */
			chan[ch].output_gain[0] = data >> 8;
			chan[ch].vol_delta = get_ramp(data & 0xff);
			break;

		default:
			break;
	}

	chan[ch].v[reg] = data;
}

static UINT16 chan_r(int ch, int reg)
{
	switch (reg)
	{
		case 0x3:
			/* no games read from this */
			return chan[ch].status;
		case 0x9:
			/* pretty certain, though no games actually read from this */
			return chan[ch].output_cutoff;
		case 0xb:
			/* lower 13 bits contain the output volume (used by G-NET driver to allocate voices) */
			return chan[ch].vol >> 3;
		default:
			break;
	}

	return chan[ch].v[reg];
}

static void control_w(int reg, UINT16 data)
{
	int base, i, ch;

	switch (reg)
	{
		case 0x00: case 0x01: case 0x02:
			/* key on */
			base = (reg & 3) << 4;
			for (i = 0; i < 16; i++)
			{
				if (data & (1 << i))
				{
					ch = base | i;
					chan[ch].status |= STATUS_ACTIVE;
					chan[ch].cur_pos = chan[ch].start_pos - 1;
					chan[ch].step_ptr = 0x10000;
					/* ignoring the "initial volume" for now because it causes lots of clicking */
					chan[ch].vol = 0;
					chan[ch].vol_delta = 0x0400; /* register 06 ? */
					chan[ch].output_cutoff = chan[ch].output_cutoff_initial;
					chan[ch].output_filter_state = 0;
				}
			}
			break;

		case 0x04: case 0x05: case 0x06:
			/* key off */
			base = (reg & 3) << 4;
			for (i = 0; i < 16; i++)
			{
				if (data & (1 << i))
				{
					ch = base | i;
					chan[ch].vol = 0;
					chan[ch].status &= ~STATUS_ACTIVE;
				}
			}
			break;

		case 0x1c:
			/* rom readback address low (low 2 bits always 0) */
			read_address = (read_address & 0x3fffc000) | (data >> 2 & 0x00003fff);
			break;
		case 0x1d:
			/* rom readback address high */
			read_address = (read_address & 0x00003fff) | ((UINT32)data << 14 & 0x3fffc000);
			break;

		default:
			if (reg < 0x20)
				zsg2_reg[reg] = data;
			break;
	}
}

static UINT16 control_r(int reg)
{
	switch (reg)
	{
		case 0x14:
			/* memory bus busy? right before reading memory, it polls until low 8 bits are 0 */
			return 0;

		case 0x1e:
			/* rom readback word low */
			return read_memory(read_address) & 0xffff;
		case 0x1f:
			/* rom readback word high */
			return read_memory(read_address) >> 16;

		default:
			if (reg < 0x20)
				return zsg2_reg[reg];
			break;
	}

	return 0;
}

void zsg2_write(offs_t offset, UINT16 data)
{
	if (offset < 0x300)
		chan_w(offset >> 4, offset & 0xf, data);
	else
		control_w(offset - 0x300, data);
}

UINT16 zsg2_read(offs_t offset)
{
	if (offset < 0x300)
		return chan_r(offset >> 4, offset & 0xf);
	return control_r(offset - 0x300);
}

void zsg2_init(const UINT8 *rom, UINT32 rom_bytes)
{
	int i;

	memset(chan, 0, sizeof(chan));
	memset(zsg2_reg, 0, sizeof(zsg2_reg));
	mem_base = rom;
	mem_blocks = rom_bytes / 4;
	mem_copy = auto_malloc(mem_blocks * sizeof(UINT32));
	full_samples = auto_malloc((mem_blocks * 4 + 4) * sizeof(INT16));
	memset(mem_copy, 0, mem_blocks * sizeof(UINT32));
	memset(full_samples, 0, (mem_blocks * 4 + 4) * sizeof(INT16));
	read_address = 0;
	sample_count = 0;

	/* the output gain table, assuming -1dB per step for now */
	memset(gain_tab, 0, sizeof(gain_tab));
	for (i = 1; i < 32; i++)
		gain_tab[i] = (UINT16)(pow(10, -(31 - i) / 20.) * 65535.);
	gain_tab[0] = 0;
}

void zsg2_reset(void)
{
	int ch, reg;

	read_address = 0;

	/* stop playing and clear all channels */
	control_w(4, 0xffff);
	control_w(5, 0xffff);
	control_w(6, 0xffff);

	for (ch = 0; ch < 48; ch++)
		for (reg = 0; reg < 0x10; reg++)
			chan_w(ch, reg, 0);
	sample_count = 0;
}

/* 48 channels: the save state takes 25 instances at most, so the channel
   number goes in the entry name */
static char zsg2_state_names[48][21][20];

void zsg2_state_register(void)
{
	int ch, n;
	for (ch = 0; ch < 48; ch++)
	{
		n = 0;
		sprintf(zsg2_state_names[ch][n], "%02d_v", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], chan[ch].v, 16);
		sprintf(zsg2_state_names[ch][n], "%02d_status", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].status, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_cur_pos", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].cur_pos, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_step_ptr", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].step_ptr, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_step", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].step, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_start_pos", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].start_pos, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_end_pos", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].end_pos, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_loop_pos", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].loop_pos, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_page", ch);
		state_save_register_UINT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].page, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_vol", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].vol, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_vol_initial", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].vol_initial, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_vol_target", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].vol_target, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_vol_delta", ch);
		state_save_register_INT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].vol_delta, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_cutoff", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].output_cutoff, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_cutoff_initial", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].output_cutoff_initial, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_cutoff_target", ch);
		state_save_register_UINT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].output_cutoff_target, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_cutoff_delta", ch);
		state_save_register_INT16("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].output_cutoff_delta, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_emphasis", ch);
		state_save_register_INT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].emphasis_filter_state, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_filter", ch);
		state_save_register_INT32("zsg2", 0, zsg2_state_names[ch][n++], &chan[ch].output_filter_state, 1);
		sprintf(zsg2_state_names[ch][n], "%02d_gain", ch);
		state_save_register_UINT8("zsg2", 0, zsg2_state_names[ch][n++], chan[ch].output_gain, 4);
		sprintf(zsg2_state_names[ch][n], "%02d_samples", ch);
		state_save_register_INT16("zsg2", 0, zsg2_state_names[ch][n++], chan[ch].samples, 5);
	}
	state_save_register_UINT16("zsg2", 0, "reg", zsg2_reg, 32);
	state_save_register_UINT32("zsg2", 0, "sample_count", &sample_count, 1);
	state_save_register_UINT32("zsg2", 0, "read_address", &read_address, 1);
}
