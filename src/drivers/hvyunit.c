/***************************************************************************************
 
Heavy Unit
Kaneko/Taito 1988
 
Driver based on djboy.c / airbustr.c
 
This game runs on Kaneko hardware. The game is similar to R-Type.
 
PCB Layout
----------
M6100391A
M6100392A  880210204
KNA-001
|----------------------------------------------------|
|                                                    |
|           6116                          6116       |
|      15Mhz                              6116       |
|                                 PAL                |
|           B73_09.2P             B73_11.5P          |
|                                                    |
|                                                    |
|                                 Z80-1   DSW1 DSW2 J|
|                                                   A|
|     16MHz                                         M|
|                                                   M|
|       12MHz                       6264  MERMAID   A|
| B73_05.1H                                          |
| B73_04.1F B73_08.2F  6116              Z80-2       |
| B73_03.1D                       Z80-3  B73_12.7E   |
| B73_02.1C B73_07.2C  PANDORA    B73_10.5C  6116    |
| B73_01.1B B73_06.2B 4164 4164   6264 PAL  YM3014   |
|                     4164 4164   PAL       YM2203   |
|----------------------------------------------------|
 
Notes:
      Z80-1 clock  : 6.000MHz
      Z80-2 clock  : 6.000MHz
      Z80-3 clock  : 6.000MHz
      YM2203 clock : 3.000MHz
      VSync        : 58Hz
      HSync        : 15.59kHz
               \-\ : KANEKO 1988. DIP40 8751 MCU
      MERMAID    | : pin 18,19 = 6.000MHz (main clock)
                 | : pin 30 = 1.000MHz (prog/ale)
               /-/ : pin 22 = 111.48Hz (port 2 bit 1)
 
      PANDORA      : KANEKO PX79480FP-3 PANDORA-CHIP (C) KANEKO 1988
 
 
***************************************************************************************/
 
#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "cpu/i8051/i8051.h"

 
	/* Video */
static struct tilemap *bg_tilemap;
data16_t hvyunit_scrollx, hvyunit_scrolly, port0_data;
static int flipscreen;
UINT8 *hvyunit_videoram, *hvyunit_colorram;
WRITE_HANDLER( hvyunit_scrollx_w );
WRITE_HANDLER( hvyunit_scrolly_w );
WRITE_HANDLER( hvyunit_videoram_w );
WRITE_HANDLER( hvyunit_colorram_w );
WRITE_HANDLER ( pandora_spriteram_w );
READ_HANDLER( pandora_spriteram_r );
VIDEO_START( hvyunit );
VIDEO_UPDATE( hvyunit );
VIDEO_EOF( hvyunit );


