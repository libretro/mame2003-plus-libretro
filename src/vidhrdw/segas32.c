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

data8_t sys32_ramtile_dirty[0x1000];



/*************************************
 *
 *  Debugging
 *
 *************************************/

#define SHOW_CLIPS				0
#define QWERTY_LAYER_ENABLE		1
#define PRINTF_MIXER_DATA		1
#define SHOW_ALPHA				0



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



/*************************************
 *
 *  Type definitions
 *
 *************************************/

struct layer_info
{
	struct mame_bitmap *	bitmap;
	UINT16					trans_mask;
	UINT8 *					transparent;
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
static struct layer_info layer_data[9];

/* sprite data */
static UINT8 sprite_render_count;
static UINT8 sprite_control_latched[8];
static UINT8 sprite_control[8];
static data32_t *spriteram_32bit;



/*************************************
 *
 *  Prototypes
 *
 *************************************/

static void sprite_erase_buffer(void);
static void sprite_swap_buffers(void);
static void sprite_render_list(void);




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

	/* allocate a copy of spriteram in 32-bit format */
	spriteram_32bit = auto_malloc ( 0x20000 );

	/* allocate the tilemaps */
	for (tmap = 0; tmap < 0x80; tmap++)
	{
		tilemap[tmap] = tilemap_create(get_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16,16, 32,16);
		tilemap_set_transparent_pen(tilemap[tmap], 0);
		tilemap_set_user_data(tilemap[tmap], &sys32_videoram[0x200 * tmap]);
	}

