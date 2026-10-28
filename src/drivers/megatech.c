/***************************************************************************

	Sega Mega-Tech

	A Mega Drive (Genesis) based board with a Z80 BIOS (Sega Master System
	VDP for the game selection screen on a second monitor, an SN76489) that
	handles the coins, the play time and up to 8 game carts. Each cart has
	the game ROM and an instruction ROM shown on the BIOS screen.

	Moved here from drivers/segac2.c, as in current MAME (megatech.cpp);
	the Mega Drive part (68000, sound Z80, I/O, interrupts) is shared with
	the other Mega Drive based boards (includes/segac2.h), the VDP is in
	vidhrdw/segavdp.c and the video glue in vidhrdw/segac2_vidhrdw.c.

	As in current MAME:
	- the BIOS cart slots (select, banked RAM, the window into the cart);
	- the BIOS display in the 315-5246 224 line mode, drawn line by line;
	- the Master System carts, run by the Mega Drive in Mark III mode;
	- the sets, ROM names and dumps (good dumps of mt_bbros, mt_ggolf,
	  mt_gng and mt_tetri), the instruction ROMs in the cart at 0x300000.
	The screen has the BIOS display (224 lines) on top of the game (two
	monitors on the real cabinet).

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "segac2.h"

/* the BIOS VDP, in vidhrdw/segasyse_vidhrdw.c */
extern UINT8 *segae_vdp_vram[];
extern UINT8 *segae_vdp_regs[];
extern UINT8 *segae_vdp_cram[];
extern UINT8 segae_vdp_vrambank[];

/******************************************************************************
	The BIOS
******************************************************************************/

static unsigned int bios_port_ctrl;
static unsigned int bios_ctrl_inputs;	/* Genesis/SMS input ports disable bit */
static unsigned char bios_ctrl[6];
static int mt_sms_cart;					/* a Master System cart */

/* the cart slots: the boards have 8, the sets have one cart in slot 1.
   As in current MAME the BIOS selects a slot through port E of the first
   CXD1095 I/O chip (each write resets the game CPUs), has 4KB of RAM per
   slot at 0x3000 and reads the selected cart through a window at 0x8000
   into the 68000 space (0x6000 sets the window address one bit at a time,
   the instruction ROM is at 0x300000); an empty slot reads 0 */
static UINT8 bios_hintcount;			/* the BIOS VDP line counter */
static UINT8 bios_screen[224][256];		/* colours of its active lines */
static UINT8 bios_backdrop[224];
static UINT8 mt_cart_select;
static UINT8 mt_io1_latch[5];			/* the first CXD1095 at 0x6400 */
static UINT8 mt_io1_dir[5];				/* 1 = input */
static UINT32 mt_bank_addr;
static UINT8 mt_banked_ram[0x1000 * 8];

static void sms_reset(void);

static void megatech_switch_cart(void)
{
	/* the game CPUs are held in reset, the one of the cart restarts */
	cpu_set_reset_line(0, ASSERT_LINE);
	cpu_set_halt_line(1, ASSERT_LINE);

	if (mt_cart_select == 0)
	{
		if (mt_sms_cart)
		{
			sms_reset();
			cpu_set_reset_line(1, PULSE_LINE);
			cpu_set_halt_line(1, CLEAR_LINE);
		}
		else
			cpu_set_reset_line(0, CLEAR_LINE);
	}
}

static READ_HANDLER( megatech_io1_r )
{
	if (offset == 0)
		return readinputport(10);		/* DSW A */
	if (offset == 1)
		return readinputport(11);		/* DSW B */
	if (offset < 5)
		return mt_io1_latch[offset] & ~mt_io1_dir[offset];
	return 0;
}

static WRITE_HANDLER( megatech_io1_w )
{
	int port;

	if (offset < 5)
	{
		if (offset == 4)
			data &= 0x0f;
		mt_io1_latch[offset] = data;

		/* port E: the cart select */
		if (offset == 4)
		{
			mt_cart_select = (data & ~mt_io1_dir[4]) & 0x07;
			megatech_switch_cart();
		}
	}
	else if (offset == 6)
	{
		/* the direction of each half of ports A-D */
		for (port = 0; port < 4; port++, data >>= 2)
			mt_io1_dir[port] = ((data & 1) ? 0x0f : 0) | ((data & 2) ? 0xf0 : 0);
	}
	else if (offset == 7)
		mt_io1_dir[4] = (data & 0x0f) | 0xf0;
}

static READ_HANDLER( megatech_banked_ram_r )
{
	return mt_banked_ram[offset + 0x1000 * mt_cart_select];
}

static WRITE_HANDLER( megatech_banked_ram_w )
{
	mt_banked_ram[offset + 0x1000 * mt_cart_select] = data;
}

static WRITE_HANDLER( megatech_z80_bank_w )
{
	mt_bank_addr = ((mt_bank_addr >> 1) | (data << 23)) & 0xff8000;
}

static READ_HANDLER( megatech_68k_window_r )
{
	UINT32 address = mt_bank_addr + offset;

	/* the cart ROM area, nothing in the empty slots */
	if (mt_cart_select != 0 || address >= 0x400000)
		return 0;
	return memory_region(REGION_CPU1)[BYTE_XOR_BE(address)];
}

static READ_HANDLER( bios_ctrl_r )
{
	if(offset == 0)
		return 0;
	if(offset == 2)
		return bios_ctrl[offset] & 0xfe;

	return bios_ctrl[offset];
}

static WRITE_HANDLER( bios_ctrl_w )
{
	if(offset == 1)
	{
		bios_ctrl_inputs = data & 0x04;  /* Genesis/SMS input ports disable bit*/
	}
	bios_ctrl[offset] = data;
}

static void megatech_state_register(void)
{
	state_save_register_UINT8 ("megatech", 0, "bios_hintcount", &bios_hintcount, 1);
	state_save_register_UINT8 ("megatech", 0, "cart_select", &mt_cart_select, 1);
	state_save_register_UINT8 ("megatech", 0, "io1_latch", mt_io1_latch, 5);
	state_save_register_UINT8 ("megatech", 0, "io1_dir", mt_io1_dir, 5);
	state_save_register_UINT32("megatech", 0, "bank_addr", &mt_bank_addr, 1);
	state_save_register_UINT8 ("megatech", 0, "banked_ram", mt_banked_ram, 0x1000 * 8);
	state_save_register_UINT32("megatech", 0, "bios_port_ctrl", (UINT32 *)&bios_port_ctrl, 1);
	state_save_register_UINT32("megatech", 0, "bios_ctrl_inputs", (UINT32 *)&bios_ctrl_inputs, 1);
	state_save_register_UINT8 ("megatech", 0, "bios_ctrl", bios_ctrl, 6);
}


/* the Mega Drive pads, unless the BIOS disables them */
static READ16_HANDLER ( megatech_genesis_io_r )
{
	/* 8-bit only, data is mirrored in both halves */

	UINT8 return_value = 0;

	switch (offset)
	{
		case 0:
		/* Charles MacDonald ( http://cgfm2.emuviews.com/ )
		    D7 : Console is 1= Export (USA, Europe, etc.) 0= Domestic (Japan)
		    D6 : Video type is 1= PAL, 0= NTSC
		    D5 : Sega CD unit is 1= not present, 0= connected.
		    D4 : Unused (always returns zero)
		    D3 : Bit 3 of version number
		    D2 : Bit 2 of version number
		    D1 : Bit 1 of version number
		    D0 : Bit 0 of version number
		*/
			return_value = 0x80; /* ? megatech is usa? */
			break;

		case 1: /* port A data (joypad 1) */

			if (genesis_io_ram[offset] & 0x40)
			{
				int iport = readinputport(9);
				return_value = iport & 0x3f;
			}
			else
			{
				int iport1 = readinputport(12);
				int iport2 = readinputport(7) >> 1;
				return_value = (iport1 & 0x10) + (iport2 & 0x20);
				if(iport1 & 0x10 || iport2 & 0x20)
					return_value+=1;
			}

			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "reading joypad 1 , type %02x %02x\n",genesis_io_ram[offset] & 0x80, return_value &0x7f);*/
			if(bios_ctrl_inputs & 0x04) return_value = 0xff;
			break;

		case 2: /* port B data (joypad 2) */

			if (genesis_io_ram[offset] & 0x40)
			{
				int iport1 = (readinputport(9) & 0xc0) >> 6;
				int iport2 = (readinputport(8) & 0x0f) << 2;
				return_value = (iport1 + iport2) & 0x3f;
			}
			else
			{
				int iport1 = readinputport(12) << 2;
				int iport2 = readinputport(7) >> 2;
				return_value = (iport1 & 0x10) + (iport2 & 0x20);
				if(iport1 & 0x10 || iport2 & 0x20)
					return_value+=1;
			}
			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "reading joypad 2 , type %02x %02x\n",genesis_io_ram[offset] & 0x80, return_value &0x7f);*/
			if(bios_ctrl_inputs & 0x04) return_value = 0xff;
			break;

		default:
			return_value = 0xe0;

	}
	return return_value | return_value << 8;
}

