/* vidhrdw/stlforce.c - see main driver for other notes */

#include "driver.h"

int which=0;
data16_t  sprites_buffer[0x400];

static struct tilemap *stlforce_bg_tilemap, *stlforce_mlow_tilemap, *stlforce_mhigh_tilemap, *stlforce_tx_tilemap;

extern data16_t *stlforce_bg_videoram, *stlforce_mlow_videoram, *stlforce_mhigh_videoram, *stlforce_tx_videoram;
extern data16_t *stlforce_bg_scrollram, *stlforce_mlow_scrollram, *stlforce_mhigh_scrollram, *stlforce_vidattrram;

extern data16_t *stlforce_spriteram;

WRITE16_HANDLER(sprites_commands_w)
{
	int i;
	/* TODO: I'm not convinced by this, from mwarr.cpp driver */
	if (which)
	{
		switch (data & 0xf)
		{
		case 0:
			/* clear sprites on screen */
			for (i = 0; i < 0x400; i++)
			{
				sprites_buffer[i] = 0;
			}
			which = 0;
			break;

		default:
			/* allow everything else to fall through, other games are writing other values */
		case 0xf: /* mwarr */
			/* refresh sprites on screen */
			for (i = 0; i < 0x400; i++)
			{
				sprites_buffer[i] = stlforce_spriteram[i];
			}
			break;

		case 0xd:
			/* keep sprites on screen */
			break;
		}
	}
	which ^= 1;
}

/* background, appears to be the bottom layer */
static void get_stlforce_bg_tile_info(int tile_index)
{
	int tileno,colour;
	tileno = stlforce_bg_videoram[tile_index] & 0x1fff;
	colour = (stlforce_bg_videoram[tile_index] & 0xe000) >> 13;
	SET_TILE_INFO(4,tileno,colour,0)
}

WRITE16_HANDLER( stlforce_bg_videoram_w )
{
		COMBINE_DATA(&stlforce_bg_videoram[offset]);
		tilemap_mark_tile_dirty(stlforce_bg_tilemap,offset);
}

WRITE16_HANDLER( stlforce_mlow_videoram_w )
{
		COMBINE_DATA(&stlforce_mlow_videoram[offset]);
		tilemap_mark_tile_dirty(stlforce_mlow_tilemap,offset);

}
WRITE16_HANDLER( stlforce_mhigh_videoram_w )
{
		COMBINE_DATA(&stlforce_mhigh_videoram[offset]);
		tilemap_mark_tile_dirty(stlforce_mhigh_tilemap,offset);
}

WRITE16_HANDLER( stlforce_tx_videoram_w )
{
		COMBINE_DATA(&stlforce_tx_videoram[offset]);
		tilemap_mark_tile_dirty(stlforce_tx_tilemap,offset);
}
/* middle layer, low */
static void get_stlforce_mlow_tile_info(int tile_index)
{
	int tileno,colour;
	tileno =  stlforce_mlow_videoram[tile_index] & 0x1fff;
	colour = (stlforce_mlow_videoram[tile_index] & 0xe000) >> 13;
	SET_TILE_INFO(3,tileno,colour,0)
}

/* middle layer, high */
static void get_stlforce_mhigh_tile_info(int tile_index)
{
	int tileno =  stlforce_mhigh_videoram[tile_index] & 0x1fff;
	int colour = (stlforce_mhigh_videoram[tile_index] & 0xe000) >> 13;
	SET_TILE_INFO(2,tileno,colour,0)
}

/* text layer, appears to be the top layer */
static void get_stlforce_tx_tile_info(int tile_index)
{
	int tileno =  stlforce_tx_videoram[tile_index] & 0x1fff;
	int colour = (stlforce_tx_videoram[tile_index] & 0xe000) >> 13;
	SET_TILE_INFO(1,tileno,colour,0)
}


int get_priority(const data16_t *source)
{
	return ((source[1] & 0x3c00) >> 10); /* Priority (1 = Low) */
}

/* the Steel Force type hardware uses an entirely different bit for priority and only appears to have 2 levels Mortal Race uses additional priorities */
int sforce_get_priority(const data16_t *source)
{
	switch (source[1] & 0x0030)
	{
	case 0x00:
		return 0x02;
	case 0x10:
		return 0x04;
	case 0x20:
		return 0x0c;
	case 0x30:
		return 0x0e;
	}

	return 0x00;
}

int spritexoffs=7;
static void draw_sprites( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	const data16_t *source = sprites_buffer + 0x400 - 4;
	const data16_t *finish = sprites_buffer;
	const struct GfxElement *gfx = Machine->gfx[0];
	int x, y, color, flipx, dy, pri, pri_mask, i;

	while (source >= finish)
	{
		/* draw sprite */
		if (source[0] & 0x0800)
		{
			y = 0x1ff - (source[0] & 0x01ff);
			x = (source[3] & 0x3ff) - spritexoffs;

			color = source[1] & 0x000f;
			flipx = source[1] & 0x0200;

			dy = (source[0] & 0xf000) >> 12;

			pri = sforce_get_priority(source); 
			pri_mask = ~((1 << (pri + 1)) - 1);     /* Above the first "pri" levels */

			for (i = 0; i <= dy; i++)
			{
				pdrawgfx(bitmap,gfx,
					source[2] + i,
					color,
					flipx, 0,
					x, y + i * 16,
					cliprect,TRANSPARENCY_PEN,0,
					pri_mask);

				/* wrap around x */
				pdrawgfx(bitmap,gfx,
					source[2] + i,
					color,
					flipx, 0,
					x - 1024, y + i * 16,
					cliprect,TRANSPARENCY_PEN,0,
					pri_mask);
				/* wrap around y */
				pdrawgfx(bitmap,gfx,
					source[2] + i,
					color,
					flipx, 0,
					x, y - 512 + i * 16,
					cliprect,TRANSPARENCY_PEN,0,
					pri_mask);

					/* wrap around x & y */
				pdrawgfx(bitmap,gfx,
					source[2] + i,
					color,
					flipx, 0,
					x - 1024, y - 512 + i * 16,
					cliprect,TRANSPARENCY_PEN,0,
					pri_mask);
			}
		}
		source -= 0x4;
	}
}

