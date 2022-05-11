/*****************************************************************************

	Irem M92 system games:

	Gunforce (World)				M92-A	(c) 1991 Irem Corp
	Gunforce (USA)					M92-A	(c) 1991 Irem America Corp
	Gunforce (Japan)				M92-A	(c) 1991 Irem Corp
	Blademaster	(World)						(c) 1991 Irem Corp
	Lethal Thunder (World)					(c) 1991 Irem Corp
	Thunder Blaster (Japan)					(c) 1991 Irem Corp
	Undercover Cops	(World)					(c) 1992 Irem Corp
	Undercover Cops	(Alpha Renewal Version)	(c) 1992 Irem America Corp
	Undercover Cops	(Japan)					(c) 1992 Irem Corp
	Mystic Riders (World)					(c) 1992 Irem Corp
	Gun Hohki (Japan)						(c) 1992 Irem Corp
	Major Title 2 (World)			M92-F	(c) 1992 Irem Corp
	The Irem Skins Game (USA Set 1)	M92-F	(c) 1992 Irem America Corp
	The Irem Skins Game (USA Set 2)	M92-F	(c) 1992 Irem America Corp
	Hook (World)							(c) 1992 Irem Corp
	Hook (USA)								(c) 1992 Irem America Corp
	Hook (Japan)							(c) 1992 Irem Corp
	R-Type Leo (World)						(c) 1992 Irem Corp
	R-Type Leo (Japan)						(c) 1992 Irem Corp
	In The Hunt	(World)				M92-E	(c) 1993 Irem Corp
	In The Hunt	(USA)				M92-E	(c) 1993 Irem Corp
	Kaitei Daisensou (Japan)		M92-E	(c) 1993 Irem Corp
	Ninja Baseball Batman (USA)				(c) 1993 Irem America Corp
	Yakyuu Kakutou League-Man (Japan)		(c) 1993 Irem Corp
	Perfect Soldiers (Japan)		M92-G	(c) 1993 Irem Corp
	Dream Soccer 94 (Japan)			M92-G	(c) 1994 Irem Corp
	Gunforce 2 (US)					M92-G	(c) 1994 Irem Corp
	Geostorm (Japan)				M92-G	(c) 1994 Irem Corp

System notes:
	Each game has an encrypted sound cpu (see irem_cpu.c), the sound cpu and
	the sprite chip are on the game board rather than the main board and
	can differ between games.

	Irem Skins Game has an eeprom and ticket payout(?).
	R-Type Leo & Lethal Thunder have a memory card.

	Many games use raster IRQ's for special video effects, eg,
		* Scrolling water in Undercover Cops
		* Score display in R-Type Leo

	These are slow to emulate, and can be turned on/off by pressing
	F1 - they are on by default.
	Todo:  Raster effects don't work in flipscreen mode.

Glitch list!

	Gunforce:
		Animated water sometimes doesn't appear on level 5 (but it
		always appears if you cheat and jump straight to the level).
		Almost certainly a core bug.

	Irem Skins:
		- Priority bug: you can't see the arrow on the top right map.
		- Gfx problems at the players information during attract mode in
		  Skins Game *only*, Major Title is fine (that part of attract mode
		  is different).
		- Eeprom load/save not yet implemented - when done, MT2EEP should
		  be removed from the ROM definition.

	Perfect Soliders:
		Shortly into the fight, the sound CPU enters a tight loop, conitnuously
		writing to the status port and with interrupts disabled. I don't see how
		it is supposed to get out of that loop. Maybe it's not supposed to enter
		it at all?

	LeagueMan:
		Raster effects don't work properly (not even cpu time per line?).

	Dream Soccer 94:
		Slight priority problems when goal scoring animation is played

	Emulation by Bryan McPhail, mish@tendril.co.uk
	Thanks to Chris Hardy and Olli Bergmann too!


Sound programs:

Game                          Year  ID string
----------------------------  ----  ------------
Gunforce					  1991  -
Blade Master				  1991  -
Lethal Thunder				  1991  -
Undercover Cops				  1992  Rev 3.40 M92
Mystic Riders				  1992  Rev 3.44 M92
Major Title 2				  1992  Rev 3.44 M92
Hook						  1992  Rev 3.45 M92
R-Type Leo					  1992  Rev 3.45 M92
In The Hunt					  1993  Rev 3.45 M92
Ninja Baseball Batman		  1993  Rev 3.50 M92
Perfect Soldiers			  1993  Rev 3.50 M92
World PK Soccer               1995  Rev 3.51 M92
Fire Barrel                   1993  Rev 3.52 M92
Dream Soccer '94              1994  Rev 3.53 M92
Gunforce 2                    1994  Rev 3.53 M92

*****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "m92.h"
#include "machine/irem_cpu.h"
#include "state.h"

static int irqvector;
static int sound_status;
static int bankaddress;

static int m92_irq_vectorbase;
static unsigned char *m92_ram,*m92_snd_ram;

#define M92_IRQ_0 ((m92_irq_vectorbase+0)/4)  /* VBL interrupt*/
#define M92_IRQ_1 ((m92_irq_vectorbase+4)/4)  /* Sprite buffer complete interrupt */
#define M92_IRQ_2 ((m92_irq_vectorbase+8)/4)  /* Raster interrupt */
#define M92_IRQ_3 ((m92_irq_vectorbase+12)/4) /* Sound cpu interrupt */

#define M92_SCANLINES	256

/* From vidhrdw/m92.c */
WRITE_HANDLER( m92_spritecontrol_w );
WRITE_HANDLER( m92_videocontrol_w );
READ_HANDLER( m92_paletteram_r );
WRITE_HANDLER( m92_paletteram_w );
READ_HANDLER( m92_vram_r );
WRITE_HANDLER( m92_vram_w );
WRITE_HANDLER( m92_pf1_control_w );
WRITE_HANDLER( m92_pf2_control_w );
WRITE_HANDLER( m92_pf3_control_w );
WRITE_HANDLER( m92_master_control_w );
VIDEO_START( m92 );
VIDEO_UPDATE( m92 );
void m92_vh_raster_partial_refresh(struct mame_bitmap *bitmap,int start_line,int end_line);

extern int m92_raster_irq_position,m92_raster_enable;
extern unsigned char *m92_vram_data,*m92_spritecontrol;

extern int m92_sprite_buffer_busy,m92_game_kludge;

/*****************************************************************************/

static void set_m92_bank(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	cpu_setbank(1,&RAM[bankaddress]);
}

/*****************************************************************************/

static READ_HANDLER( m92_eeprom_r )
{
	unsigned char *RAM = memory_region(REGION_USER1);
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "%05x: EEPROM RE %04x\n",activecpu_get_pc(),offset);*/
	return RAM[offset/2];
}

static WRITE_HANDLER( m92_eeprom_w )
{
	unsigned char *RAM = memory_region(REGION_USER1);
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "%05x: EEPROM WR %04x\n",activecpu_get_pc(),offset);*/
	RAM[offset/2]=data;
}

static WRITE_HANDLER( m92_coincounter_w )
{
	if (offset==0) {
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);
		/* Bit 0x8 is Motor(?!), used in Hook, In The Hunt, UCops */
		/* Bit 0x8 is Memcard related in RTypeLeo */
		/* Bit 0x40 set in Blade Master test mode input check */
	}
}

static WRITE_HANDLER( m92_bankswitch_w )
{
	if (offset==1) return; /* Unused top byte */
	bankaddress = 0x100000 + ((data&0x7)*0x10000);
	set_m92_bank();
}

static READ_HANDLER( m92_port_4_r )
{
	return readinputport(4) | m92_sprite_buffer_busy; /* Bit 7 low indicates busy */
}

/*****************************************************************************/

enum { VECTOR_INIT, YM2151_ASSERT, YM2151_CLEAR, V30_ASSERT, V30_CLEAR };

static void setvector_callback(int param)
{
	switch(param)
	{
		case VECTOR_INIT:	irqvector = 0;		break;
		case YM2151_ASSERT:	irqvector |= 0x2;	break;
		case YM2151_CLEAR:	irqvector &= ~0x2;	break;
		case V30_ASSERT:	irqvector |= 0x1;	break;
		case V30_CLEAR:		irqvector &= ~0x1;	break;
	}

	if (irqvector & 0x2)		/* YM2151 has precedence */
		cpu_irq_line_vector_w(1,0,0x18);
	else if (irqvector & 0x1)	/* V30 */
		cpu_irq_line_vector_w(1,0,0x19);

	if (irqvector == 0)	/* no IRQs pending */
		cpu_set_irq_line(1,0,CLEAR_LINE);
	else	/* IRQ pending */
		cpu_set_irq_line(1,0,ASSERT_LINE);
}

static WRITE_HANDLER( m92_soundlatch_w )
{
	if (offset==0)
	{
		timer_set(TIME_NOW,V30_ASSERT,setvector_callback);
		soundlatch_w(0,data);
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "soundlatch_w %02x\n",data);*/
	}
}

static READ_HANDLER( m92_sound_status_r )
{
/*logerror("%06x: read sound status\n",activecpu_get_pc());*/
	if (offset == 0)
		return sound_status&0xff;
	return sound_status>>8;
}

static READ_HANDLER( m92_soundlatch_r )
{
	if (offset == 0)
	{
		int res = soundlatch_r(offset);
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "soundlatch_r %02x\n",res);*/
		return res;
	}
	else return 0xff;
}

static WRITE_HANDLER( m92_sound_irq_ack_w )
{
	if (offset == 0)
		timer_set(TIME_NOW,V30_CLEAR,setvector_callback);
}

static WRITE_HANDLER( m92_sound_status_w )
{
	if (offset == 0) {
		sound_status = data | (sound_status&0xff00);
		cpu_set_irq_line_and_vector(0,0,HOLD_LINE,M92_IRQ_3);
	}
	else
		sound_status = (data<<8) | (sound_status&0xff);
}

static WRITE_HANDLER( m92_sound_reset_w )	/* Added sound reset line for IREM M92 */
{
  cpu_set_reset_line( 1, (data) ? CLEAR_LINE : ASSERT_LINE);
}

/*****************************************************************************/

static MEMORY_READ_START( readmem )
	{ 0x00000, 0x9ffff, MRA_ROM },
	{ 0xa0000, 0xbffff, MRA_BANK1 },
	{ 0xc0000, 0xcffff, MRA_BANK2 }, /* Mirror of rom:  Used by In The Hunt as protection */
	{ 0xd0000, 0xdffff, m92_vram_r },
	{ 0xe0000, 0xeffff, MRA_RAM },
	{ 0xf8000, 0xf87ff, MRA_RAM },
	{ 0xf8800, 0xf8fff, m92_paletteram_r },
	{ 0xffff0, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( writemem )
	{ 0x00000, 0xbffff, MWA_ROM },
	{ 0xd0000, 0xdffff, m92_vram_w, &m92_vram_data },
	{ 0xe0000, 0xeffff, MWA_RAM, &m92_ram }, /* System ram */
	{ 0xf8000, 0xf87ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf8800, 0xf8fff, m92_paletteram_w },
	{ 0xf9000, 0xf900f, m92_spritecontrol_w, &m92_spritecontrol },
	{ 0xf9800, 0xf9801, m92_videocontrol_w },
	{ 0xffff0, 0xfffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( lethalth_readmem ) /* Same as above but with different VRAM addressing PAL */
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0x80000, 0x8ffff, m92_vram_r },
	{ 0xe0000, 0xeffff, MRA_RAM },
	{ 0xf8000, 0xf87ff, MRA_RAM },
	{ 0xf8800, 0xf8fff, paletteram_r },
	{ 0xffff0, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( lethalth_writemem )
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0x80000, 0x8ffff, m92_vram_w, &m92_vram_data },
	{ 0xe0000, 0xeffff, MWA_RAM, &m92_ram }, /* System ram */
	{ 0xf8000, 0xf87ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf8800, 0xf8fff, m92_paletteram_w },
	{ 0xf9000, 0xf900f, m92_spritecontrol_w, &m92_spritecontrol },
	{ 0xf9800, 0xf9801, m92_videocontrol_w },
	{ 0xffff0, 0xfffff, MWA_ROM },
MEMORY_END

static PORT_READ_START( readport )
	{ 0x00, 0x00, input_port_0_r }, /* Player 1 */
	{ 0x01, 0x01, input_port_1_r }, /* Player 2 */
	{ 0x02, 0x02, m92_port_4_r },   /* Coins & VBL */
	{ 0x03, 0x03, input_port_7_r }, /* Dip 3 */
	{ 0x04, 0x04, input_port_6_r }, /* Dip 2 */
	{ 0x05, 0x05, input_port_5_r }, /* Dip 1 */
	{ 0x06, 0x06, input_port_2_r }, /* Player 3 */
	{ 0x07, 0x07, input_port_3_r },	/* Player 4 */
	{ 0x08, 0x09, m92_sound_status_r },	/* answer from sound CPU */
PORT_END

static PORT_WRITE_START( writeport )
	{ 0x00, 0x01, m92_soundlatch_w },
	{ 0x02, 0x03, m92_coincounter_w },
	{ 0x20, 0x21, m92_bankswitch_w },
	{ 0x40, 0x43, MWA_NOP }, /* Interrupt controller, only written to at bootup */
	{ 0x80, 0x87, m92_pf1_control_w },
	{ 0x88, 0x8f, m92_pf2_control_w },
	{ 0x90, 0x97, m92_pf3_control_w },
	{ 0x98, 0x9f, m92_master_control_w },
/*	{ 0xc0, 0xc1, m92_sound_reset_w }, */ /* causes breakage with certain games */
PORT_END

static PORT_WRITE_START( psoldier_writeport )
	{ 0x00, 0x01, m92_soundlatch_w },
	{ 0x02, 0x03, m92_coincounter_w },
	{ 0x20, 0x21, m92_bankswitch_w },
	{ 0x40, 0x43, MWA_NOP }, /* Interrupt controller, only written to at bootup */
	{ 0x80, 0x87, m92_pf1_control_w },
	{ 0x88, 0x8f, m92_pf2_control_w },
	{ 0x90, 0x97, m92_pf3_control_w },
	{ 0x98, 0x9f, m92_master_control_w },
	{ 0xc0, 0xc1, m92_sound_reset_w },
PORT_END

/******************************************************************************/

static MEMORY_READ_START( sound_readmem )
	{ 0x00000, 0x1ffff, MRA_ROM },
	{ 0xa0000, 0xa3fff, MRA_RAM },
	{ 0xa8000, 0xa803f, IremGA20_r },
	{ 0xa8042, 0xa8043, YM2151_status_port_0_r },
	{ 0xa8044, 0xa8045, m92_soundlatch_r },
	{ 0xffff0, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x00000, 0x1ffff, MWA_ROM },
	{ 0x9ff00, 0x9ffff, MWA_NOP }, /* Irq controller? */
	{ 0xa0000, 0xa3fff, MWA_RAM, &m92_snd_ram },
	{ 0xa8000, 0xa803f, IremGA20_w },
	{ 0xa8040, 0xa8041, YM2151_register_port_0_w },
	{ 0xa8042, 0xa8043, YM2151_data_port_0_w },
	{ 0xa8044, 0xa8045, m92_sound_irq_ack_w },
	{ 0xa8046, 0xa8047, m92_sound_status_w },
	{ 0xffff0, 0xfffff, MWA_ROM },
MEMORY_END

/******************************************************************************/

INPUT_PORTS_START( bmaster )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "300k only" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( gunforce )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "15000 35000 75000 120000" )
	PORT_DIPSETTING(    0x10, "20000 40000 90000 150000" )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_UNUSED	/* Game manual only mentions 2 dips */
INPUT_PORTS_END

