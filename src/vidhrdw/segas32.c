/*
    Information extracted from below, and from Modeler:

    Tile format:
        Bits               Usage
        y------- --------  Tile Y flip
        -x------ --------  Tile X flip
        --?----- --------  Unknown
        ---ccccc cccc----  Tile color palette
        ---nnnnn nnnnnnnn  Tile index

    Text format:
        Bits               Usage
        ccccccc- --------  Tile color palette
        -------n nnnnnnnn  Tile index

    Text RAM:
        Offset     Bits                  Usage
         $31FF00 : w--- ---- ---- ---- : Screen width (0= 320, 1= 412)
                   ---- f--- ---- ---- : Bitmap format (1= 8bpp, 0= 4bpp)
                   ---- -t-- ---- ---- : Tile banking related
                   ---- --f- ---- ---- : 1= All layers X+Y flip
                   ---- ---- ---- 4--- : 1= X+Y flip for NBG3
                   ---- ---- ---- -2-- : 1= X+Y flip for NBG2
                   ---- ---- ---- --1- : 1= X+Y flip for NBG1
                   ---- ---- ---- ---0 : 1= X+Y flip for NBG0
         $31FF02 : x--- ---- --x- ---- : Bitmap layer enable (?)
                   ---1 ---- ---- ---- : 1= NBG1 page wrapping disable
                   ---- 0--- ---- ---- : 1= NBG0 page wrapping disable
                   ---- ---- --b- ---- : 1= Bitmap layer disable
                   ---- ---- ---t ---- : 1= Text layer disable
                   ---- ---- ---- 3--- : 1= NBG3 layer disable
                   ---- ---- ---- -2-- : 1= NBG2 layer disable
                   ---- ---- ---- --1- : 1= NBG1 layer disable
                   ---- ---- ---- ---0 : 1= NBG0 layer disable

        F02      cccccccc --------  Per-layer clipping modes (see Modeler)
                 -------- ----d---  Disable tilemap layer 3
                 -------- -----d--  Disable tilemap layer 2
                 -------- ------d-  Disable tilemap layer 1
                 -------- -------d  Disable tilemap layer 0
        F04      tttttttt --------  Rowscroll/select table page number
                 -------- ----s---  Enable rowselect for tilemap layer 3
                 -------- -----s--  Enable rowselect for tilemap layer 2
                 -------- ------c-  Enable rowscroll for tilemap layer 3
                 -------- -------c  Enable rowscroll for tilemap layer 2
        F06      cccc---- --------  Layer 3 clip select
                 ----cccc --------  Layer 2 clip select
                 -------- cccc----  Layer 1 clip select
                 -------- ----cccc  Layer 0 clip select
        F12      ------xx xxxxxxxx  Layer 0 X scroll
        F16      -------y yyyyyyyy  Layer 0 Y scroll
        F1A      ------xx xxxxxxxx  Layer 1 X scroll
        F1E      -------y yyyyyyyy  Layer 1 Y scroll
        F22      ------xx xxxxxxxx  Layer 2 X scroll
        F26      -------y yyyyyyyy  Layer 2 Y scroll
        F2A      ------xx xxxxxxxx  Layer 3 X scroll
        F2E      -------y yyyyyyyy  Layer 3 Y scroll
        F30      ------xx xxxxxxxx  Layer 0 X offset (Modeler says X center)
        F32      -------y yyyyyyyy  Layer 0 Y offset (Modeler says Y center)
        F34      ------xx xxxxxxxx  Layer 1 X offset
        F36      -------y yyyyyyyy  Layer 1 Y offset
        F38      ------xx xxxxxxxx  Layer 2 X offset
        F3A      -------y yyyyyyyy  Layer 2 Y offset
        F3C      ------xx xxxxxxxx  Layer 3 X offset
        F3E      -------y yyyyyyyy  Layer 3 Y offset
        F40      -wwwwwww --------  Layer 0 upper-right page select
                 -------- -wwwwwww  Layer 0 upper-left page select
        F42      -wwwwwww --------  Layer 0 lower-right page select
                 -------- -wwwwwww  Layer 0 lower-left page select
        F44      -wwwwwww --------  Layer 1 upper-right page select
                 -------- -wwwwwww  Layer 1 upper-left page select
        F46      -wwwwwww --------  Layer 1 lower-right page select
                 -------- -wwwwwww  Layer 2 upper-left page select
        F48      -wwwwwww --------  Layer 2 upper-right page select
                 -------- -wwwwwww  Layer 2 lower-left page select
        F4A      -wwwwwww --------  Layer 2 lower-right page select
                 -------- -wwwwwww  Layer 3 upper-left page select
                 -wwwwwww --------  Layer 3 upper-right page select
        F4E      -------- -wwwwwww  Layer 3 lower-left page select
                 -wwwwwww --------  Layer 3 lower-right page select
        F50      xxxxxxxx xxxxxxxx  Layer 0 X step increment (0x200 = 1.0)
        F52      yyyyyyyy yyyyyyyy  Layer 0 Y step increment (0x200 = 1.0)
        F54      xxxxxxxx xxxxxxxx  Layer 1 X step increment (0x200 = 1.0)
        F56      yyyyyyyy yyyyyyyy  Layer 1 Y step increment (0x200 = 1.0)
        F58      xxxxxxxx xxxxxxxx  Layer 2 X step increment (0x200 = 1.0)
        F5A      yyyyyyyy yyyyyyyy  Layer 2 Y step increment (0x200 = 1.0)
        F5C      -------- tttt----  Text layer page select (page = 64 + t*8)
                 -------- -----bbb  Text layer tile bank

         $31FF5E : e--- ---- ---- ---- : Select backdrop color per 1= line, 0= screen
                   ---x xxxx ---- ---- : Affects color in screen mode
                   ---d dddd dddd dddd : Offset in CRAM for line mode

        F60      xxxxxxxx xxxxxxxx  Clip rect 0, left
        F62      yyyyyyyy yyyyyyyy  Clip rect 0, top
        F64      xxxxxxxx xxxxxxxx  Clip rect 0, right
        F66      yyyyyyyy yyyyyyyy  Clip rect 0, bottom
        F68      xxxxxxxx xxxxxxxx  Clip rect 1, left
        F6A      yyyyyyyy yyyyyyyy  Clip rect 1, top
        F6C      xxxxxxxx xxxxxxxx  Clip rect 1, right
        F6E      yyyyyyyy yyyyyyyy  Clip rect 1, bottom
        F70      xxxxxxxx xxxxxxxx  Clip rect 2, left
        F72      yyyyyyyy yyyyyyyy  Clip rect 2, top
        F74      xxxxxxxx xxxxxxxx  Clip rect 2, right
        F76      yyyyyyyy yyyyyyyy  Clip rect 2, bottom
        F78      xxxxxxxx xxxxxxxx  Clip rect 3, left
        F7A      yyyyyyyy yyyyyyyy  Clip rect 3, top
        F7C      xxxxxxxx xxxxxxxx  Clip rect 3, right
        F7E      yyyyyyyy yyyyyyyy  Clip rect 3, bottom

         $31FF88 : ---- ---x xxxx xxxx : Bitmap X scroll
         $31FF8A : ---- ---y yyyy yyyy : Bitmap Y scroll (bit 8 ONLY available when format is 4bpp)
         $31FF8C : ---- ---b bbbb b--- : Bitmap palette base? (bit 3 ONLY available when format is 4bpp)

         $31FF8E : ---- ---- --b- ---- : 1= Bitmap layer disable
                   ---- ---- ---3 ---- : 1= NBG3 layer disable
                   ---- ---- ---- 2--- : 1= NBG2 layer disable
                   ---- ---- ---- -1-- : 1= NBG1 layer disable
                   ---- ---- ---- --0- : 1= NBG0 layer disable
                   ---- ---- ---- ---t : 1= Text layer disable
*/

#include "driver.h"
#include "includes/segas32.h"

#define MONITOR 0
int system32_screen_mode;
int system32_screen_old_mode;
int system32_allow_high_resolution;


int sys32_tilebank_internal;
int sys32_old_tilebank_internal;
data16_t sys32_old_tilebank_external;

data32_t *multi32_videoram;
data8_t sys32_ramtile_dirty[0x1000];



/*************************************
 *
 *  Constants
 *
 *************************************/

#define MIXER_LAYER_TEXT		0
#define MIXER_LAYER_NBG0		1
#define MIXER_LAYER_NBG1		2
#define MIXER_LAYER_NBG2		3
#define MIXER_LAYER_NBG3		4
#define MIXER_LAYER_BITMAP		5
#define MIXER_LAYER_SPRITES		6
#define MIXER_LAYER_BACKGROUND	7



