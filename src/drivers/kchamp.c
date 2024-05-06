/***************************************************************************

Karate Champ - (c) 1984 Data East

driver by Ernesto Corvi


Changes:
(1999/11/11 Takahiro Nogi)
Changed "karatedo" to "karatevs".
Supported "karatedo".
Corrected DIPSW settings.(kchamp/karatedo)

Currently supported sets:
Karate Champ - 1 Player Version (kchamp)
Karate Champ - VS Version (kchampvs)
Karate Champ - 1 Player Version Japanese (karatedo)
Karate Champ - VS Version Japanese (karatevs)

VS Version Info:
---------------
Memory Map:
Main CPU
0000-bfff ROM (encrypted)
c000-cfff RAM
d000-d3ff char videoram
d400-d7ff color videoram
d800-d8ff sprites
e000-ffff ROM (encrypted)

Sound CPU
0000-5fff ROM
6000-6300 RAM

IO Ports:
Main CPU
INPUT  00 = Player 1 Controls - ( ACTIVE LOW )
INPUT  40 = Player 2 Controls - ( ACTIVE LOW )
INPUT  80 = Coins and Start Buttons - ( ACTIVE LOW )
INPUT  C0 = Dip Switches - ( ACTIVE LOW )
OUTPUT 00 = Screen Flip
OUTPUT 01 = CPU Control
                bit 0 = external nmi enable
OUTPUT 02 = Sound Reset
OUTPUT 40 = Sound latch write

Sound CPU
INPUT  01 = Sound latch read
OUTPUT 00 = AY8910 #1 data write
OUTPUT 01 = AY8910 #1 control write
OUTPUT 02 = AY8910 #2 data write
OUTPUT 03 = AY8910 #2 control write
OUTPUT 04 = MSM5205 write
OUTPUT 05 = CPU Control
                bit 0 = MSM5205 trigger
                bit 1 = external nmi enable

1P Version Info:
---------------
Same as VS version but with a DAC instead of a MSM5205. Also some minor
IO ports and memory map changes. Dip switches differ too.

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


/* from vidhrdw */
extern WRITE_HANDLER( kchamp_videoram_w );
extern WRITE_HANDLER( kchamp_colorram_w );
extern WRITE_HANDLER( kchamp_flipscreen_w );

extern PALETTE_INIT( kchamp );
extern VIDEO_START( kchamp );
extern VIDEO_UPDATE( kchamp );
extern VIDEO_UPDATE( kchampvs );


static int nmi_enable = 0;
static int sound_nmi_enable = 0;

static MEMORY_READ_START( readmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, MRA_RAM },
	{ 0xd000, 0xd3ff, MRA_RAM },
	{ 0xd400, 0xd7ff, MRA_RAM },
	{ 0xd800, 0xd8ff, MRA_RAM },
	{ 0xd900, 0xdfff, MRA_RAM },
	{ 0xe000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xd3ff, kchamp_videoram_w, &videoram },
	{ 0xd400, 0xd7ff, kchamp_colorram_w, &colorram },
	{ 0xd800, 0xd8ff, spriteram_w, &spriteram, &spriteram_size },
	{ 0xd900, 0xdfff, MWA_RAM },
	{ 0xe000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0xffff, MWA_RAM },
MEMORY_END

static WRITE_HANDLER( control_w ) {
	nmi_enable = data & 1;
}

static WRITE_HANDLER( sound_reset_w ) {
	if ( !( data & 1 ) )
		cpu_set_reset_line(1,PULSE_LINE);
}

static WRITE_HANDLER( sound_control_w ) {
	MSM5205_reset_w( 0, !( data & 1 ) );
	sound_nmi_enable = ( ( data >> 1 ) & 1 );
}

static WRITE_HANDLER( sound_command_w ) {
	soundlatch_w( 0, data );
	cpu_set_irq_line_and_vector( 1, 0, HOLD_LINE, 0xff );
}

static int msm_data = 0;
static int msm_play_lo_nibble = 1;

static WRITE_HANDLER( sound_msm_w ) {
	msm_data = data;
	msm_play_lo_nibble = 1;
}

static PORT_READ_START( readport )
	{ 0x00, 0x00, input_port_0_r }, /* Player 1 controls - ACTIVE LOW */
	{ 0x40, 0x40, input_port_1_r }, /* Player 2 controls - ACTIVE LOW */
	{ 0x80, 0x80, input_port_2_r }, /* Coins & Start - ACTIVE LOW */
	{ 0xC0, 0xC0, input_port_3_r }, /* Dipswitch */
