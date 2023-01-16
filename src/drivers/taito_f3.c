/***************************************************************************

	Taito F3 Package System (aka F3 Cybercore System)

	Emulation by Bryan McPhail, mish@tendril.co.uk/mish@mame.net
	Thanks to Ian Schmidt and Stiletto for sound information!
	Major thanks to Aaron Giles for sound info, figuring out the 68K/ES5505
	rom interface and ES5505 emulator!
	Thanks to Acho A. Tang for Kirameki Star Road sound banking info!
	Thank you to Shiriru for the scanline rendering (including alpha blending),
	sprite sync fixes, sprite zoom fixes and others!

	Main Issues:
		Sound eats lots of memory as 8 bit PCM data is decoded as 16 bit for
			use by the current ES5505 core (which rightly should be 16 bit).
		Zoomed layers are not always positioned quite correctly in flipscreen mode
		(Grid Seeker)

	Other Issues:
		Dsp isn't hooked up.
		Crowd/boards not shown in the football games
		Sound doesn't work in RidingF/RingRage/QTheater?

	Feel free to report any other issues to me.

	Taito custom chips on motherboard:

		TC0630 FDP - Playfield generator?  (Nearest tile roms)
		TC0640 FI0 - I/O & watchdog?
		TC0650 FDA - Priority mixer?  (Near paletteram & video output)
		TC0660 FCM - Sprites? (Nearest sprite roms)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"
#include "taito_f3.h"
#include "state.h"

#include "bootstrap.h"
#include "inptport.h"

VIDEO_START( f3 );
VIDEO_UPDATE( f3 );
VIDEO_EOF( f3 );
VIDEO_STOP( f3 );

extern data32_t *f3_vram,*f3_line_ram;
extern data32_t *f3_pf_data,*f3_pivot_ram;
static data32_t coin_word[2], *f3_ram;
data32_t *f3_shared_ram;
int f3_game;

WRITE32_HANDLER( f3_control_0_w );
WRITE32_HANDLER( f3_control_1_w );
WRITE32_HANDLER( f3_palette_24bit_w );
WRITE32_HANDLER( f3_pf_data_w );
WRITE32_HANDLER( f3_vram_w );
WRITE32_HANDLER( f3_pivot_w );
WRITE32_HANDLER( f3_lineram_w );
WRITE32_HANDLER( f3_videoram_w );

/* from Machine.c */
READ16_HANDLER(f3_68000_share_r);
WRITE16_HANDLER(f3_68000_share_w);
READ16_HANDLER(f3_68681_r);
WRITE16_HANDLER(f3_68681_w);
READ16_HANDLER(es5510_dsp_r);
WRITE16_HANDLER(es5510_dsp_w);
READ16_HANDLER(ridingf_dsp_r);
WRITE16_HANDLER(ridingf_dsp_w);
WRITE16_HANDLER(f3_volume_w);
WRITE16_HANDLER(f3_es5505_bank_w);
void f3_68681_reset(void);

/******************************************************************************/

NVRAM_HANDLER( recalh )
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		static const data8_t recalh_eeprom[128] =	{
			0x85,0x54,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf3,0x35,
			0x00,0x01,0x86,0xa0,0x00,0x13,0x04,0x13,0x00,0x00,0xc3,0x50,0x00,0x19,0x00,0x0a,
			0x00,0x00,0x4e,0x20,0x00,0x03,0x18,0x0d,0x00,0x00,0x27,0x10,0x00,0x05,0x14,0x18,
			0x00,0x00,0x13,0x88,0x00,0x00,0x12,0x27,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
		};

		/* RecalH does not initialise it's eeprom on first boot, so we provide one. */
		EEPROM_init(&eeprom_interface_93C46);
		if (file)
			EEPROM_load(file);
		else if (f3_game==RECALH)
			EEPROM_set_data(recalh_eeprom,128);
	}
}

static READ32_HANDLER( f3_control_r )
{
	int e;

	switch (offset)
	{
		case 0x0: /* MSW: Test switch, coins, eeprom access, LSW: Player Buttons, Start, Tilt, Service */
			e=EEPROM_read_bit();
			e=e|(e<<8);
			return ((e | readinputport(2) | (readinputport(2)<<8))<<16) /* top byte may be mirror of bottom byte??  see bubblem */
					| readinputport(1);

		case 0x1: /* MSW: Coin counters/lockouts are readable, LSW: Joysticks (Player 1 & 2) */
			return (coin_word[0]<<16) | readinputport(0) | 0xff00;

		case 0x2: /* Analog control 1 */
			return ((readinputport(3)&0xf)<<12) | ((readinputport(3)&0xff0)>>4);

		case 0x3: /* Analog control 2 */
			return ((readinputport(4)&0xf)<<12) | ((readinputport(4)&0xff0)>>4);

		case 0x4: /* Player 3 & 4 fire buttons (Player 2 top fire buttons in Kaiser Knuckle) */
			return readinputport(5)<<8;

		case 0x5: /* Player 3 & 4 joysticks (Player 1 top fire buttons in Kaiser Knuckle) */
			return (coin_word[1]<<16) | readinputport(6);
	}

	log_cb(RETRO_LOG_DEBUG, LOGPRE "CPU #0 PC %06x: warning - read unmapped control address %06x\n",activecpu_get_pc(),offset);
	return 0xffffffff;
}

static WRITE32_HANDLER( f3_control_w )
{
	switch (offset)
	{
		case 0x00: /* Watchdog */
			watchdog_reset_w(0,0);
			return;
		case 0x01: /* Coin counters & lockouts */
			if (ACCESSING_MSB32) {
				coin_lockout_w(0,~data & 0x01000000);
				coin_lockout_w(1,~data & 0x02000000);
				coin_counter_w(0, data & 0x04000000);
				coin_counter_w(1, data & 0x08000000);
				coin_word[0]=(data>>16)&0xffff;
			}
			return;
		case 0x04: /* Eeprom */
			if (ACCESSING_LSB32) {
				EEPROM_set_clock_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);
				EEPROM_write_bit(data & 0x04);
				EEPROM_set_cs_line((data & 0x10) ? CLEAR_LINE : ASSERT_LINE);
			}
			return;
		case 0x05:	/* Player 3 & 4 coin counters */
			if (ACCESSING_MSB32) {
				coin_lockout_w(2,~data & 0x01000000);
				coin_lockout_w(3,~data & 0x02000000);
				coin_counter_w(2, data & 0x04000000);
				coin_counter_w(3, data & 0x08000000);
				coin_word[1]=(data>>16)&0xffff;
			}
			return;
	}
	log_cb(RETRO_LOG_DEBUG, LOGPRE "CPU #0 PC %06x: warning - write unmapped control address %06x %08x\n",activecpu_get_pc(),offset,data);
}

static WRITE32_HANDLER( f3_sound_reset_0_w )
{
	cpu_set_reset_line(1, CLEAR_LINE);
}

static WRITE32_HANDLER( f3_sound_reset_1_w )
{
	cpu_set_reset_line(1, ASSERT_LINE);
}

static WRITE32_HANDLER( f3_sound_bankswitch_w )
{
	if (f3_game==KIRAMEKI) {
		data16_t *rom = (data16_t *)memory_region(REGION_CPU2);
		unsigned int idx;

		idx = (offset << 1) & 0x1e;
		if (ACCESSING_LSW32)
			idx += 1;

		if (idx >= 8)
			idx -= 8;

		/* Banks are 0x20000 bytes each, divide by two to get data16
		pointer rather than byte pointer */
		cpu_setbank(2, &rom[(idx*0x20000)/2 + 0x80000]);

	} else {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "Sound bankswitch in unsupported game\n");
	}
}

/******************************************************************************/

static MEMORY_READ32_START( f3_readmem )
	{ 0x000000, 0x1fffff, MRA32_ROM },
  	{ 0x400000, 0x43ffff, MRA32_RAM },
	{ 0x440000, 0x447fff, MRA32_RAM }, /* Palette ram */
	{ 0x4a0000, 0x4a0017, f3_control_r },
	{ 0x600000, 0x60ffff, MRA32_RAM }, /* Object data */
	{ 0x610000, 0x61bfff, MRA32_RAM }, /* Playfield data */
	{ 0x61c000, 0x61dfff, MRA32_RAM }, /* Text layer */
	{ 0x61e000, 0x61ffff, MRA32_RAM }, /* Vram */
	{ 0x620000, 0x62ffff, MRA32_RAM }, /* Line ram */
	{ 0x630000, 0x63ffff, MRA32_RAM }, /* Pivot ram */
	{ 0xc00000, 0xc007ff, MRA32_RAM }, /* Sound CPU shared ram */
MEMORY_END

static MEMORY_WRITE32_START( f3_writemem )
	{ 0x000000, 0x1fffff, MWA32_ROM },
	{ 0x300000, 0x30007f, f3_sound_bankswitch_w },
	{ 0x400000, 0x43ffff, MWA32_RAM, &f3_ram },
	{ 0x440000, 0x447fff, f3_palette_24bit_w, &paletteram32 },
	{ 0x4a0000, 0x4a001f, f3_control_w },
	{ 0x600000, 0x60ffff, MWA32_RAM, &spriteram32, &spriteram_size },
	{ 0x610000, 0x61bfff, f3_pf_data_w, &f3_pf_data },
	{ 0x61c000, 0x61dfff, f3_videoram_w, &videoram32 },
	{ 0x61e000, 0x61ffff, f3_vram_w, &f3_vram },
	{ 0x620000, 0x62ffff, f3_lineram_w, &f3_line_ram },
	{ 0x630000, 0x63ffff, f3_pivot_w, &f3_pivot_ram },
	{ 0x660000, 0x66000f, f3_control_0_w },
	{ 0x660010, 0x66001f, f3_control_1_w },
	{ 0xc00000, 0xc007ff, MWA32_RAM, &f3_shared_ram },
	{ 0xc80000, 0xc80003, f3_sound_reset_0_w },
	{ 0xc80100, 0xc80103, f3_sound_reset_1_w },
MEMORY_END

/******************************************************************************/

