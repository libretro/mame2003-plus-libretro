#ifndef namco54_h
#define namco54_h


/*
 * The 4 bit output of each channel is passed through a bandpass filter.
 * D0 - D3 are probably TTL level.  I will round it up to 4V until measured.
 * R1, R2, R3, C1, C2 select the filter specs.  R4 is for mixing.
 *
 *         4.7k
 *   D3 >--ZZZZ--.                       .--------+---------.
 *               |                       |        |         |
 *          10k  |                      --- c1    Z         |
 *   D2 >--ZZZZ--+                      ---       Z r3      |
 *               |                       |        Z         |
 *          22k  |      r1               |  c2    |  |\ 5V  |
 *   D1 >--ZZZZ--+-----ZZZZ----+---------+--||----+  | \|   |
 *               |             Z                  '--|- \   |     r4
 *          47k  |             Z r2                  |   >--+----ZZZZ--> out
 *   D0 >--ZZZZ--'             Z                  .--|+ /
 *                             |                  |  | /|
 *                            gnd          2V >---'  |/ gnd
 *
 */

struct namco_54xx_interface
{
	int		baseclock;		/* clock */
	int		mixing_level;	/* volume */
	double	r1[3];
	double	r2[3];
	double	r3[3];
	double	r4[3];
	double	c1[3];
	double	c2[3];
};

int namco_54xx_sh_start(const struct MachineSound *msound);
void namco_54xx_sh_reset(void);
void namco_54xx_sh_stop(void);

void namcoio_54XX_write(int data);

#endif
