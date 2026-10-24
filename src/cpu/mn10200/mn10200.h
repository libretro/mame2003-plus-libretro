/***************************************************************************

 mn10200.h : Panasonic MN10200 (MN1020012A) emulator

 Backported to the MAME 0.78 CPU interface (mame2003-plus) from current
 MAME (Olivier Galibert, R. Belmont, hap). The on-chip peripherals
 (interrupt controller, 8-bit timers, ports) are at 0xfc00-0xffff and
 handled inside the core; the four ports go to the driver through the
 callbacks below.

****************************************************************************/

#ifndef _MN10200_H_
#define _MN10200_H_

#include "osd_cpu.h"

enum
{
	MN10200_PC = 1,
	MN10200_PSW,
	MN10200_MDR,
	MN10200_D0,
	MN10200_D1,
	MN10200_D2,
	MN10200_D3,
	MN10200_A0,
	MN10200_A1,
	MN10200_A2,
	MN10200_A3,
	MN10200_NMICR,
	MN10200_IAGR
};

/* external interrupt pins */
enum
{
	MN10200_IRQ0 = 0,
	MN10200_IRQ1,
	MN10200_IRQ2,
	MN10200_IRQ3,

	MN10200_MAX_EXT_IRQ
};

typedef data8_t (*mn10200_port_read_func)(int port);
typedef void (*mn10200_port_write_func)(int port, data8_t data);

/* hook the ports 0-3 to the driver (call before reset; NULL: open bus) */
void mn10200_set_port_handlers(mn10200_port_read_func readfn, mn10200_port_write_func writefn);

extern int mn10200_ICount;

void mn10200_init(void);
void mn10200_reset(void *param);
void mn10200_exit(void);
int mn10200_execute(int cycles);
unsigned mn10200_get_context(void *dst);
void mn10200_set_context(void *src);
unsigned mn10200_get_reg(int regnum);
void mn10200_set_reg(int regnum, unsigned val);
void mn10200_set_irq_line(int irqline, int state);
void mn10200_set_irq_callback(int (*callback)(int irqline));
const char *mn10200_info(void *context, int regnum);
unsigned mn10200_dasm(char *buffer, unsigned pc);

#endif