PORT_END

static PORT_WRITE_START( writeport )
	{ 0x00, 0x00, kchamp_flipscreen_w },
	{ 0x01, 0x01, control_w },
	{ 0x02, 0x02, sound_reset_w },
	{ 0x40, 0x40, sound_command_w },
PORT_END

static PORT_READ_START( sound_readport )
	{ 0x01, 0x01, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_writeport )
	{ 0x00, 0x00, AY8910_write_port_0_w },
	{ 0x01, 0x01, AY8910_control_port_0_w },
	{ 0x02, 0x02, AY8910_write_port_1_w },
	{ 0x03, 0x03, AY8910_control_port_1_w },
	{ 0x04, 0x04, sound_msm_w },
	{ 0x05, 0x05, sound_control_w },
PORT_END

/********************
* 1 Player Version  *
********************/

static MEMORY_READ_START( kc_readmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe3ff, MRA_RAM },
	{ 0xe400, 0xe7ff, MRA_RAM },
	{ 0xea00, 0xeaff, MRA_RAM },
	{ 0xeb00, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( kc_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe3ff, kchamp_videoram_w, &videoram },
	{ 0xe400, 0xe7ff, kchamp_colorram_w, &colorram },
	{ 0xea00, 0xeaff, spriteram_w, &spriteram, &spriteram_size },
	{ 0xeb00, 0xffff, MWA_RAM },
MEMORY_END

static MEMORY_READ_START( kc_sound_readmem )
	{ 0x0000, 0xdfff, MRA_ROM },
	{ 0xe000, 0xe2ff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( kc_sound_writemem )
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe2ff, MWA_RAM },
MEMORY_END

static READ_HANDLER( sound_reset_r ) {
	cpu_set_reset_line(1,PULSE_LINE);
	return 0;
}

static WRITE_HANDLER( kc_sound_control_w ) {
	if ( offset == 0 )
		sound_nmi_enable = ( ( data >> 7 ) & 1 );
/*	else*/
/*		DAC_set_volume(0,( data == 1 ) ? 255 : 0,0);*/
}

static PORT_READ_START( kc_readport )
	{ 0x90, 0x90, input_port_0_r }, /* Player 1 controls - ACTIVE LOW */
	{ 0x98, 0x98, input_port_1_r }, /* Player 2 controls - ACTIVE LOW */
	{ 0xa0, 0xa0, input_port_2_r }, /* Coins & Start - ACTIVE LOW */
	{ 0x80, 0x80, input_port_3_r }, /* Dipswitch */
	{ 0xa8, 0xa8, sound_reset_r },
PORT_END

static PORT_WRITE_START( kc_writeport )
	{ 0x80, 0x80, kchamp_flipscreen_w },
	{ 0x81, 0x81, control_w },
	{ 0xa8, 0xa8, sound_command_w },
PORT_END

static PORT_READ_START( kc_sound_readport )
	{ 0x06, 0x06, soundlatch_r },
PORT_END

static PORT_WRITE_START( kc_sound_writeport )
	{ 0x00, 0x00, AY8910_write_port_0_w },
	{ 0x01, 0x01, AY8910_control_port_0_w },
	{ 0x02, 0x02, AY8910_write_port_1_w },
	{ 0x03, 0x03, AY8910_control_port_1_w },
	{ 0x04, 0x04, DAC_0_data_w },
	{ 0x05, 0x05, kc_sound_control_w },
PORT_END


INPUT_PORTS_START( kchampvs )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_4WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER2 | IPF_4WAY )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Easy" )
	PORT_DIPSETTING(    0x20, "Medium" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


/********************
* 1 Player Version  *
********************/

INPUT_PORTS_START( kchamp )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_4WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_PLAYER2 | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER2 | IPF_4WAY )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )
INPUT_PORTS_END

