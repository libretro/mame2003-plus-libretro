/*
    Mitsubishi M37702/37710 CPU Emulator

    The 7700 series is based on the WDC 65C816 core, with the following
    notable changes:

    - Second accumulator called "B" (on the 65816, "A" and "B" were the
      two 8-bit halves of the 16-bit "C" accumulator).
    - 6502 emulation mode and XCE instruction are not present.
    - No NMI line.  BRK and the watchdog interrupt are non-maskable, but there
      is no provision for the traditional 6502/65816 NMI line.
    - 3-bit interrupt priority levels like the 68000.  Interrupts in general
      are very different from the 65816.
    - New single-instruction immediate-to-memory move instructions (LDM)
      replaces STZ.
    - CLM and SEM (clear and set "M" status bit) replace CLD/SED.  Decimal
      mode is still available via REP/SEP instructions.
    - INC and DEC (0x1A and 0x3A) switch places for no particular reason.
    - The microcode bug that caused MVN/NVP to take 2 extra cycles per byte
      on the 65816 seems to have been fixed.
    - The WDM (0x42) and BIT immediate (0x89) instructions are now prefixes.
      0x42 when used before an instruction involving the A accumulator makes
      it use the B accumulator instead.  0x89 adds multiply and divide
      opcodes, which the real 65816 doesn't have.
    - The 65C816 preserves the upper 8 bits of A when in 8-bit M mode, but
      not the upper 8 bits of X or Y when in 8-bit X.  The 7700 preserves
      the top bits of all registers in all modes (code in the C74 BIOS
      starting at d881 requires this!).

    The various 7700 series models differ primarily by their on board
    peripherals.  The 7750 and later models do include some additional
    instructions, vs. the 770x/1x/2x, notably signed multiply/divide and
    sign extension opcodes.

    Peripherals common across the 7700 series include: programmable timers,
    digital I/O ports, and analog to digital converters.

    Reference: 7700 Family Software User's Manual (instruction set)
               7702/7703 Family User's Manual (on-board peripherals)
           7720 Family User's Manual

    Emulator by R. Belmont.
    Based on G65816 Emulator by Karl Stenrud.

    History:
    - v1.0  RB  First version, basic operation OK, timers not complete
    - v1.1  RB  Data bus is 16-bit, dozens of bugfixes to IRQs, opcodes,
                    and opcode mapping.  New opcodes added, internal timers added.
    - v1.2  RB  Fixed execution outside of bank 0, fixed LDM outside of bank 0,
                fixed so top 8 bits of X & Y are preserved while in 8-bit mode,
        added save state support.
*/

#include <stdio.h>
#include <string.h>
#include "cpuintrf.h"

/* REG_PC is redefined by m37710cm.h, keep the cpuintrf value */
enum { CPUINTRF_REG_PC = REG_PC };

#include "m37710cm.h"
#include "state.h"

#define M37710_DEBUG	(0)	// enables verbose logging for peripherals, etc.

void m37710_set_irq_line(int line, int state);

/* on-chip RAM (0x80-0x27f), internal mask ROM (0xc000-0xffff) and I/O hooks */
static UINT8 m37710_iram[0x200];
static const UINT8 *m37710_irom = NULL;
static int m37710_clock = 16000000;
static m37710_io_read_func  m37710_io_read  = NULL;
static m37710_io_write_func m37710_io_write = NULL;

void m37710_set_io_handlers(m37710_io_read_func readfn, m37710_io_write_func writefn)
{
	m37710_io_read = readfn;
	m37710_io_write = writefn;
}

void m37710_set_internal_rom(const UINT8 *rom)
{
	m37710_irom = rom;
}

static data8_t io_read_byte_8(int port)
{
	return m37710_io_read ? m37710_io_read(port) : 0xff;
}

static void io_write_byte_8(int port, data8_t data)
{
	if (m37710_io_write)
		m37710_io_write(port, data);
}

/* Our CPU structure */
m37710i_cpu_struct m37710i_cpu = {0};

int m37710_ICount = 0, m37710_fullCount = 0;

/* Temporary Variables */
uint m37710i_source;
uint m37710i_destination;

/* interrupt control mapping */

int m37710_irq_levels[M37710_LINE_MAX] =
{
	// maskable
	0x70,	// ADC
	0x73,	// UART 1 XMIT
	0x74,	// UART 1 RECV
	0x71,	// UART 0 XMIT
	0x72,	// UART 0 RECV
	0x7c,	// Timer B2
	0x7b,	// Timer B1
	0x7a,	// Timer B0
	0x79,	// Timer A4
	0x78,	// Timer A3
	0x77,	// Timer A2
	0x76,	// Timer A1
	0x75,	// Timer A0
	0x7f,	// IRQ 2
	0x7e,	// IRQ 1
	0x7d,	// IRQ 0

	// non-maskable
	0,	// watchdog
	0,	// debugger control
	0,	// BRK
	0,	// divide by zero
	0,	// reset
};

