/***************************************************************************

Wyvern F-0 (1985, Taito)

driver by Luca Elia

Typical Taito mid-80s hardware but with dual video output.

Sound board:    Z80, 2 x YM2149, OKI M5232
CPU board:      Z80, ROM and RAM, 68705P5 MCU (protected)
OBJ board:      48MHz OSC, ROMs and RAM
Video board:    ROMs and RAM, 4 x Fujitsu MB112S146 (also used on arkanoid, lkage)

The rest is just common logic, there are no custom chips.

The cabinet uses a half-silvered mirror to mix the images from two screens for a pseudo-3D effect:

http://www.higenekodo.jp/untiku/wy.htm

Backgrounds and enemies on the ground are displayed in the lower screen, while
player ship and enemies in the air are displayed in the upper screen.
And the cabinet also has two speakers. The sound of enemies on the ground is heard
from the bottom speaker and the sound of enemies in the air is heard from the top speaker.

Actual game video: http://www.nicozon.net/watch/sm10823430

TODO:
-  TA7630;

***************************************************************************/

/*
Differences from MAME:
- The two screens are mixed in a single bitmap, like in MAME.
- The Taito 68705 interface is implemented inline in this driver.
- The YM2149 chips are emulated with the AY-3-8910 core.
- The sound is mixed in mono.
*/

#include "driver.h"
#include "vidhrdw/generic.h"


static UINT8 *vram[2];
static UINT8 *scrollram;
static UINT8 *spriteram_wy;
static UINT8 objram[0x2000];
static UINT8 palram[0x400];

static struct tilemap *tmap[2];

static int sound_nmi_enable;
static int pending_nmi;

static UINT8 from_main;
static UINT8 from_mcu;
static int main_sent;
static int mcu_sent;
static UINT8 portA_in, portA_out, ddrA;
static UINT8 portB_out, ddrB;
static UINT8 portC_out, ddrC;

/***************************************************************************
  Video
***************************************************************************/

static void get_tile_info(int tile_index, int layer)
{
	int offs = tile_index * 2;
	int code = vram[layer][offs] + (vram[layer][offs + 1] << 8);
	int color = (layer << 3) + ((code & 0x3000) >> 12);

	SET_TILE_INFO(1, code & 0x3ff, color, TILE_FLIPXY(code >> 14));
}

static void get_tile_info0(int tile_index)
{
	get_tile_info(tile_index, 0);
}

static void get_tile_info1(int tile_index)
{
	get_tile_info(tile_index, 1);
}

static WRITE_HANDLER( vram0_w )
{
	vram[0][offset] = data;
	tilemap_mark_tile_dirty(tmap[0], offset / 2);
}

static WRITE_HANDLER( vram1_w )
{
	vram[1][offset] = data;
	tilemap_mark_tile_dirty(tmap[1], offset / 2);
}

static WRITE_HANDLER( palette_w )
{
	int v;

	palram[offset] = data;
	offset &= ~1;
	v = (palram[offset] << 8) | palram[offset + 1];
	palette_set_color(offset / 2, pal4bit(v >> 8), pal4bit(v >> 4), pal4bit(v));
}

static VIDEO_START( wyvernf0 )
{
	tmap[0] = tilemap_create(get_tile_info0, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8, 8, 32, 32);
	tmap[1] = tilemap_create(get_tile_info1, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8, 8, 32, 32);
	if (!tmap[0] || !tmap[1])
		return 1;

	tilemap_set_transparent_pen(tmap[0], 0);
	tilemap_set_transparent_pen(tmap[1], 0);

	tilemap_set_scrolldx(tmap[0], 0x12, 0xf4);
	tilemap_set_scrolldy(tmap[0], 0, 0);

	tilemap_set_scrolldx(tmap[1], 0x10, 0xf6);
	tilemap_set_scrolldy(tmap[1], 0, 0);

	return 0;
}

