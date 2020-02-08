/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"


static struct tilemap *tilemap;
static int char_bank,palette_bank,flipscreen,gfxctrl;

UINT8 *flstory_scrlram;


static void get_tile_info(int tile_index)
{
	int code = videoram[tile_index*2];
	int attr = videoram[tile_index*2+1];
	int tile_number = code + ((attr & 0xc0) << 2) + 0x400 + 0x800 * char_bank;
	int flags = ((attr & 0x08) ? TILE_FLIPX : 0) | ((attr & 0x10) ? TILE_FLIPY : 0);
/*	tile_info.priority = (attr & 0x20) >> 5;*/
	SET_TILE_INFO(
			0,
			tile_number,
			attr & 0x0f,
			flags)
}

static void rumba_get_tile_info(int tile_index)
{
	int code = videoram[tile_index*2];
	int attr = videoram[tile_index*2+1];
	int tile_number = code + ((attr & 0xc0) << 2) + 0x400 + 0x800 * char_bank;
	int col = (attr & 0x0f);
/*	tile_info.priority = (attr & 0x20) >> 5; */
	SET_TILE_INFO(
			0,
			tile_number,
			col,
			0)
}

VIDEO_START( flstory )
{
    tilemap = tilemap_create( get_tile_info,tilemap_scan_rows,TILEMAP_SPLIT,8,8,32,32 );
/*	tilemap_set_transparent_pen( tilemap,15 );*/
	tilemap_set_transmask(tilemap,0,0x3fff,0xc000);
	tilemap_set_scroll_cols(tilemap,32);

	paletteram = auto_malloc(0x200);
	paletteram_2 = auto_malloc(0x200);
	return video_start_generic();
}

VIDEO_START( rumba )
{
	tilemap = tilemap_create( rumba_get_tile_info,tilemap_scan_rows,TILEMAP_SPLIT,8,8,32,32 );
	tilemap_set_transmask(tilemap,0,0x3fff,0xc000);
	tilemap_set_scroll_cols(tilemap, 32);

	paletteram = auto_malloc(0x200);
	paletteram_2 = auto_malloc(0x200);
	return video_start_generic();
}

WRITE_HANDLER( flstory_videoram_w )
{
	videoram[offset] = data;
	tilemap_mark_tile_dirty(tilemap,offset/2);
}

WRITE_HANDLER( flstory_palette_w )
{
	if (offset & 0x100)
		paletteram_xxxxBBBBGGGGRRRR_split2_w((offset & 0xff) + (palette_bank << 8),data);
	else
		paletteram_xxxxBBBBGGGGRRRR_split1_w((offset & 0xff) + (palette_bank << 8),data);
}

READ_HANDLER( flstory_palette_r )
{
	if (offset & 0x100)
		return paletteram_2[ (offset & 0xff) + (palette_bank << 8) ];
	else
		return paletteram  [ (offset & 0xff) + (palette_bank << 8) ];
}

WRITE_HANDLER( flstory_gfxctrl_w )
{
	if (gfxctrl == data)
		return;
	gfxctrl = data;

	flipscreen = (~data & 0x01);
	char_bank = (data & 0x10) >> 4;
	palette_bank = (data & 0x20) >> 5;

	flip_screen_set(flipscreen);

/*usrintf_showmessage("%04x: gfxctrl = %02x\n",activecpu_get_pc(),data);*/

}

READ_HANDLER( flstory_scrlram_r )
{
	return flstory_scrlram[offset];
}

WRITE_HANDLER( flstory_scrlram_w )
{
	flstory_scrlram[offset] = data;
	tilemap_set_scrolly(tilemap, offset, data );
}

WRITE_HANDLER( rumba_gfxctrl_w )
{
	if (gfxctrl == data)
		return;
	gfxctrl = data;

	palette_bank = (data & 0x20) >> 5;

	if (data & 0x04)
	{
		flipscreen = (data & 0x01);
		flip_screen_set(flipscreen);
	}

/* usrintf_showmessage("%04x: gfxctrl = %02x\n",activecpu_get_pc(),data); */

}

