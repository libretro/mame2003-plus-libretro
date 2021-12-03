/***************************************************************************

	Beast Busters			A9003	(c) 1989 SNK Corporation
	Mechanized Attack			A8002	(c) 1989 SNK Corporation

	Beast Busters is a large dedicated (non-jamma) triple machine gun game,
	the gun positions values are read in an interrupt routine that must be
	called for each position (X and Y for 3 guns, so at least 6 times a
	frame).  However I can't make it work reliably..  So for the moment
	I'm writing the gun positions directly to memory and bypassing
	the IRQ routine.

	Mechanized Attack (A8002) is an earlier design, it only has one sprite
	chip, no eeprom, and only 2 machine guns, but the tilemaps are twice
	the size.

	Emulation by Bryan McPhail, mish@tendril.co.uk


Stephh's notes (based on the games M68000 code and some tests) :

1) 'bbusters'

  - Country/version is stored at 0x003954.w and the possible values are
    0x0000, 0x0004 and 0x0008 (0x000c being the same as 0x0008), 0x0008
    being the value stored in ROM in the current set.
    I haven't noticed any major effect (copyright/logo, language, coinage),
    the only thing I found is that the text relative to number of coins
    needed is different (but it's a lie as coinage is NOT modified).
    So my guess is that if other versions exist, part of the code (or at
    least data in it) will have to be different.
    Anyway, here is my guess to determine the versions (by using some infos
    from 'mechatt' :

       Value   Country
      0x0000    Japan
      0x0004    US?
      0x0008    World?   (value stored in the current set)
      0x000c    World?   (same as 0x0008 - impossible choice ?)

  - Coin buttons act differently depending on the "Coin Slots" Dip Switch :

      * "Coin Slots" Dip Switch set to "Common" :

          . COIN1    : adds coin(s)/credit(s) depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) depending on "Coin B" Dip Switch
          . COIN3    : NO EFFECT !
          . COIN4    : NO EFFECT !
          . COIN5    : NO EFFECT !
          . COIN6    : NO EFFECT !
          . SERVICE1 : adds coin(s)/credit(s) depending on "Coin A" Dip Switch

      * "Coin Slots" Dip Switch set to "Individual" :

          . COIN1    : adds coin(s)/credit(s) for player 1 depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) for player 1 depending on "Coin B" Dip Switch
          . COIN3    : adds coin(s)/credit(s) for player 2 depending on "Coin A" Dip Switch
          . COIN4    : adds coin(s)/credit(s) for player 2 depending on "Coin B" Dip Switch
          . COIN5    : adds coin(s)/credit(s) for player 3 depending on "Coin A" Dip Switch
          . COIN6    : adds coin(s)/credit(s) for player 3 depending on "Coin B" Dip Switch
          . SERVICE1 : adds coin(s)/credit(s) for all players depending on "Coin A" Dip Switch

    Note that I had to map COIN5 and COIN6 to SERVICE2 and SERVICE3 to be
    able to use the default parametrable keys. Let me know if there is a
    another (better ?) way to do so.


2) 'mechatt'

  - Country/version is stored at 0x06a000.w and the possible values are :

       Value   Country
      0x0000    Japan
      0x1111    World    (value stored in the current set)
      0x2222    US
      0x3333    Asia?    (it looks like Japanese text but some "symbols" are missing)

2a) Japan version

  - All texts are in Japanese.
  - "(c) 1989 (Corp) S-N-K".
  - "Coin Slots" Dip Switch has no effect.
  - "Coin A" and "Coin B" Dip Switches are the same as in the World version.
  - Coin buttons effect :

      * "Coin Slots" are ALWAYS considered as "Common" :

          . COIN1    : adds coin(s)/credit(s) depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) depending on "Coin B" Dip Switch
          . COIN3    : NO EFFECT !
          . COIN4    : NO EFFECT !
          . SERVICE1 : adds coin(s)/credit(s) depending on "Coin A" Dip Switch

2b) World version

  - All texts are in English.
  - "(c) 1989 SNK Corporation".
  - Coin buttons effect :

      * "Coin Slots" Dip Switch set to "Common" :

          . COIN1    : adds coin(s)/credit(s) depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) depending on "Coin B" Dip Switch
          . COIN3    : NO EFFECT !
          . COIN4    : NO EFFECT !
          . SERVICE1 : adds coin(s)/credit(s) depending on "Coin A" Dip Switch

      * "Coin Slots" Dip Switch set to "Individual" :

          . COIN1    : adds coin(s)/credit(s) for player 1 depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) for player 1 depending on "Coin B" Dip Switch
          . COIN3    : adds coin(s)/credit(s) for player 2 depending on "Coin A" Dip Switch
          . COIN4    : adds coin(s)/credit(s) for player 2 depending on "Coin B" Dip Switch
          . SERVICE1 : adds coin(s)/credit(s) for all players depending on "Coin A" Dip Switch

2c) US version

  - All texts are in English.
  - "(c) 1989 SNK Corp. of America".
  - Additional FBI logo as first screen as well as small FBI notice at the bottom left
    of the screens until a coin is inserted.
  - "Coin Slots" Dip Switch has no effect.
  - "Coin A" Dip Switch is different from the World version :

      World      US
      4C_1C    "Free Play"
      3C_1C    special (see below)
      2C_1C    "2 Coins to Start, 1 to Continue"
      1C_1C    1C_1C

    It's a bit hard to explain the "special" coinage, so here are some infos :

      * when you insert a coin before starting a game, you are awarded 2 credits
        if credits == 0, else you are awarded 1 credit
      * when you insert a coin to continue, you are ALWAYS awarded 1 credit

  - "Coin B" Dip Switch has no effect.

  - Coin buttons effect :

      * "Coin Slots" are ALWAYS considered as "Individual" :

          . COIN1    : adds coin(s)/credit(s) for player 1 depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) for player 2 depending on "Coin A" Dip Switch
          . COIN3    : NO EFFECT !
          . COIN4    : NO EFFECT !
          . SERVICE1 : adds coin(s)/credit(s) for all players depending on "Coin A" Dip Switch

2d) Asia? version

  - All texts are in Japanese ? (to be confirmed)
  - "(c) 1989 SNK Corporation".
  - "Coin Slots" Dip Switch has no effect.
  - "Coin A" and "Coin B" Dip Switches are the same as in the World version.
  - Coin buttons effect :

      * "Coin Slots" are ALWAYS considered as "Common" :

          . COIN1    : adds coin(s)/credit(s) depending on "Coin A" Dip Switch
          . COIN2    : adds coin(s)/credit(s) depending on "Coin B" Dip Switch
          . COIN3    : NO EFFECT !
          . COIN4    : NO EFFECT !
          . SERVICE1 : adds coin(s)/credit(s) depending on "Coin A" Dip Switch

          bbusters:

          If you only calibrate the P1 gun or do not hit the correct spots for all guns
          you will get either garbage or a black screen when rebooting.
          According to the manual this happens when the eprom contains invalid gun aim data.

          If you calibrate the guns correctly the game runs as expected:
          1) Using P1 controls fire at the indicated spots.
          2) Using P2 controls fire at the indicated spots.
          3) Using P3 controls fire at the indicated spots.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#define BBUSTERS_HACK	0
#define MECHATT_HACK	0

VIDEO_START( bbuster );
VIDEO_START( mechatt );
VIDEO_UPDATE( bbuster );
VIDEO_UPDATE( mechatt );

static data16_t *bbuster_ram, *eprom_data;
extern data16_t *bbuster_pf1_data,*bbuster_pf2_data,*bbuster_pf1_scroll_data,*bbuster_pf2_scroll_data;

WRITE16_HANDLER( bbuster_pf1_w );
WRITE16_HANDLER( bbuster_pf2_w );
WRITE16_HANDLER( bbuster_video_w );

/******************************************************************************/

