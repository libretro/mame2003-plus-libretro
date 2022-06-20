/***************************************************************************

	Rabbit Punch / Rabio Lepus

    driver by Aaron Giles

****************************************************************************

	Memory map

****************************************************************************

	========================================================================
	MAIN CPU
	========================================================================
	000000-03FFFF   R     xxxxxxxx xxxxxxxx   Program ROM
	040000-04FFFF   R/W   xxxxxxxx xxxxxxxx   Bitmap RAM (512x256 pixels)
	                R/W   xxxx---- --------      (leftmost pixel)
	                R/W   ----xxxx --------      (2nd pixel)
	                R/W   -------- xxxx----      (3rd pixel)
	                R/W   -------- ----xxxx      (rightmost pixel)
	060000-060FFF   R/W   xxxxxxxx xxxxxxxx   Sprite RAM (512 entries x 4 words)
	                R/W   -------x xxxxxxxx      (0: horizontal position)
	                R/W   xxx----- --------      (1: color index)
	                R/W   ---x---- --------      (1: horizontal flip)
	                R/W   -----xxx xxxxxxxx      (1: image number)
	                R/W   -------x xxxxxxxx      (2: Y position)
	                R/W   -------- --------      (3: not used)
	080000-081FFF   R/W   xxxxxxxx xxxxxxxx   Background 1 RAM (64x64 tiles)
	                R/W   xxx----- --------      (color index)
	                R/W   ---xxxxx xxxxxxxx      (image number)
	082000-083FFF   R/W   xxxxxxxx xxxxxxxx   Background 2 RAM (64x64 tiles)
	                R/W   xxx----- --------      (color index)
	                R/W   ---xxxxx xxxxxxxx      (image number)
	0A0000-0A01FF   R/W   -xxxxxxx xxxxxxxx   Background 1 palette RAM (256 entries)
	                R/W   -xxxxx-- --------      (red component)
	                R/W   ------xx xxx-----      (green component)
	                R/W   -------- ---xxxxx      (blue component)
	0A0200-0A03FF   R/W   -xxxxxxx xxxxxxxx   Background 2 palette RAM (256 entries)
	0A0400-0A05FF   R/W   -xxxxxxx xxxxxxxx   Bitmap palette RAM (256 entries)
	0A0600-0A07FF   R/W   -xxxxxxx xxxxxxxx   Sprite palette RAM (256 entries)
	0C0000            W   -------x xxxxxxxx   Background 1 vertical scroll
	0C0002            W   -------x xxxxxxxx   Background 1 horizontal scroll
	0C0004            W   -------x xxxxxxxx   Background 2 vertical scroll
	0C0006            W   -------x xxxxxxxx   Background 2 horizontal scroll
	0C0008            W   -------- ????????   Video controller data (CRTC)
	0C000C            W   ---xxx-- -xxxxxxx   Video flags
	                  W   ---x---- --------      (flip screen)
	                  W   ----x--- --------      (background 2 image bank)
	                  W   -----x-- --------      (background 1 image bank)
	                  W   -------- -x------      (sprite palette bank)
	                  W   -------- --x-----      (background 2 palette bank)
	                  W   -------- ---x----      (background 1 palette bank)
	                  W   -------- ----xxxx      (bitmap palette bank)
	0C000E            W   -------- xxxxxxxx   Sound communications
	0C0010            W   -------- --xxxxxx   Sprite bias (???)
	0C0012            W   -------- --xxxxxx   Bitmap bias (???)
	0C0018          R     -xxxx-xx --xxxxxx   Player 1 input port
	                R     -x------ --------      (2 player start)
	                R     --x----- --------      (1 player start)
	                R     ---x---- --------      (coin 1)
	                R     ----x--- --------      (coin 2)
	                R     ------x- --------      (test switch)
	                R     -------x --------      (service coin)
	                R     -------- --x-----      (punch button)
	                R     -------- ---x----      (missile button)
	                R     -------- ----xxxx      (joystick right/left/down/up)
	0C001A          R     -xxxx-xx --xxxxxx   Player 2 input port
	0C001C          R     xxxxxxxx xxxxxxxx   DIP switches
	                R     x------- --------      (flip screen)
	                R     -x------ --------      (continues allowed)
	                R     --x----- --------      (demo sounds)
	                R     ---x---- --------      (extended play)
	                R     ----x--- --------      (laser control)
	                R     -----x-- --------      (number of lives)
	                R     ------xx --------      (difficulty)
	                R     -------- xxxx----      (coinage 2)
	                R     -------- ----xxxx      (coinage 1)
	0C001E          R     -------- -------x   Sound busy flag
	0C0028            W   -------- ????????   Video controller register select (CRTC)
	0FC000-0FFFFF   R/W   xxxxxxxx xxxxxxxx   Program RAM
	========================================================================
	Interrupts:
		IRQ1 = VBLANK
	========================================================================


	========================================================================
	SOUND CPU
	========================================================================
	0000-EFFF   R     xxxxxxxx   Program ROM
	F000-F001   R/W   xxxxxxxx   YM2151 communications
	F200        R     xxxxxxxx   Sound command input
	F400          W   x------x   UPD7759 control
	              W   x-------      (/RESET line)
	              W   -------x      (ROM bank select)
	F600          W   xxxxxxxx   UPD7759 data/trigger
	F800-FFFF   R/W   xxxxxxxx   Program RAM
	========================================================================
	Interrupts:
		IRQ = YM2151 IRQ or'ed with the sound command latch flag
	========================================================================

***************************************************************************/


