/*************************************************************************

	Driver for Atari/Midway Seattle hardware games

	driver by Aaron Giles

	Games supported:
		* Bio Freaks [Midway]
		* CarnEvil [Midway, 150MHz]
		* NFL Blitz [Midway, 150MHz]
		* NFL Blitz 99 [Midway, 150MHz]
		* NFL Blitz 2000 [Midway, 150MHz]
		* California Speed [Atari, 150MHz]
		* Mace: The Dark Age [Atari, 200MHz]
		* San Francisco Rush [Atari]
		* Wayne Gretzky's 3d Hockey [Atari]

	Potentially to be added:
		* Hyperdrive [Midway, 200MHz]
		* Vapor TRX [Atari, 200MHz]
		* San Francisco Rush Alcatraz Edition [Atari]

	Known bugs:
		* general: Atari games timing is not quite right
		* CarnEvil: lets you set the flash brightness; need to emulate that
		* Blitz99: random black frames
		* Blitz99/2k: crash when running full powerup tests
		* Blitz99/2k: sounds play at the wrong frequency unless we use 12MHz
		* Wayne Gretzky: loses sound occasionally and has to reset it
		* SF Rush: hangs when trying to start a game (security?)

**************************************************************************/

#include "driver.h"
#include "cpu/adsp2100/adsp2100.h"
#include "cpu/mips/mips3.h"
#include "sndhrdw/dcs.h"
#include "sndhrdw/cage.h"
#include "machine/idectrl.h"
#include "machine/midwayic.h"
#include "machine/smc91c9x.h"
#include "vidhrdw/voodoo.h"
#include "bootstrap.h"
#include "inptport.h"


#define TIMER_CLOCK			TIME_IN_HZ(50000000)
#define DMA_SECS_PER_BYTE	TIME_IN_HZ(50000000)


#define LOG_TIMERS			(0)
#define LOG_DMA				(0)
#define LOG_GALILEO			(0)


static data32_t *rambase;
static data32_t *rombase;
static data32_t *galileo_regs;

static data32_t pci_bridge_regs[0x40];
static data32_t pci_3dfx_regs[0x40];

static void *timer[4];
static UINT32 timer_count[4];
static UINT8 timer_active[4];

static UINT8 vblank_latch;			/* VBLANK seen, until the game clears it (vblank_clear_w) */
static UINT8 vblank_state;			/* raw VBLANK */
static UINT8 vblank_irq;			/* 0: none, else the CPU IRQ line */
static UINT8 has_ethernet;			/* Flagstaff: SMC91C94 at CS3 0xc00000 */
static UINT8 ethernet_irq;			/* 0: none, else the CPU IRQ line */
static UINT8 ethernet_irq_state;
static data32_t *vblank_config;
static data32_t *vblank_enable;
static void vblank_set_state(int state);

static data32_t *asic_reset;

static data8_t pending_analog_read;

/* SF Rush wheel board (as current MAME): the game drives the wheel motor and, */
/* until calibrated, sees the wheel move the way it pushes it */
static UINT16 wheel_output_last;
static INT8 wheel_force;
static int wheel_offset;
static UINT8 wheel_calibrated;
static data32_t sfrush_gear;		/* shifter position, latched like a real gearbox */
static data32_t i40_data;			/* Blitz '99/2000: the I40 board's select (I/O ASIC aux output) */

static data32_t *generic_speedup;

/* direct RAM access and the MIPS recompiler, for the sets they were tested on */
static int seattle_fast_cpu;
static data32_t *generic_speedup2;


static void timer_callback(int param);

static data32_t cmos_write_enabled;



/*************************************
 *
 *	Machine init
 *
 *************************************/

static MACHINE_INIT( seattle )
{
	wheel_output_last = 0;
	wheel_force = 0;
	wheel_offset = 0;
	wheel_calibrated = 0;
	sfrush_gear = 1;
	i40_data = 0;
	ethernet_irq = 0;
	ethernet_irq_state = 0;
	if (has_ethernet)
		smc91c94_reset();

	cpu_setbank(1, rambase);
	cpu_setbank(2, rambase);
	cpu_setbank(3, rombase);

	if (mame_find_cpu_index("dcs2") != -1)
	{
		dcs_reset_w(0);
		dcs_reset_w(1);
	}
	else if (mame_find_cpu_index("cage") != -1)
	{
		cage_control_w(0);
		cage_control_w(3);
	}

	ide_controller_reset(0);

	/* direct RAM and boot ROM access for the CPU (the speedup words stay on their */
	/* handlers), and the recompiler (MIPS Dynarec core option) */
	if (seattle_fast_cpu)
	{
		/* kseg0 RAM, less the speedup words (they stay on their handlers) */
		offs_t skip[2], start = 0x80000000;
		int nskip = 0, i;
		if (generic_speedup)
			skip[nskip++] = 0x80000000 + (offs_t)((UINT8 *)generic_speedup - (UINT8 *)rambase);
		if (generic_speedup2)
			skip[nskip++] = 0x80000000 + (offs_t)((UINT8 *)generic_speedup2 - (UINT8 *)rambase);
		if (nskip == 2 && skip[1] < skip[0])
		{
			offs_t t = skip[0];
			skip[0] = skip[1];
			skip[1] = t;
		}
		for (i = 0; i < nskip; i++)
		{
			if (skip[i] > start)
				mips3_add_fastram(start, skip[i] - 1, 0, (UINT8 *)rambase + (start - 0x80000000));
			start = skip[i] + 4;
		}
		mips3_add_fastram(start, 0x807fffff, 0, (UINT8 *)rambase + (start - 0x80000000));
		mips3_add_fastram(0xa0000000, 0xa07fffff, 0, rambase);
		mips3_add_fastram(0x00000000, 0x007fffff, 0, rambase);
		mips3_add_fastram(0x9fc00000, 0x9fc7ffff, 1, rombase);
		mips3_add_fastram(0xbfc00000, 0xbfc7ffff, 1, rombase);
		if (options.mips_dynarec)
			mips3_set_jit(1);
	}

	/* the Galileo timers start stopped (as current MAME) */
	{
		int i;
		for (i = 0; i < 4; i++)
		{
			timer[i] = timer_alloc(timer_callback);
			timer_active[i] = 0;
			timer_count[i] = 0;
		}
	}

	vblank_irq = 0;
	vblank_latch = vblank_state = 0;

	voodoo_reset();
	voodoo_set_vblank_callback(vblank_set_state);
}



/*************************************
 *
 *	IDE interrupts
 *
 *************************************/

static void ide_interrupt(int state)
{
	cpu_set_irq_line(0, 2, state);
}

/*
	The byte offset in RAM of a CPU address (physical, kseg0 or kseg1), or -1
	when [address, address + count) is not all plain RAM: the words with a
	speedup handler go through the memory system, as before.
*/
static INT32 seattle_ram_offset(offs_t address, UINT32 count)
{
	UINT32 top = address >> 29, phys = address & 0x1fffffff, o;
	if ((top != 0 && top != 4 && top != 5) || phys + count > 0x00800000 || phys + count < phys)
		return -1;
	if (generic_speedup)
	{
		o = (UINT32)((UINT8 *)generic_speedup - (UINT8 *)rambase);
		if (o + 4 > phys && o < phys + count)
			return -1;
	}
	if (generic_speedup2)
	{
		o = (UINT32)((UINT8 *)generic_speedup2 - (UINT8 *)rambase);
		if (o + 4 > phys && o < phys + count)
			return -1;
	}
	return (INT32)phys;
}

/*
	The disk's bus master DMA into RAM (the game's code and data, 512 bytes
	a sector): stored directly rather than a byte at a time through the
	memory system, with what seattle_ram_w does for the recompiler.
*/
static int ide_dma_write(offs_t address, const UINT8 *data, UINT32 count)
{
	INT32 phys = seattle_ram_offset(address, count);
	UINT32 i;

	if (count == 0 || phys < 0)
		return 0;
	for (i = 0; i < count; i++)
		((UINT8 *)rambase)[BYTE4_XOR_LE(phys + i)] = data[i];
	if (seattle_fast_cpu)
		mips3_ram_written(0x80000000 + (phys & ~3), count + (phys & 3));
	return 1;
}

static struct ide_interface ide_intf =
{
	ide_interrupt,
	ide_dma_write
};



/*************************************
 *
 *	I/O ASIC interrupts
 *
 *************************************/

static void ioasic_irq(int state)
{
	cpu_set_irq_line(0, 1, state);
}



/*************************************
 *
 *	VBLANK interrupts
 *
 *************************************/

/*
	As current MAME: VBLANK is latched until the game writes the clear
	register, and asserts the configured IRQ (config bits 14-15: 0 none,
	else IRQ 2 + n) while latched and enabled (enable bit 7). Enable bit 8
	selects the edge that latches it.
*/
static void update_vblank_irq(void)
{
	if (vblank_irq)
		cpu_set_irq_line(0, vblank_irq, (vblank_latch && (*vblank_enable & 0x80)) ? ASSERT_LINE : CLEAR_LINE);
}


/* the Voodoo's VBLANK (voodoo_set_vblank_callback), as in current MAME */
static void vblank_set_state(int state)
{
	vblank_state = state;
	if ((state && !(*vblank_enable & 0x100)) || (!state && (*vblank_enable & 0x100)))
	{
		vblank_latch = 1;
		update_vblank_irq();
	}
}


/* Flagstaff ethernet: config bits 2-3 pick the IRQ, enable bit 1, state bit 1 (as current MAME) */
static void ethernet_interrupt(int state)
{
	ethernet_irq_state = state ? 1 : 0;
	if (ethernet_irq)
		cpu_set_irq_line(0, ethernet_irq, (ethernet_irq_state && (*vblank_enable & 0x02)) ? ASSERT_LINE : CLEAR_LINE);
}


static READ32_HANDLER( vblank_signalled_r )
{
	return (vblank_latch << 7) | (ethernet_irq_state << 1);
}


static READ32_HANDLER( interrupt_state2_r )
{
	return (vblank_latch << 7) | (ethernet_irq_state << 1) | (vblank_state << 8);
}


static WRITE32_HANDLER( vblank_enable_w )
{
	data32_t old = *vblank_enable;
	COMBINE_DATA(vblank_enable);
	if (old != *vblank_enable)
	{
		if (vblank_latch)
			update_vblank_irq();
		if (ethernet_irq_state)
			ethernet_interrupt(ethernet_irq_state);
	}
}


