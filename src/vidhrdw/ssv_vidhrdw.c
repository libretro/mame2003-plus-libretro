/***************************************************************************

					-= Seta, Sammy, Visco (SSV) System =-

					driver by	Luca Elia (l.elia@tin.it)

	This hardware only generates sprites. But they're of various types,
	including some large "floating tilemap" ones.

	Sprites RAM is 0x40000 bytes long. The first 0x2000 bytes hold a list
	of sprites to display (the list can be made shorter using an end-of-list
	marker).

	Each entry in the list (8 bytes) is a multi-sprite (e.g it tells the
	hardware to display up to 32 single-sprites).

	The list looks like this:

	Offset: 	Bits:					Value:

        0.w     f--- ---- ---- ----     Shadow
                -edc ---- ---- ----     Each bit enables 2 bitplanes*
                ---- ba-- ---- ----     X Size (1,2,4,8 tiles)
                ---- --98 ---- ----     Y Size (1,2,4,8 tiles)
                ---- ---- 765- ----     Index of a scroll to apply to the single-sprite(s)
                ---- ---- ---4 3210     Number of single-sprites, minus 1

		2.w		f--- ---- ---- ----		List end
				-edc ba98 7654 3210		Offset of the single-sprite(s) data

		4.w		fedc ba-- ---- ----
				---- --98 7654 3210		X displacement (ignored by tilemap sprites?)

		4.w		fedc ba-- ---- ----
				---- --98 7654 3210		Y displacement (ignored by tilemap sprites?)


* bit c, which enables/disables the 2 high order bitplanes (256 / 64 color tiles)
  is the only one implemented. Needed by keithlcy (logo), drifto94 (wheels).

	A single-sprite can be:

	1. a rectangle of tiles (only 1 tile code needs to be specified)
	2. a row of tiles of a tilemap in ram. The row is (always?) as wide
	   as the screen and 64 pixels tall.

	Rectangle case(1):
	Offset: 	Bits:					Value:

		0.w								Code (low bits)

		2.w		f--- ---- ---- ----		Flip X
				-e-- ---- ---- ----		Flip Y
				--dc ba-- ---- ----		Code (high bits)
				---- --9- ---- ----		Code? Color?
				---- ---8 7654 3210		Color code (64 color steps)

        4.w     f--- ---- ---- ----     Shadow
                -edc ---- ---- ----     Each bit enables 2 bitplanes*
                ---- ba-- ---- ----     X Size (1,2,4,8 tiles)
                ---- --98 7654 3210     X

		6.w		fedc ---- ---- ----
				---- ba-- ---- ----		Y Size (1,2,4 tiles) **
				---- --98 7654 3210		Y


	Tilemap case(2):
	Offset: 	Bits:					Value:

		0.w		fedc ba98 7654 3---
				---- ---- ---- -210		Scroll index (see below)

		2.w								Always 0

		4.w		fedc ba-- ---- ----
				---- --98 7654 3210		X?

		6.w		fedc ---- ---- ----
				---- ba-- ---- ----		**
				---- --98 7654 3210		Y

** ? both bits set means "Row Sprite" if the single-sprite type hasn't been
     specified in the sprites list ?

	There are 8 scroll values for the tilemap sprites, in the
	1c0000-1c003f area (each scroll value uses 8 bytes):

	Offset: 	Bits:					Value:

		0.w								Scroll X

		2.w								Scroll Y

		4.w								? 0000, 05ff, 057f

        6.w     fed- ---- ---- ----     Tilemap width (games only use 1 -> $200, 2 -> $400)
                ---c ---- ---- ----     ?
                ---- b--- ---- ----     Shadow
                ---- -a98 ---- ----     Each bit enables 2 bitplanes*
                ---- ---- 7654 3210     ? some games leave it to 0, others
                                          use e.g 28 for scroll 0, 29 for
                                          scroll 1, 2a etc.

	Where scroll x&y refer to a virtual $8000 x $200 tilemap (filling the
	whole spriteram) made of 16x16 tiles. A tile uses 4 bytes:

	Offset: 	Bits:					Value:

		0.w								Code (low bits)***

		2.w		f--- ---- ---- ----		Flip X
				-e-- ---- ---- ----		Flip Y
				--dc ba-- ---- ----		Code (high bits)
				---- --9- ---- ----		Code? Color?
				---- ---8 7654 3210		Color code (64 color steps)

	The tilemap is stored in ram by column.

*** The tiles size is (always?) 16x16 so Code and Code+1 are used.


    Note that there is a "background layer": a series of tilemap sprites
    that fill up the screen and use scroll 0 as the source tilemap are
    always displayed before the sprites in the sprites list.

    Shadows:

    The low bits of the pens from a "shadowing" tile (regardless of color code)
    substitute the top bits of the color index (0-7fff) in the frame buffer.
    The number of low bits from the "shadowing tile" is 4 or 2, depending on
    bit 7 of 1c0077.

Note: press Z to show some info on each sprite (debug builds only)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "seta.h"

int shadow_pen_mask, shadow_pen_shift;

void ssv_drawgfx(	struct mame_bitmap *bitmap, const struct GfxElement *gfx,
					unsigned int code,unsigned int color,int flipx,int flipy,int x0,int y0,
					const struct rectangle *cliprect, int shadow	)
{
	UINT8 *source, *addr, pen;
	UINT16 *dest;
	int sx, x1, dx;
	int sy, y1, dy;

	addr	=	(code  % gfx->total_elements) * gfx->char_modulo + gfx->gfxdata;
	color	=	gfx->color_granularity * (color % gfx->total_colors);

	if ( flipx )	{	x1 = x0-1;				x0 += gfx->width-1;		dx = -1;	}
	else			{	x1 = x0 + gfx->width;							dx =  1;	}

	if ( flipy )	{	y1 = y0-1;				y0 += gfx->height-1;	dy = -1;	}
	else			{	y1 = y0 + gfx->height;							dy =  1;	}

#define SSV_DRAWGFX(SETPIXELCOLOR)												\
	for ( sy = y0; sy != y1; sy += dy )											\
	{																			\
		if ( sy >= cliprect->min_y && sy <= cliprect->max_y )					\
		{																		\
			source	=	addr;													\
			dest	=	(UINT16 *)bitmap->line[sy];								\
																				\
			for ( sx = x0; sx != x1; sx += dx )									\
			{																	\
				pen = *source++;												\
																				\
				if ( pen && sx >= cliprect->min_x && sx <= cliprect->max_x )	\
					SETPIXELCOLOR												\
			}																	\
		}																		\
																				\
		addr	+=	gfx->line_modulo;											\
	}

	if (shadow)
	{
		SSV_DRAWGFX( { dest[sx] = ((dest[sx] & shadow_pen_mask) | (pen << shadow_pen_shift)) & 0x7fff; } )
	}
	else
	{
		SSV_DRAWGFX( { dest[sx] = (color + pen) & 0x7fff; } )
	}
}


VIDEO_START( ssv )
{
	Machine->gfx[0]->color_granularity = 64; /* 256 colour sprites with palette selectable on 64 colour boundaries */

	return 0;
}