/*************************************
 *
 *  Type definitions
 *
 *************************************/

struct layer_info
{
	struct mame_bitmap *	bitmap;
	UINT16					trans_mask;
	UINT16 *				checksums;
	UINT16					checksum_mask;
};


struct extents_list
{
	UINT8 scan_extent[256];
	UINT16 extent[32][16];
};



/*************************************
 *
 *  Globals
 *
 *************************************/

data16_t *sys32_videoram;
UINT16 *system32_spriteram;
UINT16 *system32_paletteram[2];
static UINT16 mixer_control[2][0x40];

extern data16_t sys32_displayenable;
extern data16_t sys32_tilebank_external;

static struct tilemap *tilemap[0x80];
static struct layer_info layer_data[8];
static data16_t sprite_control[8];



/*************************************
 *
 *  Tilemap callback
 *
 *************************************/

static void get_tile_info(int tile_index)
{
	data16_t *vram = (data16_t *)tile_info.user_data;
	data16_t data = vram[tile_index];
	int tilebank1 = (sys32_videoram[0x1ff00/2] & 0x400) << 3;
	int tilebank2 = (sys32_tilebank_external & 1) << 14;
	SET_TILE_INFO(0, tilebank1 + tilebank2 + (data & 0x1fff), (data >> 4) & 0x1ff, (data >> 14) & 3);
}



/*************************************
 *
 *  Video start
 *
 *************************************/

VIDEO_START( system32 )
{
	int tmap;

	/* initialize videoram */
	sys32_videoram = auto_malloc ( 0x20000 );

	/* allocate the tilemaps */
	for (tmap = 0; tmap < 0x80; tmap++)
	{
		tilemap[tmap] = tilemap_create(get_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16,16, 32,16);
		tilemap_set_transparent_pen(tilemap[tmap], 0);
		tilemap_set_user_data(tilemap[tmap], &sys32_videoram[0x200 * tmap]);
	}

	/* allocate the bitmaps */
	for (tmap = 0; tmap < 7; tmap++)
	{
		layer_data[tmap].bitmap = auto_bitmap_alloc_depth(416, 224, 16);
		layer_data[tmap].checksums = auto_malloc(sizeof(layer_data[tmap].checksums[0]) * 256);
		memset(layer_data[tmap].checksums, 0, sizeof(layer_data[tmap].checksums[0]) * 256);
	}

	return 0;
}



/*************************************
 *
 *  Palette handling
 *
 *************************************/

static INLINE UINT16 xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(UINT16 value)
{
	int r = (value >> 0) & 0x1f;
	int g = (value >> 5) & 0x1f;
	int b = (value >> 10) & 0x1f;
	value = (value & 0x8000) | ((b & 0x01) << 14) | ((g & 0x01) << 13) | ((r & 0x01) << 12);
	value |= ((b & 0x1e) << 7) | ((g & 0x1e) << 3) | ((r & 0x1e) >> 1);
	return value;
}


static INLINE UINT16 xBGRBBBBGGGGRRRR_to_xBBBBBGGGGGRRRRR(UINT16 value)
{
	int r = ((value >> 12) & 0x01) | ((value << 1) & 0x1e);
	int g = ((value >> 13) & 0x01) | ((value >> 3) & 0x1e);
	int b = ((value >> 14) & 0x01) | ((value >> 7) & 0x1e);
	return (value & 0x8000) | (b << 10) | (g << 5) | (r << 0);
}


static INLINE void update_color(int offset, UINT16 data)
{
	/* note that since we use this RAM directly, we don't technically need */
	/* to call palette_set_color() at all; however, it does give us that */
	/* nice display when you hit F4, which is useful for debugging */

	/* set the color */
	palette_set_color(offset, pal5bit(data >> 0), pal5bit(data >> 5), pal5bit(data >> 10));
}


static INLINE UINT16 common_paletteram_r(int which, offs_t offset)
{
	int convert;

	/* the lower half of palette RAM is formatted xBBBBBGGGGGRRRRR */
	/* the upper half of palette RAM is formatted xBGRBBBBGGGGRRRR */
	/* we store everything if the first format, and convert accesses to the other format */
	/* on the fly */
	convert = (offset & 0x4000);
	offset &= 0x3fff;

	if (!convert)
		return system32_paletteram[which][offset];
	else
		return xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(system32_paletteram[which][offset]);
}


static void common_paletteram_w(int which, offs_t offset, UINT16 data, UINT16 mem_mask)
{
	UINT16 value;
	int convert;

	/* the lower half of palette RAM is formatted xBBBBBGGGGGRRRRR */
	/* the upper half of palette RAM is formatted xBGRBBBBGGGGRRRR */
	/* we store everything if the first format, and convert accesses to the other format */
	/* on the fly */
	convert = (offset & 0x4000);
	offset &= 0x3fff;

	/* read, modify, and write the new value, updating the palette */
	value = system32_paletteram[which][offset];
	if (convert) value = xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(value);
	COMBINE_DATA(&value);
	if (convert) value = xBGRBBBBGGGGRRRR_to_xBBBBBGGGGGRRRRR(value);
	system32_paletteram[which][offset] = value;
	update_color(0x4000*which + offset, value);

	/* if blending is enabled, writes go to both halves of palette RAM */
	if (mixer_control[which][0x4e/2] & 0x0880)
	{
		offset ^= 0x2000;

		/* read, modify, and write the new value, updating the palette */
		value = system32_paletteram[which][offset];
		if (convert) value = xBBBBBGGGGGRRRRR_to_xBGRBBBBGGGGRRRR(value);
		COMBINE_DATA(&value);
		if (convert) value = xBGRBBBBGGGGRRRR_to_xBBBBBGGGGGRRRRR(value);
		system32_paletteram[which][offset] = value;
		update_color(0x4000*which + offset, value);
	}
}


/*************************************
 *
 *  Palette RAM access
 *
 *************************************/

READ16_HANDLER( system32_paletteram_r )
{
	return common_paletteram_r(0, offset);
}


WRITE16_HANDLER( system32_paletteram_w )
{
	common_paletteram_w(0, offset, data, mem_mask);
}


READ16_HANDLER( multi32_paletteram_0_r )
{
	return common_paletteram_r(0, offset);
}


WRITE16_HANDLER( multi32_paletteram_0_w )
{
	common_paletteram_w(0, offset, data, mem_mask);
}


READ16_HANDLER( multi32_paletteram_1_r )
{
	return common_paletteram_r(1, offset);
}


WRITE16_HANDLER( multi32_paletteram_1_w )
{
	common_paletteram_w(1, offset, data, mem_mask);
}


/*************************************
 *
 *  Sprite RAM access
 *
 *************************************/

READ16_HANDLER( system32_spriteram_r )
{
	return system32_spriteram[offset];
}


WRITE16_HANDLER ( system32_spriteram_w )
{

	COMBINE_DATA(&system32_spriteram[offset]);
}


/*************************************
 *
 *  Video RAM access
 *
 *************************************/

READ16_HANDLER ( sys32_videoram_r )
{
	return sys32_videoram[offset];
}


WRITE16_HANDLER( sys32_videoram_w )
{
	data16_t old = sys32_videoram[offset];
	COMBINE_DATA(&sys32_videoram[offset]);

	/* if we are not in the control area, just update any affected tilemaps */
	if (offset < 0x1ff00/2)
		tilemap_mark_tile_dirty(tilemap[offset / 0x200], offset % 0x200);

	/* otherwise, we need process some things specially */
	else
	{
		/* switch based on the offset */
		switch (offset)
		{
			case 0x1ff00/2:
				/* if the tile banking changes, nuke it all */
				if ((old ^ sys32_videoram[offset]) & 0x0400)
				{
					force_partial_update(cpu_getscanline());
					tilemap_mark_all_tiles_dirty(NULL);
				}

				/* if the screen size changes, update */
				if ((old ^ sys32_videoram[offset]) & 0x8000)
				{
					if (sys32_videoram[offset] & 0x8000)
						set_visible_area(0, 52*8-1, 0, 28*8-1);
					else
						set_visible_area(0, 40*8-1, 0, 28*8-1);
				}
				break;
		}
	}
}



/*************************************
 *
 *  Sprite control registers
 *
 *************************************/

WRITE16_HANDLER( system32_sprite_control_w )
{
	COMBINE_DATA(&sprite_control[offset]);
}



/*************************************
 *
 *  Mixer control registers
 *
 *************************************/

READ16_HANDLER( system32_mixer_r )
{
	return mixer_control[0][offset];
}


WRITE16_HANDLER( system32_mixer_w )
{
	COMBINE_DATA(&mixer_control[0][offset]);
}


