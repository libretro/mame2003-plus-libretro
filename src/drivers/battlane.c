/***************************************************************************

	Battle Lane Vol. 5
	1986 Taito

	2x6809

    Driver provided by Paul Leaman (paul@vortexcomputing.demon.co.uk)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"

extern UINT8 *battlane_spriteram;
extern UINT8 *battlane_tileram;

extern WRITE_HANDLER( battlane_palette_w );
extern WRITE_HANDLER( battlane_scrollx_w );
extern WRITE_HANDLER( battlane_scrolly_w );
extern WRITE_HANDLER( battlane_tileram_w );
extern WRITE_HANDLER( battlane_spriteram_w );
extern WRITE_HANDLER( battlane_bitmap_w );
extern WRITE_HANDLER( battlane_video_ctrl_w );

extern PALETTE_INIT( battlane );
extern VIDEO_START( battlane );
extern VIDEO_UPDATE( battlane );


/* CPU interrupt control register */
int battlane_cpu_control;

WRITE_HANDLER( battlane_cpu_command_w )
{
	battlane_cpu_control = data;

	/*
	CPU control register

        0x80    = Video Flip
        0x08    = NMI
        0x04    = CPU 0 IRQ   (0=Activate)
        0x02    = CPU 1 IRQ   (0=Activate)
        0x01    = Y Scroll MSB
	*/

	if (flip_screen != (data & 0x80)) {
		flip_screen_set(data & 0x80);
	}

	/*
        I think that the NMI is an inhibitor. It is constantly set
        to zero whenever an NMIs are allowed.

        However, it could also be that setting to zero could
        cause the NMI to trigger. I really don't know.
	*/

    /*
    if (~battlane_cpu_control & 0x08)
    {
        cpu_set_nmi_line(0, PULSE_LINE);
        cpu_set_nmi_line(1, PULSE_LINE);
    }
    */

	/*
        CPU2's SWI will trigger an 6809 IRQ on the master by resetting 0x04
        Master will respond by setting the bit back again
	*/
    cpu_set_irq_line(0, M6809_IRQ_LINE,  data & 0x04 ? CLEAR_LINE : HOLD_LINE);

	/*
	Slave function call (e.g. ROM test):
	FA7F: 86 03       LDA   #$03	; Function code
	FA81: 97 6B       STA   $6B
	FA83: 86 0E       LDA   #$0E
	FA85: 84 FD       ANDA  #$FD	; Trigger IRQ
	FA87: 97 66       STA   $66
	FA89: B7 1C 03    STA   $1C03	; Do Trigger
	FA8C: C6 40       LDB   #$40
	FA8E: D5 68       BITB  $68
	FA90: 27 FA       BEQ   $FA8C	; Wait for slave IRQ pre-function dispatch
	FA92: 96 68       LDA   $68
	FA94: 84 01       ANDA  #$01
	FA96: 27 FA       BEQ   $FA92	; Wait for bit to be set
	*/

	cpu_set_irq_line(1, M6809_IRQ_LINE, data & 0x02 ? CLEAR_LINE : HOLD_LINE);
}

/* Both CPUs share the same memory */

WRITE_HANDLER( battlane_shared_ram_w )
{
	UINT8 *RAM = memory_region(REGION_CPU1);
	RAM[offset] = data;
}

READ_HANDLER( battlane_shared_ram_r )
{
	UINT8 *RAM = memory_region(REGION_CPU1);
	return RAM[offset];
}


