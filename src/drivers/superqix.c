/***************************************************************************

Prebillian        (c) 1986 Taito
Hot Smash         (c) 1987 Taito
Super Qix         (c) 1987 Taito
Perestroika Girls (c) 1994 Promat (hack of Super Qix)

driver by Mirko Buffoni, Nicola Salmoria, Tomasz Slanina

Super Qix is a later revision of the hardware, featuring a bitmap layer that
is not present in the earlier games. It also has two 8910, while the earlier
games have one 8910 + a sample player.

Notes:
- The 8751 ROM, sq07.108, was bad: bit 3 was stuck high. I have recovered it
  by carefully checking the disassembly. There are probably some mistakes.
  Note that that ROM probably came from a bootleg in the first place.

- The MCU sends some ID to the Z80 on startup, but the Z80 happily ignores it.
  This happens in all sets. There appears to be code that would check part of
  the MCU init sequence ($5973 onwards), but it doesn't seem to be called.

- sqixa might be an earlier version because there is a bug with coin lockout:
  it is activated after inesrting 10 coins instead of 9. sqix doesn't have
  that bug and also inverts the coin lockout output.

- Note that the sqixa ROMs probably came from a bootleg board, where the 8751
  MCU was replaced by a model using external ROM (which was bad, and manually
  repaired). Even if it was a bootleg, at this point there is no reason to
  believe the ROMs had been tampered with.

- sqixbl is a bootleg of sqixa, with the MCU removed.

- Prebillian controls: (from the Japanese flyer):
  - pullout plunger for shot power (there's no on-screen power indicator in the game)
  - dial for aiming
  - button (fire M powerup, high score initials)
  They are mapped a bit differently in MAME. BUTTON1 simlates pulling out the
  plunger and releasing it. The plunger strength is controlled by an analog input
  (by default mapped to up/down arrows) and shown on screen. The dial is mapped
  as expected. The button is mapped on BUTTON2. BUTTON3 is also recognized by the
  game when entering initials, but was probably not present in the cabinet.


TODO:
- The way we generate NMI in sqix doesn't make much sense, but is a workaround
  for the slow gameplay you would otherwise get. Some interaction with vblank?

- I'm not sure about the NMI ack at 0418 in the original sqix, but the game hangs
  at the end of a game without it. Note that the bootleg replaces that call with
  something else.


Prebillian :
------------

PCB Layout (Prebillian, from The Guru ( http://unemulated.emuunlim.com/ )

 M6100211A
 -------------------------------------------------------------------
 |                    HM50464                                       |
 |  6                 HM50464                                       |
 |  5                 HM50464                               6116    |
 |  4                 HM50464                                       |
 |                                                                  |
 |                                                                  |
 |                                                               J  |
 |                                            68705P5 SW1(8)        |
 |               6264                                            A  |
 |                                              3     SW2(8)        |
 |                                                               M  |
 |                                                                  |
 |                                                               M  |
 |                                                                  |
 |                                   2                           A  |
 |                                                                  |
 |                                   1                              |
 |                                                                  |
 |                                   Z80B            AY-3-8910      |
 | 12MHz                                                            |
 --------------------------------------------------------------------

Notes:
       Vertical Sync: 60Hz
         Horiz. Sync: 15.67kHz
         Z80B Clock : 5.995MHz
     AY-3-8910 Clock: 1.499MHz



Hot (Vs) Smash :
----------------

Dips (not verified):

DSW1 stored @ $f236
76------ coin a
--54---- coin b
----3--- stored @ $f295 , tested @ $2a3b
------1- code @ $03ed, stored @ $f253 (flip screen)

DSW2 stored @ $f237
---4---- code @ $03b4, stored @ $f290
----32-- code @ $03d8, stored @ $f293 (3600/5400/2400/1200  -> bonus  ?)
------10 code @ $03be, stored @ $f291/92 (8,8/0,12/16,6/24,4 -> difficulty ? )

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern data8_t *superqix_videoram;
extern data8_t *superqix_bitmapram,*superqix_bitmapram2;
extern int pbillian_show_power;


WRITE_HANDLER( superqix_videoram_w );
WRITE_HANDLER( superqix_bitmapram_w );
WRITE_HANDLER( superqix_bitmapram2_w );
READ_HANDLER( superqix_bitmapram_r );
READ_HANDLER( superqix_bitmapram2_r );


WRITE_HANDLER( pbillian_0410_w );
WRITE_HANDLER( superqix_0410_w );

VIDEO_START( pbillian );
VIDEO_UPDATE( pbillian );
VIDEO_START( superqix );
VIDEO_UPDATE( superqix );


/* pbillian sample playback */
static int channel;
static INT8 *samplebuf;

