/***************************************************************************

    Fujitsu MB86233/MB86234 (TGP), from current MAME
    (src/devices/cpu/mb86233, copyright Olivier Galibert, BSD-3-Clause),
    in C for MAME 2003. The four address spaces (program, data, io and
    register file) are driver callbacks.

***************************************************************************/

#ifndef MB86233_H
#define MB86233_H

#include "osd_cpu.h"

struct mb86233_config
{
	UINT32 (*prog_r)( UINT32 offset );
	UINT32 (*data_r)( UINT32 offset );
	void   (*data_w)( UINT32 offset, UINT32 data );
	UINT32 (*io_r)( UINT32 offset );
	void   (*io_w)( UINT32 offset, UINT32 data );
	UINT32 (*rf_r)( UINT32 offset );
	void   (*rf_w)( UINT32 offset, UINT32 data );
};

extern int mb86233_icount;

extern void mb86233_init(void);
extern void mb86233_reset(void *param);
extern void mb86233_exit(void);
extern int mb86233_execute(int cycles);
extern unsigned mb86233_get_context(void *dst);
extern void mb86233_set_context(void *src);
extern unsigned mb86233_get_reg(int regnum);
extern void mb86233_set_reg(int regnum, unsigned val);
extern void mb86233_set_irq_line(int irqline, int state);
extern void mb86233_set_irq_callback(int (*callback)(int irqline));
extern const char *mb86233_info(void *context, int regnum);
extern unsigned mb86233_dasm(char *buffer, unsigned pc);

/* a memory callback that cannot complete now (FIFO empty or full):
   the instruction is repeated */
extern void mb86233_stall(void);
extern void mb86233_gpio_w(int pin, int state);

#endif
