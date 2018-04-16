/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "artwork.h"
#include "8080bw.h"
#include "math.h"

static int screen_red;
static int screen_red_enabled;		/* 1 for games that can turn the screen red */
static int color_map_select;
static int background_color;
static UINT8 cloud_pos;
static data8_t bowler_bonus_display;
static int helifire_mv2_offset;
static UINT32 helifire_star_rng;

static mem_write_handler videoram_w_p;
static void (*video_update_p)(struct mame_bitmap *bitmap,const struct rectangle *cliprect);

static WRITE_HANDLER( bw_videoram_w );
static WRITE_HANDLER( schaser_videoram_w );
static WRITE_HANDLER( lupin3_videoram_w );
static WRITE_HANDLER( polaris_videoram_w );
static WRITE_HANDLER( sstrngr2_videoram_w );
static WRITE_HANDLER( helifire_videoram_w );
static WRITE_HANDLER( phantom2_videoram_w );
static WRITE_HANDLER( invadpt2_videoram_w );
static WRITE_HANDLER( cosmo_videoram_w );

static VIDEO_UPDATE( 8080bw_common );
static VIDEO_UPDATE( helifire );
static VIDEO_UPDATE( seawolf );
static VIDEO_UPDATE( blueshrk );
static VIDEO_UPDATE( desertgu );
static VIDEO_UPDATE( bowler );

static void plot_pixel_8080(int x, int y, int col);

/* smoothed colors, overlays are not so contrasted */
#define OVERLAY_RED			MAKE_ARGB(0x08,0xff,0x20,0x20)
#define OVERLAY_GREEN		MAKE_ARGB(0x08,0x20,0xff,0x20)
#define OVERLAY_BLUE		MAKE_ARGB(0x08,0x20,0x20,0xff)
#define OVERLAY_YELLOW		MAKE_ARGB(0x08,0xff,0xff,0x20)
#define OVERLAY_CYAN		MAKE_ARGB(0x08,0x20,0xff,0xff)
#define OVERLAY_LT_BLUE		MAKE_ARGB(0x08,0xa0,0xa0,0xff)


OVERLAY_START( invaders_overlay )
	OVERLAY_RECT(   8,   0,  64, 224, OVERLAY_GREEN )
	OVERLAY_RECT(   0,  16,   8, 134, OVERLAY_GREEN )
	OVERLAY_RECT( 184,   0, 216, 224, OVERLAY_RED )
OVERLAY_END


/*
OVERLAY_START( invdpt2m_overlay )
	OVERLAY_RECT(  16,   0,  72, 224, OVERLAY_GREEN )
	OVERLAY_RECT(   0,  16,  16, 134, OVERLAY_GREEN )
	OVERLAY_RECT(  72,   0, 192, 224, OVERLAY_YELLOW )
	OVERLAY_RECT( 192,   0, 224, 224, OVERLAY_RED )
OVERLAY_END
*/


OVERLAY_START( invrvnge_overlay )
	OVERLAY_RECT(   0,   0,  64, 224, OVERLAY_GREEN )
	OVERLAY_RECT( 184,   0, 224, 224, OVERLAY_RED )
OVERLAY_END


OVERLAY_START( invad2ct_overlay )
	OVERLAY_RECT(   0,   0,  48, 224, OVERLAY_YELLOW )
	OVERLAY_RECT(  25,   0,  71, 224, OVERLAY_GREEN )
	OVERLAY_RECT(  48,   0, 140, 224, OVERLAY_CYAN )
	OVERLAY_RECT( 117,   0, 186, 224, OVERLAY_GREEN )
	OVERLAY_RECT( 163,   0, 232, 224, OVERLAY_YELLOW )
	OVERLAY_RECT( 209,   0, 256, 224, OVERLAY_RED )
OVERLAY_END


OVERLAY_START( phantom2_overlay )
	OVERLAY_RECT(   0,   0, 240, 224, OVERLAY_LT_BLUE )
OVERLAY_END


OVERLAY_START( gunfight_overlay )
	OVERLAY_RECT(   0,   0, 256, 224, OVERLAY_YELLOW )
OVERLAY_END


