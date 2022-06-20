/***************************************************************************

	POW - Prisoners Of War (US) 		A7008	SNK 1988
	POW - Prisoners Of War (Japan)		A7008	SNK 1988
	SAR - Search And Rescue (World) 	A8007	SNK 1989
	SAR - Search And Rescue (US)		A8007	SNK 1989
	Street Smart (US version 1) 		A8007	SNK 1989
	Street Smart (US version 2) 		A7008	SNK 1989
	Street Smart (World version 1) 		A8007	SNK 1989
	Street Smart (Japan version 1)		A8007	SNK 1989
	Ikari III - The Rescue (US, Rotary Joystick) 		A7007	SNK 1989
	Ikari III - The Rescue (World, 8-Way Joystick) 		A7007	SNK 1989

	For some strange reason version 2 of Street Smart runs on Pow hardware!

	Emulation by Bryan McPhail, mish@tendril.co.uk


Change Log:

FEB-2003 (AT)

- bug fixes:

    pow37b5yel: incorrect sprite priority
  powj36rc2gre: scrambled Japanese text in cut scenes

Notes:

  Sprite flickerings and pop-up's not fixed. They look more like
  sloppy programming than emulation issues. Also suggest redumping
  sound ROM "dg7", it might be the cause of pow060gre.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

VIDEO_START( pow );
VIDEO_START( searchar );
VIDEO_START( ikari3 );
VIDEO_UPDATE( pow );
VIDEO_UPDATE( searchar );
WRITE16_HANDLER( pow_paletteram16_word_w );
WRITE16_HANDLER( pow_flipscreen16_w );
WRITE16_HANDLER( pow_video16_w );

static int invert_controls;

/******************************************************************************/

static READ16_HANDLER( sound_cpu_r )
{
	return 0x0100;
}

static READ16_HANDLER( pow_video16_r )
{
	data16_t data = videoram16[offset];
	return data;
}

static WRITE16_HANDLER( pow_spriteram16_w )
{
	/* DWORD aligned bytes should be $ff */
	if (!(offset & 1))
		data |= 0xff00;
	COMBINE_DATA(&spriteram16[offset]);
}

static READ16_HANDLER( pow_spriteram16_r )
{
	return spriteram16[offset];
}

static READ16_HANDLER( control_1_r )
{
	return (readinputport(0) + (readinputport(1) << 8));
}

static READ16_HANDLER( control_2_r )
{
	return readinputport(2);
}

static READ16_HANDLER( dip_1_r )
{
	return readinputport(3) << 8;
}

static READ16_HANDLER( dip_2_r )
{
	return readinputport(4) << 8;
}

static READ16_HANDLER( rotary_1_r )
{
	return (( ~(1 << (readinputport(5) * 12 / 256)) )<<8)&0xff00;
}

static READ16_HANDLER( rotary_2_r )
{
	return (( ~(1 << (readinputport(6) * 12 / 256)) )<<8)&0xff00;
}

static READ16_HANDLER( rotary_lsb_r )
{
	return ((( ~(1 << (readinputport(6) * 12 / 256))  ) <<4)&0xf000)
		 + ((( ~(1 << (readinputport(5) * 12 / 256))  )    )&0x0f00);
}

static READ16_HANDLER( protcontrols_r )
{
	return readinputport(offset) ^ invert_controls;
}

static WRITE16_HANDLER( protection_w )
{
	/* top byte is used, meaning unknown */
	/* bottom byte is protection in ikari 3 and streetsm */
	if (ACCESSING_LSB)
		invert_controls = ((data & 0xff) == 0x07) ? 0xff : 0x00;
}

static WRITE16_HANDLER( sound_w )
{
	soundlatch_w(0,(data>>8)&0xff);
	cpu_set_irq_line(1,IRQ_LINE_NMI,PULSE_LINE);
}

/*******************************************************************************/

