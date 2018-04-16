/***************************************************************************

   Tumblepop Video emulation - Bryan McPhail, mish@tendril.co.uk

*********************************************************************

Uses Data East custom chip 55 for backgrounds, custom chip 52 for sprites.

See Dark Seal & Caveman Ninja drivers for info on these chips.

Tumblepop is one of few games to take advantage of the playfields ability
to switch between 8*8 tiles and 16*16 tiles.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static data16_t tumblep_control_0[8];
data16_t *tumblep_pf1_data,*tumblep_pf2_data;
static struct tilemap *pf1_tilemap,*pf1_alt_tilemap,*pf2_tilemap;
static int flipscreen;

/******************************************************************************/

static void tumblep_drawsprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	int offs;

	for (offs = 0;offs < 0x400;offs += 4)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		sprite = spriteram16[offs+1] & 0x3fff;
		if (!sprite) continue;

		y = spriteram16[offs];
		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		x = spriteram16[offs+2];
		colour = (x >>9) & 0xf;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
        x = 304 - x;

		sprite &= ~multi;
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (flipscreen)
		{
			y=240-y;
			x=304-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else mult=-16;

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[3],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					cliprect,TRANSPARENCY_PEN,0);

			multi--;
		}
	}
}

static void jumpkids_drawsprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	int offs;

	for (offs = 0;offs < spriteram_size/2;offs += 4)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		sprite = spriteram16[offs+1] & 0x3fff;
		if (!sprite) continue;

		y = spriteram16[offs];
		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		x = spriteram16[offs+2];
		colour = (x >>9) & 0xf;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
        x = 304 - x;

	//	sprite &= ~multi; /* Todo:  I bet TumblePop bootleg doesn't do this either */
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (flipscreen)
		{
			y=240-y;
			x=304-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else mult=-16;

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[3],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					cliprect,TRANSPARENCY_PEN,0);

			multi--;
		}
	}
}

static void fncywld_drawsprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	int offs;

	for (offs = 0;offs < 0x400;offs += 4)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		sprite = spriteram16[offs+1] & 0x3fff;
		if (!sprite) continue;

		y = spriteram16[offs];
		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		x = spriteram16[offs+2];
		colour = (x >>9) & 0x3f;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
        x = 304 - x;

	//	sprite &= ~multi; /* Todo:  I bet TumblePop bootleg doesn't do this either */
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (flipscreen)
		{
			y=240-y;
			x=304-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else mult=-16;

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[3],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					cliprect,TRANSPARENCY_PEN,15);

			multi--;
		}
	}
}

/******************************************************************************/

WRITE16_HANDLER( tumblep_pf1_data_w )
{
	data16_t oldword=tumblep_pf1_data[offset];
	COMBINE_DATA(&tumblep_pf1_data[offset]);
	if (oldword!=tumblep_pf1_data[offset]) {
		tilemap_mark_tile_dirty(pf1_tilemap,offset);
		tilemap_mark_tile_dirty(pf1_alt_tilemap,offset);

	}
}

WRITE16_HANDLER( tumblep_pf2_data_w )
{
	data16_t oldword=tumblep_pf2_data[offset];
	COMBINE_DATA(&tumblep_pf2_data[offset]);
	if (oldword!=tumblep_pf2_data[offset])
		tilemap_mark_tile_dirty(pf2_tilemap,offset);
}

WRITE16_HANDLER( fncywld_pf1_data_w )
{
	data16_t oldword=tumblep_pf1_data[offset];
	COMBINE_DATA(&tumblep_pf1_data[offset]);
	if (oldword!=tumblep_pf1_data[offset]) {
		tilemap_mark_tile_dirty(pf1_tilemap,offset/2);
		tilemap_mark_tile_dirty(pf1_alt_tilemap,offset/2);

	}
}