OVERLAY_START( bandido_overlay )
	OVERLAY_RECT(   0,   0,  24, 224, OVERLAY_BLUE )
	OVERLAY_RECT(  24,   0,  40, 100, OVERLAY_BLUE )
	OVERLAY_RECT(  24, 124,  40, 224, OVERLAY_BLUE )
	OVERLAY_RECT(  24, 100,  40, 124, OVERLAY_GREEN )
	OVERLAY_RECT(  40,   0, 184,  24, OVERLAY_BLUE )
	OVERLAY_RECT(  40,  24, 100,  32, OVERLAY_BLUE )
	OVERLAY_RECT( 124,  24, 184,  32, OVERLAY_BLUE )
	OVERLAY_RECT( 100,  24, 124,  40, OVERLAY_RED )
	OVERLAY_RECT( 184, 100, 200, 124, OVERLAY_GREEN )
	OVERLAY_RECT( 184,   0, 200, 100, OVERLAY_BLUE )
	OVERLAY_RECT( 184, 124, 200, 224, OVERLAY_BLUE )
	OVERLAY_RECT( 200,   0, 232, 224, OVERLAY_BLUE )
	OVERLAY_RECT( 232,   0, 256, 224, OVERLAY_RED )
	OVERLAY_RECT(  40,  32, 100,  40, OVERLAY_YELLOW )
	OVERLAY_RECT( 124,  32, 184,  40, OVERLAY_YELLOW )
	OVERLAY_RECT(  40,  40, 184, 184, OVERLAY_YELLOW )
	OVERLAY_RECT(  40, 184, 100, 192, OVERLAY_YELLOW )
	OVERLAY_RECT( 124, 184, 184, 192, OVERLAY_YELLOW )
	OVERLAY_RECT(  40, 192, 100, 200, OVERLAY_BLUE )
	OVERLAY_RECT( 124, 192, 184, 200, OVERLAY_BLUE )
	OVERLAY_RECT(  40, 200, 184, 224, OVERLAY_BLUE )
	OVERLAY_RECT( 100, 184, 124, 200, OVERLAY_RED )
OVERLAY_END


DRIVER_INIT( 8080bw )
{
	videoram_w_p = bw_videoram_w;
	video_update_p = video_update_8080bw_common;
	screen_red = 0;
	screen_red_enabled = 0;
	color_map_select = 0;
	flip_screen_set(0);
}

DRIVER_INIT( invaders )
{
	init_8080bw();
	artwork_set_overlay(invaders_overlay);
}

DRIVER_INIT( invaddlx )
{
	init_8080bw();
/*	artwork_set_overlay(invdpt2m_overlay);*/
}

DRIVER_INIT( invrvnge )
{
	init_8080bw();
	artwork_set_overlay(invrvnge_overlay);
}

DRIVER_INIT( invad2ct )
{
	init_8080bw();
	artwork_set_overlay(invad2ct_overlay);
}

DRIVER_INIT( sstrngr2 )
{
	init_8080bw();
	videoram_w_p = sstrngr2_videoram_w;
	screen_red_enabled = 1;
}

DRIVER_INIT( schaser )
{
	init_8080bw();
	videoram_w_p = schaser_videoram_w;
	background_color = 2;	/* blue */
}

DRIVER_INIT( rollingc )
{
	init_8080bw();
	videoram_w_p = schaser_videoram_w;
	background_color = 0;	/* black */
}

DRIVER_INIT( helifire )
{
	init_8080bw();
	videoram_w_p = helifire_videoram_w;
	video_update_p = video_update_helifire;
	helifire_mv2_offset = 0;
	helifire_star_rng = 0;
}

DRIVER_INIT( polaris )
{
	init_8080bw();
	videoram_w_p = polaris_videoram_w;
}

DRIVER_INIT( lupin3 )
{
	init_8080bw();
	videoram_w_p = lupin3_videoram_w;
}

DRIVER_INIT( invadpt2 )
{
	init_8080bw();
	videoram_w_p = invadpt2_videoram_w;
	screen_red_enabled = 1;
}

DRIVER_INIT( cosmo )
{
	init_8080bw();
	videoram_w_p = cosmo_videoram_w;
}

