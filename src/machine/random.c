/***************************************************************************

    random.c

    The random number generator of the emulation, as in current MAME
    (running_machine::rand() in src/emu/machine.cpp): a linear congruential
    generator with a fixed seed, whose state is in the save states.

    Drivers use it in place of the C library's rand(). The numbers of
    rand() differ between hosts and C libraries, and its state is not in
    the save states: after a load (RunAhead, netplay rollback, rewind) or
    on another machine (netplay), the game got other numbers and ran
    differently.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "machine/random.h"

/* the seed of current MAME */
#define MAME_RAND_SEED 0x9d14abd7

static UINT32 rand_seed = MAME_RAND_SEED;

void mame_srand(unsigned long s)
{
	rand_seed = (UINT32)s;
}

unsigned long mame_rand(void)
{
	rand_seed = 1664525 * rand_seed + 1013904223;

	/* return rotated by 16 bits; the low bits have a short period
	   and are frequently used */
	return (rand_seed >> 16) | (rand_seed << 16);
}

int mame_rand_int(void)
{
	return (int)(mame_rand() & 0x7fffffff);
}

void mame_rand_init(void)
{
	rand_seed = MAME_RAND_SEED;
	state_save_register_UINT32("mame_rand", 0, "seed", &rand_seed, 1);
}