int pbillian_sh_start(const struct MachineSound *msound)
{
	int i;

	channel = mixer_allocate_channel(50);
	mixer_set_name(channel,"Samples");

	/* convert 8-bit unsigned samples to 8-bit signed */
	samplebuf = memory_region(REGION_SOUND1);
	for (i = 0;i < memory_region_length(REGION_SOUND1);i++)
		samplebuf[i] ^= 0x80;

	return 0;
}

static WRITE_HANDLER( pbillian_sample_trigger_w )
{
	int start,end;

	start = data << 7;
	/* look for end of sample marker */
	end = start;
	while (end < 0x8000 && samplebuf[end] != (0xff^0x80))
		end++;

	mixer_play_sample(channel, samplebuf + start, end - start, 5000, 0); /* 5khz ? */
}


static data8_t port1, port3, port3_latch, from_mcu, from_z80, portb;
static int from_mcu_pending, from_z80_pending, invert_coin_lockout;


static READ_HANDLER( sqix_in0_r )
{
	return BITSWAP8(readinputport(0), 0,1,2,3,4,5,6,7);
}

WRITE_HANDLER( sqix_flipscreen_w )
{
	flip_screen_set(~data & 1);
}


/***************************************************************************

 Hot Smash 68705 protection interface

***************************************************************************/

/*
 * This wrapper routine is necessary because the dial is not connected to an
 * hardware counter as usual, but the DIR and CLOCK inputs are directly
 * connected to the 68705 which acts as a counter.
 */

int read_dial(int player)
{
	int newpos;
	static int oldpos[2];
	static int sign[2];

	/* get the new position and adjust the result */
	newpos = readinputport(3 + player);
	if (newpos != oldpos[player])
	{
		sign[player] = ((newpos - oldpos[player]) & 0x80) >> 7;
		oldpos[player] = newpos;
	}

	if (player == 0)
		return ((oldpos[player] & 1) << 2) | (sign[player] << 3);
	else	/* player == 1 */
		return ((oldpos[player] & 1) << 3) | (sign[player] << 2);
}



static void delayed_z80_mcu_w(int data)
{
log_cb(RETRO_LOG_DEBUG, LOGPRE "Z80 sends command %02x\n",data);
	from_z80 = data;
	from_mcu_pending = 0;
	cpu_set_irq_line(1, 0, HOLD_LINE);
	cpu_boost_interleave(0, TIME_IN_USEC(200));
}

static void delayed_mcu_z80_w(int data)
{
log_cb(RETRO_LOG_DEBUG, LOGPRE "68705 sends answer %02x\n",data);
	from_mcu = data;
	from_mcu_pending = 1;
}


/*
 *  Port C connections:
 *  (all lines active low)
 *
 *  0-2 W  select I/O; inputs are read from port A, outputs are written to port B
 *         000  dsw A (I)
 *         001  dsw B (I)
 *         010  not used
 *         011  from Z80 (I)
 *         100  not used
 *         101  to Z80 (O)
 *         110  P1 dial input (I)
 *         111  P2 dial input (I)
 *  3   W  clocks the active latch
 *  4-7 W  not used
 */

