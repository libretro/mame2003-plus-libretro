/***************************************************************************

	Epos games

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "epos.h"
#include "state.h"


static int current_palette;
static int flipscreen;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  These games has one 32 byte palette PROM, connected to the RGB output this way:

  bit 7 -- 240 ohm resistor  -- RED
        -- 510 ohm resistor  -- RED
        -- 1  kohm resistor  -- RED
        -- 240 ohm resistor  -- GREEN
        -- 510 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 240 ohm resistor  -- BLUE
  bit 0 -- 510 ohm resistor  -- BLUE

***************************************************************************/

static void epos_set_pal_color(int offset, int data)
{
	int bit0,bit1,bit2,r,g,b;

	/* red component */
	bit0 = (data >> 7) & 0x01;
	bit1 = (data >> 6) & 0x01;
	bit2 = (data >> 5) & 0x01;
	r = 0x92 * bit0 + 0x4a * bit1 + 0x23 * bit2;
	/* green component */
	bit0 = (data >> 4) & 0x01;
	bit1 = (data >> 3) & 0x01;
	bit2 = (data >> 2) & 0x01;
	g = 0x92 * bit0 + 0x4a * bit1 + 0x23 * bit2;
	/* blue component */
	bit0 = (data >> 1) & 0x01;
	bit1 = (data >> 0) & 0x01;
	b = 0xad * bit0 + 0x52 * bit1;

	palette_set_color(offset,r,g,b);
}

PALETTE_INIT( epos )
{
	int i;

	for (i = 0; i < Machine->drv->total_colors; i++)
		epos_set_pal_color(i, color_prom[i]);
}

/* Tristar 9000: the palette is in RAM, written through ports 0x00-0x0f */
WRITE_HANDLER( epos_palette_w )
{
	epos_set_pal_color(offset, data);
}

/* Tristar 9000: flip screen, from the AY8910 port B (bit 7) */
void epos_set_flipscreen(int flip)
{
	if (flipscreen != flip)
	{
		flipscreen = flip;
		set_vh_global_attribute(NULL,0);
	}
}

int epos_get_flipscreen(void)
{
	return flipscreen;
}


WRITE_HANDLER( epos_videoram_w )
{
	int x,y;

	videoram[offset] = data;

	x = (offset % 136) * 2;
	y = (offset / 136);

	if (flipscreen)
	{
		plot_pixel(tmpbitmap, 270 - x + 1, 240 - y, Machine->pens[current_palette | (data & 0x0f)]);
		plot_pixel(tmpbitmap, 270 - x,     240 - y, Machine->pens[current_palette | (data >> 4)]);
	}
	else
	{
		plot_pixel(tmpbitmap, x,     y, Machine->pens[current_palette | (data & 0x0f)]);
		plot_pixel(tmpbitmap, x + 1, y, Machine->pens[current_palette | (data >> 4)]);
	}
}


WRITE_HANDLER( epos_port_1_w )
{
	/* D0 - start light #1
	   D1 - start light #2
	   D2 - coin counter
	   D3 - palette select
	   D4-D7 - unused
	 */

	set_led_status(0, data & 1);
	set_led_status(1, data & 2);

	coin_counter_w(0, data & 4);

	if (current_palette != ((data & 8) << 1))
	{
		current_palette = (data & 8) << 1;

		set_vh_global_attribute(NULL,0);
	}
}


/* the screen bitmap is drawn when the video RAM is written, and it is not
   in the save states: redraw it after a state is loaded */
static void epos_postload(void)
{
	set_vh_global_attribute(NULL, 0);
}

VIDEO_START( epos )
{
	if (video_start_generic_bitmapped())
		return 1;

	state_save_register_int("epos", 0, "current_palette", &current_palette);
	state_save_register_func_postload(epos_postload);
	return 0;
}


/***************************************************************************

  Draw the game screen in the given mame_bitmap.
  To be used by bitmapped games not using sprites.

***************************************************************************/
VIDEO_UPDATE( epos )
{
	if (get_vh_global_attribute_changed())
	{
		/* redraw bitmap */

		int offs;

		for (offs = 0; offs < videoram_size; offs++)
		{
			epos_videoram_w(offs, videoram[offs]);
		}
	}
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
}
