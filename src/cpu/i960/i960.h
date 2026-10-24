/***************************************************************************

    Intel 80960KA/KB, from current MAME (src/devices/cpu/i960,
    copyright Farfetch'd, R. Belmont, BSD-3-Clause), in C for MAME 2003.

***************************************************************************/

#ifndef I960_H
#define I960_H

#include "osd_cpu.h"

enum
{
	I960_PFP = 0,
	I960_SP  = 1,
	I960_RIP = 2,
	I960_FP  = 31
};

/* register numbers for get_reg / set_reg */
enum
{
	I960_IPREG = 1, I960_PIPREG, I960_SATREG, I960_PRCBREG, I960_PCREG, I960_ACREG,
	I960_R0REG
	/* I960_R0REG + n: r0..r15 g0..g15 */
};

enum
{
	I960_IRQ0 = 0,
	I960_IRQ1 = 1,
	I960_IRQ2 = 2,
	I960_IRQ3 = 3
};

extern int i960_icount;

extern void i960_init(void);
extern void i960_reset(void *param);
extern void i960_exit(void);
extern int i960_execute(int cycles);
extern unsigned i960_get_context(void *dst);
extern void i960_set_context(void *src);
extern unsigned i960_get_reg(int regnum);
extern void i960_set_reg(int regnum, unsigned val);
extern void i960_set_irq_line(int irqline, int state);
extern void i960_set_irq_callback(int (*callback)(int irqline));
extern const char *i960_info(void *context, int regnum);
extern unsigned i960_dasm(char *buffer, unsigned pc);

/* Model 2: a FIFO access that cannot complete now (copro FIFO empty or
   full) repeats the instruction later; a multi-word access already half
   done resumes where it stopped (current MAME, i960_stall) */
extern void i960_stall(void);
/* the regions where ldl/ldt/ldq/stl/stt/stq step through the addresses
   (current MAME's BURST flag); elsewhere (FIFOs, registers) every word
   goes to the same address. NULL: everywhere. */
extern void i960_set_burst_callback(int (*is_burst)(UINT32 address));

/* the addresses an idle loop may read: reading them has no side effect
   and their contents change only between timeslices (RAM, a status
   register updated by an interrupt). NULL (default): idle loops are not
   skipped. */
extern void i960_set_idle_read_callback(int (*read_ok)(UINT32 address));

#endif
