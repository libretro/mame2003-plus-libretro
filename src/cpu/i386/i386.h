/***************************************************************************

  Intel 386 emulator (Ville Linde), from MAME 0.139

***************************************************************************/

#ifndef I386INTF_H
#define I386INTF_H

#include "cpuintrf.h"

/* the A20 gate, as an input line */
#define I386_INPUT_LINE_A20		1

extern int i386_ICount;

/* the fast memory: a range of plain RAM or ROM at base, which the CPU */
/* fetches, reads or writes directly (flags), without the memory handlers; */
/* set from DRIVER_INIT, emptied at the CPU init. Little endian hosts only */
#define I386_FASTMEM_MAX		8
#define I386_FASTMEM_FETCH		1
#define I386_FASTMEM_READ		2
#define I386_FASTMEM_WRITE		4
void i386_set_fastmem(UINT32 start, UINT32 end, UINT8 *base, int flags);

void i386_init(void);
void i386_reset(void *param);
void i386_exit(void);
int i386_execute(int cycles);
unsigned i386_get_context(void *dst);
void i386_set_context(void *src);
unsigned i386_get_reg(int regnum);
void i386_set_reg(int regnum, unsigned val);
void i386_set_irq_line(int irqline, int state);
void i386_set_irq_callback(int (*callback)(int irqline));
const char *i386_info(void *context, int regnum);
unsigned i386_dasm(char *buffer, unsigned pc);

#endif