static UINT8 portA_in, portB_out, portC;

READ_HANDLER( hotsmash_68705_portA_r )
{
/* log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: 68705 reads port A = %02x\n",activecpu_get_pc(),portA_in); */
	return portA_in;
}

WRITE_HANDLER( hotsmash_68705_portB_w )
{
	portB_out = data;
}

READ_HANDLER( hotsmash_68705_portC_r )
{
	return portC;
}

WRITE_HANDLER( hotsmash_68705_portC_w )
{
	portC = data;

	if ((data & 0x08) == 0)
	{
		switch (data & 0x07)
		{
			case 0x0:	/* dsw A */
				portA_in = readinputport(0);
				break;

			case 0x1:	/* dsw B */
				portA_in = readinputport(1);
				break;

			case 0x2:
				break;

			case 0x3:	/* command from Z80 */
				portA_in = from_z80;
log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: z80 reads command %02x\n",activecpu_get_pc(),from_z80);
				break;

			case 0x4:
				break;

			case 0x5:	/* answer to Z80 */
				timer_set(TIME_NOW, portB_out, delayed_mcu_z80_w);
				break;

			case 0x6:
				portA_in = read_dial(0);
				break;

			case 0x7:
				portA_in = read_dial(1);
				break;
		}
	}
}

static WRITE_HANDLER( hotsmash_z80_mcu_w )
{
	timer_set(TIME_NOW, data, delayed_z80_mcu_w);
}

static READ_HANDLER(hotsmash_from_mcu_r)
{
log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: z80 reads answer %02x\n",activecpu_get_pc(),from_mcu);
	from_mcu_pending = 0;
	return from_mcu;
}

static READ_HANDLER(hotsmash_ay_port_a_r)
{
/* log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: ay_port_a_r and mcu_pending is %d\n",activecpu_get_pc(),from_mcu_pending); */
	return readinputport(2) | ((from_mcu_pending^1) << 7);
}

/**************************************************************************

pbillian MCU simulation

**************************************************************************/

static WRITE_HANDLER( pbillian_z80_mcu_w )
{
	from_z80 = data;
}

static READ_HANDLER(pbillian_from_mcu_r)
{
	static int curr_player;

	switch (from_z80)
	{
		case 0x01: return readinputport(4 + 2 * curr_player);
		case 0x02: return readinputport(5 + 2 * curr_player);
		case 0x04: return readinputport(0);
		case 0x08: return readinputport(1);
		case 0x80: curr_player = 0; return 0;
		case 0x81: curr_player = 1; return 0;
	}

	log_cb(RETRO_LOG_DEBUG, LOGPRE "408[%x] r at %x\n",from_z80,activecpu_get_pc());
	return 0;
}

static READ_HANDLER(pbillian_ay_port_a_r)
{
/*  log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: ay_port_a_r\n",activecpu_get_pc()); */
	 /* bits 76------  MCU status bits */
	return (rand()&0xc0)|readinputport(3);
}


static MEMORY_READ_START( readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xe000, 0xe0ff, MRA_RAM },
	{ 0xe100, 0xe7ff, MRA_RAM },
	{ 0xe800, 0xefff, MRA_RAM },
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem )
    { 0x0000, 0x7fff, MWA_ROM },
/*	{ 0x8000, 0xbfff, MWA_BANK1 }, */
	{ 0xe000, 0xe0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe100, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xefff, superqix_videoram_w, &superqix_videoram },
	{ 0xf000, 0xffff, MWA_RAM },
MEMORY_END


static PORT_READ_START( pbillian_readport )
    { 0x0000, 0x01ff, paletteram_r },
	{ 0x0401, 0x0401, AY8910_read_port_0_r },
	{ 0x0408, 0x0408, pbillian_from_mcu_r },
	{ 0x0418, 0x0418, MRA_NOP },  
	{ 0x041b, 0x041b, MRA_NOP },  /* input related? but probably not used */