static MEMORY_READ_START( battlane_readmem )
	{ 0x0000, 0x0fff, battlane_shared_ram_r },
    { 0x1000, 0x17ff, MRA_RAM },
    { 0x1800, 0x18ff, MRA_RAM },
	{ 0x1c00, 0x1c00, input_port_0_r },
    { 0x1c01, 0x1c01, input_port_1_r },   /* VBLANK port */
	{ 0x1c02, 0x1c02, input_port_2_r },
	{ 0x1c03, 0x1c03, input_port_3_r },
	{ 0x1c04, 0x1c04, YM3526_status_port_0_r },
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( battlane_writemem )
	{ 0x0000, 0x0fff, battlane_shared_ram_w },
    { 0x1000, 0x17ff, battlane_tileram_w, &battlane_tileram },
    { 0x1800, 0x18ff, battlane_spriteram_w, &battlane_spriteram },
	{ 0x1c00, 0x1c00, battlane_video_ctrl_w },
    { 0x1c01, 0x1c01, battlane_scrollx_w },
    { 0x1c02, 0x1c02, battlane_scrolly_w },
    { 0x1c03, 0x1c03, battlane_cpu_command_w },
	{ 0x1c04, 0x1c04, YM3526_control_port_0_w },
	{ 0x1c05, 0x1c05, YM3526_write_port_0_w },
	{ 0x1e00, 0x1e3f, battlane_palette_w },
	{ 0x2000, 0x3fff, battlane_bitmap_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END


INTERRUPT_GEN( battlane_cpu1_interrupt )
{
	/* See note in battlane_cpu_command_w */

	if (~battlane_cpu_control & 0x08)
	{
		cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
		cpu_set_irq_line(1, IRQ_LINE_NMI, PULSE_LINE);
	}
}


INPUT_PORTS_START( battlane )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

    PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C )  )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy"  )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard"  )
	PORT_DIPSETTING(    0x00, "Very Hard" )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 50000" )
	PORT_DIPSETTING(    0x08, "20000 70000" )
	PORT_DIPSETTING(    0x04, "20000 90000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )
INPUT_PORTS_END


static struct GfxLayout spritelayout =
{
	16, 16,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(0,3), RGN_FRAC(1,3), RGN_FRAC(2,3) },
	{
		7, 6, 5, 4, 3, 2, 1, 0,
		16*8+7, 16*8+6, 16*8+5, 16*8+4, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
	},
	{
		15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8,
		7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8,
	},
	16*8*2
};

static struct GfxLayout tilelayout =
{
	16, 16,    /* 16*16 tiles */
	256,    /* 256 tiles */
	3,      /* 3 bits per pixel */
	{ 0x8000*8+4, 4, 0 },    /* plane offset */
	{
		3, 2, 1, 0,
		8+3, 8+2, 8+1, 8+0,
		16+3, 16+2, 16+1, 16+0,
		16+8+3, 16+8+2, 16+8+1, 16+8+0
	},
	{
		0*8*4, 1*8*4,  2*8*4,  3*8*4,  4*8*4,  5*8*4,  6*8*4,  7*8*4,
		8*8*4, 9*8*4, 10*8*4, 11*8*4, 12*8*4, 13*8*4, 14*8*4, 15*8*4
	},
	8*8*4*2     /* every char takes consecutive bytes */
};

static struct GfxLayout tilelayout2 =
{
	16, 16,    /* 16*16 tiles */
	256,    /* 256 tiles */
	3,      /* 3 bits per pixel */
    { 0x8000*8, 0x4000*8+4, 0x4000*8+0 },    /* plane offset */
	{
		3, 2, 1, 0,
		8+3, 8+2, 8+1, 8+0,
		16+3, 16+2, 16+1, 16+0,
		16+8+3, 16+8+2, 16+8+1, 16+8+0
	},
	{
		0*8*4, 1*8*4,  2*8*4,  3*8*4,  4*8*4,  5*8*4,  6*8*4,  7*8*4,
		8*8*4, 9*8*4, 10*8*4, 11*8*4, 12*8*4, 13*8*4, 14*8*4, 15*8*4
	},
	8*8*4*2     /* every char takes consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,  0, 2 },	/* colors 0x00-0x0f */
	{ REGION_GFX2, 0, &tilelayout,   32, 4 },	/* colors 0x20-0x3f */
	{ REGION_GFX2, 0, &tilelayout2,  32, 4 },	/* colors 0x20-0x3f */
	{ -1 } /* end of array */
};


