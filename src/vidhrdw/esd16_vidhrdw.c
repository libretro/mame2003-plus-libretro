/***************************************************************************

						  -= ESD 16 Bit Games =-

					driver by	Luca Elia (l.elia@tin.it)


Note:	if MAME_DEBUG is defined, pressing Z with:

		Q / W			Shows Layer 0 / 1
		A				Shows Sprites

		Keys can be used together!


	[ 2 Scrolling Layers ]

		Tile Size:				8 x 8 x 8
		Color Codes:			1 per Layer
		Layer Size (tiles) :	128 x 64
		Layer Size (pixels):	1024 x 512

	[ 256 Sprites ]

		Sprites are made of 16 x 16 x 5 tiles. Size can vary from 1 to
		8 tiles vertically, while their width is always 1 tile.

	[ Priorities ]

		The game only uses this scheme:

		Back -> Front:	Layer 0, Layer 1, Sprites

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* Variables needed by drivers: */

data16_t *esd16_vram_0, *esd16_scroll_0;
data16_t *esd16_vram_1, *esd16_scroll_1;

//extern data16_t *head_unknown1;
extern data16_t *head_layersize;
//extern data16_t *head_unknown3;
//extern data16_t *head_unknown4;
//extern data16_t *head_unknown5;

/* Functions defined in vidhrdw: */

WRITE16_HANDLER( esd16_vram_0_w );
WRITE16_HANDLER( esd16_vram_1_w );

VIDEO_START( esd16 );
VIDEO_UPDATE( esd16 );


/***************************************************************************

									Tilemaps

	Offset: 	Bits:					Value:

		0.w								Code

	No color code:	layer 0 (backmost) usues the second 256 colors,
					layer 1 the first 256.

***************************************************************************/

struct tilemap *esdtilemap_0, *esdtilemap_1, *esdtilemap_1_16x16;

static void get_tile_info_0(int tile_index)
{
	data16_t code = esd16_vram_0[tile_index];
	SET_TILE_INFO(
			1,
			code,
			1,
			0)
}

static void get_tile_info_1(int tile_index)
{
	data16_t code = esd16_vram_1[tile_index];
	SET_TILE_INFO(
			1,
			code,
			0,
			0)
}

static void get_tile_info_1_16x16(int tile_index)
{
	data16_t code = esd16_vram_1[tile_index];
	SET_TILE_INFO(
			2,
			code,
			0,
			0)
}

WRITE16_HANDLER( esd16_vram_0_w )
{
	data16_t old_data	=	esd16_vram_0[offset];
	data16_t new_data	=	COMBINE_DATA(&esd16_vram_0[offset]);
	if (old_data != new_data)	tilemap_mark_tile_dirty(esdtilemap_0,offset);
}

WRITE16_HANDLER( esd16_vram_1_w )
{
	data16_t old_data	=	esd16_vram_1[offset];
	data16_t new_data	=	COMBINE_DATA(&esd16_vram_1[offset]);
	if (old_data != new_data)
	{
		tilemap_mark_tile_dirty(esdtilemap_1,offset);
		tilemap_mark_tile_dirty(esdtilemap_1_16x16,offset);
	}
}


/***************************************************************************


							Video Hardware Init


***************************************************************************/


VIDEO_START( esd16 )
{
	esdtilemap_0 = tilemap_create(	get_tile_info_0, tilemap_scan_rows,
								TILEMAP_OPAQUE,			8,8,	0x80,0x40);

	esdtilemap_1 = tilemap_create(	get_tile_info_1, tilemap_scan_rows,
								TILEMAP_TRANSPARENT,	8,8,	0x80,0x40);

	/* hedpanic changes tilemap 1 to 16x16 at various times */
	esdtilemap_1_16x16 = tilemap_create(	get_tile_info_1_16x16, tilemap_scan_rows,
								TILEMAP_TRANSPARENT,	16,16,	0x40,0x40);

	if ( !esdtilemap_0 || !esdtilemap_1 || !esdtilemap_1_16x16 )
		return 1;

	tilemap_set_scrolldx(esdtilemap_0, -0x60 + 2, -0x60     );
	tilemap_set_scrolldx(esdtilemap_1, -0x60    , -0x60 + 2 );
	tilemap_set_scrolldx(esdtilemap_1_16x16, -0x60    , -0x60 + 2 );

	tilemap_set_transparent_pen(esdtilemap_0,0x00);
	tilemap_set_transparent_pen(esdtilemap_1,0x00);
	tilemap_set_transparent_pen(esdtilemap_1_16x16,0x00);

	return 0;
}




