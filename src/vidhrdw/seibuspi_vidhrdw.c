/***************************************************************************

  Seibu SPI hardware: the video

  Backported from current MAME src/mame/seibu/seibuspi_v.cpp,
  sei25x_rise1x_spr.cpp and seibu_crtc.cpp (Ville Linde, hap, Nicola
  Salmoria, Angelo Salese).

  The main CPU builds the tilemaps, the palette and the sprite list in its
  RAM, and copies them to the video chip by DMA. Four layers: back, middle
  and fore (16x16 tiles, 6bpp, 512x512) and text (8x8, 5bpp, 512x256),
  the back, middle and fore ones with an optional scroll per line; the
  sprites (16x16, 6bpp, up to 8x8 tiles) have four priorities between the
  layers. Some pens are blended half and half with what is under them.

  As in MAME the screen is built by hand in 32 bits: the sprites are drawn
  in a 16-bit buffer first (pen and priority), then the layers and the
  sprites of each priority go to the screen in order, through the alpha
  table. The graphics are decoded here to one byte per pixel, from the
  regions the driver decrypted; the regions are dropped afterwards.

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "includes/seibuspi.h"

#define SPI_SCREEN_W	320
#define SPI_SCREEN_H	240

data32_t *spi_mainram;

static data32_t spi_tilemap_ram[0x4000 / 4];
static data32_t spi_palette_ram[0x3000 / 4];
static data16_t spi_sprite_ram[0x1000 / 2];
static UINT16 spi_crtc_ram[0x40 / 2];
static UINT32 spi_pens[6144];
static UINT8 alpha_table[0x2000];
static UINT8 alpha_color[0x2000 / 32];	/* a pen of these 32 is blended */

static data32_t video_dma_length;
static data32_t video_dma_address;
static UINT16 layer_enable;
static UINT16 layer_bank;
static UINT8 rf2_layer_bank;
static UINT8 rowscroll_enable;
static UINT16 scrollram[6];

static int midl_layer_offset, fore_layer_offset, text_layer_offset;
static int fore_layer_d13, back_layer_d14, midl_layer_d14, fore_layer_d14;
static int bg_fore_layer_position;

/* the graphics, one byte per pixel */
static UINT8 *tile_gfx, *tile_flags;
static int tile_count;
static UINT8 *char_gfx, *char_flags;
static int char_count;
static UINT8 *sprite_gfx, *sprite_flags;
static int sprite_count;

#define GFX_TRANSPARENT	1	/* all the pixels are the transparent pen */
#define GFX_OPAQUE		2	/* none is */

/* the sprites: pen (color << 6 | pixel) and priority << 14, 0xffff for none; */
/* for each line the priorities drawn in it and the span of x, to skip the rest */
static UINT16 sprite_bitmap[SPI_SCREEN_H][SPI_SCREEN_W];
static UINT8 sprite_row_pri[SPI_SCREEN_H];
static INT16 sprite_row_minx[SPI_SCREEN_H], sprite_row_maxx[SPI_SCREEN_H];


/*****************************************************************************/

static void set_layer_offsets(void)
{
	if (rowscroll_enable)
	{
		fore_layer_offset = 0x1000 / 4;
		midl_layer_offset = 0x2000 / 4;
		text_layer_offset = 0x3000 / 4;
	}
	else
	{
		fore_layer_offset = 0x1000 / 4 / 2;
		midl_layer_offset = 0x2000 / 4 / 2;
		text_layer_offset = 0x3000 / 4 / 2;
	}

	fore_layer_d13 = layer_bank << 2 & 0x2000;
	back_layer_d14 = rf2_layer_bank << 14 & 0x4000;
	midl_layer_d14 = rf2_layer_bank << 13 & 0x4000;
	fore_layer_d14 = rf2_layer_bank << 12 & 0x4000;
}

