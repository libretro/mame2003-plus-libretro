/* Super Slam - Video Hardware */

#include "driver.h"

static struct tilemap *sslam_bg_tilemap, *sslam_tx_tilemap, *sslam_md_tilemap;

extern data16_t *sslam_bg_tileram, *sslam_tx_tileram, *sslam_md_tileram;
extern data16_t *sslam_spriteram, *sslam_regs;

static void sslam_drawsprites( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	/* there are still some sprite problems .. */

	const struct GfxElement *gfx = Machine->gfx[3];
	data16_t *source = sslam_spriteram;
	data16_t *finish = source + 0x20000/2;

	source += 3; /* strange*/

	while( source<finish )
	{
		int xpos, ypos, number, flipx, colr, eightbyeight;

		xpos = source[2] & 0x1ff;
		colr = (source[2] & 0xf000) >> 12;

		xpos = source[2] & 0x01ff;
		ypos = source[0] & 0x01ff;

		xpos -=16; xpos -=7;
		ypos = 0xff-ypos;
		ypos -=16; ypos -=7;

		number = source[3];
		flipx = (source[0]&0x4000);
		eightbyeight = (source[0]&0x1000);

		if (source[0]&0x2000) break;


		if (!eightbyeight)
		{
			if (flipx)
			{
				drawgfx(bitmap,gfx,number,colr,1,0,xpos+8,ypos,cliprect,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,gfx,number+1,colr,1,0,xpos+8,ypos+8,cliprect,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,gfx,number+2,colr,1,0,xpos,ypos,cliprect,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,gfx,number+3,colr,1,0,xpos,ypos+8,cliprect,TRANSPARENCY_PEN,0);
			}
			else
			{
				drawgfx(bitmap,gfx,number,colr,0,0,xpos,ypos,cliprect,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,gfx,number+1,colr,0,0,xpos,ypos+8,cliprect,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,gfx,number+2,colr,0,0,xpos+8,ypos,cliprect,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,gfx,number+3,colr,0,0,xpos+8,ypos+8,cliprect,TRANSPARENCY_PEN,0);
			}
		}
		else
		{
			if (flipx)
			{
				drawgfx(bitmap,gfx,number+2,colr>>12,1,0,xpos,ypos,cliprect,TRANSPARENCY_PEN,0);
			}
			else
			{
				drawgfx(bitmap,gfx,number,colr>>12,0,0,xpos,ypos,cliprect,TRANSPARENCY_PEN,0);
			}
		}



		source+=4;
	}

}


/* Text Layer */

static void get_sslam_tx_tile_info(int tile_index)
{

	int code = sslam_tx_tileram[tile_index] & 0xfff;
	int colr = sslam_tx_tileram[tile_index] & 0xe000;

	SET_TILE_INFO(2,code+0xc000 ,colr >> 13,0)
}

WRITE16_HANDLER( sslam_tx_tileram_w )
{
	COMBINE_DATA(&sslam_tx_tileram[offset]);
	tilemap_mark_tile_dirty(sslam_tx_tilemap,offset);
}

/* Middle Layer */

static void get_sslam_md_tile_info(int tile_index)
{
	int code = sslam_md_tileram[tile_index] & 0x0fff;
	int colr = sslam_md_tileram[tile_index] & 0xe000;

	SET_TILE_INFO(1,code+0x2000 ,colr >> 13,0)
}

WRITE16_HANDLER( sslam_md_tileram_w )
{
	COMBINE_DATA(&sslam_md_tileram[offset]);
	tilemap_mark_tile_dirty(sslam_md_tilemap,offset);
}

static void get_sslam_bg_tile_info(int tile_index)
{
	int code = sslam_bg_tileram[tile_index] & 0x1fff;
	int colr = sslam_bg_tileram[tile_index] & 0xe000;

	SET_TILE_INFO(0,code ,colr >> 13,0)
}

WRITE16_HANDLER( sslam_bg_tileram_w )
{
	COMBINE_DATA(&sslam_bg_tileram[offset]);
	tilemap_mark_tile_dirty(sslam_bg_tilemap,offset);
}

VIDEO_START(sslam)
{
	sslam_tx_tilemap = tilemap_create(get_sslam_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	if (!sslam_tx_tilemap) return 1;
	tilemap_set_transparent_pen(sslam_tx_tilemap,0);

	sslam_md_tilemap = tilemap_create(get_sslam_md_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	if (!sslam_md_tilemap) return 1;
	tilemap_set_transparent_pen(sslam_md_tilemap,0);

	sslam_bg_tilemap = tilemap_create(get_sslam_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,16,16,32,32);
	if (!sslam_bg_tilemap) return 1;

	return 0;
}


VIDEO_UPDATE(sslam)
{
	tilemap_set_scrollx(sslam_tx_tilemap,0, sslam_regs[0]);
	tilemap_set_scrolly(sslam_tx_tilemap,0, sslam_regs[1]+8);
	tilemap_set_scrollx(sslam_md_tilemap,0, sslam_regs[2]+2);
	tilemap_set_scrolly(sslam_md_tilemap,0, sslam_regs[3]+8);
	tilemap_set_scrollx(sslam_bg_tilemap,0, sslam_regs[4]+4);
	tilemap_set_scrolly(sslam_bg_tilemap,0, sslam_regs[5]+8);

	tilemap_draw(bitmap,cliprect,sslam_bg_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,sslam_md_tilemap,0,0);

	sslam_drawsprites(bitmap,cliprect);

	tilemap_draw(bitmap,cliprect,sslam_tx_tilemap,0,0);

/*
	usrintf_showmessage	("Regs %04x %04x %04x %04x %04x %04x %04x %04x",
		sslam_regs[0], // x scroll tx?
		sslam_regs[1], // y scroll tx?
		sslam_regs[2], // x scroll mid
		sslam_regs[3], // y scroll mid
		sslam_regs[4], // x scroll bk?
		sslam_regs[5], // y scroll bk?
		sslam_regs[6],
		sslam_regs[7]
	);
*/
}