static WRITE_HANDLER (megatech_bios_port_ctrl_w)
{
	bios_port_ctrl = data;
}

static READ_HANDLER (megatech_bios_port_dc_r)
{
	if(bios_port_ctrl == 0x55)
		return readinputport(12);
	else
		return readinputport(9);
}

static READ_HANDLER (megatech_bios_port_dd_r)
{
	if(bios_port_ctrl == 0x55)
		return readinputport(12);
	else
		return readinputport(8);
}

static WRITE_HANDLER (megatech_bios_port_7f_w)
{
/*	usrintf_showmessage("CPU #3: I/O port 0x7F write, data %02x",data);*/
}


static PORT_READ_START( megatech_bios_readport )
	{ 0xdc, 0xdc, megatech_bios_port_dc_r },  /* player inputs*/
	{ 0xdd, 0xdd, megatech_bios_port_dd_r },  /* other player 2 inputs*/
	{ 0xbe, 0xbf, megatech_bios_port_be_bf_r },			/* VDP */
PORT_END

static PORT_WRITE_START( megatech_bios_writeport )
	{ 0x3f, 0x3f, megatech_bios_port_ctrl_w },
	{ 0x7f, 0x7f, megatech_bios_port_7f_w },
	{ 0xbe, 0xbf, megatech_bios_port_be_bf_w },			/* VDP */
PORT_END

/******************************************************************************
	The Master System carts
*******************************************************************************

	As in current MAME the Mega Drive runs them in its Mark III mode: the
	68000 is off, the sound Z80 becomes the main CPU with the Master System
	map (ROM through the Sega mapper, 8KB of RAM, the VDP and the PSG on the
	I/O ports) and the 315-5313 VDP works in mode 4, which is emulated here
	the way MAME does it for this chip (the 315-5124 core with the 315-5313
	differences: no table masks, the data port write doesn't load the read
	buffer, register writes to codes 2 and 3 go to the CRAM, no sprite zoom,
	the colours of its SMS compatible palette).

	The frame has 262 lines, one interrupt callback each: lines 0-191 are
	the active display, VINT is raised at line 193 and the line counter
	runs on the active lines and on the first line of the bottom border.

******************************************************************************/

#define SMS_PALBASE		(0x800 + 32)	/* after the 32 colours set by the BIOS VDP code */
#define BIOS_PALBASE	(SMS_PALBASE + 64)

static UINT8 *sms_rom;					/* the cart ROM in Z80 byte order */
static UINT8 sms_genesis_z80_ram[0x2000];	/* unused in Mark III mode */
static UINT8 sms_ram[0x2000];
static UINT8 sms_bankreg[4];			/* the mapper registers, 0xfffc-0xffff */

static UINT8 smsvdp_vram[0x4000];
static UINT8 smsvdp_cram[0x20];
static UINT8 smsvdp_reg[16];
static UINT16 smsvdp_addr;
static UINT8 smsvdp_code;
static UINT8 smsvdp_pending;			/* first byte of a control word written */
static UINT8 smsvdp_latch;				/* the first byte */
static UINT8 smsvdp_buffer;				/* VRAM read buffer */
static UINT8 smsvdp_status;
static UINT8 smsvdp_hint;				/* line interrupt pending */
static UINT8 smsvdp_line_counter;
static UINT8 smsvdp_reg9copy;			/* vertical scroll, latched at the start of the frame */
static UINT16 smsvdp_line;				/* current line, 0 = first active line */

/* the active display, CRAM indexes (the colours are looked up at update time) */
static UINT8 sms_screen[192][256];
static UINT8 sms_backdrop[192];


static void sms_update_irq(void)
{
	int state = ((smsvdp_status & 0x80) && (smsvdp_reg[1] & 0x20)) ||
	            (smsvdp_hint && (smsvdp_reg[0] & 0x10));

	cpu_set_irq_line(1, 0, state ? ASSERT_LINE : CLEAR_LINE);
}

static void sms_set_banks(void)
{
	cpu_setbank(4, sms_rom + (sms_bankreg[1] & 0x1f) * 0x4000);
	cpu_setbank(5, sms_rom + (sms_bankreg[2] & 0x1f) * 0x4000);
	cpu_setbank(6, sms_rom + (sms_bankreg[3] & 0x1f) * 0x4000);
}

static WRITE_HANDLER( sms_mapper_w )
{
	sms_ram[0x1ffc + offset] = data;
	sms_bankreg[offset] = data;
	if (offset != 0)
		sms_set_banks();
}

static READ_HANDLER( sms_count_r )
{
	/* the V counter of an NTSC 192 line frame: 0x00-0xda, then 0xd5-0xff;
	   the H counter is only latched by the light gun input */
	if (offset & 1)
		return 0;

	return (smsvdp_line <= 0xda) ? smsvdp_line : smsvdp_line - 6;
}

static WRITE_HANDLER( sms_psg_w )
{
	SN76496_0_w(0, data);
}

static READ_HANDLER( sms_vdp_r )
{
	UINT8 result;

	smsvdp_pending = 0;

	if (!(offset & 1))
	{
		/* data port */
		result = smsvdp_buffer;
		smsvdp_buffer = smsvdp_vram[smsvdp_addr & 0x3fff];
		smsvdp_addr++;
		return result;
	}

	/* control port: the status, clears the flags */
	result = smsvdp_status;
	smsvdp_status = 0x1f;
	smsvdp_hint = 0;
	sms_update_irq();
	return result;
}

static WRITE_HANDLER( sms_vdp_w )
{
	if (!(offset & 1))
	{
		/* data port */
		smsvdp_pending = 0;
		if (smsvdp_code < 2)
			smsvdp_vram[smsvdp_addr & 0x3fff] = data;
		else
			smsvdp_cram[smsvdp_addr & 0x1f] = data;
		smsvdp_addr++;
		return;
	}

	/* control port */
	if (!smsvdp_pending)
	{
		smsvdp_pending = 1;
		smsvdp_latch = data;
		return;
	}

	smsvdp_pending = 0;
	smsvdp_code = (data >> 6) & 3;
	smsvdp_addr = (data << 8) | smsvdp_latch;

	switch (smsvdp_code)
	{
		case 0:		/* VRAM read */
			smsvdp_buffer = smsvdp_vram[smsvdp_addr & 0x3fff];
			smsvdp_addr++;
			break;

		case 2:		/* register write */
			if ((data & 0x1f) >= 16)
				break;
			smsvdp_reg[data & 0x0f] = smsvdp_latch;
			sms_update_irq();
			smsvdp_code = 0;
			break;
	}
}

static READ_HANDLER( sms_port_dc_r )
{
	/* up, down, left, right, buttons 1 and 2, player 2 up and down */
	return readinputport(9);
}

static READ_HANDLER( sms_port_dd_r )
{
	/* player 2 left, right, buttons 1 and 2; reset (not pressed) */
	return (readinputport(8) & 0x0f) | 0x10;
}


