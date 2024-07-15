/***************************************************************************

	Sega X-board hardware

***************************************************************************/

#include "driver.h"
#include "segaic16.h"



/*************************************
 *
 *	Statics
 *
 *************************************/

static UINT8 road_priority;



/*************************************
 *
 *	Video startup
 *
 *************************************/

VIDEO_START( xboard )
{
	/* compute palette info */
	segaic16_palette_init(0x2000);

	/* initialize the tile/text layers */
	if (segaic16_tilemap_init(0, SEGAIC16_TILEMAP_16B, 0x1c00, 0, 2))
		return 1;

	/* initialize the sprites */
	if (segaic16_sprites_init(0, SEGAIC16_SPRITES_XBOARD, 0x000, 0))
		return 1;
		
	/* initialize the road */
	if (segaic16_road_init(0, SEGAIC16_ROAD_OUTRUN, 0x1700, 0x1720, 0x1780, -166))
		return 1;

	return 0;
}



/*************************************
 *
 *	Miscellaneous setters
 *
 *************************************/

void xboard_set_road_priority(int priority)
{
	/* this is only set at init time */
	road_priority = priority;
}



/*************************************
 *
 *	Video update
 *
 *************************************/

VIDEO_UPDATE( xboard )
{
	/* if no drawing is happening, fill with black and get out */
	if (!segaic16_display_enable)
	{
		fillbitmap(bitmap, get_black_pen(), cliprect);
		return;
	}

	/* reset priorities */
	fillbitmap(priority_bitmap, 0, cliprect);

	/* draw the low priority road layer */
	segaic16_road_draw(0, bitmap, cliprect, SEGAIC16_ROAD_BACKGROUND);
	if (road_priority == 0)
		segaic16_road_draw(0, bitmap, cliprect, SEGAIC16_ROAD_FOREGROUND);
	
	/* draw background */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_BACKGROUND, 0, 0x01);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_BACKGROUND, 1, 0x02);

	/* draw foreground */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_FOREGROUND, 0, 0x02);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_FOREGROUND, 1, 0x04);

	/* draw the high priority road */
	if (road_priority == 1)
		segaic16_road_draw(0, bitmap, cliprect, SEGAIC16_ROAD_FOREGROUND);

	/* text layer */
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_TEXT, 0, 0x04);
	segaic16_tilemap_draw(0, bitmap, cliprect, SEGAIC16_TILEMAP_TEXT, 1, 0x08);

	/* draw the sprites */
	segaic16_sprites_draw(0, bitmap, cliprect);
}