#if BBUSTERS_HACK
static MACHINE_INIT( bbusters )
{
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU1);
	int data = readinputport(11) & 0x03;

	/* Country/Version :
	     - 0x0000 : Japan?
           - 0x0004 : US?
           - 0x0008 : World?	(default)
           - 0x000c : World?	(same as 0x0008)
	*/

	RAM[0x003954/2] = data * 4;

/*	memset (eprom_data, 0xff, 0x80);	*/ /* Force EEPROM reset*/
}
#endif

#if MECHATT_HACK
static MACHINE_INIT( mechatt )
{
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU1);
	int data = readinputport(6) & 0x03;

	/* Country :
	     - 0x0000 : Japan
           - 0x1111 : World	(default)
           - 0x2222 : US
           - 0x3333 : Asia?
	*/

	RAM[0x06a000/2] = (data << 12) | (data << 8) | (data << 4) | (data << 0);
}
#endif

/******************************************************************************/

static int sound_status;

static READ16_HANDLER( sound_status_r )
{
	return sound_status;
}

static WRITE_HANDLER( sound_status_w )
{
	sound_status = data;
}

static WRITE16_HANDLER( sound_cpu_w )
 {
	if (ACCESSING_LSB)
	{
		soundlatch_w(0,data&0xff);
	    cpu_set_irq_line(1,IRQ_LINE_NMI,PULSE_LINE);
	}
 }

