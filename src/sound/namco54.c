/***************************************************************************

Namco 54XX

This instance of the Fujitsu MB8852 MCU is programmed to act as a noise
generator (mostly for explosions).

CMD = command from CPU
OUTn = sound outputs (3 channels)

The chip will read the command when the /IRQ is pulled down.

      +------+
 EXTAL|1   28|Vcc
  XTAL|2   27|CMD7
/RESET|3   26|CMD6
OUT0.0|4   25|CMD5
OUT0.1|5   24|CMD4
OUT0.2|6   23|/IRQ
OUT0.3|7   22|n.c.
OUT1.0|8   21|n.c.
OUT1.1|9   20|OUT2.3
OUT1.2|10  19|OUT2.2
OUT1.3|11  18|OUT2.1
  CMD0|12  17|OUT2.0
  CMD1|13  16|CMD3
   GND|14  15|CMD2
      +------+

The command format is very simple:

00: nop
10: play sound type A
20: play sound type B
30: set parameters (type A) (followed by 4 bytes)
40: set parameters (type B) (followed by 4 bytes)
50: play sound type C, mode A
60: set parameters (type C) (followed by 5 bytes)
7x: play sound type C, mode B (pole position)
80-ff: nop


Behaviour:
----------
If the chip is instructed to play the sound while it is still playing
- it will immediately restart playing it from the beginning, i.e. retrigger
when the same play command is send more than once, fast
(for example 10,10 or 20,20)

If the chip is instructed to set the parameters with any of the commands: 30,
40 or 60, while it is playing any sounds then the sound reproduction will be
hold on until the last of the parameters is written. Then the chip continues
the sound reproduction as if nothing happend.

The parameters changed during sound reproduction don't take immediate effect.
For example, if the volume of the sustain parameter was changed while the chip
is playing the sustain part then the sustain volume will not change.
However, if the volume of the release part is changed while the chip is playing
the sustain or the pause parts then the chip will play the release part using
the newly set volume.

The noise generator output used by the type B is delayed by ~209 clock cycles
if compared to the noise generator used by the type A.


TODO:
- The type C RNG configuration for the noise generator and a frequency
  modulation formula [todo 2: (both are approximated for now)]


***************************************************************************/

#include "driver.h"
#include "filter.h"


static int fetch;
static int fetchmode;
static int type_A_active;
static int type_B_active;
static int type_C_active;

static int type_A_state;	/* current envelope state */
static int type_B_state;	/* current envelope state */
static int type_C_state;	/* current envelope state */


static int type_A_pos;		/* type A 'position' in time within the envelope state */
static int type_A_add;		/* type A 'position' delta (step per one sample at the target frequency)*/
static int type_A_curr_vol;	/* type A current volume */
static int type_A_rel_pos;	/* type A index in release volume table */


static int type_B_pos;		/* type B 'position' in time */
static int type_B_add;		/* type B 'position' delta (step per one sample at the target frequency)*/
static int type_B_curr_vol;	/* type B current volume */
static int type_B_rel_pos;	/* type B index in release volume table */




/* noise generators */
static int RNG;			/* shift register */
static int RNG_p;		/* shift register 'position' in time */
static int RNG_f_0;		/* shift register 'speed' per one sample at the target frequency at the zero level */
static int RNG_f_1;		/* shift register 'speed' per one sample at the target frequency at the one level */
static int RNG_delay;		/* delay between the type A output change and the following type B output change */
static int out_prev;		/* work variable (previous noise output state) */
static int noise_out_A;		/* noise output used in type A */
static int noise_out_B;		/* noise output used in type B */
static int noise_B_will_change;	/* in order to implement the delay between the RNG output in type A and the type B */
static int RNG_p_delay_B;	/* in order to implement the delay between the RNG output in type A and the type B */


/* parameters */
static UINT8 type_A_par[4];
static UINT8 type_B_par[4];
static UINT8 type_C_par[5];


#define ENV_OFF     0
#define ENV_SUSTAIN 1
#define ENV_PAUSE   2
#define ENV_RELEASE 3



#define PRECISION_SH	26	/* 6.26 fixed point for all the 'position' calculations */
#define PRECISION_MASK	((1<<PRECISION_SH)-1)


static const struct namco_54xx_interface *intf;
static double chip_clock;
static double chip_sampfreq;

