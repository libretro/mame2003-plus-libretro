/*
    license:BSD-3-Clause
    copyright-holders:R. Belmont, superctr

    c352.c - Namco C352 custom PCM chip emulation
    v2.0
    By R. Belmont
    Rewritten and improved by superctr
    Additional code by cync and the hoot development team

    Thanks to Cap of VivaNonno for info and The_Author for preliminary reverse-engineering

    Chip specs:
    32 voices
    Supports 8-bit linear and 8-bit muLaw samples
    Output: digital, 16 bit, 4 channels
    Output sample rate is the input clock / (288 * 2).

    mame2003-plus: backported from current MAME (c352.cpp) to the 0.78
    sound interface.  Only the front (L/R) outputs are mixed, as on
    System 12 where the second DAC is not populated.
*/

#include <stddef.h>
#include <string.h>
#include "driver.h"
#include "state.h"
#include "c352.h"

enum
{
	C352_FLG_BUSY       = 0x8000,   /* channel is busy */
	C352_FLG_KEYON      = 0x4000,   /* Keyon */
	C352_FLG_KEYOFF     = 0x2000,   /* Keyoff */
	C352_FLG_LOOPTRG    = 0x1000,   /* Loop Trigger */
	C352_FLG_LOOPHIST   = 0x0800,   /* Loop History */
	C352_FLG_FM         = 0x0400,   /* Frequency Modulation */
	C352_FLG_PHASERL    = 0x0200,   /* Rear Left invert phase 180 degrees */
	C352_FLG_PHASEFL    = 0x0100,   /* Front Left invert phase 180 degrees */
	C352_FLG_PHASEFR    = 0x0080,   /* invert phase 180 degrees (e.g. flip sign of sample) */
	C352_FLG_LDIR       = 0x0040,   /* loop direction */
	C352_FLG_LINK       = 0x0020,   /* "long-format" sample (can't loop, not sure what else it means) */
	C352_FLG_NOISE      = 0x0010,   /* play noise instead of sample */
	C352_FLG_MULAW      = 0x0008,   /* sample is mulaw instead of linear 8-bit PCM */
	C352_FLG_FILTER     = 0x0004,   /* don't apply filter */
	C352_FLG_REVLOOP    = 0x0003,   /* loop backwards */
	C352_FLG_LOOP       = 0x0002,   /* loop forward */
	C352_FLG_REVERSE    = 0x0001    /* play sample backwards */
};

/* the 8 registers of each voice, in chip order */
enum
{
	C352_REG_VOL_F = 0,
	C352_REG_VOL_R,
	C352_REG_FREQ,
	C352_REG_FLAGS,
	C352_REG_WAVE_BANK,
	C352_REG_WAVE_START,
	C352_REG_WAVE_END,
	C352_REG_WAVE_LOOP
};

typedef struct
{
	UINT32 pos;
	UINT32 counter;

	INT16 sample;
	INT16 last_sample;

	UINT16 regs[8];		/* vol_f, vol_r, freq, flags, wave_bank, wave_start, wave_end, wave_loop */
	UINT8 curr_vol[4];
} c352_voice_t;

#define V_VOL_F(v)      ((v)->regs[C352_REG_VOL_F])
#define V_VOL_R(v)      ((v)->regs[C352_REG_VOL_R])
#define V_FREQ(v)       ((v)->regs[C352_REG_FREQ])
#define V_FLAGS(v)      ((v)->regs[C352_REG_FLAGS])
#define V_WAVE_BANK(v)  ((v)->regs[C352_REG_WAVE_BANK])
#define V_WAVE_START(v) ((v)->regs[C352_REG_WAVE_START])
#define V_WAVE_END(v)   ((v)->regs[C352_REG_WAVE_END])
#define V_WAVE_LOOP(v)  ((v)->regs[C352_REG_WAVE_LOOP])

static struct
{
	int stream;
	const UINT8 *rom;
	UINT32 rom_mask;
	UINT32 rom_length;

	c352_voice_t v[32];
	INT16 mulawtab[256];

	UINT16 random;
	UINT16 control;
} c352;

/* 24-bit address, mirrored on a power of two; nothing past the region */
static INLINE UINT8 c352_read_byte(UINT32 address)
{
	address &= c352.rom_mask;
	if (address >= c352.rom_length)
		return 0;
	return c352.rom[address];
}

