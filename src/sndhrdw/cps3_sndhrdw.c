/***************************************************************************

    Capcom CPS-3 sound hardware

    16 voices playing signed 8-bit samples from the graphics and sample
    flash SIMMs, at the board clock / 3 / 384. A C port of current MAME's
    cps3_a.cpp (Philip Bennett).

    Voice registers (32 bits each, 8 per voice):
    1  start address, halves swapped
    2  bit 0: loop enable
    3  high: frequency, low: loop address low
    4  low: loop address high
    5  end address, halves swapped (6 holds the same)
    7  high: right volume, low: left volume (signed)

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "includes/cps3.h"

#define CPS3_SOUND_CLOCK	(42954545 / 3)

struct cps3_voice
{
	UINT32 regs[8];
	UINT32 pos;
	UINT32 frac;
};

static struct cps3_voice voice[16];
static UINT16 key;
static int stream;

static const UINT8 *sample_base;
static UINT32 sample_mask;

/* the samples are read as bytes from the SIMMs' byte stream; this core keeps
   that stream in bus order, the hardware reads it with the bytes of each
   word swapped */
void cps3_sound_set_base(const UINT8 *base, UINT32 length)
{
	sample_base = base;
	sample_mask = length - 1;
}

static void cps3_stream_update(int param, INT16 **buffer, int length)
{
	INT16 *left = buffer[0];
	INT16 *right = buffer[1];
	INT32 *mix_l, *mix_r;
	static INT32 *mixbuf;
	static int mixlen;
	int i, j;

	if (length > mixlen)
	{
		free(mixbuf);
		mixbuf = malloc(length * 2 * sizeof(INT32));
		mixlen = mixbuf ? length : 0;
		if (!mixbuf)
		{
			memset(left, 0, length * sizeof(INT16));
			memset(right, 0, length * sizeof(INT16));
			return;
		}
	}
	mix_l = mixbuf;
	mix_r = mixbuf + length;
	memset(mixbuf, 0, length * 2 * sizeof(INT32));

	if (sample_base)
	{
		for (i = 0; i < 16; i++)
		{
			struct cps3_voice *v = &voice[i];
			UINT32 start, end, loop, step, pos, frac;
			int loop_enable;
			INT32 vol_l, vol_r;

			if (!(key & (1 << i)))
				continue;

			start = ((v->regs[1] >> 16) & 0x0000ffff) | ((v->regs[1] << 16) & 0xffff0000);
			end   = ((v->regs[5] >> 16) & 0x0000ffff) | ((v->regs[5] << 16) & 0xffff0000);
			loop  = (v->regs[3] & 0x0000ffff) | ((v->regs[4] << 16) & 0xffff0000);
			loop_enable = v->regs[2] & 1;
			step  = (v->regs[3] >> 16) & 0xffff;

			vol_l = (INT16)(v->regs[7] & 0xffff);
			vol_r = (INT16)((v->regs[7] >> 16) & 0xffff);

			pos = v->pos;
			frac = v->frac;

			/* the first 4 MB of the SIMM address space is not samples */
			start -= 0x400000;
			end -= 0x400000;
			loop -= 0x400000;

			for (j = 0; j < length; j++)
			{
				INT32 sample;

				pos += (frac >> 12);
				frac &= 0xfff;

				if ((start + pos) >= end)
				{
					if (loop_enable)
						pos = (pos + loop) - end;
					else
						break;	/* sample end, the key stays on */
				}

				sample = (INT8)sample_base[((start + pos) ^ 1) & sample_mask];
				frac += step;

				/* MAME routes the left volume to its right speaker */
				mix_l[j] += sample * vol_r;
				mix_r[j] += sample * vol_l;
			}

			v->pos = pos;
			v->frac = frac;
		}
	}

	for (j = 0; j < length; j++)
	{
		INT32 l = mix_l[j] >> 8;
		INT32 r = mix_r[j] >> 8;
		left[j] = (l < -32768) ? -32768 : (l > 32767) ? 32767 : l;
		right[j] = (r < -32768) ? -32768 : (r > 32767) ? 32767 : r;
	}
}

WRITE32_HANDLER( cps3_sound_w )
{
	stream_update(stream, 0);

	if (offset < 0x80)
	{
		COMBINE_DATA(&voice[offset / 8].regs[offset & 7]);
	}
	else if (offset == 0x80)
	{
		UINT16 newkey = data >> 16;
		int i;

		for (i = 0; i < 16; i++)
		{
			/* key off -> key on */
			if ((newkey & (1 << i)) && !(key & (1 << i)))
			{
				voice[i].frac = 0;
				voice[i].pos = 0;
			}
		}
		key = newkey;
	}
}

READ32_HANDLER( cps3_sound_r )
{
	stream_update(stream, 0);

	if (offset < 0x80)
		return voice[offset / 8].regs[offset & 7] & ~mem_mask;
	else if (offset == 0x80)
		return key << 16;
	return 0;
}

static int cps3_sh_start(const struct MachineSound *msound)
{
	const char *names[] = { "CPS-3 left", "CPS-3 right" };
	int vols[] = { MIXER(100, MIXER_PAN_LEFT), MIXER(100, MIXER_PAN_RIGHT) };
	int i;

	memset(voice, 0, sizeof(voice));
	key = 0;
	stream = stream_init_multi(2, names, vols, CPS3_SOUND_CLOCK / 384, 0, cps3_stream_update);

	state_save_register_UINT16("cps3snd", 0, "key", &key, 1);
	for (i = 0; i < 16; i++)
	{
		state_save_register_UINT32("cps3snd", i, "regs", voice[i].regs, 8);
		state_save_register_UINT32("cps3snd", i, "pos", &voice[i].pos, 1);
		state_save_register_UINT32("cps3snd", i, "frac", &voice[i].frac, 1);
	}
	return 0;
}

struct CustomSound_interface cps3_custom_interface =
{
	cps3_sh_start, NULL, NULL
};