static MEMORY_READ16_START( pow_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x040000, 0x043fff, MRA16_RAM },
	{ 0x080000, 0x080001, control_1_r },
	{ 0x0c0000, 0x0c0001, control_2_r },
	{ 0x0e0000, 0x0e0001, MRA16_NOP }, /* Watchdog or IRQ ack */
	{ 0x0e8000, 0x0e8001, MRA16_NOP }, /* Watchdog or IRQ ack */
	{ 0x0f0000, 0x0f0001, dip_1_r },
	{ 0x0f0008, 0x0f0009, dip_2_r },
	{ 0x100000, 0x100fff, pow_video16_r },
	{ 0x200000, 0x207fff, pow_spriteram16_r },
	{ 0x400000, 0x400fff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( pow_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x040000, 0x043fff, MWA16_RAM },
	{ 0x080000, 0x080001, sound_w },
	{ 0x0c0000, 0x0c0001, pow_flipscreen16_w },
	{ 0x0f0008, 0x0f0009, MWA16_NOP },
	{ 0x100000, 0x100fff, pow_video16_w, &videoram16 },
	{ 0x101000, 0x101fff, MWA16_NOP }, /* unknown register writes*/
	{ 0x200000, 0x207fff, pow_spriteram16_w, &spriteram16 },
	{ 0x400000, 0x400fff, pow_paletteram16_word_w, &paletteram16 },
MEMORY_END

static MEMORY_READ16_START( searchar_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x040000, 0x043fff, MRA16_RAM },
	{ 0x080000, 0x080005, protcontrols_r }, /* Player 1 & 2 */
	{ 0x0c0000, 0x0c0001, rotary_1_r }, /* Player 1 rotary */
	{ 0x0c8000, 0x0c8001, rotary_2_r }, /* Player 2 rotary */
	{ 0x0d0000, 0x0d0001, rotary_lsb_r }, /* Extra rotary bits */
	{ 0x0e0000, 0x0e0001, MRA16_NOP },	/* Watchdog or IRQ ack */
	{ 0x0e8000, 0x0e8001, MRA16_NOP },	/* Watchdog or IRQ ack */
	{ 0x0f0000, 0x0f0001, dip_1_r },
	{ 0x0f0008, 0x0f0009, dip_2_r },
	{ 0x0f8000, 0x0f8001, sound_cpu_r },
	{ 0x100000, 0x107fff, pow_spriteram16_r },
	{ 0x200000, 0x200fff, pow_video16_r },
	{ 0x300000, 0x33ffff, MRA16_BANK1 }, /* Extra code bank */
	{ 0x400000, 0x400fff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( searchar_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x040000, 0x043fff, MWA16_RAM },
	{ 0x080000, 0x080001, sound_w },
	{ 0x080006, 0x080007, protection_w }, /* top byte unknown, bottom is protection in ikari3 and streetsm */
	{ 0x0c0000, 0x0c0001, pow_flipscreen16_w },
	{ 0x0f0000, 0x0f0001, MWA16_NOP },
	{ 0x100000, 0x107fff, pow_spriteram16_w, &spriteram16 },
	{ 0x200000, 0x200fff, pow_video16_w, &videoram16 },
	{ 0x201000, 0x201fff, pow_video16_w }, /* Mirror used by Ikari 3 */
	{ 0x400000, 0x400fff, pow_paletteram16_word_w, &paletteram16 },
MEMORY_END

/******************************************************************************/

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xf800, soundlatch_r },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
MEMORY_END

static WRITE_HANDLER( D7759_write_port_0_w )
{
	upd7759_port_w(offset,data);
	upd7759_start_w (0,0);
	upd7759_start_w (0,1);
}

static PORT_READ_START( sound_readport )
	{ 0x00, 0x00, YM3812_status_port_0_r },
PORT_END

static PORT_WRITE_START( sound_writeport )
	{ 0x00, 0x00, YM3812_control_port_0_w },
	{ 0x20, 0x20, YM3812_write_port_0_w },
	{ 0x40, 0x40, D7759_write_port_0_w },
	{ 0x80, 0x80, upd7759_0_reset_w },
PORT_END

/******************************************************************************/

INPUT_PORTS_START( pow )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode dsw */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1, all active high */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x10, "3" )
	PORT_DIPNAME( 0x20, 0x00, "Bonus Occurrence" )
	PORT_DIPSETTING(	0x00, "1st & 2nd only" )
	PORT_DIPSETTING(	0x20, "1st & every 2nd" )
	PORT_DIPNAME( 0x40, 0x00, "Language" )
	PORT_DIPSETTING(	0x00, "English" )
	PORT_DIPSETTING(	0x40, "Japanese" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2, all active high */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x02, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "20k 50k" )
	PORT_DIPSETTING(	0x08, "40k 100k" )
	PORT_DIPSETTING(	0x04, "60k 150k" )
	PORT_DIPSETTING(	0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(	0x00, "Demo Sounds On" )
	PORT_DIPSETTING(	0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(	0x30, "Freeze" )
	PORT_BITX( 0,		0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x80, "Easy" )
	PORT_DIPSETTING(	0x00, "Normal" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0xc0, "Hardest" )
INPUT_PORTS_END

/* Identical to pow, but the Language dip switch has no effect */
INPUT_PORTS_START( powj )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode dsw */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1, all active high */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x10, "3" )
	PORT_DIPNAME( 0x20, 0x00, "Bonus Occurrence" )
	PORT_DIPSETTING(	0x00, "1st & 2nd only" )
	PORT_DIPSETTING(	0x20, "1st & every 2nd" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2, all active high */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x02, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "20k 50k" )
	PORT_DIPSETTING(	0x08, "40k 100k" )
	PORT_DIPSETTING(	0x04, "60k 150k" )
	PORT_DIPSETTING(	0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(	0x00, "Demo Sounds On" )
	PORT_DIPSETTING(	0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(	0x30, "Freeze" )
	PORT_BITX( 0,		0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x80, "Easy" )
	PORT_DIPSETTING(	0x00, "Normal" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0xc0, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( searchar )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START	/* coin */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode dsw */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switches (Active high) */
	PORT_DIPNAME( 0x01, 0x00, "Joystick" )
	PORT_DIPSETTING(    0x00, "Rotary Joystick" )
	PORT_DIPSETTING(    0x01, "Standard Joystick" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x0c, "5" )
	PORT_DIPNAME( 0x30, 0x00, "Coin A & B" )
	PORT_DIPSETTING(	0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x30, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x40, 0x00, "Bonus Occurrence" )
	PORT_DIPSETTING(	0x00, "1st & 2nd only" )
	PORT_DIPSETTING(	0x40, "1st & every 2nd" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START /* Dip switches (Active high) */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x02, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "50k 200k" )
	PORT_DIPSETTING(	0x08, "70k 270k" )
	PORT_DIPSETTING(	0x04, "90k 350k" )
	PORT_DIPSETTING(	0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(	0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(	0x00, "Demo Sounds On" )
	PORT_DIPSETTING(	0x30, "Freeze" )
	PORT_BITX( 0,		0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x80, "Easy" )
	PORT_DIPSETTING(	0x00, "Normal" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0xc0, "Hardest" )

	PORT_START	/* player 1 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START	/* player 2 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 10, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( streetsm )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START	/* coin */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode dsw */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switches (Active high) */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x02, "1" )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x01, "3" )
	PORT_DIPSETTING(	0x03, "4" )
	PORT_DIPNAME( 0x0c, 0x00, "Coin A & B" )
	PORT_DIPSETTING(	0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Bonus Occurrence" )
	PORT_DIPSETTING(	0x00, "1st & 2nd only" )
	PORT_DIPSETTING(	0x20, "1st & every 2nd" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START /* Dip switches (Active high) */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x02, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "200k 400k" )
	PORT_DIPSETTING(	0x08, "400k 600k" )
	PORT_DIPSETTING(	0x04, "600k 800k" )
	PORT_DIPSETTING(	0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(	0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(	0x00, "Demo Sounds On" )
	PORT_DIPSETTING(	0x30, "Freeze" )
	PORT_BITX( 0,		0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x80, "Easy" )
	PORT_DIPSETTING(	0x00, "Normal" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0xc0, "Hardest" )

	PORT_START	/* player 1 12-way rotary control - not used in this game */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* player 2 12-way rotary control - not used in this game */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* Same as streetsm, but Coinage is different */
INPUT_PORTS_START( streetsj )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START	/* coin */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode dsw */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switches (Active high) */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x02, "1" )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x01, "3" )
	PORT_DIPSETTING(	0x03, "4" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x0c, "A 4/1 B 1/4" )
	PORT_DIPSETTING(	0x04, "A 3/1 B 1/3" )
	PORT_DIPSETTING(	0x08, "A 2/1 B 1/2" )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Bonus Occurrence" )
	PORT_DIPSETTING(	0x00, "1st & 2nd only" )
	PORT_DIPSETTING(	0x20, "1st & every 2nd" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START /* Dip switches (Active high) */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x02, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "200k 400k" )
	PORT_DIPSETTING(	0x08, "400k 600k" )
	PORT_DIPSETTING(	0x04, "600k 800k" )
	PORT_DIPSETTING(	0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(	0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(	0x00, "Demo Sounds On" )
	PORT_DIPSETTING(	0x30, "Freeze" )
	PORT_BITX( 0,		0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x80, "Easy" )
	PORT_DIPSETTING(	0x00, "Normal" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0xc0, "Hardest" )

	PORT_START	/* player 1 12-way rotary control - not used in this game */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* player 2 12-way rotary control - not used in this game */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( ikari3 )
	PORT_START	/* Player 1 controls, maybe all are active_high? */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START	/* coin */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE )	/* same as the service mode dsw */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switches (Active high) */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x02, "2" )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x01, "4" )
	PORT_DIPSETTING(	0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x00, "Coin A & B" )
	PORT_DIPSETTING(	0x08, "First 2 Coins/1 Credit then 1/1" )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, "First 1 Coin/2 Credits then 1/1" )
	PORT_DIPSETTING(	0x0c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Bonus Occurrence" )
	PORT_DIPSETTING(	0x00, "1st & 2nd only" )
	PORT_DIPSETTING(	0x20, "1st & every 2nd" )
	PORT_DIPNAME( 0x40, 0x00, "Blood" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START /* Dip switches (Active high) */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x02, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "20k 50k" )
	PORT_DIPSETTING(	0x08, "40k 100k" )
	PORT_DIPSETTING(	0x04, "60k 150k" )
	PORT_DIPSETTING(	0x0c, "None" )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(	0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(	0x00, "Demo Sounds On" )
	PORT_DIPSETTING(	0x30, "Freeze" )
	PORT_BITX( 0,		0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x00, "Easy" )
	PORT_DIPSETTING(	0x80, "Normal" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0xc0, "Hardest" )

	PORT_START	/* player 1 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START	/* player 2 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 10, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	2048,
	4,		/* 4 bits per pixel  */
	{ 0, 4, 0x8000*8, (0x8000*8)+4 },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout pow_spritelayout =
{
	16,16,	/* 16*16 sprites */
	4096*4,
	4,		/* 4 bits per pixel */
	{ 0, 0x80000*8, 0x100000*8, 0x180000*8 },
	{ 16*8+7, 16*8+6, 16*8+5, 16*8+4, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
	  7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*32	/* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout searchar_spritelayout =
{
	16,16,
	0x6000,
	4,
	{ 0, 8,0x180000*8, 0x180000*8+8 },
	{ 32*8+7, 32*8+6, 32*8+5, 32*8+4, 32*8+3, 32*8+2, 32*8+1, 32*8+0,
		7, 6, 5, 4, 3, 2, 1, 0
	},
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16
	},
	64*8
};

static struct GfxLayout ikari3_spritelayout =
{
	16,16,	/* 16*16 sprites */
	4096*5,
	4,		/* 4 bits per pixel */
	{ 0x140000*8, 0, 0xa0000*8, 0x1e0000*8 },
	{ 16*8+7, 16*8+6, 16*8+5, 16*8+4, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
	  7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*32	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo pow_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,		 0, 128 },
	{ REGION_GFX2, 0, &pow_spritelayout, 0, 128 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo searchar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,			  0,  16 },
	{ REGION_GFX2, 0, &searchar_spritelayout, 0, 128 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ikari3_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,			0,	16 },
	{ REGION_GFX2, 0, &ikari3_spritelayout, 0, 128 },
	{ -1 } /* end of array */
};

/******************************************************************************/

static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip */
	4000000,	/* 4 MHz - accurate for POW, should be accurate for others */
	{ 50 },
	{ irqhandler },
};

static struct upd7759_interface upd7759_interface =
{
	1,		/* number of chips */
	{ UPD7759_STANDARD_CLOCK },
	{ 70 }, /* volume */
	{ REGION_SOUND1 },		/* memory region */
	{0}
};

/******************************************************************************/

static MACHINE_DRIVER_START( ikari3 )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 10000000)	/* Accurate */
	MDRV_CPU_MEMORY(searchar_readmem,searchar_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* Accurate */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(ikari3_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(ikari3)
	MDRV_VIDEO_UPDATE(searchar)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(UPD7759, upd7759_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( pow )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 10000000)	/* Accurate */
	MDRV_CPU_MEMORY(pow_readmem,pow_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* Accurate */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(pow_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(pow)
	MDRV_VIDEO_UPDATE(pow)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(UPD7759, upd7759_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( searchar )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)
	MDRV_CPU_MEMORY(searchar_readmem,searchar_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(searchar_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(searchar)
	MDRV_VIDEO_UPDATE(searchar)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(UPD7759, upd7759_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( streetsm )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 10000000)	/* Accurate */
	MDRV_CPU_MEMORY(pow_readmem,pow_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* Accurate */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(searchar_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(pow)
	MDRV_VIDEO_UPDATE(searchar)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(UPD7759, upd7759_interface)
MACHINE_DRIVER_END

/******************************************************************************/

ROM_START( pow )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "dg1",   0x000000, 0x20000, CRC(8e71a8af) SHA1(72c2eb2316c2684491331e8adabcb2be084aa6a2) )
	ROM_LOAD16_BYTE( "dg2",   0x000001, 0x20000, CRC(4287affc) SHA1(59dfb37296edd3b42231319a9f4df819d384db38) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "dg8",        0x000000, 0x10000, CRC(d1d61da3) SHA1(4e78643f8a7d44db3ff091acb0a5da1cc836e3cb) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dg9",        0x000000, 0x08000, CRC(df864a08) SHA1(dd996070077efbbf9d784299b6563cab258e4a8e) )
	ROM_LOAD( "dg10",       0x008000, 0x08000, CRC(9e470d53) SHA1(f7dc6ac39ade573480e87170a2781f0f72930580) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "snk880.11a", 0x000000, 0x20000, CRC(e70fd906) SHA1(b9e734c074ee1c8ae73e6041d739ab30d2df7d62) )
	ROM_LOAD( "snk880.12a", 0x020000, 0x20000, CRC(628b1aed) SHA1(1065ef835da03f7d9776e81c225c3ecdd2affae2) )
	ROM_LOAD( "snk880.13a", 0x040000, 0x20000, CRC(19dc8868) SHA1(82aaf93fc8f4b3bf947d373d0f41cc0044207c34) )
	ROM_LOAD( "snk880.14a", 0x060000, 0x20000, CRC(47cd498b) SHA1(7fbc007f2d817c26af02fef233f5e0681a17052a) )
	ROM_LOAD( "snk880.15a", 0x080000, 0x20000, CRC(7a90e957) SHA1(9650d7cdbcbbbcdd7f75a1c3c08a195aa456e169) )
	ROM_LOAD( "snk880.16a", 0x0a0000, 0x20000, CRC(e40a6c13) SHA1(7ad9dfc65f8c8b316933f0fdd3bc7a243d6eff65) )
	ROM_LOAD( "snk880.17a", 0x0c0000, 0x20000, CRC(c7931cc2) SHA1(908313d9b7fa4395b5fb79925d068e4f5d354b21) )
	ROM_LOAD( "snk880.18a", 0x0e0000, 0x20000, CRC(eed72232) SHA1(ad614e752cf1d3eac9a04cbc90435f988e90ace7) )
	ROM_LOAD( "snk880.19a", 0x100000, 0x20000, CRC(1775b8dd) SHA1(c01154749379be6e18baa99f4d94d97942f3dd85) )
	ROM_LOAD( "snk880.20a", 0x120000, 0x20000, CRC(f8e752ec) SHA1(1e1e178303f9af84cbb15249c49a870193ef805f) )
	ROM_LOAD( "snk880.21a", 0x140000, 0x20000, CRC(27e9fffe) SHA1(e8058db40832b986c5addd22dd69b0308d10ec71) )
	ROM_LOAD( "snk880.22a", 0x160000, 0x20000, CRC(aa9c00d8) SHA1(1017ed1cc036c6084b71204a998fd05557a6e59f) )
	ROM_LOAD( "snk880.23a", 0x180000, 0x20000, CRC(adb6ad68) SHA1(ed4323d2dfa3efaa496b17f4719f9566d56725e5) )
	ROM_LOAD( "snk880.24a", 0x1a0000, 0x20000, CRC(dd41865a) SHA1(c86f14342beca896784b88920d9e0879af4179ab) )
	ROM_LOAD( "snk880.25a", 0x1c0000, 0x20000, CRC(055759ad) SHA1(f9b12320f142075d49d447fb107af99272567d58) )
	ROM_LOAD( "snk880.26a", 0x1e0000, 0x20000, CRC(9bc261c5) SHA1(f07fef465191d48ccc149d1a62e6382d3fc0ef9f) )

	ROM_REGION( 0x10000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "dg7",        0x000000, 0x10000, CRC(aba9a9d3) SHA1(5098cd3a064b8ede24797de8879a277d79e79d75) )
ROM_END

ROM_START( powj )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "1-2",   0x000000, 0x20000, CRC(2f17bfb0) SHA1(8be18990829eb2586c00b9e8b35e8779bc48296a) )
	ROM_LOAD16_BYTE( "2-2",   0x000001, 0x20000, CRC(baa32354) SHA1(a235b82527dc025e699ba2e8e9797dac15ea9440) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "dg8",        0x000000, 0x10000, CRC(d1d61da3) SHA1(4e78643f8a7d44db3ff091acb0a5da1cc836e3cb) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dg9",        0x000000, 0x08000, CRC(df864a08) SHA1(dd996070077efbbf9d784299b6563cab258e4a8e) )
	ROM_LOAD( "dg10",       0x008000, 0x08000, CRC(9e470d53) SHA1(f7dc6ac39ade573480e87170a2781f0f72930580) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "snk880.11a", 0x000000, 0x20000, CRC(e70fd906) SHA1(b9e734c074ee1c8ae73e6041d739ab30d2df7d62) )
	ROM_LOAD( "snk880.12a", 0x020000, 0x20000, CRC(628b1aed) SHA1(1065ef835da03f7d9776e81c225c3ecdd2affae2) )
	ROM_LOAD( "snk880.13a", 0x040000, 0x20000, CRC(19dc8868) SHA1(82aaf93fc8f4b3bf947d373d0f41cc0044207c34) )
	ROM_LOAD( "snk880.14a", 0x060000, 0x20000, CRC(47cd498b) SHA1(7fbc007f2d817c26af02fef233f5e0681a17052a) )
	ROM_LOAD( "snk880.15a", 0x080000, 0x20000, CRC(7a90e957) SHA1(9650d7cdbcbbbcdd7f75a1c3c08a195aa456e169) )
	ROM_LOAD( "snk880.16a", 0x0a0000, 0x20000, CRC(e40a6c13) SHA1(7ad9dfc65f8c8b316933f0fdd3bc7a243d6eff65) )
	ROM_LOAD( "snk880.17a", 0x0c0000, 0x20000, CRC(c7931cc2) SHA1(908313d9b7fa4395b5fb79925d068e4f5d354b21) )
	ROM_LOAD( "snk880.18a", 0x0e0000, 0x20000, CRC(eed72232) SHA1(ad614e752cf1d3eac9a04cbc90435f988e90ace7) )
	ROM_LOAD( "snk880.19a", 0x100000, 0x20000, CRC(1775b8dd) SHA1(c01154749379be6e18baa99f4d94d97942f3dd85) )
	ROM_LOAD( "snk880.20a", 0x120000, 0x20000, CRC(f8e752ec) SHA1(1e1e178303f9af84cbb15249c49a870193ef805f) )
	ROM_LOAD( "snk880.21a", 0x140000, 0x20000, CRC(27e9fffe) SHA1(e8058db40832b986c5addd22dd69b0308d10ec71) )
	ROM_LOAD( "snk880.22a", 0x160000, 0x20000, CRC(aa9c00d8) SHA1(1017ed1cc036c6084b71204a998fd05557a6e59f) )
	ROM_LOAD( "snk880.23a", 0x180000, 0x20000, CRC(adb6ad68) SHA1(ed4323d2dfa3efaa496b17f4719f9566d56725e5) )
	ROM_LOAD( "snk880.24a", 0x1a0000, 0x20000, CRC(dd41865a) SHA1(c86f14342beca896784b88920d9e0879af4179ab) )
	ROM_LOAD( "snk880.25a", 0x1c0000, 0x20000, CRC(055759ad) SHA1(f9b12320f142075d49d447fb107af99272567d58) )
	ROM_LOAD( "snk880.26a", 0x1e0000, 0x20000, CRC(9bc261c5) SHA1(f07fef465191d48ccc149d1a62e6382d3fc0ef9f) )

	ROM_REGION( 0x10000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "dg7",        0x000000, 0x10000, CRC(aba9a9d3) SHA1(5098cd3a064b8ede24797de8879a277d79e79d75) )
ROM_END

ROM_START( searchar )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bhw.2", 0x000000, 0x20000, CRC(e1430138) SHA1(eddc192524a13b2c09bd2bddcd5f8e8b771ceb21) )
	ROM_LOAD16_BYTE( "bhw.3", 0x000001, 0x20000, CRC(ee1f9374) SHA1(fd41c74fd69d65713d8e1a9b8078328381119379) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "bh.5",       0x000000, 0x10000, CRC(53e2fa76) SHA1(cf25b1def82545a1fd013822ab3cf02483074623) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "bh.7",       0x000000, 0x08000, CRC(b0f1b049) SHA1(ec276984d91b5759a5e2b6815d1db2abc37b99f8) )
	ROM_LOAD( "bh.8",       0x008000, 0x08000, CRC(174ddba7) SHA1(7b19087cd2ccc409878aefe7fa08bb2e9953d352) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "bh.c1",      0x000000, 0x80000, CRC(1fb8f0ae) SHA1(d63c7376aa5f01bc009176b23324e720bada4286) )
	ROM_LOAD( "bh.c3",      0x080000, 0x80000, CRC(fd8bc407) SHA1(88d750293808bf6ea23864b22070314b14fbee3c) )
	ROM_LOAD( "bh.c5",      0x100000, 0x80000, CRC(1d30acc3) SHA1(e5ca39853779475b83fe37304e7bed2c293bd587) )
	ROM_LOAD( "bh.c2",      0x180000, 0x80000, CRC(7c803767) SHA1(992516fbb28d00feabbed5769fa3a5748199a7d8) )
	ROM_LOAD( "bh.c4",      0x200000, 0x80000, CRC(eede7c43) SHA1(7645acf0beb4fff9ec92205dcf34124360cd52f6) )
	ROM_LOAD( "bh.c6",      0x280000, 0x80000, CRC(9f785cd9) SHA1(e5c7797ae7a3139e1814b068c5ecfe5c6bf30d0f) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "bh.v1",      0x000000, 0x20000, CRC(07a6114b) SHA1(224df4616b77a56f33974d3b1793473d48ad52ca) )

	ROM_REGION16_BE( 0x40000, REGION_USER1, 0 ) /* Extra code bank */
	ROM_LOAD16_BYTE( "bhw.1", 0x000000, 0x20000, CRC(62b60066) SHA1(f7e7985c8f5f8191c580e777e1b7ed29d944d23f) )
	ROM_LOAD16_BYTE( "bhw.4", 0x000001, 0x20000, CRC(16d8525c) SHA1(0098b0a7fcb23de2661bbec9a05254aa46579bb2) )
ROM_END

ROM_START( sercharu )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bh.2",  0x000000, 0x20000, CRC(c852e2e2) SHA1(c4b1b366f452122549046a3dec9b6b375bc273af) )
	ROM_LOAD16_BYTE( "bh.3",  0x000001, 0x20000, CRC(bc04a4a1) SHA1(aa91583b987248a3e99813ab5e8ee03c02dac9b9) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "bh.5",       0x000000, 0x10000, CRC(53e2fa76) SHA1(cf25b1def82545a1fd013822ab3cf02483074623) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "bh.7",       0x000000, 0x08000, CRC(b0f1b049) SHA1(ec276984d91b5759a5e2b6815d1db2abc37b99f8) )
	ROM_LOAD( "bh.8",       0x008000, 0x08000, CRC(174ddba7) SHA1(7b19087cd2ccc409878aefe7fa08bb2e9953d352) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "bh.c1",      0x000000, 0x80000, CRC(1fb8f0ae) SHA1(d63c7376aa5f01bc009176b23324e720bada4286) )
	ROM_LOAD( "bh.c3",      0x080000, 0x80000, CRC(fd8bc407) SHA1(88d750293808bf6ea23864b22070314b14fbee3c) )
	ROM_LOAD( "bh.c5",      0x100000, 0x80000, CRC(1d30acc3) SHA1(e5ca39853779475b83fe37304e7bed2c293bd587) )
	ROM_LOAD( "bh.c2",      0x180000, 0x80000, CRC(7c803767) SHA1(992516fbb28d00feabbed5769fa3a5748199a7d8) )
	ROM_LOAD( "bh.c4",      0x200000, 0x80000, CRC(eede7c43) SHA1(7645acf0beb4fff9ec92205dcf34124360cd52f6) )
	ROM_LOAD( "bh.c6",      0x280000, 0x80000, CRC(9f785cd9) SHA1(e5c7797ae7a3139e1814b068c5ecfe5c6bf30d0f) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "bh.v1",      0x000000, 0x20000, CRC(07a6114b) SHA1(224df4616b77a56f33974d3b1793473d48ad52ca) )

	ROM_REGION16_BE( 0x40000, REGION_USER1, 0 ) /* Extra code bank */
	ROM_LOAD16_BYTE( "bh.1",  0x000000, 0x20000, CRC(ba9ca70b) SHA1(c46727473673554cbe4bbbc0288d66357f99a80e) )
	ROM_LOAD16_BYTE( "bh.4",  0x000001, 0x20000, CRC(eabc5ddf) SHA1(08a2a8fcdf6a08a2694e00f4232a5bfbec98fd27) )
ROM_END

ROM_START( streetsm )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "s2-1ver2.14h", 0x00000, 0x20000, CRC(655f4773) SHA1(5374a6cf0b895c5ff839b0f52402df4cc53241cf) )
	ROM_LOAD16_BYTE( "s2-2ver2.14k", 0x00001, 0x20000, CRC(efae4823) SHA1(f3be25b76cf13feeaaaf0e9640c30a6a7371f108) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "s2-5.16c",    0x000000, 0x10000, CRC(ca4b171e) SHA1(a05fd81f68759a09be3ec09f38d7c9364dfb6c14) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "s2-9.25l",    0x000000, 0x08000, CRC(09b6ac67) SHA1(0b1ef51d9cd755eacc25b33360811cc86c32c0b7) )
	ROM_LOAD( "s2-10.25m",   0x008000, 0x08000, CRC(89e4ee6f) SHA1(21797286836ad71d2497e3e6d4df1fbe545562ab) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "stsmart.900", 0x000000, 0x80000, CRC(a8279a7e) SHA1(244bdacb29b00f71da93ed8ddddbcffcce110be8) )
	ROM_LOAD( "stsmart.902", 0x080000, 0x80000, CRC(2f021aa1) SHA1(699d0b5ac79e34e4fc4cef70eb448f21f1c3e9e2) )
	ROM_LOAD( "stsmart.904", 0x100000, 0x80000, CRC(167346f7) SHA1(fb4ea412622245db49ec15449ee4fa0d90922f06) )
	ROM_LOAD( "stsmart.901", 0x180000, 0x80000, CRC(c305af12) SHA1(18b5d448fe9608efcd2e5bb8faa24808d1489ec8) )
	ROM_LOAD( "stsmart.903", 0x200000, 0x80000, CRC(73c16d35) SHA1(40cf7a58926c649f89b08917afb35b08918d1a0f) )
	ROM_LOAD( "stsmart.905", 0x280000, 0x80000, CRC(a5beb4e2) SHA1(c26b7eee2ca32bd73fb7a09c6ef52c2ae1c7fc1c) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "s2-6.18d",    0x000000, 0x20000, CRC(47db1605) SHA1(ae00e633eb98567f04ff97e3d63e04e049d955ec) )
ROM_END

ROM_START( streets1 )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "s2-1ver1.9c",  0x00000, 0x20000, CRC(b59354c5) SHA1(086c87541d422f90bdaad8d63b14d0d520c12564) )
	ROM_LOAD16_BYTE( "s2-2ver1.10c", 0x00001, 0x20000, CRC(e448b68b) SHA1(08d674ab3d9bd3d3b1d50967a56fa6a002ce0b8d) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "s2-5.16c",    0x000000, 0x10000, CRC(ca4b171e) SHA1(a05fd81f68759a09be3ec09f38d7c9364dfb6c14) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "s2-7.15l",    0x000000, 0x08000, CRC(22bedfe5) SHA1(64efb2281c32afe5a06f35cce939e6a53226c6ed) )
	ROM_LOAD( "s2-8.15m",    0x008000, 0x08000, CRC(6a1c70ab) SHA1(019538ddcb713d0810b26b6aa65f6e4596931621) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "stsmart.900", 0x000000, 0x80000, CRC(a8279a7e) SHA1(244bdacb29b00f71da93ed8ddddbcffcce110be8) )
	ROM_LOAD( "stsmart.902", 0x080000, 0x80000, CRC(2f021aa1) SHA1(699d0b5ac79e34e4fc4cef70eb448f21f1c3e9e2) )
	ROM_LOAD( "stsmart.904", 0x100000, 0x80000, CRC(167346f7) SHA1(fb4ea412622245db49ec15449ee4fa0d90922f06) )
	ROM_LOAD( "stsmart.901", 0x180000, 0x80000, CRC(c305af12) SHA1(18b5d448fe9608efcd2e5bb8faa24808d1489ec8) )
	ROM_LOAD( "stsmart.903", 0x200000, 0x80000, CRC(73c16d35) SHA1(40cf7a58926c649f89b08917afb35b08918d1a0f) )
	ROM_LOAD( "stsmart.905", 0x280000, 0x80000, CRC(a5beb4e2) SHA1(c26b7eee2ca32bd73fb7a09c6ef52c2ae1c7fc1c) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "s2-6.18d",    0x000000, 0x20000, CRC(47db1605) SHA1(ae00e633eb98567f04ff97e3d63e04e049d955ec) )
ROM_END

ROM_START( streetsw )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "s-smart1.bin", 0x00000, 0x20000, CRC(a1f5ceab) SHA1(74f5a4288618fbce6ed3dc75b6ccfa695396193c) )
	ROM_LOAD16_BYTE( "s-smart2.bin", 0x00001, 0x20000, CRC(263f615d) SHA1(4576f9d2abb31ecf747a5075716579e75613d57c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "s2-5.16c",    0x000000, 0x10000, CRC(ca4b171e) SHA1(a05fd81f68759a09be3ec09f38d7c9364dfb6c14) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "s2-7.15l",    0x000000, 0x08000, CRC(22bedfe5) SHA1(64efb2281c32afe5a06f35cce939e6a53226c6ed) )
	ROM_LOAD( "s2-8.15m",    0x008000, 0x08000, CRC(6a1c70ab) SHA1(019538ddcb713d0810b26b6aa65f6e4596931621) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "stsmart.900", 0x000000, 0x80000, CRC(a8279a7e) SHA1(244bdacb29b00f71da93ed8ddddbcffcce110be8) )
	ROM_LOAD( "stsmart.902", 0x080000, 0x80000, CRC(2f021aa1) SHA1(699d0b5ac79e34e4fc4cef70eb448f21f1c3e9e2) )
	ROM_LOAD( "stsmart.904", 0x100000, 0x80000, CRC(167346f7) SHA1(fb4ea412622245db49ec15449ee4fa0d90922f06) )
	ROM_LOAD( "stsmart.901", 0x180000, 0x80000, CRC(c305af12) SHA1(18b5d448fe9608efcd2e5bb8faa24808d1489ec8) )
	ROM_LOAD( "stsmart.903", 0x200000, 0x80000, CRC(73c16d35) SHA1(40cf7a58926c649f89b08917afb35b08918d1a0f) )
	ROM_LOAD( "stsmart.905", 0x280000, 0x80000, CRC(a5beb4e2) SHA1(c26b7eee2ca32bd73fb7a09c6ef52c2ae1c7fc1c) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "s2-6.18d",    0x000000, 0x20000, CRC(47db1605) SHA1(ae00e633eb98567f04ff97e3d63e04e049d955ec) )
ROM_END

ROM_START( streetsj )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "s2v1j_01.bin", 0x00000, 0x20000, CRC(f031413c) SHA1(5d7dfeac03f786736914f047c28a7a0488175176) )
	ROM_LOAD16_BYTE( "s2v1j_02.bin", 0x00001, 0x20000, CRC(e403a40b) SHA1(e740848d716586737eff6e3c201fb3e3da048a09) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "s2-5.16c",    0x000000, 0x10000, CRC(ca4b171e) SHA1(a05fd81f68759a09be3ec09f38d7c9364dfb6c14) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "s2-7.15l",    0x000000, 0x08000, CRC(22bedfe5) SHA1(64efb2281c32afe5a06f35cce939e6a53226c6ed) )
	ROM_LOAD( "s2-8.15m",    0x008000, 0x08000, CRC(6a1c70ab) SHA1(019538ddcb713d0810b26b6aa65f6e4596931621) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "stsmart.900", 0x000000, 0x80000, CRC(a8279a7e) SHA1(244bdacb29b00f71da93ed8ddddbcffcce110be8) )
	ROM_LOAD( "stsmart.902", 0x080000, 0x80000, CRC(2f021aa1) SHA1(699d0b5ac79e34e4fc4cef70eb448f21f1c3e9e2) )
	ROM_LOAD( "stsmart.904", 0x100000, 0x80000, CRC(167346f7) SHA1(fb4ea412622245db49ec15449ee4fa0d90922f06) )
	ROM_LOAD( "stsmart.901", 0x180000, 0x80000, CRC(c305af12) SHA1(18b5d448fe9608efcd2e5bb8faa24808d1489ec8) )
	ROM_LOAD( "stsmart.903", 0x200000, 0x80000, CRC(73c16d35) SHA1(40cf7a58926c649f89b08917afb35b08918d1a0f) )
	ROM_LOAD( "stsmart.905", 0x280000, 0x80000, CRC(a5beb4e2) SHA1(c26b7eee2ca32bd73fb7a09c6ef52c2ae1c7fc1c) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "s2-6.18d",    0x000000, 0x20000, CRC(47db1605) SHA1(ae00e633eb98567f04ff97e3d63e04e049d955ec) )
ROM_END

ROM_START( ikari3 )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ik3-2.bin", 0x000000, 0x20000, CRC(a7b34dcd) SHA1(7c2f20ae4f7dbebd3dfa3ec5408ed714e6535b6a) )
	ROM_LOAD16_BYTE( "ik3-3.bin", 0x000001, 0x20000, CRC(50f2b83d) SHA1(b1f0c554b262614dd2cff7a3857cb974d361937f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "ik3-5.bin",  0x000000, 0x10000, CRC(ce6706fc) SHA1(95505b90a9524abf0c8c1ec6b2c40d8f25cb1d92) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ik3-7.bin",  0x000000, 0x08000, CRC(0b4804df) SHA1(66d16d245bfc404366164823faaea0bfec83e487) )
	ROM_LOAD( "ik3-8.bin",  0x008000, 0x08000, CRC(10ab4e50) SHA1(dee8416eb720848cf6471e568dadc1cfc6c2e67f) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "ik3-13.bin", 0x000000, 0x20000, CRC(9a56bd32) SHA1(9301b48f970b71a909fb44514b2e93c3f1516b38) )
	ROM_LOAD( "ik3-12.bin", 0x020000, 0x20000, CRC(0ce6a10a) SHA1(13a231aa0002b2c5a0d9404ba05a879e212d638e) )
	ROM_LOAD( "ik3-11.bin", 0x040000, 0x20000, CRC(e4e2be43) SHA1(959d2799708ddae909b017c0696694c46a52697e) )
	ROM_LOAD( "ik3-10.bin", 0x060000, 0x20000, CRC(ac222372) SHA1(8a17e37699d691b962a6d0256a18550cc73ddfef) )
	ROM_LOAD( "ik3-9.bin",  0x080000, 0x20000, CRC(c33971c2) SHA1(91f3eb301803f5a7027da1ff7dd2a28bc97e5125) )
	ROM_LOAD( "ik3-14.bin", 0x0a0000, 0x20000, CRC(453bea77) SHA1(f8f8d0c048fcf32ad99e1de622d9ab635bb86eae) )
	ROM_LOAD( "ik3-15.bin", 0x0c0000, 0x20000, CRC(781a81fc) SHA1(e08a6cf9c632d1002176afe618605bc06168e8aa) )
	ROM_LOAD( "ik3-16.bin", 0x0e0000, 0x20000, CRC(80ba400b) SHA1(2cc3e53c45f239516a60c461ad9cfa5955164262) )
	ROM_LOAD( "ik3-17.bin", 0x100000, 0x20000, CRC(0cc3ce4a) SHA1(7b34435d0bbb089055a183b821ab255170db6bec) )
	ROM_LOAD( "ik3-18.bin", 0x120000, 0x20000, CRC(ba106245) SHA1(ac609ec3046c21fe6058f91dd4528c5c6448dc15) )
	ROM_LOAD( "ik3-23.bin", 0x140000, 0x20000, CRC(d0fd5c77) SHA1(c171c64ad252f0ba5b0bbdf37808102fca37b488) )
	ROM_LOAD( "ik3-22.bin", 0x160000, 0x20000, CRC(4878d883) SHA1(8cdb541bad00e707fb65399d637b7cc9288ada77) )
	ROM_LOAD( "ik3-21.bin", 0x180000, 0x20000, CRC(50d0fbf0) SHA1(9ff5fbea8d35d0f9a38ddd7eb093edcd91d9f874) )
	ROM_LOAD( "ik3-20.bin", 0x1a0000, 0x20000, CRC(9a851efc) SHA1(bc7be338ee4da7fbfe6fe44a9c7889817416bc44) )
	ROM_LOAD( "ik3-19.bin", 0x1c0000, 0x20000, CRC(4ebdba89) SHA1(f3ecfef4c9d2aba58dc3e6aa3cf5813d68686909) )
	ROM_LOAD( "ik3-24.bin", 0x1e0000, 0x20000, CRC(e9b26d68) SHA1(067d582d33157ed4b7980bd87f2f260ab74c347b) )
	ROM_LOAD( "ik3-25.bin", 0x200000, 0x20000, CRC(073b03f1) SHA1(b8053139799fa06c7324cee928154c89d4425ab1) )
	ROM_LOAD( "ik3-26.bin", 0x220000, 0x20000, CRC(9c613561) SHA1(fc7c9a642b18faa94e6a2ba53f35a4d756a25da3) )
	ROM_LOAD( "ik3-27.bin", 0x240000, 0x20000, CRC(16dd227e) SHA1(db3b1718dea65bc9a1a736aa62aa2be389313baf) )
	ROM_LOAD( "ik3-28.bin", 0x260000, 0x20000, CRC(711715ae) SHA1(90978c86884ca3d23c138d95b654e2fb3afc6f9a) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "ik3-6.bin",  0x000000, 0x20000, CRC(59d256a4) SHA1(1e7b33329f761c695bc9a817bbc0c5e13386d073) )

	ROM_REGION16_BE( 0x40000, REGION_USER1, 0 ) /* Extra code bank */
	ROM_LOAD16_BYTE( "ik3-1.bin",  0x000000, 0x10000, CRC(47e4d256) SHA1(7c6921cf2f1b8c3dae867eb1fc14e3da218cc1e0) )
	ROM_LOAD16_BYTE( "ik3-4.bin",  0x000001, 0x10000, CRC(a43af6b5) SHA1(1ad3acadbadd21642932028ecd7c282f7fd02856) )
ROM_END

ROM_START( ikari3nr ) /* 8-Way Joystick */
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ik3-2-ver1.c10", 0x000000, 0x20000, CRC(1bae8023) SHA1(42d590a545cbabc596f2e0d9a3d56b1bc270ec9a) )
	ROM_LOAD16_BYTE( "ik3-3-ver1.c9",  0x000001, 0x20000, CRC(10e38b66) SHA1(28cc82d868f59cd6dde1c4e4c890627012e5e978) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Sound CPU */
	ROM_LOAD( "ik3-5.bin",  0x000000, 0x10000, CRC(ce6706fc) SHA1(95505b90a9524abf0c8c1ec6b2c40d8f25cb1d92) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ik3-7.bin",  0x000000, 0x08000, CRC(0b4804df) SHA1(66d16d245bfc404366164823faaea0bfec83e487) )
	ROM_LOAD( "ik3-8.bin",  0x008000, 0x08000, CRC(10ab4e50) SHA1(dee8416eb720848cf6471e568dadc1cfc6c2e67f) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "ik3-13.bin", 0x000000, 0x20000, CRC(9a56bd32) SHA1(9301b48f970b71a909fb44514b2e93c3f1516b38) )
	ROM_LOAD( "ik3-12.bin", 0x020000, 0x20000, CRC(0ce6a10a) SHA1(13a231aa0002b2c5a0d9404ba05a879e212d638e) )
	ROM_LOAD( "ik3-11.bin", 0x040000, 0x20000, CRC(e4e2be43) SHA1(959d2799708ddae909b017c0696694c46a52697e) )
	ROM_LOAD( "ik3-10.bin", 0x060000, 0x20000, CRC(ac222372) SHA1(8a17e37699d691b962a6d0256a18550cc73ddfef) )
	ROM_LOAD( "ik3-9.bin",  0x080000, 0x20000, CRC(c33971c2) SHA1(91f3eb301803f5a7027da1ff7dd2a28bc97e5125) )
	ROM_LOAD( "ik3-14.bin", 0x0a0000, 0x20000, CRC(453bea77) SHA1(f8f8d0c048fcf32ad99e1de622d9ab635bb86eae) )
	ROM_LOAD( "ik3-15.bin", 0x0c0000, 0x20000, CRC(781a81fc) SHA1(e08a6cf9c632d1002176afe618605bc06168e8aa) )
	ROM_LOAD( "ik3-16.bin", 0x0e0000, 0x20000, CRC(80ba400b) SHA1(2cc3e53c45f239516a60c461ad9cfa5955164262) )
	ROM_LOAD( "ik3-17.bin", 0x100000, 0x20000, CRC(0cc3ce4a) SHA1(7b34435d0bbb089055a183b821ab255170db6bec) )
	ROM_LOAD( "ik3-18.bin", 0x120000, 0x20000, CRC(ba106245) SHA1(ac609ec3046c21fe6058f91dd4528c5c6448dc15) )
	ROM_LOAD( "ik3-23.bin", 0x140000, 0x20000, CRC(d0fd5c77) SHA1(c171c64ad252f0ba5b0bbdf37808102fca37b488) )
	ROM_LOAD( "ik3-22.bin", 0x160000, 0x20000, CRC(4878d883) SHA1(8cdb541bad00e707fb65399d637b7cc9288ada77) )
	ROM_LOAD( "ik3-21.bin", 0x180000, 0x20000, CRC(50d0fbf0) SHA1(9ff5fbea8d35d0f9a38ddd7eb093edcd91d9f874) )
	ROM_LOAD( "ik3-20.bin", 0x1a0000, 0x20000, CRC(9a851efc) SHA1(bc7be338ee4da7fbfe6fe44a9c7889817416bc44) )
	ROM_LOAD( "ik3-19.bin", 0x1c0000, 0x20000, CRC(4ebdba89) SHA1(f3ecfef4c9d2aba58dc3e6aa3cf5813d68686909) )
	ROM_LOAD( "ik3-24.bin", 0x1e0000, 0x20000, CRC(e9b26d68) SHA1(067d582d33157ed4b7980bd87f2f260ab74c347b) )
	ROM_LOAD( "ik3-25.bin", 0x200000, 0x20000, CRC(073b03f1) SHA1(b8053139799fa06c7324cee928154c89d4425ab1) )
	ROM_LOAD( "ik3-26.bin", 0x220000, 0x20000, CRC(9c613561) SHA1(fc7c9a642b18faa94e6a2ba53f35a4d756a25da3) )
	ROM_LOAD( "ik3-27.bin", 0x240000, 0x20000, CRC(16dd227e) SHA1(db3b1718dea65bc9a1a736aa62aa2be389313baf) )
	ROM_LOAD( "ik3-28.bin", 0x260000, 0x20000, CRC(711715ae) SHA1(90978c86884ca3d23c138d95b654e2fb3afc6f9a) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* UPD7759 samples */
	ROM_LOAD( "ik3-6.bin",  0x000000, 0x20000, CRC(59d256a4) SHA1(1e7b33329f761c695bc9a817bbc0c5e13386d073) )

	ROM_REGION16_BE( 0x40000, REGION_USER1, 0 ) /* Extra code bank */
	ROM_LOAD16_BYTE( "ik3-1.c8",   0x000000, 0x10000, CRC(47e4d256) SHA1(7c6921cf2f1b8c3dae867eb1fc14e3da218cc1e0) )
	ROM_LOAD16_BYTE( "ik3-4.c12",  0x000001, 0x10000, CRC(a43af6b5) SHA1(1ad3acadbadd21642932028ecd7c282f7fd02856) )
ROM_END

/******************************************************************************/

static DRIVER_INIT( searchar )
{
	cpu_setbank(1, memory_region(REGION_USER1));
}

/******************************************************************************/

GAME( 1988, pow,      0,	pow,	  pow,	    0,		  ROT0,  "SNK", "P.O.W. - Prisoners of War (US)" )
GAME( 1988, powj,     pow,	pow,	  powj,     0,		  ROT0,  "SNK", "Datsugoku - Prisoners of War (Japan)" )
GAME( 1989, searchar, 0,	searchar, searchar, searchar,     ROT90, "SNK", "SAR - Search And Rescue (World)" )
GAME( 1989, sercharu, searchar, searchar, searchar, searchar,     ROT90, "SNK", "SAR - Search And Rescue (US)" )
GAME( 1989, streetsm, 0,	streetsm, streetsm, 0,		  ROT0,  "SNK", "Street Smart (US version 2)" )
GAME( 1989, streets1, streetsm, searchar, streetsm, 0,		  ROT0,  "SNK", "Street Smart (US version 1)" )
GAME( 1989, streetsw, streetsm, searchar, streetsj, 0,		  ROT0,  "SNK", "Street Smart (World version 1)" )
GAME( 1989, streetsj, streetsm, searchar, streetsj, 0,		  ROT0,  "SNK", "Street Smart (Japan version 1)" )
GAME( 1989, ikari3,   0,	ikari3,   ikari3,   searchar,     ROT0,  "SNK", "Ikari III - The Rescue (US, Rotary Joystick)" )
GAME( 1989, ikari3nr, ikari3,   ikari3,   ikari3,   searchar,     ROT0,  "SNK", "Ikari III - The Rescue (World, 8-Way Joystick)" )