static int m37710_irq_vectors[M37710_LINE_MAX] =
{
	// maskable          C74
	0xffd6, // A-D converter     c68b
	0xffd8, // UART1 transmit    c68e
	0xffda, // UART1 receive     c691
	0xffdc, // UART0 transmit    c694
	0xffde,	// UART0 receive     c697
	0xffe0, // Timer B2      c69a
	0xffe2, // Timer B1      c69d
	0xffe4, // Timer B0      c6a0
	0xffe6, // Timer A4      c6a3
	0xffe8, // Timer A3      c6a6
	0xffea, // Timer A2      c6a9
	0xffec, // Timer A1      c6ac
	0xffee, // Timer A0      c6af
	0xfff0, // external INT2 pin c6b2
	0xfff2, // external INT1 pin c6b5
	0xfff4, // external INT0 pin c6b8

	// non-maskable
	0xfff6, // watchdog timer
	0xfff8,	// debugger control (not used in shipping ICs?)
	0xfffa, // BRK
	0xfffc, // divide by zero
	0xfffe,	// RESET
};

// M37710 internal peripherals

#if M37710_DEBUG
static const char *m37710_rnames[128] =
{
	"",
	"",
	"Port P0 reg",
	"Port P1 reg",
	"Port P0 dir reg",
	"Port P1 dir reg",
	"Port P2 reg",
	"Port P3 reg",
	"Port P2 dir reg",
	"Port P3 dir reg",
	"Port P4 reg",
	"Port P5 reg",
	"Port P4 dir reg",
	"Port P5 dir reg",
	"Port P6 reg",
	"Port P7 reg",
	"Port P6 dir reg",	// 16
	"Port P7 dir reg",
	"Port P8 reg",
	"",
	"Port P8 dir reg",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"A/D control reg",
	"A/D sweep pin select",
	"A/D 0",		// 32 (0x20)
	"",
	"A/D 1",
	"",
	"A/D 2",
	"",
	"A/D 3",
	"",
	"A/D 4",
	"",
	"A/D 5",
	"",
	"A/D 6",
	"",
	"A/D 7",
	"",
	"UART0 transmit/recv mode", 	// 48 (0x30)
	"UART0 baud rate",	     	// 0x31
	"UART0 transmit buf L",		// 0x32
	"UART0 transmit buf H",		// 0x33
	"UART0 transmit/recv ctrl 0",	// 0x34
	"UART0 transmit/recv ctrl 1",	// 0x35
	"UART0 recv buf L",		// 0x36
	"UART0 recv buf H",		// 0x37
	"UART1 transmit/recv mode",	// 0x38
	"UART1 baud rate",
	"UART1 transmit buf L",
	"UART1 transmit buf H",
	"UART1 transmit/recv ctrl 0",
	"UART1 transmit/recv ctrl 1",
	"UART1 recv buf L",
	"UART1 recv buf H",
	"Count start",
	"",
	"One-shot start",
	"",
	"Up-down register",
	"",
	"Timer A0 L",
	"Timer A0 H",
	"Timer A1 L",
	"Timer A1 H",
	"Timer A2 L",
	"Timer A2 H",
	"Timer A3 L",
	"Timer A3 H",
	"Timer A4 L",
	"Timer A4 H",
	"Timer B0 L",
	"Timer B0 H",
	"Timer B1 L",
	"Timer B1 H",
	"Timer B2 L",
	"Timer B2 H",
	"Timer A0 mode",
	"Timer A1 mode",
	"Timer A2 mode",
	"Timer A3 mode",
	"Timer A4 mode",
	"Timer B0 mode",
	"Timer B1 mode",
	"Timer B2 mode",
	"Processor mode",
	"",
	"Watchdog reset",
	"Watchdog frequency",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"A/D IRQ ctrl",
	"UART0 xmit IRQ ctrl",
	"UART0 recv IRQ ctrl",
	"UART1 xmit IRQ ctrl",
	"UART1 recv IRQ ctrl",
	"Timer A0 IRQ ctrl",
	"Timer A1 IRQ ctrl",
	"Timer A2 IRQ ctrl",
	"Timer A3 IRQ ctrl",
	"Timer A4 IRQ ctrl",
	"Timer B0 IRQ ctrl",
	"Timer B1 IRQ ctrl",
	"Timer B2 IRQ ctrl",
	"INT0 IRQ ctrl",
	"INT1 IRQ ctrl",
	"INT2 IRQ ctrl",
};