READ16_HANDLER( multi32_mixer_0_r )
{
	return mixer_control[0][offset];
}


READ16_HANDLER( multi32_mixer_1_r )
{
	return mixer_control[1][offset];
}


WRITE16_HANDLER( multi32_mixer_0_w )
{
	COMBINE_DATA(&mixer_control[0][offset]);
}


WRITE16_HANDLER( multi32_mixer_1_w )
{
	COMBINE_DATA(&mixer_control[1][offset]);
}



/*************************************
 *
 *  Clipping extents computation
 *
 *************************************/

static int compute_clipping_extents(int flip, int enable, int clipout, int clipmask, const struct rectangle *cliprect, struct extents_list *list)
{
	struct rectangle tempclip;
	struct rectangle clips[5];
	int sorted[5];
	int i, j, y;

	/* expand our cliprect to exclude the bottom-right */
	tempclip = *cliprect;
	tempclip.max_x++;
	tempclip.max_y++;

	/* create the 0th entry */
	list->extent[0][0] = tempclip.min_x;
	list->extent[0][1] = tempclip.max_x;

	/* simple case if not enabled */
	if (!enable)
	{
		memset(&list->scan_extent[tempclip.min_y], 0, sizeof(list->scan_extent[0]) * (tempclip.max_y - tempclip.min_y));
		return 1;
	}

	/* extract the from videoram into locals, and apply the cliprect */
	for (i = 0; i < 5; i++)
	{
		if (!flip)
		{
			clips[i].min_x = sys32_videoram[0x1ff60/2 + i * 4] & 0x1ff;
			clips[i].min_y = sys32_videoram[0x1ff62/2 + i * 4] & 0x0ff;
			clips[i].max_x = (sys32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1;
			clips[i].max_y = (sys32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1;
		}
		else
		{
			clips[i].max_x = (Machine->visible_area.max_x + 1) - (sys32_videoram[0x1ff60/2 + i * 4] & 0x1ff);
			clips[i].max_y = (Machine->visible_area.max_y + 1) - (sys32_videoram[0x1ff62/2 + i * 4] & 0x0ff);
			clips[i].min_x = (Machine->visible_area.max_x + 1) - ((sys32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1);
			clips[i].min_y = (Machine->visible_area.max_y + 1) - ((sys32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1);
		}
		sect_rect(&clips[i], &tempclip);
		sorted[i] = i;
	}

	/* bubble sort them by min_x */
	for (i = 0; i < 5; i++)
		for (j = i + 1; j < 5; j++)
			if (clips[sorted[i]].min_x > clips[sorted[j]].min_x) { int temp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = temp; }

	/* create all valid extent combinations */
	for (i = 1; i < 32; i++)
		if (i & clipmask)
		{
			UINT16 *extent = &list->extent[i][0];

			/* start off with an entry at tempclip.min_x */
			*extent++ = tempclip.min_x;

			/* loop in sorted order over extents */
			for (j = 0; j < 5; j++)
				if (i & (1 << sorted[j]))
				{
					const struct rectangle *cur = &clips[sorted[j]];

					/* see if this intersects our last extent */
					if (extent != &list->extent[i][1] && cur->min_x <= extent[-1])
					{
						if (cur->max_x > extent[-1])
							extent[-1] = cur->max_x;
					}

					/* otherwise, just append to the list */
					else
					{
						*extent++ = cur->min_x;
						*extent++ = cur->max_x;
					}
				}

			/* append an ending entry */
			*extent++ = tempclip.max_x;
		}

	/* loop over scanlines and build extents */
	for (y = tempclip.min_y; y < tempclip.max_y; y++)
	{
		int sect = 0;

		/* figure out all the clips that intersect this scanline */
		for (i = 0; i < 5; i++)
			if ((clipmask & (1 << i)) && y >= clips[i].min_y && y < clips[i].max_y)
				sect |= 1 << i;
		list->scan_extent[y] = sect;
	}

	return clipout;
}



/*************************************
 *
 *  Zooming tilemaps (NBG0/1)
 *
 *************************************/

static void update_tilemap_zoom(struct layer_info *layer, const struct rectangle *cliprect, int bgnum)
{
	int clipenable, clipout, clips, clipdraw_start;
	struct mame_bitmap *bitmap = layer->bitmap;
	UINT16 *checksums = layer->checksums;
	struct extents_list clip_extents;
	UINT32 srcx, srcx_start, srcy;
	UINT32 srcxstep, srcystep;
	int dstxstep, dstystep;
	int flip;
	int x, y;

	/* configure the layer */
	layer->trans_mask = layer->checksum_mask = 0x0f;

	/* determine if we're flipped */
	flip = ((sys32_videoram[0x1ff00/2] >> 9) ^ (sys32_videoram[0x1ff00/2] >> bgnum)) & 1;

	/* determine the clipping */
	clipenable = (sys32_videoram[0x1ff02/2] >> (11 + bgnum)) & 1;
	clipout = (sys32_videoram[0x1ff02/2] >> (6 + bgnum)) & 1;
	clips = (sys32_videoram[0x1ff06/2] >> (4 * bgnum)) & 0x0f;
	clipdraw_start = compute_clipping_extents(flip, clipenable, clipout, clips, cliprect, &clip_extents);

	/* extract the X/Y step values (these are in destination space!) */
	dstxstep = sys32_videoram[0x1ff50/2 + 2 * bgnum] & 0xfff;
	if (sys32_videoram[0x1ff00/2] & 0x4000)
		dstystep = sys32_videoram[0x1ff52/2 + 2 * bgnum] & 0xfff;
	else
		dstystep = dstxstep;

	/* clamp the zoom factors */
	if (dstxstep < 0x80)
		dstxstep = 0x80;
	if (dstystep < 0x80)
		dstystep = 0x80;

	/* compute high-precision reciprocals (in 12.20 format) */
	srcxstep = (0x200 << 20) / dstxstep;
	srcystep = (0x200 << 20) / dstystep;

	/* start with the fractional scroll offsets, in source coordinates */
	srcx_start = (sys32_videoram[0x1ff12/2 + 4 * bgnum] & 0x3ff) << 20;
	srcx_start += (sys32_videoram[0x1ff10/2 + 4 * bgnum] & 0xff00) << 4;
	srcy = (sys32_videoram[0x1ff16/2 + 4 * bgnum] & 0x1ff) << 20;
	srcy += (sys32_videoram[0x1ff14/2 + 4 * bgnum] & 0xfe00) << 4;

	/* then account for the destination center coordinates */
	srcx_start -= (sys32_videoram[0x1ff30/2 + 2 * bgnum] & 0x1ff) * srcxstep;
	srcy -= (sys32_videoram[0x1ff32/2 + 2 * bgnum] & 0x1ff) * srcystep;

	/* finally, account for destination top,left coordinates */
	srcx_start += cliprect->min_x * srcxstep;
	srcy += cliprect->min_y * srcystep;

	/* if we're flipped, simply adjust the start/step parameters */
	if (flip)
	{
		srcx_start += (Machine->visible_area.max_x - 2 * cliprect->min_x) * srcxstep;
		srcy += (Machine->visible_area.max_y - 2 * cliprect->min_y) * srcystep;
		srcxstep = -srcxstep;
		srcystep = -srcystep;
	}

	/* loop over the target rows */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		int clipdraw = clipdraw_start;
		UINT16 checksum = 0;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			UINT16 *dst = (UINT16 *)bitmap->line[y];
			UINT16 *src[2];
			UINT16 pages;

			/* look up the pages and get their source pixmaps */
			pages = sys32_videoram[0x1ff40/2 + 2 * bgnum + ((srcy >> 28) & 1)];
			src[0] = tilemap_get_pixmap(tilemap[(pages >> 0) & 0x7f])->line[(srcy >> 20) & 0xff];
			src[1] = tilemap_get_pixmap(tilemap[(pages >> 8) & 0x7f])->line[(srcy >> 20) & 0xff];

			/* loop over extents */
			srcx = srcx_start;
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					for (x = extents[0]; x < extents[1]; x++)
					{
						UINT16 pix = src[(srcx >> 29) & 1][(srcx >> 20) & 0x1ff];
						srcx += srcxstep;
						dst[x] = pix;
						checksum |= pix;
					}
				}

				/* otherwise, clear to zero */
				else
				{
					memset(&dst[extents[0]], 0, (extents[1] - extents[0]) * sizeof(dst[0]));
					srcx += srcxstep * (extents[1] - extents[0]);
				}

				/* stop at the end */
				if (extents[1] > cliprect->max_x)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}
		}

		/* advance in Y and update the checksum */
		srcy += srcystep;
		checksums[y] = checksum;
	}

	/* enable this code below to display zoom information */
#if 0
	if (dstxstep != 0x200 || dstystep != 0x200)
		usrintf_showmessage("Zoom=%03X,%03X  Cent=%03X,%03X", dstxstep, dstystep,
			sys32_videoram[0x1ff30/2 + 2 * bgnum],
			sys32_videoram[0x1ff32/2 + 2 * bgnum]);
#endif
}