static MEMORY_READ16_START( sound_readmem )
	{ 0x000000, 0x03ffff, MRA16_RAM },
	{ 0x140000, 0x140fff, f3_68000_share_r },
	{ 0x200000, 0x20001f, ES5505_data_0_r },
	{ 0x260000, 0x2601ff, es5510_dsp_r },
	{ 0x280000, 0x28001f, f3_68681_r },
	{ 0xc00000, 0xc1ffff, MRA16_BANK1 },
	{ 0xc20000, 0xc3ffff, MRA16_BANK2 },
	{ 0xc40000, 0xc7ffff, MRA16_BANK3 },
	{ 0xff8000, 0xffffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( sound_writemem )
	{ 0x000000, 0x03ffff, MWA16_RAM },
	{ 0x140000, 0x140fff, f3_68000_share_w },
	{ 0x200000, 0x20001f, ES5505_data_0_w },
	{ 0x260000, 0x2601ff, es5510_dsp_w },
	{ 0x280000, 0x28001f, f3_68681_w },
	{ 0x300000, 0x30003f, f3_es5505_bank_w },
	{ 0x340000, 0x340003, f3_volume_w }, /* 8 channel volume control */
	{ 0xc00000, 0xc7ffff, MWA16_ROM },
	{ 0xff8000, 0xffffff, MWA16_RAM },
MEMORY_END

static MEMORY_READ16_START( ridingf_sound_readmem )
	{ 0x000000, 0x03ffff, MRA16_RAM },
	{ 0x140000, 0x140fff, f3_68000_share_r },
	{ 0x200000, 0x20001f, ES5505_data_0_r },
	{ 0x260000, 0x2601ff, ridingf_dsp_r },
	{ 0x280000, 0x28001f, f3_68681_r },
	{ 0xc00000, 0xc1ffff, MRA16_BANK1 },
	{ 0xc20000, 0xc3ffff, MRA16_BANK2 },
	{ 0xc40000, 0xc7ffff, MRA16_BANK3 },
	{ 0xff8000, 0xffffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( ridingf_sound_writemem )
	{ 0x000000, 0x03ffff, MWA16_RAM },
	{ 0x140000, 0x140fff, f3_68000_share_w },
	{ 0x200000, 0x20001f, ES5505_data_0_w },
	{ 0x260000, 0x2601ff, ridingf_dsp_w },
	{ 0x280000, 0x28001f, f3_68681_w },
	{ 0x300000, 0x30003f, f3_es5505_bank_w },
	{ 0x340000, 0x340003, f3_volume_w }, /* 8 channel volume control */
	{ 0xc00000, 0xc7ffff, MWA16_ROM },
	{ 0xff8000, 0xffffff, MWA16_RAM },
MEMORY_END

/******************************************************************************/

INPUT_PORTS_START( f3 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_SERVICE1 ) /* Service */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_SERVICE2 ) /* Only on some games */
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE3 ) /* Only on some games */
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* Eprom data bit */
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED ) /* Another service mode */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN4 )

	PORT_START
	PORT_ANALOGX( 0xfff, 0x000, IPT_DIAL | IPF_PLAYER1, 25, 25, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START
	PORT_ANALOGX( 0xfff, 0x000, IPT_DIAL | IPF_PLAYER2, 25, 25, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER4 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
INPUT_PORTS_END

INPUT_PORTS_START( kn )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_SERVICE1 ) /* Service */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_SERVICE2 ) /* Only on some games */
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* Eprom data bit */
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED ) /* Another service mode */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN4 )

	PORT_START
	PORT_ANALOGX( 0xfff, 0x000, IPT_DIAL | IPF_PLAYER1, 25, 25, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START
	PORT_ANALOGX( 0xfff, 0x000, IPT_DIAL | IPF_PLAYER2, 25, 25, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,
	256,
	4,
	{ 0,1,2,3 },
#ifdef MSB_FIRST
    { 7*4, 6*4, 5*4, 4*4, 3*4, 2*4, 1*4, 0*4 },
#else
    { 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4 },
#endif
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout pivotlayout =
{
	8,8,
	2048,
	4,
	{ 0,1,2,3 },
#ifdef MSB_FIRST
    { 7*4, 6*4, 5*4, 4*4, 3*4, 2*4, 1*4, 0*4 },
#else
    { 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4 },
#endif
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout spriteram_layout =
{
	16,16,
	RGN_FRAC(1,2),
	6,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+1, 0, 1, 2, 3 },
	{
	4, 0, 12, 8,
	16+4, 16+0, 16+12, 16+8,
	32+4, 32+0, 32+12, 32+8,
	48+4, 48+0, 48+12, 48+8 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout tile_layout =
{
	16,16,
	RGN_FRAC(1,2),
	6,
	{ RGN_FRAC(1,2)+2, RGN_FRAC(1,2)+3, 0, 1, 2, 3 },
	{
	4, 0, 16+4, 16+0,
    8+4, 8+0, 24+4, 24+0,
	32+4, 32+0, 48+4, 48+0,
    40+4, 40+0, 56+4, 56+0,
   	},
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 0,           0x000000, &charlayout,          0,  64 }, /* Dynamically modified */
	{ REGION_GFX2, 0x000000, &tile_layout, 	       0, 512 }, /* Tiles area */
	{ REGION_GFX1, 0x000000, &spriteram_layout, 4096, 256 }, /* Sprites area */
	{ 0,           0x000000, &pivotlayout,         0,  64 }, /* Dynamically modified */
	{ -1 } /* end of array */
};

/******************************************************************************/

static INTERRUPT_GEN( f3_interrupt )
{
	if (cpu_getiloops()) cpu_set_irq_line(0, 3, HOLD_LINE);
	else cpu_set_irq_line(0, 2, HOLD_LINE);
}

static MACHINE_INIT( f3 )
{
	/* Sound cpu program loads to 0xc00000 so we use a bank */
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU2);
	cpu_setbank(1,&RAM[0x80000]);
	cpu_setbank(2,&RAM[0x90000]);
	cpu_setbank(3,&RAM[0xa0000]);

	RAM[0]=RAM[0x80000]; /* Stack and Reset vectors */
	RAM[1]=RAM[0x80001];
	RAM[2]=RAM[0x80002];
	RAM[3]=RAM[0x80003];

	cpu_set_reset_line(1, ASSERT_LINE);
	f3_68681_reset();
}

static struct ES5505interface es5505_interface =
{
	1,					/* total number of chips */
	{ 30476100/2 },		/* freq */
	{ REGION_SOUND1 },	/* Bank 0: Unused by F3 games? */
	{ REGION_SOUND1 },	/* Bank 1: All games seem to use this */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },		/* master volume */
};

static MACHINE_DRIVER_START( f3 )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68EC020, 16000000)
	MDRV_CPU_MEMORY(f3_readmem,f3_writemem)
	MDRV_CPU_VBLANK_INT(f3_interrupt,2)

	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(624) /* 58.97 Hz, 624us vblank time */

	MDRV_MACHINE_INIT(f3)
	MDRV_NVRAM_HANDLER(93C46)

 	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(40*8+48*2, 32*8)
	MDRV_VISIBLE_AREA(46, 40*8-1+46, 24, 24+232-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(f3)
	MDRV_VIDEO_EOF(f3)
	MDRV_VIDEO_UPDATE(f3)
	MDRV_VIDEO_STOP(f3)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(ES5505, es5505_interface)
MACHINE_DRIVER_END

/* These games reprogram the video output registers to display different scanlines,
 we can't change our screen display at runtime, so we do it here instead.  None
 of the games change the registers during the game (to do so would probably require
 monitor recalibration.)
*/
static MACHINE_DRIVER_START( f3_224a )
	MDRV_IMPORT_FROM(f3)
	MDRV_VISIBLE_AREA(46, 40*8-1+46, 31, 31+224-1)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( f3_224b )
	MDRV_IMPORT_FROM(f3)
	MDRV_VISIBLE_AREA(46, 40*8-1+46, 32, 32+224-1)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( f3_224c )
	MDRV_IMPORT_FROM(f3)
	MDRV_VISIBLE_AREA(46, 40*8-1+46, 24, 24+224-1)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ringrage )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68EC020, 16000000)
	MDRV_CPU_MEMORY(f3_readmem,f3_writemem)
	MDRV_CPU_VBLANK_INT(f3_interrupt,2)

	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(ridingf_sound_readmem,ridingf_sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(624) /* 58.97 Hz, 624us vblank time */

	MDRV_MACHINE_INIT(f3)
	MDRV_NVRAM_HANDLER(93C46)

 	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(40*8+48*2, 32*8)
	MDRV_VISIBLE_AREA(46, 40*8-1+46, 31, 31+224-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(f3)
	MDRV_VIDEO_EOF(f3)
	MDRV_VIDEO_UPDATE(f3)
	MDRV_VIDEO_STOP(f3)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(ES5505, es5505_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ridingf )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68EC020, 16000000)
	MDRV_CPU_MEMORY(f3_readmem,f3_writemem)
	MDRV_CPU_VBLANK_INT(f3_interrupt,2)

	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(ridingf_sound_readmem,ridingf_sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(624) /* 58.97 Hz, 624us vblank time */

	MDRV_MACHINE_INIT(f3)
	MDRV_NVRAM_HANDLER(93C46)

 	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(40*8+48*2, 32*8)
	MDRV_VISIBLE_AREA(46, 40*8-1+46, 32, 32+224-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(f3)
	MDRV_VIDEO_EOF(f3)
	MDRV_VIDEO_UPDATE(f3)
	MDRV_VIDEO_STOP(f3)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(ES5505, es5505_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( recalh )
	MDRV_IMPORT_FROM(f3)
    MDRV_NVRAM_HANDLER(recalh)
MACHINE_DRIVER_END

/******************************************************************************/

ROM_START( ringrage )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d21-23.40", 0x000000, 0x40000, CRC(14e9ed65) SHA1(956db484375a43bcdf5a6a104e3c7d7ef5baaa1b) )
	ROM_LOAD32_BYTE("d21-22.38", 0x000001, 0x40000, CRC(6f8b65b0) SHA1(a4f786b72068c6c9c1b23df67029eb0e2a982789) )
	ROM_LOAD32_BYTE("d21-21.36", 0x000002, 0x40000, CRC(bf7234bc) SHA1(781b8f850275537e1ae2dae18a4554a1283cb432) )
	ROM_LOAD32_BYTE("ringr-25.rom",0x000003, 0x40000, CRC(aeff6e19) SHA1(7fd8f64f0a52dfe369a3709af8c043286b5b6fdf) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("d21-02.66", 0x000000, 0x200000, CRC(facd3a02) SHA1(360c6e65d01dd2c33495ba928ef9986f754b3694) )
	ROM_LOAD16_BYTE("d21-03.67", 0x000001, 0x200000, CRC(6f653e68) SHA1(840905f012c37d58160cc554c036ad25218ce3e6) )
	ROM_LOAD       ("d21-04.68", 0x600000, 0x200000, CRC(9dcdceca) SHA1(e12bab5307ebe4c3b5f9284c91f3bf7ba4c8e9bc) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("d21-06.49", 0x000000, 0x080000, CRC(92d4a720) SHA1(81dc58c58d5f4f20ceeb5d6b90491f1efcbc67d3) )
  	ROM_LOAD16_BYTE("d21-07.50", 0x000001, 0x080000, CRC(6da696e9) SHA1(74332090b0de4193a669cd5242fd655e7b90f772) )
	ROM_LOAD       ("d21-08.51", 0x180000, 0x080000, CRC(a0d95be9) SHA1(1746097e827ac10906f012c5c4f93c388a30f4b3) )
	ROM_FILL       (             0x100000, 0x080000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d21-18.5", 0x100000, 0x20000, CRC(133b55d0) SHA1(feb5c9d0b1adcae3b16eb206c8ac4e73fd88bef4) )
	ROM_LOAD16_BYTE("d21-19.6", 0x100001, 0x20000, CRC(1f98908f) SHA1(972c8f7e4e417831466714efd0b4cadca1f3e8e5) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d21-01.17", 0x000000, 0x200000, CRC(1fb6f07d) SHA1(a7d21d4b0b0b141c4dbe66554e5362e2c8876067) )
	ROM_LOAD16_BYTE("d21-05.18", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( ringragu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d21-23.40", 0x000000, 0x40000, CRC(14e9ed65) SHA1(956db484375a43bcdf5a6a104e3c7d7ef5baaa1b) )
	ROM_LOAD32_BYTE("d21-22.38", 0x000001, 0x40000, CRC(6f8b65b0) SHA1(a4f786b72068c6c9c1b23df67029eb0e2a982789) )
	ROM_LOAD32_BYTE("d21-21.36", 0x000002, 0x40000, CRC(bf7234bc) SHA1(781b8f850275537e1ae2dae18a4554a1283cb432) )
	ROM_LOAD32_BYTE("d21-24.bin",0x000003, 0x40000, CRC(404dee67) SHA1(1e46d52d72b6cbe5e8af9f0f8e8b1acf9c6feb26) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("d21-02.66", 0x000000, 0x200000, CRC(facd3a02) SHA1(360c6e65d01dd2c33495ba928ef9986f754b3694) )
	ROM_LOAD16_BYTE("d21-03.67", 0x000001, 0x200000, CRC(6f653e68) SHA1(840905f012c37d58160cc554c036ad25218ce3e6) )
	ROM_LOAD       ("d21-04.68", 0x600000, 0x200000, CRC(9dcdceca) SHA1(e12bab5307ebe4c3b5f9284c91f3bf7ba4c8e9bc) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("d21-06.49", 0x000000, 0x080000, CRC(92d4a720) SHA1(81dc58c58d5f4f20ceeb5d6b90491f1efcbc67d3) )
  	ROM_LOAD16_BYTE("d21-07.50", 0x000001, 0x080000, CRC(6da696e9) SHA1(74332090b0de4193a669cd5242fd655e7b90f772) )
	ROM_LOAD       ("d21-08.51", 0x180000, 0x080000, CRC(a0d95be9) SHA1(1746097e827ac10906f012c5c4f93c388a30f4b3) )
	ROM_FILL       (             0x100000, 0x080000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d21-18.5", 0x100000, 0x20000, CRC(133b55d0) SHA1(feb5c9d0b1adcae3b16eb206c8ac4e73fd88bef4) )
	ROM_LOAD16_BYTE("d21-19.6", 0x100001, 0x20000, CRC(1f98908f) SHA1(972c8f7e4e417831466714efd0b4cadca1f3e8e5) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d21-01.17", 0x000000, 0x200000, CRC(1fb6f07d) SHA1(a7d21d4b0b0b141c4dbe66554e5362e2c8876067) )
	ROM_LOAD16_BYTE("d21-05.18", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( ringragj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d21-23.40", 0x000000, 0x40000, CRC(14e9ed65) SHA1(956db484375a43bcdf5a6a104e3c7d7ef5baaa1b) )
	ROM_LOAD32_BYTE("d21-22.38", 0x000001, 0x40000, CRC(6f8b65b0) SHA1(a4f786b72068c6c9c1b23df67029eb0e2a982789) )
	ROM_LOAD32_BYTE("d21-21.36", 0x000002, 0x40000, CRC(bf7234bc) SHA1(781b8f850275537e1ae2dae18a4554a1283cb432) )
	ROM_LOAD32_BYTE("d21-20.34", 0x000003, 0x40000, CRC(a8eb68a4) SHA1(040238fad0d17cac21b144b0fcab1774d2924da9) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("d21-02.66", 0x000000, 0x200000, CRC(facd3a02) SHA1(360c6e65d01dd2c33495ba928ef9986f754b3694) )
	ROM_LOAD16_BYTE("d21-03.67", 0x000001, 0x200000, CRC(6f653e68) SHA1(840905f012c37d58160cc554c036ad25218ce3e6) )
	ROM_LOAD       ("d21-04.68", 0x600000, 0x200000, CRC(9dcdceca) SHA1(e12bab5307ebe4c3b5f9284c91f3bf7ba4c8e9bc) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("d21-06.49", 0x000000, 0x080000, CRC(92d4a720) SHA1(81dc58c58d5f4f20ceeb5d6b90491f1efcbc67d3) )
  	ROM_LOAD16_BYTE("d21-07.50", 0x000001, 0x080000, CRC(6da696e9) SHA1(74332090b0de4193a669cd5242fd655e7b90f772) )
	ROM_LOAD       ("d21-08.51", 0x180000, 0x080000, CRC(a0d95be9) SHA1(1746097e827ac10906f012c5c4f93c388a30f4b3) )
	ROM_FILL       (             0x100000, 0x080000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d21-18.5", 0x100000, 0x20000, CRC(133b55d0) SHA1(feb5c9d0b1adcae3b16eb206c8ac4e73fd88bef4) )
	ROM_LOAD16_BYTE("d21-19.6", 0x100001, 0x20000, CRC(1f98908f) SHA1(972c8f7e4e417831466714efd0b4cadca1f3e8e5) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d21-01.17", 0x000000, 0x200000, CRC(1fb6f07d) SHA1(a7d21d4b0b0b141c4dbe66554e5362e2c8876067) )
	ROM_LOAD16_BYTE("d21-05.18", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( arabianm )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d29-23.rom", 0x000000, 0x40000, CRC(89a0c706) SHA1(d0df02be2b63566ec776bb13947f975062766a01) )
	ROM_LOAD32_BYTE("d29-22.rom", 0x000001, 0x40000, CRC(4afc22a4) SHA1(9579d134a1e4b0c86af9b41d136acfe6cc7f6624) )
	ROM_LOAD32_BYTE("d29-21.rom", 0x000002, 0x40000, CRC(ac32eb38) SHA1(19d8d965497e41a7a2f490a197322da7fd1fa40a) )
	ROM_LOAD32_BYTE("d29-25.rom", 0x000003, 0x40000, CRC(b9b652ed) SHA1(19cceef87884adeb03e5e330159541a1e503a7f2) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d29-03.rom", 0x000000, 0x100000, CRC(aeaff456) SHA1(e70d0089e69d33d213be8195c31891f38fbcb53a) )
	ROM_LOAD16_BYTE("d29-04.rom", 0x000001, 0x100000, CRC(01711cfe) SHA1(26da4cc9dcb8d38bdf8c93015f77e58ffc9d1ba9) )
	ROM_LOAD       ("d29-05.rom", 0x300000, 0x100000, CRC(9b5f7a17) SHA1(89d9faedc7b55df6237f2c2ebb43de7de685cb66) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d29-06.rom", 0x000000, 0x080000, CRC(eea07bf3) SHA1(7860a2c0c592af000b56e59bd39d67b086fe3606) )
  	ROM_LOAD16_BYTE("d29-07.rom", 0x000001, 0x080000, CRC(db3c094d) SHA1(5b60f0fa1054bf93ce51d310376b1abdb3022541) )
	ROM_LOAD       ("d29-08.rom", 0x180000, 0x080000, CRC(d7562851) SHA1(91d86e75ba7a590ca298b932b4cf8fa9228f115e) )
	ROM_FILL       (              0x100000, 0x080000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d29-18.rom", 0x100000, 0x20000, CRC(d97780df) SHA1(d0f9d2fd7ce13f620bb44083bf012f67dda4b10b) )
	ROM_LOAD16_BYTE("d29-19.rom", 0x100001, 0x20000, CRC(b1ad365c) SHA1(1cd26d8feaaa06b50dfee32e9b7950b8ee92ac55) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d29-01.rom", 0x000000, 0x200000, CRC(545ac4b3) SHA1(f89513fca8a03cab11160aa1f0a9c3adbc8bda08) )
	ROM_LOAD16_BYTE("d29-02.rom", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( arabiamj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d29-23.rom", 0x000000, 0x40000, CRC(89a0c706) SHA1(d0df02be2b63566ec776bb13947f975062766a01) )
	ROM_LOAD32_BYTE("d29-22.rom", 0x000001, 0x40000, CRC(4afc22a4) SHA1(9579d134a1e4b0c86af9b41d136acfe6cc7f6624) )
	ROM_LOAD32_BYTE("d29-21.rom", 0x000002, 0x40000, CRC(ac32eb38) SHA1(19d8d965497e41a7a2f490a197322da7fd1fa40a) )
	ROM_LOAD32_BYTE("d29-20.34",  0x000003, 0x40000, CRC(57b833c1) SHA1(69beff431e400db17ca1983a7a4d6684a1ea701c) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d29-03.rom", 0x000000, 0x100000, CRC(aeaff456) SHA1(e70d0089e69d33d213be8195c31891f38fbcb53a) )
	ROM_LOAD16_BYTE("d29-04.rom", 0x000001, 0x100000, CRC(01711cfe) SHA1(26da4cc9dcb8d38bdf8c93015f77e58ffc9d1ba9) )
	ROM_LOAD       ("d29-05.rom", 0x300000, 0x100000, CRC(9b5f7a17) SHA1(89d9faedc7b55df6237f2c2ebb43de7de685cb66) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d29-06.rom", 0x000000, 0x080000, CRC(eea07bf3) SHA1(7860a2c0c592af000b56e59bd39d67b086fe3606) )
  	ROM_LOAD16_BYTE("d29-07.rom", 0x000001, 0x080000, CRC(db3c094d) SHA1(5b60f0fa1054bf93ce51d310376b1abdb3022541) )
	ROM_LOAD       ("d29-08.rom", 0x180000, 0x080000, CRC(d7562851) SHA1(91d86e75ba7a590ca298b932b4cf8fa9228f115e) )
	ROM_FILL       (              0x100000, 0x080000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d29-18.rom", 0x100000, 0x20000, CRC(d97780df) SHA1(d0f9d2fd7ce13f620bb44083bf012f67dda4b10b) )
	ROM_LOAD16_BYTE("d29-19.rom", 0x100001, 0x20000, CRC(b1ad365c) SHA1(1cd26d8feaaa06b50dfee32e9b7950b8ee92ac55) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d29-01.rom", 0x000000, 0x200000, CRC(545ac4b3) SHA1(f89513fca8a03cab11160aa1f0a9c3adbc8bda08) )
	ROM_LOAD16_BYTE("d29-02.rom", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( arabiamu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d29-23.rom", 0x000000, 0x40000, CRC(89a0c706) SHA1(d0df02be2b63566ec776bb13947f975062766a01) )
	ROM_LOAD32_BYTE("d29-22.rom", 0x000001, 0x40000, CRC(4afc22a4) SHA1(9579d134a1e4b0c86af9b41d136acfe6cc7f6624) )
	ROM_LOAD32_BYTE("d29-21.rom", 0x000002, 0x40000, CRC(ac32eb38) SHA1(19d8d965497e41a7a2f490a197322da7fd1fa40a) )
	ROM_LOAD32_BYTE("d29-24.bin", 0x000003, 0x40000, CRC(ceb1627b) SHA1(c705ed956fa80ad77c53e8e2b9d27020255578bd) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d29-03.rom", 0x000000, 0x100000, CRC(aeaff456) SHA1(e70d0089e69d33d213be8195c31891f38fbcb53a) )
	ROM_LOAD16_BYTE("d29-04.rom", 0x000001, 0x100000, CRC(01711cfe) SHA1(26da4cc9dcb8d38bdf8c93015f77e58ffc9d1ba9) )
	ROM_LOAD       ("d29-05.rom", 0x300000, 0x100000, CRC(9b5f7a17) SHA1(89d9faedc7b55df6237f2c2ebb43de7de685cb66) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d29-06.rom", 0x000000, 0x080000, CRC(eea07bf3) SHA1(7860a2c0c592af000b56e59bd39d67b086fe3606) )
  	ROM_LOAD16_BYTE("d29-07.rom", 0x000001, 0x080000, CRC(db3c094d) SHA1(5b60f0fa1054bf93ce51d310376b1abdb3022541) )
	ROM_LOAD       ("d29-08.rom", 0x180000, 0x080000, CRC(d7562851) SHA1(91d86e75ba7a590ca298b932b4cf8fa9228f115e) )
	ROM_FILL       (              0x100000, 0x080000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d29-18.rom", 0x100000, 0x20000, CRC(d97780df) SHA1(d0f9d2fd7ce13f620bb44083bf012f67dda4b10b) )
	ROM_LOAD16_BYTE("d29-19.rom", 0x100001, 0x20000, CRC(b1ad365c) SHA1(1cd26d8feaaa06b50dfee32e9b7950b8ee92ac55) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d29-01.rom", 0x000000, 0x200000, CRC(545ac4b3) SHA1(f89513fca8a03cab11160aa1f0a9c3adbc8bda08) )
	ROM_LOAD16_BYTE("d29-02.rom", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( ridingf )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d34-12.40", 0x000000, 0x40000, CRC(e67e69d4) SHA1(9960629a7576f6a1d8a0e17c1bfc202ceee9e824) )
	ROM_LOAD32_BYTE("d34-11.38", 0x000001, 0x40000, CRC(7d240a88) SHA1(3410d3f66e868f2696dbd95adbbd393596d34351) )
	ROM_LOAD32_BYTE("d34-10.36", 0x000002, 0x40000, CRC(8aa3f4ac) SHA1(ba3c1274dcaccf4ba97ff224cb453eb1ead510ed) )
	ROM_LOAD32_BYTE("d34_14.34", 0x000003, 0x40000, CRC(e000198e) SHA1(3c9fdd40ade7b02021d23b7ce63a28d80bb6e8e0) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d34-01.66", 0x000000, 0x200000, CRC(7974e6aa) SHA1(f3f697d0e2f52011046aa2db2df25a4b55a631d5) )
	ROM_LOAD16_BYTE("d34-02.67", 0x000001, 0x200000, CRC(f4422370) SHA1(27ba051e0dc27b39652ff1d940a2dd29965c6528) )
	ROM_FILL       (             0x400000, 0x400000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d34-05.49", 0x000000, 0x080000, CRC(72e3ee4b) SHA1(2b69487338c18ced7a2ac4f280e8e22aa7209eb3) )
  	ROM_LOAD16_BYTE("d34-06.50", 0x000001, 0x080000, CRC(edc9b9f3) SHA1(c57bec1c8882d95388c3fae7cb5a321bdb202737) )
	ROM_FILL       (             0x100000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d34-07.5", 0x100000, 0x20000, CRC(67239e2b) SHA1(8e0268fab53d26cde5c1928326c4787533dc6ffe) )
	ROM_LOAD16_BYTE("d34-08.6", 0x100001, 0x20000, CRC(2cf20323) SHA1(b2bbac3714ecfd75506ae000c7eec603dfe3e13d) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d34-03.17", 0x000000, 0x200000, CRC(e534ef74) SHA1(532d00e927d3704e7557abd59e35de8b7661c8fa) )
	ROM_LOAD16_BYTE("d34-04.18", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( ridefgtj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d34-12.40", 0x000000, 0x40000, CRC(e67e69d4) SHA1(9960629a7576f6a1d8a0e17c1bfc202ceee9e824) )
	ROM_LOAD32_BYTE("d34-11.38", 0x000001, 0x40000, CRC(7d240a88) SHA1(3410d3f66e868f2696dbd95adbbd393596d34351) )
	ROM_LOAD32_BYTE("d34-10.36", 0x000002, 0x40000, CRC(8aa3f4ac) SHA1(ba3c1274dcaccf4ba97ff224cb453eb1ead510ed) )
	ROM_LOAD32_BYTE("d34-09.34", 0x000003, 0x40000, CRC(0e0e78a2) SHA1(4c8d0a5d6b8c77be34fd7b9c4a2df4022e74443a) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d34-01.66", 0x000000, 0x200000, CRC(7974e6aa) SHA1(f3f697d0e2f52011046aa2db2df25a4b55a631d5) )
	ROM_LOAD16_BYTE("d34-02.67", 0x000001, 0x200000, CRC(f4422370) SHA1(27ba051e0dc27b39652ff1d940a2dd29965c6528) )
	ROM_FILL       (             0x400000, 0x400000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d34-05.49", 0x000000, 0x080000, CRC(72e3ee4b) SHA1(2b69487338c18ced7a2ac4f280e8e22aa7209eb3) )
  	ROM_LOAD16_BYTE("d34-06.50", 0x000001, 0x080000, CRC(edc9b9f3) SHA1(c57bec1c8882d95388c3fae7cb5a321bdb202737) )
	ROM_FILL       (             0x100000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d34-07.5", 0x100000, 0x20000, CRC(67239e2b) SHA1(8e0268fab53d26cde5c1928326c4787533dc6ffe) )
	ROM_LOAD16_BYTE("d34-08.6", 0x100001, 0x20000, CRC(2cf20323) SHA1(b2bbac3714ecfd75506ae000c7eec603dfe3e13d) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d34-03.17", 0x000000, 0x200000, CRC(e534ef74) SHA1(532d00e927d3704e7557abd59e35de8b7661c8fa) )
	ROM_LOAD16_BYTE("d34-04.18", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( ridefgtu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d34-12.40", 0x000000, 0x40000, CRC(e67e69d4) SHA1(9960629a7576f6a1d8a0e17c1bfc202ceee9e824) )
	ROM_LOAD32_BYTE("d34-11.38", 0x000001, 0x40000, CRC(7d240a88) SHA1(3410d3f66e868f2696dbd95adbbd393596d34351) )
	ROM_LOAD32_BYTE("d34-10.36", 0x000002, 0x40000, CRC(8aa3f4ac) SHA1(ba3c1274dcaccf4ba97ff224cb453eb1ead510ed) )
	ROM_LOAD32_BYTE("d34_13.34", 0x000003, 0x40000, CRC(97072918) SHA1(7ae96fb7a07b7192c39ec496e1193c1cbfbbc770) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d34-01.66", 0x000000, 0x200000, CRC(7974e6aa) SHA1(f3f697d0e2f52011046aa2db2df25a4b55a631d5) )
	ROM_LOAD16_BYTE("d34-02.67", 0x000001, 0x200000, CRC(f4422370) SHA1(27ba051e0dc27b39652ff1d940a2dd29965c6528) )
	ROM_FILL       (             0x400000, 0x400000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d34-05.49", 0x000000, 0x080000, CRC(72e3ee4b) SHA1(2b69487338c18ced7a2ac4f280e8e22aa7209eb3) )
  	ROM_LOAD16_BYTE("d34-06.50", 0x000001, 0x080000, CRC(edc9b9f3) SHA1(c57bec1c8882d95388c3fae7cb5a321bdb202737) )
	ROM_FILL       (             0x100000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d34-07.5", 0x100000, 0x20000, CRC(67239e2b) SHA1(8e0268fab53d26cde5c1928326c4787533dc6ffe) )
	ROM_LOAD16_BYTE("d34-08.6", 0x100001, 0x20000, CRC(2cf20323) SHA1(b2bbac3714ecfd75506ae000c7eec603dfe3e13d) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d34-03.17", 0x000000, 0x200000, CRC(e534ef74) SHA1(532d00e927d3704e7557abd59e35de8b7661c8fa) )
	ROM_LOAD16_BYTE("d34-04.18", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( gseeker )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d40_12.rom", 0x000000, 0x40000, CRC(884055fb) SHA1(a91c3dcd635a3e22fbec94c9fc46d3c29fde5e71) )
	ROM_LOAD32_BYTE("d40_11.rom", 0x000001, 0x40000, CRC(85e701d2) SHA1(da0bc34a2c64d2db2fe143ad5a77bf667de5b015) )
	ROM_LOAD32_BYTE("d40_10.rom", 0x000002, 0x40000, CRC(1e659ac5) SHA1(cd435d361c4353b361ef975f208d81369d5d079f) )
	ROM_LOAD32_BYTE("d40_14.rom", 0x000003, 0x40000, CRC(d9a76bd9) SHA1(8edaf114c1342b33fd518320a181743d1dd324c1) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d40_03.rom", 0x000000, 0x100000, CRC(bcd70efc) SHA1(c65f934022d84e29cfd396cf70b6c1afdf90500b) )
	ROM_LOAD16_BYTE("d40_04.rom", 0x100001, 0x080000, CRC(cd2ac666) SHA1(abf09979f1fe8575323b95b95688628ce4fc2350) )
	ROM_CONTINUE(0,0x80000)
	ROM_LOAD16_BYTE("d40_15.rom", 0x000000, 0x080000, CRC(50555125) SHA1(587cdfb2e027c1d96ecc46d2612956deca5fd36f) )
	ROM_LOAD16_BYTE("d40_16.rom", 0x000001, 0x080000, CRC(3f9bbe1e) SHA1(6d9b2d2d893257ad096c276eff4077f60a81921f) )
	/* Taito manufactured mask roms 3 + 4 wrong, and later added 15 + 16 as a patch */
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d40_05.rom", 0x000000, 0x100000, CRC(be6eec8f) SHA1(725e5e09f6ee60bd4c38fa223c4dea202c56f75f) )
	ROM_LOAD16_BYTE("d40_06.rom", 0x000001, 0x100000, CRC(a822abe4) SHA1(1a0dd9dcb8e24daab6eb1661307ef0e10f7e4d4e) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d40_07.rom", 0x100000, 0x20000, CRC(7e9b26c2) SHA1(d88ad39a9d70b4a5bd3f83e0d4d0725f659f1d2a) )
	ROM_LOAD16_BYTE("d40_08.rom", 0x100001, 0x20000, CRC(9c926a28) SHA1(9d9ee75eb895edc381c3ab4df5af941f84cd2073) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d40_01.rom", 0x000000, 0x200000, CRC(ee312e95) SHA1(885553950c2b2195d664639bf7e0d1ffa3e8346a) )
	ROM_LOAD16_BYTE("d40_02.rom", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( gseekerj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d40_12.rom", 0x000000, 0x40000, CRC(884055fb) SHA1(a91c3dcd635a3e22fbec94c9fc46d3c29fde5e71) )
	ROM_LOAD32_BYTE("d40_11.rom", 0x000001, 0x40000, CRC(85e701d2) SHA1(da0bc34a2c64d2db2fe143ad5a77bf667de5b015) )
	ROM_LOAD32_BYTE("d40_10.rom", 0x000002, 0x40000, CRC(1e659ac5) SHA1(cd435d361c4353b361ef975f208d81369d5d079f) )
	ROM_LOAD32_BYTE("d40-09.34",  0x000003, 0x40000, CRC(37a90af5) SHA1(1f3401148375c9ca638ca6db6098ea4acf7d63a6) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d40_03.rom", 0x000000, 0x100000, CRC(bcd70efc) SHA1(c65f934022d84e29cfd396cf70b6c1afdf90500b) )
	ROM_LOAD16_BYTE("d40_04.rom", 0x100001, 0x080000, CRC(cd2ac666) SHA1(abf09979f1fe8575323b95b95688628ce4fc2350) )
	ROM_CONTINUE(0,0x80000)
	ROM_LOAD16_BYTE("d40_15.rom", 0x000000, 0x080000, CRC(50555125) SHA1(587cdfb2e027c1d96ecc46d2612956deca5fd36f) )
	ROM_LOAD16_BYTE("d40_16.rom", 0x000001, 0x080000, CRC(3f9bbe1e) SHA1(6d9b2d2d893257ad096c276eff4077f60a81921f) )
	/* Taito manufactured mask roms 3 + 4 wrong, and later added 15 + 16 as a patch */
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d40_05.rom", 0x000000, 0x100000, CRC(be6eec8f) SHA1(725e5e09f6ee60bd4c38fa223c4dea202c56f75f) )
	ROM_LOAD16_BYTE("d40_06.rom", 0x000001, 0x100000, CRC(a822abe4) SHA1(1a0dd9dcb8e24daab6eb1661307ef0e10f7e4d4e) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d40_07.rom", 0x100000, 0x20000, CRC(7e9b26c2) SHA1(d88ad39a9d70b4a5bd3f83e0d4d0725f659f1d2a) )
	ROM_LOAD16_BYTE("d40_08.rom", 0x100001, 0x20000, CRC(9c926a28) SHA1(9d9ee75eb895edc381c3ab4df5af941f84cd2073) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d40_01.rom", 0x000000, 0x200000, CRC(ee312e95) SHA1(885553950c2b2195d664639bf7e0d1ffa3e8346a) )
	ROM_LOAD16_BYTE("d40_02.rom", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( gseekeru )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d40_12.rom", 0x000000, 0x40000, CRC(884055fb) SHA1(a91c3dcd635a3e22fbec94c9fc46d3c29fde5e71) )
	ROM_LOAD32_BYTE("d40_11.rom", 0x000001, 0x40000, CRC(85e701d2) SHA1(da0bc34a2c64d2db2fe143ad5a77bf667de5b015) )
	ROM_LOAD32_BYTE("d40_10.rom", 0x000002, 0x40000, CRC(1e659ac5) SHA1(cd435d361c4353b361ef975f208d81369d5d079f) )
	ROM_LOAD32_BYTE("d40-13.bin", 0x000003, 0x40000, CRC(aea05b4f) SHA1(4be054ebec49d694a7a3ee9bc66c22c46126ea4f) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d40_03.rom", 0x000000, 0x100000, CRC(bcd70efc) SHA1(c65f934022d84e29cfd396cf70b6c1afdf90500b) )
	ROM_LOAD16_BYTE("d40_04.rom", 0x100001, 0x080000, CRC(cd2ac666) SHA1(abf09979f1fe8575323b95b95688628ce4fc2350) )
	ROM_CONTINUE(0,0x80000)
	ROM_LOAD16_BYTE("d40_15.rom", 0x000000, 0x080000, CRC(50555125) SHA1(587cdfb2e027c1d96ecc46d2612956deca5fd36f) )
	ROM_LOAD16_BYTE("d40_16.rom", 0x000001, 0x080000, CRC(3f9bbe1e) SHA1(6d9b2d2d893257ad096c276eff4077f60a81921f) )
	/* Taito manufactured mask roms 3 + 4 wrong, and later added 15 + 16 as a patch */
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d40_05.rom", 0x000000, 0x100000, CRC(be6eec8f) SHA1(725e5e09f6ee60bd4c38fa223c4dea202c56f75f) )
	ROM_LOAD16_BYTE("d40_06.rom", 0x000001, 0x100000, CRC(a822abe4) SHA1(1a0dd9dcb8e24daab6eb1661307ef0e10f7e4d4e) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d40_07.rom", 0x100000, 0x20000, CRC(7e9b26c2) SHA1(d88ad39a9d70b4a5bd3f83e0d4d0725f659f1d2a) )
	ROM_LOAD16_BYTE("d40_08.rom", 0x100001, 0x20000, CRC(9c926a28) SHA1(9d9ee75eb895edc381c3ab4df5af941f84cd2073) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d40_01.rom", 0x000000, 0x200000, CRC(ee312e95) SHA1(885553950c2b2195d664639bf7e0d1ffa3e8346a) )
	ROM_LOAD16_BYTE("d40_02.rom", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( cupfinal )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d49-13.bin", 0x000000, 0x20000, CRC(ccee5e73) SHA1(5273e3b9bc6fc4fa0c63d9c62aa6b638e9780c24) )
	ROM_LOAD32_BYTE("d49-14.bin", 0x000001, 0x20000, CRC(2323bf2e) SHA1(e43f9eac6887e39d5c0f39264aa914a5d5f84cca) )
	ROM_LOAD32_BYTE("d49-16.bin", 0x000002, 0x20000, CRC(8e73f739) SHA1(620a4d52abc00908cd1393babdc600b929019a51) )
	ROM_LOAD32_BYTE("d49-20.bin", 0x000003, 0x20000, CRC(1e9c392c) SHA1(4ed9390b84c23809215a42c930ab0451531cfef1) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d49-01", 0x000000, 0x200000, CRC(1dc89f1c) SHA1(9597b1d8c9b447080ca9401aee83bb4a64bb8332) )
  	ROM_LOAD16_BYTE("d49-02", 0x000001, 0x200000, CRC(1e4c374f) SHA1(512edc6a934578d0e7371410a041150d3b13aaad) )
	ROM_LOAD16_BYTE("d49-06", 0x400000, 0x100000, CRC(71ef4ee1) SHA1(1d7729dbc77f7201ff574e8aef65a55bd81c25a7) )
  	ROM_LOAD16_BYTE("d49-07", 0x400001, 0x100000, CRC(e5655b8f) SHA1(2c21745370bfe9dbf0e95f7ce42ed34a162bff64) )
	ROM_LOAD       ("d49-03", 0x900000, 0x200000, CRC(cf9a8727) SHA1(f21787fdcdd8be2009c2d481a9b2d7fc03ce782e) )
	ROM_LOAD       ("d49-08", 0xb00000, 0x100000, CRC(7d3c6536) SHA1(289b4bf79ebd9cbdf64ab956784d226e6d546654) )
	ROM_FILL       (          0x600000, 0x300000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d49-09", 0x000000, 0x080000, CRC(257ede01) SHA1(c36397d95706c5e68a7738c84829a51c5e8f5ef7) )
	ROM_LOAD16_BYTE("d49-10", 0x000001, 0x080000, CRC(f587b787) SHA1(22db4904c134756ddd0f753f197419d27e60a827) )
	ROM_LOAD       ("d49-11", 0x180000, 0x080000, CRC(11318b26) SHA1(a7153f9f406d52189f59cbe58d65f88f4e2e6fcc) )
	ROM_FILL       (          0x100000, 0x080000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d49-17", 0x100000, 0x40000, CRC(49942466) SHA1(5e355a079b81f129919c7599c0cc09a9e193ea41) )
	ROM_LOAD16_BYTE("d49-18", 0x100001, 0x40000, CRC(9d75b7d4) SHA1(1dc823327294f5c81b78f151fcd3d0550c208697) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d49-04", 0x000000, 0x200000, CRC(44b365a9) SHA1(14c4a6b193a0069360406c74c500ba24f2a55b62) )
	ROM_LOAD16_BYTE("d49-05", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( hthero93 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d49-13.bin", 0x000000, 0x20000, CRC(ccee5e73) SHA1(5273e3b9bc6fc4fa0c63d9c62aa6b638e9780c24) )
	ROM_LOAD32_BYTE("d49-14.bin", 0x000001, 0x20000, CRC(2323bf2e) SHA1(e43f9eac6887e39d5c0f39264aa914a5d5f84cca) )
	ROM_LOAD32_BYTE("d49-16.bin", 0x000002, 0x20000, CRC(8e73f739) SHA1(620a4d52abc00908cd1393babdc600b929019a51) )
	ROM_LOAD32_BYTE("d49-19.35",  0x000003, 0x20000, CRC(f0925800) SHA1(e8d91b216a0409080b77cc1e832b7d15c66a5eef) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d49-01", 0x000000, 0x200000, CRC(1dc89f1c) SHA1(9597b1d8c9b447080ca9401aee83bb4a64bb8332) )
  	ROM_LOAD16_BYTE("d49-02", 0x000001, 0x200000, CRC(1e4c374f) SHA1(512edc6a934578d0e7371410a041150d3b13aaad) )
	ROM_LOAD16_BYTE("d49-06", 0x400000, 0x100000, CRC(71ef4ee1) SHA1(1d7729dbc77f7201ff574e8aef65a55bd81c25a7) )
  	ROM_LOAD16_BYTE("d49-07", 0x400001, 0x100000, CRC(e5655b8f) SHA1(2c21745370bfe9dbf0e95f7ce42ed34a162bff64) )
	ROM_LOAD       ("d49-03", 0x900000, 0x200000, CRC(cf9a8727) SHA1(f21787fdcdd8be2009c2d481a9b2d7fc03ce782e) )
	ROM_LOAD       ("d49-08", 0xb00000, 0x100000, CRC(7d3c6536) SHA1(289b4bf79ebd9cbdf64ab956784d226e6d546654) )
	ROM_FILL       (          0x600000, 0x300000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d49-09", 0x000000, 0x080000, CRC(257ede01) SHA1(c36397d95706c5e68a7738c84829a51c5e8f5ef7) )
	ROM_LOAD16_BYTE("d49-10", 0x000001, 0x080000, CRC(f587b787) SHA1(22db4904c134756ddd0f753f197419d27e60a827) )
	ROM_LOAD       ("d49-11", 0x180000, 0x080000, CRC(11318b26) SHA1(a7153f9f406d52189f59cbe58d65f88f4e2e6fcc) )
	ROM_FILL       (          0x100000, 0x080000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d49-17", 0x100000, 0x40000, CRC(49942466) SHA1(5e355a079b81f129919c7599c0cc09a9e193ea41) )
	ROM_LOAD16_BYTE("d49-18", 0x100001, 0x40000, CRC(9d75b7d4) SHA1(1dc823327294f5c81b78f151fcd3d0550c208697) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d49-04", 0x000000, 0x200000, CRC(44b365a9) SHA1(14c4a6b193a0069360406c74c500ba24f2a55b62) )
	ROM_LOAD16_BYTE("d49-05", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( trstar )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d53-15-1.24", 0x000000, 0x40000, CRC(098bba94) SHA1(b77990213ac790d15bdc0dc1e8f7adf04fe5e952) )
	ROM_LOAD32_BYTE("d53-16-1.26", 0x000001, 0x40000, CRC(4fa8b15c) SHA1(821c21e1b958614ba6636330583b3661f9e0cebb) )
	ROM_LOAD32_BYTE("d53-18-1.37", 0x000002, 0x40000, CRC(aa71cfcc) SHA1(ba62c01255cdfe0821d1b72b7f11d6e1f88b09d7) )
	ROM_LOAD32_BYTE("d53-20-1.rom", 0x000003, 0x40000, CRC(4de1e287) SHA1(2b592ecbf8d81aca49844ed81c351818409f596f) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d53-03.45", 0x000000, 0x200000, CRC(91b66145) SHA1(df5bc2e544ce80a98db1fe28b4a8af8c3905c7eb) )
  	ROM_LOAD16_BYTE("d53-04.46", 0x000001, 0x200000, CRC(ac3a5e80) SHA1(8a6ea8096099b465d63d56abc79ed77304fd4fa4) )
	ROM_LOAD16_BYTE("d53-06.64", 0x400000, 0x100000, CRC(f4bac410) SHA1(569bcd81d596b24add5db4a145ae04750a1bb086) )
  	ROM_LOAD16_BYTE("d53-07.65", 0x400001, 0x100000, CRC(2f4773c3) SHA1(17cef13de0836923743b336cc5a64f7452629486) )
	ROM_LOAD       ("d53-05.47", 0x900000, 0x200000, CRC(b9b68b15) SHA1(c3783b09b22954a959188b80e537fa84d827ac47) )
	ROM_LOAD       ("d53-08.66", 0xb00000, 0x100000, CRC(ad13a1ee) SHA1(341112055b6bee33072c262f4ea7c4d0970888a6) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d53-09.48", 0x000000, 0x100000, CRC(690554d3) SHA1(113afd8fe7b77a30c2e3c5baca3f19d74902625b) )
	ROM_LOAD16_BYTE("d53-10.49", 0x000001, 0x100000, CRC(0ec05dc5) SHA1(781a6362ef963417fb6383a62dcc70d6f5b3131b) )
	ROM_LOAD       ("d53-11.50", 0x300000, 0x100000, CRC(39c0a546) SHA1(53f03586f6586032fc3b4f90e987c1128edbb0a7) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d53-13.10", 0x100000, 0x20000, CRC(877f0361) SHA1(eda58d71fb06f739bee1451d7aa7e7e6dee10e03) )
	ROM_LOAD16_BYTE("d53-14.23", 0x100001, 0x20000, CRC(a8664867) SHA1(dffddca469019abac33a1abe41c3fe83fbf553ce) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d53-01.2", 0x000000, 0x200000, CRC(28fd2d9b) SHA1(e08037795952a28e7a5e90437f1b9675aadfa136) )
	ROM_LOAD16_BYTE("d53-02.3", 0xc00000, 0x200000, CRC(8bd4367a) SHA1(9b274fe321c4faedb7d44f7998ae2e37c6899688) )
ROM_END

ROM_START( trstarj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d53-15-1.24", 0x000000, 0x40000, CRC(098bba94) SHA1(b77990213ac790d15bdc0dc1e8f7adf04fe5e952) )
	ROM_LOAD32_BYTE("d53-16-1.26", 0x000001, 0x40000, CRC(4fa8b15c) SHA1(821c21e1b958614ba6636330583b3661f9e0cebb) )
	ROM_LOAD32_BYTE("d53-18-1.37", 0x000002, 0x40000, CRC(aa71cfcc) SHA1(ba62c01255cdfe0821d1b72b7f11d6e1f88b09d7) )
	ROM_LOAD32_BYTE("d53-17-1.35", 0x000003, 0x40000, CRC(a3ef83ab) SHA1(c99170047e678a7acde1bf64f903f240e9384b94) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d53-03.45", 0x000000, 0x200000, CRC(91b66145) SHA1(df5bc2e544ce80a98db1fe28b4a8af8c3905c7eb) )
  	ROM_LOAD16_BYTE("d53-04.46", 0x000001, 0x200000, CRC(ac3a5e80) SHA1(8a6ea8096099b465d63d56abc79ed77304fd4fa4) )
	ROM_LOAD16_BYTE("d53-06.64", 0x400000, 0x100000, CRC(f4bac410) SHA1(569bcd81d596b24add5db4a145ae04750a1bb086) )
  	ROM_LOAD16_BYTE("d53-07.65", 0x400001, 0x100000, CRC(2f4773c3) SHA1(17cef13de0836923743b336cc5a64f7452629486) )
	ROM_LOAD       ("d53-05.47", 0x900000, 0x200000, CRC(b9b68b15) SHA1(c3783b09b22954a959188b80e537fa84d827ac47) )
	ROM_LOAD       ("d53-08.66", 0xb00000, 0x100000, CRC(ad13a1ee) SHA1(341112055b6bee33072c262f4ea7c4d0970888a6) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d53-09.48", 0x000000, 0x100000, CRC(690554d3) SHA1(113afd8fe7b77a30c2e3c5baca3f19d74902625b) )
	ROM_LOAD16_BYTE("d53-10.49", 0x000001, 0x100000, CRC(0ec05dc5) SHA1(781a6362ef963417fb6383a62dcc70d6f5b3131b) )
	ROM_LOAD       ("d53-11.50", 0x300000, 0x100000, CRC(39c0a546) SHA1(53f03586f6586032fc3b4f90e987c1128edbb0a7) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d53-13.10", 0x100000, 0x20000, CRC(877f0361) SHA1(eda58d71fb06f739bee1451d7aa7e7e6dee10e03) )
	ROM_LOAD16_BYTE("d53-14.23", 0x100001, 0x20000, CRC(a8664867) SHA1(dffddca469019abac33a1abe41c3fe83fbf553ce) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d53-01.2", 0x000000, 0x200000, CRC(28fd2d9b) SHA1(e08037795952a28e7a5e90437f1b9675aadfa136) )
	ROM_LOAD16_BYTE("d53-02.3", 0xc00000, 0x200000, CRC(8bd4367a) SHA1(9b274fe321c4faedb7d44f7998ae2e37c6899688) )
ROM_END

ROM_START( prmtmfgt )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d53-15-1.24", 0x000000, 0x40000, CRC(098bba94) SHA1(b77990213ac790d15bdc0dc1e8f7adf04fe5e952) )
	ROM_LOAD32_BYTE("d53-16-1.26", 0x000001, 0x40000, CRC(4fa8b15c) SHA1(821c21e1b958614ba6636330583b3661f9e0cebb) )
	ROM_LOAD32_BYTE("d53-18-1.37", 0x000002, 0x40000, CRC(aa71cfcc) SHA1(ba62c01255cdfe0821d1b72b7f11d6e1f88b09d7) )
	ROM_LOAD32_BYTE("d53-19-1.bin", 0x000003, 0x40000, CRC(3ae6d211) SHA1(f3e27e0169686633d8d8f2cbac05375aa94cfde9) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d53-03.45", 0x000000, 0x200000, CRC(91b66145) SHA1(df5bc2e544ce80a98db1fe28b4a8af8c3905c7eb) )
  	ROM_LOAD16_BYTE("d53-04.46", 0x000001, 0x200000, CRC(ac3a5e80) SHA1(8a6ea8096099b465d63d56abc79ed77304fd4fa4) )
	ROM_LOAD16_BYTE("d53-06.64", 0x400000, 0x100000, CRC(f4bac410) SHA1(569bcd81d596b24add5db4a145ae04750a1bb086) )
  	ROM_LOAD16_BYTE("d53-07.65", 0x400001, 0x100000, CRC(2f4773c3) SHA1(17cef13de0836923743b336cc5a64f7452629486) )
	ROM_LOAD       ("d53-05.47", 0x900000, 0x200000, CRC(b9b68b15) SHA1(c3783b09b22954a959188b80e537fa84d827ac47) )
	ROM_LOAD       ("d53-08.66", 0xb00000, 0x100000, CRC(ad13a1ee) SHA1(341112055b6bee33072c262f4ea7c4d0970888a6) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d53-09.48", 0x000000, 0x100000, CRC(690554d3) SHA1(113afd8fe7b77a30c2e3c5baca3f19d74902625b) )
	ROM_LOAD16_BYTE("d53-10.49", 0x000001, 0x100000, CRC(0ec05dc5) SHA1(781a6362ef963417fb6383a62dcc70d6f5b3131b) )
	ROM_LOAD       ("d53-11.50", 0x300000, 0x100000, CRC(39c0a546) SHA1(53f03586f6586032fc3b4f90e987c1128edbb0a7) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d53-13.10", 0x100000, 0x20000, CRC(877f0361) SHA1(eda58d71fb06f739bee1451d7aa7e7e6dee10e03) )
	ROM_LOAD16_BYTE("d53-14.23", 0x100001, 0x20000, CRC(a8664867) SHA1(dffddca469019abac33a1abe41c3fe83fbf553ce) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d53-01.2", 0x000000, 0x200000, CRC(28fd2d9b) SHA1(e08037795952a28e7a5e90437f1b9675aadfa136) )
	ROM_LOAD16_BYTE("d53-02.3", 0xc00000, 0x200000, CRC(8bd4367a) SHA1(9b274fe321c4faedb7d44f7998ae2e37c6899688) )
ROM_END

ROM_START( trstaro )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d53-15.24", 0x000000, 0x40000, CRC(f24de51b) SHA1(d45d1b60901995edf0721eae7eb8c6e829f47d8d) )
	ROM_LOAD32_BYTE("d53-16.26", 0x000001, 0x40000, CRC(ffc84429) SHA1(23354c1a65853c06e5c959957a92b700b1418fd4) )
	ROM_LOAD32_BYTE("d53-18.37", 0x000002, 0x40000, CRC(ea2d6e13) SHA1(96461b73de745c4b0ac99267931106e1d5dcb664) )
	ROM_LOAD32_BYTE("d53-20.rom",0x000003, 0x40000, CRC(77e1f267) SHA1(763ccab234c45ea00908198b0aef3ba63ddfb8f8) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d53-03.45", 0x000000, 0x200000, CRC(91b66145) SHA1(df5bc2e544ce80a98db1fe28b4a8af8c3905c7eb) )
  	ROM_LOAD16_BYTE("d53-04.46", 0x000001, 0x200000, CRC(ac3a5e80) SHA1(8a6ea8096099b465d63d56abc79ed77304fd4fa4) )
	ROM_LOAD16_BYTE("d53-06.64", 0x400000, 0x100000, CRC(f4bac410) SHA1(569bcd81d596b24add5db4a145ae04750a1bb086) )
  	ROM_LOAD16_BYTE("d53-07.65", 0x400001, 0x100000, CRC(2f4773c3) SHA1(17cef13de0836923743b336cc5a64f7452629486) )
	ROM_LOAD       ("d53-05.47", 0x900000, 0x200000, CRC(b9b68b15) SHA1(c3783b09b22954a959188b80e537fa84d827ac47) )
	ROM_LOAD       ("d53-08.66", 0xb00000, 0x100000, CRC(ad13a1ee) SHA1(341112055b6bee33072c262f4ea7c4d0970888a6) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d53-09.48", 0x000000, 0x100000, CRC(690554d3) SHA1(113afd8fe7b77a30c2e3c5baca3f19d74902625b) )
	ROM_LOAD16_BYTE("d53-10.49", 0x000001, 0x100000, CRC(0ec05dc5) SHA1(781a6362ef963417fb6383a62dcc70d6f5b3131b) )
	ROM_LOAD       ("d53-11.50", 0x300000, 0x100000, CRC(39c0a546) SHA1(53f03586f6586032fc3b4f90e987c1128edbb0a7) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d53-13.10", 0x100000, 0x20000, CRC(877f0361) SHA1(eda58d71fb06f739bee1451d7aa7e7e6dee10e03) )
	ROM_LOAD16_BYTE("d53-14.23", 0x100001, 0x20000, CRC(a8664867) SHA1(dffddca469019abac33a1abe41c3fe83fbf553ce) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d53-01.2", 0x000000, 0x200000, CRC(28fd2d9b) SHA1(e08037795952a28e7a5e90437f1b9675aadfa136) )
	ROM_LOAD16_BYTE("d53-02.3", 0xc00000, 0x200000, CRC(8bd4367a) SHA1(9b274fe321c4faedb7d44f7998ae2e37c6899688) )
ROM_END

ROM_START( trstaroj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d53-15.24", 0x000000, 0x40000, CRC(f24de51b) SHA1(d45d1b60901995edf0721eae7eb8c6e829f47d8d) )
	ROM_LOAD32_BYTE("d53-16.26", 0x000001, 0x40000, CRC(ffc84429) SHA1(23354c1a65853c06e5c959957a92b700b1418fd4) )
	ROM_LOAD32_BYTE("d53-18.37", 0x000002, 0x40000, CRC(ea2d6e13) SHA1(96461b73de745c4b0ac99267931106e1d5dcb664) )
	ROM_LOAD32_BYTE("d53-17.35", 0x000003, 0x40000, CRC(99ef934b) SHA1(a04a27f67b2db87549f4dc09cf9d00f3480351a6) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d53-03.45", 0x000000, 0x200000, CRC(91b66145) SHA1(df5bc2e544ce80a98db1fe28b4a8af8c3905c7eb) )
  	ROM_LOAD16_BYTE("d53-04.46", 0x000001, 0x200000, CRC(ac3a5e80) SHA1(8a6ea8096099b465d63d56abc79ed77304fd4fa4) )
	ROM_LOAD16_BYTE("d53-06.64", 0x400000, 0x100000, CRC(f4bac410) SHA1(569bcd81d596b24add5db4a145ae04750a1bb086) )
  	ROM_LOAD16_BYTE("d53-07.65", 0x400001, 0x100000, CRC(2f4773c3) SHA1(17cef13de0836923743b336cc5a64f7452629486) )
	ROM_LOAD       ("d53-05.47", 0x900000, 0x200000, CRC(b9b68b15) SHA1(c3783b09b22954a959188b80e537fa84d827ac47) )
	ROM_LOAD       ("d53-08.66", 0xb00000, 0x100000, CRC(ad13a1ee) SHA1(341112055b6bee33072c262f4ea7c4d0970888a6) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d53-09.48", 0x000000, 0x100000, CRC(690554d3) SHA1(113afd8fe7b77a30c2e3c5baca3f19d74902625b) )
	ROM_LOAD16_BYTE("d53-10.49", 0x000001, 0x100000, CRC(0ec05dc5) SHA1(781a6362ef963417fb6383a62dcc70d6f5b3131b) )
	ROM_LOAD       ("d53-11.50", 0x300000, 0x100000, CRC(39c0a546) SHA1(53f03586f6586032fc3b4f90e987c1128edbb0a7) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d53-13.10", 0x100000, 0x20000, CRC(877f0361) SHA1(eda58d71fb06f739bee1451d7aa7e7e6dee10e03) )
	ROM_LOAD16_BYTE("d53-14.23", 0x100001, 0x20000, CRC(a8664867) SHA1(dffddca469019abac33a1abe41c3fe83fbf553ce) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d53-01.2", 0x000000, 0x200000, CRC(28fd2d9b) SHA1(e08037795952a28e7a5e90437f1b9675aadfa136) )
	ROM_LOAD16_BYTE("d53-02.3", 0xc00000, 0x200000, CRC(8bd4367a) SHA1(9b274fe321c4faedb7d44f7998ae2e37c6899688) )
ROM_END

ROM_START( prmtmfgo )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d53-15.24", 0x000000, 0x40000, CRC(f24de51b) SHA1(d45d1b60901995edf0721eae7eb8c6e829f47d8d) )
	ROM_LOAD32_BYTE("d53-16.26", 0x000001, 0x40000, CRC(ffc84429) SHA1(23354c1a65853c06e5c959957a92b700b1418fd4) )
	ROM_LOAD32_BYTE("d53-18.37", 0x000002, 0x40000, CRC(ea2d6e13) SHA1(96461b73de745c4b0ac99267931106e1d5dcb664) )
	ROM_LOAD32_BYTE("d53-19.35", 0x000003, 0x40000, CRC(00e6c2f1) SHA1(cf4b9ee35be8138abfaa354d01184efbfe83cea2) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d53-03.45", 0x000000, 0x200000, CRC(91b66145) SHA1(df5bc2e544ce80a98db1fe28b4a8af8c3905c7eb) )
  	ROM_LOAD16_BYTE("d53-04.46", 0x000001, 0x200000, CRC(ac3a5e80) SHA1(8a6ea8096099b465d63d56abc79ed77304fd4fa4) )
	ROM_LOAD16_BYTE("d53-06.64", 0x400000, 0x100000, CRC(f4bac410) SHA1(569bcd81d596b24add5db4a145ae04750a1bb086) )
  	ROM_LOAD16_BYTE("d53-07.65", 0x400001, 0x100000, CRC(2f4773c3) SHA1(17cef13de0836923743b336cc5a64f7452629486) )
	ROM_LOAD       ("d53-05.47", 0x900000, 0x200000, CRC(b9b68b15) SHA1(c3783b09b22954a959188b80e537fa84d827ac47) )
	ROM_LOAD       ("d53-08.66", 0xb00000, 0x100000, CRC(ad13a1ee) SHA1(341112055b6bee33072c262f4ea7c4d0970888a6) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d53-09.48", 0x000000, 0x100000, CRC(690554d3) SHA1(113afd8fe7b77a30c2e3c5baca3f19d74902625b) )
	ROM_LOAD16_BYTE("d53-10.49", 0x000001, 0x100000, CRC(0ec05dc5) SHA1(781a6362ef963417fb6383a62dcc70d6f5b3131b) )
	ROM_LOAD       ("d53-11.50", 0x300000, 0x100000, CRC(39c0a546) SHA1(53f03586f6586032fc3b4f90e987c1128edbb0a7) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d53-13.10", 0x100000, 0x20000, CRC(877f0361) SHA1(eda58d71fb06f739bee1451d7aa7e7e6dee10e03) )
	ROM_LOAD16_BYTE("d53-14.23", 0x100001, 0x20000, CRC(a8664867) SHA1(dffddca469019abac33a1abe41c3fe83fbf553ce) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d53-01.2", 0x000000, 0x200000, CRC(28fd2d9b) SHA1(e08037795952a28e7a5e90437f1b9675aadfa136) )
	ROM_LOAD16_BYTE("d53-02.3", 0xc00000, 0x200000, CRC(8bd4367a) SHA1(9b274fe321c4faedb7d44f7998ae2e37c6899688) )
ROM_END

ROM_START( gunlock )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d66-18.rom", 0x000000, 0x40000, CRC(8418513e) SHA1(a071647268fad08802b88fb6d795612218b7ddef) )
	ROM_LOAD32_BYTE("d66-19.rom", 0x000001, 0x40000, CRC(95731473) SHA1(ab79821cd6098a4db84ebc9a499c29b1525510a5) )
	ROM_LOAD32_BYTE("d66-21.rom", 0x000002, 0x40000, CRC(bd0d60f2) SHA1(609ed2b04cb9efc4b370dcbdf22fd168318989be) )
	ROM_LOAD32_BYTE("d66-24.rom", 0x000003, 0x40000, CRC(97816378) SHA1(b22cb442b663c7a10fbc292583cd788f66f10a25) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d66-03.rom", 0x000000, 0x100000, CRC(e7a4a491) SHA1(87837e8dd1c9a1db5e540b678233634bd52328f0) )
	ROM_LOAD16_BYTE("d66-04.rom", 0x000001, 0x100000, CRC(c1c7aaa7) SHA1(f929516cf50d82b2d1d1b4c49a0eb1dea819aae1) )
	ROM_LOAD       ("d66-05.rom", 0x300000, 0x100000, CRC(a3cefe04) SHA1(dd4f47a814853f4512ce25c5f25121c53ee4ada1) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d66-06.rom", 0x000000, 0x100000, CRC(b3d8126d) SHA1(3cbb44f396973c36abdf3fdf391becb22bb6d661) )
  	ROM_LOAD16_BYTE("d66-07.rom", 0x000001, 0x100000, CRC(a6da9be7) SHA1(b528505ab925db75acf31bfbed2035cbe36e7a74) )
	ROM_LOAD       ("d66-08.rom", 0x300000, 0x100000, CRC(9959f30b) SHA1(64bf2bf995c283c00d968e3c078b824de4084d3d) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("d66-23.rom", 0x100000, 0x40000, CRC(57fb7c49) SHA1(f8709fd1e9ea7cee10ee2288d13339f675a7d3ae) )
	ROM_LOAD16_BYTE("d66-22.rom", 0x100001, 0x40000, CRC(83dd7f9b) SHA1(dae21f64232d3e268f22b5e9899e0b726fdc9a9f) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d66-01.rom", 0x000000, 0x200000, CRC(58c92efa) SHA1(bb207b35b8f9538362bb99a9ec8df206694f00ce) )
	ROM_LOAD16_BYTE("d66-02.rom", 0xc00000, 0x200000, CRC(dcdafaab) SHA1(c981c7e54a2a9aaa85bb758691858495d623b029) )
ROM_END

ROM_START( rayforce )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d66-18.rom", 0x000000, 0x40000, CRC(8418513e) SHA1(a071647268fad08802b88fb6d795612218b7ddef) )
	ROM_LOAD32_BYTE("d66-19.rom", 0x000001, 0x40000, CRC(95731473) SHA1(ab79821cd6098a4db84ebc9a499c29b1525510a5) )
	ROM_LOAD32_BYTE("d66-21.rom", 0x000002, 0x40000, CRC(bd0d60f2) SHA1(609ed2b04cb9efc4b370dcbdf22fd168318989be) )
	ROM_LOAD32_BYTE("gunlocku.35",0x000003, 0x40000, CRC(e08653ee) SHA1(03ae4e457369a4b29cd7d52408e28725e41ee244) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d66-03.rom", 0x000000, 0x100000, CRC(e7a4a491) SHA1(87837e8dd1c9a1db5e540b678233634bd52328f0) )
	ROM_LOAD16_BYTE("d66-04.rom", 0x000001, 0x100000, CRC(c1c7aaa7) SHA1(f929516cf50d82b2d1d1b4c49a0eb1dea819aae1) )
	ROM_LOAD       ("d66-05.rom", 0x300000, 0x100000, CRC(a3cefe04) SHA1(dd4f47a814853f4512ce25c5f25121c53ee4ada1) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d66-06.rom", 0x000000, 0x100000, CRC(b3d8126d) SHA1(3cbb44f396973c36abdf3fdf391becb22bb6d661) )
  	ROM_LOAD16_BYTE("d66-07.rom", 0x000001, 0x100000, CRC(a6da9be7) SHA1(b528505ab925db75acf31bfbed2035cbe36e7a74) )
	ROM_LOAD       ("d66-08.rom", 0x300000, 0x100000, CRC(9959f30b) SHA1(64bf2bf995c283c00d968e3c078b824de4084d3d) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("d66-23.rom", 0x100000, 0x40000, CRC(57fb7c49) SHA1(f8709fd1e9ea7cee10ee2288d13339f675a7d3ae) )
	ROM_LOAD16_BYTE("d66-22.rom", 0x100001, 0x40000, CRC(83dd7f9b) SHA1(dae21f64232d3e268f22b5e9899e0b726fdc9a9f) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d66-01.rom", 0x000000, 0x200000, CRC(58c92efa) SHA1(bb207b35b8f9538362bb99a9ec8df206694f00ce) )
	ROM_LOAD16_BYTE("d66-02.rom", 0xc00000, 0x200000, CRC(dcdafaab) SHA1(c981c7e54a2a9aaa85bb758691858495d623b029) )
ROM_END

ROM_START( rayforcj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d66-18.rom", 0x000000, 0x40000, CRC(8418513e) SHA1(a071647268fad08802b88fb6d795612218b7ddef) )
	ROM_LOAD32_BYTE("d66-19.rom", 0x000001, 0x40000, CRC(95731473) SHA1(ab79821cd6098a4db84ebc9a499c29b1525510a5) )
	ROM_LOAD32_BYTE("d66-21.rom", 0x000002, 0x40000, CRC(bd0d60f2) SHA1(609ed2b04cb9efc4b370dcbdf22fd168318989be) )
	ROM_LOAD32_BYTE("d66-20.35",  0x000003, 0x40000, CRC(798f0254) SHA1(b070588053bddc3d0b0c2660192b0cb16bf8247f) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d66-03.rom", 0x000000, 0x100000, CRC(e7a4a491) SHA1(87837e8dd1c9a1db5e540b678233634bd52328f0) )
	ROM_LOAD16_BYTE("d66-04.rom", 0x000001, 0x100000, CRC(c1c7aaa7) SHA1(f929516cf50d82b2d1d1b4c49a0eb1dea819aae1) )
	ROM_LOAD       ("d66-05.rom", 0x300000, 0x100000, CRC(a3cefe04) SHA1(dd4f47a814853f4512ce25c5f25121c53ee4ada1) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d66-06.rom", 0x000000, 0x100000, CRC(b3d8126d) SHA1(3cbb44f396973c36abdf3fdf391becb22bb6d661) )
  	ROM_LOAD16_BYTE("d66-07.rom", 0x000001, 0x100000, CRC(a6da9be7) SHA1(b528505ab925db75acf31bfbed2035cbe36e7a74) )
	ROM_LOAD       ("d66-08.rom", 0x300000, 0x100000, CRC(9959f30b) SHA1(64bf2bf995c283c00d968e3c078b824de4084d3d) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("d66-23.rom", 0x100000, 0x40000, CRC(57fb7c49) SHA1(f8709fd1e9ea7cee10ee2288d13339f675a7d3ae) )
	ROM_LOAD16_BYTE("d66-22.rom", 0x100001, 0x40000, CRC(83dd7f9b) SHA1(dae21f64232d3e268f22b5e9899e0b726fdc9a9f) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d66-01.rom", 0x000000, 0x200000, CRC(58c92efa) SHA1(bb207b35b8f9538362bb99a9ec8df206694f00ce) )
	ROM_LOAD16_BYTE("d66-02.rom", 0xc00000, 0x200000, CRC(dcdafaab) SHA1(c981c7e54a2a9aaa85bb758691858495d623b029) )
ROM_END

ROM_START( scfinals )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d68-01", 0x000000, 0x40000, CRC(cb951856) SHA1(c7b0418b957ed0feecc9dffe5a963bd22df0ac4e) )
	ROM_LOAD32_BYTE("d68-02", 0x000001, 0x40000, CRC(4f94413a) SHA1(b46a35ab0150d5d5e53149c53f11978fbfa28159) )
	ROM_LOAD32_BYTE("d68-04", 0x000002, 0x40000, CRC(4a4e4972) SHA1(5300380a57f70fe91c69f2b1e9d25253081e61da) )
	ROM_LOAD32_BYTE("d68-03", 0x000003, 0x40000, CRC(a40be699) SHA1(03101d2aef8e7c0c332a3c8c0a025024f6cfe580) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d49-01", 0x000000, 0x200000, CRC(1dc89f1c) SHA1(9597b1d8c9b447080ca9401aee83bb4a64bb8332) )
  	ROM_LOAD16_BYTE("d49-02", 0x000001, 0x200000, CRC(1e4c374f) SHA1(512edc6a934578d0e7371410a041150d3b13aaad) )
	ROM_LOAD16_BYTE("d49-06", 0x400000, 0x100000, CRC(71ef4ee1) SHA1(1d7729dbc77f7201ff574e8aef65a55bd81c25a7) )
  	ROM_LOAD16_BYTE("d49-07", 0x400001, 0x100000, CRC(e5655b8f) SHA1(2c21745370bfe9dbf0e95f7ce42ed34a162bff64) )
	ROM_LOAD       ("d49-03", 0x900000, 0x200000, CRC(cf9a8727) SHA1(f21787fdcdd8be2009c2d481a9b2d7fc03ce782e) )
	ROM_LOAD       ("d49-08", 0xb00000, 0x100000, CRC(7d3c6536) SHA1(289b4bf79ebd9cbdf64ab956784d226e6d546654) )
	ROM_FILL       (          0x600000, 0x300000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d49-09", 0x000000, 0x080000, CRC(257ede01) SHA1(c36397d95706c5e68a7738c84829a51c5e8f5ef7) )
	ROM_LOAD16_BYTE("d49-10", 0x000001, 0x080000, CRC(f587b787) SHA1(22db4904c134756ddd0f753f197419d27e60a827) )
	ROM_LOAD       ("d49-11", 0x180000, 0x080000, CRC(11318b26) SHA1(a7153f9f406d52189f59cbe58d65f88f4e2e6fcc) )
	ROM_FILL       (          0x100000, 0x080000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d49-17", 0x100000, 0x40000, CRC(49942466) SHA1(5e355a079b81f129919c7599c0cc09a9e193ea41) )
	ROM_LOAD16_BYTE("d49-18", 0x100001, 0x40000, CRC(9d75b7d4) SHA1(1dc823327294f5c81b78f151fcd3d0550c208697) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d49-04", 0x000000, 0x200000, CRC(44b365a9) SHA1(14c4a6b193a0069360406c74c500ba24f2a55b62) )
	ROM_LOAD16_BYTE("d49-05", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( dungeonm )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d69-20.bin", 0x000000, 0x80000, CRC(33650fe4) SHA1(df8b775749b1f0f02d0df6141597cc49fb3ae227) )
	ROM_LOAD32_BYTE("d69-13.bin", 0x000001, 0x80000, CRC(dec2ec17) SHA1(8472a5aaea9e4e4fb5f7f4b5eda356b590d1541d) )
	ROM_LOAD32_BYTE("d69-15.bin", 0x000002, 0x80000, CRC(323e1955) SHA1(d76582d1ff5a9aa87a498fea3280bc3c25ee9ec0) )
	ROM_LOAD32_BYTE("d69-wrld.bin", 0x000003, 0x80000, CRC(f99e175d) SHA1(8f5f4710d72faed978e68e6e36703f47e8bab06f) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d69-06.bin", 0x000000, 0x200000, CRC(cb4aac81) SHA1(15d315c6b9695cc2fe07defc67c7a4fb26de1950) )
  	ROM_LOAD16_BYTE("d69-07.bin", 0x000001, 0x200000, CRC(b749f984) SHA1(39fd662bdc42e812519181a640a83e29e300826a) )
	ROM_LOAD16_BYTE("d69-09.bin", 0x400000, 0x100000, CRC(a96c19b8) SHA1(7872b4dd9d51877bed709fec393413e41d6b954f) )
	ROM_LOAD16_BYTE("d69-10.bin", 0x400001, 0x100000, CRC(36aa80c6) SHA1(aeb5f7632810564426761b5798539bf4c4a0c64c) )
	ROM_LOAD       ("d69-08.bin", 0x900000, 0x200000, CRC(5b68d7d8) SHA1(f2ee3dd7100a3c9d8f402fe36dae2bc66cb17be3) )
	ROM_LOAD       ("d69-11.bin", 0xb00000, 0x100000, CRC(c11adf92) SHA1(ee9ce49a43b419c4f44ac1aea8d0a12d7b289244) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d69-03.bin", 0x000000, 0x200000, CRC(6999c86f) SHA1(8a91930edfc0b5d23e59f8c3b43131db6edb4d37) )
  	ROM_LOAD16_BYTE("d69-04.bin", 0x000001, 0x200000, CRC(cc91dcb7) SHA1(97f510b1e1a3adf49efe82babdd7abce3756ce4b) )
	ROM_LOAD       ("d69-05.bin", 0x600000, 0x200000, CRC(f9f5433c) SHA1(d3de66385d883c72967c44bc29983d7a79f665d1) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d69-18.bin", 0x100000, 0x20000, CRC(04600d7b) SHA1(666cfab09b61fd6e0bc4ff277018ebf1cda01b0e) )
	ROM_LOAD16_BYTE("d69-19.bin", 0x100001, 0x20000, CRC(1484e853) SHA1(4459c18ba005786483c652857e527c6093efb036) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d69-01.bin", 0x000000, 0x200000, CRC(9ac93ac2) SHA1(1c44f6ba95505f85b0c8a90395f09d2a49da3553) )
	ROM_LOAD16_BYTE("d69-02.bin", 0xc00000, 0x200000, CRC(dce28dd7) SHA1(eacfc98349b0608fc1a944c11f0483fb6caa4445) )
ROM_END

ROM_START( dungenmu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d69-20.bin", 0x000000, 0x80000, CRC(33650fe4) SHA1(df8b775749b1f0f02d0df6141597cc49fb3ae227) )
	ROM_LOAD32_BYTE("d69-13.bin", 0x000001, 0x80000, CRC(dec2ec17) SHA1(8472a5aaea9e4e4fb5f7f4b5eda356b590d1541d) )
	ROM_LOAD32_BYTE("d69-15.bin", 0x000002, 0x80000, CRC(323e1955) SHA1(d76582d1ff5a9aa87a498fea3280bc3c25ee9ec0) )
	ROM_LOAD32_BYTE("d69-usa.bin", 0x000003, 0x80000, CRC(c9d4e051) SHA1(7c7e76f0d0bca305ff6761aa509d344c2dac8e2e) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d69-06.bin", 0x000000, 0x200000, CRC(cb4aac81) SHA1(15d315c6b9695cc2fe07defc67c7a4fb26de1950) )
  	ROM_LOAD16_BYTE("d69-07.bin", 0x000001, 0x200000, CRC(b749f984) SHA1(39fd662bdc42e812519181a640a83e29e300826a) )
	ROM_LOAD16_BYTE("d69-09.bin", 0x400000, 0x100000, CRC(a96c19b8) SHA1(7872b4dd9d51877bed709fec393413e41d6b954f) )
	ROM_LOAD16_BYTE("d69-10.bin", 0x400001, 0x100000, CRC(36aa80c6) SHA1(aeb5f7632810564426761b5798539bf4c4a0c64c) )
	ROM_LOAD       ("d69-08.bin", 0x900000, 0x200000, CRC(5b68d7d8) SHA1(f2ee3dd7100a3c9d8f402fe36dae2bc66cb17be3) )
	ROM_LOAD       ("d69-11.bin", 0xb00000, 0x100000, CRC(c11adf92) SHA1(ee9ce49a43b419c4f44ac1aea8d0a12d7b289244) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d69-03.bin", 0x000000, 0x200000, CRC(6999c86f) SHA1(8a91930edfc0b5d23e59f8c3b43131db6edb4d37) )
  	ROM_LOAD16_BYTE("d69-04.bin", 0x000001, 0x200000, CRC(cc91dcb7) SHA1(97f510b1e1a3adf49efe82babdd7abce3756ce4b) )
	ROM_LOAD       ("d69-05.bin", 0x600000, 0x200000, CRC(f9f5433c) SHA1(d3de66385d883c72967c44bc29983d7a79f665d1) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d69-18.bin", 0x100000, 0x20000, CRC(04600d7b) SHA1(666cfab09b61fd6e0bc4ff277018ebf1cda01b0e) )
	ROM_LOAD16_BYTE("d69-19.bin", 0x100001, 0x20000, CRC(1484e853) SHA1(4459c18ba005786483c652857e527c6093efb036) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d69-01.bin", 0x000000, 0x200000, CRC(9ac93ac2) SHA1(1c44f6ba95505f85b0c8a90395f09d2a49da3553) )
	ROM_LOAD16_BYTE("d69-02.bin", 0xc00000, 0x200000, CRC(dce28dd7) SHA1(eacfc98349b0608fc1a944c11f0483fb6caa4445) )
ROM_END

ROM_START( lightbr )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d69-20.bin", 0x000000, 0x80000, CRC(33650fe4) SHA1(df8b775749b1f0f02d0df6141597cc49fb3ae227) )
	ROM_LOAD32_BYTE("d69-13.bin", 0x000001, 0x80000, CRC(dec2ec17) SHA1(8472a5aaea9e4e4fb5f7f4b5eda356b590d1541d) )
	ROM_LOAD32_BYTE("d69-15.bin", 0x000002, 0x80000, CRC(323e1955) SHA1(d76582d1ff5a9aa87a498fea3280bc3c25ee9ec0) )
	ROM_LOAD32_BYTE("d69-14.bin", 0x000003, 0x80000, CRC(990bf945) SHA1(797794d7afc1e6e98ce1bfb3de3c241a96a8fa01) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d69-06.bin", 0x000000, 0x200000, CRC(cb4aac81) SHA1(15d315c6b9695cc2fe07defc67c7a4fb26de1950) )
  	ROM_LOAD16_BYTE("d69-07.bin", 0x000001, 0x200000, CRC(b749f984) SHA1(39fd662bdc42e812519181a640a83e29e300826a) )
	ROM_LOAD16_BYTE("d69-09.bin", 0x400000, 0x100000, CRC(a96c19b8) SHA1(7872b4dd9d51877bed709fec393413e41d6b954f) )
	ROM_LOAD16_BYTE("d69-10.bin", 0x400001, 0x100000, CRC(36aa80c6) SHA1(aeb5f7632810564426761b5798539bf4c4a0c64c) )
	ROM_LOAD       ("d69-08.bin", 0x900000, 0x200000, CRC(5b68d7d8) SHA1(f2ee3dd7100a3c9d8f402fe36dae2bc66cb17be3) )
	ROM_LOAD       ("d69-11.bin", 0xb00000, 0x100000, CRC(c11adf92) SHA1(ee9ce49a43b419c4f44ac1aea8d0a12d7b289244) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d69-03.bin", 0x000000, 0x200000, CRC(6999c86f) SHA1(8a91930edfc0b5d23e59f8c3b43131db6edb4d37) )
  	ROM_LOAD16_BYTE("d69-04.bin", 0x000001, 0x200000, CRC(cc91dcb7) SHA1(97f510b1e1a3adf49efe82babdd7abce3756ce4b) )
	ROM_LOAD       ("d69-05.bin", 0x600000, 0x200000, CRC(f9f5433c) SHA1(d3de66385d883c72967c44bc29983d7a79f665d1) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d69-18.bin", 0x100000, 0x20000, CRC(04600d7b) SHA1(666cfab09b61fd6e0bc4ff277018ebf1cda01b0e) )
	ROM_LOAD16_BYTE("d69-19.bin", 0x100001, 0x20000, CRC(1484e853) SHA1(4459c18ba005786483c652857e527c6093efb036) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d69-01.bin", 0x000000, 0x200000, CRC(9ac93ac2) SHA1(1c44f6ba95505f85b0c8a90395f09d2a49da3553) )
	ROM_LOAD16_BYTE("d69-02.bin", 0xc00000, 0x200000, CRC(dce28dd7) SHA1(eacfc98349b0608fc1a944c11f0483fb6caa4445) )
ROM_END

ROM_START( intcup94 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d78-07", 0x000000, 0x20000, CRC(8525d990) SHA1(b28aeb8727d615cae9eafd7710bf833a612ef7d4) )
	ROM_LOAD32_BYTE("d78-06", 0x000001, 0x20000, CRC(42db1d41) SHA1(daf617764b04cd24e76dfa95423213c2a3692068) )
	ROM_LOAD32_BYTE("d78-05", 0x000002, 0x20000, CRC(5f7fbbbc) SHA1(8936bcc4026b2819b8708911c9defe4436d070ad) )
	ROM_LOAD32_BYTE("d78-11", 0x000003, 0x20000, CRC(bb9d2987) SHA1(98bea0346702eefd9f6f1839b95932b9b8bca902) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d49-01", 0x000000, 0x200000, CRC(1dc89f1c) SHA1(9597b1d8c9b447080ca9401aee83bb4a64bb8332) )
  	ROM_LOAD16_BYTE("d49-02", 0x000001, 0x200000, CRC(1e4c374f) SHA1(512edc6a934578d0e7371410a041150d3b13aaad) )
	ROM_LOAD16_BYTE("d49-06", 0x400000, 0x100000, CRC(71ef4ee1) SHA1(1d7729dbc77f7201ff574e8aef65a55bd81c25a7) )
  	ROM_LOAD16_BYTE("d49-07", 0x400001, 0x100000, CRC(e5655b8f) SHA1(2c21745370bfe9dbf0e95f7ce42ed34a162bff64) )
	ROM_LOAD       ("d49-03", 0x900000, 0x200000, CRC(cf9a8727) SHA1(f21787fdcdd8be2009c2d481a9b2d7fc03ce782e) )
	ROM_LOAD       ("d49-08", 0xb00000, 0x100000, CRC(7d3c6536) SHA1(289b4bf79ebd9cbdf64ab956784d226e6d546654) )
	ROM_FILL       (          0x600000, 0x300000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d78-01", 0x000000, 0x080000, NO_DUMP ) /* 0x543f8967 from raine */
	ROM_LOAD16_BYTE("d78-02", 0x000001, 0x080000, NO_DUMP ) /* 0xe8289394 from raine */
	ROM_LOAD       ("d78-03", 0x180000, 0x080000, NO_DUMP ) /* 0xa8bc36e5 from raine */
	ROM_FILL       (          0x100000, 0x080000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	/* these might be ok, but I have a feeling they could be half the correct size, there isn't much sound
	   when the GFX roms are (re)dumped this should be checked too */
	ROM_LOAD16_BYTE("d78-08", 0x100000, 0x20000, BAD_DUMP CRC(a629d07c) SHA1(b2904e106633a3960ceb2bc58b600ea60034ff0b) ) /* are these the right size? */
	ROM_LOAD16_BYTE("d78-09", 0x100001, 0x20000, BAD_DUMP CRC(1f0efe01) SHA1(7bff748b9fcee170e430d90ee07eb9975d8fba59) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d49-04", 0x000000, 0x200000, CRC(44b365a9) SHA1(14c4a6b193a0069360406c74c500ba24f2a55b62) )
	ROM_LOAD16_BYTE("d49-05", 0x600000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
ROM_END

ROM_START( kaiserkn )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d84-25.rom", 0x000000, 0x80000, CRC(2840893f) SHA1(079dece4667b029189622476cc618b88e57243a6) )
	ROM_LOAD32_BYTE("d84-24.rom", 0x000001, 0x80000, CRC(bf20c755) SHA1(9f6edfe9bb40051e8a93d06a391c993ed7288db6) )
	ROM_LOAD32_BYTE("d84-23.rom", 0x000002, 0x80000, CRC(39f12a9b) SHA1(4b3fe9b8b0abb46feacd11ffb6b505568f892483) )
	ROM_LOAD32_BYTE("d84-29.rom", 0x000003, 0x80000, CRC(9821f17a) SHA1(4a2c1ebeb1a1d3d756957956c883f8374aaf4f8d) )

	ROM_REGION(0x1a00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d84-03.rom", 0x0000000, 0x200000, CRC(d786f552) SHA1(f73146892f714b5706d568fc8a135fddaa656570) )
  	ROM_LOAD16_BYTE("d84-04.rom", 0x0000001, 0x200000, CRC(d1f32b5d) SHA1(35289cce64fdbb8d966dd1d5307b5393be5e7799) )
	ROM_LOAD16_BYTE("d84-06.rom", 0x0400000, 0x200000, CRC(fa924dab) SHA1(28a8c3cd701f8df0c53069bb576bb2a820f3a331) )
  	ROM_LOAD16_BYTE("d84-07.rom", 0x0400001, 0x200000, CRC(54517a6b) SHA1(6e2c213c7ec1a3b78ad7e71db2326602557fd0f8) )
	ROM_LOAD16_BYTE("d84-09.rom", 0x0800000, 0x200000, CRC(faa78d98) SHA1(da3a2c5a45dd169743f113aa08e574f732e1f0fd) )
  	ROM_LOAD16_BYTE("d84-10.rom", 0x0800001, 0x200000, CRC(b84b7320) SHA1(f5de0d6da50d8ed753607b51e46bc9a4572ef431) )
	ROM_LOAD16_BYTE("d84-19.rom", 0x0c00000, 0x080000, CRC(6ddf77e5) SHA1(a1323acaed37fce62a19e63a0800d9d1dc2cfff7) )
  	ROM_LOAD16_BYTE("d84-20.rom", 0x0c00001, 0x080000, CRC(f85041e5) SHA1(6b2814514338f550d6aa14dbe39e848e8e64edee) )
	ROM_LOAD       ("d84-05.rom", 0x1380000, 0x200000, CRC(31a3c75d) SHA1(1a16ccb6a0a03ab715e5b016ab3b1b2cd0f1ae41) )
	ROM_LOAD       ("d84-08.rom", 0x1580000, 0x200000, CRC(07347bf1) SHA1(34bd359933acdec7fd1ce047092a30d1177afc2c) )
	ROM_LOAD       ("d84-11.rom", 0x1780000, 0x200000, CRC(a062c1d4) SHA1(158912aa3dd75c3961bf738f9ac9034f0b005b60) )
	ROM_LOAD       ("d84-21.rom", 0x1980000, 0x080000, CRC(89f68b66) SHA1(95916f02f71357324effe59da4f847f2f30ea34a) )
	ROM_FILL       (              0x0d00000, 0x680000, 0 )

	ROM_REGION(0xc00000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d84-12.rom", 0x000000, 0x200000, CRC(66a7a9aa) SHA1(a7d21f8b6370d16de3c1569019f2ad71d36e7a61) )
	ROM_LOAD16_BYTE("d84-13.rom", 0x000001, 0x200000, CRC(ae125516) SHA1(d54e76e398ab0b0fb82f3154ba54fc823ff49a1a) )
	ROM_LOAD16_BYTE("d84-16.rom", 0x400000, 0x100000, CRC(bcff9b2d) SHA1(0ca50ec809564eddf0ba7448a8fae9087d3b600b) )
	ROM_LOAD16_BYTE("d84-17.rom", 0x400001, 0x100000, CRC(0be37cc3) SHA1(b10c10b93858cad0c962ef614cfd6daea712ef6b) )
	ROM_LOAD       ("d84-14.rom", 0x900000, 0x200000, CRC(2b2e693e) SHA1(03eb37fa7dc68d54bf0f1800b8c0b581c344a40f) )
	ROM_LOAD       ("d84-18.rom", 0xb00000, 0x100000, CRC(e812bcc5) SHA1(3574e4a99232d9fc7989ec5d1e8fe76b4b30784a) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d84-26.rom", 0x100000, 0x40000, CRC(4f5b8563) SHA1(1d4e06cbea7bc73a99d6e30be714fff420151bbc) )
	ROM_LOAD16_BYTE("d84-27.rom", 0x100001, 0x40000, CRC(fb0cb1ba) SHA1(16a79b53651a6131f7636db19738b456b7c28bff) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d84-01.rom", 0x000000, 0x200000, CRC(9ad22149) SHA1(48055822e0cea228cdecf3d05ac24e50979b6f4d) )
	ROM_LOAD16_BYTE("d84-02.rom", 0xc00000, 0x200000, CRC(9e1827e4) SHA1(1840881b0f8f7b6225e6ffa12a8d4b463554988e) )
	ROM_LOAD16_BYTE("d84-15.rom", 0x800000, 0x100000, CRC(31ceb152) SHA1(d9d0bc631a6a726376f566a49605b50485ac7bf4) )
ROM_END

ROM_START( kaiserkj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d84-25.rom", 0x000000, 0x80000, CRC(2840893f) SHA1(079dece4667b029189622476cc618b88e57243a6) )
	ROM_LOAD32_BYTE("d84-24.rom", 0x000001, 0x80000, CRC(bf20c755) SHA1(9f6edfe9bb40051e8a93d06a391c993ed7288db6) )
	ROM_LOAD32_BYTE("d84-23.rom", 0x000002, 0x80000, CRC(39f12a9b) SHA1(4b3fe9b8b0abb46feacd11ffb6b505568f892483) )
	ROM_LOAD32_BYTE("d84-22.17",  0x000003, 0x80000, CRC(762f9056) SHA1(c39854d865210d05fe745493098ef5990327c56e) )

	ROM_REGION(0x1a00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d84-03.rom", 0x0000000, 0x200000, CRC(d786f552) SHA1(f73146892f714b5706d568fc8a135fddaa656570) )
  	ROM_LOAD16_BYTE("d84-04.rom", 0x0000001, 0x200000, CRC(d1f32b5d) SHA1(35289cce64fdbb8d966dd1d5307b5393be5e7799) )
	ROM_LOAD16_BYTE("d84-06.rom", 0x0400000, 0x200000, CRC(fa924dab) SHA1(28a8c3cd701f8df0c53069bb576bb2a820f3a331) )
  	ROM_LOAD16_BYTE("d84-07.rom", 0x0400001, 0x200000, CRC(54517a6b) SHA1(6e2c213c7ec1a3b78ad7e71db2326602557fd0f8) )
	ROM_LOAD16_BYTE("d84-09.rom", 0x0800000, 0x200000, CRC(faa78d98) SHA1(da3a2c5a45dd169743f113aa08e574f732e1f0fd) )
  	ROM_LOAD16_BYTE("d84-10.rom", 0x0800001, 0x200000, CRC(b84b7320) SHA1(f5de0d6da50d8ed753607b51e46bc9a4572ef431) )
	ROM_LOAD16_BYTE("d84-19.rom", 0x0c00000, 0x080000, CRC(6ddf77e5) SHA1(a1323acaed37fce62a19e63a0800d9d1dc2cfff7) )
  	ROM_LOAD16_BYTE("d84-20.rom", 0x0c00001, 0x080000, CRC(f85041e5) SHA1(6b2814514338f550d6aa14dbe39e848e8e64edee) )
	ROM_LOAD       ("d84-05.rom", 0x1380000, 0x200000, CRC(31a3c75d) SHA1(1a16ccb6a0a03ab715e5b016ab3b1b2cd0f1ae41) )
	ROM_LOAD       ("d84-08.rom", 0x1580000, 0x200000, CRC(07347bf1) SHA1(34bd359933acdec7fd1ce047092a30d1177afc2c) )
	ROM_LOAD       ("d84-11.rom", 0x1780000, 0x200000, CRC(a062c1d4) SHA1(158912aa3dd75c3961bf738f9ac9034f0b005b60) )
	ROM_LOAD       ("d84-21.rom", 0x1980000, 0x080000, CRC(89f68b66) SHA1(95916f02f71357324effe59da4f847f2f30ea34a) )
	ROM_FILL       (              0x0d00000, 0x680000, 0 )

	ROM_REGION(0xc00000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d84-12.rom", 0x000000, 0x200000, CRC(66a7a9aa) SHA1(a7d21f8b6370d16de3c1569019f2ad71d36e7a61) )
	ROM_LOAD16_BYTE("d84-13.rom", 0x000001, 0x200000, CRC(ae125516) SHA1(d54e76e398ab0b0fb82f3154ba54fc823ff49a1a) )
	ROM_LOAD16_BYTE("d84-16.rom", 0x400000, 0x100000, CRC(bcff9b2d) SHA1(0ca50ec809564eddf0ba7448a8fae9087d3b600b) )
	ROM_LOAD16_BYTE("d84-17.rom", 0x400001, 0x100000, CRC(0be37cc3) SHA1(b10c10b93858cad0c962ef614cfd6daea712ef6b) )
	ROM_LOAD       ("d84-14.rom", 0x900000, 0x200000, CRC(2b2e693e) SHA1(03eb37fa7dc68d54bf0f1800b8c0b581c344a40f) )
	ROM_LOAD       ("d84-18.rom", 0xb00000, 0x100000, CRC(e812bcc5) SHA1(3574e4a99232d9fc7989ec5d1e8fe76b4b30784a) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d84-26.rom", 0x100000, 0x40000, CRC(4f5b8563) SHA1(1d4e06cbea7bc73a99d6e30be714fff420151bbc) )
	ROM_LOAD16_BYTE("d84-27.rom", 0x100001, 0x40000, CRC(fb0cb1ba) SHA1(16a79b53651a6131f7636db19738b456b7c28bff) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d84-01.rom", 0x000000, 0x200000, CRC(9ad22149) SHA1(48055822e0cea228cdecf3d05ac24e50979b6f4d) )
	ROM_LOAD16_BYTE("d84-02.rom", 0xc00000, 0x200000, CRC(9e1827e4) SHA1(1840881b0f8f7b6225e6ffa12a8d4b463554988e) )
	ROM_LOAD16_BYTE("d84-15.rom", 0x800000, 0x100000, CRC(31ceb152) SHA1(d9d0bc631a6a726376f566a49605b50485ac7bf4) )
ROM_END

ROM_START( gblchmp )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d84-25.rom", 0x000000, 0x80000, CRC(2840893f) SHA1(079dece4667b029189622476cc618b88e57243a6) )
	ROM_LOAD32_BYTE("d84-24.rom", 0x000001, 0x80000, CRC(bf20c755) SHA1(9f6edfe9bb40051e8a93d06a391c993ed7288db6) )
	ROM_LOAD32_BYTE("d84-23.rom", 0x000002, 0x80000, CRC(39f12a9b) SHA1(4b3fe9b8b0abb46feacd11ffb6b505568f892483) )
	ROM_LOAD32_BYTE("d84-28.bin", 0x000003, 0x80000, CRC(ef26c1ec) SHA1(99440573704252b59148b3c30a006ce152b30ada) )

	ROM_REGION(0x1a00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d84-03.rom", 0x0000000, 0x200000, CRC(d786f552) SHA1(f73146892f714b5706d568fc8a135fddaa656570) )
  	ROM_LOAD16_BYTE("d84-04.rom", 0x0000001, 0x200000, CRC(d1f32b5d) SHA1(35289cce64fdbb8d966dd1d5307b5393be5e7799) )
	ROM_LOAD16_BYTE("d84-06.rom", 0x0400000, 0x200000, CRC(fa924dab) SHA1(28a8c3cd701f8df0c53069bb576bb2a820f3a331) )
  	ROM_LOAD16_BYTE("d84-07.rom", 0x0400001, 0x200000, CRC(54517a6b) SHA1(6e2c213c7ec1a3b78ad7e71db2326602557fd0f8) )
	ROM_LOAD16_BYTE("d84-09.rom", 0x0800000, 0x200000, CRC(faa78d98) SHA1(da3a2c5a45dd169743f113aa08e574f732e1f0fd) )
  	ROM_LOAD16_BYTE("d84-10.rom", 0x0800001, 0x200000, CRC(b84b7320) SHA1(f5de0d6da50d8ed753607b51e46bc9a4572ef431) )
	ROM_LOAD16_BYTE("d84-19.rom", 0x0c00000, 0x080000, CRC(6ddf77e5) SHA1(a1323acaed37fce62a19e63a0800d9d1dc2cfff7) )
  	ROM_LOAD16_BYTE("d84-20.rom", 0x0c00001, 0x080000, CRC(f85041e5) SHA1(6b2814514338f550d6aa14dbe39e848e8e64edee) )
	ROM_LOAD       ("d84-05.rom", 0x1380000, 0x200000, CRC(31a3c75d) SHA1(1a16ccb6a0a03ab715e5b016ab3b1b2cd0f1ae41) )
	ROM_LOAD       ("d84-08.rom", 0x1580000, 0x200000, CRC(07347bf1) SHA1(34bd359933acdec7fd1ce047092a30d1177afc2c) )
	ROM_LOAD       ("d84-11.rom", 0x1780000, 0x200000, CRC(a062c1d4) SHA1(158912aa3dd75c3961bf738f9ac9034f0b005b60) )
	ROM_LOAD       ("d84-21.rom", 0x1980000, 0x080000, CRC(89f68b66) SHA1(95916f02f71357324effe59da4f847f2f30ea34a) )
	ROM_FILL       (              0x0d00000, 0x680000, 0 )

	ROM_REGION(0xc00000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d84-12.rom", 0x000000, 0x200000, CRC(66a7a9aa) SHA1(a7d21f8b6370d16de3c1569019f2ad71d36e7a61) )
	ROM_LOAD16_BYTE("d84-13.rom", 0x000001, 0x200000, CRC(ae125516) SHA1(d54e76e398ab0b0fb82f3154ba54fc823ff49a1a) )
	ROM_LOAD16_BYTE("d84-16.rom", 0x400000, 0x100000, CRC(bcff9b2d) SHA1(0ca50ec809564eddf0ba7448a8fae9087d3b600b) )
	ROM_LOAD16_BYTE("d84-17.rom", 0x400001, 0x100000, CRC(0be37cc3) SHA1(b10c10b93858cad0c962ef614cfd6daea712ef6b) )
	ROM_LOAD       ("d84-14.rom", 0x900000, 0x200000, CRC(2b2e693e) SHA1(03eb37fa7dc68d54bf0f1800b8c0b581c344a40f) )
	ROM_LOAD       ("d84-18.rom", 0xb00000, 0x100000, CRC(e812bcc5) SHA1(3574e4a99232d9fc7989ec5d1e8fe76b4b30784a) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d84-26.rom", 0x100000, 0x40000, CRC(4f5b8563) SHA1(1d4e06cbea7bc73a99d6e30be714fff420151bbc) )
	ROM_LOAD16_BYTE("d84-27.rom", 0x100001, 0x40000, CRC(fb0cb1ba) SHA1(16a79b53651a6131f7636db19738b456b7c28bff) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d84-01.rom", 0x000000, 0x200000, CRC(9ad22149) SHA1(48055822e0cea228cdecf3d05ac24e50979b6f4d) )
	ROM_LOAD16_BYTE("d84-02.rom", 0xc00000, 0x200000, CRC(9e1827e4) SHA1(1840881b0f8f7b6225e6ffa12a8d4b463554988e) )
	ROM_LOAD16_BYTE("d84-15.rom", 0x800000, 0x100000, CRC(31ceb152) SHA1(d9d0bc631a6a726376f566a49605b50485ac7bf4) )
ROM_END

ROM_START( dankuga )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("dkg_mpr3.bin", 0x000000, 0x80000, CRC(ee1531ca) SHA1(5a78f44906c77a3195cbb41d256292255275643f) )
	ROM_LOAD32_BYTE("dkg_mpr2.bin", 0x000001, 0x80000, CRC(18a4748b) SHA1(31b912b532329d2cbd43df44f21e0923af7157d5) )
	ROM_LOAD32_BYTE("dkg_mpr1.bin", 0x000002, 0x80000, CRC(97566f69) SHA1(2f1ae6b9a463f20beea1558278741ddfe3901a6d) )
	ROM_LOAD32_BYTE("dkg_mpr0.bin", 0x000003, 0x80000, CRC(ad6ada07) SHA1(124db0cf8a5fbd99525633a2f783a0e1b281badf) )

	ROM_REGION(0x1a00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d84-03.rom", 0x0000000, 0x200000, CRC(d786f552) SHA1(f73146892f714b5706d568fc8a135fddaa656570) )
  	ROM_LOAD16_BYTE("d84-04.rom", 0x0000001, 0x200000, CRC(d1f32b5d) SHA1(35289cce64fdbb8d966dd1d5307b5393be5e7799) )
	ROM_LOAD16_BYTE("d84-06.rom", 0x0400000, 0x200000, CRC(fa924dab) SHA1(28a8c3cd701f8df0c53069bb576bb2a820f3a331) )
  	ROM_LOAD16_BYTE("d84-07.rom", 0x0400001, 0x200000, CRC(54517a6b) SHA1(6e2c213c7ec1a3b78ad7e71db2326602557fd0f8) )
	ROM_LOAD16_BYTE("d84-09.rom", 0x0800000, 0x200000, CRC(faa78d98) SHA1(da3a2c5a45dd169743f113aa08e574f732e1f0fd) )
  	ROM_LOAD16_BYTE("d84-10.rom", 0x0800001, 0x200000, CRC(b84b7320) SHA1(f5de0d6da50d8ed753607b51e46bc9a4572ef431) )
	ROM_LOAD16_BYTE("d84-19.rom", 0x0c00000, 0x080000, CRC(6ddf77e5) SHA1(a1323acaed37fce62a19e63a0800d9d1dc2cfff7) )
  	ROM_LOAD16_BYTE("d84-20.rom", 0x0c00001, 0x080000, CRC(f85041e5) SHA1(6b2814514338f550d6aa14dbe39e848e8e64edee) )
	ROM_LOAD       ("d84-05.rom", 0x1380000, 0x200000, CRC(31a3c75d) SHA1(1a16ccb6a0a03ab715e5b016ab3b1b2cd0f1ae41) )
	ROM_LOAD       ("d84-08.rom", 0x1580000, 0x200000, CRC(07347bf1) SHA1(34bd359933acdec7fd1ce047092a30d1177afc2c) )
	ROM_LOAD       ("d84-11.rom", 0x1780000, 0x200000, CRC(a062c1d4) SHA1(158912aa3dd75c3961bf738f9ac9034f0b005b60) )
	ROM_LOAD       ("d84-21.rom", 0x1980000, 0x080000, CRC(89f68b66) SHA1(95916f02f71357324effe59da4f847f2f30ea34a) )
	ROM_FILL       (              0x0d00000, 0x680000, 0 )

	ROM_REGION(0xc00000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d84-12.rom", 0x000000, 0x200000, CRC(66a7a9aa) SHA1(a7d21f8b6370d16de3c1569019f2ad71d36e7a61) )
	ROM_LOAD16_BYTE("d84-13.rom", 0x000001, 0x200000, CRC(ae125516) SHA1(d54e76e398ab0b0fb82f3154ba54fc823ff49a1a) )
	ROM_LOAD16_BYTE("d84-16.rom", 0x400000, 0x100000, CRC(bcff9b2d) SHA1(0ca50ec809564eddf0ba7448a8fae9087d3b600b) )
	ROM_LOAD16_BYTE("d84-17.rom", 0x400001, 0x100000, CRC(0be37cc3) SHA1(b10c10b93858cad0c962ef614cfd6daea712ef6b) )
	ROM_LOAD       ("d84-14.rom", 0x900000, 0x200000, CRC(2b2e693e) SHA1(03eb37fa7dc68d54bf0f1800b8c0b581c344a40f) )
	ROM_LOAD       ("d84-18.rom", 0xb00000, 0x100000, CRC(e812bcc5) SHA1(3574e4a99232d9fc7989ec5d1e8fe76b4b30784a) )
	ROM_FILL       (              0x600000, 0x300000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d84-26.rom", 0x100000, 0x40000, CRC(4f5b8563) SHA1(1d4e06cbea7bc73a99d6e30be714fff420151bbc) )
	ROM_LOAD16_BYTE("d84-27.rom", 0x100001, 0x40000, CRC(fb0cb1ba) SHA1(16a79b53651a6131f7636db19738b456b7c28bff) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d84-01.rom", 0x000000, 0x200000, CRC(9ad22149) SHA1(48055822e0cea228cdecf3d05ac24e50979b6f4d) )
	ROM_LOAD16_BYTE("d84-02.rom", 0xc00000, 0x200000, CRC(9e1827e4) SHA1(1840881b0f8f7b6225e6ffa12a8d4b463554988e) )
	ROM_LOAD16_BYTE("d84-15.rom", 0x800000, 0x100000, CRC(31ceb152) SHA1(d9d0bc631a6a726376f566a49605b50485ac7bf4) )
ROM_END

ROM_START( dariusg )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d87-12.bin", 0x000000, 0x80000, CRC(de78f328) SHA1(126464a826685f5bfab6cc099448ce4df207a407) )
	ROM_LOAD32_BYTE("d87-11.bin", 0x000001, 0x80000, CRC(f7bed18e) SHA1(db7d92680f9f406a5295ee85ce110c1a56ed386f) )
	ROM_LOAD32_BYTE("d87-10.bin", 0x000002, 0x80000, CRC(4149f66f) SHA1(57d36a62d490d9e53b6b80a92ea0e8c41d61799f) )
	ROM_LOAD32_BYTE("d87-wrld.bin", 0x000003, 0x80000, CRC(8f7e5901) SHA1(b920f43374af30e2f7d7d01049af6746206c8ece) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d87-03.bin", 0x000000, 0x200000, CRC(4be1666e) SHA1(35ba7bcf29ec7a8f8b6944ee3544693d4df1bfc2) )
	ROM_LOAD16_BYTE("d87-04.bin", 0x000001, 0x200000, CRC(2616002c) SHA1(003f98b740a697274385b8da03c78f3c6f7b5e89) )
	ROM_LOAD       ("d87-05.bin", 0x600000, 0x200000, CRC(4e5891a9) SHA1(fd08d848079841c9237fa359a850980fd00114d8) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d87-06.bin", 0x000000, 0x200000, CRC(3b97a07c) SHA1(72cdeffedeab0c1bd0e47f03172085390a2be393) )
	ROM_LOAD16_BYTE("d87-17.bin", 0x000001, 0x200000, CRC(e601d63e) SHA1(256a6aeb5633fe1db407fad567169a9d0c911219) )
	ROM_LOAD       ("d87-08.bin", 0x600000, 0x200000, CRC(76d23602) SHA1(ca53ea6641182c44a4038bbeaa5effb1687f1980) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d87-13.bin", 0x100000, 0x40000, CRC(15b1fff4) SHA1(28692b731ae98a47c2c5e11a8a71b61a813d9a64) )
	ROM_LOAD16_BYTE("d87-14.bin", 0x100001, 0x40000, CRC(eecda29a) SHA1(6eb238e47bc7bf635ffbdbb25fb06a37db980ef8) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d87-01.bin", 0x000000, 0x200000, CRC(3848a110) SHA1(802e91695a526f665c7fd261f0a7639a0b883c9e) )
	ROM_LOAD16_BYTE("d87-02.bin", 0xc00000, 0x200000, CRC(9250abae) SHA1(07cae8edbc3cca0a95022d9b40a5c18a55350b67) )
ROM_END

ROM_START( dariusgj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d87-12.bin", 0x000000, 0x80000, CRC(de78f328) SHA1(126464a826685f5bfab6cc099448ce4df207a407) )
	ROM_LOAD32_BYTE("d87-11.bin", 0x000001, 0x80000, CRC(f7bed18e) SHA1(db7d92680f9f406a5295ee85ce110c1a56ed386f) )
	ROM_LOAD32_BYTE("d87-10.bin", 0x000002, 0x80000, CRC(4149f66f) SHA1(57d36a62d490d9e53b6b80a92ea0e8c41d61799f) )
	ROM_LOAD32_BYTE("d87-09.bin", 0x000003, 0x80000, CRC(6170382d) SHA1(85b0f9a3400884e1c073d5bdcdf7318377650eed) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d87-03.bin", 0x000000, 0x200000, CRC(4be1666e) SHA1(35ba7bcf29ec7a8f8b6944ee3544693d4df1bfc2) )
	ROM_LOAD16_BYTE("d87-04.bin", 0x000001, 0x200000, CRC(2616002c) SHA1(003f98b740a697274385b8da03c78f3c6f7b5e89) )
	ROM_LOAD       ("d87-05.bin", 0x600000, 0x200000, CRC(4e5891a9) SHA1(fd08d848079841c9237fa359a850980fd00114d8) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d87-06.bin", 0x000000, 0x200000, CRC(3b97a07c) SHA1(72cdeffedeab0c1bd0e47f03172085390a2be393) )
	ROM_LOAD16_BYTE("d87-17.bin", 0x000001, 0x200000, CRC(e601d63e) SHA1(256a6aeb5633fe1db407fad567169a9d0c911219) )
	ROM_LOAD       ("d87-08.bin", 0x600000, 0x200000, CRC(76d23602) SHA1(ca53ea6641182c44a4038bbeaa5effb1687f1980) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d87-13.bin", 0x100000, 0x40000, CRC(15b1fff4) SHA1(28692b731ae98a47c2c5e11a8a71b61a813d9a64) )
	ROM_LOAD16_BYTE("d87-14.bin", 0x100001, 0x40000, CRC(eecda29a) SHA1(6eb238e47bc7bf635ffbdbb25fb06a37db980ef8) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d87-01.bin", 0x000000, 0x200000, CRC(3848a110) SHA1(802e91695a526f665c7fd261f0a7639a0b883c9e) )
	ROM_LOAD16_BYTE("d87-02.bin", 0xc00000, 0x200000, CRC(9250abae) SHA1(07cae8edbc3cca0a95022d9b40a5c18a55350b67) )
ROM_END

ROM_START( dariusgu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d87-12.bin", 0x000000, 0x80000, CRC(de78f328) SHA1(126464a826685f5bfab6cc099448ce4df207a407) )
	ROM_LOAD32_BYTE("d87-11.bin", 0x000001, 0x80000, CRC(f7bed18e) SHA1(db7d92680f9f406a5295ee85ce110c1a56ed386f) )
	ROM_LOAD32_BYTE("d87-10.bin", 0x000002, 0x80000, CRC(4149f66f) SHA1(57d36a62d490d9e53b6b80a92ea0e8c41d61799f) )
	ROM_LOAD32_BYTE("d87-usa.bin", 0x000003, 0x80000, CRC(f8796997) SHA1(fa286561bac9894cb260944ffa14d0059b882ab9) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d87-03.bin", 0x000000, 0x200000, CRC(4be1666e) SHA1(35ba7bcf29ec7a8f8b6944ee3544693d4df1bfc2) )
	ROM_LOAD16_BYTE("d87-04.bin", 0x000001, 0x200000, CRC(2616002c) SHA1(003f98b740a697274385b8da03c78f3c6f7b5e89) )
	ROM_LOAD       ("d87-05.bin", 0x600000, 0x200000, CRC(4e5891a9) SHA1(fd08d848079841c9237fa359a850980fd00114d8) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d87-06.bin", 0x000000, 0x200000, CRC(3b97a07c) SHA1(72cdeffedeab0c1bd0e47f03172085390a2be393) )
	ROM_LOAD16_BYTE("d87-17.bin", 0x000001, 0x200000, CRC(e601d63e) SHA1(256a6aeb5633fe1db407fad567169a9d0c911219) )
	ROM_LOAD       ("d87-08.bin", 0x600000, 0x200000, CRC(76d23602) SHA1(ca53ea6641182c44a4038bbeaa5effb1687f1980) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d87-13.bin", 0x100000, 0x40000, CRC(15b1fff4) SHA1(28692b731ae98a47c2c5e11a8a71b61a813d9a64) )
	ROM_LOAD16_BYTE("d87-14.bin", 0x100001, 0x40000, CRC(eecda29a) SHA1(6eb238e47bc7bf635ffbdbb25fb06a37db980ef8) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d87-01.bin", 0x000000, 0x200000, CRC(3848a110) SHA1(802e91695a526f665c7fd261f0a7639a0b883c9e) )
	ROM_LOAD16_BYTE("d87-02.bin", 0xc00000, 0x200000, CRC(9250abae) SHA1(07cae8edbc3cca0a95022d9b40a5c18a55350b67) )
ROM_END

ROM_START( dariusgx )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("dge_mpr3.bin", 0x000000, 0x80000, CRC(1c1e24a7) SHA1(eafde331c3be5be55d0d838a84017f357ff92634) )
	ROM_LOAD32_BYTE("dge_mpr2.bin", 0x000001, 0x80000, CRC(7be23e23) SHA1(4764355f51e207f4538dd753aea59bf2689835de) )
	ROM_LOAD32_BYTE("dge_mpr1.bin", 0x000002, 0x80000, CRC(bc030f6f) SHA1(841396911d26ddfae0c9863431e02e0b5e762ac6) )
	ROM_LOAD32_BYTE("dge_mpr0.bin", 0x000003, 0x80000, CRC(c5bd135c) SHA1(402e26a05f1c3162fa3a8d3fcb81ef334b733699) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d87-03.bin", 0x000000, 0x200000, CRC(4be1666e) SHA1(35ba7bcf29ec7a8f8b6944ee3544693d4df1bfc2) )
	ROM_LOAD16_BYTE("d87-04.bin", 0x000001, 0x200000, CRC(2616002c) SHA1(003f98b740a697274385b8da03c78f3c6f7b5e89) )
	ROM_LOAD       ("d87-05.bin", 0x600000, 0x200000, CRC(4e5891a9) SHA1(fd08d848079841c9237fa359a850980fd00114d8) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d87-06.bin", 0x000000, 0x200000, CRC(3b97a07c) SHA1(72cdeffedeab0c1bd0e47f03172085390a2be393) )
	ROM_LOAD16_BYTE("d87-17.bin", 0x000001, 0x200000, CRC(e601d63e) SHA1(256a6aeb5633fe1db407fad567169a9d0c911219) )
	ROM_LOAD       ("d87-08.bin", 0x600000, 0x200000, CRC(76d23602) SHA1(ca53ea6641182c44a4038bbeaa5effb1687f1980) )
	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d87-13.bin", 0x100000, 0x40000, CRC(15b1fff4) SHA1(28692b731ae98a47c2c5e11a8a71b61a813d9a64) )
	ROM_LOAD16_BYTE("d87-14.bin", 0x100001, 0x40000, CRC(eecda29a) SHA1(6eb238e47bc7bf635ffbdbb25fb06a37db980ef8) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d87-01.bin", 0x000000, 0x200000, CRC(3848a110) SHA1(802e91695a526f665c7fd261f0a7639a0b883c9e) )
	ROM_LOAD16_BYTE("d87-02.bin", 0xc00000, 0x200000, CRC(9250abae) SHA1(07cae8edbc3cca0a95022d9b40a5c18a55350b67) )
ROM_END

ROM_START( bublbob2 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d90.12", 0x000000, 0x40000, CRC(9e523996) SHA1(c49a426f9865f96e8021c8ed9a6ac094c5e586b1) )
	ROM_LOAD32_BYTE("d90.11", 0x000001, 0x40000, CRC(edfdbb7f) SHA1(698ad631d5b13661645f2c5ccd3e4fbf0248053c) )
	ROM_LOAD32_BYTE("d90.10", 0x000002, 0x40000, CRC(8e957d3d) SHA1(5db31e5788483b802592e1092bf98df51ff4b70e) )
	ROM_LOAD32_BYTE("d90.17", 0x000003, 0x40000, CRC(711f1894) SHA1(8e574d9a63593fbe0c87840e79a2e2dbfc227671) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d90.03", 0x000000, 0x100000, CRC(6fa894a1) SHA1(7c33e6d41e8928029b92d66557a3712b51c49c67) )
	ROM_LOAD16_BYTE("d90.02", 0x000001, 0x100000, CRC(5ab04ca2) SHA1(6d87e7ca3167ff81a041cfedbbed84d51da997de) )
	ROM_LOAD       ("d90.01", 0x300000, 0x100000, CRC(8aedb9e5) SHA1(fb49330f7985a829c9544ecfd0bc672494f29cf6) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d90.08", 0x000000, 0x100000, CRC(25a4fb2c) SHA1(c8bf6fe2291c05386b32cd26bfcb379da756d7b5) )
	ROM_LOAD16_BYTE("d90.07", 0x000001, 0x100000, CRC(b436b42d) SHA1(559827120273733147b260e0723054d926dbea5e) )
	ROM_LOAD       ("d90.06", 0x300000, 0x100000, CRC(166a72b8) SHA1(7f70b8c960794322e1dc88e6600a2d13d948d873) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d90.13", 0x100000, 0x40000, CRC(6762bd90) SHA1(771db0382bc8dab2caf13d0fc20648366c685829) )
	ROM_LOAD16_BYTE("d90.14", 0x100001, 0x40000, CRC(8e33357e) SHA1(68b81693c22e6357e37244f2a416818a81338138) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00  )
	ROM_LOAD16_BYTE("d90.04", 0x000000, 0x200000, CRC(feee5fda) SHA1(b89354013ec4d34bcd51ecded412effa66dd2f2f) )
 	ROM_LOAD16_BYTE("d90.05", 0xc00000, 0x200000, CRC(c192331f) SHA1(ebab05b3681c70b373bc06c1826be1cc397d3af7) )
ROM_END

ROM_START( bubsympe )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d90.12", 0x000000, 0x40000, CRC(9e523996) SHA1(c49a426f9865f96e8021c8ed9a6ac094c5e586b1) )
	ROM_LOAD32_BYTE("d90.11", 0x000001, 0x40000, CRC(edfdbb7f) SHA1(698ad631d5b13661645f2c5ccd3e4fbf0248053c) )
	ROM_LOAD32_BYTE("d90.10", 0x000002, 0x40000, CRC(8e957d3d) SHA1(5db31e5788483b802592e1092bf98df51ff4b70e) )
	ROM_LOAD32_BYTE("d90.16", 0x000003, 0x40000, CRC(d12ef19b) SHA1(8715102b54c730c809b3964a80cd1aed863ba334) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d90.03", 0x000000, 0x100000, CRC(6fa894a1) SHA1(7c33e6d41e8928029b92d66557a3712b51c49c67) )
	ROM_LOAD16_BYTE("d90.02", 0x000001, 0x100000, CRC(5ab04ca2) SHA1(6d87e7ca3167ff81a041cfedbbed84d51da997de) )
	ROM_LOAD       ("d90.01", 0x300000, 0x100000, CRC(8aedb9e5) SHA1(fb49330f7985a829c9544ecfd0bc672494f29cf6) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d90.08", 0x000000, 0x100000, CRC(25a4fb2c) SHA1(c8bf6fe2291c05386b32cd26bfcb379da756d7b5) )
	ROM_LOAD16_BYTE("d90.07", 0x000001, 0x100000, CRC(b436b42d) SHA1(559827120273733147b260e0723054d926dbea5e) )
	ROM_LOAD       ("d90.06", 0x300000, 0x100000, CRC(166a72b8) SHA1(7f70b8c960794322e1dc88e6600a2d13d948d873) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d90.13", 0x100000, 0x40000, CRC(6762bd90) SHA1(771db0382bc8dab2caf13d0fc20648366c685829) )
	ROM_LOAD16_BYTE("d90.14", 0x100001, 0x40000, CRC(8e33357e) SHA1(68b81693c22e6357e37244f2a416818a81338138) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00  )
	ROM_LOAD16_BYTE("d90.04", 0x000000, 0x200000, CRC(feee5fda) SHA1(b89354013ec4d34bcd51ecded412effa66dd2f2f) )
 	ROM_LOAD16_BYTE("d90.05", 0xc00000, 0x200000, CRC(c192331f) SHA1(ebab05b3681c70b373bc06c1826be1cc397d3af7) )
ROM_END

ROM_START( bubsymph )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d90.12", 0x000000, 0x40000, CRC(9e523996) SHA1(c49a426f9865f96e8021c8ed9a6ac094c5e586b1) )
	ROM_LOAD32_BYTE("d90.11", 0x000001, 0x40000, CRC(edfdbb7f) SHA1(698ad631d5b13661645f2c5ccd3e4fbf0248053c) )
	ROM_LOAD32_BYTE("d90.10", 0x000002, 0x40000, CRC(8e957d3d) SHA1(5db31e5788483b802592e1092bf98df51ff4b70e) )
	ROM_LOAD32_BYTE("d90.09", 0x000003, 0x40000, CRC(3f2090b7) SHA1(2a95c8c8dc23b618c0ce65497391d464494f4d6a) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d90.03", 0x000000, 0x100000, CRC(6fa894a1) SHA1(7c33e6d41e8928029b92d66557a3712b51c49c67) )
	ROM_LOAD16_BYTE("d90.02", 0x000001, 0x100000, CRC(5ab04ca2) SHA1(6d87e7ca3167ff81a041cfedbbed84d51da997de) )
	ROM_LOAD       ("d90.01", 0x300000, 0x100000, CRC(8aedb9e5) SHA1(fb49330f7985a829c9544ecfd0bc672494f29cf6) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d90.08", 0x000000, 0x100000, CRC(25a4fb2c) SHA1(c8bf6fe2291c05386b32cd26bfcb379da756d7b5) )
	ROM_LOAD16_BYTE("d90.07", 0x000001, 0x100000, CRC(b436b42d) SHA1(559827120273733147b260e0723054d926dbea5e) )
	ROM_LOAD       ("d90.06", 0x300000, 0x100000, CRC(166a72b8) SHA1(7f70b8c960794322e1dc88e6600a2d13d948d873) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d90.13", 0x100000, 0x40000, CRC(6762bd90) SHA1(771db0382bc8dab2caf13d0fc20648366c685829) )
	ROM_LOAD16_BYTE("d90.14", 0x100001, 0x40000, CRC(8e33357e) SHA1(68b81693c22e6357e37244f2a416818a81338138) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00  )
	ROM_LOAD16_BYTE("d90.04", 0x000000, 0x200000, CRC(feee5fda) SHA1(b89354013ec4d34bcd51ecded412effa66dd2f2f) )
 	ROM_LOAD16_BYTE("d90.05", 0xc00000, 0x200000, CRC(c192331f) SHA1(ebab05b3681c70b373bc06c1826be1cc397d3af7) )
ROM_END

ROM_START( bubsympu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d90.12", 0x000000, 0x40000, CRC(9e523996) SHA1(c49a426f9865f96e8021c8ed9a6ac094c5e586b1) )
	ROM_LOAD32_BYTE("d90.11", 0x000001, 0x40000, CRC(edfdbb7f) SHA1(698ad631d5b13661645f2c5ccd3e4fbf0248053c) )
	ROM_LOAD32_BYTE("d90.10", 0x000002, 0x40000, CRC(8e957d3d) SHA1(5db31e5788483b802592e1092bf98df51ff4b70e) )
	ROM_LOAD32_BYTE("d90.usa",0x000003, 0x40000, CRC(06182802) SHA1(c068ea8e8852033d0cf7bd4bca4b0411b7aebded) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d90.03", 0x000000, 0x100000, CRC(6fa894a1) SHA1(7c33e6d41e8928029b92d66557a3712b51c49c67) )
	ROM_LOAD16_BYTE("d90.02", 0x000001, 0x100000, CRC(5ab04ca2) SHA1(6d87e7ca3167ff81a041cfedbbed84d51da997de) )
	ROM_LOAD       ("d90.01", 0x300000, 0x100000, CRC(8aedb9e5) SHA1(fb49330f7985a829c9544ecfd0bc672494f29cf6) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d90.08", 0x000000, 0x100000, CRC(25a4fb2c) SHA1(c8bf6fe2291c05386b32cd26bfcb379da756d7b5) )
	ROM_LOAD16_BYTE("d90.07", 0x000001, 0x100000, CRC(b436b42d) SHA1(559827120273733147b260e0723054d926dbea5e) )
	ROM_LOAD       ("d90.06", 0x300000, 0x100000, CRC(166a72b8) SHA1(7f70b8c960794322e1dc88e6600a2d13d948d873) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("d90.13", 0x100000, 0x40000, CRC(6762bd90) SHA1(771db0382bc8dab2caf13d0fc20648366c685829) )
	ROM_LOAD16_BYTE("d90.14", 0x100001, 0x40000, CRC(8e33357e) SHA1(68b81693c22e6357e37244f2a416818a81338138) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00  )
	ROM_LOAD16_BYTE("d90.04", 0x000000, 0x200000, CRC(feee5fda) SHA1(b89354013ec4d34bcd51ecded412effa66dd2f2f) )
 	ROM_LOAD16_BYTE("d90.05", 0xc00000, 0x200000, CRC(c192331f) SHA1(ebab05b3681c70b373bc06c1826be1cc397d3af7) )
ROM_END

ROM_START( spcinvdj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d93_04.bin", 0x000000, 0x20000, CRC(cd9a4e5c) SHA1(b163b8274570610af8697b1b38116dcf4c3593db) )
	ROM_LOAD32_BYTE("d93_03.bin", 0x000001, 0x20000, CRC(0174bfc1) SHA1(133452f6a5bdf01b1b436077288a597734a8731a) )
	ROM_LOAD32_BYTE("d93_02.bin", 0x000002, 0x20000, CRC(01922b31) SHA1(660c9c20e76a5f4094f1bfee9d75146f0829daeb) )
	ROM_LOAD32_BYTE("d93_01.bin", 0x000003, 0x20000, CRC(4a74ab1c) SHA1(5f7ae70d8fa3f141239ed3de3a45c50e2d824864) )

	ROM_REGION(0x200000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d93-07.12", 0x000000, 0x80000, CRC(8cf5b972) SHA1(75e383aed8548f4ac7d38f1f08bf33fae2a93064) )
	ROM_LOAD16_BYTE("d93-08.08", 0x000001, 0x80000, CRC(4c11af2b) SHA1(e332372ab0d1322faa8d6d98f8a6e3bbf51d2008) )
	ROM_FILL       (             0x100000, 0x100000,0 )

	ROM_REGION(0x80000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d93-09.47", 0x000000, 0x20000, CRC(9076f663) SHA1(c94e93e40926df33b6bb528e0ef30381631913d7) )
	ROM_LOAD16_BYTE("d93-10.45", 0x000001, 0x20000, CRC(8a3f531b) SHA1(69f9971c45971018108a5d312d5bbcfd3caf9bd0) )
	ROM_FILL       (             0x040000, 0x40000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d93_05.bin", 0x100000, 0x20000, CRC(ff365596) SHA1(4cf2e0d6f42cf3fb69796be6092eff8a47f7f8b9) )
	ROM_LOAD16_BYTE("d93_06.bin", 0x100001, 0x20000, CRC(ef7ad400) SHA1(01be403d575a543f089b910a5a8c381a6603e67e) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d93-11.38", 0x000000, 0x80000, CRC(df5853de) SHA1(bb1ea604d44819dc7c82848c5bde9612f70f7528) )
	ROM_LOAD16_BYTE("d93-12.39", 0xe00000, 0x80000, CRC(b0f71d60) SHA1(35fc32764d9b82b1b40c5e9cc8e367cf842531a2) )
ROM_END

ROM_START( pwrgoal )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d94-18.bin", 0x000000, 0x40000, CRC(b92681c3) SHA1(0ca05a69d046668c878df3d2b7ae3172d748e290) )
	ROM_LOAD32_BYTE("d94-17.bin", 0x000001, 0x40000, CRC(6009333e) SHA1(4ab28f2d9e2b75adc668f5d9390e06086bbd97dc) )
	ROM_LOAD32_BYTE("d94-16.bin", 0x000002, 0x40000, CRC(c6dbc9c8) SHA1(4f096b59734db51eeddcf0649f2a6f11bdde9590) )
	ROM_LOAD32_BYTE("d94-22.rom", 0x000003, 0x40000, CRC(f672e487) SHA1(da62afc82aeae4aeeebbee0965cda3d84464ad09) )

	ROM_REGION(0x1800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d94-09.bin", 0x000000, 0x200000, CRC(425e6bec) SHA1(512508e7137fcdebdf2240dbbd37ea0cf1c4dcdc) )
	ROM_LOAD16_BYTE("d94-08.bin", 0x400000, 0x200000, CRC(bd909caf) SHA1(33952883afb8fe9b55dd258435af99881925e8d5) )
	ROM_LOAD16_BYTE("d94-07.bin", 0x800000, 0x200000, CRC(c8c95e49) SHA1(9bfdf63d6059b01a4cd5813239ba1bd98453a56b) )
	ROM_LOAD16_BYTE("d94-06.bin", 0x000001, 0x200000, CRC(0ed1df55) SHA1(10b22407ad0e03c37363783ee80f2cbf98a802a0) )
	ROM_LOAD16_BYTE("d94-05.bin", 0x400001, 0x200000, CRC(121c8542) SHA1(ec9b7e56c97a8b6ed0423f05b789ca89b1bb0d36) )
	ROM_LOAD16_BYTE("d94-04.bin", 0x800001, 0x200000, CRC(24958b50) SHA1(ea15ffa3a615e3e67c1bade6f6ef45424479115e) )
	ROM_LOAD       ("d94-03.bin", 0x1200000, 0x200000, CRC(95e32072) SHA1(9797f65ecadc6b0f209bf262396315b61855c433) )
	ROM_LOAD       ("d94-02.bin", 0x1400000, 0x200000, CRC(f460b9ac) SHA1(e36a812791bd0360380f397b1bc6c357391f585a) )
	ROM_LOAD       ("d94-01.bin", 0x1600000, 0x200000, CRC(410ffccd) SHA1(0cab00c8e9de92ad81ac61f25bbe8bfd60f45ae0) )
	ROM_FILL       (              0xc00000, 0x600000,0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d94-14.bin", 0x000000, 0x100000, CRC(b8ba5761) SHA1(7966ef3166d7d6b9913478eaef5dd4a2bf7d5a06) )
	ROM_LOAD16_BYTE("d94-13.bin", 0x000001, 0x100000, CRC(cafc68ce) SHA1(5c1f49951e83d812f0c7697751f4876ab1d08141) )
	ROM_LOAD       ("d94-12.bin", 0x300000, 0x100000, CRC(47064189) SHA1(99ceeb326dcc2e1c3acba8ac14d94dcb17c6e032) )
	ROM_FILL       (              0x200000, 0x100000,0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d94-19.bin", 0x100000, 0x40000, CRC(c93dbcf4) SHA1(413520e652d809651aff9b1b74e6353112d34c12) )
	ROM_LOAD16_BYTE("d94-20.bin", 0x100001, 0x40000, CRC(f232bf64) SHA1(bbfeae0785fc49c12aa6d9b1bd6ff7c8515f8fe7) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d94-10.bin", 0x000000, 0x200000, CRC(a22563ae) SHA1(85f2a4ca5e085ac1d4c15feb737229764697ae85) )
	ROM_LOAD16_BYTE("d94-11.bin", 0xc00000, 0x200000, CRC(61ed83fa) SHA1(f6ca60b7af61fd3ac01a987f949d7a7bc96e43ff) )
ROM_END

ROM_START( hthero95 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d94-18.bin", 0x000000, 0x40000, CRC(b92681c3) SHA1(0ca05a69d046668c878df3d2b7ae3172d748e290) )
	ROM_LOAD32_BYTE("d94-17.bin", 0x000001, 0x40000, CRC(6009333e) SHA1(4ab28f2d9e2b75adc668f5d9390e06086bbd97dc) )
	ROM_LOAD32_BYTE("d94-16.bin", 0x000002, 0x40000, CRC(c6dbc9c8) SHA1(4f096b59734db51eeddcf0649f2a6f11bdde9590) )
	ROM_LOAD32_BYTE("d94-15.bin", 0x000003, 0x40000, CRC(187c85ab) SHA1(8270930b95fafe5ad92ea978c1558c491d9668b0) )

	ROM_REGION(0x1800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d94-09.bin", 0x000000, 0x200000, CRC(425e6bec) SHA1(512508e7137fcdebdf2240dbbd37ea0cf1c4dcdc) )
	ROM_LOAD16_BYTE("d94-08.bin", 0x400000, 0x200000, CRC(bd909caf) SHA1(33952883afb8fe9b55dd258435af99881925e8d5) )
	ROM_LOAD16_BYTE("d94-07.bin", 0x800000, 0x200000, CRC(c8c95e49) SHA1(9bfdf63d6059b01a4cd5813239ba1bd98453a56b) )
	ROM_LOAD16_BYTE("d94-06.bin", 0x000001, 0x200000, CRC(0ed1df55) SHA1(10b22407ad0e03c37363783ee80f2cbf98a802a0) )
	ROM_LOAD16_BYTE("d94-05.bin", 0x400001, 0x200000, CRC(121c8542) SHA1(ec9b7e56c97a8b6ed0423f05b789ca89b1bb0d36) )
	ROM_LOAD16_BYTE("d94-04.bin", 0x800001, 0x200000, CRC(24958b50) SHA1(ea15ffa3a615e3e67c1bade6f6ef45424479115e) )
	ROM_LOAD       ("d94-03.bin", 0x1200000, 0x200000, CRC(95e32072) SHA1(9797f65ecadc6b0f209bf262396315b61855c433) )
	ROM_LOAD       ("d94-02.bin", 0x1400000, 0x200000, CRC(f460b9ac) SHA1(e36a812791bd0360380f397b1bc6c357391f585a) )
	ROM_LOAD       ("d94-01.bin", 0x1600000, 0x200000, CRC(410ffccd) SHA1(0cab00c8e9de92ad81ac61f25bbe8bfd60f45ae0) )
	ROM_FILL       (              0xc00000, 0x600000,0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d94-14.bin", 0x000000, 0x100000, CRC(b8ba5761) SHA1(7966ef3166d7d6b9913478eaef5dd4a2bf7d5a06) )
	ROM_LOAD16_BYTE("d94-13.bin", 0x000001, 0x100000, CRC(cafc68ce) SHA1(5c1f49951e83d812f0c7697751f4876ab1d08141) )
	ROM_LOAD       ("d94-12.bin", 0x300000, 0x100000, CRC(47064189) SHA1(99ceeb326dcc2e1c3acba8ac14d94dcb17c6e032) )
	ROM_FILL       (              0x200000, 0x100000,0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d94-19.bin", 0x100000, 0x40000, CRC(c93dbcf4) SHA1(413520e652d809651aff9b1b74e6353112d34c12) )
	ROM_LOAD16_BYTE("d94-20.bin", 0x100001, 0x40000, CRC(f232bf64) SHA1(bbfeae0785fc49c12aa6d9b1bd6ff7c8515f8fe7) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d94-10.bin", 0x000000, 0x200000, CRC(a22563ae) SHA1(85f2a4ca5e085ac1d4c15feb737229764697ae85) )
	ROM_LOAD16_BYTE("d94-11.bin", 0xc00000, 0x200000, CRC(61ed83fa) SHA1(f6ca60b7af61fd3ac01a987f949d7a7bc96e43ff) )
ROM_END

ROM_START( hthro95u )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("d94-18.bin", 0x000000, 0x40000, CRC(b92681c3) SHA1(0ca05a69d046668c878df3d2b7ae3172d748e290) )
	ROM_LOAD32_BYTE("d94-17.bin", 0x000001, 0x40000, CRC(6009333e) SHA1(4ab28f2d9e2b75adc668f5d9390e06086bbd97dc) )
	ROM_LOAD32_BYTE("d94-16.bin", 0x000002, 0x40000, CRC(c6dbc9c8) SHA1(4f096b59734db51eeddcf0649f2a6f11bdde9590) )
	ROM_LOAD32_BYTE("d94-21.bin", 0x000003, 0x40000, CRC(8175d411) SHA1(b93ffef510ecfaced6cae07ea6cd549af7473049) )

	ROM_REGION(0x1800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d94-09.bin", 0x000000, 0x200000, CRC(425e6bec) SHA1(512508e7137fcdebdf2240dbbd37ea0cf1c4dcdc) )
	ROM_LOAD16_BYTE("d94-08.bin", 0x400000, 0x200000, CRC(bd909caf) SHA1(33952883afb8fe9b55dd258435af99881925e8d5) )
	ROM_LOAD16_BYTE("d94-07.bin", 0x800000, 0x200000, CRC(c8c95e49) SHA1(9bfdf63d6059b01a4cd5813239ba1bd98453a56b) )
	ROM_LOAD16_BYTE("d94-06.bin", 0x000001, 0x200000, CRC(0ed1df55) SHA1(10b22407ad0e03c37363783ee80f2cbf98a802a0) )
	ROM_LOAD16_BYTE("d94-05.bin", 0x400001, 0x200000, CRC(121c8542) SHA1(ec9b7e56c97a8b6ed0423f05b789ca89b1bb0d36) )
	ROM_LOAD16_BYTE("d94-04.bin", 0x800001, 0x200000, CRC(24958b50) SHA1(ea15ffa3a615e3e67c1bade6f6ef45424479115e) )
	ROM_LOAD       ("d94-03.bin", 0x1200000, 0x200000, CRC(95e32072) SHA1(9797f65ecadc6b0f209bf262396315b61855c433) )
	ROM_LOAD       ("d94-02.bin", 0x1400000, 0x200000, CRC(f460b9ac) SHA1(e36a812791bd0360380f397b1bc6c357391f585a) )
	ROM_LOAD       ("d94-01.bin", 0x1600000, 0x200000, CRC(410ffccd) SHA1(0cab00c8e9de92ad81ac61f25bbe8bfd60f45ae0) )
	ROM_FILL       (              0xc00000, 0x600000,0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d94-14.bin", 0x000000, 0x100000, CRC(b8ba5761) SHA1(7966ef3166d7d6b9913478eaef5dd4a2bf7d5a06) )
	ROM_LOAD16_BYTE("d94-13.bin", 0x000001, 0x100000, CRC(cafc68ce) SHA1(5c1f49951e83d812f0c7697751f4876ab1d08141) )
	ROM_LOAD       ("d94-12.bin", 0x300000, 0x100000, CRC(47064189) SHA1(99ceeb326dcc2e1c3acba8ac14d94dcb17c6e032) )
	ROM_FILL       (              0x200000, 0x100000,0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d94-19.bin", 0x100000, 0x40000, CRC(c93dbcf4) SHA1(413520e652d809651aff9b1b74e6353112d34c12) )
	ROM_LOAD16_BYTE("d94-20.bin", 0x100001, 0x40000, CRC(f232bf64) SHA1(bbfeae0785fc49c12aa6d9b1bd6ff7c8515f8fe7) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d94-10.bin", 0x000000, 0x200000, CRC(a22563ae) SHA1(85f2a4ca5e085ac1d4c15feb737229764697ae85) )
	ROM_LOAD16_BYTE("d94-11.bin", 0xc00000, 0x200000, CRC(61ed83fa) SHA1(f6ca60b7af61fd3ac01a987f949d7a7bc96e43ff) )
ROM_END

ROM_START( qtheater )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
	ROM_LOAD32_BYTE("d95-12.20", 0x000000, 0x80000, CRC(fcee76ee) SHA1(9ffeeda656368d1c065ba1cdb51a8665f7f7262a) )
	ROM_LOAD32_BYTE("d95-11.19", 0x000001, 0x80000, CRC(b3c2b8d5) SHA1(536c13d71e858309f41e7c387cd988e8fe356bee) )
	ROM_LOAD32_BYTE("d95-10.18", 0x000002, 0x80000, CRC(85236e40) SHA1(727c8f7361d7e0af3239bb0c0e7778ab30b12739) )
	ROM_LOAD32_BYTE("d95-09.17", 0x000003, 0x80000, CRC(f456519c) SHA1(9226d33d8d16a7d1054c1183ac013fc5caf229e2) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("d95-02.12", 0x000000, 0x200000, CRC(74ce6f3e) SHA1(eb03f44889bd2d5705e9d8cda6516b39758d9554) )
  	ROM_LOAD16_BYTE("d95-01.8",  0x000001, 0x200000, CRC(141beb7d) SHA1(bba91f47f68367e2bb3d89298cb62fac2d4edf7b) )
	ROM_FILL       (             0x400000, 0x400000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("d95-06.47", 0x000000, 0x200000, CRC(70a0dcbb) SHA1(b1abe6a9a4afe55201229a62bae11ad1d96ca244) )
	ROM_LOAD16_BYTE("d95-05.45", 0x000001, 0x200000, CRC(1a1a852b) SHA1(89827485a31af4e2457775a5d16f747a764b6d67) )
	ROM_FILL       (             0x400000, 0x400000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("d95-07.32", 0x100000, 0x40000, CRC(d82cdfe2) SHA1(e0d2e4083ab1d2d6cc9a86e5de0d8fc9601448bf) )
	ROM_LOAD16_BYTE("d95-08.33", 0x100001, 0x40000, CRC(01c23354) SHA1(7b332edc844b1b1c1513e879215089987645fa3f) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("d95-03.38", 0x000000, 0x200000, CRC(4149ea67) SHA1(35fc9e60cd368c6eab20e23deb581aa4f46e164e) )
	ROM_LOAD16_BYTE("d95-04.41", 0x400000, 0x200000, CRC(e9049d16) SHA1(ffa7dfc5d1cb82a601bad26b634c993aedda7803) )
ROM_END

ROM_START( spcnv95u )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e06.14",     0x000000, 0x20000, CRC(71ba7f00) SHA1(6b1d994c8319778aad3bec7bf7b24dc4944a36f0) )
	ROM_LOAD32_BYTE("e06.13",     0x000001, 0x20000, CRC(f506ba4b) SHA1(551f9e87d2bfd513998648b175b63677cd6bdd74) )
	ROM_LOAD32_BYTE("e06.12",     0x000002, 0x20000, CRC(06cbd72b) SHA1(0c8e11bd5f3fcf7451908c53e74ae545a0d97640) )
	ROM_LOAD32_BYTE("e06-15.u17", 0x000003, 0x20000, CRC(a6ec0103) SHA1(4f524a6b52bbdb370b8f98d26e7446da943e3edd) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e06.03", 0x000000, 0x100000, CRC(a24070ef) SHA1(9b6ac7852114c606e871a08cfb3b9e1081ac7030) )
	ROM_LOAD16_BYTE("e06.02", 0x000001, 0x100000, CRC(8f646dea) SHA1(07cd79671f36df1a5bbf2434e92a601351a36259) )
	ROM_LOAD       ("e06.01", 0x300000, 0x100000, CRC(51721b15) SHA1(448a9a7f3631072d8987351f5c0b8dd2c908d266) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e06.08", 0x000000, 0x100000, CRC(72ae2fbf) SHA1(7fa7ec94a8031342a2446fb8eca0d89ecfd2fa4f) )
	ROM_LOAD16_BYTE("e06.07", 0x000001, 0x100000, CRC(4b02e8f5) SHA1(02d8a97da52f9ba4033b8f0c3f455a908a9dce89) )
	ROM_LOAD       ("e06.06", 0x300000, 0x100000, CRC(9380db3c) SHA1(83f5a46a01b9c15499e0dc2222df496d26baa0d4) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e06.09", 0x100000, 0x40000, CRC(9bcafc87) SHA1(10b3f6da00a41550fe6a705232f0e33fda3c7e7c) )
	ROM_LOAD16_BYTE("e06.10", 0x100001, 0x40000, CRC(b752b61f) SHA1(e948a8af19c70ba8b8e908c869bc88ed0cac8420) )

	ROM_REGION16_BE( 0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e06.04", 0x000000, 0x200000, CRC(1dac29df) SHA1(ed68a41def148dcf4057cfac87a2a563c6882e1d) )
 	ROM_LOAD16_BYTE("e06.05", 0x400000, 0x200000, CRC(f370ff15) SHA1(4bc464d1c3a28326c8b1ae2036387954cb1dd813) )
ROM_END

ROM_START( spcinv95 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e06.14",     0x000000, 0x20000, CRC(71ba7f00) SHA1(6b1d994c8319778aad3bec7bf7b24dc4944a36f0) )
	ROM_LOAD32_BYTE("e06.13",     0x000001, 0x20000, CRC(f506ba4b) SHA1(551f9e87d2bfd513998648b175b63677cd6bdd74) )
	ROM_LOAD32_BYTE("e06.12",     0x000002, 0x20000, CRC(06cbd72b) SHA1(0c8e11bd5f3fcf7451908c53e74ae545a0d97640) )
	ROM_LOAD32_BYTE("e06-wrld.u17", 0x000003, 0x20000, CRC(d1eb3195) SHA1(40c5e326e8dd9a892abdab952f853799f26601b7) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e06.03", 0x000000, 0x100000, CRC(a24070ef) SHA1(9b6ac7852114c606e871a08cfb3b9e1081ac7030) )
	ROM_LOAD16_BYTE("e06.02", 0x000001, 0x100000, CRC(8f646dea) SHA1(07cd79671f36df1a5bbf2434e92a601351a36259) )
	ROM_LOAD       ("e06.01", 0x300000, 0x100000, CRC(51721b15) SHA1(448a9a7f3631072d8987351f5c0b8dd2c908d266) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e06.08", 0x000000, 0x100000, CRC(72ae2fbf) SHA1(7fa7ec94a8031342a2446fb8eca0d89ecfd2fa4f) )
	ROM_LOAD16_BYTE("e06.07", 0x000001, 0x100000, CRC(4b02e8f5) SHA1(02d8a97da52f9ba4033b8f0c3f455a908a9dce89) )
	ROM_LOAD       ("e06.06", 0x300000, 0x100000, CRC(9380db3c) SHA1(83f5a46a01b9c15499e0dc2222df496d26baa0d4) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e06.09", 0x100000, 0x40000, CRC(9bcafc87) SHA1(10b3f6da00a41550fe6a705232f0e33fda3c7e7c) )
	ROM_LOAD16_BYTE("e06.10", 0x100001, 0x40000, CRC(b752b61f) SHA1(e948a8af19c70ba8b8e908c869bc88ed0cac8420) )

	ROM_REGION16_BE( 0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e06.04", 0x000000, 0x200000, CRC(1dac29df) SHA1(ed68a41def148dcf4057cfac87a2a563c6882e1d) )
 	ROM_LOAD16_BYTE("e06.05", 0x400000, 0x200000, CRC(f370ff15) SHA1(4bc464d1c3a28326c8b1ae2036387954cb1dd813) )
ROM_END

ROM_START( akkanvdr )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e06.14", 0x000000, 0x20000, CRC(71ba7f00) SHA1(6b1d994c8319778aad3bec7bf7b24dc4944a36f0) )
	ROM_LOAD32_BYTE("e06.13", 0x000001, 0x20000, CRC(f506ba4b) SHA1(551f9e87d2bfd513998648b175b63677cd6bdd74) )
	ROM_LOAD32_BYTE("e06.12", 0x000002, 0x20000, CRC(06cbd72b) SHA1(0c8e11bd5f3fcf7451908c53e74ae545a0d97640) )
	ROM_LOAD32_BYTE("e06.11", 0x000003, 0x20000, CRC(3fe550b9) SHA1(6258d72204834abfba58bc2d5882f3616a6fd784) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e06.03", 0x000000, 0x100000, CRC(a24070ef) SHA1(9b6ac7852114c606e871a08cfb3b9e1081ac7030) )
	ROM_LOAD16_BYTE("e06.02", 0x000001, 0x100000, CRC(8f646dea) SHA1(07cd79671f36df1a5bbf2434e92a601351a36259) )
	ROM_LOAD       ("e06.01", 0x300000, 0x100000, CRC(51721b15) SHA1(448a9a7f3631072d8987351f5c0b8dd2c908d266) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e06.08", 0x000000, 0x100000, CRC(72ae2fbf) SHA1(7fa7ec94a8031342a2446fb8eca0d89ecfd2fa4f) )
	ROM_LOAD16_BYTE("e06.07", 0x000001, 0x100000, CRC(4b02e8f5) SHA1(02d8a97da52f9ba4033b8f0c3f455a908a9dce89) )
	ROM_LOAD       ("e06.06", 0x300000, 0x100000, CRC(9380db3c) SHA1(83f5a46a01b9c15499e0dc2222df496d26baa0d4) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e06.09", 0x100000, 0x40000, CRC(9bcafc87) SHA1(10b3f6da00a41550fe6a705232f0e33fda3c7e7c) )
	ROM_LOAD16_BYTE("e06.10", 0x100001, 0x40000, CRC(b752b61f) SHA1(e948a8af19c70ba8b8e908c869bc88ed0cac8420) )

	ROM_REGION16_BE( 0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e06.04", 0x000000, 0x200000, CRC(1dac29df) SHA1(ed68a41def148dcf4057cfac87a2a563c6882e1d) )
 	ROM_LOAD16_BYTE("e06.05", 0x400000, 0x200000, CRC(f370ff15) SHA1(4bc464d1c3a28326c8b1ae2036387954cb1dd813) )
ROM_END

ROM_START( elvactr )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e02-12.20", 0x000000, 0x80000, CRC(ea5f5a32) SHA1(4f30c56fbf068fee6d3afb2479043c7e89f6c055) )
	ROM_LOAD32_BYTE("e02-11.19", 0x000001, 0x80000, CRC(bcced8ff) SHA1(09c38c78300ba9d710b4e46ca71014bbc5ac46b4) )
	ROM_LOAD32_BYTE("e02-10.18", 0x000002, 0x80000, CRC(72f1b952) SHA1(9fc41ecfbee3581d9e92ff3a2ab6e4b93567e31d) )
	ROM_LOAD32_BYTE("ea2w.b63",  0x000003, 0x80000, CRC(cd97182b) SHA1(b3387980acfeec81eb0178d5b2955ac39595e22d) )

	ROM_REGION(0x800000, REGION_GFX1, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("e02-03.12", 0x000000, 0x200000, CRC(c884ebb5) SHA1(49009056bfdc564eac0ae6b7b49f070f05dc4ee3) )
	ROM_LOAD16_BYTE("e02-02.8",  0x000001, 0x200000, CRC(c8e06cfb) SHA1(071d095a4930ce18a782c577811b553a9705fbd7) )
	ROM_LOAD       ("e02-01.4",  0x600000, 0x200000, CRC(2ba94726) SHA1(3e9cdd076338e0e5358571ce4f97576f1a6a12a7) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("e02-08.47", 0x000000, 0x200000, CRC(29c9bd02) SHA1(a5b552ae7ac15f514ee6105410ec3e6e34ea0adb) )
	ROM_LOAD16_BYTE("e02-07.45", 0x000001, 0x200000, CRC(5eeee925) SHA1(d302da28df8ac6d406ef45f1d282ee22ce243857) )
	ROM_LOAD       ("e02-06.43", 0x600000, 0x200000, CRC(4c8726e9) SHA1(8ce2320a087f43c49428a39dafffec8c40d61b03) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e02-13.32", 0x100000, 0x40000, CRC(80932702) SHA1(c468234d03aa31b2aa0c3bd6bec32034216c2ae4) )
	ROM_LOAD16_BYTE("e02-14.33", 0x100001, 0x40000, CRC(706671a5) SHA1(1ac90647d617e73f12a67274a025ae43a6b3a316) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e02-04.38", 0x800000, 0x200000, CRC(b74307af) SHA1(deb42415049efa2df70e7b25ba8b1b716aa227f1) )
	ROM_RELOAD(                  0x000000, 0x200000 ) /*fix sound*/
	ROM_LOAD16_BYTE("e02-05.39", 0xc00000, 0x200000, CRC(eb729855) SHA1(85253efe794e8b5ffaf16bcb1123bca831e776a5) )
ROM_END

ROM_START( elvactrj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e02-12.20", 0x000000, 0x80000, CRC(ea5f5a32) SHA1(4f30c56fbf068fee6d3afb2479043c7e89f6c055) )
	ROM_LOAD32_BYTE("e02-11.19", 0x000001, 0x80000, CRC(bcced8ff) SHA1(09c38c78300ba9d710b4e46ca71014bbc5ac46b4) )
	ROM_LOAD32_BYTE("e02-10.18", 0x000002, 0x80000, CRC(72f1b952) SHA1(9fc41ecfbee3581d9e92ff3a2ab6e4b93567e31d) )
	ROM_LOAD32_BYTE("e02-09.17", 0x000003, 0x80000, CRC(23997907) SHA1(e5b0b9069b29cf08e1a782c73f42137aec198f7f) )

	ROM_REGION(0x800000, REGION_GFX1, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("e02-03.12", 0x000000, 0x200000, CRC(c884ebb5) SHA1(49009056bfdc564eac0ae6b7b49f070f05dc4ee3) )
	ROM_LOAD16_BYTE("e02-02.8",  0x000001, 0x200000, CRC(c8e06cfb) SHA1(071d095a4930ce18a782c577811b553a9705fbd7) )
	ROM_LOAD       ("e02-01.4",  0x600000, 0x200000, CRC(2ba94726) SHA1(3e9cdd076338e0e5358571ce4f97576f1a6a12a7) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("e02-08.47", 0x000000, 0x200000, CRC(29c9bd02) SHA1(a5b552ae7ac15f514ee6105410ec3e6e34ea0adb) )
	ROM_LOAD16_BYTE("e02-07.45", 0x000001, 0x200000, CRC(5eeee925) SHA1(d302da28df8ac6d406ef45f1d282ee22ce243857) )
	ROM_LOAD       ("e02-06.43", 0x600000, 0x200000, CRC(4c8726e9) SHA1(8ce2320a087f43c49428a39dafffec8c40d61b03) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e02-13.32", 0x100000, 0x40000, CRC(80932702) SHA1(c468234d03aa31b2aa0c3bd6bec32034216c2ae4) )
	ROM_LOAD16_BYTE("e02-14.33", 0x100001, 0x40000, CRC(706671a5) SHA1(1ac90647d617e73f12a67274a025ae43a6b3a316) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e02-04.38", 0x800000, 0x200000, CRC(b74307af) SHA1(deb42415049efa2df70e7b25ba8b1b716aa227f1) )
	ROM_RELOAD(                  0x000000, 0x200000 ) /*fix sound*/
	ROM_LOAD16_BYTE("e02-05.39", 0xc00000, 0x200000, CRC(eb729855) SHA1(85253efe794e8b5ffaf16bcb1123bca831e776a5) )
ROM_END

ROM_START( elvact2u )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e02-12.20", 0x000000, 0x80000, CRC(ea5f5a32) SHA1(4f30c56fbf068fee6d3afb2479043c7e89f6c055) )
	ROM_LOAD32_BYTE("e02-11.19", 0x000001, 0x80000, CRC(bcced8ff) SHA1(09c38c78300ba9d710b4e46ca71014bbc5ac46b4) )
	ROM_LOAD32_BYTE("e02-10.18", 0x000002, 0x80000, CRC(72f1b952) SHA1(9fc41ecfbee3581d9e92ff3a2ab6e4b93567e31d) )
	ROM_LOAD32_BYTE("ea2.b63",   0x000003, 0x80000, CRC(ba9028bd) SHA1(1d04ce5333143ed78ec297d89c0cdb99bf6e4bde) )

	ROM_REGION(0x800000, REGION_GFX1, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("e02-03.12", 0x000000, 0x200000, CRC(c884ebb5) SHA1(49009056bfdc564eac0ae6b7b49f070f05dc4ee3) )
	ROM_LOAD16_BYTE("e02-02.8",  0x000001, 0x200000, CRC(c8e06cfb) SHA1(071d095a4930ce18a782c577811b553a9705fbd7) )
	ROM_LOAD       ("e02-01.4",  0x600000, 0x200000, CRC(2ba94726) SHA1(3e9cdd076338e0e5358571ce4f97576f1a6a12a7) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("e02-08.47", 0x000000, 0x200000, CRC(29c9bd02) SHA1(a5b552ae7ac15f514ee6105410ec3e6e34ea0adb) )
	ROM_LOAD16_BYTE("e02-07.45", 0x000001, 0x200000, CRC(5eeee925) SHA1(d302da28df8ac6d406ef45f1d282ee22ce243857) )
	ROM_LOAD       ("e02-06.43", 0x600000, 0x200000, CRC(4c8726e9) SHA1(8ce2320a087f43c49428a39dafffec8c40d61b03) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e02-13.32", 0x100000, 0x40000, CRC(80932702) SHA1(c468234d03aa31b2aa0c3bd6bec32034216c2ae4) )
	ROM_LOAD16_BYTE("e02-14.33", 0x100001, 0x40000, CRC(706671a5) SHA1(1ac90647d617e73f12a67274a025ae43a6b3a316) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e02-04.38", 0x800000, 0x200000, CRC(b74307af) SHA1(deb42415049efa2df70e7b25ba8b1b716aa227f1) )
	ROM_RELOAD(                  0x000000, 0x200000 ) /*fix sound*/
	ROM_LOAD16_BYTE("e02-05.39", 0xc00000, 0x200000, CRC(eb729855) SHA1(85253efe794e8b5ffaf16bcb1123bca831e776a5) )
ROM_END

ROM_START( twinqix )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("mpr0-3.b60", 0x000000, 0x40000, CRC(1a63d0de) SHA1(7d8d8a6c9c7f9dfc0a8a528a905e33388b8fe13d) )
	ROM_LOAD32_BYTE("mpr0-2.b61", 0x000001, 0x40000, CRC(45a70987) SHA1(8cca6845064d943fd28416143e60399188b023cd) )
	ROM_LOAD32_BYTE("mpr0-1.b62", 0x000002, 0x40000, CRC(531f9447) SHA1(4d18efaad9c3dd2b14d3125c0f9e18cfcde3a1f2) )
	ROM_LOAD32_BYTE("mpr0-0.b63", 0x000003, 0x40000, CRC(a4c44c11) SHA1(b928134028bb8cddd6e34a501a4aad56173e2ae2) )

	ROM_REGION(0x200000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("obj0-0.a08", 0x000000, 0x080000, CRC(c6ea845c) SHA1(9df710637e8f64f7fec232b5ebbede588e07c2db) )
	ROM_LOAD16_BYTE("obj0-1.a20", 0x000001, 0x080000, CRC(8c12b7fb) SHA1(8a52870fb9f508148619763fb6f37dd74b5386ca) )
	ROM_FILL       (              0x100000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
  	ROM_LOAD32_BYTE("scr0-0.b07",  0x000000, 0x080000, CRC(9a1b9b34) SHA1(ddf9c6ba0f9c340b580573e1d96ac76b1cd35beb) )
	ROM_LOAD32_BYTE("scr0-1.b06",  0x000002, 0x080000, CRC(e9bef879) SHA1(7e720f5054a1ef3a28353f1c221f4cf15d3b7428) )
	ROM_LOAD32_BYTE("scr0-2.b05",  0x000001, 0x080000, CRC(cac6854b) SHA1(c97fb7de48e1644695bbe431587d6c1be01ea62d) )
	ROM_LOAD32_BYTE("scr0-3.b04",  0x000003, 0x080000, CRC(ce063034) SHA1(2ecff74427d7d2fa8d1db4ac87481d123d7ce003) )
	ROM_LOAD16_BYTE("scr0-4.b03",  0x300000, 0x080000, CRC(d32280fe) SHA1(56b120128c5e4b8c6598a1de51269e6702a63175) )
	ROM_LOAD16_BYTE("scr0-5.b02",  0x300001, 0x080000, CRC(fdd1a85b) SHA1(1d94a4858baef3e78c456049dc58249a574205fe) )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("spr0-1.b66", 0x100000, 0x40000, CRC(4b20e99d) SHA1(faf184daea0f1131bafa50edb48bd470d4c0b141) )
	ROM_LOAD16_BYTE("spr0-0.b65", 0x100001, 0x40000, CRC(2569eb30) SHA1(ec804131025e600198cd8342925823340e7ef458) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("snd-0.b43",  0x000000, 0x80000, CRC(ad5405a9) SHA1(67ee42498d2c3c00015237b3b5cd020f9a7c4a18) )
	ROM_LOAD16_BYTE("snd-1.b44",  0x100000, 0x80000, CRC(274864af) SHA1(47fefee23038bb751bdf6b6f48312ba0b6e38b90) )
	ROM_LOAD16_BYTE("snd-14.b10", 0xe00000, 0x80000, CRC(26312451) SHA1(9f947a11592fd8420fc581914bf16e7ade75390c) )
	ROM_LOAD16_BYTE("snd-15.b11", 0xf00000, 0x80000, CRC(2edaa9dc) SHA1(72fead505c4f44e5736ff7d545d72dfa37d613e2) )
ROM_END

ROM_START( quizhuhu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
	ROM_LOAD32_BYTE("e08-16.20", 0x000000, 0x80000, CRC(faa8f373) SHA1(e263b058288fcacf9b15188ab78e8fb05a8971a7) )
	ROM_LOAD32_BYTE("e08-15.19", 0x000001, 0x80000, CRC(23acf231) SHA1(a87933439b3d1d92f8b9d545b13f20cc47a7fd4e) )
	ROM_LOAD32_BYTE("e08-14.18", 0x000002, 0x80000, CRC(33a4951d) SHA1(69e8fe994f620ce056cdedca77bff1d0c6e74483) )
	ROM_LOAD32_BYTE("e08-13.17", 0x000003, 0x80000, CRC(0936fd2a) SHA1(f0f7017c755b28644b67b4fd6d5e19c272e9c3a2) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e08-06.12", 0x000000, 0x200000, CRC(8dadc9ac) SHA1(469e3c5063f5cb0832fb5bb5000ecd3c342cd095) )
  	ROM_LOAD16_BYTE("e08-04.8",  0x000001, 0x200000, CRC(5423721d) SHA1(7e9f4492845b7b4df0336203b1da6ca5ffeb36de) )
	ROM_LOAD16_BYTE("e08-05.11", 0x400000, 0x100000, CRC(79d2e516) SHA1(7dc0c23f3995d14b443a3f67d488e5ab780e8a94) )
  	ROM_LOAD16_BYTE("e08-03.7",  0x400001, 0x100000, CRC(07b9ab6a) SHA1(db205822233c385e1dbe4a9d40b311df9bca7053) )
	ROM_LOAD       ("e08-02.4",  0x900000, 0x200000, CRC(d89eb067) SHA1(bd8e1cf4c2046894c629d927fa05806b9b73505d) )
	ROM_LOAD       ("e08-01.3",  0xb00000, 0x100000, CRC(90223c06) SHA1(f07dae563946908d471ae89db74a2e55c5ab5890) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e08-12.47", 0x000000, 0x100000, CRC(6c711d36) SHA1(3fbff7783323d968ade72ac53531a7bcf7b9d234) )
	ROM_LOAD16_BYTE("e08-11.45", 0x000001, 0x100000, CRC(56775a60) SHA1(8bb8190101f2e8487ebb707022ff89d97bb7b39a) )
	ROM_LOAD       ("e08-10.43", 0x300000, 0x100000, CRC(60abc71b) SHA1(f4aa906920c6134c33a4dfb51724f3adbd3d7de4) )
	ROM_FILL       (             0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e08-18.32", 0x100000, 0x20000, CRC(e695497e) SHA1(9d845b4c0bd9b40471fb4b5ab2f9240058bc324f) )
	ROM_LOAD16_BYTE("e08-17.33", 0x100001, 0x20000, CRC(fafc7e4e) SHA1(26f46d5900fbf26d25651e7e818e486fc7a878ec) )

	ROM_REGION16_BE(0xe00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e08-07.38", 0x000000, 0x200000, CRC(c05dc85b) SHA1(d46ae3f066bbe041edde40358dd54f93e8e195de) )
	ROM_LOAD16_BYTE("e08-08.39", 0x400000, 0x200000, CRC(3eb94a99) SHA1(e6e8832e87397811dfc40525f2a15fc0415cec68) )
	ROM_LOAD16_BYTE("e08-09.41", 0x800000, 0x200000, CRC(200b26ee) SHA1(c689d0a1c1f5d71e0af3d94073b29d3619187c5f) )
ROM_END

ROM_START( pbobble2 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
	ROM_LOAD32_BYTE("e10-11.rom", 0x000000, 0x40000, CRC(b82f81da) SHA1(2cd0fb321c853497058545525f430b52c0788fb1) )
	ROM_LOAD32_BYTE("e10-10.rom", 0x000001, 0x40000, CRC(f432267a) SHA1(f9778fc627773e4e254faa0ce10e68407251ce95) )
	ROM_LOAD32_BYTE("e10-09.rom", 0x000002, 0x40000, CRC(e0b1b599) SHA1(99ef34b014db7c52f2ced05b2b90099a9c873259) )
	ROM_LOAD32_BYTE("e10-15.rom", 0x000003, 0x40000, CRC(a2c0a268) SHA1(c96bb8a2959266c5c832fb77d119ad129b9ef9ee) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e10-02.rom", 0x000000, 0x100000, CRC(c0564490) SHA1(cbe9f880192c08f4d1db21d5ba14073b97e5f1d3) )
  	ROM_LOAD16_BYTE("e10-01.rom", 0x000001, 0x100000, CRC(8c26ff49) SHA1(cbb514c061106003d2ae2b6c43958b24feaad656) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e10-07.rom", 0x000000, 0x100000, CRC(dcb3c29b) SHA1(b80c3a8ce53d696c57675e654c9927ef8687759e) )
	ROM_LOAD16_BYTE("e10-06.rom", 0x000001, 0x100000, CRC(1b0f20e2) SHA1(66b44d059c2896abac2f0e7fc932489dee440ba0) )
	ROM_LOAD       ("e10-05.rom", 0x300000, 0x100000, CRC(81266151) SHA1(aa3b144f32995425db97efce440e234a3c7a6715) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e10-12.rom", 0x100000, 0x40000, CRC(b92dc8ad) SHA1(0c1428d313507b1ae5a2af3b2fbaaa5650135e1e) )
	ROM_LOAD16_BYTE("e10-13.rom", 0x100001, 0x40000, CRC(87842c13) SHA1(d15b47c7430e677ae172f86fd5be595e4fe72e42) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e10-03.rom", 0x000000, 0x200000, CRC(46d68ac8) SHA1(ad014e9f0d458308014959ca6823077f581ab088) )
	ROM_LOAD16_BYTE("e10-04.rom", 0x400000, 0x200000, CRC(5c0862a6) SHA1(f916f63b8629239e3221e1e231e1b39962ef38ba) )
ROM_END

ROM_START( pbobbl2j )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
	ROM_LOAD32_BYTE("e10-11.rom", 0x000000, 0x40000, CRC(b82f81da) SHA1(2cd0fb321c853497058545525f430b52c0788fb1) )
	ROM_LOAD32_BYTE("e10-10.rom", 0x000001, 0x40000, CRC(f432267a) SHA1(f9778fc627773e4e254faa0ce10e68407251ce95) )
	ROM_LOAD32_BYTE("e10-09.rom", 0x000002, 0x40000, CRC(e0b1b599) SHA1(99ef34b014db7c52f2ced05b2b90099a9c873259) )
	ROM_LOAD32_BYTE("e10-08.17",  0x000003, 0x40000, CRC(4ccec344) SHA1(dfb30d149dde6d8e1a117bf0bafb85178540aa58) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e10-02.rom", 0x000000, 0x100000, CRC(c0564490) SHA1(cbe9f880192c08f4d1db21d5ba14073b97e5f1d3) )
  	ROM_LOAD16_BYTE("e10-01.rom", 0x000001, 0x100000, CRC(8c26ff49) SHA1(cbb514c061106003d2ae2b6c43958b24feaad656) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e10-07.rom", 0x000000, 0x100000, CRC(dcb3c29b) SHA1(b80c3a8ce53d696c57675e654c9927ef8687759e) )
	ROM_LOAD16_BYTE("e10-06.rom", 0x000001, 0x100000, CRC(1b0f20e2) SHA1(66b44d059c2896abac2f0e7fc932489dee440ba0) )
	ROM_LOAD       ("e10-05.rom", 0x300000, 0x100000, CRC(81266151) SHA1(aa3b144f32995425db97efce440e234a3c7a6715) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e10-12.rom", 0x100000, 0x40000, CRC(b92dc8ad) SHA1(0c1428d313507b1ae5a2af3b2fbaaa5650135e1e) )
	ROM_LOAD16_BYTE("e10-13.rom", 0x100001, 0x40000, CRC(87842c13) SHA1(d15b47c7430e677ae172f86fd5be595e4fe72e42) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e10-03.rom", 0x000000, 0x200000, CRC(46d68ac8) SHA1(ad014e9f0d458308014959ca6823077f581ab088) )
	ROM_LOAD16_BYTE("e10-04.rom", 0x400000, 0x200000, CRC(5c0862a6) SHA1(f916f63b8629239e3221e1e231e1b39962ef38ba) )
ROM_END

ROM_START( pbobbl2u )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
	ROM_LOAD32_BYTE("e10-11.rom", 0x000000, 0x40000, CRC(b82f81da) SHA1(2cd0fb321c853497058545525f430b52c0788fb1) )
	ROM_LOAD32_BYTE("e10-10.rom", 0x000001, 0x40000, CRC(f432267a) SHA1(f9778fc627773e4e254faa0ce10e68407251ce95) )
	ROM_LOAD32_BYTE("e10-09.rom", 0x000002, 0x40000, CRC(e0b1b599) SHA1(99ef34b014db7c52f2ced05b2b90099a9c873259) )
	ROM_LOAD32_BYTE("e10-14.bin", 0x000003, 0x40000, CRC(d5c792fe) SHA1(bb28a6a3ce1e79041c27b550f44210f0994e6a46) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e10-02.rom", 0x000000, 0x100000, CRC(c0564490) SHA1(cbe9f880192c08f4d1db21d5ba14073b97e5f1d3) )
  	ROM_LOAD16_BYTE("e10-01.rom", 0x000001, 0x100000, CRC(8c26ff49) SHA1(cbb514c061106003d2ae2b6c43958b24feaad656) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e10-07.rom", 0x000000, 0x100000, CRC(dcb3c29b) SHA1(b80c3a8ce53d696c57675e654c9927ef8687759e) )
	ROM_LOAD16_BYTE("e10-06.rom", 0x000001, 0x100000, CRC(1b0f20e2) SHA1(66b44d059c2896abac2f0e7fc932489dee440ba0) )
	ROM_LOAD       ("e10-05.rom", 0x300000, 0x100000, CRC(81266151) SHA1(aa3b144f32995425db97efce440e234a3c7a6715) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e10-12.rom", 0x100000, 0x40000, CRC(b92dc8ad) SHA1(0c1428d313507b1ae5a2af3b2fbaaa5650135e1e) )
	ROM_LOAD16_BYTE("e10-13.rom", 0x100001, 0x40000, CRC(87842c13) SHA1(d15b47c7430e677ae172f86fd5be595e4fe72e42) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e10-03.rom", 0x000000, 0x200000, CRC(46d68ac8) SHA1(ad014e9f0d458308014959ca6823077f581ab088) )
	ROM_LOAD16_BYTE("e10-04.rom", 0x400000, 0x200000, CRC(5c0862a6) SHA1(f916f63b8629239e3221e1e231e1b39962ef38ba) )
ROM_END

ROM_START( pbobbl2x )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e10.29", 0x000000, 0x40000, CRC(f1e9ad3f) SHA1(8689d85f30e075d21e4be01a2a097a850a921c47) )
	ROM_LOAD32_BYTE("e10.28", 0x000001, 0x40000, CRC(412a3602) SHA1(d754e6ac886676d2c1eb52de3a727894f316e6b5) )
	ROM_LOAD32_BYTE("e10.27", 0x000002, 0x40000, CRC(88cc0b5c) SHA1(bb08a7b8b37356376052ed03f8515677811823c0) )
	ROM_LOAD32_BYTE("e10.26", 0x000003, 0x40000, CRC(a5c24047) SHA1(62861577ce0aedb8d05360f0302fceecbde15420) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e10-02.rom", 0x000000, 0x100000, CRC(c0564490) SHA1(cbe9f880192c08f4d1db21d5ba14073b97e5f1d3) )
  	ROM_LOAD16_BYTE("e10-01.rom", 0x000001, 0x100000, CRC(8c26ff49) SHA1(cbb514c061106003d2ae2b6c43958b24feaad656) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e10-07.rom", 0x000000, 0x100000, CRC(dcb3c29b) SHA1(b80c3a8ce53d696c57675e654c9927ef8687759e) )
	ROM_LOAD16_BYTE("e10-06.rom", 0x000001, 0x100000, CRC(1b0f20e2) SHA1(66b44d059c2896abac2f0e7fc932489dee440ba0) )
	ROM_LOAD       ("e10-05.rom", 0x300000, 0x100000, CRC(81266151) SHA1(aa3b144f32995425db97efce440e234a3c7a6715) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e10.30", 0x100000, 0x40000, CRC(bb090c1e) SHA1(af2ff23d6f9bd56c25530cb9bf9f452b6f5210f5) )
	ROM_LOAD16_BYTE("e10.31", 0x100001, 0x40000, CRC(f4b88d65) SHA1(c74dcb4bed979039fad1d5c7528c14ce4db1d5ec) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e10-03.rom", 0x000000, 0x200000, CRC(46d68ac8) SHA1(ad014e9f0d458308014959ca6823077f581ab088) )
	ROM_LOAD16_BYTE("e10-04.rom", 0x400000, 0x200000, CRC(5c0862a6) SHA1(f916f63b8629239e3221e1e231e1b39962ef38ba) )
ROM_END

ROM_START( gekirido )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e11-12.bin", 0x000000, 0x40000, CRC(6a7aaacf) SHA1(a8114c84e76c75c908a61d985d96aa4eb9a0ac5a) )
	ROM_LOAD32_BYTE("e11-11.bin", 0x000001, 0x40000, CRC(2284a08e) SHA1(3dcb91be0d3491ad5e77efd30bacd506dad0f848) )
	ROM_LOAD32_BYTE("e11-10.bin", 0x000002, 0x40000, CRC(8795e6ba) SHA1(9128c29fdce3276f55aad47451e4a507470c8b9f) )
	ROM_LOAD32_BYTE("e11-09.bin", 0x000003, 0x40000, CRC(b4e17ef4) SHA1(ab06ab68aaa487cc3046a15fef3dde8581197391) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e11-03.bin", 0x000000, 0x200000, CRC(f73877c5) SHA1(1f6b7c0b8a0aaab3e5427d21de7fad3d3cbf737a) )
  	ROM_LOAD16_BYTE("e11-02.bin", 0x000001, 0x200000, CRC(5722a83b) SHA1(823c20a33016a5506ca5415ec615c3d2546ca9ab) )
	ROM_LOAD       ("e11-01.bin", 0x600000, 0x200000, CRC(c2cd1069) SHA1(9744dd3d8a6d9200cea4429dafce5620b60e2960) )
 	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e11-08.bin", 0x000000, 0x200000, CRC(907f69d3) SHA1(0899ed58edcae22144625c349c9d2fe4d46d11e3) )
	ROM_LOAD16_BYTE("e11-07.bin", 0x000001, 0x200000, CRC(ef018607) SHA1(61b602b13754c3be21caf76acbfc10c87518ba47) )
	ROM_LOAD       ("e11-06.bin", 0x600000, 0x200000, CRC(200ce305) SHA1(c80a0b96510913a6411e6763fb72bf413fb792da) )
 	ROM_FILL       (              0x400000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e11-13.bin", 0x100000, 0x20000, CRC(51a11ff7) SHA1(c03042489d71423667f25856d15aa6a363ea6c92) )
	ROM_LOAD16_BYTE("e11-14.bin", 0x100001, 0x20000, CRC(dce2ba91) SHA1(00bc353c7747a7954365b587d7bc759ee5dc09c2) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e11-04.bin", 0x000000, 0x200000, CRC(e0ff4fb1) SHA1(81e186e3a692af1da316b8085a729c4f103d9a52) )
	ROM_LOAD16_BYTE("e11-05.bin", 0xc00000, 0x200000, CRC(a4d08cf1) SHA1(ae2cabef7b7bcb8a788988c73d7af6fa4bb2c444) )
ROM_END

ROM_START( ktiger2 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e15-14.bin", 0x000000, 0x40000, CRC(b527b733) SHA1(19efd647ea9c277b306714fe79ebf40d5f9d2187) )
	ROM_LOAD32_BYTE("e15-13.bin", 0x000001, 0x40000, CRC(0f03daf7) SHA1(de5aee5a339224dfe5e03a02d3ef5ffd5a39211e) )
	ROM_LOAD32_BYTE("e15-12.bin", 0x000002, 0x40000, CRC(59d832f2) SHA1(27019b4121b1f8b0b9e141234192b3da1a4af718) )
	ROM_LOAD32_BYTE("e15-11.bin", 0x000003, 0x40000, CRC(a706a286) SHA1(c3d1cdb0c5b1004acadc926ffd9083c9afea8608) )

	ROM_REGION(0xc00000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e15-04.bin", 0x000000, 0x200000, CRC(6ea8d9bd) SHA1(c31644e89752325ba2f174b60e31bd9659479391) )
  	ROM_LOAD16_BYTE("e15-02.bin", 0x000001, 0x200000, CRC(2ea7f2bd) SHA1(3b42f603f7d35e9b32931a2e8913eb735151a64c) )
	ROM_LOAD16_BYTE("e15-03.bin", 0x400000, 0x100000, CRC(be45a52f) SHA1(5d9735a774233b43003057cbab6ae7d6e0195dd2) )
  	ROM_LOAD16_BYTE("e15-01.bin", 0x400001, 0x100000, CRC(85421aac) SHA1(327e72f0107e024ec9fc9dc20d381e2e20f36248) )
 	ROM_FILL       (              0x600000, 0x600000, 0 )

	ROM_REGION(0xc00000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e15-10.bin", 0x000000, 0x200000, CRC(d8c96b00) SHA1(9cd275abb66b3475433ea2649dc872d7d35eb5b8) )
	ROM_LOAD16_BYTE("e15-08.bin", 0x000001, 0x200000, CRC(4bdb2bf3) SHA1(1146b7a5d9f26d3173a7c64768e55d53a0ab7b8e) )
	ROM_LOAD16_BYTE("e15-09.bin", 0x400000, 0x100000, CRC(07c29f60) SHA1(3ca0f632e7047cc50ee3ce24cd6c0c8c7252a278) )
	ROM_LOAD16_BYTE("e15-07.bin", 0x400001, 0x100000, CRC(8164f7ee) SHA1(4550521f820e93ec08b86d148135966d016cbf22) )
	ROM_FILL       (              0x600000, 0x600000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e15-15.bin", 0x100000, 0x20000, CRC(22126dfb) SHA1(a1af17e5c3440f1bab50d79f92c251f1a4536ca0) )
	ROM_LOAD16_BYTE("e15-16.bin", 0x100001, 0x20000, CRC(f8b58ea0) SHA1(c9e196620765efc4c7b535793a5d1f586698ce55) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e15-05.bin", 0x000000, 0x200000, CRC(3e5da5f6) SHA1(da6fc8b26cd02c45cfc0f1aa5292614e4d28cae4) )
	ROM_LOAD16_BYTE("e15-06.bin", 0x400000, 0x200000, CRC(b182a3e1) SHA1(db8569b069911bb84900b2aa5168c45ba3e985c7) )
ROM_END

ROM_START( bubblem )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e21-21.rom", 0x000000, 0x080000, CRC(cac4169c) SHA1(e1e5b9bbaecfd29ee764c8b29df8ffd08ef01866) )
	ROM_LOAD32_BYTE("e21-20.rom", 0x000001, 0x080000, CRC(7727c673) SHA1(cda3dbcf8da06e81b899008462bcd6b2ea43db81) )
	ROM_LOAD32_BYTE("e21-19.rom", 0x000002, 0x080000, CRC(be0b907d) SHA1(8bb6a149a4b0ccdb32396f7e750218a0bdc31965) )
	ROM_LOAD32_BYTE("e21-18.rom", 0x000003, 0x080000, CRC(d14e313a) SHA1(3913d396a6a72f539163c216809e54a06ecd3b96) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e21-02.rom", 0x000000, 0x200000, CRC(b7cb9232) SHA1(ba71cb98d49eadebb26d9f53bbaec1dc211077f5) )
	ROM_LOAD16_BYTE("e21-01.rom", 0x000001, 0x200000, CRC(a11f2f99) SHA1(293c5996600cad05bf98f936f5f820d93d546099) )
	ROM_FILL       (              0x400000, 0x400000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e21-07.rom", 0x000000, 0x100000, CRC(7789bf7c) SHA1(bc8ef1696adac99a1fabae9b79afcd3461cf323b) )
	ROM_LOAD16_BYTE("e21-06.rom", 0x000001, 0x100000, CRC(997fc0d7) SHA1(58a546f739072fedebfe7c972fe85f72107726b2) )
	ROM_LOAD       ("e21-05.rom", 0x300000, 0x100000, CRC(07eab58f) SHA1(ae2d7b839b39d88d11652df74804a39230674467) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* Sound CPU */
	ROM_LOAD16_BYTE("e21-12.rom", 0x100000, 0x40000, CRC(34093de1) SHA1(d69d6b5f10b8fe86f727d739ed5aecceb15e01f7) )
	ROM_LOAD16_BYTE("e21-13.rom", 0x100001, 0x40000, CRC(9e9ec437) SHA1(b0265b688846c642d240b2f3677d2330d31eaa87) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e21-03.rom", 0x000000, 0x200000, CRC(54c5f83d) SHA1(10a993199c8d5a1361bd29a4b92c404451c6da01) )
	ROM_LOAD16_BYTE("e21-04.rom", 0x400000, 0x200000, CRC(e5af2a2d) SHA1(62a49504decc7160b710260218920d2d6d2af8f0) )
ROM_END

ROM_START( bubblemj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e21-11.20", 0x000000, 0x080000, CRC(df0eeae4) SHA1(4cc8d350da881947c1b9c4e0b8fbe220494f6c38) )
	ROM_LOAD32_BYTE("e21-10.19", 0x000001, 0x080000, CRC(cdfb58f6) SHA1(70d2b8228ab4ddd572fe2ee53c1b7205b66ef6a3) )
	ROM_LOAD32_BYTE("e21-09.18", 0x000002, 0x080000, CRC(6c305f17) SHA1(c4118722d697ccf54b43626a47673892a6c2caaf) )
	ROM_LOAD32_BYTE("e21-08.17", 0x000003, 0x080000, CRC(27381ae2) SHA1(29b5d4bafa4ac02d35cb3ed7b7461e749ef2d6d6) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e21-02.rom", 0x000000, 0x200000, CRC(b7cb9232) SHA1(ba71cb98d49eadebb26d9f53bbaec1dc211077f5) )
	ROM_LOAD16_BYTE("e21-01.rom", 0x000001, 0x200000, CRC(a11f2f99) SHA1(293c5996600cad05bf98f936f5f820d93d546099) )
	ROM_FILL       (              0x400000, 0x400000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e21-07.rom", 0x000000, 0x100000, CRC(7789bf7c) SHA1(bc8ef1696adac99a1fabae9b79afcd3461cf323b) )
	ROM_LOAD16_BYTE("e21-06.rom", 0x000001, 0x100000, CRC(997fc0d7) SHA1(58a546f739072fedebfe7c972fe85f72107726b2) )
	ROM_LOAD       ("e21-05.rom", 0x300000, 0x100000, CRC(07eab58f) SHA1(ae2d7b839b39d88d11652df74804a39230674467) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* Sound CPU */
	ROM_LOAD16_BYTE("e21-12.rom", 0x100000, 0x40000, CRC(34093de1) SHA1(d69d6b5f10b8fe86f727d739ed5aecceb15e01f7) )
	ROM_LOAD16_BYTE("e21-13.rom", 0x100001, 0x40000, CRC(9e9ec437) SHA1(b0265b688846c642d240b2f3677d2330d31eaa87) )

	ROM_REGION16_BE(0xa00000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e21-03.rom", 0x000000, 0x200000, CRC(54c5f83d) SHA1(10a993199c8d5a1361bd29a4b92c404451c6da01) )
	ROM_LOAD16_BYTE("e21-04.rom", 0x400000, 0x200000, CRC(e5af2a2d) SHA1(62a49504decc7160b710260218920d2d6d2af8f0) )
ROM_END

ROM_START( cleopatr )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e28-10.bin", 0x000000, 0x80000, CRC(013fbc39) SHA1(d36ac44609b88e1da35c98dda381042e0112ea00) )
	ROM_LOAD32_BYTE("e28-09.bin", 0x000001, 0x80000, CRC(1c48a1f9) SHA1(791d321c03073cdd0269b970f926897446d2a6fb) )
	ROM_LOAD32_BYTE("e28-08.bin", 0x000002, 0x80000, CRC(7564f199) SHA1(ec4b19edb0660ad478f6c0ec27d701368696a2e4) )
	ROM_LOAD32_BYTE("e28-07.bin", 0x000003, 0x80000, CRC(a507797b) SHA1(6fa04091df1fa8c08f03b1ee378b4ec4a6ef7f51) )

	ROM_REGION(0x200000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e28-02.bin", 0x000000, 0x080000, CRC(b20d47cb) SHA1(6888e5564688840fed1c123ab38467066cd59c7f) )
	ROM_LOAD16_BYTE("e28-01.bin", 0x000001, 0x080000, CRC(4440e659) SHA1(71dece81bac8d638473c6531fed5c32798096af9) )
	ROM_FILL       (              0x100000, 0x100000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e28-06.bin", 0x000000, 0x100000, CRC(21d0c454) SHA1(f4c815984b19321cfab303fa6f21d9cad35b09f2) )
	ROM_LOAD16_BYTE("e28-05.bin", 0x000001, 0x100000, CRC(2870dbbc) SHA1(4e412b90cbd9a05956cde3d8cff615ebadca9db6) )
	ROM_LOAD       ("e28-04.bin", 0x300000, 0x100000, CRC(57aef029) SHA1(5c07209015d4749d1ffb3e9c1a890e6cfeec8cb0) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* Sound CPU */
	ROM_LOAD16_BYTE("e28-11.bin", 0x100000, 0x20000, CRC(01a06950) SHA1(94d22cd839f9027e9d45264c366e0cb5d698e0b6) )
	ROM_LOAD16_BYTE("e28-12.bin", 0x100001, 0x20000, CRC(dc19260f) SHA1(fa0ca03a236326652e4f9898d07cd837c1507a9d) )

	ROM_REGION16_BE(0x600000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e28-03.bin", 0x000000, 0x200000, CRC(15c7989d) SHA1(7cc63d93e5c1f9f52f889e973bbefd5e6f7ce807) )
ROM_END

ROM_START( pbobble3 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("pb3_12.rom", 0x000000, 0x80000, CRC(9eb19a00) SHA1(5a6417e4377070f9f01110dc6d513d0de01cff1e) )
	ROM_LOAD32_BYTE("pb3_11.rom", 0x000001, 0x80000, CRC(e54ada97) SHA1(325e2bc7156656cc262989910dde07a1746cf790) )
	ROM_LOAD32_BYTE("pb3_10.rom", 0x000002, 0x80000, CRC(1502a122) SHA1(cb981a4578aa30276c491a0ef47f5e05c05d8b28) )
	ROM_LOAD32_BYTE("pb3_16.rom", 0x000003, 0x80000, CRC(aac293da) SHA1(2188d1abe6aeefa872cf16db40999574497d982e) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("pb3_02.rom", 0x000000, 0x100000, CRC(437391d3) SHA1(b3cc64c68553d37e0bd09e0dece14901d8df5866) )
	ROM_LOAD16_BYTE("pb3_01.rom", 0x000001, 0x100000, CRC(52547c77) SHA1(d0cc8b8915cec1506c9733a1ce1638038ea93d25) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("pb3_08.rom", 0x000000, 0x100000, CRC(7040a3d5) SHA1(ea284ec530aac20348f84122e38a508bbc283f44) )
	ROM_LOAD16_BYTE("pb3_07.rom", 0x000001, 0x100000, CRC(fca2ea9b) SHA1(a87ebedd0d16657288df434a70b8933fafe0ca25) )
	ROM_LOAD       ("pb3_06.rom", 0x300000, 0x100000, CRC(c16184f8) SHA1(ded417d9d116b5a2f7518fa404bc2dda1c6a6366) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("pb3_13.rom", 0x100000, 0x40000, CRC(1ef551ef) SHA1(527defe8f35314304adb4b483285b08cd6ebe865) )
	ROM_LOAD16_BYTE("pb3_14.rom", 0x100001, 0x40000, CRC(7ee7e688) SHA1(d65aa9c449e1d64f10d1be9727a9d93ab1571e65) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("pb3_05.rom", 0x000000, 0x200000, CRC(e33c1234) SHA1(84c336ed6fd8723e824889fe7b52c284be659e62) )
	ROM_LOAD16_BYTE("pb3_04.rom", 0x400000, 0x200000, CRC(d1f42457) SHA1(2c77be6365deb5ef215da0c66da23b415623bdb1) )
	ROM_LOAD16_BYTE("pb3_03.rom", 0xc00000, 0x200000, CRC(a4371658) SHA1(26510a3f6de97f49b10dfc5cb9b7da947a44bfcb) )
	ROM_RELOAD(                   0x800000, 0x200000 ) /*fix sound*/
ROM_END

ROM_START( pbobbl3u )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("pb3_12.rom", 0x000000, 0x80000, CRC(9eb19a00) SHA1(5a6417e4377070f9f01110dc6d513d0de01cff1e) )
	ROM_LOAD32_BYTE("pb3_11.rom", 0x000001, 0x80000, CRC(e54ada97) SHA1(325e2bc7156656cc262989910dde07a1746cf790) )
	ROM_LOAD32_BYTE("pb3_10.rom", 0x000002, 0x80000, CRC(1502a122) SHA1(cb981a4578aa30276c491a0ef47f5e05c05d8b28) )
	ROM_LOAD32_BYTE("e29_09u.bin", 0x000003, 0x80000, CRC(ddc5a34c) SHA1(f38c99ac33b199b3ed99a84c67984f23a864e5d4) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("pb3_02.rom", 0x000000, 0x100000, CRC(437391d3) SHA1(b3cc64c68553d37e0bd09e0dece14901d8df5866) )
	ROM_LOAD16_BYTE("pb3_01.rom", 0x000001, 0x100000, CRC(52547c77) SHA1(d0cc8b8915cec1506c9733a1ce1638038ea93d25) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("pb3_08.rom", 0x000000, 0x100000, CRC(7040a3d5) SHA1(ea284ec530aac20348f84122e38a508bbc283f44) )
	ROM_LOAD16_BYTE("pb3_07.rom", 0x000001, 0x100000, CRC(fca2ea9b) SHA1(a87ebedd0d16657288df434a70b8933fafe0ca25) )
	ROM_LOAD       ("pb3_06.rom", 0x300000, 0x100000, CRC(c16184f8) SHA1(ded417d9d116b5a2f7518fa404bc2dda1c6a6366) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("pb3_13.rom", 0x100000, 0x40000, CRC(1ef551ef) SHA1(527defe8f35314304adb4b483285b08cd6ebe865) )
	ROM_LOAD16_BYTE("pb3_14.rom", 0x100001, 0x40000, CRC(7ee7e688) SHA1(d65aa9c449e1d64f10d1be9727a9d93ab1571e65) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("pb3_05.rom", 0x000000, 0x200000, CRC(e33c1234) SHA1(84c336ed6fd8723e824889fe7b52c284be659e62) )
	ROM_LOAD16_BYTE("pb3_04.rom", 0x400000, 0x200000, CRC(d1f42457) SHA1(2c77be6365deb5ef215da0c66da23b415623bdb1) )
	ROM_LOAD16_BYTE("pb3_03.rom", 0xc00000, 0x200000, CRC(a4371658) SHA1(26510a3f6de97f49b10dfc5cb9b7da947a44bfcb) )
	ROM_RELOAD(                   0x800000, 0x200000 ) /*fix sound*/
ROM_END

ROM_START( pbobbl3j )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("pb3_12.rom", 0x000000, 0x80000, CRC(9eb19a00) SHA1(5a6417e4377070f9f01110dc6d513d0de01cff1e) )
	ROM_LOAD32_BYTE("pb3_11.rom", 0x000001, 0x80000, CRC(e54ada97) SHA1(325e2bc7156656cc262989910dde07a1746cf790) )
	ROM_LOAD32_BYTE("pb3_10.rom", 0x000002, 0x80000, CRC(1502a122) SHA1(cb981a4578aa30276c491a0ef47f5e05c05d8b28) )
	ROM_LOAD32_BYTE("e29_09.bin", 0x000003, 0x80000, CRC(44ccf2f6) SHA1(60877525feaa992b1b374acfb5c16439e5f32161) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE("pb3_02.rom", 0x000000, 0x100000, CRC(437391d3) SHA1(b3cc64c68553d37e0bd09e0dece14901d8df5866) )
	ROM_LOAD16_BYTE("pb3_01.rom", 0x000001, 0x100000, CRC(52547c77) SHA1(d0cc8b8915cec1506c9733a1ce1638038ea93d25) )
	ROM_FILL       (              0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD16_BYTE("pb3_08.rom", 0x000000, 0x100000, CRC(7040a3d5) SHA1(ea284ec530aac20348f84122e38a508bbc283f44) )
	ROM_LOAD16_BYTE("pb3_07.rom", 0x000001, 0x100000, CRC(fca2ea9b) SHA1(a87ebedd0d16657288df434a70b8933fafe0ca25) )
	ROM_LOAD       ("pb3_06.rom", 0x300000, 0x100000, CRC(c16184f8) SHA1(ded417d9d116b5a2f7518fa404bc2dda1c6a6366) )
	ROM_FILL       (              0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("pb3_13.rom", 0x100000, 0x40000, CRC(1ef551ef) SHA1(527defe8f35314304adb4b483285b08cd6ebe865) )
	ROM_LOAD16_BYTE("pb3_14.rom", 0x100001, 0x40000, CRC(7ee7e688) SHA1(d65aa9c449e1d64f10d1be9727a9d93ab1571e65) )

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("pb3_05.rom", 0x000000, 0x200000, CRC(e33c1234) SHA1(84c336ed6fd8723e824889fe7b52c284be659e62) )
	ROM_LOAD16_BYTE("pb3_04.rom", 0x400000, 0x200000, CRC(d1f42457) SHA1(2c77be6365deb5ef215da0c66da23b415623bdb1) )
	ROM_LOAD16_BYTE("pb3_03.rom", 0xc00000, 0x200000, CRC(a4371658) SHA1(26510a3f6de97f49b10dfc5cb9b7da947a44bfcb) )
	ROM_RELOAD(                   0x800000, 0x200000 ) /*fix sound*/
ROM_END

ROM_START( arkretrn )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e36.11", 0x000000, 0x040000, CRC(b50cfb92) SHA1(dac69fc9ef03315b11bb94d19e3dfdc8867b08ed) )
	ROM_LOAD32_BYTE("e36.10", 0x000001, 0x040000, CRC(c940dba1) SHA1(ec87c9e4250f8b2f15094681a4783bca8c68f576) )
	ROM_LOAD32_BYTE("e36.09", 0x000002, 0x040000, CRC(f16985e0) SHA1(a74cfee8f958e7a32354d4353eeb199a7fb1ce64) )
	ROM_LOAD32_BYTE("e36.08", 0x000003, 0x040000, CRC(aa699e1b) SHA1(6bde0759940e0f238e4fa5bd228115574ff927d8) )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* Sound CPU */
	ROM_LOAD16_BYTE("e36.12", 0x100000, 0x40000, CRC(3bae39be) SHA1(777142ecc24799b934ed51ac4cd8700bb6da7e3c) )
	ROM_LOAD16_BYTE("e36.13", 0x100001, 0x40000, CRC(94448e82) SHA1(d7766490318623be770545918391c5e6144dd619) )

	ROM_REGION(0x100000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e36.03", 0x000000, 0x040000, CRC(1ea8558b) SHA1(b8ea4d6e1fb551b3c47f336a5e60ec33f7be525f) )
	ROM_LOAD16_BYTE("e36.02", 0x000001, 0x040000, CRC(694eda31) SHA1(1a6f85057395052571491f85c633d5632ab64865) )
	ROM_LOAD       ("e36.01", 0x0c0000, 0x040000, CRC(54b9b2cd) SHA1(55ae964ea1d2cc40a6578c5339754a270096f01f) )
	ROM_FILL       (          0x080000, 0x040000, 0 )

	ROM_REGION(0x200000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e36.07", 0x000000, 0x080000, CRC(266bf1c1) SHA1(489549478d7016400af2e643d4b98bf605237d34) )
	ROM_LOAD16_BYTE("e36.06", 0x000001, 0x080000, CRC(110ab729) SHA1(0ccc0a5abbcfd79a069daf5162cd344a5fb225d5) )
	ROM_LOAD       ("e36.05", 0x180000, 0x080000, CRC(db18bce2) SHA1(b6653facc7f5c624f5710a51f2b2abfe640177e2) )
	ROM_FILL       (          0x100000, 0x080000, 0 )

	ROM_REGION16_BE(0x600000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e36.04", 0x000000, 0x200000, CRC(2250959b) SHA1(06943f1b72bdf325485356a01278d88aeae93d87) )
ROM_END

ROM_START( kirameki )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e44-19.20", 0x000000, 0x80000, CRC(2f3b239a) SHA1(fb79955eba377d429ece04553251146b406143b1) )
	ROM_LOAD32_BYTE("e44-18.19", 0x000001, 0x80000, CRC(3137c8bc) SHA1(5f5cb47e214fe116cf985e847fa8340cf2ea5a64) )
	ROM_LOAD32_BYTE("e44-17.18", 0x000002, 0x80000, CRC(5905cd20) SHA1(52545622d3c7a31a9e95ab48e7251f1eae2c25b4) )
	ROM_LOAD32_BYTE("e44-16.17", 0x000003, 0x80000, CRC(5e9ac3fd) SHA1(3c45707d0d260961df99249978c7e8f51dd1720e) )

	ROM_REGION(0x1800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e44-06.12", 0x0000000, 0x400000, CRC(80526d58) SHA1(d32bf1e6494848b6e258b747245742310398be22) )
	ROM_LOAD16_BYTE("e44-04.8",  0x0000001, 0x400000, CRC(28c7c295) SHA1(90189ae26833499218b2236d48ce500a2eea3235) )
	ROM_LOAD16_BYTE("e44-05.11", 0x0800000, 0x200000, CRC(0fbc2b26) SHA1(0edd67213a9eba15fff0931a07608f9523ae1d95) )
	ROM_LOAD16_BYTE("e44-03.7",  0x0800001, 0x200000, CRC(d9e63fb0) SHA1(f2d8c30a4aaa2090673d5d2b1071e586a05c0236) )
	ROM_LOAD       ("e44-02.4",  0x1200000, 0x400000, CRC(5481efde) SHA1(560a1b8acf672781e912dca51599b5aa4d69520a) )
	ROM_LOAD       ("e44-01.3",  0x1600000, 0x200000, CRC(c4bdf727) SHA1(793a22a30ef44db818cfac96ff8e9d2f99cb672f) )
	ROM_FILL       (             0x0c00000, 0x600000, 0 )

	ROM_REGION(0xc00000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e44-14.47", 0x000000, 0x200000, CRC(4597c034) SHA1(3c16c20969df9e439d42a89e649146319df1b996) )
	ROM_LOAD16_BYTE("e44-12.45", 0x000001, 0x200000, CRC(7160a181) SHA1(d4a351f38219694d6545b4c502c0ba0a7f0bdfd0) )
	ROM_LOAD16_BYTE("e44-13.46", 0x400000, 0x100000, CRC(0b016c4e) SHA1(670d1741376cf929adad3c5e45f921ed4b317d31) )
	ROM_LOAD16_BYTE("e44-11.44", 0x400001, 0x100000, CRC(34d84143) SHA1(d553ab2da9188b1881f70802637d46574a42787e) )
	ROM_LOAD       ("e44-10.43", 0x900000, 0x200000, CRC(326f738e) SHA1(29c0c870341345eba10993446fecee08b6f13027) )
	ROM_LOAD       ("e44-09.42", 0xb00000, 0x100000, CRC(a8e68eb7) SHA1(843bbb8a61bd4b9cbb14c7242281ce0c83c432ff) )
	ROM_FILL       (             0x600000, 0x300000, 0 )

	ROM_REGION(0x400000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e44-20.51",      0x100000, 0x080000, CRC(4df7e051) SHA1(db0f5758458764a1c04116d5d5bbb20d4d36c875) )
	ROM_LOAD16_BYTE("e44-21.52",      0x100001, 0x080000, CRC(d31b94b8) SHA1(41ee381d10254dc6e7163c5f353568539a96fc20) )
	ROM_LOAD16_WORD_SWAP("e44-15.53", 0x200000, 0x200000, CRC(5043b608) SHA1(a328b8cc27ba1620a75a17cdf8571e217c42b9fd) ) /* Banked data */

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e44-07.38", 0x000000, 0x400000, CRC(a9e28544) SHA1(0dc3e1755a93fda310d26ed5f95dd211c05e579e) )
	ROM_LOAD16_BYTE("e44-08.39", 0x800000, 0x400000, CRC(33ba3037) SHA1(b4bbc4198929938607c444edf159ff40f53235d7) )
ROM_END

ROM_START( puchicar )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e46.16", 0x000000, 0x80000, CRC(cf2accdf) SHA1(b1e9808299a3c68c939009275108ee76cd7f5749) )
	ROM_LOAD32_BYTE("e46.15", 0x000001, 0x80000, CRC(c32c6ed8) SHA1(b0c4cca836e6957ecabdaddff23439f9d038a161) )
	ROM_LOAD32_BYTE("e46.14", 0x000002, 0x80000, CRC(a154c300) SHA1(177d9f3514f1e59a1036b979d2ab969249f519ca) )
	ROM_LOAD32_BYTE("e46.13", 0x000003, 0x80000, CRC(59fbdf3a) SHA1(4499a7579907e8e1d8ca2c29e8e8d12185e8fe4d) )

	ROM_REGION(0x1000000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e46.06", 0x000000, 0x200000, CRC(b74336f2) SHA1(f5039a4d2117c78e905e2ef6dec43e143a91915e) )
	ROM_LOAD16_BYTE("e46.04", 0x000001, 0x200000, CRC(463ecc4c) SHA1(e1649e68acc1637a5dc596b1b29c735e286056af) )
	ROM_LOAD16_BYTE("e46.05", 0x400000, 0x200000, CRC(83a32eee) SHA1(1a1059b0a5ba1542c866072ffe1874daba982387) )
	ROM_LOAD16_BYTE("e46.03", 0x400001, 0x200000, CRC(eb768193) SHA1(1d48334c0dfb9f72484717c267ac9b9b8d887fc8) )
	ROM_LOAD       ("e46.02", 0xc00000, 0x200000, CRC(fb046018) SHA1(48d9c582ec9ef59dcc7538598fbd7ea2117896af) )
	ROM_LOAD       ("e46.01", 0xe00000, 0x200000, CRC(34fc2103) SHA1(dca199bbd0ad28a11960101cda8d110943b10822) )
	ROM_FILL       (          0x800000, 0x400000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e46.12", 0x000000, 0x100000, CRC(1f3a9851) SHA1(c8711e2ef0096b41cc9b4c41e521d44b824f7181) )
	ROM_LOAD16_BYTE("e46.11", 0x000001, 0x100000, CRC(e9f10bf1) SHA1(4ee9be3846b262dc0992c904c40580353b164d46) )
	ROM_LOAD       ("e46.10", 0x300000, 0x100000, CRC(1999b76a) SHA1(83d6d2efe250bf3b119982bbf701f9b9d856af2d) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e46.19", 0x100000, 0x40000, CRC(2624eba0) SHA1(ba0b13bda241c648c7f8520106acd8c0c888fe29) )
	ROM_LOAD16_BYTE("e46.20", 0x100001, 0x40000, CRC(065e934f) SHA1(0ec1b5ae33b1c43776b9327c9d380787d64ed5f9) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e46.07", 0x400000, 0x200000, CRC(f20af91e) SHA1(86040ff7ce591418b32c06c3a02fabcbe76281f5) )
	ROM_RELOAD(               0x800000, 0x200000 ) /*fix sound*/
	ROM_LOAD16_BYTE("e46.08", 0xc00000, 0x200000, CRC(f7f96e1d) SHA1(8a83ea9036e8647b8dec6b5e144288ed9c025779) )
	ROM_LOAD16_BYTE("e46.09", 0x000000, 0x200000, CRC(824135f8) SHA1(13e9edeac38e63fa27d9fd7892d51c216f36ec30) )
ROM_END

ROM_START( pbobble4 )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e49.12", 0x000000, 0x80000, CRC(fffea203) SHA1(6351209c78099f270c8d8c8aa4a2dd9ce126c4ed) )
	ROM_LOAD32_BYTE("e49.11", 0x000001, 0x80000, CRC(bf69a087) SHA1(2acbdb7faf5bdb1d9b5b9506c0b6f02fedcbd6a5) )
	ROM_LOAD32_BYTE("e49.10", 0x000002, 0x80000, CRC(0307460b) SHA1(7ad9c6e5d319d6727444ffd14a87c6885445cee0) )
	ROM_LOAD32_BYTE("e49.16", 0x000003, 0x80000, CRC(0a021624) SHA1(21a948f9f4adce0aaf76292f419a7c289f265d30) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e49.02", 0x000000, 0x100000, CRC(c7d2f40b) SHA1(cc6813189d6b31d7db099e49443af395f137462c) )
	ROM_LOAD16_BYTE("e49.01", 0x000001, 0x100000, CRC(a3dd5f85) SHA1(2b305fdc18806bb5d7c3de0ac6a6eb07f98b4d3d) )
	ROM_FILL       (          0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e49.08", 0x000000, 0x100000, CRC(87408106) SHA1(6577568ab4b92d6a81f43cf9ea2f0e30e17e2742) )
	ROM_LOAD16_BYTE("e49.07", 0x000001, 0x100000, CRC(1e1e8e1c) SHA1(9c3b994064c6af62b6a24cab85089a74fd92cf7f) )
	ROM_LOAD       ("e49.06", 0x300000, 0x100000, CRC(ec85f7ce) SHA1(9fead68c38fc9ca84d34d70343b13665978c362b) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("e49.13", 0x100000, 0x40000, CRC(83536f7f) SHA1(2252cee00ae260954cc76d92af8eb2a87d23cbfb) )
	ROM_LOAD16_BYTE("e49.14", 0x100001, 0x40000, CRC(19815bdb) SHA1(38ad682236c7df0710055dd8dbdec30d5da0839d) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e49.03", 0x000000, 0x200000, CRC(f64303e0) SHA1(4d5df77047522419d21ff36402076e9b7c5acff8) )
	ROM_LOAD16_BYTE("e49.04", 0x800000, 0x200000, CRC(09be229c) SHA1(a3a88969b34628d2bf3163bdf85d520feac9a7ac) )
	ROM_LOAD16_BYTE("e49.05", 0x400000, 0x200000, CRC(5ce90ee2) SHA1(afafc1f64ecf2dbd94a9f7871a26150ac2d22be5) )
	ROM_RELOAD(               0xc00000, 0x200000 ) /*fix sound*/
ROM_END

ROM_START( pbobbl4j )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e49.12", 0x000000, 0x80000, CRC(fffea203) SHA1(6351209c78099f270c8d8c8aa4a2dd9ce126c4ed) )
	ROM_LOAD32_BYTE("e49.11", 0x000001, 0x80000, CRC(bf69a087) SHA1(2acbdb7faf5bdb1d9b5b9506c0b6f02fedcbd6a5) )
	ROM_LOAD32_BYTE("e49.10", 0x000002, 0x80000, CRC(0307460b) SHA1(7ad9c6e5d319d6727444ffd14a87c6885445cee0) )
	ROM_LOAD32_BYTE("e49-09.17", 0x000003, 0x80000, CRC(e40c7708) SHA1(0a8e973bb1d8c6dea9124d2742d354c6f20c08ee) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e49.02", 0x000000, 0x100000, CRC(c7d2f40b) SHA1(cc6813189d6b31d7db099e49443af395f137462c) )
	ROM_LOAD16_BYTE("e49.01", 0x000001, 0x100000, CRC(a3dd5f85) SHA1(2b305fdc18806bb5d7c3de0ac6a6eb07f98b4d3d) )
	ROM_FILL       (          0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e49.08", 0x000000, 0x100000, CRC(87408106) SHA1(6577568ab4b92d6a81f43cf9ea2f0e30e17e2742) )
	ROM_LOAD16_BYTE("e49.07", 0x000001, 0x100000, CRC(1e1e8e1c) SHA1(9c3b994064c6af62b6a24cab85089a74fd92cf7f) )
	ROM_LOAD       ("e49.06", 0x300000, 0x100000, CRC(ec85f7ce) SHA1(9fead68c38fc9ca84d34d70343b13665978c362b) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("e49.13", 0x100000, 0x40000, CRC(83536f7f) SHA1(2252cee00ae260954cc76d92af8eb2a87d23cbfb) )
	ROM_LOAD16_BYTE("e49.14", 0x100001, 0x40000, CRC(19815bdb) SHA1(38ad682236c7df0710055dd8dbdec30d5da0839d) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e49.03", 0x000000, 0x200000, CRC(f64303e0) SHA1(4d5df77047522419d21ff36402076e9b7c5acff8) )
	ROM_LOAD16_BYTE("e49.04", 0x800000, 0x200000, CRC(09be229c) SHA1(a3a88969b34628d2bf3163bdf85d520feac9a7ac) )
	ROM_LOAD16_BYTE("e49.05", 0x400000, 0x200000, CRC(5ce90ee2) SHA1(afafc1f64ecf2dbd94a9f7871a26150ac2d22be5) )
	ROM_RELOAD(               0xc00000, 0x200000 ) /*fix sound*/
ROM_END

ROM_START( pbobbl4u )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e49.12", 0x000000, 0x80000, CRC(fffea203) SHA1(6351209c78099f270c8d8c8aa4a2dd9ce126c4ed) )
	ROM_LOAD32_BYTE("e49.11", 0x000001, 0x80000, CRC(bf69a087) SHA1(2acbdb7faf5bdb1d9b5b9506c0b6f02fedcbd6a5) )
	ROM_LOAD32_BYTE("e49.10", 0x000002, 0x80000, CRC(0307460b) SHA1(7ad9c6e5d319d6727444ffd14a87c6885445cee0) )
	ROM_LOAD32_BYTE("e49-15.17", 0x000003, 0x80000, CRC(7d0526b2) SHA1(1b769f735735e9135418ff26c020d8ac7f62d857) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e49.02", 0x000000, 0x100000, CRC(c7d2f40b) SHA1(cc6813189d6b31d7db099e49443af395f137462c) )
	ROM_LOAD16_BYTE("e49.01", 0x000001, 0x100000, CRC(a3dd5f85) SHA1(2b305fdc18806bb5d7c3de0ac6a6eb07f98b4d3d) )
	ROM_FILL       (          0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e49.08", 0x000000, 0x100000, CRC(87408106) SHA1(6577568ab4b92d6a81f43cf9ea2f0e30e17e2742) )
	ROM_LOAD16_BYTE("e49.07", 0x000001, 0x100000, CRC(1e1e8e1c) SHA1(9c3b994064c6af62b6a24cab85089a74fd92cf7f) )
	ROM_LOAD       ("e49.06", 0x300000, 0x100000, CRC(ec85f7ce) SHA1(9fead68c38fc9ca84d34d70343b13665978c362b) )
	ROM_FILL       (          0x200000, 0x100000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 code */
	ROM_LOAD16_BYTE("e49.13", 0x100000, 0x40000, CRC(83536f7f) SHA1(2252cee00ae260954cc76d92af8eb2a87d23cbfb) )
	ROM_LOAD16_BYTE("e49.14", 0x100001, 0x40000, CRC(19815bdb) SHA1(38ad682236c7df0710055dd8dbdec30d5da0839d) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e49.03", 0x000000, 0x200000, CRC(f64303e0) SHA1(4d5df77047522419d21ff36402076e9b7c5acff8) )
	ROM_LOAD16_BYTE("e49.04", 0x800000, 0x200000, CRC(09be229c) SHA1(a3a88969b34628d2bf3163bdf85d520feac9a7ac) )
	ROM_LOAD16_BYTE("e49.05", 0x400000, 0x200000, CRC(5ce90ee2) SHA1(afafc1f64ecf2dbd94a9f7871a26150ac2d22be5) )
	ROM_RELOAD(               0xc00000, 0x200000 ) /*fix sound*/
ROM_END

ROM_START( popnpopj )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e51-12.20", 0x000000, 0x80000, CRC(86a237d5) SHA1(4b87a51705a4d831b21ee770af17310c6c091c2e) )
	ROM_LOAD32_BYTE("e51-11.19", 0x000001, 0x80000, CRC(8a49f34f) SHA1(8691fbc1e96f0c9eb550dbb8ae8d7ef371397166) )
	ROM_LOAD32_BYTE("e51-10.18", 0x000002, 0x80000, CRC(4bce68f8) SHA1(1a9220926f4d8db509f4ccbf318d123f34c42153) )
	ROM_LOAD32_BYTE("e51-09.17", 0x000003, 0x80000, CRC(4a086017) SHA1(edec4120b3d96a179f12949bd261b97d41351cab) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e51-03.12",0x000000, 0x100000, CRC(a24c4607) SHA1(2396fa927446568ad8d98ad6756813e2f30523dd) )
	ROM_LOAD16_BYTE("e51-02.8", 0x000001, 0x100000, CRC(6aa8b96c) SHA1(aaf7917dce5fed43c68cd3065538b58666ef3dbc) )
	ROM_LOAD       ("e51-01.4", 0x300000, 0x100000, CRC(70347e24) SHA1(6b4ab90f0209e50eac7bee3abcf40afb71ab950a) )
	ROM_FILL       (            0x200000, 0x100000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e51-08.47", 0x000000, 0x200000, CRC(3ad41f02) SHA1(a4959113c01062003df41cdf6146e8a034917ee2) )
	ROM_LOAD16_BYTE("e51-07.45", 0x000001, 0x200000, CRC(95873e46) SHA1(02504cbd920c8dbcb5abec6388305eff38f7efe0) )
	ROM_LOAD       ("e51-06.43", 0x600000, 0x200000, CRC(c240d6c8) SHA1(6f3b5224b7eb8783893375d432bbbfc37f81c230) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e51-13.32", 0x100000, 0x40000, CRC(3b9e3986) SHA1(26340bda0ad2ea580e2395135617966676a71ad5) )
	ROM_LOAD16_BYTE("e51-14.33", 0x100001, 0x40000, CRC(1f9a5015) SHA1(5da38c5fe2a50bcde6bd46ab1cb9a56dbab1a882) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e51-04.38", 0x000000, 0x200000, CRC(66790f55) SHA1(ac539b2655dbcda1bdffb9f3cf4c96fb05721e9d) )
	ROM_LOAD16_BYTE("e51-05.41", 0xc00000, 0x200000, CRC(4d08b26d) SHA1(071a11a1b1ee8b8129d02b15ec0e533912c91b04) )
ROM_END

ROM_START( popnpop )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e51-12.20", 0x000000, 0x80000, CRC(86a237d5) SHA1(4b87a51705a4d831b21ee770af17310c6c091c2e) )
	ROM_LOAD32_BYTE("e51-11.19", 0x000001, 0x80000, CRC(8a49f34f) SHA1(8691fbc1e96f0c9eb550dbb8ae8d7ef371397166) )
	ROM_LOAD32_BYTE("e51-10.18", 0x000002, 0x80000, CRC(4bce68f8) SHA1(1a9220926f4d8db509f4ccbf318d123f34c42153) )
	ROM_LOAD32_BYTE("e51-16.17", 0x000003, 0x80000, CRC(2a9d8e0f) SHA1(a5363a98f03cbc7b5f7c393b21062730bd6ee2a8) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e51-03.12",0x000000, 0x100000, CRC(a24c4607) SHA1(2396fa927446568ad8d98ad6756813e2f30523dd) )
	ROM_LOAD16_BYTE("e51-02.8", 0x000001, 0x100000, CRC(6aa8b96c) SHA1(aaf7917dce5fed43c68cd3065538b58666ef3dbc) )
	ROM_LOAD       ("e51-01.4", 0x300000, 0x100000, CRC(70347e24) SHA1(6b4ab90f0209e50eac7bee3abcf40afb71ab950a) )
	ROM_FILL       (            0x200000, 0x100000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e51-08.47", 0x000000, 0x200000, CRC(3ad41f02) SHA1(a4959113c01062003df41cdf6146e8a034917ee2) )
	ROM_LOAD16_BYTE("e51-07.45", 0x000001, 0x200000, CRC(95873e46) SHA1(02504cbd920c8dbcb5abec6388305eff38f7efe0) )
	ROM_LOAD       ("e51-06.43", 0x600000, 0x200000, CRC(c240d6c8) SHA1(6f3b5224b7eb8783893375d432bbbfc37f81c230) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e51-13.32", 0x100000, 0x40000, CRC(3b9e3986) SHA1(26340bda0ad2ea580e2395135617966676a71ad5) )
	ROM_LOAD16_BYTE("e51-14.33", 0x100001, 0x40000, CRC(1f9a5015) SHA1(5da38c5fe2a50bcde6bd46ab1cb9a56dbab1a882) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e51-04.38", 0x000000, 0x200000, CRC(66790f55) SHA1(ac539b2655dbcda1bdffb9f3cf4c96fb05721e9d) )
	ROM_LOAD16_BYTE("e51-05.41", 0xc00000, 0x200000, CRC(4d08b26d) SHA1(071a11a1b1ee8b8129d02b15ec0e533912c91b04) )
ROM_END

ROM_START( popnpopu )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e51-12.20", 0x000000, 0x80000, CRC(86a237d5) SHA1(4b87a51705a4d831b21ee770af17310c6c091c2e) )
	ROM_LOAD32_BYTE("e51-11.19", 0x000001, 0x80000, CRC(8a49f34f) SHA1(8691fbc1e96f0c9eb550dbb8ae8d7ef371397166) )
	ROM_LOAD32_BYTE("e51-10.18", 0x000002, 0x80000, CRC(4bce68f8) SHA1(1a9220926f4d8db509f4ccbf318d123f34c42153) )
	ROM_LOAD32_BYTE("e51-usa.17", 0x000003, 0x80000, CRC(1ad77903) SHA1(d5e631d70108d1f15bcfcacde914ac2fb95cb102) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e51-03.12",0x000000, 0x100000, CRC(a24c4607) SHA1(2396fa927446568ad8d98ad6756813e2f30523dd) )
	ROM_LOAD16_BYTE("e51-02.8", 0x000001, 0x100000, CRC(6aa8b96c) SHA1(aaf7917dce5fed43c68cd3065538b58666ef3dbc) )
	ROM_LOAD       ("e51-01.4", 0x300000, 0x100000, CRC(70347e24) SHA1(6b4ab90f0209e50eac7bee3abcf40afb71ab950a) )
	ROM_FILL       (            0x200000, 0x100000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e51-08.47", 0x000000, 0x200000, CRC(3ad41f02) SHA1(a4959113c01062003df41cdf6146e8a034917ee2) )
	ROM_LOAD16_BYTE("e51-07.45", 0x000001, 0x200000, CRC(95873e46) SHA1(02504cbd920c8dbcb5abec6388305eff38f7efe0) )
	ROM_LOAD       ("e51-06.43", 0x600000, 0x200000, CRC(c240d6c8) SHA1(6f3b5224b7eb8783893375d432bbbfc37f81c230) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e51-13.32", 0x100000, 0x40000, CRC(3b9e3986) SHA1(26340bda0ad2ea580e2395135617966676a71ad5) )
	ROM_LOAD16_BYTE("e51-14.33", 0x100001, 0x40000, CRC(1f9a5015) SHA1(5da38c5fe2a50bcde6bd46ab1cb9a56dbab1a882) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e51-04.38", 0x000000, 0x200000, CRC(66790f55) SHA1(ac539b2655dbcda1bdffb9f3cf4c96fb05721e9d) )
	ROM_LOAD16_BYTE("e51-05.41", 0xc00000, 0x200000, CRC(4d08b26d) SHA1(071a11a1b1ee8b8129d02b15ec0e533912c91b04) )
ROM_END

ROM_START( landmakr )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("e61-13.20", 0x000000, 0x80000, CRC(0af756a2) SHA1(2dadac6873f2491ee77703f07f00dde2aa909355) )
	ROM_LOAD32_BYTE("e61-12.19", 0x000001, 0x80000, CRC(636b3df9) SHA1(78a5bf4977bb90d710942188ce5016f3df499feb) )
	ROM_LOAD32_BYTE("e61-11.18", 0x000002, 0x80000, CRC(279a0ee4) SHA1(08380286737b33db76a79b27d0df5faba17dfb96) )
	ROM_LOAD32_BYTE("e61-10.17", 0x000003, 0x80000, CRC(daabf2b2) SHA1(dbfbe38841fc2f937052353eff1202790d364b9f) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("e61-03.12",0x000000, 0x200000, CRC(e8abfc46) SHA1(fbde006f9822af3ed8debec525270d329981ea21) )
	ROM_LOAD16_BYTE("e61-02.08",0x000001, 0x200000, CRC(1dc4a164) SHA1(33b412d9653099aaff8ed5e62d1ba4fc30aa9058) )
	ROM_LOAD       ("e61-01.04",0x600000, 0x200000, CRC(6cdd8311) SHA1(7810a5a81f3b5a730d2088c79b12fffd77659b5b) )
	ROM_FILL       (            0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("e61-09.47", 0x000000, 0x200000, CRC(6ba29987) SHA1(b63c12523e19da66b3ca07c3548ac81bf57b59a1) )
	ROM_LOAD16_BYTE("e61-08.45", 0x000001, 0x200000, CRC(76c98e14) SHA1(c021c325ab4ae410fa54e2eab61d34318867432b) )
	ROM_LOAD       ("e61-07.43", 0x600000, 0x200000, CRC(4a57965d) SHA1(8e80788e0f47fb242da9af3aa19077dc0ec829b8) )
	ROM_FILL       (             0x400000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("e61-14.32", 0x100000, 0x20000, CRC(b905f4a7) SHA1(613b954e3e129fd44b4ce64958f16e5636012d6e) )
	ROM_LOAD16_BYTE("e61-15.33", 0x100001, 0x20000, CRC(87909869) SHA1(7b90c23899a673966cac3352d375d17b83e66596) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("e61-04.38", 0x000000, 0x200000, CRC(c27aec0c) SHA1(e95da2db07a20a53662ebd45c033966e8a22a15a) )
	ROM_LOAD16_BYTE("e61-06.40", 0x800000, 0x200000, CRC(2e717bfe) SHA1(1be54cd2ec65d8fd49a5c09b5d27791fd7a320d4) )
	ROM_LOAD16_BYTE("e61-05.39", 0xc00000, 0x200000, CRC(83920d9d) SHA1(019e39ae85d1129f6d3b8460c4b1bd925f868ee2) )
ROM_END

ROM_START( landmkrp )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("mpro-3.60", 0x000000, 0x80000, CRC(f92eccd0) SHA1(88e836390be1ca08c578080662d17007a9e0bcc3) )
	ROM_LOAD32_BYTE("mpro-2.61", 0x000001, 0x80000, CRC(5a26c9e0) SHA1(e7f09722f6b7a459248c2c8ad0a2695365cc78dc) )
	ROM_LOAD32_BYTE("mpro-1.62", 0x000002, 0x80000, CRC(710776a8) SHA1(669aa086e7a5faedd90407e558c01bf5f0869790) )
	ROM_LOAD32_BYTE("mpro-0.63", 0x000003, 0x80000, CRC(bc71dd2f) SHA1(ec0d07f9729a53737975547066bd1221f78563c5) )

	ROM_REGION(0x800000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("obj0-0.8", 0x000000, 0x080000, CRC(4b862c1b) SHA1(ef46af27d0657b95f5f3bad13629f9119958fe78) )
	ROM_LOAD16_BYTE("obj1-0.7", 0x100000, 0x080000, CRC(90502355) SHA1(e1edc0cec8ca53fda4d42f9b9fdd385379d7a958) )
	ROM_LOAD16_BYTE("obj2-0.6", 0x200000, 0x080000, CRC(3bffe4b2) SHA1(6e9bb8716f312cb8c81ecebfc61f9dfc8c9013dc) )
	ROM_LOAD16_BYTE("obj3-0.5", 0x300000, 0x080000, CRC(3a0e1479) SHA1(50430c304c437caadebf04499f49ca6323ebdaba) )
	ROM_LOAD16_BYTE("obj0-1.20",0x000001, 0x080000, CRC(1dc6e1ae) SHA1(1e8fa89b1a8846de1516ca9d2ef9227b4af07e38) )
	ROM_LOAD16_BYTE("obj1-1.19",0x100001, 0x080000, CRC(a24edb24) SHA1(81fe77eccdd2a7ea02454e57e52b21ad57eb817e) )
	ROM_LOAD16_BYTE("obj2-1.18",0x200001, 0x080000, CRC(1b2a87f3) SHA1(b7dc871196b92bb4f6ea31bff0717cb3a508bc05) )
	ROM_LOAD16_BYTE("obj3-1.17",0x300001, 0x080000, CRC(c7e91180) SHA1(c8bfa43ab3b9a6c4ba08e1a7389880e964bb1d80) )
	ROM_LOAD       ("obj0-2.32",0x600000, 0x080000, CRC(94cc01d0) SHA1(f4cf4cb237a3f2bd9df35424f85a84b70b47d402) )
	ROM_LOAD       ("obj1-2.31",0x680000, 0x080000, CRC(c2757722) SHA1(83a921647eb0375e10c7f76c08ebe66f2a6fdcd9) )
	ROM_LOAD       ("obj2-2.30",0x700000, 0x080000, CRC(934556ff) SHA1(aca8585680e66635b8872259cfd38edc96e92066) )
	ROM_LOAD       ("obj3-2.29",0x780000, 0x080000, CRC(97f0f777) SHA1(787a33b91cb262cc3983a46ba259dd9b153d532a) )
	ROM_FILL       (            0x400000, 0x200000, 0 )

	ROM_REGION(0x800000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD32_BYTE("scr0-0.7", 0x000000, 0x080000, CRC(da6ba562) SHA1(6aefd249d3491380837e04583a0069ed9c495d05) )
	ROM_LOAD32_BYTE("scr0-1.6", 0x000002, 0x080000, CRC(8c201d27) SHA1(83147c35d03c7b5c84220a6442e99b87ba99cfbc) )
	ROM_LOAD32_BYTE("scr0-2.5", 0x000001, 0x080000, CRC(36756b9c) SHA1(3d293b11d03fb4cdc5c041fcdade9941bf6a72d0) )
	ROM_LOAD32_BYTE("scr0-3.4", 0x000003, 0x080000, CRC(4e0274f3) SHA1(d65378db78a310c664ef49a216f18e16c932f58d) )
	ROM_LOAD32_BYTE("scr1-0.19",0x200000, 0x080000, CRC(2689f716) SHA1(6849e7d36aca5a678b74e1cce9e6a2381928c127) )
	ROM_LOAD32_BYTE("scr1-1.18",0x200002, 0x080000, CRC(f3086949) SHA1(c21f5384294a9fcfb422dbb85565305520a334b5) )
	ROM_LOAD32_BYTE("scr1-2.17",0x200001, 0x080000, CRC(7841468a) SHA1(58b60cbb4ec7e2d0d64fc42161b53b9ff5e2ca8c) )
	ROM_LOAD32_BYTE("scr1-3.16",0x200003, 0x080000, CRC(926ad229) SHA1(4840227c184bde8d125122a90a70102bf2757ccc) )
	ROM_LOAD16_BYTE("scr0-4.3", 0x600000, 0x080000, CRC(5b3cf564) SHA1(003f1e4c653897016c95dee67161fa3964d4f5a8) )
	ROM_LOAD16_BYTE("scr0-5.2", 0x600001, 0x080000, CRC(8e1ea0fe) SHA1(aa815d1d67bf72be6a0c4076490dfd36f28a82ab) )
	ROM_LOAD16_BYTE("scr1-4.15",0x700000, 0x080000, CRC(783b6d10) SHA1(eab2c7b19890c1f6c13f0062978db5b81988499b) )
	ROM_LOAD16_BYTE("scr1-5.14",0x700001, 0x080000, CRC(24aba128) SHA1(b03804c738d86bfafc1f8fb91f8e77e878d2dc83) )
	ROM_FILL       (            0x400000, 0x200000, 0 )

	ROM_REGION(0x180000, REGION_CPU2, 0)	/* 68000 sound CPU */
	ROM_LOAD16_BYTE("spro-1.66", 0x100000, 0x40000, CRC(18961bbb) SHA1(df054def35a49c0754356c15ec15336cbf28b063) )
	ROM_LOAD16_BYTE("spro-0.65", 0x100001, 0x40000, CRC(2c64557a) SHA1(768007162d5d2cbe650c735bc1af2c10ed13b046) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("snd-0.43", 0x000000, 0x80000, CRC(0e5ef5c8) SHA1(e2840c9cedb9361b7eb307e87ea96f3bb6225487) )
	ROM_LOAD16_BYTE("snd-1.44", 0x100000, 0x80000, CRC(2998fd65) SHA1(192e32f9934465bb0da5c1ad116c5ea9b286f36a) )
	ROM_LOAD16_BYTE("snd-2.45", 0x200000, 0x80000, CRC(da7477ad) SHA1(52c69e86e8f8004d616265f3c1f508e7fac19fdc) )
	ROM_LOAD16_BYTE("snd-3.46", 0x300000, 0x80000, CRC(141670b9) SHA1(d1d75bc9c27481de68b446e397c448b0163a7916) )
	ROM_LOAD16_BYTE("snd-8.20", 0x800000, 0x80000, CRC(d98c275e) SHA1(862f5759d2e41243b8a6a3f27ab2da2a1456d92c) )
	ROM_LOAD16_BYTE("snd-9.21", 0x900000, 0x80000, CRC(82a76cfc) SHA1(a9bdc9b05cfb658165165c3292a698ed0e977ede) )
	ROM_LOAD16_BYTE("snd-10.22",0xa00000, 0x80000, CRC(0345f585) SHA1(de8a9816eba7d4db73a53103479ee9d56889e127) )
	ROM_LOAD16_BYTE("snd-11.23",0xb00000, 0x80000, CRC(4caf571a) SHA1(c209f78362442f8a952c180e3d01a5e8e9d5c71c) )
	ROM_LOAD16_BYTE("snd-4.32", 0x800000, 0x80000, CRC(e9dc18f6) SHA1(c84920246a9967b155e137893c080bce6850db85) )
	ROM_LOAD16_BYTE("snd-5.33", 0x800000, 0x80000, CRC(8af91ca8) SHA1(853d2a036602338539cf25e68eac1e686c0861d5) )
	ROM_LOAD16_BYTE("snd-6.34", 0x800000, 0x80000, CRC(6f520b82) SHA1(c559c80386de08256b2f8cbf198271223a83fdb9) )
	ROM_LOAD16_BYTE("snd-7.35", 0x800000, 0x80000, CRC(69410f0f) SHA1(ff023842383ce26818ec7361831e122737a9e94b) )
ROM_END

ROM_START( recalh )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("rh_mpr3.bin", 0x000000, 0x80000, CRC(65202dd4) SHA1(8d5748d03868b127a7d727e00c1bce51a5bae129) )
	ROM_LOAD32_BYTE("rh_mpr2.bin", 0x000001, 0x80000, CRC(3eda66db) SHA1(6d726762404d85008d6bebe5a77cebe505b650fc) )
	ROM_LOAD32_BYTE("rh_mpr1.bin", 0x000002, 0x80000, CRC(536e74ca) SHA1(2a50bb2e93563273c4b0c0c59143893fe25d007e) )
	ROM_LOAD32_BYTE("rh_mpr0.bin", 0x000003, 0x80000, CRC(38025817) SHA1(fa4cf98cfca95c462b19b873a7660f7cec71cf56) )

	ROM_REGION(0x400000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("rh_objl.bin", 0x000000, 0x100000, CRC(c1772b55) SHA1(f9a04b968c63e61fa8ca60d6f331f6df0d7dd10a) )
	ROM_LOAD16_BYTE("rh_objm.bin", 0x000001, 0x100000, CRC(ef87c0fd) SHA1(63e99f331d05a1ff4faf0ea94019393fe2117f54) )
	ROM_FILL       (               0x200000, 0x200000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("rh_scrl.bin", 0x000000, 0x100000, CRC(1e3f6b79) SHA1(fef029def6393a13f4a638686a7ec7c13851a5c0) )
	ROM_LOAD16_BYTE("rh_scrm.bin", 0x000001, 0x100000, CRC(37200968) SHA1(4a8d5a17af7eb732f481bf174099845e8d8d6b87) )
	ROM_FILL       (               0x200000, 0x200000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("rh_spr1.bin", 0x100000, 0x20000, CRC(504cbc1d) SHA1(35a775c1ebc8107c553e43b9d84eb735446c26fd) )
	ROM_LOAD16_BYTE("rh_spr0.bin", 0x100001, 0x20000, CRC(78fba467) SHA1(4586b061724be7ec413784b820c33cc0d6bbcd0c) )

	ROM_REGION16_BE(0x1000000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("rh_snd0.bin", 0x000000, 0x200000, CRC(386f5e1b) SHA1(d67d5f057c6db3092643f10ea10f977b1caa662f) )
	ROM_RELOAD( 0x400000, 0x200000)
	ROM_LOAD16_BYTE("rh_snd1.bin", 0x800000, 0x100000, CRC(ed894fe1) SHA1(5bf2fb6abdcf25bc525a2c3b29dbf7aca0b18fea) )
	ROM_RELOAD( 0xa00000, 0x100000)
	ROM_RELOAD( 0xc00000, 0x100000)
	ROM_RELOAD( 0xe00000, 0x100000)
ROM_END

ROM_START( commandw )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
 	ROM_LOAD32_BYTE("cw_mpr3.bin", 0x000000, 0x40000, CRC(636944fc) SHA1(47deffed68ac179f27bfdb21ed62d6555a4b8988) )
	ROM_LOAD32_BYTE("cw_mpr2.bin", 0x000001, 0x40000, CRC(1151a42b) SHA1(e938913ecd3211a8fb4041ec5a5694cd9df9be69) )
	ROM_LOAD32_BYTE("cw_mpr1.bin", 0x000002, 0x40000, CRC(93669389) SHA1(11336a15900c4f419f3af5c423fbc502f4db616b) )
	ROM_LOAD32_BYTE("cw_mpr0.bin", 0x000003, 0x40000, CRC(0468df52) SHA1(0da923aa779b541e700c5249272e9c59ab59e863) )

	ROM_REGION(0x1000000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE("cw_objl0.bin",  0x000000, 0x200000, CRC(9822102e) SHA1(c4e80ab4d54c39676ee6e557a03828250077765b) )
  	ROM_LOAD16_BYTE("cw_objm0.bin",  0x000001, 0x200000, CRC(f7687684) SHA1(0bed6362dee96083e2e8b6448c9f7bfa5166bfb7) )
	ROM_LOAD16_BYTE("cw_objl1.bin",  0x400000, 0x200000,  CRC(ca3ad7f6) SHA1(849fbb89f0b132c83db5b7d699078da3cc10baf6) )
  	ROM_LOAD16_BYTE("cw_objm1.bin",  0x400001, 0x200000, CRC(504b1bf5) SHA1(7b8ff7834907a9cdab5416bf713487bf71b9070e) )
	ROM_LOAD       ("cw_objh0.bin",  0xc00000, 0x200000, CRC(83d7e0ae) SHA1(774a07d0cadc2c8f5ec155270bf927e4462654e2) )
	ROM_LOAD       ("cw_objh1.bin",  0xe00000, 0x200000, CRC(324f5832) SHA1(ff91243c5d09c4c46904640fe278a7485db70577) )
	ROM_FILL       (                 0x800000, 0x400000, 0 )

	ROM_REGION(0x400000, REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD16_BYTE("cw_scr_l.bin", 0x000000, 0x100000, CRC(4d202323) SHA1(0150bcba6d2bf2c3cde88bb519f57f3b58314244) )
	ROM_LOAD16_BYTE("cw_scr_m.bin", 0x000001, 0x100000, CRC(537b1c7d) SHA1(bc61aa61891366cbea4b8ecb820d93e28d01f8d2) )
	ROM_LOAD       ("cw_scr_h.bin", 0x300000, 0x100000, CRC(001f85dd) SHA1(2532377c0b54bc964ea4e74911ff62fea2d53975) )
	ROM_FILL       (                0x200000, 0x100000, 0 )

	ROM_REGION(0x140000, REGION_CPU2, 0)	/* sound CPU */
	ROM_LOAD16_BYTE("cw_spr1.bin", 0x100000, 0x20000, CRC(c8f81c25) SHA1(1c914053826587cc2d5d2c0220a3e29a641fe6f9) )
	ROM_LOAD16_BYTE("cw_spr0.bin", 0x100001, 0x20000, CRC(2aaa9dfb) SHA1(6d4c36ff54a84035c0ddf40e4f3eafd2adc15a5e) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("cw_pcm_0.bin", 0x000000, 0x200000, CRC(a1e26629) SHA1(0c5899a767f66f67a5d59b8d287d74b54f8c3727) )
	ROM_LOAD16_BYTE("cw_pcm_1.bin", 0x400000, 0x200000, CRC(39fc6cf4) SHA1(d43ef294af62765bfec089fac1d67ad81e1b06da) )
ROM_END

ROM_START( bublbob2p )
	ROM_REGION(0x200000, REGION_CPU1, 0) /* 68020 code */
	ROM_LOAD32_BYTE("soft-3-8c9b.ic60", 0x000000, 0x40000, CRC(15d0594e) SHA1(7556377355860c3f7f600c2c352e5291da6a62f1) )
	ROM_LOAD32_BYTE("soft-2-0587.ic61", 0x000001, 0x40000, CRC(d1a5231f) SHA1(1e9ccac78f690ef79f933743ce7c4d6fa42f5acd) )
	ROM_LOAD32_BYTE("soft-1-9a9c.ic62", 0x000002, 0x40000, CRC(c11a4d26) SHA1(b327413f5420608f1ccbbac2e8941a82862377c5) )
	ROM_LOAD32_BYTE("soft-0-a523.ic63", 0x000003, 0x40000, CRC(58131f9e) SHA1(d07d34bf079277a48151ef9e5e7c1240a36d1bdb) )

	ROM_REGION(0x200000, REGION_GFX1 , ROMREGION_DISPOSE) /* Sprites */
	ROM_LOAD16_BYTE       ("cq80-obj-0l-c166.ic8",  0x000000, 0x80000, CRC(9bff223b) SHA1(acf22731d91d61aefc3373f78006fd310bb89edf) )
	ROM_LOAD16_BYTE       ("cq80-obj-0m-24f4.ic30", 0x000001, 0x80000, CRC(ee71f643) SHA1(7a2042c6fad8f1b7e0a3ad077d054dc163a22230) )
	ROM_LOAD              ("cq80-obj-0h-990d.ic32", 0x180000, 0x80000, CRC(4d3a78e0) SHA1(b19fb66e6082a68dc8600b8882ba50a3afce27c3) )

	ROM_REGION(0x400000,  REGION_GFX2 , ROMREGION_DISPOSE) /* Tiles */
	ROM_LOAD32_BYTE("cq80-scr0-5ba4.ic7", 0x000000, 0x080000, CRC(044dc38b) SHA1(0bb715c9ae8298c6852c6309d69f769e87ab2fdc) )
	ROM_LOAD32_BYTE("cq80-scr1-a5f3.ic6", 0x000002, 0x080000, CRC(3cf3a3ba) SHA1(da7282104fbd9108bae12fa6722e077d80107d6d) )
	ROM_LOAD32_BYTE("cq80-scr2-cc11.ic5", 0x000001, 0x080000, CRC(b81aa2c7) SHA1(4650c431dc2ed73f1e71337f3e7d4c1837b65bcf) )
	ROM_LOAD32_BYTE("cq80-scr3-4266.ic4", 0x000003, 0x080000, CRC(c114583f) SHA1(ec85e8f4135e48607bb84b810d57d570ef56b228) )
	ROM_LOAD16_BYTE("cq80-scr4-7fe1.ic3", 0x300000, 0x080000, CRC(2bba1728) SHA1(cdd2e651c233a185fcbb3fd8f5eabee2af30f781) )

	ROM_REGION(0x180000, REGION_CPU2, 0) /* sound CPU */
	ROM_LOAD16_BYTE("snd-h-348f.ic66", 0x100000, 0x20000, CRC(f66e60f2) SHA1(b94c97ccde179a69811137c77730c91924236bfe))
	ROM_LOAD16_BYTE("snd-l-4ec1.ic65", 0x100001, 0x20000, CRC(d302d8bc) SHA1(02a2e69d0f4406578b12b05ab25d2abdf5bbba3c) )

	ROM_REGION16_BE(0x800000, REGION_SOUND1 , ROMREGION_SOUNDONLY | ROMREGION_ERASE00  )
	ROM_LOAD16_BYTE("cq80-snd-data0-7b5f.ic43", 0x000000, 0x080000, CRC(bf8f26d3) SHA1(b165fc62ed30ae56d27caffbb0b16321d3c5ef8b) )    /* C8 */
	ROM_LOAD16_BYTE("cq80-snd-data1-933b.ic44", 0x100000, 0x080000, CRC(62b00475) SHA1(d2b44940cefca76897b291d83b5ca8ec18dbe1fa) )    /* C9 */
	
	ROM_LOAD16_BYTE("cq80-snd3-std5-3a9c.ic10", 0x600000, 0x080000, CRC(26312451) SHA1(9f947a11592fd8420fc581914bf16e7ade75390c) )    /* -std- */
	ROM_LOAD16_BYTE("cq80-snd2-std6-a148.ic11", 0x700000, 0x080000, CRC(2edaa9dc) SHA1(72fead505c4f44e5736ff7d545d72dfa37d613e2) )    /* -std- */
ROM_END

/******************************************************************************/

static void tile_decode(int uses_5bpp_tiles)
{
	unsigned char lsb,msb;
	unsigned int offset,i;
	UINT8 *gfx = memory_region(REGION_GFX2);
	int size=memory_region_length(REGION_GFX2);
	int half=size/2,data;

	/* Setup ROM formats:

		Some games will only use 4 or 5 bpp sprites, and some only use 4 bpp tiles,
		I don't believe this is software or prom controlled but simply the unused data lines
		are tied low on the game board if unused.  This is backed up by the fact the palette
		indices are always related to 4 bpp data, even in 6 bpp games.

	*/
	if (uses_5bpp_tiles)
		for (i=half; i<size; i+=2)
			gfx[i+1]=0;

	offset = size/2;
	for (i = size/2+size/4; i<size; i+=2)
	{
		/* Expand 2bits into 4bits format */
		lsb = gfx[i+1];
		msb = gfx[i];

		gfx[offset+0]=((msb&0x02)<<3) | ((msb&0x01)>>0) | ((lsb&0x02)<<4) | ((lsb&0x01)<<1);
		gfx[offset+2]=((msb&0x08)<<1) | ((msb&0x04)>>2) | ((lsb&0x08)<<2) | ((lsb&0x04)>>1);
		gfx[offset+1]=((msb&0x20)>>1) | ((msb&0x10)>>4) | ((lsb&0x20)<<0) | ((lsb&0x10)>>3);
		gfx[offset+3]=((msb&0x80)>>3) | ((msb&0x40)>>6) | ((lsb&0x80)>>2) | ((lsb&0x40)>>5);

		offset+=4;
	}

	gfx = memory_region(REGION_GFX1);
	size=memory_region_length(REGION_GFX1);

	offset = size/2;
	for (i = size/2+size/4; i<size; i++)
	{
		int d1,d2,d3,d4;

		/* Expand 2bits into 4bits format */
		data = gfx[i];
		d1 = (data>>0) & 3;
		d2 = (data>>2) & 3;
		d3 = (data>>4) & 3;
		d4 = (data>>6) & 3;

		gfx[offset] = (d1<<2) | (d2<<6);
		offset++;

		gfx[offset] = (d3<<2) | (d4<<6);
		offset++;
	}
	state_save_register_UINT32("f3", 0, "coinword", coin_word, 2);
}

#define F3_IRQ_SPEEDUP_1_R(GAME, counter, mem_addr, mask) 		\
static READ32_HANDLER( irq_speedup_r_##GAME )					\
{																\
	if (activecpu_get_pc()==counter && (f3_ram[mem_addr]&mask)!=0)	\
		cpu_spinuntil_int();									\
	return f3_ram[mem_addr];									\
}

#define F3_IRQ_SPEEDUP_2_R(GAME, counter, mem_addr, mask) 		\
static READ32_HANDLER( irq_speedup_r_##GAME )					\
{																\
	if (activecpu_get_pc()==counter && (f3_ram[mem_addr]&mask)==0)	\
		cpu_spinuntil_int();									\
	return f3_ram[mem_addr];									\
}

#define F3_IRQ_SPEEDUP_3_R(GAME, counter, mem_addr, stack) 		\
static READ32_HANDLER( irq_speedup_r_##GAME )					\
{																\
	int ptr;													\
	if ((activecpu_get_sp()&2)==0) ptr=f3_ram[(activecpu_get_sp()&0x1ffff)/4];	\
	else ptr=(((f3_ram[(activecpu_get_sp()&0x1ffff)/4])&0x1ffff)<<16) | \
	(f3_ram[((activecpu_get_sp()&0x1ffff)/4)+1]>>16); 				\
	if (activecpu_get_pc()==counter && ptr==stack)					\
		cpu_spinuntil_int();									\
	return f3_ram[mem_addr];									\
}

F3_IRQ_SPEEDUP_2_R(arabianm, 0x238,    0x8124/4, 0xff000000 )
F3_IRQ_SPEEDUP_1_R(gseeker,  0x43ac,   0xad94/4, 0xffff0000 )
F3_IRQ_SPEEDUP_1_R(gunlock,  0x646,    0x0004/4, 0xffffffff )
F3_IRQ_SPEEDUP_2_R(cupfinal, 0x254,    0x8114/4, 0x0000ff00 )
F3_IRQ_SPEEDUP_2_R(scfinals, 0x25a,    0x8114/4, 0x0000ff00 )
F3_IRQ_SPEEDUP_1_R(lightbr,  0xe0b02,  0x0130/4, 0x000000ff )
F3_IRQ_SPEEDUP_2_R(kaiserkn, 0x256,    0x8110/4, 0xff000000 )
F3_IRQ_SPEEDUP_1_R(spcinvdj, 0x60b4e,  0x0230/4, 0x000000ff )
F3_IRQ_SPEEDUP_2_R(pwrgoal,  0x234,    0x8114/4, 0x0000ff00 )
F3_IRQ_SPEEDUP_2_R(spcinv95, 0x25a,    0x8114/4, 0x0000ff00 )
F3_IRQ_SPEEDUP_2_R(ktiger2,  0x5ba,    0x0570/4, 0x0000ffff )
F3_IRQ_SPEEDUP_1_R(bubsymph, 0xe9a3e,  0x0134/4, 0x000000ff )
F3_IRQ_SPEEDUP_1_R(bubblem,  0x100a62, 0x0134/4, 0x000000ff )
F3_IRQ_SPEEDUP_2_R(cleopatr, 0x254,    0x8114/4, 0x0000ff00 )
F3_IRQ_SPEEDUP_3_R(pbobble2, 0x2c2c,   0x4a50/4, 0x00002900 )
F3_IRQ_SPEEDUP_3_R(pbobbl2x, 0x2c4c,   0x5c58/4, 0x00002920 )
F3_IRQ_SPEEDUP_3_R(pbobble3, 0xf22,    0x5af4/4, 0x0000159e )
F3_IRQ_SPEEDUP_3_R(pbobble4, 0xf8a,    0x58f4/4, 0x000015ee )
F3_IRQ_SPEEDUP_3_R(gekirido, 0x1da8,   0x6bb0/4, 0x00001a90 )
F3_IRQ_SPEEDUP_3_R(dariusg,  0x1d8e,   0x6ba8/4, 0x00001a76 )
F3_IRQ_SPEEDUP_2_R(puchicar, 0x9dc,    0x24d8/4, 0x80000000 )
F3_IRQ_SPEEDUP_2_R(popnpop,  0x9bc,    0x1cf8/4, 0x00008000 )
F3_IRQ_SPEEDUP_2_R(arkretrn, 0x960,    0x2154/4, 0x0000ffff )
F3_IRQ_SPEEDUP_3_R(landmakr, 0x146c,   0x0824/4, 0x00001178 )
F3_IRQ_SPEEDUP_3_R(eaction2, 0x133c,   0x07a0/4, 0x00001048 )
F3_IRQ_SPEEDUP_1_R(twinqix,  0xe9a52,  0x0134/4, 0x000000ff )
F3_IRQ_SPEEDUP_2_R(kirameki, 0x12fc6,  0x0414/4, 0x0000ff00 )

static DRIVER_INIT( ringrage )
{
	f3_game=RINGRAGE;
	tile_decode(0);
}

static DRIVER_INIT( arabianm )
{
	install_mem_read32_handler(0, 0x408124, 0x408127, irq_speedup_r_arabianm );
	f3_game=ARABIANM;
	tile_decode(1);
}

static DRIVER_INIT( ridingf )
{
	f3_game=RIDINGF;
	tile_decode(1);
}

static DRIVER_INIT( gseeker )
{
	install_mem_read32_handler(0, 0x40ad94, 0x40ad97, irq_speedup_r_gseeker );
	f3_game=GSEEKER;
	tile_decode(0);
}

static DRIVER_INIT( gunlock )
{
	install_mem_read32_handler(0, 0x400004, 0x400007, irq_speedup_r_gunlock );
	f3_game=GUNLOCK;
	tile_decode(1);
}

static DRIVER_INIT( elvactr )
{
	install_mem_read32_handler(0, 0x4007a0, 0x4007a3, irq_speedup_r_eaction2 );
	f3_game=EACTION2;
	tile_decode(1);
}

static DRIVER_INIT( cupfinal )
{
	install_mem_read32_handler(0, 0x408114, 0x408117, irq_speedup_r_cupfinal );
	f3_game=SCFINALS;
	tile_decode(1);
}

static DRIVER_INIT( trstaroj )
{
	f3_game=TRSTAR;
	tile_decode(1);
}

static DRIVER_INIT( scfinals )
{
	data32_t *RAM = (UINT32 *)memory_region(REGION_CPU1);

	/* Doesn't boot without this - eprom related? */
    RAM[0x5af0/4]=0x4e710000|(RAM[0x5af0/4]&0xffff);

	/* Rom checksum error */
	RAM[0xdd0/4]=0x4e750000;

	install_mem_read32_handler(0, 0x408114, 0x408117, irq_speedup_r_scfinals );
	f3_game=SCFINALS;
	tile_decode(1);
}

static DRIVER_INIT( lightbr )
{
	install_mem_read32_handler(0, 0x400130, 0x400133, irq_speedup_r_lightbr );
	f3_game=LIGHTBR;
	tile_decode(1);
}

static DRIVER_INIT( kaiserkn )
{
	install_mem_read32_handler(0, 0x408110, 0x408113, irq_speedup_r_kaiserkn );
	f3_game=KAISERKN;
	tile_decode(1);
}

static DRIVER_INIT( dariusg )
{
	install_mem_read32_handler(0, 0x406ba8, 0x406bab, irq_speedup_r_dariusg );
	f3_game=DARIUSG;
	tile_decode(0);
}

static DRIVER_INIT( spcinvdj )
{
	install_mem_read32_handler(0, 0x400230, 0x400233, irq_speedup_r_spcinvdj );
	f3_game=SPCINVDX;
	tile_decode(0);
}

static DRIVER_INIT( qtheater )
{
	f3_game=QTHEATER;
	tile_decode(0);
}

static DRIVER_INIT( spcinv95 )
{
	install_mem_read32_handler(0, 0x408114, 0x408117, irq_speedup_r_spcinv95 );
	f3_game=SPCINV95;
	tile_decode(1);
}

static DRIVER_INIT( gekirido )
{
	install_mem_read32_handler(0, 0x406bb0, 0x406bb3, irq_speedup_r_gekirido );
	f3_game=GEKIRIDO;
	tile_decode(1);
}

static DRIVER_INIT( ktiger2 )
{
	install_mem_read32_handler(0, 0x400570, 0x400573, irq_speedup_r_ktiger2 );
	f3_game=KTIGER2;
	tile_decode(0);
}

static DRIVER_INIT( bubsymph )
{
	install_mem_read32_handler(0, 0x400134, 0x400137, irq_speedup_r_bubsymph );
	f3_game=BUBSYMPH;
	tile_decode(1);
}

static DRIVER_INIT( bubblem )
{
	install_mem_read32_handler(0, 0x400134, 0x400137, irq_speedup_r_bubblem );
	f3_game=BUBBLEM;
	tile_decode(1);
}

static DRIVER_INIT( cleopatr )
{
	install_mem_read32_handler(0, 0x408114, 0x408117, irq_speedup_r_cleopatr );
	f3_game=CLEOPATR;
	tile_decode(0);
}

static DRIVER_INIT( popnpop )
{
	install_mem_read32_handler(0, 0x401cf8, 0x401cfb, irq_speedup_r_popnpop );
	f3_game=POPNPOP;
	tile_decode(0);
}

static DRIVER_INIT( landmakr )
{
	install_mem_read32_handler(0, 0x400824, 0x400827, irq_speedup_r_landmakr );
	f3_game=LANDMAKR;
	tile_decode(0);
}

static DRIVER_INIT( landmkrp )
{
	data32_t *RAM = (UINT32 *)memory_region(REGION_CPU1);

	/* For some reason the least significant byte in the last 2 long words of
	ROM is swapped.  As the roms have been verified ok, I assume this is some
	kind of basic security on the prototype development board to prevent 'release'
	roms running on it.  Easiest thing to do is switch the data around here */
	RAM[0x1ffff8/4]=0xffffffff; /* From 0xffffff03 */
	RAM[0x1ffffc/4]=0xffff0003; /* From 0xffff00ff */

	install_mem_read32_handler(0, 0x400824, 0x400827, irq_speedup_r_landmakr );
	f3_game=LANDMAKR;
	tile_decode(0);
}

static DRIVER_INIT( pbobble3 )
{
	install_mem_read32_handler(0, 0x405af4, 0x405af7, irq_speedup_r_pbobble3 );
	f3_game=PBOBBLE3;
	tile_decode(0);
}

static DRIVER_INIT( pbobble4 )
{
	install_mem_read32_handler(0, 0x4058f4, 0x4058f7, irq_speedup_r_pbobble4 );
	f3_game=PBOBBLE4;
	tile_decode(0);
}

static DRIVER_INIT( quizhuhu )
{
	f3_game=QUIZHUHU;
	tile_decode(0);
}

static DRIVER_INIT( pbobble2 )
{
	install_mem_read32_handler(0, 0x404a50, 0x404a53, irq_speedup_r_pbobble2 );
	f3_game=PBOBBLE2;
	tile_decode(0);
}

static DRIVER_INIT( pbobbl2x )
{
	install_mem_read32_handler(0, 0x405c58, 0x405c5b, irq_speedup_r_pbobbl2x );
	f3_game=PBOBBLE2;
	tile_decode(0);
}

static DRIVER_INIT( hthero95 )
{
	install_mem_read32_handler(0, 0x408114, 0x408117, irq_speedup_r_pwrgoal );
	f3_game=HTHERO95;
	tile_decode(0);
}

static DRIVER_INIT( kirameki )
{
	install_mem_read32_handler(0, 0x400414, 0x400417, irq_speedup_r_kirameki );
	f3_game=KIRAMEKI;
	tile_decode(0);
}

static DRIVER_INIT( puchicar )
{
	install_mem_read32_handler(0, 0x4024d8, 0x4024db, irq_speedup_r_puchicar );
	f3_game=PUCHICAR;
	tile_decode(0);
}

static DRIVER_INIT( twinqix )
{
	install_mem_read32_handler(0, 0x400134, 0x400137, irq_speedup_r_twinqix );
	f3_game=TWINQIX;
	tile_decode(0);
}

static DRIVER_INIT( arkretrn )
{
	install_mem_read32_handler(0, 0x402154, 0x402157, irq_speedup_r_arkretrn );
	f3_game=ARKRETRN;
	tile_decode(0);
}

static DRIVER_INIT( intcup94 )
{
	f3_game=SCFINALS;
	tile_decode(1);
}

static DRIVER_INIT( recalh )
{
	f3_game=RECALH;
	tile_decode(0);
}

static DRIVER_INIT( commandw )
{
	f3_game=COMMANDW;
	tile_decode(1);
}

/******************************************************************************/

GAME( 1992, ringrage, 0,        ringrage,f3, ringrage, ROT0,   "Taito Corporation Japan",   "Ring Rage (World)" )
GAME( 1992, ringragj, ringrage, ringrage,f3, ringrage, ROT0,   "Taito Corporation",         "Ring Rage (Japan)" )
GAME( 1992, ringragu, ringrage, ringrage,f3, ringrage, ROT0,   "Taito America Corporation", "Ring Rage (US)" )
GAME( 1992, arabianm, 0,        f3_224a, f3, arabianm, ROT0,   "Taito Corporation Japan",   "Arabian Magic (World)" )
GAME( 1992, arabiamj, arabianm, f3_224a, f3, arabianm, ROT0,   "Taito Corporation",         "Arabian Magic (Japan)" )
GAME( 1992, arabiamu, arabianm, f3_224a, f3, arabianm, ROT0,   "Taito America Corporation", "Arabian Magic (US)" )
GAME( 1992, ridingf,  0,        ridingf, f3, ridingf,  ROT0,   "Taito Corporation Japan",   "Riding Fight (World)" )
GAME( 1992, ridefgtj, ridingf,  ridingf, f3, ridingf,  ROT0,   "Taito Corporation",         "Riding Fight (Japan)" )
GAME( 1992, ridefgtu, ridingf,  ridingf, f3, ridingf,  ROT0,   "Taito America Corporation", "Riding Fight (US)" )
GAME( 1992, gseeker,  0,        f3_224b, f3, gseeker,  ROT90,  "Taito Corporation Japan",   "Grid Seeker - Project Stormhammer (World)" )
GAME( 1992, gseekerj, gseeker,  f3_224b, f3, gseeker,  ROT90,  "Taito Corporation",         "Grid Seeker - Project Stormhammer (Japan)" )
GAME( 1992, gseekeru, gseeker,  f3_224b, f3, gseeker,  ROT90,  "Taito America Corporation", "Grid Seeker - Project Stormhammer (US)" )
GAME( 1993, gunlock,  0,        f3_224a, f3, gunlock,  ROT90,  "Taito Corporation Japan",   "Gunlock (World)" )
GAME( 1993, rayforcj, gunlock,  f3_224a, f3, gunlock,  ROT90,  "Taito Corporation",         "Rayforce (Japan)" )
GAME( 1993, rayforce, gunlock,  f3_224a, f3, gunlock,  ROT90,  "Taito America Corporation", "Rayforce (US)" )
GAME( 1993, scfinals, 0,        f3_224a, f3, scfinals, ROT0,   "Taito Corporation Japan",   "Super Cup Finals (World)" )
/* I don't think these really are clones of SCFinals - SCFinals may be a sequel that just shares graphics roms (Different Taito ROM code) */
GAME( 1992, hthero93, scfinals, f3_224a, f3, cupfinal, ROT0,   "Taito Corporation",         "Hat Trick Hero '93 (Japan)" )
GAME( 1993, cupfinal, scfinals, f3_224a, f3, cupfinal, ROT0,   "Taito Corporation Japan",   "Taito Cup Finals (World)" )
GAME( 1994, intcup94, scfinals, f3_224a, f3, intcup94, ROT0,   "Taito Corporation Japan",   "International Cup '94" )
GAME( 1993, trstar,   0,        f3,      f3, trstaroj, ROT0,   "Taito Corporation Japan",   "Top Ranking Stars (World new version)" )
GAME( 1993, trstarj,  trstar,   f3,      f3, trstaroj, ROT0,   "Taito Corporation",         "Top Ranking Stars (Japan new version)" )
GAME( 1993, prmtmfgt, trstar,   f3,      f3, trstaroj, ROT0,   "Taito America Corporation", "Prime Time Fighter (US new version)" )
GAME( 1993, trstaro,  trstar,   f3,      f3, trstaroj, ROT0,   "Taito Corporation Japan",   "Top Ranking Stars (World old version)" )
GAME( 1993, trstaroj, trstar,   f3,      f3, trstaroj, ROT0,   "Taito Corporation",         "Top Ranking Stars (Japan old version)" )
GAME( 1993, prmtmfgo, trstar,   f3,      f3, trstaroj, ROT0,   "Taito America Corporation", "Prime Time Fighter (US old version)" )
GAME( 1993, dungeonm, 0,        f3_224a, f3, lightbr,  ROT0,   "Taito Corporation Japan",   "Dungeon Magic (World)" )
GAME( 1993, lightbr,  dungeonm, f3_224a, f3, lightbr,  ROT0,   "Taito Corporation",         "Light Bringer (Japan)" )
GAME( 1993, dungenmu, dungeonm, f3_224a, f3, lightbr,  ROT0,   "Taito America Corporation", "Dungeon Magic (US)" )
GAME( 1994, kaiserkn, 0,        f3_224a, kn, kaiserkn, ROT0,   "Taito Corporation Japan",   "Kaiser Knuckle (World)" )
GAME( 1994, kaiserkj, kaiserkn, f3_224a, kn, kaiserkn, ROT0,   "Taito Corporation",         "Kaiser Knuckle (Japan)" )
GAME( 1994, gblchmp,  kaiserkn, f3_224a, kn, kaiserkn, ROT0,   "Taito America Corporation", "Global Champion (US)" )
GAME( 1994, dankuga,  kaiserkn, f3_224a, kn, kaiserkn, ROT0,   "Taito Corporation",         "Dan-Ku-Ga (Prototype)" )
GAME( 1994, dariusg,  0,        f3,      f3, dariusg,  ROT0,   "Taito Corporation Japan",   "Darius Gaiden - Silver Hawk (World)" )
GAME( 1994, dariusgj, dariusg,  f3,      f3, dariusg,  ROT0,   "Taito Corporation",         "Darius Gaiden - Silver Hawk (Japan)" )
GAME( 1994, dariusgu, dariusg,  f3,      f3, dariusg,  ROT0,   "Taito America Corporation", "Darius Gaiden - Silver Hawk (US)" )
GAME( 1994, dariusgx, dariusg,  f3,      f3, dariusg,  ROT0,   "Taito Corporation",         "Darius Gaiden - Silver Hawk (Extra Version) [Official Hack]" )
GAME( 1994, bublbob2, 0,        f3_224a, f3, bubsymph, ROT0,   "Taito Corporation Japan",   "Bubble Bobble 2 (World)" )
GAME( 1994, bublbob2p,bublbob2, f3_224a, f3, bubsymph, ROT0,   "Taito Corporation Japan",   "Bubble Bobble 2 (prototype)")
GAME( 1994, bubsympe, bublbob2, f3_224a, f3, bubsymph, ROT0,   "Taito Corporation Japan",   "Bubble Symphony (Europe)" )
GAME( 1994, bubsympu, bublbob2, f3_224a, f3, bubsymph, ROT0,   "Taito America Corporation", "Bubble Symphony (US)" )
GAME( 1994, bubsymph, bublbob2, f3_224a, f3, bubsymph, ROT0,   "Taito Corporation",         "Bubble Symphony (Japan)" )
GAME( 1994, spcinvdj, spacedx,  f3,      f3, spcinvdj, ROT0,   "Taito Corporation",         "Space Invaders DX (Japan F3 version)" )
GAME( 1994, pwrgoal,  0,        f3_224a, f3, hthero95, ROT0,   "Taito Corporation Japan",   "Taito Power Goal (World)" )
GAME( 1994, hthero95, pwrgoal,  f3_224a, f3, hthero95, ROT0,   "Taito Corporation",         "Hat Trick Hero '95 (Japan)" )
GAME( 1994, hthro95u, pwrgoal,  f3_224a, f3, hthero95, ROT0,   "Taito America Corporation", "Hat Trick Hero '95 (US)" )
GAMEX(1994, qtheater, 0,        f3_224c, f3, qtheater, ROT0,   "Taito Corporation",         "Quiz Theater - 3tsu no Monogatari (Japan)", GAME_IMPERFECT_SOUND )
GAME( 1994, elvactr,  0,        f3,      f3, elvactr,  ROT0,   "Taito Corporation Japan",   "Elevator Action Returns (World)" )
GAME( 1994, elvactrj, elvactr,  f3,      f3, elvactr,  ROT0,   "Taito Corporation",         "Elevator Action Returns (Japan)" )
GAME( 1994, elvact2u, elvactr,  f3,      f3, elvactr,  ROT0,   "Taito America Corporation", "Elevator Action 2 (US)" )
/* There is also a prototype Elevator Action 2 (US) pcb with the graphics in a different rom format (same program code) */
GAME( 1995, spcinv95, 0,        f3_224a, f3, spcinv95, ROT270, "Taito Corporation Japan",   "Space Invaders '95 - Attack Of The Lunar Loonies (World)" )
GAME( 1995, spcnv95u, spcinv95, f3_224a, f3, spcinv95, ROT270, "Taito America Corporation", "Space Invaders '95 - Attack Of The Lunar Loonies (US)" )
GAME( 1995, akkanvdr, spcinv95, f3_224a, f3, spcinv95, ROT270, "Taito Corporation",         "Akkanvader (Japan)" )
GAME( 1995, twinqix,  0,        f3_224a, f3, twinqix,  ROT0,   "Taito America Corporation", "Twin Qix (US Prototype)" )
GAME( 1995, gekirido, 0,        f3,      f3, gekirido, ROT270, "Taito Corporation",         "Gekirindan (Japan)" )
GAME( 1995, quizhuhu, 0,        f3,      f3, quizhuhu, ROT0,   "Taito Corporation",         "Moriguchi Hiroko no Quiz de Hyuu!Hyuu! (Japan)" )
GAME( 1995, pbobble2, 0,        f3,      f3, pbobble2, ROT0,   "Taito Corporation Japan",   "Puzzle Bobble 2 (World)" )
GAME( 1995, pbobbl2j, pbobble2, f3,      f3, pbobble2, ROT0,   "Taito Corporation",         "Puzzle Bobble 2 (Japan)" )
GAME( 1995, pbobbl2u, pbobble2, f3,      f3, pbobble2, ROT0,   "Taito America Corporation", "Bust-A-Move Again (US)" )
GAME( 1995, pbobbl2x, pbobble2, f3,      f3, pbobbl2x, ROT0,   "Taito Corporation",         "Puzzle Bobble 2X (Japan)" )
GAME( 1995, ktiger2,  0,        f3,      f3, ktiger2,  ROT270, "Taito Corporation",         "Kyukyoku Tiger 2 (Japan)" )
/* Twin Cobra 2 (US & World) is known to exist */
GAMEC(1995, bubblem,  0,        f3_224a, f3, bubblem,  ROT0,   "Taito Corporation Japan",   "Bubble Memories - The Story Of Bubble Bobble 3 (World)", &generic_ctrl, &bubblem_bootstrap )
GAME( 1995, bubblemj, bubblem,  f3_224a, f3, bubblem,  ROT0,   "Taito Corporation",         "Bubble Memories - The Story Of Bubble Bobble 3 (Japan)" )
GAME( 1996, cleopatr, 0,        f3_224a, f3, cleopatr, ROT0,   "Taito Corporation",         "Cleopatra Fortune (Japan)" )
GAME( 1996, pbobble3, 0,        f3,      f3, pbobble3, ROT0,   "Taito Corporation",         "Puzzle Bobble 3 (World)" )
GAME( 1996, pbobbl3u, pbobble3, f3,      f3, pbobble3, ROT0,   "Taito Corporation",         "Puzzle Bobble 3 (US)" )
GAME( 1996, pbobbl3j, pbobble3, f3,      f3, pbobble3, ROT0,   "Taito Corporation",         "Puzzle Bobble 3 (Japan)" )
GAME( 1997, arkretrn, 0,        f3,      f3, arkretrn, ROT0,   "Taito Corporation",         "Arkanoid Returns (Japan)" )
GAME( 1997, kirameki, 0,        f3_224a, f3, kirameki, ROT0,   "Taito Corporation",         "Kirameki Star Road (Japan)" )
GAME( 1997, puchicar, 0,        f3,      f3, puchicar, ROT0,   "Taito Corporation",         "Puchi Carat (Japan)" )
GAME( 1997, pbobble4, 0,        f3,      f3, pbobble4, ROT0,   "Taito Corporation",         "Puzzle Bobble 4 (World)" )
GAME( 1997, pbobbl4j, pbobble4, f3,      f3, pbobble4, ROT0,   "Taito Corporation",         "Puzzle Bobble 4 (Japan)" )
GAME( 1997, pbobbl4u, pbobble4, f3,      f3, pbobble4, ROT0,   "Taito Corporation",         "Puzzle Bobble 4 (US)" )
GAME( 1997, popnpop,  0,        f3,      f3, popnpop,  ROT0,   "Taito Corporation",         "Pop 'N Pop (World)" )
GAME( 1997, popnpopj, popnpop,  f3,      f3, popnpop,  ROT0,   "Taito Corporation",         "Pop 'N Pop (Japan)" )
GAME( 1997, popnpopu, popnpop,  f3,      f3, popnpop,  ROT0,   "Taito Corporation",         "Pop 'N Pop (US)" )
GAME( 1998, landmakr, 0,        f3,      f3, landmakr, ROT0,   "Taito Corporation",         "Land Maker (Japan)" )
GAME( 1998, landmkrp, landmakr, f3,      f3, landmkrp, ROT0,   "Taito Corporation",         "Land Maker (World Prototype)" )
GAME( 1994, recalh,   0,        recalh,  f3, recalh,   ROT0,   "Taito Corporation",         "Recalhorn (Prototype)" )
GAMEX(1992, commandw, 0,        f3_224b, f3, commandw, ROT0,   "Taito Corporation",         "Command War - Super Special Battle & War Game (Prototype)", GAME_IMPERFECT_GRAPHICS )