PORT_END

static PORT_WRITE_START( pbillian_writeport )
    { 0x0000, 0x01ff, paletteram_BBGGRRII_w },
	{ 0x0402, 0x0402, AY8910_write_port_0_w },
	{ 0x0403, 0x0403, AY8910_control_port_0_w },
	{ 0x0408, 0x0408, pbillian_z80_mcu_w },
	{ 0x0410, 0x0410, pbillian_0410_w },
	{ 0x0419, 0x0419, MWA_NOP },  /*? watchdog ? */
	{ 0x041a, 0x041a, pbillian_sample_trigger_w },
PORT_END

static PORT_READ_START( hotsmash_readport )
    { 0x0000, 0x01ff, paletteram_r },
	{ 0x0401, 0x0401, AY8910_read_port_0_r },
	{ 0x0408, 0x0408, hotsmash_from_mcu_r },
	{ 0x0418, 0x0418, MRA_NOP },
	{ 0x041b, 0x041b, MRA_NOP },  /* input related? but probably not used */
PORT_END

static PORT_WRITE_START( hotsmash_writeport )
    { 0x0000, 0x01ff, paletteram_BBGGRRII_w },
	{ 0x0402, 0x0402, AY8910_write_port_0_w },
	{ 0x0403, 0x0403, AY8910_control_port_0_w },
	{ 0x0408, 0x0408, hotsmash_z80_mcu_w },
	{ 0x0410, 0x0410, pbillian_0410_w },
	{ 0x0419, 0x0419, MWA_NOP }, /*? watchdog ? */
	{ 0x041a, 0x041a, pbillian_sample_trigger_w },
PORT_END

static PORT_READ_START( sqix_readport )
    { 0x0000, 0x00ff, paletteram_r },
	{ 0x0401, 0x0401, AY8910_read_port_0_r },
	{ 0x0405, 0x0405, AY8910_read_port_1_r },
	{ 0x0418, 0x0418, input_port_2_r },
	{ 0x0800, 0x77ff, superqix_bitmapram_r },
	{ 0x8800, 0xf7ff, superqix_bitmapram2_r },
PORT_END

static PORT_WRITE_START( sqix_writeport )
    { 0x0000, 0x00ff, paletteram_BBGGRRII_w },
	{ 0x0402, 0x0402, AY8910_write_port_0_w },
	{ 0x0403, 0x0403, AY8910_control_port_0_w },
	{ 0x0406, 0x0406, AY8910_write_port_1_w },
	{ 0x0407, 0x0407, AY8910_control_port_1_w },
	{ 0x0408, 0x0408, sqix_flipscreen_w },
	{ 0x0410, 0x0410, superqix_0410_w },	/* ROM bank, NMI enable, tile bank */
	{ 0x0800, 0x77ff, superqix_bitmapram_w}, 
	{ 0x8800, 0xf7ff, superqix_bitmapram2_w },
PORT_END

static MEMORY_READ_START( m68705_readmem )
    { 0x0000, 0x0000, hotsmash_68705_portA_r },
	{ 0x0002, 0x0002, hotsmash_68705_portC_r },
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( m68705_writemem )
    { 0x0001, 0x0001, hotsmash_68705_portB_w },
	{ 0x0002, 0x0002, hotsmash_68705_portC_w },
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
MEMORY_END

