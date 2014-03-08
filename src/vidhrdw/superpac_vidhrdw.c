/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


static UINT8 color15_mask[64];


/***************************************************************************

  Convert the color PROMs into a more useable format.

  digdug has one 32x8 palette PROM and two 256x4 color lookup table PROMs
  (one for characters, one for sprites). Only the first 128 bytes of the
  lookup tables seem to be used.
  The palette PROM is connected to the RGB output this way:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/

PALETTE_INIT( superpac )
{
	int i, j;

	for (i = 0; i < 32; i++)
	{
		int bit0, bit1, bit2, r, g, b;

		bit0 = (color_prom[31-i] >> 0) & 0x01;
		bit1 = (color_prom[31-i] >> 1) & 0x01;
		bit2 = (color_prom[31-i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (color_prom[31-i] >> 3) & 0x01;
		bit1 = (color_prom[31-i] >> 4) & 0x01;
		bit2 = (color_prom[31-i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (color_prom[31-i] >> 6) & 0x01;
		bit2 = (color_prom[31-i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		palette_set_color(i,r,g,b);
	}

	/* characters */
	for (i = 0; i < 64*4; i++)
		colortable[i] = color_prom[i + 32] & 0x0f;

	/* sprites */
	for (i = 64*4; i < 128*4; i++)
		colortable[i] = 0x1f - (color_prom[i + 32] & 0x0f);

	/* for sprites, track which pens for each color map to color 31 */
	for (i = 0; i < 64; i++)
	{
		color15_mask[i] = 0;
		for (j = 0; j < 4; j++)
			if (colortable[64*4 + i*4 + j] == 0x1f)
				color15_mask[i] |= 1 << j;
	}
}


WRITE_HANDLER( superpac_flipscreen_w )
{
	flip_screen_set(data);
}



READ_HANDLER(superpac_flipscreen_r)
{
	flip_screen_set(1);

	return flip_screen;	/* return value not used */
}



static void draw_sprites(struct mame_bitmap *bitmap, struct rectangle *clip, int drawmode)
{
	const struct GfxElement *gfx = Machine->gfx[1];
	int offs;

	for (offs = 0; offs < spriteram_size; offs += 2)
	{
		/* is it on? */
		if ((spriteram_3[offs+1] & 2) == 0)
		{
			int sprite = spriteram[offs];
			int color = spriteram[offs+1] & 0x3f;
			int x = (spriteram_2[offs+1] - 40) + 0x100*(spriteram_3[offs+1] & 1);
			int y = 28*8 - spriteram_2[offs] + 1;
			int flipx = spriteram_3[offs] & 1;
			int flipy = spriteram_3[offs] & 2;
			int pens;

			if (flip_screen)
			{
				flipx = !flipx;
				flipy = !flipy;
			}

			pens = (drawmode == TRANSPARENCY_PENS) ? ~color15_mask[color] : 16;

			switch (spriteram_3[offs] & 0x0c)
			{
				case 0:		/* normal size */
					drawgfx(bitmap, gfx, sprite, color, flipx, flipy, x, y, clip, drawmode, pens);
					break;

				case 4:		/* 2x horizontal */
					sprite &= ~1;
					if (!flipx)
					{
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x + 0,  y, clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 1, color, flipx, flipy, x + 16, y, clip, drawmode, pens);
					}
					else
					{
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x + 16, y, clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 1, color, flipx, flipy, x + 0,  y, clip, drawmode, pens);
					}
					break;

				case 8:		/* 2x vertical */
					sprite &= ~2;
					if (!flipy)
					{
						drawgfx(bitmap, gfx, sprite + 2, color, flipx, flipy, x, y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x, y - 16, clip, drawmode, pens);
					}
					else
					{
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x, y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 2, color, flipx, flipy, x, y - 16, clip, drawmode, pens);
					}
					break;

				case 12:		/* 2x both ways */
					sprite &= ~3;
					if (!flipx && !flipy)
					{
						drawgfx(bitmap, gfx, sprite + 2, color, flipx, flipy, x + 0,  y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 3, color, flipx, flipy, x + 16, y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x + 0,  y - 16, clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 1, color, flipx, flipy, x + 16, y - 16, clip, drawmode, pens);
					}
					else if (flipx && flipy)
					{
						drawgfx(bitmap, gfx, sprite + 1, color, flipx, flipy, x + 0,  y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x + 16, y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 3, color, flipx, flipy, x + 0,  y - 16, clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 2, color, flipx, flipy, x + 16, y - 16, clip, drawmode, pens);
					}
					else if (flipy)
					{
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x + 0,  y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 1, color, flipx, flipy, x + 16, y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 2, color, flipx, flipy, x + 0,  y - 16, clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 3, color, flipx, flipy, x + 16, y - 16, clip, drawmode, pens);
					}
					else /* flipx */
					{
						drawgfx(bitmap, gfx, sprite + 3, color, flipx, flipy, x + 0,  y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 2, color, flipx, flipy, x + 16, y - 0,  clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 1, color, flipx, flipy, x + 0,  y - 16, clip, drawmode, pens);
						drawgfx(bitmap, gfx, sprite + 0, color, flipx, flipy, x + 16, y - 16, clip, drawmode, pens);
					}
					break;
			}
		}
	}
}