/* Eprom is byte wide, top half of word _must_ be 0xff */
static READ16_HANDLER( eprom_r )
{
	return (eprom_data[offset]&0xff) | 0xff00;
}

static int gun_select;

static READ16_HANDLER( control_3_r )
{
	UINT16 retdata = readinputport(gun_select);
	retdata >>=1; /* by lowering the precision of the gun reading hardware the game seems to work better */
	return retdata;
}

static WRITE16_HANDLER( gun_select_w )
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "%08x: gun r\n",activecpu_get_pc());

	cpu_set_irq_line(0, 2, HOLD_LINE);

	gun_select=5 + (data&0xff);
}

static READ16_HANDLER( kludge_r )
{
	/* might latch the gun value? */
	return 0x0000;

}

static READ16_HANDLER( mechatt_gun_r )
{
	int baseport=2,x,y;
	if (offset) baseport=4; /* Player 2 */

	x=readinputport(baseport);
	y=readinputport(baseport+1);

	/* Todo - does the hardware really clamp like this? */
	x+=0x18;
	if (x>0xff) x=0xff;
	if (y>0xef) y=0xef;

	return x|(y<<8);
}

/*******************************************************************************/

static MEMORY_READ16_START( bbuster_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x080000, 0x08ffff, MRA16_RAM },
	{ 0x090000, 0x090fff, MRA16_RAM },
	{ 0x0a0000, 0x0a0fff, MRA16_RAM },
	{ 0x0a1000, 0x0a7fff, MRA16_RAM },	/* service mode */
	{ 0x0a8000, 0x0a8fff, MRA16_RAM },
	{ 0x0a9000, 0x0affff, MRA16_RAM },	/* service mode */
	{ 0x0b0000, 0x0b1fff, MRA16_RAM },
	{ 0x0b2000, 0x0b3fff, MRA16_RAM },
	{ 0x0b4000, 0x0b5fff, MRA16_RAM }, 	/* service mode */
	{ 0x0d0000, 0x0d0fff, MRA16_RAM },
	{ 0x0e0000, 0x0e0001, input_port_2_word_r }, /* Coins */
	{ 0x0e0002, 0x0e0003, input_port_0_word_r }, /* Player 1 & 2 */
	{ 0x0e0004, 0x0e0005, input_port_1_word_r }, /* Player 3 */
	{ 0x0e0008, 0x0e0009, input_port_3_word_r }, /* Dip 1 */
	{ 0x0e000a, 0x0e000b, input_port_4_word_r }, /* Dip 2 */
	{ 0x0e0018, 0x0e0019, sound_status_r },
	{ 0x0e8000, 0x0e8001, kludge_r },
	{ 0x0e8002, 0x0e8003, control_3_r },
	{ 0x0f8000, 0x0f80ff, eprom_r },  /* Eeprom */
MEMORY_END


