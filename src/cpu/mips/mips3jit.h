/***************************************************************************

    mips3jit.h

    Configuration of the bundled sljit (src/lib/sljit) for the MIPS3
    recompiler. Shared by mips3.c and the sljit build unit so that both
    see the same compiler structures.

***************************************************************************/

#ifndef MIPS3JIT_H
#define MIPS3JIT_H

/* 64-bit little-endian hosts only for now (x86-64, AArch64), and not on */
/* systems that forbid or restrict run-time code generation */
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(IOS) || defined(__IOS__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || \
	defined(__SWITCH__) || defined(HAVE_LIBNX) || defined(_XBOX) || defined(__WINRT__)
#define MIPS3_NO_JIT
#endif

#if !defined(MIPS3_NO_JIT) && \
	(defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64)) && \
	!(defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && \
	!defined(MSB_FIRST)
#define MIPS3_JIT_SUPPORTED		1
#else
#define MIPS3_JIT_SUPPORTED		0
#endif

#if MIPS3_JIT_SUPPORTED
#define SLJIT_CONFIG_AUTO		1
#define SLJIT_DEBUG				0
#define SLJIT_VERBOSE			0
#include "../../lib/sljit/sljitLir.h"
#endif

#endif /* MIPS3JIT_H */