/***************************************************************************

  Draw the game screen in the given mame_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

VIDEO_UPDATE( superpac )
{
	int offs;

	if (get_vh_global_attribute_changed())
		memset(dirtybuffer, 1, videoram_size);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1; offs >= 0; offs--)
		if (dirtybuffer[offs])
		{
			int sx, sy, mx, my;

			dirtybuffer[offs] = 0;

			/* Even if Super Pac-Man's screen is 28x36, the memory layout is 32x32. We therefore */
			/* have to convert the memory coordinates into screen coordinates. */
			/* Note that 32*32 = 1024, while 28*36 = 1008: therefore 16 bytes of Video RAM */
			/* don't map to a screen position. We don't check that here, however: range */
			/* checking is performed by drawgfx(). */

			mx = offs % 32;
			my = offs / 32;

			if (my <= 1)
			{
				sx = my + 34;
				sy = mx - 2;
			}
			else if (my >= 30)
			{
				sx = my - 30;
				sy = mx - 2;
			}
			else
			{
				sx = mx + 2;
				sy = my - 2;
			}

			if (flip_screen)
			{
				sx = 35 - sx;
				sy = 27 - sy;
			}

			drawgfx(tmpbitmap, Machine->gfx[0], videoram[offs], colorram[offs],
					flip_screen, flip_screen, 8 * sx, 8 * sy,
					&Machine->visible_area, TRANSPARENCY_NONE, 0);
		}

	/* copy the character mapped graphics */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* Draw the sprites. */
	draw_sprites(bitmap, &Machine->visible_area, TRANSPARENCY_COLOR);

	/* Draw the high priority characters */
	for (offs = videoram_size - 1;offs >= 0;offs--)
		if (colorram[offs] & 0x40)
		{
			int sx, sy, mx, my;

			mx = offs % 32;
			my = offs / 32;

			if (my <= 1)
			{
				sx = my + 34;
				sy = mx - 2;
			}
			else if (my >= 30)
			{
				sx = my - 30;
				sy = mx - 2;
			}
			else
			{
				sx = mx + 2;
				sy = my - 2;
			}

			if (flip_screen)
			{
				sx = 35 - sx;
				sy = 27 - sy;
			}

			drawgfx(bitmap, Machine->gfx[0], videoram[offs], colorram[offs],
					flip_screen, flip_screen, 8 * sx, 8 * sy,
					&Machine->visible_area, TRANSPARENCY_COLOR, 31);
		}

	/* Color 31 still has priority over that (ghost eyes in Pac 'n Pal) */
	draw_sprites(bitmap, &Machine->visible_area, TRANSPARENCY_PENS);
}