#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/generic.h"
#include <math.h>



#define MASTER_CLOCK		16000000


/* video driver data & functions */
VIDEO_START( rpunch );
VIDEO_UPDATE( rpunch );

extern data16_t *rpunch_bitmapram;
extern size_t rpunch_bitmapram_size;
extern int rpunch_sprite_palette;

static UINT8 sound_data;
static UINT8 sound_busy;
static UINT8 ym2151_irq;
static UINT8 upd_rom_bank;

WRITE16_HANDLER( rpunch_bitmap_w );
WRITE16_HANDLER( rpunch_videoram_w );
WRITE16_HANDLER( rpunch_videoreg_w );
WRITE16_HANDLER( rpunch_scrollreg_w );
WRITE16_HANDLER( rpunch_ins_w );
WRITE16_HANDLER( rpunch_crtc_data_w );
WRITE16_HANDLER( rpunch_crtc_register_w );



/*************************************
 *
 *	Interrupt handling
 *
 *************************************/

static void ym2151_irq_gen(int state)
{
	ym2151_irq = state;
	cpu_set_irq_line(1, 0, (ym2151_irq | sound_busy) ? ASSERT_LINE : CLEAR_LINE);
}


MACHINE_INIT( rpunch )
{
	memcpy(memory_region(REGION_SOUND1), memory_region(REGION_SOUND1) + 0x20000, 0x20000);
}



/*************************************
 *
 *	Input ports
 *
 *************************************/

READ16_HANDLER( common_port_r )
{
	return readinputport(offset) | readinputport(2);
}



/*************************************
 *
 *	Sound I/O
 *
 *************************************/

void sound_command_w_callback(int data)
{
	sound_busy = 1;
	sound_data = data;
	cpu_set_irq_line(1, 0, (ym2151_irq | sound_busy) ? ASSERT_LINE : CLEAR_LINE);
}


static WRITE16_HANDLER( sound_command_w )
{
	if (ACCESSING_LSB)
		timer_set(TIME_NOW, data & 0xff, sound_command_w_callback);
}


static READ_HANDLER( sound_command_r )
{
	sound_busy = 0;
	cpu_set_irq_line(1, 0, (ym2151_irq | sound_busy) ? ASSERT_LINE : CLEAR_LINE);
	return sound_data;
}


static READ16_HANDLER( sound_busy_r )
{
	return sound_busy;
}



/*************************************
 *
 *	UPD7759 controller
 *
 *************************************/

WRITE_HANDLER( upd_control_w )
{
	if ((data & 1) != upd_rom_bank)
	{
		upd_rom_bank = data & 1;
		memcpy(memory_region(REGION_SOUND1), memory_region(REGION_SOUND1) + 0x20000 * (upd_rom_bank + 1), 0x20000);
	}
	upd7759_reset_w(0, data >> 7);
}


WRITE_HANDLER( upd_data_w )
{
	upd7759_port_w(0, data);
	upd7759_start_w(0, 0);
	upd7759_start_w(0, 1);
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static MEMORY_READ16_START( readmem )
	MEMORY_ADDRESS_BITS(20)
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x040000, 0x04ffff, MRA16_RAM },
	{ 0x060000, 0x060fff, MRA16_RAM },
	{ 0x080000, 0x083fff, MRA16_RAM },
	{ 0x0c0018, 0x0c001b, common_port_r },
	{ 0x0c001c, 0x0c001d, input_port_3_word_r },
	{ 0x0c001e, 0x0c001f, sound_busy_r },
	{ 0x0a0000, 0x0a07ff, MRA16_RAM },
	{ 0x0fc000, 0x0fffff, MRA16_RAM },
MEMORY_END


static MEMORY_WRITE16_START( writemem )
	MEMORY_ADDRESS_BITS(20)
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x040000, 0x04ffff, rpunch_bitmap_w, &rpunch_bitmapram, &rpunch_bitmapram_size },
	{ 0x060000, 0x060fff, MWA16_RAM, &spriteram16 },
	{ 0x080000, 0x083fff, rpunch_videoram_w, &videoram16, &videoram_size },
	{ 0x0a0000, 0x0a07ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0x0c0000, 0x0c0007, rpunch_scrollreg_w },
	{ 0x0c0008, 0x0c0009, rpunch_crtc_data_w },
	{ 0x0c000c, 0x0c000d, rpunch_videoreg_w },
	{ 0x0c000e, 0x0c000f, sound_command_w },
	{ 0x0c0010, 0x0c0013, rpunch_ins_w },
	{ 0x0c0028, 0x0c0029, rpunch_crtc_register_w },
	{ 0x0fc000, 0x0fffff, MWA16_RAM },
