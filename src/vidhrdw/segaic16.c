/***************************************************************************

	Sega 16-bit common hardware

****************************************************************************

  Sega system16 and friends hardware

               CPU      Tiles      Sprites   Priority  Color     SCPU  Sound                Other
System C       68000    315-5313                       315-5242  z80   ym3438               315-5296(IO)
Space Harrier  68000x2                                 (c)       z80   ym2203 pcm(b)
System 16B     68000    315-5197   315-5196  GAL       (c)       z80   ym2151 upd7759       315-5195
After Burner   68000x2  315-5197   315-5211A GAL       315-5242  z80   ym2151 315-5218      315-5250(a) 315-5248(x2) 315-5249(x2) 315-5275(road)
System 18      68000    315-536x   315-536x            315-5242  z80   ym3834(x2) RF5c68(d) 315-3296(IO) 315-5313(vdp)
System 24      68000x2  315-5292   315-5293  315-5294  315-5242        ym2151 dac           315-5195(x3) 315-5296(IO)
Galaxy Force   68000x3             315-5296+ 315-5312  315-5242  z80   ym2151 315-5218      315-5296(IO)
System 32      V60      315-5386A  315-5387  315-5388  315-5242  z80   ym3834(x2) RF5c68(d) 315-5296(IO)

a) 315-5250: 68000 glue and address decoding

b) 8x8-bit voices entirely in TTL.  The 315-5218 is believed to be the
   integrated version of that

c) Resistor network and latches believed to be equivalent to the 315-5242

d) Also seen as 315-5476A and ASSP 5c105 and ASSP 5c68a

Quick review of the system16 hardware:

  Hang-on hardware:
    The first one.  Two tilemap planes, one sprite plane, one road
    plane.  The shadow capability doesn't seem to be used, the
    highlight/shadow switch in the 5242-equivalent is global for all
    colors.

  Space harrier hardware:
    Similar to hang-on, with per-color highlight/shadow selection, and
    the shadows are used.

  System16a / Pre-system16:
    Space harrier without the road generator.

  System16b:
    4-layer tilemap hardware in two pairs, with selection between each
    members on the pairs on a 8-lines basis.  Slightly better sprites.

  System18
    System 16b plus a genesis vdp.

  Outrun:
    System 16b with sprites with better zooming capabilities, plus a
    road generator able to handle two roads simultaneously.

  X-Board:
    Outrun with a better fillrate and an even more flexible road
    generator.

  Y-Board:
    New design, with two sprite planes and no tilemaps.  The back
    sprite planes has a huge fillrate and the capability to have its
    frame buffer completely rotated.  Also, it has a palette
    indirection capability to allows for easier palettes rotations.
    The front sprite plane is 16b.

  System24:
    The odd one out.  Medium resolution. Entirely ram-based, no
    graphics roms.  4-layer tilemap hardware in two pairs, selection
    on a 8-pixels basis.  Tile-based sprites(!) organised as a linked
    list.  The tilemap chip has been reused for model1 and model2,
    probably because they had it handy and it handles medium res.

  System32:
    5-layer tilemap hardware consisting of 4 independant rom-based
    layers with linescroll, lineselection, linezoom and window
    clipping capability and one simpler ram-based text plane.  Mixed
    ram/rom sprite engine with palette indirection, per-color priority
    (thankfully not actually used).  The sprite list includes jumping
    and clipping capabilities, and advanced hot-spot positioning.  The
    mixer chip adds totally dynamic priorities, alpha-blending of the
    tilemaps, per-component color control, and some other funnies we
    have not been able to decypher.

  ST-V (also know as Titan or the Saturn console):
    The ultimate 2D system.  Even more advanced tilemaps, with 6-dof
    roz support, alpha up to the wazoo and other niceties, known as
    the vdp2.  Ths sprite engine, vdp1, allows for any 4-point
    streching of the sprites, actually giving polygonal 3D
    capabilities.  Interestingly, the mixer capabilities took a hit,
    with no real per-sprite mixer priority, which could be considered
    annoying for a 2D system.  It still allowed some beauties like
    Radiant Silvergun.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/res_net.h"



/*************************************
 *
 *	Globals
 *
 *************************************/

data16_t *segaic16_tileram;
data16_t *segaic16_textram;
data16_t *segaic16_spriteram;
data16_t *segaic16_roadram;

struct tilemap *segaic16_tilemaps[16];
UINT8 segaic16_tilemap_page;



/*************************************
 *
 *	Statics
 *
 *************************************/

static int palette_entries;
static UINT8 normal_pal[32];
static UINT8 shadow_pal[32];
static UINT8 hilight_pal[32];



/*************************************
 *
 *	Palette computation
 *
 *************************************/

/*
	Color generation details

	Each color is made up of 5 bits, connected through one or more resistors like so:

	Bit 0 = 1 x 3.9K ohm
	Bit 1 = 1 x 2.0K ohm
	Bit 2 = 1 x 1.0K ohm
	Bit 3 = 2 x 1.0K ohm
	Bit 4 = 4 x 1.0K ohm

	Another data bit is connected by a tristate buffer to the color output through a
	470 ohm resistor. The buffer allows the resistor to have no effect (tristate),
	halve brightness (pull-down) or double brightness (pull-up). The data bit source
	is bit 15 of each color RAM entry.
*/

