/* One Shot One Kill Video Hardware */

#include "driver.h"

extern data16_t *oneshot_sprites;
extern data16_t *oneshot_bg_videoram;
extern data16_t *oneshot_mid_videoram;
extern data16_t *oneshot_fg_videoram;
extern data16_t *oneshot_scroll;

extern int gun_x_p1,gun_y_p1,gun_x_p2,gun_y_p2;
extern int gun_x_shift;


static struct tilemap *oneshot_bg_tilemap;
static struct tilemap *oneshot_mid_tilemap;
static struct tilemap *oneshot_fg_tilemap;

/* bg tilemap */
static void get_oneshot_bg_tile_info(int tile_index)
{
	int tileno;

	tileno = oneshot_bg_videoram[tile_index*2+1];

	SET_TILE_INFO(0,tileno,0,0)
}

WRITE16_HANDLER( oneshot_bg_videoram_w )
{
	if (oneshot_bg_videoram[offset] != data)
	{
		COMBINE_DATA(&oneshot_bg_videoram[offset]);
		tilemap_mark_tile_dirty(oneshot_bg_tilemap,offset/2);
	}
}

/* mid tilemap */
static void get_oneshot_mid_tile_info(int tile_index)
{
	int tileno;

	tileno = oneshot_mid_videoram[tile_index*2+1];

	SET_TILE_INFO(0,tileno,2,0)
}

WRITE16_HANDLER( oneshot_mid_videoram_w )
{
	if (oneshot_mid_videoram[offset] != data)
	{
		COMBINE_DATA(&oneshot_mid_videoram[offset]);
		tilemap_mark_tile_dirty(oneshot_mid_tilemap,offset/2);
	}
}


/* fg tilemap */
static void get_oneshot_fg_tile_info(int tile_index)
{
	int tileno;

	tileno = oneshot_fg_videoram[tile_index*2+1];

	SET_TILE_INFO(0,tileno,3,0)
}

WRITE16_HANDLER( oneshot_fg_videoram_w )
{
	if (oneshot_fg_videoram[offset] != data)
	{
		COMBINE_DATA(&oneshot_fg_videoram[offset]);
		tilemap_mark_tile_dirty(oneshot_fg_tilemap,offset/2);
	}
}

VIDEO_START( oneshot )
{
	oneshot_bg_tilemap = tilemap_create(get_oneshot_bg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 16, 16,32,32);
	oneshot_mid_tilemap = tilemap_create(get_oneshot_mid_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 16, 16,32,32);
	oneshot_fg_tilemap = tilemap_create(get_oneshot_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 16, 16,32,32);

	tilemap_set_transparent_pen(oneshot_bg_tilemap,0);
	tilemap_set_transparent_pen(oneshot_mid_tilemap,0);
	tilemap_set_transparent_pen(oneshot_fg_tilemap,0);

	return 0;
}

static void oneshot_drawcrosshairs( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
    int xpos,ypos;
    /* get gun raw coordonates (player 1) */
    gun_x_p1 = (readinputport(5) & 0xff) * 320 / 256;
    gun_y_p1 = (readinputport(6) & 0xff) * 240 / 256;

    /* compute the coordonates for drawing (from routine at 0x009ab0) */
    xpos = gun_x_p1;
    ypos = gun_y_p1;

    gun_x_p1+=gun_x_shift;

    gun_y_p1 -= 0x0a;
    if (gun_y_p1 < 0)
        gun_y_p1=0;

    /* draw crosshair */
    draw_crosshair( 1, bitmap, xpos, ypos, cliprect );


    /* get gun raw coordonates (player 2) */
    gun_x_p2 = (readinputport(7) & 0xff) * 320 / 256;
    gun_y_p2 = (readinputport(8) & 0xff) * 240 / 256;
    /* compute the coordonates for drawing (from routine at 0x009b6e) */
    xpos = gun_x_p2;
    ypos = gun_y_p2;

    gun_x_p2 += gun_x_shift-0x0a;
    if (gun_x_p2 < 0)
        gun_x_p2=0;

    /* draw crosshair */
    draw_crosshair( 2, bitmap, xpos, ypos, cliprect );
}

static void oneshot_drawsprites( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	const UINT16 *source = oneshot_sprites;
	const UINT16 *finish = source+(0x1000/2);
	const struct GfxElement *gfx = Machine->gfx[1];

	int xpos,ypos;

	while( source<finish )
	{
		int blockx,blocky;
		int num = source[1] & 0xffff;
		int xsize = (source[2] & 0x000f)+1;
		int ysize = (source[3] & 0x000f)+1;

		ypos = source[3] & 0xff80;
		xpos = source[2] & 0xff80;

		ypos = ypos >> 7;
		xpos = xpos >> 7;


		if (source[0] == 0x0001) break;

		xpos -= 8;
		ypos -= 6;


		for (blockx = 0; blockx<xsize;blockx++) {
			for (blocky = 0; blocky<ysize;blocky++) {


				drawgfx(
						bitmap,
						gfx,
						num+(blocky*xsize)+blockx,
						1,
						0,0,
						xpos+blockx*8,ypos+blocky*8,
						cliprect,
						TRANSPARENCY_PEN,0
						);

				drawgfx(
						bitmap,
						gfx,
						num+(blocky*xsize)+blockx,
						1,
						0,0,
						xpos+blockx*8-0x200,ypos+blocky*8,
						cliprect,
						TRANSPARENCY_PEN,0
						);
			}
		}
		source += 0x4;
	}

}

VIDEO_UPDATE( oneshot )
{
	fillbitmap(bitmap, get_black_pen(), cliprect);

	tilemap_set_scrollx(oneshot_mid_tilemap,0, oneshot_scroll[0]-0x1f5);
	tilemap_set_scrolly(oneshot_mid_tilemap,0, oneshot_scroll[1]);

	tilemap_draw(bitmap,cliprect,oneshot_bg_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,oneshot_mid_tilemap,0,0);
	oneshot_drawsprites(bitmap,cliprect);
	tilemap_draw(bitmap,cliprect,oneshot_fg_tilemap,0,0);
	oneshot_drawcrosshairs(bitmap,cliprect);
}

VIDEO_UPDATE( maddonna )
{
	fillbitmap(bitmap, get_black_pen(), cliprect);

	tilemap_set_scrolly(oneshot_mid_tilemap,0, oneshot_scroll[1]); /* other registers aren't used so we don't know which layers they relate to*/

	tilemap_draw(bitmap,cliprect,oneshot_mid_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,oneshot_fg_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,oneshot_bg_tilemap,0,0);
	oneshot_drawsprites(bitmap,cliprect);
/*	oneshot_drawcrosshairs(bitmap,cliprect); // not a gun game*/

/*	usrintf_showmessage	("%04x %04x %04x %04x %04x %04x %04x %04x", oneshot_scroll[0],oneshot_scroll[1],oneshot_scroll[2],oneshot_scroll[3],oneshot_scroll[4],oneshot_scroll[5],oneshot_scroll[6],oneshot_scroll[7]);*/
}