static int stream;


#define NAMCO54xx_NUMBUF 3


/*

TYPE A
======

The type A sound is based on the output of the RNG implemented as a 15-bits long
shift register in a following configuration: 1 + x^4 + x^15.
The binary RNG output is then multiplied by an envelope that is consisted of three 'parts': sustain, pause and release.
The envelope has its initial volumes and speeds/lengths.


The type A parameters description:
----------------------------------
0 - length of the 1st part (sustain)
1 - pause length (a zero-level output part between the sustain and the release parts)
2 - release part speed
3 - volumes


(general note: nibbles of the parameters need to be swapped for more natural reading.)


parameter 0: (values on the right represent the number of samples the sustain part takes at sampfreq=44100, chip clock = 1.8432 MHz)
08 - 30389
04 - 15262
02 - 7541
01 - 3705
80 - 1833
40 - 903
20 - 430
10 - 165
00 - 24 (1003)

parameter 1: (values on the right represent the number of samples that the pause takes at sampfreq=44100, chip clock = 1.8432 MHz)
08 - 30597
04 - 15469
02 - 7940
01 - 4045
80 - 2167
40 - 1176
20 - 788
10 - 549
00 - no pause at all !!!


parameter 2: release speed (no of samples per one level) (number of chip clock cycles)
08 - 30630 (1280209)
04 - 15443 (645454)
02 - 7835  (327471)
01 - 4029  (168395)
80 - 2139  (89401)
40 - 1183  (49444)
20 - 717   (29967)
10 - 477   (19936)
00 - no release at all !!!

parameter 3: volumes for the sustain and the release parts
SR - S is the volume of the sustain part; R is an initial volume of the release part (see: release_table)

*/

static int cycles_per_bit[8]=
{
19936,
29967,
49444,
89401,
168395,
327471,
645454,
1280209
};

static INT32 speeds[257];	/* calculated at init time */

static UINT8 release_table[16][8]=
{
{ 0, 0,0,0,0,0,0,0},
{ 1, 0,0,0,0,0,0,0},
{ 2, 1,0,0,0,0,0,0},
{ 3, 2,1,0,0,0,0,0},
{ 4, 3,2,1,0,0,0,0},
{ 5, 3,2,1,0,0,0,0},
{ 6, 4,3,2,1,0,0,0},
{ 7, 5,3,2,1,0,0,0},
{ 8, 6,4,3,2,1,0,0},
{ 9, 6,4,3,2,1,0,0},
{10, 7,5,3,2,1,0,0},
{11, 7,6,4,3,2,1,0},
{12, 9,6,4,3,2,1,0},
{13, 9,6,4,3,2,1,0},
{14,10,7,5,3,2,1,0},
{15,11,8,6,4,3,2,1}
};

static struct filter2_context filter54[3];


void namco_54xx_sh_reset(void)
{
	int loop;
	for (loop = 0; loop < 3; loop++) filter2_reset(&filter54[loop]);

	fetch = 0;
	fetchmode = 0;

	type_A_active = 0; /* 0 inactive; 1 active */
	type_B_active = 0; /* 0 inactive; 1 active */
	type_C_active = 0; /* 0 inactive; 1 active in mode A; 2 active in mode B */


	type_A_state = ENV_OFF;
	type_B_state = ENV_OFF;
	type_C_state = ENV_OFF;


	/* RNG_f_x are initialized in sh_start */
	RNG_p = 0;
	RNG = 0xfff;
	noise_out_A = RNG&1;
	noise_out_B = 0;
	noise_B_will_change = 0;
	out_prev = 0;

//wonder what are the default params after the reset ?

}


static void start_A(void)
{
	int v;

	type_A_active = 1; /* 0 inactive; 1 active */
	type_A_state = ENV_SUSTAIN;
	type_A_pos = 0;

	v = type_A_par[0];	/* sustain length */
	v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
	if (v)
		type_A_add = speeds[v];
	else
		type_A_add = speeds[256];  /* zero sustain actually takes some time */
	type_A_curr_vol = (type_A_par[3]>>4) & 0x0f;	/* sustain volume */
}

