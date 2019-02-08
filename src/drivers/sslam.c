/* Super Slam (c)1993 Playmark */

/*

still some unknown reads / writes (it writes all over the place ...)
odd problem with sprites, and wrong text on tournament screen
sound needs hooking up (if possible, or is it like the other playmark games?)
inputs / dsw's need finishing / verifying

*/

/*

Here's the info about this dump:

Name:            Super Slam
Manufacturer:    PlayMark
Year:            1993
Date Dumped:     18-07-2002 (DD-MM-YYYY)

CPU:             68000
SOUND:           OKIM6295
GFX:             Unknown

Country:         Italy

About the game:

This is a Tennis game :) Unfortunately my board does not make music or
sound anymore :( Hope the it_20.bin (eprom at right of the Oki chip) is
ok, at least it always reads the same in the eprom burner and it's not
filled with 0s :) You can play with some boys and girls, an old man,
a small kid and even with a dog! And remember, Winners don't use Drugs ;)

*/

#include "driver.h"

data16_t *sslam_bg_tileram, *sslam_tx_tileram, *sslam_md_tileram;
data16_t *sslam_spriteram, *sslam_regs;


/* vidhrdw/playmark.c */
WRITE16_HANDLER( bigtwin_paletteram_w );

/* vidhrdw/sslam.c */
WRITE16_HANDLER( sslam_tx_tileram_w );
WRITE16_HANDLER( sslam_md_tileram_w );
WRITE16_HANDLER( sslam_bg_tileram_w );
VIDEO_START(sslam);
VIDEO_UPDATE(sslam);


/* Memory Maps */

/* these will need verifying .. the game writes all over the place ... */

