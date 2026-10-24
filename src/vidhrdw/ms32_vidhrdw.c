/* Jaleco MegaSystem 32 Video Hardware

   Ported from current MAME (ms32_v.cpp, ms32_sprite.cpp: David Haywood,
   Paul Priest, Luca Elia, Andrea Bogazzi).

   The layers are drawn into palette index buffers and mixed with the
   priority RAM like current MAME, then written as direct colour.

   Layers:
   - sprites: 256x256 8bpp pages, zoom, the first sprite drawn wins
   - BG: 16x16 8bpp tiles, 64x64 tiles (or 256x16 with bgmode bit 0)
   - ROZ: 16x16 8bpp tiles, 128x128 tiles, per-line ("super") or whole
     screen rotation and zoom
   - ASCII (text): 8x8 8bpp tiles, 64x64 tiles
*/

#include "driver.h"
#include "state.h"
#include "ms32.h"

static UINT8  priram_mem[0x2000];
static UINT16 palram_mem[0x10000];
static UINT16 rozram_mem[0x8000];
static UINT16 lineram_mem[0x800];
static UINT16 sprram_mem[0x8000];
static UINT16 txram_mem[0x2000];
static UINT16 bgram_mem[0x2000];
static UINT16 road_vram_mem[0x8000];
static UINT16 road_lineram_mem[0x8000];

UINT8  *ms32_priram  = priram_mem;
UINT16 *ms32_palram  = palram_mem;
UINT16 *ms32_rozram  = rozram_mem;
UINT16 *ms32_lineram = lineram_mem;
UINT16 *ms32_sprram  = sprram_mem;
UINT16 *ms32_txram   = txram_mem;
UINT16 *ms32_bgram   = bgram_mem;
data32_t ms32_roz_ctrl[0x60/4];
data32_t ms32_tx_scroll[0x18/4];
data32_t ms32_bg_scroll[0x18/4];
data32_t ms32_sprite_ctrl[0x80/4];
data32_t ms32_tilemaplayoutcontrol;

static UINT16 *sprram_buffer;

static data32_t brt[4];
static int brt_r, brt_g, brt_b;

/* screen sized work buffers: palette indices and priority */
#define MS32_BUF_W	512
#define MS32_BUF_H	512
static UINT16 *buf_tiles;		/* tile layers, palette index */
static UINT8  *buf_tilepri;		/* OR of the layers drawn at each pixel (1 BG, 2 ROZ, 4 ASCII) */
static UINT16 *buf_spr;			/* sprites: pixel | colour << 8 | priority << 12 */
static UINT8  *buf_sprpri;		/* sprite drawn flag */
static UINT8  spr_row_used[MS32_BUF_H];	/* a sprite may have drawn on this line */

static UINT32 pens[0x8000];
int ms32_palette_dirty;

/* the gfx regions and their element counts */
static UINT8 *spr_gfx, *roz_gfx, *bg_gfx, *tx_gfx;
static UINT32 spr_count, roz_count, bg_count, tx_count;

/* F1 Super Battle road layer */
UINT16 *ms32_road_vram = road_vram_mem;
UINT16 *ms32_road_lineram = road_lineram_mem;
data32_t ms32_road_ctrl[0x60/4];
int ms32_is_f1superb;
static UINT16 *txram_latch;
static UINT16 *buf_layer_tx, *buf_layer_bg, *buf_layer_road, *buf_layer_roz;
static UINT8 *road_gfx;
static UINT32 road_count;

/* code % count, with a mask when count is a power of two */
#define TILE_NUMBER(code, count, mask)	((mask) ? ((code) & (mask)) : ((code) % (count)))
#define TILE_MASK(count)		(((count) & ((count) - 1)) ? 0 : (count) - 1)

/* for each row of each tile: all pixels transparent, all opaque or mixed */
enum { ROW_MIXED = 0, ROW_EMPTY, ROW_OPAQUE };
static UINT8 *roz_rows, *bg_rows, *tx_rows;

static UINT8 *build_row_flags(const UINT8 *gfx, UINT32 count, int tilew, int tileh)
{
	UINT32 rows = count * tileh, r;
	UINT8 *flags = auto_malloc(rows ? rows : 1);
	if (!flags)
		return NULL;
	for (r = 0; r < rows; r++)
	{
		const UINT8 *src = gfx + r * tilew;
		int x, opaque = 0;
		for (x = 0; x < tilew; x++)
			if (src[x])
				opaque++;
		flags[r] = (opaque == 0) ? ROW_EMPTY : (opaque == tilew) ? ROW_OPAQUE : ROW_MIXED;
	}
	return flags;
}

static void ms32_postload(void)
{
	ms32_palette_dirty = 1;
}