/* pandora draw */
void pandora_start(UINT8 region, int x, int y);
void pandora_update(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
void pandora_eof();
void pandora_set_clear_bitmap(int clear);
void pandora_set_bg_pen( int pen );
UINT8* pandora_spriteram;
UINT8 pandora_region;
static struct mame_bitmap *pandora_sprites_bitmap; /* bitmap to render sprites to, Pandora seems to be frame'buffered' */
int	pandora_bg_pen;
int pandora_clear_bitmap;
int pandora_xoffset, pandora_yoffset;

 
	/* Mermaid */
static UINT8 data_to_mermaid;
static UINT8 data_to_z80;
static UINT8 mermaid_to_z80_full;
static UINT8 z80_to_mermaid_full;
static UINT8 mermaid_int0_l;
static UINT8 mermaid_p[4];

static UINT8 *sharedram;

static READ_HANDLER( sharedram_r )
{
	return sharedram[offset];
}

static WRITE_HANDLER( sharedram_w )
{
	sharedram[offset] = data;
}

 
/*************************************
 *
 *  Initialisation
 *
 *************************************/
 
static MACHINE_INIT( hvyunit )
{
	mermaid_int0_l = 1;
	mermaid_to_z80_full = 0;
	z80_to_mermaid_full = 0;
}
 
/*************************************
 *
 *  Video hardware
 *
 *************************************/

void pandora_set_bg_pen( int pen )
{
	pandora_bg_pen = pen;
}

void pandora_set_clear_bitmap(int clear)
{
	pandora_clear_bitmap = clear;
}

void pandora_update(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	if (!pandora_sprites_bitmap)
	{
		printf("ERROR: pandora_update with no pandora_sprites_bitmap\n");
		return;
	}

	copybitmap(bitmap,pandora_sprites_bitmap,0,0,0,0,cliprect,TRANSPARENCY_PEN,0);
}


void pandora_draw(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{

	int sx=0, sy=0, x=0, y=0, offs;


	/*
     * Sprite Tile Format
     * ------------------
     *
     * Byte | Bit(s)   | Use
     * -----+-76543210-+----------------
     *  0-2 | -------- | unused
     *  3   | xxxx.... | Palette Bank
     *  3   | .......x | XPos - Sign Bit
     *  3   | ......x. | YPos - Sign Bit
     *  3   | .....x.. | Use Relative offsets
     *  4   | xxxxxxxx | XPos
     *  5   | xxxxxxxx | YPos
     *  6   | xxxxxxxx | Sprite Number (low 8 bits)
     *  7   | ....xxxx | Sprite Number (high 4 bits)
     *  7   | x....... | Flip Sprite Y-Axis
     *  7   | .x...... | Flip Sprite X-Axis
     */

	for (offs = 0;offs < 0x1000;offs += 8)
	{
		int dx = pandora_spriteram[offs+4];
		int dy = pandora_spriteram[offs+5];
		int tilecolour = pandora_spriteram[offs+3];
		int attr = pandora_spriteram[offs+7];
		int flipx =   attr & 0x80;
		int flipy =  (attr & 0x40) << 1;
		int tile  = ((attr & 0x3f) << 8) + (pandora_spriteram[offs+6] & 0xff);

		if (tilecolour & 1) dx |= 0x100;
		if (tilecolour & 2) dy |= 0x100;

		if (tilecolour & 4)
		{
			x += dx;
			y += dy;
		}
		else
		{
			x = dx;
			y = dy;
		}

		if (flipscreen)
		{
			sx = 240 - x;
			sy = 240 - y;
			flipx = !flipx;
			flipy = !flipy;
		}
		else
		{
			sx = x;
			sy = y;
		}

		/* global offset */
		sx+=pandora_xoffset;
		sy+=pandora_yoffset;

		sx &=0x1ff;
		sy &=0x1ff;

		if (sx&0x100) sx-=0x200;
		if (sy&0x100) sy-=0x200;

		drawgfx(bitmap,Machine->gfx[pandora_region],
				tile,
				(tilecolour & 0xf0) >> 4,
				flipx, flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void pandora_eof()
{
	struct rectangle clip;

	/* draw top of screen */
	clip.min_x = Machine->visible_area.min_x;
	clip.max_x = Machine->visible_area.max_x;
	clip.min_y = Machine->visible_area.min_y;
	clip.max_y = Machine->visible_area.max_y;

	if (!pandora_spriteram)
	{
		printf("ERROR: pandora_eof with no pandora_spriteram\n");
		return;
	}

	// the games can disable the clearing of the sprite bitmap, to leave sprite trails
	if (pandora_clear_bitmap) fillbitmap(pandora_sprites_bitmap,pandora_bg_pen,&clip);

	pandora_draw(pandora_sprites_bitmap, &clip);
}

void pandora_start(UINT8 region, int x, int y)
{
	pandora_region = region;
	pandora_xoffset = x;
	pandora_yoffset = y;
	pandora_bg_pen = 0;
	pandora_spriteram = auto_malloc(0x1000);
	memset(pandora_spriteram,0x00, 0x1000);

	pandora_sprites_bitmap = auto_bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);
	pandora_clear_bitmap = 0;
}


WRITE_HANDLER ( pandora_spriteram_w )
{
	// it's either hooked up oddly on this, or on the 16-bit games
	// either way, we swap the address lines so that the spriteram is in the same format
	offset = BITSWAP16(offset,  15,14,13,12, 11,   7,6,5,4,3,2,1,0,   10,9,8  );

	if (!pandora_spriteram)
	{
		printf("ERROR: pandora_spriteram_w with no pandora_spriteram\n");
		return;
	}

	if (offset>=0x1000)
	{
		log_cb(RETRO_LOG_DEBUG, "pandora_spriteram_w write past spriteram, offset %04x %02x\n",offset,data);
		return;
	}
	pandora_spriteram[offset] = data;
}

READ_HANDLER( pandora_spriteram_r )
{
	// it's either hooked up oddly on this, or ont the 16-bit games
	// either way, we swap the address lines so that the spriteram is in the same format
	offset = BITSWAP16(offset,  15,14,13,12, 11,  7,6,5,4,3,2,1,0,  10,9,8  );

	if (!pandora_spriteram)
	{
		printf("ERROR: pandora_spriteram_r with no pandora_spriteram\n");
		return 0x00;
	}

	if (offset>=0x1000)
	{
		log_cb(RETRO_LOG_DEBUG, "pandora_spriteram_r read past spriteram, offset %04x\n",offset );
		return 0x00;
	}
	return pandora_spriteram[offset];
}

 
static void get_bg_tile_info(int tile_index)
{
	int attr = hvyunit_colorram[tile_index];
	int code = hvyunit_videoram[tile_index] + ((attr & 0x0f) << 8);
	int color = (attr >> 4);
 
	SET_TILE_INFO(1, code, color, 0);
}
 
VIDEO_START( hvyunit )
{
	bg_tilemap = tilemap_create(get_bg_tile_info, tilemap_scan_rows, TILEMAP_OPAQUE, 16, 16, 32, 32);

	if(!bg_tilemap)
		return 1;

    pandora_start(0,0,0);

	return 0;
	
}

VIDEO_UPDATE( hvyunit )
{
#define SX_POS	96
#define SY_POS	0

	tilemap_set_scrollx(bg_tilemap, 0, ((port0_data & 0x40) << 2) + hvyunit_scrollx + SX_POS); // TODO
	tilemap_set_scrolly(bg_tilemap, 0, ((port0_data & 0x80) << 1) + hvyunit_scrolly + SY_POS); // TODO
	fillbitmap(bitmap,get_black_pen(),cliprect);
	tilemap_draw(bitmap, cliprect, bg_tilemap, 0, 0);
	pandora_draw( bitmap, cliprect );

    flipscreen = 0;
}

VIDEO_EOF( hvyunit )
{
    pandora_eof();
}
 
 
/*************************************
 *
 *  Master CPU handlers
 *
 *************************************/
 
static WRITE_HANDLER( trigger_nmi_on_slave_cpu )
{
	cpu_set_nmi_line(1, PULSE_LINE);
}

 
static WRITE_HANDLER( master_bankswitch_w )
{
    int bank = data & 7;
	cpu_setbank( 1, memory_region(REGION_CPU1) + (bank * 0x4000) );
}


 
static WRITE_HANDLER( mermaid_data_w )
{
	data_to_mermaid = data;
	z80_to_mermaid_full = 1;
	mermaid_int0_l = 0;
	cpu_set_irq_line(3, 0, ASSERT_LINE);
}
 
static READ_HANDLER( mermaid_data_r )
{
	mermaid_to_z80_full = 0;
	return data_to_z80;
}
 
static READ_HANDLER( mermaid_status_r )
{
	return (!mermaid_to_z80_full << 2) | (z80_to_mermaid_full << 3);
}
 
 
/*************************************
 *
 *  Slave CPU handlers
 *
 *************************************/
 
static WRITE_HANDLER( trigger_nmi_on_sound_cpu2 )
{
	soundlatch_w(0, data);
	cpu_set_nmi_line(2, PULSE_LINE);
}
 
WRITE_HANDLER( hvyunit_videoram_w )
{
	if( hvyunit_videoram[offset] != data)
	{
		hvyunit_videoram[offset] = data;
		tilemap_mark_tile_dirty( bg_tilemap, offset);
	}
}
 
WRITE_HANDLER( hvyunit_colorram_w )
{
	if( hvyunit_colorram[offset] != data)
	{
		hvyunit_colorram[offset] = data;
		tilemap_mark_tile_dirty( bg_tilemap, offset);
	}
}

static WRITE_HANDLER( slave_bankswitch_w )
{
	int bank = data & 0x03;
	port0_data = data;
	cpu_setbank( 2, memory_region(REGION_CPU2) + (bank * 0x4000) );
}

WRITE_HANDLER( hvyunit_scrollx_w)
{
	hvyunit_scrollx=data;
}
 
WRITE_HANDLER( hvyunit_scrolly_w)
{
	hvyunit_scrolly=data;
}


static WRITE_HANDLER( coin_count_w )
{
	coin_counter_w(0, data & 1);
	coin_counter_w(1, data & 2);
}
 
 
/*************************************
 *
 *  Sound CPU handlers
 *
 *************************************/

static WRITE_HANDLER( sound_bankswitch_w )
{
	int bank = data & 0x03;
	cpu_setbank( 3, memory_region(REGION_CPU3) + (bank * 0x4000) );
}

 
/*************************************
 *
 *  Protection MCU handlers
 *
 *************************************/
 
static READ_HANDLER( mermaid_p0_r )
{
	// ?
	return 0;
}
 
static WRITE_HANDLER( mermaid_p0_w )
{
	if (!BIT(mermaid_p[0], 1) && BIT(data, 1))
	{
		mermaid_to_z80_full = 1;
		data_to_z80 = mermaid_p[1];
	}
 
	if (BIT(data, 0) == 1)
		z80_to_mermaid_full = 0;
 
	mermaid_p[0] = data;
}
 
static READ_HANDLER( mermaid_p1_r )
{
	if (BIT(mermaid_p[0], 0) == 0)
		return data_to_mermaid;
	else
		return 0; // ?
}
 
static WRITE_HANDLER( mermaid_p1_w )
{
	if (data == 0xff)
	{
		mermaid_int0_l = 1;
		cpu_set_irq_line(3, 0, CLEAR_LINE);
	}
 
	mermaid_p[1] = data;
}
 
static READ_HANDLER( mermaid_p2_r )
{
 
	switch ((mermaid_p[0] >> 2) & 3)
	{
        case 0: return readinputport(1);
		case 1: return readinputport(2);
		case 2: return readinputport(0);
		default: return 0xff;
	}
}
 
static WRITE_HANDLER( mermaid_p2_w )
{
	mermaid_p[2] = data;
}
 
static READ_HANDLER( mermaid_p3_r )
{
	UINT8 dsw = 0;
    UINT8 dsw1 = readinputport(3);
	UINT8 dsw2 = readinputport(4);
 
	switch ((mermaid_p[0] >> 5) & 3)
	{
		case 0: dsw = (BIT(dsw2, 4) << 3) | (BIT(dsw2, 0) << 2) | (BIT(dsw1, 4) << 1) | BIT(dsw1, 0); break;
		case 1: dsw = (BIT(dsw2, 5) << 3) | (BIT(dsw2, 1) << 2) | (BIT(dsw1, 5) << 1) | BIT(dsw1, 1); break;
		case 2: dsw = (BIT(dsw2, 6) << 3) | (BIT(dsw2, 2) << 2) | (BIT(dsw1, 6) << 1) | BIT(dsw1, 2); break;
		case 3: dsw = (BIT(dsw2, 7) << 3) | (BIT(dsw2, 3) << 2) | (BIT(dsw1, 7) << 1) | BIT(dsw1, 3); break;
	}
 
	return (dsw << 4) | (mermaid_int0_l << 2) | (mermaid_to_z80_full << 3);
}
 
static WRITE_HANDLER( mermaid_p3_w )
{
	mermaid_p[3] = data;
	cpu_set_reset_line(1, data & 2 ? CLEAR_LINE : ASSERT_LINE);
}

char master_ram[0xfff+1];

static READ_HANDLER( master_ram_r )
{
	return master_ram[offset];
}

static WRITE_HANDLER( master_ram_w )
{
	master_ram[offset] = data;
}

char sl_ram[0xd7ff - 0xd200+1];

static READ_HANDLER( sl_ram_r )
{
	return sl_ram[offset];
}

static WRITE_HANDLER( sl_ram_w )
{
	sl_ram[offset] = data;
}


char sl_ram2[0xdfff - 0xda00+1];


static READ_HANDLER( sl_ram2_r )
{
	return sl_ram2[offset];
}

static WRITE_HANDLER( sl_ram2_w )
{
	sl_ram2[offset] = data;
}

char snd_ram[0x7ff+1];


static READ_HANDLER( snd_ram_r )
{
	return snd_ram[offset];
}

static WRITE_HANDLER( snd_ram_w )
{
	snd_ram[offset] = data;
}
 
/*************************************
 *
 *  Memory maps
 *
 *************************************/


static MEMORY_READ_START( master_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xcfff, pandora_spriteram_r },
	{ 0xd000, 0xdfff, master_ram_r },
	{ 0xe000, 0xffff, sharedram_r },
MEMORY_END

static MEMORY_WRITE_START( master_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xcfff, pandora_spriteram_w },
	{ 0xd000, 0xdfff, master_ram_w },
	{ 0xe000, 0xffff, sharedram_w },
MEMORY_END

static PORT_WRITE_START( master_io_writeport )
    MEMORY_ADDRESS_BITS(8) /* port masking 0xff which is 8_bits in 78 */
    { 0x00, 0x00, master_bankswitch_w },
    { 0x01, 0x01, master_bankswitch_w },
	{ 0x02, 0x02, trigger_nmi_on_slave_cpu },
PORT_END


static MEMORY_READ_START( slave_readmem )
    { 0x0000, 0x7fff, MRA_RAM },
	{ 0x8000, 0xbfff, MRA_BANK2 },
	{ 0xc000, 0xc3ff, MRA_RAM },
	{ 0xc400, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd1ff, MRA_RAM },
	{ 0xd200, 0xd7ff, sl_ram_r },
	{ 0xd800, 0xd9ff, MRA_RAM },
	{ 0xda00, 0xdfff, sl_ram2_r },
	{ 0xe000, 0xffff, sharedram_r },
MEMORY_END

static MEMORY_WRITE_START( slave_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc3ff, hvyunit_videoram_w, &hvyunit_videoram },
	{ 0xc400, 0xc7ff, hvyunit_colorram_w, &hvyunit_colorram },
	{ 0xd000, 0xd1ff, paletteram_xxxxRRRRGGGGBBBB_split2_w, &paletteram_2 },
	{ 0xd200, 0xd7ff, sl_ram_w },
	{ 0xd800, 0xd9ff, paletteram_xxxxRRRRGGGGBBBB_split1_w, &paletteram },
	{ 0xda00, 0xdfff, sl_ram2_w },
	{ 0xe000, 0xffff, sharedram_w, &sharedram },
MEMORY_END

static PORT_READ_START( slave_io_readport )
    MEMORY_ADDRESS_BITS(8) /* port masking 0xff which is 8_bits in 78 */
	{ 0x04, 0x04, mermaid_data_r },
	{ 0x0c, 0x0c, mermaid_status_r },
//	{ 0x0e, 0x0e, MRA_RAM },
PORT_END

static PORT_WRITE_START( slave_io_writeport )
    MEMORY_ADDRESS_BITS(8) /* port masking 0xff which is 8_bits in 78 */
	{ 0x00, 0x00, slave_bankswitch_w },
	{ 0x02, 0x02, trigger_nmi_on_sound_cpu2 },
	{ 0x04, 0x04, mermaid_data_w },
	{ 0x06, 0x06, hvyunit_scrolly_w },
	{ 0x08, 0x08, hvyunit_scrollx_w },
	{ 0x0e, 0x0e, coin_count_w },
PORT_END

static MEMORY_READ_START( sound_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK3 },
	{ 0xc000, 0xc7ff, snd_ram_r },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, snd_ram_w },
MEMORY_END


static PORT_READ_START( sound_io_readport )
    MEMORY_ADDRESS_BITS(8) /* port masking 0xff which is 8_bits in 78 */
	{ 0x02, 0x02, YM2203_status_port_0_r },
	{ 0x03, 0x03, YM2203_read_port_0_r },
	{ 0x04, 0x04, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_io_writeport )
    MEMORY_ADDRESS_BITS(8) /* port masking 0xff which is 8_bits in 78 */
	{ 0x00, 0x00, sound_bankswitch_w },
	{ 0x02, 0x02, YM2203_control_port_0_w },
	{ 0x03, 0x03, YM2203_write_port_0_w },
PORT_END



/* I8051 memory handlers
static ADDRESS_MAP_START( mcu_map, ADDRESS_SPACE_PROGRAM, 8 )
    AM_RANGE(0x0000, 0x0fff) AM_ROM 
ADDRESS_MAP_END

static ADDRESS_MAP_START( mcu_data_map, ADDRESS_SPACE_DATA, 8 )
	AM_RANGE(0x0000, 0x007f) AM_RAM
	AM_RANGE(0x0100, 0x01ff) AM_RAM
ADDRESS_MAP_END


static ADDRESS_MAP_START( mcu_io, ADDRESS_SPACE_IO, 8 )
	AM_RANGE(0x00, 0x00) AM_READWRITE(mermaid_p0_r, mermaid_p0_w)
	AM_RANGE(0x01, 0x01) AM_READWRITE(mermaid_p1_r, mermaid_p1_w)
	AM_RANGE(0x02, 0x02) AM_READWRITE(mermaid_p2_r, mermaid_p2_w)
	AM_RANGE(0x03, 0x03) AM_READWRITE(mermaid_p3_r, mermaid_p3_w)
ADDRESS_MAP_END
*/

// combine memory and data maps into one for MAME78.??
static MEMORY_READ_START( mcu_readmem )
    { 0x0000, 0x0fff, MRA_ROM },
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0x0100, 0x01ff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( mcu_writemem )
    { 0x0000, 0x0fff, MWA_ROM },
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0x0100, 0x01ff, MWA_RAM },
MEMORY_END

// this is correct for 78
static PORT_READ_START( mcu_readport )
    { 0x00, 0x00, mermaid_p0_r },
	{ 0x01, 0x01, mermaid_p1_r },
	{ 0x02, 0x02, mermaid_p2_r },
	{ 0x03, 0x03, mermaid_p3_r },
PORT_END

static PORT_WRITE_START( mcu_writeport )
    { 0x00, 0x00, mermaid_p0_w },
	{ 0x01, 0x01, mermaid_p1_w },
	{ 0x02, 0x02, mermaid_p2_w },
	{ 0x03, 0x03, mermaid_p3_w },
PORT_END

/*************************************
 *
 *  Port definitions
 *
 *************************************/
 
INPUT_PORTS_START( hvyunit )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
 
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
 
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
 
	PORT_START
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, ( "Coin Mode" ) )
	PORT_DIPSETTING(    0x08, ( "Mode 1" ) )
	PORT_DIPSETTING(    0x00, ( "Mode 2" ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
    PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) ) 
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) ) 
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) ) 
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
    PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) ) 
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) ) 
 
	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "Allow_Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Bonus" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "7" )
    PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END
 
