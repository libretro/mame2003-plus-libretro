/***************************************************************************

	Atari Batman hardware

****************************************************************************/

#include "driver.h"
#include "machine/atarigen.h"
#include "batman.h"
#include "thunderj.h"



/*************************************
 *
 *	Globals
 *
 *************************************/

UINT8 batman_alpha_tile_bank;
INT32 mm2_startup;



/*************************************
 *
 *	Tilemap callbacks
 *
 *************************************/

static void get_alpha_tile_info(int tile_index)
{
	UINT16 data = atarigen_alpha[tile_index];
	int code = ((data & 0x400) ? (batman_alpha_tile_bank * 0x400) : 0) + (data & 0x3ff);
	int color = (data >> 11) & 0x0f;
	int opaque = data & 0x8000;
	SET_TILE_INFO(2, code, color, opaque ? TILE_IGNORE_TRANSPARENCY : 0);
}


static void get_playfield_tile_info(int tile_index)
{
	UINT16 data1 = atarigen_playfield[tile_index];
	UINT16 data2 = atarigen_playfield_upper[tile_index] & 0xff;
	int code = data1 & 0x7fff;
	int color = 0x10 + (data2 & 0x0f);
	SET_TILE_INFO(0, code, color, (data1 >> 15) & 1);
	tile_info.priority = (data2 >> 4) & 3;
}

static void get_playfield_mm2_tile_info(int tile_index)
{
	UINT16 data1 = atarigen_playfield[tile_index];

	int code = data1 & 0x3fff;
	SET_TILE_INFO(0, code, 0, (data1 >> 15) & 1);
}

static void get_playfield2_tile_info(int tile_index)
{
	UINT16 data1 = atarigen_playfield2[tile_index];
	UINT16 data2 = atarigen_playfield_upper[tile_index] >> 8;
	int code = data1 & 0x7fff;
	int color = data2 & 0x0f;
	SET_TILE_INFO(0, code, color, (data1 >> 15) & 1);
	tile_info.priority = (data2 >> 4) & 3;
}


/*************************************
 *
 *	Video system start
 *
 *************************************/

VIDEO_START( batman )
{
	static const struct atarimo_desc modesc =
	{
		1,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		1,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		8,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x100,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0x03ff,0,0,0 }},	/* mask for the link */
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
		{{ 0,0,0x0070,0 }},	/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		NULL				/* callback routine for special entries */
	};

	/* initialize the playfield */
	atarigen_playfield_tilemap = tilemap_create(get_playfield_tile_info, tilemap_scan_cols, TILEMAP_OPAQUE, 8,8, 64,64);
	if (!atarigen_playfield_tilemap)
		return 1;

	/* initialize the second playfield */
	atarigen_playfield2_tilemap = tilemap_create(get_playfield2_tile_info, tilemap_scan_cols, TILEMAP_TRANSPARENT, 8,8, 64,64);
	if (!atarigen_playfield2_tilemap)
		return 1;
	tilemap_set_transparent_pen(atarigen_playfield2_tilemap, 0);

	/* initialize the motion objects */
	if (!atarimo_init(0, &modesc))
		return 1;

	/* initialize the alphanumerics */
	atarigen_alpha_tilemap = tilemap_create(get_alpha_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8,8, 64,32);
	if (!atarigen_alpha_tilemap)
		return 1;
	tilemap_set_transparent_pen(atarigen_alpha_tilemap, 0);

	return 0;
}

VIDEO_START( mm2 )
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

		{{ 0x3ff,0,0,0 }},	/* mask for the link */
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
		{{ 0,0,0x0070,0 }},	/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		0				/* callback routine for special entries */
	};

	/* initialize the playfield */
	atarigen_playfield_tilemap = tilemap_create(get_playfield_mm2_tile_info, tilemap_scan_cols, TILEMAP_OPAQUE, 8,8, 64,64);
	if (!atarigen_playfield_tilemap)
		return 1;

	/* initialize the second playfield */
	atarigen_playfield2_tilemap = tilemap_create(get_playfield_mm2_tile_info, tilemap_scan_cols, TILEMAP_TRANSPARENT, 8,8, 64,64);
	if (!atarigen_playfield2_tilemap)
    	return 1;
	tilemap_set_transparent_pen(atarigen_playfield_tilemap, 0);

	/* Align playfields */
	tilemap_set_scrolldx(atarigen_playfield_tilemap, -4, 0);

	/* initialize the motion objects */
	if (!atarimo_init(0, &modesc))
		return 1;

	return 0;
}