WRITE16_HANDLER( fncywld_pf2_data_w )
{
	data16_t oldword=tumblep_pf2_data[offset];
	COMBINE_DATA(&tumblep_pf2_data[offset]);
	if (oldword!=tumblep_pf2_data[offset])
		tilemap_mark_tile_dirty(pf2_tilemap,offset/2);
}

WRITE16_HANDLER( tumblep_control_0_w )
{
	COMBINE_DATA(&tumblep_control_0[offset]);
}

/******************************************************************************/

static UINT32 tumblep_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5);
}

static INLINE void get_bg_tile_info(int tile_index,int gfx_bank,data16_t *gfx_base)
{
	int data = gfx_base[tile_index];

	SET_TILE_INFO(
			gfx_bank,
			data & 0x0fff,
			data >> 12,
			0)
}

static void get_bg1_tile_info(int tile_index) { get_bg_tile_info(tile_index,2,tumblep_pf1_data); }
static void get_bg2_tile_info(int tile_index) { get_bg_tile_info(tile_index,1,tumblep_pf2_data); }

static void get_fg_tile_info(int tile_index)
{
	int data = tumblep_pf1_data[tile_index];

	SET_TILE_INFO(
			0,
			data & 0x0fff,
			data >> 12,
			0)
}

static INLINE void get_fncywld_bg_tile_info(int tile_index,int gfx_bank,data16_t *gfx_base)
{
	int data = gfx_base[tile_index*2];
	int attr = gfx_base[tile_index*2+1];

	SET_TILE_INFO(
			gfx_bank,
			data & 0x1fff,
			attr & 0x1f,
			0)
}

static void get_fncywld_bg1_tile_info(int tile_index) { get_fncywld_bg_tile_info(tile_index,2,tumblep_pf1_data); }
static void get_fncywld_bg2_tile_info(int tile_index) { get_fncywld_bg_tile_info(tile_index,1,tumblep_pf2_data); }

static void get_fncywld_fg_tile_info(int tile_index)
{
	int data = tumblep_pf1_data[tile_index*2];
	int attr = tumblep_pf1_data[tile_index*2+1];

	SET_TILE_INFO(
			0,
			data & 0x1fff,
			attr & 0x1f,
			0)
}




VIDEO_START( tumblep )
{
	pf1_tilemap =     tilemap_create(get_fg_tile_info, tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,32);
	pf1_alt_tilemap = tilemap_create(get_bg1_tile_info,tumblep_scan,TILEMAP_TRANSPARENT,16,16,64,32);
	pf2_tilemap =     tilemap_create(get_bg2_tile_info,tumblep_scan,TILEMAP_OPAQUE,     16,16,64,32);

	if (!pf1_tilemap || !pf1_alt_tilemap || !pf2_tilemap)
		return 1;

	tilemap_set_transparent_pen(pf1_tilemap,0);
	tilemap_set_transparent_pen(pf1_alt_tilemap,0);

	return 0;
}

VIDEO_START( fncywld )
{
	pf1_tilemap =     tilemap_create(get_fncywld_fg_tile_info, tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,32);
	pf1_alt_tilemap = tilemap_create(get_fncywld_bg1_tile_info,tumblep_scan,TILEMAP_TRANSPARENT,16,16,64,32);
	pf2_tilemap =     tilemap_create(get_fncywld_bg2_tile_info,tumblep_scan,TILEMAP_OPAQUE,     16,16,64,32);

	if (!pf1_tilemap || !pf1_alt_tilemap || !pf2_tilemap)
		return 1;

	tilemap_set_transparent_pen(pf1_tilemap,15);
	tilemap_set_transparent_pen(pf1_alt_tilemap,15);

	return 0;
}

/******************************************************************************/

