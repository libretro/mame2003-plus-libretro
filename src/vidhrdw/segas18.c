#include "driver.h"
#include "vidhrdw/generic.h"
#include "system16.h"
#include "vidhrdw/segaic16.h"
#include "state.h"
#include "vidhrdw/segavdp.h"

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

			/* as in MAME: the VDP shadow/highlight flags (0x800/0x1000) are not
			   used, only the 64 VDP colors at 0x1800; they would also index
			   past the end of the palette */
			if (pix != 0xffff && (pix & 0xf))
			{
				dst[x] = 0x1800 + (pix & 0x3f);
				pri[x] |= priority;
			}
		}
	}
}

/*************************************
 *
 *	The VDP of System 18: its 64 colors
 *	are set directly after the System 16
 *	palette, the screen is not resized
 *
 *************************************/

int system18_vdp_start(void)
{
	struct segavdp_interface intf;

	intf.cram_mode        = SEGAVDP_CRAM_DIRECT;
	intf.cram_base        = 0x1800;
	intf.bg_palbase       = 0x1800;
	intf.sp_palbase       = 0x1800;
	intf.scanbase         = 0;
	intf.line_scale       = 1;
	intf.display_enable   = 1;
	intf.set_visible_area = 0;
	intf.irq_update       = NULL;
	return segavdp_start(&intf);
}

/* render the VDP layer, transparent where the VDP draws nothing */
void system18_vdp_update(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int y;

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		segavdp_draw_line((UINT16 *)bitmap->line[y], y, 0xffff);
}

/* the VBLANK flag of the VDP */
VIDEO_EOF( system18 )
{
	segavdp_eof();
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
	if (system18_vdp_start())
		return 1;
	/* create a temp bitmap to draw the VDP data into */
	tempbitmap = auto_bitmap_alloc_depth(Machine->drv->screen_width, Machine->drv->screen_height, 16);
	if (!tempbitmap)
		return 1;
	fillbitmap(tempbitmap, 0xffff, NULL);

	grayscale_enable = vdp_enable = vdp_mixing = 0;
	state_save_register_UINT8("system18", 0, "grayscale", &grayscale_enable, 1);
	state_save_register_UINT8("system18", 0, "vdp_enable", &vdp_enable, 1);
	state_save_register_UINT8("system18", 0, "vdp_mixing", &vdp_mixing, 1);
	state_save_register_UINT16("system18", 0, "vdp_bitmap", (UINT16 *)tempbitmap->base, tempbitmap->rowpixels * (tempbitmap->height - 1) + tempbitmap->width);
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
		if (vdp_enable)
			system18_vdp_update(tempbitmap, cliprect);
		return;
	}


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

	/* the VDP layer shows one frame later than the other layers, as in MAME
	   (it renders its lines ahead of the screen): render it for the next
	   frame. It is part of the save states. */
	if (vdp_enable)
		system18_vdp_update(tempbitmap, cliprect);
}