INPUT_PORTS_START( lethalth )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, "Continuous Play" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( hook )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	IREM_JOYSTICK_3_4(3)
	IREM_JOYSTICK_3_4(4)
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH_4PLAYERS

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Any Button to Start" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( majtitl2 )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	IREM_JOYSTICK_3_4(3)
	IREM_JOYSTICK_3_4(4)
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH_4PLAYERS

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Any Button to Start" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, "Clear Data" ) /* This could actually be a service button, rather tham a DIP */
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( mysticri )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( uccops )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	IREM_JOYSTICK_3_4(3)
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH_3PLAYERS

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	/* There is ALLWAYS a Bonus Life at 300K */
	/* It does not depends on the value of bit 0x10 */
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Any Button to Start" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( rtypeleo )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* Buy in/coin mode?? */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_UNUSED	/* Game manual only mentions 2 dips */
INPUT_PORTS_END

INPUT_PORTS_START( inthunt )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( nbbatman )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	IREM_JOYSTICK_3_4(3)
	IREM_JOYSTICK_3_4(4)
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH_4PLAYERS

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Any Button to Start" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( psoldier )
	PORT_START
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )

	PORT_UNUSED

	PORT_START /* Extra connector for kick buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )

	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) ) /* Probably difficulty */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* One of these is continue */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Any Button to Start" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( dsccr94j )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	IREM_JOYSTICK_3_4(3)
	IREM_JOYSTICK_3_4(4)
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH_4PLAYERS /* Dip Switch 2, dip 2 is listed as "Don't Change" and is "OFF" */

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, "Time" )
	PORT_DIPSETTING(    0x00, "1:30" )
	PORT_DIPSETTING(    0x03, "2:00" )
	PORT_DIPSETTING(    0x02, "2:30" )
	PORT_DIPSETTING(    0x01, "3:00" )
	PORT_DIPNAME( 0x0c, 0x0c, "Difficulty" )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, "Game Mode" )
	PORT_DIPSETTING(    0x10, "Match Mode" )
	PORT_DIPSETTING(    0x00, "Power Mode" )
/*
   Match Mode: Winner advances to the next game.  Game Over for the loser
   Power Mode: The Players can play the game until their respective powers run
               out, reguardless of whether they win or lose the game.
               Player 2 can join in any time during the game
               Player power (time) can be adjusted by dip switch #3
*/
	PORT_DIPNAME( 0x20, 0x20, "Starting Button" )
	PORT_DIPSETTING(    0x00, "Button 1" )
	PORT_DIPSETTING(    0x20, "Start Button" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 3 */
	PORT_DIPNAME( 0x03, 0x03, "Player Power" )
	PORT_DIPSETTING(    0x00, "500" )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPSETTING(    0x01, "1500" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( gunforc2 )
	IREM_JOYSTICK_1_2(1)
	IREM_JOYSTICK_1_2(2)
	PORT_UNUSED
	PORT_UNUSED
	IREM_COINS
	IREM_SYSTEM_DIPSWITCH

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Very Easy" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "15000 35000 75000 120000" )
	PORT_DIPSETTING(    0x10, "20000 40000 90000 150000" )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_UNUSED	/* Game manual only mentions 2 dips */
INPUT_PORTS_END

/***************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	RGN_FRAC(1,4),
	4,	/* 4 bits per pixel */
	{ RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8
};

static struct GfxLayout spritelayout2 =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 128 },
	{ REGION_GFX2, 0, &spritelayout, 0, 128 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo2[] =
{
	{ REGION_GFX1, 0, &charlayout,    0, 128 },
	{ REGION_GFX2, 0, &spritelayout2, 0, 128 },
	{ -1 } /* end of array */
};

/***************************************************************************/

static void sound_irq(int state)
{
	if (state)
		timer_set(TIME_NOW,YM2151_ASSERT,setvector_callback);
	else
		timer_set(TIME_NOW,YM2151_CLEAR,setvector_callback);
}

static struct YM2151interface ym2151_interface =
{
	1,
	14318180/4,
	{ YM3012_VOL(40,MIXER_PAN_LEFT,40,MIXER_PAN_RIGHT) },
	{ sound_irq }
};

static struct IremGA20_interface iremGA20_interface =
{
	14318180/4,
	REGION_SOUND1,
	{ MIXER(100,MIXER_PAN_LEFT), MIXER(100,MIXER_PAN_RIGHT) },
};

/***************************************************************************/

static INTERRUPT_GEN( m92_interrupt )
{
	if (osd_skip_this_frame()==0)
		m92_vh_raster_partial_refresh(Machine->scrbitmap,0,249);

	cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, M92_IRQ_0); /* VBL */
}

static INTERRUPT_GEN( m92_raster_interrupt )
{
	static int last_line=0;
	int line = M92_SCANLINES - cpu_getiloops();

	/* Raster interrupt */
	if (m92_raster_enable && line==m92_raster_irq_position) {
		if (osd_skip_this_frame()==0)
			m92_vh_raster_partial_refresh(Machine->scrbitmap,last_line,line+1);
		last_line=line+1;
		cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, M92_IRQ_2);
	}

	/* Redraw screen, then set vblank and trigger the VBL interrupt */
	else if (line==249) { /* 248 is last visible line */
		if (osd_skip_this_frame()==0)
			m92_vh_raster_partial_refresh(Machine->scrbitmap,last_line,249);
		last_line=249;
		cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, M92_IRQ_0);
	}

	/* End of vblank */
	else if (line==M92_SCANLINES-1) {
		last_line=0;
	}
}

void m92_sprite_interrupt(void)
{
	cpu_set_irq_line_and_vector(0,0,HOLD_LINE,M92_IRQ_1);
}

