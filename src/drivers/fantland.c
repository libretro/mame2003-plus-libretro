/***************************************************************************

					  -= Fantasy Land / Galaxy Gunners =-

					driver by	Luca Elia (l.elia@tin.it)


CPUs 	:	2 x 8086
Sound	:	YM2151 + DAC
Video	:	?

---------------------------------------------------------------------------
Year + Game				Boards
---------------------------------------------------------------------------
>=1987	Fantasy Land	?
1989	Galaxy Gunners	?
---------------------------------------------------------------------------

Notes:

- Clocks are unknown and the cpu might be an 8088 or a later x86.

[fantland]

- The year of creation isn't displayed, but it's no sooner than 1987 since
  embedded in the roms is: "MS Run-Time Library - Copyright (c) 1987, Microsoft Corp"
- Slowdowns on the ice level's boss


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

VIDEO_UPDATE( fantland );

/***************************************************************************

							Memory Maps - Main CPU

***************************************************************************/

static data8_t fantland_nmi_enable;

static WRITE_HANDLER( fantland_nmi_enable_w )
{
	fantland_nmi_enable = data;
/*	if ((fantland_nmi_enable != 0) && (fantland_nmi_enable != 8))*/
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "CPU #0 : nmi_enable = %02x - PC = %04X\n", data, activecpu_get_pc());*/
}

static WRITE_HANDLER( fantland_soundlatch_w )
{
	soundlatch_w(0,data);
	cpu_set_nmi_line(1,PULSE_LINE);
}

/***************************************************************************
								Fantasy Land
***************************************************************************/

static MEMORY_READ_START( fantland_readmem )
	{ 0x00000, 0x07fff, MRA_RAM			},
	{ 0x08000, 0x7ffff, MRA_ROM			},
	{ 0xa2000, 0xa21ff, MRA_RAM			},	/* not actually read*/
	{ 0xa3000, 0xa3000, input_port_0_r	},
	{ 0xa3001, 0xa3001, input_port_1_r	},
	{ 0xa3002, 0xa3002, input_port_2_r	},
	{ 0xa3003, 0xa3003, input_port_3_r	},
	{ 0xa4000, 0xa67ff, MRA_RAM			},	/* not actually read*/
	{ 0xc0000, 0xc03ff, MRA_RAM			},	/* ""*/
	{ 0xe0000, 0xfffff, MRA_ROM			},
MEMORY_END

static MEMORY_WRITE_START( fantland_writemem )
	{ 0x00000, 0x07fff, MWA_RAM					},
	{ 0x08000, 0x7ffff, MWA_ROM					},
	{ 0xa2000, 0xa21ff, paletteram_xRRRRRGGGGGBBBBB_w, &paletteram	},
	{ 0xa3000, 0xa3000, fantland_nmi_enable_w	},
	{ 0xa3002, 0xa3002, fantland_soundlatch_w	},
	{ 0xa4000, 0xa67ff, MWA_RAM, &spriteram		},
	{ 0xc0000, 0xc03ff, MWA_RAM, &spriteram_2	},
	{ 0xe0000, 0xfffff, MWA_ROM					},
MEMORY_END


/***************************************************************************
								Galaxy Gunners
***************************************************************************/

static MEMORY_READ_START( galaxygn_readmem )
	{ 0x00000, 0x07fff, MRA_RAM			},
	{ 0x10000, 0x2ffff, MRA_ROM			},
	{ 0x52000, 0x521ff, MRA_RAM			},	/* not actually read*/
	{ 0x53000, 0x53000, input_port_0_r	},
	{ 0x53001, 0x53001, input_port_1_r	},
	{ 0x53002, 0x53002, input_port_2_r	},
	{ 0x53003, 0x53003, input_port_3_r	},
	{ 0x54000, 0x567ff, MRA_RAM			},	/* not actually read*/
	{ 0x60000, 0x603ff, MRA_RAM			},	/* ""*/
	{ 0x60000, 0x7ffff, MRA_ROM			},
	{ 0xf0000, 0xfffff, MRA_ROM			},
MEMORY_END