/*************************************
 *
 *  Rowscroll/select tilemaps (NBG2/3)
 *
 *************************************/

static void update_tilemap_rowscroll(struct layer_info *layer, const struct rectangle *cliprect, int bgnum)
{
	int clipenable, clipout, clips, clipdraw_start;
	struct mame_bitmap *bitmap = layer->bitmap;
	UINT16 *checksums = layer->checksums;
	struct extents_list clip_extents;
	int rowscroll, rowselect;
	int xscroll, yscroll;
	UINT16 *table;
	int srcx, srcy;
	int flip;
	int x, y;

	/* configure the layer */
	layer->trans_mask = layer->checksum_mask = 0x0f;

	/* determine if we're flipped */
	flip = ((sys32_videoram[0x1ff00/2] >> 9) ^ (sys32_videoram[0x1ff00/2] >> bgnum)) & 1;

	/* determine the clipping */
	clipenable = (sys32_videoram[0x1ff02/2] >> (11 + bgnum)) & 1;
	clipout = (sys32_videoram[0x1ff02/2] >> (6 + bgnum)) & 1;
	clips = (sys32_videoram[0x1ff06/2] >> (4 * bgnum)) & 0x0f;
	clipdraw_start = compute_clipping_extents(flip, clipenable, clipout, clips, cliprect, &clip_extents);

	/* determine if row scroll and/or row select is enabled */
	rowscroll = (sys32_videoram[0x1ff04/2] >> (bgnum - 2)) & 1;
	rowselect = (sys32_videoram[0x1ff04/2] >> bgnum) & 1;
	if ((sys32_videoram[0x1ff04/2] >> (bgnum + 2)) & 1)
		rowscroll = rowselect = 0;

	/* get a pointer to the table */
	table = &sys32_videoram[(sys32_videoram[0x1ff04/2] >> 10) * 0x400];

	/* start with screen-wide X and Y scrolls */
	xscroll = (sys32_videoram[0x1ff12/2 + 4 * bgnum] & 0x3ff) - (sys32_videoram[0x1ff30/2 + 2 * bgnum] & 0x1ff);
	yscroll = (sys32_videoram[0x1ff16/2 + 4 * bgnum] & 0x1ff);

	/* render the tilemap into its bitmap */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		int clipdraw = clipdraw_start;
		UINT16 checksum = 0;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			UINT16 *dst = (UINT16 *)bitmap->line[y];
			UINT16 *src[2];
			UINT16 pages;
			int srcxstep = 1;

			/* get starting scroll values */
			srcx = cliprect->min_x + xscroll;
			srcy = yscroll + y;

			/* apply row scroll/select */
			if (rowscroll)
				srcx += table[0x000 + 0x100 * (bgnum - 2) + y] & 0x3ff;
			if (rowselect)
				srcy = table[0x200 + 0x100 * (bgnum - 2) + y] & 0x1ff;

			/* if we're flipped, simply adjust the start/step parameters */
			if (flip)
			{
				srcx += Machine->visible_area.max_x - 2 * cliprect->min_x;
				srcy += Machine->visible_area.max_y - 2 * y;
				srcxstep = -1;
			}

			/* look up the pages and get their source pixmaps */
			pages = sys32_videoram[0x1ff40/2 + 2 * bgnum + ((srcy >> 8) & 1)];
			src[0] = tilemap_get_pixmap(tilemap[(pages >> 0) & 0x7f])->line[srcy & 0xff];
			src[1] = tilemap_get_pixmap(tilemap[(pages >> 8) & 0x7f])->line[srcy & 0xff];

			/* loop over extents */
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					for (x = extents[0]; x < extents[1]; x++, srcx += srcxstep)
					{
						UINT16 pix = src[(srcx >> 9) & 1][srcx & 0x1ff];
						dst[x] = pix;
						checksum |= pix;
					}
				}

				/* otherwise, clear to zero */
				else
				{
					memset(&dst[extents[0]], 0, (extents[1] - extents[0]) * sizeof(dst[0]));
					srcx += extents[1] - extents[0];
				}

				/* stop at the end */
				if (extents[1] > cliprect->max_x)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}
		}

		/* update the checksum */
		checksums[y] = checksum;
	}

	/* enable this code below to display scroll information */
#if 0
	if (rowscroll || rowselect)
		usrintf_showmessage("Scroll=%d Select=%d  Table@%06X",
			rowscroll, rowselect, (sys32_videoram[0x1ff04/2] >> 10) * 0x800);
#endif
}



/*************************************
 *
 *  Text layer
 *
 *************************************/

static void update_tilemap_text(struct layer_info *layer, const struct rectangle *cliprect)
{
	struct mame_bitmap *bitmap = layer->bitmap;
	UINT16 *checksums = layer->checksums;
	UINT16 *tilebase;
	UINT16 *gfxbase;
	int startx, starty;
	int endx, endy;
	int x, y, iy;
	int flip;

	/* configure the layer */
	layer->trans_mask = 0x0f;
	layer->checksum_mask = 0xffff;

	/* determine if we're flipped */
	flip = (sys32_videoram[0x1ff00/2] >> 9) & 1;

	/* determine the base of the tilemap and graphics data */
	tilebase = &sys32_videoram[((sys32_videoram[0x1ff5c/2] >> 4) & 0x1f) * 0x800];
	gfxbase = &sys32_videoram[(sys32_videoram[0x1ff5c/2] & 7) * 0x2000];

	/* compute start/end tile numbers */
	startx = cliprect->min_x / 8;
	starty = cliprect->min_y / 8;
	endx = cliprect->max_x / 8;
	endy = cliprect->max_y / 8;

	/* reset checksums */
	memset(&checksums[starty * 8], 0, sizeof(checksums[0]) * 8 * (endy - starty + 1));

	/* loop over tiles */
	for (y = starty; y <= endy; y++)
		for (x = startx; x <= endx; x++)
		{
			UINT16 tile = tilebase[y * 64 + x];
			UINT16 *src = &gfxbase[(tile & 0x1ff) * 16];
			UINT16 color = (tile & 0xfe00) >> 5;

			/* non-flipped case */
			if (!flip)
			{
				UINT16 *dst = ((UINT16 *)bitmap->line[y * 8]) + x * 8;
				UINT16 *sums = &checksums[y * 8];

				/* loop over rows */
				for (iy = 0; iy < 8; iy++)
				{
					UINT16 pix = *src++;
					sums[iy] |= pix;
					dst[0] = ((pix >>  4) & 0x0f) + color;
					dst[1] = ((pix >>  0) & 0x0f) + color;
					dst[2] = ((pix >> 12) & 0x0f) + color;
					dst[3] = ((pix >>  8) & 0x0f) + color;

					pix = *src++;
					sums[iy] |= pix;
					dst[4] = ((pix >>  4) & 0x0f) + color;
					dst[5] = ((pix >>  0) & 0x0f) + color;
					dst[6] = ((pix >> 12) & 0x0f) + color;
					dst[7] = ((pix >>  8) & 0x0f) + color;

					dst += bitmap->rowpixels;
				}
			}

			/* flipped case */
			else
			{
				int effdstx = Machine->visible_area.max_x - x * 8;
				int effdsty = Machine->visible_area.max_y - y * 8;
				UINT16 *dst = ((UINT16 *)bitmap->line[effdsty]) + effdstx;
				UINT16 *sums = &checksums[effdsty];

				/* loop over rows */
				for (iy = 0; iy < 8; iy++)
				{
					UINT16 pix = *src++;
					sums[-iy] |= pix;
					dst[ 0] = ((pix >>  4) & 0x0f) + color;
					dst[-1] = ((pix >>  0) & 0x0f) + color;
					dst[-2] = ((pix >> 12) & 0x0f) + color;
					dst[-3] = ((pix >>  8) & 0x0f) + color;

					pix = *src++;
					sums[-iy] |= pix;
					dst[-4] = ((pix >>  4) & 0x0f) + color;
					dst[-5] = ((pix >>  0) & 0x0f) + color;
					dst[-6] = ((pix >> 12) & 0x0f) + color;
					dst[-7] = ((pix >>  8) & 0x0f) + color;

					dst -= bitmap->rowpixels;
				}
			}
		}
}