static struct GfxLayout tilelayout =
{
	8,8,	/* tile size */
	256*8,	/* number of tiles */
	2,	/* bits per pixel */
	{ 0x4000*8, 0 }, /* plane offsets */
	{ 0,1,2,3,4,5,6,7 }, /* x offsets */
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 }, /* y offsets */
	8*8	/* offset to next tile */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* tile size */
	512,	/* number of tiles */
	2,	/* bits per pixel */
	{ 0xC000*8, 0 }, /* plane offsets */
	{ 0,1,2,3,4,5,6,7,
		0x2000*8+0,0x2000*8+1,0x2000*8+2,0x2000*8+3,
		0x2000*8+4,0x2000*8+5,0x2000*8+6,0x2000*8+7 }, /* x offsets */
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
	8*8,9*8,10*8,11*8,12*8,13*8,14*8, 15*8 }, /* y offsets */
	16*8	/* ofset to next tile */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &tilelayout,	32*4, 32 },
	{ REGION_GFX2, 0x08000, &spritelayout,	0, 16 },
	{ REGION_GFX2, 0x04000, &spritelayout,	0, 16 },
	{ REGION_GFX2, 0x00000, &spritelayout,	0, 16 },
	{ -1 }
};



static INTERRUPT_GEN( kc_interrupt ) {

	if ( nmi_enable )
		cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
}

static void msmint( int data ) {

	static int counter = 0;

	if ( msm_play_lo_nibble )
		MSM5205_data_w( 0, msm_data & 0x0f );
	else
		MSM5205_data_w( 0, ( msm_data >> 4 ) & 0x0f );

	msm_play_lo_nibble ^= 1;

	if ( !( counter ^= 1 ) ) {
		if ( sound_nmi_enable ) {
			cpu_set_irq_line( 1, IRQ_LINE_NMI, PULSE_LINE );
		}
	}
}

static struct AY8910interface ay8910_interface =
{
	2, /* 2 chips */
	1500000,			/* 12 MHz / 8 = 1.5 MHz */
	{ 30, 30 },			/* Modified by T.Nogi 1999/11/08*/
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MSM5205interface msm_interface =
{
	1,				/* 1 chip */
	375000,				/* 12MHz / 16 / 2 */
	{ msmint },			/* interrupt function */
	{ MSM5205_S96_4B },		/* 1 / 96 = 3906.25Hz playback */
	{ 100 }
};

/********************
* 1 Player Version  *
********************/

static INTERRUPT_GEN( sound_int ) {

	if ( sound_nmi_enable )
		cpu_set_irq_line(1, IRQ_LINE_NMI, PULSE_LINE);
}

static struct DACinterface dac_interface =
{
	1,
	{ 15 }
};


static MACHINE_DRIVER_START( kchampvs )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3000000)	/* 12MHz / 4 = 3.0 MHz */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(kc_interrupt,1)

	MDRV_CPU_ADD(Z80, 3000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 12MHz / 4 = 3.0 MHz */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)
			/* irq's triggered from main cpu */
			/* nmi's from msm5205 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(256)

	MDRV_PALETTE_INIT(kchamp)
	MDRV_VIDEO_START(kchamp)
	MDRV_VIDEO_UPDATE(kchampvs)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
	MDRV_SOUND_ADD(MSM5205, msm_interface)
MACHINE_DRIVER_END

/********************
* 1 Player Version  *
********************/