INPUT_PORTS_START( pbillian )
	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x40, 0x00,( "Allow_Continue" ) )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "10/20/300K Points" )
	PORT_DIPSETTING(    0x00, "10/30/500K Points" )
	PORT_DIPSETTING(    0x08, "20/30/400K Points" )
	PORT_DIPSETTING(    0x04, "30/40/500K Points" )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, ( "Easy" ) )
	PORT_DIPSETTING(    0x10, ( "Normal" ) )
	PORT_DIPSETTING(    0x20, ( "Hard" ) )
	PORT_DIPSETTING(    0x30, ( "Very_Hard" ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )	/* high score initials */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )	/* fire (M powerup) + high score initials */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )	/* high score initials */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )	/* fire (M powerup) + high score initials */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* mcu status (pending mcu->z80) */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* mcu status (pending z80->mcu) */

	PORT_START
	PORT_ANALOG( 0x3f, 0x00, IPT_PADDLE_V| IPF_REVERSE , 30, 3, 0, 0x3f )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 20, 10, 0, 0 )

	PORT_START
	PORT_ANALOG( 0x3f, 0x00, IPT_PADDLE_V| IPF_REVERSE , 30, 3, 0, 0x3f )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1| IPF_COCKTAIL )
	
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 20, 10, 0, 0 )
	
INPUT_PORTS_END

INPUT_PORTS_START( hotsmash )
	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
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
	PORT_DIPNAME( 0x03, 0x03, "Difficulty vs. CPU" )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Difficulty vs. 2P" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Points per game" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* mcu status (0 = pending mcu->z80) */

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER1 , 15, 30, 0, 0xff )
	
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER2 , 15, 30, 0, 0xff )

INPUT_PORTS_END


INPUT_PORTS_START( superqix )
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Freeze" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, ( "Allow_Continue" ) )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ))
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, ( "Easy" ) )
	PORT_DIPSETTING(    0x03, ( "Normal" ) )
	PORT_DIPSETTING(    0x01, ( "Hard" ) )
	PORT_DIPSETTING(    0x00, ( "Hardest" ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "20000 50000" )
	PORT_DIPSETTING(    0x0c, "30000 100000" )
	PORT_DIPSETTING(    0x04, "50000 100000" )
	PORT_DIPSETTING(    0x00, ( "None" ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0xc0, 0xc0, "Fill Area" )
	PORT_DIPSETTING(    0x80, "70%" )
	PORT_DIPSETTING(    0xc0, "75%" )
	PORT_DIPSETTING(    0x40, "80%" )
	PORT_DIPSETTING(    0x00, "85%" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )	/* doesn't work in bootleg */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* Z80 status (pending z80->mcu) */

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_VBLANK )	/* ??? */
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* mcu status (pending mcu->z80) */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* mcu status (pending z80->mcu) */
INPUT_PORTS_END



static struct GfxLayout pbillian_charlayout =
{
	8,8,
	0x800,	/* doesn't use the whole ROM space */
	4,
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout sqix_charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
			32*8+0*4, 32*8+1*4, 32*8+2*4, 32*8+3*4, 32*8+4*4, 32*8+5*4, 32*8+6*4, 32*8+7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8
};


static struct GfxDecodeInfo pbillian_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pbillian_charlayout, 16*16, 16 },
	{ REGION_GFX1, 0, &spritelayout,            0, 16 },
	{ -1 }
};

static struct GfxDecodeInfo sqix_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &sqix_charlayout,   0, 16 },	/* Chars */
	{ REGION_GFX2, 0x00000, &sqix_charlayout,   0, 16 },	/* Background tiles */
	{ REGION_GFX3, 0x00000, &spritelayout,      0, 16 },	/* Sprites */
	{ -1 }
};



static struct CustomSound_interface custom_interface =
{
	pbillian_sh_start,
	0,
	0
};

static struct AY8910interface pbillian_ay8910_interface =
{
	1,	/* 1 chip */
	12000000/8,	/* 1.5 MHz */
	{ 30 },
	{ pbillian_ay_port_a_r },					/* port Aread */
	{ input_port_2_r },					/* port Bread */
	{ 0 },								/* port Awrite */
	{ 0 }								/* port Bwrite */
};