MEMORY_END



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

static MEMORY_READ_START( readmem_sound )
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf001, YM2151_status_port_0_r },
	{ 0xf200, 0xf200, sound_command_r },
	{ 0xf800, 0xffff, MRA_RAM },
MEMORY_END


static MEMORY_WRITE_START( writemem_sound )
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf000, YM2151_register_port_0_w },
	{ 0xf001, 0xf001, YM2151_data_port_0_w },
	{ 0xf400, 0xf400, upd_control_w },
	{ 0xf600, 0xf600, upd_data_w },
	{ 0xf800, 0xffff, MWA_RAM },
MEMORY_END



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( rpunch )
	PORT_START	/* c0018 lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001a lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c0018/c001a upper 8 bits */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x0200, IP_ACTIVE_HIGH )
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001c DIP switches */
	PORT_DIPNAME( 0x000f, 0x0000, DEF_STR( Coin_A ))
	PORT_DIPSETTING(      0x000d, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0005, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x00f0, 0x0000, DEF_STR( Coin_B ))
	PORT_DIPSETTING(      0x00d0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x00c0, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0050, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x0300, 0x0000, DEF_STR( Difficulty ))
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0000, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0300, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Lives ))
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0400, "3" )
	PORT_DIPNAME( 0x0800, 0x0000, "Laser" )
	PORT_DIPSETTING(      0x0000, "Manual" )
	PORT_DIPSETTING(      0x0800, "Semi-Automatic" )
	PORT_DIPNAME( 0x1000, 0x0000, "Extended Play" )
	PORT_DIPSETTING(      0x0000, "500000 points" )
	PORT_DIPSETTING(      0x1000, "None" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x0000, "Continues" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END


INPUT_PORTS_START( rabiolep )
	PORT_START	/* c0018 lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001a lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c0018/c001a upper 8 bits */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x0200, IP_ACTIVE_HIGH )
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001c DIP switches */
	PORT_DIPNAME( 0x000f, 0x0000, DEF_STR( Coin_A ))
	PORT_DIPSETTING(      0x000d, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0005, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x00f0, 0x0000, DEF_STR( Coin_B ))
	PORT_DIPSETTING(      0x00d0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x00c0, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0050, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x0300, 0x0000, DEF_STR( Difficulty ))
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0000, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0300, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Lives ))
	PORT_DIPSETTING(      0x0400, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0800, 0x0000, "Laser" )
	PORT_DIPSETTING(      0x0800, "Manual" )
	PORT_DIPSETTING(      0x0000, "Semi-Automatic" )
	PORT_DIPNAME( 0x1000, 0x0000, "Extended Play" )
	PORT_DIPSETTING(      0x0000, "500000 points" )
	PORT_DIPSETTING(      0x1000, "300000 points" )
	PORT_DIPNAME( 0x2000, 0x0000, DEF_STR( Demo_Sounds ))
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x0000, "Continues" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END



INPUT_PORTS_START( svolley )
	PORT_START	/* c0018 lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001a lower 8 bits */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c0018/c001a upper 8 bits */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0700, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* c001c DIP switches */
	PORT_DIPNAME( 0x000f, 0x0000, DEF_STR( Coin_A ))
	PORT_DIPSETTING(      0x000d, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0005, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0009, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x00f0, 0x0000, DEF_STR( Coin_B ))
	PORT_DIPSETTING(      0x00d0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(      0x00c0, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(      0x0050, "6 Coins/4 Credits")
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ))
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(      0x0090, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ))
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0x0100, 0x0000, "Game Time")
	PORT_DIPSETTING(      0x0100, "2 min/1 min" )
	PORT_DIPSETTING(      0x0000, "3 min/1.5 min" )
	PORT_DIPNAME( 0x0600, 0x0000, "2P Starting Score" )
	PORT_DIPSETTING(      0x0600, "0-0" )
	PORT_DIPSETTING(      0x0400, "5-5" )
	PORT_DIPSETTING(      0x0000, "7-7" )
	PORT_DIPSETTING(      0x0200, "9-9" )
	PORT_DIPNAME( 0x1800, 0x0000, "1P Starting Score" )
	PORT_DIPSETTING(      0x1000, "9-11" )
	PORT_DIPSETTING(      0x1800, "10-10" )
	PORT_DIPSETTING(      0x0800, "10-11" )
	PORT_DIPSETTING(      0x0000, "11-11" )
	PORT_SERVICE( 0x2000, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Demo_Sounds ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x4000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout bglayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	8*32
};


