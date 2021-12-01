/**************************************************************************
 *
 * konamigx.c - Konami System GX
 * Driver by R. Belmont, Acho A. Tang, and Phil Stroffolino.
 * ESC protection emulation and TMS57002 skipper by Olivier Galibert.
 *
 * Basic hardware consists of:
 * - MC68EC020 main CPU at 24 MHz
 * - MC68000 sound CPU with 2x K054539 PCM chips plus a TMS57002 DASP for effects
 *
 * Tilemaps are handled by the old familiar K054156 with a new partner K056832.
 * This combination is mostly compatible with the 156/157 combo but now sports
 * up to 8 bits per pixel as well as larger 128x64 tile planes.
 *
 * Sprites are handled by a K053246 combined with a K055673.  This combo was
 * actually used on some 16 bit Konami games as well.  It appears identical
 * to the standard 246/247 combo except with 5-8 bits per pixel.
 *
 * Video output is handled by a K055555 mixer/priority encoder.  This is a much
 * more capable beast than the earlier 53251, supporting a background gradient
 * and flexible priority handling among other features.  It's combined with the
 * 54338 alpha blending engine first seen in Xexex for flexible blending.
 *
 * There are actually 4 types of System GX hardware, which are differentiated
 * by their ROM board.  The most common is the "Type 2".
 *
 * 68020 memory map for Type 2:
 * 000000: BIOS ROM (128k)
 * 200000: game program ROM (1 meg, possibly up to 2)
 * 400000: data ROMs
 * c00000: Work RAM (128k)
 * cc0000: Protection chip
 * d00000: 054157 ROM readback for memory test
 * d20000: sprite RAM (4k)
 * d40000: 054157/056832 tilemap generator    (VACSET)
 * d44000: tile bank selectors	              (VSCCS)
 * d48000: 053246/055673 sprite generator     (OBJSET1)
 * d4a000: more readback for sprite generator (OBJSET2)
 * d4c000: CCU1 registers                     (CCUS1)
 * 00: HCH			  6M/288   8M/384   12M/576	       224 256
 * 02: HCL		   HCH 	    01      01        02	  VCH  01  01
 * 04: HFPH		   HCL      7f      ff        ff	  VCL  07  20
 * 06: HFPL		   HFPH	    00      00        00	  VFP  11  0c
 * 08: HBPH		   HFPL	    10      19        23	  VBP  0e  0e
 * 0A: HBPL		   HBPH	    00      00        00	  VSW  07  05
 * 10: VCH		   HBPL	    30      3f        4d
 * 12: VCL		   HSW	    03      04        09
 * 14: VFP
 * 16: VBP
 * 18: VSW/HSW
 * 1A: INT TIME
 * 1C: INT1ACK (read VCTH) INT1 = V blank
 * 1E: INT2ACK (read VCTL) INT2 = H blank
 * d4e000: CCU2 registers		    (CCUS2)
 * d50000: K055555 8-bit-per-pixel priority encoder (PCUCS)
 * d52000: shared RAM with audio 68000      (SOUNDCS)
 * d56000: EEPROM comms, bit 7 is watchdog, bit 5 is frame?? (WRPOR1)
 * d56001: IRQ acknowledge (bits 0->3 = irq 1->4), IRQ enable in hi nibble (bits 4->7 = irq 1->4)
 * d58000: control register (OBJCHA, 68000 enable/disable, probably more) (WRPOR2)
 * d5a000: dipswitch bank 1		    (RDPORT1)
 * d5a001: dipswitch bank 2
 * d5a002: input port (service switch)
 * d5a003: EEPROM data/ready in bit 0
 * d5c000: player 1 inputs		    (RDPORT2)
 * d5c001: player 2 inputs
 * d5e000: test switch			    (RDPORT3)
 * d80000: K054338 alpha blender registers
 * d90000: palette RAM for tilemaps
 * da0000: tilemap RAM (8k window, bankswitched by d40033)
 * dc0000: LANCRAMCS
 * dd0000: LANCIOCS
 * dd2000: RS232C-1
 * dd4000: RS232C-2
 * dd6000: trackball 1
 * dd8000: trackball 2
 * dda000: ADC-WRPORT
 * ddc000: ADC-RDPORT
 * dde000: EXT-WRPORT
 * de0000: EXT_RDPORT
 * e00000: type3/4: PSAC2 registers
 * e20000: type3/4: unk. register
 * e40000: type3/4: unk. register
 * e60000: type3/4: PSAC2 linecontrol RAM
 * e80000: type3/4: main monitor's palette
 * ea0000: type3/4: sub monitor's palette
 * ec0000: type3/4: frame flag
 * f00000: 32k of RnG2 additional RAM
 *
 * IRQs:
 * 1: VBL (at 60 Hz)
 * 2: HBL
 * 3: Sprite DMA complete
 * 4: from protection device, indicates command completion for "ESC" chip
 *
 */

#define GX_DEBUG     0
#define GX_SKIPIDLE  1

#include "driver.h"
#include "state.h"

#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"
#include "cpu/m68000/m68000.h"
/* #include "cpu/z80/z80.h" */
#include "machine/eeprom.h"
#include "sound/k054539.h"
#include "machine/konamigx.h"

VIDEO_START(konamigx_5bpp);
VIDEO_START(konamigx_6bpp);
VIDEO_START(konamigx_6bpp_2);
VIDEO_START(konamigx_type3);
VIDEO_START(konamigx_type4);
VIDEO_START(le2);
VIDEO_START(dragoonj);
VIDEO_START(winspike);
VIDEO_START(opengolf);
VIDEO_START(racinfrc);
VIDEO_UPDATE(konamigx);

MACHINE_INIT(konamigx);

WRITE32_HANDLER( konamigx_palette_w );
WRITE32_HANDLER( konamigx_palette2_w );
WRITE32_HANDLER( konamigx_555_palette_w );
WRITE32_HANDLER( konamigx_555_palette2_w );
WRITE32_HANDLER( konamigx_tilebank_w );

data32_t *gx_psacram, *gx_subpaletteram32;
WRITE32_HANDLER( konamigx_t1_psacmap_w );
WRITE32_HANDLER( konamigx_t4_psacmap_w );

int konamigx_cfgport;

static data32_t *gx_workram; /* workram pointer for ESC protection fun */
static data16_t *gx_sndram;
static int gx_rdport1_3, gx_syncen;

static void *dmadelay_timer;

/**********************************************************************************/
/*
   Konami ESC (E Security Chip) protection chip found on:
	- Salamander 2
	- Sexy Parodius
	- Twinbee Yahhoo
	- Dragoon Might
	- Daisu-Kiss

   The ESC is a custom microcontroller with external SRAM connected
   to it.  It's microprogram is uploaded by the host game.  The ESC
   has complete DMA access to the entire 68020 address space, like
   most Konami protection devices.

   Most games use the same or a compatible microprogram.  This
   program gathers the sprite information scattered around work RAM
   and builds a sprite list in sprite RAM which is properly sorted
   in priority order.  The Japanese version of Gokujou Parodius
   contains a 68020 version of this program at 0x2a285c.  The code
   below is almost a direct translation into C of that routine.

   Salamander 2 uses a different work RAM format and a different
   ESC program, and Dragoon Might uses a third format and program.
   These games haven't been reverse-engineered yet and so have
   no sprites.

   Winning Spike and the Type 4 games use a new protection chip
   made from a Xilinx FPGA.  See type4_prot_w for details.

*/

/* constant names mostly taken from sexy parodius, which includes
   a debug protection routine with asserts intact!  Cracking the ESC
   would have been much more difficult without it.
 */

#define ESC_INIT_CONSTANT	0x0108db04
#define ESC_OBJECT_MAGIC_ID	0xfef724fb
#define ESTATE_END	2
#define ESTATE_ERROR	3

/*    opcode 1*/
/* dragoonj*/
/*  ESC Opcode 1 : 000000xx 000000xx 00000xx0 00000000*/
/* tbyahhoo  sprites at c00000*/
/*  ESC Opcode 1 : 0000ffff 0000ffff 0000ffff 0000000e*/
/* sexyparo  sprites at c00604*/
/*  ESC Opcode 1 : 00000000 00000000 00d20000 00000000*/
/*  ESC Opcode 1 : 00000000 00000000 00d21000 00000000*/
/* salmndr2*/
/*  ESC Opcode 1 : 00c1f7f8 0022002c 00c00060 00010014*/
/* tokkae*/
/*  ESC Opcode 1 : 00c00000 0000000x 00000023 0000ffff*/
/* puzldama*/
/*  ESC Opcode 1 : 002xxxxx 00236102 xxxxxx01 xxxxxxxx*/

/* Say hello to gokuparo at 0x2a285c*/

static struct sprite_entry {
	int pri;
	unsigned int adr;
} sprites[0x100];

static void generate_sprites(UINT32 src, UINT32 spr, int count)
{
	int i;
	int scount;
	int ecount;
	scount = 0;
	ecount = 0;

	for(i=0; i<count; i++) {
		unsigned int adr = src + 0x100*i;
		int pri;
		if(!cpu_readmem24bedw_word(adr+2))
			continue;
		pri = cpu_readmem24bedw_word(adr+28);

		if(pri < 256) {
			sprites[ecount].pri = pri;
			sprites[ecount].adr = adr;
			ecount++;
		}
	}
	/*qsort(sprites, ecount, sizeof(struct sprite_entry), pri_comp);*/

	for(i=0; i<ecount; i++) {
		unsigned int adr = sprites[i].adr;
		if(adr) {
			unsigned int set =(cpu_readmem24bedw_word(adr) << 16)|cpu_readmem24bedw_word(adr+2);
			unsigned short glob_x = cpu_readmem24bedw_word(adr+4);
			unsigned short glob_y = cpu_readmem24bedw_word(adr+8);
			unsigned short flip_x = cpu_readmem24bedw_word(adr+12) ? 0x1000 : 0x0000;
			unsigned short flip_y = cpu_readmem24bedw_word(adr+14) ? 0x2000 : 0x0000;
			unsigned short glob_f = flip_x | (flip_y ^ 0x2000);
			unsigned short zoom_x = cpu_readmem24bedw_word(adr+20);
			unsigned short zoom_y = cpu_readmem24bedw_word(adr+22);
			unsigned short color_val    = 0x0000;
			unsigned short color_mask   = 0xffff;
			unsigned short color_set    = 0x0000;
			unsigned short color_rotate = 0x0000;
			unsigned short v;

			v = cpu_readmem24bedw_word(adr+24);
			if(v & 0x8000) {
				color_mask = 0xf3ff;
				color_val |= (v & 3) << 10;
			}

			v = cpu_readmem24bedw_word(adr+26);
			if(v & 0x8000) {
				color_mask &= 0xfcff;
				color_val  |= (v & 3) << 8;
			}

			v = cpu_readmem24bedw_word(adr+18);
			if(v & 0x8000) {
				color_mask &= 0xff1f;
				color_val  |= v & 0xe0;
			}

			v = cpu_readmem24bedw_word(adr+16);
			if(v & 0x8000)
				color_set = v & 0x1f;
			if(v & 0x4000)
				color_rotate = v & 0x1f;

			if(!zoom_x)
				zoom_x = 0x40;
			if(!zoom_y)
				zoom_y = 0x40;

			if(set >= 0x200000 && set < 0xd00000)
			{
				unsigned short count2 = cpu_readmem24bedw_word(set);
				set += 2;
				while(count2) {
					unsigned short idx  = cpu_readmem24bedw_word(set);
					unsigned short flip = cpu_readmem24bedw_word(set+2);
					unsigned short col  = cpu_readmem24bedw_word(set+4);
					short y = cpu_readmem24bedw_word(set+6);
					short x = cpu_readmem24bedw_word(set+8);

					if(idx == 0xffff) {
						set = (flip<<16) | col;
						if(set >= 0x200000 && set < 0xd00000)
							continue;
						else
							break;
					}

					if(zoom_y != 0x40)
						y = y*0x40/zoom_y;
					if(zoom_x != 0x40)
						x = x*0x40/zoom_x;

					if(flip_x)
						x = glob_x - x;
					else
						x = glob_x + x;
					if(x < -256 || x > 512+32)
						goto next;

					if(flip_y)
						y = glob_y - y;
					else
						y = glob_y + y;
					if(y < -256 || y > 512)
						goto next;

					col = (col & color_mask) | color_val;
					if(color_set)
						col = (col & 0xffe0) | color_set;
					if(color_rotate)
						col = (col & 0xffe0) | ((col + color_rotate) & 0x1f);

					cpu_writemem24bedw_word(spr   , (flip ^ glob_f) | sprites[i].pri);
					cpu_writemem24bedw_word(spr+ 2, idx);
					cpu_writemem24bedw_word(spr+ 4, y);
					cpu_writemem24bedw_word(spr+ 6, x);
					cpu_writemem24bedw_word(spr+ 8, zoom_y);
					cpu_writemem24bedw_word(spr+10, zoom_x);
					cpu_writemem24bedw_word(spr+12, col);
					spr += 16;
					scount++;
					if(scount == 256)
						return;
				next:
					count2--;
					set += 10;
				}
			}
		}
	}
	while(scount < 256) {
		cpu_writemem24bedw_word(spr, scount);
		scount++;
		spr += 16;
	}
}

static void tkmmpzdm_esc(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4)
{
	konamigx_esc_alert(gx_workram, 0x0142, 0x100, 0);
}

static void dragoonj_esc(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4)
{
	konamigx_esc_alert(gx_workram, 0x5c00, 0x100, 0);
}

static void sal2_esc(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4)
{
	konamigx_esc_alert(gx_workram, 0x1c8c, 0x172, 1);
}

static void sexyparo_esc(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4)
{
	/* The d20000 should probably be p3*/
	generate_sprites(0xc00604, 0xd20000, 0xfc);
}

static void tbyahhoo_esc(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4)
{
	generate_sprites(0xc00000, 0xd20000, 0x100);
}

static void daiskiss_esc(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4)
{
	generate_sprites(0xc00000, 0xd20000, 0x100);
}

static unsigned char esc_program[4096];
static void (*esc_cb)(UINT32 p1, UINT32 p2, UINT32 p3, UINT32 p4);

static WRITE32_HANDLER( esc_w )
{
	unsigned int opcode;
	unsigned int params;

	/* ignore NULL writes to the ESC (these appear to be "keepalives" on the real hardware) */
	if (!data)
	{
		return;
	}

	/* ignore out-of-range addresses */
	if ((data < 0xc00000) || (data > 0xc1ffff))
	{
		return;
	}

	/* the master opcode can be at an unaligned address, so get it "safely" */
	opcode = (cpu_readmem24bedw_word(data+2))|(cpu_readmem24bedw_word(data)<<16);

	/* if there's an OBJECT_MAGIC_ID, that means
	   there is a valid ESC command packet. */
	if (opcode == ESC_OBJECT_MAGIC_ID)
	{
		int i;
		/* get the subop now */
		opcode = cpu_readmem24bedw(data+8);
		params = (cpu_readmem24bedw_word(data+12) << 16) | cpu_readmem24bedw_word(data+14);

		switch(opcode) {
		case 5: /* Reset*/
			break;
		case 2: /* Load program*/
			for(i=0; i<4096; i++)
				esc_program[i] = cpu_readmem24bedw(params+i);
/*
			{
				FILE *f;

				f = fopen("esc.bin", "wb");
				fwrite(esc_program, 4096, 1, f);
				fclose(f);

				log_cb(RETRO_LOG_DEBUG, LOGPRE "Dumping ESC program\n");
			}
*/
			break;
		case 1: /* Run program*/
			if(esc_cb) {
				UINT32 p1 = (cpu_readmem24bedw_word(params+0)<<16) | cpu_readmem24bedw_word(params+2);
				UINT32 p2 = (cpu_readmem24bedw_word(params+4)<<16) | cpu_readmem24bedw_word(params+6);
				UINT32 p3 = (cpu_readmem24bedw_word(params+8)<<16) | cpu_readmem24bedw_word(params+10);
				UINT32 p4 = (cpu_readmem24bedw_word(params+12)<<16) | cpu_readmem24bedw_word(params+14);
				esc_cb(p1, p2, p3, p4);
			}
			break;
		default:
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "Unknown ESC opcode %d\n", opcode);*/
			break;
		}
		cpu_writemem24bedw(data+9, ESTATE_END);

		if (konamigx_wrport1_1 & 0x10)
		{
			gx_rdport1_3 &= ~8;
			cpu_set_irq_line(0, 4, HOLD_LINE);
		}
	}
	else
	{
		/* INIT_CONSTANT means just for the ESC to initialize itself,
		   there is not normal command parsing here. */
		if (opcode == ESC_INIT_CONSTANT)
		{
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "Got ESC_INIT_CONSTANT, 'booting' ESC\n");*/
			return;
		}

		/* unknown constant (never been seen in any game..) */
	}
}


/**********************************************************************************/
/* NVRAM and EEPROM handlers */

static int init_eeprom_count;

static NVRAM_HANDLER(konamigx_93C46)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface_93C46);

		if (file)
		{
			init_eeprom_count = 0;
			EEPROM_load(file);
		}
		else
			init_eeprom_count = 10;
	}
}

static READ32_HANDLER( eeprom_r )
{
	/* d5a000 = DIP switches #1 (RDPORT1)*/
	/* d5a001 = DIP switches #2*/
	/* d5a002 = input port: service4/3/2/1 coin 4/3/2/1*/
	/* d5a003 = objint stat, exioint stat, trackint stat, excgint stat, escint stat,*/
	/*	    excpuint stat, objdma stat, eeprom do*/

	/* note: racin' force expects bit 1 of the eeprom port to toggle*/
	return(readinputport(6)<<24 | readinputport(7)<<16 | readinputport(0)<<8 | EEPROM_read_bit() | gx_rdport1_3);
}

static WRITE32_HANDLER( eeprom_w )
{
	data32_t odata;

	if (!(mem_mask & 0xff000000))
	{
		odata = data >> 24;
		/*
		  bit 7: afr
		  bit 6: objscan
		  bit 5: background color select: 0 = 338 solid color, 1 = 5^5 gradient
		  bit 4: coin counter 2
		  bit 3: coin counter 1
		  bit 2: eeprom clock
		  bit 1: eeprom chip select
		  bit 0: eeprom data
		*/

		EEPROM_write_bit((odata&0x01) ? 1 : 0);
		EEPROM_set_cs_line((odata&0x02) ? CLEAR_LINE : ASSERT_LINE);
		EEPROM_set_clock_line((odata&0x04) ? ASSERT_LINE : CLEAR_LINE);

		konamigx_wrport1_0 = odata;
	}

	if (!(mem_mask & 0xff0000))
	{
		/*
		  bit 7 = mask all IRQ
		  bit 6 = LAN IRQ enable
		  bit 5 = CCU2 IRQ enable
		  bit 4 = ESC IRQ enable
		  bit 3 = EXCPU IRQ enable
		  bit 2 = OBJ IRQ enable
		  bit 1 = CCU1-INT2 enable
		  bit 0 = CCU1-INT1 enable
		*/

		konamigx_wrport1_1 = (data>>16)&0xff;
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "write %x to IRQ register (PC=%x)\n", konamigx_wrport1_1, activecpu_get_pc());*/

		/* gx_syncen is to ensure each IRQ is trigger at least once after being enabled*/
		if (konamigx_wrport1_1 & 0x80) gx_syncen |= konamigx_wrport1_1 & 0x1f;
	}
}

