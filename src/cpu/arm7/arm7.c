/*****************************************************************************
 *
 *   arm7.c
 *   Portable CPU Emulator for 32-bit ARM v3/4/5/6
 *
 *   Copyright Steve Ellenoff, all rights reserved.
 *   Thumb, DSP, and MMU support and many bugfixes by R. Belmont and Ryan Holtz.
 *
 *   - This source code is released as freeware for non-commercial purposes.
 *   - You are free to use and redistribute this code in modified or
 *     unmodified form, provided you list me in the credits.
 *   - If you modify this source code, you must add a notice to each modified
 *     source file that it has been changed.  If you're a nice person, you
 *     will clearly mark each change too.  :)
 *   - If you wish to use this for commercial purposes, please contact me at
 *     sellenoff@hotmail.com
 *   - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *  This work is based on:
 *  #1) 'Atmel Corporation ARM7TDMI (Thumb) Datasheet - January 1999'
 *  #2) Arm 2/3/6 emulator By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino (MAME CORE 0.76)
 *
 *****************************************************************************

 mame2003-plus: modified. The core files (arm7core.c/h, arm7exec.c) are
 those of MAME 0.139, with the memory accesses going through this
 version's 32-bit little endian memory system (cpu_readmem32ledw...) and
 the state in one static structure; this file is the cpu interface of
 this version. ARMv4T (ARM7TDMI): no MMU, no coprocessors.

 ******************************************************************************/

#include "driver.h"
#include "state.h"
#include "arm7.h"
#include "arm7core.h"

#define LOG(x)				logerror x
#define fatalerror			logerror
#define ENDIANNESS_BIG		1

/* the memory handlers of the core (arm7core.c) */
#define READ8(addr)         arm7_cpu_read8(cpustate, addr)
#define WRITE8(addr,data)   arm7_cpu_write8(cpustate, addr,data)
#define READ16(addr)        arm7_cpu_read16(cpustate, addr)
#define WRITE16(addr,data)  arm7_cpu_write16(cpustate, addr,data)
#define READ32(addr)        arm7_cpu_read32(cpustate, addr)
#define WRITE32(addr,data)  arm7_cpu_write32(cpustate, addr,data)
#define PTR_READ32          &arm7_cpu_read32
#define PTR_WRITE32         &arm7_cpu_write32

#define ARM7REG(reg)        cpustate->sArmRegister[reg]
#define ARM7_ICOUNT         arm7_ICount

static arm_state arm7_state;
int arm7_ICount;

/* the core's inline functions are local to this file */
#undef INLINE
#if defined( _MSC_VER )
#define INLINE static __inline
#elif defined( __GNUC__ )
#define INLINE static __inline__
#else
#define INLINE static
#endif

/* ARMv5 helpers the core refers to (CLZ, QADD...): not reached on the ARMv4T */
INLINE UINT32 count_leading_zeros(UINT32 val)
{
	UINT32 count = 0;
	if (val == 0)
		return 32;
	while (!(val & 0x80000000))
	{
		count++;
		val <<= 1;
	}
	return count;
}

INLINE INT64 saturate_qbit_overflow(arm_state *cpustate, INT64 res)
{
	if (res > 2147483647)	/* INT32_MAX */
	{
		res = 2147483647;
		SET_CPSR(GET_CPSR | Q_MASK);
	}
	else if (res < (-2147483647-1))	/* INT32_MIN */
	{
		res = (-2147483647-1);
		SET_CPSR(GET_CPSR | Q_MASK);
	}
	return res;
}

/* no MMU on the ARM7TDMI: the core only calls this when one is enabled */
INLINE UINT32 arm7_tlb_translate(arm_state *cpustate, UINT32 vaddr)
{
	return vaddr;
}

#include "arm7core.c"

void arm7_init(void)
{
	arm_state *cpustate = &arm7_state;

	memset(cpustate, 0, sizeof(*cpustate));
	cpustate->archRev = 4;
	cpustate->archFlags = eARM_ARCHFLAGS_T;
	arm7_core_init(cpustate, "arm7", cpu_getactivecpu());
}

void arm7_reset(void *param)
{
	arm7_core_reset(&arm7_state);
}