DRIVER_INIT( seawolf )
{
	init_8080bw();
	video_update_p = video_update_seawolf;
}

DRIVER_INIT( blueshrk )
{
	init_8080bw();
	video_update_p = video_update_blueshrk;
}

DRIVER_INIT( desertgu )
{
	init_8080bw();
	video_update_p = video_update_desertgu;
}

DRIVER_INIT( bowler )
{
	init_8080bw();
	video_update_p = video_update_bowler;
}

DRIVER_INIT( phantom2 )
{
	init_8080bw();
	videoram_w_p = phantom2_videoram_w;
	artwork_set_overlay(phantom2_overlay);
}

DRIVER_INIT( gunfight )
{
	init_8080bw();
	artwork_set_overlay(gunfight_overlay);
}

DRIVER_INIT( bandido )
{
	init_8080bw();
	artwork_set_overlay(bandido_overlay);
}



void c8080bw_flip_screen_w(int data)
{
	set_vh_global_attribute(&color_map_select, data);

	if (input_port_3_r(0) & 0x01)
	{
		flip_screen_set(data);
	}
}


void c8080bw_screen_red_w(int data)
{
	if (screen_red_enabled)
	{
		set_vh_global_attribute(&screen_red, data);
	}
}


INTERRUPT_GEN( polaris_interrupt )
{
	static int cloud_speed;

	cloud_speed++;

	if (cloud_speed >= 8)	/* every 4 frames - this was verified against real machine */
	{
		cloud_speed = 0;

		cloud_pos--;

		if (cloud_pos >= 0xe0)
		{
			cloud_pos = 0xdf;	/* no delay for invisible region */
		}

		set_vh_global_attribute(NULL,0);
	}

	c8080bw_interrupt();
}


INTERRUPT_GEN( phantom2_interrupt )
{
	static int cloud_speed;

	cloud_speed++;

	if (cloud_speed >= 2)	/* every 2 frames - no idea of correct */
	{
		cloud_speed = 0;

		cloud_pos++;
		set_vh_global_attribute(NULL,0);
	}

	c8080bw_interrupt();
}


static void plot_pixel_8080(int x, int y, int col)
{
	if (flip_screen)
	{
		x = 255-x;
		y = 255-y;
	}

	plot_pixel(tmpbitmap,x,y,Machine->pens[col]);
}

static INLINE void plot_byte(int x, int y, int data, int fore_color, int back_color)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		plot_pixel_8080(x, y, (data & 0x01) ? fore_color : back_color);

		x++;
		data >>= 1;
	}
}


WRITE_HANDLER( c8080bw_videoram_w )
{
	videoram_w_p(offset, data);
}


static WRITE_HANDLER( bw_videoram_w )
{
	int x,y;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	plot_byte(x, y, data, 1, 0);
}

static WRITE_HANDLER( schaser_videoram_w )
{
	UINT8 x,y,col;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	col = colorram[offset & 0x1f1f] & 0x07;

	plot_byte(x, y, data, col, background_color);
}

static WRITE_HANDLER( lupin3_videoram_w )
{
	UINT8 x,y,col;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	col = ~colorram[offset & 0x1f1f] & 0x07;

	plot_byte(x, y, data, col, 0);
}

static WRITE_HANDLER( polaris_videoram_w )
{
	int x,i,col,back_color,fore_color,color_map;
	UINT8 y, cloud_y;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	/* for the background color, bit 0 of the map PROM is connected to green gun.
	   red is 0 and blue is 1, giving cyan and blue for the background.  This
	   is different from what the schematics shows, but it's supported
	   by screenshots. */

	color_map = memory_region(REGION_PROMS)[(y >> 3 << 5) | (x >> 3)];
	back_color = (color_map & 1) ? 6 : 2;
	fore_color = ~colorram[offset & 0x1f1f] & 0x07;

	/* bit 3 is connected to the cloud enable. bits 1 and 2 are marked 'not use' (sic)
	   on the schematics */

	if (y < cloud_pos)
	{
		cloud_y = y - cloud_pos - 0x20;
	}
	else
	{
		cloud_y = y - cloud_pos;
	}

	if ((color_map & 0x08) || (cloud_y > 64))
	{
		plot_byte(x, y, data, fore_color, back_color);
	}
	else
	{
		/* cloud appears in this part of the screen */
		for (i = 0; i < 8; i++)
		{
			if (data & 0x01)
			{
				col = fore_color;
			}
			else
			{
				int bit;
				offs_t offs;

				col = back_color;

				bit = 1 << (~x & 0x03);
				offs = ((x >> 2) & 0x03) | ((~cloud_y & 0x3f) << 2);

				col = (memory_region(REGION_USER1)[offs] & bit) ? 7 : back_color;
			}

			plot_pixel_8080(x, y, col);

			x++;
			data >>= 1;
		}
	}
}

