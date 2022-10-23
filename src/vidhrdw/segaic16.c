/***************************************************************************

	Sega 16-bit common hardware

****************************************************************************

	Hang On
	------------
		Control Board (834-5668):
			315-5011       -- sprite line comparitor
			315-5012       -- sprite generator control
			315-5049 (x2)  -- tilemaps
			315-5107 (PAL x2) -- horizontal timing control
			315-5108       -- vertical timing control
			315-5122 (PAL) -- timing

	Enduro Racer
	------------
		CPU Side (171-5319):
			315-5164 (PAL)
			315-5165 (PAL)
			315-5166 (PAL)
			315-5167 (PAL)

		Video Side (171-5320):
			315-5049 (x2)  -- tilemaps
			315-5011       -- sprite line comparitor
			315-5012       -- sprite generator control
			315-5106 (PAL)
			315-5107 (PAL)
			315-5108 (PAL)
			315-5168 (PAL)
			315-5170 (PAL)
			315-5171 (PAL)
			315-5172 (PAL)

	Pre-System 16
	-------------
		Main Board (171-5335):
			315-5011       -- sprite line comparitor
			315-5012       -- sprite generator control
			315-5049 (x2)  -- tilemaps
			315-5107 (PAL) -- display timing
			315-5108 (PAL) -- display timing
			315-5141 (PAL) -- Z80 address decoding
			315-5143 (PAL) -- sprite-related?
			315-5144 (PAL) -- sprite-related?
			315-5147 (PAL) -- unknown, DTACK-related
			315-5149 (PAL) -- video mixing
			315-5193 (PAL) -- 68000/MCU interface & address decoding
			315-5202 (PAL) -- 68000/MCU interface & address decoding

	Sega System 16A
	---------------
		Bottom Board (171-5307):
			315-5011       -- sprite line comparitor
			315-5012       -- sprite generator control
			315-5049 (x2)  -- tilemaps
			315-5107 (PAL) -- display timing
			315-5108 (PAL) -- display timing
			315-5143 (PAL) -- sprite-related?
			315-5144 (PAL) -- sprite-related?
			315-5145 (PAL)

		Top Board (171-5306):
			315-5141 (PAL) -- Z80 address decoding
			315-5142 (PAL)
			315-5149 (PAL) -- video mixing
			315-5150 (PAL)

	Sega System 16B
	---------------
		Main Board (171-5357):
			315-5195       -- memory mapper
			315-5196       -- sprite generator
			315-5197       -- tilemap generator
			315-5213 (PAL) -- sprite-related
			315-5214 (PAL) -- unknown

		ROM Board (171-5521):
			315-5298 (PAL)

		ROM Board (171-5704):
			315-5298 (PAL)

		ROM Board (171-5797):
			315-5248       -- hardware multiplier
			315-5250       -- compare/timer
			315-5298 (PAL)

	Sega System 18
	--------------
		Main Board (171-5873B):
			315-5242       -- color encoder
			315-5296       -- I/O chip
			315-5313       -- VDP
			315-5360       -- memory mapper?
			315-5361       -- sprite generator
			315-5362       -- tilemap generator
			315-5373 (PAL) -- video mixing
			315-5374 (PAL) -- sprite timing
			315-5375 (PAL) -- system timing
			315-5389 (PAL) -- VDP sync
			315-5390 (PAL)
			315-5391 (PAL) -- Z80 address decoding

		Main Board (171-5873-02B):
			315-5242       -- color encoder
			315-5296       -- I/O chip
			315-5313       -- VDP
			315-5360       -- memory mapper?
			315-5361       -- sprite generator
			315-5362       -- tilemap generator
			315-5374 (PAL) -- sprite timing
			315-5375 (PAL) -- system timing
			315-5389 (PAL) -- VDP sync
			315-5391 (PAL) -- Z80 address decoding
			315-5430 (PAL) -- video mixing

		ROM Board (171-5987A):
			315-5436       -- tile/sprite banking

	Sega System C
	-------------
		Main Board:
			315-5242       -- color encoder
			315-5296       -- I/O chip
			315-5313       -- VDP
			315-5393 (PAL)
			315-5394 (PAL)
			315-5395 (PAL)

	Super Hang On
	-------------
		CPU Board 171-5376-01:
			315-5195       -- memory mapper
			315-5218       -- PCM sound controller
			315-5155 (PAL)
			315-5222 (PAL)
			315-5223a (PAL)
			315-5224 (PAL)
			315-5225 (PAL)
			315-5226 (PAL)

		VIDEO Board: (not the same as out run !) 171-5480
			315-5196       -- sprite generator
			315-5197       -- tilemap generator
			315-5213 (PAL) -- sprite-related
			315-5242       -- color encoder
			315-5251 (PAL)


	Sega System 32
	--------------
		Main Board (317-5964):
			315-5242       -- color encoder
			315-5296       -- I/O chip
			315-5385
			315-5386       -- tilemap generator
			315-5387       -- sprite generator
			315-5388       -- video mixing
			315-5441 (PAL)
			315-5476

	X-Board
	-------
		Main Board:
			315-5197       -- tilemap generator
			315-5211A      -- sprite generator
			315-5218       -- PCM sound controller
			315-5242       -- color encoder
			315-5248 (x2)  -- hardware multiplier
			315-5249 (x2)  -- hardware divider
			315-5250 (x2)  -- compare/timer
			315-5275       -- road generator
			315-5278 (PAL) -- sprite ROM bank control
			315-5279 (PAL) -- video mixing (Afterburner)
			315-5280 (PAL) -- Z80 address decoding
			315-5290 (PAL) -- main CPU address decoding
			315-5291 (PAL) -- main CPU address decoding
			315-5304 (PAL) -- video mixing (Line of Fire)

	Y-Board
	-------
		Main Board (837-6565):
			315-5218       -- PCM sound controller
			315-5248 (x3)  -- hardware multiplier
			315-5249 (x3)  -- hardware divider
			315-5280 (PAL) -- Z80 address decoding
			315-5296       -- I/O chip
			315-5314 (PAL)
			315-5315 (PAL)
			315-5316 (PAL)
			315-5317 (PAL)
			315-5318 (PAL)
			315-5328 (PAL)

		Video Board (837-6566):
			315-5196       -- sprite generator
			315-5213 (PAL) -- sprite-related
			315-5242       -- color encoder
			315-5305
			315-5306
			315-5312       -- video mixing
			315-5319 (PAL)
			315-5325 (PAL)


	Custom parts
	------------
		           SYS1  SYS2  PR16  S16A  S16B  SY18  XBRD  YBRD  SYSC  SY24  SY32
		315-5011:   xx    xx    xx    xx                                             -- sprite line comparitor
		315-5012:   xx    xx    xx    xx                                             -- sprite generator control
		315-5049:         xx    x2    x2                                             -- tilemap generator
		315-5195:                           xx                                       -- memory mapper
		315-5196:                           xx                xx                     -- sprite genereator
		315-5197:                           xx          xx                           -- tilemap generator
		315-5211A:                                      xx                           -- sprite generator
		315-5242:                                 xx    xx    xx    xx    xx    xx   -- color encoder
		315-5248:                           xx          x2    x3                     -- hardware multiplier
		315-5249:                                       x2    x3                     -- hardware divider
		315-5250:                           xx          x2                           -- compare/timer
		315-5275:                                       xx                           -- road generator
		315-5296:                                 xx          xx    xx          xx   -- I/O chip
		315-5305:                                             xx                     --
		315-5312:                                             xx                     -- video mixing
		315-5313:                                 xx                xx               -- VDP
		315-5360:                                 xx                                 -- memory mapper
		315-5361:                                 xx                                 -- sprite generator
		315-5362:                                 xx                                 -- tilemap generator
		315-5385:                                                               xx   -- ???
		315-5386:                                                               xx   -- tilemap generator
		315-5387:                                                               xx   -- sprite generator
		315-5388:                                                               xx   -- video mixing
		315-5436:                                 xx                                 -- sprite/tile banking
		315-5476:                                                               xx   -- ????

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
#include "segaic16.h"
#include "vidhrdw/res_net.h"



/*************************************
 *
 *	Debugging
 *
 *************************************/

#define PRINT_UNUSUAL_MODES		(0)
#define DEBUG_ROAD				(0)



/*************************************
 *
 *	Type definitions
 *
 *************************************/

struct palette_info
{
	INT32			entries;						/* number of entries (not counting shadows) */
	UINT8			normal[32];						/* RGB translations for normal pixels */
	UINT8			shadow[32];						/* RGB translations for shadowed pixels */
	UINT8			hilight[32];					/* RGB translations for hilighted pixels */
};


struct tilemap_callback_info
{
	data16_t *		rambase;						/* base of RAM for this tilemap page */
	const UINT8 *	bank;							/* pointer to bank array */
	UINT16			banksize;						/* size of banks */
};


struct tilemap_info
{
	UINT8			index;							/* index of this structure */
	UINT8			type;							/* type of tilemap (see segaic16.h for details) */
	UINT8			numpages;						/* number of allocated pages */
	UINT8			flip;							/* screen flip? */
	UINT8			rowscroll, colscroll;			/* are rowscroll/colscroll enabled (if external enables are used) */
	UINT8			bank[8];						/* indexes of the tile banks */
	UINT16			banksize;						/* number of tiles per bank */
	UINT16			latched_xscroll[4];				/* latched X scroll values */
	UINT16			latched_yscroll[4];				/* latched Y scroll values */
	UINT16			latched_pageselect[4];			/* latched page select values */
	INT32			xoffs;							/* X scroll offset */
	struct tilemap *tilemaps[16];					/* up to 16 tilemap pages */
	struct tilemap *textmap;						/* a single text tilemap */
	struct tilemap_callback_info tilemap_info[16];	/* callback info for 16 tilemap pages */
	struct tilemap_callback_info textmap_info;		/* callback info for a single textmap page */
	void			(*reset)(struct tilemap_info *info);/* reset callback */
	void			(*draw_layer)(struct tilemap_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int which, int flags, int priority);
	data16_t *		textram;						/* pointer to textram pointer */
	data16_t *		tileram;						/* pointer to tileram pointer */
};


struct sprite_info
{
	UINT8			index;							/* index of this structure */
	UINT8			type;							/* type of sprite system (see segaic16.h for details) */
	UINT8			flip;							/* screen flip? */
	UINT8			shadow;							/* shadow or hilight? */
	UINT8			bank[16];						/* banking redirection */
	UINT16			colorbase;						/* base color index */
	INT32			ramsize;						/* size of sprite RAM */
	INT32			xoffs;							/* X scroll offset */
	void			(*draw)(struct sprite_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect);
	data16_t *		spriteram;						/* pointer to spriteram pointer */
	data16_t *		buffer;							/* buffered spriteram for those that use it */
};


struct road_info
{
	UINT8			index;							/* index of this structure */
	UINT8			type;							/* type of road system (see segaic16.h for details) */
	UINT8			control;						/* control register value */
	UINT16			colorbase1;						/* color base for road ROM data */
	UINT16			colorbase2;						/* color base for road background data */
	UINT16			colorbase3;						/* color base for sky data */
	INT32			xoffs;							/* X scroll offset */
	void			(*draw)(struct road_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority);
	data16_t *		roadram;						/* pointer to roadram pointer */
	UINT8 *			gfx;							/* expanded road graphics */
};



/*************************************
 *
 *	Globals
 *
 *************************************/

