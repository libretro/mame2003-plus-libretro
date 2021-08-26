/*

Jolly Jogger
Taito, 1982

PCB Layout
----------

FGO70007
KDN00004
KDK00502
|----------------------------------------------------------|
|             TMM416  TMM416  TMM416  TMM416               |
|             TMM416  TMM416  TMM416  TMM416              |-|
|             TMM416  TMM416  TMM416  TMM416              | |
|             TMM416  TMM416  TMM416  TMM416              | |
|                                                         | |
|                                                         | |
|                                                         | |
|                                                         | |
|2                                                        |-|
|2                                                         |
|W   DSW1(8)                                     KD13.1F   |
|A                                                         |
|Y                                                         |
|                                                         |-|
|                                                         | |
|                                                         | |
|                                                         | |
|                MB14241                                  | |
|    DSW2(8)                                              | |
|                                                         | |
|                                                         |-|
|    VOL         AY3-8910     3.579545MHz                  |
|----------------------------------------------------------|

FGO70008
KDN00007
|----------------------------------------------------------|
|                                                          |
|                                            6116         |-|
|                                                         | |
|                                            KD21.8J      | |
|   D2125          D2125                                  | |
|                                            KD20.8H      | |
|   D2125          D2125                                  | |
|                                            KD19.8F      | |
|   D2125          D2125                                  |-|
|                                            KD18.8E       |
|   D2125          D2125                                   |
|                                            KD17.8D       |
|   D2125          D2125        Z80                        |
|                                            KD16.8C      |-|
|                                                         | |
|                                            KD15.8B      | |
|                                                         | |
|                                            KD14.8A      | |
|                                                         | |
|                                                         | |
|                                                         |-|
|                                                          |
|----------------------------------------------------------|

FGO70009
KDN00006
|----------------------------------------------------------|
|                                                18MHz     |
|                                                         |-|
|                                                         | |
|                                                         | |
|                                                         | |
|                                                         | |
|                                                         | |
|                         KD11.5H   KD12.7H               | |
|                                                         |-|
|1                                                         |
|8                                                         |
|W                                                         |
|A                                                         |
|Y                                                        |-|
|                                                         | |
|                                                         | |
|                                                         | |
|   KD09.1C  KD10.2C                                      | |
|                                                         | |
|                                                         | |
|               2114  2114     2114  2114  2114  2114     |-|
|                                                          |
|----------------------------------------------------------|


  driver by Pierpaolo Prazzoli

Notes:
  The hardware is very similar to Galaxian but has some differencies, like the 3bpp bitmap addition
  To advance input tests, press Tilt button

*/

#include "driver.h"
#include "vidhrdw/generic.h"


static struct tilemap *bg_tilemap;
static UINT8 nmi_enable = 0, jullyjgr_flip_screen_x = 0, jullyjgr_flip_screen_y = 0, bitmap_disable = 0, tilemap_bank = 0, pri = 0;
static UINT8 *jollyjgr_bitmap;
static UINT8 *bulletram;

static WRITE_HANDLER( jollyjgr_videoram_w )
{
	videoram[offset] = data;
	tilemap_mark_tile_dirty(bg_tilemap,offset);
}

static WRITE_HANDLER( jollyjgr_attrram_w )
{
	if(offset & 1)
	{
		/* color change */
		int i;

		for (i = offset >> 1; i < 0x0400; i += 32)
			tilemap_mark_tile_dirty(bg_tilemap, i);
	}
	else
	{
		tilemap_set_scrolly(bg_tilemap, offset >> 1, data);
	}

	colorram[offset] = data;
}

static WRITE_HANDLER( jollyjgr_misc_w )
{
	/* they could be swapped, because it always set "data & 3" */
	jullyjgr_flip_screen_x = data & 1;
	jullyjgr_flip_screen_y = data & 2;

	/* same for these two (used by Frog & Spiders) */
	bitmap_disable = data & 0x40;
	tilemap_bank = data & 0x20;

    pri = data & 4;

	tilemap_set_flip(bg_tilemap, (jullyjgr_flip_screen_x ? TILEMAP_FLIPX : 0) | (jullyjgr_flip_screen_y ? TILEMAP_FLIPY : 0));

	nmi_enable = data & 0x80;
}

