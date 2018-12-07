/***************************************************************************

Phozon (Namco 1983)

	Manuel Abadia (emumanu@hotmail.com)

Phozon Memory Map (preliminary)

CPU #1: (MAIN CPU)
0000-03ff   video RAM
0400-07ff   color RAM
0800-1fff   shared RAM with CPU #2
4040-43ff   shared RAM with CPU #3
4800-480f	custom IO chip #1
4810-481f	custom IO chip #2
5000-5009	???
500a-500b	CPU #3 enable
500c-500d	CPU #2 enable
500e-500f	???
7000		watchdog reset
8000-9fff	ROM
a000-bfff   ROM
c000-dfff   ROM
e000-ffff   ROM

CPU #2: (SUB CPU)
0000-03ff   video RAM (shared with CPU #1)
0400-07ff   color RAM (shared with CPU #1)
0800-1fff   shared RAM with CPU #1
a000-a7ff   RAM
e000-ffff   ROM

CPU #3: (SOUND CPU)
0000-0040   sound registers
0040-03ff   shared RAM with CPU #1
e000-ffff   ROM

TODO: cocktail mode

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *phozon_snd_sharedram;
extern unsigned char *phozon_spriteram;
extern unsigned char *phozon_customio_1, *phozon_customio_2;
extern unsigned char *mappy_soundregs;

/* memory functions */
READ_HANDLER( phozon_spriteram_r );
READ_HANDLER( phozon_snd_sharedram_r );
WRITE_HANDLER( phozon_spriteram_w );
WRITE_HANDLER( phozon_snd_sharedram_w );

/* custom IO chips & CPU functions */
READ_HANDLER( phozon_customio_1_r );
READ_HANDLER( phozon_customio_2_r );
WRITE_HANDLER( phozon_customio_1_w );
WRITE_HANDLER( phozon_customio_2_w );
WRITE_HANDLER( phozon_cpu2_enable_w );
WRITE_HANDLER( phozon_cpu3_enable_w );
WRITE_HANDLER( phozon_cpu3_reset_w );
extern MACHINE_INIT( phozon );

/* video functions */
extern VIDEO_START( phozon );
extern PALETTE_INIT( phozon );
extern VIDEO_UPDATE( phozon );

	/* CPU 1 (MAIN CPU) read addresses */
static MEMORY_READ_START( readmem_cpu1 )
	{ 0x0000, 0x03ff, videoram_r },			/* video RAM */
	{ 0x0400, 0x07ff, colorram_r },										/* color RAM */
	{ 0x0800, 0x1fff, phozon_spriteram_r },			/* shared RAM with CPU #2/sprite RAM*/
	{ 0x4040, 0x43ff, phozon_snd_sharedram_r },  /* shared RAM with CPU #3 */
	{ 0x4800, 0x480f, phozon_customio_1_r },		/* custom I/O chip #1 interface */
	{ 0x4810, 0x481f, phozon_customio_2_r },		/* custom I/O chip #2 interface */
	{ 0x8000, 0xffff, MRA_ROM },										/* ROM */
MEMORY_END

	/* CPU 1 (MAIN CPU) write addresses */
static MEMORY_WRITE_START( writemem_cpu1 )
	{ 0x0000, 0x03ff, videoram_w, &videoram, &videoram_size },				/* video RAM */
	{ 0x0400, 0x07ff, colorram_w, &colorram },  /* color RAM */
	{ 0x0800, 0x1fff, phozon_spriteram_w, &phozon_spriteram },		/* shared RAM with CPU #2/sprite RAM*/
	{ 0x4000, 0x403f, MWA_RAM },				/* initialized but probably unused */
	{ 0x4040, 0x43ff, phozon_snd_sharedram_w, &phozon_snd_sharedram }, /* shared RAM with CPU #3 */
	{ 0x4800, 0x480f, phozon_customio_1_w, &phozon_customio_1 },	/* custom I/O chip #1 interface */
	{ 0x4810, 0x481f, phozon_customio_2_w, &phozon_customio_2 },	/* custom I/O chip #2 interface */
	{ 0x4820, 0x483f, MWA_RAM },				/* initialized but probably unused */
	{ 0x5000, 0x5007, MWA_NOP },				/* ??? */
	{ 0x5008, 0x5008, phozon_cpu3_reset_w },	/* reset SOUND CPU? */
	{ 0x5009, 0x5009, MWA_NOP },				/* ??? */
	{ 0x500a, 0x500b, phozon_cpu3_enable_w },	/* SOUND CPU enable */
	{ 0x500c, 0x500d, phozon_cpu2_enable_w },	/* SUB CPU enable */
	{ 0x500e, 0x500f, MWA_NOP },				/* ??? */
	{ 0x7000, 0x7000, watchdog_reset_w },	 	/* watchdog reset */
	{ 0x8000, 0xffff, MWA_ROM },				/* ROM */