/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void batman_scanline_update(int scanline)
{
	/* update the scanline parameters */
	if (scanline <= Machine->visible_area.max_y && atarivc_state.rowscroll_enable)
	{
		data16_t *base = &atarigen_alpha[scanline / 8 * 64 + 48];
		int scan, i;

		for (scan = 0; scan < 8; scan++, scanline++)
			for (i = 0; i < 2; i++)
			{
				int data = *base++;
				switch (data & 15)
				{
					case 9:
						force_partial_update(scanline - 1);
						atarivc_state.mo_xscroll = (data >> 7) & 0x1ff;
						atarimo_set_xscroll(0, atarivc_state.mo_xscroll);
						break;

					case 10:
						force_partial_update(scanline - 1);
						atarivc_state.pf1_xscroll_raw = (data >> 7) & 0x1ff;
						atarivc_update_pf_xscrolls();
						tilemap_set_scrollx(atarigen_playfield_tilemap, 0, atarivc_state.pf0_xscroll);
						tilemap_set_scrollx(atarigen_playfield2_tilemap, 0, atarivc_state.pf1_xscroll);
						break;

					case 11:
						force_partial_update(scanline - 1);
						atarivc_state.pf0_xscroll_raw = (data >> 7) & 0x1ff;
						atarivc_update_pf_xscrolls();
						tilemap_set_scrollx(atarigen_playfield_tilemap, 0, atarivc_state.pf0_xscroll);
						break;

					case 13:
						force_partial_update(scanline - 1);
						atarivc_state.mo_yscroll = (data >> 7) & 0x1ff;
						atarimo_set_yscroll(0, atarivc_state.mo_yscroll);
						break;

					case 14:
						force_partial_update(scanline - 1);
						atarivc_state.pf1_yscroll = (data >> 7) & 0x1ff;
						tilemap_set_scrolly(atarigen_playfield2_tilemap, 0, atarivc_state.pf1_yscroll);
						break;

					case 15:
						force_partial_update(scanline - 1);
						atarivc_state.pf0_yscroll = (data >> 7) & 0x1ff;
						tilemap_set_scrolly(atarigen_playfield_tilemap, 0, atarivc_state.pf0_yscroll);
						break;
				}
			}
	}
}


/*************************************
 *
 *	Main refresh
 *
 *************************************/