void segaic16_init_palette(int entries)
{
	static const int resistances_normal[6] = { 3900, 2000, 1000, 1000/2, 1000/4, 0   };
	static const int resistances_sh[6]     = { 3900, 2000, 1000, 1000/2, 1000/4, 470 };
	double weights[2][6];
	int i;
	
	/* compute the number of palette entries */
	palette_entries = entries;

	/* compute weight table for regular palette entries */
	compute_resistor_weights(0, 255, -1.0,
		6, resistances_normal, weights[0], 0, 0,
		0, NULL, NULL, 0, 0,
		0, NULL, NULL, 0, 0);

	/* compute weight table for shadow/hilight palette entries */
	compute_resistor_weights(0, 255, -1.0,
		6, resistances_sh, weights[1], 0, 0,
		0, NULL, NULL, 0, 0,
		0, NULL, NULL, 0, 0);

	/* compute R, G, B for each weight */
	for (i = 0; i < 32; i++)
	{
		int i4 = (i >> 4) & 1;
		int i3 = (i >> 3) & 1;
		int i2 = (i >> 2) & 1;
		int i1 = (i >> 1) & 1;
		int i0 = (i >> 0) & 1;

		normal_pal[i] = combine_6_weights(weights[0], i0, i1, i2, i3, i4, 0);
		shadow_pal[i] = combine_6_weights(weights[1], i0, i1, i2, i3, i4, 0);
		hilight_pal[i] = combine_6_weights(weights[1], i0, i1, i2, i3, i4, 1);
	}
}



/*************************************
 *
 *	Palette accessors
 *
 *************************************/

WRITE16_HANDLER( segaic16_paletteram_w )
{
	data16_t newval;
	int r, g, b;

	/* get the new value */
	newval = paletteram16[offset];
	COMBINE_DATA(&newval);
	paletteram16[offset] = newval;

	/*	   byte 0    byte 1 */
	/*	sBGR BBBB GGGG RRRR */
	/*	x000 4321 4321 4321 */
	r = ((newval >> 12) & 0x01) | ((newval << 1) & 0x1e);
	g = ((newval >> 13) & 0x01) | ((newval >> 3) & 0x1e);
	b = ((newval >> 14) & 0x01) | ((newval >> 7) & 0x1e);

	/* normal colors */
	palette_set_color(offset + 0 * palette_entries, normal_pal[r],  normal_pal[g],  normal_pal[b]);
	palette_set_color(offset + 1 * palette_entries, shadow_pal[r],  shadow_pal[g],  shadow_pal[b]);
	palette_set_color(offset + 2 * palette_entries, hilight_pal[r], hilight_pal[g], hilight_pal[b]);
}



/*************************************
 *
 *	Initialize a virtual tilemap
 *
 *************************************/

int segaic16_init_virtual_tilemaps(int numpages, int palette_offset, void (*tile_cb)(int))
{
	int pagenum;

	/* create the tilemaps for the tile pages */
	for (pagenum = 0; pagenum < numpages; pagenum++)
	{
		/* each page is 64x32 */
		segaic16_tilemaps[pagenum] = tilemap_create(tile_cb, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8,8, 64,32);
		if (!segaic16_tilemaps[pagenum])
			return 0;

		/* configure the tilemap */
		tilemap_set_palette_offset(segaic16_tilemaps[pagenum], palette_offset);
		tilemap_set_transparent_pen(segaic16_tilemaps[pagenum], 0);
	}
	return 1;
}



/*************************************
 *
 *	Draw a split tilemap in up to
 *	four pieces
 *
 *************************************/

