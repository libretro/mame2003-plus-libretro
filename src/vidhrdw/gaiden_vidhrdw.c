/***************************************************************************

	Ninja Gaiden / Tecmo Knights Video Hardware

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

data16_t *gaiden_videoram,*gaiden_videoram2,*gaiden_videoram3;
INT8 tx_offset_y, bg_offset_y, fg_offset_y, spr_offset_y;
int gaiden_sprite_sizey;
int raiga_alpha;

static struct tilemap *text_layer,*foreground,*background;
static struct mame_bitmap *sprite_bitmap, *tile_bitmap_bg, *tile_bitmap_fg;

/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_bg_tile_info(int tile_index)
{
	UINT16 *videoram1 = &gaiden_videoram3[0x0800];
	UINT16 *videoram2 = gaiden_videoram3;
	SET_TILE_INFO(
			1,
			videoram1[tile_index] & 0x0fff,
			(videoram2[tile_index] & 0xf0) >> 4,
			0)
}

static void get_fg_tile_info(int tile_index)
{
	UINT16 *videoram1 = &gaiden_videoram2[0x0800];
	UINT16 *videoram2 = gaiden_videoram2;
	SET_TILE_INFO(
			2,
			videoram1[tile_index] & 0x0fff,
			(videoram2[tile_index] & 0xf0) >> 4,
			0)
}

static void get_fg_tile_info_raiga(int tile_index)
{
	UINT16 *videoram1 = &gaiden_videoram2[0x0800];
	UINT16 *videoram2 = gaiden_videoram2;

	/* bit 3 controls blending */
	tile_info.priority = (videoram2[tile_index] & 0x08) >> 3;

	SET_TILE_INFO(
			2,
			videoram1[tile_index] & 0x0fff,
			((videoram2[tile_index] & 0xf0) >> 4) | (tile_info.priority ? 0x80 : 0x00),
			0)
}