VIDEO_START( eaglshot )
{
	if ( video_start_ssv() )
		return 1;

	eaglshot_gfxram		=	(data16_t*)auto_malloc(16 * 0x40000);
	eaglshot_dirty_tile	=	(char*)auto_malloc(16 * 0x40000 / (16*8));

	if ( !eaglshot_gfxram || !eaglshot_dirty_tile )
		return 1;

	return 0;
}

static struct tilemap *gdfs_tmap;

static void get_tile_info_0(int tile_index)
{
	data16_t tile = gdfs_tmapram[tile_index];
	SET_TILE_INFO(3, tile, 0, TILE_FLIPXY( tile >> 14 ));
}

WRITE16_HANDLER( gdfs_tmapram_w )
{
	data16_t old_data	=	gdfs_tmapram[offset];
	data16_t new_data	=	COMBINE_DATA(&gdfs_tmapram[offset]);
	if (old_data != new_data)	tilemap_mark_tile_dirty(gdfs_tmap, offset);
}

VIDEO_START( gdfs )
{
	if ( video_start_ssv() )
		return 1;

	Machine->gfx[2]->color_granularity = 64; /* 256 colour sprites with palette selectable on 64 colour boundaries */

	eaglshot_gfxram		=	(data16_t*)auto_malloc(4 * 0x100000);
	eaglshot_dirty_tile	=	(char*)auto_malloc(4 * 0x100000 / (16*8));

	gdfs_tmap			=	tilemap_create(	get_tile_info_0, tilemap_scan_rows,
											TILEMAP_TRANSPARENT, 16,16, 0x100,0x100	);

	if ( !eaglshot_gfxram || !eaglshot_dirty_tile || !gdfs_tmap)
		return 1;

	tilemap_set_transparent_pen(gdfs_tmap, 0);

	return 0;
}

/* Scroll values + CRT controller registers */
data16_t *ssv_scroll;

int ssv_special;	/* kludge for hypreac2 */

int ssv_tile_code[16];

int ssv_sprites_offsx, ssv_sprites_offsy;
int ssv_tilemap_offsx, ssv_tilemap_offsy;

data16_t *eaglshot_gfxram, *gdfs_tmapram, *gdfs_tmapscroll;
char eaglshot_dirty, *eaglshot_dirty_tile;

/***************************************************************************

	CRT controller, registers that are read
	(vblank etc.?)

				1c0000 (wait for bit .. to become ..)

	keithlcy:	bit D, 0 -> 1

	mslider:	bit A, 0

	hypreact:
	meosism:
	srmp7:
	sxyreact:
	ultrax:		bit F, 0

	twineag2:
	hypreac2:	bit C, 1 -> 0
				bit F, 0

	janjans1:
	srmp4:
	survarts:	No checks

	ryorioh:
	drifto94:	bit D, 0 -> 1
				bit A, 0


	CRT controller, registers that are written
	(resolution, visible area, flipping etc. ?)

				1c0060-7f:

	drifto94:	0000 0025 00cd 01c6 - 0001 0013 0101 0106
				0300 0711 0500 0000 - 0015 5940 0000 0000
				03ea      5558	(flip)

	hypreact:	0021 0022 00cb 01c6 - 0001 000e 00fe 0106
				0301 0000 0500 c000 - 0015 5140 0000 0000
				03f0      5558	(flip)

	hypreac2:	0021 0022 00cb 01c6 - 0001 000e 00fe 0106
				0301 0000 05ff c000 - 0015 5140 0000 0000
				03ea      5558	(flip)

	janjans1:	0021 0023 00cb 01c6 - 0001 000f 00fe 0106
				0300 0000 0500 c000 - 0015 5140 0000 0000
				same!	(flip)

	keithlcy:	002b 0025 00cd 01c6 - 0001 0013 0101 0106
				0300 0711 0500 0000 - 0015 5940 0000 0000
				03ea      5558	(flip)

	meosism:	002b 002c 00d5 01c6 - 0001 0012 00fe 0106
				0301 0000 0500 c000 - 0015 5140 0000 0000
				(no flip)

	mslider:	0021 0026 00d6 01c6 - 0001 000e 00fe 0106
				03f1 0711 5550 c080 - 0015 5940 0000 0000
				0301      0500	(flip)

	ryorioh:	0021 0023*00cb 01c6 - 0001 000f 00fe 0106
				0300 0000 0500 c000 - 0015 5140 0000 0000
				03ed      5558	(flip) *0025

	srmp4:		002b 002c 00d4 01c6 - 0001 0012 0102 0106
				0301 0711 0500 0000 - 0015 4940 0000 0000
				ffe8      5557	(flip)

	srmp7:		002b 002c 00d4 01c6 - 0001 000e 00fd 0106
				0000 0000 e500 0000 - 0015 7140 0000 0000
				02f2      b558	(flip)

	stmblade:	0021 0026 00d6 01c6 - 0001 000e 00fe 0106
				03f1 0711 5550 c080 - 0015 5940 0000 0000 <- 711 becomes 0 during gameplay
				0301      0500	(flip)

	sxyreact:	0021 0022 00cb 01c6 - 0001 000e 00fe 0106
				0301 0000 0500 c000 - 0015 5140 0000 0000
				03ef      5558	(flip)

	survarts:	002b 002c 00d4 01c6 - 0001 0012 0102 0106
				0301 0000 0500 0000 - 0015 5140 0000 0000
				03e9      5558	(flip)

***************************************************************************/

READ16_HANDLER( ssv_vblank_r )
{
	if (cpu_getvblank())
		return 0x2000 | 0x1000;
	else
		return 0x0000;
}

WRITE16_HANDLER( ssv_scroll_w )
{
	COMBINE_DATA(ssv_scroll + offset);

/*	offsets 60-7f: CRT Controller	*/

}

WRITE16_HANDLER( paletteram16_xrgb_swap_word_w )
{
	int r, g, b;
	data16_t data0, data1;

	COMBINE_DATA(paletteram16 + offset);

	offset &= ~1;

	data0 = paletteram16[offset + 1];
	data1 = paletteram16[offset];

	r = data0 & 0xff;
	g = data1 >> 8;
	b = data1 & 0xff;

	palette_set_color(offset>>1, r, g, b);

	if (!(Machine->drv->video_attributes & VIDEO_NEEDS_6BITS_PER_GUN))
		usrintf_showmessage("driver should use VIDEO_NEEDS_6BITS_PER_GUN flag");
}