static const char *m37710_tnames[8] =
{
	"A0", "A1", "A2", "A3", "A4", "B0", "B1", "B2"
};
#endif

static void m37710_timer_a0_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[0], m37710i_cpu.reload[0], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERA0, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_a1_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[1], m37710i_cpu.reload[1], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERA1, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_a2_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[2], m37710i_cpu.reload[2], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERA2, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_a3_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[3], m37710i_cpu.reload[3], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERA3, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_a4_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[4], m37710i_cpu.reload[4], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERA4, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_b0_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[5], m37710i_cpu.reload[5], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERB0, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_b1_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[6], m37710i_cpu.reload[6], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERB1, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_timer_b2_cb(int param)
{
	int cpunum = param;

	cpuintrf_push_context(cpunum);
	timer_adjust(m37710i_cpu.timers[7], m37710i_cpu.reload[7], cpunum, 0);

	m37710_set_irq_line(M37710_LINE_TIMERB2, ASSERT_LINE);
	cpu_triggerint(cpunum);
	cpuintrf_pop_context();
}

static void m37710_external_tick(int timer, int state)
{
	// we only care if the state is "on"
	if (!state)
	{
		return;
	}

	// check if enabled
	if (m37710i_cpu.m37710_regs[0x40] & (1<<timer))
	{
		if ((m37710i_cpu.m37710_regs[0x56+timer] & 0x3) == 1)
		{
			if (m37710i_cpu.m37710_regs[0x46+(timer*2)] == 0xff)
			{
				m37710i_cpu.m37710_regs[0x46+(timer*2)] = 0;
				m37710i_cpu.m37710_regs[0x46+(timer*2)+1]++;
			}
			else
			{
				m37710i_cpu.m37710_regs[0x46+(timer*2)]++;
			}
		}
		else
		{
			log_cb(RETRO_LOG_DEBUG, LOGPRE "M37710: external tick for timer %d, not in event counter mode!\n", timer);
		}
	}
}

static void m37710_recalc_timer(int timer)
{
	int cpunum = cpu_getactivecpu();
	int tval;
	static const int tcr[8] = { 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d };
	double time;
	static const int tscales[4] = { 2, 16, 64, 512 };

	// check if enabled
	if (m37710i_cpu.m37710_regs[0x40] & (1<<timer))
	{
		#if M37710_DEBUG
		log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d (%s) is enabled\n", timer, m37710_tnames[timer]);
		#endif

		// set the timer's value
		tval = m37710i_cpu.m37710_regs[0x46+(timer*2)] | (m37710i_cpu.m37710_regs[0x47+(timer*2)]<<8);

		// HACK (current MAME): ignore if timer is 8MHz
		if (tval == 0 && (m37710i_cpu.m37710_regs[0x56+timer] & 0xc0) == 0) return;

		// check timer's mode
		// modes are slightly different between timer groups A and B
		if (timer < 5)
		{
			switch (m37710i_cpu.m37710_regs[0x56+timer] & 0x3)
			{
				case 0:	      	// timer mode
					time = TIME_IN_HZ(m37710_clock) * tscales[m37710i_cpu.m37710_regs[tcr[timer]]>>6];
					time = time * (tval + 1);

					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in timer mode, %f Hz\n", timer, 1.0 / time);
					#endif

					timer_adjust(m37710i_cpu.timers[timer], time, cpunum, 0);
					m37710i_cpu.reload[timer] = time;
					break;

				case 1:	      	// event counter mode
					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in event counter mode\n", timer);
					#endif
					break;

				case 2:		// one-shot pulse mode
					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in one-shot mode\n", timer);
					#endif
					break;

				case 3:	      	// PWM mode
					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in PWM mode\n", timer);
					#endif
					break;
			}
		}
		else
		{
			switch (m37710i_cpu.m37710_regs[0x56+timer] & 0x3)
			{
				case 0:	      	// timer mode
					time = TIME_IN_HZ(m37710_clock) * tscales[m37710i_cpu.m37710_regs[tcr[timer]]>>6];
					time = time * (tval + 1);

					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in timer mode, %f Hz\n", timer, 1.0 / time);
					#endif

					timer_adjust(m37710i_cpu.timers[timer], time, cpunum, 0);
					m37710i_cpu.reload[timer] = time;
					break;

				case 1:	      	// event counter mode
					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in event counter mode\n", timer);
					#endif
					break;

				case 2:		// pulse period/pulse width measurement mode
					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in pulse period/width measurement mode\n", timer);
					#endif
					break;

				case 3:
					#if M37710_DEBUG
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Timer %d in unknown mode!\n", timer);
					#endif
					break;
			}
		}
	}
}

/* port read as in modern MAME: input bits from the driver, output bits from the latch
   (the direction register follows the data register by 2) */