static MACHINE_DRIVER_START( raster )

	/* basic machine hardware */
	MDRV_CPU_ADD(V33,18000000/2)	/* NEC V33, 18 MHz clock */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(m92_raster_interrupt,M92_SCANLINES) /* First visible line 8? */

	MDRV_CPU_ADD(V30, 14318180/2)	/* 14.31818 MHz */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(512, 512)
	MDRV_VISIBLE_AREA(80, 511-112, 128+8, 511-128-8) /* 320 x 240 */
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(m92)
	MDRV_VIDEO_UPDATE(m92)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151, ym2151_interface)
	MDRV_SOUND_ADD(IREMGA20, iremGA20_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( nonraster )

	/* basic machine hardware */
	MDRV_CPU_ADD(V33, 18000000/2)	 /* NEC V33, 18 MHz clock */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(m92_interrupt,1)

	MDRV_CPU_ADD(V30, 14318180/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 14.31818 MHz */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(512, 512)
	MDRV_VISIBLE_AREA(80, 511-112, 128+8, 511-128-8) /* 320 x 240 */
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(m92)
	MDRV_VIDEO_UPDATE(m92)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151, ym2151_interface)
	MDRV_SOUND_ADD(IREMGA20, iremGA20_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( lethalth )

	/* basic machine hardware */
	MDRV_CPU_ADD(V33, 18000000/2)	/* NEC V33, 18 MHz clock */
	MDRV_CPU_MEMORY(lethalth_readmem,lethalth_writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(m92_interrupt,1)

	MDRV_CPU_ADD(V30, 14318180/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 14.31818 MHz */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(512, 512)
	MDRV_VISIBLE_AREA(80, 511-112, 128+8, 511-128-8) /* 320 x 240 */
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(m92)
	MDRV_VIDEO_UPDATE(m92)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151, ym2151_interface)
	MDRV_SOUND_ADD(IREMGA20, iremGA20_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( psoldier )

	/* basic machine hardware */
	MDRV_CPU_ADD(V33, 18000000/2)		/* NEC V33, 18 MHz clock */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(readport,psoldier_writeport)
	MDRV_CPU_VBLANK_INT(m92_interrupt,1)

	MDRV_CPU_ADD(V30, 14318180/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 14.31818 MHz */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(512, 512)
	MDRV_VISIBLE_AREA(80, 511-112, 128+8, 511-128-8) /* 320 x 240 */
	MDRV_GFXDECODE(gfxdecodeinfo2)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(m92)
	MDRV_VIDEO_UPDATE(m92)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151, ym2151_interface)
	MDRV_SOUND_ADD(IREMGA20, iremGA20_interface)
MACHINE_DRIVER_END

/***************************************************************************/

ROM_START( bmaster )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bm_d-h0.rom",  0x000001, 0x40000, CRC(49b257c7) SHA1(cb4917ef6c5f959094f95b8535ea12e6b9b0bcc2) )
	ROM_LOAD16_BYTE( "bm_d-l0.rom",  0x000000, 0x40000, CRC(a873523e) SHA1(9aee134c299e12064842e16db296f4259eccdf5b) )
	ROM_LOAD16_BYTE( "bm_d-h1.rom",  0x080001, 0x10000, CRC(082b7158) SHA1(ca2cfcb3ecd1f130d3fb893f08d53521e7d443d4) )
	ROM_LOAD16_BYTE( "bm_d-l1.rom",  0x080000, 0x10000, CRC(6ff0c04e) SHA1(7293a50445053101d22bc596d13e1a7ed67a65c6) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "bm_d-sh0.rom",  0x000001, 0x10000, CRC(9f7c075b) SHA1(1dd3fb4dc41d3adea9ca8d1b4363dadebea49bda) )
	ROM_LOAD16_BYTE( "bm_d-sl0.rom",  0x000000, 0x10000, CRC(1fa87c89) SHA1(971eae7dd2591191ed7a948a444387896735e149) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "bm_c0.rom",       0x000000, 0x40000, CRC(2cc966b8) SHA1(4d55954813efe975b7e644448effc61b22896e0b) )
	ROM_LOAD( "bm_c1.rom",       0x040000, 0x40000, CRC(46df773e) SHA1(6f075492c06768f7d2315906ec1349fe09def22f) )
	ROM_LOAD( "bm_c2.rom",       0x080000, 0x40000, CRC(05b867bd) SHA1(d44667f3f4908bacb6e10becc431b0f213c20407) )
	ROM_LOAD( "bm_c3.rom",       0x0c0000, 0x40000, CRC(0a2227a4) SHA1(30499e99f3731993607e04c77637f6bbe641c05c) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "bm_000.rom",      0x000000, 0x80000, CRC(339fc9f3) SHA1(36be0f3b5add2ecf3f602933f5456091daaeb1f6) )
	ROM_LOAD( "bm_010.rom",      0x080000, 0x80000, CRC(6a14377d) SHA1(699e5b1984810ee9e504f9ddaec604671c0cb0b7) )
	ROM_LOAD( "bm_020.rom",      0x100000, 0x80000, CRC(31532198) SHA1(7a285e003a7c359f5b1afe4da3b44069f716f7b5) )
	ROM_LOAD( "bm_030.rom",      0x180000, 0x80000, CRC(d1a041d3) SHA1(84a8cf5911426ed785cb678395f52da0a9199546) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "bm_da.rom",       0x000000, 0x80000, CRC(62ce5798) SHA1(f7bf7706f71ce36d85c99e531d4789c4d7a095a0) )
ROM_END

ROM_START( skingame )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "is-h0-d",  0x000001, 0x40000, CRC(80940abb) SHA1(7af5b667383f40987cc8190d81937410ea2c0301) )
	ROM_LOAD16_BYTE( "is-l0-d",  0x000000, 0x40000, CRC(b84beed6) SHA1(b026a68623d7d96545a4b01770fc6cdd2a0ed0f4) )
	ROM_LOAD16_BYTE( "is-h1",    0x100001, 0x40000, CRC(9ba8e1f2) SHA1(ae86697a97223d236e2e6dd33ddb8105b9f926cb) )
	ROM_LOAD16_BYTE( "is-l1",    0x100000, 0x40000, CRC(e4e00626) SHA1(e8c6c7ad6a367da4036915a155c8695ad90ae47b) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 64k for the audio CPU */
	ROM_LOAD16_BYTE( "mt2sh0",  0x000001, 0x10000, CRC(1ecbea43) SHA1(8d66ef419f75569f2c83a89c3985742b8a47914f) )
	ROM_LOAD16_BYTE( "mt2sl0",  0x000000, 0x10000, CRC(8fd5b531) SHA1(92cae3f6dac7f89b559063de3be2f38587536b65) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "c0",       0x000000, 0x40000, CRC(7e61e4b5) SHA1(d0164862937bd506e701777c51dea1ddb3e2eda4) )
	ROM_LOAD( "c1",       0x040000, 0x40000, CRC(0a667564) SHA1(d122e0619ae5cc0202f30270933784c954eb1e5d) )
	ROM_LOAD( "c2",       0x080000, 0x40000, CRC(5eb44312) SHA1(75b584b63d4f4f2236a679235461f11004aa317f) )
	ROM_LOAD( "c3",       0x0c0000, 0x40000, CRC(f2866294) SHA1(75e0071bf6282c93034dc7e73466af0f51046d01) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "k30",      0x000000, 0x100000, CRC(8c9a2678) SHA1(e8ed119c16ddd59af9e83d243e7be25974f7cbf8) )
	ROM_LOAD( "k31",      0x100000, 0x100000, CRC(5455df78) SHA1(9e49bde1d5a310ff611932c3429601fbddf3a7b1) )
	ROM_LOAD( "k32",      0x200000, 0x100000, CRC(3a258c41) SHA1(1d93fcd01728929848b782870f80a8cd0af44796) )
	ROM_LOAD( "k33",      0x300000, 0x100000, CRC(c1e91a14) SHA1(1f0dbd99d8c5067dc3f8795fc3f1bd4466f64156) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "da",        0x000000, 0x80000, CRC(713b9e9f) SHA1(91384d67d4ba9c7d926fbecb077293c661b8ec83) )

	ROM_REGION( 0x4000, REGION_USER1, 0 )	/* EEPROM */
	ROM_LOAD( "mt2eep",       0x000000, 0x800, CRC(208af971) SHA1(69384cac24b7af35a031f9b60e035131a8b10cb2) )
ROM_END

ROM_START( majtitl2 )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "mt2-ho-b.5m",0x000001, 0x40000, CRC(b163b12e) SHA1(cdb01a5266bd11f4cff1cb5c05cf24de13a527b2) )
	ROM_LOAD16_BYTE( "mt2-lo-b.5f",0x000000, 0x40000, CRC(6f3b5d9d) SHA1(a39f25f29195023fb507dc9ffbfcbd57a4e6b30a) )
	ROM_LOAD16_BYTE( "is-h1",      0x100001, 0x40000, CRC(9ba8e1f2) SHA1(ae86697a97223d236e2e6dd33ddb8105b9f926cb) )
	ROM_LOAD16_BYTE( "is-l1",      0x100000, 0x40000, CRC(e4e00626) SHA1(e8c6c7ad6a367da4036915a155c8695ad90ae47b) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 64k for the audio CPU */
	ROM_LOAD16_BYTE( "mt2sh0",  0x000001, 0x10000, CRC(1ecbea43) SHA1(8d66ef419f75569f2c83a89c3985742b8a47914f) )
	ROM_LOAD16_BYTE( "mt2sl0",  0x000000, 0x10000, CRC(8fd5b531) SHA1(92cae3f6dac7f89b559063de3be2f38587536b65) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "c0",       0x000000, 0x40000, CRC(7e61e4b5) SHA1(d0164862937bd506e701777c51dea1ddb3e2eda4) )
	ROM_LOAD( "c1",       0x040000, 0x40000, CRC(0a667564) SHA1(d122e0619ae5cc0202f30270933784c954eb1e5d) )
	ROM_LOAD( "c2",       0x080000, 0x40000, CRC(5eb44312) SHA1(75b584b63d4f4f2236a679235461f11004aa317f) )
	ROM_LOAD( "c3",       0x0c0000, 0x40000, CRC(f2866294) SHA1(75e0071bf6282c93034dc7e73466af0f51046d01) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "k30",      0x000000, 0x100000, CRC(8c9a2678) SHA1(e8ed119c16ddd59af9e83d243e7be25974f7cbf8) )
	ROM_LOAD( "k31",      0x100000, 0x100000, CRC(5455df78) SHA1(9e49bde1d5a310ff611932c3429601fbddf3a7b1) )
	ROM_LOAD( "k32",      0x200000, 0x100000, CRC(3a258c41) SHA1(1d93fcd01728929848b782870f80a8cd0af44796) )
	ROM_LOAD( "k33",      0x300000, 0x100000, CRC(c1e91a14) SHA1(1f0dbd99d8c5067dc3f8795fc3f1bd4466f64156) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "da",        0x000000, 0x80000, CRC(713b9e9f) SHA1(91384d67d4ba9c7d926fbecb077293c661b8ec83) )

	ROM_REGION( 0x4000, REGION_USER1, 0 )	/* EEPROM */
	ROM_LOAD( "mt2eep",       0x000000, 0x800, CRC(208af971) SHA1(69384cac24b7af35a031f9b60e035131a8b10cb2) )
ROM_END

ROM_START( skingam2 )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "mt2h0a", 0x000001, 0x40000, CRC(7c6dbbc7) SHA1(6ac2df542cbcac782d733aaa0f2e4ded702ec24f) )
	ROM_LOAD16_BYTE( "mt2l0a", 0x000000, 0x40000, CRC(9de5f689) SHA1(ea5057cab0a2f5c4586337fc5a17f1a728450cbf) )
	ROM_LOAD16_BYTE( "is-h1",  0x100001, 0x40000, CRC(9ba8e1f2) SHA1(ae86697a97223d236e2e6dd33ddb8105b9f926cb) )
	ROM_LOAD16_BYTE( "is-l1",  0x100000, 0x40000, CRC(e4e00626) SHA1(e8c6c7ad6a367da4036915a155c8695ad90ae47b) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "mt2sh0",  0x000001, 0x10000, CRC(1ecbea43) SHA1(8d66ef419f75569f2c83a89c3985742b8a47914f) )
	ROM_LOAD16_BYTE( "mt2sl0",  0x000000, 0x10000, CRC(8fd5b531) SHA1(92cae3f6dac7f89b559063de3be2f38587536b65) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "c0",       0x000000, 0x40000, CRC(7e61e4b5) SHA1(d0164862937bd506e701777c51dea1ddb3e2eda4) )
	ROM_LOAD( "c1",       0x040000, 0x40000, CRC(0a667564) SHA1(d122e0619ae5cc0202f30270933784c954eb1e5d) )
	ROM_LOAD( "c2",       0x080000, 0x40000, CRC(5eb44312) SHA1(75b584b63d4f4f2236a679235461f11004aa317f) )
	ROM_LOAD( "c3",       0x0c0000, 0x40000, CRC(f2866294) SHA1(75e0071bf6282c93034dc7e73466af0f51046d01) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "k30",      0x000000, 0x100000, CRC(8c9a2678) SHA1(e8ed119c16ddd59af9e83d243e7be25974f7cbf8) )
	ROM_LOAD( "k31",      0x100000, 0x100000, CRC(5455df78) SHA1(9e49bde1d5a310ff611932c3429601fbddf3a7b1) )
	ROM_LOAD( "k32",      0x200000, 0x100000, CRC(3a258c41) SHA1(1d93fcd01728929848b782870f80a8cd0af44796) )
	ROM_LOAD( "k33",      0x300000, 0x100000, CRC(c1e91a14) SHA1(1f0dbd99d8c5067dc3f8795fc3f1bd4466f64156) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "da",        0x000000, 0x80000, CRC(713b9e9f) SHA1(91384d67d4ba9c7d926fbecb077293c661b8ec83) )

	ROM_REGION( 0x4000, REGION_USER1, 0 )	/* EEPROM */
	ROM_LOAD( "mt2eep",       0x000000, 0x800, CRC(208af971) SHA1(69384cac24b7af35a031f9b60e035131a8b10cb2) )
ROM_END

ROM_START( gunforce )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "gf_h0-c.rom",  0x000001, 0x20000, CRC(c09bb634) SHA1(9b0e3174beeef173f5ef70f86f5db42bb01d9989) )
	ROM_LOAD16_BYTE( "gf_l0-c.rom",  0x000000, 0x20000, CRC(1bef6f7d) SHA1(ff4d674fc5f97f5b298b4b5dc73fb8a6772b5f09) )
	ROM_LOAD16_BYTE( "gf_h1-c.rom",  0x040001, 0x20000, CRC(c84188b7) SHA1(ff710be742f610d90538db296acdd435260bef12) )
	ROM_LOAD16_BYTE( "gf_l1-c.rom",  0x040000, 0x20000, CRC(b189f72a) SHA1(f17d87349a57e1a4b20c4947e41edd7c39eaca13) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "gf_sh0.rom",0x000001, 0x010000, CRC(3f8f16e0) SHA1(a9f568c1b585c2cf13b21716954dac0a89936fc6) )
	ROM_LOAD16_BYTE( "gf_sl0.rom",0x000000, 0x010000, CRC(db0b13a3) SHA1(6723026010610b706725a5284a7b8d70fe479dae) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "gf_c0.rom",       0x000000, 0x40000, CRC(b3b74979) SHA1(b5b4a4775e0b28c3f37273f93f69886c911af4aa) )
	ROM_LOAD( "gf_c1.rom",       0x040000, 0x40000, CRC(f5c8590a) SHA1(a7f90f23051f8ab2b2d925e950a5ef3c260170ca) )
	ROM_LOAD( "gf_c2.rom",       0x080000, 0x40000, CRC(30f9fb64) SHA1(f86e01b0d74a1f6c19d97d6d0e0f624f050dad10) )
	ROM_LOAD( "gf_c3.rom",       0x0c0000, 0x40000, CRC(87b3e621) SHA1(8e2655c6e83d00c38210fdced25003793bd93d9f) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "gf_000.rom",      0x000000, 0x40000, CRC(209e8e8d) SHA1(9720be888905be709733c53da207c3406d73aeb1) )
	ROM_LOAD( "gf_010.rom",      0x040000, 0x40000, CRC(6e6e7808) SHA1(92c30eecf8f3669581720be6e49db87fbfac7d88) )
	ROM_LOAD( "gf_020.rom",      0x080000, 0x40000, CRC(6f5c3cb0) SHA1(e41572c267489e2078f8d5605c97abe2034a091a) )
	ROM_LOAD( "gf_030.rom",      0x0c0000, 0x40000, CRC(18978a9f) SHA1(aa484710a7c3561a9922f119a064f9205475ae64) )

	ROM_REGION( 0x20000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "gf-da.rom",	 0x000000, 0x020000, CRC(933ba935) SHA1(482811e01239feecf10e232566a7809d0d4f11b8) )
ROM_END

ROM_START( gunforcj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "gfbh0e.bin",  0x000001, 0x20000, CRC(43c36e0f) SHA1(08c278861568f0a2fb2699b89a4170f6843bbcb7) )
	ROM_LOAD16_BYTE( "gfbl0e.bin",  0x000000, 0x20000, CRC(24a558d8) SHA1(89a9fb737d51798bdd5c08f448d2d8b3e161396a) )
	ROM_LOAD16_BYTE( "gfbh1e.bin",  0x040001, 0x20000, CRC(d9744f5d) SHA1(056d6e6e9874c33dcebe2e0ec946117d5eaa5d76) )
	ROM_LOAD16_BYTE( "gfbl1e.bin",  0x040000, 0x20000, CRC(a0f7b61b) SHA1(5fc7fc3f57e82a9ae4e1f3c3e8e3e3b0bd3ff8f5) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "gf_sh0.rom",0x000001, 0x010000, CRC(3f8f16e0) SHA1(a9f568c1b585c2cf13b21716954dac0a89936fc6) )
	ROM_LOAD16_BYTE( "gf_sl0.rom",0x000000, 0x010000, CRC(db0b13a3) SHA1(6723026010610b706725a5284a7b8d70fe479dae) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "gf_c0.rom",       0x000000, 0x40000, CRC(b3b74979) SHA1(b5b4a4775e0b28c3f37273f93f69886c911af4aa) )
	ROM_LOAD( "gf_c1.rom",       0x040000, 0x40000, CRC(f5c8590a) SHA1(a7f90f23051f8ab2b2d925e950a5ef3c260170ca) )
	ROM_LOAD( "gf_c2.rom",       0x080000, 0x40000, CRC(30f9fb64) SHA1(f86e01b0d74a1f6c19d97d6d0e0f624f050dad10) )
	ROM_LOAD( "gf_c3.rom",       0x0c0000, 0x40000, CRC(87b3e621) SHA1(8e2655c6e83d00c38210fdced25003793bd93d9f) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "gf_000.rom",      0x000000, 0x40000, CRC(209e8e8d) SHA1(9720be888905be709733c53da207c3406d73aeb1) )
	ROM_LOAD( "gf_010.rom",      0x040000, 0x40000, CRC(6e6e7808) SHA1(92c30eecf8f3669581720be6e49db87fbfac7d88) )
	ROM_LOAD( "gf_020.rom",      0x080000, 0x40000, CRC(6f5c3cb0) SHA1(e41572c267489e2078f8d5605c97abe2034a091a) )
	ROM_LOAD( "gf_030.rom",      0x0c0000, 0x40000, CRC(18978a9f) SHA1(aa484710a7c3561a9922f119a064f9205475ae64) )

	ROM_REGION( 0x20000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "gf-da.rom",	 0x000000, 0x020000, CRC(933ba935) SHA1(482811e01239feecf10e232566a7809d0d4f11b8) )
ROM_END

ROM_START( gunforcu )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "gf_h0-d.5m",  0x000001, 0x20000, CRC(a6db7b5c) SHA1(5656473599e924ab799ea3c6f39d8ce34b08cb29) )
	ROM_LOAD16_BYTE( "gf_l0-d.5f",  0x000000, 0x20000, CRC(82cf55f6) SHA1(42a2de61f2c5294c81fb135ea2472cc78637c66c) )
	ROM_LOAD16_BYTE( "gf_h1-d.5l",  0x040001, 0x20000, CRC(08a3736c) SHA1(0ae904cf486a371f8b635c1f9dc5201e38a73f5a) )
	ROM_LOAD16_BYTE( "gf_l1-d.5j",  0x040000, 0x20000, CRC(435f524f) SHA1(65c282ec50123747880850bc32c7ace0471ed9f2) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "gf_sh0.rom",0x000001, 0x010000, CRC(3f8f16e0) SHA1(a9f568c1b585c2cf13b21716954dac0a89936fc6) )
	ROM_LOAD16_BYTE( "gf_sl0.rom",0x000000, 0x010000, CRC(db0b13a3) SHA1(6723026010610b706725a5284a7b8d70fe479dae) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "gf_c0.rom",       0x000000, 0x40000, CRC(b3b74979) SHA1(b5b4a4775e0b28c3f37273f93f69886c911af4aa) )
	ROM_LOAD( "gf_c1.rom",       0x040000, 0x40000, CRC(f5c8590a) SHA1(a7f90f23051f8ab2b2d925e950a5ef3c260170ca) )
	ROM_LOAD( "gf_c2.rom",       0x080000, 0x40000, CRC(30f9fb64) SHA1(f86e01b0d74a1f6c19d97d6d0e0f624f050dad10) )
	ROM_LOAD( "gf_c3.rom",       0x0c0000, 0x40000, CRC(87b3e621) SHA1(8e2655c6e83d00c38210fdced25003793bd93d9f) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "gf_000.rom",      0x000000, 0x40000, CRC(209e8e8d) SHA1(9720be888905be709733c53da207c3406d73aeb1) )
	ROM_LOAD( "gf_010.rom",      0x040000, 0x40000, CRC(6e6e7808) SHA1(92c30eecf8f3669581720be6e49db87fbfac7d88) )
	ROM_LOAD( "gf_020.rom",      0x080000, 0x40000, CRC(6f5c3cb0) SHA1(e41572c267489e2078f8d5605c97abe2034a091a) )
	ROM_LOAD( "gf_030.rom",      0x0c0000, 0x40000, CRC(18978a9f) SHA1(aa484710a7c3561a9922f119a064f9205475ae64) )

	ROM_REGION( 0x20000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "gf-da.rom",	 0x000000, 0x020000, CRC(933ba935) SHA1(482811e01239feecf10e232566a7809d0d4f11b8) )
ROM_END

ROM_START( inthunt )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ith-h0-d.rom",0x000001, 0x040000, CRC(52f8e7a6) SHA1(26d9e272b01e7b82019812059dcc9fbb043c6129) )
	ROM_LOAD16_BYTE( "ith-l0-d.rom",0x000000, 0x040000, CRC(5db79eb7) SHA1(ffd4228d7b88a44a82e639a5583753da183fcb23) )
	ROM_LOAD16_BYTE( "ith-h1-b.rom",0x080001, 0x020000, CRC(fc2899df) SHA1(f811ff5fd55655afdb25950d317db85c8091b6d6) )
	ROM_LOAD16_BYTE( "ith-l1-b.rom",0x080000, 0x020000, CRC(955a605a) SHA1(2515accc2f4a06b07418e45eb62e746d09c81720) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* Irem D8000011A1 */
	ROM_LOAD16_BYTE( "ith-sh0.rom",0x000001, 0x010000, CRC(209c8b7f) SHA1(eaf4a6d9222fe181df65cea1f13c3f2ebff2ec5b) )
	ROM_LOAD16_BYTE( "ith-sl0.rom",0x000000, 0x010000, CRC(18472d65) SHA1(2705e94ee350ffda272c50ea3bf605826aa19978) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "ith_ic26.rom",0x000000, 0x080000, CRC(4c1818cf) SHA1(fc8c2ae640bc3504a52736be46febb92c998fd7d) )
	ROM_LOAD( "ith_ic25.rom",0x080000, 0x080000, CRC(91145bae) SHA1(71b2695575f189a2fc72635831ba408f824d4928) )
	ROM_LOAD( "ith_ic24.rom",0x100000, 0x080000, CRC(fc03fe3b) SHA1(7e34220b9b21b82e012dcbf3052cccb118e3c382) )
	ROM_LOAD( "ith_ic23.rom",0x180000, 0x080000, CRC(ee156a0a) SHA1(4a303ed292ce79e3f990139c35b921213eb2711d) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "ith_ic34.rom",0x000000, 0x100000, CRC(a019766e) SHA1(59012a41d152a471a95f1f86b6b1e0f9dd3f9711) )
	ROM_LOAD( "ith_ic35.rom",0x100000, 0x100000, CRC(3fca3073) SHA1(bdae171cb7705647f28354ca83ecdea3a15f6e22) )
	ROM_LOAD( "ith_ic36.rom",0x200000, 0x100000, CRC(20d1b28b) SHA1(290947d77242e837444766ff5d420bc9b53b5b01) )
	ROM_LOAD( "ith_ic37.rom",0x300000, 0x100000, CRC(90b6fd4b) SHA1(99237ebab7cf4689e06965bd546cd80a825ab024) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "ith_ic9.rom" ,0x000000, 0x080000, CRC(318ee71a) SHA1(e6f49a7adf7155ba40c4f33a8fdc9553c00f5e3d) )
ROM_END

ROM_START( inthuntu )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ithhoc.bin",0x000001, 0x040000, CRC(563dcec0) SHA1(0c7588ba603926fb0b490f2ba324ff73362a54d5) )
	ROM_LOAD16_BYTE( "ithloc.bin",0x000000, 0x040000, CRC(1638c705) SHA1(8ca7a12c2f75172d4c2c808ea666b2f2e969398c) )
	ROM_LOAD16_BYTE( "ithh1a.bin",0x080001, 0x020000, CRC(0253065f) SHA1(a11e6bf014c19b2e317b75f01a7f0d7a9a85c7d3) )
	ROM_LOAD16_BYTE( "ithl1a.bin",0x080000, 0x020000, CRC(a57d688d) SHA1(aa049de5c41097b6f1da31e9bf3bac132f67aa6c) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* Irem D8000011A1 */
	ROM_LOAD16_BYTE( "ith-sh0.rom",0x000001, 0x010000, CRC(209c8b7f) SHA1(eaf4a6d9222fe181df65cea1f13c3f2ebff2ec5b) )
	ROM_LOAD16_BYTE( "ith-sl0.rom",0x000000, 0x010000, CRC(18472d65) SHA1(2705e94ee350ffda272c50ea3bf605826aa19978) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "ith_ic26.rom",0x000000, 0x080000, CRC(4c1818cf) SHA1(fc8c2ae640bc3504a52736be46febb92c998fd7d) )
	ROM_LOAD( "ith_ic25.rom",0x080000, 0x080000, CRC(91145bae) SHA1(71b2695575f189a2fc72635831ba408f824d4928) )
	ROM_LOAD( "ith_ic24.rom",0x100000, 0x080000, CRC(fc03fe3b) SHA1(7e34220b9b21b82e012dcbf3052cccb118e3c382) )
	ROM_LOAD( "ith_ic23.rom",0x180000, 0x080000, CRC(ee156a0a) SHA1(4a303ed292ce79e3f990139c35b921213eb2711d) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "ith_ic34.rom",0x000000, 0x100000, CRC(a019766e) SHA1(59012a41d152a471a95f1f86b6b1e0f9dd3f9711) )
	ROM_LOAD( "ith_ic35.rom",0x100000, 0x100000, CRC(3fca3073) SHA1(bdae171cb7705647f28354ca83ecdea3a15f6e22) )
	ROM_LOAD( "ith_ic36.rom",0x200000, 0x100000, CRC(20d1b28b) SHA1(290947d77242e837444766ff5d420bc9b53b5b01) )
	ROM_LOAD( "ith_ic37.rom",0x300000, 0x100000, CRC(90b6fd4b) SHA1(99237ebab7cf4689e06965bd546cd80a825ab024) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "ith_ic9.rom" ,0x000000, 0x080000, CRC(318ee71a) SHA1(e6f49a7adf7155ba40c4f33a8fdc9553c00f5e3d) )
ROM_END

ROM_START( kaiteids )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ith-h0j.bin",0x000001, 0x040000, CRC(dc1dec36) SHA1(f0a6e3be19752bffd9fd5f435405c8f591eab258) )
	ROM_LOAD16_BYTE( "ith-l0j.bin",0x000000, 0x040000, CRC(8835d704) SHA1(42be25ccdc31824797a17e6f76bd06edfe853833) )
	ROM_LOAD16_BYTE( "ith-h1j.bin",0x080001, 0x020000, CRC(5a7b212d) SHA1(50562d804a43aed7c34c19c8345782ac2f85caa7) )
	ROM_LOAD16_BYTE( "ith-l1j.bin",0x080000, 0x020000, CRC(4c084494) SHA1(4f32003db32f13e19dd07c66996b4328ac2a671e) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* Irem D8000011A1 */
	ROM_LOAD16_BYTE( "ith-sh0.rom",0x000001, 0x010000, CRC(209c8b7f) SHA1(eaf4a6d9222fe181df65cea1f13c3f2ebff2ec5b) )
	ROM_LOAD16_BYTE( "ith-sl0.rom",0x000000, 0x010000, CRC(18472d65) SHA1(2705e94ee350ffda272c50ea3bf605826aa19978) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "ith_ic26.rom",0x000000, 0x080000, CRC(4c1818cf) SHA1(fc8c2ae640bc3504a52736be46febb92c998fd7d) )
	ROM_LOAD( "ith_ic25.rom",0x080000, 0x080000, CRC(91145bae) SHA1(71b2695575f189a2fc72635831ba408f824d4928) )
	ROM_LOAD( "ith_ic24.rom",0x100000, 0x080000, CRC(fc03fe3b) SHA1(7e34220b9b21b82e012dcbf3052cccb118e3c382) )
	ROM_LOAD( "ith_ic23.rom",0x180000, 0x080000, CRC(ee156a0a) SHA1(4a303ed292ce79e3f990139c35b921213eb2711d) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "ith_ic34.rom",0x000000, 0x100000, CRC(a019766e) SHA1(59012a41d152a471a95f1f86b6b1e0f9dd3f9711) )
	ROM_LOAD( "ith_ic35.rom",0x100000, 0x100000, CRC(3fca3073) SHA1(bdae171cb7705647f28354ca83ecdea3a15f6e22) )
	ROM_LOAD( "ith_ic36.rom",0x200000, 0x100000, CRC(20d1b28b) SHA1(290947d77242e837444766ff5d420bc9b53b5b01) )
	ROM_LOAD( "ith_ic37.rom",0x300000, 0x100000, CRC(90b6fd4b) SHA1(99237ebab7cf4689e06965bd546cd80a825ab024) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "ith_ic9.rom" ,0x000000, 0x080000, CRC(318ee71a) SHA1(e6f49a7adf7155ba40c4f33a8fdc9553c00f5e3d) )
ROM_END

ROM_START( hook )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "h-h0-d.rom",0x000001, 0x040000, CRC(40189ff6) SHA1(ed86a566f0f47c03dd0628cda8b31a167788116c) )
	ROM_LOAD16_BYTE( "h-l0-d.rom",0x000000, 0x040000, CRC(14567690) SHA1(74ddc300e81b006fdc57a4a86f5f178a30732dd8) )
	ROM_LOAD16_BYTE( "h-h1.rom",  0x080001, 0x020000, CRC(264ba1f0) SHA1(49ecf9b3e5375629607fb747abe264406065580b) )
	ROM_LOAD16_BYTE( "h-l1.rom",  0x080000, 0x020000, CRC(f9913731) SHA1(be7871d6843e76f66fae6b501c5ee83ccc366463) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "h-sh0.rom",0x000001, 0x010000, CRC(86a4e56e) SHA1(61163010e713be64368a4126f17d33cbdcf0c5ed) )
	ROM_LOAD16_BYTE( "h-sl0.rom",0x000000, 0x010000, CRC(10fd9676) SHA1(1b51181a8f0711997e107e9a8b8f44341d08ea81) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "hook-c0.rom",0x000000, 0x040000, CRC(dec63dcf) SHA1(e9869110f832d782c460b123928b042c65fdf8bd) )
	ROM_LOAD( "hook-c1.rom",0x040000, 0x040000, CRC(e4eb0b92) SHA1(159da3ec973490a153c69c96c1373cf4e0290736) )
	ROM_LOAD( "hook-c2.rom",0x080000, 0x040000, CRC(a52b320b) SHA1(1522562239bb3b93ef552c47445daa4ee021495c) )
	ROM_LOAD( "hook-c3.rom",0x0c0000, 0x040000, CRC(7ef67731) SHA1(af0b0ee6e1c06af04c609af7e077d4a7d76d8817) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "hook-000.rom",0x000000, 0x100000, CRC(ccceac30) SHA1(16e2b4393840344debe869034135feead7450184) )
	ROM_LOAD( "hook-010.rom",0x100000, 0x100000, CRC(8ac8da67) SHA1(a9b962cb0bc0d8bc3bda8a0ed1ce06641d666b41) )
	ROM_LOAD( "hook-020.rom",0x200000, 0x100000, CRC(8847af9a) SHA1(f82cdbd640fac373136219422172ca9fbf5d1830) )
	ROM_LOAD( "hook-030.rom",0x300000, 0x100000, CRC(239e877e) SHA1(445e1096619c4e3a2d5b50a645fd45bd7c501590) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "hook-da.rom" ,0x000000, 0x080000, CRC(88cd0212) SHA1(789532f5544b5d024d8af60eb8a5c133ae0d19d4) )
ROM_END

ROM_START( hooku )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "h0-c.3h",0x000001, 0x040000, CRC(84cc239e) SHA1(0a3011cd64cd27336b967b1b2446c8916b8be8e7) )
	ROM_LOAD16_BYTE( "l0-c.5h",0x000000, 0x040000, CRC(45e194fe) SHA1(2049f242ea3058d42004bafb8b208759020be5bc) )
	ROM_LOAD16_BYTE( "h-h1.rom",  0x080001, 0x020000, CRC(264ba1f0) SHA1(49ecf9b3e5375629607fb747abe264406065580b) )
	ROM_LOAD16_BYTE( "h-l1.rom",  0x080000, 0x020000, CRC(f9913731) SHA1(be7871d6843e76f66fae6b501c5ee83ccc366463) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "h-sh0.rom",0x000001, 0x010000, CRC(86a4e56e) SHA1(61163010e713be64368a4126f17d33cbdcf0c5ed) )
	ROM_LOAD16_BYTE( "h-sl0.rom",0x000000, 0x010000, CRC(10fd9676) SHA1(1b51181a8f0711997e107e9a8b8f44341d08ea81) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "hook-c0.rom",0x000000, 0x040000, CRC(dec63dcf) SHA1(e9869110f832d782c460b123928b042c65fdf8bd) )
	ROM_LOAD( "hook-c1.rom",0x040000, 0x040000, CRC(e4eb0b92) SHA1(159da3ec973490a153c69c96c1373cf4e0290736) )
	ROM_LOAD( "hook-c2.rom",0x080000, 0x040000, CRC(a52b320b) SHA1(1522562239bb3b93ef552c47445daa4ee021495c) )
	ROM_LOAD( "hook-c3.rom",0x0c0000, 0x040000, CRC(7ef67731) SHA1(af0b0ee6e1c06af04c609af7e077d4a7d76d8817) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "hook-000.rom",0x000000, 0x100000, CRC(ccceac30) SHA1(16e2b4393840344debe869034135feead7450184) )
	ROM_LOAD( "hook-010.rom",0x100000, 0x100000, CRC(8ac8da67) SHA1(a9b962cb0bc0d8bc3bda8a0ed1ce06641d666b41) )
	ROM_LOAD( "hook-020.rom",0x200000, 0x100000, CRC(8847af9a) SHA1(f82cdbd640fac373136219422172ca9fbf5d1830) )
	ROM_LOAD( "hook-030.rom",0x300000, 0x100000, CRC(239e877e) SHA1(445e1096619c4e3a2d5b50a645fd45bd7c501590) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "hook-da.rom" ,0x000000, 0x080000, CRC(88cd0212) SHA1(789532f5544b5d024d8af60eb8a5c133ae0d19d4) )
ROM_END

ROM_START( hookj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "h-h0-g.3h", 0x000001, 0x040000, CRC(5964c886) SHA1(fe15f328d0e62b6be09c8ae9892f5b669585fcdb) )
	ROM_LOAD16_BYTE( "h-l0-g.5h", 0x000000, 0x040000, CRC(7f7433f2) SHA1(e85c170332ed7195e713fd5a2a20c97d56a7297b) )
	ROM_LOAD16_BYTE( "h-h1.rom",  0x080001, 0x020000, CRC(264ba1f0) SHA1(49ecf9b3e5375629607fb747abe264406065580b) )
	ROM_LOAD16_BYTE( "h-l1.rom",  0x080000, 0x020000, CRC(f9913731) SHA1(be7871d6843e76f66fae6b501c5ee83ccc366463) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "h-sh0-a.3l", 0x000001, 0x010000, CRC(bd3d1f61) SHA1(0c884a0b5519f9c0823128872baf7b0c4078e5c4) )
	ROM_LOAD16_BYTE( "h-sl0-a.3n", 0x000000, 0x010000, CRC(76371def) SHA1(b7a86fd4eecdd8a538c32e08cd920c27bd50924b) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )/* Tiles */
	ROM_LOAD( "hook-c0.rom", 0x000000, 0x040000, CRC(dec63dcf) SHA1(e9869110f832d782c460b123928b042c65fdf8bd) )
	ROM_LOAD( "hook-c1.rom", 0x040000, 0x040000, CRC(e4eb0b92) SHA1(159da3ec973490a153c69c96c1373cf4e0290736) )
	ROM_LOAD( "hook-c2.rom", 0x080000, 0x040000, CRC(a52b320b) SHA1(1522562239bb3b93ef552c47445daa4ee021495c) )
	ROM_LOAD( "hook-c3.rom", 0x0c0000, 0x040000, CRC(7ef67731) SHA1(af0b0ee6e1c06af04c609af7e077d4a7d76d8817) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE )/* Sprites */
	ROM_LOAD( "hook-000.rom", 0x000000, 0x100000, CRC(ccceac30) SHA1(16e2b4393840344debe869034135feead7450184) )
	ROM_LOAD( "hook-010.rom", 0x100000, 0x100000, CRC(8ac8da67) SHA1(a9b962cb0bc0d8bc3bda8a0ed1ce06641d666b41) )
	ROM_LOAD( "hook-020.rom", 0x200000, 0x100000, CRC(8847af9a) SHA1(f82cdbd640fac373136219422172ca9fbf5d1830) )
	ROM_LOAD( "hook-030.rom", 0x300000, 0x100000, CRC(239e877e) SHA1(445e1096619c4e3a2d5b50a645fd45bd7c501590) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "hook-da.rom", 0x000000, 0x080000, CRC(88cd0212) SHA1(789532f5544b5d024d8af60eb8a5c133ae0d19d4) )
ROM_END

ROM_START( rtypeleo )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "rtl-h0-c",     0x000001, 0x040000, CRC(5fef7fa1) SHA1(7d18d4ea979d887d6da42c79734b8c695f3df02b) )
	ROM_LOAD16_BYTE( "rtl-l0-c",     0x000000, 0x040000, CRC(8156456b) SHA1(9755ab80feb92e3e3a36154d57ee2b53185b6816) )
	ROM_LOAD16_BYTE( "rtl-h1-d.bin", 0x080001, 0x020000, CRC(352ff444) SHA1(e302bc8dbf80abe5c1aaf02e92473fc72a796e72) )
	ROM_LOAD16_BYTE( "rtl-l1-d.bin", 0x080000, 0x020000, CRC(fd34ea46) SHA1(aca12d46ebff94505d03884e45805e84bbece6a7) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "rtl-sh0a.bin",0x000001, 0x010000, CRC(e518b4e3) SHA1(44ec1d6b27bc3e49ad967f43960398ba1a19c5e3) )
	ROM_LOAD16_BYTE( "rtl-sl0a.bin",0x000000, 0x010000, CRC(896f0d36) SHA1(9246b1a5a8717dd823340d4cb79012a3df6fa4b7) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "rtl-c0.bin", 0x000000, 0x080000, CRC(fb588d7c) SHA1(78e96db9912b22f8eff03d57e470b1ef946f7351) )
	ROM_LOAD( "rtl-c1.bin", 0x080000, 0x080000, CRC(e5541bff) SHA1(cd8293603298b7ead79a16697845603223bb6a45) )
	ROM_LOAD( "rtl-c2.bin", 0x100000, 0x080000, CRC(faa9ae27) SHA1(de6c7f1843adcaa9fce0d0d9407999babbf52e27) )
	ROM_LOAD( "rtl-c3.bin", 0x180000, 0x080000, CRC(3a2343f6) SHA1(dea1af889d6a422af3f49abf2cee91aec4d0cac3) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "rtl-000.bin",0x000000, 0x100000, CRC(82a06870) SHA1(c7233019c4dcfcab55b665a7b0973e74cca879cd) )
	ROM_LOAD( "rtl-010.bin",0x100000, 0x100000, CRC(417e7a56) SHA1(d33a40eb7ec0afde0a59799a428aadee12dd5c63) )
	ROM_LOAD( "rtl-020.bin",0x200000, 0x100000, CRC(f9a3f3a1) SHA1(b4eb9326ff992e62b70925277fbbd3ea2eabf359) )
	ROM_LOAD( "rtl-030.bin",0x300000, 0x100000, CRC(03528d95) SHA1(f2705646ee8d9e7b7f70cfd2c31b6e32798f459d) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "rtl-da.bin" ,0x000000, 0x080000, CRC(dbebd1ff) SHA1(b369d6e944331e6773608ff24f04b8f16267b8da) )
ROM_END

ROM_START( rtypelej )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "rtl-h0-d.bin", 0x000001, 0x040000, CRC(3dbac89f) SHA1(bfb4d1ab480b7828f6b7374df6d30d766f327b95) )
	ROM_LOAD16_BYTE( "rtl-l0-d.bin", 0x000000, 0x040000, CRC(f85a2537) SHA1(50eeca8de0c7fd28375d082a05f18473d0b15ed4) )
	ROM_LOAD16_BYTE( "rtl-h1-d.bin", 0x080001, 0x020000, CRC(352ff444) SHA1(e302bc8dbf80abe5c1aaf02e92473fc72a796e72) )
	ROM_LOAD16_BYTE( "rtl-l1-d.bin", 0x080000, 0x020000, CRC(fd34ea46) SHA1(aca12d46ebff94505d03884e45805e84bbece6a7) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "rtl-sh0a.bin",0x000001, 0x010000, CRC(e518b4e3) SHA1(44ec1d6b27bc3e49ad967f43960398ba1a19c5e3) )
	ROM_LOAD16_BYTE( "rtl-sl0a.bin",0x000000, 0x010000, CRC(896f0d36) SHA1(9246b1a5a8717dd823340d4cb79012a3df6fa4b7) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "rtl-c0.bin", 0x000000, 0x080000, CRC(fb588d7c) SHA1(78e96db9912b22f8eff03d57e470b1ef946f7351) )
	ROM_LOAD( "rtl-c1.bin", 0x080000, 0x080000, CRC(e5541bff) SHA1(cd8293603298b7ead79a16697845603223bb6a45) )
	ROM_LOAD( "rtl-c2.bin", 0x100000, 0x080000, CRC(faa9ae27) SHA1(de6c7f1843adcaa9fce0d0d9407999babbf52e27) )
	ROM_LOAD( "rtl-c3.bin", 0x180000, 0x080000, CRC(3a2343f6) SHA1(dea1af889d6a422af3f49abf2cee91aec4d0cac3) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "rtl-000.bin",0x000000, 0x100000, CRC(82a06870) SHA1(c7233019c4dcfcab55b665a7b0973e74cca879cd) )
	ROM_LOAD( "rtl-010.bin",0x100000, 0x100000, CRC(417e7a56) SHA1(d33a40eb7ec0afde0a59799a428aadee12dd5c63) )
	ROM_LOAD( "rtl-020.bin",0x200000, 0x100000, CRC(f9a3f3a1) SHA1(b4eb9326ff992e62b70925277fbbd3ea2eabf359) )
	ROM_LOAD( "rtl-030.bin",0x300000, 0x100000, CRC(03528d95) SHA1(f2705646ee8d9e7b7f70cfd2c31b6e32798f459d) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "rtl-da.bin" ,0x000000, 0x080000, CRC(dbebd1ff) SHA1(b369d6e944331e6773608ff24f04b8f16267b8da) )
