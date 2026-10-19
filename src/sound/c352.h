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

int C352_sh_start(const struct MachineSound *msound);
void C352_sh_stop(void);

READ16_HANDLER( c352_0_r );
WRITE16_HANDLER( c352_0_w );

#endif