INPUT_PORTS_START( hvyunitj )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
 
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
 
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
 
	PORT_START
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
 
	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "Allow_Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Bonus" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "7" )
    PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END
 
 
/*************************************
 *
 *  Graphics definitions
 *
 *************************************/
 
static struct GfxLayout tile_layout =
{
    16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{
		0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4,
		8*32+0*4,8*32+1*4,8*32+2*4,8*32+3*4,8*32+4*4,8*32+5*4,8*32+6*4,8*32+7*4
	},
	{
		0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32,
		16*32+0*32,16*32+1*32,16*32+2*32,16*32+3*32,16*32+4*32,16*32+5*32,16*32+6*32,16*32+7*32
	},
	4*8*32
};
 
static struct GfxDecodeInfo hvyunit_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tile_layout, 0x100, 16 }, /* sprite */
	{ REGION_GFX2, 0, &tile_layout, 0x000, 16 }, /* background tiles */
	{ -1 }
};

 
 
/*************************************
 *
 *  Machine driver
 *
 *************************************/

static INTERRUPT_GEN( hvyunit_interrupt )
{
	static int addr = 0xff;
	addr ^= 0x02;
	cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, addr);

}
 
static struct YM2203interface ym2203_interface =
{
	1,
	3000000, /* ? */
	{ YM2203_VOL(80,80) },	/* gain,volume */
	{ 0 },	/* port A read */
	{ 0 }, /* port B read */
	{ 0 }, /* port A write */
	{ 0 }, /* port B write */
	{ 0 } /* IRQ handler for YM2203 */
};
 
