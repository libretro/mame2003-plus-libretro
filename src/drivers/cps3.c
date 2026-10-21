/***************************************************************************

    Capcom CPS-3

    A C port of current MAME's cps3.cpp (David Haywood, Andreas Naive,
    Tomasz Slanina, ElSemi, MetalliC and others).

    Hardware:
    - main CPU: a custom Hitachi SH-2 at 25 MHz that decrypts the code and
      data it reads from the BIOS, the program SIMMs and its cache RAM
    - program in two SIMMs of 4 flash chips (Fujitsu 29F016A, 2 MB each),
      graphics and samples in up to four SIMMs of 8 chips
    - 2D video with sprites, tilemaps drawn as sprites, a text layer, palette
      and character DMA with RLE decompression
    - 16 voice 8-bit sample sound
    - a SCSI CD-ROM, which writes the game into the flash SIMMs

    The CD drive is not emulated: the games run from SIMMs already written,
    the "NO CD" sets with their own BIOS, the CD sets with the SIMM data of
    the same version (as FBNeo does).

    Differences from MAME, see docs/capcom-cps3.md:
    - CD sets play without the CD image
    - an idle loop skip per game (FBNeo's addresses)
    - the flash chips are not saved: no set writes them while playing
    - tiles are read from the character RAM directly (no gfx elements)

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "cpu/sh2/sh2.h"
#include "includes/cps3.h"

#define USER5_LENGTH		0x4000000	/* graphics/sample SIMMs 3-6, chip pairs interleaved */
#define PROGRAM_LENGTH		0x1000000	/* program SIMMs 1-2 */

/* in this core's memory handlers a set bit in mem_mask is a byte NOT accessed */
#define ACCESSING_BITS(m)	((mem_mask & (m)) == 0)

static data32_t *cps3_mainram;
static data32_t *cps3_spriteram;
static data32_t cps3_ppu_gscroll[8];
static data32_t cps3_tilemap_regs[16];
static data32_t cps3_ppu_crtc_zoom[8];
static data32_t *cps3_sh2cache_ram;
static data32_t cps3_sh2cache_ram_decrypted[0x400 / 4];

static UINT32 key1, key2;
static int alt_encryption;		/* 0 normal, 1 sfiii2 (dead cart: data not encrypted) */
static int skip_bios;			/* the Red Earth CD BIOSes: start the game directly */

/* the cart's region byte: the low byte of a BIOS word, region in bits 0-3 */
static UINT32 region_word;
static UINT8 region_bios;		/* as dumped */

static UINT8 *program_rom;		/* REGION_USER1: chip i of word n at byte 4 * n + i */
static UINT32 *decrypted_gamerom;	/* the program as the CPU fetches opcodes */
static UINT8 *user5;			/* REGION_USER2: the graphics/sample byte stream */

static UINT32 *char_ram;		/* 8 MB, each word holds 4 bytes, first byte in the top bits */
static UINT16 *colourram;
static UINT32 *mame_colours;	/* 15-bit colours, after the fade */
static UINT32 *pens;			/* the same as pens of the screen bitmap */
static UINT8 ss_ram[0x8000];
static UINT32 *spritelist;
static UINT32 ppu_gscroll_buff[8];

static UINT32 *renderbuffer;	/* palette indexes, 1024 x 448 */
#define RENDER_W	1024
#define RENDER_H	448
static struct rectangle renderbuffer_clip;
static int screenwidth;

static UINT32 cram_gfxflash_bank;
static UINT32 cram_bank;
static UINT16 current_eeprom_read;
static UINT32 eeprom[0x80 / 4];
static UINT32 paldma_source, paldma_realsource, paldma_dest, paldma_fade, paldma_other2, paldma_length;
static UINT32 chardma_source, chardma_other;
static INT32 current_table_address;
static UINT16 dma_status;
static UINT16 spritelist_dma;
static UINT16 ss_hscroll, ss_vscroll;
static UINT8 ss_pal_base;
static UINT32 rle_length;
static UINT8 last_normal_byte;
static UINT16 lastb, lastb2;

static mame_timer *dma_timer;
static mame_timer *spritelist_dma_timer;

static int pen_shift[3];


/***************************************************************************
    Encryption
***************************************************************************/

static UINT16 rotate_left(UINT16 value, int n)
{
	int aux = value >> (16 - n);
	return ((value << n) | aux) % 0x10000;
}

static UINT16 rotxor(UINT16 val, UINT16 xorval)
{
	UINT16 res = val + rotate_left(val, 2);

	res = rotate_left(res, 4) ^ (res & (val ^ xorval));

	return res;
}

static UINT32 cps3_mask(UINT32 address, UINT32 k1, UINT32 k2)
{
	UINT16 val;

	address ^= k1;

	val = (address & 0xffff) ^ 0xffff;
	val = rotxor(val, k2 & 0xffff);
	val ^= (address >> 16) ^ 0xffff;
	val = rotxor(val, k2 >> 16);
	val ^= (address & 0xffff) ^ (k2 & 0xffff);

	return val | (val << 16);
}


/***************************************************************************
    Flash SIMMs: Fujitsu 29F016A, 2 MB, AMD command set
***************************************************************************/

enum
{
	FM_NORMAL,
	FM_READAMDID1,
	FM_READAMDID2,
	FM_READAMDID3,
	FM_ERASEAMD1,
	FM_ERASEAMD2,
	FM_ERASEAMD3,
	FM_BYTEPROGRAM
};

struct cps3_flash
{
	UINT8 *data;	/* byte 0 of the chip */
	int stride;		/* distance between two bytes of the chip */
	int present;
	int mode;
};

/* program: SIMM 1 and 2, 4 chips each; graphics: SIMM 3 to 6, 8 chips each */
static struct cps3_flash flash_prog[2][4];
static struct cps3_flash flash_gfx[4][8];
static INT32 flash_modes[2 * 4 + 4 * 8];

static UINT8 flash_read(struct cps3_flash *f, UINT32 offset)
{
	if (f->mode == FM_READAMDID3)
	{
		switch (offset & 0xff)
		{
			case 0: return 0x04;	/* Fujitsu */
			case 1: return 0xad;	/* 29F016A */
			default: return 0x00;
		}
	}
	return f->data[(offset & 0x1fffff) * f->stride];
}

static void flash_write(struct cps3_flash *f, UINT32 offset, UINT8 data)
{
	UINT32 a = offset & 0x1fffff;

	switch (f->mode)
	{
		default:
		case FM_NORMAL:
		case FM_READAMDID3:
			if ((a & 0xfff) == 0x555 && data == 0xaa)
				f->mode = FM_READAMDID1;
			else if (data == 0xf0 || data == 0xff)
				f->mode = FM_NORMAL;
			break;
		case FM_READAMDID1:
			if ((a & 0xfff) == 0x2aa && data == 0x55)
				f->mode = FM_READAMDID2;
			else
				f->mode = FM_NORMAL;
			break;
		case FM_READAMDID2:
			if ((a & 0xfff) == 0x555 && data == 0x90)
				f->mode = FM_READAMDID3;
			else if ((a & 0xfff) == 0x555 && data == 0x80)
				f->mode = FM_ERASEAMD1;
			else if ((a & 0xfff) == 0x555 && data == 0xa0)
				f->mode = FM_BYTEPROGRAM;
			else
				f->mode = FM_NORMAL;
			break;
		case FM_ERASEAMD1:
			f->mode = ((a & 0xfff) == 0x555 && data == 0xaa) ? FM_ERASEAMD2 : FM_NORMAL;
			break;
		case FM_ERASEAMD2:
			f->mode = ((a & 0xfff) == 0x2aa && data == 0x55) ? FM_ERASEAMD3 : FM_NORMAL;
			break;
		case FM_ERASEAMD3:
		{
			UINT32 i, base = 0, len = 0;
			if ((a & 0xfff) == 0x555 && data == 0x10)
			{
				base = 0;		/* chip erase */
				len = 0x200000;
			}
			else if (data == 0x30)
			{
				base = a & ~0xffff;	/* sector erase, 64 KB */
				len = 0x10000;
			}
			for (i = 0; i < len; i++)
				f->data[(base + i) * f->stride] = 0xff;
			f->mode = FM_NORMAL;
			break;
		}
		case FM_BYTEPROGRAM:
			f->data[a * f->stride] = data;
			f->mode = FM_NORMAL;
			break;
	}
}

static void flash_setup(int simm2, int simm5_chips, int simm6)
{
	int s, c;

	program_rom = memory_region(REGION_USER1);
	user5 = memory_region(REGION_USER2);

	for (s = 0; s < 2; s++)
		for (c = 0; c < 4; c++)
		{
			struct cps3_flash *f = &flash_prog[s][c];
			f->data = program_rom + s * 0x800000 + c;
			f->stride = 4;
			f->present = (s == 0) || simm2;
			f->mode = FM_NORMAL;
		}

	for (s = 0; s < 4; s++)
		for (c = 0; c < 8; c++)
		{
			struct cps3_flash *f = &flash_gfx[s][c];
			f->data = user5 + (s * 4 + c / 2) * 0x400000 + (c & 1);
			f->stride = 2;
			if (s == 2)
				f->present = c < simm5_chips;
			else if (s == 3)
				f->present = simm6;
			else
				f->present = 1;
			f->mode = FM_NORMAL;
		}
}


/***************************************************************************
    Program SIMMs and the decrypted copy
***************************************************************************/

static UINT32 program_word(UINT32 n)
{
	const UINT8 *p = &program_rom[n * 4];
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void decrypt_program_word(UINT32 n)
{
	decrypted_gamerom[n] = program_word(n) ^ cps3_mask(0x06000000 + n * 4, key1, key2);
}

static UINT32 flashmain_r(int which, offs_t offset, data32_t mem_mask)
{
	struct cps3_flash *f = flash_prog[which];
	UINT32 result = 0;

	if (!f[0].present)
		return 0xffffffff;

	if (ACCESSING_BITS(0xff000000)) result |= flash_read(&f[0], offset) << 24;
	if (ACCESSING_BITS(0x00ff0000)) result |= flash_read(&f[1], offset) << 16;
	if (ACCESSING_BITS(0x0000ff00)) result |= flash_read(&f[2], offset) << 8;
	if (ACCESSING_BITS(0x000000ff)) result |= flash_read(&f[3], offset) << 0;
	return result;
}

static UINT32 flash_r(int which, offs_t offset, data32_t mem_mask)
{
	struct cps3_flash *f = flash_prog[which];

	/* reading the array: the decrypted copy already holds it */
	if (f[0].mode == FM_NORMAL && f[1].mode == FM_NORMAL && f[2].mode == FM_NORMAL && f[3].mode == FM_NORMAL)
	{
		if (!f[0].present)
			return 0xffffffff;
		if (!alt_encryption)
			return decrypted_gamerom[which * 0x200000 + offset];
		return program_word(which * 0x200000 + offset);
	}

	if (alt_encryption)
		return flashmain_r(which, offset, mem_mask);
	return flashmain_r(which, offset, mem_mask) ^ cps3_mask(0x06000000 + which * 0x800000 + offset * 4, key1, key2);
}

static void flash_w(int which, offs_t offset, data32_t data, data32_t mem_mask)
{
	struct cps3_flash *f = flash_prog[which];

	if (!f[0].present)
		return;

	if (ACCESSING_BITS(0xff000000)) flash_write(&f[0], offset, (data >> 24) & 0xff);
	if (ACCESSING_BITS(0x00ff0000)) flash_write(&f[1], offset, (data >> 16) & 0xff);
	if (ACCESSING_BITS(0x0000ff00)) flash_write(&f[2], offset, (data >> 8) & 0xff);
	if (ACCESSING_BITS(0x000000ff)) flash_write(&f[3], offset, (data >> 0) & 0xff);

	/* keep the copy the CPU executes from up to date */
	decrypt_program_word(which * 0x200000 + offset);
}

static READ32_HANDLER( flash1_r ) { return flash_r(0, offset, mem_mask); }
static READ32_HANDLER( flash2_r ) { return flash_r(1, offset, mem_mask); }
static WRITE32_HANDLER( flash1_w ) { flash_w(0, offset, data, mem_mask); }
static WRITE32_HANDLER( flash2_w ) { flash_w(1, offset, data, mem_mask); }


/* the CPU fetches decrypted opcodes from the BIOS (decrypted at init), the
   program SIMMs and its cache RAM */
static UINT32 *bios_words;

static data16_t cps3_opcode_read(offs_t pc)
{
	UINT32 a = pc;
	UINT32 word;

	if (a < 0x40000000)
		a &= 0x07ffffff;	/* cache-through mirror */

	if (a - 0x06000000 < 0x01000000)
		word = decrypted_gamerom[(a - 0x06000000) >> 2];
	else if (a < 0x80000)
		word = bios_words[a >> 2];
	else if (a - 0xc0000000 < 0x400)
		word = cps3_sh2cache_ram_decrypted[(a & 0x3ff) >> 2];
	else
		return cpu_readmem32bedw_word(a);

	return (a & 2) ? (word & 0xffff) : (word >> 16);
}

/* the SH-2 DMA bypasses the decryption: undo what the handlers decrypted */
static data32_t cps3_dma_read_hook(UINT32 src, UINT32 dst, data32_t data, int size)
{
	if (size == 0)
		data <<= 8 * (3 - (src & 3));
	else if (size == 1)
		data <<= (src & 2) ? 0 : 16;

	if (src < 0x80000)
		data ^= cps3_mask(src & ~3, key1, key2);
	else if (src >= 0x6000000 && src < 0x7000000)
	{
		if (!alt_encryption)
			data ^= cps3_mask(src & ~3, key1, key2);
	}

	if (size == 0)
		data = (data >> (8 * (3 - (src & 3)))) & 0xff;
	else if (size == 1)
		data = (data >> ((src & 2) ? 0 : 16)) & 0xffff;

	return data;
}

/* idle loop: the games wait for the vblank interrupt reading a word of the
   main RAM; when the loop reads it, the CPU skips to the next event (the
   addresses are FBNeo's) */
static UINT32 speedup_ram_address, speedup_code_address;

static READ32_HANDLER( cps3_speedup_r )
{
	if (activecpu_get_pc() == speedup_code_address)
		activecpu_eat_cycles(activecpu_get_icount());
	return cps3_mainram[(speedup_ram_address & 0x7fffc) / 4];
}

static struct sh2_config cps3_sh2_config =
{
	0, cps3_opcode_read, cps3_dma_read_hook
};


/***************************************************************************
    Video
***************************************************************************/

enum
{
	CPS3_TRANSPARENCY_PEN_INDEX,
	CPS3_TRANSPARENCY_PEN_INDEX_BLEND
};

static INLINE UINT8 char_byte(UINT32 a)
{
	a &= 0x7fffff;
	return (char_ram[a >> 2] >> (8 * (3 - (a & 3)))) & 0xff;
}

static INLINE void char_byte_w(UINT32 a, UINT8 data)
{
	int shift;
	a &= 0x7fffff;
	shift = 8 * (3 - (a & 3));
	char_ram[a >> 2] = (char_ram[a >> 2] & ~(0xff << shift)) | (data << shift);
}

/* 16x16 8bpp tiles from the character RAM, zoomed, into the render buffer
   (palette indexes) */
static void cps3_drawgfxzoom_tile(const struct rectangle *clip, UINT32 code, UINT32 color,
		int flipx, int flipy, int sx, int sy, int transparency, int granularity,
		int scalex, int scaley)
{
	UINT32 palbase;
	UINT32 tilebase;
	int sprite_screen_height, sprite_screen_width;
	int dx, dy, ex, ey, x_index_base, y_index, x, y;

	if (!scalex || !scaley)
		return;

	palbase = (granularity * color) & 0x1ffff;
	tilebase = (code % 0x8000) * 256;

	sprite_screen_height = (scaley * 16 + 0x8000) >> 16;
	sprite_screen_width = (scalex * 16 + 0x8000) >> 16;
	if (!sprite_screen_width || !sprite_screen_height)
		return;

	dx = (16 << 16) / sprite_screen_width;
	dy = (16 << 16) / sprite_screen_height;

	ex = sx + sprite_screen_width;
	ey = sy + sprite_screen_height;

	if (flipx)
	{
		x_index_base = (sprite_screen_width - 1) * dx;
		dx = -dx;
	}
	else
		x_index_base = 0;

	if (flipy)
	{
		y_index = (sprite_screen_height - 1) * dy;
		dy = -dy;
	}
	else
		y_index = 0;

	if (sx < clip->min_x)
	{
		int pixels = clip->min_x - sx;
		sx += pixels;
		x_index_base += pixels * dx;
	}
	if (sy < clip->min_y)
	{
		int pixels = clip->min_y - sy;
		sy += pixels;
		y_index += pixels * dy;
	}
	if (ex > clip->max_x + 1)
		ex = clip->max_x + 1;
	if (ey > clip->max_y + 1)
		ey = clip->max_y + 1;

	if (ex <= sx)
		return;

	for (y = sy; y < ey; y++)
	{
		const UINT32 *row = &char_ram[(tilebase + (y_index >> 16) * 16) >> 2];
		UINT32 *dest = &renderbuffer[y * RENDER_W];
		int x_index = x_index_base;

		for (x = sx; x < ex; x++)
		{
			int px = x_index >> 16;
			int c = (row[px >> 2] >> (8 * (3 - (px & 3)))) & 0xff;
			if (c != 0)
			{
				if (transparency == CPS3_TRANSPARENCY_PEN_INDEX)
					dest[x] = c | palbase;
				else if (granularity == 64)
					dest[x] |= (c & 0xf) << 13;	/* sfiii world map spotlight */
				else
					dest[x] |= ((c & 1) << 15) | ((color & 1) << 16);	/* jojo intro, warzard swords, shadows */
			}
			x_index += dx;
		}
		y_index += dy;
	}
}

static void draw_tilemapsprite_line(UINT32 *regs, int drawline, const struct rectangle *cliprect)
{
	int scrollx, scrolly, linescroll_enable, global_flip_x, global_flip_y;
	UINT32 linebase, mapbase;
	int line, xflip_mask, tileline, tilesubline, x;
	struct rectangle clip;

	if (!(regs[1] & 0x00008000))
		return;

	scrollx = (regs[0] & 0xffff0000) >> 16;
	scrolly = (regs[0] & 0x0000ffff) >> 0;
	linescroll_enable = (regs[1] & 0x00004000) >> 14;
	global_flip_x = (regs[1] & 0x00000800) >> 11;	/* warzard special moves */
	global_flip_y = (regs[1] & 0x00000400) >> 10;	/* sfiii2 loading screens */
	linebase = (regs[2] & 0x7f000000) >> 24;
	mapbase = (regs[2] & 0x007f0000) >> 16;

	mapbase <<= 10;
	linebase <<= 10;
	scrolly += 4;

	line = drawline + scrolly;
	line &= 0x3ff;

	if (global_flip_y)
		line ^= 0x3ff;
	xflip_mask = global_flip_x ? 0x3f : 0;

	tileline = (line / 16) + 1;
	tilesubline = line % 16;

	if (linescroll_enable)
		scrollx += (cps3_spriteram[linebase + ((line + 16) & 0x3ff)] >> 16) & 0x3ff;	/* sfiii Ryu's stage, 2nd round floor */

	clip.min_x = cliprect->min_x;
	clip.max_x = cliprect->max_x;
	clip.min_y = clip.max_y = drawline;

	for (x = cliprect->min_x / 16; x < (cliprect->max_x / 16) + 2; x++)
	{
		UINT32 dat = cps3_spriteram[mapbase + ((tileline & 63) * 64) + (((x + scrollx / 16) & 63) ^ xflip_mask)];
		UINT32 tileno = (dat & 0xfffe0000) >> 17;
		int xflip = (dat & 0x00001000) >> 12;
		int yflip = (dat & 0x00000800) >> 11;
		int alpha = (dat & 0x00000400) >> 10;
		int bpp = (dat & 0x00000200) >> 9;
		UINT32 colour = (dat & 0x000001ff) >> 0;
		int trans = alpha ? CPS3_TRANSPARENCY_PEN_INDEX_BLEND : CPS3_TRANSPARENCY_PEN_INDEX;

		xflip ^= xflip_mask & 1;

		cps3_drawgfxzoom_tile(&clip, tileno, colour, xflip, yflip, (x * 16) - scrollx % 16, drawline - tilesubline,
				trans, bpp ? 64 : 256, 0x10000, 0x10000);
	}
}

/* the text layer: 8x8 4bpp tiles from the SS RAM, straight to the screen,
   one line at a time */
static void draw_fg_line(struct mame_bitmap *bitmap, int line, int min_x, int max_x)
{
	int scrolly = (-ss_vscroll) & 0x100;
	int offset = ((line + scrolly) / 8 * 128) & 0x1fff;
	int rowscroll = ss_ram[((line + scrolly - 1) & 0x1ff) * 2 + 0x2000];	/* JoJo's combo meter */
	int tiley = line & 7;
	int x, half;

	for (x = 0; x < 64; x++)
	{
		UINT16 data = ss_ram[offset] | (ss_ram[offset + 1] << 8);
		UINT32 tile = (data & 0x01ff) >> 0;
		int pal = (data & 0x3e00) >> 9;
		int flipy = (data & 0x4000) >> 14;
		int flipx = (data & 0x8000) >> 15;
		const UINT8 *row;
		UINT32 palbase;

		pal += ss_pal_base << 5;
		palbase = (16 * pal) & 0x1ffff;
		row = &ss_ram[0x4000 + tile * 32 + (flipy ? 7 - tiley : tiley) * 4];
		offset += 2;

		/* the layer is drawn twice, 512 pixels apart */
		for (half = 0; half < 2; half++)
		{
			int sx = half * 512 + (x * 8) - rowscroll;
			int px;

			if (sx > max_x || sx + 7 < min_x)
				continue;

			for (px = 0; px < 8; px++)
			{
				int dx = sx + px;
				int bx = flipx ? 7 - px : px;
				int c;

				if (dx < min_x || dx > max_x)
					continue;
				c = (row[bx >> 1] >> ((bx & 1) ? 4 : 0)) & 0xf;
				if (c != 0)
				{
					if (bitmap->depth == 32)
						((UINT32 *)bitmap->line[line])[dx] = pens[(palbase + c) & 0x1ffff];
					else
						((UINT16 *)bitmap->line[line])[dx] = pens[(palbase + c) & 0x1ffff];
				}
			}
		}
	}
}

static void draw_fg_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int line;
	for (line = cliprect->min_y; line <= cliprect->max_y; line++)
		draw_fg_line(bitmap, line, cliprect->min_x, cliprect->max_x);
}