	/* allocate the bitmaps */
	for (tmap = 0; tmap < 9; tmap++)
	{
		layer_data[tmap].bitmap = auto_bitmap_alloc_depth(416, 224, 16);
		layer_data[tmap].transparent = auto_malloc(sizeof(layer_data[tmap].transparent[0]) * 256);
		memset(layer_data[tmap].transparent, 0, sizeof(layer_data[tmap].transparent[0]) * 256);
	}

/*
{
    int tilebanks = Machine->gfx[0]->total_elements / 0x2000;
    int i,j,b,x,y;

    for (b = 0; b < tilebanks; b++)
    {
        int best[16], best_count[16];

        printf("Tilebank %d:\n", b);

        for (i = 0; i < 16; i++)
            best[i] = best_count[i] = 0;

        for (j = 0; j < 0x2000; j++)
        {
            for (i = 0; i < 16; i++)
            {
                UINT8 *base = Machine->gfx[0]->gfxdata + (b*0x2000+j) * Machine->gfx[0]->char_modulo;
                int count = 0;
                for (y = 0; y < 16*16; y++)
                    if (base[y] == i)
                        count++;
                if (count > best_count[i])
                {
                    best_count[i] = count;
                    best[i] = j;
                }
            }
        }

        for (i = 0; i < 16; i++)
            printf("   %X: %04X (%d/256)\n", i, best[i], best_count[i]);
    }
}
*/
	return 0;
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
	value |= ((b & 0x1e) << 7) | ((g & 0x1e) << 3) | ((b & 0x1e) >> 1);
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


WRITE16_HANDLER( system32_spriteram_w )
{
	COMBINE_DATA(&system32_spriteram[offset]);
	spriteram_32bit[offset/2] =
		((system32_spriteram[offset |  1] >> 8 ) & 0x000000ff) |
		((system32_spriteram[offset |  1] << 8 ) & 0x0000ff00) |
		((system32_spriteram[offset & ~1] << 8 ) & 0x00ff0000) |
		((system32_spriteram[offset & ~1] << 24) & 0xff000000);
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

static int compute_clipping_extents(int enable, int clipout, int clipmask, const struct rectangle *cliprect, struct extents_list *list)
{
	int flip = (sys32_videoram[0x1ff00/2] >> 9) & 1;
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
	struct extents_list clip_extents;
	UINT32 srcx, srcx_start, srcy;
	UINT32 srcxstep, srcystep;
	int dstxstep, dstystep;
	int flip, opaque;
	int x, y;

	/* configure the layer */
	layer->trans_mask = 0x0f;
	opaque = 0;
//opaque = (sys32_videoram[0x1ff8e/2] >> (8 + bgnum)) & 1;
//if (code_pressed(KEYCODE_Z) && bgnum == 0) opaque = 1;
//if (code_pressed(KEYCODE_X) && bgnum == 1) opaque = 1;
//if (opaque)
//{
//  layer->trans_mask = 0xffff;
//  opaque = 0x8000;
//}

	/* determine if we're flipped */
	flip = ((sys32_videoram[0x1ff00/2] >> 9) ^ (sys32_videoram[0x1ff00/2] >> bgnum)) & 1;

	/* determine the clipping */
	clipenable = (sys32_videoram[0x1ff02/2] >> (11 + bgnum)) & 1;
	clipout = (sys32_videoram[0x1ff02/2] >> (6 + bgnum)) & 1;
	clips = (sys32_videoram[0x1ff06/2] >> (4 * bgnum)) & 0x0f;
	clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

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
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
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
						UINT16 pix = src[(srcx >> 29) & 1][(srcx >> 20) & 0x1ff] | opaque;
						srcx += srcxstep;
						dst[x] = pix;
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
			layer->transparent[y] = 0;
		}
		else if (!layer->transparent[y])
		{
			memset(&dst[cliprect->min_x], 0, (cliprect->max_x - cliprect->min_x + 1) * sizeof(dst[0]));
			layer->transparent[y] = 1;
		}

		/* advance in Y */
		srcy += srcystep;
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
	struct extents_list clip_extents;
	int rowscroll, rowselect;
	int xscroll, yscroll;
	UINT16 *table;
	int srcx, srcy;
	int flip, opaque;
	int x, y;

	/* configure the layer */
	layer->trans_mask = 0x0f;
	opaque = 0;
//opaque = (sys32_videoram[0x1ff8e/2] >> (8 + bgnum)) & 1;
//if (code_pressed(KEYCODE_C) && bgnum == 2) opaque = 1;
//if (code_pressed(KEYCODE_V) && bgnum == 3) opaque = 1;
//
//if (opaque)
//{
//  layer->trans_mask = 0xffff;
//  opaque = 0x8000;
//}

	/* determine if we're flipped */
	flip = ((sys32_videoram[0x1ff00/2] >> 9) ^ (sys32_videoram[0x1ff00/2] >> bgnum)) & 1;

	/* determine the clipping */
	clipenable = (sys32_videoram[0x1ff02/2] >> (11 + bgnum)) & 1;
	clipout = (sys32_videoram[0x1ff02/2] >> (6 + bgnum)) & 1;
	clips = (sys32_videoram[0x1ff06/2] >> (4 * bgnum)) & 0x0f;
	clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

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
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
			UINT16 *src[2];
			UINT16 pages;
			int srcxstep = 1;

			/* get starting scroll values */
			srcx = cliprect->min_x + xscroll;
			srcy = yscroll + y;

			/* apply row scroll/select before flipping */
			if (rowscroll)
				srcx += table[0x000 + 0x100 * (bgnum - 2) + y] & 0x3ff;
			if (rowselect)
				srcy = (yscroll + table[0x200 + 0x100 * (bgnum - 2) + y]) & 0x1ff;

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
						UINT16 pix = src[(srcx >> 9) & 1][srcx & 0x1ff] | opaque;
						dst[x] = pix;
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
			layer->transparent[y] = 0;
		}
		else if (!layer->transparent[y])
		{
			memset(&dst[cliprect->min_x], 0, (cliprect->max_x - cliprect->min_x + 1) * sizeof(dst[0]));
			layer->transparent[y] = 1;
		}
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
	UINT16 *tilebase;
	UINT16 *gfxbase;
	int startx, starty;
	int endx, endy;
	int x, y, iy;
	int flip;

	/* configure the layer */
	layer->trans_mask = 0x0f;

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