static MEMORY_WRITE_START( galaxygn_writemem )
	{ 0x00000, 0x07fff, MWA_RAM					},
	{ 0x10000, 0x2ffff, MWA_ROM					},
	{ 0x52000, 0x521ff, paletteram_xRRRRRGGGGGBBBBB_w, &paletteram	},
	{ 0x53000, 0x53000, fantland_nmi_enable_w	},
	{ 0x53002, 0x53002, fantland_soundlatch_w	},
	{ 0x54000, 0x567ff, MWA_RAM, &spriteram		},
	{ 0x60000, 0x603ff, MWA_RAM, &spriteram_2	},
	{ 0x60000, 0x7ffff, MWA_ROM					},
	{ 0xf0000, 0xfffff, MWA_ROM					},
MEMORY_END


/***************************************************************************

							Memory Maps - Sound CPU

***************************************************************************/

static MEMORY_READ_START( fantland_sound_readmem )
	{ 0x00000, 0x01fff, MRA_RAM },
	{ 0x80000, 0x9ffff, MRA_ROM	},
	{ 0xc0000, 0xfffff, MRA_ROM	},
MEMORY_END

static MEMORY_WRITE_START( fantland_sound_writemem )
	{ 0x00000, 0x01fff, MWA_RAM	},
	{ 0x80000, 0x9ffff, MWA_ROM	},
	{ 0xc0000, 0xfffff, MWA_ROM	},
MEMORY_END

static PORT_READ_START( fantland_sound_readport )
	{ 0x0080, 0x0080, soundlatch_r				},
	{ 0x0101, 0x0101, YM2151_status_port_0_r	},
PORT_END

static PORT_WRITE_START( fantland_sound_writeport )
	{ 0x0100, 0x0100, YM2151_register_port_0_w	},
	{ 0x0101, 0x0101, YM2151_data_port_0_w		},
	{ 0x0180, 0x0180, DAC_0_data_w				},
PORT_END


/***************************************************************************

								Input Ports

***************************************************************************/

/***************************************************************************
								Fantasy Land
***************************************************************************/

INPUT_PORTS_START( fantland )
	PORT_START	/* IN0 - a3000 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1			)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1			)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1			)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2			)

	PORT_START	/* IN1 - a3001 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN2			)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2			)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		| IPF_PLAYER2 )	/* used in test mode only*/
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	| IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	| IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	| IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1			| IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2			| IPF_PLAYER2 )

	PORT_START	/* IN2 - a3002 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, "Invulnerability" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x20, "Harder" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* IN3 - a3003 */
	PORT_DIPNAME( 0x01, 0x01, "Test Sound" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0e, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0e, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x0a, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x06, "5" )
	PORT_DIPSETTING(    0x04, "6" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "800k" )
	PORT_DIPSETTING(    0x20, "1600k" )
	PORT_DIPSETTING(    0x10, "2400k" )
	PORT_DIPSETTING(    0x00, "3200k" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )	/*unused?*/
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )	/*unused?*/
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************
								Galaxy Gunners
***************************************************************************/

INPUT_PORTS_START( galaxygn )
	PORT_START	/* IN0 - 53000 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1			)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1			)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1			)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2			)

	PORT_START	/* IN1 - 53001 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN2			)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2			)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		| IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	| IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	| IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	| IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1			| IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2			| IPF_PLAYER2 )

	PORT_START	/* IN2 - 53002 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, "Invulnerability" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) ) 	/* Demo Sounds? doesn't work*/
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )	/* Allow Continue? doesn't work*/
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x20, "Harder" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* IN3 - 53003 */
	PORT_DIPNAME( 0x01, 0x01, "Test Sound" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0e, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0e, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x0a, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x06, "5" )
	PORT_DIPSETTING(    0x04, "6" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "10k" )
	PORT_DIPSETTING(    0x20, "20k" )
	PORT_DIPSETTING(    0x10, "30k" )
	PORT_DIPSETTING(    0x00, "40k" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )	/*unused?*/
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )	/*unused?*/
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


/***************************************************************************

							Graphics Layouts

***************************************************************************/