static MACHINE_DRIVER_START( kchamp )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3000000)	/* 12MHz / 4 = 3.0 MHz */
	MDRV_CPU_MEMORY(kc_readmem,kc_writemem)
	MDRV_CPU_PORTS(kc_readport,kc_writeport)
	MDRV_CPU_VBLANK_INT(kc_interrupt,1)

	MDRV_CPU_ADD(Z80, 3000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 12MHz / 4 = 3.0 MHz */
	MDRV_CPU_MEMORY(kc_sound_readmem,kc_sound_writemem)
	MDRV_CPU_PORTS(kc_sound_readport,kc_sound_writeport)
	MDRV_CPU_PERIODIC_INT(sound_int, 125) /* Hz */
			/* irq's triggered from main cpu */
			/* nmi's from 125 Hz clock */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(256)

	MDRV_PALETTE_INIT(kchamp)
	MDRV_VIDEO_START(kchamp)
	MDRV_VIDEO_UPDATE(kchamp)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( kchamp )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "b014.bin", 0x0000, 0x2000, CRC(0000d1a0) SHA1(0c584096825e1d3cc718e0bda1abb897a7f4d2df) )
	ROM_LOAD( "b015.bin", 0x2000, 0x2000, CRC(03fae67e) SHA1(3b6a30f39f5ad512415e3b8ba6e07f3118e28d9e) )
	ROM_LOAD( "b016.bin", 0x4000, 0x2000, CRC(3b6e1d08) SHA1(ecf7d2b0f31f04c0be7d5a1f450b9c95d9f54d80) )
	ROM_LOAD( "b017.bin", 0x6000, 0x2000, CRC(c1848d1a) SHA1(eb5f85d88e170e864d0cd4c372be2a193935caa2) )
	ROM_LOAD( "b018.bin", 0x8000, 0x2000, CRC(b824abc7) SHA1(4a30fec025150e889600a78497700155e743c99e) )
	ROM_LOAD( "b019.bin", 0xa000, 0x2000, CRC(3b487a46) SHA1(7837e480fd4648e0d3f792b79fa0019e063abdc6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "b026.bin", 0x0000, 0x2000, CRC(999ed2c7) SHA1(f01e4ee81f8f7b0d1cf001d3ec01a9210f8109b4) )
	ROM_LOAD( "b025.bin", 0x2000, 0x2000, CRC(33171e07) SHA1(55ee74c9f1d86ec13d92ea0d1b700bbe24b79def) ) /* adpcm */
	ROM_LOAD( "b024.bin", 0x4000, 0x2000, CRC(910b48b9) SHA1(c6a2f8266ff1f14b462b92d47a4a86542df77cdd) ) /* adpcm */
	ROM_LOAD( "b023.bin", 0x6000, 0x2000, CRC(47f66aac) SHA1(484802cfbff7c5f51dd97cb3b2321e137b03481c) )
	ROM_LOAD( "b022.bin", 0x8000, 0x2000, CRC(5928e749) SHA1(a4dbd6f2a6a7aa9597875dfd781e55b0fb14d49b) )
	ROM_LOAD( "b021.bin", 0xa000, 0x2000, CRC(ca17e3ba) SHA1(91a3ccd6045dcef5f3293d669fe5a4df59cd954b) )
	ROM_LOAD( "b020.bin", 0xc000, 0x2000, CRC(ada4f2cd) SHA1(15a4ed7497cb6c06f523ebe38bc4eb6dbcd09549) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "b000.bin", 0x00000, 0x2000, CRC(a4fa98a1) SHA1(33b9a1a56d72ffa5f4e16b69e6e19af5a2882b2c) )  /* plane0 */ /* tiles */
	ROM_LOAD( "b001.bin", 0x04000, 0x2000, CRC(fea09f7c) SHA1(174fc8022c455438538e6a3b67c7effc857ae634) )  /* plane1 */ /* tiles */

	ROM_REGION( 0x18000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "b013.bin", 0x00000, 0x2000, CRC(eaad4168) SHA1(f31b05ffb86677157f3a44cdcf0f9a729e0ab259) )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b004.bin", 0x02000, 0x2000, CRC(10a47e2d) SHA1(97fe2de3ce2b8dc017dceffce494be18695708d2) )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, CRC(b4842ea9) SHA1(471475f65edbd292b9162ad50e5cb0c7144845b0) )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, CRC(8cd166a5) SHA1(4b623c4c0025d75b3ed9746f8b6730bf3e65d85a) )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, CRC(4cbd3aa3) SHA1(a9a683dcc4f52b18450659a20434a4d2a7b411d9) )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, CRC(6be342a6) SHA1(0b8ac7ef7c6a6464fbc027a9fd17fa7ce1ffd962) )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b007.bin", 0x0c000, 0x2000, CRC(cb91d16b) SHA1(bf32a03e7882b74280b29fa004429b77ad52e5ee) )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b010.bin", 0x0e000, 0x2000, CRC(489c9c04) SHA1(d920ef4f03e1b2e871df0cb2d672689c89febe96) )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, CRC(7346db8a) SHA1(d2b2c1700ae0ff9c614a9981a3da3d69879e9f25) )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, CRC(b78714fc) SHA1(4df7f15c37d56a9d66d0049aad65b32063e5c29a) )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, CRC(b2557102) SHA1(ec4285029fc3ee1ad0adb05f363b234c67f8903d) )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, CRC(c85aba0e) SHA1(4be21b38623c2a8ae7f1e7397fb002e4cb9e4614) )  /* bot, plane1 */ /* sprites */

	ROM_REGION( 0x0300, REGION_PROMS, 0 )
	ROM_LOAD( "br27", 0x0000, 0x0100, CRC(f683c54a) SHA1(92893990456b92f04a2be98b8e9626e97b7a2562) ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, CRC(3ddbb6c4) SHA1(0eca5594d6812bc79f8b78f83fe003877d20c973) ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, CRC(ba4a5651) SHA1(77e81bd64ab59a7466d20eabdff4be241e963c52) ) /* blue */
