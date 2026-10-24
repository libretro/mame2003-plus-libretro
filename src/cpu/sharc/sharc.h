/***************************************************************************

    Analog Devices ADSP-21062 "SHARC", from current MAME
    (src/devices/cpu/sharc, copyright Ville Linde, BSD-3-Clause),
    translated to C for MAME 2003. Interpreter only (no recompiler).

    The internal memory (two 32K x 32-bit blocks, with the 48-bit program
    and 16-bit short-word views) and the IOP registers live inside the
    core. External data memory (and external program memory, if any)
    is reached through driver callbacks in the config passed to the CPU
    with MDRV_CPU_CONFIG; the mame2003 memory system is not used.

    Only one SHARC per machine is supported (the state is static).

***************************************************************************/

#ifndef SHARC_H
#define SHARC_H

#include "osd_cpu.h"

enum
{
	SHARC_BOOT_MODE_EPROM = 0,
	SHARC_BOOT_MODE_HOST,
	SHARC_BOOT_MODE_LINK,
	SHARC_BOOT_MODE_NOBOOT
};

/* interrupt lines for set_irq_line (IRQ0-2 pins) */
enum
{
	SHARC_IRQ0 = 0,
	SHARC_IRQ1 = 1,
	SHARC_IRQ2 = 2
};

/* register numbers for get_reg / set_reg (also CPU_INFO_REG + n) */
enum
{
	SHARC_PC = 1, SHARC_PCSTK, SHARC_PCSTKP, SHARC_LSTKP, SHARC_FADDR, SHARC_DADDR,
	SHARC_MODE1, SHARC_MODE2, SHARC_ASTAT, SHARC_STKY, SHARC_IRPTL, SHARC_IMASK,
	SHARC_USTAT1, SHARC_USTAT2, SHARC_LCNTR, SHARC_CURLCNTR, SHARC_STSTKP,
	SHARC_R0,
	SHARC_R15 = SHARC_R0 + 15,
	SHARC_I0,
	SHARC_I15 = SHARC_I0 + 15
};

struct sharc_config
{
	int boot_mode;										/* SHARC_BOOT_MODE_xxx (MAME default: HOST) */
	UINT32 (*data_r)(UINT32 address);					/* external DM, 32-bit words, word address */
	void   (*data_w)(UINT32 address, UINT32 data);
	void   (*flag_out)(int flag, int state);			/* FLAG0-3 outputs, may be NULL */
	/* optional, may be NULL (unmapped: reads 0): external program memory,
	   48-bit words in the low bits, word address */
	UINT64 (*pgm_r)(UINT32 address);
	void   (*pgm_w)(UINT32 address, UINT64 data);
};

extern int sharc_icount;

/* CPU interface */
extern void sharc_init(void);
extern void sharc_reset(void *param);
extern void sharc_exit(void);
extern int sharc_execute(int cycles);
extern unsigned sharc_get_context(void *dst);
extern void sharc_set_context(void *src);
extern unsigned sharc_get_reg(int regnum);
extern void sharc_set_reg(int regnum, unsigned val);
extern void sharc_set_irq_line(int irqline, int state);
extern void sharc_set_irq_callback(int (*callback)(int irqline));
extern const char *sharc_info(void *context, int regnum);
extern unsigned sharc_dasm(char *buffer, unsigned pc);

/* helpers acting on the (single) SHARC; safe to call while another CPU
   is executing, e.g. from the host CPU's memory handlers */

/* MAME external_iop_write: host write to an IOP register */
extern void sharc_external_iop_write(UINT32 address, UINT32 data);
/* MAME external_dma_write: host write to the external port DMA buffer
   (host boot); with 16/48 packing, address % 3 selects the 16-bit piece */
extern void sharc_external_dma_write(UINT32 address, UINT64 data);
/* MAME set_flag_input: FLAG0-3 pins configured as inputs */
extern void sharc_set_flag_input(int flag_num, int state);
/* MAME write_stall: while asserted, the core does not execute */
extern void sharc_write_stall(int state);

/* to be called from a data_r/data_w callback that cannot complete now
   (FIFO empty/full): the access must not be committed by the driver
   (the value returned by data_r is ignored); the core ends the
   timeslice and executes the same instruction again next time, with no
   side effects committed. A stalled DMA transfer cycle is retried. */
extern void sharc_stall(void);

#endif
