/*****************************************************************************
 *
 *   i8051.h
 *   Portable MCS-51 Family Emulator
 *
 *   Copyright Steve Ellenoff, all rights reserved.
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
 *   October 2008, Couriersud: Major rewrite (MAME 0.128-0.139 mcs51 core)
 *
 *   MODIFIED for mame2003-plus: backported from MAME 0.139 src/emu/cpu/mcs51
 *   to the MAME 0.78 cpu_interface. DS5002FP and 80C52 variants removed,
 *   internal RAM/SFRs kept inside the CPU context, MOVX routed through
 *   per-CPU driver handlers (the 0.78 core has no DATA address space).
 *
 *****************************************************************************/

#ifndef _I8051_H
#define _I8051_H

#include "cpuintrf.h"
#include "osd_cpu.h"
#include "memory.h"

/* The MCS-51 runs one machine cycle every 12 oscillator clocks.
   Give the CPU clock in the machine driver as XTAL / I8051_CLOCK_DIVIDER. */
#define I8051_CLOCK_DIVIDER		12

/****************************************************************************
 *  Input lines (use with cpu_set_irq_line)
 *  INT0/INT1 follow the TCON IT0/IT1 setting (edge or level);
 *  T0/T1 count 0->1 transitions when the timer is in counter mode,
 *  so drive them with PULSE_LINE.
 ****************************************************************************/
enum
{
	I8051_INT0_LINE = 0,	/* P3.2: External Interrupt 0 */
	I8051_INT1_LINE,		/* P3.3: External Interrupt 1 */
	I8051_RX_LINE,			/* P3.0: Serial Port Receive Line */
	I8051_T0_LINE,			/* P3.4: Timer 0 External Input */
	I8051_T1_LINE,			/* P3.5: Timer 1 External Input */
	I8051_INPUT_LINES
};

/****************************************************************************
 *  Port space: the four parallel ports. Map them in the MCU's
 *  PORT_READ/PORT_WRITE tables, e.g. { I8051_P1, I8051_P1, mcu_p1_w }
 ****************************************************************************/
#define I8051_P0		0x00
#define I8051_P1		0x01
#define I8051_P2		0x02
#define I8051_P3		0x03

/****************************************************************************
 *  Registers
 ****************************************************************************/
enum
{
	I8051_PC=1, I8051_SP, I8051_PSW, I8051_ACC, I8051_B, I8051_DPH, I8051_DPL, I8051_IE,
	I8051_R0, I8051_R1, I8051_R2, I8051_R3, I8051_R4, I8051_R5, I8051_R6, I8051_R7, I8051_RB
};

/****************************************************************************
 *  Public functions
 ****************************************************************************/
extern int i8051_ICount;

extern void i8051_init(void);
extern void i8051_reset(void *param);
extern void i8051_exit(void);
extern int i8051_execute(int cycles);
extern unsigned i8051_get_context(void *dst);
extern void i8051_set_context(void *src);
extern unsigned i8051_get_reg(int regnum);
extern void i8051_set_reg(int regnum, unsigned val);
extern void i8051_set_irq_line(int irqline, int state);
extern void i8051_set_irq_callback(int (*callback)(int irqline));
extern const char *i8051_info(void *context, int regnum);
extern unsigned i8051_dasm(char *buffer, unsigned pc);

/* External data memory (MOVX). Called from MACHINE_INIT; cpunum is the MCU's
   CPU index. Unmapped reads return 0xff, unmapped writes are dropped. */
extern void i8051_set_movx_handlers(int cpunum, read8_handler rd, write8_handler wr);

/* Serial port hooks (optional). */
extern void i8051_set_serial_tx_callback(int cpunum, void (*callback)(int data));
extern void i8051_set_serial_rx_callback(int cpunum, int (*callback)(void));

/* The 8751 is an 8051 with internal EPROM: the driver loads the dump in the
   MCU's CPU region exactly as for the 8051. */
#define CPU_I8751	CPU_I8051

#endif /* _I8051_H */
