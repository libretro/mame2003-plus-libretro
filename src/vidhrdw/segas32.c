/*
    Open questions:

	- In f1en, the scrolling text in attract mode is very jumpy. Whatever
	  double buffering they are using seems to be out of sync with the sprite
	  rendering.

	- In radr, NBG1 should be opaque on select screen, and NBG3 should be
	  opaque while driving. 
	  This is controlled by register $31ff8e (respectively $200 and $800), 
	  likewise darkedge sets $800 on the first attract fight 
	  (which has ugly black pens which should be white according to the ref). 
	  harddunk sets $0f00 which completely breaks text display if current 
	  hookup is enabled. 
	  The theory is that opaque pens should go above background layer and 
	  behind everything else like System 24.

	- radr uses $1A0 as the X center for zooming; however, this
	  contradicts the theory that bit 9 is a sign bit. For now, the code
	  assumes that the X center has 10 bits of resolution. 

	- In svf (the field) and radr (on the field), they use tilemap-specific
	  flip in conjunction with rowscroll AND rowselect. According to Charles,
	  in this case, the rowselect lookups should be done in reverse order,
	  but this results in an incorrect display. For now, we assume there is
	  a bug in the procedure and implement it so that it looks correct.

	- titlef NBG0 and NBG2 layers are currently hidden during gameplay.
	  It sets $31ff02 with either $7be0 and $2960 (and $31ff8e is $c00).
	  Game actually uses the "rowscroll/rowselect" tables for a line window 
	  effect to draw the boxing ring over NBG0. 
	  Same deal for ga2 when in stage 2 cave a wall torch is lit.
	  
	- harddunk draws solid white in attract mode when the players are presented.
	  NBG0 is set with $200 on center X/Y, same as above or perhaps missing
	  tilemap wraparound?

	- Wrong priority cases (parenthesis for the level setup):
	  dbzvrvs: draws text layer ($e) behind sprite-based gauges ($f). 
	  dbzvrvs: Sheng-Long speech balloon during Piccoro ending (fixme: check levels). 
	  f1lap: attract mode ranking sprite-based text ($a) vs. road ($d)
	  f1lap: attract mode map display (after aforementioned), sprite-based turn names 
	  ($a) are hidden by map ($d) again;
	  (Note: Theory about these being CPU core bug(s) is debunked by the fact that latter 
	   sets up via immediate opcodes)
	
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
                   ---- --f- ---- ---- : 1= Global X/Y flip? (most games?)
                   ---- ---f ---- ---- : 1= prohbit Y flip? (Air Rescue 2nd screen title, also gets set on one of the intro sequence screens)
                   ---- ---- ---- 4--- : 1= X+Y flip for NBG3
                   ---- ---- ---- -2-- : 1= X+Y flip for NBG2
                   ---- ---- ---- --1- : 1= X+Y flip for NBG1
                   ---- ---- ---- ---0 : 1= X+Y flip for NBG0
         $31FF02 : x--- ---- --x- ---- : Bitmap layer enable (?)
				   -x-- ---- ---- ---- : 1= NBG3 page wrapping disable (clipping enable according to code?)
				   --x- ---- ---- ---- : 1= NBG2 page wrapping disable
                   ---1 ---- ---- ---- : 1= NBG1 page wrapping disable
                   ---- 0--- ---- ---- : 1= NBG0 page wrapping disable
				   ---- -x-- ---- ---- : 1= bitmap layer clipping mode (1=outside)
				   ---- --x- ---- ---- : 1= NBG3 clipping mode (1=outside)
				   ---- ---x ---- ---- : 1= NBG2 clipping mode (1=outside)
				   ---- ---- x--- ---- : 1= NBG1 clipping mode (1=outside)
				   ---- ---- -x-- ---- : 1= NBG0 clipping mode (1=outside)
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

    reference
    - arabfgt : https://www.youtube.com/watch?v=98QivDAGz3I
    - darkedge : https://www.youtube.com/watch?v=riO1yb95z7s

*/

#include "driver.h"
#include "includes/segas32.h"



/*************************************
 *
 *  Debugging
 *
 *************************************/

#define SHOW_CLIPS				0
#define QWERTY_LAYER_ENABLE		0
#define PRINTF_MIXER_DATA		0
#define SHOW_ALPHA				0
#define LOG_SPRITES				0



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
#define MIXER_LAYER_SPRITES_2	8	/* semi-kludge to have a frame buffer for sprite backlayer */
#define MIXER_LAYER_MULTISPR	9
#define MIXER_LAYER_MULTISPR_2	10

#define TILEMAP_CACHE_SIZE		32



/*************************************
 *
 *  Helper macros
 *
 *************************************/

#ifdef LSB_FIRST
#define SWAP_HALVES(x)			(x)
#else
#define SWAP_HALVES(x)			(((x) >> 16) | ((x) << 16))
#endif



/*************************************
 *
 *  Type definitions
 *
 *************************************/

struct layer_info
{
	struct mame_bitmap *	bitmap;
	UINT8 *					transparent;
};


struct extents_list
{
	UINT8					scan_extent[256];
	UINT16					extent[32][16];
};


struct cache_entry
{
	struct cache_entry *	next;
	struct tilemap *				tmap;
	UINT8					page;
	UINT8					bank;
};



/*************************************
 *
 *  Globals
 *
 *************************************/

UINT16 *system32_videoram;
UINT16 *system32_spriteram;
UINT16 *system32_paletteram[2];
UINT16 system32_displayenable[2];
UINT16 system32_tilebank_external;

bool opaquey_hack = false; /* dink */



/*************************************
 *
 *  Statics
 *
 *************************************/

static UINT8 is_multi32;

/* tilemap cache */
static struct cache_entry *cache_head;

/* mixer data */
static struct layer_info layer_data[11];
static UINT16 mixer_control[2][0x40];
static UINT16 *solid_0000;
static UINT16 *solid_ffff;
static int32_t *prev_bgstartx;
static int32_t *prev_bgendx;
static int32_t *bgcolor_line;

/* sprite data */
static UINT8 sprite_render_count;
static UINT8 sprite_control_latched[8];
static UINT8 sprite_control[8];
static UINT32 *spriteram_32bit;



/*************************************
 *
 *  Prototypes
 *
 *************************************/

static void get_tile_info(int tile_index);
static void sprite_erase_buffer(void);
static void sprite_swap_buffers(void);
static void sprite_render_list(void);




/*************************************
 *
 *  Video start
 *
 *************************************/

static int common_start(int multi32)
{
	int tmap;

	/* remember whether or not we are multi32 */
	is_multi32 = multi32;

	/* allocate videoram */
	system32_videoram = auto_malloc(0x20000);

	/* allocate a copy of spriteram in 32-bit format */
	spriteram_32bit = auto_malloc(sizeof(UINT32)*(0x20000/4));

	/* allocate the tilemap cache */
	cache_head = NULL;
	for (tmap = 0; tmap < TILEMAP_CACHE_SIZE; tmap++)
	{
		struct cache_entry *entry = auto_malloc(sizeof(struct cache_entry));

		entry->tmap = tilemap_create(get_tile_info, tilemap_scan_rows, TILEMAP_OPAQUE, 16,16, 32,16);
		entry->page = 0xff;
		entry->bank = 0;
		entry->next = cache_head;
		tilemap_set_user_data(entry->tmap, entry);

		cache_head = entry;
	}

	/* allocate the bitmaps (a few extra for multi32) */
	for (tmap = 0; tmap < 9 + 2 * multi32; tmap++)
	{
		layer_data[tmap].bitmap = auto_bitmap_alloc_depth(416, 224, 16);
		layer_data[tmap].transparent = auto_malloc(sizeof(layer_data[tmap].transparent[0]) * 256);
		memset(layer_data[tmap].transparent, 0, sizeof(layer_data[tmap].transparent[0]) * 256);
	}

	/* allocate pre-rendered solid lines of 0's and ffff's */
	solid_0000 = auto_malloc(sizeof(solid_0000[0]) * 512);
	memset(solid_0000, 0x00, sizeof(solid_0000[0]) * 512);
	solid_ffff = auto_malloc(sizeof(solid_ffff[0]) * 512);
	memset(solid_ffff, 0xff, sizeof(solid_ffff[0]) * 512);

	/* allocate background color per line*/
	prev_bgstartx = auto_malloc(sizeof(prev_bgstartx[0]) * 512);
	prev_bgendx = auto_malloc(sizeof(prev_bgendx[0]) * 512);
	bgcolor_line = auto_malloc(sizeof(bgcolor_line[0]) * 512);
	memset(prev_bgstartx, -1, sizeof(prev_bgstartx[0]) * 512);
	memset(prev_bgendx, -1, sizeof(prev_bgendx[0]) * 512);
	memset(bgcolor_line, -1, sizeof(bgcolor_line[0]) * 512);

	/* initialize videoram */
	memset(system32_videoram, 0x00, 0x20000);
	system32_videoram[0x1ff00/2] = 0x8000;

	memset(mixer_control, 0xff, sizeof(mixer_control[0][0]) * 0x80 );

	return 0;
}


VIDEO_START( system32 )
{
	return common_start(0);
}


VIDEO_START( multi32 )
{
	return common_start(1);
}



/*************************************
 *
 *  Sprite management
 *
 *************************************/

static void update_sprites(int param)
{
	/* if automatic mode is selected, do it every frame (0) or every other frame (1) */
	if (!(sprite_control[3] & 2))
	{
		/* if we count down to the start, process the automatic swapping, but only after a short delay */
		if (sprite_render_count-- == 0)
		{
			sprite_control[0] = 3;
			sprite_render_count = sprite_control[3] & 1;
		}
	}

	/* look for pending commands */
	if (sprite_control[0] & 2)
		sprite_erase_buffer();
	if (sprite_control[0] & 1)
	{
		sprite_swap_buffers();
		sprite_render_list();
	}
	sprite_control[0] = 0;
}


void system32_set_vblank(int state)
{
	/* at the end of VBLANK is when automatic sprite rendering happens */
	if (!state)
		timer_set(TIME_IN_USEC(50), 1, update_sprites);
}



/*************************************
 *
 *  Common palette handling
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
 *  Video RAM access
 *
 *************************************/