/*************************************
 *
 *  Bitmap layer
 *
 *************************************/

static void update_bitmap(struct layer_info *layer, const struct rectangle *cliprect)
{
	int clipenable, clipout, clips, clipdraw_start;
	struct mame_bitmap *bitmap = layer->bitmap;
	UINT16 *checksums = layer->checksums;
	struct extents_list clip_extents;
	int xscroll, yscroll;
	UINT16 color;
	int x, y;
	int bpp;

	/* configure the layer */
	bpp = (sys32_videoram[0x1ff00/2] & 0x0800) ? 8 : 4;
	layer->trans_mask = layer->checksum_mask = (1 << bpp) - 1;

	/* determine the clipping */
	clipenable = (sys32_videoram[0x1ff02/2] >> 15) & 1;
	clipout = (sys32_videoram[0x1ff02/2] >> 10) & 1;
	clips = 0x10;
	clipdraw_start = compute_clipping_extents(0, clipenable, clipout, clips, cliprect, &clip_extents);

	/* determine x/y scroll */
	xscroll = sys32_videoram[0x1ff88/2] & 0x1ff;
	yscroll = sys32_videoram[0x1ff8a/2] & 0x1ff;
	color = (sys32_videoram[0x1ff8c/2] << 4) & 0x1fff0 & ~layer->trans_mask;

	/* loop over target rows */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		int clipdraw = clipdraw_start;
		UINT16 checksum = 0;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			UINT16 *dst = (UINT16 *)bitmap->line[y];

			/* loop over extents */
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					/* 8bpp mode case */
					if (bpp == 8)
					{
						UINT8 *src = (UINT8 *)&sys32_videoram[512/2 * ((y + yscroll) & 0xff)];
						for (x = extents[0]; x < extents[1]; x++)
						{
							int effx = (x + xscroll) & 0x1ff;
							UINT16 pix = src[BYTE_XOR_LE(effx)] + color;
							dst[x] = pix;
							checksum |= pix;
						}
					}

					/* 4bpp mode case */
					else
					{
						UINT16 *src = &sys32_videoram[512/4 * ((y + yscroll) & 0x1ff)];
						for (x = extents[0]; x < extents[1]; x++)
						{
							int effx = (x + xscroll) & 0x1ff;
							UINT16 pix = ((src[effx / 4] >> (4 * (effx & 3))) & 0x0f) + color;
							dst[x] = pix;
							checksum |= pix;
						}
					}
				}

				/* otherwise, clear to zero */
				else
					memset(&dst[extents[0]], 0, (extents[1] - extents[0]) * sizeof(dst[0]));

				/* stop at the end */
				if (extents[1] > cliprect->max_x)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}
		}

		/* update the checksum */
		checksums[y] = checksum;
	}
}



/*************************************
 *
 *  Master tilemap chip updater
 *
 *************************************/

static UINT8 update_tilemaps(const struct rectangle *cliprect)
{
	int enable0 = !(sys32_videoram[0x1ff02/2] & 0x0001) && !(sys32_videoram[0x1ff8e/2] & 0x0002);
	int enable1 = !(sys32_videoram[0x1ff02/2] & 0x0002) && !(sys32_videoram[0x1ff8e/2] & 0x0004);
	int enable2 = !(sys32_videoram[0x1ff02/2] & 0x0004) && !(sys32_videoram[0x1ff8e/2] & 0x0008) && !(sys32_videoram[0x1ff00/2] & 0x1000);
	int enable3 = !(sys32_videoram[0x1ff02/2] & 0x0008) && !(sys32_videoram[0x1ff8e/2] & 0x0010) && !(sys32_videoram[0x1ff00/2] & 0x2000);
	int enablet = !(sys32_videoram[0x1ff02/2] & 0x0010) && !(sys32_videoram[0x1ff8e/2] & 0x0001);
	int enableb = !(sys32_videoram[0x1ff02/2] & 0x0020) && !(sys32_videoram[0x1ff8e/2] & 0x0020);

	/* update any tilemaps */
	if (enable0)
		update_tilemap_zoom(&layer_data[MIXER_LAYER_NBG0], cliprect, 0);
	if (enable1)
		update_tilemap_zoom(&layer_data[MIXER_LAYER_NBG1], cliprect, 1);
	if (enable2)
		update_tilemap_rowscroll(&layer_data[MIXER_LAYER_NBG2], cliprect, 2);
	if (enable3)
		update_tilemap_rowscroll(&layer_data[MIXER_LAYER_NBG3], cliprect, 3);
	if (enablet)
		update_tilemap_text(&layer_data[MIXER_LAYER_TEXT], cliprect);
	if (enableb)
		update_bitmap(&layer_data[MIXER_LAYER_BITMAP], cliprect);


	return (enablet << 0) | (enable0 << 1) | (enable1 << 2) | (enable2 << 3) | (enable3 << 4) | (enableb << 5);
}



/*************************************
 *
 *  Mixer layer render
 *
 *************************************/

#define FIVE_TO_EIGHT(x)	(((x) << 3) | ((x) >> 2))
static const UINT8 clamp_and_expand[32*3] =
{
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),
	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(0),

	FIVE_TO_EIGHT(0),	FIVE_TO_EIGHT(1),	FIVE_TO_EIGHT(2),	FIVE_TO_EIGHT(3),
	FIVE_TO_EIGHT(4),	FIVE_TO_EIGHT(5),	FIVE_TO_EIGHT(6),	FIVE_TO_EIGHT(7),
	FIVE_TO_EIGHT(8),	FIVE_TO_EIGHT(9),	FIVE_TO_EIGHT(10),	FIVE_TO_EIGHT(11),
	FIVE_TO_EIGHT(12),	FIVE_TO_EIGHT(13),	FIVE_TO_EIGHT(14),	FIVE_TO_EIGHT(15),
	FIVE_TO_EIGHT(16),	FIVE_TO_EIGHT(17),	FIVE_TO_EIGHT(18),	FIVE_TO_EIGHT(19),
	FIVE_TO_EIGHT(20),	FIVE_TO_EIGHT(21),	FIVE_TO_EIGHT(22),	FIVE_TO_EIGHT(23),
	FIVE_TO_EIGHT(24),	FIVE_TO_EIGHT(25),	FIVE_TO_EIGHT(26),	FIVE_TO_EIGHT(27),
	FIVE_TO_EIGHT(28),	FIVE_TO_EIGHT(29),	FIVE_TO_EIGHT(30),	FIVE_TO_EIGHT(31),

	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),
	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31),	FIVE_TO_EIGHT(31)
};


static void draw_bg_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	const UINT8 *rlookup, *glookup, *blookup;
	int shift, colselect, basecol;
	int x, y;

	/* determine the base palette entry and the shift value */
	basecol = (mixer_control[MONITOR][0x2c/2] & 0x00f0) << 6;
	shift = (mixer_control[MONITOR][0x2c/2] & 0x0300) >> 8;

	/* check the color select bit, and get pointers to the lookup tables */
	colselect = (mixer_control[MONITOR][0x3e/2] >> 14) & 1;
	rlookup = &((INT8)(mixer_control[MONITOR][0x40/2] << 2) >> 2);
	glookup = &((INT8)(mixer_control[MONITOR][0x42/2] << 2) >> 2);
	blookup = &((INT8)(mixer_control[MONITOR][0x44/2] << 2) >> 2);

	/* loop over rows with non-zero checksums */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT32 *dst = (UINT32 *)bitmap->line[y];
		UINT32 color;
		int r, g, b;

		/* look up the color index for this scanline */
		if (sys32_videoram[0x1ff5e/2] & 0x8000)
			color = (sys32_videoram[0x1ff5e/2] + y) & 0x1fff;
		else
			color = (sys32_videoram[0x1ff5e/2]) & 0x1f00;

		/* get the 5-5-5 color and convert to 8-8-8 */
		color = system32_paletteram[(basecol + (color & 0x0f) + ((color >> shift) & 0x1ff0)) & 0x3fff];
		r = rlookup[color & 0x1f];
		g = glookup[(color >> 5) & 0x1f];
		b = blookup[(color >> 10) & 0x1f];
		color = (r << 16) | (g << 8) | b;

		/* loop over columns */
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
			dst[x] = color;
	}
}


