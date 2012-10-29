/***************************************************************************

  Functions to emulate the video hardware of some Toaplan games,
  which use the BCU-2 tile controller, and the FCU-2 Sprite controller -
  and SCU Sprite controller (Only Rally Bike uses the SCU controller).


  There are 4 scrolling layers of graphics, stored in planes of 64x64 tiles.
  Each tile in each plane is assigned a priority between 1 and 15, higher
  numbers have greater priority.

 BCU controller. Each tile takes up 32 bits - the format is:

  0         1         2         3
  ---- ---- ---- ---- -ttt tttt tttt tttt = Tile number (0 - $7fff)
  ---- ---- ---- ---- h--- ---- ---- ---- = Hidden
  ---- ---- --cc cccc ---- ---- ---- ---- = Color (0 - $3f)
  pppp ---- ---- ---- ---- ---- ---- ---- = Priority (0-$f)
  ---- ???? ??-- ---- ---- ---- ---- ---- = Unknown / Unused

  Scroll Reg

  0         1         2         3
  xxxx xxxx x--- ---- ---- ---- ---- ---- = X position
  ---- ---- ---- ---- yyyy yyyy y--- ---- = Y position
  ---- ---- -??? ???? ---- ---- -??? ???? = Unknown / Unused



 FCU controller. Sprite RAM format  (except Rally Bike)

  0         1         2         3
  -sss ssss ssss ssss ---- ---- ---- ---- = Sprite number (0 - $7fff)
  h--- ---- ---- ---- ---- ---- ---- ---- = Hidden
  ---- ---- ---- ---- ---- ---- --cc cccc = Color (0 - $3f)
  ---- ---- ---- ---- ---- dddd dd-- ---- = Dimension (pointer to Size RAM)
  ---- ---- ---- ---- pppp ---- ---- ---- = Priority (0-$f)

  4         5         6         7
  ---- ---- ---- ---- xxxx xxxx x--- ---- = X position
  yyyy yyyy y--- ---- ---- ---- ---- ---- = Y position
  ---- ---- -??? ???? ---- ---- -??? ???? = Unknown



 SCU controller. Sprite RAM format  (Rally Bike)

  0         1         2         3
  ---- -sss ssss ssss ---- ---- ---- ---- = Sprite number (0 - $7FF)
  ---- ---- ---- ---- ---- ---- --cc cccc = Color (0 - $3F)
  ---- ---- ---- ---- ---- ---x ---- ---- = Flip X
  ---- ---- ---- ---- ---- --y- ---- ---- = Flip Y
  ---- ---- ---- ---- ---- pp-- ---- ---- = Priority (0h,4h,8h,Ch (shifted < 2 places))
  ???? ?--- ---- ---- ???? ---- ??-- ---- = Unknown / Unused

  4         5         6         7
  xxxx xxxx x--- ---- ---- ---- ---- ---- = X position
  ---- ---- ---- ---- yyyy yyyy y--- ---- = Y position
  ---- ---- -??? ???? ---- ---- -??? ???? = Unknown



  The tiles use a palette of 1024 colors, the sprites use a different palette
  of 1024 colors.


           BCU Controller writes                Tile Offsets
 Game      reg0  reg1  reg2  reg3         X     Y     flip-X  flip-Y
RallyBik   41e0  2e1e  148c  0f09        01e6  00fc     <- same --
Truxton    41e0  2717  0e86  0c06        01b7  00f2     0188  01fd
HellFire   41e0  2717  0e86  0c06        01b7  0102     0188  000d
ZeroWing   41e0  2717  0e86  0c06        01b7  0102     0188  000d
DemonWld   41e0  2e1e  148c  0f09        01a9  00fc     0196  0013
FireShrk   41e0  2717  0e86  0c06        01b7  00f2     0188  01fd
Out-Zone   41e0  2e1e  148c  0f09        01a9  00ec     0196  0003
Vimana     41e0  2717  0e86  0c06        01b7  00f2     0188  01fd


Sprites are of varying sizes between 8x8 and 128x128 with any variation
in between, in multiples of 8 either way.
Here we draw the first 8x8 part of the sprite, then by using the sprite
dimensions, we draw the rest of the 8x8 parts to produce the complete
sprite.


Abnormalities:
 The equations for flipscreen, don't suite the horizontal games. So a minor
 hack is implemented for them, though it still isn't 100% right - see below.

 OutZone sprite priorities are unusual. On level 4, a character with
 low priority (6) is hidden by a higher priority (8) character, yet it
 shouldn't be. The character is a shooting enemy hidden by a sliding
 left to right platform (which he should be standing on).
 So how does the real hardware deal with this ?

 How/when do priority 0 Tile layers really get displayed ?

 What are the video PROMs for ? Priority maybe ?


 ***** Notes on the horizontal game scroll Y probs (Eg, Zero Wing) *****

 Scrolls    PF1-X  PF1-Y    PF2-X  PF2-Y    PF3-X  PF3-Y    PF4-X  PF4-Y
 ------>    #4180  #f880    #1240  #f880    #4380  #f880    #e380  #f880
 -flip->    #1500  #7f00    #e8c0  #7f00    #1300  #7f00    #bb00  #7f00

 ------>    #4100  #f880    #1200  #7880    #4300  #f880    #e380  #f880
 -flip->    #1500  #7f00    #e8c0  #8580??  #1300  #7f00    #bb00  #7f00
									  |
									  |
									f880 = 1111 1000 1000 = 1f1 scroll
									7f00 - 0111 1111 0000 = 0fe scroll
									7880 = 0111 1000 1000 = 0f1 scroll
									8580 = 1000 0101 1000 = 10b scroll

 So a snapshot of the scroll equations become (from the functions below):
	1f1 - (102 - 10f) == 1fe   star background
	0Fe - (00d - 10f) == 200   star background (flipscreen)
	0f1 - (102 - 10f) == 0fe   red  background
	10B - (00d - 10f) == 20d   red  background (flipscreen) wrong!
	10B - (00d - 002) == 100   red  background (flipscreen) should equate to this (100)


***************************************************************************/


#include "driver.h"
#include "state.h"
#include "toaplan1.h"
#include "tilemap.h"
#include "palette.h"
#include "vidhrdw/generic.h"
#include "cpu/m68000/m68000.h"


#define TOAPLAN1_TILEVRAM_SIZE       0x4000	/* 4 tile layers each this RAM size */
#define TOAPLAN1_SPRITERAM_SIZE      0x800	/* sprite ram */
#define TOAPLAN1_SPRITESIZERAM_SIZE  0x80	/* sprite size ram */

static data16_t *pf4_tilevram16;	/*  ||  Drawn in this order */
static data16_t *pf3_tilevram16;	/*  ||  */
static data16_t *pf2_tilevram16;	/* \||/ */
static data16_t *pf1_tilevram16;	/*  \/  */