ROM_END

ROM_START( karatedo )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "be14", 0x0000, 0x2000, CRC(44e60aa0) SHA1(6d007d7082c15182832f947444b00b7feb0e7738) )
	ROM_LOAD( "be15", 0x2000, 0x2000, CRC(a65e3793) SHA1(bf1e8fbc6755e85414eb7629e6fab3bf154f6546) )
	ROM_LOAD( "be16", 0x4000, 0x2000, CRC(151d8872) SHA1(1bb27142fdb33e3aeaf95c7a0ad7e8c258bbcb66) )
	ROM_LOAD( "be17", 0x6000, 0x2000, CRC(8f393b6a) SHA1(f246a6e069a2f562c5b7de05a2b8a6a09c1f4d1b) )
	ROM_LOAD( "be18", 0x8000, 0x2000, CRC(a09046ad) SHA1(665973bffc38e36b8b0f6bc79e10db280be0613e) )
	ROM_LOAD( "be19", 0xa000, 0x2000, CRC(0cdc4da9) SHA1(405454deda311abb8badd58a47529e42ddce5f6a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "be26", 0x0000, 0x2000, CRC(999ab0a3) SHA1(6aa545cee7a261f3dc5774dea2066f1412f63f49) )
	ROM_LOAD( "be25", 0x2000, 0x2000, CRC(253bf0da) SHA1(33bae6401003dc57deaa14bf7f6a7ebad5b7efe3) ) /* adpcm */
	ROM_LOAD( "be24", 0x4000, 0x2000, CRC(e2c188af) SHA1(b7a0801a4c634694f1556873fd21f7e13441be17) ) /* adpcm */
	ROM_LOAD( "be23", 0x6000, 0x2000, CRC(25262de1) SHA1(6264cd82756be9e1cdcd9ad3c3dfc6fef78dab8f) )
	ROM_LOAD( "be22", 0x8000, 0x2000, CRC(38055c48) SHA1(8406a52aaa7e56093a8d8552e928988b6fdd6c95) )
	ROM_LOAD( "be21", 0xa000, 0x2000, CRC(5f0efbe7) SHA1(f831efd02c917adac827fe6db8449ca8707b3d44) )
	ROM_LOAD( "be20", 0xc000, 0x2000, CRC(cbe8a533) SHA1(04cb41c487c2f951417628ed2888e04d59a39d29) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "be00",     0x00000, 0x2000, CRC(cec020f2) SHA1(07c501cc24797000f369fd98a26efe13875107bb) )  /* plane0 */ /* tiles */
	ROM_LOAD( "be01",     0x04000, 0x2000, CRC(cd96271c) SHA1(bcc71010e5489b19ad1553141c7b2e366bbbc68f) )  /* plane1 */ /* tiles */

	ROM_REGION( 0x18000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "be13",     0x00000, 0x2000, CRC(fb358707) SHA1(37124f1f545787723fecf466d8dcd31b88cdd75d) )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "be04",     0x02000, 0x2000, CRC(48372bf8) SHA1(28231b3bdb1d7226d7856554ba667b6d61f4fe22) )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, CRC(b4842ea9) SHA1(471475f65edbd292b9162ad50e5cb0c7144845b0) )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, CRC(8cd166a5) SHA1(4b623c4c0025d75b3ed9746f8b6730bf3e65d85a) )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, CRC(4cbd3aa3) SHA1(a9a683dcc4f52b18450659a20434a4d2a7b411d9) )  /* top, plane0 */ /* sprites */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, CRC(6be342a6) SHA1(0b8ac7ef7c6a6464fbc027a9fd17fa7ce1ffd962) )  /* bot, plane0 */ /* sprites */
	ROM_LOAD( "be07",     0x0c000, 0x2000, CRC(40f2b6fb) SHA1(8d9ee04d917a8e143bd00fa7582990213bfa42d3) )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "be10",     0x0e000, 0x2000, CRC(325c0a97) SHA1(0159536ff0ebac8ccf65aac1a524a30b3fca3418) )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, CRC(7346db8a) SHA1(d2b2c1700ae0ff9c614a9981a3da3d69879e9f25) )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, CRC(b78714fc) SHA1(4df7f15c37d56a9d66d0049aad65b32063e5c29a) )  /* bot, plane1 */ /* sprites */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, CRC(b2557102) SHA1(ec4285029fc3ee1ad0adb05f363b234c67f8903d) )  /* top, plane1 */ /* sprites */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, CRC(c85aba0e) SHA1(4be21b38623c2a8ae7f1e7397fb002e4cb9e4614) )  /* bot, plane1 */ /* sprites */

	ROM_REGION( 0x0300, REGION_PROMS, 0 )
	ROM_LOAD( "br27", 0x0000, 0x0100, CRC(f683c54a) SHA1(92893990456b92f04a2be98b8e9626e97b7a2562) ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, CRC(3ddbb6c4) SHA1(0eca5594d6812bc79f8b78f83fe003877d20c973) ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, CRC(ba4a5651) SHA1(77e81bd64ab59a7466d20eabdff4be241e963c52) ) /* blue */