static WRITE32_HANDLER( vblank_config_w )
{
	int irq;
	COMBINE_DATA(vblank_config);
	if (vblank_irq)
		cpu_set_irq_line(0, vblank_irq, CLEAR_LINE);
	irq = (*vblank_config >> 14) & 3;
	vblank_irq = irq ? 2 + irq : 0;

	if (has_ethernet)
	{
		if (ethernet_irq)
			cpu_set_irq_line(0, ethernet_irq, CLEAR_LINE);
		irq = (*vblank_config >> 2) & 3;
		ethernet_irq = irq ? 2 + irq : 0;
	}

	update_vblank_irq();
	ethernet_interrupt(ethernet_irq_state);
}


/* SMC91C94: 16-bit registers in the first 8 words, byte access in the next 8 */
static READ32_HANDLER( ethernet_r )
{
	return smc91c94_r(offset & 7, (~mem_mask) & ((offset & 8) ? 0x00ff : 0xffff));
}


static WRITE32_HANDLER( ethernet_w )
{
	smc91c94_w(offset & 7, data & ((offset & 8) ? 0x00ff : 0xffff), (offset & 8) ? (((~mem_mask) & 0xffff) | 0x00ff) : 0xffff);
}


static WRITE32_HANDLER( vblank_clear_w )
{
	vblank_latch = 0;
	update_vblank_irq();
}


/*************************************
 *
 *	CMOS access
 *
 *************************************/

static WRITE32_HANDLER( cmos_w )
{
	data32_t *cmos_base = (data32_t *)generic_nvram;
	if (cmos_write_enabled)
		COMBINE_DATA(&cmos_base[offset]);
	cmos_write_enabled = 0;
}


static READ32_HANDLER( cmos_r )
{
	data32_t *cmos_base = (data32_t *)generic_nvram;
	return cmos_base[offset];
}


static WRITE32_HANDLER( cmos_protect_w )
{
	cmos_write_enabled = 1;
}


static READ32_HANDLER( cmos_protect_r )
{
	return cmos_write_enabled;
}




/*************************************
 *
 *	PCI bus writes
 *
 *************************************/

static void pci_bridge_w(UINT8 reg, UINT8 type, data32_t data)
{
	pci_bridge_regs[reg] = data;
	log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:PCI bridge write: reg %d type %d = %08X\n", activecpu_get_pc(), reg, type, data);
}


static void pci_3dfx_w(UINT8 reg, UINT8 type, data32_t data)
{
	pci_3dfx_regs[reg] = data;

	switch (reg)
	{
		case 0x04:		/* address register */
			pci_3dfx_regs[reg] &= 0xff000000;
			if (data != 0x08000000)
				log_cb(RETRO_LOG_DEBUG, LOGPRE "3dfx not mapped where we expect it!\n");
			break;

		case 0x10:		/* initEnable register */
			voodoo_set_init_enable(data);
			break;
	}
	log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:PCI 3dfx write: reg %d type %d = %08X\n", activecpu_get_pc(), reg, type, data);
}



/*************************************
 *
 *	PCI bus reads
 *
 *************************************/

static data32_t pci_bridge_r(UINT8 reg, UINT8 type)
{
	data32_t result = pci_bridge_regs[reg];

	log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:PCI bridge read: reg %d type %d = %08X\n", activecpu_get_pc(), reg, type, result);

	return result;
}


static data32_t pci_3dfx_r(UINT8 reg, UINT8 type)
{
	data32_t result = pci_3dfx_regs[reg];

	switch (reg)
	{
		case 0:		/* ID register: 0x0001 = SST-1, 0x121a = 3dfx */
			result = 0x0001121a;
			break;
	}

	log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:PCI 3dfx read: reg %d type %d = %08X\n", activecpu_get_pc(), reg, type, result);

	return result;
}



/*************************************
 *
 *	Galileo timers & interrupts
 *
 *************************************/

static void update_galileo_irqs(void)
{
	if (galileo_regs[0xc18/4] & galileo_regs[0xc1c/4])
	{
		if (LOG_GALILEO)
			log_cb(RETRO_LOG_DEBUG, LOGPRE "Galileo IRQ asserted\n");
		cpu_set_irq_line(0, 0, ASSERT_LINE);
	}
	else
	{
		if (LOG_GALILEO)
			log_cb(RETRO_LOG_DEBUG, LOGPRE "Galileo IRQ cleared\n");
		cpu_set_irq_line(0, 0, CLEAR_LINE);
	}
}


static void timer_callback(int which)
{
	if (LOG_GALILEO)
		log_cb(RETRO_LOG_DEBUG, LOGPRE "timer %d fired\n", which);

	/* copy the start value from the registers (0 counts the full range, as current MAME) */
	timer_count[which] = galileo_regs[0x850/4 + which];
	if (which != 0)
		timer_count[which] &= 0xffffff;
	if (timer_count[which] == 0)
		timer_count[which] = (which != 0) ? 0xffffff : 0xffffffff;

	/* if we're a timer, adjust the timer to fire again */
	if (galileo_regs[0x864/4] & (2 << (2 * which)))
		timer_adjust(timer[which], TIMER_CLOCK * timer_count[which], which, 0);
	else
		timer_active[which] = timer_count[which] = 0;

	/* trigger the interrupt */
	galileo_regs[0xc18/4] |= 0x100 << which;
	update_galileo_irqs();
}



/*************************************
 *
 *	Galileo DMA handler
 *
 *************************************/

/* a little-endian 32-bit value of a DMA record: from RAM directly, else through the memory system */
static data32_t dma_read_dword(offs_t address)
{
	INT32 o = seattle_ram_offset(address, 4);
	if (o >= 0)
		return ((UINT8 *)rambase)[BYTE4_XOR_LE(o)] | (((UINT8 *)rambase)[BYTE4_XOR_LE(o + 1)] << 8) |
			(((UINT8 *)rambase)[BYTE4_XOR_LE(o + 2)] << 16) | ((data32_t)((UINT8 *)rambase)[BYTE4_XOR_LE(o + 3)] << 24);
	return cpunum_read_byte(0, address) | (cpunum_read_byte(0, address + 1) << 8) |
		(cpunum_read_byte(0, address + 2) << 16) | ((data32_t)cpunum_read_byte(0, address + 3) << 24);
}

static int dma_fetch_next(int which)
{
	offs_t address = 0;
	data32_t data;

	/* no-op for unchained mode */
	if (!(galileo_regs[0x840/4 + which] & 0x200))
		address = galileo_regs[0x830/4 + which];

	/* if we hit the end address, signal an interrupt */
	if (address == 0)
	{
		if (galileo_regs[0x840/4 + which] & 0x400)
		{
			galileo_regs[0xc18/4] |= 0x10 << which;
			update_galileo_irqs();
		}
		return 0;
	}

	/* fetch the byte count */
	data = dma_read_dword(address);
	address += 4;
	galileo_regs[0x800/4 + which] = data;

	/* fetch the source address */
	data = dma_read_dword(address);
	address += 4;
	galileo_regs[0x810/4 + which] = data;

	/* fetch the dest address */
	data = dma_read_dword(address);
	address += 4;
	galileo_regs[0x820/4 + which] = data;

	/* fetch the next record address */
	data = dma_read_dword(address);
	address += 4;
	galileo_regs[0x830/4 + which] = data;

/*	logerror("DMA Fetch Record: bytes=%08X src=%08X dst=%08X nextrec=%08X\n",*/
/*		galileo_regs[0x800/4 + which],*/
/*		galileo_regs[0x810/4 + which],*/
/*		galileo_regs[0x820/4 + which],*/
/*		galileo_regs[0x830/4 + which]);*/
	return 1;
}

static void perform_dma(int which);

static void dma_finished_callback(int which)
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "DMA%d finished\n", which);*/
	galileo_regs[0x840/4 + which] &= ~0x4000;
	galileo_regs[0x840/4 + which] &= ~0x1000;

	/* interrupt? */
	if (!(galileo_regs[0x840/4 + which] & 0x400))
	{
		galileo_regs[0xc18/4] |= 0x10 << which;
		update_galileo_irqs();
	}

	/* chain? */
	if (dma_fetch_next(which))
		perform_dma(which);
}


static void perform_dma(int which)
{
	offs_t srcaddr = galileo_regs[0x810/4 + which];
	offs_t dstaddr = galileo_regs[0x820/4 + which];
	data32_t bytesleft = galileo_regs[0x800/4 + which] & 0xffff;
	int srcinc, dstinc, i;

	/* determine src/dst inc */
	switch ((galileo_regs[0x840/4 + which] >> 2) & 3)
	{
		default:
		case 0:		srcinc = 1;		break;
		case 1:		srcinc = -1;	break;
		case 2:		srcinc = 0;		break;
	}
	switch ((galileo_regs[0x840/4 + which] >> 4) & 3)
	{
		default:
		case 0:		dstinc = 1;		break;
		case 1:		dstinc = -1;	break;
		case 2:		dstinc = 0;		break;
	}

	if (LOG_DMA)
		log_cb(RETRO_LOG_DEBUG, LOGPRE "Performing DMA%d: src=%08X dst=%08X bytes=%04X sinc=%d dinc=%d\n", which, srcaddr, dstaddr, bytesleft, srcinc, dstinc);

	/* special case: transfer ram to voodoo */
	if (bytesleft % 4 == 0 && srcaddr % 4 == 0 && srcaddr < 0x007fffff && dstaddr >= 0x08000000 && dstaddr < 0x09000000)
	{
		data32_t *src = &rambase[srcaddr/4];
		bytesleft /= 4;

		/* transfer to registers */
		if (dstaddr < 0x8400000)
		{
			dstaddr = (dstaddr & 0x3fffff) / 4;
			for (i = 0; i < bytesleft; i++)
			{
				voodoo_regs_w(dstaddr, *src, 0);
				src += srcinc;
				dstaddr += dstinc;
			}
		}

		/* transfer to framebuf */
		else if (dstaddr < 0x8800000)
		{
			dstaddr = (dstaddr & 0x3fffff) / 4;
			for (i = 0; i < bytesleft; i++)
			{
				voodoo_framebuf_w(dstaddr, *src, 0);
				src += srcinc;
				dstaddr += dstinc;
			}
		}

		/* transfer to textureram */
		else
		{
			dstaddr = (dstaddr & 0x7fffff) / 4;
			for (i = 0; i < bytesleft; i++)
			{
				voodoo_textureram_w(dstaddr, *src, 0);
				src += srcinc;
				dstaddr += dstinc;
			}
		}
	}

	/* RAM to RAM, both ascending: directly, with what seattle_ram_w does for the recompiler */
	else if (srcinc == 1 && dstinc == 1 && bytesleft > 0 && seattle_ram_offset(srcaddr, bytesleft) >= 0 && seattle_ram_offset(dstaddr, bytesleft) >= 0)
	{
		UINT8 *ram = (UINT8 *)rambase;
		INT32 so = seattle_ram_offset(srcaddr, bytesleft), d0 = seattle_ram_offset(dstaddr, bytesleft), d;
		for (d = d0, i = 0; i < bytesleft; i++)
			ram[BYTE4_XOR_LE(d + i)] = ram[BYTE4_XOR_LE(so + i)];
		if (seattle_fast_cpu)
			mips3_ram_written(0x80000000 + (d0 & ~3), bytesleft + (d0 & 3));
	}

	/* standard transfer */
	else
	{
		for (i = 0; i < bytesleft; i++)
		{
			cpunum_write_byte(0, dstaddr, cpunum_read_byte(0, srcaddr));
			srcaddr += srcinc;
			dstaddr += dstinc;
		}
	}

	/* set a timer for the end */
	galileo_regs[0x840/4 + which] |= 0x4000;
	if (bytesleft > 0x100)
		timer_set(DMA_SECS_PER_BYTE * bytesleft, which, dma_finished_callback);
	else
		dma_finished_callback(which);
}



