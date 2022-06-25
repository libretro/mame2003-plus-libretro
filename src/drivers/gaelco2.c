/***************************************************************************

	Gaelco CG-1V/GAE1 based games

	Driver by Manuel Abadia <manu@teleline.es>

	Known games that run on this hardware:
	======================================
	Game           | Year | Chip      | Ref      |Protected
	---------------+------+-----------+----------+--------------------
	Alligator Hunt | 1994 | GAE1 449  | 940411   | DS5002FP, but unprotected version available
	World Rally 2  | 1995 | GAE1 449  | 950510   | DS5002FP
	Touch & Go     | 1995 | GAE1 501  | 950510-1 | DS5002FP
	Maniac Square  | 1996 | Unknown   | ???      | DS5002FP, but unprotected version available
	Snow Board     | 1996 | CG-1V 366 | 960419/1 | Lattice IspLSI 1016-80LJ
	Bang!          | 1998 | CG-1V 388 | ???      | No

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m68000/m68000.h"

extern data16_t *gaelco_sndregs;
extern data16_t *gaelco2_vregs;
extern data16_t *snowboar_protection;
extern data32_t  snowboard_latch;

/* comment this line to display 2 monitors for the dual monitor games */
#define ONE_MONITOR

/* from machine/gaelco2.c */
DRIVER_INIT( alighunt );
DRIVER_INIT( touchgo );
DRIVER_INIT( snowboar );
WRITE16_HANDLER( gaelco2_coin_w );
WRITE16_HANDLER( gaelco2_coin2_w );
WRITE16_HANDLER( wrally2_coin_w );
NVRAM_HANDLER( gaelco2 );
READ16_HANDLER( gaelco2_eeprom_r );
WRITE16_HANDLER( gaelco2_eeprom_cs_w );
WRITE16_HANDLER( gaelco2_eeprom_sk_w );
WRITE16_HANDLER( gaelco2_eeprom_data_w );
READ16_HANDLER( snowboar_protection_r );
WRITE16_HANDLER( snowboar_protection_w );

/* from vidhrdw/gaelco2.c */
WRITE16_HANDLER( gaelco2_vram_w );
WRITE16_HANDLER( gaelco2_palette_w );
VIDEO_UPDATE( gaelco2 );
VIDEO_EOF( gaelco2 );
VIDEO_START( gaelco2 );
VIDEO_UPDATE( gaelco2_dual );
VIDEO_START( gaelco2_dual );
VIDEO_UPDATE( bang );


#define TILELAYOUT16(NUM) static struct GfxLayout tilelayout16_##NUM =				\
{																					\
	16,16,											/* 16x16 tiles */				\
	NUM/32,											/* number of tiles */			\
	5,												/* 5 bpp */						\
	{ 4*NUM*8, 3*NUM*8, 2*NUM*8, 1*NUM*8, 0*NUM*8 },								\
	{ 0,1,2,3,4,5,6,7, 16*8+0,16*8+1,16*8+2,16*8+3,16*8+4,16*8+5,16*8+6,16*8+7 },	\
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8, 8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 },		\
	32*8																			\
}

#define GFXDECODEINFO(NUM,ENTRIES) static struct GfxDecodeInfo gfxdecodeinfo_##NUM[] =	\
{																						\
	{ REGION_GFX1, 0x0000000, &tilelayout16_##NUM,0,	ENTRIES },						\
	{ -1 }																				\
}

TILELAYOUT16(0x0080000);
GFXDECODEINFO(0x0080000, 128);
TILELAYOUT16(0x0200000);
GFXDECODEINFO(0x0200000, 128);
TILELAYOUT16(0x0400000);
GFXDECODEINFO(0x0400000, 128);


/*============================================================================
							MANIAC SQUARE (FINAL)
  ============================================================================*/

static MEMORY_READ16_START( maniacsq_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },			/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_r },		/* Sound Registers */
	{ 0x200000, 0x20ffff, MRA16_RAM },			/* Video RAM */
	{ 0x210000, 0x211fff, MRA16_RAM },			/* Palette */
	{ 0x218004, 0x218009, MRA16_RAM },			/* Video Registers */
	{ 0x300000, 0x300001, input_port_0_word_r },/* DSW #1 + Input 1P */
	{ 0x300002, 0x300003, input_port_1_word_r },/* DSW #2 + Input 2P */
	{ 0x320000, 0x320001, input_port_2_word_r },/* COINSW + SERVICESW */
	{ 0xfe0000, 0xfeffff, MRA16_RAM },			/* Work RAM */
MEMORY_END

static MEMORY_WRITE16_START( maniacsq_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },							/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_w, &gaelco_sndregs },		/* Sound Registers */
	{ 0x200000, 0x20ffff, gaelco2_vram_w, &spriteram16, &spriteram_size },	/* Video RAM */
	{ 0x210000, 0x211fff, gaelco2_palette_w, &paletteram16 },	/* Palette */
	{ 0x218004, 0x218009, MWA16_RAM, &gaelco2_vregs },			/* Video Registers */
	{ 0x30004a, 0x30004b, MWA16_NOP },							/* Sound muting? */
	{ 0x500000, 0x500001, gaelco2_coin_w },						/* Coin lockout + counters */
	{ 0xfe0000, 0xfeffff, MWA16_RAM },							/* Work RAM */
MEMORY_END


INPUT_PORTS_START( maniacsq )
PORT_START	/* DSW #1 + 1P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x0f00, 0x0f00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0700, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0900, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0500, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0f00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0e00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0d00, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0b00, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0a00, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0000, "Disabled or Free Play (if Coin B too)" )
	PORT_DIPNAME( 0xf000, 0xf000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x9000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0xf000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0xd000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0xb000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0000, "Disabled or Free Play (if Coin A too)" )

PORT_START	/* DSW #2 + 2P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0300, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "1P Continue" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

PORT_START	/* COINSW & SERVICESW */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE2, 1 )	/* go to service mode NOW */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static struct gaelcosnd_interface maniacsq_snd_interface =
{
	REGION_GFX1, 							/* memory region */
	{ 0*0x0080000, 1*0x0080000, 0, 0 },		/* start of each ROM bank */
	{ 100, 100 }							/* volume */
};

static MACHINE_DRIVER_START( maniacsq )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 26000000/2)		/* 13 MHz? */
	MDRV_CPU_MEMORY(maniacsq_readmem, maniacsq_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold, 1)

	MDRV_FRAMES_PER_SECOND(59.1)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*16, 32*16)
	MDRV_VISIBLE_AREA(0, 320-1, 16, 256-1)
	MDRV_GFXDECODE(gfxdecodeinfo_0x0080000)
	MDRV_PALETTE_LENGTH(4096*16 - 16)	/* game's palette is 4096 but we allocate 15 more for shadows & highlights */

	MDRV_VIDEO_START(gaelco2)
	MDRV_VIDEO_EOF(gaelco2)
	MDRV_VIDEO_UPDATE(gaelco2)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(GAELCO_GAE1, maniacsq_snd_interface)
MACHINE_DRIVER_END


ROM_START( maniacsq )
	ROM_REGION( 0x040000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "d8-d15.1m",	0x000000, 0x020000, CRC(9121d1b6) SHA1(ad8f0d996b6d42fc0c6645466608e82ca96e0b66) )
	ROM_LOAD16_BYTE( "d0-d7.1m",	0x000001, 0x020000, CRC(a95cfd2a) SHA1(b5bad76f12d2a1f6bf6b35482f2f933ceb00e552) )

	ROM_REGION( 0x0280000, REGION_GFX1, 0 ) /* GFX + Sound */
	ROM_LOAD( "d0-d7.4m",	0x0000000, 0x0080000, CRC(d8551b2f) SHA1(78b5b07112bd89fed18055180e7cc64f8e0bd0b1) )	/* GFX + Sound */
	ROM_LOAD( "d8-d15.4m",	0x0080000, 0x0080000, CRC(b269c427) SHA1(b7f9501529fbb7ee82700cff82740ba5770cf3c5) )	/* GFX + Sound */
	ROM_LOAD( "d16-d23.1m",	0x0100000, 0x0020000, CRC(af4ea5e7) SHA1(ffaf09dc2588e32c124e7dd2f86ba009f1b8b176) )	/* GFX only */
	ROM_FILL(				0x0120000, 0x0060000, 0x0 )			/* Empty */
	ROM_LOAD( "d24-d31.1m",	0x0180000, 0x0020000, CRC(578c3588) SHA1(c2e1fba29f21d6822677886fb2d26e050b336c14) )	/* GFX only */
	ROM_FILL(				0x01a0000, 0x0060000, 0x0 )			/* Empty */
	ROM_FILL(				0x0200000, 0x0080000, 0x0 )			/* to decode GFX as 5bpp */