static void irqhandler(int irq)
{
	cpu_set_irq_line(0, M6809_FIRQ_LINE, irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM3526interface ym3526_interface =
{
	1,              /* 1 chip */
	3000000,        /* 3 MHz ??? */
	{ 100 },         /* volume */
	{ irqhandler }
};


static MACHINE_DRIVER_START( battlane )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809, 1250000)        /* 1.25 MHz ? */
	MDRV_CPU_MEMORY(battlane_readmem, battlane_writemem)
	MDRV_CPU_VBLANK_INT(battlane_cpu1_interrupt, 1)

	MDRV_CPU_ADD(M6809, 1250000)        /* 1.25 MHz ? */
	MDRV_CPU_MEMORY(battlane_readmem, battlane_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32 * 8, 32 * 8)
	MDRV_VISIBLE_AREA(1 * 8, 31 * 8 - 1, 0 * 8, 32 * 8 - 1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(64)

	MDRV_VIDEO_START(battlane)
	MDRV_VIDEO_UPDATE(battlane)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3526, ym3526_interface)
MACHINE_DRIVER_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( battlane )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for main CPU */
	/* first half of da00-5 will be copied at 0x4000-0x7fff */
	ROM_LOAD( "da00-5",    0x4000, 0x8000, CRC(85b4ed73) SHA1(b8e61eedf8fb75bb07f1df91a7465cee2b6ff372) )
	ROM_LOAD( "da01-5",    0x8000, 0x8000, CRC(7a6c3f02) SHA1(bee1ee858f81453a53afc2d016f549924801b090) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64K for slave CPU */
	ROM_LOAD( "da00-5",    0x0000, 0x8000, CRC(85b4ed73) SHA1(b8e61eedf8fb75bb07f1df91a7465cee2b6ff372) )	/* ...second half goes here */
	ROM_LOAD( "da02-2",    0x8000, 0x8000, CRC(69d8dafe) SHA1(a7dab13d7f05bf8e3220bb8193066e9b45c86a17) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "da05",      0x00000, 0x8000, CRC(834829d4) SHA1(d56781d2a7ef89b645a637166cd5acde6a65f7f9) ) /* Sprites Plane 1+2 */
	ROM_LOAD( "da04",      0x08000, 0x8000, CRC(f083fd4c) SHA1(eb8f079776a0efd898574874d21f865311ecd8ba) ) /* Sprites Plane 3+4 */
	ROM_LOAD( "da03",      0x10000, 0x8000, CRC(cf187f25) SHA1(c0d2d85f85340c12c1b61cc062506ffa4841ef78) ) /* Sprites Plane 5+6 */

	ROM_REGION( 0x0c000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "da06",      0x00000, 0x8000, CRC(9c6a51b3) SHA1(0d623e8fba9373979a93f97cdfcf311c7e7f561a) ) /* Tiles*/
	ROM_LOAD( "da07",      0x08000, 0x4000, CRC(56df4077) SHA1(f4b8047c3b4d5897ba91489bc76a9504d9941072) ) /* Tiles*/

	ROM_REGION( 0x0040, REGION_PROMS, 0 )
	ROM_LOAD( "82s123.7h", 0x00000, 0x0020, CRC(b9933663) SHA1(5d5c840caa0b8416ed7dd4890dd5f3e4a9e86511) )
	ROM_LOAD( "82s123.9n", 0x00020, 0x0020, CRC(06491e53) SHA1(d6cf5003798f9a9d555bca97844dcb2966cbac9d) )
ROM_END

ROM_START( battlan2 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for main CPU */
	/* first half of da00-3 will be copied at 0x4000-0x7fff */
	ROM_LOAD( "da00-3",    0x4000, 0x8000, CRC(7a0a5d58) SHA1(ef97e5a64a668c437c18cda931c52bf39b580b4a) )
	ROM_LOAD( "da01-3",    0x8000, 0x8000, CRC(d9e40800) SHA1(dc87ae0d8631c220dbbddbf0e49b6bdaeb635269) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64K for slave CPU */
	ROM_LOAD( "da00-3",    0x0000, 0x8000, CRC(7a0a5d58) SHA1(ef97e5a64a668c437c18cda931c52bf39b580b4a) )	/* ...second half goes here */
	ROM_LOAD( "da02-2",    0x8000, 0x8000, CRC(69d8dafe) SHA1(a7dab13d7f05bf8e3220bb8193066e9b45c86a17) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "da05",      0x00000, 0x8000, CRC(834829d4) SHA1(d56781d2a7ef89b645a637166cd5acde6a65f7f9) ) /* Sprites Plane 1+2 */
	ROM_LOAD( "da04",      0x08000, 0x8000, CRC(f083fd4c) SHA1(eb8f079776a0efd898574874d21f865311ecd8ba) ) /* Sprites Plane 3+4 */
	ROM_LOAD( "da03",      0x10000, 0x8000, CRC(cf187f25) SHA1(c0d2d85f85340c12c1b61cc062506ffa4841ef78) ) /* Sprites Plane 5+6 */

	ROM_REGION( 0x0c000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "da06",      0x00000, 0x8000, CRC(9c6a51b3) SHA1(0d623e8fba9373979a93f97cdfcf311c7e7f561a) ) /* Tiles*/
	ROM_LOAD( "da07",      0x08000, 0x4000, CRC(56df4077) SHA1(f4b8047c3b4d5897ba91489bc76a9504d9941072) ) /* Tiles*/

	ROM_REGION( 0x0040, REGION_PROMS, 0 )
	ROM_LOAD( "82s123.7h", 0x00000, 0x0020, CRC(b9933663) SHA1(5d5c840caa0b8416ed7dd4890dd5f3e4a9e86511) )
	ROM_LOAD( "82s123.9n", 0x00020, 0x0020, CRC(06491e53) SHA1(d6cf5003798f9a9d555bca97844dcb2966cbac9d) )
ROM_END

ROM_START( battlan3 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for main CPU */
	/* first half of bl_04.rom will be copied at 0x4000-0x7fff */
	ROM_LOAD( "bl_04.rom", 0x4000, 0x8000, CRC(5681564c) SHA1(25b3a715e91976830d87c7c45b93b473df709241) )
	ROM_LOAD( "bl_05.rom", 0x8000, 0x8000, CRC(001c4bbe) SHA1(4320c0a85b5b3505ac7292673759e5288cf4187f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64K for slave CPU */
	ROM_LOAD( "bl_04.rom", 0x0000, 0x8000, CRC(5681564c) SHA1(25b3a715e91976830d87c7c45b93b473df709241) )	/* ...second half goes here */
	ROM_LOAD( "da02-2",    0x8000, 0x8000, CRC(69d8dafe) SHA1(a7dab13d7f05bf8e3220bb8193066e9b45c86a17) )

	ROM_REGION( 0x18000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "da05",      0x00000, 0x8000, CRC(834829d4) SHA1(d56781d2a7ef89b645a637166cd5acde6a65f7f9) ) /* Sprites Plane 1+2 */
	ROM_LOAD( "da04",      0x08000, 0x8000, CRC(f083fd4c) SHA1(eb8f079776a0efd898574874d21f865311ecd8ba) ) /* Sprites Plane 3+4 */
	ROM_LOAD( "da03",      0x10000, 0x8000, CRC(cf187f25) SHA1(c0d2d85f85340c12c1b61cc062506ffa4841ef78) ) /* Sprites Plane 5+6 */

	ROM_REGION( 0x0c000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "da06",      0x00000, 0x8000, CRC(9c6a51b3) SHA1(0d623e8fba9373979a93f97cdfcf311c7e7f561a) ) /* Tiles*/
	ROM_LOAD( "da07",      0x08000, 0x4000, CRC(56df4077) SHA1(f4b8047c3b4d5897ba91489bc76a9504d9941072) ) /* Tiles*/

	ROM_REGION( 0x0040, REGION_PROMS, 0 )
	ROM_LOAD( "82s123.7h", 0x00000, 0x0020, CRC(b9933663) SHA1(5d5c840caa0b8416ed7dd4890dd5f3e4a9e86511) )
	ROM_LOAD( "82s123.9n", 0x00020, 0x0020, CRC(06491e53) SHA1(d6cf5003798f9a9d555bca97844dcb2966cbac9d) )
ROM_END


GAME( 1986, battlane, 0,        battlane, battlane, 0, ROT90, "Technos (Taito license)", "Battle Lane! Vol. 5 (set 1)" )
GAME( 1986, battlan2, battlane, battlane, battlane, 0, ROT90, "Technos (Taito license)", "Battle Lane! Vol. 5 (set 2)" )
GAME( 1986, battlan3, battlane, battlane, battlane, 0, ROT90, "Technos (Taito license)", "Battle Lane! Vol. 5 (set 3)" )