				/* loop over rows */
				for (iy = 0; iy < 8; iy++)
				{
					UINT16 pix = *src++;
					dst[0] = ((pix >>  4) & 0x0f) + color;
					dst[1] = ((pix >>  0) & 0x0f) + color;
					dst[2] = ((pix >> 12) & 0x0f) + color;
					dst[3] = ((pix >>  8) & 0x0f) + color;

					pix = *src++;
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

				/* loop over rows */
				for (iy = 0; iy < 8; iy++)
				{
					UINT16 pix = *src++;
					dst[ 0] = ((pix >>  4) & 0x0f) + color;
					dst[-1] = ((pix >>  0) & 0x0f) + color;
					dst[-2] = ((pix >> 12) & 0x0f) + color;
					dst[-3] = ((pix >>  8) & 0x0f) + color;

					pix = *src++;
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
	struct extents_list clip_extents;
	int xscroll, yscroll;
	UINT16 color;
	int x, y;
	int bpp;

	/* configure the layer */
	bpp = (sys32_videoram[0x1ff00/2] & 0x0800) ? 8 : 4;
	layer->trans_mask = (1 << bpp) - 1;

	/* determine the clipping */
	clipenable = (sys32_videoram[0x1ff02/2] >> 15) & 1;
	clipout = (sys32_videoram[0x1ff02/2] >> 10) & 1;
	clips = 0x10;
	clipdraw_start = compute_clipping_extents(clipenable, clipout, clips, cliprect, &clip_extents);

	/* determine x/y scroll */
	xscroll = sys32_videoram[0x1ff88/2] & 0x1ff;
	yscroll = sys32_videoram[0x1ff8a/2] & 0x1ff;
	color = (sys32_videoram[0x1ff8c/2] << 4) & 0x1fff0 & ~layer->trans_mask;

	/* loop over target rows */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *extents = &clip_extents.extent[clip_extents.scan_extent[y]][0];
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		int clipdraw = clipdraw_start;

		/* optimize for the case where we are clipped out */
		if (clipdraw || extents[1] <= cliprect->max_x)
		{
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
			layer->transparent[y] = 0;
		}
		else if (!layer->transparent[y])
		{
			memset(&dst[cliprect->min_x], 0, (cliprect->max_x - cliprect->min_x + 1) * sizeof(dst[0]));
			layer->transparent[y] = 1;
		}
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
		UINT16 color;

		/* determine the color */
		if (sys32_videoram[0x1ff5e/2] & 0x8000)
			color = (sys32_videoram[0x1ff5e/2] & 0x1fff) + y;
		else
			color = sys32_videoram[0x1ff5e/2] & 0x1e00;

		/* if the color doesn't match, fill */
		if (dst[cliprect->min_x] != color)
			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
				dst[x] = color;
	}
}


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

	/* configure the layer */
	layer_data[MIXER_LAYER_SPRITES].trans_mask = 0xffff;
}


static void sprite_swap_buffers(void)
{
	/* swap between the two sprite buffers */
	struct layer_info temp;
	temp = layer_data[MIXER_LAYER_SPRITES];
	layer_data[MIXER_LAYER_SPRITES] = layer_data[MIXER_LAYER_SPRITES_2];
	layer_data[MIXER_LAYER_SPRITES_2] = temp;

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
		pix != 0 && pix != trans) 											\
	{																		\
		if (!shadow || pix != 0x0e)											\
			dest[x] = indtable[pix];										\
		else																\
			dest[x] = 0x7fff;												\
	}

#define sprite_draw_pixel_256(trans)										\
	/* only draw if onscreen, not 0 or 15 */								\
	if (x >= clipin->min_x && x <= clipin->max_x && 						\
		(!do_clipout || x < clipout->min_x || x > clipout->max_x) &&		\
		pix != 0 && pix != trans) 											\
	{																		\
		if (!shadow || pix != 0xfe)											\
			dest[x] = indtable[pix >> 4] | (pix & 0x0f);					\
		else																\
			dest[x] = 0x7fff;												\
	}