static void draw_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int layernum)
{
	struct mame_bitmap *srcbitmap = layer_data[layernum].bitmap;
	const UINT16 *checksums = layer_data[layernum].checksums;
	UINT16 checksum_mask = layer_data[layernum].checksum_mask;
	UINT16 trans_mask = layer_data[layernum].trans_mask;
	const UINT8 *rlookup, *glookup, *blookup;
	int shift, colselect, basecol;
	int x, y;

	/* determine the base palette entry and the shift value */
	basecol = (mixer_control[MONITOR][0x20/2 + layernum] & 0x00f0) << 6;
	shift = (mixer_control[MONITOR][0x20/2 + layernum] & 0x0300) >> 8;

	/* check the color select bit, and get pointers to the lookup tables */
	colselect = (mixer_control[MONITOR][0x30/2 + layernum] >> 14) & 1;
	rlookup = &clamp_and_expand[32 + ((INT8)(mixer_control[MONITOR][0x40/2 + colselect * 3] << 2) >> 2)];
	glookup = &clamp_and_expand[32 + ((INT8)(mixer_control[MONITOR][0x42/2 + colselect * 3] << 2) >> 2)];
	blookup = &clamp_and_expand[32 + ((INT8)(mixer_control[MONITOR][0x44/2 + colselect * 3] << 2) >> 2)];

	/* loop over rows with non-zero checksums */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		if (checksums[y] & checksum_mask)
		{
			UINT16 *src = (UINT16 *)srcbitmap->line[y];
			UINT32 *dst = (UINT32 *)bitmap->line[y];

			/* loop over columns */
			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
			{
				UINT16 pix = src[x];

				/* only draw non-transparent pixels */
				if (pix & trans_mask)
				{
					UINT16 color = system32_paletteram[(basecol + (pix & 0x0f) + ((pix >> shift) & 0x1ff0)) & 0x3fff];
					int r = rlookup[color & 0x1f];
					int g = glookup[(color >> 5) & 0x1f];
					int b = blookup[(color >> 10) & 0x1f];
					dst[x] = (r << 16) | (g << 8) | b;
				}
			}
		}
}


static void draw_sprite_layers(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT16 layermask)
{
	struct mame_bitmap *srcbitmap = layer_data[MIXER_LAYER_SPRITES].bitmap;
	const UINT16 *checksums = layer_data[MIXER_LAYER_SPRITES].checksums;
	UINT16 checksum_mask = layer_data[MIXER_LAYER_SPRITES].checksum_mask;
	const UINT8 *rlookup, *glookup, *blookup;
	int shift, colselect, basecol[16];
	int startx, cury, dx, dy;
	int x, y;

	/* based on the sprite controller flip bits, the data is scanned to us in different */
	/* directions; account for this */
	if (sprite_control[2] & 1)
	{
		dx = -1;
		startx = Machine->visible_area.max_x;
	}
	else
	{
		dx = 1;
		startx = Machine->visible_area.min_x;
	}

	if (sprite_control[2] & 2)
	{
		dy = -1;
		cury = Machine->visible_area.max_y - (cliprect->min_y - Machine->visible_area.min_y);
	}
	else
	{
		dy = 1;
		cury = cliprect->min_y;
	}

	/* determine the base palette entry and the shift value */
	for (x = 0; x < 16; x++)
		basecol[x] = (mixer_control[MONITOR][x] & 0x00f0) << 6;
	shift = (mixer_control[MONITOR][0x00/2] & 0x0300) >> 8;

	/* check the color select bit, and get pointers to the lookup tables */
	colselect = (mixer_control[MONITOR][0x4c/2] >> 14) & 1;
	rlookup = &clamp_and_expand[32 + ((INT8)(mixer_control[MONITOR][0x40/2 + colselect * 3] << 2) >> 2)];
	glookup = &clamp_and_expand[32 + ((INT8)(mixer_control[MONITOR][0x42/2 + colselect * 3] << 2) >> 2)];
	blookup = &clamp_and_expand[32 + ((INT8)(mixer_control[MONITOR][0x44/2 + colselect * 3] << 2) >> 2)];

	/* loop over rows with non-zero checksums */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++, cury += dy)
		if (checksums[cury] & checksum_mask)
		{
			UINT16 *src = (UINT16 *)srcbitmap->line[cury];
			UINT32 *dst = (UINT32 *)bitmap->line[y];
			int curx = startx;

			/* loop over columns */
			for (x = cliprect->min_x; x <= cliprect->max_x; x++, curx += dx)
			{
				UINT16 pix = src[curx];
				if (pix)
				{
					int layer;

					pix = (pix & 0x8000) | (((pix & 0x7fff) >> 0/*system32_mixerShift*/) & 0xfff0) | (pix & 0x0f);
					layer = (pix >> 11) & 0x0f;

					if (layermask & (1 << layer))
					{
						/* if the high bit is set, it's a shadow */
						if (pix & 0x8000)
							dst[x] = (dst[x] >> 1) & 0x7f7f7f;

						/* only draw non-transparent pixels */
						else
						{
							UINT16 color = system32_paletteram[(basecol[layer] + (pix & 0x07ff)) & 0x3fff];
							int r = rlookup[color & 0x1f];
							int g = glookup[(color >> 5) & 0x1f];
							int b = blookup[(color >> 10) & 0x1f];
							dst[x] = (r << 16) | (g << 8) | b;
						}
					}
				}
			}
		}
}




/*************************************
 *
 *  Sprite render
 *
 *************************************/

/*******************************************************************************************
 *
 *  System 32-style sprites
 *
 *      Offs  Bits               Usage
 *       +0   cc------ --------  Command (00=sprite, 01=clip, 02=jump, 03=end)
 *       +0   --i----- --------  Indirect palette enable
 *       +0   ---l---- --------  Indirect palette is inline in spriteram
 *       +0   ----s--- --------  Shadow sprite
 *       +0   -----r-- --------  Graphics from spriteram
 *       +0   ------8- --------  8bpp sprite
 *       +0   -------o --------  Opaque (no transparency)
 *       +0   -------- y-------  Flip Y
 *       +0   -------- -x------  Flip X
 *       +0   -------- --Y-----  Apply Y offset from last jump
 *       +0   -------- ---X----  Apply X offset from last jump
 *       +0   -------- ----aa--  Y alignment (00=center, 10=start, 01=end)
 *       +0   -------- ------AA  X alignment (00=center, 10=start, 01=end)
 *       +2   hhhhhhhh --------  Source data height
 *       +2   -------- wwwwwwww  Source data width
 *       +4   rrrr---- --------  Low bits of ROM bank
 *       +4   -----hhh hhhhhhhh  Onscreen height
 *       +6   -5--4--- --------  Bits 5 + 4 of ROM bank
 *       +6   -----www wwwwwwww  Onscreen width
 *       +8   ----yyyy yyyyyyyy  Y position
 *       +A   ----xxxx xxxxxxxx  X position
 *       +C   oooooooo oooooooo  Offset within selected sprite bank
 *       +E   -----ppp pppp----  Palette
 *       +E   -------- ----rrrr  Priority?
 *
 *******************************************************************************************/

#define sprite_draw_pixel_16() 												\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= cliprect->min_x && x <= cliprect->max_x && pix != 0 && pix != transp) \
	{																		\
		if (pix == 0x0e)													\
			dest[x] = 0x8000;												\
		else																\
			dest[x] = ind[pix] | color;										\
	}

#define sprite_draw_pixel_256()												\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= cliprect->min_x && x <= cliprect->max_x && pix != 0 && pix != transp) \
	{																		\
		if (pix == 0xf0)													\
			dest[x] = 0x8000;												\
		else																\
			dest[x] = (ind[0] | color) + pix;								\
	}