ROM_END

ROM_START( mysticri )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "mr-h0-b.bin",  0x000001, 0x040000, CRC(d529f887) SHA1(fedfedd23fdbb8c4a19970dc4e2c8c0f96915982) )
	ROM_LOAD16_BYTE( "mr-l0-b.bin",  0x000000, 0x040000, CRC(a457ab44) SHA1(6f85428061cf384c6d645ff0aacd850730a86987) )
	ROM_LOAD16_BYTE( "mr-h1-b.bin",  0x080001, 0x010000, CRC(e17649b9) SHA1(fb09a0ccd22475d81ba667c88d1b5eb7cc64728f) )
	ROM_LOAD16_BYTE( "mr-l1-b.bin",  0x080000, 0x010000, CRC(a87c62b4) SHA1(d3cae0f420faeb4556767b6ad817fc39d31b7273) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "mr-sh0.bin",0x000001, 0x010000, CRC(50d335e4) SHA1(a1a92e95fbd6b99d904a82cea4a1ff6fd2ac8dde) )
	ROM_LOAD16_BYTE( "mr-sl0.bin",0x000000, 0x010000, CRC(0fa32721) SHA1(1561ddd2597592060b8a78f1dff6cbb25fb7cd2e) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "mr-c0.bin", 0x000000, 0x040000, CRC(872a8fad) SHA1(236406e5959c81a1cffe96fef02d637c2150ce1e) )
	ROM_LOAD( "mr-c1.bin", 0x040000, 0x040000, CRC(d2ffb27a) SHA1(fedfb430ce8a8953b2f78970d0b0dc5571de333c) )
	ROM_LOAD( "mr-c2.bin", 0x080000, 0x040000, CRC(62bff287) SHA1(cb7b73c4a26737f1a1f9cc9423ae51c284368b1b) )
	ROM_LOAD( "mr-c3.bin", 0x0c0000, 0x040000, CRC(d0da62ab) SHA1(96c7c8e1d8dafb797731652fa91d3048aa157185) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "mr-o00.bin", 0x000000, 0x080000, CRC(a0f9ce16) SHA1(ae423313d189ebddc6d5d0785ac484e0cdf79112) )
	ROM_LOAD( "mr-o10.bin", 0x100000, 0x080000, CRC(4e70a9e9) SHA1(8f6b043b03420a590a1081c99311723169126332) )
	ROM_LOAD( "mr-o20.bin", 0x200000, 0x080000, CRC(b9c468fc) SHA1(dc42a5b80cad5373fce03cc416b9d742fcbec6e9) )
	ROM_LOAD( "mr-o30.bin", 0x300000, 0x080000, CRC(cc32433a) SHA1(a1a1ab09c4bd6c9ae85529c1aa5427ad3126b914) )

	ROM_REGION( 0x40000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "mr-da.bin" ,0x000000, 0x040000, CRC(1a11fc59) SHA1(6d1f4ca688bf015ecbbe369fbc0eb5e2bcaefcfc) )