static struct GfxLayout splayout =
{
	16,32,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 12, 8, 4, 0, 28, 24, 20, 16, 44, 40, 36, 32, 60, 56, 52, 48 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64,
			16*64, 17*64, 18*64, 19*64, 20*64, 21*64, 22*64, 23*64,
			24*64, 25*64, 26*64, 27*64, 28*64, 29*64, 30*64, 31*64 },
	8*256
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &bglayout,   0, 16 },
	{ REGION_GFX2, 0, &bglayout, 256, 16 },
	{ REGION_GFX3, 0, &splayout,   0, 16*4 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	MASTER_CLOCK/4,
	{ YM3012_VOL(50,MIXER_PAN_CENTER,50,MIXER_PAN_CENTER) },
	{ ym2151_irq_gen }
};


static struct upd7759_interface upd7759_interface =
{
	1,			/* 1 chip */
	{ UPD7759_STANDARD_CLOCK },
	{ 50 },
	{ REGION_SOUND1 },
	{ 0 }
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static MACHINE_DRIVER_START( rpunch )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, MASTER_CLOCK/2)
	MDRV_CPU_MEMORY(readmem,writemem)

	MDRV_CPU_ADD(Z80, MASTER_CLOCK/4)
	MDRV_CPU_MEMORY(readmem_sound,writemem_sound)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(rpunch)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(304, 224)
	MDRV_VISIBLE_AREA(8, 303-8, 0, 223-8)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(rpunch)
	MDRV_VIDEO_UPDATE(rpunch)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151, ym2151_interface)
	MDRV_SOUND_ADD(UPD7759, upd7759_interface)
MACHINE_DRIVER_END



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( rpunch )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "rpunch.20", 0x00000, 0x08000, CRC(a2028d59) SHA1(d304811853ad68b3977edb90b94f3e2c7507be82) )
	ROM_LOAD16_BYTE( "rpunch.21", 0x00001, 0x08000, CRC(1cdb13d3) SHA1(a51b2bbbd7b4553500b65aa6a20fabe03432e6ca) )
	ROM_LOAD16_BYTE( "rpunch.2",  0x10000, 0x08000, CRC(9b9729bb) SHA1(3fcf8df9787137d331c062bc4ee97d865af29c78) )
	ROM_LOAD16_BYTE( "rpunch.3",  0x10001, 0x08000, CRC(5704a688) SHA1(3c447cd98ecd25798dbeea328e3ccbaad384c8b1) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "rpunch.92", 0x00000, 0x10000, CRC(5e1870e3) SHA1(0ab33f39144ed72d805341d869f61764610d3df6) )

	ROM_REGION( 0x60000, REGION_GFX1, 0 )
	ROM_LOAD( "rl_c13.bin", 0x00000, 0x40000, CRC(7c8403b0) SHA1(2fb92860a41f3331076c73b2b010e175cb4929ca) )
	ROM_LOAD( "rl_c10.bin", 0x40000, 0x08000, CRC(312eb260) SHA1(31faa90fde54fbc6c110bee7b4690a30beaec469) )
	ROM_LOAD( "rl_c12.bin", 0x48000, 0x08000, CRC(bea85219) SHA1(4036bdad921dd3555a2dc6bb12e9ffa615de70ca) )
	ROM_FILL(               0x50000, 0x10000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX2, 0 )
	ROM_LOAD( "rl_a10.bin", 0x00000, 0x40000, CRC(c2a77619) SHA1(9b1e85fb18833c3b96a6c58b8714984f60a90afc) )
	ROM_LOAD( "rl_a13.bin", 0x40000, 0x08000, CRC(a39c2c16) SHA1(d8d55eb58d3fc79f982f535ec85f69593fe9d883) )
	ROM_LOAD( "rpunch.54",  0x48000, 0x08000, CRC(e2969747) SHA1(8da996fc2e2e3d281f293d0ccaf35ebdb9379d48) )
	ROM_FILL(               0x50000, 0x10000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX3, 0 )
	ROM_LOAD16_BYTE( "rl_4g.bin", 0x00000, 0x20000, CRC(c5cb4b7a) SHA1(2b6be85800ab62b000a0b01cff8af689b25c4c65) )
	ROM_LOAD16_BYTE( "rl_4h.bin", 0x00001, 0x20000, CRC(8a4d3c99) SHA1(29fca0ea60ac040fc0b0f65b9d2069fcffe039bd) )
	ROM_LOAD16_BYTE( "rl_1g.bin", 0x40000, 0x08000, CRC(74d41b2e) SHA1(cd690df46242dd54061bf5f6464203e1f29ce6d8) )
	ROM_LOAD16_BYTE( "rl_1h.bin", 0x40001, 0x08000, CRC(7dcb32bb) SHA1(3401464494447b681456d81199e8e74837419a39) )
	ROM_LOAD16_BYTE( "rpunch.85", 0x50000, 0x08000, CRC(60b88a2c) SHA1(b10aba06a5d88d0f27041f9e356aebf9f8a230df) )
	ROM_LOAD16_BYTE( "rpunch.86", 0x50001, 0x08000, CRC(91d204f6) SHA1(68b5fb29ea5404597adada1a197ad853e79ada1c) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 )
	ROM_LOAD( "rl_f18.bin", 0x20000, 0x20000, CRC(47840673) SHA1(ffe20f8772a987f5dd06a3f348a1e3cfed26e19e) )
