/***************************************************************************

							  -= Paradise =-

					driver by	Luca Elia (l.elia@tin.it)


CPU          :  Z8400B
Video Chips  :	TPC1024AFN-084C
Sound Chips  :	2 x AR17961 (OKI M6295)

Notes:

I'm not sure it's working correctly:

- The high scores table can't be entered !?
- The chance to play a bonus game is very slim. I think I got to play
  a couple in total. Is there a way to trigger them !?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "paradise.h"

/***************************************************************************

								Memory Maps

***************************************************************************/

static WRITE_HANDLER( paradise_rombank_w )
{
	int bank = data;
	int bank_n = memory_region_length(REGION_CPU1)/0x4000 - 1;
	if (bank >= bank_n)
	{
		logerror("PC %04X - invalid rom bank %x\n",activecpu_get_pc(),bank);
		bank %= bank_n;
	}

	if (bank >= 3)	bank+=1;
	cpu_setbank(1, memory_region(REGION_CPU1) + bank * 0x4000);
}

static WRITE_HANDLER( paradise_okibank_w )
{
	if (data & ~0x02)	logerror("CPU #0 - PC %04X: unknown oki bank bits %02X\n",activecpu_get_pc(),data);
	OKIM6295_set_bank_base(1, (data & 0x02) ? 0x40000 : 0);
}


static MEMORY_READ_START( paradise_readmem )
	{ 0x0000, 0x7fff, MRA_ROM		},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1		},	// ROM (banked)
	{ 0xc000, 0xffff, MRA_RAM		},	// RAM
MEMORY_END

static MEMORY_WRITE_START( paradise_writemem )
	{ 0x0000, 0x7fff, MWA_ROM		},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM		},	// ROM (banked)
	{ 0xc000, 0xc7ff, paradise_vram_2_w,&paradise_vram_2	},	// Background
	{ 0xc800, 0xcfff, paradise_vram_1_w,&paradise_vram_1	},	// Midground
	{ 0xd000, 0xd7ff, paradise_vram_0_w,&paradise_vram_0	},	// Foreground
	{ 0xd800, 0xd8ff, MWA_RAM								},	// RAM
	{ 0xd900, 0xe0ff, MWA_RAM, &spriteram, &spriteram_size	},	// Sprites
	{ 0xe100, 0xffff, MWA_RAM								},	// RAM
MEMORY_END



static PORT_READ_START( paradise_readport )
	{ 0x0000, 0x17ff, paletteram_r			},	// Palette
	{ 0x2010, 0x2010, OKIM6295_status_0_r	},	// OKI 0
	{ 0x2030, 0x2030, OKIM6295_status_1_r	},	// OKI 1
	{ 0x2020, 0x2020, input_port_0_r		},	// DSW 1
	{ 0x2021, 0x2021, input_port_1_r		},	// DSW 2
	{ 0x2022, 0x2022, input_port_2_r		},	// P1
	{ 0x2023, 0x2023, input_port_3_r		},	// P2
	{ 0x2024, 0x2024, input_port_4_r		},	// Coins
	{ 0x8000, 0xffff, videoram_r			},	// Pixmap
PORT_END

static PORT_WRITE_START( paradise_writeport )
	{ 0x0000, 0x17ff, paradise_palette_w	},	// Palette
	{ 0x1800, 0x1800, paradise_priority_w	},	// Layers priority
	{ 0x2001, 0x2001, paradise_flipscreen_w	},	// Flip Screen
	{ 0x2004, 0x2004, paradise_palbank_w	},	// Layers palette bank
	{ 0x2006, 0x2006, paradise_rombank_w	},	// ROM bank
	{ 0x2007, 0x2007, paradise_okibank_w	},	// OKI 1 samples bank
	{ 0x2010, 0x2010, OKIM6295_data_0_w		},	// OKI 0
	{ 0x2030, 0x2030, OKIM6295_data_1_w		},	// OKI 1
	{ 0x8000, 0xffff, paradise_pixmap_w		},	// Pixmap
PORT_END


/***************************************************************************

								Input Ports

***************************************************************************/

/***************************************************************************
								Paradise
***************************************************************************/

