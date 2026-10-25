/*****************************************************************************

    NEC V25 (and V25+) CPU, see v25.c

    license:BSD-3-Clause
    copyright-holders:Bryan McPhail, Alex W. Jackson

*****************************************************************************/

#ifndef V25_H
#define V25_H

#include "memory.h"
#include "osd_cpu.h"

/* input lines, besides 0 (INT) and IRQ_LINE_NMI */
#define V25_INPUT_LINE_INTP0	10
#define V25_INPUT_LINE_INTP1	11
#define V25_INPUT_LINE_INTP2	12
#define V25_INPUT_LINE_POLL		20

enum
{
	V25_PC=1, V25_IP, V25_AW, V25_CW, V25_DW, V25_BW, V25_SP, V25_BP, V25_IX, V25_IY,
	V25_DS1, V25_PS, V25_SS, V25_DS0, V25_PSW, V25_IDB, V25_PENDING
};

extern int v25_ICount;

extern void v25_init(void);
extern void v25_reset(void *param);
extern void v25_exit(void);
extern int v25_execute(int cycles);
extern unsigned v25_get_context(void *dst);
extern void v25_set_context(void *src);
extern unsigned v25_get_reg(int regnum);
extern void v25_set_reg(int regnum, unsigned val);
extern void v25_set_irq_line(int irqline, int state);
extern void v25_set_irq_callback(int (*callback)(int irqline));
extern const char *v25_info(void *context, int regnum);
extern unsigned v25_dasm(char *buffer, unsigned pc);

/* Configuration, from DRIVER_INIT (before the CPU is reset).
   The opcodes are decrypted with the table when the CPU is in secure
   mode (the default when there is a table). */
extern void v25_set_decryption_table(int cpunum, const UINT8 *table);

/* the port pins: PT (input only), P0, P1 (upper 4 bits), P2 */
extern void v25_set_port_handlers(int cpunum,
	read8_handler pt_in, read8_handler p0_in, read8_handler p1_in, read8_handler p2_in,
	write8_handler p0_out, write8_handler p1_out, write8_handler p2_out);

#endif