MEMORY_END

	/* CPU 2 (SUB CPU) read addresses */
static MEMORY_READ_START( readmem_cpu2 )
	{ 0x0000, 0x03ff, videoram_r },			/* video RAM */
	{ 0x0400, 0x07ff, colorram_r },			/* color RAM */
	{ 0x0800, 0x1fff, phozon_spriteram_r },	/* shared RAM with CPU #1/sprite RAM*/
	{ 0xa000, 0xa7ff, MRA_RAM },			/* RAM */
	{ 0xe000, 0xffff, MRA_ROM },			/* ROM */
MEMORY_END

	/* CPU 2 (SUB CPU) write addresses */
static MEMORY_WRITE_START( writemem_cpu2 )
	{ 0x0000, 0x03ff, videoram_w },			/* video RAM */
	{ 0x0400, 0x07ff, colorram_w },			/* color RAM */
	{ 0x0800, 0x1fff, phozon_spriteram_w },	/* shared RAM with CPU #1/sprite RAM*/
	{ 0xa000, 0xa7ff, MWA_RAM },			/* RAM */
	{ 0xe000, 0xffff, MWA_ROM },			/* ROM */
MEMORY_END

	/* CPU 3 (SOUND CPU) read addresses */
static MEMORY_READ_START( readmem_cpu3 )
	{ 0x0000, 0x003f, MRA_RAM },				/* sound registers */
	{ 0x0040, 0x03ff, phozon_snd_sharedram_r }, /* shared RAM with CPU #1 */
	{ 0xe000, 0xffff, MRA_ROM },				/* ROM */
MEMORY_END

	/* CPU 3 (SOUND CPU) write addresses */
static MEMORY_WRITE_START( writemem_cpu3 )
	{ 0x0000, 0x003f, mappy_sound_w, &mappy_soundregs },/* sound registers */
	{ 0x0040, 0x03ff, phozon_snd_sharedram_w },			/* shared RAM with the main CPU */
	{ 0xe000, 0xffff, MWA_ROM },						/* ROM */
MEMORY_END