/***************************************************************************


								Sprites Drawing

[mslider]
"Insert coins"			101f60: 0006 0825 00b0 000c
						104128: 1a3a 0000 63d4 0400 (16x16)

Character tilemap		100080: 000f 0410 0000 0000
						102080: 0002 0000 6200 0c00
								0002 0000 6200 0c40
								0002 0000 6200 0c80
								0002 0000 6200 0cc0
								0002 0000 6200 0d00
								..
								0002 0000 6200 0fc0
						1c0010:	73f0 00f1 05ff 4600


[hypreact]
intro					100140: 6003 04ca 0000 0000 (tilemap sprite)
						102650: 0003 0000 0000 0c00
								0003 0000 0000 0c40
								0003 0000 0000 0c80
								0003 0000 0000 0cc0
						1c0018:	15f8 00e8 057f 2600

game					100058: 6003 00db 0050 0040 (normal sprite)
						1006d8: 037c 001b 6400 0400

tiles					100060: 6019 0120 0014 000a	(sometimes 6619!)
						100900: 0a8c 0012 6400 0420 (32x16)
								0400 0012 6400 0800 (32x32)
								...

[hypreac2]
"warning"				100f60: 6106 3893 0068 00c8 (128x32)
						11c498: 00e0 00b2 6c00 0800
								0000 0000 0000 0000
text below "warning"	100f70: 6016 389b 0048 00b0 ((16 or 8)x8)
						11c4d8: 0054 0007 6000 0000

black regions			100af8: 6303 2d64 0000 03f8 (yoffs should be used)
						116b20: 0004 0000 0000 0fa0 <- move up and down
								0004 0000 0000 0fe0
								0004 0000 0000 0ce0
								0004 0000 0000 0d20
						1c0020:	6c00 00ef 057f 2600

"credits" 				1012f0: 6007 440e 00e0 0008 (16x8)
						122070: 0043 0006 6000 0000

tiles					100ce8:	6111 3464 0008 0030 (16x32,empty tile frames)
						11a320: 0460 006b 6110 0800
								0460 006b 6100 0800
								..
						100d68:	6205 34de 0008 0030 (16x32, tiles on frames)
						11a6f0: 050c 0059 6000 0800
								04c0 005a 6010 0800
								04d4 005a 6020 0800
								0500 0059 6030 0800
								..
						100cf8:	6105 348a 0008 fff8 (16x16 x 2x3, big tiles below)
						11a450: 61d6 0059 6000 0420
								61f2 0059 6000 0410
								6212 0059 6000 0400
								61d8 0059 6010 0420
								61f4 0059 6010 0410
								6214 0059 6010 0400

[keithlcy]
high scores				101030:	717f 40c0 0010 0000
						120600:	0000 0000 00c0 0025 (16x16)
								0000 0000 00d0 0025
								..
								0000 0000 0000 ffff (ysize should be 16!)
								0000 0000 0010 ffff
								..

K of "KEITH"			101180:	610e 4600 0020 0090
						123000:	4ef4 016a 0000 0040 (16x16)
								4ef6 016a 0010 0040
								..

floating robot (demo)	101000:	713d 4000 03b0 0088
						120000:	..
								71f6 0020 0000 0030
								71f8 0020 0010 0030
								..

cityscape				100030: 7304 1600 0000 007c (yoffs should not be used)
						10b000: 0001 0000 0200 0000
								0001 0000 0200 0040
								0001 0000 0200 0080
								0001 0000 0200 00c0
						1c0008:	0800 00f2 05ff 5729


[meosism]
shadows					100100: 701f 051b 0041 0020 (16x16 shadow)
						1028d8: 05aa 0030 f000 0470

[srmp4]
logo					100000: 6303 1680 0180 0078 (yoffs?)
						10b400: 0001 0000 0000 0060
								0001 0000 0000 0020
								0001 0000 0000 ffe0
								0001 0000 0000 ffa0
						1c0008:	0800 00ec 05ff 5629

tiles					100088: 6103 25b7 0028 000a (16x16)
						112db8: f4aa 0009 03f8 0018
								f4a8 0009 0008 0018
								f4ba 0009 03f8 0008
								f4bc 0009 0008 0008

[survarts]
tilemap					100030: 6303 2048 0000 0000
						110240: 0002 0000 0160 03fc
								0002 0000 0160 003c
								0002 0000 0160 007c
								0002 0000 0160 00bc
						1c0010:	0c2e 01f5 05ff 4628

player					100030: 611f 1da0 0000 fffc
						10ed00: eb16 200f 005c fffa (16x16)

"push start"			100130: 601d 1920 0048 0000
						10c900: 0441 0004 0000 004c (16x8)

[drifto94]
car shadow				100010: 8137 0640 0080 0030
						103200: ..
								544a 21e4 0030 0030 (16x16)

writings on finish		100130: 6109 4840 004e 0058 "good work"
						124200: ee6e 0135 0024 0000 (16x16)
						100158: 611e 4860 0058 0020 "you have proved yOur"..
						124300: ee92 0137 0024 0014 (16x16)
								..
								ee7e 0137 fff4 0000 (16x16!!)	; 'O'
								..

[ultrax]
sprite begin of lev1	100010:	6b60 4280 0016 00a0
						121400:	51a0 0042 6800 0c00 (64x64)

From the above some noteworthy cases are:

			101f60: 0006 0825 00b0 000c
			104128: 1a3a 0000 63d4 0400		consider y size & depth

			101030:	717f 40c0 0010 0000
			120600:	0000 0000 0000 ffff		ignore y size & depth

			100158: 611e 4860 0058 0020
			124300	ee7e 0137 fff4 0000		ignore x size & depth

			100f60: 6106 3893 0068 00c8
			11c498: 00e0 00b2 6c00 0800		consider x size & y size

			100100: 701f 051b 0041 0020
			1028d8: 05aa 0030 f000 0470		consider depth

			100010:	6b60 4280 0016 00a0
			121400:	51a0 0042 6800 0c00

			100140: 6003 04ca 0000 0000		tilemap
			102650: 0003 0000 0000 0c00

			100080: 000f 0410 0000 0000		tilemap
			102080: 0002 0000 6200 0c00

**************************************************************************/

/* Draw a tilemap sprite */