ROM_END



/*============================================================================
								BANG
  ============================================================================*/

static int clr_gun_int;

static DRIVER_INIT( bang )
{
	clr_gun_int = 0;
}

static WRITE16_HANDLER( clr_gun_int_w )
{
	clr_gun_int = 1;
}

static INTERRUPT_GEN( bang_interrupt )
{
	if (cpu_getiloops() == 0){
		cpu_set_irq_line(0, 2, HOLD_LINE);

		clr_gun_int = 0;
	}
	else if (cpu_getiloops() % 2){
		if (clr_gun_int){
			cpu_set_irq_line(0, 4, HOLD_LINE);
		}
	}
}

static READ16_HANDLER(p1_gun_x) {return (readinputport(3)*320/0x100)+1;}
static READ16_HANDLER(p1_gun_y) {return (readinputport(5)*240/0x100)-4;}
static READ16_HANDLER(p2_gun_x) {return (readinputport(4)*320/0x100)+1;}
static READ16_HANDLER(p2_gun_y) {return (readinputport(6)*240/0x100)-4;}

static MEMORY_READ16_START( bang_readmem )
    { 0x000000, 0x0fffff, MRA16_ROM }, /* ROM */
    { 0x202890, 0x2028ff, gaelcosnd_r }, /* Sound Registers */
    { 0x200000, 0x20ffff, MRA16_RAM }, /* Video RAM */
    { 0x210000, 0x211fff, MRA16_RAM }, /* Palette */
    { 0x218004, 0x218009, MRA16_RAM }, /* Video Registers */
    { 0x300000, 0x300001, input_port_0_word_r },/* 1P Input */
    { 0x300002, 0x300003, MRA16_NOP }, /* Random number generator? */
    { 0x300010, 0x300011, input_port_1_word_r },/* 2P Input */
    { 0x300020, 0x300021, gaelco2_eeprom_r }, /* EEPROM status + read */
    { 0x310000, 0x310001, p1_gun_x },/* Gun 1P X */
    { 0x310002, 0x310003, p2_gun_x },/* Gun 2P X */
    { 0x310004, 0x310005, p1_gun_y },/* Gun 1P Y */
    { 0x310006, 0x310007, p2_gun_y },/* Gun 2P Y */
    { 0xfe0000, 0xfeffff, MRA16_RAM }, /* Work RAM */
MEMORY_END

static MEMORY_WRITE16_START( bang_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },							/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_w, &gaelco_sndregs },		/* Sound Registers */
	{ 0x200000, 0x20ffff, gaelco2_vram_w, &spriteram16, &spriteram_size },	/* Video RAM */
	{ 0x210000, 0x211fff, gaelco2_palette_w, &paletteram16 },	/* Palette */
	{ 0x218004, 0x218007, MWA16_RAM, &gaelco2_vregs },			/* Video Registers */
	{ 0x218008, 0x218009, MWA16_NOP },							/* CLR INT Video */
	{ 0x300000, 0x300003, gaelco2_coin2_w },					/* Coin Counters */
	{ 0x300008, 0x300009, gaelco2_eeprom_data_w },				/* EEPROM data */
	{ 0x30000a, 0x30000b, gaelco2_eeprom_sk_w },				/* EEPROM serial clock */
	{ 0x30000c, 0x30000d, gaelco2_eeprom_cs_w },				/* EEPROM chip select */
	{ 0x310000, 0x310001, clr_gun_int_w },						/* CLR INT Gun */
	{ 0xfe0000, 0xfeffff, MWA16_RAM },							/* Work RAM */
MEMORY_END


INPUT_PORTS_START( bang )
PORT_START	/* 1P INPUTS */
	PORT_BIT_IMPULSE( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, 1 )

PORT_START	/* 2P INPUTS */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )

PORT_START	/* COINSW & Service */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x0004, IP_ACTIVE_LOW)	/* go to service mode NOW */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	/* bits 6 & 7 are used for accessing the NVRAM */

	PORT_START /* Gun 1 X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 35, 15, 0, 0xff)

	PORT_START /* Gun 2 X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 35, 15, 0, 0xff)

	PORT_START /* Gun 1 Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 35, 15, 0, 0xff)

	PORT_START /* Gun 2 Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 35, 15, 0, 0xff)
INPUT_PORTS_END

static struct gaelcosnd_interface bang_snd_interface =
{
	REGION_GFX1, 											/* memory region */
	{ 0*0x0200000, 1*0x0200000, 2*0x0200000, 3*0x0200000 },	/* start of each ROM bank */
	{ 100, 100 }											/* volume */
};

static MACHINE_DRIVER_START( bang )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 30000000/2)			/* 15 MHz */
	MDRV_CPU_MEMORY(bang_readmem, bang_writemem)
	MDRV_CPU_VBLANK_INT(bang_interrupt, 6)

	MDRV_FRAMES_PER_SECOND(59.1)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_NVRAM_HANDLER(gaelco2)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*16, 32*16)
	MDRV_VISIBLE_AREA(0, 320-1, 16, 256-1)
	MDRV_GFXDECODE(gfxdecodeinfo_0x0200000)
	MDRV_PALETTE_LENGTH(4096*16 - 16)	/* game's palette is 4096 but we allocate 15 more for shadows & highlights */

	MDRV_VIDEO_START(gaelco2)
	MDRV_VIDEO_EOF(gaelco2)
	MDRV_VIDEO_UPDATE(bang)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(GAELCO_CG1V, bang_snd_interface)
MACHINE_DRIVER_END