static MEMORY_WRITE16_START( bbuster_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x080000, 0x08ffff, MWA16_RAM, &bbuster_ram },
	{ 0x090000, 0x090fff, bbuster_video_w, &videoram16 },
	{ 0x0a0000, 0x0a0fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x0a1000, 0x0a7fff, MWA16_RAM },	/* service mode */
	{ 0x0a8000, 0x0a8fff, MWA16_RAM, &spriteram16_2, &spriteram_2_size },
	{ 0x0a9000, 0x0affff, MWA16_RAM },	/* service mode */
	{ 0x0b0000, 0x0b1fff, bbuster_pf1_w, &bbuster_pf1_data },
	{ 0x0b2000, 0x0b3fff, bbuster_pf2_w, &bbuster_pf2_data },
	{ 0x0b4000, 0x0b5fff, MWA16_RAM },	/* service mode */
	{ 0x0b8000, 0x0b8003, MWA16_RAM, &bbuster_pf1_scroll_data },
	{ 0x0b8008, 0x0b800b, MWA16_RAM, &bbuster_pf2_scroll_data },
	{ 0x0d0000, 0x0d0fff, paletteram16_RRRRGGGGBBBBxxxx_word_w, &paletteram16 },
	{ 0x0e8000, 0x0e8001, gun_select_w },
	{ 0x0f0008, 0x0f0009, MWA16_NOP },
	{ 0x0f0018, 0x0f0019, sound_cpu_w },
	{ 0x0f8000, 0x0f80ff, MWA16_RAM, &eprom_data },  /* Eeprom */
MEMORY_END

/*******************************************************************************/

static MEMORY_READ16_START( mechatt_readmem )
	{ 0x000000, 0x06ffff, MRA16_ROM },
	{ 0x070000, 0x07ffff, MRA16_RAM },
	{ 0x090000, 0x090fff, MRA16_RAM },
	{ 0x0a0000, 0x0a0fff, MRA16_RAM },
	{ 0x0b0000, 0x0b3fff, MRA16_RAM },
	{ 0x0c0000, 0x0c3fff, MRA16_RAM },
	{ 0x0d0000, 0x0d07ff, MRA16_RAM },
	{ 0x0e0000, 0x0e0001, input_port_0_word_r },
	{ 0x0e0002, 0x0e0003, input_port_1_word_r },
	{ 0x0e0004, 0x0e0007, mechatt_gun_r },
	{ 0x0e8000, 0x0e8001, sound_status_r },
MEMORY_END

static MEMORY_WRITE16_START( mechatt_writemem )
	{ 0x000000, 0x06ffff, MWA16_ROM },
	{ 0x070000, 0x07ffff, MWA16_RAM, &bbuster_ram },
	{ 0x090000, 0x090fff, bbuster_video_w, &videoram16 },
	{ 0x0a0000, 0x0a0fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x0a1000, 0x0a7fff, MWA16_NOP },
	{ 0x0b0000, 0x0b3fff, bbuster_pf1_w, &bbuster_pf1_data },
	{ 0x0b8000, 0x0b8003, MWA16_RAM, &bbuster_pf1_scroll_data },
	{ 0x0c0000, 0x0c3fff, bbuster_pf2_w, &bbuster_pf2_data },
	{ 0x0c8000, 0x0c8003, MWA16_RAM, &bbuster_pf2_scroll_data },
	{ 0x0d0000, 0x0d07ff, paletteram16_RRRRGGGGBBBBxxxx_word_w, &paletteram16 },
	{ 0x0e4002, 0x0e4003, MWA16_NOP },  /* Gun force feedback? */
	{ 0x0e8000, 0x0e8001, sound_cpu_w },
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
	{ 0xf800, 0xf800, sound_status_w },
MEMORY_END

static PORT_READ_START( sound_readport )
	{ 0x00, 0x00, YM2610_status_port_0_A_r },
	{ 0x02, 0x02, YM2610_status_port_0_B_r },
PORT_END

static PORT_WRITE_START( sound_writeport )
	{ 0x00, 0x00, YM2610_control_port_0_A_w },
	{ 0x01, 0x01, YM2610_data_port_0_A_w },
	{ 0x02, 0x02, YM2610_control_port_0_B_w },
	{ 0x03, 0x03, YM2610_data_port_0_B_w },
	{ 0xc0, 0xc1, MWA_NOP }, /* -> Main CPU */
PORT_END

static PORT_READ_START( sounda_readport )
	{ 0x00, 0x00, YM2608_status_port_0_A_r },
	{ 0x02, 0x02, YM2608_status_port_0_B_r },
PORT_END

