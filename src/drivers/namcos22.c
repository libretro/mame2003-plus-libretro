/***************************************************************************

    Namco System 22 / Super System 22

    Backported from current MAME (driver by Phil Stroffolino, hap,
    R. Belmont and others, BSD-3-Clause) to the mame2003-plus core, C89.

    Hardware
    --------
    - main CPU: 68020 (System 22) / 68EC020 (Super), 24.576 MHz
    - two Namco C71 DSPs (TMS320C25 with internal ROM), 40 MHz:
      the master transforms the display list, the slave lights and
      forwards it to the render device. As in MAME, the slave program
      runs but its output is not used: the 3D scene is built from the
      display list in polygon RAM (see the video file).
    - sound/IO: M37710S4 (Super) / C74 (System 22) MCU with a C352
    - video: polygons, sprites (Super only) and a text layer

    MAME's notes on the hardware are kept where the code is.

***************************************************************************/

#include <math.h>
#include <stdlib.h>
#include "driver.h"
#include "state.h"
#include "cpu/namcoc71/c71.h"
#include "cpu/m37710/m37710.h"
#include "sound/c352.h"
#include "includes/namcos22.h"

#define MCU_SPEEDUP		1
#define C74_IDLE	0xc0dd		/* C74 BIOS idle loop */


/* screen: 25.6 MHz pixel clock, 814 x 525 total, 640 x 480 visible */
#define S22_REFRESH		(25600000.0 / (814.0 * 525.0))
#define S22_VBLANK_USEC	(45.0 / 525.0 / S22_REFRESH * 1000000.0)

/* CPU numbers */
#define CPU_MAIN	0
#define CPU_MASTER	1
#define CPU_SLAVE	2
#define CPU_MCU		3

/* input port indices (Super System 22 games) */
#define PORT_INPUTS		0
#define PORT_DSW		1
#define PORT_ADC0		2		/* 4 ports, or the light gun */

int namcos22_gametype;
int namcos22_is_ss22;

data32_t *namcos22_polygonram;
data32_t *namcos22_spriteram;
data32_t *namcos22_vics_data;
data32_t *namcos22_vics_control;
data32_t *namcos22_mixer;
data32_t *namcos22_paletteram;
data32_t *namcos22_cgram;
data32_t *namcos22_czram;
data16_t *namcos22_shareram;
data8_t *namcos22_gamma_proms;

int namcos22_pdp_render_done;
int namcos22_render_refresh;
int namcos22_slave_simulation_active;
UINT32 namcos22_pdp_frame;
UINT8 namcos22_syscontrol[0x20];
int namcos22_irq_enabled, namcos22_irq_state;

static data32_t *workram;
static data32_t *sci_buffer;
static data8_t *eeprom;
static data16_t *master_extram;		/* 0x4000 words: data and program 0x4000-0x7fff */
static data16_t *master_intram;		/* 0x3000 words: data 0x1000-0x3fff */
static data16_t *slave_extram;		/* 0x2000 words: data and program 0x8000-0x9fff */
static UINT32 *pointram;			/* 0x20000 */
static INT32 *pointrom;
static int pointrom_size;

static int dsp_irq_enabled;
static UINT16 dsp_master_bioz;
static UINT32 point_address;
static UINT32 point_data;
static UINT16 serial_slave_to_master_next;
static UINT16 serial_slave_to_master_current;
static int renderbuf_size;
static UINT16 renderbuf_data[0x1c];
static int dsp_upload_state;
static int upload_dest_idx;
static UINT16 dspram_bank;
static UINT16 dspram16_latch;
static UINT16 pdp_base;
static UINT16 keycus_rng;
static UINT8 mcu_iocontrol;
static UINT8 mcu_outdata;

static mame_timer *mcu_irq2_timer;
static mame_timer *dsp_serial_timer;

enum { DSP_UPLOAD_READY, DSP_UPLOAD_DEST, DSP_UPLOAD_DATA };

#define MAX_RENDER_CMD_SEQ 0x1c


/***************************************************************************
    Main CPU
***************************************************************************/

/* KEYCUS: protection and random numbers */
static UINT16 keycus_word_r(int offset)
{
	UINT16 old_rng;

	switch (namcos22_gametype)
	{
		case NAMCOS22_RIDGE_RACER2:   if (offset == 0) return 0x0172; break;
		case NAMCOS22_ACE_DRIVER:     if (offset == 3) return 0x0173; break;
		case NAMCOS22_CYBER_COMMANDO: if (offset == 1) return 0x0185; break;
		case NAMCOS22_ALPINE_RACER:   if (offset == 1) return 0x0187; break;
		case NAMCOS22_CYBER_CYCLES:   if (offset == 0xf) return 0x0387; break;
		case NAMCOS22_VICTORY_LAP:    if (offset == 2) return 0x0188; break;
		case NAMCOS22_DIRT_DASH:      if (offset == 0) return 0x01a2; break;
		case NAMCOS22_ALPINE_SURFER:  if (offset == 1) return 0x01a9; break;
		case NAMCOS22_TOKYO_WARS:     if (offset == 4) return 0x01a8; break;
		case NAMCOS22_AIR_COMBAT22:   return 0;
		default: break;
	}

	/* pick a random number, but not the same twice in a row */
	old_rng = keycus_rng;
	do
	{
		keycus_rng = mame_rand() & 0xffff;
	} while (keycus_rng == old_rng);
	return keycus_rng;
}

static READ32_HANDLER( namcos22_keycus_r )
{
	data32_t result = 0;
	if (!(mem_mask & 0xffff0000)) result |= keycus_word_r(offset * 2) << 16;
	if (!(mem_mask & 0x0000ffff)) result |= keycus_word_r(offset * 2 + 1);
	return result;
}

/* C139 SCI: preliminary, as in MAME */
static READ32_HANDLER( namcos22_sci_r )
{
	return (offset == 0) ? 0x00040000 : 0;
}

/* developer pads: not connected (each read clocks a bit, lines active low) */
static READ32_HANDLER( namcos22_devpad_r )
{
	return 0x00010001;
}

static READ32_HANDLER( namcos22s_dsw_r )
{
	return 0xff00ffff | ((readinputport(PORT_DSW) & 0xff) << 16);
}

/* 28C64 parallel EEPROM on the odd byte lanes */
static READ32_HANDLER( namcos22s_eeprom_r )
{
	return (eeprom[offset * 2] << 24) | (eeprom[offset * 2 + 1] << 8);
}

static WRITE32_HANDLER( namcos22s_eeprom_w )
{
	if (!(mem_mask & 0xff000000)) eeprom[offset * 2] = data >> 24;
	if (!(mem_mask & 0x0000ff00)) eeprom[offset * 2 + 1] = data >> 8;
}

static NVRAM_HANDLER( namcos22 )
{
	if (read_or_write)
		mame_fwrite(file, eeprom, 0x2000);
	else if (file)
		mame_fread(file, eeprom, 0x2000);
	else if (memory_region(REGION_USER2))
		memcpy(eeprom, memory_region(REGION_USER2), 0x2000);	/* default EEPROM */
	else
		memset(eeprom, 0xff, 0x2000);
}


/* System controller (C383): interrupts, DSP and MCU control */

static void syscon_irqack(int line_index)
{
	int line = 1 << line_index;
	int level = namcos22_syscontrol[line_index] & 7;

	namcos22_irq_state &= ~line;
	if (level)
		cpu_set_irq_line(CPU_MAIN, level, CLEAR_LINE);
}

static void syscon_irqlevel(int line_index, UINT8 data)
{
	int line = 1 << line_index;
	int oldlevel = namcos22_syscontrol[line_index] & 7;
	int newlevel = data & 7;

	namcos22_irq_enabled &= ~line;
	if (namcos22_is_ss22)
		namcos22_irq_enabled |= (newlevel != 0) ? line : 0;
	else
		namcos22_irq_enabled |= (data & 0x10) ? line : 0;

	if (namcos22_irq_state & line)
	{
		if (!(namcos22_irq_enabled & line))
			syscon_irqack(line_index);
		else if (oldlevel != newlevel)
		{
			if (oldlevel) cpu_set_irq_line(CPU_MAIN, oldlevel, CLEAR_LINE);
			if (newlevel) cpu_set_irq_line(CPU_MAIN, newlevel, ASSERT_LINE);
		}
	}
}

static void master_enable(int enable)
{
	cpu_set_reset_line(CPU_MASTER, enable ? CLEAR_LINE : ASSERT_LINE);
}

static void slave_enable(int enable)
{
	cpu_set_reset_line(CPU_SLAVE, enable ? CLEAR_LINE : ASSERT_LINE);
	namcos22_slave_simulation_active = enable;
}

static void syscon_dspcontrol(int index, UINT8 data)
{
	if (data == namcos22_syscontrol[index])
		return;

	if (data == 0)
	{
		master_enable(0);
		slave_enable(0);
		dsp_irq_enabled = 0;
	}
	else if (data == 1)
	{
		master_enable(1);
		slave_enable(1);
		dsp_irq_enabled = 1;
	}
	else if (data == 0xff)
	{
		/* upload game-specific code to the DSPs */
		master_enable(1);
		dsp_irq_enabled = 0;
	}
}

static void syscon_mcucontrol(UINT8 data)
{
	cpu_set_reset_line(CPU_MCU, data ? CLEAR_LINE : ASSERT_LINE);
}

static void ss22_syscon_byte_w(int offset, UINT8 data)
{
	switch (offset)
	{
		case 0x00: case 0x01: case 0x02: case 0x03:	/* irq levels: vblank, hblank, SCI, unknown */
			syscon_irqlevel(offset, data);
			break;
		case 0x04: case 0x05: case 0x06: case 0x07:	/* irq acks */
			syscon_irqack(offset - 4);
			break;
		case 0x14:	/* watchdog */
			break;
		case 0x16:
			syscon_mcucontrol(data);
			break;
		case 0x1c:
			syscon_dspcontrol(offset, data);
			break;
		default:
			break;
	}
	namcos22_syscontrol[offset] = data;
}

static void s22_syscon_byte_w(int offset, UINT8 data)
{
	switch (offset)
	{
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04:	/* hblank, ?, SCI, unknown, vblank */
			syscon_irqlevel(offset, data);
			break;
		case 0x05: case 0x06: case 0x07: case 0x08: case 0x09:
			syscon_irqack(offset - 5);
			break;
		case 0x16:	/* watchdog */
			break;
		case 0x18:
			syscon_mcucontrol(data);
			break;
		case 0x1a:
			syscon_dspcontrol(offset, data);
			break;
		default:
			break;
	}
	namcos22_syscontrol[offset] = data;
}

static READ32_HANDLER( namcos22_syscon_r )
{
	const UINT8 *s = &namcos22_syscontrol[offset * 4];
	return (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

static WRITE32_HANDLER( namcos22s_syscon_w )
{
	int i;
	for (i = 0; i < 4; i++)
		if (!(mem_mask & (0xff000000 >> (i * 8))))
			ss22_syscon_byte_w(offset * 4 + i, data >> (24 - i * 8));
}

static WRITE32_HANDLER( namcos22_syscon_w )
{
	int i;
	for (i = 0; i < 4; i++)
		if (!(mem_mask & (0xff000000 >> (i * 8))))
			s22_syscon_byte_w(offset * 4 + i, data >> (24 - i * 8));
}

static INTERRUPT_GEN( namcos22s_interrupt )
{
	/* vblank irq */
	if (namcos22_irq_enabled & 1)
	{
		namcos22_irq_state |= 1;
		if (namcos22_syscontrol[0] & 7)
			cpu_set_irq_line(CPU_MAIN, namcos22_syscontrol[0] & 7, ASSERT_LINE);
	}
}


/* COM RAM, shared with the MCU */

static READ32_HANDLER( namcos22_shared_r )
{
	return (namcos22_shareram[offset * 2] << 16) | namcos22_shareram[offset * 2 + 1];
}

static WRITE32_HANDLER( namcos22_shared_w )
{
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		namcos22_shareram[offset * 2] = (namcos22_shareram[offset * 2] & (mem_mask >> 16)) | ((data >> 16) & ~(mem_mask >> 16));
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		namcos22_shareram[offset * 2 + 1] = (namcos22_shareram[offset * 2 + 1] & mem_mask) | (data & ~mem_mask & 0xffff);
}


/* polygon RAM ("DSP RAM"): only d0-23 are connected */

static READ32_HANDLER( namcos22_dspram_r )
{
	return namcos22_polygonram[offset] | 0xff000000;
}

static WRITE32_HANDLER( namcos22_dspram_w )
{
	if (!(mem_mask & 0x00ff0000))
	{
		mem_mask &= 0x00ffffff;
		data = SIGNED24(data);
	}
	COMBINE_DATA(&namcos22_polygonram[offset]);
}


/***************************************************************************
    DSPs
***************************************************************************/

static data16_t namcos22_dspram16_r(offs_t offset)
{
	UINT32 value = namcos22_polygonram[offset & 0x7fff];

	switch (dspram_bank & 3)
	{
		case 0: value &= 0xffff; break;
		case 1: value >>= 16; break;
		case 2: dspram16_latch = value >> 16; value &= 0xffff; break;
		case 3: return 0;	/* ready status? */
	}
	return value;
}

static void namcos22_dspram16_w(offs_t offset, data16_t data)
{
	UINT32 value = namcos22_polygonram[offset & 0x7fff];
	UINT16 lo = value & 0xffff;
	UINT16 hi = value >> 16;

	switch (dspram_bank & 3)
	{
		case 0: lo = data; break;
		case 1: hi = data; break;
		case 2: lo = data; hi = dspram16_latch; break;
		default: break;
	}
	namcos22_polygonram[offset & 0x7fff] = (hi << 16) | lo;
}

/* master data space above the page table: 0x8000-0xffff (polygon RAM), the rest is open */
static data16_t master_data_r(offs_t a)
{
	if (a >= 0x8000)
		return namcos22_dspram16_r(a - 0x8000);
	return 0;
}

static void master_data_w(offs_t a, data16_t d)
{
	if (a >= 0x8000)
		namcos22_dspram16_w(a - 0x8000, d);
}


/* point RAM / point ROM (3D model data, 24 bits) */

static void point_write(offs_t offs, UINT32 data)
{
	offs &= 0x00ffffff;
	if (namcos22_is_ss22)
	{
		if (offs >= 0xf80000 && offs < 0xfa0000)
			pointram[offs - 0xf80000] = data & 0x00ffffff;
	}
	else
	{
		if (offs >= 0xf00000 && offs < 0xf20000)
			pointram[offs - 0xf00000] = data & 0x00ffffff;
	}
}

static INT32 pointram_read(offs_t offs)
{
	INT32 result = -1;
	if (namcos22_is_ss22)
	{
		if (offs >= 0xf80000 && offs < 0xfa0000)
			result = pointram[offs - 0xf80000];
	}
	else
	{
		if (offs >= 0xf00000 && offs < 0xf20000)
			result = pointram[offs - 0xf00000];
	}
	return SIGNED24(result);
}

INT32 namcos22_point_read(offs_t offs)
{
	offs &= 0x00ffffff;
	return (offs < (offs_t)pointrom_size) ? pointrom[offs] : pointram_read(offs);
}

#define PDP_RAM_R(offs)		(namcos22_polygonram[(offs) & 0x7fff])
#define PDP_RAM_W(offs,d)	(namcos22_polygonram[(offs) & 0x7fff] = (d))

void namcos22_pdp_handle_commands(UINT16 offs)
{
	for (;;)
	{
		UINT16 start, cmd, numwords;
		UINT32 src, dst, data;

		offs &= 0x7fff;
		start = offs;
		cmd = PDP_RAM_R(offs++);

		switch (cmd)
		{
			case 0xfff0:	/* NOP? used in 'PDP LOOP TEST' */
				break;

			case 0xfff5:	/* write to point RAM */
				dst = PDP_RAM_R(offs++);
				data = PDP_RAM_R(offs++);
				point_write(dst, data);
				break;

			case 0xfff6:	/* read word from point RAM */
				src = PDP_RAM_R(offs++);
				dst = PDP_RAM_R(offs++);
				data = namcos22_point_read(src);
				PDP_RAM_W(dst, data);
				break;

			case 0xfff7:	/* block move (CommRAM to CommRAM) */
				src = PDP_RAM_R(offs++);
				dst = PDP_RAM_R(offs++);
				numwords = PDP_RAM_R(offs++);
				while (numwords--)
				{
					data = PDP_RAM_R(src++);
					PDP_RAM_W(dst++, data);
				}
				break;

			case 0xfff8:	/* unknown */
				offs++;
				break;

			case 0xfff9:	/* unknown, modify point RAM somehow? */
				logerror("unknown PDP cmd=0x%04x, offs=0x%x\n", cmd, 4 * (offs - 1));
				return;

			case 0xfffa:	/* read block from point RAM */
				src = PDP_RAM_R(offs++);
				dst = PDP_RAM_R(offs++);
				numwords = PDP_RAM_R(offs++);
				while (numwords--)
				{
					data = namcos22_point_read(src++);
					PDP_RAM_W(dst++, data);
				}
				break;

			case 0xfffb:	/* write block to point RAM */
				dst = PDP_RAM_R(offs++);
				numwords = PDP_RAM_R(offs++);
				while (numwords--)
				{
					data = PDP_RAM_R(offs++);
					point_write(dst++, data);
				}
				break;

			case 0xfffc:	/* point RAM to point RAM */
				src = PDP_RAM_R(offs++);
				dst = PDP_RAM_R(offs++);
				numwords = PDP_RAM_R(offs++);
				while (numwords--)
				{
					data = namcos22_point_read(src++);
					point_write(dst++, data);
				}
				break;

			case 0xfffd:	/* direct command to the render device */
				numwords = PDP_RAM_R(offs++);
				offs += numwords;
				break;

			case 0xfffe:	/* unknown */
				offs++;
				break;

			case 0xffff:	/* goto */
				offs = PDP_RAM_R(offs) & 0x7fff;
				if (offs == start)
					return;		/* goto self: the CPU rewrites it or retriggers the PDP */
				break;

			default:
				logerror("unknown PDP cmd=0x%04x, offs=0x%x\n", cmd, 4 * (offs - 1));
				return;
		}
	}
}


/* master DSP I/O ports */

static data16_t point_loword_ir(offs_t port)
{
	point_data = namcos22_point_read(point_address);
	point_address++;
	return point_data & 0xffff;
}

static void point_loword_iw(offs_t port, data16_t data)
{
	point_data = (point_data & ~0xffff) | data;
	point_write(point_address++, point_data);
}

static data16_t point_hiword_r(offs_t port)
{
	/* high bit is an unknown busy signal */
	return 0x8000 | ((point_data >> 16) & 0xff);
}

static void point_hiword_w(offs_t port, data16_t data)
{
	point_data = (point_data & 0xffff) | (data << 16);
}

static data16_t pdp_begin_r(offs_t port)
{
	/* kickstarts the PDP (polygon display processor): it parses the display list.
	   In MAME (and here) the drawing part is done in the slave simulation, the
	   commands that move data are done here */
	UINT16 offs;

	namcos22_pdp_frame = cpu_getcurrentframe();
	if (cpu_getvblank())
		namcos22_pdp_frame++;
	namcos22_pdp_render_done = 1;

	dsp_master_bioz = 1;
	offs = namcos22_is_ss22 ? (PDP_RAM_R(0x7fff) & 0xffff) : pdp_base;

	if (namcos22_is_ss22)
		namcos22_pdp_handle_commands(offs);
	return 0;
}

static void dsp_unk2_w(offs_t port, data16_t data)
{
	/* Ridge Racer: base address of the post-processed display list */
	pdp_base = data;
}

static data16_t dsp_unk_port3_r(offs_t port)
{
	dsp_master_bioz = 0;
	dsp_upload_state = DSP_UPLOAD_READY;
	return 0;
}

static void point_address_w(offs_t port, data16_t data)
{
	point_address <<= 16;
	point_address |= data;
}

static void upload_code_to_slave_dsp_w(offs_t port, data16_t data)
{
	switch (dsp_upload_state)
	{
		case DSP_UPLOAD_READY:
			switch (data)
			{
				case 0x00: slave_enable(0); break;
				case 0x01: dsp_upload_state = DSP_UPLOAD_DEST; break;
				case 0x02: break;	/* custom IC poke */
				case 0x03: slave_enable(1); break;
				case 0x04: break;
				case 0x10: slave_enable(1); break;	/* serial i/o related? */
				default:
					logerror("master port#7: 0x%04x\n", data);
					break;
			}
			break;

		case DSP_UPLOAD_DEST:
			upload_dest_idx = data;
			dsp_upload_state = DSP_UPLOAD_DATA;
			break;

		case DSP_UPLOAD_DATA:
			slave_extram[upload_dest_idx & 0x1fff] = data;
			upload_dest_idx++;
			break;
	}
}

static data16_t dsp_unk8_r(offs_t port) { return 0x0000; }				/* bit 0 is busy */
static data16_t custom_ic_status_r(offs_t port) { return 0x0063; }		/* bit 0 signals completion */
static data16_t dsp_upload_status_r(offs_t port) { return 0x0000; }

static void dsp_unk8_w(offs_t port, data16_t data)
{
	renderbuf_size = 0;
	namcos22_render_refresh = 1;
}

static void master_render_device_w(offs_t port, data16_t data)
{
	if (renderbuf_size < MAX_RENDER_CMD_SEQ)
	{
		renderbuf_data[renderbuf_size++] = data;
		if (renderbuf_size == MAX_RENDER_CMD_SEQ)
			namcos22_draw_direct_poly(renderbuf_data);
	}
}

static void dspram16_bank_w(offs_t port, data16_t data)
{
	dspram_bank = data;
}

static void dsp_nop_w(offs_t port, data16_t data) { }

static int pdp_status_r(void)
{
	return dsp_master_bioz;
}

static data16_t master_serial_io_r(void)
{
	return serial_slave_to_master_current;
}

/* slave DSP */

static int dsp_slave_bioz_r(void) { return 1; }
static data16_t dsp_slave_port3_r(offs_t port) { return 0x0010; }
static data16_t dsp_slave_zero_r(offs_t port) { return 0; }

static void slave_serial_io_w(data16_t data)
{
	serial_slave_to_master_next = data;
}

static INTERRUPT_GEN( dsp_vblank_irq )
{
	if (dsp_irq_enabled)
		cpu_set_irq_line(CPU_MASTER, C71_INT0, PULSE_LINE);
}

static void dsp_serial_pulse(int param)
{
	if (dsp_irq_enabled)
	{
		serial_slave_to_master_current = serial_slave_to_master_next;

		cpu_set_irq_line(CPU_MASTER, C71_RINT, PULSE_LINE);
		cpu_set_irq_line(CPU_MASTER, C71_XINT, PULSE_LINE);
		cpu_set_irq_line(CPU_SLAVE, C71_RINT, PULSE_LINE);
		cpu_set_irq_line(CPU_SLAVE, C71_XINT, PULSE_LINE);
	}
}

static void setup_dsps(void)
{
	const data16_t *irom_master = (const data16_t *)memory_region(REGION_CPU2);
	const data16_t *irom_slave = (const data16_t *)memory_region(REGION_CPU3);

	/* master */
	c71_set_internal_rom(CPU_MASTER, irom_master);
	c71_map_data(CPU_MASTER, 0x1000, 0x3fff, master_intram, 1);
	c71_map_data(CPU_MASTER, 0x4000, 0x7fff, master_extram, 1);
	c71_map_program(CPU_MASTER, 0x4000, 0x7fff, master_extram, 0);
	c71_set_data_handlers(CPU_MASTER, master_data_r, master_data_w);
	c71_set_io(CPU_MASTER, 0x0, point_loword_ir, point_loword_iw);
	c71_set_io(CPU_MASTER, 0x1, point_hiword_r, point_hiword_w);
	c71_set_io(CPU_MASTER, 0x2, pdp_begin_r, dsp_unk2_w);
	c71_set_io(CPU_MASTER, 0x3, dsp_unk_port3_r, point_address_w);
	c71_set_io(CPU_MASTER, 0x4, NULL, dsp_nop_w);
	c71_set_io(CPU_MASTER, 0x7, NULL, upload_code_to_slave_dsp_w);
	c71_set_io(CPU_MASTER, 0x8, dsp_unk8_r, dsp_unk8_w);
	c71_set_io(CPU_MASTER, 0x9, custom_ic_status_r, dsp_nop_w);
	c71_set_io(CPU_MASTER, 0xa, NULL, dsp_nop_w);
	c71_set_io(CPU_MASTER, 0xb, NULL, dsp_nop_w);
	c71_set_io(CPU_MASTER, 0xc, NULL, master_render_device_w);
	c71_set_io(CPU_MASTER, 0xd, NULL, dspram16_bank_w);
	c71_set_io(CPU_MASTER, 0xe, NULL, dsp_nop_w);
	c71_set_io(CPU_MASTER, 0xf, dsp_upload_status_r, dsp_nop_w);
	c71_set_bio(CPU_MASTER, pdp_status_r);
	c71_set_serial(CPU_MASTER, master_serial_io_r, NULL);

	/* slave */
	c71_set_internal_rom(CPU_SLAVE, irom_slave);
	c71_map_data(CPU_SLAVE, 0x8000, 0x9fff, slave_extram, 1);
	c71_map_program(CPU_SLAVE, 0x8000, 0x9fff, slave_extram, 0);
	c71_set_io(CPU_SLAVE, 0x3, dsp_slave_port3_r, NULL);
	c71_set_io(CPU_SLAVE, 0x4, dsp_slave_zero_r, NULL);
	c71_set_io(CPU_SLAVE, 0x5, dsp_slave_zero_r, NULL);
	c71_set_io(CPU_SLAVE, 0x6, dsp_slave_zero_r, dsp_nop_w);
	c71_set_io(CPU_SLAVE, 0x8, dsp_slave_zero_r, dsp_nop_w);
	c71_set_io(CPU_SLAVE, 0xb, dsp_slave_zero_r, dsp_nop_w);
	c71_set_io(CPU_SLAVE, 0xc, NULL, dsp_nop_w);
	c71_set_bio(CPU_SLAVE, dsp_slave_bioz_r);
	c71_set_serial(CPU_SLAVE, NULL, slave_serial_io_w);
}


/***************************************************************************
    MCU (M37710S4 on Super System 22): C352, inputs, outputs
***************************************************************************/


static READ16_HANDLER( mcu_shared_r )
{
	return namcos22_shareram[offset];
}

static WRITE16_HANDLER( mcu_shared_w )
{
	COMBINE_DATA(&namcos22_shareram[offset]);
}

static READ16_HANDLER( mcu_rom_r )
{
	return ((data16_t *)memory_region(REGION_CPU4))[offset & 0x3ffff];
}

static READ16_HANDLER( mcu_bios_r )
{
	return ((data16_t *)memory_region(REGION_CPU4))[(0xc000 / 2 + offset) & 0x3ffff];
}

/* MB87078 electronic volume of the C352 outputs */
static UINT8 mb87078_control;
static UINT8 mb87078_data;
static UINT16 mb87078_latch[4];
static UINT8 mb87078_gain_index[4];

/* channel n sets the gain of C352 output n ^ 3 (MAME): 3, 2 the front, 1, 0 the rear */
static void mb87078_apply(void)
{
	int ch;
	for (ch = 0; ch < 4; ch++)
	{
		int i = mb87078_gain_index[ch];
		int gain = (i >= 65) ? 0 : (int)(256.0 * pow(10.0, (-0.5 * i) / 20.0) + 0.5);
		c352_set_output_gain(ch ^ 3, gain);
	}
}

static void mb87078_gain_recalc(void)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		UINT16 l = mb87078_latch[i];
		if (~l & 0x40)
			mb87078_gain_index[i] = 65;		/* EN = 0: -infinity */
		else if (l & 0x100)
			mb87078_gain_index[i] = 64;		/* C32: -32dB */
		else if (l & 0x80)
			mb87078_gain_index[i] = 0;		/* C0: 0dB */
		else
			mb87078_gain_index[i] = ~l & 0x3f;
	}
	mb87078_apply();
}

static WRITE16_HANDLER( mb87078_w )
{
	if (!ACCESSING_LSB16)
		return;
	if (offset & 1)
		mb87078_control = data & 0x1f;
	else
	{
		mb87078_data = data & 0x3f;
		mb87078_latch[mb87078_control & 3] = ((mb87078_control << 4) & 0x1c0) | mb87078_data;
		mb87078_gain_recalc();
	}
}

static void mb87078_reset(void)
{
	int i;
	for (i = 0; i < 4; i++)
		mb87078_latch[i] = 0x7f;	/* all channels at 0dB */
	mb87078_gain_recalc();
}

static MEMORY_READ16_START( mcu_readmem )
	{ 0x002000, 0x002fff, c352_0_r },
	{ 0x004000, 0x00bfff, mcu_shared_r },
	{ 0x00c000, 0x00ffff, mcu_bios_r },
	{ 0x200000, 0x27ffff, mcu_rom_r },
	{ 0x300000, 0x300001, MRA16_NOP },
MEMORY_END

static MEMORY_WRITE16_START( mcu_writemem )
	{ 0x002000, 0x002fff, c352_0_w },
	{ 0x004000, 0x00bfff, mcu_shared_w },
	{ 0x301000, 0x301001, MWA16_NOP },	/* watchdog? LEDs? */
	{ 0x308000, 0x308003, mb87078_w },	/* MB87078 volume control */
MEMORY_END

/* MCU ADC channels: base + port add - port sub (-1 = none) */
static int adc_base[8], adc_add[8], adc_sub[8];

static void set_adc(int ch, int base, int add, int sub)
{
	adc_base[ch] = base;
	adc_add[ch] = add;
	adc_sub[ch] = sub;
}

static void set_adc_ports(int p0, int p1, int p2, int p3)
{
	int i;
	set_adc(0, 0, p0, -1);
	set_adc(1, 0, p1, -1);
	set_adc(2, 0, p2, -1);
	set_adc(3, 0, p3, -1);
	for (i = 4; i < 8; i++)
		set_adc(i, 0, -1, -1);
}

/* Alpine Racer / Alpine Surfer: the steplock motor locks or frees the steps,
   its state is read back on input bits 7 (free) and 8 (locked) */
static int alpine_motor;
static UINT8 alpine_motor_status;
static mame_timer *alpine_motor_timer;

static void alpine_steplock(int param)
{
	alpine_motor_status = param;
}

static void alpine_port4_w(data8_t data)
{
	if (~mcu_iocontrol & data & 0x20)
	{
		if ((mcu_outdata & 6) == 6)
		{
			if (alpine_motor_status == 2)
			{
				alpine_motor_status = 0;		/* free the steps */
				timer_adjust(alpine_motor_timer, TIME_IN_MSEC(500), 1, 0);
			}
		}
		else if (mcu_outdata & 4)
		{
			if (alpine_motor_status == 1)
			{
				alpine_motor_status = 0;		/* lock the steps */
				timer_adjust(alpine_motor_timer, TIME_IN_MSEC(500), 2, 0);
			}
		}
	}
}

static data8_t mcu_io_r(int port)
{
	switch (port)
	{
		case M37710_PORT4:
			return mcu_iocontrol;

		case M37710_PORT5:
		{
			UINT16 inputs = readinputport(PORT_INPUTS);
			if (alpine_motor)
				inputs = (inputs & ~0x180) | ((alpine_motor_status & 3) << 7);
			return (mcu_iocontrol & 0x08) ? (inputs & 0xff) : (inputs >> 8);
		}

		case M37710_PORT6:
			return 0;
	}

	if (port >= M37710_ADC0_L && port <= M37710_ADC7_H)
	{
		int ch = (port - M37710_ADC0_L) >> 1;
		int value = adc_base[ch];
		if (adc_add[ch] >= 0) value += readinputport(adc_add[ch]);
		if (adc_sub[ch] >= 0) value -= readinputport(adc_sub[ch]);
		value &= 0xffff;
		return ((port - M37710_ADC0_L) & 1) ? (value >> 8) : (value & 0xff);
	}
	return 0xff;
}

static void mcu_io_w(int port, data8_t data)
{
	switch (port)
	{
		case M37710_PORT4:
			/* d3: input select for port 5; d5,d6: output strobes for port 5 */
			if (alpine_motor)
				alpine_port4_w(data);
			if (~mcu_iocontrol & data & 0x20)
				coin_counter_w(0, mcu_outdata & 1);
			mcu_iocontrol = data;
			break;

		case M37710_PORT5:
			mcu_outdata = data;
			break;
	}
}

static void mcu_irq2_callback(int param)
{
	cpu_set_irq_line(CPU_MCU, M37710_LINE_IRQ2, HOLD_LINE);
}

static INTERRUPT_GEN( namcos22s_mcu_interrupt )
{
	/* IRQ0 at the vblank (line 480), IRQ2 at line 240 */
	cpu_set_irq_line(CPU_MCU, M37710_LINE_IRQ0, HOLD_LINE);
	timer_adjust(mcu_irq2_timer, cpu_getscanlinetime(240), 0, 0);
}

static struct C352interface namcos22_c352_interface =
{
	49152000 / 2,	/* clock */
	288,			/* divider */
	REGION_SOUND1,
	100
};


/***************************************************************************
    Prop Cycle pedal
***************************************************************************/

/* The pedal has a 1-bit "light interrupted" sensor: the faster you pedal, the
   faster it pulses. It clocks the MCU timer A3 (event counter), the program
   counts the pulses in a frame. The timings (usec) are MAME's. */
#define PORT_PEDAL	(PORT_ADC0 + 2)
static mame_timer *pedal_update_timer;
static mame_timer *pedal_pulse_timer;

static void pedal_pulse(int param)
{
	cpu_set_irq_line(CPU_MCU, M37710_LINE_TIMERA3OUT, param ? ASSERT_LINE : CLEAR_LINE);
	cpu_set_irq_line(CPU_MCU, M37710_LINE_TIMERA3TICK, PULSE_LINE);
}

static void pedal_update(int param)
{
	int pedal = readinputport(PORT_PEDAL) & 0x7f;

	if (pedal != 0)
	{
		double period = TIME_IN_USEC(750 + 100000.0 / abs(pedal));
		double left = timer_timeleft(pedal_pulse_timer);
		timer_adjust(pedal_pulse_timer, (left < period) ? left : period, pedal < 0, period);
	}
	else
		timer_adjust(pedal_pulse_timer, TIME_NEVER, 0, 0);
}


/***************************************************************************
    Armadillo Racing trackball
***************************************************************************/

/* The optical trackball clocks the MCU timers A2 (Y) and A3 (X): the speed
   sets the frequency. The timings (Hz) are MAME's. */
#define PORT_TRACKX		(PORT_ADC0)
#define PORT_TRACKY		(PORT_ADC0 + 1)
#define PORT_ADILLOR_DEV	(PORT_ADC0 + 2)
static mame_timer *trackball_update_timer;
static mame_timer *trackball_pulse_timer[2];
static UINT32 trackball_count[2];
static INT32 trackball_residual[2];