static data16_t *toaplan1_spritesizeram16;
static data16_t *toaplan1_buffered_spritesizeram16;

size_t toaplan1_colorram1_size;
size_t toaplan1_colorram2_size;
data16_t *toaplan1_colorram1;
data16_t *toaplan1_colorram2;

static int bcu_flipscreen;		/* Tile   controller flip flag */
static int fcu_flipscreen;		/* Sprite controller flip flag */

static int pf_voffs;
static int spriteram_offs;

static int pf1_scrollx;
static int pf1_scrolly;
static int pf2_scrollx;
static int pf2_scrolly;
static int pf3_scrollx;
static int pf3_scrolly;
static int pf4_scrollx;
static int pf4_scrolly;
static int scrollx_offs1;
static int scrollx_offs2;
static int scrollx_offs3;
static int scrollx_offs4;
static int scrolly_offs;


#ifdef MAME_DEBUG
static int display_pf1 = 1;
static int display_pf2 = 1;
static int display_pf3 = 1;
static int display_pf4 = 1;
static int displog = 0;
#endif
static int display_sprites = 1;

static int sprite_priority[16];
static int tiles_offsetx;
static int tiles_offsety;

static int toaplan1_reset;		/* Hack! See toaplan1_bcu_control below */

static struct tilemap *pf1_tilemap, *pf2_tilemap, *pf3_tilemap, *pf4_tilemap;


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_pf1_tile_info(int tile_index)
{
	int color, tile_number, attrib;

	tile_number = pf1_tilevram16[2*tile_index+1] & 0x7fff;
	attrib = pf1_tilevram16[2*tile_index];
	color = attrib & 0x3f;
	SET_TILE_INFO(
			0,
			tile_number,
			color,
			0)
	if (pf1_tilevram16[2*tile_index+1] & 0x8000) tile_info.priority = 0;
	else tile_info.priority = (attrib & 0xf000) >> 12;
}

static void get_pf2_tile_info(int tile_index)
{
	int color, tile_number, attrib;

	tile_number = pf2_tilevram16[2*tile_index+1] & 0x7fff;
	attrib = pf2_tilevram16[2*tile_index];
	color = attrib & 0x3f;
	SET_TILE_INFO(
			0,
			tile_number,
			color,
			0)
	if (pf2_tilevram16[2*tile_index+1] & 0x8000) tile_info.priority = 0;
	else tile_info.priority = (attrib & 0xf000) >> 12;
}

static void get_pf3_tile_info(int tile_index)
{
	int color, tile_number, attrib;

	tile_number = pf3_tilevram16[2*tile_index+1] & 0x7fff;
	attrib = pf3_tilevram16[2*tile_index];
	color = attrib & 0x3f;
	SET_TILE_INFO(
			0,
			tile_number,
			color,
			0)
	if (pf3_tilevram16[2*tile_index+1] & 0x8000) tile_info.priority = 0;
	else tile_info.priority = (attrib & 0xf000) >> 12;
}