static PORT_WRITE_START( sounda_writeport )
	{ 0x00, 0x00, YM2608_control_port_0_A_w },
	{ 0x01, 0x01, YM2608_data_port_0_A_w },
	{ 0x02, 0x02, YM2608_control_port_0_B_w },
	{ 0x03, 0x03, YM2608_data_port_0_B_w },
	{ 0xc0, 0xc1, MWA_NOP }, /* -> Main CPU */
PORT_END

/******************************************************************************/

INPUT_PORTS_START( bbusters )
	PORT_START	/* Player controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )		/* "Fire"*/
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )		/* "Grenade"*/
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )		/* "Fire"*/
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )		/* "Grenade"*/
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* Player controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )		/* "Fire"*/
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )		/* "Grenade"*/
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN5 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN6 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )		// See notes
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x06, 0x06, "Magazine / Grenade" )
	PORT_DIPSETTING(    0x04, "5 / 2" )
	PORT_DIPSETTING(    0x06, "7 / 3" )
	PORT_DIPSETTING(    0x02, "9 / 4" )
	PORT_DIPSETTING(    0x00, "12 / 5" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x80, 0x80, "Coin Slots" )			/* See notes*/
	PORT_DIPSETTING(    0x80, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Game Mode" )
	PORT_DIPSETTING(    0x08, "Demo Sounds Off" )
	PORT_DIPSETTING(    0x0c, "Demo Sounds On" )
	PORT_DIPSETTING(    0x04, "Infinite Energy (Cheat)")
	PORT_DIPSETTING(    0x00, "Freeze" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
 	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 25, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 25, 10, 0, 255 )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 25, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 25, 10, 0, 255 )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER3, 25, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER3, 25, 10, 0, 255 )

#if BBUSTERS_HACK
	PORT_START
	PORT_DIPNAME( 0x03, 0x02, "Country/Version" )
	PORT_DIPSETTING(    0x00, "Japan?" )
	PORT_DIPSETTING(    0x01, "US?" )
	PORT_DIPSETTING(    0x02, "World?" )
/*	PORT_DIPSETTING(    0x03, "World?" )			*/ /* Same as "0008" - impossible choice ?*/
#endif
INPUT_PORTS_END

INPUT_PORTS_START( mechatt )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )		/* See notes*/
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )	/* "Fire"*/
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )	/* "Grenade"*/
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* "Fire"*/
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )	/* "Grenade"*/
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x0001, 0x0001, "Coin Slots" )		/* See notes*/
	PORT_DIPSETTING(      0x0001, "Common" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x0002, 0x0002, "Allow Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( No ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x000c, 0x000c, "Magazine / Grenade" )
	PORT_DIPSETTING(      0x0008, "5 / 2" )
	PORT_DIPSETTING(      0x000c, "6 / 3" )
	PORT_DIPSETTING(      0x0004, "7 / 4" )
	PORT_DIPSETTING(      0x0000, "8 / 5" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )	/* See notes*/
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )	/* See notes*/
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0300, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Game Mode" )
	PORT_DIPSETTING(      0x0800, "Demo Sounds Off" )
	PORT_DIPSETTING(      0x0c00, "Demo Sounds On" )
	PORT_DIPSETTING(      0x0400, "Infinite Energy (Cheat)")
	PORT_DIPSETTING(      0x0000, "Freeze" )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
 	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 25, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 25, 10, 0, 255 )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 25, 10, 0, 255 )
	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 25, 10, 0, 255 )

#if MECHATT_HACK
	PORT_START
	PORT_DIPNAME( 0x03, 0x01, "Country" )
	PORT_DIPSETTING(    0x00, "Japan" )
	PORT_DIPSETTING(    0x01, "World" )
	PORT_DIPSETTING(    0x02, "US" )
	PORT_DIPSETTING(    0x03, "Asia?" )
#endif
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 0,4,8,12,16,20,24,28 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 8, 12, 0, 4 },
	{
		0, 1, 2, 3,
		16,17,18,19,

		0+32*8, 1+32*8, 2+32*8, 3+32*8,
		16+32*8,17+32*8,18+32*8,19+32*8,
	},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8
};