static void set_pen(int pen, UINT32 data)
{
	int r = data & 0x1f, g = (data >> 5) & 0x1f, b = (data >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);
	spi_pens[pen] = (r << 16) | (g << 8) | b;
}

void spi_rf2_layer_bank_w(UINT8 data)
{
	/* 00000fmb */
	/* f: fore layer d14 */
	/* m: middle layer d14 */
	/* b: back layer d14 */
	rf2_layer_bank = data;
	set_layer_offsets();
}


/*****************************************************************************

  The CRTC (0x400-0x43f): 16-bit registers, two per dword. The registers
  with a callback in MAME read back 0 (their RAM is never written), but
  0x1a (the layer bank, the game reads it).

*****************************************************************************/

static void crtc_reg_w(int reg, UINT16 data)
{
	switch (reg)
	{
	case 0x14 / 2:	/* decryption key: the tiles are decrypted at load */
		if (data != 0 && data != 1)
			logerror("SPI: decryption key %04X\n", data);
		break;

	case 0x1a / 2:	/* r000f000 0010100a, r: rowscroll enable, f: fore layer d13 */
		layer_bank = data;
		rowscroll_enable = (layer_bank >> 15) & 1;
		set_layer_offsets();
		break;

	case 0x1c / 2:	/* 00000000 000stfmb (0=on, 1=off): sprites, text, fore, middle, back */
		layer_enable = data;
		break;

	case 0x20 / 2: case 0x22 / 2: case 0x24 / 2:
	case 0x26 / 2: case 0x28 / 2: case 0x2a / 2:
		scrollram[reg - 0x20 / 2] = data;
		break;

	case 0x2c / 2: case 0x2e / 2: case 0x30 / 2: case 0x32 / 2:
	case 0x34 / 2: case 0x36 / 2: case 0x38 / 2: case 0x3a / 2:
		break;	/* layer scroll base, unused */

	default:
		spi_crtc_ram[reg] = data;
		break;
	}
}

READ32_HANDLER( spi_crtc_r )
{
	int reg = offset * 2;
	UINT16 lo = (reg == 0x1a / 2) ? layer_bank : spi_crtc_ram[reg];
	UINT16 hi = (reg + 1 == 0x1a / 2) ? layer_bank : spi_crtc_ram[reg + 1];

	return lo | (hi << 16);
}

WRITE32_HANDLER( spi_crtc_w )
{
	int half;

	for (half = 0; half < 2; half++)
	{
		int reg = offset * 2 + half;
		UINT16 mask = (mem_mask >> (16 * half)) & 0xffff;
		UINT16 old, d;

		if (mask == 0xffff)
			continue;
		if (reg == 0x1a / 2)
			old = layer_bank;
		else if (reg == 0x1c / 2)
			old = layer_enable;
		else if (reg >= 0x20 / 2 && reg <= 0x2a / 2)
			old = scrollram[reg - 0x20 / 2];
		else
			old = spi_crtc_ram[reg];
		d = (old & mask) | (((data >> (16 * half)) & 0xffff) & ~mask);
		crtc_reg_w(reg, d);
	}
}


/*****************************************************************************

  The DMAs from the main RAM

*****************************************************************************/

WRITE32_HANDLER( spi_video_dma_length_w )
{
	COMBINE_DATA(&video_dma_length);
}

WRITE32_HANDLER( spi_video_dma_address_w )
{
	COMBINE_DATA(&video_dma_address);
}