static struct AY8910interface hotsmash_ay8910_interface =
{
	1,	/* 1 chip */
	12000000/8,	/* 1.5 MHz */
	{ 30 },
	{ hotsmash_ay_port_a_r },					/* port Aread */
	{ input_port_2_r },					/* port Bread */
	{ 0 },								/* port Awrite */
	{ 0 }								/* port Bwrite */
};


static struct AY8910interface sqix_ay8910_interface =
{
	2,	/* 2 chips */
	12000000/8,	/* 1.5 MHz */
	{ 25, 25 },
	{ input_port_3_r, input_port_1_r },	/* port Aread */
	{ input_port_4_r, sqix_in0_r },	    /* port Bread */
	{ 0 },								/* port Awrite */
	{ 0 }								/* port Bwrite */
};


INTERRUPT_GEN( sqix_interrupt )
{
	/* highly suspicious... */
	if (cpu_getiloops() <= 3)
		nmi_line_pulse();
}


static MACHINE_DRIVER_START( pbillian )
	MDRV_CPU_ADD(Z80,12000000/2)		 /* 6 MHz */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(pbillian_readport,pbillian_writeport)
	MDRV_CPU_FLAGS(CPU_16BIT_PORT)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(pbillian_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(512)

	MDRV_VIDEO_START(pbillian)
	MDRV_VIDEO_UPDATE(pbillian)

	MDRV_SOUND_ADD(AY8910, pbillian_ay8910_interface)
	MDRV_SOUND_ADD(CUSTOM, custom_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( hotsmash )
	MDRV_CPU_ADD(Z80,12000000/2)		 /* 6 MHz */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(hotsmash_readport,hotsmash_writeport)
	MDRV_CPU_FLAGS(CPU_16BIT_PORT) /* needed for this game.?? */
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(M68705, 4000000/2) /* ???? */
	MDRV_CPU_MEMORY(m68705_readmem,m68705_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(pbillian_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(512)

	MDRV_VIDEO_START(pbillian)
	MDRV_VIDEO_UPDATE(pbillian)

	MDRV_SOUND_ADD(AY8910, hotsmash_ay8910_interface)
	MDRV_SOUND_ADD(CUSTOM, custom_interface)
MACHINE_DRIVER_END	

static MACHINE_DRIVER_START( sqix )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 12000000/2)	/* 6 MHz */
	MDRV_CPU_FLAGS(CPU_16BIT_PORT)
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(sqix_readport,sqix_writeport)
	MDRV_CPU_VBLANK_INT(sqix_interrupt,6)	/* ??? */

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(sqix_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)

	MDRV_VIDEO_START(superqix)
	MDRV_VIDEO_UPDATE(superqix)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, sqix_ay8910_interface)
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( pbillian )
	ROM_REGION( 0x018000, REGION_CPU1, 0 )
	ROM_LOAD( "1.6c",  0x00000, 0x08000, CRC(d379fe23) SHA1(e147a9151b1cdeacb126d9713687bd0aa92980ac) )
	ROM_LOAD( "2.6d",  0x14000, 0x04000, CRC(1af522bc) SHA1(83e002dc831bfcedbd7096b350c9b34418b79674) )

	ROM_REGION( 0x0800, REGION_CPU2, 0 )
	ROM_LOAD( "pbillian.mcu", 0x0000, 0x0800, NO_DUMP )

	ROM_REGION( 0x8000, REGION_SOUND1, 0 )
	ROM_LOAD( "3.7j",  0x0000, 0x08000, CRC(3f9bc7f1) SHA1(0b0c2ec3bea6a7f3fc6c0c8b750318f3f9ec3d1f) )

	ROM_REGION( 0x018000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "4.1n",  0x00000, 0x08000, CRC(9c08a072) SHA1(25f31fcf72216cf42528b07ad8c09113aa69861a) )
	ROM_LOAD( "5.1r",  0x08000, 0x08000, CRC(2dd5b83f) SHA1(b05e3a008050359d0207757b9cbd8cee87abc697) )
	ROM_LOAD( "6.1t",  0x10000, 0x08000, CRC(33b855b0) SHA1(5a1df4f82fc0d6f78883b759fd61f395942645eb) )
ROM_END

ROM_START( hotsmash )
	ROM_REGION( 0x018000, REGION_CPU1, 0 )
	ROM_LOAD( "b18-04",  0x00000, 0x08000, CRC(981bde2c) SHA1(ebcc901a036cde16b33d534d423500d74523b781) )

	ROM_REGION( 0x0800, REGION_CPU2, 0 )
	ROM_LOAD( "b18-06.mcu", 0x0000, 0x0800, CRC(67c0920a) SHA1(23a294892823d1d9216ea8ddfa9df1c8af149477) )

	ROM_REGION( 0x8000, REGION_SOUND1, 0 )
	ROM_LOAD( "b18-05",  0x0000, 0x08000, CRC(dab5e718) SHA1(6cf6486f283f5177dfdc657b1627fbfa3f0743e8) )

	ROM_REGION( 0x018000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "b18-01",  0x00000, 0x08000, CRC(870a4c04) SHA1(a029108bcda40755c8320d2ee297f42d816aa7c0) )
	ROM_LOAD( "b18-02",  0x08000, 0x08000, CRC(4e625cac) SHA1(2c21b32240eaada9a5f909a2ec5b335372c8c994) )
	ROM_LOAD( "b18-03",  0x14000, 0x04000, CRC(1c82717d) SHA1(6942c8877e24ac51ed71036e771a1655d82f3491) )
ROM_END

ROM_START( superqix )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "sq01.97",      0x00000, 0x08000, CRC(0888b7de) SHA1(de3e4637436de185f43d2ad4186d4cfdcd4d33d9) )
	ROM_LOAD( "sq02.96",      0x10000, 0x10000, CRC(9c23cb64) SHA1(7e04cb18cabdc0031621162cbc228cd95875a022) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "sq04.2",       0x00000, 0x08000, CRC(f815ef45) SHA1(4189d455b6ccf3ae922d410fb624c4665203febf) )

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "sq03.3",       0x00000, 0x10000, CRC(6e8b6a67) SHA1(c71117cc880a124c46397c446d1edc1cbf681200) )
	ROM_LOAD( "sq06.14",      0x10000, 0x10000, CRC(38154517) SHA1(703ad4cfe54a4786c67aedcca5998b57f39fd857) )

	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "sq05.1",       0x00000, 0x10000, CRC(df326540) SHA1(1fe025edcd38202e24c4e1005f478b6a88533453) )

	ROM_REGION( 0x1000, REGION_USER1, 0 )	/* Unknown (protection related?) */
	ROM_LOAD( "sq07.108",     0x00000, 0x1000, CRC(071a598c) SHA1(2726705c3b82f5703e856261cdec5e86d7e1994e) )	/* FIXED BITS (xxxx1xxx)*/