READ16_HANDLER( system32_videoram_r )
{
	return system32_videoram[offset];
}


WRITE16_HANDLER( system32_videoram_w )
{
	COMBINE_DATA(&system32_videoram[offset]);

	/* if we are not in the control area, just update any affected tilemaps */
	if (offset < 0x1ff00/2)
	{
		struct cache_entry *entry;
		int page = offset >> 9;
		offset &= 0x1ff;

		/* scan the cache for a matching pages */
		for (entry = cache_head; entry != NULL; entry = entry->next)
			if (entry->page == page)
				tilemap_mark_tile_dirty(entry->tmap, offset);
	}
}


READ32_HANDLER( multi32_videoram_r )
{
	return system32_videoram[offset*2+0] |
	      (system32_videoram[offset*2+1] << 16);
}


WRITE32_HANDLER( multi32_videoram_w )
{
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		system32_videoram_w(offset*2+0, data, mem_mask);
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		system32_videoram_w(offset*2+1, data >> 16, mem_mask >> 16);
}



/*************************************
 *
 *  Sprite control registers
 *
 *************************************/

READ16_HANDLER( system32_sprite_control_r )
{
	switch (offset)
	{
		case 0:
			/*  D1 : Seems to be '1' only during an erase in progress, this
                     occurs very briefly though.
                D0 : Selected frame buffer (0= A, 1= B) */
			return 0xfffc | (layer_data[MIXER_LAYER_SPRITES].bitmap < layer_data[MIXER_LAYER_SPRITES_2].bitmap);

		case 1:
			/*  D1 : ?
                D0 : ?

                Values seem to be:

                0 = Unknown (relates to *approaching* out of time condition)
                1 = Normal status
                2 = Overdraw (rendering time is over but end-of-list command not read yet)
                3 = Never occurs

                Condition 2 can occur during rendering or list processing. */
			return 0xfffc | 1;

		case 2:
			/*  D1 : 1= Vertical flip, 0= Normal orientation
                D0 : 1= Horizontal flip, 0= Normal orientation */
			return 0xfffc | sprite_control_latched[2];

		case 3:
			/*  D1 : 1= Manual mode, 0= Automatic mode
                D0 : 1= 30 Hz update, 0= 60 Hz update (automatic mode only) */
			return 0xfffc | sprite_control_latched[3];

		case 4:
			/*  D1 : ?
                D0 : ? */
			return 0xfffc | sprite_control_latched[4];

		case 5:
			/*  D1 : ?
                D0 : ? */
			return 0xfffc | sprite_control_latched[5];

		case 6:
			/*  D0 : 1= 416 pixels
                     0= 320 pixels */
			return 0xfffc | (sprite_control_latched[6] & 1);

		case 7:
			/*  D1 : ?
                D0 : ? */
			return 0xfffc;
	}
	return 0xffff;
}


WRITE16_HANDLER( system32_sprite_control_w )
{
	if (ACCESSING_LSB)
		sprite_control[offset & 7] = data;
}


READ32_HANDLER( multi32_sprite_control_r )
{
	return system32_sprite_control_r(offset*2+0, mem_mask) |
	      (system32_sprite_control_r(offset*2+1, mem_mask >> 16) << 16);
}