VIDEO_START( ms32 )
{
	spr_gfx = memory_region(REGION_GFX1); spr_count = memory_region_length(REGION_GFX1) / 0x10000;
	roz_gfx = memory_region(REGION_GFX2); roz_count = memory_region_length(REGION_GFX2) / (ms32_is_f1superb ? 2048 : 256);
	bg_gfx  = memory_region(REGION_GFX3); bg_count  = memory_region_length(REGION_GFX3) / 256;
	tx_gfx  = memory_region(REGION_GFX4); tx_count  = memory_region_length(REGION_GFX4) / 64;
	if (ms32_is_f1superb)
	{
		road_gfx = memory_region(REGION_GFX5); road_count = memory_region_length(REGION_GFX5) / 2048;
	}

	bg_rows = build_row_flags(bg_gfx, bg_count, 16, 16);
	tx_rows = build_row_flags(tx_gfx, tx_count, 8, 8);
	roz_rows = ms32_is_f1superb ? NULL : build_row_flags(roz_gfx, roz_count, 16, 16);
	if (!bg_rows || !tx_rows || (!ms32_is_f1superb && !roz_rows))
		return 1;

	sprram_buffer = auto_malloc(0x8000 * sizeof(UINT16));
	buf_tiles   = auto_malloc(MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
	buf_tilepri = auto_malloc(MS32_BUF_W * MS32_BUF_H);
	buf_spr     = auto_malloc(MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
	buf_sprpri  = auto_malloc(MS32_BUF_W * MS32_BUF_H);
	if (!sprram_buffer || !buf_tiles || !buf_tilepri || !buf_spr || !buf_sprpri)
		return 1;
	memset(sprram_buffer, 0, 0x8000 * sizeof(UINT16));
	memset(buf_tiles, 0, MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
	memset(buf_spr, 0, MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));

	if (ms32_is_f1superb)
	{
		txram_latch    = auto_malloc(0x2000 * sizeof(UINT16));
		buf_layer_tx   = auto_malloc(MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
		buf_layer_bg   = auto_malloc(MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
		buf_layer_road = auto_malloc(MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
		buf_layer_roz  = auto_malloc(MS32_BUF_W * MS32_BUF_H * sizeof(UINT16));
		if (!txram_latch || !buf_layer_tx || !buf_layer_bg || !buf_layer_road || !buf_layer_roz)
			return 1;
		memset(txram_latch, 0, 0x2000 * sizeof(UINT16));
	}

	/* tp2m32 doesn't set the brightness registers so we need sensible defaults */
	brt[0] = brt[1] = 0xffff;
	brt[2] = brt[3] = 0;
	brt_r = brt_g = brt_b = 0;
	ms32_palette_dirty = 1;
	ms32_sprite_ctrl[0x10/4] = 0x8000;
	ms32_tilemaplayoutcontrol = 0;

	state_save_register_UINT16("ms32", 0, "sprram_buffer", sprram_buffer, 0x8000);
	state_save_register_func_postload(ms32_postload);
	state_save_register_UINT32("ms32", 0, "brt", brt, 4);
	state_save_register_int("ms32", 0, "brt_r", &brt_r);
	state_save_register_int("ms32", 0, "brt_g", &brt_g);
	state_save_register_int("ms32", 0, "brt_b", &brt_b);
	if (ms32_is_f1superb)
		state_save_register_UINT16("ms32", 0, "txram_latch", txram_latch, 0x2000);
	return 0;
}

/* clear the video RAM at power on */
void ms32_video_init_ram(void)
{
	memset(priram_mem, 0, sizeof(priram_mem));
	memset(palram_mem, 0, sizeof(palram_mem));
	memset(rozram_mem, 0, sizeof(rozram_mem));
	memset(lineram_mem, 0, sizeof(lineram_mem));
	memset(sprram_mem, 0, sizeof(sprram_mem));
	memset(txram_mem, 0, sizeof(txram_mem));
	memset(bgram_mem, 0, sizeof(bgram_mem));
	memset(road_vram_mem, 0, sizeof(road_vram_mem));
	memset(road_lineram_mem, 0, sizeof(road_lineram_mem));
	memset(ms32_roz_ctrl, 0, sizeof(ms32_roz_ctrl));
	memset(ms32_tx_scroll, 0, sizeof(ms32_tx_scroll));
	memset(ms32_bg_scroll, 0, sizeof(ms32_bg_scroll));
	memset(ms32_sprite_ctrl, 0, sizeof(ms32_sprite_ctrl));
	memset(ms32_road_ctrl, 0, sizeof(ms32_road_ctrl));
	ms32_tilemaplayoutcontrol = 0;
}

/* sprite RAM is latched at the start of VBLANK, after the frame is drawn */
void ms32_latch_sprites(void)
{
	memcpy(sprram_buffer, ms32_sprram, 0x8000 * sizeof(UINT16));
}

/* F1 Super Battle copies the text RAM on the field interrupt */
void ms32_f1superb_latch_tx(void)
{
	memcpy(txram_latch, ms32_txram, 0x2000 * sizeof(UINT16));
}

WRITE32_HANDLER( ms32_brightness_w )
{
	data32_t oldword = brt[offset];
	COMBINE_DATA(&brt[offset]);

	if (brt[offset] != oldword)
	{
		/* TODO: bank "1" is for sprite colors */
		int bank = ((offset & 2) >> 1) * 0x4000;
		if (bank == 0)
		{
			ms32_palette_dirty = 1;
			brt_r = 0x100 - ((brt[0] & 0xff00) >> 8);
			brt_g = 0x100 - ((brt[0] & 0x00ff) >> 0);
			brt_b = 0x100 - ((brt[1] & 0x00ff) >> 0);
		}
	}
}

static void update_colors(void)
{
	int color;
	int depth32 = (Machine->color_depth == 32);
	if (!ms32_palette_dirty)
		return;
	ms32_palette_dirty = 0;
	for (color = 0; color < 0x8000; color++)
	{
		int r, g, b;
		if (~color & 0x4000)
		{
			r = ((ms32_palram[color*2] & 0xff00) >> 8) * brt_r / 0x100;
			g = ((ms32_palram[color*2] & 0x00ff) >> 0) * brt_g / 0x100;
			b = ((ms32_palram[color*2+1] & 0x00ff) >> 0) * brt_b / 0x100;
		}
		else
		{
			r = ((ms32_palram[color*2] & 0xff00) >> 8);
			g = ((ms32_palram[color*2] & 0x00ff) >> 0);
			b = ((ms32_palram[color*2+1] & 0x00ff) >> 0);
		}
		if (depth32)
			pens[color] = (r << 16) | (g << 8) | b;
		else
			pens[color] = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
	}
}

/* half brightness of an output pen, used for the shadow cases */
static UINT32 half_pen(UINT32 c)
{
	if (Machine->color_depth == 32)
		return (c >> 1) & 0x7f7f7f;
	return (c >> 1) & 0x3def;
}


/********** Tile layers **********/

/* scrolled tilemap of 8bpp tiles (ASCII and BG layers); rowflags as build_row_flags */
static void draw_scroll_layer(const struct rectangle *clip, UINT16 *ram, const UINT8 *gfx, UINT32 count, const UINT8 *rowflags,
		int tilesize, int cols, int rows, int scrollx, int scrolly, int colorbase, UINT8 pri,
		UINT16 *dstbuf, UINT8 *pribuf)
{
	int x, y;
	int wmask = cols * tilesize - 1;
	int hmask = rows * tilesize - 1;
	int shift = (tilesize == 16) ? 4 : 3;
	int tbytes = tilesize * tilesize;
	UINT32 cmask = TILE_MASK(count);

	for (y = clip->min_y; y <= clip->max_y; y++)
	{
		int my = (y + scrolly) & hmask;
		int row = my >> shift;
		int ty = my & (tilesize - 1);
		UINT16 *dst = dstbuf + y * MS32_BUF_W;
		UINT8 *pdst = pribuf ? pribuf + y * MS32_BUF_W : NULL;

		x = clip->min_x;
		while (x <= clip->max_x)
		{
			int mx = (x + scrollx) & wmask;
			int tile_index = row * cols + (mx >> shift);
			UINT32 tile = TILE_NUMBER(ram[tile_index * 2] & 0xffff, count, cmask);
			int base = colorbase + (ram[tile_index * 2 + 1] & 0x000f) * 256;
			int tx = mx & (tilesize - 1);
			int n = tilesize - tx, k;
			const UINT8 *src = gfx + tile * tbytes + ty * tilesize + tx;
			UINT16 *d = dst + x;
			UINT8 flag = rowflags[tile * tilesize + ty];
			if (n > clip->max_x - x + 1)
				n = clip->max_x - x + 1;
			x += n;
			if (flag == ROW_EMPTY)
				continue;
			if (flag == ROW_OPAQUE)
			{
				for (k = 0; k < n; k++)
					d[k] = base + src[k];
				if (pdst)
				{
					UINT8 *pd = pdst + (d - dst);
					for (k = 0; k < n; k++)
						pd[k] |= pri;
				}
			}
			else if (pdst)
			{
				UINT8 *pd = pdst + (d - dst);
				for (k = 0; k < n; k++)
				{
					UINT8 pix = src[k];
					if (pix)
					{
						d[k] = base + pix;
						pd[k] |= pri;
					}
				}
			}
			else
			{
				for (k = 0; k < n; k++)
				{
					UINT8 pix = src[k];
					if (pix)
						d[k] = base + pix;
				}
			}
		}
	}
}

/* rotate / zoom with wraparound, as MAME's tilemap draw_roz */
static void draw_roz_core(const struct rectangle *clip, UINT16 *ram, const UINT8 *gfx, UINT32 count, const UINT8 *rowflags,
		int tilew, int tileh, int cols, int rows, int colorbase,
		UINT32 startx, UINT32 starty, int incxx, int incxy, int incyx, int incyy, int wrap,
		UINT8 pri, UINT16 *dstbuf, UINT8 *pribuf)
{
	int x, y;
	UINT32 xmask = cols * tilew - 1;
	UINT32 ymask = rows * tileh - 1;
	UINT32 width = cols * tilew, height = rows * tileh;
	int tbytes = tilew * tileh;
	int wshift = 0, hshift = 0;
	UINT32 cmask = TILE_MASK(count);

	while ((1 << wshift) < tilew) wshift++;
	while ((1 << hshift) < tileh) hshift++;

	startx += clip->min_x * incxx + clip->min_y * incyx;
	starty += clip->min_x * incxy + clip->min_y * incyy;

	for (y = clip->min_y; y <= clip->max_y; y++)
	{
		UINT32 cx = startx, cy = starty;
		UINT16 *dst = dstbuf + y * MS32_BUF_W;
		UINT8 *pdst = pribuf ? pribuf + y * MS32_BUF_W : NULL;

		if (incxy == 0)
		{
			/* the row of the map is the same for the whole line: fetch each
			   tile once while the line stays in it */
			UINT32 py = wrap ? ((cy >> 16) & ymask) : (cy >> 16);
			if (wrap || py < height)
			{
				const UINT16 *maprow = ram + (py >> hshift) * cols * 2;
				UINT32 trow = (py & (tileh - 1)) << wshift;
				UINT32 lastcol = 0xffffffff;
				const UINT8 *src = NULL;
				int base = 0;
				UINT8 flag = ROW_MIXED;
				UINT32 ty = py & (tileh - 1);

				for (x = clip->min_x; x <= clip->max_x; x++, cx += incxx)
				{
					UINT32 px = wrap ? ((cx >> 16) & xmask) : (cx >> 16);
					UINT32 col;
					UINT8 pix;
					if (!wrap && px >= width)
						continue;
					col = px >> wshift;
					if (col != lastcol)
					{
						UINT32 tile = TILE_NUMBER(maprow[col * 2] & 0xffff, count, cmask);
						base = colorbase + (maprow[col * 2 + 1] & 0x000f) * 256;
						src = gfx + tile * tbytes + trow;
						flag = rowflags ? rowflags[tile * tileh + ty] : ROW_MIXED;
						lastcol = col;
					}
					if (flag == ROW_EMPTY)
						continue;
					pix = src[px & (tilew - 1)];
					if (pix)
					{
						dst[x] = base + pix;
						if (pdst) pdst[x] |= pri;
					}
				}
			}
		}
		else
		{
			for (x = clip->min_x; x <= clip->max_x; x++)
			{
				UINT32 px, py;
				int ok = 1;
				if (wrap)
				{
					px = (cx >> 16) & xmask;
					py = (cy >> 16) & ymask;
				}
				else
				{
					px = cx >> 16;
					py = cy >> 16;
					if (px >= width || py >= height) ok = 0;
				}
				if (ok)
				{
					int tile_index = (py >> hshift) * cols + (px >> wshift);
					UINT32 code = ram[tile_index * 2] & 0xffff;
					int colour = ram[tile_index * 2 + 1] & 0x000f;
					UINT8 pix = gfx[TILE_NUMBER(code, count, cmask) * tbytes + ((py & (tileh - 1)) << wshift) + (px & (tilew - 1))];
					if (pix)
					{
						dst[x] = colorbase + colour * 256 + pix;
						if (pdst) pdst[x] |= pri;
					}
				}
				cx += incxx;
				cy += incxy;
			}
		}
		startx += incyx;
		starty += incyy;
	}
}

static void draw_roz(const struct rectangle *cliprect, UINT8 priority)
{
	/* TODO: registers 0x40 / 0x44 and 0x50 / 0x54 are used, unknown meaning */
	if (ms32_roz_ctrl[0x5c/4] & 1)	/* "super" mode */
	{
		struct rectangle my_clip = *cliprect;
		int y;

		for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		{
			UINT16 *lineaddr = &ms32_lineram[8 * (y & 0xff)];
			int start2x = (lineaddr[0x00/4] & 0xffff) | ((lineaddr[0x04/4] & 3) << 16);
			int start2y = (lineaddr[0x08/4] & 0xffff) | ((lineaddr[0x0c/4] & 3) << 16);
			int incxx  = (lineaddr[0x10/4] & 0xffff) | ((lineaddr[0x14/4] & 1) << 16);
			int incxy  = (lineaddr[0x18/4] & 0xffff) | ((lineaddr[0x1c/4] & 1) << 16);
			int startx = (ms32_roz_ctrl[0x00/4] & 0xffff) | ((ms32_roz_ctrl[0x04/4] & 3) << 16);
			int starty = (ms32_roz_ctrl[0x08/4] & 0xffff) | ((ms32_roz_ctrl[0x0c/4] & 3) << 16);
			int offsx  = ms32_roz_ctrl[0x30/4];
			int offsy  = ms32_roz_ctrl[0x34/4];

			my_clip.min_y = my_clip.max_y = y;

			offsx += (ms32_roz_ctrl[0x38/4] & 1) * 0x400;	/* ??? gratia, hayaosi1... */
			offsy += (ms32_roz_ctrl[0x3c/4] & 1) * 0x400;	/* ??? gratia, hayaosi1... */

			/* extend sign */
			if (start2x & 0x20000) start2x |= ~0x3ffff;
			if (start2y & 0x20000) start2y |= ~0x3ffff;
			if (startx & 0x20000) startx |= ~0x3ffff;
			if (starty & 0x20000) starty |= ~0x3ffff;
			if (incxx & 0x10000) incxx |= ~0x1ffff;
			if (incxy & 0x10000) incxy |= ~0x1ffff;

			draw_roz_core(&my_clip, ms32_rozram, roz_gfx, roz_count, roz_rows, 16, 16, 128, 128, 0x2000,
					(UINT32)(start2x + startx + offsx) << 16, (UINT32)(start2y + starty + offsy) << 16,
					(int)((UINT32)incxx << 8), (int)((UINT32)incxy << 8), 0, 0,
					1, priority, buf_tiles, buf_tilepri);
		}
	}
	else	/* "simple" mode */
	{
		int startx = (ms32_roz_ctrl[0x00/4] & 0xffff) | ((ms32_roz_ctrl[0x04/4] & 3) << 16);
		int starty = (ms32_roz_ctrl[0x08/4] & 0xffff) | ((ms32_roz_ctrl[0x0c/4] & 3) << 16);
		int incxx  = (ms32_roz_ctrl[0x10/4] & 0xffff) | ((ms32_roz_ctrl[0x14/4] & 1) << 16);
		int incxy  = (ms32_roz_ctrl[0x18/4] & 0xffff) | ((ms32_roz_ctrl[0x1c/4] & 1) << 16);
		int incyy  = (ms32_roz_ctrl[0x20/4] & 0xffff) | ((ms32_roz_ctrl[0x24/4] & 1) << 16);
		int incyx  = (ms32_roz_ctrl[0x28/4] & 0xffff) | ((ms32_roz_ctrl[0x2c/4] & 1) << 16);
		int offsx  = ms32_roz_ctrl[0x30/4];
		int offsy  = ms32_roz_ctrl[0x34/4];

		offsx += (ms32_roz_ctrl[0x38/4] & 1) * 0x400;	/* ??? gratia, hayaosi1... */
		offsy += (ms32_roz_ctrl[0x3c/4] & 1) * 0x400;	/* ??? gratia, hayaosi1... */

		/* extend sign */
		if (startx & 0x20000) startx |= ~0x3ffff;
		if (starty & 0x20000) starty |= ~0x3ffff;
		if (incxx & 0x10000) incxx |= ~0x1ffff;
		if (incxy & 0x10000) incxy |= ~0x1ffff;
		if (incyy & 0x10000) incyy |= ~0x1ffff;
		if (incyx & 0x10000) incyx |= ~0x1ffff;

		draw_roz_core(cliprect, ms32_rozram, roz_gfx, roz_count, roz_rows, 16, 16, 128, 128, 0x2000,
				(UINT32)(startx + offsx) << 16, (UINT32)(starty + offsy) << 16,
				(int)((UINT32)incxx << 8), (int)((UINT32)incxy << 8), (int)((UINT32)incyx << 8), (int)((UINT32)incyy << 8),
				1, priority, buf_tiles, buf_tilepri);
	}
}

static void draw_tile_layers(const struct rectangle *cliprect)
{
	int asc_pri, scr_pri, rot_pri, prin;
	int scrollx, scrolly, bgx, bgy;

	scrollx = ms32_tx_scroll[0x00/4] + ms32_tx_scroll[0x08/4] + 0x18;
	scrolly = ms32_tx_scroll[0x0c/4] + ms32_tx_scroll[0x14/4];
	bgx = ms32_bg_scroll[0x00/4] + ms32_bg_scroll[0x08/4] + 0x10;
	bgy = ms32_bg_scroll[0x0c/4] + ms32_bg_scroll[0x14/4];

	/* TODO: actually understand this (per-scanline priority and alpha-blend over every layer?) */
	asc_pri = scr_pri = rot_pri = 0;

	if ((ms32_priram[0x2b00 / 2] & 0x00ff) == 0x0034)
		asc_pri++;
	else
		rot_pri++;

	if ((ms32_priram[0x2e00 / 2] & 0x00ff) == 0x0034)
		asc_pri++;
	else
		scr_pri++;

	/* Suchiepai 2 title & Gratia gameplay intermissions uses 0x0f
	   hayaosi3 uses 0x09 during flames screen on attract (text should go above the rest)
	   this is otherwise 0x17 most of the time except for 0x15 in hayaosi3, tetris plus 2 & world pk soccer 2
	   kirarast flips between 0x16 in gameplay and 0x17 otherwise */
	if (ms32_priram[0x3a00 / 2] == 0x09)
		asc_pri = 3;
	if ((ms32_priram[0x3a00 / 2] & 0x0030) == 0x00)
		scr_pri++;
	else
		rot_pri++;

	/* tile-tile mixing */
	for (prin = 0; prin < 4; prin++)
	{
		if (rot_pri == prin)
			draw_roz(cliprect, 1 << 1);
		else if (scr_pri == prin)
		{
			if (ms32_tilemaplayoutcontrol & 1)
				draw_scroll_layer(cliprect, ms32_bgram, bg_gfx, bg_count, bg_rows, 16, 256, 16, bgx, bgy, 0x1000, 1 << 0, buf_tiles, buf_tilepri);
			else
				draw_scroll_layer(cliprect, ms32_bgram, bg_gfx, bg_count, bg_rows, 16, 64, 64, bgx, bgy, 0x1000, 1 << 0, buf_tiles, buf_tilepri);
		}
		else if (asc_pri == prin)
			draw_scroll_layer(cliprect, ms32_txram, tx_gfx, tx_count, tx_rows, 8, 64, 64, scrollx, scrolly, 0x6000, 1 << 2, buf_tiles, buf_tilepri);
	}
}


/********** Sprites **********/

/*
    Sprite format (16 byte per each sprite):
    Offset Bits              Description
           fedcba98 76543210
    00     -------- xxxx---- Priority
           -------- -----x-- Visible
           -------- ------x- Flip Y
           -------- -------x Flip X
    02     xxxxxxxx -------- Source Y offset (1 pixel each)
           -------- xxxxxxxx Source X offset (1 pixel each)
    04     xxxx---- -------- Palette Select
           ----xxxx xxxxxxxx Source Texture Select (each texture is 256 x 256 pixels)
    06     xxxxxxxx -------- Source Width - 1 (1 pixel each)
           -------- xxxxxxxx Source Height - 1 (1 pixel each)
    08     ------xx xxxxxxxx Y (10 bits signed)
    0a     -----xxx xxxxxxxx X (11 bits signed)
    0c     xxxxxxxx xxxxxxxx Zoom X (0x200 = 50%, 0x100 = 100%, 0x80 = 200%)
    0e     xxxxxxxx xxxxxxxx Zoom Y
*/
static void draw_sprite(const struct rectangle *clip, UINT32 code, UINT16 color, int flipx, int flipy,
		INT32 destx, INT32 desty, UINT32 tx, UINT32 ty, UINT32 srcwidth, UINT32 srcheight, UINT32 incx, UINT32 incy)
{
	const UINT8 *srcdata;
	INT32 srcstartx = tx << 8, srcstarty = ty << 8;
	UINT32 srcendx = srcwidth << 8, srcendy = srcheight << 8;
	UINT32 srcx = 0, srcy = 0;
	INT32 curx, cury;

	if (!incx || !incy)
		return;

	code %= spr_count;
	srcdata = spr_gfx + code * 0x10000;

	if (incx == 0x100 && incy == 0x100)
	{
		/* MAME's non-zoom core */
		INT32 destendx = destx + srcwidth - 1, destendy = desty + srcheight - 1;
		INT32 dx = 1, dy = 1;
		if (destx > clip->max_x || destendx < clip->min_x) return;
		if (destx < clip->min_x) { srcx = clip->min_x - destx; destx = clip->min_x; }
		if (destendx > clip->max_x) destendx = clip->max_x;
		if (desty > clip->max_y || destendy < clip->min_y) return;
		if (desty < clip->min_y) { srcy = clip->min_y - desty; desty = clip->min_y; }
		if (destendy > clip->max_y) destendy = clip->max_y;
		if (flipx) { srcx = srcwidth - 1 - srcx; dx = -1; }
		if (flipy) { srcy = srcheight - 1 - srcy; dy = -1; }

		/* the texture column is tx + srcx +/- (curx - destx), drawn where it is 0..255 */
		{
			INT32 col0 = (INT32)(tx + srcx);
			INT32 startx = destx, endx = destendx;
			if (dx > 0)
			{
				if (destx + 255 - col0 < endx)
					endx = destx + 255 - col0;
			}
			else
			{
				if (col0 > 255)
					startx = destx + col0 - 255;
				if (destx + col0 < endx)
					endx = destx + col0;
			}

			for (cury = desty; cury <= destendy; cury++)
			{
				UINT32 drawy = ty + srcy;
				UINT16 *dst;
				UINT8 *pri;
				const UINT8 *src;
				srcy += dy;
				if (drawy >= 256)
					continue;
				dst = buf_spr + cury * MS32_BUF_W;
				spr_row_used[cury] = 1;
				pri = buf_sprpri + cury * MS32_BUF_W;
				src = srcdata + drawy * 256;
				if (dx > 0)
				{
					for (curx = startx; curx <= endx; curx++)
					{
						UINT8 p = src[col0 + (curx - destx)];
						if (p && !pri[curx])
						{
							dst[curx] = color + p;
							pri[curx] = 31;
						}
					}
				}
				else
				{
					for (curx = startx; curx <= endx; curx++)
					{
						UINT8 p = src[col0 - (curx - destx)];
						if (p && !pri[curx])
						{
							dst[curx] = color + p;
							pri[curx] = 31;
						}
					}
				}
			}
		}
		return;
	}

	/* zoom core */
	if (destx < clip->min_x)
	{
		srcx = (clip->min_x - destx) * incx;
		destx = clip->min_x;
	}
	if (srcx >= srcendx)
		return;
	if (desty < clip->min_y)
	{
		srcy = (clip->min_y - desty) * incy;
		desty = clip->min_y;
	}
	if (srcy >= srcendy)
		return;

	for (cury = desty; (cury <= clip->max_y) && (srcy < srcendy); cury++, srcy += incy)
	{
		UINT32 drawy = (srcstarty + (flipy ? (srcendy - srcy - 1) : srcy)) >> 8;
		UINT32 cursrcx = srcx;
		UINT16 *dst;
		UINT8 *pri;
		const UINT8 *src;
		if (drawy >= 256)
			continue;
		dst = buf_spr + cury * MS32_BUF_W;
		spr_row_used[cury] = 1;
		pri = buf_sprpri + cury * MS32_BUF_W;
		src = srcdata + drawy * 256;
		if (!flipx)
		{
			/* the texture column grows: past 255 it stays past */
			for (curx = destx; (curx <= clip->max_x) && (cursrcx < srcendx); curx++, cursrcx += incx)
			{
				UINT32 drawx = (srcstartx + cursrcx) >> 8;
				UINT8 p;
				if (drawx >= 256)
					break;
				p = src[drawx];
				if (p && !pri[curx])
				{
					dst[curx] = color + p;
					pri[curx] = 31;
				}
			}
		}
		else
		{
			for (curx = destx; (curx <= clip->max_x) && (cursrcx < srcendx); curx++, cursrcx += incx)
			{
				UINT32 drawx = (srcstartx + (srcendx - cursrcx - 1)) >> 8;
				UINT8 p;
				if (drawx >= 256)
					continue;
				p = src[drawx];
				if (p && !pri[curx])
				{
					dst[curx] = color + p;
					pri[curx] = 31;
				}
			}
		}
	}
}

static void draw_sprites(const struct rectangle *cliprect)
{
	const int sprite_tail = 0x8000 - 8;
	UINT16 *source = sprram_buffer;
	UINT16 *finish = sprram_buffer + sprite_tail;
	/* TODO: sprite control 0x10 also uses bits 0-11 for sprite start address? */
	int reverseorder = (ms32_sprite_ctrl[0x10/4] & 0x8000) == 0x0000;
	int step = 8;

	if (reverseorder)
	{
		source = sprram_buffer + sprite_tail;
		finish = sprram_buffer;
		step = -8;
	}

	for (; reverseorder ? (source >= finish) : (source < finish); source += step)
	{
		UINT16 attr = source[0];
		int pri = attr & 0x00f0;
		int flipx = attr & 1, flipy = attr & 2;
		UINT32 code = source[1];
		UINT32 color = source[2];
		int tx = (code >> 0) & 0xff;
		int ty = (code >> 8) & 0xff;
		UINT16 size = source[3];
		UINT32 srcwidth = ((size >> 0) & 0xff) + 1;
		UINT32 srcheight = ((size >> 8) & 0xff) + 1;
		INT32 sx = (source[5] & 0x3ff) - (source[5] & 0x400);
		INT32 sy = (source[4] & 0x1ff) - (source[4] & 0x200);
		UINT32 xzoom = source[6], yzoom = source[7];

		if (!(attr & 0x0004) || !xzoom || !yzoom)
			continue;

		code = color & 0x0fff;
		color = (color >> 12) & 0xf;

		/* passes the priority as the upper bits of the colour, for the mixer */
		draw_sprite(cliprect, code, (UINT16)((color << 8) | (pri << 8)), flipx, flipy, sx, sy, tx, ty, srcwidth, srcheight, xzoom, yzoom);
	}
}


/********** Mixing (current MAME) **********/

/* what a pixel shows, from the sprite priority's mask, the tile layers
   drawn there and whether the sprite pixel is opaque (current MAME's cases) */
enum { MIX_SPRITE, MIX_TILE, MIX_TILE_HALF, MIX_BLACK };

static int mix_select(int primask, int src_tilepri, int sprite_opaque)
{
	switch (primask)
	{
	case 0x00:
		return sprite_opaque ? MIX_SPRITE : MIX_TILE;
	case 0xc0:	/* unhandled in MAME too (random pens) */
		return MIX_BLACK;
	case 0xcc:	/* hayaosi3 final round */
		if (src_tilepri & 0x02)
			return MIX_TILE;
		return sprite_opaque ? MIX_SPRITE : MIX_TILE;
	case 0xf0:
		if (src_tilepri <= 0x03)
			return sprite_opaque ? MIX_SPRITE : MIX_TILE;
		return MIX_TILE;
	case 0xfc:
		if (src_tilepri <= 0x01)
			return sprite_opaque ? MIX_SPRITE : MIX_TILE;
		return MIX_TILE;
	case 0xfe:
		if (src_tilepri == 0x00)
			return sprite_opaque ? MIX_SPRITE : MIX_TILE;
		if (src_tilepri <= 0x03)
			return MIX_TILE_HALF;	/* shadow, gametngk title / mode select */
		return MIX_TILE;
	case 0xf8:	/* gratia ending */
		if (sprite_opaque && src_tilepri == 0x02)
			return MIX_SPRITE;
		return MIX_TILE;
	default:	/* $fa actually used on hayaosi3 second champ transition, unknown purpose */
		return MIX_BLACK;
	}
}

/* the switch of mix_layers for one pixel */
static UINT32 mix_pixel(UINT8 sel, UINT16 src_spri, UINT16 src_tile)
{
	switch (sel)
	{
	case MIX_SPRITE:	return pens[src_spri & 0x0fff];
	case MIX_TILE:		return pens[src_tile];
	case MIX_TILE_HALF:	return half_pen(pens[src_tile]);
	default:		return 0;
	}
}

static void mix_layers(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT8 sel[16][8][2];
	UINT8 sprite_wins[16];	/* bit t: an opaque sprite pixel wins over tile priority t */
	UINT8 special[16];	/* this sprite priority has cases other than sprite / tile */
	int i, t, xx, yy;
	int depth32 = (Machine->color_depth == 32);

	/* the priority RAM lookup depends only on the sprite priority */
	for (i = 0; i < 16; i++)
	{
		int spritepri = i << 4;
		int primask = 0;
		if (ms32_priram[(spritepri | 0x0a00 | 0x1500) / 2] & 0x38) primask |= 1 << 0;
		if (ms32_priram[(spritepri | 0x0a00 | 0x1400) / 2] & 0x38) primask |= 1 << 1;
		if (ms32_priram[(spritepri | 0x0a00 | 0x1100) / 2] & 0x38) primask |= 1 << 2;
		if (ms32_priram[(spritepri | 0x0a00 | 0x1000) / 2] & 0x38) primask |= 1 << 3;
		if (ms32_priram[(spritepri | 0x0a00 | 0x0500) / 2] & 0x38) primask |= 1 << 4;
		if (ms32_priram[(spritepri | 0x0a00 | 0x0400) / 2] & 0x38) primask |= 1 << 5;
		if (ms32_priram[(spritepri | 0x0a00 | 0x0100) / 2] & 0x38) primask |= 1 << 6;
		if (ms32_priram[(spritepri | 0x0a00 | 0x0000) / 2] & 0x38) primask |= 1 << 7;
		sprite_wins[i] = 0;
		special[i] = 0;
		for (t = 0; t < 8; t++)
		{
			sel[i][t][0] = mix_select(primask, t, 0);
			sel[i][t][1] = mix_select(primask, t, 1);
			if (sel[i][t][0] != MIX_TILE || (sel[i][t][1] != MIX_TILE && sel[i][t][1] != MIX_SPRITE))
				special[i] = 1;
			if (sel[i][t][1] == MIX_SPRITE)
				sprite_wins[i] |= 1 << t;
		}
	}

	/* most pixels show the sprite or the tile: that case without the switch */
	for (yy = cliprect->min_y; yy <= cliprect->max_y; yy++)
	{
		const UINT16 *srcptr_tile = buf_tiles + yy * MS32_BUF_W;
		const UINT8 *srcptr_tilepri = buf_tilepri + yy * MS32_BUF_W;
		const UINT16 *srcptr_spri = buf_spr + yy * MS32_BUF_W;

		if (!spr_row_used[yy] && !special[0])
		{
			/* no sprite on this line: the tiles everywhere */
			if (depth32)
			{
				UINT32 *dst = (UINT32 *)bitmap->line[yy];
				for (xx = cliprect->min_x; xx <= cliprect->max_x; xx++)
					dst[xx] = pens[srcptr_tile[xx]];
			}
			else
			{
				UINT16 *dst = (UINT16 *)bitmap->line[yy];
				for (xx = cliprect->min_x; xx <= cliprect->max_x; xx++)
					dst[xx] = (UINT16)pens[srcptr_tile[xx]];
			}
		}
		else if (depth32)
		{
			UINT32 *dst = (UINT32 *)bitmap->line[yy];
			for (xx = cliprect->min_x; xx <= cliprect->max_x; xx++)
			{
				UINT16 src_spri = srcptr_spri[xx];
				int p = src_spri >> 12, tp = srcptr_tilepri[xx] & 7;
				if (special[p])
					dst[xx] = mix_pixel(sel[p][tp][(src_spri & 0xff) != 0], src_spri, srcptr_tile[xx]);
				else if ((src_spri & 0xff) && (sprite_wins[p] & (1 << tp)))
					dst[xx] = pens[src_spri & 0x0fff];
				else
					dst[xx] = pens[srcptr_tile[xx]];
			}
		}
		else
		{
			UINT16 *dst = (UINT16 *)bitmap->line[yy];
			for (xx = cliprect->min_x; xx <= cliprect->max_x; xx++)
			{
				UINT16 src_spri = srcptr_spri[xx];
				int p = src_spri >> 12, tp = srcptr_tilepri[xx] & 7;
				if (special[p])
					dst[xx] = (UINT16)mix_pixel(sel[p][tp][(src_spri & 0xff) != 0], src_spri, srcptr_tile[xx]);
				else if ((src_spri & 0xff) && (sprite_wins[p] & (1 << tp)))
					dst[xx] = (UINT16)pens[src_spri & 0x0fff];
				else
					dst[xx] = (UINT16)pens[srcptr_tile[xx]];
			}
		}
	}
}


/********** F1 Super Battle (current MAME) **********/

static void draw_line_plane(const struct rectangle *cliprect, UINT16 *dstbuf, const UINT8 *gfx, UINT32 count,
		int colorbase_add, const UINT16 *vram, const UINT16 *lineram, const data32_t *ctrl, int wrap, UINT16 *line_colour)
{
	int y;
	int startx = (int)(((ctrl[0x00/4] & 0xffff) | ((ctrl[0x04/4] & 3) << 16)) << 14) >> 14;
	int starty = (int)(((ctrl[0x08/4] & 0xffff) | ((ctrl[0x0c/4] & 3) << 16)) << 14) >> 14;
	int offsx = ctrl[0x30/4] + (ctrl[0x38/4] & 1) * 0x400;
	int offsy = ctrl[0x34/4] + (ctrl[0x3c/4] & 1) * 0x400;
	struct rectangle clip = *cliprect;

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const UINT16 *line = &lineram[8 * (y & 0xff)];
		int start2x = (int)((UINT32)((line[0] & 0xffff) | ((line[1] & 3) << 16)) << 14) >> 14;
		int start2y = (int)((UINT32)((line[2] & 0xffff) | ((line[3] & 3) << 16)) << 14) >> 14;
		int incxx = (int)((UINT32)((line[4] & 0xffff) | ((line[5] & 1) << 16)) << 15) >> 15;
		int incxy = (int)((UINT32)((line[6] & 0xffff) | ((line[7] & 1) << 16)) << 15) >> 15;
		int row = (start2y + starty + offsy) & 0x3ff;

		line_colour[y & 0xff] = 0;
		if (!vram[row * 2])
			continue;
		line_colour[y & 0xff] = vram[row * 2 + 1];

		clip.min_y = clip.max_y = y;
		/* a 1 x 0x400 tilemap of 2048 x 1 tiles */
		draw_roz_core(&clip, (UINT16 *)vram, gfx, count, NULL, 2048, 1, 1, 0x400, colorbase_add,
				(UINT32)(start2x + startx + offsx) << 16, (UINT32)(start2y + starty + offsy) << 16,
				(int)((UINT32)incxx * 0x100), (int)((UINT32)incxy * 0x100), 0, 0,
				wrap, 0, dstbuf, NULL);
	}
}

/*
    Per-pixel lookup in priority RAM:
    bit 12     sprite transparent
    bit 11     text transparent
    bit 10     unknown, always 1 on the games checked
    bit 9      ROZ transparent
    bit 8      road plane transparent
    bit 7      BG transparent
    bits 6-3   sprite priority (attribute bits 7-4)
    bits 2-0   line depth, colour bits 6-4 of the ROZ line, or of the road plane line where ROZ is transparent

    Output: bits 5-3 select the layer (0 sprite, 1 BG, 2 ROZ, 4 road plane, 6 text), bit 6 selects the backdrop.
    TODO: bit 2 clear is approximated as half brightness, bits 1-0 are ignored
*/
static void f1superb_mix_layers(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int x, y;
	int depth32 = (Machine->color_depth == 32);
	UINT16 road_line_colour[256], roz_line_colour[256];
	int bgx = ms32_bg_scroll[0x00/4] + ms32_bg_scroll[0x08/4] + 0x10;
	int bgy = ms32_bg_scroll[0x0c/4] + ms32_bg_scroll[0x14/4];
	int txx = ms32_tx_scroll[0x00/4] + ms32_tx_scroll[0x08/4] + 0x18;
	int txy = ms32_tx_scroll[0x0c/4] + ms32_tx_scroll[0x14/4];

	memset(road_line_colour, 0, sizeof(road_line_colour));
	memset(roz_line_colour, 0, sizeof(roz_line_colour));
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			int o = y * MS32_BUF_W + x;
			buf_layer_tx[o] = buf_layer_bg[o] = buf_layer_road[o] = buf_layer_roz[o] = 0xffff;
		}
	}

	draw_scroll_layer(cliprect, txram_latch, tx_gfx, tx_count, tx_rows, 8, 64, 64, txx, txy, 0x6000, 0, buf_layer_tx, NULL);
	if (ms32_tilemaplayoutcontrol & 1)
		draw_scroll_layer(cliprect, ms32_bgram, bg_gfx, bg_count, bg_rows, 16, 256, 16, bgx, bgy, 0x1000, 0, buf_layer_bg, NULL);
	else
		draw_scroll_layer(cliprect, ms32_bgram, bg_gfx, bg_count, bg_rows, 16, 64, 64, bgx, bgy, 0x1000, 0, buf_layer_bg, NULL);
	draw_line_plane(cliprect, buf_layer_road, road_gfx, road_count, 0x50 * 256, ms32_road_vram, ms32_road_lineram, ms32_road_ctrl, 1, road_line_colour);
	draw_line_plane(cliprect, buf_layer_roz, roz_gfx, roz_count, 0x2000, ms32_rozram, ms32_lineram, ms32_roz_ctrl, 0, roz_line_colour);

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const UINT16 *spr = buf_spr + y * MS32_BUF_W;
		const UINT16 *tx = buf_layer_tx + y * MS32_BUF_W;
		const UINT16 *bg = buf_layer_bg + y * MS32_BUF_W;
		const UINT16 *road = buf_layer_road + y * MS32_BUF_W;
		const UINT16 *roz = buf_layer_roz + y * MS32_BUF_W;
		UINT32 *dst32 = depth32 ? (UINT32 *)bitmap->line[y] : NULL;
		UINT16 *dst16 = depth32 ? NULL : (UINT16 *)bitmap->line[y];
		UINT16 road_depth = (road_line_colour[y & 0xff] >> 4) & 7;
		UINT16 roz_depth = (roz_line_colour[y & 0xff] >> 4) & 7;

		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			int s_op = (spr[x] & 0xff) != 0;
			UINT16 pri = s_op ? (spr[x] >> 12) : 0;
			UINT16 depth = (roz[x] != 0xffff) ? roz_depth : road_depth;
			UINT16 idx = (!s_op << 12) | ((tx[x] == 0xffff) << 11) | (1 << 10) | ((roz[x] == 0xffff) << 9) | ((road[x] == 0xffff) << 8) | ((bg[x] == 0xffff) << 7) | (pri << 3) | depth;
			UINT8 code = ms32_priram[idx];
			UINT16 pen = 0;
			UINT32 c;

			if (!(code & 0x40))
			{
				switch ((code >> 3) & 7)
				{
				case 0: pen = spr[x] & 0x0fff; break;
				case 1: pen = bg[x]; break;
				case 2: pen = roz[x]; break;
				case 4: pen = road[x]; break;
				case 6: pen = tx[x]; break;
				default: pen = 0; break;
				}
				if (pen == 0xffff)
					pen = 0;
			}

			c = pens[pen & 0x7fff];
			if (!(code & 0x04))
				c = half_pen(c);
			if (depth32) dst32[x] = c; else dst16[x] = (UINT16)c;
		}
	}
}


VIDEO_UPDATE( ms32 )
{
	int y;

	update_colors();

	/* clear our work buffers */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		int w = cliprect->max_x - cliprect->min_x + 1;
		memset(buf_tiles + y * MS32_BUF_W + cliprect->min_x, 0, w * sizeof(UINT16));	/* bg color */
		memset(buf_tilepri + y * MS32_BUF_W + cliprect->min_x, 0, w);
		memset(buf_spr + y * MS32_BUF_W + cliprect->min_x, 0, w * sizeof(UINT16));
		memset(buf_sprpri + y * MS32_BUF_W + cliprect->min_x, 0, w);
		spr_row_used[y] = 0;
	}

	draw_sprites(cliprect);

	if (ms32_is_f1superb)
	{
		f1superb_mix_layers(bitmap, cliprect);
		return;
	}

	draw_tile_layers(cliprect);
	mix_layers(bitmap, cliprect);
}
