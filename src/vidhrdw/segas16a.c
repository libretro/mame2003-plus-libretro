/***************************************************************************

	Sega System 16A hardware

***************************************************************************/

#include "driver.h"
#include "segaic16.h"


#define PRINT_UNUSUAL_MODES		(0)


/*************************************
 *
 *	Statics
 *
 *************************************/

static struct tilemap *textmap;

static UINT8 draw_enable;
static UINT8 screen_flip;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static void get_tile_info(int tile_index);
static void get_text_info(int tile_index);



/*************************************
 *
 *	Video startup
 *
 *************************************/

VIDEO_START( system16a )
{
	/* create the tilemap for the text layer */
	textmap = tilemap_create(get_text_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8,8, 64,28);
	if (!textmap)
		return 1;
	
	/* configure it */
	tilemap_set_transparent_pen(textmap, 0);
	tilemap_set_scrolldx(textmap, -24*8, -24*8);
	tilemap_set_scrollx(textmap, 0, 0);

	/* create the tilemaps for the bg/fg layers */
	if (!segaic16_init_virtual_tilemaps(8, get_tile_info))
		return 1;

	/* initialize globals */
	draw_enable = 1;
	screen_flip = 0;

	/* compute palette info */
	segaic16_init_palette();
	return 0;
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
	??-?------------ Unknown
	--b------------- Bank
	----ccccccc----- Palette (0-127)
	----nnnnnnnnnnnn Tile index (0-4095)
*/
	UINT16 data = segaic16_tileram[segaic16_tilemap_page * (64*32) + tile_index];
	int bank = (data >> 13) & 1;
	int color = (data >> 5) & 0x7f;
	int code = data & 0xfff;

	SET_TILE_INFO(0, bank * 0x1000 + code, color, 0);
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
	?????----------- Unknown
	-----ccc-------- Palette (0-7)
	--------nnnnnnnn Tile index (0-255)
*/
	UINT16 data = segaic16_textram[tile_index];
	int color = (data >> 8) & 0x07;
	int code = data & 0xff;

	SET_TILE_INFO(0, code, color, 0);
}



/*************************************
 *
 *	Miscellaneous setters
 *
 *************************************/

void system16a_set_draw_enable(int enable)
{
	force_partial_update(cpu_getscanline());
	draw_enable = enable;
}


void system16a_set_screen_flip(int flip)
{
	force_partial_update(cpu_getscanline());
	screen_flip = (flip != 0);
	tilemap_set_flip(NULL, flip ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0);
}



/*************************************
 *
 *	Tilemap accessors
 *
 *************************************/

WRITE16_HANDLER( system16a_textram_w )
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

static void system16a_draw_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int which, int flags, int priority)
{
	/* note that the scrolling for these games can only scroll as much as the top-left */
	/* page; in order to scroll beyond that they swap pages and reset the scroll value */
	UINT16 xscroll = segaic16_textram[0xff8/2 + which] & 0x1ff;
	UINT16 yscroll = segaic16_textram[0xf24/2 + which] & 0x0ff;
	UINT16 pages = segaic16_textram[0xe9e/2 - which];

	/* adjust scroll values and clamp to the appropriate number of bits */
	xscroll = (-320 + 8 - xscroll) & 0x3ff;
	yscroll = (-256 + yscroll) & 0x1ff;
	
	/* adjust the xscroll for flipped screen -- note that this is not good enough for */
	/* fantzone, but keeps things aligned in mjleague */
	if (screen_flip)
		xscroll = (xscroll - 17) & 0x3ff;
	
	/* pages are swapped along the X direction, and there are only 8 of them */
	pages = ((pages >> 4) & 0x0707) | ((pages << 4) & 0x7070);
	
	/* draw the tilemap */
	segaic16_draw_virtual_tilemap(bitmap, cliprect, pages, xscroll, yscroll, flags, priority);
}



/*************************************
 *
 *	Draw a single sprite
 *
 *************************************/

