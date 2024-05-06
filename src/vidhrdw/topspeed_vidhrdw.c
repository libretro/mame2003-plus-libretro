#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/taitoic.h"

data16_t *topspeed_spritemap;
data16_t *topspeed_raster_ctrl;

/****************************************************************************/

VIDEO_START( topspeed )
{
	/* (chips, gfxnum, x_offs, y_offs, y_invert, opaque, dblwidth) */
	if (PC080SN_vh_start(2,1,0,8,0,0,0))
		return 1;

	return 0;
}


/********************************************************************************
                                     SPRITES

	Layout 8 bytes per sprite
	-------------------------

	+0x00   xxxxxxx. ........   Zoom Y
	        .......x xxxxxxxx   Y

	+0x02   x....... ........   Flip Y
	        ........ .xxxxxxx   Zoom X

	+0x04   x....... ........   Priority
	        .x...... ........   Flip X
	        ..x..... ........   Unknown
	        .......x xxxxxxxx   X

	+0x06   xxxxxxxx ........   Color
	        ........ xxxxxxxx   Tile number

********************************************************************************/

void topspeed_draw_sprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	int offs,map_offset,x,y,curx,cury,sprite_chunk;
	data16_t *spritemap = topspeed_spritemap;
	UINT16 data,tilenum,code,color;
	UINT8 flipx,flipy,priority,bad_chunks;
	UINT8 j,k,px,py,zx,zy,zoomx,zoomy;
	int primasks[2] = {0xff00,0xfffc};	/* Sprites are over bottom layer or under top layer */

	/* Most of spriteram is not used by the 68000: rest is scratch space for the h/w perhaps ? */

	for (offs = 0; offs < spriteram_size / 2; offs += 4)
	{
		data = spriteram16[offs+2];

		tilenum = spriteram16[offs+3] & 0xff;
		color = (spriteram16[offs+3] & 0xff00) >> 8;
		flipx = (data & 0x4000) >> 14;
		flipy = (spriteram16[offs+1] & 0x8000) >> 15;
		x = data & 0x1ff;
		y = spriteram16[offs] & 0x1ff;
		zoomx = (spriteram16[offs+1]& 0x7f);
		zoomy = (spriteram16[offs] & 0xfe00) >> 9;
		priority = (data & 0x8000) >> 15;
/*		unknown = (data & 0x2000) >> 13;*/

		/* End of sprite list */
		if (y == 0x180) break;

		map_offset = tilenum << 7;

		zoomx += 1;
		zoomy += 1;

		y += 3 + (128-zoomy);

		/* treat coords as signed */
		if (x > 0x140) x -= 0x200;
		if (y > 0x140) y -= 0x200;

		bad_chunks = 0;

		for (sprite_chunk = 0;sprite_chunk < 128;sprite_chunk++)
		{
			k = sprite_chunk % 8;   /* 8 sprite chunks per row */
			j = sprite_chunk / 8;   /* 16 rows */

			/* pick tiles back to front for x and y flips */
			px = (flipx) ? (7-k) : (k);
			py = (flipy) ? (15-j) : (j);

			code = spritemap[map_offset + (py<<3) + px];

			if (code & 0x8000)
			{
				bad_chunks += 1;
				continue;
			}

			curx = x + ((k*zoomx)/8);
			cury = y + ((j*zoomy)/16);

			zx = x + (((k+1)*zoomx)/8) - curx;
			zy = y + (((j+1)*zoomy)/16) - cury;

			pdrawgfxzoom(bitmap,Machine->gfx[0],
					code,
					color,
					flipx,flipy,
					curx,cury,
					cliprect,TRANSPARENCY_PEN,0,
					zx<<12,zy<<13,
					primasks[priority]);
		}

		if (bad_chunks)
log_cb(RETRO_LOG_DEBUG, LOGPRE "Sprite number %04x had %02x invalid chunks\n",tilenum,bad_chunks);
	}
}


/***************************************************************************/

VIDEO_UPDATE( topspeed )
{
	UINT8 layer[4];

#ifdef MAME_DEBUG
	static UINT8 dislayer[5];
#endif

#ifdef MAME_DEBUG
	if (keyboard_pressed_memory (KEYCODE_V))
	{
		dislayer[0] ^= 1;
		usrintf_showmessage("bg: %01x",dislayer[0]);
	}

	if (keyboard_pressed_memory (KEYCODE_B))
	{
		dislayer[1] ^= 1;
		usrintf_showmessage("fg: %01x",dislayer[1]);
	}

	if (keyboard_pressed_memory (KEYCODE_N))
	{
		dislayer[2] ^= 1;
		usrintf_showmessage("bg2: %01x",dislayer[2]);
	}

	if (keyboard_pressed_memory (KEYCODE_M))
	{
		dislayer[3] ^= 1;
		usrintf_showmessage("fg2: %01x",dislayer[3]);
	}

	if (keyboard_pressed_memory (KEYCODE_C))
	{
		dislayer[4] ^= 1;
		usrintf_showmessage("sprites: %01x",dislayer[4]);
	}
#endif

	PC080SN_tilemap_update();

	/* Tilemap layer priority seems hardwired (the order is odd, too) */
	layer[0] = 1;
	layer[1] = 0;
	layer[2] = 1;
	layer[3] = 0;

	fillbitmap(priority_bitmap,0,cliprect);
	fillbitmap(bitmap, Machine->pens[0], cliprect);

#ifdef MAME_DEBUG
	if (dislayer[3]==0)
#endif
	PC080SN_tilemap_draw(bitmap,cliprect,1,layer[0],TILEMAP_IGNORE_TRANSPARENCY,1);

#ifdef MAME_DEBUG
	if (dislayer[2]==0)
#endif
	PC080SN_tilemap_draw_special(bitmap,cliprect,1,layer[1],0,2,topspeed_raster_ctrl);

#ifdef MAME_DEBUG
	if (dislayer[1]==0)
#endif
 	PC080SN_tilemap_draw_special(bitmap,cliprect,0,layer[2],0,4,topspeed_raster_ctrl + 0x100);

#ifdef MAME_DEBUG
	if (dislayer[0]==0)
#endif
	PC080SN_tilemap_draw(bitmap,cliprect,0,layer[3],0,8);

#ifdef MAME_DEBUG
	if (dislayer[4]==0)
#endif

	topspeed_draw_sprites(bitmap,cliprect);

{
	int x,y,i;

	char gear_high[] = "HI";
	char gear_low[]  = "LO";

	int in = readinputport( 3 );

	/* draw on the original game. */

	x = Machine->visible_area.min_x + 2;
	y = Machine->visible_area.max_y - 8;

	for (i = 0; i < 2; i++)
	{
		drawgfx(bitmap,Machine->uifont,
				(in & 0x10) ? gear_low[i] : gear_high[i],
				UI_COLOR_NORMAL,
				0,0,
				x,y,
				cliprect,TRANSPARENCY_NONE,0);

		x += Machine->uifontwidth;
	}
}

}