static UINT8 m37710_port_r(int offset, int port)
{
	UINT8 d = m37710i_cpu.m37710_regs[offset + 2];
	return (io_read_byte_8(port) & ~d) | (m37710i_cpu.m37710_regs[offset] & d);
}

static UINT8 m37710_internal_r(int offset)
{
	#if M37710_DEBUG
	if (offset > 1)
	log_cb(RETRO_LOG_DEBUG, LOGPRE "m37710_internal_r from %02x: %s (PC=%x)\n", (int)offset, m37710_rnames[(int)offset], REG_PB<<16 | REG_PC);
	#endif

	switch (offset)
	{
		case 2: // p0
			return m37710_port_r(2, M37710_PORT0);
			break;
		case 3: // p1
			return m37710_port_r(3, M37710_PORT1);
			break;
		case 6: // p2
			return m37710_port_r(6, M37710_PORT2);
			break;
		case 7: // p3
			return m37710_port_r(7, M37710_PORT3);
			break;
		case 0xa: // p4
			return m37710_port_r(0xa, M37710_PORT4);
			break;
		case 0xb: // p5
			return m37710_port_r(0xb, M37710_PORT5);
			break;
		case 0xe: // p6
			return m37710_port_r(0xe, M37710_PORT6);
			break;
		case 0xf: // p7
			return m37710_port_r(0xf, M37710_PORT7);
			break;
		case 0x12: // p8
			return m37710_port_r(0x12, M37710_PORT8);
			break;

		case 0x20:
			return io_read_byte_8(M37710_ADC0_L);
			break;
		case 0x21:
			return io_read_byte_8(M37710_ADC0_H);
			break;
		case 0x22:
			return io_read_byte_8(M37710_ADC1_L);
			break;
		case 0x23:
			return io_read_byte_8(M37710_ADC1_H);
			break;
		case 0x24:
			return io_read_byte_8(M37710_ADC2_L);
			break;
		case 0x25:
			return io_read_byte_8(M37710_ADC2_H);
			break;
		case 0x26:
			return io_read_byte_8(M37710_ADC3_L);
			break;
		case 0x27:
			return io_read_byte_8(M37710_ADC3_H);
			break;
		case 0x28:
			return io_read_byte_8(M37710_ADC4_L);
			break;
		case 0x29:
			return io_read_byte_8(M37710_ADC4_H);
			break;
		case 0x2a:
			return io_read_byte_8(M37710_ADC5_L);
			break;
		case 0x2b:
			return io_read_byte_8(M37710_ADC5_H);
			break;
		case 0x2c:
			return io_read_byte_8(M37710_ADC6_L);
			break;
		case 0x2d:
			return io_read_byte_8(M37710_ADC6_H);
			break;
		case 0x2e:
			return io_read_byte_8(M37710_ADC7_L);
			break;
		case 0x2f:
			return io_read_byte_8(M37710_ADC7_H);
			break;
		case 0x35:
			return 0xff;	// UART control
			break;


	}

	return m37710i_cpu.m37710_regs[offset];
}

static void m37710_internal_w(int offset, UINT8 data)
{
	int i;

	switch(offset)
	{
		case 2: // p0
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT0, data);
			return;
			break;
		case 3: // p1
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT1, data);
			return;
			break;
		case 6: // p2
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT2, data);
			return;
			break;
		case 7: // p3
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT3, data);
			return;
			break;
		case 0xa: // p4
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT4, data);
			return;
			break;
		case 0xb: // p5
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT5, data);
			return;
			break;
		case 0xe: // p6
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT6, data);
			return;
			break;
		case 0xf: // p7
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT7, data);
			return;
			break;
		case 0x12: // p8
			m37710i_cpu.m37710_regs[offset] = data;
			io_write_byte_8(M37710_PORT8, data);
			return;
			break;

		case 0x40:	// count start
			for (i = 0; i < 8; i++)
			{
				if ((data & (1<<i)) && !(m37710i_cpu.m37710_regs[offset] & (1<<i)))
				{
					m37710i_cpu.m37710_regs[offset] |= (1<<i);
					m37710_recalc_timer(i);
				}
			}

			m37710i_cpu.m37710_regs[offset] = data;

			return;
			break;

		case 0x60:     	// watchdog reset
			return;
			break;

		case 0x1e:	// A/D control: conversions complete immediately
			if ((data & 0x40) && !(data & 0x08))
			{
				data &= ~0x40;
				m37710i_cpu.m37710_regs[offset] = data;
				m37710_set_irq_line(M37710_LINE_ADC, ASSERT_LINE);
				return;
			}
			break;
	}

	m37710i_cpu.m37710_regs[offset] = data;

	if (offset >= 0x70 && offset <= 0x7f)
		m37710i_update_irqs();

	#if M37710_DEBUG
	if (offset >= 0x1e && offset <= 0x40)
	log_cb(RETRO_LOG_DEBUG, LOGPRE "m37710_internal_w %x to %02x: %s = %x\n", data, (int)offset, m37710_rnames[(int)offset], m37710i_cpu.m37710_regs[offset]);
	#endif
}

