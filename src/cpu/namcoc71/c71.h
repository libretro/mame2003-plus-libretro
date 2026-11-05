/***************************************************************************

    Namco C71 (TMS320C25 with Namco internal ROM) - fast DSP core

    Written for Namco System 22 / Super System 22. The instruction
    semantics follow the TMS320C2x core of current MAME; the memory
    system is a page table set up by the driver, so the core does not
    go through the generic memory handlers.

    Word addresses everywhere: program, data and I/O are 16-bit word
    spaces.

***************************************************************************/

#ifndef C71_H
#define C71_H

#include "osd_cpu.h"
#include "cpuintrf.h"
#include "memory.h"

/* interrupt lines (IFR bits) */
#define C71_INT0		0
#define C71_INT1		1
#define C71_INT2		2
#define C71_TINT		3
#define C71_RINT		4
#define C71_XINT		5

enum
{
	C71_PC=1,
	C71_PFC,  C71_STR0, C71_STR1, C71_IFR,
	C71_RPTC, C71_ACC,  C71_PREG, C71_TREG,
	C71_AR0,  C71_AR1,  C71_AR2,  C71_AR3,
	C71_AR4,  C71_AR5,  C71_AR6,  C71_AR7,
	C71_STK0, C71_STK1, C71_STK2, C71_STK3,
	C71_STK4, C71_STK5, C71_STK6, C71_STK7,
	C71_DRR,  C71_DXR,  C71_TIM,  C71_PRD,
	C71_IMR,  C71_GREG
};

/* the generic memory interface is not used: give the CPU empty maps */
#define NAMCOC71_PGM_OFFSET 0

typedef data16_t (*c71_read_fn)(offs_t offset);
typedef void (*c71_write_fn)(offs_t offset, data16_t data);

/* driver configuration, by CPU number; call from DRIVER_INIT or MACHINE_INIT */
void c71_set_internal_rom(int cpunum, const data16_t *rom);		/* 0x1000 words, native order (a CPU region) */
void c71_map_data(int cpunum, offs_t start, offs_t end, data16_t *base, int writable);
void c71_map_program(int cpunum, offs_t start, offs_t end, data16_t *base, int writable);
void c71_set_data_handlers(int cpunum, c71_read_fn rd, c71_write_fn wr);	/* everything not mapped */
void c71_set_io(int cpunum, int port, c71_read_fn rd, c71_write_fn wr);
void c71_set_bio(int cpunum, int (*bio)(void));
void c71_set_serial(int cpunum, data16_t (*dr_in)(void), void (*dx_out)(data16_t));
/* the DSP spins in [pc_start, pc_end] waiting for BIO or an interrupt: burn the timeslice there */
void c71_set_idle_loop(int cpunum, int slot, UINT16 pc_start, UINT16 pc_end);

extern int c71_icount;

void c71_init(void);
void c71_reset(void *param);
void c71_exit(void);
int c71_execute(int cycles);
unsigned c71_get_context(void *dst);
void c71_set_context(void *src);
unsigned c71_get_reg(int regnum);
void c71_set_reg(int regnum, unsigned val);
void c71_set_irq_line(int irqline, int state);
void c71_set_irq_callback(int (*callback)(int irqline));
const char *c71_info(void *context, int regnum);
unsigned c71_dasm(char *buffer, unsigned pc);

#endif