ROM_END

ROM_START( gunhohki )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "mr-h0.bin",  0x000001, 0x040000, CRC(83352270) SHA1(25393ac0ec0f91c2890bbfc8c1b12e0f6bccb2ab) )
	ROM_LOAD16_BYTE( "mr-l0.bin",  0x000000, 0x040000, CRC(9db308ae) SHA1(eadec2e07a602d104a38bf9e159865405ab11581) )
	ROM_LOAD16_BYTE( "mr-h1.bin",  0x080001, 0x010000, CRC(c9532b60) SHA1(b83322ba7bb3eea4c64dd65b3c0a5cade61841d8) )
	ROM_LOAD16_BYTE( "mr-l1.bin",  0x080000, 0x010000, CRC(6349b520) SHA1(406620d9c63ce3d6801105c8122e1d0bbe6152ad) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "mr-sh0.bin",0x000001, 0x010000, CRC(50d335e4) SHA1(a1a92e95fbd6b99d904a82cea4a1ff6fd2ac8dde) )
	ROM_LOAD16_BYTE( "mr-sl0.bin",0x000000, 0x010000, CRC(0fa32721) SHA1(1561ddd2597592060b8a78f1dff6cbb25fb7cd2e) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "mr-c0.bin", 0x000000, 0x040000, CRC(872a8fad) SHA1(236406e5959c81a1cffe96fef02d637c2150ce1e) )
	ROM_LOAD( "mr-c1.bin", 0x040000, 0x040000, CRC(d2ffb27a) SHA1(fedfb430ce8a8953b2f78970d0b0dc5571de333c) )
	ROM_LOAD( "mr-c2.bin", 0x080000, 0x040000, CRC(62bff287) SHA1(cb7b73c4a26737f1a1f9cc9423ae51c284368b1b) )
	ROM_LOAD( "mr-c3.bin", 0x0c0000, 0x040000, CRC(d0da62ab) SHA1(96c7c8e1d8dafb797731652fa91d3048aa157185) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "mr-o00.bin", 0x000000, 0x080000, CRC(a0f9ce16) SHA1(ae423313d189ebddc6d5d0785ac484e0cdf79112) )
	ROM_LOAD( "mr-o10.bin", 0x100000, 0x080000, CRC(4e70a9e9) SHA1(8f6b043b03420a590a1081c99311723169126332) )
	ROM_LOAD( "mr-o20.bin", 0x200000, 0x080000, CRC(b9c468fc) SHA1(dc42a5b80cad5373fce03cc416b9d742fcbec6e9) )
	ROM_LOAD( "mr-o30.bin", 0x300000, 0x080000, CRC(cc32433a) SHA1(a1a1ab09c4bd6c9ae85529c1aa5427ad3126b914) )

	ROM_REGION( 0x40000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "mr-da.bin" ,0x000000, 0x040000, CRC(1a11fc59) SHA1(6d1f4ca688bf015ecbbe369fbc0eb5e2bcaefcfc) )
ROM_END

ROM_START( uccops )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "uc_h0.rom",  0x000001, 0x040000, CRC(240aa5f7) SHA1(8d864bb1377e9f6d266631ed365c5809b9da33f8) )
	ROM_LOAD16_BYTE( "uc_l0.rom",  0x000000, 0x040000, CRC(df9a4826) SHA1(298033d97b9587e3548cb3bffa16b7ba9a6ff20d) )
	ROM_LOAD16_BYTE( "uc_h1.rom",  0x080001, 0x020000, CRC(8d29bcd6) SHA1(470b77d1b8f88824bac294bd12a205a23dad2287) )
	ROM_LOAD16_BYTE( "uc_l1.rom",  0x080000, 0x020000, CRC(a8a402d8) SHA1(0b40fb69f0a3e24e6b60117d2d2fd4cc170bc621) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "uc_sh0.rom", 0x000001, 0x010000, CRC(df90b198) SHA1(6b334457f06f6b9cfb355ba3d399bebb37b5733e) )
	ROM_LOAD16_BYTE( "uc_sl0.rom", 0x000000, 0x010000, CRC(96c11aac) SHA1(16c47b4f97f0532fff30bb163f26d8cf6b923a2e) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "uc_w38m.rom", 0x000000, 0x080000, CRC(130a40e5) SHA1(f70bad2fe126bb0e451a3fa6100a610928e9a502) )
	ROM_LOAD( "uc_w39m.rom", 0x080000, 0x080000, CRC(e42ca144) SHA1(ea83b1027d403e874fda6e68097814f8b9ce25d6) )
	ROM_LOAD( "uc_w40m.rom", 0x100000, 0x080000, CRC(c2961648) SHA1(b5d28638e72ab50d598e284f31bf389956ae12c6) )
	ROM_LOAD( "uc_w41m.rom", 0x180000, 0x080000, CRC(f5334b80) SHA1(6fa70ceba4f67fb0562be7b24b28bda0ffc13ef5) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "uc_k16m.rom", 0x000000, 0x100000, CRC(4a225f09) SHA1(f4d27813241fd9b020e4df8b03c852c8ecb92586) )
	ROM_LOAD( "uc_k17m.rom", 0x100000, 0x100000, CRC(e4ed9a54) SHA1(55befbd2e156c765c5e79a3176cf4336d2111293) )
	ROM_LOAD( "uc_k18m.rom", 0x200000, 0x100000, CRC(a626eb12) SHA1(826c4796c2e63f777490b43f84ffa37a6b749ca2) )
	ROM_LOAD( "uc_k19m.rom", 0x300000, 0x100000, CRC(5df46549) SHA1(87b0b799b50bf2b6ee916d9f8dfc1ee7666ce800) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "uc_w42.rom", 0x000000, 0x080000, CRC(d17d3fd6) SHA1(b02da0d01c41c7bf50cd35d6c75bacc3e3e0b85a) )
ROM_END

ROM_START( uccopsar ) /* Alpha Renewal Version */
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "uc_h0_a.ic28", 0x000001, 0x040000, CRC(9e17cada) SHA1(086bb9c1ab851cab3734c2f9188d8ff3c5f98913) )
	ROM_LOAD16_BYTE( "uc_l0_a.ic39", 0x000000, 0x040000, CRC(4a4e3208) SHA1(d61c74d46584e2c15e70f7a17b598e51981da9e8) )
	ROM_LOAD16_BYTE( "uc_h1.ic27",   0x080001, 0x020000, CRC(79d79742) SHA1(f9c03c4d42b5b3d0f0185462868b04f1bb679f90) )
	ROM_LOAD16_BYTE( "uc_l1.ic38",   0x080000, 0x020000, CRC(37211581) SHA1(b8fdff96b2c7d5cf2975dcf81c00581ccb595c15) )

	ROM_REGION(  0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "uc_sh0.ic30", 0x000001, 0x010000, CRC(f0ca1b03) SHA1(07154a2c747091f8be23587c109d91ed1672da6e) )
	ROM_LOAD16_BYTE( "uc_sl0.ic31", 0x000000, 0x010000, CRC(d1661723) SHA1(bdc00196aa2074e7b21e5949f73e9f2b93d76fd9) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "uc_c0.ic26", 0x000000, 0x080000, CRC(6a419a36) SHA1(1907d15fcc4a8bf875d19768667ee4de4702cc2a) )
	ROM_LOAD( "uc_c1.ic25", 0x080000, 0x080000, CRC(d703ecc7) SHA1(9716a8fde668e63cf3060450eb32ea43edf143d8) )
	ROM_LOAD( "uc_c2.ic24", 0x100000, 0x080000, CRC(96397ac6) SHA1(6dfe507bd9f41b5d46d85ef5f46a368745593b52) )
	ROM_LOAD( "uc_c3.ic23", 0x180000, 0x080000, CRC(5d07d10d) SHA1(ee1a928b37043c476346f189f75d2bfcc44bffe6) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "uc_030.ic37", 0x000000, 0x100000, CRC(97f7775e) SHA1(5cd147fd940b1ab6eba8e6c6f803bdcc5da5a563) )
	ROM_LOAD( "uc_020.ic36", 0x100000, 0x100000, CRC(5e0b1d65) SHA1(9e45753d10b2d7b580cd11cef74181209a424189) )
	ROM_LOAD( "uc_010.ic35", 0x200000, 0x100000, CRC(bdc224b3) SHA1(09477ec39890d954fac6ff653b9f46c9adea56b6) )
	ROM_LOAD( "uc_000.ic34", 0x300000, 0x100000, CRC(7526daec) SHA1(79431d711deb6ed09dc52be753b7b0f2c5588dc3) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "uc_da.bin", 0x000000, 0x080000, CRC(0b2855e9) SHA1(70f9decd78eab679a2ccad69e01cb303b61e0d38) )
ROM_END