static void draw_sprites_old( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{

	const UINT16 *source = stlforce_spriteram+0x0;
	const UINT16 *finish = stlforce_spriteram+0x800;
	const struct GfxElement *gfx = Machine->gfx[0];

	while( source<finish )
	{
		int ypos = source[0]& 0x01ff;
		int attr = source[1]& 0x000f;
		int xpos = source[3]& 0x03ff;
		int num = (source[2] & 0x1fff);

		ypos = 512-ypos;

		drawgfx(
				bitmap,
				gfx,
				num,
				64+attr,
				0,0,
				xpos,ypos,
				cliprect,
				TRANSPARENCY_PEN,0
				);
		source += 0x4;
	}
}

VIDEO_UPDATE( stlforce )
{
	int i;
	fillbitmap(priority_bitmap, 0, cliprect);
	/* xscrolls - Steel Force clearly shows that each layer needs -1 scroll compared to the previous, do enable flags change this?  */

	if (BIT(stlforce_vidattrram[6], 0))
	{
		for (i = 0; i < 256; i++)
			tilemap_set_scrollx( stlforce_bg_tilemap, i, stlforce_bg_scrollram[i] + 18);
	}

	else
	{
		for (i = 0; i < 256; i++)
			tilemap_set_scrollx( stlforce_bg_tilemap, i, stlforce_bg_scrollram[0] + 18);
	}

	if (BIT(stlforce_vidattrram[6], 2))
	{
		for (i = 0; i < 256; i++)
			tilemap_set_scrollx( stlforce_mlow_tilemap, i, stlforce_mlow_scrollram[i] + 17 );
	}
	else
	{
		for (i = 0; i < 256; i++)
			tilemap_set_scrollx( stlforce_mlow_tilemap, i, stlforce_mlow_scrollram[0] + 17 );
	}

	if (BIT(stlforce_vidattrram[6], 4))
	{
		for (i = 0; i < 256; i++)
			tilemap_set_scrollx( stlforce_mhigh_tilemap, i, stlforce_mhigh_scrollram[i] + 16);
	}
	else
	{
		for (i = 0; i < 256; i++)
			tilemap_set_scrollx( stlforce_mhigh_tilemap, i, stlforce_mhigh_scrollram[0] + 16);
	}
	
	tilemap_set_scrollx( stlforce_bg_tilemap, 0, stlforce_vidattrram[0] + 15);

	tilemap_set_scrolly( stlforce_bg_tilemap, 0, stlforce_vidattrram[1]+1 );
	tilemap_set_scrolly( stlforce_mlow_tilemap, 0, stlforce_vidattrram[2] +1 );
	tilemap_set_scrolly( stlforce_mhigh_tilemap, 0, stlforce_vidattrram[3] +1 );
	tilemap_set_scrolly( stlforce_tx_tilemap, 0, stlforce_vidattrram[4] +1 );

	if (BIT(stlforce_vidattrram[5], 0))
		tilemap_draw(bitmap,cliprect,stlforce_bg_tilemap,0,0x01);

	if (BIT(stlforce_vidattrram[5], 1))
		tilemap_draw(bitmap,cliprect,stlforce_mlow_tilemap,0,0x02);

	if (BIT(stlforce_vidattrram[5], 2))
		tilemap_draw(bitmap,cliprect,stlforce_mhigh_tilemap,0,0x04);

	if (BIT(stlforce_vidattrram[5], 3))
			tilemap_draw(bitmap,cliprect,stlforce_tx_tilemap,0,0x10);

	if (BIT(stlforce_vidattrram[5], 4))
		draw_sprites(bitmap, cliprect);
}

VIDEO_START( stlforce )
{
	stlforce_bg_tilemap = tilemap_create(get_stlforce_bg_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE, 16, 16,64,16);

	stlforce_mlow_tilemap = tilemap_create(get_stlforce_mlow_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT, 16, 16,64,16);
	tilemap_set_transparent_pen(stlforce_mlow_tilemap,0);

	stlforce_mhigh_tilemap = tilemap_create(get_stlforce_mhigh_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT, 16, 16,64,16);
	tilemap_set_transparent_pen(stlforce_mhigh_tilemap,0);

	stlforce_tx_tilemap = tilemap_create(get_stlforce_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,32);
	tilemap_set_transparent_pen(stlforce_tx_tilemap,0);

	tilemap_set_scroll_rows(stlforce_bg_tilemap, 256);
	tilemap_set_scroll_rows(stlforce_mlow_tilemap, 256);
	tilemap_set_scroll_rows(stlforce_mhigh_tilemap, 256);
	return 0;
}