static WRITE32_HANDLER( control_w )
{
	/*logerror("write %x to control register (mask=%x)\n", data, mem_mask);*/

	/* known controls:*/
	/* bit 23 = reset graphics chips*/
	/* bit 22 = 0 to halt 68000, 1 to let it run (SOUNDRESET)*/
	/* bit 21 = VRAM-CHARD 0=VRAM, 1=ROM*/
	/* bit 20 = OBJCHA line for '246*/
	/* bit 19 = select top 2 sprite priority bits to be 14/15 or 16/17 of the*/
	/*          spritelist "color" word.*/
	/* bit 18 = if 0, the top 2 sprite priority bits are "11" else use bit 19's*/
	/*          results.*/
	/* bit 17 = DOTSEL1 : 0 = 6M, 1=8M, 2=12M, 3=16M*/
	/* bit 16 = DOTSEL0*/
	if (!(mem_mask & 0x00ff0000))
	{
		if (data & 0x400000)
		{
			/* enable 68k*/
			/* clear the halt condition and reset the 68000*/
			cpu_set_halt_line(1, CLEAR_LINE);
			cpu_set_reset_line(1, PULSE_LINE);
		}
		else
		{
			/* disable 68k*/
			cpu_set_halt_line(1, ASSERT_LINE);
		}

		K053246_set_OBJCHA_line((data&0x100000) ? ASSERT_LINE : CLEAR_LINE);

		konamigx_wrport2 = (data>>16)&0xff;
	}
}


/**********************************************************************************/
/* IRQ controllers */

#define ADD_SKIPPER32(PC, BASE, START, END, DATA, MASK){ \
  waitskip.pc   = PC;        \
  waitskip.offs = START/4;   \
  waitskip.data = DATA;      \
  waitskip.mask = MASK;      \
  resume_trigger= 1000;      \
  install_mem_read32_handler \
  (0, (BASE+START)&~3, (BASE+END)|3, waitskip_r);}

static struct { UINT32 offs, pc, mask, data; } waitskip;
static int suspension_active, resume_trigger;

static READ32_HANDLER(waitskip_r)
{
	data32_t data = gx_workram[waitskip.offs+offset];
	mem_mask = ~mem_mask;

	if (activecpu_get_pc() == waitskip.pc && (data & mem_mask) == (waitskip.data & mem_mask))
	{
		cpu_spinuntil_trigger(resume_trigger);
		suspension_active = 1;
	}

	return(data);
}


static READ32_HANDLER( ccu_r )
{
	/* the routine at 204abe in opengolf polls to see if we're in vblank (it wants values between 0x111 and 0x1df)*/
	if (offset == 0x1c/4)
	{
		return 0x01002000;
	}
	else
	{
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "Read unhandled CCU register %x\n", offset);*/
	}

	return 0;
}

static WRITE32_HANDLER( ccu_w )
{
	if (offset == 0x1c/4)
	{
		/* vblank interrupt ACK*/
		if (!(mem_mask & 0xff000000))
		{
			cpu_set_irq_line(0, 1, CLEAR_LINE);
			gx_syncen |= 0x20;
		}

		/* hblank interrupt ACK*/
		if (!(mem_mask & 0x0000ff00))
		{
			cpu_set_irq_line(0, 2, CLEAR_LINE);
			gx_syncen |= 0x40;
		}
	}
}


/*
	GX object DMA timings:

	 6Mhz dotclock: 42.7us(clear) / 341.3us(transfer)
	 8Mhz dotclock: 32.0us(clear) / 256.0us(transfer)
	12Mhz dotclock: 42.7us(clear) / 341.3us(transfer)
*/

static void dmaend_callback(int data)
{
	/* foul-proof (CPU0 could be deactivated while we wait)*/
	if (resume_trigger && suspension_active) { suspension_active = 0; cpu_trigger(resume_trigger); }

	/* DMA busy flag must be cleared before triggering IRQ 3*/
	gx_rdport1_3 &= ~2;

	/* IRQ 3 is the "object DMA end" IRQ also happens during vblank*/
	if ((konamigx_wrport1_1 & 0x84) == 0x84 || (gx_syncen & 4))
	{
		gx_syncen &= ~4;

		/* lower OBJINT-REQ flag and trigger interrupt*/
		gx_rdport1_3 &= ~0x80;
		cpu_set_irq_line(0, 3, HOLD_LINE);
	}
}

static void dmastart_callback(int data)
{
	/* raise the DMA busy flag*/
	gx_rdport1_3 |= 2;

	/* begin transfer if DMAEN(bit4 of OBJSET1) is set (see p.48)*/
	if (K053246_read_register(5) & 0x10)
	{
		/* disabled by default since it doesn't work too well in MAME*/
		konamigx_objdma();
	}

	/* simulate DMA delay*/
	timer_adjust(dmadelay_timer, TIME_IN_USEC(120), 0, 0);
}


static INTERRUPT_GEN(konamigx_vbinterrupt)
{
	/* lift idle suspension*/
	if (resume_trigger && suspension_active) { suspension_active = 0; cpu_trigger(resume_trigger); }

	/* IRQ 1 is the main 60hz vblank interrupt*/
	if (gx_syncen & 0x20)
	{
		gx_syncen &= ~0x20;

		if ((konamigx_wrport1_1 & 0x81) == 0x81 || (gx_syncen & 1))
		{
			gx_syncen &= ~1;
			cpu_set_irq_line(0, 1, HOLD_LINE);
		}
	}

	dmastart_callback(0);
}

static INTERRUPT_GEN(konamigx_vbinterrupt_type4)
{
	/* lift idle suspension*/
	if (resume_trigger && suspension_active) { suspension_active = 0; cpu_trigger(resume_trigger); }

	/* IRQ 1 is the main 60hz vblank interrupt*/
	/* the gx_syncen & 0x20 test doesn't work on type 3 or 4 ROM boards, likely because the ROM board*/
	/* generates the timing in those cases.  With this change, rushing heroes and rng2 boot :)*/
	if (1) /* gx_syncen & 0x20)*/
	{
		gx_syncen &= ~0x20;

		if ((konamigx_wrport1_1 & 0x81) == 0x81 || (gx_syncen & 1))
		{
			gx_syncen &= ~1;
			cpu_set_irq_line(0, 1, HOLD_LINE);
		}
	}

	dmastart_callback(0);
}

static INTERRUPT_GEN(konamigx_hbinterrupt)
{
	if (!cpu_getiloops())
	{
		konamigx_vbinterrupt_type4();
	}
	else	/* hblank*/
	{
		/* IRQ 2 is a programmable interrupt with scanline resolution*/
		if (gx_syncen & 0x40)
		{
			gx_syncen &= ~0x40;

			if ((konamigx_wrport1_1 & 0x82) == 0x82 || (gx_syncen & 2))
			{
				gx_syncen &= ~2;
				cpu_set_irq_line(0, 2, HOLD_LINE);
			}
		}
	}
}


/**********************************************************************************/
/* sound communication handlers */

static data8_t sndto000[16], sndto020[16];	/* read/write split mapping */
static int snd020_hack;

static READ32_HANDLER( sound020_r )
{
	data32_t reg, MSW, LSW, rv = 0;

	reg = offset << 1;

	if (!(mem_mask & 0xff000000))
	{
		MSW = sndto020[reg];
		if (reg == 2) MSW &= ~3; /* supress VOLWR busy flags*/
		rv |= MSW<<24;
	}

	if (!(mem_mask & 0x0000ff00))
	{
		LSW = sndto020[reg+1];
		rv |= LSW<<8;
	}

/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "Read 68k @ %x (PC=%x)\n", reg, activecpu_get_pc());*/

	/* we clearly have some problem because some games require these hacks*/
	/* perhaps 68000/68020 timing is skewed?*/
	switch (snd020_hack)
	{
		case 1: /* Lethal Enforcer init*/
			if (reg == 0) rv |= 0xff00;
			break;
		case 2: /* Winning Spike*/
			if (activecpu_get_pc() == 0x2026fe) rv = 0xc0c0c0c0;
			break;
		case 3: /* Run'n Gun 2*/
			if (activecpu_get_pc() == 0x24f122) rv = 0xc0c0c0c0;
			break;
		case 4:	/* Rushing Heroes*/
			if (activecpu_get_pc() == 0x20eda6) rv = 0xc0c0c0c0;
			break;
		case 5:	/* Vs. Net Soccer*/
			if (activecpu_get_pc() == 0x24c63e) rv = 0xc0c0c0c0;
			break;
		case 6: /* Slam Dunk 2*/
			if (activecpu_get_pc() == 0x24f21c) rv = 0xc0c0c0c0;
			break;
	}

	return(rv);
}

static INLINE void write_snd_020(int reg, int val)
{
	sndto000[reg] = val;

	if (reg == 7)
	{
		cpu_set_irq_line(1, 1, HOLD_LINE);
	}
}

static WRITE32_HANDLER( sound020_w )
{
	int reg=0, val=0;

	if (!(mem_mask & 0xff000000))
	{
		reg = offset<<1;
		val = data>>24;
		write_snd_020(reg, val);
	}

	if (!(mem_mask & 0x0000ff00))
	{
		reg = (offset<<1)+1;
		val = (data>>8)&0xff;
		write_snd_020(reg, val);
	}
}


/**********************************************************************************/
/* input handlers */

/* National Semiconductor ADC0834 4-channel serial ADC emulation */
static int analog_prevclk;
static int analog_val = 0;
static int analog_latch;
static int analog_state = 0;
static int analog_cmd = 0;

static READ32_HANDLER( adc0834_r )
{
	return analog_val;
}

static WRITE32_HANDLER( adc0834_w )
{
	int clk, cs, di;

	clk = (data >> 24)&1;		/* clock*/
	di = (data >> 25)&1;		/* data in*/
	cs = ((data >> 26)^1)&1;	/* chip select*/

	/* resync states if CS drops*/
	if (!cs)
	{
		analog_state = 0;
		analog_prevclk = 0;
		return;
	}

	/* CS is up, we act on the rising edge of CLK*/
	if ((clk) && (!analog_prevclk))
	{
		switch (analog_state)
		{
			case 0:	  /* wait for addr*/
			case 1:
			case 2:
			case 3:
			case 4:
			  	analog_cmd <<= 1;
				analog_cmd |= di;
			      	analog_state++;
				break;

			case 5:	/* got command, latch in the proper analog read*/
				if (analog_cmd & 4)
				{
					analog_latch = readinputport(10);	/* gas*/
				}
				else
				{
					analog_latch = readinputport(9);	/* steer*/
				}
				analog_val = (analog_latch & 0x80)<<17;
				analog_latch <<= 1;
				analog_state++;
				break;

			case 6:	/* clock out the data*/
				analog_val = (analog_latch & 0x80)<<17;
				analog_latch <<= 1;
				break;
		}
	}

	analog_prevclk = clk;
}

static READ32_HANDLER( le2_gun_H_r )
{
	int p1x = readinputport(9)*290/0xff+20;
	int p2x = readinputport(11)*290/0xff+20;

	/* make "off the left" reload too */
	if (p1x <= 0x15) p1x = 310;
	if (p2x <= 0x15) p2x = 310;

	return (p1x<<16)|p2x;
}

static READ32_HANDLER( le2_gun_V_r )
{
	int p1y = readinputport(10)*224/0xff;
	int p2y = readinputport(12)*224/0xff;

	/* make "off the bottom" reload too */
	if (p1y >= 0xdf) p1y = 0;
	if (p2y >= 0xdf) p2y = 0;

	return (p1y<<16)|p2y;
}

static READ32_HANDLER( service_r )
{
	int res = (readinputport(1)<<24) | (readinputport(8)<<8);

	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= ~0x08000000;
	}

	return res;
}

static READ32_HANDLER( players_r )
{
	return (readinputport(2)<<24) | (readinputport(3)<<16) | (readinputport(4)<<8) | (readinputport(5));
}


/**********************************************************************************/
/* system or game dependent handlers */

static READ32_HANDLER( gx5bppspr_r )
{
	return (K055673_rom_word_r(offset*2+1, 0xffff) | K055673_rom_word_r(offset*2, 0xffff)<<16);
}

static READ32_HANDLER( gx6bppspr_r )
{
	return (K055673_GX6bpp_rom_word_r(offset*2+1, 0xffff) | K055673_GX6bpp_rom_word_r(offset*2, 0xffff)<<16);
}

static READ32_HANDLER( type1_roz_r1 )
{
	data32_t *ROM = (data32_t *)memory_region(REGION_GFX3);

	return ROM[offset];
}

static READ32_HANDLER( type1_roz_r2 )
{
	data32_t *ROM = (data32_t *)memory_region(REGION_GFX3);

	ROM += (0x600000/2);

	return ROM[offset];
}

static int sync_frame = 0;

static READ32_HANDLER( type3_sync_r )
{
	sync_frame ^= 0xffffffff;

	return sync_frame;
}

static int last_prot_op = -1, last_prot_clk;

/*
	Run and Gun 2, Rushing Heroes, Winning Spike, and Vs. Net Soccer contain a XILINX FPGA that serves as security.

	RnG2's version is stamped "K002204", while Rushing Heroes' is "K0000035891".  Vs Net's is unknown at this time.
	Winning Spike's is "0000032652".

	RnG2's is used to generate the sprite list just like the ESC, among other tasks.  (RnG2 sends many commands per frame to the protection).

	Rushing Heroes is much simpler and uses only 1 command during gameplay.  They set up a giant table of pointers
	at C10200->C102EF (see the routine at 2043CE.  C10001 contains which monitor they want to update (main or sub)
	and it changes the pointers accordingly).  This sets up the palettes, the sprite list, and the ROZ tilemap, from the looks of things.

	Here are the lists constructed by Rushing Heroes (I've reordered the original code so it's in linear address order).
	Vs. Net Soccer does the same thing starting at 2064DC (in fact, Rushing Heroes appears to be heavily based on Vs. Net's code).

	    main monitor (2043da)            sub monitor (204616)
	move.l  #$C0C000,($C10200).l	   move.l  #$C1C000,($C10200).l
	move.l  #$C10000,($C10204).l	   move.l  #$C20000,($C10204).l
	move.l  #$C0C000,($C10208).l	   move.l  #$C1C000,($C10208).l
	move.l  #$C0C200,($C1020C).l	   move.l  #$C1C200,($C1020C).l
	move.l  #$C0C200,($C10210).l	   move.l  #$C1C200,($C10210).l
	move.l  #$C0D800,($C10214).l	   move.l  #$C1D800,($C10214).l
	move.l  #$C0D800,($C10218).l	   move.l  #$C1D800,($C10218).l
	move.l  #$C0EE00,($C1021C).l	   move.l  #$C1EE00,($C1021C).l
	move.l  #$C0EE00,($C10220).l	   move.l  #$C1EE00,($C10220).l
	move.l  #$C0F200,($C10224).l	   move.l  #$C1F200,($C10224).l
	move.l  #$C0EE00,($C10228).l	   move.l  #$C1EE00,($C10228).l
	move.l  #$C0EE00,($C1022C).l	   move.l  #$C1EE00,($C1022C).l
	move.l  #$C0F000,($C10230).l	   move.l  #$C1F000,($C10230).l
	move.l  #$C1C200,($C10234).l	   move.l  #$C0C200,($C10234).l
	move.l  #$C1EE00,($C10238).l	   move.l  #$C0EE00,($C10238).l
	move.l  #$C0A000,($C1023C).l	   move.l  #$C1A000,($C1023C).l
	move.l  #$C0C000,($C10240).l	   move.l  #$C1C000,($C10240).l
	move.l  #$C0A000,($C10244).l	   move.l  #$C1A000,($C10244).l
	move.l  #$C10000,($C10248).l	   move.l  #$C20000,($C10248).l
	move.l  #$C0FA00,($C1024C).l	   move.l  #$C1FA00,($C1024C).l
	move.l  #$C1A000,($C10250).l	   move.l  #$C0A000,($C10250).l
	move.l  #$C00000,($C10260).l	   move.l  #$C10000,($C10260).l
	move.l  #$C10000,($C10264).l	   move.l  #$C00000,($C10264).l
	move.l  #$C00800,($C10268).l	   move.l  #$C10800,($C10268).l
	move.l  #$C01000,($C1026C).l	   move.l  #$C11000,($C1026C).l
	move.l  #$C00500,($C10270).l	   move.l  #$C10500,($C10270).l
	move.l  #$C00520,($C10274).l	   move.l  #$C10520,($C10274).l
	move.l  #$C00540,($C10278).l	   move.l  #$C10540,($C10278).l
	move.l  #$C00560,($C1027C).l	   move.l  #$C10560,($C1027C).l
	move.l  #$C100C0,($C10280).l	   move.l  #$C100E0,($C10280).l
	move.l  #$C100D0,($C10284).l	   move.l  #$C100F0,($C10284).l
	move.l  #$C100E0,($C10288).l	   move.l  #$C100C0,($C10288).l
	move.l  #$C100F0,($C1028C).l	   move.l  #$C100D0,($C1028C).l
	move.l  #$E82000,($C10290).l	   move.l  #$EA2000,($C10290).l    palette segment (E80000 = main monitor palette, EA0000 = sub monitor palette)
	move.l  #$E83000,($C10294).l	   move.l  #$EA3000,($C10294).l    palette segment
	move.l  #$E84000,($C10298).l	   move.l  #$EA4000,($C10298).l    palette segment
	move.l  #$E85000,($C1029C).l	   move.l  #$EA5000,($C1029C).l    palette segment
	move.l  #$E80000,($C102A0).l	   move.l  #$EA0000,($C102A0).l    palette segment
	move.l  #$E86000,($C102A4).l	   move.l  #$EA6000,($C102A4).l    palette segment
	move.l  #$E86800,($C102A8).l	   move.l  #$EA6800,($C102A8).l    palette segment
	move.l  #$D20000,($C102B0).l	   move.l  #$D21000,($C102B0).l    sprite list
	move.l  #$D21000,($C102B4).l	   move.l  #$D22000,($C102B4).l    sprite end
	move.l  #$C09000,($C102B8).l	   move.l  #$C19000,($C102B8).l
	move.l  #$C0A000,($C102BC).l	   move.l  #$C1A000,($C102BC).l
	move.l  #$C00700,($C102C0).l	   move.l  #$C10700,($C102C0).l
	move.l  #$C00780,($C102C4).l	   move.l  #$C10780,($C102C4).l
	move.l  #$C10700,($C102C8).l	   move.l  #$C00700,($C102C8).l
	move.l  #$C10780,($C102CC).l	   move.l  #$C00780,($C102CC).l
	move.l  #$C02070,($C102D0).l	   move.l  #$C12070,($C102D0).l
	move.l  #$C09000,($C102D4).l	   move.l  #$C19000,($C102D4).l
	move.l  #$C12070,($C102D8).l	   move.l  #$C02070,($C102D8).l
	move.l  #$C19000,($C102DC).l	   move.l  #$C09000,($C102DC).l
	move.l  #$C12000,($C102E0).l	   move.l  #$C12000,($C102E0).l
	move.l  #$C20000,($C102E4).l	   move.l  #$C20000,($C102E4).l
	move.l  #$C10300,($C102E8).l	   move.l  #$C10300,($C102E8).l
	move.l  #$C10400,($C102EC).l	   move.l  #$C10400,($C102EC).l
*/