/*************************************
 *
 *  Machine driver
 *
 *************************************/
 
static MACHINE_DRIVER_START( hvyunit )

	MDRV_CPU_ADD(Z80,6000000)
    MDRV_CPU_MEMORY(master_readmem, master_writemem)
	MDRV_CPU_PORTS(0, master_io_writeport)
	MDRV_CPU_VBLANK_INT(hvyunit_interrupt, 2)
 
	MDRV_CPU_ADD(Z80,6000000)
    MDRV_CPU_MEMORY(slave_readmem, slave_writemem)
	MDRV_CPU_PORTS(slave_io_readport, slave_io_writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold, 1)
 
	MDRV_CPU_ADD(Z80,6000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
    MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_io_readport,sound_io_writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold, 1)

	MDRV_CPU_ADD(I8051,6000000)
    MDRV_CPU_MEMORY(mcu_readmem, mcu_writemem)
	MDRV_CPU_PORTS(mcu_readport, mcu_writeport)

	MDRV_MACHINE_INIT(hvyunit)
 
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)
 
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 16, 240-1)
	MDRV_GFXDECODE(hvyunit_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(0x800)
 
	MDRV_VIDEO_START(hvyunit)
	MDRV_VIDEO_UPDATE(hvyunit)
	MDRV_VIDEO_EOF(hvyunit)
 
	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
MACHINE_DRIVER_END
 
 
 
/*************************************
 *
 *  ROM definition(s)
 *
 *************************************/
 
ROM_START( hvyunit )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "b73_10.5c",  0x00000, 0x20000, CRC(ca52210f) SHA1(346951962aa5bbad641117dbd66f035dddc7c0bf) )
 
	ROM_REGION( 0x20000, REGION_CPU2, 0 )
	ROM_LOAD( "b73_11.5p",  0x00000, 0x10000, CRC(cb451695) SHA1(116fd59f96a54c22fae65eea9ee5e58cb9ce5074) )
 
	ROM_REGION( 0x20000, REGION_CPU3, 0 )
	ROM_LOAD( "b73_12.7e",  0x000000, 0x010000, CRC(d1d24fab) SHA1(ed0312535d0b136d79aa885b9e6eea19ebde6409) )
 
	ROM_REGION( 0x1000, REGION_CPU4, 0 )
	ROM_LOAD( "mermaid.bin",  0x0000, 0x0e00, CRC(88c5dd27) SHA1(5043fed7fd192891be7e4096f2c5daaae1538bc4) )
 
	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROM_LOAD( "b73_08.2f",  0x000000, 0x080000, CRC(f83dd808) SHA1(09d5f1e86fad3a0d2d3ac1845103d3f2833c6793) )
	ROM_LOAD( "b73_07.2c",  0x100000, 0x010000, CRC(5cffa42c) SHA1(687e047345039479b35d5099e56dbc1d57284ed9) )
	ROM_LOAD( "b73_06.2b",  0x120000, 0x010000, CRC(a98e4aea) SHA1(560fef03ad818894c9c7578c6282d55b646e8129) )
	ROM_LOAD( "b73_01.1b",  0x140000, 0x010000, CRC(3a8a4489) SHA1(a01d7300015f90ce6dd571ad93e7a58270a99e47) )
	ROM_LOAD( "b73_02.1c",  0x160000, 0x010000, CRC(025c536c) SHA1(075e95cc39e792049ae656404e7f7440df064391) )
	ROM_LOAD( "b73_03.1d",  0x180000, 0x010000, CRC(ec6020cf) SHA1(2973aa2dc3deb2f27c9f1bad07a7664bad95b3f2) )
	/*                      0x190000, 0x010000  no data */
	ROM_LOAD( "b73_04.1f",  0x1a0000, 0x010000, CRC(f7badbb2) SHA1(d824ab4aba94d7ca02401f4f6f34213143c282ec) )
	/*                      0x1b0000, 0x010000  no data */
	ROM_LOAD( "b73_05.1h",  0x1c0000, 0x010000, CRC(b8e829d2) SHA1(31102358500d7b58173d4f18647decf5db744416) )
 
	ROM_REGION( 0x80000, REGION_GFX2, 0 )
	ROM_LOAD( "b73_09.2p",  0x000000, 0x080000, CRC(537c647f) SHA1(941c0f4e251bc68e53d62e70b033a3a6c145bb7e) )