ROM_START( bang )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "bang.u53",	0x000000, 0x080000, CRC(014bb939) SHA1(bb245acf7a3bd4a56b3559518bcb8d0ae39dbaf4) )
	ROM_LOAD16_BYTE( "bang.u55",	0x000001, 0x080000, CRC(582f8b1e) SHA1(c9b0d4c1dee71cdb2c01d49f20ffde32eddc9583) )

	ROM_REGION( 0x0a00000, REGION_GFX1, 0 ) /* GFX + Sound */
	ROM_LOAD( "bang.u16",	0x0000000, 0x0080000, CRC(6ee4b878) SHA1(f646380d95650a60b5a17973bdfd3b80450a4d3b) )	/* GFX only */
	ROM_LOAD( "bang.u17",	0x0080000, 0x0080000, CRC(0c35aa6f) SHA1(df0474b1b9466d3c199e5aade39b7233f0cb45ee) )	/* GFX only */
	ROM_LOAD( "bang.u18",	0x0100000, 0x0080000, CRC(2056b1ad) SHA1(b796f92eef4bbb0efa12c53580e429b8a0aa394c) )	/* Sound only */
	ROM_FILL(				0x0180000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang.u9",	0x0200000, 0x0080000, CRC(078195dc) SHA1(362ff194e2579346dfc7af88559b0718bc36ec8a) )	/* GFX only */
	ROM_LOAD( "bang.u10",	0x0280000, 0x0080000, CRC(06711eeb) SHA1(3662ffe730fb54ee48925de9765f88be1abd5e4e) )	/* GFX only */
	ROM_LOAD( "bang.u11",	0x0300000, 0x0080000, CRC(2088d15c) SHA1(0c043ab9fd33836fa4b7ad60fd8e7cb96ffb6121) )	/* Sound only */
	ROM_FILL(				0x0380000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang.u1",	0x0400000, 0x0080000, CRC(e7b97b0f) SHA1(b5503687ae3ca0a0faa4b867a267d89dac788d6d) )	/* GFX only */
	ROM_LOAD( "bang.u2",	0x0480000, 0x0080000, CRC(ff297a8f) SHA1(28819a9d7b3cb177e7a7db3fe23a94f5cba33049) )	/* GFX only */
	ROM_LOAD( "bang.u3",	0x0500000, 0x0080000, CRC(d3da5d4f) SHA1(b9bea0b4d20ab0bfda3fac2bb1fab974c007aaf0) )	/* Sound only */
	ROM_FILL(				0x0580000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang.u20",	0x0600000, 0x0080000, CRC(a1145df8) SHA1(305cda041a6f201cb011982f1bf1fc6a4153a669) )	/* GFX only */
	ROM_LOAD( "bang.u13",	0x0680000, 0x0080000, CRC(fe3e8d07) SHA1(7a37561b1cf422b47cddb8751a6b6d57dec8baae) )	/* GFX only */
	ROM_LOAD( "bang.u5",	0x0700000, 0x0080000, CRC(9bee444c) SHA1(aebaa3306e7e5aada99ed469da9bf64507808cff) )	/* Sound only */
	ROM_FILL(				0x0780000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang.u21",	0x0800000, 0x0080000, CRC(fd93d7f2) SHA1(ff9d8eb5ac8d9757132aa6d79d2f7662c14cd650) )	/* GFX only */
	ROM_LOAD( "bang.u14",	0x0880000, 0x0080000, CRC(858fcbf9) SHA1(1e67431c8775666f4839bdc427fabf59ffc708c0) )	/* GFX only */
	ROM_FILL(				0x0900000, 0x0100000, 0x0 )			/* Empty */
ROM_END



ROM_START( bangj )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "bang-a.u53",	0x000000, 0x080000, CRC(5ee514e9) SHA1(b78b507d18de41be58049f5c597acd107ec1273f) )
	ROM_LOAD16_BYTE( "bang-a.u55",	0x000001, 0x080000, CRC(b90223ab) SHA1(7c097754a710169f41c574c3cc1a6346824853c4) )

	ROM_REGION( 0x0a00000, REGION_GFX1, 0 ) /* GFX + Sound */
	ROM_LOAD( "bang-a.u16",	0x0000000, 0x0080000, CRC(3b63acfc) SHA1(48f5598cdbc70f342d6b75909166571271920a8f) )	/* GFX only */
	ROM_LOAD( "bang-a.u17",	0x0080000, 0x0080000, CRC(72865b80) SHA1(ec7753ea7961015149b9e6386fdeb9bd59aa962a) )	/* GFX only */
	ROM_LOAD( "bang.u18",	0x0100000, 0x0080000, CRC(2056b1ad) SHA1(b796f92eef4bbb0efa12c53580e429b8a0aa394c) )	/* Sound only */
	ROM_FILL(				0x0180000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang-a.u9",	0x0200000, 0x0080000, CRC(3cb86360) SHA1(c803b3add253a552a1554714218740bdfca91764) )	/* GFX only */
	ROM_LOAD( "bang-a.u10",	0x0280000, 0x0080000, CRC(03fdd777) SHA1(9eec194239f93d961ee9902a585c872dcdc7728f) )	/* GFX only */
	ROM_LOAD( "bang.u11",	0x0300000, 0x0080000, CRC(2088d15c) SHA1(0c043ab9fd33836fa4b7ad60fd8e7cb96ffb6121) )	/* Sound only */
	ROM_FILL(				0x0380000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang-a.u1",	0x0400000, 0x0080000, CRC(965d0ad9) SHA1(eff521735129b7dd9366855c6312ed568950233c) )	/* GFX only */
	ROM_LOAD( "bang-a.u2",	0x0480000, 0x0080000, CRC(8ea261a7) SHA1(50b59cf058ca03c0b8c888f6ddb40c720a210ece) )	/* GFX only */
	ROM_LOAD( "bang.u3",	0x0500000, 0x0080000, CRC(d3da5d4f) SHA1(b9bea0b4d20ab0bfda3fac2bb1fab974c007aaf0) )	/* Sound only */
	ROM_FILL(				0x0580000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang-a.u20",	0x0600000, 0x0080000, CRC(4b828f3c) SHA1(5227a89c05c659a85d33f092c6778ce9d57a0236) )	/* GFX only */
	ROM_LOAD( "bang-a.u13",	0x0680000, 0x0080000, CRC(d1146b92) SHA1(2b28d49fbffea6c038160fdab177bc0045195ca8) )	/* GFX only */
	ROM_LOAD( "bang.u5",	0x0700000, 0x0080000, CRC(9bee444c) SHA1(aebaa3306e7e5aada99ed469da9bf64507808cff) )	/* Sound only */
	ROM_FILL(				0x0780000, 0x0080000, 0x0 )			/* Empty */
	ROM_LOAD( "bang-a.u21",	0x0800000, 0x0080000, CRC(531ce3b6) SHA1(196bb720591acc082f815b609a7cf1609510c8c1) )	/* GFX only */
	ROM_LOAD( "bang-a.u14",	0x0880000, 0x0080000, CRC(f8e1cf84) SHA1(559c08584094e605635c5ef3a25534ea0bcfa199) )	/* GFX only */
	ROM_FILL(				0x0900000, 0x0100000, 0x0 )			/* Empty */
ROM_END


/*============================================================================
							ALLIGATOR HUNT
  ============================================================================*/


static MEMORY_READ16_START( alighunt_readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM },			/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_r },		/* Sound Registers */
	{ 0x200000, 0x20ffff, MRA16_RAM },			/* Video RAM */
	{ 0x210000, 0x211fff, MRA16_RAM },			/* Palette */
	{ 0x218004, 0x218009, MRA16_RAM },			/* Video Registers */
	{ 0x300000, 0x300001, input_port_0_word_r },/* DSW #1 + Input 1P */
	{ 0x300002, 0x300003, input_port_1_word_r },/* DSW #2 + Input 2P */
	{ 0x320000, 0x320001, input_port_2_word_r },/* COINSW + Service */
	{ 0xfe0000, 0xfeffff, MRA16_RAM },			/* Work RAM */
MEMORY_END

static MEMORY_WRITE16_START( alighunt_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },							/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_w, &gaelco_sndregs },		/* Sound Registers */
	{ 0x200000, 0x20ffff, gaelco2_vram_w, &spriteram16, &spriteram_size },	/* Video RAM */
	{ 0x210000, 0x211fff, gaelco2_palette_w, &paletteram16 },	/* Palette */
	{ 0x218004, 0x218009, MWA16_RAM, &gaelco2_vregs },			/* Video Registers */
	{ 0x500000, 0x500001, gaelco2_coin_w },						/* Coin lockout + counters */
	{ 0x500006, 0x500007, MWA16_NOP },							/* ??? */
	{ 0xfe0000, 0xfeffff, MWA16_RAM },							/* Work RAM */
MEMORY_END


INPUT_PORTS_START( alighunt )

PORT_START	/* DSW #1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x0f00, 0x0f00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0700, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0900, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0500, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0f00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0e00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0d00, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0b00, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0a00, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0000, "Disabled or Free Play (if Coin B too)" )
	PORT_DIPNAME( 0xf000, 0xf000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x9000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0xf000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0xd000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0xb000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x0000, "Disabled or Free Play (if Coin A too)" )

PORT_START	/* DSW #2 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0300, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0800, "1" )
	PORT_DIPSETTING(      0x0c00, "2" )
	PORT_DIPSETTING(      0x0400, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x1000, 0x1000, "Sound Type" )
	PORT_DIPSETTING(      0x0000, "Mono" )
	PORT_DIPSETTING(      0x1000, "Stereo" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Joystick" )
	PORT_DIPSETTING(      0x0000, "Analog" )		/* TO-DO */
	PORT_DIPSETTING(      0x4000, "Standard" )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

PORT_START	/* COINSW & Service */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE2 )	/* go to test mode NOW */
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static struct gaelcosnd_interface alighunt_snd_interface =
{
	REGION_GFX1, 											/* memory region */
	{ 0*0x0400000, 1*0x0400000, 2*0x0400000, 3*0x0400000 },	/* start of each ROM bank */
	{ 100, 100 }											/* volume */
};

static MACHINE_DRIVER_START( alighunt )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 24000000/2)			/* 12 MHz */
	MDRV_CPU_MEMORY(alighunt_readmem, alighunt_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold, 1)

	MDRV_FRAMES_PER_SECOND(59.1)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*16, 32*16)
	MDRV_VISIBLE_AREA(0, 320-1, 16, 256-1)
	MDRV_GFXDECODE(gfxdecodeinfo_0x0400000)
	MDRV_PALETTE_LENGTH(4096*16 - 16)	/* game's palette is 4096 but we allocate 15 more for shadows & highlights */

	MDRV_VIDEO_START(gaelco2)
	MDRV_VIDEO_EOF(gaelco2)
	MDRV_VIDEO_UPDATE(gaelco2)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(GAELCO_GAE1, alighunt_snd_interface)
MACHINE_DRIVER_END


/*
PCB Layout:

REF: 940411
------------------------------------------------------------------------------
|                POT1               KM428C256J-6 (x3)                        |
|                                                                            |
|                POT2                                                        |
|---                                                                         |
   |                                                               U47       |
   |                   30.000MHz          |----------|                       |
|---                                      |          |             U48       |
|                                         | GAE1 449 |                       |
| J                            6264       | (QFP208) |             U49       |
|                              6264       |          |                       |
| A                                       |----------|             U50       |
|                                                                            |
| M                                                                          |
|                         |-------------------------|                        |
| M                       |                         |  24.000MHz     62256   |
|                         |  62256  DS5002  BATT_3V |                62256   |
| A                       |                         |                        |
|                         |-------------------------|                        |
|                                                                            |
|---                                    62256                                |
   |                                    62256                                |
   |                                                                         |
|---                                                                         |
|   DSW1                         MC68000P12        U45                       |
|                                                  U44                       |
|   DSW2                                                                     |
|                                                                            |
-----------------------------------------------------------------------------|
*/


ROM_START( aligator )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE(	"u45",	0x000000, 0x080000, CRC(61c47c56) SHA1(6dd3fc6fdab252e0fb43c0793eef70203c888d7f) )
	ROM_LOAD16_BYTE(	"u44",	0x000001, 0x080000, CRC(f0be007a) SHA1(2112b2e5f020028b50c8f2c72c83c9fee7a78224) )

	ROM_REGION( 0x1400000, REGION_GFX1, 0 ) /* GFX + Sound */
	/* 0x0000000-0x0ffffff filled in in the DRIVER_INIT */
	ROM_FILL(				0x1000000, 0x0400000, 0x0 )		/* to decode GFX as 5 bpp */

	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_DISPOSE ) /* Temporary storage */
	ROM_LOAD( "u48",		0x0000000, 0x0400000, CRC(19e03bf1) SHA1(2b3a4bb438b0aebf4f6a9fd26b071e5c9dd222b8) )	/* GFX only */
	ROM_LOAD( "u47",		0x0400000, 0x0400000, CRC(74a5a29f) SHA1(8ea2aa1f8a80c5b88ca9222c5ecc3c4794e0a160) )	/* GFX + Sound */
	ROM_LOAD( "u50",		0x0800000, 0x0400000, CRC(85daecf9) SHA1(824f6d2491075b1ef96ecd6667c5510409338a2f) )	/* GFX only */
	ROM_LOAD( "u49",		0x0c00000, 0x0400000, CRC(70a4ee0b) SHA1(07b09916f0366d0c6eed94a905ec0b9d6ac9e7e1) )	/* GFX + Sound */