/***************************************************************************

								Sprites Drawing

	Offset: 	Bits:					Value:

		0.w		fedc b--- ---- ----
				---- -a9- ---- ----		Y Size: (1 << N) Tiles
				---- ---8 7654 3210		Y (Signed, Bottom-Up)

		2.w								Code

		4.w		fed- ---- ---- ----
				---c ---- ---- ----		Color?
				---- ba9- ---- ----		Color
				---- ---8 7654 3210		X (Signed)

		6.w		fedc ba9- ---- ----
				---- ---8 ---- ----		? 1 (Display Sprite?)
				---- ---- 7654 3210

- To Do: Flip X&Y ? They seem unused.

***************************************************************************/

static void esd16_draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int offs;

	int max_x		=	Machine->drv->screen_width;
	int max_y		=	Machine->drv->screen_height;

	for ( offs = 0; offs < spriteram_size/2; offs += 8/2 )
	{
		int y, starty, endy, incy;

		int	sy		=	spriteram16[ offs + 0 ];
		int	code	=	spriteram16[ offs + 1 ];
		int	sx		=	spriteram16[ offs + 2 ];
		int	attr	=	spriteram16[ offs + 3 ];

		int dimy	=	1 << ((sy >> 9) & 3);

		int	flipx	=	attr & 0x0000;
		int	flipy	=	attr & 0x0000;

		int color	=	(sx >> 9) & 0xf;

		sx	=	sx & 0x1ff;
		if (sx >= 0x180)	sx -= 0x200;

		sy	 =	0x100 - ((sy & 0xff)  - (sy & 0x100));
		sy	-=	dimy*16;

		if (flip_screen)
		{	flipx = !flipx;		sx = max_x - sx -    1 * 16 + 2;	// small offset
			flipy = !flipy;		sy = max_y - sy - dimy * 16;	}

		if (flipy)	{	starty = sy+(dimy-1)*16;	endy = sy-16;		incy = -16;	}
		else		{	starty = sy;				endy = sy+dimy*16;	incy = +16;	}

		for (y = starty ; y != endy ; y += incy)
		{
			drawgfx(	bitmap, Machine->gfx[0],
						code++,
						color,
						flipx, flipy,
						sx, y,
						cliprect, TRANSPARENCY_PEN,0	);
		}
	}
}

/* note, check if i can re-merge this with the other or if its really different */
static void hedpanic_draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int offs;

	int max_x		=	Machine->drv->screen_width;
	int max_y		=	Machine->drv->screen_height;

	for ( offs = 0; offs < spriteram_size/2; offs += 8/2 )
	{
		int y, starty, endy, incy;

		int	sy		=	spriteram16[ offs + 0 ];
		int	code	=	spriteram16[ offs + 1 ];
		int	sx		=	spriteram16[ offs + 2 ];
//		int	attr	=	spriteram16[ offs + 3 ];

		int dimy	=	1 << ((sy >> 9) & 3);

		int	flipx	=	spriteram16[ offs + 0 ] & 0x2000;
		int	flipy	=	sy & 0x0000;

		int color	=	(sx >> 9) & 0xf;

		sx	=	sx & 0x1ff;
		if (sx >= 0x180)	sx -= 0x200;

		sy &= 0x1ff;

		sx -= 24;

		sy = 0x1ff-sy;

		if (flip_screen)
		{	flipx = !flipx;		sx = max_x - sx -    1 * 16 + 2;	// small offset
			flipy = !flipy;		sy = max_y - sy - dimy * 16;	}

		if (flipy)	{	starty = sy+(dimy-1)*16;	endy = sy-16;		incy = -16;	}
		else		{	starty = sy-dimy*16;				endy = sy;	incy = +16;	}

		for (y = starty ; y != endy ; y += incy)
		{
			drawgfx(	bitmap, Machine->gfx[0],
						code++,
						color,
						flipx, flipy,
						sx, y,
						cliprect, TRANSPARENCY_PEN,0	);
		}
	}
}



