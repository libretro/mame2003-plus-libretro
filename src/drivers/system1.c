/******************************************************************************

Sega System 1 / System 2

driver by Jarek Parchanski, Nicola Salmoria, Mirko Buffoni


Up'n Down, Mister Viking, Flicky, SWAT, Water Match and Bull Fight are known
to run on IDENTICAL hardware (they were sold by Bally-Midway as ROM swaps).

TODO: - background is misplaced in wbmlju
	  - sprites stick in Pitfall II
	  - sprite priorities are probably wrong
	  - remove patch in noboranb if possible and fully understand the
	    ports involved in the protection

******************************************************************************/

#include "driver.h"
#include "vidhrdw/system1.h"
#include "cpu/z80/z80.h"
#include "machine/segacrpt.h"


static UINT8 *system1_ram;
static UINT8 gun_trigger = 0;
static UINT8 gun_output = 0;

static MACHINE_INIT( system1 )
{
	/* skip the long IC CHECK in Teddyboy Blues and Choplifter */
	/* this is not a ROM patch, the game checks a RAM location */
	/* before doing the test */
/*	memory_region(REGION_CPU1)[0xeffe] = 0x4f;*/
/*	memory_region(REGION_CPU1)[0xefff] = 0x4b;*/

	system1_define_background_memory(system1_BACKGROUND_MEMORY_SINGLE);
}

static MACHINE_INIT( wbml )
{
	/* skip the long IC CHECK in Teddyboy Blues and Choplifter */
	/* this is not a ROM patch, the game checks a RAM location */
	/* before doing the test */
/*	memory_region(REGION_CPU1)[0xeffe] = 0x4f;*/
/*	memory_region(REGION_CPU1)[0xefff] = 0x4b;*/

	system1_define_background_memory(system1_BACKGROUND_MEMORY_BANKED);
}

/* Noboranka: there seems to be some protection? involving reads / writes to ports in the 2x region*/

static int inport16_step,inport17_step,inport23_step;

static READ_HANDLER( inport16_r )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "IN  $16 : pc = %04x - data = %02x\n",activecpu_get_pc(),inport16_step);*/
	return(inport16_step);
}

static READ_HANDLER( inport1c_r )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "IN  $1c : pc = %04x - data = 0x80\n",activecpu_get_pc());*/
	return(0x80);	/* infinite loop (at 0x0fb3) until bit 7 is set*/
}

static READ_HANDLER( inport22_r )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "IN  $22 : pc = %04x - data = %02x\n",activecpu_get_pc(),inport17_step);*/
	return(inport17_step);
}

static READ_HANDLER( inport23_r )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "IN  $23 : pc = %04x - step = %02x\n",activecpu_get_pc(),inport23_step);*/
	return(inport23_step);
}

static WRITE_HANDLER( outport16_w )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "OUT $16 : pc = %04x - data = %02x\n",activecpu_get_pc(),data);*/
	inport16_step = data;
}

static WRITE_HANDLER( outport17_w )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "OUT $17 : pc = %04x - data = %02x\n",activecpu_get_pc(),data);*/
	inport17_step = data;
}

static WRITE_HANDLER( outport24_w )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "OUT $24 : pc = %04x - data = %02x\n",activecpu_get_pc(),data);*/
	inport23_step = data;
}

WRITE_HANDLER( hvymetal_videomode_w )
{
	int bankaddress;
	unsigned char *rom = memory_region(REGION_CPU1);

	/* patch out the obnoxiously long startup RAM tests */
/*	rom[0x4a55 + memory_region_length(REGION_CPU1) / 2] = 0xc3;*/
/*	rom[0x4a56] = 0xb6;*/
/*	rom[0x4a57] = 0x4a;*/

	bankaddress = 0x10000 + (((data & 0x04)>>2) * 0x4000) + (((data & 0x40)>>5) * 0x4000);
	cpu_setbank(1,&rom[bankaddress]);

	system1_videomode_w(0, data);
}

WRITE_HANDLER( brain_videomode_w )
{
	int bankaddress;
	unsigned char *rom = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + (((data & 0x04)>>2) * 0x4000) + (((data & 0x40)>>5) * 0x4000);
	cpu_setbank(1,&rom[bankaddress]);

	system1_videomode_w(0, data);
}

WRITE_HANDLER( chplft_videomode_w )
{
	int bankaddress;
	unsigned char *rom = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + (((data & 0x0c)>>2) * 0x4000);
	cpu_setbank(1,&rom[bankaddress]);

	system1_videomode_w(0, data);
}


WRITE_HANDLER( system1_soundport_w )
{
	soundlatch_w(0,data);
	cpu_set_irq_line(1,IRQ_LINE_NMI,PULSE_LINE);
	/* spin for a while to let the Z80 read the command (fixes hanging sound in Regulus) */
	cpu_spinuntil_time(TIME_IN_USEC(50));
}



static MEMORY_READ_START( readmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xf3ff, MRA_RAM },
	{ 0xf800, 0xfbff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAMROM },
	{ 0xd000, 0xd1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd800, 0xddff, system1_paletteram_w, &paletteram },
	{ 0xe000, 0xe7ff, system1_backgroundram_w, &system1_backgroundram, &system1_backgroundram_size },
	{ 0xe800, 0xeeff, MWA_RAM, &system1_videoram, &system1_videoram_size },
	{ 0xefbd, 0xefbd, MWA_RAM, &system1_scroll_y },
	{ 0xeffc, 0xeffd, MWA_RAM, &system1_scroll_x },
	{ 0xf000, 0xf3ff, system1_background_collisionram_w, &system1_background_collisionram },
	{ 0xf800, 0xfbff, system1_sprites_collisionram_w, &system1_sprites_collisionram },
MEMORY_END

static MEMORY_WRITE_START( blockgal_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAMROM },
	{ 0xd000, 0xd1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd800, 0xddff, system1_paletteram_w, &paletteram },
	{ 0xe800, 0xeeff, system1_backgroundram_w, &system1_backgroundram, &system1_backgroundram_size },
	{ 0xe000, 0xe6ff, MWA_RAM, &system1_videoram, &system1_videoram_size },
	{ 0xe7bd, 0xe7bd, MWA_RAM, &system1_scroll_y },	/* ???*/
	{ 0xe7c0, 0xe7c1, MWA_RAM, &system1_scroll_x },
	{ 0xf000, 0xf3ff, system1_background_collisionram_w, &system1_background_collisionram },
	{ 0xf800, 0xfbff, system1_sprites_collisionram_w, &system1_sprites_collisionram },
MEMORY_END

static MEMORY_READ_START( brain_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xf3ff, MRA_RAM },
	{ 0xf800, 0xfbff, MRA_RAM },
MEMORY_END

static MEMORY_READ_START( wbml_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xefff, wbml_paged_videoram_r },
	{ 0xf020, 0xf03f, MRA_RAM },
	{ 0xf800, 0xfbff, MRA_RAM },
MEMORY_END


static MEMORY_WRITE_START( wbml_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAMROM },
	{ 0xd000, 0xd1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd800, 0xddff, system1_paletteram_w, &paletteram },
	{ 0xe000, 0xefff, wbml_paged_videoram_w },
	{ 0xf000, 0xf3ff, system1_background_collisionram_w, &system1_background_collisionram },
	{ 0xf800, 0xfbff, system1_sprites_collisionram_w, &system1_sprites_collisionram },
MEMORY_END

static MEMORY_WRITE_START( chplft_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAMROM, &system1_ram },
	{ 0xd000, 0xd1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd800, 0xddff, system1_paletteram_w, &paletteram },
	{ 0xe7c0, 0xe7ff, choplifter_scroll_x_w, &system1_scrollx_ram },
	{ 0xe000, 0xe7ff, MWA_RAM, &system1_videoram, &system1_videoram_size },
	{ 0xe800, 0xeeff, system1_backgroundram_w, &system1_backgroundram, &system1_backgroundram_size },
	{ 0xf000, 0xf3ff, system1_background_collisionram_w, &system1_background_collisionram },
	{ 0xf800, 0xfbff, system1_sprites_collisionram_w, &system1_sprites_collisionram },
MEMORY_END

static MEMORY_READ_START( nobo_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( nobo_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc3ff, system1_background_collisionram_w, &system1_background_collisionram },
	{ 0xc800, 0xcbff, system1_sprites_collisionram_w, &system1_sprites_collisionram },
	{ 0xd000, 0xd1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd200, 0xd7ff, MWA_RAM },
	{ 0xd800, 0xddff, system1_paletteram_w, &paletteram },
	{ 0xde00, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe7ff, system1_backgroundram_w, &system1_backgroundram, &system1_backgroundram_size },
	{ 0xe800, 0xeeff, MWA_RAM, &system1_videoram, &system1_videoram_size },
	{ 0xefbd, 0xefbd, MWA_RAM, &system1_scroll_y },
	{ 0xeffc, 0xeffd, MWA_RAM, &system1_scroll_x },
	{ 0xf000, 0xffff, MWA_RAM },

	/* These addresses are written during P.O.S.T. but don't seem to be used after */
	{ 0xc400, 0xc7ff, MWA_RAM },
	{ 0xcc00, 0xcfff, MWA_RAM },
	{ 0xd200, 0xd7ff, MWA_RAM },
	{ 0xde00, 0xdfff, MWA_RAM },
	{ 0xef00, 0xefbc, MWA_RAM },
	{ 0xefbe, 0xeffb, MWA_RAM },
	{ 0xeffe, 0xefff, MWA_RAM },
MEMORY_END

static PORT_READ_START( readport )
	{ 0x00, 0x00, input_port_0_r }, /* joy1 */
	{ 0x04, 0x04, input_port_1_r }, /* joy2 */
	{ 0x08, 0x08, input_port_2_r }, /* coin,start */
	{ 0x0c, 0x0c, input_port_3_r }, /* DIP2 */
	{ 0x0e, 0x0e, input_port_3_r }, /* DIP2 blckgalb reads it from here */
	{ 0x0d, 0x0d, input_port_4_r }, /* DIP1 some games read it from here... */
	{ 0x10, 0x10, input_port_4_r }, /* DIP1 ... and some others from here */
									/* but there are games which check BOTH! */
	{ 0x15, 0x15, system1_videomode_r },
	{ 0x19, 0x19, system1_videomode_r },    /* mirror address */
PORT_END

static PORT_WRITE_START( writeport )
	{ 0x14, 0x14, system1_soundport_w },    /* sound commands */
	{ 0x15, 0x15, system1_videomode_w },    /* video control and (in some games) bank switching */
	{ 0x18, 0x18, system1_soundport_w },    /* mirror address */
	{ 0x19, 0x19, system1_videomode_w },    /* mirror address */
PORT_END

static PORT_READ_START( wbml_readport )
	{ 0x00, 0x00, input_port_0_r }, /* joy1 */
	{ 0x04, 0x04, input_port_1_r }, /* joy2 */
	{ 0x08, 0x08, input_port_2_r }, /* coin,start */
	{ 0x0c, 0x0c, input_port_3_r }, /* DIP2 */
	{ 0x0d, 0x0d, input_port_4_r }, /* DIP1 some games read it from here... */
	{ 0x10, 0x10, input_port_4_r }, /* DIP1 ... and some others from here */
									/* but there are games which check BOTH! */
	{ 0x15, 0x15, system1_videomode_r },
	{ 0x16, 0x16, wbml_videoram_bank_latch_r },
	{ 0x19, 0x19, system1_videomode_r },  /* mirror address */
PORT_END

static READ_HANDLER( gun_output_r )
{
	return gun_output;
}

static READ_HANDLER( gun_trigger_r )
{
	UINT8 newval = readinputport(5);

	if (newval && BIT(gun_output, 0))
		gun_trigger = 1;

	/* bit 6 = gun trigger latch */
	/* bit 7 = light sensor? */
	return ~(gun_trigger << 6);
}

static PORT_READ_START( sht_readport )
/*	{ 0x00, 0x00, input_port_0_r }, joy1 */
/*	{ 0x04, 0x04, input_port_1_r }, joy2 */
	{ 0x08, 0x08, input_port_2_r }, /* coin,start */
	{ 0x0c, 0x0c, input_port_3_r }, /* DIP2 */
	{ 0x0d, 0x0d, input_port_4_r }, /* DIP1 some games read it from here... */
	{ 0x10, 0x10, gun_output_r }, /* DIP1 ... and some others from here */
									/* but there are games which check BOTH! - gun read here instead */

	{ 0x12, 0x12, gun_trigger_r }, /* trigger is here.. */
	{ 0x1c, 0x1c, input_port_6_r }, /* gunx */
	{ 0x1d, 0x1d, input_port_7_r }, /* guny */
	{ 0x18, 0x18, input_port_8_r }, /* 2 rotary switches */

	{ 0x15, 0x15, system1_videomode_r },
	{ 0x16, 0x16, wbml_videoram_bank_latch_r },
	{ 0x19, 0x19, system1_videomode_r },  /* mirror address */
PORT_END

static PORT_READ_START( nobo_readport )
	{ 0x00, 0x00, input_port_0_r },	/* Player 1 inputs */
	{ 0x04, 0x04, input_port_1_r },	/* Player 2 inputs */
	{ 0x08, 0x08, input_port_2_r },	/* System inputs */
	{ 0x0c, 0x0c, input_port_3_r },	/* DSW1 */
	{ 0x0d, 0x0d, input_port_4_r },	/* DSW0 */
	{ 0x15, 0x15, system1_videomode_r },
	{ 0x16, 0x16, inport16_r },			/* Used - check code at 0x05cb */
	{ 0x1c, 0x1c, inport1c_r },			/* Shouldn't be called ! */
	{ 0x22, 0x22, inport22_r },			/* Used - check code at 0xb253 */
	{ 0x23, 0x23, inport23_r },			/* Used - check code at 0xb275 and 0xb283 */
PORT_END

static PORT_WRITE_START( wbml_writeport )
	{ 0x14, 0x14, system1_soundport_w },    /* sound commands */
	{ 0x15, 0x15, chplft_videomode_w },
	{ 0x16, 0x16, wbml_videoram_bank_latch_w },
PORT_END

/* protection values from real hardware, these were verified to be the same on the title
   screen and in the first level... they're all jumps that the MCU appears to put in RAM
   at some point */
static const int shtngtab[]=
{
	0xC3,0xC1,0x39,
	0xC3,0x6F,0x0A,
	0xC3,0x56,0x39,
	0xC3,0x57,0x0C,
	0xC3,0xE2,0x0B,
	0xC3,0x68,0x03,
	0xC3,0xF1,0x06,
	0xC3,0xCA,0x06,
	0xC3,0xC4,0x06,
	0xC3,0xD6,0x07,
	0xC3,0x89,0x13,
	0xC3,0x75,0x13,
	0xC3,0x9F,0x13,
	0xC3,0xFF,0x38,
	0xC3,0x60,0x13,
	0xC3,0x62,0x00,
	0xC3,0x39,0x04,
	-1
};
static void mcuenable_hack(void)
{
	/* hooked gun feedback */
	int i=0;
	while(shtngtab[i]>=0)
	{
		system1_ram[i+0x40]=shtngtab[i];
		i++;
	}

	system1_ram[0x2ff]=0x49; /* I ? */
	system1_ram[0x3ff]=0x54; /* T ? */
}

static WRITE_HANDLER( gun_output_w )
{
	/* bit 0 readies the gun? */
	if (!BIT(data, 0))
		gun_trigger = 0;

	/* bit 2 = gun solenoid */
	/*gun_solenoid = BIT(data, 2);*/

	gun_output = data;

	mcuenable_hack();
}

static PORT_WRITE_START( sht_writeport )
	{ 0x10, 0x10, gun_output_w },
	{ 0x14, 0x14, system1_soundport_w },    /* sound commands */
	{ 0x15, 0x15, chplft_videomode_w },
	{ 0x16, 0x16, wbml_videoram_bank_latch_w },
PORT_END


static PORT_WRITE_START( hvymetal_writeport )
	{ 0x18, 0x18, system1_soundport_w },    /* sound commands */
	{ 0x19, 0x19, hvymetal_videomode_w },
PORT_END

static PORT_WRITE_START( brain_writeport )
	{ 0x18, 0x18, system1_soundport_w },    /* sound commands */
	{ 0x19, 0x19, brain_videomode_w },
PORT_END

static PORT_WRITE_START( chplft_writeport )
	{ 0x14, 0x14, system1_soundport_w },    /* sound commands */
	{ 0x15, 0x15, chplft_videomode_w },
PORT_END

static PORT_WRITE_START( nobo_writeport )
	{ 0x14, 0x14, system1_soundport_w },	/* sound commands ? */
	{ 0x15, 0x15, brain_videomode_w },	/* video control + bank switching */
	{ 0x16, 0x16, outport16_w },			/* Used - check code at 0x05cb */
	{ 0x17, 0x17, outport17_w },			/* Not handled in emul. of other SS1/2 games */
	{ 0x24, 0x24, outport24_w },			/* Used - check code at 0xb24e and 0xb307 */
PORT_END


static unsigned char *work_ram;

static READ_HANDLER( work_ram_r )
{
	return work_ram[offset];
}

static WRITE_HANDLER( work_ram_w )
{
	work_ram[offset] = data;
}

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, work_ram_r },
	{ 0x8800, 0x8fff, work_ram_r },
	{ 0xe000, 0xe000, soundlatch_r },
	{ 0xffff, 0xffff, soundlatch_r },   /* 4D warriors reads also from here - bug? */
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, work_ram_w, &work_ram },
	{ 0x8800, 0x8fff, work_ram_w },
	{ 0xa000, 0xa003, SN76496_0_w },    /* Choplifter writes to the four addresses */
	{ 0xc000, 0xc003, SN76496_1_w },    /* in sequence */
MEMORY_END