/*************************************
 *
 *	Galileo system controller
 *
 *************************************/

/*
	0x000 = CPU interface configuration

	0x008 = RAS[1:0] low decode address
	0x010 = RAS[1:0] high decode address
	0x018 = RAS[3:2] low decode address
	0x020 = RAS[3:2] high decode address
	0x028 = CS[2:0] low decode address
	0x030 = CS[2:0] high decode address
	0x038 = CS[3] & boot CS low decode address
	0x040 = CS[3] & boot CS high decode address
	0x048 = PCI I/O low decode address
	0x050 = PCI I/O high decode address
	0x058 = PCI memory low decode address
	0x060 = PCI memory high decode address
	0x068 = internal space decode
	0x070 = bus error address low processor
	0x078 = bus error address high processor

	0x400 = RAS[0] low decode address
	0x404 = RAS[0] high decode address
	0x408 = RAS[1] low decode address
	0x40c = RAS[1] high decode address
	0x410 = RAS[2] low decode address
	0x414 = RAS[2] high decode address
	0x418 = RAS[3] low decode address
	0x41c = RAS[3] high decode address
	0x420 = CS[0] low decode address
	0x424 = CS[0] high decode address
	0x428 = CS[1] low decode address
	0x42c = CS[1] high decode address
	0x430 = CS[2] low decode address
	0x434 = CS[2] high decode address
	0x438 = CS[3] low decode address
	0x43c = CS[3] high decode address
	0x440 = boot CS low decode address
	0x444 = boot CS high decode address
	0x448 = DRAM configuration
	0x44c = DRAM bank 0 parameters
	0x450 = DRAM bank 1 parameters
	0x454 = DRAM bank 2 parameters
	0x458 = DRAM bank 3 parameters
	0x45c = device bank 0 parameters
	0x460 = device bank 1 parameters
	0x464 = device bank 2 parameters
	0x468 = device bank 3 parameters
	0x46c = device boot bank parameters
	0x470 = address decode error

	0x800 = channel 0 DMA byte count
	0x804 = channel 1 DMA byte count
	0x808 = channel 2 DMA byte count
	0x80c = channel 3 DMA byte count
	0x810 = channel 0 DMA source address
	0x814 = channel 1 DMA source address
	0x818 = channel 2 DMA source address
	0x81c = channel 3 DMA source address
	0x820 = channel 0 DMA destination address
	0x824 = channel 1 DMA destination address
	0x828 = channel 2 DMA destination address
	0x82c = channel 3 DMA destination address
	0x830 = channel 0 next record pointer
	0x834 = channel 1 next record pointer
	0x838 = channel 2 next record pointer
	0x83c = channel 3 next record pointer
	0x840 = channel 0 control
	0x844 = channel 1 control
	0x848 = channel 2 control
	0x84c = channel 3 control
	0x850 = timer/counter 0
	0x854 = timer/counter 1
	0x858 = timer/counter 2
	0x85c = timer/counter 3
	0x860 = DMA arbiter control
	0x864 = timer/counter control

	0xc00 = PCI internal command
	0xc04 = PCI internal time out & retry
	0xc08 = PCI internal RAS[1:0] bank size
	0xc0c = PCI internal RAS[3:2] bank size
	0xc10 = PCI internal CS[2:0] bank size
	0xc14 = PCI internal CS[3] & boot CS bank size
	0xc18 = interrupt cause
	0xc1c = CPU interrupt mask
	0xc24 = PCI interrupt mask
	0xc28 = SErr mask
	0xc34 = interrupt acknowledge
	0xcf8 = configuration address
	0xcfc = configuration data
*/

static READ32_HANDLER( galileo_r )
{
	data32_t result = galileo_regs[offset];

	/* switch off the offset for special cases */
	switch (offset)
	{
		case 0x850/4:		/* timer/counter 0 count */
		case 0x854/4:		/* timer/counter 1 count */
		case 0x858/4:		/* timer/counter 2 count */
		case 0x85c/4:		/* timer/counter 3 count */
		{
			int which = offset % 4;

			result = timer_count[which];
			if (timer_active[which])
			{
				UINT32 elapsed = (UINT32)(timer_timeelapsed(timer[which]) / TIMER_CLOCK);
				result = (result > elapsed) ? (result - elapsed) : 0;
			}


			if (LOG_TIMERS)
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:hires_timer_r = %08X\n", activecpu_get_pc(), result);
			break;
		}

		case 0xc00/4:		/* PCI internal command */
			/* code at 40188 loops until this returns non-zero in bit 0*/
			result = 0x0001;
			break;

		case 0xc18/4:		/* interrupt cause */
			if (LOG_GALILEO)
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:Galileo read from offset %03X = %08X\n", activecpu_get_pc(), offset*4, result);
			break;

		case 0xcfc/4:		/* configuration data */
		{
			int bus = (galileo_regs[0xcf8/4] >> 16) & 0xff;
			int unit = (galileo_regs[0xcf8/4] >> 11) & 0x1f;
			int func = (galileo_regs[0xcf8/4] >> 8) & 7;
			int reg = (galileo_regs[0xcf8/4] >> 2) & 0x3f;
			int type = galileo_regs[0xcf8/4] & 3;

			/* unit 0 is the PCI bridge */
			if (unit == 0 && func == 0)
				result = pci_bridge_r(reg >> 2, type);

			/* unit 6 is the 3dfx card */
			else if (unit == 6 && func == 0)
				result = pci_3dfx_r(reg, type);

			/* anything else, just log */
			else
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:PCIBus read: bus %d unit %d func %d reg %d type %d = %08X\n", activecpu_get_pc(), bus, unit, func, reg, type, result);
			break;
		}

		case 0x864/4:		/* timer/counter control */
			break;

		default:
			log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:Galileo read from offset %03X = %08X\n", activecpu_get_pc(), offset*4, result);
			break;
	}

	return result;
}


static WRITE32_HANDLER( galileo_w )
{
	UINT32 oldata = galileo_regs[offset];
	COMBINE_DATA(&galileo_regs[offset]);

	/* switch off the offset for special cases */
	switch (offset)
	{
		case 0x840/4:		/* DMA channel 0 control */
		case 0x844/4:		/* DMA channel 1 control */
		case 0x848/4:		/* DMA channel 2 control */
		case 0x84c/4:		/* DMA channel 3 control */
		{
			int which = offset % 4;

			/* keep the read only activity bit */
			galileo_regs[offset] &= ~0x4000;
			galileo_regs[offset] |= (oldata & 0x4000);

			/* fetch next record */
			if (data & 0x2000)
				dma_fetch_next(which);
			galileo_regs[offset] &= ~0x2000;

			/* if enabling, start the DMA */
			if (!(oldata & 0x1000) && (data & 0x1000))
				perform_dma(which);
			break;
		}

		case 0x850/4:		/* timer/counter 0 reset value */
		case 0x854/4:		/* timer/counter 1 reset value */
		case 0x858/4:		/* timer/counter 2 reset value */
		case 0x85c/4:		/* timer/counter 3 reset value */
		{
			int which = offset % 4;

			if (which != 0)
				data &= 0xffffff;
			if (!timer_active[which])
				timer_count[which] = data;
			if (LOG_TIMERS)
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:timer/counter %d count = %08X [start=%08X]\n", activecpu_get_pc(), offset % 4, data, timer_count[which]);
			break;
		}

		case 0x864/4:		/* timer/counter control */
		{
			int which, mask;

			if (LOG_TIMERS)
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:timer/counter control = %08X\n", activecpu_get_pc(), data);
			for (which = 0, mask = 0x01; which < 4; which++, mask <<= 2)
			{
				if (!timer_active[which] && (data & mask))
				{
					timer_active[which] = 1;
					if (timer_count[which] == 0)
					{
						timer_count[which] = galileo_regs[0x850/4 + which];
						if (which != 0)
							timer_count[which] &= 0xffffff;
						if (timer_count[which] == 0)
							timer_count[which] = (which != 0) ? 0xffffff : 0xffffffff;
					}
					timer_adjust(timer[which], TIMER_CLOCK * timer_count[which], which, 0);
					if (LOG_TIMERS)
						log_cb(RETRO_LOG_DEBUG, LOGPRE "Adjusted timer to fire in %f secs\n", TIMER_CLOCK * timer_count[which]);
				}
				else if (timer_active[which] && !(data & mask))
				{
					UINT32 elapsed = (UINT32)(timer_timeelapsed(timer[which]) / TIMER_CLOCK);
					timer_active[which] = 0;
					timer_count[which] = (timer_count[which] > elapsed) ? (timer_count[which] - elapsed) : 0;
					timer_adjust(timer[which], TIME_NEVER, which, 0);
					if (LOG_TIMERS)
						log_cb(RETRO_LOG_DEBUG, LOGPRE "Disabled timer\n");
				}
			}
			break;
		}

		case 0xc18/4:		/* IRQ clear */
			if (LOG_GALILEO)
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:Galileo write to IRQ clear = %08X & %08X\n", offset*4, data, ~mem_mask);
			galileo_regs[offset] = oldata & data;
			update_galileo_irqs();
			break;

		case 0xcf8/4:		/* configuration address */
			break;

		case 0xcfc/4:		/* configuration data */
		{
			int bus = (galileo_regs[0xcf8/4] >> 16) & 0xff;
			int unit = (galileo_regs[0xcf8/4] >> 11) & 0x1f;
			int func = (galileo_regs[0xcf8/4] >> 8) & 7;
			int reg = (galileo_regs[0xcf8/4] >> 2) & 0x3f;
			int type = galileo_regs[0xcf8/4] & 3;

			/* unit 0 is the PCI bridge */
			if (unit == 0 && func == 0)
				pci_bridge_w(reg >> 2, type, data);

			/* unit 6 is the 3dfx card */
			else if (unit == 6 && func == 0)
				pci_3dfx_w(reg, type, data);

			/* anything else, just log */
			else
				log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:PCIBus write: bus %d unit %d func %d reg %d type %d = %08X\n", activecpu_get_pc(), bus, unit, func, reg, type, data);
			break;
		}

		default:
			log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:Galileo write to offset %03X = %08X & %08X\n", activecpu_get_pc(), offset*4, data, ~mem_mask);
			break;
	}
}