static WRITE32_HANDLER( type4_prot_w )
{
	int clk;
	int i;

	if (offset == 1)
	{
		last_prot_op = data>>16;
	}
	else
	{
		data >>= 16;

		clk = data & 0x200;
		if ((clk == 0) && (last_prot_clk != 0))
		{
			if (last_prot_op != -1)
			{
				/*
					known commands:
					rng2   rushhero  vsnet  winspike   what
					------------------------------------------------------------------------------
				        0a56   0d96	 0d14   0d1c       memcpy from c01000 to c01400 for 0x400 bytes
					0b16                               generate sprite list at c01000 or c08400 (not sure entirely, see routine at 209922 in rungun2)
					       0d97	 0515              parse big DMA list at c10200
				*/

				if ((last_prot_op == 0xa56) || (last_prot_op == 0xd96) || (last_prot_op == 0xd14) || (last_prot_op == 0xd1c))
				{
					/* memcpy from c01000 to c01400 for 0x400 bytes (startup check for type 4 games)*/
					for (i = 0; i < 0x400; i += 2)
					{
						cpu_writemem24bedw_word(0xc01400+i, cpu_readmem24bedw_word(0xc01000+i));
					}
				}
				else
				{
					log_cb(RETRO_LOG_DEBUG, LOGPRE "GXT4: unknown protection command %x (PC=%x)\n", last_prot_op, activecpu_get_pc());
				}

				if (konamigx_wrport1_1 & 0x10)
				{
					gx_rdport1_3 &= ~8;
					cpu_set_irq_line(0, 4, HOLD_LINE);
				}

				/* don't accidentally do a phony command*/
				last_prot_op = -1;
			}
		}
		last_prot_clk = clk;
	}
}

/* cabinet lamps for type 1 games*/
static WRITE32_HANDLER( type1_cablamps_w )
{
	set_led_status(0, (data>>24)&1);
}

/**********************************************************************************/
/* 68020 memory handlers */

static MEMORY_READ32_START( readmem )
	{ 0x000000, 0x01ffff, MRA32_ROM },		/* bios*/
	{ 0x200000, 0x2fffff, MRA32_ROM },		/* game program*/
	{ 0x400000, 0x5fffff, MRA32_ROM },		/* data ROM*/
	{ 0xc00000, 0xc1ffff, MRA32_RAM },		/* work ram*/
	{ 0xd00000, 0xd01fff, K056832_5bpp_rom_long_r },	/* tile ROM readthrough (for test menu)*/
	{ 0xd20000, 0xd20fff, K053247_long_r },	/* sprite RAM*/
	{ 0xd21000, 0xd23fff, MRA32_RAM },		/* additional RAM in the sprite region*/
	{ 0xd44000, 0xd44003, le2_gun_H_r },	/* gun horizontal position*/
	{ 0xd44004, 0xd44007, le2_gun_V_r },	/* gun vertical position*/
	{ 0xd4c000, 0xd4c01f, ccu_r },			/* CRT control unit*/
	{ 0xd52010, 0xd5201f, sound020_r },		/* shared RAM with sound 68000*/
	{ 0xd5a000, 0xd5a003, eeprom_r },		/* EEPROM read*/
	{ 0xd5c000, 0xd5c003, players_r },		/* player 1 & 2 JAMMA inputs*/
	{ 0xd5e000, 0xd5e003, service_r },		/* service switch*/
	{ 0xd90000, 0xd97fff, MRA32_RAM },		/* palette RAM*/
	{ 0xda0000, 0xda1fff, K056832_ram_long_r },	/* tilemap RAM*/
	{ 0xda2000, 0xda3fff, K056832_ram_long_r },	/* tilemap RAM mirror read*/
#if GX_DEBUG
	{ 0xd40000, 0xd4003f, K056832_long_r },
	{ 0xd50000, 0xd500ff, K055555_long_r },
	{ 0xd4a010, 0xd4a01f, K053247_reg_long_r },
#endif
MEMORY_END

static MEMORY_WRITE32_START( writemem )
	{ 0xc00000, 0xc1ffff, MWA32_RAM, &gx_workram },
	{ 0xcc0000, 0xcc0003, esc_w },
	{ 0xd20000, 0xd20fff, K053247_long_w },
	{ 0xd21000, 0xd23fff, MWA32_RAM },
	{ 0xd40000, 0xd4003f, K056832_long_w },		/* VACSET*/
	{ 0xd44000, 0xd4400f, konamigx_tilebank_w },/* VSCCS*/
	{ 0xd48000, 0xd48007, K053246_long_w },		/* OBJSET1*/
	{ 0xd4a010, 0xd4a01f, K053247_reg_long_w },	/* OBJSET2*/
	{ 0xd4c000, 0xd4c01f, ccu_w },				/* CCU1(ccu_w)*/
	{ 0xd4e000, 0xd4e01f, MWA32_NOP },			/* CCU2(not used by GX)*/
	{ 0xd50000, 0xd500ff, K055555_long_w },		/* PCU2*/
	{ 0xd52000, 0xd5200f, sound020_w },
	{ 0xd56000, 0xd56003, eeprom_w },
	{ 0xd58000, 0xd58003, control_w },
	{ 0xd80000, 0xd8001f, K054338_long_w },		/* CLTC*/
	{ 0xda0000, 0xda1fff, K056832_ram_long_w },
	{ 0xda2000, 0xda3fff, K056832_ram_long_w },	/* tilemap RAM mirror write*/
	{ 0xd90000, 0xd97fff, konamigx_palette_w, &paletteram32 },
MEMORY_END

static MEMORY_READ32_START( type1readmem )
	{ 0x000000, 0x01ffff, MRA32_ROM },		/* bios*/
	{ 0x200000, 0x2fffff, MRA32_ROM },		/* game program*/
	{ 0x400000, 0x7fffff, MRA32_ROM },		/* data ROM*/
	{ 0xc00000, 0xc1ffff, MRA32_RAM },		/* work ram*/
	{ 0xd00000, 0xd01fff, K056832_6bpp_rom_long_r },
	{ 0xd20000, 0xd20fff, K053247_long_r },	/* sprite RAM*/
	{ 0xd21000, 0xd23fff, MRA32_RAM },		/* additional RAM in the sprite region*/
	{ 0xd4a000, 0xd4a01f, gx6bppspr_r },	/* sprite ROM readback*/
	{ 0xd4c000, 0xd4c01f, ccu_r },			/* CRT control unit*/
	{ 0xd52010, 0xd5201f, sound020_r },		/* shared RAM with sound 68000*/
	{ 0xd5a000, 0xd5a003, eeprom_r },		/* EEPROM read*/
	{ 0xd5c000, 0xd5c003, players_r },		/* player 1 & 2 JAMMA inputs*/
	{ 0xd5e000, 0xd5e003, service_r }, 		/* service switch*/
	{ 0xd90000, 0xd97fff, MRA32_RAM },		/* palette RAM*/
	{ 0xda0000, 0xda1fff, K056832_ram_long_r },	/* tilemap RAM*/
	{ 0xda2000, 0xda3fff, K056832_ram_long_r },	/* tilemap RAM mirror read*/
	{ 0xdc0000, 0xdc1fff, MRA32_RAM },		/* LAN?  (Racin Force has, KOG doesn't)*/
	{ 0xdd0000, 0xdd00ff, MRA32_NOP },		/* LAN board*/
	{ 0xddc000, 0xddcfff, adc0834_r },
	{ 0xe80000, 0xe81fff, MRA32_RAM },	/* chips 21L+19L / S*/
	{ 0xec0000, 0xedffff, MRA32_RAM },	/* chips 20J+23J+18J / S*/
	{ 0xf00000, 0xf3ffff, type1_roz_r1 },	/* ROM readback*/
	{ 0xf40000, 0xf7ffff, type1_roz_r2 },	/* ROM readback*/
	{ 0xf80000, 0xf80fff, MRA32_RAM },	/* chip 21Q / S*/
	{ 0xfc0000, 0xfc00ff, MRA32_RAM },	/* chip 22N / S*/
MEMORY_END

static MEMORY_WRITE32_START( type1writemem )
	{ 0xc00000, 0xc1ffff, MWA32_RAM, &gx_workram },
	{ 0xcc0000, 0xcc0003, esc_w },
	{ 0xd20000, 0xd20fff, K053247_long_w },
	{ 0xd21000, 0xd23fff, MWA32_RAM },
	{ 0xd40000, 0xd4003f, K056832_long_w },		/* VACSET*/
	{ 0xd44000, 0xd4400f, konamigx_tilebank_w },	/* VSCCS*/
	{ 0xd48000, 0xd48007, K053246_long_w },		/* OBJSET1*/
	{ 0xd4a010, 0xd4a01f, K053247_reg_long_w },	/* OBJSET2*/
	{ 0xd4c000, 0xd4c01f, ccu_w },			/* CCU1(ccu_w)*/
	{ 0xd4e000, 0xd4e01f, MWA32_NOP },		/* CCU2(not used by GX)*/
	{ 0xd50000, 0xd500ff, K055555_long_w },		/* PCU2*/
	{ 0xd52000, 0xd5200f, sound020_w },
	{ 0xd56000, 0xd56003, eeprom_w },
	{ 0xd58000, 0xd58003, control_w },
	{ 0xd80000, 0xd8001f, K054338_long_w },		/* CLTC*/
	{ 0xda0000, 0xda1fff, K056832_ram_long_w },
	{ 0xda2000, 0xda3fff, K056832_ram_long_w },	/* tilemap RAM mirror write*/
	{ 0xd90000, 0xd97fff, konamigx_palette_w, &paletteram32 },
	{ 0xdc0000, 0xdc1fff, MWA32_RAM },		/* LAN? (Racin Force has, KOG doesn't)*/
	{ 0xdd0000, 0xdd00ff, MWA32_NOP },		/* LAN board*/
	{ 0xdda000, 0xddafff, adc0834_w },
	{ 0xdde000, 0xdde003, type1_cablamps_w },
	{ 0xe00000, 0xe0001f, MWA32_RAM, (data32_t**)&K053936_1_ctrl },
	{ 0xe20000, 0xe2000f, MWA32_NOP },
	{ 0xe40000, 0xe40003, MWA32_NOP },
	{ 0xe80000, 0xe81fff, MWA32_RAM, (data32_t**)&K053936_1_linectrl },
	{ 0xec0000, 0xedffff, konamigx_t1_psacmap_w, &gx_psacram },
	{ 0xf80000, 0xf80fff, MWA32_RAM },
	{ 0xfc0000, 0xfc00ff, MWA32_RAM },
MEMORY_END

static MEMORY_READ32_START( type3readmem )
	{ 0x000000, 0x01ffff, MRA32_ROM },		/* bios*/
	{ 0x200000, 0x2fffff, MRA32_ROM },		/* game program*/
	{ 0x400000, 0x5fffff, MRA32_ROM },		/* data ROM*/
	{ 0xc00000, 0xc1ffff, MRA32_RAM },		/* work ram*/
	{ 0xd00000, 0xd01fff, K056832_rom_long_r },	/* tile ROM readthrough (for test menu)*/
	{ 0xd20000, 0xd20fff, K053247_long_r },	/* sprite RAM*/
	{ 0xd21000, 0xd23fff, MRA32_RAM },		/* additional RAM in the sprite region*/
/*	{ 0xd4a000, 0xd4bfff, K053246_long_r },	*/ /* sprite ROM readthrough (for test menu)*/
	{ 0xd4c000, 0xd4c01f, ccu_r },			/* CRT control unit*/
	{ 0xd52010, 0xd5201f, sound020_r },		/* shared RAM with sound 68000*/
	{ 0xd5a000, 0xd5a003, eeprom_r },		/* EEPROM read*/
	{ 0xd5c000, 0xd5c003, players_r },		/* player 1 & 2 JAMMA inputs*/
	{ 0xd5e000, 0xd5e003, service_r }, 		/* service switch*/
	{ 0xd90000, 0xd97fff, MRA32_RAM },		/* palette RAM*/
	{ 0xda0000, 0xda1fff, K056832_ram_long_r },	/* tilemap RAM*/
	{ 0xe60000, 0xe60fff, MRA32_RAM },
	{ 0xe80000, 0xe87fff, MRA32_RAM },
	{ 0xea0000, 0xea3fff, MRA32_RAM },
	{ 0xec0000, 0xec0003, type3_sync_r },
	{ 0xf00000, 0xf07fff, MRA32_RAM },
MEMORY_END

static MEMORY_WRITE32_START( type3writemem )
	{ 0xc00000, 0xc1ffff, MWA32_RAM, &gx_workram },
	{ 0xcc0000, 0xcc0007, type4_prot_w },
	{ 0xd20000, 0xd20fff, K053247_long_w },
	{ 0xd21000, 0xd23fff, MWA32_RAM },
	{ 0xd44000, 0xd4400f, konamigx_tilebank_w },
	{ 0xd4a018, 0xd4a01b, MWA32_NOP },
	{ 0xd4c01c, 0xd4c01f, MWA32_NOP },
	{ 0xd50000, 0xd5007f, K055555_long_w },
	{ 0xd52000, 0xd5200f, sound020_w },
	{ 0xd40000, 0xd4003f, K056832_long_w },
	{ 0xd48000, 0xd4803f, K053246_long_w },
	{ 0xd56000, 0xd56003, eeprom_w },
	{ 0xd58000, 0xd58003, control_w },
	{ 0xd80000, 0xd800ff, K054338_long_w },
	{ 0xda0000, 0xda1fff, K056832_ram_long_w },
	{ 0xd90000, 0xd97fff, MWA32_RAM },
	{ 0xe00000, 0xe0001f, MWA32_RAM, (data32_t**)&K053936_1_ctrl },
	{ 0xe20000, 0xe20003, MWA32_NOP },
	{ 0xe40000, 0xe40003, MWA32_NOP },
	{ 0xe60000, 0xe60fff, MWA32_RAM, (data32_t**)&K053936_1_linectrl },
	{ 0xe80000, 0xe87fff, konamigx_555_palette_w, &paletteram32 }, 	/* main monitor palette (twice as large as reality)*/
	{ 0xea0000, 0xea3fff, konamigx_555_palette2_w, &gx_subpaletteram32 }, /* sub monitor palette*/
	{ 0xf00000, 0xf07fff, MWA32_RAM },
MEMORY_END

static MEMORY_READ32_START( type4readmem )
	{ 0x000000, 0x01ffff, MRA32_ROM },		/* bios*/
	{ 0x200000, 0x2fffff, MRA32_ROM },		/* game program*/
	{ 0x400000, 0x5fffff, MRA32_ROM },		/* data ROM*/
	{ 0xc00000, 0xc1ffff, MRA32_RAM },		/* work ram*/
	{ 0xd00000, 0xd01fff, K056832_rom_long_r },	/* tile ROM readthrough (for test menu)*/
	{ 0xd20000, 0xd20fff, K053247_long_r },	/* sprite RAM*/
	{ 0xd21000, 0xd23fff, MRA32_RAM },		/* additional RAM in the sprite region*/
	{ 0xd4a000, 0xd4bfff, K053246_long_r },	/* sprite ROM readthrough (for test menu)*/
	{ 0xd4c000, 0xd4c01f, ccu_r },			/* CRT control unit*/
	{ 0xd52010, 0xd5201f, sound020_r },		/* shared RAM with sound 68000*/
	{ 0xd5a000, 0xd5a003, eeprom_r },		/* EEPROM read*/
	{ 0xd5c000, 0xd5c003, players_r },		/* player 1 & 2 JAMMA inputs*/
	{ 0xd5e000, 0xd5e003, service_r }, 		/* service switch*/
	{ 0xd90000, 0xd97fff, MRA32_RAM },		/* palette RAM*/
	{ 0xda0000, 0xda1fff, K056832_ram_long_r },	/* tilemap RAM*/
	{ 0xe60000, 0xe60fff, MRA32_RAM },
	{ 0xe80000, 0xe8ffff, MRA32_RAM },
	{ 0xea0000, 0xea7fff, MRA32_RAM },
	{ 0xec0000, 0xec0003, type3_sync_r },		/* type 4 polls this too*/
	{ 0xf00000, 0xf07fff, MRA32_RAM },
MEMORY_END

static MEMORY_WRITE32_START( type4writemem )
	{ 0xc00000, 0xc1ffff, MWA32_RAM, &gx_workram },
	{ 0xcc0000, 0xcc0007, type4_prot_w },
	{ 0xd44000, 0xd4400f, konamigx_tilebank_w },
	{ 0xd4a018, 0xd4a01b, MWA32_NOP },
	{ 0xd4c01c, 0xd4c01f, MWA32_NOP },
	{ 0xd50000, 0xd5007f, K055555_long_w },
	{ 0xd20000, 0xd20fff, K053247_long_w },
	{ 0xd21000, 0xd23fff, MWA32_RAM },
	{ 0xd52000, 0xd5200f, sound020_w },
	{ 0xd40000, 0xd4003f, K056832_long_w },
	{ 0xd48000, 0xd4803f, K053246_long_w },
	{ 0xd56000, 0xd56003, eeprom_w },
	{ 0xd58000, 0xd58003, control_w },
	{ 0xd80000, 0xd800ff, K054338_long_w },
	{ 0xd90000, 0xd97fff, MWA32_RAM },
	{ 0xda0000, 0xda1fff, K056832_ram_long_w },
	{ 0xe00000, 0xe0001f, MWA32_RAM, (data32_t**)&K053936_1_ctrl },
	{ 0xe20000, 0xe20003, MWA32_NOP },
	{ 0xe40000, 0xe40003, MWA32_NOP },
	{ 0xe60000, 0xe60fff, MWA32_RAM, (data32_t**)&K053936_1_linectrl },  /* 29C & 29G (PSAC2 line control)*/
	{ 0xe80000, 0xe8ffff, konamigx_palette_w, &paletteram32 }, /* 11G/13G/15G (main screen palette RAM) (twice as large as reality)*/
	{ 0xea0000, 0xea7fff, konamigx_palette2_w, &gx_subpaletteram32 }, /* 5G/7G/9G (sub screen palette RAM)*/
	{ 0xf00000, 0xf07fff, konamigx_t4_psacmap_w, &gx_psacram },	/* PSAC2 tilemap*/
MEMORY_END

/**********************************************************************************/
/* Sound handling */