WRITE32_HANDLER( spi_tilemap_dma_start_w )
{
	int index = video_dma_address / 4;
	int i;

	/* back layer */
	for (i = 0; i < 0x800 / 4; i++)
		spi_tilemap_ram[i] = spi_mainram[index++ & 0xffff];

	/* back layer row scroll */
	if (rowscroll_enable)
		for (i = 0; i < 0x800 / 4; i++)
			spi_tilemap_ram[0x800 / 4 + i] = spi_mainram[index++ & 0xffff];

	/* fore layer */
	for (i = 0; i < 0x800 / 4; i++)
		spi_tilemap_ram[i + fore_layer_offset] = spi_mainram[index++ & 0xffff];

	/* fore layer row scroll */
	if (rowscroll_enable)
		for (i = 0; i < 0x800 / 4; i++)
			spi_tilemap_ram[0x2800 / 4 + i] = spi_mainram[index++ & 0xffff];

	/* middle layer */
	for (i = 0; i < 0x800 / 4; i++)
		spi_tilemap_ram[i + midl_layer_offset] = spi_mainram[index++ & 0xffff];

	/* middle layer row scroll */
	if (rowscroll_enable)
		for (i = 0; i < 0x800 / 4; i++)
			spi_tilemap_ram[0x1800 / 4 + i] = spi_mainram[index++ & 0xffff];

	/* text layer */
	for (i = 0; i < 0x1000 / 4; i++)
		spi_tilemap_ram[i + text_layer_offset] = spi_mainram[index++ & 0xffff];
}

WRITE32_HANDLER( spi_palette_dma_start_w )
{
	int dma_length = (video_dma_length + 1) * 2;
	int i;

	if (dma_length > 0x3000)
		dma_length = 0x3000;
	for (i = 0; i < dma_length / 4; i++)
	{
		UINT32 color = spi_mainram[(video_dma_address / 4 + i) & 0xffff];
		if (spi_palette_ram[i] != color)
		{
			spi_palette_ram[i] = color;
			set_pen((i * 2), color);
			set_pen((i * 2) + 1, color >> 16);
		}
	}
}

void spi_sprite_dma_start(void)
{
	int i;

	for (i = 0; i < 0x1000 / 4; i++)
	{
		UINT32 d = spi_mainram[((video_dma_address / 4) + i) & 0xffff];
		spi_sprite_ram[i << 1] = d & 0xffff;
		spi_sprite_ram[(i << 1) | 1] = (d >> 16) & 0xffff;
	}
}


/*****************************************************************************

  The graphics decoding, to one byte per pixel (the layouts of MAME)

*****************************************************************************/

/* 16x16 tiles, 6 planes { 0, 4, 8, 12, 16, 20 }, x STEP4(3,-1) STEP4(27,-1) */
/* STEP4(51,-1) STEP4(75,-1), y STEP16(0,96): four groups of 24 bits a row, */
/* each four pixels; 8x8 chars, 5 planes { 4, 8, 12, 16, 20 }, two groups */
static void decode_group24(const UINT8 *src, UINT8 *dest, int planes)
{
	UINT32 w = (src[0] << 16) | (src[1] << 8) | src[2];
	int i, p;

	for (i = 0; i < 4; i++)
	{
		int pix = 0;
		for (p = 0; p < planes; p++)
		{
			/* plane p at bit offset 4 * (p + 6 - planes), pixel i at offset 3 - i */
			int bit = 23 - (4 * (p + 6 - planes) + 3 - i);
			pix |= ((w >> bit) & 1) << (planes - 1 - p);
		}
		dest[i] = pix;
	}
}

static void decode_tiles(const UINT8 *src, int count, int size, int planes, UINT8 *gfx, UINT8 *flags)
{
	int n, y, g, i;
	int bytes_per_row = size / 4 * 3;		/* 12 for tiles, 6 for chars */
	int transpen = (1 << planes) - 1;

	for (n = 0; n < count; n++)
	{
		UINT8 *dest = gfx + n * size * size;
		int opaque = 1, transparent = 1;

		for (y = 0; y < size; y++)
			for (g = 0; g < size / 4; g++)
				decode_group24(src + n * size * bytes_per_row + y * bytes_per_row + g * 3, dest + y * size + g * 4, planes);
		for (i = 0; i < size * size; i++)
		{
			if (dest[i] == transpen)
				opaque = 0;
			else
				transparent = 0;
		}
		flags[n] = (transparent ? GFX_TRANSPARENT : 0) | (opaque ? GFX_OPAQUE : 0);
	}
}

