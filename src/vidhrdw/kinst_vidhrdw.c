/***************************************************************************

	Killer Instinct hardware

	driver by Aaron Giles and Bryan McPhail

***************************************************************************/

#include "driver.h"
#include "kinst.h"


static data32_t *video_base;

/* hardware is xBBBBBGGGGGRRRRR; the direct RGB bitmap is xRRRRRGGGGGBBBBB */
#define KINST_RGB(v) ((((v) & 0x001f) << 10) | ((v) & 0x03e0) | (((v) >> 10) & 0x001f))



/*************************************
 *
 *	Video RAM base selection
 *
 *************************************/

void kinst_set_video_base(data32_t *base)
{
	/* the frame buffer is read straight from RAM, as in current MAME */
	video_base = base;
}



/*************************************
 *
 *	Palette setup
 *
 *************************************/

PALETTE_INIT( kinst )
{
	int i;

	/* standard 5-5-5 RGB palette */
	for (i = 0; i < 32768; i++)
	{
		int r = i & 31;
		int g = (i >> 5) & 31;
		int b = (i >> 10) & 31;
		palette_set_color(i, (r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
	}
}



/*************************************
 *
 *	Video system start
 *
 *************************************/

VIDEO_START( kinst )
{
	return 0;
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

VIDEO_UPDATE( kinst )
{
	int x, y;

	/* loop over rows and copy to the destination */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const data32_t *src = &video_base[640/4 * y + cliprect->min_x / 2];
		UINT16 *dest = (UINT16 *)bitmap->line[y] + (cliprect->min_x & ~1);

		/* two pixels per 32-bit word */
		for (x = cliprect->min_x & ~1; x <= cliprect->max_x; x += 2)
		{
			data32_t pix = *src++;
			*dest++ = KINST_RGB(pix);
			*dest++ = KINST_RGB(pix >> 16);
		}
	}
}