WRITE32_HANDLER( multi32_sprite_control_w )
{
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		system32_sprite_control_w(offset*2+0, data, mem_mask);
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		system32_sprite_control_w(offset*2+1, data >> 16, mem_mask >> 16);
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


WRITE16_HANDLER( system32_spriteram_w )
{
	COMBINE_DATA(&system32_spriteram[offset]);
	spriteram_32bit[offset/2] =
		((system32_spriteram[offset |  1] >> 8 ) & 0x000000ff) |
		((system32_spriteram[offset |  1] << 8 ) & 0x0000ff00) |
		((system32_spriteram[offset & ~1] << 8 ) & 0x00ff0000) |
		((system32_spriteram[offset & ~1] << 24) & 0xff000000);
}


READ32_HANDLER( multi32_spriteram_r )
{
	return system32_spriteram[offset*2+0] |
	      (system32_spriteram[offset*2+1] << 16);
}


WRITE32_HANDLER( multi32_spriteram_w )
{
	data = SWAP_HALVES(data);
	mem_mask = SWAP_HALVES(mem_mask);
	COMBINE_DATA((UINT32 *)&system32_spriteram[offset*2]);
	spriteram_32bit[offset/2] =
		((system32_spriteram[offset |  1] >> 8 ) & 0x000000ff) |
		((system32_spriteram[offset |  1] << 8 ) & 0x0000ff00) |
		((system32_spriteram[offset & ~1] << 8 ) & 0x00ff0000) |
		((system32_spriteram[offset & ~1] << 24) & 0xff000000);
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

WRITE16_HANDLER( multi32_mixer_0_w )
{
	COMBINE_DATA(&mixer_control[0][offset]);
}

READ16_HANDLER( multi32_mixer_1_r )
{
	return mixer_control[1][offset];
}

WRITE16_HANDLER( multi32_mixer_1_w )
{
	COMBINE_DATA(&mixer_control[1][offset]);
}



/*************************************
 *
 *  Tilemap cache
 *
 *************************************/

static struct tilemap *find_cache_entry(int page, int bank)
{
	struct cache_entry *entry, *prev;

	/* scan the list for a matching entry */
	prev = NULL;
	entry = cache_head;
	while (1)
	{
		if (entry->page == page && entry->bank == bank)
		{
			/* move us to the head before returning */
			if (prev)
			{
				prev->next = entry->next;
				entry->next = cache_head;
				cache_head = entry;
			}
			return entry->tmap;
		}

		/* stop on the last entry */
		if (entry->next == NULL)
			break;
		prev = entry;
		entry = entry->next;
	}

	/* okay, we didn't find one; take over this last entry */
	entry->page = page;
	entry->bank = bank;
	tilemap_mark_all_tiles_dirty(entry->tmap);

	/* move it to the head */
	prev->next = entry->next;
	entry->next = cache_head;
	cache_head = entry;

	return entry->tmap;
}



/*************************************
 *
 *  Tilemap callback
 *
 *************************************/

static void get_tile_info(int tile_index)
{
	struct cache_entry *entry = tile_info.user_data;
	UINT16 data = system32_videoram[(entry->page & 0x7f) << 9 | tile_index];
	SET_TILE_INFO(0, (entry->bank << 13) + (data & 0x1fff), (data >> 4) & 0x1ff, (data >> 14) & 3);
}



/*************************************
 *
 *  Clipping extents computation
 *
 *************************************/

static int compute_clipping_extents(int enable, int clipout, int clipmask, const struct rectangle *cliprect, struct extents_list *list)
{
	int flip = (system32_videoram[0x1ff00/2] >> 9) & 1;
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
			clips[i].min_x = system32_videoram[0x1ff60/2 + i * 4] & 0x1ff;
			clips[i].min_y = system32_videoram[0x1ff62/2 + i * 4] & 0x0ff;
			clips[i].max_x = (system32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1;
			clips[i].max_y = (system32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1;
		}
		else
		{
			clips[i].max_x = (Machine->visible_area.max_x + 1) - (system32_videoram[0x1ff60/2 + i * 4] & 0x1ff);
			clips[i].max_y = (Machine->visible_area.max_y + 1) - (system32_videoram[0x1ff62/2 + i * 4] & 0x0ff);
			clips[i].min_x = (Machine->visible_area.max_x + 1) - ((system32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1);
			clips[i].min_y = (Machine->visible_area.max_y + 1) - ((system32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1);
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


static void compute_tilemap_flips(int bgnum, int *flipx, int *flipy)
{
	int layer_flip;

	/* determine if we're flipped */
	int global_flip = (system32_videoram[0x1ff00 / 2] >> 9)&1;

	*flipx = global_flip;
	*flipy = global_flip;

	layer_flip = (system32_videoram[0x1ff00 / 2] >> bgnum) & 1;

	*flipy ^= layer_flip;
	*flipx ^= layer_flip;

	// this bit is set on Air Rescue (screen 2) title screen, during the Air Rescue introduction demo, and in f1en when you win a single player race
	// it seems to prohibit (at least) the per-tilemap y flipping (maybe global y can override it)
	if ((system32_videoram[0x1ff00 / 2] >> 8) & 1) *flipy = 0;
}

/*************************************
 *
 *  Zooming tilemaps (NBG0/1)
 *
 *************************************/

static INLINE void get_tilemaps(int bgnum, struct tilemap **tilemaps)
{
	int tilebank, page;

	/* determine the current tilebank */
	if (is_multi32)
		tilebank = (system32_tilebank_external >> (2*bgnum)) & 3;
	else
		tilebank = ((system32_tilebank_external & 1) << 1) | ((system32_videoram[0x1ff00/2] & 0x400) >> 10);

	/* find the cache entries */
	page = (system32_videoram[0x1ff40/2 + 2 * bgnum + 0] >> 0) & 0x7f;
	tilemaps[0] = find_cache_entry(page, tilebank);
	page = (system32_videoram[0x1ff40/2 + 2 * bgnum + 0] >> 8) & 0x7f;
	tilemaps[1] = find_cache_entry(page, tilebank);
	page = (system32_videoram[0x1ff40/2 + 2 * bgnum + 1] >> 0) & 0x7f;
	tilemaps[2] = find_cache_entry(page, tilebank);
	page = (system32_videoram[0x1ff40/2 + 2 * bgnum + 1] >> 8) & 0x7f;
	tilemaps[3] = find_cache_entry(page, tilebank);
}


static void update_tilemap_zoom(struct layer_info *layer, const struct rectangle *cliprect, int bgnum)
{
	int clipenable, clipout, clips, clipdraw_start;
	struct mame_bitmap *bitmap = layer->bitmap;
	struct extents_list clip_extents;
	struct tilemap *tilemaps[4];
	UINT32 srcx, srcx_start, srcy;
	UINT32 srcxstep, srcystep;
	int dstxstep, dstystep;
	int flipx, flipy;
	int opaque;
	int x, y;

	/* get the tilemaps */
	get_tilemaps(bgnum, tilemaps);

	/* configure the layer */
	opaque = 0;
//opaque = (system32_videoram[0x1ff8e/2] >> (8 + bgnum)) & 1;
//if (code_pressed(KEYCODE_Z) && bgnum == 0) opaque = 1;
//if (code_pressed(KEYCODE_X) && bgnum == 1) opaque = 1;

	/* todo determine flipping */
	compute_tilemap_flips(bgnum, &flipx, &flipy);

	/* determine the clipping */
	clipenable = (system32_videoram[0x1ff02/2] >> (11 + bgnum)) & 1;
	clipout = (system32_videoram[0x1ff02/2] >> (6 + bgnum)) & 1;
	clips = (system32_videoram[0x1ff06/2] >> (4 * bgnum)) & 0x0f;
	clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	/* extract the X/Y step values (these are in destination space!) */
	dstxstep = system32_videoram[0x1ff50/2 + 2 * bgnum] & 0xfff;
	if (system32_videoram[0x1ff00/2] & 0x4000)
		dstystep = system32_videoram[0x1ff52/2 + 2 * bgnum] & 0xfff;
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
	srcx_start = (system32_videoram[0x1ff12/2 + 4 * bgnum] & 0x3ff) << 20;
	srcx_start += (system32_videoram[0x1ff10/2 + 4 * bgnum] & 0xff00) << 4;
	srcy = (system32_videoram[0x1ff16/2 + 4 * bgnum] & 0x1ff) << 20;
	srcy += (system32_videoram[0x1ff14/2 + 4 * bgnum] & 0xfe00) << 4;

	/* then account for the destination center coordinates */
	srcx_start -= ((INT16)(system32_videoram[0x1ff30/2 + 2 * bgnum] << 6) >> 6) * srcxstep;
	srcy -= ((INT16)(system32_videoram[0x1ff32/2 + 2 * bgnum] << 7) >> 7) * srcystep;

	/* finally, account for destination top,left coordinates */
	srcx_start += cliprect->min_x * srcxstep;
	srcy += cliprect->min_y * srcystep;

	/* if we're flipped, simply adjust the start/step parameters */
	if (flipy)
	{
		srcy += (Machine->visible_area.max_y - 2 * cliprect->min_y) * srcystep;
		srcystep = -srcystep;
	}

	if (flipx)
	{
		srcx_start += (Machine->visible_area.max_x - 2 * cliprect->min_x) * srcxstep;
		srcxstep = -srcxstep;
	}

	/* loop over the target rows */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			int transparent = 0;
			UINT16 *src[2];

			/* look up the pages and get their source pixmaps */
			src[0] = tilemap_get_pixmap(tilemaps[((srcy >> 27) & 2) + 0])->line[(srcy >> 20) & 0xff];
			src[1] = tilemap_get_pixmap(tilemaps[((srcy >> 27) & 2) + 1])->line[(srcy >> 20) & 0xff];

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
						if ((pix & 0x0f) == 0 && !opaque)
							pix = 0, transparent++;
						dst[x] = pix;
					}
				}

				/* otherwise, clear to zero */
				else
				{
					int pixels = extents[1] - extents[0];
					memset(&dst[extents[0]], 0, pixels * sizeof(dst[0]));
					srcx += srcxstep * pixels;
					transparent += pixels;
				}

				/* stop at the end */
				if (extents[1] > cliprect->max_x)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}

			layer->transparent[y] = (transparent == cliprect->max_x - cliprect->min_x + 1);
		}
		else
			layer->transparent[y] = 1;

		/* advance in Y */
		srcy += srcystep;
	}

	/* enable this code below to display zoom information */
#if 0
	if (dstxstep != 0x200 || dstystep != 0x200)
		usrintf_showmessage("Zoom=%03X,%03X  Cent=%03X,%03X", dstxstep, dstystep,
			system32_videoram[0x1ff30/2 + 2 * bgnum],
			system32_videoram[0x1ff32/2 + 2 * bgnum]);
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
	struct extents_list clip_extents;
	struct tilemap *tilemaps[4];
	int rowscroll, rowselect;
	int xscroll, yscroll;
	UINT16 *table;
	int srcx, srcy;
	int flipx, flipy;
	int opaque;
	int x, y;

	/* get the tilemaps */
	get_tilemaps(bgnum, tilemaps);

	/* configure the layer */
	opaque = (opaquey_hack) ? ((system32_videoram[0x1ff8e/2] >> (8 + bgnum)) & 1) : 0;
//opaque = (system32_videoram[0x1ff8e/2] >> (8 + bgnum)) & 1;
//if (code_pressed(KEYCODE_C) && bgnum == 2) opaque = 1;
//if (code_pressed(KEYCODE_V) && bgnum == 3) opaque = 1;

	/* todo determine flipping */
	compute_tilemap_flips(bgnum, &flipx, &flipy);

	/* determine the clipping */
	clipenable = (system32_videoram[0x1ff02/2] >> (11 + bgnum)) & 1;
	clipout = (system32_videoram[0x1ff02/2] >> (6 + bgnum)) & 1;
	clips = (system32_videoram[0x1ff06/2] >> (4 * bgnum)) & 0x0f;
	clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	/* determine if row scroll and/or row select is enabled */
	rowscroll = (system32_videoram[0x1ff04/2] >> (bgnum - 2)) & 1;
	rowselect = (system32_videoram[0x1ff04/2] >> bgnum) & 1;
	if ((system32_videoram[0x1ff04/2] >> (bgnum + 2)) & 1)
		rowscroll = rowselect = 0;

	/* get a pointer to the table */
	table = &system32_videoram[(system32_videoram[0x1ff04/2] >> 10) * 0x400];

	/* start with screen-wide X and Y scrolls */
	xscroll = (system32_videoram[0x1ff12/2 + 4 * bgnum] & 0x3ff) - (system32_videoram[0x1ff30/2 + 2 * bgnum] & 0x1ff);
	yscroll = (system32_videoram[0x1ff16/2 + 4 * bgnum] & 0x1ff);

	/* render the tilemap into its bitmap */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			int transparent = 0;
			UINT16 *src[2];
			int srcxstep;

			/* if we're not flipped, things are straightforward */
			if (!flipx)
			{
				srcx = cliprect->min_x + xscroll;
				srcxstep = 1;				
			}
			else
			{	
				srcx = cliprect->max_x + xscroll;
				srcxstep = -1;
			}

			if (!flipy)
			{
				srcy = yscroll + y;
			}
			else
			{
				srcy = yscroll + Machine->visible_area.max_y - y;
			}

			/* apply row scroll/select */
			if (rowscroll)
				srcx += table[0x000 + 0x100 * (bgnum - 2) + y] & 0x3ff;
			if (rowselect)
				srcy = (yscroll + table[0x200 + 0x100 * (bgnum - 2) + y]) & 0x1ff;


			/* look up the pages and get their source pixmaps */
			src[0] = tilemap_get_pixmap(tilemaps[((srcy >> 7) & 2) + 0])->line[srcy & 0xff];
			src[1] = tilemap_get_pixmap(tilemaps[((srcy >> 7) & 2) + 1])->line[srcy & 0xff];

			/* loop over extents */
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					for (x = extents[0]; x < extents[1]; x++, srcx += srcxstep)
					{
						UINT16 pix = src[(srcx >> 9) & 1][srcx & 0x1ff];
						if ((pix & 0x0f) == 0 && !opaque)
							pix = 0, transparent++;
						dst[x] = pix;
					}
				}

				/* otherwise, clear to zero */
				else
				{
					int pixels = extents[1] - extents[0];
					memset(&dst[extents[0]], 0, pixels * sizeof(dst[0]));
					srcx += srcxstep * pixels;
					transparent += pixels;
				}

				/* stop at the end */
				if (extents[1] > cliprect->max_x)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}

			layer->transparent[y] = (transparent == cliprect->max_x - cliprect->min_x + 1);
		}
		else
			layer->transparent[y] = 1;
	}

	/* enable this code below to display scroll information */
