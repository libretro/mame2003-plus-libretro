/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

data8_t *superqix_videoram;
int pbillian_show_power;

static int gfxbank;
static struct mame_bitmap *bitmap1,*bitmap2;
static struct tilemap *bg_tilemap;

data8_t p_ram[0x200];
data8_t sq_bitmap_1[0xffff];// put the right size in at some point
data8_t sq_bitmap_2[0xffff];// put the right size in at some point 


data8_t *superqix_bitmapram  = sq_bitmap_1;
data8_t *superqix_bitmapram2 = sq_bitmap_2;

/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void pb_get_bg_tile_info(int tile_index)
{
	int attr = superqix_videoram[tile_index + 0x400];
	int code = superqix_videoram[tile_index] + 256 * (attr & 0x7);
	int color = (attr & 0xf0) >> 4;
	SET_TILE_INFO(0, code, color, 0)
}

static void sqix_get_bg_tile_info(int tile_index)
{
	int attr = superqix_videoram[tile_index + 0x400];
	int bank = (attr & 0x04) ? 0 : 1;
	int code = superqix_videoram[tile_index] + 256 * (attr & 0x03);
	int color = (attr & 0xf0) >> 4;

	if (bank) code += 1024 * gfxbank;

	SET_TILE_INFO(bank, code, color, TILE_SPLIT((attr & 0x08) >> 3))
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

VIDEO_START( pbillian )
{
	paletteram = p_ram;
	bg_tilemap = tilemap_create(pb_get_bg_tile_info, tilemap_scan_rows, TILEMAP_OPAQUE, 8, 8,32,32);

	if (!bg_tilemap)
		return 1;

	return 0;
}

VIDEO_START( superqix )
{
	paletteram = p_ram;
	bitmap1 = auto_bitmap_alloc(256, 256);
	bitmap2 = auto_bitmap_alloc(256, 256);
	bg_tilemap = tilemap_create(sqix_get_bg_tile_info, tilemap_scan_rows, TILEMAP_SPLIT, 8, 8, 32, 32);

	if (!bitmap1 || !bitmap2 || !bg_tilemap)
		return 1;

	tilemap_set_transmask(bg_tilemap,0,0xffff,0x0000); /* split type 0 is totally transparent in front half */
	tilemap_set_transmask(bg_tilemap,1,0x0001,0xfffe); /* split type 1 has pen 0 transparent in front half */

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( superqix_videoram_w )
{
	if (superqix_videoram[offset] != data)
	{
		superqix_videoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap, offset & 0x3ff);
	}
}

READ_HANDLER( superqix_bitmapram_r )
{
	return superqix_bitmapram[offset];
}

READ_HANDLER( superqix_bitmapram2_r )
{
	return superqix_bitmapram2[offset];
}

WRITE_HANDLER( superqix_bitmapram_w )
{
	
	if (superqix_bitmapram[offset] != data)
	{
		int x = 2 * (offset % 128);
		int y = offset / 128 + 16;

		superqix_bitmapram[offset] = data;

		plot_pixel(bitmap1, x,   y, Machine->pens[data >> 4]);
		plot_pixel(bitmap1, x+1, y, Machine->pens[data & 0x0f]);

	}

superqix_bitmapram[offset]=data;
}

WRITE_HANDLER( superqix_bitmapram2_w )
{
	if (data != superqix_bitmapram2[offset])
	{
		int x = 2 * (offset % 128);
		int y = offset / 128 + 16;

		superqix_bitmapram2[offset] = data;

		plot_pixel(bitmap2, x,   y, Machine->pens[data >> 4]);
		plot_pixel(bitmap2, x+1, y, Machine->pens[data & 0x0f]);
	}
superqix_bitmapram2[offset]=data;
}

WRITE_HANDLER( pbillian_0410_w )
{
	int bankaddress;
	UINT8 *rom = memory_region(REGION_CPU1);

	/*
	 -------0  ? [not used]
	 ------1-  coin counter 1
	 -----2--  coin counter 2
	 ----3---  rom 2 HI (reserved for ROM banking , not used)
	 ---4----  nmi enable/disable
	 --5-----  flip screen
	*/

	coin_counter_w(0,data & 0x02);
	coin_counter_w(1,data & 0x04);

	bankaddress = 0x10000 + ((data & 0x08) >> 3) * 0x4000;
	cpu_setbank(1,&rom[bankaddress]);

	interrupt_enable_w(0,data & 0x10);
	flip_screen_set(data & 0x20);
}

WRITE_HANDLER( superqix_0410_w )
{
	int bankaddress;
	UINT8 *rom = memory_region(REGION_CPU1);

	/* bits 0-1 select the tile bank */
	if (gfxbank != (data & 0x03))
	{
		gfxbank = data & 0x03;
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	/* bit 2 unused? (maybe space for one more gfx bank) */

	/* bit 3 enables NMI */
	interrupt_enable_w(offset,data & 0x08);

	/* bits 4-5 control ROM bank */
	bankaddress = 0x10000 + ((data & 0x30) >> 4) * 0x4000;
	cpu_setbank(1,&rom[bankaddress]);
}



/***************************************************************************

  Display refresh

***************************************************************************/

static void pb_draw_sprites( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	int offs;

	for (offs = 0; offs < spriteram_size; offs += 4)
	{
		int attr = spriteram[offs + 3];
		int code = ((spriteram[offs] & 0xfc) >> 2) + 64 * (attr & 0x0f);
		int color = (attr & 0xf0) >> 4;
		int sx = spriteram[offs + 1] + 256 * (spriteram[offs] & 0x01);
		int sy = spriteram[offs + 2];

		if (flip_screen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				code,
				color,
				flip_screen, flip_screen,
				sx, sy,
				cliprect, TRANSPARENCY_PEN, 0);
	}
}

static void sqix_draw_sprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	int offs;

	for (offs = 0; offs < spriteram_size; offs += 4)
	{
		int attr = spriteram[offs + 3];
		int code = spriteram[offs] + 256 * (attr & 0x01);
		int color = (attr & 0xf0) >> 4;
		int flipx = attr & 0x04;
		int flipy = attr & 0x08;
		int sx = spriteram[offs + 1];
		int sy = spriteram[offs + 2];

		if (flip_screen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[2],
				code,
				color,
				flipx, flipy,
				sx, sy,
				cliprect, TRANSPARENCY_PEN, 0);
	}
}

VIDEO_UPDATE( pbillian )
{
	tilemap_draw(bitmap, cliprect, bg_tilemap, 0, 0);
	pb_draw_sprites(bitmap,cliprect);

	if (pbillian_show_power)
	{
		static int last_power[2];
		int curr_power;

		curr_power = ((readinputport(4)&0x3f)*100)/0x3f;
		if (last_power[0] != curr_power)
		{
			usrintf_showmessage	("Power %d%%", curr_power);
			last_power[0] = curr_power;
		}

		curr_power = ((readinputport(6)&0x3f)*100)/0x3f;
		if (last_power[1] != curr_power)
		{
			usrintf_showmessage	("Power %d%%", curr_power);
			last_power[1] = curr_power;
		}
	}
}

VIDEO_UPDATE( superqix )
{
	tilemap_draw(bitmap, cliprect, bg_tilemap, TILEMAP_BACK, 0);
	copybitmap(bitmap,bitmap1,flip_screen,flip_screen,0,0,cliprect,TRANSPARENCY_PEN,0);
	sqix_draw_sprites(bitmap,cliprect);
	copybitmap(bitmap,bitmap2,flip_screen,flip_screen,0,0,cliprect,TRANSPARENCY_PEN,0);	/* not used? */
	tilemap_draw(bitmap, cliprect, bg_tilemap, TILEMAP_FRONT, 0);
}