INPUT_PORTS_START( paradise )
	PORT_START	// IN0 - port $2020 - DSW 1
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x08, "Fill Area" )
	PORT_DIPSETTING(    0x0c, "75%" )
	PORT_DIPSETTING(    0x08, "80%" )
	PORT_DIPSETTING(    0x04, "85%" )
	PORT_DIPSETTING(    0x00, "90%" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0x60, 0x20, "Time" )
	PORT_DIPSETTING(    0x00, "45" )
	PORT_DIPSETTING(    0x20, "60" )
	PORT_DIPSETTING(    0x40, "75" )
	PORT_DIPSETTING(    0x60, "90" )
	PORT_DIPNAME( 0x80, 0x80, "Sound Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	// IN1 - port $2021 - DSW 2
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Characters Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	// IN2 - port $2022 - Player 1
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	// alias for button1?
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	// alias for button1?
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_START1  )

	PORT_START	// IN3 - port $2023 - Player 2
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	// alias for button1?
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	// alias for button1?
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_START2  )

	PORT_START	// IN4 - port $2024 - Coins
	PORT_BIT_IMPULSE(  0x01, IP_ACTIVE_LOW, IPT_COIN1, 5)
	PORT_BIT_IMPULSE(  0x02, IP_ACTIVE_LOW, IPT_COIN2, 5)
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_VBLANK  )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


/***************************************************************************

								Graphics Layouts

***************************************************************************/

static struct GfxLayout layout_8x8x4 =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP8(0,4) },
	{ STEP8(0,4*8) },
	8*8*4
};

static struct GfxLayout layout_8x8x8 =
{
	8,8,
	RGN_FRAC(1,2),
	8,
	{ STEP4(RGN_FRAC(1,2),1), STEP4(RGN_FRAC(0,2),1) },
	{ STEP8(0,4) },
	{ STEP8(0,4*8) },
	8*8*4
};

static struct GfxLayout layout_16x16x8 =
{
	16,16,
	RGN_FRAC(1,2),
	8,
	{ STEP4(RGN_FRAC(1,2),1), STEP4(RGN_FRAC(0,2),1) },
	{ STEP8(8*8*4*0,4), STEP8(8*8*4*1,4) },
	{ STEP8(8*8*4*0,4*8), STEP8(8*8*4*2,4*8) },
	16*16*4
};

static struct GfxDecodeInfo paradise_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_16x16x8,	0x100, 1  }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_8x8x4,	0x400, 16 }, // [1] Background
	{ REGION_GFX3, 0, &layout_8x8x8,	0x300, 1  }, // [2] Midground
	{ REGION_GFX4, 0, &layout_8x8x8,	0x000, 1  }, // [3] Foreground
	{ -1 }
};


/***************************************************************************

								Machine Drivers

***************************************************************************/

static struct OKIM6295interface paradise_okim6295_intf =
{
	2,
	{ 8000,8000 },		/* ? */
	{ REGION_SOUND1,REGION_SOUND2 },
	{ 50,50 }
};