static void get_pf4_tile_info(int tile_index)
{
	int color, tile_number, attrib;

	tile_number = pf4_tilevram16[2*tile_index+1] & 0x7fff;
	attrib = pf4_tilevram16[2*tile_index];
	color = attrib & 0x3f;
	SET_TILE_INFO(
			0,
			tile_number,
			color,
			0)
	if (pf4_tilevram16[2*tile_index+1] & 0x8000) tile_info.priority = 0;
	else tile_info.priority = (attrib & 0xf000) >> 12;
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

static int toaplan1_create_tilemaps(void)
{
	pf1_tilemap = tilemap_create(get_pf1_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	pf2_tilemap = tilemap_create(get_pf2_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	pf3_tilemap = tilemap_create(get_pf3_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	pf4_tilemap = tilemap_create(get_pf4_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

	if (!pf1_tilemap || !pf2_tilemap || !pf3_tilemap || !pf4_tilemap)
		return 1;

	tilemap_set_transparent_pen(pf1_tilemap,0);
	tilemap_set_transparent_pen(pf2_tilemap,0);
	tilemap_set_transparent_pen(pf3_tilemap,0);
	tilemap_set_transparent_pen(pf4_tilemap,0);

	return 0;
}


static int toaplan1_paletteram_alloc(void)
{
	if ((paletteram16 = (data16_t *)auto_malloc(toaplan1_colorram1_size + toaplan1_colorram2_size)) == 0)
		return 1;

	return 0;
}

static int toaplan1_vram_alloc(void)
{
	if ((pf1_tilevram16 = (data16_t *)auto_malloc(TOAPLAN1_TILEVRAM_SIZE)) == 0)
		return 1;
	memset(pf1_tilevram16,0,TOAPLAN1_TILEVRAM_SIZE);

	if ((pf2_tilevram16 = (data16_t *)auto_malloc(TOAPLAN1_TILEVRAM_SIZE)) == 0)
		return 1;
	memset(pf2_tilevram16,0,TOAPLAN1_TILEVRAM_SIZE);

	if ((pf3_tilevram16 = (data16_t *)auto_malloc(TOAPLAN1_TILEVRAM_SIZE)) == 0)
		return 1;
	memset(pf3_tilevram16,0,TOAPLAN1_TILEVRAM_SIZE);

	if ((pf4_tilevram16 = (data16_t *)auto_malloc(TOAPLAN1_TILEVRAM_SIZE)) == 0)
		return 1;
	memset(pf4_tilevram16,0,TOAPLAN1_TILEVRAM_SIZE);

	return 0;
}

static int toaplan1_spritevram_alloc(void)
{
	if ((spriteram16 = (data16_t *)auto_malloc(TOAPLAN1_SPRITERAM_SIZE)) == 0)
		return 1;
	memset(spriteram16,0,TOAPLAN1_SPRITERAM_SIZE);

	if ((buffered_spriteram16 = (data16_t *)auto_malloc(TOAPLAN1_SPRITERAM_SIZE)) == 0)
		return 1;
	memset(buffered_spriteram16,0,TOAPLAN1_SPRITERAM_SIZE);

	if ((toaplan1_spritesizeram16 = (data16_t *)auto_malloc(TOAPLAN1_SPRITESIZERAM_SIZE)) == 0)
		return 1;
	memset(toaplan1_spritesizeram16,0,TOAPLAN1_SPRITESIZERAM_SIZE);

	if ((toaplan1_buffered_spritesizeram16 = (data16_t *)auto_malloc(TOAPLAN1_SPRITESIZERAM_SIZE)) == 0)
		return 1;
	memset(toaplan1_buffered_spritesizeram16,0,TOAPLAN1_SPRITESIZERAM_SIZE);

	spriteram_size = TOAPLAN1_SPRITERAM_SIZE;
	return 0;
}

void toaplan1_set_scrolls(void)
{
	tilemap_set_scrollx(pf1_tilemap,0,(pf1_scrollx >> 7) - (tiles_offsetx - scrollx_offs1));
	tilemap_set_scrollx(pf2_tilemap,0,(pf2_scrollx >> 7) - (tiles_offsetx - scrollx_offs2));
	tilemap_set_scrollx(pf3_tilemap,0,(pf3_scrollx >> 7) - (tiles_offsetx - scrollx_offs3));
	tilemap_set_scrollx(pf4_tilemap,0,(pf4_scrollx >> 7) - (tiles_offsetx - scrollx_offs4));
	tilemap_set_scrolly(pf1_tilemap,0,(pf1_scrolly >> 7) - (tiles_offsety - scrolly_offs));
	tilemap_set_scrolly(pf2_tilemap,0,(pf2_scrolly >> 7) - (tiles_offsety - scrolly_offs));
	tilemap_set_scrolly(pf3_tilemap,0,(pf3_scrolly >> 7) - (tiles_offsety - scrolly_offs));
	tilemap_set_scrolly(pf4_tilemap,0,(pf4_scrolly >> 7) - (tiles_offsety - scrolly_offs));
}

void rallybik_flipscreen(void)
{
	rallybik_bcu_flipscreen_w(0, bcu_flipscreen, 0);
}

void toaplan1_flipscreen(void)
{
	toaplan1_bcu_flipscreen_w(0, bcu_flipscreen, 0);
}


VIDEO_START( rallybik )
{
	if (toaplan1_create_tilemaps())  return 1;
	if (toaplan1_paletteram_alloc()) return 1;
	if (toaplan1_vram_alloc())       return 1;

	scrollx_offs1 = 0x0d + 6;
	scrollx_offs2 = 0x0d + 4;
	scrollx_offs3 = 0x0d + 2;
	scrollx_offs4 = 0x0d + 0;
	scrolly_offs  = 0x111;

	bcu_flipscreen = -1;
	toaplan1_reset = 0;

	state_save_register_UINT16("toaplan1", 0, "PaletteRam", paletteram16, (toaplan1_colorram1_size + toaplan1_colorram2_size)/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField1", pf1_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField2", pf2_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField3", pf3_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField4", pf4_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);

	state_save_register_int("toaplan1", 0, "PF1 scrollx offs", &scrollx_offs1);
	state_save_register_int("toaplan1", 0, "PF2 scrollx offs", &scrollx_offs2);
	state_save_register_int("toaplan1", 0, "PF3 scrollx offs", &scrollx_offs3);
	state_save_register_int("toaplan1", 0, "PF4 scrollx offs", &scrollx_offs4);
	state_save_register_int("toaplan1", 0, "PF  scrolly offs", &scrolly_offs);
	state_save_register_int("toaplan1", 0, "BCU flipscreen", &bcu_flipscreen);
	state_save_register_int("toaplan1", 0, "PF1 scrollx", &pf1_scrollx);
	state_save_register_int("toaplan1", 0, "PF1 scrolly", &pf1_scrolly);
	state_save_register_int("toaplan1", 0, "PF2 scrollx", &pf2_scrollx);
	state_save_register_int("toaplan1", 0, "PF2 scrolly", &pf2_scrolly);
	state_save_register_int("toaplan1", 0, "PF3 scrollx", &pf3_scrollx);
	state_save_register_int("toaplan1", 0, "PF3 scrolly", &pf3_scrolly);
	state_save_register_int("toaplan1", 0, "PF4 scrollx", &pf4_scrollx);
	state_save_register_int("toaplan1", 0, "PF4 scrolly", &pf4_scrolly);
	state_save_register_int("toaplan1", 0, "Tiles offsetx", &tiles_offsetx);
	state_save_register_int("toaplan1", 0, "Tiles offsety", &tiles_offsety);
	state_save_register_int("toaplan1", 0, "PlayField video offs", &pf_voffs);
	state_save_register_int("toaplan1", 0, "SpriteRAM video offs", &spriteram_offs);

	state_save_register_func_postload(rallybik_flipscreen);

	return 0;
}

VIDEO_START( toaplan1 )
{
	if (toaplan1_create_tilemaps())  return 1;
	if (toaplan1_paletteram_alloc()) return 1;
	if (toaplan1_vram_alloc())       return 1;
	if (toaplan1_spritevram_alloc()) return 1;

	scrollx_offs1 = 0x1ef + 6;
	scrollx_offs2 = 0x1ef + 4;
	scrollx_offs3 = 0x1ef + 2;
	scrollx_offs4 = 0x1ef + 0;
	scrolly_offs  = 0x101;

	bcu_flipscreen = -1;
	fcu_flipscreen = 0;
	toaplan1_reset = 1;

	state_save_register_UINT16("toaplan1", 0, "PaletteRam", paletteram16, (toaplan1_colorram1_size + toaplan1_colorram2_size)/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField1", pf1_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField2", pf2_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField3", pf3_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "PlayField4", pf4_tilevram16, TOAPLAN1_TILEVRAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "SpriteRam", spriteram16, TOAPLAN1_SPRITERAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "Buffered SpriteRam", buffered_spriteram16, TOAPLAN1_SPRITERAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "SpriteSize RAM", toaplan1_spritesizeram16, TOAPLAN1_SPRITESIZERAM_SIZE/2);
	state_save_register_UINT16("toaplan1", 0, "Buffered SpriteSize RAM", toaplan1_buffered_spritesizeram16, TOAPLAN1_SPRITESIZERAM_SIZE/2);

	state_save_register_int("toaplan1", 0, "PF1 scrollx offs", &scrollx_offs1);
	state_save_register_int("toaplan1", 0, "PF2 scrollx offs", &scrollx_offs2);
	state_save_register_int("toaplan1", 0, "PF3 scrollx offs", &scrollx_offs3);
	state_save_register_int("toaplan1", 0, "PF4 scrollx offs", &scrollx_offs4);
	state_save_register_int("toaplan1", 0, "PF  scrolly offs", &scrolly_offs);
	state_save_register_int("toaplan1", 0, "BCU flipscreen", &bcu_flipscreen);
	state_save_register_int("toaplan1", 0, "FCU flipscreen", &fcu_flipscreen);
	state_save_register_int("toaplan1", 0, "PF1 scrollx", &pf1_scrollx);
	state_save_register_int("toaplan1", 0, "PF1 scrolly", &pf1_scrolly);
	state_save_register_int("toaplan1", 0, "PF2 scrolly", &pf2_scrolly);
	state_save_register_int("toaplan1", 0, "PF2 scrollx", &pf2_scrollx);
	state_save_register_int("toaplan1", 0, "PF3 scrollx", &pf3_scrollx);
	state_save_register_int("toaplan1", 0, "PF3 scrolly", &pf3_scrolly);
	state_save_register_int("toaplan1", 0, "PF4 scrollx", &pf4_scrollx);
	state_save_register_int("toaplan1", 0, "PF4 scrolly", &pf4_scrolly);
	state_save_register_int("toaplan1", 0, "Tiles offsetx", &tiles_offsetx);
	state_save_register_int("toaplan1", 0, "Tiles offsety", &tiles_offsety);
	state_save_register_int("toaplan1", 0, "PlayField video offs", &pf_voffs);
	state_save_register_int("toaplan1", 0, "SpriteRam video offs", &spriteram_offs);

	state_save_register_func_postload(toaplan1_flipscreen);

	return 0;
}


/***************************************************************************

  Video I/O port hardware.

***************************************************************************/

READ16_HANDLER( toaplan1_frame_done_r )
{
	return cpu_getvblank();
}

WRITE16_HANDLER( toaplan1_tile_offsets_w )
{
	if ( offset == 0 )
	{
		COMBINE_DATA(&tiles_offsetx);
		logerror("Tiles_offsetx now = %08x\n",tiles_offsetx);
	}
	else
	{
		COMBINE_DATA(&tiles_offsety);
		logerror("Tiles_offsety now = %08x\n",tiles_offsety);
	}
	toaplan1_reset = 1;
	toaplan1_set_scrolls();
}

WRITE16_HANDLER( rallybik_bcu_flipscreen_w )
{
	if (ACCESSING_LSB && (data != bcu_flipscreen))
	{
		logerror("Setting BCU controller flipscreen port to %04x\n",data);
		bcu_flipscreen = data & 0x01;		/* 0x0001 = flip, 0x0000 = no flip */
		tilemap_set_flip(ALL_TILEMAPS, (data ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0));
		if (bcu_flipscreen)
		{
			scrollx_offs1 = 0x080 - 6;
			scrollx_offs2 = 0x080 - 4;
			scrollx_offs3 = 0x080 - 2;
			scrollx_offs4 = 0x080 - 0;
			scrolly_offs  = 0x1f8;
		}
		else
		{
			scrollx_offs1 = 0x0d + 6;
			scrollx_offs2 = 0x0d + 4;
			scrollx_offs3 = 0x0d + 2;
			scrollx_offs4 = 0x0d + 0;
			scrolly_offs  = 0x111;
		}
		toaplan1_set_scrolls();
	}
}

WRITE16_HANDLER( toaplan1_bcu_flipscreen_w )
{
	if (ACCESSING_LSB && (data != bcu_flipscreen))
	{
		logerror("Setting BCU controller flipscreen port to %04x\n",data);
		bcu_flipscreen = data & 0x01;		/* 0x0001 = flip, 0x0000 = no flip */
		tilemap_set_flip(ALL_TILEMAPS, (data ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0));
		if (bcu_flipscreen)
		{
			scrollx_offs1 = 0x011 - 6;
			scrollx_offs2 = 0x011 - 4;
			scrollx_offs3 = 0x011 - 2;
			scrollx_offs4 = 0x011 - 0;
			scrolly_offs  = 0xff;
			if ((Machine->orientation & ORIENTATION_MASK) == ROT0)
			{
				scrolly_offs = 0x10f;
			}
		}
		else
		{
			scrollx_offs1 = 0x1ef + 6;
			scrollx_offs2 = 0x1ef + 4;
			scrollx_offs3 = 0x1ef + 2;
			scrollx_offs4 = 0x1ef + 0;
			scrolly_offs  = 0x101;
		}
		toaplan1_set_scrolls();
	}
}

WRITE16_HANDLER( toaplan1_fcu_flipscreen_w )
{
	if (ACCESSING_MSB)
	{
		logerror("Setting FCU controller flipscreen port to %04x\n",data);
		fcu_flipscreen = data & 0x8000;	/* 0x8000 = flip, 0x0000 = no flip */
	}
}

READ16_HANDLER( toaplan1_spriteram_offs_r ) /// this aint really needed ?
{
	return spriteram_offs;
}

WRITE16_HANDLER( toaplan1_spriteram_offs_w )
{
	COMBINE_DATA(&spriteram_offs);
}


/* tile palette */
READ16_HANDLER( toaplan1_colorram1_r )
{
	return toaplan1_colorram1[offset];
}

WRITE16_HANDLER( toaplan1_colorram1_w )
{
	COMBINE_DATA(&toaplan1_colorram1[offset]);
	paletteram16_xBBBBBGGGGGRRRRR_word_w(offset, data, mem_mask);
}

/* sprite palette */
READ16_HANDLER( toaplan1_colorram2_r )
{
	return toaplan1_colorram2[offset];
}

WRITE16_HANDLER( toaplan1_colorram2_w )
{
	COMBINE_DATA(&toaplan1_colorram2[offset]);
	paletteram16_xBBBBBGGGGGRRRRR_word_w(offset+(toaplan1_colorram1_size/2), data, mem_mask);
}

READ16_HANDLER( toaplan1_spriteram16_r )
{
	return spriteram16[spriteram_offs & ((TOAPLAN1_SPRITERAM_SIZE/2)-1)];
}

WRITE16_HANDLER( toaplan1_spriteram16_w )
{
	COMBINE_DATA(&spriteram16[spriteram_offs & ((TOAPLAN1_SPRITERAM_SIZE/2)-1)]);

#ifdef MAME_DEBUG
	if (spriteram_offs >= (TOAPLAN1_SPRITERAM_SIZE/2))
	{
		logerror("Sprite_RAM_word_w, %08x out of range !\n", spriteram_offs);
		return;
	}
#endif

	spriteram_offs++;
}

READ16_HANDLER( toaplan1_spritesizeram16_r )
{
	return toaplan1_spritesizeram16[spriteram_offs & ((TOAPLAN1_SPRITESIZERAM_SIZE/2)-1)];
}

WRITE16_HANDLER( toaplan1_spritesizeram16_w )
{
	COMBINE_DATA(&toaplan1_spritesizeram16[spriteram_offs & ((TOAPLAN1_SPRITESIZERAM_SIZE/2)-1)]);

#ifdef MAME_DEBUG
	if (spriteram_offs >= (TOAPLAN1_SPRITESIZERAM_SIZE/2))
	{
		logerror("Sprite_Size_RAM_word_w, %08x out of range !\n", spriteram_offs);
		return;
	}
#endif

	spriteram_offs++;	/// really ? shouldn't happen on the sizeram
}



WRITE16_HANDLER( toaplan1_bcu_control_w )
{
	logerror("BCU tile controller register:%02x now = %04x\n",offset,data);

	/*** Hack for Zero Wing and OutZone, to reset the sound system on */
	/*** soft resets. These two games don't have a sound reset port,  */
	/*** unlike the other games */

	if (toaplan1_unk_reset_port && toaplan1_reset)
	{
		toaplan1_reset = 0;
		toaplan1_reset_sound(0,0,0);
	}
}

READ16_HANDLER( toaplan1_tileram_offs_r )
{
	return pf_voffs;
}

WRITE16_HANDLER( toaplan1_tileram_offs_w )
{
	if (data >= 0x4000)
		logerror("Hmmm, unknown video layer being selected (%08x)\n",data);
	COMBINE_DATA(&pf_voffs);
}


READ16_HANDLER( toaplan1_tileram16_r )
{
	offs_t vram_offset;
	data16_t video_data = 0;

	switch (pf_voffs & 0xf000)	/* Locate Layer (PlayField) */
	{
		case 0x0000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				video_data = pf1_tilevram16[vram_offset];
				break;
		case 0x1000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				video_data = pf2_tilevram16[vram_offset];
				break;
		case 0x2000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				video_data = pf3_tilevram16[vram_offset];
				break;
		case 0x3000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				video_data = pf4_tilevram16[vram_offset];
				break;
		default:
				logerror("Hmmm, reading %04x from unknown playfield layer address %06x  Offset:%01x !!!\n",video_data,pf_voffs,offset);
				break;
	}

	return video_data;
}

READ16_HANDLER( rallybik_tileram16_r )
{
	data16_t data = toaplan1_tileram16_r(offset, mem_mask);

	if (offset == 0)	/* some bit lines may be stuck to others */
	{
		data |= ((data & 0xf000) >> 4);
		data |= ((data & 0x0030) << 2);
	}
	return data;
}

WRITE16_HANDLER( toaplan1_tileram16_w )
{
	data16_t oldword = 0;
	offs_t vram_offset;

	switch (pf_voffs & 0xf000)	/* Locate Layer (PlayField) */
	{
		case 0x0000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				oldword = pf1_tilevram16[vram_offset];
				if (data != oldword)
				{
					COMBINE_DATA(&pf1_tilevram16[vram_offset]);
					tilemap_mark_tile_dirty(pf1_tilemap,vram_offset/2);
				}
				break;
		case 0x1000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				oldword = pf2_tilevram16[vram_offset];
				if (data != oldword)
				{
					COMBINE_DATA(&pf2_tilevram16[vram_offset]);
					tilemap_mark_tile_dirty(pf2_tilemap,vram_offset/2);
				}
				break;
		case 0x2000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				oldword = pf3_tilevram16[vram_offset];
				if (data != oldword)
				{
					COMBINE_DATA(&pf3_tilevram16[vram_offset]);
					tilemap_mark_tile_dirty(pf3_tilemap,vram_offset/2);
				}
				break;
		case 0x3000:
				vram_offset = ((pf_voffs * 2) + offset) & ((TOAPLAN1_TILEVRAM_SIZE/2)-1);
				oldword = pf4_tilevram16[vram_offset];
				if (data != oldword)
				{
					COMBINE_DATA(&pf4_tilevram16[vram_offset]);
					tilemap_mark_tile_dirty(pf4_tilemap,vram_offset/2);
				}
				break;
		default:
				logerror("Hmmm, writing %04x to unknown playfield layer address %06x  Offset:%01x\n",data,pf_voffs,offset);
				break;
	}
}



READ16_HANDLER( toaplan1_scroll_regs_r )
{
	data16_t scroll = 0;

	switch(offset)
	{
		case 00: scroll = pf1_scrollx; break;
		case 01: scroll = pf1_scrolly; break;
		case 02: scroll = pf2_scrollx; break;
		case 03: scroll = pf2_scrolly; break;
		case 04: scroll = pf3_scrollx; break;
		case 05: scroll = pf3_scrolly; break;
		case 06: scroll = pf4_scrollx; break;
		case 07: scroll = pf4_scrolly; break;
		default: logerror("Hmmm, reading unknown video scroll register (%08x) !!!\n",offset);
				 break;
	}
	return scroll;
}


WRITE16_HANDLER( toaplan1_scroll_regs_w )
{
	switch(offset)
	{
		case 00: COMBINE_DATA(&pf1_scrollx);		/* 1D3h */
				 tilemap_set_scrollx(pf1_tilemap,0,(pf1_scrollx >> 7) - (tiles_offsetx - scrollx_offs1));
				 break;
		case 01: COMBINE_DATA(&pf1_scrolly);		/* 1EBh */
				 tilemap_set_scrolly(pf1_tilemap,0,(pf1_scrolly >> 7) - (tiles_offsety - scrolly_offs));
				 break;
		case 02: COMBINE_DATA(&pf2_scrollx);		/* 1D5h */
				 tilemap_set_scrollx(pf2_tilemap,0,(pf2_scrollx >> 7) - (tiles_offsetx - scrollx_offs2));
				 break;
		case 03: COMBINE_DATA(&pf2_scrolly);		/* 1EBh */
				 tilemap_set_scrolly(pf2_tilemap,0,(pf2_scrolly >> 7) - (tiles_offsety - scrolly_offs));
				 break;
		case 04: COMBINE_DATA(&pf3_scrollx);		/* 1D7h */
				 tilemap_set_scrollx(pf3_tilemap,0,(pf3_scrollx >> 7) - (tiles_offsetx - scrollx_offs3));
				 break;
		case 05: COMBINE_DATA(&pf3_scrolly);		/* 1EBh */
				 tilemap_set_scrolly(pf3_tilemap,0,(pf3_scrolly >> 7) - (tiles_offsety - scrolly_offs));
				 break;
		case 06: COMBINE_DATA(&pf4_scrollx);		/* 1D9h */
				 tilemap_set_scrollx(pf4_tilemap,0,(pf4_scrollx >> 7) - (tiles_offsetx - scrollx_offs4));
				 break;
		case 07: COMBINE_DATA(&pf4_scrolly);		/* 1EBh */
				 tilemap_set_scrolly(pf4_tilemap,0,(pf4_scrolly >> 7) - (tiles_offsety - scrolly_offs));
				 break;
		default: logerror("Hmmm, writing %08x to unknown video scroll register (%08x) !!!\n",data ,offset);
				 break;
	}
}




#ifdef MAME_DEBUG
void toaplan1_log_vram(void)
{
	if ( keyboard_pressed(KEYCODE_M) )
	{
		offs_t sprite_voffs;
		while (keyboard_pressed(KEYCODE_M)) ;
		if (toaplan1_spritesizeram16)			/* FCU controller */
		{
			int schar,sattr,sxpos,sypos,bschar,bsattr,bsxpos,bsypos;
			data16_t *size  = (data16_t *)(toaplan1_spritesizeram16);
			data16_t *bsize = (data16_t *)(toaplan1_buffered_spritesizeram16);
			logerror("Scrolls    PF1-X  PF1-Y     PF2-X  PF2-Y     PF3-X  PF3-Y     PF4-X  PF4-Y\n");
			logerror("------>    #%04x  #%04x     #%04x  #%04x     #%04x  #%04x     #%04x  #%04x\n",pf1_scrollx,pf1_scrolly,pf2_scrollx,pf2_scrolly,pf3_scrollx,pf3_scrolly,pf4_scrollx,pf4_scrolly);
			for ( sprite_voffs = 0; sprite_voffs < (spriteram_size/2); sprite_voffs += 4 )
			{
				bschar = buffered_spriteram16[sprite_voffs];
				bsattr = buffered_spriteram16[sprite_voffs + 1];
				bsxpos = buffered_spriteram16[sprite_voffs + 2];
				bsypos = buffered_spriteram16[sprite_voffs + 3];
				schar = spriteram16[sprite_voffs];
				sattr = spriteram16[sprite_voffs + 1];
				sxpos = spriteram16[sprite_voffs + 2];
				sypos = spriteram16[sprite_voffs + 3];
				logerror("$(%04x)  Tile-Attr-Xpos-Ypos Now:%04x %04x %04x.%01x %04x.%01x  nxt:%04x %04x %04x.%01x %04x.%01x\n", sprite_voffs,
											 schar, sattr, sxpos, size[( sattr>>6)&0x3f]&0xf, sypos,( size[( sattr>>6)&0x3f]>>4)&0xf,
											bschar,bsattr,bsxpos,bsize[(bsattr>>6)&0x3f]&0xf,bsypos,(bsize[(bsattr>>6)&0x3f]>>4)&0xf);
			}
		}
		else									/* SCU controller */
		{
			int schar,sattr,sxpos,sypos,bschar,bsattr,bsxpos,bsypos;
			logerror("Scrolls    PF1-X  PF1-Y     PF2-X  PF2-Y     PF3-X  PF3-Y     PF4-X  PF4-Y\n");
			logerror("------>    #%04x  #%04x     #%04x  #%04x     #%04x  #%04x     #%04x  #%04x\n",pf1_scrollx,pf1_scrolly,pf2_scrollx,pf2_scrolly,pf3_scrollx,pf3_scrolly,pf4_scrollx,pf4_scrolly);
			for ( sprite_voffs = 0; sprite_voffs < (spriteram_size/2); sprite_voffs += 4 )
			{
				bschar = buffered_spriteram16[sprite_voffs];
				bsattr = buffered_spriteram16[sprite_voffs + 1];
				bsypos = buffered_spriteram16[sprite_voffs + 2];
				bsxpos = buffered_spriteram16[sprite_voffs + 3];
				schar = spriteram16[sprite_voffs];
				sattr = spriteram16[sprite_voffs + 1];
				sypos = spriteram16[sprite_voffs + 2];
				sxpos = spriteram16[sprite_voffs + 3];
				logerror("$(%04x)  Tile-Attr-Xpos-Ypos Now:%04x %04x %04x %04x  nxt:%04x %04x %04x %04x\n", sprite_voffs,
											 schar, sattr, sxpos, sypos,
											bschar,bsattr,bsxpos, bsypos);
			}
		}
	}

	if ( keyboard_pressed(KEYCODE_SLASH) )
	{
		data16_t *size  = (data16_t *)(toaplan1_spritesizeram16);
		data16_t *bsize = (data16_t *)(toaplan1_buffered_spritesizeram16);
		offs_t offs;
		while (keyboard_pressed(KEYCODE_SLASH)) ;
		if (toaplan1_spritesizeram16)			/* FCU controller */
		{
			logerror("Scrolls    PF1-X  PF1-Y     PF2-X  PF2-Y     PF3-X  PF3-Y     PF4-X  PF4-Y\n");
			logerror("------>    #%04x  #%04x     #%04x  #%04x     #%04x  #%04x     #%04x  #%04x\n",pf1_scrollx,pf1_scrolly,pf2_scrollx,pf2_scrolly,pf3_scrollx,pf3_scrolly,pf4_scrollx,pf4_scrolly);
			for ( offs = 0; offs < (TOAPLAN1_SPRITESIZERAM_SIZE/2); offs +=4 )
			{
				logerror("SizeOffs:%04x   now:%04x %04x %04x %04x    next: %04x %04x %04x %04x\n", offs,
												bsize[offs+0], bsize[offs+1],
												bsize[offs+2], bsize[offs+3],
												size[offs+0], size[offs+1],
												size[offs+2], size[offs+3]);
			}
		}
	}

	if ( keyboard_pressed(KEYCODE_N) )
	{
		offs_t tile_voffs;
		int tchar[5], tattr[5];
		while (keyboard_pressed(KEYCODE_N)) ;	/* BCU controller */
		logerror("Scrolls    PF1-X  PF1-Y     PF2-X  PF2-Y     PF3-X  PF3-Y     PF4-X  PF4-Y\n");
		logerror("------>    #%04x  #%04x     #%04x  #%04x     #%04x  #%04x     #%04x  #%04x\n",pf1_scrollx,pf1_scrolly,pf2_scrollx,pf2_scrolly,pf3_scrollx,pf3_scrolly,pf4_scrollx,pf4_scrolly);
		for ( tile_voffs = 0; tile_voffs < (TOAPLAN1_TILEVRAM_SIZE/2); tile_voffs += 2 )
		{
			tchar[1] = pf1_tilevram16[tile_voffs + 1];
			tattr[1] = pf1_tilevram16[tile_voffs];
			tchar[2] = pf2_tilevram16[tile_voffs + 1];
			tattr[2] = pf2_tilevram16[tile_voffs];
			tchar[3] = pf3_tilevram16[tile_voffs + 1];
			tattr[3] = pf3_tilevram16[tile_voffs];
			tchar[4] = pf4_tilevram16[tile_voffs + 1];
			tattr[4] = pf4_tilevram16[tile_voffs];
//			logerror("PF3 offs:%04x   Tile:%04x  Attr:%04x\n", tile_voffs, tchar, tattr);
			logerror("$(%04x)  Attr-Tile PF1:%04x-%04x  PF2:%04x-%04x  PF3:%04x-%04x  PF4:%04x-%04x\n", tile_voffs,
									tattr[1], tchar[1],  tattr[2], tchar[2],
									tattr[3], tchar[3],  tattr[4], tchar[4]);
		}
	}

	if ( keyboard_pressed(KEYCODE_W) )
	{
		while (keyboard_pressed(KEYCODE_W)) ;
		logerror("Mark here\n");
	}
	if ( keyboard_pressed(KEYCODE_E) )
	{
		while (keyboard_pressed(KEYCODE_E)) ;
		displog += 1;
		displog &= 1;
	}
	if (displog)
	{
		logerror("Scrolls    PF1-X  PF1-Y     PF2-X  PF2-Y     PF3-X  PF3-Y     PF4-X  PF4-Y\n");
		logerror("------>    #%04x  #%04x     #%04x  #%04x     #%04x  #%04x     #%04x  #%04x\n",pf1_scrollx,pf1_scrolly,pf2_scrollx,pf2_scrolly,pf3_scrollx,pf3_scrolly,pf4_scrollx,pf4_scrolly);
	}
	if ( keyboard_pressed(KEYCODE_B) )
	{
//		while (keyboard_pressed(KEYCODE_B)) ;
		scrollx_offs1 += 0x1; scrollx_offs2 += 0x1; scrollx_offs3 += 0x1; scrollx_offs4 += 0x1;
		logerror("Scrollx_offs now = %08x\n",scrollx_offs4);
		tilemap_set_scrollx(pf1_tilemap,0,(pf1_scrollx >> 7) - (tiles_offsetx - scrollx_offs1));
		tilemap_set_scrollx(pf2_tilemap,0,(pf2_scrollx >> 7) - (tiles_offsetx - scrollx_offs2));
		tilemap_set_scrollx(pf3_tilemap,0,(pf3_scrollx >> 7) - (tiles_offsetx - scrollx_offs3));
		tilemap_set_scrollx(pf4_tilemap,0,(pf4_scrollx >> 7) - (tiles_offsetx - scrollx_offs4));
	}
	if ( keyboard_pressed(KEYCODE_V) )
	{
//		while (keyboard_pressed(KEYCODE_V)) ;
		scrollx_offs1 -= 0x1; scrollx_offs2 -= 0x1; scrollx_offs3 -= 0x1; scrollx_offs4 -= 0x1;
		logerror("Scrollx_offs now = %08x\n",scrollx_offs4);
		tilemap_set_scrollx(pf1_tilemap,0,(pf1_scrollx >> 7) - (tiles_offsetx - scrollx_offs1));
		tilemap_set_scrollx(pf2_tilemap,0,(pf2_scrollx >> 7) - (tiles_offsetx - scrollx_offs2));
		tilemap_set_scrollx(pf3_tilemap,0,(pf3_scrollx >> 7) - (tiles_offsetx - scrollx_offs3));
		tilemap_set_scrollx(pf4_tilemap,0,(pf4_scrollx >> 7) - (tiles_offsetx - scrollx_offs4));
	}
	if ( keyboard_pressed(KEYCODE_C) )
	{
//		while (keyboard_pressed(KEYCODE_C)) ;
		scrolly_offs += 0x1;
		logerror("Scrolly_offs now = %08x\n",scrolly_offs);
		tilemap_set_scrolly(pf1_tilemap,0,(pf1_scrolly >> 7) - (tiles_offsety - scrolly_offs));
		tilemap_set_scrolly(pf2_tilemap,0,(pf2_scrolly >> 7) - (tiles_offsety - scrolly_offs));
		tilemap_set_scrolly(pf3_tilemap,0,(pf3_scrolly >> 7) - (tiles_offsety - scrolly_offs));
		tilemap_set_scrolly(pf4_tilemap,0,(pf4_scrolly >> 7) - (tiles_offsety - scrolly_offs));
	}
	if ( keyboard_pressed(KEYCODE_X) )
	{
//		while (keyboard_pressed(KEYCODE_X)) ;
		scrolly_offs -= 0x1;
		logerror("Scrolly_offs now = %08x\n",scrolly_offs);
		tilemap_set_scrolly(pf1_tilemap,0,(pf1_scrolly >> 7) - (tiles_offsety - scrolly_offs));
		tilemap_set_scrolly(pf2_tilemap,0,(pf2_scrolly >> 7) - (tiles_offsety - scrolly_offs));
		tilemap_set_scrolly(pf3_tilemap,0,(pf3_scrolly >> 7) - (tiles_offsety - scrolly_offs));
		tilemap_set_scrolly(pf4_tilemap,0,(pf4_scrolly >> 7) - (tiles_offsety - scrolly_offs));
	}

	if ( keyboard_pressed(KEYCODE_COLON) )	/* Turn Sprites on/off */
	{
		while (keyboard_pressed(KEYCODE_COLON)) ;
		display_sprites += 1;
		display_sprites &= 1;
	}
	if ( keyboard_pressed(KEYCODE_L) )		/* Turn Playfield 4 on/off */
	{
		while (keyboard_pressed(KEYCODE_L)) ;
		display_pf4 += 1;
		display_pf4 &= 1;
		tilemap_set_enable(pf4_tilemap, display_pf4);
	}
	if ( keyboard_pressed(KEYCODE_K) )		/* Turn Playfield 3 on/off */
	{
		while (keyboard_pressed(KEYCODE_K)) ;
		display_pf3 += 1;
		display_pf3 &= 1;
		tilemap_set_enable(pf3_tilemap, display_pf3);
	}
	if ( keyboard_pressed(KEYCODE_J) )		/* Turn Playfield 2 on/off */
	{
		while (keyboard_pressed(KEYCODE_J)) ;
		display_pf2 += 1;
		display_pf2 &= 1;
		tilemap_set_enable(pf2_tilemap, display_pf2);
	}
	if ( keyboard_pressed(KEYCODE_H) )		/* Turn Playfield 1 on/off */
	{
		while (keyboard_pressed(KEYCODE_H)) ;
		display_pf1 += 1;
		display_pf1 &= 1;
		tilemap_set_enable(pf1_tilemap, display_pf1);
	}
}
#endif



/***************************************************************************
	Mark the sprite priority used list.
***************************************************************************/

static void mark_toaplan1_sprite_priority(void)
{
	int priority;
	offs_t offs;

	for (priority = 0; priority < 16; priority++)
		sprite_priority[priority] = 0;		/* Clear priorities used list */

	for (offs = 0; offs < (TOAPLAN1_SPRITERAM_SIZE/2); offs += 4)
	{
		if ((buffered_spriteram16[offs] & 0x8000) == 0)	/* Is sprite is turned off ? */
		{
			priority = (buffered_spriteram16[offs + 1] & 0xf000) >> 12;
			sprite_priority[priority] = display_sprites;
		}
	}
}

static void mark_rallybik_sprite_priority(void)
{
	int priority;
	offs_t offs;

	for (priority = 0; priority < 16; priority++)
		sprite_priority[priority] = 0;		/* Clear priorities used list */

	for (offs = 0; offs < (spriteram_size/2); offs += 4)
	{
		if (buffered_spriteram16[offs + 3] != 0x8000)	/* Is sprite is turned off ? */
		{
			priority = (buffered_spriteram16[offs + 1] & 0x0c00) >> 8;
			sprite_priority[priority] = display_sprites;
		}
	}
}



/***************************************************************************
	Sprite Handlers
***************************************************************************/

static void draw_sprites( struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority_to_display )
{
	data16_t *source = (data16_t *)(buffered_spriteram16);
	data16_t *size   = (data16_t *)(toaplan1_buffered_spritesizeram16);

	offs_t offs;

	for (offs = 0; offs < (spriteram_size/2); offs += 4)
	{
		int attrib, sprite, color, priority, sx, sy;
		int sprite_sizex, sprite_sizey, dim_x, dim_y, sx_base, sy_base;
		int sizeram_ptr;

		attrib = source[offs+1];
		priority = (attrib & 0xf000) >> 12;

		if ((priority == priority_to_display) && ((source[offs] & 0x8000) == 0))
		{
			sprite = source[offs] & 0x7fff;
			color = attrib & 0x3f;

			/****** find sprite dimension ******/
			sizeram_ptr = (attrib >> 6) & 0x3f;
			sprite_sizex = ( size[sizeram_ptr]       & 0x0f) * 8;
			sprite_sizey = ((size[sizeram_ptr] >> 4) & 0x0f) * 8;

			/****** find position to display sprite ******/
			sx_base = (source[offs + 2] >> 7) & 0x1ff;
			sy_base = (source[offs + 3] >> 7) & 0x1ff;

			if (sx_base >= 0x180) sx_base -= 0x200;
			if (sy_base >= 0x180) sy_base -= 0x200;

			/****** flip the sprite layer ******/
			if (fcu_flipscreen)
			{
				sx_base += 8;
				if ((Machine->orientation & ORIENTATION_MASK) == ROT0)
				{
					sy_base -= 24;
				}
				else
				{
					sy_base += 8;
				}
				sx_base = 320 - sx_base;
				sy_base = 240 - sy_base;
			}

			for (dim_y = 0; dim_y < sprite_sizey; dim_y += 8)
			{
				if (fcu_flipscreen) sy = sy_base - dim_y;
				else                sy = sy_base + dim_y;

				for (dim_x = 0; dim_x < sprite_sizex; dim_x += 8)
				{
					if (fcu_flipscreen) sx = sx_base - dim_x;
					else                sx = sx_base + dim_x;

					drawgfx(bitmap,Machine->gfx[1],
							sprite,
							color,
							fcu_flipscreen,fcu_flipscreen,
							sx,sy,
							cliprect,TRANSPARENCY_PEN,0);

					sprite++ ;
				}
			}
		}
	}
}


static void draw_rallybik_sprites( struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority )
{
	int offs;

	for (offs = 0; offs < (spriteram_size/2); offs += 4)
	{
		int attrib, sx, sy, flipx, flipy;
		int sprite, color;

		attrib = buffered_spriteram16[offs + 1];
		if ((attrib & 0x0c00) == priority)
		{
			sy = (buffered_spriteram16[offs + 3] >> 7) & 0x1ff;
			if (sy != 0x0100)		/* sx = 0x01a0 or 0x0040*/
			{
				sprite = buffered_spriteram16[offs] & 0x7ff;
				color  = attrib & 0x3f;
				sx = (buffered_spriteram16[offs + 2] >> 7) & 0x1ff;
				flipx = attrib & 0x100;
				if (flipx) sx -= 15;
				flipy = attrib & 0x200;
				drawgfx(bitmap,Machine->gfx[1],
					sprite,
					color,
					flipx,flipy,
					sx-31,sy-16,
					cliprect,TRANSPARENCY_PEN,0);
			}
		}
	}
}


/***************************************************************************
	Draw the game screen in the given mame_bitmap.
***************************************************************************/

VIDEO_UPDATE( rallybik )
{
	int priority;

#ifdef MAME_DEBUG
	toaplan1_log_vram();
#endif

	mark_rallybik_sprite_priority();

	fillbitmap(bitmap,Machine->pens[0],cliprect);

	tilemap_draw(bitmap,cliprect,pf1_tilemap,TILEMAP_IGNORE_TRANSPARENCY | 0,0);
	tilemap_draw(bitmap,cliprect,pf1_tilemap,TILEMAP_IGNORE_TRANSPARENCY | 1,0);

	for (priority = 1; priority < 16; priority++)
	{
		tilemap_draw(bitmap,cliprect,pf4_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf3_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf2_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf1_tilemap,priority,0);
		if (sprite_priority[priority])
			draw_rallybik_sprites(bitmap,cliprect,priority << 8);
	}
}

VIDEO_UPDATE( toaplan1 )
{
	int priority = 0;

#ifdef MAME_DEBUG
	toaplan1_log_vram();
#endif

	mark_toaplan1_sprite_priority();

	fillbitmap(bitmap,Machine->pens[0x120],cliprect);

	tilemap_draw(bitmap,cliprect,pf4_tilemap,TILEMAP_IGNORE_TRANSPARENCY,0);
	for (priority = 8; priority < 16; priority++)
		tilemap_draw(bitmap,cliprect,pf1_tilemap,TILEMAP_IGNORE_TRANSPARENCY | priority,0);

	for (priority = 1; priority < 16; priority++)
	{
		if (sprite_priority[priority])
			draw_sprites(bitmap,cliprect,priority);
		tilemap_draw(bitmap,cliprect,pf4_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf3_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf2_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf1_tilemap,priority,0);
	}
}

VIDEO_UPDATE( zerowing )
{
	video_update_toaplan1(bitmap,cliprect);
}

VIDEO_UPDATE( demonwld )
{
	int priority = 0;

#ifdef MAME_DEBUG
	toaplan1_log_vram();
#endif

	mark_toaplan1_sprite_priority();

	fillbitmap(bitmap,Machine->pens[0x120],cliprect);

	tilemap_draw(bitmap,cliprect,pf1_tilemap,TILEMAP_IGNORE_TRANSPARENCY | 0,0);
	tilemap_draw(bitmap,cliprect,pf1_tilemap,TILEMAP_IGNORE_TRANSPARENCY | 1,0);

	for (priority = 1; priority < 16; priority++)
	{
		if (sprite_priority[priority])
			draw_sprites(bitmap,cliprect,priority);
		tilemap_draw(bitmap,cliprect,pf4_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf3_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf2_tilemap,priority,0);
		tilemap_draw(bitmap,cliprect,pf1_tilemap,priority,0);
	}
}


/****************************************************************************
	Spriteram is always 1 frame ahead, suggesting spriteram buffering.
	There are no CPU output registers that control this so we
	assume it happens automatically every frame, at the end of vblank
****************************************************************************/

VIDEO_EOF( rallybik )
{
	buffer_spriteram16_w(0, 0, 0);
}

VIDEO_EOF( toaplan1 )
{
	buffer_spriteram16_w(0, 0, 0);
	memcpy(toaplan1_buffered_spritesizeram16, toaplan1_spritesizeram16, TOAPLAN1_SPRITESIZERAM_SIZE);
}

VIDEO_EOF( samesame )
{
	buffer_spriteram16_w(0, 0, 0);
	memcpy(toaplan1_buffered_spritesizeram16, toaplan1_spritesizeram16, TOAPLAN1_SPRITESIZERAM_SIZE);
	cpu_set_irq_line(0, MC68000_IRQ_2, HOLD_LINE);	/* Frame done */
}