static WRITE_HANDLER( helifire_videoram_w )
{
	int x,y,back_color,foreground_color;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	back_color = 8; /* TRANSPARENT PEN */
	foreground_color = colorram[offset] & 0x07;

	plot_byte(x, y, data, foreground_color, back_color);
}


WRITE_HANDLER( helifire_colorram_w )
{
	colorram[offset] = data;

	/* redraw region with (possibly) changed color */
	videoram_w_p(offset, videoram[offset]);
}


WRITE_HANDLER( schaser_colorram_w )
{
	int i;


	offset &= 0x1f1f;

	colorram[offset] = data;

	/* redraw region with (possibly) changed color */
	for (i = 0; i < 8; i++, offset += 0x20)
	{
		videoram_w_p(offset, videoram[offset]);
	}
}

READ_HANDLER( schaser_colorram_r )
{
	return colorram[offset & 0x1f1f];
}


static WRITE_HANDLER( phantom2_videoram_w )
{
	static int CLOUD_SHIFT[] = { 0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08,
	                             0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x80, 0x80 };

	int i,col;
	UINT8 x,y,cloud_x;
	UINT8 *cloud_region;
	offs_t cloud_offs;


	videoram[offset] = data;

	y = offset / 32;
	x = (offset % 32) * 8;


	cloud_region = memory_region(REGION_PROMS);
	cloud_offs = ((y - cloud_pos) & 0xff) >> 1 << 4;
	cloud_x = x - 12;  /* based on screen shots */


	for (i = 0; i < 8; i++)
	{
		if (data & 0x01)
		{
			col = 1;	/* white foreground */
		}
		else
		{
			UINT8 cloud_data;


			cloud_offs = (cloud_offs & 0xfff0) | (cloud_x >> 4);
			cloud_data = cloud_region[cloud_offs];

			if (cloud_data & (CLOUD_SHIFT[cloud_x & 0x0f]))
			{
				col = 2;	/* grey cloud */
			}
			else
			{
				col = 0;	/* black background */
			}
		}

		plot_pixel_8080(x, y, col);

		x++;
		cloud_x++;
		data >>= 1;
	}
}


/***************************************************************************

  Draw the game screen in the given mame_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
VIDEO_UPDATE( 8080bw )
{
	video_update_p(bitmap, cliprect);
}


static VIDEO_UPDATE( 8080bw_common )
{
	if (get_vh_global_attribute_changed())
	{
		int offs;

		for (offs = 0;offs < videoram_size;offs++)
			videoram_w_p(offs, videoram[offs]);
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,cliprect,TRANSPARENCY_NONE,0);
}



static int sea_waveform[8] = {0,70,90,97,99,30,10,3}; /* percentage of RC charge (charging and discharging curve)*/
static int helifire_star_latch = 0;
static int MVx_count = 0;
static UINT16 scanline[256];

static int last_colors_change = -1;
static int b_to_g = 0;
static int g_to_r = 0;
static int death_colors_rng = 0;
static int death_colors_timing = 0;

PALETTE_INIT( helifire )
{
	int i;

	for (i = 0; i < 8; i++)
	{
		int r = 0xff * ((i >> 0) & 1);
		int g = 0xff * ((i >> 1) & 1);
		int b = 0xff * ((i >> 2) & 1);
		palette_set_color(i,r,g,b);
	}
	for (i = 0; i < 256; i++)
	{
		double V, time;
		int level;

		time = i;
		V = 255.0 * pow(2.71828, -time/ (255.0/3.0) ); /* capacitor discharge */
		level = V;

		palette_set_color(8+i    ,0,0,level);	/* shades of blue without green star */

		palette_set_color(512+8+i,level,0,0);	/* shades of red without green star */

		/* green brightness was not verified */
		palette_set_color(256+8+i,0,192,level);	/* shades of blue with green star */

		/* green brightness was not verified */
		palette_set_color(768+8+i,level,192,0);	/* shades of red with green star */
	}
}