ROM_END

ROM_START( aligatun )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE(	"ahntu45n.040",	0x000000, 0x080000, CRC(fc02cb2d) SHA1(700aa60ec0d2bb705b1335de63daae678dcb8570) )
	ROM_LOAD16_BYTE(	"ahntu44n.040",	0x000001, 0x080000, CRC(7fbea3a3) SHA1(89efa5b7908c2f010a3097954dbccd9cb7adc50c) )

	ROM_REGION( 0x1400000, REGION_GFX1, 0 ) /* GFX + Sound */
	/* 0x0000000-0x0ffffff filled in in the DRIVER_INIT */
	ROM_FILL(				0x1000000, 0x0400000, 0x0 )		/* to decode GFX as 5 bpp */

	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_DISPOSE ) /* Temporary storage */
	ROM_LOAD( "u48",		0x0000000, 0x0400000, CRC(19e03bf1) SHA1(2b3a4bb438b0aebf4f6a9fd26b071e5c9dd222b8) )	/* GFX only */
	ROM_LOAD( "u47",		0x0400000, 0x0400000, CRC(74a5a29f) SHA1(8ea2aa1f8a80c5b88ca9222c5ecc3c4794e0a160) )	/* GFX + Sound */
	ROM_LOAD( "u50",		0x0800000, 0x0400000, CRC(85daecf9) SHA1(824f6d2491075b1ef96ecd6667c5510409338a2f) )	/* GFX only */
	ROM_LOAD( "u49",		0x0c00000, 0x0400000, CRC(70a4ee0b) SHA1(07b09916f0366d0c6eed94a905ec0b9d6ac9e7e1) )	/* GFX + Sound */
ROM_END




/*============================================================================
							TOUCH & GO
  ============================================================================*/

static WRITE16_HANDLER( touchgo_coin_w )
{
	if ((offset >> 2) == 0){
		coin_counter_w(0, data & 0x01);
		coin_counter_w(1, data & 0x02);
		coin_counter_w(2, data & 0x04);
		coin_counter_w(3, data & 0x08);
	}
}

/* the game expects this value each frame to know that the DS5002FP is alive 
disabled now as the working unprotected version does not require this patch */
READ16_HANDLER ( dallas_kludge_r )
{
	return 0x0200;
}

static MEMORY_READ16_START( touchgo_readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM },			/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_r },		/* Sound Registers */
	{ 0x200000, 0x20ffff, MRA16_RAM },			/* Video RAM */
	{ 0x210000, 0x211fff, MRA16_RAM },			/* Palette */
	{ 0x218004, 0x218009, MRA16_RAM },			/* Video Registers */
	{ 0x300000, 0x300001, input_port_0_word_r },/* DSW #2 + Input 1P */
	{ 0x300002, 0x300003, input_port_1_word_r },/* DSW #1 + Input 2P */
	{ 0x300004, 0x300005, input_port_2_word_r },/* COINSW + Input 3P */
	{ 0x300006, 0x300007, input_port_3_word_r },/* SERVICESW + Input 4P */
/*	{ 0xfefffa, 0xfefffb, dallas_kludge_r },*/	/* DS5002FP related patch */
	{ 0xfe0000, 0xfeffff, MRA16_RAM },			/* Work RAM */
MEMORY_END

static MEMORY_WRITE16_START( touchgo_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },							/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_w, &gaelco_sndregs },		/* Sound Registers */
	{ 0x200000, 0x20ffff, gaelco2_vram_w, &spriteram16, &spriteram_size },	/* Video RAM */
	{ 0x210000, 0x211fff, gaelco2_palette_w, &paletteram16 },	/* Palette */
	{ 0x218004, 0x218009, MWA16_RAM, &gaelco2_vregs },			/* Video Registers */
	{ 0x500000, 0x50001f, touchgo_coin_w },						/* Coin counters */
	{ 0xfe0000, 0xfeffff, MWA16_RAM },							/* Work RAM */
MEMORY_END


INPUT_PORTS_START( touchgo )

PORT_START	/* DSW #2 + 1P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0300, "Normal" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, "Credit configuration" )
	PORT_DIPSETTING(      0x0400, "1 Credit Start/1 Credit Continue" )
	PORT_DIPSETTING(      0x0000, "2 Credits Start/1 Credit Continue" )
	PORT_DIPNAME( 0x0800, 0x0800, "Coin Slot" )
	PORT_DIPSETTING(      0x0800, "Independent" )
	PORT_DIPSETTING(      0x0000, "Common" )
#ifdef ONE_MONITOR
	PORT_DIPNAME( 0x3000, 0x3000, "Monitor Type" )
	PORT_DIPSETTING(      0x0000, "Double monitor, 4 players" )
	PORT_DIPSETTING(      0x2000, "Single monitor, 4 players" )
	PORT_DIPSETTING(      0x3000, "Single monitor, 2 players" )
#else
	PORT_DIPNAME( 0x3000, 0x0000, "Monitor Type" )
	PORT_DIPSETTING(      0x0000, "Double monitor, 4 players" )
	PORT_DIPSETTING(      0x2000, "Single monitor, 4 players" )
	PORT_DIPSETTING(      0x3000, "Single monitor, 2 players" )
#endif
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

PORT_START	/* DSW #1 + 2P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x0f00, 0x0f00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, "Disabled or Free Play (if Coin B too)" )
	PORT_DIPSETTING(      0x0300, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0600, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0900, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0500, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x0f00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0e00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0700, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0x0d00, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0b00, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0a00, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xf000, 0xf000, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, "Disabled or Free Play (if Coin A too)" )
	PORT_DIPSETTING(      0x3000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x9000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x5000, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0xf000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x7000, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(      0xd000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0xb000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_6C ) )

PORT_START	/* COINSW + 3P */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0xf000, IP_ACTIVE_LOW, IPT_UNKNOWN )

