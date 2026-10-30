/***********************************************************************************************

	Sega System C (System 14)/C2 Driver
	driver by David Haywood and Aaron Giles
	---------------------------------------

	Sega's C2 was used between 1989 and 1994, the hardware being very similar to that
	used by the Sega MegaDrive/Genesis Home Console Sega produced around the same time.

	The Columns and Bloxeed (USA) manual call this system 'System 14' instead of system C/C2.

	As in current MAME (segac2.cpp): the sets and ROMs, the inputs, the protection chip
	(the EPM5032 logic of each game, as a function), the alternate palette mode of
	Ribbit! and Twin Squash, the address mirrors and the battery-backed RAM. The VDP is
	in vidhrdw/segavdp.c, the Mega Drive part (the VDP interrupts) in machine/megadriv.c;
	the other Mega Drive based boards have their own drivers (megaplay.c, megatech.c,
	megadriv_acbl.c, megadriv_ybox.c, megadriv_sunmix.c).

	Year  Game                       Developer         Protection Chip  Board
	====  ====================       ================  ===============  =====
	1989  Bloxeed                    Sega / Elorg      n/a              C
	1989  Bloxeed (USA)              Sega / Elorg      317-0140         C
	1990  Columns                    Sega              317-0149         C
	1990  Columns II                 Sega              317-0160         C
	1990  Thunder Force AC           Sega / Technosoft 317-0172         C2
	1990  Borench                    Sega              317-0173         C2
	1991  Twin Squash                Sega              317-0193         C2
	1991  Waku Waku Sonic Patrol Car Sega              317-0140         C2
	1992  Ribbit!                    Sega              317-0178         C2
	1992  Puyo Puyo                  Sega / Compile    317-0203         C2
	1992  Tant-R (Japan)             Sega              317-0211         C2
	1992  Tant-R (Korea)             Sega              ?                C2
	1992  Waku Waku Marine           Sega              317-0140         C2
	1993  SegaSonic Popcorn Shop     Sega              317-0140         C2
	1993  SegaSonic Cosmo Fighter    Sega              317-0140         C2
	1994  PotoPoto (Japan)           Sega              317-0218         C2
	1994  Stack Columns (Japan)      Sega              317-0219         C2
	1994  Stack Columns (World)      Sega              317-0223         C2
	1994  Ichidant-R (Japan)         Sega              317-0224         C2
	1994  Ichidant-R (World)         Sega              ?                C2
	1994  Ichidant-R (Korea)         Sega              ?                C2
	1994  Puyo Puyo 2                Compile           317-0228         C2
	1994  Zunzunkyou no Yabou        Sega              317-0221         C2

	1995  Print Club (Vol.1)         Atlus             ?                C2
	1995  Print Club (Vol.2)         Atlus             ?                C2
	1996  Print Club (Vol.3)         Atlus             317-5030         C2
	1996  Print Club (Vol.4)         Atlus             ?                C2
	1996  Print Club (Vol.5)         Atlus             ?                C2

	Notes:	Bloxeed doesn't read from the protection chip at all; all of the other games do.

			I'm assuming System-C was the Board without the uPD7759 chip and System-C2 was the
			version of the board with it, this could be completely wrong but it doesn't really
			matter anyway.

			The "Print Club" boards used by Atlus are variants of the C2 board with a 52.867MHz
			XL2 instead of 53.693MHz, a camera and a printer (not emulated).

	Thanks:	(in no particular order) to any MameDev that helped me out .. (OG, Mish etc.)
			Charles MacDonald for his C2Emu .. without it working out what were bugs in my code
				and issues due to protection would have probably killed the driver long ago :p
			Razoola & Antiriad .. for helping teach me some 68k ASM needed to work out just why
				the games were crashing :)
			Sega for producing some Fantastic Games...
			and anyone else who knows they've contributed :)

***********************************************************************************************/


#include "driver.h"
#include "state.h"
#include "segac2.h"

#define LOG_PROTECTION		0
#define LOG_PALETTE			0
#define LOG_IOCHIP			0


/******************************************************************************
	Constants
******************************************************************************/

#define XL1_CLOCK			640000
#define XL2_CLOCK			53693175
#define XL2_CLOCK_PCLUB		52867000


/******************************************************************************
	Global variables
******************************************************************************/

/* internal states */
static UINT8		iochip_reg[0x10];	/* holds values written to the I/O chip */

/* protection-related tracking */
static int			(*prot_func)(int in);	/* the EPM5032 logic of the game */
static UINT16		prot_write_buf;		/* remembers what was written */
static UINT16		prot_read_buf;		/* remembers what was returned */

/* sound-related variables */
static UINT8		sound_banks;		/* number of sound banks */
static UINT8		bloxeed_sound;		/* use kludge for bloxeed sound? */
static UINT8		last_port;			/* the last YM3438 register selected (for the kludge) */

/* the alternate palette mode of the EPM5032 (Ribbit!, Twin Squash) */
static int			alt_palette_mode;

/* the free-spinning wheel of Waku Waku Marine / Anpanman */
static UINT8		wheel_input;

/* Print Club camera */
static int			cam_data;

/* RAM pointers */
static data16_t *	main_ram;			/* pointer to main RAM */


/******************************************************************************
	Machine init
*******************************************************************************

	This is called at init time, when it's safe to create a timer. We use
	it to prime the scanline interrupt timer.

******************************************************************************/

static MACHINE_INIT( segac2 )
{
	/* the VDP interrupts (machine/megadriv.c); bit 4 of port H masks the H-INT */
	megadriv_irq_init(&iochip_reg[7]);

	/* the RAM mirror at 0xe00000 */
	cpu_setbank(1, main_ram);

	/* determine how many sound banks */
	sound_banks = 0;
	if (memory_region(REGION_SOUND1))
		sound_banks = memory_region_length(REGION_SOUND1) / 0x20000;

	/* reset the protection */
	prot_write_buf = 0;
	prot_read_buf = 0;
	alt_palette_mode = 0;
	segavdp_set_alt_palette(0);
}


/******************************************************************************
	Sound handlers
*******************************************************************************

	These handlers are responsible for communicating with the (genenerally)
	8-bit sound chips. All accesses are via the low byte.

	The Sega C/C2 system uses a YM3438 (compatible with the YM2612) for FM-
	based music generation, and an SN76489 for PSG and noise effects. The
	C2 board also appears to have a UPD7759 for sample playback.

******************************************************************************/

/* handle reads from the YM3438 */
static READ16_HANDLER( ym3438_r )
{
	switch (offset)
	{
		case 0: return YM2612_status_port_0_A_r(0);
		case 1: return YM2612_read_port_0_r(0);
		case 2: return YM2612_status_port_0_B_r(0);
	}
	return 0xff;
}


/* handle writes to the YM3438 */
static WRITE16_HANDLER( ym3438_w )
{
	/* only works if we're accessing the low byte */
	if (ACCESSING_LSB)
	{
		/* kludge for Bloxeed - it seems to accidentally trip timer 2  */
		/* and has no recourse for clearing the interrupt; until we    */
		/* find more documentation on the 2612/3438, it's unknown what */
		/* to do here */
		if (bloxeed_sound && last_port == 0x27 && (offset & 1))
			data &= ~0x08;

		switch (offset)
		{
			case 0: YM2612_control_port_0_A_w(0, data & 0xff);	last_port = data;	break;
			case 1: YM2612_data_port_0_A_w(0, data & 0xff);							break;
			case 2: YM2612_control_port_0_B_w(0, data & 0xff);	last_port = data;	break;
			case 3: YM2612_data_port_0_B_w(0, data & 0xff);							break;
		}
	}
}

/* handle writes to the UPD7759 */
static WRITE16_HANDLER( UPD7759_w )
{
	/* make sure we have a UPD chip */
	if (!sound_banks)
		return;

	/* only works if we're accessing the low byte */
	if (ACCESSING_LSB)
	{
		upd7759_reset_w(0, 0);
		upd7759_reset_w(0, 1);
		upd7759_port_w(0, data & 0xff);
		upd7759_start_w(0, 0);
		upd7759_start_w(0, 1);
	}
}



/******************************************************************************
	Palette RAM Read / Write Handlers
*******************************************************************************

	The following Read / Write Handlers are used when accessing Palette RAM.
	The C2 Hardware appears to use 4 Banks of Colours 1 of which can be Mapped
	to 0x8C0000 - 0x8C03FF at any given time by writes to 0x84000E (This same
	address also looks to be used for things like Sample Banking)

	Each Colour uses 15-bits (from a 16-bit word) in the Format
		xBGRBBBB GGGGRRRR  (x = unused, B = Blue, G = Green, R = Red)

	As this works out the Palette RAM Stores 2048 from a Possible 4096 Colours
	at any given time.

	In the alternate palette mode (Ribbit!, Twin Squash) the EPM5032 swizzles
	the colour RAM address bits (see vidhrdw/segavdp.c).

******************************************************************************/

static offs_t palette_offset(offs_t offset)
{
	offset &= 0x1ff;
	if (alt_palette_mode)
		offset = ((offset << 1) & 0x100) | ((offset << 2) & 0x80) | ((~offset >> 2) & 0x40) | ((offset >> 1) & 0x20) | (offset & 0x1f);
	return offset + segavdp_get_palbank();
}

/* handle reads from the paletteram */
static READ16_HANDLER( palette_r )
{
	return paletteram16[palette_offset(offset)];
}


/* handle writes to the paletteram */
static WRITE16_HANDLER( palette_w )
{
	int r,g,b,newword;

	/* adjust for the palette bank */
	offset = palette_offset(offset);

	/* combine data */
	COMBINE_DATA(&paletteram16[offset]);
	newword = paletteram16[offset];

	/* up to 8 bits */
	r = ((newword << 4) & 0xf0) | ((newword >>  9) & 0x08);
	g = ((newword >> 0) & 0xf0) | ((newword >> 10) & 0x08);
	b = ((newword >> 4) & 0xf0) | ((newword >> 11) & 0x08);
	r |= r >> 5;
	g |= g >> 5;
	b |= b >> 5;

	/* set the color */
	palette_set_color(offset + 0x0000, r, g, b);
}



/******************************************************************************
	Sega 315-5296 I/O chip
*******************************************************************************

	Port A = player 1, port B = player 2, port C = the UPD7759 busy line,
	port D = the coin meters, port E = coins and service, ports F and G =
	the DIP switches, port H = the palette and UPD7759 sample banks. Poto
	Poto reads 'S' 'E' 'G' 'A' (SEGA) from this area as a form of protection.

******************************************************************************/

/* handle I/O chip reads */
static READ16_HANDLER( iochip_r )
{
	switch (offset)
	{
		case 0x00:	if (wheel_input)
					{
						/* the wheel: some mechanism makes the input trigger repeatedly */
						int wheel = (readinputport(5) & 3) | ((cpu_getcurrentframe() & 1) * 3);
						return 0xff00 | (readinputport(1) & 0x3f) | (wheel << 6);
					}
					return 0xff00 | readinputport(1);
		case 0x01:	return 0xff00 | readinputport(2);
		case 0x02:	if (sound_banks)
						return 0xff00 | (upd7759_0_busy_r(0) << 6) | 0xbf; /* must return high bit on */
					else
						return 0xffff;
		case 0x04:	return 0xff00 | readinputport(0);
		case 0x05:	return 0xff00 | readinputport(3);
		case 0x06:	return 0xff00 | readinputport(4);
		case 0x08:	return 0xff00 | 'S'; /* for Poto Poto */
		case 0x09:	return 0xff00 | 'E'; /* for Poto Poto */
		case 0x0a:	return 0xff00 | 'G'; /* for Poto Poto */
		case 0x0b:	return 0xff00 | 'A'; /* for Poto Poto */
		default:	return 0xff00 | iochip_reg[offset];
	}
	return 0xffff;
}


/* handle I/O chip writes */
static WRITE16_HANDLER( iochip_w )
{
	int newbank;

	/* only LSB matters */
	if (!ACCESSING_LSB)
		return;

	/* track what was written */
	iochip_reg[offset] = data;

	/* handle special writes */
	switch (offset)
	{
		case 0x03:
			/* Bits 0-1 = coin meters 1 and 2 */
			coin_counter_w(0, data & 0x01);
			coin_counter_w(1, data & 0x02);
			break;

		case 0x07:
			/* Bits 0-1 = master palette base - maps 1 of 4 1024-entry banks into paletteram */
			/* Bits 2-3 = UPD7759 sample bank - maps 1 of 4 128k banks into the UPD7759 space */
			/* Bit  4   = IRQ4 enable/acknowledge */
			/* Bit  5   = IRQ6 enable/acknowledge (?) */
			newbank = (data & 3) * 0x200;
			if (newbank != segavdp_get_palbank())
			{
				force_partial_update(segavdp_scanline() + 1 + segavdp_get_scanbase());
				segavdp_set_palbank(newbank);
			}
			if (sound_banks > 1)
			{
				newbank = (data >> 2) & (sound_banks - 1);
				upd7759_set_bank_base(0, newbank * 0x20000);
			}
			break;

		case 0x0e:
			/* Bit  1   = YM3438 reset? no - breaks poto poto */
/*			if (!(data & 2))
				YM2612_sh_reset();*/
			break;

		case 0x0f:
			/* ???? */
			if (data != 0x88)
				if (LOG_IOCHIP) log_cb(RETRO_LOG_DEBUG, LOGPRE "%06x:I/O write @ %02x = %02x\n", activecpu_get_previouspc(), offset, data & 0xff);
			break;

		default:
			if (LOG_IOCHIP) log_cb(RETRO_LOG_DEBUG, LOGPRE "%06x:I/O write @ %02x = %02x\n", activecpu_get_previouspc(), offset, data & 0xff);
			break;
	}
}



/******************************************************************************
	Control Write Handler
*******************************************************************************

	Seems to control some global states. The most important bit is the low
	one, which enables/disables the display. This is used while tiles are
	being modified in Bloxeed.

******************************************************************************/

static void set_alt_palette(int enable)
{
	if (enable != alt_palette_mode)
	{
		force_partial_update(segavdp_scanline() + 1 + segavdp_get_scanbase());
		alt_palette_mode = enable;
		segavdp_set_alt_palette(enable);
	}
}

static WRITE16_HANDLER( control_w )
{
	/* skip if not LSB */
	if (!ACCESSING_LSB)
		return;
	data &= 0x0f;

	/* bit 0 controls display enable */
	segavdp_enable_display(~data & 1);

	/* bit 1 resets the protection */
	if (!(data & 2))
		prot_write_buf = prot_read_buf = 0;

	/* bit 2 controls palette shuffling; only ribbit and twinsqua use this feature */
	set_alt_palette((~data & 4) >> 2);
}