static void start_B(void)
{
	int v;

	type_B_active = 1; /* 0 inactive; 1 active */
	type_B_state = ENV_SUSTAIN;
	type_B_pos = 0;

	v = type_B_par[0];
	v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
	if (v)
		type_B_add = speeds[v];
	else
		type_B_add = speeds[256];  /* zero sustain actually takes some time */
	type_B_curr_vol = (type_B_par[3]>>4) & 0x0f;	/* sustain volume */
}


/* call one time per sample at the _target_ frequency */
static void noise_step(void)
{
int b04, b15;
int i;

	/* two different lenghts depending on the output level */
	if (noise_out_A)
		RNG_p += RNG_f_1;
	else
		RNG_p += RNG_f_0;

	i = (RNG_p>>PRECISION_SH);		/* number of the events (shifts of the shift register) */
	RNG_p &= PRECISION_MASK;

	if (noise_B_will_change == 0)
		RNG_p_delay_B = RNG_p;		/* preinitalize delay counter */

	while (i)
	{
		b15 = (RNG>>0) & 1;
		b04 = (RNG>>11)& 1;
		noise_out_A = RNG&1;

		if ( (out_prev != noise_out_A) && (noise_out_A == 0) ) /* OK = 0 for type B */
		{
			/* matters if it changes odd number of times, even number of changes is equal to no change at all*/
			noise_B_will_change ^= 1;
		}
		out_prev = noise_out_A;

		RNG = (RNG>>1) | ((b04^b15)<<14);

		i--;
	}
	if (noise_B_will_change)
	{
		RNG_p_delay_B += RNG_delay;
		i = (RNG_p_delay_B>>PRECISION_SH);/* number of the events (shifts of the shift register) */
		if (i)
		{
			noise_out_B ^= 1;
			noise_B_will_change = 0;
		}
	}
}


static INT16 calc_A(void)
{
//if (type_A_active)
//	logerror("calc a: env_state=%2i vol=%2i noise=%2i\n", type_A_state, type_A_curr_vol, noise_out_A);

	switch (type_A_state)
	{
	case ENV_OFF:
	default:
		return 0;
		break;

	case ENV_SUSTAIN:
		return (noise_out_A * type_A_curr_vol);
		break;

	case ENV_PAUSE:
		return 0;
		break;

	case ENV_RELEASE:
		return (noise_out_A * release_table[ type_A_curr_vol ] [ type_A_rel_pos ] );
		break;
	}
}
static INT16 calc_B(void)
{
	switch (type_B_state)
	{
	case ENV_OFF:
	default:
		return 0;
		break;

	case ENV_SUSTAIN:
		return (noise_out_B * type_B_curr_vol);
		break;

	case ENV_PAUSE:
		return 0;
		break;

	case ENV_RELEASE:
		return (noise_out_B * release_table[ type_B_curr_vol ] [ type_B_rel_pos ] );
		break;
	}
}

static INT16 calc_C(void)
{

	return 0;
}




static void advance(void)
{
int i,v;

//type A
	type_A_pos += type_A_add;
	i = (type_A_pos >> PRECISION_SH);
	type_A_pos &= PRECISION_MASK;

	if (i)
	{	//switch to the next envelope state

		//logerror("type A switch, from mode=%2i\n",type_A_state);

		switch(type_A_state)
		{
		case ENV_OFF:
		default:
			break;

		case ENV_SUSTAIN:
			type_A_state = ENV_PAUSE;
			type_A_curr_vol = 0;
			v = type_A_par[1];	/* pause length */
			v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
			type_A_add = speeds[v];
			break;

		case ENV_PAUSE:
			type_A_state = ENV_RELEASE;
			type_A_curr_vol = (type_A_par[3]&0x0f);
			type_A_rel_pos = 0;
			v = type_A_par[2];	/* release length */
			v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
			type_A_add = speeds[v];
			break;

		case ENV_RELEASE:
			if (type_A_rel_pos<7)
			{
				v = type_A_par[2];	/* release length */
				v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
				type_A_add = speeds[v];
				type_A_rel_pos++;
			}
			else
			{
				type_A_state = ENV_OFF;
			}
			break;
		}
	}


//type B
	type_B_pos += type_B_add;
	i = (type_B_pos >> PRECISION_SH);
	type_B_pos &= PRECISION_MASK;

	if (i)
	{	//switch to the next envelope state

		//logerror("type B switch, from mode=%2i\n",type_B_state);

		switch(type_B_state)
		{
		case ENV_OFF:
		default:
			break;

		case ENV_SUSTAIN:
			type_B_state = ENV_PAUSE;
			type_B_curr_vol = 0;
			v = type_B_par[1];	/* pause length */
			v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
			type_B_add = speeds[v];
			break;

		case ENV_PAUSE:
			type_B_state = ENV_RELEASE;
			type_B_curr_vol = (type_B_par[3]&0x0f);
			type_B_rel_pos = 0;
			v = type_B_par[2];	/* release length */
			v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
			type_B_add = speeds[v];
			break;

		case ENV_RELEASE:
			if (type_B_rel_pos<7)
			{
				v = type_B_par[2];	/* release length */
				v = ((v&0xf0)>>4) | ((v&0x0f)<<4); /*swap the nibbles */
				type_B_add = speeds[v];
				type_B_rel_pos++;
			}
			else
			{
				type_B_state = ENV_OFF;
			}
			break;
		}
	}




//TODO type C


	noise_step();

}