PORT_START	/* SERVICESW + 4P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_SERVICE3 ) /* go to test mode NOW */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0xf800, IP_ACTIVE_LOW, IPT_UNKNOWN )

PORT_START	/* Fake: To switch between monitors at run time */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_SERVICE4 | IPF_TOGGLE )
INPUT_PORTS_END

static struct gaelcosnd_interface touchgo_snd_interface =
{
	REGION_GFX1, 							/* memory region */
	{ 0*0x0400000, 1*0x0400000, 0, 0 },		/* start of each ROM bank */
	{ 100, 100 }							/* volume */
};

static MACHINE_DRIVER_START( touchgo )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 32000000/2)			/* 16 MHz */
	MDRV_CPU_MEMORY(touchgo_readmem, touchgo_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold, 1)

	MDRV_FRAMES_PER_SECOND(59.1)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*16, 32*16)
#ifdef ONE_MONITOR
	MDRV_VISIBLE_AREA(0, 480-1, 16, 256-1)
#else
	MDRV_VISIBLE_AREA(0, 2*480-1, 16, 256-1)
	MDRV_ASPECT_RATIO(8,3)
#endif
	MDRV_GFXDECODE(gfxdecodeinfo_0x0400000)
	MDRV_PALETTE_LENGTH(4096*16 - 16)	/* game's palette is 4096 but we allocate 15 more for shadows & highlights */

	MDRV_VIDEO_START(gaelco2_dual)
	MDRV_VIDEO_EOF(gaelco2)
	MDRV_VIDEO_UPDATE(gaelco2_dual)

	/* sound hardware */
	/* the chip is stereo, but the game sound is mono because the right channel
	   output is for cabinet 1 and the left channel output is for cabinet 2 */
#ifndef ONE_MONITOR
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
#endif
	MDRV_SOUND_ADD(GAELCO_GAE1, touchgo_snd_interface)
MACHINE_DRIVER_END

/*
PCB Layout:

REF: 950510-1
------------------------------------------------------------------------------
|                POT1                        KM428C256J-6 (x4)               |
|                POT2                                                        |
|                                            ----------------------------    |
|                                            | (Plug-In Daughterboard)  |    |
|                                            |                          |    |
|---                                         |     IC66        IC67     |    |
   |                                         |                          |    |
   |                                         |                          |    |
|---                                         |     IC65        IC69     |    |
|                                            |                          |    |
|                                            ----------------------------    |
|                                                                            |
|                                            |----------|                    |
| J                                          |          |                    |
|                                            | GAE1 501 |                    |
| A                               6264       | (QFP208) |                    |
|                                 6264       |          |                    |
| M                                          |----------|                    |
|                                                                            |
| M                       |-------------------------|                        |
|                         |                         |  40.000MHz     62256   |
| A                       |  62256  DS5002  BATT_3V |                62256   |
|                         |                         |                        |
|                         |-------------------------|                        |
|                                                                            |
|---                                    62256                                |
   |                                    62256                                |
   |  DSW1                                                                   |
|---  DSW2                                                                   |
|                                                                            |
|                                32.000MHz      MC68000P16      TG57         |
| CONN1                                                         TG56         |
|                                                                            |
| CONN4    CONN2    CONN3                                                    |
-----------------------------------------------------------------------------|
*/


ROM_START( touchgo )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE(	"tg56",	0x000000, 0x080000, CRC(6d0f5c65) SHA1(00db7a7da3ec1676169aa78fe4f08a7746c3accf) )
	ROM_LOAD16_BYTE(	"tg57",	0x000001, 0x080000, CRC(845787b5) SHA1(27c9910cd9f38328326ecb5cd093dfeb6d4f6244) )

	ROM_REGION( 0x1400000, REGION_GFX1, 0 ) /* GFX + Sound */
	/* 0x0000000-0x0ffffff filled in in the DRIVER_INIT */
	ROM_LOAD( "ic69",  0x1000000, 0x0200000, CRC(18bb12d4) SHA1(ee6e7a63b86c56d71e62db0ae5892ab3ab94b0a0) ) /* GFX only */

	ROM_REGION( 0x0c00000, REGION_GFX2, ROMREGION_DISPOSE ) /* Temporary storage */
	ROM_LOAD( "ic65",		0x0000000, 0x0400000, CRC(91b89c7c) SHA1(1c24b494b56845b0f21be40ab737f251d7683c7d) )	/* GFX only */
	ROM_LOAD( "ic66",		0x0400000, 0x0200000, CRC(52682953) SHA1(82cde061bdd827ed4a47a9a4256cd0e887ebc29d) )	/* Sound only */
	ROM_FILL(				0x0600000, 0x0200000, 0x0 )			/* Empty */
	ROM_LOAD( "ic67",		0x0800000, 0x0400000, CRC(c0a2ce5b) SHA1(94b024373c7c546c0f4fe9737639f02e9c7ebbdb) )	/* GFX only */
ROM_END

ROM_START( touchgok ) /* REF: 950510-1 - ds5002fp unpopulated, game is unprotected */
	ROM_REGION( 0x100000, REGION_CPU1, 0 )    /* 68000 code */
	ROM_LOAD16_BYTE( "56.IC56", 0x000000, 0x080000, CRC(cbb87505) SHA1(f19832af60fb6273c3263ebdd93bb7705ab61e20) )
	ROM_LOAD16_BYTE( "57.IC57", 0x000001, 0x080000, CRC(36bcc7e7) SHA1(2fff881ba0a99ebcfe3c03fdc61f4bf40e152c7f))

	ROM_REGION( 0x1400000, REGION_GFX1, 0 ) /* GFX + Sound */
	/* 0x0000000-0x0ffffff filled in in the DRIVER_INIT */
	ROM_LOAD( "ic69",  0x1000000, 0x0200000, CRC(18bb12d4) SHA1(ee6e7a63b86c56d71e62db0ae5892ab3ab94b0a0) ) /* GFX only */

 	ROM_REGION( 0x0c00000, REGION_GFX2, ROMREGION_DISPOSE ) /* Temporary storage */
	ROM_LOAD( "ic65",		0x0000000, 0x0400000, CRC(91b89c7c) SHA1(1c24b494b56845b0f21be40ab737f251d7683c7d) )	/* GFX only */
	ROM_LOAD( "ic66",		0x0400000, 0x0200000, CRC(52682953) SHA1(82cde061bdd827ed4a47a9a4256cd0e887ebc29d) )	/* Sound only */
	ROM_FILL(				0x0600000, 0x0200000, 0x0 )			/* Empty */
	ROM_LOAD( "ic67",		0x0800000, 0x0400000, CRC(c0a2ce5b) SHA1(94b024373c7c546c0f4fe9737639f02e9c7ebbdb) )	/* GFX only */
ROM_END


/*============================================================================
							SNOW BOARD
  ============================================================================*/

static MEMORY_READ16_START( snowboar_readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM },			/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_r },		/* Sound Registers */
	{ 0x200000, 0x20ffff, MRA16_RAM },			/* Video RAM */
	{ 0x210000, 0x211fff, MRA16_RAM },			/* Palette */
	{ 0x212000, 0x213fff, MRA16_RAM },			/* Extra RAM */
	{ 0x218004, 0x218009, MRA16_RAM },			/* Video Registers */
	{ 0x300000, 0x300001, input_port_0_word_r },/* Input 1P */
	{ 0x300010, 0x300011, input_port_1_word_r },/* Input 2P */
	{ 0x300020, 0x300021, gaelco2_eeprom_r },	/* EEPROM status + read */
	{ 0x310000, 0x31ffff, snowboar_protection_r },/* Protection */
	{ 0xfe0000, 0xfeffff, MRA16_RAM },			/* Work RAM */
