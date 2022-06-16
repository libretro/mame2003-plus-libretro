/***************************************************************************

	Sega System 16B hardware

***************************************************************************/

#include "driver.h"
#include "segaic16.h"
#include "system16.h"


#define PRINT_UNUSUAL_MODES		(0)


/*************************************
 *
 *	Statics
 *
 *************************************/

static struct tilemap *textmap;

static UINT8 tile_bank[2];

static const UINT8 default_banklist[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static const UINT8 alternate_banklist[] = { 255,255,255,255, 255,255,255,3, 255,255,255,2, 255,1,0,255 };
static const UINT8 *banklist;

static UINT8 draw_enable;
static UINT8 screen_flip;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static void get_tile_info(int tile_index);
static void get_tile_info_timscanr(int tile_index);
static void get_text_info(int tile_index);
static void get_text_info_timscanr(int tile_index);



/*************************************
 *
 *	Video startup
 *
 *************************************/

static int video_start_common(void (*tilecb)(int), void (*textcb)(int))
{
//	int pagenum;
	segaic16_tileram=sys16_tileram;
	segaic16_textram=sys16_textram;
	segaic16_spriteram=sys16_spriteram;

	/* create the tilemap for the text layer */
	textmap = tilemap_create(textcb, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8,8, 64,28);
	if (!textmap)
		return 1;

	/* configure it */
	tilemap_set_transparent_pen(textmap, 0);
	tilemap_set_scrolldx(textmap, -24*8, -24*8);
	tilemap_set_scrollx(textmap, 0, 0);

	/* create the tilemaps for the bg/fg layers */
	if (!segaic16_init_virtual_tilemaps(16, tilecb))
		return 1;

	/* initialize globals */
	draw_enable = 1;
	screen_flip = 0;

	/* for ROM boards without banking, set up the tile banks to be 0 and 1 */
	tile_bank[0] = 0;
	tile_bank[1] = 1;

	/* compute palette info */
	segaic16_init_palette();
	return 0;
}


VIDEO_START( system16b )
{
	return video_start_common(get_tile_info, get_text_info);
}


VIDEO_START( timscanr )
{
	return video_start_common(get_tile_info_timscanr, get_text_info_timscanr);
}



/*************************************
 *
 *	Tilemap callbacks
 *
 *************************************/

static void get_tile_info(int tile_index)
{
/*
	MSB          LSB
	p--------------- Priority flag
	-??------------- Unknown
	---b------------ Tile bank select (0-1)
	---ccccccc------ Palette (0-127)
	----nnnnnnnnnnnn Tile index (0-4095)
*/
	UINT16 data = segaic16_tileram[segaic16_tilemap_page * (64*32) + tile_index];
	int bank = tile_bank[(data >> 12) & 1];
	int color = (data >> 6) & 0x7f;
	int code = data & 0x0fff;

	SET_TILE_INFO(0, bank * 0x1000 + code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}


static void get_tile_info_timscanr(int tile_index)
{
/*
	MSB          LSB
	p--------------- Priority flag
	-??------------- Unknown
	---b------------ Tile bank select (0-1)
	----ccccccc----- Palette (0-127)
	----nnnnnnnnnnnn Tile index (0-4095)
*/
	UINT16 data = segaic16_tileram[segaic16_tilemap_page * (64*32) + tile_index];
	int bank = tile_bank[(data >> 12) & 1];
	int color = (data >> 5) & 0x7f;
	int code = data & 0x0fff;

	SET_TILE_INFO(0, bank * 0x1000 + code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}



/*************************************
 *
 *	Textmap callbacks
 *
 *************************************/

static void get_text_info(int tile_index)
{
/*
	MSB          LSB
	p--------------- Priority flag
	-???------------ Unknown
	----ccc--------- Palette (0-7)
	-------nnnnnnnnn Tile index (0-511)
*/
	UINT16 data = segaic16_textram[tile_index];
	int bank = tile_bank[0];
	int color = (data >> 9) & 0x07;
	int code = data & 0x1ff;

	SET_TILE_INFO(0, bank * 0x1000 + code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}


static void get_text_info_timscanr(int tile_index)
{
/*
	MSB          LSB
	p--------------- Priority flag
	-????----------- Unknown
	-----ccc-------- Palette (0-7)
	--------nnnnnnnn Tile index (0-255)
*/
	UINT16 data = segaic16_textram[tile_index];
	int bank = tile_bank[0];
	int color = (data >> 8) & 0x07;
	int code = data & 0xff;

	SET_TILE_INFO(0, bank * 0x1000 + code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}



/*************************************
 *
 *	Miscellaneous setters
 *
 *************************************/

void system16b_set_draw_enable(int enable)
{
	force_partial_update(cpu_getscanline());
	draw_enable = enable;
}


void system16b_set_screen_flip(int flip)
{
	force_partial_update(cpu_getscanline());
	screen_flip = flip;
}


void system16b_configure_sprite_banks(int use_default)
{
	if (use_default)
		banklist = default_banklist;
	else
		banklist = alternate_banklist;
}


void system16b_set_tile_bank(int which, int bank)
{
//	int i;

	if (bank != tile_bank[which])
	{
		force_partial_update(cpu_getscanline());
		tile_bank[which] = bank;

		/* mark all tilemaps dirty */
		tilemap_mark_all_tiles_dirty(NULL);
	}
}



/*************************************
 *
 *	Tilemap accessors
 *
 *************************************/

WRITE16_HANDLER( system16b_textram_w )
{
	/* certain ranges need immediate updates */
	if (offset >= 0xe80/2)
		force_partial_update(cpu_getscanline());

	COMBINE_DATA(&segaic16_textram[offset]);
	tilemap_mark_tile_dirty(textmap, offset);
}



/*************************************
 *
 *	Draw a single tilemap layer
 *
 *************************************/

static void draw_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int which, int flags, int priority)
{
	UINT16 xscroll, yscroll, pages;
	int x, y;

	/* get global values */
	xscroll = segaic16_textram[0xe98/2 + which];
	yscroll = segaic16_textram[0xe90/2 + which];
	pages = segaic16_textram[0xe80/2 + which];

	/* column AND row scroll */
	if ((xscroll & 0x8000) && (yscroll & 0x8000))
	{
		if (PRINT_UNUSUAL_MODES) printf("Column AND row scroll\n");

		/* loop over row chunks */
		for (y = cliprect->min_y & ~7; y <= cliprect->max_y; y += 8)
		{
			struct rectangle rowcolclip;

			/* adjust to clip this row only */
			rowcolclip.min_y = (y < cliprect->min_y) ? cliprect->min_y : y;
			rowcolclip.max_y = (y + 7 > cliprect->max_y) ? cliprect->max_y : y + 7;

			/* loop over column chunks */
			for (x = cliprect->min_x & ~15; x <= cliprect->max_x; x += 16)
			{
				UINT16 effxscroll, effyscroll;
				UINT16 effpages = pages;

				/* adjust to clip this column only */
				rowcolclip.min_x = (x < cliprect->min_x) ? cliprect->min_x : x;
				rowcolclip.max_x = (x + 15 > cliprect->max_x) ? cliprect->max_x : x + 15;

				/* get the effective scroll values */
				effxscroll = segaic16_textram[0xf80/2 + 0x40/2 * which + y/8];
				effyscroll = segaic16_textram[0xf06/2 + 0x40/2 * which + x/16];

				/* are we using an alternate? */
				if (effxscroll & 0x8000)
				{
					effxscroll = segaic16_textram[0xe9c/2 + which];
					effyscroll = segaic16_textram[0xe94/2 + which];
					effpages = segaic16_textram[0xe84/2 + which];
				}

				/* draw the chunk */
				effxscroll = (-320 - effxscroll) & 0x3ff;
				effyscroll = (-256 + effyscroll) & 0x1ff;
				segaic16_draw_virtual_tilemap(bitmap, &rowcolclip, effpages, effxscroll, effyscroll, flags, priority);
			}
		}
	}
	else if (yscroll & 0x8000)
	{
		if (PRINT_UNUSUAL_MODES) printf("Column scroll\n");

		/* loop over column chunks */
		for (x = cliprect->min_x & ~15; x <= cliprect->max_x; x += 16)
		{
			struct rectangle colclip = *cliprect;
			UINT16 effxscroll, effyscroll;

			/* adjust to clip this row only */
			colclip.min_x = (x < cliprect->min_x) ? cliprect->min_x : x;
			colclip.max_x = (x + 15 > cliprect->max_x) ? cliprect->max_x : x + 15;

			/* get the effective scroll values */
			effyscroll = segaic16_textram[0xf06/2 + 0x40/2 * which + x/16];

			/* draw the chunk */
			effxscroll = (-320 - xscroll) & 0x3ff;
			effyscroll = (-256 + effyscroll) & 0x1ff;
			segaic16_draw_virtual_tilemap(bitmap, &colclip, pages, effxscroll, effyscroll, flags, priority);
		}
	}
	else if (xscroll & 0x8000)
	{
		if (PRINT_UNUSUAL_MODES) printf("Row scroll\n");

		/* loop over row chunks */
		for (y = cliprect->min_y & ~7; y <= cliprect->max_y; y += 8)
		{
			struct rectangle rowclip = *cliprect;
			UINT16 effxscroll, effyscroll;
			UINT16 effpages = pages;

			/* adjust to clip this row only */
			rowclip.min_y = (y < cliprect->min_y) ? cliprect->min_y : y;
			rowclip.max_y = (y + 7 > cliprect->max_y) ? cliprect->max_y : y + 7;

			/* get the effective scroll values */
			effxscroll = segaic16_textram[0xf80/2 + 0x40/2 * which + y/8];
			effyscroll = yscroll;

			/* are we using an alternate? */
			if (effxscroll & 0x8000)
			{
				effxscroll = segaic16_textram[0xe9c/2 + which];
				effyscroll = segaic16_textram[0xe94/2 + which];
				effpages = segaic16_textram[0xe84/2 + which];
			}

			/* draw the chunk */
			effxscroll = (-320 - effxscroll) & 0x3ff;
			effyscroll = (-256 + effyscroll) & 0x1ff;
			segaic16_draw_virtual_tilemap(bitmap, &rowclip, effpages, effxscroll, effyscroll, flags, priority);
		}
	}
	else
	{
		xscroll = (-320 - xscroll) & 0x3ff;
		yscroll = (-256 + yscroll) & 0x1ff;
		segaic16_draw_virtual_tilemap(bitmap, cliprect, pages, xscroll, yscroll, flags, priority);
	}
}



/*************************************
 *
 *	Draw a single sprite
 *
 *************************************/

/*
	A note about zooming:

	The current implementation is a guess at how the hardware works. Hopefully
	we will eventually get some good tests run on the hardware to understand
	which rows/columns are skipped during a zoom operation.

	The ring sprites in hwchamp are excellent testbeds for the zooming.
*/

#define draw_pixel() 														\
	/* only draw if onscreen, not 0 or 15, and high enough priority */		\
	if (x >= cliprect->min_x && pix != 0 && pix != 15 && sprpri > pri[x])	\
	{																		\
		/* shadow/hilight mode? */											\
		if (color == 1024 + (0x3f << 4))									\
			dest[x] += (paletteram16[dest[x]] & 0x8000) ? 4096 : 2048;		\
																			\
		/* regular draw */													\
		else																\
			dest[x] = pix | color;											\
																			\
		/* always mark priority so no one else draws here */				\
		pri[x] = 0xff;														\
	}																		\


static void draw_one_sprite(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT16 *data)
{
	int bottom  = data[0] >> 8;
	int top     = data[0] & 0xff;
	int xpos    = (data[1] & 0x1ff) - 0xb8;
	int hide    = data[2] & 0x4000;
	int flip    = data[2] & 0x100;
	int pitch   = (INT8)(data[2] & 0xff);
	UINT16 addr = data[3];
	int bank    = banklist[(data[4] >> 8) & 0xf];
	int sprpri  = 1 << ((data[4] >> 6) & 0x3);
	int color   = 1024 + ((data[4] & 0x3f) << 4);
	int vzoom   = (data[5] >> 5) & 0x1f;
	int hzoom   = data[5] & 0x1f;
	int x, y, pix, numbanks;
	UINT16 *spritedata;

	/* initialize the end address to the start address */
	data[7] = addr;

	/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
	if (hide || (top >= bottom) || bank == 255)
		return;

	/* clamp to within the memory region size */
	numbanks = memory_region_length(REGION_GFX2) / 0x20000;
	if (numbanks)
		bank %= numbanks;
	spritedata = (UINT16 *)memory_region(REGION_GFX2) + 0x10000 * bank;

	/* reset the yzoom counter */
	data[5] &= 0x03ff;

	/* for the non-flipped case, we start one row ahead */
	if (!flip)
		addr += pitch;

	/* loop from top to bottom */
	for (y = top; y < bottom; y++)
	{
		/* skip drawing if not within the cliprect */
		if (y >= cliprect->min_y && y <= cliprect->max_y)
		{
			UINT16 *dest = (UINT16 *)bitmap->line[y];
			UINT8 *pri = (UINT8 *)priority_bitmap->line[y];
			int xacc = 0x20;

			/* non-flipped case */
			if (!flip)
			{
				/* start at the word before because we preincrement below */
				data[7] = addr - 1;
				for (x = xpos; x <= cliprect->max_x; )
				{
					UINT16 pixels = spritedata[++data[7]];

					/* draw four pixels */
					pix = (pixels >> 12) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;
					pix = (pixels >>  8) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;
					pix = (pixels >>  4) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;
					pix = (pixels >>  0) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;

					/* stop if the last pixel in the group was 0xf */
					if (pix == 15)
						break;
				}
			}

			/* flipped case */
			else
			{
				/* start at the word after because we predecrement below */
				data[7] = addr + pitch + 1;
				for (x = xpos; x <= cliprect->max_x; )
				{
					UINT16 pixels = spritedata[--data[7]];

					/* draw four pixels */
					pix = (pixels >>  0) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;
					pix = (pixels >>  4) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;
					pix = (pixels >>  8) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;
					pix = (pixels >> 12) & 0xf; if (xacc < 0x40) { draw_pixel(); x++; } else xacc -= 0x40; xacc += hzoom;

					/* stop if the last pixel in the group was 0xf */
					if (pix == 15)
						break;
				}
			}
		}

		/* advance a row */
		addr += pitch;

		/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
		data[5] += vzoom << 10;
		if (data[5] & 0x8000)
		{
			addr += pitch;
			data[5] &= ~0x8000;
		}
	}
}



/*************************************
 *
 *	Sprite drawing
 *
 *************************************/

static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT16 *cursprite;

	/* first scan forward to find the end of the list */
	for (cursprite = segaic16_spriteram; cursprite < segaic16_spriteram + 0x7ff/2; cursprite += 8)
		if (cursprite[2] & 0x8000)
			break;

	/* now scan backwards and render the sprites in order */
	for (cursprite -= 8; cursprite >= segaic16_spriteram; cursprite -= 8)
		draw_one_sprite(bitmap, cliprect, cursprite);
}



/*************************************
 *
 *	Video update
 *
 *************************************/

VIDEO_UPDATE( system16b )
{
	/* if no drawing is happening, fill with black and get out */
	if (!draw_enable)
	{
		fillbitmap(bitmap, get_black_pen(), cliprect);
		return;
	}

	/* reset priorities */
	fillbitmap(priority_bitmap, 0, cliprect);

	/* draw background opaquely first, not setting any priorities */
	draw_layer(bitmap, cliprect, 1, 0 | TILEMAP_IGNORE_TRANSPARENCY, 0x00);
	draw_layer(bitmap, cliprect, 1, 1 | TILEMAP_IGNORE_TRANSPARENCY, 0x00);

	/* draw background again, just to set the priorities on non-transparent pixels */
	draw_layer(NULL, cliprect, 1, 0, 0x01);
	draw_layer(NULL, cliprect, 1, 1, 0x02);

	/* draw foreground */
	draw_layer(bitmap, cliprect, 0, 0, 0x02);
	draw_layer(bitmap, cliprect, 0, 1, 0x04);

	/* text layer */
	tilemap_draw(bitmap, cliprect, textmap, 0, 0x04);
	tilemap_draw(bitmap, cliprect, textmap, 1, 0x08);

	/* draw the sprites */
	draw_sprites(bitmap, cliprect);
}