ROM_END

ROM_START( kchampvs )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "bs24", 0x0000, 0x2000, CRC(829da69b) SHA1(3266e7686e537f34ee5ce4cccc349eb12fc65038) )
	ROM_LOAD( "bs23", 0x2000, 0x2000, CRC(091f810e) SHA1(283edb08ce106835185a1c2d6b88f7544d75f3b4) )
	ROM_LOAD( "bs22", 0x4000, 0x2000, CRC(d4df2a52) SHA1(60d6cb1cb51c6f80a0f88913d4152ab8bda752d6) )
	ROM_LOAD( "bs21", 0x6000, 0x2000, CRC(3d4ef0da) SHA1(228c8e47bb7123b69746506402edb875a43d7af5) )
	ROM_LOAD( "bs20", 0x8000, 0x2000, CRC(623a467b) SHA1(5f150c67632f8e32769b75aa0615d0eb018afdc4) )
	ROM_LOAD( "bs19", 0xa000, 0x2000, CRC(43e196c4) SHA1(8029798ea0a560603c3dcde56db5a1ccde58c514) )
	ROM_CONTINUE(     0xe000, 0x2000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "bs18", 0x0000, 0x2000, CRC(eaa646eb) SHA1(cbd48f4d5d225b71c2dd0b14f420838561e3f83e) )
	ROM_LOAD( "bs17", 0x2000, 0x2000, CRC(d71031ad) SHA1(b168f4ef4feb4195305404df699acecb731eab02) ) /* adpcm */
	ROM_LOAD( "bs16", 0x4000, 0x2000, CRC(6f811c43) SHA1(1d33ac8129562ab709bd7396b4c2457b6db99277) ) /* adpcm */

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "bs12",     0x00000, 0x2000, CRC(4c574ecd) SHA1(86914eef33da73463ba6261eecae75209d24fac1) )
	ROM_LOAD( "bs13",     0x02000, 0x2000, CRC(750b66af) SHA1(c7824994b977d4e846f3ecadfcfc51331f52b6f4) )
	ROM_LOAD( "bs14",     0x04000, 0x2000, CRC(9ad6227c) SHA1(708af5e70927040cf7f2ae6f792344c19099530c) )
	ROM_LOAD( "bs15",     0x06000, 0x2000, CRC(3b6d5de5) SHA1(288fffcbc9369db5c75e7e0d6181612de6f12da3) )

	ROM_REGION( 0x18000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "bs00",     0x00000, 0x2000, CRC(51eda56c) SHA1(31438e115e95c2a684ec65ed2bdb9125e3675226) )
	ROM_LOAD( "bs06",     0x02000, 0x2000, CRC(593264cf) SHA1(866469f37b6c90afc65e53e6589b67ac4b25997e) )
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, CRC(b4842ea9) SHA1(471475f65edbd292b9162ad50e5cb0c7144845b0) )  /* bs01 */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, CRC(8cd166a5) SHA1(4b623c4c0025d75b3ed9746f8b6730bf3e65d85a) )  /* bs07 */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, CRC(4cbd3aa3) SHA1(a9a683dcc4f52b18450659a20434a4d2a7b411d9) )  /* bs02 */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, CRC(6be342a6) SHA1(0b8ac7ef7c6a6464fbc027a9fd17fa7ce1ffd962) )  /* bs08 */
	ROM_LOAD( "bs03",     0x0c000, 0x2000, CRC(8dcd271a) SHA1(0abeaa46433a59c110815ecf188c7afd6fa387a4) )
	ROM_LOAD( "bs09",     0x0e000, 0x2000, CRC(4ee1dba7) SHA1(717ce9a4e20f6e02adf678b1400af4aaecdbfb40) )
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, CRC(7346db8a) SHA1(d2b2c1700ae0ff9c614a9981a3da3d69879e9f25) )  /* bs04 */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, CRC(b78714fc) SHA1(4df7f15c37d56a9d66d0049aad65b32063e5c29a) )  /* bs10 */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, CRC(b2557102) SHA1(ec4285029fc3ee1ad0adb05f363b234c67f8903d) )  /* bs05 */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, CRC(c85aba0e) SHA1(4be21b38623c2a8ae7f1e7397fb002e4cb9e4614) )  /* bs11 */

	ROM_REGION( 0x0300, REGION_PROMS, 0 )
	ROM_LOAD( "br27", 0x0000, 0x0100, CRC(f683c54a) SHA1(92893990456b92f04a2be98b8e9626e97b7a2562) ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, CRC(3ddbb6c4) SHA1(0eca5594d6812bc79f8b78f83fe003877d20c973) ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, CRC(ba4a5651) SHA1(77e81bd64ab59a7466d20eabdff4be241e963c52) ) /* blue */