static void trackball_pulse(int param)
{
	cpu_set_irq_line(CPU_MCU, (param & 1) ? M37710_LINE_TIMERA2OUT : M37710_LINE_TIMERA3OUT, (param & 2) ? ASSERT_LINE : CLEAR_LINE);
	cpu_set_irq_line(CPU_MCU, (param & 1) ? M37710_LINE_TIMERA2TICK : M37710_LINE_TIMERA3TICK, PULSE_LINE);
}

static void trackball_update(int param)
{
	UINT32 trackval[2];
	INT32 delta[2];
	int speed[2], params[2], axis;
	double x, y, t[2];

	trackval[0] = readinputport(PORT_TRACKX) & 0xffff;
	trackval[1] = readinputport(PORT_TRACKY) & 0xffff;
	for (axis = 0; axis < 2; axis++)
	{
		delta[axis] = (INT32)(trackval[axis] - trackball_count[axis]);
		trackball_count[axis] = trackval[axis];
		if (delta[axis] > 0x8000)
			delta[axis] -= 0x10000;
		else if (delta[axis] < -0x8000)
			delta[axis] += 0x10000;
		trackball_residual[axis] += delta[axis];
		speed[axis] = trackball_residual[axis];
		if (speed[axis] > 0x7f) speed[axis] = 0x7f;
		if (speed[axis] < -0x7f) speed[axis] = -0x7f;
		trackball_residual[axis] -= speed[axis];
	}

	/* -1.0 .. 1.0 */
	x = speed[0] / 127.0;
	y = speed[1] / 127.0;

	/* the trackball is rotated by 45 degrees: axes like (x), not (+) */
	if (readinputport(PORT_ADILLOR_DEV) & 2)
	{
		double ox = x, oy = y, scale = 1.0 / sqrt(2.0);
		x = (ox - oy) * scale;
		y = (oy + ox) * scale;
	}

	t[0] = fabs(y);		/* y -> A2 */
	t[1] = fabs(x);		/* x -> A3 */
	params[0] = (y >= 0.0) ? 2 : 0;
	params[1] = (x <= 0.0) ? 3 : 1;

	for (axis = 0; axis < 2; axis++)
	{
		if (t[axis] > (1.0 / 1250.0))
		{
			double period = TIME_IN_HZ(20 + 1250 * t[axis]);
			double left = timer_timeleft(trackball_pulse_timer[axis]);
			timer_adjust(trackball_pulse_timer[axis], (left < period) ? left : period, params[axis], period);
		}
		else
			timer_adjust(trackball_pulse_timer[axis], TIME_NEVER, axis, 0);
	}
}


/***************************************************************************
    Time Crisis light gun
***************************************************************************/

/* MAME's light gun ranges (tuned for CRT): X 68-694, Y 43-284; the screen edges mean off-screen */
#define TIMECRIS_GUN_X_MIN	68
#define TIMECRIS_GUN_X_MAX	(68 + 626)
#define TIMECRIS_GUN_Y_MIN	43
#define TIMECRIS_GUN_Y_MAX	(43 + 241)

static READ32_HANDLER( timecris_gun_r )
{
	UINT16 xpos = readinputport(PORT_ADC0);
	UINT16 ypos = readinputport(PORT_ADC0 + 1);

	/* the screen edges are off screen (MAME) */
	if (xpos <= TIMECRIS_GUN_X_MIN || xpos >= TIMECRIS_GUN_X_MAX || ypos <= TIMECRIS_GUN_Y_MIN || ypos >= TIMECRIS_GUN_Y_MAX)
		xpos = ypos = 0;

	/* 430004 and 430008 both give y: the game adds them */
	switch (offset)
	{
		case 0: return xpos << 16;
		case 1:
		case 2: return ypos << 16;
	}
	return 0;
}

/* the gun crosshair, on the whole screen as MAME's PORT_CROSSHAIR(X/Y, 1.0, 0.0, 0) */
void namcos22_draw_crosshairs(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	const struct rectangle *visible = &Machine->visible_area;
	int x, y;

	if (namcos22_gametype != NAMCOS22_TIME_CRISIS)
		return;
	x = readinputport(PORT_ADC0) - TIMECRIS_GUN_X_MIN;
	y = readinputport(PORT_ADC0 + 1) - TIMECRIS_GUN_Y_MIN;
	x = visible->min_x + x * (visible->max_x - visible->min_x) / (TIMECRIS_GUN_X_MAX - TIMECRIS_GUN_X_MIN);
	y = visible->min_y + y * (visible->max_y - visible->min_y) / (TIMECRIS_GUN_Y_MAX - TIMECRIS_GUN_Y_MIN);
	draw_crosshair(1, bitmap, x, y, cliprect);
}


/***************************************************************************
    Alpine Surfer ROM banking
***************************************************************************/

/* 0x200000-0x3fffff is a window on the ROM at 0x200000 + bank * 0x200000 (data only).
   A read handler: the RAM areas of this map take the dynamic memory banks. */
static int alpines_bank;

static READ32_HANDLER( alpines_bank_r )
{
	return ((data32_t *)memory_region(REGION_CPU1))[((0x200000 + alpines_bank * 0x200000) >> 2) + offset];
}

static WRITE32_HANDLER( alpines_bank_w )
{
	int bank = data & 3;
	alpines_bank = (bank > 2) ? 2 : bank;
}


/***************************************************************************
    Memory maps
***************************************************************************/

static MEMORY_READ32_START( namcos22s_readmem )
	{ 0x000000, 0x3fffff, MRA32_ROM },
	{ 0x400000, 0x40001f, namcos22_keycus_r },
	{ 0x410000, 0x413fff, MRA32_RAM },
	{ 0x420000, 0x42000f, namcos22_sci_r },
	{ 0x440000, 0x440003, namcos22s_dsw_r },
	{ 0x450008, 0x45000b, namcos22_devpad_r },
	{ 0x460000, 0x463fff, namcos22s_eeprom_r },
	{ 0x700000, 0x70001f, namcos22_syscon_r },
	{ 0x810000, 0x81000f, namcos22s_czattr_r },
	{ 0x810200, 0x8103ff, namcos22s_czram_r },
	{ 0x824000, 0x8243ff, MRA32_RAM },
	{ 0x828000, 0x83ffff, MRA32_RAM },
	{ 0x860000, 0x860007, namcos22s_spotram_r },
	{ 0x880000, 0x89ffff, MRA32_RAM },
	{ 0x8a0000, 0x8a000f, namcos22_tilemapattr_r },
	{ 0x900000, 0x90ffff, MRA32_RAM },
	{ 0x940000, 0x94007f, namcos22s_vics_control_r },
	{ 0x980000, 0x9affff, MRA32_RAM },
	{ 0xa04000, 0xa0bfff, namcos22_shared_r },
	{ 0xc00000, 0xc1ffff, namcos22_dspram_r },
	{ 0xe00000, 0xe3ffff, MRA32_RAM },
MEMORY_END

static MEMORY_WRITE32_START( namcos22s_writemem )
	{ 0x000000, 0x3fffff, MWA32_ROM },
	{ 0x400000, 0x40001f, MWA32_NOP },
	{ 0x410000, 0x413fff, MWA32_RAM, &sci_buffer },
	{ 0x420000, 0x42000f, MWA32_NOP },
	{ 0x430000, 0x430003, MWA32_NOP },	/* CPU LEDs */
	{ 0x450008, 0x45000b, MWA32_NOP },	/* dev pad strobe */
	{ 0x460000, 0x463fff, namcos22s_eeprom_w },
	{ 0x700000, 0x70001f, namcos22s_syscon_w },
	{ 0x800000, 0x800003, namcos22s_chipselect_w },
	{ 0x810000, 0x81000f, namcos22s_czattr_w },
	{ 0x810200, 0x8103ff, namcos22s_czram_w },
	{ 0x820000, 0x8202ff, MWA32_NOP },
	{ 0x824000, 0x8243ff, MWA32_RAM, &namcos22_mixer },
	{ 0x828000, 0x83ffff, namcos22_paletteram_w, &namcos22_paletteram },
	{ 0x860000, 0x860007, namcos22s_spotram_w },
	{ 0x880000, 0x89ffff, namcos22_cgram_w, &namcos22_cgram },
	{ 0x8a0000, 0x8a000f, namcos22_tilemapattr_w },
	{ 0x900000, 0x90ffff, MWA32_RAM, &namcos22_vics_data },
	{ 0x940000, 0x94007f, namcos22s_vics_control_w, &namcos22_vics_control },
	{ 0x980000, 0x9affff, MWA32_RAM, &namcos22_spriteram },
	{ 0xa04000, 0xa0bfff, namcos22_shared_w },
	{ 0xc00000, 0xc1ffff, namcos22_dspram_w },
	{ 0xe00000, 0xe3ffff, MWA32_RAM, &workram },
MEMORY_END

/* the C71s go through the core's page tables: empty maps for the generic memory system */
static MEMORY_READ16_START( c71_readmem )
	{ 0x0000, 0x0001, MRA16_NOP },
MEMORY_END

static MEMORY_WRITE16_START( c71_writemem )
	{ 0x0000, 0x0001, MWA16_NOP },
MEMORY_END


/***************************************************************************
    System 22
***************************************************************************/

/* input ports (System 22 driving games) */
#define S22_PORT_INPUTS	0
#define S22_PORT_SW2	1
#define S22_PORT_SW3	2
#define S22_PORT_ADC0	3		/* steering, gas, brake (and a 4th) */

static UINT32 credits1, credits2;
static int old_coin_state;

static READ32_HANDLER( namcos22_dsw_r )
{
	return ((readinputport(S22_PORT_SW3) & 0xff) << 24) | ((readinputport(S22_PORT_SW2) & 0xff) << 16) | 0xffff;
}

/* HN58C65 EEPROM: a byte per address */
static READ32_HANDLER( namcos22_eeprom_r )
{
	const UINT8 *e = &eeprom[offset * 4];
	return (e[0] << 24) | (e[1] << 16) | (e[2] << 8) | e[3];
}

static WRITE32_HANDLER( namcos22_eeprom_w )
{
	int i;
	for (i = 0; i < 4; i++)
		if (!(mem_mask & (0xff000000 >> (i * 8))))
			eeprom[offset * 4 + i] = data >> (24 - i * 8);
}

/* main RAM mirror at 0x18000000 */
static READ32_HANDLER( workram_mirror_r ) { return workram[offset]; }
static WRITE32_HANDLER( workram_mirror_w ) { COMBINE_DATA(&workram[offset]); }

/* inputs, coins and the analog controls go through the I/O board MCU, not emulated (as MAME) */
static void handle_coinage(UINT16 flags)
{
	int coin_state = ((flags & 0x1000) >> 12) | ((flags & 0x0200) >> 8);

	if (!(coin_state & 1) && (old_coin_state & 1))
		credits1++;
	if (!(coin_state & 2) && (old_coin_state & 2))
		credits2++;

	old_coin_state = coin_state;
	namcos22_shareram[0x3a / 2] = ((credits1 & 0xff) << 8) | (credits2 & 0xff);
}

static void handle_driving_io(void)
{
	if (namcos22_syscontrol[0x18] != 0)
	{
		UINT16 flags = readinputport(S22_PORT_INPUTS);
		UINT16 steer = readinputport(S22_PORT_ADC0 + 0);
		UINT16 gas = readinputport(S22_PORT_ADC0 + 1);
		UINT16 brake = readinputport(S22_PORT_ADC0 + 2);

		switch (namcos22_gametype)
		{
			case NAMCOS22_RIDGE_RACER:
			case NAMCOS22_RIDGE_RACER2:
				steer += 0x160;
				gas += 884;
				brake += 809;
				break;
			case NAMCOS22_RAVE_RACER:
				steer += 32;
				gas += 992;
				brake += 3008;
				break;
			case NAMCOS22_ACE_DRIVER:
			case NAMCOS22_VICTORY_LAP:
				steer += 2048;
				gas += 992;
				brake += 3008;
				break;
			default:
				break;
		}

		namcos22_shareram[0x030 / 2] = flags;
		namcos22_shareram[0x032 / 2] = steer;
		namcos22_shareram[0x034 / 2] = gas;
		namcos22_shareram[0x036 / 2] = brake;
		handle_coinage(flags);
	}
}

static void handle_cybrcomm_io(void)
{
	if (namcos22_syscontrol[0x18] != 0)
	{
		UINT16 flags = readinputport(S22_PORT_INPUTS);
		int i;

		namcos22_shareram[0x030 / 2] = flags;
		for (i = 0; i < 4; i++)
			namcos22_shareram[0x032 / 2 + i] = (readinputport(S22_PORT_ADC0 + i) & 0xff) * 0x10;
		handle_coinage(flags);
	}
}

static INTERRUPT_GEN( namcos22_interrupt )
{
	switch (namcos22_gametype)
	{
		case NAMCOS22_RIDGE_RACER:
		case NAMCOS22_RIDGE_RACER2:
		case NAMCOS22_RAVE_RACER:
		case NAMCOS22_ACE_DRIVER:
		case NAMCOS22_VICTORY_LAP:
			handle_driving_io();
			break;
		case NAMCOS22_CYBER_COMMANDO:
			handle_cybrcomm_io();
			break;
	}

	/* vblank irq */
	if (namcos22_irq_enabled & (1 << 4))
	{
		namcos22_irq_state |= (1 << 4);
		if (namcos22_syscontrol[4] & 7)
			cpu_set_irq_line(CPU_MAIN, namcos22_syscontrol[4] & 7, ASSERT_LINE);
	}
}

static MEMORY_READ32_START( namcos22_readmem )
	{ 0x00000000, 0x001fffff, MRA32_ROM },
	{ 0x10000000, 0x1001ffff, MRA32_RAM },
	{ 0x18000000, 0x1801ffff, workram_mirror_r },
	{ 0x20000000, 0x2000000f, namcos22_keycus_r },
	{ 0x20010000, 0x20013fff, MRA32_RAM },
	{ 0x20020000, 0x2002000f, namcos22_sci_r },
	{ 0x40000000, 0x4000001f, namcos22_syscon_r },
	{ 0x48000000, 0x4800003f, MRA32_NOP },
	{ 0x50000000, 0x50000003, namcos22_dsw_r },
	{ 0x50000008, 0x5000000b, namcos22_devpad_r },
	{ 0x58000000, 0x58001fff, namcos22_eeprom_r },
	{ 0x60004000, 0x6000bfff, namcos22_shared_r },
	{ 0x70000000, 0x7001ffff, namcos22_dspram_r },
	{ 0x90010000, 0x90017fff, MRA32_RAM },
	{ 0x90020000, 0x90027fff, MRA32_RAM },
	{ 0x90028000, 0x9003ffff, MRA32_RAM },
	{ 0x90040000, 0x9007ffff, MRA32_NOP },
	{ 0x90080000, 0x9009ffff, MRA32_RAM },
	{ 0x900a0000, 0x900a000f, namcos22_tilemapattr_r },
MEMORY_END

static MEMORY_WRITE32_START( namcos22_writemem )
	{ 0x00000000, 0x001fffff, MWA32_ROM },
	{ 0x10000000, 0x1001ffff, MWA32_RAM, &workram },
	{ 0x18000000, 0x1801ffff, workram_mirror_w },
	{ 0x20000000, 0x2000000f, MWA32_NOP },
	{ 0x20010000, 0x20013fff, MWA32_RAM, &sci_buffer },
	{ 0x20020000, 0x2002000f, MWA32_NOP },
	{ 0x40000000, 0x4000001f, namcos22_syscon_w },
	{ 0x48000000, 0x4800003f, MWA32_NOP },
	{ 0x50000000, 0x50000003, MWA32_NOP },	/* CPU LEDs */
	{ 0x50000008, 0x5000000b, MWA32_NOP },
	{ 0x58000000, 0x58001fff, namcos22_eeprom_w },
	{ 0x60000000, 0x60003fff, MWA32_NOP },
	{ 0x60004000, 0x6000bfff, namcos22_shared_w },
	{ 0x70000000, 0x7001ffff, namcos22_dspram_w },
	{ 0x90000000, 0x90000003, MWA32_NOP },
	{ 0x90010000, 0x90017fff, MWA32_RAM, &namcos22_czram },
	{ 0x90020000, 0x90027fff, MWA32_RAM, &namcos22_mixer },
	{ 0x90028000, 0x9003ffff, namcos22_paletteram_w, &namcos22_paletteram },
	{ 0x90080000, 0x9009ffff, namcos22_cgram_w, &namcos22_cgram },
	{ 0x900a0000, 0x900a000f, namcos22_tilemapattr_w },
MEMORY_END

/* C74 (M37702 with internal BIOS) on the CPU board: sound */
static MEMORY_READ16_START( mcu_s22_readmem )
	{ 0x002000, 0x002fff, c352_0_r },
	{ 0x004000, 0x00bfff, mcu_shared_r },
	{ 0x200000, 0x27ffff, mcu_rom_r },
MEMORY_END

static MEMORY_WRITE16_START( mcu_s22_writemem )
	{ 0x002000, 0x002fff, c352_0_w },
	{ 0x004000, 0x00bfff, mcu_shared_w },
MEMORY_END

static data8_t mcu_s22_io_r(int port)
{
	/* port 4 0x10 selects the sound role of the C74 */
	if (port == M37710_PORT4)
		return 0x10;
	if (port >= M37710_ADC0_L && port <= M37710_ADC7_H)
		return 0;
	return 0xff;
}

static void mcu_s22_io_w(int port, data8_t data)
{
}

/***************************************************************************
    Machine
***************************************************************************/

static MACHINE_INIT( namcos22 )
{
	master_enable(0);
	slave_enable(0);
	dsp_irq_enabled = 0;
	cpu_set_reset_line(CPU_MCU, ASSERT_LINE);

	dsp_upload_state = DSP_UPLOAD_READY;
	namcos22_irq_enabled = 0;
	namcos22_irq_state = 0;

	namcos22_video_reset();
	mb87078_reset();
}

static void namcos22_common_init(int gametype, int ss22)
{
	int i;
	const UINT8 *rom;

	namcos22_gametype = gametype;
	namcos22_is_ss22 = ss22;

	namcos22_polygonram = auto_malloc(0x20000);
	namcos22_shareram = auto_malloc(0x8000);
	eeprom = auto_malloc(0x2000);
	master_extram = auto_malloc(0x4000 * 2);
	master_intram = auto_malloc(0x3000 * 2);
	slave_extram = auto_malloc(0x2000 * 2);
	pointram = auto_malloc(0x20000 * 4);

	memset(namcos22_polygonram, 0xcc, 0x20000);
	memset(namcos22_shareram, 0, 0x8000);
	memset(eeprom, 0xff, 0x2000);
	memset(master_extram, 0, 0x4000 * 2);
	memset(master_intram, 0, 0x3000 * 2);
	memset(slave_extram, 0, 0x2000 * 2);
	memset(pointram, 0, 0x20000 * 4);

	/* point ROM: three byte planes, low/mid/high */
	pointrom_size = memory_region_length(REGION_USER1) / 3;
	pointrom = auto_malloc(pointrom_size * sizeof(INT32));
	rom = memory_region(REGION_USER1);
	for (i = 0; i < pointrom_size; i++)
		pointrom[i] = SIGNED24((rom[i + 2 * pointrom_size] << 16) | (rom[i + pointrom_size] << 8) | rom[i]);

	setup_dsps();

	mcu_irq2_timer = timer_alloc(mcu_irq2_callback);
	dsp_serial_timer = timer_alloc(dsp_serial_pulse);
	timer_adjust(dsp_serial_timer, TIME_IN_HZ(100), 0, TIME_IN_HZ(100));

	m37710_set_io_handlers(mcu_io_r, mcu_io_w);
	set_adc_ports(-1, -1, -1, -1);
	alpine_motor = 0;

	state_save_register_UINT32("namcos22", 0, "polygonram", namcos22_polygonram, 0x8000);
	state_save_register_UINT16("namcos22", 0, "shareram", namcos22_shareram, 0x4000);
	state_save_register_UINT8("namcos22", 0, "eeprom", eeprom, 0x2000);
	state_save_register_UINT16("namcos22", 0, "master_extram", master_extram, 0x4000);
	state_save_register_UINT16("namcos22", 0, "master_intram", master_intram, 0x3000);
	state_save_register_UINT16("namcos22", 0, "slave_extram", slave_extram, 0x2000);
	state_save_register_UINT32("namcos22", 0, "pointram", pointram, 0x20000);
	state_save_register_UINT8("namcos22", 0, "syscontrol", namcos22_syscontrol, 0x20);
	state_save_register_int("namcos22", 0, "irq_enabled", &namcos22_irq_enabled);
	state_save_register_int("namcos22", 0, "irq_state", &namcos22_irq_state);
	state_save_register_int("namcos22", 0, "dsp_irq_enabled", &dsp_irq_enabled);
	state_save_register_UINT16("namcos22", 0, "dsp_master_bioz", &dsp_master_bioz, 1);
	state_save_register_UINT32("namcos22", 0, "point_address", &point_address, 1);
	state_save_register_UINT32("namcos22", 0, "point_data", &point_data, 1);
	state_save_register_UINT16("namcos22", 0, "serial_next", &serial_slave_to_master_next, 1);
	state_save_register_UINT16("namcos22", 0, "serial_current", &serial_slave_to_master_current, 1);
	state_save_register_int("namcos22", 0, "renderbuf_size", &renderbuf_size);
	state_save_register_UINT16("namcos22", 0, "renderbuf_data", renderbuf_data, MAX_RENDER_CMD_SEQ);
	state_save_register_int("namcos22", 0, "dsp_upload_state", &dsp_upload_state);
	state_save_register_int("namcos22", 0, "upload_dest_idx", &upload_dest_idx);
	state_save_register_UINT16("namcos22", 0, "dspram_bank", &dspram_bank, 1);
	state_save_register_UINT16("namcos22", 0, "dspram16_latch", &dspram16_latch, 1);
	state_save_register_UINT16("namcos22", 0, "pdp_base", &pdp_base, 1);
	state_save_register_UINT16("namcos22", 0, "keycus_rng", &keycus_rng, 1);
	state_save_register_UINT8("namcos22", 0, "mcu_iocontrol", &mcu_iocontrol, 1);
	state_save_register_UINT8("namcos22", 0, "mcu_outdata", &mcu_outdata, 1);
	state_save_register_int("namcos22", 0, "pdp_render_done", &namcos22_pdp_render_done);
	state_save_register_int("namcos22", 0, "render_refresh", &namcos22_render_refresh);
	state_save_register_int("namcos22", 0, "slave_sim_active", &namcos22_slave_simulation_active);
	state_save_register_UINT32("namcos22", 0, "pdp_frame", &namcos22_pdp_frame, 1);
	state_save_register_UINT8("namcos22", 0, "mb87078_control", &mb87078_control, 1);
	state_save_register_UINT8("namcos22", 0, "mb87078_data", &mb87078_data, 1);
	state_save_register_UINT16("namcos22", 0, "mb87078_latch", mb87078_latch, 4);
	state_save_register_func_postload(mb87078_gain_recalc);
}

static MACHINE_DRIVER_START( namcos22s )
	MDRV_CPU_ADD_TAG("main", M68EC020, 24576000)
	MDRV_CPU_MEMORY(namcos22s_readmem, namcos22s_writemem)
	MDRV_CPU_VBLANK_INT(namcos22s_interrupt, 1)

	MDRV_CPU_ADD(NAMCOC71, 40000000)	/* master DSP */
	MDRV_CPU_MEMORY(c71_readmem, c71_writemem)
	MDRV_CPU_VBLANK_INT(dsp_vblank_irq, 1)

	MDRV_CPU_ADD(NAMCOC71, 40000000)	/* slave DSP */
	MDRV_CPU_MEMORY(c71_readmem, c71_writemem)

	/* one cycle per two XIN clocks (current MAME clocks_to_cycles) */
	MDRV_CPU_ADD_TAG("mcu", M37702, 16384000 / 2)
	MDRV_CPU_MEMORY(mcu_readmem, mcu_writemem)
	MDRV_CPU_VBLANK_INT(namcos22s_mcu_interrupt, 1)

	MDRV_FRAMES_PER_SECOND(S22_REFRESH)
	MDRV_VBLANK_DURATION(S22_VBLANK_USEC)
	MDRV_INTERLEAVE(668)	/* 40 kHz, as MAME */

	MDRV_MACHINE_INIT(namcos22)
	MDRV_NVRAM_HANDLER(namcos22)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(640, 480)
	MDRV_VISIBLE_AREA(0, 639, 0, 479)
	MDRV_PALETTE_LENGTH(0x8000)

	MDRV_VIDEO_START(namcos22s)
	MDRV_VIDEO_UPDATE(namcos22s)
	MDRV_VIDEO_STOP(namcos22)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(C352, namcos22_c352_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( namcos22 )
	MDRV_IMPORT_FROM(namcos22s)

	MDRV_CPU_REPLACE("main", M68020, 24576000)
	MDRV_CPU_MEMORY(namcos22_readmem, namcos22_writemem)
	MDRV_CPU_VBLANK_INT(namcos22_interrupt, 1)

	/* the C74 has no periodic interrupts: it runs off its timer A0 */
	MDRV_CPU_MODIFY("mcu")
	MDRV_CPU_MEMORY(mcu_s22_readmem, mcu_s22_writemem)
	MDRV_CPU_VBLANK_INT(NULL, 0)

	MDRV_VIDEO_START(namcos22)
	MDRV_VIDEO_UPDATE(namcos22)
MACHINE_DRIVER_END


/***************************************************************************
    Input ports
***************************************************************************/

/* port 0: MCU inputs (16 bits, read 8 at a time), port 1: DIP switches SW4 (active low) */

#define NAMCOS22S_DSW_UNKNOWN \
	PORT_START \
	PORT_DIPNAME( 0x01, 0x01, "SW4:1" ) \
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x02, 0x02, "SW4:2" ) \
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x04, 0x04, "SW4:3" ) \
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x08, 0x08, "SW4:4" ) \
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x10, 0x10, "SW4:5" ) \
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x20, 0x20, "SW4:6" ) \
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x40, 0x40, "SW4:7" ) \
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_START( timecris )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )	/* gun trigger */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )	/* foot pedal */
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	NAMCOS22S_DSW_UNKNOWN
	PORT_DIPNAME( 0x80, 0x80, "Test Mode" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* the gun position as the game reads it (MAME's ranges, tuned for a CRT);
	   the edges are off screen, to reload */
	PORT_START	/* OPT.0: gun X */
	PORT_ANALOG( 0xffff, TIMECRIS_GUN_X_MIN + 626 / 2, IPT_LIGHTGUN_X | IPF_PLAYER1, 48, 10, TIMECRIS_GUN_X_MIN, TIMECRIS_GUN_X_MAX )
	PORT_START	/* OPT.1: gun Y */
	PORT_ANALOG( 0xffff, TIMECRIS_GUN_Y_MIN + 241 / 2, IPT_LIGHTGUN_Y | IPF_PLAYER1, 64, 4, TIMECRIS_GUN_Y_MIN, TIMECRIS_GUN_Y_MAX )
INPUT_PORTS_END

INPUT_PORTS_START( propcycl )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xfe00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	NAMCOS22S_DSW_UNKNOWN
	PORT_DIPNAME( 0x80, 0x80, "SW4:8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* ADC.0: stick X */
	PORT_ANALOG( 0x3ff, 0x1ff, IPT_AD_STICK_X | IPF_REVERSE, 100, 40, 0x0bf, 0x33f )
	PORT_START	/* ADC.1: stick Y */
	PORT_ANALOG( 0x3ff, 0x1ff, IPT_AD_STICK_Y, 100, 40, 0x0bf, 0x33f )
	PORT_START	/* OPT.0: cycle pedal (forward speed; MAME's 0x80-0xff range) */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL, 100, 10, 0x00, 0x7f )
INPUT_PORTS_END


INPUT_PORTS_START( ridgera2 )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )		/* shift down */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	/* shift up */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )	/* deluxe cabinet only */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON3 )			/* clutch pedal */
	PORT_BIT( 0x00e0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x2100, 0x2000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, "50 Inch" )
	PORT_DIPSETTING(      0x0100, "Twin" )
	PORT_DIPSETTING(      0x2000, "Standard" )
	PORT_DIPSETTING(      0x2100, "Deluxe" )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x0400, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xc000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* SW2 */
	PORT_DIPNAME( 0x01, 0x01, "Test Mode" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* SW3 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x80, 0x80, "Test Mode 2" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* ADC.0: steering */
	PORT_ANALOG( 0xfff, 0x800, IPT_PADDLE, 100, 160, 0x280, 0xd80 )
	PORT_START	/* ADC.1: gas */
	PORT_ANALOG( 0xfff, 0x000, IPT_PEDAL, 100, 160, 0x000, 0x610 )
	PORT_START	/* ADC.2: brake */
	PORT_ANALOG( 0xfff, 0x000, IPT_PEDAL2, 100, 160, 0x000, 0x610 )
INPUT_PORTS_END


#define S22_SW_UNUSED \
	PORT_START	/* SW2 */ \
	PORT_DIPNAME( 0x01, 0x01, "Test Mode" ) \
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_START	/* SW3 */ \
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

#define S22_DRIVING_ANALOG(steer_min, steer_max, steer_delta, gas_max, gas_delta, brake_max, brake_delta) \
	PORT_START	/* ADC.0: steering */ \
	PORT_ANALOG( 0xfff, 0x800, IPT_PADDLE, 100, steer_delta, steer_min, steer_max ) \
	PORT_START	/* ADC.1: gas */ \
	PORT_ANALOG( 0xfff, 0x000, IPT_PEDAL, 100, gas_delta, 0x000, gas_max ) \
	PORT_START	/* ADC.2: brake */ \
	PORT_ANALOG( 0xfff, 0x000, IPT_PEDAL2, 100, brake_delta, 0x000, brake_max )

INPUT_PORTS_START( ridgera )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )		/* shift down */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	/* shift up */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )	/* deluxe cabinet only */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON3 )			/* clutch pedal */
	PORT_BIT( 0x00e0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, "Standard" )
	PORT_DIPSETTING(      0x0100, "Deluxe" )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x0400, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	S22_SW_UNUSED
	S22_DRIVING_ANALOG(0x280, 0xd80, 160, 0x610, 160, 0x610, 160)
INPUT_PORTS_END

INPUT_PORTS_START( raverace )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )		/* shift down */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	/* shift up */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )	/* deluxe cabinet only */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON3 )			/* clutch pedal */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON4 )			/* view change */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x2100, 0x2000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, "50 Inch" )
	PORT_DIPSETTING(      0x0100, "Twin" )
	PORT_DIPSETTING(      0x2000, "Standard" )
	PORT_DIPSETTING(      0x2100, "Deluxe" )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x0400, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xc000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	S22_SW_UNUSED
	S22_DRIVING_ANALOG(0x280, 0xd80, 160, 0x610, 160, 0x610, 160)
INPUT_PORTS_END

INPUT_PORTS_START( acedrive )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )		/* shift down */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	/* shift up */
	PORT_BIT( 0x003c, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )			/* view change */
	PORT_BIT( 0x0180, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x0400, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 )			/* motion stop */

	PORT_START	/* SW2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* SW3: test mode is SW3:7 + SW3:8 */
	PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x40, 0x40, "Test Mode (SW3:7)" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Test Mode (SW3:8)" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	S22_DRIVING_ANALOG(0x200, 0xe00, 160, 0x480, 120, 0x240, 80)
INPUT_PORTS_END

/* the test mode shows +-224: the EEPROM is calibrated for these ranges */
INPUT_PORTS_START( cybrcomm )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 )			/* gun trigger */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2 )			/* missile button */
	PORT_BIT( 0x003c, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )			/* view change */
	PORT_BIT( 0x0180, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x0400, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* SW2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* SW3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC.0: right joystick, vertical */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 10, 0x47, 0xb7 )
	PORT_START	/* ADC.1: left joystick, vertical */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0x47, 0xb7 )
	PORT_START	/* ADC.2: right joystick, horizontal */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_PLAYER2, 100, 10, 0x47, 0xb7 )
	PORT_START	/* ADC.3: left joystick, horizontal */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0x47, 0xb7 )
INPUT_PORTS_END

/* Super System 22 */

#define SS22_DSW_TEST \
	NAMCOS22S_DSW_UNKNOWN \
	PORT_DIPNAME( 0x80, 0x80, "SW4:8" ) \
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

/* SW4:1 is the test mode */
#define SS22_DSW_TEST1 \
	PORT_START \
	PORT_DIPNAME( 0x01, 0x01, "Test Mode" ) \
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNUSED )

/* bits 7 and 8 are the steplock motor state, set by the driver */
INPUT_PORTS_START( alpiner )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START1 )			/* decision / view change */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )	/* L selection */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )	/* R selection */
	PORT_BIT( 0x0180, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0xfe00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST

	PORT_START	/* ADC.0: steps swing */
	PORT_ANALOG( 0x3ff, 0x200, IPT_AD_STICK_X, 100, 16, 0x080, 0x380 )
	PORT_START	/* ADC.1: steps edge */
	PORT_ANALOG( 0x3ff, 0x200, IPT_AD_STICK_X | IPF_PLAYER2, 100, 16, 0x080, 0x380 )
INPUT_PORTS_END

INPUT_PORTS_START( alpines )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START1 )			/* decision / view change */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )	/* L selection */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )	/* R selection */
	PORT_BIT( 0x0180, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0xfe00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST

	PORT_START	/* ADC.0: steps swing */
	PORT_ANALOG( 0x3ff, 0x200, IPT_AD_STICK_X | IPF_REVERSE, 100, 16, 0x080, 0x380 )
	PORT_START	/* ADC.1: steps edge */
	PORT_ANALOG( 0x3ff, 0x200, IPT_AD_STICK_X | IPF_PLAYER2, 100, 16, 0x080, 0x380 )
INPUT_PORTS_END

INPUT_PORTS_START( airco22 )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 )			/* missile button */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 )			/* gun trigger */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_START1 )			/* also view change */
	PORT_BIT( 0xff80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST

	PORT_START	/* ADC.0: stick X */
	PORT_ANALOG( 0x3ff, 0x200, IPT_AD_STICK_X, 100, 12, 0x100, 0x300 )
	PORT_START	/* ADC.1: stick Y */
	PORT_ANALOG( 0x3ff, 0x200, IPT_AD_STICK_Y, 100, 12, 0x100, 0x300 )
	PORT_START	/* ADC.2 = 0x200 + this - next: throttle stick, auto-centering */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL, 100, 12, 0x000, 0x100 )
	PORT_START
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL2, 100, 12, 0x000, 0x100 )
INPUT_PORTS_END

INPUT_PORTS_START( cybrcycc )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START1 )			/* also view change */
	PORT_BIT( 0xffe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST1

	PORT_START	/* ADC.0: bike bank */
	PORT_ANALOG( 0x3ff, 0x200, IPT_PADDLE, 100, 40, 0x004, 0x3fc )
	PORT_START	/* ADC.1: throttle grip */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL, 100, 40, 0x000, 0x300 )
	PORT_START	/* ADC.2: brake lever */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL2, 100, 40, 0x000, 0x100 )