void arm7_exit(void)
{
}

int arm7_execute(int cycles)
{
	arm7_ICount = cycles;
#include "arm7exec.c"
	return cycles - arm7_ICount;
}

unsigned arm7_get_context(void *dst)
{
	if (dst)
		memcpy(dst, &arm7_state, sizeof(arm7_state));
	return sizeof(arm7_state);
}

void arm7_set_context(void *src)
{
	if (src)
		memcpy(&arm7_state, src, sizeof(arm7_state));
}

unsigned arm7_get_reg(int regnum)
{
	arm_state *cpustate = &arm7_state;

	switch (regnum)
	{
		case REG_PC:
		case ARM7_PC:			return R15;
		case REG_SP:			return GetRegister(cpustate, 13);
		case REG_PREVIOUSPC:	return 0;
		case ARM7_CPSR:			return GET_CPSR;
	}
	if (regnum >= ARM7_R0 && regnum <= ARM7_R15)
		return GetRegister(cpustate, regnum - ARM7_R0);
	return 0;
}

void arm7_set_reg(int regnum, unsigned val)
{
	arm_state *cpustate = &arm7_state;

	switch (regnum)
	{
		case REG_PC:
		case ARM7_PC:			R15 = val; return;
		case REG_SP:			SetRegister(cpustate, 13, val); return;
		case ARM7_CPSR:			SET_CPSR(val); return;
	}
	if (regnum >= ARM7_R0 && regnum <= ARM7_R15)
		SetRegister(cpustate, regnum - ARM7_R0, val);
}

void arm7_set_irq_line(int irqline, int state)
{
	arm7_core_set_irq_line(&arm7_state, irqline, state);
}

void arm7_set_irq_callback(int (*callback)(int irqline))
{
	arm7_state.irq_callback = callback;
}

const char *arm7_info(void *context, int regnum)
{
	static char buffer[16][64];
	static int which = 0;
	arm_state *cpustate = context ? (arm_state *)context : &arm7_state;

	which = (which + 1) % 16;
	buffer[which][0] = '\0';

	if (regnum >= CPU_INFO_REG + ARM7_R0 && regnum <= CPU_INFO_REG + ARM7_R15)
	{
		sprintf(buffer[which], "R%-3d:%08x", regnum - CPU_INFO_REG - ARM7_R0, GetRegister(cpustate, regnum - CPU_INFO_REG - ARM7_R0));
		return buffer[which];
	}
	switch (regnum)
	{
		case CPU_INFO_REG + ARM7_PC:	sprintf(buffer[which], "PC  :%08x", R15); break;
		case CPU_INFO_REG + ARM7_CPSR:	sprintf(buffer[which], "CPSR:%08x", GET_CPSR); break;
		case CPU_INFO_FLAGS:
			sprintf(buffer[which], "%c%c%c%c%c%c%c %s",
				(GET_CPSR & N_MASK) ? 'N' : '-',
				(GET_CPSR & Z_MASK) ? 'Z' : '-',
				(GET_CPSR & C_MASK) ? 'C' : '-',
				(GET_CPSR & V_MASK) ? 'V' : '-',
				(GET_CPSR & I_MASK) ? 'I' : '-',
				(GET_CPSR & F_MASK) ? 'F' : '-',
				(GET_CPSR & T_MASK) ? 'T' : '-',
				GetModeText(GET_CPSR));
			break;
		case CPU_INFO_NAME:		return "ARM7";
		case CPU_INFO_FAMILY:	return "Acorn Risc Machine";
		case CPU_INFO_VERSION:	return "2.0 (MAME 0.139 core)";
		case CPU_INFO_FILE:		return __FILE__;
		case CPU_INFO_CREDITS:	return "Copyright Steve Ellenoff, sellenoff@hotmail.com";
	}
	return buffer[which];
}

unsigned arm7_dasm(char *buffer, unsigned pc)
{
	if (T_IS_SET(arm7_state.sArmRegister[eCPSR]))
	{
		sprintf(buffer, "$%04x", cpu_readmem32ledw_word(pc & ~1));
		return 2;
	}
	sprintf(buffer, "$%08x", cpu_readmem32ledw_dword(pc & ~3));
	return 4;
}