MEMORY_END

static MEMORY_WRITE16_START( snowboar_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },							/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_w, &gaelco_sndregs },		/* Sound Registers */
	{ 0x200000, 0x20ffff, gaelco2_vram_w, &spriteram16, &spriteram_size },	/* Video RAM */
	{ 0x210000, 0x211fff, gaelco2_palette_w, &paletteram16 },	/* Palette */
	{ 0x212000, 0x213fff, MWA16_RAM },							/* Extra RAM */
	{ 0x218004, 0x218009, MWA16_RAM, &gaelco2_vregs },			/* Video Registers */
	{ 0x300000, 0x300003, gaelco2_coin2_w },					/* Coin Counters */
	{ 0x300008, 0x300009, gaelco2_eeprom_data_w },				/* EEPROM data */
	{ 0x30000a, 0x30000b, gaelco2_eeprom_sk_w },				/* EEPROM serial clock */
	{ 0x30000c, 0x30000d, gaelco2_eeprom_cs_w },				/* EEPROM chip select */
	{ 0x310000, 0x31ffff, snowboar_protection_w, &snowboar_protection },/* Protection */
	{ 0xfe0000, 0xfeffff, MWA16_RAM },							/* Work RAM */
MEMORY_END


INPUT_PORTS_START( snowboar )
PORT_START	/* 1P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0xff80, IP_ACTIVE_LOW, IPT_UNKNOWN )

PORT_START	/* 2P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0xff80, IP_ACTIVE_LOW, IPT_UNKNOWN )

PORT_START	/* COINSW & Service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW)	/* go to service mode NOW */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	/* bits 6 & 7 are used for accessing the NVRAM */
INPUT_PORTS_END

static struct gaelcosnd_interface snowboar_snd_interface =
{
	REGION_GFX1, 							/* memory region */
	{ 0*0x0400000, 1*0x0400000, 0, 0 },		/* start of each ROM bank */
	{ 100, 100 }							/* volume */
};

static MACHINE_DRIVER_START( snowboar )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 30000000/2)			/* 15 MHz */
	MDRV_CPU_MEMORY(snowboar_readmem, snowboar_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold, 1)

	MDRV_FRAMES_PER_SECOND(59.1)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_NVRAM_HANDLER(gaelco2)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*16, 32*16)
	MDRV_VISIBLE_AREA(0, 384-1, 16, 256-1)
	MDRV_GFXDECODE(gfxdecodeinfo_0x0400000)
	MDRV_PALETTE_LENGTH(4096*16 - 16)	/* game's palette is 4096 but we allocate 15 more for shadows & highlights */

	MDRV_VIDEO_START(gaelco2)
	MDRV_VIDEO_EOF(gaelco2)
	MDRV_VIDEO_UPDATE(gaelco2)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(GAELCO_CG1V, snowboar_snd_interface)
MACHINE_DRIVER_END


/*
PCB Layout:

REF: 960419/1
------------------------------------------------------------------------------
|                                   KM428C256J-6 (x2)                        |
|                                                                            |
|                POT1                                                        |
|---                                                                         |
   |         SW1                                                   IC43      |
   |                   34.000MHz          |----------|                       |
|---        93C66                         |          |             IC44      |
|                                         | GC-1V    |                       |
| J                            6264       |   366    |             IC45      |
|                              6264       |          |                       |
| A                                       |----------|             IC46      |
|                                                                            |
| M                                                                          |
|                                                                            |
| M                         62256                                    62256   |
|                                                                    62256   |
| A                         62256                                    62256   |
|                                            |----------|                    |
|                                30.000MHz   | Lattice  |                    |
|---                                         | IspLSI   |                    |
   |                                         |   1016   |          SB55      |
   |                                         |----------|                    |
|---                                        |------------|           62256   |
|                                           |            |                   |
|                                           |  MC68HC000 |         SB53      |
|                                           |    FN16    |                   |
|                                           |------------|                   |
-----------------------------------------------------------------------------|
*/

ROM_START( snowboar )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE(	"sb53",	0x000000, 0x080000, CRC(e4eaefd4) SHA1(c7de2ae3a4a919fbe16d4997e3f9e2303b8c96b1) )
	ROM_LOAD16_BYTE(	"sb55",	0x000001, 0x080000, CRC(e2476994) SHA1(2ad18652a1fc6ac058c8399373fb77e7a81d5bbd) )

	ROM_REGION( 0x1400000, REGION_GFX1, 0 )	/* GFX + Sound */
	/* 0x0000000-0x0ffffff filled in in the DRIVER_INIT */
	ROM_LOAD( "sb43",		0x1000000, 0x0200000, CRC(afce54ed) SHA1(1d2933d64790612918adbaabcd2a82dad79953c9) )	/* GFX only */
	ROM_FILL(				0x1200000, 0x0200000, 0x0 )			/* Empty */

	ROM_REGION( 0x0c00000, REGION_GFX2, ROMREGION_DISPOSE ) /* Temporary storage */
	ROM_LOAD( "sb44",		0x0000000, 0x0400000, CRC(1bbe88bc) SHA1(15bce9ada2b742ba4d537fa8efc0f29f661bff00) )	/* GFX only */
	ROM_LOAD( "sb45",		0x0400000, 0x0400000, CRC(373983d9) SHA1(05e35a8b27cab469885f0ec2a5df200a366b50a1) )	/* Sound only */
	ROM_LOAD( "sb46",		0x0800000, 0x0400000, CRC(22e7c648) SHA1(baddb9bc13accd83bea61533d7286cf61cd89279) )	/* GFX only */
ROM_END

