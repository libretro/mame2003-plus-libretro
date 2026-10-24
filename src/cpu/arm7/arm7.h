/*****************************************************************************
 *
 *   arm7.h
 *   Portable ARM7TDMI CPU Emulator
 *
 *   Copyright Steve Ellenoff, all rights reserved.
 *   Thumb, DSP, and MMU support and many bugfixes by R. Belmont and Ryan Holtz.
 *   (see arm7core.c for the full notice)
 *
 *   mame2003-plus: the MAME 0.139 core with the cpu interface of this
 *   version (one ARM7 at a time, 32-bit little endian bus).
 *
 *****************************************************************************/

#ifndef ARM7_H
#define ARM7_H

#include "driver.h"

/* interrupt lines and exceptions */
#define ARM7_IRQ_LINE                   0
#define ARM7_FIRQ_LINE                  1
#define ARM7_ABORT_EXCEPTION            2
#define ARM7_ABORT_PREFETCH_EXCEPTION   3
#define ARM7_UNDEFINE_EXCEPTION         4


extern int arm7_ICount;

extern void arm7_init(void);
extern void arm7_reset(void *param);
extern void arm7_exit(void);
extern int arm7_execute(int cycles);
extern unsigned arm7_get_context(void *dst);
extern void arm7_set_context(void *src);
extern unsigned arm7_get_reg(int regnum);
extern void arm7_set_reg(int regnum, unsigned val);
extern void arm7_set_irq_line(int irqline, int state);
extern void arm7_set_irq_callback(int (*callback)(int irqline));
extern const char *arm7_info(void *context, int regnum);
extern unsigned arm7_dasm(char *buffer, unsigned pc);

#endif /* ARM7_H */