ROM_START( uccopsj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "uca-h0.bin", 0x000001, 0x040000, CRC(9e17cada) SHA1(086bb9c1ab851cab3734c2f9188d8ff3c5f98913) )
	ROM_LOAD16_BYTE( "uca-l0.bin", 0x000000, 0x040000, CRC(4a4e3208) SHA1(d61c74d46584e2c15e70f7a17b598e51981da9e8) )
	ROM_LOAD16_BYTE( "uca-h1.bin", 0x080001, 0x020000, CRC(83f78dea) SHA1(6d197c3ea76beac31c3ea6e54a3ffea9d6c0c653) )
	ROM_LOAD16_BYTE( "uca-l1.bin", 0x080000, 0x020000, CRC(19628280) SHA1(e6c06cb7c37e46a7db3b4f318e836aa5a2390eda) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU - encrypted V30 = NANAO custom D80001 (?) */
	ROM_LOAD16_BYTE( "uca-sh0.bin", 0x000001, 0x010000, CRC(f0ca1b03) SHA1(07154a2c747091f8be23587c109d91ed1672da6e) )
	ROM_LOAD16_BYTE( "uca-sl0.bin", 0x000000, 0x010000, CRC(d1661723) SHA1(bdc00196aa2074e7b21e5949f73e9f2b93d76fd9) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "uca-c0.bin", 0x000000, 0x080000, CRC(6a419a36) SHA1(1907d15fcc4a8bf875d19768667ee4de4702cc2a) )
	ROM_LOAD( "uca-c1.bin", 0x080000, 0x080000, CRC(d703ecc7) SHA1(9716a8fde668e63cf3060450eb32ea43edf143d8) )
	ROM_LOAD( "uca-c2.bin", 0x100000, 0x080000, CRC(96397ac6) SHA1(6dfe507bd9f41b5d46d85ef5f46a368745593b52) )
	ROM_LOAD( "uca-c3.bin", 0x180000, 0x080000, CRC(5d07d10d) SHA1(ee1a928b37043c476346f189f75d2bfcc44bffe6) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "uca-o3.bin", 0x000000, 0x100000, CRC(97f7775e) SHA1(5cd147fd940b1ab6eba8e6c6f803bdcc5da5a563) )
	ROM_LOAD( "uca-o2.bin", 0x100000, 0x100000, CRC(5e0b1d65) SHA1(9e45753d10b2d7b580cd11cef74181209a424189) )
	ROM_LOAD( "uca-o1.bin", 0x200000, 0x100000, CRC(bdc224b3) SHA1(09477ec39890d954fac6ff653b9f46c9adea56b6) )
	ROM_LOAD( "uca-o0.bin", 0x300000, 0x100000, CRC(7526daec) SHA1(79431d711deb6ed09dc52be753b7b0f2c5588dc3) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "uca-da.bin", 0x000000, 0x080000, CRC(0b2855e9) SHA1(70f9decd78eab679a2ccad69e01cb303b61e0d38) )
ROM_END

ROM_START( lethalth )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "lt_d-h0.rom",  0x000001, 0x020000, CRC(20c68935) SHA1(edbb5322082bde7136ab015931fdcd18e5c293a8) )
	ROM_LOAD16_BYTE( "lt_d-l0.rom",  0x000000, 0x020000, CRC(e1432fb3) SHA1(4b6c22d740cd598d0e34e257910fe7c3d4f3fd32) )
	ROM_LOAD16_BYTE( "lt_d-h1.rom",  0x040001, 0x020000, CRC(d7dd3d48) SHA1(b848feee55159e334f711e4f661d415ffc1e3513) )
	ROM_LOAD16_BYTE( "lt_d-l1.rom",  0x040000, 0x020000, CRC(b94b3bd8) SHA1(7b89d9177d8b357b09317606cb2070c14c3449a5) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU */
	ROM_LOAD16_BYTE( "lt_d-sh0.rom",0x000001, 0x010000, CRC(af5b224f) SHA1(a07f2c6ca0e65af016d74b90342cfaab7535324e) )
	ROM_LOAD16_BYTE( "lt_d-sl0.rom",0x000000, 0x010000, CRC(cb3faac3) SHA1(e1ee32fac7ee9e97fbf68904572e90aa9d0c9460) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "lt_7a.rom", 0x000000, 0x040000, CRC(ada0fd50) SHA1(7eeb33360cacddf8887f3acce65350af0251936d) )
	ROM_LOAD( "lt_7b.rom", 0x040000, 0x040000, CRC(d2596883) SHA1(5a9f7384c63304c3c1e27375419d59a2b476f46a) )
	ROM_LOAD( "lt_7d.rom", 0x080000, 0x040000, CRC(2de637ef) SHA1(bd1be59d4fe9bf365454c1d471effd88aa942df6) )
	ROM_LOAD( "lt_7h.rom", 0x0c0000, 0x040000, CRC(9f6585cd) SHA1(5d59addc65c3ce20e7ea090a178fe9e17fba525b) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "lt_7j.rom", 0x000000, 0x040000, CRC(baf8863e) SHA1(fd5937cd70fcffd861a207dc4769d34459dd28d3) )
	ROM_LOAD( "lt_7l.rom", 0x040000, 0x040000, CRC(40fd50af) SHA1(d9bf1e339671fd167fad237bb5bc6d1b183686f5) )
	ROM_LOAD( "lt_7s.rom", 0x080000, 0x040000, CRC(c8e970df) SHA1(7771c17b36dcc9f01ac9e033f3f86e571c5ebbd3) )
	ROM_LOAD( "lt_7y.rom", 0x0c0000, 0x040000, CRC(f5436708) SHA1(e8cb278f4d310eeeb67e01534d17562c7fce62f0) )

	ROM_REGION( 0x40000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "lt_8a.rom" ,0x000000, 0x040000, CRC(357762a2) SHA1(d13b2a0f5d48c0171bcef708589cad194a7ea1ed) )
ROM_END

ROM_START( thndblst )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "lt_d-h0j.rom", 0x000001, 0x020000, CRC(dc218a18) SHA1(f092245fd56ca75096c77ce6abf848454b905cfc) )
	ROM_LOAD16_BYTE( "lt_d-l0j.rom", 0x000000, 0x020000, CRC(ae9a3f81) SHA1(c323073e2c245b3c52f93e07b98b0c2d4f4e97b1) )
	ROM_LOAD16_BYTE( "lt_d-h1.rom",  0x040001, 0x020000, CRC(d7dd3d48) SHA1(b848feee55159e334f711e4f661d415ffc1e3513) )
	ROM_LOAD16_BYTE( "lt_d-l1.rom",  0x040000, 0x020000, CRC(b94b3bd8) SHA1(7b89d9177d8b357b09317606cb2070c14c3449a5) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU */
	ROM_LOAD16_BYTE( "lt_d-sh0.rom",0x000001, 0x010000, CRC(af5b224f) SHA1(a07f2c6ca0e65af016d74b90342cfaab7535324e) )
	ROM_LOAD16_BYTE( "lt_d-sl0.rom",0x000000, 0x010000, CRC(cb3faac3) SHA1(e1ee32fac7ee9e97fbf68904572e90aa9d0c9460) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "lt_7a.rom", 0x000000, 0x040000, CRC(ada0fd50) SHA1(7eeb33360cacddf8887f3acce65350af0251936d) )
	ROM_LOAD( "lt_7b.rom", 0x040000, 0x040000, CRC(d2596883) SHA1(5a9f7384c63304c3c1e27375419d59a2b476f46a) )
	ROM_LOAD( "lt_7d.rom", 0x080000, 0x040000, CRC(2de637ef) SHA1(bd1be59d4fe9bf365454c1d471effd88aa942df6) )
	ROM_LOAD( "lt_7h.rom", 0x0c0000, 0x040000, CRC(9f6585cd) SHA1(5d59addc65c3ce20e7ea090a178fe9e17fba525b) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "lt_7j.rom", 0x000000, 0x040000, CRC(baf8863e) SHA1(fd5937cd70fcffd861a207dc4769d34459dd28d3) )
	ROM_LOAD( "lt_7l.rom", 0x040000, 0x040000, CRC(40fd50af) SHA1(d9bf1e339671fd167fad237bb5bc6d1b183686f5) )
	ROM_LOAD( "lt_7s.rom", 0x080000, 0x040000, CRC(c8e970df) SHA1(7771c17b36dcc9f01ac9e033f3f86e571c5ebbd3) )
	ROM_LOAD( "lt_7y.rom", 0x0c0000, 0x040000, CRC(f5436708) SHA1(e8cb278f4d310eeeb67e01534d17562c7fce62f0) )

	ROM_REGION( 0x40000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "lt_8a.rom" ,0x000000, 0x040000, CRC(357762a2) SHA1(d13b2a0f5d48c0171bcef708589cad194a7ea1ed) )
ROM_END

ROM_START( nbbatman )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "a1-h0-a.34",  0x000001, 0x040000, CRC(24a9b794) SHA1(a4867a89ea2749d60e6d1225bd84a488403b8cf3) )
	ROM_LOAD16_BYTE( "a1-l0-a.31",  0x000000, 0x040000, CRC(846d7716) SHA1(28434fd74b168ef73d00779b3e5d8b36b1f3ef80) )
	ROM_LOAD16_BYTE( "a1-h1-.33",   0x100001, 0x040000, CRC(3ce2aab5) SHA1(b39f17853bcab7ab290fdfaf9f3d8e8c2d91072a) )
	ROM_LOAD16_BYTE( "a1-l1-.32",   0x100000, 0x040000, CRC(116d9bcc) SHA1(c2faf8d1c6b51ac1483757777fd55961b74501fb) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU */
	ROM_LOAD16_BYTE( "a1-sh0-.14",0x000001, 0x010000, CRC(b7fae3e6) SHA1(ce41380d6c0f29f2facf9bf23dd4403648cd9eb4) )
	ROM_LOAD16_BYTE( "a1-sl0-.17",0x000000, 0x010000, CRC(b26d54fc) SHA1(136e1a83da08a0dc9046faf71f3f58d8d3095fde) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "lh534k0c.9",  0x000000, 0x080000, CRC(314a0c6d) SHA1(a918ae638f10b18165f7d34ab7db54fbf258df01) )
	ROM_LOAD( "lh534k0e.10", 0x080000, 0x080000, CRC(dc31675b) SHA1(81b0a6b35285e855c778c7f32f31115f1edce099) )
	ROM_LOAD( "lh534k0f.11", 0x100000, 0x080000, CRC(e15d8bfb) SHA1(74ea6f9748ed52e579cb08445282c871b3fd0f3a) )
	ROM_LOAD( "lh534k0g.12", 0x180000, 0x080000, CRC(888d71a3) SHA1(d1609e326fda5ac579ddf1ad5dc77443ec2a180f) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "lh538393.42", 0x000000, 0x100000, CRC(26cdd224) SHA1(ab2a3dd8eafec78866a0d45c1f051209025bdc77) )
	ROM_LOAD( "lh538394.43", 0x100000, 0x100000, CRC(4bbe94fa) SHA1(7c13b22e056dc1cf497ea0b3e9766579c33d4370) )
	ROM_LOAD( "lh538395.44", 0x200000, 0x100000, CRC(2a533b5e) SHA1(ceb9750b674adfa5fa0f88e46bce7b2b58440873) )
	ROM_LOAD( "lh538396.45", 0x300000, 0x100000, CRC(863a66fa) SHA1(0edc4734daee8fc1738df4f4f17bcd817f0ade0a) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "lh534k0k.8" ,0x000000, 0x080000, CRC(735e6380) SHA1(bf019815e579ef2393c00869f101a01f746e04d6) )
ROM_END

ROM_START( leaguemn )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "lma1-h0.34",  0x000001, 0x040000, CRC(47c54204) SHA1(59de4e9a75c88dba71aa1949e7ac2c4b9e98f413) )
	ROM_LOAD16_BYTE( "lma1-l0.31",  0x000000, 0x040000, CRC(1d062c82) SHA1(8d5969dc0264a05334196132bc2b5a3a59fb9e3a) )
	ROM_LOAD16_BYTE( "a1-h1-.33",   0x100001, 0x040000, CRC(3ce2aab5) SHA1(b39f17853bcab7ab290fdfaf9f3d8e8c2d91072a) )
	ROM_LOAD16_BYTE( "a1-l1-.32",   0x100000, 0x040000, CRC(116d9bcc) SHA1(c2faf8d1c6b51ac1483757777fd55961b74501fb) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU */
	ROM_LOAD16_BYTE( "a1-sh0-.14",0x000001, 0x010000, CRC(b7fae3e6) SHA1(ce41380d6c0f29f2facf9bf23dd4403648cd9eb4) )
	ROM_LOAD16_BYTE( "a1-sl0-.17",0x000000, 0x010000, CRC(b26d54fc) SHA1(136e1a83da08a0dc9046faf71f3f58d8d3095fde) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "lh534k0c.9",  0x000000, 0x080000, CRC(314a0c6d) SHA1(a918ae638f10b18165f7d34ab7db54fbf258df01) )
	ROM_LOAD( "lh534k0e.10", 0x080000, 0x080000, CRC(dc31675b) SHA1(81b0a6b35285e855c778c7f32f31115f1edce099) )
	ROM_LOAD( "lh534k0f.11", 0x100000, 0x080000, CRC(e15d8bfb) SHA1(74ea6f9748ed52e579cb08445282c871b3fd0f3a) )
	ROM_LOAD( "lh534k0g.12", 0x180000, 0x080000, CRC(888d71a3) SHA1(d1609e326fda5ac579ddf1ad5dc77443ec2a180f) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "lh538393.42", 0x000000, 0x100000, CRC(26cdd224) SHA1(ab2a3dd8eafec78866a0d45c1f051209025bdc77) )
	ROM_LOAD( "lh538394.43", 0x100000, 0x100000, CRC(4bbe94fa) SHA1(7c13b22e056dc1cf497ea0b3e9766579c33d4370) )
	ROM_LOAD( "lh538395.44", 0x200000, 0x100000, CRC(2a533b5e) SHA1(ceb9750b674adfa5fa0f88e46bce7b2b58440873) )
	ROM_LOAD( "lh538396.45", 0x300000, 0x100000, CRC(863a66fa) SHA1(0edc4734daee8fc1738df4f4f17bcd817f0ade0a) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "lh534k0k.8" ,0x000000, 0x080000, CRC(735e6380) SHA1(bf019815e579ef2393c00869f101a01f746e04d6) )
ROM_END

ROM_START( ssoldier )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "f3-h0-h.bin",  0x000001, 0x040000, CRC(b63fb9da) SHA1(429beb7ebc98815809fdd0ff69fcb4a14e1d8a14) )
	ROM_LOAD16_BYTE( "f3-l0-h.bin",  0x000000, 0x040000, CRC(419361a2) SHA1(42284a7afedefdb58a9b505e87effeee8bb5a9d8) )
	ROM_LOAD16_BYTE( "f3-h1-a.bin",  0x080001, 0x020000, CRC(e3d9f619) SHA1(7f450413d1fae7250d2fcbe0ff4ee13d52fa15e8) )
	ROM_LOAD16_BYTE( "f3-l1-a.bin",  0x080000, 0x020000, CRC(8cb5c396) SHA1(af130632b4ffb846cf355064391130d8c7ba73ad) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 ) /* 1MB for the audio CPU */
	ROM_LOAD16_BYTE( "f3_sh0.sh0",0x000001, 0x010000, CRC(90b55e5e) SHA1(cf77ccb68a10a29289bc42db348f480e21c3a558) )
	ROM_LOAD16_BYTE( "f3_sl0.sl0",0x000000, 0x010000, CRC(77c16d57) SHA1(68c7f026b718b700f1f9162f53cdc859b65944b9) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "f3_w50.c0",  0x000000, 0x040000, CRC(47e788ee) SHA1(79a6624c9a36f380057c4fbda511128d62f9161e) )
	ROM_LOAD( "f3_w51.c1",  0x080000, 0x040000, CRC(8e535e3f) SHA1(a51a5a660d13e95da559e7c1eaf23479eddd196f) )
	ROM_LOAD( "f3_w52.c2",  0x100000, 0x040000, CRC(a6eb2e56) SHA1(db45fd5ffefbe407247069c611a1d40849770297) )
	ROM_LOAD( "f3_w53.c3",  0x180000, 0x040000, CRC(2f992807) SHA1(bc0fe02b7ad31cb06ab0bf3f91de4ca5130893f1) )

	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE( "f3_w37.000", 0x000001, 0x100000, CRC(fd4cda03) SHA1(34bfabb5a0fdc96507d3c3c028a0b087c406a0d1) )
	ROM_LOAD16_BYTE( "f3_w38.001", 0x000000, 0x100000, CRC(755bab10) SHA1(8d3a584f5e34da24a162c1812ec5a3fea49778d7) )
	ROM_LOAD16_BYTE( "f3_w39.010", 0x200001, 0x100000, CRC(b21ced92) SHA1(0af44bddaef77f9427f7073dfc96e8a59d7a9ba5) )
	ROM_LOAD16_BYTE( "f3_w40.011", 0x200000, 0x100000, CRC(2e906889) SHA1(2aee05ce8f0074302090f1b1c58054c4a861ae68) )
	ROM_LOAD16_BYTE( "f3_w41.020", 0x400001, 0x100000, CRC(02455d10) SHA1(4f83d8349d39b220a2150a52d0202c7f8d2b588f) )
	ROM_LOAD16_BYTE( "f3_w42.021", 0x400000, 0x100000, CRC(124589b9) SHA1(dc8f95a0ff205fd24136738941a8931c16c380a4) )
	ROM_LOAD16_BYTE( "f3_w43.030", 0x600001, 0x100000, CRC(dae7327a) SHA1(3c742b57f30df3ee8d5f5b36dc890af1ec396df5) )
	ROM_LOAD16_BYTE( "f3_w44.031", 0x600000, 0x100000, CRC(d0fc84ac) SHA1(19154f81c4182be1fe835b5647fa30360c3507aa) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "f3_w95.da" ,0x000000, 0x080000, CRC(f7ca432b) SHA1(274458b68f906e6043bc36110a4903280647ac2d) )