static struct GfxLayout tilelayout =
{
	16,16,	/* 16*16 sprites */
	RGN_FRAC(1,1),
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0, 4, 8, 12, 16, 20, 24, 28,
		0+64*8, 4+64*8, 8+64*8, 12+64*8,
		16+64*8,20+64*8,24+64*8,28+64*8,
	},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
		8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	128*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 256, 16 },
	{ REGION_GFX3, 0, &spritelayout, 512, 16 },
	{ REGION_GFX4, 0, &tilelayout,   768, 16 },
	{ REGION_GFX5, 0, &tilelayout,  1024+256, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_mechatt[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 256, 16 },
	{ REGION_GFX3, 0, &spritelayout, 512, 16 },
	{ REGION_GFX4, 0, &tilelayout,   512, 16 },
	{ REGION_GFX5, 0, &tilelayout,   768, 16 },
	{ -1 } /* end of array */
};

/******************************************************************************/

static void sound_irq( int irq )
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2608interface ym2608_interface =
{
	1,
	8000000,	/* 8 MHz */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ sound_irq },
	{ REGION_SOUND1 },
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) }
};

struct YM2610interface ym2610_interface =
{
	1,
	8000000,
	{ MIXERG(100,MIXER_GAIN_4x,MIXER_PAN_CENTER) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ sound_irq },
	{ REGION_SOUND2 },
	{ REGION_SOUND1 },
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) }
};

/******************************************************************************/

/* default eeprom with reasonable calibration for MAME */
unsigned char bbusters_default_eeprom[128] = {
	                                    /*y*/                   /*y*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0xEE, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0xFE, 0x00,
	                                    /*y*/                   /*y*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00,	0x00, 0x00, 0xEE, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0xFE, 0x00,
	                                    /*y*/                   /*y*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00,	0x00, 0x00, 0xEE, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0xFE, 0x00,


	0x42, 0x00, 0x45, 0x00, 0x41, 0x00, 0x53, 0x00,	0x54, 0x00, 0x20, 0x00,
	0x42, 0x00, 0x55, 0x00, 0x53, 0x00, 0x54, 0x00, 0x45, 0x00, 0x52, 0x00,
	0x53, 0x00, 0x20, 0x00, 0x54, 0x00, 0x4D, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};



static NVRAM_HANDLER( bbusters )
{
	if( read_or_write ) {
		mame_fwrite (file, eprom_data, 0x80);
	}
	else {
		if (file)
			mame_fread (file, eprom_data, 0x80);
		else
			memcpy(eprom_data, bbusters_default_eeprom, 0x80);
	}
}


static VIDEO_EOF( bbuster )
{
	buffer_spriteram16_w(0,0,0);
	buffer_spriteram16_2_w(0,0,0);
}

static VIDEO_EOF( mechatt )
{
	buffer_spriteram16_w(0,0,0);
}

static MACHINE_DRIVER_START( bbusters )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)
	MDRV_CPU_MEMORY(bbuster_readmem,bbuster_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold,1)

	MDRV_CPU_ADD(Z80,4000000) /* Accurate */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)

#if BBUSTERS_HACK
	MDRV_MACHINE_INIT(bbusters)
#endif

	MDRV_NVRAM_HANDLER(bbusters)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(bbuster)
	MDRV_VIDEO_UPDATE(bbuster)
	MDRV_VIDEO_EOF(bbuster)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( mechatt )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)
	MDRV_CPU_MEMORY(mechatt_readmem,mechatt_writemem)
	MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_CPU_ADD(Z80,4000000) /* Accurate */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sounda_readport,sounda_writeport)

	MDRV_FRAMES_PER_SECOND(60)

#if MECHATT_HACK
	MDRV_MACHINE_INIT(mechatt)
#endif

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo_mechatt)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(mechatt)
	MDRV_VIDEO_UPDATE(mechatt)
	MDRV_VIDEO_EOF(mechatt)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2608, ym2608_interface)
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
MACHINE_DRIVER_END

/******************************************************************************/