ROM_END
 
ROM_START( hvyunitj )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "b73_30.5c",  0x00000, 0x20000, CRC(600af545) SHA1(c52b9be2bae28848ad0818c296f000a1bda4fa4f) )
 
	ROM_REGION( 0x20000, REGION_CPU2, 0 )
	ROM_LOAD( "b73_14.5p",  0x00000, 0x10000, CRC(0dfb51d4) SHA1(0e6f3b3d4558f12fe1b1620f57a0f4ac2065fd1a) )
 
	ROM_REGION( 0x20000, REGION_CPU3, 0 )
	ROM_LOAD( "b73_12.7e",  0x000000, 0x010000, CRC(d1d24fab) SHA1(ed0312535d0b136d79aa885b9e6eea19ebde6409) )
 
	ROM_REGION( 0x1000, REGION_CPU4, 0 )
	ROM_LOAD( "mermaid.bin",  0x0000, 0x0e00, CRC(88c5dd27) SHA1(5043fed7fd192891be7e4096f2c5daaae1538bc4) )
 
	ROM_REGION( 0x200000, REGION_GFX1, 0 ) /* note, the rom ordering on this is different to the other Japan set */
	ROM_LOAD( "b73_08.2f",  0x000000, 0x080000, CRC(f83dd808) SHA1(09d5f1e86fad3a0d2d3ac1845103d3f2833c6793) )
	ROM_LOAD( "b73_07.2c",  0x100000, 0x010000, CRC(5cffa42c) SHA1(687e047345039479b35d5099e56dbc1d57284ed9) )
	ROM_LOAD( "b73_06.2b",  0x110000, 0x010000, CRC(a98e4aea) SHA1(560fef03ad818894c9c7578c6282d55b646e8129) )
	ROM_LOAD( "b73_01.1b",  0x120000, 0x010000, CRC(3a8a4489) SHA1(a01d7300015f90ce6dd571ad93e7a58270a99e47) )
	ROM_LOAD( "b73_02.1c",  0x130000, 0x010000, CRC(025c536c) SHA1(075e95cc39e792049ae656404e7f7440df064391) )
	ROM_LOAD( "b73_03.1d",  0x140000, 0x010000, CRC(ec6020cf) SHA1(2973aa2dc3deb2f27c9f1bad07a7664bad95b3f2) )
	ROM_LOAD( "b73_04.1f",  0x150000, 0x010000, CRC(f7badbb2) SHA1(d824ab4aba94d7ca02401f4f6f34213143c282ec) )
	ROM_LOAD( "b73_05.1h",  0x160000, 0x010000, CRC(b8e829d2) SHA1(31102358500d7b58173d4f18647decf5db744416) )
 
	ROM_REGION( 0x80000, REGION_GFX2, 0 )
	ROM_LOAD( "b73_09.2p",  0x000000, 0x080000, CRC(537c647f) SHA1(941c0f4e251bc68e53d62e70b033a3a6c145bb7e) )