ROM_END

ROM_START( psoldier )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "f3_h0d.h0",  0x000001, 0x040000, CRC(38f131fd) SHA1(0e513a5edfd8ab14440c360000a40b9d750cb54a) )
	ROM_LOAD16_BYTE( "f3_l0d.l0",  0x000000, 0x040000, CRC(1662969c) SHA1(8de1683076d7128ec16f1d053afb5e236add73e6) )
	ROM_LOAD16_BYTE( "f3_h1.h1",   0x080001, 0x040000, CRC(c8d1947c) SHA1(832a448f117224941799aeece2ec0b25065be3e2) )
	ROM_LOAD16_BYTE( "f3_l1.l1",   0x080000, 0x040000, CRC(7b9492fc) SHA1(335166d096dec3773ec69b05dad6763505818dd6) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )	/* 1MB for the audio CPU */
	ROM_LOAD16_BYTE( "f3_sh0.sh0",0x000001, 0x010000, CRC(90b55e5e) SHA1(cf77ccb68a10a29289bc42db348f480e21c3a558) )
	ROM_LOAD16_BYTE( "f3_sl0.sl0",0x000000, 0x010000, CRC(77c16d57) SHA1(68c7f026b718b700f1f9162f53cdc859b65944b9) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE ) /* Tiles */
	ROM_LOAD( "f3_w50.c0",  0x000000, 0x040000, CRC(47e788ee) SHA1(79a6624c9a36f380057c4fbda511128d62f9161e) )
	ROM_LOAD( "f3_w51.c1",  0x080000, 0x040000, CRC(8e535e3f) SHA1(a51a5a660d13e95da559e7c1eaf23479eddd196f) )
	ROM_LOAD( "f3_w52.c2",  0x100000, 0x040000, CRC(a6eb2e56) SHA1(db45fd5ffefbe407247069c611a1d40849770297) )
	ROM_LOAD( "f3_w53.c3",  0x180000, 0x040000, CRC(2f992807) SHA1(bc0fe02b7ad31cb06ab0bf3f91de4ca5130893f1) )

	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD16_BYTE( "f3_w37.000", 0x000001, 0x100000, CRC(fd4cda03) SHA1(34bfabb5a0fdc96507d3c3c028a0b087c406a0d1) )
	ROM_LOAD16_BYTE( "f3_w38.001", 0x000000, 0x100000, CRC(755bab10) SHA1(8d3a584f5e34da24a162c1812ec5a3fea49778d7) )
	ROM_LOAD16_BYTE( "f3_w39.010", 0x200001, 0x100000, CRC(b21ced92) SHA1(0af44bddaef77f9427f7073dfc96e8a59d7a9ba5) )
	ROM_LOAD16_BYTE( "f3_w40.011", 0x200000, 0x100000, CRC(2e906889) SHA1(2aee05ce8f0074302090f1b1c58054c4a861ae68) )
	ROM_LOAD16_BYTE( "f3_w41.020", 0x400001, 0x100000, CRC(02455d10) SHA1(4f83d8349d39b220a2150a52d0202c7f8d2b588f) )
	ROM_LOAD16_BYTE( "f3_w42.021", 0x400000, 0x100000, CRC(124589b9) SHA1(dc8f95a0ff205fd24136738941a8931c16c380a4) )
	ROM_LOAD16_BYTE( "f3_w43.030", 0x600001, 0x100000, CRC(dae7327a) SHA1(3c742b57f30df3ee8d5f5b36dc890af1ec396df5) )
	ROM_LOAD16_BYTE( "f3_w44.031", 0x600000, 0x100000, CRC(d0fc84ac) SHA1(19154f81c4182be1fe835b5647fa30360c3507aa) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_SOUNDONLY )
	ROM_LOAD( "f3_w95.da" ,0x000000, 0x080000, CRC(f7ca432b) SHA1(274458b68f906e6043bc36110a4903280647ac2d) )
ROM_END

ROM_START( dsccr94j )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE("a3_-h0-e.bin", 0x000001, 0x040000, CRC(8de1dbcd) SHA1(3726c7f8bc1e61a488ab7ef0b79a7a45054235c2) )
	ROM_LOAD16_BYTE("a3_-l0-e.bin", 0x000000, 0x040000, CRC(d3df8bfd) SHA1(b98064579491aef8eb8ccb94195412e79674a0c1) )
	ROM_LOAD16_BYTE("ds_h1-c.rom",  0x100001, 0x040000, CRC(6109041b) SHA1(063898a88f8a6a9f1510aa55e53a39f037b02903) )
	ROM_LOAD16_BYTE("ds_l1-c.rom",  0x100000, 0x040000, CRC(97a01f6b) SHA1(e188e28f880f5f3f4d7b49eca639d643989b1468) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("ds_sh0.rom",   0x000001, 0x010000, CRC(23fe6ffc) SHA1(896377961cafc19e44d9d889f9fbfdbaedd556da) )
	ROM_LOAD16_BYTE("ds_sl0.rom",   0x000000, 0x010000, CRC(768132e5) SHA1(1bb64516eb58d3b246f08e1c07f091e78085689f) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )	/* chars */
	ROM_LOAD("c0.bin",   0x000000, 0x100000, CRC(83ea8a47) SHA1(b29c8cc50da85c8168dda92446dfa12582580f96) )
	ROM_LOAD("c1.bin",   0x100000, 0x100000, CRC(64063e6d) SHA1(80b66e08292a3682f80d5670c5fe9f0fcc92062e) )
	ROM_LOAD("c2.bin",   0x200000, 0x100000, CRC(cc1f621a) SHA1(a0bdfe582206d49ca01bedc2b6973ebe5248efe4) )
	ROM_LOAD("c3.bin",   0x300000, 0x100000, CRC(515829e1) SHA1(2b5a5151eeb56cd3da30c8cb6415605cbe1d82e9) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD16_BYTE("a3-o00-w.bin",   0x000001, 0x80000, CRC(b094e5ad) SHA1(9acceb24a72eeb3c6e629c08d4cc9ef2a171da32) )
	ROM_LOAD16_BYTE("a3-o01-w.bin",   0x000000, 0x80000, CRC(91f34018) SHA1(4982b914ecce0358d63800caf7e249e1723bf7cf) )
	ROM_LOAD16_BYTE("a3-o10-w.bin",   0x100001, 0x80000, CRC(edddeef4) SHA1(73a90c20c99209206370e8bff35199c3a6b9dc3d) )
	ROM_LOAD16_BYTE("a3-o11-w.bin",   0x100000, 0x80000, CRC(274a9526) SHA1(2844079b2ec33ff2ccf6f73586ff426bdab9cf83) )
	ROM_LOAD16_BYTE("a3-o20-w.bin",   0x200001, 0x80000, CRC(32064393) SHA1(bacd4902079557141133920c44b16b52242692e7) )
	ROM_LOAD16_BYTE("a3-o21-w.bin",   0x200000, 0x80000, CRC(57bae3d9) SHA1(11face0e157ed42d7836bb8d60a4b75740de52d5) )
	ROM_LOAD16_BYTE("a3-o30-w.bin",   0x300001, 0x80000, CRC(be838e2f) SHA1(fc8a42b9183dfc60c317cbcef1da6798fed125ef) )
	ROM_LOAD16_BYTE("a3-o31-w.bin",   0x300000, 0x80000, CRC(bf899f0d) SHA1(f781454df089743186816ce98d863c94f7b208bd) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )
	ROM_LOAD("ds_da0.rom" ,  0x000000, 0x100000, CRC(67fc52fd) SHA1(5771e948115af8fe4a6d3f448c03a2a9b42b6f20) )
ROM_END

ROM_START( gunforc2 )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE("a2-h0-a.6h", 0x000001, 0x040000, CRC(49965e22) SHA1(077283c66a4cc2c47221c5f3267f440223615a15) )
	ROM_LOAD16_BYTE("a2-l0-a.8h", 0x000000, 0x040000, CRC(8c88b278) SHA1(0fd8e663619dcd8c81b3baa290bb0e72c185273a) )
	ROM_LOAD16_BYTE("a2-h1-a.6f", 0x100001, 0x040000, CRC(34280b88) SHA1(3fd3cdf8acfa845abacb0708fb48741ee44dbf13) )
	ROM_LOAD16_BYTE("a2-l1-a.8f", 0x100000, 0x040000, CRC(c8c13f51) SHA1(fde3fd983ebb920f79e6898aa0576da9dd9f0c15) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("a2_sho.3l",   0x000001, 0x010000, CRC(2e2d103d) SHA1(6b663948f69218308d9ecdb677557b2db1dfbf5a) )
	ROM_LOAD16_BYTE("a2_slo.5l",   0x000000, 0x010000, CRC(2287e0b3) SHA1(755dab510915161428ed57ab18410c393e138e65) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE )	/* chars */
	ROM_LOAD("a2_c0.1a",   0x000000, 0x080000, CRC(68b8f574) SHA1(fb935947cdde43e84453f82caeea141a4ae7226d) )
	ROM_LOAD("a2_c1.1b",   0x080000, 0x080000, CRC(0b9efe67) SHA1(1df4108d30d2538f6407e328513517cd3412321f) )
	ROM_LOAD("a2_c2.3a",   0x100000, 0x080000, CRC(7a9e9978) SHA1(241dc310e75960e306701a2e86e30d9c1a60ebff) )
	ROM_LOAD("a2_c3.3b",   0x180000, 0x080000, CRC(1395ee6d) SHA1(e9befc966e6ee046eaca185a9969976304a119d8) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "a2_000.8a", 0x000000, 0x100000, CRC(38e03147) SHA1(cc5bacad9592aa5e91632b139955e1c704a67a33) )
	ROM_LOAD( "a2_010.8b", 0x100000, 0x100000, CRC(1d5b05f8) SHA1(884f134ed51b432965a4e5e79915ba9c0ab562c6) )
	ROM_LOAD( "a2_020.8c", 0x200000, 0x100000, CRC(f2f461cc) SHA1(04e91efc749d022c8012caac493767ec1f6a992d) )
	ROM_LOAD( "a2_030.8d", 0x300000, 0x100000, CRC(97609d9d) SHA1(71ddff85a8ddeac69863bbf6c493c5c3973fd175) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )
	ROM_LOAD("a2_da.1l" ,  0x000000, 0x100000, CRC(3c8cdb6a) SHA1(d1f4186e8ddf99698443f8ee1c60a6e6bc367b09) )
ROM_END

ROM_START( geostorm )
	ROM_REGION( 0x180000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE("geo-h0.bin", 0x000001, 0x040000, CRC(9be58d09) SHA1(ab98b91abc8129c342c59674eab9683cccc6ca35) )
	ROM_LOAD16_BYTE("geo-l0.bin", 0x000000, 0x040000, CRC(59abb75d) SHA1(52b48685470ffa3f36a8259bf333448bf40caea9) )
	ROM_LOAD16_BYTE("a2-h1-a.6f", 0x100001, 0x040000, CRC(34280b88) SHA1(3fd3cdf8acfa845abacb0708fb48741ee44dbf13) )
	ROM_LOAD16_BYTE("a2-l1-a.8f", 0x100000, 0x040000, CRC(c8c13f51) SHA1(fde3fd983ebb920f79e6898aa0576da9dd9f0c15) )

	ROM_REGION( 0x100000 * 2, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE("a2_sho.3l",   0x000001, 0x010000, CRC(2e2d103d) SHA1(6b663948f69218308d9ecdb677557b2db1dfbf5a) )
	ROM_LOAD16_BYTE("a2_slo.5l",   0x000000, 0x010000, CRC(2287e0b3) SHA1(755dab510915161428ed57ab18410c393e138e65) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE )	/* chars */
	ROM_LOAD("a2_c0.1a",   0x000000, 0x080000, CRC(68b8f574) SHA1(fb935947cdde43e84453f82caeea141a4ae7226d) )
	ROM_LOAD("a2_c1.1b",   0x080000, 0x080000, CRC(0b9efe67) SHA1(1df4108d30d2538f6407e328513517cd3412321f) )
	ROM_LOAD("a2_c2.3a",   0x100000, 0x080000, CRC(7a9e9978) SHA1(241dc310e75960e306701a2e86e30d9c1a60ebff) )
	ROM_LOAD("a2_c3.3b",   0x180000, 0x080000, CRC(1395ee6d) SHA1(e9befc966e6ee046eaca185a9969976304a119d8) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE ) /* Sprites */
	ROM_LOAD( "a2_000.8a", 0x000000, 0x100000, CRC(38e03147) SHA1(cc5bacad9592aa5e91632b139955e1c704a67a33) )
	ROM_LOAD( "a2_010.8b", 0x100000, 0x100000, CRC(1d5b05f8) SHA1(884f134ed51b432965a4e5e79915ba9c0ab562c6) )
	ROM_LOAD( "a2_020.8c", 0x200000, 0x100000, CRC(f2f461cc) SHA1(04e91efc749d022c8012caac493767ec1f6a992d) )
	ROM_LOAD( "a2_030.8d", 0x300000, 0x100000, CRC(97609d9d) SHA1(71ddff85a8ddeac69863bbf6c493c5c3973fd175) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )
	ROM_LOAD("a2_da.1l" ,  0x000000, 0x100000, CRC(3c8cdb6a) SHA1(d1f4186e8ddf99698443f8ee1c60a6e6bc367b09) )
ROM_END

/***************************************************************************/

static READ_HANDLER( lethalth_cycle_r )
{
	if (activecpu_get_pc()==0x1f4 && m92_ram[0x1e]==2 && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x1e + offset];
}

static READ_HANDLER( hook_cycle_r )
{
	if (activecpu_get_pc()==0x55ba && m92_ram[0x12]==0 && m92_ram[0x13]==0 && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x12 + offset];
}

static READ_HANDLER( bmaster_cycle_r )
{
	int d=activecpu_geticount();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000) {
		if (activecpu_get_pc()==0x410 && m92_ram[0x6fde]==0 && m92_ram[0x6fdf]==0 && offset==0) {
			/* Adjust in-game counter, based on cycles left to run */
			int old;

			old=m92_ram[0x74aa]+(m92_ram[0x74ab]<<8);
			old=(old+d/25)&0xffff; /* 25 cycles per increment */
			m92_ram[0x74aa]=old&0xff;
			m92_ram[0x74ab]=old>>8;
			cpu_spinuntil_int();
		}
	}
	return m92_ram[0x6fde + offset];
}

static READ_HANDLER( psoldier_cycle_r )
{
	int a=m92_ram[0]+(m92_ram[1]<<8);
	int b=m92_ram[0x1aec]+(m92_ram[0x1aed]<<8);
	int c=m92_ram[0x1aea]+(m92_ram[0x1aeb]<<8);

	if (activecpu_get_pc()==0x2dae && b!=a && c!=a && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x1aec + offset];
}

static READ_HANDLER( ssoldier_cycle_r )
{
	int a=m92_ram[0]+(m92_ram[1]<<8);
	int b=m92_ram[0x1aec]+(m92_ram[0x1aed]<<8);
	int c=m92_ram[0x1aea]+(m92_ram[0x1aeb]<<8);

	if (activecpu_get_pc()==0x2e36 && b!=a && c!=a && offset==0)
		cpu_spinuntil_int();

	return m92_ram[0x1aec + offset];
}

static READ_HANDLER( psoldier_snd_cycle_r )
{
	int a=m92_snd_ram[0xc34];
/*logerror("%08x: %d %d\n",activecpu_get_pc(),a,offset);*/
	if (activecpu_get_pc()==0x8f0 && (a&0x80)!=0x80 && offset==0) {
		cpu_spinuntil_int();
	}

	return m92_snd_ram[0xc34 + offset];
}

static READ_HANDLER( inthunt_cycle_r )
{
	int d=activecpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if (activecpu_get_pc()==0x858 && m92_ram[0x25f]==0 && offset==1) {
			/* Adjust in-game counter, based on cycles left to run */
			int old;

			old=m92_ram[0xb892]+(m92_ram[0xb893]<<8);
			old=(old+d/82)&0xffff; /* 82 cycles per increment */
			m92_ram[0xb892]=old&0xff;
			m92_ram[0xb893]=old>>8;

			cpu_spinuntil_int();
		}
	}

	return m92_ram[0x25e + offset];
}

static READ_HANDLER( kaiteids_cycle_r ) /* by bkc */
{
	int d=activecpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if ((activecpu_get_pc()==0x885 || activecpu_get_pc()==0x8ac) 
			&& m92_ram[0x25f]==0 && offset==1) { /* 0x8ac , 0x885 */
			/* Adjust in-game counter, based on cycles left to run */
			int old;

			old=m92_ram[0xb898]+(m92_ram[0xb899]<<8);
			old=(old+d/82)&0xffff; /* 82 cycles per increment */
			m92_ram[0xb898]=old&0xff;
			m92_ram[0xb899]=old>>8;

			cpu_spinuntil_int();
		}
	}

	return m92_ram[0x25e + offset];
}

static READ_HANDLER( uccops_cycle_r )
{
	int a=m92_ram[0x3f28]+(m92_ram[0x3f29]<<8);
	int b=m92_ram[0x3a00]+(m92_ram[0x3a01]<<8);
	int c=m92_ram[0x3a02]+(m92_ram[0x3a03]<<8);
	int d=activecpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if ((activecpu_get_pc()==0x900ff || activecpu_get_pc()==0x90103) && b==c && offset==1) {
			cpu_spinuntil_int();
			/* Update internal counter based on cycles left to run */
			a=(a+d/127)&0xffff; /* 127 cycles per loop increment */
			m92_ram[0x3f28]=a&0xff;
			m92_ram[0x3f29]=a>>8;
		}
	}

	return m92_ram[0x3a02 + offset];
}

static READ_HANDLER( rtypeleo_cycle_r )
{
	if (activecpu_get_pc()==0x30791 && offset==0 && m92_ram[0x32]==2 && m92_ram[0x33]==0)
		cpu_spinuntil_int();

	return m92_ram[0x32 + offset];
}

static READ_HANDLER( rtypelej_cycle_r )
{
	if (activecpu_get_pc()==0x307a3 && offset==0 && m92_ram[0x32]==2 && m92_ram[0x33]==0)
		cpu_spinuntil_int();

	return m92_ram[0x32 + offset];
}

static READ_HANDLER( gunforce_cycle_r )
{
	int a=m92_ram[0x6542]+(m92_ram[0x6543]<<8);
	int b=m92_ram[0x61d0]+(m92_ram[0x61d1]<<8);
	int d=activecpu_geticount();
	int line = 256 - cpu_getiloops();

	/* If possible skip this cpu segment - idle loop */
	if (d>159 && d<0xf0000000 && line<247) {
		if (activecpu_get_pc()==0x40a && ((b&0x8000)==0) && offset==1) {
			cpu_spinuntil_int();
			/* Update internal counter based on cycles left to run */
			a=(a+d/80)&0xffff; /* 80 cycles per loop increment */
			m92_ram[0x6542]=a&0xff;
			m92_ram[0x6543]=a>>8;
		}
	}

	return m92_ram[0x61d0 + offset];
}

static READ_HANDLER( dsccr94j_cycle_r )
{
	int a=m92_ram[0x965a]+(m92_ram[0x965b]<<8);
	int d=activecpu_geticount();

	if (activecpu_get_pc()==0x988 && m92_ram[0x8636]==0 && offset==0) {
		cpu_spinuntil_int();

		/* Update internal counter based on cycles left to run */
		a=(a+d/56)&0xffff; /* 56 cycles per loop increment */
		m92_ram[0x965a]=a&0xff;
		m92_ram[0x965b]=a>>8;
	}

	return m92_ram[0x8636 + offset];
}

static READ_HANDLER( gunforc2_cycle_r )
{
	int a=m92_ram[0x9fa0]+(m92_ram[0x9fa1]<<8);
	int b=m92_ram[0x9fa2]+(m92_ram[0x9fa3]<<8);
	int c=m92_ram[0xa6aa]+(m92_ram[0xa6ab]<<8);
	int d=activecpu_geticount();

	if (activecpu_get_pc()==0x510 && a==b && offset==0) {
		cpu_spinuntil_int();

		/* Update internal counter based on cycles left to run */
		c=(c+d/62)&0xffff; /* 62 cycles per loop increment */
		m92_ram[0xa6aa]=a&0xff;
		m92_ram[0xa6ab]=a>>8;
	}

	return m92_ram[0x9fa0 + offset];
}

static READ_HANDLER( gunforc2_snd_cycle_r )
{
	int a=m92_snd_ram[0xc31];

	if (activecpu_get_pc()==0x8aa && a!=3 && offset==1) {
		cpu_spinuntil_int();
	}

	return m92_snd_ram[0xc30 + offset];
}

/***************************************************************************/

static void m92_startup(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	memcpy(RAM+0xffff0,RAM+0x7fff0,0x10); /* Start vector */
	bankaddress = 0xa0000; /* Initial bank */
	set_m92_bank();

	/* Mirror used by In The Hunt for protection */
	memcpy(RAM+0xc0000,RAM+0x00000,0x10000);
	cpu_setbank(2,&RAM[0xc0000]);

	RAM = memory_region(REGION_CPU2);
	memcpy(RAM+0xffff0,RAM+0x1fff0,0x10); /* Sound cpu Start vector */

	m92_game_kludge=0;
	m92_irq_vectorbase=0x80;
	m92_raster_enable=1;
	m92_sprite_buffer_busy=0x80;
}

static void init_m92(const unsigned char *decryption_table)
{
	m92_startup();
	setvector_callback(VECTOR_INIT);
	irem_cpu_decrypt(1,decryption_table);

	state_save_register_int  ("main", 0, "irqvector",    &irqvector);
	state_save_register_int  ("main", 0, "sound_status", &sound_status);
	state_save_register_int("main", 0, "bankaddress", &bankaddress);
	state_save_register_func_postload(set_m92_bank);
}

static DRIVER_INIT( bmaster )
{
	install_mem_read_handler(0, 0xe6fde, 0xe6fdf, bmaster_cycle_r);
	init_m92(bomberman_decryption_table);
}

static DRIVER_INIT( gunforce )
{
	install_mem_read_handler(0, 0xe61d0, 0xe61d1, gunforce_cycle_r);
	init_m92(gunforce_decryption_table);
}

static DRIVER_INIT( hook )
{
	install_mem_read_handler(0, 0xe0012, 0xe0013, hook_cycle_r);
	init_m92(hook_decryption_table);
}

static DRIVER_INIT( mysticri )
{
	init_m92(mysticri_decryption_table);
}

static DRIVER_INIT( uccops )
{
	install_mem_read_handler(0, 0xe3a02, 0xe3a03, uccops_cycle_r);
	init_m92(dynablaster_decryption_table);
}

static DRIVER_INIT( rtypeleo )
{
	install_mem_read_handler(0, 0xe0032, 0xe0033, rtypeleo_cycle_r);
	init_m92(rtypeleo_decryption_table);
	m92_irq_vectorbase=0x20;
	m92_game_kludge=3;
}

static DRIVER_INIT( rtypelej )
{
	install_mem_read_handler(0, 0xe0032, 0xe0033, rtypelej_cycle_r);
	init_m92(rtypeleo_decryption_table);
	m92_irq_vectorbase=0x20;
	m92_game_kludge=3;
}

static DRIVER_INIT( majtitl2 )
{
	init_m92(majtitl2_decryption_table);

	/* This game has an eprom on the game board */
	install_mem_read_handler(0, 0xf0000, 0xf3fff, m92_eeprom_r);
	install_mem_write_handler(0, 0xf0000, 0xf3fff, m92_eeprom_w);

	m92_game_kludge=2;
}

static DRIVER_INIT( inthunt )
{
	install_mem_read_handler(0, 0xe025e, 0xe025f, inthunt_cycle_r);
	init_m92(inthunt_decryption_table);
}

static DRIVER_INIT( kaiteids )
{
	install_mem_read_handler(0, 0xe025e, 0xe025f, kaiteids_cycle_r);
	init_m92(inthunt_decryption_table);
}

static DRIVER_INIT( lethalth )
{
	install_mem_read_handler(0, 0xe001e, 0xe001f, lethalth_cycle_r);
	init_m92(lethalth_decryption_table);
	m92_irq_vectorbase=0x20;

	/* This game sets the raster IRQ position, but the interrupt routine
		is just an iret, no need to emulate it */
	m92_raster_enable=0;
	m92_game_kludge=3; /* No upper palette bank? It could be a different motherboard */
}

static DRIVER_INIT( nbbatman )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	init_m92(leagueman_decryption_table);

	memcpy(RAM+0x80000,RAM+0x100000,0x20000);
}