void NAMCO54xxUpdateOne(int num, INT16 **buffers, int length)
{
	int i, loop;
	INT16 out1, out2, out3;
	INT16 *buf1, *buf2, *buf3;

	buf1 = buffers[0];
	buf2 = buffers[1];
	buf3 = buffers[2];

	for (i = 0; i < length; i++)
	{
		//if (fetch) /* no sound is produced while waiting for the parameters */

		out1 = calc_A();	/* pins 4-7 */
		out2 = calc_B();	/* pins 8-11 */
		out3 = calc_C();	/* pins 17-20 */

		/* Convert the binary value to a voltage and filter it. */
		/* I am assuming a 4V output when a bit is high. */
		filter54[0].x0 = 4.0/15 * out1 - 2;
		filter54[1].x0 = 4.0/15 * out2 - 2;
		filter54[2].x0 = 4.0/15 * out3 - 2;
		for (loop = 0; loop < 3; loop++)
		{
			filter2_step(&filter54[loop]);
			/* The op-amp powered @ 5V will clip to 0V & 3.5V.
			 * Adjusted to vRef of 2V, we will clip as follows: */
			if (filter54[loop].y0 > 1.5) filter54[loop].y0 = 1.5;
			if (filter54[loop].y0 < -2) filter54[loop].y0 = -2;
		}

		(buf1)[i] = filter54[0].y0 * (32768/2);
		(buf2)[i] = filter54[1].y0 * (32768/2);
		(buf3)[i] = filter54[2].y0 * (32768/2);

		advance();
	}
}


void namcoio_54XX_write(int data)
{
logerror("%04x: custom 54XX write %02x\n",activecpu_get_pc(),data);

	stream_update(stream, 0);

	if (fetch)
	{
		switch (fetchmode)
		{
			default:
			case 1:
				type_A_par[4-(fetch--)] = data;
				break;

			case 2:
				type_B_par[4-(fetch--)] = data;
				break;

			case 3:
				type_C_par[5-(fetch--)] = data;
				break;
		}
	}
	else
	{
		switch (data & 0xf0)
		{
		case 0x00:	/* nop */
		default:
			break;

		case 0x10:	/* type A start, output sound on pins 4-7 only */
			start_A();
			break;

		case 0x20:	/* type B, output sound on pins 8-11 only */
			start_B();
			break;

		case 0x30:	/* type A parameters input mode */
			fetch = 4;
			fetchmode = 1;
			break;

		case 0x40:	/* type B parameters input mode */
			fetch = 4;
			fetchmode = 2;
			break;

		case 0x50:	/* type C, mode A, output sound on pins 17-20 only */
			if (memcmp(type_C_par,"\x08\x04\x21\x00\xf1",5) == 0)
				// bosco
				sample_start(0, 0, 0);
			break;

		case 0x60:	/* type C parameters input mode */
			fetch = 5;
			fetchmode = 3;
			break;

		case 0x70:	/* type C, mode B, output sound on pins 17-20 only */
				// polepos
				/* 0x7n = Screech sound. n = volume (if 0 then no sound) */
				/* followed by 0x60 command */
#if 1
			if (( data & 0x0f ) == 0)
			{
				if (sample_playing(0))
					sample_stop(0);
			}
			else
			{
				int freq = (int)( ( 44100.0f / 10.0f ) * (float)(data & 0x0f) ); /* this is wrong, it's a volume and not a freq */

				if (!sample_playing(0))
					sample_start(0, 0, 1);
				sample_set_freq(0, freq);
			}
#endif
			break;
		}
	}
}




