/***************************************************************************

    mips3jit_sljit.c

    Builds the bundled sljit (src/lib/sljit) for the MIPS3 recompiler,
    on the hosts it supports (see mips3jit.h); empty elsewhere.

***************************************************************************/

#include "mips3jit.h"

#if MIPS3_JIT_SUPPORTED
#include "../../lib/sljit/sljitLir.c"
#else
/* ISO C forbids an empty translation unit */
typedef int mips3jit_sljit_unused;
#endif
