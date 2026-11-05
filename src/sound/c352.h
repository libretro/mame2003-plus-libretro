/*********************************************************

    Namco C352 PCM sound chip

*********************************************************/

#ifndef _C352_H_
#define _C352_H_

struct C352interface
{
	int clock;			/* input clock */
	int divider;		/* output rate = clock / divider (288 on System 12) */
	int region;			/* sample ROM region */
	int mixing_level;	/* master volume */
};

/* Optional mixing of the four outputs (front left, front right, rear left,
   rear right) into the stereo stream, as current MAME maps a cabinet's extra
   speakers to stereo. Call before the sound starts (DRIVER_INIT); without it
   only the front outputs are used. route[n][0..1]: gain of output n into the
   left and right stream, 0x100 = 1.0. */
void c352_set_routing(const int route[4][2]);
/* per output gain, 0x100 = 1.0 (MB87078 volume on Super System 22); routing only */
void c352_set_output_gain(int output, int gain);

int C352_sh_start(const struct MachineSound *msound);
void C352_sh_stop(void);

READ16_HANDLER( c352_0_r );
WRITE16_HANDLER( c352_0_w );

#endif
