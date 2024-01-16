/***************************************************************************

	Atari Shuuz hardware

****************************************************************************/

#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"
#include "shuuz.h"



/*************************************
 *
 *	Tilemap callbacks
 *
 *************************************/

static void get_playfield_tile_info(int tile_index)
{
	UINT16 data1 = atarigen_playfield[tile_index];
	UINT16 data2 = atarigen_playfield_upper[tile_index] >> 8;
	int code = data1 & 0x3fff;
	int color = data2 & 0x0f;
	SET_TILE_INFO(0, code, color, (data1 >> 15) & 1);
}



/*************************************
 *
 *	Video system start
 *
 *************************************/

VIDEO_START( shuuz )
{
	static const struct atarimo_desc modesc =
	{
		1,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		0,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		8,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x000,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0x00ff,0,0,0 }},	/* mask for the link */
		{{ 0 }},			/* mask for the graphics bank */
		{{ 0,0x7fff,0,0 }},	/* mask for the code index */
		{{ 0 }},			/* mask for the upper code index */
		{{ 0,0,0x000f,0 }},	/* mask for the color */
		{{ 0,0,0xff80,0 }},	/* mask for the X position */
		{{ 0,0,0,0xff80 }},	/* mask for the Y position */
		{{ 0,0,0,0x0070 }},	/* mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	/* mask for the height, in tiles */
		{{ 0,0x8000,0,0 }},	/* mask for the horizontal flip */
		{{ 0 }},			/* mask for the vertical flip */
		{{ 0 }},			/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		0					/* callback routine for special entries */
	};

	/* initialize the playfield */
	atarigen_playfield_tilemap = tilemap_create(get_playfield_tile_info, tilemap_scan_cols, TILEMAP_OPAQUE, 8,8, 64,64);
	if (!atarigen_playfield_tilemap)
		return 1;

	/* initialize the motion objects */
	if (!atarimo_init(0, &modesc))
		return 1;
	return 0;
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

VIDEO_UPDATE( shuuz )
{
	struct atarimo_rect_list rectlist;
	struct mame_bitmap *mobitmap;
	int x, y, r;

	/* draw the playfield */
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 0, 0);

	/* draw and merge the MO */
	mobitmap = atarimo_render(0, cliprect, &rectlist);
	for (r = 0; r < rectlist.numrects; r++, rectlist.rect++)
		for (y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
		{
			UINT16 *mo = (UINT16 *)mobitmap->base + mobitmap->rowpixels * y;
			UINT16 *pf = (UINT16 *)bitmap->base + bitmap->rowpixels * y;
			for (x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
			{	
				if (mo[x])
				{
					/* verified from the GALs on the real PCB; equations follow
					 *
					 *		--- O13 is 1 if (PFS7-4 == 0xf)
					 *		O13=PFS6*PFS7*(PFS5&PFS4)
					 *
					 *		--- PF/M is 1 if MOs have priority, or 0 if playfield has priority
					 *		MO/PF=!PFS7*!(LBD7&LBD6)*!M1*!O13
					 *		   +!PFS7*!(LBD7&LBD6)*!M2*!O13
					 *		   +!PFS7*!(LBD7&LBD6)*!M3*!O13
					 *		   +PFS7*(LBD7&LBD6)*!M1*!O13
					 *		   +PFS7*(LBD7&LBD6)*!M2*!O13
					 *		   +PFS7*(LBD7&LBD6)*!M3*!O13
					 *
					 */

				    /* This is based on observations, and not verified against schematics and GAL equations.
					 * TODO:
					 * Locate schematics for (or trace out) video mixing section.
					 * Obtain equations for video mixing GALs.
					 */
					int o13 = (pf[x] & 0xf0) == 0xf0;
					int mopf = ((pf[x] & 0x80) ? ((mo[x] & 0xc0) == 0xc0) : ((mo[x] & 0xc0) != 0xc0)) && !o13;

					/* if MO/PF is asserted, we draw the MO */
					if (mopf)
                    {
						if (mo[x] & 0x0e)       /* solid colors */
							pf[x] = mo[x];
						else if (mo[x] & 0x01)  /* shadows */
						    pf[x] |= 0x200;
				    }

					/* erase behind ourselves */
					mo[x] = 0;
				}
				
		    }	
		}
}