static INLINE UINT8 get_fade(int c, int f)
{
	/* bit 6 fade enable, bit 5 fade mode (1 = invert input values and output), bits 4-0 fade value */
	if (f & 0x40)
		c = (f & 0x20) ? ((((c ^ 0x1f) * (~f & 0x1f)) >> 5) ^ 0x1f) : (c * (f & 0x1f) >> 5);
	return c;
}

static void set_mame_colours(int colournum, UINT16 data, UINT32 fadeval)
{
	int r = (data >> 0) & 0x1f;
	int g = (data >> 5) & 0x1f;
	int b = (data >> 10) & 0x1f;

	if (fadeval & 0x40400040)
	{
		r = get_fade(r, (fadeval & 0x7f000000) >> 24);
		g = get_fade(g, (fadeval & 0x007f0000) >> 16);
		b = get_fade(b, (fadeval & 0x0000007f) >> 0);

		data = (data & 0x8000) | (r << 0) | (g << 5) | (b << 10);
	}

	colournum &= 0x1ffff;
	colourram[colournum] = data;
	mame_colours[colournum] = (r << 0) | (g << 5) | (b << 10);

	/* no colour extension: the video DAC's RGB bits 0-2 are grounded */
	if (Machine->color_depth == 32)
		pens[colournum] = ((r << 3) << pen_shift[0]) | ((g << 3) << pen_shift[1]) | ((b << 3) << pen_shift[2]);
	else
		pens[colournum] = (r << pen_shift[0]) | (g << pen_shift[1]) | (b << pen_shift[2]);
}

static void compute_pen_layout(void)
{
	int i;
	for (i = 0; i < 3; i++)
	{
		UINT32 c = direct_rgb_components[i];
		int sh = 0;
		while (c && !(c & 1))
		{
			c >>= 1;
			sh++;
		}
		pen_shift[i] = sh;
	}
}

/* the colour RAM holds the colours after the fade: rebuild the pens */
static void cps3_video_postload(void)
{
	int i;
	for (i = 0; i < 0x20000; i++)
		set_mame_colours(i, colourram[i], 0);
}

VIDEO_START( cps3 )
{
	char_ram = auto_malloc(0x800000);
	colourram = auto_malloc(0x20000 * sizeof(UINT16));
	mame_colours = auto_malloc(0x20000 * sizeof(UINT32));
	pens = auto_malloc(0x20000 * sizeof(UINT32));
	spritelist = auto_malloc(0x80000);
	renderbuffer = auto_malloc(RENDER_W * RENDER_H * sizeof(UINT32));
	if (!char_ram || !colourram || !mame_colours || !pens || !spritelist || !renderbuffer)
		return 1;

	memset(char_ram, 0, 0x800000);
	memset(colourram, 0, 0x20000 * sizeof(UINT16));
	memset(mame_colours, 0, 0x20000 * sizeof(UINT32));
	memset(pens, 0, 0x20000 * sizeof(UINT32));
	memset(spritelist, 0, 0x80000);
	memset(ss_ram, 0, sizeof(ss_ram));
	memset(renderbuffer, 0, RENDER_W * RENDER_H * sizeof(UINT32));
	spritelist[0] = 0x80000000;

	compute_pen_layout();

	screenwidth = 384;
	renderbuffer_clip.min_x = 0;
	renderbuffer_clip.max_x = screenwidth - 1;
	renderbuffer_clip.min_y = 0;
	renderbuffer_clip.max_y = 224 - 1;

	state_save_register_UINT32("cps3", 0, "char_ram", char_ram, 0x800000 / 4);
	state_save_register_UINT16("cps3", 0, "colourram", colourram, 0x20000);
	state_save_register_UINT8("cps3", 0, "ss_ram", ss_ram, 0x8000);
	state_save_register_UINT32("cps3", 0, "spritelist", spritelist, 0x80000 / 4);
	state_save_register_func_postload(cps3_video_postload);
	return 0;
}

static void fill_renderbuffer(const struct rectangle *clip)
{
	int y;
	for (y = clip->min_y; y <= clip->max_y; y++)
		memset(&renderbuffer[y * RENDER_W + clip->min_x], 0, (clip->max_x - clip->min_x + 1) * sizeof(UINT32));
}

VIDEO_UPDATE( cps3 )
{
	int width = ((cps3_ppu_crtc_zoom[1] & 0xffff0000) >> 16) - (cps3_ppu_crtc_zoom[0] & 0xffff);
	UINT32 fullscreenzoomx, fullscreenzoomy, fszx, fszy;
	int i;

	if (width > 0 && width <= 496 && screenwidth != width)
	{
		int height = ((cps3_ppu_crtc_zoom[5] & 0xffff0000) >> 16) - (cps3_ppu_crtc_zoom[4] & 0xffff);
		if (height <= 0 || height > 224)
			height = 224;
		set_visible_area(0, width - 1, 0, height - 1);
		screenwidth = width;
	}

	fullscreenzoomx = cps3_ppu_crtc_zoom[3] & 0x000000ff;
	fullscreenzoomy = cps3_ppu_crtc_zoom[7] & 0x000000ff;
	/* clamp at 0x80, not known to be accurate */
	if (fullscreenzoomx > 0x80) fullscreenzoomx = 0x80;
	if (fullscreenzoomy > 0x80) fullscreenzoomy = 0x80;

	fszx = (fullscreenzoomx << 16) / 0x40;
	fszy = (fullscreenzoomy << 16) / 0x40;

	if (fullscreenzoomx == 0x40 && fullscreenzoomy == 0x40)
		renderbuffer_clip = *cliprect;
	else
	{
		renderbuffer_clip.min_x = (cliprect->min_x * fszx) >> 16;
		renderbuffer_clip.max_x = (((cliprect->max_x + 1) * fszx + 0x8000) >> 16) - 1;
		renderbuffer_clip.min_y = (cliprect->min_y * fszy) >> 16;
		renderbuffer_clip.max_y = (((cliprect->max_y + 1) * fszy + 0x8000) >> 16) - 1;
	}
	if (renderbuffer_clip.max_x >= RENDER_W) renderbuffer_clip.max_x = RENDER_W - 1;
	if (renderbuffer_clip.max_y >= RENDER_H) renderbuffer_clip.max_y = RENDER_H - 1;
	fill_renderbuffer(&renderbuffer_clip);

	/* sprites */
	for (i = 0x00000 / 4; i < 0x2000 / 4; i += 4)
	{
		int gscroll, xpos, ypos, whichbpp, whichpal, global_xflip, global_yflip, global_alpha, global_bpp;
		UINT32 length, start, global_pal;
		int gscrollx, gscrolly, j;

		if (spritelist[i + 0] & 0x80000000)
			break;

		gscroll = (spritelist[i + 0] & 0x70000000) >> 28;
		length = (spritelist[i + 0] & 0x01ff0000) >> 16;	/* entries in the sprite table */
		start = (spritelist[i + 0] & 0x00007ff0) >> 4;

		xpos = (spritelist[i + 1] & 0x03ff0000) >> 16;
		ypos = spritelist[i + 1] & 0x000003ff;

		whichbpp = (spritelist[i + 2] & 0x40000000) >> 30;	/* jojo title / characters */
		whichpal = (spritelist[i + 2] & 0x20000000) >> 29;
		global_xflip = (spritelist[i + 2] & 0x10000000) >> 28;
		global_yflip = (spritelist[i + 2] & 0x08000000) >> 27;
		global_alpha = (spritelist[i + 2] & 0x04000000) >> 26;	/* all games except warzard */
		global_bpp = (spritelist[i + 2] & 0x02000000) >> 25;
		global_pal = (spritelist[i + 2] & 0x01ff0000) >> 16;

		gscrollx = (ppu_gscroll_buff[gscroll] & 0x03ff0000) >> 16;
		gscrolly = (ppu_gscroll_buff[gscroll] & 0x000003ff) >> 0;
		start = (start * 0x100) >> 2;

		for (j = 0; j < (int)length * 4; j += 4)
		{
			static const int tilestable[4] = { 8, 1, 2, 4 };
			UINT32 value1 = spritelist[(start + j + 0) & 0x1ffff];
			UINT32 value2 = spritelist[(start + j + 1) & 0x1ffff];
			UINT32 value3 = spritelist[(start + j + 2) & 0x1ffff];

			UINT32 tileno = (value1 & 0xfffe0000) >> 17;
			int flipx = (value1 & 0x00001000) >> 12;
			int flipy = (value1 & 0x00000800) >> 11;
			int alpha = (value1 & 0x00000400) >> 10;	/* warzard alpha effects */
			int bpp = (value1 & 0x00000200) >> 9;
			UINT32 pal = (value1 & 0x000001ff);

			int xpos2 = (value2 & 0x03ff0000) >> 16;
			int ypos2 = (value2 & 0x000003ff) >> 0;

			/* the size to draw */
			UINT32 ysizedraw2 = ((value3 & 0x7f000000) >> 24) + 1;
			UINT32 xsizedraw2 = ((value3 & 0x007f0000) >> 16) + 1;
			int ysize2 = ((value3 & 0x0000000c) >> 2);
			int xsize2 = ((value3 & 0x00000003) >> 0);

			if (ysize2 == 0)
				continue;

			if (xsize2 == 0)
			{
				/* an x size of 0 tiles draws a tilemap */
				int tilemapnum = ((value3 & 0x00000030) >> 4);
				UINT32 *regs = &cps3_tilemap_regs[tilemapnum * 4];
				int yy;

				for (yy = 0; yy < (int)ysizedraw2; yy++)
				{
					/* placed like sprites in Y, but from the edge, not the centre */
					int cury_pos = ypos2 + gscrolly - yy;
					cury_pos = ~cury_pos;
					cury_pos -= 18;
					cury_pos &= 0x3ff;

					if (cury_pos >= renderbuffer_clip.min_y && cury_pos <= renderbuffer_clip.max_y)
						draw_tilemapsprite_line(regs, cury_pos, &renderbuffer_clip);
				}
			}
			else
			{
				UINT32 xinc, yinc, xscale, yscale;
				int actualpal, trans, granularity, count, xx;

				ysize2 = tilestable[ysize2];
				xsize2 = tilestable[xsize2];

				xinc = (xsizedraw2 << 16) / xsize2;
				yinc = (ysizedraw2 << 16) / ysize2;

				xscale = xinc / 16;
				yscale = yinc / 16;

				/* round up to avoid holes between the tiles */
				if (xscale & 0xffff) xscale += (1 << 16) / 16;
				if (yscale & 0xffff) yscale += (1 << 16) / 16;

				xsize2 -= 1;
				ysize2 -= 1;

				flipx ^= global_xflip;
				flipy ^= global_yflip;

				if (!flipx) xpos2 += (xsizedraw2 / 2);
				else xpos2 -= (xsizedraw2 / 2);

				ypos2 += (ysizedraw2 / 2);

				if (!flipx) xpos2 -= ((xsize2 + 1) * xinc) >> 16;
				else xpos2 += (xsize2 * xinc) >> 16;

				if (flipy) ypos2 -= (ysize2 * yinc) >> 16;

				/* palette and bpp from the main list or from the sublists */
				actualpal = whichpal ? global_pal : pal;
				granularity = (whichbpp ? global_bpp : bpp) ? 64 : 256;
				trans = (global_alpha || alpha) ? CPS3_TRANSPARENCY_PEN_INDEX_BLEND : CPS3_TRANSPARENCY_PEN_INDEX;

				count = 0;
				for (xx = 0; xx < xsize2 + 1; xx++)
				{
					int current_xpos, yy;

					if (!flipx) current_xpos = (xpos + xpos2 + ((xx * xinc) >> 16));
					else current_xpos = (xpos + xpos2 - ((xx * xinc) >> 16));
					current_xpos += gscrollx;
					current_xpos += 1;
					current_xpos &= 0x3ff;
					if (current_xpos & 0x200) current_xpos -= 0x400;

					for (yy = 0; yy < ysize2 + 1; yy++)
					{
						int current_ypos;

						if (flipy) current_ypos = (ypos + ypos2 + ((yy * yinc) >> 16));
						else current_ypos = (ypos + ypos2 - ((yy * yinc) >> 16));

						current_ypos += gscrolly;
						current_ypos = 0x3ff - current_ypos;
						current_ypos -= 17;
						current_ypos &= 0x3ff;

						if (current_ypos & 0x200) current_ypos -= 0x400;

						cps3_drawgfxzoom_tile(&renderbuffer_clip, tileno + count, actualpal, flipx, flipy,
								current_xpos, current_ypos, trans, granularity, xscale, yscale);
						count++;
					}
				}
			}
		}
	}

	/* the render buffer to the screen, with the full screen zoom */
	{
		UINT32 srcy = cliprect->min_y * fszy;
		int rendery;

		for (rendery = cliprect->min_y; rendery <= cliprect->max_y; rendery++)
		{
			const UINT32 *src = &renderbuffer[((srcy >> 16) < RENDER_H ? (srcy >> 16) : RENDER_H - 1) * RENDER_W];
			UINT32 srcx = cliprect->min_x * fszx;
			int renderx;

			if (bitmap->depth == 32)
			{
				UINT32 *dst = (UINT32 *)bitmap->line[rendery];
				for (renderx = cliprect->min_x; renderx <= cliprect->max_x; renderx++)
				{
					dst[renderx] = pens[src[(srcx >> 16) & (RENDER_W - 1)] & 0x1ffff];
					srcx += fszx;
				}
			}
			else
			{
				UINT16 *dst = (UINT16 *)bitmap->line[rendery];
				for (renderx = cliprect->min_x; renderx <= cliprect->max_x; renderx++)
				{
					dst[renderx] = pens[src[(srcx >> 16) & (RENDER_W - 1)] & 0x1ffff];
					srcx += fszx;
				}
			}
			srcy += fszy;
		}
	}

	draw_fg_layer(bitmap, cliprect);
}


/***************************************************************************
    Memory handlers
***************************************************************************/

/* SS RAM, even bytes only: 0x0000 tilemap, 0x2000 rowscroll, 0x4000 characters */
static READ32_HANDLER( ssram_r )
{
	return (ss_ram[(offset * 2) & 0x7fff] << 16) | ss_ram[(offset * 2 + 1) & 0x7fff];
}

static WRITE32_HANDLER( ssram_w )
{
	if (ACCESSING_BITS(0x00ff0000)) ss_ram[(offset * 2) & 0x7fff] = (data >> 16) & 0xff;
	if (ACCESSING_BITS(0x000000ff)) ss_ram[(offset * 2 + 1) & 0x7fff] = data & 0xff;
}

static void ssregs_byte_w(int offset, UINT8 data)
{
	switch (offset)
	{
		case 0x07: ss_hscroll = (ss_hscroll & 0xff00) | data; break;
		case 0x08: ss_hscroll = (ss_hscroll & 0xff) | (data << 8); break;
		case 0x10: ss_vscroll = (ss_vscroll & 0xff00) | data; break;
		case 0x11: ss_vscroll = (ss_vscroll & 0xff) | (data << 8); break;
		case 0x12: ss_pal_base = data; break;
		default: break;
	}
}

static WRITE32_HANDLER( ssregs_w )
{
	if (ACCESSING_BITS(0x00ff0000)) ssregs_byte_w(offset * 2, (data >> 16) & 0xff);
	if (ACCESSING_BITS(0x000000ff)) ssregs_byte_w(offset * 2 + 1, data & 0xff);
}

static WRITE32_HANDLER( sh2cache_ram_w )
{
	COMBINE_DATA(&cps3_sh2cache_ram[offset]);
	/* a decrypted copy for the opcode fetch */
	cps3_sh2cache_ram_decrypted[offset] = cps3_sh2cache_ram[offset] ^ cps3_mask(offset * 4 + 0xc0000000, key1, key2);
}