static DRIVER_INIT( ssoldier )
{
 install_mem_read_handler(0, 0xe1aec, 0xe1aed, ssoldier_cycle_r);
 install_mem_read_handler(1, 0xa0c34, 0xa0c35, psoldier_snd_cycle_r);

 init_m92(psoldier_decryption_table);
 m92_irq_vectorbase=0x20;
 /* main CPU expects an answer even before writing the first command */
 sound_status = 0x80;
}

static DRIVER_INIT( psoldier )
{
	install_mem_read_handler(0, 0xe1aec, 0xe1aed, psoldier_cycle_r);
	install_mem_read_handler(1, 0xa0c34, 0xa0c35, psoldier_snd_cycle_r);

	init_m92(psoldier_decryption_table);
	m92_irq_vectorbase=0x20;
	/* main CPU expects an answer even before writing the first command */
	sound_status = 0x80;
}

static DRIVER_INIT( dsccr94j )
{
	install_mem_read_handler(0, 0xe8636, 0xe8637, dsccr94j_cycle_r);
	init_m92(dsoccr94_decryption_table);
}

static DRIVER_INIT( gunforc2 )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	init_m92(lethalth_decryption_table);
	memcpy(RAM+0x80000,RAM+0x100000,0x20000);

	install_mem_read_handler(0, 0xe9fa0, 0xe9fa1, gunforc2_cycle_r);
	install_mem_read_handler(1, 0xa0c30, 0xa0c31, gunforc2_snd_cycle_r);
}

/***************************************************************************/

/* The 'nonraster' machine is for games that don't use the raster interrupt feature - slightly faster to emulate */
GAME( 1991, gunforce, 0,        raster,    gunforce, gunforce, ROT0,   "Irem",         "Gunforce - Battle Fire Engulfed Terror Island (World)" )
GAME( 1991, gunforcj, gunforce, raster,    gunforce, gunforce, ROT0,   "Irem",         "Gunforce - Battle Fire Engulfed Terror Island (Japan)" )
GAME( 1991, gunforcu, gunforce, raster,    gunforce, gunforce, ROT0,   "Irem America", "Gunforce - Battle Fire Engulfed Terror Island (US)" )
GAME( 1991, bmaster,  0,        nonraster, bmaster,  bmaster,  ROT0,   "Irem",         "Blade Master (World)" )
GAME( 1991, lethalth, 0,        lethalth,  lethalth, lethalth, ROT270, "Irem",         "Lethal Thunder (World)" )
GAME( 1991, thndblst, lethalth, lethalth,  lethalth, lethalth, ROT270, "Irem",         "Thunder Blaster (Japan)" )
GAME( 1992, uccops,   0,        raster,    uccops,   uccops,   ROT0,   "Irem",         "Undercover Cops (World)" )
GAME( 1992, uccopsar, uccops,   raster,    uccops,   uccops,   ROT0,   "Irem",         "Undercover Cops (Alpha Renewal Version)" )
GAME( 1992, uccopsj,  uccops,   raster,    uccops,   uccops,   ROT0,   "Irem",         "Undercover Cops (Japan)" )
GAME( 1992, mysticri, 0,        nonraster, mysticri, mysticri, ROT0,   "Irem",         "Mystic Riders (World)" )
GAME( 1992, gunhohki, mysticri, nonraster, mysticri, mysticri, ROT0,   "Irem",         "Gun Hohki (Japan)" )
GAMEX(1992, majtitl2, 0,        raster,    majtitl2, majtitl2, ROT0,   "Irem",         "Major Title 2 (World)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1992, skingame, majtitl2, raster,    majtitl2, majtitl2, ROT0,   "Irem America", "The Irem Skins Game (US set 1)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1992, skingam2, majtitl2, raster,    majtitl2, majtitl2, ROT0,   "Irem America", "The Irem Skins Game (US set 2)", GAME_IMPERFECT_GRAPHICS )
GAME( 1992, hook,     0,        nonraster, hook,     hook,     ROT0,   "Irem",         "Hook (World)" )
GAME( 1992, hooku,    hook,     nonraster, hook,     hook,     ROT0,   "Irem America", "Hook (US)" )
GAME( 1992, hookj,    hook,     nonraster, hook,     hook,     ROT0,   "Irem", 	       "Hook (Japan)" )
GAME( 1992, rtypeleo, 0,        raster,    rtypeleo, rtypeleo, ROT0,   "Irem",         "R-Type Leo (World rev. C)" )
GAME( 1992, rtypelej, rtypeleo, raster,    rtypeleo, rtypelej, ROT0,   "Irem",         "R-Type Leo (Japan rev. D)" )
GAME( 1993, inthunt,  0,        raster,    inthunt,  inthunt,  ROT0,   "Irem",         "In The Hunt (World)" )
GAME( 1993, inthuntu, inthunt,  raster,    inthunt,  inthunt,  ROT0,   "Irem America", "In The Hunt (US)" )
GAME( 1993, kaiteids, inthunt,  raster,    inthunt,  kaiteids, ROT0,   "Irem",         "Kaitei Daisensou (Japan)" )
GAMEX(1993, nbbatman, 0,        raster,    nbbatman, nbbatman, ROT0,   "Irem America", "Ninja Baseball Batman (US)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1993, leaguemn, nbbatman, raster,    nbbatman, nbbatman, ROT0,   "Irem",         "Yakyuu Kakutou League-Man (Japan)", GAME_IMPERFECT_GRAPHICS )
GAME( 1993, ssoldier, 0,        psoldier,  psoldier, ssoldier, ROT0,   "Irem America", "Superior Soldiers (US)" )
GAME( 1993, psoldier, ssoldier, psoldier,  psoldier, psoldier, ROT0,   "Irem",         "Perfect Soldiers (Japan)" )
GAME( 1994, dsccr94j, dsoccr94, psoldier,  dsccr94j, dsccr94j, ROT0,   "Irem",         "Dream Soccer '94 (Japan)" )
GAME( 1994, gunforc2, 0,        raster,    gunforc2, gunforc2, ROT0,   "Irem",         "Gunforce 2 (US)" )
GAME( 1994, geostorm, gunforc2, raster,    gunforc2, gunforc2, ROT0,   "Irem",         "Geostorm (Japan)" )