ROM_END

ROM_START( karatevs )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "br24", 0x0000, 0x2000, CRC(ea9cda49) SHA1(7d753a8d391418d0fe5231eb88b3627f7d3fd99e) )
	ROM_LOAD( "br23", 0x2000, 0x2000, CRC(46074489) SHA1(5593f819b6893820ef0c0fece13cf3ca83e1ab85) )
	ROM_LOAD( "br22", 0x4000, 0x2000, CRC(294f67ba) SHA1(45f13a7deb75bb167176c5405128de3ca76e22f0) )
	ROM_LOAD( "br21", 0x6000, 0x2000, CRC(934ea874) SHA1(dbc139715a1598033beedbf4f8fec73703b016d6) )
	ROM_LOAD( "br20", 0x8000, 0x2000, CRC(97d7816a) SHA1(e02f9306fc3539f4feaedfcabea66d172d09a510) )
	ROM_LOAD( "br19", 0xa000, 0x2000, CRC(dd2239d2) SHA1(0533d5abf8e25a4aeec2f7832b657eab56fd11f0) )
	ROM_CONTINUE(     0xe000, 0x2000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* Sound CPU */ /* 64k for code */
	ROM_LOAD( "br18", 0x0000, 0x2000, CRC(00ccb8ea) SHA1(83d69684dc3ad37aca03c901fd23c7652134766f) )
	ROM_LOAD( "bs17", 0x2000, 0x2000, CRC(d71031ad) SHA1(b168f4ef4feb4195305404df699acecb731eab02) ) /* adpcm */
	ROM_LOAD( "br16", 0x4000, 0x2000, CRC(2512d961) SHA1(f0cd1be112b915d700e0587759606d48d115a83f) ) /* adpcm */

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "br12",     0x00000, 0x2000, CRC(9ed6f00d) SHA1(3def985deb29a7644309ede3bd82c225b4ae23f8) )
	ROM_LOAD( "bs13",     0x02000, 0x2000, CRC(750b66af) SHA1(c7824994b977d4e846f3ecadfcfc51331f52b6f4) )
	ROM_LOAD( "br14",     0x04000, 0x2000, CRC(fc399229) SHA1(e8d633151b0d7fa49c455920c4b0588575a7084e) )
	ROM_LOAD( "bs15",     0x06000, 0x2000, CRC(3b6d5de5) SHA1(288fffcbc9369db5c75e7e0d6181612de6f12da3) )

	ROM_REGION( 0x18000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "br00",     0x00000, 0x2000, CRC(c46a8b88) SHA1(a47e56a6dc7f36b896b8156e77a1da7e8be2332e) )
	ROM_LOAD( "br06",     0x02000, 0x2000, CRC(cf8982ff) SHA1(aafb249503ad51f64b1f31ea2d869dfc0e065d19) )
	ROM_LOAD( "b012.bin", 0x04000, 0x2000, CRC(b4842ea9) SHA1(471475f65edbd292b9162ad50e5cb0c7144845b0) )  /* bs01 */
	ROM_LOAD( "b003.bin", 0x06000, 0x2000, CRC(8cd166a5) SHA1(4b623c4c0025d75b3ed9746f8b6730bf3e65d85a) )  /* bs07 */
	ROM_LOAD( "b011.bin", 0x08000, 0x2000, CRC(4cbd3aa3) SHA1(a9a683dcc4f52b18450659a20434a4d2a7b411d9) )  /* bs02 */
	ROM_LOAD( "b002.bin", 0x0a000, 0x2000, CRC(6be342a6) SHA1(0b8ac7ef7c6a6464fbc027a9fd17fa7ce1ffd962) )  /* bs08 */
	ROM_LOAD( "br03",     0x0c000, 0x2000, CRC(bde8a52b) SHA1(1a0800472caf8c79a15cc977dad1a7bc97c74b2b) )
	ROM_LOAD( "br09",     0x0e000, 0x2000, CRC(e9a5f945) SHA1(e6b21912bee97de06819c8ac85a45bbc70030f88) )
	ROM_LOAD( "b006.bin", 0x10000, 0x2000, CRC(7346db8a) SHA1(d2b2c1700ae0ff9c614a9981a3da3d69879e9f25) )  /* bs04 */
	ROM_LOAD( "b009.bin", 0x12000, 0x2000, CRC(b78714fc) SHA1(4df7f15c37d56a9d66d0049aad65b32063e5c29a) )  /* bs10 */
	ROM_LOAD( "b005.bin", 0x14000, 0x2000, CRC(b2557102) SHA1(ec4285029fc3ee1ad0adb05f363b234c67f8903d) )  /* bs05 */
	ROM_LOAD( "b008.bin", 0x16000, 0x2000, CRC(c85aba0e) SHA1(4be21b38623c2a8ae7f1e7397fb002e4cb9e4614) )  /* bs11 */

	ROM_REGION( 0x0300, REGION_PROMS, 0 )
	ROM_LOAD( "br27", 0x0000, 0x0100, CRC(f683c54a) SHA1(92893990456b92f04a2be98b8e9626e97b7a2562) ) /* red */
	ROM_LOAD( "br26", 0x0100, 0x0100, CRC(3ddbb6c4) SHA1(0eca5594d6812bc79f8b78f83fe003877d20c973) ) /* green */
	ROM_LOAD( "br25", 0x0200, 0x0100, CRC(ba4a5651) SHA1(77e81bd64ab59a7466d20eabdff4be241e963c52) ) /* blue */