/*	ROM_LOAD( "rpunch.91", 0x00000, 0x0f000, CRC(7512cc59) )*/
ROM_END

ROM_START( rabiolep )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "rl_e2.bin", 0x00000, 0x08000, CRC(7d936a12) SHA1(4f472ccbceb13b4d0b14c686d151faa8d4c466c7) )
	ROM_LOAD16_BYTE( "rl_d2.bin", 0x00001, 0x08000, CRC(d8d85429) SHA1(dc52c30c18026300a4625bddc987a00396bcd079) )
	ROM_LOAD16_BYTE( "rl_e4.bin", 0x10000, 0x08000, CRC(5bfaee12) SHA1(c918b1843ad96410f41915574cf12949658c6e90) )
	ROM_LOAD16_BYTE( "rl_d4.bin", 0x10001, 0x08000, CRC(e64216bf) SHA1(851ebab8d7cddb60ab0a8a290df151b35d605e3e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "rl_f20.bin", 0x00000, 0x10000, CRC(a6f50351) SHA1(3152d4ed100b0dfaf0da4ee79cd9e0f1692335e0) )

	ROM_REGION( 0x60000, REGION_GFX1, 0 )
	ROM_LOAD( "rl_c13.bin", 0x00000, 0x40000, CRC(7c8403b0) SHA1(2fb92860a41f3331076c73b2b010e175cb4929ca) )
	ROM_LOAD( "rl_c10.bin", 0x40000, 0x08000, CRC(312eb260) SHA1(31faa90fde54fbc6c110bee7b4690a30beaec469) )
	ROM_LOAD( "rl_c12.bin", 0x48000, 0x08000, CRC(bea85219) SHA1(4036bdad921dd3555a2dc6bb12e9ffa615de70ca) )
	ROM_FILL(               0x50000, 0x10000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX2, 0 )
	ROM_LOAD( "rl_a10.bin", 0x00000, 0x40000, CRC(c2a77619) SHA1(9b1e85fb18833c3b96a6c58b8714984f60a90afc) )
	ROM_LOAD( "rl_a13.bin", 0x40000, 0x08000, CRC(a39c2c16) SHA1(d8d55eb58d3fc79f982f535ec85f69593fe9d883) )
	ROM_LOAD( "rl_a12.bin", 0x48000, 0x08000, CRC(970b0e32) SHA1(a1d4025ee4470a41aa047c6f06ca7aa98a1f7ffd) )
	ROM_FILL(               0x50000, 0x10000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX3, 0 )
	ROM_LOAD16_BYTE( "rl_4g.bin", 0x00000, 0x20000, CRC(c5cb4b7a) SHA1(2b6be85800ab62b000a0b01cff8af689b25c4c65) )
	ROM_LOAD16_BYTE( "rl_4h.bin", 0x00001, 0x20000, CRC(8a4d3c99) SHA1(29fca0ea60ac040fc0b0f65b9d2069fcffe039bd) )
	ROM_LOAD16_BYTE( "rl_1g.bin", 0x40000, 0x08000, CRC(74d41b2e) SHA1(cd690df46242dd54061bf5f6464203e1f29ce6d8) )
	ROM_LOAD16_BYTE( "rl_1h.bin", 0x40001, 0x08000, CRC(7dcb32bb) SHA1(3401464494447b681456d81199e8e74837419a39) )
	ROM_LOAD16_BYTE( "rl_2g.bin", 0x50000, 0x08000, CRC(744903b4) SHA1(ba931a7f6bea8cebab8314551ed34896316b6661) )
	ROM_LOAD16_BYTE( "rl_2h.bin", 0x50001, 0x08000, CRC(09649e75) SHA1(a650561a11970fbcbc4610fc67cb9f54fa3145a6) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 )
	ROM_LOAD( "rl_f18.bin", 0x20000, 0x20000, CRC(47840673) SHA1(ffe20f8772a987f5dd06a3f348a1e3cfed26e19e) )
ROM_END


ROM_START( svolley )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "sps_13.bin", 0x00000, 0x10000, CRC(2fbc5dcf) SHA1(fba4d353948f29b75b4db464509f7e606703f9dc) )
	ROM_LOAD16_BYTE( "sps_11.bin", 0x00001, 0x10000, CRC(51b025c9) SHA1(4eae91ee9fe893083d0ff5fed28d847dba49b244) )
	ROM_LOAD16_BYTE( "sps_14.bin", 0x20000, 0x08000, CRC(e7630122) SHA1(d200afe5134030be615f112af0ab54ac3b349eca) )
	ROM_LOAD16_BYTE( "sps_12.bin", 0x20001, 0x08000, CRC(b6b24910) SHA1(2e4cf80a8eb1fcd9448405ff881bb99ae4ce8909) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "sps_17.bin", 0x00000, 0x10000, CRC(48b89688) SHA1(1f39d979a852f5237a7d95231e86a28cdc1f4d65) )

	ROM_REGION( 0x60000, REGION_GFX1, 0 )
	ROM_LOAD( "sps_02.bin", 0x00000, 0x10000, CRC(1a0abe75) SHA1(49251c5e377f9317471f7df26ac2c6b8cfa51007) )
	ROM_LOAD( "sps_03.bin", 0x10000, 0x10000, CRC(36279075) SHA1(6c4cf3fab9eb764cb8bc10ab4f8aa54d0afb65d9) )
	ROM_LOAD( "sps_04.bin", 0x20000, 0x10000, CRC(7cede7d9) SHA1(9c7e3a9b7dd8d390b327d52ced35b03b8c1fd5ee) )
	ROM_LOAD( "sps_01.bin", 0x30000, 0x08000, CRC(6425e6d7) SHA1(b6c81155c22072d1de88ca23d58bd9621139dc6c) )
	ROM_FILL(               0x38000, 0x08000, 0xff )
	ROM_LOAD( "sps_10.bin", 0x40000, 0x08000, CRC(a12b1589) SHA1(ecaa941f29c028ca94fcd1d86edfd69884e61d2c) )
	ROM_FILL(               0x48000, 0x18000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX2, 0 )
	ROM_LOAD( "sps_05.bin", 0x00000, 0x10000, CRC(b0671d12) SHA1(defc71b6d7c31c74a58789a1620a506f36b85837) )
	ROM_LOAD( "sps_06.bin", 0x10000, 0x10000, CRC(c231957e) SHA1(b56afd41969bd865ad3ca16fb51e39030aeb1943) )
	ROM_LOAD( "sps_07.bin", 0x20000, 0x10000, CRC(904b7709) SHA1(9b66a565cd599928b666baad9f97c50f35ffcc37) )
	ROM_LOAD( "sps_08.bin", 0x30000, 0x10000, CRC(5430ffac) SHA1(163311d96f2f7e1ecb0901d0be73ac357b01bf6a) )
	ROM_LOAD( "sps_09.bin", 0x40000, 0x10000, CRC(414a6278) SHA1(baa9dc9ab0dd3c5f27c128de23053edcddf45ad0) )
	ROM_FILL(               0x50000, 0x10000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX3, 0 )
	ROM_LOAD16_BYTE( "sps_20.bin", 0x00000, 0x10000, CRC(c9e7206d) SHA1(af5b2f49387a3b46c6693f4782aa0e587f17ab25) )
	ROM_LOAD16_BYTE( "sps_23.bin", 0x00001, 0x10000, CRC(7b15c805) SHA1(b55d67956ca10c172f244edce4dc0a8bd155b3ce) )
	ROM_LOAD16_BYTE( "sps_19.bin", 0x20000, 0x08000, CRC(8ac2f232) SHA1(6ccd003d7e6fb933241e58964e682bd0fcc37b35) )
	ROM_LOAD16_BYTE( "sps_22.bin", 0x20001, 0x08000, CRC(fcc754e3) SHA1(84a4083262095d099bca4d5c29829527d981130f) )
	ROM_LOAD16_BYTE( "sps_18.bin", 0x30000, 0x08000, CRC(4d6c8f0c) SHA1(27f58a53cd6aef071c685eda532e4909ea915c8d) )
	ROM_LOAD16_BYTE( "sps_21.bin", 0x30001, 0x08000, CRC(9dd28b42) SHA1(5f49456ee49ed7df59629d02a9da57eac370c388) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 )
	ROM_LOAD( "sps_16.bin", 0x20000, 0x20000, CRC(456d0f36) SHA1(3d1bdc5c79b41a7b33932d6a8b838f01cea9d4ed) )
	ROM_LOAD( "sps_15.bin", 0x40000, 0x10000, CRC(f33f415f) SHA1(1dd465d9b3009754a7d53400562a53dacff364fc) )