/* one line of a mode 4 display into CRAM indexes (0-31), for the BIOS VDP
   (315-5246, md = 0) and for the Mega Drive VDP in Mark III mode (md = 1);
   height is 192, or 224 for the 315-5246 extended mode */
static void mode4_draw_line(UINT8 *dest, const UINT8 *vram, const UINT8 *reg, int line, int height, int y_scroll_frame, int md, UINT8 *status)
{
	UINT16 prio[256];
	int x_scroll, start_column, fine_x, name_base, scroll_mod, column;
	int sprite_height, zoom, sat, extra, sprite, count;
	int sprite_x[8], sprite_tile[8], sprite_line[8];
	UINT8 collision[256];
	int backdrop = 0x10 + (reg[7] & 0x0f);

	if (!(reg[1] & 0x40))
	{
		memset(dest, backdrop, 256);
		return;
	}

	/* background: 32 columns, the top 2 rows can be fixed horizontally and
	   the right 8 columns vertically */
	x_scroll = ((reg[0] & 0x40) && line < 16) ? 0 : reg[8];
	start_column = 32 - (x_scroll >> 3);
	fine_x = x_scroll & 7;
	if (height == 192)
	{
		name_base = (reg[2] << 10) & 0x3800;
		scroll_mod = 224;
	}
	else
	{
		name_base = ((reg[2] & 0x0c) << 10) | 0x0700;
		scroll_mod = 256;
	}

	for (column = 0; column < 32; column++)
	{
		int y_scroll = ((reg[0] & 0x80) && column > 23) ? 0 : y_scroll_frame;
		int row = (((line + y_scroll) % scroll_mod) >> 3) << 6;
		int entry = (name_base + row + (((column + start_column) & 0x1f) << 1)) & 0x3fff;
		int tile = vram[entry] | (vram[(entry + 1) & 0x3fff] << 8);
		int pal = (tile & 0x800) ? 0x10 : 0x00;
		int tile_line = (line + y_scroll) & 7;
		const UINT8 *pattern;
		int px;

		if (tile & 0x400)
			tile_line = 7 - tile_line;
		pattern = &vram[((tile & 0x1ff) << 5) + (tile_line << 2)];

		/* the pixels left by the fine scroll get colour 0 of a palette: the
		   one of the next tile on the Mega Drive, on the SMS the one selected
		   by a bit of tile 0x100 */
		if (column == 0 && fine_x)
		{
			int left = pal;
			if (!md)
				left = (vram[(0x100 << 5) + (((line - 1) & 7) << 2) + 1] & 0x08) ? 0x10 : 0x00;
			for (px = 0; px < fine_x; px++)
			{
				dest[px] = left;
				prio[px] = 0;
			}
		}

		for (px = 0; px < 8; px++)
		{
			int bit = 7 - px;
			int pen = ((pattern[0] >> bit) & 1) | (((pattern[1] >> bit) & 1) << 1) |
			          (((pattern[2] >> bit) & 1) << 2) | (((pattern[3] >> bit) & 1) << 3);
			int plot = fine_x + (column << 3) + ((tile & 0x200) ? 7 - px : px);

			if (plot < 256)
			{
				dest[plot] = pal | pen;
				prio[plot] = (tile & 0x1000) | pen;
			}
		}
	}

	/* sprites: the table is parsed for the previous line, up to 8 per line;
	   the 315-5313 has no sprite zoom */
	sprite_height = (reg[1] & 0x02) ? 16 : 8;
	zoom = (!md && (reg[1] & 0x01)) ? 2 : 1;
	sat = (reg[5] << 7) & 0x3f00;
	extra = sat + 0x80;
	count = 0;

	for (sprite = 0; sprite < 64; sprite++)
	{
		int parse_line = line - 1;
		int y = vram[sat + sprite];

		if (height == 192 && y == 0xd0)
			break;
		if (y >= 240)
			y -= 256;
		if (zoom > 1)
		{
			parse_line >>= 1;
			y >>= 1;
		}

		if (parse_line >= y && parse_line < y + sprite_height)
		{
			if (count < 8)
			{
				int sline = parse_line - y;
				int tile = vram[extra + (sprite << 1) + 1];
				int x = vram[extra + (sprite << 1)];

				if (reg[0] & 0x08)
					x -= 8;
				if (reg[6] & 0x04)
					tile += 256;
				if (sprite_height == 16)
					tile &= 0x1fe;
				if (sline > 7)
					tile++;

				sprite_x[count] = x;
				sprite_tile[count] = tile;
				sprite_line[count] = (sline & 7) << 2;
				count++;
			}
			else if (status && !(*status & 0x80))
				*status |= 0x40;		/* overflow */
		}
	}

	memset(collision, 0, sizeof(collision));
	for (sprite = count - 1; sprite >= 0; sprite--)
	{
		const UINT8 *pattern = &vram[((sprite_tile[sprite] << 5) + sprite_line[sprite]) & 0x3fff];
		int px, z;

		for (px = 0; px < 8; px++)
		{
			int bit = 7 - px;
			int pen = ((pattern[0] >> bit) & 1) | (((pattern[1] >> bit) & 1) << 1) |
			          (((pattern[2] >> bit) & 1) << 2) | (((pattern[3] >> bit) & 1) << 3);

			if (pen == 0)
				continue;

			for (z = 0; z < zoom; z++)
			{
				int plot = sprite_x[sprite] + px * zoom + z;

				if (plot < 0 || plot > 255)
					continue;

				/* behind a background tile with priority, unless it's transparent */
				if (!(prio[plot] & 0x1000) || prio[plot] == 0x1000)
				{
					dest[plot] = 0x10 | pen;
					prio[plot] = 0x10 | pen;
				}

				if (collision[plot] && status)
					*status |= 0x20;
				collision[plot] = 1;
			}
		}
	}

	/* left column blanked */
	if (reg[0] & 0x20)
		memset(dest, backdrop, 8);
}

/* one line of the cart display */
static void sms_draw_line(int line)
{
	UINT8 index[256];
	int x;

	mode4_draw_line(index, smsvdp_vram, smsvdp_reg, line, 192, smsvdp_reg9copy, 1, &smsvdp_status);

	/* the colours are taken at the time the line is drawn */
	for (x = 0; x < 256; x++)
		sms_screen[line][x] = smsvdp_cram[index[x]] & 0x3f;
	sms_backdrop[line] = smsvdp_cram[0x10 + (smsvdp_reg[7] & 0x0f)] & 0x3f;
}

/* the 262 lines of the frame */
static INTERRUPT_GEN( megatech_sms_line )
{
	int line = 261 - cpu_getiloops();

	smsvdp_line = line;

	if (line <= 192)
	{
		if (line == 0)
			smsvdp_reg9copy = smsvdp_reg[9];

		if (smsvdp_line_counter == 0)
		{
			smsvdp_line_counter = smsvdp_reg[10];
			smsvdp_hint = 1;
		}
		else
			smsvdp_line_counter--;

		if (line < 192)
			sms_draw_line(line);
	}
	else
		smsvdp_line_counter = smsvdp_reg[10];

	if (line == 193)
		smsvdp_status |= 0x80;

	sms_update_irq();
}