static void c352_fetch_sample(c352_voice_t *v)
{
	v->last_sample = v->sample;

	if (V_FLAGS(v) & C352_FLG_NOISE)
	{
		c352.random = (c352.random >> 1) ^ ((-(c352.random & 1)) & 0xfff6);
		v->sample = c352.random;
	}
	else
	{
		INT8 s = (INT8)c352_read_byte(v->pos);
		UINT16 pos;

		if (V_FLAGS(v) & C352_FLG_MULAW)
			v->sample = c352.mulawtab[s & 0xff];
		else
			v->sample = s << 8;

		pos = v->pos & 0xffff;

		if ((V_FLAGS(v) & C352_FLG_LOOP) && (V_FLAGS(v) & C352_FLG_REVERSE))
		{
			/* backwards>forwards */
			if ((V_FLAGS(v) & C352_FLG_LDIR) && pos == V_WAVE_LOOP(v))
				V_FLAGS(v) &= ~C352_FLG_LDIR;
			/* forwards>backwards */
			else if (!(V_FLAGS(v) & C352_FLG_LDIR) && pos == V_WAVE_END(v))
				V_FLAGS(v) |= C352_FLG_LDIR;

			v->pos += (V_FLAGS(v) & C352_FLG_LDIR) ? -1 : 1;
		}
		else if (pos == V_WAVE_END(v))
		{
			if ((V_FLAGS(v) & C352_FLG_LINK) && (V_FLAGS(v) & C352_FLG_LOOP))
			{
				v->pos = (V_WAVE_START(v) << 16) | V_WAVE_LOOP(v);
				V_FLAGS(v) |= C352_FLG_LOOPHIST;
			}
			else if (V_FLAGS(v) & C352_FLG_LOOP)
			{
				v->pos = (v->pos & 0xff0000) | V_WAVE_LOOP(v);
				V_FLAGS(v) |= C352_FLG_LOOPHIST;
			}
			else
			{
				V_FLAGS(v) |= C352_FLG_KEYOFF;
				V_FLAGS(v) &= ~C352_FLG_BUSY;
				v->sample = 0;
			}
		}
		else
		{
			v->pos += (V_FLAGS(v) & C352_FLG_REVERSE) ? -1 : 1;
		}
	}
}

static INLINE void c352_ramp_volume(c352_voice_t *v, int ch, UINT8 val)
{
	INT16 vol_delta = v->curr_vol[ch] - val;
	if (vol_delta != 0)
		v->curr_vol[ch] += (vol_delta > 0) ? -1 : 1;
}

static INLINE INT16 c352_clamp(int val)
{
	if (val > 32767) return 32767;
	if (val < -32768) return -32768;
	return val;
}

static void c352_update(int param, INT16 **buffer, int length)
{
	INT16 *bufl = buffer[0];
	INT16 *bufr = buffer[1];
	int i, j;

	for (i = 0; i < length; i++)
	{
		int out[2] = { 0, 0 };

		for (j = 0; j < 32; j++)
		{
			c352_voice_t *v = &c352.v[j];
			INT16 s = 0;

			if (V_FLAGS(v) & C352_FLG_BUSY)
			{
				INT32 next_counter = v->counter + V_FREQ(v);

				if (next_counter & 0x10000)
					c352_fetch_sample(v);

				if ((next_counter ^ v->counter) & 0x18000)
				{
					c352_ramp_volume(v, 0, V_VOL_F(v) >> 8);
					c352_ramp_volume(v, 1, V_VOL_F(v) & 0xff);
					c352_ramp_volume(v, 2, V_VOL_R(v) >> 8);
					c352_ramp_volume(v, 3, V_VOL_R(v) & 0xff);
				}

				v->counter = next_counter & 0xffff;

				s = v->sample;

				/* Interpolate samples */
				if ((V_FLAGS(v) & C352_FLG_FILTER) == 0)
					/* product in 64 bits: same 16-bit result, no signed overflow */
					s = v->last_sample + (INT32)(((INT64)v->counter * (v->sample - v->last_sample)) >> 16);
			}

			/* Left */
			out[0] += (((V_FLAGS(v) & C352_FLG_PHASEFL) ? -s : s) * v->curr_vol[0]) >> 8;
			/* Right */
			out[1] += (((V_FLAGS(v) & C352_FLG_PHASEFR) ? -s : s) * v->curr_vol[1]) >> 8;
		}

		bufl[i] = c352_clamp(out[0] >> 3);
		bufr[i] = c352_clamp(out[1] >> 3);
	}
}

static UINT16 c352_reg_read(offs_t offset)
{
	if (offset < 0x100)
		return c352.v[offset / 8].regs[offset % 8];
	else if (offset == 0x200)
		return c352.control;
	return 0;
}

