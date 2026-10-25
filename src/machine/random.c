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

    The code that only changes the picture has a generator of its own:
    the screen updates don't run on the frames skipped by the frameskip,
    which depends on the host speed, so they must not take numbers from
    the emulation's generator.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "machine/random.h"

/* the seed of current MAME */
#define MAME_RAND_SEED 0x9d14abd7

static UINT32 rand_seed = MAME_RAND_SEED;
static UINT32 rand_video_seed = MAME_RAND_SEED;

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

int mame_rand_video(void)
{
	rand_video_seed = 1664525 * rand_video_seed + 1013904223;
	return (int)(((rand_video_seed >> 16) | (rand_video_seed << 16)) & 0x7fffffff);
}

void mame_rand_init(void)
{
	rand_seed = MAME_RAND_SEED;
	rand_video_seed = MAME_RAND_SEED;
	state_save_register_UINT32("mame_rand", 0, "seed", &rand_seed, 1);
	state_save_register_UINT32("mame_rand", 0, "video_seed", &rand_video_seed, 1);
}
