/***************************************************************************

    sljit_mame.h

    Configuration of the bundled sljit for the recompilers in this core
    (MIPS III in mips3jit.c, PSX CPU in psxjit.c). sljit itself is built
    once, in sljit_mame.c.

    SLJIT_MAME_SUPPORTED is 1 on the hosts the recompilers support:
    little-endian x86 and ARM, 64-bit (x86-64, AArch64) and 32-bit (x86,
    ARMv6/ARMv7/Thumb-2), except on systems that forbid or restrict
    run-time code generation. Elsewhere it is 0 and sljit is not built.

***************************************************************************/

#ifndef SLJIT_MAME_H
#define SLJIT_MAME_H

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(IOS) || defined(__IOS__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || \
	defined(__SWITCH__) || defined(HAVE_LIBNX) || defined(_XBOX) || defined(__WINRT__)
#define SLJIT_MAME_NO_JIT
#endif
/* MSVC before 2013 (the 2003-2010 lanes): no recompiler, the interpreter is used */
#if defined(_MSC_VER) && _MSC_VER < 1800
#define SLJIT_MAME_NO_JIT
#endif

#if !defined(SLJIT_MAME_NO_JIT) && \
	(defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64) || \
	 defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(_M_ARM)) && \
	!defined(__ARMEB__) && \
	!(defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && \
	!defined(MSB_FIRST)
#define SLJIT_MAME_SUPPORTED	1
#else
#define SLJIT_MAME_SUPPORTED	0
#endif

#if SLJIT_MAME_SUPPORTED
#define SLJIT_CONFIG_AUTO		1
#ifndef SLJIT_DEBUG
#define SLJIT_DEBUG				0
#endif
#define SLJIT_VERBOSE			0
#include "sljitLir.h"
#endif

#endif /* SLJIT_MAME_H */