/* memory access including the on-chip resources */
UINT8 m37710_mem_read8(UINT32 address)
{
	address &= 0xffffff;
	if (address < 0x80)
		return m37710_internal_r(address);
	if (address < 0x280)
		return m37710_iram[address - 0x80];
	if (m37710_irom && address >= 0xc000 && address < 0x10000)
		return m37710_irom[address - 0xc000];
	return cpu_readmem24lew(address);
}

UINT16 m37710_mem_read16(UINT32 address)
{
	address &= 0xffffff;
	if (address < 0x280 || (m37710_irom && address >= 0xc000 && address < 0x10000))
		return m37710_mem_read8(address) | (m37710_mem_read8(address + 1) << 8);
	return cpu_readmem24lew_word(address);
}

void m37710_mem_write8(UINT32 address, UINT8 data)
{
	address &= 0xffffff;
	if (address < 0x80)
	{
		m37710_internal_w(address, data);
		return;
	}
	if (address < 0x280)
	{
		m37710_iram[address - 0x80] = data;
		return;
	}
	if (m37710_irom && address >= 0xc000 && address < 0x10000)
		return;
	cpu_writemem24lew(address, data);
}

void m37710_mem_write16(UINT32 address, UINT16 data)
{
	address &= 0xffffff;
	if (address < 0x280 || (m37710_irom && address >= 0xc000 && address < 0x10000))
	{
		m37710_mem_write8(address, data & 0xff);
		m37710_mem_write8(address + 1, data >> 8);
		return;
	}
	cpu_writemem24lew_word(address, data);
}

extern void (*m37710i_opcodes_M0X0[])(void);
extern void (*m37710i_opcodes42_M0X0[])(void);
extern void (*m37710i_opcodes89_M0X0[])(void);
extern uint m37710i_get_reg_M0X0(int regnum);
extern void m37710i_set_reg_M0X0(int regnum, uint val);
extern void m37710i_set_line_M0X0(int line, int state);
extern int  m37710i_execute_M0X0(int cycles);

extern void (*m37710i_opcodes_M0X1[])(void);
extern void (*m37710i_opcodes42_M0X1[])(void);
extern void (*m37710i_opcodes89_M0X1[])(void);
extern uint m37710i_get_reg_M0X1(int regnum);
extern void m37710i_set_reg_M0X1(int regnum, uint val);
extern void m37710i_set_line_M0X1(int line, int state);
extern int  m37710i_execute_M0X1(int cycles);

extern void (*m37710i_opcodes_M1X0[])(void);
extern void (*m37710i_opcodes42_M1X0[])(void);
extern void (*m37710i_opcodes89_M1X0[])(void);
extern uint m37710i_get_reg_M1X0(int regnum);
extern void m37710i_set_reg_M1X0(int regnum, uint val);
extern void m37710i_set_line_M1X0(int line, int state);
extern int  m37710i_execute_M1X0(int cycles);

extern void (*m37710i_opcodes_M1X1[])(void);
extern void (*m37710i_opcodes42_M1X1[])(void);
extern void (*m37710i_opcodes89_M1X1[])(void);
extern uint m37710i_get_reg_M1X1(int regnum);
extern void m37710i_set_reg_M1X1(int regnum, uint val);
extern void m37710i_set_line_M1X1(int line, int state);
extern int  m37710i_execute_M1X1(int cycles);

void (**m37710i_opcodes[4])(void) =
{
	m37710i_opcodes_M0X0,
	m37710i_opcodes_M0X1,
	m37710i_opcodes_M1X0,
	m37710i_opcodes_M1X1,
};

void (**m37710i_opcodes2[4])(void) =
{
	m37710i_opcodes42_M0X0,
	m37710i_opcodes42_M0X1,
	m37710i_opcodes42_M1X0,
	m37710i_opcodes42_M1X1,
};

void (**m37710i_opcodes3[4])(void) =
{
	m37710i_opcodes89_M0X0,
	m37710i_opcodes89_M0X1,
	m37710i_opcodes89_M1X0,
	m37710i_opcodes89_M1X1,
};

uint (*m37710i_get_reg[4])(int regnum) =
{
	m37710i_get_reg_M0X0,
	m37710i_get_reg_M0X1,
	m37710i_get_reg_M1X0,
	m37710i_get_reg_M1X1,
};