static int draw_one_sprite(UINT16 *data, int xoffs, int yoffs, const struct rectangle *cliprect)
{
	static const UINT16 non_indirect[256] =
	{
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
		0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
		0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
		0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
		0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
		0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
		0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
		0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
		0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
		0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
		0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
		0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
		0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
		0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
		0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
	};
	struct mame_bitmap *bitmap = layer_data[MIXER_LAYER_SPRITES].bitmap;
	UINT16 *checksums = layer_data[MIXER_LAYER_SPRITES].checksums;
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x40000;
	const UINT32 *spritebase = (const UINT32 *)memory_region(REGION_GFX2);

	{
		int shadow  = data[0] & 0x0800;
		int bpp8    = data[0] & 0x0200;
		int transp  = (data[0] & 0x0100) ? 0 : (bpp8 ? 0xe0 : 0x0f);
		int flipy   = data[0] & 0x0080;
		int flipx   = data[0] & 0x0040;
		int srch    = data[1] >> 8;
		int srcw    = data[1] & 0xff;
		int bank    = (data[2] >> 12) | ((data[3] & 0x0800) >> 7) | ((data[3] & 0x4000) >> 9);
		int dsth    = data[2] & 0x3ff;
		int dstw    = data[3] & 0x7ff;
		int ypos    = (INT16)(data[4] << 4) >> 4;
		int xpos    = (INT16)(data[5] << 4) >> 4;
		UINT16 addr = data[6];
		int color   = data[7] & 0x7ff0;
		int hzoom, vzoom;
		int xdelta = 1, ydelta = 1;
		int x, y, xtarget, ytarget, yacc = 0, pix;
		const UINT32 *spritedata;
		UINT16 addrmask, curaddr;
		const UINT16 *ind;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (srcw == 0 || srch == 0 || dstw == 0 || dsth == 0)
			goto bail;

		/* if we're shadowed, set the high bit of the color */
		if (shadow)
			color |= 0x8000;

		/* pick the indirection palette */
		if (data[0] & 0x2000)
		{
			ind = (data[0] & 0x1000) ? &data[8] : &system32_spriteram[8 * (data[7] & 0x1fff)];
			color = 0;
		}
		else
			ind = non_indirect;

		/* clamp to within the memory region size */
		if (data[0] & 0x0400)
		{
//          printf("RAM-sprite\n");
			goto bail;
//          spritedata = sys32_spriteram32;
			addrmask = (0x20000 / 8) - 1;
		}
		else
		{
			if (numbanks)
				bank %= numbanks;
			spritedata = spritebase + 0x10000 * bank;
			addrmask = 0xffff;
		}

		/* compute X/Y deltas */
		hzoom = ((4 * srcw) << 16) / dstw;
		vzoom = (srch << 16) / dsth;

		/* adjust the starting X position */
		if (data[0] & 0x10)
			xpos += xoffs;
		switch ((data[0] >> 0) & 3)
		{
			case 0:
			case 3:	xpos -= dstw / 2; 	break;
			case 1: xpos -= dstw - 1;	break;
			case 2:						break;
		}

		/* adjust the starting Y position */
		if (data[0] & 0x20)
			ypos += yoffs;
		switch ((data[0] >> 2) & 3)
		{
			case 0:
			case 3:	ypos -= dsth / 2; 	break;
			case 1: ypos -= dsth - 1;	break;
			case 2:						break;
		}

		/* adjust for flipping */
		if (flipx)
		{
			xpos += dstw - 1;
			xdelta = -1;
		}
		if (flipy)
		{
			ypos += dsth - 1;
			ydelta = -1;
		}

		/* if we're 4bpp, the pitch is half */
		if (!bpp8)
			srcw /= 2;

		/* compute target X,Y positions for loops */
		xtarget = xpos + xdelta * dstw;
		ytarget = ypos + ydelta * dsth;

		/* adjust target x for clipping */
		if (xdelta > 0 && xtarget > cliprect->max_x)
		{
			xtarget = cliprect->max_x;
			if (xpos >= xtarget)
				goto bail;
		}
		if (xdelta < 0 && xtarget < cliprect->min_x)
		{
			xtarget = cliprect->min_x;
			if (xpos <= xtarget)
				goto bail;
		}

		/* loop from top to bottom */
		for (y = ypos; y != ytarget; y += ydelta)
		{
			/* skip drawing if not within the cliprect */
			if (y >= cliprect->min_y && y <= cliprect->max_y)
			{
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				int xacc = 0;

				/* set a non-zero checksum for this row */
				checksums[y] = 0xffff;

				/* 4bpp case */
				if (!bpp8)
				{
					/* start at the word before because we preincrement below */
					curaddr = addr - 1;
					for (x = xpos; x != xtarget; )
					{
						UINT32 pixels = spritedata[++curaddr & addrmask];

						/* draw four pixels */
						pix = (pixels >> 28) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >>  0) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					}
				}

				/* 8bpp case */
				else
				{
					/* start at the word before because we preincrement below */
					curaddr = addr - 1;
					for (x = xpos; x != xtarget; )
					{
						UINT32 pixels = spritedata[++curaddr & addrmask];

						/* draw four pixels */
						pix = (pixels >> 24) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >> 16) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >>  8) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
						pix = (pixels >>  0) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					}
				}
			}

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			yacc += vzoom;
			addr += srcw * (yacc >> 16);
			yacc &= 0xffff;
		}
	}

	/* if we had an inline indirect palette, we skip two entries */
bail:
	return (data[0] & 0x1000) ? 16 : 0;
}



static void update_sprites(const struct rectangle *cliprect)
{
	struct rectangle curclip = *cliprect;
	int xoffs = 0, yoffs = 0;
	int numentries = 0;
	UINT16 *sprite;

	/* configure the layer */
	layer_data[MIXER_LAYER_SPRITES].trans_mask = 0xffff;
	layer_data[MIXER_LAYER_SPRITES].checksum_mask = 0xffff;

	/* zap the bitmap and checksums */
	fillbitmap(layer_data[MIXER_LAYER_SPRITES].bitmap, 0, cliprect);
	memset(layer_data[MIXER_LAYER_SPRITES].checksums, 0, sizeof(layer_data[MIXER_LAYER_SPRITES].checksums));

	logerror("----\n");

	/* now draw */
	sprite = &system32_spriteram[0];
	while (numentries++ < 0x20000/16)
	{
		/* top two bits are a command */
		switch (sprite[0] >> 14)
		{
			/* command 0 = draw sprite */
			case 0:
				logerror("Sprite draw: %04X %04X %04X %04X - %04X %04X %04X %04X\n",
					sprite[0], sprite[1], sprite[2], sprite[3], sprite[4], sprite[5], sprite[6], sprite[7]);

				sprite += 8 + draw_one_sprite(sprite, xoffs, yoffs, &curclip);
				break;

			/* command 1 = set clipping */
			case 1:
				logerror("Sprite clip: %04X %04X %04X %04X - %04X %04X %04X %04X\n",
					sprite[0], sprite[1], sprite[2], sprite[3], sprite[4], sprite[5], sprite[6], sprite[7]);

				/* suspicion: there are four clip registers that can be set independently */
				if (sprite[0] & 0x3000)
				{
					curclip.min_y = sprite[0] & 0xfff;
					curclip.max_y = sprite[1] & 0xfff;
					curclip.min_x = sprite[2] & 0xfff;
					curclip.max_x = sprite[3] & 0xfff;
					sect_rect(&curclip, cliprect);
				}
				else
					curclip = *cliprect;

				/* advance to the next entry */
				sprite += 8;
				break;

			/* command 2 = jump to position, and set X offset */
			case 2:
				logerror("Sprite jump: %04X %04X %04X %04X - %04X %04X %04X %04X\n",
					sprite[0], sprite[1], sprite[2], sprite[3], sprite[4], sprite[5], sprite[6], sprite[7]);

				if (sprite[0] & 0x2000)
				{
					yoffs = sprite[1];
					xoffs = sprite[2];
				}
				sprite = &system32_spriteram[8 * (sprite[0] & 0x1fff)];
				break;

			/* command 3 = done */
			case 3:
				logerror("Sprite done: %04X %04X %04X %04X - %04X %04X %04X %04X\n",
					sprite[0], sprite[1], sprite[2], sprite[3], sprite[4], sprite[5], sprite[6], sprite[7]);
				numentries = 0x20000/16;
				break;
		}
	}
}



/*************************************
 *
 *  Master update routine
 *
 *************************************/

