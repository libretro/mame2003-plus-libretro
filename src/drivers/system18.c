/*	System18 Hardware
**
**	MC68000 + Z80
**	2xYM3438 + Custom PCM
**
**	Alien Storm
**	Bloxeed
**	Clutch Hitter
**	D.D. Crew
**	Laser Ghost
**	Michael Jackson's Moonwalker
**	Shadow Dancer
**	Search Wally
*/
/*
need to new inputs too ddcrew and aquario as per changes in io using the new code
 
Changes:
04/28/04  Charles MacDonald
- Added MSM5205 sample playback to shdancbl.

03/17/04
- Added Where's Wally? (wwally) It's encrypted and unplayable.
- Moved Ace Attacker to system16.c (it's a not a System 18 game)
- Fixed System 18 sample ROM banking. This doesn't help the current working games, but will support others when/if they are decrypted.
- Fixed RF5C68A clock (7.15 MHz -> 8.00 MHz).
- Fixed Z80 clock (8.192 MHz -> 8.00 MHz).
- Cleaned up shdancbl sound hardware a little.

03/12/04
- Added preliminary VDP emulation to fix shdancer tile banking and VDP memory tests in other games.
- Added I/O chip emulation, fixes blanked screen at end of shdancer memory test, coin meter/lockout implemented.
- Cleaned up shdancer,shdancrj,shdancbl drivers.
- Added Shadow Dancer (Rev.B) driver (shdancrb).
- Added sound emulation to shdancbl.

To do:
- moonwlkb VDP test says IC68 (315-5313) is bad, but the test code seems to print the 'bad' message always, and it
  doesn't look like that VDP layer is supposed to cover the text (indicating success).
- astormbl writes to mirrored sprite RAM at $141000.
- moonwlkb writes to mirrored palette RAM at $841000.

shdancbl:
- Sampled sound needs to be implemented.
- ROM test fails. Maybe the original game code was not patched to fix this, rather than the ROMs really being bad.
- Supporting the screen blanking control at $E4001D causes some problems after coin-up.
- Player 1 inputs map to player 2.
- Misplaced tilemap horizontal scrolling in several locations:
  Level map screen, bonus stage, train level, sewer level, etc.
- Bad sprites in some specific cases:
  - Green shield-throwing man, animation when dog attacks enemy, fade-in when ninjas appear, etc.
- Missing "Shadow Dancer" logo on title screen if you insert a coin instead of watching the intro sequence.

I very carefully checked the sprite ROMs to make sure they are loaded correctly, and it seems that for the bad
sprites listed above, one of the even/odd pair is correct while the other is not. The correct data isn't present
in any of the ROMs I think. Maybe they need to be redumped, or it's an actual problem of the bootleg?

Other notes:
- The sys18_sound_info structure holds the offset to each ROM in the REGION_CPU2 space as well as a mask to be applied
   to the 8K bank offset into each ROM. For an unused ROM, the mask should be set to zero.

*/

#include <math.h>
#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "system16.h"
#include "ost_samples.h"
#include "vidhrdw/segaic16.h"
#include "cpu/i8051/i8051.h"
#include "machine/fd1094.h"
#include "state.h"

#include "vidhrdw/segavdp.h"

static read16_handler custom_io_r;
static write16_handler custom_io_w;

extern WRITE16_HANDLER( sys18_extrombank_w );

static UINT8 misc_io_data[0x10];
static int io_chip_5874_banking;	/* port H banks the tiles (171-5874 ROM board) */

static READ16_HANDLER( io_chip_r )
{
	offset &= 0x1f/2;

	switch (offset)
	{
		/* I/O ports */
		case 0x00/2:
		case 0x02/2:
		case 0x04/2:
		case 0x06/2:
		case 0x08/2:
		case 0x0a/2:
		case 0x0c/2:
		case 0x0e/2:
			/* if the port is configured as an output, return the last thing written */
			if (misc_io_data[0x1e/2] & (1 << offset))
				return misc_io_data[offset];
			/* otherwise, return an input port */
			return readinputport(offset);

		/* 'SEGA' protection */
		case 0x10/2:
			return 'S';
		case 0x12/2:
			return 'E';
		case 0x14/2:
			return 'G';
		case 0x16/2:
			return 'A';

		/* CNT register & mirror */
		case 0x18/2:
		case 0x1c/2:
			return misc_io_data[0x1c/2];

		/* port direction register & mirror */
		case 0x1a/2:
		case 0x1e/2:
			return misc_io_data[0x1e/2];
	}
	return 0xffff;
}


static WRITE16_HANDLER( io_chip_w )
{
	UINT8 old;

	/* generic implementation */
	offset &= 0x1f/2;
	old = misc_io_data[offset];
	misc_io_data[offset] = data;

	switch (offset)
	{
		/* I/O ports */
		case 0x00/2:
		case 0x02/2:
		case 0x04/2:
		case 0x08/2:
		case 0x0a/2:
		case 0x0c/2:
			break;

		/* miscellaneous output */
		case 0x06/2:
			system18_set_grayscale(~data & 0x40); 
			segaic16_tilemap_set_flip(0, data & 0x20);
			segaic16_sprites_set_flip(0, data & 0x20);
/* These are correct according to cgfm's docs, but mwalker and ddcrew both
   enable the lockout and never turn it off
			coin_lockout_w(1, data & 0x08);
			coin_lockout_w(0, data & 0x04); */
			//coin_counter_w(1, data & 0x02);
			//coin_counter_w(0, data & 0x01);
			break;

		/* tile banking */
		case 0x0e/2:
			if (io_chip_5874_banking)	/* 171-5874 ROM board */
			{
				int i;
				for (i = 0; i < 4; i++)
				{
					segaic16_tilemap_set_bank(0, 0 + i, (data & 0xf) * 4 + i);
					segaic16_tilemap_set_bank(0, 4 + i, ((data >> 4) & 0xf) * 4 + i);
				}
			}
			break;

		/* CNT register */
		case 0x1c/2:
			segaic16_set_display_enable(data & 2);
			if ((old ^ data) & 4)
				system18_set_vdp_enable(data & 4);
			break;
	}
}

static READ16_HANDLER( misc_io_r )
{
	offset &= 0x1fff;
	switch (offset & (0x3000/2))
	{
		/* I/O chip */
		case 0x0000/2:
		case 0x1000/2:
			return io_chip_r(offset, mem_mask);

		/* video control latch */
		case 0x2000/2:
			return readinputport(4 + (offset & 1));
	}
	if (custom_io_r)
		return custom_io_r(offset, mem_mask);
	logerror("%06X:misc_io_r - unknown read access to address %04X\n", activecpu_get_pc(), offset * 2);
	return 0xff;
}

static WRITE16_HANDLER( misc_io_w )
{
	offset &= 0x1fff;
	switch (offset & (0x3000/2))
	{
		/* I/O chip */
		case 0x0000/2:
		case 0x1000/2:
			if (ACCESSING_LSB)
			{
				io_chip_w(offset, data, mem_mask);
				return;
			}
			break;

		/* video control latch */
		case 0x2000/2:
			if (ACCESSING_LSB)
			{
				system18_set_vdp_mixing(data & 0xff);
				return;
			}
			break;
	}
	if (custom_io_w)
		custom_io_w(offset, data, mem_mask);
}

static READ16_HANDLER( ddcrew_custom_io_r )
{
	switch (offset)
	{
		case 0x3020/2:
			return readinputport(8);

		case 0x3022/2:
			return readinputport(9);

		case 0x3024/2:
			return readinputport(10);
	}
	return 0xff;
}

static void hamaway_interrupt(int state)
{

//cpu_set_nmi_line(1, PULSE_LINE);

	cpu_set_irq_line(1,0,state ? ASSERT_LINE : CLEAR_LINE);
}

struct YM2612interface hamaway =
{
	2,	/* 2 chips */
	8000000,
	{ YM3012_VOL(40,MIXER_PAN_CENTER,40,MIXER_PAN_CENTER),
			YM3012_VOL(40,MIXER_PAN_CENTER,40,MIXER_PAN_CENTER) },	/* Volume */
	{ 0 },	{ 0 },	{ 0 },	{ 0 }, {hamaway_interrupt}
};

/***************************************************************************/

static WRITE16_HANDLER( sys18_refreshenable_w )
{
	if(ACCESSING_LSB)
	{
		sys16_refreshenable = data & 0x02;
	}
}

static WRITE16_HANDLER( sys18_tilebank_w )
{
	if(ACCESSING_LSB)
	{
		sys16_tile_bank0 = (data >> 0) & 0x0F;
		sys16_tile_bank1 = (data >> 4) & 0x0F;
	}
}

/***************************************************************************/

static void set_fg_page( int data ){
	sys16_fg_page[0] = data>>12;
	sys16_fg_page[1] = (data>>8)&0xf;
	sys16_fg_page[2] = (data>>4)&0xf;
	sys16_fg_page[3] = data&0xf;
}

static void set_bg_page( int data ){
	sys16_bg_page[0] = data>>12;
	sys16_bg_page[1] = (data>>8)&0xf;
	sys16_bg_page[2] = (data>>4)&0xf;
	sys16_bg_page[3] = data&0xf;
}

static void set_fg2_page( int data ){
	sys16_fg2_page[0] = data>>12;
	sys16_fg2_page[1] = (data>>8)&0xf;
	sys16_fg2_page[2] = (data>>4)&0xf;
	sys16_fg2_page[3] = data&0xf;
}

static void set_bg2_page( int data ){
	sys16_bg2_page[0] = data>>12;
	sys16_bg2_page[1] = (data>>8)&0xf;
	sys16_bg2_page[2] = (data>>4)&0xf;
	sys16_bg2_page[3] = data&0xf;
}

static WRITE16_HANDLER( rom_837_7525_bank_w )
{
	if (!ACCESSING_LSB)
		return;
	offset &= 0xf;
	data &= 0xff;

	/* tile banking */
	if (offset < 8)
	{
	//	int maxbanks = m_gfxdecode->gfx(0)->elements() / 1024;
		data &= 0x9f;

		if (data & 0x80) data += 0x20;
		data &= 0x3f;

	    segaic16_tilemap_set_bank(0, offset, data);
	}

	// sprite banking
	else
	{
		//printf("%02x %02x\n", offset, data);
		// not needed?
	}
}


static WRITE16_HANDLER( rom_5987_bank_w )
{
	if (!ACCESSING_LSB)
		return;
	offset &= 0xf;
	data &= 0xff;

	/* tile banking */
	if (offset < 8)
	{
		int maxbanks = Machine->gfx[0]->total_elements / 1024;
		if (data >= maxbanks)
			data %= maxbanks;
		segaic16_tilemap_set_bank(0, offset, data);
	}

	/* sprite banking */
	else
	{
		int maxbanks = memory_region_length(REGION_GFX2) / 0x40000;
		if (data >= maxbanks)
			data = 255;
		segaic16_sprites_set_bank(0, (offset - 8) * 2 + 0, data * 2 + 0);
		segaic16_sprites_set_bank(0, (offset - 8) * 2 + 1, data * 2 + 1);
	}
}
/***************************************************************************/
/*
	Sound hardware for Shadow Dancer (Datsu bootleg)

	Z80 memory map
	0000-7FFF : ROM (fixed)
	8000-BFFF : ROM (banked)
	C000-C007 : ?
	C400      : Sound command (r/o)
	C800      : MSM5205 sample data output (w/o)
	CC00-CC03 : YM3438 #1
	D000-D003 : YM3438 #2
	D400      : ROM bank control (w/o)
	DF00-DFFF : ?
	E000-FFFF : Work RAM

	The unused memory locations and I/O port access seem to be remnants of the original code that were not patched out:

	- Program accesses RF5C68A channel registes at $C000-$C007
	- Program clears RF5C68A wave memory at $DF00-$DFFF
	- Program writes to port $A0 to access sound ROM banking control latch
	- Program reads port $C0 to access sound command

	Interrupts

	IRQ = Triggered when 68000 writes sound command. Z80 reads from $C400.
	NMI = Triggered when second nibble of sample data has been output to the MSM5205.
	      Program copies sample data from ROM bank to the MSM5205 sample data buffer at $C800.

	ROM banking seems correct.
	It doesn't look like there's a way to reset the MSM5205, unless that's related to bit 7 of the
	ROM bank control register.
	MSM5205 clock speed hasn't been confirmed.
*/
/***************************************************************************/

static int sample_buffer = 0;
static int sample_select = 0;

static WRITE_HANDLER( shdancbl_msm5205_data_w )
{
	sample_buffer = data;
}

static void shdancbl_msm5205_callback(int data)
{
	MSM5205_data_w(0, sample_buffer & 0x0F);
	sample_buffer >>= 4;
	sample_select ^= 1;
	if(sample_select == 0)
		cpu_set_nmi_line(1, PULSE_LINE);
}

static struct MSM5205interface shdancbl_msm5205_interface =
{
	1,
	200000, /* 200KHz */
	{ shdancbl_msm5205_callback },
	{ MSM5205_S48_4B},
	{ 80 }
};

UINT8* shdancbl_soundbank_ptr = NULL;		/* Pointer to currently selected portion of ROM */

static WRITE16_HANDLER( sound_command_irq_w ){
	if( ACCESSING_LSB ){
		soundlatch_w( 0,data&0xff );
		cpu_set_irq_line( 1, 0, HOLD_LINE );
	}
}

static READ_HANDLER( shdancbl_soundbank_r )
{
	if(shdancbl_soundbank_ptr) return shdancbl_soundbank_ptr[offset & 0x3FFF];
	return 0xFF;
}

static WRITE_HANDLER( shdancbl_bankctrl_w )
{
	UINT8 *mem = memory_region(REGION_CPU2);

	switch(data)
	{
		case 0:
			shdancbl_soundbank_ptr = &mem[0x18000]; /* IC45 8000-BFFF */
			break;
		case 1:
			shdancbl_soundbank_ptr = &mem[0x1C000]; /* IC45 C000-FFFF */
			break;
		case 2:
			shdancbl_soundbank_ptr = &mem[0x20000]; /* IC46 0000-3FFF */
			break;
		case 3:
			shdancbl_soundbank_ptr = &mem[0x24000]; /* IC46 4000-7FFF */
			break;
		default:
			shdancbl_soundbank_ptr = NULL;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "Invalid bank setting %02X (%04X)\n", data, activecpu_get_pc());
			break;
	}
}

static MEMORY_READ_START( shdancbl_sound_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, shdancbl_soundbank_r },
	{ 0xc400, 0xc400, soundlatch_r },
	{ 0xcc00, 0xcc00, YM2612_status_port_0_A_r },
	{ 0xcc01, 0xcc01, YM2612_status_port_0_B_r },
	{ 0xcc02, 0xcc02, YM2612_status_port_0_B_r },
	{ 0xcc03, 0xcc03, YM2612_status_port_0_B_r },
	{ 0xd000, 0xd000, YM2612_status_port_1_A_r },
	{ 0xd001, 0xd001, YM2612_status_port_1_B_r },
	{ 0xd002, 0xd002, YM2612_status_port_1_B_r },
	{ 0xd003, 0xd003, YM2612_status_port_1_B_r },
	{ 0xdf00, 0xdfff, MRA_NOP },
	{ 0xe000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( shdancbl_sound_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xbfff, MWA_NOP }, /* ROM bank */
	{ 0xc000, 0xc00f, MWA_NOP },
	{ 0xc800, 0xc800, shdancbl_msm5205_data_w },
	{ 0xcc00, 0xcc00, YM2612_control_port_0_A_w },
	{ 0xcc01, 0xcc01, YM2612_data_port_0_A_w },
	{ 0xcc02, 0xcc02, YM2612_control_port_0_B_w },
	{ 0xcc03, 0xcc03, YM2612_data_port_0_B_w },
	{ 0xd000, 0xd000, YM2612_control_port_1_A_w },
	{ 0xd001, 0xd001, YM2612_data_port_1_A_w },
	{ 0xd002, 0xd002, YM2612_control_port_1_B_w },
	{ 0xd003, 0xd003, YM2612_data_port_1_B_w },
	{ 0xd400, 0xd400, shdancbl_bankctrl_w },
	{ 0xdf00, 0xdfff, MWA_NOP },
	{ 0xe000, 0xffff, MWA_RAM },
MEMORY_END

static PORT_READ_START( shdancbl_sound_readport )
    { 0xc0, 0xdf, MRA_NOP },
PORT_END

static PORT_WRITE_START( shdancbl_sound_writeport )
    { 0xa0, 0xbf, MWA_NOP },
PORT_END

/***************************************************************************/

static MEMORY_READ_START( sound_readmem_18 )
    { 0x0000, 0x9fff, MRA_ROM },
	{ 0xa000, 0xbfff, MRA_BANK1 },
	/**** D/A register ****/
	{ 0xd000, 0xdfff, RF5C68_r },
	{ 0xe000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem_18 )
    { 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xbfff, MWA_BANK1 },
	/**** D/A register ****/
	{ 0xc000, 0xc008, RF5C68_reg_w },
	{ 0xd000, 0xdfff, RF5C68_w },
	{ 0xe000, 0xffff, MWA_RAM },	/*?? */
MEMORY_END

static UINT8 sys18_soundbank;

static WRITE_HANDLER( sys18_soundbank_w )
{
	sys18_soundbank = data;
	cpu_setbank(1, memory_region(REGION_CPU2) + 0x2000 * data);
}

static void sys18_soundbank_postload(void)
{
	cpu_setbank(1, memory_region(REGION_CPU2) + 0x2000 * sys18_soundbank);
}

static PORT_READ_START( sound_readport_18 )
    { 0x80, 0x80, YM2612_status_port_0_A_r },
/*	{ 0x82, 0x82, YM2612_status_port_0_B_r }, */
	{ 0x90, 0x90, YM2612_status_port_1_A_r },
/*	{ 0x92, 0x92, YM2612_status_port_1_B_r }, */
	{ 0xc0, 0xc0, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_writeport_18 )
    { 0x80, 0x80, YM2612_control_port_0_A_w },
	{ 0x81, 0x81, YM2612_data_port_0_A_w },
	{ 0x82, 0x82, YM2612_control_port_0_B_w },
	{ 0x83, 0x83, YM2612_data_port_0_B_w },
	{ 0x90, 0x90, YM2612_control_port_1_A_w },
	{ 0x91, 0x91, YM2612_data_port_1_A_w },
	{ 0x92, 0x92, YM2612_control_port_1_B_w },
	{ 0x93, 0x93, YM2612_data_port_1_B_w },
	{ 0xa0, 0xa0, sys18_soundbank_w },
PORT_END

static void sound_command_nmi(int data)
{
	if( ost_support_enabled(OST_SUPPORT_MOONWALKER) ) {
		if(generate_ost_sound( data )) {
			soundlatch_w( 0,data&0xff );
			cpu_set_nmi_line(1, PULSE_LINE);
		}
	}
	else {
		soundlatch_w( 0,data&0xff );
		cpu_set_nmi_line(1, PULSE_LINE);
	}
}

static WRITE16_HANDLER( sound_command_nmi_w ){

	if( ACCESSING_LSB )
		sound_command_nmi(data & 0xff);
}

/***************************************************************************/

/*
	315-5313 Video Display Processor (VDP) emulation
*/

struct {
	UINT16 vram[0x8000], read_buffer;
	UINT16 cram[0x40];
	UINT16 vsram[0x40];
	UINT8 reg[0x20];
	int pending, code, addr, addr_latch;
} vdp;

static WRITE16_HANDLER( vdp_w )
{
	switch(offset)
	{
		case 0: /* Data port */
		case 1: /* Data port */
			vdp.pending = 0;
			switch(vdp.code & 0x0F)
			{
				case 1: /* VRAM */
					vdp.vram[(vdp.addr >> 1) & 0x7FFF] = data;
					break;
				case 3: /* CRAM */
					vdp.cram[(vdp.addr >> 1) & 0x3F] = data;
					break;
				case 5: /* VSRAM */
					vdp.vsram[(vdp.addr >> 1) & 0x3F] = data;
					break;
			}
			vdp.addr = (vdp.addr + vdp.reg[0x0F]) & 0xFFFF;
			break;

		case 2: /* Control port */
		case 3: /* Control port */
			if(vdp.pending == 0)
			{
				if((data & 0xC000) == 0x8000)
				{
					int d = (data >> 0) & 0xFF;
					int r = (data >> 8) & 0x1F;
					vdp.reg[r] = d;
				}
				else
				{
					vdp.pending = 1;
					vdp.code = (vdp.code & 0x3C) | ((data >> 14) & 3);
					vdp.addr = (vdp.addr_latch & 0xC000) | (data & 0x3FFF);
				}
			}
			else
			{
				vdp.pending = 0;
				vdp.code = (vdp.code & 0x03) | ((data >> 2) & 0x3C);
				vdp.addr = (vdp.addr & 0x3FFF) | ((data & 3) << 14);
				vdp.addr_latch = vdp.addr & 0xC000;

				/* Read-ahead for VRAM */
				if(vdp.code == 0)
				{
					vdp.read_buffer = vdp.vram[(vdp.addr >> 1) & 0x7FFF];
					vdp.addr = (vdp.addr + vdp.reg[0x0F]) & 0xFFFF;
				}

				/* Check for DMA request */
				if((vdp.reg[1] & 0x20) && (vdp.code & 0x20))
				{
					log_cb(RETRO_LOG_DEBUG, LOGPRE "vdp: DMA disabled in this system.\n");
				}
			}
			break;

		default: /* Unused */
			log_cb(RETRO_LOG_DEBUG, LOGPRE "vdp: write %04X to %08X\n", data, offset);
			break;
	}
}

static READ16_HANDLER( vdp_r )
{
	UINT16 temp = -1;

	vdp.pending = 0;

	switch(offset)
	{
		case 0: /* Data port */
		case 1: /* Data port */
			switch(vdp.code & 0x0F)
			{
				case 0: /* VRAM */
					temp = vdp.read_buffer;
					vdp.read_buffer = vdp.vram[(vdp.addr >> 1) & 0x7FFF];
					break;

				case 4: /* VSRAM */
					temp = vdp.vsram[(vdp.addr >> 1) & 0x3F];
					break;

				case 8: /* CRAM */
					temp = vdp.cram[(vdp.addr >> 1) & 0x3F];
					break;
			}
			vdp.addr = (vdp.addr + vdp.reg[0x0F]) & 0xFFFF;
			break;

		case 2: /* Control port */
		case 3: /* Control port */

			/* VRAM write FIFO is always empty */
			temp = 0x0200;

			/* Having screen turned off forces V-Blank flag to be set */
			if((vdp.reg[1] & 0x40) == 0)
				temp |= 0x80;

			break;
	}

	return temp;
}

/***************************************************************************/

/*
	315-5296 I/O chip emulation
*/

static int io_reg[0x10];

void sys18_io_reset(void)
{
	/* All output latches are reset */
	io_reg[0x00] = 0x00;
	io_reg[0x01] = 0x00;
	io_reg[0x02] = 0x00;
	io_reg[0x03] = 0x00;
	io_reg[0x04] = 0x00;
	io_reg[0x05] = 0x00;
	io_reg[0x06] = 0x00;
	io_reg[0x07] = 0x00;

	/* CNT2-0 pins reset */
	io_reg[0x0E] = 0x00;

	/* All ports are inputs */
	io_reg[0x0F] = 0x00;
}

static READ16_HANDLER( sys18_io_r )
{
	if(ACCESSING_LSB)
	{
		switch(offset & 0x3000/2)
		{
			case 0x0000/2: /* I/O chip internal locations */
			case 0x1000/2: /* I/O chip internal locations (mirror) */
				switch(offset & 0x1F)
				{
					case 0x00: /* Port A - 1P controls */
						if(io_reg[0x0F] & 0x01)
							return io_reg[0x00];
						else
							return readinputport(0);
						break;

					case 0x01: /* Port B - 2P controls */
						if(io_reg[0x0F] & 0x02)
							return io_reg[0x01];
						else
							return readinputport(1);
						break;

					case 0x02: /* Port C - Bidirectional I/O port */
						if(io_reg[0x0F] & 0x04)
							return io_reg[0x02];
						else
							return -1;
						break;

					case 0x03: /* Port D - Miscellaneous outputs */
						if(io_reg[0x0F] & 0x08)
							return io_reg[0x03];
						else
							return -1;
						break;

					case 0x04: /* Port E - Service / Coin inputs */
						if(io_reg[0x0F] & 0x10)
							return io_reg[0x04];
						else
							return readinputport(2);
						break;

					case 0x05: /* Port F - DIP switch #1 */
						if(io_reg[0x0F] & 0x20)
							return io_reg[0x05];
						else
							return readinputport(3);
						break;

					case 0x06: /* Port G - DIP switch #2 */
						if(io_reg[0x0F] & 0x40)
							return io_reg[0x06];
						else
							return readinputport(4);
						break;

					case 0x07: /* Port H - Tile banking control */
						if(io_reg[0x0F] & 0x80)
							return io_reg[0x07];
						else
							return -1;
						break;

					case 0x08: /* Protection #1 */
						return 'S';
					case 0x09: /* Protection #2 */
						return 'E';
					case 0x0A: /* Protection #3 */
						return 'G';
					case 0x0B: /* Protection #4 */
						return 'A';

					case 0x0C: /* CNT2-0 pin output control (mirror) */
					case 0x0E: /* CNT2-0 pin output control */
						return io_reg[0x0E];

					case 0x0D: /* Port direction control (mirror) */
					case 0x0F: /* Port direction control */
						return io_reg[0x0F];
				}
				return -1;

			case 0x2000/2: /* Unused */
				log_cb(RETRO_LOG_DEBUG, LOGPRE "read video control latch %06X (%06X)\n", offset, activecpu_get_pc());
				return -1;

			case 0x3000/2: /* Expansion connector */
				log_cb(RETRO_LOG_DEBUG, LOGPRE "read expansion area %06X (%06X)\n", offset, activecpu_get_pc());
				return -1;
		}
	}

	return -1;
}

static WRITE16_HANDLER( sys18_io_w )
{
	if(ACCESSING_LSB)
	{
		switch(offset & 0x3000/2)
		{
			case 0x0000/2: /* I/O chip internal locations */
			case 0x1000/2: /* I/O chip internal locations (mirror) */
				switch(offset & 0x1F)
				{
					case 0x00: /* Port A - 1P controls */
						io_reg[0x00] = data;
						break;

					case 0x01: /* Port B - 2P controls */
						io_reg[0x01] = data;
						break;

					case 0x02: /* Port C - Bidirectional I/O port */
						io_reg[0x02] = data;
						break;

					case 0x03: /* Port D - Miscellaneous outputs */
						io_reg[0x03] = data;
						coin_lockout_w(1, data & 8);
						coin_lockout_w(0, data & 4);
						coin_counter_w(1, data & 2);
						coin_counter_w(0, data & 1);
						break;

					case 0x04: /* Port E - Service / Coin inputs */
						io_reg[0x04] = data;
						break;

					case 0x05: /* Port F - DIP switch #1 */
						io_reg[0x05] = data;
						break;

					case 0x06: /* Port G - DIP switch #2 */
						io_reg[0x06] = data;
						break;

					case 0x07: /* Port H - Tile banking control */
						io_reg[0x07] = data;
						sys16_tile_bank0 = (data >> 0) & 0x0F;
						sys16_tile_bank1 = (data >> 4) & 0x0F;
						break;

					case 0x0E: /* CNT2-0 pin output control */
						io_reg[0x0E] = data;
						sys16_refreshenable = data & 0x02;
						break;

					case 0x0F: /* Port direction control */
						io_reg[0x0F] = data;
						break;
				}
				break;

			case 0x2000/2: /* Video control latch */
				log_cb(RETRO_LOG_DEBUG, LOGPRE "write video control latch %06X = %04X (%06X)\n", offset, data, activecpu_get_pc());
				break;

			case 0x3000/2: /* Expansion connector */
/*				log_cb(RETRO_LOG_DEBUG, LOGPRE "write expansion area %06X = %04X (%06X)\n", offset, data, activecpu_get_pc()); */
				break;
		}
	}
}

/***************************************************************************/
/*
	Shadow Dancer (Bootleg)

	This seems to be a modified version of shdancer. It has no warning screen, displays English text during the
	attract sequence, and has a 2P input test. The 'Sega' copyright text was changed to 'Datsu', and their
	logo is missing.

	Access to the configuration registers, I/O chip, and VDP are done even though it's likely none of this hardware
	exists in the bootleg. For example:

	- Most I/O port access has been redirected to new addresses.
	- Z80 sound command has been redirected to a new address.
	- The tilebank routine which saves the bank value in VDP VRAM has a form of protection has been modified to store
	  the tilebank value directly to $E4001F.
	- Implementing screen blanking control via $E4001D leaves the screen blanked at the wrong times (after coin-up).

	This is probably due to unmodified parts of the original code accessing these components, which would be ignored
	on the bootleg hardware. Both the I/O chip and VDP are supported in this driver, just as I don't know for certain
	how much of either are present on the real board.

	Bootleg specific addresses:

	C40001 = DIP switch #1
	C40003 = DIP switch #2
	C40007 = Z80 sound command
	C41001 = Service input
	C41003 = Player 1 input
	C41005 = Player 2 input
	C44000 = Has 'clr.w' done after setting tile bank in $E4000F.
	C460xx = Extra video hardware controls

	Here are the I/O chip addresses accessed:

	E40001 = Player 1
	E40007 = Miscellaneous outputs (coin control, etc.)
	E4000F = Tile bank
	E4001D = CNT2-0 pin output state
	E4001F = I/O chip port direction
*/
/***************************************************************************/

static MEMORY_READ16_START( shdancbl_readmem )
    { 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
	{ 0x440000, 0x4407ff, SYS16_MRA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0xc00000, 0xc0ffff, vdp_r },
	{ 0xe40000, 0xe4ffff, sys18_io_r },
	{ 0xc40000, 0xc40001, input_port_3_word_r }, /* dip1 */
	{ 0xc40002, 0xc40003, input_port_4_word_r }, /* dip2 */
	{ 0xc41002, 0xc41003, input_port_0_word_r }, /* player1 */
	{ 0xc41004, 0xc41005, input_port_1_word_r }, /* player2 */
	{ 0xc41000, 0xc41001, input_port_2_word_r }, /* service */
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( shdancbl_writemem )
    { 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MWA16_TILERAM, &sys16_tileram },
	{ 0x410000, 0x410fff, SYS16_MWA16_TEXTRAM, &sys16_textram },
	{ 0x440000, 0x440fff, SYS16_MWA16_SPRITERAM, &sys16_spriteram },
	{ 0x840000, 0x840fff, SYS16_MWA16_PALETTERAM, &paletteram16 },
	{ 0xc00000, 0xc0ffff, vdp_w },
	{ 0xe4001c, 0xe4001d, MWA16_NOP }, /* to prevent access to screen blanking control below */
	{ 0xe40000, 0xe4ffff, sys18_io_w },
	{ 0xc40006, 0xc40007, sound_command_irq_w },
	{ 0xc44000, 0xc44001, MWA16_NOP }, /* only used via clr.w after tilebank set */
	{ 0xc46000, 0xc46fff, MWA16_NOP },/* bootleg specific video hardware */
	{ 0xfe0020, 0xfe003f, MWA16_NOP }, /* config regs */
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

/***************************************************************************/

static void shdancbl_update_proc( void )
{
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];

	set_fg_page( sys16_textram[0x0e80/2] );
	set_bg_page( sys16_textram[0x0e82/2] );

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	set_fg2_page( sys16_textram[0x0e84/2] );
	set_bg2_page( sys16_textram[0x0e82/2] );

	sys18_bg2_active=0;
	sys18_fg2_active=0;

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2])
		sys18_fg2_active=1;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2])
		sys18_bg2_active=1;
}