static WRITE_HANDLER( jollyjgr_coin_lookout_w )
{
	coin_lockout_global_w(data & 1);

	/* bits 4, 5, 6 and 7 are used too */
}

static MEMORY_READ_START( jollyjgr_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8ff8, 0x8ff8, input_port_0_r },
	{ 0x8ff9, 0x8ff9, input_port_1_r },
	{ 0x8ffa, 0x8ffa, input_port_2_r },
	{ 0x8fff, 0x8fff, input_port_3_r },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9800, 0x983f, MRA_RAM },
	{ 0x9840, 0x987f, MRA_RAM },
	{ 0x9880, 0x9bff, MRA_RAM },
	{ 0xa000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( jollyjgr_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8ff8, 0x8ff8, AY8910_control_port_0_w },
	{ 0x8ffa, 0x8ffa, AY8910_write_port_0_w },
	{ 0x8ffc, 0x8ffc, jollyjgr_misc_w },
	{ 0x8ffd, 0x8ffd, jollyjgr_coin_lookout_w },
	{ 0x9000, 0x93ff, jollyjgr_videoram_w, &videoram },
	{ 0x9800, 0x983f, jollyjgr_attrram_w, &colorram },
	{ 0x9840, 0x987f, MWA_RAM, &spriteram },
	{ 0x9880, 0x9bff, MWA_RAM },
	{ 0xa000, 0xffff, MWA_RAM, &jollyjgr_bitmap },
MEMORY_END

static MEMORY_READ_START( fspider_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8ff8, 0x8ff8, input_port_0_r },
	{ 0x8ff9, 0x8ff9, input_port_1_r },
	{ 0x8ffa, 0x8ffa, input_port_2_r },
	{ 0x8fff, 0x8fff, input_port_3_r },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9800, 0x983f, MRA_RAM },
	{ 0x9840, 0x987f, MRA_RAM },
	{ 0x9880, 0x989f, MRA_RAM }, /* ? */
	{ 0x98a0, 0x98af, MRA_RAM },
	{ 0x98b0, 0x9bff, MRA_RAM }, /* ? */
	{ 0xa000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( fspider_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8ff8, 0x8ff8, AY8910_control_port_0_w },
	{ 0x8ffa, 0x8ffa, AY8910_write_port_0_w },
	{ 0x8ffc, 0x8ffc, jollyjgr_misc_w },
	{ 0x8ffd, 0x8ffd, jollyjgr_coin_lookout_w },
	{ 0x9000, 0x93ff, jollyjgr_videoram_w, &videoram },
	{ 0x9800, 0x983f, jollyjgr_attrram_w, &colorram },
	{ 0x9840, 0x987f, MWA_RAM, &spriteram },
	{ 0x9880, 0x989f, MWA_RAM }, /* ? */
	{ 0x98a0, 0x98af, MWA_RAM, &bulletram },
	{ 0x98b0, 0x9bff, MWA_RAM }, /* ? */
	{ 0xa000, 0xffff, MWA_RAM, &jollyjgr_bitmap },
MEMORY_END


INPUT_PORTS_START( jollyjgr )
	PORT_START
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x03, "10000" )
	PORT_DIPSETTING(    0x02, "20000" )
	PORT_DIPSETTING(    0x01, "30000" )
	PORT_DIPSETTING(    0x00, "40000" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x10, "Timer" )
	PORT_DIPSETTING(    0x18, "2 min 20 sec" )
	PORT_DIPSETTING(    0x10, "2 min 40 sec" )
	PORT_DIPSETTING(    0x08, "3 min" )
	PORT_DIPSETTING(    0x00, "3 min 20 sec" )
	PORT_SERVICE( 0x20, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Flip_Screen ) ) /* it works only when Cabinet is set to Upright */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN |  IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT |  IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP |    IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN |  IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT |  IPF_4WAY | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x10, 0x00, "Display Coinage" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, "Display Year" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, "No Hit" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Number of Coin Switches" )
	PORT_DIPSETTING(    0x80, "1" )
	PORT_DIPSETTING(    0x00, "2" )