#define IN0_PORT \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define DSW1_PORT \
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(	0x07, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(	0x08, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(	0x09, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(	0x05, "2 Coins/1 Credit 5/3 6/4" ) \
	PORT_DIPSETTING(	0x04, "2 Coins/1 Credit 4/3" ) \
	PORT_DIPSETTING(	0x0f, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(	0x03, "1 Coin/1 Credit 5/6" ) \
	PORT_DIPSETTING(	0x02, "1 Coin/1 Credit 4/5" ) \
	PORT_DIPSETTING(	0x01, "1 Coin/1 Credit 2/3" ) \
	PORT_DIPSETTING(	0x06, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(	0x0e, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(	0x0d, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(	0x0c, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(	0x0b, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(	0x0a, DEF_STR( 1C_6C ) ) \
/*  PORT_DIPSETTING(	0x00, "1/1" ) */ \
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(	0x70, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(	0x80, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(	0x90, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(	0x50, "2 Coins/1 Credit 5/3 6/4" ) \
	PORT_DIPSETTING(	0x40, "2 Coins/1 Credit 4/3" ) \
	PORT_DIPSETTING(	0xf0, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(	0x30, "1 Coin/1 Credit 5/6" ) \
	PORT_DIPSETTING(	0x20, "1 Coin/1 Credit 4/5" ) \
	PORT_DIPSETTING(	0x10, "1 Coin/1 Credit 2/3" ) \
	PORT_DIPSETTING(	0x60, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(	0xe0, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(	0xd0, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(	0xc0, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(	0xb0, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(	0xa0, DEF_STR( 1C_6C ) )
/*  PORT_DIPSETTING(	0x00, "1/1" ) */

/* If you don't like the description, feel free to change it */
#define DSW0_BIT7 \
	PORT_DIPNAME( 0x80, 0x80, "SW 0 Read From" ) \
	PORT_DIPSETTING(	0x80, "Port $0D" ) \
	PORT_DIPSETTING(	0x00, "Port $10" )

INPUT_PORTS_START( starjack )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x38, 0x30, DEF_STR (Bonus_Life ) )
	PORT_DIPSETTING(	0x38, "Every 20k" )
	PORT_DIPSETTING(	0x28, "Every 30k" )
	PORT_DIPSETTING(	0x18, "Every 40k" )
	PORT_DIPSETTING(	0x08, "Every 50k" )
	PORT_DIPSETTING(	0x30, "20k, then every 30k" )
	PORT_DIPSETTING(	0x20, "30k, then every 40k" )
	PORT_DIPSETTING(	0x10, "40k, then every 50k" )
	PORT_DIPSETTING(	0x00, "50k, then every 60k" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0xc0, "Easy" )
	PORT_DIPSETTING(	0x80, "Medium" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0x00, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( starjacs )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x08, 0x08, "Ship" )
	PORT_DIPSETTING(	0x08, "Single" )
	PORT_DIPSETTING(	0x00, "Multi" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "30k, then every 40k" )
	PORT_DIPSETTING(	0x20, "40k, then every 50k" )
	PORT_DIPSETTING(	0x10, "50k, then every 60k" )
	PORT_DIPSETTING(	0x00, "60k, then every 70k" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0xc0, "Easy" )
	PORT_DIPSETTING(	0x80, "Medium" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0x00, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( regulus )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, "Unused SW 0-1" )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x10, "Unused SW 0-4" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unused SW 0-5" )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x80, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

/* Same as 'regulus', but no "Allow Continue" Dip Switch */
INPUT_PORTS_START( reguluso )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, "Unused SW 0-1" )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x10, "Unused SW 0-4" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unused SW 0-5" )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, "Unused SW 0-7" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( upndown )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x38, "10000" )
	PORT_DIPSETTING(	0x30, "20000" )
	PORT_DIPSETTING(	0x28, "30000" )
	PORT_DIPSETTING(	0x20, "40000" )
	PORT_DIPSETTING(	0x18, "50000" )
	PORT_DIPSETTING(	0x10, "60000" )
	PORT_DIPSETTING(	0x08, "70000" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0xc0, "Easy" )
	PORT_DIPSETTING(	0x80, "Medium" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPSETTING(	0x00, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( mrviking )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, "Maximum Credits" )
	PORT_DIPSETTING(	0x02, "9" )
	PORT_DIPSETTING(	0x00, "99" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "10k, 30k then every 30k" )
	PORT_DIPSETTING(	0x20, "20k, 40k then every 30k" )
	PORT_DIPSETTING(	0x10, "30k, then every 30k" )
	PORT_DIPSETTING(	0x00, "40k, then every 30k" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

/* Same as 'mrviking', but no "Maximum Credits" Dip Switch and "Difficulty" Dip Switch is
   handled by bit 7 instead of bit 6 (so bit 6 is unused) */
INPUT_PORTS_START( mrvikngj )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, "Unused SW 0-1" )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "10k, 30k then every 30k" )
	PORT_DIPSETTING(	0x20, "20k, 40k then every 30k" )
	PORT_DIPSETTING(	0x10, "30k, then every 30k" )
	PORT_DIPSETTING(	0x00, "40k, then every 30k" )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x80, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
INPUT_PORTS_END

INPUT_PORTS_START( swat )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x38, "30000" )
	PORT_DIPSETTING(	0x30, "40000" )
	PORT_DIPSETTING(	0x28, "50000" )
	PORT_DIPSETTING(	0x20, "60000" )
	PORT_DIPSETTING(	0x18, "70000" )
	PORT_DIPSETTING(	0x10, "80000" )
	PORT_DIPSETTING(	0x08, "90000" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unused SW 0-7" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( flicky )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, "Unused SW 0-1" )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "30000 80000 160000" )
	PORT_DIPSETTING(	0x20, "30000 100000 200000" )
	PORT_DIPSETTING(	0x10, "40000 120000 240000" )
	PORT_DIPSETTING(	0x00, "40000 140000 280000" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( wmatch )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 ) /* TURN P1 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL ) /* TURN P2 */

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Time" )
	PORT_DIPSETTING(	0x0c, "Normal" )
	PORT_DIPSETTING(	0x08, "Fast" )
	PORT_DIPSETTING(	0x04, "Faster" )
	PORT_DIPSETTING(	0x00, "Fastest" )
	PORT_DIPNAME( 0x10, 0x10, "Unused SW 0-4" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unused SW 0-5" )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( bullfgt )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "30000" )
	PORT_DIPSETTING(	0x20, "50000" )
	PORT_DIPSETTING(	0x10, "70000" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( spatter )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "40k, 120k and 480k" )
	PORT_DIPSETTING(	0x20, "50k and 200k" )
	PORT_DIPSETTING(	0x10, "100k only" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, "Reset Timer/Objects On Life Loss" )
	PORT_DIPSETTING(	0x40, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( pitfall2 )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x10, "20000 50000" )
	PORT_DIPSETTING(	0x00, "30000 70000" )
	PORT_DIPNAME( 0x20, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x20, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Time" )
	PORT_DIPSETTING(	0x00, "2 Minutes" )
	PORT_DIPSETTING(	0x40, "3 Minutes" )
	PORT_DIPNAME( 0x80, 0x80, "Unused SW 0-7" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( pitfallu )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x18, 0x18, "Starting Stage" )
	PORT_DIPSETTING(	0x18, "1" )
	PORT_DIPSETTING(	0x10, "2" )
	PORT_DIPSETTING(	0x08, "3" )
	PORT_DIPSETTING(	0x00, "4" )
	PORT_DIPNAME( 0x20, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x20, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Time" )
	PORT_DIPSETTING(	0x00, "2 Minutes" )
	PORT_DIPSETTING(	0x40, "3 Minutes" )
	PORT_DIPNAME( 0x80, 0x80, "Unused SW 0-7" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( seganinj )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "240", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x10, "20k 70k 120k 170k" )
	PORT_DIPSETTING(	0x00, "50k 100k 150k 200k" )
	PORT_DIPNAME( 0x20, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x20, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( imsorry )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0C, 0x0C, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0C, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "30000" )
	PORT_DIPSETTING(	0x20, "40000" )
	PORT_DIPSETTING(	0x10, "50000" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( teddybb )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "252", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "100k 400k" )
	PORT_DIPSETTING(	0x20, "200k 600k" )
	PORT_DIPSETTING(	0x10, "400k 800k" )
	PORT_DIPSETTING(	0x00, "600k" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( hvymetal )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "50000 100000" )
	PORT_DIPSETTING(	0x20, "60000 120000" )
	PORT_DIPSETTING(	0x10, "70000 150000" )
	PORT_DIPSETTING(	0x00, "100000" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x80, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( myhero )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "30000" )
	PORT_DIPSETTING(	0x20, "50000" )
	PORT_DIPSETTING(	0x10, "70000" )
	PORT_DIPSETTING(	0x00, "90000" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( shtngmst )
	PORT_START  /* IN1 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* IN2 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED ) /* no 2P start */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Shots Per Second" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPSETTING(    0x00, "Infinite" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "100k, 500k" )
	PORT_DIPSETTING(    0x20, "150k, 600k" )
	PORT_DIPSETTING(    0x10, "200k, 700k" )
	PORT_DIPSETTING(    0x00, "300k, 800k" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Medium" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	  /* DSW0 */
	DSW1_PORT

	PORT_START  /* trigger must pulse 1 frame for HS entry to work */
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1, 1 )

	PORT_START /* 1c */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 48, 8, 0x00, 0xff )

	PORT_START /* 1d */
	PORT_ANALOG( 0xff, 0x8f, IPT_LIGHTGUN_Y | IPF_PLAYER1 | IPF_REVERSE, 64, 8, 0x00, 0xff )

	PORT_START /* 2 rotary switches */
	PORT_DIPNAME( 0x0f, 0x0f, "Gun Y Offset" ) /* SW 1 */
	PORT_DIPSETTING(    0x00, "-16" )
	PORT_DIPSETTING(    0x01, "-14" )
	PORT_DIPSETTING(    0x02, "-12" )
	PORT_DIPSETTING(    0x03, "-10" )
	PORT_DIPSETTING(    0x04, "-8" )
	PORT_DIPSETTING(    0x05, "-6" )
	PORT_DIPSETTING(    0x06, "-4" )
	PORT_DIPSETTING(    0x07, "-2" )
	PORT_DIPSETTING(    0x08, "0" )
	PORT_DIPSETTING(    0x09, "+2" )
	PORT_DIPSETTING(    0x0a, "+4" )
	PORT_DIPSETTING(    0x0b, "+6" )
	PORT_DIPSETTING(    0x0c, "+8" )
	PORT_DIPSETTING(    0x0d, "+10" )
	PORT_DIPSETTING(    0x0e, "+12" )
	PORT_DIPSETTING(    0x0f, "+14" )
	PORT_DIPNAME( 0xf0, 0x70, "Gun X Offset" ) /* SW 2 */
	PORT_DIPSETTING(    0x00, "-16" )
	PORT_DIPSETTING(    0x10, "-14" )
	PORT_DIPSETTING(    0x20, "-12" )
	PORT_DIPSETTING(    0x30, "-10" )
	PORT_DIPSETTING(    0x40, "-8" )
	PORT_DIPSETTING(    0x50, "-6" )
	PORT_DIPSETTING(    0x60, "-4" )
	PORT_DIPSETTING(    0x70, "-2" )
	PORT_DIPSETTING(    0x80, "0" )
	PORT_DIPSETTING(    0x90, "+2" )
	PORT_DIPSETTING(    0xa0, "+4" )
	PORT_DIPSETTING(    0xb0, "+6" )
	PORT_DIPSETTING(    0xc0, "+8" )
	PORT_DIPSETTING(    0xd0, "+10" )
	PORT_DIPSETTING(    0xe0, "+12" )
	PORT_DIPSETTING(    0xf0, "+14" )
INPUT_PORTS_END

INPUT_PORTS_START( chplft )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "20000 70000" )
	PORT_DIPSETTING(	0x20, "50000 100000" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x00, "Easy" )
	PORT_DIPSETTING(	0x40, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	  /* DSW0 */
	DSW1_PORT
INPUT_PORTS_END

INPUT_PORTS_START( 4dwarrio )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x38, "30000" )
	PORT_DIPSETTING(	0x30, "40000" )
	PORT_DIPSETTING(	0x28, "50000" )
	PORT_DIPSETTING(	0x20, "60000" )
	PORT_DIPSETTING(	0x18, "70000" )
	PORT_DIPSETTING(	0x10, "80000" )
	PORT_DIPSETTING(	0x08, "90000" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( brain )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( raflesia )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x30, "20k, 70k and 120k" )
	PORT_DIPSETTING(	0x20, "30k, 80k and 150k" )
	PORT_DIPSETTING(	0x10, "50k, 100k and 200k" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	DSW0_BIT7
INPUT_PORTS_END

INPUT_PORTS_START( wboy )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* down - unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* up - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* down - unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* up - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x10, "30k 100k 170k 240k" )
	PORT_DIPSETTING(	0x00, "30k 120k 210k 300k" )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* same as wboy, additional Energy Consumption switch */
INPUT_PORTS_START( wbdeluxe )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Has to be 0 otherwise the game resets */
												/* if you die after level 1. */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* down - unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* up - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* down - unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* up - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START	  /* DSW1 */
	DSW1_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x08, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x10, "30k 100k 170k 240k" )
	PORT_DIPSETTING(	0x00, "30k 120k 210k 300k" )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x40, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x00, "Energy Consumption" )
	PORT_DIPSETTING(	0x00, "Slow" )
	PORT_DIPSETTING(	0x80, "Fast" )
INPUT_PORTS_END

INPUT_PORTS_START( wboyu )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* down - unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* up - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* down - unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* up - unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x06, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	  /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 4C_2C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Mode" )
	PORT_DIPSETTING(	0xc0, "Normal Game" )
	PORT_DIPSETTING(	0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(	0x40, "Test Mode" )
	PORT_DIPSETTING(	0x00, "Endless Game" )
INPUT_PORTS_END

/* Notes about the bootleg version (as this is the only "working" one) :

Coinage is almost the same as the other Sega games.

However, when you set DSW1 to 00, you enter a pseudo "free play" mode :

  - When you insert a coin, or press the "Service" button, you are given 2 credits
    and this number is NEVER incremented nor decremented
  - You are given 3 lives at start and this number is NEVER decremented
    (it can however be incremented depending on the "Bonus Life" Dip Switch)

If only one nibble is set to 0, it will give a standard 1C_1C.


There is an ingame bug with the "Bonus Life" Dip Switch, but I don't know if it's only
a "feature" of the bootleg :

  - Check routine at 0x2366, and you'll notice that 0xc02d (player 1) and 0xc02e (player 2)
    act like a "pointer" to the bonus life table (0x5ab6 or 0x5abb)
  - Once you get enough points, 1 life is added, and the pointer is incremented
  - There is however NO test to the limit of this pointer ! So, once you've got your 5th
    extra life at 150k, the pointed value will be 3 (= extra life at 30k), and as your
    score is over this value, you'll be given another extra life ... and so on ...


Bits 2 and 6 of DSW0 aren't tested in the game (I can't tell about the "test mode")


Useful addresses (to unprotect 'blockgal' ?) :

  - 0xc040 : credits (0x00-0x09)
  - 0xc019 : player 1 lives
  - 0xc021 : player 2 lives
  - 0xc018 : player 1 level (0x01-0x14)
  - 0xc020 : player 2 level (0x01-0x14)
  - 0xc02d : player 1 bonus life "pointer"
  - 0xc02e : player 1 bonus life "pointer"
  - 0xc01a - 0xc01c : player 1 score (BCD coded - LSB first)
  - 0xc022 - 0xc024 : player 1 score (BCD coded - LSB first)

  - 0xc050 : when == 01, "free play" mode
  - 0xc00c : when == 01, you end the level

*/

INPUT_PORTS_START( blockgal )
	PORT_START  /* IN1 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 60, 15, 0, 0)

	PORT_START  /* IN2 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_COCKTAIL, 60, 15, 0, 0)

	PORT_START  /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unused SW 0-2" )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x10, "10k 30k 60k 100k 150k" )
	PORT_DIPSETTING(	0x00, "30k 50k 100k 200k 300k" )
	PORT_DIPNAME( 0x20, 0x00, "Allow Continue" )
	PORT_DIPSETTING(	0x20, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	  /* DSW1 */
	DSW1_PORT
INPUT_PORTS_END

INPUT_PORTS_START( tokisens )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "1" )
	PORT_DIPSETTING(	0x08, "2" )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x04, "4" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	  /* DSW1 */
	DSW1_PORT
INPUT_PORTS_END

INPUT_PORTS_START( wbml )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x04, "3" )
	PORT_DIPSETTING(	0x0c, "4" )
	PORT_DIPSETTING(	0x08, "5" )
/* 0x00 gives 4 lives */
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x10, "30000 100000 200000" )
	PORT_DIPSETTING(	0x00, "50000 150000 250000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x20, "Easy" )
	PORT_DIPSETTING(	0x00, "Hard" )
	PORT_BITX(	0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Test Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	  /* DSW0 */
	DSW1_PORT
INPUT_PORTS_END

INPUT_PORTS_START( noboranb )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )				/* shot*/
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )				/* fly*/
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )		/* shot*/
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )		/* fly*/
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x20, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x20, "Easy" )
	PORT_DIPSETTING(	0x30, "Medium" )
	PORT_DIPSETTING(	0x10, "Hard" )
	PORT_DIPSETTING(	0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x02, "2" )
	PORT_DIPSETTING(	0x03, "3" )
	PORT_DIPSETTING(	0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "99", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x08, "40k, 80k, 120k and 160k" )
	PORT_DIPSETTING(	0x0c, "50k, 100k and 150k" )
	PORT_DIPSETTING(	0x04, "60k, 120k and 180k" )
	PORT_DIPSETTING(	0x00, "None" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unused SW 0-6" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unused SW 0-7" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( gardia )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	
	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	
	PORT_START  /* IN0 */
	IN0_PORT
	
	PORT_START	  /* DSW1 */
	DSW1_PORT
	
	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPSETTING(    0x00, "Infinite" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "5k, 20k and 30k" )
	PORT_DIPSETTING(    0x20, "10k, 25k and 50k" )
	PORT_DIPSETTING(    0x10, "15k, 30k and 60k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) ) /* Manual states "Always On" */
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ufosensi )
	PORT_START  /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )

	PORT_START  /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START  /* IN0 */
	IN0_PORT

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x00, "Easy" )
	PORT_DIPSETTING(	0x01, "Normal" )
	PORT_DIPSETTING(	0x02, "Hard" )
	PORT_DIPSETTING(	0x03, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x0c, "3" )
	PORT_DIPSETTING(	0x04, "4" )
/*	PORT_DIPSETTING(	0x08, "4" ) */
	PORT_DIPSETTING(	0x00, "5" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x20, 0x00, "Allow_Continue" )
	PORT_DIPSETTING(	0x20, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Invulnerability" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	  /* DSW0 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x05, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(	0x04, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(	0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x03, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(	0x02, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(	0x01, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(	0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(	0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(	0x00, "Infinite Lives (both set)" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x70, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x50, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(	0x40, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(	0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x30, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(	0x20, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(	0x10, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(	0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(	0x00, "Infinite Lives (both set)" )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(0,3), RGN_FRAC(1,3), RGN_FRAC(2,3) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	/* sprites use colors 0-511, but are not defined here */
	{ REGION_GFX1, 0, &charlayout, 512, 128 },
	{ -1 } /* end of array */
};



static struct SN76496interface sn76496_interface =
{
	2,      /* 2 chips */
	{ 2000000, 4000000 },   /* 8 MHz / 4 ?*/
	{ 50, 50 }
};



static MACHINE_DRIVER_START( system1 )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", Z80, 4000000)	/* My Hero has 2 OSCs 8 & 20 MHz (Cabbe Info) */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD_TAG("sound", Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,4)		 /* NMIs are caused by the main CPU */

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(system1)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1536)
	MDRV_COLORTABLE_LENGTH(1536)

	MDRV_PALETTE_INIT(system1)
	MDRV_VIDEO_START(system1)
	MDRV_VIDEO_UPDATE(system1)

	/* sound hardware */
	MDRV_SOUND_ADD(SN76496, sn76496_interface)
MACHINE_DRIVER_END

/* driver with reduced visible area for scrolling games */
static MACHINE_DRIVER_START( small )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )

	/* video hardware */
	MDRV_VISIBLE_AREA(0*8+8, 32*8-1-8, 0*8, 28*8-1)

MACHINE_DRIVER_END


static MACHINE_DRIVER_START( pitfall2 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_REPLACE( "main", Z80, 3600000 )/* should be 4 MHz but that value makes the title screen disappear */

MACHINE_DRIVER_END


static MACHINE_DRIVER_START( hvymetal )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(brain_readmem,writemem)
	MDRV_CPU_PORTS(wbml_readport,hvymetal_writeport)

MACHINE_DRIVER_END


static MACHINE_DRIVER_START( chplft )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(brain_readmem,chplft_writemem)
	MDRV_CPU_PORTS(wbml_readport,chplft_writeport)

	/* video hardware */
	MDRV_VIDEO_UPDATE(choplifter)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( shtngmst )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(brain_readmem,chplft_writemem)
	MDRV_CPU_PORTS(sht_readport,sht_writeport)

	MDRV_MACHINE_INIT(wbml) /* banked */

	/* video hardware - same as small - left / right 8 pixels clipped */
	MDRV_VISIBLE_AREA(0*8+8, 32*8-1-8, 0*8, 28*8-1)

	MDRV_VIDEO_UPDATE(shtngmst)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( brain )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(brain_readmem,writemem)
	MDRV_CPU_PORTS(readport,brain_writeport)

MACHINE_DRIVER_END


static MACHINE_DRIVER_START( wbml )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(wbml_readmem,wbml_writemem)
	MDRV_CPU_PORTS(wbml_readport,wbml_writeport)

	MDRV_MACHINE_INIT(wbml)

	/* video hardware */
	MDRV_VIDEO_UPDATE(wbml)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( noboranb )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_REPLACE( "main", Z80, 8000000)    /* ? guess ? */
	MDRV_CPU_MEMORY(nobo_readmem,nobo_writemem)
	MDRV_CPU_PORTS(nobo_readport,nobo_writeport)

	/* video hardware */
	MDRV_VISIBLE_AREA(1*8, 31*8-1, 0*8, 28*8-1)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( blockgal )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( system1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem,blockgal_writemem)

	/* video hardware */
	MDRV_VIDEO_UPDATE(blockgal)
	
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ufosensi )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( wbml )

	/* video hardware */
	MDRV_VISIBLE_AREA(1*8, 31*8-1, 1*8, 27*8-1)

	/* video hardware */
	MDRV_VIDEO_UPDATE(ufosensi)
	
MACHINE_DRIVER_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

/* Since the standard System 1 PROM has part # 5317, Star Jacker, whose first */
/* ROM is #5318, is probably the first or second System 1 game produced */
ROM_START( starjack )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr5320b.129",	0x0000, 0x2000, CRC(7ab72ecd) SHA1(28d3f87851cccc94a86eb0217893de0baf8e62fd) )
	ROM_LOAD( "epr5321a.130",	0x2000, 0x2000, CRC(38b99050) SHA1(79fd23bb7db577d1dbf1b50503085eccdd17b98c) )
	ROM_LOAD( "epr5322a.131",	0x4000, 0x2000, CRC(103a595b) SHA1(6bb8a063279c93341ff472351b79c92795845f74) )
	ROM_LOAD( "epr-5323.132",	0x6000, 0x2000, CRC(46af0d58) SHA1(6cfa288e28e3b415db5ef3cef8e8849259234af9) )
	ROM_LOAD( "epr-5324.133",	0x8000, 0x2000, CRC(1e89efe2) SHA1(d36ef8f176d5e44884d3d0b9af81be6f7fbd0cde) )
	ROM_LOAD( "epr-5325.134",	0xa000, 0x2000, CRC(d6e379a1) SHA1(27362b3e10d9d4235647eadb9cd1404700a8fb49) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5332.3",		0x0000, 0x2000, CRC(7a72ab3d) SHA1(4a6ad09949a438562d7043532d628cefdbb5a2fe) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5331.82",	0x0000, 0x2000, CRC(251d898f) SHA1(353067a75d583d5f53ce2f473b52a35dd912639f) )
	ROM_LOAD( "epr-5330.65",	0x2000, 0x2000, CRC(eb048745) SHA1(a2e90d20a07608f43273e84d1eb22f195b19626c) )
	ROM_LOAD( "epr-5329.81",	0x4000, 0x2000, CRC(3e8bcaed) SHA1(6d19543427b9c4d8d8f5ea0872cdf8cc4fe5018c) )
	ROM_LOAD( "epr-5328.64",	0x6000, 0x2000, CRC(9ed7849f) SHA1(cc30d144ff70539bbc82c848c154e156a33b638c) )
	ROM_LOAD( "epr-5327.80",	0x8000, 0x2000, CRC(79e92cb1) SHA1(03124ce123684b8469cf42be6ed5f0fffa64c480) )
	ROM_LOAD( "epr-5326.63",	0xa000, 0x2000, CRC(ba7e2b47) SHA1(bc7528456fe8dee9aa21212aa996fc347c5d55b4) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5318.86",	0x0000, 0x4000, CRC(6f2e1fd3) SHA1(326d538551245fce67d0fdba85975e27093b7a93) )
	ROM_LOAD( "epr-5319.93",	0x4000, 0x4000, CRC(ebee4999) SHA1(bb331be270dc1da63699533d9f02d73ce28f1bc5) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( starjacs )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "sja1.129",		0x0000, 0x2000, CRC(59a22a1f) SHA1(4827b537f8df04429ed53c2119c67a32efcf04a2) )
	ROM_LOAD( "sja1.130",		0x2000, 0x2000, CRC(7f4597dc) SHA1(7574853fc2e38880f8493cf628100a890f7aa7dc) )
	ROM_LOAD( "sja1.131",		0x4000, 0x2000, CRC(6074c046) SHA1(5d2bd679d6a13a6c3b203662ce5496b801383db9) )
	ROM_LOAD( "sja1.132",		0x6000, 0x2000, CRC(1c48a3fa) SHA1(4a2e7798600bc4a5fd68533083547212d148d347) )
	ROM_LOAD( "sja1.133",		0x8000, 0x2000, CRC(7598bd51) SHA1(0c18b83dc7874aefcd94593ffbe2b50cc0329367) )
	ROM_LOAD( "sja1.134",		0xa000, 0x2000, CRC(f66fa604) SHA1(d7a81920217fcf7a687ba5e2d10abad5c78085d2) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5332.3",		0x0000, 0x2000, CRC(7a72ab3d) SHA1(4a6ad09949a438562d7043532d628cefdbb5a2fe) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5331.82",	0x0000, 0x2000, CRC(251d898f) SHA1(353067a75d583d5f53ce2f473b52a35dd912639f) )
	ROM_LOAD( "sja1.65",		0x2000, 0x2000, CRC(0ab1893c) SHA1(97877f5d8be7a7b80bbf9fe8dae2acd47c411d64) )
	ROM_LOAD( "epr-5329.81",	0x4000, 0x2000, CRC(3e8bcaed) SHA1(6d19543427b9c4d8d8f5ea0872cdf8cc4fe5018c) )
	ROM_LOAD( "sja1.64",		0x6000, 0x2000, CRC(7f628ae6) SHA1(f859a505b543382b42a478c8ae5cd90f3ea2bc2c) )
	ROM_LOAD( "epr-5327.80",	0x8000, 0x2000, CRC(79e92cb1) SHA1(03124ce123684b8469cf42be6ed5f0fffa64c480) )
	ROM_LOAD( "sja1.63",		0xa000, 0x2000, CRC(5bcb253e) SHA1(8c34a8377344940bcfb2495bfda3ffc6794f261b) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	/* SJA1IC86 and SJA1IC93 in the original set were bad, so I'm using the ones */
	/* from the Sega version. However I suspect the real ones should be slightly */
	/* different. */
	ROM_LOAD( "epr-5318.86",	0x0000, 0x4000, BAD_DUMP CRC(6f2e1fd3) SHA1(326d538551245fce67d0fdba85975e27093b7a93)  )
	ROM_LOAD( "epr-5319.93",	0x4000, 0x4000, BAD_DUMP CRC(ebee4999) SHA1(bb331be270dc1da63699533d9f02d73ce28f1bc5)  )

	ROM_REGION( 0x0100, REGION_USER1, 0 )	/* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( regulus )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr5640a.129",	0x0000, 0x2000, CRC(dafb1528) SHA1(9140c5507bd931df3f8ef8d2910bc74f737b1a5a) ) /* encrypted */
	ROM_LOAD( "epr5641a.130",	0x2000, 0x2000, CRC(0fcc850e) SHA1(d2d6b06bf1e2dc404aa5451cc9f1b919fb5be0f5) ) /* encrypted */
	ROM_LOAD( "epr5642a.131",	0x4000, 0x2000, CRC(4feffa17) SHA1(9d9f4227c4e60a5cc53c369e7c9ce59ea8df3553) ) /* encrypted */
	ROM_LOAD( "epr5643a.132",	0x6000, 0x2000, CRC(b8ac7eb4) SHA1(f96bcde021060a8c1c5270b73487e24d1893e8e5) ) /* encrypted */
	ROM_LOAD( "epr-5644.133",	0x8000, 0x2000, CRC(ffd05b7d) SHA1(6fe471548d227d834c012d5d148b1ea1c12dfd00) )
	ROM_LOAD( "epr5645a.134",	0xa000, 0x2000, CRC(6b4bf77c) SHA1(0200efb58b85a6873db44ffa70c3c14dbca958a6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5652.3",		0x0000, 0x2000, CRC(74edcb98) SHA1(bc181c73a6009ca723e715650adb920b77bd311c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5651.82",	0x0000, 0x2000, CRC(f07f3e82) SHA1(f86acf2de639ac89f80cdf627d1d6b5f5e4f1557) )
	ROM_LOAD( "epr-5650.65",	0x2000, 0x2000, CRC(84c1baa2) SHA1(27ba8e2bb820913e58cb029da9c18d35e67728b8) )
	ROM_LOAD( "epr-5649.81",	0x4000, 0x2000, CRC(6774c895) SHA1(28f74bcf1e6bc06db0984dcf86dd527e301b0c01) )
	ROM_LOAD( "epr-5648.64",	0x6000, 0x2000, CRC(0c69e92a) SHA1(1ee18562250468f8f09a3062705422c28c740674) )
	ROM_LOAD( "epr-5647.80",	0x8000, 0x2000, CRC(9330f7b5) SHA1(2c1be04de6ec652ea8a566eb0eb1a9bcb4c90e66) )
	ROM_LOAD( "epr-5646.63",	0xa000, 0x2000, CRC(4dfacbbc) SHA1(e34d1e1aaf3ae7a138e75df5dedebfb4acd79340) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5638.86",	0x0000, 0x4000, CRC(617363dd) SHA1(c8024f541086a8a940e21219fa4522646aeb365a) )
	ROM_LOAD( "epr-5639.93",	0x4000, 0x4000, CRC(a4ec5131) SHA1(033bf46d2625f99544a784fe3fa299cc1b1b48e1) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( reguluso )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-5640.129",	0x0000, 0x2000, CRC(8324d0d4) SHA1(204713938bc85e8b62c161d8ae00d087ecc9089c) ) /* encrypted */
	ROM_LOAD( "epr-5641.130",	0x2000, 0x2000, CRC(0a09f5c7) SHA1(0d45bff29442908b9f4111c89baea0326f0a9ec9) ) /* encrypted */
	ROM_LOAD( "epr-5642.131",	0x4000, 0x2000, CRC(ff27b2f6) SHA1(fe294a53deffe2d46afa444fdae213e9d8763316) ) /* encrypted */
	ROM_LOAD( "epr-5643.132",	0x6000, 0x2000, CRC(0d867df0) SHA1(adccc78072c0772ec20c0178a0be3426759900bf) ) /* encrypted */
	ROM_LOAD( "epr-5644.133",	0x8000, 0x2000, CRC(ffd05b7d) SHA1(6fe471548d227d834c012d5d148b1ea1c12dfd00) )
	ROM_LOAD( "epr-5645.134",	0xa000, 0x2000, CRC(57a2b4b4) SHA1(9de8f5948c7993f1b6d8bf7032f7fc3d9dff5c77) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5652.3",		0x0000, 0x2000, CRC(74edcb98) SHA1(bc181c73a6009ca723e715650adb920b77bd311c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5651.82",	0x0000, 0x2000, CRC(f07f3e82) SHA1(f86acf2de639ac89f80cdf627d1d6b5f5e4f1557) )
	ROM_LOAD( "epr-5650.65",	0x2000, 0x2000, CRC(84c1baa2) SHA1(27ba8e2bb820913e58cb029da9c18d35e67728b8) )
	ROM_LOAD( "epr-5649.81",	0x4000, 0x2000, CRC(6774c895) SHA1(28f74bcf1e6bc06db0984dcf86dd527e301b0c01) )
	ROM_LOAD( "epr-5648.64",	0x6000, 0x2000, CRC(0c69e92a) SHA1(1ee18562250468f8f09a3062705422c28c740674) )
	ROM_LOAD( "epr-5647.80",	0x8000, 0x2000, CRC(9330f7b5) SHA1(2c1be04de6ec652ea8a566eb0eb1a9bcb4c90e66) )
	ROM_LOAD( "epr-5646.63",	0xa000, 0x2000, CRC(4dfacbbc) SHA1(e34d1e1aaf3ae7a138e75df5dedebfb4acd79340) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5638.86",	0x0000, 0x4000, CRC(617363dd) SHA1(c8024f541086a8a940e21219fa4522646aeb365a) )
	ROM_LOAD( "epr-5639.93",	0x4000, 0x4000, CRC(a4ec5131) SHA1(033bf46d2625f99544a784fe3fa299cc1b1b48e1) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( regulusu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr-5950.129",	0x0000, 0x2000, CRC(3b047b67) SHA1(0164cb919a50013f23568f59caff19ff2d0bf11f) )
	ROM_LOAD( "epr-5951.130",	0x2000, 0x2000, CRC(d66453ab) SHA1(9e339c716c646bd02bedbe27096b75f633554e7c) )
	ROM_LOAD( "epr-5952.131",	0x4000, 0x2000, CRC(f3d0158a) SHA1(9b6d8b2e0a0bec45bfbb9f8ccc728e18e909685f) )
	ROM_LOAD( "epr-5953.132",	0x6000, 0x2000, CRC(a9ad4f44) SHA1(1e051595aff34db06186542bcfc3849bc88eb5d4) )
	ROM_LOAD( "epr-5644.133",	0x8000, 0x2000, CRC(ffd05b7d) SHA1(6fe471548d227d834c012d5d148b1ea1c12dfd00) )
	ROM_LOAD( "epr-5955.134",	0xa000, 0x2000, CRC(65ddb2a3) SHA1(4f94eaac900da5ca512289e2339776b1139e03e1) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5652.3",		0x0000, 0x2000, CRC(74edcb98) SHA1(bc181c73a6009ca723e715650adb920b77bd311c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5651.82",	0x0000, 0x2000, CRC(f07f3e82) SHA1(f86acf2de639ac89f80cdf627d1d6b5f5e4f1557) )
	ROM_LOAD( "epr-5650.65",	0x2000, 0x2000, CRC(84c1baa2) SHA1(27ba8e2bb820913e58cb029da9c18d35e67728b8) )
	ROM_LOAD( "epr-5649.81",	0x4000, 0x2000, CRC(6774c895) SHA1(28f74bcf1e6bc06db0984dcf86dd527e301b0c01) )
	ROM_LOAD( "epr-5648.64",	0x6000, 0x2000, CRC(0c69e92a) SHA1(1ee18562250468f8f09a3062705422c28c740674) )
	ROM_LOAD( "epr-5647.80",	0x8000, 0x2000, CRC(9330f7b5) SHA1(2c1be04de6ec652ea8a566eb0eb1a9bcb4c90e66) )
	ROM_LOAD( "epr-5646.63",	0xa000, 0x2000, CRC(4dfacbbc) SHA1(e34d1e1aaf3ae7a138e75df5dedebfb4acd79340) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5638.86",	0x0000, 0x4000, CRC(617363dd) SHA1(c8024f541086a8a940e21219fa4522646aeb365a) )
	ROM_LOAD( "epr-5639.93",	0x4000, 0x4000, CRC(a4ec5131) SHA1(033bf46d2625f99544a784fe3fa299cc1b1b48e1) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( upndown )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr5516a.129",	0x0000, 0x2000, CRC(038c82da) SHA1(b7f403068ed9f97a4b960fb8863615892bb770ed) ) /* encrypted */
	ROM_LOAD( "epr5517a.130",	0x2000, 0x2000, CRC(6930e1de) SHA1(8a5564c76e1fd20c8e5d95e5f538980e13c41744) ) /* encrypted */
	ROM_LOAD( "epr-5518.131",	0x4000, 0x2000, CRC(2a370c99) SHA1(3d1b2f1cf0d5d2d6369a33e5b3b460a3113d6a3e) ) /* encrypted */
	ROM_LOAD( "epr-5519.132",	0x6000, 0x2000, CRC(9d664a58) SHA1(84f2d012dac63e8d0de3935a76f5202539423a74) ) /* encrypted */
	ROM_LOAD( "epr-5520.133",	0x8000, 0x2000, CRC(208dfbdf) SHA1(eff0c91ce6c2c1f6e191bcbf9ae83dd377cbb408) )
	ROM_LOAD( "epr-5521.134",	0xa000, 0x2000, CRC(e7b8d87a) SHA1(3419318bf6d87b902433bfe3b92baf5e5bad7df3) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5535.3",		0x0000, 0x2000, CRC(cf4e4c45) SHA1(d14a204a9966d37f4b9f3ea4c1d371c9d04e750a) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5527.82",	0x0000, 0x2000, CRC(b2d616f1) SHA1(c079136a5d73e1d55ddbad6efb5e7067d0ff412b) )
	ROM_LOAD( "epr-5526.65",	0x2000, 0x2000, CRC(8a8b33c2) SHA1(db796d5c4ab3f749287133eaf05818f89dc2afb7) )
	ROM_LOAD( "epr-5525.81",	0x4000, 0x2000, CRC(e749c5ef) SHA1(2022cbd42ff0177cdd661bb00b1004459b6af83a) )
	ROM_LOAD( "epr-5524.64",	0x6000, 0x2000, CRC(8b886952) SHA1(6a9c909d10ccb03a8af6fa9d8067946d60b91592) )
	ROM_LOAD( "epr-5523.80",	0x8000, 0x2000, CRC(dede35d9) SHA1(6c47fa433e16ccc3fff9347a4fe8f0165d20a3d2) )
	ROM_LOAD( "epr-5522.63",	0xa000, 0x2000, CRC(5e6d9dff) SHA1(4f18274f5dc349b99b3daec517ccf5ccbb932d1c) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5514.86",	0x0000, 0x4000, CRC(fcc0a88b) SHA1(ca7db3df10deb6720096e6c50eddd9b74c47f0a0) )
	ROM_LOAD( "epr-5515.93",	0x4000, 0x4000, CRC(60908838) SHA1(aedff8ce07ab16942037e5aff212652e51c19e71) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( upndownu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr-5679.129",	0x0000, 0x2000, CRC(c4f2f9c2) SHA1(7904ffb46a2c3ef69b9784f343ff37d81bbee11d) )
	ROM_LOAD( "epr-5680.130",	0x2000, 0x2000, CRC(837f021c) SHA1(14cc846f03b71e0922689388a6757955cfd88bd8) )
	ROM_LOAD( "epr-5681.131",	0x4000, 0x2000, CRC(e1c7ff7e) SHA1(440dc8c18183612c32486c617f5d7f38fd804f0e) )
	ROM_LOAD( "epr-5682.132",	0x6000, 0x2000, CRC(4a5edc1e) SHA1(71f06d1c4a580fed07ad32c6d1f2d37d47ed95b1) )
	ROM_LOAD( "epr-5520.133",	0x8000, 0x2000, CRC(208dfbdf) SHA1(eff0c91ce6c2c1f6e191bcbf9ae83dd377cbb408) ) /* epr-5683.133 */
	ROM_LOAD( "epr-5684.133",	0xa000, 0x2000, CRC(32fa95da) SHA1(ebe87d28dde6b8356d40572e9f2cd35ec240075f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5528.3",		0x0000, 0x2000, CRC(00cd44ab) SHA1(7f5385aa0773681329a4759b0fa6f975e3de6755) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5527.82",	0x0000, 0x2000, CRC(b2d616f1) SHA1(c079136a5d73e1d55ddbad6efb5e7067d0ff412b) )
	ROM_LOAD( "epr-5526.65",	0x2000, 0x2000, CRC(8a8b33c2) SHA1(db796d5c4ab3f749287133eaf05818f89dc2afb7) )
	ROM_LOAD( "epr-5525.81",	0x4000, 0x2000, CRC(e749c5ef) SHA1(2022cbd42ff0177cdd661bb00b1004459b6af83a) )
	ROM_LOAD( "epr-5524.64",	0x6000, 0x2000, CRC(8b886952) SHA1(6a9c909d10ccb03a8af6fa9d8067946d60b91592) )
	ROM_LOAD( "epr-5523.80",	0x8000, 0x2000, CRC(dede35d9) SHA1(6c47fa433e16ccc3fff9347a4fe8f0165d20a3d2) )
	ROM_LOAD( "epr-5522.63",	0xa000, 0x2000, CRC(5e6d9dff) SHA1(4f18274f5dc349b99b3daec517ccf5ccbb932d1c) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5514.86",	0x0000, 0x4000, CRC(fcc0a88b) SHA1(ca7db3df10deb6720096e6c50eddd9b74c47f0a0) )
	ROM_LOAD( "epr-5515.93",	0x4000, 0x4000, CRC(60908838) SHA1(aedff8ce07ab16942037e5aff212652e51c19e71) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( mrviking )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-5873.129",	0x0000, 0x2000, CRC(14d21624) SHA1(70e185d03e782be908e6b5c6342cf6a7ebae618c) ) /* encrypted */
	ROM_LOAD( "epr-5874.130",	0x2000, 0x2000, CRC(6df7de87) SHA1(c2200e0c2f322a08af10e9c2e9191d1c595801a4) ) /* encrypted */
	ROM_LOAD( "epr-5875.131",	0x4000, 0x2000, CRC(ac226100) SHA1(11568db9fbca44013eeb0035c0a0a67d6dd18d00) ) /* encrypted */
	ROM_LOAD( "epr-5876.132",	0x6000, 0x2000, CRC(e77db1dc) SHA1(7b1aa19a16fb44f6c69cf053e2e10e5179416796) ) /* encrypted */
	ROM_LOAD( "epr-5755.133",	0x8000, 0x2000, CRC(edd62ae1) SHA1(9648f1ae3033c30ed8ab8d9c87b111756dab7b5e) )
	ROM_LOAD( "epr-5756.134",	0xa000, 0x2000, CRC(11974040) SHA1(a0904d19d06fb5ef5eb6da0dc4efe556bc29b33e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5763.3",		0x0000, 0x2000, CRC(d712280d) SHA1(8393dfb57d9af22b3280ecaef736b6f9d856dbee) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5762.82",	0x0000, 0x2000, CRC(4a91d08a) SHA1(4687ecc4061719fca5f85b2b290ebb7ced15ee5b) )
	ROM_LOAD( "epr-5761.65",	0x2000, 0x2000, CRC(f7d61b65) SHA1(a7a992f52406413e931945be60b35175f8aea6c2) )
	ROM_LOAD( "epr-5760.81",	0x4000, 0x2000, CRC(95045820) SHA1(d1848fc4f3d66603d0e8217373a37148aa2eeef5) )
	ROM_LOAD( "epr-5759.64",	0x6000, 0x2000, CRC(5f9bae4e) SHA1(6fff6086a96be6aa28bec05d1c94c257bb29ef1e) )
	ROM_LOAD( "epr-5758.80",	0x8000, 0x2000, CRC(808ee706) SHA1(d38ca7c6f36db6e35a3ce87bacdd70f293f23104) )
	ROM_LOAD( "epr-5757.63",	0xa000, 0x2000, CRC(480f7074) SHA1(c54a1fa02e312676658d7c5392a5a841bdb15d44) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5749.86",	0x0000, 0x4000, CRC(e24682cd) SHA1(3f626f3e5e2db486ccf727e9869ab488643b4a8c) )
	ROM_LOAD( "epr-5750.93",	0x4000, 0x4000, CRC(6564d1ad) SHA1(f246afee7e73bc30054b0e5dcb83fa0edd2d2164) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( mrvikngj )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-5751.129",	0x0000, 0x2000, CRC(ae97a4c5) SHA1(12edd757bd5b00d42ada1e10c43817f71cfe77dc) ) /* encrypted */
	ROM_LOAD( "epr-5752.130",	0x2000, 0x2000, CRC(d48e6726) SHA1(934b5e7568c85005c5ec40d75e49727a18562d50) ) /* encrypted */
	ROM_LOAD( "epr-5753.131",	0x4000, 0x2000, CRC(28c60887) SHA1(9673335586221336c3373f5d7c8ae4fc11cc4b7f) ) /* encrypted */
	ROM_LOAD( "epr-5754.132",	0x6000, 0x2000, CRC(1f47ed02) SHA1(d1147cd29fb342111f4f20a1d1d03263dce478f3) ) /* encrypted */
	ROM_LOAD( "epr-5755.133",	0x8000, 0x2000, CRC(edd62ae1) SHA1(9648f1ae3033c30ed8ab8d9c87b111756dab7b5e) )
	ROM_LOAD( "epr-5756.134",	0xa000, 0x2000, CRC(11974040) SHA1(a0904d19d06fb5ef5eb6da0dc4efe556bc29b33e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5763.3",		0x0000, 0x2000, CRC(d712280d) SHA1(8393dfb57d9af22b3280ecaef736b6f9d856dbee) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5762.82",	0x0000, 0x2000, CRC(4a91d08a) SHA1(4687ecc4061719fca5f85b2b290ebb7ced15ee5b) )
	ROM_LOAD( "epr-5761.65",	0x2000, 0x2000, CRC(f7d61b65) SHA1(a7a992f52406413e931945be60b35175f8aea6c2) )
	ROM_LOAD( "epr-5760.81",	0x4000, 0x2000, CRC(95045820) SHA1(d1848fc4f3d66603d0e8217373a37148aa2eeef5) )
	ROM_LOAD( "epr-5759.64",	0x6000, 0x2000, CRC(5f9bae4e) SHA1(6fff6086a96be6aa28bec05d1c94c257bb29ef1e) )
	ROM_LOAD( "epr-5758.80",	0x8000, 0x2000, CRC(808ee706) SHA1(d38ca7c6f36db6e35a3ce87bacdd70f293f23104) )
	ROM_LOAD( "epr-5757.63",	0xa000, 0x2000, CRC(480f7074) SHA1(c54a1fa02e312676658d7c5392a5a841bdb15d44) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5749.86",	0x0000, 0x4000, CRC(e24682cd) SHA1(3f626f3e5e2db486ccf727e9869ab488643b4a8c) )
	ROM_LOAD( "epr-5750.93",	0x4000, 0x4000, CRC(6564d1ad) SHA1(f246afee7e73bc30054b0e5dcb83fa0edd2d2164) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( swat )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr5807b.129",	0x0000, 0x2000, CRC(93db9c9f) SHA1(56e9d9a33f04b4d5971c0db24cc8719a52e64678) ) /* encrypted */
	ROM_LOAD( "epr-5808.130",	0x2000, 0x2000, CRC(67116665) SHA1(e8aa72f2835d38367be5e8a9313e51b64f452ee7) ) /* encrypted */
	ROM_LOAD( "epr-5809.131",	0x4000, 0x2000, CRC(fd792fc9) SHA1(a0b4f0c2e537bd16f7345590da00f2622947d7e4) ) /* encrypted */
	ROM_LOAD( "epr-5810.132",	0x6000, 0x2000, CRC(dc2b279d) SHA1(e740cbe239d379705fdffb3e500d6f5a2fece2e2) ) /* encrypted */
	ROM_LOAD( "epr-5811.133",	0x8000, 0x2000, CRC(093e3ab1) SHA1(abf1f23dc26a7518357d0c1749e869b539c3bbed) )
	ROM_LOAD( "epr-5812.134",	0xa000, 0x2000, CRC(5bfd692f) SHA1(adc8dcf643d8d0b0a1d0dda0494567263ea11a00) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5819.3",		0x0000, 0x2000, CRC(f6afd0fd) SHA1(06062648b9ebc70b4b5c30b043f537adc0052047) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5818.82",	0x0000, 0x2000, CRC(b22033d9) SHA1(ad217cd8dad178f3f2f1fd44a58adcc4887fb6b7) )
	ROM_LOAD( "epr-5817.65",	0x2000, 0x2000, CRC(fd942797) SHA1(da7378e8d12cc2970df2efa075c944c79b3b74d2) )
	ROM_LOAD( "epr-5816.81",	0x4000, 0x2000, CRC(4384376d) SHA1(78ae13a38d6368e44ba95642cce7f5515a5b6022) )
	ROM_LOAD( "epr-5815.64",	0x6000, 0x2000, CRC(16ad046c) SHA1(a0b97e016e5cf43f223ecb6c5fe7dec7c8e9c098) )
	ROM_LOAD( "epr-5814.80",	0x8000, 0x2000, CRC(be721c99) SHA1(bbb0afe2b195d60418014c36acf3de95adfd90d8) )
	ROM_LOAD( "epr-5813.63",	0xa000, 0x2000, CRC(0d42c27e) SHA1(06b1d23cacfef3017e5951dc10e8471e9b3103d5) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5805.86",	0x0000, 0x4000, CRC(5a732865) SHA1(55c54e54f052187ddd957131e56400c9c432a6b2) )
	ROM_LOAD( "epr-5806.93",	0x4000, 0x4000, CRC(26ac258c) SHA1(e4e9f929ab8ae7da74f885481cf94335d7553a1c) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( flicky )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr5978a.116",	0x0000, 0x4000, CRC(296f1492) SHA1(52e2c63ce376ab8124b2c68bdfa432b6621cfa78) ) /* encrypted */
	ROM_LOAD( "epr5979a.109",	0x4000, 0x4000, CRC(64b03ef9) SHA1(7519aa7f036bce6d52a5d4be2418139559f9a8a5) ) /* encrypted */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5869.120",	0x0000, 0x2000, CRC(6d220d4e) SHA1(fe02a7a94a1ad046fc775a7f67f460c8d0f6dca6) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5868.62",	0x0000, 0x2000, CRC(7402256b) SHA1(5bd660ac24a2d0d8ad983e948674a82a2d2e8b49) )
	ROM_LOAD( "epr-5867.61",	0x2000, 0x2000, CRC(2f5ce930) SHA1(4bc3bc6eb8f03926d3710c9f96fcc1b116e918d3) )
	ROM_LOAD( "epr-5866.64",	0x4000, 0x2000, CRC(967f1d9a) SHA1(652be7848526c6e61db4a502f75d1689d2ff2f59) )
	ROM_LOAD( "epr-5865.63",	0x6000, 0x2000, CRC(03d9a34c) SHA1(e158db3e0b86f2b8ad34cefc2714cb0a942efde7) )
	ROM_LOAD( "epr-5864.66",	0x8000, 0x2000, CRC(e659f358) SHA1(cf59f1fb0f9fb77d5ac36be52b6ee946ee85d6de) )
	ROM_LOAD( "epr-5863.65",	0xa000, 0x2000, CRC(a496ca15) SHA1(8c629a853486bbe049b1deecdc00f9e16b87698f) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5855.117",	0x0000, 0x4000, CRC(b5f894a1) SHA1(2c72dc16739dad155fcd572e1add067a7647f5bd) )
	ROM_LOAD( "epr-5856.110",	0x4000, 0x4000, CRC(266af78f) SHA1(dcbfce550d10a1f2b3ce3e7e081fc008cb575708) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( flickyo )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-5857.bin",	0x0000, 0x2000, CRC(a65ac88e) SHA1(1d1c276f7ffb33bc9f216b6b69517f1783d435a4) ) /* encrypted */
	ROM_LOAD( "epr5858a.bin",	0x2000, 0x2000, CRC(18b412f4) SHA1(6205dc2a6c1092f9bc7752672b7c06d5faf2f65e) ) /* encrypted */
	ROM_LOAD( "epr-5859.bin",	0x4000, 0x2000, CRC(a5558d7e) SHA1(ca59c7e57ae45f960f769db9a04ffa5c870005dd) ) /* encrypted */
	ROM_LOAD( "epr-5860.bin",	0x6000, 0x2000, CRC(1b35fef1) SHA1(53ca5361309c59a2b3490ea0037c6e58f07837d9) ) /* encrypted */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-5869.120",	0x0000, 0x2000, CRC(6d220d4e) SHA1(fe02a7a94a1ad046fc775a7f67f460c8d0f6dca6) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-5868.62",	0x0000, 0x2000, CRC(7402256b) SHA1(5bd660ac24a2d0d8ad983e948674a82a2d2e8b49) )
	ROM_LOAD( "epr-5867.61",	0x2000, 0x2000, CRC(2f5ce930) SHA1(4bc3bc6eb8f03926d3710c9f96fcc1b116e918d3) )
	ROM_LOAD( "epr-5866.64",	0x4000, 0x2000, CRC(967f1d9a) SHA1(652be7848526c6e61db4a502f75d1689d2ff2f59) )
	ROM_LOAD( "epr-5865.63",	0x6000, 0x2000, CRC(03d9a34c) SHA1(e158db3e0b86f2b8ad34cefc2714cb0a942efde7) )
	ROM_LOAD( "epr-5864.66",	0x8000, 0x2000, CRC(e659f358) SHA1(cf59f1fb0f9fb77d5ac36be52b6ee946ee85d6de) )
	ROM_LOAD( "epr-5863.65",	0xa000, 0x2000, CRC(a496ca15) SHA1(8c629a853486bbe049b1deecdc00f9e16b87698f) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-5855.117",	0x0000, 0x4000, CRC(b5f894a1) SHA1(2c72dc16739dad155fcd572e1add067a7647f5bd) )
	ROM_LOAD( "epr-5856.110",	0x4000, 0x4000, CRC(266af78f) SHA1(dcbfce550d10a1f2b3ce3e7e081fc008cb575708) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wmatch )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "wm.129",			0x0000, 0x2000, CRC(b6db4442) SHA1(9f31b3b2d4b4a430f9de84141ebd66bdba063387) ) /* encrypted */
	ROM_LOAD( "wm.130",			0x2000, 0x2000, CRC(59a0a7a0) SHA1(a1707d08ba968d1ad01f3249c046a62dde8e2730) ) /* encrypted */
	ROM_LOAD( "wm.131",			0x4000, 0x2000, CRC(4cb3856a) SHA1(983f52bfb2f8e3871518137f424786a9a8e5c53d) ) /* encrypted */
	ROM_LOAD( "wm.132",			0x6000, 0x2000, CRC(e2e44b29) SHA1(53208666c1368887ab347ea1f261e692cc041d40) ) /* encrypted */
	ROM_LOAD( "wm.133",			0x8000, 0x2000, CRC(43a36445) SHA1(6cc5a6fa8319d4e2b454b326d8a908ff764fa65f) )
	ROM_LOAD( "wm.134",			0xa000, 0x2000, CRC(5624794c) SHA1(7cfb0a35b7fb8394e0e7efa6b63ba83bd5c9b8e7) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "wm.3",			0x0000, 0x2000, CRC(50d2afb7) SHA1(21b109d389d0b52d89cf635467c3213f6b24d7df) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "wm.82",			0x0000, 0x2000, CRC(540f0bf3) SHA1(3898dee3ed9e7382a9dfc3ee2af177c5b832ea84) )
	ROM_LOAD( "wm.65",			0x2000, 0x2000, CRC(92c1e39e) SHA1(a701a66ed75fbc0be4819751dabb86e51a1dbbc4) )
	ROM_LOAD( "wm.81",			0x4000, 0x2000, CRC(6a01ff2a) SHA1(f609fe9ec648dd428a6e2fc544585935d7adc562) )
	ROM_LOAD( "wm.64",			0x6000, 0x2000, CRC(aae6449b) SHA1(852d6c01420ea55e4215ec99adbb6896fa16a02d) )
	ROM_LOAD( "wm.80",			0x8000, 0x2000, CRC(fc3f0bd4) SHA1(887ff0d6c5fff0d1e631518fc89901d43a0d7088) )
	ROM_LOAD( "wm.63",			0xa000, 0x2000, CRC(c2ce9b93) SHA1(934f4dddf2f42a23f91385dd62fc166b117063b8) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "wm.86",			0x0000, 0x4000, CRC(238ae0e5) SHA1(af18cfe7f8103358a0ce2aef9bbd949fdc0bfbfc) )
	ROM_LOAD( "wm.93",			0x4000, 0x4000, CRC(a2f19170) SHA1(47dacc380b09c6365c737d320145cedad54ecedb) )

	ROM_REGION( 0x0100, REGION_USER1, 0 )	/* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.106",	0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( bullfgt )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-.129",		0x0000, 0x2000, CRC(29f19156) SHA1(86cca9601f63b9b3d3aaaf21c3a3e456a50ca6b8) ) /* encrypted */
	ROM_LOAD( "epr-.130",		0x2000, 0x2000, CRC(e37d2b95) SHA1(9d2523190e49c9d45a5832da912cbc0cd23e2496) ) /* encrypted */
	ROM_LOAD( "epr-.131",		0x4000, 0x2000, CRC(eaf5773d) SHA1(7db6a7c1c4d9e5f5b4de97b41ab5dd591e2e1548) ) /* encrypted */
	ROM_LOAD( "epr-.132",		0x6000, 0x2000, CRC(72c3c712) SHA1(1c1ac6d7248382228b99d2652f53fbe15246f253) ) /* encrypted */
	ROM_LOAD( "epr-.133",		0x8000, 0x2000, CRC(7d9fa4cd) SHA1(b6f0d86281c7e8de7a23b0c55c1991350d5bc9b1) )
	ROM_LOAD( "epr-.134",		0xa000, 0x2000, CRC(061f2797) SHA1(f13acd4c5b33ed85229a3907744283646e020867) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6077.120",	0x0000, 0x2000, CRC(02a37602) SHA1(1b67b0d80a228f7faf054bfd79aff120d92c8166) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-.82",		0x0000, 0x2000, CRC(b71c349f) SHA1(5a0e9b90c71708dadab201da09c71449e05268e1) )
	ROM_LOAD( "epr-.65",		0x2000, 0x2000, CRC(86deafa8) SHA1(b4b9d38bd4a47ce2e75ec0ef3d7507aef8a16858) )
	ROM_LOAD( "epr-6087.64",	0x4000, 0x2000, CRC(2677742c) SHA1(6a6154f1c2cc53b9d224fc73bab47e6deb7c505f) ) /* epr-6087.81 */
	ROM_LOAD( "epr-.64",		0x6000, 0x2000, CRC(6f0a62be) SHA1(30c93c4d7f916f7b9a725f412a3a4a71f24c4f22) )
	ROM_LOAD( "epr-6085.66",	0x8000, 0x2000, CRC(9c3ddc62) SHA1(3332824de114836760a40133fb65d8f40474bc81) ) /* epr-6085.80 */
	ROM_LOAD( "epr-.63",		0xa000, 0x2000, CRC(c0fce57c) SHA1(74f2c987f77e73b7069014d3bd6809d8bb3596c7) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-6069.117",	0x0000, 0x4000, CRC(fe691e41) SHA1(90faf26685202e2a25bb3024750456014d0722b3) ) /* epr-6069.86 */
	ROM_LOAD( "epr-6070.110",	0x4000, 0x4000, CRC(34f080df) SHA1(0e7d28e3325c8c3f06438fde29ea0ffe57fc325f) ) /* epr-6070.93 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( thetogyu )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6071.116",	0x0000, 0x4000, CRC(96b57df9) SHA1(bfce24bf570961d3cfb449078e23e546fad7229e) ) /* encrypted */
	ROM_LOAD( "epr-6072.109",	0x4000, 0x4000, CRC(f7baadd0) SHA1(45a05b72561d47e4ac5475509fe2b57d870c89cd) ) /* encrypted */
	ROM_LOAD( "epr-6073.96",	0x8000, 0x4000, CRC(721af166) SHA1(0b345715227e70fa6857f5967f0c7da9577f8887) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6077.120",	0x0000, 0x2000, CRC(02a37602) SHA1(1b67b0d80a228f7faf054bfd79aff120d92c8166) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6089.62",	0x0000, 0x2000, CRC(a183e5ff) SHA1(bb710377a8e88f530b669141ab46abd867c6cb83) )
	ROM_LOAD( "epr-6088.61",	0x2000, 0x2000, CRC(b919b4a6) SHA1(ca11a96bee2e2059552ac6cce6f8dead1965ef4b) )
	ROM_LOAD( "epr-6087.64",	0x4000, 0x2000, CRC(2677742c) SHA1(6a6154f1c2cc53b9d224fc73bab47e6deb7c505f) )
	ROM_LOAD( "epr-6086.63",	0x6000, 0x2000, CRC(76b5a084) SHA1(32fd23f0d6fc8f5c3b5aae9a20907191a6d70611) )
	ROM_LOAD( "epr-6085.66",	0x8000, 0x2000, CRC(9c3ddc62) SHA1(3332824de114836760a40133fb65d8f40474bc81) )
	ROM_LOAD( "epr-6084.65",	0xa000, 0x2000, CRC(90e1fa5f) SHA1(e37a7f872229a93a70e42615e6452aa608d53a93) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-6069.117",	0x0000, 0x4000, CRC(fe691e41) SHA1(90faf26685202e2a25bb3024750456014d0722b3) )
	ROM_LOAD( "epr-6070.110",	0x4000, 0x4000, CRC(34f080df) SHA1(0e7d28e3325c8c3f06438fde29ea0ffe57fc325f) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( pitfall2 )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr6456a.116",	0x0000, 0x4000, CRC(bcc8406b) SHA1(2e5c76886fce2c9863db7a914b85b088971aceef) ) /* encrypted */
	ROM_LOAD( "epr6457a.109",	0x4000, 0x4000, CRC(a016fd2a) SHA1(866f82066466bc5eaf6ab1b6f85a1c173692a1f7) ) /* encrypted */
	ROM_LOAD( "epr6458a.96",	0x8000, 0x4000, CRC(5c30b3e8) SHA1(9048091ebf054d0ba0c6a92520ddfac38a479034) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6462.120",	0x0000, 0x2000, CRC(86bb9185) SHA1(89add2e3784e8f5a20b895fb2c4466bdd6c34b0c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr6474a.62",	0x0000, 0x2000, CRC(9f1711b9) SHA1(c652010a8b19828f81fd101aa1ea781e250c4ec2) )
	ROM_LOAD( "epr6473a.61",	0x2000, 0x2000, CRC(8e53b8dd) SHA1(23e04589f2b523d6b8e46d16f40e59685e27f522) )
	ROM_LOAD( "epr6472a.64",	0x4000, 0x2000, CRC(e0f34a11) SHA1(b7a96a1867f8bd3cc1251b5fd12991c406e62a37) )
	ROM_LOAD( "epr6471a.63",	0x6000, 0x2000, CRC(d5bc805c) SHA1(520afa7617e8dfd09bf469c01ac606a4a3acdc5e) )
	ROM_LOAD( "epr6470a.66",	0x8000, 0x2000, CRC(1439729f) SHA1(54ea6ef54be6dcc2a5d00f7f817fd8836a02b3b9) )
	ROM_LOAD( "epr6469a.65",	0xa000, 0x2000, CRC(e4ac6921) SHA1(f95e3b368c2c6dbf8265fb314d73019fe7dcce22) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr6454a.117",	0x0000, 0x4000, CRC(a5d96780) SHA1(e0571f6fd031bbe2d971c3be7b96a017b0ea4be9) )
	ROM_LOAD( "epr-6455.05",	0x4000, 0x4000, CRC(32ee64a1) SHA1(21743f78735fc9105fbbfac420bdaa2965b4b56f) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( pitfallu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr-6623.116",	0x0000, 0x4000, CRC(bcb47ed6) SHA1(d33421999f899c0a4dc0d4553614c1f5c7027257) )
	ROM_LOAD( "epr6624a.109",	0x4000, 0x4000, CRC(6e8b09c1) SHA1(4869ca4d3f0b08cd3df4c82be9cfc774ddeb3010) )
	ROM_LOAD( "epr-6625.96",	0x8000, 0x4000, CRC(dc5484ba) SHA1(62fffff7d935c104def5f09e9dc4a26fa4ce4f94) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6462.120",	0x0000, 0x2000, CRC(86bb9185) SHA1(89add2e3784e8f5a20b895fb2c4466bdd6c34b0c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr6474a.62",	0x0000, 0x2000, CRC(9f1711b9) SHA1(c652010a8b19828f81fd101aa1ea781e250c4ec2) )
	ROM_LOAD( "epr6473a.61",	0x2000, 0x2000, CRC(8e53b8dd) SHA1(23e04589f2b523d6b8e46d16f40e59685e27f522) )
	ROM_LOAD( "epr6472a.64",	0x4000, 0x2000, CRC(e0f34a11) SHA1(b7a96a1867f8bd3cc1251b5fd12991c406e62a37) )
	ROM_LOAD( "epr6471a.63",	0x6000, 0x2000, CRC(d5bc805c) SHA1(520afa7617e8dfd09bf469c01ac606a4a3acdc5e) )
	ROM_LOAD( "epr6470a.66",	0x8000, 0x2000, CRC(1439729f) SHA1(54ea6ef54be6dcc2a5d00f7f817fd8836a02b3b9) )
	ROM_LOAD( "epr6469a.65",	0xa000, 0x2000, CRC(e4ac6921) SHA1(f95e3b368c2c6dbf8265fb314d73019fe7dcce22) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr6454a.117",	0x0000, 0x4000, CRC(a5d96780) SHA1(e0571f6fd031bbe2d971c3be7b96a017b0ea4be9) )
	ROM_LOAD( "epr-6455.05",	0x4000, 0x4000, CRC(32ee64a1) SHA1(21743f78735fc9105fbbfac420bdaa2965b4b56f) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( seganinj )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-.116",		0x0000, 0x4000, CRC(a5d0c9d0) SHA1(b60caccab8269f40d4f6e7a50f3aa0d4901c1e57) ) /* encrypted */
	ROM_LOAD( "epr-.109",		0x4000, 0x4000, CRC(b9e6775c) SHA1(f39e815c3c034015125b96de34a2a225b81392b5) ) /* encrypted */
	ROM_LOAD( "epr-6552.96",	0x8000, 0x4000, CRC(f2eeb0d8) SHA1(1f0d1c73ba9eaa2887ffc596f0038b0af37ced49) ) /* epr-7151.96 */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6559.120",	0x0000, 0x2000, CRC(5a1570ee) SHA1(fd9215e007b6687d057ea7aee01f6d3dcbc8f894) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6558.62",	0x0000, 0x2000, CRC(2af9eaeb) SHA1(a8a472e9f156c34f1cfcf6d6be808da4303a2276) )
	ROM_LOAD( "epr-6592.61",	0x2000, 0x2000, CRC(7804db86) SHA1(8229781b8296d3ffdfa2f0901e2eed297cc3e160) )
	ROM_LOAD( "epr-6556.64",	0x4000, 0x2000, CRC(79fd26f7) SHA1(a7de0f21ccbcfda495a5c93237569a9b3919d2d5) )
	ROM_LOAD( "epr-6590.63",	0x6000, 0x2000, CRC(bf858cad) SHA1(1c18c4aa4b9a59f3c06aa459eab6bdd1b298d848) )
	ROM_LOAD( "epr-6554.66",	0x8000, 0x2000, CRC(5ac9d205) SHA1(c3094d10d1d6226bf9ad174d2dd1631b8d6ca33a) )
	ROM_LOAD( "epr-6588.65",	0xa000, 0x2000, CRC(dc931dbb) SHA1(4729b27843f226ba5861c3106f8418db70e7c47d) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6546.117",	0x0000, 0x4000, CRC(a4785692) SHA1(95ce23076dc86c5d6d3a65274873d4c48e91cc06) )
	ROM_LOAD( "epr-6548.04",	0x4000, 0x4000, CRC(bdf278c1) SHA1(7ebe505f4f0434edb2cee17a6cbce6b900b29cc4) )
	ROM_LOAD( "epr-6547.110",	0x8000, 0x4000, CRC(34451b08) SHA1(ee8708f6c886b63f138bcc10dc2a053bfad96c37) )
	ROM_LOAD( "epr-6549.05",	0xc000, 0x4000, CRC(d2057668) SHA1(ded2a04f7555eb3b1e4da57901ca00635de2c043) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( seganinu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr-7149.116",	0x0000, 0x4000, CRC(cd9fade7) SHA1(958ef5c449df6ef5346b8634cb34a646950f706e) )
	ROM_LOAD( "epr-7150.109",	0x4000, 0x4000, CRC(c36351e2) SHA1(17734d3f410feb4cad617d1931b3356192b69ac0) )
	ROM_LOAD( "epr-6552.96",	0x8000, 0x4000, CRC(f2eeb0d8) SHA1(1f0d1c73ba9eaa2887ffc596f0038b0af37ced49) ) /* epr-7151.96 */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6559.120",	0x0000, 0x2000, CRC(5a1570ee) SHA1(fd9215e007b6687d057ea7aee01f6d3dcbc8f894) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6558.62",	0x0000, 0x2000, CRC(2af9eaeb) SHA1(a8a472e9f156c34f1cfcf6d6be808da4303a2276) )
	ROM_LOAD( "epr-6592.61",	0x2000, 0x2000, CRC(7804db86) SHA1(8229781b8296d3ffdfa2f0901e2eed297cc3e160) )
	ROM_LOAD( "epr-6556.64",	0x4000, 0x2000, CRC(79fd26f7) SHA1(a7de0f21ccbcfda495a5c93237569a9b3919d2d5) )
	ROM_LOAD( "epr-6590.63",	0x6000, 0x2000, CRC(bf858cad) SHA1(1c18c4aa4b9a59f3c06aa459eab6bdd1b298d848) )
	ROM_LOAD( "epr-6554.66",	0x8000, 0x2000, CRC(5ac9d205) SHA1(c3094d10d1d6226bf9ad174d2dd1631b8d6ca33a) )
	ROM_LOAD( "epr-6588.65",	0xa000, 0x2000, CRC(dc931dbb) SHA1(4729b27843f226ba5861c3106f8418db70e7c47d) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6546.117",	0x0000, 0x4000, CRC(a4785692) SHA1(95ce23076dc86c5d6d3a65274873d4c48e91cc06) )
	ROM_LOAD( "epr-6548.04",	0x4000, 0x4000, CRC(bdf278c1) SHA1(7ebe505f4f0434edb2cee17a6cbce6b900b29cc4) )
	ROM_LOAD( "epr-6547.110",	0x8000, 0x4000, CRC(34451b08) SHA1(ee8708f6c886b63f138bcc10dc2a053bfad96c37) )
	ROM_LOAD( "epr-6549.05",	0xc000, 0x4000, CRC(d2057668) SHA1(ded2a04f7555eb3b1e4da57901ca00635de2c043) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( nprinces )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6612.129",	0x0000, 0x2000, CRC(1b30976f) SHA1(f76b7f3d88985a5c190e7880c27ab057f102db31) ) /* encrypted */
	ROM_LOAD( "epr-6613.130",	0x2000, 0x2000, CRC(18281f27) SHA1(3fcf2fbd1fc13eda678b77c58c53aa881882286c) ) /* encrypted */
	ROM_LOAD( "epr-6614.131",	0x4000, 0x2000, CRC(69fc3d73) SHA1(287e6b252ae3cd23812b56afe23d4f239f3a76d5) ) /* encrypted */
	ROM_LOAD( "epr-6615.132",	0x6000, 0x2000, CRC(1d0374c8) SHA1(6d818470e294c03b51ec6db8a285d7b71ab2b61f) ) /* encrypted */
	ROM_LOAD( "epr-6577.133",	0x8000, 0x2000, CRC(73616e03) SHA1(429615ee1e041d3e14fc557ec39c380fea07de71) ) /* epr-6616.133 */
	ROM_LOAD( "epr-6617.134",	0xa000, 0x2000, CRC(20b6f895) SHA1(9c9cb3b0c33c4da2850a5756b63c3886634ec544) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6559.120",	0x0000, 0x2000, CRC(5a1570ee) SHA1(fd9215e007b6687d057ea7aee01f6d3dcbc8f894) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6558.62",	0x0000, 0x2000, CRC(2af9eaeb) SHA1(a8a472e9f156c34f1cfcf6d6be808da4303a2276) ) /* epr-6558.82 */
	ROM_LOAD( "epr-6557.61",	0x2000, 0x2000, CRC(6eb131d0) SHA1(27e6f7a3b6ed9a9a5aecfc9981202686b3a81cb4) ) /* epr-6557.65 */
	ROM_LOAD( "epr-6556.64",	0x4000, 0x2000, CRC(79fd26f7) SHA1(a7de0f21ccbcfda495a5c93237569a9b3919d2d5) ) /* epr-6556.81 */
	ROM_LOAD( "epr-6555.63",	0x6000, 0x2000, CRC(7f669aac) SHA1(24ad708112eb26bddf58a70a15273a267121e166) ) /* epr-6555.64 */
	ROM_LOAD( "epr-6554.66",	0x8000, 0x2000, CRC(5ac9d205) SHA1(c3094d10d1d6226bf9ad174d2dd1631b8d6ca33a) ) /* epr-6554.80 */
	ROM_LOAD( "epr-6553.65",	0xa000, 0x2000, CRC(eb82a8fe) SHA1(ec6a418ffbdc8563293d40617aae45382f68ecc2) ) /* epr-6553.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6546.117",	0x0000, 0x4000, CRC(a4785692) SHA1(95ce23076dc86c5d6d3a65274873d4c48e91cc06) ) /* epr-6546.3 */
	ROM_LOAD( "epr-6548.04",	0x4000, 0x4000, CRC(bdf278c1) SHA1(7ebe505f4f0434edb2cee17a6cbce6b900b29cc4) ) /* epr-6548.1 */
	ROM_LOAD( "epr-6547.110",	0x8000, 0x4000, CRC(34451b08) SHA1(ee8708f6c886b63f138bcc10dc2a053bfad96c37) ) /* epr-6547.4 */
	ROM_LOAD( "epr-6549.05",	0xc000, 0x4000, CRC(d2057668) SHA1(ded2a04f7555eb3b1e4da57901ca00635de2c043) ) /* epr-6549.2 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( nprincso )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6550.116",	0x0000, 0x4000, CRC(5f6d59f1) SHA1(e151bf22799c6507a167f83262e48fe2ba74dbd9) ) /* encrypted */
	ROM_LOAD( "epr-6551.109",	0x4000, 0x4000, CRC(1af133b2) SHA1(d3ff924782223ea0566d52ab8b45f17af433966e) ) /* encrypted */
	ROM_LOAD( "epr-6552.96",	0x8000, 0x4000, CRC(f2eeb0d8) SHA1(1f0d1c73ba9eaa2887ffc596f0038b0af37ced49) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6559.120",	0x0000, 0x2000, CRC(5a1570ee) SHA1(fd9215e007b6687d057ea7aee01f6d3dcbc8f894) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6558.62",	0x0000, 0x2000, CRC(2af9eaeb) SHA1(a8a472e9f156c34f1cfcf6d6be808da4303a2276) )
	ROM_LOAD( "epr-6557.61",	0x2000, 0x2000, CRC(6eb131d0) SHA1(27e6f7a3b6ed9a9a5aecfc9981202686b3a81cb4) )
	ROM_LOAD( "epr-6556.64",	0x4000, 0x2000, CRC(79fd26f7) SHA1(a7de0f21ccbcfda495a5c93237569a9b3919d2d5) )
	ROM_LOAD( "epr-6555.63",	0x6000, 0x2000, CRC(7f669aac) SHA1(24ad708112eb26bddf58a70a15273a267121e166) )
	ROM_LOAD( "epr-6554.66",	0x8000, 0x2000, CRC(5ac9d205) SHA1(c3094d10d1d6226bf9ad174d2dd1631b8d6ca33a) )
	ROM_LOAD( "epr-6553.65",	0xa000, 0x2000, CRC(eb82a8fe) SHA1(ec6a418ffbdc8563293d40617aae45382f68ecc2) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6546.117",	0x0000, 0x4000, CRC(a4785692) SHA1(95ce23076dc86c5d6d3a65274873d4c48e91cc06) )
	ROM_LOAD( "epr-6548.04",	0x4000, 0x4000, CRC(bdf278c1) SHA1(7ebe505f4f0434edb2cee17a6cbce6b900b29cc4) )
	ROM_LOAD( "epr-6547.110",	0x8000, 0x4000, CRC(34451b08) SHA1(ee8708f6c886b63f138bcc10dc2a053bfad96c37) )
	ROM_LOAD( "epr-6549.05",	0xc000, 0x4000, CRC(d2057668) SHA1(ded2a04f7555eb3b1e4da57901ca00635de2c043) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( nprincsu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr-6573.129",	0x0000, 0x2000, CRC(d2919c7d) SHA1(993fdde7dd8d4dbad42f8072829cfea794693a37) )
	ROM_LOAD( "epr-6574.130",	0x2000, 0x2000, CRC(5a132833) SHA1(c21cdca6062a6ea2ca306a8dd26b572b3be86321) )
	ROM_LOAD( "epr-6575.131",	0x4000, 0x2000, CRC(a94b0bd4) SHA1(068db579de3dbd545ae41f930a24f2997a2efedf) )
	ROM_LOAD( "epr-6576.132",	0x6000, 0x2000, CRC(27d3bbdb) SHA1(c7f729798c174de73b6582087f6fe2d4db848b6b) )
	ROM_LOAD( "epr-6577.133",	0x8000, 0x2000, CRC(73616e03) SHA1(429615ee1e041d3e14fc557ec39c380fea07de71) )
	ROM_LOAD( "epr-6578.134",	0xa000, 0x2000, CRC(ab68499f) SHA1(6c662a0ff827cc68bcdb26f6b9d48add4f8ef2e9) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6559.120",	0x0000, 0x2000, CRC(5a1570ee) SHA1(fd9215e007b6687d057ea7aee01f6d3dcbc8f894) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6558.62",	0x0000, 0x2000, CRC(2af9eaeb) SHA1(a8a472e9f156c34f1cfcf6d6be808da4303a2276) ) /* epr-6558.82 */
	ROM_LOAD( "epr-6557.61",	0x2000, 0x2000, CRC(6eb131d0) SHA1(27e6f7a3b6ed9a9a5aecfc9981202686b3a81cb4) ) /* epr-6557.65 */
	ROM_LOAD( "epr-6556.64",	0x4000, 0x2000, CRC(79fd26f7) SHA1(a7de0f21ccbcfda495a5c93237569a9b3919d2d5) ) /* epr-6556.81 */
	ROM_LOAD( "epr-6555.63",	0x6000, 0x2000, CRC(7f669aac) SHA1(24ad708112eb26bddf58a70a15273a267121e166) ) /* epr-6555.64 */
	ROM_LOAD( "epr-6554.66",	0x8000, 0x2000, CRC(5ac9d205) SHA1(c3094d10d1d6226bf9ad174d2dd1631b8d6ca33a) ) /* epr-6554.80 */
	ROM_LOAD( "epr-6553.65",	0xa000, 0x2000, CRC(eb82a8fe) SHA1(ec6a418ffbdc8563293d40617aae45382f68ecc2) ) /* epr-6553.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6546.117",	0x0000, 0x4000, CRC(a4785692) SHA1(95ce23076dc86c5d6d3a65274873d4c48e91cc06) ) /* epr-6546.3 */
	ROM_LOAD( "epr-6548.04",	0x4000, 0x4000, CRC(bdf278c1) SHA1(7ebe505f4f0434edb2cee17a6cbce6b900b29cc4) ) /* epr-6548.1 */
	ROM_LOAD( "epr-6547.110",	0x8000, 0x4000, CRC(34451b08) SHA1(ee8708f6c886b63f138bcc10dc2a053bfad96c37) ) /* epr-6547.4 */
	ROM_LOAD( "epr-6549.05",	0xc000, 0x4000, CRC(d2057668) SHA1(ded2a04f7555eb3b1e4da57901ca00635de2c043) ) /* epr-6549.2 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( nprincsb )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "nprinces.001",	0x0000, 0x4000, CRC(e0de073c) SHA1(26aec99ddb080124225e0abf17aac4cc4aed1834) )  /* encrypted */
	ROM_LOAD( "nprinces.002",	0x4000, 0x4000, CRC(27219c7f) SHA1(3f4b0ea9b49907231d10a38d89e2f1803dc168c9) )  /* encrypted */
	ROM_LOAD( "epr-6552.96",	0x8000, 0x4000, CRC(f2eeb0d8) SHA1(1f0d1c73ba9eaa2887ffc596f0038b0af37ced49) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6559.120",	0x0000, 0x2000, CRC(5a1570ee) SHA1(fd9215e007b6687d057ea7aee01f6d3dcbc8f894) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6558.62",	0x0000, 0x2000, CRC(2af9eaeb) SHA1(a8a472e9f156c34f1cfcf6d6be808da4303a2276) )
	ROM_LOAD( "epr-6557.61",	0x2000, 0x2000, CRC(6eb131d0) SHA1(27e6f7a3b6ed9a9a5aecfc9981202686b3a81cb4) )
	ROM_LOAD( "epr-6556.64",	0x4000, 0x2000, CRC(79fd26f7) SHA1(a7de0f21ccbcfda495a5c93237569a9b3919d2d5) )
	ROM_LOAD( "epr-6555.63",	0x6000, 0x2000, CRC(7f669aac) SHA1(24ad708112eb26bddf58a70a15273a267121e166) )
	ROM_LOAD( "epr-6554.66",	0x8000, 0x2000, CRC(5ac9d205) SHA1(c3094d10d1d6226bf9ad174d2dd1631b8d6ca33a) )
	ROM_LOAD( "epr-6553.65",	0xa000, 0x2000, CRC(eb82a8fe) SHA1(ec6a418ffbdc8563293d40617aae45382f68ecc2) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6546.117",	0x0000, 0x4000, CRC(a4785692) SHA1(95ce23076dc86c5d6d3a65274873d4c48e91cc06) )
	ROM_LOAD( "epr-6548.04",	0x4000, 0x4000, CRC(bdf278c1) SHA1(7ebe505f4f0434edb2cee17a6cbce6b900b29cc4) )
	ROM_LOAD( "epr-6547.110",	0x8000, 0x4000, CRC(34451b08) SHA1(ee8708f6c886b63f138bcc10dc2a053bfad96c37) )
	ROM_LOAD( "epr-6549.05",	0xc000, 0x4000, CRC(d2057668) SHA1(ded2a04f7555eb3b1e4da57901ca00635de2c043) )

	ROM_REGION( 0x0220, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
	ROM_LOAD( "nprinces.129",	0x0100, 0x0100, CRC(ae765f62) SHA1(9434b5a23d118a9c62015b479719826b38269cd4) ) /* decryption table (not used) */
	ROM_LOAD( "nprinces.123",	0x0200, 0x0020, CRC(ed5146e9) SHA1(7044035c07636e4029f4b746c1a92e15173869e9) ) /* decryption table (not used) */
ROM_END

ROM_START( imsorry )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6676.116",	0x0000, 0x4000, CRC(eb087d7f) SHA1(b9bcc76bbdfa597d252e7db60fa0f7529e884cce) ) /* encrypted */
	ROM_LOAD( "epr-6677.109",	0x4000, 0x4000, CRC(bd244bee) SHA1(ad9c722fde08f48d8bc835b244450b01a3d747c2) ) /* encrypted */
	ROM_LOAD( "epr-6678.96",	0x8000, 0x4000, CRC(2e16b9fd) SHA1(3395fb769c79f048d099e2898bb7a15611b006c0) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6656.120",	0x0000, 0x2000, CRC(25e3d685) SHA1(a0267d6533af6ff5bf76b9858f2913821a915baf) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6684.62",	0x0000, 0x2000, CRC(2c8df377) SHA1(abcabdecee0ce52000dab831ae1e50fe12c97066) )
	ROM_LOAD( "epr-6683.61",	0x2000, 0x2000, CRC(89431c48) SHA1(99c0d141eb5519c31b194693a1fe9be882cb03fd) )
	ROM_LOAD( "epr-6682.64",	0x4000, 0x2000, CRC(256a9246) SHA1(6aed392a5dd639c54bf54acd3651a77274c0a277) )
	ROM_LOAD( "epr-6681.63",	0x6000, 0x2000, CRC(6974d189) SHA1(57999a73511b2b3f52d7d6a32addc0641255d7b1) )
	ROM_LOAD( "epr-6680.66",	0x8000, 0x2000, CRC(10a629d6) SHA1(fa2c7df33c685e48020ccabcfba5830e7609e392) )
	ROM_LOAD( "epr-6674.65",	0xa000, 0x2000, CRC(143d883c) SHA1(e35f6fae7feb9a353321d8239ac8990bc773e60b) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-6645.117",	0x0000, 0x4000, CRC(1ba167ee) SHA1(5a105cc3112f2533e7c5982233405d365402fba2) )
	ROM_LOAD( "epr-6646.04",	0x4000, 0x4000, CRC(edda7ad6) SHA1(eef7dcde632787283c4cb522380b138060018204) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( imsorryj )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6647.116",	0x0000, 0x4000, CRC(cc5d915d) SHA1(1e2def1f7a03db3504177127dc784fe6c99a7440) ) /* encrypted */
	ROM_LOAD( "epr-6648.109",	0x4000, 0x4000, CRC(37574d60) SHA1(c7c8507b608976973e766956bd28dfb17222de35) ) /* encrypted */
	ROM_LOAD( "epr-6649.96",	0x8000, 0x4000, CRC(5f59bdee) SHA1(289ba35a7869a5b833c8aa4819e76fadde2d1ace) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6656.120",	0x0000, 0x2000, CRC(25e3d685) SHA1(a0267d6533af6ff5bf76b9858f2913821a915baf) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6655.62",	0x0000, 0x2000, CRC(be1f762f) SHA1(abf7af29b1fe4003342fbb431541921433a1fc7c) )
	ROM_LOAD( "epr-6654.61",	0x2000, 0x2000, CRC(ed5f7fc8) SHA1(2e77e8292f644f5bbeebc807f193f20d4591f47a) )
	ROM_LOAD( "epr-6653.64",	0x4000, 0x2000, CRC(8b4845a7) SHA1(048efa9d8122d4a91f4d005d023261a5a5b8b046) )
	ROM_LOAD( "epr-6652.63",	0x6000, 0x2000, CRC(001d68cb) SHA1(c23b4bfbb09b7d3047e04b92d19b69d2ea550879) )
	ROM_LOAD( "epr-6651.66",	0x8000, 0x2000, CRC(4ee9b5e6) SHA1(821bdeefea03c5d3be6d83d0dd30841969d81bd4) )
	ROM_LOAD( "epr-6650.65",	0xa000, 0x2000, CRC(3fca4414) SHA1(d4c80e06bb7027dbc8aea42fb48c71d9fa08ca40) )

	ROM_REGION( 0x8000, REGION_GFX2, 0 ) /* 32k for sprites data */
	ROM_LOAD( "epr-6645.117",	0x0000, 0x4000, CRC(1ba167ee) SHA1(5a105cc3112f2533e7c5982233405d365402fba2) )
	ROM_LOAD( "epr-6646.04",	0x4000, 0x4000, CRC(edda7ad6) SHA1(eef7dcde632787283c4cb522380b138060018204) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( teddybb )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6768.116",	0x0000, 0x4000, CRC(5939817e) SHA1(84d78412d3e13da493d08a40deb2ff3fd51ff9f8) ) /* encrypted */
	ROM_LOAD( "epr-6769.109",	0x4000, 0x4000, CRC(14a98ddd) SHA1(197fa05fb476c02d64e9027cde5aaac26f59b5e8) ) /* encrypted */
	ROM_LOAD( "epr-6770.96",	0x8000, 0x4000, CRC(67b0c7c2) SHA1(b955719c954af5266e06ae7b04ff20f9dc414997) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr6748x.120",	0x0000, 0x2000, CRC(c2a1b89d) SHA1(55c5461640ccb26bed332c13adfbb99c27237bcb) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6747.62",	0x0000, 0x2000, CRC(a0e5aca7) SHA1(e7d35ed5e1606a1ea8b29eeca3ca807ed163573b) ) /* epr-6776.62 */
	ROM_LOAD( "epr-6746.61",	0x2000, 0x2000, CRC(cdb77e51) SHA1(590855f41b62fe9a84db51f90242697abb603c00) ) /* epr-6775.61 */
	ROM_LOAD( "epr-6745.64",	0x4000, 0x2000, CRC(0cab75c3) SHA1(ef9b74c62fbd81db8942f0b7aa2569a8f4843e9d) ) /* epr-6774.64 */
	ROM_LOAD( "epr-6744.63",	0x6000, 0x2000, CRC(0ef8d2cd) SHA1(cf9ebf8e3c1d0794b3d3377464f3908d4fcee6f7) ) /* epr-6773.63 */
	ROM_LOAD( "epr-6743.66",	0x8000, 0x2000, CRC(c33062b5) SHA1(5845da895059ff0271a6ed6fd0fa1392be1ac223) ) /* epr-6772.66 */
	ROM_LOAD( "epr-6742.65",	0xa000, 0x2000, CRC(c457e8c5) SHA1(3c1008ae8b054c198cfeb0a66534fb51beaee0f6) ) /* epr-6771.65 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6735.117",	0x0000, 0x4000, CRC(1be35a97) SHA1(7524cfa1a9c9a2e37753f119e7ac7aa3158621be) )
	ROM_LOAD( "epr-6737.04",	0x4000, 0x4000, CRC(6b53aa7a) SHA1(b1b3ff9460b2321e72b49befa63b61c9c36fedd9) )
	ROM_LOAD( "epr-6736.110",	0x8000, 0x4000, CRC(565c25d0) SHA1(5ae524ef01138c5042b223286d65eb9043c0f0d5) )
	ROM_LOAD( "epr-6738.05",	0xc000, 0x4000, CRC(e116285f) SHA1(b6fb50b02a981b3b23385200045ae537092d26d6) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( teddybbo )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6739.116",	0x0000, 0x4000, CRC(81a37e69) SHA1(ddd0fd7ba5b3646c43ae4261f1e3fedd4184d92c) ) /* encrypted */
	ROM_LOAD( "epr-6740.109",	0x4000, 0x4000, CRC(715388a9) SHA1(5affc4ecb1e0d58b69093aed732b1e292b8d3118) ) /* encrypted */
	ROM_LOAD( "epr-6741.96",	0x8000, 0x4000, CRC(e5a74f5f) SHA1(ccf18b424d4aaeec0bae1e6f096b4c176f6ab554) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6748.120",	0x0000, 0x2000, CRC(9325a1cf) SHA1(555d137b1c974b144ebe6593b4c32c97b3bb5de9) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6747.62",	0x0000, 0x2000, CRC(a0e5aca7) SHA1(e7d35ed5e1606a1ea8b29eeca3ca807ed163573b) )
	ROM_LOAD( "epr-6746.61",	0x2000, 0x2000, CRC(cdb77e51) SHA1(590855f41b62fe9a84db51f90242697abb603c00) )
	ROM_LOAD( "epr-6745.64",	0x4000, 0x2000, CRC(0cab75c3) SHA1(ef9b74c62fbd81db8942f0b7aa2569a8f4843e9d) )
	ROM_LOAD( "epr-6744.63",	0x6000, 0x2000, CRC(0ef8d2cd) SHA1(cf9ebf8e3c1d0794b3d3377464f3908d4fcee6f7) )
	ROM_LOAD( "epr-6743.66",	0x8000, 0x2000, CRC(c33062b5) SHA1(5845da895059ff0271a6ed6fd0fa1392be1ac223) )
	ROM_LOAD( "epr-6742.65",	0xa000, 0x2000, CRC(c457e8c5) SHA1(3c1008ae8b054c198cfeb0a66534fb51beaee0f6) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6735.117",	0x0000, 0x4000, CRC(1be35a97) SHA1(7524cfa1a9c9a2e37753f119e7ac7aa3158621be) )
	ROM_LOAD( "epr-6737.04",	0x4000, 0x4000, CRC(6b53aa7a) SHA1(b1b3ff9460b2321e72b49befa63b61c9c36fedd9) )
	ROM_LOAD( "epr-6736.110",	0x8000, 0x4000, CRC(565c25d0) SHA1(5ae524ef01138c5042b223286d65eb9043c0f0d5) )
	ROM_LOAD( "epr-6738.05",	0xc000, 0x4000, CRC(e116285f) SHA1(b6fb50b02a981b3b23385200045ae537092d26d6) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

/* This is the first System 1 game to have extended ROM space */
ROM_START( hvymetal )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 128k for code + 128k for decrypted opcodes */
	ROM_LOAD( "epra6790.1",   0x00000, 0x8000, CRC(59195bb9) SHA1(63dde673bd875dd23d445b152decb1d70c3750a4) ) /* encrypted */
	ROM_LOAD( "epra6789.2",   0x10000, 0x8000, CRC(83e1d18a) SHA1(07ef58ee2a5212e1e2800efc2bd48d2b2a9ed10d) )
	ROM_LOAD( "epra6788.3",   0x18000, 0x8000, CRC(6ecefd57) SHA1(3236313d5d826873d58af5ad80652c8d0ae0cc31) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr6787.120",  0x0000, 0x8000, CRC(b64ac7f0) SHA1(2b16c2702d3230891b700714a66ece95f1a74b44) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr6795.62",   0x00000, 0x4000, CRC(58a3d038) SHA1(9aabfad143748e2ec1b41fde72a1d533bac3f9d8) )
	ROM_LOAD( "epr6796.61",   0x04000, 0x4000, CRC(d8b08a55) SHA1(cfa5370aa430947637bfe57a5a1f802f273b43f7) )
	ROM_LOAD( "epr6793.64",   0x08000, 0x4000, CRC(487407c2) SHA1(9bb9fff24fe057fa17057ba9263d412905a0c036) )
	ROM_LOAD( "epr6794.63",   0x0c000, 0x4000, CRC(89eb3793) SHA1(90a0cc81d917122c726238585eb802763d34884e) )
	ROM_LOAD( "epr6791.66",   0x10000, 0x4000, CRC(a7dcd042) SHA1(d9bac10aa7ac591a20bfed4e391ec1669eadc32d) )
	ROM_LOAD( "epr6792.65",   0x14000, 0x4000, CRC(d0be5e33) SHA1(1e61c6e14c3c736e74e6c2ff5cde71d1d20b99a4) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr6778.117",  0x00000, 0x8000, CRC(0af61aee) SHA1(90879d4d1bef38714a39ca71c101bd103d250284) )
	ROM_LOAD( "epr6777.110",  0x08000, 0x8000, CRC(91d7a197) SHA1(34c12b7de22169d369ff5b8a8d86da62404267f8) )
	ROM_LOAD( "epr6780.4",    0x10000, 0x8000, CRC(55b31df5) SHA1(aa1ce0b1666e17db196bd1e079691fbe433a9226) )
	ROM_LOAD( "epr6779.5",    0x18000, 0x8000, CRC(e03a2b28) SHA1(7e742c09e832d01f74fe4025d194cbc8d2f24b70) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr7036.3",     0x0000, 0x0100, CRC(146f16fb) SHA1(0a2ac871383b115c16491b9ba5973f0d363eac49) ) /* palette red component */
	ROM_LOAD( "pr7035.2",     0x0100, 0x0100, CRC(50b201ed) SHA1(14c3a585c083dc387532d64bfd63e34f5220e6de) ) /* palette green component */
	ROM_LOAD( "pr7034.1",     0x0200, 0x0100, CRC(dfb5f139) SHA1(56cba261819fd5f2beab56ffd80bb3fd328efe3e) ) /* palette blue component */
	ROM_LOAD( "pr5317p.4",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( myhero )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "epr6963b.116",	0x0000, 0x4000, CRC(4daf89d4) SHA1(6fd69964d4e0dcd5637920711361f1879fcf330e) )
	ROM_LOAD( "epr6964a.109",	0x4000, 0x4000, CRC(c26188e5) SHA1(48d7871a9c63de774c48f1bd9dcaf84b4188f84f) )
	ROM_LOAD( "epr-6927.96",	0x8000, 0x4000, CRC(3cbbaf64) SHA1(fdb5f2ca38010729afa4ed24c087119cf398f27d) ) /* epr-6965.96 */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-69xx.120",	0x0000, 0x2000, CRC(0039e1e9) SHA1(ead2e8a8a518da5ac6ccd5cd6db4cf167ea47c76) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6966.62",	0x0000, 0x2000, CRC(157f0401) SHA1(f07eb40de95054d6a2c2ebec0b251685e8931b37) )
	ROM_LOAD( "epr-6961.61",	0x2000, 0x2000, CRC(be53ce47) SHA1(de6073e7a00cba7e13aca0248c55126b16595d50) )
	ROM_LOAD( "epr-6960.64",	0x4000, 0x2000, CRC(bd381baa) SHA1(e160db821422232fb8f6b4f1c4ce0b61f7bed463) )
	ROM_LOAD( "epr-6959.63",	0x6000, 0x2000, CRC(bc04e79a) SHA1(df93f96aabde981fe9ecf32ef1f99dfebe968835) )
	ROM_LOAD( "epr-6958.66",	0x8000, 0x2000, CRC(714f2c26) SHA1(4696c9322d7b9b27f56309312fe498f14cb32827) )
	ROM_LOAD( "epr-6958.65",	0xa000, 0x2000, CRC(80920112) SHA1(745d029f99b6878efcca535885b9bf98bf8702f2) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6921.117",	0x0000, 0x4000, CRC(f19e05a1) SHA1(98288ba2e96c03a4ab9c8235faa7e01bb376d021) )
	ROM_LOAD( "epr-6923.04",	0x4000, 0x4000, CRC(7988adc3) SHA1(4ee9e964c24234366660af4981566e8c45f46db9) )
	ROM_LOAD( "epr-6922.110",	0x8000, 0x4000, CRC(37f77a78) SHA1(01d8bd41303bd5e3a6f1cdafa4a1d682e4c659a2) )
	ROM_LOAD( "epr-6924.05",	0xc000, 0x4000, CRC(42bdc8f6) SHA1(f31d82641187a7cc77a4a19189b5a15d5168cbd7) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( sscandal )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr6925b.116",	0x0000, 0x4000, CRC(ff54dcec) SHA1(634ba5c79dc20dc6ab3efd9597b9fb1e4f86f58f) ) /* encrypted */
	ROM_LOAD( "epr6926a.109",	0x4000, 0x4000, CRC(5c41eea8) SHA1(6a060a9739ee85c5c3a3e205bfac46bff1ed0b91) ) /* encrypted */
	ROM_LOAD( "epr-6927.96",	0x8000, 0x4000, CRC(3cbbaf64) SHA1(fdb5f2ca38010729afa4ed24c087119cf398f27d) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6934.120",	0x0000, 0x2000, CRC(af467223) SHA1(d79a67e761fe483407cad645dd3b93d86e8790e3) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6933.62",	0x0000, 0x2000, CRC(e7304036) SHA1(cff10b180832703ef472a6abd481f8433308d462) )
	ROM_LOAD( "epr-6932.61",	0x2000, 0x2000, CRC(f5cfbfda) SHA1(52044e3eb6f2e82c9490856410758c5223eb116b) )
	ROM_LOAD( "epr-6931.64",	0x4000, 0x2000, CRC(599d7f87) SHA1(c581001b45856447b2878dc5bdeb92bffb15086a) )
	ROM_LOAD( "epr-6930.63",	0x6000, 0x2000, CRC(cb6616c2) SHA1(84d4f65379cb9d5c9774d29bbad137529ab221a6) )
	ROM_LOAD( "epr-6929.66",	0x8000, 0x2000, CRC(27a16856) SHA1(1e386dfa5178a0902f5d5e64f4d0414593f2e801) )
	ROM_LOAD( "epr-6928.65",	0xa000, 0x2000, CRC(c0c9cfa4) SHA1(3a98f25beab2dcacf5ec4457501ecfde9bc6e8eb) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6921.117",	0x0000, 0x4000, CRC(f19e05a1) SHA1(98288ba2e96c03a4ab9c8235faa7e01bb376d021) )
	ROM_LOAD( "epr-6923.04",	0x4000, 0x4000, CRC(7988adc3) SHA1(4ee9e964c24234366660af4981566e8c45f46db9) )
	ROM_LOAD( "epr-6922.110",	0x8000, 0x4000, CRC(37f77a78) SHA1(01d8bd41303bd5e3a6f1cdafa4a1d682e4c659a2) )
	ROM_LOAD( "epr-6924.05",	0xc000, 0x4000, CRC(42bdc8f6) SHA1(f31d82641187a7cc77a4a19189b5a15d5168cbd7) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( myherok )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	/* all the three program ROMs have bits 0-1 swapped */
	/* when decoded, they are identical to the Japanese version */
	ROM_LOAD( "ry-11.rom",		0x0000, 0x4000, CRC(6f4c8ee5) SHA1(bbbb87a66be383d9d44ae3bb7f4d1ff56933fd57) ) /* encrypted */
	ROM_LOAD( "ry-09.rom",		0x4000, 0x4000, CRC(369302a1) SHA1(670bf97e401c0a665330d2264c126c275f4c5f8d) ) /* encrypted */
	ROM_LOAD( "ry-07.rom",		0x8000, 0x4000, CRC(b8e9922e) SHA1(f563fd415d5218c2c3e0071776c91b6250cacea3) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6934.120",	0x0000, 0x2000, CRC(af467223) SHA1(d79a67e761fe483407cad645dd3b93d86e8790e3) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	/* all three gfx ROMs have address lines A4 and A5 swapped, also #1 and #3 */
	/* have data lines D0 and D6 swapped, while #2 has data lines D1 and D5 swapped. */
	ROM_LOAD( "ry-04.rom",		0x0000, 0x4000, CRC(dfb75143) SHA1(b1943e0b8ca4439d5ef27abecd48e6fc806d3a0e) )
	ROM_LOAD( "ry-03.rom",		0x4000, 0x4000, CRC(cf68b4a2) SHA1(7f1607320943c452bcc30b4805e8e9c9d2a61955) )
	ROM_LOAD( "ry-02.rom",		0x8000, 0x4000, CRC(d100eaef) SHA1(d917a85c3560578cc7640bfcb4725b4217f0ed91) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6921.117",	0x0000, 0x4000, CRC(f19e05a1) SHA1(98288ba2e96c03a4ab9c8235faa7e01bb376d021) )
	ROM_LOAD( "epr-6923.04",	0x4000, 0x4000, CRC(7988adc3) SHA1(4ee9e964c24234366660af4981566e8c45f46db9) )
	ROM_LOAD( "epr-6922.110",	0x8000, 0x4000, CRC(37f77a78) SHA1(01d8bd41303bd5e3a6f1cdafa4a1d682e4c659a2) )
	ROM_LOAD( "epr-6924.05",	0xc000, 0x4000, CRC(42bdc8f6) SHA1(f31d82641187a7cc77a4a19189b5a15d5168cbd7) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( shtngmst )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD("epr-7100.ic18", 0x00000, 0x8000, CRC(268ecb1d) SHA1(a9274c9718f7244235cc6df76331d6a0b7e4e4c8) )
	ROM_LOAD( "epr7101",      0x10000, 0x8000, CRC(ebf5ff72) SHA1(13ae06e3a81cf00b80ec939d5baf30143d61d480) )
	ROM_LOAD( "epr7102",      0x18000, 0x8000, CRC(c890a4ad) SHA1(4b59d37902ace3a69b380ff40652ee37c85f0e9d) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr7043",      0x0000, 0x8000, CRC(99a368ab) SHA1(a9451f39ee2613e5c3e2791d4d8d837b4a3ab666) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr7040",      0x00000, 0x8000, CRC(f30769fa) SHA1(366c1fbe4e1c8943b209f6c831c9a6b7e4372105) )
	ROM_LOAD( "epr7041",      0x08000, 0x8000, CRC(f3e273f9) SHA1(b8715c528299dc1e4f0c19c50d91ca9861a423a1) )
	ROM_LOAD( "epr7042",      0x10000, 0x8000, CRC(6841c917) SHA1(6553843eea0131eb7b5a9aa29dddf641e41d8cc3) )

	ROM_REGION( 0x40000, REGION_GFX2, ROMREGION_ERASEFF )
	ROM_LOAD( "epr7110",   0x00000, 0x8000, CRC(5d1a5048) SHA1(d1626ab1981080451c912df7e4ad7f76c0cb3459) )
	ROM_LOAD( "epr7106",   0x08000, 0x8000, CRC(ae7ab7a2) SHA1(153691e468d29d21b95f1fbffb6896a3140d7e14) )
	ROM_LOAD( "epr7108",   0x10000, 0x8000, CRC(816180ac) SHA1(a59670ec77d4359041ebf12dae5b74add55d82ac) )
	ROM_LOAD( "epr7104",   0x18000, 0x8000, CRC(84a679c5) SHA1(19a21b1b33fc215f606093bfd61d597e4bd0b3d0) )
	ROM_LOAD( "epr7109",   0x20000, 0x8000, CRC(097f7481) SHA1(4d93ea01b811af1cd3e136116625e4b8e06358a2) )
	ROM_LOAD( "epr7105",   0x28000, 0x8000, CRC(13111729) SHA1(57ca2b945db36b056d0e40a39456fd8bf9d0a3ec) )
	ROM_LOAD( "epr7107",   0x30000, 0x8000, CRC(8f50ea24) SHA1(781687e202dedca7b72c9bd5b97d9d46fcfd601c) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "epr7113",      0x0000, 0x0100, CRC(5c0e1360) SHA1(2011b3eef2a58f9bd3f3b1bb9e6c201db85727c2) ) /* palette red component */
	ROM_LOAD( "epr7112",      0x0100, 0x0100, CRC(46fbd351) SHA1(1fca7fbc5d5f8e13e58bbac735511bd0af392446) ) /* palette green component */
	ROM_LOAD( "epr7111",      0x0200, 0x0100, CRC(8123b6b9) SHA1(fb2c5498f0603b5cd270402a738c891a85453666) ) /* palette blue component */
	ROM_LOAD( "epr5317",      0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( chplft )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD( "epr-7124.90",	0x00000, 0x8000, CRC(678d5c41) SHA1(7553979f78270c2ddc5b3f3ebf7817ead8e08de7) )
	ROM_LOAD( "epr-7125.91",	0x10000, 0x8000, CRC(f5283498) SHA1(1ad40f6d7b4cd18212ee56917240c0796f1a4ec2) )
	ROM_LOAD( "epr-7126.92",	0x18000, 0x8000, CRC(dbd192ab) SHA1(03d280c82599a14fc6a2065d57c6241cdc6f1143) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7130.126",	0x0000, 0x8000, CRC(346af118) SHA1(ef579818a45b8ebb276d5832092b26e232d5a737) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7127.4",		0x00000, 0x8000, CRC(1e708f6d) SHA1(b975e13bdc44105e7a15c2694e3ec53b60e23e5e) )
	ROM_LOAD( "epr-7128.5",		0x08000, 0x8000, CRC(b922e787) SHA1(16087671ec7de25f749b5fd66409d48ef7b35820) )
	ROM_LOAD( "epr-7129.6",		0x10000, 0x8000, CRC(bd3b6e6e) SHA1(c66f21b98cb8fc61a9318041ac1812c13099d974) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr-7121.87",	0x00000, 0x8000, CRC(f2b88f73) SHA1(2b06da1beabbea82d502fbe12f6ec3ef26056edd) )
	ROM_LOAD( "epr-7120.86",	0x08000, 0x8000, CRC(517d7fd3) SHA1(3fb5c00224920c3f62fb86e82caf0fee2293e1e2) )
	ROM_LOAD( "epr-7123.89",	0x10000, 0x8000, CRC(8f16a303) SHA1(5f2465505f001dc052e9de4cf66bc1d53fc8c7da) )
	ROM_LOAD( "epr-7122.88",	0x18000, 0x8000, CRC(7c93f160) SHA1(6ab156cad7556808496070f8b02a708ce405c492) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr7119.20",		0x0000, 0x0100, CRC(b2a8260f) SHA1(36c1debb4b3f2f190a25b18d533319d7380416de) ) /* palette red component */
	ROM_LOAD( "pr7118.14",		0x0100, 0x0100, CRC(693e20c7) SHA1(9ebf4bd2c30ddd9648bc4b41c7739cfdf80100da) ) /* palette green component */
	ROM_LOAD( "pr7117.8",		0x0200, 0x0100, CRC(4124307e) SHA1(cee28d891e6ce732c43a61acb5beeafd2200cf37) ) /* palette blue component */
	ROM_LOAD( "pr5317.28",		0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( chplftb )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD( "epr-7152.90",	0x00000, 0x8000, CRC(fe49d83e) SHA1(307be38dd73ed37b275c1b464d266a752cb06132) )
	ROM_LOAD( "epr-7153.91",	0x10000, 0x8000, CRC(48697666) SHA1(0f4c6db9558272f5ceb347e742b284474f18b707) )
	ROM_LOAD( "epr-7154.92",	0x18000, 0x8000, CRC(56d6222a) SHA1(ad8ccf15fe7f1d6716f78490892da0167d79f678) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7130.126",	0x0000, 0x8000, CRC(346af118) SHA1(ef579818a45b8ebb276d5832092b26e232d5a737) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7127.4",		0x00000, 0x8000, CRC(1e708f6d) SHA1(b975e13bdc44105e7a15c2694e3ec53b60e23e5e) )
	ROM_LOAD( "epr-7128.5",		0x08000, 0x8000, CRC(b922e787) SHA1(16087671ec7de25f749b5fd66409d48ef7b35820) )
	ROM_LOAD( "epr-7129.6",		0x10000, 0x8000, CRC(bd3b6e6e) SHA1(c66f21b98cb8fc61a9318041ac1812c13099d974) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr-7121.87",	0x00000, 0x8000, CRC(f2b88f73) SHA1(2b06da1beabbea82d502fbe12f6ec3ef26056edd) )
	ROM_LOAD( "epr-7120.86",	0x08000, 0x8000, CRC(517d7fd3) SHA1(3fb5c00224920c3f62fb86e82caf0fee2293e1e2) )
	ROM_LOAD( "epr-7123.89",	0x10000, 0x8000, CRC(8f16a303) SHA1(5f2465505f001dc052e9de4cf66bc1d53fc8c7da) )
	ROM_LOAD( "epr-7122.88",	0x18000, 0x8000, CRC(7c93f160) SHA1(6ab156cad7556808496070f8b02a708ce405c492) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr7119.20",		0x0000, 0x0100, CRC(b2a8260f) SHA1(36c1debb4b3f2f190a25b18d533319d7380416de) ) /* palette red component */
	ROM_LOAD( "pr7118.14",		0x0100, 0x0100, CRC(693e20c7) SHA1(9ebf4bd2c30ddd9648bc4b41c7739cfdf80100da) ) /* palette green component */
	ROM_LOAD( "pr7117.8",		0x0200, 0x0100, CRC(4124307e) SHA1(cee28d891e6ce732c43a61acb5beeafd2200cf37) ) /* palette blue component */
	ROM_LOAD( "pr5317.28",		0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( chplftbl )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD( "ep7124bl.90",	0x00000, 0x8000, CRC(71a37932) SHA1(72b6f8949d356b3adc5248fdaa13c2a1b9c0fa70) )
	ROM_LOAD( "epr-7125.91",	0x10000, 0x8000, CRC(f5283498) SHA1(1ad40f6d7b4cd18212ee56917240c0796f1a4ec2) )
	ROM_LOAD( "epr-7126.92",	0x18000, 0x8000, CRC(dbd192ab) SHA1(03d280c82599a14fc6a2065d57c6241cdc6f1143) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7130.126",	0x0000, 0x8000, CRC(346af118) SHA1(ef579818a45b8ebb276d5832092b26e232d5a737) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7127.4",		0x00000, 0x8000, CRC(1e708f6d) SHA1(b975e13bdc44105e7a15c2694e3ec53b60e23e5e) )
	ROM_LOAD( "epr-7128.5",		0x08000, 0x8000, CRC(b922e787) SHA1(16087671ec7de25f749b5fd66409d48ef7b35820) )
	ROM_LOAD( "epr-7129.6",		0x10000, 0x8000, CRC(bd3b6e6e) SHA1(c66f21b98cb8fc61a9318041ac1812c13099d974) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr-7121.87",	0x00000, 0x8000, CRC(f2b88f73) SHA1(2b06da1beabbea82d502fbe12f6ec3ef26056edd) )
	ROM_LOAD( "epr-7120.86",	0x08000, 0x8000, CRC(517d7fd3) SHA1(3fb5c00224920c3f62fb86e82caf0fee2293e1e2) )
	ROM_LOAD( "epr-7123.89",	0x10000, 0x8000, CRC(8f16a303) SHA1(5f2465505f001dc052e9de4cf66bc1d53fc8c7da) )
	ROM_LOAD( "epr-7122.88",	0x18000, 0x8000, CRC(7c93f160) SHA1(6ab156cad7556808496070f8b02a708ce405c492) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr7119.20",		0x0000, 0x0100, CRC(b2a8260f) SHA1(36c1debb4b3f2f190a25b18d533319d7380416de) ) /* palette red component */
	ROM_LOAD( "pr7118.14",		0x0100, 0x0100, CRC(693e20c7) SHA1(9ebf4bd2c30ddd9648bc4b41c7739cfdf80100da) ) /* palette green component */
	ROM_LOAD( "pr7117.8",		0x0200, 0x0100, CRC(4124307e) SHA1(cee28d891e6ce732c43a61acb5beeafd2200cf37) ) /* palette blue component */
	ROM_LOAD( "pr5317.28",		0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( 4dwarrio )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "4d.116",       0x0000, 0x4000, CRC(546d1bc7) SHA1(724bb2f77a2b82fae85e535ae4a37820cfb323d0) ) /* encrypted */
	ROM_LOAD( "4d.109",       0x4000, 0x4000, CRC(f1074ec3) SHA1(bc368abeb6c0a7172e03bd7a1754cf4a6ecbb4f8) ) /* encrypted */
	ROM_LOAD( "4d.96",        0x8000, 0x4000, CRC(387c1e8f) SHA1(520ecbafd1c7271dad24410a68067dfd801fa6d6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "4d.120",       0x0000, 0x2000, CRC(5241c009) SHA1(b7a21f95b63234f2496d5ea6e7dc8050ca1b39fc) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "4d.62",        0x0000, 0x2000, CRC(f31b2e09) SHA1(fdc288769495f4b0ca8c7594c9ab7dc0f29e57a4) )
	ROM_LOAD( "4d.61",        0x2000, 0x2000, CRC(5430e925) SHA1(55f92309223c41871175b1f54418c8b08339deb0) )
	ROM_LOAD( "4d.64",        0x4000, 0x2000, CRC(9f442351) SHA1(07076ef66e29c730050e38aecabdfbfced9f9bc4) )
	ROM_LOAD( "4d.63",        0x6000, 0x2000, CRC(633232bd) SHA1(c09c1df4f04608381d665a83776005607ad97ad4) )
	ROM_LOAD( "4d.66",        0x8000, 0x2000, CRC(52bfa2ed) SHA1(ea1c18d07957301f2006350b02fe40d13dbe2aa5) )
	ROM_LOAD( "4d.65",        0xa000, 0x2000, CRC(e9ba4658) SHA1(ba2581a52eb54e2d9f1e1bf30050280df3f5df1b) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "4d.117",       0x0000, 0x4000, CRC(436e4141) SHA1(2574d5c3b01c89d8a041c82af976147d3b87b36b) )
	ROM_LOAD( "4d.04",        0x4000, 0x4000, CRC(8b7cecef) SHA1(4851754cb56784ac248f699f0781646455dd556b) )
	ROM_LOAD( "4d.110",       0x8000, 0x4000, CRC(6ec5990a) SHA1(a26dbd470744c38a26a016e5d4792ac2f2b9bc4b) )
	ROM_LOAD( "4d.05",        0xc000, 0x4000, CRC(f31a1e6a) SHA1(f49dbc4b381e7096d5ffe3c16660dd63121dabf7) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr5317.76",    0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( brain )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD( "brain.1",      0x00000, 0x8000, CRC(2d2aec31) SHA1(02dfbb0e9ca01b864e3aa594cf38306fe82a4b5d) )
	ROM_LOAD( "brain.2",      0x10000, 0x8000, CRC(810a8ab5) SHA1(87cd39f5b1047f355e1d257c691ef11fc55824ca) )
	ROM_RELOAD(               0x08000, 0x8000 )	/* there's code falling through from 7fff */
												/* so I have to copy the ROM there */
	ROM_LOAD( "brain.3",      0x18000, 0x8000, CRC(9a225634) SHA1(9f137938592dd9c5ab2273864a11a682e0f7f783) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "brain.120",    0x0000, 0x8000, CRC(c7e50278) SHA1(9709a59004c6bc39173d0cb94f3602c358367976) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "brain.62",     0x0000, 0x4000, CRC(7dce2302) SHA1(ebf15da3aea36f6a831a5395b0e5fc253852a3ee) )
	ROM_LOAD( "brain.64",     0x4000, 0x4000, CRC(7ce03fd3) SHA1(11f037c75d606276cbf4ec76a2cfdde94a756493) )
	ROM_LOAD( "brain.66",     0x8000, 0x4000, CRC(ea54323f) SHA1(08a4d2543a75a1fbb6ef2c126e3aeb4945bf458f) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "brain.117",    0x00000, 0x8000, CRC(92ff71a4) SHA1(856646c595e0ef7bbcf18844ee34b04e05893ffa) )
	ROM_LOAD( "brain.110",    0x08000, 0x8000, CRC(a1b847ec) SHA1(d71664822b9b863bd2a37da71b4e0850893b9876) )
	ROM_LOAD( "brain.4",      0x10000, 0x8000, CRC(fd2ea53b) SHA1(c7f2d267f19d2c27a550120e003ebfcb10d8af89) )
	/* 18000-1ffff empty */

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "bprom.3",      0x0000, 0x0100, BAD_DUMP CRC(8eee0f72) SHA1(b5694c120f604a5f7cc95618a71ab16a1a6151ed)  ) /* palette red component */
	ROM_LOAD( "bprom.2",      0x0100, 0x0100, BAD_DUMP CRC(3e7babd7) SHA1(d4f8790db4dce75e27156a4c6de2dcef2baf6d76)  ) /* palette green component */
	ROM_LOAD( "bprom.1",      0x0200, 0x0100, BAD_DUMP CRC(371c44a6) SHA1(ac37458d1feb6566b09a795b20c21953d4ab109d)  ) /* palette blue component */
	ROM_LOAD( "pr5317.76",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( raflesia )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-7411.116",	0x0000, 0x4000, CRC(88a0c6c6) SHA1(1deaa8d8d607100966696e5e9dd5f799ba693af0) ) /* encrypted */
	ROM_LOAD( "epr-7412.109",	0x4000, 0x4000, CRC(d3b8cddf) SHA1(368c74d8ae46442cacdb67813dc1c039245da266) ) /* encrypted */
	ROM_LOAD( "epr-7413.96",	0x8000, 0x4000, CRC(b7e688b3) SHA1(ba5c6d5d19e7d51e41949fd5fa576fdae38f9c9c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7420.120",	0x0000, 0x2000, CRC(14387666) SHA1(9cb18e3002c32f658e4725707069f9cd2f496507) ) /* epr-7420.3 */

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7419.62",	0x0000, 0x2000, CRC(bfd5f34c) SHA1(78c4d380d5558212e535c3262223137447d64818) ) /* epr-7419.82 */
	ROM_LOAD( "epr-7418.61",	0x2000, 0x2000, CRC(f8cbc9b6) SHA1(48be9337f704a11ac1fdeb64a3b3518c796bcdd0) ) /* epr-7418.65 */
	ROM_LOAD( "epr-7417.64",	0x4000, 0x2000, CRC(e63501bc) SHA1(5cfd19241c54782c262bbb23c6f682534e77feb7) ) /* epr-7417.81 */
	ROM_LOAD( "epr-7416.63",	0x6000, 0x2000, CRC(093e5693) SHA1(78bb1c4651bd63a9f776766d2eac4f1c09242ed5) ) /* epr-7416.64 */
	ROM_LOAD( "epr-7415.66",	0x8000, 0x2000, CRC(1a8d6bd6) SHA1(b04ee35f603c6c9923ba888914eb43a8b7753d92) ) /* epr-7415.80 */
	ROM_LOAD( "epr-7414.65",	0xa000, 0x2000, CRC(5d20f218) SHA1(bdc0185d133f7bbe287106882bacde846634ffa4) ) /* epr-7414.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7407.117",	0x0000, 0x4000, CRC(f09fc057) SHA1(c6f06144b708055b31fbcba9f38b63736db789d8) ) /* epr-7407.3 */
	ROM_LOAD( "epr-7409.04",	0x4000, 0x4000, CRC(819fedb8) SHA1(e63f0422814423be91d8e1937a13d19693a1a5fc) ) /* epr-7409.1 */
	ROM_LOAD( "epr-7408.110",	0x8000, 0x4000, CRC(3189f33c) SHA1(8476c2c01920f0492cf643929d4f023f3afe0164) ) /* epr-7408.4 */
	ROM_LOAD( "epr-7410.05",	0xc000, 0x4000, CRC(ced74789) SHA1(d0ad845bfe83412ac8d43125e1c50d0581a5b47e) ) /* epr-7410.2 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( spatter )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6392.116",	0x0000, 0x4000, CRC(329b4506) SHA1(8f71ffc3015c4fcf84a895bf53760830602f1040) ) /* encrypted */
	ROM_LOAD( "epr-6393.109",	0x4000, 0x4000, CRC(3b56e25f) SHA1(23f26f8632c8a370b5b3b7a3ec58f359cdf04f73) ) /* encrypted */
	ROM_LOAD( "epr-6394.96",	0x8000, 0x4000, CRC(647c1301) SHA1(5142abfcc63772fd1b47eb584ccda0bc3830e337) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6316.120",	0x0000, 0x2000, CRC(1df95511) SHA1(5780631c8c5a2c3fcd4085f217affa660d72a4e9) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6328.62",	0x0000, 0x2000, CRC(a2bf2832) SHA1(5d7047a6a0c0588a4e98b6ce94d5fd0e6ab963f9) )
	ROM_LOAD( "epr-6397.61",	0x2000, 0x2000, CRC(c60d4471) SHA1(9e8130d575fa342485dfe093e086a4b48e51b904) )
	ROM_LOAD( "epr-6326.64",	0x4000, 0x2000, CRC(269fbb4c) SHA1(7b91f551360698195bf9ce8e32dd2e8fa17e9db8) )
	ROM_LOAD( "epr-6396.63",	0x6000, 0x2000, CRC(c15ccf3b) SHA1(14809ab81816eedb85cacda042e437d48cf9b31a) )
	ROM_LOAD( "epr-6324.66",	0x8000, 0x2000, CRC(8ab3b563) SHA1(6ede93b9f1593dbcbabd6c875bac8ec01a1b40a2) )
	ROM_LOAD( "epr-6395.65",	0xa000, 0x2000, CRC(3f083065) SHA1(cb17c8c2fe04baa58863c10cd8f359a58def3417) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6306.04",	0x0000, 0x4000, CRC(e871e132) SHA1(55f7ab1a8c9a118911c64930452ea05f6ee37fc4) )
	ROM_LOAD( "epr-6308.117",	0x4000, 0x4000, CRC(99c2d90e) SHA1(5be54d931622892b7acc320e714d5b1cdce02d19) )
	ROM_LOAD( "epr-6307.05",	0x8000, 0x4000, CRC(0a5ad543) SHA1(5acada30c1affc4ffbebc8365a9ba4465f213d47) )
	ROM_LOAD( "epr-6309.110",	0xc000, 0x4000, CRC(7423ad98) SHA1(e19b4c64795f30e1491520160d315e4148d58df2) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( ssanchan )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-6310.116",	0x0000, 0x4000, CRC(26b43701) SHA1(e041bde10da12a3f698da09220f0a7cc2ee99abe) ) /* encrypted */
	ROM_LOAD( "epr-6311.109",	0x4000, 0x4000, CRC(cb2bc620) SHA1(ecc69360ad9fcc825b35955fbc29da9ea28b8846) ) /* encrypted */
	ROM_LOAD( "epr-6312.96",	0x8000, 0x4000, CRC(71b15b47) SHA1(7c955be049f9a8d7ca18d877183b698dd5ffe4da) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-6316.120",	0x0000, 0x2000, CRC(1df95511) SHA1(5780631c8c5a2c3fcd4085f217affa660d72a4e9) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-6328.62",	0x0000, 0x2000, CRC(a2bf2832) SHA1(5d7047a6a0c0588a4e98b6ce94d5fd0e6ab963f9) )
	ROM_LOAD( "epr-6327.61",	0x2000, 0x2000, CRC(53298109) SHA1(75fd37034aee78d63939d8b4f584c1dc1042264b) )
	ROM_LOAD( "epr-6326.64",	0x4000, 0x2000, CRC(269fbb4c) SHA1(7b91f551360698195bf9ce8e32dd2e8fa17e9db8) )
	ROM_LOAD( "epr-6325.63",	0x6000, 0x2000, CRC(bf038745) SHA1(2fda2412f76b8971ba543ec10da07d4b0d1f2006) )
	ROM_LOAD( "epr-6324.66",	0x8000, 0x2000, CRC(8ab3b563) SHA1(6ede93b9f1593dbcbabd6c875bac8ec01a1b40a2) )
	ROM_LOAD( "epr-6323.65",	0xa000, 0x2000, CRC(0394673c) SHA1(fbee6a5cb37d0394db95781b9f165d766546eb33) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-6306.04",	0x0000, 0x4000, CRC(e871e132) SHA1(55f7ab1a8c9a118911c64930452ea05f6ee37fc4) )
	ROM_LOAD( "epr-6308.117",	0x4000, 0x4000, CRC(99c2d90e) SHA1(5be54d931622892b7acc320e714d5b1cdce02d19) )
	ROM_LOAD( "epr-6307.05",	0x8000, 0x4000, CRC(0a5ad543) SHA1(5acada30c1affc4ffbebc8365a9ba4465f213d47) )
	ROM_LOAD( "epr-6309.110",	0xc000, 0x4000, CRC(7423ad98) SHA1(e19b4c64795f30e1491520160d315e4148d58df2) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( wboy )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-7489.116",	0x0000, 0x4000, CRC(130f4b70) SHA1(4a2ea5bc06f3a240c68813be3a9f9bef2bcf4e9c) ) /* encrypted */
	ROM_LOAD( "epr-7490.109",	0x4000, 0x4000, CRC(9e656733) SHA1(2233beb874b7cb48899afe603fef567932951a88) ) /* encrypted */
	ROM_LOAD( "epr-7491.96",	0x8000, 0x4000, CRC(1f7d0efe) SHA1(a1b4f8faf1614f4808df1292209c340f1490adbd) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7498.120",	0x0000, 0x2000, CRC(78ae1e7b) SHA1(86032f443359b0bb2766e33024ed2e320aa9bc84) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) )
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) )
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) )
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) )
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) )
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7485.117",	0x0000, 0x4000, CRC(c2891722) SHA1(e4e11c0e9bd0dc121c25349493f2b13d2ff8c807) )
	ROM_LOAD( "epr-7487.04",	0x4000, 0x4000, CRC(2d3a421b) SHA1(d70440a8703ccface3212cd9544c950b36263e8c) )
	ROM_LOAD( "epr-7486.110",	0x8000, 0x4000, CRC(8d622c50) SHA1(9a76a50204c618347d3e8eee6cda841becd906eb) )
	ROM_LOAD( "epr-7488.05",	0xc000, 0x4000, CRC(007c2f1b) SHA1(c2f1376144a49d20cb35384648e06d06978474c1) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wboyo )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-.116",		0x0000, 0x4000, CRC(51d27534) SHA1(1cbc7201aacde89857f83b2600f309b514c5e758) ) /* encrypted */
	ROM_LOAD( "epr-.109",		0x4000, 0x4000, CRC(e29d1cd1) SHA1(f6ff4a6fffea77cc5706549bb2d8bf9e96ed0be0) ) /* encrypted */
	ROM_LOAD( "epr-7491.96",	0x8000, 0x4000, CRC(1f7d0efe) SHA1(a1b4f8faf1614f4808df1292209c340f1490adbd) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7498.120",	0x0000, 0x2000, CRC(78ae1e7b) SHA1(86032f443359b0bb2766e33024ed2e320aa9bc84) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) )
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) )
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) )
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) )
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) )
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7485.117",	0x0000, 0x4000, CRC(c2891722) SHA1(e4e11c0e9bd0dc121c25349493f2b13d2ff8c807) )
	ROM_LOAD( "epr-7487.04",	0x4000, 0x4000, CRC(2d3a421b) SHA1(d70440a8703ccface3212cd9544c950b36263e8c) )
	ROM_LOAD( "epr-7486.110",	0x8000, 0x4000, CRC(8d622c50) SHA1(9a76a50204c618347d3e8eee6cda841becd906eb) )
	ROM_LOAD( "epr-7488.05",	0xc000, 0x4000, CRC(007c2f1b) SHA1(c2f1376144a49d20cb35384648e06d06978474c1) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wboy2 )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "epr-7587.129",	0x0000, 0x2000, CRC(1bbb7354) SHA1(e299979299c93981f5d28a1a614ad644506911dd) ) /* encrypted */
	ROM_LOAD( "epr-7588.130",	0x2000, 0x2000, CRC(21007413) SHA1(f45443a49e916465e5c8a8b348897ab426a897bd) ) /* encrypted */
	ROM_LOAD( "epr-7589.131",	0x4000, 0x2000, CRC(44b30433) SHA1(558d799c8f48f76c651f19e2a81160eb78ac6642) ) /* encrypted */
	ROM_LOAD( "epr-7590.132",	0x6000, 0x2000, CRC(bb525a0b) SHA1(5cd4731e0adfb5c660144eccda759e12a30ce78e) ) /* encrypted */
	ROM_LOAD( "epr-7591.133",	0x8000, 0x2000, CRC(8379aa23) SHA1(da47e0150b724a00878ef5f953fa6ac80bb27d8d) )
	ROM_LOAD( "epr-7592.134",	0xa000, 0x2000, CRC(c767a5d7) SHA1(a4e8d6a8278ac2227bde8c24d45aa7ab2a273579) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7498.120",	0x0000, 0x2000, CRC(78ae1e7b) SHA1(86032f443359b0bb2766e33024ed2e320aa9bc84) ) /* epr-7498.3 */

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) ) /* epr-7497.82 */
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) ) /* epr-7496.65 */
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) ) /* epr-7495.81 */
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) ) /* epr-7494.64 */
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) ) /* epr-7493.80 */
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) ) /* epr-7492.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7485.117",	0x0000, 0x4000, CRC(c2891722) SHA1(e4e11c0e9bd0dc121c25349493f2b13d2ff8c807) ) /* epr-7485.3 */
	ROM_LOAD( "epr-7487.04",	0x4000, 0x4000, CRC(2d3a421b) SHA1(d70440a8703ccface3212cd9544c950b36263e8c) ) /* epr-7487.1 */
	ROM_LOAD( "epr-7486.110",	0x8000, 0x4000, CRC(8d622c50) SHA1(9a76a50204c618347d3e8eee6cda841becd906eb) ) /* epr-7486.4 */
	ROM_LOAD( "epr-7488.05",	0xc000, 0x4000, CRC(007c2f1b) SHA1(c2f1376144a49d20cb35384648e06d06978474c1) ) /* epr-7488.2 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( wboy2u )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "ic129_02.bin",	0x0000, 0x2000, CRC(32c4b709) SHA1(e57b7b6818f12fdd5f1600ed54c0b8a7f538aa71) )
	ROM_LOAD( "ic130_03.bin",	0x2000, 0x2000, CRC(56463ede) SHA1(c58c220aa0d0e194581646e6db2491075fdc37b9) )
	ROM_LOAD( "ic131_04.bin",	0x4000, 0x2000, CRC(775ed392) SHA1(073f8f70685913736eb04be8215a47b5253cb531) )
	ROM_LOAD( "ic132_05.bin",	0x6000, 0x2000, CRC(7b922708) SHA1(c2e1f67b756f558d6904fe82d6f5483cda5f9045) )
	ROM_LOAD( "epr-7591.133",	0x8000, 0x2000, CRC(8379aa23) SHA1(da47e0150b724a00878ef5f953fa6ac80bb27d8d) )
	ROM_LOAD( "epr-7592.134",	0xa000, 0x2000, CRC(c767a5d7) SHA1(a4e8d6a8278ac2227bde8c24d45aa7ab2a273579) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr7498a.3",		0x0000, 0x2000, CRC(c198205c) SHA1(d2d5cd154ce6a5a3c6a099b4ab2ea7cc045ab0a1) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) ) /* epr-7497.82 */
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) ) /* epr-7496.65 */
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) ) /* epr-7495.81 */
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) ) /* epr-7494.64 */
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) ) /* epr-7493.80 */
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) ) /* epr-7492.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7485.117",	0x0000, 0x4000, CRC(c2891722) SHA1(e4e11c0e9bd0dc121c25349493f2b13d2ff8c807) ) /* epr-7485.3 */
	ROM_LOAD( "epr-7487.04",	0x4000, 0x4000, CRC(2d3a421b) SHA1(d70440a8703ccface3212cd9544c950b36263e8c) ) /* epr-7487.1 */
	ROM_LOAD( "epr-7486.110",	0x8000, 0x4000, CRC(8d622c50) SHA1(9a76a50204c618347d3e8eee6cda841becd906eb) ) /* epr-7486.4 */
	ROM_LOAD( "epr-7488.05",	0xc000, 0x4000, CRC(007c2f1b) SHA1(c2f1376144a49d20cb35384648e06d06978474c1) ) /* epr-7488.2 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( wboy3 )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "wb_1",			0x0000, 0x4000, CRC(bd6fef49) SHA1(6469a84cc1fd4ebf8c58b6efd3b255414bc86699) ) /* encrypted */
	ROM_LOAD( "wb_2",			0x4000, 0x4000, CRC(4081b624) SHA1(892fd347638ec900a7afc3d338b68e9d0a14f2b4) ) /* encrypted */
	ROM_LOAD( "wb_3",			0x8000, 0x4000, CRC(c48a0e36) SHA1(c9b9e51334e8b698be2195dda7701bb51760e502) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7498.120",	0x0000, 0x2000, CRC(78ae1e7b) SHA1(86032f443359b0bb2766e33024ed2e320aa9bc84) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) )
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) )
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) )
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) )
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) )
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7485.117",	0x0000, 0x4000, CRC(c2891722) SHA1(e4e11c0e9bd0dc121c25349493f2b13d2ff8c807) )
	ROM_LOAD( "epr-7487.04",	0x4000, 0x4000, CRC(2d3a421b) SHA1(d70440a8703ccface3212cd9544c950b36263e8c) )
	ROM_LOAD( "epr-7486.110",	0x8000, 0x4000, CRC(8d622c50) SHA1(9a76a50204c618347d3e8eee6cda841becd906eb) )
	ROM_LOAD( "epr-7488.05",	0xc000, 0x4000, CRC(007c2f1b) SHA1(c2f1376144a49d20cb35384648e06d06978474c1) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wboyu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "ic116_89.bin",	0x0000, 0x4000, CRC(73d8cef0) SHA1(a6f1f8de44a88f995836ce03b5a073306c56aaeb) )
	ROM_LOAD( "ic109_90.bin",	0x4000, 0x4000, CRC(29546828) SHA1(905d76bc2b212a161ad2f2bef144261bb73c94cb) )
	ROM_LOAD( "ic096_91.bin",	0x8000, 0x4000, CRC(c7145c2a) SHA1(0b2fd6f519a4b87bc27db5d03c489c7ff75e942a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr-7498.120",	0x0000, 0x2000, CRC(78ae1e7b) SHA1(86032f443359b0bb2766e33024ed2e320aa9bc84) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) ) /* epr-7497.82 */
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) ) /* epr-7496.65 */
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) ) /* epr-7495.81 */
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) ) /* epr-7494.64 */
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) ) /* epr-7493.80 */
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) ) /* epr-7492.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "ic117_85.bin",	0x0000, 0x4000, CRC(1ee96ae8) SHA1(4e69b87e919894b961477e6cc5272f448495d847) )
	ROM_LOAD( "ic004_87.bin",	0x4000, 0x4000, CRC(119735bb) SHA1(001efa55d7fbcd2fdb6da17b136f295e5ea4a4c2) )
	ROM_LOAD( "ic110_86.bin",	0x8000, 0x4000, CRC(26d0fac4) SHA1(2e6a06f6850b2d19e3dd7dcdc6b700d0eda878cb) )
	ROM_LOAD( "ic005_88.bin",	0xc000, 0x4000, CRC(2602e519) SHA1(00e94ec7ae37b5063137d4d49af7806fb0357c4b) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wbdeluxe )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "wbd1.bin",		0x0000, 0x2000, CRC(a1bedbd7) SHA1(32d171847ca02b01a7ac810cac3185c81c923285) )
	ROM_LOAD( "ic130_03.bin",	0x2000, 0x2000, CRC(56463ede) SHA1(c58c220aa0d0e194581646e6db2491075fdc37b9) )
	ROM_LOAD( "wbd3.bin",		0x4000, 0x2000, CRC(6fcdbd4c) SHA1(4fb9a916c99bf267c0035cb80b16400732991f83) )
	ROM_LOAD( "ic132_05.bin",	0x6000, 0x2000, CRC(7b922708) SHA1(c2e1f67b756f558d6904fe82d6f5483cda5f9045) )
	ROM_LOAD( "wbd5.bin",		0x8000, 0x2000, CRC(f6b02902) SHA1(9a43b84d9537d70e9c0d75010a824bcaec57a50c) )
	ROM_LOAD( "wbd6.bin",		0xa000, 0x2000, CRC(43df21fe) SHA1(c1b88505942f48b0df2362bbb618689febe00d1f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr7498a.3",		0x0000, 0x2000, CRC(c198205c) SHA1(d2d5cd154ce6a5a3c6a099b4ab2ea7cc045ab0a1) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr-7497.62",	0x0000, 0x2000, CRC(08d609ca) SHA1(11799e9ef7e6942b304f132b404bff3ed44d524b) ) /* epr-7497.82 */
	ROM_LOAD( "epr-7496.61",	0x2000, 0x2000, CRC(6f61fdf1) SHA1(21826aebf5835b9f3d9c467c8647809c1bc0d01f) ) /* epr-7496.65 */
	ROM_LOAD( "epr-7495.64",	0x4000, 0x2000, CRC(6a0d2c2d) SHA1(8c21d7f0768e8dda2b7185f3c510cae4229a4a2e) ) /* epr-7495.81 */
	ROM_LOAD( "epr-7494.63",	0x6000, 0x2000, CRC(a8e281c7) SHA1(a88b80a7b94ab1401bbf28d7707fdf28a5505127) ) /* epr-7494.64 */
	ROM_LOAD( "epr-7493.66",	0x8000, 0x2000, CRC(89305df4) SHA1(7a5098624769a31e7512f56831e818bce6a18871) ) /* epr-7493.80 */
	ROM_LOAD( "epr-7492.65",	0xa000, 0x2000, CRC(60f806b1) SHA1(f91e5868a455dff2bce3c2891a7cfd648957cd73) ) /* epr-7492.63 */

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "epr-7485.117",	0x0000, 0x4000, CRC(c2891722) SHA1(e4e11c0e9bd0dc121c25349493f2b13d2ff8c807) ) /* epr-7485.3 */
	ROM_LOAD( "epr-7487.04",	0x4000, 0x4000, CRC(2d3a421b) SHA1(d70440a8703ccface3212cd9544c950b36263e8c) ) /* epr-7487.1 */
	ROM_LOAD( "epr-7486.110",	0x8000, 0x4000, CRC(8d622c50) SHA1(9a76a50204c618347d3e8eee6cda841becd906eb) ) /* epr-7486.4 */
	ROM_LOAD( "epr-7488.05",	0xc000, 0x4000, CRC(007c2f1b) SHA1(c2f1376144a49d20cb35384648e06d06978474c1) ) /* epr-7488.2 */

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr-5317.76",		0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
															 /* pr-5317.106 */
ROM_END

ROM_START( gardia )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 128k for code + 128k for decrypted opcodes */
	ROM_LOAD( "epr10255.1",   0x00000, 0x8000, CRC(89282a6b) SHA1(f19e345e5fae6a518276cc1bd09d1e2083672b25) ) /* encrypted */
	ROM_LOAD( "epr10254.2",   0x10000, 0x8000, CRC(2826b6d8) SHA1(de1faf33cca031b72052bf5244fcb0bd79d85659) )
	ROM_LOAD( "epr10253.3",   0x18000, 0x8000, CRC(7911260f) SHA1(44196f0a6c4c2b22a68609ddfc75be6a7877a69a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr10243.120", 0x0000, 0x4000, CRC(87220660) SHA1(3f2bfc03e0f1053a4aa0ec5ebb0d573f2e20964c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr10249.61",  0x0000, 0x4000, CRC(4e0ad0f2) SHA1(b76c155b674f3ad8938278d5dbb0452351c716a5) )
	ROM_LOAD( "epr10248.64",  0x4000, 0x4000, CRC(3515d124) SHA1(39b28a103d8bfe702a376ebd880d6060e3d1ab30) )
	ROM_LOAD( "epr10247.66",  0x8000, 0x4000, CRC(541e1555) SHA1(6660204c74a9f7e63b3ba08d99fb854aa863710e) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr10234.117", 0x00000, 0x8000, CRC(8a6aed33) SHA1(044836885ace8294124b1be9b3a4828f772bb9ee) )
	ROM_LOAD( "epr10233.110", 0x08000, 0x8000, CRC(c52784d3) SHA1(b37d7f261be12616dbe11dfa375eaf6878e4a0f3) )
	ROM_LOAD( "epr10236.04",  0x10000, 0x8000, CRC(b35ab227) SHA1(616f6097afddffa9af89fe84d8b6df59c567c1e6) )
	ROM_LOAD( "epr10235.5",   0x18000, 0x8000, CRC(006a3151) SHA1(a575f9d5c026e6b18e990720ec7520b6b5ae94e3) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "bprom.3",      0x0000, 0x0100, CRC(8eee0f72) SHA1(b5694c120f604a5f7cc95618a71ab16a1a6151ed) ) /* palette red component */
	ROM_LOAD( "bprom.2",      0x0100, 0x0100, CRC(3e7babd7) SHA1(d4f8790db4dce75e27156a4c6de2dcef2baf6d76) ) /* palette green component */
	ROM_LOAD( "bprom.1",      0x0200, 0x0100, CRC(371c44a6) SHA1(ac37458d1feb6566b09a795b20c21953d4ab109d) ) /* palette blue component */
	ROM_LOAD( "pr5317.4",     0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( gardiab )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 128k for code + 128k for decrypted opcodes */
	ROM_LOAD( "gardiabl.5",   0x00000, 0x8000, CRC(207f9cbb) SHA1(647de15ac69a904344f3c18c9da8cefd626387db) ) /* encrypted */
	ROM_LOAD( "gardiabl.6",   0x10000, 0x8000, CRC(b2ed05dc) SHA1(c520bf7024c85dc759c27eccb0a31998f4d72b5f) )
	ROM_LOAD( "gardiabl.7",   0x18000, 0x8000, CRC(0a490588) SHA1(18df754ebdf062096f2d631a722b168901610345) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr10243.120", 0x0000, 0x4000, CRC(87220660) SHA1(3f2bfc03e0f1053a4aa0ec5ebb0d573f2e20964c) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gardiabl.8",   0x0000, 0x4000, CRC(367c9a17) SHA1(bde7592ce94bbc6674c04b427c52e74207066f56) )
	ROM_LOAD( "gardiabl.9",   0x4000, 0x4000, CRC(1540fd30) SHA1(e2d134e0715231a428fd112be81493a0e2a2642f) )
	ROM_LOAD( "gardiabl.10",  0x8000, 0x4000, CRC(e5c9af10) SHA1(6bff5bbc0f339e84a8e31446dc9897c02600fbcf) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr10234.117", 0x00000, 0x8000, CRC(8a6aed33) SHA1(044836885ace8294124b1be9b3a4828f772bb9ee) )
	ROM_LOAD( "epr10233.110", 0x08000, 0x8000, CRC(c52784d3) SHA1(b37d7f261be12616dbe11dfa375eaf6878e4a0f3) )
	ROM_LOAD( "epr10236.04",  0x10000, 0x8000, CRC(b35ab227) SHA1(616f6097afddffa9af89fe84d8b6df59c567c1e6) )
	ROM_LOAD( "epr10235.5",   0x18000, 0x8000, CRC(006a3151) SHA1(a575f9d5c026e6b18e990720ec7520b6b5ae94e3) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "bprom.3",      0x0000, 0x0100, CRC(8eee0f72) SHA1(b5694c120f604a5f7cc95618a71ab16a1a6151ed) ) /* palette red component */
	ROM_LOAD( "bprom.2",      0x0100, 0x0100, CRC(3e7babd7) SHA1(d4f8790db4dce75e27156a4c6de2dcef2baf6d76) ) /* palette green component */
	ROM_LOAD( "bprom.1",      0x0200, 0x0100, CRC(371c44a6) SHA1(ac37458d1feb6566b09a795b20c21953d4ab109d) ) /* palette blue component */
	ROM_LOAD( "pr5317.4",     0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( blockgal )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* 64k for code */
	ROM_LOAD( "bg.116",       0x0000, 0x4000, CRC(a99b231a) SHA1(42ba45a4fd315255e9500bc3a0e8fe653c4c5a9c) ) /* encrypted */
	ROM_LOAD( "bg.109",       0x4000, 0x4000, CRC(a6b573d5) SHA1(33547a3895bbe65d5a6c40453eeb93e1fedad6de) ) /* encrypted */
	/* 0x8000-0xbfff empty (was same as My Hero) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "bg.120",       0x0000, 0x2000, CRC(d848faff) SHA1(5974cc0c3090800ca79f580a620f5b6615f5d039) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "bg.62",        0x0000, 0x2000, CRC(7e3ea4eb) SHA1(8bf020b083e2da12fe95ddae9ac7a385490525bc) )
	ROM_LOAD( "bg.61",        0x2000, 0x2000, CRC(4dd3d39d) SHA1(759fca021f8d59e861dc19543d5a184428a5e472) )
	ROM_LOAD( "bg.64",        0x4000, 0x2000, CRC(17368663) SHA1(e8f2ac6de0fddf08aefae07e693cac100cfb0db4) )
	ROM_LOAD( "bg.63",        0x6000, 0x2000, CRC(0c8bc404) SHA1(fc96fb682da3af6b7fc852cea6d8a957c4ce57e3) )
	ROM_LOAD( "bg.66",        0x8000, 0x2000, CRC(2b7dc4fa) SHA1(79d3677b24682cee0c08088433646800703be531) )
	ROM_LOAD( "bg.65",        0xa000, 0x2000, CRC(ed121306) SHA1(89f812b3954922e22fcf8d9cc4ee5ba295279cb6) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "bg.117",       0x0000, 0x4000, CRC(e99cc920) SHA1(b2b9199a9296e0c34fcf4dd20ffd3e8de08f42da) )
	ROM_LOAD( "bg.04",        0x4000, 0x4000, CRC(213057f8) SHA1(a872631aaa2b73e9198f2ad6cede2a889279e610) )
	ROM_LOAD( "bg.110",       0x8000, 0x4000, CRC(064c812c) SHA1(673790dc5131fd280333386a0e9915fb94e9f3e1) )
	ROM_LOAD( "bg.05",        0xc000, 0x4000, CRC(02e0b040) SHA1(fb626fc31dfe25bf9fac0c8d76d5041609b06e82) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr5317.76",    0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( blckgalb )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 ) /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "ic62",         0x10000, 0x8000, CRC(65c47676) SHA1(bc283761e6f9ebf65fb405b1c8922c3c98c8d00e) ) /* decrypted opcodes */
	ROM_CONTINUE(             0x00000, 0x8000 )             /* decrypted data */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "bg.120",       0x0000, 0x2000, CRC(d848faff) SHA1(5974cc0c3090800ca79f580a620f5b6615f5d039) )

	ROM_REGION( 0xc000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "bg.62",        0x0000, 0x2000, CRC(7e3ea4eb) SHA1(8bf020b083e2da12fe95ddae9ac7a385490525bc) )
	ROM_LOAD( "bg.61",        0x2000, 0x2000, CRC(4dd3d39d) SHA1(759fca021f8d59e861dc19543d5a184428a5e472) )
	ROM_LOAD( "bg.64",        0x4000, 0x2000, CRC(17368663) SHA1(e8f2ac6de0fddf08aefae07e693cac100cfb0db4) )
	ROM_LOAD( "bg.63",        0x6000, 0x2000, CRC(0c8bc404) SHA1(fc96fb682da3af6b7fc852cea6d8a957c4ce57e3) )
	ROM_LOAD( "bg.66",        0x8000, 0x2000, CRC(2b7dc4fa) SHA1(79d3677b24682cee0c08088433646800703be531) )
	ROM_LOAD( "bg.65",        0xa000, 0x2000, CRC(ed121306) SHA1(89f812b3954922e22fcf8d9cc4ee5ba295279cb6) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 ) /* 64k for sprites data */
	ROM_LOAD( "bg.117",       0x0000, 0x4000, CRC(e99cc920) SHA1(b2b9199a9296e0c34fcf4dd20ffd3e8de08f42da) )
	ROM_LOAD( "bg.04",        0x4000, 0x4000, CRC(213057f8) SHA1(a872631aaa2b73e9198f2ad6cede2a889279e610) )
	ROM_LOAD( "bg.110",       0x8000, 0x4000, CRC(064c812c) SHA1(673790dc5131fd280333386a0e9915fb94e9f3e1) )
	ROM_LOAD( "bg.05",        0xc000, 0x4000, CRC(02e0b040) SHA1(fb626fc31dfe25bf9fac0c8d76d5041609b06e82) )

	ROM_REGION( 0x0100, REGION_USER1, 0 ) /* misc PROMs, but no color so don't use REGION_PROMS! */
	ROM_LOAD( "pr5317.76",    0x0000, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( tokisens )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD( "epr10961.90",  0x00000, 0x8000, CRC(1466b61d) SHA1(99f93813834d3a7c9f6228076d400f74d9b6dea9) )
	ROM_LOAD( "epr10962.91",  0x10000, 0x8000, CRC(a8479f91) SHA1(0700746fb481fd2bd22ae82c9881aa61222a6379) )
	ROM_LOAD( "epr10963.92",  0x18000, 0x8000, CRC(b7193b39) SHA1(d40fb8591b1ff83f3d56b955ac11a07496a0adbb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr10967.126", 0x0000, 0x8000, CRC(97966bf2) SHA1(b5a3d36afbb3d6e2e2e2c121609a30dc080ccf13) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr10964.4",   0x00000, 0x8000, CRC(9013b85c) SHA1(c27322245052ffc9d840fe683ed35965c61bf9e8) )
	ROM_LOAD( "epr10965.5",   0x08000, 0x8000, CRC(e4755cc6) SHA1(33370d556a70e19edce5e0c7fa8b11453ccbe91b) )
	ROM_LOAD( "epr10966.6",   0x10000, 0x8000, CRC(5bbfbdcc) SHA1(e7e679da874a79dfdda0be58d1352c192635296d) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr10958.87",  0x00000, 0x8000, CRC(fc2bcbd7) SHA1(6b9007f2057e4c860ecae4ba5db4e02b8aaae8fd) )
	ROM_LOAD( "epr10957.86",  0x08000, 0x8000, CRC(4ec56860) SHA1(9fd6ba8a68b4cb98183e8ac8643656c251f1c72d) )
	ROM_LOAD( "epr10960.89",  0x10000, 0x8000, CRC(880e0d44) SHA1(2b2dc144807d1d048ffe81bfd33a77ccf618dd3e) )
	ROM_LOAD( "epr10959.88",  0x18000, 0x8000, CRC(4deda48f) SHA1(12db2a69286f22cd8243be6faa9a075fafec1dfd) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "bprom.20",      0x0000, 0x0100, CRC(8eee0f72) SHA1(b5694c120f604a5f7cc95618a71ab16a1a6151ed) ) /* palette red component */
	ROM_LOAD( "bprom.14",      0x0100, 0x0100, CRC(3e7babd7) SHA1(d4f8790db4dce75e27156a4c6de2dcef2baf6d76) ) /* palette green component */
	ROM_LOAD( "bprom.8",       0x0200, 0x0100, CRC(371c44a6) SHA1(ac37458d1feb6566b09a795b20c21953d4ab109d) ) /* palette blue component */
	ROM_LOAD( "bprom.28",      0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wbml )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 256k for code + 256k for decrypted opcodes */
	ROM_LOAD( "ep11031a.90",  0x00000, 0x8000, CRC(bd3349e5) SHA1(65cc16e5d3b08429388946df254b8122ad1da339) ) /* encrypted */
	ROM_LOAD( "epr11032.91",  0x10000, 0x8000, CRC(9d03bdb2) SHA1(7dbab23e7c7972d9b51a0d3d046374720b7d6af5) ) /* encrypted */
	ROM_LOAD( "epr11033.92",  0x18000, 0x8000, CRC(7076905c) SHA1(562fbd9bd60851f7e4e60b725193395b4f193479) ) /* encrypted */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11037.126", 0x0000, 0x8000, CRC(7a4ee585) SHA1(050436106cced5dcbf40a3d94d48202eedddc3ad) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr11034.4",   0x00000, 0x8000, CRC(37a2077d) SHA1(57f032a98022bf03ff98cb2e563178ba97e4b63c) )
	ROM_LOAD( "epr11035.5",   0x08000, 0x8000, CRC(cdf2a21b) SHA1(db2553866f21e03bd9d668c179be3352adbaf8a6) )
	ROM_LOAD( "epr11036.6",   0x10000, 0x8000, CRC(644687fa) SHA1(d6c5bc95da4fc7e81091dcfe6205b6f47d54af76) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11028.87",  0x00000, 0x8000, CRC(af0b3972) SHA1(413825f66b84c7e45aa1855131482abead8f7f3b) )
	ROM_LOAD( "epr11027.86",  0x08000, 0x8000, CRC(277d8f1d) SHA1(7854673503ed03d276abe971805a11f8c992f6d6) )
	ROM_LOAD( "epr11030.89",  0x10000, 0x8000, CRC(f05ffc76) SHA1(f3dbb518240f86430840c3d4cda04bac79c20f69) )
	ROM_LOAD( "epr11029.88",  0x18000, 0x8000, CRC(cedc9c61) SHA1(dbe5744f9b6f2a406b52b910dd4e133db7bce6b2) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11026.20",   0x0000, 0x0100, CRC(27057298) SHA1(654be7abb937bb0720263ee6512e31194662effe) )
	ROM_LOAD( "pr11025.14",   0x0100, 0x0100, CRC(41e4d86b) SHA1(a86e8bb0a465d01b04410edfbb82eb96f12b909f) )
	ROM_LOAD( "pr11024.8",    0x0200, 0x0100, CRC(08d71954) SHA1(df045dbfb3d669e4d42fbdba1e7191cd046f7b47) )
	ROM_LOAD( "pr5317.37",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wbmljo )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 256k for code + 256k for decrypted opcodes */
	ROM_LOAD( "epr11031.90",  0x00000, 0x8000, CRC(497ebfb4) SHA1(d90872c7d5285c85b05879bc67638f640e0339d5) ) /* encrypted */
	ROM_LOAD( "epr11032.91",  0x10000, 0x8000, CRC(9d03bdb2) SHA1(7dbab23e7c7972d9b51a0d3d046374720b7d6af5) ) /* encrypted */
	ROM_LOAD( "epr11033.92",  0x18000, 0x8000, CRC(7076905c) SHA1(562fbd9bd60851f7e4e60b725193395b4f193479) ) /* encrypted */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11037.126", 0x0000, 0x8000, CRC(7a4ee585) SHA1(050436106cced5dcbf40a3d94d48202eedddc3ad) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr11034.4",   0x00000, 0x8000, CRC(37a2077d) SHA1(57f032a98022bf03ff98cb2e563178ba97e4b63c) )
	ROM_LOAD( "epr11035.5",   0x08000, 0x8000, CRC(cdf2a21b) SHA1(db2553866f21e03bd9d668c179be3352adbaf8a6) )
	ROM_LOAD( "epr11036.6",   0x10000, 0x8000, CRC(644687fa) SHA1(d6c5bc95da4fc7e81091dcfe6205b6f47d54af76) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11028.87",  0x00000, 0x8000, CRC(af0b3972) SHA1(413825f66b84c7e45aa1855131482abead8f7f3b) )
	ROM_LOAD( "epr11027.86",  0x08000, 0x8000, CRC(277d8f1d) SHA1(7854673503ed03d276abe971805a11f8c992f6d6) )
	ROM_LOAD( "epr11030.89",  0x10000, 0x8000, CRC(f05ffc76) SHA1(f3dbb518240f86430840c3d4cda04bac79c20f69) )
	ROM_LOAD( "epr11029.88",  0x18000, 0x8000, CRC(cedc9c61) SHA1(dbe5744f9b6f2a406b52b910dd4e133db7bce6b2) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11026.20",   0x0000, 0x0100, CRC(27057298) SHA1(654be7abb937bb0720263ee6512e31194662effe) )
	ROM_LOAD( "pr11025.14",   0x0100, 0x0100, CRC(41e4d86b) SHA1(a86e8bb0a465d01b04410edfbb82eb96f12b909f) )
	ROM_LOAD( "pr11024.8",    0x0200, 0x0100, CRC(08d71954) SHA1(df045dbfb3d669e4d42fbdba1e7191cd046f7b47) )
	ROM_LOAD( "pr5317.37",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wbmljb )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 256k for code + 256k for decrypted opcodes */
	ROM_LOAD( "wbml.01",      0x20000, 0x8000, CRC(66482638) SHA1(887f93015f0effa2d0fa1f1f59082f75ac072221) ) /* Unencrypted opcodes */
	ROM_CONTINUE(             0x00000, 0x8000 )             /* Now load the operands in RAM */
	ROM_LOAD( "m-6.bin",      0x30000, 0x8000, CRC(8c08cd11) SHA1(5103f3c887c213b09aee858c4a883f2869b9ffb5) ) /* Unencrypted opcodes */
	ROM_CONTINUE(             0x10000, 0x8000 )
	ROM_LOAD( "m-7.bin",      0x38000, 0x8000, CRC(11881703) SHA1(b5e4d477158e7653b0fef5a4806be7b4871e917d) ) /* Unencrypted opcodes */
	ROM_CONTINUE(             0x18000, 0x8000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11037.126", 0x0000, 0x8000, CRC(7a4ee585) SHA1(050436106cced5dcbf40a3d94d48202eedddc3ad) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr11034.4",   0x00000, 0x8000, CRC(37a2077d) SHA1(57f032a98022bf03ff98cb2e563178ba97e4b63c) )
	ROM_LOAD( "epr11035.5",   0x08000, 0x8000, CRC(cdf2a21b) SHA1(db2553866f21e03bd9d668c179be3352adbaf8a6) )
	ROM_LOAD( "epr11036.6",   0x10000, 0x8000, CRC(644687fa) SHA1(d6c5bc95da4fc7e81091dcfe6205b6f47d54af76) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11028.87",  0x00000, 0x8000, CRC(af0b3972) SHA1(413825f66b84c7e45aa1855131482abead8f7f3b) )
	ROM_LOAD( "epr11027.86",  0x08000, 0x8000, CRC(277d8f1d) SHA1(7854673503ed03d276abe971805a11f8c992f6d6) )
	ROM_LOAD( "epr11030.89",  0x10000, 0x8000, CRC(f05ffc76) SHA1(f3dbb518240f86430840c3d4cda04bac79c20f69) )
	ROM_LOAD( "epr11029.88",  0x18000, 0x8000, CRC(cedc9c61) SHA1(dbe5744f9b6f2a406b52b910dd4e133db7bce6b2) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11026.20",   0x0000, 0x0100, CRC(27057298) SHA1(654be7abb937bb0720263ee6512e31194662effe) )
	ROM_LOAD( "pr11025.14",   0x0100, 0x0100, CRC(41e4d86b) SHA1(a86e8bb0a465d01b04410edfbb82eb96f12b909f) )
	ROM_LOAD( "pr11024.8",    0x0200, 0x0100, CRC(08d71954) SHA1(df045dbfb3d669e4d42fbdba1e7191cd046f7b47) )
	ROM_LOAD( "pr5317.37",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wbmlb )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 256k for code + 256k for decrypted opcodes */
	ROM_LOAD( "wbml.01",      0x20000, 0x8000, CRC(66482638) SHA1(887f93015f0effa2d0fa1f1f59082f75ac072221) ) /* Unencrypted opcodes */
	ROM_CONTINUE(             0x00000, 0x8000 )             /* Now load the operands in RAM */
	ROM_LOAD( "wbml.02",      0x30000, 0x8000, CRC(48746bb6) SHA1(a0049cba53e7548afa8d7b16a7e9494e628d2a0f) ) /* Unencrypted opcodes */
	ROM_CONTINUE(             0x10000, 0x8000 )
	ROM_LOAD( "wbml.03",      0x38000, 0x8000, CRC(d57ba8aa) SHA1(16f095cb78e31af5ce76d36c20fe4c3e0d027aea) ) /* Unencrypted opcodes */
	ROM_CONTINUE(             0x18000, 0x8000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11037.126", 0x0000, 0x8000, CRC(7a4ee585) SHA1(050436106cced5dcbf40a3d94d48202eedddc3ad) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "wbml.08",      0x00000, 0x8000, CRC(bbea6afe) SHA1(ba56c6789a35eb57cd226296ebf57e9aa19ba625) )
	ROM_LOAD( "wbml.09",      0x08000, 0x8000, CRC(77567d41) SHA1(2ac501661522615859f8a1718dbb8451272d6931) )
	ROM_LOAD( "wbml.10",      0x10000, 0x8000, CRC(a52ffbdd) SHA1(609375112268b770a798186697ecab5853f29f89) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11028.87",  0x00000, 0x8000, CRC(af0b3972) SHA1(413825f66b84c7e45aa1855131482abead8f7f3b) )
	ROM_LOAD( "epr11027.86",  0x08000, 0x8000, CRC(277d8f1d) SHA1(7854673503ed03d276abe971805a11f8c992f6d6) )
	ROM_LOAD( "epr11030.89",  0x10000, 0x8000, CRC(f05ffc76) SHA1(f3dbb518240f86430840c3d4cda04bac79c20f69) )
	ROM_LOAD( "epr11029.88",  0x18000, 0x8000, CRC(cedc9c61) SHA1(dbe5744f9b6f2a406b52b910dd4e133db7bce6b2) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11026.20",   0x0000, 0x0100, CRC(27057298) SHA1(654be7abb937bb0720263ee6512e31194662effe) )
	ROM_LOAD( "pr11025.14",   0x0100, 0x0100, CRC(41e4d86b) SHA1(a86e8bb0a465d01b04410edfbb82eb96f12b909f) )
	ROM_LOAD( "pr11024.8",    0x0200, 0x0100, CRC(08d71954) SHA1(df045dbfb3d669e4d42fbdba1e7191cd046f7b47) )
	ROM_LOAD( "pr5317.37",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( wbmlvc )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 256k for code + 256k for decrypted opcodes */
	ROM_LOAD( "vc.ic90",  0x20000, 0x8000, CRC(093c4852) SHA1(8dfbfe89c5b27b381fc54610e1e262a0e1f1ec59) ) /* Unencrypted opcodes */
	ROM_CONTINUE(         0x00000, 0x8000 )             /* Now load the operands in RAM */
	ROM_LOAD( "vc.ic91",  0x30000, 0x8000, CRC(7e973ece) SHA1(bd98287d376c4333313432f4ddab45dae9fdcd93) ) /* Unencrypted opcodes */
	ROM_CONTINUE(         0x10000, 0x8000 )
	ROM_LOAD( "vc.ic92",  0x38000, 0x8000, CRC(32661e7e) SHA1(5e06735b7dcc529b142bf6aa311d0e9f389daedd) ) /* Unencrypted opcodes */
	ROM_RELOAD(           0x18000, 0x8000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11037.126", 0x0000, 0x8000, CRC(7a4ee585) SHA1(050436106cced5dcbf40a3d94d48202eedddc3ad) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "vc.ic4",   0x00000, 0x8000, CRC(820bee59) SHA1(47afff58387eb67a8b0849d74023bd2c176a45e9) )
	ROM_LOAD( "vc.ic6",   0x08000, 0x8000, CRC(a9a1447e) SHA1(f7e55080c4fd6e1ff9e21a19b2f71dfd512d62c3) )
	ROM_LOAD( "vc.ic5",   0x10000, 0x8000, CRC(359026a0) SHA1(a20c801dbc758f172fcfc505a5083ddb76604243) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11028.87",  0x00000, 0x8000, CRC(af0b3972) SHA1(413825f66b84c7e45aa1855131482abead8f7f3b) )
	ROM_LOAD( "epr11027.86",  0x08000, 0x8000, CRC(277d8f1d) SHA1(7854673503ed03d276abe971805a11f8c992f6d6) )
	ROM_LOAD( "epr11030.89",  0x10000, 0x8000, CRC(f05ffc76) SHA1(f3dbb518240f86430840c3d4cda04bac79c20f69) )
	ROM_LOAD( "epr11029.88",  0x18000, 0x8000, CRC(cedc9c61) SHA1(dbe5744f9b6f2a406b52b910dd4e133db7bce6b2) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11026.20",   0x0000, 0x0100, CRC(27057298) SHA1(654be7abb937bb0720263ee6512e31194662effe) )
	ROM_LOAD( "pr11025.14",   0x0100, 0x0100, CRC(41e4d86b) SHA1(a86e8bb0a465d01b04410edfbb82eb96f12b909f) )
	ROM_LOAD( "pr11024.8",    0x0200, 0x0100, CRC(08d71954) SHA1(df045dbfb3d669e4d42fbdba1e7191cd046f7b47) )
	ROM_LOAD( "pr5317.37",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) )
ROM_END

ROM_START( dakkochn )
	ROM_REGION( 0x20000, REGION_CPU1, 0 ) /* 128k for code */
	ROM_LOAD( "epr11224.90",  0x00000, 0x8000, CRC(9fb1972b) SHA1(1bb61c6ec2b5b8eb39f74f20d5bcd0f14501bd21) ) /* encrypted */
	ROM_LOAD( "epr11225.91",  0x10000, 0x8000, CRC(c540f9e2) SHA1(dbda9355e8b796bcfaee2789714d248c4d7ad58c) ) /* encrypted */
	/* 18000-1ffff empty */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11229.126", 0x0000, 0x8000, CRC(c11648d0) SHA1(c2df3d767d497c3365ae70748c4790f4ee394958) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr11226.4",   0x00000, 0x8000, CRC(3dbc2f78) SHA1(f3f7ee2c0bedcc21c1c1f5394838af6d0a8833d8) )
	ROM_LOAD( "epr11227.5",   0x08000, 0x8000, CRC(34156e8d) SHA1(e23d8604a3d5db413cf150f9891fca2b1e0163fa) )
	ROM_LOAD( "epr11228.6",   0x10000, 0x8000, CRC(fdd5323f) SHA1(c47099c78207bb2258d34b98b48e3c04beb6407e) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11221.87",  0x00000, 0x8000, CRC(f9a44916) SHA1(9d9ba96146cff4c1ed18b7134ab19919e144d326) )
	ROM_LOAD( "epr11220.86",  0x08000, 0x8000, CRC(fdd25d8a) SHA1(53636e2a9102a2d86277cff812639d1d92587fc4) )
	ROM_LOAD( "epr11223.89",  0x10000, 0x8000, CRC(538adc55) SHA1(542af53a56f580e5ab455aa6bed955ee5fd4a252) )
	ROM_LOAD( "epr11222.88",  0x18000, 0x8000, CRC(33fab0b2) SHA1(eb3c08009315e46590c2c0df17fc3fa391034c66) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11219.20",   0x0000, 0x0100, CRC(45e252d9) SHA1(92d8f1d0f1a9e65234521ce02d512f08b5e06d78) ) /* palette red component */
	ROM_LOAD( "pr11218.14",   0x0100, 0x0100, CRC(3eda3a1b) SHA1(cc98c792521845259088eb163a150cd5bb603d5d) ) /* palette green component */
	ROM_LOAD( "pr11217.8",    0x0200, 0x0100, CRC(49dbde88) SHA1(7057da5617de7e4775adf092cce1709135066129) ) /* palette blue component */
	ROM_LOAD( "pr5317.37",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( ufosensi )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "epr11661.90",  0x00000, 0x8000, CRC(f3e394e2) SHA1(a295a2aa80a164a548995822c46f32fd9fad7a0b) ) /* encrypted */
	ROM_LOAD( "epr11662.91",  0x10000, 0x8000, CRC(0c2e4120) SHA1(d81fbefa95868e3efd29ef3bacf108329781ca17) ) /* encrypted */
	ROM_LOAD( "epr11663.92",  0x18000, 0x8000, CRC(4515ebae) SHA1(9b823f10999746292762c2f0a1ca9039efa22506) ) /* encrypted */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11667.126", 0x0000, 0x8000, CRC(110baba9) SHA1(e14cf5af11ac9691eca897bbae7c238665cd2a4d) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr11664.4",   0x00000, 0x8000, CRC(1b1bc3d5) SHA1(2a09e0dbe2d467c151dce705f249367df849eaeb) )
	ROM_LOAD( "epr11665.5",   0x08000, 0x8000, CRC(3659174a) SHA1(176d2436abb45827a8d387241082854f55dc0314) )
	ROM_LOAD( "epr11666.6",   0x10000, 0x8000, CRC(99dcc793) SHA1(ad1d0acb60e7c1a7016955e142ebca1cf07b4908) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11658.87",  0x00000, 0x8000, CRC(3b5a20f7) SHA1(03e0934b0913c3a2cadf1d28b8a700d70b80fbac) )
	ROM_LOAD( "epr11657.86",  0x08000, 0x8000, CRC(010f81a9) SHA1(1b7ee05c80edfa403e32c216fa69387ca556895e) )
	ROM_LOAD( "epr11660.89",  0x10000, 0x8000, CRC(e1e2e7c5) SHA1(434039a70049a6e74e2a2f48b60345f720e6b1af) )
	ROM_LOAD( "epr11659.88",  0x18000, 0x8000, CRC(286c7286) SHA1(449a19ea9a9f9df47005e8dac1b8eacaebc515e7) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11656.20",   0x0000, 0x0100, CRC(640740eb) SHA1(9a601a3665f612d00c70019d33c7abd3cca9434b) ) /* palette red component */
	ROM_LOAD( "pr11655.14",   0x0100, 0x0100, CRC(a0c3fa77) SHA1(cdffa1de06d30ec421323145dfc3271803fc25d4) ) /* palette green component */
	ROM_LOAD( "pr11654.8",    0x0200, 0x0100, CRC(ba624305) SHA1(eb1d0dde60f81ff510ac8c1212e0ed5703febaf3) ) /* palette blue component */
	ROM_LOAD( "pr5317.28",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( ufosensb )
	ROM_REGION( 2*0x20000, REGION_CPU1, 0 ) /* 256k for code + 256k for decrypted opcodes */
	ROM_LOAD( "k108.bin",     0x20000, 0x8000, CRC(6b1d0955) SHA1(dbda145d40eaecd30c1d55a9675c58a2967c20c4) )
	ROM_CONTINUE(             0x00000, 0x8000 )             /* Now load the operands in RAM */
	ROM_LOAD( "k109.bin",     0x30000, 0x8000, CRC(fc543b26) SHA1(b9e1d2ca6f9811bf341edf104fe209dbf56e4b2d) )
	ROM_CONTINUE(             0x10000, 0x8000 )
	ROM_LOAD( "k110.bin",     0x38000, 0x8000, CRC(6ba2dc77) SHA1(09a65f55988ae28e285d402af9a2a1f1dc05a82c) )
	ROM_CONTINUE(             0x18000, 0x8000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* 64k for sound cpu */
	ROM_LOAD( "epr11667.126", 0x0000, 0x8000, CRC(110baba9) SHA1(e14cf5af11ac9691eca897bbae7c238665cd2a4d) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "epr11664.4",   0x00000, 0x8000, CRC(1b1bc3d5) SHA1(2a09e0dbe2d467c151dce705f249367df849eaeb) )
	ROM_LOAD( "epr11665.5",   0x08000, 0x8000, CRC(3659174a) SHA1(176d2436abb45827a8d387241082854f55dc0314) )
	ROM_LOAD( "epr11666.6",   0x10000, 0x8000, CRC(99dcc793) SHA1(ad1d0acb60e7c1a7016955e142ebca1cf07b4908) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "epr11658.87",  0x00000, 0x8000, CRC(3b5a20f7) SHA1(03e0934b0913c3a2cadf1d28b8a700d70b80fbac) )
	ROM_LOAD( "epr11657.86",  0x08000, 0x8000, CRC(010f81a9) SHA1(1b7ee05c80edfa403e32c216fa69387ca556895e) )
	ROM_LOAD( "epr11660.89",  0x10000, 0x8000, CRC(e1e2e7c5) SHA1(434039a70049a6e74e2a2f48b60345f720e6b1af) )
	ROM_LOAD( "epr11659.88",  0x18000, 0x8000, CRC(286c7286) SHA1(449a19ea9a9f9df47005e8dac1b8eacaebc515e7) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "pr11656.20",   0x0000, 0x0100, CRC(640740eb) SHA1(9a601a3665f612d00c70019d33c7abd3cca9434b) ) /* palette red component */
	ROM_LOAD( "pr11655.14",   0x0100, 0x0100, CRC(a0c3fa77) SHA1(cdffa1de06d30ec421323145dfc3271803fc25d4) ) /* palette green component */
	ROM_LOAD( "pr11654.8",    0x0200, 0x0100, CRC(ba624305) SHA1(eb1d0dde60f81ff510ac8c1212e0ed5703febaf3) ) /* palette blue component */
	ROM_LOAD( "pr5317.28",    0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

ROM_START( noboranb )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "nobo-t.bin", 0x00000, 0x8000, CRC(176fd168) SHA1(f262521f07e5340f175019e2a06a54120a4aa3b7) )
	ROM_LOAD( "nobo-r.bin", 0x10000, 0x8000, CRC(d61cf3c9) SHA1(0f80011d713c51e67853810813ebba579ade0303) )
	ROM_LOAD( "nobo-s.bin", 0x18000, 0x8000, CRC(b0e7697f) SHA1(ad5394ca629152a8c73fb85d3fce8ea620ae6ff1) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "nobo-m.bin", 0x0000, 0x4000, CRC(415adf76) SHA1(fbd6f8921aa3246702983ba81fa9ae53fa10c19d) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nobo-j.bin", 0x08000, 0x8000, CRC(f12df039) SHA1(159de205f77fd74da30717054e6ddda2c0bb63d0) )
	ROM_LOAD( "nobo-k.bin", 0x00000, 0x8000, CRC(446fbcdd) SHA1(e3c8364eccfa6c8af7a57b599238b0e4ebe8cc59) )
	ROM_LOAD( "nobo-l.bin", 0x10000, 0x8000, CRC(35f396df) SHA1(ebf0a252513ae2b31ef012ac71d64fb20b8725cc) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* 128k for sprites data */
	ROM_LOAD( "nobo-q.bin", 0x00000, 0x8000, CRC(2442b86d) SHA1(2eed80e1ff9cd782990142d0d73ca4fa13db4731) )
	ROM_LOAD( "nobo-o.bin", 0x08000, 0x8000, CRC(e33743a6) SHA1(56dce565523f19e673c9272992030386ca648e41) )
	ROM_LOAD( "nobo-p.bin", 0x10000, 0x8000, CRC(7fbba01d) SHA1(ded22806ae0d6642b45cd33c0ceab67390a6e319) )
	ROM_LOAD( "nobo-n.bin", 0x18000, 0x8000, CRC(85e7a29f) SHA1(0ca77c66599650f157450d703682ec114f0453cf) )

	ROM_REGION( 0x0400, REGION_PROMS, 0 )
	ROM_LOAD( "nobo_pr.16d", 0x0000, 0x0100, CRC(95010ac2) SHA1(deaf84b408cd1f3396eb851ef04cc1654d5e9a46) ) /* palette red component */
	ROM_LOAD( "nobo_pr.15d", 0x0100, 0x0100, CRC(c55aac0c) SHA1(0f7f2d383a90e9f7f319626b4d5565805f44a1f9) ) /* palette green component */
	ROM_LOAD( "nobo_pr.14d", 0x0200, 0x0100, CRC(de394cee) SHA1(511c53f22459e5e238b48685f85b10f5e15f2ac1) ) /* palette blue component */
	ROM_LOAD( "nobo_pr.13a", 0x0300, 0x0100, CRC(648350b8) SHA1(c7986aa9127ef5b50b845434cb4e81dff9861cd2) ) /* timing? (not used) */
ROM_END

static DRIVER_INIT( regulus )	{ regulus_decode(); }
static DRIVER_INIT( mrviking )	{ mrviking_decode(); }
static DRIVER_INIT( swat )		{ swat_decode(); }
static DRIVER_INIT( flicky )	{ flicky_decode(); }
static DRIVER_INIT( wmatch )	{ wmatch_decode(); }
static DRIVER_INIT( bullfgtj )	{ bullfgtj_decode(); }
static DRIVER_INIT( spatter )	{ spatter_decode(); }
static DRIVER_INIT( pitfall2 )	{ pitfall2_decode(); }
static DRIVER_INIT( nprinces )	{ nprinces_decode(); }
static DRIVER_INIT( seganinj )	{ seganinj_decode(); }
static DRIVER_INIT( imsorry )	{ imsorry_decode(); }
static DRIVER_INIT( teddybb )	{ teddybb_decode(); }
static DRIVER_INIT( hvymetal )	{ hvymetal_decode(); }
static DRIVER_INIT( myheroj )	{ myheroj_decode(); }
static DRIVER_INIT( 4dwarrio )	{ fdwarrio_decode(); }
static DRIVER_INIT( wboy )		{ astrofl_decode(); }
static DRIVER_INIT( wboy2 )		{ wboy2_decode(); }
static DRIVER_INIT( gardia )	{ gardia_decode(); }
static DRIVER_INIT( gardiab )	{ gardiab_decode(); }

void mc8123_decrypt_0043(void);
void mc8123_decrypt_0064(void);
static DRIVER_INIT( ufosensi )  { mc8123_decrypt_0064(); }
static DRIVER_INIT( wbml )  { mc8123_decrypt_0043(); }


DRIVER_INIT( myherok )
{
	int A;
	unsigned char *rom;

	/* additionally to the usual protection, all the program ROMs have data lines */
	/* D0 and D1 swapped. */
	rom = memory_region(REGION_CPU1);
	for (A = 0;A < 0xc000;A++)
		rom[A] = (rom[A] & 0xfc) | ((rom[A] & 1) << 1) | ((rom[A] & 2) >> 1);

	/* the tile gfx ROMs are mangled as well: */
	rom = memory_region(REGION_GFX1);

	/* the first ROM has data lines D0 and D6 swapped. */
	for (A = 0x0000;A < 0x4000;A++)
		rom[A] = (rom[A] & 0xbe) | ((rom[A] & 0x01) << 6) | ((rom[A] & 0x40) >> 6);

	/* the second ROM has data lines D1 and D5 swapped. */
	for (A = 0x4000;A < 0x8000;A++)
		rom[A] = (rom[A] & 0xdd) | ((rom[A] & 0x02) << 4) | ((rom[A] & 0x20) >> 4);

	/* the third ROM has data lines D0 and D6 swapped. */
	for (A = 0x8000;A < 0xc000;A++)
		rom[A] = (rom[A] & 0xbe) | ((rom[A] & 0x01) << 6) | ((rom[A] & 0x40) >> 6);

	/* also, all three ROMs have address lines A4 and A5 swapped. */
	for (A = 0;A < 0xc000;A++)
	{
		int A1;
		unsigned char temp;

		A1 = (A & 0xffcf) | ((A & 0x0010) << 1) | ((A & 0x0020) >> 1);
		if (A < A1)
		{
			temp = rom[A];
			rom[A] = rom[A1];
			rom[A1] = temp;
		}
	}

	myheroj_decode();
}

static DRIVER_INIT( bootleg )
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	memory_set_opcode_base(0,rom+diff);
}

static DRIVER_INIT( noboranb )
{
	/* Patch to get PRG ROMS ('T', 'R' and 'S) status as "GOOD" in the "test mode" */
	/* not really needed */

/*	data8_t *ROM = memory_region(REGION_CPU1);*/

/*	ROM[0x3296] = 0x18;		*/ /* 'jr' instead of 'jr z' - 'T' (PRG Main ROM)*/
/*	ROM[0x32be] = 0x18;		*/ /* 'jr' instead of 'jr z' - 'R' (Banked ROM 1)*/
/*	ROM[0x32ea] = 0x18;		*/ /* 'jr' instead of 'jr z' - 'S' (Banked ROM 2)*/

	/* Patch to avoid the internal checksum that will hang the game after an amount of time
	   (check code at 0x3313 in 'R' (banked ROM 1)) */

/*	ROM[0x10000 + 0 * 0x8000 + 0x3347] = 0x18;	*/ /* 'jr' instead of 'jr z'*/

	/* Patch to get sound in later levels(the program enters into a tight loop)*/
	data8_t *ROM2 = memory_region(REGION_CPU2);

	ROM2[0x02f9] = 0x28;/*'jr z' instead of 'jr'*/
}





GAME( 1983, starjack, 0,        small,    starjack, 0,        ROT270, "Sega", 			 	   "Star Jacker (Sega)" )
GAME( 1983, starjacs, starjack, small,    starjacs, 0,        ROT270, "Stern",			  	   "Star Jacker (Stern)" )
GAME( 1983, regulus,  0,        small,	  regulus,  regulus,  ROT270, "Sega", 			 	   "Regulus (New Ver.)" )
GAME( 1983, reguluso, regulus,  small,	  reguluso, regulus,  ROT270, "Sega", 			 	   "Regulus (Old Ver.)" )
GAME( 1983, regulusu, regulus,  small,	  regulus,  0,        ROT270, "Sega", 			 	   "Regulus (not encrypted)" )
GAME( 1983, upndown,  0,        system1,  upndown,  nprinces, ROT270, "Sega", 			 	   "Up'n Down" )
GAME( 1983, upndownu, upndown,  system1,  upndown,  0,        ROT270, "Sega", 			 	   "Up'n Down (not encrypted)" )
GAME( 1984, mrviking, 0,        small,    mrviking, mrviking, ROT270, "Sega", 			 	   "Mister Viking" )
GAME( 1984, mrvikngj, mrviking, small,    mrvikngj, mrviking, ROT270, "Sega", 			 	   "Mister Viking (Japan)" )
GAME( 1984, swat,     0,        system1,  swat,     swat,     ROT270, "Coreland / Sega", 	   "SWAT" )
GAME( 1984, flicky,   0,        system1,  flicky,   flicky,   ROT0,   "Sega", 			 	   "Flicky (128k Ver.)" )
GAME( 1984, flickyo,  flicky,   system1,  flicky,   flicky,   ROT0,   "Sega", 			 	   "Flicky (64k Ver.)" )
GAME( 1984, wmatch,   0,        small, 	  wmatch,   wmatch,   ROT270, "Sega", 			 	   "Water Match" )
GAME( 1984, bullfgt,  0,        system1,  bullfgt,  bullfgtj, ROT0,   "Coreland / Sega", 	   "Bullfight" )
GAME( 1984, thetogyu, bullfgt,  system1,  bullfgt,  bullfgtj, ROT0,   "Coreland / Sega", 	   "The Togyu (Japan)" )
GAME( 1984, spatter,  0,        small,    spatter,  spatter,  ROT0,   "Sega", 			 	   "Spatter" )
GAME( 1984, ssanchan, spatter,  small,    spatter,  spatter,  ROT0,   "Sega", 			 	   "Sanrin San Chan (Japan)" )
GAME( 1985, pitfall2, 0,        pitfall2, pitfall2, pitfall2, ROT0,   "Sega", 			 	   "Pitfall II" )
GAME( 1985, pitfallu, pitfall2, pitfall2, pitfallu, 0,        ROT0,   "Sega", 			 	   "Pitfall II (not encrypted)" )
GAME( 1985, seganinj, 0,        system1,  seganinj, seganinj, ROT0,   "Sega", 			 	   "Sega Ninja" )
GAME( 1985, seganinu, seganinj, system1,  seganinj, 0,        ROT0,   "Sega", 			 	   "Sega Ninja (not encrypted)" )
GAME( 1985, nprinces, seganinj, system1,  seganinj, flicky,   ROT0,   "bootleg?", 		 	   "Ninja Princess (64k Ver. bootleg[Q])" )
GAME( 1985, nprincso, seganinj, system1,  seganinj, nprinces, ROT0,   "Sega", 			 	   "Ninja Princess (128k Ver.)" )
GAME( 1985, nprincsu, seganinj, system1,  seganinj, 0,        ROT0,   "Sega", 			 	   "Ninja Princess (64k Ver. not encrypted)" )
GAME( 1985, nprincsb, seganinj, system1,  seganinj, flicky,   ROT0,   "bootleg?", 		 	   "Ninja Princess (128k Ver. bootleg[Q])" )
GAME( 1985, imsorry,  0,        system1,  imsorry,  imsorry,  ROT0,   "Coreland / Sega", 	   "I'm Sorry (US)" )
GAME( 1985, imsorryj, imsorry,  system1,  imsorry,  imsorry,  ROT0,   "Coreland / Sega", 	   "Gonbee no I'm Sorry (Japan)" )
GAME( 1985, teddybb,  0,        system1,  teddybb,  teddybb,  ROT0,   "Sega", 			 	   "TeddyBoy Blues (New Ver.)" )
GAME( 1985, teddybbo, teddybb,  system1,  teddybb,  teddybb,  ROT0,   "Sega", 			 	   "TeddyBoy Blues (Old Ver.)" )
GAME( 1985, hvymetal, 0,        hvymetal, hvymetal, hvymetal, ROT0,   "Sega", 			 	   "Heavy Metal" )
GAME( 1985, myhero,   0,        system1,  myhero,   0,        ROT0,   "Sega", 			 	   "My Hero (US)" )
GAME( 1985, sscandal, myhero,   system1,  myhero,   myheroj,  ROT0,   "Coreland / Sega", 	   "Seishun Scandal (Japan)" )
GAME( 1985, myherok,  myhero,   system1,  myhero,   myherok,  ROT0,   "Coreland / Sega", 	   "My Hero (Korea)" )
GAMEX(1985, shtngmst, 0,        shtngmst, shtngmst, 0,        ROT0,   "Sega", 			 	   "Shooting Master", GAME_IMPERFECT_GRAPHICS )	/* 8751 protection handled via hack */
GAMEX(1985, chplft,   0,        chplft,   chplft,   0,        ROT0,   "Sega", 			 	   "Choplifter", GAME_UNEMULATED_PROTECTION )	/* 8751 protection */
GAME( 1985, chplftb,  chplft,   chplft,   chplft,   0,        ROT0,   "Sega", 			 	   "Choplifter (alternate)" )
GAME( 1985, chplftbl, chplft,   chplft,   chplft,   0,        ROT0,   "bootleg", 		 	   "Choplifter (bootleg)" )
GAME( 1985, 4dwarrio, 0,        system1,  4dwarrio, 4dwarrio, ROT0,   "Coreland / Sega", 	   "4-D Warriors" )
GAME( 1986, brain,    0,        brain,    brain,    0,        ROT0,   "Coreland / Sega", 	   "Brain" )
GAME( 1986, raflesia, 0,        system1,  raflesia, 4dwarrio, ROT270, "Coreland / Sega", 	   "Rafflesia" )
GAME( 1986, wboy,     0,        system1,  wboy,     wboy,     ROT0,   "Sega (Escape license)", "Wonder Boy (set 1, new encryption)" )
GAME( 1986, wboyo,    wboy,     system1,  wboy,     hvymetal, ROT0,   "Sega (Escape license)", "Wonder Boy (set 1, old encryption)" )
GAME( 1986, wboy2,    wboy,     system1,  wboy,     wboy2,    ROT0,   "Sega (Escape license)", "Wonder Boy (set 2)" )
GAME( 1986, wboy2u,   wboy,     system1,  wboy,     0,        ROT0,   "Sega (Escape license)", "Wonder Boy (set 2 not encrypted)" )
GAME( 1986, wboy3,    wboy,     system1,  wboy,     hvymetal, ROT0,   "Sega (Escape license)", "Wonder Boy (set 3)" )
GAME( 1986, wboyu,    wboy,     system1,  wboyu,    0,        ROT0,   "Sega (Escape license)", "Wonder Boy (not encrypted)" )
GAME( 1986, wbdeluxe, wboy,     system1,  wbdeluxe, 0,        ROT0,   "Sega (Escape license)", "Wonder Boy Deluxe" )
GAMEX(1986, gardia,   0,        brain,    gardia,   gardia,   ROT270, "Sega / Coreland", 	   "Gardia", GAME_NO_COCKTAIL )
GAMEX(1986, gardiab,  gardia,   brain,    gardia,   gardiab,  ROT270, "bootleg", 		 	   "Gardia (bootleg)", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAME( 1986, noboranb, 0,        noboranb, noboranb, noboranb, ROT270, "bootleg", 		 	   "Noboranka (Japan)" )
GAMEX(1987, blockgal, 0,        blockgal, blockgal, 0,        ROT90,  "Sega / Vic Tokai",	   "Block Gal", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX(1987, blckgalb, blockgal, blockgal, blockgal, bootleg,  ROT90,  "bootleg", 		 	   "Block Gal (bootleg)", GAME_NO_COCKTAIL )
GAMEX(1987, tokisens, 0,        wbml,     tokisens, 0,        ROT90,  "Sega", 			 	   "Toki no Senshi - Chrono Soldier", GAME_NO_COCKTAIL )
GAMEX(1987, wbml,     0,        wbml,     wbml,     wbml,     ROT0,   "Sega / Westone",  "Wonder Boy in Monster Land (Japan New Ver.)", GAME_NO_COCKTAIL )
GAMEX(1987, wbmljo,   wbml,     wbml,     wbml,     wbml,     ROT0,   "Sega / Westone",  "Wonder Boy in Monster Land (Japan Old Ver.)", GAME_NO_COCKTAIL )
GAMEX(1987, wbmljb,   wbml,     wbml,     wbml,     bootleg,  ROT0,   "bootleg", 		     "Wonder Boy in Monster Land (Japan not encrypted)", GAME_NO_COCKTAIL )
GAMEX(1987, wbmlb,    wbml,     wbml,     wbml,     bootleg,  ROT0,   "bootleg", 		     "Wonder Boy in Monster Land", GAME_NO_COCKTAIL )
GAMEX(2009, wbmlvc,   wbml,     wbml,     wbml,     bootleg,  ROT0,   "Sega / Westone",  "Wonder Boy in Monster Land (English, Virtual Console)", GAME_NO_COCKTAIL )
GAMEX(1987, dakkochn, 0,        chplft,   chplft,   0,        ROT0,   "Sega", 			 	   "DakkoChan Jansoh", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX(1988, ufosensi, 0,        ufosensi, ufosensi, ufosensi, ROT0,   "Sega", 			 	   "Ufo Senshi Yohko Chan", GAME_NO_COCKTAIL )
GAMEX(1988, ufosensb, ufosensi, ufosensi, ufosensi, bootleg,  ROT0,   "bootleg", 			   "Ufo Senshi Yohko Chan (not encrypted)", GAME_NO_COCKTAIL )
