/***************************************************************************

 h83002.h : Public constants and function defs for the H8/3002 emulator.

 Backported to the MAME 0.78 CPU interface (mame2003-plus) from the legacy
 H8/3xx core of MAME 0.152 (original by The_Author & DynaChicken for ZiNc,
 MAME changes by R. Belmont, Luca Elia, Tomasz Slanina, Tim Schuerewegen,
 O. Galibert).

 Only the H8/3002 (16-bit, advanced mode, 24-bit address space) is
 supported.  On-chip RAM (0xfffd10-0xffff0f) and the on-chip peripheral
 registers (0xffff10-0xffffff) are handled inside the core.

 The 0.78 I/O port space is sized after the data bus (16 bit), so the
 H8 on-chip ports, ADC and serial channels are routed to the driver
 through the callbacks below instead of a PORT map.

****************************************************************************/

#ifndef _H83002_H_
#define _H83002_H_

#include "osd_cpu.h"

#define H8_IFLAG  0x80
#define H8_UIFLAG 0x40
#define H8_HFLAG  0x20
#define H8_UFLAG  0x10
#define H8_NFLAG  0x08
#define H8_ZFLAG  0x04
#define H8_VFLAG  0x02
#define H8_CFLAG  0x01

/* registers */
enum
{
	H8_E0 = 1,
	H8_E1,
	H8_E2,
	H8_E3,
	H8_E4,
	H8_E5,
	H8_E6,
	H8_E7,

	H8_PC,
	H8_CCR
};

/* external interrupt lines */
enum
{
	H8_IRQ0 = 0,
	H8_IRQ1,
	H8_IRQ2,
	H8_IRQ3,
	H8_IRQ4,
	H8_IRQ5,

	H8_SCI_0_RX,	/* incoming character on SCI 0 */
	H8_SCI_1_RX		/* incoming character on SCI 1 */
};

/* I/O "ports" passed to the driver callbacks */
enum
{
	/* digital I/O ports */
	H8_PORT_4 = 3,
	H8_PORT_6 = 5,
	H8_PORT_7,
	H8_PORT_8,
	H8_PORT_9,
	H8_PORT_A,
	H8_PORT_B,

	/* analog inputs */
	H8_ADC_0_H = 0x20,
	H8_ADC_0_L,
	H8_ADC_1_H,
	H8_ADC_1_L,
	H8_ADC_2_H,
	H8_ADC_2_L,
	H8_ADC_3_H,
	H8_ADC_3_L,

	/* serial ports */
	H8_SERIAL_0 = 0x30,
	H8_SERIAL_1
};

typedef data8_t (*h8_io_read_func)(int port);
typedef void (*h8_io_write_func)(int port, data8_t data);

/* hook the on-chip ports/ADC/serial to the driver (call before reset) */
void h8_3002_set_io_handlers(h8_io_read_func readfn, h8_io_write_func writefn);
/*
	Speedup (end 0 = off): memory in start-end only changes outside the
	H8's timeslice (other CPUs, the H8's own interrupts) and reading it has
	no side effects. A polling loop on it, "mov.b @aa:24, Rd / bne (or
	beq) back", leaves the same state on every pass, so once one pass has
	run in a timeslice the whole passes that still fit are skipped:
	registers, memory and cycles end up exactly as if they had run.
*/
void h8_3002_set_idle_poll(UINT32 start, UINT32 end);

extern int h8_3002_ICount;

void h8_3002_init(void);
void h8_3002_reset(void *param);
void h8_3002_exit(void);
int h8_3002_execute(int cycles);
unsigned h8_3002_get_context(void *dst);
void h8_3002_set_context(void *src);
unsigned h8_3002_get_reg(int regnum);
void h8_3002_set_reg(int regnum, unsigned val);
void h8_3002_set_irq_line(int irqline, int state);
void h8_3002_set_irq_callback(int (*callback)(int irqline));
const char *h8_3002_info(void *context, int regnum);
unsigned h8_3002_dasm(char *buffer, unsigned pc);

#endif