void (*m37710i_set_reg[4])(int regnum, uint val) =
{
	m37710i_set_reg_M0X0,
	m37710i_set_reg_M0X1,
	m37710i_set_reg_M1X0,
	m37710i_set_reg_M1X1,
};

void (*m37710i_set_line[4])(int line, int state) =
{
	m37710i_set_line_M0X0,
	m37710i_set_line_M0X1,
	m37710i_set_line_M1X0,
	m37710i_set_line_M1X1,
};

int (*m37710i_execute[4])(int cycles) =
{
	m37710i_execute_M0X0,
	m37710i_execute_M0X1,
	m37710i_execute_M1X0,
	m37710i_execute_M1X1,
};

/* internal functions */

static INLINE void m37710i_push_8(uint value)
{
	m37710_write_8(REG_S, value);
	REG_S = MAKE_UINT_16(REG_S-1);
}

static INLINE void m37710i_push_16(uint value)
{
	m37710i_push_8(value>>8);
	m37710i_push_8(value&0xff);
}

static INLINE uint m37710i_get_reg_p(void)
{
	return	(FLAG_N&0x80)		|
			((FLAG_V>>1)&0x40)	|
			FLAG_M				|
			FLAG_X				|
			FLAG_D				|
			FLAG_I				|
			((!FLAG_Z)<<1)		|
			((FLAG_C>>8)&1);
}

void m37710i_update_irqs(void)
{
	/* interrupt selection from current MAME */
	int curirq, pending = LINE_IRQ;
	int wantedIRQ = -1;
	int curpri = 0;

	for (curirq = M37710_LINE_WATCHDOG + 4; curirq >= 0; curirq--)
	{
		if ((pending & (1 << curirq)))
		{
			// this IRQ is set
			if (m37710_irq_levels[curirq])
			{
				int thispri = m37710i_cpu.m37710_regs[m37710_irq_levels[curirq]] & 7;
				// it's maskable, check if the level works, also make sure it's acceptable for the current CPU level
				if (!FLAG_I && thispri > curpri && thispri > (int)m37710i_cpu.ipl)
				{
					// mark us as the best candidate
					wantedIRQ = curirq;
					curpri = thispri;
				}
			}
			else
			{
				// non-maskable
				wantedIRQ = curirq;
				curpri = 7;
				break;	// no more processing, NMIs always win
			}
		}
	}

	if (wantedIRQ != -1)
	{
		if (INT_ACK) INT_ACK(wantedIRQ);

		// make sure we're running to service the interrupt
		CPU_STOPPED &= ~STOP_LEVEL_WAI;

		// auto-clear line
		m37710_set_irq_line(wantedIRQ, CLEAR_LINE);

		// let's do it...
		// push PB, then PC, then status
		CLK(13);
		m37710i_push_8(REG_PB>>16);
		m37710i_push_16(REG_PC);
		m37710i_push_8(m37710i_cpu.ipl);
		m37710i_push_8(m37710i_get_reg_p());

		// set I to 1, set IPL to the interrupt we're taking
		FLAG_I = IFLAG_SET;
		m37710i_cpu.ipl = curpri;
		// then PB=0, PC=(vector)
		REG_PB = 0;
		REG_PC = m37710_read_8(m37710_irq_vectors[wantedIRQ]) |
			 m37710_read_8(m37710_irq_vectors[wantedIRQ]+1)<<8;
		m37710i_jumping(REG_PB | REG_PC);
	}
}

/* external functions */

void m37710_reset(void *param)
{
	m37710_clock = Machine->drv->cpu[cpu_getactivecpu()].cpu_clock;

	/* Start the CPU */
	CPU_STOPPED = 0;

	/* 37710 boots in full native mode */
	REG_D = 0;
	REG_PB = 0;
	REG_DB = 0;
	REG_S = (REG_S & 0xff) | 0x100;
	REG_X &= 0xff;
	REG_Y &= 0xff;
	if(!FLAG_M)
	{
		REG_B = REG_A & 0xff00;
		REG_A &= 0xff;
	}
	FLAG_M = MFLAG_CLEAR;
	FLAG_X = XFLAG_CLEAR;

	/* Clear D and set I */
	FLAG_D = DFLAG_CLEAR;
	FLAG_I = IFLAG_SET;

	/* Clear all pending interrupts (should we really do this?) */
	LINE_IRQ = 0;
	IRQ_DELAY = 0;

	/* Set the function tables to emulation mode */
	m37710i_set_execution_mode(EXECUTION_MODE_M0X0);

	FLAG_Z = ZFLAG_CLEAR;
	REG_S = 0x1ff;

	/* Fetch the reset vector */
	REG_PC = m37710_read_8(0xfffe) | (m37710_read_8(0xffff)<<8);
	m37710i_jumping(REG_PB | REG_PC);
}

