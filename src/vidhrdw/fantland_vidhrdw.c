/***************************************************************************

					  -= Fantasy Land / Galaxy Gunners =-

					driver by	Luca Elia (l.elia@tin.it)

	This game has sprites only:

	tiles are 16 x 16 x 6. There are $400 sprites, each one is allotted
	8 bytes of memory (but only 5 are used) :

	Offset: 	Bits:			Value:

		0						X (low bits)

		1		7--- ----		X (high bit)
				-6-- ----		Y (high bit)
				--5- ----		Flip X
				---4 ----		Flip Y
				---- 32--
				---- --10		Color

		2						Code (high bits)

		3						Code (low bits)

		4						Y (low bits)

	Then follows a table with 1 byte per sprite: the index of a x,y
	and code offset	that sprite will use, from a table with 256 entries:

		0						Y offset (low bits)

		1		7654 321-		Code offset
				---- ---0		Y offset (high bit)

		2						X offset (low bits)

		3						X offset (high bit)

***************************************************************************/

#include "vidhrdw/generic.h"

static void fantland_draw_sprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	data8_t	*ram	=	spriteram,		/* spriteram start*/
			*end	=	ram + 0x2000,	/* spriteram end*/
			*ram2	=	ram + 0x2000;	/* table of indexes into the table of offsets*/

	for ( ; ram < end; ram += 8,ram2 ++)
	{
		int attr,code,color, x,y,xoffs,yoffs,flipx,flipy, idx;

		attr	=	ram[1];

		x		=	ram[0];
		code	=	ram[3] + (ram[2] << 8);
		y		=	ram[4];

		color	=	(attr & 0x03);
		flipy	=	(attr & 0x10);
		flipx	=	(attr & 0x20);

		y		+=	(attr & 0x40) << 2;
		x		+=	(attr & 0x80) << 1;

		/* Index in the table of offsets */
		idx		=	ram2[0] * 4;

		/* Fetch the offsets */
		yoffs	=	spriteram_2[idx + 0] + (spriteram_2[idx + 1] << 8);
		xoffs	=	spriteram_2[idx + 2] + (spriteram_2[idx + 3] << 8);

		y		+=	yoffs;
		x		+=	xoffs;
		code	+=	yoffs >> 9;

		y		=	(y & 0xff) - (y & 0x100);
		x		=	(x & 0x1ff);

		if (x >= 0x180)		x -= 0x200;

		drawgfx(bitmap,Machine->gfx[0], code,color, flipx,flipy, x,y, &Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

VIDEO_UPDATE( fantland )
{
	fillbitmap(bitmap,Machine->pens[0],cliprect);
	fantland_draw_sprites(bitmap,cliprect);
}