INPUT_PORTS_END


INPUT_PORTS_START( fspider )
	PORT_START
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_8C ) )

	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_8C ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_TILT )

	PORT_START
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x04, "20000" )
	PORT_DIPSETTING(    0x08, "30000" )
	PORT_DIPSETTING(    0x0c, "40000" )
	PORT_DIPNAME( 0x10, 0x10, "Display Coinage Settings" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, "Show only 1P Coinage" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END


static PALETTE_INIT( jollyjgr )
{
	int i;

	/* tilemap / sprites palette */
	for (i = 0;i < 32;i++)
	{
		int bit0,bit1,bit2,r,g,b;

		/* red component */
		bit0 = BIT(*color_prom,0);
		bit1 = BIT(*color_prom,1);
		bit2 = BIT(*color_prom,2);
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = BIT(*color_prom,3);
		bit1 = BIT(*color_prom,4);
		bit2 = BIT(*color_prom,5);
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = BIT(*color_prom,6);
		bit1 = BIT(*color_prom,7);
		b = 0x4f * bit0 + 0xa8 * bit1;

		palette_set_color(i,r,g,b);
		color_prom++;
	}

	/* bitmap palette */
	for (i = 0;i < 8;i++)
		palette_set_color(32 + i, pal1bit(i >> 0), pal1bit(i >> 1), pal1bit(i >> 2));
}

/* Tilemap is the same as in Galaxian */
static void get_bg_tile_info(int tile_index)
{
	int color = colorram[((tile_index & 0x1f) << 1) | 1] & 7;
	int region = (tilemap_bank & 0x20) ? 2 : 0;
	SET_TILE_INFO(region, videoram[tile_index], color, 0);
}

VIDEO_START( jollyjgr )
{
	bg_tilemap = tilemap_create(get_bg_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8, 8, 32, 32);

	if(!bg_tilemap)
		return 1;

	tilemap_set_transparent_pen(bg_tilemap, 0);
	tilemap_set_scroll_cols(bg_tilemap, 32);

	return 0;
}

static void draw_bitmap(struct mame_bitmap *bitmap)
{
	int x,y,count;
	int i, bit0, bit1, bit2;
	int color;

	count = 0;
	for (y=0;y<256;y++)
	{
		for (x=0;x<256/8;x++)
		{
			for(i = 0; i < 8; i++)
			{
				bit0 = (jollyjgr_bitmap[count] >> i) & 1;
				bit1 = (jollyjgr_bitmap[count + 0x2000] >> i) & 1;
				bit2 = (jollyjgr_bitmap[count + 0x4000] >> i) & 1;
				color = bit0 | (bit1 << 1) | (bit2 << 2);

				if(color)
				{
					if(jullyjgr_flip_screen_x && jullyjgr_flip_screen_y)
						plot_pixel(bitmap, x*8 + i, y, Machine->pens[color + 32]);
					else if(jullyjgr_flip_screen_x && !jullyjgr_flip_screen_y)
						plot_pixel(bitmap, x*8 + i, 255 - y, Machine->pens[color + 32]);
					else if(!jullyjgr_flip_screen_x && jullyjgr_flip_screen_y)
						plot_pixel(bitmap, 255 - x*8 - i, y, Machine->pens[color + 32]);
					else
						plot_pixel(bitmap, 255 - x*8 - i, 255 - y, Machine->pens[color + 32]);
				}
			}

			count++;
		}
	}
}

VIDEO_UPDATE( jollyjgr )
{
	int offs;

	fillbitmap(bitmap,Machine->pens[32],cliprect);

	if(pri) /* used in Frog & Spiders level 3 */
	{
		if(!(bitmap_disable))
			draw_bitmap(bitmap);

		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);
	}
	else
	{
		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);

		if(!(bitmap_disable))
			draw_bitmap(bitmap);
	}

	/* Sprites are the same as in Galaxian */
	for(offs = 0; offs < 0x40; offs += 4)
	{
		int sx = spriteram[offs + 3] + 1;
		int sy = spriteram[offs];
		int flipx = spriteram[offs + 1] & 0x40;
		int flipy = spriteram[offs + 1] & 0x80;
		int code = spriteram[offs + 1] & 0x3f;
		int color = spriteram[offs + 2] & 7;

		if (jullyjgr_flip_screen_x)
		{
			sx = 240 - sx;
			flipx = !flipx;
		}

		if (jullyjgr_flip_screen_y)
		{
			flipy = !flipy;
		}
		else
		{
			sy = 240 - sy;
		}

		if (offs < 3*4)  sy++;

		drawgfx(bitmap,Machine->gfx[1],
				code,color,
				flipx,flipy,
				sx,sy,
				cliprect,TRANSPARENCY_PEN,0);

	}
}