static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int foreground)
{
	const struct GfxElement *gfx = Machine->gfx[0];
	UINT8 *sprram = spriteram_wy + (foreground ? 0x80 : 0);
	int offs;

	for (offs = 0; offs < 0x80; offs += 4)
	{
		int sx = sprram[offs + 3] - ((sprram[offs + 2] & 0x80) << 1);
		int sy = 256 - 8 - sprram[offs + 0] - 23;
		int flipx = 0;
		int flipy = sprram[offs + 1] & 0x80;
		int code = sprram[offs + 1] & 0x7f;
		int color = sprram[offs + 2] & 0x0f;
		int x, y;

		if (flip_screen_x)
		{
			flipx = !flipx;
			sx = 256 - 8 - sx - 3 * 8;
		}
		if (flip_screen_y)
		{
			flipy = !flipy;
			sy = 256 - 8 - sy - 3 * 8;
		}

		if (foreground)
		{
			code += 0x80;
			color += 0x10;
		}

		for (y = 0; y < 4; ++y)
		{
			for (x = 0; x < 4; ++x)
			{
				int objoffs = code * 0x20 + (x + y * 4) * 2;

				drawgfx(bitmap, gfx,
					((objram[objoffs + 1] << 8) + objram[objoffs]) % gfx->total_elements,
					color,
					flipx, flipy,
					sx + (flipx ? 3 - x : x) * 8, sy + (flipy ? 3 - y : y) * 8,
					cliprect, TRANSPARENCY_PEN, 0);
			}
		}
	}
}

static VIDEO_UPDATE( wyvernf0 )
{
	tilemap_set_scrollx(tmap[1], 0, scrollram[0]);
	tilemap_set_scrolly(tmap[1], 0, scrollram[1]);

	tilemap_set_scrollx(tmap[0], 0, scrollram[2]);
	tilemap_set_scrolly(tmap[0], 0, scrollram[3]);

	fillbitmap(bitmap, 0, cliprect);

	/* background monitor */
	tilemap_draw(bitmap, cliprect, tmap[0], 0, 0);
	draw_sprites(bitmap, cliprect, 0);

	/* foreground monitor */
	draw_sprites(bitmap, cliprect, 1);
	tilemap_draw(bitmap, cliprect, tmap[1], 0, 0);
}

/***************************************************************************
  MCU
***************************************************************************/

static READ_HANDLER( mcu_status_r )
{
	/* bit 0 = when 1, MCU is ready to receive data from main CPU
	   bit 1 = when 1, MCU has sent data to the main CPU */
	return (main_sent ? 0x00 : 0x01) | (mcu_sent ? 0x02 : 0x00);
}

static READ_HANDLER( mcu_r )
{
	mcu_sent = 0;
	return from_mcu;
}

static WRITE_HANDLER( mcu_w )
{
	from_main = data;
	main_sent = 1;
	cpu_set_irq_line(2, 0, ASSERT_LINE);
}