INPUT_PORTS_END

INPUT_PORTS_START( dirtdash )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON3 )			/* view change */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	/* shift up */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )		/* shift down */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 )			/* motion stop */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0200, 0x0000, DEF_STR( Cabinet ) )		/* J4 pin 6 on the mother PCB */
	PORT_DIPSETTING(      0x0000, "Standard" )
	PORT_DIPSETTING(      0x0200, "Deluxe" )				/* motion cabinet */
	PORT_BIT( 0xfc00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST

	PORT_START	/* ADC.0: steering */
	PORT_ANALOG( 0x3ff, 0x200, IPT_PADDLE, 100, 12, 0x001, 0x3ff )
	PORT_START	/* ADC.1: gas */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL, 100, 40, 0x000, 0x140 )
	PORT_START	/* ADC.2: brake */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL2, 100, 40, 0x000, 0x100 )
INPUT_PORTS_END

INPUT_PORTS_START( tokyowar )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START1 )			/* also view change */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )			/* right trigger */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 )			/* left trigger */
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0080, "Standard" )
	PORT_DIPSETTING(      0x0000, "Deluxe" )				/* cannon recoil motors */
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST1

	PORT_START	/* ADC.0: steering */
	PORT_ANALOG( 0x3ff, 0x200, IPT_PADDLE, 100, 20, 0x100, 0x300 )
	PORT_START	/* ADC.2: forward pedal */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL, 100, 20, 0x000, 0x100 )
	PORT_START	/* ADC.3: backward pedal */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL2, 100, 20, 0x000, 0x100 )
INPUT_PORTS_END

INPUT_PORTS_START( aquajet )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xffe0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST1

	PORT_START	/* ADC.0: handle bar */
	PORT_ANALOG( 0x3ff, 0x1fc, IPT_PADDLE | IPF_REVERSE, 100, 40, 0x000, 0x3f8 )
	PORT_START	/* ADC.1 = 0x1fc - this: throttle */
	PORT_ANALOG( 0x3ff, 0x000, IPT_PEDAL, 100, 40, 0x000, 0x11c )
	PORT_START	/* ADC.2: body lean */
	PORT_ANALOG( 0x3ff, 0x1fc, IPT_AD_STICK_Y | IPF_REVERSE, 100, 40, 0x000, 0x3f8 )
INPUT_PORTS_END

INPUT_PORTS_START( adillor )
	PORT_START	/* INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x0008, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xfe00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SS22_DSW_TEST1

	PORT_START	/* OPT.0 */
	PORT_ANALOG( 0xffff, 0x0000, IPT_TRACKBALL_X, 100, 16, 0, 0 )
	PORT_START	/* OPT.1 */
	PORT_ANALOG( 0xffff, 0x0000, IPT_TRACKBALL_Y | IPF_REVERSE, 100, 16, 0, 0 )
	PORT_START
	PORT_DIPNAME( 0x02, 0x02, "Trackball Orientation" )
	PORT_DIPSETTING(    0x00, "Direct" )
	PORT_DIPSETTING(    0x02, "Natural" )
INPUT_PORTS_END


/***************************************************************************
    ROMs
***************************************************************************/

#define C71_BIOS \
	ROM_REGION16_BE( 0x2000, REGION_CPU2, 0 )	/* master DSP internal ROM */ \
	ROM_LOAD( "c71.bin", 0, 0x2000, CRC(47c623ab) SHA1(e363ac50f5556f83308d4cc191b455e9b62bcfc8) ) \
	ROM_REGION16_BE( 0x2000, REGION_CPU3, 0 )	/* slave DSP internal ROM */ \
	ROM_LOAD( "c71.bin", 0, 0x2000, CRC(47c623ab) SHA1(e363ac50f5556f83308d4cc191b455e9b62bcfc8) )

#define TIMECRIS_DATA \
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 )	/* MCU: S22-BIOS ver1.30 and data */ \
	ROM_LOAD( "ts1data.8k", 0, 0x080000, CRC(e68aa973) SHA1(663e80d249be5d5841139d98a9d72e2396851272) ) \
	C71_BIOS \
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */ \
	ROM_LOAD( "ts1scg0.12f", 0x000000, 0x200000, CRC(14a3674d) SHA1(c5792a385572452b43bbc7eb8428335b19daa3c0) ) \
	ROM_LOAD( "ts1scg1.10f", 0x200000, 0x200000, CRC(11791dbf) SHA1(3d75b468d69a8bf398d45f310cdb8bc88b63f25c) ) \
	ROM_LOAD( "ts1scg2.8f",  0x400000, 0x200000, CRC(d630fff9) SHA1(691394027b858702f06282f965f5b53e6fed496b) ) \
	ROM_LOAD( "ts1scg3.7f",  0x600000, 0x200000, CRC(1a62f015) SHA1(7d09ae480ae7813391616ae0090929ba845a345a) ) \
	ROM_LOAD( "ts1scg4.5f",  0x800000, 0x200000, CRC(511b8dd6) SHA1(936649c0a61d29f024a28e4ab64cce4b55d58f64) ) \
	ROM_LOAD( "ts1scg5.3f",  0xa00000, 0x200000, CRC(553bb246) SHA1(94659bee4fd0afe834a8bf3414d8825411cf9e86) ) \
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */ \
	ROM_LOAD( "ts1cg0.8d",   0x000000, 0x200000, CRC(de07b22c) SHA1(f4d07b8840ec8be625eff634bce619e960c334a5) ) \
	ROM_LOAD( "ts1cg1.10d",  0x200000, 0x200000, CRC(992d26f6) SHA1(a0b9007312804b413d4c1748527378da4d8d53b3) ) \
	ROM_LOAD( "ts1cg2.12d",  0x400000, 0x200000, CRC(6273954f) SHA1(d73a43888b53e4c42fc33e8e1b38e60fd3329413) ) \
	ROM_LOAD( "ts1cg3.13d",  0x600000, 0x200000, CRC(38171f24) SHA1(d04caaa5b1b377ced38501b014e4cb7fc831c41d) ) \
	ROM_LOAD( "ts1cg4.14d",  0x800000, 0x200000, CRC(51f09856) SHA1(0eef421907ee813d5117e62cf0005bf00eb29c88) ) \
	ROM_LOAD( "ts1cg5.16d",  0xa00000, 0x200000, CRC(4cd9fd79) SHA1(0d2018ec914683a75bdec8655d678fd562eb6d15) ) \
	ROM_LOAD( "ts1cg6.18d",  0xc00000, 0x200000, CRC(f17f2ec9) SHA1(ed88ec524626e5bbe2e1ea6838412d3ac85671dd) ) \
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap: 16-bit little endian words, then packed attribute nibbles */ \
	ROM_LOAD( "ts1ccrl.3d",  0x000000, 0x200000, CRC(56cad2df) SHA1(49c0e57d5cf5d5fc4c75da6969bec01d6d443259) ) \
	ROM_LOAD( "ts1ccrh.1d",  0x200000, 0x080000, CRC(a1cc3741) SHA1(7fe57924c42e287b134e5d7ad00cffdff1f18084) ) \
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */ \
	ROM_LOAD( "ts1ptrl0.18k", 0x000000, 0x080000, CRC(e5f2d275) SHA1(2f5057e65ec8a3ec03f841f15f10769ae1f69139) ) \
	ROM_LOAD( "ts1ptrl1.16k", 0x080000, 0x080000, CRC(2bba3800) SHA1(1d9c944cb06417cb0ac47a58b922dddb83387586) ) \
	ROM_LOAD( "ts1ptrl2.15k", 0x100000, 0x080000, CRC(d4441c08) SHA1(6a6bb9cecbf35cb81b7681e220fc33df9a01d07f) ) \
	ROM_LOAD( "ts1ptrm0.18j", 0x180000, 0x080000, CRC(8aea02ba) SHA1(44ba85ba6d59758448d17ec39dfb628882ddc684) ) \
	ROM_LOAD( "ts1ptrm1.16j", 0x200000, 0x080000, CRC(bccf19bc) SHA1(4a6566948bdd2b0f82b7c30e57d3fe65005c26e3) ) \
	ROM_LOAD( "ts1ptrm2.15j", 0x280000, 0x080000, CRC(7280be31) SHA1(476b7171ae855d8bbd968ccbaa55b5100d274e3b) ) \
	ROM_LOAD( "ts1ptru0.18f", 0x300000, 0x080000, CRC(c30d6332) SHA1(a5c59d0abfe38de975fa0d606ed8500eb02008b7) ) \
	ROM_LOAD( "ts1ptru1.16f", 0x380000, 0x080000, CRC(993cde84) SHA1(c9cdcca1d60bcc41ad881c02dda9895563963ead) ) \
	ROM_LOAD( "ts1ptru2.15f", 0x400000, 0x080000, CRC(7cb25c73) SHA1(616eab3ac238864a584394f7ec8736ece227974a) ) \
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */ \
	ROM_LOAD16_WORD_SWAP( "ts1wavea.2l", 0x000000, 0x400000, CRC(d1123301) SHA1(4bf1fd746fef4e6befa63c61a761971d729e1573) ) \
	ROM_LOAD( "ts1waveb.1l", 0x800000, 0x200000, CRC(bf4d7272) SHA1(c7c7b3620e7b3176644b6784ee36e679c9e31cc1) )

#define NAMCO_C74_BIOS \
	ROM_REGION( 0x4000, REGION_USER4, 0 )	/* C74 internal BIOS */ \
	ROM_LOAD( "c74.bin", 0x0000, 0x4000, CRC(a3dce360) SHA1(8f3248b1890abb2e649927240ae46f73bb171e3b) )

ROM_START( namcoc74 )
	NAMCO_C74_BIOS
ROM_END

#define RIDGERA2_DATA \
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 )	/* sound data */ \
	ROM_LOAD( "rrs1data.6r", 0, 0x080000, CRC(b7063aa8) SHA1(08ff689e8dd529b91eee423c93f084945c6de417) ) \
	C71_BIOS \
	NAMCO_C74_BIOS \
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */ \
	ROM_LOAD( "rrs1cg0.1a", 0x800000, 0x200000, CRC(714c0091) SHA1(df29512bd6e64827660c40304051366d2c4d7977) ) \
	ROM_LOAD( "rrs1cg1.2a", 0xa00000, 0x200000, CRC(836545c1) SHA1(05e3346463d8d42b5d33216207e855033a65510d) ) \
	ROM_LOAD( "rrs1cg2.3a", 0xc00000, 0x200000, CRC(00e9799d) SHA1(280184451138420f64080efe13e5e2795f7b61d4) ) \
	ROM_LOAD( "rrs1cg3.5a", 0xe00000, 0x200000, CRC(3858983f) SHA1(feda270b71f1310ecf4c17823bc8827ca2951b40) ) \
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */ \
	ROM_LOAD( "rrs1ccrl.5a", 0x000000, 0x200000, CRC(304a8b57) SHA1(f4f3e7c194697d754375f36a0e41d0941fa5d225) ) \
	ROM_LOAD( "rrs1ccrh.5c", 0x200000, 0x080000, CRC(bd3c86ab) SHA1(cd3a8774843c5864e651fa8989c80e2d975a13e8) ) \
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */ \
	ROM_LOAD( "rrs1pol0.5b", 0x000000, 0x080000, CRC(9376c384) SHA1(cde0e36db1beab1523607098a760d81fac2ea90e) ) \
	ROM_LOAD( "rrs1pol1.4b", 0x080000, 0x080000, CRC(094fa832) SHA1(cc59442540b1cdef068c4408b6e048c11042beb8) ) \
	ROM_LOAD( "rrs1pom0.5c", 0x100000, 0x080000, CRC(b47a7f8b) SHA1(0fa0456ad8b4864a7071b5b5ba1a78877c1ac0f0) ) \
	ROM_LOAD( "rrs1pom1.4c", 0x180000, 0x080000, CRC(27260361) SHA1(8775cc779eb8b6a0d79fa84d606c970ec2d6ea8d) ) \
	ROM_LOAD( "rrs1pou0.5d", 0x200000, 0x080000, CRC(74d6ec84) SHA1(63f5beee51443c98100330ec04291f71e10716c4) ) \
	ROM_LOAD( "rrs1pou1.4d", 0x280000, 0x080000, CRC(f527caaa) SHA1(f92bdd15323239d593ddac92a11d23a27e6635ed) ) \
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */ \
	ROM_LOAD( "rrs1wav0.10r", 0x000000, 0x100000, CRC(99d11a2d) SHA1(1f3db98a99be0f07c03b0a7817561459a58f310e) ) \
	ROM_LOAD( "rrs1wav1.10p", 0x200000, 0x100000, CRC(ad28444a) SHA1(c31bbf3cae5015e5494fe4988b9b01d822224c69) ) \
	ROM_LOAD( "rrs1wav2.10n", 0x100000, 0x100000, CRC(6f0d4619) SHA1(cd3d57f2ea21497f388ffa29ec7d2665647a01c0) ) \
	ROM_LOAD( "rrs1wav3.10l", 0x300000, 0x100000, CRC(106e761f) SHA1(97f47b857bdcbc79b0aface53dd385e67fcc9108) ) \
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */ \
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) ) \
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) ) \
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )

ROM_START( ridgera2 )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rrs2_prgll.4d", 0x00003, 0x80000, CRC(88199c0f) SHA1(5cf5bb714c3d209943a8d815eaea60afd34641ff) )
	ROM_LOAD32_BYTE( "rrs2_prglm.2d", 0x00002, 0x80000, CRC(8e86f199) SHA1(7bd9ec9147ef0380864508f66203ef2c6ad1f7f6) )
	ROM_LOAD32_BYTE( "rrs2_prgum.8d", 0x00001, 0x80000, CRC(78c360b6) SHA1(8ee502291359cbc8aef39145c8fe7538311cc58f) )
	ROM_LOAD32_BYTE( "rrs2_prguu.6d", 0x00000, 0x80000, CRC(60d6d4a4) SHA1(759762a9b7d7aee7ee1b44b1721e5356898aa7ea) )
	RIDGERA2_DATA
ROM_END

ROM_START( ridgera2j )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rrs1_prgllb.4d", 0x00003, 0x80000, CRC(22f069e5) SHA1(fcaec3aa83853c39d713ed01c511060663027ccd) )
	ROM_LOAD32_BYTE( "rrs1_prglmb.2d", 0x00002, 0x80000, CRC(8e86f199) SHA1(7bd9ec9147ef0380864508f66203ef2c6ad1f7f6) )
	ROM_LOAD32_BYTE( "rrs1_prgumb.8d", 0x00001, 0x80000, CRC(78c360b6) SHA1(8ee502291359cbc8aef39145c8fe7538311cc58f) )
	ROM_LOAD32_BYTE( "rrs1_prguub.6d", 0x00000, 0x80000, CRC(60d6d4a4) SHA1(759762a9b7d7aee7ee1b44b1721e5356898aa7ea) )
	RIDGERA2_DATA
ROM_END

ROM_START( ridgera2ja )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rrs1_prgll.4d", 0x00003, 0x80000, CRC(fbf785a2) SHA1(b9333c7623f68f48aa6ae50913a22a527a19576a) )
	ROM_LOAD32_BYTE( "rrs1_prglm.2d", 0x00002, 0x80000, CRC(562f747a) SHA1(79d818b87c9a992fc9706fb39e6d560c2b0aa392) )
	ROM_LOAD32_BYTE( "rrs1_prgum.8d", 0x00001, 0x80000, CRC(93259fb0) SHA1(c29787e873797a003db27adbd20d7b852e26d8c6) )
	ROM_LOAD32_BYTE( "rrs1_prguu.6d", 0x00000, 0x80000, CRC(31cdefe8) SHA1(ae836d389bed43dd156eb4cf3e97b6f1ad68181e) )
	RIDGERA2_DATA
ROM_END


ROM_START( timecris )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ts2ver-b.1", 0x00003, 0x100000, CRC(29b377f7) SHA1(21864ba964602115c1268fd5edd8006a13a86cfc) )
	ROM_LOAD32_BYTE( "ts2ver-b.2", 0x00002, 0x100000, CRC(79512e25) SHA1(137a215ec192e76e93511456ad504481a566c9c9) )
	ROM_LOAD32_BYTE( "ts2ver-b.3", 0x00001, 0x100000, CRC(9f4ced33) SHA1(32768b5ff263a9e3d11b7b36f6b2d7e951e07419) )
	ROM_LOAD32_BYTE( "ts2ver-b.4", 0x00000, 0x100000, CRC(3e0cfb38) SHA1(3c56342bd73b1617ea579a0d53e19d59bb04fd99) )
	TIMECRIS_DATA
ROM_END

ROM_START( timecrisa )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_WORD_SWAP( "ts2ver-a.1", 0x00002, 0x200000, CRC(d57eb74b) SHA1(536dd9305d0ac44110c575776333310cc57b5242) )
	ROM_LOAD32_WORD_SWAP( "ts2ver-a.2", 0x00000, 0x200000, CRC(671588af) SHA1(63f992c6795521fd263a0ebf230f8dc88cbfc443) )
	TIMECRIS_DATA
ROM_END

#define PROPCYCL_DATA \
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 )	/* MCU: SS22-BIOS ver1.41 and data */ \
	ROM_LOAD( "pr1data.8k", 0, 0x080000, CRC(2e5767a4) SHA1(390bf05c90044d841fe2dd4a427177fa1570b9a6) ) \
	C71_BIOS \
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */ \
	ROM_LOAD( "pr1scg0.12f",  0x000000, 0x200000, CRC(2d09a869) SHA1(ce8beabaac255e1de29d944c9866022bad713519) ) \
	ROM_LOAD( "pr1scg1.10f",  0x200000, 0x200000, CRC(7433c5bd) SHA1(a8fd4e73de66e3d443c0cb5b5beef8f467014815) ) \
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */ \
	ROM_LOAD( "pr1cg0.12b",  0x000000, 0x200000, CRC(0a041238) SHA1(da5688970432f7fe39337ee9fb46ca25a53fdb11) ) \
	ROM_LOAD( "pr1cg1.10d",  0x200000, 0x200000, CRC(7d09e6a7) SHA1(892317ee0bd796fa5c70d64912ef2e696792a2d4) ) \
	ROM_LOAD( "pr1cg2.12d",  0x400000, 0x200000, CRC(659f006e) SHA1(23362a922cb1100950181fac4858b953d8fc0794) ) \
	ROM_LOAD( "pr1cg3.13d",  0x600000, 0x200000, CRC(d30bffa3) SHA1(2f05227d91d257db9fa8cae114974de602d98729) ) \
	ROM_LOAD( "pr1cg4.14d",  0x800000, 0x200000, CRC(f4636cc9) SHA1(4e01a476e418e5790878572e83a8a11536ce30ae) ) \
	ROM_LOAD( "pr1cg5.16d",  0xa00000, 0x200000, CRC(97d333de) SHA1(e8f8383f49aae834dd8b57231b25899703cef966) ) \
	ROM_LOAD( "pr1cg6.18a",  0xc00000, 0x200000, CRC(3e081c03) SHA1(6ccb162952f6076359b2785b5d800b39a9a3c5ce) ) \
	ROM_LOAD( "pr1cg7.15a",  0xe00000, 0x200000, CRC(ec9fc5c8) SHA1(16de614b26f06bbddae3ab56cebba45efd6fe81b) ) \
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */ \
	ROM_LOAD( "pr1ccrl.3d",  0x000000, 0x200000, CRC(e01321fd) SHA1(5938c6eff8e1b3642728c3be733f567a97cb5aad) ) \
	ROM_LOAD( "pr1ccrh.1d",  0x200000, 0x080000, CRC(1d68bc31) SHA1(d534d0daebe7018e83b57cc7919c294ab89bddc8) ) \
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */ \
	ROM_LOAD( "pr1ptrl0.18k", 0x000000, 0x080000, CRC(fddb27a2) SHA1(6e837b45e3f9ed7ca3d1a457d0f0124de5618d1f) ) \
	ROM_LOAD( "pr1ptrl1.16k", 0x080000, 0x080000, CRC(6964dd06) SHA1(f38a550165504693d20892a7dcfaf01db19b04ef) ) \
	ROM_LOAD( "pr1ptrl2.15k", 0x100000, 0x080000, CRC(4d7ed1d4) SHA1(8f72864a06ff8962e640cb36d062bddf5d110308) ) \
	ROM_LOAD( "pr1ptrm0.18j", 0x180000, 0x080000, CRC(b6f204b7) SHA1(3b34f240b399b6406faaf338ae0ab536247e64a6) ) \
	ROM_LOAD( "pr1ptrm1.16j", 0x200000, 0x080000, CRC(949588b7) SHA1(fdaf50ff2496200b9c981efc18b035f3c0a96ace) ) \
	ROM_LOAD( "pr1ptrm2.15j", 0x280000, 0x080000, CRC(dc1cef0a) SHA1(8cbc02cf73fac3cc110b676d77602ae628385eae) ) \
	ROM_LOAD( "pr1ptru0.18f", 0x300000, 0x080000, CRC(5d66a7c4) SHA1(c9ed1c18724192d45c1f6b40096f15d02baf2401) ) \
	ROM_LOAD( "pr1ptru1.16f", 0x380000, 0x080000, CRC(e9a3f72b) SHA1(f967e1adf8eee4fffdf4288d36a93c5bb4f9a126) ) \
	ROM_LOAD( "pr1ptru2.15f", 0x400000, 0x080000, CRC(c346a842) SHA1(299bc0a30d0e74d8adfa3dc605aebf6439f5bc18) ) \
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */ \
	ROM_LOAD( "pr1wavea.2l", 0x000000, 0x400000, CRC(320f3913) SHA1(3887b7334ca7762794c14198dd24bab47fcd9505) ) \
	ROM_LOAD( "pr1waveb.1l", 0x800000, 0x400000, CRC(d91acb26) SHA1(c2161e2d70e08aed15cbc875ffee685190611daf) )

ROM_START( propcycl )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "pr2ver-a.1", 0x00003, 0x100000, CRC(3f58594c) SHA1(5fdd8c61b47b51088a201799ce0c2f08c32ef852) )
	ROM_LOAD32_BYTE( "pr2ver-a.2", 0x00002, 0x100000, CRC(c0da354a) SHA1(f27a71a62385b842404fcd8ed6513158e3639b8f) )
	ROM_LOAD32_BYTE( "pr2ver-a.3", 0x00001, 0x100000, CRC(74bf4b74) SHA1(02713aa07238cc9e30163ae24d12c034aa972ff3) )
	ROM_LOAD32_BYTE( "pr2ver-a.4", 0x00000, 0x100000, CRC(cf4d5638) SHA1(2ddd00d6ec3b85c234820507650d201e176c94a2) )
	PROPCYCL_DATA
ROM_END

ROM_START( propcyclj )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "pr1ver-a.1", 0x00003, 0x100000, CRC(dfea7ff7) SHA1(9c81b145d0b6fbe14e1c5ce93ea67dacb2567220) )
	ROM_LOAD32_BYTE( "pr1ver-a.2", 0x00002, 0x100000, CRC(84a6f608) SHA1(c05ff3fce709bcdb7c30f019ae794979ba79970c) )
	ROM_LOAD32_BYTE( "pr1ver-a.3", 0x00001, 0x100000, CRC(77f957d2) SHA1(c8a62464f427cc858d9ea33446a8648e9171ead9) )
	ROM_LOAD32_BYTE( "pr1ver-a.4", 0x00000, 0x100000, CRC(7b6844e4) SHA1(1b1c03d258c4a6d95ae227de7741f29b7d9e28d8) )
	PROPCYCL_DATA
ROM_END