/* The dipswitches and player inputs are not memory mapped, they are handled by an I/O chip. */
INPUT_PORTS_START( phozon )
	PORT_START  /* DSW0 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )

	PORT_START  /* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x05, "5" )
	PORT_DIPSETTING(    0x06, "6" )
	PORT_DIPSETTING(    0x07, "7" )
	PORT_SERVICE( 0x08, IP_ACTIVE_HIGH )
/* Todo: those are different for 4 and 5 lives */
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0xc0, "20k 80k" )
	PORT_DIPSETTING(    0x40, "30k 60k" )
	PORT_DIPSETTING(    0x80, "30k 120k and every 120k" )
	PORT_DIPSETTING(    0x00, "30k 100k" )

	PORT_START  /* IN0 */
	PORT_BIT_IMPULSE(   0x01, IP_ACTIVE_HIGH, IPT_START1, 1 )
	PORT_BIT_IMPULSE(   0x02, IP_ACTIVE_HIGH, IPT_START2, 1 )
	PORT_BIT_IMPULSE(   0x10, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE(   0x20, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START  /* IN1 */
	PORT_BIT(   0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT(   0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT(   0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT(   0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT(   0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(   0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(   0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(   0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )

	PORT_START  /* IN2 */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1, 1 )
	PORT_BITX(  0x02, IP_ACTIVE_HIGH, IPT_BUTTON1, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2, 1 )
	PORT_BITX(  0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,            /* 8*8 characters */
	256,            /* 256 characters */
	2,				/* 2 bits per pixel */
	{ 0, 4 },
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },   /* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },   /* characters are rotated 90 degrees */
	16*8			/* every char takes 16 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,                                         /* 16*16 sprites */
	128,                                           /* 128 sprites */
	2,                                             /* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8                                           /* every sprite takes 64 bytes */
};

static struct GfxLayout spritelayout8 =
{
	8,8,                                         /* 16*16 sprites */
	512,                                           /* 128 sprites */
	2,                                             /* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8                                           /* every sprite takes 64 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,       0, 64 },
	{ REGION_GFX2, 0, &charlayout,       0, 64 },
	{ REGION_GFX3, 0, &spritelayout,  64*4, 64 },
	{ REGION_GFX3, 0, &spritelayout8, 64*4, 64 },
	{ -1 } /* end of table */
};

static struct namco_interface namco_interface =
{
	24000,	/* sample rate */
	8,		/* number of voices */
	100,	/* playback volume */
	REGION_SOUND1	/* memory region */
};

static MACHINE_DRIVER_START( phozon )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809,	1536000)	/* MAIN CPU, same as Gaplus? */
	MDRV_CPU_MEMORY(readmem_cpu1,writemem_cpu1)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(M6809,	1536000)	/* SUB CPU, same as Gaplus? */
	MDRV_CPU_MEMORY(readmem_cpu2,writemem_cpu2)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(M6809,	1536000)	/* SOUND CPU, same as Gaplus? */
	MDRV_CPU_MEMORY(readmem_cpu3,writemem_cpu3)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* a high value to ensure proper synchronization of the CPUs */
	
	MDRV_MACHINE_INIT(phozon)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(64*4+64*8)

	MDRV_PALETTE_INIT(phozon)
	MDRV_VIDEO_START(phozon)
	MDRV_VIDEO_UPDATE(phozon)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO_15XX, namco_interface)
MACHINE_DRIVER_END



ROM_START( phozon )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code for the MAIN CPU  */
	ROM_LOAD( "6e.rom", 0x8000, 0x2000, CRC(a6686af1) SHA1(87a948b289356675d0418c87c3c0ae36ceba3ee0) )
	ROM_LOAD( "6h.rom", 0xa000, 0x2000, CRC(72a65ba0) SHA1(b1d5146c009469d4c6695f08ea2c6ad5d05b5b9b) )
	ROM_LOAD( "6c.rom", 0xc000, 0x2000, CRC(f1fda22e) SHA1(789881e94743efae01c63c1e3ce8d039cfa0324c) )
	ROM_LOAD( "6d.rom", 0xe000, 0x2000, CRC(f40e6df0) SHA1(48585ac1eff8fb7ed35f56c767d725cae88ff128) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )     /* 64k for the SUB CPU */
	ROM_LOAD( "9r.rom", 0xe000, 0x2000, CRC(5d9f0a28) SHA1(2caef680229180b237f8c4becf052f1a96592efd) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )     /* 64k for the SOUND CPU */
	ROM_LOAD( "3b.rom", 0xe000, 0x2000, CRC(5a4b3a79) SHA1(2774681ea668403de31ea218d5df3ce64e3b9243) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "7j.rom", 0x0000, 0x1000, CRC(27f9db5b) SHA1(12ef817136b45927d7f279952fa19049a1349f60) ) /* characters (set 1) */

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "8j.rom", 0x0000, 0x1000, CRC(15b12ef8) SHA1(e3303656b4e8b988e55a9551e5344e289958f677) ) /* characters (set 2) */

	ROM_REGION( 0x2000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "5t.rom", 0x0000, 0x2000, CRC(d50f08f8) SHA1(4e9dda0d5ad1c1b8b3be7edb05b3060f5f63a9c7) ) /* sprites */

	ROM_REGION( 0x0520, REGION_PROMS, 0 )
	ROM_LOAD( "red.prm",     0x0000, 0x0100, CRC(a2880667) SHA1(b24d9b3354d20a7ecc02c428245669c6c86bfd61) ) /* red palette ROM (4 bits) */
	ROM_LOAD( "green.prm",   0x0100, 0x0100, CRC(d6e08bef) SHA1(b0ca7f8a77b7208cf974a8cc565fc91b7f40f51f) ) /* green palette ROM (4 bits) */
	ROM_LOAD( "blue.prm",    0x0200, 0x0100, CRC(b2d69c72) SHA1(e7b1ed698ab0e87872cb3a8f3ec102ca3a753259) ) /* blue palette ROM (4 bits) */
	ROM_LOAD( "chr.prm",     0x0300, 0x0100, CRC(429e8fee) SHA1(7b1899ca3f33f4561b572de1f24d9ea9d7d84b59) ) /* characters */
	ROM_LOAD( "sprite.prm",  0x0400, 0x0100, CRC(9061db07) SHA1(4305d37e613e1d15d37539b152c948648189c2cd) ) /* sprites */
	ROM_LOAD( "palette.prm", 0x0500, 0x0020, CRC(60e856ed) SHA1(dcc9a2dfc728b9ca1ab895008de07e20ebed9da3) ) /* palette (unused?) */

	ROM_REGION( 0x0100, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "sound.prm", 0x0000, 0x0100, CRC(ad43688f) SHA1(072f427453efb1dda8147da61804fff06e1bc4d5) )
ROM_END



GAMEX( 1983, phozon, 0, phozon, phozon, 0, ROT90, "Namco", "Phozon (Japan)", GAME_NO_COCKTAIL )
