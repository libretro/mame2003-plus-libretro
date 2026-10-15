#ifndef HEADER__M37710
#define HEADER__M37710

/* ======================================================================== */
/* =============================== COPYRIGHT ============================== */
/* ======================================================================== */
/*

M37710 CPU Emulator v0.1

mame2003-plus: backported from MAME 0.121 to the 0.78 CPU interface.
The on-chip registers (0x00-0x7f), on-chip RAM (0x80-0x27f) and, for the
M37702 with internal mask ROM, the internal ROM (0xc000-0xffff) are
handled inside the core.  The on-chip I/O ports and ADC are routed to the
driver through m37710_set_io_handlers().  Single instance only.

*/

#include "osd_cpu.h"

/* ======================================================================== */
/* =============================== DEFINES ================================ */
/* ======================================================================== */
/*
   Input lines - used with cpu_set_irq_line() and the like.
   WARNING: these are in the same order as the vector table for simplicity.
   Do not alter this order!
*/

enum
{
	/* these interrupts are maskable */
	M37710_LINE_ADC = 0,
	M37710_LINE_UART1XMIT,
	M37710_LINE_UART1RECV,
	M37710_LINE_UART0XMIT,
	M37710_LINE_UART0RECV,
	M37710_LINE_TIMERB2,
	M37710_LINE_TIMERB1,
	M37710_LINE_TIMERB0,
	M37710_LINE_TIMERA4,
	M37710_LINE_TIMERA3,
	M37710_LINE_TIMERA2,
	M37710_LINE_TIMERA1,
	M37710_LINE_TIMERA0,
	M37710_LINE_IRQ2,
	M37710_LINE_IRQ1,
	M37710_LINE_IRQ0,
	/* these interrupts are non-maskable */
	M37710_LINE_WATCHDOG,
	M37710_LINE_DEBUG,
	M37710_LINE_BRK,
	M37710_LINE_ZERODIV,
	M37710_LINE_RESET,

	/* these are not interrupts, they're signals external hardware can send */
	M37710_LINE_TIMERA0TICK,
	M37710_LINE_TIMERA1TICK,
	M37710_LINE_TIMERA2TICK,
	M37710_LINE_TIMERA3TICK,
	M37710_LINE_TIMERA4TICK,
	M37710_LINE_TIMERB0TICK,
	M37710_LINE_TIMERB1TICK,
	M37710_LINE_TIMERB2TICK,

	M37710_LINE_MAX
};

/* Registers - used by m37710_set_reg() and m37710_get_reg() */
enum
{
	M37710_PC=1, M37710_S, M37710_P, M37710_A, M37710_B, M37710_X, M37710_Y,
	M37710_PB, M37710_DB, M37710_D, M37710_E,
	M37710_NMI_STATE, M37710_IRQ_STATE
};


/* I/O ports */
enum
{
	M37710_PORT0 = 0,
	M37710_PORT1, M37710_PORT2, M37710_PORT3, M37710_PORT4,
	M37710_PORT5, M37710_PORT6, M37710_PORT7, M37710_PORT8,

	M37710_ADC0_L = 0x10, M37710_ADC0_H,
	M37710_ADC1_L, M37710_ADC1_H, M37710_ADC2_L, M37710_ADC2_H, M37710_ADC3_L, M37710_ADC3_H,
	M37710_ADC4_L, M37710_ADC4_H, M37710_ADC5_L, M37710_ADC5_H, M37710_ADC6_L, M37710_ADC6_H,
	M37710_ADC7_L, M37710_ADC7_H,

	M37710_SER0_REC = 0x20,
	M37710_SER0_XMIT, M37710_SER1_REC, M37710_SER1_XMIT
};

/* ======================================================================== */
/* ============================== PROTOTYPES ============================== */
/* ======================================================================== */

typedef data8_t (*m37710_io_read_func)(int port);
typedef void (*m37710_io_write_func)(int port, data8_t data);

/* hook the on-chip ports/ADC to the driver */
void m37710_set_io_handlers(m37710_io_read_func readfn, m37710_io_write_func writefn);
/* M37702 with internal mask ROM: 16KB mapped at 0xc000-0xffff (NULL = external) */
void m37710_set_internal_rom(const UINT8 *rom);

extern int m37710_ICount;				/* cycle count */

void m37710_init(void);
void m37710_reset(void *param);
void m37710_exit(void);
int m37710_execute(int cycles);
unsigned m37710_get_context(void *dst);
void m37710_set_context(void *src);
unsigned m37710_get_reg(int regnum);
void m37710_set_reg(int regnum, unsigned val);
void m37710_set_irq_line(int irqline, int state);
void m37710_set_irq_callback(int (*callback)(int irqline));
const char *m37710_info(void *context, int regnum);
unsigned m37710_dasm(char *buffer, unsigned pc);

#define m37702_ICount           m37710_ICount
#define m37702_init             m37710_init
#define m37702_reset            m37710_reset
#define m37702_exit             m37710_exit
#define m37702_execute          m37710_execute
#define m37702_get_context      m37710_get_context
#define m37702_set_context      m37710_set_context
#define m37702_get_reg          m37710_get_reg
#define m37702_set_reg          m37710_set_reg
#define m37702_set_irq_line     m37710_set_irq_line
#define m37702_set_irq_callback m37710_set_irq_callback
#define m37702_info             m37710_info
#define m37702_dasm             m37710_dasm

/* memory access, including the on-chip resources */
UINT8 m37710_mem_read8(UINT32 address);
UINT16 m37710_mem_read16(UINT32 address);
void m37710_mem_write8(UINT32 address, UINT8 data);
void m37710_mem_write16(UINT32 address, UINT16 data);

#undef M37710_CALL_DEBUGGER

#define M37710_CALL_DEBUGGER CALL_MAME_DEBUG
#define m37710_read_8(addr)             m37710_mem_read8(addr)
#define m37710_write_8(addr,data)       m37710_mem_write8(addr,data)
#define m37710_read_8_immediate(A)      m37710_mem_read8(A)
#define m37710_read_16(addr)            m37710_mem_read16(addr)
#define m37710_write_16(addr,data)      m37710_mem_write16(addr,data)
#define m37710_jumping(A)
#define m37710_branching(A)

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* HEADER__M37710 */