#if 0
	if (rowscroll || rowselect)
		usrintf_showmessage("Scroll=%d Select=%d  Table@%06X",
			rowscroll, rowselect, (system32_videoram[0x1ff04/2] >> 10) * 0x800);
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
	UINT16 *tilebase;
	UINT16 *gfxbase;
	int startx, starty;
	int endx, endy;
	int x, y, iy;
	int flip;

	/* determine if we're flipped */
	flip = (system32_videoram[0x1ff00/2] >> 9) & 1;

	/* determine the base of the tilemap and graphics data */
	tilebase = &system32_videoram[((system32_videoram[0x1ff5c/2] >> 4) & 0x1f) * 0x800];
	gfxbase = &system32_videoram[(system32_videoram[0x1ff5c/2] & 7) * 0x2000];

	/* compute start/end tile numbers */
	startx = cliprect->min_x / 8;
	starty = cliprect->min_y / 8;
	endx = cliprect->max_x / 8;
	endy = cliprect->max_y / 8;

	/* loop over tiles */
	for (y = starty; y <= endy; y++)
		for (x = startx; x <= endx; x++)
		{
			int tile = tilebase[y * 64 + x];
			UINT16 *src = &gfxbase[(tile & 0x1ff) * 16];
			int color = (tile & 0xfe00) >> 5;

			/* non-flipped case */
			if (!flip)
			{
				UINT16 *dst = ((UINT16 *)bitmap->line[y * 8]) + x * 8;

				/* loop over rows */
				for (iy = 0; iy < 8; iy++)
				{
					int pixels = *src++;
					int pix;

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[0] = pix;

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[1] = pix;

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[2] = pix;

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[3] = pix;

					pixels = *src++;

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[4] = pix;

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[5] = pix;

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[6] = pix;

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[7] = pix;

					dst += bitmap->rowpixels;
				}
			}

			/* flipped case */
			else
			{
				int effdstx = Machine->visible_area.max_x - x * 8;
				int effdsty = Machine->visible_area.max_y - y * 8;
				UINT16 *dst = ((UINT16 *)bitmap->line[effdsty]) + effdstx;

				/* loop over rows */
				for (iy = 0; iy < 8; iy++)
				{
					int pixels = *src++;
					int pix;

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[0] = pix;

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[-1] = pix;

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[-2] = pix;

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[-3] = pix;

					pix = *src++;

					pix = (pixels >> 4) & 0x0f;
					if (pix)
						pix |= color;
					dst[-4] = pix;

					pix = (pixels >> 0) & 0x0f;
					if (pix)
						pix |= color;
					dst[-5] = pix;

					pix = (pixels >> 12) & 0x0f;
					if (pix)
						pix |= color;
					dst[-6] = pix;

					pix = (pixels >> 8) & 0x0f;
					if (pix)
						pix |= color;
					dst[-7] = pix;

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
	struct extents_list clip_extents;
	int xscroll, yscroll;
	int color;
	int x, y;
	int bpp;

	/* configure the layer */
	bpp = (system32_videoram[0x1ff00/2] & 0x0800) ? 8 : 4;

	/* determine the clipping */
	clipenable = (system32_videoram[0x1ff02/2] >> 15) & 1;
	clipout = (system32_videoram[0x1ff02/2] >> 10) & 1;
	clips = 0x10;
	clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	/* determine x/y scroll */
	xscroll = system32_videoram[0x1ff88/2] & 0x1ff;
	yscroll = system32_videoram[0x1ff8a/2] & 0x1ff;
	color = (system32_videoram[0x1ff8c/2] << 4) & 0x1fff0 & ~((1 << bpp) - 1);

	/* loop over target rows */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			int transparent = 0;

			/* loop over extents */
			while (1)
			{
				/* if we're drawing on this extent, draw it */
				if (clipdraw)
				{
					/* 8bpp mode case */
					if (bpp == 8)
					{
						UINT8 *src = (UINT8 *)&system32_videoram[512/2 * ((y + yscroll) & 0xff)];
						for (x = extents[0]; x < extents[1]; x++)
						{
							int effx = (x + xscroll) & 0x1ff;
							int pix = src[BYTE_XOR_LE(effx)] + color;
							if ((pix & 0xff) == 0)
								pix = 0, transparent++;
							dst[x] = pix;
						}
					}

					/* 4bpp mode case */
					else
					{
						UINT16 *src = &system32_videoram[512/4 * ((y + yscroll) & 0x1ff)];
						for (x = extents[0]; x < extents[1]; x++)
						{
							int effx = (x + xscroll) & 0x1ff;
							int pix = ((src[effx / 4] >> (4 * (effx & 3))) & 0x0f) + color;
							if ((pix & 0x0f) == 0)
								pix = 0, transparent++;
							dst[x] = pix;
						}
					}
				}

				/* otherwise, clear to zero */
				else
				{
					int pixels = extents[1] - extents[0];
					memset(&dst[extents[0]], 0, pixels * sizeof(dst[0]));
					transparent += pixels;
				}

				/* stop at the end */
				if (extents[1] > cliprect->max_x)
					break;

				/* swap states and advance to the next extent */
				clipdraw = !clipdraw;
				extents++;
			}

			layer->transparent[y] = (transparent == cliprect->max_x - cliprect->min_x + 1);
		}
		else
			layer->transparent[y] = 1;
	}
}



/*************************************
 *
 *  Master tilemap chip updater
 *
 *************************************/

static void update_background(struct layer_info *layer, const struct rectangle *cliprect)
{
	struct mame_bitmap *bitmap = layer->bitmap;
	int x, y;

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int color;

		/* determine the color */
		if (system32_videoram[0x1ff5e/2] & 0x8000)
		{
			/* line color select (bank wraps at 511, confirmed by arabfgt and kokoroj2) */
			int yoffset = (system32_videoram[0x1ff5e/2] + y) & 0x1ff;
			color = (system32_videoram[0x1ff5e/2] & 0x1e00) + yoffset;
		}
		else
			color = system32_videoram[0x1ff5e/2] & 0x1e00;

		/* if the color doesn't match, fill */
		if ((bgcolor_line[y & 0x1ff] != color) || (prev_bgstartx[y & 0x1ff] != cliprect->min_x) || (prev_bgendx[y & 0x1ff] != cliprect->max_x))
		{
			int x;
			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
				dst[x] = color;

			prev_bgstartx[y & 0x1ff] = cliprect->min_x;
			prev_bgendx[y & 0x1ff] = cliprect->max_x;
			bgcolor_line[y & 0x1ff] = color;
		}
	}
}


static UINT8 update_tilemaps(const struct rectangle *cliprect)
{
	int enable0 = !(system32_videoram[0x1ff02/2] & 0x0001) && !(system32_videoram[0x1ff8e/2] & 0x0002);
	int enable1 = !(system32_videoram[0x1ff02/2] & 0x0002) && !(system32_videoram[0x1ff8e/2] & 0x0004);
	int enable2 = !(system32_videoram[0x1ff02/2] & 0x0004) && !(system32_videoram[0x1ff8e/2] & 0x0008) && !(system32_videoram[0x1ff00/2] & 0x1000);
	int enable3 = !(system32_videoram[0x1ff02/2] & 0x0008) && !(system32_videoram[0x1ff8e/2] & 0x0010) && !(system32_videoram[0x1ff00/2] & 0x2000);
	int enablet = !(system32_videoram[0x1ff02/2] & 0x0010) && !(system32_videoram[0x1ff8e/2] & 0x0001);
	int enableb = !(system32_videoram[0x1ff02/2] & 0x0020) && !(system32_videoram[0x1ff8e/2] & 0x0020);

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
	update_background(&layer_data[MIXER_LAYER_BACKGROUND], cliprect);

	return (enablet << 0) | (enable0 << 1) | (enable1 << 2) | (enable2 << 3) | (enable3 << 4) | (enableb << 5);
}



/*************************************
 *
 *  Sprite buffer management
 *
 *************************************/

static void sprite_erase_buffer(void)
{
	/* erase the visible sprite buffer and clear the checksums */
	fillbitmap(layer_data[MIXER_LAYER_SPRITES].bitmap, 0xffff, NULL);

	/* for multi32, erase the other buffer as well */
	if (is_multi32)
		fillbitmap(layer_data[MIXER_LAYER_MULTISPR].bitmap, 0xffff, NULL);
}


static void sprite_swap_buffers(void)
{
	/* swap between the two sprite buffers */
	struct layer_info temp;
	temp = layer_data[MIXER_LAYER_SPRITES];
	layer_data[MIXER_LAYER_SPRITES] = layer_data[MIXER_LAYER_SPRITES_2];
	layer_data[MIXER_LAYER_SPRITES_2] = temp;

	/* for multi32, swap the other buffer as well */
	if (is_multi32)
	{
		temp = layer_data[MIXER_LAYER_MULTISPR];
		layer_data[MIXER_LAYER_MULTISPR] = layer_data[MIXER_LAYER_MULTISPR_2];
		layer_data[MIXER_LAYER_MULTISPR_2] = temp;
	}

	/* latch any pending info */
	memcpy(sprite_control_latched, sprite_control, sizeof(sprite_control_latched));
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

#define sprite_draw_pixel_16(trans)											\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= clipin->min_x && x <= clipin->max_x && 						\
		(!do_clipout || x < clipout->min_x || x > clipout->max_x) &&		\
		pix != trans) 														\
	{																		\
		if (!indirect)														\
		{																	\
			if (pix != 0)													\
			{																\
				if (!shadow)												\
					dest[x] = color | pix;									\
				else														\
					dest[x] &= 0x7fff;										\
			}																\
		}																	\
		else																\
		{																	\
			int indpix = indtable[pix];										\
			if ((indpix & transmask) != transmask)							\
			{																\
				if (!shadow)												\
					dest[x] = indpix;										\
				else														\
					dest[x] &= 0x7fff;										\
			}																\
		}																	\
	}

#define sprite_draw_pixel_256(trans)										\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= clipin->min_x && x <= clipin->max_x && 						\
		(!do_clipout || x < clipout->min_x || x > clipout->max_x) &&		\
		pix != trans) 														\
	{																		\
		if (!indirect)														\
		{																	\
			if (pix != 0)													\
			{																\
				if (!shadow)												\
					dest[x] = color | pix;									\
				else														\
					dest[x] &= 0x7fff;										\
			}																\
		}																	\
		else																\
		{																	\
			int indpix = (indtable[pix >> 4]) | (pix & 0x0f);				\
			if ((indpix & transmask) != transmask)							\
			{																\
				if (!shadow)												\
					dest[x] = indpix;										\
				else														\
					dest[x] &= 0x7fff;										\
			}																\
		}																	\
	}