/* 16x16 sprites, the region in thirds, planes { 0, 8, F/3, F/3+8, 2F/3, 2F/3+8 }, */
/* x STEP8(7,-1) STEP8(23,-1), y STEP16(0,32) */
static void decode_sprites(const UINT8 *src, int third, int count, UINT8 *gfx)
{
	int n, y, x;

	for (n = 0; n < count; n++)
	{
		for (y = 0; y < 16; y++)
		{
			const UINT8 *s0 = src + n * 64 + y * 4;
			const UINT8 *s1 = s0 + third;
			const UINT8 *s2 = s1 + third;
			UINT8 *dest = gfx + n * 256 + y * 16;

			for (x = 0; x < 16; x++)
			{
				int o = (x < 8) ? 0 : 2;
				int b = x & 7;
				dest[x] = (((s0[o] >> b) & 1) << 5) | (((s0[o + 1] >> b) & 1) << 4) |
						(((s1[o] >> b) & 1) << 3) | (((s1[o + 1] >> b) & 1) << 2) |
						(((s2[o] >> b) & 1) << 1) | ((s2[o + 1] >> b) & 1);
			}
		}
	}
}


/*****************************************************************************/

static void spi_postload(void)
{
	int i;

	for (i = 0; i < 0x3000 / 4; i++)
	{
		set_pen((i * 2), spi_palette_ram[i]);
		set_pen((i * 2) + 1, spi_palette_ram[i] >> 16);
	}
	set_layer_offsets();
}

