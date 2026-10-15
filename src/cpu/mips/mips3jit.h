/***************************************************************************

    mips3jit.h

    Host support of the MIPS3 recompiler: the bundled sljit, configured
    and built once for all the recompilers (src/lib/sljit/sljit_mame.h).

***************************************************************************/

#ifndef MIPS3JIT_H
#define MIPS3JIT_H

/* the sljit build shared by the recompilers decides where they run */
#include "../../lib/sljit/sljit_mame.h"

#define MIPS3_JIT_SUPPORTED		SLJIT_MAME_SUPPORTED

#endif /* MIPS3JIT_H */