static MACHINE_INIT( shdancbl )
{
	sys16_sprxoffset = -0xbc+0x77;
	sys16_update_proc = shdancbl_update_proc;
}

static READ16_HANDLER( shdancbl_skip_r ){
	if (activecpu_get_pc()==0x2f76) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0];
}

static DRIVER_INIT( shdancbl )
{
	int i;
	UINT8 *mem;

	/* Invert tile ROM data*/
	mem = memory_region(REGION_GFX1);
	for(i = 0; i < 0xc0000; i++)
		mem[i] ^= 0xFF;

	install_mem_read16_handler(0, 0xffc000, 0xffc001, shdancbl_skip_r );

	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];
	sys16_MaxShadowColors=0;

	/* Copy first 32K of IC45 to Z80 address space */
	mem = memory_region(REGION_CPU2);
	memcpy(mem, mem+0x10000, 0x8000);
}

/***************************************************************************/
/*
	Moonwalker (Bootleg)
*/
/***************************************************************************/

static READ16_HANDLER( moonwlkb_skip_r ){
	if (activecpu_get_pc()==0x308a) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0x202c/2];
}

static MEMORY_READ16_START( moonwalk_readmem )
    { 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MRA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0xc00000, 0xc0ffff, vdp_r },
	{ 0xc40000, 0xc40001, input_port_3_word_r }, /* dip1 */
	{ 0xc40002, 0xc40003, input_port_4_word_r }, /* dip2 */
	{ 0xc41002, 0xc41003, input_port_0_word_r }, /* player1 */
	{ 0xc41004, 0xc41005, input_port_1_word_r }, /* player2 */
	{ 0xc41006, 0xc41007, input_port_5_word_r }, /* player3 */
	{ 0xc41008, 0xc41009, MRA16_NOP }, /* figure this out, extra input for 3p? */
	{ 0xc41000, 0xc41001, input_port_2_word_r }, /* service */
	{ 0xe40000, 0xe4ffff, SYS16_MRA16_EXTRAM2 },
	{ 0xffe02c, 0xffe02d, moonwlkb_skip_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( moonwalk_writemem )
    { 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MWA16_TILERAM, &sys16_tileram },
	{ 0x410000, 0x410fff, SYS16_MWA16_TEXTRAM, &sys16_textram },
	{ 0x440000, 0x440fff, SYS16_MWA16_SPRITERAM, &sys16_spriteram },
	{ 0x840000, 0x840fff, SYS16_MWA16_PALETTERAM, &paletteram16 },
	{ 0xc00000, 0xc0ffff, vdp_w },
	{ 0xc40006, 0xc40007, sound_command_nmi_w },
	{ 0xc46600, 0xc46601, sys18_refreshenable_w },
	{ 0xc46800, 0xc46801, sys18_tilebank_w },
	{ 0xe40000, 0xe4ffff, SYS16_MWA16_EXTRAM2, &sys16_extraram2 },
	{ 0xfe0020, 0xfe003f, MWA16_NOP }, /* config regs */
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

/***************************************************************************/

static void moonwalk_update_proc( void ){
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];

	set_fg_page( sys16_textram[0x0e80/2] );
	set_bg_page( sys16_textram[0x0e82/2] );

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	set_fg2_page( sys16_textram[0x0e84/2] );
	set_bg2_page( sys16_textram[0x0e86/2] );

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2])
		sys18_fg2_active=1;
	else
		sys18_fg2_active=0;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2])
		sys18_bg2_active=1;
	else
		sys18_bg2_active=0;
}

static MACHINE_INIT( moonwalk ){
	sys16_bg_priority_value=0x1000;
	sys16_sprxoffset = -0x238;

	sys16_patch_code( 0x70116, 0x4e);
	sys16_patch_code( 0x70117, 0x71);

	sys16_patch_code( 0x314a, 0x46);
	sys16_patch_code( 0x314b, 0x42);

	sys16_patch_code( 0x311b, 0x3f);

	sys16_patch_code( 0x70103, 0x00);
	sys16_patch_code( 0x70109, 0x00);
	sys16_patch_code( 0x07727, 0x00);
	sys16_patch_code( 0x07729, 0x00);
	sys16_patch_code( 0x0780d, 0x00);
	sys16_patch_code( 0x0780f, 0x00);
	sys16_patch_code( 0x07861, 0x00);
	sys16_patch_code( 0x07863, 0x00);
	sys16_patch_code( 0x07d47, 0x00);
	sys16_patch_code( 0x07863, 0x00);
	sys16_patch_code( 0x08533, 0x00);
	sys16_patch_code( 0x08535, 0x00);
	sys16_patch_code( 0x085bd, 0x00);
	sys16_patch_code( 0x085bf, 0x00);
	sys16_patch_code( 0x09a4b, 0x00);
	sys16_patch_code( 0x09a4d, 0x00);
	sys16_patch_code( 0x09b2f, 0x00);
	sys16_patch_code( 0x09b31, 0x00);
	sys16_patch_code( 0x0a05b, 0x00);
	sys16_patch_code( 0x0a05d, 0x00);
	sys16_patch_code( 0x0a23f, 0x00);
	sys16_patch_code( 0x0a241, 0x00);
	sys16_patch_code( 0x10159, 0x00);
	sys16_patch_code( 0x1015b, 0x00);
	sys16_patch_code( 0x109fb, 0x00);
	sys16_patch_code( 0x109fd, 0x00);

	/* SEGA mark */
	sys16_patch_code( 0x70212, 0x4e);
	sys16_patch_code( 0x70213, 0x71);

	sys16_update_proc = moonwalk_update_proc;
}

static DRIVER_INIT( moonwalk ){

	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];

	
}

/***************************************************************************/
/*
	Moonwalker (original board)

	The 68000 is an FD1094 (mwalk, mwalku, mwalkj); mwalkd, mwalkud and
	mwalkjd are the same sets with the program decrypted.

	An i8751 (315-5437) drives the game through the 315-5195 memory mapper:
	its MOVX space 00-1f are the mapper registers. It interrupts the 68000,
	reads and writes the 68000 memory and talks to the sound CPU, so the
	68000 has no vblank interrupt of its own. As in MAME (segas18.cpp,
	system18_i8751).

	The mapper registers 10-1f place the 68000 memory regions; the game
	always sets the same layout, which is the fixed map below.
*/
/***************************************************************************/

#define MWALK_MCU_CPUNUM	2

static UINT8 mwalk_mapper_regs[0x20];
static UINT8 mwalk_from_sound;
static int   mwalk_encrypted;
static int   mwalk_has_mcu;		/* the 8751 is there (Moonwalker) */

static data8_t mwalk_mapper_read(offs_t offset)
{
	offset &= 0x1f;
	switch (offset)
	{
		case 0x00:
		case 0x01:
			/* data latches */
			return mwalk_mapper_regs[offset];

		case 0x02:
			/* 00 while the 68000 is held in reset, 0f while it runs */
			return ((mwalk_mapper_regs[0x02] & 3) == 3) ? 0x00 : 0x0f;

		case 0x03:
			/* data from the sound CPU; reading it clears the MCU interrupt */
			if (mwalk_has_mcu)
				cpu_set_irq_line(MWALK_MCU_CPUNUM, I8051_INT1_LINE, CLEAR_LINE);
			return mwalk_from_sound;
	}
	return 0xff;
}

static void mwalk_mapper_write(offs_t offset, data8_t data)
{
	UINT8 oldval;
	offs_t addr;
	int irqnum;

	offset &= 0x1f;
	oldval = mwalk_mapper_regs[offset];
	mwalk_mapper_regs[offset] = data;

	switch (offset)
	{
		case 0x02:
			/* 03 holds the 68000 in reset, 00 lets it run */
			if ((oldval ^ data) & 3)
			{
				if ((data & 3) == 3)
					cpu_set_reset_line(0, ASSERT_LINE);
				else
				{
					if (mwalk_encrypted)
						fd1094_cpu_reset();
					cpu_set_reset_line(0, CLEAR_LINE);
				}
			}
			break;

		case 0x03:
			/* to the sound CPU */
			sound_command_nmi(data);
			break;

		case 0x04:
			/* 68000 interrupt, negative logic: write $B for IRQ4 */
			if ((data & 7) != 7)
				for (irqnum = 1; irqnum < 8; irqnum++)
					cpu_set_irq_line(0, irqnum, (irqnum == (~data & 7)) ? HOLD_LINE : CLEAR_LINE);
			break;

		case 0x05:
			/* 01: write the word in 00/01 at 2 * (0a,0b,0c)
			   02: read the word at 2 * (07,08,09) into 00/01 */
			if (data == 0x01)
			{
				addr = (mwalk_mapper_regs[0x0a] << 17) | (mwalk_mapper_regs[0x0b] << 9) | (mwalk_mapper_regs[0x0c] << 1);
				cpuintrf_push_context(0);
				cpu_writemem24bew_word(addr & 0xffffff, (mwalk_mapper_regs[0x00] << 8) | mwalk_mapper_regs[0x01]);
				cpuintrf_pop_context();
			}
			else if (data == 0x02)
			{
				data16_t result;
				addr = (mwalk_mapper_regs[0x07] << 17) | (mwalk_mapper_regs[0x08] << 9) | (mwalk_mapper_regs[0x09] << 1);
				cpuintrf_push_context(0);
				result = cpu_readmem24bew_word(addr & 0xffffff);
				cpuintrf_pop_context();
				mwalk_mapper_regs[0x00] = result >> 8;
				mwalk_mapper_regs[0x01] = result;
			}
			break;
	}
}

/* 68000 side: the low bytes at fe0000-fe003f */
static READ16_HANDLER( mwalk_mapper_r )
{
	return 0xff00 | mwalk_mapper_read(offset);
}

static WRITE16_HANDLER( mwalk_mapper_w )
{
	if (ACCESSING_LSB)
		mwalk_mapper_write(offset, data & 0xff);
}

/* MCU side: MOVX 00-1f, the high address byte is not decoded */
static READ_HANDLER( mwalk_mcu_movx_r )
{
	offset &= 0xff;
	if (offset < 0x20)
		return mwalk_mapper_read(offset);
	return 0xff;
}

static WRITE_HANDLER( mwalk_mcu_movx_w )
{
	offset &= 0xff;
	if (offset < 0x20)
		mwalk_mapper_write(offset, data);
}

/* the sound CPU writes back to the mapper, which interrupts the MCU */
static void mwalk_from_sound_cb(int param)
{
	mwalk_from_sound = param;
	cpu_set_irq_line(MWALK_MCU_CPUNUM, I8051_INT1_LINE, ASSERT_LINE);
}

static WRITE_HANDLER( mwalk_sound_to_mapper_w )
{
	timer_set(TIME_NOW, data, mwalk_from_sound_cb);
}

static INTERRUPT_GEN( mwalk_mcu_vblank )
{
	cpu_set_irq_line(MWALK_MCU_CPUNUM, I8051_INT0_LINE, HOLD_LINE);
}

static MEMORY_READ16_START( mwalk_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, segaic16_tileram_r },
	{ 0x410000, 0x410fff, segaic16_textram_r },
	{ 0x440000, 0x4407ff, segaic16_spriteram_r },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0xc00000, 0xc0ffff, segavdp_r },
	{ 0xe40000, 0xe4ffff, misc_io_r },
	{ 0xfe0000, 0xfe003f, mwalk_mapper_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( mwalk_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, segaic16_tileram_0_w, &segaic16_tileram_0 },
	{ 0x410000, 0x410fff, segaic16_textram_0_w, &segaic16_textram_0 },
	{ 0x440000, 0x4407ff, SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0 },
	{ 0x840000, 0x840fff, segaic16_paletteram_w, &paletteram16 },
	{ 0xc00000, 0xc0ffff, segavdp_w },
	{ 0xe40000, 0xe4ffff, misc_io_w },
	{ 0xfe0000, 0xfe003f, mwalk_mapper_w },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

static MEMORY_READ_START( mwalk_mcu_readmem )
	{ 0x0000, 0x0fff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( mwalk_mcu_writemem )
	{ 0x0000, 0x0fff, MWA_ROM },
MEMORY_END

static MACHINE_INIT( mwalk )
{
	segaic16_tilemap_reset(0);
	io_chip_5874_banking = 1;

	memset(mwalk_mapper_regs, 0, sizeof(mwalk_mapper_regs));
	mwalk_from_sound = 0;
	mwalk_has_mcu = 1;
	i8051_set_movx_handlers(MWALK_MCU_CPUNUM, mwalk_mcu_movx_r, mwalk_mcu_movx_w);

	if (mwalk_encrypted)
		fd1094_machine_init();

	/* the MCU and the 68000 talk a lot at startup */
	cpu_boost_interleave(0, TIME_IN_MSEC(10));
}

static DRIVER_INIT( mwalkd )
{
	mwalk_encrypted = 0;

	state_save_register_UINT8("mwalk", 0, "mapper", mwalk_mapper_regs, 0x20);
	state_save_register_UINT8("mwalk", 0, "from_sound", &mwalk_from_sound, 1);
	state_save_register_UINT8("mwalk", 0, "io", misc_io_data, 0x10);
	state_save_register_UINT8("mwalk", 0, "soundbank", &sys18_soundbank, 1);
	state_save_register_func_postload(sys18_soundbank_postload);
}

static DRIVER_INIT( mwalk )
{
	init_mwalkd();
	mwalk_encrypted = 1;
	fd1094_driver_init(0, REGION_USER1);
}


/***************************************************************************/
/*
	The other System 18 games, as in MAME (segas18.cpp): the 315-5195 mapper
	places the 68000 memory regions as the game program tells it at boot,
	and every game always sets the same layout, which is one of the fixed
	maps below (registers 10-1f read from MAME):

	                171-5874 ROM board      171-5987 ROM board
	                astorm     bloxeed      cltchitr, wwallyj
	ROM 0           000000     000000       000000
	ROM 1           -          -            200000 (writes: tile/sprite banks)
	tile/text RAM   100000     400000       400000
	palette RAM     140000     840000       840000
	sprite RAM      200000     440000       440000
	I/O             a00000     e40000       a40000
	VDP             c00000     c00000       c00000
	work RAM        ff0000     ff0000       ff0000

	The RAMs are mirrored inside their region, as on the board. The 171-5874
	board banks the tiles from the I/O chip port H, the 171-5987 board through
	writes to the ROM 1 region. The FD1094 sets decrypt the opcodes of both
	ROM regions.
*/
/***************************************************************************/


/* the RAM mirrors */
static READ16_HANDLER( sys18_textram_mirror_r )  { return segaic16_textram_0[offset & 0x7ff]; }
static WRITE16_HANDLER( sys18_textram_mirror_w ) { segaic16_textram_0_w(offset & 0x7ff, data, mem_mask); }
static READ16_HANDLER( sys18_palette_mirror_r )  { return paletteram16[offset & 0x7ff]; }
static WRITE16_HANDLER( sys18_palette_mirror_w ) { segaic16_paletteram_w(offset & 0x7ff, data, mem_mask); }
static READ16_HANDLER( sys18_sprite_mirror_r )   { return segaic16_spriteram_0[offset & 0x3ff]; }
static WRITE16_HANDLER( sys18_sprite_mirror_w )  { COMBINE_DATA(&segaic16_spriteram_0[offset & 0x3ff]); }
static READ16_HANDLER( sys18_workram_mirror_r )  { return sys16_workingram[offset & 0x1fff]; }
static WRITE16_HANDLER( sys18_workram_mirror_w ) { COMBINE_DATA(&sys16_workingram[offset & 0x1fff]); }

/* 171-5874: Alien Storm */
static MEMORY_READ16_START( sys18_astorm_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x100000, 0x10ffff, segaic16_tileram_r },
	{ 0x110000, 0x110fff, segaic16_textram_r },
	{ 0x111000, 0x11ffff, sys18_textram_mirror_r },
	{ 0x140000, 0x140fff, SYS16_MRA16_PALETTERAM },
	{ 0x141000, 0x14ffff, sys18_palette_mirror_r },
	{ 0x200000, 0x2007ff, segaic16_spriteram_r },
	{ 0x200800, 0x20ffff, sys18_sprite_mirror_r },
	{ 0xa00000, 0xa0ffff, misc_io_r },
	{ 0xc00000, 0xc0ffff, segavdp_r },
	{ 0xfe0000, 0xfe003f, mwalk_mapper_r },
	{ 0xff0000, 0xffbfff, sys18_workram_mirror_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( sys18_astorm_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x100000, 0x10ffff, segaic16_tileram_0_w, &segaic16_tileram_0 },
	{ 0x110000, 0x110fff, segaic16_textram_0_w, &segaic16_textram_0 },
	{ 0x111000, 0x11ffff, sys18_textram_mirror_w },
	{ 0x140000, 0x140fff, segaic16_paletteram_w, &paletteram16 },
	{ 0x141000, 0x14ffff, sys18_palette_mirror_w },
	{ 0x200000, 0x2007ff, SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0 },
	{ 0x200800, 0x20ffff, sys18_sprite_mirror_w },
	{ 0xa00000, 0xa0ffff, misc_io_w },
	{ 0xc00000, 0xc0ffff, segavdp_w },
	{ 0xfe0000, 0xfe003f, mwalk_mapper_w },
	{ 0xff0000, 0xffbfff, sys18_workram_mirror_w },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

/* 171-5874: Bloxeed (the Moonwalker layout) */
static MEMORY_READ16_START( sys18_bloxeed_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, segaic16_tileram_r },
	{ 0x410000, 0x410fff, segaic16_textram_r },
	{ 0x411000, 0x41ffff, sys18_textram_mirror_r },
	{ 0x440000, 0x4407ff, segaic16_spriteram_r },
	{ 0x440800, 0x44ffff, sys18_sprite_mirror_r },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0x841000, 0x84ffff, sys18_palette_mirror_r },
	{ 0xc00000, 0xc0ffff, segavdp_r },
	{ 0xe40000, 0xe4ffff, misc_io_r },
	{ 0xfe0000, 0xfe003f, mwalk_mapper_r },
	{ 0xff0000, 0xffbfff, sys18_workram_mirror_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( sys18_bloxeed_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, segaic16_tileram_0_w, &segaic16_tileram_0 },
	{ 0x410000, 0x410fff, segaic16_textram_0_w, &segaic16_textram_0 },
	{ 0x411000, 0x41ffff, sys18_textram_mirror_w },
	{ 0x440000, 0x4407ff, SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0 },
	{ 0x440800, 0x44ffff, sys18_sprite_mirror_w },
	{ 0x840000, 0x840fff, segaic16_paletteram_w, &paletteram16 },
	{ 0x841000, 0x84ffff, sys18_palette_mirror_w },
	{ 0xc00000, 0xc0ffff, segavdp_w },
	{ 0xe40000, 0xe4ffff, misc_io_w },
	{ 0xfe0000, 0xfe003f, mwalk_mapper_w },
	{ 0xff0000, 0xffbfff, sys18_workram_mirror_w },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

/* 171-5987: ROM 0 at 000000, ROM 1 at 200000 (writes: tile and sprite
   banks); the 080000 and 280000 halves are the second half of 1MB ROMs
   (Desert Breaker) or mirrors of the first one. I/O at a40000 (Clutch
   Hitter, Wally wo Sagase!) or e40000 (D. D. Crew, Desert Breaker,
   Laser Ghost). */
#define SYS18_5987_READMEM(name, io) \
static MEMORY_READ16_START( name ) \
	{ 0x000000, 0x07ffff, MRA16_ROM }, \
	{ 0x080000, 0x0fffff, MRA16_BANK3 }, \
	{ 0x200000, 0x27ffff, MRA16_BANK2 }, \
	{ 0x280000, 0x2fffff, MRA16_BANK4 }, \
	{ 0x400000, 0x40ffff, segaic16_tileram_r }, \
	{ 0x410000, 0x410fff, segaic16_textram_r }, \
	{ 0x411000, 0x41ffff, sys18_textram_mirror_r }, \
	{ 0x440000, 0x4407ff, segaic16_spriteram_r }, \
	{ 0x440800, 0x44ffff, sys18_sprite_mirror_r }, \
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM }, \
	{ 0x841000, 0x84ffff, sys18_palette_mirror_r }, \
	{ io, io + 0xffff, misc_io_r }, \
	{ 0xc00000, 0xc0ffff, segavdp_r }, \
	{ 0xfe0000, 0xfe003f, mwalk_mapper_r }, \
	{ 0xff0000, 0xffbfff, sys18_workram_mirror_r }, \
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM }, \
MEMORY_END

#define SYS18_5987_WRITEMEM(name, io) \
static MEMORY_WRITE16_START( name ) \
	{ 0x000000, 0x07ffff, MWA16_ROM }, \
	{ 0x080000, 0x0fffff, MWA16_NOP }, \
	{ 0x200000, 0x3fffff, rom_5987_bank_w }, \
	{ 0x400000, 0x40ffff, segaic16_tileram_0_w, &segaic16_tileram_0 }, \
	{ 0x410000, 0x410fff, segaic16_textram_0_w, &segaic16_textram_0 }, \
	{ 0x411000, 0x41ffff, sys18_textram_mirror_w }, \
	{ 0x440000, 0x4407ff, SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0 }, \
	{ 0x440800, 0x44ffff, sys18_sprite_mirror_w }, \
	{ 0x840000, 0x840fff, segaic16_paletteram_w, &paletteram16 }, \
	{ 0x841000, 0x84ffff, sys18_palette_mirror_w }, \
	{ io, io + 0xffff, misc_io_w }, \
	{ 0xc00000, 0xc0ffff, segavdp_w }, \
	{ 0xfe0000, 0xfe003f, mwalk_mapper_w }, \
	{ 0xff0000, 0xffbfff, sys18_workram_mirror_w }, \
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram }, \
MEMORY_END

SYS18_5987_READMEM(sys18_5987_readmem, 0xa40000)
SYS18_5987_WRITEMEM(sys18_5987_writemem, 0xa40000)
SYS18_5987_READMEM(sys18_5987e_readmem, 0xe40000)
SYS18_5987_WRITEMEM(sys18_5987e_writemem, 0xe40000)


/* Wally wo Sagase!: three uPD4701 trackball counters at 3000-3017 */
static UINT16 wwally_trackball_start[6];