static void sms_state_register(void)
{
	state_save_register_UINT8 ("mt_sms", 0, "ram", sms_ram, 0x2000);
	state_save_register_UINT8 ("mt_sms", 0, "bankreg", sms_bankreg, 4);
	state_save_register_UINT8 ("mt_sms", 0, "vram", smsvdp_vram, 0x4000);
	state_save_register_UINT8 ("mt_sms", 0, "cram", smsvdp_cram, 0x20);
	state_save_register_UINT8 ("mt_sms", 0, "reg", smsvdp_reg, 16);
	state_save_register_UINT16("mt_sms", 0, "addr", &smsvdp_addr, 1);
	state_save_register_UINT8 ("mt_sms", 0, "code", &smsvdp_code, 1);
	state_save_register_UINT8 ("mt_sms", 0, "pending", &smsvdp_pending, 1);
	state_save_register_UINT8 ("mt_sms", 0, "latch", &smsvdp_latch, 1);
	state_save_register_UINT8 ("mt_sms", 0, "buffer", &smsvdp_buffer, 1);
	state_save_register_UINT8 ("mt_sms", 0, "status", &smsvdp_status, 1);
	state_save_register_UINT8 ("mt_sms", 0, "hint", &smsvdp_hint, 1);
	state_save_register_UINT8 ("mt_sms", 0, "line_counter", &smsvdp_line_counter, 1);
	state_save_register_UINT8 ("mt_sms", 0, "reg9copy", &smsvdp_reg9copy, 1);
	state_save_register_UINT16("mt_sms", 0, "line", &smsvdp_line, 1);
	state_save_register_func_postload(sms_set_banks);
}

static void sms_reset(void)
{
	memset(sms_ram, 0, sizeof(sms_ram));
	sms_bankreg[0] = 0;
	sms_bankreg[1] = 0;
	sms_bankreg[2] = 1;
	sms_bankreg[3] = 2;
	sms_set_banks();
	cpu_setbank(7, sms_ram);
	cpu_setbank(8, sms_ram);

	memset(smsvdp_vram, 0, sizeof(smsvdp_vram));
	memset(smsvdp_cram, 0, sizeof(smsvdp_cram));
	memset(smsvdp_reg, 0, sizeof(smsvdp_reg));
	smsvdp_reg[2] = 0x0e;
	smsvdp_reg[10] = 0xff;
	smsvdp_addr = 0;
	smsvdp_code = 0;
	smsvdp_pending = 0;
	smsvdp_latch = 0;
	smsvdp_buffer = 0;
	smsvdp_status = 0x1f;
	smsvdp_hint = 0;
	smsvdp_line_counter = 0;
	smsvdp_reg9copy = 0;
	smsvdp_line = 0;
	memset(sms_screen, 0, sizeof(sms_screen));
	memset(sms_backdrop, 0, sizeof(sms_backdrop));

	sms_state_register();
}

static MEMORY_READ_START( megatech_sms_readmem )
	{ 0x0000, 0x3fff, MRA_BANK4 },
	{ 0x4000, 0x7fff, MRA_BANK5 },
	{ 0x8000, 0xbfff, MRA_BANK6 },
	{ 0xc000, 0xdfff, MRA_BANK7 },
	{ 0xe000, 0xffff, MRA_BANK8 },		/* mirror */
MEMORY_END

static MEMORY_WRITE_START( megatech_sms_writemem )
	{ 0x0000, 0xbfff, MWA_NOP },
	{ 0xc000, 0xdfff, MWA_BANK7 },
	{ 0xe000, 0xfffb, MWA_BANK8 },		/* mirror */
	{ 0xfffc, 0xffff, sms_mapper_w },
MEMORY_END

static PORT_READ_START( megatech_sms_readport )
	{ 0x10, 0x10, sms_port_dd_r },
	{ 0x40, 0x7f, sms_count_r },
	{ 0x80, 0xbf, sms_vdp_r },
	{ 0xdc, 0xdc, sms_port_dc_r },
	{ 0xdd, 0xdf, sms_port_dd_r },
PORT_END

static PORT_WRITE_START( megatech_sms_writeport )
	{ 0x40, 0x7f, sms_psg_w },
	{ 0x80, 0xbf, sms_vdp_w },
PORT_END

/******************************************************************************
	Memory maps
******************************************************************************/

static MEMORY_READ16_START( megatech_68k_readmem )
	{ 0x000000, 0x3fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, megatech_genesis_io_r },				/* Genesis Input */
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },				/* VDP Access */
	{ 0xfe0000, 0xfeffff, MRA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( megatech_68k_writemem )
	{ 0x000000, 0x3fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, genesis_io_w, &genesis_io_ram },				/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },
	{ 0xc00000, 0xc0000f, segavdp_w },				/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },/* Main Ram */
MEMORY_END

static MEMORY_READ_START(megatech_bios_readmem)
	{ 0x0000, 0x2fff, MRA_ROM },
	{ 0x3000, 0x3fff, megatech_banked_ram_r },	/* per slot */
	{ 0x4000, 0x5fff, MRA_RAM },
	{ 0x6400, 0x6407, megatech_io1_r },
	{ 0x6800, 0x6800, input_port_6_r },
	{ 0x6801, 0x6801, input_port_7_r },
	{ 0x6802, 0x6807, bios_ctrl_r },
	{ 0x7000, 0x77ff, MRA_ROM },
	{ 0x8000, 0x9fff, megatech_68k_window_r },	/* the selected cart */
MEMORY_END

static MEMORY_WRITE_START(megatech_bios_writemem)
	{ 0x0000, 0x2fff, MWA_ROM },
	{ 0x3000, 0x3fff, megatech_banked_ram_w },
	{ 0x4000, 0x5fff, MWA_RAM },
	{ 0x6000, 0x6000, megatech_z80_bank_w },
	{ 0x6400, 0x6407, megatech_io1_w },
	{ 0x6802, 0x6807, bios_ctrl_w },
	{ 0x7000, 0x77ff, MWA_ROM },
MEMORY_END

/******************************************************************************
	Machine init
******************************************************************************/

static MACHINE_INIT( megatech )
{
	/* the Master System map has no Mega Drive Z80 RAM */
	if (mt_sms_cart)
		genesis_z80_ram = sms_genesis_z80_ram;

	/* mirroring of ram etc. */
	cpu_setbank(1, &genesis_z80_ram[0]);
	cpu_setbank(2, &genesis_z80_ram[0]);
	cpu_setbank(3, &genesis_68k_ram[0]);

	machine_init_genesis();

	/* a Master System cart: the 68000 is off, the Z80 runs the cart */
	if (mt_sms_cart)
	{
		cpu_set_halt_line(0, ASSERT_LINE);
		cpu_set_halt_line(1, CLEAR_LINE);
		sms_reset();
	}

	/* slot 1 selected, the I/O chip ports are inputs */
	mt_cart_select = 0;
	memset(mt_io1_latch, 0, sizeof(mt_io1_latch));
	memset(mt_io1_dir, 0xff, sizeof(mt_io1_dir));
	mt_bank_addr = 0;
	memset(mt_banked_ram, 0, sizeof(mt_banked_ram));

	bios_hintcount = 0;
	memset(bios_screen, 0, sizeof(bios_screen));
	memset(bios_backdrop, 0, sizeof(bios_backdrop));

	/* save states (repeated registrations are ignored) */
	megatech_state_register();
}

/******************************************************************************
	Input ports
******************************************************************************/

INPUT_PORTS_START( megatech ) /* Genesis Input Ports */
	PORT_START


	PORT_START	/* Player 1 Controls - part 2 */
/*	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )*/
/*	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )*/
/*	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )*/
/*	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )*/
/*	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )*/
/*	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )*/
/*	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNUSED )*/
/*	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNUSED )*/

	PORT_START	/* Player 1 Controls - part 1 */
/*	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )*/
/*	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_START1 )*/


	PORT_START	/* Player 2 Controls - part 2 */
/*	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )*/
/*	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )*/
/*	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )*/
/*	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2  )*/
/*	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )*/
/*	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )*/
/*	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNUSED )*/
/*	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNUSED )*/

	PORT_START	/* Player 2 Controls - part 1 */
/*	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )*/
/*	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_START1 | IPF_PLAYER2 )*/

	PORT_START	/* Temp - Fake dipswitch to turn on / off sms vdp display */
