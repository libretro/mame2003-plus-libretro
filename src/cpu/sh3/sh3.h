/*****************************************************************************

	sh3.h

	Hitachi SH-3 (SH7709S) CPU core, from current MAME (sh.cpp, sh4.cpp,
	sh3comn.cpp, sh4comn.cpp, sh4tmu.cpp, sh7709s.cpp: R. Belmont,
	David Haywood, Jesus Ramos and the other authors of those files).

	Big endian, 32-bit data bus (32bedw). No FPU and no MMU (current MAME
	doesn't emulate the SH-3 MMU either). The internal registers of the
	SH7709S are emulated in the core; the I/O ports call the driver.

*****************************************************************************/

#ifndef _SH3_H
#define _SH3_H

#include "cpuintrf.h"
#include "osd_cpu.h"

/* the external interrupt lines IRL0-IRL3 (current MAME: SH4_IRL0-SH4_IRL3) */
#define SH3_IRL0		0
#define SH3_IRL1		1
#define SH3_IRL2		2
#define SH3_IRL3		3

/* the I/O ports, for the port callbacks */
enum
{
	SH3_PORT_A = 0, SH3_PORT_B, SH3_PORT_C, SH3_PORT_D, SH3_PORT_E, SH3_PORT_F,
	SH3_PORT_G, SH3_PORT_H, SH3_PORT_J, SH3_PORT_K, SH3_PORT_L, SH3_PORT_COUNT
};

enum
{
	SH3_PC=1, SH3_SR, SH3_PR, SH3_GBR, SH3_VBR, SH3_MACH, SH3_MACL,
	SH3_R0, SH3_R1, SH3_R2, SH3_R3, SH3_R4, SH3_R5, SH3_R6, SH3_R7,
	SH3_R8, SH3_R9, SH3_R10, SH3_R11, SH3_R12, SH3_R13, SH3_R14, SH3_R15,
	SH3_SSR, SH3_SPC, SH3_EA
};

struct sh3_config
{
	/* the SH7709S cache and bus timing of current MAME (sh7709s.cpp, made
	   for the CV1000): data accesses and cache misses cost cycles */
	int cache_timing;
	/* the clock of the peripheral modules (TMU), Hz */
	int pm_clock;
	/* the I/O ports (SH3_PORT_x); NULL reads 0 */
	data8_t (*port_r)(int port);
	void (*port_w)(int port, data8_t data);
};

extern int sh3_ICount;

/* the fast memory: plain RAM or ROM at base (an array of data32_t, as the
   memory system keeps a 32-bit big-endian space), read, written and fetched
   without the memory handlers; physical addresses (29 bits). Set from
   DRIVER_INIT, emptied at the CPU init */
#define SH3_FASTRAM_MAX		4
void sh3_set_fastram(UINT32 start, UINT32 end, int readonly, data32_t *base);

/* the cache and bus timing of an access done by a memory handler (current
   MAME: sh7709s_device::update_access_cycles) */
void sh3_update_access_cycles(UINT32 address, int write);

/* the recompiler (sh3jit.c): available on this host, and on/off (off by default) */
int sh3_jit_available(void);
void sh3_set_jit(int enable);

/* the total number of cycles run */
UINT32 sh3_total_cycles_lo(void);

void sh3_init(void);
void sh3_reset(void *param);
void sh3_exit(void);
int sh3_execute(int cycles);
unsigned sh3_get_context(void *dst);
void sh3_set_context(void *src);
unsigned sh3_get_reg(int regnum);
void sh3_set_reg(int regnum, unsigned val);
void sh3_set_irq_line(int irqline, int state);
void sh3_set_irq_callback(int (*callback)(int irqline));
const char *sh3_info(void *context, int regnum);
unsigned sh3_dasm(char *buffer, unsigned pc);

#endif