/* Exit and clean up */
void m37710_exit(void)
{
	/* nothing to do yet */
}

/* Execute some instructions */
int m37710_execute(int cycles)
{
	m37710_fullCount = cycles;

	m37710i_update_irqs();

	return FTABLE_EXECUTE(cycles);
}

/* single instance: the state lives in m37710i_cpu, contexts are not swapped */
unsigned m37710_get_context(void *dst)
{
	return 1;
}

void m37710_set_context(void *src)
{
}

/* Get a register */
unsigned m37710_get_reg(int regnum)
{
	switch (regnum)
	{
		case CPUINTRF_REG_PC: return REG_PB | REG_PC;
		case REG_PREVIOUSPC:  return REG_PPC;
		case REG_SP:          return REG_S;
		case M37710_P:        return FTABLE_GET_REG(M37710_P) | (m37710i_cpu.ipl<<8);
	}
	return FTABLE_GET_REG(regnum);
}

/* Set a register */
void m37710_set_reg(int regnum, unsigned value)
{
	switch (regnum)
	{
		case CPUINTRF_REG_PC:
			REG_PB = value & 0xff0000;
			REG_PC = MAKE_UINT_16(value);
			return;
		case REG_SP:
			REG_S = MAKE_UINT_16(value);
			return;
		case M37710_P:
			FTABLE_SET_REG(M37710_P, value & 0xff);
			m37710i_cpu.ipl = (value>>8)&0xff;
			return;
	}
	FTABLE_SET_REG(regnum, value);
}

/* Set an interrupt line */
void m37710_set_irq_line(int line, int state)
{
	if (line >= M37710_LINE_TIMERA0TICK && line <= M37710_LINE_TIMERB2TICK)
	{
		m37710_external_tick(line - M37710_LINE_TIMERA0TICK, state);
		return;
	}
	FTABLE_SET_LINE(line, state);
}

/* Set the callback that is called when servicing an interrupt */
void m37710_set_irq_callback(int (*callback)(int))
{
	INT_ACK = callback;
}

unsigned m37710_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%02X", m37710_read_8(pc));
	return 1;
}

static void m37710_restore_state(void)
{
	/* restore proper function pointers */
	m37710i_set_execution_mode((FLAG_M>>4) | (FLAG_X>>4));
}

void m37710_init(void)
{
	int index = cpu_getactivecpu();
	char name[16];
	int i;

	memset(m37710_iram, 0, sizeof(m37710_iram));

	m37710i_cpu.timers[0] = timer_alloc(m37710_timer_a0_cb);
	m37710i_cpu.timers[1] = timer_alloc(m37710_timer_a1_cb);
	m37710i_cpu.timers[2] = timer_alloc(m37710_timer_a2_cb);
	m37710i_cpu.timers[3] = timer_alloc(m37710_timer_a3_cb);
	m37710i_cpu.timers[4] = timer_alloc(m37710_timer_a4_cb);
	m37710i_cpu.timers[5] = timer_alloc(m37710_timer_b0_cb);
	m37710i_cpu.timers[6] = timer_alloc(m37710_timer_b1_cb);
	m37710i_cpu.timers[7] = timer_alloc(m37710_timer_b2_cb);

	state_save_register_UINT32("M377xx", index, "a",        &m37710i_cpu.a, 1);
	state_save_register_UINT32("M377xx", index, "b",        &m37710i_cpu.b, 1);
	state_save_register_UINT32("M377xx", index, "ba",       &m37710i_cpu.ba, 1);
	state_save_register_UINT32("M377xx", index, "bb",       &m37710i_cpu.bb, 1);
	state_save_register_UINT32("M377xx", index, "x",        &m37710i_cpu.x, 1);
	state_save_register_UINT32("M377xx", index, "y",        &m37710i_cpu.y, 1);
	state_save_register_UINT32("M377xx", index, "s",        &m37710i_cpu.s, 1);
	state_save_register_UINT32("M377xx", index, "pc",       &m37710i_cpu.pc, 1);
	state_save_register_UINT32("M377xx", index, "ppc",      &m37710i_cpu.ppc, 1);
	state_save_register_UINT32("M377xx", index, "pb",       &m37710i_cpu.pb, 1);
	state_save_register_UINT32("M377xx", index, "db",       &m37710i_cpu.db, 1);
	state_save_register_UINT32("M377xx", index, "d",        &m37710i_cpu.d, 1);
	state_save_register_UINT32("M377xx", index, "flag_e",   &m37710i_cpu.flag_e, 1);
	state_save_register_UINT32("M377xx", index, "flag_m",   &m37710i_cpu.flag_m, 1);
	state_save_register_UINT32("M377xx", index, "flag_x",   &m37710i_cpu.flag_x, 1);
	state_save_register_UINT32("M377xx", index, "flag_n",   &m37710i_cpu.flag_n, 1);
	state_save_register_UINT32("M377xx", index, "flag_v",   &m37710i_cpu.flag_v, 1);
	state_save_register_UINT32("M377xx", index, "flag_d",   &m37710i_cpu.flag_d, 1);
	state_save_register_UINT32("M377xx", index, "flag_i",   &m37710i_cpu.flag_i, 1);
	state_save_register_UINT32("M377xx", index, "flag_z",   &m37710i_cpu.flag_z, 1);
	state_save_register_UINT32("M377xx", index, "flag_c",   &m37710i_cpu.flag_c, 1);
	state_save_register_UINT32("M377xx", index, "line_irq", &m37710i_cpu.line_irq, 1);
	state_save_register_UINT32("M377xx", index, "ipl",      &m37710i_cpu.ipl, 1);
	state_save_register_UINT32("M377xx", index, "ir",       &m37710i_cpu.ir, 1);
	state_save_register_UINT32("M377xx", index, "im",       &m37710i_cpu.im, 1);
	state_save_register_UINT32("M377xx", index, "im2",      &m37710i_cpu.im2, 1);
	state_save_register_UINT32("M377xx", index, "im3",      &m37710i_cpu.im3, 1);
	state_save_register_UINT32("M377xx", index, "im4",      &m37710i_cpu.im4, 1);
	state_save_register_UINT32("M377xx", index, "irq_delay",&m37710i_cpu.irq_delay, 1);
	state_save_register_UINT32("M377xx", index, "irq_level",&m37710i_cpu.irq_level, 1);
	state_save_register_UINT32("M377xx", index, "stopped",  &m37710i_cpu.stopped, 1);
	state_save_register_UINT8 ("M377xx", index, "regs",     m37710i_cpu.m37710_regs, 128);
	state_save_register_UINT8 ("M377xx", index, "iram",     m37710_iram, sizeof(m37710_iram));
	for (i = 0; i < 8; i++)
	{
		sprintf(name, "reload%d", i);
		state_save_register_double("M377xx", index, name, &m37710i_cpu.reload[i], 1);
	}

	state_save_register_func_postload(m37710_restore_state);
}