ROM_START( bbusters )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bb-3.k10",   0x000000, 0x20000, CRC(04da1820) SHA1(0b6e06adf9c181d7aef28f781efbdd2c225fe81e) )
	ROM_LOAD16_BYTE( "bb-5.k12",   0x000001, 0x20000, CRC(777e0611) SHA1(b7ac0c6ea3738d560a5be75aed286821de918808) )
	ROM_LOAD16_BYTE( "bb-2.k8",    0x040000, 0x20000, CRC(20141805) SHA1(0958579681bda81bcf48d020a14bc147c1e575f1) )
	ROM_LOAD16_BYTE( "bb-4.k11",   0x040001, 0x20000, CRC(d482e0e9) SHA1(e56ca92965e8954b613ba4b0e3975e3a12840c30) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "bb-1.e6",     0x000000, 0x10000, CRC(4360f2ee) SHA1(4c6b212f59389bdf4388893d2030493b110ac087) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "bb-10.l9",    0x000000, 0x20000, CRC(490c0d9b) SHA1(567c25a6d96407259c64061d674305e4117d9fa4) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "bb-f11.m16",  0x000000, 0x80000, CRC(39fdf9c0) SHA1(80392947e3a1831c3ee80139f6f3bdc3bafa4f0d) )
	ROM_LOAD( "bb-f12.m13",  0x080000, 0x80000, CRC(69ee046b) SHA1(5c0435f1ce76b584fa8d154d7617d73c7ab5f62f) )
	ROM_LOAD( "bb-f13.m12",  0x100000, 0x80000, CRC(f5ef840e) SHA1(dd0f630c52076e0d330f47931e68a3ae9a401078) )
	ROM_LOAD( "bb-f14.m11",  0x180000, 0x80000, CRC(1a7df3bb) SHA1(1f27a528e6f89fe56a7342c4f1ff733da0a09327) )

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "bb-f21.l10",  0x000000, 0x80000, CRC(530f595b) SHA1(820898693b878c4423de9c244f943d39ea69515e) )
	ROM_LOAD( "bb-f22.l12",  0x080000, 0x80000, CRC(889c562e) SHA1(d19172d6515ab9793c98de75d6e41687e61a408d) )
	ROM_LOAD( "bb-f23.l13",  0x100000, 0x80000, CRC(c89fe0da) SHA1(92be860a7191e7473c42aa2da981eda873219d3d) )
	ROM_LOAD( "bb-f24.l15",  0x180000, 0x80000, CRC(e0d81359) SHA1(2213c17651b6c023a456447f352b0739439f913a) )

	ROM_REGION( 0x80000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "bb-back1.m4", 0x000000, 0x80000, CRC(b5445313) SHA1(3c99b557b2af30ff0fbc8a7dc6c40448c4f327db) )

	ROM_REGION( 0x80000, REGION_GFX5, ROMREGION_DISPOSE )
	ROM_LOAD( "bb-back2.m6", 0x000000, 0x80000, CRC(8be996f6) SHA1(1e2c56f4c24793f806d7b366b92edc03145ae94c) )

	ROM_REGION( 0x10000, REGION_USER1, 0 ) /* Zoom table */
	ROM_LOAD( "bb-6.e7",       0x000000, 0x10000, CRC(61f3de03) SHA1(736f9634fe054ea68a2aa90a743bd0dc320f23c9) )
	/* This rom also as bb-7.h7 */
	/* This rom also as bb-8.a14 */
	/* This rom also as bb-9.c14 */

	ROM_REGION( 0x80000, REGION_SOUND1, 0 )
	ROM_LOAD( "bb-pcma.l5",  0x000000, 0x80000, CRC(44cd5bfe) SHA1(26a612191a0aa614c090203485aba17c99c763ee) )

	ROM_REGION( 0x80000, REGION_SOUND2, 0 )
	ROM_LOAD( "bb-pcmb.l3",  0x000000, 0x80000, CRC(c8d5dd53) SHA1(0f7e94532cc14852ca12c1b792e5479667af899e) )
ROM_END