/***************************************************************************

  Draw the game screen in the given mame_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

void flstory_draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int pri)
{
	int i;

	for (i = 0; i < 0x20; i++)
	{
		int pr = spriteram[spriteram_size-1 -i];
		int offs = (pr & 0x1f) * 4;

		if ((pr & 0x80) == pri)
		{
			int code,sx,sy,flipx,flipy;

			code = spriteram[offs+2] + ((spriteram[offs+1] & 0x30) << 4);
			sx = spriteram[offs+3];
			sy = spriteram[offs+0];

			if (flipscreen)
			{
				sx = (240 - sx) & 0xff ;
				sy = sy - 1 ;
			}
			else
				sy = 240 - sy - 1 ;

			flipx = ((spriteram[offs+1]&0x40)>>6)^flipscreen;
			flipy = ((spriteram[offs+1]&0x80)>>7)^flipscreen;

			drawgfx(bitmap,Machine->gfx[1],
					code,
					spriteram[offs+1] & 0x0f,
					flipx,flipy,
					sx,sy,
					cliprect,TRANSPARENCY_PEN,15);
			/* wrap around */
			if (sx > 240)
				drawgfx(bitmap,Machine->gfx[1],
						code,
						spriteram[offs+1] & 0x0f,
						flipx,flipy,
						sx-256,sy,
						cliprect,TRANSPARENCY_PEN,15);
		}
	}
}

VIDEO_UPDATE( flstory )
{
	int offs;

	tilemap_draw(bitmap,cliprect,tilemap,TILEMAP_BACK,0);
	flstory_draw_sprites(bitmap,cliprect,0x00);
	tilemap_draw(bitmap,cliprect,tilemap,TILEMAP_FRONT,0);
	flstory_draw_sprites(bitmap,cliprect,0x80);


	for (offs = videoram_size - 2;offs >= 0;offs -= 2)
	{
		if (videoram[offs + 1] & 0x20)
		{
			int sx,sy,code;

			sx = (offs/2)%32;
			sy = (offs/2)/32;
			sy = sy*8 - flstory_scrlram[sx];
			sx = sx * 8;

			if (flipscreen)
			{
				sx = 248-sx;
				sy = 248-sy;
			}
			code = videoram[offs] + ((videoram[offs + 1] & 0xc0) << 2) + 0x400 + 0x800 * char_bank;

			drawgfx(bitmap,Machine->gfx[0],
				code,
				(videoram[offs + 1] & 0x0f),
				( ( videoram[offs + 1] & 0x08 ) >> 3 ) ^ flipscreen,
				( ( videoram[offs + 1] & 0x10 ) >> 4 ) ^ flipscreen,
				sx,sy & 0xff,
				cliprect,TRANSPARENCY_PEN,15);
		}
	}

}

void rumba_draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int i;

	for (i = 0; i < 0x20; i++)
	{
		int pr = spriteram[spriteram_size-1 -i];
		int offs = (pr & 0x1f) * 4;

		/* if ((pr & 0x80) == pri) */
		{
			int code,sx,sy,flipx,flipy;

			code = spriteram[offs+2] + ((spriteram[offs+1] & 0x20) << 3);
			sx = spriteram[offs+3];
			sy = spriteram[offs+0];

			if (flipscreen)
			{
				sx = (240 - sx + 1) & 0xff ;
				sy = sy + 1 ;
			}
			else
				sy = 240 - sy + 1 ;

			flipx = ((spriteram[offs+1]&0x40)>>6)^flipscreen;
			flipy = ((spriteram[offs+1]&0x80)>>7)^flipscreen;

			drawgfx(bitmap,Machine->gfx[1],
					code,
					spriteram[offs+1] & 0x0f,
					flipx,flipy,
					sx,sy,
					cliprect,TRANSPARENCY_PEN,15);
			/* wrap around */
			if (sx > 240)
				drawgfx(bitmap,Machine->gfx[1],
						code,
						spriteram[offs+1] & 0x0f,
						flipx,flipy,
						sx-256,sy,
						cliprect,TRANSPARENCY_PEN,15);
		}
	}
}


VIDEO_UPDATE( rumba )
{
	tilemap_draw(bitmap,cliprect,tilemap,TILEMAP_BACK,0);
	rumba_draw_sprites(bitmap,cliprect);
	tilemap_draw(bitmap,cliprect,tilemap,TILEMAP_FRONT,0);
	rumba_draw_sprites(bitmap,cliprect);

}