static READ16_HANDLER( wwally_custom_io_r )
{
	if (offset >= 0x3000/2 && offset < 0x3018/2)
	{
		int chip = (offset & 0x0018/2) >> 2;
		int axis = chip * 2 + ((offset >> 1) & 1);
		UINT16 count = (readinputport(8 + axis) - wwally_trackball_start[axis]) & 0xfff;
		return (offset & 1) ? (count >> 8) : (count & 0xff);
	}
	return 0xffff;
}

static WRITE16_HANDLER( wwally_custom_io_w )
{
	if (offset >= 0x3000/2 && offset < 0x3018/2)
	{
		/* reset the X and Y counters of the chip */
		int chip = (offset & 0x0018/2) >> 2;
		wwally_trackball_start[chip * 2 + 0] = readinputport(8 + chip * 2 + 0);
		wwally_trackball_start[chip * 2 + 1] = readinputport(8 + chip * 2 + 1);
	}
}


/* D. D. Crew 4 players: the 8255 at 3020-3027 reads players 3 and 4 */
static WRITE16_HANDLER( ddcrew_custom_io_w )
{
	/* the 8255 is only set up as inputs */
}


/* Laser Ghost: the guns through an MSM6253 A/D converter at 3010-3017,
   read one bit at a time; 3020 selects player 2 or 3 on the second pair.
   The offset correction of the guns of players 1 and 3 is MAME's. */
static UINT8 lghost_shift;
static UINT8 lghost_select;

static int lghost_round(double v)
{
	double r = floor(v + 0.5);
	return (int)r;
}

static int lghost_y1(void)
{
	int x = readinputport(8), y = 255 - readinputport(9);

	if (!(readinputport(14) & 1))
		return y;
	if (x >= 50 && x <= 99)
		return (y >= 130 && y <= 225) ? lghost_round(y * 0.94 + 0.80) : y;
	if (x >= 100 && x <= 199)
		return (y >= 100 && y <= 225) ? lghost_round(y * 0.89 + 6.00) : y;
	if (x >= 200 && x <= 249)
	{
		if (y >= 30 && y <= 55)   return lghost_round(y * 0.78 + 18.28);
		if (y >= 100 && y <= 205) return lghost_round(y * 0.70 + 28.00);
		if (y >= 206 && y <= 225) return lghost_round(y * 1.58 - 151.48);
	}
	return y;
}

static int lghost_x1(void)
{
	int x = readinputport(8);

	if (!(readinputport(14) & 1))
		return x;
	if (x >= 26 && x <= 85)   return lghost_round(x * 1.13 + 0.95);
	if (x >= 86 && x <= 140)  return lghost_round(x * 1.10 + 4.00);
	if (x >= 141 && x <= 190) return lghost_round(x * 1.02 + 11.20);
	if (x >= 191 && x <= 240) return lghost_round(x * 0.76 + 62.60);
	return x;
}

static int lghost_y2(void)
{
	if (lghost_select)
	{
		int x = readinputport(12), y = 255 - readinputport(13);

		if (!(readinputport(14) & 1))
			return y;
		if (x >= 128 && x <= 255)
		{
			if (y >= 30 && y <= 125)  return lghost_round(y * 1.01 + 11.82);
			if (y >= 126 && y <= 235) return lghost_round(y * 0.94 + 21.90);
		}
		else if (x >= 17 && x <= 127)
		{
			if (y >= 40 && y <= 145)  return lghost_round(y * 0.82 + 31.80);
			if (y >= 200 && y <= 225) return lghost_round(y * 0.83 + 29.95);
		}
		return y;
	}
	return 255 - readinputport(11);
}

static int lghost_x2(void)
{
	if (lghost_select)
	{
		int x = readinputport(12);

		if (!(readinputport(14) & 1))
			return x;
		if (x >= 17 && x <= 34)   return x - 17;
		if (x >= 35 && x <= 110)  return lghost_round(x * 0.94 - 14.08);
		if (x >= 111 && x <= 225) return lghost_round(x * 1.15 - 35.65);
		return x;
	}
	return readinputport(10);
}

static READ16_HANDLER( lghost_custom_io_r )
{
	if (offset >= 0x3010/2 && offset <= 0x3016/2)
	{
		UINT16 result = (lghost_shift & 0x80) ? 0xff : 0x7f;
		lghost_shift <<= 1;
		return result;
	}
	return 0xffff;
}

static WRITE16_HANDLER( lghost_custom_io_w )
{
	switch (offset)
	{
		case 0x3010/2:	lghost_shift = lghost_y1(); break;
		case 0x3012/2:	lghost_shift = lghost_x1(); break;
		case 0x3014/2:	lghost_shift = lghost_y2(); break;
		case 0x3016/2:	lghost_shift = lghost_x2(); break;
		case 0x3020/2:	lghost_select = data & 1; break;
	}
}


/* ROM banks of the 171-5987 board */
static UINT8 *sys18_rom_open;		/* 0xff where the board has no ROM */
static UINT8 *sys18_bank_rom0b, *sys18_bank_rom1a, *sys18_bank_rom1b;

static void sys18_machine_init(int banking_5874)
{
	segaic16_tilemap_reset(0);
	io_chip_5874_banking = banking_5874;
	memset(mwalk_mapper_regs, 0, sizeof(mwalk_mapper_regs));
	mwalk_from_sound = 0;
	mwalk_has_mcu = 0;
	if (mwalk_encrypted)
		fd1094_machine_init();
}

static MACHINE_INIT( sys18_5874 )
{
	sys18_machine_init(1);
}

static MACHINE_INIT( sys18_5987 )
{
	sys18_machine_init(0);
	cpu_setbank(3, sys18_bank_rom0b);
	cpu_setbank(2, sys18_bank_rom1a);
	cpu_setbank(4, sys18_bank_rom1b);
}

static void sys18_driver_init(int encrypted, int board_5987)
{
	UINT8 *rom = memory_region(REGION_CPU1);
	UINT32 romsize = memory_region_length(REGION_CPU1);

	mwalk_encrypted = encrypted;
	custom_io_r = NULL;
	custom_io_w = NULL;

	if (encrypted)
		fd1094_driver_init(0, REGION_USER1);

	if (board_5987)
	{
		/* ROM 0 and ROM 1 are 512KB each, or 1MB each (romsize 2MB) */
		UINT32 window = (romsize > 0x100000) ? 0x100000 : 0x80000;
		UINT32 rom1b = (window == 0x100000) ? window + 0x80000 : window;

		sys18_rom_open = auto_malloc(0x80000);
		memset(sys18_rom_open, 0xff, 0x80000);

		sys18_bank_rom0b = rom + ((window == 0x100000) ? 0x80000 : 0);
		sys18_bank_rom1a = (romsize > window) ? rom + window : sys18_rom_open;
		sys18_bank_rom1b = (romsize > window) ? rom + rom1b : sys18_rom_open;

		if (encrypted)
		{
			fd1094_set_window(0, 0x000000, 0x07ffff, 0x000000);
			fd1094_set_window(1, 0x080000, 0x0fffff, (window == 0x100000) ? 0x80000 : 0);
			if (romsize > window)
			{
				fd1094_set_window(2, 0x200000, 0x27ffff, window);
				fd1094_set_window(3, 0x280000, 0x2fffff, rom1b);
			}
		}
	}
	else if (encrypted)
		fd1094_set_window(0, 0x000000, 0x07ffff, 0x000000);

	state_save_register_UINT8("sys18", 0, "mapper", mwalk_mapper_regs, 0x20);
	state_save_register_UINT8("sys18", 0, "io", misc_io_data, 0x10);
	state_save_register_UINT8("sys18", 0, "soundbank", &sys18_soundbank, 1);
	state_save_register_func_postload(sys18_soundbank_postload);
}

static DRIVER_INIT( sys18_5874 )    { sys18_driver_init(0, 0); }
static DRIVER_INIT( sys18_5874_fd ) { sys18_driver_init(1, 0); }
static DRIVER_INIT( sys18_5987 )    { sys18_driver_init(0, 1); }
static DRIVER_INIT( sys18_5987_fd ) { sys18_driver_init(1, 1); }

static void wwally_init(void)
{
	custom_io_r = wwally_custom_io_r;
	custom_io_w = wwally_custom_io_w;
	memset(wwally_trackball_start, 0, sizeof(wwally_trackball_start));
	state_save_register_UINT16("wwally", 0, "trackball", wwally_trackball_start, 6);
}

static DRIVER_INIT( wwally )    { sys18_driver_init(0, 1); wwally_init(); }
static DRIVER_INIT( wwally_fd ) { sys18_driver_init(1, 1); wwally_init(); }

static void ddcrew4p_init(void)
{
	custom_io_r = ddcrew_custom_io_r;
	custom_io_w = ddcrew_custom_io_w;
}

static DRIVER_INIT( ddcrew4p )    { sys18_driver_init(0, 1); ddcrew4p_init(); }
static DRIVER_INIT( ddcrew4p_fd ) { sys18_driver_init(1, 1); ddcrew4p_init(); }

static void lghost_init(void)
{
	custom_io_r = lghost_custom_io_r;
	custom_io_w = lghost_custom_io_w;
	lghost_shift = lghost_select = 0;
	state_save_register_UINT8("lghost", 0, "shift", &lghost_shift, 1);
	state_save_register_UINT8("lghost", 0, "select", &lghost_select, 1);
}

static DRIVER_INIT( lghost )    { sys18_driver_init(0, 1); lghost_init(); }
static DRIVER_INIT( lghost_fd ) { sys18_driver_init(1, 1); lghost_init(); }

/***************************************************************************/

static READ16_HANDLER( astorm_skip_r ){
	if (activecpu_get_pc()==0x3d4c) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0x2c2c/2];
}

static MEMORY_READ16_START( astorm_readmem )
    { 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x100000, 0x10ffff, SYS16_MRA16_TILERAM },
	{ 0x110000, 0x110fff, SYS16_MRA16_TEXTRAM },
	{ 0x140000, 0x140fff, SYS16_MRA16_PALETTERAM },
	{ 0x200000, 0x200fff, SYS16_MRA16_SPRITERAM },
	{ 0xa00000, 0xa00001, input_port_3_word_r }, /* dip1 */
	{ 0xa00002, 0xa00003, input_port_4_word_r }, /* dip2 */
	{ 0xa01002, 0xa01003, input_port_0_word_r }, /* player1 */
	{ 0xa01004, 0xa01005, input_port_1_word_r }, /* player2 */
	{ 0xa01006, 0xa01007, input_port_5_word_r }, /* player3 */
	{ 0xa01000, 0xa01001, input_port_2_word_r }, /* service */
	{ 0xc00000, 0xc0ffff, vdp_r },
	{ 0xffec2c, 0xffec2d, astorm_skip_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( astorm_writemem )
    { 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x100000, 0x10ffff, SYS16_MWA16_TILERAM, &sys16_tileram },
	{ 0x110000, 0x110fff, SYS16_MWA16_TEXTRAM, &sys16_textram },
	{ 0x140000, 0x140fff, SYS16_MWA16_PALETTERAM, &paletteram16 },
	{ 0x200000, 0x200fff, SYS16_MWA16_SPRITERAM, &sys16_spriteram },
	{ 0xa00006, 0xa00007, sound_command_nmi_w },
	{ 0xa0000e, 0xa0000f, sys18_tilebank_w },
	{ 0xc00000, 0xc0ffff, vdp_w },
	{ 0xc46600, 0xc46601, sys18_refreshenable_w },
	{ 0xfe0020, 0xfe003f, MWA16_NOP },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

/***************************************************************************/

static void astorm_update_proc( void ){
	data16_t data;
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];

	data = sys16_textram[0x0e80/2];
	sys16_fg_page[1] = data>>12;
	sys16_fg_page[3] = (data>>8)&0xf;
	sys16_fg_page[0] = (data>>4)&0xf;
	sys16_fg_page[2] = data&0xf;

	data = sys16_textram[0x0e82/2];
	sys16_bg_page[1] = data>>12;
	sys16_bg_page[3] = (data>>8)&0xf;
	sys16_bg_page[0] = (data>>4)&0xf;
	sys16_bg_page[2] = data&0xf;

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	data = sys16_textram[0x0e84/2];
	sys16_fg2_page[1] = data>>12;
	sys16_fg2_page[3] = (data>>8)&0xf;
	sys16_fg2_page[0] = (data>>4)&0xf;
	sys16_fg2_page[2] = data&0xf;

	data = sys16_textram[0x0e86/2];
	sys16_bg2_page[1] = data>>12;
	sys16_bg2_page[3] = (data>>8)&0xf;
	sys16_bg2_page[0] = (data>>4)&0xf;
	sys16_bg2_page[2] = data&0xf;

/* enable regs */
	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2])
		sys18_fg2_active=1;
	else
		sys18_fg2_active=0;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2])
		sys18_bg2_active=1;
	else
		sys18_bg2_active=0;
}

static MACHINE_INIT( astorm ){
	sys16_fgxoffset = sys16_bgxoffset = -9;

	sys16_patch_code( 0x2D6E, 0x32 );
	sys16_patch_code( 0x2D6F, 0x3c );
	sys16_patch_code( 0x2D70, 0x80 );
	sys16_patch_code( 0x2D71, 0x00 );
	sys16_patch_code( 0x2D72, 0x33 );
	sys16_patch_code( 0x2D73, 0xc1 );
	sys16_patch_code( 0x2ea2, 0x30 );
	sys16_patch_code( 0x2ea3, 0x38 );
	sys16_patch_code( 0x2ea4, 0xec );
	sys16_patch_code( 0x2ea5, 0xf6 );
	sys16_patch_code( 0x2ea6, 0x30 );
	sys16_patch_code( 0x2ea7, 0x80 );
	sys16_patch_code( 0x2e5c, 0x30 );
	sys16_patch_code( 0x2e5d, 0x38 );
	sys16_patch_code( 0x2e5e, 0xec );
	sys16_patch_code( 0x2e5f, 0xe2 );
	sys16_patch_code( 0x2e60, 0xc0 );
	sys16_patch_code( 0x2e61, 0x7c );

	sys16_patch_code( 0x4cd8, 0x02 );
	sys16_patch_code( 0x4cec, 0x03 );
	sys16_patch_code( 0x2dc6c, 0xe9 );
	sys16_patch_code( 0x2dc64, 0x10 );
	sys16_patch_code( 0x2dc65, 0x10 );
	sys16_patch_code( 0x3a100, 0x10 );
	sys16_patch_code( 0x3a101, 0x13 );
	sys16_patch_code( 0x3a102, 0x90 );
	sys16_patch_code( 0x3a103, 0x2b );
	sys16_patch_code( 0x3a104, 0x00 );
	sys16_patch_code( 0x3a105, 0x01 );
	sys16_patch_code( 0x3a106, 0x0c );
	sys16_patch_code( 0x3a107, 0x00 );
	sys16_patch_code( 0x3a108, 0x00 );
	sys16_patch_code( 0x3a109, 0x01 );
	sys16_patch_code( 0x3a10a, 0x66 );
	sys16_patch_code( 0x3a10b, 0x06 );
	sys16_patch_code( 0x3a10c, 0x42 );
	sys16_patch_code( 0x3a10d, 0x40 );
	sys16_patch_code( 0x3a10e, 0x54 );
	sys16_patch_code( 0x3a10f, 0x8b );
	sys16_patch_code( 0x3a110, 0x60 );
	sys16_patch_code( 0x3a111, 0x02 );
	sys16_patch_code( 0x3a112, 0x30 );
	sys16_patch_code( 0x3a113, 0x1b );
	sys16_patch_code( 0x3a114, 0x34 );
	sys16_patch_code( 0x3a115, 0xc0 );
	sys16_patch_code( 0x3a116, 0x34 );
	sys16_patch_code( 0x3a117, 0xdb );
	sys16_patch_code( 0x3a118, 0x24 );
	sys16_patch_code( 0x3a119, 0xdb );
	sys16_patch_code( 0x3a11a, 0x24 );
	sys16_patch_code( 0x3a11b, 0xdb );
	sys16_patch_code( 0x3a11c, 0x4e );
	sys16_patch_code( 0x3a11d, 0x75 );
	sys16_patch_code( 0xaf8e, 0x66 );

	/* fix missing credit text */
	sys16_patch_code( 0x3f9a, 0xec );
	sys16_patch_code( 0x3f9b, 0x36 );

	sys16_update_proc = astorm_update_proc;
}

static DRIVER_INIT( astorm ){
	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];
	sys16_MaxShadowColors = 0; /* doesn't seem to use transparent shadows */
}