VIDEO_UPDATE( tumblep )
{
	int offs;

	flipscreen=tumblep_control_0[0]&0x80;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	if (flipscreen) offs=1; else offs=-1;

	tilemap_set_scrollx( pf1_tilemap,0, tumblep_control_0[1]+offs );
	tilemap_set_scrolly( pf1_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf1_alt_tilemap,0, tumblep_control_0[1]+offs );
	tilemap_set_scrolly( pf1_alt_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf2_tilemap,0, tumblep_control_0[3]+offs );
	tilemap_set_scrolly( pf2_tilemap,0, tumblep_control_0[4] );

	tilemap_draw(bitmap,cliprect,pf2_tilemap,0,0);
	if (tumblep_control_0[6]&0x80)
		tilemap_draw(bitmap,cliprect,pf1_tilemap,0,0);
	else
		tilemap_draw(bitmap,cliprect,pf1_alt_tilemap,0,0);
	tumblep_drawsprites(bitmap,cliprect);
}

VIDEO_UPDATE( tumblepb )
{
	int offs,offs2;

	flipscreen=tumblep_control_0[0]&0x80;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	if (flipscreen) offs=1; else offs=-1;
	if (flipscreen) offs2=-3; else offs2=-5;

	tilemap_set_scrollx( pf1_tilemap,0, tumblep_control_0[1]+offs2 );
	tilemap_set_scrolly( pf1_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf1_alt_tilemap,0, tumblep_control_0[1]+offs2 );
	tilemap_set_scrolly( pf1_alt_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf2_tilemap,0, tumblep_control_0[3]+offs );
	tilemap_set_scrolly( pf2_tilemap,0, tumblep_control_0[4] );

	tilemap_draw(bitmap,cliprect,pf2_tilemap,0,0);
	if (tumblep_control_0[6]&0x80)
		tilemap_draw(bitmap,cliprect,pf1_tilemap,0,0);
	else
		tilemap_draw(bitmap,cliprect,pf1_alt_tilemap,0,0);
	tumblep_drawsprites(bitmap,cliprect);
}

VIDEO_UPDATE( jumpkids )
{
	int offs,offs2;

	flipscreen=tumblep_control_0[0]&0x80;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	if (flipscreen) offs=1; else offs=-1;
	if (flipscreen) offs2=-3; else offs2=-5;

	tilemap_set_scrollx( pf1_tilemap,0, tumblep_control_0[1]+offs2 );
	tilemap_set_scrolly( pf1_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf1_alt_tilemap,0, tumblep_control_0[1]+offs2 );
	tilemap_set_scrolly( pf1_alt_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf2_tilemap,0, tumblep_control_0[3]+offs );
	tilemap_set_scrolly( pf2_tilemap,0, tumblep_control_0[4] );

	tilemap_draw(bitmap,cliprect,pf2_tilemap,0,0);
	if (tumblep_control_0[6]&0x80)
		tilemap_draw(bitmap,cliprect,pf1_tilemap,0,0);
	else
		tilemap_draw(bitmap,cliprect,pf1_alt_tilemap,0,0);
	jumpkids_drawsprites(bitmap,cliprect);
}

VIDEO_UPDATE( fncywld )
{
	int offs,offs2;

	flipscreen=tumblep_control_0[0]&0x80;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	if (flipscreen) offs=1; else offs=-1;
	if (flipscreen) offs2=-3; else offs2=-5;

	tilemap_set_scrollx( pf1_tilemap,0, tumblep_control_0[1]+offs2 );
	tilemap_set_scrolly( pf1_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf1_alt_tilemap,0, tumblep_control_0[1]+offs2 );
	tilemap_set_scrolly( pf1_alt_tilemap,0, tumblep_control_0[2] );
	tilemap_set_scrollx( pf2_tilemap,0, tumblep_control_0[3]+offs );
	tilemap_set_scrolly( pf2_tilemap,0, tumblep_control_0[4] );

	tilemap_draw(bitmap,cliprect,pf2_tilemap,0,0);
	if (tumblep_control_0[6]&0x80)
		tilemap_draw(bitmap,cliprect,pf1_tilemap,0,0);
	else
		tilemap_draw(bitmap,cliprect,pf1_alt_tilemap,0,0);
	fncywld_drawsprites(bitmap,cliprect);
}