int namco_54xx_sh_start(const struct MachineSound *msound)
{
	char buf[NAMCO54xx_NUMBUF][40];
	const char *name[NAMCO54xx_NUMBUF];
	int vol[NAMCO54xx_NUMBUF];

	double scaler, c_value, r_in, r_min;
	int i;

	intf = msound->sound_interface;

	/* setup the filters */
	r_min = intf->r4[0];
	for (i = 0; i < 3; i++)
	{
		r_in = intf->r1[i] + ( 1.0 / ( 1.0/4700 + 1.0/10000 + 1.0/22000 + 1.0/47000));
		filter_opamp_m_bandpass_setup(r_in, intf->r2[i], intf->r3[i], intf->c1[i], intf->c2[i],
										&filter54[i]);
		if (intf->r4[i] < r_min) r_min = intf->r4[i];
	}

	/* setup relative gains */
	for (i = 0; i < 3; i++)
	{
		scaler = r_min / intf->r4[i];
		filter54[i].b0 *= scaler;
	    filter54[i].b1 *= scaler;
	    filter54[i].b2 *= scaler;
	}

	chip_clock    = intf->baseclock;
	chip_sampfreq = Machine->sample_rate;

	if( chip_sampfreq == 0 ) chip_sampfreq = 1000;	/* kludge to prevent nasty crashes */

	for (i = 0; i < NAMCO54xx_NUMBUF; i++)
	{
		vol[i] = intf->mixing_level;
		name[i] = buf[i];
		sprintf(buf[i],"%s Ch %d",sound_name(msound),i);
	}

	stream = stream_init_multi(NAMCO54xx_NUMBUF, name, vol, chip_sampfreq, 0, NAMCO54xxUpdateOne);

	logerror("Namco 54xx clock=%f sample rate=%f\n", chip_clock, chip_sampfreq);

	/* calculate emulation tables */
	scaler = chip_clock / chip_sampfreq;

/* 1254 chip clock cycles per _two_ shifts of the RNG (when o follows 1 directly);
exactly speaking: 0-level lasts for 13 samples while 1-level lasts for 17 samples - this gives 30 samples
altogether which corresponds to 1254 chip clock cycles. 13 samples = 543 cycles, 17 samples= 711 cycles.

*/
	c_value = ((double)(1<<PRECISION_SH)) / 543.0;
	RNG_f_0 = c_value * scaler;	/* scaled to our sample rate */
	c_value = ((double)(1<<PRECISION_SH)) / 711.0;
	RNG_f_1 = c_value * scaler;	/* scaled to our sample rate */

	c_value = ((double)(1<<PRECISION_SH)) / 209.0;
	RNG_delay = c_value * scaler;	/* scaled to our sample rate */

	logerror("rng_f_0  =%08x\n", RNG_f_0);
	logerror("rng_f_1  =%08x\n", RNG_f_1);
	logerror("rng_delay=%08x\n", RNG_delay);

	/* 0 means no particular part so we allow 1 cycle */
	c_value = ((double)(1<<PRECISION_SH)) / 1.0;
	speeds[0] = c_value * scaler;	/* scaled to our sample rate */
	logerror("speed[%2i]=%08x\n", 0, speeds[0]);

	for (i = 1; i < 256; i++)
	{
		int cycles_num, j;

		cycles_num = 0;
		for (j=0; j<8; j++)
		{
			if (i&(1<<j))
				cycles_num += cycles_per_bit[j];
		}
		c_value = ((double)(1<<PRECISION_SH)) / ((double)cycles_num);
		speeds[i] = c_value * scaler;	/* scaled to our sample rate */
		logerror("speed[%2i]=%08x\n", i, speeds[i]);
	}

	/* special case for sustain param =0, which takes 1003 cycles */
	c_value = ((double)(1<<PRECISION_SH)) / 1003.0;
	speeds[256] = c_value * scaler;	/* scaled to our sample rate */
	logerror("speed[%2i]=%08x\n", 256, speeds[256]);


	namco_54xx_sh_reset();
	return 0;
}

void namco_54xx_sh_stop(void)
{
}