static int draw_one_sprite(UINT16 *data, int xoffs, int yoffs, const struct rectangle *clipin, const struct rectangle *clipout)
{
	struct mame_bitmap *bitmap = layer_data[MIXER_LAYER_SPRITES_2].bitmap;
	UINT8 numbanks = memory_region_length(REGION_GFX2) / 0x400000;
	const UINT32 *spritebase = (const UINT32 *)memory_region(REGION_GFX2);

	int indirect = data[0] & 0x2000;
	int indlocal = data[0] & 0x1000;
//  int shadow   = data[0] & 0x0800;
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
	int bank     = ((data[3] & 0x0800) >> 11) | ((data[3] & 0x4000) >> 13);
	int dsth     = data[2] & 0x3ff;
	int dstw     = data[3] & 0x3ff;
	int ypos     = (INT16)(data[4] << 4) >> 4;
	int xpos     = (INT16)(data[5] << 4) >> 4;
	UINT32 addr  = data[6] | ((data[2] & 0xf000) << 4);
	int hzoom, vzoom;
	int xdelta = 1, ydelta = 1;
	int x, y, xtarget, ytarget, yacc = 0, pix, shadow = 0;
	const UINT32 *spritedata;
	UINT32 addrmask, curaddr;
	UINT16 indtable[16];

	/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
	if (srcw == 0 || srch == 0 || dstw == 0 || dsth == 0)
		goto bail;

	/* create the local palette for the indirect case */
	if (indirect)
	{
		UINT16 *src = indlocal ? &data[8] : &system32_spriteram[8 * (data[7] & 0x1fff)];
		UINT16 mask = bpp8 ? 0x7ff0 : 0x7fff;
		UINT16 shadowmask = bpp8 ? 0x7fe0 : 0x7ffe;
		for (x = 0; x < 16; x++)
			indtable[x] = ((src[x] & shadowmask) == shadowmask) ? 0x7fff : (src[x] & mask);
	}

	/* create the local palette for the direct case */
	else
	{
		shadow = bpp8 ? ((data[7] & 0x7f00) == 0x7f00) : ((data[7] & 0x7ff0) == 0x7ff0);
		for (x = 0; x < 16; x++)
			indtable[x] = bpp8 ? ((data[7] & 0x7f00) | (x << 4)) : ((data[7] & 0x7ff0) | x);
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
		spritedata = spritebase + 0x100000 * bank;
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
		case 3:	xpos -= dstw / 2; 	break;
		case 1: xpos -= dstw - 1;	break;
		case 2:						break;
	}

	/* adjust the starting Y position */
	if (offsety)
		ypos += yoffs;
	switch (adjusty)
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

	/* if we had an inline indirect palette, we skip two entries */
bail:
	return indlocal ? 2 : 0;
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
	outerclip.max_x = (sprite_control_latched[6] & 1) ? 415 : 319;
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
//printf("clipin=(%d,%d)-(%d,%d)\n", clipin.min_x, clipin.min_y, clipin.max_x, clipin.max_y);
				}

				/* set the exclusive cliprect */
				if (sprite[0] & 0x2000)
				{
					clipout.min_y = (INT16)(sprite[4] << 4) >> 4;
					clipout.max_y = (INT16)(sprite[5] << 4) >> 4;
					clipout.min_x = (INT16)(sprite[6] << 4) >> 4;
					clipout.max_x = (INT16)(sprite[7] << 4) >> 4;
//printf("clipout=(%d,%d)-(%d,%d)\n", clipout.min_x, clipout.min_y, clipout.max_x, clipout.max_y);
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

INLINE UINT8 compute_color_offsets(int layerbit, int layerflag)
{
	int mode = ((mixer_control[MONITOR][0x3e/2] & 0x8000) >> 14) | (layerbit & 1);

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

INLINE UINT16 compute_sprite_blend(UINT8 encoding)
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

/*
    equal priority order =

        sprite
        text
        nbg0
        nbg1
        nbg2
        nbg3
        bitmap
        line
*/


static void mix_all_layers(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT8 enablemask)
{
	int blendenable = mixer_control[MONITOR][0x4e/2] & 0x0800;
	int blendfactor = (mixer_control[MONITOR][0x4e/2] >> 8) & 7;
	struct layer_info
	{
		UINT16		palbase;			/* palette base from control reg */
		UINT16		sprblendmask;		/* mask of sprite priorities this layer blends with */
		UINT8		blendmask;			/* mask of layers this layer blends with */
		UINT8		index;				/* index of this layer (MIXER_LAYER_XXX) */
		UINT8		effpri;				/* effective priority = (priority << 3) | layer_priority */
		UINT8		mixshift;			/* shift from control reg */
		UINT8		coloroffs;			/* color offset index */
	} layerorder[16][8], layersort[8];
	UINT8 sprgroup_shift, sprgroup_mask, sprgroup_or;
	int numlayers, laynum, groupnum;
	int rgboffs[3][3];
	int sprpixmask;
	int sprx, spry, sprx_start;
	int sprdx, sprdy;
	int x, y, i;

	/* extract the RGB offsets */
	rgboffs[0][0] = (INT8)(mixer_control[MONITOR][0x40/2] << 2) >> 2;
	rgboffs[0][1] = (INT8)(mixer_control[MONITOR][0x42/2] << 2) >> 2;
	rgboffs[0][2] = (INT8)(mixer_control[MONITOR][0x44/2] << 2) >> 2;
	rgboffs[1][0] = (INT8)(mixer_control[MONITOR][0x46/2] << 2) >> 2;
	rgboffs[1][1] = (INT8)(mixer_control[MONITOR][0x48/2] << 2) >> 2;
	rgboffs[1][2] = (INT8)(mixer_control[MONITOR][0x4a/2] << 2) >> 2;
	rgboffs[2][0] = 0;
	rgboffs[2][1] = 0;
	rgboffs[2][2] = 0;

	/* determine the sprite grouping parameters first */
	switch (mixer_control[MONITOR][0x4c/2] & 0x0f)
	{
		default:
		case 0x0:	sprgroup_shift = 15;	sprgroup_mask = 0x00;	sprgroup_or = 0x01;	break;
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
	sprpixmask = ((1 << sprgroup_shift) - 1) & 0x3fff;

	/* extract info about TEXT, NBG0-3, and BITMAP layers, which all follow the same pattern */
	numlayers = 0;
	for (laynum = MIXER_LAYER_TEXT; laynum <= MIXER_LAYER_BITMAP; laynum++)
	{
		int priority = mixer_control[MONITOR][0x20/2 + laynum] & 0x0f;
		if ((enablemask & (1 << laynum)) && priority != 0)
		{
			layersort[numlayers].index = laynum;
			layersort[numlayers].effpri = (priority << 3) | (6 - laynum);
			layersort[numlayers].palbase = (mixer_control[MONITOR][0x20/2 + laynum] & 0x00f0) << 6;
			layersort[numlayers].mixshift = (mixer_control[MONITOR][0x20/2 + laynum] >> 8) & 3;
			layersort[numlayers].blendmask = blendenable ? ((mixer_control[MONITOR][0x30/2 + laynum] >> 6) & 0xff) : 0;
			layersort[numlayers].sprblendmask = compute_sprite_blend(mixer_control[MONITOR][0x30/2 + laynum] & 0x3f);
			layersort[numlayers].coloroffs = compute_color_offsets((mixer_control[MONITOR][0x3e/2] >> laynum) & 1, (mixer_control[MONITOR][0x30/2 + laynum] >> 14) & 1);
			numlayers++;
		}
	}

	/* extract info about the BACKGROUND layer */
	layersort[numlayers].index = MIXER_LAYER_BACKGROUND;
	layersort[numlayers].effpri = (1 << 3) | 0;
	layersort[numlayers].palbase = (mixer_control[MONITOR][0x2c/2] & 0x00f0) << 6;
	layersort[numlayers].mixshift = (mixer_control[MONITOR][0x2c/2] >> 8) & 3;
	layersort[numlayers].blendmask = 0;
	layersort[numlayers].sprblendmask = 0;
	layersort[numlayers].coloroffs = compute_color_offsets((mixer_control[MONITOR][0x3e/2] >> 8) & 1, (mixer_control[MONITOR][0x3e/2] >> 14) & 1);
	numlayers++;

	/* now bubble sort the list by effective priority */
	for (laynum = 0; laynum < numlayers; laynum++)
		for (i = laynum + 1; i < numlayers; i++)
			if (layersort[i].effpri > layersort[laynum].effpri)
			{
				struct layer_info temp = layersort[i];
				layersort[i] = layersort[laynum];
				layersort[laynum] = temp;
			}

	/* for each possible sprite group, insert the sprites into the list at the appropriate point */
	for (groupnum = 0; groupnum <= sprgroup_mask; groupnum++)
	{
		int effgroup = sprgroup_or | groupnum;
		int priority = mixer_control[MONITOR][0x00/2 + effgroup] & 0x0f;
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
		if ((mixer_control[MONITOR][0x4c/2] & 3) != 3)
			layerorder[groupnum][sprindex].palbase = (mixer_control[MONITOR][0x00/2 + effgroup] & 0x00f0) << 6;
		else
			layerorder[groupnum][sprindex].palbase = (mixer_control[MONITOR][0x4c/2] & 0x00f0) << 6;
		layerorder[groupnum][sprindex].mixshift = (mixer_control[MONITOR][0x00/2 + effgroup] >> 8) & 3;
		layerorder[groupnum][sprindex].blendmask = 0;
		layerorder[groupnum][sprindex].sprblendmask = 0;
		layerorder[groupnum][sprindex].coloroffs = compute_color_offsets((mixer_control[MONITOR][0x3e/2] >> 6) & 1, (mixer_control[MONITOR][0x4c/2] >> 15) & 1);
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
}
*/

	/* based on the sprite controller flip bits, the data is scanned to us in different */
	/* directions; account for this */
	if (sprite_control_latched[2] & 1)
	{
		sprx_start = cliprect->max_x;
		sprdx = -1;
	}
	else
	{
		sprx_start = cliprect->min_x;
		sprdx = 1;
	}

	if (sprite_control_latched[2] & 2)
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
		UINT32 *dest = (UINT32 *)bitmap->line[y];
		UINT16 *layerbase[8];

		/* get the starting address for each layer */
		layerbase[MIXER_LAYER_TEXT] = (UINT16 *)layer_data[MIXER_LAYER_TEXT].bitmap->line[y];
		layerbase[MIXER_LAYER_NBG0] = (UINT16 *)layer_data[MIXER_LAYER_NBG0].bitmap->line[y];
		layerbase[MIXER_LAYER_NBG1] = (UINT16 *)layer_data[MIXER_LAYER_NBG1].bitmap->line[y];
		layerbase[MIXER_LAYER_NBG2] = (UINT16 *)layer_data[MIXER_LAYER_NBG2].bitmap->line[y];
		layerbase[MIXER_LAYER_NBG3] = (UINT16 *)layer_data[MIXER_LAYER_NBG3].bitmap->line[y];
		layerbase[MIXER_LAYER_BITMAP] = (UINT16 *)layer_data[MIXER_LAYER_BITMAP].bitmap->line[y];
		layerbase[MIXER_LAYER_SPRITES] = (UINT16 *)layer_data[MIXER_LAYER_SPRITES].bitmap->line[spry];
		layerbase[MIXER_LAYER_BACKGROUND] = (UINT16 *)layer_data[MIXER_LAYER_BACKGROUND].bitmap->line[y];

		/* loop over columns */
		for (x = cliprect->min_x, sprx = sprx_start; x <= cliprect->max_x; x++, sprx += sprdx)
		{
			struct layer_info *first;
			int *rgbdelta;
			int firstpix;
			int sprpix, sprgroup;
			int r, g, b;

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
					if ((firstpix & layer_data[laynum].trans_mask) || laynum == MIXER_LAYER_BACKGROUND)
						break;
				}

				/* sprite layers are special */
				else
				{
					firstpix = sprpix;
					if (firstpix != 0xffff)
					{
						firstpix &= sprpixmask;
						break;
					}
				}
			}

			/* adjust the first pixel */
			firstpix = system32_paletteram[(first->palbase + ((firstpix >> first->mixshift) & 0xfff0) + (firstpix & 0x0f)) & 0x3fff];

			/* compute R, G, B */
			rgbdelta = &rgboffs[first->coloroffs][0];
			r = ((firstpix >>  0) & 0x1f) + rgbdelta[0];
			g = ((firstpix >>  5) & 0x1f) + rgbdelta[1];
			b = ((firstpix >> 10) & 0x1f) + rgbdelta[2];

			/* if there are potential blends, keep looking */
			if (first->blendmask != 0)
			{
				struct layer_info *second;
				int secondpix;

				/* now scan the layers to find the topmost non-transparent pixel */
				for (second = first + 1; ; second++)
				{
					laynum = second->index;

					/* non-sprite layers are treated similarly */
					if (laynum != MIXER_LAYER_SPRITES)
					{
						secondpix = layerbase[laynum][x] & 0x1fff;
						if ((secondpix & layer_data[laynum].trans_mask) || laynum == MIXER_LAYER_BACKGROUND)
							break;
					}

					/* sprite layers are special */
					else
					{
						secondpix = sprpix;
						if (secondpix != 0xffff)
						{
							secondpix &= sprpixmask;
							break;
						}
					}
				}

				/* are we blending with that layer? */
				if ((first->blendmask & (1 << laynum)) &&
					(laynum != MIXER_LAYER_SPRITES || (first->sprblendmask & (1 << sprgroup))))
				{
					/* adjust the second pixel */
					secondpix = system32_paletteram[(second->palbase + ((secondpix >> second->mixshift) & 0xfff0) + (secondpix & 0x0f)) & 0x3fff];

					/* compute first RGB */
					r *= 8 - blendfactor;
					g *= 8 - blendfactor;
					b *= 8 - blendfactor;

					/* add in second RGB */
					rgbdelta = &rgboffs[second->coloroffs][0];
					r += (((secondpix >>  0) & 0x1f) + rgbdelta[0]) * blendfactor;
					g += (((secondpix >>  5) & 0x1f) + rgbdelta[1]) * blendfactor;
					b += (((secondpix >> 10) & 0x1f) + rgbdelta[2]) * blendfactor;

					/* shift off the extra bits */
					r >>= 3;
					g >>= 3;
					b >>= 3;
				}
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
}



/*************************************
 *
 *  Master update routine
 *
 *************************************/

VIDEO_UPDATE( system32 )
{
	UINT8 enablemask;

	/* if the display is off, punt */
	if (!sys32_displayenable)
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
	mix_all_layers(bitmap, cliprect, enablemask);
	profiler_mark(PROFILER_END);

#if SHOW_ALPHA
{
	static const char *layername[] = { "TEXT ", "NBG0 ", "NBG1 ", "NBG2 ", "NBG3 ", "BITMAP " };
	char temp[100];
	int count = 0, i;
	sprintf(temp, "ALPHA(%d):", (mixer_control[MONITOR][0x4e/2] >> 8) & 7);
	for (i = 0; i < 6; i++)
		if (enablemask & (1 << i))
			if ((mixer_control[MONITOR][0x30/2 + i] & 0x1010) == 0x1010)
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
		int flip = (sys32_videoram[0x1ff00/2] >> 9) & 1;
		int clips = (sys32_videoram[0x1ff06/2] >> (4 * showclip)) & 0x0f;
		if (((sys32_videoram[0x1ff02/2] >> (11 + showclip)) & 1) && clips)
		{
			int i, x, y;
			for (i = 0; i < 4; i++)
				if (clips & (1 << i))
				{
					struct rectangle rect;
					if (!flip)
					{
						rect.min_x = sys32_videoram[0x1ff60/2 + i * 4] & 0x1ff;
						rect.min_y = sys32_videoram[0x1ff62/2 + i * 4] & 0x0ff;
						rect.max_x = (sys32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1;
						rect.max_y = (sys32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1;
					}
					else
					{
						rect.max_x = (Machine->visible_area.max_x + 1) - (sys32_videoram[0x1ff60/2 + i * 4] & 0x1ff);
						rect.max_y = (Machine->visible_area.max_y + 1) - (sys32_videoram[0x1ff62/2 + i * 4] & 0x0ff);
						rect.min_x = (Machine->visible_area.max_x + 1) - ((sys32_videoram[0x1ff64/2 + i * 4] & 0x1ff) + 1);
						rect.min_y = (Machine->visible_area.max_y + 1) - ((sys32_videoram[0x1ff66/2 + i * 4] & 0x0ff) + 1);
					}
					sect_rect(&rect, &Machine->visible_area);

					if (rect.min_y <= rect.max_y && rect.min_x <= rect.max_x)
					{
						for (y = rect.min_y; y <= rect.max_y; y++)
						{
							bitmap->plot(bitmap, rect.min_x, y, Machine->uifont->colortable[1]);
							bitmap->plot(bitmap, rect.max_x, y, Machine->uifont->colortable[1]);
						}
						for (x = rect.min_x; x <= rect.max_x; x++)
						{
							bitmap->plot(bitmap, x, rect.min_y, Machine->uifont->colortable[1]);
							bitmap->plot(bitmap, x, rect.max_y, Machine->uifont->colortable[1]);
						}
					}
				}
		}
	}
}
#endif

#if PRINTF_MIXER_DATA
{
	static int count = 0;
	if (++count > 60 * 5)
	{
		printf("\n");
		printf("OP: %04X\n", sys32_videoram[0x1ff8e/2]);
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
			mixer_control[MONITOR][0x00],
			mixer_control[MONITOR][0x01],
			mixer_control[MONITOR][0x02],
			mixer_control[MONITOR][0x03],
			mixer_control[MONITOR][0x04],
			mixer_control[MONITOR][0x05],
			mixer_control[MONITOR][0x06],
			mixer_control[MONITOR][0x07],
			mixer_control[MONITOR][0x08],
			mixer_control[MONITOR][0x09],
			mixer_control[MONITOR][0x0a],
			mixer_control[MONITOR][0x0b],
			mixer_control[MONITOR][0x0c],
			mixer_control[MONITOR][0x0d],
			mixer_control[MONITOR][0x0e],
			mixer_control[MONITOR][0x0f]);
		printf("20: %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X\n",
			mixer_control[MONITOR][0x10],
			mixer_control[MONITOR][0x11],
			mixer_control[MONITOR][0x12],
			mixer_control[MONITOR][0x13],
			mixer_control[MONITOR][0x14],
			mixer_control[MONITOR][0x15],
			mixer_control[MONITOR][0x16],
			mixer_control[MONITOR][0x17],
			mixer_control[MONITOR][0x18],
			mixer_control[MONITOR][0x19],
			mixer_control[MONITOR][0x1a],
			mixer_control[MONITOR][0x1b],
			mixer_control[MONITOR][0x1c],
			mixer_control[MONITOR][0x1d],
			mixer_control[MONITOR][0x1e],
			mixer_control[MONITOR][0x1f]);
		printf("40: %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X - %04X %04X %04X %04X\n",
			mixer_control[MONITOR][0x20],
			mixer_control[MONITOR][0x21],
			mixer_control[MONITOR][0x22],
			mixer_control[MONITOR][0x23],
			mixer_control[MONITOR][0x24],
			mixer_control[MONITOR][0x25],
			mixer_control[MONITOR][0x26],
			mixer_control[MONITOR][0x27],
			mixer_control[MONITOR][0x28],
			mixer_control[MONITOR][0x29],
			mixer_control[MONITOR][0x2a],
			mixer_control[MONITOR][0x2b],
			mixer_control[MONITOR][0x2c],
			mixer_control[MONITOR][0x2d],
			mixer_control[MONITOR][0x2e],
			mixer_control[MONITOR][0x2f]);
		count = 0;
	}
}
#endif

}



#if 0

#endif

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
