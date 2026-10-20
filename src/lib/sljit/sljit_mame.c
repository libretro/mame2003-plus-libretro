/***************************************************************************

    sljit_mame.c

    Builds the bundled sljit once for the recompilers in this core, on
    the hosts they support (see sljit_mame.h); empty elsewhere.

***************************************************************************/

/* the core builds with _XOPEN_SOURCE=500, which on macOS hides MAP_ANON and
   MAP_JIT (sljit's executable memory allocator needs them): ask for the
   Darwin extensions too, before any system header */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#include "sljit_mame.h"

#if SLJIT_MAME_SUPPORTED
#include "sljitLir.c"
#else
/* ISO C forbids an empty translation unit */
typedef int sljit_mame_unused;
#endif
