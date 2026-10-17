/***************************************************************************

    sljit_mame.c

    Builds the bundled sljit once for the recompilers in this core, on
    the hosts they support (see sljit_mame.h); empty elsewhere.

***************************************************************************/

#include "sljit_mame.h"

#if SLJIT_MAME_SUPPORTED
#include "sljitLir.c"
#else
/* ISO C forbids an empty translation unit */
typedef int sljit_mame_unused;
#endif