ROM_END
 
ROM_START( hvyunitjo )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "b73_13.5c",  0x00000, 0x20000, CRC(e2874601) SHA1(7f7f3287113b8622eb365d04135d2d9c35d70554) )
 
	ROM_REGION( 0x20000, REGION_CPU2, 0 )
	ROM_LOAD( "b73_14.5p",  0x00000, 0x10000, CRC(0dfb51d4) SHA1(0e6f3b3d4558f12fe1b1620f57a0f4ac2065fd1a) )
 
	ROM_REGION( 0x20000, REGION_CPU3, 0 )
	ROM_LOAD( "b73_12.7e",  0x000000, 0x010000, CRC(d1d24fab) SHA1(ed0312535d0b136d79aa885b9e6eea19ebde6409) )
 
	ROM_REGION( 0x1000, REGION_CPU4, 0 )
	ROM_LOAD( "mermaid.bin",  0x0000, 0x0e00, CRC(88c5dd27) SHA1(5043fed7fd192891be7e4096f2c5daaae1538bc4) )
 
	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROM_LOAD( "b73_08.2f",  0x000000, 0x080000, CRC(f83dd808) SHA1(09d5f1e86fad3a0d2d3ac1845103d3f2833c6793) )
	ROM_LOAD( "b73_07.2c",  0x100000, 0x010000, CRC(5cffa42c) SHA1(687e047345039479b35d5099e56dbc1d57284ed9) )
	ROM_LOAD( "b73_06.2b",  0x120000, 0x010000, CRC(a98e4aea) SHA1(560fef03ad818894c9c7578c6282d55b646e8129) )
	ROM_LOAD( "b73_01.1b",  0x140000, 0x010000, CRC(3a8a4489) SHA1(a01d7300015f90ce6dd571ad93e7a58270a99e47) )
	ROM_LOAD( "b73_02.1c",  0x160000, 0x010000, CRC(025c536c) SHA1(075e95cc39e792049ae656404e7f7440df064391) )
	ROM_LOAD( "b73_03.1d",  0x180000, 0x010000, CRC(ec6020cf) SHA1(2973aa2dc3deb2f27c9f1bad07a7664bad95b3f2) )
	/*                      0x190000, 0x010000  no data */
	ROM_LOAD( "b73_04.1f",  0x1a0000, 0x010000, CRC(f7badbb2) SHA1(d824ab4aba94d7ca02401f4f6f34213143c282ec) )
	/*                      0x1b0000, 0x010000  no data */
	ROM_LOAD( "b73_05.1h",  0x1c0000, 0x010000, CRC(b8e829d2) SHA1(31102358500d7b58173d4f18647decf5db744416) )
 
	ROM_REGION( 0x80000, REGION_GFX2, 0 )
	ROM_LOAD( "b73_09.2p",  0x000000, 0x080000, CRC(537c647f) SHA1(941c0f4e251bc68e53d62e70b033a3a6c145bb7e) )