static MEMORY_READ16_START( sslam_readmem )
	{ 0x000000, 0x0003ff, MRA16_ROM },

	{ 0x000400, 0x0107ff, MRA16_RAM },
	{ 0x020000, 0x07ffff, MRA16_RAM },

	{ 0x100000, 0x10ffff, MRA16_RAM },
	{ 0x280000, 0x280fff, MRA16_RAM },
	{ 0x201000, 0x220fff, MRA16_RAM },

	{ 0x300010, 0x300011, input_port_0_word_r },
	{ 0x300012, 0x300013, input_port_1_word_r },
	{ 0x300014, 0x300015, input_port_2_word_r },
	{ 0x300016, 0x300017, input_port_3_word_r },
	{ 0x300018, 0x300019, input_port_4_word_r },

	{ 0x30001a, 0x30001b, input_port_5_word_r },
	{ 0x30001c, 0x30001d, input_port_6_word_r },

	{ 0xc00000, 0xcfffff, MRA16_ROM },

	{ 0xfe0000, 0xfeffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( sslam_writemem )
	{ 0x000000, 0x0003ff, MWA16_ROM },

	{ 0x000400, 0x0107ff, MWA16_RAM },
	{ 0x020000, 0x07ffff, MWA16_RAM },

	{ 0x100000, 0x103fff, sslam_bg_tileram_w, &sslam_bg_tileram },
	{ 0x104000, 0x107fff, sslam_md_tileram_w, &sslam_md_tileram },
	{ 0x108000, 0x10ffff, sslam_tx_tileram_w, &sslam_tx_tileram },

	{ 0x110000, 0x11000f, MWA16_RAM, &sslam_regs },

	{ 0x280000, 0x280fff, bigtwin_paletteram_w, &paletteram16 },
	{ 0x201000, 0x220fff, MWA16_RAM, &sslam_spriteram }, /* probably not all of it .. */

	{ 0xc00000, 0xcfffff, MWA16_ROM },

	{ 0xfe0000, 0xfeffff, MWA16_RAM },
MEMORY_END

/* Input Ports */

INPUT_PORTS_START( sslam )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN4 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START4 )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Unknown ) )		/* 0x000522 = 0x00400e*/
	PORT_DIPSETTING(    0x03, "0" )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )		/* 0x00052e = 0x004004 - code at 0xc069b2 and 0xc069d6*/
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Time" )				/* 0x000530 = 0x004006*/
	PORT_DIPSETTING(    0x08, "Table 1" )
	PORT_DIPSETTING(    0x00, "Table 2" )
	PORT_DIPNAME( 0x30, 0x30, "Starting Score" )
	PORT_DIPSETTING(    0x30, "4-4" )
	PORT_DIPSETTING(    0x20, "3-4" )
	PORT_DIPSETTING(    0x10, "3-3" )
	PORT_DIPSETTING(    0x00, "0-0" )
	PORT_DIPNAME( 0x40, 0x40, "Max Players" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )	/* 0x000524 - code at 0xc0eb4e*/
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x07, 0x07, "Coin(s) per Player" )
	PORT_DIPSETTING(    0x07, "1" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0x38, 0x38, "Coin Multiplicator" )
	PORT_DIPSETTING(    0x38, "*1" )
	PORT_DIPSETTING(    0x30, "*2" )
	PORT_DIPSETTING(    0x28, "*3" )
	PORT_DIPSETTING(    0x20, "*4" )
	PORT_DIPSETTING(    0x18, "*5" )
	PORT_DIPSETTING(    0x10, "*6" )
	PORT_DIPSETTING(    0x08, "*7" )
	PORT_DIPSETTING(    0x00, "*8" )
	PORT_DIPNAME( 0x40, 0x00, "On Time Up" )
	PORT_DIPSETTING(    0x00, "End After Point" )
	PORT_DIPSETTING(    0x40, "End After Game" )
	PORT_DIPNAME( 0x80, 0x80, "Coin Slots" )
	PORT_DIPSETTING(    0x80, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
INPUT_PORTS_END

/* GFX Decodes */

static struct GfxLayout tiles8x8_layout =
{
	8,8,
	RGN_FRAC(1,4),
	4,
	{ 0, RGN_FRAC(1,4), RGN_FRAC(2,4), RGN_FRAC(3,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout tiles16x16_layout =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ 0, RGN_FRAC(1,4), RGN_FRAC(2,4), RGN_FRAC(3,4) },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*16
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tiles16x16_layout, 0    , 16 }, /* bg */
	{ REGION_GFX1, 0, &tiles16x16_layout, 0x100, 16 }, /* mid */
	{ REGION_GFX1, 0, &tiles8x8_layout,   0x200, 16 }, /* tx */
	{ REGION_GFX2, 0, &tiles8x8_layout,   0x300, 16 }, /* spr */
	{ -1 }
};

/* Machine Driver */

static MACHINE_DRIVER_START( sslam )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)	/* 12 MHz? */
	MDRV_CPU_MEMORY(sslam_readmem,sslam_writemem)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(1*8, 39*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(0x800)

	MDRV_VIDEO_START(sslam)
	MDRV_VIDEO_UPDATE(sslam)

	/* sound hardware */
/*	MDRV_SOUND_ADD(OKIM6295, okim6295_interface)*/
MACHINE_DRIVER_END

ROM_START( sslam )
	ROM_REGION( 0xD00000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "it_21.bin", 0x00000, 0x80000, CRC(1ce52917) SHA1(b9b1d14ea44c248ce6e615c5c553c0d485c1302b) )
	ROM_RELOAD ( 0xc00000, 0x80000 )
	ROM_LOAD16_BYTE( "it_22.bin", 0x00001, 0x80000, CRC(51c56828) SHA1(d71d64b0268c156456bed64b4c13b98181fa3e0f) )
	ROM_RELOAD ( 0xc00001, 0x80000 )

	ROM_REGION( 0x200000, REGION_GFX1, 0 ) /* Bg */
	ROM_LOAD( "it_23.bin", 0x180000, 0x80000, CRC(8e15fb9d) SHA1(47917d8aac1bce2e15f36904f5c2534e5b80236b) )
	ROM_LOAD( "it_24.bin", 0x100000, 0x80000, CRC(8d18bdc6) SHA1(cacc4f475f85438a00ead4911730202e995983a7) )
	ROM_LOAD( "it_25.bin", 0x080000, 0x80000, CRC(6928065c) SHA1(ad5b1889bebf0358df0295d6041b798ac53ac625) )
	ROM_LOAD( "it_26.bin", 0x000000, 0x80000, CRC(64ecdde9) SHA1(576ba1169d90970622249e532baa4209bf12de5a) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* Sprites */
	ROM_LOAD( "it_27.bin", 0x000000, 0x80000, CRC(19bb89dd) SHA1(c2a0c32d350a193d366b5086502998281fd0bec4) )
	ROM_LOAD( "it_28.bin", 0x080000, 0x80000, CRC(d50d86c7) SHA1(7ecbcc03851a8174610f7f5ad889e40543da928e) )
	ROM_LOAD( "it_29.bin", 0x100000, 0x80000, CRC(681b8ac8) SHA1(ebfeffc091f53af246311574b9c5d83d2716a7be) )
	ROM_LOAD( "it_30.bin", 0x180000, 0x80000, CRC(e41f89e3) SHA1(e4b39411a4cea6aa6c01564f74bb8e432d382a73) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* OKI Samples */
	ROM_LOAD( "it_20.bin", 0x00000, 0x80000, CRC(d0a9245f) SHA1(2e840cdd7bdfe7c6f986daf88576de0559597499) )
ROM_END

GAMEX(1993, sslam, 0, sslam, sslam, 0, ROT0, "Playmark", "Super Slam", GAME_NO_SOUND | GAME_IMPERFECT_GRAPHICS )