static READ_HANDLER( mcu_portA_r )
{
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

static WRITE_HANDLER( mcu_portA_w )
{
	portA_out = data;
}

static WRITE_HANDLER( mcu_ddrA_w )
{
	ddrA = data;
}

static READ_HANDLER( mcu_portB_r )
{
	return portB_out & ddrB;
}

static WRITE_HANDLER( mcu_portB_w )
{
	/* PB1 high to low enables the latch with the command of the main CPU */
	if ((ddrB & 0x02) && (~data & 0x02) && (portB_out & 0x02))
	{
		portA_in = from_main;
		if (main_sent)
			cpu_set_irq_line(2, 0, CLEAR_LINE);
		main_sent = 0;
	}

	/* PB2 low to high copies the port A to the latch for the main CPU */
	if ((ddrB & 0x04) && (data & 0x04) && (~portB_out & 0x04))
	{
		from_mcu = portA_out;
		mcu_sent = 1;
	}

	portB_out = data;
}

static WRITE_HANDLER( mcu_ddrB_w )
{
	ddrB = data;
}

static READ_HANDLER( mcu_portC_r )
{
	UINT8 in = 0;

	if (main_sent)
		in |= 0x01;
	if (!mcu_sent)
		in |= 0x02;

	return (portC_out & ddrC) | (in & ~ddrC);
}

static WRITE_HANDLER( mcu_portC_w )
{
	portC_out = data;
}

static WRITE_HANDLER( mcu_ddrC_w )
{
	ddrC = data;
}

/***************************************************************************
  Memory maps
***************************************************************************/

static WRITE_HANDLER( rambank_w )
{
	flip_screen_x_set(data & 0x01);
	flip_screen_y_set(data & 0x02);

	coin_lockout_w(0, !(data & 0x40));
	coin_lockout_w(1, !(data & 0x40));

	cpu_setbank(2, objram + ((data & 0x80) ? 0x1000 : 0));
}

static WRITE_HANDLER( rombank_w )
{
	cpu_setbank(1, memory_region(REGION_USER1) + (data & 0x07) * 0x2000);
}

static void nmi_callback(int param)
{
	if (sound_nmi_enable)
		cpu_set_nmi_line(1, PULSE_LINE);
	else
		pending_nmi = 1;
}

static WRITE_HANDLER( sound_command_w )
{
	soundlatch_w(0, data);
	timer_set(TIME_NOW, data, nmi_callback);
}

static WRITE_HANDLER( nmi_disable_w )
{
	sound_nmi_enable = 0;
}

static WRITE_HANDLER( nmi_enable_w )
{
	sound_nmi_enable = 1;
	if (pending_nmi)
	{
		cpu_set_nmi_line(1, PULSE_LINE);
		pending_nmi = 0;
	}
}

static MEMORY_READ_START( main_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9000, 0x9fff, MRA_BANK2 },
	{ 0xa000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xcfff, MRA_RAM },
	{ 0xd300, 0xd303, MRA_RAM },
	{ 0xd400, 0xd400, mcu_r },
	{ 0xd401, 0xd401, mcu_status_r },
	{ 0xd500, 0xd5ff, MRA_RAM },
	{ 0xd600, 0xd600, input_port_0_r },
	{ 0xd601, 0xd601, input_port_1_r },
	{ 0xd602, 0xd602, input_port_2_r },
	{ 0xd603, 0xd603, input_port_3_r },
	{ 0xd604, 0xd604, input_port_4_r },
	{ 0xd605, 0xd605, input_port_5_r },
	{ 0xd606, 0xd606, input_port_6_r },
    { 0xd607, 0xd607, input_port_7_r },
	{ 0xd610, 0xd610, soundlatch_r },
	{ 0xd800, 0xdbff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( main_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },
	{ 0x9000, 0x9fff, MWA_BANK2 },
	{ 0xc000, 0xc7ff, vram1_w, &vram[1] },
	{ 0xc800, 0xcfff, vram0_w, &vram[0] },
	{ 0xd000, 0xd000, MWA_NOP },
	{ 0xd100, 0xd100, rambank_w },
	{ 0xd200, 0xd200, rombank_w },
	{ 0xd300, 0xd303, MWA_RAM, &scrollram },
	{ 0xd400, 0xd400, mcu_w },
	{ 0xd500, 0xd5ff, MWA_RAM, &spriteram_wy },
	{ 0xd610, 0xd610, sound_command_w },
	{ 0xd800, 0xdbff, palette_w },
	{ 0xdc00, 0xdc00, MWA_NOP },
MEMORY_END

static MEMORY_READ_START( sound_readmem )
    { 0x0000, 0x3fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd000, soundlatch_r },
	{ 0xe000, 0xefff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
    { 0x0000, 0x3fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc800, AY8910_control_port_0_w },
	{ 0xc801, 0xc801, AY8910_write_port_0_w },
	{ 0xc802, 0xc802, AY8910_control_port_1_w },
	{ 0xc803, 0xc803, AY8910_write_port_1_w },
	{ 0xc900, 0xc90d, MSM5232_0_w },
	{ 0xca00, 0xca00, MWA_NOP },
	{ 0xcb00, 0xcb00, MWA_NOP },
	{ 0xcc00, 0xcc00, MWA_NOP },
	{ 0xd000, 0xd000, soundlatch_w },
	{ 0xd200, 0xd200, nmi_enable_w },
	{ 0xd400, 0xd400, nmi_disable_w },
	{ 0xd600, 0xd600, DAC_0_data_w },
	{ 0xe000, 0xefff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( mcu_readmem )
//	ADDRESS_MAP_FLAGS( AMEF_ABITS(11) )
	{ 0x0000, 0x0000, mcu_portA_r },
	{ 0x0001, 0x0001, mcu_portB_r },
	{ 0x0002, 0x0002, mcu_portC_r },
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( mcu_writemem )
//	ADDRESS_MAP_FLAGS( AMEF_ABITS(11) )
	{ 0x0000, 0x0000, mcu_portA_w },
	{ 0x0001, 0x0001, mcu_portB_w },
	{ 0x0002, 0x0002, mcu_portC_w },
	{ 0x0004, 0x0004, mcu_ddrA_w },
	{ 0x0005, 0x0005, mcu_ddrB_w },
	{ 0x0006, 0x0006, mcu_ddrC_w },
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
MEMORY_END

/***************************************************************************
  Input ports
***************************************************************************/

INPUT_PORTS_START( wyvernf0 )
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "?? 0" )
	PORT_DIPSETTING(    0x01, "?? 1" )
	PORT_DIPSETTING(    0x02, "?? 2" )
	PORT_DIPSETTING(    0x03, "?? 3" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x0f, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_8C ) )
	PORT_DIPNAME( 0xf0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_8C ) )

	PORT_START	/* DSW3 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Coinage Display" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, "Copyright" )
	PORT_DIPSETTING(    0x00, "Taito Corporation" )
	PORT_DIPSETTING(    0x20, "Taito Corp. 1985" )
	PORT_DIPNAME( 0x40, 0x40, "Invulnerability (Cheat)")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Coin Slots" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x80, "2" )

	PORT_START	/* SYSTEM */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* JOY1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER1)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER1)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER1)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER1)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* FIRE1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER1)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* JOY2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* FIRE2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