ROM_END

ROM_START( sqixbl )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "cpu.2",        0x00000, 0x08000, CRC(682e28e3) SHA1(fe9221d26d7397be5a0fc8fdc51672b5924f3cf2) )
	ROM_LOAD( "sq02.96",      0x10000, 0x10000, CRC(9c23cb64) SHA1(7e04cb18cabdc0031621162cbc228cd95875a022) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "sq04.2",       0x00000, 0x08000, CRC(f815ef45) SHA1(4189d455b6ccf3ae922d410fb624c4665203febf) )

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "sq03.3",       0x00000, 0x10000, CRC(6e8b6a67) SHA1(c71117cc880a124c46397c446d1edc1cbf681200) )
	ROM_LOAD( "sq06.14",      0x10000, 0x10000, CRC(38154517) SHA1(703ad4cfe54a4786c67aedcca5998b57f39fd857) )

	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "sq05.1",       0x00000, 0x10000, CRC(df326540) SHA1(1fe025edcd38202e24c4e1005f478b6a88533453) )
ROM_END

ROM_START( perestro )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )	/* 64k for code */
	/* 0x8000 - 0x10000 in the rom is empty anyway */
	ROM_LOAD( "rom1.bin",        0x00000, 0x20000, CRC(0cbf96c1) SHA1(cf2b1367887d1b8812a56aa55593e742578f220c) )

	ROM_REGION( 0x10000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "rom4.bin",       0x00000, 0x10000, CRC(c56122a8) SHA1(1d24b2f0358e14aca5681f92175869224584a6ea) ) /* both halves identical */

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "rom2.bin",       0x00000, 0x20000, CRC(36f93701) SHA1(452cb23efd955c6c155cef2b1b650e253e195738) )

	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "rom3.bin",       0x00000, 0x10000, CRC(00c91d5a) SHA1(fdde56d3689a47e6bfb296e442207b93b887ec7a) )