VIDEO_UPDATE( batman )
{
	struct atarimo_rect_list rectlist;
	struct mame_bitmap *mobitmap;
	int x, y, r;

	/* draw the playfield */
	fillbitmap(priority_bitmap, 0, cliprect);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 0, 0x00);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 1, 0x01);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 2, 0x02);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 3, 0x03);
	tilemap_draw(bitmap, cliprect, atarigen_playfield2_tilemap, 0, 0x80);
	tilemap_draw(bitmap, cliprect, atarigen_playfield2_tilemap, 1, 0x84);
	tilemap_draw(bitmap, cliprect, atarigen_playfield2_tilemap, 2, 0x88);
	tilemap_draw(bitmap, cliprect, atarigen_playfield2_tilemap, 3, 0x8c);

	/* draw and merge the MO */
	mobitmap = atarimo_render(0, cliprect, &rectlist);
	for (r = 0; r < rectlist.numrects; r++, rectlist.rect++)
		for (y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
		{
			UINT16 *mo = (UINT16 *)mobitmap->base + mobitmap->rowpixels * y;
			UINT16 *pf = (UINT16 *)bitmap->base + bitmap->rowpixels * y;
			UINT8 *pri = (UINT8 *)priority_bitmap->base + priority_bitmap->rowpixels * y;
			for (x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
				if (mo[x])
				{
					/* verified on real hardware:

						for all MO colors, MO priority 0:
							obscured by low fg playfield pens priority 1-3
							obscured by high fg playfield pens priority 3 only
							obscured by bg playfield priority 3 only

						for all MO colors, MO priority 1:
							obscured by low fg playfield pens priority 2-3
							obscured by high fg playfield pens priority 3 only
							obscured by bg playfield priority 3 only

						for all MO colors, MO priority 2-3:
							obscured by low fg playfield pens priority 3 only
							obscured by high fg playfield pens priority 3 only
							obscured by bg playfield priority 3 only
					*/
					int mopriority = mo[x] >> ATARIMO_PRIORITY_SHIFT;

					/* upper bit of MO priority signals special rendering and doesn't draw anything */
					if (mopriority & 4)
						continue;

					/* foreground playfield case */
					if (pri[x] & 0x80)
					{
						int pfpriority = (pri[x] >> 2) & 3;

						/* playfield priority 3 always wins */
						if (pfpriority == 3)
							;

						/* priority is consistent for upper pens in playfield */
						else if (pf[x] & 0x08)
							pf[x] = mo[x] & ATARIMO_DATA_MASK;

						/* otherwise, we need to compare */
						else if (mopriority >= pfpriority)
							pf[x] = mo[x] & ATARIMO_DATA_MASK;
					}

					/* background playfield case */
					else
					{
						int pfpriority = pri[x] & 3;

						/* playfield priority 3 always wins */
						if (pfpriority == 3)
							;

						/* otherwise, MOs get shown */
						else
							pf[x] = mo[x] & ATARIMO_DATA_MASK;
					}

					/* don't erase yet -- we need to make another pass later */
				}
		}

	/* add the alpha on top */
	tilemap_draw(bitmap, cliprect, atarigen_alpha_tilemap, 0, 0);

	/* now go back and process the upper bit of MO priority */
	rectlist.rect -= rectlist.numrects;
	for (r = 0; r < rectlist.numrects; r++, rectlist.rect++)
		for (y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
		{
			UINT16 *mo = (UINT16 *)mobitmap->base + mobitmap->rowpixels * y;
			UINT16 *pf = (UINT16 *)bitmap->base + bitmap->rowpixels * y;
			for (x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
				if (mo[x])
				{
					int mopriority = mo[x] >> ATARIMO_PRIORITY_SHIFT;

					/* upper bit of MO priority might mean palette kludges */
					if (mopriority & 4)
					{
						/* if bit 2 is set, start setting high palette bits */
						if (mo[x] & 2)
							thunderj_mark_high_palette(bitmap, pf, mo, x, y);
					}

					/* erase behind ourselves */
					mo[x] = 0;
				}
		}
}

VIDEO_UPDATE( mm2 )
{
	struct atarimo_rect_list rectlist;
	struct mame_bitmap *mobitmap;
	int x, y, r;

	/* draw the playfield */
	fillbitmap(priority_bitmap, 0, cliprect);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 0, 0x00);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 1, 0x01);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 2, 0x02);
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 3, 0x03);

    for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
            UINT16 *src = (UINT16 *)bitmap->base + bitmap->rowpixels * y;
			UINT8 *dst = (UINT8 *)priority_bitmap->base + priority_bitmap->rowpixels * y;
		/* top bit of the gfxdata appears to be priority, so we don't want it in the render bitmap */
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			dst[x] = src[x];
			src[x] &= 0x007f;
		}
	}

	/* draw and merge the MO */
	mobitmap = atarimo_render(0, cliprect, &rectlist);

	if (mm2_startup == 0)
	{
		mm2_startup = 1;
		for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		{
			UINT16 *mo = (UINT16 *)mobitmap->base + mobitmap->rowpixels * y;

			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
			{
				mo[x] = 0xffff;
			}
		}
	}

	for (r = 0; r < rectlist.numrects; r++, rectlist.rect++)
		for (y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
		{
			UINT16 *mo = (UINT16 *)mobitmap->base + mobitmap->rowpixels * y;
			UINT16 *pf2 = (UINT16 *)bitmap->base + bitmap->rowpixels * y;
			UINT8 *pri = (UINT8 *)priority_bitmap->base + priority_bitmap->rowpixels * y;
			for (x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
			{
				if (mo[x] != 0xffff)
				{
					if (pri[x] & 0x80) /* check against top bit of temp pf render */
					{
						if (mo[x] & 0x80)
							pf2[x] = mo[x];
					}
					else
					{
						pf2[x] = mo[x] | 0x80;
					}
					mo[x] = 0xffff;
				}
			}
		}

}