static READ16_HANDLER( dual539_r )
{
	data16_t ret = 0;

	if (ACCESSING_LSB16)
		ret |= K054539_1_r(offset);
	if (ACCESSING_MSB16)
		ret |= K054539_0_r(offset)<<8;

	return ret;
}

static WRITE16_HANDLER( dual539_w )
{
	if (ACCESSING_LSB16)
		K054539_1_w(offset, data);
	if (ACCESSING_MSB16)
		K054539_0_w(offset, data>>8);
}

static READ16_HANDLER( sndcomm68k_r )
{
	return sndto000[offset];
}

static WRITE16_HANDLER( sndcomm68k_w )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "68K: write %x to %x\n", data, offset);*/
	sndto020[offset] = data;
}

/* 68000 memory handling */
static MEMORY_READ16_START( sndreadmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x100000, 0x10ffff, MRA16_RAM },
	{ 0x200000, 0x2004ff, dual539_r },
	{ 0x300000, 0x300001, tms57002_data_word_r },
	{ 0x400010, 0x40001f, sndcomm68k_r },
	{ 0x500000, 0x500001, tms57002_status_word_r },
MEMORY_END

static MEMORY_WRITE16_START( sndwritemem )
	{ 0x100000, 0x10ffff, MWA16_RAM, &gx_sndram },
	{ 0x200000, 0x2004ff, dual539_w },
	{ 0x300000, 0x300001, tms57002_data_word_w },
	{ 0x400000, 0x40000f, sndcomm68k_w },
	{ 0x500000, 0x500001, tms57002_control_word_w },
	{ 0x580000, 0x580001, MWA16_NOP }, /* unknown write*/
MEMORY_END

/* 68000 timer interrupt controller */
static INTERRUPT_GEN(gxaudio_interrupt)
{
	cpu_set_irq_line(1, 2, HOLD_LINE);
}

static struct K054539interface k054539_interface =
{
	2,			/* 2 chips */
	48000,
	{ REGION_SOUND1, REGION_SOUND1 },
	{ { 100, 100 }, { 100, 100 } },
	{ NULL }
};


/**********************************************************************************/
/* hardware definitions */

/* i think we could reduce the number of machine drivers with different visible areas by adjusting the sprite
   positioning on a per game basis too */

static struct GfxLayout bglayout_8bpp =
{
	16,16,
	RGN_FRAC(1,1),
	8,
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	{ 0*128, 1*128, 2*128, 3*128, 4*128, 5*128, 6*128, 7*128, 8*128, 9*128, 10*128, 11*128, 12*128, 13*128, 14*128, 15*128 },
	16*128
};

struct GfxLayout t1_charlayout6 =
{
	16, 16,
	RGN_FRAC(1,1),
	6,
	{ 20, 16, 12, 8, 4, 0 },
	{ 3, 2, 1, 0, 27, 26, 25, 24, 51, 50, 49, 48, 75, 74, 73, 72 },
	{ 0, 12*8, 12*8*2, 12*8*3, 12*8*4, 12*8*5, 12*8*6, 12*8*7,
	  12*8*8, 12*8*9, 12*8*10, 12*8*11, 12*8*12, 12*8*13, 12*8*14, 12*8*15 },
	16*16*6
};

struct GfxLayout t1_charlayout8 =
{
	16, 16,
	RGN_FRAC(1,1),
	8,
	{ 28, 24, 20, 16, 12, 8, 4, 0 },
	{ 3, 2, 1, 0, 35, 34, 33, 32, 67, 66, 65, 64, 99, 98, 97, 96 },
	{ 0, 16*8, 16*8*2, 16*8*3, 16*8*4, 16*8*5, 16*8*6, 16*8*7,
	  16*8*8, 16*8*9, 16*8*10, 16*8*11, 16*8*12, 16*8*13, 16*8*14, 16*8*15 },
	16*16*8
};

/* type 1 (opengolf + racinfrc) use 6 and 8 bpp planar layouts for the 53936 */
static struct GfxDecodeInfo gfxdecodeinfo_opengolf[] =
{
	{ REGION_GFX3, 0, &t1_charlayout8, 0x0000, 8 },
	{ REGION_GFX4, 0, &t1_charlayout6, 0x0000, 8 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_racinfrc[] =
{
	{ REGION_GFX3, 0, &t1_charlayout6, 0x0000, 8 },
	{ REGION_GFX4, 0, &t1_charlayout6, 0x0000, 8 },
	{ -1 } /* end of array */
};

/* type 3 & 4 games use a simple 8bpp decode for the 53936 */
static struct GfxDecodeInfo gfxdecodeinfo_type34[] =
{
	{ REGION_GFX3, 0, &bglayout_8bpp, 0x0000, 8 },
	{ -1 } /* end of array */
};

static MACHINE_DRIVER_START( konamigx )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68EC020, 24000000)
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(konamigx_vbinterrupt, 1)

	/* note: part is a -8, crystals are 18.4 and 32.0 MHz, and
	   twinbee yahhoo will not pass POST if the 68000 isn't
	   running at least this fast.  so the higher speed is probably a HACK... */
	MDRV_CPU_ADD_TAG("sound", M68000, 9200000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sndreadmem,sndwritemem)
	MDRV_CPU_PERIODIC_INT(irq2_line_hold, 480)

	MDRV_INTERLEAVE(32);
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(600)

	MDRV_MACHINE_INIT(konamigx)
	MDRV_NVRAM_HANDLER(konamigx_93C46)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN | VIDEO_RGB_DIRECT | VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(24, 24+288-1, 16, 16+224-1)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(konamigx_5bpp)
	MDRV_VIDEO_UPDATE(konamigx)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(K054539, k054539_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( dragoonj )
	MDRV_IMPORT_FROM(konamigx)
	MDRV_CPU_REPLACE("main", M68EC020, 26400000) /* needs higher clock to stop sprite flickerings*/
	MDRV_VISIBLE_AREA(40, 40+384-1, 16, 16+224-1)
	MDRV_VIDEO_START(dragoonj)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( le2 )
	MDRV_IMPORT_FROM(konamigx)
	MDRV_VIDEO_START(le2)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( konamigx_6bpp )
	MDRV_IMPORT_FROM(konamigx)
	MDRV_VIDEO_START(konamigx_6bpp)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( konamigx_6bpp_2 )
	MDRV_IMPORT_FROM(konamigx)
	MDRV_VIDEO_START(konamigx_6bpp_2)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( opengolf )
	MDRV_IMPORT_FROM(konamigx)
	MDRV_VISIBLE_AREA(40, 40+384-1, 16, 16+224-1)
	MDRV_GFXDECODE(gfxdecodeinfo_opengolf)
	MDRV_VIDEO_START(opengolf)

	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(type1readmem, type1writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( racinfrc )
	MDRV_IMPORT_FROM(konamigx)
	MDRV_VISIBLE_AREA(32, 32+384-1, 16, 16+224-1)
	MDRV_GFXDECODE(gfxdecodeinfo_racinfrc)
	MDRV_VIDEO_START(racinfrc)

	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(type1readmem, type1writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( gxtype3 )
	MDRV_IMPORT_FROM(konamigx)

	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(type3readmem, type3writemem)
	MDRV_CPU_VBLANK_INT(konamigx_hbinterrupt, 262)

	MDRV_VIDEO_START(konamigx_type3)
	MDRV_PALETTE_LENGTH(16384)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0, 64*8-1, 0, 32*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo_type34)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( gxtype4 )
	MDRV_IMPORT_FROM(konamigx)

	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(type4readmem, type4writemem)
	MDRV_CPU_VBLANK_INT(konamigx_hbinterrupt, 262)

	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0, 64*8-1, 0, 32*8-1)
	MDRV_PALETTE_LENGTH(16384)
	MDRV_GFXDECODE(gfxdecodeinfo_type34)
	MDRV_VIDEO_START(konamigx_type4)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( winspike )
	MDRV_IMPORT_FROM(konamigx)

	MDRV_VISIBLE_AREA(38, 38+384-1, 16, 16+224-1)
	MDRV_VIDEO_START(winspike)
MACHINE_DRIVER_END

/**********************************************************************************/
/* port maps */

INPUT_PORTS_START( konamigx )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* for gun games */
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, "Stereo")
	PORT_DIPSETTING(    0x00, "Stereo")
	PORT_DIPSETTING(    0x01, "Mono")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Foo")
	PORT_DIPSETTING(    0x00, "Foo")
	PORT_DIPSETTING(    0x01, "Bar")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( racinfrc )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* for gun games */
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )		/* racin force needs this set to get past the calibration screen*/
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, "Stereo")
	PORT_DIPSETTING(    0x00, "Stereo")
	PORT_DIPSETTING(    0x01, "Mono")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Foo")
	PORT_DIPSETTING(    0x00, "Foo")
	PORT_DIPSETTING(    0x01, "Bar")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* no gun */

	PORT_START /* mask default type                     sens delta min max */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_CENTER, 35, 5, 0x38, 0xc8 )

	PORT_START
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 35, 5, 0, 0x68, KEYCODE_LCONTROL, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )
INPUT_PORTS_END

INPUT_PORTS_START( le2 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* for gun games */
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Stereo")
	PORT_DIPSETTING(    0x01, "Stereo")
	PORT_DIPSETTING(    0x00, "Mono")
	PORT_DIPNAME( 0x02, 0x02, "Coin Mechanism")
	PORT_DIPSETTING(    0x02, "Common")
	PORT_DIPSETTING(    0x00, "Independant")
	PORT_DIPNAME( 0x04, 0x04, "Stage Select" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Mirror" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* for gun games */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* mask default type                     sens delta min max */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 35, 15, 0, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 35, 15, 0, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 35, 15, 0, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 35, 15, 0, 0xff )
INPUT_PORTS_END

INPUT_PORTS_START( le2_flip )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* for gun games */
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Stereo")
	PORT_DIPSETTING(    0x01, "Stereo")
	PORT_DIPSETTING(    0x00, "Mono")
	PORT_DIPNAME( 0x02, 0x02, "Coin Mechanism")
	PORT_DIPSETTING(    0x02, "Common")
	PORT_DIPSETTING(    0x00, "Independant")
	PORT_DIPNAME( 0x04, 0x04, "Stage Select" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Mirror" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* for gun games */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* mask default type                     sens delta min max */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 35, 15, 0, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1 | IPF_REVERSE, 35, 15, 0, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 35, 15, 0, 0xff )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2 | IPF_REVERSE, 35, 15, 0, 0xff )
INPUT_PORTS_END

INPUT_PORTS_START( gokuparo )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, "Stereo")
	PORT_DIPSETTING(    0x00, "Stereo")
	PORT_DIPSETTING(    0x01, "Mono")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( puzldama )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, "Stereo")
	PORT_DIPSETTING(    0x00, "Stereo")
	PORT_DIPSETTING(    0x01, "Mono")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, "Vs. cabinet" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x02, "High" )
INPUT_PORTS_END

INPUT_PORTS_START( dragoonj )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, "Stereo")
	PORT_DIPSETTING(    0x00, "Stereo")
	PORT_DIPSETTING(    0x01, "Mono")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, "Vs. cabinet" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x02, "High" )
INPUT_PORTS_END

INPUT_PORTS_START( type3 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, "Stereo")
	PORT_DIPSETTING(    0x00, "Stereo")
	PORT_DIPSETTING(    0x01, "Mono")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Screens" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPSETTING(    0x02, "High" )
INPUT_PORTS_END


/**********************************************************************************/
/* BIOS and ROM maps */

#define GX_BIOS ROM_LOAD("300a01.34k", 0x000000, 128*1024, CRC(d5fa95f5) SHA1(c483aa98ff8ef40cdac359c19ad23fea5ecc1906) )

ROM_START(konamigx)
	ROM_REGION( 0x10000, REGION_CPU1, 0)
	GX_BIOS
ROM_END

#define SPR_WOR_DROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPSIZE(2) | ROM_SKIP(5))
#define SPR_5TH_ROM_LOAD(name,offset,length,crc)	 ROMX_LOAD(name, offset, length, crc, ROM_GROUPSIZE(1) | ROM_SKIP(5))

#define TILE_WORD_ROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPDWORD | ROM_SKIP(1))
#define TILE_BYTE_ROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPBYTE | ROM_SKIP(4))

#define TILE_WORDS2_ROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPDWORD | ROM_SKIP(2))
#define TILE_BYTES2_ROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPWORD | ROM_SKIP(4))

#define T1_PSAC6_ROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPBYTE | ROM_SKIP(2))
#define T1_PSAC8_ROM_LOAD(name,offset,length,crc) ROMX_LOAD(name, offset, length, crc, ROM_GROUPBYTE | ROM_SKIP(3))

#define _48_WORD_ROM_LOAD(name,offset,length,crc)	ROMX_LOAD(name, offset, length, crc, ROM_GROUPWORD | ROM_SKIP(4))
#define _64_WORD_ROM_LOAD(name,offset,length,crc)	ROMX_LOAD(name, offset, length, crc, ROM_GROUPWORD | ROM_SKIP(6))


/* Gokujou Parodius */
ROM_START( gokuparo )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "321jad02.31b", 0x200002, 512*1024, CRC(c2e548c0) SHA1(48fbcc96d1f56bb3abb5098400536a18a676d934) )
	ROM_LOAD32_WORD_SWAP( "321jad04.27b", 0x200000, 512*1024, CRC(916a7951) SHA1(d6f56ff5f6c6708939767e69a3ebc7c7eddb6003) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("321_b06.9c", 0x000000, 128*1024, CRC(da810554) SHA1(f253e1aa137eecf283d8b083ef2b3b049e8366f4) )
	ROM_LOAD16_BYTE("321_b07.7c", 0x000001, 128*1024, CRC(c47634c0) SHA1(20e4105df5bbc33edd01894e78f74ed5f173576e) )

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "fj-jap.17h", 0x000000, 2*1024*1024, CRC(437d0057) SHA1(30c449200e0510dc664289b527bade6e10dbe57a) )
	TILE_BYTE_ROM_LOAD( "321b12.13g", 0x000004, 512*1024, CRC(5f9edfa0) SHA1(36d54c5fe498a4d0fa64757cef11c56c67518258) )

	/* sprites */
	ROM_REGION( 0x500000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "fj-jap.25g", 0x000000, 2*1024*1024, CRC(c6e2e74d) SHA1(3875a50923e46e2986dbe2573453af5c7fa726f7) )
	ROM_LOAD32_WORD( "fj-jap.28g", 0x000002, 2*1024*1024, CRC(ea9f8c48) SHA1(b5e880015887308a5f1c1c623011d9b0903e848f) )
	ROM_LOAD( "321b09.30g", 0x400000, 1*1024*1024, CRC(94add237) SHA1(9a6d0a9727e7fa02d91ece220b145074a6741a95) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "321b17.9g", 0x000000, 2*1024*1024, CRC(b3e8d5d8) SHA1(6644a414e7f0e69ded9aa1bf892566002cebae26) )
	ROM_LOAD( "321b18.7g", 0x200000, 2*1024*1024, CRC(2c561ad0) SHA1(6265054072ba1c2837dd96e0259b20bc50457160) )
ROM_END

/* Fantastic Journey (US version) */
ROM_START( fantjour )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "fsus31b.bin", 0x200002, 512*1024, CRC(afaf9d17) SHA1(a12214c6e634862d6507f56719b55d4a23a0ef0f) )
	ROM_LOAD32_WORD_SWAP( "fsus27b.bin", 0x200000, 512*1024, CRC(b2cfe225) SHA1(7fd43acb1dd853a7980e7fcf48971ae28175e421) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("321_b06.9c", 0x000000, 128*1024, CRC(da810554) SHA1(f253e1aa137eecf283d8b083ef2b3b049e8366f4) )
	ROM_LOAD16_BYTE("321_b07.7c", 0x000001, 128*1024, CRC(c47634c0) SHA1(20e4105df5bbc33edd01894e78f74ed5f173576e) )

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "fj-jap.17h", 0x000000, 2*1024*1024, CRC(437d0057) SHA1(30c449200e0510dc664289b527bade6e10dbe57a) )
	TILE_BYTE_ROM_LOAD( "321b12.13g", 0x000004, 512*1024, CRC(5f9edfa0) SHA1(36d54c5fe498a4d0fa64757cef11c56c67518258) )

	/* sprites */
	ROM_REGION( 0x500000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "fj-jap.25g", 0x000000, 2*1024*1024, CRC(c6e2e74d) SHA1(3875a50923e46e2986dbe2573453af5c7fa726f7) )
	ROM_LOAD32_WORD( "fj-jap.28g", 0x000002, 2*1024*1024, CRC(ea9f8c48) SHA1(b5e880015887308a5f1c1c623011d9b0903e848f) )
	ROM_LOAD( "321b09.30g", 0x400000, 1*1024*1024, CRC(94add237) SHA1(9a6d0a9727e7fa02d91ece220b145074a6741a95) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "321b17.9g", 0x000000, 2*1024*1024, CRC(b3e8d5d8) SHA1(6644a414e7f0e69ded9aa1bf892566002cebae26) )
	ROM_LOAD( "321b18.7g", 0x200000, 2*1024*1024, CRC(2c561ad0) SHA1(6265054072ba1c2837dd96e0259b20bc50457160) )
ROM_END

/* Salamander 2 */
ROM_START( salmndr2 )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "521jaa02.31b", 0x200002, 512*1024, CRC(f6c3a95b) SHA1(c4ef3631eca898e5787fb2d356355da7e5d475eb) )
	ROM_LOAD32_WORD_SWAP( "521jaa03.27b", 0x200000, 512*1024, CRC(c3be5e0a) SHA1(13bbce62c4d04a657de4594cc4d258e2468a59a4) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("521-a04.9c", 0x000000, 64*1024, CRC(efddca7a) SHA1(ac6b45044b6abeb2455ec21a61322185bf1c7219) )
	ROM_LOAD16_BYTE("521-a05.7c", 0x000001, 64*1024, CRC(51a3af2c) SHA1(94d220ae619d53747bd3e762000ed59cf1b4d305) )

	/* tiles */
	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORDS2_ROM_LOAD("521-a09.17h", 0x000000, 2*1024*1024, CRC(fb9e2f5e) SHA1(acb41616625d6976ad50e184787ab74e29f86039) )
	TILE_WORDS2_ROM_LOAD("521-a11.15h", 0x300000, 1*1024*1024, CRC(25e0a6e5) SHA1(592e9f183f077e9272a4f0ead441b5bfd8029816) )
	TILE_BYTES2_ROM_LOAD("521-a13.13c", 0x000004, 2*1024*1024, CRC(3ed7441b) SHA1(57e3e8035c056cf46a383d228c76a7da7def134f) )

	/* sprites */
	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASE00 )
	_48_WORD_ROM_LOAD( "521-a08.25g", 0x000000, 2*1024*1024, CRC(f24f76bd) SHA1(823f614d436901241743c923206cb61d8bbb5c58) )
	_48_WORD_ROM_LOAD( "521-a07.28g", 0x000002, 2*1024*1024, CRC(50ef9b7a) SHA1(104eac2bce43e99d4adc208145afe7be9156628e) )
	_48_WORD_ROM_LOAD( "521-a06.30g", 0x000004, 2*1024*1024, CRC(cba5db2c) SHA1(505efdf8571ae28d8788dcafbfffcfb67e3189ce) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "521-a12.9g", 0x000000, 2*1024*1024, CRC(66614d3b) SHA1(e1e5ebe546bced6ab74b0af500acf0f3308902a4) )
	ROM_LOAD( "521-a13.7g", 0x200000, 1*1024*1024, CRC(c3322475) SHA1(1774524ff031e0c4a7f3432810e968d37f9c6331) )