/*	PORT_DIPNAME( 0x01, 0x01, "SMS VDP Display (fake)" )*/
/*	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )*/
/*	PORT_DIPSETTING(    0x01, DEF_STR( On ) )*/

	PORT_START
    PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_SERVICE2, "Select", KEYCODE_0, JOYCODE_NONE )
    PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN , "0x6800 bit 1", KEYCODE_Y, JOYCODE_NONE )
    PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6800 bit 2", KEYCODE_U, JOYCODE_NONE )
    PORT_BITX( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6800 bit 3", KEYCODE_I, JOYCODE_NONE )
    PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_SPECIAL, "Door 1", KEYCODE_K, JOYCODE_NONE )
    PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SPECIAL, "Door 2", KEYCODE_L, JOYCODE_NONE )
	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6800 bit 6", KEYCODE_O, JOYCODE_NONE )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_SERVICE, "Test mode", KEYCODE_F2, JOYCODE_NONE )

	PORT_START
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_COIN1 )  /* a few coin inputs here*/
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1, "Service coin", KEYCODE_9, JOYCODE_NONE )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3,"Enter", KEYCODE_MINUS, JOYCODE_NONE )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2)
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 4", KEYCODE_NONE, JOYCODE_NONE )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 5", KEYCODE_NONE, JOYCODE_NONE )
	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 6", KEYCODE_NONE, JOYCODE_NONE )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 7", KEYCODE_NONE, JOYCODE_NONE )

	PORT_START	 /* up, down, left, right, button 2,3, 2P up, down.*/
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )

    PORT_START	/* DSW A */
	PORT_DIPNAME( 0x02, 0x02, "Coin slot 3" )
	PORT_DIPSETTING (   0x00, "Inhibit" )
	PORT_DIPSETTING (   0x02, "Accept" )
	PORT_DIPNAME( 0x01, 0x01, "Coin slot 4" )
	PORT_DIPSETTING (   0x00, "Inhibit" )
	PORT_DIPSETTING (   0x01, "Accept" )
	PORT_DIPNAME( 0x1c, 0x1c, "Coin slot 3/4 value" )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/10 credits" )
	PORT_DIPNAME( 0xe0, 0x60, "Coin slot 2 value" )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Inhibit" )

	PORT_START /* DSW B */
	PORT_DIPNAME( 0x0f, 0x01, "Coin Slot 1 value" )
	PORT_DIPSETTING(    0x00, "Inhibit" )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_9C ) )
	PORT_DIPSETTING(    0x0a, "1 coin/10 credits" )
	PORT_DIPSETTING(    0x0b, "1 coin/11 credits" )
	PORT_DIPSETTING(    0x0c, "1 coin/12 credits" )
	PORT_DIPSETTING(    0x0d, "1 coin/13 credits" )
	PORT_DIPSETTING(    0x0e, "1 coin/14 credits" )
	PORT_DIPSETTING(    0x0f, "1 coin/15 credits" )
	PORT_DIPNAME( 0xf0, 0xa0, "Time per credit" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x10, "7:30" )
	PORT_DIPSETTING(    0x20, "7:00" )
	PORT_DIPSETTING(    0x30, "6:30" )
	PORT_DIPSETTING(    0x40, "6:00" )
	PORT_DIPSETTING(    0x50, "5:30" )
	PORT_DIPSETTING(    0x60, "5:00" )
	PORT_DIPSETTING(    0x70, "4:30" )
	PORT_DIPSETTING(    0x80, "4:00" )
	PORT_DIPSETTING(    0x90, "3:30" )
	PORT_DIPSETTING(    0xa0, "3:00" )
	PORT_DIPSETTING(    0xb0, "2:30" )
	PORT_DIPSETTING(    0xc0, "2:00" )
	PORT_DIPSETTING(    0xd0, "1:30" )
	PORT_DIPSETTING(    0xe0, "1:00" )
	PORT_DIPSETTING(    0xf0, "0:30" )

	PORT_START	 /* BIOS input ports extra*/
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1)
/*	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 4", KEYCODE_NONE, JOYCODE_NONE )*/
/*	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 5", KEYCODE_NONE, JOYCODE_NONE )*/
/*	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 6", KEYCODE_NONE, JOYCODE_NONE )*/
/*	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN , "port DD bit 7", KEYCODE_NONE, JOYCODE_NONE )*/

INPUT_PORTS_END

/******************************************************************************
	The BIOS VDP lines
*******************************************************************************

	As drivers/segac2.c megatech_irq, with the 224 line mode the menu
	uses: the line counter and the VINT follow the active height, and each
	line is drawn when it's reached, so the scroll and colour changes made
	during the frame are seen where they happen.

******************************************************************************/

/* in drivers/segasyse.c */
extern UINT8 vintpending;
extern UINT8 hintpending;


static int megatech_bios_height(void)
{
	const UINT8 *reg = segae_vdp_regs[0];

	/* the 315-5246 224 line mode: M1, M2 and M4 */
	return ((reg[0] & 0x06) == 0x06 && (reg[1] & 0x18) == 0x10) ? 224 : 192;
}

static void mode4_draw_line(UINT8 *dest, const UINT8 *vram, const UINT8 *reg, int line, int height, int y_scroll_frame, int md, UINT8 *status);

static void megatech_bios_draw_line(int line, int height)
{
	const UINT8 *reg = segae_vdp_regs[0];
	const UINT8 *vram = segae_vdp_vram[0] + segae_vdp_vrambank[0] * 0x4000;
	const UINT8 *cram = segae_vdp_cram[0];
	UINT8 index[256];
	int x;

	mode4_draw_line(index, vram, reg, line, height, reg[9], 0, NULL);
	for (x = 0; x < 256; x++)
		bios_screen[line][x] = cram[index[x]] & 0x3f;
	bios_backdrop[line] = cram[0x10 + (reg[7] & 0x0f)] & 0x3f;
}

static INTERRUPT_GEN( megatech_bios_irq )
{
	const UINT8 *reg = segae_vdp_regs[0];
	int sline = 261 - cpu_getiloops();
	int height = megatech_bios_height();

	if (sline == 0)
		bios_hintcount = reg[10];

	if (sline < height)
		megatech_bios_draw_line(sline, height);

	if (sline <= height)
	{
		if (sline == height)
			vintpending = 1;

		if (bios_hintcount == 0)
		{
			bios_hintcount = reg[10];
			hintpending = 1;

			if (reg[0] & 0x10)
			{
				cpu_set_irq_line(2, 0, HOLD_LINE);
				return;
			}
		}
		else
			bios_hintcount--;
	}

	if (sline > height)
	{
		bios_hintcount = reg[10];

		if (sline < height + 32 && vintpending)
			cpu_set_irq_line(2, 0, HOLD_LINE);
	}
}

/******************************************************************************
	Video
******************************************************************************/

/* the colours of the two VDPs in mode 4: the 315-5246 of the BIOS and the
   SMS compatible palette of the 315-5313 */
static VIDEO_START( megatech_mt )
{
	static const UINT8 level_5246[4] = { 0, 89, 174, 255 };
	static const UINT8 level_5313[4] = { 0, 99, 162, 255 };
	int i;

	if (video_start_megatech())
		return 1;

	for (i = 0; i < 64; i++)
	{
		palette_set_color(BIOS_PALBASE + i, level_5246[i & 3], level_5246[(i >> 2) & 3], level_5246[(i >> 4) & 3]);
		palette_set_color(SMS_PALBASE + i, level_5313[i & 3], level_5313[(i >> 2) & 3], level_5313[(i >> 4) & 3]);
	}

	return 0;
}

/* the BIOS display, rows 0-223: its lines are drawn during the frame, so
   the rows go to the screen with the last update of the frame (the partial
   updates of the game VDP start from row 0 and would show the lines of the
   previous frame not drawn yet) */
static void megatech_draw_bios(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	const pen_t *pens = &Machine->pens[BIOS_PALBASE];
	int height = megatech_bios_height();
	int top = (224 - height) / 2;
	int y, x;

	if (cliprect->max_y != Machine->visible_area.max_y)
		return;

	for (y = 0; y < 224; y++)
	{
		UINT16 *dest = (UINT16 *)bitmap->line[y];
		int line = y - top;

		if (line < 0 || line >= height)
		{
			pen_t border = pens[segae_vdp_cram[0][0x10 + (segae_vdp_regs[0][7] & 0x0f)] & 0x3f];
			for (x = 0; x < 320; x++)
				dest[x] = border;
		}
		else
		{
			pen_t border = pens[bios_backdrop[line]];
			for (x = 0; x < 32; x++)
				dest[x] = dest[x + 288] = border;
			for (x = 0; x < 256; x++)
				dest[x + 32] = pens[bios_screen[line][x]];
		}
	}
}