ROM_START( mechatt )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ma5-e.bin", 0x000000, 0x20000, CRC(9bbb852a) SHA1(34b696bf79cf53cac1c384a3143c0f3f243a71f3) )
	ROM_LOAD16_BYTE( "ma4.bin",   0x000001, 0x20000, CRC(0d414918) SHA1(0d51b893d37ba124b983beebb691e65bdc52d300) )
	ROM_LOAD16_BYTE( "ma7.bin",   0x040000, 0x20000, CRC(61d85e1b) SHA1(46234d48ac21c481a5e70c6a654a341ebdd4cd3a) )
	ROM_LOAD16_BYTE( "ma6-f.bin", 0x040001, 0x20000, CRC(4055fe8d) SHA1(b4d8bd5f73805ce1c332eff657dddbb88ff45b37) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "ma3.bin",       0x000000, 0x10000, CRC(c06cc8e1) SHA1(65f5f1901120d633f7c3ba07432a188fd7fd7272) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ma1.bin",       0x000000, 0x10000, CRC(24766917) SHA1(9082a8ae849605ce65b5a0493ae69cfe282f7e7b) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "mao89p13.bin",  0x000000, 0x80000, CRC(8bcb16cf) SHA1(409ee1944188d9ce39adce29b1df029b560dd5b0) )
	ROM_LOAD( "ma189p15.bin",  0x080000, 0x80000, CRC(b84d9658) SHA1(448adecb0067d8f5b219ec2f94a8dec84187a554) )
	ROM_LOAD( "ma289p17.bin",  0x100000, 0x80000, CRC(6cbe08ac) SHA1(8f81f6e92b84ab6867452011d52f3e7689c62a1a) )
	ROM_LOAD( "ma389m15.bin",  0x180000, 0x80000, CRC(34d4585e) SHA1(38d9fd5d775e4b3c8b8b487a6ba9b8bdcb3274b0) )

	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_DISPOSE )
	/* Unused */

	ROM_REGION( 0x80000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "mab189a2.bin",  0x000000, 0x80000, CRC(e1c8b4d0) SHA1(2f8a1839cca892f8380c7cffe7a12e615d38fd55) )

	ROM_REGION( 0x80000, REGION_GFX5, ROMREGION_DISPOSE )
	ROM_LOAD( "mab289c2.bin",  0x000000, 0x80000, CRC(14f97ceb) SHA1(a22033532ea616dc3a3db8b66ad6ccc6172ed7cc) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )
	ROM_LOAD( "ma2.bin",       0x000000, 0x20000, CRC(ea4cc30d) SHA1(d8f089fc0ce76309411706a8110ad907f93dc97e) )

	ROM_REGION( 0x20000, REGION_USER1, 0 ) /* Zoom table */
	ROM_LOAD( "ma8.bin",       0x000000, 0x10000, CRC(61f3de03) SHA1(736f9634fe054ea68a2aa90a743bd0dc320f23c9) )
	/* ma9 is identical to ma8 */
ROM_END

#if 0
static void bbusters_patch_code(UINT16 offset)
{
	/* To avoid checksum error */
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU1);
	RAM[(offset +  0)/2] = 0x4e71;
	RAM[(offset +  2)/2] = 0x4e71;
	RAM[(offset + 10)/2] = 0x4e71;
	RAM[(offset + 12)/2] = 0x4e71;
	RAM[(offset + 20)/2] = 0x4e71;
	RAM[(offset + 22)/2] = 0x4e71;
	RAM[(offset + 30)/2] = 0x4e71;
	RAM[(offset + 32)/2] = 0x4e71;
}
#endif

DRIVER_INIT( bbusters )
{
	#if BBUSTERS_HACK
	bbusters_patch_code(0x00234c);
	#endif
}

DRIVER_INIT( mechatt )
{
	#if MECHATT_HACK
	bbusters_patch_code(0x003306);
	#endif
}

/******************************************************************************/

#if !MECHATT_HACK
GAME( 1989, bbusters, 0, bbusters, bbusters, bbusters, ROT0,  "SNK", "Beast Busters (World [Q])" )
#else
GAME( 1989, bbusters, 0, bbusters, bbusters, bbusters, ROT0,  "SNK", "Beast Busters" )
#endif
#if !MECHATT_HACK
GAME( 1989, mechatt,  0, mechatt,  mechatt,  mechatt,  ROT0,  "SNK", "Mechanized Attack (World)" )
#else
GAME( 1989, mechatt,  0, mechatt,  mechatt,  mechatt,  ROT0,  "SNK", "Mechanized Attack" )
#endif