READ16_HANDLER( c352_0_r )
{
	stream_update(c352.stream, 0);
	return c352_reg_read(offset);
}

WRITE16_HANDLER( c352_0_w )
{
	int i;

	stream_update(c352.stream, 0);

	if (offset < 0x100)
	{
		UINT16 newval = c352_reg_read(offset);
		COMBINE_DATA(&newval);
		c352.v[offset / 8].regs[offset % 8] = newval;
	}
	else if (offset == 0x200)
	{
		COMBINE_DATA(&c352.control);
	}
	else if (offset == 0x202) /* execute keyons/keyoffs */
	{
		if (mem_mask != 0) /* 16 bit only */
			return;

		for (i = 0; i < 32; i++)
		{
			c352_voice_t *v = &c352.v[i];

			if (V_FLAGS(v) & C352_FLG_KEYON)
			{
				v->pos = (V_WAVE_BANK(v) << 16) | V_WAVE_START(v);

				v->sample = 0;
				v->last_sample = 0;
				v->counter = 0xffff;

				V_FLAGS(v) |= C352_FLG_BUSY;
				V_FLAGS(v) &= ~(C352_FLG_KEYON | C352_FLG_LOOPHIST);

				v->curr_vol[0] = v->curr_vol[1] = 0;
				v->curr_vol[2] = v->curr_vol[3] = 0;
			}
			if (V_FLAGS(v) & C352_FLG_KEYOFF)
			{
				V_FLAGS(v) &= ~(C352_FLG_BUSY | C352_FLG_KEYOFF);
				v->counter = 0xffff;
			}
		}
	}
}

int C352_sh_start(const struct MachineSound *msound)
{
	const struct C352interface *intf = msound->sound_interface;
	const char *names[2] = { "C352 Left", "C352 Right" };
	int vol[2];
	int i, j, length;
	char buf[32];

	memset(&c352, 0, sizeof(c352));

	vol[0] = MIXER(intf->mixing_level, MIXER_PAN_LEFT);
	vol[1] = MIXER(intf->mixing_level, MIXER_PAN_RIGHT);

	c352.rom = memory_region(intf->region);
	length = c352.rom ? memory_region_length(intf->region) : 0;
	if (c352.rom == NULL)
	{
		static const UINT8 empty = 0;
		c352.rom = &empty;
	}
	c352.rom_length = (UINT32)length;
	/* the chip addresses 16MB; mirror smaller ROMs on a power of two
	   (reads past the end of the region give 0) */
	c352.rom_mask = 1;
	while (c352.rom_mask < (UINT32)length && c352.rom_mask < 0x1000000) c352.rom_mask <<= 1;
	c352.rom_mask -= 1;
	if (c352.rom_mask > 0xffffff) c352.rom_mask = 0xffffff;

	c352.stream = stream_init_multi(2, names, vol, intf->clock / intf->divider, 0, c352_update);

	/* generate mulaw table (Output similar to namco's VC emulator) */
	j = 0;
	for (i = 0; i < 128; i++)
	{
		c352.mulawtab[i] = j << 5;
		if (i < 16)
			j += 1;
		else if (i < 24)
			j += 2;
		else if (i < 48)
			j += 4;
		else if (i < 100)
			j += 8;
		else
			j += 16;
	}
	for (i = 0; i < 128; i++)
		c352.mulawtab[i + 128] = (~c352.mulawtab[i]) & 0xffe0;

	/* init noise generator */
	c352.random = 0x1234;
	c352.control = 0;

	for (i = 0; i < 32; i++)
	{
		sprintf(buf, "C352 voice %02d", i);
		state_save_register_UINT32(buf, 0, "pos",         &c352.v[i].pos, 1);
		state_save_register_UINT32(buf, 0, "counter",     &c352.v[i].counter, 1);
		state_save_register_INT16 (buf, 0, "sample",      &c352.v[i].sample, 1);
		state_save_register_INT16 (buf, 0, "last_sample", &c352.v[i].last_sample, 1);
		state_save_register_UINT16(buf, 0, "regs",        c352.v[i].regs, 8);
		state_save_register_UINT8 (buf, 0, "curr_vol",    c352.v[i].curr_vol, 4);
	}
	state_save_register_UINT16("C352", 0, "random",  &c352.random, 1);
	state_save_register_UINT16("C352", 0, "control", &c352.control, 1);

	return 0;
}

void C352_sh_stop(void)
{
}