ROM_START( snowbalt )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE(	"sb.53",	0x000000, 0x080000, CRC(4742749e) SHA1(933e39893ab74895ae4a99a932f8245a03ea0b5d) )
	ROM_LOAD16_BYTE(	"sb.55",	0x000001, 0x080000, CRC(6ddc431f) SHA1(8801c0cf1711bb956447ba1e631db28bd075caea) )

	ROM_REGION( 0x1400000, REGION_GFX1, 0 )	/* GFX + Sound */
	ROM_LOAD( "sb.a0",		0x0000000, 0x0080000, CRC(aa476e44) SHA1(2b87689489b9619e9e5ca32c3e3d2aec8ef31c88) )	/* GFX only */
	ROM_LOAD( "sb.a1",		0x0080000, 0x0080000, CRC(6bc99195) SHA1(276e9383fac9cb5141b23ffdf381b0d7e60a6861) )	/* GFX only */
	ROM_LOAD( "sb.a2",		0x0100000, 0x0080000, CRC(fae2ebba) SHA1(653a12846abe4de36f5565c3bf849fce7c2893b6) )	/* GFX only */
	ROM_LOAD( "sb.a3",		0x0180000, 0x0080000, CRC(17ed9cf8) SHA1(c6cab61bbba3b2b1d06b64a68313946299205cc5) )	/* GFX only */
	ROM_LOAD( "sb.a4",		0x0200000, 0x0080000, CRC(2ba3a5c8) SHA1(93de0382cbb41806ae3349ce7cfecdc1404bfb88) )	/* Sound only */
	ROM_LOAD( "sb.a5",		0x0280000, 0x0080000, CRC(ae011eb3) SHA1(17223404640c55637364fa6e51cf07d8e64df085) )	/* Sound only */
	ROM_FILL(				0x0300000, 0x0100000, 0x0 )			/* Empty */
	ROM_LOAD( "sb.b0",		0x0400000, 0x0080000, CRC(96c714cd) SHA1(c6225c43b88531a70436cc8a631b8ba401903e45) )  /* GFX only */
	ROM_LOAD( "sb.b1",		0x0480000, 0x0080000, CRC(39a4c30c) SHA1(4598a68ef41483ba372aa3a40383de8eb70d706e) )	/* GFX only */
	ROM_LOAD( "sb.b2",		0x0500000, 0x0080000, CRC(b58fcdd6) SHA1(21a8c00778be77165f89421fb2e3123244cf02c6) )	/* GFX only */
	ROM_LOAD( "sb.b3",		0x0580000, 0x0080000, CRC(96afdebf) SHA1(880cfb365efa93bbee882aeb483ad6d75d8b7430) )	/* GFX only */
	ROM_LOAD( "sb.b4",		0x0600000, 0x0080000, CRC(e62cf8df) SHA1(8df8df45d99967e52dcec5b589246799f7a39601) )	/* Sound only */
	ROM_LOAD( "sb.b5",		0x0680000, 0x0080000, CRC(caa90856) SHA1(a8f18a878b211366faaf66911c09d0452770cc3f) )	/* Sound only */
	ROM_FILL(				0x0700000, 0x0100000, 0x0 )			/* Empty */
	ROM_LOAD( "sb.c0",		0x0800000, 0x0080000, CRC(c9d57a71) SHA1(4e8b7d821e31afc0750db283470f9c76bceb54da) )	/* GFX only */
	ROM_LOAD( "sb.c1",		0x0880000, 0x0080000, CRC(1d14a3d4) SHA1(eb89cadfe331f77dbc0463151574ba801c248238) )	/* GFX only */
	ROM_LOAD( "sb.c2",		0x0900000, 0x0080000, CRC(55026352) SHA1(7b92f45624dbd122c29e44f82c3c2ffded190efa) )	/* GFX only */
	ROM_LOAD( "sb.c3",		0x0980000, 0x0080000, CRC(d9b62dee) SHA1(409ab4d9a6f9341cf59510c130c705d1ec42d1b3) )	/* GFX only */
	ROM_FILL(				0x0a00000, 0x0200000, 0x0 )			/* Empty */
	ROM_LOAD( "sb.d0",		0x0c00000, 0x0080000, CRC(7434c1ae) SHA1(8e0e6567a461c694a8ba2de5d4cf9ad73e0c83c8) )	/* GFX only */
	ROM_LOAD( "sb.d1",		0x0c80000, 0x0080000, CRC(f00cc6c8) SHA1(b4835e2187e1a985993471d09495cbc1f5cd9417) )	/* GFX only */
	ROM_LOAD( "sb.d2",		0x0d00000, 0x0080000, CRC(019f9aec) SHA1(1a97b84ebbf57e860792ef7a7dc6f51553ae3e26) )	/* GFX only */
	ROM_LOAD( "sb.d3",		0x0d80000, 0x0080000, CRC(d05bd286) SHA1(9eff6f5a4755375b7a16b9d4967a0df933e1b9c4) )	/* GFX only */
	ROM_FILL(				0x0e00000, 0x0200000, 0x0 )			/* Empty */
	ROM_LOAD( "sb.e0",		0x1000000, 0x0080000, CRC(e6195323) SHA1(5ba1cb750dd8cfd0721905174bda6cfbf8c8e694) )	/* GFX only */
	ROM_LOAD( "sb.e1",		0x1080000, 0x0080000, CRC(9f38910b) SHA1(0243c19c7b1bdd3361fc6e177c64528bacafcc33) )	/* GFX only */
	ROM_LOAD( "sb.e2",		0x1100000, 0x0080000, CRC(f5948c6c) SHA1(91bba817ced194b02885ce84b7a8132ef5ca631a) )	/* GFX only */
	ROM_LOAD( "sb.e3",		0x1180000, 0x0080000, CRC(4baa678f) SHA1(a7fbbd687e2d8d7e96207c8ace0799a3cc9c3272) )	/* GFX only */
	ROM_FILL(				0x1200000, 0x0200000, 0x0 )			/* Empty */
ROM_END



/*============================================================================
							WORLD RALLY 2
  ============================================================================*/

static MEMORY_READ16_START( wrally2_readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM },			/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_r },		/* Sound Registers */
	{ 0x200000, 0x20ffff, MRA16_RAM },			/* Video RAM */
	{ 0x210000, 0x211fff, MRA16_RAM },			/* Palette */
	{ 0x212000, 0x213fff, MRA16_RAM },			/* Extra RAM */
	{ 0x218004, 0x218009, MRA16_RAM },			/* Video Registers */
	{ 0x300000, 0x300001, input_port_0_word_r },/* DIPSW #2 + Inputs 1P */
	{ 0x300002, 0x300003, input_port_1_word_r },/* DIPSW #1 */
	{ 0x300004, 0x300005, input_port_2_word_r },/* Inputs 2P + COINSW */
	{ 0x300006, 0x300007, input_port_3_word_r },/* SERVICESW */
	{ 0xfe0000, 0xfeffff, MRA16_RAM },			/* Work RAM */
MEMORY_END

static MEMORY_WRITE16_START( wrally2_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },							/* ROM */
	{ 0x202890, 0x2028ff, gaelcosnd_w, &gaelco_sndregs },		/* Sound Registers */
	{ 0x200000, 0x20ffff, gaelco2_vram_w, &spriteram16, &spriteram_size },	/* Video RAM */
	{ 0x210000, 0x211fff, gaelco2_palette_w, &paletteram16 },	/* Palette */
	{ 0x212000, 0x213fff, MWA16_RAM },							/* Extra RAM */
	{ 0x218004, 0x218009, MWA16_RAM, &gaelco2_vregs },			/* Video Registers */
	{ 0x400000, 0x400011, wrally2_coin_w },						/* Coin Counters */
	{ 0x400028, 0x400031, MWA16_NOP },							/* Pot Wheel input bit select */
	{ 0xfe0000, 0xfeffff, MWA16_RAM },							/* Work RAM */
MEMORY_END


INPUT_PORTS_START( wrally2 )
PORT_START	/* DIPSW #2 + 1P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	/* 0x0040 - Pot Wheel 1P (16 bit serial input) */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0200, 0x0000, "Coin mechanism" )
	PORT_DIPSETTING(      0x0000, "Common" )
	PORT_DIPSETTING(      0x0200, "Independent" )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "Cabinet 1 Control Configuration" )
	PORT_DIPSETTING(      0x0000, "Pot Wheel" )		/* TO-DO */
	PORT_DIPSETTING(      0x0800, "Joystick" )
	PORT_DIPNAME( 0x1000, 0x1000, "Cabinet 2 Control Configuration" )
	PORT_DIPSETTING(      0x0000, "Pot Wheel" )		/* TO-DO */
	PORT_DIPSETTING(      0x1000, "Joystick" )
#ifdef ONE_MONITOR
	PORT_DIPNAME( 0x2000, 0x0000, "Monitors" )
	PORT_DIPSETTING(      0x0000, "One" )
	PORT_DIPSETTING(      0x2000, "Two" )
#else
	PORT_DIPNAME( 0x2000, 0x2000, "Monitors" )
	PORT_DIPSETTING(      0x0000, "One" )
	PORT_DIPSETTING(      0x2000, "Two" )
#endif
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x4000, "Easy" )
	PORT_DIPSETTING(      0xc000, "Normal" )
	PORT_DIPSETTING(      0x8000, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )

PORT_START	/* DIPSW #1 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "Credit configuration" )
	PORT_DIPSETTING(    0x0000, "Start 2C/Continue 1C" )
	PORT_DIPSETTING(    0x0200, "Start 1C/Continue 1C" )
	PORT_DIPNAME( 0x1c00, 0x1c00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x1800, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x1c00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x1400, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0xe000, 0xe000, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_4C ) )

PORT_START	/* 2P INPUTS */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	/* 0x0040 - Pot Wheel 2P (16 bit serial input) */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xfa00, IP_ACTIVE_LOW, IPT_UNKNOWN )

PORT_START	/* SERVICESW */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_SERVICE3 ) /* go to test mode NOW */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0xf800, IP_ACTIVE_LOW, IPT_UNKNOWN )

PORT_START	/* Fake: To switch between monitors at run time */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_SERVICE4 | IPF_TOGGLE )
INPUT_PORTS_END

static struct gaelcosnd_interface wrally2_snd_interface =
{
	REGION_GFX1, 						/* memory region */
	{ 0*0x0200000, 1*0x0200000, 0, 0 },	/* start of each ROM bank */
	{ 100, 100 }						/* volume */
};

static MACHINE_DRIVER_START( wrally2 )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 26000000/2)			/* 13 MHz */
	MDRV_CPU_MEMORY(wrally2_readmem, wrally2_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold, 1)

	MDRV_FRAMES_PER_SECOND(59.1)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_NVRAM_HANDLER(gaelco2)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*16, 32*16)
#ifdef ONE_MONITOR
	MDRV_VISIBLE_AREA(0, 384-1, 16, 256-1)