VIDEO_START( spi )
{
	int region_length = memory_region_length(REGION_GFX2);

	video_dma_length = 0;
	video_dma_address = 0;
	layer_enable = 0;
	layer_bank = 0;
	rf2_layer_bank = 0;
	rowscroll_enable = 0;
	memset(scrollram, 0, sizeof(scrollram));
	memset(spi_crtc_ram, 0, sizeof(spi_crtc_ram));
	memset(spi_tilemap_ram, 0, sizeof(spi_tilemap_ram));
	memset(spi_palette_ram, 0, sizeof(spi_palette_ram));
	memset(spi_sprite_ram, 0, sizeof(spi_sprite_ram));
	memset(spi_pens, 0, sizeof(spi_pens));
	memset(sprite_bitmap, 0xff, sizeof(sprite_bitmap));
	memset(sprite_row_pri, 0, sizeof(sprite_row_pri));
	set_layer_offsets();

	if (region_length <= 0x300000)
		bg_fore_layer_position = 0x2000;
	else if (region_length <= 0x600000)
		bg_fore_layer_position = 0x4000;
	else
		bg_fore_layer_position = 0x8000;

	/* the graphics */
	tile_count = region_length / 192;
	tile_gfx = auto_malloc(tile_count * 256);
	tile_flags = auto_malloc(tile_count);
	char_count = memory_region_length(REGION_GFX1) / 48;
	char_gfx = auto_malloc(char_count * 64);
	char_flags = auto_malloc(char_count);
	sprite_count = memory_region_length(REGION_GFX3) / 3 / 64;
	sprite_gfx = auto_malloc(sprite_count * 256);
	sprite_flags = auto_malloc(sprite_count);
	if (!tile_gfx || !tile_flags || !char_gfx || !char_flags || !sprite_gfx || !sprite_flags)
		return 1;
	decode_tiles(memory_region(REGION_GFX2), tile_count, 16, 6, tile_gfx, tile_flags);
	decode_tiles(memory_region(REGION_GFX1), char_count, 8, 5, char_gfx, char_flags);
	decode_sprites(memory_region(REGION_GFX3), memory_region_length(REGION_GFX3) / 3, sprite_count, sprite_gfx);
	{
		int n, i;
		for (n = 0; n < sprite_count; n++)
		{
			const UINT8 *g = sprite_gfx + n * 256;
			int opaque = 1, transparent = 1;
			for (i = 0; i < 256; i++)
			{
				if (g[i] == 63)
					opaque = 0;
				else
					transparent = 0;
			}
			sprite_flags[n] = (transparent ? GFX_TRANSPARENT : 0) | (opaque ? GFX_OPAQUE : 0);
		}
	}

	/* TODO : Differs per games? */
	/* alpha blending (preliminary) */
	memset(alpha_table, 0, sizeof(alpha_table));

	/* sprites(0000-0fff): */
	/*memset(alpha_table + 0x700, 1, 0x10);   breaks rdft */
	memset(alpha_table + 0x730, 1, 0x10);
	memset(alpha_table + 0x780, 1, 0x20);
	/*memset(alpha_table + 0x7c0, 1, 0x40);   breaks batlball */
	/*memset(alpha_table + 0xf00, 1, 0x40);   breaks rdft */
	memset(alpha_table + 0xfc0, 1, 0x40);

	/* back layer(1000-11ff): nope */
	/* fore layer(1200-13ff): */
	memset(alpha_table + 0x1200 + 0x160, 1, 0x20);
	memset(alpha_table + 0x1200 + 0x1b0, 1, 0x10);
	memset(alpha_table + 0x1200 + 0x1f0, 1, 0x10);
	/* midl layer(1400-15ff) */
	memset(alpha_table + 0x1400 + 0x1b0, 1, 0x10);
	memset(alpha_table + 0x1400 + 0x1f0, 1, 0x10);
	/* text layer(1600-17ff) */
	memset(alpha_table + 0x1600 + 0x170, 1, 0x10);
	memset(alpha_table + 0x1600 + 0x1f0, 1, 0x10);
	{
		int i;
		memset(alpha_color, 0, sizeof(alpha_color));
		for (i = 0; i < 0x2000; i++)
			if (alpha_table[i])
				alpha_color[i >> 5] = 1;
	}

	state_save_register_UINT32("spi_video", 0, "tilemap_ram", spi_tilemap_ram, 0x4000 / 4);
	state_save_register_UINT32("spi_video", 0, "palette_ram", spi_palette_ram, 0x3000 / 4);
	state_save_register_UINT16("spi_video", 0, "sprite_ram", spi_sprite_ram, 0x1000 / 2);
	state_save_register_UINT16("spi_video", 0, "crtc_ram", spi_crtc_ram, 0x40 / 2);
	state_save_register_UINT32("spi_video", 0, "video_dma_length", &video_dma_length, 1);
	state_save_register_UINT32("spi_video", 0, "video_dma_address", &video_dma_address, 1);
	state_save_register_UINT16("spi_video", 0, "layer_enable", &layer_enable, 1);
	state_save_register_UINT16("spi_video", 0, "layer_bank", &layer_bank, 1);
	state_save_register_UINT8("spi_video", 0, "rf2_layer_bank", &rf2_layer_bank, 1);
	state_save_register_UINT8("spi_video", 0, "rowscroll_enable", &rowscroll_enable, 1);
	state_save_register_UINT16("spi_video", 0, "scrollram", scrollram, 6);
	state_save_register_func_postload(spi_postload);

	return 0;
}


/*****************************************************************************

  The screen

*****************************************************************************/

static INLINE UINT32 spi_alpha_blend(UINT32 d, UINT32 s)
{
	/* level 0x7f, as MAME */
	return ((((s & 0x0000ff) * 0x7f + (d & 0x0000ff) * 0x81) >> 8)) |
			((((s & 0x00ff00) * 0x7f + (d & 0x00ff00) * 0x81) >> 8) & 0x00ff00) |
			((((s & 0xff0000) * 0x7f + (d & 0xff0000) * 0x81) >> 8) & 0xff0000);
}