ROM_END

/* Twinbee Yahoo! */
ROM_START( tbyahhoo )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "426jaa02.31b", 0x200002, 512*1024, CRC(0416ad78) SHA1(a94c37a95e431c8f8cc3db66713faed406ab27c4) )
	ROM_LOAD32_WORD_SWAP( "424jaa04.27b", 0x200000, 512*1024, CRC(bcbe0e40) SHA1(715f72a172a0662e6e65a57baa1f5a18d6210389) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("424a06.9c", 0x000000, 128*1024, CRC(a4760e14) SHA1(78dbd309f3f7fa61e92c9554e594449a7d4eed5a) )
	ROM_LOAD16_BYTE("424a07.7c", 0x000001, 128*1024, CRC(fa90d7e2) SHA1(6b6dee29643309005834416bdfdb18d74f34cb1b) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "424a14.17h", 0x000000, 2*1024*1024, CRC(b1d9fce8) SHA1(143ed2f03ac10a0f18d878c0ee0509a5714e4664) )
	TILE_BYTE_ROM_LOAD( "424a12.13g", 0x000004, 512*1024, CRC(7f9cb8b1) SHA1(f5e18d70fcb572bb85f9b064995fc0ab0bb581e8) )

	/* sprites */
	ROM_REGION( 0x500000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "424a11.25g", 0x000000, 2*1024*1024, CRC(29592688) SHA1(a4b44e9153988a510915af83116e3c18dd15642f) )
	ROM_LOAD32_WORD( "424a10.28g", 0x000002, 2*1024*1024, CRC(cf24e5e3) SHA1(095bf2ae4f47c6e4768515ae5e22c982fbc660a5) )
	ROM_LOAD( "424a09.30g", 0x400000, 1*1024*1024, CRC(daa07224) SHA1(198cafa3d0ead2aa2593be066c6f372e66c11c44) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "424a17.9g", 0x000000, 2*1024*1024, CRC(e9dd9692) SHA1(c289019c8d1dd71b3cec26479c39b649de804707) )
	ROM_LOAD( "424a18.7g", 0x200000, 2*1024*1024, CRC(0f0d9f3a) SHA1(57f6b113b80f06964b7e672ad517c1654c5569c5) )
ROM_END

/* Daisu-Kiss */
ROM_START( daiskiss )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "535jaa02.31b", 0x200002, 512*1024, CRC(e5b3e0e5) SHA1(94910d79299e99022a1759998304b87440694ca0) )
	ROM_LOAD32_WORD_SWAP( "535jaa03.27b", 0x200000, 512*1024, CRC(9dc10140) SHA1(0c4cc20b2c8ff5080fbd4ceb9446e6940b12cc53) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("535a08.9c", 0x000000, 128*1024, CRC(449416a7) SHA1(c79bf0d68c8639f67eb17f24f1bc10dd867a4c37) )
	ROM_LOAD16_BYTE("535a09.7c", 0x000001, 128*1024, CRC(8ec57ab4) SHA1(bd8e12c796d42d2cb27c1e47dc6253bfb74a2887) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "535a19.17h", 0x000000, 2*1024*1024, CRC(fa1c59d1) SHA1(7344afab2b8101f979c35ff9ec8d9c18475bb821) )
	TILE_BYTE_ROM_LOAD( "535a18.13g", 0x000004, 512*1024,    CRC(d02e5103) SHA1(43c63a718a034636bad29d2def054d8b48f071e3) )

	/* sprites */
	ROM_REGION( 0x500000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "535a17.25g", 0x000000, 1*1024*1024, CRC(b12070e2) SHA1(51a763bf0e2c1d5c2b9983fcd4304d74c7fe6dd1) )
	ROM_LOAD32_WORD( "535a13.28g", 0x000002, 1*1024*1024, CRC(10cf9d05) SHA1(6c6e51082ce340643d381863fec9b220e3d0ac53) )
	ROM_LOAD( "535a11.30g", 0x400000, 512*1024, CRC(2b176b0f) SHA1(ecf4114d95a308be8f96a5c602c0f5ed5ffc8f29) )

	/* sound data */
	ROM_REGION( 0x200000, REGION_SOUND1, 0 )
	ROM_LOAD( "535a22.9g", 0x000000, 2*1024*1024, CRC(7ee59acb) SHA1(782bf15f205e9fe7bd069f6445eb8187837dee32) )
ROM_END

/* Sexy Parodius Japan */
ROM_START( sexyparo )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "533jaa02.31b", 0x200002, 512*1024, CRC(b8030abc) SHA1(ee0add1513f620e35583a6ec1e773f53ea27e455) )
	ROM_LOAD32_WORD_SWAP( "533jaa03.27b", 0x200000, 512*1024, CRC(4a95e80d) SHA1(ff0aef613745c07b5891e66b6b1759e048599214) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("533a08.9c", 0x000000, 128*1024, CRC(06d14cff) SHA1(21c01a06eabfacc4ac1d83bfad389f3dfa41b95f) )
	ROM_LOAD16_BYTE("533a09.7c", 0x000001, 128*1024, CRC(a93c6f0b) SHA1(bee1abab985c7163212cad1a4bc0a427804dfed3) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "533a19.17h", 0x000000, 2*1024*1024, CRC(3ec1843e) SHA1(5d2c37f1eb299c846daa63f35ccd5334a516a1f5) )
	TILE_BYTE_ROM_LOAD( "533-ja.13g", 0x000004, 512*1024,    CRC(d3e0d058) SHA1(c50bdb3493501bfbbe092d01f5d4c38bfa3412f8) )

	/* sprites */
	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "533a17.25g", 0x000000, 2*1024*1024, CRC(9947af57) SHA1(a8f67cb49cf55e8402de352bb530c7c90c643144) )
	ROM_LOAD32_WORD( "533a13.28g", 0x000002, 2*1024*1024, CRC(58f1fc38) SHA1(9662b4fb036ffe90f294ee36fa52a0c1e1dbd116) )
	ROM_LOAD( "533a11.30g", 0x400000, 2*1024*1024, CRC(983105e1) SHA1(c688f6f73fab16107f01523081558a2e02a5311c) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "533a22.9g", 0x000000, 2*1024*1024, CRC(97233814) SHA1(dba20a81517796b7baf7c82551bd7f1c1a8ecd7e) )
	ROM_LOAD( "533a23.7g", 0x200000, 2*1024*1024, CRC(1bb7552b) SHA1(3c6f96b4ab97737c3634c08b94dd304d5517d88d) )
ROM_END

/* Sexy Parodius Asia */
ROM_START( sexyparoa )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "533aaa02.31b", 0x200002, 512*1024, CRC(4fdc4298) SHA1(6aa0d6d00dada9d1bfe2b29cd342b11e2d42bf5a) )
	ROM_LOAD32_WORD_SWAP( "533aaa03.27b", 0x200000, 512*1024, CRC(9c5e07cb) SHA1(4d7dbd9b0e47d501ab3f22c48942bb9e54450d87) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("533aaa08.9c", 0x000000, 128*1024, CRC(f2e2c963) SHA1(5b4ac1df208467cfac2927ce0b340090d631f190) )
	ROM_LOAD16_BYTE("533aaa09.7c", 0x000001, 128*1024, CRC(49086451) SHA1(8fdbeb5889e476dfd3f31619d5b5280a0494de69) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "533a19.17h", 0x000000, 2*1024*1024, CRC(3ec1843e) SHA1(5d2c37f1eb299c846daa63f35ccd5334a516a1f5) )
	TILE_BYTE_ROM_LOAD( "533-ja.13g", 0x000004, 512*1024,    CRC(d3e0d058) SHA1(c50bdb3493501bfbbe092d01f5d4c38bfa3412f8) )

	/* sprites */
	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "533a17.25g", 0x000000, 2*1024*1024, CRC(9947af57) SHA1(a8f67cb49cf55e8402de352bb530c7c90c643144) )
	ROM_LOAD32_WORD( "533a13.28g", 0x000002, 2*1024*1024, CRC(58f1fc38) SHA1(9662b4fb036ffe90f294ee36fa52a0c1e1dbd116) )
	ROM_LOAD( "533a11.30g", 0x400000, 2*1024*1024, CRC(983105e1) SHA1(c688f6f73fab16107f01523081558a2e02a5311c) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "533a22.9g", 0x000000, 2*1024*1024, CRC(97233814) SHA1(dba20a81517796b7baf7c82551bd7f1c1a8ecd7e) )
	ROM_LOAD( "533a23.7g", 0x200000, 2*1024*1024, CRC(1bb7552b) SHA1(3c6f96b4ab97737c3634c08b94dd304d5517d88d) )
ROM_END

/* Run and Gun 2 */
ROM_START( rungun2 )
	/* main program */
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "505uaa02.31b", 0x200002, 512*1024, CRC(cfca23f7) SHA1(dfea871f0aaf6b2db6d924ddfd4174e7a14333e8))
	ROM_LOAD32_WORD_SWAP( "505uaa03.27b", 0x200000, 512*1024, CRC(ad7f9ded) SHA1(824448daeb6109b822667e54baa1c73484642ac9))

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "505a04.31r", 0x400000, 1024*1024, CRC(11a73f01) SHA1(0738f347f1b639130d512f31034888d2063767c0) )
	ROM_LOAD32_WORD_SWAP( "505a05.29r", 0x400002, 1024*1024, CRC(5da5d695) SHA1(02bfbfa4ba0213a23819828a9be02923740dccd6) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("505a06.9m", 0x000000, 128*1024, CRC(920013f1) SHA1(6bd7f2bdeaa777412d12eeef4ba6c7f952805739) )
	ROM_LOAD16_BYTE("505a07.7m", 0x000001, 128*1024, CRC(5641c603) SHA1(1af1f92032e7f870e1668e8d720742fb53c4d0e2) )

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "505a21.11r", 0x000000, 1024*1024, CRC(03fda175) SHA1(4fdf7cfaa0d4024a2c40bba1f229c41e0627b8c8) )
	ROM_LOAD16_BYTE( "505a20.11m", 0x000001, 1024*1024, CRC(a6a300fb) SHA1(290d97c6ec36e3cab8e6fcd5310030e00fb0ce07) )

	/* sprites */
	ROM_REGION( 0x1800000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "505a19.14r", 0x0000000, 2*1024*1024, CRC(ffde4f17) SHA1(df93853f7bd3c775a15836b0ca9042f75eb65630) )
	ROM_LOAD32_WORD( "505a15.18r", 0x0000002, 2*1024*1024, CRC(d9ab1e6c) SHA1(748a61d939bd335c1b50f440e819303552b3d5a1) )
	ROM_LOAD32_WORD( "505a18.18m", 0x0400000, 2*1024*1024, CRC(c12bacfe) SHA1(5b5f4dd9a51c7a305dd4de1354cd1df2ce75c932) )
	ROM_LOAD32_WORD( "505a14.14m", 0x0400002, 2*1024*1024, CRC(356a75b0) SHA1(5f8b7a9d06d4207f19ed0f7c89513226488afde1) )
	ROM_LOAD32_WORD( "505a13.21r", 0x0800000, 2*1024*1024, CRC(e60c5191) SHA1(02a8af81682838800489aa1123a453045d70acd8) )
	ROM_LOAD32_WORD( "505a17.16r", 0x0800002, 2*1024*1024, CRC(8176f2f5) SHA1(d7944314b35bcd5301bbfba8a5b1ed6b35b9b888) )
	ROM_LOAD32_WORD( "505a12.21m", 0x0c00000, 2*1024*1024, CRC(421d5034) SHA1(f7a85b7e41f3ddf9ddbdc6f8b6d3dbf8ba40d61b) )
	ROM_LOAD32_WORD( "505a16.16m", 0x0c00002, 2*1024*1024, CRC(ca9c2193) SHA1(cc3fb558b834e0b7914879ab47c3750170d257f4) )
	ROM_LOAD32_WORD( "505a11.23r", 0x1000000, 2*1024*1024, CRC(75c13df0) SHA1(6680f75a67ca510fac29b65bce32fef64e844695) )
	ROM_LOAD32_WORD( "505a10.23m", 0x1000002, 2*1024*1024, CRC(fc315ee0) SHA1(4dab661e0bd8e5386e52d514a1511ceba6e5b7bd) )
	ROM_LOAD32_WORD( "505a09.25r", 0x1400000, 2*1024*1024, CRC(3e1d5a15) SHA1(ec4d46c2f2cc57e6193865357ffb3d62a9eecd4f) )
	ROM_LOAD32_WORD( "505a08.25m", 0x1400002, 2*1024*1024, CRC(442ed3ec) SHA1(d44e1c4e9f8c63a8f754f8d20064cec15ae0b6d6) )

	/* PSAC2 tiles */
	ROM_REGION( 0x200000, REGION_GFX3, 0 )
	ROM_LOAD("505a24.22h", 0x000000, 2*1024*1024, CRC(70e906da) SHA1(4b1a412a71910633f48c6a0b9fd6949dcc82e365) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "505a23.7r", 0x000000, 2*1024*1024, CRC(67f03445) SHA1(9b5c1d1bb7b0ee275862d10effd9daed49568af4) )
	ROM_LOAD( "505a22.9r", 0x200000, 2*1024*1024, CRC(c2b67a9d) SHA1(9ff091972d7fad50bf0df2b3d8b5ee989e3df27f) )
ROM_END

/* Slam Dunk 2 */
ROM_START( slamdnk2 )
	/* main program */
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "505jaa02.31m", 0x200002, 512*1024, CRC(9f72d48e) SHA1(6dd0520d0f0312e46f21ad4f6c41e47f3b5cb16b) )
	ROM_LOAD32_WORD_SWAP( "505jaa03.29m", 0x200000, 512*1024, CRC(52513794) SHA1(8a8fadb0eb582db53163620982dd53d1e5f8ca4c) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "505a04.31r", 0x400000, 1024*1024, CRC(11a73f01) SHA1(0738f347f1b639130d512f31034888d2063767c0) )
	ROM_LOAD32_WORD_SWAP( "505a05.29r", 0x400002, 1024*1024, CRC(5da5d695) SHA1(02bfbfa4ba0213a23819828a9be02923740dccd6) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("505a06.9m", 0x000000, 128*1024, CRC(920013f1) SHA1(6bd7f2bdeaa777412d12eeef4ba6c7f952805739) )
	ROM_LOAD16_BYTE("505a07.7m", 0x000001, 128*1024, CRC(5641c603) SHA1(1af1f92032e7f870e1668e8d720742fb53c4d0e2) )

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "505a21.11r", 0x000000, 1024*1024, CRC(03fda175) SHA1(4fdf7cfaa0d4024a2c40bba1f229c41e0627b8c8) )
	ROM_LOAD16_BYTE( "505a20.11m", 0x000001, 1024*1024, CRC(a6a300fb) SHA1(290d97c6ec36e3cab8e6fcd5310030e00fb0ce07) )

	/* sprites */
	ROM_REGION( 0x1800000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "505a19.14r", 0x0000000, 2*1024*1024, CRC(ffde4f17) SHA1(df93853f7bd3c775a15836b0ca9042f75eb65630) )
	ROM_LOAD32_WORD( "505a15.18r", 0x0000002, 2*1024*1024, CRC(d9ab1e6c) SHA1(748a61d939bd335c1b50f440e819303552b3d5a1) )
	ROM_LOAD32_WORD( "505a18.18m", 0x0400000, 2*1024*1024, CRC(c12bacfe) SHA1(5b5f4dd9a51c7a305dd4de1354cd1df2ce75c932) )
	ROM_LOAD32_WORD( "505a14.14m", 0x0400002, 2*1024*1024, CRC(356a75b0) SHA1(5f8b7a9d06d4207f19ed0f7c89513226488afde1) )
	ROM_LOAD32_WORD( "505a13.21r", 0x0800000, 2*1024*1024, CRC(e60c5191) SHA1(02a8af81682838800489aa1123a453045d70acd8) )
	ROM_LOAD32_WORD( "505a17.16r", 0x0800002, 2*1024*1024, CRC(8176f2f5) SHA1(d7944314b35bcd5301bbfba8a5b1ed6b35b9b888) )
	ROM_LOAD32_WORD( "505a12.21m", 0x0c00000, 2*1024*1024, CRC(421d5034) SHA1(f7a85b7e41f3ddf9ddbdc6f8b6d3dbf8ba40d61b) )
	ROM_LOAD32_WORD( "505a16.16m", 0x0c00002, 2*1024*1024, CRC(ca9c2193) SHA1(cc3fb558b834e0b7914879ab47c3750170d257f4) )
	ROM_LOAD32_WORD( "505a11.23r", 0x1000000, 2*1024*1024, CRC(75c13df0) SHA1(6680f75a67ca510fac29b65bce32fef64e844695) )
	ROM_LOAD32_WORD( "505a10.23m", 0x1000002, 2*1024*1024, CRC(fc315ee0) SHA1(4dab661e0bd8e5386e52d514a1511ceba6e5b7bd) )
	ROM_LOAD32_WORD( "505a09.25r", 0x1400000, 2*1024*1024, CRC(3e1d5a15) SHA1(ec4d46c2f2cc57e6193865357ffb3d62a9eecd4f) )
	ROM_LOAD32_WORD( "505a08.25m", 0x1400002, 2*1024*1024, CRC(442ed3ec) SHA1(d44e1c4e9f8c63a8f754f8d20064cec15ae0b6d6) )

	/* PSAC2 tiles */
	ROM_REGION( 0x200000, REGION_GFX3, 0 )
	ROM_LOAD("505a24.22h", 0x000000, 2*1024*1024, CRC(70e906da) SHA1(4b1a412a71910633f48c6a0b9fd6949dcc82e365) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "505a23.7r", 0x000000, 2*1024*1024, CRC(67f03445) SHA1(9b5c1d1bb7b0ee275862d10effd9daed49568af4) )
	ROM_LOAD( "505a22.9r", 0x200000, 2*1024*1024, CRC(c2b67a9d) SHA1(9ff091972d7fad50bf0df2b3d8b5ee989e3df27f) )
ROM_END