#define draw_pixel() 														\
	/* only draw if onscreen, not 0 or 15, and high enough priority */		\
	if (pix != 0 && pix != 15 && sprpri > pri[x])							\
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
	int bottom  = (data[0] >> 8) + 1;
	int top     = (data[0] & 0xff) + 1;
	int xpos    = (data[1] & 0x1ff) - 0xbd;
	int pitch   = (INT16)data[2];
	UINT16 addr = data[3];
	int bank    = (data[4] >> 4) & 0x7;
	int sprpri  = 1 << ((data[4] >> 0) & 0x3);
	int color   = 1024 + (((data[4] >> 8) & 0x3f) << 4);
	int x, y, pix, numbanks;
	UINT16 *spritedata;

	/* initialize the end address to the start address */
	data[7] = addr;

	/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
	if ((top >= bottom) || bank == 255)
		return;

	/* clamp to within the memory region size */
	numbanks = memory_region_length(REGION_GFX2) / 0x10000;
	if (numbanks)
		bank %= numbanks;
	spritedata = (UINT16 *)memory_region(REGION_GFX2) + 0x8000 * bank;
	
	/* adjust top/bottom position for screen flipping */
	if (screen_flip)
	{
		int temp = top;
		top = 224 - bottom;
		bottom = 224 - temp;
	}

	/* for the non-flipped case, we start one row ahead */
	
	/* note that the System 16A sprites have a design flaw that allows the address */
	/* to carry into the flip flag, which is the topmost bit -- it is very important */
	/* to emulate this as the games compensate for it */
	if (!(addr & 0x8000))
		addr += pitch;

	/* loop from top to bottom */
	for (y = top; y < bottom; y++)
	{
		/* skip drawing if not within the cliprect */
		if (y >= cliprect->min_y && y <= cliprect->max_y)
		{
			UINT16 *dest = (UINT16 *)bitmap->line[y];
			UINT8 *pri = (UINT8 *)priority_bitmap->line[y];

			/* non-screen-flipped case */
			if (!screen_flip)
			{
				/* non-flipped case */
				if (!(addr & 0x8000))
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = xpos; x <= cliprect->max_x; )
					{
						UINT16 pixels = spritedata[++data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;
						pix = (pixels >>  8) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;
						pix = (pixels >>  4) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;
						pix = (pixels >>  0) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;

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
						UINT16 pixels = spritedata[--data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;
						pix = (pixels >>  4) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;
						pix = (pixels >>  8) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;
						pix = (pixels >> 12) & 0xf; if (x >= cliprect->min_x) draw_pixel(); x++;

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
			
			/* screen-flipped case */
			else
			{
				/* non-flipped case */
				if (!(addr & 0x8000))
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = 320 - xpos; x >= cliprect->min_x; )
					{
						UINT16 pixels = spritedata[++data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;
						pix = (pixels >>  8) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;
						pix = (pixels >>  4) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;
						pix = (pixels >>  0) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;

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
					for (x = 320 - xpos; x >= cliprect->min_x; )
					{
						UINT16 pixels = spritedata[--data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;
						pix = (pixels >>  4) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;
						pix = (pixels >>  8) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;
						pix = (pixels >> 12) & 0xf; if (x <= cliprect->max_x) draw_pixel(); x--;

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
		}

		/* advance a row */
		addr += pitch;
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
		if ((cursprite[0] >> 16) > 0xf0)
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

VIDEO_UPDATE( system16a )
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
	system16a_draw_layer(bitmap, cliprect, 1, 0 | TILEMAP_IGNORE_TRANSPARENCY, 0x00);

	/* draw background again, just to set the priorities on non-transparent pixels */
	system16a_draw_layer(NULL, cliprect, 1, 0, 0x01);

	/* draw foreground */
	system16a_draw_layer(bitmap, cliprect, 0, 0, 0x02);

	/* text layer */
	tilemap_draw(bitmap, cliprect, textmap, 0, 0x04);

	/* draw the sprites */
	draw_sprites(bitmap, cliprect);
}