static INLINE void blend_pixel(UINT32 *dest, int pen)
{
	if (alpha_table[pen])
		*dest = spi_alpha_blend(*dest, spi_pens[pen]);
	else
		*dest = spi_pens[pen];
}

/* the sprites in the raw buffer, the last one first (the first one on top) */
static void draw_sprites(void)
{
	int i, ax, ay, x, y;

	/* clear what the frame before drew */
	for (y = 0; y < SPI_SCREEN_H; y++)
	{
		if (sprite_row_pri[y])
			for (x = sprite_row_minx[y]; x <= sprite_row_maxx[y]; x++)
				sprite_bitmap[y][x] = 0xffff;
		sprite_row_pri[y] = 0;
		sprite_row_minx[y] = SPI_SCREEN_W;
		sprite_row_maxx[y] = -1;
	}

	for (i = 0x1000 / 2 - 4; i >= 0; i -= 4)
	{
		const UINT16 *spr = &spi_sprite_ram[i];
		UINT32 code = spr[1];
		int flipy, sizey, flipx, sizex, pri, color, ext, sx0, sy0;

		if ((code % sprite_count) == 0)
			continue;

		flipy = (spr[0] >> 15) & 1;
		sizey = ((spr[0] >> 12) & 7) + 1;
		flipx = (spr[0] >> 11) & 1;
		sizex = ((spr[0] >> 8) & 7) + 1;
		pri = (spr[0] >> 6) & 3;
		color = spr[0] & 0x3f;
		ext = (spr[2] >> 12) & 1;
		sx0 = spr[2] & 0x1ff;
		sy0 = spr[3] & 0x1ff;

		if (sx0 >= 0x180) sx0 -= 0x200;
		if (sy0 >= 0x180) sy0 -= 0x200;

		color = (color << 6) | (pri << 14);
		if (sprite_count > 0x10000 && ext)
			code |= 0x10000;

		for (ax = 0; ax < sizex; ax++)
		{
			int sx = flipx ? (sx0 + 16 * (sizex - ax - 1)) : (sx0 + 16 * ax);
			for (ay = 0; ay < sizey; ay++)
			{
				int sy = flipy ? (sy0 + 16 * (sizey - ay - 1)) : (sy0 + 16 * ay);
				int c = code % sprite_count;
				const UINT8 *gfx = sprite_gfx + c * 256;
				int x0 = 0, x1 = 16, y0 = 0, y1 = 16;

				code++;
				if (sx >= SPI_SCREEN_W || sy >= SPI_SCREEN_H || sx <= -16 || sy <= -16)
					continue;
				if (sprite_flags[c] & GFX_TRANSPARENT)
					continue;
				if (sx < 0) x0 = -sx;
				if (sx + 16 > SPI_SCREEN_W) x1 = SPI_SCREEN_W - sx;
				if (sy < 0) y0 = -sy;
				if (sy + 16 > SPI_SCREEN_H) y1 = SPI_SCREEN_H - sy;

				for (y = y0; y < y1; y++)
				{
					const UINT8 *src = gfx + (flipy ? 15 - y : y) * 16;
					UINT16 *dest = &sprite_bitmap[sy + y][sx];
					sprite_row_pri[sy + y] |= 1 << pri;
					if (sx + x0 < sprite_row_minx[sy + y]) sprite_row_minx[sy + y] = sx + x0;
					if (sx + x1 - 1 > sprite_row_maxx[sy + y]) sprite_row_maxx[sy + y] = sx + x1 - 1;
					if (flipx)
					{
						const UINT8 *s = src + 15;
						for (x = x0; x < x1; x++)
						{
							int pix = s[-x];
							if (pix != 63)
								dest[x] = color + pix;
						}
					}
					else
					{
						for (x = x0; x < x1; x++)
						{
							int pix = src[x];
							if (pix != 63)
								dest[x] = color + pix;
						}
					}
				}
			}
		}
	}
}