static READ32_HANDLER( cram_data_r )
{
	return char_ram[((cram_bank & 0x7) * 0x100000) / 4 + offset];
}

static WRITE32_HANDLER( cram_data_w )
{
	COMBINE_DATA(&char_ram[((cram_bank & 0x7) * 0x100000) / 4 + offset]);
}

/* colour RAM: 0x20000 colours, two per word */
static READ32_HANDLER( colourram_r )
{
	return (colourram[offset * 2] << 16) | colourram[offset * 2 + 1];
}

static WRITE32_HANDLER( colourram_w )
{
	if (!ACCESSING_BITS(0xffff0000) || !ACCESSING_BITS(0x0000ffff))
	{
		/* partial write: merge with the colours there */
		data32_t old = (colourram[offset * 2] << 16) | colourram[offset * 2 + 1];
		data = (old & mem_mask) | (data & ~mem_mask);
	}
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		set_mame_colours(offset * 2, data >> 16, 0);
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		set_mame_colours(offset * 2 + 1, data & 0xffff, 0);
}

/* GFX flash: two chips of a SIMM at a time */
static READ32_HANDLER( gfxflash_r )
{
	UINT32 result = 0;
	struct cps3_flash *chip0, *chip1;
	UINT32 bank = cram_gfxflash_bank;

	if (bank & 1)
		offset += 0x200000 / 4;
	if (bank / 8 >= 4)
		return 0xffffffff;

	chip0 = &flash_gfx[bank / 8][(bank % 8) & ~1];
	chip1 = &flash_gfx[bank / 8][(bank % 8) | 1];
	if (!chip0->present || !chip1->present)
		return 0xffffffff;

	if (ACCESSING_BITS(0xff000000)) result |= flash_read(chip0, offset << 1) << 24;
	if (ACCESSING_BITS(0x00ff0000)) result |= flash_read(chip1, offset << 1) << 16;
	if (ACCESSING_BITS(0x0000ff00)) result |= flash_read(chip0, (offset << 1) + 1) << 8;
	if (ACCESSING_BITS(0x000000ff)) result |= flash_read(chip1, (offset << 1) + 1) << 0;
	return result;
}

static WRITE32_HANDLER( gfxflash_w )
{
	struct cps3_flash *chip0, *chip1;
	UINT32 bank = cram_gfxflash_bank;

	if (bank & 1)
		offset += 0x200000 / 4;
	if (bank / 8 >= 4)
		return;

	chip0 = &flash_gfx[bank / 8][(bank % 8) & ~1];
	chip1 = &flash_gfx[bank / 8][(bank % 8) | 1];
	if (!chip0->present || !chip1->present)
		return;

	/* the chips write straight into the byte stream used for drawing */
	if (ACCESSING_BITS(0xff000000)) flash_write(chip0, offset << 1, (data >> 24) & 0xff);
	if (ACCESSING_BITS(0x00ff0000)) flash_write(chip1, offset << 1, (data >> 16) & 0xff);
	if (ACCESSING_BITS(0x0000ff00)) flash_write(chip0, (offset << 1) + 1, (data >> 8) & 0xff);
	if (ACCESSING_BITS(0x000000ff)) flash_write(chip1, (offset << 1) + 1, (data >> 0) & 0xff);
}

static void cram_gfxflash_bank_w(data32_t data, data32_t mem_mask)
{
	if (ACCESSING_BITS(0xff000000))
	{
		/* 2, 3: first and second half of SIMM 3 chips 0 + 1, ... the first
		   4 MB are skipped, as with the sound and DMA addresses */
		cram_gfxflash_bank = (data & 0xffff0000) >> 16;
		cram_gfxflash_bank -= 0x0002;
	}
}


/* EEPROM, through an interface: write words at 0x080, latch a word for
   reading at 0x100, read it at 0x200 */
static READ32_HANDLER( eeprom_r )
{
	int addr = offset * 4;

	if (addr >= 0x100 && addr <= 0x17f)
	{
		if (ACCESSING_BITS(0xff000000))
			current_eeprom_read = (eeprom[offset - 0x100 / 4] & 0xffff0000) >> 16;
		else
			current_eeprom_read = (eeprom[offset - 0x100 / 4] & 0x0000ffff) >> 0;
		return 0x00000000;
	}
	else if (addr == 0x200)
	{
		if (ACCESSING_BITS(0xff000000))
			return 0;
		return current_eeprom_read;
	}
	return 0x00000000;
}

static WRITE32_HANDLER( eeprom_w )
{
	int addr = offset * 4;

	if (addr >= 0x080 && addr <= 0x0ff)
	{
		offset -= 0x80 / 4;
		COMBINE_DATA(&eeprom[offset]);
	}
}

static READ32_HANDLER( cps3_inputs_r )
{
	return (readinputport(1) << 16) | readinputport(0);
}

static READ32_HANDLER( cps3_extra_r )
{
	return (readinputport(3) << 16) | readinputport(2);
}

static WRITE32_HANDLER( outport_w )
{
	/* a 16-bit register in the top half */
	if (ACCESSING_BITS(0x00ff0000))
	{
		coin_lockout_w(0, ~data & 0x00010000);
		coin_lockout_w(1, ~data & 0x00020000);
		coin_counter_w(0, data & 0x00100000);
		coin_counter_w(1, data & 0x00200000);
	}
}

static READ32_HANDLER( dev_dipsw_r )
{
	/* warzard crashes before booting if some bit is not set */
	return 0xffffffff;
}

static WRITE32_HANDLER( irq12_ack_w ) { cpu_set_irq_line(0, 12, CLEAR_LINE); }
static WRITE32_HANDLER( irq10_ack_w ) { cpu_set_irq_line(0, 10, CLEAR_LINE); }
static WRITE32_HANDLER( irq14_ack_w ) { cpu_set_irq_line(0, 14, CLEAR_LINE); }
static WRITE32_HANDLER( irq6_ack_w ) { cpu_set_irq_line(0, 6, CLEAR_LINE); }


/***************************************************************************
    DMA
***************************************************************************/

static void dma_interrupt(int param)
{
	dma_status &= ~6;
	cpu_set_irq_line(0, 10, ASSERT_LINE);
}

static void sprite_dma_cb(int param)
{
	dma_status &= ~1;
}

static void spritedma_w(UINT16 data)
{
	UINT16 prev = spritelist_dma;
	spritelist_dma = data;

	/* display list DMA: copy the lists and the global scroll, the rest is
	   done while drawing */
	if (!(dma_status & 1) && (spritelist_dma & 9) == 8 && (prev & 9) == 9)
	{
		int i;
		for (i = 0; i < 0x2000 / 4; i += 4)
		{
			UINT32 dat = cps3_spriteram[i];
			UINT32 offs, length;

			memcpy(&spritelist[i], &cps3_spriteram[i], 4 * sizeof(UINT32));	/* main list record */
			if (dat & 0x80000000)
				break;
			offs = (dat & 0x00007fff) << 2;
			length = (dat & 0x01ff0000) >> 16;
			if (offs + length * 4 > 0x80000 / 4)
				length = (0x80000 / 4 - offs) / 4;
			memcpy(&spritelist[offs], &cps3_spriteram[offs], length * 4 * sizeof(UINT32));	/* sublist */
		}
		memcpy(ppu_gscroll_buff, cps3_ppu_gscroll, 8 * sizeof(UINT32));

		dma_status |= 1;
		/* a short delay skips repeated 8/9 writes; the real speed is not known */
		timer_adjust(spritelist_dma_timer, TIME_IN_USEC(4), 0, 0);
	}
}

/* a big-endian 16-bit word of the SIMM byte stream */
static INLINE UINT16 user5_word(UINT32 n)
{
	UINT32 a = (n * 2) & (USER5_LENGTH - 1);
	return (user5[a] << 8) | user5[a + 1];
}

static void palettedma_w(int offset, data32_t data, data32_t mem_mask)
{
	if (offset == 0)
	{
		COMBINE_DATA(&paldma_source);
		paldma_realsource = (paldma_source << 1) - 0x400000;
	}
	else if (offset == 1)
		COMBINE_DATA(&paldma_dest);
	else if (offset == 2)
		COMBINE_DATA(&paldma_fade);
	else if (offset == 3)
	{
		COMBINE_DATA(&paldma_other2);

		if (ACCESSING_BITS(0xff000000))
			paldma_length = (data & 0xffff0000) >> 16;
		if (ACCESSING_BITS(0x000000ff))
		{
			if (data & 0x0002)
			{
				UINT32 dmalen = paldma_length | ((data & 1) << 16);
				UINT32 i;

				/* MAME reads the source and writes the colours with the 16-bit
				   halves of each word swapped; the swaps match */
				for (i = 0; i < dmalen; i++)
				{
					UINT16 coldata = user5_word(((paldma_realsource >> 1) + i) ^ 1);
					set_mame_colours((paldma_dest + i) ^ 1, coldata, paldma_fade);
				}
				dma_status |= 4;
				timer_adjust(dma_timer, TIME_IN_USEC(100), 0, 0);	/* the real DMA speed is not known */
			}
		}
	}
}

static UINT32 process_byte(UINT8 real_byte, UINT32 destination, int max_length)
{
	destination &= 0x7fffff;

	if (real_byte & 0x40)
	{
		int tranfercount = 0;

		rle_length = (real_byte & 0x3f) + 1;

		while (rle_length)
		{
			char_byte_w(destination + tranfercount, last_normal_byte & 0x3f);

			tranfercount++;
			rle_length--;
			max_length--;

			if ((destination + tranfercount) > 0x7fffff)
				return max_length;
		}
		return tranfercount;
	}
	else
	{
		char_byte_w(destination, real_byte);
		last_normal_byte = real_byte;
		return 1;
	}
}

static void do_char_dma(UINT32 real_source, UINT32 real_destination, UINT32 real_length)
{
	int length_remaining;

	last_normal_byte = 0;
	rle_length = 0;
	length_remaining = real_length;
	while (length_remaining)
	{
		UINT8 current_byte;
		UINT32 length_processed;

		current_byte = user5[real_source & (USER5_LENGTH - 1)];
		real_source++;

		if (current_byte & 0x80)
		{
			UINT8 real_byte;
			current_byte &= 0x7f;

			real_byte = user5[(current_table_address + current_byte * 2 + 0) & (USER5_LENGTH - 1)];
			length_processed = process_byte(real_byte, real_destination, length_remaining);
			length_remaining -= length_processed;
			real_destination += length_processed;
			if (real_destination > 0x7fffff) return;
			if (length_remaining <= 0) return;

			real_byte = user5[(current_table_address + current_byte * 2 + 1) & (USER5_LENGTH - 1)];
			length_processed = process_byte(real_byte, real_destination, length_remaining);
			length_remaining -= length_processed;
			real_destination += length_processed;
			if (real_destination > 0x7fffff) return;
			if (length_remaining <= 0) return;
		}
		else
		{
			length_processed = process_byte(current_byte, real_destination, length_remaining);
			length_remaining -= length_processed;
			real_destination += length_processed;
			if (real_destination > 0x7fffff) return;
			if (length_remaining <= 0) return;
		}
	}
}

static UINT32 process_byte8(UINT8 b, UINT32 dst_offset)
{
	int l = 0;

	if (lastb == lastb2)	/* rle */
	{
		int rle = (b + 1) & 0xff;
		int i;

		for (i = 0; i < rle; ++i)
		{
			char_byte_w(dst_offset, (UINT8)lastb);
			dst_offset++;
			++l;
		}
		lastb2 = 0xffff;
		return l;
	}
	else
	{
		lastb2 = lastb;
		lastb = b;
		char_byte_w(dst_offset, b);
		return 1;
	}
}

static void do_alt_char_dma(UINT32 src, UINT32 real_dest, UINT32 real_length)
{
	UINT32 start = real_dest;
	UINT32 ds = real_dest;

	lastb = 0xfffe;
	lastb2 = 0xffff;

	for (;;)
	{
		UINT8 ctrl = user5[src & (USER5_LENGTH - 1)];
		int i;
		++src;

		for (i = 0; i < 8; ++i)
		{
			UINT8 p = user5[src & (USER5_LENGTH - 1)];

			if (ctrl & 0x80)
			{
				UINT8 real_byte;
				p &= 0x7f;
				real_byte = user5[(current_table_address + p * 2 + 0) & (USER5_LENGTH - 1)];
				ds += process_byte8(real_byte, ds);
				real_byte = user5[(current_table_address + p * 2 + 1) & (USER5_LENGTH - 1)];
				ds += process_byte8(real_byte, ds);
			}
			else
				ds += process_byte8(p, ds);
			++src;
			ctrl <<= 1;

			if ((ds - start) >= real_length)
				return;
		}
	}
}

static void process_character_dma(UINT32 address)
{
	int i;

	for (i = 0; i < 0x1000; i += 3)
	{
		UINT32 dat1 = char_ram[(i + 0 + address) & 0x1fffff];
		UINT32 dat2 = char_ram[(i + 1 + address) & 0x1fffff];
		UINT32 dat3 = char_ram[(i + 2 + address) & 0x1fffff];
		UINT32 dma_command, real_length, real_destination, real_source;

		/* end of list */
		if (dat1 & 0x01000000)
			break;

		dma_command = (dat1 & 0x00e00000) >> 21;
		real_length = (((dat1 & 0x001fffff) + 1) << 3);
		real_destination = dat2 << 3;
		real_source = (dat3 << 1) - 0x400000;

		switch (dma_command)
		{
			case 4:	/* the table used by the decompression */
				current_table_address = real_source;
				break;
			case 2:	/* 6bpp decompression: most sprites and backgrounds */
				do_char_dma(real_source, real_destination, real_length);
				break;
			case 3:	/* 8bpp decompression: SFIII NG Sean's stage only */
				do_alt_char_dma(real_source, real_destination, real_length);
				break;
			case 0:	/* not compressed: warzard, at the start of a stage */
				for (; real_length; --real_length)
				{
					char_byte_w(real_destination, user5[real_source & (USER5_LENGTH - 1)]);
					real_source++;
					real_destination++;
				}
				break;
			default:
				break;
		}
	}
	dma_status |= 2;
	timer_adjust(dma_timer, TIME_IN_USEC(100), 0, 0);	/* the real DMA speed is not known */
}

static void characterdma_w(int offset, data32_t data, data32_t mem_mask)
{
	if (offset == 0)
	{
		if (ACCESSING_BITS(0x000000ff))
			chardma_source = data & 0x0000ffff;
	}
	else if (offset == 1)
	{
		COMBINE_DATA(&chardma_other);

		if (ACCESSING_BITS(0xff000000))
		{
			if ((data >> 16) & 0x0040)
			{
				UINT32 list_address = (chardma_source | ((chardma_other & 0x003f0000)));
				process_character_dma(list_address);
			}
		}
	}
}

/* PPU registers at 0x040c0000 */
static READ32_HANDLER( ppu_r )
{
	if (offset == 0x0c / 4)
		return dma_status << 16;
	return 0;
}

static WRITE32_HANDLER( ppu_w )
{
	if (offset < 0x20 / 4)
		COMBINE_DATA(&cps3_ppu_gscroll[offset]);
	else if (offset < 0x60 / 4)
		COMBINE_DATA(&cps3_tilemap_regs[offset - 0x20 / 4]);
	else if (offset < 0x80 / 4)
		COMBINE_DATA(&cps3_ppu_crtc_zoom[offset - 0x60 / 4]);
	else if (offset == 0x80 / 4)
	{
		if (ACCESSING_BITS(0x0000ffff))
			spritedma_w(data & 0xffff);
	}
	else if (offset == 0x84 / 4)
	{
		if (ACCESSING_BITS(0x000000ff))
			cram_bank = data;
	}
	else if (offset == 0x88 / 4)
		cram_gfxflash_bank_w(data, mem_mask);
	else if (offset >= 0x94 / 4 && offset < 0x9c / 4)
		characterdma_w(offset - 0x94 / 4, data, mem_mask);
	else if (offset >= 0xa0 / 4 && offset < 0xb0 / 4)
		palettedma_w(offset - 0xa0 / 4, data, mem_mask);
}


static MEMORY_READ32_START( cps3_readmem )
	{ 0x00000000, 0x0007ffff, MRA32_ROM },			/* BIOS */
	{ 0x02000000, 0x0207ffff, MRA32_RAM },			/* main RAM */
	{ 0x03000000, 0x030003ff, MRA32_RAM },			/* 'FRAM', memory test only */
	{ 0x04000000, 0x0407ffff, MRA32_RAM },			/* sprite RAM */
	{ 0x04080000, 0x040bffff, colourram_r },
	{ 0x040c0000, 0x040c00ff, ppu_r },
	{ 0x040e0000, 0x040e02ff, cps3_sound_r },
	{ 0x04100000, 0x041fffff, cram_data_r },
	{ 0x04200000, 0x043fffff, gfxflash_r },
	{ 0x05000000, 0x05000003, cps3_inputs_r },
	{ 0x05000004, 0x05000007, cps3_extra_r },
	{ 0x05000a00, 0x05000a1f, dev_dipsw_r },
	{ 0x05001000, 0x05001203, eeprom_r },
	{ 0x05040000, 0x0504ffff, ssram_r },
	{ 0x05140000, 0x05140003, MRA32_NOP },			/* CD-ROM controller */
	{ 0x06000000, 0x067fffff, flash1_r },			/* SIMM 1 */
	{ 0x06800000, 0x06ffffff, flash2_r },			/* SIMM 2 */
	{ 0xc0000000, 0xc00003ff, MRA32_RAM },			/* cache RAM, executes code */
MEMORY_END

static MEMORY_WRITE32_START( cps3_writemem )
	{ 0x00000000, 0x0007ffff, MWA32_ROM },
	{ 0x02000000, 0x0207ffff, MWA32_RAM, &cps3_mainram },
	{ 0x03000000, 0x030003ff, MWA32_RAM },
	{ 0x04000000, 0x0407ffff, MWA32_RAM, &cps3_spriteram },
	{ 0x04080000, 0x040bffff, colourram_w },
	{ 0x040c0000, 0x040c00ff, ppu_w },
	{ 0x040e0000, 0x040e02ff, cps3_sound_w },
	{ 0x04100000, 0x041fffff, cram_data_w },
	{ 0x04200000, 0x043fffff, gfxflash_w },
	{ 0x05000008, 0x0500000b, outport_w },
	{ 0x05001000, 0x05001203, eeprom_w },
	{ 0x05040000, 0x0504ffff, ssram_w },
	{ 0x05050000, 0x0505002b, ssregs_w },
	{ 0x05100000, 0x05100003, irq12_ack_w },
	{ 0x05110000, 0x05110003, irq10_ack_w },
	{ 0x05120000, 0x05120003, irq14_ack_w },
	{ 0x05130000, 0x05130003, irq6_ack_w },
	{ 0x05140000, 0x05140003, MWA32_NOP },
	{ 0x06000000, 0x067fffff, flash1_w },
	{ 0x06800000, 0x06ffffff, flash2_w },
	{ 0x07ff0048, 0x07ff004b, MWA32_NOP },			/* bit 0 toggles while programming */
	{ 0xc0000000, 0xc00003ff, sh2cache_ram_w, &cps3_sh2cache_ram },
