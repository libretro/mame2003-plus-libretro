/***********************************************************************************************

	Sega System C/C2 Driver
	-----------------------
	Version 0.54 - 02 Feb 2003
	(for changes see drivers\segac2.c)

	The video of the System C/C2, Mega-Tech and Mega Play boards and of the
	Mega Drive based bootlegs: the VDP itself is in vidhrdw/segavdp.c.

***********************************************************************************************/

#include "driver.h"
#include "segac2.h"

/* in vidhrdw/segasyse.c */
int start_megatech_video_normal(void);
void update_megatech_video_normal(struct mame_bitmap *bitmap, const struct rectangle *cliprect );
void update_megaplay_video_normal(struct mame_bitmap *bitmap, const struct rectangle *cliprect );



/******************************************************************************
	Video Start Functions
*******************************************************************************

	The palette RAM is shared by the VDP (CRAM writes) and the board.

******************************************************************************/

static int start_vdp(int sp_palbase, int display_enable, int scanbase)
{
	struct segavdp_interface intf;

	/* reset the palettes */
	memset(paletteram16, 0, 0x800 * sizeof(data16_t));

	intf.cram_mode        = SEGAVDP_CRAM_PALETTERAM;
	intf.cram_base        = 0;
	intf.bg_palbase       = 0x000;
	intf.sp_palbase       = sp_palbase;
	intf.scanbase         = scanbase;
	intf.display_enable   = display_enable;
	intf.set_visible_area = 1;
	intf.irq_update       = segac2_vdp_irq_update;
	return segavdp_start(&intf);
}


VIDEO_START( segac2 )
{
	/* the display is enabled by the board (control_w) */
	return start_vdp(0x100, 0, 0);
}


VIDEO_START( puckpkmn )
{
	paletteram16 = auto_malloc(0x800 * sizeof(data16_t));

	/* same palettes for sprites and bg */
	return start_vdp(0x000, 1, 0);
}


VIDEO_START( megatech )
{
	paletteram16 = auto_malloc(0x800 * sizeof(data16_t));

	/* same palettes for sprites and bg */
	if (start_vdp(0x000, 1, 256*2))
		return 1;

	return start_megatech_video_normal();
}


VIDEO_START( megaplay )
{
	paletteram16 = auto_malloc(0x800 * sizeof(data16_t));

	/* same palettes for sprites and bg */
	if (start_vdp(0x000, 1, 0))
		return 1;

	return start_megatech_video_normal();
}



/* end-of-frame callback to mark the start of VBLANK */
VIDEO_EOF( segac2 )
{
	segavdp_eof();
}



/******************************************************************************
	Screen Refresh Functions
******************************************************************************/

/* core refresh: computes the final screen */
VIDEO_UPDATE( segac2 )
{
	int y;

	/* generate the final screen */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		segavdp_draw_line((UINT16 *)bitmap->line[y], y, 0);
}


/* megatech, same but drawing the sms display too */
VIDEO_UPDATE( megatech )
{
	int y;

	/* generate the final screen */
	for (y = cliprect->min_y+192; y <= cliprect->max_y; y++)
		segavdp_draw_line((UINT16 *)bitmap->line[y], y-192, 0);

	/* sms display should be on second monitor, for now we control it with a fake dipswitch while
	   the driver is in development (if (readinputport(5)&0x01)) */
	update_megatech_video_normal(bitmap, cliprect);
}


/* megaplay, draws either Genesis or SMS (single screen display) */
VIDEO_UPDATE( megaplay )
{
	int y;

	/* generate the final screen */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		segavdp_draw_line((UINT16 *)bitmap->line[y], y, 0);

	update_megaplay_video_normal(bitmap, cliprect);
}
