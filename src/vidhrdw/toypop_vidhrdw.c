/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *m68000_sharedram;

data16_t *toypop_bg_image;
static int flipscreen, palettebank;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  toypop has three 256x4 palette PROM and two 256x8 color lookup table PROMs
  (one for characters, one for sprites).


***************************************************************************/
PALETTE_INIT( toypop )
{
	int i;

	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2,bit3,r,g,b;

		// red component
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		bit3 = (color_prom[i] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		// green component
		bit0 = (color_prom[i+0x100] >> 0) & 0x01;
		bit1 = (color_prom[i+0x100] >> 1) & 0x01;
		bit2 = (color_prom[i+0x100] >> 2) & 0x01;
		bit3 = (color_prom[i+0x100] >> 3) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		// blue component
		bit0 = (color_prom[i+0x200] >> 0) & 0x01;
		bit1 = (color_prom[i+0x200] >> 1) & 0x01;
		bit2 = (color_prom[i+0x200] >> 2) & 0x01;
		bit3 = (color_prom[i+0x200] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		palette_set_color(i,r,g,b);
	}

	for (i = 0;i < 256;i++)
	{
		// characters
		colortable[i]     = color_prom[i + 0x300] | 0x70;
		colortable[i+256] = color_prom[i + 0x300] | 0xf0;
		// sprites
		colortable[i+512] = color_prom[i + 0x500];
	}
}

WRITE_HANDLER( toypop_palettebank_w )
{
	if (offset)
		palettebank = 1;
	else
		palettebank = 0;
}

WRITE16_HANDLER( toypop_flipscreen_w )
{
	flipscreen = offset;
}

READ16_HANDLER( toypop_merged_background_r )
{
	int data1, data2;

	// 0x0a0b0c0d is read as 0xabcd
	data1 = toypop_bg_image[2*offset];
	data2 = toypop_bg_image[2*offset + 1];
	return ((data1 & 0xf00) << 4) | ((data1 & 0xf) << 8) | ((data2 & 0xf00) >> 4) | (data2 & 0xf);
}

WRITE16_HANDLER( toypop_merged_background_w )
{
	// 0xabcd is written as 0x0a0b0c0d in the background image
	if (ACCESSING_MSB)
		toypop_bg_image[2*offset] = ((data & 0xf00) >> 8) | ((data & 0xf000) >> 4);

	if (ACCESSING_LSB)
		toypop_bg_image[2*offset+1] = (data & 0xf) | ((data & 0xf0) << 4);
}

static INLINE void toypop_draw_sprite(struct mame_bitmap *dest,unsigned int code,unsigned int color,
	int flipx,int flipy,int sx,int sy)
{
	drawgfx(dest,Machine->gfx[1],code,color,flipx,flipy,sx,sy,&Machine->visible_area,TRANSPARENCY_COLOR,0xff);
}

void draw_background_and_characters(struct mame_bitmap *bitmap)
{
	register int offs, x, y;
	UINT8 scanline[288];

	// copy the background image from RAM (0x190200-0x19FDFF) to bitmap
	if (flipscreen)
	{
		offs = 0xFDFE/2;
		for (y = 0; y < 224; y++)
		{
			for (x = 0; x < 288; x+=2)
			{
				data16_t data = toypop_bg_image[offs];
				scanline[x]   = data;
				scanline[x+1] = data >> 8;
				offs--;
			}
			draw_scanline8(bitmap, 0, y, 288, scanline, &Machine->pens[0x60 + 0x80*palettebank], -1);
		}
	}
	else
	{
		offs = 0x200/2;
		for (y = 0; y < 224; y++)
		{
			for (x = 0; x < 288; x+=2)
			{
				data16_t data = toypop_bg_image[offs];
				scanline[x]   = data >> 8;
				scanline[x+1] = data;
				offs++;
			}
			draw_scanline8(bitmap, 0, y, 288, scanline, &Machine->pens[0x60 + 0x80*palettebank], -1);
		}
	}

	// draw every character in the Video RAM (videoram_size = 1024)
	for (offs = 1021; offs >= 2; offs--) {
		if (offs >= 960) {
			// Draw the 2 columns at left
			x = ((offs >> 5) - 30) << 3;
			y = ((offs & 0x1f) - 2) << 3;
		} else if (offs < 64) {
			// Draw the 2 columns at right
			x = ((offs >> 5) + 34) << 3;
			y = ((offs & 0x1f) - 2) << 3;
		} else {
			// draw the rest of the screen
			x = ((offs & 0x1f) + 2) << 3;
			y = ((offs >> 5) - 2) << 3;
		}
		if (flipscreen) {
			x = 280 - x;
			y = 216 - y;
		}
		drawgfx(bitmap,Machine->gfx[0],videoram[offs],colorram[offs] + 64*palettebank,flipscreen,flipscreen,x,y,0,TRANSPARENCY_PEN,0);
	}
}

VIDEO_UPDATE( toypop )
{
	register int offs, x, y;

	draw_background_and_characters(bitmap);

	// Draw the sprites
	for (offs = 0;offs < spriteram_size;offs += 2) {
		// is it on?
		if ((spriteram_2[offs]) != 0xe9) {
			int sprite = spriteram[offs];
			int color = spriteram[offs+1];
			int flipx = spriteram_3[offs] & 1;
			int flipy = spriteram_3[offs] & 2;

			x = (spriteram_2[offs+1] | ((spriteram_3[offs+1] & 1) << 8)) - 71;
			y = 217 - spriteram_2[offs];
			if (flipscreen) {
				flipx = !flipx;
				flipy = !flipy;
			}

			switch (spriteram_3[offs] & 0x0c)
			{
				case 0:		/* normal size */
					toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
					break;
				case 4:		/* 2x horizontal */
					sprite &= ~1;
					if (!flipx) {
						toypop_draw_sprite(bitmap,1+sprite,color,0,flipy,x+16,y);
						toypop_draw_sprite(bitmap,sprite,color,0,flipy,x,y);
					} else {
						toypop_draw_sprite(bitmap,sprite,color,1,flipy,x+16,y);
						toypop_draw_sprite(bitmap,1+sprite,color,1,flipy,x,y);
					}
					break;
				case 8:		/* 2x vertical */
					sprite &= ~2;
					if (!flipy) {
						toypop_draw_sprite(bitmap,sprite,color,flipx,0,x,y-16);
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,0,x,y);
					} else {
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,1,x,y-16);
						toypop_draw_sprite(bitmap,sprite,color,flipx,1,x,y);
					}
					break;
				case 12:		/* 2x both ways */
					sprite &= ~3;
					if (!flipy && !flipx) {
						toypop_draw_sprite(bitmap,2+sprite,color,0,0,x,y);
						toypop_draw_sprite(bitmap,3+sprite,color,0,0,x+16,y);
						toypop_draw_sprite(bitmap,sprite,color,0,0,x,y-16);
						toypop_draw_sprite(bitmap,1+sprite,color,0,0,x+16,y-16);
					} else if (flipy && flipx) {
						toypop_draw_sprite(bitmap,1+sprite,color,1,1,x,y);
						toypop_draw_sprite(bitmap,sprite,color,1,1,x+16,y);
						toypop_draw_sprite(bitmap,3+sprite,color,1,1,x,y-16);
						toypop_draw_sprite(bitmap,2+sprite,color,1,1,x+16,y-16);
					} else if (flipx) {
						toypop_draw_sprite(bitmap,3+sprite,color,1,0,x,y);
						toypop_draw_sprite(bitmap,2+sprite,color,1,0,x+16,y);
						toypop_draw_sprite(bitmap,1+sprite,color,1,0,x,y-16);
						toypop_draw_sprite(bitmap,sprite,color,1,0,x+16,y-16);
					} else {	// flipy
						toypop_draw_sprite(bitmap,sprite,color,0,1,x,y);
						toypop_draw_sprite(bitmap,1+sprite,color,0,1,x+16,y);
						toypop_draw_sprite(bitmap,2+sprite,color,0,1,x,y-16);
						toypop_draw_sprite(bitmap,3+sprite,color,0,1,x+16,y-16);
					}
					break;
			}
		}
	}
}