static MEMORY_READ16_START( aquario_readmem )
    { 0x000000, 0x2fffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, segaic16_tileram_r  },
	{ 0x410000, 0x410fff, segaic16_textram_r  },
	{ 0x600000, 0x6007ff, segaic16_spriteram_r },
	{ 0x800000, 0x801fff, SYS16_MRA16_PALETTERAM },
	{ 0xa00000, 0xa03fff,  misc_io_r }, //
	{ 0xc00000, 0xc0ffff, segavdp_r },
	{ 0xff0000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END


static MEMORY_WRITE16_START( aquario_writemem )
    { 0x000000, 0x2fffff, MWA16_ROM },
    { 0x300000, 0x3fffff, rom_5987_bank_w },
	{ 0x400000, 0x40ffff, segaic16_tileram_0_w, &segaic16_tileram_0 },
	{ 0x410000, 0x410fff, segaic16_textram_0_w, &segaic16_textram_0 },
	{ 0x600000, 0x6007ff, SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0  },
	{ 0x800000, 0x801fff, segaic16_paletteram_w, &paletteram16  },
	{ 0xa00000, 0xa03fff,  misc_io_w },
	{ 0xc00000, 0xc0ffff, segavdp_w },
	{ 0xf00006, 0xf00007, sound_command_nmi_w },
	{ 0xff0000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram},
MEMORY_END

static MEMORY_READ16_START( hamaway_readmem )
    { 0x000000, 0x07ffff, MRA16_ROM },
   	{ 0X200000, 0X27ffff, MRA16_RAM }, /*rom/bank */
	{ 0x400000, 0x40ffff, segaic16_tileram_r },
	{ 0x410000, 0x410fff, segaic16_textram_r },
	{ 0x500000, 0x5007ff, segaic16_spriteram_r },
	{ 0x840000, 0x841fff, SYS16_MRA16_PALETTERAM },
	{ 0xa00000, 0xa03fff, misc_io_r },
	{ 0xc00000, 0xc0ffff, segavdp_r },
	{ 0xff0000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( hamaway_writemem )
    { 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x3e0000, 0x3e001f, rom_837_7525_bank_w },
	{ 0x400000, 0x40ffff, segaic16_tileram_0_w, &segaic16_tileram_0},
	{ 0x410000, 0x410fff, segaic16_textram_0_w, &segaic16_textram_0  },
	{ 0x500000, 0x500fff,  SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0 },
	{ 0x840000, 0x841fff, segaic16_paletteram_w, &paletteram16  },
	{ 0xa00000, 0xa03fff, misc_io_w },
   	{ 0xc00000, 0xc0ffff, segavdp_w },
	{ 0xfe0006, 0xfe0007, sound_command_nmi_w },
	{ 0xff0000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

/*****************************************************************************/

static MACHINE_DRIVER_START( system18 )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 10000000)
	MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_CPU_ADD_TAG("sound", Z80, 8000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem_18,sound_writemem_18)
	MDRV_CPU_PORTS(sound_readport_18,sound_writeport_18)

	MDRV_FRAMES_PER_SECOND(57.23) /*dankcushions */
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(40*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(sys16_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048*ShadowColorsMultiplier)

	/* initilize system16 variables prior to driver_init and video_start */
	machine_init_sys16_onetime();

	MDRV_VIDEO_START(system18)
	MDRV_VIDEO_UPDATE(system18)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD_TAG("3438", YM3438, sys18_ym3438_interface)
	MDRV_SOUND_ADD_TAG("5c68", RF5C68, sys18_rf5c68_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( astorm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(astorm_readmem,astorm_writemem)

	MDRV_MACHINE_INIT(astorm)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( moonwalk )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(moonwalk_readmem,moonwalk_writemem)

	MDRV_MACHINE_INIT(moonwalk)

	MDRV_INSTALL_OST_SUPPORT(OST_SUPPORT_MOONWALKER)
MACHINE_DRIVER_END


static PORT_WRITE_START( mwalk_sound_writeport )
	{ 0x80, 0x80, YM2612_control_port_0_A_w },
	{ 0x81, 0x81, YM2612_data_port_0_A_w },
	{ 0x82, 0x82, YM2612_control_port_0_B_w },
	{ 0x83, 0x83, YM2612_data_port_0_B_w },
	{ 0x90, 0x90, YM2612_control_port_1_A_w },
	{ 0x91, 0x91, YM2612_data_port_1_A_w },
	{ 0x92, 0x92, YM2612_control_port_1_B_w },
	{ 0x93, 0x93, YM2612_data_port_1_B_w },
	{ 0xa0, 0xa0, sys18_soundbank_w },
	{ 0xc0, 0xc0, mwalk_sound_to_mapper_w },
PORT_END

static MACHINE_DRIVER_START( mwalk )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(mwalk_readmem,mwalk_writemem)
	MDRV_CPU_VBLANK_INT(0,0)	/* the MCU interrupts the 68000 */

	MDRV_CPU_MODIFY("sound")
	MDRV_CPU_PORTS(sound_readport_18,mwalk_sound_writeport)

	MDRV_CPU_ADD(I8051, 8000000/I8051_CLOCK_DIVIDER)	/* 8751 */
	MDRV_CPU_MEMORY(mwalk_mcu_readmem,mwalk_mcu_writemem)
	MDRV_CPU_VBLANK_INT(mwalk_mcu_vblank,1)

	MDRV_MACHINE_INIT(mwalk)
	MDRV_INTERLEAVE(262)	/* the MCU interrupts the 68000 within the VBLANK */

	MDRV_PALETTE_LENGTH(2048*3+2048)
	MDRV_VBLANK_DURATION(1000000.0 * (262 - 224) / (262 * 57.23))
	MDRV_VIDEO_START(system18_new)
	MDRV_VIDEO_UPDATE(system18_new)
	MDRV_VIDEO_EOF(system18)	/* the VDP VBLANK */

	MDRV_INSTALL_OST_SUPPORT(OST_SUPPORT_MOONWALKER)
MACHINE_DRIVER_END


/* the other games on the 315-5195 mapper, segaic16 video and VDP */
static MACHINE_DRIVER_START( sys18_astorm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(sys18_astorm_readmem,sys18_astorm_writemem)

	MDRV_MACHINE_INIT(sys18_5874)

	MDRV_PALETTE_LENGTH(2048*3+2048)
	MDRV_VBLANK_DURATION(1000000.0 * (262 - 224) / (262 * 57.23))
	MDRV_VIDEO_START(system18_new)
	MDRV_VIDEO_UPDATE(system18_new)
	MDRV_VIDEO_EOF(system18)	/* the VDP VBLANK */
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( sys18_bloxeed )
	MDRV_IMPORT_FROM(sys18_astorm)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(sys18_bloxeed_readmem,sys18_bloxeed_writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( sys18_5987 )
	MDRV_IMPORT_FROM(sys18_astorm)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(sys18_5987_readmem,sys18_5987_writemem)
	MDRV_MACHINE_INIT(sys18_5987)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( sys18_5987e )
	MDRV_IMPORT_FROM(sys18_5987)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(sys18_5987e_readmem,sys18_5987e_writemem)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( shdancbl )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(shdancbl_readmem,shdancbl_writemem)

	MDRV_CPU_MODIFY("sound")
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(shdancbl_sound_readmem,shdancbl_sound_writemem)
	MDRV_CPU_PORTS(shdancbl_sound_readport,shdancbl_sound_writeport)
	MDRV_SOUND_REMOVE("5c68")
	MDRV_SOUND_ADD_TAG("5205", MSM5205, shdancbl_msm5205_interface)

	MDRV_MACHINE_INIT(shdancbl)
MACHINE_DRIVER_END


static MACHINE_INIT( hamaway )
{
	segaic16_tilemap_reset(0);
	io_chip_5874_banking = 0;

};
static MACHINE_DRIVER_START( aquario )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(aquario_readmem,aquario_writemem)
	MDRV_PALETTE_LENGTH(2048*3+2048)
	MDRV_SCREEN_SIZE(40*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
	MDRV_VBLANK_DURATION(1000000 * (262 - 224) / (262 * 60))
	MDRV_VIDEO_START( system18_new )
	MDRV_VIDEO_UPDATE( system18_new )
	MDRV_VIDEO_EOF(system18)	/* the VDP VBLANK */
	MDRV_MACHINE_INIT( hamaway )
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( hamaway )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(hamaway_readmem,hamaway_writemem)
	MDRV_PALETTE_LENGTH(2048*3+2048)
	MDRV_SCREEN_SIZE(40*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
	MDRV_VBLANK_DURATION(1000000 * (262 - 224) / (262 * 60))
	MDRV_VIDEO_START(system18_new)
	MDRV_VIDEO_UPDATE(system18_new)
	MDRV_VIDEO_EOF(system18)	/* the VDP VBLANK */
	MDRV_MACHINE_INIT( hamaway )
	MDRV_SOUND_REMOVE("3438")
	MDRV_SOUND_ADD_TAG("3438", YM3438, hamaway)
MACHINE_DRIVER_END

/***************************************************************************/

/* System 18 games on the mapper, as in MAME: ports A-H of the I/O chip */

INPUT_PORTS_START( sys18_astorm )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easiest" )
	PORT_DIPSETTING(    0x08, "Easier" )
	PORT_DIPSETTING(    0x0c, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x14, "Harder" )
	PORT_DIPSETTING(    0x18, DEF_STR( Hardest ) )
	PORT_DIPSETTING(    0x00, "Special" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chutes" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( sys18_astorm2p )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easiest" )
	PORT_DIPSETTING(    0x08, "Easier" )
	PORT_DIPSETTING(    0x0c, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x14, "Harder" )
	PORT_DIPSETTING(    0x18, DEF_STR( Hardest ) )
	PORT_DIPSETTING(    0x00, "Special" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chutes" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( bloxeed )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "Allow VS Mode" )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Lines Per Level" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "High Speed Mode" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( cltchitr )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Game Time P1" )
	PORT_DIPSETTING(    0x04, "2 Credits 18 Outcounts 14 Min." )
	PORT_DIPSETTING(    0x00, "1 Credit 6 Outcounts 7 Min." )
	PORT_DIPSETTING(    0x08, "1 Credit 12 Outcounts 12 Min." )
	PORT_DIPSETTING(    0x0c, "1Credit 6 Outcounts 8M./2Credits 18 Outcounts 14M." )
	PORT_DIPNAME( 0x30, 0x30, "Game Time P2" )
	PORT_DIPSETTING(    0x10, "4 Credits 18 Outcounts 16 Min." )
	PORT_DIPSETTING(    0x00, "2 Credits 6 Outcounts 8 Min." )
	PORT_DIPSETTING(    0x20, "2 Credits 12 Outcounts 14 Min." )
	PORT_DIPSETTING(    0x30, "2Credits 6 Outcounts 8M./4Credits 18 Outcounts 16M." )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easiest" )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hard ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( wwally )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port C */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Chute" )
	PORT_DIPSETTING(    0x04, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x60, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* trackball 1 X */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_X | IPF_REVERSE, 75, 5, 0, 0 )

	PORT_START	/* trackball 1 Y */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_Y, 75, 5, 0, 0 )

	PORT_START	/* trackball 2 X */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_X | IPF_REVERSE | IPF_PLAYER2, 75, 5, 0, 0 )

	PORT_START	/* trackball 2 Y */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_Y | IPF_PLAYER2, 75, 5, 0, 0 )

	PORT_START	/* trackball 3 X (not connected) */
	PORT_BIT( 0xfff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* trackball 3 Y (not connected) */
	PORT_BIT( 0xfff, IP_ACTIVE_HIGH, IPT_UNUSED )

INPUT_PORTS_END

/* 3 players: the coin switches follow the Coin Chute dip, the default
   (individual) one is used here */
INPUT_PORTS_START( wwally3p )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0xfa, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port C */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Chute" )
	PORT_DIPSETTING(    0x00, "Common" )
	PORT_DIPSETTING(    0x04, "Individual" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x60, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* trackball 1 X */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_X | IPF_REVERSE, 75, 5, 0, 0 )

	PORT_START	/* trackball 1 Y */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_Y, 75, 5, 0, 0 )

	PORT_START	/* trackball 2 X */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_X | IPF_REVERSE | IPF_PLAYER2, 75, 5, 0, 0 )

	PORT_START	/* trackball 2 Y */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_Y | IPF_PLAYER2, 75, 5, 0, 0 )

	PORT_START	/* trackball 3 X */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_X | IPF_REVERSE | IPF_PLAYER3, 75, 5, 0, 0 )

	PORT_START	/* trackball 3 Y */
	PORT_ANALOG( 0xfff, 0x000, IPT_TRACKBALL_Y | IPF_PLAYER3, 75, 5, 0, 0 )

INPUT_PORTS_END

/* D. D. Crew: 2, 3 and 4 players (4 players: two more through an 8255) */
INPUT_PORTS_START( sys18_ddcrew4p )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits needed" )
	PORT_DIPSETTING(    0x01, "1 to start, 1 to continue" )
	PORT_DIPSETTING(    0x00, "2 to start, 1 to continue" )
	PORT_DIPNAME( 0x02, 0x02, "Switch to Start" )
	PORT_DIPSETTING(    0x02, "Start" )
	PORT_DIPSETTING(    0x00, "Attack" )
	PORT_DIPNAME( 0x04, 0x04, "Coin Chute" )
	PORT_DIPSETTING(    0x04, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Player Start/Continue" )
	PORT_DIPSETTING(    0x30, "3/3" )
	PORT_DIPSETTING(    0x20, "2/3" )
	PORT_DIPSETTING(    0x10, "2/2" )
	PORT_DIPSETTING(    0x00, "3/4" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 8255 port A: player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

	PORT_START	/* 8255 port B: player 4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )

	PORT_START	/* 8255 port C */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( sys18_ddcrew2p )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits needed" )
	PORT_DIPSETTING(    0x01, "1 to start, 1 to continue" )
	PORT_DIPSETTING(    0x00, "2 to start, 1 to continue" )
	PORT_DIPNAME( 0x02, 0x02, "Switch to Start" )
	PORT_DIPSETTING(    0x02, "Start" )
	PORT_DIPSETTING(    0x00, "Attack" )
	PORT_DIPNAME( 0x04, 0x04, "Coin Chute" )
	PORT_DIPSETTING(    0x04, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Player Start/Continue" )
	PORT_DIPSETTING(    0x30, "3/3" )
	PORT_DIPSETTING(    0x20, "2/3" )
	PORT_DIPSETTING(    0x10, "2/2" )
	PORT_DIPSETTING(    0x00, "3/4" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( sys18_ddcrew3p )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits needed" )
	PORT_DIPSETTING(    0x01, "1 to start, 1 to continue" )
	PORT_DIPSETTING(    0x00, "2 to start, 1 to continue" )
	PORT_DIPNAME( 0x02, 0x02, "Switch to Start" )
	PORT_DIPSETTING(    0x02, "Start" )
	PORT_DIPSETTING(    0x00, "Attack" )
	PORT_DIPNAME( 0x04, 0x04, "Coin Chute" )
	PORT_DIPSETTING(    0x04, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Player Start/Continue" )
	PORT_DIPSETTING(    0x30, "3/3" )
	PORT_DIPSETTING(    0x20, "2/3" )
	PORT_DIPSETTING(    0x10, "2/2" )
	PORT_DIPSETTING(    0x00, "3/4" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( sys18_desertbr )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Play Mode" )
	PORT_DIPSETTING(    0x00, "2 players" )
	PORT_DIPSETTING(    0x08, "3 players" )
	PORT_DIPNAME( 0x10, 0x10, "Coin Chute" )
	PORT_DIPSETTING(    0x10, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x20, 0x20, "Start Button" )
	PORT_DIPSETTING(    0x20, "Use" )
	PORT_DIPSETTING(    0x00, DEF_STR( Unused ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( sys18_lghost )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x30, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easiest" )
	PORT_DIPSETTING(    0x14, "Easier" )
	PORT_DIPSETTING(    0x18, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x04, "Harder" )
	PORT_DIPSETTING(    0x08, DEF_STR( Hardest ) )
	PORT_DIPSETTING(    0x00, "Extra Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Allow_Continue ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Coin Chute" )
	PORT_DIPSETTING(    0x00, "Common" )
	PORT_DIPSETTING(    0x40, "Individual" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* gun 1 X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X, 50, 5, 0, 255 )

	PORT_START	/* gun 1 Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y, 50, 5, 0, 255 )

	PORT_START	/* gun 2 X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 50, 5, 0, 255 )

	PORT_START	/* gun 2 Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 50, 5, 0, 255 )

	PORT_START	/* gun 3 X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER3, 50, 5, 0, 255 )

	PORT_START	/* gun 3 Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER3, 50, 5, 0, 255 )

	PORT_START	/* fake */
	PORT_DIPNAME( 0x01, 0x01, "Correct P1/P3 Gun Offsets" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( sys18_shdancer )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Time Adjust" )
	PORT_DIPSETTING(    0x00, "2.20" )
	PORT_DIPSETTING(    0x40, "2.40" )
	PORT_DIPSETTING(    0xc0, "3.00" )
	PORT_DIPSETTING(    0x80, "3.30" )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( astorm )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )
	SYS16_COINAGE
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easiest" )
	PORT_DIPSETTING(    0x08, "Easier" )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x1c, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x14, "Harder" )
	PORT_DIPSETTING(    0x18, "Hardest" )
	PORT_DIPSETTING(    0x00, "Special" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chutes" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_START /* player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
INPUT_PORTS_END

INPUT_PORTS_START( moonwalk )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_START /* service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_TILT, "Test", KEYCODE_T, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	SYS16_COINAGE
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x08, "Player Vitality" )
	PORT_DIPSETTING(    0x08, "Low" )
	PORT_DIPSETTING(    0x00, "High" )
	PORT_DIPNAME( 0x10, 0x00, "Play Mode" )
	PORT_DIPSETTING(    0x10, "2 Players" )
	PORT_DIPSETTING(    0x00, "3 Players" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Mode" )
	PORT_DIPSETTING(    0x20, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_START /* player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
/*	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
INPUT_PORTS_END

/* inputs in the order of the I/O chip ports. The coin chute dip moves coin 1:
   bit 0 of port E with a common chute (the default here), bit 7 with
   individual chutes, where bit 0 is coin 3 */
INPUT_PORTS_START( mwalk )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x08, "Player Vitality" )
	PORT_DIPSETTING(    0x08, "Low" )
	PORT_DIPSETTING(    0x00, "High" )
	PORT_DIPNAME( 0x10, 0x10, "Play Mode" )
	PORT_DIPSETTING(    0x10, "2 Players" )
	PORT_DIPSETTING(    0x00, "3 Players" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chute" )
	PORT_DIPSETTING(    0x20, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* US: the play mode and coin chute dips are inverted for the 3 players
   cabinet, whose default is individual coin chutes */
INPUT_PORTS_START( mwalka )
	PORT_START	/* port A: player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START	/* port B: player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START	/* port C: player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )

	PORT_START	/* port D: outputs */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* port E: service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	SYS16_COINAGE	/* port F */

	PORT_START	/* port G: DSW */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x08, "Player Vitality" )
	PORT_DIPSETTING(    0x08, "Low" )
	PORT_DIPSETTING(    0x00, "High" )
	PORT_DIPNAME( 0x10, 0x10, "Play Mode" )
	PORT_DIPSETTING(    0x00, "2 Players" )
	PORT_DIPSETTING(    0x10, "3 Players" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chute" )
	PORT_DIPSETTING(    0x00, "Common" )
	PORT_DIPSETTING(    0x20, "Individual" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* port H: tile banking */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( shdancer )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	SYS16_SERVICE
	SYS16_COINAGE
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, "Time Adjust" )
	PORT_DIPSETTING(    0x00, "2.20" )
	PORT_DIPSETTING(    0x40, "2.40" )
	PORT_DIPSETTING(    0xc0, "3.00" )
	PORT_DIPSETTING(    0x80, "3.30" )
INPUT_PORTS_END

INPUT_PORTS_START( aquario )
	PORT_START  /*P1*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START /*P2*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START  /*PORTC*/
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /*PORTD*/
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /*SERVICE*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /*COINAGE */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0x04, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x02, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin B too) or 1/1" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0x40, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x20, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin A too) or 1/1" )

	PORT_START /*DSW*/
PORT_DIPNAME( 0x01, 0x01, "Credits to Start" ) 
	PORT_DIPSETTING(    0x01, "1")
	PORT_DIPSETTING(    0x00, "2")
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, "Number of Players" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Normal ) )
	PORT_DIPNAME( 0x20, 0x20, "Switch to Start" ) 
	PORT_DIPSETTING(    0x20, "Start" )
	PORT_DIPSETTING(    0x00, "Attack" )
	//"SW2:7" unused
	//"SW2:8" unused
	PORT_START /*PORTH*/
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( hamaway )
	PORT_START  /*P1*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START /*P2*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START  /*PORTC*/
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /*PORTD*/
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /*SERVICE*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /*COINAGE */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0x04, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x02, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin B too) or 1/1" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0x40, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x20, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin A too) or 1/1" )

	PORT_START /*DSW*/
PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
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

	PORT_START /*PORTH*/
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END
/*****************************************************************************/

/* Alien Storm */
ROM_START( astorm )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13182.a6", 0x000000, 0x40000, CRC(e31f2a1c) SHA1(690ee10c36e5bb6175470fb5564527e0e4a94d2c) )
	ROM_LOAD16_BYTE( "epr-13181.a5", 0x000001, 0x40000, CRC(78cd3b26) SHA1(a81b807c5da625d8e4648ae80c41e4ca3870c0fa) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0154.key", 0x0000, 0x2000, CRC(b86b6b8f) SHA1(869405383d563a3f3842c89462a7b2e184928532) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083a.bin", 0x000000, 0x20000, CRC(e7528e06) SHA1(1f4e618692c20aeb316d578c5ded12440eb072ab) ) /* Also known to come with EPR-13083B ROM instead of EPR-13083A (like astormj) */
	ROM_LOAD( "epr-13076.bin",  0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin",  0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin",  0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astorm3 )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13165.a6", 0x000000, 0x40000, CRC(6efcd381) SHA1(547c6703a34c3b9b887f5a63ec59a7055067bf3b) )
	ROM_LOAD16_BYTE( "epr-13164.a5", 0x000001, 0x40000, CRC(97d693c6) SHA1(1a9aa98b32aae9367ed897e6931b2633b11b079e) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0148.key", 0x0000, 0x2000, CRC(72e4b64a) SHA1(945580d0cf25691370e9f2056cdffc01331d54ad) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083.bin", 0x000000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) ) /* Also known to come with EPR-13083A ROM instead of EPR-13083 */
	ROM_LOAD( "epr-13076.bin", 0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin", 0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin", 0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astormu )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13095.a6", 0x000000, 0x40000, CRC(55d40742) SHA1(c30fcd7da1fe062b1f00275dc8ac79c3c619b719) )
	ROM_LOAD16_BYTE( "epr-13094.a5", 0x000001, 0x40000, CRC(92b305f9) SHA1(d24a1de619d29a8f6ff9dfce455c2c7d6457ddbe) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0147.key", 0x0000, 0x2000, CRC(3fd54ba7) SHA1(2d74f44f2ed779ed2b119b4fc0bc844d90678c74) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083.bin", 0x000000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) )
	ROM_LOAD( "epr-13076.bin", 0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin", 0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin", 0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astormj )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13085.a6", 0x000000, 0x40000, CRC(15f74e2d) SHA1(30d9d099ec18907edd3d20df312565c3bd5a80de) )
	ROM_LOAD16_BYTE( "epr-13084.a5", 0x000001, 0x40000, CRC(9687b38f) SHA1(cdeb5b4f06ad4ad8ca579392c1ec901487b08e76) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0146.key", 0x0000, 0x2000, CRC(e94991c5) SHA1(c9a8b56e01792654436f24b219d7a92c0852461f) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083b.bin", 0x000000, 0x20000, CRC(169b4b5f) SHA1(bb3dfc4aff939df5ad04cf3ecb8e7de366097743) )
	ROM_LOAD( "epr-13076.bin",  0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin",  0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin",  0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astorm3d )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13165.a6", 0x000000, 0x40000, CRC(2a0dbff5) SHA1(c58f999e0362ec88f2596cd8dbb405af64c90209) )
	ROM_LOAD16_BYTE( "bootleg_epr-13164.a5", 0x000001, 0x40000, CRC(f981a183) SHA1(5077a28ece7f2284e6fb98243776a5e64745303c) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083.bin", 0x000000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) )
	ROM_LOAD( "epr-13076.bin", 0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin", 0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin", 0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astormud )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13095.a6", 0x000000, 0x40000, CRC(a29b1eea) SHA1(ff34ab6806a4aea3519cefe06c8b85962bd9942d) )
	ROM_LOAD16_BYTE( "bootleg_epr-13094.a5", 0x000001, 0x40000, CRC(81f4c6b6) SHA1(c2c69b281a90635746c76d285b2a8342151a1cd5) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083.bin", 0x000000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) )
	ROM_LOAD( "epr-13076.bin", 0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin", 0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin", 0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astormjd )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13085.a6", 0x000000, 0x40000, CRC(c0d7f3c2) SHA1(1123f0d4cf25c3ee82a57212d9ffee49667c9f84) )
	ROM_LOAD16_BYTE( "bootleg_epr-13084.a5", 0x000001, 0x40000, CRC(006635f0) SHA1(7b4fce33151c7cfe5a33065a5b33ff678bac933c) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr-13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr-13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr-13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr-13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr-13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr-13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr-13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr-13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr-13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13083b.bin", 0x000000, 0x20000, CRC(169b4b5f) SHA1(bb3dfc4aff939df5ad04cf3ecb8e7de366097743) )
	ROM_LOAD( "epr-13076.bin",  0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr-13077.bin",  0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr-13078.bin",  0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( bloxeeda )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-12890.a6", 0x000000, 0x20000, CRC(b0b85294) SHA1(8d9142f593eb48a5b22a1b4b1ef7f6805e88f9bb) ) /* Only shows (c) 1989 SEGA */
	ROM_LOAD16_BYTE( "epr-12889.a5", 0x000001, 0x20000, CRC(96c67e36) SHA1(a9370307ca779a892b1e98ba7f2e3415b7188f91) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0139.key", 0x0000, 0x2000, CRC(9aae84cb) SHA1(806515d61ecacb260b2b5e5fe023b494d35ce315) )

	ROM_REGION( 0x30000, REGION_GFX1, 0 )
	ROM_LOAD( "opr-12884.b1", 0x00000, 0x10000, CRC(e024aa33) SHA1(d734be240cd05031aaadf9735c0b1b00e8e6d4cb) )
	ROM_LOAD( "opr-12885.b2", 0x10000, 0x10000, CRC(8041b814) SHA1(29fa49ba9a73eed07865a86ea774e2c6a60aed5b) )
	ROM_LOAD( "opr-12886.b3", 0x20000, 0x10000, CRC(de32285e) SHA1(8994dc128d6a23763e5fcfca1868b336d4aa0a21) )

	ROM_REGION16_BE( 0x20000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "opr-12891.a11", 0x00000, 0x10000, CRC(90d31a8c) SHA1(1747652a5109ce65add197cf06535f2463a99fdc) )
	ROM_LOAD16_BYTE( "opr-12887.b11", 0x00001, 0x10000, CRC(f0c0f49d) SHA1(7ecd591265165f3149241e2ceb5059faab88360f) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12888.a4", 0x000000, 0x20000, CRC(6f2fc63c) SHA1(3cce22c8f80013f05b5a2d36c42a61a81e4d6cbd) )
ROM_END

ROM_START( bloxeedd )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-12911.a6", 0x000000, 0x20000, CRC(2d634453) SHA1(7a17084057c89d581125b7823c04219a3fb599a5))
	ROM_LOAD16_BYTE( "bootleg_epr-12910.a5", 0x000001, 0x20000, CRC(e6d2e9cc) SHA1(3ee269bce104d0da9a2eb0d42a07924f60f4d3a8) )

	ROM_REGION( 0x30000, REGION_GFX1, 0 )
	ROM_LOAD( "opr-12884.b1", 0x00000, 0x10000, CRC(e024aa33) SHA1(d734be240cd05031aaadf9735c0b1b00e8e6d4cb) )
	ROM_LOAD( "opr-12885.b2", 0x10000, 0x10000, CRC(8041b814) SHA1(29fa49ba9a73eed07865a86ea774e2c6a60aed5b) )
	ROM_LOAD( "opr-12886.b3", 0x20000, 0x10000, CRC(de32285e) SHA1(8994dc128d6a23763e5fcfca1868b336d4aa0a21) )

	ROM_REGION16_BE( 0x20000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "opr-12891.a11", 0x00000, 0x10000, CRC(90d31a8c) SHA1(1747652a5109ce65add197cf06535f2463a99fdc) )
	ROM_LOAD16_BYTE( "opr-12887.b11", 0x00001, 0x10000, CRC(f0c0f49d) SHA1(7ecd591265165f3149241e2ceb5059faab88360f) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12888.a4", 0x000000, 0x20000, CRC(6f2fc63c) SHA1(3cce22c8f80013f05b5a2d36c42a61a81e4d6cbd) )
ROM_END

ROM_START( cltchitrj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13783.a4", 0x00000, 0x40000, CRC(e2a1d5af) SHA1(cb6710fe2093889d5d159eaf55a3bd95c6f2ef87) )
	ROM_LOAD16_BYTE( "epr-13796.a6", 0x00001, 0x40000, CRC(06001c67) SHA1(3aa48631013e6dc55e4c1d222b465e6b41ece36b) )
	ROM_LOAD16_BYTE( "epr-13785.a5", 0x80000, 0x40000, CRC(09714762) SHA1(c75c88b1c313e172fdb7f9a570d57be38f959b2b) )
	ROM_LOAD16_BYTE( "epr-13797.a7", 0x80001, 0x40000, CRC(361ade9f) SHA1(a7fd48c55695fd322d0456ff7dc2d2b2bc3e561b) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0175.key", 0x0000, 0x2000, CRC(70d9d283) SHA1(ff309b2a221d9a03ccf301a208c76a7c2eaea790) )

	ROM_REGION( 0x180000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13773.c1",  0x000000, 0x80000, CRC(3fc600e5) SHA1(8bec4ecf6a4e4b38d13133960036a5a4800a668e) )
	ROM_LOAD( "mpr-13774.c2",  0x080000, 0x80000, CRC(2411a824) SHA1(0e383ccc4e0830ffb395d5102e2950610c147007) )
	ROM_LOAD( "mpr-13775.c3",  0x100000, 0x80000, CRC(cf527bf6) SHA1(1f9cf1f0e92709f0465dc97ebbdaa715a419f7a7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13779.c10", 0x000001, 0x80000, CRC(c707f416) SHA1(e6a9d89849f7f1c303a3ca29a629f81397945a2d) )
	ROM_LOAD16_BYTE( "mpr-13787.a10", 0x000000, 0x80000, CRC(f05c68c6) SHA1(b6a0535b6c734a0c89fdb6506c32ffe6ab3aa8cd) )
	ROM_LOAD16_BYTE( "mpr-13780.c11", 0x200001, 0x80000, CRC(a4c341e0) SHA1(15a0b5a42b56465a7b7df98968cc2ed177ce6f59) )
	ROM_LOAD16_BYTE( "mpr-13788.a11", 0x200000, 0x80000, CRC(0106fea6) SHA1(e16e2a469ecbbc704021dee6264db30aa0898368) )
	ROM_LOAD16_BYTE( "mpr-13781.c12", 0x400001, 0x80000, CRC(f33b13af) SHA1(d3eb64dcf12d532454bf3cd6c86528c0f11ee316) )
	ROM_LOAD16_BYTE( "mpr-13789.a12", 0x400000, 0x80000, CRC(09ba8835) SHA1(72e83dd1793a7f4b2b881e71f262493e3d4992b3) )
	/* extra gfx ROMs?? */
	ROM_LOAD16_BYTE( "epr-13782.c13", 0x600001, 0x40000, CRC(73790852) SHA1(891345cb8ce4b04bd293ee9bac9b1b9940d6bcb2) )
	ROM_LOAD16_BYTE( "epr-13790.a13", 0x600000, 0x40000, CRC(23849101) SHA1(1aeb0fefb6688dfd841bd7d0b17ffdfce69f1dd9) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
 /* another copy in different set is epr-13778.b7 - 9c54c038 */
	ROM_LOAD( "epr-13778.c7", 0x000000, 0x20000, CRC(35e86146) SHA1(9be82165dc12d5f32ef26f37ea02b29e3621893f) )
	ROM_LOAD( "epr-13777.c6", 0x080000, 0x80000, CRC(d1524782) SHA1(121c5804927ed686ea50d5d81d0226e041f50f6f) )
	ROM_LOAD( "epr-13776.c5", 0x100000, 0x80000, CRC(282ac9fe) SHA1(4f54d93779c35c036d7c85fce6736df80f3bbe33) )
ROM_END

ROM_START( cltchitrd )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13794.a4", 0x00000, 0x40000, CRC(b15de2c5) SHA1(06bbd1de3429a9543ebe69895f2c485feaf54187) )
	ROM_LOAD16_BYTE( "bootleg_epr-13795.a6", 0x00001, 0x40000, CRC(8fc7e524) SHA1(8c55cbb952e4b6aef51b9bd417f842284cb76663) )
	ROM_LOAD16_BYTE( "epr-13784.a5", 0x80000, 0x40000, CRC(80c8180d) SHA1(80e72ab7d97714009fd31b3d50176af84b0dcdb7) )
	ROM_LOAD16_BYTE( "epr-13786.a7", 0x80001, 0x40000, CRC(3095dac0) SHA1(20edce74b6f2a82a3865613e601a0e212615d0e4) )

	ROM_REGION( 0x180000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13773.c1",  0x000000, 0x80000, CRC(3fc600e5) SHA1(8bec4ecf6a4e4b38d13133960036a5a4800a668e) )
	ROM_LOAD( "mpr-13774.c2",  0x080000, 0x80000, CRC(2411a824) SHA1(0e383ccc4e0830ffb395d5102e2950610c147007) )
	ROM_LOAD( "mpr-13775.c3",  0x100000, 0x80000, CRC(cf527bf6) SHA1(1f9cf1f0e92709f0465dc97ebbdaa715a419f7a7) )

	ROM_REGION16_BE( 0x600000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13779.c10", 0x000001, 0x80000, CRC(c707f416) SHA1(e6a9d89849f7f1c303a3ca29a629f81397945a2d) )
	ROM_LOAD16_BYTE( "mpr-13787.a10", 0x000000, 0x80000, CRC(f05c68c6) SHA1(b6a0535b6c734a0c89fdb6506c32ffe6ab3aa8cd) )
	ROM_LOAD16_BYTE( "mpr-13780.c11", 0x200001, 0x80000, CRC(a4c341e0) SHA1(15a0b5a42b56465a7b7df98968cc2ed177ce6f59) )
	ROM_LOAD16_BYTE( "mpr-13788.a11", 0x200000, 0x80000, CRC(0106fea6) SHA1(e16e2a469ecbbc704021dee6264db30aa0898368) )
	ROM_LOAD16_BYTE( "mpr-13781.c12", 0x400001, 0x80000, CRC(f33b13af) SHA1(d3eb64dcf12d532454bf3cd6c86528c0f11ee316) )
	ROM_LOAD16_BYTE( "mpr-13789.a12", 0x400000, 0x80000, CRC(09ba8835) SHA1(72e83dd1793a7f4b2b881e71f262493e3d4992b3) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13793.c7", 0x000000, 0x80000, CRC(a3d31944) SHA1(44d17aa0598eacfac4b12c8955fd1067ca09abbd) )
	ROM_LOAD( "epr-13792.c6", 0x080000, 0x80000, CRC(808f9695) SHA1(d50677d20083ad56dbf0864db05f76f93a4e9eba) )
	ROM_LOAD( "epr-13791.c5", 0x100000, 0x80000, CRC(35c16d80) SHA1(7ed04600748c5efb63e25f066daa163e9c0d8038) )
ROM_END

ROM_START( cltchitrjd )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13783.a4", 0x00000, 0x40000, CRC(dafd48af) SHA1(5be2d1f4c75426f3d41cbb43d63b1eceae8c3d57) )
	ROM_LOAD16_BYTE( "bootleg_epr-13796.a6", 0x00001, 0x40000, CRC(219aae33) SHA1(2dc9c32cfd82abc9f4855459ea427f05a299d596) )
	ROM_LOAD16_BYTE( "epr-13785.a5", 0x80000, 0x40000, CRC(09714762) SHA1(c75c88b1c313e172fdb7f9a570d57be38f959b2b) )
	ROM_LOAD16_BYTE( "epr-13797.a7", 0x80001, 0x40000, CRC(361ade9f) SHA1(a7fd48c55695fd322d0456ff7dc2d2b2bc3e561b) )

	ROM_REGION( 0x180000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13773.c1",  0x000000, 0x80000, CRC(3fc600e5) SHA1(8bec4ecf6a4e4b38d13133960036a5a4800a668e) )
	ROM_LOAD( "mpr-13774.c2",  0x080000, 0x80000, CRC(2411a824) SHA1(0e383ccc4e0830ffb395d5102e2950610c147007) )
	ROM_LOAD( "mpr-13775.c3",  0x100000, 0x80000, CRC(cf527bf6) SHA1(1f9cf1f0e92709f0465dc97ebbdaa715a419f7a7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13779.c10", 0x000001, 0x80000, CRC(c707f416) SHA1(e6a9d89849f7f1c303a3ca29a629f81397945a2d) )
	ROM_LOAD16_BYTE( "mpr-13787.a10", 0x000000, 0x80000, CRC(f05c68c6) SHA1(b6a0535b6c734a0c89fdb6506c32ffe6ab3aa8cd) )
	ROM_LOAD16_BYTE( "mpr-13780.c11", 0x200001, 0x80000, CRC(a4c341e0) SHA1(15a0b5a42b56465a7b7df98968cc2ed177ce6f59) )
	ROM_LOAD16_BYTE( "mpr-13788.a11", 0x200000, 0x80000, CRC(0106fea6) SHA1(e16e2a469ecbbc704021dee6264db30aa0898368) )
	ROM_LOAD16_BYTE( "mpr-13781.c12", 0x400001, 0x80000, CRC(f33b13af) SHA1(d3eb64dcf12d532454bf3cd6c86528c0f11ee316) )
	ROM_LOAD16_BYTE( "mpr-13789.a12", 0x400000, 0x80000, CRC(09ba8835) SHA1(72e83dd1793a7f4b2b881e71f262493e3d4992b3) )
	/* extra gfx ROMs?? */
	ROM_LOAD16_BYTE( "epr-13782.c13", 0x600001, 0x40000, CRC(73790852) SHA1(891345cb8ce4b04bd293ee9bac9b1b9940d6bcb2) )
	ROM_LOAD16_BYTE( "epr-13790.a13", 0x600000, 0x40000, CRC(23849101) SHA1(1aeb0fefb6688dfd841bd7d0b17ffdfce69f1dd9) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
 /* another copy in different set is epr-13778.b7 - 9c54c038 */
	ROM_LOAD( "epr-13778.c7", 0x000000, 0x20000, CRC(35e86146) SHA1(9be82165dc12d5f32ef26f37ea02b29e3621893f) )
	ROM_LOAD( "epr-13777.c6", 0x080000, 0x80000, CRC(d1524782) SHA1(121c5804927ed686ea50d5d81d0226e041f50f6f) )
	ROM_LOAD( "epr-13776.c5", 0x100000, 0x80000, CRC(282ac9fe) SHA1(4f54d93779c35c036d7c85fce6736df80f3bbe33) )
ROM_END

ROM_START( wwallyj )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code - custom CPU 317-0197B */
	ROM_LOAD16_BYTE( "epr-14730b.a4", 0x000000, 0x40000, CRC(e72bc17a) SHA1(ac3b7d86571a6f510c202735134c1bc4809aa26e) )
	ROM_LOAD16_BYTE( "epr-14731b.a6", 0x000001, 0x40000, CRC(6e3235b9) SHA1(11d5628644e8301550c36c93e5f137c67c11e735) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0197b.key", 0x0000, 0x2000, CRC(f5b7c5b4) SHA1(be971a2349e7c3adc995581355fea48f5123421c) )

	ROM_REGION( 0x0c0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14719.c1", 0x000000, 0x40000, CRC(8b58c743) SHA1(ee50baa184d68558d62d1817b2b9c138226ed25a) )
	ROM_LOAD( "mpr-14720.c2", 0x040000, 0x40000, CRC(f96d19f4) SHA1(e350b551db8d01062397cd9a0c952a7b5dbf3fe6) )
	ROM_LOAD( "mpr-14721.c3", 0x080000, 0x40000, CRC(c4ced91d) SHA1(4c8a070959ca10e2dddf407ddbba212415d78727) )

	ROM_REGION16_BE( 0x500000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14726.c10", 0x000001, 0x100000, CRC(7213d1d3) SHA1(b70346a1dd3aa112bc696a7f4dd9ca908c3e5afa) )
	ROM_LOAD16_BYTE( "mpr-14732.a10", 0x000000, 0x100000, CRC(04ced549) SHA1(59fd6510f0e14613d830ac6527f12ccc0b9351a5) )
	ROM_LOAD16_BYTE( "mpr-14727.c11", 0x200001, 0x100000, CRC(3b74e0f0) SHA1(40432dbbbf75dae1e5e32b7cc2c4884f5e9e3bf5) )
	ROM_LOAD16_BYTE( "mpr-14733.a11", 0x200000, 0x100000, CRC(6da0444f) SHA1(80c32895af19bda3277376fdbd1c163f0ed25665) )
	ROM_LOAD16_BYTE( "mpr-14728.c12", 0x400001, 0x080000, CRC(5b921587) SHA1(2779dc658bb7a51f2399af5a6463ccb2dd388e88) )
	ROM_LOAD16_BYTE( "mpr-14734.a12", 0x400000, 0x080000, CRC(6f3f5ed9) SHA1(01972c8bd5bfde58715bc0adc7dea73bf6350a26) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14725.c7",   0x000000, 0x20000, CRC(2b29684f) SHA1(b83962a4f475f9b3e79d4f7ff577b170c4043156) )
	ROM_LOAD( "mpr-14724.c6",   0x080000, 0x80000, CRC(47cbea86) SHA1(c70d1fed2912c7c05223ce0bb0941706f957295f) )
	ROM_LOAD( "mpr-14723.c5",   0x100000, 0x80000, CRC(bc5adc27) SHA1(09405002b940a3d3eb0f1258f37af51e0b7581b9) )
	ROM_LOAD( "mpr-14722.c4",   0x180000, 0x80000, CRC(1bd081f8) SHA1(e5b0b5d8334486f813d7c430bb7fce3f69605a21) )
ROM_END

ROM_START( wwallyja )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code - custom CPU 317-0197A */
	ROM_LOAD16_BYTE( "epr-14730a.a4", 0x000000, 0x40000, CRC(daa7880e) SHA1(9ea83e04c3e07d84afa67097c28b3951c9db8d00) )
	ROM_LOAD16_BYTE( "epr-14731a.a6", 0x000001, 0x40000, CRC(5e36353b) SHA1(488c54bbef3c8a129785465887bff3b301e11387) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0197a.key", 0x0000, 0x2000, CRC(2fb6a9a1) SHA1(2649d0905527dbe0dd0ad5cf68c457b4aa5fb32c) )

	ROM_REGION( 0x0c0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14719.c1", 0x000000, 0x40000, CRC(8b58c743) SHA1(ee50baa184d68558d62d1817b2b9c138226ed25a) )
	ROM_LOAD( "mpr-14720.c2", 0x040000, 0x40000, CRC(f96d19f4) SHA1(e350b551db8d01062397cd9a0c952a7b5dbf3fe6) )
	ROM_LOAD( "mpr-14721.c3", 0x080000, 0x40000, CRC(c4ced91d) SHA1(4c8a070959ca10e2dddf407ddbba212415d78727) )

	ROM_REGION16_BE( 0x500000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14726.c10", 0x000001, 0x100000, CRC(7213d1d3) SHA1(b70346a1dd3aa112bc696a7f4dd9ca908c3e5afa) )
	ROM_LOAD16_BYTE( "mpr-14732.a10", 0x000000, 0x100000, CRC(04ced549) SHA1(59fd6510f0e14613d830ac6527f12ccc0b9351a5) )
	ROM_LOAD16_BYTE( "mpr-14727.c11", 0x200001, 0x100000, CRC(3b74e0f0) SHA1(40432dbbbf75dae1e5e32b7cc2c4884f5e9e3bf5) )
	ROM_LOAD16_BYTE( "mpr-14733.a11", 0x200000, 0x100000, CRC(6da0444f) SHA1(80c32895af19bda3277376fdbd1c163f0ed25665) )
	ROM_LOAD16_BYTE( "mpr-14728.c12", 0x400001, 0x080000, CRC(5b921587) SHA1(2779dc658bb7a51f2399af5a6463ccb2dd388e88) )
	ROM_LOAD16_BYTE( "mpr-14734.a12", 0x400000, 0x080000, CRC(6f3f5ed9) SHA1(01972c8bd5bfde58715bc0adc7dea73bf6350a26) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14725.c7",   0x000000, 0x20000, CRC(2b29684f) SHA1(b83962a4f475f9b3e79d4f7ff577b170c4043156) )
	ROM_LOAD( "mpr-14724.c6",   0x080000, 0x80000, CRC(47cbea86) SHA1(c70d1fed2912c7c05223ce0bb0941706f957295f) )
	ROM_LOAD( "mpr-14723.c5",   0x100000, 0x80000, CRC(bc5adc27) SHA1(09405002b940a3d3eb0f1258f37af51e0b7581b9) )
	ROM_LOAD( "mpr-14722.c4",   0x180000, 0x80000, CRC(1bd081f8) SHA1(e5b0b5d8334486f813d7c430bb7fce3f69605a21) )
ROM_END

ROM_START( wwallyja3p )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code - custom CPU 317-0198A */
	ROM_LOAD16_BYTE( "epr-14736a.a4", 0x000000, 0x40000, CRC(d897bcc6) SHA1(15f0b602eb3ac870241f53246a4251c07b27fb27) )
	ROM_LOAD16_BYTE( "epr-14737a.a6", 0x000001, 0x40000, CRC(64328385) SHA1(7220d285762ec9d3656cb958cc71b43094aa4052) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0198a.key", 0x0000, 0x2000, CRC(32d313a4) SHA1(bb7a651c34a9e925d5a9928fabb77ca8724009f2) )

	ROM_REGION( 0x0c0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14719.c1", 0x000000, 0x40000, CRC(8b58c743) SHA1(ee50baa184d68558d62d1817b2b9c138226ed25a) )
	ROM_LOAD( "mpr-14720.c2", 0x040000, 0x40000, CRC(f96d19f4) SHA1(e350b551db8d01062397cd9a0c952a7b5dbf3fe6) )
	ROM_LOAD( "mpr-14721.c3", 0x080000, 0x40000, CRC(c4ced91d) SHA1(4c8a070959ca10e2dddf407ddbba212415d78727) )

	ROM_REGION16_BE( 0x500000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14726.c10", 0x000001, 0x100000, CRC(7213d1d3) SHA1(b70346a1dd3aa112bc696a7f4dd9ca908c3e5afa) )
	ROM_LOAD16_BYTE( "mpr-14732.a10", 0x000000, 0x100000, CRC(04ced549) SHA1(59fd6510f0e14613d830ac6527f12ccc0b9351a5) )
	ROM_LOAD16_BYTE( "mpr-14727.c11", 0x200001, 0x100000, CRC(3b74e0f0) SHA1(40432dbbbf75dae1e5e32b7cc2c4884f5e9e3bf5) )
	ROM_LOAD16_BYTE( "mpr-14733.a11", 0x200000, 0x100000, CRC(6da0444f) SHA1(80c32895af19bda3277376fdbd1c163f0ed25665) )
	ROM_LOAD16_BYTE( "mpr-14728.c12", 0x400001, 0x080000, CRC(5b921587) SHA1(2779dc658bb7a51f2399af5a6463ccb2dd388e88) )
	ROM_LOAD16_BYTE( "mpr-14734.a12", 0x400000, 0x080000, CRC(6f3f5ed9) SHA1(01972c8bd5bfde58715bc0adc7dea73bf6350a26) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14725.c7",   0x000000, 0x20000, CRC(2b29684f) SHA1(b83962a4f475f9b3e79d4f7ff577b170c4043156) )
	ROM_LOAD( "mpr-14724.c6",   0x080000, 0x80000, CRC(47cbea86) SHA1(c70d1fed2912c7c05223ce0bb0941706f957295f) )
	ROM_LOAD( "mpr-14723.c5",   0x100000, 0x80000, CRC(bc5adc27) SHA1(09405002b940a3d3eb0f1258f37af51e0b7581b9) )
	ROM_LOAD( "mpr-14722.c4",   0x180000, 0x80000, CRC(1bd081f8) SHA1(e5b0b5d8334486f813d7c430bb7fce3f69605a21) )
ROM_END

ROM_START( wwallyjd )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-14730b.a4", 0x000000, 0x40000, CRC(1d46f791) SHA1(5bfffd108254db69e582a4e4c1012b9c9e223830) )
	ROM_LOAD16_BYTE( "bootleg_epr-14731b.a6", 0x000001, 0x40000, CRC(a0f6325e) SHA1(a952d08c3d2324425db9d959e4e43f0c7024165b) )

	ROM_REGION( 0x0c0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14719.c1", 0x000000, 0x40000, CRC(8b58c743) SHA1(ee50baa184d68558d62d1817b2b9c138226ed25a) )
	ROM_LOAD( "mpr-14720.c2", 0x040000, 0x40000, CRC(f96d19f4) SHA1(e350b551db8d01062397cd9a0c952a7b5dbf3fe6) )
	ROM_LOAD( "mpr-14721.c3", 0x080000, 0x40000, CRC(c4ced91d) SHA1(4c8a070959ca10e2dddf407ddbba212415d78727) )

	ROM_REGION16_BE( 0x500000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14726.c10", 0x000001, 0x100000, CRC(7213d1d3) SHA1(b70346a1dd3aa112bc696a7f4dd9ca908c3e5afa) )
	ROM_LOAD16_BYTE( "mpr-14732.a10", 0x000000, 0x100000, CRC(04ced549) SHA1(59fd6510f0e14613d830ac6527f12ccc0b9351a5) )
	ROM_LOAD16_BYTE( "mpr-14727.c11", 0x200001, 0x100000, CRC(3b74e0f0) SHA1(40432dbbbf75dae1e5e32b7cc2c4884f5e9e3bf5) )
	ROM_LOAD16_BYTE( "mpr-14733.a11", 0x200000, 0x100000, CRC(6da0444f) SHA1(80c32895af19bda3277376fdbd1c163f0ed25665) )
	ROM_LOAD16_BYTE( "mpr-14728.c12", 0x400001, 0x080000, CRC(5b921587) SHA1(2779dc658bb7a51f2399af5a6463ccb2dd388e88) )
	ROM_LOAD16_BYTE( "mpr-14734.a12", 0x400000, 0x080000, CRC(6f3f5ed9) SHA1(01972c8bd5bfde58715bc0adc7dea73bf6350a26) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14725.c7",   0x000000, 0x20000, CRC(2b29684f) SHA1(b83962a4f475f9b3e79d4f7ff577b170c4043156) )
	ROM_LOAD( "mpr-14724.c6",   0x080000, 0x80000, CRC(47cbea86) SHA1(c70d1fed2912c7c05223ce0bb0941706f957295f) )
	ROM_LOAD( "mpr-14723.c5",   0x100000, 0x80000, CRC(bc5adc27) SHA1(09405002b940a3d3eb0f1258f37af51e0b7581b9) )
	ROM_LOAD( "mpr-14722.c4",   0x180000, 0x80000, CRC(1bd081f8) SHA1(e5b0b5d8334486f813d7c430bb7fce3f69605a21) )
ROM_END

ROM_START( wwallyjad )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-14730a.a4", 0x000000, 0x40000, CRC(0d9fb6df) SHA1(ea9c6ff545e31a8269c2e23620582e95817dac60))
	ROM_LOAD16_BYTE( "bootleg_epr-14731a.a6", 0x000001, 0x40000, CRC(b88c140d) SHA1(407ded758caf76644d728e046d94b464fa03ca02) )

	ROM_REGION( 0x0c0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14719.c1", 0x000000, 0x40000, CRC(8b58c743) SHA1(ee50baa184d68558d62d1817b2b9c138226ed25a) )
	ROM_LOAD( "mpr-14720.c2", 0x040000, 0x40000, CRC(f96d19f4) SHA1(e350b551db8d01062397cd9a0c952a7b5dbf3fe6) )
	ROM_LOAD( "mpr-14721.c3", 0x080000, 0x40000, CRC(c4ced91d) SHA1(4c8a070959ca10e2dddf407ddbba212415d78727) )

	ROM_REGION16_BE( 0x500000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14726.c10", 0x000001, 0x100000, CRC(7213d1d3) SHA1(b70346a1dd3aa112bc696a7f4dd9ca908c3e5afa) )
	ROM_LOAD16_BYTE( "mpr-14732.a10", 0x000000, 0x100000, CRC(04ced549) SHA1(59fd6510f0e14613d830ac6527f12ccc0b9351a5) )
	ROM_LOAD16_BYTE( "mpr-14727.c11", 0x200001, 0x100000, CRC(3b74e0f0) SHA1(40432dbbbf75dae1e5e32b7cc2c4884f5e9e3bf5) )
	ROM_LOAD16_BYTE( "mpr-14733.a11", 0x200000, 0x100000, CRC(6da0444f) SHA1(80c32895af19bda3277376fdbd1c163f0ed25665) )
	ROM_LOAD16_BYTE( "mpr-14728.c12", 0x400001, 0x080000, CRC(5b921587) SHA1(2779dc658bb7a51f2399af5a6463ccb2dd388e88) )
	ROM_LOAD16_BYTE( "mpr-14734.a12", 0x400000, 0x080000, CRC(6f3f5ed9) SHA1(01972c8bd5bfde58715bc0adc7dea73bf6350a26) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14725.c7",   0x000000, 0x20000, CRC(2b29684f) SHA1(b83962a4f475f9b3e79d4f7ff577b170c4043156) )
	ROM_LOAD( "mpr-14724.c6",   0x080000, 0x80000, CRC(47cbea86) SHA1(c70d1fed2912c7c05223ce0bb0941706f957295f) )
	ROM_LOAD( "mpr-14723.c5",   0x100000, 0x80000, CRC(bc5adc27) SHA1(09405002b940a3d3eb0f1258f37af51e0b7581b9) )
	ROM_LOAD( "mpr-14722.c4",   0x180000, 0x80000, CRC(1bd081f8) SHA1(e5b0b5d8334486f813d7c430bb7fce3f69605a21) )
ROM_END

ROM_START( astormbl )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "astorm.a6", 0x000000, 0x40000, CRC(7682ed3e) SHA1(b857352ad9c66488e91f60989472638c483e4ae8) )
	ROM_LOAD16_BYTE( "astorm.a5", 0x000001, 0x40000, CRC(efe9711e) SHA1(496fd9e30941fde1658fab7292a669ef7964cecb) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "astorm.a11",   0x000000, 0x40000, CRC(7829c4f3) SHA1(3adb7aa7f70163d3848c98316e18b9783c41d663) )
	ROM_LOAD16_BYTE( "mpr13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x200000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13083.bin", 0x000000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) )
	ROM_LOAD( "epr13076.bin", 0x080000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr13077.bin", 0x100000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr13078.bin", 0x180000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

/* Bloxeed */
ROM_START( bloxeed )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-12911.a6", 0x000000, 0x20000, CRC(a481581a) SHA1(5ce5a0a082622919d2fe0e7d52ec807b2e2c25a2) ) /* adds (c) 1989 Elorg */
	ROM_LOAD16_BYTE( "epr-12910.a5", 0x000001, 0x20000, CRC(dd1bc3bf) SHA1(c0d79862a349ea4dac103c17325633c5dd4a93d1) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0139.key", 0x0000, 0x2000, CRC(9aae84cb) SHA1(806515d61ecacb260b2b5e5fe023b494d35ce315) )

	ROM_REGION( 0x30000, REGION_GFX1, 0 )
	ROM_LOAD( "opr-12884.b1", 0x00000, 0x10000, CRC(e024aa33) SHA1(d734be240cd05031aaadf9735c0b1b00e8e6d4cb) )
	ROM_LOAD( "opr-12885.b2", 0x10000, 0x10000, CRC(8041b814) SHA1(29fa49ba9a73eed07865a86ea774e2c6a60aed5b) )
	ROM_LOAD( "opr-12886.b3", 0x20000, 0x10000, CRC(de32285e) SHA1(8994dc128d6a23763e5fcfca1868b336d4aa0a21) )

	ROM_REGION16_BE( 0x20000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "opr-12891.a11", 0x00000, 0x10000, CRC(90d31a8c) SHA1(1747652a5109ce65add197cf06535f2463a99fdc) )
	ROM_LOAD16_BYTE( "opr-12887.b11", 0x00001, 0x10000, CRC(f0c0f49d) SHA1(7ecd591265165f3149241e2ceb5059faab88360f) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12888.a4", 0x000000, 0x20000, CRC(6f2fc63c) SHA1(3cce22c8f80013f05b5a2d36c42a61a81e4d6cbd) )
ROM_END

/* Clutch Hitter */
ROM_START( cltchitr )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13794.a4", 0x00000, 0x40000, CRC(c8d80233) SHA1(69cdbb989f580abbb006820347becf8d233fa773) )
	ROM_LOAD16_BYTE( "epr-13795.a6", 0x00001, 0x40000, CRC(b0b60b67) SHA1(ee3325ddb7461008f556c1696157a766225b9ef5) )
	ROM_LOAD16_BYTE( "epr-13784.a5", 0x80000, 0x40000, CRC(80c8180d) SHA1(80e72ab7d97714009fd31b3d50176af84b0dcdb7) )
	ROM_LOAD16_BYTE( "epr-13786.a7", 0x80001, 0x40000, CRC(3095dac0) SHA1(20edce74b6f2a82a3865613e601a0e212615d0e4) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0176.key", 0x0000, 0x2000, CRC(9b072430) SHA1(3bc1c7a6d71b4351a42d85e68e70715a7659c096) )

	ROM_REGION( 0x180000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13773.c1",  0x000000, 0x80000, CRC(3fc600e5) SHA1(8bec4ecf6a4e4b38d13133960036a5a4800a668e) )
	ROM_LOAD( "mpr-13774.c2",  0x080000, 0x80000, CRC(2411a824) SHA1(0e383ccc4e0830ffb395d5102e2950610c147007) )
	ROM_LOAD( "mpr-13775.c3",  0x100000, 0x80000, CRC(cf527bf6) SHA1(1f9cf1f0e92709f0465dc97ebbdaa715a419f7a7) )

	ROM_REGION16_BE( 0x600000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13779.c10", 0x000001, 0x80000, CRC(c707f416) SHA1(e6a9d89849f7f1c303a3ca29a629f81397945a2d) )
	ROM_LOAD16_BYTE( "mpr-13787.a10", 0x000000, 0x80000, CRC(f05c68c6) SHA1(b6a0535b6c734a0c89fdb6506c32ffe6ab3aa8cd) )
	ROM_LOAD16_BYTE( "mpr-13780.c11", 0x200001, 0x80000, CRC(a4c341e0) SHA1(15a0b5a42b56465a7b7df98968cc2ed177ce6f59) )
	ROM_LOAD16_BYTE( "mpr-13788.a11", 0x200000, 0x80000, CRC(0106fea6) SHA1(e16e2a469ecbbc704021dee6264db30aa0898368) )
	ROM_LOAD16_BYTE( "mpr-13781.c12", 0x400001, 0x80000, CRC(f33b13af) SHA1(d3eb64dcf12d532454bf3cd6c86528c0f11ee316) )
	ROM_LOAD16_BYTE( "mpr-13789.a12", 0x400000, 0x80000, CRC(09ba8835) SHA1(72e83dd1793a7f4b2b881e71f262493e3d4992b3) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13793.c7", 0x000000, 0x80000, CRC(a3d31944) SHA1(44d17aa0598eacfac4b12c8955fd1067ca09abbd) )
	ROM_LOAD( "epr-13792.c6", 0x080000, 0x80000, CRC(808f9695) SHA1(d50677d20083ad56dbf0864db05f76f93a4e9eba) )
	ROM_LOAD( "epr-13791.c5", 0x100000, 0x80000, CRC(35c16d80) SHA1(7ed04600748c5efb63e25f066daa163e9c0d8038) )
ROM_END

/* DD Crew USA 4 player using decrypted roms from ddcrewud*/
/* Laser Ghost */
ROM_START( ddcrew )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-14160.a4", 0x00000, 0x40000, CRC(b9f897b7) SHA1(65cee6c8006f328eee648e144e11fa60b1433ff5) )
	ROM_LOAD16_BYTE( "epr-14161.a6", 0x00001, 0x40000, CRC(bb03c1f0) SHA1(9e7fbd2cda448992c6cbf4b96078b57305def097) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0190.key", 0x0000, 0x2000, CRC(2d502b11) SHA1(c4e94da59b0e15a5a302ebe88988d1657e7e9814 ) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewu )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-14152.a4", 0x00000, 0x40000, CRC(69c7b571) SHA1(9fe4815a1cff0a46a754a6bdee12abaf7beb6501) )
	ROM_LOAD16_BYTE( "epr-14153.a6", 0x00001, 0x40000, CRC(e01fae0c) SHA1(7166f955324f73e94d8ae6d2a5b2f4b497e62933) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0186.key", 0x0000, 0x2000, CRC(7acaf1fd) SHA1(236d6382072adda8f7907d98d428fcca36700fa0) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrew2 )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-14148.a4", 0x00000, 0x40000, CRC(df4cb0cf) SHA1(153993997e9ceb06a9d4c73794ea66d0c585a291) )
	ROM_LOAD16_BYTE( "epr-14149.a6", 0x00001, 0x40000, CRC(380ff818) SHA1(7c7dd7aa825665f1a9aaebb5af4ecf5dd109b0ca) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0184.key", 0x0000, 0x2000, CRC(cee06254) SHA1(d64903055fdefb49c584cbcd84f0d4fa811bd789) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrew1 )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-14154.a4", 0x00000, 0x40000, CRC(9a0dadf0) SHA1(b55c8cdd3158607ec8203bfebc9e7aba24d6d565) )
	ROM_LOAD16_BYTE( "epr-14155.a6", 0x00001, 0x40000, CRC(e74362f4) SHA1(a6f96d714baeb826221b712b996e99831cf25abf) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0187.key", 0x0000, 0x2000, CRC(1dfb60be) SHA1(7bd42a2e54fca574076e5ed164ab4e0cbb645a4f) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-14150.a4", 0x00000, 0x40000, CRC(fc866b19) SHA1(1e59d23c25ac5e34c8982cb21e09d52cccc7aca7) )
	ROM_LOAD16_BYTE( "epr-14151.a6", 0x00001, 0x40000, CRC(46d23fe4) SHA1(926136db7a7d6d3bcdc552156e8cb4cb4224c2e5) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0185.key", 0x0000, 0x2000, CRC(a650b506) SHA1(05ab361114f26576d69c3477943b42f7957fb879) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewj2 )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-14138.a4", 0x00000, 0x40000, CRC(df280b1b) SHA1(581e8b6cbf3231d772de6b0e26b94541931215fd) )
	ROM_LOAD16_BYTE( "epr-14140.a6", 0x00001, 0x40000, CRC(48f223ee) SHA1(5192b92d081829d2a4fcf2258675b24c94cfb4e5) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0182.key", 0x0000, 0x2000, CRC(2e8a3601) SHA1(8b6e10babfd2398c1669ba6bf9aad61cd02f23ba) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewd )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14160.a4", 0x00000, 0x40000, CRC(22a6c8e3) SHA1(a4ed9b776b1c61fb6ac158534084152b364d214d) )
	ROM_LOAD16_BYTE( "bootleg_epr-14161.a6", 0x00001, 0x40000, CRC(d9eaba00) SHA1(39d88d7ea22dce3456185aa2c4f2440c798a42d8) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewud )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14152.a4", 0x00000, 0x40000, CRC(41b782d0) SHA1(b84ebbe65b9b2aed0599a3309ff5afbdd40e3c13) )
	ROM_LOAD16_BYTE( "bootleg_epr-14153.a6", 0x00001, 0x40000, CRC(48070486) SHA1(2cac94337eff256b05d614a960dc2c9cd707fe28) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrew2d )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14148.a4", 0x00000, 0x40000, CRC(f3ddb3b9) SHA1(493ad0330362e7244927ee36587fab9634925ee1) )
	ROM_LOAD16_BYTE( "bootleg_epr-14149.a6", 0x00001, 0x40000, CRC(98a83ee1) SHA1(33e37becccf63d6005e901d38418c003834e469f) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrew1d )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14154.a4", 0x00000, 0x40000, CRC(95749c77) SHA1(fdb924e0bb104326ba2e72e76b91d1fcc500aac0) )
	ROM_LOAD16_BYTE( "bootleg_epr-14155.a6", 0x00001, 0x40000, CRC(bef3c932) SHA1(f74f114aedafdace6ec51c4d3fa80a1b411b1543) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewjd )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14150.a4", 0x00000, 0x40000, CRC(1f267de5) SHA1(24b5683c427ea8a56906d8246807e6be2ec8dbe6) )
	ROM_LOAD16_BYTE( "bootleg_epr-14151.a6", 0x00001, 0x40000, CRC(35074062) SHA1(a9b657a550c652a62096d396f7eccf8a0888b482))
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( ddcrewj2d )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14138.a4", 0x00000, 0x40000, CRC(1a586bb2) SHA1(897f7b73a5099752980fe81a190cfac959481c1e) )
	ROM_LOAD16_BYTE( "bootleg_epr-14140.a6", 0x00001, 0x40000, CRC(43e030ae) SHA1(4c145709a726b2180801f65a0d68022ff6f0c619) )
	ROM_LOAD16_BYTE( "mpr-14139.a5", 0x80000, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )
	ROM_LOAD16_BYTE( "mpr-14141.a7", 0x80001, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )


	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-14127.c1", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "epr-14128.c2", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "epr-14129.c3", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14134.c10", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "mpr-14142.a10", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "mpr-14135.c11", 0x200001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "mpr-14143.a11", 0x200000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "mpr-14136.c12", 0x400001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "mpr-14144.a12", 0x400000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "epr-14137.c13", 0x600001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "epr-14145.a13", 0x600000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14133.c7", 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "mpr-14132.c6", 0x080000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
	ROM_LOAD( "mpr-14131.c5", 0x100000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "epr-14130.c4", 0x180000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
ROM_END

ROM_START( desertbr )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* 68000 code - custom CPU 317-0196 */
	ROM_LOAD16_BYTE( "epr-14802.a4", 0x000000, 0x80000, CRC(9ab93cbc) SHA1(a8d0013e17519c26c6ba7d28ec73e22ea5bde0e9) )
	ROM_LOAD16_BYTE( "epr-14902.a6", 0x000001, 0x80000, CRC(6724e7b1) SHA1(540c8bb7e848488dead81ca58f3bece45a87e611) )
	ROM_LOAD16_BYTE( "epr-14793.a5", 0x100000, 0x80000, CRC(dc9d7af3) SHA1(1fc1fedc1a4beed94cece268d0bb4bf62eeb407c) )
	ROM_LOAD16_BYTE( "epr-14795.a7", 0x100001, 0x80000, CRC(7e5bf7d9) SHA1(32ac68ee423a34e0f1bedc8765e03f40e01c3af1) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0196.key", 0x0000, 0x2000, CRC(cb942262) SHA1(7ad7cd3df887c6e6435d74784cb12ce016acd0da) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14781.c1", 0x000000, 0x100000, CRC(c4f7d7aa) SHA1(3c69dd7a26efccd7111ef33dfa6e8b738095c0bf) )
	ROM_LOAD( "mpr-14782.c2", 0x100000, 0x100000, CRC(ccc98d05) SHA1(b89594bbfff45e3b4fe433aeeaf8b4073c2cabb5) )
	ROM_LOAD( "mpr-14783.c3", 0x200000, 0x100000, CRC(ef202bec) SHA1(b557092f8a3e1c9889d34588344c6dd2c6f06731) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14788.c10", 0x000001, 0x100000, CRC(b5b05536) SHA1(f8fde7ebca38c0a6f6a864c17771aa155e6fc30d) )
	ROM_LOAD16_BYTE( "mpr-14796.a10", 0x000000, 0x100000, CRC(c033220a) SHA1(279d3ef62b41d2c6a18ce1217a549402a874638b) )
	ROM_LOAD16_BYTE( "mpr-14789.c11", 0x200001, 0x100000, CRC(0f9bcb97) SHA1(c15ab3ece596c54e1c4d8e8a755473609334e8ba) )
	ROM_LOAD16_BYTE( "mpr-14797.a11", 0x200000, 0x100000, CRC(4c301cc9) SHA1(8aea8af0b078b81d1054331b273568b1b903531b) )
	ROM_LOAD16_BYTE( "mpr-14790.c12", 0x400001, 0x100000, CRC(6a07ac27) SHA1(0558b662c7965c5b74cbc552423194a8facbc092) )
	ROM_LOAD16_BYTE( "mpr-14798.a12", 0x400000, 0x100000, CRC(50634625) SHA1(f4baaebdb1f850e92ca865e103fbca68cdb0de0f) )
	ROM_LOAD16_BYTE( "mpr-14791.c13", 0x600001, 0x100000, CRC(a4ae352b) SHA1(dc814e1c2e167e191cd43fa554ff8ee974d47152) )
	ROM_LOAD16_BYTE( "mpr-14799.a13", 0x600000, 0x100000, CRC(aeb7b025) SHA1(9ce2a9a46176a110c8d2e77deb3d8b9b69b902fa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14787.c7", 0x000000, 0x40000, CRC(cc6feec7) SHA1(31cc243178b98681a52500a485d74ed9e1274888) )
	ROM_LOAD( "mpr-14786.c6", 0x080000, 0x80000, CRC(cc8349f2) SHA1(9f00d8ea372b70ba44a90dab497deadcc5be3dab) )
	ROM_LOAD( "mpr-14785.c5", 0x100000, 0x80000, CRC(7babba13) SHA1(190cd9ea0f73272e0df34bbdfd8e0035f9e9b0b0) )
	ROM_LOAD( "mpr-14784.c4", 0x180000, 0x80000, CRC(073878e4) SHA1(eff08080d06a16fccf4876e42b389fef599cceba) )
ROM_END

ROM_START( desertbrj )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* 68000 code - custom CPU 317-0196 */
	ROM_LOAD16_BYTE( "epr-14792.a4", 0x000000, 0x80000, CRC(453d9d02) SHA1(df5d85c2e1a25c18860aa96462ea4893bb633190) )
	ROM_LOAD16_BYTE( "epr-14794.a6", 0x000001, 0x80000, CRC(4426758f) SHA1(d8bebcf05a83d34a93f74e2263f4290e995656ba) )
	ROM_LOAD16_BYTE( "epr-14793.a5", 0x100000, 0x80000, CRC(dc9d7af3) SHA1(1fc1fedc1a4beed94cece268d0bb4bf62eeb407c) )
	ROM_LOAD16_BYTE( "epr-14795.a7", 0x100001, 0x80000, CRC(7e5bf7d9) SHA1(32ac68ee423a34e0f1bedc8765e03f40e01c3af1) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0194.key", 0x0000, 0x2000, CRC(40cbc4cb) SHA1(51777d8a619268ac0b1fda6e7781cde753354419) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14781.c1", 0x000000, 0x100000, CRC(c4f7d7aa) SHA1(3c69dd7a26efccd7111ef33dfa6e8b738095c0bf) )
	ROM_LOAD( "mpr-14782.c2", 0x100000, 0x100000, CRC(ccc98d05) SHA1(b89594bbfff45e3b4fe433aeeaf8b4073c2cabb5) )
	ROM_LOAD( "mpr-14783.c3", 0x200000, 0x100000, CRC(ef202bec) SHA1(b557092f8a3e1c9889d34588344c6dd2c6f06731) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14788.c10", 0x000001, 0x100000, CRC(b5b05536) SHA1(f8fde7ebca38c0a6f6a864c17771aa155e6fc30d) )
	ROM_LOAD16_BYTE( "mpr-14796.a10", 0x000000, 0x100000, CRC(c033220a) SHA1(279d3ef62b41d2c6a18ce1217a549402a874638b) )
	ROM_LOAD16_BYTE( "mpr-14789.c11", 0x200001, 0x100000, CRC(0f9bcb97) SHA1(c15ab3ece596c54e1c4d8e8a755473609334e8ba) )
	ROM_LOAD16_BYTE( "mpr-14797.a11", 0x200000, 0x100000, CRC(4c301cc9) SHA1(8aea8af0b078b81d1054331b273568b1b903531b) )
	ROM_LOAD16_BYTE( "mpr-14790.c12", 0x400001, 0x100000, CRC(6a07ac27) SHA1(0558b662c7965c5b74cbc552423194a8facbc092) )
	ROM_LOAD16_BYTE( "mpr-14798.a12", 0x400000, 0x100000, CRC(50634625) SHA1(f4baaebdb1f850e92ca865e103fbca68cdb0de0f) )
	ROM_LOAD16_BYTE( "mpr-14791.c13", 0x600001, 0x100000, CRC(a4ae352b) SHA1(dc814e1c2e167e191cd43fa554ff8ee974d47152) )
	ROM_LOAD16_BYTE( "mpr-14799.a13", 0x600000, 0x100000, CRC(aeb7b025) SHA1(9ce2a9a46176a110c8d2e77deb3d8b9b69b902fa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14787.c7", 0x000000, 0x40000, CRC(cc6feec7) SHA1(31cc243178b98681a52500a485d74ed9e1274888) )
	ROM_LOAD( "mpr-14786.c6", 0x080000, 0x80000, CRC(cc8349f2) SHA1(9f00d8ea372b70ba44a90dab497deadcc5be3dab) )
	ROM_LOAD( "mpr-14785.c5", 0x100000, 0x80000, CRC(7babba13) SHA1(190cd9ea0f73272e0df34bbdfd8e0035f9e9b0b0) )
	ROM_LOAD( "mpr-14784.c4", 0x180000, 0x80000, CRC(073878e4) SHA1(eff08080d06a16fccf4876e42b389fef599cceba) )
ROM_END

ROM_START( desertbrd )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-14802.a4", 0x000000, 0x80000, CRC(af74af1c) SHA1(42048fc8042816886a5f7e05f195ee9ee9af9c04) )
	ROM_LOAD16_BYTE( "bootleg_epr-14902.a6", 0x000001, 0x80000, CRC(c883404b) SHA1(e11bb91fc1280158d3b524d598ee87d72e6efe03) )
	ROM_LOAD16_BYTE( "epr-14793.a5", 0x100000, 0x80000, CRC(dc9d7af3) SHA1(1fc1fedc1a4beed94cece268d0bb4bf62eeb407c) )
	ROM_LOAD16_BYTE( "epr-14795.a7", 0x100001, 0x80000, CRC(7e5bf7d9) SHA1(32ac68ee423a34e0f1bedc8765e03f40e01c3af1) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14781.c1", 0x000000, 0x100000, CRC(c4f7d7aa) SHA1(3c69dd7a26efccd7111ef33dfa6e8b738095c0bf) )
	ROM_LOAD( "mpr-14782.c2", 0x100000, 0x100000, CRC(ccc98d05) SHA1(b89594bbfff45e3b4fe433aeeaf8b4073c2cabb5) )
	ROM_LOAD( "mpr-14783.c3", 0x200000, 0x100000, CRC(ef202bec) SHA1(b557092f8a3e1c9889d34588344c6dd2c6f06731) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14788.c10", 0x000001, 0x100000, CRC(b5b05536) SHA1(f8fde7ebca38c0a6f6a864c17771aa155e6fc30d) )
	ROM_LOAD16_BYTE( "mpr-14796.a10", 0x000000, 0x100000, CRC(c033220a) SHA1(279d3ef62b41d2c6a18ce1217a549402a874638b) )
	ROM_LOAD16_BYTE( "mpr-14789.c11", 0x200001, 0x100000, CRC(0f9bcb97) SHA1(c15ab3ece596c54e1c4d8e8a755473609334e8ba) )
	ROM_LOAD16_BYTE( "mpr-14797.a11", 0x200000, 0x100000, CRC(4c301cc9) SHA1(8aea8af0b078b81d1054331b273568b1b903531b) )
	ROM_LOAD16_BYTE( "mpr-14790.c12", 0x400001, 0x100000, CRC(6a07ac27) SHA1(0558b662c7965c5b74cbc552423194a8facbc092) )
	ROM_LOAD16_BYTE( "mpr-14798.a12", 0x400000, 0x100000, CRC(50634625) SHA1(f4baaebdb1f850e92ca865e103fbca68cdb0de0f) )
	ROM_LOAD16_BYTE( "mpr-14791.c13", 0x600001, 0x100000, CRC(a4ae352b) SHA1(dc814e1c2e167e191cd43fa554ff8ee974d47152) )
	ROM_LOAD16_BYTE( "mpr-14799.a13", 0x600000, 0x100000, CRC(aeb7b025) SHA1(9ce2a9a46176a110c8d2e77deb3d8b9b69b902fa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14787.c7", 0x000000, 0x40000, CRC(cc6feec7) SHA1(31cc243178b98681a52500a485d74ed9e1274888) )
	ROM_LOAD( "mpr-14786.c6", 0x080000, 0x80000, CRC(cc8349f2) SHA1(9f00d8ea372b70ba44a90dab497deadcc5be3dab) )
	ROM_LOAD( "mpr-14785.c5", 0x100000, 0x80000, CRC(7babba13) SHA1(190cd9ea0f73272e0df34bbdfd8e0035f9e9b0b0) )
	ROM_LOAD( "mpr-14784.c4", 0x180000, 0x80000, CRC(073878e4) SHA1(eff08080d06a16fccf4876e42b389fef599cceba) )
ROM_END

ROM_START( desertbrjd )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-14792.a4", 0x000000, 0x80000, CRC(fd8ed26d) SHA1(db5a9a68a05d03db7a1fe60a281310a0d38a86ed) )
	ROM_LOAD16_BYTE( "bootleg_epr-14794.a6", 0x000001, 0x80000, CRC(399f167d) SHA1(ee2d8cb1f6725b38f038e0021317e11b79ce81f0) )
	ROM_LOAD16_BYTE( "epr-14793.a5", 0x100000, 0x80000, CRC(dc9d7af3) SHA1(1fc1fedc1a4beed94cece268d0bb4bf62eeb407c) )
	ROM_LOAD16_BYTE( "epr-14795.a7", 0x100001, 0x80000, CRC(7e5bf7d9) SHA1(32ac68ee423a34e0f1bedc8765e03f40e01c3af1) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-14781.c1", 0x000000, 0x100000, CRC(c4f7d7aa) SHA1(3c69dd7a26efccd7111ef33dfa6e8b738095c0bf) )
	ROM_LOAD( "mpr-14782.c2", 0x100000, 0x100000, CRC(ccc98d05) SHA1(b89594bbfff45e3b4fe433aeeaf8b4073c2cabb5) )
	ROM_LOAD( "mpr-14783.c3", 0x200000, 0x100000, CRC(ef202bec) SHA1(b557092f8a3e1c9889d34588344c6dd2c6f06731) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-14788.c10", 0x000001, 0x100000, CRC(b5b05536) SHA1(f8fde7ebca38c0a6f6a864c17771aa155e6fc30d) )
	ROM_LOAD16_BYTE( "mpr-14796.a10", 0x000000, 0x100000, CRC(c033220a) SHA1(279d3ef62b41d2c6a18ce1217a549402a874638b) )
	ROM_LOAD16_BYTE( "mpr-14789.c11", 0x200001, 0x100000, CRC(0f9bcb97) SHA1(c15ab3ece596c54e1c4d8e8a755473609334e8ba) )
	ROM_LOAD16_BYTE( "mpr-14797.a11", 0x200000, 0x100000, CRC(4c301cc9) SHA1(8aea8af0b078b81d1054331b273568b1b903531b) )
	ROM_LOAD16_BYTE( "mpr-14790.c12", 0x400001, 0x100000, CRC(6a07ac27) SHA1(0558b662c7965c5b74cbc552423194a8facbc092) )
	ROM_LOAD16_BYTE( "mpr-14798.a12", 0x400000, 0x100000, CRC(50634625) SHA1(f4baaebdb1f850e92ca865e103fbca68cdb0de0f) )
	ROM_LOAD16_BYTE( "mpr-14791.c13", 0x600001, 0x100000, CRC(a4ae352b) SHA1(dc814e1c2e167e191cd43fa554ff8ee974d47152) )
	ROM_LOAD16_BYTE( "mpr-14799.a13", 0x600000, 0x100000, CRC(aeb7b025) SHA1(9ce2a9a46176a110c8d2e77deb3d8b9b69b902fa) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-14787.c7", 0x000000, 0x40000, CRC(cc6feec7) SHA1(31cc243178b98681a52500a485d74ed9e1274888) )
	ROM_LOAD( "mpr-14786.c6", 0x080000, 0x80000, CRC(cc8349f2) SHA1(9f00d8ea372b70ba44a90dab497deadcc5be3dab) )
	ROM_LOAD( "mpr-14785.c5", 0x100000, 0x80000, CRC(7babba13) SHA1(190cd9ea0f73272e0df34bbdfd8e0035f9e9b0b0) )
	ROM_LOAD( "mpr-14784.c4", 0x180000, 0x80000, CRC(073878e4) SHA1(eff08080d06a16fccf4876e42b389fef599cceba) )
ROM_END

ROM_START( lghost )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13429.a4", 0x00000, 0x40000, CRC(09bd65c0) SHA1(f2b332a86d52af3c5270f668bdd43a0d44eca346) )
	ROM_LOAD16_BYTE( "epr-13430.a6", 0x00001, 0x40000, CRC(51009fe0) SHA1(f1e6e3714c01c15c0e932470a9e1a17abb59e958) )
	ROM_LOAD16_BYTE( "epr-13411.a5", 0x80000, 0x40000, CRC(5160167b) SHA1(3d176a18c7527b1e485f10b144bb4db1b945e709) )
	ROM_LOAD16_BYTE( "epr-13413.a7", 0x80001, 0x40000, CRC(656b3bd8) SHA1(db81d4ae3138308dce1e3db7a859f1d63c4ff815) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0166.key", 0x0000, 0x2000, CRC(8379961f) SHA1(44e0662e92ece65ad2049b2cd804f516e631166e) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13414.c1", 0x00000, 0x40000, CRC(dada2419) SHA1(f6ffd02d75232a09ea83fd199e5e30b2773b0cf5) )
	ROM_LOAD( "epr-13415.c2", 0x40000, 0x40000, CRC(bbb62c48) SHA1(7a4c5bd11b73a92deece72b55627f48ac167acd6) )
	ROM_LOAD( "epr-13416.c3", 0x80000, 0x40000, CRC(1d11dbae) SHA1(331aa49c6b38d32ec33184dbd0851888463ddbc7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "epr-13603.a10", 0x000000, 0x80000, CRC(5350a94e) SHA1(47e99803cab4b508feb51069c940d6c824d6961d) )
	ROM_LOAD16_BYTE( "epr-13604.c10", 0x000001, 0x80000, CRC(4009c8e5) SHA1(97f513d312f4c90f8bffdf797afa3749779989a5) )
	ROM_LOAD16_BYTE( "mpr-13421.a11", 0x200000, 0x80000, CRC(2fc75890) SHA1(9f97f07dba3b978df8eb357894168ad74f151d30) )
	ROM_LOAD16_BYTE( "mpr-13424.c11", 0x200001, 0x80000, CRC(fb98d920) SHA1(cebdebe88902e96c631df6053ac2589f794da155) )
	ROM_LOAD16_BYTE( "mpr-13422.a12", 0x400000, 0x80000, CRC(48a0754d) SHA1(9fead9f8319593adb4bddaaa4d053b21ca726009) )
	ROM_LOAD16_BYTE( "mpr-13425.c12", 0x400001, 0x80000, CRC(f8252589) SHA1(5a1ed24296d0609393e53df3ee585a366da4ee46) )
	ROM_LOAD16_BYTE( "mpr-13423.a13", 0x600000, 0x80000, CRC(335bbc9d) SHA1(78793335b2f8a1bb05809259521db193c17c9b98) )
	ROM_LOAD16_BYTE( "mpr-13426.c13", 0x600001, 0x80000, CRC(5cfb1e25) SHA1(1dd57475604f339e58bf946e17ae0dc5cf4a3dba) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13417.c7",   0x000000, 0x20000, CRC(cd7beb49) SHA1(2435ce000f1eefdd06b27ea93e22fd82c0e999d2) )
	ROM_LOAD( "mpr-13420.c6",   0x080000, 0x40000, CRC(3de0dee4) SHA1(31833684df5a34d5e9ef04f2ab42355b8e9cbb45) )
	ROM_LOAD( "mpr-13419.c5",   0x100000, 0x40000, CRC(e7021b0a) SHA1(82e390fac63965d4f80ae01758c19ae951c39475) )
	ROM_LOAD( "mpr-13418.c4",   0x180000, 0x40000, CRC(0732594d) SHA1(9fbeae29f1a31d136ddc9a49c786b2a08a523e0d) )
ROM_END

ROM_START( lghostu )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13427.a4",  0x00000, 0x40000, CRC(5bf8fb6b) SHA1(587bbf45b5e470da7d9166b2cbf4ac58a1f2a825) )
	ROM_LOAD16_BYTE( "epr-13428.a6",  0x00001, 0x40000, CRC(276775f5) SHA1(5dd5dabe7e9311b3520d0405dda0c983e9bc4ba2) )
	ROM_LOAD16_BYTE( "epr-13411.a5",  0x80000, 0x40000, CRC(5160167b) SHA1(3d176a18c7527b1e485f10b144bb4db1b945e709) )
	ROM_LOAD16_BYTE( "epr-13413.a7",  0x80001, 0x40000, CRC(656b3bd8) SHA1(db81d4ae3138308dce1e3db7a859f1d63c4ff815) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0165.key", 0x0000, 0x2000,  CRC(a04267ab) SHA1(688ee59dfaaf240e23de4cada648689d1717ab04) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13414.c1", 0x00000, 0x40000, CRC(dada2419) SHA1(f6ffd02d75232a09ea83fd199e5e30b2773b0cf5) )
	ROM_LOAD( "epr-13415.c2", 0x40000, 0x40000, CRC(bbb62c48) SHA1(7a4c5bd11b73a92deece72b55627f48ac167acd6) )
	ROM_LOAD( "epr-13416.c3", 0x80000, 0x40000, CRC(1d11dbae) SHA1(331aa49c6b38d32ec33184dbd0851888463ddbc7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "epr-13603.a10", 0x000000, 0x80000, CRC(5350a94e) SHA1(47e99803cab4b508feb51069c940d6c824d6961d) )
	ROM_LOAD16_BYTE( "epr-13604.c10", 0x000001, 0x80000, CRC(4009c8e5) SHA1(97f513d312f4c90f8bffdf797afa3749779989a5) )
	ROM_LOAD16_BYTE( "mpr-13421.a11", 0x200000, 0x80000, CRC(2fc75890) SHA1(9f97f07dba3b978df8eb357894168ad74f151d30) )
	ROM_LOAD16_BYTE( "mpr-13424.c11", 0x200001, 0x80000, CRC(fb98d920) SHA1(cebdebe88902e96c631df6053ac2589f794da155) )
	ROM_LOAD16_BYTE( "mpr-13422.a12", 0x400000, 0x80000, CRC(48a0754d) SHA1(9fead9f8319593adb4bddaaa4d053b21ca726009) )
	ROM_LOAD16_BYTE( "mpr-13425.c12", 0x400001, 0x80000, CRC(f8252589) SHA1(5a1ed24296d0609393e53df3ee585a366da4ee46) )
	ROM_LOAD16_BYTE( "mpr-13423.a13", 0x600000, 0x80000, CRC(335bbc9d) SHA1(78793335b2f8a1bb05809259521db193c17c9b98) )
	ROM_LOAD16_BYTE( "mpr-13426.c13", 0x600001, 0x80000, CRC(5cfb1e25) SHA1(1dd57475604f339e58bf946e17ae0dc5cf4a3dba) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13417.c7",   0x000000, 0x20000, CRC(cd7beb49) SHA1(2435ce000f1eefdd06b27ea93e22fd82c0e999d2) )
	ROM_LOAD( "mpr-13420.c6",   0x080000, 0x40000, CRC(3de0dee4) SHA1(31833684df5a34d5e9ef04f2ab42355b8e9cbb45) )
	ROM_LOAD( "mpr-13419.c5",   0x100000, 0x40000, CRC(e7021b0a) SHA1(82e390fac63965d4f80ae01758c19ae951c39475) )
	ROM_LOAD( "mpr-13418.c4",   0x180000, 0x40000, CRC(0732594d) SHA1(9fbeae29f1a31d136ddc9a49c786b2a08a523e0d) )
ROM_END

ROM_START( lghostj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-13410.a4",  0x00000, 0x40000, CRC(9c1d672b) SHA1(d617b1fbdad1b42af6f1341e4556c3481c4a7031) )
	ROM_LOAD16_BYTE( "epr-13412.a6",  0x00001, 0x40000, CRC(3a3155fa) SHA1(534231450d4da0668e8742ada8c9c86f2e5768a6))
	ROM_LOAD16_BYTE( "epr-13411.a5",  0x80000, 0x40000, CRC(5160167b) SHA1(3d176a18c7527b1e485f10b144bb4db1b945e709) )
	ROM_LOAD16_BYTE( "epr-13413.a7",  0x80001, 0x40000, CRC(656b3bd8) SHA1(db81d4ae3138308dce1e3db7a859f1d63c4ff815) )

	ROM_REGION( 0x2000, REGION_USER1, 0 ) /* decryption key */
	ROM_LOAD( "317-0164.key", 0x0000, 0x2000,  CRC(6bf5cf2d) SHA1(9f43b057ac162a62a5f3b5511b0825f34a2a31a3))

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13414.c1", 0x00000, 0x40000, CRC(dada2419) SHA1(f6ffd02d75232a09ea83fd199e5e30b2773b0cf5) )
	ROM_LOAD( "epr-13415.c2", 0x40000, 0x40000, CRC(bbb62c48) SHA1(7a4c5bd11b73a92deece72b55627f48ac167acd6) )
	ROM_LOAD( "epr-13416.c3", 0x80000, 0x40000, CRC(1d11dbae) SHA1(331aa49c6b38d32ec33184dbd0851888463ddbc7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "epr-13603.a10", 0x000000, 0x80000, CRC(5350a94e) SHA1(47e99803cab4b508feb51069c940d6c824d6961d) )
	ROM_LOAD16_BYTE( "epr-13604.c10", 0x000001, 0x80000, CRC(4009c8e5) SHA1(97f513d312f4c90f8bffdf797afa3749779989a5) )
	ROM_LOAD16_BYTE( "mpr-13421.a11", 0x200000, 0x80000, CRC(2fc75890) SHA1(9f97f07dba3b978df8eb357894168ad74f151d30) )
	ROM_LOAD16_BYTE( "mpr-13424.c11", 0x200001, 0x80000, CRC(fb98d920) SHA1(cebdebe88902e96c631df6053ac2589f794da155) )
	ROM_LOAD16_BYTE( "mpr-13422.a12", 0x400000, 0x80000, CRC(48a0754d) SHA1(9fead9f8319593adb4bddaaa4d053b21ca726009) )
	ROM_LOAD16_BYTE( "mpr-13425.c12", 0x400001, 0x80000, CRC(f8252589) SHA1(5a1ed24296d0609393e53df3ee585a366da4ee46) )
	ROM_LOAD16_BYTE( "mpr-13423.a13", 0x600000, 0x80000, CRC(335bbc9d) SHA1(78793335b2f8a1bb05809259521db193c17c9b98) )
	ROM_LOAD16_BYTE( "mpr-13426.c13", 0x600001, 0x80000, CRC(5cfb1e25) SHA1(1dd57475604f339e58bf946e17ae0dc5cf4a3dba) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13417.c7",   0x000000, 0x20000, CRC(cd7beb49) SHA1(2435ce000f1eefdd06b27ea93e22fd82c0e999d2) )
	ROM_LOAD( "mpr-13420.c6",   0x080000, 0x40000, CRC(3de0dee4) SHA1(31833684df5a34d5e9ef04f2ab42355b8e9cbb45) )
	ROM_LOAD( "mpr-13419.c5",   0x100000, 0x40000, CRC(e7021b0a) SHA1(82e390fac63965d4f80ae01758c19ae951c39475) )
	ROM_LOAD( "mpr-13418.c4",   0x180000, 0x40000, CRC(0732594d) SHA1(9fbeae29f1a31d136ddc9a49c786b2a08a523e0d) )
ROM_END

ROM_START( lghostd )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13429.a4", 0x00000, 0x40000, CRC(3d641671) SHA1(68941985c19732b073363fa1eb3848b78e31bc05) )
	ROM_LOAD16_BYTE( "bootleg_epr-13430.a6", 0x00001, 0x40000, CRC(f0ba14be) SHA1(a771ec7ca9f7087ed990d6565cce5c15d08766a4) )
	ROM_LOAD16_BYTE( "epr-13411.a5", 0x80000, 0x40000, CRC(5160167b) SHA1(3d176a18c7527b1e485f10b144bb4db1b945e709) )
	ROM_LOAD16_BYTE( "epr-13413.a7", 0x80001, 0x40000, CRC(656b3bd8) SHA1(db81d4ae3138308dce1e3db7a859f1d63c4ff815) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13414.c1", 0x00000, 0x40000, CRC(dada2419) SHA1(f6ffd02d75232a09ea83fd199e5e30b2773b0cf5) )
	ROM_LOAD( "epr-13415.c2", 0x40000, 0x40000, CRC(bbb62c48) SHA1(7a4c5bd11b73a92deece72b55627f48ac167acd6) )
	ROM_LOAD( "epr-13416.c3", 0x80000, 0x40000, CRC(1d11dbae) SHA1(331aa49c6b38d32ec33184dbd0851888463ddbc7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "epr-13603.a10", 0x000000, 0x80000, CRC(5350a94e) SHA1(47e99803cab4b508feb51069c940d6c824d6961d) )
	ROM_LOAD16_BYTE( "epr-13604.c10", 0x000001, 0x80000, CRC(4009c8e5) SHA1(97f513d312f4c90f8bffdf797afa3749779989a5) )
	ROM_LOAD16_BYTE( "mpr-13421.a11", 0x200000, 0x80000, CRC(2fc75890) SHA1(9f97f07dba3b978df8eb357894168ad74f151d30) )
	ROM_LOAD16_BYTE( "mpr-13424.c11", 0x200001, 0x80000, CRC(fb98d920) SHA1(cebdebe88902e96c631df6053ac2589f794da155) )
	ROM_LOAD16_BYTE( "mpr-13422.a12", 0x400000, 0x80000, CRC(48a0754d) SHA1(9fead9f8319593adb4bddaaa4d053b21ca726009) )
	ROM_LOAD16_BYTE( "mpr-13425.c12", 0x400001, 0x80000, CRC(f8252589) SHA1(5a1ed24296d0609393e53df3ee585a366da4ee46) )
	ROM_LOAD16_BYTE( "mpr-13423.a13", 0x600000, 0x80000, CRC(335bbc9d) SHA1(78793335b2f8a1bb05809259521db193c17c9b98) )
	ROM_LOAD16_BYTE( "mpr-13426.c13", 0x600001, 0x80000, CRC(5cfb1e25) SHA1(1dd57475604f339e58bf946e17ae0dc5cf4a3dba) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13417.c7",   0x000000, 0x20000, CRC(cd7beb49) SHA1(2435ce000f1eefdd06b27ea93e22fd82c0e999d2) )
	ROM_LOAD( "mpr-13420.c6",   0x080000, 0x40000, CRC(3de0dee4) SHA1(31833684df5a34d5e9ef04f2ab42355b8e9cbb45) )
	ROM_LOAD( "mpr-13419.c5",   0x100000, 0x40000, CRC(e7021b0a) SHA1(82e390fac63965d4f80ae01758c19ae951c39475) )
	ROM_LOAD( "mpr-13418.c4",   0x180000, 0x40000, CRC(0732594d) SHA1(9fbeae29f1a31d136ddc9a49c786b2a08a523e0d) )
ROM_END

ROM_START( lghostud )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13427.a4",  0x00000, 0x40000, CRC(8e9e092e) SHA1(3751a5d74d729f72d34b881bede34b6029ab7f95) )
	ROM_LOAD16_BYTE( "bootleg_epr-13428.a6",  0x00001, 0x40000, CRC(346d4ba3) SHA1(d020bc1456843abc3a4132ea212c3e8140bfd4d2) )
	ROM_LOAD16_BYTE( "epr-13411.a5",  0x80000, 0x40000, CRC(5160167b) SHA1(3d176a18c7527b1e485f10b144bb4db1b945e709) )
	ROM_LOAD16_BYTE( "epr-13413.a7",  0x80001, 0x40000, CRC(656b3bd8) SHA1(db81d4ae3138308dce1e3db7a859f1d63c4ff815) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "epr-13414.c1", 0x00000, 0x40000, CRC(dada2419) SHA1(f6ffd02d75232a09ea83fd199e5e30b2773b0cf5) )
	ROM_LOAD( "epr-13415.c2", 0x40000, 0x40000, CRC(bbb62c48) SHA1(7a4c5bd11b73a92deece72b55627f48ac167acd6) )
	ROM_LOAD( "epr-13416.c3", 0x80000, 0x40000, CRC(1d11dbae) SHA1(331aa49c6b38d32ec33184dbd0851888463ddbc7) )

	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "epr-13603.a10", 0x000000, 0x80000, CRC(5350a94e) SHA1(47e99803cab4b508feb51069c940d6c824d6961d) )
	ROM_LOAD16_BYTE( "epr-13604.c10", 0x000001, 0x80000, CRC(4009c8e5) SHA1(97f513d312f4c90f8bffdf797afa3749779989a5) )
	ROM_LOAD16_BYTE( "mpr-13421.a11", 0x200000, 0x80000, CRC(2fc75890) SHA1(9f97f07dba3b978df8eb357894168ad74f151d30) )
	ROM_LOAD16_BYTE( "mpr-13424.c11", 0x200001, 0x80000, CRC(fb98d920) SHA1(cebdebe88902e96c631df6053ac2589f794da155) )
	ROM_LOAD16_BYTE( "mpr-13422.a12", 0x400000, 0x80000, CRC(48a0754d) SHA1(9fead9f8319593adb4bddaaa4d053b21ca726009) )
	ROM_LOAD16_BYTE( "mpr-13425.c12", 0x400001, 0x80000, CRC(f8252589) SHA1(5a1ed24296d0609393e53df3ee585a366da4ee46) )
	ROM_LOAD16_BYTE( "mpr-13423.a13", 0x600000, 0x80000, CRC(335bbc9d) SHA1(78793335b2f8a1bb05809259521db193c17c9b98) )
	ROM_LOAD16_BYTE( "mpr-13426.c13", 0x600001, 0x80000, CRC(5cfb1e25) SHA1(1dd57475604f339e58bf946e17ae0dc5cf4a3dba) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13417.c7",   0x000000, 0x20000, CRC(cd7beb49) SHA1(2435ce000f1eefdd06b27ea93e22fd82c0e999d2) )
	ROM_LOAD( "mpr-13420.c6",   0x080000, 0x40000, CRC(3de0dee4) SHA1(31833684df5a34d5e9ef04f2ab42355b8e9cbb45) )
	ROM_LOAD( "mpr-13419.c5",   0x100000, 0x40000, CRC(e7021b0a) SHA1(82e390fac63965d4f80ae01758c19ae951c39475) )
	ROM_LOAD( "mpr-13418.c4",   0x180000, 0x40000, CRC(0732594d) SHA1(9fbeae29f1a31d136ddc9a49c786b2a08a523e0d) )
ROM_END

ROM_START( shdancer )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-12774b.a6",  0x000000, 0x40000, CRC(3d5b3fa9) SHA1(370dd6e8ab9fb9e77eee9262d13fbdb4cf575abc) )
	ROM_LOAD16_BYTE( "epr-12773b.a5",  0x000001, 0x40000, CRC(2596004e) SHA1(1b993aa74e7559f7e99253fd2144db9449c04cce) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-12712.b1",  0x00000, 0x40000, CRC(9bdabe3d) SHA1(4bb30fa2d4cdefe4a864cef7153b516bc5b02c42) )
	ROM_LOAD( "mpr-12713.b2",  0x40000, 0x40000, CRC(852d2b1c) SHA1(8e5bc83d45e48b621ea3016207f2028fe41701e6) )
	ROM_LOAD( "mpr-12714.b3",  0x80000, 0x40000, CRC(448226ce) SHA1(3060e4a43311069e2691d659c1e0c1a48edfeedb) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-12719.b11", 0x000001, 0x40000, CRC(d6888534) SHA1(2201f1921a68cf39e5a94b487c90e48d032d630f) )
	ROM_LOAD16_BYTE( "mpr-12726.a11", 0x000000, 0x40000, CRC(ff344945) SHA1(2743778c42f53321f9691d60bbf94ea8baf1382f) )
	ROM_LOAD16_BYTE( "mpr-12718.b10", 0x080001, 0x40000, CRC(ba2efc0c) SHA1(459a1a280f870c94aefb70127ed007cb090ed203) )
	ROM_LOAD16_BYTE( "mpr-12725.a10", 0x080000, 0x40000, CRC(268a0c17) SHA1(2756054fa3c3aed30a1fce5e41acb0ceaebe90b5) )
	ROM_LOAD16_BYTE( "mpr-12717.b9",  0x100001, 0x40000, CRC(c81cc4f8) SHA1(22f364e85057ceef533e051c8d0755b9691c5ec4) )
	ROM_LOAD16_BYTE( "mpr-12724.a9",  0x100000, 0x40000, CRC(0f4903dc) SHA1(851bd60e877c9e39be23dc1fde91efc9da513c29) )
	ROM_LOAD16_BYTE( "epr-12716.b8",  0x180001, 0x40000, CRC(a870e629) SHA1(29f6633240f9737ec19e16100decc7aa045b2060) )
	ROM_LOAD16_BYTE( "epr-12723.a8",  0x180000, 0x40000, CRC(c606cf90) SHA1(cb53ae9a6da1eb31c584173d1fbbd1c8539fb54c) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12987.a4",  0x00000, 0x20000, CRC(d1c020cc) SHA1(7823e31fc44180570ee2512a73e993533204b5ab) )
	ROM_LOAD( "mpr-12715.b4",  0x80000, 0x40000, CRC(07051a52) SHA1(d48658497f4a34665d3e051f893ff057c38925ae) )
ROM_END

ROM_START( shdancerj )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-12722b.a6",  0x000000, 0x40000, CRC(c00552a2) SHA1(74fddfe596bc00bc11c4a06e2103417e8fd334f6) )
	ROM_LOAD16_BYTE( "epr-12721b.a5",  0x000001, 0x40000, CRC(653d351a) SHA1(1a03a154cb81a5a2f28c38aecdd6b5d107ea7ffa) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-12712.b1",  0x00000, 0x40000, CRC(9bdabe3d) SHA1(4bb30fa2d4cdefe4a864cef7153b516bc5b02c42) )
	ROM_LOAD( "mpr-12713.b2",  0x40000, 0x40000, CRC(852d2b1c) SHA1(8e5bc83d45e48b621ea3016207f2028fe41701e6) )
	ROM_LOAD( "mpr-12714.b3",  0x80000, 0x40000, CRC(448226ce) SHA1(3060e4a43311069e2691d659c1e0c1a48edfeedb) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-12719.b11", 0x000001, 0x40000, CRC(d6888534) SHA1(2201f1921a68cf39e5a94b487c90e48d032d630f) )
	ROM_LOAD16_BYTE( "mpr-12726.a11", 0x000000, 0x40000, CRC(ff344945) SHA1(2743778c42f53321f9691d60bbf94ea8baf1382f) )
	ROM_LOAD16_BYTE( "mpr-12718.b10", 0x080001, 0x40000, CRC(ba2efc0c) SHA1(459a1a280f870c94aefb70127ed007cb090ed203) )
	ROM_LOAD16_BYTE( "mpr-12725.a10", 0x080000, 0x40000, CRC(268a0c17) SHA1(2756054fa3c3aed30a1fce5e41acb0ceaebe90b5) )
	ROM_LOAD16_BYTE( "mpr-12717.b9",  0x100001, 0x40000, CRC(c81cc4f8) SHA1(22f364e85057ceef533e051c8d0755b9691c5ec4) )
	ROM_LOAD16_BYTE( "mpr-12724.a9",  0x100000, 0x40000, CRC(0f4903dc) SHA1(851bd60e877c9e39be23dc1fde91efc9da513c29) )
	ROM_LOAD16_BYTE( "epr-12716.b8",  0x180001, 0x40000, CRC(a870e629) SHA1(29f6633240f9737ec19e16100decc7aa045b2060) )
	ROM_LOAD16_BYTE( "epr-12723.a8",  0x180000, 0x40000, CRC(c606cf90) SHA1(cb53ae9a6da1eb31c584173d1fbbd1c8539fb54c) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12720.a4",  0x00000, 0x20000, CRC(7a0d8de1) SHA1(eca5e2104e5b3e772d083a718171234f06ea8a55) )
	ROM_LOAD( "mpr-12715.b4",  0x80000, 0x40000, CRC(07051a52) SHA1(d48658497f4a34665d3e051f893ff057c38925ae) )
ROM_END

ROM_START( shdancer1 )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr-12772b.a6",  0x000000, 0x40000, CRC(6868a4d4) SHA1(f0d142c81fe1eba4f5c59a0163e25c80ccfe85d7) )
	ROM_LOAD16_BYTE( "epr-12771b.a5",  0x000001, 0x40000, CRC(04e30c84) SHA1(6c5705f7de6ee1bd754b51988cc7d1008f817c78) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-12712.b1",  0x00000, 0x40000, CRC(9bdabe3d) SHA1(4bb30fa2d4cdefe4a864cef7153b516bc5b02c42) )
	ROM_LOAD( "mpr-12713.b2",  0x40000, 0x40000, CRC(852d2b1c) SHA1(8e5bc83d45e48b621ea3016207f2028fe41701e6) )
	ROM_LOAD( "mpr-12714.b3",  0x80000, 0x40000, CRC(448226ce) SHA1(3060e4a43311069e2691d659c1e0c1a48edfeedb) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-12719.b11", 0x000001, 0x40000, CRC(d6888534) SHA1(2201f1921a68cf39e5a94b487c90e48d032d630f) )
	ROM_LOAD16_BYTE( "mpr-12726.a11", 0x000000, 0x40000, CRC(ff344945) SHA1(2743778c42f53321f9691d60bbf94ea8baf1382f) )
	ROM_LOAD16_BYTE( "mpr-12718.b10", 0x080001, 0x40000, CRC(ba2efc0c) SHA1(459a1a280f870c94aefb70127ed007cb090ed203) )
	ROM_LOAD16_BYTE( "mpr-12725.a10", 0x080000, 0x40000, CRC(268a0c17) SHA1(2756054fa3c3aed30a1fce5e41acb0ceaebe90b5) )
	ROM_LOAD16_BYTE( "mpr-12717.b9",  0x100001, 0x40000, CRC(c81cc4f8) SHA1(22f364e85057ceef533e051c8d0755b9691c5ec4) )
	ROM_LOAD16_BYTE( "mpr-12724.a9",  0x100000, 0x40000, CRC(0f4903dc) SHA1(851bd60e877c9e39be23dc1fde91efc9da513c29) )
	ROM_LOAD16_BYTE( "epr-12716.b8",  0x180001, 0x40000, CRC(a870e629) SHA1(29f6633240f9737ec19e16100decc7aa045b2060) )
	ROM_LOAD16_BYTE( "epr-12723.a8",  0x180000, 0x40000, CRC(c606cf90) SHA1(cb53ae9a6da1eb31c584173d1fbbd1c8539fb54c) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12720.a4",  0x00000, 0x20000, CRC(7a0d8de1) SHA1(eca5e2104e5b3e772d083a718171234f06ea8a55) )
	ROM_LOAD( "mpr-12715.b4",  0x80000, 0x40000, CRC(07051a52) SHA1(d48658497f4a34665d3e051f893ff057c38925ae) )
ROM_END

ROM_START( mwalk )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code - custom cpu 317-0159 */
	ROM_LOAD16_BYTE( "epr-13235.a6", 0x000000, 0x40000, CRC(6983e129) SHA1(a8dd430620ab8ce11df46aa208d762d47f510464) )
	ROM_LOAD16_BYTE( "epr-13234.a5", 0x000001, 0x40000, CRC(c9fd20f2) SHA1(9476e6481e6d8f223acd52f543fa04f408d48dc3) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )  /* decryption key */
	ROM_LOAD( "317-0159.key", 0x0000, 0x2000, CRC(507838f0) SHA1(0c92d313da40b5dec7398c05b57698de6153b4b0) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr-13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr-13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr-13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr-13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr-13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr-13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr-13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr-13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr-13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr-13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr-13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr-13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )

	ROM_REGION( 0x1000, REGION_CPU3, 0 ) /* protection MCU */
	ROM_LOAD( "315-5437.ic4", 0x00000, 0x1000,  CRC(4bf63bc1) SHA1(2766ab30b466b079febb30c488adad9ea56813f7) )
ROM_END

ROM_START( mwalkd )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-13235.a6", 0x000000, 0x40000, CRC(9973f6c3) SHA1(e6267361e808499fcd1c49af6243596b7ebe434d) )
	ROM_LOAD16_BYTE( "bootleg_epr-13234.a5", 0x000001, 0x40000, CRC(ef5098e9) SHA1(2bd17e07477d0f31bddc7e0f53e3447065913072) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr-13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr-13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr-13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr-13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr-13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr-13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr-13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr-13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr-13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr-13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr-13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr-13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )

	ROM_REGION( 0x1000, REGION_CPU3, 0 ) /* protection MCU */
	ROM_LOAD( "315-5437.ic4", 0x00000, 0x1000,  CRC(4bf63bc1) SHA1(2766ab30b466b079febb30c488adad9ea56813f7) )
ROM_END

ROM_START( mwalku )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code - custom cpu 317-0158 */
	ROM_LOAD16_BYTE( "epr-13233.a6", 0x000000, 0x40000, CRC(f3dac671) SHA1(cd9d372c7e272d2371bc1f9fb0167831c804423f) )
	ROM_LOAD16_BYTE( "epr-13232.a5", 0x000001, 0x40000, CRC(541d8bdf) SHA1(6a99153fddca246ba070e93c4bacd145f15f76bf) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )  /* decryption key */
	ROM_LOAD( "317-0158.key", 0x0000, 0x2000, CRC(a8a50e8c) SHA1(6e05a40dbf31b4007df1bb27eee85a78da3d8417) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr-13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr-13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr-13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr-13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr-13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr-13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr-13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr-13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr-13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr-13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr-13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr-13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )

	ROM_REGION( 0x1000, REGION_CPU3, 0 ) /* protection MCU */
	ROM_LOAD( "315-5437.ic4", 0x00000, 0x1000,  CRC(4bf63bc1) SHA1(2766ab30b466b079febb30c488adad9ea56813f7) )
ROM_END

ROM_START( mwalkud )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-13233.a6", 0x000000, 0x40000, CRC(b34fe725) SHA1(9d656b68ba758fba9739413cf404ee9b1471fd68) )
	ROM_LOAD16_BYTE( "bootleg_epr-13232.a5", 0x000001, 0x40000, CRC(0d100fd4) SHA1(8c09c00dcb11fac2c7226e4fcc36b25458ccccff) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr-13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr-13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr-13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr-13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr-13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr-13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr-13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr-13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr-13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr-13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr-13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr-13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )

	ROM_REGION( 0x1000, REGION_CPU3, 0 ) /* protection MCU */
	ROM_LOAD( "315-5437.ic4", 0x00000, 0x1000,  CRC(4bf63bc1) SHA1(2766ab30b466b079febb30c488adad9ea56813f7) )
ROM_END

ROM_START( mwalkj )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code - custom cpu 317-0157 */
	ROM_LOAD16_BYTE( "epr-13227.a6", 0x000000, 0x40000, CRC(6c0534b3) SHA1(23f35d1a15275cbc4b6d2f81f5634abac3832282) )
	ROM_LOAD16_BYTE( "epr-13226.a5", 0x000001, 0x40000, CRC(99765854) SHA1(c00776c676b77fed4e94bb02f52f905c845ee73c) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )  /* decryption key */
	ROM_LOAD( "317-0157.key", 0x0000, 0x2000, CRC(324d6931) SHA1(f8f4530a75aeeace1c8456da37118975c5c43316) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr-13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr-13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr-13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr-13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr-13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr-13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr-13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr-13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr-13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr-13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr-13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr-13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )

	ROM_REGION( 0x1000, REGION_CPU3, 0 ) /* protection MCU */
	ROM_LOAD( "315-5437.ic4", 0x00000, 0x1000, CRC(4bf63bc1) SHA1(2766ab30b466b079febb30c488adad9ea56813f7) )
ROM_END

ROM_START( mwalkjd )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bootleg_epr-13227.a6", 0x000000, 0x40000, CRC(d8e00521) SHA1(a53cdf7e359b25527f4a39ae1a5d0a461ac73242) )
	ROM_LOAD16_BYTE( "bootleg_epr-13226.a5", 0x000001, 0x40000, CRC(cb0ba822) SHA1(c2db762525e7cc65a7272e7a732ba9e3cee29c5d) )

	ROM_REGION( 0xc0000, REGION_GFX1, 0 )
	ROM_LOAD( "mpr-13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr-13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr-13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mpr-13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr-13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr-13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr-13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr-13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr-13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr-13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr-13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr-13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr-13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr-13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )

	ROM_REGION( 0x1000, REGION_CPU3, 0 ) /* protection MCU */
	/* not verified if mcu is the same as the other sets.. */
	ROM_LOAD( "315-5437.ic4", 0x00000, 0x1000, BAD_DUMP CRC(4bf63bc1) SHA1(2766ab30b466b079febb30c488adad9ea56813f7) )
ROM_END

ROM_START( moonwlkb )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "moonwlkb.01", 0x000000, 0x10000, CRC(f49cdb16) SHA1(34b7e98d31c3b9db2f0f055d7b249b0e5e5cb746) )
	ROM_LOAD16_BYTE( "moonwlkb.05", 0x000001, 0x10000, CRC(c483f29f) SHA1(8fdfa764d8e49754844a9dc001400d439f9af9f0) )
	ROM_LOAD16_BYTE( "moonwlkb.02", 0x020000, 0x10000, CRC(0bde1896) SHA1(42731ae90d56918dc50c0dcb53d092dcfb957159) )
	ROM_LOAD16_BYTE( "moonwlkb.06", 0x020001, 0x10000, CRC(5b9fc688) SHA1(53d8143c3876548f63b392f0ea16c0e7c30a7917) )
	ROM_LOAD16_BYTE( "moonwlkb.03", 0x040000, 0x10000, CRC(0c5fe15c) SHA1(626e3f37f019448c3c96bf73b2d2b5fe4b3716c0) )
	ROM_LOAD16_BYTE( "moonwlkb.07", 0x040001, 0x10000, CRC(9e600704) SHA1(efd3d450b26f81dc2b74f44b4aaf906fa017e437) )
	ROM_LOAD16_BYTE( "moonwlkb.04", 0x060000, 0x10000, CRC(64692f79) SHA1(ad7f32997b78863e3aa3214018cdd24e3ec9c5cb) )
	ROM_LOAD16_BYTE( "moonwlkb.08", 0x060001, 0x10000, CRC(546ca530) SHA1(51f74878fdc221fee026e2e6a7ca96f290c8947f) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x200000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x000000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr13219.b4", 0x080000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr13220.b5", 0x100000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr13249.b6", 0x180000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )
ROM_END

/* Shadow Dancer */
ROM_START( shdancbl )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "ic39", 0x000000, 0x10000, CRC(adc1781c) SHA1(b2ca2831a48779df7533e6b2a406ee539e1f650c) )
	ROM_LOAD16_BYTE( "ic53", 0x000001, 0x10000, CRC(1c1ac463) SHA1(21075f7afae372daef197f04f5f12d14479a8140) )
	ROM_LOAD16_BYTE( "ic38", 0x020000, 0x10000, CRC(cd6e155b) SHA1(e37b53cc431533091d26b37be9b8e30494de5faf) )
	ROM_LOAD16_BYTE( "ic52", 0x020001, 0x10000, CRC(bb3c49a4) SHA1(ab01a6de1a6d338d30f9cfea7b3bf80dda67f215) )
	ROM_LOAD16_BYTE( "ic37", 0x040000, 0x10000, CRC(1bd8d5c3) SHA1(4d663362c059e112ac6c742d80200be98d50d175) )
	ROM_LOAD16_BYTE( "ic51", 0x040001, 0x10000, CRC(ce2e71b4) SHA1(3e251319cd4c8c63c66e6b92b2eef514d79dba8e) )
	ROM_LOAD16_BYTE( "ic36", 0x060000, 0x10000, CRC(bb861290) SHA1(62ea8eec74c6b1f5530ee86f97ad821daeac26ad) )
	ROM_LOAD16_BYTE( "ic50", 0x060001, 0x10000, CRC(7f7b82b1) SHA1(675020b57ce689b2767ff83773e2b828cda5aeed) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "ic4",  0x00000, 0x20000, CRC(f0a016fe) SHA1(1426f3fbf50a04a8c5e998e071ca0e78d15f37a8) )
	ROM_LOAD( "ic18", 0x20000, 0x20000, CRC(f6bee053) SHA1(39ee5edfcc67bb4855217c7428254f3e8c862ba0) )
	ROM_LOAD( "ic3",  0x40000, 0x20000, CRC(e07e6b5d) SHA1(bdeb1193415049d0c9261ca261073bdd9e251b88) )
	ROM_LOAD( "ic17", 0x60000, 0x20000, CRC(f59deba1) SHA1(21188d22fe607281bb7da1e1f418a33d4a315695) )
	ROM_LOAD( "ic2",  0x80000, 0x20000, CRC(60095070) SHA1(913c2ee51fb6f838f3c6cbd27032bdf754fbadf1) )
	ROM_LOAD( "ic16", 0xa0000, 0x20000, CRC(0f0d5dd3) SHA1(76812e2f831256a8b6598257dd84a7f07443642e) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */

	/* 12719 */
	ROM_LOAD16_BYTE( "ic73", 0x000001, 0x10000, CRC(59e77c96) SHA1(08da058529ac83352a4528d3792a21edda348f7a) )
	ROM_LOAD16_BYTE( "ic74", 0x020001, 0x10000, CRC(90ea5407) SHA1(4bdd93c86cb35822517433d491aa8be6857dd36c) )
	ROM_LOAD16_BYTE( "ic75", 0x040001, 0x10000, CRC(27d2fa61) SHA1(0ba3cd9448e54ce9fc9433f3edd28de9a4e451e9) )
	ROM_LOAD16_BYTE( "ic76", 0x060001, 0x10000, CRC(f36db688) SHA1(a527298ce9ca1d9f5aa7b9eac93985f34ca8119f) )

	/* 12726 */
	ROM_LOAD16_BYTE( "ic58", 0x000000, 0x10000, CRC(9cd5c8c7) SHA1(54c2d0a683bda37eb9a75f90f4ca5e620c09c4cf) )
	ROM_LOAD16_BYTE( "ic59", 0x020000, 0x10000, CRC(ff40e872) SHA1(bd2c4aac427d106a46318f4cb2eb05c34d3c70b6) )
	ROM_LOAD16_BYTE( "ic60", 0x040000, 0x10000, CRC(826d7245) SHA1(bb3394de058bd63b9939cd05f22c925e0cdc840a) )
	ROM_LOAD16_BYTE( "ic61", 0x060000, 0x10000, CRC(dcf8068b) SHA1(9c78de224df76fc90fb90f1bbd9b22dad0874f69) )

	/* 12718 */
	ROM_LOAD16_BYTE( "ic77", 0x080001, 0x10000, CRC(f93470b7) SHA1(1041afa43aa8d0589d6def9743721cdbda617f78) )
	ROM_LOAD16_BYTE( "ic78", 0x0A0001, 0x10000, CRC(4d523ea3) SHA1(053c30778017127dddeae0783af463aef17bcc9a) ) /* corrupt? (bad sprite when dog attacts in attract mode) */
	ROM_LOAD16_BYTE( "ic95", 0x0C0001, 0x10000, CRC(828b8294) SHA1(f2cdb882fb0709a909e6ef98f0315aceeb8bf283) )
	ROM_LOAD16_BYTE( "ic94", 0x0E0001, 0x10000, CRC(542b2d1e) SHA1(1ce91aea6c49e6e365a91c30ca3049682c2162da) )

	/* 12725 */
	ROM_LOAD16_BYTE( "ic62", 0x080000, 0x10000, CRC(50ca8065) SHA1(8c0d6ae34b9da6c376df387e8fc8b1068bcb4dcb) )
	ROM_LOAD16_BYTE( "ic63", 0x0A0000, 0x10000, CRC(d1866aa9) SHA1(524c82a12a1c484a246b8d49d9f05a774d008108) )
	ROM_LOAD16_BYTE( "ic90", 0x0C0000, 0x10000, CRC(3602b758) SHA1(d25b6c8420e07d0f2ac3e1d8717f14738466df16) )
	ROM_LOAD16_BYTE( "ic89", 0x0E0000, 0x10000, CRC(1ba4be93) SHA1(6f4fe2016e375be3df477436f5cde7508a24ecd1) )

	/* 12717 */
	ROM_LOAD16_BYTE( "ic79", 0x100001, 0x10000, CRC(f22548ee) SHA1(723cb7604784c6715817daa8c86c18c6bcd1388d) )
	ROM_LOAD16_BYTE( "ic80", 0x120001, 0x10000, CRC(6209f7f9) SHA1(09b33c99d972a62af8ef56dacfa6262f002aba0c) )
	ROM_LOAD16_BYTE( "ic81", 0x140001, 0x10000, CRC(34692f23) SHA1(56126a81ac279662e3e3423da5205f65a62c4600) )
	ROM_LOAD16_BYTE( "ic82", 0x160001, 0x10000, CRC(7ae40237) SHA1(fae97cfcfd3cd557da3330158831e4727c438745) )

	/* 12724 */
	ROM_LOAD16_BYTE( "ic64", 0x100000, 0x10000, CRC(7a8b7bcc) SHA1(00cbbbc4b3db48ca3ac65ff56b02c7d63a1b898a) )
	ROM_LOAD16_BYTE( "ic65", 0x120000, 0x10000, CRC(90ffca14) SHA1(00962e5309a79ce34c6f420036054bc607595dfe) )
	ROM_LOAD16_BYTE( "ic66", 0x140000, 0x10000, CRC(5d655517) SHA1(2a1c197dde62bd7946ca7b5f1c2833bdbc2e2e32) )
	ROM_LOAD16_BYTE( "ic67", 0x160000, 0x10000, CRC(0e5d0855) SHA1(3c15088f7fdda5c2bba9c89d244bbcff022f05fd) )

	/* 12716 */
	ROM_LOAD16_BYTE( "ic83", 0x180001, 0x10000, CRC(a9040a32) SHA1(7b0b375285f528b2833c50892b55b0d4c550506d) )
	ROM_LOAD16_BYTE( "ic84", 0x1A0001, 0x10000, CRC(d6810031) SHA1(a82857a9ac442fbe076cdafcf7390765391ed136) )
	ROM_LOAD16_BYTE( "ic92", 0x1C0001, 0x10000, CRC(b57d5cb5) SHA1(636f1a07a84d37cecbe388a2f585893c4611436c) )
	ROM_LOAD16_BYTE( "ic91", 0x1E0001, 0x10000, CRC(49def6c8) SHA1(d8b2cc1993f0808553f87bf56fdbe47374576c5a) )

	/* 12723 */
	ROM_LOAD16_BYTE( "ic68", 0x180000, 0x10000, CRC(8d684e53) SHA1(00e82ddaf875a7452ff978b7b7eb87a1a5a8fb64) )
	ROM_LOAD16_BYTE( "ic69", 0x1A0000, 0x10000, CRC(c47d32e2) SHA1(92b21f51abdd7950fb09d965b1d71b7bffac31ec) )
	ROM_LOAD16_BYTE( "ic88", 0x1C0000, 0x10000, CRC(9de140e1) SHA1(f1125e056a898a4fa519b49ae866c5c742e36bf7) )
	ROM_LOAD16_BYTE( "ic87", 0x1E0000, 0x10000, CRC(8172a991) SHA1(6d12b1533a19cb02613b473cc8ba73ece1f2a2fc) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "ic45", 0x10000, 0x10000, CRC(576b3a81) SHA1(b65356a3837ed3875634ab0cbcd61acce44f2bb9) )
	ROM_LOAD( "ic46", 0x20000, 0x10000, CRC(c84e8c84) SHA1(f57895bedb6152c30733e91e6f4795702a62ac3a) )
ROM_END

ROM_START( aquario )
	ROM_REGION( 0x300000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "a4.bin",  0x000000, 0x080000, CRC(c58ff95f) SHA1(27301df8f72ccc8a27e9520e11035aa63b8e03b6) )
	ROM_LOAD16_BYTE( "a6.bin",  0x000001, 0x080000, CRC(b4a94cd9) SHA1(2059942a26d9e1de63b329d1c8b643535761d2d8) )
	ROM_LOAD16_BYTE( "a5.bin",  0x200000, 0x80000, CRC(1cef8145) SHA1(78a1be8ea0cc0d4e56b2cf9a7c1bd3e08352e175) )
	ROM_LOAD16_BYTE( "a7.bin",  0x200001, 0x80000, CRC(504e4665) SHA1(9b052b48b7cb2da880d6589fdcd1041eca555f7c) )

// these are same as Clutch Hitter boot values
	ROM_REGION( 0x180000,  REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "c1.bin",       0x000000, 0x080000, CRC(93ad1357) SHA1(09b35481798035b5f7d7d533e27418a298c6e2c7) )
	ROM_LOAD( "c2.bin",       0x080000, 0x080000, CRC(4010d14b) SHA1(f9d2e726a032f49fac69a223107966f2884821b5) )
	ROM_LOAD( "c3.bin",       0x100000, 0x080000, CRC(3a3d0285) SHA1(21899b3b2bcb979d53e78b0d48c493a9a15955c7) )

// these are the same as Laser Ghost boot values
	ROM_REGION16_BE( 0x800000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "a10.bin",      0x000000, 0x080000, CRC(b863e533) SHA1(e80fa6a74c43c040fd4b857247aecf03a3de3d87) )
	ROM_LOAD16_BYTE( "c10.bin",      0x000001, 0x080000, CRC(c9ce76f9) SHA1(a096583f5e81f02d6a34802688d201d8d986a84a) )
	ROM_LOAD16_BYTE( "a11.bin",      0x200000, 0x080000, CRC(8b568940) SHA1(19cd028cd43fa07904deb0250564251ba0128c4b) )
	ROM_LOAD16_BYTE( "c11.bin",      0x200001, 0x080000, CRC(06edb7bc) SHA1(e24ec7b52638edaa8debee0aca40ddf902c63334) )
	ROM_LOAD16_BYTE( "a12.bin",      0x400000, 0x080000, CRC(0219923f) SHA1(1e52df5ef155f5a4d74eabea22bb431a569e344f) )
	ROM_LOAD16_BYTE( "c12.bin",      0x400001, 0x080000, CRC(0bb79c56) SHA1(fdaa6cc9efb3104b78392530547bd82c21cff825) )
	ROM_LOAD16_BYTE( "a13.bin",      0x600000, 0x080000, CRC(9ea5c73d) SHA1(e42002cc13548a8aba6ffb0c60470b345b88eaa8) )
	ROM_LOAD16_BYTE( "c13.bin",      0x600001, 0x080000, CRC(0beef46e) SHA1(eccba6d4e015e93f5ca25ef6df31a491193d08a4) )

// Use generic MAME95 segas18 sound loading with 4 and 8 same as Desert Breaker not sure as per SHA1's yet
	ROM_REGION( 0x210000, REGION_CPU2, ROMREGION_ERASEFF ) /* sound CPU */
	ROM_LOAD( "c7.bin",  0x000000, 0x40000, CRC(f1183938) )
	ROM_LOAD( "c6.bin",  0x080000, 0x80000, CRC(39f11291) )
	ROM_LOAD( "c5.bin",  0x100000, 0x80000, CRC(6a380dca) )
	ROM_LOAD( "c4.bin",  0x180000, 0x80000, CRC(1bd081f8) )
ROM_END

/**************************************************************************************************************************
    Hammer Away, Sega System 18
    CPU: M68000
    ROM Board: 837-7525
*/

ROM_START( hamaway )
	ROM_REGION( 0x280000, REGION_CPU1, 0 ) // 68000 code
	ROM_LOAD16_BYTE( "4.bin",  0x000000, 0x40000, CRC(cc0981e1) SHA1(63528bd36f27e62fdf40715101e6d05b73e48f16) ) // 1xxxxxxxxxxxxxxxxx = 0xFF
	ROM_LOAD16_BYTE( "6.bin",  0x000001, 0x40000, CRC(e8599ee6) SHA1(3e32b025403aecbaecfcdd0325e4acd676e99c4e) ) // 1xxxxxxxxxxxxxxxxx = 0xFF
	ROM_LOAD16_BYTE( "5.bin",  0x200000, 0x40000, CRC(fdb247fd) SHA1(ee9db799fb5de27f81904f8ef792203415b6d4a6) )
	ROM_LOAD16_BYTE( "7.bin",  0x200001, 0x40000, CRC(63711470) SHA1(6c4be3a0cf0f897c34ef0b3bf549f52b185bb915) )

	ROM_REGION( 0x180000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "c10.bin",  0x000000, 0x40000, CRC(c55cb5cf) SHA1(396179632b29ac5f8b7f8f3c91d7cf834e548bdf) )
	ROM_LOAD( "1.bin",    0x040000, 0x40000, CRC(33be003f) SHA1(134fa6b3347c306d9e30882dfcf24632b49f85ea) )
	ROM_LOAD( "c11.bin",  0x080000, 0x40000, CRC(37787915) SHA1(c8d251be6c41de3aed2da6da70aa87071b70b1f6) )
	ROM_LOAD( "2.bin",    0x0c0000, 0x40000, CRC(60ca5c9f) SHA1(6358ea00125a5e3f55acf73aeb9c36b1db6e711e) )
	ROM_LOAD( "c12.bin",  0x100000, 0x40000, CRC(f12f1cf3) SHA1(45e883029c58e617a2a20ac1ab5c5f598c95f4bd) )
	ROM_LOAD( "3.bin",    0x140000, 0x40000, CRC(520aa7ae) SHA1(9584206aedd8be5ce9dca0ed370f8fe77aabaf76) )

	ROM_REGION16_BE( 0x200000, REGION_GFX2, ROMREGION_ERASEFF ) // sprites
	ROM_LOAD16_BYTE( "c17.bin", 0x000001, 0x40000, CRC(aa28d7aa) SHA1(3dd5d95b05e408c023f9bd77753c37080714239d) )
	ROM_LOAD16_BYTE( "10.bin",  0x000000, 0x40000, CRC(c4c95161) SHA1(2e313a4ca9506f53a2062b4a8e5ba7b381ba93ae) )
	ROM_LOAD16_BYTE( "c18.bin", 0x080001, 0x40000, CRC(0f8fe8bb) SHA1(e6f68442b8d4def29b106458496a47344f70d511) )
	ROM_LOAD16_BYTE( "11.bin",  0x080000, 0x40000, CRC(2b5eacbc) SHA1(ba3690501588b9c88a31022b44bc3c82b44ae26b) )
	ROM_LOAD16_BYTE( "c19.bin", 0x100001, 0x40000, CRC(3c616caa) SHA1(d48a6239b7a52ac13971f7513a65a17af492bfdf) ) // 11xxxxxxxxxxxxxxxx = 0xFF
	ROM_LOAD16_BYTE( "12.bin",  0x100000, 0x40000, CRC(c7bbd579) SHA1(ab87bfdad66ea241cb23c9bbfea05f5a1574d6c9) ) // 1ST AND 2ND HALF IDENTICAL (but ok, because pairing ROM has no data in the 2nd half anyway)

	ROM_REGION( 0x210000, REGION_CPU2, ROMREGION_ERASEFF ) // sound CPU
	ROM_LOAD( "c16.bin", 0x000000, 0x40000, CRC(913cc18c) SHA1(4bf4ec14937586c3ae77fcad57dcb21f6433ef81) )
	ROM_LOAD( "c15.bin", 0x080000, 0x40000, CRC(b53694fc) SHA1(0e42be2730abce1b52ea94a9fe61cbd1c9a0ccae) )
ROM_END


/*****************************************************************************/

GAME( 1990, astorm,     0,        sys18_astorm,  sys18_astorm2p, sys18_5874_fd, ROT0, "Sega",         "Alien Storm (World, 2 Players) (FD1094 317-0154)" )
GAME( 1990, astorm3,    astorm,   sys18_astorm,  sys18_astorm,   sys18_5874_fd, ROT0, "Sega",         "Alien Storm (World, 3 Players) (FD1094 317-0148)" )
GAME( 1990, astormu,    astorm,   sys18_astorm,  sys18_astorm,   sys18_5874_fd, ROT0, "Sega",         "Alien Storm (US, 3 Players) (FD1094 317-0147)" )
GAME( 1990, astormj,    astorm,   sys18_astorm,  sys18_astorm2p, sys18_5874_fd, ROT0, "Sega",         "Alien Storm (Japan, 2 Players) (FD1094 317-0146)" )
GAME( 1990, astorm3d,   astorm,   sys18_astorm,  sys18_astorm,   sys18_5874,    ROT0, "bootleg",      "Alien Storm (World, 3 Players) (bootleg of FD1094 317-0148 set)" )
GAME( 1990, astormud,   astorm,   sys18_astorm,  sys18_astorm,   sys18_5874,    ROT0, "bootleg",      "Alien Storm (US, 3 Players) (bootleg of FD1094 317-0147 set)" )
GAME( 1990, astormjd,   astorm,   sys18_astorm,  sys18_astorm2p, sys18_5874,    ROT0, "bootleg",      "Alien Storm (Japan, 2 Players) (bootleg of FD1094 317-0146 set)" )
GAME( 1989, bloxeed,    0,        sys18_bloxeed, bloxeed,        sys18_5874_fd, ROT0, "Sega / Elorg", "Bloxeed (Japan) (FD1094 317-0139)" )
GAME( 1989, bloxeeda,   bloxeed,  sys18_bloxeed, bloxeed,        sys18_5874_fd, ROT0, "Sega",         "Bloxeed (Japan, earlier) (FD1094 317-0139)" )
GAME( 1989, bloxeedd,   bloxeed,  sys18_bloxeed, bloxeed,        sys18_5874,    ROT0, "bootleg",      "Bloxeed (Japan) (bootleg of FD1094 317-0139 set)" )
GAME( 1991, cltchitr,   0,        sys18_5987,    cltchitr,       sys18_5987_fd, ROT0, "Sega",         "Clutch Hitter (US) (FD1094 317-0176)" )
GAME( 1991, cltchitrj,  cltchitr, sys18_5987,    cltchitr,       sys18_5987_fd, ROT0, "Sega",         "Clutch Hitter (Japan) (FD1094 317-0175)" )
GAME( 1991, cltchitrd,  cltchitr, sys18_5987,    cltchitr,       sys18_5987,    ROT0, "bootleg",      "Clutch Hitter (US) (bootleg of FD1094 317-0176 set)" )
GAME( 1991, cltchitrjd, cltchitr, sys18_5987,    cltchitr,       sys18_5987,    ROT0, "bootleg",      "Clutch Hitter (Japan) (bootleg of FD1094 317-0175 set)" )
GAME( 1992, wwallyj,    0,        sys18_5987,    wwally,         wwally_fd,     ROT0, "Sega",         "Wally wo Sagase! (rev B, Japan, 2 players) (FD1094 317-0197B)" )
GAME( 1992, wwallyja,   wwallyj,  sys18_5987,    wwally,         wwally_fd,     ROT0, "Sega",         "Wally wo Sagase! (rev A, Japan, 2 players) (FD1094 317-0197A)" )
GAME( 1992, wwallyja3p, wwallyj,  sys18_5987,    wwally3p,       wwally_fd,     ROT0, "Sega",         "Wally wo Sagase! (rev A, Japan, 3 players) (FD1094 317-0198A)" )
GAME( 1992, wwallyjd,   wwallyj,  sys18_5987,    wwally,         wwally,        ROT0, "bootleg",      "Wally wo Sagase! (rev B, Japan) (bootleg of FD1094 317-0197B set)" )
GAME( 1992, wwallyjad,  wwallyj,  sys18_5987,    wwally,         wwally,        ROT0, "bootleg",      "Wally wo Sagase! (rev A, Japan) (bootleg of FD1094 317-0197A set)" )
GAME( 1991, ddcrew,    0,        sys18_5987e,   sys18_ddcrew3p,  sys18_5987_fd,  ROT0, "Sega",         "D. D. Crew (World, 3 Players) (FD1094 317-0190)" )
GAME( 1991, ddcrewu,   ddcrew,   sys18_5987e,   sys18_ddcrew4p,  ddcrew4p_fd,    ROT0, "Sega",         "D. D. Crew (US, 4 Players) (FD1094 317-0186)" )
GAME( 1991, ddcrew2,   ddcrew,   sys18_5987e,   sys18_ddcrew2p,  sys18_5987_fd,  ROT0, "Sega",         "D. D. Crew (World, 2 Players) (FD1094 317-0184)" )
GAME( 1991, ddcrew1,   ddcrew,   sys18_5987e,   sys18_ddcrew4p,  ddcrew4p_fd,    ROT0, "Sega",         "D. D. Crew (World, 4 Players) (FD1094 317-0187)" )
GAME( 1991, ddcrewj,   ddcrew,   sys18_5987e,   sys18_ddcrew4p,  ddcrew4p_fd,    ROT0, "Sega",         "D. D. Crew (Japan, 4 Players) (FD1094 317-0185)" )
GAME( 1991, ddcrewj2,  ddcrew,   sys18_5987e,   sys18_ddcrew2p,  sys18_5987_fd,  ROT0, "Sega",         "D. D. Crew (Japan, 2 Players) (FD1094 317-0182)" )
GAME( 1991, ddcrewd,   ddcrew,   sys18_5987e,   sys18_ddcrew3p,  sys18_5987,     ROT0, "bootleg",      "D. D. Crew (World, 3 Players) (bootleg of FD1094 317-0190 set)" )
GAME( 1991, ddcrewud,  ddcrew,   sys18_5987e,   sys18_ddcrew4p,  ddcrew4p,       ROT0, "bootleg",      "D. D. Crew (US, 4 Players) (bootleg of FD1094 317-0186 set)" )
GAME( 1991, ddcrew2d,  ddcrew,   sys18_5987e,   sys18_ddcrew2p,  sys18_5987,     ROT0, "bootleg",      "D. D. Crew (World, 2 Players) (bootleg of FD1094 317-0184 set)" )
GAME( 1991, ddcrew1d,  ddcrew,   sys18_5987e,   sys18_ddcrew4p,  ddcrew4p,       ROT0, "bootleg",      "D. D. Crew (World, 4 Players) (bootleg of FD1094 317-0187 set)" )
GAME( 1991, ddcrewjd,  ddcrew,   sys18_5987e,   sys18_ddcrew4p,  ddcrew4p,       ROT0, "bootleg",      "D. D. Crew (Japan, 4 Players) (bootleg of FD1094 317-0185 set)" )
GAME( 1991, ddcrewj2d, ddcrew,   sys18_5987e,   sys18_ddcrew2p,  sys18_5987,     ROT0, "bootleg",      "D. D. Crew (Japan, 2 Players) (bootleg of FD1094 317-0182 set)" )
GAME( 1992, desertbr,  0,        sys18_5987e,   sys18_desertbr,  sys18_5987_fd,  ROT270, "Sega",         "Desert Breaker (World) (FD1094 317-0196)" )
GAME( 1992, desertbrj, desertbr, sys18_5987e,   sys18_desertbr,  sys18_5987_fd,  ROT270, "Sega",         "Desert Breaker (Japan) (FD1094 317-0194)" )
GAME( 1992, desertbrd, desertbr, sys18_5987e,   sys18_desertbr,  sys18_5987,     ROT270, "bootleg",      "Desert Breaker (World) (bootleg of FD1094 317-0196 set)" )
GAME( 1992, desertbrjd, desertbr, sys18_5987e,   sys18_desertbr,  sys18_5987,     ROT270, "bootleg",      "Desert Breaker (Japan) (bootleg of FD1094 317-0194 set)" )
GAME( 1990, lghost,    0,        sys18_5987e,   sys18_lghost,    lghost_fd,      ROT0, "Sega",         "Laser Ghost (World) (FD1094 317-0166)" )
GAME( 1990, lghostu,   lghost,   sys18_5987e,   sys18_lghost,    lghost_fd,      ROT0, "Sega",         "Laser Ghost (US) (FD1094 317-0165)" )
GAME( 1990, lghostj,   lghost,   sys18_5987e,   sys18_lghost,    lghost_fd,      ROT0, "Sega",         "Laser Ghost (Japan) (FD1094 317-0164)" )
GAME( 1990, lghostd,   lghost,   sys18_5987e,   sys18_lghost,    lghost,         ROT0, "bootleg",      "Laser Ghost (World) (bootleg of FD1094 317-0166 set)" )
GAME( 1990, lghostud,  lghost,   sys18_5987e,   sys18_lghost,    lghost,         ROT0, "bootleg",      "Laser Ghost (US) (bootleg of FD1094 317-0165 set)" )
GAME( 1989, shdancer,  0,        sys18_bloxeed, sys18_shdancer,  sys18_5874,     ROT0, "Sega",         "Shadow Dancer (World)" )
GAME( 1989, shdancerj, shdancer, sys18_bloxeed, sys18_shdancer,  sys18_5874,     ROT0, "Sega",         "Shadow Dancer (Japan)" )
GAME( 1989, shdancer1, shdancer, sys18_bloxeed, sys18_shdancer,  sys18_5874,     ROT0, "Sega",         "Shadow Dancer (US)" )
GAME( 1990, astormbl, astorm,   astorm,   astorm,   astorm,   ROT0, "bootleg", "Alien Storm (bootleg)" )
GAME( 1990, mwalk,    0,        mwalk,    mwalk,    mwalk,    ROT0, "Sega",    "Michael Jackson's Moonwalker (World) (FD1094/8751 317-0159)" )
GAME( 1990, mwalku,   mwalk,    mwalk,    mwalka,   mwalk,    ROT0, "Sega",    "Michael Jackson's Moonwalker (US) (FD1094/8751 317-0158)" )
GAME( 1990, mwalkj,   mwalk,    mwalk,    mwalk,    mwalk,    ROT0, "Sega",    "Michael Jackson's Moonwalker (Japan) (FD1094/8751 317-0157)" )
GAME( 1990, mwalkd,   mwalk,    mwalk,    mwalk,    mwalkd,   ROT0, "bootleg", "Michael Jackson's Moonwalker (World) (bootleg of FD1094/8751 317-0159 set)" )
GAME( 1990, mwalkud,  mwalk,    mwalk,    mwalka,   mwalkd,   ROT0, "bootleg", "Michael Jackson's Moonwalker (US) (bootleg of FD1094/8751 317-0158 set)" )
GAME( 1990, mwalkjd,  mwalk,    mwalk,    mwalk,    mwalkd,   ROT0, "bootleg", "Michael Jackson's Moonwalker (Japan) (bootleg of FD1094/8751 317-0157 set)" )
GAME( 1990, moonwlkb, mwalk,    moonwalk, moonwalk, moonwalk, ROT0, "bootleg", "Michael Jackson's Moonwalker (bootleg)" )
GAMEX(1989, shdancbl, shdancer, shdancbl, shdancer, shdancbl, ROT0, "bootleg", "Shadow Dancer (bootleg)", GAME_IMPERFECT_GRAPHICS)
GAME( 2021, aquario,  0,        aquario,  aquario,  0,        ROT0, "Sega / Westone", "Clockwork Aquario (prototype)")
GAME( 1991, hamaway,  0,        hamaway,  hamaway,  0,        ROT90, "Sega / Santos", "Hammer Away (Japan, prototype)" )