/******************************************************************************
	Protection Read / Write Handlers
*******************************************************************************

	The protection chip is fairly simple. Writes to it control the palette
	banking for the sprites and backgrounds. The low 4 bits are also
	remembered in a 2-stage FIFO buffer. A read from this chip should
	return a value from a 256x4-bit table. The index into this table is
	computed by taking the second-to-last value written in the upper 4 bits,
	and the previously-fetched table value in the lower 4 bits.

******************************************************************************/

/* protection chip reads */
static READ16_HANDLER( prot_r )
{
	if (LOG_PROTECTION) log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:protection r=%02X\n", activecpu_get_previouspc(), prot_read_buf);
	return prot_read_buf | 0xf0;
}


/* protection chip writes */
static WRITE16_HANDLER( prot_w )
{
	int new_sp_palbase = ((data >> 2) & 3) * 0x40 + 0x100;
	int new_bg_palbase = (data & 3) * 0x40;
	int table_index;

	/* only works for the LSB */
	if (!ACCESSING_LSB)
		return;

	/* compute the table index */
	table_index = ((prot_write_buf << 4) | prot_read_buf) & 0xff;

	/* keep track of the last write for the next table lookup */
	prot_write_buf = data & 0x0f;

	/* determine the value to return, should a read occur */
	prot_read_buf = prot_func(table_index);
	if (LOG_PROTECTION) log_cb(RETRO_LOG_DEBUG, LOGPRE "%06X:protection w=%02X, new result=%02X\n", activecpu_get_previouspc(), data & 0x0f, prot_read_buf);

	/* if the palette changed, force an update */
	if (new_sp_palbase != segavdp_get_sp_palbase() || new_bg_palbase != segavdp_get_bg_palbase())
	{
		force_partial_update(segavdp_scanline() + 1 + segavdp_get_scanbase());
		segavdp_set_palbases(new_bg_palbase, new_sp_palbase);
		if (LOG_PALETTE) log_cb(RETRO_LOG_DEBUG, LOGPRE "Set palbank: %d/%d (scan=%d)\n", new_bg_palbase, new_sp_palbase, cpu_getscanline());
	}
}



/******************************************************************************
	Counter/timer I/O
*******************************************************************************

	There appears to be a chip that is used to count coins and track time
	played, or at the very least the current status of the game. All games
	except Puyo Puyo 2 and Poto Poto access this in a mostly consistent
	manner. The coin meters are on port D of the I/O chip.

******************************************************************************/

static WRITE16_HANDLER( counter_timer_w )
{
}



/******************************************************************************
	NVRAM Handling
*******************************************************************************

	There is a battery on the board that keeps the high scores, as in current
	MAME the whole work RAM is kept (0xff filled the first time: borencha has
	no sound otherwise, it lacks some of the init code of the borench set).

******************************************************************************/

static NVRAM_HANDLER( segac2 )
{
	if (read_or_write)
		mame_fwrite(file, main_ram, 0x10000);
	else if (file)
		mame_fread(file, main_ram, 0x10000);
	else
		memset(main_ram, 0xff, 0x10000);
}



/******************************************************************************
	Memory Maps
*******************************************************************************

	The System C/C2 68k Memory map is fairly similar to the Genesis in terms
	of RAM, ROM, VDP access locations, although the differences between the
	arcade system and the Genesis means its not same.

	As in current MAME the chips are mirrored: 0x800000-0x9fffff is decoded
	by A19-A18 and A9/A8, the VDP is mirrored up to 0xdfffff and the work RAM
	from 0xe00000 to 0xffffff.

******************************************************************************/

static READ16_HANDLER( io_space_r )
{
	offs_t a = 0x800000 + (offset << 1);

	switch ((a >> 18) & 3)
	{
		case 0:	/* 0x800000: protection / control */
			if (!(a & 0x200))
				return prot_r(0, mem_mask);
			break;

		case 1:	/* 0x840000: I/O chip / YM3438 */
			if (!(a & 0x100))
				return iochip_r((a >> 1) & 0x0f, mem_mask);
			return ym3438_r((a >> 1) & 0x03, mem_mask);

		case 3:	/* 0x8c0000: palette RAM */
			return palette_r((a >> 1) & 0x7ff, mem_mask);
	}
	return 0;
}

static WRITE16_HANDLER( io_space_w )
{
	offs_t a = 0x800000 + (offset << 1);

	switch ((a >> 18) & 3)
	{
		case 0:	/* 0x800000: protection / control */
			if (!(a & 0x200))
				prot_w(0, data, mem_mask);
			else
				control_w(0, data, mem_mask);
			break;

		case 1:	/* 0x840000: I/O chip / YM3438 */
			if (!(a & 0x100))
				iochip_w((a >> 1) & 0x0f, data, mem_mask);
			else
				ym3438_w((a >> 1) & 0x03, data, mem_mask);
			break;

		case 2:	/* 0x880000: UPD7759 / counter */
			if (!(a & 0x100))
				UPD7759_w(0, data, mem_mask);
			else
				counter_timer_w(0, data, mem_mask);
			break;

		case 3:	/* 0x8c0000: palette RAM */
			palette_w((a >> 1) & 0x7ff, data, mem_mask);
			break;
	}
}

/* the VDP mirrors */
static READ16_HANDLER( vdp_mirror_r )
{
	offs_t a = 0xc00000 + (offset << 1);

	if ((a & 0x0700e0) == 0)
		return segavdp_r((a >> 1) & 0x0f, mem_mask);
	return 0;
}

static WRITE16_HANDLER( vdp_mirror_w )
{
	offs_t a = 0xc00000 + (offset << 1);

	if ((a & 0x0700e0) == 0)
	{
		if (!(a & 0x10))
			segavdp_w((a >> 1) & 0x07, data, mem_mask);
		else if (!(a & 0x08))
			sn76489_w((a >> 1) & 0x03, data, mem_mask);
	}
}

/* the work RAM mirrors */
static READ16_HANDLER( ram_mirror_r )
{
	return main_ram[offset & 0x7fff];
}

static WRITE16_HANDLER( ram_mirror_w )
{
	COMBINE_DATA(&main_ram[offset & 0x7fff]);
}

static MEMORY_READ16_START( readmem )
	{ 0x000000, 0x1fffff, MRA16_ROM },					/* Main 68k Program Roms */
	{ 0x800000, 0x9fffff, io_space_r },					/* Protection, I/O chip, YM3438, palette RAM */
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xc00020, 0xdfffff, vdp_mirror_r },				/* VDP mirrors */
	{ 0xe00000, 0xe0ffff, MRA16_BANK1 },				/* Main Ram mirror */
	{ 0xe10000, 0xfeffff, ram_mirror_r },				/* Main Ram mirrors */
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( writemem )
	{ 0x000000, 0x1fffff, MWA16_ROM },					/* Main 68k Program Roms */
	{ 0x800000, 0x9fffff, io_space_w },					/* Protection, I/O chip, YM3438, UPD7759, palette RAM */
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xc00020, 0xdfffff, vdp_mirror_w },				/* VDP mirrors */
	{ 0xe00000, 0xe0ffff, MWA16_BANK1 },				/* Main Ram mirror */
	{ 0xe10000, 0xfeffff, ram_mirror_w },				/* Main Ram mirrors */
	{ 0xff0000, 0xffffff, MWA16_RAM, &main_ram },		/* Main Ram */
MEMORY_END


/******************************************************************************
	Protection
*******************************************************************************

	All of the Sega C/C2 games apart from Bloxeed used a protection chip.
	The games contain various checks which make sure this protection chip is
	present and returning the expected values. As in current MAME the EPM5032
	logic of each game is a function of the 8-bit table index.

	The basic operation in the EPM5032 macrocell (that is, without using the expander
	products) is:

	out = x1 XOR (x2 OR x3 OR x4)

	where x1, x2, x3, and x4 are the AND of any number of input bits. Each input can
	be inverted. The inputs come either from the external pins or from feedback from
	the output pins.

******************************************************************************/

static int prot_func_dummy(int in)
{
	return 0x0;
}