/* the BIOS display on top, the game below (two monitors on the real cabinet) */
static VIDEO_UPDATE( megatech )
{
	int y;


	megatech_draw_bios(bitmap, cliprect);

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *dest = (UINT16 *)bitmap->line[y];

		if (y < 224)
			continue;

		if (segavdp_get_reg(12) & 0x81)
			segavdp_draw_line(dest, y - 224, 0);
		else
		{
			/* H32 mode: 256 pixels in the middle of the screen (the VDP
			   draws a little outside the 320 pixels: the margins) */
			UINT16 line[16 + 320 + 16];
			int x;

			segavdp_draw_line(line + 16, y - 224, 0);
			for (x = 0; x < 32; x++)
				dest[x] = dest[x + 288] = get_black_pen();
			memcpy(dest + 32, line + 16, 256 * sizeof(UINT16));
		}
	}
}

static VIDEO_UPDATE( megatech_sms )
{
	const pen_t *pens = &Machine->pens[SMS_PALBASE];
	int y, x;

	megatech_draw_bios(bitmap, cliprect);

	/* the 256x192 display centred in the game screen, with the border colour */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *dest = (UINT16 *)bitmap->line[y];
		int line = y - 224 - 16;

		if (y < 224)
			continue;

		if (line < 0 || line >= 192)
		{
			pen_t border = pens[smsvdp_cram[0x10 + (smsvdp_reg[7] & 0x0f)] & 0x3f];
			for (x = 0; x < 320; x++)
				dest[x] = border;
		}
		else
		{
			const UINT8 *src = sms_screen[line];
			pen_t border = pens[sms_backdrop[line]];

			for (x = 0; x < 32; x++)
				dest[x] = dest[x + 288] = border;
			for (x = 0; x < 256; x++)
				dest[x + 32] = pens[src[x]];
		}
	}
}

/******************************************************************************
	Machine drivers
******************************************************************************/

static MACHINE_DRIVER_START( megatech )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( genesis_base )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(megatech_68k_readmem, megatech_68k_writemem)
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS | VIDEO_DUAL_MONITOR)

	MDRV_VIDEO_START(megatech_mt)
	MDRV_VIDEO_UPDATE(megatech)
	MDRV_MACHINE_INIT(megatech)

	MDRV_ASPECT_RATIO(4,6)
	MDRV_SCREEN_SIZE(320,224+224) /* the BIOS screen/menu on top */
	MDRV_VISIBLE_AREA(0, 319, 0, 224+223)
	MDRV_PALETTE_LENGTH(2048+32+64+64) /* +32 for the BIOS VDP code, +64+64 for the mode 4 colours */

	MDRV_CPU_ADD_TAG("megatech_bios", Z80, 53693100 / 15) /* ?? */
	MDRV_CPU_MEMORY(megatech_bios_readmem, megatech_bios_writemem)
	MDRV_CPU_PORTS(megatech_bios_readport,megatech_bios_writeport)
	MDRV_CPU_VBLANK_INT(megatech_bios_irq, 262)
	MDRV_SOUND_ADD(SN76496, megatech_sn76489_intf)

MACHINE_DRIVER_END


static MACHINE_DRIVER_START( megatech_sms )

	MDRV_IMPORT_FROM( megatech )

	MDRV_CPU_MODIFY("sound")
	MDRV_CPU_MEMORY(megatech_sms_readmem, megatech_sms_writemem)
	MDRV_CPU_PORTS(megatech_sms_readport, megatech_sms_writeport)
	MDRV_CPU_VBLANK_INT(megatech_sms_line, 262)

	MDRV_VIDEO_UPDATE(megatech_sms)

MACHINE_DRIVER_END

/******************************************************************************
	ROM definitions
******************************************************************************/

/* MegaTech Games - Genesis & sms! Games with a timer */

/* 12368-xx  xx is the game number? if so there are a _lot_ of carts, mt_beast is 01, mt_sonic is 52! */

#define MEGATECH_BIOS \
	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* Bios */ \
	ROM_LOAD( "epr-12664.20", 0x000000, 0x8000, CRC(f71e9526) SHA1(1c7887541d02c41426992d17f8e3db9e03975953) )

ROM_START( megatech )
	MEGATECH_BIOS
ROM_END