/*************************************
 *
 *	Misc accesses
 *
 *************************************/

static WRITE32_HANDLER( seattle_watchdog_w )
{
	activecpu_eat_cycles(100);
}


static WRITE32_HANDLER( asic_reset_w )
{
	COMBINE_DATA(asic_reset);
	if (!(*asic_reset & 0x0002))
		midway_ioasic_reset();
}


static WRITE32_HANDLER( asic_fifo_w )
{
	midway_ioasic_fifo_w(data);
}



/*************************************
 *
 *	Misc unknown accesses
 *
 *************************************/




/*************************************
 *
 *	Analog input handling
 *
 *************************************/

static READ32_HANDLER( analog_port_r )
{
	return pending_analog_read;
}


static WRITE32_HANDLER( analog_port_w )
{
	int index = data & 7;
	int value = readinputport(4 + index) & 0xff;

	if (data < 8 || data > 15)
		log_cb(RETRO_LOG_DEBUG, LOGPRE "%08X:Unexpected analog port select = %08X\n", activecpu_get_pc(), data);

	/* the wheel board turns the wheel while the game calibrates it */
	if (!wheel_calibrated && (wheel_force > 20 || wheel_force < -20))
	{
		if (wheel_force > 0 && wheel_offset < 128)
			wheel_offset++;
		else if (wheel_offset > -128)
			wheel_offset--;
		value += wheel_offset;
		if (value < ((index == 7) ? 0x10 : 0x00))
			value = (index == 7) ? 0x10 : 0x00;
		else if (value > ((index == 7) ? 0xf0 : 0xff))
			value = (index == 7) ? 0xf0 : 0xff;
	}
	pending_analog_read = value;

	/* calibration is over as soon as a system button is pressed */
	if (!wheel_calibrated && ((~readinputport(1)) & 0xffff))
		wheel_calibrated = 1;
}


/* the shifter stays in the last gear selected (the gear inputs are momentary) */
static data32_t sfrush_port_filter(int port, data32_t data)
{
	if (port == 2)
	{
		data32_t pressed = (~data >> 8) & 0x0f;
		int i;

		for (i = 0; i < 4; i++)
			if (pressed & (1 << i))
				sfrush_gear = 1 << i;
		data = (data & ~0x0f00) | ((~sfrush_gear & 0x0f) << 8);
	}
	return data;
}


/*
	Blitz: the 49-way joysticks (the Joysticks DIP set to 49-Way), as
	current MAME. Each axis of the left analog stick (0x00-0x6f) is one of
	7 optical positions; Blitz reads players 1 and 2 on IN2, Blitz '99 and
	2000 read players 1 to 4 one axis at a time through the I40 board,
	selected by the I/O ASIC aux output.
*/
static const UINT8 translate49[7] = { 0x8, 0xc, 0xe, 0xf, 0x3, 0x1, 0x0 };

static int pos49(int port)
{
	int v = (readinputport(port) & 0xff) >> 4;
	return translate49[(v > 6) ? 6 : v];
}

static data32_t blitz_port_filter(int port, data32_t data)
{
	if (port == 3 && !(readinputport(0) & 0x100))
	{
		data32_t v = (pos49(7) << 12) | (pos49(6) << 8) | (pos49(5) << 4) | pos49(4);
		data = (data & ~0xffff) | (~v & 0xffff);
	}
	return data;
}

static void i40_w(data32_t data)
{
	i40_data = data;
}

static data32_t blitz99_port_filter(int port, data32_t data)
{
	if (port == 3 && !(readinputport(0) & 0x100))
	{
		int index = i40_data & 0xf;
		data32_t v = 0;
		if (index < 8)
			v = pos49(4 + index);			/* P1 X, P1 Y, P2 X, ... P4 Y */
		else if (index >= 10 && index <= 12)
			v = ~index & 0xf;				/* I40 detection */
		data = (data & ~0x0f00) | ((~v & 0xf) << 8);
	}
	return data;
}


/* writes to the I/O ASIC aux output: the wheel board (latched on bit 11 rising) */
static void wheel_board_w(data32_t data)
{
	if (!(wheel_output_last & 0x800) && (data & 0x800))
	{
		if (!(data & 0x400) && !(data & 0x300))
			wheel_force = (INT8)(data & 0xff);	/* target wheel angle, signed */
	}
	wheel_output_last = data;
}



/*************************************
 *
 *	CarnEvil gun handling
 *
 *************************************/

static INLINE void get_crosshair_xy(int player, int *x, int *y)
{
	*x = (((readinputport(4 + player * 2) & 0xff) << 4) * Machine->visible_area.max_x) / 0xfff;
	*y = (((readinputport(5 + player * 2) & 0xff) << 2) * Machine->visible_area.max_y) / 0x3ff;
}


static VIDEO_UPDATE( carnevil )
{
	int beamx, beamy;

	/* first do common video update */
	video_update_voodoo(bitmap, cliprect);

	/* now draw the crosshairs */
	get_crosshair_xy(0, &beamx, &beamy);
	draw_crosshair(1, bitmap, beamx, beamy, cliprect);
	get_crosshair_xy(1, &beamx, &beamy);
	draw_crosshair(2, bitmap, beamx, beamy, cliprect);
}


static READ32_HANDLER( carnevil_gun_r )
{
	data32_t result = 0;

	switch (offset)
	{
		case 0:		/* low 8 bits of X */
			result = (readinputport(4) << 4) & 0xff;
			break;

		case 1:		/* upper 4 bits of X */
			result = (readinputport(4) >> 4) & 0x0f;
			result |= (readinputport(8) & 0x03) << 4;
			result |= 0x40;
			break;

		case 2:		/* low 8 bits of Y */
			result = (readinputport(5) << 2) & 0xff;
			break;

		case 3:		/* upper 4 bits of Y */
			result = (readinputport(5) >> 6) & 0x03;
			break;

		case 4:		/* low 8 bits of X */
			result = (readinputport(6) << 4) & 0xff;
			break;

		case 5:		/* upper 4 bits of X */
			result = (readinputport(6) >> 4) & 0x0f;
			result |= (readinputport(8) & 0x30);
			result |= 0x40;
			break;

		case 6:		/* low 8 bits of Y */
			result = (readinputport(7) << 2) & 0xff;
			break;

		case 7:		/* upper 4 bits of Y */
			result = (readinputport(7) >> 6) & 0x03;
			break;
	}
	return result;
}


static WRITE32_HANDLER( carnevil_gun_w )
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "carnevil_gun_w(%d) = %02X\n", offset, data);
}



/*************************************
 *
 *	Speedups
 *
 *************************************/

static READ32_HANDLER( generic_speedup_r )
{
	activecpu_eat_cycles(100);
	return *generic_speedup;
}


static WRITE32_HANDLER( generic_speedup_w )
{
	activecpu_eat_cycles(100);
	COMBINE_DATA(generic_speedup);
}


static READ32_HANDLER( generic_speedup2_r )
{
	activecpu_eat_cycles(100);
	return *generic_speedup2;
}



/*************************************
 *
 *	Main RAM writes from outside the CPU
 *
 *************************************/

/*
	With fast RAM (mips3_add_fastram) the CPU writes the RAM directly; these
	handlers then only see the other writers, the IDE and Galileo DMA,
	which load the game's code from the disk. Code that the recompiler
	translated must be dropped when they overwrite it.
*/
static WRITE32_HANDLER( seattle_ram_w )
{
	COMBINE_DATA(&rambase[offset]);
	if (seattle_fast_cpu)
		mips3_ram_written(0x80000000 + offset * 4, 4);
}



/*************************************
 *
 *	Memory maps
 *
 *************************************/

static MEMORY_READ32_START( seattle_readmem )
	{ 0x00000000, 0x007fffff, MRA32_BANK1 },
	{ 0x80000000, 0x807fffff, MRA32_BANK2 },
	{ 0x88000000, 0x883fffff, voodoo_regs_r },
	{ 0x88400000, 0x887fffff, voodoo_framebuf_r },
	{ 0x9fc00000, 0x9fc7ffff, MRA32_BANK3 },
	{ 0xa0000000, 0xa07fffff, MRA32_RAM },			/* wg3dh only has 4MB; sfrush, blitz99 8MB*/
	{ 0xa8000000, 0xa83fffff, voodoo_regs_r },
	{ 0xa8400000, 0xa87fffff, voodoo_framebuf_r },
	{ 0xaa000000, 0xaa0003ff, ide_controller32_0_r },
	{ 0xaa00040c, 0xaa00040f, MRA32_NOP },			/* IDE-related, but annoying*/
	{ 0xaa000f00, 0xaa000f07, ide_bus_master32_0_r },
	{ 0xac000000, 0xac000fff, galileo_r },
	{ 0xb6000000, 0xb600003f, midway_ioasic_r },
	{ 0xb6100000, 0xb611ffff, cmos_r },
	{ 0xb7000000, 0xb7000003, cmos_protect_r },
	{ 0xb7300000, 0xb7300003, MRA32_RAM },
	{ 0xb7400000, 0xb7400003, MRA32_RAM },
	{ 0xb7500000, 0xb7500003, vblank_signalled_r },
	{ 0xb7600000, 0xb7600003, interrupt_state2_r },
	{ 0xb7900000, 0xb7900003, MRA32_NOP },			/* very noisy -- status LEDs?*/
	{ 0xb7f00000, 0xb7f00003, MRA32_RAM },
	{ 0xbfc00000, 0xbfc7ffff, MRA32_ROM },
MEMORY_END