/***************************************************************************
  Graphics layouts
***************************************************************************/

static struct GfxLayout layout_8x8x4 =
{
	8,8,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(0,4), RGN_FRAC(1,4), RGN_FRAC(2,4), RGN_FRAC(3,4) },
	{ STEP8(7,-1) },
	{ STEP8(0,8) },
	8*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_8x8x4, 0, 32 },	/* sprites */
	{ REGION_GFX2, 0, &layout_8x8x4, 0, 32 },	/* tilemaps */
	{ -1 }
};

/***************************************************************************
  Machine drivers
***************************************************************************/

static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	2000000,	/* 2.0 MHz ? */
	{ 100, 100 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MSM5232interface msm5232_interface =
{
    1,
	2000000, /* 2.0 MHz*/
	{ { 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6, 1.0e-6 } },
	{25}
};

static struct DACinterface dac_interface =
{
	1,
	{ 25 }
};


static MACHINE_INIT( wyvernf0 )
{
	sound_nmi_enable = 0;
	pending_nmi = 0;

	from_main = 0;
	from_mcu = 0;
	main_sent = 0;
	mcu_sent = 0;
	portA_in = portA_out = ddrA = 0;
	portB_out = ddrB = 0;
	portC_out = ddrC = 0;

	cpu_setbank(1, memory_region(REGION_USER1));
	cpu_setbank(2, objram);
}

static MACHINE_DRIVER_START( wyvernf0 )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 48000000/8)
	MDRV_CPU_MEMORY(main_readmem, main_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold, 1)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_MEMORY(sound_readmem, sound_writemem)
	MDRV_CPU_PERIODIC_INT(irq0_line_hold, (60*2))

	MDRV_CPU_ADD(M68705, 48000000/16)
	MDRV_CPU_MEMORY(mcu_readmem, mcu_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)

	MDRV_MACHINE_INIT(wyvernf0)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(512)

	MDRV_VIDEO_START(wyvernf0)
	MDRV_VIDEO_UPDATE(wyvernf0)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
	MDRV_SOUND_ADD(MSM5232, msm5232_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

/***************************************************************************
  ROMs
***************************************************************************/

ROM_START( wyvernf0 )
	ROM_REGION( 0x8000, REGION_CPU1, 0 )
	ROM_LOAD( "a39_01-1.ic37", 0x0000, 0x4000, CRC(a94887ec) SHA1(0b4406290810494e88442dcec7a750c7d3cf316a) )
	ROM_LOAD( "a39_02-1.ic36", 0x4000, 0x4000, CRC(171cfdbe) SHA1(41d922df00c869b8f1f6a026dbe102afffc42cc6) )

	ROM_REGION( 0x10000, REGION_USER1, 0 )
	ROM_LOAD( "a39_03.ic35", 0x0000, 0x4000, CRC(50314281) SHA1(0f4805f06b92c170469b7bc2c0342db919107a91) )
	ROM_LOAD( "a39_04.ic34", 0x4000, 0x4000, CRC(7a225bf9) SHA1(4f0c287051e27f5bc936736225003a685cdf8ad3) )
	ROM_LOAD( "a39_05.ic33", 0x8000, 0x4000, CRC(41f21a67) SHA1(bee4a692259c727baf5fc4f47e09efb953b1c94e) )
	ROM_LOAD( "a39_06.ic32", 0xc000, 0x4000, CRC(deb2d850) SHA1(1d1f265e320fb2a48507c3133fd8a080f7bc4846) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "a39_16.ic26", 0x0000, 0x4000, CRC(5a681fb4) SHA1(e31e751a54fa9853acb462ce22dd2ff5286808f0) )
	ROM_FILL(                0xe000, 0x2000, 0xff )

	ROM_REGION( 0x0800, REGION_CPU3, 0 )
	ROM_LOAD( "a39_mc68705p5s.ic23", 0x0000, 0x0800, CRC(14bff574) SHA1(c91446540e7628b3e62135e2f560a118f7e0dad4) )

	ROM_REGION( 0x10000, REGION_GFX1, 0 )
	ROM_LOAD( "a39_11.ic99", 0x0000, 0x4000, CRC(af70e1dc) SHA1(98dba673750cdfdf25c119c24da10428eff6591b) )
	ROM_LOAD( "a39_10.ic78", 0x4000, 0x4000, CRC(a84380fb) SHA1(ed77892c1a789040fdfecd5903a23b8cbc1df1da) )
	ROM_LOAD( "a39_09.ic96", 0x8000, 0x4000, CRC(c0cee243) SHA1(97f66dde552c7a011ecc7ca8da0e62bc83ef8102) )
	ROM_LOAD( "a39_08.ic75", 0xc000, 0x4000, CRC(0ad69501) SHA1(29037c60bed9435568e997689d193f161f6a4f5b) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_LOAD( "a39_15.ic99",  0x0000, 0x2000, CRC(90a66147) SHA1(8515c43980b7fa55933ca74fb23172e8c832a830) )
	ROM_LOAD( "a39_14.ic73",  0x2000, 0x2000, CRC(a31f3507) SHA1(f72e089dbd700639d64e418812d4b6f4dc1dff75) )
	ROM_LOAD( "a39_13.ic100", 0x4000, 0x2000, CRC(be708238) SHA1(f12d433af7bf6010dea9454a1b3bb2990a42a372) )
	ROM_LOAD( "a39_12.ic74",  0x6000, 0x2000, CRC(1cc389de) SHA1(4213484d3a82688f312811e7a5c4d128e40584c3) )
ROM_END

ROM_START( wyvernf0a )
	ROM_REGION( 0x8000, REGION_CPU1, 0 )
	ROM_LOAD( "soft1_c2a0.ic37", 0x0000, 0x4000, CRC(15f0beb8) SHA1(4105f7064bf94460a020aecca8795553870e3fdc) )
	ROM_LOAD( "soft2_7b60.ic36", 0x4000, 0x4000, CRC(569a40c4) SHA1(5391b6cdc854277e63e4658f79889da4a941ee42) )

	ROM_REGION( 0x10000, REGION_USER1, 0 )
	ROM_LOAD( "ext1.ic35",      0x0000, 0x4000, CRC(50314281) SHA1(0f4805f06b92c170469b7bc2c0342db919107a91) )
	ROM_LOAD( "ext2.ic34",      0x4000, 0x4000, CRC(7a225bf9) SHA1(4f0c287051e27f5bc936736225003a685cdf8ad3) )
	ROM_LOAD( "ext3.ic33",      0x8000, 0x4000, CRC(41f21a67) SHA1(bee4a692259c727baf5fc4f47e09efb953b1c94e) )
	ROM_LOAD( "ext4_8ca8.ic32", 0xc000, 0x4000, CRC(793e36de) SHA1(2a316d832ce524250c36602ca910bb4c8befa15d) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "sound_4182.ic26", 0x0000, 0x4000, CRC(5a681fb4) SHA1(e31e751a54fa9853acb462ce22dd2ff5286808f0) )
	ROM_FILL(                    0xe000, 0x2000, 0xff )

	ROM_REGION( 0x0800, REGION_CPU3, 0 )
	ROM_LOAD( "a39_mc68705p5s.ic23", 0x0000, 0x0800, CRC(14bff574) SHA1(c91446540e7628b3e62135e2f560a118f7e0dad4) )

	ROM_REGION( 0x10000, REGION_GFX1, 0 )
	ROM_LOAD( "obj4_d779.ic99", 0x0000, 0x4000, CRC(af70e1dc) SHA1(98dba673750cdfdf25c119c24da10428eff6591b) )
	ROM_LOAD( "obj3_5852.ic78", 0x4000, 0x4000, CRC(a84380fb) SHA1(ed77892c1a789040fdfecd5903a23b8cbc1df1da) )
	ROM_LOAD( "obj2_50fd.ic96", 0x8000, 0x4000, CRC(c0cee243) SHA1(97f66dde552c7a011ecc7ca8da0e62bc83ef8102) )
	ROM_LOAD( "obj1_bd50.ic75", 0xc000, 0x4000, CRC(0ad69501) SHA1(29037c60bed9435568e997689d193f161f6a4f5b) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_LOAD( "sch_4.ic99",  0x0000, 0x2000, CRC(90a66147) SHA1(8515c43980b7fa55933ca74fb23172e8c832a830) )
	ROM_LOAD( "sch_3.ic73",  0x2000, 0x2000, CRC(a31f3507) SHA1(f72e089dbd700639d64e418812d4b6f4dc1dff75) )
	ROM_LOAD( "sch_2.ic100", 0x4000, 0x2000, CRC(be708238) SHA1(f12d433af7bf6010dea9454a1b3bb2990a42a372) )
	ROM_LOAD( "sch_1.ic74",  0x6000, 0x2000, CRC(1cc389de) SHA1(4213484d3a82688f312811e7a5c4d128e40584c3) )
ROM_END

GAMEX( 1985, wyvernf0,  0,        wyvernf0, wyvernf0, 0, ROT270, "Taito Corporation", "Wyvern F-0 (Rev 1)", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
GAMEX( 1985, wyvernf0a, wyvernf0, wyvernf0, wyvernf0, 0, ROT270, "Taito Corporation", "Wyvern F-0", GAME_IMPERFECT_COLORS | GAME_IMPERFECT_SOUND )