MEMORY_END


/***************************************************************************
    Inputs
***************************************************************************/

#define CPS3_COMMON_INPUTS \
	PORT_START	/* 0x05000000, low half */ \
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 ) \
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 ) \
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 ) \
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 ) \
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) \
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) \
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 ) \
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED ) \
\
	PORT_START	/* 0x05000000, high half */ \
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	PORT_BITX(0x0002, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT( 0x00fc, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 ) \
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT( 0xc000, IP_ACTIVE_LOW, IPT_UNUSED ) \
\
	PORT_START	/* 0x05000004, low half */ \
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNUSED ) \
\
	PORT_START	/* 0x05000004, high half */ \
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 ) \
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 ) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 ) \
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

/* not on the board: the cart's region byte, patched at reset. The default
   is Europe, USA for the games with no European release */
#define CPS3_REGION_DIP(def) \
	PORT_START \
	PORT_DIPNAME( 0x0f, def, "Region" ) \
	PORT_DIPSETTING(    0x00, "As BIOS" ) \
	PORT_DIPSETTING(    0x01, "Japan" ) \
	PORT_DIPSETTING(    0x02, "Asia" ) \
	PORT_DIPSETTING(    0x03, "Europe" ) \
	PORT_DIPSETTING(    0x04, "USA" ) \
	PORT_DIPSETTING(    0x05, "Hispanic" ) \
	PORT_DIPSETTING(    0x06, "Brazil" ) \
	PORT_DIPSETTING(    0x07, "Oceania" ) \
	PORT_DIPSETTING(    0x08, "Asia (BIOS set 2)" ) \
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

INPUT_PORTS_START( cps3 )
	CPS3_COMMON_INPUTS
	CPS3_REGION_DIP( 0x03 )
INPUT_PORTS_END

/* 2nd Impact: USA, Japan, Asia and Hispanic only */
INPUT_PORTS_START( sfiii2 )
	CPS3_COMMON_INPUTS
	CPS3_REGION_DIP( 0x04 )
INPUT_PORTS_END


/***************************************************************************
    Machine
***************************************************************************/

static INTERRUPT_GEN( cps3_vbl_interrupt )
{
	cpu_set_irq_line(0, 12, ASSERT_LINE);
}

/* Red Earth / Warzard: the CD BIOSes have no no-CD flag and stop on the
   missing drive. Start the game from the SIMMs as the BIOS would, with the
   vector base on its table */
static void skip_bios_boot(int param)
{
	/* the reset vector: PC, then SP */
	cpunum_set_reg(0, SH2_PC, decrypted_gamerom[0]);
	cpunum_set_reg(0, SH2_R15, decrypted_gamerom[1]);
	cpunum_set_reg(0, SH2_VBR, 0x06000000);
}

static MACHINE_INIT( cps3 )
{
	int s, c;

	current_table_address = -1;
	dma_status = 0;
	spritelist_dma = 0;
	cram_bank = 0;
	cram_gfxflash_bank = 0;

	for (s = 0; s < 2; s++)
		for (c = 0; c < 4; c++)
			flash_prog[s][c].mode = FM_NORMAL;
	for (s = 0; s < 4; s++)
		for (c = 0; c < 8; c++)
			flash_gfx[s][c].mode = FM_NORMAL;

	/* the region, as set in the fake DIP switch */
	{
		int dip = readinputport(4) & 0x0f;
		UINT8 region = dip ? (region_bios & 0xf0) | dip : region_bios;

		bios_words[region_word] = (bios_words[region_word] & ~0xff) | region;
	}

	if (skip_bios)
		timer_set(TIME_NOW, 0, skip_bios_boot);	/* after the CPU reset */
}

static void flash_presave(void)
{
	int s, c;
	for (s = 0; s < 2; s++)
		for (c = 0; c < 4; c++)
			flash_modes[s * 4 + c] = flash_prog[s][c].mode;
	for (s = 0; s < 4; s++)
		for (c = 0; c < 8; c++)
			flash_modes[8 + s * 8 + c] = flash_gfx[s][c].mode;
}

static void flash_postload(void)
{
	int s, c;
	for (s = 0; s < 2; s++)
		for (c = 0; c < 4; c++)
			flash_prog[s][c].mode = flash_modes[s * 4 + c];
	for (s = 0; s < 4; s++)
		for (c = 0; c < 8; c++)
			flash_gfx[s][c].mode = flash_modes[8 + s * 8 + c];
	for (c = 0; c < 0x400 / 4; c++)
		cps3_sh2cache_ram_decrypted[c] = cps3_sh2cache_ram[c] ^ cps3_mask(c * 4 + 0xc0000000, key1, key2);
}

static NVRAM_HANDLER( cps3 )
{
	UINT8 buf[0x80];
	int i;

	if (read_or_write)
	{
		for (i = 0; i < 0x80 / 4; i++)
		{
			buf[i * 4 + 0] = eeprom[i] >> 24;
			buf[i * 4 + 1] = eeprom[i] >> 16;
			buf[i * 4 + 2] = eeprom[i] >> 8;
			buf[i * 4 + 3] = eeprom[i] >> 0;
		}
		mame_fwrite(file, buf, 0x80);
	}
	else
	{
		memset(buf, 0, sizeof(buf));
		if (file)
			mame_fread(file, buf, 0x80);
		for (i = 0; i < 0x80 / 4; i++)
			eeprom[i] = (buf[i * 4 + 0] << 24) | (buf[i * 4 + 1] << 16) | (buf[i * 4 + 2] << 8) | buf[i * 4 + 3];
	}
}

static MACHINE_DRIVER_START( cps3 )
	/* external clock 6.25 MHz, multiplied by 4 inside */
	MDRV_CPU_ADD(SH2, 6250000 * 4)
	MDRV_CPU_CONFIG(cps3_sh2_config)
	MDRV_CPU_MEMORY(cps3_readmem, cps3_writemem)
	MDRV_CPU_VBLANK_INT(cps3_vbl_interrupt, 1)

	/* 42.954545 MHz / 5 pixel clock, 546 x 264 total */
	MDRV_FRAMES_PER_SECOND(42954545.0 / 5 / 546 / 264)
	MDRV_VBLANK_DURATION(2543)

	MDRV_MACHINE_INIT(cps3)
	MDRV_NVRAM_HANDLER(cps3)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(496, 224)
	MDRV_VISIBLE_AREA(0, 384-1, 0, 224-1)
	MDRV_PALETTE_LENGTH(16)

	MDRV_VIDEO_START(cps3)
	MDRV_VIDEO_UPDATE(cps3)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(CUSTOM, cps3_custom_interface)
MACHINE_DRIVER_END


/***************************************************************************
    Init
***************************************************************************/

static void cps3_region_byte(UINT32 address)
{
	region_word = address / 4;
	region_bios = bios_words[region_word] & 0xff;
}

static void cps3_init(UINT32 k1, UINT32 k2, int alt, int simm2, int simm5_chips, int simm6,
		UINT32 idle_ram, UINT32 idle_pc)
{
	UINT32 *bios = (UINT32 *)memory_region(REGION_CPU1);
	UINT32 i;

	bios_words = bios;

	key1 = k1;
	key2 = k2;
	alt_encryption = alt;
	skip_bios = 0;
	speedup_ram_address = idle_ram;
	speedup_code_address = idle_pc;

	/* the BIOS region holds native 32-bit words: decrypt it in place */
	for (i = 0; i < 0x80000 / 4; i++)
		bios[i] ^= cps3_mask(i * 4, key1, key2);

	/* no CD drive here: set the BIOS flag the no-CD BIOSes have, so the CD
	   BIOSes run the game from the SIMMs instead of asking for the drive */
	bios[0x1fecc / 4] |= 0x01000000;
	cps3_region_byte(0x1fec8);

	flash_setup(simm2, simm5_chips, simm6);

	decrypted_gamerom = auto_malloc(PROGRAM_LENGTH);
	for (i = 0; i < PROGRAM_LENGTH / 4; i++)
		decrypt_program_word(i);

	cps3_sound_set_base(user5, USER5_LENGTH);

	if (speedup_ram_address)
		install_mem_read32_handler(0, speedup_ram_address & ~3, (speedup_ram_address & ~3) + 3, cps3_speedup_r);

	dma_timer = timer_alloc(dma_interrupt);
	spritelist_dma_timer = timer_alloc(sprite_dma_cb);

	memset(cps3_sh2cache_ram_decrypted, 0, sizeof(cps3_sh2cache_ram_decrypted));

	state_save_register_UINT32("cps3", 0, "cram_gfxflash_bank", &cram_gfxflash_bank, 1);
	state_save_register_UINT32("cps3", 0, "cram_bank", &cram_bank, 1);
	state_save_register_UINT16("cps3", 0, "current_eeprom_read", &current_eeprom_read, 1);
	state_save_register_UINT32("cps3", 0, "paldma_source", &paldma_source, 1);
	state_save_register_UINT32("cps3", 0, "paldma_realsource", &paldma_realsource, 1);
	state_save_register_UINT32("cps3", 0, "paldma_dest", &paldma_dest, 1);
	state_save_register_UINT32("cps3", 0, "paldma_fade", &paldma_fade, 1);
	state_save_register_UINT32("cps3", 0, "paldma_other2", &paldma_other2, 1);
	state_save_register_UINT32("cps3", 0, "paldma_length", &paldma_length, 1);
	state_save_register_UINT32("cps3", 0, "chardma_source", &chardma_source, 1);
	state_save_register_UINT32("cps3", 0, "chardma_other", &chardma_other, 1);
	state_save_register_INT32("cps3", 0, "current_table_address", &current_table_address, 1);
	state_save_register_UINT16("cps3", 0, "dma_status", &dma_status, 1);
	state_save_register_UINT32("cps3", 0, "ppu_gscroll_buff", ppu_gscroll_buff, 8);
	state_save_register_UINT16("cps3", 0, "ss_hscroll", &ss_hscroll, 1);
	state_save_register_UINT16("cps3", 0, "ss_vscroll", &ss_vscroll, 1);
	state_save_register_UINT8("cps3", 0, "ss_pal_base", &ss_pal_base, 1);
	state_save_register_UINT16("cps3", 0, "spritelist_dma", &spritelist_dma, 1);
	state_save_register_UINT32("cps3", 0, "eeprom", eeprom, 0x80 / 4);
	state_save_register_UINT32("cps3", 0, "ppu_gscroll", cps3_ppu_gscroll, 8);
	state_save_register_UINT32("cps3", 0, "tilemap_regs", cps3_tilemap_regs, 16);
	state_save_register_UINT32("cps3", 0, "ppu_crtc_zoom", cps3_ppu_crtc_zoom, 8);
	state_save_register_INT32("cps3", 0, "flash_modes", flash_modes, 2 * 4 + 4 * 8);
	state_save_register_func_presave(flash_presave);
	state_save_register_func_postload(flash_postload);
}

/*                                 key 1       key 2     alt SIMM2 SIMM5 SIMM6  idle loop RAM, PC */
static DRIVER_INIT( redearth ) { cps3_init(0x9e300ab1, 0xa175b82c, 0, 0, 2, 0, 0x0202136c, 0x0600194e); cps3_region_byte(0x1fed8); }	/* region in another word */
static DRIVER_INIT( sfiii )    { cps3_init(0xb5fe053e, 0xfc03925a, 0, 0, 2, 0, 0x0200cc6c, 0x06000884); }
static DRIVER_INIT( sfiii2 )   { cps3_init(0x00000000, 0x00000000, 1, 1, 8, 0, 0x0200dfe4, 0x06000884); }	/* runs off a 'dead' cart */
static DRIVER_INIT( jojo )     { cps3_init(0x02203ee3, 0x01301972, 0, 1, 2, 0, 0x020223d8, 0x0600065c); }
static DRIVER_INIT( jojor2 )   { cps3_init(0x02203ee3, 0x01301972, 0, 1, 2, 0, 0x020223c0, 0x0600065c); }
static DRIVER_INIT( sfiii3 )   { cps3_init(0xa55432b4, 0x0c129981, 0, 1, 8, 1, 0x0200d794, 0x06000884); }
static DRIVER_INIT( jojoba )   { cps3_init(0x23323ee3, 0x03021972, 0, 1, 8, 0, 0x020267dc, 0x0600065c); }
static DRIVER_INIT( redearthcd ) { init_redearth(); skip_bios = 1; }


/***************************************************************************
    ROMs
***************************************************************************/