/* 317-0149 */
static int prot_func_columns(int in)
{
	int b0 = BIT(in, 2) ^ ((BIT(~in, 0) & BIT(in, 7)) | (BIT(in, 4) & BIT(in, 6)));
	int b1 = BIT(~in, 0) ^ (BIT(in, 2) | (BIT(in, 5) & BIT(~in, 6) & BIT(in, 7)));
	int b2 = BIT(in, 3) ^ ((BIT(in, 0) & BIT(in, 1)) | (BIT(in, 4) & BIT(in, 6)));
	int b3 = BIT(in, 1) ^ ((BIT(in, 0) & BIT(in, 1)) | (BIT(in, 4) & BIT(in, 5)) | (BIT(~in, 6) & BIT(in, 7)));   /* 1 repeated */

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0160 */
static int prot_func_columns2(int in)
{
	int b0 =  BIT(in, 2) ^ (BIT(in, 1) | (BIT(in, 4) & BIT(in, 5)));
	int b1 = (BIT(in, 0) & BIT(in, 3) & BIT(in, 4)) ^ (BIT(in, 6) | (BIT(in, 5) & BIT(in, 7)));
	int b2 = (BIT(in, 3) & BIT(~in, 2) & BIT(in, 4)) ^ (BIT(in, 5) | (BIT(in, 0) & BIT(in, 1)) | (BIT(in, 4) & BIT(in, 6))); /* 4 repeated */
	int b3 = (BIT(in, 1) & BIT(in, 0) & BIT(in, 2)) ^ ((BIT(in, 4) & BIT(~in, 6)) | (BIT(in, 6) & BIT(in, 7)));   /* 6 repeated */

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0172 */
static int prot_func_tfrceac(int in)
{
	int b0 = BIT(~in, 2) ^ ((BIT(in, 0) & BIT(~in, 7)) | (BIT(in, 3) & BIT(in, 4)));
	int b1 = (BIT(in, 4) & BIT(~in, 5) & BIT(in, 7)) ^ ((BIT(~in, 0) | BIT(~in, 3)) & (BIT(~in, 6) | BIT(~in, 7)));   /* not in the form x1 XOR (x2 OR x3 OR x4) */
	int b2 = BIT(in, 2) ^ ((BIT(in, 4) & BIT(~in, 5) & BIT(in, 7)) | (BIT(~in, 1) & BIT(in, 6)));
	int b3 = BIT(in, 0) ^ ((BIT(in, 1) & BIT(in, 4) & BIT(in, 6)) | (BIT(in, 1) & BIT(in, 4) & BIT(in, 7))); /* 1,4 repeated */

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0173 */
static int prot_func_borench(int in)
{
	int b0 = (BIT(in, 1) & BIT(in, 2) & BIT(in, 3) & BIT(in, 7))   ^ (BIT(in, 5) | (BIT(~in, 0) & BIT(~in, 4)));
	int b1 = (BIT(~in, 2) & BIT(in, 3) & BIT(in, 5))                ^ (BIT(in, 1) | (BIT(in, 0) & BIT(~in, 4)));
	int b2 = (BIT(in, 1) & BIT(~in, 4) & BIT(~in, 6))                ^ (BIT(in, 2) | BIT(in, 3) | (BIT(in, 5) & BIT(in, 7)));
	int b3 = (BIT(~in, 0) & BIT(in, 5) & (BIT(in, 6) | BIT(in, 7))) ^ (BIT(in, 1) | (BIT(in, 3) & BIT(in, 4)));   /* not in the form x1 XOR (x2 OR x3 OR x4) */

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0178 */
static int prot_func_ribbit(int in)
{
	int b0 = (BIT(in, 0) & BIT(in, 4)) ^ ((BIT(in, 1) & BIT(in, 2)) | BIT(in, 3) | BIT(~in, 5));
	int b1 = (BIT(in, 1) & BIT(in, 5)) ^ ((BIT(in, 2) & BIT(in, 3)) | BIT(in, 0) | BIT(~in, 6));
	int b2 = (BIT(in, 2) & BIT(in, 7)) ^ ((BIT(in, 3) & BIT(in, 0)) | BIT(~in, 1) | BIT(in, 7));
	int b3 = (BIT(in, 3) & BIT(in, 6)) ^ ((BIT(in, 0) & BIT(in, 1)) | BIT(~in, 2) | BIT(in, 4));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0193 */
static int prot_func_twinsqua(int in)
{
	int b0 = (BIT(in, 2) & BIT(~in, 5)) ^ (BIT(in, 3) | BIT(~in, 4));
	int b1 = (BIT(in, 0) & BIT(~in, 2) & BIT(in, 4)) ^ (BIT(~in, 0) | BIT(~in, 4) | BIT(~in, 6)); /* 0,4 repeated */
	int b2 = (BIT(in, 3) & BIT(~in, 5)) ^ (BIT(in, 4) & BIT(in, 7));
	int b3 =  BIT(in, 1) ^ ((BIT(~in, 3) & BIT(~in, 6)) | (BIT(in, 4) & BIT(~in, 6)) | (BIT(~in, 1) & BIT(in, 3) & BIT(~in, 4)));    /* 1,3,4,6 repeated */

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0203 */
static int prot_func_puyo(int in)
{
	int b0 = (BIT(~in, 3) & BIT(in, 7)) ^ ((BIT(~in, 0) & BIT(~in, 1)) | (BIT(~in, 1) & BIT(~in, 4))); /* 1 repeated */
	int b1 = (BIT(in, 3) & BIT(in, 5)) ^ (BIT(~in, 2) | BIT(in, 4) | BIT(in, 6));
	int b2 = (BIT(~in, 2) & BIT(~in, 5)) ^ (BIT(in, 1) | BIT(~in, 3) | BIT(~in, 6));
	int b3 =  BIT(in, 1)               ^ ((BIT(in, 0) & BIT(in, 3) & BIT(in, 7)) | BIT(in, 4));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0211 */
static int prot_func_tantr(int in)
{
	int b0 = (BIT(in, 0) & BIT(in, 4)) ^ ( BIT(in, 5) | BIT(~in, 6)  | (BIT(~in, 3) & BIT(in, 7)));
	int b1 = (BIT(in, 2) & BIT(in, 6)) ^ ((BIT(in, 1) & BIT(in, 5)) | (BIT(in, 3) & BIT(in, 4)));
	int b2 = (BIT(~in, 0) & BIT(in, 2)) ^ ( BIT(in, 4) | BIT(in, 7)  | (BIT(in, 1) & BIT(~in, 5)));
	int b3 = (BIT(~in, 2) & BIT(in, 7)) ^ ( BIT(~in, 0) | BIT(in, 1)  | (BIT(in, 3) & BIT(in, 6)));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-???? */
static int prot_func_tantrkor(int in)
{
	int b0 = (BIT(~in, 1) & BIT(~in, 7)) ^ (BIT(~in, 2) & BIT(~in, 4));
	int b1 = (BIT(in, 2) & BIT(in, 6)) ^ (BIT(in, 0) & BIT(in, 1));
	int b2 = (BIT(~in, 3) & BIT(~in, 6)) ^ (BIT(in, 1) | BIT(~in, 4));
	int b3 = (BIT(~in, 0) & BIT(~in, 2)) ^ (BIT(in, 5) & BIT(~in, 6));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0218 */
static int prot_func_potopoto(int in)
{
	int b0 = (BIT(~in, 2) & BIT(~in, 4)) ^ (BIT(~in, 1) & BIT(in, 3));
	int b1 = (BIT(in, 0) & BIT(in, 5)) ^ (BIT(in, 2) | BIT(~in, 7));
	int b2 = (BIT(in, 0) & BIT(in, 6)) ^ (BIT(~in, 1) & BIT(in, 7));
	int b3 = (BIT(in, 0) & BIT(~in, 7)) ^ (BIT(~in, 1) & BIT(~in, 6));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0219 */
static int prot_func_stkclmnj(int in)
{
	int b0 = (BIT(in, 1) & BIT(in, 4)) ^ (BIT(in, 5) & BIT(in, 2));
	int b1 = (BIT(~in, 2) & BIT(in, 6)) ^ (BIT(~in, 5) & BIT(in, 7));
	int b2 = (BIT(~in, 3) & BIT(in, 6)) ^ (BIT(~in, 5) & BIT(~in, 1));
	int b3 = (BIT(~in, 3) & BIT(in, 5)) ^ (BIT(~in, 6) | BIT(~in, 7));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0223 */
static int prot_func_stkclmns(int in)
{
	int b0 = (BIT(in, 2) & BIT(in, 4)) ^ (BIT(in, 1) | BIT(~in, 3));
	int b1 = (BIT(in, 0) & BIT(in, 5)) ^ (BIT(in, 2) & BIT(in, 7));
	int b2 = (BIT(in, 0) & BIT(~in, 6)) ^ (BIT(in, 1) & BIT(~in, 7));
	int b3 = (BIT(in, 0) & BIT(~in, 7)) ^ (BIT(~in, 1) & BIT(in, 6));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0224 */
static int prot_func_ichirj(int in)
{
	int b0 = (BIT(in, 2) & BIT(in, 4)) ^ (BIT(~in, 5) & BIT(~in, 2));
	int b1 = (BIT(in, 2) & BIT(~in, 6)) ^ (BIT(in, 5) & BIT(in, 7));
	int b2 = (BIT(~in, 3) & BIT(in, 6)) ^ (BIT(~in, 5) & BIT(~in, 1));
	int b3 = (BIT(~in, 1) & BIT(in, 5)) ^ (BIT(~in, 5) & BIT(in, 7));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-???? */
static int prot_func_ichir(int in)
{
	int b0 = (BIT(~in, 2) & BIT(in, 4)) ^ (BIT(in, 5) & BIT(~in, 2));
	int b1 = (BIT(in, 1) & BIT(in, 6)) ^ (BIT(in, 5) | BIT(in, 7));
	int b2 = (BIT(~in, 3) & BIT(in, 6)) ^ (BIT(~in, 5) & BIT(~in, 3));
	int b3 = (BIT(in, 0) & BIT(~in, 5)) ^ (BIT(in, 5) & BIT(in, 7));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-???? */
static int prot_func_ichirk(int in)
{
	int b0 = (BIT(~in, 2) & BIT(in, 4)) ^ (BIT(in, 5) & BIT(~in, 1));
	int b1 = (BIT(in, 0) & BIT(in, 6)) ^ (BIT(in, 5) & BIT(in, 4));
	int b2 = (BIT(~in, 1) & BIT(~in, 6)) ^ (BIT(~in, 5) & BIT(in, 3));
	int b3 = (BIT(in, 1) & BIT(in, 5)) ^ (BIT(in, 6) & BIT(in, 7));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/* 317-0228 */
static int prot_func_puyopuy2(int in)
{
	int b0 = (BIT(~in, 0) & BIT(~in, 7)) ^ (BIT(in, 1) | BIT(~in, 4) | BIT(~in, 6));
	int b1 = (BIT(in, 0) & BIT(~in, 6)) ^ (BIT(in, 3) & BIT(in, 5));
	int b2 = (BIT(~in, 4) & BIT(~in, 7)) ^ (BIT(in, 0) | BIT(~in, 6));
	int b3 = (BIT(~in, 1) & BIT(in, 4)) ^ (BIT(in, 2) & BIT(~in, 3));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

static int prot_func_zunkyou(int in)
{
	int b0 = (BIT(~in, 1) & BIT(in, 6)) ^ (BIT(~in, 5) & BIT(in, 7));
	int b1 = (BIT(in, 0) & BIT(~in, 5)) ^ (BIT(~in, 3) | BIT(in, 4));
	int b2 = (BIT(in, 2) & BIT(~in, 3)) ^ (BIT(in, 4) & BIT(~in, 5));
	int b3 = (BIT(in, 0) & BIT(~in, 4)) ^ (BIT(~in, 2) & BIT(~in, 6));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

static int prot_func_pclub(int in)
{
	return 0xf;
}

static int prot_func_pclubjv2(int in)
{
	int b0 = (BIT(in, 3) & BIT(~in, 4)) ^ ((BIT(~in, 1) & BIT(~in, 7)) | BIT(in, 6));
	int b1 = (BIT(in, 0) & BIT(in, 5)) ^  (BIT(in, 2) & BIT(~in, 6));
	int b2 = (BIT(~in, 1) & BIT(in, 6)) ^  (BIT(in, 3) | BIT(~in, 5)  | BIT(~in, 1)); /* 1 repeated */
	int b3 = (BIT(~in, 2) & BIT(~in, 7)) ^  (BIT(~in, 0) | BIT(~in, 4));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

static int prot_func_pclubjv3(int in)
{
	/* TODO: Determine correctly. This is just copied from V2 as a placeholder. */
	int b0 = (BIT(in, 3) & BIT(~in, 4)) ^ ((BIT(~in, 1) & BIT(~in, 7)) | BIT(in, 6));
	int b1 = (BIT(in, 0) & BIT(in, 5)) ^  (BIT(in, 2) & BIT(~in, 6));
	int b2 = (BIT(~in, 1) & BIT(in, 6)) ^  (BIT(in, 3) | BIT(~in, 5)  | BIT(~in, 1)); /* 1 repeated */
	int b3 = (BIT(~in, 2) & BIT(~in, 7)) ^  (BIT(~in, 0) | BIT(~in, 4));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

static int prot_func_pclubjv4(int in)
{
	int b0 = (BIT(~in, 2) & BIT(in, 4)) ^ (BIT(in, 1) & BIT(~in, 6) & BIT(~in, 3));
	int b1 = (BIT(~in, 3) & BIT(~in, 4)) ^ (BIT(in, 0) & BIT(in, 5) & BIT(~in, 6));
	int b2 =  BIT(~in, 0)               ^ (BIT(in, 3) & BIT(in, 4));
	int b3 = (BIT(~in, 1) & BIT(in, 7)) ^ (BIT(in, 5) & BIT(~in, 7)); /* 7 repeated */

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

static int prot_func_pclubjv5(int in)
{
	int b0 = (BIT(~in, 1) & BIT(in, 5)) ^ (BIT(~in, 2) & BIT(~in, 6));
	int b1 = (BIT(~in, 0) & BIT(in, 4)) ^ (BIT(~in, 3) & BIT(~in, 7));
	int b2 = (BIT(~in, 3) & BIT(in, 7)) ^ (BIT(~in, 0) | BIT(~in, 4));
	int b3 = (BIT(~in, 2) & BIT(in, 6)) ^ (BIT(~in, 1) & BIT(~in, 5));

	return (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
}

/******************************************************************************
	Input Ports
*******************************************************************************

	The input ports on the C2 games always consist of 1 Coin Port, 2 Player
	Input ports and 2 Dipswitch Ports, 1 of those Dipswitch Ports being used
	for coinage, the other for Game Options. As in current MAME.

******************************************************************************/

#define COIN_A \
    PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) ) \
    PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) ) \
    PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) ) \
    PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) ) \
    PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 5/3 6/4" ) \
    PORT_DIPSETTING(    0x04, "2 Coins/1 Credit, 4/3" ) \
    PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) ) \
    PORT_DIPSETTING(    0x03, "1 Coin/1 Credit, 5/6" ) \
    PORT_DIPSETTING(    0x02, "1 Coin/1 Credit, 4/5" ) \
    PORT_DIPSETTING(    0x01, "1 Coin/1 Credit, 2/3" ) \
    PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) ) \
    PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) ) \
    PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) ) \
    PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) ) \
    PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) ) \
    PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) ) \
    PORT_DIPSETTING(    0x00, "1 Coin/1 Credit (Freeplay if Coin B also)" )

#define COIN_B \
    PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) ) \
    PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) ) \
    PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) ) \
    PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) ) \
    PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 5/3 6/4" ) \
    PORT_DIPSETTING(    0x40, "2 Coins/1 Credit, 4/3" ) \
    PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) ) \
    PORT_DIPSETTING(    0x30, "1 Coin/1 Credit, 5/6" ) \
    PORT_DIPSETTING(    0x20, "1 Coin/1 Credit, 4/5" ) \
    PORT_DIPSETTING(    0x10, "1 Coin/1 Credit, 2/3" ) \
    PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) ) \
    PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) ) \
    PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) ) \
    PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) ) \
    PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) ) \
    PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x00, "1 Coin/1 Credit (Freeplay if Coin A also)" )


INPUT_PORTS_START( bloxeedc )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start (VS)" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPNAME( 0x02, 0x02, "Credits to Start (Normal)" )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "High Speed Mode" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( bloxeedu )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x03, "Credits to Start (Normal / VS)" )
	PORT_DIPSETTING(    0x01, "1 / 1" )
	PORT_DIPSETTING(    0x03, "1 / 2" )
	PORT_DIPSETTING(    0x02, "1 / 2" )
	PORT_DIPSETTING(    0x00, "2 / 4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "High Speed Mode" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( columns )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easiest" )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( columnsu )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Background Music" )
	PORT_DIPSETTING(    0x04, "BGM #1" )
	PORT_DIPSETTING(    0x00, "BGM #2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easiest" )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( columns2 )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "VS. Mode Credits/Match" )
	PORT_DIPSETTING(    0x0c, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x30, 0x30, "Flash Mode Difficulty" )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( tfrceac )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "10k, 70k, 150k" )
	PORT_DIPSETTING(    0x30, "20k, 100k, 200k" )
	PORT_DIPSETTING(    0x20, "40k, 150k, 300k" )
	PORT_DIPSETTING(    0x00, DEF_STR( None ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, "Medium" )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
INPUT_PORTS_END

INPUT_PORTS_START( borench )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Lives 1P Mode" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPNAME( 0x30, 0x30, "Lives 2P Mode" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, "Medium" )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
INPUT_PORTS_END

INPUT_PORTS_START( ribbit )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ribbitj )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( twinsqua )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_START	/* P1 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 30, 15, 0, 0 )
	PORT_START	/* P2 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER2, 30, 15, 0, 0 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Buy In" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x18, "Medium" )
	PORT_DIPSETTING(    0x08, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x20, 0x20, "Seat Type" )
	PORT_DIPSETTING(    0x20, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x00, "Moving" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( soniccar )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x03, "Demo Sound Interval" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, "Every 4 Minutes" )
	PORT_DIPSETTING(    0x02, "Every 2 Minutes" )
	PORT_DIPSETTING(    0x03, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Lighting Time" )
	PORT_DIPSETTING(    0x04, "Advertise & Playtime" )
	PORT_DIPSETTING(    0x00, "Playtime Only" )
	PORT_DIPNAME( 0x08, 0x08, "Light" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Screen Display" )
	PORT_DIPSETTING(    0x10, "Insert 100-400 Yen" )
	PORT_DIPSETTING(    0x00, "Insert Money" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ssonicbr )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x06, "Medium" )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ooparts )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x06, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x18, "Medium" )
	PORT_DIPSETTING(    0x08, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x60, 0x60, "Region" )
	PORT_DIPSETTING(    0x60, "Japan" )
	PORT_DIPSETTING(    0x40, "USA" )
	PORT_DIPSETTING(    0x20, "Export" )
	PORT_DIPSETTING(    0x00, "Export" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( puyo )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "VS. Mode Credits/Match" )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x18, 0x18, "1P Mode Difficulty" )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x18, "Medium" )
	PORT_DIPSETTING(    0x08, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Moving Seat" )
	PORT_DIPSETTING(    0x80, "No Use" )
	PORT_DIPSETTING(    0x00, "In Use" )
INPUT_PORTS_END

INPUT_PORTS_START( ichir )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x06, "Medium" )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( wwmarine )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0xc0, 0x00, IPT_UNUSED )
	PORT_START	/* P2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x03, "Demo Sound Interval" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, "Every 3 Demo Cycles" )
	PORT_DIPSETTING(    0x02, "Every 2 Demo Cycles" )
	PORT_DIPSETTING(    0x03, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Capsule Mode" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Credit Mode" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_START	/* WHEEL */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
INPUT_PORTS_END

INPUT_PORTS_START( wwjumbo )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* COINAGE */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x03, "Demo Sound Interval" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, "Every 4 Minutes" )
	PORT_DIPSETTING(    0x02, "Every 2 Minutes" )
	PORT_DIPSETTING(    0x03, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Lighting Time" )
	PORT_DIPSETTING(    0x04, "Playtime Only" )
	PORT_DIPSETTING(    0x00, "Advertise & Playtime" )
	PORT_DIPNAME( 0x08, 0x08, "Light" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Screen Display" )
	PORT_DIPSETTING(    0x10, "Insert 100-400 Yen" )
	PORT_DIPSETTING(    0x00, "Insert Money" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( wwpajero )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x03, "Demo Sound Interval" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, "Every 8 Demo Cycles" )
	PORT_DIPSETTING(    0x02, "Every 4 Demo Cycles" )
	PORT_DIPSETTING(    0x03, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( anpanman )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* P1 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3, "Relay", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON4, "Coinblock", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_BUTTON5, "Sensor", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_START2, "Cup Select 2", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_START1, "Cup Select 1", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2, "Handle B", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1, "Handle A", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_START	/* P2 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_BUTTON9, "Sold Out LED1", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON10, "Sold Out LED2", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3, "Sold Out LED3", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON7, "Sold Out SW2", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_BUTTON8, "Sold Out SW3", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6, "Sold Out SW1", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPNAME( 0x02, 0x02, "Demo Voice" )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( sonicpop )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* P1 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3, "Relay", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON4, "Coinblock", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_BUTTON5, "Sensor", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_START2, "Cup Select 2", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_START1, "Cup Select 1", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2, "Handle B", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1, "Handle A", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_START	/* P2 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_BUTTON9, "Sold Out LED1", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON10, "Sold Out LED2", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3, "Sold Out LED3", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON7, "Sold Out SW2", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_BUTTON8, "Sold Out SW3", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6, "Sold Out SW1", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPNAME( 0x02, 0x02, "Trouble BGM" )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPNAME( 0x0c, 0x00, "Region" )
	PORT_DIPSETTING(    0x00, "USA" )
	PORT_DIPSETTING(    0x04, "Export" )
	PORT_DIPSETTING(    0x08, "USA (duplicate)" )
	PORT_DIPSETTING(    0x0c, "Japan" )
	PORT_DIPNAME( 0x10, 0x10, "Sensor Advertise" )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( sonicfgt )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0xdf, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credit Mode" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( potopoto )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Credits to Start" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Chute Type" )
	PORT_DIPSETTING(    0x04, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x08, 0x08, "Credits to Continue" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x10, 0x10, "Buy-In" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x60, "Medium" )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x80, 0x80, "Moving Seat" )
	PORT_DIPSETTING(    0x80, "No Use" )
	PORT_DIPSETTING(    0x00, "In Use" )
INPUT_PORTS_END

INPUT_PORTS_START( stkclmns )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Match Mode Price" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( puyopuy2 )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Rannyu Off Button" )
	PORT_DIPSETTING(    0x01, "Use" )
	PORT_DIPSETTING(    0x00, "No Use" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Turn Direction" )
	PORT_DIPSETTING(    0x04, "1:Right  2:Left" )
	PORT_DIPSETTING(    0x00, "1:Left  2:Right" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x18, "Medium" )
	PORT_DIPSETTING(    0x08, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x60, 0x60, "VS Mode Match/1 Play" )
	PORT_DIPSETTING(    0x60, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x80, 0x80, "Battle Start credit" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x80, "2" )
INPUT_PORTS_END

INPUT_PORTS_START( zunkyou )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "Game Difficulty 1" )
	PORT_DIPSETTING(    0x01, "Medium" )
	PORT_DIPSETTING(    0x00, DEF_STR( Hard ) )
	PORT_DIPNAME( 0x02, 0x02, "Game Difficulty 2" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x00, DEF_STR( Hard ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( headonch )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_START	/* COINAGE */
	COIN_A
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x06, "Medium" )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x18, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( pclub )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, "Ok", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2, "Cancel", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* COINAGE */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( pclubjv2 )
	PORT_START	/* SERVICE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* P1 */
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, "Ok", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2, "Cancel", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_START	/* P2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_START	/* COINAGE */
	PORT_DIPNAME( 0x07, 0x07, "Coins per Credit (Normal / Alternate)" )
	PORT_DIPSETTING(    0x00, "25 / 30" )
	PORT_DIPSETTING(    0x01, "6 / 7" )
	PORT_DIPSETTING(    0x02, "10 / 12" )
	PORT_DIPSETTING(    0x03, "1 / 2" )
	PORT_DIPSETTING(    0x07, "3 / Free Play" )
	PORT_DIPSETTING(    0x04, "15 / 20" )
	PORT_DIPSETTING(    0x05, "4 / 5" )
	PORT_DIPSETTING(    0x06, "8 / 9" )
	PORT_DIPNAME( 0x08, 0x08, "Alternate Coinage" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	COIN_B
	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


/******************************************************************************
	Sound interfaces
******************************************************************************/

static struct upd7759_interface upd7759_intf =
{
	1,								/* One chip */
	{ XL1_CLOCK },
	{ 50 },							/* Volume */
	{ REGION_SOUND1 },				/* Memory pointer (gen.h) */
};

static struct YM2612interface ym3438_intf =
{
	1,								/* One chip */
	XL2_CLOCK/7,					/* Clock: 7.67 MHz */
	{ YM3012_VOL(50,MIXER_PAN_CENTER,50,MIXER_PAN_CENTER) },	/* Volume */
	{ 0 },							/* port I/O */
	{ 0 },							/* port I/O */
	{ 0 },							/* port I/O */
	{ 0 },							/* port I/O */
	{ megadriv_ym3438_irq }		/* IRQ handler */
};

static struct SN76496interface sn76489_intf =
{
	1,								/* One chip */
	{ XL2_CLOCK/15 },				/* Clock: 3.58 MHz */
	{ 50 }							/* Volume */
};



/******************************************************************************
	Machine Drivers
*******************************************************************************

	General Overview
		M68000 @ 8.95MHz (Main Processor)
		YM3438 (Fm Sound)
		SN76489 (PSG, Noise, Part of the VDP)
		UPD7759 (Sample Playback, C-2 Only)

******************************************************************************/

static MACHINE_DRIVER_START( segac )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main",M68000, XL2_CLOCK/6)
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(genesis_vblank_interrupt,1)

	MDRV_FRAMES_PER_SECOND((double)XL2_CLOCK / 10.0 / 262.0 / 342.0)	/* 59.92 Hz, as in current MAME */
	MDRV_VBLANK_DURATION((int)(((262. - 224.) / 262.) * 1000000. / ((double)XL2_CLOCK / 10.0 / 262.0 / 342.0)))

	MDRV_MACHINE_INIT(segac2)
	MDRV_NVRAM_HANDLER(segac2)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS)
	MDRV_SCREEN_SIZE(320,224)
	MDRV_VISIBLE_AREA(0, 319, 0, 223)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(segac2)
	MDRV_VIDEO_EOF(segac2)
	MDRV_VIDEO_UPDATE(segac2)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2612, ym3438_intf)
	MDRV_SOUND_ADD(SN76496, sn76489_intf)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( segac2 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( segac )

	/* sound hardware */
	MDRV_SOUND_ADD(UPD7759, upd7759_intf)
MACHINE_DRIVER_END


/* Ribbit! checks how long the UPD7759 takes to play a sample (current MAME
   delays the start of the playback) */
static MACHINE_DRIVER_START( ribbit )

	MDRV_IMPORT_FROM( segac2 )
MACHINE_DRIVER_END


/* Print Club boards use a different crystal, possibly for better
   compatibility with the camera timings */
static MACHINE_DRIVER_START( pclub )

	MDRV_IMPORT_FROM( segac2 )
	MDRV_CPU_REPLACE("main", M68000, XL2_CLOCK_PCLUB/6)
MACHINE_DRIVER_END



/******************************************************************************
	Rom Definitions
*******************************************************************************

	Games are in Order of Date (Year) with System-C titles coming first.

******************************************************************************/

ROM_START( bloxeedc ) /* Bloxeed (C System Version)  (c)1989 Sega / Elorg */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-12908.ic32", 0x000000, 0x020000, CRC(fc77cb91) SHA1(248a462e3858ffdc171af7d806e57deecb5dae50) )
	ROM_LOAD16_BYTE( "epr-12907.ic31", 0x000001, 0x020000, CRC(e5fcbac6) SHA1(a1adec5ef5574bff96a3d66619a24a6715097bb9) )
	ROM_LOAD16_BYTE( "epr-12993.ic34", 0x040000, 0x020000, CRC(487bc8fc) SHA1(3fb205bf56f35443e993e08b39c1a08c13ca5e3b) )
	ROM_LOAD16_BYTE( "epr-12992.ic33", 0x040001, 0x020000, CRC(19b0084c) SHA1(b3ba0f3d8d39a19aa66edb24885ea21192e22704) )
ROM_END

ROM_START( bloxeedu ) /* Bloxeed USA (C System Version)  (c)1989 Sega / Elorg - 834-7306-01 BLOXEED Rev.A */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-12997a.ic32", 0x000000, 0x020000, CRC(23655bc9) SHA1(32fc1f75a43aa49dc656d40d34ec10f3f0a2bdb3) )
	ROM_LOAD16_BYTE( "epr-12996a.ic31", 0x000001, 0x020000, CRC(83c83f0c) SHA1(ca8e2ad7cceabd8de7a91b91cb92eafb6dd3171f) )
	ROM_LOAD16_BYTE( "epr-12993.ic34",  0x040000, 0x020000, CRC(487bc8fc) SHA1(3fb205bf56f35443e993e08b39c1a08c13ca5e3b) )
	ROM_LOAD16_BYTE( "epr-12992.ic33",  0x040001, 0x020000, CRC(19b0084c) SHA1(b3ba0f3d8d39a19aa66edb24885ea21192e22704) )

ROM_END

ROM_START( columns ) /* Columns (World) (c)1990 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13114.ic32", 0x000000, 0x020000, CRC(ff78f740) SHA1(0a034103a4b942f43e62f6e717f5dbf1bfb0b613) )
	ROM_LOAD16_BYTE( "epr-13113.ic31", 0x000001, 0x020000, CRC(9a426d9b) SHA1(3322e65ebf8d0a6047f7d408387c63ea401b8973) )
ROM_END

ROM_START( columnsu ) /* Columns (US) (c)1990 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13116a.ic32", 0x000000, 0x020000, CRC(a0284b16) SHA1(a72c8567ab2386ef6bc7bb83cc1612f4c6bf8461) )
	ROM_LOAD16_BYTE( "epr-13115a.ic31", 0x000001, 0x020000, CRC(e37496f3) SHA1(30ebeed76613ae8d6d3ce9fca282124685067b27) )
ROM_END

ROM_START( columnsj ) /* Columns (Jpn) (c)1990 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13112.ic32", 0x000000, 0x020000, CRC(bae6e53e) SHA1(2c2fd621eecd55591f22d076323972a7d0314615) )
	ROM_LOAD16_BYTE( "epr-13111.ic31", 0x000001, 0x020000, CRC(aa5ccd6d) SHA1(480e29e3112282d1790f1fb68075453325ba4336) )
ROM_END

ROM_START( columns2 ) /* Columns II - The Voyage Through Time  (c)1990 Sega - 834-7555-01 COLUMNS 2 */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13363.ic32", 0x000000, 0x020000, CRC(c99e4ffd) SHA1(67981aa08c8a625af35dd7689011364159cf9194) )
	ROM_LOAD16_BYTE( "epr-13362.ic31", 0x000001, 0x020000, CRC(394e2419) SHA1(d4f726b32cf301d0d52611237b83177e69bfaf71) )
ROM_END

ROM_START( column2j ) /* Columns II - The Voyage Through Time (Jpn)  (c)1990 Sega - 834-7555 COLUMNS 2 */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13361.ic32", 0x000000, 0x020000, CRC(b54b5f12) SHA1(4d7fbae7d9bcadd433ebc25aef255dc43df611bc) )
	ROM_LOAD16_BYTE( "epr-13360.ic31", 0x000001, 0x020000, CRC(a59b1d4f) SHA1(e9ee315677782e1c61ae8f11260101cc03176188) )
ROM_END

ROM_START( tantrbl2 ) /* Tant-R (Puzzle & Action) (Alt Bootleg Running on C Board?, No Samples) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "trb2_2.32",       0x000000, 0x080000, CRC(8fc99c48) SHA1(d90ed673fe1f6e1f878c0d8fc62f5439b56d0a47) )
	ROM_LOAD16_BYTE( "trb2_1.31",       0x000001, 0x080000, CRC(c318d00d) SHA1(703760d4ddc45bc0921ae96a27d9a8fbf12a1e96) )
	ROM_LOAD16_BYTE( "mpr-15616.ic34",  0x100000, 0x080000, CRC(17b80202) SHA1(f47bf2aa0c5972647438619b8453c7dede5c422f) )
	ROM_LOAD16_BYTE( "mpr-15615.ic33",  0x100001, 0x080000, CRC(36a88bd4) SHA1(cc7f6a947d1b79bb86957c43035b53d6d2bcfa28) )
ROM_END

ROM_START( tantrbl3 ) /* Tant-R (Puzzle & Action) (Alt Bootleg Running on C Board?, No Samples) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "2.u29",  0x000000, 0x080000, CRC(faecb7b1) SHA1(ddca61996dbd0d66e9ecc04647e61a6d0af25807) )
	ROM_LOAD16_BYTE( "1.u28",  0x000001, 0x080000, CRC(3304556d) SHA1(586265c846bf6404dcb33404e961bf0733e24bf5) )
	ROM_LOAD16_BYTE( "4.u31",  0x100000, 0x080000, CRC(17b80202) SHA1(f47bf2aa0c5972647438619b8453c7dede5c422f) )
	ROM_LOAD16_BYTE( "3.u30",  0x100001, 0x080000, CRC(36a88bd4) SHA1(cc7f6a947d1b79bb86957c43035b53d6d2bcfa28) )
ROM_END

ROM_START( ichirjbl ) /* Ichidant-R (Puzzle & Action 2) (Bootleg Running on C Board?, No Samples) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "27c4000.2",0x000000, 0x080000, CRC(5a194f44) SHA1(67a4d21b91704f8c2210b5106e82e22ba3366f4c) )
	ROM_LOAD16_BYTE( "27c4000.1",0x000001, 0x080000, CRC(de209f84) SHA1(0860d0ebfab2952e82fc1e292bf9410d673d9322) )
	ROM_LOAD16_BYTE( "epr-16888", 0x100000, 0x080000, CRC(85d73722) SHA1(7ebe81b4d6c89f87f60200a3a8cddb07d581adef) )
	ROM_LOAD16_BYTE( "epr-16887", 0x100001, 0x080000, CRC(bc3bbf25) SHA1(e760ad400bc183b38e9787d88c8ac084fbe2ae21) )
ROM_END

ROM_START( borench ) /* Borench  (c)1990 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ic32.bin", 0x000000, 0x040000, CRC(2c54457d) SHA1(adf3ea5393d2633ec6215e64f0cd89ad4567e765) ) /* Need to verify proper label - EPR-13589? */
	ROM_LOAD16_BYTE( "ic31.bin", 0x000001, 0x040000, CRC(b46445fc) SHA1(24e85ef5abbc5376a854b13ed90f08f0c30d7f25) ) /* Need to verify proper label - EPR-13588? */

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13587.ic4", 0x000000, 0x020000, CRC(62b85e56) SHA1(822ab733c87938bb70a9e32cc5dd36bbf6f21d11) )
ROM_END

ROM_START( borencha ) /* Borench  (c)1990 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13591.ic32", 0x000000, 0x040000, CRC(7851078b) SHA1(122934f0414a29b4b363acad01ee4db369259e72) )
	ROM_LOAD16_BYTE( "epr-13590.ic31", 0x000001, 0x040000, CRC(01bc6fe6) SHA1(b241b09852f52f712e3ddc6660ec3eb436b1302c) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13587.ic4", 0x000000, 0x020000, CRC(62b85e56) SHA1(822ab733c87938bb70a9e32cc5dd36bbf6f21d11) )
ROM_END

ROM_START( borenchj ) /* Borench  (c)1990 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13586.ic32", 0x000000, 0x040000, CRC(62d7f8e8) SHA1(a2b11584c79ead70e6b8cab0b076df9bbf114803) )
	ROM_LOAD16_BYTE( "epr-13585.ic31", 0x000001, 0x040000, CRC(087b9704) SHA1(1e974d1e40ea28e8f5e721ccf78a2ffc76f4d17d) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13587.ic4", 0x000000, 0x020000, CRC(62b85e56) SHA1(822ab733c87938bb70a9e32cc5dd36bbf6f21d11) )
ROM_END

ROM_START( tfrceac ) /* Thunder Force AC  (c)1990 Technosoft / Sega - 834-7745-02 THUNDER FORCE AC (EMP5032 labeled 317-0172) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13675.ic32", 0x000000, 0x040000, CRC(95ecf202) SHA1(92b0f351f2bee7d59873a4991615f14f1afe4da7) )
	ROM_LOAD16_BYTE( "epr-13674.ic31", 0x000001, 0x040000, CRC(e63d7f1a) SHA1(a40d0a5a96f379a467048dc8fddd8aaaeb94da1d) )
	/* 0x080000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "epr-13659.ic34", 0x100000, 0x040000, CRC(29f23461) SHA1(032a7125fef5a660b85654d595aafc46812cdde6) )
	ROM_LOAD16_BYTE( "epr-13658.ic33", 0x100001, 0x040000, CRC(9e23734f) SHA1(64d27dc53f0ffc3513345a26ed077751b25d15f1) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13655.ic4", 0x000000, 0x040000, CRC(e09961f6) SHA1(e109b5f41502b765d191f22e3bbcff97d6defaa1) )
ROM_END

ROM_START( tfrceacj ) /* Thunder Force AC (Japan)  (c)1990 Technosoft / Sega - 834-7745 THUNDER FORCE AC (EMP5032 labeled 317-0172) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13657.ic32", 0x000000, 0x040000, CRC(a0f38ffd) SHA1(da548e7f61aed0e82a460553a119941da8857bc4) )
	ROM_LOAD16_BYTE( "epr-13656.ic31", 0x000001, 0x040000, CRC(b9438d1e) SHA1(598209c9fec3527fde720af09e5bebd7379f5b2b) )
	/* 0x080000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "epr-13659.ic34", 0x100000, 0x040000, CRC(29f23461) SHA1(032a7125fef5a660b85654d595aafc46812cdde6) )
	ROM_LOAD16_BYTE( "epr-13658.ic33", 0x100001, 0x040000, CRC(9e23734f) SHA1(64d27dc53f0ffc3513345a26ed077751b25d15f1) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13655.ic4", 0x000000, 0x040000, CRC(e09961f6) SHA1(e109b5f41502b765d191f22e3bbcff97d6defaa1) )
ROM_END

ROM_START( tfrceacjpb ) /* protection chip simply marked T-FORCE (not used outside of startup init?) */
	ROM_REGION( 0x200000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_BYTE( "ic32_t.f.ac_075f.ic32", 0x000000, 0x040000, CRC(2167dd93) SHA1(0e5b8eb87e07e6e5cecf096e6b62e15ff7406bba) )
	ROM_LOAD16_BYTE( "ic31_t.f.ac_0d26.id31", 0x000001, 0x040000, CRC(ebf02bba) SHA1(effdc60837063ba04b7b4517e57a240b29a199e1) )
	/* 0x080000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "ic34_t.f.ac_549d.ic34", 0x100000, 0x040000, CRC(902ad2ec) SHA1(58db20ca5888110e97f80e7df9dac1b8e9817562) )
	ROM_LOAD16_BYTE( "ic33_t.f.ac_d131.ic33", 0x100001, 0x040000, CRC(b162219d) SHA1(ad022307019b4a70cb532e1101cb5ed8d31f10e2) )

	ROM_REGION( 0x040000, REGION_SOUND1, ROMREGION_ERASE )
	/* empty socket (not used) */
ROM_END

ROM_START( tfrceacb ) /* Thunder Force AC (Bootleg)  (c)1990 Technosoft / Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "4.bin",    0x000000, 0x040000, CRC(eba059d3) SHA1(7bc04401f9a138fa151ac09a528b70acfb2021e3) )
	ROM_LOAD16_BYTE( "3.bin",    0x000001, 0x040000, CRC(3e5dc542) SHA1(4a66dc842afaa145dab82b232738eea107bdf0f8) )
	/* 0x080000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "epr-13659.ic34", 0x100000, 0x040000, CRC(29f23461) SHA1(032a7125fef5a660b85654d595aafc46812cdde6) )
	ROM_LOAD16_BYTE( "epr-13658.ic33", 0x100001, 0x040000, CRC(9e23734f) SHA1(64d27dc53f0ffc3513345a26ed077751b25d15f1) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13655.ic4", 0x000000, 0x040000, CRC(e09961f6) SHA1(e109b5f41502b765d191f22e3bbcff97d6defaa1) )
ROM_END

ROM_START( twinsqua ) /* Twin Squash  (c)1991 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14657.ic32", 0x000000, 0x040000, CRC(becbb1a1) SHA1(787b1a4bf420186d05b5448582f6492e40d394fa) )
	ROM_LOAD16_BYTE( "epr-14656.ic31", 0x000001, 0x040000, CRC(411906e7) SHA1(68a4e66b9e18499d77cdb584470f35f67edec6fd) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-14588.ic4", 0x000000, 0x020000, CRC(5a9b6881) SHA1(d86ec7f569fae5a1ce93a1cf40998cbb13726e0c) )
ROM_END

ROM_START( soniccar ) /* Waku Waku Sonic Patrol Car  (c)1991 Sega - 834-8401 SONIC PATCAR (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14369.ic32", 0x000000, 0x040000, CRC(2ea4c9a3) SHA1(92ee27c93bd1cbb99c88af39f5f227a8cadfb13c) )
	ROM_LOAD16_BYTE( "epr-14395.ic31", 0x000001, 0x040000, CRC(39622e18) SHA1(b7093c6d86e429e9721a0e1fd781231dc4342b31) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-14394.ic4", 0x000000, 0x040000, CRC(476e30dd) SHA1(c9d381160c58b05763ea286a53c7ca6de074fda2) )
ROM_END

ROM_START( wwmarine ) /* Waku Waku Marine  (c)1992 Sega - 834-9082 WAKUWAKU MARINE (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15097.ic32", 0x000000, 0x040000, CRC(1223a77a) SHA1(3725f17745da0d1f2d50b0a99a37b768c5539ce2) )
	ROM_LOAD16_BYTE( "epr-15096.ic31", 0x000001, 0x040000, CRC(1b833932) SHA1(a37caa75cbcc2b7ce84fccfd5768e6d1580c0bc0) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15095.ic4", 0x000000, 0x040000, CRC(df13755b) SHA1(177aac7aaadc36e14dbcdf12bd42dbe70b3edd49) )
ROM_END

ROM_START( wwanpanm ) /* Waku Waku Anpanman - 834-8191 sticker */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14123a.ic32", 0x000000, 0x040000, CRC(0e4f38c6) SHA1(2913fbde9a7e6428bab05c6e550c3e2d79c9f211) ) /* same as the original revision */
	ROM_LOAD16_BYTE( "epr-14122a.ic31", 0x000001, 0x040000, CRC(01b8fe20) SHA1(8d21c346b141a298074d199ce2bc4094217e8c25) ) /* just two bytes + the checksum bytes changed from the original revision */

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-14121.ic4", 0x000000, 0x040000, CRC(69adf3a1) SHA1(63233e723ab9be8d5663651cb2e6e54b64a7bb8e) )
ROM_END

ROM_START( wwanpanmo ) /* Waku Waku Anpanman - 837-7204 PCB */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14123.ic32", 0x000000, 0x040000, CRC(0e4f38c6) SHA1(2913fbde9a7e6428bab05c6e550c3e2d79c9f211) )
	ROM_LOAD16_BYTE( "epr-14122.ic31", 0x000001, 0x040000, CRC(20c0db3b) SHA1(de3b599d3d348008752a9691f72255e6f63d9d27) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-14121.ic4", 0x000000, 0x040000, CRC(69adf3a1) SHA1(63233e723ab9be8d5663651cb2e6e54b64a7bb8e) )
ROM_END

ROM_START( wwjumbo ) /* Waku Waku Jumbo (Rev.A) (c)1990 Sega - 834-7691 (EMP5032 labeled 317-0140 at ic27) */
	ROM_REGION( 0x200000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_BYTE( "epr-13474a.ic32", 0x000000, 0x020000, CRC(a9373fe0) SHA1(1abb334bbd16ab62459b25834ca1510af6dd5291) ) /* hand-written label */
	ROM_LOAD16_BYTE( "epr-13473a.ic31", 0x000001, 0x020000, CRC(860a29cb) SHA1(31b36d6dc8226663c4e18e1b15ca6a9691b400b6) ) /* " */

	ROM_REGION( 0x040000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-13472.ic4", 0x000000, 0x020000, CRC(e0c677fc) SHA1(42dc5798876b863c5c2175798f40067215527145) )
ROM_END

ROM_START( wwpajero )
	ROM_REGION( 0x200000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_BYTE( "epr-12925.ic32", 0x000000, 0x020000, CRC(47a814a1) SHA1(19199a13d823615fb804a2ae7896871442a844bb) )
	ROM_LOAD16_BYTE( "epr-12924.ic31", 0x000001, 0x020000, CRC(69a5df3a) SHA1(39b082034c4800547bedffca47eb24b43f9832fc) )
	ROM_LOAD16_BYTE( "epr-13125.ic34", 0x040000, 0x020000, CRC(a1ee9e69) SHA1(19bebb71aa3b8279b0e140f495f2df756d80c7f7) ) /* tested at 0x40000 */
	ROM_RELOAD(                        0x080000, 0x020000 ) /* pulls gfx data at 0x80000 */
	ROM_LOAD16_BYTE( "epr-13124.ic33", 0x040001, 0x020000, CRC(edf17b0d) SHA1(3709c5a31766a81baf01188d09364fce6b75dd1a) ) /* tested at 0x40001 */
	ROM_RELOAD(                        0x080001, 0x020000 ) /* pulls gfx data at 0x80001 */

	ROM_REGION( 0x040000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD( "epr-12923.ic4", 0x000000, 0x020000, CRC(774557a9) SHA1(e267369338717b26546cb4e4706dd04a3eee0905) )

	/* unmarked protection chip, seems unused? */
ROM_END

ROM_START( ssonicbr ) /* hack: supposedly the data ROM mapping was modified */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ssonicbr.ic32", 0x000000, 0x040000, CRC(cf254ecd) SHA1(4bb295ec80f8ddfeab4e360eebf12c5e2dfb9800) )
	ROM_LOAD16_BYTE( "ssonicbr.ic31", 0x000001, 0x040000, CRC(03709746) SHA1(0b457f557da77acd3f43950428117c1decdfaf26) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "ssonicbr.ic4", 0x000000, 0x020000, CRC(78e56a51) SHA1(8a72c12975cd74919b4337e0f681273e6b5cbbc6) )
ROM_END

ROM_START( anpanman ) /* Sega Soreike! Anpanman Popcorn Koujou (Rev.B) (c)1993 Sega - 834-8795-01 (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14804b.ic32", 0x000000, 0x040000, CRC(7ce88c49) SHA1(959ee459a5b4a6324488a935fa6a48e38ce93464) ) /* 27C020 */
	ROM_LOAD16_BYTE( "epr-14803b.ic31", 0x000001, 0x040000, CRC(eb3ca1b9) SHA1(e4dd9d2bd2301f47f167d6516457386ba4e57df0) ) /* 27C020 */
	ROM_LOAD16_BYTE( "epr-14806.ic34",  0x100000, 0x040000, CRC(40f398db) SHA1(abaaf4404342232b58f02c7ea0571926b2417b45) ) /* 27C020 */
	ROM_LOAD16_BYTE( "epr-14805.ic33",  0x100001, 0x040000, CRC(f27229ed) SHA1(307182cdcd8954bbc56d4b412df452b406466d53) ) /* 27C020 */

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-14807.ic4", 0x000000, 0x040000, CRC(9827549f) SHA1(66d195299085ec690498fc795a3088c05e6db820) ) /* 27C020 */
ROM_END

ROM_START( anpanmana ) /* Sega Soreike! Anpanman Popcorn Koujou (Rev.A) (c)1993 Sega - 834-8795-01 (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14804a.ic32", 0x000000, 0x040000, CRC(a80bd024) SHA1(bfd3cc45214cd89d5cbed99f88ba2d3e8cc61538) ) /* 27C020 */
	ROM_LOAD16_BYTE( "epr-14803a.ic31", 0x000001, 0x040000, CRC(32e1f248) SHA1(151c59d0be5bb2e77c777da2b99627c887d49f55) ) /* 27C020 */
	ROM_LOAD16_BYTE( "epr-14806.ic34",  0x100000, 0x040000, CRC(40f398db) SHA1(abaaf4404342232b58f02c7ea0571926b2417b45) ) /* 27C020 */
	ROM_LOAD16_BYTE( "epr-14805.ic33",  0x100001, 0x040000, CRC(f27229ed) SHA1(307182cdcd8954bbc56d4b412df452b406466d53) ) /* 27C020 */

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-14807.ic4", 0x000000, 0x040000, CRC(9827549f) SHA1(66d195299085ec690498fc795a3088c05e6db820) ) /* 27C020 */
ROM_END

ROM_START( sonicpop ) /* SegaSonic Popcorn Shop (Rev.B) (c)1993 Sega - 834-9555-02 (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-14592b.ic32", 0x000000, 0x040000, CRC(bac586a1) SHA1(0208213bfa1a5093e76edb1a7e0ba5ebc862801d) )
	ROM_LOAD16_BYTE( "epr-15491b.ic31", 0x000001, 0x040000, CRC(527106c3) SHA1(97f08006bba4b87c304c7ad3b1480b77e99dff10) )
	ROM_LOAD16_BYTE( "epr-15494.ic34",  0x100000, 0x040000, CRC(0520df5e) SHA1(5a795a1630d841406a106a566228223583deef44) )
	ROM_LOAD16_BYTE( "epr-15493.ic33",  0x100001, 0x040000, CRC(d51b3b85) SHA1(66f6b4656841ab70ffde0141a613eaf91c06f86b) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15495.ic4", 0x000000, 0x040000, CRC(d3ee4c68) SHA1(557c57b22521339d94d9a3e6fd2af68a67a153b6) )
ROM_END

ROM_START( sonicfgt ) /* SegaSonic Cosmo Fighter (c)1993 Sega - 834-10082 940725-3598T (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-17178.ic32", 0x000000, 0x040000, CRC(e05b7388) SHA1(38fd175c265986a14d1365e3a403e12fbc6c73c3) )
	ROM_LOAD16_BYTE( "epr-17177.ic31", 0x000001, 0x040000, CRC(7c2ec4eb) SHA1(f74f21f5dcfbf6ee902c2a44fed908a628d60363) )
	ROM_LOAD16_BYTE( "epr-17180.ic34", 0x100000, 0x040000, CRC(8933e91c) SHA1(5dc7451874f97e0e5d0c666800c26907b9abf5f5) )
	ROM_LOAD16_BYTE( "epr-17179.ic33", 0x100001, 0x040000, CRC(0ae979cd) SHA1(a4d4f096e976d4993123de0c2505382f878ea42a) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-17176.ic4", 0x000000, 0x040000, CRC(4211745d) SHA1(710f7dab436bf0551b95786efc5ea4303c0fd5ec) )
ROM_END

ROM_START( sonicfgtj ) /* SegaSonic Cosmo Fighter (c)1993 Sega - 834-10082 930719-1755T (EMP5032 labeled 317-0140) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-16001.ic32", 0x000000, 0x040000, CRC(8ed1dc11) SHA1(cd1cb6066c2ff159bec88802bc4b7ca7fff2ed71) )
	ROM_LOAD16_BYTE( "epr-16000.ic31", 0x000001, 0x040000, CRC(1440caec) SHA1(9e50c28544d6c42cdc7d3ae0f321670fed68fedb) )
	ROM_LOAD16_BYTE( "epr-16003.ic34", 0x100000, 0x040000, CRC(8933e91c) SHA1(5dc7451874f97e0e5d0c666800c26907b9abf5f5) )
	ROM_LOAD16_BYTE( "epr-16002.ic33", 0x100001, 0x040000, CRC(0ae979cd) SHA1(a4d4f096e976d4993123de0c2505382f878ea42a) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-16004.ic4", 0x000000, 0x040000, CRC(e87e8433) SHA1(b2b7945c7660ab2383049af5a2434768544a4ea8) )
ROM_END

ROM_START( ribbit ) /* Ribbit  (c)1991 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13833.ic32", 0x000000, 0x040000, CRC(5347f8ce) SHA1(b95b99536157edfbf0d74a42f64235f47dca7ee1) )
	ROM_LOAD16_BYTE( "epr-13832.ic31", 0x000001, 0x040000, CRC(889c42c2) SHA1(0839a50a68b64a66d995f1bfaff42fcb60bb4d45) )
	ROM_COPY( REGION_CPU1, 0x000000, 0x080000, 0x080000 )
	ROM_LOAD16_BYTE( "epr-13838.ic34", 0x100000, 0x080000, CRC(a5d62ac3) SHA1(8d83a7bc4017e125ef4231278f766b2368d5fc1f) )
	ROM_LOAD16_BYTE( "epr-13837.ic33", 0x100001, 0x080000, CRC(434de159) SHA1(cf2973131cabf2bc0ebb2bfe9f804ad3d7d0a733) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13834.ic4", 0x000000, 0x020000, CRC(ab0c1833) SHA1(f864e12ecf6c0524da20fc66747a4fa4280e67e9) )
ROM_END

ROM_START( ribbitj ) /* Ribbit  (c)1991 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-13836.ic32", 0x000000, 0x040000, CRC(21f222e2) SHA1(1e4b640833d3ec428bd50d89dc93a670a17a4451) )
	ROM_LOAD16_BYTE( "epr-13835.ic31", 0x000001, 0x040000, CRC(1c4b291a) SHA1(61b2e7721aefb97bbd38b3ca9275d7f345f3d0a6) )
	ROM_COPY( REGION_CPU1, 0x000000, 0x080000, 0x080000 )
	ROM_LOAD16_BYTE( "epr-13838.ic34", 0x100000, 0x080000, CRC(a5d62ac3) SHA1(8d83a7bc4017e125ef4231278f766b2368d5fc1f) )
	ROM_LOAD16_BYTE( "epr-13837.ic33", 0x100001, 0x080000, CRC(434de159) SHA1(cf2973131cabf2bc0ebb2bfe9f804ad3d7d0a733) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13834.ic4", 0x000000, 0x020000, CRC(ab0c1833) SHA1(f864e12ecf6c0524da20fc66747a4fa4280e67e9) )
ROM_END

ROM_START( tantr ) /* Tant-R (Puzzle & Action)  (c)1992 Sega - 834-9664 TANT-R (EMP5032 labeled 317-0211) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15614.ic32", 0x000000, 0x080000, CRC(557782bc) SHA1(1546a999ab97c380dc87f6c95d5687722206740d) )
	ROM_LOAD16_BYTE( "epr-15613.ic31", 0x000001, 0x080000, CRC(14bbb235) SHA1(8dbfec5fb1d7a695acbb2fc0e78e4bdf76eb8d9d) )
	ROM_LOAD16_BYTE( "mpr-15616.ic34", 0x100000, 0x080000, CRC(17b80202) SHA1(f47bf2aa0c5972647438619b8453c7dede5c422f) )
	ROM_LOAD16_BYTE( "mpr-15615.ic33", 0x100001, 0x080000, CRC(36a88bd4) SHA1(cc7f6a947d1b79bb86957c43035b53d6d2bcfa28) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15617.ic4", 0x000000, 0x040000, CRC(338324a1) SHA1(79e2782d0d4764dd723886f846c852a6f6c1fb64) )
ROM_END

ROM_START( tantrkor ) /* Tant-R (Puzzle & Action) (Korea) (c)1993 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	/* strange names, but this is what was printed on the (original) chips */
	ROM_LOAD16_BYTE( "mpr-15592b.ic32", 0x000000, 0x080000, CRC(7efe26b3) SHA1(958420b9b400eafe392745af90bff729463427c7) )
	ROM_LOAD16_BYTE( "mpr-15592b.ic31", 0x000001, 0x080000, CRC(af5a860f) SHA1(cb0011f420721d035e9f0e43bb72cf286982fd32) )
	ROM_LOAD16_BYTE( "mpr-15992b.ic34", 0x100000, 0x080000, CRC(6282a5d4) SHA1(9220e119e79d969d7d70e8a25c75dd3d9bc340ae) )
	ROM_LOAD16_BYTE( "mpr-15592b.ic33", 0x100001, 0x080000, CRC(82d78413) SHA1(9ff9c2b1632e280444965110bab90c0fc98cd6da) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15617.ic4", 0x000000, 0x040000, CRC(338324a1) SHA1(79e2782d0d4764dd723886f846c852a6f6c1fb64) )
ROM_END

ROM_START( tantrbl ) /* Tant-R (Puzzle & Action) (Bootleg)  (c)1992 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "pa_e10.bin",  0x000000, 0x080000, CRC(6c3f711f) SHA1(55aa2d50422134b95d9a7c5cbdc453b207b91b4c) )
	ROM_LOAD16_BYTE( "pa_f10.bin",  0x000001, 0x080000, CRC(75526786) SHA1(8f5aa7f6918b71a79e6fca18194beec2aef15844) )
	ROM_LOAD16_BYTE( "mpr-15616.ic34", 0x100000, 0x080000, CRC(17b80202) SHA1(f47bf2aa0c5972647438619b8453c7dede5c422f) )
	ROM_LOAD16_BYTE( "mpr-15615.ic33", 0x100001, 0x080000, CRC(36a88bd4) SHA1(cc7f6a947d1b79bb86957c43035b53d6d2bcfa28) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "pa_e03.bin", 0x000000, 0x020000, CRC(72918c58) SHA1(cb42363b163727a887a0b762519c72dcdf0a6460) )
	ROM_LOAD( "pa_e02.bin", 0x020000, 0x020000, CRC(4e85b2a3) SHA1(3f92fb931d315c5a2d6c54b3204718574928cb7b) )
ROM_END

ROM_START( tantrbl4 ) /* extremely similar to tantrbl, but in this one the card dispenser related text hasn't been removed */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "a.bin", 0x000000, 0x080000, CRC(25cafec2) SHA1(872ce10c080c8bddc2eaf1a48be7b43eeae32f28) )
	ROM_LOAD16_BYTE( "b.bin", 0x000001, 0x080000, CRC(8cf5ffd5) SHA1(21bcde46d3e075a93725292a344c3d9784ae5178) )
	ROM_LOAD16_BYTE( "d.bin", 0x100000, 0x080000, CRC(17b80202) SHA1(f47bf2aa0c5972647438619b8453c7dede5c422f) )
	ROM_LOAD16_BYTE( "c.bin", 0x100001, 0x080000, CRC(36a88bd4) SHA1(cc7f6a947d1b79bb86957c43035b53d6d2bcfa28) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "2.bin", 0x000000, 0x020000, CRC(72918c58) SHA1(cb42363b163727a887a0b762519c72dcdf0a6460) )
	ROM_LOAD( "1.bin", 0x020000, 0x020000, CRC(4e85b2a3) SHA1(3f92fb931d315c5a2d6c54b3204718574928cb7b) )
ROM_END

ROM_START( ooparts ) /* hack: modified to be playable with digital joystick */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ooparts.ic32", 0x000000, 0x080000, CRC(8dcf2940) SHA1(f72630e8a26e7f2089da56878a1599268c355246) )
	ROM_LOAD16_BYTE( "ooparts.ic31", 0x000001, 0x080000, CRC(35381899) SHA1(524f6e1b1292542079589275e20f45c2eb68605c) )
	ROM_LOAD16_BYTE( "ooparts.ic34", 0x100000, 0x080000, CRC(7192ac29) SHA1(d3028a9bbb7faa733285cf7e47fd840ec0d0bf69) )
	ROM_LOAD16_BYTE( "ooparts.ic33", 0x100001, 0x080000, CRC(42755dc2) SHA1(cd0aa79418b922266c5d41bf24b9136f9f105dc5) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-13655.ic4", 0x000000, 0x040000, CRC(e09961f6) SHA1(e109b5f41502b765d191f22e3bbcff97d6defaa1) )
ROM_END

ROM_START( puyo ) /* Puyo Puyo  (c)1992 Sega / Compile */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15198.ic32", 0x000000, 0x020000, CRC(9610d80c) SHA1(1ffad09d3369c1942d4db611c41bae47d08c7564) )
	ROM_LOAD16_BYTE( "epr-15197.ic31", 0x000001, 0x020000, CRC(7b1f3229) SHA1(13d0905291e748973d7d17eb404a286ffb94de03) )
	/* 0x040000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "epr-15200.ic34", 0x100000, 0x020000, CRC(0a0692e5) SHA1(d4ecc5b1791a91e3b33a5d4d0dd305f1623483d9) )
	ROM_LOAD16_BYTE( "epr-15199.ic33", 0x100001, 0x020000, CRC(353109b8) SHA1(92440987add3124b758e7eaa77a3a6f54ca61bb8) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15196.ic4", 0x000000, 0x020000, CRC(79112b3b) SHA1(fc3a202e1e2ff39950d4af689b7fcca86c301805) )
ROM_END

ROM_START( puyoj ) /* Puyo Puyo (Rev B)  (c)1992 Sega / Compile - 834-9029 (EMP5032 labeled 317-0203) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15036b.ic32", 0x000000, 0x020000, CRC(5310ca1b) SHA1(dcfe2bf7476b640dfb790e8716e75b483d535e48) )
	ROM_LOAD16_BYTE( "epr-15035b.ic31", 0x000001, 0x020000, CRC(bc62e400) SHA1(12bb6031574838a28889f6edb31dbb689265287c) )
	/* 0x040000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "epr-15038.ic34", 0x100000, 0x020000, CRC(3b9eea0c) SHA1(e3e6148c1769834cc0061932eb035daa79673144) )
	ROM_LOAD16_BYTE( "epr-15037.ic33", 0x100001, 0x020000, CRC(be2f7974) SHA1(77027ced7a62f94e9fc6e8a0a4ac0c62f7ea813b) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15034.ic4", 0x000000, 0x020000, CRC(5688213b) SHA1(f3f234e482871ca903a782e51008f3bfed04ee63) )

ROM_END

ROM_START( puyoja ) /* Puyo Puyo (Rev A)  (c)1992 Sega / Compile - 834-9029 (EMP5032 labeled 317-0203) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15036a.ic32", 0x000000, 0x020000, CRC(61b35257) SHA1(e09a7e992999befc88fc7928a478d1e2d14d7b08) )
	ROM_LOAD16_BYTE( "epr-15035a.ic31", 0x000001, 0x020000, CRC(dfebb6d9) SHA1(6f685729ef4660c2eba409c5236c6d2f313eef5b) )
	/* 0x040000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "epr-15038.ic34",  0x100000, 0x020000, CRC(3b9eea0c) SHA1(e3e6148c1769834cc0061932eb035daa79673144) )
	ROM_LOAD16_BYTE( "epr-15037.ic33",  0x100001, 0x020000, CRC(be2f7974) SHA1(77027ced7a62f94e9fc6e8a0a4ac0c62f7ea813b) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-15034.ic4", 0x000000, 0x020000, CRC(5688213b) SHA1(f3f234e482871ca903a782e51008f3bfed04ee63) )

ROM_END

ROM_START( puyobl ) /* Puyo Puyo  (c)1992 Sega / Compile  Bootleg */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "puyopuyb.4bo", 0x000000, 0x020000, CRC(89ea4d33) SHA1(bef9d011524e71c072d309f6da3c2ebc38878e0e) )
	ROM_LOAD16_BYTE( "puyopuyb.3bo", 0x000001, 0x020000, CRC(c002e545) SHA1(7a59ac764d60e9955830d9617b0bd122b44e7b2f) )
	/* 0x040000 - 0x100000 Empty */
	ROM_LOAD16_BYTE( "puyopuyb.6bo", 0x100000, 0x020000, CRC(0a0692e5) SHA1(d4ecc5b1791a91e3b33a5d4d0dd305f1623483d9) ) /* same as epr-15200.34 from the world set */
	ROM_LOAD16_BYTE( "puyopuyb.5bo", 0x100001, 0x020000, CRC(353109b8) SHA1(92440987add3124b758e7eaa77a3a6f54ca61bb8) ) /* same as epr-15199.33 from the world set */

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "puyopuyb.abo", 0x000000, 0x020000, CRC(79112b3b) SHA1(fc3a202e1e2ff39950d4af689b7fcca86c301805) ) /* same as epr-15196.4 from the world set */
ROM_END

ROM_START( ichir ) /* Ichidant-R (Puzzle & Action 2)  (c)1994 Sega (World) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "pa2_32.bin",     0x000000, 0x080000, CRC(7ba0c025) SHA1(855e9bb2a20c6f51b26381233c57c26aa96ad1f6) )
	ROM_LOAD16_BYTE( "pa2_31.bin",     0x000001, 0x080000, CRC(5f86e5cc) SHA1(44e201de00dfbf7c66d0e0d40d17b162c6f0625b) )
	ROM_LOAD16_BYTE( "epr-16888.ic34", 0x100000, 0x080000, CRC(85d73722) SHA1(7ebe81b4d6c89f87f60200a3a8cddb07d581adef) )
	ROM_LOAD16_BYTE( "epr-16887.ic33", 0x100001, 0x080000, CRC(bc3bbf25) SHA1(e760ad400bc183b38e9787d88c8ac084fbe2ae21) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "pa2_02.bin", 0x000000, 0x080000, CRC(fc7b0da5) SHA1(46770aa7e19b4f8a183be3f433c48ad677b552b1) )
ROM_END

ROM_START( ichirbl ) /* bootleg of the World version, only difference is the protection checks are NOPed out */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "2.f11", 0x000000, 0x080000, CRC(b8201c2e) SHA1(95ac035ffd5948745324d855ecffdb7d2ff9f0fb) )
	ROM_LOAD16_BYTE( "1.f10", 0x000001, 0x080000, CRC(af0dd811) SHA1(75b1f51ad93f99933037ffff4106c92f9c2393e6) )
	ROM_LOAD16_BYTE( "4.f8",  0x100000, 0x080000, CRC(85d73722) SHA1(7ebe81b4d6c89f87f60200a3a8cddb07d581adef) )
	ROM_LOAD16_BYTE( "3.f9",  0x100001, 0x080000, CRC(bc3bbf25) SHA1(e760ad400bc183b38e9787d88c8ac084fbe2ae21) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "2.e3", 0x000000, 0x080000, CRC(fc7b0da5) SHA1(46770aa7e19b4f8a183be3f433c48ad677b552b1) )
ROM_END

ROM_START( ichirk ) /* Ichidant-R (Puzzle & Action 2)  (c)1994 Sega (Korea) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	/* Again the part numbers are quite strange for the Korean verison */
	ROM_LOAD16_BYTE( "epr_ichi.32", 0x000000, 0x080000, CRC(804dea11) SHA1(40bf8cbd40969a5880df10914252b7f64d5ce8e9) )
	ROM_LOAD16_BYTE( "epr_ichi.31", 0x000001, 0x080000, CRC(92452353) SHA1(d2e1da5b139965611cd8d707d23396b5d4c07d12) )
	ROM_LOAD16_BYTE( "epr-16888.ic34", 0x100000, 0x080000, CRC(85d73722) SHA1(7ebe81b4d6c89f87f60200a3a8cddb07d581adef) )  /* m17235a.34 */
	ROM_LOAD16_BYTE( "epr-16887.ic33", 0x100001, 0x080000, CRC(bc3bbf25) SHA1(e760ad400bc183b38e9787d88c8ac084fbe2ae21) )  /* m17220a.33 */

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "pa2_02.bin", 0x000000, 0x080000, CRC(fc7b0da5) SHA1(46770aa7e19b4f8a183be3f433c48ad677b552b1) ) /* m17220a.4 */
ROM_END

ROM_START( ichirj ) /* Ichidant-R (Puzzle & Action 2)  (c)1994 Sega (Japan) - 834-10935 ICHIDANT-R */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-16886.ic32", 0x000000, 0x080000, CRC(38208e28) SHA1(07fc634bdf2d3e25274c9c374b3506dec765114c) )
	ROM_LOAD16_BYTE( "epr-16885.ic31", 0x000001, 0x080000, CRC(1ce4e837) SHA1(16600600e12e3f35e3da89524f7f51f019b5ad17) )
	ROM_LOAD16_BYTE( "epr-16888.ic34", 0x100000, 0x080000, CRC(85d73722) SHA1(7ebe81b4d6c89f87f60200a3a8cddb07d581adef) )
	ROM_LOAD16_BYTE( "epr-16887.ic33", 0x100001, 0x080000, CRC(bc3bbf25) SHA1(e760ad400bc183b38e9787d88c8ac084fbe2ae21) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-16884.ic4", 0x000000, 0x080000, CRC(fd9dcdd6) SHA1(b8053a2e68072e7664ffc3c53f983f3ba72a892b) )
ROM_END

ROM_START( stkclmns ) /* Stack Columns  (c)1994 Sega */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-16874.ic32", 0x000000, 0x080000, CRC(d78a871c) SHA1(7efcd5d07b089442be5170a3cf9e09579527252f) )
	ROM_LOAD16_BYTE( "epr-16873.ic31", 0x000001, 0x080000, CRC(1def1da4) SHA1(da534a971b40277b2d58ef22c07ca468250d23ca) )
	ROM_LOAD16_BYTE( "mpr-16797.ic34", 0x100000, 0x080000, CRC(b28e9bd5) SHA1(227eb591d10c9dbc52b35954ebd322e2a4451df2) )
	ROM_LOAD16_BYTE( "mpr-16796.ic33", 0x100001, 0x080000, CRC(ec7de52d) SHA1(85bc48cef15e615ad9059500808d17916c854a87) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-16793.ic4", 0x000000, 0x020000, CRC(ebb2d057) SHA1(4a19ee5d71e4aabe7d9b9b968ab5ee4bc6262aad) )
ROM_END

ROM_START( stkclmnsj ) /* Stack Columns  (c)1994 Sega - 834-10853 (EMP5032 labeled 317-0219) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-16795.ic32", 0x000000, 0x080000, CRC(b478fd02) SHA1(aaf9d9f9f4dc900b4e8ff6f258f26e782e5c3166) )
	ROM_LOAD16_BYTE( "epr-16794.ic31", 0x000001, 0x080000, CRC(6d0e8c56) SHA1(8f98d9fd98a1faa70b173cfd72f15102d11e79ae) )
	ROM_LOAD16_BYTE( "mpr-16797.ic34", 0x100000, 0x080000, CRC(b28e9bd5) SHA1(227eb591d10c9dbc52b35954ebd322e2a4451df2) )
	ROM_LOAD16_BYTE( "mpr-16796.ic33", 0x100001, 0x080000, CRC(ec7de52d) SHA1(85bc48cef15e615ad9059500808d17916c854a87) )

	ROM_REGION( 0x020000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-16793.ic4", 0x000000, 0x020000, CRC(ebb2d057) SHA1(4a19ee5d71e4aabe7d9b9b968ab5ee4bc6262aad) )
ROM_END

ROM_START( puyopuy2 ) /* Puyo Puyo 2  (c)1994 Compile */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-17241.ic32", 0x000000, 0x080000, CRC(1cad1149) SHA1(77fb0482fa35e615c0bed65f4d7f4dd89b241f23) )
	ROM_LOAD16_BYTE( "epr-17240.ic31", 0x000001, 0x080000, CRC(beecf96d) SHA1(c2bdad4b6184c11f81f2a5db409cb4ea186205a7) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-17239.ic4", 0x000000, 0x080000, CRC(020ff6ef) SHA1(6095b8277b47a6fd7a9721f15a70ae5bf6be9b1a) )
ROM_END

ROM_START( potopoto ) /* Poto Poto  (c)1994 Sega - 834-10778 (EMP5032 labeled 317-0218) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-16662a.ic32", 0x000000, 0x040000, CRC(bbd305d6) SHA1(1a4f4869fefac188c69bc67df0b625e43a0c3f1f) )
	ROM_LOAD16_BYTE( "epr-16661a.ic31", 0x000001, 0x040000, CRC(5a7d14f4) SHA1(a615b5f481256366db7b1c6302a8dcb69708102b) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-16660.ic4", 0x000000, 0x040000, CRC(8251c61c) SHA1(03eef3aa0bdde2c1d93128648f54fd69278d85dd) )
ROM_END

ROM_START( zunkyou ) /* Zunzunkyou no Yabou  (c)1994 Sega - 834-10857 (EMP5032 labeled 317-0221) */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-16812.ic32", 0x000000, 0x080000, CRC(eb088fb0) SHA1(69089a3516ad50f35e81971ef3c33eb3f5d52374) )
	ROM_LOAD16_BYTE( "epr-16811.ic31", 0x000001, 0x080000, CRC(9ac7035b) SHA1(1803ffbadc1213e04646d483e27da1591e22cd06) )
	ROM_LOAD16_BYTE( "epr-16814.ic34", 0x100000, 0x080000, CRC(821b3b77) SHA1(c45c7393a792ce8306a52f83f8ed8f6b0d7c11e9) )
	ROM_LOAD16_BYTE( "epr-16813.ic33", 0x100001, 0x080000, CRC(3cba9e8f) SHA1(208819bc1a205eaab089542afc7a59f69ce5bb81) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-16810.ic4", 0x000000, 0x080000, CRC(d542f0fe) SHA1(23ea50110dfe1cd9f286a535d15e0c3bcba73b00) )
ROM_END

ROM_START( headonch ) /* hack: protection routine was removed */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "headonch.ic32", 0x000000, 0x080000, CRC(091cf538) SHA1(04673678f543743b395edea39ad4ee6177436dc0) )
	ROM_LOAD16_BYTE( "headonch.ic31", 0x000001, 0x080000, CRC(91f3b5f1) SHA1(15cbe7a172dde7de7b73f0c9eeddfee41e8d1f80) )
	ROM_LOAD16_BYTE( "headonch.ic34", 0x100000, 0x080000, CRC(d8dc6323) SHA1(e7e891324764641691dcb63e5222f2ed9207fb96) )
	ROM_LOAD16_BYTE( "headonch.ic33", 0x100001, 0x080000, CRC(3268e38b) SHA1(10ded2be01465014ca9e6c64ffab1190ec985359) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )
	ROM_LOAD( "headonch.ic4", 0x000000, 0x040000, CRC(90af7301) SHA1(227227cb5d0df6612bac7b4c94b99e2287686ccd) )
ROM_END

ROM_START( pclubj ) /* Print Club (c)1995 Atlus */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr18171.32", 0x000000, 0x080000, CRC(6c8eb8e2) SHA1(bbd885a83269524215c1d8470544086e3e82c05c) )
	ROM_LOAD16_BYTE( "epr18170.31", 0x000001, 0x080000, CRC(72c631e6) SHA1(77c4ed793db6cb75346998f38a637db64fd258bd) )
	ROM_LOAD16_BYTE( "epr18173.34", 0x100000, 0x080000, CRC(9809dc72) SHA1(6dbe6b7d4e525aa9b6174f8dc5aee12a5e00a009) )
	ROM_LOAD16_BYTE( "epr18172.33", 0x100001, 0x080000, CRC(c61d819b) SHA1(4813ed3161e16099f482e0cf8df3cbe6c01c619c) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr18169.4", 0x000000, 0x080000, CRC(5c00ccfb) SHA1(d043ffa6528bb9b76774c96df4edf8222a1878a4) )
ROM_END

ROM_START( pclubjv2 ) /* Print Club vol.2 (c)1995 Atlus */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "p2jwn.u32", 0x000000, 0x080000, CRC(dfc0f7f1) SHA1(d2399f3ff05006590903f943cd77a9c709b9b5b1) )
	ROM_LOAD16_BYTE( "p2jwn.u31", 0x000001, 0x080000, CRC(6ab4c694) SHA1(d8cfaa1a49e86842079c6e3800a95c5afaf76ab6) )
	ROM_LOAD16_BYTE( "p2jwn.u34", 0x100000, 0x080000, CRC(854fd456) SHA1(eff7413a7acd8ee37cb73bc8dfd4f4ae53c04836) )
	ROM_LOAD16_BYTE( "p2jwn.u33", 0x100001, 0x080000, CRC(64428a69) SHA1(e2c5ead4b35db76fda1db03adcd020bde5ca1dd2) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr18169.4", 0x000000, 0x080000, CRC(5c00ccfb) SHA1(d043ffa6528bb9b76774c96df4edf8222a1878a4) )
ROM_END

ROM_START( pclubjv3 ) /* Print Club vol.3 (c)1996 Atlus */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ic32-p3jsp.ic32", 0x000000, 0x080000, CRC(cb5b6221) SHA1(43c1352bfcaa4012019ae58742a5d09c3c4e5a8f) )
	ROM_LOAD16_BYTE( "ic31-p3jsp.ic31", 0x000001, 0x080000, CRC(cdec8418) SHA1(7b5d6316182ec89230984b617ce0404ea0f1c331) )
	ROM_LOAD16_BYTE( "ic34-p3jsp.ic34", 0x100000, 0x080000, CRC(c33f71a3) SHA1(21f9de9db4504307d5290c11dc86bfed9ee23ef0) )
	ROM_LOAD16_BYTE( "ic33-p3jsp.ic33", 0x100001, 0x080000, CRC(c8da13e4) SHA1(38434f628b43549a234cf612f8a22993c76c20f0) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr18169.4", 0x000000, 0x080000, CRC(5c00ccfb) SHA1(d043ffa6528bb9b76774c96df4edf8222a1878a4) )
ROM_END

ROM_START( pclubjv4 ) /* Print Club vol.4 (c)1996 Atlus */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "p4jsm.u32", 0x000000, 0x080000, CRC(36ff5f80) SHA1(33872aa00c8ca3f54dd7503a44562fbdad92df7d) )
	ROM_LOAD16_BYTE( "p4jsm.u31", 0x000001, 0x080000, CRC(f3c021ad) SHA1(34792d861265b609d5022955eb7d2f471c63dfb8) )
	ROM_LOAD16_BYTE( "p4jsm.u34", 0x100000, 0x080000, CRC(d0fd4b33) SHA1(c272404f09bdb6596740ab150eb158cc22cc9aa6) )
	ROM_LOAD16_BYTE( "p4jsm.u33", 0x100001, 0x080000, CRC(ec667875) SHA1(d235a1d8dfa90e1c638e1f079ce528f61450e1f0) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-18169.ic4", 0x000000, 0x080000, CRC(5c00ccfb) SHA1(d043ffa6528bb9b76774c96df4edf8222a1878a4) )
ROM_END

ROM_START( pclubjv5 ) /* Print Club vol.5 (c)1996 Atlus */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "p5jat.u32", 0x000000, 0x080000, CRC(72220e69) SHA1(615de759d73469841987fb028eaf5d5598c32553) )
	ROM_LOAD16_BYTE( "p5jat.u31", 0x000001, 0x080000, CRC(06d83fde) SHA1(dc68375ccb16cde7900eb05f702bc15e7e702ea5) )
	ROM_LOAD16_BYTE( "p5jat.u34", 0x100000, 0x080000, CRC(b172ab58) SHA1(47a70bd678f6c4dafe70b83bd3db678cf44de48b) )
	ROM_LOAD16_BYTE( "p5jat.u33", 0x100001, 0x080000, CRC(ba38ec50) SHA1(666fdba56d8a4dab041015c5e8102305b491d293) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-18169.ic4", 0x000000, 0x080000, CRC(5c00ccfb) SHA1(d043ffa6528bb9b76774c96df4edf8222a1878a4) )
ROM_END

ROM_START( pclub ) /* english version based on v2 */
	ROM_REGION( 0x200000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-ic32.32", 0x000000, 0x080000, CRC(3fe9bda7) SHA1(2d3bf0f10c8b9a07a263365c5d81067c91974fe1) )
	ROM_LOAD16_BYTE( "epr-ic31.31", 0x000001, 0x080000, CRC(90f994d0) SHA1(687d537481bfc05a5809161a8f1c686aa157cb8f) )
	ROM_LOAD16_BYTE( "epr-ic34.34", 0x100000, 0x080000, CRC(4d1ebb55) SHA1(f403990ff41418d14bae645cffd5c042ce13312c) )
	ROM_LOAD16_BYTE( "epr-ic33.33", 0x100001, 0x080000, CRC(bdfdc797) SHA1(aec561a591ca1387b7d38ffe48a2c179a39c1b06) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 )
	ROM_LOAD( "epr-ic4.4", 0x000000, 0x080000, CRC(84eed1c4) SHA1(271b199250f9a7f6ba5d3bf09e187417b7c2f88e) )
ROM_END


/******************************************************************************
	Machine Init Functions
******************************************************************************/

static void init_saves(void)
{
	megadriv_irq_state_register();

	state_save_register_UINT8 ("C2_IO", 0, "I/O Writes", iochip_reg, 0x10);
	state_save_register_UINT8 ("C2_IO", 0, "YM3438 Last Port", &last_port, 1);

	state_save_register_UINT16 ("C2 Protection", 0, "Write Buffer", &prot_write_buf, 1);
	state_save_register_UINT16 ("C2 Protection", 0, "Read Buffer", &prot_read_buf, 1);
	state_save_register_int    ("C2 Protection", 0, "Alt Palette", &alt_palette_mode);
	state_save_register_int    ("C2 Protection", 0, "cam_data", &cam_data);
}

static void segac2_common_init(int (*func)(int in))
{
	prot_func = func;
	bloxeed_sound = 0;
	wheel_input = 0;
	init_saves();
}

static DRIVER_INIT( noprot )	{ segac2_common_init(prot_func_dummy); }
static DRIVER_INIT( columns )	{ segac2_common_init(prot_func_columns); }
static DRIVER_INIT( columns2 )	{ segac2_common_init(prot_func_columns2); }
static DRIVER_INIT( tfrceac )	{ segac2_common_init(prot_func_tfrceac); }
static DRIVER_INIT( borench )	{ segac2_common_init(prot_func_borench); }
static DRIVER_INIT( twinsqua )	{ segac2_common_init(prot_func_twinsqua); }
static DRIVER_INIT( ribbit )	{ segac2_common_init(prot_func_ribbit); }
static DRIVER_INIT( puyo )		{ segac2_common_init(prot_func_puyo); }
static DRIVER_INIT( tantr )		{ segac2_common_init(prot_func_tantr); }
static DRIVER_INIT( tantrkor )	{ segac2_common_init(prot_func_tantrkor); }
static DRIVER_INIT( potopoto )	{ segac2_common_init(prot_func_potopoto); }
static DRIVER_INIT( stkclmns )	{ segac2_common_init(prot_func_stkclmns); }
static DRIVER_INIT( stkclmnj )	{ segac2_common_init(prot_func_stkclmnj); }
static DRIVER_INIT( ichir )		{ segac2_common_init(prot_func_ichir); }
static DRIVER_INIT( ichirk )	{ segac2_common_init(prot_func_ichirk); }
static DRIVER_INIT( ichirj )	{ segac2_common_init(prot_func_ichirj); }
static DRIVER_INIT( puyopuy2 )	{ segac2_common_init(prot_func_puyopuy2); }
static DRIVER_INIT( zunkyou )	{ segac2_common_init(prot_func_zunkyou); }

static DRIVER_INIT( bloxeedc )
{
	segac2_common_init(prot_func_dummy);
	bloxeed_sound = 1;
}

static DRIVER_INIT( tfrceacb )
{
	segac2_common_init(prot_func_dummy);

	/* disable the palette bank switching from the protection chip */
	install_mem_write16_handler(0, 0x800000, 0x800001, MWA16_NOP);
}

static READ16_HANDLER( ichirjbl_prot_r )
{
	return 0x00f5;
}

static DRIVER_INIT( ichirjbl )
{
	segac2_common_init(prot_func_dummy);

	install_mem_read16_handler(0, 0x840108, 0x840109, ichirjbl_prot_r);
}

static DRIVER_INIT( wwmarine )
{
	segac2_common_init(prot_func_dummy);
	wheel_input = 1;
}


/* Print Club camera handling: just some fake stuff to get us to boot */

static READ16_HANDLER( printer_r )
{
	return cam_data;
}

static WRITE16_HANDLER( print_club_camera_w )
{
	cam_data = data;
}

static void pclub_init(int (*func)(int in))
{
	segac2_common_init(func);

	install_mem_read16_handler(0,  0x880120, 0x880121, printer_r );
	install_mem_read16_handler(0,  0x880124, 0x880125, printer_r );
	install_mem_write16_handler(0, 0x880124, 0x880125, print_club_camera_w);
}

static DRIVER_INIT( pclubj )	{ pclub_init(prot_func_pclub); }
static DRIVER_INIT( pclubjv2 )	{ pclub_init(prot_func_pclubjv2); }
static DRIVER_INIT( pclubjv3 )	{ pclub_init(prot_func_pclubjv3); }
static DRIVER_INIT( pclubjv4 )	{ pclub_init(prot_func_pclubjv4); }
static DRIVER_INIT( pclubjv5 )	{ pclub_init(prot_func_pclubjv5); }



/******************************************************************************
	Game Drivers
*******************************************************************************

	These cover all the above games.

	Dates are all verified correct from Ingame display, some of the Titles
	such as Ichidant-R, Tant-R might be slightly incorrect as I've seen the
	games referred to by other names such as Ichidant-R, Tanto-R, Tanto Arle
	etc.

	bloxeedc is set as as clone of bloxeed as it is the same game but running
	on a different piece of hardware.  The parent 'bloxeed' is a system18 game

******************************************************************************/

GAME( 1989, bloxeedc,  bloxeed,  segac,  bloxeedc, bloxeedc, ROT0, "Sega / Elorg", "Bloxeed (World, C System)" )
GAME( 1989, bloxeedu,  bloxeed,  segac,  bloxeedu, bloxeedc, ROT0, "Sega / Elorg", "Bloxeed (US, C System, Rev A)" )
GAME( 1990, columns,   0,        segac,  columns,  columns,  ROT0, "Sega", "Columns (World)" )
GAME( 1990, columnsu,  columns,  segac,  columnsu, columns,  ROT0, "Sega", "Columns (US, cocktail, Rev A)" )
GAME( 1990, columnsj,  columns,  segac,  columns,  columns,  ROT0, "Sega", "Columns (Japan)" )
GAME( 1990, columns2,  0,        segac,  columns2, columns2, ROT0, "Sega", "Columns II: The Voyage Through Time (World)" )
GAME( 1990, column2j,  columns2, segac,  columns2, columns2, ROT0, "Sega", "Columns II: The Voyage Through Time (Japan)" )
GAME( 1990, tfrceac,   0,        segac2, tfrceac,  tfrceac,  ROT0, "Technosoft / Sega", "Thunder Force AC" )
GAME( 1990, tfrceacj,  tfrceac,  segac2, tfrceac,  tfrceac,  ROT0, "Technosoft / Sega", "Thunder Force AC (Japan)" )
GAME( 1990, tfrceacb,  tfrceac,  segac2, tfrceac,  tfrceacb, ROT0, "bootleg", "Thunder Force AC (bootleg)" )
GAME( 1990, tfrceacjpb, tfrceac,  segac2, tfrceac,  tfrceac,  ROT0, "bootleg", "Thunder Force AC (Japan, prototype, bootleg)" )
GAME( 1990, borench,   0,        segac2, borench,  borench,  ROT0, "Sega", "Borench (set 1)" )
GAME( 1990, borencha,  borench,  segac2, borench,  borench,  ROT0, "Sega", "Borench (set 2)" )
GAME( 1990, borenchj,  borench,  segac2, borench,  borench,  ROT0, "Sega", "Borench (Japan)" )
GAME( 1991, ribbit,    0,        ribbit, ribbit,   ribbit,   ROT0, "Sega", "Ribbit!" )
GAME( 1991, ribbitj,   ribbit,   ribbit, ribbitj,  ribbit,   ROT0, "Sega", "Ribbit! (Japan)" )
GAME( 1991, twinsqua,  0,        segac2, twinsqua, twinsqua, ROT0, "Sega", "Twin Squash" )
GAME( 1991, soniccar,  0,        segac2, soniccar, noprot,   ROT0, "Sega", "Waku Waku Sonic Patrol Car" )
GAME( 1992, ssonicbr,  0,        segac2, ssonicbr, bloxeedc, ROT0, "hack", "SegaSonic Bros. (prototype, hack)" )
GAME( 1992, ooparts,   0,        segac2, ooparts,  noprot,   ROT270, "hack", "OOPArts (prototype, joystick hack)" )
GAME( 1992, puyo,      0,        segac2, puyo,     puyo,     ROT0, "Compile / Sega", "Puyo Puyo (World)" )
GAME( 1992, puyobl,    puyo,     segac2, puyo,     puyo,     ROT0, "bootleg", "Puyo Puyo (World, bootleg)" )
GAME( 1992, puyoj,     puyo,     segac2, puyo,     puyo,     ROT0, "Compile / Sega", "Puyo Puyo (Japan, Rev B)" )
GAME( 1992, puyoja,    puyo,     segac2, puyo,     puyo,     ROT0, "Compile / Sega", "Puyo Puyo (Japan, Rev A)" )
GAME( 1992, tantr,     0,        segac2, ichir,    tantr,    ROT0, "Sega", "Puzzle & Action: Tant-R (Japan)" )
GAME( 1993, tantrkor,  tantr,    segac2, ichir,    tantrkor, ROT0, "Sega", "Puzzle & Action: Tant-R (Korea)" )
GAME( 1992, tantrbl,   tantr,    segac2, ichir,    noprot,   ROT0, "bootleg", "Puzzle & Action: Tant-R (Japan) (bootleg set 1)" )
GAME( 1992, tantrbl4,  tantr,    segac2, ichir,    noprot,   ROT0, "bootleg", "Puzzle & Action: Tant-R (Japan) (bootleg set 4)" )
GAME( 1994, tantrbl2,  tantr,    segac,  ichir,    tantr,    ROT0, "bootleg", "Puzzle & Action: Tant-R (Japan) (bootleg set 2)" )
GAME( 1994, tantrbl3,  tantr,    segac,  ichir,    tantr,    ROT0, "bootleg", "Puzzle & Action: Tant-R (Japan) (bootleg set 3)" )
GAME( 1992, wwanpanm,  0,        segac2, wwmarine, wwmarine, ROT0, "Sega", "Waku Waku Anpanman (Rev A)" )
GAME( 1992, wwanpanmo, wwanpanm, segac2, wwmarine, wwmarine, ROT0, "Sega", "Waku Waku Anpanman" )
GAME( 1990, wwjumbo,   0,        segac2, wwjumbo,  noprot,   ROT0, "Sega", "Waku Waku Jumbo (Rev A)" )
GAME( 1992, wwmarine,  0,        segac2, wwmarine, wwmarine, ROT0, "Sega", "Waku Waku Marine" )
GAME( 1990, wwpajero,  0,        segac2, wwpajero, noprot,   ROT0, "Sega", "Waku Waku Pajero" )
GAME( 1992, anpanman,  0,        segac2, anpanman, noprot,   ROT0, "Sega", "Soreike! Anpanman Popcorn Koujou (Rev B)" )
GAME( 1992, anpanmana, anpanman, segac2, anpanman, noprot,   ROT0, "Sega", "Soreike! Anpanman Popcorn Koujou (Rev A)" )
GAME( 1993, sonicpop,  0,        segac2, sonicpop, noprot,   ROT0, "Sega", "SegaSonic Popcorn Shop (Rev B)" )
GAME( 1993, sonicfgt,  0,        segac2, sonicfgt, noprot,   ROT0, "Sega", "SegaSonic Cosmo Fighter (World)" )
GAME( 1993, sonicfgtj, sonicfgt, segac2, sonicfgt, noprot,   ROT0, "Sega", "SegaSonic Cosmo Fighter (Japan)" )
GAME( 1994, potopoto,  0,        segac2, potopoto, potopoto, ROT0, "Sega", "Poto Poto (Japan, Rev A)" )
GAME( 1994, stkclmns,  0,        segac2, stkclmns, stkclmns, ROT0, "Sega", "Stack Columns (World)" )
GAME( 1994, stkclmnsj, stkclmns, segac2, stkclmns, stkclmnj, ROT0, "Sega", "Stack Columns (Japan)" )
GAME( 1994, ichir,     0,        segac2, ichir,    ichir,    ROT0, "Sega", "Puzzle & Action: Ichidant-R (World)" )
GAME( 1994, ichirbl,   ichir,    segac2, ichir,    noprot,   ROT0, "bootleg", "Puzzle & Action: Ichidant-R (World) (bootleg)" )
GAME( 1994, ichirk,    ichir,    segac2, ichir,    ichirk,   ROT0, "Sega", "Puzzle & Action: Ichidant-R (Korea)" )
GAME( 1994, ichirj,    ichir,    segac2, ichir,    ichirj,   ROT0, "Sega", "Puzzle & Action: Ichidant-R (Japan)" )
GAME( 1994, ichirjbl,  ichir,    segac,  ichir,    ichirjbl, ROT0, "bootleg", "Puzzle & Action: Ichidant-R (Japan) (bootleg)" )
GAME( 1994, puyopuy2,  0,        segac2, puyopuy2, puyopuy2, ROT0, "Compile (Sega license)", "Puyo Puyo 2 (Japan)" )
GAME( 1994, zunkyou,   0,        segac2, zunkyou,  zunkyou,  ROT0, "Sega", "Zunzunkyou no Yabou (Japan)" )
GAME( 1994, headonch,  0,        segac2, headonch, noprot,   ROT0, "hack", "Monita to Rimoko no Head On Channel (prototype, hack)" )
GAMEX(1995, pclubj,    0,        pclub,  pclub,    pclubj,   ROT0, "Atlus", "Print Club (Japan Vol.1)", GAME_NOT_WORKING )
GAMEX(1995, pclubjv2,  0,        pclub,  pclubjv2, pclubjv2, ROT0, "Atlus", "Print Club (Japan Vol.2)", GAME_NOT_WORKING )
GAMEX(1995, pclub,     pclubjv2, pclub,  pclubjv2, pclubj,   ROT0, "Atlus", "Print Club (World)", GAME_NOT_WORKING )
GAMEX(1995, pclubjv3,  0,        pclub,  pclubjv2, pclubjv3, ROT0, "Atlus", "Print Club (Japan Vol.3)", GAME_NOT_WORKING )
GAMEX(1996, pclubjv4,  0,        pclub,  pclubjv2, pclubjv4, ROT0, "Atlus", "Print Club (Japan Vol.4)", GAME_NOT_WORKING )
GAMEX(1996, pclubjv5,  0,        pclub,  pclubjv2, pclubjv5, ROT0, "Atlus", "Print Club (Japan Vol.5)", GAME_NOT_WORKING )