ROM_START( mt_beast ) /* Altered Beast */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12538.ic1", 0x000000, 0x080000, CRC(3bea3dce) SHA1(ec72e4fde191dedeb3f148f132603ed3c23f0f86) )
	ROM_LOAD16_BYTE( "epr-12368-01.ic2", 0x300001, 0x08000, CRC(40cb0088) SHA1(e1711532c29f395a35a1cb34d789015881b5a1ed) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_astro ) /* Astro Warrior */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "ep13817.ic2", 0x000000, 0x20000, CRC(299cbb74) SHA1(901697a3535ad70190647f34ad5b30b695d54542) )
	ROM_LOAD16_BYTE( "epr-12368-13.ic1", 0x300001, 0x08000, CRC(4038cbd1) SHA1(696bc1efce45d9f0052b2cf0332a232687c8d6ab) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_wcsoc ) /* World Championship Soccer */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12607b.ic1", 0x000000, 0x080000, CRC(bc591b30) SHA1(55e8577171c0933eee53af1dabd0f4c6462d5fc8) )
	ROM_LOAD16_BYTE( "epr-12368-21.ic2", 0x300001, 0x08000, CRC(028ee46b) SHA1(cd8f81d66e5ae62107eb20e0ca5db4b66d4b2987) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_gng ) /* Ghouls'n Ghosts */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12605.ic1", 0x000000, 0x020000, CRC(1066c6ab) SHA1(c30e4442732bdb38c96d780542f8550a94d127b0) )
	ROM_LOAD16_WORD_SWAP( "mpr12606.ic2", 0x080000, 0x020000, CRC(d0be7777) SHA1(a44b2a3d427f6973b5c1a3dcd8d1776366acb9f7) )
	ROM_CONTINUE(0x020000,0x60000)
	ROM_LOAD16_BYTE( "epr-12368-23.ic3", 0x300001, 0x08000, CRC(7ee58546) SHA1(ad5bb0934475eacdc5e354f67c96fe0d2512d33b) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_gaxe ) /* Golden Axe */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "12806.ic1", 0x000000, 0x080000, CRC(43456820) SHA1(2f7f1fcd979969ac99426f11ab99999a5494a121) )
	ROM_LOAD16_BYTE( "epr-12368-25.ic2", 0x300001, 0x08000, CRC(1f07ed28) SHA1(9d54192f4c6c1f8a51c38a835c1dd1e4e3e8279e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_smgp ) /* Super Monaco Grand Prix */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "13250.ic1", 0x000000, 0x080000, CRC(189b885f) SHA1(31c06ffcb48b1604989a94e584261457de4f1f46) )
	ROM_LOAD16_BYTE( "epr-12368-39.ic2", 0x300001, 0x08000, CRC(64b3ce25) SHA1(83a9f2432d146a712b037f96f261742f7dc810bb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_sonic ) /* Sonic The Hedgehog */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp13913.ic1", 0x000000, 0x080000, CRC(480b4b5c) SHA1(ab1dc1f738e3b2d0898a314b123fa71182bf572e) )
	ROM_LOAD16_BYTE( "epr-12368-52.ic2", 0x300001, 0x08000, CRC(6a69d20c) SHA1(e483b39ff6eca37dc192dc296d004049e220554a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_gaxe2 ) /* Golden Axe 2 */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp14272.ic1", 0x000000, 0x080000, CRC(d4784cae) SHA1(b6c286027d06fd850016a2a1ee1f1aeea080c3bb) )
	ROM_LOAD16_BYTE( "epr-12368-57.ic2", 0x300001, 0x08000, CRC(dc9b4433) SHA1(efd3a598569010cdc4bf38ecbf9ed1b4e14ffe36) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_fshrk ) /* Fire Shark */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp14341.ic1", 0x000000, 0x080000, CRC(04d65ebc) SHA1(24338aecdc52b6f416548be722ca475c83dbae96) )
	ROM_LOAD16_BYTE( "epr-12368-53.ic2", 0x300001, 0x08000, CRC(4fa61044) SHA1(7810deea221c10b0b2f5233443d81f4f1998ee58) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_eswat ) /* ESWAT */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp13192-h.ic1", 0x000000, 0x080000, CRC(82f458ef) SHA1(58444b783312def71ecffc4ad021b72a609685cb) )
	ROM_LOAD16_BYTE( "epr-12368-38.ic2", 0x300001, 0x08000, CRC(43c5529b) SHA1(104f85adea6da1612c0aa96d553efcaa387d7aaf) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_bbros ) /* Bonanza Bros. */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mpr-13905a.ic1", 0x000000, 0x080000, CRC(6d617940) SHA1(11d5ff1c2db79632f6dea2edf97f56af2149cea4) )
	ROM_LOAD16_BYTE( "epr-12368-49.ic2", 0x300001, 0x08000, CRC(c5101da2) SHA1(636f30043e2e9291e193ef9a2ead2e97a0bf7380) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_kcham ) /* Kid Chameleon */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp14557.ic1", 0x000000, 0x100000, CRC(e1a889a4) SHA1(a2768eacafc47d371e5276f0cce4b12b6041337a) )
	ROM_LOAD16_BYTE( "epr-12368-60.ic2", 0x300001, 0x08000, CRC(a8e4af18) SHA1(dfa49f6ec4047718f33dba1180f6204dbaff884c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_lastb ) /* Last Battle */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12578f.ic1", 0x000000, 0x080000, CRC(531191a0) SHA1(f6bc26e975c01a3e10ab4033e4c5f494627a1e2f) )
	ROM_LOAD16_BYTE( "epr-12368-20.ic2", 0x300001, 0x08000, CRC(e1a71c91) SHA1(c250da18660d8aea86eb2abace41ba46130dabc8) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_mwalk ) /* Moonwalker */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp13285a.ic1", 0x000000, 0x080000, CRC(189516e4) SHA1(2a79e07da2e831832b8d448cae87a833c85e67c9) )
	ROM_LOAD16_BYTE( "epr-12368-40.ic2", 0x300001, 0x08000, CRC(0482378c) SHA1(734772f3ddb5ff82b76c3514d18a464b2bce8381) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_mystd ) /* Mystic Defender */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12707.1", 0x000000, 0x080000, CRC(4f2c513d) SHA1(f9bb548b3688170fe18bb3f1b5b54182354143cf) )
	ROM_LOAD16_BYTE( "epr-12368-27.ic2", 0x300001, 0x08000, CRC(caf46f78) SHA1(a9659e86a6a223646338cd8f29c346866e4406c7) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_shar2 ) /* Space Harrier 2 */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp11934.ic1", 0x000000, 0x080000, CRC(932daa09) SHA1(a2d7a76f3604c6227d43229908bfbd02b0ef5fd9) )
	ROM_LOAD16_BYTE( "epr-12368-02.ic2", 0x300001, 0x08000, CRC(c129c66c) SHA1(e7c0c97db9df9eb04e2f9ff561b64305219b8f1f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_stbld ) /* Super Thunder Blade */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp11996f.ic1", 0x000000, 0x080000,  CRC(9355c34e) SHA1(26ff91c2921408673c644b0b1c8931d98524bf63) )
	ROM_LOAD16_BYTE( "epr-12368-03.ic2", 0x300001, 0x08000, CRC(1ba4ac5d) SHA1(9bde57d70189d159ebdc537a9026001abfd0deae) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_tetri ) /* Tetris */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "mpr-12356f.ic1", 0x000001, 0x020000, CRC(1e71c1a0) SHA1(44b2312792e49d46d71e0417a7f022e5ffddbbfe) )
	ROM_LOAD16_BYTE( "mpr-12357f.ic2", 0x000000, 0x020000, CRC(d52ca49c) SHA1(a9159892eee2c0cf28ebfcfa99f81f80781851c6) )
	ROM_LOAD16_BYTE( "epr-12368-22.ic3", 0x300001, 0x08000, CRC(1c1b6468) SHA1(568a38f4186167486e39ab4aa2c1ceffd0b81156) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_tfor2 ) /* Thunder Force II */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12559.ic1", 0x000000, 0x080000, CRC(b093bee3) SHA1(0bf6194c3d228425f8cf1903ed70d8da1b027b6a) )
	ROM_LOAD16_BYTE( "epr-12368-11.ic2", 0x300001, 0x08000, CRC(f4f27e8d) SHA1(ae1a2823deb416c53838115966f1833d5dac72d4) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_tlbba ) /* Tommy Lasorda Baseball */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12706.ic1", 0x000000, 0x080000, CRC(8901214f) SHA1(f5ec166be1cf9b86623b9d7a78ec903b899da32a) )
	ROM_LOAD16_BYTE( "epr-12368-35.ic2", 0x300001, 0x08000, CRC(67bbe482) SHA1(6fc283b22e68befabb44b2cc61a7f82a71d6f029) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_ggolf ) /* Great Golf */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp11129f.ic1", 0x000000, 0x020000, CRC(c6611c84) SHA1(eab0eed872dd26b13bcf0b2dd74fcbbc078812c9) )
	ROM_LOAD16_BYTE( "epr-12368-04.ic2", 0x300001, 0x08000, CRC(62e5579b) SHA1(e1f531be5c40a1216d4192baeda9352384444410) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_gsocr ) /* Great Soccer */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp10747f.ic1", 0x000000, 0x020000, BAD_DUMP CRC(9cf53703) SHA1(c6b4d1de56bd5bf067ec7fc80449c07686d01337) )
	ROM_LOAD16_BYTE( "epr-12368-05.ic2", 0x300001, 0x08000, CRC(bab91fcc) SHA1(a160c9d34b253e93ac54fdcef33f95f44d8fa90c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_parlg ) /* Parlour Games */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp11404.ic1", 0x000000, 0x020000, CRC(e030e66c) SHA1(06664daf208f07cb00b603b12eccfc3f01213a17) )
	ROM_LOAD16_BYTE( "epr-12368-29.ic2", 0x300001, 0x08000, CRC(534151e8) SHA1(219238d90c1d3ac07ff64c9a2098b490fff68f04) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_shnbi ) /* Shinobi */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp11706.ic1", 0x000000, 0x040000, CRC(0c6fac4e) SHA1(7c0778c055dc9c2b0aae1d166dbdb4734e55b9d1) )
	ROM_LOAD16_BYTE( "epr-12368-08.ic2", 0x300001, 0x08000, CRC(103a0459) SHA1(d803ddf7926b83785e8503c985b8c78e7ccb5dac) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_revsh ) /* The Revenge Of Shinobi */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp12675.ic1", 0x000000, 0x080000, CRC(672a1d4d) SHA1(5fd0af14c8f2cf8ceab1ae61a5a19276d861289a) )
	ROM_LOAD16_BYTE( "epr-12368-28.ic2", 0x300001, 0x08000, CRC(0d30bede) SHA1(73a090d84b78a570e02fb54a33666dcada52849b) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_aftrb ) /* After Burner */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mp11271.ic1", 0x000000, 0x080000, CRC(1c951f8e) SHA1(51531df038783c84640a0cab93122e0b59e3b69a) )
	ROM_LOAD16_BYTE( "epr-12368-10.ic2", 0x300001, 0x08000, CRC(2a7cb590) SHA1(2236963bddc89ca9045b530259cc7b5ccf889eaf) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

ROM_START( mt_tgolf ) /* Arnold Palmer Tournament Golf */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "mpr-12645f.ic1", 0x000000, 0x080000, CRC(c07ef8d2) SHA1(9d111fdc7bb92d52bfa048cd134aa488b4f475ef) )
	ROM_LOAD16_BYTE( "epr-12368-31.ic2", 0x300001, 0x08000, CRC(30af7e4a) SHA1(baf91d527393dc90aba9371abcb1e690bcc83c7e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	MEGATECH_BIOS
ROM_END

/******************************************************************************
	Driver init and game drivers
******************************************************************************/

static DRIVER_INIT( megatech )
{
	mt_sms_cart = 0;
	init_segac2();
}

static DRIVER_INIT( mt_sms )
{
	const UINT8 *rom = memory_region(REGION_CPU1);
	int i;

	/* the cart is loaded word swapped for the 68000 (like the console dumps),
	   the Z80 sees the file in its byte order; 32 banks of 16KB for the mapper */
	sms_rom = auto_malloc(0x80000);
	for (i = 0; i < 0x80000; i++)
		sms_rom[i] = rom[BYTE_XOR_LE(i)];

	mt_sms_cart = 1;
	init_segac2();
}

/* nn */ /* nn is part of the instruction rom name, should there be a game for each number? */
/* -- */ GAMEX( 1989, megatech, 0,        megatech, megatech, megatech, ROT0, "Sega", "Mega-Tech BIOS", NOT_A_DRIVER )
/* 01 */ GAMEX( 1988, mt_beast, megatech, megatech, megatech, megatech, ROT0, "Sega", "Altered Beast (Mega-Tech)", GAME_IMPERFECT_GRAPHICS )
/* 02 */ GAME ( 1988, mt_shar2, megatech, megatech, megatech, megatech, ROT0, "Sega", "Space Harrier II (Mega-Tech)" )
/* 03 */ GAME ( 1988, mt_stbld, megatech, megatech, megatech, megatech, ROT0, "Sega", "Super Thunder Blade (Mega-Tech)" )
/* 04 */ GAME ( 1987, mt_ggolf, megatech, megatech_sms, megatech, mt_sms, ROT0, "Sega", "Great Golf (Mega-Tech, SMS based)" )
/* 05 */ GAMEX( 198?, mt_gsocr, megatech, megatech_sms, megatech, mt_sms, ROT0, "Sega", "Great Soccer (Mega-Tech, SMS based)", GAME_NOT_WORKING ) /* bad dump, as in current MAME */
/* 06 */ /* unknown*/
/* 07 */ /* unknown*/
/* 08 */ GAME ( 1987, mt_shnbi, megatech, megatech_sms, megatech, mt_sms, ROT0, "Sega", "Shinobi (Mega-Tech, SMS based)" )
/* 09 */ /* unknown*/
/* 10 */ GAME ( 1987, mt_aftrb, megatech, megatech_sms, megatech, mt_sms, ROT0, "Sega", "After Burner (Mega-Tech, SMS based)" )
/* 11 */ GAMEX( 1989, mt_tfor2, megatech, megatech, megatech, megatech, ROT0, "Techno Soft / Sega", "Thunder Force II MD (Mega-Tech)", GAME_NOT_WORKING )
/* 12 */ /* unknown*/
/* 13 */ GAME ( 1986, mt_astro, megatech, megatech_sms, megatech, mt_sms, ROT0, "Sega", "Astro Warrior (Mega-Tech, SMS based)" )
/* 14 */ /* unknown*/
/* 15 */ /* unknown*/
/* 16 */ /* unknown*/ 
/* 17 */ /* unknown*/
/* 18 */ /* unknown*/
/* 19 */ /* unknown*/
/* 20 */ GAME ( 1989, mt_lastb, megatech, megatech, megatech, megatech, ROT0, "Sega", "Last Battle (Mega-Tech)" )
/* 21 */ GAME ( 1989, mt_wcsoc, megatech, megatech, megatech, megatech, ROT0, "Sega", "World Championship Soccer (Mega-Tech)" )
/* 22 */ GAME ( 1989, mt_tetri, megatech, megatech, megatech, megatech, ROT0, "Sega", "Tetris (Mega-Tech)" )
/* 23 */ GAME ( 1989, mt_gng, megatech, megatech, megatech, megatech, ROT0, "Capcom / Sega", "Ghouls'n Ghosts (Mega-Tech)" )
/* 24 */ /* unknown*/
/* 25 */ GAMEX( 1989, mt_gaxe, megatech, megatech, megatech, megatech, ROT0, "Sega", "Golden Axe (Mega-Tech)", GAME_NOT_WORKING )
/* 26 */ /* unknown*/
/* 27 */ GAME ( 1989, mt_mystd, megatech, megatech, megatech, megatech, ROT0, "Sega", "Mystic Defender (Mega-Tech)" )
/* 28 */ GAME ( 1989, mt_revsh, megatech, megatech, megatech, megatech, ROT0, "Sega", "The Revenge of Shinobi (Mega-Tech)" )
/* 29 */ GAME ( 1987, mt_parlg, megatech, megatech_sms, megatech, mt_sms, ROT0, "Sega", "Parlour Games (Mega-Tech, SMS based)" )
/* 30 */ /* unknown*/
/* 31 */ GAME ( 1989, mt_tgolf, megatech, megatech, megatech, megatech, ROT0, "Sega", "Arnold Palmer Tournament Golf (Mega-Tech)" )
/* 32 */ /* unknown*/
/* 33 */ /* unknown*/
/* 34 */ /* unknown*/
/* 35 */ GAME ( 1989, mt_tlbba, megatech, megatech, megatech, megatech, ROT0, "Sega", "Tommy Lasorda Baseball (Mega-Tech)" )
/* 36 */ /* unknown*/
/* 37 */ /* unknown*/
/* 38 */ GAME ( 1990, mt_eswat, megatech, megatech, megatech, megatech, ROT0, "Sega", "Cyber Police ESWAT: Enhanced Special Weapons and Tactics (Mega-Tech)" )
/* 39 */ GAMEX( 1990, mt_smgp, megatech, megatech, megatech, megatech, ROT0, "Sega", "Super Monaco GP (Mega-Tech)", GAME_NOT_WORKING )
/* 40 */ GAMEX( 1990, mt_mwalk, megatech, megatech, megatech, megatech, ROT0, "Sega", "Michael Jackson's Moonwalker (Mega-Tech)", GAME_NOT_WORKING )
/* 41 */ /* unknown*/
/* 42 */ /* unknown*/
/* 43 */ /* unknown*/
/* 44 */ /* unknown*/
/* 45 */ /* unknown*/
/* 46 */ /* unknown*/
/* 47 */ /* unknown*/
/* 48 */ /* unknown*/
/* 49 */ GAME ( 1991, mt_bbros, megatech, megatech, megatech, megatech, ROT0, "Sega", "Bonanza Bros. (Mega-Tech)" )
/* 50 */ /* unknown*/
/* 51 */ /* unknown*/
/* 52 */ GAME ( 1991, mt_sonic, megatech, megatech, megatech, megatech, ROT0, "Sega", "Sonic The Hedgehog (Mega-Tech, set 1)" )
/* 53 */ GAME ( 1990, mt_fshrk, megatech, megatech, megatech, megatech, ROT0, "Toaplan / Sega", "Fire Shark (Mega-Tech)" )
/* 54 */ /* unknown*/
/* 55 */ /* unknown*/
/* 56 */ /* unknown*/
/* 57 */ GAME ( 1991, mt_gaxe2, megatech, megatech, megatech, megatech, ROT0, "Sega", "Golden Axe II (Mega-Tech)" )
/* 58 */ /* unknown*/
/* 59 */ /* unknown*/
/* 60 */ GAME ( 1992, mt_kcham, megatech, megatech, megatech, megatech, ROT0, "Sega", "Kid Chameleon (Mega-Tech)" )
/* more? */