static MEMORY_WRITE32_START( seattle_writemem )
	{ 0x00000000, 0x007fffff, seattle_ram_w },
	{ 0x80000000, 0x807fffff, seattle_ram_w },
	{ 0x88000000, 0x883fffff, voodoo_regs_w },
	{ 0x88400000, 0x887fffff, voodoo_framebuf_w },
	{ 0x88800000, 0x88ffffff, voodoo_textureram_w },
	{ 0x9fc00000, 0x9fc7ffff, MWA32_ROM },
	{ 0xa0000000, 0xa07fffff, seattle_ram_w, &rambase },		/* wg3dh only has 4MB*/
	{ 0xa8000000, 0xa83fffff, voodoo_regs_w, &voodoo_regs },
	{ 0xa8400000, 0xa87fffff, voodoo_framebuf_w },
	{ 0xa8800000, 0xa8ffffff, voodoo_textureram_w },
	{ 0xaa000000, 0xaa0003ff, ide_controller32_0_w },
	{ 0xaa000f00, 0xaa000f07, ide_bus_master32_0_w },
	{ 0xac000000, 0xac000fff, galileo_w, &galileo_regs },
	{ 0xb3000000, 0xb3000003, asic_fifo_w },
	{ 0xb6000000, 0xb600003f, midway_ioasic_w },
	{ 0xb6100000, 0xb611ffff, cmos_w, (data32_t **)&generic_nvram, &generic_nvram_size },
	{ 0xb7000000, 0xb7000003, cmos_protect_w },
	{ 0xb7100000, 0xb7100003, seattle_watchdog_w },
	{ 0xb7300000, 0xb7300003, vblank_enable_w, &vblank_enable },
	{ 0xb7400000, 0xb7400003, vblank_config_w, &vblank_config },
	{ 0xb7700000, 0xb7700003, vblank_clear_w },
	{ 0xb7800000, 0xb7800003, MWA32_NOP },						/* unknown*/
	{ 0xb7900000, 0xb7900003, MWA32_NOP },						/* very noisy -- status LEDs?*/
	{ 0xb7f00000, 0xb7f00003, asic_reset_w, &asic_reset },
	{ 0xbfc00000, 0xbfc7ffff, MWA32_ROM, &rombase },
MEMORY_END



/*************************************
 *
 *	Input ports
 *
 *************************************/

INPUT_PORTS_START( wg3dh )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown0001" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_SERVICE( 0x0002, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown0004" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown0008" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown0010" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown0020" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown0040" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0080, "Unknown0080" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0100, 0x0100, "Unknown0100" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Unknown0200" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown0400" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown0800" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Unknown1000" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Unknown2000" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Unknown4000" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Unknown8000" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SPECIAL )	/* Bill */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* 3d cam */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( mace )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown0001" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0002, "Unknown0002" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown0004" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown0008" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown0010" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown0020" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0080, 0x0080, "Unknown0080" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0100, 0x0100, "Unknown0100" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Unknown0200" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown0400" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown0800" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Unknown1000" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Unknown2000" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Unknown4000" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, "Resolution" )
	PORT_DIPSETTING(      0x8000, "Low" )
	PORT_DIPSETTING(      0x0000, "Medium" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SPECIAL )	/* Bill */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* quick */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* strong */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* evade */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* kick */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* quick */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )	/* strong */
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )	/* evade */
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )	/* kick */

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

INPUT_PORTS_END


INPUT_PORTS_START( sfrush )
	PORT_START	    /* DIPs */
	PORT_BITX(0x0001, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_DIPNAME( 0x0002, 0x0002, "Boot ROM Test" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown0004" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown0008" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown0010" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown0020" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown0040" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0080, "Unknown0080" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0100, 0x0100, "Unknown0100" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Unknown0200" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown0400" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown0800" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Unknown1000" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Unknown2000" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Unknown4000" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Unknown8000" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )	/* coin 1 */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )	/* coin 2 */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )	/* abort */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) 	/* tilt */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE2 )	/* test */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON10 | IPF_PLAYER1 )	/* reverse */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )	/* service coin */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )	/* coin 3 */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )	/* coin 4 */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON7 | IPF_PLAYER1 )	/* view 1 */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON8 | IPF_PLAYER1 )	/* view 2 */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON9 | IPF_PLAYER1 )	/* view 3 */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* music */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )	/* track 1 */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )	/* track 2 */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )	/* track 3 */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )	/* track 4 */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* 1st gear */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* 2nd gear */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* 3rd gear */
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* 4th gear */
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BITX(0x2000, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 25, 20, 0x00, 0xff )	/* accelerator */

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL2 | IPF_PLAYER1, 25, 100, 0x00, 0xff )	/* brake */

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 25, 100, 0x00, 0xff )	/* clutch */

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE, 25, 5, 0x10, 0xf0 )	/* steering */
INPUT_PORTS_END


INPUT_PORTS_START( calspeed )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown0001" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0002, "Unknown0002" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown0004" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown0008" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown0010" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown0020" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Boot ROM Test" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_BITX(0x0080, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_DIPNAME( 0x0100, 0x0100, "Unknown0100" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Unknown0200" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown0400" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown0800" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Unknown1000" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Unknown2000" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Unknown4000" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Unknown8000" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )	/* coin 1 */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )	/* coin 2 */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )	/* start */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) 	/* tilt */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE2 )	/* test */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )	/* service coin */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )	/* coin 3 */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )	/* coin 4 */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )	/* radio */
	PORT_BIT( 0x000c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )	/* road cam */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )	/* tailgate cam */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )	/* sky cam */
	PORT_BIT( 0x0f80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* 1st gear */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* 2nd gear */
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* 3rd gear */
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )	/* 4th gear */

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE, 25, 5, 0x10, 0xf0 )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 25, 20, 0x00, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 25, 100, 0x00, 0xff )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0xff, 0x80, IPT_SPECIAL )
INPUT_PORTS_END


INPUT_PORTS_START( biofreak )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown0001" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0002, "Unknown0002" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown0004" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown0008" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown0010" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown0020" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown0040" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0080, "Unknown0080" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0100, 0x0100, "Unknown0100" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, "Unknown0200" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown0400" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Unknown0800" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Unknown1000" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Unknown2000" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Unknown4000" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Unknown8000" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SPECIAL )	/* Bill */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* left punch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* right punch */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )	/* fire/shield */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* left punch */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )	/* right punch */
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )	/* fire/shield */
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* kicks and hover, as current MAME */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )	/* left kick */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )	/* right kick */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )	/* hover */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )	/* left kick */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )	/* right kick */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )	/* hover */
	PORT_BIT( 0xff80, IP_ACTIVE_LOW, IPT_UNUSED )

INPUT_PORTS_END


INPUT_PORTS_START( blitz )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0001, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x000e, 0x000e, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x000e, "Mode 1" )
	PORT_DIPSETTING(      0x0008, "Mode 2" )
	PORT_DIPSETTING(      0x0009, "Mode 3" )
	PORT_DIPSETTING(      0x0002, "Mode 4" )
	PORT_DIPSETTING(      0x000c, "Mode ECA" )
/*	PORT_DIPSETTING(      0x0004, "Not Used 1" )		 // Marked as Unused in the manual /*/
/*	PORT_DIPSETTING(      0x0008, "Not Used 2" )		 // Marked as Unused in the manual /*/
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0030, 0x0030, "Curency Type" )
	PORT_DIPSETTING(      0x0030, "USA" )
	PORT_DIPSETTING(      0x0020, "French" )
	PORT_DIPSETTING(      0x0010, "German" )
/*	PORT_DIPSETTING(      0x0000, "Not Used" )		 // Marked as Unused in the manual /*/
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Unknown ))	/* Marked as Unused in the manual */
	PORT_DIPSETTING(      0x0040, "0" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x0080, 0x0080, "Power Up Test Loop" )
	PORT_DIPSETTING(      0x0080, "One Time" )
	PORT_DIPSETTING(      0x0000, "Continuous" )
	PORT_DIPNAME( 0x0100, 0x0100, "Joysticks" )
	PORT_DIPSETTING(      0x0100, "8-Way" )
	PORT_DIPSETTING(      0x0000, "49-Way" )
	PORT_DIPNAME( 0x0600, 0x0200, "Graphics Mode" )
	PORT_DIPSETTING(      0x0200, "512x385 @ 25KHz" )
	PORT_DIPSETTING(      0x0400, "512x256 @ 15KHz" )
/*	PORT_DIPSETTING(      0x0600, "0" )			 // Marked as Unused in the manual /*/
/*	PORT_DIPSETTING(      0x0000, "3" )			 // Marked as Unused in the manual /*/
	PORT_DIPNAME( 0x1800, 0x1800, "Graphics Speed" )
	PORT_DIPSETTING(      0x0000, "45 MHz" )
	PORT_DIPSETTING(      0x0800, "47 MHz" )
	PORT_DIPSETTING(      0x1000, "49 MHz" )
	PORT_DIPSETTING(      0x1800, "51 MHz" )
	PORT_DIPNAME( 0x2000, 0x0000, "Bill Validator" )
	PORT_DIPSETTING(      0x2000, "None" )
	PORT_DIPSETTING(      0x0000, "One" )
	PORT_DIPNAME( 0x4000, 0x0000, "Power On Self Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ))
	PORT_DIPSETTING(      0x4000, DEF_STR( Yes ))
	PORT_DIPNAME( 0x8000, 0x8000, "Test Switch" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SPECIAL )	/* Bill */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 49-way P1 X */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P1 Y */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P2 X */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X | IPF_PLAYER2, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P2 Y */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 10, 0x00, 0x6f )
INPUT_PORTS_END


INPUT_PORTS_START( blitz99 )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0001, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x003e, 0x003e, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x003e, "USA 1" )
	PORT_DIPSETTING(      0x003c, "USA 2" )
	PORT_DIPSETTING(      0x003a, "USA 3" )
	PORT_DIPSETTING(      0x0038, "USA 4" )
	PORT_DIPSETTING(      0x0036, "USA 5" )
	PORT_DIPSETTING(      0x0034, "USA 6" )
	PORT_DIPSETTING(      0x0032, "USA 7" )
	PORT_DIPSETTING(      0x0030, "USA ECA" )
	PORT_DIPSETTING(      0x002e, "France 1" )
	PORT_DIPSETTING(      0x002c, "France 2" )
	PORT_DIPSETTING(      0x002a, "France 3" )
	PORT_DIPSETTING(      0x0028, "France 4" )
	PORT_DIPSETTING(      0x0026, "France 5" )
	PORT_DIPSETTING(      0x0024, "France 6" )
	PORT_DIPSETTING(      0x0022, "France 7" )
	PORT_DIPSETTING(      0x0020, "France ECA" )
	PORT_DIPSETTING(      0x001e, "German 1" )
	PORT_DIPSETTING(      0x001c, "German 2" )
	PORT_DIPSETTING(      0x001a, "German 3" )
	PORT_DIPSETTING(      0x0018, "German 4" )
	PORT_DIPSETTING(      0x0016, "German 5" )