#else
	MDRV_VISIBLE_AREA(0, 2*384-1, 16, 256-1)
	MDRV_ASPECT_RATIO(8,3)
#endif
	MDRV_GFXDECODE(gfxdecodeinfo_0x0200000)
	MDRV_PALETTE_LENGTH(4096*16 - 16)	/* game's palette is 4096 but we allocate 15 more for shadows & highlights */

	MDRV_VIDEO_START(gaelco2_dual)
	MDRV_VIDEO_EOF(gaelco2)
	MDRV_VIDEO_UPDATE(gaelco2_dual)

	/* sound hardware */
	/* the chip is stereo, but the game sound is mono because the right channel
	   output is for cabinet 1 and the left channel output is for cabinet 2 */
#ifndef ONE_MONITOR
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
#endif
	MDRV_SOUND_ADD(GAELCO_GAE1, wrally2_snd_interface)
MACHINE_DRIVER_END

/*
PCB Layout:

REF: 950510
------------------------------------------------------------------------------
|                POT1                        KM428C256J-6 (x4)               |
|                POT2                                                        |
|                                            ----------------------------    |
|                                            | (Plug-In Daughterboard)  |    |
|                                            | WR2.1   WR2.9    WR2.16  |    |
|---                                         | WR2.2   WR2.10   WR2.17  |    |
   |                                         |         WR2.11   WR2.18  |    |
   |                                         |         WR2.12   WR2.19  |    |
|---                                         |         WR2.13   WR2.20  |    |
|                                            |         WR2.14   WR2.21  |    |
|                                            ----------------------------    |
|                                                                            |
|                                            |----------|                    |
| J                                          |          |                    |
|                                            | GAE1 449 |                    |
| A                               6264       | (QFP208) |                    |
|                                 6264       |          |                    |
| M                                          |----------|                    |
|                                                                            |
| M                       |-------------------------|                        |
|                         |                         |  34.000MHz     62256   |
| A                       |  62256  DS5002  BATT_3V |                62256   |
|                         |                         |                        |
|                         |-------------------------|                        |
|                                                                            |
|---                                    62256                                |
   |                                    62256                                |
   |  DSW1                                                                   |
|---  DSW2                                                                   |
|                                                                            |
|                                26.000MHz      MC68000P12      WR2.63       |
| CONN1                                                         WR2.64       |
|                                                                            |
| CONN2    CONN3                                                             |
-----------------------------------------------------------------------------|


Notes
-----
All ROMs are type 27C040
CONN1: RGBSync OUT (additional to JAMMA RGBSync)
CONN2: Right speaker sound OUT (for second cabinat)
CONN3: For connection of wheel etc
POT1/2: Volume adjust of left/right channel
*/

ROM_START( wrally2 )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "wr2.64",	0x000000, 0x080000, CRC(4cdf4e1e) SHA1(a3b3ff4a70336b61c7bba5d518527bf4bd901867) )
	ROM_LOAD16_BYTE( "wr2.63",	0x000001, 0x080000, CRC(94887c9f) SHA1(ad09f1fbeff4c3ba47f72346d261b22fa6a51457) )

	ROM_REGION( 0x0a00000, REGION_GFX1, 0 )	/* GFX + Sound */
	ROM_LOAD( "wr2.16d",	0x0000000, 0x0080000, CRC(ad26086b) SHA1(487ffaaca57c9d030fc486b8cae6735ee40a0ac3) ) 	/* GFX only */
	ROM_LOAD( "wr2.17d",	0x0080000, 0x0080000, CRC(c1ec0745) SHA1(a6c3ce9c889e6a53f4155f54d6655825af34a35b) ) 	/* GFX only */
	ROM_LOAD( "wr2.18d",	0x0100000, 0x0080000, CRC(e3617814) SHA1(9f9514052bb07d7e243f33b11bae409a444b7d9f) ) 	/* Sound only */
	ROM_LOAD( "wr2.19d",	0x0180000, 0x0080000, CRC(2dae988c) SHA1(a585e10b0e1519b828738b0b90698f8600082250) ) 	/* Sound only */
	ROM_LOAD( "wr2.09d",	0x0200000, 0x0080000, CRC(372d70c8) SHA1(a6d8419765eab1fa20c6d3ddff9d026adaab5cd9) ) 	/* GFX only */
	ROM_LOAD( "wr2.10d",	0x0280000, 0x0080000, CRC(5db67eb3) SHA1(faa58dafa26befb3291e5185ee04c39ce3b45b3f) ) 	/* GFX only */
	ROM_LOAD( "wr2.11d",	0x0300000, 0x0080000, CRC(ae66b97c) SHA1(bd0eba0b1c77864e06a9e136cfd834b35f200683) ) 	/* Sound only */
	ROM_LOAD( "wr2.12d",	0x0380000, 0x0080000, CRC(6dbdaa95) SHA1(f23df65e3df92d79f7b1e99d611c067a79fc849a) ) 	/* Sound only */
	ROM_LOAD( "wr2.01d",	0x0400000, 0x0080000, CRC(753a138d) SHA1(b05348af6d25e95208fc39007eb2082b759384e8) ) 	/* GFX only */
	ROM_LOAD( "wr2.02d",	0x0480000, 0x0080000, CRC(9c2a723c) SHA1(5259c8fa1ad73518e89a8df6e76a565b8f8799e3) ) 	/* GFX only */
	ROM_FILL(				0x0500000, 0x0100000, 0x0 )			/* Empty */
	ROM_LOAD( "wr2.20d",	0x0600000, 0x0080000, CRC(4f7ade84) SHA1(c8efcd4bcb1f2ad6ab8104ec0daea8324cefd3fd) ) 	/* GFX only */
	ROM_LOAD( "wr2.13d",	0x0680000, 0x0080000, CRC(a4cd32f8) SHA1(bc4cc73b7a58aecd735bf55bb5062baa6dd22f83) ) 	/* GFX only */
	ROM_FILL(				0x0700000, 0x0100000, 0x0 )			/* Empty */
	ROM_LOAD( "wr2.21d",	0x0800000, 0x0080000, CRC(899b0583) SHA1(a313e679980cc4da22bc70f2c7c9685af4f3d6df) ) 	/* GFX only */
	ROM_LOAD( "wr2.14d",	0x0880000, 0x0080000, CRC(6eb781d5) SHA1(d5c13db88e6de606b34805391cef9f3fbf09fac4) ) 	/* GFX only */
	ROM_FILL(				0x0900000, 0x0100000, 0x0 )			/* Empty */
ROM_END



GAMEX(1994, aligator, 0,        alighunt, alighunt, alighunt, ROT0, "Gaelco", "Alligator Hunt", GAME_UNEMULATED_PROTECTION | GAME_NOT_WORKING )
GAME( 1994, aligatun, aligator, alighunt, alighunt, alighunt, ROT0, "Gaelco", "Alligator Hunt (unprotected)" )
GAMEX(1995, touchgo,  0,        touchgo,  touchgo,  touchgo,  ROT0, "Gaelco", "Touch and Go", GAME_UNEMULATED_PROTECTION | GAME_NOT_WORKING )
GAMEX(1995, touchgok, touchgo,  touchgo,  touchgo,  touchgo,  ROT0, "Gaelco", "Touch and Go (Korea, unprotected)", GAME_IMPERFECT_SOUND )
GAMEX(1995, wrally2,  0,        wrally2,  wrally2,  0,        ROT0, "Gaelco", "World Rally 2 - Twin Racing", GAME_UNEMULATED_PROTECTION | GAME_NOT_WORKING )
GAME( 1996, maniacsq, 0,        maniacsq, maniacsq, 0,        ROT0, "Gaelco", "Maniac Square (unprotected)" )
GAME( 1996, snowboar, 0,        snowboar, snowboar, snowboar, ROT0, "Gaelco", "Snow Board Championship (set 1)" )
GAME( 1996, snowbalt, snowboar, snowboar, snowboar, 0,        ROT0, "Gaelco", "Snow Board Championship (set 2)" )
GAME( 1998, bang,     0,        bang,     bang,     bang,     ROT0, "Gaelco", "Bang!" )
GAME( 1998, bangj,    bang,     bang,     bang,     bang,     ROT0, "Gaelco", "Gun Gabacho (Japan)" )