UINT8 segaic16_display_enable;
data16_t *segaic16_tileram_0;
data16_t *segaic16_textram_0;
data16_t *segaic16_spriteram_0;
data16_t *segaic16_spriteram_1;
data16_t *segaic16_roadram_0;



/*************************************
 *
 *	Statics
 *
 *************************************/

static struct palette_info palette;
static struct tilemap_info tilemap[SEGAIC16_MAX_TILEMAPS];
static struct sprite_info sprites[SEGAIC16_MAX_SPRITES];
static struct road_info road[SEGAIC16_MAX_ROADS];



/*************************************
 *
 *	Misc functions
 *
 *************************************/

void segaic16_set_display_enable(int enable)
{
	enable = (enable != 0);
	if (segaic16_display_enable != enable)
	{
		force_partial_update(cpu_getscanline());
		segaic16_display_enable = enable;
	}
}



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

void segaic16_palette_init(int entries)
{
	static const int resistances_normal[6] = { 3900, 2000, 1000, 1000/2, 1000/4, 0   };
	static const int resistances_sh[6]     = { 3900, 2000, 1000, 1000/2, 1000/4, 470 };
	double weights[2][6];
	int i;

	/* compute the number of palette entries */
	palette.entries = entries;

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

		palette.normal[i] = combine_6_weights(weights[0], i0, i1, i2, i3, i4, 0);
		palette.shadow[i] = combine_6_weights(weights[1], i0, i1, i2, i3, i4, 0);
		palette.hilight[i] = combine_6_weights(weights[1], i0, i1, i2, i3, i4, 1);
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
	palette_set_color(offset + 0 * palette.entries, palette.normal[r],  palette.normal[g],  palette.normal[b]);
	palette_set_color(offset + 1 * palette.entries, palette.shadow[r],  palette.shadow[g],  palette.shadow[b]);
	palette_set_color(offset + 2 * palette.entries, palette.hilight[r], palette.hilight[g], palette.hilight[b]);
}



/*************************************
 *
 *	Draw a split tilemap in up to
 *	four pieces
 *
 *************************************/