static int draw_one_sprite(UINT16 *data, int xoffs, int yoffs, const struct rectangle *clipin, const struct rectangle *clipout)
{
	static const int transparency_masks[4][4] =
	{
		{ 0x7fff, 0x3fff, 0x1fff, 0x0fff },
		{ 0x3fff, 0x1fff, 0x0fff, 0x07ff },
		{ 0x3fff, 0x1fff, 0x0fff, 0x07ff },
		{ 0x1fff, 0x0fff, 0x07ff, 0x03ff }
	};

	struct mame_bitmap *bitmap = layer_data[(!is_multi32 || !(data[3] & 0x0800)) ? MIXER_LAYER_SPRITES_2 : MIXER_LAYER_MULTISPR_2].bitmap;
	UINT8 numbanks = memory_region_length(REGION_GFX2) >> 20;
	const UINT32 *spritebase = (const UINT32 *)memory_region(REGION_GFX2);

	int indirect = data[0] & 0x2000;
	int indlocal = data[0] & 0x1000;
	int shadow   = (data[0] & 0x0800) && (sprite_control_latched[0x0a/2] & 1);
	int fromram  = data[0] & 0x0400;
	int bpp8     = data[0] & 0x0200;
	int transp   = (data[0] & 0x0100) ? 0 : (bpp8 ? 0xff : 0x0f);
	int flipy    = data[0] & 0x0080;
	int flipx    = data[0] & 0x0040;
	int offsety  = data[0] & 0x0020;
	int offsetx  = data[0] & 0x0010;
	int adjusty  = (data[0] >> 2) & 3;
	int adjustx  = (data[0] >> 0) & 3;
	int srch     = (data[1] >> 8);
	int srcw     = bpp8 ? (data[1] & 0x3f) : ((data[1] >> 1) & 0x3f);
	int bank     = is_multi32 ?
					((data[3] & 0x2000) >> 13) | ((data[3] & 0x8000) >> 14) :
					((data[3] & 0x0800) >> 11) | ((data[3] & 0x4000) >> 13);
	int dsth     = data[2] & 0x3ff;
	int dstw     = data[3] & 0x3ff;
	int ypos     = (INT16)(data[4] << 4) >> 4;
	int xpos     = (INT16)(data[5] << 4) >> 4;
	UINT32 addr  = data[6] | ((data[2] & 0xf000) << 4);
	int color    = 0x8000 | (data[7] & (bpp8 ? 0x7f00 : 0x7ff0));
	int hzoom, vzoom;
	int xdelta = 1, ydelta = 1;
	int x, y, xtarget, ytarget, yacc = 0, pix, transmask;
	const UINT32 *spritedata;
	UINT32 addrmask, curaddr;
	UINT16 indtable[16];

	/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
	if (srcw == 0 || srch == 0 || dstw == 0 || dsth == 0)
		goto bail;

	/* determine the transparency mask for pixels */
	transmask = transparency_masks[sprite_control_latched[0x08/2] & 3][sprite_control_latched[0x0a/2] & 3];
	if (bpp8)
		transmask &= 0xfff0;

	/* create the local palette for the indirect case */
	if (indirect)
	{
		UINT16 *src = indlocal ? &data[8] : &system32_spriteram[8 * (data[7] & 0x1fff)];
		for (x = 0; x < 16; x++)
			indtable[x] = (src[x] & (bpp8 ? 0xfff0 : 0xffff)) | ((sprite_control_latched[0x0a/2] & 1) ? 0x8000 : 0x0000);
	}

	/* clamp to within the memory region size */
	if (fromram)
	{
		spritedata = spriteram_32bit;
		addrmask = (0x20000 / 4) - 1;
	}
	else
	{
		if (numbanks)
			bank %= numbanks;
		spritedata = &spritebase[bank << 20];
		addrmask = 0xfffff;
	}

	/* compute X/Y deltas */
	hzoom = (((bpp8 ? 4 : 8) * srcw) << 16) / dstw;
	vzoom = (srch << 16) / dsth;

	/* adjust the starting X position */
	if (offsetx)
		xpos += xoffs;
	switch (adjustx)
	{
		case 0:
		case 3:	xpos -= (dstw - 1) / 2; 	break;
		case 1: xpos -= dstw - 1;			break;
		case 2:								break;
	}

	/* adjust the starting Y position */
	if (offsety)
		ypos += yoffs;
	switch (adjusty)
	{
		case 0:
		case 3:	ypos -= (dsth - 1) / 2; 	break;
		case 1: ypos -= dsth - 1;			break;
		case 2:								break;
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

	/* compute target X,Y positions for loops */
	xtarget = xpos + xdelta * dstw;
	ytarget = ypos + ydelta * dsth;

	/* adjust target x for clipping */
	if (xdelta > 0 && xtarget > clipin->max_x)
	{
		xtarget = clipin->max_x + 1;
		if (xpos >= xtarget)
			goto bail;
	}
	if (xdelta < 0 && xtarget < clipin->min_x)
	{
		xtarget = clipin->min_x - 1;
		if (xpos <= xtarget)
			goto bail;
	}

	/* loop from top to bottom */
	for (y = ypos; y != ytarget; y += ydelta)
	{
		/* skip drawing if not within the inclusive cliprect */
		if (y >= clipin->min_y && y <= clipin->max_y)
		{
			int do_clipout = (y >= clipout->min_y && y <= clipout->max_y);
			UINT16 *dest = (UINT16 *)bitmap->line[y];
			int xacc = 0;

			/* 4bpp case */
			if (!bpp8)
			{
				/* start at the word before because we preincrement below */
				curaddr = addr - 1;
				for (x = xpos; x != xtarget; )
				{
					UINT32 pixels = spritedata[++curaddr & addrmask];

					/* draw four pixels */
					pix = (pixels >> 28) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(transp)  x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 24) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 20) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 16) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 12) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  8) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  4) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  0) & 0xf; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_16(transp); x += xdelta; xacc += hzoom; } xacc -= 0x10000;

					/* check for end code */
					if (transp != 0 && pix == 0x0f)
						break;
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
					pix = (pixels >> 24) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(transp); x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >> 16) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  8) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(0);      x += xdelta; xacc += hzoom; } xacc -= 0x10000;
					pix = (pixels >>  0) & 0xff; while (xacc < 0x10000 && x != xtarget) { sprite_draw_pixel_256(transp); x += xdelta; xacc += hzoom; } xacc -= 0x10000;

					/* check for end code */
					if (transp != 0 && pix == 0xff)
						break;
				}
			}
		}

		/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
		yacc += vzoom;
		addr += srcw * (yacc >> 16);
		yacc &= 0xffff;
	}

	/* if we had an enabled inline indirect palette, we skip two entries */
bail:
	return (indirect && indlocal) ? 2 : 0;
}



static void sprite_render_list(void)
{
	struct rectangle outerclip, clipin, clipout;
	int xoffs = 0, yoffs = 0;
	int numentries = 0;
	int spritenum = 0;
	UINT16 *sprite;

	profiler_mark(PROFILER_USER2);

	logerror("----\n");

	/* compute the outer clip */
	outerclip.min_x = outerclip.min_y = 0;
	outerclip.max_x = (sprite_control_latched[0x0c/2] & 1) ? 415 : 319;
	outerclip.max_y = 223;

	/* initialize the cliprects */
	clipin = outerclip;
	clipout.min_x = clipout.min_y = 0;
	clipout.max_x = clipout.max_y = -1;

	/* now draw */
	while (numentries++ < 0x20000/16)
	{
		/* top two bits are a command */
		sprite = &system32_spriteram[8 * (spritenum & 0x1fff)];
		switch (sprite[0] >> 14)
		{
			/* command 0 = draw sprite */
			case 0:
				spritenum += 1 + draw_one_sprite(sprite, xoffs, yoffs, &clipin, &clipout);
				break;

			/* command 1 = set clipping */
			case 1:

				/* set the inclusive cliprect */
				if (sprite[0] & 0x1000)
				{
					clipin.min_y = (INT16)(sprite[0] << 4) >> 4;
					clipin.max_y = (INT16)(sprite[1] << 4) >> 4;
					clipin.min_x = (INT16)(sprite[2] << 4) >> 4;
					clipin.max_x = (INT16)(sprite[3] << 4) >> 4;
					sect_rect(&clipin, &outerclip);
				}

				/* set the exclusive cliprect */
				if (sprite[0] & 0x2000)
				{
					clipout.min_y = (INT16)(sprite[4] << 4) >> 4;
					clipout.max_y = (INT16)(sprite[5] << 4) >> 4;
					clipout.min_x = (INT16)(sprite[6] << 4) >> 4;
					clipout.max_x = (INT16)(sprite[7] << 4) >> 4;
				}

				/* advance to the next entry */
				spritenum++;
				break;

			/* command 2 = jump to position, and set X offset */
			case 2:

				/* set the global offset */
				if (sprite[0] & 0x2000)
				{
					yoffs = (INT16)(sprite[1] << 4) >> 4;
					xoffs = (INT16)(sprite[2] << 4) >> 4;
				}
				spritenum = sprite[0] & 0x1fff;
				break;

			/* command 3 = done */
			case 3:
				numentries = 0x20000/16;
				break;
		}
	}

	profiler_mark(PROFILER_END);
}



/*************************************
 *
 *  Mixer layer render
 *
 *************************************/

static INLINE UINT8 compute_color_offsets(int which, int layerbit, int layerflag)
{
	int mode = ((mixer_control[which][0x3e/2] & 0x8000) >> 14) | (layerbit & 1);

	switch (mode)
	{
		case 0:
		case 3:
		default:
			return !layerflag;

		case 1:
			/* fix me -- these are grayscale modes */
			return 2;

		case 2:
			return (!layerflag) ? 2 : 0;
	}
}

static INLINE UINT16 compute_sprite_blend(UINT8 encoding)
{
	int value = encoding & 0xf;

	switch ((encoding >> 4) & 3)
	{
		/* blend if priority == value */
		case 0:		return 1 << value;

		/* blend if priority <= value */
		case 1:		return (1 << value) | ((1 << value) - 1);

		/* blend if priority >= value */
		case 2:		return ~((1 << value) - 1) & 0xffff;

		/* blend always */
		default:
		case 3:		return 0xffff;
	}
}

static INLINE UINT16 *get_layer_scanline(int layer, int scanline)
{
	if (layer_data[layer].transparent[scanline])
		return (layer == MIXER_LAYER_SPRITES) ? solid_ffff : solid_0000;
	return (UINT16 *)layer_data[layer].bitmap->line[scanline];
}