/* SIMM data of the 961121 version */
#define CPS3_REDEARTH_961121_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "redearth-simm1.0", 0x0000000, 0x200000, CRC(cad468f8) SHA1(b3aa4f7d3fae84e8821417ccde9528d3eda2b7a6) ) \
	ROM_LOAD32_BYTE( "redearth-simm1.1", 0x0000001, 0x200000, CRC(e9721d89) SHA1(5c63d10bdbce52d50b6dde14d4a0f1369383d656) ) \
	ROM_LOAD32_BYTE( "redearth-simm1.2", 0x0000002, 0x200000, CRC(2889ec98) SHA1(a94310eb4777f908d87e9d90969db8504b4140ff) ) \
	ROM_LOAD32_BYTE( "redearth-simm1.3", 0x0000003, 0x200000, CRC(5a6cd148) SHA1(d65c6e8378a91828474a16a3bbcd13c4b3b15f13) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "redearth-simm3.0", 0x0000000, 0x200000, CRC(83350cc5) SHA1(922b1abf80a4a89f35279b66311a7369d3965bd0) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.1", 0x0000001, 0x200000, CRC(56734de6) SHA1(75699fa6efe5bec335e4b02e15b3c45726b68fa8) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.2", 0x0400000, 0x200000, CRC(800ea0f1) SHA1(33871ab56dc1cd24441389d53e43fb8e43b149d9) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.3", 0x0400001, 0x200000, CRC(97e9146c) SHA1(ab7744709615081440bee72f4080d6fd5b938668) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.4", 0x0800000, 0x200000, CRC(0cb1d648) SHA1(7042a590c2b7ec55323062127e254da3cdc790a1) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.5", 0x0800001, 0x200000, CRC(7a1099f0) SHA1(c6a92ec86eb24485f1db530e0e78f647e8432231) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.6", 0x0c00000, 0x200000, CRC(aeff8f54) SHA1(fd760e237c2e5fb2da45e32a1c12fd3defb4c3e4) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.7", 0x0c00001, 0x200000, CRC(f770acd0) SHA1(4b3ccb6f91568f95f04ede6c574144918d131201) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.0", 0x1000000, 0x200000, CRC(301e56f2) SHA1(4847d971bff70a2aeed4599e1201c7ec9677da60) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.1", 0x1000001, 0x200000, CRC(2048e103) SHA1(b21f95b05cd99749bd3f25cc71b2671c2026847b) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.2", 0x1400000, 0x200000, CRC(c9433455) SHA1(63a269d76bac332c2e991d0f6a20c35e0e88680a) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.3", 0x1400001, 0x200000, CRC(c02171a8) SHA1(2e9228729b27a6113d9f2e42af310a834979f714) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.4", 0x1800000, 0x200000, CRC(2ddbf276) SHA1(b232baaa8edc8db18f8a3bdcc2d38fe984a94a34) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.5", 0x1800001, 0x200000, CRC(fea820a6) SHA1(55ee8ef95751f5a509fb126513e1b2a70a3414e5) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.6", 0x1c00000, 0x200000, CRC(c7528df1) SHA1(aa312f80c2d7759d18d1aa8d416cf932b2850824) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.7", 0x1c00001, 0x200000, CRC(2449cf3b) SHA1(c60d8042136d74e547f668ad787cae529c42eed9) ) \
	ROM_LOAD16_BYTE( "redearth-simm5.0", 0x2000000, 0x200000, CRC(424451b9) SHA1(250fb92254c9e7ff5bc8dbeea5872f8a771dc9bd) ) \
	ROM_LOAD16_BYTE( "redearth-simm5.1", 0x2000001, 0x200000, CRC(9b8cb56b) SHA1(2ff1081dc99bb7c2f1e036f4c112137c96b83d23) )

/* SIMM data of the 961023 version */
#define CPS3_REDEARTH_961023_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "redearthr1-simm1.0", 0x0000000, 0x200000, CRC(65bac346) SHA1(6f4ba0c2cae91a37fc97bea5fc8a50aaf6ca6513) ) \
	ROM_LOAD32_BYTE( "redearthr1-simm1.1", 0x0000001, 0x200000, CRC(a8ec4aae) SHA1(0012cb6ba630ddd74958f7759de34706bf919338) ) \
	ROM_LOAD32_BYTE( "redearthr1-simm1.2", 0x0000002, 0x200000, CRC(2caf8995) SHA1(ca012b6dec0481b043edf9c7e931bd952ec74ebb) ) \
	ROM_LOAD32_BYTE( "redearthr1-simm1.3", 0x0000003, 0x200000, CRC(13ebc21d) SHA1(465bdea0633526a8bf07b35495a5311c8bf213d5) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "redearth-simm3.0", 0x0000000, 0x200000, CRC(83350cc5) SHA1(922b1abf80a4a89f35279b66311a7369d3965bd0) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.1", 0x0000001, 0x200000, CRC(56734de6) SHA1(75699fa6efe5bec335e4b02e15b3c45726b68fa8) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.2", 0x0400000, 0x200000, CRC(800ea0f1) SHA1(33871ab56dc1cd24441389d53e43fb8e43b149d9) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.3", 0x0400001, 0x200000, CRC(97e9146c) SHA1(ab7744709615081440bee72f4080d6fd5b938668) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.4", 0x0800000, 0x200000, CRC(0cb1d648) SHA1(7042a590c2b7ec55323062127e254da3cdc790a1) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.5", 0x0800001, 0x200000, CRC(7a1099f0) SHA1(c6a92ec86eb24485f1db530e0e78f647e8432231) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.6", 0x0c00000, 0x200000, CRC(aeff8f54) SHA1(fd760e237c2e5fb2da45e32a1c12fd3defb4c3e4) ) \
	ROM_LOAD16_BYTE( "redearth-simm3.7", 0x0c00001, 0x200000, CRC(f770acd0) SHA1(4b3ccb6f91568f95f04ede6c574144918d131201) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.0", 0x1000000, 0x200000, CRC(301e56f2) SHA1(4847d971bff70a2aeed4599e1201c7ec9677da60) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.1", 0x1000001, 0x200000, CRC(2048e103) SHA1(b21f95b05cd99749bd3f25cc71b2671c2026847b) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.2", 0x1400000, 0x200000, CRC(c9433455) SHA1(63a269d76bac332c2e991d0f6a20c35e0e88680a) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.3", 0x1400001, 0x200000, CRC(c02171a8) SHA1(2e9228729b27a6113d9f2e42af310a834979f714) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.4", 0x1800000, 0x200000, CRC(2ddbf276) SHA1(b232baaa8edc8db18f8a3bdcc2d38fe984a94a34) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.5", 0x1800001, 0x200000, CRC(fea820a6) SHA1(55ee8ef95751f5a509fb126513e1b2a70a3414e5) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.6", 0x1c00000, 0x200000, CRC(c7528df1) SHA1(aa312f80c2d7759d18d1aa8d416cf932b2850824) ) \
	ROM_LOAD16_BYTE( "redearth-simm4.7", 0x1c00001, 0x200000, CRC(2449cf3b) SHA1(c60d8042136d74e547f668ad787cae529c42eed9) ) \
	ROM_LOAD16_BYTE( "redearth-simm5.0", 0x2000000, 0x200000, CRC(424451b9) SHA1(250fb92254c9e7ff5bc8dbeea5872f8a771dc9bd) ) \
	ROM_LOAD16_BYTE( "redearth-simm5.1", 0x2000001, 0x200000, CRC(9b8cb56b) SHA1(2ff1081dc99bb7c2f1e036f4c112137c96b83d23) )

/* SIMM data of the 970204 version */
#define CPS3_SFIII_970204_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "sfiii-simm1.0", 0x0000000, 0x200000, CRC(cfc9e45a) SHA1(5d9061f76680642e730373e3ac29b24926dc5c0c) ) \
	ROM_LOAD32_BYTE( "sfiii-simm1.1", 0x0000001, 0x200000, CRC(57920546) SHA1(c8452e7e101b8888fb806d1c9874c6be49fc7dbd) ) \
	ROM_LOAD32_BYTE( "sfiii-simm1.2", 0x0000002, 0x200000, CRC(0d8f2680) SHA1(ade7b28acd11023696c4b20136f3d2f34da6b1be) ) \
	ROM_LOAD32_BYTE( "sfiii-simm1.3", 0x0000003, 0x200000, CRC(ea4ca054) SHA1(f91c55c4e4fc428ce15d27be38aeed3a483d028c) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "sfiii-simm3.0", 0x0000000, 0x200000, CRC(080b3bd3) SHA1(f51bc5de95ab22b87ba09ea721285b308afd0bda) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.1", 0x0000001, 0x200000, CRC(5c356f2f) SHA1(e969ce388f6e565d9612e65b0895560c7bb472e6) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.2", 0x0400000, 0x200000, CRC(f9c97a45) SHA1(58a9691696c3f26a1150a451567c501f55cf1874) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.3", 0x0400001, 0x200000, CRC(09de3ead) SHA1(2f41d84a96cb5e0d169200a4e9358ad5f407a2b7) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.4", 0x0800000, 0x200000, CRC(7dd7e1f3) SHA1(bcf1023287457d97f09d9f6e9c93fdf24cc24a07) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.5", 0x0800001, 0x200000, CRC(47a03a3a) SHA1(2509e5737059251888e4e1efbcdfac86a89ff1a1) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.6", 0x0c00000, 0x200000, CRC(e9eb7a26) SHA1(b8547edb7085e9149aa59d5226ad2d1976cab2bd) ) \
	ROM_LOAD16_BYTE( "sfiii-simm3.7", 0x0c00001, 0x200000, CRC(7f44395c) SHA1(f4d2e283cb3a4aad4eae4e13963a74e20be7c181) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.0", 0x1000000, 0x200000, CRC(9ac080fc) SHA1(2e5024b35b147513ee42eda8748df9d669410377) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.1", 0x1000001, 0x200000, CRC(6e2c4c94) SHA1(5a185cb76b5999bd826bc9b5ea584a5c3498f69d) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.2", 0x1400000, 0x200000, CRC(8afc22d4) SHA1(04a419a3092c98fc4a7693e6acf30ae5a849e5c1) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.3", 0x1400001, 0x200000, CRC(9f3873b8) SHA1(33499d6f02bc84c80acb56be078aaed7f8d1300d) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.4", 0x1800000, 0x200000, CRC(166b3c97) SHA1(40e6e9d43cbbd8496b430931b8ab7db01dc1c6d5) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.5", 0x1800001, 0x200000, CRC(e5ea2547) SHA1(a823c689098f37a3054d728bddb0033a4b8396f1) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.6", 0x1c00000, 0x200000, CRC(e85b9fdd) SHA1(264cb10fe9b3ede384c7db42bfc58ed5c21ea8f8) ) \
	ROM_LOAD16_BYTE( "sfiii-simm4.7", 0x1c00001, 0x200000, CRC(362c01b7) SHA1(9c404312a6aabe8e91e68dde193e3972bc1636cd) ) \
	ROM_LOAD16_BYTE( "sfiii-simm5.0", 0x2000000, 0x200000, CRC(9bc108b2) SHA1(894dadab7957044bf877029c7f8e556d5d6e85d3) ) \
	ROM_LOAD16_BYTE( "sfiii-simm5.1", 0x2000001, 0x200000, CRC(c6f1c066) SHA1(00de492dd1ef7aef05027a8c501c296b6602e917) )

/* SIMM data of the 970930 version */
#define CPS3_SFIII2_970930_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "sfiii2-simm1.0", 0x0000000, 0x200000, CRC(2d666f0b) SHA1(68de034b3a3aeaf4b26122a84ad48b0b763e4122) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm1.1", 0x0000001, 0x200000, CRC(2a3a8ef6) SHA1(31fb58fd1360ed8c951e2c4ac898a5a7104528d6) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm1.2", 0x0000002, 0x200000, CRC(161d2206) SHA1(58999f876e64c1a088e8765962a9cd504f22a706) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm1.3", 0x0000003, 0x200000, CRC(87ded8a3) SHA1(4ccef64f80d2ee63940b0958b500364ee515db51) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm2.0", 0x0800000, 0x200000, CRC(94a4ce0f) SHA1(2c8e26a66d1dcd17c22c70baa2a3ff5a54511514) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm2.1", 0x0800001, 0x200000, CRC(67585033) SHA1(24df9968a54c330fbe95f8e4dfe6e7dfd144ed0c) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm2.2", 0x0800002, 0x200000, CRC(fabffcd5) SHA1(9399f64c42f63a64e44a21a2690e44779943a2b2) ) \
	ROM_LOAD32_BYTE( "sfiii2-simm2.3", 0x0800003, 0x200000, CRC(623c09ca) SHA1(dc9618a08bb7f44e569ac17605d268511155a14e) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "sfiii2-simm3.0", 0x0000000, 0x200000, CRC(dab2d766) SHA1(d265cc8b1b497eb4bedd63b3f1de60eb1c1db0df) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.1", 0x0000001, 0x200000, CRC(1f2aa34b) SHA1(38b224d34c4550f1f33c2c368e2a252d0d176cc0) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.2", 0x0400000, 0x200000, CRC(6f1a04eb) SHA1(980ca929114075d1920e2da44f9a22087cc92e55) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.3", 0x0400001, 0x200000, CRC(e05ef205) SHA1(e604e3832549740f953581fc91e850beda6a73c8) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.4", 0x0800000, 0x200000, CRC(affb074f) SHA1(0e76973807039bc66fd0f3233401cea8d2c45f84) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.5", 0x0800001, 0x200000, CRC(6962872e) SHA1(f16b2d0792697345145d0e9d950e912a2ffabe0d) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.6", 0x0c00000, 0x200000, CRC(6eed87de) SHA1(5d5067ad36234c5efd57a2baebeffa2f44f2caec) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm3.7", 0x0c00001, 0x200000, CRC(e18f479e) SHA1(cd4c1812ab422336bfa414e0b2098b472d2f9251) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.0", 0x1000000, 0x200000, CRC(764c2503) SHA1(cad3f20ade2e1d3ac52f8c318443da20062ae943) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.1", 0x1000001, 0x200000, CRC(3e16af6e) SHA1(afde2ed4bf3a3e95035fc02c572c5b83178a9467) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.2", 0x1400000, 0x200000, CRC(215705e6) SHA1(42d3849f8a9242a89ba465dbc205f310186c67cd) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.3", 0x1400001, 0x200000, CRC(e30cbd9c) SHA1(c205101ada86154921e09fed4f6908d15ec60761) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.4", 0x1800000, 0x200000, CRC(4185ded9) SHA1(24bf9b5f25d7753f1feb09b82611f7482f30d304) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.5", 0x1800001, 0x200000, CRC(4e8db013) SHA1(6816df2b6c60005fb375530ea93bb30a960c9b01) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.6", 0x1c00000, 0x200000, CRC(08df48ce) SHA1(e8a3b68ebeab193539446c3f6e0a19b37f1f3495) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm4.7", 0x1c00001, 0x200000, CRC(bb8f80a5) SHA1(35d9e86637d54405c97fdb7da9c42cc53907cae3) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.0", 0x2000000, 0x200000, CRC(ebdc4787) SHA1(f86e8ebf4b2214be166dbe4ea921058a552364ea) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.1", 0x2000001, 0x200000, CRC(6b7c550e) SHA1(77cdabccf3ecebf142ac86dffe6e24052941e3a1) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.2", 0x2400000, 0x200000, CRC(56ff8c50) SHA1(16f7602a4549a5b724e3fcdb75b0f3c397077b81) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.3", 0x2400001, 0x200000, CRC(3f2ac3e9) SHA1(a7b631f18ce572a42f46314f37a01d9840abc765) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.4", 0x2800000, 0x200000, CRC(48cda50e) SHA1(35e9f27fb8b69e3b3a313ea33dc53b1102e5f66e) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.5", 0x2800001, 0x200000, CRC(520c0af6) SHA1(7bed1b6707974eafbfb62ccb84a51df8a100e070) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.6", 0x2c00000, 0x200000, CRC(2edc5986) SHA1(761ab2c67d0d873ffd74158eb77f7722c076f3e3) ) \
	ROM_LOAD16_BYTE( "sfiii2-simm5.7", 0x2c00001, 0x200000, CRC(93ffa199) SHA1(33ec2379f30c6fdf47ba72c1d0cad8bdd02f17df) )

/* SIMM data of the 990128 version */
#define CPS3_JOJO_990128_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "jojo-simm1.0", 0x0000000, 0x200000, CRC(9516948b) SHA1(4d7e6c1eb7d1bebff2a5069bcd186070a9105474) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.1", 0x0000001, 0x200000, CRC(a847848d) SHA1(4df70309395f1d2a2e8f85bc34e17453d4a76f81) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.2", 0x0000002, 0x200000, CRC(853e8846) SHA1(d120b7e2de9502e3261e8dd101f97589b2ed1c38) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.3", 0x0000003, 0x200000, CRC(c04fe00e) SHA1(d09409b77460d19b56aaaf4a64356f3d37a1ee41) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.0", 0x0800000, 0x200000, CRC(e1a4b3c8) SHA1(5dc298431644e1ca470aaab752a7d74f2f9dc7a1) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.1", 0x0800001, 0x200000, CRC(189cef95) SHA1(ebe42a019358461557f69fb17d65d84d0f733415) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.2", 0x0800002, 0x200000, CRC(47db5ec6) SHA1(e80271e4013e4391c2cc4229ff1fbd4a2b7c6f04) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.3", 0x0800003, 0x200000, CRC(e3d3a155) SHA1(75e9b5da93dd8894cf70fa4dac56f3958be4c766) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "jojo-simm3.0", 0x0000000, 0x200000, CRC(de7fc9c1) SHA1(662b85a990b04c855773506c936317e62fab4a05) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.1", 0x0000001, 0x200000, CRC(43d053d3) SHA1(54ff0e9c164e0d1649522c330ccc7e5d79e0bc85) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.2", 0x0400000, 0x200000, CRC(2ffd7fa5) SHA1(9018c8e2b286a333ba606208e90caa764951ea3f) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.3", 0x0400001, 0x200000, CRC(4da4985b) SHA1(2552b1730a21ce17d58b69a79ad212a6a5829439) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.4", 0x0800000, 0x200000, CRC(fde98d72) SHA1(654563e12d033e8656dc74a268a08b15b171470d) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.5", 0x0800001, 0x200000, CRC(edb2a266) SHA1(19ebada8422c7f4bf70d0c9ad42b84268967b316) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.6", 0x0c00000, 0x200000, CRC(be7cf319) SHA1(7893f5907992e6b903b2683980bba6d3d003bb06) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.7", 0x0c00001, 0x200000, CRC(56fe1a9f) SHA1(01741fe1256f4e682f687e94040f4e8bbb8bedb2) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.0", 0x1000000, 0x200000, CRC(c4e7bf68) SHA1(a4d1ddea58a3d42db82a63a5e974cbf38d9b792a) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.1", 0x1000001, 0x200000, CRC(b62b2719) SHA1(cb577b89e9e14fda67715716fefd47a782d518ab) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.2", 0x1400000, 0x200000, CRC(18d15809) SHA1(2b406cd1aaa4799a436213dcaa65473eacb4c6d7) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.3", 0x1400001, 0x200000, CRC(9af0ad79) SHA1(075ee048e17b50188876f25d7a6571d6ace84d7d) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.4", 0x1800000, 0x200000, CRC(4124c1f0) SHA1(e4946a8029adc5d0bacead8d766521b4ccd1722b) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.5", 0x1800001, 0x200000, CRC(5e001fd1) SHA1(6457a39f336381b46e587aa2f5f719810ee5bcf9) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.6", 0x1c00000, 0x200000, CRC(9affa23b) SHA1(e3d77e777c47277d841a9dadc1dd6e3157706a2e) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.7", 0x1c00001, 0x200000, CRC(2511572a) SHA1(725adcf71bcee5c8bb839d2d1c5e3456b8c6886b) ) \
	ROM_LOAD16_BYTE( "jojo-simm5.0", 0x2000000, 0x200000, CRC(797615fc) SHA1(29874be9f1da5515c90f5d601aa5924c263f8feb) ) \
	ROM_LOAD16_BYTE( "jojo-simm5.1", 0x2000001, 0x200000, CRC(734fd162) SHA1(16cdfac74d18a6c2216afb1ce6afbd7f15297c32) )

/* SIMM data of the 990108 version */
#define CPS3_JOJO_990108_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "jojo-simm1.0", 0x0000000, 0x200000, CRC(cfbc38d6) SHA1(c33e3a51fe8ab54e0912a1d6e662fe1ade73cee7) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.1", 0x0000001, 0x200000, CRC(42578d94) SHA1(fa46f92ac1a6716430adec9ab27214a11fa61749) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.2", 0x0000002, 0x200000, CRC(1b40c566) SHA1(9833799e9b4fecf7f9ce14bca64936646b3fdbde) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.3", 0x0000003, 0x200000, CRC(bba709b4) SHA1(0dd71e575f2193505f2ab960568ac1eccf40d53f) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.0", 0x0800000, 0x200000, CRC(417e5dc1) SHA1(54ee9596c1c51811f3bdef7dbe77b44b34f230ca) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.1", 0x0800001, 0x200000, CRC(d3b3267d) SHA1(eb2cff347880f1489fb5b1b8bd16df8f50c7f494) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.2", 0x0800002, 0x200000, CRC(c66d96b1) SHA1(909d5aac165748b549b6056a6091c41df012f5df) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.3", 0x0800003, 0x200000, CRC(aa34cc85) SHA1(7677cc6fa913755fc699691b350698bbe8904118) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "jojo-simm3.0", 0x0000000, 0x200000, CRC(de7fc9c1) SHA1(662b85a990b04c855773506c936317e62fab4a05) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.1", 0x0000001, 0x200000, CRC(43d053d3) SHA1(54ff0e9c164e0d1649522c330ccc7e5d79e0bc85) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.2", 0x0400000, 0x200000, CRC(2ffd7fa5) SHA1(9018c8e2b286a333ba606208e90caa764951ea3f) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.3", 0x0400001, 0x200000, CRC(4da4985b) SHA1(2552b1730a21ce17d58b69a79ad212a6a5829439) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.4", 0x0800000, 0x200000, CRC(fde98d72) SHA1(654563e12d033e8656dc74a268a08b15b171470d) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.5", 0x0800001, 0x200000, CRC(edb2a266) SHA1(19ebada8422c7f4bf70d0c9ad42b84268967b316) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.6", 0x0c00000, 0x200000, CRC(be7cf319) SHA1(7893f5907992e6b903b2683980bba6d3d003bb06) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.7", 0x0c00001, 0x200000, CRC(56fe1a9f) SHA1(01741fe1256f4e682f687e94040f4e8bbb8bedb2) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.0", 0x1000000, 0x200000, CRC(c4e7bf68) SHA1(a4d1ddea58a3d42db82a63a5e974cbf38d9b792a) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.1", 0x1000001, 0x200000, CRC(b62b2719) SHA1(cb577b89e9e14fda67715716fefd47a782d518ab) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.2", 0x1400000, 0x200000, CRC(18d15809) SHA1(2b406cd1aaa4799a436213dcaa65473eacb4c6d7) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.3", 0x1400001, 0x200000, CRC(9af0ad79) SHA1(075ee048e17b50188876f25d7a6571d6ace84d7d) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.4", 0x1800000, 0x200000, CRC(4124c1f0) SHA1(e4946a8029adc5d0bacead8d766521b4ccd1722b) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.5", 0x1800001, 0x200000, CRC(5e001fd1) SHA1(6457a39f336381b46e587aa2f5f719810ee5bcf9) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.6", 0x1c00000, 0x200000, CRC(9affa23b) SHA1(e3d77e777c47277d841a9dadc1dd6e3157706a2e) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.7", 0x1c00001, 0x200000, CRC(2511572a) SHA1(725adcf71bcee5c8bb839d2d1c5e3456b8c6886b) ) \
	ROM_LOAD16_BYTE( "jojo-simm5.0", 0x2000000, 0x200000, CRC(797615fc) SHA1(29874be9f1da5515c90f5d601aa5924c263f8feb) ) \
	ROM_LOAD16_BYTE( "jojo-simm5.1", 0x2000001, 0x200000, CRC(734fd162) SHA1(16cdfac74d18a6c2216afb1ce6afbd7f15297c32) )

/* SIMM data of the 981202 version */
#define CPS3_JOJO_981202_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "jojo-simm1.0", 0x0000000, 0x200000, CRC(e06ba886) SHA1(4defd5e8e1e6d0c439fed8a6454e89a59e24ea4c) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.1", 0x0000001, 0x200000, CRC(6dd177c8) SHA1(c39db980f6fcca9c221e9be6f777eaf38f1b136b) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.2", 0x0000002, 0x200000, CRC(d35a15e0) SHA1(576b92a94505764a10b9bcf82c02335e7ef62014) ) \
	ROM_LOAD32_BYTE( "jojo-simm1.3", 0x0000003, 0x200000, CRC(66d865ac) SHA1(5248c3f124af62b4a672d954ef15f86629feeacb) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.0", 0x0800000, 0x200000, CRC(417e5dc1) SHA1(54ee9596c1c51811f3bdef7dbe77b44b34f230ca) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.1", 0x0800001, 0x200000, CRC(c891c887) SHA1(42e84f774ee655e9a39b016a3cfe94262ed2e9f1) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.2", 0x0800002, 0x200000, CRC(1e101f30) SHA1(56518c1646bb9452334856bb8bcc58892f9f93b9) ) \
	ROM_LOAD32_BYTE( "jojo-simm2.3", 0x0800003, 0x200000, CRC(1fd1d3e4) SHA1(bed2b77d58f1fdf7ba5ca7126d3db1dd0f8c80b4) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "jojo-simm3.0", 0x0000000, 0x200000, CRC(de7fc9c1) SHA1(662b85a990b04c855773506c936317e62fab4a05) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.1", 0x0000001, 0x200000, CRC(43d053d3) SHA1(54ff0e9c164e0d1649522c330ccc7e5d79e0bc85) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.2", 0x0400000, 0x200000, CRC(2ffd7fa5) SHA1(9018c8e2b286a333ba606208e90caa764951ea3f) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.3", 0x0400001, 0x200000, CRC(4da4985b) SHA1(2552b1730a21ce17d58b69a79ad212a6a5829439) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.4", 0x0800000, 0x200000, CRC(fde98d72) SHA1(654563e12d033e8656dc74a268a08b15b171470d) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.5", 0x0800001, 0x200000, CRC(edb2a266) SHA1(19ebada8422c7f4bf70d0c9ad42b84268967b316) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.6", 0x0c00000, 0x200000, CRC(be7cf319) SHA1(7893f5907992e6b903b2683980bba6d3d003bb06) ) \
	ROM_LOAD16_BYTE( "jojo-simm3.7", 0x0c00001, 0x200000, CRC(56fe1a9f) SHA1(01741fe1256f4e682f687e94040f4e8bbb8bedb2) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.0", 0x1000000, 0x200000, CRC(c4e7bf68) SHA1(a4d1ddea58a3d42db82a63a5e974cbf38d9b792a) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.1", 0x1000001, 0x200000, CRC(b62b2719) SHA1(cb577b89e9e14fda67715716fefd47a782d518ab) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.2", 0x1400000, 0x200000, CRC(18d15809) SHA1(2b406cd1aaa4799a436213dcaa65473eacb4c6d7) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.3", 0x1400001, 0x200000, CRC(9af0ad79) SHA1(075ee048e17b50188876f25d7a6571d6ace84d7d) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.4", 0x1800000, 0x200000, CRC(4124c1f0) SHA1(e4946a8029adc5d0bacead8d766521b4ccd1722b) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.5", 0x1800001, 0x200000, CRC(5e001fd1) SHA1(6457a39f336381b46e587aa2f5f719810ee5bcf9) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.6", 0x1c00000, 0x200000, CRC(9affa23b) SHA1(e3d77e777c47277d841a9dadc1dd6e3157706a2e) ) \
	ROM_LOAD16_BYTE( "jojo-simm4.7", 0x1c00001, 0x200000, CRC(2511572a) SHA1(725adcf71bcee5c8bb839d2d1c5e3456b8c6886b) ) \
	ROM_LOAD16_BYTE( "jojo-simm5.0", 0x2000000, 0x200000, CRC(797615fc) SHA1(29874be9f1da5515c90f5d601aa5924c263f8feb) ) \
	ROM_LOAD16_BYTE( "jojo-simm5.1", 0x2000001, 0x200000, CRC(734fd162) SHA1(16cdfac74d18a6c2216afb1ce6afbd7f15297c32) )

/* SIMM data of the 990608 version */
#define CPS3_SFIII3_990608_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "sfiii3-simm1.0", 0x0000000, 0x200000, CRC(11dfd3cd) SHA1(dba1f77c46e80317e3279298411154dfb6db2309) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm1.1", 0x0000001, 0x200000, CRC(c50585e6) SHA1(a289237957ea1c7f58b1c65e24c54ceb34cb1712) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm1.2", 0x0000002, 0x200000, CRC(8e011d9b) SHA1(e0861bcd3c4f865474d7ce47aa9eeec7b3d28da6) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm1.3", 0x0000003, 0x200000, CRC(dca8d92f) SHA1(7cd241641c943df446e2c75b88b5cf2d2ebf7b2e) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.0", 0x0800000, 0x200000, CRC(06eb969e) SHA1(d89f6a6585b76692d57d337f0f8186398fb056da) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.1", 0x0800001, 0x200000, CRC(e7039f82) SHA1(8e81e66b5a4f45ae14b070a491bde47a6a74499f) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.2", 0x0800002, 0x200000, CRC(645c96f7) SHA1(06d5a54874d4bf100b776131ec9060da209ad037) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.3", 0x0800003, 0x200000, CRC(610efab1) SHA1(bbc21ed6ff6220ff6017a3f02ebd9a341fbc9040) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "sfiii3-simm3.0", 0x0000000, 0x200000, CRC(7baa1f79) SHA1(3f409df28c24dd7221966b5340d59898ea756b6f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.1", 0x0000001, 0x200000, CRC(234bf8fe) SHA1(2191781ae4d726cab28de97f27efa4a13f3bdd69) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.2", 0x0400000, 0x200000, CRC(d9ebc308) SHA1(af6a0dca77e5181c9f20533a06760a782c5fd51d) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.3", 0x0400001, 0x200000, CRC(293cba77) SHA1(294604cacdc24261aec4d39e489de91c41fa1758) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.4", 0x0800000, 0x200000, CRC(6055e747) SHA1(3813852c5a4a5355ef739ca8f0913bbd390b984b) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.5", 0x0800001, 0x200000, CRC(499aa6fc) SHA1(5b9b6eab3e99ff3e1d7c1f50b9d8bc6a81f3f8a9) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.6", 0x0c00000, 0x200000, CRC(6c13879e) SHA1(de189b0b8f42bc7dd89983e62bc2ecb4237b3277) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.7", 0x0c00001, 0x200000, CRC(cf4f8ede) SHA1(e0fb68fcb0e445f824c62fa828d6e1dcd7e3683a) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.0", 0x1000000, 0x200000, CRC(091fd5ba) SHA1(3327ad7c2623c119bf728af717ea2ce3b74673a9) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.1", 0x1000001, 0x200000, CRC(0bca8917) SHA1(b7b284e2f16f46d46bcfaae779b232c5b980924f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.2", 0x1400000, 0x200000, CRC(a0fd578b) SHA1(100c9db9f00ecd88d518076f5a0822e6ac3695b3) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.3", 0x1400001, 0x200000, CRC(4bf8c699) SHA1(2c0b4288b5ebc5e54d9e782dfc39eb8c78fd4c21) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.4", 0x1800000, 0x200000, CRC(137b8785) SHA1(56a579520a8ce2abbf36be57777f024e80474eee) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.5", 0x1800001, 0x200000, CRC(4fb70671) SHA1(9aba83c18cfc099a5ce18793119bff0c2b9c777f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.6", 0x1c00000, 0x200000, CRC(832374a4) SHA1(c84629e32fbf47cb7b5b4ee7555bfc2ac9b3857f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.7", 0x1c00001, 0x200000, CRC(1c88576d) SHA1(0f039944d0c2305999ed5dbd351c3eb87812dc3b) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.0", 0x2000000, 0x200000, CRC(c67d9190) SHA1(d265475244099d0ec153059986f3445c7bd910a3) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.1", 0x2000001, 0x200000, CRC(6cb79868) SHA1(c94237f30e05bfcb2e23945530c812d9e4c73416) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.2", 0x2400000, 0x200000, CRC(df69930e) SHA1(c76b7c559a1d5558138afbc796249efa2f49f6a8) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.3", 0x2400001, 0x200000, CRC(333754e0) SHA1(4c18a569c26524a492ecd6f4c8b3c8e803a077d3) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.4", 0x2800000, 0x200000, CRC(78f6d417) SHA1(a69577cc5399fcf0a24548661168f27f3e7e8e40) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.5", 0x2800001, 0x200000, CRC(8ccad9b1) SHA1(f8bda399f87be2497b7ac39e9661f9863bf4f873) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.6", 0x2c00000, 0x200000, CRC(85de59e5) SHA1(748b5c91f15777b85d8c1d35b685cd90d3185ec6) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.7", 0x2c00001, 0x200000, CRC(ee7e29b3) SHA1(63dc30c6904ca2f58d229249bee5eef51fafa158) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.0", 0x3000000, 0x200000, CRC(8da69042) SHA1(fd3d08295342635b2136e48d543c9350d287bb22) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.1", 0x3000001, 0x200000, CRC(1c8c7ac4) SHA1(ac9f8353a4c356ef98aa7c226baba00b01f5a80f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.2", 0x3400000, 0x200000, CRC(a671341d) SHA1(636f4c04962bc1e1ddb29d2e01244b00389b234f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.3", 0x3400001, 0x200000, CRC(1a990249) SHA1(2acc639e2c0c53bf24096b8620eab090bc25d03b) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.4", 0x3800000, 0x200000, CRC(20cb39ac) SHA1(7d13a0fea1ef719dd2ff77dfb547d53c6023cc9e) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.5", 0x3800001, 0x200000, CRC(5f844b2f) SHA1(564e4934f89ed3b92a4c4874519f8f00f3b48696) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.6", 0x3c00000, 0x200000, CRC(450e8d28) SHA1(885db658132aa27926df617ec2d2a1f38abdbb60) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.7", 0x3c00001, 0x200000, CRC(cc5f4187) SHA1(248ddace21ed4736a56e92f77cc6ad219d7fef0b) )

/* SIMM data of the 990512 version */
#define CPS3_SFIII3_990512_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "sfiii3-simm1.0", 0x0000000, 0x200000, CRC(66e66235) SHA1(0a98038721d176458d4f85dbd76c5edb93a65322) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm1.1", 0x0000001, 0x200000, CRC(186e8c5f) SHA1(a63040201a660b56217a8cbab32f5c2c466ee5dd) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm1.2", 0x0000002, 0x200000, CRC(bce18cab) SHA1(a5c28063d98c22403756fc926a20631456fb7dcc) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm1.3", 0x0000003, 0x200000, CRC(129dc2c9) SHA1(c1e634d94b1c8f7f02a47703622de5cab3d0da3f) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.0", 0x0800000, 0x200000, CRC(06eb969e) SHA1(d89f6a6585b76692d57d337f0f8186398fb056da) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.1", 0x0800001, 0x200000, CRC(e7039f82) SHA1(8e81e66b5a4f45ae14b070a491bde47a6a74499f) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.2", 0x0800002, 0x200000, CRC(645c96f7) SHA1(06d5a54874d4bf100b776131ec9060da209ad037) ) \
	ROM_LOAD32_BYTE( "sfiii3-simm2.3", 0x0800003, 0x200000, CRC(610efab1) SHA1(bbc21ed6ff6220ff6017a3f02ebd9a341fbc9040) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "sfiii3-simm3.0", 0x0000000, 0x200000, CRC(7baa1f79) SHA1(3f409df28c24dd7221966b5340d59898ea756b6f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.1", 0x0000001, 0x200000, CRC(234bf8fe) SHA1(2191781ae4d726cab28de97f27efa4a13f3bdd69) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.2", 0x0400000, 0x200000, CRC(d9ebc308) SHA1(af6a0dca77e5181c9f20533a06760a782c5fd51d) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.3", 0x0400001, 0x200000, CRC(293cba77) SHA1(294604cacdc24261aec4d39e489de91c41fa1758) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.4", 0x0800000, 0x200000, CRC(6055e747) SHA1(3813852c5a4a5355ef739ca8f0913bbd390b984b) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.5", 0x0800001, 0x200000, CRC(499aa6fc) SHA1(5b9b6eab3e99ff3e1d7c1f50b9d8bc6a81f3f8a9) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.6", 0x0c00000, 0x200000, CRC(6c13879e) SHA1(de189b0b8f42bc7dd89983e62bc2ecb4237b3277) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm3.7", 0x0c00001, 0x200000, CRC(cf4f8ede) SHA1(e0fb68fcb0e445f824c62fa828d6e1dcd7e3683a) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.0", 0x1000000, 0x200000, CRC(091fd5ba) SHA1(3327ad7c2623c119bf728af717ea2ce3b74673a9) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.1", 0x1000001, 0x200000, CRC(0bca8917) SHA1(b7b284e2f16f46d46bcfaae779b232c5b980924f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.2", 0x1400000, 0x200000, CRC(a0fd578b) SHA1(100c9db9f00ecd88d518076f5a0822e6ac3695b3) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.3", 0x1400001, 0x200000, CRC(4bf8c699) SHA1(2c0b4288b5ebc5e54d9e782dfc39eb8c78fd4c21) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.4", 0x1800000, 0x200000, CRC(137b8785) SHA1(56a579520a8ce2abbf36be57777f024e80474eee) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.5", 0x1800001, 0x200000, CRC(4fb70671) SHA1(9aba83c18cfc099a5ce18793119bff0c2b9c777f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.6", 0x1c00000, 0x200000, CRC(832374a4) SHA1(c84629e32fbf47cb7b5b4ee7555bfc2ac9b3857f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm4.7", 0x1c00001, 0x200000, CRC(1c88576d) SHA1(0f039944d0c2305999ed5dbd351c3eb87812dc3b) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.0", 0x2000000, 0x200000, CRC(c67d9190) SHA1(d265475244099d0ec153059986f3445c7bd910a3) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.1", 0x2000001, 0x200000, CRC(6cb79868) SHA1(c94237f30e05bfcb2e23945530c812d9e4c73416) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.2", 0x2400000, 0x200000, CRC(df69930e) SHA1(c76b7c559a1d5558138afbc796249efa2f49f6a8) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.3", 0x2400001, 0x200000, CRC(333754e0) SHA1(4c18a569c26524a492ecd6f4c8b3c8e803a077d3) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.4", 0x2800000, 0x200000, CRC(78f6d417) SHA1(a69577cc5399fcf0a24548661168f27f3e7e8e40) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.5", 0x2800001, 0x200000, CRC(8ccad9b1) SHA1(f8bda399f87be2497b7ac39e9661f9863bf4f873) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.6", 0x2c00000, 0x200000, CRC(85de59e5) SHA1(748b5c91f15777b85d8c1d35b685cd90d3185ec6) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm5.7", 0x2c00001, 0x200000, CRC(ee7e29b3) SHA1(63dc30c6904ca2f58d229249bee5eef51fafa158) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.0", 0x3000000, 0x200000, CRC(8da69042) SHA1(fd3d08295342635b2136e48d543c9350d287bb22) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.1", 0x3000001, 0x200000, CRC(1c8c7ac4) SHA1(ac9f8353a4c356ef98aa7c226baba00b01f5a80f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.2", 0x3400000, 0x200000, CRC(a671341d) SHA1(636f4c04962bc1e1ddb29d2e01244b00389b234f) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.3", 0x3400001, 0x200000, CRC(1a990249) SHA1(2acc639e2c0c53bf24096b8620eab090bc25d03b) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.4", 0x3800000, 0x200000, CRC(20cb39ac) SHA1(7d13a0fea1ef719dd2ff77dfb547d53c6023cc9e) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.5", 0x3800001, 0x200000, CRC(5f844b2f) SHA1(564e4934f89ed3b92a4c4874519f8f00f3b48696) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.6", 0x3c00000, 0x200000, CRC(450e8d28) SHA1(885db658132aa27926df617ec2d2a1f38abdbb60) ) \
	ROM_LOAD16_BYTE( "sfiii3-simm6.7", 0x3c00001, 0x200000, CRC(cc5f4187) SHA1(248ddace21ed4736a56e92f77cc6ad219d7fef0b) )

/* SIMM data of the 991015 version */
#define CPS3_JOJOBA_991015_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "jojoba-simm1.0", 0x0000000, 0x200000, CRC(b3cc516d) SHA1(cac67a5bf9d01af226be377e2b1b8ed4ea7fd164) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.1", 0x0000001, 0x200000, CRC(dab4bdc7) SHA1(47283071de152a0b2b96d3f3fc22537677c35d3f) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.2", 0x0000002, 0x200000, CRC(a6a4bf48) SHA1(1684663252894538de4855eee336ffc031142ba9) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.3", 0x0000003, 0x200000, CRC(731229ee) SHA1(2171ffa115752b0dcc1695f4a5d0557237a73db8) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.0", 0x0800000, 0x200000, CRC(535f2eba) SHA1(167bec0dccfc2f91cb10cb1e2631ee619b3eb9fe) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.1", 0x0800001, 0x200000, CRC(01dd3a01) SHA1(08c462219796baa3ec28d78d038a18187cd838bb) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.2", 0x0800002, 0x200000, CRC(61432672) SHA1(d0416a75d395926041f90a3d34edb96a080acfd6) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.3", 0x0800003, 0x200000, CRC(acdc9aca) SHA1(89f77ddd6286709182a676fd9bd6c333a3b16271) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "jojoba-simm3.0", 0x0000000, 0x200000, CRC(4d16e111) SHA1(f198007375be65e89856d64ee2b3857a18b4eab8) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.1", 0x0000001, 0x200000, CRC(9b3406d3) SHA1(54e90cd334d13e2c74305c6b87ebce1365ef3d59) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.2", 0x0400000, 0x200000, CRC(f2414997) SHA1(fb89d5784250538ad17fd527267b513afb6eca20) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.3", 0x0400001, 0x200000, CRC(954b9c7d) SHA1(0d64d97167d4e669d7e4f3a388f9d5ec1e18ed42) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.4", 0x0800000, 0x200000, CRC(625adc1d) SHA1(533d62759ecece10c711d99bfca403e5cba279b5) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.5", 0x0800001, 0x200000, CRC(20a70bb4) SHA1(3bd8376304ffc974fb8031eac8bebff27969538c) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.6", 0x0c00000, 0x200000, CRC(a10ec5af) SHA1(9b403260e8fbdacaa5369ab79fc05855cc6a6bdb) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.7", 0x0c00001, 0x200000, CRC(0bd0de7a) SHA1(1debecda5f282f2a1dd17e887e522a4d00c5dc9d) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.0", 0x1000000, 0x200000, CRC(6ea14adc) SHA1(696b2ec66f3c197817a60f507a1b4c78db37f488) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.1", 0x1000001, 0x200000, CRC(8f4c42fb) SHA1(363d769b0b066ce139125426d2da6dfa15d1eb28) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.2", 0x1400000, 0x200000, CRC(ef0586d1) SHA1(8fcc350da20e3e59fa76fa14e10f2c47233ba9dc) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.3", 0x1400001, 0x200000, CRC(93ccc470) SHA1(5d267679e61c0fb592ad5f696d3c06ec1746d0b3) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.4", 0x1800000, 0x200000, CRC(3d9ec7d2) SHA1(665b867bab928be183c2006527e55f9b8ec4a271) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.5", 0x1800001, 0x200000, CRC(03e66850) SHA1(8478662dc9db20d9a186d315a883bd1cbb5e5000) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.6", 0x1c00000, 0x200000, CRC(01606ac3) SHA1(ccc74edeca6abdd86fc1cf42ececa1ea393b3261) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.7", 0x1c00001, 0x200000, CRC(36392b87) SHA1(e62080c8461775c1e180400dfb44414679fd0fc1) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.0", 0x2000000, 0x200000, CRC(2ef8c60c) SHA1(dea87a73a11b8edd27c3c9c5ab2af295cb5508f9) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.1", 0x2000001, 0x200000, CRC(cf7d7ca6) SHA1(b347707b1e5bc71d28b282273f893592e5f9e333) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.2", 0x2400000, 0x200000, CRC(b7815bfa) SHA1(0b5a3a2ffe1b3c0ca765dcedc297e78e5928302b) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.3", 0x2400001, 0x200000, CRC(9bfec049) SHA1(62cc9a1920047863205544b77344ee18f310f084) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.4", 0x2800000, 0x200000, CRC(d167536b) SHA1(e2637d3486f168ce44e0a00413d38960cb86db4c) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.5", 0x2800001, 0x200000, CRC(55e7a042) SHA1(c18bda61fa005d9174a27b7b7d324004262a4525) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.6", 0x2c00000, 0x200000, CRC(4fb32906) SHA1(3a5965b3197517932c8aa4c07a6ea6a190a338d7) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.7", 0x2c00001, 0x200000, CRC(8c8be520) SHA1(c461f3f76a83592b36b29afb316679a7c8972404) )

/* SIMM data of the 990927 version */
#define CPS3_JOJOBA_990927_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "jojoba-simm1.0", 0x0000000, 0x200000, CRC(adcd8377) SHA1(f1aacbe061e3bcade5cca34435c3f86aec5f1499) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.1", 0x0000001, 0x200000, CRC(d7590b59) SHA1(bfee627ebb7cb7b28216527b17e1b06a4e6f19f4) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.2", 0x0000002, 0x200000, CRC(e62e240b) SHA1(70468cae67c009a80b45954c2a30794577343c77) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.3", 0x0000003, 0x200000, CRC(c95450c3) SHA1(55616e009b007180d1ac6290c8da44b0d864a494) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.0", 0x0800000, 0x200000, CRC(535f2eba) SHA1(167bec0dccfc2f91cb10cb1e2631ee619b3eb9fe) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.1", 0x0800001, 0x200000, CRC(01dd3a01) SHA1(08c462219796baa3ec28d78d038a18187cd838bb) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.2", 0x0800002, 0x200000, CRC(61432672) SHA1(d0416a75d395926041f90a3d34edb96a080acfd6) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.3", 0x0800003, 0x200000, CRC(acdc9aca) SHA1(89f77ddd6286709182a676fd9bd6c333a3b16271) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "jojoba-simm3.0", 0x0000000, 0x200000, CRC(4d16e111) SHA1(f198007375be65e89856d64ee2b3857a18b4eab8) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.1", 0x0000001, 0x200000, CRC(9b3406d3) SHA1(54e90cd334d13e2c74305c6b87ebce1365ef3d59) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.2", 0x0400000, 0x200000, CRC(f2414997) SHA1(fb89d5784250538ad17fd527267b513afb6eca20) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.3", 0x0400001, 0x200000, CRC(954b9c7d) SHA1(0d64d97167d4e669d7e4f3a388f9d5ec1e18ed42) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.4", 0x0800000, 0x200000, CRC(625adc1d) SHA1(533d62759ecece10c711d99bfca403e5cba279b5) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.5", 0x0800001, 0x200000, CRC(20a70bb4) SHA1(3bd8376304ffc974fb8031eac8bebff27969538c) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.6", 0x0c00000, 0x200000, CRC(a10ec5af) SHA1(9b403260e8fbdacaa5369ab79fc05855cc6a6bdb) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.7", 0x0c00001, 0x200000, CRC(0bd0de7a) SHA1(1debecda5f282f2a1dd17e887e522a4d00c5dc9d) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.0", 0x1000000, 0x200000, CRC(6ea14adc) SHA1(696b2ec66f3c197817a60f507a1b4c78db37f488) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.1", 0x1000001, 0x200000, CRC(8f4c42fb) SHA1(363d769b0b066ce139125426d2da6dfa15d1eb28) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.2", 0x1400000, 0x200000, CRC(ef0586d1) SHA1(8fcc350da20e3e59fa76fa14e10f2c47233ba9dc) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.3", 0x1400001, 0x200000, CRC(93ccc470) SHA1(5d267679e61c0fb592ad5f696d3c06ec1746d0b3) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.4", 0x1800000, 0x200000, CRC(3d9ec7d2) SHA1(665b867bab928be183c2006527e55f9b8ec4a271) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.5", 0x1800001, 0x200000, CRC(03e66850) SHA1(8478662dc9db20d9a186d315a883bd1cbb5e5000) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.6", 0x1c00000, 0x200000, CRC(01606ac3) SHA1(ccc74edeca6abdd86fc1cf42ececa1ea393b3261) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.7", 0x1c00001, 0x200000, CRC(36392b87) SHA1(e62080c8461775c1e180400dfb44414679fd0fc1) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.0", 0x2000000, 0x200000, CRC(2ef8c60c) SHA1(dea87a73a11b8edd27c3c9c5ab2af295cb5508f9) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.1", 0x2000001, 0x200000, CRC(cf7d7ca6) SHA1(b347707b1e5bc71d28b282273f893592e5f9e333) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.2", 0x2400000, 0x200000, CRC(b7815bfa) SHA1(0b5a3a2ffe1b3c0ca765dcedc297e78e5928302b) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.3", 0x2400001, 0x200000, CRC(9bfec049) SHA1(62cc9a1920047863205544b77344ee18f310f084) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.4", 0x2800000, 0x200000, CRC(d167536b) SHA1(e2637d3486f168ce44e0a00413d38960cb86db4c) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.5", 0x2800001, 0x200000, CRC(55e7a042) SHA1(c18bda61fa005d9174a27b7b7d324004262a4525) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.6", 0x2c00000, 0x200000, CRC(4fb32906) SHA1(3a5965b3197517932c8aa4c07a6ea6a190a338d7) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.7", 0x2c00001, 0x200000, CRC(8c8be520) SHA1(c461f3f76a83592b36b29afb316679a7c8972404) )

/* SIMM data of the 990913 version */
#define CPS3_JOJOBA_990913_SIMMS \
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */ \
	ROM_LOAD32_BYTE( "jojoba-simm1.0", 0x0000000, 0x200000, CRC(76976231) SHA1(90adde7e5983ec6a4e02789d5cefe9e85c9c52d5) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.1", 0x0000001, 0x200000, CRC(cedd78e7) SHA1(964988b90a2f14c1da2cfc48d943e16e54da3fd3) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.2", 0x0000002, 0x200000, CRC(2955b77f) SHA1(2a907a5cd91448bfc420c318584e5ef4bbe55a91) ) \
	ROM_LOAD32_BYTE( "jojoba-simm1.3", 0x0000003, 0x200000, CRC(280139d7) SHA1(b7c28f6f0218688fb873a3106d2f95ea2e1e927c) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.0", 0x0800000, 0x200000, CRC(305c4914) SHA1(c3a73ffe58f61ab8f1cd9e3f0891037638dc5a9b) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.1", 0x0800001, 0x200000, CRC(18af4f3b) SHA1(04b8fdf23a782b10c203b111cc634a6d3474044a) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.2", 0x0800002, 0x200000, CRC(397e5c9e) SHA1(021d86ee66bf951fb6a1dd90fb7007c6865cbb8b) ) \
	ROM_LOAD32_BYTE( "jojoba-simm2.3", 0x0800003, 0x200000, CRC(a9d0a7d7) SHA1(b2cfc0661f8903ddbeea8a604ee8b42097e10ab8) ) \
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */ \
	ROM_LOAD16_BYTE( "jojoba-simm3.0", 0x0000000, 0x200000, CRC(4d16e111) SHA1(f198007375be65e89856d64ee2b3857a18b4eab8) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.1", 0x0000001, 0x200000, CRC(9b3406d3) SHA1(54e90cd334d13e2c74305c6b87ebce1365ef3d59) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.2", 0x0400000, 0x200000, CRC(f2414997) SHA1(fb89d5784250538ad17fd527267b513afb6eca20) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.3", 0x0400001, 0x200000, CRC(954b9c7d) SHA1(0d64d97167d4e669d7e4f3a388f9d5ec1e18ed42) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.4", 0x0800000, 0x200000, CRC(625adc1d) SHA1(533d62759ecece10c711d99bfca403e5cba279b5) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.5", 0x0800001, 0x200000, CRC(20a70bb4) SHA1(3bd8376304ffc974fb8031eac8bebff27969538c) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.6", 0x0c00000, 0x200000, CRC(a10ec5af) SHA1(9b403260e8fbdacaa5369ab79fc05855cc6a6bdb) ) \
	ROM_LOAD16_BYTE( "jojoba-simm3.7", 0x0c00001, 0x200000, CRC(0bd0de7a) SHA1(1debecda5f282f2a1dd17e887e522a4d00c5dc9d) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.0", 0x1000000, 0x200000, CRC(6ea14adc) SHA1(696b2ec66f3c197817a60f507a1b4c78db37f488) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.1", 0x1000001, 0x200000, CRC(8f4c42fb) SHA1(363d769b0b066ce139125426d2da6dfa15d1eb28) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.2", 0x1400000, 0x200000, CRC(ef0586d1) SHA1(8fcc350da20e3e59fa76fa14e10f2c47233ba9dc) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.3", 0x1400001, 0x200000, CRC(93ccc470) SHA1(5d267679e61c0fb592ad5f696d3c06ec1746d0b3) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.4", 0x1800000, 0x200000, CRC(3d9ec7d2) SHA1(665b867bab928be183c2006527e55f9b8ec4a271) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.5", 0x1800001, 0x200000, CRC(03e66850) SHA1(8478662dc9db20d9a186d315a883bd1cbb5e5000) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.6", 0x1c00000, 0x200000, CRC(01606ac3) SHA1(ccc74edeca6abdd86fc1cf42ececa1ea393b3261) ) \
	ROM_LOAD16_BYTE( "jojoba-simm4.7", 0x1c00001, 0x200000, CRC(36392b87) SHA1(e62080c8461775c1e180400dfb44414679fd0fc1) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.0", 0x2000000, 0x200000, CRC(2ef8c60c) SHA1(dea87a73a11b8edd27c3c9c5ab2af295cb5508f9) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.1", 0x2000001, 0x200000, CRC(cf7d7ca6) SHA1(b347707b1e5bc71d28b282273f893592e5f9e333) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.2", 0x2400000, 0x200000, CRC(b7815bfa) SHA1(0b5a3a2ffe1b3c0ca765dcedc297e78e5928302b) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.3", 0x2400001, 0x200000, CRC(9bfec049) SHA1(62cc9a1920047863205544b77344ee18f310f084) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.4", 0x2800000, 0x200000, CRC(d167536b) SHA1(e2637d3486f168ce44e0a00413d38960cb86db4c) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.5", 0x2800001, 0x200000, CRC(55e7a042) SHA1(c18bda61fa005d9174a27b7b7d324004262a4525) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.6", 0x2c00000, 0x200000, CRC(4fb32906) SHA1(3a5965b3197517932c8aa4c07a6ea6a190a338d7) ) \
	ROM_LOAD16_BYTE( "jojoba-simm5.7", 0x2c00001, 0x200000, CRC(8c8be520) SHA1(c461f3f76a83592b36b29afb316679a7c8972404) )


ROM_START( redearth )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "redearth_euro.29f400.u2", 0x000000, 0x080000, CRC(02e0f336) SHA1(acc37e830dfeb9674f5a0fb24f4cc23217ae4ff5) )
	CPS3_REDEARTH_961121_SIMMS
ROM_END

ROM_START( warzard )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "warzard_japan.29f400.u2", 0x000000, 0x080000, CRC(f8e2f0c6) SHA1(93d6a986f44c211fff014e55681eca4d2a2774d6) )
	CPS3_REDEARTH_961121_SIMMS
ROM_END

ROM_START( redearthn )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "redearth_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(7a4f0851) SHA1(660ca716960ec761038e5ad4de636be13b0dddd8) )
	CPS3_REDEARTH_961121_SIMMS
ROM_END

ROM_START( redearthr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "redearth_euro.29f400.u2", 0x000000, 0x080000, CRC(02e0f336) SHA1(acc37e830dfeb9674f5a0fb24f4cc23217ae4ff5) )
	CPS3_REDEARTH_961023_SIMMS
ROM_END

ROM_START( warzardr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "warzard_japan.29f400.u2", 0x000000, 0x080000, CRC(f8e2f0c6) SHA1(93d6a986f44c211fff014e55681eca4d2a2774d6) )
	CPS3_REDEARTH_961023_SIMMS
ROM_END

ROM_START( redearthnr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "redearth_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(7a4f0851) SHA1(660ca716960ec761038e5ad4de636be13b0dddd8) )
	CPS3_REDEARTH_961023_SIMMS
ROM_END

ROM_START( sfiii )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_euro.29f400.u2", 0x000000, 0x080000, CRC(27699ddc) SHA1(d8b525cd27e584560b129598df31fd2c5b2a682a) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiiiu )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_usa_region_b1.29f400.u2", 0x000000, 0x080000, CRC(fb172a8e) SHA1(48ebf59910f246835f7dc0c588da30f7a908072f) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiiia )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_asia_region_bd.29f400.u2", 0x000000, 0x080000, CRC(cbd28de7) SHA1(9c15ecb73b9587d20850e62e8683930a45caa01b) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiiij )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_japan.29f400.u2", 0x000000, 0x080000, CRC(74205250) SHA1(c3e83ace7121d32da729162662ec6b5285a31211) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiiih )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_hispanic.29f400.u2", 0x000000, 0x080000, CRC(d2b3cd48) SHA1(00ebb270c24a66515c97e35331de54ff5358000e) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiiin )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(ca2b715f) SHA1(86319987f9af4afd272a2488e73de8382743cb37) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiiina )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(73e32463) SHA1(45d144e533e4b20cc5a744ca4f618e288430c601) )
	CPS3_SFIII_970204_SIMMS
ROM_END

ROM_START( sfiii2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii2_usa.29f400.u2", 0x000000, 0x080000, CRC(75dd72e0) SHA1(5a12d6ea6734df5de00ecee6f9ef470749d2f242) )
	CPS3_SFIII2_970930_SIMMS
ROM_END

ROM_START( sfiii2j )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii2_japan.29f400.u2", 0x000000, 0x080000, CRC(faea0a3e) SHA1(a03cd63bcf52e4d57f7a598c8bc8e243694624ec) )
	CPS3_SFIII2_970930_SIMMS
ROM_END

ROM_START( sfiii2h )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii2_hispanic.29f400.u2", 0x000000, 0x080000, CRC(5c799526) SHA1(5a56345b500cd7e20d5fb6cabc791655c6ff4ed2) )
	CPS3_SFIII2_970930_SIMMS
ROM_END

ROM_START( sfiii2a )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii2_asia.29f400.u2", 0x000000, 0x080000, CRC(4518d589) SHA1(43dba305c1b630e0ee51697b6802109ebb8268b5) )
	CPS3_SFIII2_970930_SIMMS
ROM_END

ROM_START( sfiii2n )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii2_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(fd297c0d) SHA1(4323deda2789f104b53f32a663196ec16de73215) )
	CPS3_SFIII2_970930_SIMMS
ROM_END

ROM_START( jojo )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_euro.29f400.u2", 0x000000, 0x080000, CRC(513e40ec) SHA1(03b91f0fbd5be56d24feed4698c7543d4df07837) )
	CPS3_JOJO_990128_SIMMS
ROM_END

ROM_START( jojou )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_usa.29f400.u2", 0x000000, 0x080000, CRC(8d40f7be) SHA1(2a4bd83db2f959c33b071e517941aa55a0f919c0) )
	CPS3_JOJO_990128_SIMMS
ROM_END

ROM_START( jojoa )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_asia.29f400.u2", 0x000000, 0x080000, CRC(789aa72a) SHA1(afcefb963d7c103514585f4a6738b2deb5b7d27a) )
	CPS3_JOJO_990128_SIMMS
ROM_END

ROM_START( jojoj )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_japan.29f400.u2", 0x000000, 0x080000, CRC(02778f60) SHA1(a167f9ebe030592a0cdb0c6a3c75835c6a43be4c) )
	CPS3_JOJO_990128_SIMMS
ROM_END

ROM_START( jojon )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(05b4f953) SHA1(c746c7bb5359acc9adced817cb4870b1912eaefd) )
	CPS3_JOJO_990128_SIMMS
ROM_END

ROM_START( jojor1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_euro.29f400.u2", 0x000000, 0x080000, CRC(513e40ec) SHA1(03b91f0fbd5be56d24feed4698c7543d4df07837) )
	CPS3_JOJO_990108_SIMMS
ROM_END

ROM_START( jojour1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_usa.29f400.u2", 0x000000, 0x080000, CRC(8d40f7be) SHA1(2a4bd83db2f959c33b071e517941aa55a0f919c0) )
	CPS3_JOJO_990108_SIMMS
ROM_END

ROM_START( jojoar1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_asia.29f400.u2", 0x000000, 0x080000, CRC(789aa72a) SHA1(afcefb963d7c103514585f4a6738b2deb5b7d27a) )
	CPS3_JOJO_990108_SIMMS
ROM_END

ROM_START( jojojr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_japan.29f400.u2", 0x000000, 0x080000, CRC(02778f60) SHA1(a167f9ebe030592a0cdb0c6a3c75835c6a43be4c) )
	CPS3_JOJO_990108_SIMMS
ROM_END

ROM_START( jojonr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(05b4f953) SHA1(c746c7bb5359acc9adced817cb4870b1912eaefd) )
	CPS3_JOJO_990108_SIMMS
ROM_END

ROM_START( jojor2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_euro.29f400.u2", 0x000000, 0x080000, CRC(513e40ec) SHA1(03b91f0fbd5be56d24feed4698c7543d4df07837) )
	CPS3_JOJO_981202_SIMMS
ROM_END

ROM_START( jojour2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_usa.29f400.u2", 0x000000, 0x080000, CRC(8d40f7be) SHA1(2a4bd83db2f959c33b071e517941aa55a0f919c0) )
	CPS3_JOJO_981202_SIMMS
ROM_END

ROM_START( jojoar2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_asia.29f400.u2", 0x000000, 0x080000, CRC(789aa72a) SHA1(afcefb963d7c103514585f4a6738b2deb5b7d27a) )
	CPS3_JOJO_981202_SIMMS
ROM_END

ROM_START( jojojr2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_japan.29f400.u2", 0x000000, 0x080000, CRC(02778f60) SHA1(a167f9ebe030592a0cdb0c6a3c75835c6a43be4c) )
	CPS3_JOJO_981202_SIMMS
ROM_END

ROM_START( jojonr2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojo_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(05b4f953) SHA1(c746c7bb5359acc9adced817cb4870b1912eaefd) )
	CPS3_JOJO_981202_SIMMS
ROM_END

ROM_START( sfiii3 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_euro.29f400.u2", 0x000000, 0x080000, CRC(30bbf293) SHA1(f094c2eeaf4f6709060197aca371a4532346bf78) )
	CPS3_SFIII3_990608_SIMMS
ROM_END

ROM_START( sfiii3u )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_usa.29f400.u2", 0x000000, 0x080000, CRC(ecc545c1) SHA1(e39083820aae914fd8b80c9765129bedb745ceba) )
	CPS3_SFIII3_990608_SIMMS
ROM_END

ROM_START( sfiii3j )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_japan.29f400.u2", 0x000000, 0x080000, CRC(63f23d1f) SHA1(58559403c325454f8c8d3eb0f569a531aa22db26) )
	CPS3_SFIII3_990608_SIMMS
ROM_END

ROM_START( sfiii3n )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_japan_nocd.29f400.u2", 0x000000, 0x080000, CRC(1edc6366) SHA1(60b4b9adeb030a33059d74fdf03873029e465b52) )
	CPS3_SFIII3_990608_SIMMS
ROM_END

ROM_START( sfiii3na )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(a12ebcd1) SHA1(f0925a3099e3279da42d2d292dfa69e834219197) )
	CPS3_SFIII3_990608_SIMMS
ROM_END

ROM_START( sfiii3r1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_euro.29f400.u2", 0x000000, 0x080000, CRC(30bbf293) SHA1(f094c2eeaf4f6709060197aca371a4532346bf78) )
	CPS3_SFIII3_990512_SIMMS
ROM_END

ROM_START( sfiii3ur1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_usa.29f400.u2", 0x000000, 0x080000, CRC(ecc545c1) SHA1(e39083820aae914fd8b80c9765129bedb745ceba) )
	CPS3_SFIII3_990512_SIMMS
ROM_END

ROM_START( sfiii3jr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_japan.29f400.u2", 0x000000, 0x080000, CRC(63f23d1f) SHA1(58559403c325454f8c8d3eb0f569a531aa22db26) )
	CPS3_SFIII3_990512_SIMMS
ROM_END

ROM_START( sfiii3nr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_japan_nocd.29f400.u2", 0x000000, 0x080000, CRC(1edc6366) SHA1(60b4b9adeb030a33059d74fdf03873029e465b52) )
	CPS3_SFIII3_990512_SIMMS
ROM_END

ROM_START( sfiii3nar1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_asia_nocd.29f400.u2", 0x000000, 0x080000, CRC(a12ebcd1) SHA1(f0925a3099e3279da42d2d292dfa69e834219197) )
	CPS3_SFIII3_990512_SIMMS
ROM_END

ROM_START( jojoba )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_euro_nocd.29f400.u2", 0x000000, 0x080000, CRC(1ee2d679) SHA1(9e129b454a376606b3f7e8aec64de425cf9c635c) )
	CPS3_JOJOBA_991015_SIMMS
ROM_END

ROM_START( jojoban )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_japan_nocd.29f400.u2", 0x000000, 0x080000, CRC(4dab19f5) SHA1(ba07190e7662937fc267f07285c51e99a45c061e) )
	CPS3_JOJOBA_991015_SIMMS
ROM_END

ROM_START( jojobar1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_euro.29f400.u2", 0x000000, 0x080000, CRC(63cc8800) SHA1(f0c7e6abb205a16dab7a114e017b193521071a4b) )
	CPS3_JOJOBA_990927_SIMMS
ROM_END

ROM_START( jojobajr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_japan.29f400.u2", 0x000000, 0x080000, CRC(3085478c) SHA1(055eab1fc42816f370a44b17fd7e87ffcb10e8b7) )
	CPS3_JOJOBA_990927_SIMMS
ROM_END

ROM_START( jojobanr1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_japan_nocd.29f400.u2", 0x000000, 0x080000, CRC(4dab19f5) SHA1(ba07190e7662937fc267f07285c51e99a45c061e) )
	CPS3_JOJOBA_990927_SIMMS
ROM_END

ROM_START( jojobaner1 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_euro_nocd.29f400.u2", 0x000000, 0x080000, CRC(1ee2d679) SHA1(9e129b454a376606b3f7e8aec64de425cf9c635c) )
	CPS3_JOJOBA_990927_SIMMS
ROM_END

ROM_START( jojobar2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_euro.29f400.u2", 0x000000, 0x080000, CRC(63cc8800) SHA1(f0c7e6abb205a16dab7a114e017b193521071a4b) )
	CPS3_JOJOBA_990913_SIMMS
ROM_END

ROM_START( jojobajr2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_japan.29f400.u2", 0x000000, 0x080000, CRC(3085478c) SHA1(055eab1fc42816f370a44b17fd7e87ffcb10e8b7) )
	CPS3_JOJOBA_990913_SIMMS
ROM_END

ROM_START( jojobanr2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_japan_nocd.29f400.u2", 0x000000, 0x080000, CRC(4dab19f5) SHA1(ba07190e7662937fc267f07285c51e99a45c061e) )
	CPS3_JOJOBA_990913_SIMMS
ROM_END

ROM_START( jojobaner2 )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "jojoba_euro_nocd.29f400.u2", 0x000000, 0x080000, CRC(1ee2d679) SHA1(9e129b454a376606b3f7e8aec64de425cf9c635c) )
	CPS3_JOJOBA_990913_SIMMS
ROM_END

/* hack of 3rd Strike, from FBNeo: SIMM 1 and two chips of SIMM 5 changed */
ROM_START( sfiii4n )
	ROM_REGION32_BE( 0x080000, REGION_CPU1, 0 ) /* BIOS */
	ROM_LOAD( "sfiii3_japan_nocd.29f400.u2", 0x000000, 0x080000, CRC(1edc6366) SHA1(60b4b9adeb030a33059d74fdf03873029e465b52) )
	ROM_REGION( 0x1000000, REGION_USER1, ROMREGION_ERASEFF ) /* SIMM 1-2: program, 4 chips each */
	ROM_LOAD32_BYTE( "4rd-simm1.0", 0x0000000, 0x200000, CRC(ca97f95e) SHA1(624f1af8c3722c0554a65bfb4f0682ed66a70b55) )
	ROM_LOAD32_BYTE( "4rd-simm1.1", 0x0000001, 0x200000, CRC(5bc7faa6) SHA1(291b6102d8ad7478ea45e73d34ad8d65332beaf0) )
	ROM_LOAD32_BYTE( "4rd-simm1.2", 0x0000002, 0x200000, CRC(ec2eaa29) SHA1(981d452f15da43d4577339ab5799c7561f298d62) )
	ROM_LOAD32_BYTE( "4rd-simm1.3", 0x0000003, 0x200000, CRC(41c4ce7c) SHA1(273f21735200becb0da06ab3e40023918df2a062) )
	ROM_LOAD32_BYTE( "sfiii3-simm2.0", 0x0800000, 0x200000, CRC(06eb969e) SHA1(d89f6a6585b76692d57d337f0f8186398fb056da) )
	ROM_LOAD32_BYTE( "sfiii3-simm2.1", 0x0800001, 0x200000, CRC(e7039f82) SHA1(8e81e66b5a4f45ae14b070a491bde47a6a74499f) )
	ROM_LOAD32_BYTE( "sfiii3-simm2.2", 0x0800002, 0x200000, CRC(645c96f7) SHA1(06d5a54874d4bf100b776131ec9060da209ad037) )
	ROM_LOAD32_BYTE( "sfiii3-simm2.3", 0x0800003, 0x200000, CRC(610efab1) SHA1(bbc21ed6ff6220ff6017a3f02ebd9a341fbc9040) )
	ROM_REGION( 0x4000000, REGION_USER2, ROMREGION_ERASEFF ) /* SIMM 3-6: graphics and samples, chip pairs */
	ROM_LOAD16_BYTE( "sfiii3-simm3.0", 0x0000000, 0x200000, CRC(7baa1f79) SHA1(3f409df28c24dd7221966b5340d59898ea756b6f) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.1", 0x0000001, 0x200000, CRC(234bf8fe) SHA1(2191781ae4d726cab28de97f27efa4a13f3bdd69) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.2", 0x0400000, 0x200000, CRC(d9ebc308) SHA1(af6a0dca77e5181c9f20533a06760a782c5fd51d) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.3", 0x0400001, 0x200000, CRC(293cba77) SHA1(294604cacdc24261aec4d39e489de91c41fa1758) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.4", 0x0800000, 0x200000, CRC(6055e747) SHA1(3813852c5a4a5355ef739ca8f0913bbd390b984b) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.5", 0x0800001, 0x200000, CRC(499aa6fc) SHA1(5b9b6eab3e99ff3e1d7c1f50b9d8bc6a81f3f8a9) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.6", 0x0c00000, 0x200000, CRC(6c13879e) SHA1(de189b0b8f42bc7dd89983e62bc2ecb4237b3277) )
	ROM_LOAD16_BYTE( "sfiii3-simm3.7", 0x0c00001, 0x200000, CRC(cf4f8ede) SHA1(e0fb68fcb0e445f824c62fa828d6e1dcd7e3683a) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.0", 0x1000000, 0x200000, CRC(091fd5ba) SHA1(3327ad7c2623c119bf728af717ea2ce3b74673a9) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.1", 0x1000001, 0x200000, CRC(0bca8917) SHA1(b7b284e2f16f46d46bcfaae779b232c5b980924f) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.2", 0x1400000, 0x200000, CRC(a0fd578b) SHA1(100c9db9f00ecd88d518076f5a0822e6ac3695b3) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.3", 0x1400001, 0x200000, CRC(4bf8c699) SHA1(2c0b4288b5ebc5e54d9e782dfc39eb8c78fd4c21) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.4", 0x1800000, 0x200000, CRC(137b8785) SHA1(56a579520a8ce2abbf36be57777f024e80474eee) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.5", 0x1800001, 0x200000, CRC(4fb70671) SHA1(9aba83c18cfc099a5ce18793119bff0c2b9c777f) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.6", 0x1c00000, 0x200000, CRC(832374a4) SHA1(c84629e32fbf47cb7b5b4ee7555bfc2ac9b3857f) )
	ROM_LOAD16_BYTE( "sfiii3-simm4.7", 0x1c00001, 0x200000, CRC(1c88576d) SHA1(0f039944d0c2305999ed5dbd351c3eb87812dc3b) )
	ROM_LOAD16_BYTE( "sfiii3-simm5.0", 0x2000000, 0x200000, CRC(c67d9190) SHA1(d265475244099d0ec153059986f3445c7bd910a3) )
	ROM_LOAD16_BYTE( "sfiii3-simm5.1", 0x2000001, 0x200000, CRC(6cb79868) SHA1(c94237f30e05bfcb2e23945530c812d9e4c73416) )
	ROM_LOAD16_BYTE( "sfiii3-simm5.2", 0x2400000, 0x200000, CRC(df69930e) SHA1(c76b7c559a1d5558138afbc796249efa2f49f6a8) )
	ROM_LOAD16_BYTE( "sfiii3-simm5.3", 0x2400001, 0x200000, CRC(333754e0) SHA1(4c18a569c26524a492ecd6f4c8b3c8e803a077d3) )
	ROM_LOAD16_BYTE( "sfiii3-simm5.4", 0x2800000, 0x200000, CRC(78f6d417) SHA1(a69577cc5399fcf0a24548661168f27f3e7e8e40) )
	ROM_LOAD16_BYTE( "sfiii3-simm5.5", 0x2800001, 0x200000, CRC(8ccad9b1) SHA1(f8bda399f87be2497b7ac39e9661f9863bf4f873) )
	ROM_LOAD16_BYTE( "4rd-simm5.6", 0x2c00000, 0x200000, CRC(e9b61a56) SHA1(9277ac5da4715aa5325ae9f8c055f1e826b4ce54) )
	ROM_LOAD16_BYTE( "4rd-simm5.7", 0x2c00001, 0x200000, CRC(8db3a249) SHA1(db38963ce011535a95f11426c17fb3eeda300917) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.0", 0x3000000, 0x200000, CRC(8da69042) SHA1(fd3d08295342635b2136e48d543c9350d287bb22) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.1", 0x3000001, 0x200000, CRC(1c8c7ac4) SHA1(ac9f8353a4c356ef98aa7c226baba00b01f5a80f) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.2", 0x3400000, 0x200000, CRC(a671341d) SHA1(636f4c04962bc1e1ddb29d2e01244b00389b234f) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.3", 0x3400001, 0x200000, CRC(1a990249) SHA1(2acc639e2c0c53bf24096b8620eab090bc25d03b) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.4", 0x3800000, 0x200000, CRC(20cb39ac) SHA1(7d13a0fea1ef719dd2ff77dfb547d53c6023cc9e) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.5", 0x3800001, 0x200000, CRC(5f844b2f) SHA1(564e4934f89ed3b92a4c4874519f8f00f3b48696) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.6", 0x3c00000, 0x200000, CRC(450e8d28) SHA1(885db658132aa27926df617ec2d2a1f38abdbb60) )
	ROM_LOAD16_BYTE( "sfiii3-simm6.7", 0x3c00001, 0x200000, CRC(cc5f4187) SHA1(248ddace21ed4736a56e92f77cc6ad219d7fef0b) )
ROM_END


GAME( 1996, redearth,   0,        cps3, cps3, redearthcd, ROT0, "Capcom", "Red Earth (Europe 961121)" )
GAME( 1996, warzard,    redearth, cps3, cps3, redearthcd, ROT0, "Capcom", "Warzard (Japan 961121)" )
GAME( 1996, redearthn,  redearth, cps3, cps3, redearth, ROT0, "Capcom", "Red Earth (Asia 961121, NO CD)" )
GAME( 1996, redearthr1, redearth, cps3, cps3, redearthcd, ROT0, "Capcom", "Red Earth (Europe 961023)" )
GAME( 1996, warzardr1,  redearth, cps3, cps3, redearthcd, ROT0, "Capcom", "Warzard (Japan 961023)" )
GAME( 1996, redearthnr1, redearth, cps3, cps3, redearth, ROT0, "Capcom", "Red Earth (Asia 961023, NO CD)" )
GAME( 1997, sfiii,      0,        cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (Europe 970204)" )
GAME( 1997, sfiiiu,     sfiii,    cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (USA 970204)" )
GAME( 1997, sfiiia,     sfiii,    cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (Asia 970204)" )
GAME( 1997, sfiiij,     sfiii,    cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (Japan 970204)" )
GAME( 1997, sfiiih,     sfiii,    cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (Hispanic 970204)" )
GAME( 1997, sfiiin,     sfiii,    cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (Asia 970204, NO CD, BIOS set 1)" )
GAME( 1997, sfiiina,    sfiii,    cps3, cps3, sfiii,   ROT0, "Capcom", "Street Fighter III: New Generation (Asia 970204, NO CD, BIOS set 2)" )
GAME( 1997, sfiii2,     0,        cps3, sfiii2, sfiii2,  ROT0, "Capcom", "Street Fighter III 2nd Impact: Giant Attack (USA 970930)" )
GAME( 1997, sfiii2j,    sfiii2,   cps3, sfiii2, sfiii2,  ROT0, "Capcom", "Street Fighter III 2nd Impact: Giant Attack (Japan 970930)" )
GAME( 1997, sfiii2h,    sfiii2,   cps3, sfiii2, sfiii2,  ROT0, "Capcom", "Street Fighter III 2nd Impact: Giant Attack (Hispanic 970930)" )
GAME( 1997, sfiii2a,    sfiii2,   cps3, sfiii2, sfiii2,  ROT0, "Capcom", "Street Fighter III 2nd Impact: Giant Attack (Asia 970930)" )
GAME( 1997, sfiii2n,    sfiii2,   cps3, sfiii2, sfiii2,  ROT0, "Capcom", "Street Fighter III 2nd Impact: Giant Attack (Asia 970930, NO CD)" )
GAME( 1998, jojo,       0,        cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (Europe 990128)" )
GAME( 1998, jojou,      jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (USA 990128)" )
GAME( 1998, jojoa,      jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (Asia 990128)" )
GAME( 1998, jojoj,      jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo no Kimyou na Bouken (Japan 990128)" )
GAME( 1998, jojon,      jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (Asia 990128, NO CD)" )
GAME( 1998, jojor1,     jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (Europe 990108)" )
GAME( 1998, jojour1,    jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (USA 990108)" )
GAME( 1998, jojoar1,    jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (Asia 990108)" )
GAME( 1998, jojojr1,    jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo no Kimyou na Bouken (Japan 990108)" )
GAME( 1998, jojonr1,    jojo,     cps3, cps3, jojo,    ROT0, "Capcom", "JoJo's Venture (Asia 990108, NO CD)" )
GAME( 1998, jojor2,     jojo,     cps3, cps3, jojor2,  ROT0, "Capcom", "JoJo's Venture (Europe 981202)" )
GAME( 1998, jojour2,    jojo,     cps3, cps3, jojor2,  ROT0, "Capcom", "JoJo's Venture (USA 981202)" )
GAME( 1998, jojoar2,    jojo,     cps3, cps3, jojor2,  ROT0, "Capcom", "JoJo's Venture (Asia 981202)" )
GAME( 1998, jojojr2,    jojo,     cps3, cps3, jojor2,  ROT0, "Capcom", "JoJo no Kimyou na Bouken (Japan 981202)" )
GAME( 1998, jojonr2,    jojo,     cps3, cps3, jojor2,  ROT0, "Capcom", "JoJo's Venture (Asia 981202, NO CD)" )
GAME( 1999, sfiii3,     0,        cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Europe 990608)" )
GAME( 1999, sfiii3u,    sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (USA 990608)" )
GAME( 1999, sfiii3j,    sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Japan 990608)" )
GAME( 1999, sfiii3n,    sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Japan 990608, NO CD)" )
GAME( 1999, sfiii3na,   sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Asia 990608, NO CD)" )
GAME( 1999, sfiii3r1,   sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Europe 990512)" )
GAME( 1999, sfiii3ur1,  sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (USA 990512)" )
GAME( 1999, sfiii3jr1,  sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Japan 990512)" )
GAME( 1999, sfiii3nr1,  sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Japan 990512, NO CD)" )
GAME( 1999, sfiii3nar1, sfiii3,   cps3, cps3, sfiii3,  ROT0, "Capcom", "Street Fighter III 3rd Strike: Fight for the Future (Asia 990512, NO CD)" )
GAME( 1999, jojoba,     0,        cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo's Bizarre Adventure (Europe 991015, NO CD)" )
GAME( 1999, jojoban,    jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 991015, NO CD)" )
GAME( 1999, jojobar1,   jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo's Bizarre Adventure (Europe 990927)" )
GAME( 1999, jojobajr1,  jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990927)" )
GAME( 1999, jojobanr1,  jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990927, NO CD)" )
GAME( 1999, jojobaner1, jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo's Bizarre Adventure (Europe 990927, NO CD)" )
GAME( 1999, jojobar2,   jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo's Bizarre Adventure (Europe 990913)" )
GAME( 1999, jojobajr2,  jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990913)" )
GAME( 1999, jojobanr2,  jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990913, NO CD)" )
GAME( 1999, jojobaner2, jojoba,   cps3, cps3, jojoba,  ROT0, "Capcom", "JoJo's Bizarre Adventure (Europe 990913, NO CD)" )
GAME( 2013, sfiii4n,    sfiii3,   cps3, cps3, sfiii3,  ROT0, "hack", "Street Fighter III 3rd Strike: Fight for the Future (4rd Arrange Edition 2013)" )