void segaic16_draw_virtual_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT16 pages, UINT16 xscroll, UINT16 yscroll, UINT32 flags, UINT32 priority)
{
	int leftmin = -1, leftmax = -1, rightmin = -1, rightmax = -1;
	int topmin = -1, topmax = -1, bottommin = -1, bottommax = -1;
	struct rectangle pageclip;

	/* which half/halves of the virtual tilemap do we intersect in the X direction? */
	if (xscroll < 64*8 - Machine->drv->screen_width)
	{
		leftmin = 0;
		leftmax = Machine->drv->screen_width - 1;
		rightmin = -1;
	}
	else if (xscroll < 64*8)
	{
		leftmin = 0;
		leftmax = 64*8 - xscroll - 1;
		rightmin = leftmax + 1;
		rightmax = Machine->drv->screen_width - 1;
	}
	else if (xscroll < 128*8 - Machine->drv->screen_width)
	{
		rightmin = 0;
		rightmax = Machine->drv->screen_width - 1;
		leftmin = -1;
	}
	else
	{
		rightmin = 0;
		rightmax = 128*8 - xscroll - 1;
		leftmin = rightmax + 1;
		leftmax = Machine->drv->screen_width - 1;
	}

	/* which half/halves of the virtual tilemap do we intersect in the Y direction? */
	if (yscroll < 32*8 - Machine->drv->screen_height)
	{
		topmin = 0;
		topmax = Machine->drv->screen_height - 1;
		bottommin = -1;
	}
	else if (yscroll < 32*8)
	{
		topmin = 0;
		topmax = 32*8 - yscroll - 1;
		bottommin = topmax + 1;
		bottommax = Machine->drv->screen_height - 1;
	}
	else if (yscroll < 64*8 - Machine->drv->screen_height)
	{
		bottommin = 0;
		bottommax = Machine->drv->screen_height - 1;
		topmin = -1;
	}
	else
	{
		bottommin = 0;
		bottommax = 64*8 - yscroll - 1;
		topmin = bottommax + 1;
		topmax = Machine->drv->screen_height - 1;
	}

	/* draw the upper-left chunk */
	if (leftmin != -1 && topmin != -1)
	{
		pageclip.min_x = (leftmin < cliprect->min_x) ? cliprect->min_x : leftmin;
		pageclip.max_x = (leftmax > cliprect->max_x) ? cliprect->max_x : leftmax;
		pageclip.min_y = (topmin < cliprect->min_y) ? cliprect->min_y : topmin;
		pageclip.max_y = (topmax > cliprect->max_y) ? cliprect->max_y : topmax;
		if (pageclip.min_x <= pageclip.max_x && pageclip.min_y <= pageclip.max_y)
		{
			segaic16_tilemap_page = (pages >> 0) & 0xf;
			tilemap_set_scrollx(segaic16_tilemaps[segaic16_tilemap_page], 0, xscroll);
			tilemap_set_scrolly(segaic16_tilemaps[segaic16_tilemap_page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, segaic16_tilemaps[segaic16_tilemap_page], flags, priority);
		}
	}

	/* draw the upper-right chunk */
	if (rightmin != -1 && topmin != -1)
	{
		pageclip.min_x = (rightmin < cliprect->min_x) ? cliprect->min_x : rightmin;
		pageclip.max_x = (rightmax > cliprect->max_x) ? cliprect->max_x : rightmax;
		pageclip.min_y = (topmin < cliprect->min_y) ? cliprect->min_y : topmin;
		pageclip.max_y = (topmax > cliprect->max_y) ? cliprect->max_y : topmax;
		if (pageclip.min_x <= pageclip.max_x && pageclip.min_y <= pageclip.max_y)
		{
			segaic16_tilemap_page = (pages >> 4) & 0xf;
			tilemap_set_scrollx(segaic16_tilemaps[segaic16_tilemap_page], 0, xscroll);
			tilemap_set_scrolly(segaic16_tilemaps[segaic16_tilemap_page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, segaic16_tilemaps[segaic16_tilemap_page], flags, priority);
		}
	}

	/* draw the lower-left chunk */
	if (leftmin != -1 && bottommin != -1)
	{
		pageclip.min_x = (leftmin < cliprect->min_x) ? cliprect->min_x : leftmin;
		pageclip.max_x = (leftmax > cliprect->max_x) ? cliprect->max_x : leftmax;
		pageclip.min_y = (bottommin < cliprect->min_y) ? cliprect->min_y : bottommin;
		pageclip.max_y = (bottommax > cliprect->max_y) ? cliprect->max_y : bottommax;
		if (pageclip.min_x <= pageclip.max_x && pageclip.min_y <= pageclip.max_y)
		{
			segaic16_tilemap_page = (pages >> 8) & 0xf;
			tilemap_set_scrollx(segaic16_tilemaps[segaic16_tilemap_page], 0, xscroll);
			tilemap_set_scrolly(segaic16_tilemaps[segaic16_tilemap_page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, segaic16_tilemaps[segaic16_tilemap_page], flags, priority);
		}
	}

	/* draw the lower-right chunk */
	if (rightmin != -1 && bottommin != -1)
	{
		pageclip.min_x = (rightmin < cliprect->min_x) ? cliprect->min_x : rightmin;
		pageclip.max_x = (rightmax > cliprect->max_x) ? cliprect->max_x : rightmax;
		pageclip.min_y = (bottommin < cliprect->min_y) ? cliprect->min_y : bottommin;
		pageclip.max_y = (bottommax > cliprect->max_y) ? cliprect->max_y : bottommax;
		if (pageclip.min_x <= pageclip.max_x && pageclip.min_y <= pageclip.max_y)
		{
			segaic16_tilemap_page = (pages >> 12) & 0xf;
			tilemap_set_scrollx(segaic16_tilemaps[segaic16_tilemap_page], 0, xscroll);
			tilemap_set_scrolly(segaic16_tilemaps[segaic16_tilemap_page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, segaic16_tilemaps[segaic16_tilemap_page], flags, priority);
		}
	}
}



/*************************************
 *
 *	Generic write handler for
 *	tilemaps
 *
 *************************************/

WRITE16_HANDLER( segaic16_tileram_w )
{
	COMBINE_DATA(&segaic16_tileram[offset]);
	tilemap_mark_tile_dirty(segaic16_tilemaps[offset / (64*32)], offset % (64*32));
}