/*	PORT_DIPSETTING(      0x0014, "German 5" )*/
/*	PORT_DIPSETTING(      0x0012, "German 5" )*/
	PORT_DIPSETTING(      0x0010, "German ECA" )
	PORT_DIPSETTING(      0x000e, "U.K. 1 ECA" )
	PORT_DIPSETTING(      0x000c, "U.K. 2 ECA" )
	PORT_DIPSETTING(      0x000a, "U.K. 3 ECA" )
	PORT_DIPSETTING(      0x0008, "U.K. 4" )
	PORT_DIPSETTING(      0x0006, "U.K. 5" )
	PORT_DIPSETTING(      0x0004, "U.K. 6 ECA" )
	PORT_DIPSETTING(      0x0002, "U.K. 7 ECA" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Unknown ))
	PORT_DIPSETTING(      0x0040, "0" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x0080, 0x0080, "Power Up Test Loop" )
	PORT_DIPSETTING(      0x0080, DEF_STR( No ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Yes ))
	PORT_DIPNAME( 0x0100, 0x0100, "Joysticks" )
	PORT_DIPSETTING(      0x0100, "8-Way" )
	PORT_DIPSETTING(      0x0000, "49-Way" )
	PORT_DIPNAME( 0x0600, 0x0200, "Graphics Mode" )
	PORT_DIPSETTING(      0x0200, "512x385 @ 25KHz" )
	PORT_DIPSETTING(      0x0400, "512x256 @ 15KHz" )
/*	PORT_DIPSETTING(      0x0600, "0" )			 // Marked as Unused in the manual /*/
/*	PORT_DIPSETTING(      0x0000, "3" )			 // Marked as Unused in the manual /*/
	PORT_DIPNAME( 0x1800, 0x1800, "Graphics Speed" )
	PORT_DIPSETTING(      0x0000, "45 MHz" )
	PORT_DIPSETTING(      0x0800, "47 MHz" )
	PORT_DIPSETTING(      0x1000, "49 MHz" )
	PORT_DIPSETTING(      0x1800, "51 MHz" )
	PORT_DIPNAME( 0x2000, 0x0000, "Players" )
	PORT_DIPSETTING(      0x2000, "2" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x4000, 0x0000, "Power On Self Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ))
	PORT_DIPSETTING(      0x4000, DEF_STR( Yes ))
	PORT_DIPNAME( 0x8000, 0x8000, "Test Switch" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SPECIAL )	/* Bill */

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 49-way P1 X */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P1 Y */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P2 X */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X | IPF_PLAYER2, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P2 Y */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_PLAYER2, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P3 X */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X | IPF_PLAYER3, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P3 Y */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_PLAYER3, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P4 X */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_X | IPF_PLAYER4, 100, 10, 0x00, 0x6f )

	PORT_START	/* 49-way P4 Y */
	PORT_ANALOG( 0xff, 0x38, IPT_AD_STICK_Y | IPF_PLAYER4, 100, 10, 0x00, 0x6f )
INPUT_PORTS_END


INPUT_PORTS_START( carnevil )
	PORT_START	    /* DIPs */
	PORT_DIPNAME( 0x0001, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0001, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x003e, 0x003e, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x003e, "USA 1" )
	PORT_DIPSETTING(      0x003c, "USA 2" )
	PORT_DIPSETTING(      0x003a, "USA 3" )
	PORT_DIPSETTING(      0x0038, "USA 4" )
	PORT_DIPSETTING(      0x0036, "USA 5" )
	PORT_DIPSETTING(      0x0034, "USA 6" )
	PORT_DIPSETTING(      0x0032, "USA 7" )
	PORT_DIPSETTING(      0x0030, "USA ECA" )
	PORT_DIPSETTING(      0x002e, "France 1" )
	PORT_DIPSETTING(      0x002c, "France 2" )
	PORT_DIPSETTING(      0x002a, "France 3" )
	PORT_DIPSETTING(      0x0028, "France 4" )
	PORT_DIPSETTING(      0x0026, "France 5" )
	PORT_DIPSETTING(      0x0024, "France 6" )
	PORT_DIPSETTING(      0x0022, "France 7" )
	PORT_DIPSETTING(      0x0020, "France ECA" )
	PORT_DIPSETTING(      0x001e, "German 1" )
	PORT_DIPSETTING(      0x001c, "German 2" )
	PORT_DIPSETTING(      0x001a, "German 3" )
	PORT_DIPSETTING(      0x0018, "German 4" )
	PORT_DIPSETTING(      0x0016, "German 5" )
/*	PORT_DIPSETTING(      0x0014, "German 5" )*/
/*	PORT_DIPSETTING(      0x0012, "German 5" )*/
	PORT_DIPSETTING(      0x0010, "German ECA" )
	PORT_DIPSETTING(      0x000e, "U.K. 1" )
	PORT_DIPSETTING(      0x000c, "U.K. 2" )
	PORT_DIPSETTING(      0x000a, "U.K. 3" )
	PORT_DIPSETTING(      0x0008, "U.K. 4" )
	PORT_DIPSETTING(      0x0006, "U.K. 5" )
	PORT_DIPSETTING(      0x0004, "U.K. 6" )
	PORT_DIPSETTING(      0x0002, "U.K. 7 ECA" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Unknown ))
	PORT_DIPSETTING(      0x0040, "0" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x0080, 0x0080, "Power Up Test Loop" )
	PORT_DIPSETTING(      0x0080, DEF_STR( No ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Yes ))
	PORT_DIPNAME( 0x0100, 0x0000, DEF_STR( Unknown ))
	PORT_DIPSETTING(      0x0100, "0" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x0600, 0x0400, "Resolution" )
/*	PORT_DIPSETTING(      0x0600, "0" )*/
/*	PORT_DIPSETTING(      0x0200, "Medium" )*/
	PORT_DIPSETTING(      0x0400, "Low" )
/*	PORT_DIPSETTING(      0x0000, "3" )*/
	PORT_DIPNAME( 0x1800, 0x1800, "Graphics Speed" )
	PORT_DIPSETTING(      0x0000, "45 MHz" )
	PORT_DIPSETTING(      0x0800, "47 MHz" )
	PORT_DIPSETTING(      0x1000, "49 MHz" )
	PORT_DIPSETTING(      0x1800, "51 MHz" )
	PORT_DIPNAME( 0x2000, 0x0000, DEF_STR( Unknown ))
	PORT_DIPSETTING(      0x2000, "0" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x4000, 0x0000, "Power On Self Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ))
	PORT_DIPSETTING(      0x4000, DEF_STR( Yes ))
	PORT_DIPNAME( 0x8000, 0x8000, "Test Switch" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) /* Test switch */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0780, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SPECIAL )	/* Bill */

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START				/* fake analog X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X, 50, 10, 0, 255 )

	PORT_START				/* fake analog Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y, 70, 10, 0, 255 )

	PORT_START				/* fake analog X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 50, 10, 0, 255 )

	PORT_START				/* fake analog Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 70, 10, 0, 255 )

	PORT_START				/* fake switches */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
INPUT_PORTS_END



/*************************************
 *
 *	Machine drivers
 *
 *************************************/

static struct mips3_config config =
{
	16384,	/* code cache size */
	16384	/* data cache size */
};