void c8080bw_helifire_colors_change_w(int data) /* 1 - don't change colors, 0 - change colors of fonts and objects */
{
	if (last_colors_change != data)
	{
		last_colors_change = data;
	}
}

VIDEO_EOF (helifire)
{
	int i;

	/* there are two circuits:
		one generates physical 256 lines (and takes exactly 256 horizontal blanks) ,
		the other one that generates MVx lines (takes 257 horizontal blanks)
	  The final effect of this is that the waves and stars in background move right by 1 pixel per frame
	*/
	helifire_mv2_offset = (helifire_mv2_offset + 1) & 255;


	death_colors_timing = (death_colors_timing + 1) & 15;

	if (death_colors_timing & 1) /* 1,3,5,7,9,11,13,15 */
		death_colors_rng = (((death_colors_rng ^ (death_colors_rng<<1) ^ 0x80)&0x80)>>7) | ((death_colors_rng&0x7f)<<1);

	b_to_g = (death_colors_rng & 0x20) >> 5;


	if (death_colors_timing == 8)
		g_to_r = 1;
	if (death_colors_timing == 0)
		g_to_r = 0;	


	if (last_colors_change)
	{
		/* normal palette */
		for (i = 0; i < 8; i++)
		{
			int r = 0xff * ((i >> 0) & 1);
			int g = 0xff * ((i >> 1) & 1);
			int b = 0xff * ((i >> 2) & 1);
			palette_set_color(i,r,g,b);
		}
	}
	else
	{
		/* randomized palette */
		for (i = 0; i < 8; i++)
		{
			int r = 0xff * ((i >> 0) & 1);
			int g = 0xff * ((i >> 1) & 1);
			int b = 0xff * ((i >> 2) & 1);

			if (b_to_g)
				g |= b;	/* perhaps we should use |= instead ? */
			if (g_to_r)
				r |= g;	/* perhaps we should use |= instead ? */

			palette_set_color(i,r,g,b);
		}
	}
}


