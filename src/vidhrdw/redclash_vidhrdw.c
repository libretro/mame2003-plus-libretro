/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int star_speed;
static int gfxbank;

static struct tilemap *fg_tilemap;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  I'm using the same palette conversion as Lady Bug, but the Zero Hour
  schematics show a different resistor network.

***************************************************************************/
PALETTE_INIT( redclash )
{
	int i;

	for (i = 0;i < 32;i++)
	{
		int bit1,bit2,r,g,b;


		bit1 = (color_prom[i] >> 0) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		r = 0x47 * bit1 + 0x97 * bit2;
		bit1 = (color_prom[i] >> 2) & 0x01;
		bit2 = (color_prom[i] >> 6) & 0x01;
		g = 0x47 * bit1 + 0x97 * bit2;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x47 * bit1 + 0x97 * bit2;
		palette_set_color(i,r,g,b);
	}

	/* characters */
	for (i = 0;i < 8;i++)
	{
		colortable[4 * i] = 0;
		colortable[4 * i + 1] = i + 0x08;
		colortable[4 * i + 2] = i + 0x10;
		colortable[4 * i + 3] = i + 0x18;
	}

	/* sprites */
	for (i = 0;i < 4 * 8;i++)
	{
		int bit0,bit1,bit2,bit3;


		/* low 4 bits are for sprite n */
		bit0 = (color_prom[i + 32] >> 3) & 0x01;
		bit1 = (color_prom[i + 32] >> 2) & 0x01;
		bit2 = (color_prom[i + 32] >> 1) & 0x01;
		bit3 = (color_prom[i + 32] >> 0) & 0x01;
		colortable[i + 4 * 8] = 1 * bit0 + 2 * bit1 + 4 * bit2 + 8 * bit3;

		/* high 4 bits are for sprite n + 8 */
		bit0 = (color_prom[i + 32] >> 7) & 0x01;
		bit1 = (color_prom[i + 32] >> 6) & 0x01;
		bit2 = (color_prom[i + 32] >> 5) & 0x01;
		bit3 = (color_prom[i + 32] >> 4) & 0x01;
		colortable[i + 4 * 16] = 1 * bit0 + 2 * bit1 + 4 * bit2 + 8 * bit3;
	}
}

WRITE_HANDLER( redclash_videoram_w )
{
	if (videoram[offset] != data)
	{
		videoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap, offset);
	}
}

WRITE_HANDLER( redclash_gfxbank_w )
{
	if (gfxbank != (data & 0x01))
	{
		gfxbank = data & 0x01;
		tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
	}
}

WRITE_HANDLER( redclash_flipscreen_w )
{
	flip_screen_set(data & 0x01);
}

/*
star_speed:
0 = unused
1 = unused
2 = forward fast
3 = forward medium
4 = forward slow
5 = backwards slow
6 = backwards medium
7 = backwards fast
*/
WRITE_HANDLER( redclash_star0_w ) { star_speed = (star_speed & ~1) | ((data & 1) << 0); }
WRITE_HANDLER( redclash_star1_w ) { star_speed = (star_speed & ~2) | ((data & 1) << 1); }
WRITE_HANDLER( redclash_star2_w ) { star_speed = (star_speed & ~4) | ((data & 1) << 2); }
WRITE_HANDLER( redclash_star_reset_w ) { }

static void get_fg_tile_info(int tile_index)
{
	int code = videoram[tile_index];
	int color = (videoram[tile_index] & 0x70) >> 4; /* ??*/

	SET_TILE_INFO(0, code, color, 0)
}

VIDEO_START( redclash )
{
	fg_tilemap = tilemap_create(get_fg_tile_info, tilemap_scan_rows, 
		TILEMAP_TRANSPARENT, 8, 8, 32, 32);

	if ( !fg_tilemap )
		return 1;

	tilemap_set_transparent_pen(fg_tilemap, 0);

	return 0;
}

static void redclash_draw_sprites( struct mame_bitmap *bitmap )
{
	int i, offs;

	for (offs = spriteram_size - 0x20;offs >= 0;offs -= 0x20)
	{
		i = 0;
		while (i < 0x20 && spriteram[offs + i] != 0)
			i += 4;

		while (i > 0)
		{
			i -= 4;

			if (spriteram[offs + i] & 0x80)
			{
				int color = spriteram[offs + i + 2] & 0x0f;
				int sx = spriteram[offs + i + 3];
				int sy = offs / 4 + (spriteram[offs + i] & 0x07);


				switch ((spriteram[offs + i] & 0x18) >> 3)
				{
					case 3:	/* 24x24 */
					{
						int code = ((spriteram[offs + i + 1] & 0xf0) >> 4) + ((gfxbank & 1) << 4);

						drawgfx(bitmap,Machine->gfx[3],
								code,
								color,
								0,0,
								sx,sy - 16,
								&Machine->visible_area,TRANSPARENCY_PEN,0);
						/* wraparound */
						drawgfx(bitmap,Machine->gfx[3],
								code,
								color,
								0,0,
								sx - 256,sy - 16,
								&Machine->visible_area,TRANSPARENCY_PEN,0);
						break;
					}

					case 2:	/* 16x16 */
						if (spriteram[offs + i] & 0x20)	/* zero hour spaceships */
						{
							int code = ((spriteram[offs + i + 1] & 0xf8) >> 3) + ((gfxbank & 1) << 5);
							int bank = (spriteram[offs + i + 1] & 0x02) >> 1;

							drawgfx(bitmap,Machine->gfx[4+bank],
									code,
									color,
									0,0,
									sx,sy - 16,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
						}
						else
						{
							int code = ((spriteram[offs + i + 1] & 0xf0) >> 4) + ((gfxbank & 1) << 4);

							drawgfx(bitmap,Machine->gfx[2],
									code,
									color,
									0,0,
									sx,sy - 16,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
						}
						break;

					case 1:	/* 8x8 */
						drawgfx(bitmap,Machine->gfx[1],
								spriteram[offs + i + 1],/* + 4 * (spriteram[offs + i + 2] & 0x10),*/
								color,
								0,0,
								sx,sy - 16,
								&Machine->visible_area,TRANSPARENCY_PEN,0);
						break;

					case 0:
usrintf_showmessage("unknown sprite size 0");
						break;
				}
			}
		}
	}
}

static void redclash_draw_bullets( struct mame_bitmap *bitmap )
{
	int offs;

	for (offs = 0; offs < 0x20; offs++)
	{
/*		sx = videoram[offs];*/
		int sx = 8 * offs + (videoram[offs] & 0x07);	/* ?? */
		int sy = 0xff - videoram[offs + 0x20];

		if (flip_screen)
		{
			sx = 240 - sx;
		}

		if (sx >= Machine->visible_area.min_x && sx <= Machine->visible_area.max_x &&
				sy >= Machine->visible_area.min_y && sy <= Machine->visible_area.max_y)
			plot_pixel(bitmap, sx, sy, Machine->pens[0x0e]);
	}
}

VIDEO_UPDATE( redclash )
{
	fillbitmap(bitmap, get_black_pen(), &Machine->visible_area);
	redclash_draw_sprites(bitmap);
	redclash_draw_bullets(bitmap);
	tilemap_draw(bitmap, &Machine->visible_area, fg_tilemap, 0, 0);
}