/* Rushing Heroes */
ROM_START( rushhero )
	/* main program */
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "605b02.31m", 0x200002, 512*1024, CRC(94c3d835) SHA1(f48d34987fa6575a2c41d3ca3359e9e2cbc817e0) )
	ROM_LOAD32_WORD_SWAP( "605b03.29m", 0x200000, 512*1024, CRC(c5b8d31d) SHA1(6c5b359e1fcf511c50d6a876946631fc38a6dade) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "605a04.31r", 0x400000, 1024*1024, CRC(f6788154) SHA1(093c145d5348b4f10193acc258f5539bd59138a1))
	ROM_LOAD32_WORD_SWAP( "605a05.29r", 0x400002, 1024*1024, CRC(9bca4297) SHA1(c20be1ffcee8bd56f69d4fcc19d0035b3f74b8f2))

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("605a06.9m", 0x000000, 128*1024, CRC(9ca03dce) SHA1(008106e864d8390d7ae8645a2fe06d0eaaa746e0))
	ROM_LOAD16_BYTE("605a07.7m", 0x000001, 128*1024, CRC(3116a8b0) SHA1(f0899d7027464d9aad45ffa6a464288a51a80dc1))

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "605a21.11r", 0x000000, 1024*1024, CRC(0e5add29) SHA1(f80d81ff8110825ba19ebc3cf50480b8cf275571))
	ROM_LOAD16_BYTE( "605a20.11m", 0x000001, 1024*1024, CRC(a8fb4288) SHA1(b0ee6c2add5a8063f771ac8bbdfd78c0382a5036))

	/* sprites */
	ROM_REGION( 0x3000000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "605a19.14r", 0x0000000, 4*1024*1024, CRC(293427d0) SHA1(c31f93797bda09ea7e990100a5556eb0fde64968) )
	ROM_LOAD32_WORD( "605a15.18r", 0x0000002, 4*1024*1024, CRC(19e6e356) SHA1(b2568e14d6fb9a9792f95aafcf694dbf00c0d2c8) )
	ROM_LOAD32_WORD( "605a18.18m", 0x0800000, 4*1024*1024, CRC(b5115d76) SHA1(48c3119afb649c58d4df36806fe5530ddd379782) )
	ROM_LOAD32_WORD( "605a14.14m", 0x0800002, 4*1024*1024, CRC(4d4dbecb) SHA1(7c3cb2739d6b729d855d652b1991c7af6cd79d1c) )
	ROM_LOAD32_WORD( "605a13.21r", 0x1000000, 4*1024*1024, CRC(08137923) SHA1(c1af6b55c1c08e16384d2660b2210ccf3b955be9) )
	ROM_LOAD32_WORD( "605a17.16r", 0x1000002, 4*1024*1024, CRC(42e6dc6f) SHA1(8035b7160267a988a1aa2690423c68b6f1975f1a) )
	ROM_LOAD32_WORD( "605a12.21m", 0x1800000, 4*1024*1024, CRC(194ffad0) SHA1(1c56f4e89bfe72b435793b907e7ca3e62ecddf4b) )
	ROM_LOAD32_WORD( "605a16.16m", 0x1800002, 4*1024*1024, CRC(aab542ca) SHA1(9728b028f48768236f47a7a9bddb27944297b583) )
	ROM_LOAD32_WORD( "605a11.23r", 0x2000000, 4*1024*1024, CRC(bc61339c) SHA1(77a5737501bf8ffd7ae4192a6e5924c479eb6655) )
	ROM_LOAD32_WORD( "605a10.23m", 0x2000002, 4*1024*1024, CRC(4f47d434) SHA1(c4503993c738e1b8df6f045f5a82504363682db7) )
	ROM_LOAD32_WORD( "605a09.25r", 0x2800000, 4*1024*1024, CRC(624fd486) SHA1(edd81d5487f8239ffa89b931430cf41f06a17cf6) )
	ROM_LOAD32_WORD( "605a08.25m", 0x2800002, 4*1024*1024, CRC(ea80ddfd) SHA1(4be61af09bcc80c97505196a6f43797753d14f85) )

	/* PSAC2 tiles */
	ROM_REGION( 0x200000, REGION_GFX3, 0 )
	ROM_LOAD("605a24.22h", 0x000000, 1024*1024, CRC(e5ba1bb7) SHA1(d677e03b418a8bde7a4e4932fd452924768e7d72))

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "605a23.7r", 0x000000, 4*1024*1024, CRC(992c4751) SHA1(18ecfc21138f9dc62f0658750808bbef649510f7))
ROM_END

/* Taisen Tokkae-dama */
ROM_START( tokkae )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "615jaa02.31b", 0x200002, 512*1024, CRC(f66d6dbf) SHA1(16c4a84e37475d3773b505f33f8a0ee8168f282f) )
	ROM_LOAD32_WORD_SWAP( "615jaa03.27b", 0x200000, 512*1024, CRC(b7760e2b) SHA1(7903d8c2d32ef7d324e965e52544a9a41abf62fd) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("615a08.9c", 0x000000, 128*1024, CRC(a5340de4) SHA1(96d74624f44423e57fe8fcecadecdd76d91c27bc) )
	ROM_LOAD16_BYTE("615a09.7c", 0x000001, 128*1024, CRC(c61f954c) SHA1(5242a2872db1db9ab4edd9951c2ac2d872f06dc7) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORDS2_ROM_LOAD( "615a19.17h", 0x000000, 1*1024*1024, CRC(07749e1e) SHA1(79a5f979b1dc7fa92ae37af03447edf4885ecdf8) )
	TILE_BYTES2_ROM_LOAD( "615a20.13c", 0x000004, 512*1024,    CRC(9911b5a1) SHA1(7dc9348fd23331ca7614db27dc5f280610f87a20) )

	/* sprites */
	ROM_REGION( 0xa00000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "615a17.25g", 0x000000, 2*1024*1024, CRC(b864654b) SHA1(bbd74c992ba3c3c629520e68687d7c8f3c26d0b9) )
	ROM_LOAD32_WORD( "615a13.28g", 0x000002, 2*1024*1024, CRC(4e8afa1a) SHA1(d980104ddf9670e689236f381db3345471aff6fd) )
	ROM_LOAD32_WORD( "615a16.18h", 0x400000, 2*1024*1024, CRC(dfa0f0fe) SHA1(4f68767f8329f6348055a472d923557e7dec3154) )
	ROM_LOAD32_WORD( "615a12.27g", 0x400002, 2*1024*1024, CRC(fbc563fd) SHA1(19a6544297e0eade09e69741b9e3d8b32c7e2794) )

	ROM_LOAD( "615a11.30g", 0x800000, 2*1024*1024, CRC(f25946e4) SHA1(e7744cdbeccc7325fdb31e134fed71d4cf8f9b0a) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "615a22.9g", 0x000000, 2*1024*1024, CRC(ea7e47dd) SHA1(5bf5bad9427b083757c400eaf58c63a6267c1caf) )
	ROM_LOAD( "615a23.7g", 0x200000, 2*1024*1024, CRC(22d71f36) SHA1(3f24bb4cd8e1d693b42219e05960ad0c756b08cb) )
ROM_END

/* Tokimeki Memorial Taisen Puzzle-dama */
ROM_START( tkmmpzdm )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "515jab02.31b", 0x200002, 512*1024, CRC(60d4d577) SHA1(5320ce2e004f01ca73e2f2048a622f14ab7a053d) )
	ROM_LOAD32_WORD_SWAP( "515jab03.27b", 0x200000, 512*1024, CRC(c383413d) SHA1(1227e4c8bfdb5149896da81efa0109a55ae62708) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("515a04.9c", 0x000000, 128*1024, CRC(a9b7bb45) SHA1(ad11a7b5c24a03658ff4309dbd8c7334f4adf7b4) )
	ROM_LOAD16_BYTE("515a05.7c", 0x000001, 128*1024, CRC(dea4ca2f) SHA1(5d11469a93293381228233baad6896e098994d9b) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORDS2_ROM_LOAD( "515a11.17h", 0x000000, 1024*1024, CRC(8689852d) SHA1(42ba16a9dfba47132fe07c6b1d044c5b32753220) )
	TILE_BYTES2_ROM_LOAD( "515a12.13c", 0x000004, 512*1024, CRC(6936f94a) SHA1(e2c7fc327638ee39eef6109c4f164eaf98972f00) )

	/* sprites */
	ROM_REGION( 0xa00000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "515a10.25g", 0x000000, 2*1024*1024, CRC(e6e7ab7e) SHA1(3f7ddab4b814673264b542d2a8761c56f82f2180) )
	ROM_LOAD32_WORD( "515a08.28g", 0x000002, 2*1024*1024, CRC(5613daea) SHA1(43480c8104582cc27d8ab6920ec113e660de5ae7) )
	ROM_LOAD32_WORD( "515a09.18h", 0x400000, 2*1024*1024, CRC(28ffdb48) SHA1(8511def7bb151f912755c2bbcb0cae1a2e52f405) )
	ROM_LOAD32_WORD( "515a07.27g", 0x400002, 2*1024*1024, CRC(246e6cb1) SHA1(a320e0820895717c765d07f80cf7983b502af8f0) )
	ROM_LOAD( "515a06.30g", 0x800000, 2*1024*1024, CRC(13b7b953) SHA1(4393c5b3515f3ded9db3ac5d59308f99f40f2b76) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "515a13.9g", 0x000000, 2*1024*1024, CRC(4b066b00) SHA1(874dd49847b10e6d9c39decb81557534baa36d79) )
	ROM_LOAD( "515a14.7g", 0x200000, 2*1024*1024, CRC(128cc944) SHA1(b0cd2ec1b9a2ac936d57b6d6c2a70f9c13dc97a5) )
ROM_END

/* Winning Spike */
ROM_START( winspike )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "705jaa02.31b", 0x200002, 512*1024, CRC(85f11b03) SHA1(ab597d16df654179050cc029179aaf946f0426c5) )
	ROM_LOAD32_WORD_SWAP( "705jaa03.27b", 0x200000, 512*1024, CRC(1d5e3922) SHA1(b5e1fd1ea4f872159522e64c2725bf8441a58ef9) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("705a08.9c", 0x000000, 128*1024, CRC(0d531639) SHA1(14a72925f75528e7b4d6b701b2b51f4557f031f0) )
	ROM_LOAD16_BYTE("705a09.7c", 0x000001, 128*1024, CRC(24e58845) SHA1(a01caced5bad9d98a3f33d72ca5eb9096c45e4ba) )

	/* tiles: length of 1 meg each is TRUSTED by the internal checksum code */
	/* do NOT change these to the 4 meg dumps again, those are WRONG!!!!!!! */
	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_ERASE00 )
    ROM_LOAD16_BYTE( "705a19.17h", 0x000000, 0x100000, CRC(bab84b30) SHA1(8522a0dc5e37524f51d632e9d975e949a14c0dc3) )
	ROM_LOAD16_BYTE( "705a18.22h", 0x000001, 0x100000, CRC(eb97fb5f) SHA1(13de0ad060fd6f1312fa10edde1fef6481e8df64) )

	/* sprites */
	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_ERASE00 )
        _64_WORD_ROM_LOAD( "705a10.33g",   0x000000, 0x400000, CRC(fc4dc78b) SHA1(520cdcf9ca20ec1c84be734e06e183e7a871090b) )
        _64_WORD_ROM_LOAD( "705a11.30g",   0x000002, 0x400000, CRC(68542ce9) SHA1(a4294da1d1026e3a9d070575e5855935389a705f) )
        _64_WORD_ROM_LOAD( "705a13.28g",   0x000004, 0x400000, CRC(3b62584b) SHA1(69718f47ff1e8d65a11972af1ed5068db175f625) )
        _64_WORD_ROM_LOAD( "705a17.25g",   0x000006, 0x400000, CRC(971d2812) SHA1(ee0819faf6f6c8420d5d3742cb39dfb76b9ce7a4) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "705a22.9g", 0x000000, 4*1024*1024, CRC(1a9246f6) SHA1(a40ff43310d035f7b88c4e397a4ee75151578c17) )
ROM_END

/* Taisen Puzzle-dama */
ROM_START( puzldama )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "315jaa02.31b", 0x200002, 512*1024, CRC(e0a35c7d) SHA1(6f35dc0c43fa331fc71968c67f111fb73e5add2f) )
	ROM_LOAD32_WORD_SWAP( "315jaa04.27b", 0x200000, 512*1024, CRC(abe4f0e7) SHA1(ece76088c61eddcad2efb554937d24d642d38be6) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("315a06.9c", 0x000000, 128*1024, CRC(06580a9f) SHA1(75e13aa13e3c1060cdd630c101d6644b3904317f) )
	ROM_LOAD16_BYTE("315a07.7c", 0x000001, 128*1024, CRC(431c58f3) SHA1(4888e305875d56cca5e1d792bdf27e57b3e42b03) )

	/* tiles */
	ROM_REGION( 0xa00000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "315a14.17h", 0x000000, 512*1024, CRC(0ab731e0) SHA1(1f7d6ce40e689e1dddfee656bb46bd044012c2d6) )
	TILE_BYTE_ROM_LOAD( "315a12.13g", 0x000004, 2*1024*1024, CRC(3047b8d2) SHA1(99fa4d20ee5aae89b9093ceb581f187bc9acc0ae) )

	/* sprites */
	ROM_REGION( 0x500000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "315a11.25g", 0x000000, 2*1024*1024, CRC(b8a99c29) SHA1(60086f663aa6cbfc3fb378caeb2509c65637564e) )
	ROM_LOAD32_WORD( "315a10.28g", 0x000002, 2*1024*1024, CRC(77d175dc) SHA1(73506df30db5ce38a9a21a1dce3e8b4cc1dfa7be) )
	ROM_LOAD( "315a09.30g", 0x400000, 1*1024*1024, CRC(82580329) SHA1(99749a67f1843dfd0fe93cc6bbcbc126b7bb7fb4) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "315a17.9g", 0x000000, 2*1024*1024, CRC(ea763d61) SHA1(2a7dcb2a2a23c9fea62fb82ffc18949bf15b9f6f) )
	ROM_LOAD( "315a18.7g", 0x200000, 2*1024*1024, CRC(6e416cee) SHA1(145a766ad2fa2b692692053dd36e0caf51d67a56) )
ROM_END

/* Dragoon Might */
ROM_START( dragoonj )
	/* main program */
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "417jaa02.31b", 0x200002, 512*1024, CRC(533cbbd5) SHA1(4b7a0345ce0e503c647c7cde6f284ad0ee10f0ff) )
	ROM_LOAD32_WORD_SWAP( "417jaa03.27b", 0x200000, 512*1024, CRC(8e1f883f) SHA1(e9f25c0fae7491c55812fda336436a2884c4d417) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "417a04.26c", 0x400002, 1024*1024, CRC(dc574747) SHA1(43cbb6a08c27bb96bb25568c3b636c44fff3e08e) )
	ROM_LOAD32_WORD_SWAP( "417a05.23c", 0x400000, 1024*1024, CRC(2ee2c587) SHA1(a1b2b288c375a3406d4b12e66c973484c03fe26e) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("417a06.9c", 0x000000, 128*1024, CRC(8addbbee) SHA1(fdb38fab1fd65b7362578b108bf6128e926b5f13) )
	ROM_LOAD16_BYTE("417a07.7c", 0x000001, 128*1024, CRC(c1fd7584) SHA1(1b204165ef07b6b53f47adc16eed69d11dab53b2) )

	/* tiles */
	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "417a16.17h", 0x000000, 2*1024*1024, CRC(88b2213b) SHA1(ac4ac57618cf98d7486b147f5494e6943bff1a4d) )

	/* sprites */
	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "417a15.25g", 0x000000, 2*1024*1024, CRC(83bccd01) SHA1(c0e65c43115164c3f64ac14a449c65c4e3e3c4cf) )
	ROM_LOAD32_WORD( "417a11.28g", 0x000002, 2*1024*1024, CRC(624a7c4c) SHA1(5fda37cd02b4dcb328b80b29041214c685c77a78) )
	ROM_LOAD32_WORD( "417a14.18h", 0x400000, 2*1024*1024, CRC(fbf551f1) SHA1(871c5804aba9845aa04596db51def3ba3b8bae30) )
	ROM_LOAD32_WORD( "417a10.27g", 0x400002, 2*1024*1024, CRC(18fde49f) SHA1(f85b2981172be2cddc5d691bb803f0133a36cb1a) )
	ROM_LOAD32_WORD( "417a13.20h", 0x800000, 2*1024*1024, CRC(d2e3959d) SHA1(efe516e6b84c67c0a154726a0f7f7054ee866738) )
	ROM_LOAD32_WORD( "417a09.30g", 0x800002, 2*1024*1024, CRC(b5653e24) SHA1(ffa44d6b65feef298fa4dcc064ebd173c7cc22aa) )
	ROM_LOAD32_WORD( "417a12.23h", 0xc00000, 2*1024*1024, CRC(25496115) SHA1(e53164f8ad95187011059c465a67fff1d18ba888) )
	ROM_LOAD32_WORD( "417a08.33g", 0xc00002, 2*1024*1024, CRC(801e9d93) SHA1(9364d802b4ca03e652b25304c8298be8de8936b4) )

	/* sound data */
	ROM_REGION( 0x200000, REGION_SOUND1, 0 )
	ROM_LOAD( "417a17.9g", 0x000000, 2*1024*1024, CRC(88d47dfd) SHA1(b5d6dd7ee9ac0c427dc3e714a97945c954260913) )
ROM_END

/* Soccer Superstars */
ROM_START( soccerss )
	/* main program */
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "427jaa02.28m", 0x200000, 512*1024, CRC(210f9ba7) SHA1(766fc821d1c7aaf9e306c6e2379d85e7aa50738c) )
	ROM_LOAD32_WORD_SWAP( "427jaa03.30m", 0x200002, 512*1024, CRC(f76add04) SHA1(755dff41ce3b0488ed8f9f5feebfe95a22b70d16) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "427a04.28r",   0x400000, 0x080000, CRC(c7d3e1a2) SHA1(5e1e4f4c97def36902ad853248014a7af62e0c5e) )
	ROM_LOAD32_WORD_SWAP( "427a05.30r",   0x400002, 0x080000, CRC(5372f0a5) SHA1(36e8d0a73918cbd018c1865d1a05445daba8997c) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("427a07.6m", 0x000000, 128*1024, CRC(8dbaf4c7) SHA1(cb69bf94090a4871b35e7ba1f58e3225077b82cd) )
	ROM_LOAD16_BYTE("427a06.9m", 0x000001, 128*1024, CRC(979df65d) SHA1(7499e9a27aa562692bd3a296789696492a6254bc) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORDS2_ROM_LOAD( "427a15.11r", 0x000000, 0x100000, CRC(33ce2b8e) SHA1(b0936386cdc7c41f33b1d7b4f5ce25fe618d1286) )
	TILE_BYTES2_ROM_LOAD( "427a14.143", 0x000004, 0x080000, CRC(7575a0ed) SHA1(92fda2747ac090f93e60cff8478af6721b949dc2) )

	/* sprites */
	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "427a08.140", 0x000000, 2*1024*1024, CRC(221250af) SHA1(fd24e7f0e3024df5aa08506523953c5e35d2267b) )
	ROM_LOAD32_WORD( "427a09.137", 0x000002, 2*1024*1024, CRC(56bdd480) SHA1(01d164aedc77f71f6310cfd739c00b33289a2e7e) )
	ROM_LOAD32_WORD( "427a10.25r", 0x400000, 2*1024*1024, CRC(6b3ccb41) SHA1(b246ef350a430e60f0afd1b80ff48139c325e926) )
	ROM_LOAD32_WORD( "427a11.23r", 0x400002, 2*1024*1024, CRC(c1ca74c1) SHA1(b7286df8e59f8f1939ebf17aaf9345a857b0b100) )
	ROM_LOAD32_WORD( "427a12.21r", 0x800000, 2*1024*1024, CRC(97d6fd38) SHA1(8d2895850cafdea95db08c84e7eeea90a1921515) )
	ROM_LOAD32_WORD( "427a13.18r", 0x800002, 2*1024*1024, CRC(815a9b87) SHA1(7d9d5932fff7dd7aa4cbccf0c8d3784dc8042e70) )

	/* PSAC2 tiles */
	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_ERASE00 )
	ROM_LOAD( "427a18.145", 0x000000, 0x100000, CRC(bb6e6ec6) SHA1(aa1365a4318866d9e7e74461a6e6c113f83b6771) )

	/* PSAC2 map data */
	ROM_REGION( 0x080000, REGION_GFX4, ROMREGION_ERASE00 )
	ROM_LOAD( "427a17.24c", 0x000000, 0x080000, CRC(fb6eb01f) SHA1(28cdb30ff70ee5fc7624e18fe048dd85dfa49ace) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "427a16.9r", 0x000000, 2*1024*1024,  CRC(39547265) SHA1(c0efd68c0c1ea59141045150842f36d43e1f01d8) )
