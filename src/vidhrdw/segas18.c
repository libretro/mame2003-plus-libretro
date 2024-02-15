#include "driver.h"
#include "vidhrdw/generic.h"
#include "system16.h"
#include "vidhrdw/segaic16.h"

static struct mame_bitmap *tempbitmap;

static UINT8 grayscale_enable;
static UINT8 vdp_enable;
static UINT8 vdp_mixing;

/*************************************
 *
 *	VDP drawing
 *
 *************************************/

static void draw_vdp(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority)
{
	int x, y;

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *src = (UINT16 *)tempbitmap->line[y];
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		UINT8 *pri = (UINT8 *)priority_bitmap->line[y];

		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			UINT16 pix = src[x];
			if (pix != 0xffff)
			{
				dst[x] = pix;
				pri[x] |= priority;
			}
		}
	}
}

void system18_set_grayscale(int enable)
{
	enable = (enable != 0);
	if (enable != grayscale_enable)
	{
		force_partial_update(cpu_getscanline());
		grayscale_enable = enable;
//		printf("Grayscale = %02X\n", enable);
	}
}

void system18_set_vdp_enable(int enable)
{
	enable = (enable != 0);
	if (enable != vdp_enable)
	{
		force_partial_update(cpu_getscanline());
		vdp_enable = enable;
#if DEBUG_VDP
		printf("VDP enable = %02X\n", enable);
#endif
	}
}

void system18_set_vdp_mixing(int mixing)
{
	if (mixing != vdp_mixing)
	{
		force_partial_update(cpu_getscanline());
		vdp_mixing = mixing;
#if DEBUG_VDP
		printf("VDP mixing = %02X\n", mixing);
#endif
	}
}

VIDEO_START( system18_new )
{
	/* compute palette info */
	segaic16_palette_init(0x800);

	/* initialize the tile/text layers */
	if (segaic16_tilemap_init(0, SEGAIC16_TILEMAP_16B, 0x000, 0, 8))
		return 1;

	/* initialize the sprites */
	if (segaic16_sprites_init(0, SEGAIC16_SPRITES_16B, 0x400, 0))
		return 1;

	/* create the VDP */
	start_system18_vdp();
	/* create a temp bitmap to draw the VDP data into */
	tempbitmap = auto_bitmap_alloc_depth(Machine->drv->screen_width, Machine->drv->screen_height, 16);
	if (!tempbitmap)
		return 1;
	return 0;
}

VIDEO_UPDATE( system18_new )
{
	int vdppri, vdplayer;

	vdplayer = (vdp_mixing >> 1) & 3;
	vdppri = (vdp_mixing & 1) ? (1 << vdplayer) : 0;

	if (!segaic16_display_enable)
	{
		fillbitmap(bitmap, get_black_pen(), cliprect);
		return;
	}

	/* if the VDP is enabled, update our tempbitmap */
	if (vdp_enable)
		update_system18_vdp(tempbitmap, cliprect);

	/* reset priorities */
	fillbitmap(priority_bitmap, 0, cliprect);

	/* draw background opaquely first, not setting any priorities */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_BACKGROUND, 0 | TILEMAP_IGNORE_TRANSPARENCY, 0x00);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_BACKGROUND, 1 | TILEMAP_IGNORE_TRANSPARENCY, 0x00);
	if (vdp_enable && vdplayer == 0) draw_vdp(bitmap, cliprect, vdppri);

	/* draw background again to draw non-transparent pixels over the VDP and set the priority */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_BACKGROUND, 0, 0x01);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_BACKGROUND, 1, 0x02);
	if (vdp_enable && vdplayer == 1) draw_vdp(bitmap, cliprect, vdppri);

	/* draw foreground */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_FOREGROUND, 0, 0x02);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_FOREGROUND, 1, 0x04);
	if (vdp_enable && vdplayer == 2) draw_vdp(bitmap, cliprect, vdppri);

	/* text layer */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_TEXT, 0, 0x04);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_TEXT, 1, 0x08);
	if (vdp_enable && vdplayer == 3) draw_vdp(bitmap, cliprect, vdppri);
	/* draw the sprites */
	segaic16_sprites_draw(0, bitmap, cliprect);
}