ROM_END



static DRIVER_INIT( pbillian )
{
	pbillian_show_power = 1;
}

static DRIVER_INIT( hotsmash )
{
	pbillian_show_power = 0;
}

static DRIVER_INIT( sqix )
{
	invert_coin_lockout = 1;
}


static DRIVER_INIT( perestro )
{
	data8_t *src;
	int len;
	data8_t temp[16];
	int i,j;

	/* decrypt program code; the address lines are shuffled around in a non-trivial way */
	src = memory_region(REGION_CPU1);
	len = memory_region_length(REGION_CPU1);
	for (i = 0;i < len;i += 16)
	{
		memcpy(temp,&src[i],16);
		for (j = 0;j < 16;j++)
		{
			static int convtable[16] =
			{
				0xc, 0x9, 0xb, 0xa,
				0x8, 0xd, 0xf, 0xe,
				0x4, 0x1, 0x3, 0x2,
				0x0, 0x5, 0x7, 0x6
			};

			src[i+j] = temp[convtable[j]];
		}
	}

	/* decrypt gfx ROMs; simple bit swap on the address lines */
	src = memory_region(REGION_GFX1);
	len = memory_region_length(REGION_GFX1);
	for (i = 0;i < len;i += 16)
	{
		memcpy(temp,&src[i],16);
		for (j = 0;j < 16;j++)
		{
			src[i+j] = temp[BITSWAP8(j,7,6,5,4,3,2,0,1)];
		}
	}

	src = memory_region(REGION_GFX2);
	len = memory_region_length(REGION_GFX2);
	for (i = 0;i < len;i += 16)
	{
		memcpy(temp,&src[i],16);
		for (j = 0;j < 16;j++)
		{
			src[i+j] = temp[BITSWAP8(j,7,6,5,4,0,1,2,3)];
		}
	}

	src = memory_region(REGION_GFX3);
	len = memory_region_length(REGION_GFX3);
	for (i = 0;i < len;i += 16)
	{
		memcpy(temp,&src[i],16);
		for (j = 0;j < 16;j++)
		{
			src[i+j] = temp[BITSWAP8(j,7,6,5,4,1,0,3,2)];
		}
	}
}



GAME( 1986, pbillian, 0,        pbillian, pbillian, pbillian, ROT0,  "Taito", "Prebillian" )
GAME( 1987, hotsmash, 0,        hotsmash, hotsmash, hotsmash, ROT90, "Taito", "Vs. Hot Smash" )
GAMEX(1987, superqix, 0,        sqix,     superqix, sqix,     ROT90, "Taito", "Super Qix (set 1)", GAME_NOT_WORKING )
GAME( 1987, sqixbl,   superqix, sqix,     superqix, 0,        ROT90, "bootleg", "Super Qix (bootleg)" )
GAME( 1994, perestro, 0,        sqix,     superqix, perestro, ROT90, "Promat", "Perestroika Girls" )