VIDEO_UPDATE( fspider )
{
	int offs, x;

	fillbitmap(bitmap,Machine->pens[32],cliprect);

	if(pri) /* used in Frog & Spiders level 3 */
	{
		if(!(bitmap_disable))
			draw_bitmap(bitmap);

		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);
	}
	else
	{
		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);

		if(!(bitmap_disable))
			draw_bitmap(bitmap);
	}

	/* Sprites are the same as in Galaxian */
	for(offs = 0; offs < 0x40; offs += 4)
	{
		int sx = spriteram[offs + 3] + 1;
		int sy = spriteram[offs];
		int flipx = spriteram[offs + 1] & 0x40;
		int flipy = spriteram[offs + 1] & 0x80;
		int code = spriteram[offs + 1] & 0x3f;
		int color = spriteram[offs + 2] & 7;

		if (jullyjgr_flip_screen_x)
		{
			sx = 240 - sx;
			flipx = !flipx;
		}

		if (jullyjgr_flip_screen_y)
		{
			flipy = !flipy;
		}
		else
		{
			sy = 240 - sy;
		}

		if (offs < 3*4)  sy++;

		drawgfx(bitmap,Machine->gfx[1],
				code,color,
				flipx,flipy,
				sx,sy,
				cliprect,TRANSPARENCY_PEN,0);

	}

	/* Draw bullets
	16 bytes, 2 bytes per bullet (y,x). 2 player bullets, 6 enemy bullets.
	Assume bullets to look the same as on Galaxian hw,
	that is, simply 4 pixels. Colours are unknown. */
	for (offs=0;offs<0x10;offs+=2) {
		UINT8 sy=~bulletram[offs];
		UINT8 sx=~bulletram[offs|1];
		UINT16 bc=(offs<4)?
			32+7: /* player, white */
			32+3; /* enemy, yellow */

		if (jullyjgr_flip_screen_y) sy^=0xff;
		if (jullyjgr_flip_screen_y) sx+=8;

		if (sy>=cliprect->min_y && sy<=cliprect->max_y)
			for (x=sx-4;x<sx;x++)
				if (x>=cliprect->min_x && x<=cliprect->max_x)
				((UINT16 *)(bitmap->line[sy]))[x]=bc;
	}

}


static struct GfxLayout jollyjgr_charlayout =
{
	8,8,
	RGN_FRAC(1,2),
	2,
	{ RGN_FRAC(0,2), RGN_FRAC(1,2) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout jollyjgr_spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	2,
	{ RGN_FRAC(0,2), RGN_FRAC(1,2) },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8
};

static struct GfxDecodeInfo jollyjgr_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &jollyjgr_charlayout,   0, 8 },
	{ REGION_GFX2, 0, &jollyjgr_spritelayout, 0, 8 },
	{ REGION_GFX3, 0, &jollyjgr_charlayout,   0, 8 },
	{ -1 }
};