static VIDEO_UPDATE( helifire )
{
	int x, y;
	int sun_brightness = readinputport(4);
	int sea_brightness = readinputport(5);
	int sea_level = 116 + readinputport(6); /* 116 is a guess */
	int wave_height = readinputport(7);

	if (get_vh_global_attribute_changed())
	{
		int offs;

		for (offs = 0;offs < videoram_size;offs++)
			videoram_w_p(offs, videoram[offs]);
	}


	/* background */
	for (y = 0; y < 256; y++)
	{
		int start;
		int color;		
		int sea_wave = sea_level + (sea_waveform[ (y-helifire_mv2_offset) & 7 ] * wave_height / 100);
#if 1
		MVx_count++;
		if (MVx_count>=257)	/* the RNG is reset every MV_RST which is every 257 physical lines */
		{
			helifire_star_rng = 0;
			MVx_count = 0;
			logerror("257 = offs=%4i mvoff+y=%4i y=%4i\n", helifire_mv2_offset, helifire_mv2_offset + y, y );
		}
#endif
//		if (y == helifire_mv2_offset) /* when MV_RST *//* the RNG is reset every MV_RST which is every 257 physical lines */
//		{
//			//helifire_star_rng = 0;
//			logerror("mv2 = offs=%4i mvoff+y=%4i y=%4i\n", helifire_mv2_offset, helifire_mv2_offset + y, y );
//		}

		//if (((helifire_mv2_offset + y)&7) == 4) /* when MV2 goes high */
		if ((MVx_count&7) == 4) /* when MV2 goes high */
		{
			helifire_star_latch = (helifire_star_rng & 0x0f)<<3;	/* latched value is being compared to H0, H1, H2, H3 (where H0 is physical H3) */
		}

		/* star RNG circuit, 1 step per 1 pixel */
		for(x = 256; x > 0; x--)
		{
			/* negated: bit 7 xor bit 6 goes to bit 0 */
			helifire_star_rng = (((helifire_star_rng ^ (helifire_star_rng<<1) ^ 0x80)&0x80)>>7) | ((helifire_star_rng&0x7f)<<1);
		}

		/* draw the sea */
		start = 0;
		for (x = 0; x < sea_wave; x++)
		{
			color = start + sea_brightness;
			if (color>255) color = 255;
			scanline[x] = 8 + color;
			start++;
		}

		/* draw the sun glow */
		start = 0;
		for (x = sea_wave; x < 256; x++)
		{
			color = start + sun_brightness;
			if (color>255) color = 255;
			scanline[x] = 512 + 8 + color;
			start++;
		}

		/* there will be two stars:
			- one at each line when MV2 goes low->high in a pixels range of 128-255
			- one at the very next line in a pixels range of 0-127
		*/

		//if (((helifire_mv2_offset + y)&7) == 4) /* when MV2 goes high */
		if ((MVx_count&7) == 4) /* when MV2 goes high */
		{
			scanline[ 128 + helifire_star_latch ] = scanline[ 128 + helifire_star_latch ] + 256; /* background with the star */
		}
		//if (((helifire_mv2_offset + y)&7) == 5) /* when MV2 goes high - the very next line */
		if ((MVx_count&7) == 5) /* when MV2 goes high */
		{
			scanline[ helifire_star_latch ] = scanline[ helifire_star_latch ] + 256; /* background with the star */
		}

		draw_scanline16(bitmap, 0, y, 256, scanline, &Machine->pens[0], -1);
	}

	/* foreground */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,cliprect,TRANSPARENCY_PEN,8);
}

static void draw_sight(struct mame_bitmap *bitmap,const struct rectangle *cliprect,int x_center, int y_center)
{
	int x,y;
	int sight_xs;
	int sight_xc;
	int sight_xe;
	int sight_ys;
	int sight_yc;
	int sight_ye;


	sight_xc = x_center;
	if( sight_xc < 2 )
	{
		sight_xc = 2;
	}
	else if( sight_xc > 253 )
	{
		sight_xc = 253;
	}

	sight_yc = y_center;
	if( sight_yc < 2 )
	{
		sight_yc = 2;
	}
	else if( sight_yc > 221 )
	{
		sight_yc = 221;
	}

	sight_xs = sight_xc - 20;
	if( sight_xs < 0 )
	{
		sight_xs = 0;
	}
	sight_xe = sight_xc + 20;
	if( sight_xe > 255 )
	{
		sight_xe = 255;
	}

	sight_ys = sight_yc - 20;
	if( sight_ys < 0 )
	{
		sight_ys = 0;
	}
	sight_ye = sight_yc + 20;
	if( sight_ye > 223 )
	{
		sight_ye = 223;
	}

	x = sight_xc;
	y = sight_yc;
	if (flip_screen)
	{
		x = 255-x;
		y = 255-y;
	}


	draw_crosshair(bitmap,x,y,cliprect);
}


static VIDEO_UPDATE( seawolf )
{
	/* update the bitmap (and erase old cross) */
	video_update_8080bw_common(bitmap, cliprect);

    draw_sight(bitmap,cliprect,((input_port_0_r(0) & 0x1f) * 8) + 4, 63);
}

static VIDEO_UPDATE( blueshrk )
{
	/* update the bitmap (and erase old cross) */
	video_update_8080bw_common(bitmap, cliprect);

    draw_sight(bitmap,cliprect,((input_port_0_r(0) & 0x7f) * 2) - 12, 63);
}

static VIDEO_UPDATE( desertgu )
{
	/* update the bitmap (and erase old cross) */
	video_update_8080bw_common(bitmap, cliprect);

	draw_sight(bitmap,cliprect,
			   ((input_port_0_r(0) & 0x7f) * 2) - 30,
			   ((input_port_2_r(0) & 0x7f) * 2) + 2);
}


WRITE_HANDLER( bowler_bonus_display_w )
{
	/* Bits 0-6 control which score is lit.
	   Bit 7 appears to be a global enable, but the exact
	   effect is not known. */

	bowler_bonus_display = data;
}