ROM_END

ROM_START( svolleyk )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "a14.bin", 0x00000, 0x10000, CRC(dbab3bf9) SHA1(b329245666fb5082871d661714332b10cf49fe41) )
	ROM_LOAD16_BYTE( "a11.bin", 0x00001, 0x10000, CRC(92afd56f) SHA1(6edb421af6051de640c3ed9bb75bd9e7f609ce14) )
	ROM_LOAD16_BYTE( "a15.bin", 0x20000, 0x08000, CRC(d8f89c4a) SHA1(454b42277457d3ebdefb04fc91f6bdc1e0d28439) )
	ROM_LOAD16_BYTE( "a12.bin", 0x20001, 0x08000, CRC(de3dd5cb) SHA1(ea997b91ff45513e69fe76d71da40c8f9dc112f7) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "sps_17.bin", 0x00000, 0x10000, CRC(48b89688) SHA1(1f39d979a852f5237a7d95231e86a28cdc1f4d65) )

	ROM_REGION( 0x60000, REGION_GFX1, 0 )
	ROM_LOAD( "sps_02.bin", 0x00000, 0x10000, CRC(1a0abe75) SHA1(49251c5e377f9317471f7df26ac2c6b8cfa51007) )
	ROM_LOAD( "sps_03.bin", 0x10000, 0x10000, CRC(36279075) SHA1(6c4cf3fab9eb764cb8bc10ab4f8aa54d0afb65d9) )
	ROM_LOAD( "sps_04.bin", 0x20000, 0x10000, CRC(7cede7d9) SHA1(9c7e3a9b7dd8d390b327d52ced35b03b8c1fd5ee) )
	ROM_LOAD( "sps_01.bin", 0x30000, 0x08000, CRC(6425e6d7) SHA1(b6c81155c22072d1de88ca23d58bd9621139dc6c) )
	ROM_FILL(               0x38000, 0x08000, 0xff )
	ROM_LOAD( "sps_10.bin", 0x40000, 0x08000, CRC(a12b1589) SHA1(ecaa941f29c028ca94fcd1d86edfd69884e61d2c) )
	ROM_FILL(               0x48000, 0x18000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX2, 0 )
	ROM_LOAD( "sps_05.bin", 0x00000, 0x10000, CRC(b0671d12) SHA1(defc71b6d7c31c74a58789a1620a506f36b85837) )
	ROM_LOAD( "sps_06.bin", 0x10000, 0x10000, CRC(c231957e) SHA1(b56afd41969bd865ad3ca16fb51e39030aeb1943) )
	ROM_LOAD( "sps_07.bin", 0x20000, 0x10000, CRC(904b7709) SHA1(9b66a565cd599928b666baad9f97c50f35ffcc37) )
	ROM_LOAD( "sps_08.bin", 0x30000, 0x10000, CRC(5430ffac) SHA1(163311d96f2f7e1ecb0901d0be73ac357b01bf6a) )
	ROM_LOAD( "sps_09.bin", 0x40000, 0x10000, CRC(414a6278) SHA1(baa9dc9ab0dd3c5f27c128de23053edcddf45ad0) )
	ROM_LOAD( "a09.bin",    0x50000, 0x08000, CRC(dd92dfe1) SHA1(08c956e11d567a215ec3cdaf6ef75fa9a886513a) )
	ROM_FILL(               0x58000, 0x08000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX3, 0 )
	ROM_LOAD16_BYTE( "sps_20.bin", 0x00000, 0x10000, CRC(c9e7206d) SHA1(af5b2f49387a3b46c6693f4782aa0e587f17ab25) )
	ROM_LOAD16_BYTE( "sps_23.bin", 0x00001, 0x10000, CRC(7b15c805) SHA1(b55d67956ca10c172f244edce4dc0a8bd155b3ce) )
	ROM_LOAD16_BYTE( "sps_19.bin", 0x20000, 0x08000, CRC(8ac2f232) SHA1(6ccd003d7e6fb933241e58964e682bd0fcc37b35) )
	ROM_LOAD16_BYTE( "sps_22.bin", 0x20001, 0x08000, CRC(fcc754e3) SHA1(84a4083262095d099bca4d5c29829527d981130f) )
	ROM_LOAD16_BYTE( "sps_18.bin", 0x30000, 0x08000, CRC(4d6c8f0c) SHA1(27f58a53cd6aef071c685eda532e4909ea915c8d) )
	ROM_LOAD16_BYTE( "sps_21.bin", 0x30001, 0x08000, CRC(9dd28b42) SHA1(5f49456ee49ed7df59629d02a9da57eac370c388) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 )
	ROM_LOAD( "sps_16.bin", 0x20000, 0x20000, CRC(456d0f36) SHA1(3d1bdc5c79b41a7b33932d6a8b838f01cea9d4ed) )
	ROM_LOAD( "sps_15.bin", 0x40000, 0x10000, CRC(f33f415f) SHA1(1dd465d9b3009754a7d53400562a53dacff364fc) )