ROM_END

/* Vs. Net Soccer */
ROM_START(vsnetscr)
	/* main program */
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "627uab03.29m", 0x200000, 512*1024, CRC(53ca7eec) SHA1(d2d5a491417849c31aaff61a93da4ab2e94495d4) )
	ROM_LOAD32_WORD_SWAP( "627uab02.31m", 0x200002, 512*1024, CRC(c352cc6f) SHA1(d8d0d802eb6bd0910e35dcc6b81b7ac9036e32ea) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "627a05.31r", 0x400000, 1024*1024, CRC(be4e7b3c) SHA1(f44e7b1913aa54f759bd31bb86fdedbb9747b2d5) )
	ROM_LOAD32_WORD_SWAP( "627a04.29r", 0x400002, 1024*1024, CRC(17334e9a) SHA1(82cdba016c29160550c43feee7a4feff6e1184aa) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("627b06.9m", 0x000000, 128*1024, CRC(c8337b9d) SHA1(574c674676f493ca4b5a135728ce01e664d1293d) )
	ROM_LOAD16_BYTE("627b07.7m", 0x000001, 128*1024, CRC(d7d92579) SHA1(929b8e90cfef2ef14d84173267b637e4efdb6867) )

	/* tiles */
	ROM_REGION( 0x500000, REGION_GFX1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "627a21.11r", 0x000000, 1024*1024, CRC(d0755fb8) SHA1(de37ea2a7969a97b6f2abccb7dc2a58950482bf0) )
	ROM_LOAD16_BYTE( "627a20.11m", 0x000001, 1024*1024, CRC(f68b28f2) SHA1(1463717ed581494fcab77a80dc6ffd3ab82ab1fa) )

	/* sprites */
	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD( "627a19.14r", 0x000000, 2*1024*1024, CRC(5efaa3bc) SHA1(95314c1054ccf5b9626f0b06f9e1c857a127e2ca) )
	ROM_LOAD32_WORD( "627a15.18r", 0x000002, 2*1024*1024, CRC(5180ca06) SHA1(d5569f6fc6b0374cd111f8313f635e4b7c49351f) )
	ROM_LOAD32_WORD( "627a13.21r", 0x400000, 2*1024*1024, CRC(af48849d) SHA1(c43981883ef042968444b6d993a640edc429daae) )
	ROM_LOAD32_WORD( "627a17.16r", 0x400002, 2*1024*1024, CRC(ca99d29c) SHA1(919dfb029dbc7d2c5e420d54df36eef3ec3bb1a2) )
	ROM_LOAD32_WORD( "627a11.23r", 0x800000, 2*1024*1024, CRC(a2e507f2) SHA1(ceaaccbac22fecef32fa34f887568bb18464265d) )
	ROM_LOAD32_WORD( "627a09.25r", 0x800002, 2*1024*1024, CRC(312cf8a4) SHA1(107456fc1a1906a60b5b50f4ca6b7e8cd258e6ee) )

	/* PSAC2 tiles */
	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_ERASE00)
	ROM_LOAD( "627a24.22h", 0x000000, 0x200000, CRC(2cd73305) SHA1(5a46148c08198499639adc4b6936af0b2b530bc9) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	/* probably should be 4M byte mode like Rushing Heroes*/
	ROM_LOAD( "627a23.7r", 0x000000, 0x200000, BAD_DUMP CRC(b0266eb6) SHA1(65fafcee4b8f9a6e9f05df6f1fb4db519e97f776) )
ROM_END

/* Lethal Enforcers II */
ROM_START( le2 )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_BYTE( "312eaa05.26b", 0x200000, 128*1024, CRC(875f6561) SHA1(8ede0697ff91134011f9fb5eb26e596cbc6e8f12) )
	ROM_LOAD32_BYTE( "312eaa04.28b", 0x200001, 128*1024, CRC(d5fb8d30) SHA1(824d6a43bed5aff2f65096922620ff5bff3b29f9) )
	ROM_LOAD32_BYTE( "312eaa03.30b", 0x200002, 128*1024, CRC(cfe07036) SHA1(cd7181ace76feb0e684a51db488b64b86ced5f55) )
	ROM_LOAD32_BYTE( "312eaa02.33b", 0x200003, 128*1024, CRC(5094b965) SHA1(cb597b663c49d0f63af770fd6ce344e5df9a1ed9) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("312b06.9c", 0x000000, 128*1024, CRC(a6f62539) SHA1(b333e02c55237a0429d8c5386ec68b67797a1107) )
	ROM_LOAD16_BYTE("312b07.7c", 0x000001, 128*1024, CRC(1aa19c41) SHA1(5b879fb17ac514f266e63db6af50f2f4af7da32c) )

	/* tiles */
	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "312a14.17h", 0x000000, 2*1024*1024, CRC(dc862f19) SHA1(8ec9f8715b622462fb8c79a48162c161eb9fe13b) )
	ROM_LOAD16_BYTE( "312a12.22h", 0x000001, 2*1024*1024, CRC(98c04ddd) SHA1(7bc7af21625466e75003da9fd950437249e75b78) )
	ROM_LOAD16_BYTE( "312a15.15h", 0x400000, 2*1024*1024, CRC(516f2941) SHA1(07415fec2d96fe6b707f801a9e9e963186d83d6a) )
	ROM_LOAD16_BYTE( "312a13.20h", 0x400001, 2*1024*1024, CRC(16e5fdaa) SHA1(f04e09ee4207eb2bd67533997d36f4b3cf42a439) )

	/* sprites */
	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_ERASE00 )
	_64_WORD_ROM_LOAD( "312a08.33g", 0x000000, 2*1024*1024, CRC(29015d56) SHA1(7273270804ecefd8f59469c2c2a8a89fb045a12b) )
	_64_WORD_ROM_LOAD( "312a09.30g", 0x000002, 2*1024*1024, CRC(b2c5d6d5) SHA1(8248612275ca862c6688de5c6f24f37aeb3f9fe5) )
	_64_WORD_ROM_LOAD( "312a10.28g", 0x000004, 2*1024*1024, CRC(3c570d04) SHA1(ebbf7d28726e98c8895c9bf901f8b2dd38018c77) )
	_64_WORD_ROM_LOAD( "312a11.25g", 0x000006, 2*1024*1024, CRC(5f474357) SHA1(1f6d99f1ea69e07a65731ea4eae5917452cfcab6) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "312a17.9g", 0x000000, 2*1024*1024, CRC(ed101448) SHA1(ef1342f37fbbb092eddee0c237b40989ad42cf26) )
	ROM_LOAD( "312a18.7g", 0x200000, 1*1024*1024, CRC(5717abd7) SHA1(d304d733e7fca0363ea6b3872c2d3bbe4edf1179) )
ROM_END

/* Lethal Enforcers II (US Version) */
ROM_START( le2u )
	/* main program */
	ROM_REGION( 0x300000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_BYTE( "312uaa05.26b", 0x200000, 128*1024, CRC(973aa500) SHA1(ad834b0fed37502ff3e3fe7f608fd5dbe172f452) )
	ROM_LOAD32_BYTE( "312uaa04.28b", 0x200001, 128*1024, CRC(cba39552) SHA1(243f2115047d9e646f69368d78b18be47b7eacc6) )
	ROM_LOAD32_BYTE( "312uaa03.30b", 0x200002, 128*1024, CRC(20bc94e6) SHA1(652ca7b3b7ece9e702134691373e913ce3651401) )
	ROM_LOAD32_BYTE( "312uaa02.33b", 0x200003, 128*1024, CRC(04f3bd9e) SHA1(0617fd065efeae8c5fa665a65ed69a246d7a8ed7) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("312a06.9c", 0x000000, 128*1024, CRC(ff6f2cd4) SHA1(a52cab6b64b54bd60e96437ed91277c76293af2d) )
	ROM_LOAD16_BYTE("312a07.7c", 0x000001, 128*1024, CRC(3d31e989) SHA1(1fdf205b0f9c21093bc6147aaacdf178aa628508) )

	/* tiles */
	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "312a14.17h", 0x000000, 2*1024*1024, CRC(dc862f19) SHA1(8ec9f8715b622462fb8c79a48162c161eb9fe13b) )
	ROM_LOAD16_BYTE( "312a12.22h", 0x000001, 2*1024*1024, CRC(98c04ddd) SHA1(7bc7af21625466e75003da9fd950437249e75b78) )
	ROM_LOAD16_BYTE( "312a15.15h", 0x400000, 2*1024*1024, CRC(516f2941) SHA1(07415fec2d96fe6b707f801a9e9e963186d83d6a) )
	ROM_LOAD16_BYTE( "312a13.20h", 0x400001, 2*1024*1024, CRC(16e5fdaa) SHA1(f04e09ee4207eb2bd67533997d36f4b3cf42a439) )

	/* sprites */
	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_ERASE00 )
	_64_WORD_ROM_LOAD( "312a08.33g", 0x000000, 2*1024*1024, CRC(29015d56) SHA1(7273270804ecefd8f59469c2c2a8a89fb045a12b) )
	_64_WORD_ROM_LOAD( "312a09.30g", 0x000002, 2*1024*1024, CRC(b2c5d6d5) SHA1(8248612275ca862c6688de5c6f24f37aeb3f9fe5) )
	_64_WORD_ROM_LOAD( "312a10.28g", 0x000004, 2*1024*1024, CRC(3c570d04) SHA1(ebbf7d28726e98c8895c9bf901f8b2dd38018c77) )
	_64_WORD_ROM_LOAD( "312a11.25g", 0x000006, 2*1024*1024, CRC(5f474357) SHA1(1f6d99f1ea69e07a65731ea4eae5917452cfcab6) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "312a17.9g", 0x000000, 2*1024*1024, CRC(ed101448) SHA1(ef1342f37fbbb092eddee0c237b40989ad42cf26) )
	ROM_LOAD( "312a18.7g", 0x200000, 1*1024*1024, CRC(5717abd7) SHA1(d304d733e7fca0363ea6b3872c2d3bbe4edf1179) )
ROM_END

