/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"

unsigned char *toypop_sound_sharedram, *toypop_m68000_sharedram, *toypop_customio;
static unsigned char interrupt_enable_mainCPU, interrupt_enable_sound, interrupt_enable_68k;
/* variables used by the coinage of Libble Rabble*/
static int credits, coinsA, coinsB;
static int coinageA[8][2] = {{1,1},{2,1},{1,3},{3,1},{1,2},{2,3},{1,6},{3,2}};
static int coinageB[4][2] = {{1,1},{1,7},{1,5},{2,1}};

MACHINE_INIT( toypop )
{
	credits = coinsA = coinsB = 0;
	interrupt_enable_mainCPU = 0;
	interrupt_enable_sound = 0;
	interrupt_enable_68k = 0;
}

READ_HANDLER( toypop_sound_sharedram_r )
{
	/* to speed up emulation, we check for the loop the sound CPU sits in most of the time
	   and end the current iteration (things will start going again with the next IRQ) */
	if (offset == 0xa1 - 0x40 && toypop_sound_sharedram[offset] == 0 && activecpu_get_pc() == 0xe4df)
		cpu_spinuntil_int();
	return toypop_sound_sharedram[offset];
}

WRITE_HANDLER( toypop_sound_sharedram_w )
{
	toypop_sound_sharedram[offset] = data;
}

READ16_HANDLER( toypop_m68000_sharedram_r )
{
	return toypop_m68000_sharedram[offset];
}

WRITE16_HANDLER( toypop_m68000_sharedram_w )
{
	if (ACCESSING_LSB)
		toypop_m68000_sharedram[offset] = data & 0xff;
}

WRITE_HANDLER( toypop_main_interrupt_enable_w )
{
	interrupt_enable_mainCPU = 1;
}

WRITE_HANDLER( toypop_main_interrupt_disable_w )
{
	interrupt_enable_mainCPU = 0;
}

WRITE_HANDLER( toypop_sound_interrupt_enable_w )
{
	interrupt_enable_sound = 1;
}

WRITE_HANDLER( toypop_sound_interrupt_disable_w )
{
	interrupt_enable_sound = 0;
}

WRITE16_HANDLER( toypop_m68000_interrupt_enable_w )
{
	interrupt_enable_68k = 1;
}

WRITE16_HANDLER( toypop_m68000_interrupt_disable_w )
{
	interrupt_enable_68k = 0;
}

INTERRUPT_GEN( toypop_main_interrupt )
{
	if (interrupt_enable_mainCPU)
		irq0_line_hold();
}

INTERRUPT_GEN( toypop_sound_interrupt )
{
	if (interrupt_enable_sound)
		irq0_line_hold();
}

INTERRUPT_GEN( toypop_m68000_interrupt )
{
	if (interrupt_enable_68k)
		cpu_set_irq_line(2, 6, HOLD_LINE);
}

READ_HANDLER( toypop_customio_r )
{
	int mode = toypop_customio[8];

	/* mode 5 values are actually checked against these numbers during power up */
	if (mode == 5)
		switch (offset) {
			case 2:
				return 15;
			case 6:
				return 12;
			case 16:
				return 6;
			case 17:
				return 9;
			case 32:
				return 6;
			case 33:
				return 9;
			default:
				return toypop_customio[offset];
		}
	else
		switch (offset) {
			case 4:
				return readinputport(0) & 0x0f;
			case 5:
				return readinputport(0) >> 4;
			case 6:
				return readinputport(1) & 0x0f;
			case 7:
				return readinputport(1) >> 4;
			case 16:
				return readinputport(2) & 0x0f;
			case 17:
				return readinputport(2) >> 4;
			case 18:
				return readinputport(3) & 0x0f;
			case 19:
				return readinputport(3) >> 4;
			default:
				return toypop_customio[offset];
		}
}

READ_HANDLER( liblrabl_customio_r )
{
	static int lastcoin, laststart;
	int val, tmp, mode = toypop_customio[24];

	/* mode 7 values are actually checked against these numbers during power up */
	if (mode == 7)
		switch (offset) {
			case 2:
				return 15;
			case 6:
				return 12;
			case 18:
				return 14;
			case 39:
				return 6;
			default:
				return toypop_customio[offset];
		}
	else if (mode == 1)
		switch (offset) {
			case 0:		/* Coin slots*/
				val = readinputport(3) & 0x0f;
				/* bit 0 is a trigger for the coin 1 slot*/
				if ((val & 1) && !(lastcoin & 1)) {
					tmp = (readinputport(1) & 0xe0) >> 5;
					coinsA++;
					if (coinsA == coinageA[tmp][0]) {
						credits += coinageA[tmp][1];
						coinsA = 0;
					}
				}
				/* bit 1 is a trigger for the coin 2 slot*/
				if ((val & 2) && !(lastcoin & 2)) {
					tmp = (readinputport(0) & 0x18) >> 3;
					coinsB++;
					if (coinsB == coinageB[tmp][0]) {
						credits += coinageB[tmp][1];
						coinsB = 0;
					}
				}
				return lastcoin = val;
			case 1:		/* Start buttons*/
				val = readinputport(3) >> 4;
				/* bit 0 is a trigger for the 1 player start*/
				if ((val & 1) && !(laststart & 1))
					credits--;
				/* bit 1 is a trigger for the 2 player start*/
				if ((val & 2) && !(laststart & 2)) {
					if (credits >= 2)
						credits -= 2;
					else
						val &= ~2;	/* otherwise you can start with no credits*/
				}
				return laststart = val;
			case 2:		/* High BCD of credits*/
				return credits / 10;
			case 3:		/* Low BCD of credits*/
				return credits % 10;
/*			case 5:		*/ /* read, but unknown*/
/*				return readinputport(2) >> 4;*/
			case 4:		/* Right joystick*/
				return readinputport(4) >> 4;
			case 6:		/* Player 2 right joystick in cocktail mode*/
				return readinputport(5) >> 4;
			case 16:
				return readinputport(1) >> 4;
			case 17:
				return readinputport(0) & 0x0f;
			case 18:
				return readinputport(0) >> 4;
			case 19:
				return readinputport(1) & 0x0f;
			case 34:	/* Left joystick*/
				return readinputport(4) & 0x0f;
			case 36:	/* Player 2 left joystick in cocktail mode*/
				return readinputport(5) & 0x0f;
			case 39:
				return readinputport(2) & 0x0f;
			default:
				return toypop_customio[offset];
		}
	else
		return toypop_customio[offset];
}

WRITE_HANDLER( toypop_sound_clear_w )
{
	cpu_set_reset_line(1, CLEAR_LINE);
}

WRITE_HANDLER( toypop_sound_assert_w )
{
	cpu_set_reset_line(1, ASSERT_LINE);
}

WRITE_HANDLER( toypop_m68000_clear_w )
{
	cpu_set_reset_line(2, CLEAR_LINE);
}

WRITE_HANDLER( toypop_m68000_assert_w )
{
	cpu_set_reset_line(2, ASSERT_LINE);
}