static void blend_sprite(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int pri)
{
	int x, y;

	if (layer_enable & 0x10)
		return;

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const UINT16 *src = sprite_bitmap[y];
		UINT32 *dest = (UINT32 *)bitmap->line[y];
		int x0 = sprite_row_minx[y], x1 = sprite_row_maxx[y];

		if (!(sprite_row_pri[y] & (1 << pri)))
			continue;
		if (x0 < cliprect->min_x) x0 = cliprect->min_x;
		if (x1 > cliprect->max_x) x1 = cliprect->max_x;
		for (x = x0; x <= x1; x++)
		{
			int pen = src[x];
			if (((pen & 0xc000) == (pri << 14)) && (pen != 0xffff))
				blend_pixel(&dest[x], pen & ~0xc000);
		}
	}
}

enum { LAYER_BACK = 0, LAYER_MIDL, LAYER_FORE };

/* a 16x16 layer, 32x32 tiles in columns */
/* sprites_only: only where the priority 0 sprites are (the back layer */
/* drawn again over them; elsewhere it would draw the same pixels again) */
static void combine_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int layer, int sx, int sy, int opaque, const data32_t *rowscroll, int sprites_only)
{
	int x, y, maxx;
	int offset, colorbase, codeor;

	switch (layer)
	{
	default:
	case LAYER_BACK:
		offset = 0;
		colorbase = 4096;
		codeor = back_layer_d14;
		break;
	case LAYER_MIDL:
		offset = midl_layer_offset;
		colorbase = 4096 + 16 * 64;
		codeor = 0x2000 | midl_layer_d14;
		break;
	case LAYER_FORE:
		offset = fore_layer_offset;
		colorbase = 4096 + 8 * 64;
		codeor = bg_fore_layer_position | fore_layer_d13 | fore_layer_d14;
		break;
	}

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT32 *dest = (UINT32 *)bitmap->line[y];
		int rx = sx;
		int my = (y + sy) & 511;
		int row = my >> 4;
		int line = (my & 15) * 16;

		if (rowscroll)
		{
			/* the adder probably comes from the CRTC */
			int r = (y + 19) & 511;
			rx += (INT16)(rowscroll[r >> 1] >> ((r & 1) * 16));
		}

		x = cliprect->min_x;
		maxx = cliprect->max_x;
		if (sprites_only)
		{
			if (!(sprite_row_pri[y] & 1))
				continue;
			if (sprite_row_minx[y] > x) x = sprite_row_minx[y];
			if (sprite_row_maxx[y] < maxx) maxx = sprite_row_maxx[y];
		}
		while (x <= maxx)
		{
			int mx = (x + rx) & 511;
			int tile_index = (mx >> 4) * 32 + row;
			UINT32 tile = (spi_tilemap_ram[(tile_index >> 1) + offset] >> ((tile_index & 1) * 16)) & 0xffff;
			int code = (tile & 0x1fff) | codeor;
			int pens = colorbase + ((tile >> 13) & 7) * 64;
			const UINT8 *src;
			int span = 16 - (mx & 15);
			int i;

			if (code >= tile_count)
				code %= tile_count;
			src = tile_gfx + code * 256 + line;

			if (x + span > maxx + 1)
				span = maxx + 1 - x;

			/* the span stays in the tile: its pixels from mx & 15 on */
			src += mx & 15;
			if (!opaque && (tile_flags[code] & GFX_TRANSPARENT))
				;	/* nothing to draw */
			else if (!(alpha_color[pens >> 5] | alpha_color[(pens >> 5) + 1]))
			{
				/* nothing blended: the pens straight */
				const UINT32 *pal = &spi_pens[pens];
				UINT32 *d = &dest[x];
				if (opaque || (tile_flags[code] & GFX_OPAQUE))
				{
					for (i = 0; i < span; i++)
						d[i] = pal[src[i]];
				}
				else
				{
					for (i = 0; i < span; i++)
					{
						int pix = src[i];
						if (pix != 63)
							d[i] = pal[pix];
					}
				}
			}
			else
			{
				for (i = 0; i < span; i++)
				{
					int pix = src[i];
					if (opaque || pix != 63)
						blend_pixel(&dest[x + i], pens + pix);
				}
			}
			x += span;
		}
	}
}