ROM_END



static DRIVER_INIT( kchampvs )
{
	UINT8 *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;
	int A;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0;A < 0x10000;A++)
		rom[A+diff] = (rom[A] & 0x55) | ((rom[A] & 0x88) >> 2) | ((rom[A] & 0x22) << 2);

	/*
		Note that the first 4 opcodes that the program
		executes aren't encrypted for some obscure reason.
		The address for the 2nd opcode (a jump) is encrypted too.
		It's not clear what the 3rd and 4th opcode are supposed to do,
		they just write to a RAM location. This write might be what
		turns the encryption on, but this doesn't explain the
		encrypted address for the jump.
	 */
	rom[0+diff] = rom[0];	/* this is a jump */
	A = rom[1] + 256 * rom[2];
	rom[A+diff] = rom[A];	/* fix opcode on first jump address (again, a jump) */
	rom[A+1] ^= 0xee;		/* fix address of the second jump */
	A = rom[A+1] + 256 * rom[A+2];
	rom[A+diff] = rom[A];	/* fix third opcode (ld a,$xx) */
	A += 2;
	rom[A+diff] = rom[A];	/* fix fourth opcode (ld ($xxxx),a */
	/* and from here on, opcodes are encrypted */
}



GAME( 1984, kchamp,   0,      kchamp,   kchamp,   0,        ROT90, "Data East USA", "Karate Champ (US)" )
GAME( 1984, karatedo, kchamp, kchamp,   kchamp,   0,        ROT90, "Data East Corporation", "Karate Dou (Japan)" )
GAME( 1984, kchampvs, kchamp, kchampvs, kchampvs, kchampvs, ROT90, "Data East USA", "Karate Champ (US VS version)" )
GAME( 1984, karatevs, kchamp, kchampvs, kchampvs, kchampvs, ROT90, "Data East Corporation", "Taisen Karate Dou (Japan VS version)" )
