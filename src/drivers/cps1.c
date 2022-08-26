/***************************************************************************

  Capcom System 1
  ===============

  Driver provided by:
  Paul Leaman (paul@vortexcomputing.demon.co.uk)

  68000 for game, Z80, YM-2151 and OKIM6295 for sound.

  68000 clock speeds are unknown for all games (except where commented)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"
#include "ost_samples.h"
#include "cps1.h"       /* External CPS1 definitions */


/* in machine/kabuki.c */
void wof_decode(void);
void dino_decode(void);
void punisher_decode(void);
void slammast_decode(void);


static READ16_HANDLER( cps1_input_r )
{
	int control=readinputport(offset);
	return (control<<8) | control;
}

static READ16_HANDLER( cps1_input2_r )
{
	int buttons=readinputport(5);
	return buttons << 8 | buttons;
}

static READ16_HANDLER( cps1_input3_r )
{
    int buttons=readinputport(6);
	return buttons << 8 | buttons;
}


static int dial[2];

static READ16_HANDLER( forgottn_dial_0_r )
{
	return ((readinputport(5) - dial[0]) >> (8*offset)) & 0xff;
}

static READ16_HANDLER( forgottn_dial_1_r )
{
	return ((readinputport(6) - dial[1]) >> (8*offset)) & 0xff;
}

static WRITE16_HANDLER( forgottn_dial_0_reset_w )
{
	dial[0] = readinputport(5);
}

static WRITE16_HANDLER( forgottn_dial_1_reset_w )
{
	dial[1] = readinputport(6);
}


static int cps1_sound_fade_timer;

static WRITE_HANDLER( cps1_snd_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);
	int length = memory_region_length(REGION_CPU2) - 0x10000;
	int bankaddr;

	bankaddr = (data * 0x4000) & (length-1);
	cpu_setbank(1,&RAM[0x10000 + bankaddr]);

	if (data & 0xfe) log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: write %02x to f004\n",activecpu_get_pc(),data);
}

static WRITE16_HANDLER( cps1_sound_fade_w )
{
	if (ACCESSING_LSB)
		cps1_sound_fade_timer = data & 0xff;
}

static READ_HANDLER( cps1_snd_fade_timer_r )
{
	return cps1_sound_fade_timer;
}

static WRITE16_HANDLER( cps1_sound_command_w )
{
	/* Debug.*/
	/*
	if (data != 0xff) {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "%X\n", data);
	}
	*/

	if(ACCESSING_LSB) {
		/* We are playing Final Fight. */
		if( ost_support_enabled(OST_SUPPORT_FFIGHT) ) {
			if(generate_ost_sound_ffight( data ))
				soundlatch_w(0,data & 0xff);
		}
		/* We are playing Street Fighter 2. */
		else if( ost_support_enabled(OST_SUPPORT_SF2) ) {
			if(generate_ost_sound_sf2( data ))
				soundlatch_w(0,data & 0xff);
		}
		else
			soundlatch_w(0,data & 0xff);
	}
}

static WRITE16_HANDLER( cps1_coinctrl_w )
{
/*	usrintf_showmessage("coinctrl %04x",data);*/

	if (ACCESSING_MSB)
	{
		coin_counter_w(0,data & 0x0100);
		coin_counter_w(1,data & 0x0200);
		coin_lockout_w(0,~data & 0x0400);
		coin_lockout_w(1,~data & 0x0800);
	}

	if (ACCESSING_LSB)
	{
		/* mercs sets bit 0 */
		set_led_status(0,data & 0x02);
		set_led_status(1,data & 0x04);
		set_led_status(2,data & 0x08);
	}
}

static WRITE16_HANDLER( cpsq_coinctrl2_w )
{
	if (ACCESSING_LSB)
	{
		coin_counter_w(2,data & 0x01);
		coin_lockout_w(2,~data & 0x02);
		coin_counter_w(3,data & 0x04);
		coin_lockout_w(3,~data & 0x08);
/*
  	{
       char baf[40];
       sprintf(baf,"0xf1c004=%04x", data);
       usrintf_showmessage(baf);
       }
*/
    }
}

static INTERRUPT_GEN( cps1_interrupt )
{
	/* Strider also has a IRQ4 handler. It is input port related, but the game */
	/* works without it (maybe it's used to multiplex controls). It is the */
	/* *only* game to have that. */
	cpu_set_irq_line(0, 2, HOLD_LINE);

	if( ost_support_enabled(OST_SUPPORT_SF2) )
		ost_fade_volume();
}

/********************************************************************
*
*  Q Sound
*  =======
*
********************************************************************/

struct QSound_interface qsound_interface =
{
	QSOUND_CLOCK,
	REGION_SOUND1,
	{ 100,100 }
};

static unsigned char *qsound_sharedram1,*qsound_sharedram2;

INTERRUPT_GEN( cps1_qsound_interrupt )
{
#if 0
I have removed CPU_AUDIO_CPU from the Z(0 so this is no longer necessary
	/* kludge to pass the sound board test with sound disabled */
	if (Machine->sample_rate == 0)
		qsound_sharedram1[0xfff] = 0x77;
#endif

	cpu_set_irq_line(cpu_getactivecpu(), 2, HOLD_LINE);
}


READ16_HANDLER( qsound_rom_r )
{
	unsigned char *rom = memory_region(REGION_USER1);

	if (rom) return rom[offset] | 0xff00;
	else
	{
		usrintf_showmessage("%06x: read sound ROM byte %04x",activecpu_get_pc(),offset);
		return 0;
	}
}

READ16_HANDLER( qsound_sharedram1_r )
{
	return qsound_sharedram1[offset] | 0xff00;
}

WRITE16_HANDLER( qsound_sharedram1_w )
{
	if (ACCESSING_LSB)
		qsound_sharedram1[offset] = data;
}

static READ16_HANDLER( qsound_sharedram2_r )
{
	return qsound_sharedram2[offset] | 0xff00;
}

static WRITE16_HANDLER( qsound_sharedram2_w )
{
	if (ACCESSING_LSB)
		qsound_sharedram2[offset] = data;
}

static WRITE_HANDLER( qsound_banksw_w )
{
	/*
	Z80 bank register for music note data. It's odd that it isn't encrypted
	though.
	*/
	unsigned char *RAM = memory_region(REGION_CPU2);
	int bankaddress=0x10000+((data&0x0f)*0x4000);
	if (bankaddress >= memory_region_length(REGION_CPU2))
	{
		log_cb(RETRO_LOG_DEBUG, LOGPRE "WARNING: Q sound bank overflow (%02x)\n", data);
		bankaddress=0x10000;
	}
	cpu_setbank(1, &RAM[bankaddress]);
}


/********************************************************************
*
*  EEPROM
*  ======
*
*   The EEPROM is accessed by a serial protocol using the register
*   0xf1c006
*
********************************************************************/

static struct EEPROM_interface qsound_eeprom_interface =
{
	7,		/* address bits */
	8,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111"	/* erase command */
};

static struct EEPROM_interface pang3_eeprom_interface =
{
	6,		/* address bits */
	16,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111"	/* erase command */
};

static NVRAM_HANDLER( qsound )
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&qsound_eeprom_interface);

		if (file)
			EEPROM_load(file);
	}
}

static NVRAM_HANDLER( pang3 )
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&pang3_eeprom_interface);

		if (file)
			EEPROM_load(file);
	}
}

READ16_HANDLER( cps1_eeprom_port_r )
{
	return EEPROM_read_bit();
}

WRITE16_HANDLER( cps1_eeprom_port_w )
{
	if (ACCESSING_LSB)
	{
		/*
		bit 0 = data
		bit 6 = clock
		bit 7 = cs
		*/
		EEPROM_write_bit(data & 0x01);
		EEPROM_set_cs_line((data & 0x80) ? CLEAR_LINE : ASSERT_LINE);
		EEPROM_set_clock_line((data & 0x40) ? ASSERT_LINE : CLEAR_LINE);
	}
}



static MEMORY_READ16_START( cps1_readmem )
	{ 0x000000, 0x1fffff, MRA16_ROM }, /* 68000 ROM */
	{ 0x800000, 0x800001, input_port_4_word_r }, /* Player input ports */
	{ 0x800010, 0x800011, input_port_4_word_r }, /* ?? */
	{ 0x800018, 0x80001f, cps1_input_r }, /* Input ports */
	{ 0x800020, 0x800021, MRA16_NOP }, /* ? Used by Rockman ? */
	{ 0x800052, 0x800055, forgottn_dial_0_r }, /* forgotten worlds */
	{ 0x80005a, 0x80005d, forgottn_dial_1_r }, /* forgotten worlds */
	{ 0x800176, 0x800177, cps1_input2_r }, /* Extra input ports */
	{ 0x800178, 0x800179, cps1_input3_r }, /* Captain Commando player 4 controls */
	{ 0x8001fc, 0x8001fd, cps1_input2_r }, /* Input ports (SF Rev E) */
	{ 0x800100, 0x8001ff, cps1_output_r },   /* Output ports */
	{ 0x900000, 0x92ffff, MRA16_RAM },	/* SF2CE executes code from here */
	{ 0xf00000, 0xf0ffff, qsound_rom_r },		/* Slammasters protection */
	{ 0xf18000, 0xf19fff, qsound_sharedram1_r },	/* Q RAM */
	{ 0xf1c000, 0xf1c001, cps1_input2_r },   /* Player 3 controls (later games) */
	{ 0xf1c002, 0xf1c003, cps1_input3_r },   /* Player 4 controls (later games - muscle bombers) */
	{ 0xf1c006, 0xf1c007, cps1_eeprom_port_r },
	{ 0xf1e000, 0xf1ffff, qsound_sharedram2_r },	/* Q RAM */
	{ 0xff0000, 0xffffff, MRA16_RAM },   /* RAM */
MEMORY_END

static MEMORY_WRITE16_START( cps1_writemem )
	{ 0x000000, 0x1fffff, MWA16_ROM },      /* ROM */
	{ 0x800030, 0x800031, cps1_coinctrl_w },
	{ 0x800040, 0x800041, forgottn_dial_0_reset_w },
	{ 0x800048, 0x800049, forgottn_dial_1_reset_w },
	{ 0x800180, 0x800181, cps1_sound_command_w },  /* Sound command */
	{ 0x800188, 0x800189, cps1_sound_fade_w },
	{ 0x800100, 0x8001ff, cps1_output_w, &cps1_output, &cps1_output_size },  /* Output ports */
	{ 0x900000, 0x92ffff, cps1_gfxram_w, &cps1_gfxram, &cps1_gfxram_size },
	{ 0xf18000, 0xf19fff, qsound_sharedram1_w }, /* Q RAM */
	{ 0xf1c004, 0xf1c005, cpsq_coinctrl2_w },   /* Coin control2 (later games) */
	{ 0xf1c006, 0xf1c007, cps1_eeprom_port_w },
	{ 0xf1e000, 0xf1ffff, qsound_sharedram2_w }, /* Q RAM */
	{ 0xff0000, 0xffffff, MWA16_RAM },        /* RAM */
MEMORY_END


static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xd000, 0xd7ff, MRA_RAM },
	{ 0xf001, 0xf001, YM2151_status_port_0_r },
	{ 0xf002, 0xf002, OKIM6295_status_0_r },
	{ 0xf008, 0xf008, soundlatch_r },
	{ 0xf00a, 0xf00a, cps1_snd_fade_timer_r }, /* Sound timer fade */
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xd000, 0xd7ff, MWA_RAM },
	{ 0xf000, 0xf000, YM2151_register_port_0_w },
	{ 0xf001, 0xf001, YM2151_data_port_0_w },
	{ 0xf002, 0xf002, OKIM6295_data_0_w },
	{ 0xf004, 0xf004, cps1_snd_bankswitch_w },
/*	{ 0xf006, 0xf006, MWA_NOP },  // ???? Unknown ???? /*/
MEMORY_END

MEMORY_READ_START( qsound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },  /* banked (contains music data) */
	{ 0xc000, 0xcfff, MRA_RAM },
	{ 0xd007, 0xd007, qsound_status_r },
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END

MEMORY_WRITE_START( qsound_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM, &qsound_sharedram1 },
	{ 0xd000, 0xd000, qsound_data_h_w },
	{ 0xd001, 0xd001, qsound_data_l_w },
	{ 0xd002, 0xd002, qsound_cmd_w },
	{ 0xd003, 0xd003, qsound_banksw_w },
	{ 0xf000, 0xffff, MWA_RAM, &qsound_sharedram2 },
MEMORY_END

/***********************************************************
			 INPUT PORTS, DIPs
***********************************************************/

#define CPS1_COINAGE_1 \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) ) \
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )

#define CPS1_COINAGE_2 \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )

#define CPS1_COINAGE_3 \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x00, "2 Coins/1 Credit (1 to continue)" ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) ) \
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x00, "2 Coins/1 Credit (1 to continue)" ) \
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )

#define CPS1_DIFFICULTY_1 \
	PORT_DIPNAME( 0x07, 0x04, DEF_STR( Difficulty ) ) \
	PORT_DIPSETTING(    0x07, "0 (Easiest)" ) \
	PORT_DIPSETTING(    0x06, "1" ) \
	PORT_DIPSETTING(    0x05, "2" ) \
	PORT_DIPSETTING(    0x04, "3 (Normal)" ) \
	PORT_DIPSETTING(    0x03, "4" ) \
	PORT_DIPSETTING(    0x02, "5" ) \
	PORT_DIPSETTING(    0x01, "6" ) \
	PORT_DIPSETTING(    0x00, "7 (Hardest)" )

#define CPS1_DIFFICULTY_2 \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) ) \
	PORT_DIPSETTING(    0x04, "1 (Easiest)" ) \
	PORT_DIPSETTING(    0x05, "2" ) \
	PORT_DIPSETTING(    0x06, "3" ) \
	PORT_DIPSETTING(    0x07, "4 (Normal)" ) \
	PORT_DIPSETTING(    0x03, "5" ) \
	PORT_DIPSETTING(    0x02, "6" ) \
	PORT_DIPSETTING(    0x01, "7" ) \
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )


INPUT_PORTS_START( forgottn )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )			/* Check code at 0x00111c*/
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )			/* (0x00112c in 'lostwrld')*/
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )			/* Check code at 0x013c78*/
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )			/* (0x013690 in 'lostwrld')*/
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_ANALOGX( 0x0fff, 0x0000, IPT_DIAL | IPF_PLAYER1, 100, 20, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START
	PORT_ANALOGX( 0x0fff, 0x0000, IPT_DIAL | IPF_PLAYER2, 100, 20, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( ghouls )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )  /* Service, but it doesn't give any credit */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0xc0, "Upright 1 Player" )
	PORT_DIPSETTING(    0x80, "Upright 2 Players" )
/*	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )*/
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "10K, 30K and every 30K" )
	PORT_DIPSETTING(    0x10, "20K, 50K and every 70K" )
	PORT_DIPSETTING(    0x30, "30K, 60K and every 70K" )
	PORT_DIPSETTING(    0x00, "40K, 70K and every 80K" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )		/* "Demo Sounds" in manual; doesn?t work*/
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Same as 'ghouls' but additional "Freeze" Dip Switch, different "Lives" Dip Switch,
   and LOTS of "debug" features (to be implemented) */
INPUT_PORTS_START( ghoulsu )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )  /* Service, but it doesn't give any credit */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0xc0, "Upright 1 Player" )
	PORT_DIPSETTING(    0x80, "Upright 2 Players" )
/*	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )*/
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "10K, 30K and every 30K" )
	PORT_DIPSETTING(    0x10, "20K, 50K and every 70K" )
	PORT_DIPSETTING(    0x30, "30K, 60K and every 70K" )
	PORT_DIPSETTING(    0x00, "40K, 70K and every 80K" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )		/* "Demo Sounds" in manual; doesn?t work*/
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Same as 'ghouls' but additional "Freeze" Dip Switch */
INPUT_PORTS_START( daimakai )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )  /* Service, but it doesn't give any credit */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0xc0, "Upright 1 Player" )
	PORT_DIPSETTING(    0x80, "Upright 2 Players" )
/*	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )*/
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "10K, 30K and every 30K" )
	PORT_DIPSETTING(    0x10, "20K, 50K and every 70K" )
	PORT_DIPSETTING(    0x30, "30K, 60K and every 70K" )
	PORT_DIPSETTING(    0x00, "40K, 70K and every 80K" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )		/* "Demo Sounds" in manual; doesn?t work*/
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* "Debug" features to be implemented */
INPUT_PORTS_START( strider )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0xc0, "Upright 1 Player" )
	PORT_DIPSETTING(    0x80, "Upright 2 Players" )
/*	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )*/
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	/* In 'striderj', bit 3 is stored at 0xff8e77 ($e77,A5) via code at 0x000a2a,
	   but this address is never checked again.
	   In 'strider' and 'stridrja', this code even doesn't exist ! */
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "20K, 40K then every 60K" )
	PORT_DIPSETTING(    0x20, "30K, 50K then every 70K" )
	PORT_DIPSETTING(    0x10, "20K & 60K only" )
	PORT_DIPSETTING(    0x00, "30K & 60K only" )
	PORT_DIPNAME( 0xc0, 0x00, "Internal Diff. on Life Loss" )		/* Check code at 0x00d15a*/
	PORT_DIPSETTING(    0xc0, "-3" )
/*	PORT_DIPSETTING(    0x40, "-1" )*/
	PORT_DIPSETTING(    0x00, "-1" )
	PORT_DIPSETTING(    0x80, "Default" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )						/* "6" in the "test mode"*/
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x04, 0x04, "Freeze" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )					/* To enable the "debug" features*/

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* "Debug" features to be implemented
   Same as strider but additional "2 Coins to Start, 1 to Continue" Dip Switch */
INPUT_PORTS_START( stridrua )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0xc0, "Upright 1 Player" )
	PORT_DIPSETTING(    0x80, "Upright 2 Players" )
/*	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )*/
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	/* In 'striderj', bit 3 is stored at 0xff8e77 ($e77,A5) via code at 0x000a2a,
	   but this address is never checked again.
	   In 'strider' and 'stridrja', this code even doesn't exist ! */
	PORT_DIPNAME( 0x08, 0x08, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "20K, 40K then every 60K" )
	PORT_DIPSETTING(    0x20, "30K, 50K then every 70K" )
	PORT_DIPSETTING(    0x10, "20K & 60K only" )
	PORT_DIPSETTING(    0x00, "30K & 60K only" )
	PORT_DIPNAME( 0xc0, 0x00, "Internal Diff. on Life Loss" )		/* Check code at 0x00d15a*/
	PORT_DIPSETTING(    0xc0, "-3" )
/*	PORT_DIPSETTING(    0x40, "-1" )*/
	PORT_DIPSETTING(    0x00, "-1" )
	PORT_DIPSETTING(    0x80, "Default" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )						/* "6" in the "test mode"*/
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x04, 0x04, "Freeze" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )					/* To enable the "debug" features*/

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( dw )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_3
	/* According to the manual, ALL bits 0 to 5 must be ON to have
	   "2 Coins/1 Credit (1 to continue)" for both coin slots */
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, "Freeze" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Turbo Mode" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )			/* "ON"  in the "test mode"*/
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )			/* "OFF" in the "test mode"*/
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )			/* "ON"  in the "test mode"*/
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )			/* "OFF" in the "test mode"*/
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )


	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* "Debug" features to be implemented */
INPUT_PORTS_START( willow )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_3
	/* According to the manual, ALL bits 0 to 5 must be ON to have
	   "2 Coins/1 Credit (1 to continue)" for both coin slots */
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0xc0, "Upright 1 Player" )
	PORT_DIPSETTING(    0x80, "Upright 2 Players" )
/*	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )*/
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_2
	PORT_DIPNAME( 0x18, 0x18, "Nando Speed" )
	PORT_DIPSETTING(    0x10, "Slow" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Fast" )
	PORT_DIPSETTING(    0x00, "Very Fast" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Stage Magic Continue" )		/* Check code at 0x002e1c*/
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* When the Dip Switch is set to "On" , your magic and sword power will be increased
	   at the end of the level if you haven't bought the magic/sword item. But you won't
         notice this before you use the character again. For example, magic power will be
         increased at the end of level 1 but you won't notice it before level 3, and sword
         power will be increased at the end of level 2 but you won't notice it before level 4.
	   Useful addresses to know :
		- 0xff8344.w ($344,A5) : level       (00-05)
		- 0xff8366.b ($366,A5) : magic power (00-06)
		- 0xff8367.b ($367,A5) : sword power (00-04)
	*/

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, "Vitality" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )					/* To enable the "debug" features*/

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* To enable extra choices in the "test mode", you must press "Coin 1" ('5') AND "Service Mode" ('F2') */
INPUT_PORTS_START( unsquad )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_3
	/* According to the manual, ALL bits 0 to 5 must be ON to have
	   "2 Coins/1 Credit (1 to continue)" for both coin slots */
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x18, 0x18, "Damage" )				/* Check code at 0x006f4e*/
	PORT_DIPSETTING(    0x10, "Small" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Big" )
	PORT_DIPSETTING(    0x00, "Biggest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* To enable other choices in the "test mode", you must press ("P1 Button 1" ('Ctrl')
   or "P1 Button 2" ('Alt')) when "Service Mode" is ON */
INPUT_PORTS_START( ffight )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, "Difficulty Level 1" )
	PORT_DIPSETTING(    0x07, "Easiest" )				/* "01"*/
	PORT_DIPSETTING(    0x06, "Easier" )				/* "02"*/
	PORT_DIPSETTING(    0x05, "Easy" )				/* "03"*/
	PORT_DIPSETTING(    0x04, "Normal" )				/* "04"*/
	PORT_DIPSETTING(    0x03, "Medium" )				/* "05"*/
	PORT_DIPSETTING(    0x02, "Hard" )				/* "06"*/
	PORT_DIPSETTING(    0x01, "Harder" )				/* "07"*/
	PORT_DIPSETTING(    0x00, "Hardest" )				/* "08"*/
	PORT_DIPNAME( 0x18, 0x10, "Difficulty Level 2" )
	PORT_DIPSETTING(    0x18, "Easy" )				/* "01"*/
	PORT_DIPSETTING(    0x10, "Normal" )				/* "02"*/
	PORT_DIPSETTING(    0x08, "Hard" )				/* "03"*/
	PORT_DIPSETTING(    0x00, "Hardest" )				/* "04"*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x60, "100k" )
	PORT_DIPSETTING(    0x40, "200k" )
	PORT_DIPSETTING(    0x20, "100k and every 200k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 | IPF_CHEAT )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 | IPF_CHEAT )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END
                                        
INPUT_PORTS_START( ffightae )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, "Difficulty Level 1" )
	PORT_DIPSETTING(    0x07, "Easiest" )				// "01"
	PORT_DIPSETTING(    0x06, "Easier" )				// "02"
	PORT_DIPSETTING(    0x05, "Easy" )				// "03"
	PORT_DIPSETTING(    0x04, "Normal" )				// "04"
	PORT_DIPSETTING(    0x03, "Medium" )				// "05"
	PORT_DIPSETTING(    0x02, "Hard" )				// "06"
	PORT_DIPSETTING(    0x01, "Harder" )				// "07"
	PORT_DIPSETTING(    0x00, "Hardest" )				// "08"
	PORT_DIPNAME( 0x18, 0x10, "Difficulty Level 2" )
	PORT_DIPSETTING(    0x18, "Easy" )				// "01"
	PORT_DIPSETTING(    0x10, "Normal" )				// "02"
	PORT_DIPSETTING(    0x08, "Hard" )				// "03"
	PORT_DIPSETTING(    0x00, "Hardest" )				// "04"
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x60, "100k" )
	PORT_DIPSETTING(    0x40, "200k" )
	PORT_DIPSETTING(    0x20, "100k and every 200k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	
	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END                                       

INPUT_PORTS_START( 1941 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x18, 0x18, "Life Bar" )
	PORT_DIPSETTING(    0x18, "More Slowly" )
	PORT_DIPSETTING(    0x10, "Slowly" )
	PORT_DIPSETTING(    0x08, "Quickly" )
	PORT_DIPSETTING(    0x00, "More Quickly" )
	PORT_DIPNAME( 0x60, 0x60, "Bullet's Speed" )
	PORT_DIPSETTING(    0x60, "Very Slow" )
	PORT_DIPSETTING(    0x40, "Slow" )
	PORT_DIPSETTING(    0x20, "Fast" )
	PORT_DIPSETTING(    0x00, "Very Fast" )
	PORT_DIPNAME( 0x80, 0x80, "Initial Vitality" )
	PORT_DIPSETTING(    0x80, "3 Bars" )
	PORT_DIPSETTING(    0x00, "4 Bars" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, "Throttle Game Speed" ) 	/* turning this off will break the game*/
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( mercs )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x08, 0x08, "Coin Slots" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPNAME( 0x10, 0x10, "Max Players" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

/* According to code at 0x001c4e ('mtwins') or ('chikij') , ALL bits 0 to 5 of DSWA
   must be ON to have "2 Coins/1 Credit (1 to continue)" for both coin slots.
   But according to routine starting at 0x06b27c ('mtwins') or 0x06b4fa ('chikij'),
   bit 6 of DSWA is tested to have the same "feature" in the "test mode".

   Bits 3 and 4 of DSWB affect the number of lives AND the level of damage when you get hit.
   When bit 5 of DSWB is ON you ALWAYS have 1 life but more energy (0x38 instead of 0x20).
   Useful addresses to know :
     - 0xff147b.b : lives  (player 1)
     - 0xff153b.b : lives  (player 2)
     - 0xff14ab.w : energy (player 1)
     - 0xff156b.w : energy (player 2)
*/
INPUT_PORTS_START( mtwins )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x38, 0x18, DEF_STR( Lives ) )
/*	PORT_DIPSETTING(    0x30, "1" )					*/ /* 0x38 energy, smallest damage*/
/*	PORT_DIPSETTING(    0x38, "1" )					*/ /* 0x38 energy, small damage*/
/*	PORT_DIPSETTING(    0x28, "1" )					*/ /* 0x38 energy, big damage*/
/*	PORT_DIPSETTING(    0x20, "1" )					*/ /* 0x38 energy, biggest damage*/
	PORT_DIPSETTING(    0x10, "1" )					/* 0x20 energy, smallest damage*/
	PORT_DIPSETTING(    0x18, "2" )					/* 0x20 energy, small damage*/
	PORT_DIPSETTING(    0x08, "3" )					/* 0x20 energy, big damage*/
	PORT_DIPSETTING(    0x00, "4" )					/* 0x20 energy, biggest damage*/
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* I guess that bit 7 of DSWB was used for debug purpose :
     - code at 0x001094 : move players during "attract mode"
     - code at 0x019b62 ('msword' and 'mswordr1'), 0x019bde ('mswordu') or 0x019c26 ('mswordj') : unknown effect
     - code at 0x01c322 ('msword' and 'mswordr1'), 0x01c39e ('mswordu') or 0x01c3e0 ('mswordj') : unknown effect
   These features are not available because of the 'bra' instruction after the test of bit 7. */
INPUT_PORTS_START( msword )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, "Player's vitality consumption" )		/* "Level 1"*/
	PORT_DIPSETTING(    0x07, "1 (Easiest)" )				/* "Easy 3"		(-1 every 28 frames)*/
	PORT_DIPSETTING(    0x06, "2" )						/* "Easy 2"		(-1 every 24 frames)*/
	PORT_DIPSETTING(    0x05, "3" )						/* "Easy 1"		(-1 every 20 frames)*/
	PORT_DIPSETTING(    0x04, "4 (Normal)" )				/* "Normal"		(-1 every 18 frames)*/
	PORT_DIPSETTING(    0x03, "5" )						/* "Difficult 1"	(-1 every 16 frames)*/
	PORT_DIPSETTING(    0x02, "6" )						/* "Difficult 2"	(-1 every 14 frames)*/
	PORT_DIPSETTING(    0x01, "7" )						/* "Difficult 3"	(-1 every 12 frames)*/
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )				/* "Difficult 4"	(-1 every 8 frames)*/
	PORT_DIPNAME( 0x38, 0x38, "Enemy's vitality and attacking power" )	/* "Level 2"*/
	PORT_DIPSETTING(    0x20, "1 (Easiest)" )				/* "Easy 3"*/
	PORT_DIPSETTING(    0x28, "2" )						/* "Easy 2"*/
	PORT_DIPSETTING(    0x30, "3" )						/* "Easy 1"*/
	PORT_DIPSETTING(    0x38, "4 (Normal)" )				/* "Normal"*/
	PORT_DIPSETTING(    0x18, "5" )						/* "Difficult 1"*/
	PORT_DIPSETTING(    0x10, "6" )						/* "Difficult 2"*/
	PORT_DIPSETTING(    0x08, "7" )						/* "Difficult 3"*/
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )				/* "Difficult 4"*/
	PORT_DIPNAME( 0x40, 0x00, "Stage Select" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, "Vitality Packs" )
	PORT_DIPSETTING(    0x00, "1" )						/* 0x0320*/
	PORT_DIPSETTING(    0x03, "2" )						/* 0x0640*/
	PORT_DIPSETTING(    0x02, "3 (2 when continue)" )			/* 0x0960 (0x0640 when continue)*/
	PORT_DIPSETTING(    0x01, "4 (3 when continue)" )			/* 0x0c80 (0x0960 when continue)*/
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( cawing )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, "Difficulty Level (Enemy's Strength)" )
	PORT_DIPSETTING(    0x07, "1 (Easiest)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4 (Normal)" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x18, 0x18, "Difficulty Level (Player's Strength)" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* "Debug" features to be implemented */
INPUT_PORTS_START( nemo )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x18, 0x18, "Life Bar" )
	PORT_DIPSETTING(    0x00, "Minimun" )
	PORT_DIPSETTING(    0x18, "Medium" )
/*	PORT_DIPSETTING(    0x10, "Medium" )*/
	PORT_DIPSETTING(    0x08, "Maximum" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )					/* To enable the "debug" features*/

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( sf2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Extra buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( sf2j )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x08, 0x00, "2 Players Game" )
	PORT_DIPSETTING(    0x08, "1 Credit/No Continue" )
	PORT_DIPSETTING(    0x00, "2 Credits/Winner Continue" )
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

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Extra buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( 3wonders )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* Free Play: ALL bits 0 to 7 must be ON ; 4C_1C, 4C_1C, 2 Coins to Start, 1 to Continue ON */
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x03, 0x02, "Lives (Midnight Wanderers)" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x08, "Difficulty (Midnight Wanderers)" )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x10, "Lives (Chariot)" )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0xc0, 0x80, "Difficulty (Chariot)" )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x01, "Lives (Don?t Pull)" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x08, "Difficulty (Don?t Pull)" )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( kod )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, "Coin Slots" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPNAME( 0x10, 0x10, "Max Players" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x38, "2" )
	PORT_DIPSETTING(    0x28, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPSETTING(    0x10, "6" )
	PORT_DIPSETTING(    0x08, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x80, "80k and every 400k" )
	PORT_DIPSETTING(    0xc0, "100k and every 450k" )
	PORT_DIPSETTING(    0x40, "160k and every 450k" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

/* Needs further checking
   Same as kod but different "Bonus_life" values */
INPUT_PORTS_START( kodj )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, "Coin Slots" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPNAME( 0x10, 0x10, "Max Players" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x38, "2" )
	PORT_DIPSETTING(    0x28, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPSETTING(    0x10, "6" )
	PORT_DIPSETTING(    0x08, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x80, "80k and every 400k" )
	PORT_DIPSETTING(    0xc0, "200k and every 450k" )
	PORT_DIPSETTING(    0x40, "160k and every 450k" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END


INPUT_PORTS_START( captcomm )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, "Difficulty 1" )
	PORT_DIPSETTING(    0x07, "1 (Easiest)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4 (Normal)" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x18, 0x10, "Difficulty 2" )
	PORT_DIPSETTING(    0x18, "Easy" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, "Max Players" )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x00, "4" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START      /* Player 4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )
INPUT_PORTS_END

INPUT_PORTS_START( knights )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, "Enemy's attack frequency" )
	PORT_DIPSETTING(    0x07, "1 (Easiest)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4 (Normal)" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x38, 0x38, "Enemy's attack power" )
	PORT_DIPSETTING(    0x10, "1 (Easiest)" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x38, "4 (Normal)" )
	PORT_DIPSETTING(    0x30, "5" )
	PORT_DIPSETTING(    0x28, "6" )
	PORT_DIPSETTING(    0x20, "7" )
	PORT_DIPSETTING(    0x18, "8 (Hardest)" )
	PORT_DIPNAME( 0x40, 0x40, "Coin Slots" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPNAME( 0x80, 0x80, "Max Players" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "3" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

INPUT_PORTS_START( varth )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_1
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "600k and every 1.400k" )
	PORT_DIPSETTING(    0x10, "600k 2.000k and 4500k" )
	PORT_DIPSETTING(    0x08, "1.200k 3.500k" )
	PORT_DIPSETTING(    0x00, "2000k only" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( cworld2j )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Extended Test Mode" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x06, "0" )
	PORT_DIPSETTING(    0x05, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPNAME( 0x18, 0x18, "Extend" )
	PORT_DIPSETTING(    0x18, "N" )
	PORT_DIPSETTING(    0x10, "E" )
	PORT_DIPSETTING(    0x00, "D" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0xe0, "3" )
	PORT_DIPSETTING(    0xa0, "4" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( wof )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWB (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf7, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( dino )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWB (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf7, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( punisher )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWB (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf7, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( slammast )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWB (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xf7, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )

	PORT_START     /* Player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START     /* Player 4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( pnickj )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, "Coin Slots" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x00, "2" )
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

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Vs Play Mode" )
	PORT_DIPSETTING(    0xc0, "1 Game Match" )
	PORT_DIPSETTING(    0x80, "3 Games Match" )
	PORT_DIPSETTING(    0x40, "5 Games Match" )
	PORT_DIPSETTING(    0x00, "7 Games Match" )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( qad )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x04, DEF_STR( Difficulty ) )
/*	PORT_DIPSETTING(    0x07, "Easiest" )*/
	PORT_DIPSETTING(    0x06, "Easiest" )
	PORT_DIPSETTING(    0x05, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x03, "Hard" )
	PORT_DIPSETTING(    0x02, "Hardest" )
/*	PORT_DIPSETTING(    0x01, "Hardest" )*/
/*	PORT_DIPSETTING(    0x00, "Hardest" )*/
	PORT_DIPNAME( 0x18, 0x10, "Wisdom" )
	PORT_DIPSETTING(    0x18, "Easy" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x60, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0xc0, "4" )
	PORT_DIPSETTING(    0xe0, "5" )
/*	PORT_DIPSETTING(    0x40, "1" )*/
/*	PORT_DIPSETTING(    0x20, "1" )*/
/*	PORT_DIPSETTING(    0x00, "1" )*/

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( qadj )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "0" )
	PORT_DIPSETTING(    0x06, "1" )
	PORT_DIPSETTING(    0x05, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x03, "4" )
/*	PORT_DIPSETTING(    0x02, "4" )*/
/*	PORT_DIPSETTING(    0x01, "4" )*/
/*	PORT_DIPSETTING(    0x00, "4" )*/
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xa0, "1" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0xe0, "3" )
/*	PORT_DIPSETTING(    0x00, "1" )*/
/*	PORT_DIPSETTING(    0x20, "1" )*/
/*	PORT_DIPSETTING(    0x80, "1" )*/
/*	PORT_DIPSETTING(    0x40, "2" )*/
/*	PORT_DIPSETTING(    0x60, "3" )*/

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( qtono2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	CPS1_COINAGE_2
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	CPS1_DIFFICULTY_1
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x60, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0xe0, "3" )
	PORT_DIPSETTING(    0xa0, "4" )
	PORT_DIPSETTING(    0xc0, "5" )
/*	PORT_DIPSETTING(    0x40, "?" )*/
/*	PORT_DIPSETTING(    0x20, "?" )*/
/*	PORT_DIPSETTING(    0x00, "?" )*/

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x02, 0x02, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( pang3 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWB (not used, EEPROM) */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Needs further checking */
INPUT_PORTS_START( megaman )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x1f, 0x1f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x11, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x12, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x13, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x15, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x16, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0e, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x1f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x1e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x1d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x1b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x1a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x19, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x17, DEF_STR( 1C_9C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( Free_Play ) )
	/* 0x00 to 0x0c 1 Coin/1 Credit */
	PORT_DIPNAME( 0x60, 0x60, "Coin slots" )
/*	PORT_DIPSETTING(    0x00, "Invalid" )*/
	PORT_DIPSETTING(    0x40, "1, Individual" )
	PORT_DIPSETTING(    0x20, "1, Common" )
	PORT_DIPSETTING(    0x60, "2, Common" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Time" )
	PORT_DIPSETTING(    0x0c, "100" )
	PORT_DIPSETTING(    0x08, "90" )
	PORT_DIPSETTING(    0x04, "70" )
	PORT_DIPSETTING(    0x00, "60" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Voice" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
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
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Needs further checking */
/* Same as 'megaman' but no "Voice" Dip Switch */
INPUT_PORTS_START( rockmanj )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE_NO_TOGGLE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSWA */
	PORT_DIPNAME( 0x1f, 0x1f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x11, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x12, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x13, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x15, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x16, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0e, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x1f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x1e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x1d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x1b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x1a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x19, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x17, DEF_STR( 1C_9C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( Free_Play ) )
	/* 0x00 to 0x0c 1 Coin/1 Credit */
	PORT_DIPNAME( 0x60, 0x60, "Coin slots" )
/*	PORT_DIPSETTING(    0x00, "Invalid" )*/
	PORT_DIPSETTING(    0x40, "1, Individual" )
	PORT_DIPSETTING(    0x20, "1, Common" )
	PORT_DIPSETTING(    0x60, "2, Common" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Time" )
	PORT_DIPSETTING(    0x0c, "100" )
	PORT_DIPSETTING(    0x08, "90" )
	PORT_DIPSETTING(    0x04, "70" )
	PORT_DIPSETTING(    0x00, "60" )
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

	PORT_START      /* DSWC */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
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
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END
					
/* CPS Changer */
INPUT_PORTS_START( wofch )
	PORT_START    /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, "Pause", KEYCODE_F1, IP_JOY_NONE ) /* pause */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE  )	/* pause */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )

	PORT_START
	PORT_DIPNAME( 0xff, 0xff, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0xff, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0xff, 0xff, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0xff, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0xff, 0xff, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0xff, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START     /* Player 1 & 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
    PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START      /* Player 3 */
    PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED)
    PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START3)
	
	PORT_START      /* Player 4 - not used */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END	

INPUT_PORTS_START( gulunpa )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )  /* Service, but it doesn't give any credit */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	
	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x18, 0x18, "Coin slots" )
	PORT_DIPSETTING(    0x18, "1, Common" )
	PORT_DIPSETTING(    0x00, "2, Common" )
	PORT_DIPSETTING(    0x10, "2, Common (duplicate 1)" )
	PORT_DIPSETTING(    0x08, "2, Common (duplicate 2)" )
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
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 Easiest" )
	PORT_DIPSETTING(    0x06, "2 Very Easy" )
	PORT_DIPSETTING(    0x05, "3 Easy" )
	PORT_DIPSETTING(    0x04, "4 Medium" )
	PORT_DIPSETTING(    0x03, "5 Medium Hard" )
	PORT_DIPSETTING(    0x02, "6 Hard" )
	PORT_DIPSETTING(    0x01, "7 Very Hard" )
	PORT_DIPSETTING(    0x00, "8 Hardest" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* flipping this on any screen causes scroll1 layer left edge to be filled with tile 0x0014 */
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
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME, "Game Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, "Game" )
	PORT_DIPSETTING(    0x00, "Test" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END                                        
                                        

static struct GfxLayout layout8x8 =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ GFX_RAW },
	{ 4*8 },	/* org displacement - 8x8 tiles are taken from the RIGHT side of the 16x16 tile
				   (fixes cawing which uses character 0x0002 as space, typo instead of 0x20?) */
	{ 8*8 },	/* line modulo */
	64*8		/* char modulo */
};

static struct GfxLayout layout16x16 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ GFX_RAW },
	{ 0 },		/* org displacement */
	{ 8*8 },	/* line modulo */
	128*8		/* char modulo */
};

static struct GfxLayout layout32x32 =
{
	32,32,
	RGN_FRAC(1,1),
	4,
	{ GFX_RAW },
	{ 0 },		/* org displacement */
	{ 16*8 },	/* line modulo */
	512*8		/* char modulo */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout8x8,   0, 0x100 },
	{ REGION_GFX1, 0, &layout16x16, 0, 0x100 },
	{ REGION_GFX1, 0, &layout32x32, 0, 0x100 },
	{ -1 }
};



static void cps1_irq_handler_mus(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2151interface ym2151_interface =
{
	1,  /* 1 chip */
	3579580,    /* 3.579580 MHz ? */
	{ YM3012_VOL(35,MIXER_PAN_LEFT,35,MIXER_PAN_RIGHT) },
	{ cps1_irq_handler_mus }
};

static struct OKIM6295interface okim6295_interface_6061 =
{
	1,  /* 1 chip */
	{ 6061 },
	{ REGION_SOUND1 },
	{ 30 }
};

static struct OKIM6295interface okim6295_interface_7576 =
{
	1,  /* 1 chip */
	{ 7576 },
	{ REGION_SOUND1 },
	{ 30 }
};



/********************************************************************
*
*  Machine Driver macro
*  ====================
*
*  Abusing the pre-processor.
*
********************************************************************/

static MACHINE_DRIVER_START( cps1 )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 10000000)
	MDRV_CPU_MEMORY(cps1_readmem,cps1_writemem)
	MDRV_CPU_VBLANK_INT(cps1_interrupt,1)

	MDRV_CPU_ADD_TAG("sound", Z80, 4000000)	/* 4 MHz ??? TODO: find real FRQ */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(8*8, (64-8)*8-1, 2*8, 30*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(4096)

	MDRV_VIDEO_START(cps1)
	MDRV_VIDEO_EOF(cps1)
	MDRV_VIDEO_UPDATE(cps1)

	/* sound hardware */
	MDRV_SOUND_ADD_TAG("2151", YM2151, ym2151_interface)
	MDRV_SOUND_ADD_TAG("okim", OKIM6295, okim6295_interface_7576)
MACHINE_DRIVER_END

/* For Final Fight.*/
static MACHINE_DRIVER_START( ffight_hack )
	
	/* basic machine hardware */
	MDRV_IMPORT_FROM(cps1)

	/* Lets add our Final Fight music sample packs.*/
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_INSTALL_OST_SUPPORT(OST_SUPPORT_FFIGHT)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( forgottn )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(cps1)

	/* sound hardware */
	MDRV_SOUND_REPLACE("okim", OKIM6295, okim6295_interface_6061)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( sf2 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(cps1)
	MDRV_CPU_REPLACE("main", M68000, 12000000)

	/* Lets add our Street Fighter 2 music sample packs.*/
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_INSTALL_OST_SUPPORT(OST_SUPPORT_SF2)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( pang3 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(cps1)
	MDRV_NVRAM_HANDLER(pang3)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( qsound )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(cps1)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_VBLANK_INT(cps1_qsound_interrupt,1)  /* ??? interrupts per frame */

	MDRV_CPU_REPLACE("sound", Z80, 6000000)
	MDRV_CPU_FLAGS(0)	/* can't use CPU_AUDIO_CPU, slammast requires the Z80 for protection */
	MDRV_CPU_MEMORY(qsound_readmem,qsound_writemem)
	MDRV_CPU_PERIODIC_INT(irq0_line_hold,250)	/* ?? */

	MDRV_NVRAM_HANDLER(qsound)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_REPLACE("2151", QSOUND, qsound_interface)
	MDRV_SOUND_REMOVE("okim")

MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

#define CODE_SIZE 0x200000

ROM_START( forgottn )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "lwu11a",        0x00000, 0x20000, CRC(ddf78831) SHA1(b9c815613efdfde933d4500b588798b7fb4c1854) )
	ROM_LOAD16_BYTE( "lwu15a",        0x00001, 0x20000, CRC(f7ce2097) SHA1(44c06fabdb6de7d8afc2164458c90b0be9cf945d) )
	ROM_LOAD16_BYTE( "lwu10a",        0x40000, 0x20000, CRC(8cb38c81) SHA1(1d36cab7d17ff778ee7dfcd9606a3a87f6906f21) )
	ROM_LOAD16_BYTE( "lwu14a",        0x40001, 0x20000, CRC(d70ef9fd) SHA1(b393aa2a7bea440fdcf057ffc6ff233fc0d35d4b) )
	ROM_LOAD16_WORD_SWAP( "lw-07",         0x80000, 0x80000, CRC(fd252a26) SHA1(5cfb097984912a5167a8c7ec4c2e119b642f9970) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "lw-02",         0x000000, 0x80000, CRC(43e6c5c8) SHA1(d3e6c971de0477ec4e178adc82508208dd8b397f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-09",         0x000002, 0x80000, CRC(899cb4ad) SHA1(95e61af338945e690f2a82746feba3871ea224eb) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-06",         0x000004, 0x80000, CRC(5b9edffc) SHA1(6fd8f4a3ab070733b52365ab1945bf86acb2bf62) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-13",         0x000006, 0x80000, CRC(8e058ef5) SHA1(00f2c0050fd106276ea5398511c5861ebfbc0d10) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-01",         0x200000, 0x80000, CRC(0318f298) SHA1(178ffd6da7bf845e30abf1bfc38a469cd319a73f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-08",         0x200002, 0x80000, CRC(25a8e43c) SHA1(d57cee1fc508db2677e84882fb814e4d9ad20543) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-05",         0x200004, 0x80000, CRC(e4552fd7) SHA1(11147afc475904848458425661473586dd6f60cc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-12",         0x200006, 0x80000, CRC(8e6a832b) SHA1(d63a1331fda2365f090fa31950098f321a720ea8) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU */
	ROM_LOAD( "lwu00",         0x00000, 0x08000, CRC(59df2a63) SHA1(dfe1fffc7a17179a80a2ae623e93b30a7d6df20d) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "lw-03u",        0x00000, 0x20000, CRC(807d051f) SHA1(720e4733787b9b11f4d1cdce0892b69475802844) )
	ROM_LOAD( "lw-04u",        0x20000, 0x20000, CRC(e6cd098e) SHA1(667f6e5736f76a1c4c450c4e2035574ea89d7910) )
ROM_END

ROM_START( lostwrld )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "lw-11c.14f",    0x00000, 0x20000, CRC(67e42546) SHA1(3e385661f71616180a26b74e443978077246fe66) )
	ROM_LOAD16_BYTE( "lw-15c.14g",    0x00001, 0x20000, CRC(402e2a46) SHA1(cbb7017e75a425706505717bf83c2615f53309f9) )
	ROM_LOAD16_BYTE( "lw-10c.13f",    0x40000, 0x20000, CRC(c46479d7) SHA1(84fd9ef33ae7d0af2110e8dc299de25c0f039cee) )
	ROM_LOAD16_BYTE( "lw-14c.13g",    0x40001, 0x20000, CRC(97670f4a) SHA1(f249977c814abdff85007216d7fa57db5684be0f) )
	ROM_LOAD16_WORD_SWAP( "lw-07",         0x80000, 0x80000, CRC(fd252a26) SHA1(5cfb097984912a5167a8c7ec4c2e119b642f9970) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "lw-02",         0x000000, 0x80000, CRC(43e6c5c8) SHA1(d3e6c971de0477ec4e178adc82508208dd8b397f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-09",         0x000002, 0x80000, CRC(899cb4ad) SHA1(95e61af338945e690f2a82746feba3871ea224eb) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-06",         0x000004, 0x80000, CRC(5b9edffc) SHA1(6fd8f4a3ab070733b52365ab1945bf86acb2bf62) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-13",         0x000006, 0x80000, CRC(8e058ef5) SHA1(00f2c0050fd106276ea5398511c5861ebfbc0d10) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-01",         0x200000, 0x80000, CRC(0318f298) SHA1(178ffd6da7bf845e30abf1bfc38a469cd319a73f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-08",         0x200002, 0x80000, CRC(25a8e43c) SHA1(d57cee1fc508db2677e84882fb814e4d9ad20543) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-05",         0x200004, 0x80000, CRC(e4552fd7) SHA1(11147afc475904848458425661473586dd6f60cc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "lw-12",         0x200006, 0x80000, CRC(8e6a832b) SHA1(d63a1331fda2365f090fa31950098f321a720ea8) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU */
	ROM_LOAD( "lwu00",         0x00000, 0x08000, CRC(59df2a63) SHA1(dfe1fffc7a17179a80a2ae623e93b30a7d6df20d) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "lw-03.14c",     0x00000, 0x20000, CRC(ce2159e7) SHA1(77d564f8b768c1cbd6e5b334f7ee86c4c3f9d62e) )
	ROM_LOAD( "lw-04.13c",     0x20000, 0x20000, CRC(39305536) SHA1(ad24d7b6df2dc5e84a35aecb9ba9b0aaa27ab6e5) )
ROM_END

ROM_START( ghouls )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ghl29.bin",    0x00000, 0x20000, CRC(166a58a2) SHA1(f21fcf88d2ebb7bc9e8885fde760a5d82f295c1a) )
	ROM_LOAD16_BYTE( "ghl30.bin",    0x00001, 0x20000, CRC(7ac8407a) SHA1(3613699213db47bfeabedf87f12eb0fa7e5973b6) )
	ROM_LOAD16_BYTE( "ghl27.bin",    0x40000, 0x20000, CRC(f734b2be) SHA1(fa230bf5503487ec11d767485a18f0a55dcc13d2) )
	ROM_LOAD16_BYTE( "ghl28.bin",    0x40001, 0x20000, CRC(03d3e714) SHA1(a07786062358c89f3b4634b8822173261802290b) )
	ROM_LOAD16_WORD( "ghl17.bin",    0x80000, 0x80000, CRC(3ea1b0f2) SHA1(c51f1c38cdaed77ad715cedd845617a291ab2441) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROMX_LOAD( "ghl5.bin",   0x000000, 0x80000, CRC(0ba9c0b0) SHA1(c4945b603115f32b7346d72426571dc2d361159f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl7.bin",   0x000002, 0x80000, CRC(5d760ab9) SHA1(212176947933fcfef991bc80ad5bd91718689ffe) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl6.bin",   0x000004, 0x80000, CRC(4ba90b59) SHA1(35bc9dec5ddbf064c30c951627581c16764456ac) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl8.bin",   0x000006, 0x80000, CRC(4bdee9de) SHA1(7d0c4736f16577afe9966447a18f039728f6fbdf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl09.bin",  0x200000, 0x10000, CRC(ae24bb19) SHA1(aa91c6ffe657b878e10e4e930457b530f7bb529b) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl18.bin",  0x200001, 0x10000, CRC(d34e271a) SHA1(55211fc2861dce32951f41624c9c99c09bf3b184) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl13.bin",  0x200002, 0x10000, CRC(3f70dd37) SHA1(9ecb4dec9d131e9fca15ded7d71986a9fcb62c19) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl22.bin",  0x200003, 0x10000, CRC(7e69e2e6) SHA1(4e0b4d2474beaa5c869c8f1a91893c79ac6e7f39) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl11.bin",  0x200004, 0x10000, CRC(37c9b6c6) SHA1(b2bb82537e335339846dbf9588cfacfdbdd75ee6) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl20.bin",  0x200005, 0x10000, CRC(2f1345b4) SHA1(14c450abcf9defa29c6f48e5ffd0b9d1e4a66a1d) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl15.bin",  0x200006, 0x10000, CRC(3c2a212a) SHA1(f8fa0e0e2d09ea37c54d1c2493752b4e97e3f534) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl24.bin",  0x200007, 0x10000, CRC(889aac05) SHA1(9301dcecee699e7f7672bb498122e1f4831ce536) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl10.bin",  0x280000, 0x10000, CRC(bcc0f28c) SHA1(02f587aa4ae71631f27b0e3aaf1829cdded1bdc2) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl19.bin",  0x280001, 0x10000, CRC(2a40166a) SHA1(dc4e75d7ed87ae5386d721a09113bba364740465) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl14.bin",  0x280002, 0x10000, CRC(20f85c03) SHA1(86385139a9b42270aade758bfe338525936f5671) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl23.bin",  0x280003, 0x10000, CRC(8426144b) SHA1(2dbf9625413b302fcdad5bef8733a9dfbfaead52) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl12.bin",  0x280004, 0x10000, CRC(da088d61) SHA1(67229eff2827a42af97a60ceb252e132e7f307bc) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl21.bin",  0x280005, 0x10000, CRC(17e11df0) SHA1(42fb15e9300b07fc5f4bc21744484869859b130c) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl16.bin",  0x280006, 0x10000, CRC(f187ba1c) SHA1(6d9441d04ecef2a9d9c7a2cc7781acd7904c2061) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl25.bin",  0x280007, 0x10000, CRC(29f79c78) SHA1(26000a58454a06c3016f99ebc3a79c52911a7070) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU */
	ROM_LOAD( "ghl26.bin",     0x00000, 0x08000, CRC(3692f6e5) SHA1(61b8438d60a39b4cf5062dff0a53228e8a4e4b5f) )
	ROM_CONTINUE(              0x10000, 0x08000 )
ROM_END

ROM_START( ghoulsu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "dmu29",        0x00000, 0x20000, CRC(334d85b2) SHA1(89bacc28b7c799c7568420e3de5a99060baa7b0f) )
	ROM_LOAD16_BYTE( "dmu30",        0x00001, 0x20000, CRC(cee8ceb5) SHA1(fc8db1ce0c143dfda0b5989d02d5e5a872e27cd2) )
	ROM_LOAD16_BYTE( "dmu27",        0x40000, 0x20000, CRC(4a524140) SHA1(cebd651293c3570912d5506c1c223c39bcccc802) )
	ROM_LOAD16_BYTE( "dmu28",        0x40001, 0x20000, CRC(94aae205) SHA1(514b3c1b9b0b22300a94229825c3be66332ea5ed) )
	ROM_LOAD16_WORD( "ghl17.bin",    0x80000, 0x80000, CRC(3ea1b0f2) SHA1(c51f1c38cdaed77ad715cedd845617a291ab2441) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROMX_LOAD( "ghl5.bin",   0x000000, 0x80000, CRC(0ba9c0b0) SHA1(c4945b603115f32b7346d72426571dc2d361159f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl7.bin",   0x000002, 0x80000, CRC(5d760ab9) SHA1(212176947933fcfef991bc80ad5bd91718689ffe) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl6.bin",   0x000004, 0x80000, CRC(4ba90b59) SHA1(35bc9dec5ddbf064c30c951627581c16764456ac) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl8.bin",   0x000006, 0x80000, CRC(4bdee9de) SHA1(7d0c4736f16577afe9966447a18f039728f6fbdf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl09.bin",  0x200000, 0x10000, CRC(ae24bb19) SHA1(aa91c6ffe657b878e10e4e930457b530f7bb529b) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl18.bin",  0x200001, 0x10000, CRC(d34e271a) SHA1(55211fc2861dce32951f41624c9c99c09bf3b184) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl13.bin",  0x200002, 0x10000, CRC(3f70dd37) SHA1(9ecb4dec9d131e9fca15ded7d71986a9fcb62c19) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl22.bin",  0x200003, 0x10000, CRC(7e69e2e6) SHA1(4e0b4d2474beaa5c869c8f1a91893c79ac6e7f39) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl11.bin",  0x200004, 0x10000, CRC(37c9b6c6) SHA1(b2bb82537e335339846dbf9588cfacfdbdd75ee6) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl20.bin",  0x200005, 0x10000, CRC(2f1345b4) SHA1(14c450abcf9defa29c6f48e5ffd0b9d1e4a66a1d) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl15.bin",  0x200006, 0x10000, CRC(3c2a212a) SHA1(f8fa0e0e2d09ea37c54d1c2493752b4e97e3f534) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl24.bin",  0x200007, 0x10000, CRC(889aac05) SHA1(9301dcecee699e7f7672bb498122e1f4831ce536) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl10.bin",  0x280000, 0x10000, CRC(bcc0f28c) SHA1(02f587aa4ae71631f27b0e3aaf1829cdded1bdc2) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl19.bin",  0x280001, 0x10000, CRC(2a40166a) SHA1(dc4e75d7ed87ae5386d721a09113bba364740465) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl14.bin",  0x280002, 0x10000, CRC(20f85c03) SHA1(86385139a9b42270aade758bfe338525936f5671) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl23.bin",  0x280003, 0x10000, CRC(8426144b) SHA1(2dbf9625413b302fcdad5bef8733a9dfbfaead52) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl12.bin",  0x280004, 0x10000, CRC(da088d61) SHA1(67229eff2827a42af97a60ceb252e132e7f307bc) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl21.bin",  0x280005, 0x10000, CRC(17e11df0) SHA1(42fb15e9300b07fc5f4bc21744484869859b130c) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl16.bin",  0x280006, 0x10000, CRC(f187ba1c) SHA1(6d9441d04ecef2a9d9c7a2cc7781acd7904c2061) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl25.bin",  0x280007, 0x10000, CRC(29f79c78) SHA1(26000a58454a06c3016f99ebc3a79c52911a7070) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU */
	ROM_LOAD( "ghl26.bin",     0x00000, 0x08000, CRC(3692f6e5) SHA1(61b8438d60a39b4cf5062dff0a53228e8a4e4b5f) )
	ROM_CONTINUE(              0x10000, 0x08000 )
ROM_END

ROM_START( daimakai )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "dmj_38.bin",   0x00000, 0x20000, CRC(82fd1798) SHA1(7a199384659d8e6602384b1953339f221d61a9e6) )
	ROM_LOAD16_BYTE( "dmj_39.bin",   0x00001, 0x20000, CRC(35366ccc) SHA1(42c7004a641f34b9dd1333be51b50639a97e2be9) )
	ROM_LOAD16_BYTE( "dmj_40.bin",   0x40000, 0x20000, CRC(a17c170a) SHA1(62a9cb65df90827334d453a98e826dc1bfc27136) )
	ROM_LOAD16_BYTE( "dmj_41.bin",   0x40001, 0x20000, CRC(6af0b391) SHA1(5a2d74d207c04e24bcea7eeffa1c8b96b6df77e1) )
	ROM_LOAD16_WORD( "ghl17.bin",    0x80000, 0x80000, CRC(3ea1b0f2) SHA1(c51f1c38cdaed77ad715cedd845617a291ab2441) )

	ROM_REGION( 0x300000, REGION_GFX1, 0 )
	ROMX_LOAD( "ghl5.bin",   0x000000, 0x80000, CRC(0ba9c0b0) SHA1(c4945b603115f32b7346d72426571dc2d361159f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl7.bin",   0x000002, 0x80000, CRC(5d760ab9) SHA1(212176947933fcfef991bc80ad5bd91718689ffe) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl6.bin",   0x000004, 0x80000, CRC(4ba90b59) SHA1(35bc9dec5ddbf064c30c951627581c16764456ac) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl8.bin",   0x000006, 0x80000, CRC(4bdee9de) SHA1(7d0c4736f16577afe9966447a18f039728f6fbdf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ghl09.bin",  0x200000, 0x10000, CRC(ae24bb19) SHA1(aa91c6ffe657b878e10e4e930457b530f7bb529b) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl18.bin",  0x200001, 0x10000, CRC(d34e271a) SHA1(55211fc2861dce32951f41624c9c99c09bf3b184) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl13.bin",  0x200002, 0x10000, CRC(3f70dd37) SHA1(9ecb4dec9d131e9fca15ded7d71986a9fcb62c19) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl22.bin",  0x200003, 0x10000, CRC(7e69e2e6) SHA1(4e0b4d2474beaa5c869c8f1a91893c79ac6e7f39) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl11.bin",  0x200004, 0x10000, CRC(37c9b6c6) SHA1(b2bb82537e335339846dbf9588cfacfdbdd75ee6) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl20.bin",  0x200005, 0x10000, CRC(2f1345b4) SHA1(14c450abcf9defa29c6f48e5ffd0b9d1e4a66a1d) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl15.bin",  0x200006, 0x10000, CRC(3c2a212a) SHA1(f8fa0e0e2d09ea37c54d1c2493752b4e97e3f534) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl24.bin",  0x200007, 0x10000, CRC(889aac05) SHA1(9301dcecee699e7f7672bb498122e1f4831ce536) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl10.bin",  0x280000, 0x10000, CRC(bcc0f28c) SHA1(02f587aa4ae71631f27b0e3aaf1829cdded1bdc2) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl19.bin",  0x280001, 0x10000, CRC(2a40166a) SHA1(dc4e75d7ed87ae5386d721a09113bba364740465) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl14.bin",  0x280002, 0x10000, CRC(20f85c03) SHA1(86385139a9b42270aade758bfe338525936f5671) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl23.bin",  0x280003, 0x10000, CRC(8426144b) SHA1(2dbf9625413b302fcdad5bef8733a9dfbfaead52) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl12.bin",  0x280004, 0x10000, CRC(da088d61) SHA1(67229eff2827a42af97a60ceb252e132e7f307bc) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl21.bin",  0x280005, 0x10000, CRC(17e11df0) SHA1(42fb15e9300b07fc5f4bc21744484869859b130c) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl16.bin",  0x280006, 0x10000, CRC(f187ba1c) SHA1(6d9441d04ecef2a9d9c7a2cc7781acd7904c2061) , ROM_SKIP(7) )
	ROMX_LOAD( "ghl25.bin",  0x280007, 0x10000, CRC(29f79c78) SHA1(26000a58454a06c3016f99ebc3a79c52911a7070) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU */
	ROM_LOAD( "ghl26.bin",     0x00000, 0x08000, CRC(3692f6e5) SHA1(61b8438d60a39b4cf5062dff0a53228e8a4e4b5f) )
	ROM_CONTINUE(              0x10000, 0x08000 )
ROM_END

ROM_START( strider )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "strider.30",   0x00000, 0x20000, CRC(da997474) SHA1(3e4ac98f9a6967d61899281b31c7de779723397b) )
	ROM_LOAD16_BYTE( "strider.35",   0x00001, 0x20000, CRC(5463aaa3) SHA1(e2d07ec2d818e9a2e2d7a77ff0309ae4011c0083) )
	ROM_LOAD16_BYTE( "strider.31",   0x40000, 0x20000, CRC(d20786db) SHA1(c9c75488e6bb37cfd0d56073faf87ff5713bc9a0) )
	ROM_LOAD16_BYTE( "strider.36",   0x40001, 0x20000, CRC(21aa2863) SHA1(446dc9280630318deb423531210a4eedfb4adfa6) )
	ROM_LOAD16_WORD_SWAP( "strider.32",   0x80000, 0x80000, CRC(9b3cfc08) SHA1(a7d7f270a097437affa845d80bed82a1fa874878) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "strider.06",   0x000000, 0x80000, CRC(4eee9aea) SHA1(5e619fd5f3f1181e32a8fd9dbb4661d74ff8a484) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.08",   0x000002, 0x80000, CRC(2d7f21e4) SHA1(593cec513de40ff802084d54313bb25a4561e25d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.02",   0x000004, 0x80000, CRC(7705aa46) SHA1(6cbfa30b2852fd117d117beefba434ce41d24c2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.04",   0x000006, 0x80000, CRC(5b18b722) SHA1(cf71c62348ca6b404279e87a6686cb3a842eb381) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.05",   0x200000, 0x80000, CRC(005f000b) SHA1(e6f65af7cc3295be9efaaded352e7ae6320b4133) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.07",   0x200002, 0x80000, CRC(b9441519) SHA1(bb0926dc484dae4f64c5e5a6bce20afdc7aeba55) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.01",   0x200004, 0x80000, CRC(b7d04e8b) SHA1(5c5a079baa694927c33d0e0c23e5ff09d6c9d985) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.03",   0x200006, 0x80000, CRC(6b4713b4) SHA1(759b8b1fc7a5c4b00d74a27c2dd11667db44b09e) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "strider.09",    0x00000, 0x08000, CRC(2ed403bc) SHA1(4ce863ea40d789db5a7cfce91d2c7c720deb9be5) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "strider.18",   0x00000, 0x20000, CRC(4386bc80) SHA1(fb2b261995aeacfa13e7ee40b1a973dfb178f015) )
	ROM_LOAD( "strider.19",   0x20000, 0x20000, CRC(444536d7) SHA1(a14f5de2f6b5b29ae5161dca1f8c08c566301a91) )
ROM_END

ROM_START( stridrua )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "strid.30",     0x00000, 0x20000, CRC(66aec273) SHA1(576b1e9062874e68d68f8725949c151509eb6d56) )
	ROM_LOAD16_BYTE( "strid.35",     0x00001, 0x20000, CRC(50e0e865) SHA1(201ef385c228c124ed9412002233a501ea514efd) )
	ROM_LOAD16_BYTE( "strid.31",     0x40000, 0x20000, CRC(eae93bd1) SHA1(b320a00b67ea3c7fffc6c37d57863163975f7b80) )
	ROM_LOAD16_BYTE( "strid.36",     0x40001, 0x20000, CRC(b904a31d) SHA1(5509d1024151eb8548fd1b29e6c0c95775c61364) )
	ROM_LOAD16_WORD_SWAP( "strider.32",   0x80000, 0x80000, CRC(9b3cfc08) SHA1(a7d7f270a097437affa845d80bed82a1fa874878) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "strider.06",   0x000000, 0x80000, CRC(4eee9aea) SHA1(5e619fd5f3f1181e32a8fd9dbb4661d74ff8a484) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.08",   0x000002, 0x80000, CRC(2d7f21e4) SHA1(593cec513de40ff802084d54313bb25a4561e25d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.02",   0x000004, 0x80000, CRC(7705aa46) SHA1(6cbfa30b2852fd117d117beefba434ce41d24c2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.04",   0x000006, 0x80000, CRC(5b18b722) SHA1(cf71c62348ca6b404279e87a6686cb3a842eb381) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.05",   0x200000, 0x80000, CRC(005f000b) SHA1(e6f65af7cc3295be9efaaded352e7ae6320b4133) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.07",   0x200002, 0x80000, CRC(b9441519) SHA1(bb0926dc484dae4f64c5e5a6bce20afdc7aeba55) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.01",   0x200004, 0x80000, CRC(b7d04e8b) SHA1(5c5a079baa694927c33d0e0c23e5ff09d6c9d985) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.03",   0x200006, 0x80000, CRC(6b4713b4) SHA1(759b8b1fc7a5c4b00d74a27c2dd11667db44b09e) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "strid.09",      0x00000, 0x08000, CRC(08d63519) SHA1(c120ecfe25c3c50bc51bc7d5a9ef1c8ca6591240) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "strider.18",   0x00000, 0x20000, CRC(4386bc80) SHA1(fb2b261995aeacfa13e7ee40b1a973dfb178f015) )
	ROM_LOAD( "strider.19",   0x20000, 0x20000, CRC(444536d7) SHA1(a14f5de2f6b5b29ae5161dca1f8c08c566301a91) )
ROM_END

ROM_START( striderj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "sthj23.bin",   0x00000, 0x080000, CRC(046e7b12) SHA1(a5761f730f6844a7e93556a6aeae76240a99540c) )
	ROM_LOAD16_WORD_SWAP( "strider.32",   0x80000, 0x80000, CRC(9b3cfc08) SHA1(a7d7f270a097437affa845d80bed82a1fa874878) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "strider.06",   0x000000, 0x80000, CRC(4eee9aea) SHA1(5e619fd5f3f1181e32a8fd9dbb4661d74ff8a484) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.08",   0x000002, 0x80000, CRC(2d7f21e4) SHA1(593cec513de40ff802084d54313bb25a4561e25d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.02",   0x000004, 0x80000, CRC(7705aa46) SHA1(6cbfa30b2852fd117d117beefba434ce41d24c2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.04",   0x000006, 0x80000, CRC(5b18b722) SHA1(cf71c62348ca6b404279e87a6686cb3a842eb381) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.05",   0x200000, 0x80000, CRC(005f000b) SHA1(e6f65af7cc3295be9efaaded352e7ae6320b4133) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.07",   0x200002, 0x80000, CRC(b9441519) SHA1(bb0926dc484dae4f64c5e5a6bce20afdc7aeba55) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.01",   0x200004, 0x80000, CRC(b7d04e8b) SHA1(5c5a079baa694927c33d0e0c23e5ff09d6c9d985) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.03",   0x200006, 0x80000, CRC(6b4713b4) SHA1(759b8b1fc7a5c4b00d74a27c2dd11667db44b09e) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "strider.09",    0x00000, 0x08000, CRC(2ed403bc) SHA1(4ce863ea40d789db5a7cfce91d2c7c720deb9be5) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "strider.18",   0x00000, 0x20000, CRC(4386bc80) SHA1(fb2b261995aeacfa13e7ee40b1a973dfb178f015) )
	ROM_LOAD( "strider.19",   0x20000, 0x20000, CRC(444536d7) SHA1(a14f5de2f6b5b29ae5161dca1f8c08c566301a91) )
ROM_END

ROM_START( stridrja )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sth36.bin",   0x00000, 0x20000, CRC(53c7b006) SHA1(30daa256a32b209b907e5d916a82068017862a01) )
	ROM_LOAD16_BYTE( "sth42.bin",   0x00001, 0x20000, CRC(4037f65f) SHA1(490b9fb15f80772316101ea15e61ab32f42feaec) )
	ROM_LOAD16_BYTE( "sth37.bin",   0x40000, 0x20000, CRC(80e8877d) SHA1(806a62c03007efe6d58fb24dac467d4fc39bb96a) )
	ROM_LOAD16_BYTE( "sth43.bin",   0x40001, 0x20000, CRC(6b3fa466) SHA1(6a3c9bd491eecf864971f7fdf02d01112d5ef7dd) )
	ROM_LOAD16_WORD_SWAP( "strider.32",   0x80000, 0x80000, CRC(9b3cfc08) SHA1(a7d7f270a097437affa845d80bed82a1fa874878) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "strider.06",   0x000000, 0x80000, CRC(4eee9aea) SHA1(5e619fd5f3f1181e32a8fd9dbb4661d74ff8a484) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.08",   0x000002, 0x80000, CRC(2d7f21e4) SHA1(593cec513de40ff802084d54313bb25a4561e25d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.02",   0x000004, 0x80000, CRC(7705aa46) SHA1(6cbfa30b2852fd117d117beefba434ce41d24c2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.04",   0x000006, 0x80000, CRC(5b18b722) SHA1(cf71c62348ca6b404279e87a6686cb3a842eb381) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.05",   0x200000, 0x80000, CRC(005f000b) SHA1(e6f65af7cc3295be9efaaded352e7ae6320b4133) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.07",   0x200002, 0x80000, CRC(b9441519) SHA1(bb0926dc484dae4f64c5e5a6bce20afdc7aeba55) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.01",   0x200004, 0x80000, CRC(b7d04e8b) SHA1(5c5a079baa694927c33d0e0c23e5ff09d6c9d985) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "strider.03",   0x200006, 0x80000, CRC(6b4713b4) SHA1(759b8b1fc7a5c4b00d74a27c2dd11667db44b09e) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "strider.09",    0x00000, 0x08000, CRC(2ed403bc) SHA1(4ce863ea40d789db5a7cfce91d2c7c720deb9be5) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "strider.18",   0x00000, 0x20000, CRC(4386bc80) SHA1(fb2b261995aeacfa13e7ee40b1a973dfb178f015) )
	ROM_LOAD( "strider.19",   0x20000, 0x20000, CRC(444536d7) SHA1(a14f5de2f6b5b29ae5161dca1f8c08c566301a91) )
ROM_END

ROM_START( dw )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "d_wars30.11f", 0x00000, 0x20000, CRC(f9ec6d68) SHA1(02912db2b48f77489b0b841c2a5414bfb49b93f4) )
	ROM_LOAD16_BYTE( "d_wars35.11h", 0x00001, 0x20000, CRC(e41fff2f) SHA1(a960c39c69f97b46d5efcbcd3e2bc652888094c4) )
	ROM_LOAD16_BYTE( "d_wars31.12f", 0x40000, 0x20000, CRC(e3de76ff) SHA1(fdc552312e10c91dd00bfa72e4e686ac356d2244) )
	ROM_LOAD16_BYTE( "d_wars36.12h", 0x40001, 0x20000, CRC(7a13cfbf) SHA1(c6b4d775a2e507fdefbb895cc75bb5bdb442218d) )
	ROM_LOAD16_BYTE( "34.bin",       0x80000, 0x20000, CRC(8f663d00) SHA1(77811783c87c7aee058b8533e34049a01047258a) )
	ROM_LOAD16_BYTE( "40.bin",       0x80001, 0x20000, CRC(1586dbf3) SHA1(d9f03e001effdef021a9ceda512e73a24726fca1) )
	ROM_LOAD16_BYTE( "35.bin",       0xc0000, 0x20000, CRC(9db93d7a) SHA1(f75e3fb5273baef0cd5d8eea26f07d5acaa743ca) )
	ROM_LOAD16_BYTE( "41.bin",       0xc0001, 0x20000, CRC(1aae69a4) SHA1(56e4761818f7857bc7520f2b8de90eabd857c577) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "09.bin",       0x000000, 0x20000, CRC(c3e83c69) SHA1(bd361a39dc6428fea8f56ebbe5cdcc4bf63a51f0) , ROM_SKIP(7) )
	ROMX_LOAD( "01.bin",       0x000001, 0x20000, CRC(187b2886) SHA1(b16121f57926d9fd2c3bc82ae6babf6a2297f140) , ROM_SKIP(7) )
	ROMX_LOAD( "13.bin",       0x000002, 0x20000, CRC(0273d87d) SHA1(7803b04d72eedb4c8b39f63fea458cfef0034813) , ROM_SKIP(7) )
	ROMX_LOAD( "05.bin",       0x000003, 0x20000, CRC(339378b8) SHA1(c3dfe7039e4572b9ef56166346f3cbc6f6ab07c1) , ROM_SKIP(7) )
	ROMX_LOAD( "24.bin",       0x000004, 0x20000, CRC(c6909b6f) SHA1(2828bd6bdc8e3f87a4a37d4e20bdff86cb6850c9) , ROM_SKIP(7) )
	ROMX_LOAD( "17.bin",       0x000005, 0x20000, CRC(2e2f8320) SHA1(7bcb80447d9ce7cc9a38e2506196acd6bf50b49f) , ROM_SKIP(7) )
	ROMX_LOAD( "38.bin",       0x000006, 0x20000, CRC(cd7923ed) SHA1(29187b99847a4b56f2f1763d086b8e7dc5cebed7) , ROM_SKIP(7) )
	ROMX_LOAD( "32.bin",       0x000007, 0x20000, CRC(21a0a453) SHA1(ace38c5943f9f744212cfdb7caa2caa43312e82c) , ROM_SKIP(7) )
	ROMX_LOAD( "10.bin",       0x100000, 0x20000, CRC(ff28f8d0) SHA1(c8c4851816f17a4a0494164f5e8cc910f16669e8) , ROM_SKIP(7) )
	ROMX_LOAD( "02.bin",       0x100001, 0x20000, CRC(cc83c02f) SHA1(915e9d7acec1ba7a2035ae140f576839eba8694f) , ROM_SKIP(7) )
	ROMX_LOAD( "14.bin",       0x100002, 0x20000, CRC(18fb232c) SHA1(c690ca668a56c756c04ef5db4900eb3fd34897e7) , ROM_SKIP(7) )
	ROMX_LOAD( "06.bin",       0x100003, 0x20000, CRC(6f9edd75) SHA1(e8d43c0ec2165e88aefbb5c92048fbcd06fe578b) , ROM_SKIP(7) )
	ROMX_LOAD( "25.bin",       0x100004, 0x20000, CRC(152ea74a) SHA1(c0c56b1bdfa0d7fdea040dbcc6ff871e5957a5b6) , ROM_SKIP(7) )
	ROMX_LOAD( "18.bin",       0x100005, 0x20000, CRC(1833f932) SHA1(81f94d26bdb6758736ca02d7b1772801be4da181) , ROM_SKIP(7) )
	ROMX_LOAD( "39.bin",       0x100006, 0x20000, CRC(bc09b360) SHA1(de2c9a42490db79c8e5fe57b9107f1adbe5dd241) , ROM_SKIP(7) )
	ROMX_LOAD( "33.bin",       0x100007, 0x20000, CRC(89de1533) SHA1(e48312e37c0f98faeec91546acde5daf0da8f6b3) , ROM_SKIP(7) )
	ROMX_LOAD( "11.bin",       0x200000, 0x20000, CRC(29eaf490) SHA1(42fcb67c7014e0ad62cde9e77c79e61268647528) , ROM_SKIP(7) )
	ROMX_LOAD( "03.bin",       0x200001, 0x20000, CRC(7bf51337) SHA1(c21938029641ebcbc484680cf8a57186cdde220f) , ROM_SKIP(7) )
	ROMX_LOAD( "15.bin",       0x200002, 0x20000, CRC(d36cdb91) SHA1(66ab873ce285e857f30294dd1c9b1dda0c6c6b76) , ROM_SKIP(7) )
	ROMX_LOAD( "07.bin",       0x200003, 0x20000, CRC(e04af054) SHA1(f227b8a0a3d8f41e1922d184eaec7a1243c7c3af) , ROM_SKIP(7) )
	ROMX_LOAD( "26.bin",       0x200004, 0x20000, CRC(07fc714b) SHA1(eda97a3c5596ebdfa61bdd01d39647c89b9a2f13) , ROM_SKIP(7) )
	ROMX_LOAD( "19.bin",       0x200005, 0x20000, CRC(7114e5c6) SHA1(2f2925b942af50781857f4fe74e9a58f2cf7b883) , ROM_SKIP(7) )
	ROMX_LOAD( "28.bin",       0x200006, 0x20000, CRC(af62bf07) SHA1(a6e0f598de1fa8a4960e89d655b7514572ed6310) , ROM_SKIP(7) )
	ROMX_LOAD( "21.bin",       0x200007, 0x20000, CRC(523f462a) SHA1(b0fc9e29d6ca44aafb20a62355bde9f4b4cf1e43) , ROM_SKIP(7) )
	ROMX_LOAD( "12.bin",       0x300000, 0x20000, CRC(38652339) SHA1(930a035bbe34c81c26d774d2ab45f53a3a9205fb) , ROM_SKIP(7) )
	ROMX_LOAD( "04.bin",       0x300001, 0x20000, CRC(4951bc0f) SHA1(07f424c147d787321b668d787216733c35e2cff9) , ROM_SKIP(7) )
	ROMX_LOAD( "16.bin",       0x300002, 0x20000, CRC(381608ae) SHA1(666e15e61c7c59df5a97bdc2d77db611d60b3ca8) , ROM_SKIP(7) )
	ROMX_LOAD( "08.bin",       0x300003, 0x20000, CRC(b475d4e9) SHA1(dc5d223bc2a27904e6e38b68507d2e87fbbde158) , ROM_SKIP(7) )
	ROMX_LOAD( "27.bin",       0x300004, 0x20000, CRC(a27e81fa) SHA1(b25854d4a7e52d500c19445badb4cfe745d88d23) , ROM_SKIP(7) )
	ROMX_LOAD( "20.bin",       0x300005, 0x20000, CRC(002796dc) SHA1(2dba0434916dd82c59a66e2f3ce8d3165713c308) , ROM_SKIP(7) )
	ROMX_LOAD( "29.bin",       0x300006, 0x20000, CRC(6b41f82d) SHA1(111af606d8107d377e3af618584a75ed6cfc9bbd) , ROM_SKIP(7) )
	ROMX_LOAD( "22.bin",       0x300007, 0x20000, CRC(52145369) SHA1(bd422f0c51cdd62b69229f926569ad05d430bd57) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 ) /* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "23.bin",        0x00000, 0x08000, CRC(b3b79d4f) SHA1(2b960545741d3b9a53ffbf3ed83030392aa02698) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "d_wars19.12c",  0x20000, 0x20000, CRC(068741db) SHA1(ab48aff639a7ac218b7d5304145e10e92d61fd9f) )
	ROM_LOAD( "d_wars18.11c",  0x00000, 0x20000, CRC(ac6e307d) SHA1(b490ce625bb7ce0904b0fd121fbfbd5252790f7a) )
ROM_END

ROM_START( dwj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "36.bin",       0x00000, 0x20000, CRC(1a516657) SHA1(f5c7c3bfd482eb59221cfd3eec4d47e717b04efa) )
	ROM_LOAD16_BYTE( "42.bin",       0x00001, 0x20000, CRC(12a290a0) SHA1(29fd3f77c497ef8db48121301beab2862ca380b4) )
	ROM_LOAD16_BYTE( "37.bin",       0x40000, 0x20000, CRC(932fc943) SHA1(1bd1c696072e61db791c075fae8936dece73d1d8) )
	ROM_LOAD16_BYTE( "43.bin",       0x40001, 0x20000, CRC(872ad76d) SHA1(77cfb380dd358eb9e65894a026e0718918c5b68f) )
	ROM_LOAD16_BYTE( "34.bin",       0x80000, 0x20000, CRC(8f663d00) SHA1(77811783c87c7aee058b8533e34049a01047258a) )
	ROM_LOAD16_BYTE( "40.bin",       0x80001, 0x20000, CRC(1586dbf3) SHA1(d9f03e001effdef021a9ceda512e73a24726fca1) )
	ROM_LOAD16_BYTE( "35.bin",       0xc0000, 0x20000, CRC(9db93d7a) SHA1(f75e3fb5273baef0cd5d8eea26f07d5acaa743ca) )
	ROM_LOAD16_BYTE( "41.bin",       0xc0001, 0x20000, CRC(1aae69a4) SHA1(56e4761818f7857bc7520f2b8de90eabd857c577) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "09.bin",       0x000000, 0x20000, CRC(c3e83c69) SHA1(bd361a39dc6428fea8f56ebbe5cdcc4bf63a51f0) , ROM_SKIP(7) )
	ROMX_LOAD( "01.bin",       0x000001, 0x20000, CRC(187b2886) SHA1(b16121f57926d9fd2c3bc82ae6babf6a2297f140) , ROM_SKIP(7) )
	ROMX_LOAD( "13.bin",       0x000002, 0x20000, CRC(0273d87d) SHA1(7803b04d72eedb4c8b39f63fea458cfef0034813) , ROM_SKIP(7) )
	ROMX_LOAD( "05.bin",       0x000003, 0x20000, CRC(339378b8) SHA1(c3dfe7039e4572b9ef56166346f3cbc6f6ab07c1) , ROM_SKIP(7) )
	ROMX_LOAD( "24.bin",       0x000004, 0x20000, CRC(c6909b6f) SHA1(2828bd6bdc8e3f87a4a37d4e20bdff86cb6850c9) , ROM_SKIP(7) )
	ROMX_LOAD( "17.bin",       0x000005, 0x20000, CRC(2e2f8320) SHA1(7bcb80447d9ce7cc9a38e2506196acd6bf50b49f) , ROM_SKIP(7) )
	ROMX_LOAD( "38.bin",       0x000006, 0x20000, CRC(cd7923ed) SHA1(29187b99847a4b56f2f1763d086b8e7dc5cebed7) , ROM_SKIP(7) )
	ROMX_LOAD( "32.bin",       0x000007, 0x20000, CRC(21a0a453) SHA1(ace38c5943f9f744212cfdb7caa2caa43312e82c) , ROM_SKIP(7) )
	ROMX_LOAD( "10.bin",       0x100000, 0x20000, CRC(ff28f8d0) SHA1(c8c4851816f17a4a0494164f5e8cc910f16669e8) , ROM_SKIP(7) )
	ROMX_LOAD( "02.bin",       0x100001, 0x20000, CRC(cc83c02f) SHA1(915e9d7acec1ba7a2035ae140f576839eba8694f) , ROM_SKIP(7) )
	ROMX_LOAD( "14.bin",       0x100002, 0x20000, CRC(18fb232c) SHA1(c690ca668a56c756c04ef5db4900eb3fd34897e7) , ROM_SKIP(7) )
	ROMX_LOAD( "06.bin",       0x100003, 0x20000, CRC(6f9edd75) SHA1(e8d43c0ec2165e88aefbb5c92048fbcd06fe578b) , ROM_SKIP(7) )
	ROMX_LOAD( "25.bin",       0x100004, 0x20000, CRC(152ea74a) SHA1(c0c56b1bdfa0d7fdea040dbcc6ff871e5957a5b6) , ROM_SKIP(7) )
	ROMX_LOAD( "18.bin",       0x100005, 0x20000, CRC(1833f932) SHA1(81f94d26bdb6758736ca02d7b1772801be4da181) , ROM_SKIP(7) )
	ROMX_LOAD( "39.bin",       0x100006, 0x20000, CRC(bc09b360) SHA1(de2c9a42490db79c8e5fe57b9107f1adbe5dd241) , ROM_SKIP(7) )
	ROMX_LOAD( "33.bin",       0x100007, 0x20000, CRC(89de1533) SHA1(e48312e37c0f98faeec91546acde5daf0da8f6b3) , ROM_SKIP(7) )
	ROMX_LOAD( "11.bin",       0x200000, 0x20000, CRC(29eaf490) SHA1(42fcb67c7014e0ad62cde9e77c79e61268647528) , ROM_SKIP(7) )
	ROMX_LOAD( "03.bin",       0x200001, 0x20000, CRC(7bf51337) SHA1(c21938029641ebcbc484680cf8a57186cdde220f) , ROM_SKIP(7) )
	ROMX_LOAD( "15.bin",       0x200002, 0x20000, CRC(d36cdb91) SHA1(66ab873ce285e857f30294dd1c9b1dda0c6c6b76) , ROM_SKIP(7) )
	ROMX_LOAD( "07.bin",       0x200003, 0x20000, CRC(e04af054) SHA1(f227b8a0a3d8f41e1922d184eaec7a1243c7c3af) , ROM_SKIP(7) )
	ROMX_LOAD( "26.bin",       0x200004, 0x20000, CRC(07fc714b) SHA1(eda97a3c5596ebdfa61bdd01d39647c89b9a2f13) , ROM_SKIP(7) )
	ROMX_LOAD( "19.bin",       0x200005, 0x20000, CRC(7114e5c6) SHA1(2f2925b942af50781857f4fe74e9a58f2cf7b883) , ROM_SKIP(7) )
	ROMX_LOAD( "28.bin",       0x200006, 0x20000, CRC(af62bf07) SHA1(a6e0f598de1fa8a4960e89d655b7514572ed6310) , ROM_SKIP(7) )
	ROMX_LOAD( "21.bin",       0x200007, 0x20000, CRC(523f462a) SHA1(b0fc9e29d6ca44aafb20a62355bde9f4b4cf1e43) , ROM_SKIP(7) )
	ROMX_LOAD( "12.bin",       0x300000, 0x20000, CRC(38652339) SHA1(930a035bbe34c81c26d774d2ab45f53a3a9205fb) , ROM_SKIP(7) )
	ROMX_LOAD( "04.bin",       0x300001, 0x20000, CRC(4951bc0f) SHA1(07f424c147d787321b668d787216733c35e2cff9) , ROM_SKIP(7) )
	ROMX_LOAD( "16.bin",       0x300002, 0x20000, CRC(381608ae) SHA1(666e15e61c7c59df5a97bdc2d77db611d60b3ca8) , ROM_SKIP(7) )
	ROMX_LOAD( "08.bin",       0x300003, 0x20000, CRC(b475d4e9) SHA1(dc5d223bc2a27904e6e38b68507d2e87fbbde158) , ROM_SKIP(7) )
	ROMX_LOAD( "27.bin",       0x300004, 0x20000, CRC(a27e81fa) SHA1(b25854d4a7e52d500c19445badb4cfe745d88d23) , ROM_SKIP(7) )
	ROMX_LOAD( "20.bin",       0x300005, 0x20000, CRC(002796dc) SHA1(2dba0434916dd82c59a66e2f3ce8d3165713c308) , ROM_SKIP(7) )
	ROMX_LOAD( "29.bin",       0x300006, 0x20000, CRC(6b41f82d) SHA1(111af606d8107d377e3af618584a75ed6cfc9bbd) , ROM_SKIP(7) )
	ROMX_LOAD( "22.bin",       0x300007, 0x20000, CRC(52145369) SHA1(bd422f0c51cdd62b69229f926569ad05d430bd57) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "23.bin",        0x00000, 0x08000, CRC(b3b79d4f) SHA1(2b960545741d3b9a53ffbf3ed83030392aa02698) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "30.bin",       0x00000, 0x20000, CRC(7e5f6cb4) SHA1(c7b6b7d6dfe5f9f0e1521e7ce990229f480cf68d) )
	ROM_LOAD( "31.bin",       0x20000, 0x20000, CRC(4a30c737) SHA1(426eb90f2edf73eb468c94b4a094da3d46acbab2) )
ROM_END

ROM_START( willow )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "wlu_30.rom",   0x00000, 0x20000, CRC(d604dbb1) SHA1(b5d78871011ff11a67f1a0cad147cd4de8d67f35) )
	ROM_LOAD16_BYTE( "willow-u.35",  0x00001, 0x20000, CRC(7a791e77) SHA1(fe1429588b7eceab1d369abe03f2cad8de727f71) )
	ROM_LOAD16_BYTE( "wlu_31.rom",   0x40000, 0x20000, CRC(0eb48a83) SHA1(28c40c4b5d767f88922cd899e948abf11a85a864) )
	ROM_LOAD16_BYTE( "wlu_36.rom",   0x40001, 0x20000, CRC(36100209) SHA1(63c9338e71dba8b52daffba50b4bca31aaa10d9e) )
	ROM_LOAD16_WORD_SWAP( "wl_32.rom",    0x80000, 0x80000, CRC(dfd9f643) SHA1(9c760c30af593a87e7fd39fb213a4c73c68ca440) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "wl_gfx5.rom",  0x000000, 0x80000, CRC(afa74b73) SHA1(09081926260c76986a13ac5351dddd2ea11d7a10) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx7.rom",  0x000002, 0x80000, CRC(12a0dc0b) SHA1(fea235ce9489f04919daf52f4d3f3bac9b558316) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx1.rom",  0x000004, 0x80000, CRC(c6f2abce) SHA1(ff5fcfe417c43b4747bbe12db6052fdb60f5f0e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx3.rom",  0x000006, 0x80000, CRC(4aa4c6d3) SHA1(7dd6f18f6126c380821a2ca8955439fd6864f4c6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_24.rom",    0x200000, 0x20000, CRC(6f0adee5) SHA1(07b18e51b376001f25173b78e0e816f252400210) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_14.rom",    0x200001, 0x20000, CRC(9cf3027d) SHA1(1e8eb20d51a54f6f756c0ab9395ac38b96e67fb2) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_26.rom",    0x200002, 0x20000, CRC(f09c8ecf) SHA1(b39f83e80af010d6481693d9ec8b1d7e258b531d) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_16.rom",    0x200003, 0x20000, CRC(e35407aa) SHA1(7ddae9cef96839da72488c1fe73268c50e0262ff) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_20.rom",    0x200004, 0x20000, CRC(84992350) SHA1(f0ebd810ce099337cda94222dccce8ab9b3c3281) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_10.rom",    0x200005, 0x20000, CRC(b87b5a36) SHA1(25fb8f9698142473233ee509d4146089920e94e1) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_22.rom",    0x200006, 0x20000, CRC(fd3f89f0) SHA1(51ff95cff56ac78682ea56401b35a0aa63cef8cb) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_12.rom",    0x200007, 0x20000, CRC(7da49d69) SHA1(b0ae7ac4f858ee8d72e6877c4275da7a631e2e4c) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "wl_09.rom",     0x00000, 0x08000, CRC(f6b3d060) SHA1(0ed2e2f64ba53ba2c371b66ab1e52e40b16d8baf) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "wl_18.rom",    0x00000, 0x20000, CRC(bde23d4d) SHA1(d1fee2f99c858dfb07edcd600da491c7b656afe0) )
	ROM_LOAD( "wl_19.rom",    0x20000, 0x20000, CRC(683898f5) SHA1(316a77b663d78c8b9ff6d85756cb05aaaeef4003) )
ROM_END

ROM_START( willowj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "wl36.bin",     0x00000, 0x20000, CRC(2b0d7cbc) SHA1(58172b4fdf856efa8d77abbde76738de2424f712) )
	ROM_LOAD16_BYTE( "wl42.bin",     0x00001, 0x20000, CRC(1ac39615) SHA1(c9fa6d20418b9bdc5a08df1fb86368b40709280a) )
	ROM_LOAD16_BYTE( "wl37.bin",     0x40000, 0x20000, CRC(30a717fa) SHA1(cb815e9ee2691761925898e3932b502f8f399cb4) )
	ROM_LOAD16_BYTE( "wl43.bin",     0x40001, 0x20000, CRC(d0dddc9e) SHA1(1176b9a43b3355a5ba44e59abde01ee7eaa89c25) )
	ROM_LOAD16_WORD_SWAP( "wl_32.rom",    0x80000, 0x80000, CRC(dfd9f643) SHA1(9c760c30af593a87e7fd39fb213a4c73c68ca440) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "wl_gfx5.rom",  0x000000, 0x80000, CRC(afa74b73) SHA1(09081926260c76986a13ac5351dddd2ea11d7a10) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx7.rom",  0x000002, 0x80000, CRC(12a0dc0b) SHA1(fea235ce9489f04919daf52f4d3f3bac9b558316) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx1.rom",  0x000004, 0x80000, CRC(c6f2abce) SHA1(ff5fcfe417c43b4747bbe12db6052fdb60f5f0e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx3.rom",  0x000006, 0x80000, CRC(4aa4c6d3) SHA1(7dd6f18f6126c380821a2ca8955439fd6864f4c6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_24.rom",    0x200000, 0x20000, CRC(6f0adee5) SHA1(07b18e51b376001f25173b78e0e816f252400210) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_14.rom",    0x200001, 0x20000, CRC(9cf3027d) SHA1(1e8eb20d51a54f6f756c0ab9395ac38b96e67fb2) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_26.rom",    0x200002, 0x20000, CRC(f09c8ecf) SHA1(b39f83e80af010d6481693d9ec8b1d7e258b531d) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_16.rom",    0x200003, 0x20000, CRC(e35407aa) SHA1(7ddae9cef96839da72488c1fe73268c50e0262ff) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_20.rom",    0x200004, 0x20000, CRC(84992350) SHA1(f0ebd810ce099337cda94222dccce8ab9b3c3281) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_10.rom",    0x200005, 0x20000, CRC(b87b5a36) SHA1(25fb8f9698142473233ee509d4146089920e94e1) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_22.rom",    0x200006, 0x20000, CRC(fd3f89f0) SHA1(51ff95cff56ac78682ea56401b35a0aa63cef8cb) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_12.rom",    0x200007, 0x20000, CRC(7da49d69) SHA1(b0ae7ac4f858ee8d72e6877c4275da7a631e2e4c) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "wl_09.rom",     0x00000, 0x08000, CRC(f6b3d060) SHA1(0ed2e2f64ba53ba2c371b66ab1e52e40b16d8baf) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "wl_18.rom",    0x00000, 0x20000, CRC(bde23d4d) SHA1(d1fee2f99c858dfb07edcd600da491c7b656afe0) )
	ROM_LOAD( "wl_19.rom",    0x20000, 0x20000, CRC(683898f5) SHA1(316a77b663d78c8b9ff6d85756cb05aaaeef4003) )
ROM_END

ROM_START( willowje )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "wlu_30.rom",   0x00000, 0x20000, CRC(d604dbb1) SHA1(b5d78871011ff11a67f1a0cad147cd4de8d67f35) )
	ROM_LOAD16_BYTE( "wlu_35.rom",   0x00001, 0x20000, CRC(daee72fe) SHA1(2ec62f44394fac2887821881f56b6f24d05234b3) )
	ROM_LOAD16_BYTE( "wlu_31.rom",   0x40000, 0x20000, CRC(0eb48a83) SHA1(28c40c4b5d767f88922cd899e948abf11a85a864) )
	ROM_LOAD16_BYTE( "wlu_36.rom",   0x40001, 0x20000, CRC(36100209) SHA1(63c9338e71dba8b52daffba50b4bca31aaa10d9e) )
	ROM_LOAD16_WORD_SWAP( "wl_32.rom",    0x80000, 0x80000, CRC(dfd9f643) SHA1(9c760c30af593a87e7fd39fb213a4c73c68ca440) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "wl_gfx5.rom",  0x000000, 0x80000, CRC(afa74b73) SHA1(09081926260c76986a13ac5351dddd2ea11d7a10) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx7.rom",  0x000002, 0x80000, CRC(12a0dc0b) SHA1(fea235ce9489f04919daf52f4d3f3bac9b558316) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx1.rom",  0x000004, 0x80000, CRC(c6f2abce) SHA1(ff5fcfe417c43b4747bbe12db6052fdb60f5f0e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_gfx3.rom",  0x000006, 0x80000, CRC(4aa4c6d3) SHA1(7dd6f18f6126c380821a2ca8955439fd6864f4c6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "wl_24.rom",    0x200000, 0x20000, CRC(6f0adee5) SHA1(07b18e51b376001f25173b78e0e816f252400210) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_14.rom",    0x200001, 0x20000, CRC(9cf3027d) SHA1(1e8eb20d51a54f6f756c0ab9395ac38b96e67fb2) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_26.rom",    0x200002, 0x20000, CRC(f09c8ecf) SHA1(b39f83e80af010d6481693d9ec8b1d7e258b531d) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_16.rom",    0x200003, 0x20000, CRC(e35407aa) SHA1(7ddae9cef96839da72488c1fe73268c50e0262ff) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_20.rom",    0x200004, 0x20000, CRC(84992350) SHA1(f0ebd810ce099337cda94222dccce8ab9b3c3281) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_10.rom",    0x200005, 0x20000, CRC(b87b5a36) SHA1(25fb8f9698142473233ee509d4146089920e94e1) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_22.rom",    0x200006, 0x20000, CRC(fd3f89f0) SHA1(51ff95cff56ac78682ea56401b35a0aa63cef8cb) , ROM_SKIP(7) )
	ROMX_LOAD( "wl_12.rom",    0x200007, 0x20000, CRC(7da49d69) SHA1(b0ae7ac4f858ee8d72e6877c4275da7a631e2e4c) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "wl_09.rom",     0x00000, 0x08000, CRC(f6b3d060) SHA1(0ed2e2f64ba53ba2c371b66ab1e52e40b16d8baf) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "wl_18.rom",    0x00000, 0x20000, CRC(bde23d4d) SHA1(d1fee2f99c858dfb07edcd600da491c7b656afe0) )
	ROM_LOAD( "wl_19.rom",    0x20000, 0x20000, CRC(683898f5) SHA1(316a77b663d78c8b9ff6d85756cb05aaaeef4003) )
ROM_END

ROM_START( unsquad )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "unsquad.30",   0x00000, 0x20000, CRC(24d8f88d) SHA1(9c39aa1140e92307d6d9c0ca198003282bf78c78) )
	ROM_LOAD16_BYTE( "unsquad.35",   0x00001, 0x20000, CRC(8b954b59) SHA1(33114f1417d48f60c6da3e14a094be7c0f0fd979) )
	ROM_LOAD16_BYTE( "unsquad.31",   0x40000, 0x20000, CRC(33e9694b) SHA1(90db3052ac2ff859ede8473dd13e0f5be148590c) )
	ROM_LOAD16_BYTE( "unsquad.36",   0x40001, 0x20000, CRC(7cc8fb9e) SHA1(f70118d1a368fd4795d9953c55d283305d1f9630) )
	ROM_LOAD16_WORD_SWAP( "unsquad.32",   0x80000, 0x80000, CRC(ae1d7fb0) SHA1(bb51e77574db5e2d807c4ca8e85a5d9661f5d3b3) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "unsquad.05",   0x000000, 0x80000, CRC(bf4575d8) SHA1(1b268e1698be8ff9c16f80f7b9081b6be9f72601) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "unsquad.07",   0x000002, 0x80000, CRC(a02945f4) SHA1(ff35cdbd6c1e43b16a906f68e416559cb3d5746b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "unsquad.01",   0x000004, 0x80000, CRC(5965ca8d) SHA1(49abf80fc012a7f73306139a2871aeac7fd6a3d0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "unsquad.03",   0x000006, 0x80000, CRC(ac6db17d) SHA1(78eef9ba6a392859f70467f6d7cb5aa91964abed) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "unsquad.09",    0x00000, 0x08000, CRC(f3dd1367) SHA1(09eef72e862ac6b1a5cce5a45938b45bf4e456ad) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "unsquad.18",   0x00000, 0x20000, CRC(584b43a9) SHA1(7820815c8c67d484baf2fdad7e55d8c14b98b860) )
ROM_END

ROM_START( area88 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "ar36.bin",     0x00000, 0x20000, CRC(65030392) SHA1(d9dea5cfde28345716b0e519ee033c475be0454b) )
	ROM_LOAD16_BYTE( "ar42.bin",     0x00001, 0x20000, CRC(c48170de) SHA1(e968522dbdd217dd8e4cd6aaeaef801c63488c1d) )
	ROM_LOAD16_BYTE( "unsquad.31",   0x40000, 0x20000, CRC(33e9694b) SHA1(90db3052ac2ff859ede8473dd13e0f5be148590c) )
	ROM_LOAD16_BYTE( "unsquad.36",   0x40001, 0x20000, CRC(7cc8fb9e) SHA1(f70118d1a368fd4795d9953c55d283305d1f9630) )
	ROM_LOAD16_WORD_SWAP( "unsquad.32",   0x80000, 0x80000, CRC(ae1d7fb0) SHA1(bb51e77574db5e2d807c4ca8e85a5d9661f5d3b3) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "unsquad.05",   0x000000, 0x80000, CRC(bf4575d8) SHA1(1b268e1698be8ff9c16f80f7b9081b6be9f72601) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "unsquad.07",   0x000002, 0x80000, CRC(a02945f4) SHA1(ff35cdbd6c1e43b16a906f68e416559cb3d5746b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "unsquad.01",   0x000004, 0x80000, CRC(5965ca8d) SHA1(49abf80fc012a7f73306139a2871aeac7fd6a3d0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "unsquad.03",   0x000006, 0x80000, CRC(ac6db17d) SHA1(78eef9ba6a392859f70467f6d7cb5aa91964abed) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "unsquad.09",    0x00000, 0x08000, CRC(f3dd1367) SHA1(09eef72e862ac6b1a5cce5a45938b45bf4e456ad) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "unsquad.18",   0x00000, 0x20000, CRC(584b43a9) SHA1(7820815c8c67d484baf2fdad7e55d8c14b98b860) )
ROM_END

ROM_START( ffight )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "ff30-36.bin",  0x00000, 0x20000, CRC(f9a5ce83) SHA1(0756ae576a1f6d5b8b22f8630dca40ef38567ea6) )
	ROM_LOAD16_BYTE( "ff35-42.bin",  0x00001, 0x20000, CRC(65f11215) SHA1(5045a467f3e228c02b4a355b52f58263ffa90113) )
	ROM_LOAD16_BYTE( "ff31-37.bin",  0x40000, 0x20000, CRC(e1033784) SHA1(38f44434c8befd623953ae23d6e5ff4e201d6627) )
	ROM_LOAD16_BYTE( "ff36-43.bin",  0x40001, 0x20000, CRC(995e968a) SHA1(de16873d1639ac1738be0937270b108a9914f263) )
	ROM_LOAD16_WORD_SWAP( "ff32-32m.bin", 0x80000, 0x80000, CRC(c747696e) SHA1(d3362dadded31ccb7eaf71ef282d698d18edd722) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ff05-05m.bin", 0x000000, 0x80000, CRC(9c284108) SHA1(7868f5801347340867720255f8380548ad1a65a7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff07-07m.bin", 0x000002, 0x80000, CRC(a7584dfb) SHA1(f7b00a3ca8cb85264ab293089f9f540a8292b49c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff01-01m.bin", 0x000004, 0x80000, CRC(0b605e44) SHA1(5ce16af72858a57aefbf6efed820c2c51935882a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff03-03m.bin", 0x000006, 0x80000, CRC(52291cd2) SHA1(df5f3d3aa96a7a33ff22f2a31382942c4c4f1111) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ff09-09.bin",   0x00000, 0x08000, CRC(b8367eb5) SHA1(ec3db29fdd6200e9a8f4f8073a7e34aef731354f) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ff18-18.bin",  0x00000, 0x20000, CRC(375c66e7) SHA1(36189e23209ce4ae5d9cbabd1574540d0591e7b3) )
	ROM_LOAD( "ff19-19.bin",  0x20000, 0x20000, CRC(1ef137f9) SHA1(974b5e72aa28b87ebfa7438efbdfeda769dedf5e) )
ROM_END

ROM_START( ffightu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "36",           0x00000, 0x20000, CRC(e2a48af9) SHA1(11e06f95bdf575af396dded2b84d858f6c7388f1) )
	ROM_LOAD16_BYTE( "42",           0x00001, 0x20000, CRC(f4bb480e) SHA1(32114df1d2f4f98a4a2280a330c7b6af8ab4d862) )
	ROM_LOAD16_BYTE( "37",           0x40000, 0x20000, CRC(c371c667) SHA1(633977c91a8ff09b7fe83128eced7c4dee9aee1d) )
	ROM_LOAD16_BYTE( "43",           0x40001, 0x20000, CRC(2f5771f9) SHA1(fb532402bc00b5619a23dfa7e4525f1717020303) )
	ROM_LOAD16_WORD_SWAP( "ff32-32m.bin", 0x80000, 0x80000, CRC(c747696e) SHA1(d3362dadded31ccb7eaf71ef282d698d18edd722) )

	/* Note: the gfx ROMs were missing from this set. I used the ones from */
	/* the World version, assuming that if the scantily clad woman shouldn't */
	/* be seen in Europe, it shouldn't be seen in the USA as well. */
	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ff05-05m.bin", 0x000000, 0x80000, CRC(9c284108) SHA1(7868f5801347340867720255f8380548ad1a65a7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff07-07m.bin", 0x000002, 0x80000, CRC(a7584dfb) SHA1(f7b00a3ca8cb85264ab293089f9f540a8292b49c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff01-01m.bin", 0x000004, 0x80000, CRC(0b605e44) SHA1(5ce16af72858a57aefbf6efed820c2c51935882a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff03-03m.bin", 0x000006, 0x80000, CRC(52291cd2) SHA1(df5f3d3aa96a7a33ff22f2a31382942c4c4f1111) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ff09-09.bin",   0x00000, 0x08000, CRC(b8367eb5) SHA1(ec3db29fdd6200e9a8f4f8073a7e34aef731354f) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ff18-18.bin",  0x00000, 0x20000, CRC(375c66e7) SHA1(36189e23209ce4ae5d9cbabd1574540d0591e7b3) )
	ROM_LOAD( "ff19-19.bin",  0x20000, 0x20000, CRC(1ef137f9) SHA1(974b5e72aa28b87ebfa7438efbdfeda769dedf5e) )
ROM_END

ROM_START( ffightj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "ff30-36.bin",  0x00000, 0x20000, CRC(f9a5ce83) SHA1(0756ae576a1f6d5b8b22f8630dca40ef38567ea6) )
	ROM_LOAD16_BYTE( "ff35-42.bin",  0x00001, 0x20000, CRC(65f11215) SHA1(5045a467f3e228c02b4a355b52f58263ffa90113) )
	ROM_LOAD16_BYTE( "ff31-37.bin",  0x40000, 0x20000, CRC(e1033784) SHA1(38f44434c8befd623953ae23d6e5ff4e201d6627) )
	ROM_LOAD16_BYTE( "ff43.bin",     0x40001, 0x20000, CRC(b6dee1c3) SHA1(3a85312a2e9d8e06259b73d91ccb5e66a6bad62d) )
	ROM_LOAD16_WORD_SWAP( "ff32-32m.bin", 0x80000, 0x80000, CRC(c747696e) SHA1(d3362dadded31ccb7eaf71ef282d698d18edd722) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ff09.bin",     0x000000, 0x20000, CRC(5b116d0d) SHA1(a24e829fdfa043bd27b508d7cc0788ad80fd180e) , ROM_SKIP(7) )
	ROMX_LOAD( "ff01.bin",     0x000001, 0x20000, CRC(815b1797) SHA1(549e5eefc8f607fec1c954ba715ff21b8e44a5aa) , ROM_SKIP(7) )
	ROMX_LOAD( "ff13.bin",     0x000002, 0x20000, CRC(8721a7da) SHA1(39b2b324fd7810342503f23695e423f364a6294d) , ROM_SKIP(7) )
	ROMX_LOAD( "ff05.bin",     0x000003, 0x20000, CRC(d0fcd4b5) SHA1(97ebcbead0cca7e425143c500c433dbcf9cadcc2) , ROM_SKIP(7) )
	ROMX_LOAD( "ff24.bin",     0x000004, 0x20000, CRC(a1ab607a) SHA1(56784c028b82d9e2affd9610f56fde57063e4c28) , ROM_SKIP(7) )
	ROMX_LOAD( "ff17.bin",     0x000005, 0x20000, CRC(2dc18cf4) SHA1(5e3bd895600cd30d561a75a2fcb6cc8bc84f4bd1) , ROM_SKIP(7) )
	ROMX_LOAD( "ff38.bin",     0x000006, 0x20000, CRC(6535a57f) SHA1(f4da9ec13cad7e3287e34dcceb0eb2d20107bad6) , ROM_SKIP(7) )
	ROMX_LOAD( "ff32.bin",     0x000007, 0x20000, CRC(c8bc4a57) SHA1(3eaf2b4e910fe1f79154020122d786d23a2e594a) , ROM_SKIP(7) )
	ROMX_LOAD( "ff10.bin",     0x100000, 0x20000, CRC(624a924a) SHA1(48fd0498f9ed54003bf6578fbcbc8b7e90a195eb) , ROM_SKIP(7) )
	ROMX_LOAD( "ff02.bin",     0x100001, 0x20000, CRC(5d91f694) SHA1(e0ea9ec82dec985d8bf5e7cebf5fe3d8ef7557b1) , ROM_SKIP(7) )
	ROMX_LOAD( "ff14.bin",     0x100002, 0x20000, CRC(0a2e9101) SHA1(6c8d550d2066cd53355ccf14ac1fd35914982094) , ROM_SKIP(7) )
	ROMX_LOAD( "ff06.bin",     0x100003, 0x20000, CRC(1c18f042) SHA1(f708296570fecad82a76dc59744873a2f5568ea1) , ROM_SKIP(7) )
	ROMX_LOAD( "ff25.bin",     0x100004, 0x20000, CRC(6e8181ea) SHA1(2c32bc0364650ee6ca0d24754a7a3401295ffcd5) , ROM_SKIP(7) )
	ROMX_LOAD( "ff18.bin",     0x100005, 0x20000, CRC(b19ede59) SHA1(7e79ad9f17b36e042d774bef3bbb44018332ca01) , ROM_SKIP(7) )
	ROMX_LOAD( "ff39.bin",     0x100006, 0x20000, CRC(9416b477) SHA1(f2310dfcfe960e8b822c07849b594d54dfc2b2ca) , ROM_SKIP(7) )
	ROMX_LOAD( "ff33.bin",     0x100007, 0x20000, CRC(7369fa07) SHA1(3b2750fe33729395217c96909b4b6c5f3d6e9943) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ff09-09.bin",   0x00000, 0x08000, CRC(b8367eb5) SHA1(ec3db29fdd6200e9a8f4f8073a7e34aef731354f) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ff18-18.bin",  0x00000, 0x20000, CRC(375c66e7) SHA1(36189e23209ce4ae5d9cbabd1574540d0591e7b3) )
	ROM_LOAD( "ff19-19.bin",  0x20000, 0x20000, CRC(1ef137f9) SHA1(974b5e72aa28b87ebfa7438efbdfeda769dedf5e) )
ROM_END

ROM_START( ffightj1 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "ff30-36.rom",  0x00000, 0x20000, CRC(088ed1c9) SHA1(7b96cd45f4d3d2c0fe94904882652814b7790869) )
	ROM_LOAD16_BYTE( "ff35-42.rom",  0x00001, 0x20000, CRC(c4c491e6) SHA1(d0e34d7b94f67c33615710ea721da8fefe832e3a) )
	ROM_LOAD16_BYTE( "ff31-37.rom",  0x40000, 0x20000, CRC(708557ff) SHA1(89e56bfd9486623a18fdbf984a72bb52054ca0e6) )
	ROM_LOAD16_BYTE( "ff36-43.rom",  0x40001, 0x20000, CRC(c004004a) SHA1(10ccf27972591f65645a8dd2bb65989176ac07d5) )
	ROM_LOAD16_WORD_SWAP( "ff32-32m.bin", 0x80000, 0x80000, CRC(c747696e) SHA1(d3362dadded31ccb7eaf71ef282d698d18edd722) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ff09.bin",     0x000000, 0x20000, CRC(5b116d0d) SHA1(a24e829fdfa043bd27b508d7cc0788ad80fd180e) , ROM_SKIP(7) )
	ROMX_LOAD( "ff01.bin",     0x000001, 0x20000, CRC(815b1797) SHA1(549e5eefc8f607fec1c954ba715ff21b8e44a5aa) , ROM_SKIP(7) )
	ROMX_LOAD( "ff13.bin",     0x000002, 0x20000, CRC(8721a7da) SHA1(39b2b324fd7810342503f23695e423f364a6294d) , ROM_SKIP(7) )
	ROMX_LOAD( "ff05.bin",     0x000003, 0x20000, CRC(d0fcd4b5) SHA1(97ebcbead0cca7e425143c500c433dbcf9cadcc2) , ROM_SKIP(7) )
	ROMX_LOAD( "ff24.bin",     0x000004, 0x20000, CRC(a1ab607a) SHA1(56784c028b82d9e2affd9610f56fde57063e4c28) , ROM_SKIP(7) )
	ROMX_LOAD( "ff17.bin",     0x000005, 0x20000, CRC(2dc18cf4) SHA1(5e3bd895600cd30d561a75a2fcb6cc8bc84f4bd1) , ROM_SKIP(7) )
	ROMX_LOAD( "ff38.bin",     0x000006, 0x20000, CRC(6535a57f) SHA1(f4da9ec13cad7e3287e34dcceb0eb2d20107bad6) , ROM_SKIP(7) )
	ROMX_LOAD( "ff32.bin",     0x000007, 0x20000, CRC(c8bc4a57) SHA1(3eaf2b4e910fe1f79154020122d786d23a2e594a) , ROM_SKIP(7) )
	ROMX_LOAD( "ff10.bin",     0x100000, 0x20000, CRC(624a924a) SHA1(48fd0498f9ed54003bf6578fbcbc8b7e90a195eb) , ROM_SKIP(7) )
	ROMX_LOAD( "ff02.bin",     0x100001, 0x20000, CRC(5d91f694) SHA1(e0ea9ec82dec985d8bf5e7cebf5fe3d8ef7557b1) , ROM_SKIP(7) )
	ROMX_LOAD( "ff14.bin",     0x100002, 0x20000, CRC(0a2e9101) SHA1(6c8d550d2066cd53355ccf14ac1fd35914982094) , ROM_SKIP(7) )
	ROMX_LOAD( "ff06.bin",     0x100003, 0x20000, CRC(1c18f042) SHA1(f708296570fecad82a76dc59744873a2f5568ea1) , ROM_SKIP(7) )
	ROMX_LOAD( "ff25.bin",     0x100004, 0x20000, CRC(6e8181ea) SHA1(2c32bc0364650ee6ca0d24754a7a3401295ffcd5) , ROM_SKIP(7) )
	ROMX_LOAD( "ff18.bin",     0x100005, 0x20000, CRC(b19ede59) SHA1(7e79ad9f17b36e042d774bef3bbb44018332ca01) , ROM_SKIP(7) )
	ROMX_LOAD( "ff39.bin",     0x100006, 0x20000, CRC(9416b477) SHA1(f2310dfcfe960e8b822c07849b594d54dfc2b2ca) , ROM_SKIP(7) )
	ROMX_LOAD( "ff33.bin",     0x100007, 0x20000, CRC(7369fa07) SHA1(3b2750fe33729395217c96909b4b6c5f3d6e9943) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ff09-09.bin",   0x00000, 0x08000, CRC(b8367eb5) SHA1(ec3db29fdd6200e9a8f4f8073a7e34aef731354f) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ff18-18.bin",  0x00000, 0x20000, CRC(375c66e7) SHA1(36189e23209ce4ae5d9cbabd1574540d0591e7b3) )
	ROM_LOAD( "ff19-19.bin",  0x20000, 0x20000, CRC(1ef137f9) SHA1(974b5e72aa28b87ebfa7438efbdfeda769dedf5e) )
ROM_END
                                        
ROM_START( ffightae )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "ff-23m.8h", 0x00000, 0x80000, CRC(86DEF74F) SHA1(5206cc13bfe40fb4f9c3677629aee89099623ee6) )
	ROM_LOAD16_WORD_SWAP( "ff-22m.7h", 0x80000, 0x80000, CRC(CBDD8689) SHA1(a75918ee837dfccdd4fd02b716928a2de2003103) )
	
	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ff-5m.7a", 0x000000, 0x80000, CRC(91A909BD) SHA1(09621cb33a9c26798b1bba186dceb02e5f126e1a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff-7m.9a", 0x000002, 0x80000, CRC(89F8B4CD) SHA1(c169c445686d3c79eae2dc42460b8194c491ccb0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff-1m.3a", 0x000004, 0x80000, CRC(D5469303) SHA1(0c1e33a87eb3ef79e6a5ba80753eb495284e666c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ff-3m.5a", 0x000006, 0x80000, CRC(0C6302BF) SHA1(03ee13a67a8a3b92fac462623ace752d77b9e9f1) , ROM_GROUPWORD | ROM_SKIP(6) )
	
	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ff09-09.bin",   0x00000, 0x08000, CRC(b8367eb5) SHA1(ec3db29fdd6200e9a8f4f8073a7e34aef731354f) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ff18-18.bin",  0x00000, 0x20000, CRC(375c66e7) SHA1(36189e23209ce4ae5d9cbabd1574540d0591e7b3) )
	ROM_LOAD( "ff19-19.bin",  0x20000, 0x20000, CRC(1ef137f9) SHA1(974b5e72aa28b87ebfa7438efbdfeda769dedf5e) )
ROM_END                                       

ROM_START( 1941 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "41e_30.rom",   0x00000, 0x20000, CRC(9deb1e75) SHA1(68d9f91bef6a5c9e1bcbf286629aed6b37b4acb9) )
	ROM_LOAD16_BYTE( "41e_35.rom",   0x00001, 0x20000, CRC(d63942b3) SHA1(b4bc7d06dcefbc075d316f2d31abbd4c7a99dbae) )
	ROM_LOAD16_BYTE( "41e_31.rom",   0x40000, 0x20000, CRC(df201112) SHA1(d84f63bffeb9255cbabc02f23d7511f9b3c6a96c) )
	ROM_LOAD16_BYTE( "41e_36.rom",   0x40001, 0x20000, CRC(816a818f) SHA1(3e491a30352b71ddd775142f3a80cdde480b669f) )
	ROM_LOAD16_WORD_SWAP( "41_32.rom",    0x80000, 0x80000, CRC(4e9648ca) SHA1(d8e67e6e3a6dc79053e4f56cfd83431385ea7611) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "41_gfx5.rom",  0x000000, 0x80000, CRC(01d1cb11) SHA1(621e5377d1aaa9f7270d85bea1bdeef6721cdd05) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "41_gfx7.rom",  0x000002, 0x80000, CRC(aeaa3509) SHA1(6124ef06d9dfdd879181856bd49853f1800c3b87) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "41_gfx1.rom",  0x000004, 0x80000, CRC(ff77985a) SHA1(7e08df3a829bf9617470a46c79b713d4d9ebacae) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "41_gfx3.rom",  0x000006, 0x80000, CRC(983be58f) SHA1(83a4decdd775f859240771269b8af3a5981b244c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "41_09.rom",     0x00000, 0x08000, CRC(0f9d8527) SHA1(3a00dd5772f38081fde11d8d61ba467379e2a636) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "41_18.rom",    0x00000, 0x20000, CRC(d1f15aeb) SHA1(88089383f2d54fc97026a67f067d448eee5bd0c2) )
	ROM_LOAD( "41_19.rom",    0x20000, 0x20000, CRC(15aec3a6) SHA1(8153c03aba005bab62bf0e8b3d15ec1c346326fd) )
ROM_END

ROM_START( 1941j )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "4136.bin",     0x00000, 0x20000, CRC(7fbd42ab) SHA1(4e52a599e3099bf3cccabb89152c69f216fde79e) )
	ROM_LOAD16_BYTE( "4142.bin",     0x00001, 0x20000, CRC(c7781f89) SHA1(7e99c433de0c903791ae153a3cc8632042b0a90d) )
	ROM_LOAD16_BYTE( "4137.bin",     0x40000, 0x20000, CRC(c6464b0b) SHA1(abef422d891d32334a858d49599f1ef7cf0db45d) )
	ROM_LOAD16_BYTE( "4143.bin",     0x40001, 0x20000, CRC(440fc0b5) SHA1(e725535533c25a2c80a45a2200bbfd0dcda5ed97) )
	ROM_LOAD16_WORD_SWAP( "41_32.rom",    0x80000, 0x80000, CRC(4e9648ca) SHA1(d8e67e6e3a6dc79053e4f56cfd83431385ea7611) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "41_gfx5.rom",  0x000000, 0x80000, CRC(01d1cb11) SHA1(621e5377d1aaa9f7270d85bea1bdeef6721cdd05) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "41_gfx7.rom",  0x000002, 0x80000, CRC(aeaa3509) SHA1(6124ef06d9dfdd879181856bd49853f1800c3b87) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "41_gfx1.rom",  0x000004, 0x80000, CRC(ff77985a) SHA1(7e08df3a829bf9617470a46c79b713d4d9ebacae) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "41_gfx3.rom",  0x000006, 0x80000, CRC(983be58f) SHA1(83a4decdd775f859240771269b8af3a5981b244c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "41_09.rom",     0x00000, 0x08000, CRC(0f9d8527) SHA1(3a00dd5772f38081fde11d8d61ba467379e2a636) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "41_18.rom",    0x00000, 0x20000, CRC(d1f15aeb) SHA1(88089383f2d54fc97026a67f067d448eee5bd0c2) )
	ROM_LOAD( "41_19.rom",    0x20000, 0x20000, CRC(15aec3a6) SHA1(8153c03aba005bab62bf0e8b3d15ec1c346326fd) )
ROM_END

ROM_START( mercs )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "so2_30e.rom",  0x00000, 0x20000, CRC(e17f9bf7) SHA1(f44bb378de428b429c97a21f74829182d3187ace) )
	ROM_LOAD16_BYTE( "so2_35e.rom",  0x00001, 0x20000, CRC(78e63575) SHA1(5776de0daaaedd0dec2cec8d088a0fd8bb3d4dbe) )
	ROM_LOAD16_BYTE( "so2_31e.rom",  0x40000, 0x20000, CRC(51204d36) SHA1(af288fc369d092f38ea73be967705aacade06f28) )
	ROM_LOAD16_BYTE( "so2_36e.rom",  0x40001, 0x20000, CRC(9cfba8b4) SHA1(df8ee5e3a68f056f68f096c46fdb548f63d29446) )
	ROM_LOAD16_WORD_SWAP( "so2_32.rom",   0x80000, 0x80000, CRC(2eb5cf0c) SHA1(e0d765fb6957d156ffd40cabf51ba6098cbbeb19) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "so2_gfx6.rom", 0x000000, 0x80000, CRC(aa6102af) SHA1(4a45f3547a3640f256e5e20bfd72784f880f03f5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx8.rom", 0x000002, 0x80000, CRC(839e6869) SHA1(7741141a9f1b1e2956edc1d11f9cc3974390c4ed) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx2.rom", 0x000004, 0x80000, CRC(597c2875) SHA1(440bd04db2c121a6976e5e1027071d28812942d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx4.rom", 0x000006, 0x80000, CRC(912a9ca0) SHA1(b226a4a388e57e23d7a7559773ebee434125a2e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_24.rom",   0x200000, 0x20000, CRC(3f254efe) SHA1(5db36eb98a6d3c7acccb561d92c1988d1330cbbf) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_14.rom",   0x200001, 0x20000, CRC(f5a8905e) SHA1(fada8b635d490c06b75711ed505a025bb0aa4454) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_26.rom",   0x200002, 0x20000, CRC(f3aa5a4a) SHA1(bcb3396de5524fffd4110bfbeeeca1c936990eb3) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_16.rom",   0x200003, 0x20000, CRC(b43cd1a8) SHA1(01c2bb802469848a172968802a674c0045a8b8dc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_20.rom",   0x200004, 0x20000, CRC(8ca751a3) SHA1(e93bbe7311f14e7e3cbfb42b83fd7fee4bb9cefc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_10.rom",   0x200005, 0x20000, CRC(e9f569fd) SHA1(39ae9eacdf1f35ef90d131444c37958d7aaf7238) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_22.rom",   0x200006, 0x20000, CRC(fce9a377) SHA1(5de5f696f63326f2cb4c38bcb05e07bcf2246071) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_12.rom",   0x200007, 0x20000, CRC(b7df8a06) SHA1(b42cb0d3f55a1e8fe8afbbd9aeae50074cdc5f08) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "so2_09.rom",    0x00000, 0x08000, CRC(d09d7c7a) SHA1(8e8532be08818c855d9c3ce45716eb07cfab5767) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "so2_18.rom",   0x00000, 0x20000, CRC(bbea1643) SHA1(d43d68a120550067bf0b181f88687ad230cd7908) )
	ROM_LOAD( "so2_19.rom",   0x20000, 0x20000, CRC(ac58aa71) SHA1(93102272e358bc49d3936302efdc5bb68df84d68) )
ROM_END

ROM_START( mercsu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "so2_30e.rom",  0x00000, 0x20000, CRC(e17f9bf7) SHA1(f44bb378de428b429c97a21f74829182d3187ace) )
	ROM_LOAD16_BYTE( "s02-35",       0x00001, 0x20000, CRC(4477df61) SHA1(e9b42357c7073c098e8fde7e7d0e4a6e3062fd0d) )
	ROM_LOAD16_BYTE( "so2_31e.rom",  0x40000, 0x20000, CRC(51204d36) SHA1(af288fc369d092f38ea73be967705aacade06f28) )
	ROM_LOAD16_BYTE( "so2_36e.rom",  0x40001, 0x20000, CRC(9cfba8b4) SHA1(df8ee5e3a68f056f68f096c46fdb548f63d29446) )
	ROM_LOAD16_WORD_SWAP( "so2_32.rom",   0x80000, 0x80000, CRC(2eb5cf0c) SHA1(e0d765fb6957d156ffd40cabf51ba6098cbbeb19) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "so2_gfx6.rom", 0x000000, 0x80000, CRC(aa6102af) SHA1(4a45f3547a3640f256e5e20bfd72784f880f03f5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx8.rom", 0x000002, 0x80000, CRC(839e6869) SHA1(7741141a9f1b1e2956edc1d11f9cc3974390c4ed) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx2.rom", 0x000004, 0x80000, CRC(597c2875) SHA1(440bd04db2c121a6976e5e1027071d28812942d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx4.rom", 0x000006, 0x80000, CRC(912a9ca0) SHA1(b226a4a388e57e23d7a7559773ebee434125a2e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_24.rom",   0x200000, 0x20000, CRC(3f254efe) SHA1(5db36eb98a6d3c7acccb561d92c1988d1330cbbf) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_14.rom",   0x200001, 0x20000, CRC(f5a8905e) SHA1(fada8b635d490c06b75711ed505a025bb0aa4454) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_26.rom",   0x200002, 0x20000, CRC(f3aa5a4a) SHA1(bcb3396de5524fffd4110bfbeeeca1c936990eb3) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_16.rom",   0x200003, 0x20000, CRC(b43cd1a8) SHA1(01c2bb802469848a172968802a674c0045a8b8dc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_20.rom",   0x200004, 0x20000, CRC(8ca751a3) SHA1(e93bbe7311f14e7e3cbfb42b83fd7fee4bb9cefc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_10.rom",   0x200005, 0x20000, CRC(e9f569fd) SHA1(39ae9eacdf1f35ef90d131444c37958d7aaf7238) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_22.rom",   0x200006, 0x20000, CRC(fce9a377) SHA1(5de5f696f63326f2cb4c38bcb05e07bcf2246071) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_12.rom",   0x200007, 0x20000, CRC(b7df8a06) SHA1(b42cb0d3f55a1e8fe8afbbd9aeae50074cdc5f08) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "so2_09.rom",    0x00000, 0x08000, CRC(d09d7c7a) SHA1(8e8532be08818c855d9c3ce45716eb07cfab5767) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "so2_18.rom",   0x00000, 0x20000, CRC(bbea1643) SHA1(d43d68a120550067bf0b181f88687ad230cd7908) )
	ROM_LOAD( "so2_19.rom",   0x20000, 0x20000, CRC(ac58aa71) SHA1(93102272e358bc49d3936302efdc5bb68df84d68) )
ROM_END

ROM_START( mercsua )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "so2.30a",		 0x00000, 0x20000, CRC(e4e725d7) SHA1(b0454dedeb741a7dd4ceb18bac958417ca74a7e6) )
	ROM_LOAD16_BYTE( "so2.35a",      0x00001, 0x20000, CRC(e7843445) SHA1(192c85ced637e05b37ed889246ebb73e792e984b) )
	ROM_LOAD16_BYTE( "so2.31a",		 0x40000, 0x20000, CRC(c0b91dea) SHA1(5c1d086ae09e4f66384a03994b3c5e12d80582ff) )
	ROM_LOAD16_BYTE( "so2.36a",      0x40001, 0x20000, CRC(591edf6c) SHA1(68d77e21fe32e0b95d2fabe40bc1cadd419ab0bd) )
	ROM_LOAD16_WORD_SWAP( "so2_32.rom",   0x80000, 0x80000, CRC(2eb5cf0c) SHA1(e0d765fb6957d156ffd40cabf51ba6098cbbeb19) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "so2_gfx6.rom", 0x000000, 0x80000, CRC(aa6102af) SHA1(4a45f3547a3640f256e5e20bfd72784f880f03f5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx8.rom", 0x000002, 0x80000, CRC(839e6869) SHA1(7741141a9f1b1e2956edc1d11f9cc3974390c4ed) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx2.rom", 0x000004, 0x80000, CRC(597c2875) SHA1(440bd04db2c121a6976e5e1027071d28812942d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx4.rom", 0x000006, 0x80000, CRC(912a9ca0) SHA1(b226a4a388e57e23d7a7559773ebee434125a2e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_24.rom",   0x200000, 0x20000, CRC(3f254efe) SHA1(5db36eb98a6d3c7acccb561d92c1988d1330cbbf) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_14.rom",   0x200001, 0x20000, CRC(f5a8905e) SHA1(fada8b635d490c06b75711ed505a025bb0aa4454) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_26.rom",   0x200002, 0x20000, CRC(f3aa5a4a) SHA1(bcb3396de5524fffd4110bfbeeeca1c936990eb3) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_16.rom",   0x200003, 0x20000, CRC(b43cd1a8) SHA1(01c2bb802469848a172968802a674c0045a8b8dc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_20.rom",   0x200004, 0x20000, CRC(8ca751a3) SHA1(e93bbe7311f14e7e3cbfb42b83fd7fee4bb9cefc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_10.rom",   0x200005, 0x20000, CRC(e9f569fd) SHA1(39ae9eacdf1f35ef90d131444c37958d7aaf7238) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_22.rom",   0x200006, 0x20000, CRC(fce9a377) SHA1(5de5f696f63326f2cb4c38bcb05e07bcf2246071) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_12.rom",   0x200007, 0x20000, CRC(b7df8a06) SHA1(b42cb0d3f55a1e8fe8afbbd9aeae50074cdc5f08) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "so2_09.rom",    0x00000, 0x08000, CRC(d09d7c7a) SHA1(8e8532be08818c855d9c3ce45716eb07cfab5767) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "so2_18.rom",   0x00000, 0x20000, CRC(bbea1643) SHA1(d43d68a120550067bf0b181f88687ad230cd7908) )
	ROM_LOAD( "so2_19.rom",   0x20000, 0x20000, CRC(ac58aa71) SHA1(93102272e358bc49d3936302efdc5bb68df84d68) )
ROM_END

ROM_START( mercsj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "so2_30e.rom",  0x00000, 0x20000, CRC(e17f9bf7) SHA1(f44bb378de428b429c97a21f74829182d3187ace) )
	ROM_LOAD16_BYTE( "so2_42.bin",   0x00001, 0x20000, CRC(2c3884c6) SHA1(98c3e93741d2344fe0a699aacdc5038bdd9007a0) )
	ROM_LOAD16_BYTE( "so2_31e.rom",  0x40000, 0x20000, CRC(51204d36) SHA1(af288fc369d092f38ea73be967705aacade06f28) )
	ROM_LOAD16_BYTE( "so2_36e.rom",  0x40001, 0x20000, CRC(9cfba8b4) SHA1(df8ee5e3a68f056f68f096c46fdb548f63d29446) )
	ROM_LOAD16_WORD_SWAP( "so2_32.rom",   0x80000, 0x80000, CRC(2eb5cf0c) SHA1(e0d765fb6957d156ffd40cabf51ba6098cbbeb19) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "so2_gfx6.rom", 0x000000, 0x80000, CRC(aa6102af) SHA1(4a45f3547a3640f256e5e20bfd72784f880f03f5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx8.rom", 0x000002, 0x80000, CRC(839e6869) SHA1(7741141a9f1b1e2956edc1d11f9cc3974390c4ed) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx2.rom", 0x000004, 0x80000, CRC(597c2875) SHA1(440bd04db2c121a6976e5e1027071d28812942d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_gfx4.rom", 0x000006, 0x80000, CRC(912a9ca0) SHA1(b226a4a388e57e23d7a7559773ebee434125a2e4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "so2_24.rom",   0x200000, 0x20000, CRC(3f254efe) SHA1(5db36eb98a6d3c7acccb561d92c1988d1330cbbf) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_14.rom",   0x200001, 0x20000, CRC(f5a8905e) SHA1(fada8b635d490c06b75711ed505a025bb0aa4454) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_26.rom",   0x200002, 0x20000, CRC(f3aa5a4a) SHA1(bcb3396de5524fffd4110bfbeeeca1c936990eb3) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_16.rom",   0x200003, 0x20000, CRC(b43cd1a8) SHA1(01c2bb802469848a172968802a674c0045a8b8dc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_20.rom",   0x200004, 0x20000, CRC(8ca751a3) SHA1(e93bbe7311f14e7e3cbfb42b83fd7fee4bb9cefc) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_10.rom",   0x200005, 0x20000, CRC(e9f569fd) SHA1(39ae9eacdf1f35ef90d131444c37958d7aaf7238) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_22.rom",   0x200006, 0x20000, CRC(fce9a377) SHA1(5de5f696f63326f2cb4c38bcb05e07bcf2246071) , ROM_SKIP(7) )
	ROMX_LOAD( "so2_12.rom",   0x200007, 0x20000, CRC(b7df8a06) SHA1(b42cb0d3f55a1e8fe8afbbd9aeae50074cdc5f08) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "so2_09.rom",    0x00000, 0x08000, CRC(d09d7c7a) SHA1(8e8532be08818c855d9c3ce45716eb07cfab5767) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "so2_18.rom",   0x00000, 0x20000, CRC(bbea1643) SHA1(d43d68a120550067bf0b181f88687ad230cd7908) )
	ROM_LOAD( "so2_19.rom",   0x20000, 0x20000, CRC(ac58aa71) SHA1(93102272e358bc49d3936302efdc5bb68df84d68) )
ROM_END

ROM_START( mtwins )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "che_30.rom",   0x00000, 0x20000, CRC(9a2a2db1) SHA1(57524e76311afc8ab5d5affa76c85cb1be5a1faf) )
	ROM_LOAD16_BYTE( "che_35.rom",   0x00001, 0x20000, CRC(a7f96b02) SHA1(b5fda02e5069f9e1cdafbacf98334510e9af8fcd) )
	ROM_LOAD16_BYTE( "che_31.rom",   0x40000, 0x20000, CRC(bbff8a99) SHA1(1f931fad9f43a1494f3b8dbcf910156d5b0bd458) )
	ROM_LOAD16_BYTE( "che_36.rom",   0x40001, 0x20000, CRC(0fa00c39) SHA1(6404d91590c5c521c8fe944a0aa7091e35b664ae) )
	ROM_LOAD16_WORD_SWAP( "ch_32.rom",    0x80000, 0x80000, CRC(9b70bd41) SHA1(28ec37d9d0ace5b9fd212fdc02e0f13dc280c068) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ch_gfx5.rom",  0x000000, 0x80000, CRC(4ec75f15) SHA1(a4669e3864009b01894406db784116ad5cd2eced) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ch_gfx7.rom",  0x000002, 0x80000, CRC(d85d00d6) SHA1(ca6ddcbfbb0f9ad98dc19f09e879fdac5b62d168) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ch_gfx1.rom",  0x000004, 0x80000, CRC(f33ca9d4) SHA1(480d90ff16f27777cc7d7de6925ed6378b35dc27) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ch_gfx3.rom",  0x000006, 0x80000, CRC(0ba2047f) SHA1(efee13b955c2ded52700025cecbb9fb301098b61) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ch_09.rom",     0x00000, 0x08000, CRC(4d4255b7) SHA1(81a76b58043af7252a854b7efc4109957ef0e679) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ch_18.rom",    0x00000, 0x20000, CRC(f909e8de) SHA1(2dd5bd4076e7d5ded98b72919f868ea700df2e4f) )
	ROM_LOAD( "ch_19.rom",    0x20000, 0x20000, CRC(fc158cf7) SHA1(294b93d0aea60663ffe96364671552e944a1264b) )
ROM_END

ROM_START( chikij )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "chj36a.bin",   0x00000, 0x20000, CRC(ec1328d8) SHA1(a7111f9c264c56d1c6474ec3ad90e394a32a86f6) )
	ROM_LOAD16_BYTE( "chj42a.bin",   0x00001, 0x20000, CRC(4ae13503) SHA1(c47db0445e107ad4fb62b74e277a7dc2b4d9b7ea) )
	ROM_LOAD16_BYTE( "chj37a.bin",   0x40000, 0x20000, CRC(46d2cf7b) SHA1(5cb7ed3003d89a08882d4dcd326c8fd9430f0eac) )
	ROM_LOAD16_BYTE( "chj43a.bin",   0x40001, 0x20000, CRC(8d387fe8) SHA1(7832ecd487b5ef4e49b5ea78e80e52f8e2dcaa17) )
	ROM_LOAD16_WORD_SWAP( "ch_32.rom",    0x80000, 0x80000, CRC(9b70bd41) SHA1(28ec37d9d0ace5b9fd212fdc02e0f13dc280c068) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ch_gfx5.rom",  0x000000, 0x80000, CRC(4ec75f15) SHA1(a4669e3864009b01894406db784116ad5cd2eced) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ch_gfx7.rom",  0x000002, 0x80000, CRC(d85d00d6) SHA1(ca6ddcbfbb0f9ad98dc19f09e879fdac5b62d168) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ch_gfx1.rom",  0x000004, 0x80000, CRC(f33ca9d4) SHA1(480d90ff16f27777cc7d7de6925ed6378b35dc27) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ch_gfx3.rom",  0x000006, 0x80000, CRC(0ba2047f) SHA1(efee13b955c2ded52700025cecbb9fb301098b61) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ch_09.rom",     0x00000, 0x08000, CRC(4d4255b7) SHA1(81a76b58043af7252a854b7efc4109957ef0e679) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ch_18.rom",    0x00000, 0x20000, CRC(f909e8de) SHA1(2dd5bd4076e7d5ded98b72919f868ea700df2e4f) )
	ROM_LOAD( "ch_19.rom",    0x20000, 0x20000, CRC(fc158cf7) SHA1(294b93d0aea60663ffe96364671552e944a1264b) )
ROM_END

ROM_START( msword )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "mse_30.rom",   0x00000, 0x20000, CRC(03fc8dbc) SHA1(a9e4e8a06e2d170faeae75a8b17fd65e6e5fecd4) )
	ROM_LOAD16_BYTE( "mse_35.rom",   0x00001, 0x20000, CRC(d5bf66cd) SHA1(37c5bc4deafd7037ec5cf09c88bb89f35ea3d95c) )
	ROM_LOAD16_BYTE( "mse_31.rom",   0x40000, 0x20000, CRC(30332bcf) SHA1(1c77c06028b77473276cb5dde5ecf414b43a7b78) )
	ROM_LOAD16_BYTE( "mse_36.rom",   0x40001, 0x20000, CRC(8f7d6ce9) SHA1(7694c940023c12520663593f973ddb4168a6bfa5) )
	ROM_LOAD16_WORD_SWAP( "ms_32.rom",    0x80000, 0x80000, CRC(2475ddfc) SHA1(cc34dfae8124aa781320be6870a1929495eee456) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ms_gfx5.rom",  0x000000, 0x80000, CRC(c00fe7e2) SHA1(1ce82ea36996908620d3ac8aabd3650118d6c255) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx7.rom",  0x000002, 0x80000, CRC(4ccacac5) SHA1(f2e30edf6ad100da411584bb0b828420256a9d5c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx1.rom",  0x000004, 0x80000, CRC(0d2bbe00) SHA1(dca13fc7ff63ad7fb175a71ada1ee22d21a8811d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx3.rom",  0x000006, 0x80000, CRC(3a1a5bf4) SHA1(88a7cc0bf29b3516a97f661691500ff28e91a362) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ms_9.rom",      0x00000, 0x08000, CRC(57b29519) SHA1(a6b4fc2b9595d1a49f2b93581f107b68d484d156) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ms_18.rom",    0x00000, 0x20000, CRC(fb64e90d) SHA1(d1a596ce2f8ac14a80b34335b173369a14b45f55) )
	ROM_LOAD( "ms_19.rom",    0x20000, 0x20000, CRC(74f892b9) SHA1(bf48db5c438154e7b96fd31fde1be4aad5cf25eb) )
ROM_END

ROM_START( mswordr1 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "ms30.11f",   0x00000, 0x20000, CRC(21c1f078) SHA1(f32bd3b462cc84466244b362a66510b9d40ac2fd) )
	ROM_LOAD16_BYTE( "ms35.11h",   0x00001, 0x20000, CRC(a540a73a) SHA1(1c91241ba0d17d13adaa68e231b95dfd49d93b6d) )
	ROM_LOAD16_BYTE( "ms31.12f",   0x40000, 0x20000, CRC(d7e762b5) SHA1(6977130e9c0cd36d8a67e242c132df38f7aea5b7) )
	ROM_LOAD16_BYTE( "ms36.12h",   0x40001, 0x20000, CRC(66f2dcdb) SHA1(287508b1c96762d0048a10272cf2cbd39a7fba5c) )
	ROM_LOAD16_WORD_SWAP( "ms_32.rom",    0x80000, 0x80000, CRC(2475ddfc) SHA1(cc34dfae8124aa781320be6870a1929495eee456) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ms_gfx5.rom",  0x000000, 0x80000, CRC(c00fe7e2) SHA1(1ce82ea36996908620d3ac8aabd3650118d6c255) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx7.rom",  0x000002, 0x80000, CRC(4ccacac5) SHA1(f2e30edf6ad100da411584bb0b828420256a9d5c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx1.rom",  0x000004, 0x80000, CRC(0d2bbe00) SHA1(dca13fc7ff63ad7fb175a71ada1ee22d21a8811d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx3.rom",  0x000006, 0x80000, CRC(3a1a5bf4) SHA1(88a7cc0bf29b3516a97f661691500ff28e91a362) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ms_9.rom",      0x00000, 0x08000, CRC(57b29519) SHA1(a6b4fc2b9595d1a49f2b93581f107b68d484d156) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ms_18.rom",    0x00000, 0x20000, CRC(fb64e90d) SHA1(d1a596ce2f8ac14a80b34335b173369a14b45f55) )
	ROM_LOAD( "ms_19.rom",    0x20000, 0x20000, CRC(74f892b9) SHA1(bf48db5c438154e7b96fd31fde1be4aad5cf25eb) )
ROM_END

ROM_START( mswordu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "msu30",   0x00000, 0x20000, CRC(d963c816) SHA1(e23844a60ddfc0a8a98c0ada9c3d58fce71c5484) )
	ROM_LOAD16_BYTE( "msu35",   0x00001, 0x20000, CRC(72f179b3) SHA1(8d31cdc84b02fc345fc78e8f231410adeb834c28) )
	ROM_LOAD16_BYTE( "msu31",   0x40000, 0x20000, CRC(20cd7904) SHA1(cea2db01be97f69dc10e9da80f3b46f6ddaa953a) )
	ROM_LOAD16_BYTE( "msu36",   0x40001, 0x20000, CRC(bf88c080) SHA1(b8cd0b127fd3e1afc45402e667ff4b4b01602384) )
	ROM_LOAD16_WORD_SWAP( "ms_32.rom",    0x80000, 0x80000, CRC(2475ddfc) SHA1(cc34dfae8124aa781320be6870a1929495eee456) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ms_gfx5.rom",  0x000000, 0x80000, CRC(c00fe7e2) SHA1(1ce82ea36996908620d3ac8aabd3650118d6c255) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx7.rom",  0x000002, 0x80000, CRC(4ccacac5) SHA1(f2e30edf6ad100da411584bb0b828420256a9d5c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx1.rom",  0x000004, 0x80000, CRC(0d2bbe00) SHA1(dca13fc7ff63ad7fb175a71ada1ee22d21a8811d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx3.rom",  0x000006, 0x80000, CRC(3a1a5bf4) SHA1(88a7cc0bf29b3516a97f661691500ff28e91a362) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ms_9.rom",      0x00000, 0x08000, CRC(57b29519) SHA1(a6b4fc2b9595d1a49f2b93581f107b68d484d156) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ms_18.rom",    0x00000, 0x20000, CRC(fb64e90d) SHA1(d1a596ce2f8ac14a80b34335b173369a14b45f55) )
	ROM_LOAD( "ms_19.rom",    0x20000, 0x20000, CRC(74f892b9) SHA1(bf48db5c438154e7b96fd31fde1be4aad5cf25eb) )
ROM_END

ROM_START( mswordj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "msj_36.bin",   0x00000, 0x20000, CRC(04f0ef50) SHA1(76dac695732ade1873bb6da51834ef90c6595689) )
	ROM_LOAD16_BYTE( "msj_42.bin",   0x00001, 0x20000, CRC(9fcbb9cd) SHA1(bfbf805ddecd3fa9e209a658526e1430ad9e459a) )
	ROM_LOAD16_BYTE( "msj_37.bin",   0x40000, 0x20000, CRC(6c060d70) SHA1(7fe56f125bc11156955bf0defc956fe7c18a1c72) )
	ROM_LOAD16_BYTE( "msj_43.bin",   0x40001, 0x20000, CRC(aec77787) SHA1(3260f9a80b67394dd90dbabdd544c9b8b31e5817) )
	ROM_LOAD16_WORD_SWAP( "ms_32.rom",    0x80000, 0x80000, CRC(2475ddfc) SHA1(cc34dfae8124aa781320be6870a1929495eee456) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ms_gfx5.rom",  0x000000, 0x80000, CRC(c00fe7e2) SHA1(1ce82ea36996908620d3ac8aabd3650118d6c255) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx7.rom",  0x000002, 0x80000, CRC(4ccacac5) SHA1(f2e30edf6ad100da411584bb0b828420256a9d5c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx1.rom",  0x000004, 0x80000, CRC(0d2bbe00) SHA1(dca13fc7ff63ad7fb175a71ada1ee22d21a8811d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ms_gfx3.rom",  0x000006, 0x80000, CRC(3a1a5bf4) SHA1(88a7cc0bf29b3516a97f661691500ff28e91a362) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ms_9.rom",      0x00000, 0x08000, CRC(57b29519) SHA1(a6b4fc2b9595d1a49f2b93581f107b68d484d156) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ms_18.rom",    0x00000, 0x20000, CRC(fb64e90d) SHA1(d1a596ce2f8ac14a80b34335b173369a14b45f55) )
	ROM_LOAD( "ms_19.rom",    0x20000, 0x20000, CRC(74f892b9) SHA1(bf48db5c438154e7b96fd31fde1be4aad5cf25eb) )
ROM_END

ROM_START( cawing )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "cae_30a.rom",  0x00000, 0x20000, CRC(91fceacd) SHA1(4845999a96fee829264346ca399fdd64a8408001) )
	ROM_LOAD16_BYTE( "cae_35a.rom",  0x00001, 0x20000, CRC(3ef03083) SHA1(297dfc9ec1e0f07d6083bf5efaa0de8d0fb361fa) )
	ROM_LOAD16_BYTE( "cae_31a.rom",  0x40000, 0x20000, CRC(e5b75caf) SHA1(4d04220c78620867b7598deea5685bbe88298ae6) )
	ROM_LOAD16_BYTE( "cae_36a.rom",  0x40001, 0x20000, CRC(c73fd713) SHA1(fa202c252b2cc5972d42d634c466d89cf8b5d178) )
	ROM_LOAD16_WORD_SWAP( "ca_32.rom", 0x80000, 0x80000, CRC(0c4837d4) SHA1(1c61958b43066b59d86eb4bae0b52c3109be4b07) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ca_gfx5.rom",  0x000000, 0x80000, CRC(66d4cc37) SHA1(d355ea64ff29d228dcbfeee72bcf11882bf1cd9d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ca_gfx7.rom",  0x000002, 0x80000, CRC(b6f896f2) SHA1(bdb6820b81fbce77d7eacb01777af7c380490402) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ca_gfx1.rom",  0x000004, 0x80000, CRC(4d0620fd) SHA1(5f62cd551b6a230edefd81fa60c10c84186ca804) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ca_gfx3.rom",  0x000006, 0x80000, CRC(0b0341c3) SHA1(c31f0e78f49d94ea9dea20eb0cbd98a6c613bcbf) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ca_9.rom",      0x00000, 0x08000, CRC(96fe7485) SHA1(10466889dfc6bc8afd3075385e241a16372efbeb) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ca_18.rom",    0x00000, 0x20000, CRC(4a613a2c) SHA1(06e10644fc60925b85d2ca0888c9fa057bfe996a) )
	ROM_LOAD( "ca_19.rom",    0x20000, 0x20000, CRC(74584493) SHA1(5cfb15f1b9729323707972646313aee8ab3ac4eb) )
ROM_END

ROM_START( cawingu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "cae_30a.rom",  0x00000, 0x20000, CRC(91fceacd) SHA1(4845999a96fee829264346ca399fdd64a8408001) )
	ROM_LOAD16_BYTE( "cau_35a.rom",  0x00001, 0x20000, CRC(f090d9b2) SHA1(261dc4ac79507299a7f9a1ad5edb8425345db06c) )
	ROM_LOAD16_BYTE( "cae_31a.rom",  0x40000, 0x20000, CRC(e5b75caf) SHA1(4d04220c78620867b7598deea5685bbe88298ae6) )
	ROM_LOAD16_BYTE( "cae_36a.rom",  0x40001, 0x20000, CRC(c73fd713) SHA1(fa202c252b2cc5972d42d634c466d89cf8b5d178) )
	ROM_LOAD16_WORD_SWAP( "ca_32.rom", 0x80000, 0x80000, CRC(0c4837d4) SHA1(1c61958b43066b59d86eb4bae0b52c3109be4b07) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "ca_gfx5.rom",  0x000000, 0x80000, CRC(66d4cc37) SHA1(d355ea64ff29d228dcbfeee72bcf11882bf1cd9d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ca_gfx7.rom",  0x000002, 0x80000, CRC(b6f896f2) SHA1(bdb6820b81fbce77d7eacb01777af7c380490402) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ca_gfx1.rom",  0x000004, 0x80000, CRC(4d0620fd) SHA1(5f62cd551b6a230edefd81fa60c10c84186ca804) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ca_gfx3.rom",  0x000006, 0x80000, CRC(0b0341c3) SHA1(c31f0e78f49d94ea9dea20eb0cbd98a6c613bcbf) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ca_9.rom",      0x00000, 0x08000, CRC(96fe7485) SHA1(10466889dfc6bc8afd3075385e241a16372efbeb) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ca_18.rom",    0x00000, 0x20000, CRC(4a613a2c) SHA1(06e10644fc60925b85d2ca0888c9fa057bfe996a) )
	ROM_LOAD( "ca_19.rom",    0x20000, 0x20000, CRC(74584493) SHA1(5cfb15f1b9729323707972646313aee8ab3ac4eb) )
ROM_END

ROM_START( cawingj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "cae_30a.rom",  0x00000, 0x20000, CRC(91fceacd) SHA1(4845999a96fee829264346ca399fdd64a8408001) )
	ROM_LOAD16_BYTE( "caj42a.bin",   0x00001, 0x20000, CRC(039f8362) SHA1(3fc7a642ddeaf94abdfdd5788a4b3c3b1f1b4c5e) )
	ROM_LOAD16_BYTE( "cae_31a.rom",  0x40000, 0x20000, CRC(e5b75caf) SHA1(4d04220c78620867b7598deea5685bbe88298ae6) )
	ROM_LOAD16_BYTE( "cae_36a.rom",  0x40001, 0x20000, CRC(c73fd713) SHA1(fa202c252b2cc5972d42d634c466d89cf8b5d178) )
	ROM_LOAD16_BYTE( "caj34.bin",    0x80000, 0x20000, CRC(51ea57f4) SHA1(7d7080dbf4b6f9b801b796937e9c3c45afed602f) )
	ROM_LOAD16_BYTE( "caj40.bin",    0x80001, 0x20000, CRC(2ab71ae1) SHA1(23814b58322902b23c4bdd744e60d819811462cc) )
	ROM_LOAD16_BYTE( "caj35.bin",    0xc0000, 0x20000, CRC(01d71973) SHA1(1f5fc0d47f1456a6338284f883dabc89697f8aa5) )
	ROM_LOAD16_BYTE( "caj41.bin",    0xc0001, 0x20000, CRC(3a43b538) SHA1(474a701500632cbd395ae404ede1d10a9969b342) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "caj09.bin",    0x000000, 0x20000, CRC(41b0f9a6) SHA1(5a59df64d0c665d5d479ef2d9e7ec191ca0e7a92) , ROM_SKIP(7) )
	ROMX_LOAD( "caj01.bin",    0x000001, 0x20000, CRC(1002d0b8) SHA1(896bec683c1164c6f1fa1d81cadb8a1c549d4a4e) , ROM_SKIP(7) )
	ROMX_LOAD( "caj13.bin",    0x000002, 0x20000, CRC(6f3948b2) SHA1(942d37b84c727074941316b042679110594ae249) , ROM_SKIP(7) )
	ROMX_LOAD( "caj05.bin",    0x000003, 0x20000, CRC(207373d7) SHA1(4fa67c847f65e2657900f4fc93f1d8a7b95c12e6) , ROM_SKIP(7) )
	ROMX_LOAD( "caj24.bin",    0x000004, 0x20000, CRC(e356aad7) SHA1(55f1489044e70a57ad15e1f2c20567bd6c770f71) , ROM_SKIP(7) )
	ROMX_LOAD( "caj17.bin",    0x000005, 0x20000, CRC(540f2fd8) SHA1(4f500c7795aa41d472c59d2594fc84f6b17ed137) , ROM_SKIP(7) )
	ROMX_LOAD( "caj38.bin",    0x000006, 0x20000, CRC(2464d4ab) SHA1(092864551e7c1c6adbeb901a556f650ccf6ca2f4) , ROM_SKIP(7) )
	ROMX_LOAD( "caj32.bin",    0x000007, 0x20000, CRC(9b5836b3) SHA1(3fda5409d99104f355fa42ec413ccb799d1506c9) , ROM_SKIP(7) )
	ROMX_LOAD( "caj10.bin",    0x100000, 0x20000, CRC(bf8a5f52) SHA1(8cdd31a58de560d282e708c57cda0fefa7d6c92f) , ROM_SKIP(7) )
	ROMX_LOAD( "caj02.bin",    0x100001, 0x20000, CRC(125b018d) SHA1(edb3271f668e0328efd59e0929ee86efd5aa7b1f) , ROM_SKIP(7) )
	ROMX_LOAD( "caj14.bin",    0x100002, 0x20000, CRC(8458e7d7) SHA1(7ebe0a3597a7d8482c2d33640ed1b994fd3a02b2) , ROM_SKIP(7) )
	ROMX_LOAD( "caj06.bin",    0x100003, 0x20000, CRC(cf80e164) SHA1(d83573947fec01a9814919df719474aa3e6ae9a0) , ROM_SKIP(7) )
	ROMX_LOAD( "caj25.bin",    0x100004, 0x20000, CRC(cdd0204d) SHA1(7cbc129bc148718f8c36e27f05583cdecc57b63e) , ROM_SKIP(7) )
	ROMX_LOAD( "caj18.bin",    0x100005, 0x20000, CRC(29c1d4b1) SHA1(d0109ab2f521786a64548910947ca24976ec1218) , ROM_SKIP(7) )
	ROMX_LOAD( "caj39.bin",    0x100006, 0x20000, CRC(eea23b67) SHA1(79d35c15b4b4430d90cd6c270cdd3a064bc2e1a3) , ROM_SKIP(7) )
	ROMX_LOAD( "caj33.bin",    0x100007, 0x20000, CRC(dde3891f) SHA1(25b8069a9c8615323b94157b1ce39805559b68f4) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "ca_9.rom",      0x00000, 0x08000, CRC(96fe7485) SHA1(10466889dfc6bc8afd3075385e241a16372efbeb) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ca_18.rom",    0x00000, 0x20000, CRC(4a613a2c) SHA1(06e10644fc60925b85d2ca0888c9fa057bfe996a) )
	ROM_LOAD( "ca_19.rom",    0x20000, 0x20000, CRC(74584493) SHA1(5cfb15f1b9729323707972646313aee8ab3ac4eb) )
ROM_END

ROM_START( nemo )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "nme_30a.rom",  0x00000, 0x20000, CRC(d2c03e56) SHA1(df468e3b5deba01a6825b742f1cc87bfb26c1981) )
	ROM_LOAD16_BYTE( "nme_35a.rom",  0x00001, 0x20000, CRC(5fd31661) SHA1(12f92a7255e8cae6975452db956670cf72d51768) )
	ROM_LOAD16_BYTE( "nme_31a.rom",  0x40000, 0x20000, CRC(b2bd4f6f) SHA1(82a59b5f36cb4c23dca05297e2a643842fc12609) )
	ROM_LOAD16_BYTE( "nme_36a.rom",  0x40001, 0x20000, CRC(ee9450e3) SHA1(a5454268ef58533e71fe07167b4c3fd263363f77) )
	ROM_LOAD16_WORD_SWAP( "nm_32.rom",    0x80000, 0x80000, CRC(d6d1add3) SHA1(61c3013d322dbb7622cca032adcd020ba318e885) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "nm_gfx5.rom",  0x000000, 0x80000, CRC(487b8747) SHA1(f14339b02b8f7ec2002632349e88fed4afc30050) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "nm_gfx7.rom",  0x000002, 0x80000, CRC(203dc8c6) SHA1(d52577500e822b89904d1510d559f8575c2aaa78) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "nm_gfx1.rom",  0x000004, 0x80000, CRC(9e878024) SHA1(9a5ce3a6a7952a8954d0709b9473db9253793d70) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "nm_gfx3.rom",  0x000006, 0x80000, CRC(bb01e6b6) SHA1(3883e28f721d0278b2f4f877a804e95ee14f53e4) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "nm_09.rom",     0x00000, 0x08000, CRC(0f4b0581) SHA1(2e5a2885149c632abfaf4292a1bf032c13c8da6c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "nm_18.rom",    0x00000, 0x20000, CRC(bab333d4) SHA1(c1d0fb61ec46f17eb7edf69e1ad5ac91b5d51daa) )
	ROM_LOAD( "nm_19.rom",    0x20000, 0x20000, CRC(2650a0a8) SHA1(e9e8cc1b27a2cb3e87124061fabcf42982f0611f) )
ROM_END

ROM_START( nemoj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "nm36.bin",     0x00000, 0x20000, CRC(daeceabb) SHA1(ebd44922be9d07a3d3411af52edee8a60cb11dad) )
	ROM_LOAD16_BYTE( "nm42.bin",     0x00001, 0x20000, CRC(55024740) SHA1(4bacbd191bb435de5dc548ac7fa16ed286bd2d3b) )
	ROM_LOAD16_BYTE( "nm37.bin",     0x40000, 0x20000, CRC(619068b6) SHA1(2507c6f77a06a80f913c848dcb6816bcbf4bba8a) )
	ROM_LOAD16_BYTE( "nm43.bin",     0x40001, 0x20000, CRC(a948a53b) SHA1(65c2abf321cf8b171bbfbb51ed57bc99eb552ca9) )
	ROM_LOAD16_WORD_SWAP( "nm_32.rom",    0x80000, 0x80000, CRC(d6d1add3) SHA1(61c3013d322dbb7622cca032adcd020ba318e885) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "nm_gfx5.rom",  0x000000, 0x80000, CRC(487b8747) SHA1(f14339b02b8f7ec2002632349e88fed4afc30050) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "nm_gfx7.rom",  0x000002, 0x80000, CRC(203dc8c6) SHA1(d52577500e822b89904d1510d559f8575c2aaa78) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "nm_gfx1.rom",  0x000004, 0x80000, CRC(9e878024) SHA1(9a5ce3a6a7952a8954d0709b9473db9253793d70) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "nm_gfx3.rom",  0x000006, 0x80000, CRC(bb01e6b6) SHA1(3883e28f721d0278b2f4f877a804e95ee14f53e4) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "nm_09.rom",     0x00000, 0x08000, CRC(0f4b0581) SHA1(2e5a2885149c632abfaf4292a1bf032c13c8da6c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "nm_18.rom",    0x00000, 0x20000, CRC(bab333d4) SHA1(c1d0fb61ec46f17eb7edf69e1ad5ac91b5d51daa) )
	ROM_LOAD( "nm_19.rom",    0x20000, 0x20000, CRC(2650a0a8) SHA1(e9e8cc1b27a2cb3e87124061fabcf42982f0611f) )
ROM_END

ROM_START( sf2 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2e.30g",      0x00000, 0x20000, CRC(fe39ee33) SHA1(22558eb15e035b09b80935a32b8425d91cd79669) )	/* matches sf2u.30i*/
	ROM_LOAD16_BYTE( "sf2e.37g",      0x00001, 0x20000, CRC(fb92cd74) SHA1(bf1ccfe7cc1133f0f65556430311108722add1f2) )
	ROM_LOAD16_BYTE( "sf2e.31g",      0x40000, 0x20000, CRC(69a0a301) SHA1(86a3954335310865b14ce8b4e0e4499feb14fc12) )	/* matches sf2u.31i*/
	ROM_LOAD16_BYTE( "sf2e.38g",      0x40001, 0x20000, CRC(5e22db70) SHA1(6565946591a18eaf46f04c1aa449ee0ae9ac2901) )
	ROM_LOAD16_BYTE( "sf2e.28g",      0x80000, 0x20000, CRC(8bf9f1e5) SHA1(bbcef63f35e5bff3f373968ba1278dd6bd86b593) )
	ROM_LOAD16_BYTE( "sf2e.35g",      0x80001, 0x20000, CRC(626ef934) SHA1(507bda3e4519de237aca919cf72e543403ec9724) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2eb )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2_30a.bin",   0x00000, 0x20000, CRC(57bd7051) SHA1(5e211e75b1649b07723cabc03cf15636dbbae595) )
	ROM_LOAD16_BYTE( "sf2e_37b.rom",  0x00001, 0x20000, CRC(62691cdd) SHA1(328703c3e737ada544e67c36119eeb4a100ca740) )
	ROM_LOAD16_BYTE( "sf2_31a.bin",   0x40000, 0x20000, CRC(a673143d) SHA1(e565f0ec23d6deb543c72af5a83f070c07319477) )
	ROM_LOAD16_BYTE( "sf2_38a.bin",   0x40001, 0x20000, CRC(4c2ccef7) SHA1(77b119c70c255622b023de25d9af3b3aac52ea47) )
	ROM_LOAD16_BYTE( "sf2_28a.bin",   0x80000, 0x20000, CRC(4009955e) SHA1(7842dbef7650485639fbae49b9f4db7494d4f73d) )
	ROM_LOAD16_BYTE( "sf2_35a.bin",   0x80001, 0x20000, CRC(8c1f3994) SHA1(5e1d334399d05a837c2d80f79eada543e83afaf7) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ua )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2u.30a",      0x00000, 0x20000, CRC(08beb861) SHA1(d47f16d0d692dc6405df0aecd7d9fc3f9718c0d1) )
	ROM_LOAD16_BYTE( "sf2u.37a",      0x00001, 0x20000, CRC(b7638d69) SHA1(b615a2e0e8772462fd875b2e8d2ccba82a8b3c47) )
	ROM_LOAD16_BYTE( "sf2u.31a",      0x40000, 0x20000, CRC(0d5394e0) SHA1(e1d88ff3669f1dbe1e3fbdf8aa9e2c63adbbcb48) )
	ROM_LOAD16_BYTE( "sf2u.38a",      0x40001, 0x20000, CRC(42d6a79e) SHA1(5f1e2c176d065325883a60767d05b1a542372b6a) )
	ROM_LOAD16_BYTE( "sf2u.28a",      0x80000, 0x20000, CRC(387a175c) SHA1(2635bb82758cf217cee63b254a537b02275a6838) )
	ROM_LOAD16_BYTE( "sf2u.35a",      0x80001, 0x20000, CRC(a1a5adcc) SHA1(47874e6d403256d828474b29e3d93c92efd9e1ce) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ub )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2_30a.bin",   0x00000, 0x20000, CRC(57bd7051) SHA1(5e211e75b1649b07723cabc03cf15636dbbae595) )
	ROM_LOAD16_BYTE( "sf2u.37b",      0x00001, 0x20000, CRC(4a54d479) SHA1(eaff7a0d3c858a567c02086fde163850f0f5631e) )
	ROM_LOAD16_BYTE( "sf2_31a.bin",   0x40000, 0x20000, CRC(a673143d) SHA1(e565f0ec23d6deb543c72af5a83f070c07319477) )
	ROM_LOAD16_BYTE( "sf2_38a.bin",   0x40001, 0x20000, CRC(4c2ccef7) SHA1(77b119c70c255622b023de25d9af3b3aac52ea47) )
	ROM_LOAD16_BYTE( "sf2_28a.bin",   0x80000, 0x20000, CRC(4009955e) SHA1(7842dbef7650485639fbae49b9f4db7494d4f73d) )
	ROM_LOAD16_BYTE( "sf2_35a.bin",   0x80001, 0x20000, CRC(8c1f3994) SHA1(5e1d334399d05a837c2d80f79eada543e83afaf7) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ud )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2u.30d",   0x00000, 0x20000, CRC(4bb2657c) SHA1(b2d077296b77be7db371f953b7fc446a67d8a9d6) )
	ROM_LOAD16_BYTE( "sf2u.37d",   0x00001, 0x20000, CRC(b33b42f2) SHA1(2e0babc8734c79dc2b51a6be64433bb2411c3da5) )
	ROM_LOAD16_BYTE( "sf2u.31d",   0x40000, 0x20000, CRC(d57b67d7) SHA1(43d0b47c9fada8d9b445caa4b96ac8493061aa8b) )
	ROM_LOAD16_BYTE( "sf2u.38d",   0x40001, 0x20000, CRC(9c8916ef) SHA1(a4629356a816454bcc1d7b41e70e147d4769a682) )
	ROM_LOAD16_BYTE( "sf2u.28d",   0x80000, 0x20000, CRC(175819d1) SHA1(c98b6b7af4e57735dbfb3d1e61ba1bfb9f145d33) )
	ROM_LOAD16_BYTE( "sf2u.35d",   0x80001, 0x20000, CRC(82060da4) SHA1(7487cfc28cce3d76772ece657aef83b56034011e) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ue )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2u.30e",      0x00000, 0x20000, CRC(f37cd088) SHA1(48b71e44ce88d5f682ed679c737e7ec5262bb0df) )
	ROM_LOAD16_BYTE( "sf2u.37e",      0x00001, 0x20000, CRC(6c61a513) SHA1(6dc9ccd58fd5ef15ff9df20c865ff6c850f2b7dc) )
	ROM_LOAD16_BYTE( "sf2u.31e",      0x40000, 0x20000, CRC(7c4771b4) SHA1(6637b24194c86ec72a1775d4e976891243cd66fd) )
	ROM_LOAD16_BYTE( "sf2u.38e",      0x40001, 0x20000, CRC(a4bd0cd9) SHA1(32a2bc18d1f860668141e53cbca862ceec238c19) )
	ROM_LOAD16_BYTE( "sf2u.28e",      0x80000, 0x20000, CRC(e3b95625) SHA1(f7277f9980040f96434d1bd162eaf9ba0dfbb005) )
	ROM_LOAD16_BYTE( "sf2u.35e",      0x80001, 0x20000, CRC(3648769a) SHA1(74e5934b0e3b4da35ff48086f41e7502b42731c6) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2uf )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2e.30g",      0x00000, 0x20000, CRC(fe39ee33) SHA1(22558eb15e035b09b80935a32b8425d91cd79669) )
	ROM_LOAD16_BYTE( "sf2u.37f",      0x00001, 0x20000, CRC(169e7388) SHA1(c7cb2de529d94cea4a018ed3bd611037fe54abe7) )
	ROM_LOAD16_BYTE( "sf2e.31g",      0x40000, 0x20000, CRC(69a0a301) SHA1(86a3954335310865b14ce8b4e0e4499feb14fc12) )
	ROM_LOAD16_BYTE( "sf2u.38f",      0x40001, 0x20000, CRC(1510e4e2) SHA1(fbfdd4e42c4bc894592dbe5a84c88d5f13d21da4) )
	ROM_LOAD16_BYTE( "sf2u.28f",      0x80000, 0x20000, CRC(acd8175b) SHA1(504991c46fa568d31ce69bd63e2a67926a06b5a9) )
	ROM_LOAD16_BYTE( "sf2u.35f",      0x80001, 0x20000, CRC(c0a80bd1) SHA1(ac25a9ed488e03baf4115541fdcce3973ce6a442) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ui )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2e.30g",      0x00000, 0x20000, CRC(fe39ee33) SHA1(22558eb15e035b09b80935a32b8425d91cd79669) )
	ROM_LOAD16_BYTE( "sf2u.37i",      0x00001, 0x20000, CRC(9df707dd) SHA1(b148ea450f9e96f3c20f487010a3c57f778e40c1) )
	ROM_LOAD16_BYTE( "sf2e.31g",      0x40000, 0x20000, CRC(69a0a301) SHA1(86a3954335310865b14ce8b4e0e4499feb14fc12) )
	ROM_LOAD16_BYTE( "sf2u.38i",      0x40001, 0x20000, CRC(4cb46daf) SHA1(dee103ae1391cd9ac150f787187233cd8c06ea1e) )
	ROM_LOAD16_BYTE( "sf2u.28i",      0x80000, 0x20000, CRC(1580be4c) SHA1(d89ed0ff4bf14ff2eaae4609f55970b6b37c8e32) )
	ROM_LOAD16_BYTE( "sf2u.35i",      0x80001, 0x20000, CRC(1468d185) SHA1(750de0cad3859e4917aebb02c2e137dea619f201) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2uk )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2u.30k",      0x00000, 0x20000, CRC(8f66076c) SHA1(f9653b36bb5012e6bde5fe3bcade4a6a7a7e7def) )
	ROM_LOAD16_BYTE( "sf2u.37k",      0x00001, 0x20000, CRC(4e1f6a83) SHA1(ee679b79ff3c3165979d3de23e0f668839cf465f) )
	ROM_LOAD16_BYTE( "sf2u.31k",      0x40000, 0x20000, CRC(f9f89f60) SHA1(c3b71482b85c83576518f300be768655412276b0) )
	ROM_LOAD16_BYTE( "sf2u.38k",      0x40001, 0x20000, CRC(6ce0a85a) SHA1(567fd18cd626c94496d9123ecef87dc638f0041a) )
	ROM_LOAD16_BYTE( "sf2u.28k",      0x80000, 0x20000, CRC(8e958f31) SHA1(81359bc988c4e9e375b5bbd960921d425b77f706) )
	ROM_LOAD16_BYTE( "sf2u.35k",      0x80001, 0x20000, CRC(fce76fad) SHA1(66f881ba600c7e6bbe960cfd0772ed16208b79c8) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2j )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2j30.bin",    0x00000, 0x20000, CRC(79022b31) SHA1(b7cfe0498260cdd2779580c47829dd02435ffff4) )
	ROM_LOAD16_BYTE( "sf2j37.bin",    0x00001, 0x20000, CRC(516776ec) SHA1(4f8b63c4d4265a105751fa72b50bd0fa538bf881) )
	ROM_LOAD16_BYTE( "sf2j31.bin",    0x40000, 0x20000, CRC(fe15cb39) SHA1(383478524881ea70d9e04c9b6143b8735b637eee) )
	ROM_LOAD16_BYTE( "sf2j38.bin",    0x40001, 0x20000, CRC(38614d70) SHA1(39c58096f3a8e01fb439639b742b83102bbaa7f6) )
	ROM_LOAD16_BYTE( "sf2j28.bin",    0x80000, 0x20000, CRC(d283187a) SHA1(5ea83d2652e43e46b831b614d1fe06d465bac9a3) )
	ROM_LOAD16_BYTE( "sf2j35.bin",    0x80001, 0x20000, CRC(d28158e4) SHA1(bf2bca6068e374011afa95e99809d262f522df18) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ja )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2_30a.bin",   0x00000, 0x20000, CRC(57bd7051) SHA1(5e211e75b1649b07723cabc03cf15636dbbae595) )
	ROM_LOAD16_BYTE( "sf2j_37a.bin",  0x00001, 0x20000, CRC(1e1f6844) SHA1(c80e5ac6a6cea39511c38e31ea55b6cd3888024f) )
	ROM_LOAD16_BYTE( "sf2_31a.bin",   0x40000, 0x20000, CRC(a673143d) SHA1(e565f0ec23d6deb543c72af5a83f070c07319477) )
	ROM_LOAD16_BYTE( "sf2_38a.bin",   0x40001, 0x20000, CRC(4c2ccef7) SHA1(77b119c70c255622b023de25d9af3b3aac52ea47) )
	ROM_LOAD16_BYTE( "sf2_28a.bin",   0x80000, 0x20000, CRC(4009955e) SHA1(7842dbef7650485639fbae49b9f4db7494d4f73d) )
	ROM_LOAD16_BYTE( "sf2_35a.bin",   0x80001, 0x20000, CRC(8c1f3994) SHA1(5e1d334399d05a837c2d80f79eada543e83afaf7) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2jc )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "sf2_30c.bin",   0x00000, 0x20000, CRC(8add35ec) SHA1(b08428ff262ca4feddd3c72058b4b674a5401aba) )
	ROM_LOAD16_BYTE( "sf2j_37c.bin",  0x00001, 0x20000, CRC(0d74a256) SHA1(587fd0ee1c2ef54554237486eb5b0d1ec30c2868) )
	ROM_LOAD16_BYTE( "sf2_31c.bin",   0x40000, 0x20000, CRC(c4fff4a9) SHA1(4b593ace201fe7f5a00b5cd7f4e8fc3f8dd4ceed) )
	ROM_LOAD16_BYTE( "sf2_38c.bin",   0x40001, 0x20000, CRC(8210fc0e) SHA1(7d6cfb99afa89d0e6e991d9f7c1808b740def125) )
	ROM_LOAD16_BYTE( "sf2_28c.bin",   0x80000, 0x20000, CRC(6eddd5e8) SHA1(62bd1c2fc0321809421c9a592f691b5b1a1d8807) )
	ROM_LOAD16_BYTE( "sf2_35c.bin",   0x80001, 0x20000, CRC(6bcb404c) SHA1(b5f24556c633c521aadd94e016d78db6922e3dfa) )
	ROM_LOAD16_BYTE( "sf2_29a.bin",   0xc0000, 0x20000, CRC(bb4af315) SHA1(75f0827f4f7e9f292add46467f8d4fe19b2514c9) )
	ROM_LOAD16_BYTE( "sf2_36a.bin",   0xc0001, 0x20000, CRC(c02a13eb) SHA1(b807cc495bff3f95d03b061fc629c95f965cb6d8) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "sf2_06.bin", 0x000000, 0x80000, CRC(22c9cc8e) SHA1(b9194fb337b30502c1c9501cd6c64ae4035544d4) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx02.rom */
	ROMX_LOAD( "sf2_08.bin", 0x000002, 0x80000, CRC(57213be8) SHA1(3759b851ac0904ec79cbb67a2264d384b6f2f9f9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx04.rom */
	ROMX_LOAD( "sf2_05.bin", 0x000004, 0x80000, CRC(ba529b4f) SHA1(520840d727161cf09ca784919fa37bc9b54cc3ce) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx01.rom */
	ROMX_LOAD( "sf2_07.bin", 0x000006, 0x80000, CRC(4b1b33a8) SHA1(2360cff890551f76775739e2d6563858bff80e41) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx03.rom */
	ROMX_LOAD( "sf2_15.bin", 0x200000, 0x80000, CRC(2c7e2229) SHA1(357c2275af9133fd0bd6fbb1fa9ad5e0b490b3a2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx11.rom */
	ROMX_LOAD( "sf2_17.bin", 0x200002, 0x80000, CRC(b5548f17) SHA1(baa92b91cf616bc9e2a8a66adc777ffbf962a51b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx13.rom */
	ROMX_LOAD( "sf2_14.bin", 0x200004, 0x80000, CRC(14b84312) SHA1(2eea16673e60ba7a10bd4d8f6c217bb2441a5b0e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx10.rom */
	ROMX_LOAD( "sf2_16.bin", 0x200006, 0x80000, CRC(5e9cd89a) SHA1(f787aab98668d4c2c54fc4ba677c0cb808e4f31e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx12.rom */
	ROMX_LOAD( "sf2_25.bin", 0x400000, 0x80000, CRC(994bfa58) SHA1(5669b845f624b10e7be56bfc89b76592258ce48b) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx21.rom */
	ROMX_LOAD( "sf2_27.bin", 0x400002, 0x80000, CRC(3e66ad9d) SHA1(9af9df0826988872662753e9717c48d46f2974b0) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx23.rom */
	ROMX_LOAD( "sf2_24.bin", 0x400004, 0x80000, CRC(c1befaa8) SHA1(a6a7f4725e52678cbd8d557285c01cdccb2c2602) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx20.rom */
	ROMX_LOAD( "sf2_26.bin", 0x400006, 0x80000, CRC(0627c831) SHA1(f9a92d614e8877d648449de2612fc8b43c85e4c2) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2gfx22.rom */

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "sf2_09.bin",    0x00000, 0x08000, CRC(a4823a1b) SHA1(7b6bf59dfd578bfbbdb64c27988796783442d659) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sf2_18.bin",       0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "sf2_19.bin",       0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( 3wonders )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "rte.30a",      0x00000, 0x20000, CRC(ef5b8b33) SHA1(2313168e5f10505ceece5fdaada0d30df3ca146c) )
	ROM_LOAD16_BYTE( "rte.35a",      0x00001, 0x20000, CRC(7d705529) SHA1(b456629b5755b701cca8a438d24957367a260ec5) )
	ROM_LOAD16_BYTE( "rte.31a",      0x40000, 0x20000, CRC(32835e5e) SHA1(9ec530561030a75a1283ff2aacc21e55613b682b) )
	ROM_LOAD16_BYTE( "rte.36a",      0x40001, 0x20000, CRC(7637975f) SHA1(56935032eebd3e1c5059f6842b97001dae0aa55f) )
	ROM_LOAD16_BYTE( "3wonders.28",  0x80000, 0x20000, CRC(054137c8) SHA1(e4c406e0a32198323a5931093fbaa6836510b8ad) )
	ROM_LOAD16_BYTE( "3wonders.33",  0x80001, 0x20000, CRC(7264cb1b) SHA1(b367acb9f6579569321ecaa98a14e29dd775b9db) )
	ROM_LOAD16_BYTE( "rte.29a",      0xc0000, 0x20000, CRC(cddaa919) SHA1(0c98e95ad5033d2c5ade7651243e7ccdb4e35463) )
	ROM_LOAD16_BYTE( "rte.34a",      0xc0001, 0x20000, CRC(ed52e7e5) SHA1(352433ae484967d26376141e3a8a0f968b98fde6) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "3wonders.05",  0x000000, 0x80000, CRC(86aef804) SHA1(723927ef3bf992d12395c52db051ece7bf57d5e5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.07",  0x000002, 0x80000, CRC(4f057110) SHA1(b7d35c883a74cf4bfb242d9f15a0e40ed1ec111f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.01",  0x000004, 0x80000, CRC(902489d0) SHA1(748ba416a8b9343059a3e7d8b93f02162feb1d0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.03",  0x000006, 0x80000, CRC(e35ce720) SHA1(6c1a87a1f819bdc20408b5a7823cf35a79d34110) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.06",  0x200000, 0x80000, CRC(13cb0e7c) SHA1(e429d594d9a7ff4cc6306e2796a9d6ad0fa25569) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.08",  0x200002, 0x80000, CRC(1f055014) SHA1(d64f5be9bb2ef761ca9b2e797dbc3554cf996a79) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.02",  0x200004, 0x80000, CRC(e9a034f4) SHA1(deb4cb5886705380b57d4fe9b9bf3c032e1d6227) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.04",  0x200006, 0x80000, CRC(df0eea8b) SHA1(5afa05654cccb0504bd44569d42fd68f08fd172f) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "3wonders.09",   0x00000, 0x08000, CRC(abfca165) SHA1(428069d3bdc45775854cd0e8abe447f134fe5492) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "3wonders.18",  0x00000, 0x20000, CRC(26b211ab) SHA1(0ea03fdd9edff41eacfc52aa9e0421c10968356b) )
	ROM_LOAD( "3wonders.19",  0x20000, 0x20000, CRC(dbe64ad0) SHA1(09f2ad522fe75d7bcca094b8c6696c3733b539d5) )
ROM_END

ROM_START( 3wonderu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "3wonders.30",  0x00000, 0x20000, CRC(0b156fd8) SHA1(1ec811cd7cbd12066f876db7255394e754ceb25e) )
	ROM_LOAD16_BYTE( "3wonders.35",  0x00001, 0x20000, CRC(57350bf4) SHA1(33e8685cce82eee7bcb7c2787318a130764e97e2) )
	ROM_LOAD16_BYTE( "3wonders.31",  0x40000, 0x20000, CRC(0e723fcc) SHA1(91eeab6376a5aa852152af9920aef60bc7c689dd) )
	ROM_LOAD16_BYTE( "3wonders.36",  0x40001, 0x20000, CRC(523a45dc) SHA1(6d6743803016fa5ba713e0d6f61affce8a3255ec) )
	ROM_LOAD16_BYTE( "3wonders.28",  0x80000, 0x20000, CRC(054137c8) SHA1(e4c406e0a32198323a5931093fbaa6836510b8ad) )
	ROM_LOAD16_BYTE( "3wonders.33",  0x80001, 0x20000, CRC(7264cb1b) SHA1(b367acb9f6579569321ecaa98a14e29dd775b9db) )
	ROM_LOAD16_BYTE( "3wonders.29",  0xc0000, 0x20000, CRC(37ba3e20) SHA1(a128b1a17639b06a4fd8acffe0357f1dbd1d4fe9) )
	ROM_LOAD16_BYTE( "3wonders.34",  0xc0001, 0x20000, CRC(f99f46c0) SHA1(cda24a6baa3f861e7078fb2fa91328cc1cddc866) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "3wonders.05",  0x000000, 0x80000, CRC(86aef804) SHA1(723927ef3bf992d12395c52db051ece7bf57d5e5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.07",  0x000002, 0x80000, CRC(4f057110) SHA1(b7d35c883a74cf4bfb242d9f15a0e40ed1ec111f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.01",  0x000004, 0x80000, CRC(902489d0) SHA1(748ba416a8b9343059a3e7d8b93f02162feb1d0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.03",  0x000006, 0x80000, CRC(e35ce720) SHA1(6c1a87a1f819bdc20408b5a7823cf35a79d34110) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.06",  0x200000, 0x80000, CRC(13cb0e7c) SHA1(e429d594d9a7ff4cc6306e2796a9d6ad0fa25569) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.08",  0x200002, 0x80000, CRC(1f055014) SHA1(d64f5be9bb2ef761ca9b2e797dbc3554cf996a79) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.02",  0x200004, 0x80000, CRC(e9a034f4) SHA1(deb4cb5886705380b57d4fe9b9bf3c032e1d6227) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.04",  0x200006, 0x80000, CRC(df0eea8b) SHA1(5afa05654cccb0504bd44569d42fd68f08fd172f) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "3wonders.09",   0x00000, 0x08000, CRC(abfca165) SHA1(428069d3bdc45775854cd0e8abe447f134fe5492) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "3wonders.18",  0x00000, 0x20000, CRC(26b211ab) SHA1(0ea03fdd9edff41eacfc52aa9e0421c10968356b) )
	ROM_LOAD( "3wonders.19",  0x20000, 0x20000, CRC(dbe64ad0) SHA1(09f2ad522fe75d7bcca094b8c6696c3733b539d5) )
ROM_END

ROM_START( wonder3 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "rtj36.bin",    0x00000, 0x20000, CRC(e3741247) SHA1(4deb0f667697631693fbefddaeb8cf98fd0b90ce) )
	ROM_LOAD16_BYTE( "rtj42.bin",    0x00001, 0x20000, CRC(b4baa117) SHA1(44486b3d50f9b0a8c32c2c2dc5f1a046aface7b6) )
	ROM_LOAD16_BYTE( "rtj37.bin",    0x40000, 0x20000, CRC(a1f677b0) SHA1(e1511ea0fa4a689d1355119ac37c075192880dde) )
	ROM_LOAD16_BYTE( "rtj43.bin",    0x40001, 0x20000, CRC(85337a47) SHA1(0a247aa56c5cc17b1e888df7b502f65e88715469) )

	ROM_LOAD16_BYTE( "3wonders.28",  0x80000, 0x20000, CRC(054137c8) SHA1(e4c406e0a32198323a5931093fbaa6836510b8ad) )
	ROM_LOAD16_BYTE( "3wonders.33",  0x80001, 0x20000, CRC(7264cb1b) SHA1(b367acb9f6579569321ecaa98a14e29dd775b9db) )
	ROM_LOAD16_BYTE( "rtj35.bin",    0xc0000, 0x20000, CRC(e72f9ea3) SHA1(c63df200416bd61af73e8589204f7daef743041e) )
	ROM_LOAD16_BYTE( "rtj41.bin",    0xc0001, 0x20000, CRC(a11ee998) SHA1(b892398e2ff4e40e51b858cfdbce866a75c670e6) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "3wonders.05",  0x000000, 0x80000, CRC(86aef804) SHA1(723927ef3bf992d12395c52db051ece7bf57d5e5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.07",  0x000002, 0x80000, CRC(4f057110) SHA1(b7d35c883a74cf4bfb242d9f15a0e40ed1ec111f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.01",  0x000004, 0x80000, CRC(902489d0) SHA1(748ba416a8b9343059a3e7d8b93f02162feb1d0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.03",  0x000006, 0x80000, CRC(e35ce720) SHA1(6c1a87a1f819bdc20408b5a7823cf35a79d34110) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.06",  0x200000, 0x80000, CRC(13cb0e7c) SHA1(e429d594d9a7ff4cc6306e2796a9d6ad0fa25569) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.08",  0x200002, 0x80000, CRC(1f055014) SHA1(d64f5be9bb2ef761ca9b2e797dbc3554cf996a79) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.02",  0x200004, 0x80000, CRC(e9a034f4) SHA1(deb4cb5886705380b57d4fe9b9bf3c032e1d6227) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3wonders.04",  0x200006, 0x80000, CRC(df0eea8b) SHA1(5afa05654cccb0504bd44569d42fd68f08fd172f) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "rt23.bin",      0x00000, 0x08000, CRC(7d5a77a7) SHA1(4f2a6534d7a9e518061102c1dd7d7aca39600c33) )    /* could have one bad byte */
	ROM_CONTINUE(              0x10000, 0x08000 )                /* (compare with US version, */
														/* which is verified to be correct) */
	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "3wonders.18",  0x00000, 0x20000, CRC(26b211ab) SHA1(0ea03fdd9edff41eacfc52aa9e0421c10968356b) )
	ROM_LOAD( "3wonders.19",  0x20000, 0x20000, CRC(dbe64ad0) SHA1(09f2ad522fe75d7bcca094b8c6696c3733b539d5) )
ROM_END

ROM_START( kod )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "kod30.rom",    0x00000, 0x20000, CRC(c7414fd4) SHA1(37d27fbe7c617a26b53bfdfcd532cf573d42f33b) )
	ROM_LOAD16_BYTE( "kod37.rom",    0x00001, 0x20000, CRC(a5bf40d2) SHA1(cd34dbeabd0974709411579e669f01d0d799c2a1) )
	ROM_LOAD16_BYTE( "kod31.rom",    0x40000, 0x20000, CRC(1fffc7bd) SHA1(822c9ad996ca51a99a2bb1fe08fa19e18413030d) )
	ROM_LOAD16_BYTE( "kod38.rom",    0x40001, 0x20000, CRC(89e57a82) SHA1(aad35f86a8b1b7e3a0b5f3e6efd0e844b3d3d82f) )
	ROM_LOAD16_BYTE( "kod28.rom",    0x80000, 0x20000, CRC(9367bcd9) SHA1(8243b4b9bb9756f3fa726717e19a166cb2f5b50a) )
	ROM_LOAD16_BYTE( "kod35.rom",    0x80001, 0x20000, CRC(4ca6a48a) SHA1(9d440ecd8d2d0e293fecf64ca3915252b94e7aef) )
	ROM_LOAD16_BYTE( "kod29.rom",    0xc0000, 0x20000, CRC(6a0ba878) SHA1(82e4037d73889a76b0cdc7a4f8e77e585d38e56e) )
	ROM_LOAD16_BYTE( "kod36.rom",    0xc0001, 0x20000, CRC(b509b39d) SHA1(6023855e54b170e55abf0f607600031e19e5e722) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kod02.rom",    0x000000, 0x80000, CRC(e45b8701) SHA1(604e39e455e81695ee4f899f102d0bcd789cedd0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod04.rom",    0x000002, 0x80000, CRC(a7750322) SHA1(3c583496a53cd64edf377db35f7f40f02b59b7e7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod01.rom",    0x000004, 0x80000, CRC(5f74bf78) SHA1(b7c43eea9bf77a0fb571dcd53f8be719e6655fd9) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod03.rom",    0x000006, 0x80000, CRC(5e5303bf) SHA1(d9f90b898ffdf4398b2bbeb48247f06f728e7c00) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod11.rom",    0x200000, 0x80000, CRC(113358f3) SHA1(9d98eafa74a046f65bf3847fe1d88ea1b0c82b0c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod13.rom",    0x200002, 0x80000, CRC(38853c44) SHA1(a6e552fb0138a76a7878b90d202904e2b44ae7ec) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod10.rom",    0x200004, 0x80000, CRC(9ef36604) SHA1(b42ca0a910b65e1e7bb6e7d734e853ce67e821bf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod12.rom",    0x200006, 0x80000, CRC(402b9b4f) SHA1(4c11976976eadf1ad293b31b0a4d047d05032b06) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kod09.rom",     0x00000, 0x08000, CRC(f5514510) SHA1(07e9c836adf9ef2f7e7729e99015f71e3b5f16e0) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kod18.rom",    0x00000, 0x20000, CRC(69ecb2c8) SHA1(fadf266b6b20bd6329a3e638918c5a3106413476) )
	ROM_LOAD( "kod19.rom",    0x20000, 0x20000, CRC(02d851c1) SHA1(c959a6fc3e7d893557f319debae91f28471f4be2) )
ROM_END

ROM_START( kodu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "kdu-30b.bin",  0x00000, 0x20000, CRC(825817f9) SHA1(250f61effcbe59f8b70baaf26eb8aef419fed66b) )
	ROM_LOAD16_BYTE( "kdu-37b.bin",  0x00001, 0x20000, CRC(d2422dfb) SHA1(6e369a62012f3c480755b700d4d4f4c112c79483) )
	ROM_LOAD16_BYTE( "kdu-31b.bin",  0x40000, 0x20000, CRC(9af36039) SHA1(f2645178a042689a387f916b4ecd7d1d859d758a) )
	ROM_LOAD16_BYTE( "kdu-38b.bin",  0x40001, 0x20000, CRC(be8405a1) SHA1(8d4f9a0489dc4b2971b20170713284151bc10eb7) )
	ROM_LOAD16_BYTE( "kod28.rom",    0x80000, 0x20000, CRC(9367bcd9) SHA1(8243b4b9bb9756f3fa726717e19a166cb2f5b50a) )
	ROM_LOAD16_BYTE( "kod35.rom",    0x80001, 0x20000, CRC(4ca6a48a) SHA1(9d440ecd8d2d0e293fecf64ca3915252b94e7aef) )
	ROM_LOAD16_BYTE( "kd-29.bin",    0xc0000, 0x20000, CRC(0360fa72) SHA1(274769c8717a874397cf37369e3ef80a682d9ef2) )
	ROM_LOAD16_BYTE( "kd-36a.bin",   0xc0001, 0x20000, CRC(95a3cef8) SHA1(9b75c1ed0eafacc230197ffd9b81e0c8f4f2c464) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kod02.rom",    0x000000, 0x80000, CRC(e45b8701) SHA1(604e39e455e81695ee4f899f102d0bcd789cedd0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod04.rom",    0x000002, 0x80000, CRC(a7750322) SHA1(3c583496a53cd64edf377db35f7f40f02b59b7e7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod01.rom",    0x000004, 0x80000, CRC(5f74bf78) SHA1(b7c43eea9bf77a0fb571dcd53f8be719e6655fd9) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod03.rom",    0x000006, 0x80000, CRC(5e5303bf) SHA1(d9f90b898ffdf4398b2bbeb48247f06f728e7c00) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod11.rom",    0x200000, 0x80000, CRC(113358f3) SHA1(9d98eafa74a046f65bf3847fe1d88ea1b0c82b0c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod13.rom",    0x200002, 0x80000, CRC(38853c44) SHA1(a6e552fb0138a76a7878b90d202904e2b44ae7ec) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod10.rom",    0x200004, 0x80000, CRC(9ef36604) SHA1(b42ca0a910b65e1e7bb6e7d734e853ce67e821bf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod12.rom",    0x200006, 0x80000, CRC(402b9b4f) SHA1(4c11976976eadf1ad293b31b0a4d047d05032b06) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kd09.bin",      0x00000, 0x08000, CRC(bac6ec26) SHA1(6cbb6d55660150ae3f5270e023328275ee1bbf50) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kd18.bin",    0x00000, 0x20000, CRC(4c63181d) SHA1(270f27534a95cb0be3ff3f9ca71c502320d8090b) )
	ROM_LOAD( "kd19.bin",    0x20000, 0x20000, CRC(92941b80) SHA1(5fa7c2793e6febee54a83042d118ddd4f2b7d127) )
ROM_END

ROM_START( kodj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "kd30.bin",    0x00000, 0x20000, CRC(ebc788ad) SHA1(f4a297e409fcdbb2c15a13b39a16e4a83e7e060b) )
	ROM_LOAD16_BYTE( "kd37.bin",    0x00001, 0x20000, CRC(e55c3529) SHA1(a5254895499a53b4fbaac6fd50464b9e08175b8d) )
	ROM_LOAD16_BYTE( "kd31.bin",    0x40000, 0x20000, CRC(c710d722) SHA1(a2e9b84d3e7d835a910ab9f584bdc64c2559995a) )
	ROM_LOAD16_BYTE( "kd38.bin",    0x40001, 0x20000, CRC(57d6ed3a) SHA1(a47da5068723c8e16ed458fbfa3e3db57b32d87d) )
	ROM_LOAD16_WORD_SWAP("kd33.bin", 0x80000, 0x80000, CRC(9bd7ad4b) SHA1(7bece5d408fd13116bd5518014b632ecc9a2feaa) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kod02.rom",    0x000000, 0x80000, CRC(e45b8701) SHA1(604e39e455e81695ee4f899f102d0bcd789cedd0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod04.rom",    0x000002, 0x80000, CRC(a7750322) SHA1(3c583496a53cd64edf377db35f7f40f02b59b7e7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod01.rom",    0x000004, 0x80000, CRC(5f74bf78) SHA1(b7c43eea9bf77a0fb571dcd53f8be719e6655fd9) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod03.rom",    0x000006, 0x80000, CRC(5e5303bf) SHA1(d9f90b898ffdf4398b2bbeb48247f06f728e7c00) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod11.rom",    0x200000, 0x80000, CRC(113358f3) SHA1(9d98eafa74a046f65bf3847fe1d88ea1b0c82b0c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod13.rom",    0x200002, 0x80000, CRC(38853c44) SHA1(a6e552fb0138a76a7878b90d202904e2b44ae7ec) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod10.rom",    0x200004, 0x80000, CRC(9ef36604) SHA1(b42ca0a910b65e1e7bb6e7d734e853ce67e821bf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod12.rom",    0x200006, 0x80000, CRC(402b9b4f) SHA1(4c11976976eadf1ad293b31b0a4d047d05032b06) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kd09.bin",      0x00000, 0x08000, CRC(bac6ec26) SHA1(6cbb6d55660150ae3f5270e023328275ee1bbf50) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kd18.bin",    0x00000, 0x20000, CRC(4c63181d) SHA1(270f27534a95cb0be3ff3f9ca71c502320d8090b) )
	ROM_LOAD( "kd19.bin",    0x20000, 0x20000, CRC(92941b80) SHA1(5fa7c2793e6febee54a83042d118ddd4f2b7d127) )
ROM_END

ROM_START( kodb )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "kod.17",    0x00000, 0x080000, CRC(036dd74c) SHA1(489344e56863429e86b4c362b82d89819c1d6afb) )
	ROM_LOAD16_BYTE( "kod.18",    0x00001, 0x080000, CRC(3e4b7295) SHA1(3245640bae7d141238051dfe5c7683d05c6d3848) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kod02.rom",    0x000000, 0x80000, CRC(e45b8701) SHA1(604e39e455e81695ee4f899f102d0bcd789cedd0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod04.rom",    0x000002, 0x80000, CRC(a7750322) SHA1(3c583496a53cd64edf377db35f7f40f02b59b7e7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod01.rom",    0x000004, 0x80000, CRC(5f74bf78) SHA1(b7c43eea9bf77a0fb571dcd53f8be719e6655fd9) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod03.rom",    0x000006, 0x80000, CRC(5e5303bf) SHA1(d9f90b898ffdf4398b2bbeb48247f06f728e7c00) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod11.rom",    0x200000, 0x80000, CRC(113358f3) SHA1(9d98eafa74a046f65bf3847fe1d88ea1b0c82b0c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod13.rom",    0x200002, 0x80000, CRC(38853c44) SHA1(a6e552fb0138a76a7878b90d202904e2b44ae7ec) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod10.rom",    0x200004, 0x80000, CRC(9ef36604) SHA1(b42ca0a910b65e1e7bb6e7d734e853ce67e821bf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kod12.rom",    0x200006, 0x80000, CRC(402b9b4f) SHA1(4c11976976eadf1ad293b31b0a4d047d05032b06) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kod.15",        0x00000, 0x08000, CRC(01cae60c) SHA1(b2cdd883fd859f0b701230831aca1f1a74ad6087) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kd18.bin",    0x00000, 0x20000, CRC(4c63181d) SHA1(270f27534a95cb0be3ff3f9ca71c502320d8090b) )
	ROM_LOAD( "kd19.bin",    0x20000, 0x20000, CRC(92941b80) SHA1(5fa7c2793e6febee54a83042d118ddd4f2b7d127) )
ROM_END

ROM_START( captcomm )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "cce_23d.rom",  0x000000, 0x80000, CRC(19c58ece) SHA1(6e23e87db29b2c5698b7cead99d1106a2e190648) )
	ROM_LOAD16_WORD_SWAP( "cc_22d.rom",   0x080000, 0x80000, CRC(a91949b7) SHA1(c027af89cd8f6bd3aaed61114582322c42e0c74f) )
	ROM_LOAD16_BYTE( "cc_24d.rom",        0x100000, 0x20000, CRC(680e543f) SHA1(cfa963ab6329f615807db213bf53841860ed3149) )
	ROM_LOAD16_BYTE( "cc_28d.rom",        0x100001, 0x20000, CRC(8820039f) SHA1(d68ce0b34ade75b8c5214168b2b1e0cdff45cd52) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "gfx_01.rom",   0x000000, 0x80000, CRC(7261d8ba) SHA1(4b66292e42d20d0b79a756f0e445492ddb9c6bbc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_03.rom",   0x000002, 0x80000, CRC(6a60f949) SHA1(87391ff92abaf3e451f70d789a938cffbd1fd222) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_02.rom",   0x000004, 0x80000, CRC(00637302) SHA1(2c554b59cceec2de67a9a4bc6281fe846d3c8cd2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_04.rom",   0x000006, 0x80000, CRC(cc87cf61) SHA1(7fb1f49494cc1a08aded20754bb0cefb1c323198) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_05.rom",   0x200000, 0x80000, CRC(28718bed) SHA1(dfdc4dd14dc609783bad94d608a9e9b137dea944) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_07.rom",   0x200002, 0x80000, CRC(d4acc53a) SHA1(d03282ebbde362e679cc97f772aa9baf163d7606) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_06.rom",   0x200004, 0x80000, CRC(0c69f151) SHA1(a170b8e568439e4a26d84376d53560e4248e4e2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_08.rom",   0x200006, 0x80000, CRC(1f9ebb97) SHA1(023d00cb7b6a52d1b29e2052abe08ef34cb0c55c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "cc_09.rom",     0x00000, 0x08000, CRC(698e8b58) SHA1(b7a3d905a7ed2c430426ca2e185e3d7e75e752a1) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "cc_18.rom",    0x00000, 0x20000, CRC(6de2c2db) SHA1(9a1eaba8d104f59a5e61f89679bb5de0c0c64364) )
	ROM_LOAD( "cc_19.rom",    0x20000, 0x20000, CRC(b99091ae) SHA1(b19197c7ad3aeaf5f41c26bf853b0c9b502ecfca) )
ROM_END

ROM_START( captcomu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "23b",   0x000000, 0x80000, CRC(03da44fd) SHA1(0bf382933b4b44082bbaf63e96acd83ab8808a34) )
	ROM_LOAD16_WORD_SWAP( "22c",   0x080000, 0x80000, CRC(9b82a052) SHA1(8247fe45fea8c47072a66d6707202bcdb8c62923) )
	ROM_LOAD16_BYTE( "24b",        0x100000, 0x20000, CRC(84ff99b2) SHA1(5b02c91f3d0f8fb46db9596136b683f5a22dc15f) )
	ROM_LOAD16_BYTE( "28b",        0x100001, 0x20000, CRC(fbcec223) SHA1(daf484baece5b3a11f3dcabb758b8bdd736a1fb6) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "gfx_01.rom",   0x000000, 0x80000, CRC(7261d8ba) SHA1(4b66292e42d20d0b79a756f0e445492ddb9c6bbc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_03.rom",   0x000002, 0x80000, CRC(6a60f949) SHA1(87391ff92abaf3e451f70d789a938cffbd1fd222) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_02.rom",   0x000004, 0x80000, CRC(00637302) SHA1(2c554b59cceec2de67a9a4bc6281fe846d3c8cd2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_04.rom",   0x000006, 0x80000, CRC(cc87cf61) SHA1(7fb1f49494cc1a08aded20754bb0cefb1c323198) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_05.rom",   0x200000, 0x80000, CRC(28718bed) SHA1(dfdc4dd14dc609783bad94d608a9e9b137dea944) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_07.rom",   0x200002, 0x80000, CRC(d4acc53a) SHA1(d03282ebbde362e679cc97f772aa9baf163d7606) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_06.rom",   0x200004, 0x80000, CRC(0c69f151) SHA1(a170b8e568439e4a26d84376d53560e4248e4e2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_08.rom",   0x200006, 0x80000, CRC(1f9ebb97) SHA1(023d00cb7b6a52d1b29e2052abe08ef34cb0c55c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "cc_09.rom",     0x00000, 0x08000, CRC(698e8b58) SHA1(b7a3d905a7ed2c430426ca2e185e3d7e75e752a1) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "cc_18.rom",    0x00000, 0x20000, CRC(6de2c2db) SHA1(9a1eaba8d104f59a5e61f89679bb5de0c0c64364) )
	ROM_LOAD( "cc_19.rom",    0x20000, 0x20000, CRC(b99091ae) SHA1(b19197c7ad3aeaf5f41c26bf853b0c9b502ecfca) )
ROM_END

ROM_START( captcomj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "cc23.bin",   0x000000, 0x80000, CRC(5b482b62) SHA1(c871aa1eb9ecc117c3079995d1f5212193bd2e12) )
	ROM_LOAD16_WORD_SWAP( "cc22.bin",   0x080000, 0x80000, CRC(0fd34195) SHA1(fb2b9a53af43507f13c4f94eaebbf0b538b2e754) )
	ROM_LOAD16_BYTE( "cc24.bin",        0x100000, 0x20000, CRC(3a794f25) SHA1(7f3722a4ef0c1d7acb73e6bac9dd6ae7b35e6374) )
	ROM_LOAD16_BYTE( "cc28.bin",        0x100001, 0x20000, CRC(fc3c2906) SHA1(621c3b79b6fdea1665bb316eb539e5916e890656) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "gfx_01.rom",   0x000000, 0x80000, CRC(7261d8ba) SHA1(4b66292e42d20d0b79a756f0e445492ddb9c6bbc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_03.rom",   0x000002, 0x80000, CRC(6a60f949) SHA1(87391ff92abaf3e451f70d789a938cffbd1fd222) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_02.rom",   0x000004, 0x80000, CRC(00637302) SHA1(2c554b59cceec2de67a9a4bc6281fe846d3c8cd2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_04.rom",   0x000006, 0x80000, CRC(cc87cf61) SHA1(7fb1f49494cc1a08aded20754bb0cefb1c323198) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_05.rom",   0x200000, 0x80000, CRC(28718bed) SHA1(dfdc4dd14dc609783bad94d608a9e9b137dea944) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_07.rom",   0x200002, 0x80000, CRC(d4acc53a) SHA1(d03282ebbde362e679cc97f772aa9baf163d7606) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_06.rom",   0x200004, 0x80000, CRC(0c69f151) SHA1(a170b8e568439e4a26d84376d53560e4248e4e2f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "gfx_08.rom",   0x200006, 0x80000, CRC(1f9ebb97) SHA1(023d00cb7b6a52d1b29e2052abe08ef34cb0c55c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "cc_09.rom",     0x00000, 0x08000, CRC(698e8b58) SHA1(b7a3d905a7ed2c430426ca2e185e3d7e75e752a1) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "cc_18.rom",    0x00000, 0x20000, CRC(6de2c2db) SHA1(9a1eaba8d104f59a5e61f89679bb5de0c0c64364) )
	ROM_LOAD( "cc_19.rom",    0x20000, 0x20000, CRC(b99091ae) SHA1(b19197c7ad3aeaf5f41c26bf853b0c9b502ecfca) )
ROM_END

ROM_START( knights )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "kr_23e.rom",   0x00000, 0x80000, CRC(1b3997eb) SHA1(724b68eff319fcdf0dd3bc1eb6662996c1f6ecd9) )
	ROM_LOAD16_WORD_SWAP( "kr_22.rom",    0x80000, 0x80000, CRC(d0b671a9) SHA1(9865472c5fc3f617345e23b5de5a9ba177945b5a) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kr_gfx1.rom",  0x000000, 0x80000, CRC(9e36c1a4) SHA1(772daae74e119371dfb76fde9775bda78a8ba125) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx3.rom",  0x000002, 0x80000, CRC(c5832cae) SHA1(a188cf401cd3a2909b377d3059f14d22ec3b0643) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx2.rom",  0x000004, 0x80000, CRC(f095be2d) SHA1(0427d1574062f277a9d04440019d5638b05de561) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx4.rom",  0x000006, 0x80000, CRC(179dfd96) SHA1(b1844e69da7ab13474da569978d5b47deb8eb2be) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx5.rom",  0x200000, 0x80000, CRC(1f4298d2) SHA1(4b162a7f649b0bcd676f8ca0c5eee9a1250d6452) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx7.rom",  0x200002, 0x80000, CRC(37fa8751) SHA1(b88b39d1f08621f15a5620095aef998346fa9891) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx6.rom",  0x200004, 0x80000, CRC(0200bc3d) SHA1(c900b1be2b4e49b951e5c1e3fd1e19d21b82986e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx8.rom",  0x200006, 0x80000, CRC(0bb2b4e7) SHA1(983b800925d58e4aeb4e5105f93ed5faf66d009c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kr_09.rom",     0x00000, 0x08000, CRC(5e44d9ee) SHA1(47a7503321be8d52b5c44af838e3bb82ee15a415) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kr_18.rom",    0x00000, 0x20000, CRC(da69d15f) SHA1(9616207e693bae85705f786cef60b9f6951b5067) )
	ROM_LOAD( "kr_19.rom",    0x20000, 0x20000, CRC(bfc654e9) SHA1(01b3d92e4dedf55ea3933d387c7ddb9ba2549773) )
ROM_END

ROM_START( knightsu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "kru23.rom",    0x00000, 0x80000, CRC(252bc2ba) SHA1(4f4901c253bd64bbe68ea01994ae663fe2ccd056) )
	ROM_LOAD16_WORD_SWAP( "kr_22.rom",    0x80000, 0x80000, CRC(d0b671a9) SHA1(9865472c5fc3f617345e23b5de5a9ba177945b5a) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kr_gfx1.rom",  0x000000, 0x80000, CRC(9e36c1a4) SHA1(772daae74e119371dfb76fde9775bda78a8ba125) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx3.rom",  0x000002, 0x80000, CRC(c5832cae) SHA1(a188cf401cd3a2909b377d3059f14d22ec3b0643) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx2.rom",  0x000004, 0x80000, CRC(f095be2d) SHA1(0427d1574062f277a9d04440019d5638b05de561) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx4.rom",  0x000006, 0x80000, CRC(179dfd96) SHA1(b1844e69da7ab13474da569978d5b47deb8eb2be) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx5.rom",  0x200000, 0x80000, CRC(1f4298d2) SHA1(4b162a7f649b0bcd676f8ca0c5eee9a1250d6452) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx7.rom",  0x200002, 0x80000, CRC(37fa8751) SHA1(b88b39d1f08621f15a5620095aef998346fa9891) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx6.rom",  0x200004, 0x80000, CRC(0200bc3d) SHA1(c900b1be2b4e49b951e5c1e3fd1e19d21b82986e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx8.rom",  0x200006, 0x80000, CRC(0bb2b4e7) SHA1(983b800925d58e4aeb4e5105f93ed5faf66d009c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kr_09.rom",     0x00000, 0x08000, CRC(5e44d9ee) SHA1(47a7503321be8d52b5c44af838e3bb82ee15a415) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kr_18.rom",    0x00000, 0x20000, CRC(da69d15f) SHA1(9616207e693bae85705f786cef60b9f6951b5067) )
	ROM_LOAD( "kr_19.rom",    0x20000, 0x20000, CRC(bfc654e9) SHA1(01b3d92e4dedf55ea3933d387c7ddb9ba2549773) )
ROM_END

ROM_START( knightsj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "krj30.bin",   0x00000, 0x20000, CRC(ad3d1a8e) SHA1(327f9e818f1500836fc549afeffbb2a3c5aafe8c) )
	ROM_LOAD16_BYTE( "krj37.bin",   0x00001, 0x20000, CRC(e694a491) SHA1(5a4d27c879c10032c49880019501de3e45ab1b35) )
	ROM_LOAD16_BYTE( "krj31.bin",   0x40000, 0x20000, CRC(85596094) SHA1(74ad294de63aa6b60aa8b885c45c3d41a07ce19a) )
	ROM_LOAD16_BYTE( "krj38.bin",   0x40001, 0x20000, CRC(9198bf8f) SHA1(aa3610600286ab25ce81705ea1319d42e7cc7f6c) )
	ROM_LOAD16_WORD_SWAP( "kr_22.rom",    0x80000, 0x80000, CRC(d0b671a9) SHA1(9865472c5fc3f617345e23b5de5a9ba177945b5a) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "kr_gfx1.rom",  0x000000, 0x80000, CRC(9e36c1a4) SHA1(772daae74e119371dfb76fde9775bda78a8ba125) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx3.rom",  0x000002, 0x80000, CRC(c5832cae) SHA1(a188cf401cd3a2909b377d3059f14d22ec3b0643) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx2.rom",  0x000004, 0x80000, CRC(f095be2d) SHA1(0427d1574062f277a9d04440019d5638b05de561) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx4.rom",  0x000006, 0x80000, CRC(179dfd96) SHA1(b1844e69da7ab13474da569978d5b47deb8eb2be) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx5.rom",  0x200000, 0x80000, CRC(1f4298d2) SHA1(4b162a7f649b0bcd676f8ca0c5eee9a1250d6452) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx7.rom",  0x200002, 0x80000, CRC(37fa8751) SHA1(b88b39d1f08621f15a5620095aef998346fa9891) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx6.rom",  0x200004, 0x80000, CRC(0200bc3d) SHA1(c900b1be2b4e49b951e5c1e3fd1e19d21b82986e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "kr_gfx8.rom",  0x200006, 0x80000, CRC(0bb2b4e7) SHA1(983b800925d58e4aeb4e5105f93ed5faf66d009c) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "kr_09.rom",     0x00000, 0x08000, CRC(5e44d9ee) SHA1(47a7503321be8d52b5c44af838e3bb82ee15a415) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "kr_18.rom",    0x00000, 0x20000, CRC(da69d15f) SHA1(9616207e693bae85705f786cef60b9f6951b5067) )
	ROM_LOAD( "kr_19.rom",    0x20000, 0x20000, CRC(bfc654e9) SHA1(01b3d92e4dedf55ea3933d387c7ddb9ba2549773) )
ROM_END

ROM_START( sf2ce )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "sf2ce.23",     0x000000, 0x80000, CRC(3f846b74) SHA1(c8d7a01b626771870123f1663a01a81f9c8fe582) )
	ROM_LOAD16_WORD_SWAP( "sf2ce.22",     0x080000, 0x80000, CRC(99f1cca4) SHA1(64111eba81d743fc3fd51d7a89cd0b2eefcc900d) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ceua )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "s92u-23a",     0x000000, 0x80000, CRC(ac44415b) SHA1(218f8b1886eb72b8547127042b5ae47600e18944) )
	ROM_LOAD16_WORD_SWAP( "sf2ce.22",     0x080000, 0x80000, CRC(99f1cca4) SHA1(64111eba81d743fc3fd51d7a89cd0b2eefcc900d) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ceub )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "s92-23b",      0x000000, 0x80000, CRC(996a3015) SHA1(fdf45da54b1c14478a60f2b86e37ffe32a98b135) )
	ROM_LOAD16_WORD_SWAP( "s92-22b",      0x080000, 0x80000, CRC(2bbe15ed) SHA1(a8e2edef62fa99c5ef701b28bfb6bc42f3af183d) )
	ROM_LOAD16_WORD_SWAP( "s92-21b",      0x100000, 0x80000, CRC(b383cb1c) SHA1(fd527d5b27a853758bc6ed0f4108f3c634484de6) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2ceuc )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "s92-23c",      0x000000, 0x80000, CRC(0a8b6aa2) SHA1(a19871271172119e1cf1ff47700bb1917b08514b) )
	ROM_LOAD16_WORD_SWAP( "s92-22c",      0x080000, 0x80000, CRC(5fd8630b) SHA1(f0ef9c5ab91a4b421fb4b1747eef99c964c15de3) )
	ROM_LOAD16_WORD_SWAP( "s92-21b",      0x100000, 0x80000, CRC(b383cb1c) SHA1(fd527d5b27a853758bc6ed0f4108f3c634484de6) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2cej )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "s92j_23b.bin", 0x000000, 0x80000, CRC(140876c5) SHA1(304630e6d8bae9f8d29090e05f7e013c7dafe9cc) )
	ROM_LOAD16_WORD_SWAP( "s92j_22b.bin", 0x080000, 0x80000, CRC(2fbb3bfe) SHA1(e364564a12022730c2c0d0e8fd435e2c30ef9410) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2rb )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD( "sf2d__23.rom", 0x000000, 0x80000, CRC(450532b0) SHA1(14d5ff44ce97247ef4c42147157856d16c5fb4b8) )
	ROM_LOAD16_WORD( "sf2d__22.rom", 0x080000, 0x80000, CRC(fe9d9cf5) SHA1(91afb25d8c0fd1a721f982cebf8fdf563fe11760) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2rb2 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "27.bin",    0x000000, 0x20000, CRC(40296ecd) SHA1(6006b9bc7e2e8ccec47f87b51791d3c0512620df) )
	ROM_LOAD16_BYTE( "31.bin",    0x000001, 0x20000, CRC(87954a41) SHA1(67225f180e1f954f0bebba49b618f793a973af14) )
	ROM_LOAD16_BYTE( "26.bin",    0x040000, 0x20000, CRC(a6974195) SHA1(f7e0fd43bd75229d49d5c330820bdc5c3b11ab03) )
	ROM_LOAD16_BYTE( "30.bin",    0x040001, 0x20000, CRC(8141fe32) SHA1(e6ea1ee331f674c64e63a776ad4e428f6081c79c) )
	ROM_LOAD16_BYTE( "25.bin",    0x080000, 0x20000, CRC(9ef8f772) SHA1(3ee271413521cc2d6ac9544e401ff38eff8a1347) )
	ROM_LOAD16_BYTE( "29.bin",    0x080001, 0x20000, CRC(7d9c479c) SHA1(a1195444caac5230a1f74f3444b024ceaf1d0667) )
	ROM_LOAD16_BYTE( "24.bin",    0x0c0000, 0x20000, CRC(93579684) SHA1(9052b46f635cae7843e9d37a601db0189a89e0f9) )
	ROM_LOAD16_BYTE( "28.bin",    0x0c0001, 0x20000, CRC(ff728865) SHA1(ad4522294ff2e02b594d960b45940a3e57a5d1ec) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",     0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2red )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "sf2red.23",    0x000000, 0x80000, CRC(40276abb) SHA1(a991661f5a1a3116445594bcfed3150e36971dd7) )
	ROM_LOAD16_WORD_SWAP( "sf2red.22",    0x080000, 0x80000, CRC(18daf387) SHA1(1a9e4c04ca54e8b33f19dd7bedbe05a200249701) )
	ROM_LOAD16_WORD_SWAP( "sf2red.21",    0x100000, 0x80000, CRC(52c486bb) SHA1(b7df7b10faa4c9a2f86ebf64cd63ac148d62dd09) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2v004 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "sf2v004.23",   0x000000, 0x80000, CRC(52d19f2c) SHA1(6a77b9244dc9b7d9a0ca8a642d4257cc944ac566) )
	ROM_LOAD16_WORD_SWAP( "sf2v004.22",   0x080000, 0x80000, CRC(4b26fde7) SHA1(48e3aacbf9147f2374a93e10f945291c87f24855) )
	ROM_LOAD16_WORD_SWAP( "sf2red.21",    0x100000, 0x80000, CRC(52c486bb) SHA1(b7df7b10faa4c9a2f86ebf64cd63ac148d62dd09) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2accp2 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "sf2ca-23.bin", 0x000000, 0x80000, CRC(36c3ba2f) SHA1(a3ddc479b725ddb3521757c8efa7aed125004997) )
	ROM_LOAD16_WORD_SWAP( "sf2ca-22.bin", 0x080000, 0x80000, CRC(0550453d) SHA1(f9efed86528dd10f142636278f098584d33ccde6) )
	ROM_LOAD16_WORD_SWAP( "sf2ca-21.bin", 0x100000, 0x40000, CRC(4c1c43ba) SHA1(16abce268373eddd7c3b93fe8d44b200a8c140fe) )
	/* ROM space ends at 13ffff, but the code checks 180ca6 and */
	/* crashes if it isn't 0 - protection? */

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m1 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "222e", 0x000000, 0x80000, CRC(1e20d0a3) SHA1(5e05b52fd938aff5190bca7e178705d7236aef66) )
	ROM_LOAD16_BYTE( "196e", 0x000001, 0x80000, CRC(88cc38a3) SHA1(6049962f943bd37748a9531cc3254e8b59326eac) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m2 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "ch222esp", 0x000000, 0x80000, CRC(9e6d058a) SHA1(8c9adca7b65dc929c325c0a62304d24dc0902c08) )
	ROM_LOAD16_BYTE( "ch196esp", 0x000001, 0x80000, CRC(ed2ff437) SHA1(e76fc2953b6c800d5955c8fb442b80142e40e375) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m3 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "u222chp", 0x000000, 0x80000, CRC(db567b66) SHA1(315bfbf2786ef67a95afb87de836ab348523dbbe) )
	ROM_LOAD16_BYTE( "u196chp", 0x000001, 0x80000, CRC(95ea597e) SHA1(5eb82feaa1de5611a96888e4670744bbb7d90393) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m4 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "u222ne", 0x000000, 0x80000, CRC(7133489e) SHA1(036ef100c64c6e912c911340b32eea0da0b6f6d9) )
	ROM_LOAD16_BYTE( "u196ne", 0x000001, 0x80000, CRC(b07a4f90) SHA1(7a4a800bddc43cfa60f9097723b44a05c9d290ae) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m5 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "u222", 0x000000, 0x80000, CRC(03991fba) SHA1(6c42bf15248640fdb3e98fb01b0a870649deb410) )
	ROM_LOAD16_BYTE( "u196", 0x000001, 0x80000, CRC(39f15a1e) SHA1(901c4fea76bf5bff7330ed07ffde54cdccdaa680) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m6 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "u222-6b", 0x000000, 0x80000, CRC(0a3692be) SHA1(7b937b7b0130e460b5f12188b19f464c55b507c9) )
	ROM_LOAD16_BYTE( "u196-6b", 0x000001, 0x80000, CRC(80454da7) SHA1(64f6dba14d342c9933ce632aa7ca126b34b4ee8b) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2m7 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "u222-2i", 0x000000, 0x40000, CRC(1ca7adbd) SHA1(45e9dc05766ad156edcfc9e59a9804f74f90dc68) )
	ROM_LOAD16_BYTE( "u196-2i", 0x000001, 0x40000, CRC(f758408c) SHA1(aac44a7287bb3b7ba35d68aff279e265dbd3f6d3) )
	ROM_LOAD16_BYTE( "u222-2s", 0x080000, 0x40000, CRC(720cea3e) SHA1(ec4f22159d44a8abc40643b986b88a4f947d6aea) )
	ROM_LOAD16_BYTE( "u196-2s", 0x080001, 0x40000, CRC(9932832c) SHA1(0da0f5ebab91b0759c5fc00902cfe4b12a856466) )
	ROM_LOAD16_WORD_SWAP( "s92_21a.bin",  0x100000, 0x80000, CRC(925a7877) SHA1(1960dca35f0ca6f2b399a9fccfbc0132ac6425d1) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2yyc )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "b12.rom", 0x000000, 0x80000, CRC(8f742fd5) SHA1(a78a00e686856481011d8b5f5e60ed18197a5225) )
	ROM_LOAD16_BYTE( "b14.rom", 0x000001, 0x80000, CRC(8831ec7f) SHA1(0293ff189cbacf90098e734fb31fcbf3c3165e6b) )
	ROM_LOAD16_BYTE( "b11.rom", 0x100000, 0x20000, CRC(94a46525) SHA1(2712b979ce2bfd87e74da3369e0fceaae2a0654c) )
	ROM_RELOAD ( 0x140000, 0x20000 )
	ROM_LOAD16_BYTE( "b13.rom", 0x100001, 0x20000, CRC(8fb3dd47) SHA1(ebf30ad7ae60eeda446e23bd74f6e2d98dde4158) )
	ROM_RELOAD ( 0x140001, 0x20000 )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2koryu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "u222.rom",   0x000000, 0x80000, CRC(9236a79a) SHA1(39c47b0b0ca2f5f569ff07ebb91040b95d0cb43b) )
	ROM_LOAD16_BYTE( "u196.rom",   0x000001, 0x80000, CRC(b23a869d) SHA1(24247d412f20d069919cc8a7fff208af3f7aa1d2) )
	ROM_LOAD16_BYTE( "u221.rom",   0x100000, 0x20000, CRC(64e6e091) SHA1(32ec05db955e538d4ada26d19ee50926f74b684f) )
	ROM_LOAD16_BYTE( "u195.rom",   0x100001, 0x20000, CRC(c95e4443) SHA1(28417dee9ccdfa65b0f4a92aa29b90279fe8cd85) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_10.bin",   0x400000, 0x80000, CRC(960687d5) SHA1(2868c31121b1c7564e9767b9a19cdbf655c7ed1d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_11.bin",   0x400002, 0x80000, CRC(978ecd18) SHA1(648a59706b93c84b4206a968ecbdc3e834c476f6) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.11 */
	ROMX_LOAD( "s92_12.bin",   0x400004, 0x80000, CRC(d6ec9a0a) SHA1(ed6143f8737013b6ef1684e37c05e037e7a80dae) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.12 */
	ROMX_LOAD( "s92_13.bin",   0x400006, 0x80000, CRC(ed2c67f6) SHA1(0083c0ffaf6fe7659ff0cf822be4346cd6e61329) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( varth )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "vae_30a.rom",  0x00000, 0x20000, CRC(7fcd0091) SHA1(7bed452736eda4a26c43c5dd54ec6799afa6e770) )
	ROM_LOAD16_BYTE( "vae_35a.rom",  0x00001, 0x20000, CRC(35cf9509) SHA1(a189ca7740d77262413ec2891af034d0057892be) )
	ROM_LOAD16_BYTE( "vae_31a.rom",  0x40000, 0x20000, CRC(15e5ee81) SHA1(6c6248b07f7e956a37d5dcb4b67d026f57fae13b) )
	ROM_LOAD16_BYTE( "vae_36a.rom",  0x40001, 0x20000, CRC(153a201e) SHA1(5936e447d5cd02ff13802cf78393b521431ad06c) )
	ROM_LOAD16_BYTE( "vae_28a.rom",  0x80000, 0x20000, CRC(7a0e0d25) SHA1(203692ef1daeef7ba08b154cf029cc07a2e0e23d) )
	ROM_LOAD16_BYTE( "vae_33a.rom",  0x80001, 0x20000, CRC(f2365922) SHA1(efb2221033e4b46fedaf3d8c850e208f849e6af0) )
	ROM_LOAD16_BYTE( "vae_29a.rom",  0xc0000, 0x20000, CRC(5e2cd2c3) SHA1(eff955c7dc0d8ae215e7188cc4865726104c7777) )
	ROM_LOAD16_BYTE( "vae_34a.rom",  0xc0001, 0x20000, CRC(3d9bdf83) SHA1(d655803a6f07b90e44aacaa3e6059ac330ef2ec6) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "va_gfx5.rom",  0x000000, 0x80000, CRC(b1fb726e) SHA1(5ac0876b6c49d0a99710dda68653664f4d8c1167) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx7.rom",  0x000002, 0x80000, CRC(4c6588cd) SHA1(d14e8cf051ac934ccc989d8c571c6cc9eed34af5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx1.rom",  0x000004, 0x80000, CRC(0b1ace37) SHA1(6f9493c22f667f683db2789972fd16bb94724679) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx3.rom",  0x000006, 0x80000, CRC(44dfe706) SHA1(a013a434df3161a91aafbb35dc4e20dfb3f177f4) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "va_09.rom",     0x00000, 0x08000, CRC(7a99446e) SHA1(ca027f41e3e58be5abc33ad7380746658cb5380a) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "va_18.rom",    0x00000, 0x20000, CRC(de30510e) SHA1(8e878696192606b76a3a0e53553e638d9621cff7) )
	ROM_LOAD( "va_19.rom",    0x20000, 0x20000, CRC(0610a4ac) SHA1(3da02ea6a7a56c85de898806d2a1cf6bc526c1b3) )
ROM_END

ROM_START( varthu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "vau23a.bin",  0x00000, 0x80000, CRC(fbe68726) SHA1(68917d366551d2203400adc3261355dd3b332bcb) )
	ROM_LOAD16_BYTE( "vae_28a.rom",  0x80000, 0x20000, CRC(7a0e0d25) SHA1(203692ef1daeef7ba08b154cf029cc07a2e0e23d) )
	ROM_LOAD16_BYTE( "vae_33a.rom",  0x80001, 0x20000, CRC(f2365922) SHA1(efb2221033e4b46fedaf3d8c850e208f849e6af0) )
	ROM_LOAD16_BYTE( "vae_29a.rom",  0xc0000, 0x20000, CRC(5e2cd2c3) SHA1(eff955c7dc0d8ae215e7188cc4865726104c7777) )
	ROM_LOAD16_BYTE( "vae_34a.rom",  0xc0001, 0x20000, CRC(3d9bdf83) SHA1(d655803a6f07b90e44aacaa3e6059ac330ef2ec6) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "va_gfx5.rom",  0x000000, 0x80000, CRC(b1fb726e) SHA1(5ac0876b6c49d0a99710dda68653664f4d8c1167) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx7.rom",  0x000002, 0x80000, CRC(4c6588cd) SHA1(d14e8cf051ac934ccc989d8c571c6cc9eed34af5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx1.rom",  0x000004, 0x80000, CRC(0b1ace37) SHA1(6f9493c22f667f683db2789972fd16bb94724679) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx3.rom",  0x000006, 0x80000, CRC(44dfe706) SHA1(a013a434df3161a91aafbb35dc4e20dfb3f177f4) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "va_09.rom",     0x00000, 0x08000, CRC(7a99446e) SHA1(ca027f41e3e58be5abc33ad7380746658cb5380a) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "va_18.rom",    0x00000, 0x20000, CRC(de30510e) SHA1(8e878696192606b76a3a0e53553e638d9621cff7) )
	ROM_LOAD( "va_19.rom",    0x20000, 0x20000, CRC(0610a4ac) SHA1(3da02ea6a7a56c85de898806d2a1cf6bc526c1b3) )
ROM_END

ROM_START( varthj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "vaj36b.bin",   0x00000, 0x20000, CRC(1d798d6a) SHA1(b12579e9dcb88416bc00653b143832d9347adbaf) )
	ROM_LOAD16_BYTE( "vaj42b.bin",   0x00001, 0x20000, CRC(0f720233) SHA1(2d9442ceafd5e2208aa8cd4bcb66861bff6aec47) )
	ROM_LOAD16_BYTE( "vaj37b.bin",   0x40000, 0x20000, CRC(24414b17) SHA1(6c0b24cf8045fc033217c737dba2c046d7d0a09a) )
	ROM_LOAD16_BYTE( "vaj43b.bin",   0x40001, 0x20000, CRC(34b4b06c) SHA1(3033d1d053ba97d6da17064d7b944a10817b93b1) )
	ROM_LOAD16_BYTE( "vaj34b.bin",   0x80000, 0x20000, CRC(87c79aed) SHA1(bb90720d1d04ed6ad276a5230cb078229aa8a40a) )
	ROM_LOAD16_BYTE( "vaj40b.bin",   0x80001, 0x20000, CRC(210b4bd0) SHA1(15771c32af9fb4760953ef5475de228200851b42) )
	ROM_LOAD16_BYTE( "vaj35b.bin",   0xc0000, 0x20000, CRC(6b0da69f) SHA1(5883bea31a22a44ad7494d6acd523c88b62f8743) )
	ROM_LOAD16_BYTE( "vaj41b.bin",   0xc0001, 0x20000, CRC(6542c8a4) SHA1(5f828cf28ef905e4701c92f317e1257a40964a65) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "va_gfx5.rom",  0x000000, 0x80000, CRC(b1fb726e) SHA1(5ac0876b6c49d0a99710dda68653664f4d8c1167) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx7.rom",  0x000002, 0x80000, CRC(4c6588cd) SHA1(d14e8cf051ac934ccc989d8c571c6cc9eed34af5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx1.rom",  0x000004, 0x80000, CRC(0b1ace37) SHA1(6f9493c22f667f683db2789972fd16bb94724679) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "va_gfx3.rom",  0x000006, 0x80000, CRC(44dfe706) SHA1(a013a434df3161a91aafbb35dc4e20dfb3f177f4) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "va_09.rom",     0x00000, 0x08000, CRC(7a99446e) SHA1(ca027f41e3e58be5abc33ad7380746658cb5380a) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "va_18.rom",    0x00000, 0x20000, CRC(de30510e) SHA1(8e878696192606b76a3a0e53553e638d9621cff7) )
	ROM_LOAD( "va_19.rom",    0x20000, 0x20000, CRC(0610a4ac) SHA1(3da02ea6a7a56c85de898806d2a1cf6bc526c1b3) )
ROM_END

ROM_START( cworld2j )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "q536.bin",       0x00000, 0x20000, CRC(38a08099) SHA1(961f733baa2bbf8419e4de70f18660098dba7d52) )
	ROM_LOAD16_BYTE( "q542.bin",       0x00001, 0x20000, CRC(4d29b3a4) SHA1(bf40fc22c0161fe131ca69100b2a4d102e86bde6) )
	ROM_LOAD16_BYTE( "q537.bin",       0x40000, 0x20000, CRC(eb547ebc) SHA1(fce470b05ce095badd180c3740677146f52f6080) )
	ROM_LOAD16_BYTE( "q543.bin",       0x40001, 0x20000, CRC(3ef65ea8) SHA1(2348d84b380c0e8ebe270a37d4ff3ce5204abc8c) )
	ROM_LOAD16_BYTE( "q534.bin",       0x80000, 0x20000, CRC(7fcc1317) SHA1(672ca45d3fad5eec4d65bbbbd1d21cbf6be4ec8b) )
	ROM_LOAD16_BYTE( "q540.bin",       0x80001, 0x20000, CRC(7f14b7b4) SHA1(5564eb9f65dad76ebe40d12d5c39fec5e246adf0) )
	ROM_LOAD16_BYTE( "q535.bin",       0xc0000, 0x20000, CRC(abacee26) SHA1(2f513c02f715ffeec12a6d1c292619e214155cbc) )
	ROM_LOAD16_BYTE( "q541.bin",       0xc0001, 0x20000, CRC(d3654067) SHA1(0b597483e136ff19b031171941cb8439bcd7f145) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "q509.bin",   0x000000, 0x20000, CRC(48496d80) SHA1(bdfaca6375c8275b06b2bc170a25ff6aa62394dc) , ROM_SKIP(7) )
	ROMX_LOAD( "q501.bin",   0x000001, 0x20000, CRC(c5453f56) SHA1(113fe6cc6c830352df5992be9fa34c4d70bf32ed) , ROM_SKIP(7) )
	ROMX_LOAD( "q513.bin",   0x000002, 0x20000, CRC(c741ac52) SHA1(d8b4aeacfd62586b98a1381da357dcc5ab16c1c6) , ROM_SKIP(7) )
	ROMX_LOAD( "q505.bin",   0x000003, 0x20000, CRC(143e068f) SHA1(24cdc49c09a9f0c93e04b37cf7ebba09a929c9b0) , ROM_SKIP(7) )
	ROMX_LOAD( "q524.bin",   0x000004, 0x20000, CRC(b419d139) SHA1(46cd97da2413eb5fbd38fa2c20914f3c5f1c6ec8) , ROM_SKIP(7) )
	ROMX_LOAD( "q517.bin",   0x000005, 0x20000, CRC(bd3b4d11) SHA1(bb62169bc52562715878a33cc4f8558e05d581d3) , ROM_SKIP(7) )
	ROMX_LOAD( "q538.bin",   0x000006, 0x20000, CRC(9c24670c) SHA1(3b98078b7360e21b3905fd973e01b88b02090759) , ROM_SKIP(7) )
	ROMX_LOAD( "q532.bin",   0x000007, 0x20000, CRC(3ef9c7c2) SHA1(52a18d7b12f0c14c5cf68a3dd63571e955005f4c) , ROM_SKIP(7) )
	ROMX_LOAD( "q510.bin",   0x100000, 0x20000, CRC(119e5e93) SHA1(b5b6c2e3516ebe555a26ecfb5934f3b65371bf36) , ROM_SKIP(7) )
	ROMX_LOAD( "q502.bin",   0x100001, 0x20000, CRC(a2cadcbe) SHA1(3d1079f62cce628cbc5b810c0bd51c67c87f4eca) , ROM_SKIP(7) )
	ROMX_LOAD( "q514.bin",   0x100002, 0x20000, CRC(a8755f82) SHA1(0a2fbc8b96651f9ab72eb451723e56ca0a859868) , ROM_SKIP(7) )
	ROMX_LOAD( "q506.bin",   0x100003, 0x20000, CRC(c92a91fc) SHA1(dfe9682349cf94be414b7e1895b632de41729194) , ROM_SKIP(7) )
	ROMX_LOAD( "q525.bin",   0x100004, 0x20000, CRC(979237cb) SHA1(9534b05523317a220b3b957a18fec51f1d4e37b3) , ROM_SKIP(7) )
	ROMX_LOAD( "q518.bin",   0x100005, 0x20000, CRC(c57da03c) SHA1(ad7bce859f56d201d229032baf4fb9f65b54765b) , ROM_SKIP(7) )
	ROMX_LOAD( "q539.bin",   0x100006, 0x20000, CRC(a5839b25) SHA1(20c4c4f24f21a325a03538306de799df2a89f6cb) , ROM_SKIP(7) )
	ROMX_LOAD( "q533.bin",   0x100007, 0x20000, CRC(04d03930) SHA1(37f2556eeb52f8edfcddd3f3642fa24565d5a7bd) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "q523.bin",      0x00000, 0x08000, CRC(e14dc524) SHA1(0020a9002572002458fbfe45e8a959cb90de3f03) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "q530.bin",       0x00000, 0x20000, CRC(d10c1b68) SHA1(2423241f3340d8ab1b6bf9514ca8c3bba1273873) )
	ROM_LOAD( "q531.bin",       0x20000, 0x20000, CRC(7d17e496) SHA1(a274b94ec4f042dddc239ecb9ac2e1e2375f5eb2) )
ROM_END

ROM_START( wof )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "tk2e_23b.rom",  0x000000, 0x80000, CRC(11fb2ed1) SHA1(19e09ad6f9edc7997b030cddfe1d9c96d88135f2) )
	ROM_LOAD16_WORD_SWAP( "tk2e_22b.rom",  0x080000, 0x80000, CRC(479b3f24) SHA1(9fb8ae06856fe115addfb6794c28978a4f6716ec) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "tk2_gfx1.rom",   0x000000, 0x80000, CRC(0d9cb9bf) SHA1(cc7140e9a01a14b252cb1090bcea32b0de461928) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx3.rom",   0x000002, 0x80000, CRC(45227027) SHA1(b21afc593f0d4d8909dfa621d659cbb40507d1b2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx2.rom",   0x000004, 0x80000, CRC(c5ca2460) SHA1(cbe14867f7b94b638ca80db7c8e0c60881183469) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx4.rom",   0x000006, 0x80000, CRC(e349551c) SHA1(1d977bdf256accf750ad9930ec4a0a19bbf86964) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx5.rom",   0x200000, 0x80000, CRC(291f0f0b) SHA1(094baf0f960f25fc2525b3b1cc378a49d9a0955d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx7.rom",   0x200002, 0x80000, CRC(3edeb949) SHA1(c155698dd9ee9eb24bbc97a21118ef2e897ea82f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx6.rom",   0x200004, 0x80000, CRC(1abd14d6) SHA1(dffff3126f102b4ec028a81405fc5b9bd7bb65b3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx8.rom",   0x200006, 0x80000, CRC(b27948e3) SHA1(870d5d23f56798831c641e877ea94217058b2ddc) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "tk2_qa.rom",     0x00000, 0x08000, CRC(c9183a0d) SHA1(d8b1d41c572f08581f8ab9eb878de77d6ea8615d) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "tk2_q1.rom",     0x000000, 0x80000, CRC(611268cf) SHA1(83ab059f2110fb25fdcff928d56b790fc1f5c975) )
	ROM_LOAD( "tk2_q2.rom",     0x080000, 0x80000, CRC(20f55ca9) SHA1(90134e9a9c4749bb65c728b66ea4dac1fd4d88a4) )
	ROM_LOAD( "tk2_q3.rom",     0x100000, 0x80000, CRC(bfcf6f52) SHA1(2a85ff3fc89b4cbabd20779ec12da2e116333c7c) )
	ROM_LOAD( "tk2_q4.rom",     0x180000, 0x80000, CRC(36642e88) SHA1(8ab25b19e2b67215a5cb1f3aa81b9d26009cfeb8) )
ROM_END

ROM_START( wofa )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "tk2a_23b.rom",  0x000000, 0x80000, CRC(2e024628) SHA1(647f8700fe3b410d798a823bac2e4a89cc9ad8d5) )
	ROM_LOAD16_WORD_SWAP( "tk2a_22b.rom",  0x080000, 0x80000, CRC(900ad4cd) SHA1(988007447f93f3467029b9c29fd9670a7ecadaa3) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "tk2_gfx1.rom",   0x000000, 0x80000, CRC(0d9cb9bf) SHA1(cc7140e9a01a14b252cb1090bcea32b0de461928) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx3.rom",   0x000002, 0x80000, CRC(45227027) SHA1(b21afc593f0d4d8909dfa621d659cbb40507d1b2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx2.rom",   0x000004, 0x80000, CRC(c5ca2460) SHA1(cbe14867f7b94b638ca80db7c8e0c60881183469) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx4.rom",   0x000006, 0x80000, CRC(e349551c) SHA1(1d977bdf256accf750ad9930ec4a0a19bbf86964) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx5.rom",   0x200000, 0x80000, CRC(291f0f0b) SHA1(094baf0f960f25fc2525b3b1cc378a49d9a0955d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx7.rom",   0x200002, 0x80000, CRC(3edeb949) SHA1(c155698dd9ee9eb24bbc97a21118ef2e897ea82f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx6.rom",   0x200004, 0x80000, CRC(1abd14d6) SHA1(dffff3126f102b4ec028a81405fc5b9bd7bb65b3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx8.rom",   0x200006, 0x80000, CRC(b27948e3) SHA1(870d5d23f56798831c641e877ea94217058b2ddc) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "tk2_qa.rom",     0x00000, 0x08000, CRC(c9183a0d) SHA1(d8b1d41c572f08581f8ab9eb878de77d6ea8615d) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "tk2_q1.rom",     0x000000, 0x80000, CRC(611268cf) SHA1(83ab059f2110fb25fdcff928d56b790fc1f5c975) )
	ROM_LOAD( "tk2_q2.rom",     0x080000, 0x80000, CRC(20f55ca9) SHA1(90134e9a9c4749bb65c728b66ea4dac1fd4d88a4) )
	ROM_LOAD( "tk2_q3.rom",     0x100000, 0x80000, CRC(bfcf6f52) SHA1(2a85ff3fc89b4cbabd20779ec12da2e116333c7c) )
	ROM_LOAD( "tk2_q4.rom",     0x180000, 0x80000, CRC(36642e88) SHA1(8ab25b19e2b67215a5cb1f3aa81b9d26009cfeb8) )
ROM_END

ROM_START( wofu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "tk2u.23c",  0x000000, 0x80000, CRC(29b89c12) SHA1(2b474b4f45a4ccb0db2a4d5e7ef30e28b5c6cc3a) )
	ROM_LOAD16_WORD_SWAP( "tk2u.22c",  0x080000, 0x80000, CRC(f5af4774) SHA1(f6d53cf5b330e6d68f84da3e8c831a475585b93e) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "tk2_gfx1.rom",   0x000000, 0x80000, CRC(0d9cb9bf) SHA1(cc7140e9a01a14b252cb1090bcea32b0de461928) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx3.rom",   0x000002, 0x80000, CRC(45227027) SHA1(b21afc593f0d4d8909dfa621d659cbb40507d1b2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx2.rom",   0x000004, 0x80000, CRC(c5ca2460) SHA1(cbe14867f7b94b638ca80db7c8e0c60881183469) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx4.rom",   0x000006, 0x80000, CRC(e349551c) SHA1(1d977bdf256accf750ad9930ec4a0a19bbf86964) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx5.rom",   0x200000, 0x80000, CRC(291f0f0b) SHA1(094baf0f960f25fc2525b3b1cc378a49d9a0955d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx7.rom",   0x200002, 0x80000, CRC(3edeb949) SHA1(c155698dd9ee9eb24bbc97a21118ef2e897ea82f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx6.rom",   0x200004, 0x80000, CRC(1abd14d6) SHA1(dffff3126f102b4ec028a81405fc5b9bd7bb65b3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx8.rom",   0x200006, 0x80000, CRC(b27948e3) SHA1(870d5d23f56798831c641e877ea94217058b2ddc) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "tk2_qa.rom",     0x00000, 0x08000, CRC(c9183a0d) SHA1(d8b1d41c572f08581f8ab9eb878de77d6ea8615d) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "tk2_q1.rom",     0x000000, 0x80000, CRC(611268cf) SHA1(83ab059f2110fb25fdcff928d56b790fc1f5c975) )
	ROM_LOAD( "tk2_q2.rom",     0x080000, 0x80000, CRC(20f55ca9) SHA1(90134e9a9c4749bb65c728b66ea4dac1fd4d88a4) )
	ROM_LOAD( "tk2_q3.rom",     0x100000, 0x80000, CRC(bfcf6f52) SHA1(2a85ff3fc89b4cbabd20779ec12da2e116333c7c) )
	ROM_LOAD( "tk2_q4.rom",     0x180000, 0x80000, CRC(36642e88) SHA1(8ab25b19e2b67215a5cb1f3aa81b9d26009cfeb8) )
ROM_END

ROM_START( wofj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "tk2j23c.bin",  0x000000, 0x80000, CRC(9b215a68) SHA1(fc83ed26441fbfb15e21b093c7a6bed44b586e51) )
	ROM_LOAD16_WORD_SWAP( "tk2j22c.bin",  0x080000, 0x80000, CRC(b74b09ac) SHA1(3a44d6db5f51e1b5d2b43ef0ad1191da21e48427) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "tk2_gfx1.rom",   0x000000, 0x80000, CRC(0d9cb9bf) SHA1(cc7140e9a01a14b252cb1090bcea32b0de461928) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx3.rom",   0x000002, 0x80000, CRC(45227027) SHA1(b21afc593f0d4d8909dfa621d659cbb40507d1b2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx2.rom",   0x000004, 0x80000, CRC(c5ca2460) SHA1(cbe14867f7b94b638ca80db7c8e0c60881183469) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2_gfx4.rom",   0x000006, 0x80000, CRC(e349551c) SHA1(1d977bdf256accf750ad9930ec4a0a19bbf86964) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk205.bin",      0x200000, 0x80000, CRC(e4a44d53) SHA1(b747679f4d63e5e62d9fd81b3120fba0401fadfb) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk206.bin",      0x200002, 0x80000, CRC(58066ba8) SHA1(c93af968e21094d020e4b2002e0c6fc0d746af0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk207.bin",      0x200004, 0x80000, CRC(d706568e) SHA1(7886414dc86c42e35d24b85c4bfa41a9f0c167ac) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk208.bin",      0x200006, 0x80000, CRC(d4a19a02) SHA1(ff396b1d33d9b4842140f2c6d085fe05748e3244) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "tk2_qa.rom",     0x00000, 0x08000, CRC(c9183a0d) SHA1(d8b1d41c572f08581f8ab9eb878de77d6ea8615d) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "tk2_q1.rom",     0x000000, 0x80000, CRC(611268cf) SHA1(83ab059f2110fb25fdcff928d56b790fc1f5c975) )
	ROM_LOAD( "tk2_q2.rom",     0x080000, 0x80000, CRC(20f55ca9) SHA1(90134e9a9c4749bb65c728b66ea4dac1fd4d88a4) )
	ROM_LOAD( "tk2_q3.rom",     0x100000, 0x80000, CRC(bfcf6f52) SHA1(2a85ff3fc89b4cbabd20779ec12da2e116333c7c) )
	ROM_LOAD( "tk2_q4.rom",     0x180000, 0x80000, CRC(36642e88) SHA1(8ab25b19e2b67215a5cb1f3aa81b9d26009cfeb8) )
ROM_END

ROM_START( sf2t )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "sf2.23",        0x000000, 0x80000, CRC(89a1fc38) SHA1(aafb40fc311e318250973be8c6aa0d3f7902cb3c) )
	ROM_LOAD16_WORD_SWAP( "sf2_22.bin",    0x080000, 0x80000, CRC(aea6e035) SHA1(ce5fe961b2c1c95d231d1235bfc03b47de489f2a) )
	ROM_LOAD16_WORD_SWAP( "sf2_21.bin",    0x100000, 0x80000, CRC(fd200288) SHA1(3817b67ab77c7b3d4a573a63f18671bea6905e26) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s2t_10.bin",   0x400000, 0x80000, CRC(3c042686) SHA1(307e1ca8ad0b11f3265b7e5467ba4c90f90ec97f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s2t_11.bin",   0x400002, 0x80000, CRC(8b7e7183) SHA1(c8eaedfbddbf0b83311d2dbb9e19a1efef0dffa9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2t.12 */
	ROMX_LOAD( "s2t_12.bin",   0x400004, 0x80000, CRC(293c888c) SHA1(5992ea9aa90fdd8b9dacca9d2a1fdaf25ac2cb65) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2t.11 */
	ROMX_LOAD( "s2t_13.bin",   0x400006, 0x80000, CRC(842b35a4) SHA1(35864a140a0c8d76501e69b2e01bc4ad76f27909) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( sf2tj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "s2tj_23.bin",   0x000000, 0x80000, CRC(ea73b4dc) SHA1(efbc73277d00bac86505755db35225e14ea25a36) )
	ROM_LOAD16_WORD_SWAP( "s2t_22.bin",    0x080000, 0x80000, CRC(aea6e035) SHA1(ce5fe961b2c1c95d231d1235bfc03b47de489f2a) )
	ROM_LOAD16_WORD_SWAP( "s2t_21.bin",    0x100000, 0x80000, CRC(fd200288) SHA1(3817b67ab77c7b3d4a573a63f18671bea6905e26) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "s92_01.bin",   0x000000, 0x80000, CRC(03b0d852) SHA1(f370f25c96ad2b94f8c53d6b7139100285a25bef) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_02.bin",   0x000002, 0x80000, CRC(840289ec) SHA1(2fb42a242f60ba7e74009b5a90eb26e035ba1e82) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.03 */
	ROMX_LOAD( "s92_03.bin",   0x000004, 0x80000, CRC(cdb5f027) SHA1(4c7d944fef200fdfcaf57758b901b5511188ed2e) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.02 */
	ROMX_LOAD( "s92_04.bin",   0x000006, 0x80000, CRC(e2799472) SHA1(27d3796429338d82a8de246a0ea06dd487a87768) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_05.bin",   0x200000, 0x80000, CRC(ba8a2761) SHA1(4b696d66c51611e43522bed752654314e76d33b6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s92_06.bin",   0x200002, 0x80000, CRC(e584bfb5) SHA1(ebdf1f5e2638eed3a65dda82b1ed9151a355f4c9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.07 */
	ROMX_LOAD( "s92_07.bin",   0x200004, 0x80000, CRC(21e3f87d) SHA1(4a4961bb68c3a1ce15f9d393d9c03ecb2466cc29) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2.06 */
	ROMX_LOAD( "s92_08.bin",   0x200006, 0x80000, CRC(befc47df) SHA1(520390420da3a0271ba90b0a933e65143265e5cf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s2t_10.bin",   0x400000, 0x80000, CRC(3c042686) SHA1(307e1ca8ad0b11f3265b7e5467ba4c90f90ec97f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "s2t_11.bin",   0x400002, 0x80000, CRC(8b7e7183) SHA1(c8eaedfbddbf0b83311d2dbb9e19a1efef0dffa9) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2t.12 */
	ROMX_LOAD( "s2t_12.bin",   0x400004, 0x80000, CRC(293c888c) SHA1(5992ea9aa90fdd8b9dacca9d2a1fdaf25ac2cb65) , ROM_GROUPWORD | ROM_SKIP(6) ) /* sf2t.11 */
	ROMX_LOAD( "s2t_13.bin",   0x400006, 0x80000, CRC(842b35a4) SHA1(35864a140a0c8d76501e69b2e01bc4ad76f27909) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "s92_09.bin",    0x00000, 0x08000, CRC(08f6b60e) SHA1(8258fcaca4ac419312531eec67079b97f471179c) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "s92_18.bin",    0x00000, 0x20000, CRC(7f162009) SHA1(346bf42992b4c36c593e21901e22c87ae4a7d86d) )
	ROM_LOAD( "s92_19.bin",    0x20000, 0x20000, CRC(beade53f) SHA1(277c397dc12752719ec6b47d2224750bd1c07f79) )
ROM_END

ROM_START( dino )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "cde_23a.rom",  0x000000, 0x80000, CRC(8f4e585e) SHA1(55ecba3652066cdafad140c4524b1fc81228e69b) )
	ROM_LOAD16_WORD_SWAP( "cde_22a.rom",  0x080000, 0x80000, CRC(9278aa12) SHA1(58cbbd53a98abe640ccb233f8dbd8ca6d63475e7) )
	ROM_LOAD16_WORD_SWAP( "cde_21a.rom",  0x100000, 0x80000, CRC(66d23de2) SHA1(19b8a365f630411d524d055459020f4c8cf930f1) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "cd_gfx01.rom",   0x000000, 0x80000, CRC(8da4f917) SHA1(4f7b2304b7d9b545d6707d7ec921d3e28200699d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx03.rom",   0x000002, 0x80000, CRC(6c40f603) SHA1(cdbd11dfcec08e87355d7e21e9fd39f7eacab016) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx02.rom",   0x000004, 0x80000, CRC(09c8fc2d) SHA1(d0c0a1258ec5dd484ab6ec1c5663425431f929ee) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx04.rom",   0x000006, 0x80000, CRC(637ff38f) SHA1(859926b33b9955b3ed67471c61faa442d42b9696) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx05.rom",   0x200000, 0x80000, CRC(470befee) SHA1(a42e38319e9b7424381352512f11bd8edf0bbb96) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx07.rom",   0x200002, 0x80000, CRC(22bfb7a3) SHA1(c44959bd3d42b9fc8ecb482dfaf63fbd469d2c3e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx06.rom",   0x200004, 0x80000, CRC(e7599ac4) SHA1(0e788a38547a8701115d01190ddeaca64388db4d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx08.rom",   0x200006, 0x80000, CRC(211b4b15) SHA1(374f6b185faa0f14f5c45b9b1d60d0772d93fb17) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "cd_q.rom",       0x00000, 0x08000, CRC(605fdb0b) SHA1(9da90ddc6513aaaf2260f0c69719c6b0e585ba8c) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "cd_q1.rom",      0x000000, 0x80000, CRC(60927775) SHA1(f8599bc84c38573ebbe8685822c58b6a38b50462) )
	ROM_LOAD( "cd_q2.rom",      0x080000, 0x80000, CRC(770f4c47) SHA1(fec8ef00a6669d4d5e37787ecc7b58ee46709326) )
	ROM_LOAD( "cd_q3.rom",      0x100000, 0x80000, CRC(2f273ffc) SHA1(f0de462f6c4d251911258e0ebd886152c14d1586) )
	ROM_LOAD( "cd_q4.rom",      0x180000, 0x80000, CRC(2c67821d) SHA1(6e2528d0b22508300a6a142a796dd3bf53a66946) )
ROM_END

ROM_START( dinou )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "cdu.23a",      0x000000, 0x80000, CRC(7c2543cd) SHA1(6b7a90392fe4c31b2d57620b0ddcb3412401efc3) )
	ROM_LOAD16_WORD_SWAP( "cdu.22a",      0x080000, 0x80000, CRC(fab740a9) SHA1(149cec3fa5e1d6e39bcaf079274d47fe768c910b) )
	ROM_LOAD16_WORD_SWAP( "cde_21a.rom",  0x100000, 0x80000, CRC(66d23de2) SHA1(19b8a365f630411d524d055459020f4c8cf930f1) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "cd_gfx01.rom",   0x000000, 0x80000, CRC(8da4f917) SHA1(4f7b2304b7d9b545d6707d7ec921d3e28200699d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx03.rom",   0x000002, 0x80000, CRC(6c40f603) SHA1(cdbd11dfcec08e87355d7e21e9fd39f7eacab016) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx02.rom",   0x000004, 0x80000, CRC(09c8fc2d) SHA1(d0c0a1258ec5dd484ab6ec1c5663425431f929ee) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx04.rom",   0x000006, 0x80000, CRC(637ff38f) SHA1(859926b33b9955b3ed67471c61faa442d42b9696) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx05.rom",   0x200000, 0x80000, CRC(470befee) SHA1(a42e38319e9b7424381352512f11bd8edf0bbb96) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx07.rom",   0x200002, 0x80000, CRC(22bfb7a3) SHA1(c44959bd3d42b9fc8ecb482dfaf63fbd469d2c3e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx06.rom",   0x200004, 0x80000, CRC(e7599ac4) SHA1(0e788a38547a8701115d01190ddeaca64388db4d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx08.rom",   0x200006, 0x80000, CRC(211b4b15) SHA1(374f6b185faa0f14f5c45b9b1d60d0772d93fb17) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "cd_q.rom",       0x00000, 0x08000, CRC(605fdb0b) SHA1(9da90ddc6513aaaf2260f0c69719c6b0e585ba8c) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "cd_q1.rom",      0x000000, 0x80000, CRC(60927775) SHA1(f8599bc84c38573ebbe8685822c58b6a38b50462) )
	ROM_LOAD( "cd_q2.rom",      0x080000, 0x80000, CRC(770f4c47) SHA1(fec8ef00a6669d4d5e37787ecc7b58ee46709326) )
	ROM_LOAD( "cd_q3.rom",      0x100000, 0x80000, CRC(2f273ffc) SHA1(f0de462f6c4d251911258e0ebd886152c14d1586) )
	ROM_LOAD( "cd_q4.rom",      0x180000, 0x80000, CRC(2c67821d) SHA1(6e2528d0b22508300a6a142a796dd3bf53a66946) )
ROM_END

ROM_START( dinoj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "cdj-23a.8f",   0x000000, 0x80000, CRC(5f3ece96) SHA1(33ffb08ff8c5d3bfb2fa17fa00f254da2fc61f44) )
	ROM_LOAD16_WORD_SWAP( "cdj-22a.7f",   0x080000, 0x80000, CRC(a0d8de29) SHA1(79d916f181804b6176581efe2a1b7f210ec79c07) )
	ROM_LOAD16_WORD_SWAP( "cde_21a.rom",  0x100000, 0x80000, CRC(66d23de2) SHA1(19b8a365f630411d524d055459020f4c8cf930f1) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "cd_gfx01.rom",   0x000000, 0x80000, CRC(8da4f917) SHA1(4f7b2304b7d9b545d6707d7ec921d3e28200699d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx03.rom",   0x000002, 0x80000, CRC(6c40f603) SHA1(cdbd11dfcec08e87355d7e21e9fd39f7eacab016) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx02.rom",   0x000004, 0x80000, CRC(09c8fc2d) SHA1(d0c0a1258ec5dd484ab6ec1c5663425431f929ee) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx04.rom",   0x000006, 0x80000, CRC(637ff38f) SHA1(859926b33b9955b3ed67471c61faa442d42b9696) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx05.rom",   0x200000, 0x80000, CRC(470befee) SHA1(a42e38319e9b7424381352512f11bd8edf0bbb96) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx07.rom",   0x200002, 0x80000, CRC(22bfb7a3) SHA1(c44959bd3d42b9fc8ecb482dfaf63fbd469d2c3e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx06.rom",   0x200004, 0x80000, CRC(e7599ac4) SHA1(0e788a38547a8701115d01190ddeaca64388db4d) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "cd_gfx08.rom",   0x200006, 0x80000, CRC(211b4b15) SHA1(374f6b185faa0f14f5c45b9b1d60d0772d93fb17) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "cd_q.rom",       0x00000, 0x08000, CRC(605fdb0b) SHA1(9da90ddc6513aaaf2260f0c69719c6b0e585ba8c) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "cd_q1.rom",      0x000000, 0x80000, CRC(60927775) SHA1(f8599bc84c38573ebbe8685822c58b6a38b50462) )
	ROM_LOAD( "cd_q2.rom",      0x080000, 0x80000, CRC(770f4c47) SHA1(fec8ef00a6669d4d5e37787ecc7b58ee46709326) )
	ROM_LOAD( "cd_q3.rom",      0x100000, 0x80000, CRC(2f273ffc) SHA1(f0de462f6c4d251911258e0ebd886152c14d1586) )
	ROM_LOAD( "cd_q4.rom",      0x180000, 0x80000, CRC(2c67821d) SHA1(6e2528d0b22508300a6a142a796dd3bf53a66946) )
ROM_END

ROM_START( punisher )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "pse_26.rom",       0x000000, 0x20000, CRC(389a99d2) SHA1(e97f4225554e4603cb0e3edd296a90bb2e467ca7) )
	ROM_LOAD16_BYTE( "pse_30.rom",       0x000001, 0x20000, CRC(68fb06ac) SHA1(189e25ca7e4aaa80492c03ce06696952cc1b1553) )
	ROM_LOAD16_BYTE( "pse_27.rom",       0x040000, 0x20000, CRC(3eb181c3) SHA1(a2442449f4bbe3be03d2be7d4e2cbb69f9741dac) )
	ROM_LOAD16_BYTE( "pse_31.rom",       0x040001, 0x20000, CRC(37108e7b) SHA1(78aaa6e2913e6b1b852b39416557ac4a394d7d8b) )
	ROM_LOAD16_BYTE( "pse_24.rom",       0x080000, 0x20000, CRC(0f434414) SHA1(aaacf835a93551fc792571d6e824a01f3c5d4469) )
	ROM_LOAD16_BYTE( "pse_28.rom",       0x080001, 0x20000, CRC(b732345d) SHA1(472d84f846e9f73f129562d78352376194e0211e) )
	ROM_LOAD16_BYTE( "pse_25.rom",       0x0c0000, 0x20000, CRC(b77102e2) SHA1(2e39b2c2c0eed5ca2320a57e69bcf377f809a20c) )
	ROM_LOAD16_BYTE( "pse_29.rom",       0x0c0001, 0x20000, CRC(ec037bce) SHA1(f86e7feb63d7662a38048e6d51d7b5a69dafaffb) )
	ROM_LOAD16_WORD_SWAP( "ps_21.rom",   0x100000, 0x80000, CRC(8affa5a9) SHA1(268760b83b1723ff50a019ec51ef7af2e49935bf) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "ps_gfx1.rom",   0x000000, 0x80000, CRC(77b7ccab) SHA1(e08e5d55a79e4c0c8ca819d6d7d2a14f753c6ec3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx3.rom",   0x000002, 0x80000, CRC(0122720b) SHA1(5f0d3097e097f64106048156fbb0d343fe78fffa) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx2.rom",   0x000004, 0x80000, CRC(64fa58d4) SHA1(d4a774285ed15273195b6b26d2965ce370e54e73) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx4.rom",   0x000006, 0x80000, CRC(60da42c8) SHA1(95eec4a58d9628a2d9764951dd8dc11e4860a899) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx5.rom",   0x200000, 0x80000, CRC(c54ea839) SHA1(0733f37329edd9d0cace1319a7544b40aa7ecb0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx7.rom",   0x200002, 0x80000, CRC(04c5acbd) SHA1(fddc94b0f36d4d22d7c357856ae15b7514c342d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx6.rom",   0x200004, 0x80000, CRC(a544f4cc) SHA1(9552df8934ba25f19a22f2e07783712d8c8ef03c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx8.rom",   0x200006, 0x80000, CRC(8f02f436) SHA1(a2f0ebb7e9593469c7b843f8962a66f3d77f79e5) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "ps_q.rom",       0x00000, 0x08000, CRC(49ff4446) SHA1(87af12f87a940a6c5428b4574ad44a4b54867bc3) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "ps_q1.rom",      0x000000, 0x80000, CRC(31fd8726) SHA1(1d73a76682e9fb908db0c55b9a18163f7539fea1) )
	ROM_LOAD( "ps_q2.rom",      0x080000, 0x80000, CRC(980a9eef) SHA1(36571381f349bc726508a7e618ba1c635ec9d271) )
	ROM_LOAD( "ps_q3.rom",      0x100000, 0x80000, CRC(0dd44491) SHA1(903cea1d7f3120545ea3229d30fbd687d11ad68f) )
	ROM_LOAD( "ps_q4.rom",      0x180000, 0x80000, CRC(bed42f03) SHA1(21302f7e75f9c795392a3b34e16a959fc5f6e4e9) )
ROM_END

ROM_START( punishru )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE ( "psu26.rom",       0x000000, 0x20000, CRC(9236d121) SHA1(52d5d00009f61089157319943cde8f1a1ed48ad4) )
	ROM_LOAD16_BYTE ( "psu30.rom",       0x000001, 0x20000, CRC(8320e501) SHA1(bb3b74135df9dd494a277a1bc3bef2917351203f) )
	ROM_LOAD16_BYTE ( "psu27.rom",       0x040000, 0x20000, CRC(61c960a1) SHA1(f8fe651283cc1f138d013cab65b833505de6df9f) )
	ROM_LOAD16_BYTE ( "psu31.rom",       0x040001, 0x20000, CRC(78d4c298) SHA1(6e7fbaed9ad9230a6e5035c6eda64b2f1f83048c) )
	ROM_LOAD16_BYTE ( "psu24.rom",       0x080000, 0x20000, CRC(1cfecad7) SHA1(f4dcf5066dc59507cece0c53ccc208e4323ae26f) )
	ROM_LOAD16_BYTE ( "psu28.rom",       0x080001, 0x20000, CRC(bdf921c1) SHA1(89a6709756c7c32e7c888806f983ce5af61cfcef) )
	ROM_LOAD16_BYTE ( "psu25.rom",       0x0c0000, 0x20000, CRC(c51acc94) SHA1(34ffd6392914e3e67d7d0804215bd1193846b554) )
	ROM_LOAD16_BYTE ( "psu29.rom",       0x0c0001, 0x20000, CRC(52dce1ca) SHA1(45277abe34feacdcaedaec56f513b7437d4260e9) )
	ROM_LOAD16_WORD_SWAP( "ps_21.rom",   0x100000, 0x80000, CRC(8affa5a9) SHA1(268760b83b1723ff50a019ec51ef7af2e49935bf) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "ps_gfx1.rom",   0x000000, 0x80000, CRC(77b7ccab) SHA1(e08e5d55a79e4c0c8ca819d6d7d2a14f753c6ec3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx3.rom",   0x000002, 0x80000, CRC(0122720b) SHA1(5f0d3097e097f64106048156fbb0d343fe78fffa) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx2.rom",   0x000004, 0x80000, CRC(64fa58d4) SHA1(d4a774285ed15273195b6b26d2965ce370e54e73) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx4.rom",   0x000006, 0x80000, CRC(60da42c8) SHA1(95eec4a58d9628a2d9764951dd8dc11e4860a899) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx5.rom",   0x200000, 0x80000, CRC(c54ea839) SHA1(0733f37329edd9d0cace1319a7544b40aa7ecb0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx7.rom",   0x200002, 0x80000, CRC(04c5acbd) SHA1(fddc94b0f36d4d22d7c357856ae15b7514c342d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx6.rom",   0x200004, 0x80000, CRC(a544f4cc) SHA1(9552df8934ba25f19a22f2e07783712d8c8ef03c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx8.rom",   0x200006, 0x80000, CRC(8f02f436) SHA1(a2f0ebb7e9593469c7b843f8962a66f3d77f79e5) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "ps_q.rom",       0x00000, 0x08000, CRC(49ff4446) SHA1(87af12f87a940a6c5428b4574ad44a4b54867bc3) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "ps_q1.rom",      0x000000, 0x80000, CRC(31fd8726) SHA1(1d73a76682e9fb908db0c55b9a18163f7539fea1) )
	ROM_LOAD( "ps_q2.rom",      0x080000, 0x80000, CRC(980a9eef) SHA1(36571381f349bc726508a7e618ba1c635ec9d271) )
	ROM_LOAD( "ps_q3.rom",      0x100000, 0x80000, CRC(0dd44491) SHA1(903cea1d7f3120545ea3229d30fbd687d11ad68f) )
	ROM_LOAD( "ps_q4.rom",      0x180000, 0x80000, CRC(bed42f03) SHA1(21302f7e75f9c795392a3b34e16a959fc5f6e4e9) )
ROM_END

ROM_START( punishrj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "psj23.bin",   0x000000, 0x80000, CRC(6b2fda52) SHA1(5f95a79b7b802609ae9ddd6641cc52610d428bf4) )
	ROM_LOAD16_WORD_SWAP( "psj22.bin",   0x080000, 0x80000, CRC(e01036bc) SHA1(a01886014dabe8f9ab45619865c6bd9f27472eae) )
	ROM_LOAD16_WORD_SWAP( "ps_21.rom",   0x100000, 0x80000, CRC(8affa5a9) SHA1(268760b83b1723ff50a019ec51ef7af2e49935bf) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "ps_gfx1.rom",   0x000000, 0x80000, CRC(77b7ccab) SHA1(e08e5d55a79e4c0c8ca819d6d7d2a14f753c6ec3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx3.rom",   0x000002, 0x80000, CRC(0122720b) SHA1(5f0d3097e097f64106048156fbb0d343fe78fffa) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx2.rom",   0x000004, 0x80000, CRC(64fa58d4) SHA1(d4a774285ed15273195b6b26d2965ce370e54e73) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx4.rom",   0x000006, 0x80000, CRC(60da42c8) SHA1(95eec4a58d9628a2d9764951dd8dc11e4860a899) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx5.rom",   0x200000, 0x80000, CRC(c54ea839) SHA1(0733f37329edd9d0cace1319a7544b40aa7ecb0b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx7.rom",   0x200002, 0x80000, CRC(04c5acbd) SHA1(fddc94b0f36d4d22d7c357856ae15b7514c342d3) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx6.rom",   0x200004, 0x80000, CRC(a544f4cc) SHA1(9552df8934ba25f19a22f2e07783712d8c8ef03c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ps_gfx8.rom",   0x200006, 0x80000, CRC(8f02f436) SHA1(a2f0ebb7e9593469c7b843f8962a66f3d77f79e5) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "ps_q.rom",       0x00000, 0x08000, CRC(49ff4446) SHA1(87af12f87a940a6c5428b4574ad44a4b54867bc3) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "ps_q1.rom",      0x000000, 0x80000, CRC(31fd8726) SHA1(1d73a76682e9fb908db0c55b9a18163f7539fea1) )
	ROM_LOAD( "ps_q2.rom",      0x080000, 0x80000, CRC(980a9eef) SHA1(36571381f349bc726508a7e618ba1c635ec9d271) )
	ROM_LOAD( "ps_q3.rom",      0x100000, 0x80000, CRC(0dd44491) SHA1(903cea1d7f3120545ea3229d30fbd687d11ad68f) )
	ROM_LOAD( "ps_q4.rom",      0x180000, 0x80000, CRC(bed42f03) SHA1(21302f7e75f9c795392a3b34e16a959fc5f6e4e9) )
ROM_END

ROM_START( slammast )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "mbe_23e.rom",  0x000000, 0x80000, CRC(5394057a) SHA1(57f8b40c0a15e82c98ce5f0a8c4bdf60a1bc3107) )
	ROM_LOAD16_BYTE( "mbe_24b.rom",       0x080000, 0x20000, CRC(95d5e729) SHA1(df3be896e55c92eb50887a4317178a3d11048433) )
	ROM_LOAD16_BYTE( "mbe_28b.rom",       0x080001, 0x20000, CRC(b1c7cbcb) SHA1(cf5ad72be4a055db876e7347b1826325b9bf81d9) )
	ROM_LOAD16_BYTE( "mbe_25b.rom",       0x0c0000, 0x20000, CRC(a50d3fd4) SHA1(dc3d108c3bc27f45b8b2e11919ba2a86e05b41d1) )
	ROM_LOAD16_BYTE( "mbe_29b.rom",       0x0c0001, 0x20000, CRC(08e32e56) SHA1(70ad78b079f777ec02089f0df20ce2baad7adce5) )
	ROM_LOAD16_WORD_SWAP( "mbe_21a.rom",  0x100000, 0x80000, CRC(d5007b05) SHA1(c55e55908aeda40ca2318c76ccbc05d333676875) )
	ROM_LOAD16_WORD_SWAP( "mbe_20a.rom",  0x180000, 0x80000, CRC(aeb557b0) SHA1(530551942961d776f0a85852e02bb243840ca671) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "mb_gfx01.rom",   0x000000, 0x80000, CRC(41468e06) SHA1(fb365798f2889a20eebaea2393c9c2c8827003c4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx03.rom",   0x000002, 0x80000, CRC(f453aa9e) SHA1(24a103dc6f0dc96f8d0f6164ad732909c9cd2d6a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx02.rom",   0x000004, 0x80000, CRC(2ffbfea8) SHA1(13e30133664a009686e1114c92b558bdbb91ea32) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx04.rom",   0x000006, 0x80000, CRC(1eb9841d) SHA1(685da3e011a96b36be9f639a241b2f8f27da4629) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_05.bin",      0x200000, 0x80000, CRC(506b9dc9) SHA1(933bf2fb9bcc1a408f961f0e7052da80144bddad) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_07.bin",      0x200002, 0x80000, CRC(aff8c2fb) SHA1(ce37a6d5b1eb58c2d74f23f84ec824c214c93217) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_06.bin",      0x200004, 0x80000, CRC(b76c70e9) SHA1(c21e255815ec9a985919dbd760ed266c28bd47cd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_08.bin",      0x200006, 0x80000, CRC(e60c9556) SHA1(b91c14092aa8dbb0922d96998123ef1970a658f6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_10.bin",      0x400000, 0x80000, CRC(97976ff5) SHA1(ec9d3460816ab971a02fbce42960283091777e47) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_12.bin",      0x400002, 0x80000, CRC(b350a840) SHA1(2b8b996cd08051e7e8e134bff5448775d78058a0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_11.bin",      0x400004, 0x80000, CRC(8fb94743) SHA1(294f6182c8a41b640d1f57cb5e3a2abce3b06482) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_13.bin",      0x400006, 0x80000, CRC(da810d5f) SHA1(392bbd405244b8c99024c9228cfec6a7ef0accdb) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "mb_qa.rom",      0x00000, 0x08000, CRC(e21a03c4) SHA1(98c03fd2c9b6bf8a4fc25a4edca87fff7c3c3819) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x8000, REGION_USER1, 0 )
	/* the encrypted Z80 ROM will be copied here, where the main CPU can read it. */

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "mb_q1.bin",      0x000000, 0x80000, CRC(0630c3ce) SHA1(520fc74c5c3638f611fa2f1b5efb08b91747e29b) )
	ROM_LOAD( "mb_q2.bin",      0x080000, 0x80000, CRC(354f9c21) SHA1(1dc6b39791fd0f760697f409a6b62361a7bf62e9) )
	ROM_LOAD( "mb_q3.bin",      0x100000, 0x80000, CRC(7838487c) SHA1(056b7da05cfca46873edacd674ca25c70855c6db) )
	ROM_LOAD( "mb_q4.bin",      0x180000, 0x80000, CRC(ab66e087) SHA1(066ea69a0157e8647eea3c44d0a1843898860678) )
	ROM_LOAD( "mb_q5.bin",      0x200000, 0x80000, CRC(c789fef2) SHA1(10d1e3d92288fccd4e064a3716a788a165efc3c9) )
	ROM_LOAD( "mb_q6.bin",      0x280000, 0x80000, CRC(ecb81b61) SHA1(e339f21ae47de4782f3b338befcdac659c3503f6) )
	ROM_LOAD( "mb_q7.bin",      0x300000, 0x80000, CRC(041e49ba) SHA1(3220b033a5c0cfbbe75c0c113cf2db39fb093a7e) )
	ROM_LOAD( "mb_q8.bin",      0x380000, 0x80000, CRC(59fe702a) SHA1(807178dfc6d864e49fd7aabb5c4895835cf0e85b) )
ROM_END

ROM_START( slammasu )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "mbu-23e.rom",  0x000000, 0x80000, CRC(224f0062) SHA1(d961f2e7db7acac576539c24a69e7dd9bf8fc406) )
	ROM_LOAD16_BYTE( "mbe_24b.rom",       0x080000, 0x20000, CRC(95d5e729) SHA1(df3be896e55c92eb50887a4317178a3d11048433) )
	ROM_LOAD16_BYTE( "mbe_28b.rom",       0x080001, 0x20000, CRC(b1c7cbcb) SHA1(cf5ad72be4a055db876e7347b1826325b9bf81d9) )
	ROM_LOAD16_BYTE( "mbe_25b.rom",       0x0c0000, 0x20000, CRC(a50d3fd4) SHA1(dc3d108c3bc27f45b8b2e11919ba2a86e05b41d1) )
	ROM_LOAD16_BYTE( "mbe_29b.rom",       0x0c0001, 0x20000, CRC(08e32e56) SHA1(70ad78b079f777ec02089f0df20ce2baad7adce5) )
	ROM_LOAD16_WORD_SWAP( "mbe_21a.rom",  0x100000, 0x80000, CRC(d5007b05) SHA1(c55e55908aeda40ca2318c76ccbc05d333676875) )
	ROM_LOAD16_WORD_SWAP( "mbu-20a.rom",  0x180000, 0x80000, CRC(fc848af5) SHA1(cd3f6e50779b89ee57a9d08bfa1d58dea286457c) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "mb_gfx01.rom",   0x000000, 0x80000, CRC(41468e06) SHA1(fb365798f2889a20eebaea2393c9c2c8827003c4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx03.rom",   0x000002, 0x80000, CRC(f453aa9e) SHA1(24a103dc6f0dc96f8d0f6164ad732909c9cd2d6a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx02.rom",   0x000004, 0x80000, CRC(2ffbfea8) SHA1(13e30133664a009686e1114c92b558bdbb91ea32) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx04.rom",   0x000006, 0x80000, CRC(1eb9841d) SHA1(685da3e011a96b36be9f639a241b2f8f27da4629) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_05.bin",      0x200000, 0x80000, CRC(506b9dc9) SHA1(933bf2fb9bcc1a408f961f0e7052da80144bddad) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_07.bin",      0x200002, 0x80000, CRC(aff8c2fb) SHA1(ce37a6d5b1eb58c2d74f23f84ec824c214c93217) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_06.bin",      0x200004, 0x80000, CRC(b76c70e9) SHA1(c21e255815ec9a985919dbd760ed266c28bd47cd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_08.bin",      0x200006, 0x80000, CRC(e60c9556) SHA1(b91c14092aa8dbb0922d96998123ef1970a658f6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_10.bin",      0x400000, 0x80000, CRC(97976ff5) SHA1(ec9d3460816ab971a02fbce42960283091777e47) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_12.bin",      0x400002, 0x80000, CRC(b350a840) SHA1(2b8b996cd08051e7e8e134bff5448775d78058a0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_11.bin",      0x400004, 0x80000, CRC(8fb94743) SHA1(294f6182c8a41b640d1f57cb5e3a2abce3b06482) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_13.bin",      0x400006, 0x80000, CRC(da810d5f) SHA1(392bbd405244b8c99024c9228cfec6a7ef0accdb) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "mb_qa.rom",      0x00000, 0x08000, CRC(e21a03c4) SHA1(98c03fd2c9b6bf8a4fc25a4edca87fff7c3c3819) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x8000, REGION_USER1, 0 )
	/* the encrypted Z80 ROM will be copied here, where the main CPU can read it. */

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "mb_q1.bin",      0x000000, 0x80000, CRC(0630c3ce) SHA1(520fc74c5c3638f611fa2f1b5efb08b91747e29b) )
	ROM_LOAD( "mb_q2.bin",      0x080000, 0x80000, CRC(354f9c21) SHA1(1dc6b39791fd0f760697f409a6b62361a7bf62e9) )
	ROM_LOAD( "mb_q3.bin",      0x100000, 0x80000, CRC(7838487c) SHA1(056b7da05cfca46873edacd674ca25c70855c6db) )
	ROM_LOAD( "mb_q4.bin",      0x180000, 0x80000, CRC(ab66e087) SHA1(066ea69a0157e8647eea3c44d0a1843898860678) )
	ROM_LOAD( "mb_q5.bin",      0x200000, 0x80000, CRC(c789fef2) SHA1(10d1e3d92288fccd4e064a3716a788a165efc3c9) )
	ROM_LOAD( "mb_q6.bin",      0x280000, 0x80000, CRC(ecb81b61) SHA1(e339f21ae47de4782f3b338befcdac659c3503f6) )
	ROM_LOAD( "mb_q7.bin",      0x300000, 0x80000, CRC(041e49ba) SHA1(3220b033a5c0cfbbe75c0c113cf2db39fb093a7e) )
	ROM_LOAD( "mb_q8.bin",      0x380000, 0x80000, CRC(59fe702a) SHA1(807178dfc6d864e49fd7aabb5c4895835cf0e85b) )
ROM_END

ROM_START( mbomberj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "mbj23e",       0x000000, 0x80000, CRC(0d06036a) SHA1(e1b102888038b4bb612a41ac94a43333d468a245) )
	ROM_LOAD16_BYTE( "mbe_24b.rom",       0x080000, 0x20000, CRC(95d5e729) SHA1(df3be896e55c92eb50887a4317178a3d11048433) )
	ROM_LOAD16_BYTE( "mbe_28b.rom",       0x080001, 0x20000, CRC(b1c7cbcb) SHA1(cf5ad72be4a055db876e7347b1826325b9bf81d9) )
	ROM_LOAD16_BYTE( "mbe_25b.rom",       0x0c0000, 0x20000, CRC(a50d3fd4) SHA1(dc3d108c3bc27f45b8b2e11919ba2a86e05b41d1) )
	ROM_LOAD16_BYTE( "mbe_29b.rom",       0x0c0001, 0x20000, CRC(08e32e56) SHA1(70ad78b079f777ec02089f0df20ce2baad7adce5) )
	ROM_LOAD16_WORD_SWAP( "mbe_21a.rom",  0x100000, 0x80000, CRC(d5007b05) SHA1(c55e55908aeda40ca2318c76ccbc05d333676875) )
	ROM_LOAD16_WORD_SWAP( "mbe_20a.rom",  0x180000, 0x80000, CRC(aeb557b0) SHA1(530551942961d776f0a85852e02bb243840ca671) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "mbj_01.bin",     0x000000, 0x80000, CRC(a53b1c81) SHA1(d1efb88eeaf6e30e51aaf1432078003e52454dd9) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mbj_03.bin",     0x000002, 0x80000, CRC(23fe10f6) SHA1(deefa7cac4394b0642f7fb444f9374dbe0bc8843) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mbj_02.bin",     0x000004, 0x80000, CRC(cb866c2f) SHA1(b087f52e3b2a514a209612319d1d7c4f1c12b8bd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mbj_04.bin",     0x000006, 0x80000, CRC(c9143e75) SHA1(e30090625ef6ac971a4f65d53f5458cebb5f146c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_05.bin",      0x200000, 0x80000, CRC(506b9dc9) SHA1(933bf2fb9bcc1a408f961f0e7052da80144bddad) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_07.bin",      0x200002, 0x80000, CRC(aff8c2fb) SHA1(ce37a6d5b1eb58c2d74f23f84ec824c214c93217) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_06.bin",      0x200004, 0x80000, CRC(b76c70e9) SHA1(c21e255815ec9a985919dbd760ed266c28bd47cd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_08.bin",      0x200006, 0x80000, CRC(e60c9556) SHA1(b91c14092aa8dbb0922d96998123ef1970a658f6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_10.bin",      0x400000, 0x80000, CRC(97976ff5) SHA1(ec9d3460816ab971a02fbce42960283091777e47) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_12.bin",      0x400002, 0x80000, CRC(b350a840) SHA1(2b8b996cd08051e7e8e134bff5448775d78058a0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_11.bin",      0x400004, 0x80000, CRC(8fb94743) SHA1(294f6182c8a41b640d1f57cb5e3a2abce3b06482) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_13.bin",      0x400006, 0x80000, CRC(da810d5f) SHA1(392bbd405244b8c99024c9228cfec6a7ef0accdb) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "mb_qa.rom",      0x00000, 0x08000, CRC(e21a03c4) SHA1(98c03fd2c9b6bf8a4fc25a4edca87fff7c3c3819) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x8000, REGION_USER1, 0 )
	/* the encrypted Z80 ROM will be copied here, where the main CPU can read it. */

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "mb_q1.bin",      0x000000, 0x80000, CRC(0630c3ce) SHA1(520fc74c5c3638f611fa2f1b5efb08b91747e29b) )
	ROM_LOAD( "mb_q2.bin",      0x080000, 0x80000, CRC(354f9c21) SHA1(1dc6b39791fd0f760697f409a6b62361a7bf62e9) )
	ROM_LOAD( "mb_q3.bin",      0x100000, 0x80000, CRC(7838487c) SHA1(056b7da05cfca46873edacd674ca25c70855c6db) )
	ROM_LOAD( "mb_q4.bin",      0x180000, 0x80000, CRC(ab66e087) SHA1(066ea69a0157e8647eea3c44d0a1843898860678) )
	ROM_LOAD( "mb_q5.bin",      0x200000, 0x80000, CRC(c789fef2) SHA1(10d1e3d92288fccd4e064a3716a788a165efc3c9) )
	ROM_LOAD( "mb_q6.bin",      0x280000, 0x80000, CRC(ecb81b61) SHA1(e339f21ae47de4782f3b338befcdac659c3503f6) )
	ROM_LOAD( "mb_q7.bin",      0x300000, 0x80000, CRC(041e49ba) SHA1(3220b033a5c0cfbbe75c0c113cf2db39fb093a7e) )
	ROM_LOAD( "mb_q8.bin",      0x380000, 0x80000, CRC(59fe702a) SHA1(807178dfc6d864e49fd7aabb5c4895835cf0e85b) )
ROM_END

ROM_START( mbombrd )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "mbd_26.bin",        0x000000, 0x20000, CRC(72b7451c) SHA1(380ef57bb00f1c92d2f29e551b0a670eb5a56cb7) )
	ROM_LOAD16_BYTE( "mbde_30.rom",       0x000001, 0x20000, CRC(a036dc16) SHA1(a68cf74976f482dbc581734e143669511a9a4bee) )
	ROM_LOAD16_BYTE( "mbd_27.bin",        0x040000, 0x20000, CRC(4086f534) SHA1(a2b949f00035b06cb1cd01185902daca3d89d0e3) )
	ROM_LOAD16_BYTE( "mbd_31.bin",        0x040001, 0x20000, CRC(085f47f0) SHA1(ac93a196faf17b7dbe7179ce1e850d9cd7293a21) )
	ROM_LOAD16_BYTE( "mbd_24.bin",        0x080000, 0x20000, CRC(c20895a5) SHA1(35116f7ef8576753ec989647ca2f6a6131d6909f) )
	ROM_LOAD16_BYTE( "mbd_28.bin",        0x080001, 0x20000, CRC(2618d5e1) SHA1(50797c6dda04df95267ff9ef08933c17c3ce7057) )
	ROM_LOAD16_BYTE( "mbd_25.bin",        0x0c0000, 0x20000, CRC(9bdb6b11) SHA1(fbfbd6b5a72ca3237713ce43a798660f899b707d) )
	ROM_LOAD16_BYTE( "mbd_29.bin",        0x0c0001, 0x20000, CRC(3f52d5e5) SHA1(0b1ed8e876a6ec2cfb83676afe43a81e8a033e52) )
	ROM_LOAD16_WORD_SWAP( "mbd_21.bin",   0x100000, 0x80000, CRC(690c026a) SHA1(80ad780743b50750b6bfe1d4e28efe98e562233e) )
	ROM_LOAD16_WORD_SWAP( "mbd_20.bin",   0x180000, 0x80000, CRC(b8b2139b) SHA1(88c9169a9979b711ab7afb8272df0a1c80bb357b) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "mb_gfx01.rom",   0x000000, 0x80000, CRC(41468e06) SHA1(fb365798f2889a20eebaea2393c9c2c8827003c4) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx03.rom",   0x000002, 0x80000, CRC(f453aa9e) SHA1(24a103dc6f0dc96f8d0f6164ad732909c9cd2d6a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx02.rom",   0x000004, 0x80000, CRC(2ffbfea8) SHA1(13e30133664a009686e1114c92b558bdbb91ea32) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_gfx04.rom",   0x000006, 0x80000, CRC(1eb9841d) SHA1(685da3e011a96b36be9f639a241b2f8f27da4629) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_05.bin",      0x200000, 0x80000, CRC(506b9dc9) SHA1(933bf2fb9bcc1a408f961f0e7052da80144bddad) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_07.bin",      0x200002, 0x80000, CRC(aff8c2fb) SHA1(ce37a6d5b1eb58c2d74f23f84ec824c214c93217) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_06.bin",      0x200004, 0x80000, CRC(b76c70e9) SHA1(c21e255815ec9a985919dbd760ed266c28bd47cd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_08.bin",      0x200006, 0x80000, CRC(e60c9556) SHA1(b91c14092aa8dbb0922d96998123ef1970a658f6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_10.bin",      0x400000, 0x80000, CRC(97976ff5) SHA1(ec9d3460816ab971a02fbce42960283091777e47) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_12.bin",      0x400002, 0x80000, CRC(b350a840) SHA1(2b8b996cd08051e7e8e134bff5448775d78058a0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_11.bin",      0x400004, 0x80000, CRC(8fb94743) SHA1(294f6182c8a41b640d1f57cb5e3a2abce3b06482) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_13.bin",      0x400006, 0x80000, CRC(da810d5f) SHA1(392bbd405244b8c99024c9228cfec6a7ef0accdb) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "mb_q.bin",       0x00000, 0x08000, CRC(d6fa76d1) SHA1(3bfcb703e0e458ef1bb843230f8537167f1d4c3c) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "mb_q1.bin",      0x000000, 0x80000, CRC(0630c3ce) SHA1(520fc74c5c3638f611fa2f1b5efb08b91747e29b) )
	ROM_LOAD( "mb_q2.bin",      0x080000, 0x80000, CRC(354f9c21) SHA1(1dc6b39791fd0f760697f409a6b62361a7bf62e9) )
	ROM_LOAD( "mb_q3.bin",      0x100000, 0x80000, CRC(7838487c) SHA1(056b7da05cfca46873edacd674ca25c70855c6db) )
	ROM_LOAD( "mb_q4.bin",      0x180000, 0x80000, CRC(ab66e087) SHA1(066ea69a0157e8647eea3c44d0a1843898860678) )
	ROM_LOAD( "mb_q5.bin",      0x200000, 0x80000, CRC(c789fef2) SHA1(10d1e3d92288fccd4e064a3716a788a165efc3c9) )
	ROM_LOAD( "mb_q6.bin",      0x280000, 0x80000, CRC(ecb81b61) SHA1(e339f21ae47de4782f3b338befcdac659c3503f6) )
	ROM_LOAD( "mb_q7.bin",      0x300000, 0x80000, CRC(041e49ba) SHA1(3220b033a5c0cfbbe75c0c113cf2db39fb093a7e) )
	ROM_LOAD( "mb_q8.bin",      0x380000, 0x80000, CRC(59fe702a) SHA1(807178dfc6d864e49fd7aabb5c4895835cf0e85b) )
ROM_END

ROM_START( mbombrdj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "mbd_26.bin",        0x000000, 0x20000, CRC(72b7451c) SHA1(380ef57bb00f1c92d2f29e551b0a670eb5a56cb7) )
	ROM_LOAD16_BYTE( "mbdj_30.bin",       0x000001, 0x20000, CRC(beff31cf) SHA1(8a3a1fa848fe8fad239d21aef1871e54bbcb582f) )
	ROM_LOAD16_BYTE( "mbd_27.bin",        0x040000, 0x20000, CRC(4086f534) SHA1(a2b949f00035b06cb1cd01185902daca3d89d0e3) )
	ROM_LOAD16_BYTE( "mbd_31.bin",        0x040001, 0x20000, CRC(085f47f0) SHA1(ac93a196faf17b7dbe7179ce1e850d9cd7293a21) )
	ROM_LOAD16_BYTE( "mbd_24.bin",        0x080000, 0x20000, CRC(c20895a5) SHA1(35116f7ef8576753ec989647ca2f6a6131d6909f) )
	ROM_LOAD16_BYTE( "mbd_28.bin",        0x080001, 0x20000, CRC(2618d5e1) SHA1(50797c6dda04df95267ff9ef08933c17c3ce7057) )
	ROM_LOAD16_BYTE( "mbd_25.bin",        0x0c0000, 0x20000, CRC(9bdb6b11) SHA1(fbfbd6b5a72ca3237713ce43a798660f899b707d) )
	ROM_LOAD16_BYTE( "mbd_29.bin",        0x0c0001, 0x20000, CRC(3f52d5e5) SHA1(0b1ed8e876a6ec2cfb83676afe43a81e8a033e52) )
	ROM_LOAD16_WORD_SWAP( "mbd_21.bin",   0x100000, 0x80000, CRC(690c026a) SHA1(80ad780743b50750b6bfe1d4e28efe98e562233e) )
	ROM_LOAD16_WORD_SWAP( "mbd_20.bin",   0x180000, 0x80000, CRC(b8b2139b) SHA1(88c9169a9979b711ab7afb8272df0a1c80bb357b) )

	ROM_REGION( 0x600000, REGION_GFX1, 0 )
	ROMX_LOAD( "mbj_01.bin",     0x000000, 0x80000, CRC(a53b1c81) SHA1(d1efb88eeaf6e30e51aaf1432078003e52454dd9) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mbj_03.bin",     0x000002, 0x80000, CRC(23fe10f6) SHA1(deefa7cac4394b0642f7fb444f9374dbe0bc8843) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mbj_02.bin",     0x000004, 0x80000, CRC(cb866c2f) SHA1(b087f52e3b2a514a209612319d1d7c4f1c12b8bd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mbj_04.bin",     0x000006, 0x80000, CRC(c9143e75) SHA1(e30090625ef6ac971a4f65d53f5458cebb5f146c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_05.bin",      0x200000, 0x80000, CRC(506b9dc9) SHA1(933bf2fb9bcc1a408f961f0e7052da80144bddad) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_07.bin",      0x200002, 0x80000, CRC(aff8c2fb) SHA1(ce37a6d5b1eb58c2d74f23f84ec824c214c93217) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_06.bin",      0x200004, 0x80000, CRC(b76c70e9) SHA1(c21e255815ec9a985919dbd760ed266c28bd47cd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_08.bin",      0x200006, 0x80000, CRC(e60c9556) SHA1(b91c14092aa8dbb0922d96998123ef1970a658f6) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_10.bin",      0x400000, 0x80000, CRC(97976ff5) SHA1(ec9d3460816ab971a02fbce42960283091777e47) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_12.bin",      0x400002, 0x80000, CRC(b350a840) SHA1(2b8b996cd08051e7e8e134bff5448775d78058a0) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_11.bin",      0x400004, 0x80000, CRC(8fb94743) SHA1(294f6182c8a41b640d1f57cb5e3a2abce3b06482) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mb_13.bin",      0x400006, 0x80000, CRC(da810d5f) SHA1(392bbd405244b8c99024c9228cfec6a7ef0accdb) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "mb_q.bin",       0x00000, 0x08000, CRC(d6fa76d1) SHA1(3bfcb703e0e458ef1bb843230f8537167f1d4c3c) )
	ROM_CONTINUE(               0x10000, 0x18000 )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "mb_q1.bin",      0x000000, 0x80000, CRC(0630c3ce) SHA1(520fc74c5c3638f611fa2f1b5efb08b91747e29b) )
	ROM_LOAD( "mb_q2.bin",      0x080000, 0x80000, CRC(354f9c21) SHA1(1dc6b39791fd0f760697f409a6b62361a7bf62e9) )
	ROM_LOAD( "mb_q3.bin",      0x100000, 0x80000, CRC(7838487c) SHA1(056b7da05cfca46873edacd674ca25c70855c6db) )
	ROM_LOAD( "mb_q4.bin",      0x180000, 0x80000, CRC(ab66e087) SHA1(066ea69a0157e8647eea3c44d0a1843898860678) )
	ROM_LOAD( "mb_q5.bin",      0x200000, 0x80000, CRC(c789fef2) SHA1(10d1e3d92288fccd4e064a3716a788a165efc3c9) )
	ROM_LOAD( "mb_q6.bin",      0x280000, 0x80000, CRC(ecb81b61) SHA1(e339f21ae47de4782f3b338befcdac659c3503f6) )
	ROM_LOAD( "mb_q7.bin",      0x300000, 0x80000, CRC(041e49ba) SHA1(3220b033a5c0cfbbe75c0c113cf2db39fb093a7e) )
	ROM_LOAD( "mb_q8.bin",      0x380000, 0x80000, CRC(59fe702a) SHA1(807178dfc6d864e49fd7aabb5c4895835cf0e85b) )
ROM_END

ROM_START( pnickj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "pnij36.bin",   0x00000, 0x20000, CRC(2d4ffb2b) SHA1(6e49cf89a36834fd1de8b4b860fe66f3d7d67a84) )
	ROM_LOAD16_BYTE( "pnij42.bin",   0x00001, 0x20000, CRC(c085dfaf) SHA1(a31ededc3413ec4f3f5e3a1fb615b60c6197f4a5) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "pnij09.bin",   0x000000, 0x20000, CRC(48177b0a) SHA1(eba5de6cd9bb0c4ad76a13bddc9cdeb2e4380122) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij01.bin",   0x000001, 0x20000, CRC(01a0f311) SHA1(9bcd8716f90ccd410543ffcdc5c2916077b8d4c3) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij13.bin",   0x000002, 0x20000, CRC(406451b0) SHA1(5a7a7fecba7de8b8cf4a284b2ae7adae901623f6) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij05.bin",   0x000003, 0x20000, CRC(8c515dc0) SHA1(aa1e13cf9e7cf0458bb5c4332b1ea73034f9a874) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij26.bin",   0x000004, 0x20000, CRC(e2af981e) SHA1(3c2b28b4a4d457aa94a760dfca0181a9f050c319) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij18.bin",   0x000005, 0x20000, CRC(f17a0e56) SHA1(7c89aca230f176e12f995892f9d1bce22c57fbdf) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij38.bin",   0x000006, 0x20000, CRC(eb75bd8c) SHA1(2129460e06eb64019fc5f7eab6334ff43229b995) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij32.bin",   0x000007, 0x20000, CRC(84560bef) SHA1(9e94ae434b50ecf82781080e11d0c4741e992d0d) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij10.bin",   0x100000, 0x20000, CRC(c2acc171) SHA1(7c86db3f2acca1252d403c5f12c871d0357fa109) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij02.bin",   0x100001, 0x20000, CRC(0e21fc33) SHA1(c4a29d45c4257c8871038d3c9b13140e874db0c1) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij14.bin",   0x100002, 0x20000, CRC(7fe59b19) SHA1(a273b8b8fbfd5d31d25479a9ede09ce35e1cc873) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij06.bin",   0x100003, 0x20000, CRC(79f4bfe3) SHA1(bc17cc1c8535e3d202588893713926b6c06f92fd) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij27.bin",   0x100004, 0x20000, CRC(83d5cb0e) SHA1(44c93fa5eedcafc8dc6d88ee827c6cadc9c671f0) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij19.bin",   0x100005, 0x20000, CRC(af08b230) SHA1(a3b5b3013012efa1860699648518f8d8031c5f30) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij39.bin",   0x100006, 0x20000, CRC(70fbe579) SHA1(b5b7ed5588ecd884b20dd50bfc5385a9af03c5d8) , ROM_SKIP(7) )
	ROMX_LOAD( "pnij33.bin",   0x100007, 0x20000, CRC(3ed2c680) SHA1(0afe84d8d89f8d45afc79f6172337e622e29a8a2) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "pnij17.bin",    0x00000, 0x08000, CRC(e86f787a) SHA1(de04cbe89c655faf04afe169bfd9913049ccc4a8) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "pnij24.bin",   0x00000, 0x20000, CRC(5092257d) SHA1(95dc9d10940653b2fb37baf5c1ed27145b02104e) )
	ROM_LOAD( "pnij25.bin",   0x20000, 0x20000, CRC(22109aaa) SHA1(cf21e75674d81b2daae2083d02f9f4b6e52722c6) )
ROM_END

ROM_START( qad )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "qdu_36a.rom",  0x00000, 0x20000, CRC(de9c24a0) SHA1(458962943e8d97d1f4e5a15ac1c8d3bcaa32918b) )
	ROM_LOAD16_BYTE( "qdu_42a.rom",  0x00001, 0x20000, CRC(cfe36f0c) SHA1(370a47461b2dbb7807f547f5b4b33296572c5d78) )
	ROM_LOAD16_BYTE( "qdu_37a.rom",  0x40000, 0x20000, CRC(10d22320) SHA1(73b2876d5447f50a850c466789d9297269f732d6) )
	ROM_LOAD16_BYTE( "qdu_43a.rom",  0x40001, 0x20000, CRC(15e6beb9) SHA1(68d11e9bdd82775060281c5880f249e3515dc235) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "qdu_09.rom", 0x000000, 0x20000, CRC(8c3f9f44) SHA1(b5ab20515b7f3e7db023be42d4c7ed1941b37d9b) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_01.rom", 0x000001, 0x20000, CRC(f688cf8f) SHA1(1b20095e536a24406513715cded249c9be1aa1d2) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_13.rom", 0x000002, 0x20000, CRC(afbd551b) SHA1(02e2f12196c542a004325689bda8949213ef0333) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_05.rom", 0x000003, 0x20000, CRC(c3db0910) SHA1(cf3aa3d3b64031dea92a80e5650151315cf871bf) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_24.rom", 0x000004, 0x20000, CRC(2f1bd0ec) SHA1(017e0dc521bf402c700775ee06cbc124f7ce0e3f) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_17.rom", 0x000005, 0x20000, CRC(a812f9e2) SHA1(9b7ceb347fbe00c40338b97ee6e8e4d1db9e7cb3) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_38.rom", 0x000006, 0x20000, CRC(ccdddd1f) SHA1(8304c4cdfaa1ae6b37e2733e9a6ddce9252fd43a) , ROM_SKIP(7) )
	ROMX_LOAD( "qdu_32.rom", 0x000007, 0x20000, CRC(a8d295d3) SHA1(d4d0bdaeb40f652ef33b317cb2b566b4c3550242) , ROM_SKIP(7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "qdu_23.rom",    0x00000, 0x08000, CRC(cfb5264b) SHA1(e662ed5555d02ccf4e62cdbcfa0bbfc019734ee1) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "qdu_30.rom",  0x00000, 0x20000, CRC(f190da84) SHA1(d5cd4c69b5d135a2f2fea8ca9631251c9da79e70) )
	ROM_LOAD( "qdu_31.rom",  0x20000, 0x20000, CRC(b7583f73) SHA1(3896e0fcf375e9e5d9ba70cc1ed001cd702f9ff7) )
ROM_END

ROM_START( qadj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "qad23a.bin",   0x00000, 0x080000, CRC(4d3553de) SHA1(07eabcb02fbbe11397ce91405a2e6bb53b3d5d4f) )
	ROM_LOAD16_WORD_SWAP( "qad22a.bin",   0x80000, 0x80000, CRC(3191ddd0) SHA1(2806021a5dc809ca43692bbe9c4f5ef690c9ac14) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "qad01.bin",   0x000000, 0x80000, CRC(9d853b57) SHA1(380b41a3eced1f4a5523999b63d80b7593a85eca) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "qad02.bin",   0x000002, 0x80000, CRC(b35976c4) SHA1(3e128db89186c4e88c46be9da310b755ae5b816c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "qad03.bin",   0x000004, 0x80000, CRC(cea4ca8c) SHA1(5c50758647419129f2b35ab4dc712796fa801c12) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "qad04.bin",   0x000006, 0x80000, CRC(41b74d1b) SHA1(78aa2faec512c505f98b4e8053fc161941d41773) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "qad09.bin",     0x00000, 0x08000, CRC(733161cc) SHA1(dfb8c5a1037bd3b2712fb327122ec39ceb993b8d) )
	ROM_CONTINUE(              0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "qad18.bin",   0x00000, 0x20000, CRC(2bfe6f6a) SHA1(b2a98ac034c65b7ac8167431f05f35d4799032ea) )
	ROM_LOAD( "qad19.bin",   0x20000, 0x20000, CRC(13d3236b) SHA1(785d49de484e9ac6971eaceebebfecb8e58563f6) )
ROM_END

ROM_START( qtono2 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_BYTE( "tn2j-30.11e",  0x00000, 0x20000, CRC(9226eb5e) SHA1(91649974f9652caed90eb28ec6caf4fe61f5d279) )
	ROM_LOAD16_BYTE( "tn2j-37.11f",  0x00001, 0x20000, CRC(d1d30da1) SHA1(7ca1695ed804b5860d4c15964cdbb922db3918ee) )
	ROM_LOAD16_BYTE( "tn2j-31.12e",  0x40000, 0x20000, CRC(015e6a8a) SHA1(0835bec4867438a167bd01e3550090c88e7ae779) )
	ROM_LOAD16_BYTE( "tn2j-38.12f",  0x40001, 0x20000, CRC(1f139bcc) SHA1(ee907f1bfef1a887e2c768648fe811e0733eddf7) )
	ROM_LOAD16_BYTE( "tn2j-28.9e",   0x80000, 0x20000, CRC(86d27f71) SHA1(89d6d18e05deaaa1ac7deb70ca03d051d2fde472) )
	ROM_LOAD16_BYTE( "tn2j-35.9f",   0x80001, 0x20000, CRC(7a1ab87d) SHA1(f1729a8c0c82cf42f60644a7796dc8a39bf7c6fa) )
	ROM_LOAD16_BYTE( "tn2j-29.10e",  0xc0000, 0x20000, CRC(9c384e99) SHA1(3d3961f625ccc4776531eff50fc1b4bee062370e) )
	ROM_LOAD16_BYTE( "tn2j-36.10f",  0xc0001, 0x20000, CRC(4c4b2a0a) SHA1(9a25fcfb9358ea42d9bc662df2cafea08febb411) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "tn2-02m.4a",   0x000000, 0x80000, CRC(f2016a34) SHA1(3862960fa14742547f6a6deacf0b9f409d08fee8) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-04m.6a",   0x000002, 0x80000, CRC(094e0fb1) SHA1(7c9a9a7d03e226109002dd389c872e3d4be43287) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-01m.3a",   0x000004, 0x80000, CRC(cb950cf9) SHA1(8337a500141c1aec82b6636ad79ecafbdbebd691) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-03m.5a",   0x000006, 0x80000, CRC(18a5bf59) SHA1(afbfcb28c40551747bb5276aac2b9c15a24328e1) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-11m.4c",   0x200000, 0x80000, CRC(d0edd30b) SHA1(a76d7f134f9e52f79a485402d17dcc7a1fe99f29) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-13m.6c",   0x200002, 0x80000, CRC(426621c3) SHA1(89156bc9d585f546cd619db419dd1f4d9871d930) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-10m.3c",   0x200004, 0x80000, CRC(a34ece70) SHA1(15864d6b280f624245add8a611f1699da570392b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tn2-12m.5c",   0x200006, 0x80000, CRC(e04ff2f4) SHA1(774c19909a2ae2c691f5d3f15b6e19cc94baf799) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "tn2j-09.12a",   0x00000, 0x08000, CRC(6d8edcef) SHA1(9ec2d64278b30cc4316238c3efee663a8bbb255e) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "tn2j-18.11c",  0x00000, 0x20000, CRC(a40bf9a7) SHA1(07cb1076262a281e31a621cbcc10be0cae883175) )
	ROM_LOAD( "tn2j-19.12c",  0x20000, 0x20000, CRC(5b3b931e) SHA1(cf28891f84814cbfaa3adaade8bb08b1e0546a3d) )
ROM_END

ROM_START( pang3 )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "pa3w-17.11l",  0x00000, 0x80000, CRC(12138234) SHA1(956a2c847a3cfb94007d1a636167fd2bb9f826ec) )
	ROM_LOAD16_WORD_SWAP( "pa3w-16.10l",  0x80000, 0x80000, CRC(d1ba585c) SHA1(c6d04441fe97abf0a72b23c917777a7b58e94a85) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "pa3-01m.2c",   0x000000, 0x100000, CRC(068a152c) SHA1(fa491874068924c39bcc7de93dfda3b27f5d9613) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROM_CONTINUE(              0x000004, 0x100000 )
	ROMX_LOAD( "pa3-07m.2f",   0x000002, 0x100000, CRC(3a4a619d) SHA1(cfe68e24632b53fb6cd6d03b2166d6b5ba28b778) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROM_CONTINUE(              0x000006, 0x100000 )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "pa3-11.11f",    0x00000, 0x08000, CRC(90a08c46) SHA1(7544adab2d7e052e0d21c920bff7841d9d718345) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "pa3-05.10d",    0x00000, 0x20000, CRC(73a10d5d) SHA1(999465e4fbc35a34746d2db61ad49f61403d5af7) )
	ROM_LOAD( "pa3-06.11d",    0x20000, 0x20000, CRC(affa4f82) SHA1(27b9292bbc121cf585f53297a79fe8f0d0a729ae) )
ROM_END

ROM_START( pang3j )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "pa3j-17.11l",  0x00000, 0x80000, CRC(21f6e51f) SHA1(b447e05261f59b3b2e89bbc0f606d7136b29cb56) )
	ROM_LOAD16_WORD_SWAP( "pa3j-16.10l",  0x80000, 0x80000, CRC(ca1d7897) SHA1(46aa9232e81a838f3eff1e9b992492a264914fd5) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "pa3-01m.2c",   0x000000, 0x100000, CRC(068a152c) SHA1(fa491874068924c39bcc7de93dfda3b27f5d9613) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROM_CONTINUE(              0x000004, 0x100000 )
	ROMX_LOAD( "pa3-07m.2f",   0x000002, 0x100000, CRC(3a4a619d) SHA1(cfe68e24632b53fb6cd6d03b2166d6b5ba28b778) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROM_CONTINUE(              0x000006, 0x100000 )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "pa3-11.11f",    0x00000, 0x08000, CRC(90a08c46) SHA1(7544adab2d7e052e0d21c920bff7841d9d718345) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "pa3-05.10d",    0x00000, 0x20000, CRC(73a10d5d) SHA1(999465e4fbc35a34746d2db61ad49f61403d5af7) )
	ROM_LOAD( "pa3-06.11d",    0x20000, 0x20000, CRC(affa4f82) SHA1(27b9292bbc121cf585f53297a79fe8f0d0a729ae) )
ROM_END

ROM_START( megaman )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "rcma_23b.rom",   0x000000, 0x80000, CRC(61e4a397) SHA1(a90b1cbef4206a4554398bc458a4b3e2c46d4c4f) )
	ROM_LOAD16_WORD_SWAP( "rcma_22b.rom",   0x080000, 0x80000, CRC(708268c4) SHA1(554e011cad285b95dd1b6aa19be61b2413662a3a) )
	ROM_LOAD16_WORD_SWAP( "rcma_21a.rom",   0x100000, 0x80000, CRC(4376ea95) SHA1(7370ceffca513aa9f68a74f6869d561476589200) )

	ROM_REGION( 0x800000, REGION_GFX1, 0 )
	ROMX_LOAD( "rcm_01.rom",    0x000000, 0x80000, CRC(6ecdf13f) SHA1(2a8fe06bf5011e3f990f90d9224f91d8631ec0cc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_02.rom",    0x000002, 0x80000, CRC(944d4f0f) SHA1(665dc9a537e9c9b565f6136f939ff5c2861f875f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_03.rom",    0x000004, 0x80000, CRC(36f3073c) SHA1(457d68e63599d06a136e152a9ad60adac1c91edd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_04.rom",    0x000006, 0x80000, CRC(54e622ff) SHA1(36f6297e3d410f041be5e582919478b0d52520ca) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_05.rom",    0x200000, 0x80000, CRC(5dd131fd) SHA1(1a7fc8cf38901245d40901996e946e7ad9c0e0c5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_06.rom",    0x200002, 0x80000, CRC(f0faf813) SHA1(adff01c2ecc4c8ce6f8a50cbd07d8f8bb9f48168) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_07.rom",    0x200004, 0x80000, CRC(826de013) SHA1(47f36b1d92a487c43c8dadc8293b8e6f40649286) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_08.rom",    0x200006, 0x80000, CRC(fbff64cf) SHA1(f0cb531ef195dc1dcd224a208906a62fb5d199a1) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_10.rom",    0x400000, 0x80000, CRC(4dc8ada9) SHA1(776c2b3ef24c2b8f390c05a9c6728b14ceec696e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_11.rom",    0x400002, 0x80000, CRC(f2b9ee06) SHA1(db315b00d1caed1a8c0f6e0ae726e8fa05b011fa) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_12.rom",    0x400004, 0x80000, CRC(fed5f203) SHA1(23db14490519b5e2d0bb92ffe6e14540d1999e4b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_13.rom",    0x400006, 0x80000, CRC(5069d4a9) SHA1(b832b98be94371af52bd4bb911e18ec57430a7db) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_14.rom",    0x600000, 0x80000, CRC(303be3bd) SHA1(1e5c3fd71966ea9f457840c40582795b501c323e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_15.rom",    0x600002, 0x80000, CRC(4f2d372f) SHA1(db6a94d1f92c1b96e404b38ebcb1eedbec3ae6cc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_16.rom",    0x600004, 0x80000, CRC(93d97fde) SHA1(e4be5216f98ad08a9118d629d398be2bd54e2e2a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_17.rom",    0x600006, 0x80000, CRC(92371042) SHA1(c55833cbaddcc986edd23c009a3e3c7ff09c2708) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x28000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "rcm_09.rom",    0x00000, 0x08000, CRC(9632d6ef) SHA1(2bcb6f17005ffbc9ef8fa4478a814f24b2e6e0b6) )
	ROM_CONTINUE(              0x10000, 0x18000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "rcm_18.rom",    0x00000, 0x20000, CRC(80f1f8aa) SHA1(4a5b7b2a6941ad68da7472c63362c7bcd353fa54) )
	ROM_LOAD( "rcm_19.rom",    0x20000, 0x20000, CRC(f257dbe1) SHA1(967def6b6f93039dbc46373caabeb3301577be75) )
ROM_END

ROM_START( rockmanj )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "rcm23a.bin",   0x000000, 0x80000, CRC(efd96cb2) SHA1(cbe81554f60d0c897f3f2ebc5bc966bb03cc23fe) )
	ROM_LOAD16_WORD_SWAP( "rcm22a.bin",   0x080000, 0x80000, CRC(8729a689) SHA1(14ddb34d8201c544ea9d3d8c2cc52d380bc72930) )
	ROM_LOAD16_WORD_SWAP( "rcm21a.bin",   0x100000, 0x80000, CRC(517ccde2) SHA1(492256c192f0c4814efa1ee1dd390453dd2e5865) )

	ROM_REGION( 0x800000, REGION_GFX1, 0 )
	ROMX_LOAD( "rcm_01.rom",    0x000000, 0x80000, CRC(6ecdf13f) SHA1(2a8fe06bf5011e3f990f90d9224f91d8631ec0cc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_02.rom",    0x000002, 0x80000, CRC(944d4f0f) SHA1(665dc9a537e9c9b565f6136f939ff5c2861f875f) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_03.rom",    0x000004, 0x80000, CRC(36f3073c) SHA1(457d68e63599d06a136e152a9ad60adac1c91edd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_04.rom",    0x000006, 0x80000, CRC(54e622ff) SHA1(36f6297e3d410f041be5e582919478b0d52520ca) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_05.rom",    0x200000, 0x80000, CRC(5dd131fd) SHA1(1a7fc8cf38901245d40901996e946e7ad9c0e0c5) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_06.rom",    0x200002, 0x80000, CRC(f0faf813) SHA1(adff01c2ecc4c8ce6f8a50cbd07d8f8bb9f48168) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_07.rom",    0x200004, 0x80000, CRC(826de013) SHA1(47f36b1d92a487c43c8dadc8293b8e6f40649286) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_08.rom",    0x200006, 0x80000, CRC(fbff64cf) SHA1(f0cb531ef195dc1dcd224a208906a62fb5d199a1) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_10.rom",    0x400000, 0x80000, CRC(4dc8ada9) SHA1(776c2b3ef24c2b8f390c05a9c6728b14ceec696e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_11.rom",    0x400002, 0x80000, CRC(f2b9ee06) SHA1(db315b00d1caed1a8c0f6e0ae726e8fa05b011fa) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_12.rom",    0x400004, 0x80000, CRC(fed5f203) SHA1(23db14490519b5e2d0bb92ffe6e14540d1999e4b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_13.rom",    0x400006, 0x80000, CRC(5069d4a9) SHA1(b832b98be94371af52bd4bb911e18ec57430a7db) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_14.rom",    0x600000, 0x80000, CRC(303be3bd) SHA1(1e5c3fd71966ea9f457840c40582795b501c323e) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_15.rom",    0x600002, 0x80000, CRC(4f2d372f) SHA1(db6a94d1f92c1b96e404b38ebcb1eedbec3ae6cc) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_16.rom",    0x600004, 0x80000, CRC(93d97fde) SHA1(e4be5216f98ad08a9118d629d398be2bd54e2e2a) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "rcm_17.rom",    0x600006, 0x80000, CRC(92371042) SHA1(c55833cbaddcc986edd23c009a3e3c7ff09c2708) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */

	ROM_REGION( 0x28000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "rcm_09.rom",    0x00000, 0x08000, CRC(9632d6ef) SHA1(2bcb6f17005ffbc9ef8fa4478a814f24b2e6e0b6) )
	ROM_CONTINUE(              0x10000, 0x18000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "rcm_18.rom",    0x00000, 0x20000, CRC(80f1f8aa) SHA1(4a5b7b2a6941ad68da7472c63362c7bcd353fa54) )
	ROM_LOAD( "rcm_19.rom",    0x20000, 0x20000, CRC(f257dbe1) SHA1(967def6b6f93039dbc46373caabeb3301577be75) )
ROM_END
         
/* CPS Prototype */                                        

ROM_START( gulunpa )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
  ROM_LOAD16_BYTE( "26",  0x00000, 0x020000, CRC(f30ffa29) SHA1(9e70daf4229485dc5700b074dba55839c7357351) )
  ROM_LOAD16_BYTE( "30",  0x00001, 0x020000, CRC(5d35f737) SHA1(47b6bfa6eaa512684e12c23162243d1a21cb1a7a) )

	ROM_REGION( 0x200000, REGION_GFX1, 0 )
	ROMX_LOAD( "1",     0x000000, 0x080000, CRC(b55e648f) SHA1(e22eec707b3b1ad8fb93c0f2df41ccf72cd03440) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "2",     0x000002, 0x080000, CRC(ad033bce) SHA1(b37b1d341e61502aa4213b049b14974fab8a0445) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "3",     0x000004, 0x080000, CRC(36c3951a) SHA1(74edaca2c78dd6a304ea702091a9f0b7f6036e41) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "4",     0x000006, 0x080000, CRC(ff0cb826) SHA1(fec7833652e6789e886a1ec7b4680a608ddbbe20) , ROM_GROUPWORD | ROM_SKIP(6) )
  ROM_FILL(0x000006, 1, 0xff)
	
	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */
	
	ROM_REGION( 0x18000, REGION_CPU2, 0 ) /* 64k for the audio CPU (+banks) */
	ROM_LOAD( "9",      0x00000, 0x08000, CRC(15afd06f) SHA1(1a4ff3e11e55266e7c93743b6564c226eaaba142) )
	ROM_CONTINUE(       0x10000, 0x08000 )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "18",     0x00000, 0x20000, CRC(9997a34f) SHA1(8e107d6413836c48fc57e4a9b89ae99a9e381e8b) )
	ROM_LOAD( "19",     0x20000, 0x20000, CRC(e95270ac) SHA1(dc684abfa1ea276a00ec541ab8f3f9f131375faa) )
ROM_END                                        
                                        
/* Home 'CPS Changer' Unit  */

/* B-Board 91635B-2 */
ROM_START( wofch )
	ROM_REGION( CODE_SIZE, REGION_CPU1, 0 )      /* 68000 code */
	ROM_LOAD16_WORD_SWAP( "tk2=ch=_23.8f", 0x000000, 0x80000, CRC(4e0b8dee) SHA1(d2fb716d62b7a259f46bbc74c1976a18d56696ea) )
	ROM_LOAD16_WORD_SWAP( "tk2=ch=_22.7f", 0x080000, 0x80000, CRC(d0937a8d) SHA1(01d7be446e2e3ef8ca767f59c178240dfd52dd93) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 )
	ROMX_LOAD( "tk2-1m.3a",      0x000000, 0x80000, CRC(0d9cb9bf) SHA1(cc7140e9a01a14b252cb1090bcea32b0de461928) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2-3m.5a",      0x000002, 0x80000, CRC(45227027) SHA1(b21afc593f0d4d8909dfa621d659cbb40507d1b2) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2-2m.4a",      0x000004, 0x80000, CRC(c5ca2460) SHA1(cbe14867f7b94b638ca80db7c8e0c60881183469) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2-4m.6a",      0x000006, 0x80000, CRC(e349551c) SHA1(1d977bdf256accf750ad9930ec4a0a19bbf86964) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "tk2=ch=_05.7a",  0x200000, 0x80000, CRC(e4a44d53) SHA1(b747679f4d63e5e62d9fd81b3120fba0401fadfb) , ROM_GROUPWORD | ROM_SKIP(6) )    // == tk2_05.7a
	ROMX_LOAD( "tk2=ch=_06.8a",  0x200002, 0x80000, CRC(58066ba8) SHA1(c93af968e21094d020e4b2002e0c6fc0d746af0b) , ROM_GROUPWORD | ROM_SKIP(6) )    // == tk2_06.8a
	ROMX_LOAD( "tk2=ch=_07.9a",  0x200004, 0x80000, CRC(cc9006c9) SHA1(cfcbec3a67052268a7739538aa28a6391fe5400e) , ROM_GROUPWORD | ROM_SKIP(6) )    /* 1 byte different from wofj, pcb verified */
	ROMX_LOAD( "tk2=ch=_08.10a", 0x200006, 0x80000, CRC(d4a19a02) SHA1(ff396b1d33d9b4842140f2c6d085fe05748e3244) , ROM_GROUPWORD | ROM_SKIP(6) )    // == tk2_08.10a

	ROM_REGION( 0x8000, REGION_GFX2, 0 )
	ROM_COPY( REGION_GFX1, 0x000000, 0x000000, 0x8000 )	/* stars */
	
	ROM_REGION( 2*0x28000, REGION_CPU2, 0 ) /* QSound Z80 code + space for decrypted opcodes */
	ROM_LOAD( "tk2_qa.5k",       0x00000, 0x08000, CRC(c9183a0d) SHA1(d8b1d41c572f08581f8ab9eb878de77d6ea8615d) )
	ROM_CONTINUE(                0x10000, 0x18000 )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* QSound samples */
	ROM_LOAD( "tk2-q1.1k",       0x000000, 0x80000, CRC(611268cf) SHA1(83ab059f2110fb25fdcff928d56b790fc1f5c975) )
	ROM_LOAD( "tk2-q2.2k",       0x080000, 0x80000, CRC(20f55ca9) SHA1(90134e9a9c4749bb65c728b66ea4dac1fd4d88a4) )
	ROM_LOAD( "tk2-q3.3k",       0x100000, 0x80000, CRC(bfcf6f52) SHA1(2a85ff3fc89b4cbabd20779ec12da2e116333c7c) )
	ROM_LOAD( "tk2-q4.4k",       0x180000, 0x80000, CRC(36642e88) SHA1(8ab25b19e2b67215a5cb1f3aa81b9d26009cfeb8) )
ROM_END		
					


static DRIVER_INIT( wof )
{
	wof_decode();
	init_cps1();
}

static DRIVER_INIT( dino )
{
	dino_decode();
	init_cps1();
}

static DRIVER_INIT( punisher )
{
	punisher_decode();
	init_cps1();
}

static DRIVER_INIT( slammast )
{
	slammast_decode();
	init_cps1();
}

static DRIVER_INIT( pang3 )
{
	data16_t *rom = (data16_t *)memory_region(REGION_CPU1);
	int A,src,dst;

	for (A = 0x80000;A < 0x100000;A += 2)
	{
		/* only the low 8 bits of each word are encrypted */
		src = rom[A/2];
		dst = src & 0xff00;
		if ( src & 0x01) dst ^= 0x04;
		if ( src & 0x02) dst ^= 0x21;
		if ( src & 0x04) dst ^= 0x01;
		if (~src & 0x08) dst ^= 0x50;
		if ( src & 0x10) dst ^= 0x40;
		if ( src & 0x20) dst ^= 0x06;
		if ( src & 0x40) dst ^= 0x08;
		if (~src & 0x80) dst ^= 0x88;
		rom[A/2] = dst;
	}

	init_cps1();
}


GAME( 1988, forgottn, 0,        forgottn, forgottn, cps1,     ROT0,   "Capcom", "Forgotten Worlds (US)" )
GAME( 1988, lostwrld, forgottn, forgottn, forgottn, cps1,     ROT0,   "Capcom", "Lost Worlds (Japan)" )
GAME( 1988, ghouls,   0,        cps1,     ghouls,   cps1,     ROT0,   "Capcom", "Ghouls'n Ghosts (World)" )						/* Wed.26.10.1988 in the ROMS*/
GAME( 1988, ghoulsu,  ghouls,   cps1,     ghoulsu,  cps1,     ROT0,   "Capcom", "Ghouls'n Ghosts (US)" )						/* Wed.26.10.1988 in the ROMS*/
GAME( 1988, daimakai, ghouls,   cps1,     daimakai, cps1,     ROT0,   "Capcom", "Dai Makai-Mura (Japan)" )						/* Wed.26.10.1988 in the ROMS*/
GAME( 1989, strider,  0,        cps1,     strider,  cps1,     ROT0,   "Capcom", "Strider (US set 1)" )
GAME( 1989, stridrua, strider,  cps1,     stridrua, cps1,     ROT0,   "Capcom", "Strider (US set 2)" )
GAME( 1989, striderj, strider,  cps1,     strider,  cps1,     ROT0,   "Capcom", "Strider Hiryu (Japan set 1)" )
GAME( 1989, stridrja, strider,  cps1,     strider,  cps1,     ROT0,   "Capcom", "Strider Hiryu (Japan set 2)" )
GAME( 1989, dw,       0,        cps1,     dw,       cps1,     ROT0,   "Capcom", "Dynasty Wars (World)" )						/* (c) Capcom U.S.A.*/
GAME( 1989, dwj,      dw,       cps1,     dw,       cps1,     ROT0,   "Capcom", "Tenchi wo Kurau (Japan)" )
GAME( 1989, willow,   0,        cps1,     willow,   cps1,     ROT0,   "Capcom", "Willow (US)" )
GAME( 1989, willowj,  willow,   cps1,     willow,   cps1,     ROT0,   "Capcom", "Willow (Japan, Japanese)" )					/* Japan - warning*/
GAME( 1989, willowje, willow,   cps1,     willow,   cps1,     ROT0,   "Capcom", "Willow (Japan, English)" )						/* (c) Capcom U.S.A. but Japan - warning*/
GAME( 1989, unsquad,  0,        cps1,     unsquad,  cps1,     ROT0,   "Capcom", "U.N. Squadron (US)" )
GAME( 1989, area88,   unsquad,  cps1,     unsquad,  cps1,     ROT0,   "Capcom", "Area 88 (Japan)" )

GAME( 1989, ffight,   0,        ffight_hack,     ffight,   cps1,     ROT0,   "Capcom", "Final Fight (World)" )
GAME( 1989, ffightu,  ffight,   ffight_hack,     ffight,   cps1,     ROT0,   "Capcom", "Final Fight (US 900112)" )
GAME( 1989, ffightj,  ffight,   ffight_hack,     ffight,   cps1,     ROT0,   "Capcom", "Final Fight (Japan set 1)" )
GAME( 1989, ffightj1, ffight,   ffight_hack,     ffight,   cps1,     ROT0,   "Capcom", "Final Fight (Japan set 2)" )
GAME( 1989, ffightae, ffight,   ffight_hack,     ffightae, cps1,     ROT0,   "bootleg", "Final Fight 30th Anniversary Edition" )


GAME( 1990, 1941,     0,        cps1,     1941,     cps1,     ROT270, "Capcom", "1941 - Counter Attack (World)" )
GAME( 1990, 1941j,    1941,     cps1,     1941,     cps1,     ROT270, "Capcom", "1941 - Counter Attack (Japan)" )
GAME( 1990, mercs,    0,        cps1,     mercs,    cps1,     ROT270, "Capcom", "Mercs (World 900302)" )						/* ETC*/
GAME( 1990, mercsu,   mercs,    cps1,     mercs,    cps1,     ROT270, "Capcom", "Mercs (US 900302)" )
GAME( 1990, mercsua,  mercs,    cps1,     mercs,    cps1,     ROT270, "Capcom", "Mercs (US 900608)" )
GAME( 1990, mercsj,   mercs,    cps1,     mercs,    cps1,     ROT270, "Capcom", "Senjou no Ookami II (Japan 900302)" )
GAME( 1990, mtwins,   0,        cps1,     mtwins,   cps1,     ROT0,   "Capcom", "Mega Twins (World 900619)" )					/* ETC - (c) Capcom U.S.A. with World - warning */
GAME( 1990, chikij,   mtwins,   cps1,     mtwins,   cps1,     ROT0,   "Capcom", "Chiki Chiki Boys (Japan 900619)" )
GAME( 1990, msword,   0,        cps1,     msword,   cps1,     ROT0,   "Capcom", "Magic Sword - Heroic Fantasy (World 900725)" )		/* 25.07.1990  Other Country*/
GAME( 1990, mswordr1, msword,   cps1,     msword,   cps1,     ROT0,   "Capcom", "Magic Sword - Heroic Fantasy (World 900623)" )		/* 23.06.1990  Other Country*/
GAME( 1990, mswordu,  msword,   cps1,     msword,   cps1,     ROT0,   "Capcom", "Magic Sword - Heroic Fantasy (US 900725)" )			/* 25.07.1990  U.S.A.*/
GAME( 1990, mswordj,  msword,   cps1,     msword,   cps1,     ROT0,   "Capcom", "Magic Sword (Japan 900623)" )					/* 23.06.1990  Japan*/
GAME( 1990, cawing,   0,        cps1,     cawing,   cps1,     ROT0,   "Capcom", "Carrier Air Wing (World 901012)" )				/* ETC*/
GAME( 1990, cawingu,  cawing,   cps1,     cawing,   cps1,     ROT0,   "Capcom", "Carrier Air Wing (US 901012)" )
GAME( 1990, cawingj,  cawing,   cps1,     cawing,   cps1,     ROT0,   "Capcom", "U.S. Navy (Japan 901012)" )
GAME( 1990, nemo,     0,        cps1,     nemo,     cps1,     ROT0,   "Capcom", "Nemo (World 901130)" )						/* ETC */
GAME( 1990, nemoj,    nemo,     cps1,     nemo,     cps1,     ROT0,   "Capcom", "Nemo (Japan 901120)" )
GAMEC(1991, sf2,      0,        sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (World 910522)", &sf2_ctrl, NULL )	/* ETC*/
GAMEC(1991, sf2eb,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (World 910214)", &sf2_ctrl, NULL )	/* ETC*/
GAMEC(1991, sf2ua,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 910206)", &sf2_ctrl, NULL )
GAMEC(1991, sf2ub,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 910214)", &sf2_ctrl, NULL )
GAMEC(1991, sf2ud,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 910318)", &sf2_ctrl, NULL )
GAMEC(1991, sf2ue,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 910228)", &sf2_ctrl, NULL )
GAMEC(1991, sf2uf,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 910411)", &sf2_ctrl, NULL )
GAMEC(1991, sf2ui,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 910522)", &sf2_ctrl, NULL )
GAMEC(1991, sf2uk,    sf2,      sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (US 911101)", &sf2_ctrl, NULL )
GAMEC(1991, sf2j,     sf2,      sf2,      sf2j,     cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (Japan 911210)", &sf2_ctrl, NULL )
GAMEC(1991, sf2ja,    sf2,      sf2,      sf2j,     cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (Japan 910214)", &sf2_ctrl, NULL )
GAMEC(1991, sf2jc,    sf2,      sf2,      sf2j,     cps1,     ROT0,   "Capcom", "Street Fighter II - The World Warrior (Japan 910306)", &sf2_ctrl, NULL )
GAME( 1991, 3wonders, 0,        cps1,     3wonders, cps1,     ROT0,   "Capcom", "Three Wonders (World 910520)" )					/* ETC */
GAME( 1991, 3wonderu, 3wonders, cps1,     3wonders, cps1,     ROT0,   "Capcom", "Three Wonders (US 910520)" )
GAME( 1991, wonder3,  3wonders, cps1,     3wonders, cps1,     ROT0,   "Capcom", "Wonder 3 (Japan 910520)" )
GAME( 1991, kod,      0,        cps1,     kod,      cps1,     ROT0,   "Capcom", "The King of Dragons (World 910711)" )				/* ETC */
GAME( 1991, kodu,     kod,      cps1,     kodj,     cps1,     ROT0,   "Capcom", "The King of Dragons (US 910910)" )
GAME( 1991, kodj,     kod,      cps1,     kodj,     cps1,     ROT0,   "Capcom", "The King of Dragons (Japan 910805)" )
GAMEX(1991, kodb,     kod,      cps1,     kod,      cps1,     ROT0,   "Capcom", "The King of Dragons (bootleg)", GAME_NOT_WORKING )		/* 910731  ETC*/
GAME( 1991, captcomm, 0,        cps1,     captcomm, cps1,     ROT0,   "Capcom", "Captain Commando (World 911014)" )				/* OTHER COUNTRY*/
GAME( 1991, captcomu, captcomm, cps1,     captcomm, cps1,     ROT0,   "Capcom", "Captain Commando (US 910928)" )
GAME( 1991, captcomj, captcomm, cps1,     captcomm, cps1,     ROT0,   "Capcom", "Captain Commando (Japan 911202)" )
GAME( 1991, knights,  0,        cps1,     knights,  cps1,     ROT0,   "Capcom", "Knights of the Round (World 911127)" )				/* ETC */
GAME( 1991, knightsu, knights,  cps1,     knights,  cps1,     ROT0,   "Capcom", "Knights of the Round (US 911127)" )
GAME( 1991, knightsj, knights,  cps1,     knights,  cps1,     ROT0,   "Capcom", "Knights of the Round (Japan 911127)" )
GAMEC(1992, sf2ce,    0,        sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II' - Champion Edition (World 920313)", &sf2_ctrl, NULL )	/* ETC */
GAMEC(1992, sf2ceua,  sf2ce,    sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II' - Champion Edition (US 920313)", &sf2_ctrl, NULL )
GAMEC(1992, sf2ceub,  sf2ce,    sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II' - Champion Edition (US 920513)", &sf2_ctrl, NULL )
GAMEC(1992, sf2ceuc,  sf2ce,    sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II' - Champion Edition (US 920803)", &sf2_ctrl, NULL )
GAMEC(1992, sf2cej,   sf2ce,    sf2,      sf2j,     cps1,     ROT0,   "Capcom", "Street Fighter II' - Champion Edition (Japan 920513)", &sf2_ctrl, NULL )
GAMEC(1992, sf2rb,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (Rainbow set 1)", &sf2_ctrl, NULL )	/* 920322 - based on World version*/
GAMEC(1992, sf2rb2,   sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (Rainbow set 2)", &sf2_ctrl, NULL )	/* 920322 - based on World version*/
GAMEC(1992, sf2red,   sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (Red Wave)", &sf2_ctrl, NULL )		/* 920313 - based on World version*/
GAMEC(1992, sf2v004,  sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II! - Champion Edition (V004)", &sf2_ctrl, NULL )		/* 102092 !!! - based on (heavily modified) World version*/
GAMEC(1992, sf2accp2, sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (Accelerator Pt.II)", &sf2_ctrl, NULL )  /* 920313 - based on USA version*/
GAMECX(1992, sf2m1,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M1)", GAME_NOT_WORKING, &sf2_ctrl, NULL )
GAMECX(1992, sf2m2,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M2)", GAME_NOT_WORKING, &sf2_ctrl, NULL )
GAMECX(1992, sf2m3,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M3)", GAME_NOT_WORKING , &sf2_ctrl, NULL )
GAMEC(1992, sf2m4,    sf2ce,    sf2,      sf2j,     cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M4)", &sf2_ctrl, NULL )
GAMEC(1992, sf2m5,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M5)", &sf2_ctrl, NULL )
GAMEC(1992, sf2m6,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M6)", &sf2_ctrl, NULL )
GAMEC(1992, sf2m7,    sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (M7)", &sf2_ctrl, NULL )
GAMEC(1992, sf2yyc,   sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (YYC)", &sf2_ctrl, NULL )
GAMEC(1992, sf2koryu, sf2ce,    sf2,      sf2,      cps1,     ROT0,   "bootleg","Street Fighter II' - Champion Edition (Kouryu)", &sf2_ctrl, NULL )
GAME( 1992, varth,    0,        cps1,     varth,    cps1,     ROT270, "Capcom", "Varth - Operation Thunderstorm (World 920612)" )		/* ETC */
GAME( 1992, varthu,   varth,    cps1,     varth,    cps1,     ROT270, "Capcom (Romstar license)", "Varth - Operation Thunderstorm (US 920612)" )
GAME( 1992, varthj,   varth,    cps1,     varth,    cps1,     ROT270, "Capcom", "Varth - Operation Thunderstorm (Japan 920714)" )
GAME( 1992, cworld2j, 0,        cps1,     cworld2j, cps1,     ROT0,   "Capcom", "Capcom World 2 (Japan 920611)" )
GAME( 1992, sf2t,     sf2ce,    sf2,      sf2,      cps1,     ROT0,   "Capcom", "Street Fighter II' - Hyper Fighting (US 921209)" )
GAME( 1992, sf2tj,    sf2ce,    sf2,      sf2j,     cps1,     ROT0,   "Capcom", "Street Fighter II' Turbo - Hyper Fighting (Japan 921209)" )
GAME( 1992, qad,      0,        cps1,     qad,      cps1,     ROT0,   "Capcom", "Quiz and Dragons (US 920701)" )
GAME( 1994, qadj,     qad,      cps1,     qadj,     cps1,     ROT0,   "Capcom", "Quiz and Dragons (Japan 940921)" )
GAME( 1995, qtono2,   0,        cps1,     qtono2,   cps1,     ROT0,   "Capcom", "Quiz Tonosama no Yabou 2 Zenkoku-ban (Japan 950123)" )
GAME( 1995, megaman,  0,        cps1,     megaman,  cps1,     ROT0,   "Capcom", "Mega Man - The Power Battle (CPS1 Asia 951006)" )
GAME( 1995, rockmanj, megaman,  cps1,     megaman,  cps1,     ROT0,   "Capcom", "Rockman - The Power Battle (CPS1 Japan 950922)" )

GAME( 1992, wof,      0,        qsound,   wof,      wof,      ROT0,   "Capcom", "Warriors of Fate (World 921002)" )				/* ETC */
GAME( 1992, wofa,     wof,      qsound,   wof,      wof,      ROT0,   "Capcom", "Sangokushi II (Asia 921005)" )					/* World - warning */
GAME( 1992, wofu,     wof,      qsound,   wof,      wof,      ROT0,   "Capcom", "Warriors of Fate (US 921031)" )					/* World - warning */
GAME( 1992, wofj,     wof,      qsound,   wof,      wof,      ROT0,   "Capcom", "Tenchi wo Kurau II - Sekiheki no Tatakai (Japan 921031)" )
GAME( 1993, dino,     0,        qsound,   dino,     dino,     ROT0,   "Capcom", "Cadillacs and Dinosaurs (World 930201)" )			/* ETC */
GAME( 1993, dinou,    dino,     qsound,   dino,     dino ,    ROT0,   "Capcom", "Cadillacs and Dinosaurs (US 930201)" )
GAME( 1993, dinoj,    dino,     qsound,   dino,     dino ,    ROT0,   "Capcom", "Cadillacs Kyouryuu-Shinseiki (Japan 930201)" )
GAME( 1993, punisher, 0,        qsound,   punisher, punisher, ROT0,   "Capcom", "The Punisher (World 930422)" )					/* ETC*/
GAME( 1993, punishru, punisher, qsound,   punisher, punisher, ROT0,   "Capcom", "The Punisher (US 930422)" )
GAME( 1993, punishrj, punisher, qsound,   punisher, punisher, ROT0,   "Capcom", "The Punisher (Japan 930422)" )
GAME( 1993, slammast, 0,        qsound,   slammast, slammast, ROT0,   "Capcom", "Saturday Night Slam Masters (World 930713)" )		/* ETC */
GAME( 1993, slammasu, slammast, qsound,   slammast, slammast, ROT0,   "Capcom", "Saturday Night Slam Masters (US 930713)" )
GAME( 1993, mbomberj, slammast, qsound,   slammast, slammast, ROT0,   "Capcom", "Muscle Bomber - The Body Explosion (Japan 930713)" )
GAME( 1993, mbombrd,  slammast, qsound,   slammast, slammast, ROT0,   "Capcom", "Muscle Bomber Duo - Ultimate Team Battle (World 931206)" )  /* ETC */
GAME( 1993, mbombrdj, slammast, qsound,   slammast, slammast, ROT0,   "Capcom", "Muscle Bomber Duo - Heat Up Warriors (Japan 931206)" )

GAME( 1994, pnickj,   0,        cps1,     pnickj,   cps1,     ROT0,   "Compile (Capcom license)", "Pnickies (Japan 940608)" )
/* Japanese version of Pang 3 is encrypted, Euro version is not */
GAME( 1995, pang3,    0,        pang3,    pang3,    cps1,     ROT0,   "Mitchell", "Pang! 3 (Euro 950511)" )
GAME( 1995, pang3j,   pang3,    pang3,    pang3,    pang3,    ROT0,   "Mitchell", "Pang! 3 (Japan 950511)" )
                                        
/* CPS Prototype */
GAME( 1993, gulunpa,  0,        cps1,     gulunpa,  cps1,     ROT0,   "Capcom", "Gulun.Pa! (Japan 931220 L Prototype)" )
                                        			
/* CPS Changer */
GAME( 1994, wofch,    0,        qsound,   wofch,    wof,      ROT0,   "Capcom", "Tenchi wo Kurau II: Sekiheki no Tatakai (CPS Changer, Japan 921031)" )