const char *m37710_info(void *context, int regnum)
{
	static char buffer[16][47+1];
	static int which = 0;
	m37710i_cpu_struct *r = &m37710i_cpu;

	which = (which+1) % 16;
	buffer[which][0] = '\0';

	switch (regnum)
	{
		case CPU_INFO_REG+M37710_PC: sprintf(buffer[which], "PC:%04X", r->pc); break;
		case CPU_INFO_REG+M37710_PB: sprintf(buffer[which], "PB:%02X", r->pb>>16); break;
		case CPU_INFO_REG+M37710_DB: sprintf(buffer[which], "DB:%02X", r->db>>16); break;
		case CPU_INFO_REG+M37710_D:  sprintf(buffer[which], "D:%04X", r->d); break;
		case CPU_INFO_REG+M37710_S:  sprintf(buffer[which], "S:%04X", r->s); break;
		case CPU_INFO_REG+M37710_A:  sprintf(buffer[which], "A:%04X", r->a | r->b); break;
		case CPU_INFO_REG+M37710_B:  sprintf(buffer[which], "B:%04X", r->ba | r->bb); break;
		case CPU_INFO_REG+M37710_X:  sprintf(buffer[which], "X:%04X", r->x); break;
		case CPU_INFO_REG+M37710_Y:  sprintf(buffer[which], "Y:%04X", r->y); break;
		case CPU_INFO_FLAGS:
			sprintf(buffer[which], "%c%c%c%c%c%c%c%c",
				r->flag_n & NFLAG_SET ? 'N':'.',
				r->flag_v & VFLAG_SET ? 'V':'.',
				r->flag_m & MFLAG_SET ? 'M':'.',
				r->flag_x & XFLAG_SET ? 'X':'.',
				r->flag_d & DFLAG_SET ? 'D':'.',
				r->flag_i & IFLAG_SET ? 'I':'.',
				r->flag_z == 0        ? 'Z':'.',
				r->flag_c & CFLAG_SET ? 'C':'.');
			break;
		case CPU_INFO_NAME: return "M37702";
		case CPU_INFO_FAMILY: return "M7700";
		case CPU_INFO_VERSION: return "1.2";
		case CPU_INFO_FILE: return __FILE__;
		case CPU_INFO_CREDITS: return "Copyright (c) 2004-2006 R. Belmont, based on G65816 by Karl Stenerud";
	}
	return buffer[which];
}

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