ROM_END

ROM_START( svolleyu )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "svb-du8.137", 0x00000, 0x10000, CRC(ffd5d261) SHA1(cf1a6897b88481d6dc7ffca647f85b91b04c2242) )
	ROM_LOAD16_BYTE( "svb-du5.136", 0x00001, 0x10000, CRC(c1e943f5) SHA1(dda0db7dcf61038fd14d1717ee036e0e5b92253d) )
	ROM_LOAD16_BYTE( "svb-du9.127", 0x20000, 0x08000, CRC(70e04a2e) SHA1(0dec1e21a7bf611caf3487a31965654ce1fe2989) )
	ROM_LOAD16_BYTE( "svb-du6.126", 0x20001, 0x08000, CRC(acb3872b) SHA1(a720efe3f248c6a8c8f95c5c90bfe7f1e8537911) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "sps_17.bin", 0x00000, 0x10000, CRC(48b89688) SHA1(1f39d979a852f5237a7d95231e86a28cdc1f4d65) )

	ROM_REGION( 0x60000, REGION_GFX1, 0 )
	ROM_LOAD( "sps_02.bin", 0x00000, 0x10000, CRC(1a0abe75) SHA1(49251c5e377f9317471f7df26ac2c6b8cfa51007) )
	ROM_LOAD( "sps_03.bin", 0x10000, 0x10000, CRC(36279075) SHA1(6c4cf3fab9eb764cb8bc10ab4f8aa54d0afb65d9) )
	ROM_LOAD( "sps_04.bin", 0x20000, 0x10000, CRC(7cede7d9) SHA1(9c7e3a9b7dd8d390b327d52ced35b03b8c1fd5ee) )
	ROM_LOAD( "sps_01.bin", 0x30000, 0x08000, CRC(6425e6d7) SHA1(b6c81155c22072d1de88ca23d58bd9621139dc6c) )
	ROM_FILL(               0x38000, 0x08000, 0xff )
	ROM_LOAD( "sps_10.bin", 0x40000, 0x08000, CRC(a12b1589) SHA1(ecaa941f29c028ca94fcd1d86edfd69884e61d2c) )
	ROM_FILL(               0x48000, 0x18000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX2, 0 )
	ROM_LOAD( "sps_05.bin", 0x00000, 0x10000, CRC(b0671d12) SHA1(defc71b6d7c31c74a58789a1620a506f36b85837) )
	ROM_LOAD( "sps_06.bin", 0x10000, 0x10000, CRC(c231957e) SHA1(b56afd41969bd865ad3ca16fb51e39030aeb1943) )
	ROM_LOAD( "sps_07.bin", 0x20000, 0x10000, CRC(904b7709) SHA1(9b66a565cd599928b666baad9f97c50f35ffcc37) )
	ROM_LOAD( "sps_08.bin", 0x30000, 0x10000, CRC(5430ffac) SHA1(163311d96f2f7e1ecb0901d0be73ac357b01bf6a) )
	ROM_LOAD( "sps_09.bin", 0x40000, 0x10000, CRC(414a6278) SHA1(baa9dc9ab0dd3c5f27c128de23053edcddf45ad0) )
	ROM_LOAD( "a09.bin",    0x50000, 0x08000, CRC(dd92dfe1) SHA1(08c956e11d567a215ec3cdaf6ef75fa9a886513a) )
	ROM_FILL(               0x58000, 0x08000, 0xff )

	ROM_REGION( 0x60000, REGION_GFX3, 0 )
	ROM_LOAD16_BYTE( "sps_20.bin", 0x00000, 0x10000, CRC(c9e7206d) SHA1(af5b2f49387a3b46c6693f4782aa0e587f17ab25) )
	ROM_LOAD16_BYTE( "sps_23.bin", 0x00001, 0x10000, CRC(7b15c805) SHA1(b55d67956ca10c172f244edce4dc0a8bd155b3ce) )
	ROM_LOAD16_BYTE( "sps_19.bin", 0x20000, 0x08000, CRC(8ac2f232) SHA1(6ccd003d7e6fb933241e58964e682bd0fcc37b35) )
	ROM_LOAD16_BYTE( "sps_22.bin", 0x20001, 0x08000, CRC(fcc754e3) SHA1(84a4083262095d099bca4d5c29829527d981130f) )
	ROM_LOAD16_BYTE( "sps_18.bin", 0x30000, 0x08000, CRC(4d6c8f0c) SHA1(27f58a53cd6aef071c685eda532e4909ea915c8d) )
	ROM_LOAD16_BYTE( "sps_21.bin", 0x30001, 0x08000, CRC(9dd28b42) SHA1(5f49456ee49ed7df59629d02a9da57eac370c388) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 )
	ROM_LOAD( "sps_16.bin", 0x20000, 0x20000, CRC(456d0f36) SHA1(3d1bdc5c79b41a7b33932d6a8b838f01cea9d4ed) )
	ROM_LOAD( "sps_15.bin", 0x40000, 0x10000, CRC(f33f415f) SHA1(1dd465d9b3009754a7d53400562a53dacff364fc) )
ROM_END



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static DRIVER_INIT( rabiolep )
{
	rpunch_sprite_palette = 0x300;
}


static DRIVER_INIT( svolley )
{
	/* the main differences between Super Volleyball and Rabbit Punch are */
	/* the lack of direct-mapped bitmap and a different palette base for sprites */
	rpunch_sprite_palette = 0x080;
	rpunch_bitmapram = NULL;
}



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1987, rabiolep, 0,        rpunch,   rabiolep, rabiolep, ROT0, "V-System Co.", "Rabio Lepus (Japan)" )
GAME( 1987, rpunch,   rabiolep, rpunch,   rpunch,   rabiolep, ROT0, "V-System Co. (Bally/Midway/Sente license)", "Rabbit Punch (US)" )
GAME( 1989, svolley,  0,        rpunch,   svolley,  svolley,  ROT0, "V-System Co.", "Super Volleyball (Japan)" )
GAME( 1989, svolleyk, svolley,  rpunch,   svolley,  svolley,  ROT0, "V-System Co.", "Super Volleyball (Korea)" )
GAME( 1989, svolleyu, svolley,  rpunch,   svolley,  svolley,  ROT0, "V-System Co. (Data East license)", "Super Volleyball (US)" )