static MACHINE_DRIVER_START( paradise )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 6000000)			/* Z8400B */
	MDRV_CPU_FLAGS(CPU_16BIT_PORT)
	MDRV_CPU_MEMORY(paradise_readmem,paradise_writemem)
	MDRV_CPU_PORTS(paradise_readport,paradise_writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,4)	/* No nmi routine */

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)	/* we're using IPT_VBLANK */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 0+16, 256-1-16)
	MDRV_GFXDECODE(paradise_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(0x800 + 16)

	MDRV_VIDEO_START(paradise)
	MDRV_VIDEO_UPDATE(paradise)

	/* sound hardware */
	MDRV_SOUND_ADD(OKIM6295, paradise_okim6295_intf)
MACHINE_DRIVER_END


/***************************************************************************

								ROMs Loading

***************************************************************************/

/***************************************************************************

									Paradise

(c) yun sung  year ??
another porn qix alike game
1 main cpu tpc1024afn-084c  ??
1 sound z8400b ps
2 ar17961  (oki?)
1 12.000 oscillator cristal

The year is not shown but must be >= 1994, since the development system
(cross compiler?) they used left a "1994.8-1989" in the rom

***************************************************************************/

ROM_START( paradise )
	ROM_REGION( 0x44000, REGION_CPU1, 0 )		/* Z80 Code */
	ROM_LOAD( "u128", 0x00000, 0x0c000, CRC(8e5b5a24) SHA1(a4e559d9329f8a7a9d12cd90d98d0525958085d8) )
	ROM_CONTINUE(     0x10000, 0x34000    )

	ROM_REGION( 0x80000, REGION_GFX1, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 16x16x8 Sprites */
	ROM_LOAD( "u114", 0x00000, 0x40000, CRC(c748ba3b) SHA1(ad23bda4e001ca539f849c1ca256de5daf7c233b) )
	ROM_LOAD( "u115", 0x40000, 0x40000, CRC(0d517bbb) SHA1(5bf7c5036f3d660901e26f14baaea1a3c0327dfe) )

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 8x8x4 Background */
	ROM_LOAD( "u94", 0x00000, 0x20000, CRC(e3a99209) SHA1(5db79dc1a38d93b458b043499a58516285c65aa8) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 8x8x8 Foreground */
	ROM_LOAD( "u92", 0x00000, 0x80000, CRC(633d24f0) SHA1(26b25ec1014fba1a3d0d2bdba0c867c57034647d) )
	ROM_LOAD( "u93", 0x80000, 0x80000, CRC(bbf5c632) SHA1(9d31e136f014c2dd7dd988c3aee0adfcfea91bc9) )

	ROM_REGION( 0x40000, REGION_GFX4, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 8x8x8 Midground */
	ROM_LOAD( "u110", 0x00000, 0x20000, CRC(9807a7e6) SHA1(30e2a741a93954cfe672c61c93a990d0c3b25145) )
	ROM_LOAD( "u111", 0x20000, 0x20000, CRC(bc9f93f0) SHA1(dd4cfc849a0c0f918ac0dfeb7f00a67aae5a1c13) )

	ROM_REGION( 0x40000, REGION_SOUND1, ROMREGION_SOUNDONLY )	/* Samples */
	ROM_LOAD( "u85", 0x00000, 0x40000, CRC(bf3c3065) SHA1(54dd7ffea2fb3f31ed575e982b82691cddc2581a) )

	ROM_REGION( 0x80000, REGION_SOUND2, ROMREGION_SOUNDONLY )	/* Samples (banked) */
	ROM_LOAD( "u113", 0x00000, 0x80000, CRC(53de6025) SHA1(c94b3778b57ff7f46ce4cff661841019fb187d5d) )
ROM_END

/* target ball

looks like its probably similar hardware ... */

READ_HANDLER( tgt_ball_unk )
{
	return rand();
}

DRIVER_INIT (tgtball)
{
	install_port_read_handler(0, 0x2000, 0x2fff, tgt_ball_unk);
}

ROM_START( tgtball )
	ROM_REGION( 0x44000, REGION_CPU1, 0 )		/* Z80 Code */
	ROM_LOAD( "rom7.bin", 0x00000, 0x0c000, CRC(8dbeab12) SHA1(7181c23459990aecbe2d13377aaf19f65108eac6) )
	ROM_CONTINUE(     0x10000, 0x34000    )

	ROM_REGION( 0x80000, REGION_GFX1, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 16x16x8 Sprites */
	ROM_LOAD( "rom6.bin", 0x00000, 0x40000, CRC(30f49dac) SHA1(b70d37973bd03069c48641d6c0804be6f9aa6553) )
	ROM_LOAD( "rom5.bin", 0x40000, 0x40000, CRC(3dbe1872) SHA1(754f90123a3944ca548fc66ee65a93615155bf30) )

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 8x8x4 Background */
	/* not for this game? */

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 8x8x8 Foreground */
	ROM_LOAD( "rom2.bin", 0x00000, 0x80000, CRC(fe4004ec) SHA1(fde782665445ad465b8f8fb95df5f60cd24016ad) )
	ROM_LOAD( "rom1.bin", 0x80000, 0x80000, CRC(aef17762) SHA1(3dd8924695b67eec0f25549dbe2461b927268b8f) )

	ROM_REGION( 0x100000, REGION_GFX4, ROMREGION_DISPOSE | ROMREGION_INVERT)	/* 8x8x8 Midground */
	ROM_LOAD( "rom4.bin", 0x00000, 0x80000,  CRC(0a5abf62) SHA1(6900d598764300c81c90f5a7efb294639178bee6) )
	ROM_LOAD( "rom3.bin", 0x80000, 0x80000,  CRC(94822bbf) SHA1(9fa6595eb819f163b58181926c276346cfa5c332) )

	ROM_REGION( 0x40000, REGION_SOUND1, ROMREGION_SOUNDONLY )	/* Samples */
	ROM_LOAD( "rom8.bin", 0x00000, 0x20000, CRC(cdf3336b) SHA1(98029d6d5d8ffb3b24ae2bcf950618a7d5b404c3) )

	ROM_REGION( 0x80000, REGION_SOUND2, ROMREGION_SOUNDONLY )	/* Samples (banked) */
	ROM_LOAD( "rom9.bin", 0x00000, 0x40000, CRC(150a6cc6) SHA1(b435fcf8ba48006f506db6b63ba54a30a6b3eade) )
ROM_END

/***************************************************************************

								Game Drivers

***************************************************************************/

GAME( 1994+, paradise, 0, paradise, paradise, 0, ROT90, "Yun Sung", "Paradise" )
GAMEX(199?,  tgtball,  0, paradise, paradise, tgtball, ROT0,  "Yun Sung", "Target Ball", GAME_NOT_WORKING )

