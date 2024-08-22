/***************************************************************************

Namco 52XX

This instance of the Fujitsu MB8852 MCU is programmed to act as a sample player.
It is used by just two games: Bosconian and Pole Position.

A0-A15 = address to read from sample ROMs
D0-D7 = data freom sample ROMs
CMD = command from CPU (sample to play, 0 = none)
OUT = sound output

      +------+
 EXTAL|1   42|Vcc
  XTAL|2   41|CMD3
/RESET|3   40|CMD2
  /IRQ|4   39|CMD1
  n.c.|5   38|CMD0
  [2] |6   37|A7
  n.c.|7   36|A6
  [1] |8   35|A5
  OUT0|9   34|A4
  OUT1|10  33|A3
  OUT2|11  32|A2
  OUT3|12  31|A1
    A8|13  30|A0
    A9|14  29|D7
   A10|15  28|D6
   A11|16  27|D5
[3]A12|17  26|D4
[3]A13|18  25|D3
[3]A14|19  24|D2
[3]A15|20  23|D1
   GND|21  22|D0
      +------+

[1] in polepos, GND; in bosco, 4kHz output from a 555 timer
[2] in polepos, +5V; in bosco, GND
[3] in polepos, these are true address lines, in bosco they are chip select lines
    (each one select one of the four ROM chips). Behaviour related to [2]?

TODO:
- the purpose of the 555 timer in bosco is unknown; maybe modulate the output?
Jan 12, 2005.  The 555 is probably an external playback frequency.

***************************************************************************/

#include "driver.h"
#include "filter.h"


static const struct namco_52xx_interface *intf;	/* pointer to our config data */
static UINT8 *rom;			/* pointer to sample ROM */
static int rom_len;
static int stream;			/* the output stream */
static double n52_pb_cycle;	/* playback clock time based on machine sample rate */
static double n52_step;		/* playback clock step based on machine sample rate */
/* n52_pb_cycle is incremented by n52_step every machine-sample.
 * At every integer value of n52_pb_cycle the next 4bit value is used. */
static int n52_start;		/* current effect start position in the ROM */
static int n52_end;			/* current effect end position in the ROM */
static int n52_length;		/* # of 4bit samples in current effect */
static int n52_pos;			/* current 4bit sample of effect */
static struct filter2_context n52_hp_filter;
static struct filter2_context n52_lp_filter;


static void namco_52xx_stream_update_one(int num, INT16 *buffer, int length)
{
	int i, rom_pos, whole_pb_cycles, buf;

	if (n52_start >= n52_end)
	{
		memset(buffer, 0, length * sizeof(INT16));
		return;
	}

	for (i = 0; i < length; i++)
	{
		n52_pb_cycle += n52_step;
		if (n52_pb_cycle >= 1)
		{
			whole_pb_cycles = (int)n52_pb_cycle;
			n52_pos += whole_pb_cycles;
			n52_pb_cycle -= whole_pb_cycles;
		}

		if (n52_pos > n52_length)
		{
			/* sample done */
			memset(&buffer[i], 0, (length - i) * sizeof(INT16));
			i = length;
			namco_52xx_sh_reset();
		}
		else
		{
			/* filter and fill the buffer */
			rom_pos = n52_start + (n52_pos >> 1);
			/* get the 4bit sample from rom and shift to +7/-8 value */
			n52_hp_filter.x0 = (((n52_pos & 1) ? rom[rom_pos] >> 4 : rom[rom_pos]) & 0x0f) - 0x08;
			filter2_step(&n52_hp_filter);
			n52_lp_filter.x0 = n52_hp_filter.y0;
			filter2_step(&n52_lp_filter);
			/* convert 4bit filtered to 16bit allowing room for filter gain */
			buf = (int)(n52_lp_filter.y0 * 0x0fff);
			if (buf > 32767) buf = 32767;
			if (buf < -32768) buf = -32768;
			buffer[i] = buf;
		}
	}
}


void namco_52xx_sh_reset(void)
{
	n52_pb_cycle = n52_start = n52_end = n52_length = n52_pos = 0;

	filter2_reset(&n52_hp_filter);
	filter2_reset(&n52_lp_filter);
}

int namco_52xx_sh_start(const struct MachineSound *msound)
{
	intf = msound->sound_interface;
	rom     = memory_region(intf->region);
	rom_len = memory_region_length(intf->region);

	if (Machine->sample_rate == 0)
		return 1;

	if (intf->play_rate == 0)
	{
		/* If play clock is 0 (grounded) then default to internal clock */
		n52_step = (double)(intf->baseclock) / 384 / Machine->sample_rate;
	}
	else
	{
		n52_step = intf->play_rate / Machine->sample_rate;
	}
	filter2_setup(FILTER_HIGHPASS, intf->hp_filt_fc, Q_TO_DAMP(intf->hp_filt_q), 1, &n52_hp_filter);
	filter2_setup(FILTER_LOWPASS,  intf->lp_filt_fc, Q_TO_DAMP(intf->lp_filt_q), intf->filt_gain, &n52_lp_filter);


	stream = stream_init(sound_name(msound), intf->mixing_level, Machine->sample_rate, 0, namco_52xx_stream_update_one);

	/* handle failure */
	if (stream == -1)
		osd_die("Namco 52xx - Stream init returned an error\n");

	namco_52xx_sh_reset();

	return 0;
}


void namcoio_52XX_write(int data)
{
	data &= 0x0f;

	if (data != 0)
	{
		stream_update(stream, 0);

		n52_start = rom[data-1] + (rom[data-1+0x10] << 8);
		n52_end = rom[data] + (rom[data+0x10] << 8);

		if (n52_end >= rom_len)
			n52_end = rom_len;

		n52_length = (n52_end - n52_start) * 2;
		n52_pos = 0;
		n52_pb_cycle= 0;
	}
}


void namco_52xx_sh_stop(void)
{
}