static void mix_all_layers(int which, int xoffs, struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT8 enablemask)
{
	int blendenable = mixer_control[which][0x4e/2] & 0x0800;
	int blendfactor = (mixer_control[which][0x4e/2] >> 8) & 7;
	struct mixer_layer_info
	{
		UINT16		palbase;			/* palette base from control reg */
		UINT16		sprblendmask;		/* mask of sprite priorities this layer blends with */
		UINT8		blendmask;			/* mask of layers this layer blends with */
		UINT8		index;				/* index of this layer (MIXER_LAYER_XXX) */
		UINT8		effpri;				/* effective priority = (priority << 3) | layer_priority */
		UINT8		mixshift;			/* shift from control reg */
		UINT8		coloroffs;			/* color offset index */
	} layerorder[16][8], layersort[8];
	struct layer_info temp_sprite_save = { 0 };
	UINT8 sprgroup_shift, sprgroup_mask, sprgroup_or;
	int numlayers, laynum, groupnum;
	int rgboffs[3][3];
	int sprpixmask, sprshadowmask;
	int sprx, spry, sprx_start;
	int sprdx, sprdy;
	int sprshadow;
	int x, y, i;

	/* if we are the second monitor on multi32, swap in the proper sprite bank */
	if (which == 1)
	{
		temp_sprite_save = layer_data[MIXER_LAYER_SPRITES];
		layer_data[MIXER_LAYER_SPRITES] = layer_data[MIXER_LAYER_MULTISPR];
	}

	/* extract the RGB offsets */
	rgboffs[0][0] = (INT8)(mixer_control[which][0x40/2] << 2) >> 2;
	rgboffs[0][1] = (INT8)(mixer_control[which][0x42/2] << 2) >> 2;
	rgboffs[0][2] = (INT8)(mixer_control[which][0x44/2] << 2) >> 2;
	rgboffs[1][0] = (INT8)(mixer_control[which][0x46/2] << 2) >> 2;
	rgboffs[1][1] = (INT8)(mixer_control[which][0x48/2] << 2) >> 2;
	rgboffs[1][2] = (INT8)(mixer_control[which][0x4a/2] << 2) >> 2;
	rgboffs[2][0] = 0;
	rgboffs[2][1] = 0;
	rgboffs[2][2] = 0;

	/* determine the sprite grouping parameters first */
	switch (mixer_control[which][0x4c/2] & 0x0f)
	{
		default:
		case 0x0:	sprgroup_shift = 14;	sprgroup_mask = 0x00;	sprgroup_or = 0x01;	break;
		case 0x1:	sprgroup_shift = 14;	sprgroup_mask = 0x01;	sprgroup_or = 0x02;	break;
		case 0x2:	sprgroup_shift = 13;	sprgroup_mask = 0x03;	sprgroup_or = 0x04;	break;
		case 0x3:	sprgroup_shift = 12;	sprgroup_mask = 0x07;	sprgroup_or = 0x08;	break;

		case 0x4:	sprgroup_shift = 14;	sprgroup_mask = 0x01;	sprgroup_or = 0x00;	break;
		case 0x5:	sprgroup_shift = 13;	sprgroup_mask = 0x03;	sprgroup_or = 0x00;	break;
		case 0x6:	sprgroup_shift = 12;	sprgroup_mask = 0x07;	sprgroup_or = 0x00;	break;
		case 0x7:	sprgroup_shift = 11;	sprgroup_mask = 0x0f;	sprgroup_or = 0x00;	break;

		case 0x8:	sprgroup_shift = 14;	sprgroup_mask = 0x01;	sprgroup_or = 0x00;	break;
		case 0x9:	sprgroup_shift = 13;	sprgroup_mask = 0x03;	sprgroup_or = 0x00;	break;
		case 0xa:	sprgroup_shift = 12;	sprgroup_mask = 0x07;	sprgroup_or = 0x00;	break;
		case 0xb:	sprgroup_shift = 11;	sprgroup_mask = 0x0f;	sprgroup_or = 0x00;	break;

		case 0xc:	sprgroup_shift = 13;	sprgroup_mask = 0x01;	sprgroup_or = 0x00;	break;
		case 0xd:	sprgroup_shift = 12;	sprgroup_mask = 0x03;	sprgroup_or = 0x00;	break;
		case 0xe:	sprgroup_shift = 11;	sprgroup_mask = 0x07;	sprgroup_or = 0x00;	break;
		case 0xf:	sprgroup_shift = 10;	sprgroup_mask = 0x0f;	sprgroup_or = 0x00;	break;
	}
	sprshadowmask = (mixer_control[which][0x4c/2] & 0x04) ? 0x8000 : 0x0000;
	sprpixmask = ((1 << sprgroup_shift) - 1) & 0x3fff;
	sprshadow = 0x7ffe & sprpixmask;

	/* extract info about TEXT, NBG0-3, and BITMAP layers, which all follow the same pattern */
	numlayers = 0;
	for (laynum = MIXER_LAYER_TEXT; laynum <= MIXER_LAYER_BITMAP; laynum++)
	{
		int priority = mixer_control[which][0x20/2 + laynum] & 0x0f;
		if ((enablemask & (1 << laynum)) && priority != 0)
		{
			layersort[numlayers].index = laynum;
			layersort[numlayers].effpri = (priority << 3) | (6 - laynum);
			layersort[numlayers].palbase = (mixer_control[which][0x20/2 + laynum] & 0x00f0) << 6;
			layersort[numlayers].mixshift = (mixer_control[which][0x20/2 + laynum] >> 8) & 3;
			layersort[numlayers].blendmask = blendenable ? ((mixer_control[which][0x30/2 + laynum] >> 6) & 0xff) : 0;
			layersort[numlayers].sprblendmask = compute_sprite_blend(mixer_control[which][0x30/2 + laynum] & 0x3f);
			layersort[numlayers].coloroffs = compute_color_offsets(which, (mixer_control[which][0x3e/2] >> laynum) & 1, (mixer_control[which][0x30/2 + laynum] >> 14) & 1);
			numlayers++;
		}
	}

	/* extract info about the BACKGROUND layer */
	layersort[numlayers].index = MIXER_LAYER_BACKGROUND;
	layersort[numlayers].effpri = (1 << 3) | 0;
	layersort[numlayers].palbase = (mixer_control[which][0x2c/2] & 0x00f0) << 6;
	layersort[numlayers].mixshift = (mixer_control[which][0x2c/2] >> 8) & 3;
	layersort[numlayers].blendmask = 0;
	layersort[numlayers].sprblendmask = 0;
	layersort[numlayers].coloroffs = compute_color_offsets(which, (mixer_control[which][0x3e/2] >> 8) & 1, (mixer_control[which][0x3e/2] >> 14) & 1);
	numlayers++;

	/* now bubble sort the list by effective priority */
	for (laynum = 0; laynum < numlayers; laynum++)
		for (i = laynum + 1; i < numlayers; i++)
			if (layersort[i].effpri > layersort[laynum].effpri)
			{
				struct mixer_layer_info temp = layersort[i];
				layersort[i] = layersort[laynum];
				layersort[laynum] = temp;
			}

	/* for each possible sprite group, insert the sprites into the list at the appropriate point */
	for (groupnum = 0; groupnum <= sprgroup_mask; groupnum++)
	{
		int effgroup = sprgroup_or | groupnum;
		int priority = mixer_control[which][0x00/2 + effgroup] & 0x0f;
		int effpri = (priority << 3) | 7;
		int sprindex = numlayers;
		int dstnum = 0;

		/* make a copy of the sorted list, finding a location for the sprite entry */
		for (laynum = 0; laynum < numlayers; laynum++)
		{
			if (effpri > layersort[laynum].effpri && sprindex == numlayers)
				sprindex = dstnum++;
			layerorder[groupnum][dstnum++] = layersort[laynum];
		}

		/* build the sprite entry */
		layerorder[groupnum][sprindex].index = MIXER_LAYER_SPRITES;
		layerorder[groupnum][sprindex].effpri = effpri;
		if ((mixer_control[which][0x4c/2] & 3) != 3)
			layerorder[groupnum][sprindex].palbase = (mixer_control[which][0x00/2 + effgroup] & 0x00f0) << 6;
		else
			layerorder[groupnum][sprindex].palbase = (mixer_control[which][0x4c/2] & 0x00f0) << 6;
		layerorder[groupnum][sprindex].mixshift = (mixer_control[which][0x00/2 + effgroup] >> 8) & 3;
		layerorder[groupnum][sprindex].blendmask = 0;
		layerorder[groupnum][sprindex].sprblendmask = 0;
		layerorder[groupnum][sprindex].coloroffs = compute_color_offsets(which, (mixer_control[which][0x3e/2] >> 6) & 1, (mixer_control[which][0x4c/2] >> 15) & 1);
	}
/*
{
    static const char *layname[] = { "TEXT", "NBG0", "NBG1", "NBG2", "NBG3", "BITM", "SPRI", "LINE" };
    for (groupnum = 0; groupnum <= sprgroup_mask; groupnum++)
    {
        printf("%X: ", groupnum);
        for (i = 0; i <= numlayers; i++)
            printf("%s(%02X) ", layname[layerorder[groupnum][i].index], layerorder[groupnum][i].effpri);
        printf("\n");
    }
}*/

	/* based on the sprite controller flip bits, the data is scanned to us in different */
	/* directions; account for this */
	if (sprite_control_latched[0x04/2] & 1)
	{
		sprx_start = cliprect->max_x;
		sprdx = -1;
	}
	else
	{
		sprx_start = cliprect->min_x;
		sprdx = 1;
	}

	if (sprite_control_latched[0x04/2] & 2)
	{
		spry = cliprect->max_y;
		sprdy = -1;
	}
	else
	{
		spry = cliprect->min_y;
		sprdy = 1;
	}

	/* loop over rows */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++, spry += sprdy)
	{
		UINT32 *dest = (UINT32 *)bitmap->line[y] + xoffs;
		UINT16 *layerbase[8];

		/* get the starting address for each layer */
		layerbase[MIXER_LAYER_TEXT] = get_layer_scanline(MIXER_LAYER_TEXT, y);
		layerbase[MIXER_LAYER_NBG0] = get_layer_scanline(MIXER_LAYER_NBG0, y);
		layerbase[MIXER_LAYER_NBG1] = get_layer_scanline(MIXER_LAYER_NBG1, y);
		layerbase[MIXER_LAYER_NBG2] = get_layer_scanline(MIXER_LAYER_NBG2, y);
		layerbase[MIXER_LAYER_NBG3] = get_layer_scanline(MIXER_LAYER_NBG3, y);
		layerbase[MIXER_LAYER_BITMAP] = get_layer_scanline(MIXER_LAYER_BITMAP, y);
		layerbase[MIXER_LAYER_SPRITES] = get_layer_scanline(MIXER_LAYER_SPRITES, spry);
		layerbase[MIXER_LAYER_BACKGROUND] = get_layer_scanline(MIXER_LAYER_BACKGROUND, y);

		/* loop over columns */
		for (x = cliprect->min_x, sprx = sprx_start; x <= cliprect->max_x; x++, sprx += sprdx)
		{
			struct mixer_layer_info *first;
			int *rgbdelta;
			int firstpix;
			int sprpix, sprgroup;
			int r, g, b;
			int shadow = 0;

			/* first grab the current sprite pixel and determine the group */
			sprpix = layerbase[MIXER_LAYER_SPRITES][sprx];
			sprgroup = (sprpix >> sprgroup_shift) & sprgroup_mask;

			/* now scan the layers to find the topmost non-transparent pixel */
			for (first = &layerorder[sprgroup][0]; ; first++)
			{
				laynum = first->index;

				/* non-sprite layers are treated similarly */
				if (laynum != MIXER_LAYER_SPRITES)
				{
					firstpix = layerbase[laynum][x] & 0x1fff;
					if (firstpix != 0 || laynum == MIXER_LAYER_BACKGROUND)
						break;
				}

				/* sprite layers are special */
				else
				{
					firstpix = sprpix;
					shadow = ~firstpix & sprshadowmask;
					if ((firstpix & 0x7fff) != 0x7fff)
					{
						firstpix &= sprpixmask;
						if ((firstpix & 0x7ffe) != sprshadow)
							break;
						shadow = 1;
					}
				}
			}

			/* adjust the first pixel */
			firstpix = system32_paletteram[which][(first->palbase + ((firstpix >> first->mixshift) & 0xfff0) + (firstpix & 0x0f)) & 0x3fff];

			/* compute R, G, B */
			rgbdelta = &rgboffs[first->coloroffs][0];
			r = ((firstpix >>  0) & 0x1f) + rgbdelta[0];
			g = ((firstpix >>  5) & 0x1f) + rgbdelta[1];
			b = ((firstpix >> 10) & 0x1f) + rgbdelta[2];

			/* if there are potential blends, keep looking */
			if (first->blendmask != 0)
			{
				struct mixer_layer_info *second;
				int secondpix;

				/* now scan the layers to find the topmost non-transparent pixel */
				for (second = first + 1; ; second++)
				{
					laynum = second->index;

					/* non-sprite layers are treated similarly */
					if (laynum != MIXER_LAYER_SPRITES)
					{
						secondpix = layerbase[laynum][x] & 0x1fff;
						if (secondpix != 0 || laynum == MIXER_LAYER_BACKGROUND)
							break;
					}

					/* sprite layers are special */
					else
					{
						secondpix = sprpix;
						shadow = ~secondpix & sprshadowmask;
						if ((secondpix & 0x7fff) != 0x7fff)
						{
							secondpix &= sprpixmask;
							if ((secondpix & 0x7ffe) != sprshadow)
								break;
							shadow = 1;
						}
					}
				}

				/* are we blending with that layer? */
				if ((first->blendmask & (1 << laynum)) &&
					(laynum != MIXER_LAYER_SPRITES || (first->sprblendmask & (1 << sprgroup))))
				{
					/* adjust the second pixel */
					secondpix = system32_paletteram[which][(second->palbase + ((secondpix >> second->mixshift) & 0xfff0) + (secondpix & 0x0f)) & 0x3fff];

					/* compute first RGB */
					r *= 7 - blendfactor;
					g *= 7 - blendfactor;
					b *= 7 - blendfactor;

					/* add in second RGB */
					rgbdelta = &rgboffs[second->coloroffs][0];
					r += (((secondpix >>  0) & 0x1f) + rgbdelta[0]) * (blendfactor + 1);
					g += (((secondpix >>  5) & 0x1f) + rgbdelta[1]) * (blendfactor + 1);
					b += (((secondpix >> 10) & 0x1f) + rgbdelta[2]) * (blendfactor + 1);

					/* shift off the extra bits */
					r >>= 3;
					g >>= 3;
					b >>= 3;
				}
			}

			/* apply shadow/hilight */
			if (shadow)
			{
				r >>= 1;
				g >>= 1;
				b >>= 1;
			}

			/* clamp and combine */
			if (r > 31)
				firstpix = 31 << (16+3);
			else if (r > 0)
				firstpix = r << (16+3);
			else
				firstpix = 0;

			if (g > 31)
				firstpix |= 31 << (8+3);
			else if (g > 0)
				firstpix |= g << (8+3);

			if (b > 31)
				firstpix |= 31 << (0+3);
			else if (b > 0)
				firstpix |= b << (0+3);
			dest[x] = firstpix;
		}
	}

	/* if we are the second monitor on multi32, swap back the sprite layer */
	if (which == 1)
		layer_data[MIXER_LAYER_SPRITES] = temp_sprite_save;
}