ROM_END

ROM_START( hvyunitu )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "b73_34.5c",  0x00000, 0x20000, CRC(05c30a90) SHA1(97cc0ded2896e0945d790247c284e5058c28c735) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 )
	ROM_LOAD( "b73_35.6p",  0x00000, 0x10000, CRC(aed1669d) SHA1(d0539261d6128fa2d58b529e8383b6d1f3ccac77) )

	ROM_REGION( 0x20000, REGION_CPU3, 0 )
	ROM_LOAD( "b73_12.7e",  0x000000, 0x010000, CRC(d1d24fab) SHA1(ed0312535d0b136d79aa885b9e6eea19ebde6409) )

	ROM_REGION( 0x1000, REGION_CPU4, 0 )
	ROM_LOAD( "mermaid.bin",  0x0000, 0x0e00, CRC(88c5dd27) SHA1(5043fed7fd192891be7e4096f2c5daaae1538bc4) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROM_LOAD( "b73_08.2f",  0x000000, 0x080000, CRC(f83dd808) SHA1(09d5f1e86fad3a0d2d3ac1845103d3f2833c6793) )
	ROM_LOAD( "b73_28.2c",  0x100000, 0x020000, CRC(a02e08d6) SHA1(72764d4e8474aaac0674fd1c20278a706da7ade2) )
	ROM_LOAD( "b73_27.2b",  0x120000, 0x020000, CRC(8708f97c) SHA1(ccddc7f2fa53c5e35345c2db0520f515c512b723) )
	ROM_LOAD( "b73_25.0b",  0x140000, 0x020000, CRC(2f13f81e) SHA1(9d9c1869bf582a0bc0581cdf5b65237124b9e456) ) /* the data in first half of this actually differs slightly to the other sets, a 0x22 fill is replaced by 0xff on empty tiles */
	ROM_LOAD( "b73_26.0c",  0x160000, 0x010000, CRC(b8e829d2) SHA1(31102358500d7b58173d4f18647decf5db744416) ) /* == b73_05.1h, despite the different label */

	ROM_REGION( 0x80000, REGION_GFX2, 0 )
	ROM_LOAD( "b73_09.2p",  0x000000, 0x080000, CRC(537c647f) SHA1(941c0f4e251bc68e53d62e70b033a3a6c145bb7e) )
ROM_END
 

 
/*************************************
 *
 *  Game driver(s)
 *
 *************************************/
 
 
GAME( 1988, hvyunit,   0,       hvyunit, hvyunit,  0, ROT0, "Kaneko / Taito", "Heavy Unit (World)" )
GAME( 1988, hvyunitj,  hvyunit, hvyunit, hvyunitj, 0, ROT0, "Kaneko / Taito", "Heavy Unit (Japan, Newer)" )
GAME( 1988, hvyunitjo, hvyunit, hvyunit, hvyunitj, 0, ROT0, "Kaneko / Taito", "Heavy Unit (Japan, Older)" )
GAME( 1988, hvyunitu,  hvyunit, hvyunit, hvyunitj, 0, ROT0, "Kaneko / Taito", "Heavy Unit -U.S.A. Version- (US)" )

 