static void get_tx_tile_info(int tile_index)
{
	UINT16 *videoram1 = &gaiden_videoram[0x0400];
	UINT16 *videoram2 = gaiden_videoram;
	SET_TILE_INFO(
			0,
			videoram1[tile_index] & 0x07ff,
			(videoram2[tile_index] & 0xf0) >> 4,
			0)
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

VIDEO_START( gaiden )
{
	/* set up tile layers */
	background = tilemap_create(get_bg_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 64, 32);
	foreground = tilemap_create(get_fg_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 64, 32);
	text_layer = tilemap_create(get_tx_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT,  8,  8, 32, 32);

	if (!text_layer || !foreground || !background)
		return 1;

	tilemap_set_transparent_pen(background, 0);
	tilemap_set_transparent_pen(foreground, 0);
	tilemap_set_transparent_pen(text_layer, 0);
	
	tilemap_set_scrolldy(background, 0, 33);
	tilemap_set_scrolldy(foreground, 0, 33);
	tilemap_set_scrolldy(text_layer, 0, 31);

	tilemap_set_scrolldx(background, 0, -1);
	tilemap_set_scrolldx(foreground, 0, -1);
	tilemap_set_scrolldx(text_layer, 0, -1);

	return 0;
}

VIDEO_START( raiga )
{
	/* set up tile layers */
	tile_bitmap_bg = auto_bitmap_alloc_depth(Machine->drv->screen_width, Machine->drv->screen_height, 16);
	tile_bitmap_fg = auto_bitmap_alloc_depth(Machine->drv->screen_width, Machine->drv->screen_height, 16);

	if (!tile_bitmap_bg || !tile_bitmap_fg)
		return 1;

	background = tilemap_create(get_bg_tile_info,	   tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,32);
	foreground = tilemap_create(get_fg_tile_info_raiga,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,32);
	text_layer = tilemap_create(get_tx_tile_info,	   tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,32,32);

	if (!text_layer || !foreground || !background)
		return 1;

	tilemap_set_transparent_pen(background,0);
	tilemap_set_transparent_pen(foreground,0);
	tilemap_set_transparent_pen(text_layer,0);

	/* set up sprites */
	sprite_bitmap = auto_bitmap_alloc_depth(Machine->drv->screen_width, Machine->drv->screen_height, 16);

	if (!sprite_bitmap)
		return 1;

	return 0;
}

VIDEO_START( drgnbowl )
{
	/* set up tile layers */
	background = tilemap_create(get_bg_tile_info, tilemap_scan_rows, TILEMAP_OPAQUE,      16, 16, 64, 32);
	foreground = tilemap_create(get_fg_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 64, 32);
	text_layer = tilemap_create(get_tx_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT,  8,  8, 32, 32);

	if (!text_layer || !foreground || !background)
		return 1;

	tilemap_set_transparent_pen(foreground, 15);
	tilemap_set_transparent_pen(text_layer, 15);

	tilemap_set_scrolldx(background, -248, 248);
	tilemap_set_scrolldx(foreground, -252, 252);

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE16_HANDLER( gaiden_flip_w )
{
	if (ACCESSING_LSB)
		flip_screen_set(data & 1);
}

WRITE16_HANDLER( gaiden_txscrollx_w )
{
	static data16_t scroll;
	COMBINE_DATA(&scroll);
	tilemap_set_scrollx(text_layer, 0, scroll);
}

WRITE16_HANDLER( gaiden_txscrolly_w )
{
	static data16_t scroll;
	COMBINE_DATA(&scroll);
	tilemap_set_scrolly(text_layer, 0, (scroll - tx_offset_y) & 0xffff);
}

WRITE16_HANDLER( gaiden_fgscrollx_w )
{
	static data16_t scroll;
	COMBINE_DATA(&scroll);
	tilemap_set_scrollx(foreground, 0, scroll);
}

WRITE16_HANDLER( gaiden_fgscrolly_w )
{
	static data16_t scroll;
	COMBINE_DATA(&scroll);
	tilemap_set_scrolly(foreground, 0, (scroll - fg_offset_y) & 0xffff);
}

WRITE16_HANDLER( gaiden_bgscrollx_w )
{
	static data16_t scroll;
	COMBINE_DATA(&scroll);
	tilemap_set_scrollx(background, 0, scroll);
}

WRITE16_HANDLER( gaiden_bgscrolly_w )
{
	static data16_t scroll;
	COMBINE_DATA(&scroll);
	tilemap_set_scrolly(background, 0, (scroll - bg_offset_y) & 0xffff);
}

WRITE16_HANDLER( gaiden_txoffsety_w )
{
    static data16_t scroll;
	if (ACCESSING_LSB) {
		tx_offset_y = data;
		tilemap_set_scrolly(text_layer, 0, (scroll - tx_offset_y) & 0xffff);
	}
}

WRITE16_HANDLER( gaiden_fgoffsety_w )
{
    static data16_t scroll;
	if (ACCESSING_LSB) {
		fg_offset_y = data;
		tilemap_set_scrolly(foreground, 0, (scroll - fg_offset_y) & 0xffff);
	}
}

WRITE16_HANDLER( gaiden_bgoffsety_w )
{
    static data16_t scroll;
	if (ACCESSING_LSB) {
		bg_offset_y = data;
		tilemap_set_scrolly(background, 0, (scroll - bg_offset_y) & 0xffff);
	}
}

WRITE16_HANDLER( gaiden_sproffsety_w )
{
	if (ACCESSING_LSB) {
		spr_offset_y = data;
		/* handled in draw_sprites */
	}
}

WRITE16_HANDLER( gaiden_videoram3_w )
{
	int oldword = gaiden_videoram3[offset];
	COMBINE_DATA(&gaiden_videoram3[offset]);
	if (oldword != gaiden_videoram3[offset])
		tilemap_mark_tile_dirty(background,offset & 0x07ff);
}


READ16_HANDLER( gaiden_videoram3_r )
{
   return gaiden_videoram3[offset];
}

WRITE16_HANDLER( gaiden_videoram2_w )
{
	int oldword = gaiden_videoram2[offset];
	COMBINE_DATA(&gaiden_videoram2[offset]);
	if (oldword != gaiden_videoram2[offset])
		tilemap_mark_tile_dirty(foreground,offset & 0x07ff);
}

READ16_HANDLER( gaiden_videoram2_r )
{
   return gaiden_videoram2[offset];
}

WRITE16_HANDLER( gaiden_videoram_w )
{
	int oldword = gaiden_videoram[offset];
	COMBINE_DATA(&gaiden_videoram[offset]);
	if (oldword != gaiden_videoram[offset])
		tilemap_mark_tile_dirty(text_layer,offset & 0x03ff);
}



/***************************************************************************

  Display refresh

***************************************************************************/

/* mix & blend the paletted 16-bit tile and sprite bitmaps into an RGB 32-bit bitmap */
static void blendbitmaps(
		struct mame_bitmap *dest,struct mame_bitmap *src1,struct mame_bitmap *src2,struct mame_bitmap *src3,
		int sx,int sy,const struct rectangle *clip)
{
	int ox;
	int oy;
	int ex;
	int ey;

	/* check bounds */
	ox = sx;
	oy = sy;

	ex = sx + src1->width - 1;
	if (sx < 0) sx = 0;
	if (sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width - 1;
	if (ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;

	ey = sy + src1->height - 1;
	if (sy < 0) sy = 0;
	if (sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height - 1;
	if (ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	{
		pen_t *paldata = Machine->pens;
		UINT32 *end;

		UINT16 *sd1 = ((UINT16 *)src1->line[0]);								/* source data   */
		UINT16 *sd2 = ((UINT16 *)src2->line[0]);
		UINT16 *sd3 = ((UINT16 *)src3->line[0]);

		int sw = ex-sx+1;														/* source width  */
		int sh = ey-sy+1;														/* source height */
		int sm = ((UINT16 *)src1->line[1]) - ((UINT16 *)src1->line[0]);			/* source modulo */

		UINT32 *dd = ((UINT32 *)dest->line[sy]) + sx;							/* dest data     */
		int dm = ((UINT32 *)dest->line[1]) - ((UINT32 *)dest->line[0]);			/* dest modulo   */

		sd1 += (sx-ox);
		sd1 += sm * (sy-oy);
		sd2 += (sx-ox);
		sd2 += sm * (sy-oy);
		sd3 += (sx-ox);
		sd3 += sm * (sy-oy);

		sm -= sw;
		dm -= sw;

		while (sh)
		{

#define BLENDPIXEL(x)	if (sd3[x]) {														\
							if (sd2[x]) {													\
								dd[x] = paldata[sd2[x] | 0x0400] + paldata[sd3[x]];			\
							} else {														\
								dd[x] = paldata[sd1[x] | 0x0400] + paldata[sd3[x]];			\
							}																\
						} else {															\
							if (sd2[x]) {													\
								if (sd2[x] & 0x0800) {										\
									dd[x] = paldata[sd1[x] | 0x0400] + paldata[sd2[x]];		\
								} else {													\
									dd[x] = paldata[sd2[x]];								\
								}															\
							} else {														\
								dd[x] = paldata[sd1[x]];									\
							}																\
						}

			end = dd + sw;
			while (dd <= end - 8)
			{
				BLENDPIXEL(0);
				BLENDPIXEL(1);
				BLENDPIXEL(2);
				BLENDPIXEL(3);
				BLENDPIXEL(4);
				BLENDPIXEL(5);
				BLENDPIXEL(6);
				BLENDPIXEL(7);
				dd += 8;
				sd1 += 8;
				sd2 += 8;
				sd3 += 8;
			}
			while (dd < end)
			{
				BLENDPIXEL(0);
				dd++;
				sd1++;
				sd2++;
				sd3++;
			}
			dd += dm;
			sd1 += sm;
			sd2 += sm;
			sd3 += sm;
			sh--;

#undef BLENDPIXEL

		}
	}
}

/* sprite format:
 *
 *  word        bit                 usage
 * --------+-fedcba9876543210-+----------------
 *    0    | ---------------x | flip x
 *         | --------------x- | flip y
 *         | -------------x-- | enable
 *         | ----------x----- | blend
 *         | --------xx------ | sprite-tile priority
 *    1    | xxxxxxxxxxxxxxxx | number
 *    2    | --------xxxx---- | palette
 *         | --------------xx | size: 8x8, 16x16, 32x32, 64x64
 *    3    | xxxxxxxxxxxxxxxx | y position
 *    4    | xxxxxxxxxxxxxxxx | x position
 *    5,6,7|                  | unused
 */

#define NUM_SPRITES 256

static void draw_sprites(struct mame_bitmap *bitmap_bg, struct mame_bitmap *bitmap_fg, struct mame_bitmap *bitmap_sp, const struct rectangle *cliprect)
{
	const UINT8 layout[8][8] =
	{
		{ 0, 1, 4, 5,16,17,20,21},
		{ 2, 3, 6, 7,18,19,22,23},
		{ 8, 9,12,13,24,25,28,29},
		{10,11,14,15,26,27,30,31},
		{32,33,36,37,48,49,52,53},
		{34,35,38,39,50,51,54,55},
		{40,41,44,45,56,57,60,61},
		{42,43,46,47,58,59,62,63}
	};

	const struct GfxElement *gfx = Machine->gfx[3];
	struct mame_bitmap *bitmap = bitmap_bg;
	const UINT16 *source = (NUM_SPRITES - 1) * 8 + spriteram16;
	const UINT8 blend_support = (bitmap_fg && bitmap_sp);
	int count = NUM_SPRITES;

	/* draw all sprites from front to back */
	while (count--)
	{
		UINT32 attributes = source[0];
		UINT32 priority_mask;
		int col,row;

		if (attributes & 0x04)
		{
			UINT32 priority = (attributes >> 6) & 3;
			UINT32 flipx = (attributes & 1);
			UINT32 flipy = (attributes & 2);

			UINT32 color = source[2];
			UINT32 sizex = 1 << ((color >> 0) & 3);						/* 1,2,4,8 */
			UINT32 sizey = 1 << ((color >> gaiden_sprite_sizey) & 3);	/* 1,2,4,8 */

			/* raiga needs something like this */
			UINT32 number = (source[1] & (sizex > 2 ? 0x7ff8 : 0x7ffc));

			int ypos = (source[3] + spr_offset_y) & 0x01ff;
			int xpos = source[4] & 0x01ff;

			if (!blend_support && (attributes & 0x20) && (cpu_getcurrentframe() & 1))
				goto skip_sprite;

			color = (color >> 4) & 0x0f;

			/* wraparound */
			if (xpos >= 256)
				xpos -= 512;
			if (ypos >= 256)
				ypos -= 512;

			if (flip_screen)
			{
				flipx = !flipx;
				flipy = !flipy;

				xpos = 256 - (8 * sizex) - xpos;
				ypos = 256 - (8 * sizey) - ypos;

				if (xpos <= -256)
					xpos += 512;
				if (ypos <= -256)
					ypos += 512;
			}

			/* bg: 1; fg:2; text: 4 */
			switch( priority )
			{
				default:
				case 0x0: priority_mask = 0;					break;
				case 0x1: priority_mask = 0xf0;					break;	/* obscured by text layer */
				case 0x2: priority_mask = 0xf0 | 0xcc;			break;	/* obscured by foreground */
				case 0x3: priority_mask = 0xf0 | 0xcc | 0xaa;	break;	/* obscured by bg and fg  */
			}

			/* blending */
			if (blend_support && (attributes & 0x20))
			{
				color |= 0x80;

				for (row = 0; row < sizey; row++)
				{
					for (col = 0; col < sizex; col++)
					{
						int sx = xpos + 8 * (flipx ? (sizex - 1 - col) : col);
						int sy = ypos + 8 * (flipy ? (sizey - 1 - row) : row);

						pdrawgfx(bitmap_sp, gfx,
							number + layout[row][col],
							color,
							flipx, flipy,
							sx, sy,
							cliprect, TRANSPARENCY_PEN, 0,
							priority_mask);
					}
				}
			}
			else
			{
				if (blend_support)
					bitmap = (priority >= 2) ? bitmap_bg : bitmap_fg;

				for (row = 0; row < sizey; row++)
				{
					for (col = 0; col < sizex; col++)
					{
						int sx = xpos + 8 * (flipx ? (sizex - 1 - col) : col);
						int sy = ypos + 8 * (flipy ? (sizey - 1 - row) : row);

						pdrawgfx(bitmap, gfx,
							number + layout[row][col],
							color,
							flipx, flipy,
							sx, sy,
							cliprect, TRANSPARENCY_PEN, 0,
							priority_mask);
					}
				}
			}
		}
skip_sprite:
		source -= 8;
	}
}


/* sprite format:
 *
 *  word        bit                 usage
 * --------+-fedcba9876543210-+----------------
 *    0    | --------xxxxxxxx | sprite code (lower bits)
 *         | ---xxxxx-------- | unused ?
 *    1    | --------xxxxxxxx | y position
 *         | ------x--------- | unused ?
 *    2    | --------xxxxxxxx | x position
 *         | -------x-------- | unused ?
 *    3    | -----------xxxxx | sprite code (upper bits)
 *         | ----------x----- | sprite-tile priority
 *         | ---------x------ | flip x
 *         | --------x------- | flip y
 * 0x400   |-------------xxxx | color
 *         |---------x------- | x position (high bit)
 */

static void drgnbowl_draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int i, code, color, x, y, flipx, flipy, priority_mask;

	for( i = 0; i < 0x800/2; i += 4 )
	{
		code = (spriteram16[i + 0] & 0xff) | ((spriteram16[i + 3] & 0x1f) << 8);
		y = 256 - (spriteram16[i + 1] & 0xff) - 12;
		x = spriteram16[i + 2] & 0xff;
		color = (spriteram16[(0x800/2) + i] & 0x0f);
		flipx = spriteram16[i + 3] & 0x40;
		flipy = spriteram16[i + 3] & 0x80;

		if(spriteram16[(0x800/2) + i] & 0x80)
			x -= 256;

		x += 256;

		if(spriteram16[i + 3] & 0x20)
			priority_mask = 0xf0 | 0xcc; /* obscured by foreground */
		else
			priority_mask = 0;

		pdrawgfx(bitmap,Machine->gfx[3],
				code,
				color,flipx,flipy,x,y,
				cliprect,
				TRANSPARENCY_PEN,15,
				priority_mask);

		/* wrap x*/
		pdrawgfx(bitmap,Machine->gfx[3],
				code,
				color,flipx,flipy,x-512,y,
				cliprect,
				TRANSPARENCY_PEN,15,
				priority_mask);

	}
}

VIDEO_UPDATE( gaiden )
{
	fillbitmap(priority_bitmap,                    0, cliprect);
	fillbitmap(bitmap,          Machine->pens[0x200], cliprect);

	tilemap_draw(bitmap, cliprect, background, 0, 1);
	tilemap_draw(bitmap, cliprect, foreground, 0, 2);
	tilemap_draw(bitmap, cliprect, text_layer, 0, 4);

	draw_sprites(bitmap, NULL, NULL, cliprect);
}

VIDEO_UPDATE( raiga )
{
	fillbitmap(priority_bitmap,    0, cliprect);

	fillbitmap(tile_bitmap_bg, 0x200, cliprect);
	fillbitmap(tile_bitmap_fg,     0, cliprect);
	fillbitmap(sprite_bitmap,      0, cliprect);

	/* draw tilemaps into a 16-bit bitmap */
	tilemap_draw(tile_bitmap_bg, cliprect,background, 0, 1);
	tilemap_draw(tile_bitmap_fg, cliprect,foreground, 0, 2);
	/* draw the blended tiles at a lower priority
	   so sprites covered by them will still be drawn */
	tilemap_draw(tile_bitmap_fg, cliprect,foreground, 1, 0);
	tilemap_draw(tile_bitmap_fg, cliprect,text_layer, 0, 4);

	/* draw sprites into a 16-bit bitmap */
	draw_sprites(tile_bitmap_bg, tile_bitmap_fg, sprite_bitmap, cliprect);

	/* mix & blend the tilemaps and sprites into a 32-bit bitmap */
	blendbitmaps(bitmap, tile_bitmap_bg, tile_bitmap_fg, sprite_bitmap, 0, 0, cliprect);
}

VIDEO_UPDATE( drgnbowl )
{
	fillbitmap(priority_bitmap, 0, cliprect);

	tilemap_draw(bitmap, cliprect, background, 0, 1);
	tilemap_draw(bitmap, cliprect, foreground, 0, 2);
	tilemap_draw(bitmap, cliprect, text_layer, 0, 4);
	drgnbowl_draw_sprites(bitmap, cliprect);
}