/*************************************
 *
 *  Master update routine
 *
 *************************************/

static void print_mixer_data(int which)
{
#if PRINTF_MIXER_DATA
	static int count = 0;
	if (++count > 60 * 5)
	{
		printf("\n");
		printf("OP: %04X\n", system32_videoram[0x1ff8e/2]);
		printf("SC: %04X %04X %04X %04X - %04X %04X %04X %04X\n",
			sprite_control_latched[0x00],
			sprite_control_latched[0x01],
			sprite_control_latched[0x02],
			sprite_control_latched[0x03],
			sprite_control_latched[0x04],
			sprite_control_latched[0x05],
			sprite_control_latched[0x06],
			sprite_control_latched[0x07]);
		printf("00: %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X\n",
			mixer_control[which][0x00],
			mixer_control[which][0x01],
			mixer_control[which][0x02],
			mixer_control[which][0x03],
			mixer_control[which][0x04],
			mixer_control[which][0x05],
			mixer_control[which][0x06],
			mixer_control[which][0x07],
			mixer_control[which][0x08],
			mixer_control[which][0x09],
			mixer_control[which][0x0a],
			mixer_control[which][0x0b],
			mixer_control[which][0x0c],
			mixer_control[which][0x0d],
			mixer_control[which][0x0e],
			mixer_control[which][0x0f]);
		printf("20: %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X\n",
			mixer_control[which][0x10],
			mixer_control[which][0x11],
			mixer_control[which][0x12],
			mixer_control[which][0x13],
			mixer_control[which][0x14],
			mixer_control[which][0x15],
			mixer_control[which][0x16],
			mixer_control[which][0x17],
			mixer_control[which][0x18],
			mixer_control[which][0x19],
			mixer_control[which][0x1a],
			mixer_control[which][0x1b],
			mixer_control[which][0x1c],
			mixer_control[which][0x1d],
			mixer_control[which][0x1e],
			mixer_control[which][0x1f]);
		printf("40: %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X\n",
			mixer_control[which][0x20],
			mixer_control[which][0x21],
			mixer_control[which][0x22],
			mixer_control[which][0x23],
			mixer_control[which][0x24],
			mixer_control[which][0x25],
			mixer_control[which][0x26],
			mixer_control[which][0x27],
			mixer_control[which][0x28],
			mixer_control[which][0x29],
			mixer_control[which][0x2a],
			mixer_control[which][0x2b],
			mixer_control[which][0x2c],
			mixer_control[which][0x2d],
			mixer_control[which][0x2e],
			mixer_control[which][0x2f]);
		count = 0;
	}
#endif
}