void segaic16_draw_virtual_tilemap(struct tilemap_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT16 pages, UINT16 xscroll, UINT16 yscroll, UINT32 flags, UINT32 priority)
{
	int leftmin = -1, leftmax = -1, rightmin = -1, rightmax = -1;
	int topmin = -1, topmax = -1, bottommin = -1, bottommax = -1;
	struct rectangle pageclip;
	int page;

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

	/* if the tilemap is flipped, we need to flip our sense within each quadrant */
	if (info->flip)
	{
		if (leftmin != -1)
		{
			int temp = leftmin;
			leftmin = Machine->drv->screen_width - 1 - leftmax;
			leftmax = Machine->drv->screen_width - 1 - temp;
		}
		if (rightmin != -1)
		{
			int temp = rightmin;
			rightmin = Machine->drv->screen_width - 1 - rightmax;
			rightmax = Machine->drv->screen_width - 1 - temp;
		}
		if (topmin != -1)
		{
			int temp = topmin;
			topmin = Machine->drv->screen_height - 1 - topmax;
			topmax = Machine->drv->screen_height - 1 - temp;
		}
		if (bottommin != -1)
		{
			int temp = bottommin;
			bottommin = Machine->drv->screen_height - 1 - bottommax;
			bottommax = Machine->drv->screen_height - 1 - temp;
		}
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
			page = (pages >> 0) & 0xf;
			tilemap_set_scrollx(info->tilemaps[page], 0, xscroll);
			tilemap_set_scrolly(info->tilemaps[page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, info->tilemaps[page], flags, priority);
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
			page = (pages >> 4) & 0xf;
			tilemap_set_scrollx(info->tilemaps[page], 0, xscroll);
			tilemap_set_scrolly(info->tilemaps[page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, info->tilemaps[page], flags, priority);
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
			page = (pages >> 8) & 0xf;
			tilemap_set_scrollx(info->tilemaps[page], 0, xscroll);
			tilemap_set_scrolly(info->tilemaps[page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, info->tilemaps[page], flags, priority);
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
			page = (pages >> 12) & 0xf;
			tilemap_set_scrollx(info->tilemaps[page], 0, xscroll);
			tilemap_set_scrolly(info->tilemaps[page], 0, yscroll);
			tilemap_draw(bitmap, &pageclip, info->tilemaps[page], flags, priority);
		}
	}
}



/*******************************************************************************************
 *
 *	Hang On/System 16A-style tilemaps
 *
 *	4 total pages (Hang On)
 *	8 total pages (System 16A)
 *	Column/rowscroll enabled via external signals
 *
 *	Tile format:
 *		Bits               Usage
 *		??------ --------  Unknown
 *		--b----- --------  Tile bank select
 *		---p---- --------  Tile priority versus sprites
 *		----cccc ccc-----  Tile color palette
 *		----nnnn nnnnnnnn  Tile index
 *
 *	Text format:
 *		Bits               Usage
 *		????---- --------  Unknown
 *		----p--- --------  Priority
 *		-----ccc --------  Tile color palette
 *		-------- nnnnnnnn  Tile index
 *
 *	Text RAM:
 *		Offset   Bits               Usage
 *		E8C      -aaa-bbb -ccc-ddd  Background tilemap page select (screen flipped)
 *		E8E      -aaa-bbb -ccc-ddd  Foreground tilemap page select (screen flipped)
 *		E9C      -aaa-bbb -ccc-ddd  Background tilemap page select
 *		E9E      -aaa-bbb -ccc-ddd  Foreground tilemap page select
 *		F24      -------- vvvvvvvv  Foreground tilemap vertical scroll
 *		F26      -------- vvvvvvvv  Background tilemap vertical scroll
 *		F30-F7D  -------- vvvvvvvv  Foreground tilemap per-16-pixel-column vertical scroll (every 2 words)
 *		F32-F7F  -------- vvvvvvvv  Background tilemap per-16-pixel-column vertical scroll (every 2 words)
 *		F80-FED  -------h hhhhhhhh  Foreground tilemap per-8-pixel-row horizontal scroll (every 2 words)
 *		F82-FEF  -------h hhhhhhhh  Background tilemap per-8-pixel-row horizontal scroll (every 2 words)
 *		FF8      -------h hhhhhhhh  Foreground tilemap horizontal scroll
 *		FFA      -------h hhhhhhhh  Background tilemap horizontal scroll
 *
 *******************************************************************************************/

static void segaic16_tilemap_16a_tile_info(int tile_index)
{
	const struct tilemap_callback_info *info = tile_info.user_data;
	UINT16 data = info->rambase[tile_index];
	int code = ((data >> 1) & 0x1000) | (data & 0xfff);
	int color = (data >> 5) & 0x7f;

	SET_TILE_INFO(0, code, color, 0);
	tile_info.priority = (data >> 12) & 1;
}


static void segaic16_tilemap_16a_text_info(int tile_index)
{
	const struct tilemap_callback_info *info = tile_info.user_data;
	UINT16 data = info->rambase[tile_index];
	int color = (data >> 8) & 0x07;
	int code = data & 0xff;

	SET_TILE_INFO(0, code, color, 0);
	tile_info.priority = (data >> 11) & 1;
}


static void segaic16_tilemap_16a_draw_layer(struct tilemap_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int which, int flags, int priority)
{
	data16_t *textram = info->textram;

	/* note that the scrolling for these games can only scroll as much as the top-left */
	/* page; in order to scroll beyond that they swap pages and reset the scroll value */
	UINT16 xscroll = textram[0xff8/2 + which] & 0x1ff;
	UINT16 yscroll = textram[0xf24/2 + which] & 0x0ff;
	UINT16 pages = textram[(info->flip ? 0xe8e/2 : 0xe9e/2) - which];
	int x, y;

	/* pages are swapped along the X direction, and there are only 8 of them */
	pages = ((pages >> 4) & 0x0707) | ((pages << 4) & 0x7070);
	if (info->numpages == 4)
		pages &= 0x3333;

	/* column AND row scroll */
	if (info->colscroll && info->rowscroll)
	{
		if (PRINT_UNUSUAL_MODES) printf("Column AND row scroll\n");

		/* loop over row chunks */
		for (y = cliprect->min_y & ~7; y <= cliprect->max_y; y += 8)
		{
			int rowscrollindex = (info->flip ? (216 - y) : y) / 8;
			struct rectangle rowcolclip;

			/* adjust to clip this row only */
			rowcolclip.min_y = (y < cliprect->min_y) ? cliprect->min_y : y;
			rowcolclip.max_y = (y + 7 > cliprect->max_y) ? cliprect->max_y : y + 7;

			/* loop over column chunks */
			for (x = cliprect->min_x & ~15; x <= cliprect->max_x; x += 16)
			{
				UINT16 effxscroll, effyscroll;

				/* adjust to clip this column only */
				rowcolclip.min_x = (x < cliprect->min_x) ? cliprect->min_x : x;
				rowcolclip.max_x = (x + 15 > cliprect->max_x) ? cliprect->max_x : x + 15;

				/* get the effective scroll values */
				effxscroll = textram[0xf80/2 + rowscrollindex * 2 + which] & 0x1ff;
				effyscroll = textram[0xf30/2 + (x/16) * 2 + which] & 0x0ff;

				/* adjust the xscroll for flipped screen */
				if (info->flip)
					effxscroll += 17;

				/* draw the chunk */
				effxscroll = (0xc8 - effxscroll + info->xoffs) & 0x3ff;
				effyscroll = effyscroll & 0x1ff;
				segaic16_draw_virtual_tilemap(info, bitmap, &rowcolclip, pages, effxscroll, effyscroll, flags, priority);
			}
		}
	}
	else if (info->colscroll)
	{
		if (PRINT_UNUSUAL_MODES) printf("Column scroll\n");

		/* loop over column chunks */
		for (x = cliprect->min_x & ~15; x <= cliprect->max_x; x += 16)
		{
			struct rectangle colclip = *cliprect;
			UINT16 effxscroll, effyscroll;

			/* adjust to clip this row only */
			colclip.min_x = (x < cliprect->min_x) ? cliprect->min_x : x;
			colclip.max_x = (x + 15 > cliprect->max_x) ? cliprect->max_x : x + 15;

			/* get the effective scroll values */
			effyscroll = textram[0xf30/2 + (x/16) * 2 + which] & 0x0ff;

			/* adjust the xscroll for flipped screen */
			if (info->flip)
				effxscroll += 17;

			/* draw the chunk */
			effxscroll = (0xc8 - xscroll + info->xoffs) & 0x3ff;
			effyscroll = effyscroll & 0x1ff;
			segaic16_draw_virtual_tilemap(info, bitmap, &colclip, pages, effxscroll, effyscroll, flags, priority);
		}
	}
	else if (info->rowscroll)
	{
		if (PRINT_UNUSUAL_MODES) printf("Row scroll\n");

		/* loop over row chunks */
		for (y = cliprect->min_y & ~7; y <= cliprect->max_y; y += 8)
		{
			int rowscrollindex = (info->flip ? (216 - y) : y) / 8;
			struct rectangle rowclip = *cliprect;
			UINT16 effxscroll, effyscroll;

			/* adjust to clip this row only */
			rowclip.min_y = (y < cliprect->min_y) ? cliprect->min_y : y;
			rowclip.max_y = (y + 7 > cliprect->max_y) ? cliprect->max_y : y + 7;

			/* get the effective scroll values */
			effxscroll = textram[0xf80/2 + rowscrollindex * 2 + which] & 0x1ff;
			effyscroll = yscroll;

			/* adjust the xscroll for flipped screen */
			if (info->flip)
				effxscroll += 17;

			/* draw the chunk */
			effxscroll = (0xc8 - effxscroll + info->xoffs) & 0x3ff;
			effyscroll = effyscroll & 0x1ff;
			segaic16_draw_virtual_tilemap(info, bitmap, &rowclip, pages, effxscroll, effyscroll, flags, priority);
		}
	}
	else
	{
		/* adjust the xscroll for flipped screen */
		if (info->flip)
			xscroll += 17;
		xscroll = (0xc8 - xscroll + info->xoffs) & 0x3ff;
		yscroll = yscroll & 0x1ff;
		segaic16_draw_virtual_tilemap(info, bitmap, cliprect, pages, xscroll, yscroll, flags, priority);
	}
}



/*******************************************************************************************
 *
 *	System 16B-style tilemaps
 *
 *	16 total pages
 *	Column/rowscroll enabled via bits in text layer
 *	Alternate tilemap support
 *
 *	Tile format:
 *		Bits               Usage
 *		p------- --------  Tile priority versus sprites
 *		-??----- --------  Unknown
 *		---ccccc cc------  Tile color palette
 *		---nnnnn nnnnnnnn  Tile index
 *
 *	Text format:
 *		Bits               Usage
 *		p------- --------  Tile priority versus sprites
 *		-???---- --------  Unknown
 *		----ccc- --------  Tile color palette
 *		-------n nnnnnnnn  Tile index
 *
 *	Alternate tile format:
 *		Bits               Usage
 *		p------- --------  Tile priority versus sprites
 *		-??----- --------  Unknown
 *		----cccc ccc-----  Tile color palette
 *		---nnnnn nnnnnnnn  Tile index
 *
 *	Alternate text format:
 *		Bits               Usage
 *		p------- --------  Tile priority versus sprites
 *		-???---- --------  Unknown
 *		-----ccc --------  Tile color palette
 *		-------- nnnnnnnn  Tile index
 *
 *	Text RAM:
 *		Offset   Bits               Usage
 *		E80      aaaabbbb ccccdddd  Foreground tilemap page select
 *		E82      aaaabbbb ccccdddd  Background tilemap page select
 *		E84      aaaabbbb ccccdddd  Alternate foreground tilemap page select
 *		E86      aaaabbbb ccccdddd  Alternate background tilemap page select
 *		E90      c------- --------  Foreground tilemap column scroll enable
 *		         -------v vvvvvvvv  Foreground tilemap vertical scroll
 *	    E92      c------- --------  Background tilemap column scroll enable
 *		         -------v vvvvvvvv  Background tilemap vertical scroll
 *		E94      -------v vvvvvvvv  Alternate foreground tilemap vertical scroll
 *	    E96      -------v vvvvvvvv  Alternate background tilemap vertical scroll
 *	    E98      r------- --------  Foreground tilemap row scroll enable
 *		         ------hh hhhhhhhh  Foreground tilemap horizontal scroll
 *	    E9A      r------- --------  Background tilemap row scroll enable
 *		         ------hh hhhhhhhh  Background tilemap horizontal scroll
 *	    E9C      ------hh hhhhhhhh  Alternate foreground tilemap horizontal scroll
 *	    E9E      ------hh hhhhhhhh  Alternate background tilemap horizontal scroll
 *	    F16-F3F  -------- vvvvvvvv  Foreground tilemap per-16-pixel-column vertical scroll
 *		F56-F7F  -------- vvvvvvvv  Background tilemap per-16-pixel-column vertical scroll
 *		F80-FB7  a------- --------  Foreground tilemap per-8-pixel-row alternate tilemap enable
 *		         -------h hhhhhhhh  Foreground tilemap per-8-pixel-row horizontal scroll
 *		FC0-FF7  a------- --------  Background tilemap per-8-pixel-row alternate tilemap enable
 *		         -------h hhhhhhhh  Background tilemap per-8-pixel-row horizontal scroll
 *
 *******************************************************************************************/

static void segaic16_tilemap_16b_tile_info(int tile_index)
{
	const struct tilemap_callback_info *info = tile_info.user_data;
	UINT16 data = info->rambase[tile_index];
	int color = (data >> 6) & 0x7f;
	int code = data & 0x1fff;

	code = info->bank[code / info->banksize] * info->banksize + code % info->banksize;

	SET_TILE_INFO(0, code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}


static void segaic16_tilemap_16b_text_info(int tile_index)
{
	const struct tilemap_callback_info *info = tile_info.user_data;
	UINT16 data = info->rambase[tile_index];
	int bank = info->bank[0];
	int color = (data >> 9) & 0x07;
	int code = data & 0x1ff;

	SET_TILE_INFO(0, bank * info->banksize + code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}


static void segaic16_tilemap_16b_alt_tile_info(int tile_index)
{
	const struct tilemap_callback_info *info = tile_info.user_data;
	UINT16 data = info->rambase[tile_index];
	int color = (data >> 5) & 0x7f;
	int code = data & 0x1fff;

	code = info->bank[code / info->banksize] * info->banksize + code % info->banksize;

	SET_TILE_INFO(0, code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}


static void segaic16_tilemap_16b_alt_text_info(int tile_index)
{
	const struct tilemap_callback_info *info = tile_info.user_data;
	UINT16 data = info->rambase[tile_index];
	int bank = info->bank[0];
	int color = (data >> 8) & 0x07;
	int code = data & 0xff;

	SET_TILE_INFO(0, bank * info->banksize + code, color, 0);
	tile_info.priority = (data >> 15) & 1;
}


static void segaic16_tilemap_16b_draw_layer(struct tilemap_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int which, int flags, int priority)
{
	data16_t *textram = info->textram;
	UINT16 xscroll, yscroll, pages;
	int x, y;

	/* get global values */
	xscroll = info->latched_xscroll[which];
	yscroll = info->latched_yscroll[which];
	pages = info->latched_pageselect[which];

	/* column scroll? */
	if (yscroll & 0x8000)
	{
		if (PRINT_UNUSUAL_MODES) printf("Column AND row scroll\n");

		/* loop over row chunks */
		for (y = cliprect->min_y & ~7; y <= cliprect->max_y; y += 8)
		{
			int rowscrollindex = (info->flip ? (216 - y) : y) / 8;
			struct rectangle rowcolclip;

			/* adjust to clip this row only */
			rowcolclip.min_y = (y < cliprect->min_y) ? cliprect->min_y : y;
			rowcolclip.max_y = (y + 7 > cliprect->max_y) ? cliprect->max_y : y + 7;

			/* loop over column chunks */
			for (x = ((cliprect->min_x + 8) & ~15) - 8; x <= cliprect->max_x; x += 16)
			{
				UINT16 effxscroll, effyscroll, rowscroll;
				UINT16 effpages = pages;

				/* adjust to clip this column only */
				rowcolclip.min_x = (x < cliprect->min_x) ? cliprect->min_x : x;
				rowcolclip.max_x = (x + 15 > cliprect->max_x) ? cliprect->max_x : x + 15;

				/* get the effective scroll values */
				rowscroll = textram[0xf80/2 + 0x40/2 * which + rowscrollindex];
				effxscroll = (xscroll & 0x8000) ? rowscroll : xscroll;
				effyscroll = textram[0xf16/2 + 0x40/2 * which + (x+8)/16];

				/* are we using an alternate? */
				if (rowscroll & 0x8000)
				{
					effxscroll = info->latched_xscroll[which + 2];
					effyscroll = info->latched_yscroll[which + 2];
					effpages = info->latched_pageselect[which + 2];
				}

				/* draw the chunk */
				effxscroll = (0xc0 - effxscroll + info->xoffs) & 0x3ff;
				effyscroll = effyscroll & 0x1ff;
				segaic16_draw_virtual_tilemap(info, bitmap, &rowcolclip, effpages, effxscroll, effyscroll, flags, priority);
			}
		}
	}
	else
	{
		if (PRINT_UNUSUAL_MODES) printf("Row scroll\n");

		/* loop over row chunks */
		for (y = cliprect->min_y & ~7; y <= cliprect->max_y; y += 8)
		{
			int rowscrollindex = (info->flip ? (216 - y) : y) / 8;
			struct rectangle rowclip = *cliprect;
			UINT16 effxscroll, effyscroll, rowscroll;
			UINT16 effpages = pages;

			/* adjust to clip this row only */
			rowclip.min_y = (y < cliprect->min_y) ? cliprect->min_y : y;
			rowclip.max_y = (y + 7 > cliprect->max_y) ? cliprect->max_y : y + 7;

			/* get the effective scroll values */
			rowscroll = textram[0xf80/2 + 0x40/2 * which + rowscrollindex];
			effxscroll = (xscroll & 0x8000) ? rowscroll : xscroll;
			effyscroll = yscroll;

			/* are we using an alternate? */
			if (rowscroll & 0x8000)
			{
				effxscroll = info->latched_xscroll[which + 2];
				effyscroll = info->latched_yscroll[which + 2];
				effpages = info->latched_pageselect[which + 2];
			}

			/* draw the chunk */
			effxscroll = (0xc0 - effxscroll + info->xoffs) & 0x3ff;
			effyscroll = effyscroll & 0x1ff;
			segaic16_draw_virtual_tilemap(info, bitmap, &rowclip, effpages, effxscroll, effyscroll, flags, priority);
		}
	}
}


static void segaic16_tilemap_16b_latch_values(int param)
{
	struct tilemap_info *info = &tilemap[param];
	data16_t *textram = info->textram;
	int i;

	/* latch the scroll and page select values */
	for (i = 0; i < 4; i++)
	{
		info->latched_pageselect[i] = textram[0xe80/2 + i];
		info->latched_yscroll[i] = textram[0xe90/2 + i];
		info->latched_xscroll[i] = textram[0xe98/2 + i];
	}

	/* set a timer to do this again next frame */
	timer_set(cpu_getscanlinetime(261), param, segaic16_tilemap_16b_latch_values);
}


void segaic16_tilemap_16b_reset(struct tilemap_info *info)
{
	/* set a timer to latch values on scanline 261 */
	timer_set(cpu_getscanlinetime(261), info->index, segaic16_tilemap_16b_latch_values);
}



/*************************************
 *
 *	General tilemap initialization
 *
 *************************************/

int segaic16_tilemap_init(int which, int type, int colorbase, int xoffs, int numbanks)
{
	struct tilemap_info *info = &tilemap[which];
	void (*get_text_info)(int);
	void (*get_tile_info)(int);
	int pagenum;
	int i;

	/* reset the tilemap info */
	memset(info, 0, sizeof(*info));
	info->index = which;
	info->type = type;
	for (i = 0; i < numbanks; i++)
		info->bank[i] = i;
	info->banksize = 0x2000 / numbanks;
	info->xoffs = xoffs;

	/* set up based on which tilemap */
	switch (which)
	{
		case 0:
			info->textram = segaic16_textram_0;
			info->tileram = segaic16_tileram_0;
			break;

		default:
			 mame_done();
	}

	/* determine the parameters of the tilemaps */
	switch (type)
	{
		case SEGAIC16_TILEMAP_HANGON:
			get_text_info = segaic16_tilemap_16a_text_info;
			get_tile_info = segaic16_tilemap_16a_tile_info;
			info->numpages = 4;
			info->draw_layer = segaic16_tilemap_16a_draw_layer;
			info->reset = NULL;
			break;

		case SEGAIC16_TILEMAP_16A:
			get_text_info = segaic16_tilemap_16a_text_info;
			get_tile_info = segaic16_tilemap_16a_tile_info;
			info->numpages = 8;
			info->draw_layer = segaic16_tilemap_16a_draw_layer;
			info->reset = NULL;
			break;

		case SEGAIC16_TILEMAP_16B:
			get_text_info = segaic16_tilemap_16b_text_info;
			get_tile_info = segaic16_tilemap_16b_tile_info;
			info->numpages = 16;
			info->draw_layer = segaic16_tilemap_16b_draw_layer;
			info->reset = segaic16_tilemap_16b_reset;
			break;

		case SEGAIC16_TILEMAP_16B_ALT:
			get_text_info = segaic16_tilemap_16b_alt_text_info;
			get_tile_info = segaic16_tilemap_16b_alt_tile_info;
			info->numpages = 16;
			info->draw_layer = segaic16_tilemap_16b_draw_layer;
			info->reset = segaic16_tilemap_16b_reset;
			break;

		default:
			mame_done();
	}

	/* create the tilemap for the text layer */
	info->textmap = tilemap_create(get_text_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8,8, 64,28);
	if (!info->textmap)
		return 1;

	/* configure it */
	info->textmap_info.rambase = info->textram;
	info->textmap_info.bank = info->bank;
	info->textmap_info.banksize = info->banksize;
	tilemap_set_user_data(info->textmap, &info->textmap_info);
	tilemap_set_palette_offset(info->textmap, colorbase);
	tilemap_set_transparent_pen(info->textmap, 0);
	tilemap_set_scrolldx(info->textmap, -24*8 + xoffs, -24*8 + xoffs);

	/* create the tilemaps for the tile pages */
	for (pagenum = 0; pagenum < info->numpages; pagenum++)
	{
		/* each page is 64x32 */
		info->tilemaps[pagenum] = tilemap_create(get_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8,8, 64,32);
		if (!info->tilemaps[pagenum])
			return 1;

		/* configure the tilemap */
		info->tilemap_info[pagenum].rambase = info->tileram + pagenum * 64*32;
		info->tilemap_info[pagenum].bank = info->bank;
		info->tilemap_info[pagenum].banksize = info->banksize;
		tilemap_set_user_data(info->tilemaps[pagenum], &info->tilemap_info[pagenum]);
		tilemap_set_palette_offset(info->tilemaps[pagenum], colorbase);
		tilemap_set_transparent_pen(info->tilemaps[pagenum], 0);
	}
	return 0;
}



/*************************************
 *
 *	General tilemap rendering
 *
 *************************************/

void segaic16_tilemap_draw(int which, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int map, int priority, int priority_mark)
{
	struct tilemap_info *info = &tilemap[which];

	/* text layer is a special common case */
	if (map == SEGAIC16_TILEMAP_TEXT)
		tilemap_draw(bitmap, cliprect, info->textmap, priority, priority_mark);

	/* other layers are handled differently per-system */
	else
		(*info->draw_layer)(info, bitmap, cliprect, map, priority, priority_mark);
}



/*************************************
 *
 *	General tilemap reset
 *
 *************************************/

void segaic16_tilemap_reset(int which)
{
	struct tilemap_info *info = &tilemap[which];

	if (info->reset)
		(*info->reset)(info);
}



/*************************************
 *
 *	General tilemap banking
 *
 *************************************/

void segaic16_tilemap_set_bank(int which, int banknum, int offset)
{
	struct tilemap_info *info = &tilemap[which];

	if (info->bank[banknum] != offset)
	{
		force_partial_update(cpu_getscanline());
		info->bank[banknum] = offset;
		tilemap_mark_all_tiles_dirty(NULL);
	}
}



/*************************************
 *
 *	General tilemap screen flipping
 *
 *************************************/

void segaic16_tilemap_set_flip(int which, int flip)
{
	struct tilemap_info *info = &tilemap[which];
	int pagenum;

	flip = (flip != 0);
	if (info->flip != flip)
	{
		force_partial_update(cpu_getscanline());
		info->flip = flip;
		tilemap_set_flip(info->textmap, flip ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0);
		for (pagenum = 0; pagenum < info->numpages; pagenum++)
			tilemap_set_flip(info->tilemaps[pagenum], flip ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0);
	}
}



/*************************************
 *
 *	General tilemap row scroll enable
 *
 *************************************/

void segaic16_tilemap_set_rowscroll(int which, int enable)
{
	struct tilemap_info *info = &tilemap[which];

	enable = (enable != 0);
	if (info->rowscroll != enable)
	{
		force_partial_update(cpu_getscanline());
		info->rowscroll = enable;
	}
}



/*************************************
 *
 *	General tilemap column scroll enable
 *
 *************************************/

void segaic16_tilemap_set_colscroll(int which, int enable)
{
	struct tilemap_info *info = &tilemap[which];

	enable = (enable != 0);
	if (info->colscroll != enable)
	{
		force_partial_update(cpu_getscanline());
		info->colscroll = enable;
	}
}



/*************************************
 *
 *	General tilemap write handlers
 *
 *************************************/

WRITE16_HANDLER( segaic16_tileram_0_w )
{
	COMBINE_DATA(&segaic16_tileram_0[offset]);
	tilemap_mark_tile_dirty(tilemap[0].tilemaps[offset / (64*32)], offset % (64*32));
}


WRITE16_HANDLER( segaic16_textram_0_w )
{
	/* certain ranges need immediate updates */
	if (offset >= 0xe80/2)
		force_partial_update(cpu_getscanline());

	COMBINE_DATA(&segaic16_textram_0[offset]);
	tilemap_mark_tile_dirty(tilemap[0].textmap, offset);
}



/*******************************************************************************************
 *
 *	Hang On-style sprites
 *
 *		Offs  Bits               Usage
 *		 +0   bbbbbbbb --------  Bottom scanline of sprite - 1
 *		 +0   -------- tttttttt  Top scanline of sprite - 1
 *		 +2   bbbb---- --------  Sprite bank
 *		 +2   -------x xxxxxxxx  X position of sprite (position $BD is screen position 0)
 *		 +4   pppppppp pppppppp  Signed 16-bit pitch value between scanlines
 *		 +6   -ooooooo oooooooo  Offset within selected sprite bank
 *		 +6   f------- --------  Horizontal flip: read the data backwards if set
 *		 +8   --cccccc --------  Sprite color palette
 *		 +8   -------- zzzzzz--  Zoom factor
 *		 +8   -------- ------pp  Sprite priority
 *		 +E   dddddddd dddddddd  Scratch space for current address
 *
 *	Special notes:
 *
 *		There is an interaction between the horizonal flip bit and the offset.
 *		The offset is maintained as a 16-bit value, even though only the lower
 *		15 bits are used for the address. The top bit is used to control flipping.
 *		This means that if the low 15 bits overflow during rendering, the sprite
 *		data will be read backwards after the overflow. This is important to
 *		emulate correctly as many games make use of this feature to render sprites
 *		at the beginning of a bank.
 *
 *******************************************************************************************/

#define hangon_draw_pixel()													\
	/* only draw if onscreen, not 0 or 15 */								\
	if (pix != 0 && pix != 15)												\
	{																		\
		/* are we high enough priority to be visible? */					\
		if (sprpri > pri[x])												\
		{																	\
			/* shadow/hilight mode? */										\
			if (color == info->colorbase + (0x3f << 4))						\
				dest[x] += info->shadow ? palette.entries*2 : palette.entries;	\
																			\
			/* regular draw */												\
			else															\
				dest[x] = pix | color;										\
		}																	\
																			\
		/* always mark priority so no one else draws here */				\
		pri[x] = 0xff;														\
	}																		\

static void segaic16_sprites_hangon_draw(struct sprite_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x10000;
	const UINT16 *spritebase = (const UINT16 *)memory_region(REGION_GFX2);
	const UINT8 *zoom = (const UINT8 *)memory_region(REGION_PROMS);
	UINT16 *data;

	/* first scan forward to find the end of the list */
	for (data = info->spriteram; data < info->spriteram + info->ramsize/2; data += 8)
		if ((data[0] >> 8) > 0xf0)
			break;

	/* now scan backwards and render the sprites in order */
	for (data -= 8; data >= info->spriteram; data -= 8)
	{
		int bottom  = (data[0] >> 8) + 1;
		int top     = (data[0] & 0xff) + 1;
		int bank    = info->bank[(data[1] >> 12) & 0xf];
		int xpos    = (data[1] & 0x1ff) - 0xbd;
		int pitch   = (INT16)data[2];
		UINT16 addr = data[3];
		int color   = info->colorbase + (((data[4] >> 8) & 0x3f) << 4);
		int vzoom   = (data[4] >> 2) & 0x3f;
		int hzoom   = vzoom << 1;
		int sprpri  = 1 << ((data[4] >> 0) & 0x3);
		int x, y, pix, zaddr, zmask;
		const UINT16 *spritedata;

		/* initialize the end address to the start address */
		data[7] = addr;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if ((top >= bottom) || bank == 255)
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x8000 * bank;

		/* determine the starting zoom address and mask */
		zaddr = (vzoom & 0x38) << 5;
		zmask = 1 << (vzoom & 7);

		/* loop from top to bottom */
		for (y = top; y < bottom; y++)
		{
			/* advance a row */
			addr += pitch;

			/* if the zoom bit says so, add pitch a second time */
			if (zoom[zaddr++] & zmask)
				addr += pitch;

			/* skip drawing if not within the cliprect */
			if (y >= cliprect->min_y && y <= cliprect->max_y)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT8 *pri = (UINT8 *)priority_bitmap->line[y];
				int xacc = 0x00;

				/* note that the System 16A sprites have a design flaw that allows the address */
				/* to carry into the flip flag, which is the topmost bit -- it is very important */
				/* to emulate this as the games compensate for it */

				/* non-flipped case */
				if (!(addr & 0x8000))
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = xpos; x <= cliprect->max_x; )
					{
						UINT16 pixels = spritedata[++data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					data[7] = addr + 1;
					for (x = xpos; x <= cliprect->max_x; )
					{
						UINT16 pixels = spritedata[--data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) hangon_draw_pixel(); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
		}
	}
}



/*******************************************************************************************
 *
 *	Space Harrier-style sprites
 *
 *		Offs  Bits               Usage
 *		 +0   bbbbbbbb --------  Bottom scanline of sprite - 1
 *		 +0   -------- tttttttt  Top scanline of sprite - 1
 *		 +2   bbbb---- --------  Sprite bank
 *		 +2   -------x xxxxxxxx  X position of sprite (position $BD is screen position 0)
 *		 +4   s------- --------  Sprite shadow enable (0=enable, 1=disable)
 *		 +4   -p------ --------  Sprite priority
 *		 +4   --cccccc --------  Sprite color palette
 *		 +4   -------- -ppppppp  Signed 7-bit pitch value between scanlines
 *		 +6   f------- --------  Horizontal flip: read the data backwards if set
 *		 +6   -ooooooo oooooooo  Offset within selected sprite bank
 *		 +8   --zzzzzz --------  Horizontal zoom factor
 *		 +8   -------- --zzzzzz  Vertical zoom factor
 *		 +E   dddddddd dddddddd  Scratch space for current address
 *
 *	Special notes:
 *
 *		There is an interaction between the horizonal flip bit and the offset.
 *		The offset is maintained as a 16-bit value, even though only the lower
 *		15 bits are used for the address. The top bit is used to control flipping.
 *		This means that if the low 15 bits overflow during rendering, the sprite
 *		data will be read backwards after the overflow. This is important to
 *		emulate correctly as many games make use of this feature to render sprites
 *		at the beginning of a bank.
 *
 *******************************************************************************************/

#define sharrier_draw_pixel()												\
	/* only draw if onscreen, not 0 or 15 */								\
	if (pix != 0 && pix != 15)												\
	{																		\
		/* are we high enough priority to be visible? */					\
		if (sprpri > pri[x])												\
		{																	\
			/* shadow/hilight mode? */										\
			if (shadow && pix == 0xa)										\
				dest[x] += (paletteram16[dest[x]] & 0x8000) ? palette.entries*2 : palette.entries;	\
																			\
			/* regular draw */												\
			else															\
				dest[x] = pix | color;										\
		}																	\
																			\
		/* always mark priority so no one else draws here */				\
		pri[x] = 0xff;														\
	}																		\

static void segaic16_sprites_sharrier_draw(struct sprite_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x20000;
	const UINT32 *spritebase = (const UINT32 *)memory_region(REGION_GFX2);
	const UINT8 *zoom = (const UINT8 *)memory_region(REGION_PROMS);
	UINT16 *data;

	/* first scan forward to find the end of the list */
	for (data = info->spriteram; data < info->spriteram + info->ramsize/2; data += 8)
		if ((data[0] >> 8) > 0xf0)
			break;

	/* now scan backwards and render the sprites in order */
	for (data -= 8; data >= info->spriteram; data -= 8)
	{
		int bottom  = (data[0] >> 8) + 1;
		int top     = (data[0] & 0xff) + 1;
		int bank    = info->bank[(data[1] >> 12) & 0x7];
		int xpos    = (data[1] & 0x1ff) - 0xbd;
		int shadow  = (~data[2] >> 15) & 1;
		int sprpri  = ((data[2] >> 14) & 1) ? (1<<3) : (1<<1);
		int color   = info->colorbase + (((data[2] >> 8) & 0x3f) << 4);
		int pitch   = (INT16)(data[2] << 9) >> 9;
		UINT16 addr = data[3];
		int hzoom   = ((data[4] >> 8) & 0x3f) << 1;
		int vzoom   = (data[4] >> 0) & 0x3f;
		int x, y, pix, zaddr, zmask;
		const UINT32 *spritedata;

		/* initialize the end address to the start address */
		data[7] = addr;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if ((top >= bottom) || bank == 255)
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x8000 * bank;

		/* determine the starting zoom address and mask */
		zaddr = (vzoom & 0x38) << 5;
		zmask = 1 << (vzoom & 7);

		/* loop from top to bottom */
		for (y = top; y < bottom; y++)
		{
			/* advance a row */
			addr += pitch;

			/* if the zoom bit says so, add pitch a second time */
			if (zoom[zaddr++] & zmask)
				addr += pitch;

			/* skip drawing if not within the cliprect */
			if (y >= cliprect->min_y && y <= cliprect->max_y)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT8 *pri = (UINT8 *)priority_bitmap->line[y];
				int xacc = 0x00;

				/* note that the System 16A sprites have a design flaw that allows the address */
				/* to carry into the flip flag, which is the topmost bit -- it is very important */
				/* to emulate this as the games compensate for it */

				/* non-flipped case */
				if (!(addr & 0x8000))
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = xpos; x <= cliprect->max_x; )
					{
						UINT32 pixels = spritedata[++data[7] & 0x7fff];

						/* draw 8 pixels */
						pix = (pixels >> 28) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 24) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 20) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 16) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					data[7] = addr + 1;
					for (x = xpos; x <= cliprect->max_x; )
					{
						UINT32 pixels = spritedata[--data[7] & 0x7fff];

						/* draw 8 pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 16) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 20) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 24) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }
						pix = (pixels >> 28) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= cliprect->min_x) sharrier_draw_pixel(); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
		}
	}
}



/*******************************************************************************************
 *
 *	System 16A-style sprites
 *
 *		Offs  Bits               Usage
 *		 +0   bbbbbbbb --------  Bottom scanline of sprite - 1
 *		 +0   -------- tttttttt  Top scanline of sprite - 1
 *		 +2   -------x xxxxxxxx  X position of sprite (position $BD is screen position 0)
 *		 +4   pppppppp pppppppp  Signed 16-bit pitch value between scanlines
 *		 +6   -ooooooo oooooooo  Offset within selected sprite bank
 *		 +6   f------- --------  Horizontal flip: read the data backwards if set
 *		 +8   --cccccc --------  Sprite color palette
 *		 +8   -------- -bbb----  Sprite bank
 *		 +8   -------- ------pp  Sprite priority
 *		 +E   dddddddd dddddddd  Scratch space for current address
 *
 *	Special notes:
 *
 *		There is an interaction between the horizonal flip bit and the offset.
 *		The offset is maintained as a 16-bit value, even though only the lower
 *		15 bits are used for the address. The top bit is used to control flipping.
 *		This means that if the low 15 bits overflow during rendering, the sprite
 *		data will be read backwards after the overflow. This is important to
 *		emulate correctly as many games make use of this feature to render sprites
 *		at the beginning of a bank.
 *
 *******************************************************************************************/

#define system16a_draw_pixel()												\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= cliprect->min_x && x <= cliprect->max_x && pix != 0 && pix != 15) \
	{																		\
		/* are we high enough priority to be visible? */					\
		if (sprpri > pri[x])												\
		{																	\
			/* shadow/hilight mode? */										\
			if (color == info->colorbase + (0x3f << 4))						\
				dest[x] += (paletteram16[dest[x]] & 0x8000) ? palette.entries*2 : palette.entries;	\
																			\
			/* regular draw */												\
			else															\
				dest[x] = pix | color;										\
		}																	\
																			\
		/* always mark priority so no one else draws here */				\
		pri[x] = 0xff;														\
	}																		\

static void segaic16_sprites_16a_draw(struct sprite_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x10000;
	const UINT16 *spritebase = (const UINT16 *)memory_region(REGION_GFX2);
	UINT16 *data;

	/* first scan forward to find the end of the list */
	for (data = info->spriteram; data < info->spriteram + info->ramsize/2; data += 8)
		if ((data[0] >> 8) > 0xf0)
			break;

	/* now scan backwards and render the sprites in order */
	for (data -= 8; data >= info->spriteram; data -= 8)
	{
		int bottom  = (data[0] >> 8) + 1;
		int top     = (data[0] & 0xff) + 1;
		int xpos    = (data[1] & 0x1ff) - 0xbd;
		int pitch   = (INT16)data[2];
		UINT16 addr = data[3];
		int color   = info->colorbase + (((data[4] >> 8) & 0x3f) << 4);
		int bank    = info->bank[(data[4] >> 4) & 0x7];
		int sprpri  = 1 << ((data[4] >> 0) & 0x3);
		const UINT16 *spritedata;
		int x, y, pix, xdelta = 1;

		/* initialize the end address to the start address */
		data[7] = addr;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if ((top >= bottom) || bank == 255)
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x8000 * bank;

		/* adjust positions for screen flipping */
		if (info->flip)
		{
			int temp = top;
			top = 224 - bottom;
			bottom = 224 - temp;
			xpos = 320 - xpos;
			xdelta = -1;
		}

		/* loop from top to bottom */
		for (y = top; y < bottom; y++)
		{
			/* advance a row */
			addr += pitch;

			/* skip drawing if not within the cliprect */
			if (y >= cliprect->min_y && y <= cliprect->max_y)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT8 *pri = (UINT8 *)priority_bitmap->line[y];

				/* note that the System 16A sprites have a design flaw that allows the address */
				/* to carry into the flip flag, which is the topmost bit -- it is very important */
				/* to emulate this as the games compensate for it */

				/* non-flipped case */
				if (!(addr & 0x8000))
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; )
					{
						UINT16 pixels = spritedata[++data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; system16a_draw_pixel(); x += xdelta;
						pix = (pixels >>  8) & 0xf; system16a_draw_pixel(); x += xdelta;
						pix = (pixels >>  4) & 0xf; system16a_draw_pixel(); x += xdelta;
						pix = (pixels >>  0) & 0xf; system16a_draw_pixel(); x += xdelta;

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					data[7] = addr + 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; )
					{
						UINT16 pixels = spritedata[--data[7] & 0x7fff];

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; system16a_draw_pixel(); x += xdelta;
						pix = (pixels >>  4) & 0xf; system16a_draw_pixel(); x += xdelta;
						pix = (pixels >>  8) & 0xf; system16a_draw_pixel(); x += xdelta;
						pix = (pixels >> 12) & 0xf; system16a_draw_pixel(); x += xdelta;

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
		}
	}
}



/*******************************************************************************************
 *
 *	System 16B-style sprites
 *
 *		Offs  Bits               Usage
 *		 +0   bbbbbbbb --------  Bottom scanline of sprite - 1
 *		 +0   -------- tttttttt  Top scanline of sprite - 1
 *		 +2   -------x xxxxxxxx  X position of sprite (position $BD is screen position 0)
 *		 +4   e------- --------  Signify end of sprite list
 *		 +4   -h------ --------  Hide this sprite
 *		 +4   -------f --------  Horizontal flip: read the data backwards if set
 *		 +4   -------- pppppppp  Signed 8-bit pitch value between scanlines
 *		 +6   oooooooo oooooooo  Offset within selected sprite bank
 *		 +8   ----bbbb --------  Sprite bank
 *		 +8   -------- pp------  Sprite priority, relative to tilemaps
 *		 +8   -------- --cccccc  Sprite color palette
 *		 +A   ------vv vvv-----  Vertical zoom factor (0 = full size, 0x10 = half size)
 *		 +A   -------- ---hhhhh  Horizontal zoom factor (0 = full size, 0x10 = half size)
 *		 +E   dddddddd dddddddd  Scratch space for current address
 *
 *	Note that the zooming described below is 100% accurate to the real board.
 *
 *******************************************************************************************/

#define system16b_draw_pixel() 												\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= cliprect->min_x && x <= cliprect->max_x && pix != 0 && pix != 15) \
	{																		\
		/* are we high enough priority to be visible? */					\
		if (sprpri > pri[x])												\
		{																	\
			/* shadow/hilight mode? */										\
			if (color == info->colorbase + (0x3f << 4))						\
				dest[x] += (paletteram16[dest[x]] & 0x8000) ? palette.entries*2 : palette.entries;	\
																			\
			/* regular draw */												\
			else															\
				dest[x] = pix | color;										\
		}																	\
																			\
		/* always mark priority so no one else draws here */				\
		pri[x] = 0xff;														\
	}																		\

static void segaic16_sprites_16b_draw(struct sprite_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x20000;
	const UINT16 *spritebase = (const UINT16 *)memory_region(REGION_GFX2);
	UINT16 *data;

	/* first scan forward to find the end of the list */
	for (data = info->spriteram; data < info->spriteram + info->ramsize/2; data += 8)
		if (data[2] & 0x8000)
			break;

	/* now scan backwards and render the sprites in order */
	for (data -= 8; data >= info->spriteram; data -= 8)
	{
		int bottom  = data[0] >> 8;
		int top     = data[0] & 0xff;
		int xpos    = (data[1] & 0x1ff) - 0xb8;
		int hide    = data[2] & 0x4000;
		int flip    = data[2] & 0x100;
		int pitch   = (INT8)(data[2] & 0xff);
		UINT16 addr = data[3];
		int bank    = info->bank[(data[4] >> 8) & 0xf];
		int sprpri  = 1 << ((data[4] >> 6) & 0x3);
		int color   = info->colorbase + ((data[4] & 0x3f) << 4);
		int vzoom   = (data[5] >> 5) & 0x1f;
		int hzoom   = data[5] & 0x1f;
		const UINT16 *spritedata;
		int x, y, pix, xdelta = 1;

		/* initialize the end address to the start address */
		data[7] = addr;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || (top >= bottom) || bank == 255)
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* reset the yzoom counter */
		data[5] &= 0x03ff;

		/* adjust positions for screen flipping */
		if (info->flip)
		{
			int temp = top;
			top = 224 - bottom;
			bottom = 224 - temp;
			xpos = 320 - xpos;
			xdelta = -1;
		}

		/* loop from top to bottom */
		for (y = top; y < bottom; y++)
		{
			/* advance a row */
			addr += pitch;

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			data[5] += vzoom << 10;
			if (data[5] & 0x8000)
			{
				addr += pitch;
				data[5] &= ~0x8000;
			}

			/* skip drawing if not within the cliprect */
			if (y >= cliprect->min_y && y <= cliprect->max_y)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT8 *pri = (UINT8 *)priority_bitmap->line[y];
				int xacc;

				/* compute the initial X zoom accumulator; this is verified on the real PCB */
				xacc = 4 * hzoom;

				/* non-flipped case */
				if (!flip)
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; )
					{
						UINT16 pixels = spritedata[++data[7]];

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					data[7] = addr + 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; )
					{
						UINT16 pixels = spritedata[--data[7]];

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { system16b_draw_pixel(); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
		}
	}
}



/*******************************************************************************************
 *
 *	Out Run/X-Board-style sprites
 *
 *		Offs  Bits               Usage
 *		 +0   e------- --------  Signify end of sprite list
 *		 +0   -h-h---- --------  Hide this sprite if either bit is set
 *		 +0   ----bbb- --------  Sprite bank
 *		 +0   -------t tttttttt  Top scanline of sprite + 256
 *		 +2   oooooooo oooooooo  Offset within selected sprite bank
 *		 +4   ppppppp- --------  Signed 7-bit pitch value between scanlines
 *		 +4   -------x xxxxxxxx  X position of sprite (position $BE is screen position 0)
 *		 +6   -s------ --------  Enable shadows
 *		 +6   --pp---- --------  Sprite priority, relative to tilemaps
 *		 +6   ------vv vvvvvvvv  Vertical zoom factor (0x200 = full size, 0x100 = half size, 0x300 = 2x size)
 *		 +8   y------- --------  Render from top-to-bottom (1) or bottom-to-top (0) on screen
 *		 +8   -f------ --------  Horizontal flip: read the data backwards if set
 *		 +8   --x----- --------  Render from left-to-right (1) or right-to-left (0) on screen
 *		 +8   ------hh hhhhhhhh  Horizontal zoom factor (0x200 = full size, 0x100 = half size, 0x300 = 2x size)
 *		 +E   dddddddd dddddddd  Scratch space for current address
 *
 *	  Out Run only:
 *		 +A   hhhhhhhh --------  Height in scanlines - 1
 *		 +A   -------- -ccccccc  Sprite color palette
 *
 *	  X-Board only:
 *		 +A   ----hhhh hhhhhhhh  Height in scanlines - 1
 *		 +C   -------- cccccccc  Sprite color palette
 *
 *******************************************************************************************/

#define outrun_draw_pixel() 												\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= cliprect->min_x && x <= cliprect->max_x && pix != 0 && pix != 15) \
	{																		\
		/* are we high enough priority to be visible? */					\
		if (sprpri > pri[x])												\
		{																	\
			/* shadow/hilight mode? */										\
			if (shadow && pix == 0xa)										\
				dest[x] += (paletteram16[dest[x]] & 0x8000) ? palette.entries*2 : palette.entries;	\
																			\
			/* regular draw */												\
			else															\
				dest[x] = pix | color;										\
		}																	\
																			\
		/* always mark priority so no one else draws here */				\
		pri[x] = 0xff;														\
	}																		\

static void segaic16_sprites_outrun_draw(struct sprite_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x40000;
	const UINT32 *spritebase = (const UINT32 *)memory_region(REGION_GFX2);
	UINT16 *data;

	/* first scan forward to find the end of the list */
	for (data = info->buffer; data < info->buffer + info->ramsize/2; data += 8)
		if (data[0] & 0x8000)
			break;

	/* now scan backwards and render the sprites in order */
	for (data -= 8; data >= info->buffer; data -= 8)
	{
		int hide    = (data[0] & 0x5000);
		int bank    = (data[0] >> 9) & 7;
		int top     = (data[0] & 0x1ff) - 0x100;
		UINT16 addr = data[1];
		int pitch   = (INT16)data[2] >> 9;
		int xpos    = (data[2] & 0x1ff) - 0xbd;
		int shadow  = (data[3] >> 14) & 1;
		int sprpri  = 1 << ((data[3] >> 12) & 3);
		int vzoom   = data[3] & 0x3ff;
		int ydelta  = (data[4] & 0x8000) ? 1 : -1;
		int flip    = (~data[4] >> 14) & 1;
		int xdelta  = (data[4] & 0x2000) ? 1 : -1;
		int hzoom   = data[4] & 0x3ff;
		int height  = ((info->type == SEGAIC16_SPRITES_OUTRUN) ? (data[5] >> 8) : (data[5] & 0xfff)) + 1;
		int color   = info->colorbase + (((info->type == SEGAIC16_SPRITES_OUTRUN) ? (data[5] & 0x7f) : (data[6] & 0xff)) << 4);
		int x, y, ytarget, yacc = 0, pix;
		const UINT32 *spritedata;

		/* adjust X coordinate */
		/* note: if this threshhold is too close to 0, rachero will draw garbage */
		/* if it is too far from 0, smgp won't draw the bottom part of the road */
		if (xpos < -0x40 && xdelta < 0)
			xpos += 512;

		/* initialize the end address to the start address */
		data[7] = addr;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || height == 0)
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* clamp to a maximum of 8x (not 100% confirmed) */
		if (vzoom < 0x40) vzoom = 0x40;
		if (hzoom < 0x40) hzoom = 0x40;

		/* loop from top to bottom */
		ytarget = top + ydelta * height;
		for (y = top; y != ytarget; y += ydelta)
		{
			/* skip drawing if not within the cliprect */
			if (y >= cliprect->min_y && y <= cliprect->max_y)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT8 *pri = (UINT8 *)priority_bitmap->line[y];
				int xacc = 0;

				/* non-flipped case */
				if (!flip)
				{
					/* start at the word before because we preincrement below */
					data[7] = addr - 1;
					for (x = xpos; (xdelta > 0 && x <= cliprect->max_x) || (xdelta < 0 && x >= cliprect->min_x); )
					{
						UINT32 pixels = spritedata[++data[7]];

						/* draw four pixels */
						pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;

						/* stop if the second-to-last pixel in the group was 0xf */
						if ((pixels & 0x000000f0) == 0x000000f0)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					data[7] = addr + 1;
					for (x = xpos; (xdelta > 0 && x <= cliprect->max_x) || (xdelta < 0 && x >= cliprect->min_x); )
					{
						UINT32 pixels = spritedata[--data[7]];

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { outrun_draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;

						/* stop if the second-to-last pixel in the group was 0xf */
						if ((pixels & 0x0f000000) == 0x0f000000)
							break;
					}
				}
			}

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			yacc += vzoom;
			addr += pitch * (yacc >> 9);
			yacc &= 0x1ff;
		}
	}
}



/*************************************
 *
 *	General sprite initialization
 *
 *************************************/

int segaic16_sprites_init(int which, int type, int colorbase, int xoffs)
{
	struct sprite_info *info = &sprites[which];
	int i, buffer = 0;

	/* reset the sprite info */
	memset(info, 0, sizeof(*info));
	info->index = which;
	info->type = type;
	for (i = 0; i < 16; i++)
		info->bank[i] = i;
	info->colorbase = colorbase;
	info->xoffs = xoffs;

	/* set up based on which sprite system */
	switch (which)
	{
		case 0:
			info->spriteram = segaic16_spriteram_0;
			break;

		case 1:
			info->spriteram = segaic16_spriteram_1;
			break;

		default:
			mame_done();
	}

	/* determine the parameters of the sprites */
	switch (type)
	{
		case SEGAIC16_SPRITES_HANGON:
			info->draw = segaic16_sprites_hangon_draw;
			info->ramsize = 0x800;
			break;

		case SEGAIC16_SPRITES_16A:
			info->draw = segaic16_sprites_16a_draw;
			info->ramsize = 0x800;
			break;

		case SEGAIC16_SPRITES_16B:
			info->draw = segaic16_sprites_16b_draw;
			info->ramsize = 0x800;
			break;

		case SEGAIC16_SPRITES_SHARRIER:
			info->draw = segaic16_sprites_sharrier_draw;
			info->ramsize = 0x1000;
			break;

		case SEGAIC16_SPRITES_OUTRUN:
		case SEGAIC16_SPRITES_XBOARD:
			info->draw = segaic16_sprites_outrun_draw;
			info->ramsize = 0x1000;
			buffer = 1;
			break;

		default:
			mame_done();
	}

	/* if the sprites need buffering, allocate memory for the buffer */
	if (buffer)
	{
		info->buffer = auto_malloc(info->ramsize);
		if (!info->buffer)
			return 1;
	}
	return 0;
}



/*************************************
 *
 *	General sprite drawing
 *
 *************************************/

void segaic16_sprites_draw(int which, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	struct sprite_info *info = &sprites[which];
	(*info->draw)(info, bitmap, cliprect);
}



/*************************************
 *
 *	General sprite banking
 *
 *************************************/

void segaic16_sprites_set_bank(int which, int banknum, int offset)
{
	struct sprite_info *info = &sprites[which];

	if (info->bank[banknum] != offset)
	{
		force_partial_update(cpu_getscanline());
		info->bank[banknum] = offset;
	}
}



/*************************************
 *
 *	General sprite screen flipping
 *
 *************************************/

void segaic16_sprites_set_flip(int which, int flip)
{
	struct sprite_info *info = &sprites[which];

	flip = (flip != 0);
	if (info->flip != flip)
	{
		force_partial_update(cpu_getscanline());
		info->flip = flip;
	}
}



/*************************************
 *
 *	General sprite shadows
 *
 *************************************/

void segaic16_sprites_set_shadow(int which, int shadow)
{
	struct sprite_info *info = &sprites[which];

	shadow = (shadow != 0);
	if (info->shadow != shadow)
	{
		force_partial_update(cpu_getscanline());
		info->shadow = shadow;
	}
}



/*************************************
 *
 *	General sprite buffer control
 *
 *************************************/

static void segaic16_sprites_buffer(struct sprite_info *info)
{
	if (info->buffer)
	{
		UINT32 *src = (UINT32 *)info->spriteram;
		UINT32 *dst = (UINT32 *)info->buffer;
		int i;

		/* swap the halves of the sprite RAM */
		for (i = 0; i < info->ramsize/4; i++)
		{
			UINT32 temp = *src;
			*src++ = *dst;
			*dst++ = temp;
		}

		/* hack for thunderblade */
		*info->spriteram = 0xffff;
	}

	/* we will render the sprites when the video update happens */
}


WRITE16_HANDLER( segaic16_sprites_draw_0_w )
{
	segaic16_sprites_buffer(&sprites[0]);
}


WRITE16_HANDLER( segaic16_sprites_draw_1_w )
{
	segaic16_sprites_buffer(&sprites[1]);
}



/*******************************************************************************************
 *
 *	Hang On/Space Harrier-style road chip
 *
 *	Road RAM:
 *		Offset   Bits               Usage
 *		000-1FF  ----pp-- --------  road priority versus tilemaps and sprites
 *		         ------s- --------  (Hang On only) Stripe coloring enable (1=enable)
 *		         ------s- --------  (Space Harrier only) Solid color fill (1=solid, 0=from ROM)
 *		         -------m --------  mirror enable (1=enable)
 *		         -------- iiiiiiii  index for other tables
 *		         -------- rrrrrrrr  road ROM line select
 *		200-3FF  ----hhhh hhhhhhhh  horizontal scroll
 *		400-5FF  --bbbbbb --------  background color (colorset 0)
 *		         -------- --bbbbbb  background color (colorset 1)
 *		600-7FF  -------- s-------  stripe color index (colorset 1)
 *		         -------- -s------  stripe color index (colorset 0)
 *		         -------- --a-----  pixel value 2 color index (colorset 1)
 *		         -------- ---a----  pixel value 2 color index (colorset 0)
 *		         -------- ----b---  pixel value 1 color index (colorset 1)
 *		         -------- -----b--  pixel value 1 color index (colorset 0)
 *		         -------- ------c-  pixel value 0 color index (colorset 1)
 *		         -------- -------c  pixel value 0 color index (colorset 0)
 *
 *	Logic:
 *		First, the scanline is used to index into the table at 000-1FF
 *
 *		The index is taken from the low 8 bits of the table value from 000-1FF
 *
 *		The horizontal scroll value is looked up using the index in the table at
 *			200-3FF
 *
 *		The background color information is looked up using the index in the table at 400-5FF.
 *
 *		The pixel color information is looked up using the index in the table at 600-7FF.
 *
 *******************************************************************************************/

static void segaic16_road_hangon_decode(struct road_info *info)
{
	int x, y;

	/* allocate memory for the unpacked road data */
	info->gfx = auto_malloc(256 * 512);
	if (!info->gfx)
		mame_done();

	/* loop over rows */
	for (y = 0; y < 256; y++)
	{
		UINT8 *src = memory_region(REGION_GFX3) + ((y & 0xff) * 0x40) % memory_region_length(REGION_GFX3);
		UINT8 *dst = info->gfx + y * 512;

		/* loop over columns */
		for (x = 0; x < 512; x++)
			dst[x] = (((src[x/8] >> (~x & 7)) & 1) << 0) | (((src[x/8 + 0x4000] >> (~x & 7)) & 1) << 1);
	}
}


static void segaic16_road_hangon_draw(struct road_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority)
{
	UINT16 *roadram = info->roadram;
	int x, y;

	/* for debugging road issues */
	if (DEBUG_ROAD)
	{
		static int dx,dy;
		if (code_pressed(KEYCODE_J)) dx--;
		if (code_pressed(KEYCODE_L)) dx++;
		if (code_pressed(KEYCODE_I)) dy--;
		if (code_pressed(KEYCODE_K)) dy++;
		usrintf_showmessage("X=%d Y=%d", dx, dy);

		if (code_pressed(KEYCODE_D))
		{
			palette_set_color(0, 0, 0, 255);
			palette_set_color(1, 0, 255, 0);
			palette_set_color(2, 255, 0, 0);
			palette_set_color(3, 255, 0, 255);
			palette_set_color(7, 255, 255, 255);

			for (y = cliprect->min_y; y <= cliprect->max_y; y++)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT8 *src = info->gfx + ((y + dy) & 0xff) * 512;
				for (x = cliprect->min_x; x <= cliprect->max_x; x++)
					dest[x] = src[(x + dx) & 0x3ff];
			}
			return;
		}
		else
		{
			for (x = 0; x < 8; x++)
				segaic16_paletteram_w(x, paletteram16[x], 0);
		}
		info->xoffs = dx;
	}

	/* loop over scanlines */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *dest = (UINT16 *)bitmap->line[y];
		int control = roadram[0x000 + y];
		int hpos = roadram[0x100 + (control & 0xff)];
		int color0 = roadram[0x200 + (control & 0xff)];
		int color1 = roadram[0x300 + (control & 0xff)];
		int ff9j1, ff9j2, ctr9m, ctr9n9p, ctr9n9p_ena, ss8j, plycont;
		UINT8 *src;

		/* the PLYCONT signal controls the road layering */
		plycont = (control >> 10) & 3;

		/* skip layers we aren't supposed to be drawing */
		if ((plycont == 0 && priority != SEGAIC16_ROAD_BACKGROUND) ||
			(plycont != 0 && priority != SEGAIC16_ROAD_FOREGROUND))
			continue;

		/* compute the offset of the road graphics for this line */
		src = info->gfx + (0x000 + (control & 0xff)) * 512;

		/* initialize the 4-bit counter at 9M, which counts bits within each road byte */
		ctr9m = hpos & 7;

		/* initialize the two 4-bit counters at 9P (low) and 9N (high), which count road data bytes */
		ctr9n9p = (hpos >> 3) & 0xff;

		/* initialize the flip-flop at 9J (lower half), which controls the counting direction */
		ff9j1 = (hpos >> 11) & 1;

		/* initialize the flip-flop at 9J (upper half), which controls the background color */
		ff9j2 = 1;

		/* initialize the serial shifter at 8S, which delays several signals after we flip */
		ss8j = 0;

		/* draw this scanline from the beginning */
		for (x = -24; x <= cliprect->max_x; x++)
		{
			int md, color, select;

			/* ---- the following logic all happens constantly ---- */

			/* the enable is controlled by the value in the counter at 9M */
			ctr9n9p_ena = (ctr9m == 7);

			/* if we carried out of the 9P/9N counters, we will forcibly clear the flip-flop at 9J (lower half) */
			if ((ctr9n9p & 0xff) == 0xff)
				ff9j1 = 0;

			/* if the control word bit 8 is clear, we will forcibly set the flip-flop at 9J (lower half) */
			if (!(control & 0x100))
				ff9j1 = 1;

			/* for the Hang On/Super Hang On case only: if the control word bit 9 is clear, we will forcibly */
			/* set the flip-flip at 9J (upper half) */
			if (info->type == SEGAIC16_ROAD_HANGON && !(control & 0x200))
				ff9j2 = 1;

			/* ---- now process the pixel ---- */
			md = 3;

			/* the Space Harrier/Enduro Racer hardware has a tweak that maps the control word bit 9 to the */
			/* /CE line on the road ROM; use this to effectively disable the road data */
			if (info->type != SEGAIC16_ROAD_SHARRIER || !(control & 0x200))

				/* the /OE line on the road ROM is linked to the AND of bits 2 & 3 of the counter at 9N */
				if ((ctr9n9p & 0xc0) == 0xc0)
				{
					/* note that the pixel logic is hidden in a custom at 9S; this is just a guess */
					if (ss8j & 1)
						md = src[((ctr9n9p & 0x3f) << 3) | ctr9m];
					else
						md = src[((ctr9n9p & 0x3f) << 3) | (ctr9m ^ 7)];
				}

			/* "select" is a made-up signal that comes from bit 3 of the serial shifter and is */
			/* used in several places for color selection */
			select = (ss8j >> 3) & 1;

			/* check the flip-flop at 9J (upper half) to determine if we should use the background color; */
			/* the output of this is ANDed with M0 and M1 so it only affects pixels with a value of 3; */
			/* this is done by the AND gates at 9L and 7K */
			if (ff9j2 && md == 3)
			{
				/* in this case, the "select" signal is used to select which background color to use */
				/* since the color0 control word contains two selections */
				color = (color0 >> (select ? 0 : 8)) & 0x3f;
				color |= info->colorbase2;
			}

			/* if we're not using the background color, we select pixel data from an alternate path */
			else
			{
				/* the AND gates at 7L, 9K, and 7K clamp the pixel value to 0 if bit 7 of the color 1 */
				/* signal is 1 and if the pixel value is 3 (both M0 and M1 == 1) */
				if ((color1 & 0x80) && md == 3)
					md = 0;

				/* the pixel value plus the "select" line combine to form a mux into the low 8 bits of color1 */
				color = (color1 >> ((md << 1) | select)) & 1;

				/* this value becomes the low bit of the final color; the "select" line itself and the pixel */
				/* value form the other bits */
				color |= select << 3;
				color |= md << 1;
				color |= info->colorbase1;
			}

			/* write the pixel if we're past the minimum clip */
			if (x >= cliprect->min_x)
				dest[x] = color;

			/* ---- the following logic all happens on the 6M clock ---- */

			/* clock the counter at 9M */
			ctr9m = (ctr9m + 1) & 7;

			/* if enabled, clock on the two cascaded 4-bit counters at 9P and 9N */
			if (ctr9n9p_ena)
			{
				if (ff9j1)
					ctr9n9p++;
				else
					ctr9n9p--;
			}

			/* clock the flip-flop at 9J (upper half) */
			ff9j2 = !(!ff9j1 && (ss8j & 0x80));

			/* clock the serial shift register at 8J */
			ss8j = (ss8j << 1) | ff9j1;
		}
	}
}



/*******************************************************************************************
 *
 *	Out Run/X-Board-style road chip
 *
 *	Road control register:
 *		Bits               Usage
 *		-------- -----d--  Direct scanline mode (1) or indirect mode (0)
 *		-------- ------pp  Road enable/priorities:
 *		                      0 = road 0 only visible
 *		                      1 = both roads visible, road 0 has priority
 *		                      2 = both roads visible, road 1 has priority
 *		                      3 = road 1 only visible
 *
 *	Road RAM:
 *		Offset   Bits               Usage
 *		000-1FF  ----s--- --------  Road 0: Solid fill (1) or ROM fill
 *		         -------- -ccccccc  Road 0: Solid color (if solid fill)
 *		         -------i iiiiiiii  Road 0: Index for other tables (if in indirect mode)
 *		         -------r rrrrrrr-  Road 0: Road ROM line select
 *		200-3FF  ----s--- --------  Road 1: Solid fill (1) or ROM fill
 *		         -------- -ccccccc  Road 1: Solid color (if solid fill)
 *		         -------i iiiiiiii  Road 1: Index for other tables (if in indirect mode)
 *		         -------r rrrrrrr-  Road 1: Road ROM line select
 *		400-7FF  ----hhhh hhhhhhhh  Road 0: horizontal scroll
 *		800-BFF  ----hhhh hhhhhhhh  Road 1: horizontal scroll
 *		C00-FFF  ----bbbb --------  Background color index
 *		         -------- s-------  Road 1: stripe color index
 *		         -------- -a------  Road 1: pixel value 2 color index
 *		         -------- --b-----  Road 1: pixel value 1 color index
 *		         -------- ---c----  Road 1: pixel value 0 color index
 *		         -------- ----s---  Road 0: stripe color index
 *		         -------- -----a--  Road 0: pixel value 2 color index
 *		         -------- ------b-  Road 0: pixel value 1 color index
 *		         -------- -------c  Road 0: pixel value 0 color index
 *
 *	Logic:
 *		First, the scanline is used to index into the tables at 000-1FF/200-3FF
 *			- if solid fill, the background is filled with the specified color index
 *			- otherwise, the remaining tables are used
 *
 *		If indirect mode is selected, the index is taken from the low 9 bits of the
 *			table value from 000-1FF/200-3FF
 *		If direct scanline mode is selected, the index is set equal to the scanline
 *			for road 0, or the scanline + 256 for road 1
 *
 *		The horizontal scroll value is looked up using the index in the tables at
 *			400-7FF/800-BFF
 *
 *		The color information is looked up using the index in the table at C00-FFF. Note
 *			that the same table is used for both roads.
 *
 *******************************************************************************************/

static void segaic16_road_outrun_decode(struct road_info *info)
{
	int x, y;

	/* allocate memory for the unpacked road data */
	info->gfx = auto_malloc((256 * 2 + 1) * 512);
	if (!info->gfx)
		mame_done();
		
	/* loop over rows */
	for (y = 0; y < 256 * 2; y++)
	{
		UINT8 *src = memory_region(REGION_GFX3) + ((y & 0xff) * 0x40 + (y >> 8) * 0x8000) % memory_region_length(REGION_GFX3);
		UINT8 *dst = info->gfx + y * 512;

		/* loop over columns */
		for (x = 0; x < 512; x++)
		{
			dst[x] = (((src[x/8] >> (~x & 7)) & 1) << 0) | (((src[x/8 + 0x4000] >> (~x & 7)) & 1) << 1);

			/* pre-mark road data in the "stripe" area with a high bit */
			if (x >= 256-8 && x < 256 && dst[x] == 3)
				dst[x] |= 4;
		}
	}

	/* set up a dummy road in the last entry */
	memset(info->gfx + 256 * 2 * 512, 3, 512);
}


static void segaic16_road_outrun_draw(struct road_info *info, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority)
{
	UINT16 *roadram = info->roadram;
	int x, y;

	/* for debugging road issues */
	if (DEBUG_ROAD)
	{
		if (code_pressed(KEYCODE_D))
		{
			palette_set_color(info->colorbase1 ^ 0, 0, 0, 255);
			palette_set_color(info->colorbase1 ^ 1, 0, 0, 255);
			palette_set_color(info->colorbase1 ^ 2, 0, 255, 0);
			palette_set_color(info->colorbase1 ^ 3, 0, 255, 0);
			palette_set_color(info->colorbase1 ^ 4, 255, 0, 0);
			palette_set_color(info->colorbase1 ^ 5, 255, 0, 0);
			palette_set_color(info->colorbase1 ^ 6, 255, 255, 255);
			palette_set_color(info->colorbase1 ^ 7, 255, 255, 255);
			for (x = 0; x < 16; x++)
				palette_set_color(info->colorbase2 ^ x, 255, 0, 255);
		}
		else
		{
			for (x = 0; x < 8; x++)
				segaic16_paletteram_w(info->colorbase1 ^ x, paletteram16[info->colorbase1 ^ x], 0);
			for (x = 0; x < 16; x++)
				segaic16_paletteram_w(info->colorbase2 ^ x, paletteram16[info->colorbase2 ^ x], 0);
		}
	}

	/* loop over scanlines */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *dest = (UINT16 *)bitmap->line[y];
		int data0 = roadram[0x000 + y];
		int data1 = roadram[0x100 + y];

		/* background case: look for solid fill scanlines */
		if (priority == SEGAIC16_ROAD_BACKGROUND)
		{
			int color = -1;

			/* based on the info->control, we can figure out which sky to draw */
			switch (info->control & 3)
			{
				case 0:
					if (data0 & 0x800)
						color = data0 & 0x7f;
					break;

				case 1:
					if (data0 & 0x800)
						color = data0 & 0x7f;
					else if (data1 & 0x800)
						color = data1 & 0x7f;
					break;

				case 2:
					if (data1 & 0x800)
						color = data1 & 0x7f;
					else if (data0 & 0x800)
						color = data0 & 0x7f;
					break;

				case 3:
					if (data1 & 0x800)
						color = data1 & 0x7f;
					break;
			}

			/* fill the scanline with color */
			if (color != -1)
			{
				color |= info->colorbase3;
				for (x = cliprect->min_x; x <= cliprect->max_x; x++)
					dest[x] = color;
			}
		}

		/* foreground case: render from ROM */
		else
		{
			static const UINT8 priority_map[2][8] =
			{
				{ 0x80,0x81,0x81,0x83,0,0,0,0x00 },
				{ 0x81,0x87,0x87,0x8f,0,0,0,0x00 }
			};
			int hpos0, hpos1, color0, color1;
			int control = info->control & 3;
			UINT16 color_table[32];
			UINT8 *src0, *src1;
			UINT8 bgcolor;

			/* if both roads are low priority, skip */
			if ((data0 & 0x800) && (data1 & 0x800))
				continue;

			/* get road 0 data */
			src0 = (data0 & 0x800) ? info->gfx + 256 * 2 * 512 : (info->gfx + (0x000 + ((data0 >> 1) & 0xff)) * 512);
			hpos0 = (roadram[0x200 + ((info->control & 4) ? y : (data0 & 0x1ff))]) & 0xfff;
			color0 = roadram[0x600 + ((info->control & 4) ? y : (data0 & 0x1ff))];

			/* get road 1 data */
			src1 = (data1 & 0x800) ? info->gfx + 256 * 2 * 512 : (info->gfx + (0x100 + ((data1 >> 1) & 0xff)) * 512);
			hpos1 = (roadram[0x400 + ((info->control & 4) ? (0x100 + y) : (data1 & 0x1ff))]) & 0xfff;
			color1 = roadram[0x600 + ((info->control & 4) ? (0x100 + y) : (data1 & 0x1ff))];

			/* determine the 5 colors for road 0 */
			color_table[0x00] = info->colorbase1 ^ 0x00 ^ ((color0 >> 0) & 1);
			color_table[0x01] = info->colorbase1 ^ 0x02 ^ ((color0 >> 1) & 1);
			color_table[0x02] = info->colorbase1 ^ 0x04 ^ ((color0 >> 2) & 1);
			bgcolor = (color0 >> 8) & 0xf;
			color_table[0x03] = (data0 & 0x200) ? color_table[0x00] : (info->colorbase2 ^ 0x00 ^ bgcolor);
			color_table[0x07] = info->colorbase1 ^ 0x06 ^ ((color0 >> 3) & 1);

			/* determine the 5 colors for road 1 */
			color_table[0x10] = info->colorbase1 ^ 0x08 ^ ((color1 >> 4) & 1);
			color_table[0x11] = info->colorbase1 ^ 0x0a ^ ((color1 >> 5) & 1);
			color_table[0x12] = info->colorbase1 ^ 0x0c ^ ((color1 >> 6) & 1);
			bgcolor = (color1 >> 8) & 0xf;
			color_table[0x13] = (data1 & 0x200) ? color_table[0x10] : (info->colorbase2 ^ 0x10 ^ bgcolor);
			color_table[0x17] = info->colorbase1 ^ 0x0e ^ ((color1 >> 7) & 1);

			/* draw the road */
			switch (control)
			{
				case 0:
					if (data0 & 0x800)
						continue;
					hpos0 = (hpos0 - (0x5f8 + info->xoffs)) & 0xfff;
					for (x = cliprect->min_x; x <= cliprect->max_x; x++)
					{
						int pix0 = (hpos0 < 0x200) ? src0[hpos0] : 3;
						dest[x] = color_table[0x00 + pix0];
						hpos0 = (hpos0 + 1) & 0xfff;
					}
					break;

				case 1:
					hpos0 = (hpos0 - (0x5f8 + info->xoffs)) & 0xfff;
					hpos1 = (hpos1 - (0x5f8 + info->xoffs)) & 0xfff;
					for (x = cliprect->min_x; x <= cliprect->max_x; x++)
					{
						int pix0 = (hpos0 < 0x200) ? src0[hpos0] : 3;
						int pix1 = (hpos1 < 0x200) ? src1[hpos1] : 3;
						if ((priority_map[0][pix0] >> pix1) & 1)
							dest[x] = color_table[0x10 + pix1];
						else
							dest[x] = color_table[0x00 + pix0];
						hpos0 = (hpos0 + 1) & 0xfff;
						hpos1 = (hpos1 + 1) & 0xfff;
					}
					break;

				case 2:
					hpos0 = (hpos0 - (0x5f8 + info->xoffs)) & 0xfff;
					hpos1 = (hpos1 - (0x5f8 + info->xoffs)) & 0xfff;
					for (x = cliprect->min_x; x <= cliprect->max_x; x++)
					{
						int pix0 = (hpos0 < 0x200) ? src0[hpos0] : 3;
						int pix1 = (hpos1 < 0x200) ? src1[hpos1] : 3;
						if ((priority_map[1][pix0] >> pix1) & 1)
							dest[x] = color_table[0x10 + pix1];
						else
							dest[x] = color_table[0x00 + pix0];
						hpos0 = (hpos0 + 1) & 0xfff;
						hpos1 = (hpos1 + 1) & 0xfff;
					}
					break;

				case 3:
					if (data1 & 0x800)
						continue;
					hpos1 = (hpos1 - (0x5f8 + info->xoffs)) & 0xfff;
					for (x = cliprect->min_x; x <= cliprect->max_x; x++)
					{
						int pix1 = (hpos1 < 0x200) ? src1[hpos1] : 3;
						dest[x] = color_table[0x10 + pix1];
						hpos1 = (hpos1 + 1) & 0xfff;
					}
					break;
			}
		}
	}
}



/*************************************
 *
 *	General road initialization
 *
 *************************************/

int segaic16_road_init(int which, int type, int colorbase1, int colorbase2, int colorbase3, int xoffs)
{
	struct road_info *info = &road[which];

	/* reset the tilemap info */
	memset(info, 0, sizeof(*info));
	info->index = which;
	info->type = type;
	info->colorbase1 = colorbase1;
	info->colorbase2 = colorbase2;
	info->colorbase3 = colorbase3;
	info->xoffs = xoffs;

	/* set up based on which road generator */
	switch (which)
	{
		case 0:
			info->roadram = segaic16_roadram_0;
			break;

		default:
			mame_done();
	}

	/* determine the parameters of the road */
	switch (type)
	{
		case SEGAIC16_ROAD_HANGON:
		case SEGAIC16_ROAD_SHARRIER:
			info->draw = segaic16_road_hangon_draw;
			segaic16_road_hangon_decode(info);
			break;

		case SEGAIC16_ROAD_OUTRUN:
			info->draw = segaic16_road_outrun_draw;
			segaic16_road_outrun_decode(info);
			break;

		default:
			mame_done();
	}
	return 0;
}



/*************************************
 *
 *	General road drawing
 *
 *************************************/

void segaic16_road_draw(int which, struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority)
{
	struct road_info *info = &road[which];
	(*info->draw)(info, bitmap, cliprect, priority);
}



/*************************************
 *
 *	General road control read/write
 *
 *************************************/

READ16_HANDLER( segaic16_road_control_0_r )
{
	return 0xffff;
}


WRITE16_HANDLER( segaic16_road_control_0_w )
{
	if (ACCESSING_LSB)
	{
		road[0].control = data & 0xff;
		if (DEBUG_ROAD)
			printf("road_control = %02X\n", data & 0xff);
	}
}