VIDEO_UPDATE( system32 )
{
	UINT16 sprite_layers;
	UINT8 enablemask;
	int priority;

	/* if the display is off, punt */
	if (!sys32_displayenable)
	{
		fillbitmap(bitmap, get_black_pen(), cliprect);
		return;
  }

	/* update the tilemaps */
	enablemask = update_tilemaps(cliprect);

	/* update the sprites */
	update_sprites(cliprect);

	/* debugging */
/*
    usrintf_showmessage("en=%d%d%d%d%d%d col=%04X+%04X",
        (enablemask >> 0) & 1,
        (enablemask >> 1) & 1,
        (enablemask >> 2) & 1,
        (enablemask >> 3) & 1,
        (enablemask >> 4) & 1,
        (enablemask >> 5) & 1,
        sys32_videoram[0x1ff5e/2], mixer_control[0x2c/2]);
*/
#ifdef MAME_DEBUG
	if (code_pressed(KEYCODE_Q)) enablemask = 0x01;
	if (code_pressed(KEYCODE_W)) enablemask = 0x02;
	if (code_pressed(KEYCODE_E)) enablemask = 0x04;
	if (code_pressed(KEYCODE_R)) enablemask = 0x08;
	if (code_pressed(KEYCODE_T)) enablemask = 0x10;
	if (code_pressed(KEYCODE_Y)) enablemask = 0x20;
#endif

	/* fill the background */
	draw_bg_layer(bitmap, cliprect);

	/* crude mixing */
	sprite_layers = 0;
	for (priority = 1; priority < 16; priority++)
	{
		int splayer;

		for (splayer = 0; splayer < 16; splayer++)
			if (priority == (mixer_control[MONITOR][splayer] & 0x000f))
				sprite_layers |= 1 << splayer;

		if ((enablemask & 0x01) && priority == (mixer_control[MONITOR][0x20/2] & 0x000f))
		{
			if (sprite_layers)
				draw_sprite_layers(bitmap, cliprect, sprite_layers);
			sprite_layers = 0;
			draw_layer(bitmap, cliprect, MIXER_LAYER_TEXT);
		}

		if ((enablemask & 0x02) && priority == (mixer_control[MONITOR][0x22/2] & 0x000f))
		{
			if (sprite_layers)
				draw_sprite_layers(bitmap, cliprect, sprite_layers);
			sprite_layers = 0;
			draw_layer(bitmap, cliprect, MIXER_LAYER_NBG0);
		}

		if ((enablemask & 0x04) && priority == (mixer_control[MONITOR][0x24/2] & 0x000f))
		{
			if (sprite_layers)
				draw_sprite_layers(bitmap, cliprect, sprite_layers);
			sprite_layers = 0;
			draw_layer(bitmap, cliprect, MIXER_LAYER_NBG1);
		}

		if ((enablemask & 0x08) && priority == (mixer_control[MONITOR][0x26/2] & 0x000f))
		{
			if (sprite_layers)
				draw_sprite_layers(bitmap, cliprect, sprite_layers);
			sprite_layers = 0;
			draw_layer(bitmap, cliprect, MIXER_LAYER_NBG2);
		}

		if ((enablemask & 0x10) && priority == (mixer_control[MONITOR][0x28/2] & 0x000f))
		{
			if (sprite_layers)
				draw_sprite_layers(bitmap, cliprect, sprite_layers);
			sprite_layers = 0;
			draw_layer(bitmap, cliprect, MIXER_LAYER_NBG3);
		}

		if ((enablemask & 0x20) && priority == (mixer_control[MONITOR][0x2a/2] & 0x000f))
		{
			if (sprite_layers)
				draw_sprite_layers(bitmap, cliprect, sprite_layers);
			sprite_layers = 0;
			draw_layer(bitmap, cliprect, MIXER_LAYER_BITMAP);
		}
	}

	if (sprite_layers)
		draw_sprite_layers(bitmap, cliprect, sprite_layers);
}

/*
arescue:
SC: 0003 0000 0000 0002 - 0002 0003 0000 0000
00: 0011 0014 0018 001F - 0014 0015 0016 0017 - 0018 0019 001A 001B - 001C 001D 001E 001F
20: 000E 0141 0142 014E - 0146 000F 0000 0000 - 4000 4000 5008 5008 - 4000 4000 4000 4000
40: 0040 0040 0040 0000 - 0000 0000 BE4D 0C00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

alien3:
SC: 0000 0000 0000 0000 - 0001 0003 0000 0000
00: 0011 0017 001B 001F - 0014 0015 0016 0017 - 0018 0019 001A 001B - 001C 001D 001E 001F
20: 000E 014C 014A 0147 - 0144 000B 0000 0000 - 0000 0000 1008 103C - 0000 4000 0000 0000
40: FFEB FFEB FFEB 0000 - 0000 0000 BE4D 0C00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

arabfgt:
SC: 0000 0000 0000 0000 - 0000 0000 0000 0000
00: 000E 000A 0008 0006 - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 036D 036F 0366 - 0367 004F 0051 0050 - 0000 4000 57B0 4000 - 4000 0000 0000 4000
40: 0000 0000 0000 0000 - 0000 0000 8049 0F00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

brival:
SC: 0000 0000 0000 0000 - 0000 0000 0001 0000
00: 000E 000A 0008 0006 - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 036D 036F 036C - 0367 004F 0051 0050 - 0000 4000 7FF0 4000 - 4000 0000 0000 4000
40: 0000 0000 0000 0000 - 0000 0000 8049 0B00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

darkedge:
SC: 0000 0000 0000 0000 - 0001 0003 0000 0000
00: 000E 000C 0008 000E - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 025D 025B 0259 - 0257 007E 0071 0070 - 4000 0000 0000 0000 - 0000 0000 0000 0000
40: 0020 0020 0020 0020 - 0020 0020 3E4D 0C00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

dbzvrvs:
SC: 0000 0000 0000 0000 - 0001 0000 0000 0000
00: 000D 000B 0009 000F - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007E 036C 036C 036A - 036A 007C 0073 0072 - 4000 0000 0000 0000 - 0000 0000 0000 0000
40: 0000 0000 0000 0000 - 0000 0000 3E01 0300 - 0000 0000 0000 0000 - 0000 0000 0000 0000

f1en:
SC: 0000 0000 0000 0000 - 0000 0000 0000 0000
00: 000E 000A 0008 0006 - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 0361 0361 0361 - 036F 0041 0051 0050 - 4000 4000 4300 571B - 5216 0000 0000 0000
40: 0000 0000 0000 0000 - 0000 0000 3E49 0021 - 0000 0000 0000 0000 - 0000 0000 0000 0000

ga2j: - 8 priorities + shadow
SC: 0000 0000 0000 0000 - 0002 0003 0000 0000
00: 000F 000D 000B 0009 - 0007 0007 0005 0003 - 000F 000D 000B 0009 - 0007 0007 0005 0003
20: 003E 014E 0142 014C - 0148 003E 0030 0030 - 4000 5119 4099 4000 - 4000 4000 4000 C000
40: 0000 0000 0000 0000 - 0000 0000 924E 0B00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

holo:
SC: 0003 0000 0000 0000 - 0002 0003 0000 0000
00: 0011 0014 0018 001F - 0014 0015 0016 0017 - 0018 0019 001A 001B - 001C 001D 001E 001F
20: 000F 0146 0143 014E - 014E 000E 0000 0000 - 4000 4000 5008 5008 - 4000 4000 4000 C000
40: FF00 FF00 FF00 0000 - 0000 0000 BE4D 0C00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

jpark:
SC: 0000 0000 0000 0000 - 0000 0003 0000 0000
00: 000A 000D 0007 0006 - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 0368 036B 036E - 0369 007F 0077 0076 - 0000 4100 4200 15B0 - 0100 0000 0000 8010
40: 0000 0000 0000 0000 - 0000 0000 9E4C 0B00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

radm: - 8 priorities + shadow
SC: 0000 0000 0000 0000 - 0001 0002 0000 0000
00: 000E 000E 000A 0008 - 0006 0004 0002 0000 - 000E 000C 000A 0008 - 0006 0004 0002 0000
20: 007F 0367 0369 036B - 036D 0071 0071 0070 - 4000 100E 100E 100E - 100E 0000 0000 0000
40: 0000 0000 0000 0000 - 0000 0000 3E49 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000

radr: - 4 priorities
SC: 0000 0000 0000 0000 - 0001 0002 0000 0000
00: 000E 000A 0008 0006 - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 036E 036C 036D - 0367 0078 0071 0070 - 4000 4000 4300 571B - 5216 0000 0000 0000
40: 0000 0000 0000 0000 - 0000 0000 3E49 0B00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

sonic: - 4 priorities
SC: 0000 0000 0000 0000 - 0001 0002 0000 0000
00: 003E 003B 003C 0036 - 0030 0030 0030 0030 - 0030 0030 0030 0030 - 0030 0030 0030 0030
20: 002F 020E 020B 0208 - 0207 0021 0021 0020 - 0000 400B 4300 471B - 4216 0000 0000 0000
40: 0000 0000 0000 0000 - 0000 0000 3E49 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000

spidman: - 8 priorities + shadow
SC: 0000 0000 0000 0000 - 0002 0003 0000 0000
00: 000E 000C 000A 0008 - 0006 0004 0002 0000 - 000E 000C 000A 0008 - 0006 0004 0002 0000
20: 003F 0149 0149 014C - 0140 003E 0030 0030 - 4000 4000 4000 4000 - 4000 4000 4000 C000
40: 0000 0000 0000 0000 - 0000 0000 BE4E 0F00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

svf: - 4 priorities
SC: 0003 0000 0000 0002 - 0002 0003 0000 0000
00: 0014 0012 0018 001E - 0014 0015 0016 0017 - 0018 0019 001A 001B - 001C 001D 001E 001E
20: 000F 0143 0141 0142 - 0142 000E 0000 0000 - 4000 4000 5008 5008 - 5008 4000 4000 C001
40: 00FF 00FF 00FF 0000 - 0000 0000 BE4D 0900 - 0000 0000 0000 0000 - 0000 0000 0000 0000

*/