MACHINE_DRIVER_START( seattle_flagstaff_common )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", R5000LE, 50000000*3)
	MDRV_CPU_CONFIG(config)
	MDRV_CPU_MEMORY(seattle_readmem,seattle_writemem)

	MDRV_FRAMES_PER_SECOND(57)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(seattle)
	MDRV_NVRAM_HANDLER(generic_1fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(512, 400)
	MDRV_VISIBLE_AREA(0, 511, 0, 399)
	MDRV_PALETTE_LENGTH(65536)

	MDRV_VIDEO_START(voodoo_1x4mb)
	MDRV_VIDEO_STOP(voodoo)
	MDRV_VIDEO_UPDATE(voodoo)

	/* sound hardware */
MACHINE_DRIVER_END


MACHINE_DRIVER_START( seattle150 )
	MDRV_IMPORT_FROM(seattle_flagstaff_common)

	/* sound hardware */
	MDRV_IMPORT_FROM(dcs2_audio)
MACHINE_DRIVER_END


/*
	Frame rates: each game programs the Voodoo's video timing; current MAME
	(voodoo_1_device::recompute_video_timing) runs it at the total lines over
	the nearest standard horizontal rate (15.75, 25 or 31.5 kHz). The rates
	below are the ones it picks for each game.
*/

/* Phoenix board (Wayne Gretzky): 100 MHz R4700, 280 lines at 15.75 kHz */
MACHINE_DRIVER_START( phoenix )
	MDRV_IMPORT_FROM(seattle150)
	MDRV_CPU_REPLACE("main", R4600LE, 50000000*2)
	MDRV_FRAMES_PER_SECOND(56.25)
MACHINE_DRIVER_END


/* Blitz and Blitz '99: 439 lines at 25 kHz */
MACHINE_DRIVER_START( blitz )
	MDRV_IMPORT_FROM(seattle150)
	MDRV_FRAMES_PER_SECOND(56.947608)
MACHINE_DRIVER_END


/* Mace: 150 MHz (as current MAME), 414 lines at 25 kHz */
MACHINE_DRIVER_START( mace )
	MDRV_IMPORT_FROM(seattle150)
	MDRV_FRAMES_PER_SECOND(60.386473)
MACHINE_DRIVER_END


MACHINE_DRIVER_START( carnevil )
	MDRV_IMPORT_FROM(seattle150)
	/* the game programs the Voodoo for 291 lines a frame on a standard */
	/* resolution (15.75 kHz) monitor: 15750 / 291 Hz, as current MAME */
	MDRV_FRAMES_PER_SECOND(54.123711)
	MDRV_VIDEO_UPDATE(carnevil)
MACHINE_DRIVER_END


MACHINE_DRIVER_START( flagstaff )
	MDRV_IMPORT_FROM(seattle_flagstaff_common)
	MDRV_CPU_REPLACE("main", R5000LE, 50000000*4)

	/* 414 lines at 25 kHz (SF Rush) */
	MDRV_FRAMES_PER_SECOND(60.386473)

	/* video hardware */
	MDRV_VIDEO_START(voodoo_2x4mb)

	/* sound hardware */
	MDRV_IMPORT_FROM(cage_seattle)
MACHINE_DRIVER_END



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( wg3dh )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version L1.1 */
	ROM_LOAD( "l1.1_gretzky_video_system_boot_u95.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(c589458c) SHA1(0cf970a35910a74cdcf3bd8119bfc0c693e19b00) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version L1.2 */
	ROM_LOAD( "l1.2_gretzky_video_system_boot_u32.u32", 0x000000, 0x80000, CRC(15e4cea2) SHA1(72c0db7dc53ce645ba27a5311b5ce803ad39f131) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "wg3dh.chd", 0, SHA1(4fc6f25d7f043d9bcf8743aa8df1d9be3cbc375b) )	/* ST94420AG hard drive */
ROM_END


ROM_START( wg3dhfe )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version L1.1 */
	ROM_LOAD( "l0.1_gretzky_video_system_boot_u95.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(c589458c) SHA1(0cf970a35910a74cdcf3bd8119bfc0c693e19b00) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code L0.1, the board the disk came with */
	ROM_LOAD( "l0.1_gretzky_video_system_boot_u32.u32", 0x000000, 0x80000, CRC(80fb6dfd) SHA1(b78d1c8abdb875196c5cd9afc986f54865a90aba) )

	DISK_REGION( REGION_DISKS )	/* Guts: Feb 4 1997, Main: Mar 21 1997 */
	DISK_IMAGE( "wg3dhfe.chd", 0, SHA1(49b72e7e76036b119bfc8b9a0ef8b9acd346d0be) )	/* ST94420AG hard drive */
ROM_END


ROM_START( mace )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version L1.1 */
	ROM_LOAD( "soundl11.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(c589458c) SHA1(0cf970a35910a74cdcf3bd8119bfc0c693e19b00) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version 1.0ce 7/2/97 */
	ROM_LOAD( "mace10ce.u32", 0x000000, 0x80000, CRC(7a50b37e) SHA1(33788835f84a9443566c80bee9f20a1691490c6d) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.0b 6/10/97 */
	DISK_IMAGE( "mace.chd", 0, SHA1(96ec8d3ff5dd894e21aa81403bcdbeba44bb97ea) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "314_mace.u96", 0x0000, 0x2000, CRC(65943e82) SHA1(984a1938f43671fc8f6394677396451dab7f042b) )
ROM_END


ROM_START( macea )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version L1.1 */
	ROM_LOAD( "soundl11.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(c589458c) SHA1(0cf970a35910a74cdcf3bd8119bfc0c693e19b00) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version ??? 5/7/97 */
	ROM_LOAD( "maceboot.u32", 0x000000, 0x80000, CRC(effe3ebc) SHA1(7af3ca3580d6276ffa7ab8b4c57274e15ee6bcbb) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.0a (Guts 6/9/97, Main 5/12/97) */
	DISK_IMAGE( "macea.chd", 0, BAD_DUMP SHA1(9bd4a60627915d71932cab24f89c48ea21f4c1cb) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "314_mace.u96", 0x0000, 0x2000, CRC(65943e82) SHA1(984a1938f43671fc8f6394677396451dab7f042b) )
ROM_END


ROM_START( sfrush )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( 0x040000, REGION_CPU2, 0 )		/* RAM for TMS320C31 */

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version L1.0 */
	ROM_LOAD( "hdboot.u32", 0x000000, 0x80000, CRC(39a35f1b) SHA1(c46d83448399205d38e6e41dd56abbc362254254) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 )	/* TMS320C31 boot ROM  Version L1.0 */
	ROM_LOAD32_BYTE( "sndboot.u69", 0x000000, 0x080000, CRC(7e52cdc7) SHA1(f735063e19d2ca672cef6d761a2a47df272e8c59) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 )	/* TMS320C31 sound ROMs */
	ROM_LOAD32_WORD( "sfrush.u62",  0x400000, 0x200000, CRC(5d66490e) SHA1(bd39ea3b45d44cae6ca5890f365653326bbecd2d) )
	ROM_LOAD32_WORD( "sfrush.u61",  0x400002, 0x200000, CRC(f3a00ee8) SHA1(c1ac780efc32b2e30522d7cc3e6d92e7daaadddd) )
	ROM_LOAD32_WORD( "sfrush.u53",  0x800000, 0x200000, CRC(71f8ddb0) SHA1(c24bef801f43bae68fda043c4356e8cf1298ca97) )
	ROM_LOAD32_WORD( "sfrush.u49",  0x800002, 0x200000, CRC(dfb0a54c) SHA1(ed34f9485f7a7e5bb73bf5c6428b27548e12db12) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version L1.06 */
	DISK_IMAGE( "sfrush.chd", 0, SHA1(e2db0270a707fb2115207f988d5751081d6b4994) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "315_sf_rush.u96", 0x0000, 0x2000, CRC(e3527a3a) SHA1(5e556e6dfd87df5a895bdf4bd7f77708ac327db7) )
ROM_END


ROM_START( sfrushrk )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( 0x040000, REGION_CPU2, 0 )		/* RAM for TMS320C31 */

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code */
	ROM_LOAD( "boot.bin", 0x000000, 0x80000, CRC(0555b3cf) SHA1(a48abd6d06a26f4f9b6c52d8c0af6095b6be57fd) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 )	/* TMS320C31 boot ROM */
	ROM_LOAD32_BYTE( "audboot.bin", 0x000000, 0x080000, CRC(c70c060d) SHA1(dd014bd13efdf5adc5450836bd4650351abefc46) )

	ROM_REGION32_LE( 0x1000000, REGION_USER3, 0 )	/* TMS320C31 sound ROMs */
	ROM_LOAD32_WORD( "audio.u62",  0x400000, 0x200000, CRC(cacf09e3) SHA1(349af1767cb0ee2a0eb9d7c2ab078fcae5fec8e7) )
	ROM_LOAD32_WORD( "audio.u61",  0x400002, 0x200000, CRC(ea895d29) SHA1(1edde0497f2abd1636c5d7bcfbc03bcff321261c) )
	ROM_LOAD32_WORD( "audio.u53",  0x800000, 0x200000, CRC(51c89a14) SHA1(6bc62bcda224040a4596d795132874828011a038) )
	ROM_LOAD32_WORD( "audio.u49",  0x800002, 0x200000, CRC(e6b684d3) SHA1(1f5bab7fae974cecc8756dd23e3c7aa2cf6e7dc7) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.2 */
	DISK_IMAGE( "sfrushrk.chd", 0, SHA1(e763f26aca67ebc17fe8b8df4fba91d492cf7837) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "331_rush_the_rock.u96", 0x0000, 0x1000, CRC(425155b6) SHA1(4ec5471d8ede6154322e9dc6de372b8f4686e11b) )
ROM_END


ROM_START( calspeed )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD( "caspd1_2.u32", 0x000000, 0x80000, CRC(0a235e4e) SHA1(b352f10fad786260b58bd344b5002b6ea7aaf76d) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "calspeed.chd", 0, SHA1(08d411c591d4b8bbdd6437ea80d01c4cec8516f8) )	/* Release version 2.1a (4/17/98) */

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "329_calif_speed_31.u96", 0x0000, 0x2000, CRC(8b470160) SHA1(4e4cc431432f07423cff2d711ae03de0a4e22f97) )
ROM_END


ROM_START( biofreak )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD( "biofreak.u32", 0x000000, 0x80000, CRC(cefa00bb) SHA1(7e171610ede1e8a448fb8d175f9cb9e7d549de28) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "biofreak.chd", 0, SHA1(711241642f92ded8eaf20c418ea748989183fe10) )	/* Build Date 12/11/97 */
ROM_END


ROM_START( blitz )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version 1.2 */
	ROM_LOAD( "blitz1_2.u32", 0x000000, 0x80000, CRC(38dbecf5) SHA1(7dd5a5b3baf83a7f8f877ff4cd3f5e8b5201b36f) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.21 */
	DISK_IMAGE( "blitz.chd", 0, SHA1(9131c7888e89b3c172780156ed3fe1fe46f78b0a) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "444_blitz.u96", 0x0000, 0x2000, CRC(240c4f08) SHA1(a12b53995679e4ac8bbc8248ed446bf99bbb4ea1) )
ROM_END


ROM_START( blitz99 )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version 1.0 */
	ROM_LOAD( "bltz9910.u32", 0x000000, 0x80000, CRC(777119b2) SHA1(40d255181c2f3a787919c339e83593fd506779a5) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.30 */
	DISK_IMAGE( "blitz99.chd", 0, SHA1(19877e26ffce81dd525031e9e2b4f83ff982e2d9) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "481_blitz-99.u96", 0x0000, 0x2000, CRC(f58df548) SHA1(5bda123035f49f06b4721ab4a1577a115470aa02) )
ROM_END


ROM_START( blitz99a )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version 1.0 */
	ROM_LOAD( "bltz9910.u32", 0x000000, 0x80000, CRC(777119b2) SHA1(40d255181c2f3a787919c339e83593fd506779a5) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.2 (the update ROM rev.-1.3.u33 brings it to 1.30) */
	DISK_IMAGE( "blitz99a.chd", 0, SHA1(43f834727ce01d7a63b482fc28cbf292477fc6f2) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "481_blitz-99.u96", 0x0000, 0x2000, CRC(f58df548) SHA1(5bda123035f49f06b4721ab4a1577a115470aa02) )
ROM_END


ROM_START( blitz2k )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Code Version 1.4 */
	ROM_LOAD( "bltz2k14.u32", 0x000000, 0x80000, CRC(ac4f0051) SHA1(b8125c17370db7bfd9b783230b4ef3d5b22a2025) )

	DISK_REGION( REGION_DISKS )	/* Hard Drive Version 1.5 */
	DISK_IMAGE( "blitz2k.chd", 0, SHA1(e89b7fbd4b4a9854d47ae97493e0afffbd1f69e7) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "494_blitz_2000.u96", 0x0000, 0x2000, CRC(f27b38a4) SHA1(919a772d69d888738dd56c96ed443ccea67049aa) )
ROM_END


ROM_START( carnevil )
	ROM_REGION( 0x800000, REGION_CPU1, 0 )		/* dummy R5000 region */

	ROM_REGION( ADSP2100_SIZE + 0x408000, REGION_CPU2, 0 )	/* ADSP-2115 data Version 1.02 */
	ROM_LOAD( "sound102.u95", ADSP2100_SIZE + 0x000000, 0x8000, CRC(bec7d3ae) SHA1(db80aa4a645804a4574b07b9f34dec6b6b64190d) )

	ROM_REGION32_LE( 0x80000, REGION_USER1, 0 )	/* Boot Rom Version 1.9 */
	ROM_LOAD( "carnevil1_9.u32", 0x000000, 0x80000, CRC(82c07f2e) SHA1(fa51c58022ce251c53bad12fc6ffadb35adb8162) )

	DISK_REGION( REGION_DISKS )
	DISK_IMAGE( "carnevil.chd", 0, BAD_DUMP MD5(6eafae86091c0a915cf8cfdc3d73adc2) SHA1(5e6524d4b97de141c38e301a17e8af15661cb5d6) )

	ROM_REGION( 0x2000, REGION_USER4, ROMREGION_ERASEFF )	/* security PIC (simulated: not needed) */
	ROM_LOAD_OPTIONAL( "486_carnevil.u96", 0x0000, 0x2000, CRC(40eea9d4) SHA1(60a5f5c4de716722fa2ff55dccfc5fbbd7cd02e1) )
ROM_END



/*************************************
 *
 *	Driver init
 *
 *************************************/

static void init_common(int ioasic, int serialnum, int yearoffs)
{
	/* off unless the set's init turns it on */
	seattle_fast_cpu = 0;
	generic_speedup = generic_speedup2 = NULL;
	has_ethernet = 0;

	/* initialize the subsystems */
	ide_controller_init(0, &ide_intf);
	midway_ioasic_init(ioasic, serialnum, yearoffs, ioasic_irq);

	/* copy the boot ROM into its home location */
	memcpy(rombase, memory_region(REGION_USER1), memory_region_length(REGION_USER1));

	/* set the fastest DRC options, but strict verification */
	mips3drc_set_options(0, MIPS3DRC_FASTEST_OPTIONS + MIPS3DRC_STRICT_VERIFY);
}


static DRIVER_INIT( wg3dh )
{
	dcs2_init(0x3839);
	init_common(MIDWAY_IOASIC_STANDARD, 310/* others? */, 80);

	/* speedups */
	install_mem_read32_handler(0, 0x80115e00, 0x80115e03, generic_speedup_r);
	generic_speedup = &rambase[0x115e00/4];
}


static DRIVER_INIT( mace )
{
	dcs2_init(0x3839);
	init_common(MIDWAY_IOASIC_MACE, 319/* or 314 */, 80);

	/* no obvious speedups */
}


/* SF Rush and The Rock: the wheel board, the shifter, the Flagstaff board's ethernet and the analog inputs */
static void init_sfrush_common(void)
{
	midway_ioasic_set_aux_output(wheel_board_w);
	midway_ioasic_set_port_filter(sfrush_port_filter);

	/* the Flagstaff board's ethernet: the race waits on the link even with one cabinet */
	has_ethernet = 1;
	smc91c94_init(ethernet_interrupt);
	install_mem_read32_handler(0, 0xb6c00000, 0xb6c0003f, ethernet_r);
	install_mem_write32_handler(0, 0xb6c00000, 0xb6c0003f, ethernet_w);

	/* set up the analog inputs */
	install_mem_read32_handler(0, 0xb4000000, 0xb4000003, analog_port_r);
	install_mem_write32_handler(0, 0xb4000000, 0xb4000003, analog_port_w);
}


static DRIVER_INIT( sfrush )
{
	cage_init(REGION_USER2, 0x5236);
	init_common(MIDWAY_IOASIC_STANDARD, 315/* no alternates */, 100);
	init_sfrush_common();

	/* speedups */
	install_mem_read32_handler(0, 0x8012498c, 0x8012498f, generic_speedup_r);
	generic_speedup = &rambase[0x12498c/4];
	install_mem_read32_handler(0, 0x80120000, 0x80120003, generic_speedup2_r);
	generic_speedup2 = &rambase[0x120000/4];

	/* direct RAM access and the recompiler: checked on this set */
	seattle_fast_cpu = 1;
}


static DRIVER_INIT( sfrushrk )
{
	cage_init(REGION_USER2, 0x5329);
	init_common(MIDWAY_IOASIC_SFRUSHRK, 331/* no alternates */, 100);
	init_sfrush_common();

	/* direct RAM access and the recompiler: checked on this set */
	seattle_fast_cpu = 1;
}


static DRIVER_INIT( calspeed )
{
	dcs2_init(0x39c0);
	init_common(MIDWAY_IOASIC_CALSPEED, 328/* 27"; as current MAME */, 100);
	midway_ioasic_set_auto_ack(1);

	/* set up the analog inputs */
	install_mem_read32_handler(0, 0xb6c00010, 0xb6c00013, analog_port_r);
	install_mem_write32_handler(0, 0xb6c00010, 0xb6c00013, analog_port_w);
	install_mem_write32_handler(0, 0xb6c0000c, 0xb6c0000f, MWA32_NOP);

	/* speedups */
	install_mem_read32_handler(0, 0x802e6480, 0x802e6483, generic_speedup_r);
	generic_speedup = &rambase[0x2e6480/4];
}


static DRIVER_INIT( biofreak )
{
	dcs2_init(0x3835);
	init_common(MIDWAY_IOASIC_STANDARD, 231/* no alternates */, 80);

	/* speedups */
/*	install_mem_write32_handler(0, 0x802502bc, 0x802502bf, generic_speedup_w);*/
/*	generic_speedup = &rambase[0x2502bc/4];*/
}


static DRIVER_INIT( blitz )
{
	dcs2_init(0x39c2);
	init_common(MIDWAY_IOASIC_BLITZ99, 444/* or 528 */, 80);

	/* for some reason, the code in the ROM appears buggy; this is a small patch to fix it */
	rombase[0x934/4] += 4;

	midway_ioasic_set_port_filter(blitz_port_filter);

	/* speedups */
	install_mem_write32_handler(0, 0x80243d58, 0x80243d5b, generic_speedup_w);
	generic_speedup = &rambase[0x243d58/4];
}


static DRIVER_INIT( blitz99 )
{
	dcs2_init(0x0afb);
	init_common(MIDWAY_IOASIC_BLITZ99, 481/* or 484 or 520 */, 80);

	midway_ioasic_set_aux_output(i40_w);
	midway_ioasic_set_port_filter(blitz99_port_filter);

	/* speedups */
	install_mem_write32_handler(0, 0x802502bc, 0x802502bf, generic_speedup_w);
	generic_speedup = &rambase[0x2502bc/4];
}


static DRIVER_INIT( blitz2k )
{
	dcs2_init(0x0b5d);
	init_common(MIDWAY_IOASIC_BLITZ99, 494/* or 498 */, 80);

	midway_ioasic_set_aux_output(i40_w);
	midway_ioasic_set_port_filter(blitz99_port_filter);

	/* speedups */
	install_mem_write32_handler(0, 0x8024e8d8, 0x8024e8db, generic_speedup_w);
	generic_speedup = &rambase[0x24e8d8/4];
}


static DRIVER_INIT( carnevil )
{
	dcs2_init(0x0af7);
	init_common(MIDWAY_IOASIC_CARNEVIL, 469/* or 486 or 528 */, 80);

	/* set up the gun */
	install_mem_read32_handler(0, 0xb6800000, 0xb680001f, carnevil_gun_r);
	install_mem_write32_handler(0, 0xb6800000, 0xb680001f, carnevil_gun_w);

	/* speedups */
	install_mem_write32_handler(0, 0x801a2bac, 0x801a2baf, generic_speedup_w);
	generic_speedup = &rambase[0x1a2bac/4];

	/* direct RAM access and the recompiler: checked on this set */
	seattle_fast_cpu = 1;
}



/*************************************
 *
 *	Game drivers
 *
 *************************************/

/* Atari */
GAME ( 1996, wg3dh,    0,        phoenix,    wg3dh,    wg3dh,    ROT0, "Atari Games",  "Wayne Gretzky's 3D Hockey (ver 1.3)" )
GAME ( 1996, wg3dhfe,  wg3dh,    phoenix,    wg3dh,    wg3dh,    ROT0, "Atari Games",  "Wayne Gretzky's 3D Hockey Fatality Edition (prototype)" )
GAME ( 1996, mace,     0,        mace,       mace,     mace,     ROT0, "Atari Games",  "Mace: The Dark Age (boot ROM 1.0ce, HDD 1.0b)" )
GAME ( 1997, macea,    mace,     mace,       mace,     mace,     ROT0, "Atari Games",  "Mace: The Dark Age (HDD 1.0a)" )
GAMEC( 1996, sfrush,   0,        flagstaff,  sfrush,   sfrush,   ROT0, "Atari Games",  "San Francisco Rush", &sfrush_ctrl, NULL )
GAMEC( 1997, sfrushrk, 0,        flagstaff,  sfrush,   sfrushrk, ROT0, "Atari Games",  "San Francisco Rush: The Rock (boot rom L 1.0, GUTS Oct 6 1997 / MAIN Oct 16 1997)", &sfrush_ctrl, NULL )
GAME ( 1998, calspeed, 0,        seattle150, calspeed, calspeed, ROT0, "Atari Games",  "California Speed (Version 2.1a Apr 17 1998, GUTS 1.25 Apr 17 1998 / MAIN Apr 17 1998)" )

/* Midway */
GAMEC( 1997, biofreak, 0,        seattle150, biofreak, biofreak, ROT0, "Midway Games", "Bio F.R.E.A.K.S (prototype, boot ROM 0.1i)", &biofreak_ctrl, NULL )
GAME ( 1997, blitz,    0,        blitz,      blitz,    blitz,    ROT0, "Midway Games", "NFL Blitz (ver 1.21, Dec 5 1997)" )
GAME ( 1998, blitz99,  0,        blitz,      blitz99,  blitz99,  ROT0, "Midway Games", "NFL Blitz '99 (ver 1.30, Sep 22 1998)" )
GAME ( 1998, blitz99a, blitz99,  blitz,      blitz99,  blitz99,  ROT0, "Midway Games", "NFL Blitz '99 (ver 1.2, Aug 28 1998)" )
GAME ( 1999, blitz2k,  0,        seattle150, blitz99,  blitz2k,  ROT0, "Midway Games", "NFL Blitz 2000 Gold Edition (ver 1.2, Sep 22 1999)" )
GAMEC( 1998, carnevil, 0,        carnevil,   carnevil, carnevil, ROT0, "Midway Games", "CarnEvil", &carnevil_ctrl, &carnevil_bootstrap )