static struct GfxLayout layout16x16x6 =
{
	16,16,
	RGN_FRAC(1,1),
	6,
	{ 0,1,2,3,4,5 },
	{ STEP4(3*6,-6),STEP4(7*6,-6),STEP4(11*6,-6),STEP4(15*6,-6) },
	{ STEP16(0,16*6) },
	16*16*6
};

static struct GfxDecodeInfo fantland_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout16x16x6, 0, 4 }, /* [0] Sprites*/
	{ -1 }
};

/***************************************************************************

								Machine Drivers

***************************************************************************/

static MACHINE_INIT( fantland )
{
	fantland_nmi_enable = 0;
}

static INTERRUPT_GEN( fantland_irq )
{
	if (fantland_nmi_enable & 8)
		cpu_set_nmi_line(0, PULSE_LINE);
}

static INTERRUPT_GEN( fantland_sound_irq )
{
	cpu_set_irq_line_and_vector(1, 0, HOLD_LINE, 0x80/4);
}

static struct YM2151interface fantland_ym2151_interface =
{
	1,
	3000000,	/* ?*/
	{ YM3012_VOL(35,MIXER_PAN_CENTER,25,MIXER_PAN_CENTER) },
	{ 0 }
};

static struct DACinterface fantland_dac_interface =
{
	1,
	{ 80 }
};

static MACHINE_DRIVER_START( fantland )
	/* basic machine hardware */
	MDRV_CPU_ADD(I86, 8000000)        /* ?*/
	MDRV_CPU_MEMORY(fantland_readmem, fantland_writemem)
	MDRV_CPU_VBLANK_INT(fantland_irq,1)

	MDRV_CPU_ADD(I86, 8000000)        /* ?*/
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(fantland_sound_readmem, fantland_sound_writemem)
	MDRV_CPU_PORTS(fantland_sound_readport,fantland_sound_writeport)
	MDRV_CPU_PERIODIC_INT(fantland_sound_irq,8000)
	/* NMI when soundlatch is written*/

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(fantland)

	MDRV_INTERLEAVE(8000/60)	/* sound irq must feed the DAC at 8kHz*/

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(352,256)
	MDRV_VISIBLE_AREA(0, 352-1, 0, 256-1)
	MDRV_GFXDECODE(fantland_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)

	MDRV_VIDEO_UPDATE(fantland)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151,	fantland_ym2151_interface	)
	MDRV_SOUND_ADD(DAC,		fantland_dac_interface		)
MACHINE_DRIVER_END


static void galaxygn_sound_irq(int line)
{
	cpu_set_irq_line_and_vector(1, 0, line ? ASSERT_LINE : CLEAR_LINE, 0x80/4);
}

static struct YM2151interface galaxygn_ym2151_interface =
{
	1,
	3000000,	/* ?*/
	{ YM3012_VOL(100,MIXER_PAN_CENTER,100,MIXER_PAN_CENTER) },
	{ galaxygn_sound_irq }
};

static MACHINE_DRIVER_START( galaxygn )
	/* basic machine hardware */
	MDRV_CPU_ADD(I86, 8000000)        /* ?*/
	MDRV_CPU_MEMORY(galaxygn_readmem, galaxygn_writemem)
	MDRV_CPU_VBLANK_INT(fantland_irq,1)

	MDRV_CPU_ADD(I86, 8000000)        /* ?*/
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(fantland_sound_readmem, fantland_sound_writemem)
	MDRV_CPU_PORTS(fantland_sound_readport,fantland_sound_writeport)
	/* IRQ by YM2151, NMI when soundlatch is written*/

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(fantland)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(352,256)
	MDRV_VISIBLE_AREA(0, 352-1, 0, 256-1)
	MDRV_GFXDECODE(fantland_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)

	MDRV_VIDEO_UPDATE(fantland)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151,	galaxygn_ym2151_interface	)
MACHINE_DRIVER_END

/***************************************************************************

								ROMs Loading

***************************************************************************/