/* the text layer, 8x8, 64x32 tiles in rows, no scroll */
static void combine_text(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int x, y;

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT32 *dest = (UINT32 *)bitmap->line[y];
		int my = y & 255;
		int row = my >> 3;
		int line = (my & 7) * 8;

		x = cliprect->min_x;
		while (x <= cliprect->max_x)
		{
			int mx = x & 511;
			int tile_index = row * 64 + (mx >> 3);
			UINT32 tile = (spi_tilemap_ram[(tile_index >> 1) + text_layer_offset] >> ((tile_index & 1) * 16)) & 0xffff;
			int code = tile & 0xfff;
			int pens = 5632 + ((tile >> 12) & 0xf) * 32;
			const UINT8 *src;
			int span = 8 - (mx & 7);
			int i;

			if (code >= char_count)
				code %= char_count;
			src = char_gfx + code * 64 + line;

			if (x + span > cliprect->max_x + 1)
				span = cliprect->max_x + 1 - x;

			src += mx & 7;
			if (char_flags[code] & GFX_TRANSPARENT)
				;	/* nothing to draw */
			else if (!alpha_color[pens >> 5])
			{
				const UINT32 *pal = &spi_pens[pens];
				UINT32 *d = &dest[x];
				for (i = 0; i < span; i++)
				{
					int pix = src[i];
					if (pix != 31)
						d[i] = pal[pix];
				}
			}
			else
			{
				for (i = 0; i < span; i++)
				{
					int pix = src[i];
					if (pix != 31)
						blend_pixel(&dest[x + i], pens + pix);
				}
			}
			x += span;
		}
	}
}

VIDEO_UPDATE( spi )
{
	const data32_t *back_rowscroll, *midl_rowscroll, *fore_rowscroll;

	if (rowscroll_enable)
	{
		back_rowscroll = &spi_tilemap_ram[0x200];
		midl_rowscroll = &spi_tilemap_ram[0x600];
		fore_rowscroll = &spi_tilemap_ram[0xa00];
	}
	else
	{
		back_rowscroll = NULL;
		midl_rowscroll = NULL;
		fore_rowscroll = NULL;
	}

	if (!(layer_enable & 0x10))
		draw_sprites();

	if (layer_enable & 1)
		fillbitmap(bitmap, 0, cliprect);
	else
		combine_layer(bitmap, cliprect, LAYER_BACK, scrollram[0], scrollram[1], 1, back_rowscroll, 0);

	blend_sprite(bitmap, cliprect, 0);

	/* if fore layer is enabled, draw priority 0 sprites behind back layer */
	if ((layer_enable & 0x15) == 0)
		combine_layer(bitmap, cliprect, LAYER_BACK, scrollram[0], scrollram[1], 0, back_rowscroll, 1);

	/* if fore layer is enabled, draw priority 1 sprites behind middle layer */
	if (~layer_enable & 4)
		blend_sprite(bitmap, cliprect, 1);

	if (~layer_enable & 2)
		combine_layer(bitmap, cliprect, LAYER_MIDL, scrollram[2], scrollram[3], 0, midl_rowscroll, 0);

	/* if fore layer is disabled, draw priority 1 sprites above middle layer */
	if (layer_enable & 4)
		blend_sprite(bitmap, cliprect, 1);

	blend_sprite(bitmap, cliprect, 2);

	if (~layer_enable & 4)
		combine_layer(bitmap, cliprect, LAYER_FORE, scrollram[4], scrollram[5], 0, fore_rowscroll, 0);

	blend_sprite(bitmap, cliprect, 3);

	if (~layer_enable & 8)
		combine_text(bitmap, cliprect);
}
