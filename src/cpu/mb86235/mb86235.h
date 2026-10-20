/***************************************************************************

    Fujitsu MB86235 "TGPx4", from current MAME
    (src/devices/cpu/mb86235; copyright Angelo Salese, ElSemi, Ville Linde,
    Matthew Daniels, BSD-3-Clause), the interpreter in C for MAME 2003.
    Program memory, the external bus and the FIFOs are driver callbacks;
    the internal A and B bus RAMs are part of the CPU.

***************************************************************************/

#ifndef MB86235_H
#define MB86235_H

#include "osd_cpu.h"

#define MB86235_FIFO_IFF	0	/* input FIFO full */
#define MB86235_FIFO_IFE	1	/* input FIFO empty */
#define MB86235_FIFO_OFF	2	/* output FIFO full */
#define MB86235_FIFO_OFE	3	/* output FIFO empty */

struct mb86235_config
{
	UINT64 (*prog_r)( UINT32 offset );				/* 64-bit words, 12-bit address */
	UINT32 (*ext_r)( UINT32 offset );				/* external bus, dword address */
	void   (*ext_w)( UINT32 offset, UINT32 data );
	UINT32 (*fifoin_r)( void );						/* calls mb86235_stall() when empty */
	void   (*fifoout_w)( UINT32 data );
	int    (*fifo_flag)( int which );
	void   (*fifo_clear)( int which );				/* 0: input, 1: output */
};

extern int mb86235_icount;

extern void mb86235_init(void);
extern void mb86235_reset(void *param);
extern void mb86235_exit(void);
extern int mb86235_execute(int cycles);
extern unsigned mb86235_get_context(void *dst);
extern void mb86235_set_context(void *src);
extern unsigned mb86235_get_reg(int regnum);
extern void mb86235_set_reg(int regnum, unsigned val);
extern void mb86235_set_irq_line(int irqline, int state);
extern void mb86235_set_irq_callback(int (*callback)(int irqline));
extern const char *mb86235_info(void *context, int regnum);
extern unsigned mb86235_dasm(char *buffer, unsigned pc);

/* a FIFO read that cannot complete now: the instruction is repeated */
extern void mb86235_stall(void);

#endif