static VIDEO_UPDATE( bowler )
{
	int x,y,i;

	char score_line_1[] = "Bonus 200 400 500 700 500 400 200";
	char score_line_2[] = "      110 220 330 550 330 220 110";


	/* update the bitmap */
	video_update_8080bw_common(bitmap, cliprect);


	/* draw the current bonus value - on the original game this
	   was done using lamps that lit score displays on the bezel. */

	x = 33 * 8;
	y = 31 * 8;

	for (i = 0; i < 33; i++)
	{
		int col;


		col = UI_COLOR_NORMAL;

		if ((i >= 6) && ((i % 4) != 1))
		{
			int bit = (i - 6) / 4;

			if (bowler_bonus_display & (1 << bit))
			{
				col = UI_COLOR_INVERSE;
			}
		}


		drawgfx(bitmap,Machine->uifont,
				score_line_1[i],col,
				0,1,
				x,y,
				cliprect,TRANSPARENCY_NONE,0);

		drawgfx(bitmap,Machine->uifont,
				score_line_2[i],col,
				0,1,
				x+8,y,
				cliprect,TRANSPARENCY_NONE,0);

		y -= Machine->uifontwidth;
	}
}


PALETTE_INIT( invadpt2 )
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		/* this bit arrangment is a little unusual but are confirmed by screen shots */
		int r = 0xff * ((i >> 0) & 1);
		int g = 0xff * ((i >> 2) & 1);
		int b = 0xff * ((i >> 1) & 1);
		palette_set_color(i,r,g,b);
	}
}


PALETTE_INIT( sflush )
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		/* this bit arrangment is a little unusual but are confirmed by screen shots */
		int r = 0xff * ((i >> 0) & 1);
		int g = 0xff * ((i >> 2) & 1);
		int b = 0xff * ((i >> 1) & 1);
		palette_set_color(i,r,g,b);
	}
	palette_set_color(0,0x80,0x80,0xff);
}


static WRITE_HANDLER( invadpt2_videoram_w )
{
	UINT8 x,y,col;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	/* 32 x 32 colormap */
	if (!screen_red)
	{
		UINT16 colbase;

		colbase = color_map_select ? 0x0400 : 0;
		col = memory_region(REGION_PROMS)[colbase | (y >> 3 << 5) | (x >> 3)] & 0x07;
	}
	else
		col = 1;	/* red */

	plot_byte(x, y, data, col, 0);
}

PALETTE_INIT( cosmo )
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int r = 0xff * ((i >> 0) & 1);
		int g = 0xff * ((i >> 1) & 1);
		int b = 0xff * ((i >> 2) & 1);
		palette_set_color(i,r,g,b);
	}
}

WRITE_HANDLER( cosmo_colorram_w )
{
	int i;
	int offs = ((offset>>5)<<8) | (offset&0x1f);

	colorram[offset] = data;

	/* redraw region with (possibly) changed color */
	for (i=0; i<8; i++)
	{
		videoram_w_p(offs, videoram[offs]);
		offs+= 0x20;
	}		
}

static WRITE_HANDLER( cosmo_videoram_w )
{
	UINT8 x,y,col;

	videoram[offset] = data;

	y = offset / 32;
	x = offset % 32;

	/* 32 x 32 colormap */
	col = colorram[(y >> 3 << 5) | x ] & 0x07;

	plot_byte(8*x, y, data, col, 0);
}

static WRITE_HANDLER( sstrngr2_videoram_w )
{
	UINT8 x,y,col;

	videoram[offset] = data;

	y = offset / 32;
	x = 8 * (offset % 32);

	/* 16 x 32 colormap */
	if (!screen_red)
	{
		UINT16 colbase;

		colbase = color_map_select ? 0 : 0x0200;
		col = memory_region(REGION_PROMS)[colbase | (y >> 4 << 5) | (x >> 3)] & 0x0f;
	}
	else
		col = 1;	/* red */

	if (color_map_select)
	{
		x = 240 - x;
		y = 31 - y;
	}

	plot_byte(x, y, data, col, 0);
}