ROM_START( ridgerac )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rr2_prgllb.4d", 0x00003, 0x80000, CRC(d50d4fa0) SHA1(0fcd55e653d4f596b38b4e7b876ccd1f2a3836df) )
	ROM_LOAD32_BYTE( "rr2_prglmb.2d", 0x00002, 0x80000, CRC(6a266f42) SHA1(470613dbddd6b3050f666de9a2b222a4b7e1e809) )
	ROM_LOAD32_BYTE( "rr2_prgumb.8d", 0x00001, 0x80000, CRC(64c3aff1) SHA1(46b30ee0601a0a7887d29bc94d595f50a5a812d6) )
	ROM_LOAD32_BYTE( "rr2_prguub.6d", 0x00000, 0x80000, CRC(e6ff0b8d) SHA1(a02ffd2d3dc0a2c4a5665303c8021fd99a50eb8e) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rr1data.6r", 0, 0x080000, CRC(18f5f748) SHA1(e0d149a66de36156edd9b55f604c9a9801aaefa8) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rr1cg0.bin", 0x800000, 0x200000, CRC(b557a795) SHA1(f345486ffbe797246ad80a55d3c4a332ed6e2888) )
	ROM_LOAD( "rr1cg1.bin", 0xa00000, 0x200000, CRC(0fa212d9) SHA1(a1311de0a504e2d399044fa8ac32ec6c56ec965f) )
	ROM_LOAD( "rr1cg2.bin", 0xc00000, 0x200000, CRC(18e2d2bd) SHA1(69c2ea62eeb255f27d3c69373f6716b0a34683cc) )
	ROM_LOAD( "rr1cg3.bin", 0xe00000, 0x200000, CRC(9564488b) SHA1(6b27d1aea75d6be747c62e165cfa49ecc5d9e767) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rr1ccrl.bin", 0x000000, 0x200000, CRC(6092d181) SHA1(52c0e3ac20aa23059a87d1a985d24ae641577310) )
	ROM_LOAD( "rr1ccrh.bin", 0x200000, 0x080000, CRC(dd332fd5) SHA1(a7d9c1d6b5a8e3a937b525c1363880e404dcd147) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rr1potl0.5b", 0x000000, 0x080000, CRC(3ac193e3) SHA1(ff213766f15e34dc1b25187b57d94e17930090a3) )
	ROM_LOAD( "rr1potl1.4b", 0x080000, 0x080000, CRC(ac3ffba5) SHA1(4eb4dda5faeff237e0d35725b56d309948fba900) )
	ROM_LOAD( "rr1potm0.5c", 0x100000, 0x080000, CRC(42a3fa08) SHA1(15db0ae7ccf7f5a77b9dd9a9d82a488b67f8aaff) )
	ROM_LOAD( "rr1potm1.4c", 0x180000, 0x080000, CRC(1bc1ea7b) SHA1(52c21eef4989c45acc5fa4deda2d0b63214731c8) )
	ROM_LOAD( "rr1potu0.5d", 0x200000, 0x080000, CRC(5e367f72) SHA1(5887f011379dce865fef238b402678a3d2033de9) )
	ROM_LOAD( "rr1potu1.4d", 0x280000, 0x080000, CRC(31d92475) SHA1(51d3c0baa223e1bc16ea2950f2e085597528f870) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rr1wav0.10r", 0x000000, 0x100000, CRC(a8e85bde) SHA1(b56677e9f6c98f7b600043f5dcfef3a482ca7455) )
	ROM_LOAD( "rr1wav1.10p", 0x200000, 0x100000, CRC(35f47c8e) SHA1(7c3f9e942f532af8008fbead2a96fee6084bcde6) )
	ROM_LOAD( "rr1wav2.10n", 0x100000, 0x100000, CRC(3244cb59) SHA1(b3283b30cfafbfdcbc6d482ecc4ed6a47a527ca4) )
	ROM_LOAD( "rr1wav3.10l", 0x300000, 0x100000, CRC(c4cda1a7) SHA1(60bc96880ec79efdff3cc70c09e848692a40bea4) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( ridgeraca )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rr2_prgll.4d", 0x00003, 0x80000, CRC(4bb7fc86) SHA1(8291375b8ec4d37e0d9e3bf38da2d5907b0f31bd) )
	ROM_LOAD32_BYTE( "rr2_prglm.2d", 0x00002, 0x80000, CRC(68e13830) SHA1(ddc447c7afbb5c4238969d7e78bfe9cf8fac6061) )
	ROM_LOAD32_BYTE( "rr2_prgum.8d", 0x00001, 0x80000, CRC(705ef78a) SHA1(881903413e66d6fd83d46eb18c4e1230531832ae) )
	ROM_LOAD32_BYTE( "rr2_prguu.6d", 0x00000, 0x80000, CRC(a79e456f) SHA1(049c596e01e53e3a401c5c4260517f170688d387) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rr1data.6r", 0, 0x080000, CRC(18f5f748) SHA1(e0d149a66de36156edd9b55f604c9a9801aaefa8) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rr1cg0.bin", 0x800000, 0x200000, CRC(b557a795) SHA1(f345486ffbe797246ad80a55d3c4a332ed6e2888) )
	ROM_LOAD( "rr1cg1.bin", 0xa00000, 0x200000, CRC(0fa212d9) SHA1(a1311de0a504e2d399044fa8ac32ec6c56ec965f) )
	ROM_LOAD( "rr1cg2.bin", 0xc00000, 0x200000, CRC(18e2d2bd) SHA1(69c2ea62eeb255f27d3c69373f6716b0a34683cc) )
	ROM_LOAD( "rr1cg3.bin", 0xe00000, 0x200000, CRC(9564488b) SHA1(6b27d1aea75d6be747c62e165cfa49ecc5d9e767) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rr1ccrl.bin", 0x000000, 0x200000, CRC(6092d181) SHA1(52c0e3ac20aa23059a87d1a985d24ae641577310) )
	ROM_LOAD( "rr1ccrh.bin", 0x200000, 0x080000, CRC(dd332fd5) SHA1(a7d9c1d6b5a8e3a937b525c1363880e404dcd147) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rr1potl0.5b", 0x000000, 0x080000, CRC(3ac193e3) SHA1(ff213766f15e34dc1b25187b57d94e17930090a3) )
	ROM_LOAD( "rr1potl1.4b", 0x080000, 0x080000, CRC(ac3ffba5) SHA1(4eb4dda5faeff237e0d35725b56d309948fba900) )
	ROM_LOAD( "rr1potm0.5c", 0x100000, 0x080000, CRC(42a3fa08) SHA1(15db0ae7ccf7f5a77b9dd9a9d82a488b67f8aaff) )
	ROM_LOAD( "rr1potm1.4c", 0x180000, 0x080000, CRC(1bc1ea7b) SHA1(52c21eef4989c45acc5fa4deda2d0b63214731c8) )
	ROM_LOAD( "rr1potu0.5d", 0x200000, 0x080000, CRC(5e367f72) SHA1(5887f011379dce865fef238b402678a3d2033de9) )
	ROM_LOAD( "rr1potu1.4d", 0x280000, 0x080000, CRC(31d92475) SHA1(51d3c0baa223e1bc16ea2950f2e085597528f870) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rr1wav0.10r", 0x000000, 0x100000, CRC(a8e85bde) SHA1(b56677e9f6c98f7b600043f5dcfef3a482ca7455) )
	ROM_LOAD( "rr1wav1.10p", 0x200000, 0x100000, CRC(35f47c8e) SHA1(7c3f9e942f532af8008fbead2a96fee6084bcde6) )
	ROM_LOAD( "rr1wav2.10n", 0x100000, 0x100000, CRC(3244cb59) SHA1(b3283b30cfafbfdcbc6d482ecc4ed6a47a527ca4) )
	ROM_LOAD( "rr1wav3.10l", 0x300000, 0x100000, CRC(c4cda1a7) SHA1(60bc96880ec79efdff3cc70c09e848692a40bea4) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( ridgeracb )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rr3_prgllb.4d", 0x00003, 0x80000, CRC(856fe5ec) SHA1(72d95b8bd5da551c3d358b8ab266373a89f8aa6a) )
	ROM_LOAD32_BYTE( "rr3_prglmb.2d", 0x00002, 0x80000, CRC(1e9ef0a9) SHA1(a4577bcdf13673568793d8a324945fca30b10f43) )
	ROM_LOAD32_BYTE( "rr3_prgumb.8d", 0x00001, 0x80000, CRC(e160f63f) SHA1(9b4b7a13eb4bc19fcb53daedb87e4945c20a1b8e) )
	ROM_LOAD32_BYTE( "rr3_prguub.6d", 0x00000, 0x80000, CRC(f07c78c0) SHA1(dbed76d868b761711faf5b6e11f2c9affb91db5d) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rr1data.6r", 0, 0x080000, CRC(18f5f748) SHA1(e0d149a66de36156edd9b55f604c9a9801aaefa8) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rr1cg0.bin", 0x800000, 0x200000, CRC(b557a795) SHA1(f345486ffbe797246ad80a55d3c4a332ed6e2888) )
	ROM_LOAD( "rr1cg1.bin", 0xa00000, 0x200000, CRC(0fa212d9) SHA1(a1311de0a504e2d399044fa8ac32ec6c56ec965f) )
	ROM_LOAD( "rr1cg2.bin", 0xc00000, 0x200000, CRC(18e2d2bd) SHA1(69c2ea62eeb255f27d3c69373f6716b0a34683cc) )
	ROM_LOAD( "rr1cg3.bin", 0xe00000, 0x200000, CRC(9564488b) SHA1(6b27d1aea75d6be747c62e165cfa49ecc5d9e767) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rr1ccrl.bin", 0x000000, 0x200000, CRC(6092d181) SHA1(52c0e3ac20aa23059a87d1a985d24ae641577310) )
	ROM_LOAD( "rr1ccrh.bin", 0x200000, 0x080000, CRC(dd332fd5) SHA1(a7d9c1d6b5a8e3a937b525c1363880e404dcd147) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rr1potl0.5b", 0x000000, 0x080000, CRC(3ac193e3) SHA1(ff213766f15e34dc1b25187b57d94e17930090a3) )
	ROM_LOAD( "rr1potl1.4b", 0x080000, 0x080000, CRC(ac3ffba5) SHA1(4eb4dda5faeff237e0d35725b56d309948fba900) )
	ROM_LOAD( "rr1potm0.5c", 0x100000, 0x080000, CRC(42a3fa08) SHA1(15db0ae7ccf7f5a77b9dd9a9d82a488b67f8aaff) )
	ROM_LOAD( "rr1potm1.4c", 0x180000, 0x080000, CRC(1bc1ea7b) SHA1(52c21eef4989c45acc5fa4deda2d0b63214731c8) )
	ROM_LOAD( "rr1potu0.5d", 0x200000, 0x080000, CRC(5e367f72) SHA1(5887f011379dce865fef238b402678a3d2033de9) )
	ROM_LOAD( "rr1potu1.4d", 0x280000, 0x080000, CRC(31d92475) SHA1(51d3c0baa223e1bc16ea2950f2e085597528f870) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rr1wav0.10r", 0x000000, 0x100000, CRC(a8e85bde) SHA1(b56677e9f6c98f7b600043f5dcfef3a482ca7455) )
	ROM_LOAD( "rr1wav1.10p", 0x200000, 0x100000, CRC(35f47c8e) SHA1(7c3f9e942f532af8008fbead2a96fee6084bcde6) )
	ROM_LOAD( "rr1wav2.10n", 0x100000, 0x100000, CRC(3244cb59) SHA1(b3283b30cfafbfdcbc6d482ecc4ed6a47a527ca4) )
	ROM_LOAD( "rr1wav3.10l", 0x300000, 0x100000, CRC(c4cda1a7) SHA1(60bc96880ec79efdff3cc70c09e848692a40bea4) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( ridgeracc )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rr3_prgll-3s.4d", 0x000003, 0x080000, CRC(2c3d8cb7) SHA1(46a7b62938fe3edde5c52ce3fdfe447000cd6af0) )
	ROM_LOAD32_BYTE( "rr3_prglm-3s.2d", 0x000002, 0x080000, CRC(b15343f2) SHA1(3056eb5a3036a74b2ac641a4c3221986c0be1e27) )
	ROM_LOAD32_BYTE( "rr3_prgum-3s.8d", 0x000001, 0x080000, CRC(8fda06ac) SHA1(7e9adba198eb0941100cda64ecedac504f6ac696) )
	ROM_LOAD32_BYTE( "rr3_prguu-3s.6d", 0x000000, 0x080000, CRC(868398df) SHA1(422e0f9884904b0df93fcacd1468b8da0458eb8e) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rr1data.6r", 0, 0x080000, CRC(18f5f748) SHA1(e0d149a66de36156edd9b55f604c9a9801aaefa8) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rr1cg0.bin", 0x800000, 0x200000, CRC(b557a795) SHA1(f345486ffbe797246ad80a55d3c4a332ed6e2888) )
	ROM_LOAD( "rr1cg1.bin", 0xa00000, 0x200000, CRC(0fa212d9) SHA1(a1311de0a504e2d399044fa8ac32ec6c56ec965f) )
	ROM_LOAD( "rr1cg2.bin", 0xc00000, 0x200000, CRC(18e2d2bd) SHA1(69c2ea62eeb255f27d3c69373f6716b0a34683cc) )
	ROM_LOAD( "rr1cg3.bin", 0xe00000, 0x200000, CRC(9564488b) SHA1(6b27d1aea75d6be747c62e165cfa49ecc5d9e767) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rr1ccrl.bin", 0x000000, 0x200000, CRC(6092d181) SHA1(52c0e3ac20aa23059a87d1a985d24ae641577310) )
	ROM_LOAD( "rr1ccrh.bin", 0x200000, 0x080000, CRC(dd332fd5) SHA1(a7d9c1d6b5a8e3a937b525c1363880e404dcd147) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rr1potl0.5b", 0x000000, 0x080000, CRC(3ac193e3) SHA1(ff213766f15e34dc1b25187b57d94e17930090a3) )
	ROM_LOAD( "rr1potl1.4b", 0x080000, 0x080000, CRC(ac3ffba5) SHA1(4eb4dda5faeff237e0d35725b56d309948fba900) )
	ROM_LOAD( "rr1potm0.5c", 0x100000, 0x080000, CRC(42a3fa08) SHA1(15db0ae7ccf7f5a77b9dd9a9d82a488b67f8aaff) )
	ROM_LOAD( "rr1potm1.4c", 0x180000, 0x080000, CRC(1bc1ea7b) SHA1(52c21eef4989c45acc5fa4deda2d0b63214731c8) )
	ROM_LOAD( "rr1potu0.5d", 0x200000, 0x080000, CRC(5e367f72) SHA1(5887f011379dce865fef238b402678a3d2033de9) )
	ROM_LOAD( "rr1potu1.4d", 0x280000, 0x080000, CRC(31d92475) SHA1(51d3c0baa223e1bc16ea2950f2e085597528f870) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rr1wav0.10r", 0x000000, 0x100000, CRC(a8e85bde) SHA1(b56677e9f6c98f7b600043f5dcfef3a482ca7455) )
	ROM_LOAD( "rr1wav1.10p", 0x200000, 0x100000, CRC(35f47c8e) SHA1(7c3f9e942f532af8008fbead2a96fee6084bcde6) )
	ROM_LOAD( "rr1wav2.10n", 0x100000, 0x100000, CRC(3244cb59) SHA1(b3283b30cfafbfdcbc6d482ecc4ed6a47a527ca4) )
	ROM_LOAD( "rr1wav3.10l", 0x300000, 0x100000, CRC(c4cda1a7) SHA1(60bc96880ec79efdff3cc70c09e848692a40bea4) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( ridgeracj )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rr1_prgll.4d", 0x00003, 0x80000, CRC(4bb7fc86) SHA1(8291375b8ec4d37e0d9e3bf38da2d5907b0f31bd) )
	ROM_LOAD32_BYTE( "rr1_prglm.2d", 0x00002, 0x80000, CRC(68e13830) SHA1(ddc447c7afbb5c4238969d7e78bfe9cf8fac6061) )
	ROM_LOAD32_BYTE( "rr1_prgum.8d", 0x00001, 0x80000, CRC(705ef78a) SHA1(881903413e66d6fd83d46eb18c4e1230531832ae) )
	ROM_LOAD32_BYTE( "rr1_prguu.6d", 0x00000, 0x80000, CRC(c1371f96) SHA1(a78e0bf6c147c034487a85efa0a8470f4e8f4bf0) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rr1data.6r", 0, 0x080000, CRC(18f5f748) SHA1(e0d149a66de36156edd9b55f604c9a9801aaefa8) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rr1cg0.bin", 0x800000, 0x200000, CRC(b557a795) SHA1(f345486ffbe797246ad80a55d3c4a332ed6e2888) )
	ROM_LOAD( "rr1cg1.bin", 0xa00000, 0x200000, CRC(0fa212d9) SHA1(a1311de0a504e2d399044fa8ac32ec6c56ec965f) )
	ROM_LOAD( "rr1cg2.bin", 0xc00000, 0x200000, CRC(18e2d2bd) SHA1(69c2ea62eeb255f27d3c69373f6716b0a34683cc) )
	ROM_LOAD( "rr1cg3.bin", 0xe00000, 0x200000, CRC(9564488b) SHA1(6b27d1aea75d6be747c62e165cfa49ecc5d9e767) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rr1ccrl.bin", 0x000000, 0x200000, CRC(6092d181) SHA1(52c0e3ac20aa23059a87d1a985d24ae641577310) )
	ROM_LOAD( "rr1ccrh.bin", 0x200000, 0x080000, CRC(dd332fd5) SHA1(a7d9c1d6b5a8e3a937b525c1363880e404dcd147) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rr1potl0.5b", 0x000000, 0x080000, CRC(3ac193e3) SHA1(ff213766f15e34dc1b25187b57d94e17930090a3) )
	ROM_LOAD( "rr1potl1.4b", 0x080000, 0x080000, CRC(ac3ffba5) SHA1(4eb4dda5faeff237e0d35725b56d309948fba900) )
	ROM_LOAD( "rr1potm0.5c", 0x100000, 0x080000, CRC(42a3fa08) SHA1(15db0ae7ccf7f5a77b9dd9a9d82a488b67f8aaff) )
	ROM_LOAD( "rr1potm1.4c", 0x180000, 0x080000, CRC(1bc1ea7b) SHA1(52c21eef4989c45acc5fa4deda2d0b63214731c8) )
	ROM_LOAD( "rr1potu0.5d", 0x200000, 0x080000, CRC(5e367f72) SHA1(5887f011379dce865fef238b402678a3d2033de9) )
	ROM_LOAD( "rr1potu1.4d", 0x280000, 0x080000, CRC(31d92475) SHA1(51d3c0baa223e1bc16ea2950f2e085597528f870) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rr1wav0.10r", 0x000000, 0x100000, CRC(a8e85bde) SHA1(b56677e9f6c98f7b600043f5dcfef3a482ca7455) )
	ROM_LOAD( "rr1wav1.10p", 0x200000, 0x100000, CRC(35f47c8e) SHA1(7c3f9e942f532af8008fbead2a96fee6084bcde6) )
	ROM_LOAD( "rr1wav2.10n", 0x100000, 0x100000, CRC(3244cb59) SHA1(b3283b30cfafbfdcbc6d482ecc4ed6a47a527ca4) )
	ROM_LOAD( "rr1wav3.10l", 0x300000, 0x100000, CRC(c4cda1a7) SHA1(60bc96880ec79efdff3cc70c09e848692a40bea4) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( ridgera28 )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rrs_8_prgll.4d", 0x00003, 0x80000, CRC(c8d2bc61) SHA1(9db734fc45f47e6742ef2a8ba2558efb881df94a) )
	ROM_LOAD32_BYTE( "rrs_8_prglm.2d", 0x00002, 0x80000, CRC(bb1d23c7) SHA1(feaa1387b7003794c192edef4478636d8b125fef) )
	ROM_LOAD32_BYTE( "rrs_8_prgum.8d", 0x00001, 0x80000, CRC(720c854d) SHA1(9c8e2868dd7ef425fdc6f701968b9713bb12b56c) )
	ROM_LOAD32_BYTE( "rrs_8_prguu.6d", 0x00000, 0x80000, CRC(fb4b7e8f) SHA1(1919803dbe35a88bbb6b608d0c3ab8daea31f580) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rrs_8_data.6r", 0, 0x080000, CRC(8bf75a96) SHA1(987d24f2195eb5a84c10c68584404556ef1dfe88) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rrs1cg0.1a", 0x800000, 0x200000, CRC(714c0091) SHA1(df29512bd6e64827660c40304051366d2c4d7977) )
	ROM_LOAD( "rrs1cg1.2a", 0xa00000, 0x200000, CRC(836545c1) SHA1(05e3346463d8d42b5d33216207e855033a65510d) )
	ROM_LOAD( "rrs1cg2.3a", 0xc00000, 0x200000, CRC(00e9799d) SHA1(280184451138420f64080efe13e5e2795f7b61d4) )
	ROM_LOAD( "rrs1cg3.5a", 0xe00000, 0x200000, CRC(3858983f) SHA1(feda270b71f1310ecf4c17823bc8827ca2951b40) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rrs1ccrl.5a", 0x000000, 0x200000, CRC(304a8b57) SHA1(f4f3e7c194697d754375f36a0e41d0941fa5d225) )
	ROM_LOAD( "rrs1ccrh.5c", 0x200000, 0x080000, CRC(bd3c86ab) SHA1(cd3a8774843c5864e651fa8989c80e2d975a13e8) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rrs1pol0.5b", 0x000000, 0x080000, CRC(9376c384) SHA1(cde0e36db1beab1523607098a760d81fac2ea90e) )
	ROM_LOAD( "rrs1pol1.4b", 0x080000, 0x080000, CRC(094fa832) SHA1(cc59442540b1cdef068c4408b6e048c11042beb8) )
	ROM_LOAD( "rrs1pom0.5c", 0x100000, 0x080000, CRC(b47a7f8b) SHA1(0fa0456ad8b4864a7071b5b5ba1a78877c1ac0f0) )
	ROM_LOAD( "rrs1pom1.4c", 0x180000, 0x080000, CRC(27260361) SHA1(8775cc779eb8b6a0d79fa84d606c970ec2d6ea8d) )
	ROM_LOAD( "rrs1pou0.5d", 0x200000, 0x080000, CRC(74d6ec84) SHA1(63f5beee51443c98100330ec04291f71e10716c4) )
	ROM_LOAD( "rrs1pou1.4d", 0x280000, 0x080000, CRC(f527caaa) SHA1(f92bdd15323239d593ddac92a11d23a27e6635ed) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rrs1wav0.10r", 0x000000, 0x100000, CRC(99d11a2d) SHA1(1f3db98a99be0f07c03b0a7817561459a58f310e) )
	ROM_LOAD( "rrs1wav1.10p", 0x200000, 0x100000, CRC(ad28444a) SHA1(c31bbf3cae5015e5494fe4988b9b01d822224c69) )
	ROM_LOAD( "rrs1wav2.10n", 0x100000, 0x100000, CRC(6f0d4619) SHA1(cd3d57f2ea21497f388ffa29ec7d2665647a01c0) )
	ROM_LOAD( "rrs1wav3.10l", 0x300000, 0x100000, CRC(106e761f) SHA1(97f47b857bdcbc79b0aface53dd385e67fcc9108) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( cybrcomm )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "cy1prgll.4d", 0x00003, 0x80000, CRC(b3eab156) SHA1(2a5c4e0360c3b9500687a4d70f7110a0c30da2a5) )
	ROM_LOAD32_BYTE( "cy1prglm.2d", 0x00002, 0x80000, CRC(884a5b0e) SHA1(0e27ae366b8a2695fe112b4740c8c9013bb97e26) )
	ROM_LOAD32_BYTE( "cy1prgum.8d", 0x00001, 0x80000, CRC(c9c4a921) SHA1(76a52461165a8bd8d984a34063fbeb4cb73624af) )
	ROM_LOAD32_BYTE( "cy1prguu.6d", 0x00000, 0x80000, CRC(5f22975b) SHA1(a1a5cb66358d64a3c564b912f2eeafa182786b1e) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "cy1data.6r", 0x00000, 0x20000, CRC(10d0005b) SHA1(10508eeaf74d24a611b44cd3bb12417ceb78904f) )
	ROM_RELOAD( 0x20000, 0x20000 )
	ROM_RELOAD( 0x40000, 0x20000 )
	ROM_RELOAD( 0x60000, 0x20000 )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "cyc1cg0.1a", 0x800000, 0x200000, CRC(e839b9bd) SHA1(fee43d37dcca7f1fb952a6bfb886b7ee30b7d75c) )
	ROM_LOAD( "cyc1cg1.2a", 0xa00000, 0x200000, CRC(7d13993f) SHA1(96ac82bcc63afe395bae73f005eb66dad7742d48) )
	ROM_LOAD( "cyc1cg2.3a", 0xc00000, 0x200000, CRC(7c464566) SHA1(69817ac3a7c6e43b960e8a904962b58b23417163) )
	ROM_LOAD( "cyc1cg3.5a", 0xe00000, 0x200000, CRC(2222e16f) SHA1(562bcd4d43b1543303d8fd66d9f0d9a8e3702492) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "cyc1ccrl.1c", 0x000000, 0x100000, CRC(1a0dc5f0) SHA1(bf0093d9cbdcb45a82705e966c48a1f408fa344e) )
	ROM_LOAD( "cyc1ccrh.2c", 0x200000, 0x080000, CRC(8c4090b8) SHA1(456d548a48833e840c5d39d47b2dcca03f8d6321) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "cyc1ptl0.5b", 0x000000, 0x080000, CRC(d91de03d) SHA1(05819d285f6111867c41337bda9c4b9ad5394b6b) )
	ROM_LOAD( "cyc1ptl1.4b", 0x080000, 0x080000, CRC(e5b98021) SHA1(7416cbf74da969f822e0363ced7a25b967277e28) )
	ROM_LOAD( "cyc1ptl2.3b", 0x100000, 0x080000, CRC(7ba786c6) SHA1(1a5319dec495453bab9d70ae773a807f0036b355) )
	ROM_LOAD( "cyc1ptm0.5c", 0x180000, 0x080000, CRC(d454b5c6) SHA1(95ae6f0455e9fd7dff066e74cd4343c94d1bc212) )
	ROM_LOAD( "cyc1ptm1.4c", 0x200000, 0x080000, CRC(74fdf8cc) SHA1(f2627f400e247b6d4c4157eaf0ec69d57212e566) )
	ROM_LOAD( "cyc1ptm2.3c", 0x280000, 0x080000, CRC(b9c99a45) SHA1(c86cf594b416776eaf9a32c3cb9d34acc79777e9) )
	ROM_LOAD( "cyc1ptu0.5d", 0x300000, 0x080000, CRC(4d40897f) SHA1(ffe2a0ab66443553c83512f9a1be94b2e385cf2f) )
	ROM_LOAD( "cyc1ptu1.4d", 0x380000, 0x080000, CRC(3bdaeeeb) SHA1(826f97e2165af8569cfec03874b16030a9486559) )
	ROM_LOAD( "cyc1ptu2.3d", 0x400000, 0x080000, CRC(a0e73674) SHA1(1e22142a564e664031c12b250664fc82e3b3d43b) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "cy1wav0.10r", 0x000000, 0x100000, CRC(c6f366a2) SHA1(795dbee09df159d3501c748fb3de16cca81742d6) )
	ROM_LOAD( "cy1wav1.10p", 0x200000, 0x100000, CRC(f30b5e37) SHA1(9f5a94d82741ef9688c6e415ebb9009c906737c9) )
	ROM_LOAD( "cy1wav2.10n", 0x100000, 0x100000, CRC(b98c1ca6) SHA1(4b66aa05f82be5ef3315acc30031872698ff4391) )
	ROM_LOAD( "cy1wav3.10l", 0x300000, 0x100000, CRC(43dbac19) SHA1(83fd4ae4e7ec264fc217ed18caf59bf438af0c3d) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "cy1eeprm.9e", 0x0000, 0x2000, CRC(8432c066) SHA1(99d4bfda3f8aec288dbeaf291bce85fe9009a1de) )
ROM_END

ROM_START( raverace )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rv2_prgllb.4d", 0x00003, 0x80000, CRC(3017cd1e) SHA1(ccd648b4a5dfc74fd141815af2969f423311042f) )
	ROM_LOAD32_BYTE( "rv2_prglmb.2d", 0x00002, 0x80000, CRC(894be0c3) SHA1(4dba93dc3ca1cf502c5f54018b64ad79bb2a632b) )
	ROM_LOAD32_BYTE( "rv2_prgumb.8d", 0x00001, 0x80000, CRC(6414a800) SHA1(c278ff644909d12a43ba6fc2bf8d2092e469c3e6) )
	ROM_LOAD32_BYTE( "rv2_prguub.6d", 0x00000, 0x80000, CRC(a9f18714) SHA1(8e7b17749d151f92020f68d1ac06003cf1f5c573) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rv1data.6r", 0, 0x080000, CRC(d358ec20) SHA1(140c513349240417bb546dd2d151f3666b818e91) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rv1cg0.1a", 0x000000, 0x200000, CRC(c518f06b) SHA1(4c01d453244192dd13087bdc72a7f7be80b47cbc) )
	ROM_LOAD( "rv1cg1.1c", 0x200000, 0x200000, CRC(6628f792) SHA1(7a5405c5fcb2f3f001ae17df393c31e61a834f2b) )
	ROM_LOAD( "rv1cg2.1d", 0x400000, 0x200000, CRC(0b707cc5) SHA1(38e1a554b278062edc369565353497ac4b016f78) )
	ROM_LOAD( "rv1cg3.1e", 0x600000, 0x200000, CRC(39b62921) SHA1(873287d81338baf10dd85214d82f6c38bfdf199e) )
	ROM_LOAD( "rv1cg4.1f", 0x800000, 0x200000, CRC(a9791ea2) SHA1(245b2ebbadd1fbca90dc241f88e9f6f341b2a01a) )
	ROM_LOAD( "rv1cg5.1j", 0xa00000, 0x200000, CRC(b2c79ec1) SHA1(6f669996863bdf1fe09b0c1a2a876625029d3d43) )
	ROM_LOAD( "rv1cg6.1k", 0xc00000, 0x200000, CRC(8cddedc2) SHA1(e3993f5505bc7e61bec7be5b48c873572e1220f7) )
	ROM_LOAD( "rv1cg7.1n", 0xe00000, 0x200000, CRC(b39147ca) SHA1(50ca6691fc809c95e6999dd52e39f2b8c2d22f3b) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rv1ccrl.5a", 0x000000, 0x200000, CRC(bc634f72) SHA1(b5c504ed92bca7682614fc4c51f38cff607e6f2a) )
	ROM_LOAD( "rv1ccrh.5c", 0x200000, 0x080000, CRC(a741b262) SHA1(363076220a0eacc67befda05f8253963e8ffbcaa) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rv1potl0.5b", 0x000000, 0x080000, CRC(de2ce519) SHA1(2fe0dd000571f76d1a4df6a439d40119125170ef) )
	ROM_LOAD( "rv1potl1.4b", 0x080000, 0x080000, CRC(2215cb5a) SHA1(d48ee692ab3dbcffdc49d22f6f232ca9390da766) )
	ROM_LOAD( "rv1potl2.3b", 0x100000, 0x080000, CRC(ddb15bf7) SHA1(4c54ec98e0cba10841d43a4ce593cdacfd7f90f8) )
	ROM_LOAD( "rv1potl3.2b", 0x180000, 0x080000, CRC(fa9361ca) SHA1(35a5c2712bca9c62400b724754de3a931ad21561) )
	ROM_LOAD( "rv1potm0.5c", 0x200000, 0x080000, CRC(3c024f3a) SHA1(711f0442823797b2d410352796a5cca66af98dce) )
	ROM_LOAD( "rv1potm1.4c", 0x280000, 0x080000, CRC(b1a32a68) SHA1(e24abb3a7e35d098abae5420bf8ef5c975718987) )
	ROM_LOAD( "rv1potm2.3c", 0x300000, 0x080000, CRC(a414fe15) SHA1(eb27cdca045ab2ab27dec179043328847fb65e11) )
	ROM_LOAD( "rv1potm3.2c", 0x380000, 0x080000, CRC(2953bbb4) SHA1(aca1acd87f7130d2522d0c6f8e60beeb7ab7495a) )
	ROM_LOAD( "rv1potu0.5d", 0x400000, 0x080000, CRC(b9eaf3cc) SHA1(3b2a9041f1fa90706ecf7d4fbff918516f891a07) )
	ROM_LOAD( "rv1potu1.4d", 0x480000, 0x080000, CRC(a5c55258) SHA1(826d4dde761aec7d848456f7bc4ba6268fe99605) )
	ROM_LOAD( "rv1potu2.3d", 0x500000, 0x080000, CRC(c18fcb74) SHA1(a4009ae2b014dc89aed4741fd97f84350117c2f4) )
	ROM_LOAD( "rv1potu3.2d", 0x580000, 0x080000, CRC(79735aaa) SHA1(1cf14274669b916a7641f7a16785da1b72347485) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rv1wav0.10r", 0x000000, 0x100000, CRC(5aef8143) SHA1(a75d31298e3ff9b290f238976a11e8b85cfb72d3) )
	ROM_LOAD( "rv1wav1.10p", 0x200000, 0x100000, CRC(9ed9e6b3) SHA1(dd1da2b08d1b6aa0912daacc77744c9799aabb78) )
	ROM_LOAD( "rv1wav2.10n", 0x100000, 0x100000, CRC(5af9dc83) SHA1(9aeb7f8217b806a6f3ed93056513af9fbcb6b372) )
	ROM_LOAD( "rv1wav3.10l", 0x300000, 0x100000, CRC(ffb9ad75) SHA1(a9a61a597bd3bbe9732f92747d82264fe4d9af48) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "rv1eeprm.9e", 0x0000, 0x2000, CRC(e00dd412) SHA1(f594b31ace5e5e980e904faa8b83a450cc95db17) )
ROM_END

ROM_START( raveracej )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rv1_prgllb.4d", 0x00003, 0x80000, CRC(71da3eea) SHA1(8a641bb23e0ad89cae5ee1570f8a3627b2434d20) )
	ROM_LOAD32_BYTE( "rv1_prglmb.2d", 0x00002, 0x80000, CRC(6ab7e9ce) SHA1(0c6376ca5a63409aeea344bbc201af6c47afe9ab) )
	ROM_LOAD32_BYTE( "rv1_prgumb.8d", 0x00001, 0x80000, CRC(375fabcf) SHA1(448e3db3e3fab8c7c27e214ab5a5fa84e5f84366) )
	ROM_LOAD32_BYTE( "rv1_prguub.6d", 0x00000, 0x80000, CRC(92f834d6) SHA1(028368790f0293fcfea5c7b12f7f315e27a62f77) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rv1data.6r", 0, 0x080000, CRC(d358ec20) SHA1(140c513349240417bb546dd2d151f3666b818e91) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rv1cg0.1a", 0x000000, 0x200000, CRC(c518f06b) SHA1(4c01d453244192dd13087bdc72a7f7be80b47cbc) )
	ROM_LOAD( "rv1cg1.1c", 0x200000, 0x200000, CRC(6628f792) SHA1(7a5405c5fcb2f3f001ae17df393c31e61a834f2b) )
	ROM_LOAD( "rv1cg2.1d", 0x400000, 0x200000, CRC(0b707cc5) SHA1(38e1a554b278062edc369565353497ac4b016f78) )
	ROM_LOAD( "rv1cg3.1e", 0x600000, 0x200000, CRC(39b62921) SHA1(873287d81338baf10dd85214d82f6c38bfdf199e) )
	ROM_LOAD( "rv1cg4.1f", 0x800000, 0x200000, CRC(a9791ea2) SHA1(245b2ebbadd1fbca90dc241f88e9f6f341b2a01a) )
	ROM_LOAD( "rv1cg5.1j", 0xa00000, 0x200000, CRC(b2c79ec1) SHA1(6f669996863bdf1fe09b0c1a2a876625029d3d43) )
	ROM_LOAD( "rv1cg6.1k", 0xc00000, 0x200000, CRC(8cddedc2) SHA1(e3993f5505bc7e61bec7be5b48c873572e1220f7) )
	ROM_LOAD( "rv1cg7.1n", 0xe00000, 0x200000, CRC(b39147ca) SHA1(50ca6691fc809c95e6999dd52e39f2b8c2d22f3b) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rv1ccrl.5a", 0x000000, 0x200000, CRC(bc634f72) SHA1(b5c504ed92bca7682614fc4c51f38cff607e6f2a) )
	ROM_LOAD( "rv1ccrh.5c", 0x200000, 0x080000, CRC(a741b262) SHA1(363076220a0eacc67befda05f8253963e8ffbcaa) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rv1potl0.5b", 0x000000, 0x080000, CRC(de2ce519) SHA1(2fe0dd000571f76d1a4df6a439d40119125170ef) )
	ROM_LOAD( "rv1potl1.4b", 0x080000, 0x080000, CRC(2215cb5a) SHA1(d48ee692ab3dbcffdc49d22f6f232ca9390da766) )
	ROM_LOAD( "rv1potl2.3b", 0x100000, 0x080000, CRC(ddb15bf7) SHA1(4c54ec98e0cba10841d43a4ce593cdacfd7f90f8) )
	ROM_LOAD( "rv1potl3.2b", 0x180000, 0x080000, CRC(fa9361ca) SHA1(35a5c2712bca9c62400b724754de3a931ad21561) )
	ROM_LOAD( "rv1potm0.5c", 0x200000, 0x080000, CRC(3c024f3a) SHA1(711f0442823797b2d410352796a5cca66af98dce) )
	ROM_LOAD( "rv1potm1.4c", 0x280000, 0x080000, CRC(b1a32a68) SHA1(e24abb3a7e35d098abae5420bf8ef5c975718987) )
	ROM_LOAD( "rv1potm2.3c", 0x300000, 0x080000, CRC(a414fe15) SHA1(eb27cdca045ab2ab27dec179043328847fb65e11) )
	ROM_LOAD( "rv1potm3.2c", 0x380000, 0x080000, CRC(2953bbb4) SHA1(aca1acd87f7130d2522d0c6f8e60beeb7ab7495a) )
	ROM_LOAD( "rv1potu0.5d", 0x400000, 0x080000, CRC(b9eaf3cc) SHA1(3b2a9041f1fa90706ecf7d4fbff918516f891a07) )
	ROM_LOAD( "rv1potu1.4d", 0x480000, 0x080000, CRC(a5c55258) SHA1(826d4dde761aec7d848456f7bc4ba6268fe99605) )
	ROM_LOAD( "rv1potu2.3d", 0x500000, 0x080000, CRC(c18fcb74) SHA1(a4009ae2b014dc89aed4741fd97f84350117c2f4) )
	ROM_LOAD( "rv1potu3.2d", 0x580000, 0x080000, CRC(79735aaa) SHA1(1cf14274669b916a7641f7a16785da1b72347485) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rv1wav0.10r", 0x000000, 0x100000, CRC(5aef8143) SHA1(a75d31298e3ff9b290f238976a11e8b85cfb72d3) )
	ROM_LOAD( "rv1wav1.10p", 0x200000, 0x100000, CRC(9ed9e6b3) SHA1(dd1da2b08d1b6aa0912daacc77744c9799aabb78) )
	ROM_LOAD( "rv1wav2.10n", 0x100000, 0x100000, CRC(5af9dc83) SHA1(9aeb7f8217b806a6f3ed93056513af9fbcb6b372) )
	ROM_LOAD( "rv1wav3.10l", 0x300000, 0x100000, CRC(ffb9ad75) SHA1(a9a61a597bd3bbe9732f92747d82264fe4d9af48) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "rv1eeprm.9e", 0x0000, 0x2000, CRC(e00dd412) SHA1(f594b31ace5e5e980e904faa8b83a450cc95db17) )
ROM_END

ROM_START( raveraceja )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "rv1_prgll.4d", 0x00003, 0x80000, CRC(5dfce6cd) SHA1(1aeeca1e507ae4cbe3d39ca5efd1cc4fe1ab03a8) )
	ROM_LOAD32_BYTE( "rv1_prglm.2d", 0x00002, 0x80000, CRC(0d4d9f74) SHA1(f886b0629cbf5a369af1f44e53c6fd3f51b3fbc9) )
	ROM_LOAD32_BYTE( "rv1_prgum.8d", 0x00001, 0x80000, CRC(28e503e3) SHA1(a3071461f840f28c65c660de215c73f812f356b3) )
	ROM_LOAD32_BYTE( "rv1_prguu.6d", 0x00000, 0x80000, CRC(c47d9ff4) SHA1(4d7c4ac4151a3b306e7277937add8eee26e561a6) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "rv1data.6r", 0, 0x080000, CRC(d358ec20) SHA1(140c513349240417bb546dd2d151f3666b818e91) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "rv1cg0.1a", 0x000000, 0x200000, CRC(c518f06b) SHA1(4c01d453244192dd13087bdc72a7f7be80b47cbc) )
	ROM_LOAD( "rv1cg1.1c", 0x200000, 0x200000, CRC(6628f792) SHA1(7a5405c5fcb2f3f001ae17df393c31e61a834f2b) )
	ROM_LOAD( "rv1cg2.1d", 0x400000, 0x200000, CRC(0b707cc5) SHA1(38e1a554b278062edc369565353497ac4b016f78) )
	ROM_LOAD( "rv1cg3.1e", 0x600000, 0x200000, CRC(39b62921) SHA1(873287d81338baf10dd85214d82f6c38bfdf199e) )
	ROM_LOAD( "rv1cg4.1f", 0x800000, 0x200000, CRC(a9791ea2) SHA1(245b2ebbadd1fbca90dc241f88e9f6f341b2a01a) )
	ROM_LOAD( "rv1cg5.1j", 0xa00000, 0x200000, CRC(b2c79ec1) SHA1(6f669996863bdf1fe09b0c1a2a876625029d3d43) )
	ROM_LOAD( "rv1cg6.1k", 0xc00000, 0x200000, CRC(8cddedc2) SHA1(e3993f5505bc7e61bec7be5b48c873572e1220f7) )
	ROM_LOAD( "rv1cg7.1n", 0xe00000, 0x200000, CRC(b39147ca) SHA1(50ca6691fc809c95e6999dd52e39f2b8c2d22f3b) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "rv1ccrl.5a", 0x000000, 0x200000, CRC(bc634f72) SHA1(b5c504ed92bca7682614fc4c51f38cff607e6f2a) )
	ROM_LOAD( "rv1ccrh.5c", 0x200000, 0x080000, CRC(a741b262) SHA1(363076220a0eacc67befda05f8253963e8ffbcaa) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "rv1potl0.5b", 0x000000, 0x080000, CRC(de2ce519) SHA1(2fe0dd000571f76d1a4df6a439d40119125170ef) )
	ROM_LOAD( "rv1potl1.4b", 0x080000, 0x080000, CRC(2215cb5a) SHA1(d48ee692ab3dbcffdc49d22f6f232ca9390da766) )
	ROM_LOAD( "rv1potl2.3b", 0x100000, 0x080000, CRC(ddb15bf7) SHA1(4c54ec98e0cba10841d43a4ce593cdacfd7f90f8) )
	ROM_LOAD( "rv1potl3.2b", 0x180000, 0x080000, CRC(fa9361ca) SHA1(35a5c2712bca9c62400b724754de3a931ad21561) )
	ROM_LOAD( "rv1potm0.5c", 0x200000, 0x080000, CRC(3c024f3a) SHA1(711f0442823797b2d410352796a5cca66af98dce) )
	ROM_LOAD( "rv1potm1.4c", 0x280000, 0x080000, CRC(b1a32a68) SHA1(e24abb3a7e35d098abae5420bf8ef5c975718987) )
	ROM_LOAD( "rv1potm2.3c", 0x300000, 0x080000, CRC(a414fe15) SHA1(eb27cdca045ab2ab27dec179043328847fb65e11) )
	ROM_LOAD( "rv1potm3.2c", 0x380000, 0x080000, CRC(2953bbb4) SHA1(aca1acd87f7130d2522d0c6f8e60beeb7ab7495a) )
	ROM_LOAD( "rv1potu0.5d", 0x400000, 0x080000, CRC(b9eaf3cc) SHA1(3b2a9041f1fa90706ecf7d4fbff918516f891a07) )
	ROM_LOAD( "rv1potu1.4d", 0x480000, 0x080000, CRC(a5c55258) SHA1(826d4dde761aec7d848456f7bc4ba6268fe99605) )
	ROM_LOAD( "rv1potu2.3d", 0x500000, 0x080000, CRC(c18fcb74) SHA1(a4009ae2b014dc89aed4741fd97f84350117c2f4) )
	ROM_LOAD( "rv1potu3.2d", 0x580000, 0x080000, CRC(79735aaa) SHA1(1cf14274669b916a7641f7a16785da1b72347485) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "rv1wav0.10r", 0x000000, 0x100000, CRC(5aef8143) SHA1(a75d31298e3ff9b290f238976a11e8b85cfb72d3) )
	ROM_LOAD( "rv1wav1.10p", 0x200000, 0x100000, CRC(9ed9e6b3) SHA1(dd1da2b08d1b6aa0912daacc77744c9799aabb78) )
	ROM_LOAD( "rv1wav2.10n", 0x100000, 0x100000, CRC(5af9dc83) SHA1(9aeb7f8217b806a6f3ed93056513af9fbcb6b372) )
	ROM_LOAD( "rv1wav3.10l", 0x300000, 0x100000, CRC(ffb9ad75) SHA1(a9a61a597bd3bbe9732f92747d82264fe4d9af48) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "rv1eeprm.9e", 0x0000, 0x2000, CRC(e00dd412) SHA1(f594b31ace5e5e980e904faa8b83a450cc95db17) )
ROM_END

ROM_START( acedrive )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ad2_prgll.4d", 0x00003, 0x80000, CRC(808c5ff8) SHA1(119c90ecb5aa099a0e5d1d7944c004beacead367) )
	ROM_LOAD32_BYTE( "ad2_prglm.2d", 0x00002, 0x80000, CRC(5f726a10) SHA1(d077312c6a387fbdf906d278c73c6a3730687f32) )
	ROM_LOAD32_BYTE( "ad2_prgum.8d", 0x00001, 0x80000, CRC(d5042d6e) SHA1(9ae93e7ea7126302831a879ba0aadcb6e5b842f5) )
	ROM_LOAD32_BYTE( "ad2_prguu.6d", 0x00000, 0x80000, CRC(86d4661d) SHA1(2a1529a51ca5466994a2d0d84c7aab13cef95a11) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ad1data.6r", 0, 0x080000, CRC(82024f74) SHA1(711ab0c4f027716aeab18e3a5d3d06fa82af8007) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ad1cg0.1a", 0x800000, 0x200000, CRC(faaa1ee2) SHA1(878f2b74587ed4d06c5110a0eb0020c49ddc5dfa) )
	ROM_LOAD( "ad1cg1.2a", 0xa00000, 0x200000, CRC(1aab1eb7) SHA1(b8f9eeafec7e0de340cf48e38fa55dd14404c867) )
	ROM_LOAD( "ad1cg2.3a", 0xc00000, 0x200000, CRC(cdcd1874) SHA1(5a7a4a0d897cca4956b0a4f178f39f618c921861) )
	ROM_LOAD( "ad1cg3.5a", 0xe00000, 0x200000, CRC(effdd2cd) SHA1(9ff156e7e38c103b8fa6f3c29776dd38482d9cf2) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ad1ccrl.1c", 0x000000, 0x200000, CRC(bc3c9b12) SHA1(088e861e5c4b37c54b7f72963113a10870bf7927) )
	ROM_LOAD( "ad1ccrh.2c", 0x200000, 0x080000, CRC(71f44526) SHA1(bb4811fc5de626380ce6a17bee73e5e47926d850) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ad1potl0.5b", 0x000000, 0x080000, CRC(dfc7e729) SHA1(5e3deef66d0a5dd2ff0584b8c8be4bf5e798e4d0) )
	ROM_LOAD( "ad1potl1.4b", 0x080000, 0x080000, CRC(5914ef8e) SHA1(f6db9c3061ceda76eef0a9538d9c048366b71124) )
	ROM_LOAD( "ad1potm0.5c", 0x100000, 0x080000, CRC(844bcd6b) SHA1(629b8dc0a7e94410c08c8874b69d9f4bc22f3e4f) )
	ROM_LOAD( "ad1potm1.4c", 0x180000, 0x080000, CRC(515cf541) SHA1(db1522813ea3e982d479cc1903d18799bf75aea9) )
	ROM_LOAD( "ad1potu0.5d", 0x200000, 0x080000, CRC(e0f44949) SHA1(ffdb64d600883974b05edaa9ed3071af355ee17f) )
	ROM_LOAD( "ad1potu1.4d", 0x280000, 0x080000, CRC(f2cd2cbb) SHA1(19fe6e3454a1e4353c7fe0a0d7a71742fea946de) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "ad1wave0.10r", 0x000000, 0x100000, CRC(c7879a72) SHA1(ae04d664858b0944583590ed0003a9420032d5ca) )
	ROM_LOAD( "ad1wave1.10p", 0x200000, 0x100000, CRC(69c1d41e) SHA1(b5cdfe7b75075c585dfd842347f8e4e692bb2781) )
	ROM_LOAD( "ad1wave2.10n", 0x100000, 0x100000, CRC(365a6831) SHA1(ddaa44a4436d6de120b64a5d130b1ee18f872e19) )
	ROM_LOAD( "ad1wave3.10l", 0x300000, 0x100000, CRC(cd8ecb0b) SHA1(7950b5a3a81f5554f57accabc7a623b8265a21a1) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( victlap )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "adv2_prgllb.4d", 0x00003, 0x80000, CRC(03ba6e42) SHA1(0e8138214463a6db59a6f7cbea3f0b6ed1b82465) )
	ROM_LOAD32_BYTE( "adv2_prglmb.2d", 0x00002, 0x80000, CRC(9f4b49c0) SHA1(e8d5fc070f3855f11ef12b41bfd889f06acccd3d) )
	ROM_LOAD32_BYTE( "adv2_prgumb.8d", 0x00001, 0x80000, CRC(ccad3e90) SHA1(16ccddf5114f1847808b0c5655aadd8ac040ff4e) )
	ROM_LOAD32_BYTE( "adv2_prguub.6d", 0x00000, 0x80000, CRC(f3fffc41) SHA1(277cc5b24a094e4adff41d6d35ffc111bc8fef27) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "adv1data.6r", 0, 0x080000, CRC(10eecdb4) SHA1(aaedeed166614e6670e765e0d7e4e9eb5f38ad10) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "adv1cg0.2a", 0x000000, 0x200000, CRC(13353848) SHA1(c6c7693e3cb086919daf9fcaf6bf602142213073) )
	ROM_LOAD( "adv1cg1.1c", 0x200000, 0x200000, CRC(1542066c) SHA1(20a053e919b7a81da2a17d31dc7482832a4d4ffe) )
	ROM_LOAD( "adv1cg2.2d", 0x400000, 0x200000, CRC(111f371c) SHA1(29d8062daae51b3c1712bd30baa9813a2b5b374d) )
	ROM_LOAD( "adv1cg3.1e", 0x600000, 0x200000, CRC(a077831f) SHA1(71bb95199b368e48bc474123ca84d19213f73137) )
	ROM_LOAD( "adv1cg4.2f", 0x800000, 0x200000, CRC(71abdacf) SHA1(64409e6aa40dd9e5a6dd1dc306860fbbf6ee7c3e) )
	ROM_LOAD( "adv1cg5.1j", 0xa00000, 0x200000, CRC(cd6cd798) SHA1(51070997a457c0ace078174569cd548ac2226b2d) )
	ROM_LOAD( "adv1cg6.2k", 0xc00000, 0x200000, CRC(94bdafba) SHA1(41e64fa99b342edd8b0ed95ae9869c23e03399e6) )
	ROM_LOAD( "adv1cg7.1n", 0xe00000, 0x200000, CRC(18823475) SHA1(a3244d665b59c352593de21f5cb8d55ddf8cee5c) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "adv1ccrl.5a", 0x000000, 0x200000, CRC(dd2b96ae) SHA1(6337ce17e617234c27ebad578ba82451649aad9c) )
	ROM_LOAD( "adv1ccrh.5c", 0x200000, 0x080000, CRC(5719844a) SHA1(a17d7bc239235e9f566931ba4fee1d6ad7964d83) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "adv1pot.l0", 0x000000, 0x080000, CRC(3b85b2a4) SHA1(84c92ed0105618d4aa5508af344b4b6cfa772567) )
	ROM_LOAD( "adv1pot.l1", 0x080000, 0x080000, CRC(601d6488) SHA1(c7932103ba6070e17deb3cc06060eed7789f938e) )
	ROM_LOAD( "adv1pot.l2", 0x100000, 0x080000, CRC(a0323a84) SHA1(deadf9a47461df7b137759d6886e676137b39fd2) )
	ROM_LOAD( "adv1pot.m0", 0x180000, 0x080000, CRC(20951aa2) SHA1(3de55bded443a5b78699cec4845470b53b22301a) )
	ROM_LOAD( "adv1pot.m1", 0x200000, 0x080000, CRC(5aed6fbf) SHA1(8cee781d8a12e00635b9a1e5cc8d82e64b17e8f1) )
	ROM_LOAD( "adv1pot.m2", 0x280000, 0x080000, CRC(00cbff92) SHA1(09a11ba064aafc921a7ca0add5898d91b773f10a) )
	ROM_LOAD( "adv1pot.u0", 0x300000, 0x080000, CRC(6b73dd2a) SHA1(e3654ab2b62e4f3314558209e37c5636f871a6c7) )
	ROM_LOAD( "adv1pot.u1", 0x380000, 0x080000, CRC(c8788f74) SHA1(606e10b05146e3db824aa608745de80584420d12) )
	ROM_LOAD( "adv1pot.u2", 0x400000, 0x080000, CRC(e67f29c5) SHA1(16222afb4f1f494711dd00ebb347c824db333bae) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "adv1wav0.10r", 0x000000, 0x100000, CRC(f07b2d9d) SHA1(fd46c23b336d5e9a748f7f8d825c19737125d2fb) )
	ROM_LOAD( "adv1wav1.10p", 0x200000, 0x100000, CRC(737f3c7a) SHA1(4737994f146c0076e7270785f41f3a85c53c7c5f) )
	ROM_LOAD( "adv1wav2.10n", 0x100000, 0x100000, CRC(c1a5ca5e) SHA1(27e6f9256d5fe5966e91d6be1e6e80900a764af1) )
	ROM_LOAD( "adv1wav3.10l", 0x300000, 0x100000, CRC(fc6b8004) SHA1(5c9e0805895931ec2b6a43376059bdbf5777222f) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( victlapa )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "adv2_prgll.4d", 0x00003, 0x80000, CRC(4dc1b0ab) SHA1(b5913388d16f824af6dbb01b5b0350d510667a87) )
	ROM_LOAD32_BYTE( "adv2_prglm.2d", 0x00002, 0x80000, CRC(7b658bef) SHA1(cf982b49fde0c1897c4c16e77f9eb2a145d8cd42) )
	ROM_LOAD32_BYTE( "adv2_prgum.8d", 0x00001, 0x80000, CRC(af67f2fb) SHA1(f391843ee0d053e33660c60e3718871142d932f2) )
	ROM_LOAD32_BYTE( "adv2_prguu.6d", 0x00000, 0x80000, CRC(b60e5d2b) SHA1(f5740615c2864c5c6433275cf4388bda5122b7a7) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "adv1data.6r", 0, 0x080000, CRC(10eecdb4) SHA1(aaedeed166614e6670e765e0d7e4e9eb5f38ad10) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "adv1cg0.2a", 0x000000, 0x200000, CRC(13353848) SHA1(c6c7693e3cb086919daf9fcaf6bf602142213073) )
	ROM_LOAD( "adv1cg1.1c", 0x200000, 0x200000, CRC(1542066c) SHA1(20a053e919b7a81da2a17d31dc7482832a4d4ffe) )
	ROM_LOAD( "adv1cg2.2d", 0x400000, 0x200000, CRC(111f371c) SHA1(29d8062daae51b3c1712bd30baa9813a2b5b374d) )
	ROM_LOAD( "adv1cg3.1e", 0x600000, 0x200000, CRC(a077831f) SHA1(71bb95199b368e48bc474123ca84d19213f73137) )
	ROM_LOAD( "adv1cg4.2f", 0x800000, 0x200000, CRC(71abdacf) SHA1(64409e6aa40dd9e5a6dd1dc306860fbbf6ee7c3e) )
	ROM_LOAD( "adv1cg5.1j", 0xa00000, 0x200000, CRC(cd6cd798) SHA1(51070997a457c0ace078174569cd548ac2226b2d) )
	ROM_LOAD( "adv1cg6.2k", 0xc00000, 0x200000, CRC(94bdafba) SHA1(41e64fa99b342edd8b0ed95ae9869c23e03399e6) )
	ROM_LOAD( "adv1cg7.1n", 0xe00000, 0x200000, CRC(18823475) SHA1(a3244d665b59c352593de21f5cb8d55ddf8cee5c) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "adv1ccrl.5a", 0x000000, 0x200000, CRC(dd2b96ae) SHA1(6337ce17e617234c27ebad578ba82451649aad9c) )
	ROM_LOAD( "adv1ccrh.5c", 0x200000, 0x080000, CRC(5719844a) SHA1(a17d7bc239235e9f566931ba4fee1d6ad7964d83) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "adv1pot.l0", 0x000000, 0x080000, CRC(3b85b2a4) SHA1(84c92ed0105618d4aa5508af344b4b6cfa772567) )
	ROM_LOAD( "adv1pot.l1", 0x080000, 0x080000, CRC(601d6488) SHA1(c7932103ba6070e17deb3cc06060eed7789f938e) )
	ROM_LOAD( "adv1pot.l2", 0x100000, 0x080000, CRC(a0323a84) SHA1(deadf9a47461df7b137759d6886e676137b39fd2) )
	ROM_LOAD( "adv1pot.m0", 0x180000, 0x080000, CRC(20951aa2) SHA1(3de55bded443a5b78699cec4845470b53b22301a) )
	ROM_LOAD( "adv1pot.m1", 0x200000, 0x080000, CRC(5aed6fbf) SHA1(8cee781d8a12e00635b9a1e5cc8d82e64b17e8f1) )
	ROM_LOAD( "adv1pot.m2", 0x280000, 0x080000, CRC(00cbff92) SHA1(09a11ba064aafc921a7ca0add5898d91b773f10a) )
	ROM_LOAD( "adv1pot.u0", 0x300000, 0x080000, CRC(6b73dd2a) SHA1(e3654ab2b62e4f3314558209e37c5636f871a6c7) )
	ROM_LOAD( "adv1pot.u1", 0x380000, 0x080000, CRC(c8788f74) SHA1(606e10b05146e3db824aa608745de80584420d12) )
	ROM_LOAD( "adv1pot.u2", 0x400000, 0x080000, CRC(e67f29c5) SHA1(16222afb4f1f494711dd00ebb347c824db333bae) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "adv1wav0.10r", 0x000000, 0x100000, CRC(f07b2d9d) SHA1(fd46c23b336d5e9a748f7f8d825c19737125d2fb) )
	ROM_LOAD( "adv1wav1.10p", 0x200000, 0x100000, CRC(737f3c7a) SHA1(4737994f146c0076e7270785f41f3a85c53c7c5f) )
	ROM_LOAD( "adv1wav2.10n", 0x100000, 0x100000, CRC(c1a5ca5e) SHA1(27e6f9256d5fe5966e91d6be1e6e80900a764af1) )
	ROM_LOAD( "adv1wav3.10l", 0x300000, 0x100000, CRC(fc6b8004) SHA1(5c9e0805895931ec2b6a43376059bdbf5777222f) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( victlapj )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "adv1_prgllc.4d", 0x00003, 0x80000, CRC(1c630018) SHA1(c4cddeb18c6b81c768e25489f1d5af0e9c0bbfa5) )
	ROM_LOAD32_BYTE( "adv1_prglmc.2d", 0x00002, 0x80000, CRC(78413604) SHA1(08245cc1f8d97a166b6c0022fc03c423d92a3653) )
	ROM_LOAD32_BYTE( "adv1_prgumc.8d", 0x00001, 0x80000, CRC(464388d9) SHA1(afad780532aff175b0a547392e80c9f01efbf9d9) )
	ROM_LOAD32_BYTE( "adv1_prguuc.6d", 0x00000, 0x80000, CRC(ad3cb5f9) SHA1(9a62043f60de4d4c82c5bec169ec975add271367) )
	ROM_REGION16_LE( 0x80000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "adv1data.6r", 0, 0x080000, CRC(10eecdb4) SHA1(aaedeed166614e6670e765e0d7e4e9eb5f38ad10) )
	C71_BIOS
	NAMCO_C74_BIOS
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "adv1cg0.2a", 0x000000, 0x200000, CRC(13353848) SHA1(c6c7693e3cb086919daf9fcaf6bf602142213073) )
	ROM_LOAD( "adv1cg1.1c", 0x200000, 0x200000, CRC(1542066c) SHA1(20a053e919b7a81da2a17d31dc7482832a4d4ffe) )
	ROM_LOAD( "adv1cg2.2d", 0x400000, 0x200000, CRC(111f371c) SHA1(29d8062daae51b3c1712bd30baa9813a2b5b374d) )
	ROM_LOAD( "adv1cg3.1e", 0x600000, 0x200000, CRC(a077831f) SHA1(71bb95199b368e48bc474123ca84d19213f73137) )
	ROM_LOAD( "adv1cg4.2f", 0x800000, 0x200000, CRC(71abdacf) SHA1(64409e6aa40dd9e5a6dd1dc306860fbbf6ee7c3e) )
	ROM_LOAD( "adv1cg5.1j", 0xa00000, 0x200000, CRC(cd6cd798) SHA1(51070997a457c0ace078174569cd548ac2226b2d) )
	ROM_LOAD( "adv1cg6.2k", 0xc00000, 0x200000, CRC(94bdafba) SHA1(41e64fa99b342edd8b0ed95ae9869c23e03399e6) )
	ROM_LOAD( "adv1cg7.1n", 0xe00000, 0x200000, CRC(18823475) SHA1(a3244d665b59c352593de21f5cb8d55ddf8cee5c) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "adv1ccrl.5a", 0x000000, 0x200000, CRC(dd2b96ae) SHA1(6337ce17e617234c27ebad578ba82451649aad9c) )
	ROM_LOAD( "adv1ccrh.5c", 0x200000, 0x080000, CRC(5719844a) SHA1(a17d7bc239235e9f566931ba4fee1d6ad7964d83) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "adv1pot.l0", 0x000000, 0x080000, CRC(3b85b2a4) SHA1(84c92ed0105618d4aa5508af344b4b6cfa772567) )
	ROM_LOAD( "adv1pot.l1", 0x080000, 0x080000, CRC(601d6488) SHA1(c7932103ba6070e17deb3cc06060eed7789f938e) )
	ROM_LOAD( "adv1pot.l2", 0x100000, 0x080000, CRC(a0323a84) SHA1(deadf9a47461df7b137759d6886e676137b39fd2) )
	ROM_LOAD( "adv1pot.m0", 0x180000, 0x080000, CRC(20951aa2) SHA1(3de55bded443a5b78699cec4845470b53b22301a) )
	ROM_LOAD( "adv1pot.m1", 0x200000, 0x080000, CRC(5aed6fbf) SHA1(8cee781d8a12e00635b9a1e5cc8d82e64b17e8f1) )
	ROM_LOAD( "adv1pot.m2", 0x280000, 0x080000, CRC(00cbff92) SHA1(09a11ba064aafc921a7ca0add5898d91b773f10a) )
	ROM_LOAD( "adv1pot.u0", 0x300000, 0x080000, CRC(6b73dd2a) SHA1(e3654ab2b62e4f3314558209e37c5636f871a6c7) )
	ROM_LOAD( "adv1pot.u1", 0x380000, 0x080000, CRC(c8788f74) SHA1(606e10b05146e3db824aa608745de80584420d12) )
	ROM_LOAD( "adv1pot.u2", 0x400000, 0x080000, CRC(e67f29c5) SHA1(16222afb4f1f494711dd00ebb347c824db333bae) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "adv1wav0.10r", 0x000000, 0x100000, CRC(f07b2d9d) SHA1(fd46c23b336d5e9a748f7f8d825c19737125d2fb) )
	ROM_LOAD( "adv1wav1.10p", 0x200000, 0x100000, CRC(737f3c7a) SHA1(4737994f146c0076e7270785f41f3a85c53c7c5f) )
	ROM_LOAD( "adv1wav2.10n", 0x100000, 0x100000, CRC(c1a5ca5e) SHA1(27e6f9256d5fe5966e91d6be1e6e80900a764af1) )
	ROM_LOAD( "adv1wav3.10l", 0x300000, 0x100000, CRC(fc6b8004) SHA1(5c9e0805895931ec2b6a43376059bdbf5777222f) )
	ROM_REGION( 0x300, REGION_PROMS, 0 ) /* gamma */
	ROM_LOAD( "rr1gam.2d", 0x0000, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.3d", 0x0100, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
	ROM_LOAD( "rr1gam.4d", 0x0200, 0x0100, CRC(b2161bce) SHA1(d2681cc0cf8e68df0d942d392b4eb4458c4bb356) )
ROM_END

ROM_START( alpinerd )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ar2verd.1", 0x00003, 0x100000, CRC(fa3380b9) SHA1(2a46988745bd2672f8082399a68ae0d0ab3d28f2) )
	ROM_LOAD32_BYTE( "ar2verd.2", 0x00002, 0x100000, CRC(76141352) SHA1(0f7230dd9cd6f1b83d499034affc7bc2c4385ab5) )
	ROM_LOAD32_BYTE( "ar2verd.3", 0x00001, 0x100000, CRC(9beffe6a) SHA1(d8efd1e3829d32bb06537d7cecb59f8df9b6d663) )
	ROM_LOAD32_BYTE( "ar2verd.4", 0x00000, 0x100000, CRC(1f3f1134) SHA1(0afa78444d1463d214f1afd7ec500af76d567489) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ar1datab.8k", 0, 0x080000, CRC(c26306f8) SHA1(6d8d993c076d5ced523143a86bd0938b3794478d) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "ar1scg0.12f", 0x000000, 0x200000, CRC(e7be830a) SHA1(60e2162eecd7401a0c26c525de2715cbfb10c1c5) )
	ROM_LOAD( "ar1scg1.10f", 0x200000, 0x200000, CRC(8f15a686) SHA1(bce2d4380c6c39aa402566ddb0f62bbe6d7bfa1d) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ar1cg0.8d", 0x000000, 0x200000, CRC(93f3a9d9) SHA1(7e94c81ad5ace98a2f0d00d101d464883d38c197) )
	ROM_LOAD( "ar1cg1.10d", 0x200000, 0x200000, CRC(39828c8b) SHA1(424aa67eb0b898c9cab8a4749893a9c5696ac430) )
	ROM_LOAD( "ar1cg2.12d", 0x400000, 0x200000, CRC(f7b058d1) SHA1(fffd0f01724a26dd47b1ecceecf4a139d5746f81) )
	ROM_LOAD( "ar1cg3.13d", 0x600000, 0x200000, CRC(c28a3d2a) SHA1(cdc44fdbc99274e860c834e42b4cfafb478d4d26) )
	ROM_LOAD( "ar1cg4.14d", 0x800000, 0x200000, CRC(abdb161f) SHA1(260bff9b0e94c1b2ea4b9d7fa170fbca212e85ee) )
	ROM_LOAD( "ar1cg5.16d", 0xa00000, 0x200000, CRC(2381cfea) SHA1(1de4c8b94df233fd74771fa47843290a3d8df0c8) )
	ROM_LOAD( "ar1cg6.18d", 0xc00000, 0x200000, CRC(ca0b6d23) SHA1(df969e0eeec557a95584b06995b0d55f2c6ec70a) )
	ROM_LOAD( "ar1cg7.19d", 0xe00000, 0x200000, CRC(ffb9f9f9) SHA1(2b8c75b580f77e887df7d50909a3a95cda570e20) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ar1ccrl.3d", 0x000000, 0x200000, CRC(17387b2c) SHA1(dfd7cadaf97917347c0fa98f395364a543e49612) )
	ROM_LOAD( "ar1ccrh.1d", 0x200000, 0x080000, CRC(ee7a4803) SHA1(8383c9a8ef5ed94df13446ca5cefa5f9e518f175) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ar1ptrl0.18k", 0x000000, 0x080000, CRC(82405108) SHA1(0a40882a9bc8621c620bede404c78f6b1333f223) )
	ROM_LOAD( "ar1ptrl1.16k", 0x080000, 0x080000, CRC(8739b09c) SHA1(cd603c4dc2f9ffc4185f891eb83e4c383c564294) )
	ROM_LOAD( "ar1ptrl2.15k", 0x100000, 0x080000, CRC(bda693a9) SHA1(fe71dd3c63198737aa2d39527f0004e977e3be20) )
	ROM_LOAD( "ar1ptrl3.14k", 0x180000, 0x080000, CRC(82797405) SHA1(2f205fee2d33e183c80a906fb38900167c011240) )
	ROM_LOAD( "ar1ptrm0.18j", 0x200000, 0x080000, CRC(64bd6620) SHA1(2e33ff22208805ece304128be8887646fc890f6d) )
	ROM_LOAD( "ar1ptrm1.16j", 0x280000, 0x080000, CRC(2232f0a5) SHA1(3fccf6d4a0c4100cc85e3051024d659c4a1c769e) )
	ROM_LOAD( "ar1ptrm2.15j", 0x300000, 0x080000, CRC(8ee14e6f) SHA1(f6f1cbb748b109b365255378c18e710ba6270c1c) )
	ROM_LOAD( "ar1ptrm3.14j", 0x380000, 0x080000, CRC(1094a970) SHA1(d41b10f48e1ef312bcaf09f27fabc7252c30e648) )
	ROM_LOAD( "ar1ptru0.18f", 0x400000, 0x080000, CRC(26d88467) SHA1(d528f989fab4dd5ac1aec9b596a05fbadcc0587a) )
	ROM_LOAD( "ar1ptru1.16f", 0x480000, 0x080000, CRC(c5e2c208) SHA1(152fde0b95a5df8c781e4a83577cfbbc7672ae0d) )
	ROM_LOAD( "ar1ptru2.15f", 0x500000, 0x080000, CRC(1321ec59) SHA1(dbd3687a4c6b1aa0b18e336f99dabb9010d36639) )
	ROM_LOAD( "ar1ptru3.14f", 0x580000, 0x080000, CRC(139d7dc1) SHA1(6d25e6ad552a91a0c5fc03db7e1a801ccf9c9556) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "ar1wavea.2l", 0, 0x200000, CRC(dbf64562) SHA1(454fd7d5b860f0e5557d8900393be95d6c992ad1) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "alpiner_defaults.nv", 0x0000, 0x2000, CRC(efbef3d8) SHA1(459035600655cd83780db6c59aba044981cdcdc4) )
ROM_END

ROM_START( alpinerc )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ar2verc.1", 0x00003, 0x100000, CRC(61323842) SHA1(e3c33248340bee252f230124fa9b7fa935a60565) )
	ROM_LOAD32_BYTE( "ar2verc.2", 0x00002, 0x100000, CRC(43795b2d) SHA1(e060f3259661279a36300431c5ca7347bde8b6ec) )
	ROM_LOAD32_BYTE( "ar2verc.3", 0x00001, 0x100000, CRC(acb3003b) SHA1(ea0cbf3a1607b06b108df051f38fec1f214f42d2) )
	ROM_LOAD32_BYTE( "ar2verc.4", 0x00000, 0x100000, CRC(800acc21) SHA1(41d26766da2db46954a2351bbc50aea94bc1d564) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ar1datab.8k", 0, 0x080000, CRC(c26306f8) SHA1(6d8d993c076d5ced523143a86bd0938b3794478d) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "ar1scg0.12f", 0x000000, 0x200000, CRC(e7be830a) SHA1(60e2162eecd7401a0c26c525de2715cbfb10c1c5) )
	ROM_LOAD( "ar1scg1.10f", 0x200000, 0x200000, CRC(8f15a686) SHA1(bce2d4380c6c39aa402566ddb0f62bbe6d7bfa1d) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ar1cg0.8d", 0x000000, 0x200000, CRC(93f3a9d9) SHA1(7e94c81ad5ace98a2f0d00d101d464883d38c197) )
	ROM_LOAD( "ar1cg1.10d", 0x200000, 0x200000, CRC(39828c8b) SHA1(424aa67eb0b898c9cab8a4749893a9c5696ac430) )
	ROM_LOAD( "ar1cg2.12d", 0x400000, 0x200000, CRC(f7b058d1) SHA1(fffd0f01724a26dd47b1ecceecf4a139d5746f81) )
	ROM_LOAD( "ar1cg3.13d", 0x600000, 0x200000, CRC(c28a3d2a) SHA1(cdc44fdbc99274e860c834e42b4cfafb478d4d26) )
	ROM_LOAD( "ar1cg4.14d", 0x800000, 0x200000, CRC(abdb161f) SHA1(260bff9b0e94c1b2ea4b9d7fa170fbca212e85ee) )
	ROM_LOAD( "ar1cg5.16d", 0xa00000, 0x200000, CRC(2381cfea) SHA1(1de4c8b94df233fd74771fa47843290a3d8df0c8) )
	ROM_LOAD( "ar1cg6.18d", 0xc00000, 0x200000, CRC(ca0b6d23) SHA1(df969e0eeec557a95584b06995b0d55f2c6ec70a) )
	ROM_LOAD( "ar1cg7.19d", 0xe00000, 0x200000, CRC(ffb9f9f9) SHA1(2b8c75b580f77e887df7d50909a3a95cda570e20) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ar1ccrl.3d", 0x000000, 0x200000, CRC(17387b2c) SHA1(dfd7cadaf97917347c0fa98f395364a543e49612) )
	ROM_LOAD( "ar1ccrh.1d", 0x200000, 0x080000, CRC(ee7a4803) SHA1(8383c9a8ef5ed94df13446ca5cefa5f9e518f175) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ar1ptrl0.18k", 0x000000, 0x080000, CRC(82405108) SHA1(0a40882a9bc8621c620bede404c78f6b1333f223) )
	ROM_LOAD( "ar1ptrl1.16k", 0x080000, 0x080000, CRC(8739b09c) SHA1(cd603c4dc2f9ffc4185f891eb83e4c383c564294) )
	ROM_LOAD( "ar1ptrl2.15k", 0x100000, 0x080000, CRC(bda693a9) SHA1(fe71dd3c63198737aa2d39527f0004e977e3be20) )
	ROM_LOAD( "ar1ptrl3.14k", 0x180000, 0x080000, CRC(82797405) SHA1(2f205fee2d33e183c80a906fb38900167c011240) )
	ROM_LOAD( "ar1ptrm0.18j", 0x200000, 0x080000, CRC(64bd6620) SHA1(2e33ff22208805ece304128be8887646fc890f6d) )
	ROM_LOAD( "ar1ptrm1.16j", 0x280000, 0x080000, CRC(2232f0a5) SHA1(3fccf6d4a0c4100cc85e3051024d659c4a1c769e) )
	ROM_LOAD( "ar1ptrm2.15j", 0x300000, 0x080000, CRC(8ee14e6f) SHA1(f6f1cbb748b109b365255378c18e710ba6270c1c) )
	ROM_LOAD( "ar1ptrm3.14j", 0x380000, 0x080000, CRC(1094a970) SHA1(d41b10f48e1ef312bcaf09f27fabc7252c30e648) )
	ROM_LOAD( "ar1ptru0.18f", 0x400000, 0x080000, CRC(26d88467) SHA1(d528f989fab4dd5ac1aec9b596a05fbadcc0587a) )
	ROM_LOAD( "ar1ptru1.16f", 0x480000, 0x080000, CRC(c5e2c208) SHA1(152fde0b95a5df8c781e4a83577cfbbc7672ae0d) )
	ROM_LOAD( "ar1ptru2.15f", 0x500000, 0x080000, CRC(1321ec59) SHA1(dbd3687a4c6b1aa0b18e336f99dabb9010d36639) )
	ROM_LOAD( "ar1ptru3.14f", 0x580000, 0x080000, CRC(139d7dc1) SHA1(6d25e6ad552a91a0c5fc03db7e1a801ccf9c9556) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "ar1wavea.2l", 0, 0x200000, CRC(dbf64562) SHA1(454fd7d5b860f0e5557d8900393be95d6c992ad1) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "alpiner_defaults.nv", 0x0000, 0x2000, CRC(efbef3d8) SHA1(459035600655cd83780db6c59aba044981cdcdc4) )
ROM_END

ROM_START( alpinerjc )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_WORD_SWAP( "ar1verc.1", 0x00002, 0x200000, CRC(8a87dc27) SHA1(ea3675667dce4c184da61dbb204e39da7ae93e82) )
	ROM_LOAD32_WORD_SWAP( "ar1verc.2", 0x00000, 0x200000, CRC(5f4f5615) SHA1(ff8f818a4017ee8546037a10522f2cf1de6dc7cd) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ar1datab.8k", 0, 0x080000, CRC(c26306f8) SHA1(6d8d993c076d5ced523143a86bd0938b3794478d) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "ar1scg0.12f", 0x000000, 0x200000, CRC(e7be830a) SHA1(60e2162eecd7401a0c26c525de2715cbfb10c1c5) )
	ROM_LOAD( "ar1scg1.10f", 0x200000, 0x200000, CRC(8f15a686) SHA1(bce2d4380c6c39aa402566ddb0f62bbe6d7bfa1d) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ar1cg0.8d", 0x000000, 0x200000, CRC(93f3a9d9) SHA1(7e94c81ad5ace98a2f0d00d101d464883d38c197) )
	ROM_LOAD( "ar1cg1.10d", 0x200000, 0x200000, CRC(39828c8b) SHA1(424aa67eb0b898c9cab8a4749893a9c5696ac430) )
	ROM_LOAD( "ar1cg2.12d", 0x400000, 0x200000, CRC(f7b058d1) SHA1(fffd0f01724a26dd47b1ecceecf4a139d5746f81) )
	ROM_LOAD( "ar1cg3.13d", 0x600000, 0x200000, CRC(c28a3d2a) SHA1(cdc44fdbc99274e860c834e42b4cfafb478d4d26) )
	ROM_LOAD( "ar1cg4.14d", 0x800000, 0x200000, CRC(abdb161f) SHA1(260bff9b0e94c1b2ea4b9d7fa170fbca212e85ee) )
	ROM_LOAD( "ar1cg5.16d", 0xa00000, 0x200000, CRC(2381cfea) SHA1(1de4c8b94df233fd74771fa47843290a3d8df0c8) )
	ROM_LOAD( "ar1cg6.18d", 0xc00000, 0x200000, CRC(ca0b6d23) SHA1(df969e0eeec557a95584b06995b0d55f2c6ec70a) )
	ROM_LOAD( "ar1cg7.19d", 0xe00000, 0x200000, CRC(ffb9f9f9) SHA1(2b8c75b580f77e887df7d50909a3a95cda570e20) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ar1ccrl.3d", 0x000000, 0x200000, CRC(17387b2c) SHA1(dfd7cadaf97917347c0fa98f395364a543e49612) )
	ROM_LOAD( "ar1ccrh.1d", 0x200000, 0x080000, CRC(ee7a4803) SHA1(8383c9a8ef5ed94df13446ca5cefa5f9e518f175) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ar1ptrl0.18k", 0x000000, 0x080000, CRC(82405108) SHA1(0a40882a9bc8621c620bede404c78f6b1333f223) )
	ROM_LOAD( "ar1ptrl1.16k", 0x080000, 0x080000, CRC(8739b09c) SHA1(cd603c4dc2f9ffc4185f891eb83e4c383c564294) )
	ROM_LOAD( "ar1ptrl2.15k", 0x100000, 0x080000, CRC(bda693a9) SHA1(fe71dd3c63198737aa2d39527f0004e977e3be20) )
	ROM_LOAD( "ar1ptrl3.14k", 0x180000, 0x080000, CRC(82797405) SHA1(2f205fee2d33e183c80a906fb38900167c011240) )
	ROM_LOAD( "ar1ptrm0.18j", 0x200000, 0x080000, CRC(64bd6620) SHA1(2e33ff22208805ece304128be8887646fc890f6d) )
	ROM_LOAD( "ar1ptrm1.16j", 0x280000, 0x080000, CRC(2232f0a5) SHA1(3fccf6d4a0c4100cc85e3051024d659c4a1c769e) )
	ROM_LOAD( "ar1ptrm2.15j", 0x300000, 0x080000, CRC(8ee14e6f) SHA1(f6f1cbb748b109b365255378c18e710ba6270c1c) )
	ROM_LOAD( "ar1ptrm3.14j", 0x380000, 0x080000, CRC(1094a970) SHA1(d41b10f48e1ef312bcaf09f27fabc7252c30e648) )
	ROM_LOAD( "ar1ptru0.18f", 0x400000, 0x080000, CRC(26d88467) SHA1(d528f989fab4dd5ac1aec9b596a05fbadcc0587a) )
	ROM_LOAD( "ar1ptru1.16f", 0x480000, 0x080000, CRC(c5e2c208) SHA1(152fde0b95a5df8c781e4a83577cfbbc7672ae0d) )
	ROM_LOAD( "ar1ptru2.15f", 0x500000, 0x080000, CRC(1321ec59) SHA1(dbd3687a4c6b1aa0b18e336f99dabb9010d36639) )
	ROM_LOAD( "ar1ptru3.14f", 0x580000, 0x080000, CRC(139d7dc1) SHA1(6d25e6ad552a91a0c5fc03db7e1a801ccf9c9556) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "ar1wavea.2l", 0, 0x200000, CRC(dbf64562) SHA1(454fd7d5b860f0e5557d8900393be95d6c992ad1) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "alpiner_defaults.nv", 0x0000, 0x2000, CRC(efbef3d8) SHA1(459035600655cd83780db6c59aba044981cdcdc4) )
ROM_END

ROM_START( airco22b )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "acs1verb.1", 0x00003, 0x100000, CRC(062c4f61) SHA1(98e1c75dd0f493eb6ebb64b46543217c1d40116e) )
	ROM_LOAD32_BYTE( "acs1verb.2", 0x00002, 0x100000, CRC(8ae69711) SHA1(4c5323fa8f0419275e330fec66d1fb2b89bb3795) )
	ROM_LOAD32_BYTE( "acs1verb.3", 0x00001, 0x100000, CRC(71738e67) SHA1(eb8c66dedbeff911b6166ebbda466fb9656ef0fb) )
	ROM_LOAD32_BYTE( "acs1verb.4", 0x00000, 0x100000, CRC(3b193add) SHA1(5e3bca13905bfa3a2947f4f16ca01878b0a14a3a) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "acs1data.8k", 0, 0x080000, CRC(33824bc9) SHA1(80ec63883770e5eec1f5f1ddc16a85ef8f22a48b) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "acs1scg0.12f", 0x000000, 0x200000, CRC(e5235404) SHA1(3133b71d1bde3a9815cd02e97382b8078b62b0bb) )
	ROM_LOAD( "acs1scg1.10f", 0x200000, 0x200000, CRC(828e91e7) SHA1(8383b029cd29fbad107fd49e866defb50c11c99a) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "acs1cg0.8d", 0x000000, 0x200000, CRC(1f31343e) SHA1(25ba730cec74e0ed0b404f5c4430b7c3368c9b52) )
	ROM_LOAD( "acs1cg1.10d", 0x200000, 0x200000, CRC(ccd5481d) SHA1(050e6fc7d4e0591f8ffc9552d140b6bd4533c06d) )
	ROM_LOAD( "acs1cg2.12d", 0x400000, 0x200000, CRC(14e5d0d2) SHA1(3147ad11098030e9cfd93fbc0a1b3aafa8b8aba6) )
	ROM_LOAD( "acs1cg3.13d", 0x600000, 0x200000, CRC(1a7bcc16) SHA1(bbc4ca5b208bea8394d1679e4e2d17d22331e2c8) )
	ROM_LOAD( "acs1cg4.14d", 0x800000, 0x200000, CRC(1920b7fb) SHA1(56318f2a96c55998bb9a8d791d56be3dfb39867e) )
	ROM_LOAD( "acs1cg5.16d", 0xa00000, 0x200000, CRC(3dd109b7) SHA1(a7f914b9b80f1bca1afb6144698578a29ca74676) )
	ROM_LOAD( "acs1cg6.18d", 0xc00000, 0x200000, CRC(ec71c8a3) SHA1(86892a91883d483ca0d422b78fa36042e02f3ad3) )
	ROM_LOAD( "acs1cg7.19d", 0xe00000, 0x200000, CRC(82271757) SHA1(023c935e78b14da310e4c29da8785b82aa3241ac) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "acs1ccrl.3d", 0x000000, 0x200000, CRC(07088ba1) SHA1(a962c0821d5af28ed508cfdbd613675454e306e3) )
	ROM_LOAD( "acs1ccrh.1d", 0x200000, 0x080000, CRC(62936af6) SHA1(ca80b68415aa2cd2ce4e90404f10640d0ae38be9) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "acs1ptl0.18k", 0x000000, 0x080000, CRC(bd5896c7) SHA1(58ec7d0f1e0bfdbf4908e1d920bbd7f094993777) )
	ROM_LOAD( "acs1ptl1.16k", 0x080000, 0x080000, CRC(e583b975) SHA1(beb0cc2b44bc69af057c2bb744cd7e1b95de577a) )
	ROM_LOAD( "acs1ptl2.15k", 0x100000, 0x080000, CRC(802d737a) SHA1(3d99a369db70d13fb87c2ff26c82b4b39afe94d9) )
	ROM_LOAD( "acs1ptl3.14k", 0x180000, 0x080000, CRC(fe556ecb) SHA1(9d9dbbb4f1d3688fb763001834640d79d9987d47) )
	ROM_LOAD( "acs1ptm0.18j", 0x200000, 0x080000, CRC(949b6c58) SHA1(6ea016551b10f5d5764921dcc5a4b81d2b93d701) )
	ROM_LOAD( "acs1ptm1.16j", 0x280000, 0x080000, CRC(8b2b99d9) SHA1(89c3545c4035509307728a9577018c1100ce3a54) )
	ROM_LOAD( "acs1ptm2.15j", 0x300000, 0x080000, CRC(f1515080) SHA1(27a87217a140477a6840a610c95ae57abc0d01a6) )
	ROM_LOAD( "acs1ptm3.14j", 0x380000, 0x080000, CRC(e364f4aa) SHA1(3af6a864765871664cccad82c4795f677be68d51) )
	ROM_LOAD( "acs1ptu0.18f", 0x400000, 0x080000, CRC(746b3084) SHA1(73397d1f22300fb3a81a0a068da4d0a8cfdc0a36) )
	ROM_LOAD( "acs1ptu1.16f", 0x480000, 0x080000, CRC(b44f1d3b) SHA1(f3f1a85c082053653e4da7d7f01f1baef1a013c8) )
	ROM_LOAD( "acs1ptu2.15f", 0x500000, 0x080000, CRC(fdd2d778) SHA1(0269f971d778e908a1efb5a63b08fb3365d98c2a) )
	ROM_LOAD( "acs1ptu3.14f", 0x580000, 0x080000, CRC(38b425d4) SHA1(8ff6dd6775d42afdff4c9fb2232e4d72b38e515a) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "acs1wav0.1", 0x400000, 0x400000, CRC(52fb9762) SHA1(125c163e62d701c2e17ba0b572ed27c944ca0412) )
	ROM_LOAD( "acs1wav1.2", 0x800000, 0x400000, CRC(b568dca2) SHA1(503deb277691d801acac1380ded2868a5d5ac501) )
ROM_END

ROM_START( cybrcycc )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "cb2verc.1", 0x00003, 0x100000, CRC(a8e07a14) SHA1(9bef7068c9bf792960df922ea79e4565d7680433) )
	ROM_LOAD32_BYTE( "cb2verc.2", 0x00002, 0x100000, CRC(054c504f) SHA1(9bde803ff09be0402f9b0388e55407362a2508e3) )
	ROM_LOAD32_BYTE( "cb2verc.3", 0x00001, 0x100000, CRC(47e6306c) SHA1(39d6fc2c3cb9b4c9d3569cedb79b916a90537115) )
	ROM_LOAD32_BYTE( "cb2verc.4", 0x00000, 0x100000, CRC(398426e4) SHA1(f20cd4892420e7b978baa51c9129b362422a3895) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "cb1datab.8k", 0, 0x080000, CRC(e2404221) SHA1(b88810dd45aee8a5475c30806cdfded25fa14e0e) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "cb1scg0.12f", 0x000000, 0x200000, CRC(7aaca90d) SHA1(9808819db5d86d555a03bb20a2fbedf060d04f0e) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "cb1cg0.8d", 0x000000, 0x200000, CRC(762a47a0) SHA1(8a49c700dca7afec5d8d6a38fedcd3ad4b0e6713) )
	ROM_LOAD( "cb1cg1.10d", 0x200000, 0x200000, CRC(df92c3e6) SHA1(302d7ee7e073a45e7baa948543bd30251f903a6d) )
	ROM_LOAD( "cb1cg2.12d", 0x400000, 0x200000, CRC(07bc508e) SHA1(7675694d10b50e57bb10b350559bd321df75d1ea) )
	ROM_LOAD( "cb1cg3.13d", 0x600000, 0x200000, CRC(50c86dea) SHA1(7837a1d2bd3ade470f7fbc732513dd598badd219) )
	ROM_LOAD( "cb1cg4.14d", 0x800000, 0x200000, CRC(e93b8894) SHA1(4d28b557b7ed2667e6af9f970f3e99cda785b940) )
	ROM_LOAD( "cb1cg5.16d", 0xa00000, 0x200000, CRC(9ee610a1) SHA1(ebc7892b6a66461ca6b6b912a264da1594340b2d) )
	ROM_LOAD( "cb1cg6.18d", 0xc00000, 0x200000, CRC(ddc3b5cc) SHA1(34edffee9eb6fbf4a00fce0da34d9354b1a1155f) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "cb1ccrl.3d", 0x000000, 0x200000, CRC(2f171c48) SHA1(52b76213e37379b4a5cea7de40cf5396dc2998d8) )
	ROM_LOAD( "cb1ccrh.1d", 0x200000, 0x080000, CRC(86124b93) SHA1(f2cfd726313cbeff162d402a15de2360377630e7) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "cb1ptrl0.18k", 0x000000, 0x080000, CRC(f1393a03) SHA1(c9e808601eef5839e6bff630e5f83380e073c5c0) )
	ROM_LOAD( "cb1ptrl1.16k", 0x080000, 0x080000, CRC(2ad51de7) SHA1(efd102b960ca10cda70da84661acf61e4bbb9f00) )
	ROM_LOAD( "cb1ptrl2.15k", 0x100000, 0x080000, CRC(78f77c0d) SHA1(5183a8909c2ac0a3d80e707393bcbb4441d79a3c) )
	ROM_LOAD( "cb1ptrl3.14k", 0x180000, 0x080000, CRC(804bfb4a) SHA1(74b3fc3931265398e23605d3da7ca84a002da632) )
	ROM_LOAD( "cb1ptrm0.18j", 0x200000, 0x080000, CRC(f4eece49) SHA1(3f34d1ae5986f0d340563ab0fb637bfdacb8712c) )
	ROM_LOAD( "cb1ptrm1.16j", 0x280000, 0x080000, CRC(5f3cbd7d) SHA1(d00d0a96b71d6a3b98907c4ba7c702e549dd0adb) )
	ROM_LOAD( "cb1ptrm2.15j", 0x300000, 0x080000, CRC(02c7e4af) SHA1(6a541a28163b1026a824f6f8aed05d0eb0c8ae93) )
	ROM_LOAD( "cb1ptrm3.14j", 0x380000, 0x080000, CRC(ace3123b) SHA1(2b590ed967572d77b3cc6b37e341a5bdc55c762f) )
	ROM_LOAD( "cb1ptru0.18f", 0x400000, 0x080000, CRC(58d35341) SHA1(a5fe00bdcf39521f0465743664ff0dd78be5d6e8) )
	ROM_LOAD( "cb1ptru1.16f", 0x480000, 0x080000, CRC(f4d005b0) SHA1(0862ed1dd0818bfb765d97f1f9d996c321b0ec83) )
	ROM_LOAD( "cb1ptru2.15f", 0x500000, 0x080000, CRC(68ffcd50) SHA1(5ca5f71b6b079fde14d76c869d211a815bffae68) )
	ROM_LOAD( "cb1ptru3.14f", 0x580000, 0x080000, CRC(d89c1c2b) SHA1(9c25df696b2d120ce33d7774381460297740007a) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "cb1wavea.2l", 0x000000, 0x400000, CRC(b79a624d) SHA1(c0ee358a183ba6d0835731dbdd191b64718fde6e) )
	ROM_LOAD( "cb1waveb.1l", 0x800000, 0x200000, CRC(33bf08f6) SHA1(bf9d68b26a8158ea1abfe8428b7454cac25242c5) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "cybrcycc_defaults.nv", 0x0000, 0x2000, CRC(57fbd7d3) SHA1(c93e0d7875f5e66a661aed757fb4a314fe2025c2) )
ROM_END

ROM_START( cybrcyccj )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "cb1verc.1", 0x00003, 0x100000, CRC(91166c69) SHA1(08e4d0d8226234b01b7d4fbc8ba2ade1ae409fe4) )
	ROM_LOAD32_BYTE( "cb1verc.2", 0x00002, 0x100000, CRC(b2e25e15) SHA1(095df28601dbc98b8974cd99b9329312980eacaf) )
	ROM_LOAD32_BYTE( "cb1verc.3", 0x00001, 0x100000, CRC(97fa2f39) SHA1(865e016553f733430aac70809b21a3b2914bb638) )
	ROM_LOAD32_BYTE( "cb1verc.4", 0x00000, 0x100000, CRC(529bd227) SHA1(6d7979643d015ae388a4a93b9dccc7f43f93baff) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "cb1datab.8k", 0, 0x080000, CRC(e2404221) SHA1(b88810dd45aee8a5475c30806cdfded25fa14e0e) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "cb1scg0.12f", 0x000000, 0x200000, CRC(7aaca90d) SHA1(9808819db5d86d555a03bb20a2fbedf060d04f0e) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "cb1cg0.8d", 0x000000, 0x200000, CRC(762a47a0) SHA1(8a49c700dca7afec5d8d6a38fedcd3ad4b0e6713) )
	ROM_LOAD( "cb1cg1.10d", 0x200000, 0x200000, CRC(df92c3e6) SHA1(302d7ee7e073a45e7baa948543bd30251f903a6d) )
	ROM_LOAD( "cb1cg2.12d", 0x400000, 0x200000, CRC(07bc508e) SHA1(7675694d10b50e57bb10b350559bd321df75d1ea) )
	ROM_LOAD( "cb1cg3.13d", 0x600000, 0x200000, CRC(50c86dea) SHA1(7837a1d2bd3ade470f7fbc732513dd598badd219) )
	ROM_LOAD( "cb1cg4.14d", 0x800000, 0x200000, CRC(e93b8894) SHA1(4d28b557b7ed2667e6af9f970f3e99cda785b940) )
	ROM_LOAD( "cb1cg5.16d", 0xa00000, 0x200000, CRC(9ee610a1) SHA1(ebc7892b6a66461ca6b6b912a264da1594340b2d) )
	ROM_LOAD( "cb1cg6.18d", 0xc00000, 0x200000, CRC(ddc3b5cc) SHA1(34edffee9eb6fbf4a00fce0da34d9354b1a1155f) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "cb1ccrl.3d", 0x000000, 0x200000, CRC(2f171c48) SHA1(52b76213e37379b4a5cea7de40cf5396dc2998d8) )
	ROM_LOAD( "cb1ccrh.1d", 0x200000, 0x080000, CRC(86124b93) SHA1(f2cfd726313cbeff162d402a15de2360377630e7) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "cb1ptrl0.18k", 0x000000, 0x080000, CRC(f1393a03) SHA1(c9e808601eef5839e6bff630e5f83380e073c5c0) )
	ROM_LOAD( "cb1ptrl1.16k", 0x080000, 0x080000, CRC(2ad51de7) SHA1(efd102b960ca10cda70da84661acf61e4bbb9f00) )
	ROM_LOAD( "cb1ptrl2.15k", 0x100000, 0x080000, CRC(78f77c0d) SHA1(5183a8909c2ac0a3d80e707393bcbb4441d79a3c) )
	ROM_LOAD( "cb1ptrl3.14k", 0x180000, 0x080000, CRC(804bfb4a) SHA1(74b3fc3931265398e23605d3da7ca84a002da632) )
	ROM_LOAD( "cb1ptrm0.18j", 0x200000, 0x080000, CRC(f4eece49) SHA1(3f34d1ae5986f0d340563ab0fb637bfdacb8712c) )
	ROM_LOAD( "cb1ptrm1.16j", 0x280000, 0x080000, CRC(5f3cbd7d) SHA1(d00d0a96b71d6a3b98907c4ba7c702e549dd0adb) )
	ROM_LOAD( "cb1ptrm2.15j", 0x300000, 0x080000, CRC(02c7e4af) SHA1(6a541a28163b1026a824f6f8aed05d0eb0c8ae93) )
	ROM_LOAD( "cb1ptrm3.14j", 0x380000, 0x080000, CRC(ace3123b) SHA1(2b590ed967572d77b3cc6b37e341a5bdc55c762f) )
	ROM_LOAD( "cb1ptru0.18f", 0x400000, 0x080000, CRC(58d35341) SHA1(a5fe00bdcf39521f0465743664ff0dd78be5d6e8) )
	ROM_LOAD( "cb1ptru1.16f", 0x480000, 0x080000, CRC(f4d005b0) SHA1(0862ed1dd0818bfb765d97f1f9d996c321b0ec83) )
	ROM_LOAD( "cb1ptru2.15f", 0x500000, 0x080000, CRC(68ffcd50) SHA1(5ca5f71b6b079fde14d76c869d211a815bffae68) )
	ROM_LOAD( "cb1ptru3.14f", 0x580000, 0x080000, CRC(d89c1c2b) SHA1(9c25df696b2d120ce33d7774381460297740007a) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "cb1wavea.2l", 0x000000, 0x400000, CRC(b79a624d) SHA1(c0ee358a183ba6d0835731dbdd191b64718fde6e) )
	ROM_LOAD( "cb1waveb.1l", 0x800000, 0x200000, CRC(33bf08f6) SHA1(bf9d68b26a8158ea1abfe8428b7454cac25242c5) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "cybrcycc_defaults.nv", 0x0000, 0x2000, CRC(57fbd7d3) SHA1(c93e0d7875f5e66a661aed757fb4a314fe2025c2) )
ROM_END

ROM_START( dirtdash )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "dt2verc.rom1", 0x000003, 0x100000, CRC(b2d7916c) SHA1(4f4cb75ef12b57bdc12b6aba59678f09b22bcfce) )
	ROM_LOAD32_BYTE( "dt2verc.rom2", 0x000002, 0x100000, CRC(5dc040d1) SHA1(4b285d146adb0cff146b79d7471597148b3c4a97) )
	ROM_LOAD32_BYTE( "dt2verc.rom3", 0x000001, 0x100000, CRC(a55157e1) SHA1(ede052ad55430418905987b8bcba4aa229ac037c) )
	ROM_LOAD32_BYTE( "dt2verc.rom4", 0x000000, 0x100000, CRC(f3a9bedb) SHA1(71366108dec0803b585d409095f126cc55ec3e4d) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "dt1dataa.8k", 0x000000, 0x080000, CRC(9bcdea21) SHA1(26ae025cf746d3a703a82495eb2bb515b828a650) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "dt1scg0.12f", 0x000000, 0x200000, CRC(a09b5760) SHA1(3dd54ebebf9da1de76874a1adf491ed15849e1b1) )
	ROM_LOAD( "dt1scg1.10f", 0x200000, 0x200000, CRC(f9ac8111) SHA1(814074ae8cc81c6c1201d764a84dd95fe914f19c) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "dt1cg0.8d", 0x000000, 0x200000, CRC(10ab95e0) SHA1(ffde1f00ac3e82a36fbcfa060c6b97c92dfcfc8b) )
	ROM_LOAD( "dt1cg1.10d", 0x200000, 0x200000, CRC(d9f1ba53) SHA1(5a1095b726c55001cc1d4c695adc38097e6a0201) )
	ROM_LOAD( "dt1cg2.12d", 0x400000, 0x200000, CRC(bd8b1e0b) SHA1(fcd94e33a0cbd17c9308cb8952e3c618ab56f9fc) )
	ROM_LOAD( "dt1cg3.13d", 0x600000, 0x200000, CRC(ba960663) SHA1(e98149bc4652ea7933ac47d760a6f7e6489f15e2) )
	ROM_LOAD( "dt1cg4.14d", 0x800000, 0x200000, CRC(424b9652) SHA1(fa8865110db03559740c4e633e123d1a009782c4) )
	ROM_LOAD( "dt1cg5.16d", 0xa00000, 0x200000, CRC(29516626) SHA1(1f12c5dc3975b88dc60d87d0409bf311837e9fa4) )
	ROM_LOAD( "dt1cg6.18d", 0xc00000, 0x200000, CRC(e6fa7180) SHA1(85316cde282cff1f913cf9f155cfa36adcc1108e) )
	ROM_LOAD( "dt1cg7.19d", 0xe00000, 0x200000, CRC(2ca19153) SHA1(c82403c8b40bf85daedf610b1bc7bfea9dfc6206) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "dt1ccrl.3d", 0x000000, 0x200000, CRC(e536b313) SHA1(7357da993d2bb3fcc8c1c2feb53689ad368cd80a) )
	ROM_LOAD( "dt1ccrh.1d", 0x200000, 0x080000, CRC(af257064) SHA1(0da561d9f8824618c00209ccef6146e9f3ad72bb) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "dt1ptrl0.18k", 0x000000, 0x080000, CRC(4e0cac3a) SHA1(c2778e9e93be2de729c6f118caf62ac9f48efbb0) )
	ROM_LOAD( "dt1ptrl1.16k", 0x080000, 0x080000, CRC(59ba9dba) SHA1(a2e9488cf0ff255284c06a1ef653ae86c0d98adc) )
	ROM_LOAD( "dt1ptrl2.15k", 0x100000, 0x080000, CRC(cfe80c67) SHA1(ba3bc48aa39712e63c915070a76974fbd560dee6) )
	ROM_LOAD( "dt1ptrm0.18j", 0x180000, 0x080000, CRC(41f34337) SHA1(7e624e7b6fdefe156168b1c9cc5e919db3b2fbaa) )
	ROM_LOAD( "dt1ptrm1.16j", 0x200000, 0x080000, CRC(f620fd41) SHA1(18cf6e11eb68da1b7f7fcc32562dc952c247de65) )
	ROM_LOAD( "dt1ptrm2.15j", 0x280000, 0x080000, CRC(71e6714d) SHA1(6aad6db3be5020213d7add61c7d927ae9c4fea4e) )
	ROM_LOAD( "dt1ptru0.18f", 0x300000, 0x080000, CRC(4909bd7d) SHA1(0e4ef3987c43ef0438331b82b50dcc97363a45d0) )
	ROM_LOAD( "dt1ptru1.16f", 0x380000, 0x080000, CRC(4a5097df) SHA1(a9c814b0ed4bd92accd0e57be8e3d887114b06a5) )
	ROM_LOAD( "dt1ptru2.15f", 0x400000, 0x080000, CRC(1171eaf5) SHA1(168365ea619386f218585c49025cdd7fd1224082) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "dt1wavea.2l", 0x000000, 0x400000, CRC(cbd52e40) SHA1(dc995dd919548c96a90efb0375e5b5f1055e05cb) )
	ROM_LOAD( "dt1waveb.1l", 0x800000, 0x400000, CRC(6b736f94) SHA1(ac3715480aa9a9c2dec099607f89859bb3b73a6a) )
ROM_END

ROM_START( dirtdasha )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_WORD_SWAP( "dt2vera.1", 0x000002, 0x200000, CRC(402a3d73) SHA1(009b57ed0ea228ccedb139d945b9eaf2a36e2502) )
	ROM_LOAD32_WORD_SWAP( "dt2vera.2", 0x000000, 0x200000, CRC(66ed140d) SHA1(a472fdc7b6aaeb4b3643ecdafd32fa665e7c7aa2) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "dt1dataa.8k", 0x000000, 0x080000, CRC(9bcdea21) SHA1(26ae025cf746d3a703a82495eb2bb515b828a650) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "dt1scg0.12f", 0x000000, 0x200000, CRC(a09b5760) SHA1(3dd54ebebf9da1de76874a1adf491ed15849e1b1) )
	ROM_LOAD( "dt1scg1.10f", 0x200000, 0x200000, CRC(f9ac8111) SHA1(814074ae8cc81c6c1201d764a84dd95fe914f19c) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "dt1cg0.8d", 0x000000, 0x200000, CRC(10ab95e0) SHA1(ffde1f00ac3e82a36fbcfa060c6b97c92dfcfc8b) )
	ROM_LOAD( "dt1cg1.10d", 0x200000, 0x200000, CRC(d9f1ba53) SHA1(5a1095b726c55001cc1d4c695adc38097e6a0201) )
	ROM_LOAD( "dt1cg2.12d", 0x400000, 0x200000, CRC(bd8b1e0b) SHA1(fcd94e33a0cbd17c9308cb8952e3c618ab56f9fc) )
	ROM_LOAD( "dt1cg3.13d", 0x600000, 0x200000, CRC(ba960663) SHA1(e98149bc4652ea7933ac47d760a6f7e6489f15e2) )
	ROM_LOAD( "dt1cg4.14d", 0x800000, 0x200000, CRC(424b9652) SHA1(fa8865110db03559740c4e633e123d1a009782c4) )
	ROM_LOAD( "dt1cg5.16d", 0xa00000, 0x200000, CRC(29516626) SHA1(1f12c5dc3975b88dc60d87d0409bf311837e9fa4) )
	ROM_LOAD( "dt1cg6.18d", 0xc00000, 0x200000, CRC(e6fa7180) SHA1(85316cde282cff1f913cf9f155cfa36adcc1108e) )
	ROM_LOAD( "dt1cg7.19d", 0xe00000, 0x200000, CRC(2ca19153) SHA1(c82403c8b40bf85daedf610b1bc7bfea9dfc6206) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "dt1ccrl.3d", 0x000000, 0x200000, CRC(e536b313) SHA1(7357da993d2bb3fcc8c1c2feb53689ad368cd80a) )
	ROM_LOAD( "dt1ccrh.1d", 0x200000, 0x080000, CRC(af257064) SHA1(0da561d9f8824618c00209ccef6146e9f3ad72bb) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "dt1ptrl0.18k", 0x000000, 0x080000, CRC(4e0cac3a) SHA1(c2778e9e93be2de729c6f118caf62ac9f48efbb0) )
	ROM_LOAD( "dt1ptrl1.16k", 0x080000, 0x080000, CRC(59ba9dba) SHA1(a2e9488cf0ff255284c06a1ef653ae86c0d98adc) )
	ROM_LOAD( "dt1ptrl2.15k", 0x100000, 0x080000, CRC(cfe80c67) SHA1(ba3bc48aa39712e63c915070a76974fbd560dee6) )
	ROM_LOAD( "dt1ptrm0.18j", 0x180000, 0x080000, CRC(41f34337) SHA1(7e624e7b6fdefe156168b1c9cc5e919db3b2fbaa) )
	ROM_LOAD( "dt1ptrm1.16j", 0x200000, 0x080000, CRC(f620fd41) SHA1(18cf6e11eb68da1b7f7fcc32562dc952c247de65) )
	ROM_LOAD( "dt1ptrm2.15j", 0x280000, 0x080000, CRC(71e6714d) SHA1(6aad6db3be5020213d7add61c7d927ae9c4fea4e) )
	ROM_LOAD( "dt1ptru0.18f", 0x300000, 0x080000, CRC(4909bd7d) SHA1(0e4ef3987c43ef0438331b82b50dcc97363a45d0) )
	ROM_LOAD( "dt1ptru1.16f", 0x380000, 0x080000, CRC(4a5097df) SHA1(a9c814b0ed4bd92accd0e57be8e3d887114b06a5) )
	ROM_LOAD( "dt1ptru2.15f", 0x400000, 0x080000, CRC(1171eaf5) SHA1(168365ea619386f218585c49025cdd7fd1224082) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "dt1wavea.2l", 0x000000, 0x400000, CRC(cbd52e40) SHA1(dc995dd919548c96a90efb0375e5b5f1055e05cb) )
	ROM_LOAD( "dt1waveb.1l", 0x800000, 0x400000, CRC(6b736f94) SHA1(ac3715480aa9a9c2dec099607f89859bb3b73a6a) )
ROM_END

ROM_START( dirtdashb )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* main program */
	ROM_LOAD32_BYTE( "dt2verb.rom1", 0x000003, 0x100000, CRC(d133c6d7) SHA1(6fbdb823771826ba8c62d8d85223eeda38c081e7) )
	ROM_LOAD32_BYTE( "dt2verb.rom2", 0x000002, 0x100000, CRC(ba4d7626) SHA1(c48b724a454b97c47122548c77793599e9d8f92c) )
	ROM_LOAD32_BYTE( "dt2verb.rom3", 0x000001, 0x100000, CRC(f0d9aff2) SHA1(2ebccbd9524d29c12e001d897fd81948fb6068fa) )
	ROM_LOAD32_BYTE( "dt2verb.rom4", 0x000000, 0x100000, CRC(9a80fc82) SHA1(81a14749a39d213db58527f7a98d48dbfca1c153) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "dt1dataa.8k", 0x000000, 0x080000, CRC(9bcdea21) SHA1(26ae025cf746d3a703a82495eb2bb515b828a650) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "dt1scg0.12f", 0x000000, 0x200000, CRC(a09b5760) SHA1(3dd54ebebf9da1de76874a1adf491ed15849e1b1) )
	ROM_LOAD( "dt1scg1.10f", 0x200000, 0x200000, CRC(f9ac8111) SHA1(814074ae8cc81c6c1201d764a84dd95fe914f19c) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "dt1cg0.8d", 0x000000, 0x200000, CRC(10ab95e0) SHA1(ffde1f00ac3e82a36fbcfa060c6b97c92dfcfc8b) )
	ROM_LOAD( "dt1cg1.10d", 0x200000, 0x200000, CRC(d9f1ba53) SHA1(5a1095b726c55001cc1d4c695adc38097e6a0201) )
	ROM_LOAD( "dt1cg2.12d", 0x400000, 0x200000, CRC(bd8b1e0b) SHA1(fcd94e33a0cbd17c9308cb8952e3c618ab56f9fc) )
	ROM_LOAD( "dt1cg3.13d", 0x600000, 0x200000, CRC(ba960663) SHA1(e98149bc4652ea7933ac47d760a6f7e6489f15e2) )
	ROM_LOAD( "dt1cg4.14d", 0x800000, 0x200000, CRC(424b9652) SHA1(fa8865110db03559740c4e633e123d1a009782c4) )
	ROM_LOAD( "dt1cg5.16d", 0xa00000, 0x200000, CRC(29516626) SHA1(1f12c5dc3975b88dc60d87d0409bf311837e9fa4) )
	ROM_LOAD( "dt1cg6.18d", 0xc00000, 0x200000, CRC(e6fa7180) SHA1(85316cde282cff1f913cf9f155cfa36adcc1108e) )
	ROM_LOAD( "dt1cg7.19d", 0xe00000, 0x200000, CRC(2ca19153) SHA1(c82403c8b40bf85daedf610b1bc7bfea9dfc6206) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "dt1ccrl.3d", 0x000000, 0x200000, CRC(e536b313) SHA1(7357da993d2bb3fcc8c1c2feb53689ad368cd80a) )
	ROM_LOAD( "dt1ccrh.1d", 0x200000, 0x080000, CRC(af257064) SHA1(0da561d9f8824618c00209ccef6146e9f3ad72bb) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "dt1ptrl0.18k", 0x000000, 0x080000, CRC(4e0cac3a) SHA1(c2778e9e93be2de729c6f118caf62ac9f48efbb0) )
	ROM_LOAD( "dt1ptrl1.16k", 0x080000, 0x080000, CRC(59ba9dba) SHA1(a2e9488cf0ff255284c06a1ef653ae86c0d98adc) )
	ROM_LOAD( "dt1ptrl2.15k", 0x100000, 0x080000, CRC(cfe80c67) SHA1(ba3bc48aa39712e63c915070a76974fbd560dee6) )
	ROM_LOAD( "dt1ptrm0.18j", 0x180000, 0x080000, CRC(41f34337) SHA1(7e624e7b6fdefe156168b1c9cc5e919db3b2fbaa) )
	ROM_LOAD( "dt1ptrm1.16j", 0x200000, 0x080000, CRC(f620fd41) SHA1(18cf6e11eb68da1b7f7fcc32562dc952c247de65) )
	ROM_LOAD( "dt1ptrm2.15j", 0x280000, 0x080000, CRC(71e6714d) SHA1(6aad6db3be5020213d7add61c7d927ae9c4fea4e) )
	ROM_LOAD( "dt1ptru0.18f", 0x300000, 0x080000, CRC(4909bd7d) SHA1(0e4ef3987c43ef0438331b82b50dcc97363a45d0) )
	ROM_LOAD( "dt1ptru1.16f", 0x380000, 0x080000, CRC(4a5097df) SHA1(a9c814b0ed4bd92accd0e57be8e3d887114b06a5) )
	ROM_LOAD( "dt1ptru2.15f", 0x400000, 0x080000, CRC(1171eaf5) SHA1(168365ea619386f218585c49025cdd7fd1224082) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "dt1wavea.2l", 0x000000, 0x400000, CRC(cbd52e40) SHA1(dc995dd919548c96a90efb0375e5b5f1055e05cb) )
	ROM_LOAD( "dt1waveb.1l", 0x800000, 0x400000, CRC(6b736f94) SHA1(ac3715480aa9a9c2dec099607f89859bb3b73a6a) )
ROM_END

ROM_START( dirtdashj )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_WORD_SWAP( "dt1vera.1", 0x000002, 0x200000, CRC(057b280b) SHA1(e7f038c1b3c7520d32c9962cc00104bcf65d60f6) )
	ROM_LOAD32_WORD_SWAP( "dt1vera.2", 0x000000, 0x200000, CRC(82f822d2) SHA1(170cf326903fe7b4e203b181124de609826b8e1f) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "dt1dataa.8k", 0x000000, 0x080000, CRC(9bcdea21) SHA1(26ae025cf746d3a703a82495eb2bb515b828a650) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "dt1scg0.12f", 0x000000, 0x200000, CRC(a09b5760) SHA1(3dd54ebebf9da1de76874a1adf491ed15849e1b1) )
	ROM_LOAD( "dt1scg1.10f", 0x200000, 0x200000, CRC(f9ac8111) SHA1(814074ae8cc81c6c1201d764a84dd95fe914f19c) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "dt1cg0.8d", 0x000000, 0x200000, CRC(10ab95e0) SHA1(ffde1f00ac3e82a36fbcfa060c6b97c92dfcfc8b) )
	ROM_LOAD( "dt1cg1.10d", 0x200000, 0x200000, CRC(d9f1ba53) SHA1(5a1095b726c55001cc1d4c695adc38097e6a0201) )
	ROM_LOAD( "dt1cg2.12d", 0x400000, 0x200000, CRC(bd8b1e0b) SHA1(fcd94e33a0cbd17c9308cb8952e3c618ab56f9fc) )
	ROM_LOAD( "dt1cg3.13d", 0x600000, 0x200000, CRC(ba960663) SHA1(e98149bc4652ea7933ac47d760a6f7e6489f15e2) )
	ROM_LOAD( "dt1cg4.14d", 0x800000, 0x200000, CRC(424b9652) SHA1(fa8865110db03559740c4e633e123d1a009782c4) )
	ROM_LOAD( "dt1cg5.16d", 0xa00000, 0x200000, CRC(29516626) SHA1(1f12c5dc3975b88dc60d87d0409bf311837e9fa4) )
	ROM_LOAD( "dt1cg6.18d", 0xc00000, 0x200000, CRC(e6fa7180) SHA1(85316cde282cff1f913cf9f155cfa36adcc1108e) )
	ROM_LOAD( "dt1cg7.19d", 0xe00000, 0x200000, CRC(2ca19153) SHA1(c82403c8b40bf85daedf610b1bc7bfea9dfc6206) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "dt1ccrl.3d", 0x000000, 0x200000, CRC(e536b313) SHA1(7357da993d2bb3fcc8c1c2feb53689ad368cd80a) )
	ROM_LOAD( "dt1ccrh.1d", 0x200000, 0x080000, CRC(af257064) SHA1(0da561d9f8824618c00209ccef6146e9f3ad72bb) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "dt1ptrl0.18k", 0x000000, 0x080000, CRC(4e0cac3a) SHA1(c2778e9e93be2de729c6f118caf62ac9f48efbb0) )
	ROM_LOAD( "dt1ptrl1.16k", 0x080000, 0x080000, CRC(59ba9dba) SHA1(a2e9488cf0ff255284c06a1ef653ae86c0d98adc) )
	ROM_LOAD( "dt1ptrl2.15k", 0x100000, 0x080000, CRC(cfe80c67) SHA1(ba3bc48aa39712e63c915070a76974fbd560dee6) )
	ROM_LOAD( "dt1ptrm0.18j", 0x180000, 0x080000, CRC(41f34337) SHA1(7e624e7b6fdefe156168b1c9cc5e919db3b2fbaa) )
	ROM_LOAD( "dt1ptrm1.16j", 0x200000, 0x080000, CRC(f620fd41) SHA1(18cf6e11eb68da1b7f7fcc32562dc952c247de65) )
	ROM_LOAD( "dt1ptrm2.15j", 0x280000, 0x080000, CRC(71e6714d) SHA1(6aad6db3be5020213d7add61c7d927ae9c4fea4e) )
	ROM_LOAD( "dt1ptru0.18f", 0x300000, 0x080000, CRC(4909bd7d) SHA1(0e4ef3987c43ef0438331b82b50dcc97363a45d0) )
	ROM_LOAD( "dt1ptru1.16f", 0x380000, 0x080000, CRC(4a5097df) SHA1(a9c814b0ed4bd92accd0e57be8e3d887114b06a5) )
	ROM_LOAD( "dt1ptru2.15f", 0x400000, 0x080000, CRC(1171eaf5) SHA1(168365ea619386f218585c49025cdd7fd1224082) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "dt1wavea.2l", 0x000000, 0x400000, CRC(cbd52e40) SHA1(dc995dd919548c96a90efb0375e5b5f1055e05cb) )
	ROM_LOAD( "dt1waveb.1l", 0x800000, 0x400000, CRC(6b736f94) SHA1(ac3715480aa9a9c2dec099607f89859bb3b73a6a) )
ROM_END

ROM_START( timecrisj )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ts1verb.1", 0x00003, 0x100000, CRC(17c7ba64) SHA1(bb915000dba1a259953e0cd62143e95407f57499) )
	ROM_LOAD32_BYTE( "ts1verb.2", 0x00002, 0x100000, CRC(77bacf13) SHA1(8d0bab13858e8ea813daeb99f2a9bb6946ac5888) )
	ROM_LOAD32_BYTE( "ts1verb.3", 0x00001, 0x100000, CRC(b68e62f3) SHA1(5cd0543ceeaf4399994efbfc377ff65e4e7baca8) )
	ROM_LOAD32_BYTE( "ts1verb.4", 0x00000, 0x100000, CRC(8ea2331f) SHA1(36fc49b617775b7d8d2867d6ff215962d1b96571) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ts1data.8k", 0, 0x080000, CRC(e68aa973) SHA1(663e80d249be5d5841139d98a9d72e2396851272) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "ts1scg0.12f", 0x000000, 0x200000, CRC(14a3674d) SHA1(c5792a385572452b43bbc7eb8428335b19daa3c0) )
	ROM_LOAD( "ts1scg1.10f", 0x200000, 0x200000, CRC(11791dbf) SHA1(3d75b468d69a8bf398d45f310cdb8bc88b63f25c) )
	ROM_LOAD( "ts1scg2.8f", 0x400000, 0x200000, CRC(d630fff9) SHA1(691394027b858702f06282f965f5b53e6fed496b) )
	ROM_LOAD( "ts1scg3.7f", 0x600000, 0x200000, CRC(1a62f015) SHA1(7d09ae480ae7813391616ae0090929ba845a345a) )
	ROM_LOAD( "ts1scg4.5f", 0x800000, 0x200000, CRC(511b8dd6) SHA1(936649c0a61d29f024a28e4ab64cce4b55d58f64) )
	ROM_LOAD( "ts1scg5.3f", 0xa00000, 0x200000, CRC(553bb246) SHA1(94659bee4fd0afe834a8bf3414d8825411cf9e86) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ts1cg0.8d", 0x000000, 0x200000, CRC(de07b22c) SHA1(f4d07b8840ec8be625eff634bce619e960c334a5) )
	ROM_LOAD( "ts1cg1.10d", 0x200000, 0x200000, CRC(992d26f6) SHA1(a0b9007312804b413d4c1748527378da4d8d53b3) )
	ROM_LOAD( "ts1cg2.12d", 0x400000, 0x200000, CRC(6273954f) SHA1(d73a43888b53e4c42fc33e8e1b38e60fd3329413) )
	ROM_LOAD( "ts1cg3.13d", 0x600000, 0x200000, CRC(38171f24) SHA1(d04caaa5b1b377ced38501b014e4cb7fc831c41d) )
	ROM_LOAD( "ts1cg4.14d", 0x800000, 0x200000, CRC(51f09856) SHA1(0eef421907ee813d5117e62cf0005bf00eb29c88) )
	ROM_LOAD( "ts1cg5.16d", 0xa00000, 0x200000, CRC(4cd9fd79) SHA1(0d2018ec914683a75bdec8655d678fd562eb6d15) )
	ROM_LOAD( "ts1cg6.18d", 0xc00000, 0x200000, CRC(f17f2ec9) SHA1(ed88ec524626e5bbe2e1ea6838412d3ac85671dd) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ts1ccrl.3d", 0x000000, 0x200000, CRC(56cad2df) SHA1(49c0e57d5cf5d5fc4c75da6969bec01d6d443259) )
	ROM_LOAD( "ts1ccrh.1d", 0x200000, 0x080000, CRC(a1cc3741) SHA1(7fe57924c42e287b134e5d7ad00cffdff1f18084) )
	ROM_REGION( 0x480000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ts1ptrl0.18k", 0x000000, 0x080000, CRC(e5f2d275) SHA1(2f5057e65ec8a3ec03f841f15f10769ae1f69139) )
	ROM_LOAD( "ts1ptrl1.16k", 0x080000, 0x080000, CRC(2bba3800) SHA1(1d9c944cb06417cb0ac47a58b922dddb83387586) )
	ROM_LOAD( "ts1ptrl2.15k", 0x100000, 0x080000, CRC(d4441c08) SHA1(6a6bb9cecbf35cb81b7681e220fc33df9a01d07f) )
	ROM_LOAD( "ts1ptrm0.18j", 0x180000, 0x080000, CRC(8aea02ba) SHA1(44ba85ba6d59758448d17ec39dfb628882ddc684) )
	ROM_LOAD( "ts1ptrm1.16j", 0x200000, 0x080000, CRC(bccf19bc) SHA1(4a6566948bdd2b0f82b7c30e57d3fe65005c26e3) )
	ROM_LOAD( "ts1ptrm2.15j", 0x280000, 0x080000, CRC(7280be31) SHA1(476b7171ae855d8bbd968ccbaa55b5100d274e3b) )
	ROM_LOAD( "ts1ptru0.18f", 0x300000, 0x080000, CRC(c30d6332) SHA1(a5c59d0abfe38de975fa0d606ed8500eb02008b7) )
	ROM_LOAD( "ts1ptru1.16f", 0x380000, 0x080000, CRC(993cde84) SHA1(c9cdcca1d60bcc41ad881c02dda9895563963ead) )
	ROM_LOAD( "ts1ptru2.15f", 0x400000, 0x080000, CRC(7cb25c73) SHA1(616eab3ac238864a584394f7ec8736ece227974a) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD16_WORD_SWAP( "ts1wavea.2l", 0x000000, 0x400000, CRC(d1123301) SHA1(4bf1fd746fef4e6befa63c61a761971d729e1573) )
	ROM_LOAD( "ts1waveb.1l", 0x800000, 0x200000, CRC(bf4d7272) SHA1(c7c7b3620e7b3176644b6784ee36e679c9e31cc1) )
ROM_END

ROM_START( alpines )
	ROM_REGION( 0x800000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "af2ver-a_ll.ic2", 0x000003, 0x200000, CRC(e776159d) SHA1(5110364afb7ec606074d58a1d216d7d687b9df62) )
	ROM_LOAD32_BYTE( "af2ver-a_lm.ic3", 0x000002, 0x200000, CRC(c5333d38) SHA1(9486cead964f95f8e56dac2f88486f3b98561aa6) )
	ROM_LOAD32_BYTE( "af2ver-a_um.ic4", 0x000001, 0x200000, CRC(c9095af3) SHA1(4981c8c84057373ef678e51fe72ebac40d1a0cf4) )
	ROM_LOAD32_BYTE( "af2ver-a_uu.ic5", 0x000000, 0x200000, CRC(54ee33a1) SHA1(0eaa8707ab13a0a66551f61a08986c98f5c9e446) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "af1data.8k", 0x000000, 0x080000, CRC(ef13ebe8) SHA1(5d3f697994d4b5b19ee7fea1e2aef8e39449b68e) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "af1scg0b.12f", 0x000000, 0x200000, CRC(46a6222a) SHA1(5322ef60690625b9b8dbe1cfe0c49dcd9c8b1a4c) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "af1cg0.8d", 0x000000, 0x200000, CRC(7423f3ff) SHA1(6a2fd44823ef46111deb57d328b1b75cc355d413) )
	ROM_LOAD( "af1cg1.10d", 0x200000, 0x200000, CRC(ea76689a) SHA1(73dd3af737a3e9903abe5ed9c9ae7eded51d8350) )
	ROM_LOAD( "af1cg2.12d", 0x400000, 0x200000, CRC(2a38943a) SHA1(15d737996f49bf6374ef6191bbfbe0298d398378) )
	ROM_LOAD( "af1cg3.13d", 0x600000, 0x200000, CRC(7f5a3e0f) SHA1(241f9995323b28df23d20a75e1f43ce6e05434cd) )
	ROM_LOAD( "af1cg4.14d", 0x800000, 0x200000, CRC(a5ee13e2) SHA1(48fd3c912690f21cbbc2a39bed0a82be41a0d011) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "af1ccrl.3d", 0x000000, 0x200000, CRC(6c054698) SHA1(8537607646b183883c5aa4060fb0af640da4af87) )
	ROM_LOAD( "af1ccrh.1d", 0x200000, 0x080000, CRC(95a02a27) SHA1(32ee87b76ae9fcec6d825e3cf4d5cbb97db39544) )
	ROM_REGION( 0x300000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "af1ptrl0.18k", 0x000000, 0x080000, CRC(31ce46d3) SHA1(568fb9ee9ac14e613a4fd7668cb38315c10be62b) )
	ROM_LOAD( "af1ptrl1.16k", 0x080000, 0x080000, CRC(e869bf00) SHA1(b3c3026891ae3958d1774c905e97c3b57a414ea7) )
	ROM_LOAD( "af1ptrm0.18j", 0x100000, 0x080000, CRC(ef7f4d8a) SHA1(02f77c68004b7dccc99b61126e7d07960eb15028) )
	ROM_LOAD( "af1ptrm1.16j", 0x180000, 0x080000, CRC(7dd01d52) SHA1(adc1087435d31ed6163ad046466955f01517450f) )
	ROM_LOAD( "af1ptru0.18f", 0x200000, 0x080000, CRC(177f1591) SHA1(3969e780e5603eca0a65f65c1ad14d1cef918b39) )
	ROM_LOAD( "af1ptru1.16f", 0x280000, 0x080000, CRC(7521d18e) SHA1(dc03ef369db16f59c138ff4e22260d1c04782d1f) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "af1wavea.2l", 0x000000, 0x400000, CRC(28cca494) SHA1(4ff87ab85fd17bf8dbee5b03d99cc5c31dd6349a) )
ROM_END

ROM_START( tokyowar )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "tw2vera.1", 0x000003, 0x100000, CRC(2b17ca92) SHA1(7bcb1658059c59fb1a0131a41ede7157855130a8) )
	ROM_LOAD32_BYTE( "tw2vera.2", 0x000002, 0x100000, CRC(12da84e3) SHA1(a9406d0b77f60ba930c30e60bf4b3656c8905585) )
	ROM_LOAD32_BYTE( "tw2vera.3", 0x000001, 0x100000, CRC(7d42c516) SHA1(28c1596dd55c15207bbb41a8b9a5abc97abc2bc8) )
	ROM_LOAD32_BYTE( "tw2vera.4", 0x000000, 0x100000, CRC(b904ed16) SHA1(773e11536e1b3fe4971608a63a8e6eca702f8667) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "tw1data.8k", 0x000000, 0x080000, CRC(bd046e4b) SHA1(162bc4ab69959ccab49fd69de291d34d472fb1c8) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "tw1scg0.12f", 0x000000, 0x200000, CRC(e3ec4daa) SHA1(f3a71ae9820d62075b814ffa2fecf3343ae09ffe) )
	ROM_LOAD( "tw1scg1.10f", 0x200000, 0x200000, CRC(b18a06e9) SHA1(ecf6a1e11603b8ea5119a036a57595dea021d778) )
	ROM_LOAD( "tw1scg2.8f", 0x400000, 0x200000, CRC(36f8c3d8) SHA1(d5c965d5cdd258c77b9db3137ce33404a5a3641c) )
	ROM_LOAD( "tw1scg3.7f", 0x600000, 0x200000, CRC(8e14d013) SHA1(ca63105a5c07bb9653499eef7a757db52612b59b) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "tw1cg0.8d", 0x000000, 0x200000, CRC(98b9b070) SHA1(cb920a34700dab330c967cc634717134c04b7e1d) )
	ROM_LOAD( "tw1cg1.10d", 0x200000, 0x200000, CRC(f96a723a) SHA1(5ba14963a4c51c875ac8d3b42049bc334de90038) )
	ROM_LOAD( "tw1cg2.12d", 0x400000, 0x200000, CRC(573e9ded) SHA1(815bda1ac000532c915c2d65ffdb04fee6fa8201) )
	ROM_LOAD( "tw1cg3.13d", 0x600000, 0x200000, CRC(302d5c74) SHA1(5a823f6842cf0f79eb93da47d5bf8c5f51e420db) )
	ROM_LOAD( "tw1cg4.14d", 0x800000, 0x200000, CRC(ab8aa1df) SHA1(355192d999f493e0761fbc822fa9b30c33d8e1c4) )
	ROM_LOAD( "tw1cg5.16d", 0xa00000, 0x200000, CRC(5063f3d0) SHA1(ad8dd2f4184373a3a3ca748b411a5eec1835dc97) )
	ROM_LOAD( "tw1cg6.18d", 0xc00000, 0x200000, CRC(d764027c) SHA1(5cbf93392683885c220628936ba50c09cb40fcfb) )
	ROM_LOAD( "tw1cg7.19d", 0xe00000, 0x200000, CRC(12da0a43) SHA1(99287348a5219146bcd3e61fde3f2bc26b0c5a25) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "tw1ccrl.3d", 0x000000, 0x200000, CRC(d08f5794) SHA1(336a97a2b060505e259e3bcedb9eb8aa4ea8815e) )
	ROM_LOAD( "tw1ccrh.1d", 0x200000, 0x080000, CRC(ad17e693) SHA1(4f06dc82c03159894fb8e10383862920f94563b1) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "tw1ptrl0.18k", 0x000000, 0x080000, CRC(44ac5e86) SHA1(5e42db58f2e352c0fe5e49588a9283817dd15ab1) )
	ROM_LOAD( "tw1ptrl1.16k", 0x080000, 0x080000, CRC(3c769860) SHA1(19b32c3d262f2a9d07354fbd6ac6be97b05f176e) )
	ROM_LOAD( "tw1ptrl2.15k", 0x100000, 0x080000, CRC(6e94103c) SHA1(ee90b77939a9f5780ce271882133cdf977eb643e) )
	ROM_LOAD( "tw1ptrl3.14k", 0x180000, 0x080000, CRC(e3ce5eb2) SHA1(4e15a6f630be15eb017cd51c5e7901db3138a061) )
	ROM_LOAD( "tw1ptrm0.18j", 0x200000, 0x080000, CRC(e170cea2) SHA1(ca259508d76fdab97a9d0502f871f0e560e6c308) )
	ROM_LOAD( "tw1ptrm1.16j", 0x280000, 0x080000, CRC(36a32237) SHA1(f46851dc5c094810ddc42d56310a9f85908bf715) )
	ROM_LOAD( "tw1ptrm2.15j", 0x300000, 0x080000, CRC(c426c278) SHA1(64232ac3c1649e0d0adb4b03e58b8b5ea4013f83) )
	ROM_LOAD( "tw1ptrm3.14j", 0x380000, 0x080000, CRC(d9b9a651) SHA1(0c49a051526081149d894d629f19cb0f2b66a698) )
	ROM_LOAD( "tw1ptru0.18f", 0x400000, 0x080000, CRC(62a9e9fb) SHA1(24739adba029b0acf2d7078962c9d01098a29a6c) )
	ROM_LOAD( "tw1ptru1.16f", 0x480000, 0x080000, CRC(2fd36177) SHA1(368b915261d914be01ae9daeb52571bead52d14d) )
	ROM_LOAD( "tw1ptru2.15f", 0x500000, 0x080000, CRC(ceacb1c9) SHA1(b86cf576e16bbe26ad0d6d6df8bf28d0071c25e2) )
	ROM_LOAD( "tw1ptru3.14f", 0x580000, 0x080000, CRC(939044c2) SHA1(f4c1c0a1c2f07ca7f784d59ef4162a2a6a8bbc43) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "tw1wavea.2l", 0x000000, 0x400000, CRC(ebce6366) SHA1(44ebe90ff3c7af5bebbf1baba3b7a2b1863daebb) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "tokyowar_defaults.nv", 0x0000, 0x2000, CRC(e8bd7d09) SHA1(7e59017b9d5eb78984b4f177b50a4727ad72a623) )
ROM_END

ROM_START( tokyowarj )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "tw1vera.1", 0x000003, 0x100000, CRC(29b38f05) SHA1(d55a34dce5f61d72e3edc331d3a346397069a5fc) )
	ROM_LOAD32_BYTE( "tw1vera.2", 0x000002, 0x100000, CRC(5f8e2e9e) SHA1(14513480fed483e5381ac5bf4467a1c17b0d9e75) )
	ROM_LOAD32_BYTE( "tw1vera.3", 0x000001, 0x100000, CRC(17146e7f) SHA1(0fd270152eef1966e0960531554cbe115668205f) )
	ROM_LOAD32_BYTE( "tw1vera.4", 0x000000, 0x100000, CRC(12698e2a) SHA1(51761ea96cb458f56d832cd233afbc374ded7f20) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "tw1data.8k", 0x000000, 0x080000, CRC(bd046e4b) SHA1(162bc4ab69959ccab49fd69de291d34d472fb1c8) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "tw1scg0.12f", 0x000000, 0x200000, CRC(e3ec4daa) SHA1(f3a71ae9820d62075b814ffa2fecf3343ae09ffe) )
	ROM_LOAD( "tw1scg1.10f", 0x200000, 0x200000, CRC(b18a06e9) SHA1(ecf6a1e11603b8ea5119a036a57595dea021d778) )
	ROM_LOAD( "tw1scg2.8f", 0x400000, 0x200000, CRC(36f8c3d8) SHA1(d5c965d5cdd258c77b9db3137ce33404a5a3641c) )
	ROM_LOAD( "tw1scg3.7f", 0x600000, 0x200000, CRC(8e14d013) SHA1(ca63105a5c07bb9653499eef7a757db52612b59b) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "tw1cg0.8d", 0x000000, 0x200000, CRC(98b9b070) SHA1(cb920a34700dab330c967cc634717134c04b7e1d) )
	ROM_LOAD( "tw1cg1.10d", 0x200000, 0x200000, CRC(f96a723a) SHA1(5ba14963a4c51c875ac8d3b42049bc334de90038) )
	ROM_LOAD( "tw1cg2.12d", 0x400000, 0x200000, CRC(573e9ded) SHA1(815bda1ac000532c915c2d65ffdb04fee6fa8201) )
	ROM_LOAD( "tw1cg3.13d", 0x600000, 0x200000, CRC(302d5c74) SHA1(5a823f6842cf0f79eb93da47d5bf8c5f51e420db) )
	ROM_LOAD( "tw1cg4.14d", 0x800000, 0x200000, CRC(ab8aa1df) SHA1(355192d999f493e0761fbc822fa9b30c33d8e1c4) )
	ROM_LOAD( "tw1cg5.16d", 0xa00000, 0x200000, CRC(5063f3d0) SHA1(ad8dd2f4184373a3a3ca748b411a5eec1835dc97) )
	ROM_LOAD( "tw1cg6.18d", 0xc00000, 0x200000, CRC(d764027c) SHA1(5cbf93392683885c220628936ba50c09cb40fcfb) )
	ROM_LOAD( "tw1cg7.19d", 0xe00000, 0x200000, CRC(12da0a43) SHA1(99287348a5219146bcd3e61fde3f2bc26b0c5a25) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "tw1ccrl.3d", 0x000000, 0x200000, CRC(d08f5794) SHA1(336a97a2b060505e259e3bcedb9eb8aa4ea8815e) )
	ROM_LOAD( "tw1ccrh.1d", 0x200000, 0x080000, CRC(ad17e693) SHA1(4f06dc82c03159894fb8e10383862920f94563b1) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "tw1ptrl0.18k", 0x000000, 0x080000, CRC(44ac5e86) SHA1(5e42db58f2e352c0fe5e49588a9283817dd15ab1) )
	ROM_LOAD( "tw1ptrl1.16k", 0x080000, 0x080000, CRC(3c769860) SHA1(19b32c3d262f2a9d07354fbd6ac6be97b05f176e) )
	ROM_LOAD( "tw1ptrl2.15k", 0x100000, 0x080000, CRC(6e94103c) SHA1(ee90b77939a9f5780ce271882133cdf977eb643e) )
	ROM_LOAD( "tw1ptrl3.14k", 0x180000, 0x080000, CRC(e3ce5eb2) SHA1(4e15a6f630be15eb017cd51c5e7901db3138a061) )
	ROM_LOAD( "tw1ptrm0.18j", 0x200000, 0x080000, CRC(e170cea2) SHA1(ca259508d76fdab97a9d0502f871f0e560e6c308) )
	ROM_LOAD( "tw1ptrm1.16j", 0x280000, 0x080000, CRC(36a32237) SHA1(f46851dc5c094810ddc42d56310a9f85908bf715) )
	ROM_LOAD( "tw1ptrm2.15j", 0x300000, 0x080000, CRC(c426c278) SHA1(64232ac3c1649e0d0adb4b03e58b8b5ea4013f83) )
	ROM_LOAD( "tw1ptrm3.14j", 0x380000, 0x080000, CRC(d9b9a651) SHA1(0c49a051526081149d894d629f19cb0f2b66a698) )
	ROM_LOAD( "tw1ptru0.18f", 0x400000, 0x080000, CRC(62a9e9fb) SHA1(24739adba029b0acf2d7078962c9d01098a29a6c) )
	ROM_LOAD( "tw1ptru1.16f", 0x480000, 0x080000, CRC(2fd36177) SHA1(368b915261d914be01ae9daeb52571bead52d14d) )
	ROM_LOAD( "tw1ptru2.15f", 0x500000, 0x080000, CRC(ceacb1c9) SHA1(b86cf576e16bbe26ad0d6d6df8bf28d0071c25e2) )
	ROM_LOAD( "tw1ptru3.14f", 0x580000, 0x080000, CRC(939044c2) SHA1(f4c1c0a1c2f07ca7f784d59ef4162a2a6a8bbc43) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "tw1wavea.2l", 0x000000, 0x400000, CRC(ebce6366) SHA1(44ebe90ff3c7af5bebbf1baba3b7a2b1863daebb) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "tokyowar_defaults.nv", 0x0000, 0x2000, CRC(e8bd7d09) SHA1(7e59017b9d5eb78984b4f177b50a4727ad72a623) )
ROM_END

ROM_START( aquajet )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "aj2verb.1", 0x000003, 0x100000, CRC(3a67b9f4) SHA1(8cd51f319e082297fdb99634486fe297a0ace654) )
	ROM_LOAD32_BYTE( "aj2verb.2", 0x000002, 0x100000, CRC(f5e8fc96) SHA1(e23fcf6f84724d1de15870ff578ff8a6b26e8f31) )
	ROM_LOAD32_BYTE( "aj2verb.3", 0x000001, 0x100000, CRC(ef6ebcf7) SHA1(358973b678b9a3065e945fb589af16e8102d437b) )
	ROM_LOAD32_BYTE( "aj2verb.4", 0x000000, 0x100000, CRC(7799b909) SHA1(e40005f96f51742b2778605926b8184c9b2c1ad2) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "aj1data.8k", 0x000000, 0x080000, CRC(52bcc6d5) SHA1(25319ea6db35cc9bdcb39cc83d597a2a9f1690f3) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "aj1scg0.12f", 0x000000, 0x200000, CRC(13ea766c) SHA1(a1a259bd8f468b90cbc891f1c2875fe03bba9802) )
	ROM_LOAD( "aj1scg1.10f", 0x200000, 0x200000, CRC(cb3638de) SHA1(0af99aaf00782036d7f479b00b0c3d9d7ad4fc37) )
	ROM_LOAD( "aj1scg2.8f", 0x400000, 0x200000, CRC(1048a09b) SHA1(6859533e24db5ac54e28d480aaac7b411a648dfe) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "aj1cg0.8d", 0x000000, 0x200000, CRC(b814e1eb) SHA1(1af29897dcfd0a58743b4fcdd5049d9e6e3d4cbd) )
	ROM_LOAD( "aj1cg1.10d", 0x200000, 0x200000, CRC(dc63d496) SHA1(d3c3eea6b134850004062dd513c0f26096101227) )
	ROM_LOAD( "aj1cg2.12d", 0x400000, 0x200000, CRC(71fbb571) SHA1(40f84ab5a4a7bb4438fb53751762609671d17800) )
	ROM_LOAD( "aj1cg3.13d", 0x600000, 0x200000, CRC(e28052e2) SHA1(3dbc446d7a13312ed4c1d20c2e1209947853f2c4) )
	ROM_LOAD( "aj1cg4.14d", 0x800000, 0x200000, CRC(c77ae1a0) SHA1(e15f2ccbadb8634f6e5cfd2cf48f428d68bd92e1) )
	ROM_LOAD( "aj1cg5.16d", 0xa00000, 0x200000, CRC(15be0080) SHA1(ca14dfd2a66996f0b32fa1155c04f21d300e8f30) )
	ROM_LOAD( "aj1cg6.18d", 0xc00000, 0x200000, CRC(1a4f733a) SHA1(60a991f06e73667fa2c9016189999c4301cba24f) )
	ROM_LOAD( "aj1cg7.19d", 0xe00000, 0x200000, CRC(ea118130) SHA1(24ef22e5c8c6f6e8a01c72466be1e7acbfba63bc) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "aj1ccrl.3d", 0x000000, 0x200000, CRC(3cc7a247) SHA1(336e4dd506d932987e20a5890dd3b0db75c02ccf) )
	ROM_LOAD( "aj1ccrh.1d", 0x200000, 0x080000, CRC(9d936030) SHA1(a383bcca494a9f8d9a08fbe9940c8071d4525d65) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "aj1ptrl0.18k", 0x000000, 0x080000, CRC(16205d45) SHA1(7e9681af852e2e875f80cbbe350e4982eaf80884) )
	ROM_LOAD( "aj1ptrl1.16k", 0x080000, 0x080000, CRC(6f114da7) SHA1(a1e7349ef8eb0ef043fce06f88bfbb76c2967393) )
	ROM_LOAD( "aj1ptrl2.15k", 0x100000, 0x080000, CRC(719b73f0) SHA1(bf8530a706097ae663769090177f1f259c540aa6) )
	ROM_LOAD( "aj1ptrl3.14k", 0x180000, 0x080000, CRC(9555fe31) SHA1(03bdd0784817191b3600d643b775191181a21706) )
	ROM_LOAD( "aj1ptrm0.18j", 0x200000, 0x080000, CRC(89bee2e0) SHA1(d84b06ef1318814fcf4782fef09a85ae1a87beb8) )
	ROM_LOAD( "aj1ptrm1.16j", 0x280000, 0x080000, CRC(0ecf88c7) SHA1(20b7cb09a6a13599cac024ef19c73612ce2952f6) )
	ROM_LOAD( "aj1ptrm2.15j", 0x300000, 0x080000, CRC(829bc7ba) SHA1(7104d66a027911e38ecf521f01ff098ccb76d5fb) )
	ROM_LOAD( "aj1ptrm3.14j", 0x380000, 0x080000, CRC(7d0a222a) SHA1(e78c405d00429580015c6c7d1bcd35393317a769) )
	ROM_LOAD( "aj1ptru0.18f", 0x400000, 0x080000, CRC(90d4e36a) SHA1(bdc44aac6aef5266d289f03c816aa2abdb263e9b) )
	ROM_LOAD( "aj1ptru1.16f", 0x480000, 0x080000, CRC(bf0cf4bf) SHA1(bbf06c7605c083d2fb8c72528c5c9d3b8b067073) )
	ROM_LOAD( "aj1ptru2.15f", 0x500000, 0x080000, CRC(91ffbb77) SHA1(fbfe7d32ef22037f7190d1b8263b5e8c55f2d892) )
	ROM_LOAD( "aj1ptru3.14f", 0x580000, 0x080000, CRC(d83d8d42) SHA1(e1561ce4538b01db92b7e645ad008cd1a2ddaf8a) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "aj1wavea.2l", 0x000000, 0x400000, CRC(8c72ea59) SHA1(3ae8dbd8baae08f1daab2b218932ba9d9451231d) )
	ROM_LOAD( "aj1waveb.1l", 0x800000, 0x400000, CRC(ab5a457f) SHA1(c34531fd574eb0c3e78fc31a9af8658df3446adc) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "aquajet_defaults.nv", 0x0000, 0x2000, CRC(a00b3e44) SHA1(6bdbb46f4176314b61bd5063ecc968189212cb4c) )
ROM_END

ROM_START( alpinr2b )
	ROM_REGION( 0x800000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ars2verb.2", 0x000003, 0x200000, CRC(ed977f83) SHA1(26c57cdfc15f799a999ee22f141e1c0cabfc91dc) )
	ROM_LOAD32_BYTE( "ars2verb.3", 0x000001, 0x200000, CRC(8e7a9983) SHA1(34c82e5f080efe04d6b77a77a8391cb48b69c1af) )
	ROM_LOAD32_BYTE( "ars2verb.4", 0x000002, 0x200000, CRC(610e49c2) SHA1(433c6d2216551bac31584306f748af1c912c3b07) )
	ROM_LOAD32_BYTE( "ars2verb.5", 0x000000, 0x200000, CRC(7f3517b0) SHA1(3e6ba1a51bf235f40f933aae1f00638b88bba522) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ars2data.8k", 0x000000, 0x080000, CRC(29b36dcb) SHA1(70fde130c11789c822829493a70ecefb077c0c15) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "ars1scg0.12f", 0x000000, 0x200000, CRC(bc49ed86) SHA1(289b39f2cb21c723dbe4ddd64ee4b2c5fa65c368) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ars1cg0.8d", 0x000000, 0x200000, CRC(74f4515c) SHA1(1e3a96281d543213d10c962b4d387c414d76e0c3) )
	ROM_LOAD( "ars1cg1.10d", 0x200000, 0x200000, CRC(329a95c1) SHA1(2cad7fd9e5ca7c64729ca2548ef4f873a0b8de64) )
	ROM_LOAD( "ars1cg2.12d", 0x400000, 0x200000, CRC(5648345a) SHA1(0dc7aedba65b7d97687a9e38a63597f16cee6179) )
	ROM_LOAD( "ars1cg3.13d", 0x600000, 0x200000, CRC(a752f205) SHA1(373b5a69e4488bc30763568ceae512ab7039f5f8) )
	ROM_LOAD( "ars1cg4.14d", 0x800000, 0x200000, CRC(54bf35b6) SHA1(aec43b66e7597ad7d113ae785417bf26164c1bca) )
	ROM_LOAD( "ars1cg5.16d", 0xa00000, 0x200000, CRC(e24a19a2) SHA1(34c1b51eea954ae3000602e550eb1cef0a10e651) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ars1ccrl.3d", 0x000000, 0x200000, CRC(fc8c9161) SHA1(ad0fcfae27e02d68a6f8d1d03c514dc2f12d9ee8) )
	ROM_LOAD( "ars1ccrh.1d", 0x200000, 0x080000, CRC(a17660bb) SHA1(bae2c3f20772c6cea99f271ee3f39b1f999038c6) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ars1ptrl0.18k", 0x000000, 0x080000, CRC(f04e0e61) SHA1(d58a1d3ca1d0922e134db21a04feaee7dc97b020) )
	ROM_LOAD( "ars1ptrl1.16k", 0x080000, 0x080000, CRC(8bdb4970) SHA1(a504cd4beca4bedd1a7a228c83dd6b844ca3a1e0) )
	ROM_LOAD( "ars1ptrl2.15k", 0x100000, 0x080000, CRC(ec993a4f) SHA1(883f64e3e8d951415e9cef589c354eba9406c0aa) )
	ROM_LOAD( "ars1ptrl3.14k", 0x180000, 0x080000, CRC(4d453f3c) SHA1(7a82e5d8f974d9e56d0031b35e73647fe6aeec2e) )
	ROM_LOAD( "ars1ptrm0.18j", 0x200000, 0x080000, CRC(d1bdc524) SHA1(b898bb38de397551ada4da4677dd733bf8fa5010) )
	ROM_LOAD( "ars1ptrm1.16j", 0x280000, 0x080000, CRC(86b81c81) SHA1(45096abf46794f06a4b647f5e4222798d8467632) )
	ROM_LOAD( "ars1ptrm2.15j", 0x300000, 0x080000, CRC(24116b83) SHA1(41c6a880abce7b543c409fda767682b2537b0d99) )
	ROM_LOAD( "ars1ptrm3.14j", 0x380000, 0x080000, CRC(772bede3) SHA1(f9565b7a40f0bbf11081d619fe5a46feafce2e56) )
	ROM_LOAD( "ars1ptru0.18f", 0x400000, 0x080000, CRC(a4cf197a) SHA1(ee78cc259e87395df75179bbe5b6e521e762b582) )
	ROM_LOAD( "ars1ptru1.16f", 0x480000, 0x080000, CRC(1deb1fc0) SHA1(bfd1dfcaccf5a0f851b6757995fa7195452a3965) )
	ROM_LOAD( "ars1ptru2.15f", 0x500000, 0x080000, CRC(bcfad0ba) SHA1(bee7f2f9ecd2b289c6706e19fa86863913b286b5) )
	ROM_LOAD( "ars1ptru3.14f", 0x580000, 0x080000, CRC(73ce6958) SHA1(918b0fb0fca33dbe3be3ac679b8b28f58213f75b) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "ars1wavea.2l", 0x000000, 0x400000, CRC(f8d107e9) SHA1(5c418691f0b35403553f21f5570eda8bbb66890f) )
	ROM_LOAD( "ars2waveb.1l", 0x800000, 0x400000, CRC(deab4ad1) SHA1(580ad88d516280baaf6cc92b2e07cdc0cfc486f3) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "alpiner2_defaults.nv", 0x0000, 0x2000, CRC(1f21154e) SHA1(a141d7d235955d042c60d013a89619d35c02308f) )
ROM_END

ROM_START( alpinr2a )
	ROM_REGION( 0x800000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "ars2vera.2", 0x000003, 0x200000, CRC(b07b15a4) SHA1(ea3b2d7b4ef4ccf3aafeef7e7eac92e8d446f4e7) )
	ROM_LOAD32_BYTE( "ars2vera.3", 0x000002, 0x200000, CRC(90a92e40) SHA1(bf8083256e56e7e33e61b4cdaf9fd03dabfb36ba) )
	ROM_LOAD32_BYTE( "ars2vera.4", 0x000001, 0x200000, CRC(9e9d771d) SHA1(6fb983e3f4f8233544667b1bbf87864e4fb8698c) )
	ROM_LOAD32_BYTE( "ars2vera.5", 0x000000, 0x200000, CRC(e93c7771) SHA1(305f35488a55be1b845702df972bba8334c0726c) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "ars2data.8k", 0x000000, 0x080000, CRC(29b36dcb) SHA1(70fde130c11789c822829493a70ecefb077c0c15) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "ars1scg0.12f", 0x000000, 0x200000, CRC(bc49ed86) SHA1(289b39f2cb21c723dbe4ddd64ee4b2c5fa65c368) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "ars1cg0.8d", 0x000000, 0x200000, CRC(74f4515c) SHA1(1e3a96281d543213d10c962b4d387c414d76e0c3) )
	ROM_LOAD( "ars1cg1.10d", 0x200000, 0x200000, CRC(329a95c1) SHA1(2cad7fd9e5ca7c64729ca2548ef4f873a0b8de64) )
	ROM_LOAD( "ars1cg2.12d", 0x400000, 0x200000, CRC(5648345a) SHA1(0dc7aedba65b7d97687a9e38a63597f16cee6179) )
	ROM_LOAD( "ars1cg3.13d", 0x600000, 0x200000, CRC(a752f205) SHA1(373b5a69e4488bc30763568ceae512ab7039f5f8) )
	ROM_LOAD( "ars1cg4.14d", 0x800000, 0x200000, CRC(54bf35b6) SHA1(aec43b66e7597ad7d113ae785417bf26164c1bca) )
	ROM_LOAD( "ars1cg5.16d", 0xa00000, 0x200000, CRC(e24a19a2) SHA1(34c1b51eea954ae3000602e550eb1cef0a10e651) )
	ROM_REGION( 0x280000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "ars1ccrl.3d", 0x000000, 0x200000, CRC(fc8c9161) SHA1(ad0fcfae27e02d68a6f8d1d03c514dc2f12d9ee8) )
	ROM_LOAD( "ars1ccrh.1d", 0x200000, 0x080000, CRC(a17660bb) SHA1(bae2c3f20772c6cea99f271ee3f39b1f999038c6) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "ars1ptrl0.18k", 0x000000, 0x080000, CRC(f04e0e61) SHA1(d58a1d3ca1d0922e134db21a04feaee7dc97b020) )
	ROM_LOAD( "ars1ptrl1.16k", 0x080000, 0x080000, CRC(8bdb4970) SHA1(a504cd4beca4bedd1a7a228c83dd6b844ca3a1e0) )
	ROM_LOAD( "ars1ptrl2.15k", 0x100000, 0x080000, CRC(ec993a4f) SHA1(883f64e3e8d951415e9cef589c354eba9406c0aa) )
	ROM_LOAD( "ars1ptrl3.14k", 0x180000, 0x080000, CRC(4d453f3c) SHA1(7a82e5d8f974d9e56d0031b35e73647fe6aeec2e) )
	ROM_LOAD( "ars1ptrm0.18j", 0x200000, 0x080000, CRC(d1bdc524) SHA1(b898bb38de397551ada4da4677dd733bf8fa5010) )
	ROM_LOAD( "ars1ptrm1.16j", 0x280000, 0x080000, CRC(86b81c81) SHA1(45096abf46794f06a4b647f5e4222798d8467632) )
	ROM_LOAD( "ars1ptrm2.15j", 0x300000, 0x080000, CRC(24116b83) SHA1(41c6a880abce7b543c409fda767682b2537b0d99) )
	ROM_LOAD( "ars1ptrm3.14j", 0x380000, 0x080000, CRC(772bede3) SHA1(f9565b7a40f0bbf11081d619fe5a46feafce2e56) )
	ROM_LOAD( "ars1ptru0.18f", 0x400000, 0x080000, CRC(a4cf197a) SHA1(ee78cc259e87395df75179bbe5b6e521e762b582) )
	ROM_LOAD( "ars1ptru1.16f", 0x480000, 0x080000, CRC(1deb1fc0) SHA1(bfd1dfcaccf5a0f851b6757995fa7195452a3965) )
	ROM_LOAD( "ars1ptru2.15f", 0x500000, 0x080000, CRC(bcfad0ba) SHA1(bee7f2f9ecd2b289c6706e19fa86863913b286b5) )
	ROM_LOAD( "ars1ptru3.14f", 0x580000, 0x080000, CRC(73ce6958) SHA1(918b0fb0fca33dbe3be3ac679b8b28f58213f75b) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD( "ars1wavea.2l", 0x000000, 0x400000, CRC(f8d107e9) SHA1(5c418691f0b35403553f21f5570eda8bbb66890f) )
	ROM_LOAD( "ars2waveb.1l", 0x800000, 0x400000, CRC(deab4ad1) SHA1(580ad88d516280baaf6cc92b2e07cdc0cfc486f3) )
	ROM_REGION( 0x2000, REGION_USER2, 0 ) /* default EEPROM */
	ROM_LOAD( "alpiner2_defaults.nv", 0x0000, 0x2000, CRC(1f21154e) SHA1(a141d7d235955d042c60d013a89619d35c02308f) )
ROM_END

ROM_START( adillor )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "am2vera.rom1", 0x000003, 0x100000, CRC(b974e931) SHA1(db1c99ed56044f667a9a0a5801f62215004af847) )
	ROM_LOAD32_BYTE( "am2vera.rom2", 0x000002, 0x100000, CRC(c7cd78f1) SHA1(2f909be1c9a9acf42a8e446c83a06b3b36012196) )
	ROM_LOAD32_BYTE( "am2vera.rom3", 0x000001, 0x100000, CRC(80e9435a) SHA1(d92f6ec607e628270ceb3e09c1980d6e13bd88a7) )
	ROM_LOAD32_BYTE( "am2vera.rom4", 0x000000, 0x100000, CRC(1e3a6032) SHA1(f4c6539a78f94ae2c240aeb8f85712a612933e8c) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "am1data.8k", 0x000000, 0x080000, CRC(3c176589) SHA1(fabf8debfa118893449f6086986fd1aa012daf27) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "am1scg0.12f", 0x000000, 0x200000, CRC(e576f993) SHA1(704645c2b2af4d3f6b77f2299cd7277837e204d2) )
	ROM_LOAD( "am1scg1.10f", 0x200000, 0x200000, CRC(d6d26cc1) SHA1(1da35ec2f89cfc5778d81b6a892fcf100d6e9933) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "am1cg0.8d", 0x000000, 0x200000, CRC(b91e4259) SHA1(3de7fee2be9843ef6a8718f9dcbc3aeb1fb23b64) )
	ROM_LOAD( "am1cg1.10d", 0x200000, 0x200000, CRC(c1b82a26) SHA1(25dc3cdf5fe18de89e6d7304ff76b6157aba2cd6) )
	ROM_LOAD( "am1cg2.12d", 0x400000, 0x200000, CRC(36a7d3fd) SHA1(f07bb746f775d1eb70c4b6a65e6002d2136cfe4d) )
	ROM_LOAD( "am1cg3.13d", 0x600000, 0x200000, CRC(8b0857ef) SHA1(1c86db318d7bb7bc303ba65a9db493b033035d7b) )
	ROM_LOAD( "am1cg4.14d", 0x800000, 0x200000, CRC(ebc516fe) SHA1(585554f14cfdf69a89a6431867b7cfc1a9eae379) )
	ROM_LOAD( "am1cg5.16d", 0xa00000, 0x200000, CRC(30fad30d) SHA1(bb0d9e2b479d83cc38ffd76d0d55f06dca0f304a) )
	ROM_LOAD( "am1cg6.18d", 0xc00000, 0x200000, CRC(62eea883) SHA1(37e441bb1289e45ef3043f70f77646c15b6dff0e) )
	ROM_LOAD( "am1cg7.19d", 0xe00000, 0x200000, CRC(2d257f47) SHA1(6406556b4b142b6c596eb25f407275a63f6a44f1) )
	ROM_REGION( 0x300000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "am1ccrl.3d", 0x000000, 0x200000, CRC(76c6d5f3) SHA1(637efe30d004a9c42864b7536e02e9805ed9b6ef) )
	ROM_LOAD( "am1ccrh.1d", 0x200000, 0x080000, CRC(180c4a33) SHA1(5e0818ba8a135c51953bc3bc5c4a1475f7f93eff) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "am1ptrl0.18k", 0x000000, 0x080000, CRC(6fdb34f6) SHA1(5dc7359eef0dbe6d421808eefd3920159ce34944) )
	ROM_LOAD( "am1ptrl1.16k", 0x080000, 0x080000, CRC(32e40601) SHA1(161b148aae688e944cfe456d9bca1ca7139812e1) )
	ROM_LOAD( "am1ptrl2.15k", 0x100000, 0x080000, CRC(e27e29cc) SHA1(3a2b0d18d23b2ace6fe1c6c4dbcc73cb74acd189) )
	ROM_LOAD( "am1ptrl3.14k", 0x180000, 0x080000, CRC(0c638e3e) SHA1(2cd68e6aa04a9235f792fad0913314545ab70e71) )
	ROM_LOAD( "am1ptrm0.18j", 0x200000, 0x080000, CRC(e676cd98) SHA1(f7b31c848bc2afc31f3ad53d9eda7daae5f28865) )
	ROM_LOAD( "am1ptrm1.16j", 0x280000, 0x080000, CRC(eb115d9d) SHA1(165cb13a3d0f2982c35ae5e6e796de48cf2b5846) )
	ROM_LOAD( "am1ptrm2.15j", 0x300000, 0x080000, CRC(86b4ed0c) SHA1(d97f2b26cc3981b4b337029723c5ddec9316628f) )
	ROM_LOAD( "am1ptrm3.14j", 0x380000, 0x080000, CRC(97c1c6b0) SHA1(84e5f982f50560debd6f1f4d3d96f58888616529) )
	ROM_LOAD( "am1ptru0.18f", 0x400000, 0x080000, CRC(6a3fd855) SHA1(c6d2029e676426c4e71d9cfddbac7ecdec81c4da) )
	ROM_LOAD( "am1ptru1.16f", 0x480000, 0x080000, CRC(551bf714) SHA1(4af1e65ad1e42a95accafb57422fc276743f408a) )
	ROM_LOAD( "am1ptru2.15f", 0x500000, 0x080000, CRC(ce790635) SHA1(fb49646a9b35ee7cb3ace59a7f0932a08c393052) )
	ROM_LOAD( "am1ptru3.14f", 0x580000, 0x080000, CRC(5e4e6333) SHA1(5897251e4694c5c24f1810ff6f9177a0456baf2e) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD16_WORD_SWAP( "am1wavea.2l", 0x000000, 0x400000, CRC(48f8c20c) SHA1(48b4fbcb7e9dbbb70a542ef7cb7ee0e46fad23fc) )
	ROM_LOAD16_WORD_SWAP( "am1waveb.1l", 0x800000, 0x400000, CRC(fd8e7384) SHA1(91e53ab0293f81f8357645fd319249abc128b78e) )
ROM_END

ROM_START( adillorj )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD32_BYTE( "am1vera.rom1", 0x000003, 0x100000, CRC(e99157f9) SHA1(7a200f3b5890f5badbf529a8c8ac6a8548adb801) )
	ROM_LOAD32_BYTE( "am1vera.rom2", 0x000002, 0x100000, CRC(b63d79b0) SHA1(c83251727b2973f5b9dc2eea23b51b1275bd88ed) )
	ROM_LOAD32_BYTE( "am1vera.rom3", 0x000001, 0x100000, CRC(af0983bc) SHA1(136d2e14485864e20d7a6947d640577b8a85243c) )
	ROM_LOAD32_BYTE( "am1vera.rom4", 0x000000, 0x100000, CRC(4424047f) SHA1(d0ca736c085db58d33b603813b7a54c8ce995bac) )
	ROM_REGION16_LE( 0x080000, REGION_CPU4, 0 ) /* MCU */
	ROM_LOAD( "am1data.8k", 0x000000, 0x080000, CRC(3c176589) SHA1(fabf8debfa118893449f6086986fd1aa012daf27) )
	C71_BIOS
	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_ERASEFF ) /* 32x32x8bpp sprite tiles */
	ROM_LOAD( "am1scg0.12f", 0x000000, 0x200000, CRC(e576f993) SHA1(704645c2b2af4d3f6b77f2299cd7277837e204d2) )
	ROM_LOAD( "am1scg1.10f", 0x200000, 0x200000, CRC(d6d26cc1) SHA1(1da35ec2f89cfc5778d81b6a892fcf100d6e9933) )
	ROM_REGION( 0x1000000, REGION_GFX2, 0 ) /* 16x16x8bpp texture tiles */
	ROM_LOAD( "am1cg0.8d", 0x000000, 0x200000, CRC(b91e4259) SHA1(3de7fee2be9843ef6a8718f9dcbc3aeb1fb23b64) )
	ROM_LOAD( "am1cg1.10d", 0x200000, 0x200000, CRC(c1b82a26) SHA1(25dc3cdf5fe18de89e6d7304ff76b6157aba2cd6) )
	ROM_LOAD( "am1cg2.12d", 0x400000, 0x200000, CRC(36a7d3fd) SHA1(f07bb746f775d1eb70c4b6a65e6002d2136cfe4d) )
	ROM_LOAD( "am1cg3.13d", 0x600000, 0x200000, CRC(8b0857ef) SHA1(1c86db318d7bb7bc303ba65a9db493b033035d7b) )
	ROM_LOAD( "am1cg4.14d", 0x800000, 0x200000, CRC(ebc516fe) SHA1(585554f14cfdf69a89a6431867b7cfc1a9eae379) )
	ROM_LOAD( "am1cg5.16d", 0xa00000, 0x200000, CRC(30fad30d) SHA1(bb0d9e2b479d83cc38ffd76d0d55f06dca0f304a) )
	ROM_LOAD( "am1cg6.18d", 0xc00000, 0x200000, CRC(62eea883) SHA1(37e441bb1289e45ef3043f70f77646c15b6dff0e) )
	ROM_LOAD( "am1cg7.19d", 0xe00000, 0x200000, CRC(2d257f47) SHA1(6406556b4b142b6c596eb25f407275a63f6a44f1) )
	ROM_REGION( 0x300000, REGION_GFX3, 0 ) /* texture tilemap */
	ROM_LOAD( "am1ccrl.3d", 0x000000, 0x200000, CRC(76c6d5f3) SHA1(637efe30d004a9c42864b7536e02e9805ed9b6ef) )
	ROM_LOAD( "am1ccrh.1d", 0x200000, 0x080000, CRC(180c4a33) SHA1(5e0818ba8a135c51953bc3bc5c4a1475f7f93eff) )
	ROM_REGION( 0x600000, REGION_USER1, 0 ) /* 3D model data */
	ROM_LOAD( "am1ptrl0.18k", 0x000000, 0x080000, CRC(6fdb34f6) SHA1(5dc7359eef0dbe6d421808eefd3920159ce34944) )
	ROM_LOAD( "am1ptrl1.16k", 0x080000, 0x080000, CRC(32e40601) SHA1(161b148aae688e944cfe456d9bca1ca7139812e1) )
	ROM_LOAD( "am1ptrl2.15k", 0x100000, 0x080000, CRC(e27e29cc) SHA1(3a2b0d18d23b2ace6fe1c6c4dbcc73cb74acd189) )
	ROM_LOAD( "am1ptrl3.14k", 0x180000, 0x080000, CRC(0c638e3e) SHA1(2cd68e6aa04a9235f792fad0913314545ab70e71) )
	ROM_LOAD( "am1ptrm0.18j", 0x200000, 0x080000, CRC(e676cd98) SHA1(f7b31c848bc2afc31f3ad53d9eda7daae5f28865) )
	ROM_LOAD( "am1ptrm1.16j", 0x280000, 0x080000, CRC(eb115d9d) SHA1(165cb13a3d0f2982c35ae5e6e796de48cf2b5846) )
	ROM_LOAD( "am1ptrm2.15j", 0x300000, 0x080000, CRC(86b4ed0c) SHA1(d97f2b26cc3981b4b337029723c5ddec9316628f) )
	ROM_LOAD( "am1ptrm3.14j", 0x380000, 0x080000, CRC(97c1c6b0) SHA1(84e5f982f50560debd6f1f4d3d96f58888616529) )
	ROM_LOAD( "am1ptru0.18f", 0x400000, 0x080000, CRC(6a3fd855) SHA1(c6d2029e676426c4e71d9cfddbac7ecdec81c4da) )
	ROM_LOAD( "am1ptru1.16f", 0x480000, 0x080000, CRC(551bf714) SHA1(4af1e65ad1e42a95accafb57422fc276743f408a) )
	ROM_LOAD( "am1ptru2.15f", 0x500000, 0x080000, CRC(ce790635) SHA1(fb49646a9b35ee7cb3ace59a7f0932a08c393052) )
	ROM_LOAD( "am1ptru3.14f", 0x580000, 0x080000, CRC(5e4e6333) SHA1(5897251e4694c5c24f1810ff6f9177a0456baf2e) )
	ROM_REGION( 0x1000000, REGION_SOUND1, 0 ) /* C352 samples */
	ROM_LOAD16_WORD_SWAP( "am1wavea.2l", 0x000000, 0x400000, CRC(48f8c20c) SHA1(48b4fbcb7e9dbbb70a542ef7cb7ee0e46fad23fc) )
	ROM_LOAD16_WORD_SWAP( "am1waveb.1l", 0x800000, 0x400000, CRC(fd8e7384) SHA1(91e53ab0293f81f8357645fd319249abc128b78e) )
ROM_END


/***************************************************************************
    Driver inits
***************************************************************************/

/* C352 outputs to the stereo stream, as current MAME maps each game's
   speakers: rear speakers to the near side, speakers in the middle of the
   cabinet (tank, road, seat) to both sides, subwoofers and bass shakers
   (LFE) nowhere. 0x100 = 1.0 */
enum { C352_FRONT, C352_REAR, C352_OUT2_CENTER, C352_OUT3_CENTER_HALF, C352_OUT3_CENTER };

static void c352_routing(int mode)
{
	int route[4][2];
	memset(route, 0, sizeof(route));
	route[0][0] = 0x100;
	route[1][1] = 0x100;
	switch (mode)
	{
		case C352_REAR:				route[2][0] = 0x100; route[3][1] = 0x100; break;
		case C352_OUT2_CENTER:		route[2][0] = route[2][1] = 0x100; break;
		case C352_OUT3_CENTER_HALF:	route[3][0] = route[3][1] = 0x80; break;
		case C352_OUT3_CENTER:		route[3][0] = route[3][1] = 0x100; break;
	}
	c352_set_routing((const int (*)[2])route);
}

/* System 22: the C74 (internal BIOS) plays the sound, the I/O board is simulated */
static void s22_init(int gametype)
{
	namcos22_common_init(gametype, 0);
	c352_routing(gametype == NAMCOS22_CYBER_COMMANDO ? C352_REAR : C352_FRONT);
	m37710_set_io_handlers(mcu_s22_io_r, mcu_s22_io_w);
	m37710_set_internal_rom(memory_region(REGION_USER4));
	/* the C74 BIOS (NSTX7702 v1.00) waits for its timer in "lda $81 / beq" */
	if (MCU_SPEEDUP) m37710_set_idle_loop(C74_IDLE, 2);
	state_save_register_UINT32("namcos22", 0, "credits1", &credits1, 1);
	state_save_register_UINT32("namcos22", 0, "credits2", &credits2, 1);
	state_save_register_int("namcos22", 0, "old_coin_state", &old_coin_state);
}

/* Super System 22: the S22-BIOS ver1.20/1.30 and SS22-BIOS ver1.41 MCU
   programs wait for their interrupts in "lda $83 / beq" */
#define BIOS_130	0xc128
#define BIOS_141	0xc12b

static void ss22_init(int gametype, UINT32 idle_pc)
{
	namcos22_common_init(gametype, 1);
	switch (gametype)
	{
		case NAMCOS22_CYBER_CYCLES:
		case NAMCOS22_AQUA_JET:		c352_routing(C352_OUT2_CENTER); break;		/* "tank" speaker */
		case NAMCOS22_DIRT_DASH:	c352_routing(C352_OUT3_CENTER_HALF); break;	/* "road" speaker at 0.5 */
		case NAMCOS22_TOKYO_WARS:	c352_routing(C352_OUT3_CENTER); break;		/* seat speaker (the bass shaker is LFE) */
		default:					c352_routing(C352_FRONT); break;			/* airco22b: the subwoofer is LFE */
	}
	if (MCU_SPEEDUP)
		m37710_set_idle_loop(idle_pc, 2);
}

static void alpine_init(int gametype, UINT32 idle_pc)
{
	ss22_init(gametype, idle_pc);
	set_adc_ports(PORT_ADC0, PORT_ADC0 + 1, -1, -1);
	alpine_motor = 1;
	alpine_motor_status = 2;		/* steps locked */
	alpine_motor_timer = timer_alloc(alpine_steplock);
	state_save_register_UINT8("namcos22", 0, "alpine_motor", &alpine_motor_status, 1);
}

static DRIVER_INIT( ridgerac ) { s22_init(NAMCOS22_RIDGE_RACER); }
static DRIVER_INIT( ridgera2 ) { s22_init(NAMCOS22_RIDGE_RACER2); }
static DRIVER_INIT( raverace ) { s22_init(NAMCOS22_RAVE_RACER); }
static DRIVER_INIT( acedrive ) { s22_init(NAMCOS22_ACE_DRIVER); }
static DRIVER_INIT( victlap )  { s22_init(NAMCOS22_VICTORY_LAP); }
static DRIVER_INIT( cybrcomm ) { s22_init(NAMCOS22_CYBER_COMMANDO); }

static DRIVER_INIT( timecris )
{
	ss22_init(NAMCOS22_TIME_CRISIS, BIOS_130);
	install_mem_read32_handler(0, 0x430000, 0x43000f, timecris_gun_r);
}

static void propcycl_common(void)
{
	ss22_init(NAMCOS22_PROP_CYCLE, BIOS_141);
	set_adc_ports(PORT_ADC0, PORT_ADC0 + 1, -1, -1);
	pedal_pulse_timer = timer_alloc(pedal_pulse);
	pedal_update_timer = timer_alloc(pedal_update);
	timer_adjust(pedal_update_timer, TIME_IN_MSEC(20), 0, TIME_IN_MSEC(20));
}

static DRIVER_INIT( propcycl )
{
	propcycl_common();
	/* HACK (MAME): patch out a strange routine (uninitialized EEPROM related?) */
	((data32_t *)memory_region(REGION_CPU1))[0x1992c / 4] = 0x4e754e75;
}

static DRIVER_INIT( propcyclj )
{
	propcycl_common();
	((data32_t *)memory_region(REGION_CPU1))[0x1990a / 4] = 0x4e754e75;
}

static DRIVER_INIT( alpiner )  { alpine_init(NAMCOS22_ALPINE_RACER, BIOS_130); }
static DRIVER_INIT( alpiner2 ) { alpine_init(NAMCOS22_ALPINE_RACER_2, BIOS_130); }

static DRIVER_INIT( alpines )
{
	alpine_init(NAMCOS22_ALPINE_SURFER, BIOS_141);
	install_mem_read32_handler(0, 0x200000, 0x3fffff, alpines_bank_r);
	install_mem_write32_handler(0, 0x300000, 0x300003, alpines_bank_w);
	alpines_bank = 0;
	state_save_register_int("namcos22", 0, "alpines_bank", &alpines_bank);
}

static DRIVER_INIT( airco22 )
{
	ss22_init(NAMCOS22_AIR_COMBAT22, BIOS_130);	/* S22-BIOS ver1.20 */
	set_adc_ports(PORT_ADC0, PORT_ADC0 + 1, -1, -1);
	/* the throttle stick centers at 0x200: one pedal pushes it, the other pulls it */
	set_adc(2, 0x200, PORT_ADC0 + 2, PORT_ADC0 + 3);
}

static DRIVER_INIT( cybrcycc )
{
	ss22_init(NAMCOS22_CYBER_CYCLES, BIOS_130);
	set_adc_ports(PORT_ADC0, PORT_ADC0 + 1, PORT_ADC0 + 2, -1);
}

static DRIVER_INIT( dirtdash )
{
	ss22_init(NAMCOS22_DIRT_DASH, BIOS_141);
	set_adc_ports(PORT_ADC0, PORT_ADC0 + 1, PORT_ADC0 + 2, -1);
}

static DRIVER_INIT( tokyowar )
{
	ss22_init(NAMCOS22_TOKYO_WARS, BIOS_141);
	set_adc_ports(PORT_ADC0, -1, PORT_ADC0 + 1, PORT_ADC0 + 2);
}

static DRIVER_INIT( aquajet )
{
	ss22_init(NAMCOS22_AQUA_JET, BIOS_141);
	set_adc_ports(PORT_ADC0, -1, PORT_ADC0 + 2, -1);
	set_adc(1, 0x1fc, -1, PORT_ADC0 + 1);	/* 0x1fc released, 0x0e0 full */
}

static DRIVER_INIT( adillor )
{
	int axis;
	ss22_init(NAMCOS22_ARMADILLO_RACING, BIOS_141);
	for (axis = 0; axis < 2; axis++)
	{
		trackball_pulse_timer[axis] = timer_alloc(trackball_pulse);
		trackball_count[axis] = 0;
		trackball_residual[axis] = 0;
	}
	trackball_update_timer = timer_alloc(trackball_update);
	timer_adjust(trackball_update_timer, TIME_IN_MSEC(20), 0, TIME_IN_MSEC(20));
	state_save_register_UINT32("namcos22", 0, "trackball_count", trackball_count, 2);
	state_save_register_INT32("namcos22", 0, "trackball_residual", trackball_residual, 2);
}


/***************************************************************************
    Game drivers
***************************************************************************/

/*    YEAR, NAME,      PARENT,   MACHINE,   INPUT,    INIT,      MONITOR, COMPANY, FULLNAME, FLAGS */
GAMEX( 1994, namcoc74,   0,        namcos22,  ridgera2, 0,         ROT0, "Namco", "Namco C74 BIOS", NOT_A_DRIVER )
GAMEX( 1993, ridgerac,   namcoc74, namcos22,  ridgera,  ridgerac,  ROT0, "Namco", "Ridge Racer (World, RR2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1993, ridgeraca,  ridgerac, namcos22,  ridgera,  ridgerac,  ROT0, "Namco", "Ridge Racer (World, RR2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1993, ridgeracb,  ridgerac, namcos22,  ridgera,  ridgerac,  ROT0, "Namco", "Ridge Racer (US, RR3 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1993, ridgeracc,  ridgerac, namcos22,  ridgera,  ridgerac,  ROT0, "Namco", "Ridge Racer (US, RR3)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1993, ridgeracj,  ridgerac, namcos22,  ridgera,  ridgerac,  ROT0, "Namco", "Ridge Racer (Japan, RR1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, ridgera2,   namcoc74, namcos22,  ridgera2, ridgera2,  ROT0, "Namco", "Ridge Racer 2 (World, RRS2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, ridgera2j,  ridgera2, namcos22,  ridgera2, ridgera2,  ROT0, "Namco", "Ridge Racer 2 (Japan, RRS1 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, ridgera2ja, ridgera2, namcos22,  ridgera2, ridgera2,  ROT0, "Namco", "Ridge Racer 2 (Japan, RRS1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, ridgera28,  ridgera2, namcos22,  ridgera2, ridgera2,  ROT0, "Namco", "Ridge Racer 2 (World, RRS8)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, cybrcomm,   namcoc74, namcos22,  cybrcomm, cybrcomm,  ROT0, "Namco", "Cyber Commando (Japan, CY1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, raverace,   namcoc74, namcos22,  raverace, raverace,  ROT0, "Namco", "Rave Racer (World, RV2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, raveracej,  raverace, namcos22,  raverace, raverace,  ROT0, "Namco", "Rave Racer (Japan, RV1 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, raveraceja, raverace, namcos22,  raverace, raverace,  ROT0, "Namco", "Rave Racer (Japan, RV1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, acedrive,   namcoc74, namcos22,  acedrive, acedrive,  ROT0, "Namco", "Ace Driver (World, AD2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, victlap,    namcoc74, namcos22,  acedrive, victlap,   ROT0, "Namco", "Ace Driver: Victory Lap (World, ADV2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, victlapa,   victlap,  namcos22,  acedrive, victlap,   ROT0, "Namco", "Ace Driver: Victory Lap (World, ADV2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, victlapj,   victlap,  namcos22,  acedrive, victlap,   ROT0, "Namco", "Ace Driver: Victory Lap (Japan, ADV1 Ver.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, alpinerd,   0,        namcos22s, alpiner,  alpiner,   ROT0, "Namco", "Alpine Racer (World, AR2 Ver.D)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, alpinerc,   alpinerd, namcos22s, alpiner,  alpiner,   ROT0, "Namco", "Alpine Racer (World, AR2 Ver.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, alpinerjc,  alpinerd, namcos22s, alpiner,  alpiner,   ROT0, "Namco", "Alpine Racer (Japan, AR1 Ver.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, airco22b,   0,        namcos22s, airco22,  airco22,   ROT0, "Namco", "Air Combat 22 (Japan, ACS1 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, cybrcycc,   0,        namcos22s, cybrcycc, cybrcycc,  ROT0, "Namco", "Cyber Cycles (World, CB2 Ver.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, cybrcyccj,  cybrcycc, namcos22s, cybrcycc, cybrcycc,  ROT0, "Namco", "Cyber Cycles (Japan, CB1 Ver.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, dirtdash,   0,        namcos22s, dirtdash, dirtdash,  ROT0, "Namco", "Dirt Dash (World, DT2 Ver.C)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, dirtdasha,  dirtdash, namcos22s, dirtdash, dirtdash,  ROT0, "Namco", "Dirt Dash (World, DT2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, dirtdashb,  dirtdash, namcos22s, dirtdash, dirtdash,  ROT0, "Namco", "Dirt Dash (World, DT2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, dirtdashj,  dirtdash, namcos22s, dirtdash, dirtdash,  ROT0, "Namco", "Dirt Dash (Japan, DT1 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, timecris,  0,        namcos22s, timecris, timecris,  ROT0, "Namco", "Time Crisis (World, TS2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, timecrisa, timecris, namcos22s, timecris, timecris,  ROT0, "Namco", "Time Crisis (World, TS2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, timecrisj, timecris, namcos22s, timecris, timecris,  ROT0, "Namco", "Time Crisis (Japan, TS1 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, propcycl,  0,        namcos22s, propcycl, propcycl,  ROT0, "Namco", "Prop Cycle (World, PR2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, propcyclj, propcycl, namcos22s, propcycl, propcyclj, ROT0, "Namco", "Prop Cycle (Japan, PR1 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, alpines,    0,        namcos22s, alpines,  alpines,   ROT0, "Namco", "Alpine Surfer (World, AF2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, tokyowar,   0,        namcos22s, tokyowar, tokyowar,  ROT0, "Namco", "Tokyo Wars (World, TW2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, tokyowarj,  tokyowar, namcos22s, tokyowar, tokyowar,  ROT0, "Namco", "Tokyo Wars (Japan, TW1 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, aquajet,    0,        namcos22s, aquajet,  aquajet,   ROT0, "Namco", "Aqua Jet (World, AJ2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, alpinr2b,   0,        namcos22s, alpiner,  alpiner2,  ROT0, "Namco", "Alpine Racer 2 (World, ARS2 Ver.B)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, alpinr2a,   alpinr2b, namcos22s, alpiner,  alpiner2,  ROT0, "Namco", "Alpine Racer 2 (World, ARS2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, adillor,    0,        namcos22s, adillor,  adillor,   ROT0, "Namco", "Armadillo Racing (World, AM2 Ver.A)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, adillorj,   adillor,  namcos22s, adillor,  adillor,   ROT0, "Namco", "Armadillo Racing (Japan, AM1 Ver.A)", GAME_IMPERFECT_GRAPHICS )