/***************************************************************************

Fantasy Land by Electronic Devices of Italy

Fantasyl.ev2  od2  d0  d1 s1  s2 :are 27c010 devices
Fantasyl.ev1 &  od1  are 27c512 devices

s1 & s2 are sound files.  (labeled 5 and 6 on devices)
d0 & d1 are next to the image banks  (labeled  7 and 8 on devices)
ev1 od1  od2  ev2  are believed to be program eproms
     (labeled 2, 1, 3, 4 on devices respectively)
     (also labeled under the eprom, location U3, U8, U7, U2 respectively)


3) - 23c4000 mask roms  "05, 06, 07"               !!!! [ 32 pin devices!!! ]
5) - 23c4100 mask roms  "00, 01, 02, 03, 04"   !!!! [ 40 pin devices!!! ]

Fantasyl.01, 00, 02, 03, 04  was read as if it was a 27c400

Fantasy.05, 06, 07 was read as if it was a 27c040

***************************************************************************/

ROM_START( fantland )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )					/* Main CPU*/
	ROMX_LOAD( "fantasyl.ev2", 0x00000, 0x20000, CRC(f5bdca0e) SHA1(d05cf6f68d4d1a3dcc0171f7cf220c4920bd47bb) , ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.od2", 0x00001, 0x20000, CRC(9db35023) SHA1(81e2accd67dcf8563a68b2c4e35526f23a40150c) , ROM_SKIP(1) )
	ROM_COPY( REGION_CPU1,     0x00000, 0x40000, 0x40000 )
	ROMX_LOAD( "fantasyl.ev1", 0xe0000, 0x10000, CRC(70e0ee30) SHA1(5253213da56b3f97e2811f2b10927d0e542447f0) , ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.od1", 0xe0001, 0x10000, CRC(577b4bd7) SHA1(1f08202d99c3e39e0dd1ed4947b928b695a5b411) , ROM_SKIP(1) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )					/* Sound CPU*/
	ROM_LOAD16_WORD( "fantasyl.s2", 0x80000, 0x20000, CRC(f23837d8) SHA1(4048784f759781e50ae445ea61f1ca908e8e6ac1) )	/* samples (8 bit unsigned)*/
	ROM_LOAD16_WORD( "fantasyl.s1", 0xc0000, 0x20000, CRC(1a324a69) SHA1(06f6877af6cd19bfaac8a4ea8057ef8faee276f5) )
	ROM_COPY( REGION_CPU2,          0xc0000, 0xe0000, 0x20000 )

	ROM_REGION( 0x480000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites*/
	ROMX_LOAD( "fantasyl.m00", 0x000000, 0x80000, CRC(82d819ff) SHA1(2b5b0759de8260eaa84ddded9dc2d12a6e0f5ec9) , ROM_GROUPWORD | ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.m01", 0x0c0000, 0x80000, CRC(70a94139) SHA1(689fbfa267d60821cde13d5dc2dfe1dea67b434a) , ROM_GROUPWORD | ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.05",  0x000002, 0x80000, CRC(62b9a00b) SHA1(ecd18e5e7a5e3535956fb693d2f7e35d2bb7ede9) , ROM_SKIP(2) )

	ROMX_LOAD( "fantasyl.m02", 0x180000, 0x80000, CRC(ae52bf37) SHA1(60daa24d1f456cfeb643fa2107119d2939af0ffa) , ROM_GROUPWORD | ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.m03", 0x240000, 0x80000, CRC(f3f534a1) SHA1(9d47cc5b5a40146ed1d9e57a16d67a1d92f3b5be) , ROM_GROUPWORD | ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.06",  0x180002, 0x80000, CRC(867fa549) SHA1(9777b4837e5bb25a39639597e88b713d43361a80) , ROM_SKIP(2) )

	ROMX_LOAD( "fantasyl.m04", 0x300000, 0x80000, CRC(e7b1918c) SHA1(97230b21bb54c4c928dced83e0b3396068ab72db) , ROM_GROUPWORD | ROM_SKIP(1) )
	ROMX_LOAD( "fantasyl.d0",  0x3c0001, 0x20000, CRC(0f907f19) SHA1(eea90e7d7e2e29db809e867d9b1205f4fbb7ada8) , ROM_SKIP(2) )
	ROMX_LOAD( "fantasyl.d1",  0x3c0000, 0x20000, CRC(10d10389) SHA1(3a5639050c769eedc62924dfde57c1bf020970c8) , ROM_SKIP(2) )
	ROMX_LOAD( "fantasyl.07",  0x300002, 0x80000, CRC(162ad422) SHA1(0d3609e630481018d1326a908d1d4c204dfcdf13) , ROM_SKIP(2) )
ROM_END

/***************************************************************************
								Galaxy Gunners
***************************************************************************/

ROM_START( galaxygn )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )					/* Main CPU*/
	ROM_LOAD( "gg03.bin", 0x10000, 0x10000, CRC(9e469189) SHA1(07e5d36ca9665bdd13e3bb4241d34b9042371b79) )
	ROM_LOAD( "gg02.bin", 0x20000, 0x10000, CRC(b87a438f) SHA1(96c39cc4d51a2fc0779f148971220117967173c0) )
	ROM_LOAD( "gg01.bin", 0x60000, 0x10000, CRC(ad0e5b29) SHA1(f9a7ebce9f47a009af213e4e10811bb1c26f891a) )
	ROM_COPY( REGION_CPU1, 0x60000, 0x70000, 0x10000 )
	ROM_COPY( REGION_CPU1, 0x60000, 0xf0000, 0x10000 )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )					/* Sound CPU*/
	ROM_LOAD( "gg20.bin", 0xc0000, 0x10000, CRC(f5c65a85) SHA1(a094fa9531ea4e68ec0a448568e7d4b2307c8185) )
	ROM_COPY( REGION_CPU2, 0xc0000, 0xd0000, 0x10000 )
	ROM_COPY( REGION_CPU2, 0xc0000, 0xe0000, 0x10000 )
	ROM_COPY( REGION_CPU2, 0xc0000, 0xf0000, 0x10000 )

	ROM_REGION( 0x1b0000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites*/
	ROMX_LOAD( "gg54.bin", 0x000000, 0x10000, CRC(b3621119) SHA1(66ade772077e57f872ef1c8f45e244f4006023f0) , ROM_SKIP(2) )
	ROMX_LOAD( "gg38.bin", 0x000001, 0x10000, CRC(52b70f3e) SHA1(65f11d5700337d6d9b6325ff70c86d076e1bdc26) , ROM_SKIP(2) )
	ROMX_LOAD( "gg22.bin", 0x000002, 0x10000, CRC(ea49fee4) SHA1(29ae3e5dfade421a5e97efe5be1cb17862fdcea1) , ROM_SKIP(2) )
	ROMX_LOAD( "gg55.bin", 0x030000, 0x10000, CRC(bffe278f) SHA1(b8077794c4af5aa13ea6f231988e698c1bf229bd) , ROM_SKIP(2) )
	ROMX_LOAD( "gg39.bin", 0x030001, 0x10000, CRC(3f7df1e6) SHA1(c1ef3a2f7aaf2e7e850c884dc5d3c1f1545a2526) , ROM_SKIP(2) )
	ROMX_LOAD( "gg23.bin", 0x030002, 0x10000, CRC(4dcbbc99) SHA1(57ba915043a2c3867bb28875a0d0bc3f81ae69d5) , ROM_SKIP(2) )
	ROMX_LOAD( "gg56.bin", 0x060000, 0x10000, CRC(0306069e) SHA1(e10b142a23a93caac0b505d59b6d5d6a4e195d4b) , ROM_SKIP(2) )
	ROMX_LOAD( "gg40.bin", 0x060001, 0x10000, CRC(f635aa7e) SHA1(3e5ce1c3b25cb2c0387ae5dfe650040ccc353d8a) , ROM_SKIP(2) )
	ROMX_LOAD( "gg24.bin", 0x060002, 0x10000, CRC(733f5dcf) SHA1(e516537dcb3f18da2af307ffded3ee7914e28e20) , ROM_SKIP(2) )
	ROMX_LOAD( "gg57.bin", 0x090000, 0x10000, CRC(c3919bef) SHA1(1eebe888135c51c053d689fda3031769f1efa70a) , ROM_SKIP(2) )
	ROMX_LOAD( "gg41.bin", 0x090001, 0x10000, CRC(1f2757de) SHA1(c853f5ce08141e0197988a9d9e5c0032a29a5824) , ROM_SKIP(2) )
	ROMX_LOAD( "gg25.bin", 0x090002, 0x10000, CRC(1d094f95) SHA1(bcb654c7edd8eec9a655261ebc7c812144d4ff6d) , ROM_SKIP(2) )
	ROMX_LOAD( "gg58.bin", 0x0c0000, 0x10000, CRC(4a459cb8) SHA1(e17de49c56d91942a274206d005dd7bf0f074a7f) , ROM_SKIP(2) )
	ROMX_LOAD( "gg42.bin", 0x0c0001, 0x10000, CRC(ae7a8e1e) SHA1(8714d2b58a26138a9644f9117fcdd89256135a98) , ROM_SKIP(2) )
	ROMX_LOAD( "gg26.bin", 0x0c0002, 0x10000, CRC(c2f310b4) SHA1(510e3bc773456b69609c4e29583753f21dac6165) , ROM_SKIP(2) )
	ROMX_LOAD( "gg59.bin", 0x0f0000, 0x10000, CRC(c8d4fbc2) SHA1(f8e9e7d31fa4c7920a3db36295999ef0ee86dbe9) , ROM_SKIP(2) )
	ROMX_LOAD( "gg43.bin", 0x0f0001, 0x10000, CRC(74d3a0df) SHA1(c8d611c969898f135a254ea53b465305715625c6) , ROM_SKIP(2) )
	ROMX_LOAD( "gg27.bin", 0x0f0002, 0x10000, CRC(c2cfd3f9) SHA1(8c2ad28aa64a124d3c97fde804bf167378ba4c20) , ROM_SKIP(2) )
	ROMX_LOAD( "gg60.bin", 0x120000, 0x10000, CRC(6e32b549) SHA1(541860ad2f2b197fdf3877d8aeded0609ccb7fb0) , ROM_SKIP(2) )
	ROMX_LOAD( "gg44.bin", 0x120001, 0x10000, CRC(fcda6efa) SHA1(b4eb575dee8f78c4f0d3ae26204315924d4ce9bd) , ROM_SKIP(2) )
	ROMX_LOAD( "gg28.bin", 0x120002, 0x10000, CRC(4d4fc01c) SHA1(1ab5186ac440dc004140d5a8bf19df521b60e62d) , ROM_SKIP(2) )
	ROMX_LOAD( "gg61.bin", 0x150000, 0x10000, CRC(177a767a) SHA1(09e2883eaefeb20782bdd5aad069fe35b06b8329) , ROM_SKIP(2) )
	ROMX_LOAD( "gg45.bin", 0x150001, 0x10000, CRC(2ba49d47) SHA1(380943bde589dd2ea079a54fa7bcf327da2990a7) , ROM_SKIP(2) )
	ROMX_LOAD( "gg29.bin", 0x150002, 0x10000, CRC(c1c68148) SHA1(171d25aa20accf6638e1b0886a15db9fac2d8b85) , ROM_SKIP(2) )
	ROMX_LOAD( "gg62.bin", 0x180000, 0x10000, CRC(0fb2d41a) SHA1(41b179e4e9ae142b3057e7cdad6eee8efcd952c4) , ROM_SKIP(2) )
	ROMX_LOAD( "gg46.bin", 0x180001, 0x10000, CRC(5f1bf8ad) SHA1(b831ea433ff008377b522a3be4666d6d1b86cbb4) , ROM_SKIP(2) )
	ROMX_LOAD( "gg30.bin", 0x180002, 0x10000, CRC(ded7cacf) SHA1(adbfaa8f46e5ce8df264d5b5a201d75ca2b3dbeb) , ROM_SKIP(2) )
ROM_END

GAME( 19??, fantland, 0, fantland, fantland, 0, ROT0,  "Electronic Devices Italy", "Fantasy Land"   )
GAME( 1989, galaxygn, 0, galaxygn, galaxygn, 0, ROT90, "Electronic Devices Italy", "Galaxy Gunners" )