/***************************************************************************


								Screen Drawing


***************************************************************************/

VIDEO_UPDATE( esd16 )
{
	int layers_ctrl = -1;

	tilemap_set_scrollx(esdtilemap_0, 0, esd16_scroll_0[0]);
	tilemap_set_scrolly(esdtilemap_0, 0, esd16_scroll_0[1]);

	tilemap_set_scrollx(esdtilemap_1, 0, esd16_scroll_1[0]);
	tilemap_set_scrolly(esdtilemap_1, 0, esd16_scroll_1[1]);

#ifdef MAME_DEBUG
if ( keyboard_pressed(KEYCODE_Z) )
{	int msk = 0;
	if (keyboard_pressed(KEYCODE_Q))	msk |= 1;
	if (keyboard_pressed(KEYCODE_W))	msk |= 2;
	if (keyboard_pressed(KEYCODE_A))	msk |= 4;
	if (msk != 0) layers_ctrl &= msk;	}
#endif

	if (layers_ctrl & 1)	tilemap_draw(bitmap,cliprect,esdtilemap_0,0,0);
	else					fillbitmap(bitmap,Machine->pens[0],cliprect);

	if (layers_ctrl & 2)	tilemap_draw(bitmap,cliprect,esdtilemap_1,0,0);

	if (layers_ctrl & 4)	esd16_draw_sprites(bitmap,cliprect);
}


VIDEO_UPDATE( hedpanic )
{
	int layers_ctrl = -1;

	tilemap_set_scrollx(esdtilemap_0, 0, esd16_scroll_0[0]);
	tilemap_set_scrolly(esdtilemap_0, 0, esd16_scroll_0[1]);


#ifdef MAME_DEBUG
if ( keyboard_pressed(KEYCODE_Z) )
{	int msk = 0;
	if (keyboard_pressed(KEYCODE_Q))	msk |= 1;
	if (keyboard_pressed(KEYCODE_W))	msk |= 2;
	if (keyboard_pressed(KEYCODE_A))	msk |= 4;
	if (msk != 0) layers_ctrl &= msk;	}
#endif

	if (layers_ctrl & 1)	tilemap_draw(bitmap,cliprect,esdtilemap_0,0,0);
	else					fillbitmap(bitmap,Machine->pens[0],cliprect);

	if (layers_ctrl & 2)
	{
		if (head_layersize[0]&0x0002)
		{
			tilemap_set_scrollx(esdtilemap_1_16x16, 0, esd16_scroll_1[0]);
			tilemap_set_scrolly(esdtilemap_1_16x16, 0, esd16_scroll_1[1]);
			tilemap_draw(bitmap,cliprect,esdtilemap_1_16x16,0,0);
		}
		else
		{
			tilemap_set_scrollx(esdtilemap_1, 0, esd16_scroll_1[0]);
			tilemap_set_scrolly(esdtilemap_1, 0, esd16_scroll_1[1]);
			tilemap_draw(bitmap,cliprect,esdtilemap_1,0,0);
		}

	}
	if (layers_ctrl & 4)	hedpanic_draw_sprites(bitmap,cliprect);


//	usrintf_showmessage("%04x %04x %04x %04x %04x",head_unknown1[0],head_layersize[0],head_unknown3[0],head_unknown4[0],head_unknown5[0]);
}