/* Racin' Force */
ROM_START( racinfrc )
	/* main program */
	ROM_REGION( 0x800000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "250uab02.34n", 0x200000, 512*1024, CRC(315040c6) SHA1(940d54c1eb898d9a44d823f9f5ae9e91a20f746f) )
	ROM_LOAD32_WORD_SWAP( "250uab03.31n", 0x200002, 512*1024, CRC(171134ab) SHA1(308b7e76a80c3d860a15408a144b1e0f76fcee87) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "250a04.34s", 0x400000, 2*1024*1024, CRC(45e4d43c) SHA1(a668431d53b50fd41e1fa3c8959c0dc96e50c52b) )
	ROM_LOAD32_WORD_SWAP( "250a05.31s", 0x400002, 2*1024*1024, CRC(a235af3e) SHA1(381cd16552f007ccb508411a03fdfd18e32203d0) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("250a06.8p", 0x000000, 128*1024, CRC(2d0a3ff1) SHA1(ce4261d5f86821e98e971a35403c793506d0566b) )
	ROM_LOAD16_BYTE("250a07.6p", 0x000001, 128*1024, CRC(612b670a) SHA1(255515fa5096fcc4681b32defa0ae855286d8ed1) )

	/* tiles */
	ROM_REGION( 0x300000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORDS2_ROM_LOAD( "250a15.19y", 0x000000, 0x100000, CRC(60abc472) SHA1(ff360d81222e2d8cd55b907ca5a9947f958aaaab) )
	TILE_BYTES2_ROM_LOAD( "250a14.21y", 0x000004, 0x080000, CRC(d14abf98) SHA1(14827a01deb659c96fd38a5c76f1c9cead5f83c7) )

	/* sprites */
	ROM_REGION( 0xa00000, REGION_GFX2, ROMREGION_ERASE00 )
        ROM_LOAD32_WORD( "250a12.26y", 0x000000, 0x200000, CRC(e4ca3cff) SHA1(5dfddda4b5257e98a53fb8669714004ae3aeb3a7) )
	ROM_LOAD32_WORD( "250a10.31y", 0x000002, 0x200000, CRC(75c02d12) SHA1(3ca471d887b92261b1c3f50777903df13f07b1a9) )
        ROM_LOAD32_WORD( "250a13.24y", 0x400000, 0x200000, CRC(7aeef929) SHA1(9f656e2ede27aea7d51f0f0a3a91a8f2c2d250c0) )
        ROM_LOAD32_WORD( "250a11.28y", 0x400002, 0x200000, CRC(dfbce309) SHA1(831444e7a7588833ffc9b712412f7aef34a7fa2e) )
	ROM_LOAD( "250a08.36y", 0x800000, 0x200000, CRC(25ff6414) SHA1(0af4ef7fe00d7da5fcb5dd0770d470a556c62d61) )

	/* K053936 tiles (CROM and HROM from the schematics) */
	ROM_REGION( 0x300000, REGION_GFX3, ROMREGION_ERASE00 )
	T1_PSAC6_ROM_LOAD( "250a20.10d", 0x000000, 0x100000, CRC(26a2fcaf) SHA1(d2e38dc0c61e6fed93441dfe2b811993ac9f0ad3) )
	T1_PSAC6_ROM_LOAD( "250a21.7d",  0x000001, 0x100000, CRC(370d7771) SHA1(59ab52287d5aca37baa68d941db165d8da212c69) )
	T1_PSAC6_ROM_LOAD( "250a22.5d",  0x000002, 0x100000, CRC(c66a7775) SHA1(80087b2a3a221f8b2d6c4d1c1c535602e611b561) )

	ROM_REGION( 0x300000, REGION_GFX4, ROMREGION_ERASE00 )
	T1_PSAC6_ROM_LOAD( "250a24.10h", 0x000000, 0x100000, CRC(a14547da) SHA1(a379ff2f62b340a6ea46c84878a865ccff0d132c) )
	T1_PSAC6_ROM_LOAD( "250a25.7h",  0x000001, 0x100000, CRC(58310501) SHA1(e0be82f112fd86cdb448c9c8ceda0ad4cc03e3e4) )
	T1_PSAC6_ROM_LOAD( "250a26.5h",  0x000002, 0x100000, CRC(f72e4cbe) SHA1(822895b42fe4dc8fc1c55501009b6d6e57ee46a1) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "250a17.14y", 0x000000, 2*1024*1024, CRC(adefa079) SHA1(d25911e3a02d92dc936c3d7e9d76fc270bd1a75a) )
	ROM_LOAD( "250a18.12y", 0x200000, 2*1024*1024, CRC(8014a2eb) SHA1(d82f0a7d559340ae05a78ecc8bb69bb35b9c0658) )
ROM_END

/* Konami Open Golf Championship */
ROM_START( opengolf )
	/* main program */
	ROM_REGION( 0x800000, REGION_CPU1, 0 )
	GX_BIOS
	ROM_LOAD32_WORD_SWAP( "218ead02.34n", 0x200000, 512*1024, CRC(eeb58816) SHA1(fe88a4515b552975f78897543bc564495b69dd68) )
	ROM_LOAD32_WORD_SWAP( "218ead03.31n", 0x200002, 512*1024, CRC(5c36f84c) SHA1(8dcbc9e1a8857be9d407a9d0e962a62b963e7187) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "218a04.34s", 0x400000, 512*1024, CRC(e50043a7) SHA1(d3a8e214362c34c4151408f422c9b1c723f7f01c) )
	ROM_LOAD32_WORD_SWAP( "218a05.31s", 0x400002, 512*1024, CRC(46c6b5d3) SHA1(e59c6d2dac9db635589149e4b4852e5f6a9c3c4f) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("218a06.8p", 0x000000, 128*1024, CRC(6755ccf9) SHA1(3bcb18302c385a937c748cf586970c12cef21b38) )
	ROM_LOAD16_BYTE("218a07.6p", 0x000001, 128*1024, CRC(221e5293) SHA1(44b0b4fa37da4c19c29d4d2e5b93b94fbec03633) )

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "218a15.19y", 0x000000, 2*1024*1024, CRC(78ddc8af) SHA1(24313881dbf7e1b06da008080b0143c3ca5e15b1) )
	TILE_WORD_ROM_LOAD( "218a16.16y", 0x280000, 512*1024,    CRC(a41a3ec8) SHA1(dfef4c3e4d6d4e453a4958f2bd52788497c64093) )
	TILE_BYTE_ROM_LOAD( "218a14.22y", 0x000004, 1*1024*1024, CRC(508cd75e) SHA1(adfaac92bc55f60b178a5817c48774a664d8980d) )

	/* sprites */
	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASE00 )
	_48_WORD_ROM_LOAD( "218a12.26y", 0x000000, 2*1024*1024, CRC(83158653) SHA1(b7e43d63f12a793b18ce9fc9cc2c38352d211905) )
	_48_WORD_ROM_LOAD( "218a10.31y", 0x000002, 2*1024*1024, CRC(059bfee3) SHA1(e7f4621313b7f9a6cad81d455700172654bc7404) )
	_48_WORD_ROM_LOAD( "218a08.35y", 0x000004, 2*1024*1024, CRC(5b7098f3) SHA1(91bedf731e94d1554f9a8f86f79425a2c58bbaf9) )
	_48_WORD_ROM_LOAD( "218a13.24y", 0x600000, 1*1024*1024, CRC(b9ffd12a) SHA1(f8a689957b8ff966a38f999a679cdbf18a6dfc77) )
	_48_WORD_ROM_LOAD( "218a11.28y", 0x600002, 1*1024*1024, CRC(b57231e5) SHA1(e1cd1854e909ca53dc2c32b27e5f9bb2217b0e4c) )
	_48_WORD_ROM_LOAD( "218a09.33y", 0x600004, 1*1024*1024, CRC(13627443) SHA1(b51758e19ed7d6bb1e313f7c8a509ad1aad8b22c) )

	/* K053936 tiles (CROM and HROM from the schematics) */
	ROM_REGION( 0x800000, REGION_GFX3, ROMREGION_ERASE00 )
	T1_PSAC8_ROM_LOAD( "218a20.10d", 0x000000, 0x200000, CRC(f0ac2d6f) SHA1(acae9e20d663932a9a44a2e1089546338442c71f) )
	T1_PSAC8_ROM_LOAD( "218a21.7d",  0x000001, 0x200000, CRC(cb15122a) SHA1(2d159dcfbd4a7fc6e824c1be0cc5b81dee7ce8a3) )
	T1_PSAC8_ROM_LOAD( "218a22.5d",  0x000002, 0x200000, CRC(1b08d7dc) SHA1(2b963dbc415a30d3545ea730e47a592798f30a45) )
	T1_PSAC8_ROM_LOAD( "218a23.3d",  0x000003, 0x200000, CRC(1e4224b5) SHA1(f34849d500a35001944da6b8864c796e7a0a7224) )

	ROM_REGION( 0x600000, REGION_GFX4, ROMREGION_ERASE00 )
	T1_PSAC6_ROM_LOAD( "218a24.10h", 0x000000, 0x200000, CRC(e938d96a) SHA1(176a9bfd252f01bc034ca348d018705926a1a545) )
	T1_PSAC6_ROM_LOAD( "218a25.7h",  0x000001, 0x200000, CRC(11600c2d) SHA1(721c9361761dd20810ff18c63543b222c98a47a4) )
	T1_PSAC6_ROM_LOAD( "218a26.5h",  0x000002, 0x200000, CRC(b37e4b7a) SHA1(3d21e540a366f6ef8ba761855fceecd8591179d7) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "218a17.14y", 0x000000, 2*1024*1024, CRC(0b525127) SHA1(218b306c12e1094a676815b7dddaf13bf19be2d5) )
	ROM_LOAD( "218a18.12y", 0x200000, 1*1024*1024, CRC(98ec4cfb) SHA1(638753f9d9269719a37133b9c39c242507fdd8ac) )
ROM_END

/* Golfing Greats 2 */
ROM_START( ggreats2 )
	/* main program */
	ROM_REGION( 0x800000, REGION_CPU1, 0 )
	GX_BIOS
        ROM_LOAD32_WORD_SWAP( "218jac02.34n", 0x200000, 0x080000, CRC(e4d47f92) SHA1(4761cf69670d97db548b146b127c9af04f8809d6) )
        ROM_LOAD32_WORD_SWAP( "218jac03.31n", 0x200002, 0x080000, CRC(ec10c0b2) SHA1(4084d8420c53e73630786adf1a4f0fde548fa212) )

	/* data roms */
	ROM_LOAD32_WORD_SWAP( "218a04.34s", 0x400000, 512*1024, CRC(e50043a7) SHA1(d3a8e214362c34c4151408f422c9b1c723f7f01c) )
	ROM_LOAD32_WORD_SWAP( "218a05.31s", 0x400002, 512*1024, CRC(46c6b5d3) SHA1(e59c6d2dac9db635589149e4b4852e5f6a9c3c4f) )

	/* sound program */
	ROM_REGION( 0x40000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("218a06.8p", 0x000000, 128*1024, CRC(6755ccf9) SHA1(3bcb18302c385a937c748cf586970c12cef21b38) )
	ROM_LOAD16_BYTE("218a07.6p", 0x000001, 128*1024, CRC(221e5293) SHA1(44b0b4fa37da4c19c29d4d2e5b93b94fbec03633) )

	/* tiles */
	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_ERASE00 )
	TILE_WORD_ROM_LOAD( "218a15.19y", 0x000000, 2*1024*1024, CRC(78ddc8af) SHA1(24313881dbf7e1b06da008080b0143c3ca5e15b1) )
	TILE_WORD_ROM_LOAD( "218a16.16y", 0x280000, 512*1024,    CRC(a41a3ec8) SHA1(dfef4c3e4d6d4e453a4958f2bd52788497c64093) )
	TILE_BYTE_ROM_LOAD( "218a14.22y", 0x000004, 1*1024*1024, CRC(508cd75e) SHA1(adfaac92bc55f60b178a5817c48774a664d8980d) )

	/* sprites */
	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASE00 )
	_48_WORD_ROM_LOAD( "218a12.26y", 0x000000, 2*1024*1024, CRC(83158653) SHA1(b7e43d63f12a793b18ce9fc9cc2c38352d211905) )
	_48_WORD_ROM_LOAD( "218a10.31y", 0x000002, 2*1024*1024, CRC(059bfee3) SHA1(e7f4621313b7f9a6cad81d455700172654bc7404) )
	_48_WORD_ROM_LOAD( "218a08.35y", 0x000004, 2*1024*1024, CRC(5b7098f3) SHA1(91bedf731e94d1554f9a8f86f79425a2c58bbaf9) )
	_48_WORD_ROM_LOAD( "218a13.24y", 0x600000, 1*1024*1024, CRC(b9ffd12a) SHA1(f8a689957b8ff966a38f999a679cdbf18a6dfc77) )
	_48_WORD_ROM_LOAD( "218a11.28y", 0x600002, 1*1024*1024, CRC(b57231e5) SHA1(e1cd1854e909ca53dc2c32b27e5f9bb2217b0e4c) )
	_48_WORD_ROM_LOAD( "218a09.33y", 0x600004, 1*1024*1024, CRC(13627443) SHA1(b51758e19ed7d6bb1e313f7c8a509ad1aad8b22c) )

	/* K053936 tiles (CROM and HROM from the schematics) */
	ROM_REGION( 0x800000, REGION_GFX3, ROMREGION_ERASE00 )
	T1_PSAC8_ROM_LOAD( "218a20.10d", 0x000000, 0x200000, CRC(f0ac2d6f) SHA1(acae9e20d663932a9a44a2e1089546338442c71f) )
	T1_PSAC8_ROM_LOAD( "218a21.7d",  0x000001, 0x200000, CRC(cb15122a) SHA1(2d159dcfbd4a7fc6e824c1be0cc5b81dee7ce8a3) )
	T1_PSAC8_ROM_LOAD( "218a22.5d",  0x000002, 0x200000, CRC(1b08d7dc) SHA1(2b963dbc415a30d3545ea730e47a592798f30a45) )
	T1_PSAC8_ROM_LOAD( "218a23.3d",  0x000003, 0x200000, CRC(1e4224b5) SHA1(f34849d500a35001944da6b8864c796e7a0a7224) )

	ROM_REGION( 0x600000, REGION_GFX4, ROMREGION_ERASE00 )
	T1_PSAC6_ROM_LOAD( "218a24.10h", 0x000000, 0x200000, CRC(e938d96a) SHA1(176a9bfd252f01bc034ca348d018705926a1a545) )
	T1_PSAC6_ROM_LOAD( "218a25.7h",  0x000001, 0x200000, CRC(11600c2d) SHA1(721c9361761dd20810ff18c63543b222c98a47a4) )
	T1_PSAC6_ROM_LOAD( "218a26.5h",  0x000002, 0x200000, CRC(b37e4b7a) SHA1(3d21e540a366f6ef8ba761855fceecd8591179d7) )

	/* sound data */
	ROM_REGION( 0x400000, REGION_SOUND1, 0 )
	ROM_LOAD( "218a17.14y", 0x000000, 2*1024*1024, CRC(0b525127) SHA1(218b306c12e1094a676815b7dddaf13bf19be2d5) )
	ROM_LOAD( "218a18.12y", 0x200000, 1*1024*1024, CRC(98ec4cfb) SHA1(638753f9d9269719a37133b9c39c242507fdd8ac) )
ROM_END

/**********************************************************************************/
/* initializers */

MACHINE_INIT(konamigx)
{
	konamigx_wrport1_0 = konamigx_wrport1_1 = 0;
	konamigx_wrport2 = 0;

/*
	bit0  : EEPROM data(don't care)
	bit1  : DMA busy   (cleared)
	bit2-7: IRQ ready  (all set)
*/
	gx_rdport1_3 = 0xfc;
	gx_syncen    = 0;
	suspension_active = 0;

	memset(sndto000, 0, 16);
	memset(sndto020, 0, 16);

	tms57002_init();

	/* sound CPU initially disabled?*/
	cpu_set_halt_line(1, ASSERT_LINE);
}

static DRIVER_INIT(konamigx)
{
#define BPP5  1
#define BPP6  2
#define BPP66 3

	int readback = 0;

	konamigx_cfgport = -1;
	last_prot_op = -1;
	last_prot_clk = 0;

	esc_cb = 0;
	snd020_hack = 0;
	resume_trigger = 0;

	state_save_register_UINT8("KonamiGX", 0, "IRQ enable", &konamigx_wrport1_1, 1);
	state_save_register_UINT8("KonamiGX", 0, "Sound comms 1", sndto020, 16);
	state_save_register_UINT8("KonamiGX", 0, "Sound comms 2", sndto000, 16);

	dmadelay_timer = timer_alloc(dmaend_callback);

	/* running down the list is not a good idea but easier than maintaining individual drivers*/
	if (!strcmp(Machine->gamedrv->name, "racinfrc"))
	{
		konamigx_cfgport = 11;
	}

	else if (!strcmp(Machine->gamedrv->name, "opengolf"))
	{
		konamigx_cfgport = 11;
	}

	else if (!strcmp(Machine->gamedrv->name, "le2") || !strcmp(Machine->gamedrv->name, "le2u"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x2010f0, 0xc00000, 0xfe, 0x13f, -1, 0xff)
		#endif

		install_mem_read32_handler(0, 0xd44000, 0xd44003,  le2_gun_H_r );
		install_mem_read32_handler(0, 0xd44004, 0xd44007,  le2_gun_V_r );

		snd020_hack = 1;
		konamigx_cfgport = 13;
	}

	else if (!strcmp(Machine->gamedrv->name, "gokuparo") || !strcmp(Machine->gamedrv->name, "fantjour"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x2a0a66, 0xc00000, 0xd400, 0xd400, 0, 0xffff0000)
		#endif

		readback = BPP5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "puzldama"))
	{
		#if GX_SKIPIDLE
		#endif
			ADD_SKIPPER32(0x20d494, 0xc00000, 0x540, 0x6ff, -1, 0xffff)

		readback = BPP5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "tbyahhoo"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x297b9a, 0xc00000, 0xf800, 0xf800, 0, 0xffff0000)
		#endif

		esc_cb = tbyahhoo_esc;
		readback = BPP5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "tkmmpzdm"))
	{
		int i;
		data32_t *rom = (data32_t*)memory_region(REGION_CPU1);

		/* The display is initialized after POST but the copyright screen disabled*/
		/* planes B,C,D and didn't bother restoring them. I've spent a good*/
		/* amount of time chasing this bug but the cause remains inconclusive.*/
		/* My guess is the CCU somehow masked or delayed vblank interrupts*/
		/* during the copyright message.*/
		rom[0x810f1] &= ~1;      /* fix checksum*/
		rom[0x872ea] |= 0xe0000; /* enable plane B,C,D*/

		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x2060d4, 0xc00000, 0x13e48, 0x14027, 0, -1)
		#endif

		esc_cb = tkmmpzdm_esc;
		readback = BPP6;
		konamigx_cfgport = 7;

		/* boost voice(chip 1 channel 3-7)*/
		for (i=3; i<=7; i++) K054539_set_gain(1, i, 2.0);
	}

	else if (!strcmp(Machine->gamedrv->name, "dragoonj"))
	{
		int i;
		esc_cb = dragoonj_esc;
		konamigx_cfgport = 7;

		/* soften percussions(chip 1 channel 0-3), boost voice(chip 1 channel 4-7)*/
		for (i=0; i<=3; i++)
		{
			K054539_set_gain(1, i, 0.8);
			K054539_set_gain(1, i+4, 2.0);
		}
	}

	else if (!strcmp(Machine->gamedrv->name, "sexyparo") || !strcmp(Machine->gamedrv->name, "sexyparoa"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x289baa, 0xc00000, 0x10204, 0x10204, 0, 0xffff0000)
		#endif

		esc_cb = sexyparo_esc;
		readback = BPP5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "daiskiss"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x28707e, 0xc00000, 0x8400, 0x8400, 0, 0xffff0000)
		#endif

		esc_cb = daiskiss_esc;
		readback = BPP5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "tokkae"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x206b94, 0xc00000, 0x142ac, 0x142ac, 0, 0xff000000)
		#endif

		readback = BPP5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "salmndr2"))
	{
		#if GX_SKIPIDLE
			ADD_SKIPPER32(0x220070, 0xc00000, 0xbe8, 0xbe8, 0, 0x0000ffff)
		#endif

		esc_cb = sal2_esc;
		readback = BPP66;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "winspike"))
	{
		snd020_hack = 2;
		konamigx_cfgport = 8;
		/* Winning Spike uses the type 4 Xilinx protection*/
		install_mem_write32_handler(0, 0xcc0000, 0xcc0007, type4_prot_w );
	}

	else if (!strcmp(Machine->gamedrv->name, "soccerss"))
	{
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "vsnetscr"))
	{
		snd020_hack = 5;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "rungun2"))
	{
		snd020_hack = 3;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "slamdnk2"))
	{
		snd020_hack = 6;
		konamigx_cfgport = 7;
	}

	else if (!strcmp(Machine->gamedrv->name, "rushhero"))
	{
		snd020_hack = 4;
		konamigx_cfgport = 7;
	}

	switch (readback)
	{
		case BPP5:
			install_mem_read32_handler(0, 0xd4a000, 0xd4a00f, gx5bppspr_r);
		break;

		case BPP66:
			install_mem_read32_handler(0, 0xd00000, 0xd01fff, K056832_6bpp_rom_long_r);

		case BPP6:
			install_mem_read32_handler(0, 0xd4a000, 0xd4a00f, gx6bppspr_r);
		break;
	}

#undef BPP5
#undef BPP6
#undef BPP66
}

/**********************************************************************************/
/*     year  ROM       parent    machine   inp    	 init */

/* dummy parent for the BIOS */
GAMEX(1994, konamigx, 0, konamigx, konamigx, konamigx, ROT0, "Konami", "System GX", NOT_A_DRIVER )


/* Type 1: standard with an add-on 53936 on the ROM board, analog inputs, and LAN (link) capability */
/* needs the 53936 to be playable */
GAMEX( 1994, racinfrc, konamigx, racinfrc, racinfrc, konamigx, ROT0, "Konami", "Racin' Force (ver UAB)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING  )
GAMEX( 1994, opengolf, konamigx, opengolf,  racinfrc, konamigx, ROT0, "Konami", "Konami's Open Golf Championship (ver EAD)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING  )
GAMEX( 1994, ggreats2, opengolf, opengolf,  racinfrc, konamigx, ROT0, "Konami", "Golfing Greats 2 (ver JAC)", GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING )


/* Type 2: totally stock, sometimes with funny protection chips on the ROM board */
/* these games work and are playable with minor graphics glitches */
GAMEX( 1994, le2,      konamigx, le2,      le2,      konamigx, ROT0, "Konami", "Lethal Enforcers II - Gun Fighters (ver EAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, le2u,     le2,      le2,      le2_flip, konamigx, ORIENTATION_FLIP_Y, "Konami", "Lethal Enforcers II - Gun Fighters (ver UAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, gokuparo, konamigx, konamigx, gokuparo, konamigx, ROT0, "Konami", "Gokujyou Parodius (ver JAD)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1994, puzldama, konamigx, konamigx, puzldama, konamigx, ROT0, "Konami", "Taisen Puzzle-dama (ver JAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tbyahhoo, konamigx, konamigx, gokuparo, konamigx, ROT0, "Konami", "Twin Bee Yahhoo! (ver JAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, tkmmpzdm, konamigx, konamigx_6bpp, puzldama, konamigx, ROT0, "Konami", "Tokimeki Memorial Taisen Puzzle-dama (ver JAB)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, dragoonj, konamigx, dragoonj, dragoonj, konamigx, ROT0, "Konami", "Dragoon Might (ver JAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, sexyparo, konamigx, konamigx, gokuparo, konamigx, ROT0, "Konami", "Sexy Parodius (ver JAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, sexyparoa,sexyparo, konamigx, gokuparo, konamigx, ROT0, "Konami", "Sexy Parodius (ver AAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, daiskiss, konamigx, konamigx, gokuparo, konamigx, ROT0, "Konami", "Daisu-Kiss (ver JAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, tokkae,   konamigx, konamigx_6bpp, puzldama, konamigx, ROT0, "Konami", "Taisen Tokkae-dama (ver JAA)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, salmndr2, konamigx, konamigx_6bpp_2, gokuparo, konamigx, ROT0, "Konami", "Salamander 2 (ver JAA)", GAME_IMPERFECT_GRAPHICS|GAME_UNEMULATED_PROTECTION )

/* these games are unplayable due to protection (winspike has the same FPGA protection as the type 4 games) */
GAMEX( 1994, fantjour, gokuparo, konamigx, gokuparo, konamigx, ROT0, "Konami", "Fantastic Journey", GAME_NOT_WORKING|GAME_UNEMULATED_PROTECTION )
GAMEX( 1997, winspike, konamigx, winspike, konamigx, konamigx, ROT0, "Konami", "Winning Spike (ver JAA)", GAME_UNEMULATED_PROTECTION | GAME_IMPERFECT_GRAPHICS | GAME_NOT_WORKING  )


/* Type 3: dual monitor output and 53936 on the ROM board, external palette RAM */
GAMEX( 1994, soccerss, konamigx, gxtype3,  type3, konamigx, ROT0, "Konami", "Soccer Superstars (ver JAA)", GAME_NOT_WORKING )


/* Type 4: dual monitor output and 53936 on the ROM board, external palette RAM, DMA protection */
GAMEX( 1996, vsnetscr, konamigx, gxtype4, type3, konamigx, ROT0, "Konami", "Versus Net Soccer (ver UAB)", GAME_NOT_WORKING|GAME_UNEMULATED_PROTECTION )
GAMEX( 1996, rungun2,  konamigx, gxtype4, type3, konamigx, ROT0, "Konami", "Run and Gun 2 (ver UAA)", GAME_NOT_WORKING|GAME_UNEMULATED_PROTECTION )
GAMEX( 1996, slamdnk2, rungun2,  gxtype4, type3, konamigx, ROT0, "Konami", "Slam Dunk 2 (ver JAA)", GAME_NOT_WORKING|GAME_UNEMULATED_PROTECTION )
GAMEX( 1996, rushhero, konamigx, gxtype4, type3, konamigx, ROT0, "Konami", "Rushing Heroes (ver UAB)", GAME_UNEMULATED_PROTECTION | GAME_NOT_WORKING  )