VIDEO_UPDATE( system32 )
{
	UINT8 enablemask;

	/* update the visible area */
	if (system32_videoram[0x1ff00/2] & 0x8000)
		set_visible_area(0, 52*8-1, 0, 28*8-1);
	else
		set_visible_area(0, 40*8-1, 0, 28*8-1);

	/* if the display is off, punt */
	if (!system32_displayenable[0])
	{
		fillbitmap(bitmap, get_black_pen(), cliprect);
		return;
	}

	/* update the tilemaps */
	profiler_mark(PROFILER_USER1);
	enablemask = update_tilemaps(cliprect);
	profiler_mark(PROFILER_END);

	/* debugging */
#if QWERTY_LAYER_ENABLE
	if (code_pressed(KEYCODE_Q)) enablemask = 0x01;
	if (code_pressed(KEYCODE_W)) enablemask = 0x02;
	if (code_pressed(KEYCODE_E)) enablemask = 0x04;
	if (code_pressed(KEYCODE_R)) enablemask = 0x08;
	if (code_pressed(KEYCODE_T)) enablemask = 0x10;
	if (code_pressed(KEYCODE_Y)) enablemask = 0x20;
#endif

	/* do the mixing */
	profiler_mark(PROFILER_USER3);
	mix_all_layers(0, 0, bitmap, cliprect, enablemask);
	profiler_mark(PROFILER_END);

#if LOG_SPRITES
{
	if (code_pressed(KEYCODE_L))
	{
		FILE *f = fopen("sprite.txt", "w");
		int x, y;

		for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
		{
			UINT16 *src = get_layer_scanline(MIXER_LAYER_SPRITES, y);
			for (x = Machine->visible_area.min_x; x <= Machine->visible_area.max_x; x++)
				fprintf(f, "%04X ", *src++);
			fprintf(f, "\n");
		}
		fclose(f);

		f = fopen("nbg0.txt", "w");
		for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
		{
			UINT16 *src = get_layer_scanline(MIXER_LAYER_NBG0, y);
			for (x = Machine->visible_area.min_x; x <= Machine->visible_area.max_x; x++)
				fprintf(f, "%04X ", *src++);
			fprintf(f, "\n");
		}
		fclose(f);

		f = fopen("nbg1.txt", "w");
		for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
		{
			UINT16 *src = get_layer_scanline(MIXER_LAYER_NBG1, y);
			for (x = Machine->visible_area.min_x; x <= Machine->visible_area.max_x; x++)
				fprintf(f, "%04X ", *src++);
			fprintf(f, "\n");
		}
		fclose(f);

		f = fopen("nbg2.txt", "w");
		for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
		{
			UINT16 *src = get_layer_scanline(MIXER_LAYER_NBG2, y);
			for (x = Machine->visible_area.min_x; x <= Machine->visible_area.max_x; x++)
				fprintf(f, "%04X ", *src++);
			fprintf(f, "\n");
		}
		fclose(f);

		f = fopen("nbg3.txt", "w");
		for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
		{
			UINT16 *src = get_layer_scanline(MIXER_LAYER_NBG3, y);
			for (x = Machine->visible_area.min_x; x <= Machine->visible_area.max_x; x++)
				fprintf(f, "%04X ", *src++);
			fprintf(f, "\n");
		}
		fclose(f);
	}
}
#endif

#if SHOW_ALPHA
{
	static const char *layername[] = { "TEXT ", "NBG0 ", "NBG1 ", "NBG2 ", "NBG3 ", "BITMAP " };
	char temp[100];
	int count = 0, i;
	sprintf(temp, "ALPHA(%d):", (mixer_control[which][0x4e/2] >> 8) & 7);
	for (i = 0; i < 6; i++)
		if (enablemask & (1 << i))
			if ((mixer_control[which][0x30/2 + i] & 0x1010) == 0x1010)
			{
				count++;
				strcat(temp, layername[i]);
			}
	if (count)
		usrintf_showmessage("%s", temp);
}
#endif

#if SHOW_CLIPS
{
	int showclip = -1;

//  if (code_pressed(KEYCODE_V))
//      showclip = 0;
//  if (code_pressed(KEYCODE_B))
//      showclip = 1;
//  if (code_pressed(KEYCODE_N))
//      showclip = 2;
//  if (code_pressed(KEYCODE_M))
//      showclip = 3;
//  if (showclip != -1)
for (showclip = 0; showclip < 4; showclip++)
	{
		int flip = (system32_videoram[0x1ff00/2] >> 9) & 1;
		int clips = (system32_videoram[0x1ff06/2] >> (4 * showclip)) & 0x0f;
		if (((system32_videoram[0x1ff02/2] >> (11 + showclip)) & 1) && clips)
		{
			int i, x, y;
			for (i = 0; i < 4; i++)
				if (clips & (1 << i))
				{
					struct rectangle rect;
					pen_t white = Machine->uifont->colortable[1];
					if (!flip)
					{
						rect.min_x = system32_videoram[0x1ff60/2 + i * 4] & 0x1ff;
						rect.min_y = system32_videoram[0x1ff62/2 + i * 4] & 0x0ff;
						rect.max_x = (system32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1;
						rect.max_y = (system32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1;
					}
					else
					{
						rect.max_x = (Machine->visible_area.max_x + 1) - (system32_videoram[0x1ff60/2 + i * 4] & 0x1ff);
						rect.max_y = (Machine->visible_area.max_y + 1) - (system32_videoram[0x1ff62/2 + i * 4] & 0x0ff);
						rect.min_x = (Machine->visible_area.max_x + 1) - ((system32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1);
						rect.min_y = (Machine->visible_area.max_y + 1) - ((system32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1);
					}
					sect_rect(&rect, &Machine->visible_area);

					if (rect.min_y <= rect.max_y && rect.min_x <= rect.max_x)
					{
						for (y = rect.min_y; y <= rect.max_y; y++)
						{
							bitmap->plot(bitmap, rect.min_x, y, white);
							bitmap->plot(bitmap, rect.max_x, y, white);
						}
						for (x = rect.min_x; x <= rect.max_x; x++)
						{
							bitmap->plot(bitmap, x, rect.min_y, white);
							bitmap->plot(bitmap, x, rect.max_y, white);
						}
					}
				}
		}
	}
}
#endif

	print_mixer_data(0);
}


VIDEO_UPDATE( multi32 )
{
	struct rectangle clipleft, clipright;
	UINT8 enablemask;
  
	int monitor_setting;
	int monitor_display_start;
	int monitor_display_width;
	int monitor_vertical_offset;

/*
   MAME2003-PLUS uses a single screen to draw to where as current mame
   uses dedicated left and right screens. The machine driver doubles the
   y max to allow for this so we must manually set our y clip max to the
   actual maximum for drawing purposes.
*/
	monitor_setting=readinputport(0xf);
	monitor_vertical_offset=1;
	monitor_display_start=0;
	if (monitor_setting==2) monitor_display_start=1;
	if (monitor_setting==3) {
		monitor_vertical_offset=2;
		monitor_display_width=2;
  }
  else
		monitor_display_width=1+monitor_display_start;

	/* update the visible area */
	if (system32_videoram[0x1ff00/2] & 0x8000)
	{
		set_visible_area(52*monitor_display_start*8, 52*8*monitor_display_width-1, 0, 28*8*monitor_vertical_offset-1);
		clipleft.min_x = 0;
		clipleft.max_x = 52*8-1;
		clipright.min_x = 52*8;
		clipright.max_x = 52*2*8-1;
	}
	else
	{
		set_visible_area(40*monitor_display_start*8, 40*8*monitor_display_width-1, 0, 28*8*monitor_vertical_offset-1);
		clipleft.min_x = 0;
		clipleft.max_x = 40*8-1;
		clipright.min_x = 40*8;
		clipright.max_x = 40*2*8-1;
	}
	clipleft.min_y = clipright.min_y = cliprect->min_y;
	clipleft.max_y = clipright.max_y = 28*8-1;

	/* if the display is off, punt */
	if (!system32_displayenable[0] && !system32_displayenable[1])
	{
		fillbitmap(bitmap, get_black_pen(), cliprect);
		return;
	}

	/* update the tilemaps */
	profiler_mark(PROFILER_USER1);
	enablemask = update_tilemaps(&clipleft);
	profiler_mark(PROFILER_END);

	/* debugging */
#if QWERTY_LAYER_ENABLE
	if (code_pressed(KEYCODE_Q)) enablemask = 0x01;
	if (code_pressed(KEYCODE_W)) enablemask = 0x02;
	if (code_pressed(KEYCODE_E)) enablemask = 0x04;
	if (code_pressed(KEYCODE_R)) enablemask = 0x08;
	if (code_pressed(KEYCODE_T)) enablemask = 0x10;
	if (code_pressed(KEYCODE_Y)) enablemask = 0x20;
#endif

	/* do the mixing */
	profiler_mark(PROFILER_USER3);
	if (system32_displayenable[0])
		mix_all_layers(0, 0, bitmap, &clipleft, enablemask);
	else
		fillbitmap(bitmap, get_black_pen(), &clipleft);
	if (system32_displayenable[1])
		mix_all_layers(1, clipright.min_x, bitmap, &clipleft, enablemask);
	else
		fillbitmap(bitmap, get_black_pen(), &clipright);
	profiler_mark(PROFILER_END);

	if (!code_pressed(KEYCODE_M)) print_mixer_data(0);
	else print_mixer_data(1);
#if LOG_SPRITES
{
	if (code_pressed(KEYCODE_L))
	{
		FILE *f = fopen("sprite.txt", "w");
		int x, y;

		for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
		{
			UINT16 *src = get_layer_scanline(MIXER_LAYER_SPRITES, y);
			for (x = Machine->visible_area.min_x; x <= Machine->visible_area.max_x; x++)
				fprintf(f, "%04X ", *src++);
			fprintf(f, "\n");
		}
		fclose(f);
	}
}
#endif

}


/*

Blending registers:

?--- ---- ---- ----  Unknown
-c-- ---- ---- ----  Color selection
--l- ---- ---- ----  1= blend with line color
---s ---- ---- ----  1= blend with sprites, under certain conditions
---- b--- ---- ----  1= blend with bitmap
---- -3-- ---- ----  1= blend with NBG3
---- --2- ---- ----  1= blend with NBG2
---- ---1 ---- ----  1= blend with NBG1
---- ---- 0--- ----  1= blend with NBG0
---- ---- -t-- ----  1= blend with text
---- ---- --mm ----  sprite priority comparison (see below)
---- ---- ---- vvvv  sprite priority comparison value

If sprite blending is enabled, blending is only performed if the
underlying sprite pixel's group priority matches certain criteria.
These criteria are specified in the low 6 bits of the register.
If SPGP refers to the sprite pixel group prioity, then:

   if (mm == 00) blending is performed only if (vvvv == SPGP)
   if (mm == 01) blending is performed only if (vvvv >= SPGP)
   if (mm == 10) blending is performed only if (vvvv <= SPGP)
   if (mm == 11) blending is performed regardless of SPGP



    equal priority order =

        sprite
        text
        nbg0
        nbg1
        nbg2
        nbg3
        bitmap
        line



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

harddunk:
SC: 0003 0000 0000 0000 - 0001 0001 0000 0000
00: 000A 0006 000E 000A - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 036D 0000 036C - 0000 0074 0072 0070 - 0000 4000 4000 4000 - 4000 0000 0000 4000
40: 0000 0000 0000 0000 - 0000 0000 9E05 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000

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

orunners:
SC: 0003 0000 0000 0000 - 0001 0003 0000 0000
00: 000E 000A 0006 0002 - 000E 000A 0006 0002 - 000E 000A 0006 0002 - 000E 000A 0006 0002
20: 003C 0141 0000 0142 - 0000 0038 0030 0030 - 5030 4000 4000 4000 - 4000 4000 4000 C000
40: 0000 0000 0000 0000 - 0000 0000 BE4D 0A00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

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

scross:
SC: 0003 0000 0000 0002 - 0001 0001 0000 0000
00: 001B 0017 001D 001F - 0010 0010 0010 0010 - 0010 0010 0010 0010 - 0010 0010 0010 0010
20: 000E 015A 0000 0156 - 0000 0000 0000 0000 - 4000 4000 4000 4000 - 4000 4000 4000 C000
40: 0000 0000 0000 0000 - 0000 0000 BE1D 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000

slipstrm:
SC: 0003 0000 00FC 0002 - 0001 0003 0001 0000
00: 004D 004D 004D 004A - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 0108 010E 010B - 0109 0000 0000 0000 - 4030 4030 4030 7C90 - 4030 4000 0000 C040
40: 0000 0000 0000 001F - 001F 001F 7F0D 0C00 - 0000 0000 0000 0000 - 0000 0000 0000 0000

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

titlef:
SC: 0003 0000 0000 0000 - 0001 0001 0000 0000
00: 000E 0002 0005 000F - 0000 0000 0000 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000
20: 007F 0366 0000 0364 - 0000 0070 0071 0070 - 0000 4000 4000 4000 - 4000 0000 0000 4000
40: 0000 0000 0000 0000 - 0000 0000 9E05 0000 - 0000 0000 0000 0000 - 0000 0000 0000 0000

====
back layer setups (register $31ff5e):
alien3:   $0200
arabfgt:  $8000-$81ff -- depending on the scene
arescue:  $0200
as1:      (untested)
brival:   $8000
darkedge: $0200
dbzvrvs:  $0200
f1en:     $0000
f1lap:    $0000
ga2:      $0200
harddunk: $8200
holo:     $0200
jpark:    $0200
kokoroj:  (untested)
kokoroj2: $8000 --
          $8000-$81fc (in steps of 4) -- on introduction/initials scenes
orunners: $0200
radm:     $0200
radr:     $8200 -- gameplay
          $0200 -- title screen
scross:   $0200
slipstrm: $0000
sonic:    $0000 -- on sega logo/title screen
          $0200 -- everything else
spidman:  $0200
svf:      $0201 -- on attract
          $0200 -- on gameplay
titlef:   $8200

*/