static INTERRUPT_GEN( jollyjgr_interrupt )
{
	if(nmi_enable)
		cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
}

static struct AY8910interface ay8910_interface =
{
	1,
	3579545,
	{45},
	{0},
	{0},
	{0},
	{0}
};

static MACHINE_DRIVER_START( jollyjgr )
	MDRV_CPU_ADD(Z80, 3579545)		 /* 3,579545 MHz */
	MDRV_CPU_MEMORY(jollyjgr_readmem,jollyjgr_writemem)
	MDRV_CPU_VBLANK_INT(jollyjgr_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)

	MDRV_GFXDECODE(jollyjgr_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(32+8) /* 32 for tilemap and sprites + 8 for the bitmap */

	MDRV_PALETTE_INIT(jollyjgr)
	MDRV_VIDEO_START(jollyjgr)
	MDRV_VIDEO_UPDATE(jollyjgr)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( fspider )
	MDRV_CPU_ADD(Z80, 3579545)		 /* 3,579545 MHz */
	MDRV_CPU_MEMORY(fspider_readmem,fspider_writemem)
	MDRV_CPU_VBLANK_INT(jollyjgr_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)

	MDRV_GFXDECODE(jollyjgr_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(32+8) /* 32 for tilemap and sprites + 8 for the bitmap */

	MDRV_PALETTE_INIT(jollyjgr)
	MDRV_VIDEO_START(jollyjgr)
	MDRV_VIDEO_UPDATE(fspider)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
MACHINE_DRIVER_END


ROM_START( jollyjgr )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "kd14.8a",      0x0000, 0x1000, CRC(404cfa2b) SHA1(023abecbc614d1deb6a239906f62e25bb688ac14) )
	ROM_LOAD( "kd15.8b",      0x1000, 0x1000, CRC(4cdc4c8b) SHA1(07257863a2de3a0e6bc1b41b8dcaae8c89bc4720) )
	ROM_LOAD( "kd16.8c",      0x2000, 0x1000, CRC(a2fa3500) SHA1(b85439e43a31c3445420896c231ac59f95331226) )
	ROM_LOAD( "kd17.8d",      0x3000, 0x1000, CRC(a5ab8c89) SHA1(b5a41581bba1c26ac3819aded29aeb48a5de64f8) )
	ROM_LOAD( "kd18.8e",      0x4000, 0x1000, CRC(8547c9a7) SHA1(43319a2271a294c2876e87a30bb7667b67582b76) )
	ROM_LOAD( "kd19.8f",      0x5000, 0x1000, CRC(2d0ed544) SHA1(547bcecd7d97f343de721d6af5f13ae6f787d838) )
	ROM_LOAD( "kd20.8h",      0x6000, 0x1000, CRC(017a0e5a) SHA1(23e066fea44690279ff7b3b65b03e4096e4d2984) )
	ROM_LOAD( "kd21.8j",      0x7000, 0x1000, CRC(e4faed01) SHA1(9b9afaff6cc4addfed7c1929a0d845bfbe9d18cc) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "kd09.1c",      0x0000, 0x0800, CRC(ecafd535) SHA1(a1f0bee247e6ab4f9fc3578560b62f5913b4ece2) )
	ROM_LOAD( "kd10.2c",      0x0800, 0x0800, CRC(e40fc594) SHA1(1a9bd670dda0405600e4c4d0107b881906969991) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "kd11.5h",      0x0000, 0x0800, CRC(d686245c) SHA1(73567b15d9399e450121ad01ad2dcb91bedc1099) )
	ROM_LOAD( "kd12.7h",      0x0800, 0x0800, CRC(d69cbb4e) SHA1(f33cc161f93cae9cc314067fa2453838fa8ac3ba) )

	ROM_REGION( 0x2000, REGION_GFX3, ROMREGION_ERASE00 )

	/* it's identical to kd14.8a, except for the first 32 bytes which are palette bytes */
	ROM_REGION( 0x1000, REGION_PROMS, 0 )
	ROM_LOAD( "kd13.1f",      0x0000, 0x1000, CRC(4f4e4e13) SHA1(a8fe0e1fd354e6cc2cf65eab66882c3b98c82100) )
ROM_END

ROM_START( fspiderb )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "1.5l",   0x0000, 0x1000, CRC(3679cab1) SHA1(21c055a1e6e42dda7070452a5de9bce01fa256b1) )
	ROM_LOAD( "2.8b",   0x7000, 0x1000, CRC(6e543acf) SHA1(45b66068654aabfe9745c8c893424a6d360a1748) )
	ROM_LOAD( "3.8c",   0x6000, 0x1000, CRC(f74f83d9) SHA1(18fdd7ebf5096b022207de4504e254860428f469) )
	ROM_LOAD( "4.8e",   0x5000, 0x1000, CRC(26add629) SHA1(96a2691f81ea7d0215d99c45f7f6dad4a4a6844e) )
	ROM_LOAD( "5.8f",   0x4000, 0x1000, CRC(0457de7b) SHA1(e7a7765d971d328333e8784178762c6fe4e2d783) )
	ROM_LOAD( "6.8h",   0x2000, 0x1000, CRC(329d4716) SHA1(898d0f3d053a8ac90c731f39c1d05769882cdcf6) )
	ROM_LOAD( "7.8j",   0x3000, 0x1000, CRC(a7d8fc3c) SHA1(3b39155001d21e75f16196bf3c11b34fb6d5fa0b) )
	ROM_RELOAD(         0x1000, 0x1000 )

	ROM_REGION( 0x4000, REGION_USER1, 0 )
	ROM_LOAD(  "8.1c",   0x0000, 0x1000, CRC(4e39abad) SHA1(225a2a08a7afe404e6b74789aab8c97a39a21214) )
	ROM_LOAD(  "9.2c",   0x1000, 0x1000, CRC(04dd1604) SHA1(9e686b09e2fc59fa879fd62982adb1c681f3eb73) )
	ROM_LOAD( "10.5h",   0x2000, 0x1000, CRC(d4bce323) SHA1(f49df8318aa9e8bd49fad1931480dfd483a0248a) )
	ROM_LOAD( "11.7h",   0x3000, 0x1000, CRC(7ab56309) SHA1(b43f542a7359c3a4ccf6f116e3a84bd13af6876f) )

	ROM_REGION( 0x1000, REGION_GFX1, 0 )
	ROM_COPY( REGION_USER1, 0x0000, 0x0000, 0x800)
	ROM_COPY( REGION_USER1, 0x1000, 0x0800, 0x800)

	ROM_REGION( 0x1000, REGION_GFX2, 0 )
	ROM_COPY( REGION_USER1, 0x2000, 0x0000, 0x800)
	ROM_COPY( REGION_USER1, 0x3000, 0x0800, 0x800)

	ROM_REGION( 0x1000, REGION_GFX3, ROMREGION_ERASE00 )
	ROM_COPY( REGION_USER1, 0x0800, 0x0400, 0x400)
	ROM_COPY( REGION_USER1, 0x1800, 0x0c00, 0x400)
/*	ROM_COPY( REGION_USER1, 0x2800, 0x1000, 0x800) */
/*	ROM_COPY( REGION_USER1, 0x3800, 0x1800, 0x800) */

	ROM_REGION( 0x1000, REGION_PROMS, 0 )
	ROM_LOAD( "82s123.1f", 0x0000, 0x0020, CRC(cda6001a) SHA1(e10fe848e8123e53bd2db8a14cfa2d8c6621d6aa) )
ROM_END

GAME( 1981, fspiderb, 0, fspider,  fspider,  0, ROT90, "Taito Corporation", "Frog & Spiders (bootleg?)" ) /* comes from a Fawaz Group bootleg(?) board */
GAME( 1982, jollyjgr, 0, jollyjgr, jollyjgr, 0, ROT90, "Taito Corporation", "Jolly Jogger" )