static void ssv_draw_row(struct mame_bitmap *bitmap, int sx, int sy, int scroll)
{
	struct rectangle clip;
	int attr, code, color, mode, size, page, shadow;
	int x, x1, sx1, flipx, xnum, xstart, xend, xinc;
	int y, y1, sy1, flipy, ynum, ystart, yend, yinc;
	data16_t *s3;

	xnum	=		0x20;		/* width in tiles (screen-wide) */
	ynum	=		0x8;		/* height in tiles (always 64 pixels?) */

	scroll	&=		0x7;		/* scroll register index */

	/* Sign extend the position */
	sx = 0;
	sy = (sy & 0x1ff) - (sy & 0x200);

	/* Set up a clipping region for the tilemap slice .. */

	clip.min_x = sx;
	clip.max_x = sx + xnum * 0x10 - 1;
	clip.min_y = sy;
	clip.max_y = sy + ynum * 0x8  - 1;

	/* .. and clip it against the visible screen */

	if (clip.min_x > Machine->visible_area.max_x)	return;
	if (clip.min_y > Machine->visible_area.max_y)	return;

	if (clip.max_x < Machine->visible_area.min_x)	return;
	if (clip.max_y < Machine->visible_area.min_y)	return;

	if (clip.min_x < Machine->visible_area.min_x)	clip.min_x = Machine->visible_area.min_x;
	if (clip.max_x > Machine->visible_area.max_x)	clip.max_x = Machine->visible_area.max_x;

	if (clip.min_y < Machine->visible_area.min_y)	clip.min_y = Machine->visible_area.min_y;
	if (clip.max_y > Machine->visible_area.max_y)	clip.max_y = Machine->visible_area.max_y;

	/* Get the scroll data */

	x		=	ssv_scroll[ scroll * 4 + 0 ];	/* x scroll */
	y		=	ssv_scroll[ scroll * 4 + 1 ];	/* y scroll */
/*				ssv_scroll[ scroll * 4 + 2 ];   ? 0, 05ff, 057f */
	mode	=	ssv_scroll[ scroll * 4 + 3 ];	/* shadow, depth etc.*/

	/* How is the background layer disabled ? */
	if ((mode & 0xe000) == 0)	return;

	shadow	=	(mode & 0x0800);

	/* Decide the actual size of the tilemap. $200 and $400 pixels
       are the only values actually used AFAIK */
	size	=	1 << (8 + ((mode & 0xe000) >> 13));
	page	=	(x & 0x7fff) / size;

	/* Given a fixed scroll value, the portion of tilemap displayed changes
       with the sprite postion */
	x	+=	sx;
	y	+=	sy;

	/* Tweak the scroll values (game specific) */
	x	+=	ssv_tilemap_offsx;
	y	+=	ssv_tilemap_offsy;

	/* Draw the rows */

	x1	=	x;
	y1	=	y;
	sx1	=	sx - (x & 0xf);
	sy1	=	sy - (y & 0xf);

	for (sx=sx1,x=x1; sx <= clip.max_x; sx+=0x10,x+=0x10)
	{
		for (sy=sy1,y=y1; sy <= clip.max_y; sy+=0x10,y+=0x10)
		{
			int tx, ty, gfx;

			s3	=	&spriteram16[	page * (size * ((0x1000/0x200)/2))	+
									((x & ((size -1) & ~0xf)) << 2)	+
									((y & ((0x200-1) & ~0xf)) >> 3)		];

			code	=	s3[0];	/* code high bits */
			attr	=	s3[1];	/* code low  bits + color */

			/* Code's high bits are scrambled */
			code	+=	ssv_tile_code[(attr & 0x3c00)>>10];
			flipy	=	(attr & 0x4000);
			flipx	=	(attr & 0x8000);
			
			if ((ssv_scroll[0x74/2] & 0x1000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
			{
				if (flipx == 0) flipx = 1; else flipx = 0;
			}
			if ((ssv_scroll[0x74/2] & 0x4000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
			{
				if (flipy == 0) flipy = 1; else flipy = 0;
			}

			color	=	attr;

			/* Select 256 or 64 color tiles */
			gfx	=	((mode & 0x0100) ? 0 : 1);

			/* Force 16x16 tiles ? */
			if (flipx)	{ xstart = 1-1;  xend = -1; xinc = -1; }
			else		{ xstart = 0;    xend = 1;  xinc = +1; }

			if (flipy)	{ ystart = 2-1;  yend = -1; yinc = -1; }
			else		{ ystart = 0;    yend = 2;  yinc = +1; }

			/* Draw a tile (16x16) */
			for (tx = xstart; tx != xend; tx += xinc)
			{
				for (ty = ystart; ty != yend; ty += yinc)
				{
					ssv_drawgfx( bitmap,	Machine->gfx[gfx],
											code++,
											color,
											flipx, flipy,
											sx + tx * 16, sy + ty * 8,
											&clip, shadow	);
				} /* ty */
			} /* tx */

		} /* sy */
	} /* sx */

}

/* Draw the "background layer" using multiple tilemap sprites */
static void ssv_draw_row2(struct mame_bitmap *bitmap, int sx, int sy, int scroll)
{
	struct rectangle clip;
	int attr, code, color, mode, size, page, shadow;
	int x, x1, sx1, flipx, xnum, xstart, xend, xinc;
	int y, y1, sy1, flipy, ynum, ystart, yend, yinc;
	data16_t *s3;

	xnum	=		0x20;		/* width in tiles (screen-wide) */
	ynum	=		0x8;		/* height in tiles (always 64 pixels?) */

	scroll	&=		0x7;		/* scroll register index */

	/* Sign extend the position */
	sx = 0;
	sy = (sy & 0x1ff) - (sy & 0x200);

	/* Set up a clipping region for the tilemap slice .. */

	clip.min_x = sx;
	clip.max_x = sx + xnum * 0x10 - 1;
	clip.min_y = sy;
	clip.max_y = sy + ynum * 0x8  - 1;

	/* .. and clip it against the visible screen */

	if (clip.min_x > Machine->visible_area.max_x)	return;
	if (clip.min_y > Machine->visible_area.max_y)	return;

	if (clip.max_x < Machine->visible_area.min_x)	return;
	if (clip.max_y < Machine->visible_area.min_y)	return;

	if (clip.min_x < Machine->visible_area.min_x)	clip.min_x = Machine->visible_area.min_x;
	if (clip.max_x > Machine->visible_area.max_x)	clip.max_x = Machine->visible_area.max_x;

	if (clip.min_y < Machine->visible_area.min_y)	clip.min_y = Machine->visible_area.min_y;
	if (clip.max_y > Machine->visible_area.max_y)	clip.max_y = Machine->visible_area.max_y;

	/* Get the scroll data */

	x		=	ssv_scroll[ scroll * 4 + 0 ];	/* x scroll */
	y		=	ssv_scroll[ scroll * 4 + 1 ];	/* y scroll */
/*				ssv_scroll[ scroll * 4 + 2 ];	? 0, 05ff, 057f */
	mode	=	ssv_scroll[ scroll * 4 + 3 ];	/* shadow, depth etc. */

	/* How is the background layer disabled ? */
	if ((mode & 0xe000) == 0)	return;

	shadow	=	(mode & 0x0800);

	/* Decide the actual size of the tilemap. $200 and $400 pixels
	   are the only values actually used AFAIK */
	size	=	1 << (8 + ((mode & 0xe000) >> 13));
	page	=	(x & 0x7fff) / size;

	/* Given a fixed scroll value, the portion of tilemap displayed changes
	   with the sprite postion */
	x	+=	sx;
	y	+=	sy;

	/* Tweak the scroll values (game specific) */
	x	+=	ssv_tilemap_offsx;
	y	+=	ssv_tilemap_offsy;

	/* Draw the rows */

	x1	=	x;
	y1	=	y;
	sx1	=	sx - (x & 0xf);
	sy1	=	sy - (y & 0xf);

	for (sx=sx1,x=x1; sx <= clip.max_x; sx+=0x10,x+=0x10)
	{
		for (sy=sy1,y=y1; sy <= clip.max_y; sy+=0x10,y+=0x10)
		{
			int tx, ty, gfx, transparency;

			s3	=	&spriteram16[	page * (size * ((0x1000/0x200)/2))	+
									((x & ((size -1) & ~0xf)) << 2)	+
									((y & ((0x200-1) & ~0xf)) >> 3)		];

			code	=	s3[0];	/* code high bits */
			attr	=	s3[1];	/* code low  bits + color */

			/* Code's high bits are scrambled */
			code	+=	ssv_tile_code[(attr & 0x3c00)>>10];
			flipy	=	(attr & 0x4000);
			flipx	=	(attr & 0x8000);
			if ((ssv_scroll[0x74/2] & 0x1000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
			{
				if (flipx == 0) flipx = 1; else flipx = 0;
			}
			if ((ssv_scroll[0x74/2] & 0x4000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
			{
				if (flipy == 0) flipy = 1; else flipy = 0;
			}
			color	=	attr;

			/* Select 256 or 64 color tiles */
			gfx	=	((mode & 0x0100) ? 0 : 1);

			/* Force 16x16 tiles ? */
			if (flipx)	{ xstart = 1-1;  xend = -1; xinc = -1; }
			else		{ xstart = 0;    xend = 1;  xinc = +1; }

			if (flipy)	{ ystart = 2-1;  yend = -1; yinc = -1; }
			else		{ ystart = 0;    yend = 2;  yinc = +1; }

			transparency = shadow ? TRANSPARENCY_ALPHA : TRANSPARENCY_PEN;

			/* Draw a tile (16x16) */
			for (tx = xstart; tx != xend; tx += xinc)
			{
				for (ty = ystart; ty != yend; ty += yinc)
				{
					drawgfx( bitmap,	Machine->gfx[gfx],
										code++,
										color,
										flipx, flipy,
										sx + tx * 16, sy + ty * 8,
										&clip,
										transparency ,0 );
				} /* ty */
			} /* tx */

		} /* sy */
	} /* sx */

}
static void ssv_draw_layer2(struct mame_bitmap *bitmap,int  nr)
{
	int sy;
	for ( sy = 0; sy <= Machine->visible_area.max_y; sy += 0x40 )
		ssv_draw_row2(bitmap, 0, sy, nr);
}
static void ssv_draw_layer(struct mame_bitmap *bitmap,int  nr)
{
	int sy;
	for ( sy = 0; sy <= Machine->visible_area.max_y; sy += 0x40 )
		ssv_draw_row(bitmap, 0, sy, nr);
}

/* Draw sprites in the sprites list */
static void ssv_draw_sprites2(struct mame_bitmap *bitmap)
{
	/* Sprites list */

	data16_t *s1	=	spriteram16;
	data16_t *end1	=	spriteram16 + 0x02000/2;
	data16_t *end2	=	spriteram16 + 0x40000/2;
	data16_t *s2;

	for ( ; s1 < end1; s1+=4 )
	{
		int attr, code, color, num, sprite;
		int sx, x, xoffs, flipx, xnum, xstart, xend, xinc, sprites_offsx;
		int sy, y, yoffs, flipy, ynum, ystart, yend, yinc, sprites_offsy, tilemaps_offsy;
		int mode,global_depth,global_xnum,global_ynum;

		mode	=		s1[ 0 ];
		sprite	=		s1[ 1 ];
		xoffs	=		s1[ 2 ];
		yoffs	=		s1[ 3 ];

		/* Last sprite */
		if (sprite & 0x8000) break;

		/* Single-sprite address */
		s2		=		&spriteram16[ (sprite & 0x7fff) * 4 ];
		tilemaps_offsy = ((s2[3] & 0x1ff) - (s2[3] & 0x200));

		/* Every single sprite is offset by x & yoffs, and additionally
		   by one of the 8 x & y offsets in the 1c0040-1c005f area   */

		xoffs	+=		ssv_scroll[((mode & 0x00e0) >> 4) + 0x40/2];
		yoffs	+=		ssv_scroll[((mode & 0x00e0) >> 4) + 0x42/2];

		/* Number of single-sprites (1-32) */
		num				=	(mode & 0x001f) + 1;
		global_ynum		=	(mode & 0x0300) << 2;
		global_xnum		=	(mode & 0x0c00);
		global_depth	=	(mode & 0xf000);

		for( ; num > 0; num--,s2+=4 )
		{
			int depth, local_depth, local_xnum, local_ynum;

			if (s2 >= end2)	break;

			sx		=		s2[ 2 ];
			sy		=		s2[ 3 ];

			local_depth		=	sx & 0xf000;
			local_xnum		=	sx & 0x0c00;
			local_ynum		=	sy & 0x0c00;

			depth = global_depth ? global_depth : local_depth;
			xnum = local_xnum;
			ynum = local_ynum;

			if (global_depth && global_depth != local_depth)
			{
				if (global_xnum || global_ynum)
				{
					xnum = global_xnum;
					ynum = global_ynum;
				}
				else
					depth = local_depth;
			}

			if ( s2[0] <= 7 && s2[1] == 0 && xnum == 0 && ynum == 0x0c00)
			{
				/* Tilemap Sprite */
				int scroll;

				scroll	=	s2[ 0 ];	/* scroll index */

				if (ssv_scroll[0x76/2] & 0x1000)
					sy -= 0x20;						/* eaglshot */
				else
				{
					if (ssv_scroll[0x7a/2] & 0x0800)
					{
						if (ssv_scroll[0x7a/2] & 0x1000)	/* drifto94, dynagear, keithlcy, mslider, stmblade, gdfs, ultrax, twineag2 */
							sy -= tilemaps_offsy;
						else						/* srmp4 */
							sy += tilemaps_offsy;
					}
				}

				if ((mode & 0x001f) != 0)
					ssv_draw_row2(bitmap,sx, sy, scroll);
			}

/* 	"normal" sprite
	hot spots:
	"warning" in hypreac2 has mode & 0x0100 and is not 16x16
	keithlcy high scores has mode & 0x0100 and y & 0x0c00 can be 0x0c00
	drifto94 "you have proved yOur".. has mode & 0x0100 and x & 0x0c00 can be 0x0c00
	ultrax (begin of lev1):	100010:	6b60 4280 0016 00a0
							121400:	51a0 0042 6800 0c00	needs to be a normal sprite
*/
			else
			{
				int shadow, gfx, transparency;
				if (s2 >= end2)	break;

				code	=	s2[0];	/* code high bits */
				attr	=	s2[1];	/* code low  bits + color */

				/* Code's high bits are scrambled */
				code	+=	ssv_tile_code[(attr & 0x3c00)>>10];
				flipy	=	(attr & 0x4000);
				flipx	=	(attr & 0x8000);
								
				color	=	attr;

				/* Select 256 or 64 color tiles */
				gfx		=	(depth & 0x1000) ? 0 : 1;
				shadow	=	(depth & 0x8000);

				/* Single-sprite tile size */
				xnum = 1 << (xnum >> 10);	/* 1, 2, 4 or 8 tiles */
				ynum = 1 << (ynum >> 10);	/* 1, 2, 4 tiles (8 means tilemap sprite?) */

				if (flipx)	{ xstart = xnum-1;  xend = -1;    xinc = -1; }
				else		{ xstart = 0;       xend = xnum;  xinc = +1; }

				if (flipy)	{ ystart = ynum-1;  yend = -1;    yinc = -1; }
				else		{ ystart = 0;       yend = ynum;  yinc = +1; }

				/* Apply global offsets */
				sx	+=	xoffs;
				sy	+=	yoffs;

				/* Sign extend the position */
				sx	=	(sx & 0x1ff) - (sx & 0x200);
				sy	=	(sy & 0x1ff) - (sy & 0x200);

               sprites_offsx =  ((ssv_scroll[0x74/2] & 0x7f) - (ssv_scroll[0x74/2] & 0x80));

				sprites_offsy = -((ssv_scroll[0x70/2] & 0x1ff) - (ssv_scroll[0x70/2] & 0x200) + ssv_scroll[0x6a/2] + 1);

				if (ssv_scroll[0x74/2] & 0x4000) /* flipscreen y */
				{
					sy = -sy;
					if (ssv_scroll[0x74/2] & 0x8000)
						sy += 0x00;			
					else
						sy -= 0x10;			/* vasara (hack) */
				}

				if (ssv_scroll[0x74/2] & 0x1000) /* flipscreen x */
				{
					sx = -sx + 0x100;
				}

				/* sprites can be relative to a side, the other side or the center */

				if (ssv_scroll[0x7a/2] == 0x7140)
				{
					/* srmp7 */
					sx	=	sprites_offsx + sx;
					sy	=	sprites_offsy - sy;
				}
				else if (ssv_scroll[0x7a/2] & 0x0800)
				{
					/* dynagear, drifto94, eaglshot, keithlcy, mslider, srmp4, stmblade, twineag2, ultrax */
					sx	=	sprites_offsx + sx - (xnum * 8)    ;
					sy	=	sprites_offsy - sy - (ynum * 8) / 2;
				}
				else
				{
					/* hypreact, hypreac2, janjans1, meosism, ryorioh, survarts, sxyreact, sxyreac2, vasara, vasara2 */
					sx	=	sprites_offsx + sx;
					sy	=	sprites_offsy - sy - (ynum * 8);
				}



				/* Sprite code masking */
				if (xnum == 2 && ynum == 4) /* needed by hypreact */
				{
					code &= ~7;
				}

				

				/* Draw the tiles */

				transparency = shadow ? TRANSPARENCY_ALPHA : TRANSPARENCY_PEN;

				for (x = xstart; x != xend; x += xinc)
				{
					for (y = ystart; y != yend; y += yinc)
					{
						drawgfx( bitmap,	Machine->gfx[gfx],
											code++,
											color,
											flipx, flipy,
											sx + x * 16, sy + y * 8,
											&Machine->visible_area,
											transparency, 0 );
					}
				}

				#ifdef MAME_DEBUG
				if (keyboard_pressed(KEYCODE_Z))	/* Display some info on each sprite */
				{	struct DisplayText dt[2];	char buf[10];
					sprintf(buf, "%02X",/*(s2[2] & ~0x3ff)>>8*/mode>>8);
					dt[0].text = buf;	dt[0].color = ((s1[0] & 0x0200) ? UI_COLOR_INVERSE : UI_COLOR_NORMAL);
					dt[0].x = sx;		dt[0].y = sy;
					dt[1].text = 0;	/* terminate array */
					displaytext(Machine->scrbitmap,dt);		}
				#endif

			}		/* sprite type */

		}	/* single-sprites */

	}	/* sprites list */
}
static void ssv_draw_sprites(struct mame_bitmap *bitmap)
{
	/* Sprites list */

	data16_t *s1	=	spriteram16;
	data16_t *end1	=	spriteram16 + 0x02000/2;
	data16_t *end2	=	spriteram16 + 0x40000/2;
	data16_t *s2;

	for ( ; s1 < end1; s1+=4 )
	{
		int attr, code, color, num, sprite;
		int sx, x, xoffs, flipx, xnum, xstart, xend, xinc, sprites_offsx;
		int sy, y, yoffs, flipy, ynum, ystart, yend, yinc, sprites_offsy, tilemaps_offsy;
		int mode,global_depth,global_xnum,global_ynum;

		mode	=		s1[ 0 ];
		sprite	=		s1[ 1 ];
		xoffs	=		s1[ 2 ];
		yoffs	=		s1[ 3 ];

		/* Last sprite */
		if (sprite & 0x8000) break;

		/* Single-sprite address */
		s2		=		&spriteram16[ (sprite & 0x7fff) * 4 ];
		tilemaps_offsy = ((s2[3] & 0x1ff) - (s2[3] & 0x200));


		/* Every single sprite is offset by x & yoffs, and additionally
           by one of the 8 x & y offsets in the 1c0040-1c005f area   */
		xoffs	+=		ssv_scroll[((mode & 0x00e0) >> 4) + 0x40/2];
		yoffs	+=		ssv_scroll[((mode & 0x00e0) >> 4) + 0x42/2];

		/* Number of single-sprites (1-32) */
		num				=	(mode & 0x001f) + 1;
		global_ynum		=	(mode & 0x0300) << 2;
		global_xnum		=	(mode & 0x0c00);
		global_depth	=	(mode & 0xf000);

		for( ; num > 0; num--,s2+=4 )
		{
			int depth, local_depth, local_xnum, local_ynum;

			if (s2 >= end2)	break;

			sx		=		s2[ 2 ];
			sy		=		s2[ 3 ];

			local_depth		=	sx & 0xf000;
			local_xnum		=	sx & 0x0c00;
			local_ynum		=	sy & 0x0c00;

			if (ssv_scroll[0x76/2] & 0x4000)
			{
				xnum	=	local_xnum;
				ynum	=	local_ynum;
				depth	=	local_depth;
			}
			else
			{
				xnum	=	global_xnum;
				ynum	=	global_ynum;
				depth	=	global_depth;
			}

			if ( s2[0] <= 7 && s2[1] == 0 && xnum == 0 && ynum == 0x0c00)
			{
				/* Tilemap Sprite */
				int scroll;

				scroll	=	s2[ 0 ];	/* scroll index */

				if (ssv_scroll[0x76/2] & 0x1000)
					sy -= 0x20;						/* eaglshot */
				else
				{
					if (ssv_scroll[0x7a/2] & 0x0800)
					{
						if (ssv_scroll[0x7a/2] & 0x1000)	/* drifto94, dynagear, keithlcy, mslider, stmblade, gdfs, ultrax, twineag2 */
							sy -= tilemaps_offsy;
						else						/* srmp4 */
							sy += tilemaps_offsy;
					}
				}

				if ((mode & 0x001f) != 0)
					ssv_draw_row(bitmap,sx, sy, scroll);
			}
			else
			{

/*  "normal" sprite
    hot spots:
    "warning" in hypreac2 has mode & 0x0100 and is not 16x16
    keithlcy high scores has mode & 0x0100 and y & 0x0c00 can be 0x0c00
    drifto94 "you have proved yOur".. has mode & 0x0100 and x & 0x0c00 can be 0x0c00
    ultrax (begin of lev1): 100010: 6b60 4280 0016 00a0
                            121400: 51a0 0042 6800 0c00 needs to be a normal sprite
*/

				int shadow, gfx;
				if (s2 >= end2)	break;

				code	=	s2[0];	/* code high bits */
				attr	=	s2[1];	/* code low  bits + color */

				/* Code's high bits are scrambled */
				code	+=	ssv_tile_code[(attr & 0x3c00)>>10];
				flipy	=	(attr & 0x4000);
				flipx	=	(attr & 0x8000);
				
				if ((ssv_scroll[0x74/2] & 0x1000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
				{
					if (flipx == 0) flipx = 1; else flipx = 0;
				}
				if ((ssv_scroll[0x74/2] & 0x4000) && ((ssv_scroll[0x74/2] & 0x2000) == 0))
				{
					if (flipy == 0) flipy = 1; else flipy = 0;
				}

				color	=	attr;

				/* Select 256 or 64 color tiles */
				gfx		=	(depth & 0x1000) ? 0 : 1;
				shadow	=	(depth & 0x8000);

				/* Single-sprite tile size */
				xnum = 1 << (xnum >> 10);	/* 1, 2, 4 or 8 tiles */
				ynum = 1 << (ynum >> 10);	/* 1, 2, 4 tiles (8 means tilemap sprite?) */

				if (flipx)	{ xstart = xnum-1;  xend = -1;    xinc = -1; }
				else		{ xstart = 0;       xend = xnum;  xinc = +1; }

				if (flipy)	{ ystart = ynum-1;  yend = -1;    yinc = -1; }
				else		{ ystart = 0;       yend = ynum;  yinc = +1; }

				/* Apply global offsets */
				sx	+=	xoffs;
				sy	+=	yoffs;

				/* Sign extend the position */
				sx	=	(sx & 0x1ff) - (sx & 0x200);
				sy	=	(sy & 0x1ff) - (sy & 0x200);

				sprites_offsx =  ((ssv_scroll[0x74/2] & 0x7f) - (ssv_scroll[0x74/2] & 0x80));

				sprites_offsy = -((ssv_scroll[0x70/2] & 0x1ff) - (ssv_scroll[0x70/2] & 0x200) + ssv_scroll[0x6a/2] + 1);

				if (ssv_scroll[0x74/2] & 0x4000) /* flipscreen y */
				{
					sy = -sy;
					if (ssv_scroll[0x74/2] & 0x8000)
						sy += 0x00;			
					else
						sy -= 0x10;			/* vasara (hack) */
				}

				if (ssv_scroll[0x74/2] & 0x1000) /* flipscreen x */
				{
					sx = -sx + 0x100;
				}

				/* sprites can be relative to a side, the other side or the center */

				if (ssv_scroll[0x7a/2] == 0x7140)
				{
					/* srmp7 */
					sx	=	sprites_offsx + sx;
					sy	=	sprites_offsy - sy;
				}
				else if (ssv_scroll[0x7a/2] & 0x0800)
				{
					/* dynagear, drifto94, eaglshot, keithlcy, mslider, srmp4, stmblade, twineag2, ultrax */
					sx	=	sprites_offsx + sx - (xnum * 8)    ;
					sy	=	sprites_offsy - sy - (ynum * 8) / 2;
				}
				else
				{
					/* hypreact, hypreac2, janjans1, meosism, ryorioh, survarts, sxyreact, sxyreac2, vasara, vasara2 */
					sx	=	sprites_offsx + sx;
					sy	=	sprites_offsy - sy - (ynum * 8);
				}



				/* Sprite code masking */
				if (xnum == 2 && ynum == 4) /* needed by hypreact */
				{
					code &= ~7;
				}


				/* Draw the tiles */

				for (x = xstart; x != xend; x += xinc)
				{
					for (y = ystart; y != yend; y += yinc)
					{
						ssv_drawgfx( bitmap,	Machine->gfx[gfx],
												code++,
												color,
												flipx, flipy,
												sx + x * 16, sy + y * 8,
												&Machine->visible_area, shadow	);
					}
				}

				#ifdef MAME_DEBUG
				if (keyboard_pressed(KEYCODE_Z))	/* Display some info on each sprite */
				{	struct DisplayText dt[2];	char buf[10];
					sprintf(buf, "%02X",/*(s2[2] & ~0x3ff)>>8*/mode>>8);
					dt[0].text = buf;	dt[0].color = ((s1[0] & 0x0200) ? UI_COLOR_INVERSE : UI_COLOR_NORMAL);
					dt[0].x = sx;		dt[0].y = sy;
					dt[1].text = 0;	/* terminate array */
					displaytext(Machine->scrbitmap,dt);		}
				#endif

			}		/* sprite type */

		}	/* single-sprites */

	}	/* sprites list */
}


/***************************************************************************


								Screen Drawing


***************************************************************************/

VIDEO_UPDATE( eaglshot )
{
	int tile;

	/* Decode tiles from ram */
	if (eaglshot_dirty)
	{
		eaglshot_dirty = 0;

		for (tile = 0; tile < (16 * 0x40000 / (16*8)); tile++)
		{
			if (eaglshot_dirty_tile[tile])
			{
				eaglshot_dirty_tile[tile] = 0;

				decodechar(Machine->gfx[0], tile, (UINT8 *)eaglshot_gfxram, Machine->drv->gfxdecodeinfo[0].gfxlayout);
				decodechar(Machine->gfx[1], tile, (UINT8 *)eaglshot_gfxram, Machine->drv->gfxdecodeinfo[1].gfxlayout);
			}
		}
	}

	video_update_ssv(bitmap, cliprect);
}

/*
    Sprites RAM is 0x80000 bytes long. The first 0x2000? bytes hold a list
    of sprites to display (the list can be made shorter using an end-of-list
    marker).

    Each entry in the list (16 bytes) is a multi-sprite (e.g it tells the
    hardware to display several single-sprites).

    The list looks like this:

    Offset:     Bits:                   Value:

        0.h     fedc ba-- ---- ----
                ---- --98 7654 3210     X displacement

        2.h     fedc ba-- ---- ----
                ---- --98 7654 3210     Y displacement

        4.h     f--- ---- ---- ----     List end
                -edc ba98 7654 3210     Offset of the single-sprite(s) data

        0.h                             Number of single-sprites (how many bits?)

    A single-sprite is:

    Offset:     Bits:                   Value:

        0.h                             Code

        2.h     f--- ---- ---- ----     Flip X
                -e-- ---- ---- ----     Flip Y
                ---- -a-- ---- ----     0 = 256 color steps, 1 = 64 color steps
                ---- --98 7654 3210     Color code

        4.h     fedc ba-- ---- ----
                ---- --98 7654 3210     X displacement

        6.h     fedc ba-- ---- ----
                ---- --98 7654 3210     Y displacement

        8.h     fedc ba98 ---- ----     Y Size
                ---- ---- 7654 3210     X Size

        A.h     fedc ba98 ---- ----
                ---- ---- 7654 ----     Priority
                ---- ---- ---- 32--     Y Tiles (1,2,4,8)
                ---- ---- ---- --10     X Tiles (1,2,4,8)

        C.h                             Unused

        E.h                             Unused

*/
static void gdfs_draw_zooming_sprites(struct mame_bitmap *bitmap, int priority)
{
	/* Sprites list */

	data16_t *s1	=	spriteram16_2;
	data16_t *end1	=	spriteram16_2 + 0x02000/2;
	data16_t *s2;

	priority <<= 4;

	for ( ; s1 < end1; s1+=8/2 )
	{
		int attr, code, color, num, sprite, zoom, size;
		int sx, x, xoffs, flipx, xnum, xstart, xend, xinc, xdim, xscale;
		int sy, y, yoffs, flipy, ynum, ystart, yend, yinc, ydim, yscale;

		xoffs	=		s1[ 0 ];
		yoffs	=		s1[ 1 ];
		sprite	=		s1[ 2 ];
		num		=		s1[ 3 ] % 0x101;

		/* Last sprite */
		if (sprite & 0x8000) break;

		/* Single-sprite address */
		s2		=		&spriteram16_2[ (sprite & 0x7fff) * 16/2 ];

		for( ; num > 0; num--,s2+=16/2 )
		{
			code	=	s2[ 0 ];
			attr	=	s2[ 1 ];
			sx		=	s2[ 2 ];
			sy		=	s2[ 3 ];
			zoom	=	s2[ 4 ];
			size	=	s2[ 5 ];

			if (priority != (size & 0xf0))
				break;

			flipx	=	(attr & 0x8000);
			flipy	=	(attr & 0x4000);
			color	=	(attr & 0x0400) ? attr : attr * 4;

			/* Single-sprite tile size */
			xnum = 1 << ((size >> 0) & 3);
			ynum = 1 << ((size >> 2) & 3);

			xnum = (xnum + 1) / 2;

			if (flipx)	{ xstart = xnum-1;  xend = -1;    xinc = -1; }
			else		{ xstart = 0;       xend = xnum;  xinc = +1; }

			if (flipy)	{ ystart = ynum-1;  yend = -1;    yinc = -1; }
			else		{ ystart = 0;       yend = ynum;  yinc = +1; }

			/* Apply global offsets */
			sx	+=	xoffs;
			sy	+=	yoffs;

			/* Sign extend the position */
			sx	=	(sx & 0x1ff) - (sx & 0x200);
			sy	=	(sy & 0x1ff) - (sy & 0x200);

			sy	=	-sy;

			/* Use fixed point values (16.16), for accuracy */
			sx <<= 16;
			sy <<= 16;

			xdim	=	( ( ((zoom >> 0) & 0xff) + 1) << 16 ) / xnum;
			ydim	=	( ( ((zoom >> 8) & 0xff) + 1) << 16 ) / ynum;

			xscale	=	xdim / 16;
			yscale	=	ydim / 8;

			/* Let's approximate to the nearest greater integer value
               to avoid holes in between tiles */
			if (xscale & 0xffff)	xscale += (1<<16) / 16;
			if (yscale & 0xffff)	yscale += (1<<16) / 8;

			/* Draw the tiles */

			for (x = xstart; x != xend; x += xinc)
			{
				for (y = ystart; y != yend; y += yinc)
				{
					drawgfxzoom(	bitmap,	Machine->gfx[2],
									code++,
									color,
									flipx, flipy,
									(sx + x * xdim) / 0x10000, (sy + y * ydim) / 0x10000,
									&Machine->visible_area, TRANSPARENCY_PEN, 0,
									xscale, yscale
					);
				}
			}

			#ifdef MAME_DEBUG
			if (code_pressed(KEYCODE_Z))	/* Display some info on each sprite */
			{	struct DisplayText dt[2];	char buf[10];
				sprintf(buf, "%X",size);
				dt[0].text = buf;	dt[0].color = UI_COLOR_INVERSE;
				dt[0].x = sx / 0x10000;		dt[0].y = sy / 0x10000;
				dt[1].text = 0;	/* terminate array */
				displaytext(Machine->scrbitmap,dt);		}
			#endif
		}	/* single-sprites */

	}	/* sprites list */
}

VIDEO_UPDATE( gdfs )
{
	int tile, pri;

	video_update_ssv(bitmap, cliprect);

	/* Decode zooming sprites tiles from ram */
	if (eaglshot_dirty)
	{
		eaglshot_dirty = 0;

		for (tile = 0; tile < (4 * 0x100000 / (16*8)); tile++)
		{
			if (eaglshot_dirty_tile[tile])
			{
				eaglshot_dirty_tile[tile] = 0;

				decodechar(Machine->gfx[2], tile, (UINT8 *)eaglshot_gfxram, Machine->drv->gfxdecodeinfo[2].gfxlayout);
			}
		}
	}

	for (pri = 0; pri <= 0xf; pri++)
		gdfs_draw_zooming_sprites(bitmap, pri);

	tilemap_set_scrollx(gdfs_tmap,0,gdfs_tmapscroll[0x0c/2]);
	tilemap_set_scrolly(gdfs_tmap,0,gdfs_tmapscroll[0x10/2]);
	tilemap_draw(bitmap,cliprect, gdfs_tmap, 0, 0);

#if 0
	draw_crosshair(1, bitmap,
		Machine->visible_area.min_x + ((Machine->visible_area.max_x - Machine->visible_area.min_x) * readinputport(5)) / 255,
		Machine->visible_area.min_y + ((Machine->visible_area.max_y - Machine->visible_area.min_y) * readinputport(6)) / 255,
		cliprect,0);

	draw_crosshair(2, bitmap,
		Machine->visible_area.min_x + ((Machine->visible_area.max_x - Machine->visible_area.min_x) * readinputport(7)) / 255,
		Machine->visible_area.min_y + ((Machine->visible_area.max_y - Machine->visible_area.min_y) * readinputport(8)) / 255,
		cliprect,1);
#endif
}


static int enable_video;

void ssv_enable_video(int enable)
{
	enable_video = enable;
}

VIDEO_UPDATE( ssv )
{
	if (ssv_special !=3)
	{
	if (ssv_scroll[0x76/2] & 0x0080)
	{
		/* 4 bit shadows (mslider, stmblade) */
    shadow_pen_shift = 15-4;
	}
	else
	{
		/* 2 bit shadows */
    shadow_pen_shift = 15-2;
	}
	}
  
  shadow_pen_mask = (1 << shadow_pen_shift) - 1;

	/* The background color is the first one in the palette */
	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	if (!enable_video)	return;

	if (ssv_special !=3)
	{
		ssv_draw_layer(bitmap,0);		/* "background layer" */
		ssv_draw_sprites(bitmap);	/* sprites list */
	} /* dynagears is weird, whats really going on? */
	else
	{
		ssv_draw_layer2(bitmap,0);
		ssv_draw_layer2(bitmap,1);
		ssv_draw_sprites2(bitmap);
		ssv_draw_layer2(bitmap,3);
	}
}
