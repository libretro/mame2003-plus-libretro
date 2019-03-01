/***************************************************************************

Return of the Invaders

driver by Jarek Parchanski, Andrea Mazzoleni

the game was developed by UPL for Taito.

***************************************************************************/

#include "driver.h"

/* in machine */
READ_HANDLER( retofinv_68705_portA_r );
WRITE_HANDLER( retofinv_68705_portA_w );
WRITE_HANDLER( retofinv_68705_ddrA_w );
READ_HANDLER( retofinv_68705_portB_r );
WRITE_HANDLER( retofinv_68705_portB_w );
WRITE_HANDLER( retofinv_68705_ddrB_w );
READ_HANDLER( retofinv_68705_portC_r );
WRITE_HANDLER( retofinv_68705_portC_w );
WRITE_HANDLER( retofinv_68705_ddrC_w );
WRITE_HANDLER( retofinv_mcu_w );
READ_HANDLER( retofinv_mcu_r );
READ_HANDLER( retofinv_mcu_status_r );

/* in vidhrdw */
VIDEO_START( retofinv );
PALETTE_INIT( retofinv );
VIDEO_UPDATE( retofinv );
READ_HANDLER( retofinv_bg_videoram_r );
READ_HANDLER( retofinv_fg_videoram_r );
READ_HANDLER( retofinv_bg_colorram_r );
READ_HANDLER( retofinv_fg_colorram_r );
WRITE_HANDLER( retofinv_bg_videoram_w );
WRITE_HANDLER( retofinv_fg_videoram_w );
WRITE_HANDLER( retofinv_bg_colorram_w );
WRITE_HANDLER( retofinv_fg_colorram_w );
WRITE_HANDLER( retofinv_flip_screen_w );

extern size_t retofinv_videoram_size;
extern unsigned char *retofinv_sprite_ram1;
extern unsigned char *retofinv_sprite_ram2;
extern unsigned char *retofinv_sprite_ram3;
extern unsigned char *retofinv_fg_videoram;
extern unsigned char *retofinv_bg_videoram;
extern unsigned char *retofinv_fg_colorram;
extern unsigned char *retofinv_bg_colorram;
extern unsigned char *retofinv_fg_char_bank;
extern unsigned char *retofinv_bg_char_bank;

static unsigned char cpu2_m6000=0;

#if 0
static MACHINE_INIT( retofinv )
{
	cpu2_m6000 = 0;
}
#endif

static unsigned char *sharedram;

static READ_HANDLER( retofinv_shared_ram_r )
{
	return sharedram[offset];
}

static WRITE_HANDLER( retofinv_shared_ram_w )
{
	sharedram[offset] = data;
}

static WRITE_HANDLER( reset_cpu2_w )
{
     if (data)
	    cpu_set_reset_line(2,PULSE_LINE);
}

static WRITE_HANDLER( reset_cpu1_w )
{
    if (data)
	    cpu_set_reset_line(1,PULSE_LINE);
}

static WRITE_HANDLER( cpu1_halt_w )
{
	cpu_set_halt_line(1, data ? CLEAR_LINE : ASSERT_LINE);
}

static WRITE_HANDLER( cpu2_m6000_w )
{
	cpu2_m6000 = data;
}

static READ_HANDLER( cpu0_mf800_r )
{
	return cpu2_m6000;
}

static WRITE_HANDLER( soundcommand_w )
{
      soundlatch_w(0, data);
      cpu_set_irq_line(2, 0, HOLD_LINE);
}

static MEMORY_READ_START( readmem )
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x7b00, 0x7b00, MRA_NOP },	/* space for diagnostic ROM? The code looks */
									/* for a string here, and jumps if it's present */
	{ 0x8000, 0x83ff, retofinv_fg_videoram_r },
	{ 0x8400, 0x87ff, retofinv_fg_colorram_r },
	{ 0x8800, 0x9fff, MRA_RAM },
	{ 0xa000, 0xa3ff, retofinv_bg_videoram_r },
	{ 0xa400, 0xa7ff, retofinv_bg_colorram_r },
	{ 0xc800, 0xc800, MRA_NOP },
	{ 0xc000, 0xc000, input_port_1_r },
	{ 0xc001, 0xc001, input_port_2_r },
	{ 0xc002, 0xc002, MRA_NOP },	/* bit 7 must be 0, otherwise game resets */
	{ 0xc003, 0xc003, retofinv_mcu_status_r },
	{ 0xc004, 0xc004, input_port_0_r },
	{ 0xc005, 0xc005, input_port_3_r },
	{ 0xc006, 0xc006, input_port_5_r },
	{ 0xc007, 0xc007, input_port_4_r },
	{ 0xe000, 0xe000, retofinv_mcu_r },
	{ 0xf800, 0xf800, cpu0_mf800_r },
MEMORY_END

static MEMORY_WRITE_START( writemem )
	{ 0x0000, 0x5fff, MWA_ROM },
/*	{ 0x7fff, 0x7fff, MWA_NOP },*/
	{ 0x8000, 0x83ff, retofinv_fg_videoram_w, &retofinv_fg_videoram, &retofinv_videoram_size },
	{ 0x8400, 0x87ff, retofinv_fg_colorram_w, &retofinv_fg_colorram },
	{ 0x8800, 0x9fff, MWA_RAM, &sharedram },
	{ 0x8f00, 0x8f7f, MWA_RAM, &retofinv_sprite_ram1 },	/* covered by the above, */
	{ 0x9700, 0x977f, MWA_RAM, &retofinv_sprite_ram2 },	/* here only to */
	{ 0x9f00, 0x9f7f, MWA_RAM, &retofinv_sprite_ram3 },	/* initialize the pointers */
	{ 0xa000, 0xa3ff, retofinv_bg_videoram_w, &retofinv_bg_videoram },
	{ 0xa400, 0xa7ff, retofinv_bg_colorram_w, &retofinv_bg_colorram },
	{ 0xb800, 0xb800, retofinv_flip_screen_w },
	{ 0xb801, 0xb801, MWA_RAM, &retofinv_fg_char_bank },
	{ 0xb802, 0xb802, MWA_RAM, &retofinv_bg_char_bank },
	{ 0xc800, 0xc800, MWA_NOP },
	{ 0xc801, 0xc801, reset_cpu2_w },
	{ 0xc802, 0xc802, reset_cpu1_w },
	{ 0xc803, 0xc803, MWA_NOP },
	{ 0xc804, 0xc804, MWA_NOP },
	{ 0xc805, 0xc805, cpu1_halt_w },
	{ 0xd800, 0xd800, soundcommand_w },
	{ 0xd000, 0xd000, MWA_NOP },
	{ 0xe800, 0xe800, retofinv_mcu_w },
MEMORY_END

static MEMORY_READ_START( readmem_sub )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x8000, 0x83ff, retofinv_fg_videoram_r },
	{ 0x8400, 0x87ff, retofinv_fg_colorram_r },
	{ 0x8800, 0x9fff, retofinv_shared_ram_r },
	{ 0xa000, 0xa3ff, retofinv_bg_videoram_r },
	{ 0xa400, 0xa7ff, retofinv_bg_colorram_r },
	{ 0xc804, 0xc804, MRA_NOP },
MEMORY_END

static MEMORY_WRITE_START( writemem_sub )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x8000, 0x83ff, retofinv_fg_videoram_w },
	{ 0x8400, 0x87ff, retofinv_fg_colorram_w },
	{ 0x8800, 0x9fff, retofinv_shared_ram_w },
	{ 0xa000, 0xa3ff, retofinv_bg_videoram_w },
	{ 0xa400, 0xa7ff, retofinv_bg_colorram_w },
	{ 0xc804, 0xc804, MWA_NOP },
MEMORY_END

static MEMORY_READ_START( readmem_sound )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x4000, 0x4000, soundlatch_r },
	{ 0xe000, 0xe000, MRA_NOP },  		/* Rom version ? */
MEMORY_END

static MEMORY_WRITE_START( writemem_sound )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x27ff, MWA_RAM },
	{ 0x6000, 0x6000, cpu2_m6000_w },
	{ 0x8000, 0x8000, SN76496_0_w },
	{ 0xa000, 0xa000, SN76496_1_w },
MEMORY_END

static MEMORY_READ_START( mcu_readmem )
	{ 0x0000, 0x0000, retofinv_68705_portA_r },
	{ 0x0001, 0x0001, retofinv_68705_portB_r },
	{ 0x0002, 0x0002, retofinv_68705_portC_r },
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( mcu_writemem )
	{ 0x0000, 0x0000, retofinv_68705_portA_w },
	{ 0x0001, 0x0001, retofinv_68705_portB_w },
	{ 0x0002, 0x0002, retofinv_68705_portC_w },
	{ 0x0004, 0x0004, retofinv_68705_ddrA_w },
	{ 0x0005, 0x0005, retofinv_68705_ddrB_w },
	{ 0x0006, 0x0006, retofinv_68705_ddrC_w },
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
MEMORY_END



INPUT_PORTS_START( retofinv )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH,IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT  | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x03, "30k, 80k & every 80k" )
	PORT_DIPSETTING(    0x02, "30k, 80k" )
	PORT_DIPSETTING(    0x01, "30k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x04, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START      /* DSW3 modified by Shingo Suzuki 1999/11/03 */
	PORT_BITX(    0x01, 0x01, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Push Start to Skip Stage", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Coin Per Play Display" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, "Year Display" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x80, "A and B" )
	PORT_DIPSETTING(    0x00, "A only" )

        PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x0f, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_8C ) )
	PORT_DIPNAME( 0xf0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_8C ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	1,	/* 1 bits per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },		/* x bit */
	{ 56, 48, 40, 32, 24, 16, 8, 0 },	/* y bit */
	8*8 	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout bglayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	4,	/* 4 bits per pixel */
	{ 0, 0x2000*8+4, 0x2000*8, 4 },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 characters */
	256,	/* 256 characters */
	4,	/* 4 bits per pixel */
	{ 0, 0x4000*8+4, 0x4000*8, 4 },
	{ 24*8+3, 24*8+2, 24*8+1, 24*8+0, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
	  8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 39*8, 38*8, 37*8, 36*8, 35*8, 34*8, 33*8, 32*8,
	  7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
  	64*8	/* every char takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,             0, 256 },
	{ REGION_GFX2, 0, &bglayout,           256*2,  64 },
	{ REGION_GFX3, 0, &spritelayout, 64*16+256*2,  64 },
	{ -1 } /* end of array */
};



static struct SN76496interface sn76496_interface =
{
	2,		/* 2 chips */
	{ 3072000, 3072000 },	/* ??? */
	{ 80, 80 }
};



static MACHINE_DRIVER_START( retofinv )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem_sub,writemem_sub)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem_sound,writemem_sound)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,2)

	MDRV_CPU_ADD(M68705,8000000/2)  /* 4 MHz */
	MDRV_CPU_MEMORY(mcu_readmem,mcu_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - enough for the sound CPU to read all commands */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(256*2+64*16+64*16)

	MDRV_PALETTE_INIT(retofinv)
	MDRV_VIDEO_START(retofinv)
	MDRV_VIDEO_UPDATE(retofinv)

	/* sound hardware */
	MDRV_SOUND_ADD(SN76496, sn76496_interface)
MACHINE_DRIVER_END


/* bootleg has no mcu */
static MACHINE_DRIVER_START( retofinb )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem_sub,writemem_sub)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem_sound,writemem_sound)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,2)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - enough for the sound CPU to read all commands */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(256*2+64*16+64*16)

	MDRV_PALETTE_INIT(retofinv)
	MDRV_VIDEO_START(retofinv)
	MDRV_VIDEO_UPDATE(retofinv)

	/* sound hardware */
	MDRV_SOUND_ADD(SN76496, sn76496_interface)
MACHINE_DRIVER_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( retofinv )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "ic70.rom", 0x0000, 0x2000, CRC(eae7459d) SHA1(c105f6adbd4c09decaad68ed13163d8f9b55e646) )
	ROM_LOAD( "ic71.rom", 0x2000, 0x2000, CRC(72895e37) SHA1(42fb904338e9f92a79d587eac401d456e7fb6e55) )
	ROM_LOAD( "ic72.rom", 0x4000, 0x2000, CRC(505dd20b) SHA1(3a34b1515bb834ff9e2d86b0b43a752d9619307b) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for code */
	ROM_LOAD( "ic62.rom", 0x0000, 0x2000, CRC(d2899cc1) SHA1(fdbec743b06f4cdcc134ef863e4e71337ad0b2c5) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for sound cpu */
	ROM_LOAD( "ic17.rom", 0x0000, 0x2000, CRC(9025abea) SHA1(2f03e8572f23624d7cd1215a55109e97fd66e271) )

	ROM_REGION( 0x0800, REGION_CPU4, 0 )	/* 8k for the microcontroller */
	ROM_LOAD( "68705p3.bin", 0x00000, 0x0800, CRC(79bd6ded) SHA1(4967e95b4461c1bfb4e933d1804677799014f77b) )	/* from a bootleg board */

	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ic61.rom", 0x0000, 0x2000, CRC(4e3f501c) SHA1(2d832f4038ae65bfdeedfab870f6f1176ec6b676) )

	ROM_REGION( 0x04000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ic55.rom", 0x0000, 0x2000, CRC(ef7f8651) SHA1(2d91057501e5e9c4255e0d55fff0d99c2a5be7e8) )
	ROM_LOAD( "ic56.rom", 0x2000, 0x2000, CRC(03b40905) SHA1(c10d87796e8a6e6a2a37c6fb713821cc87299cc8) )

	ROM_REGION( 0x08000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "ic8.rom",  0x0000, 0x2000, CRC(6afdeec8) SHA1(782fe0a8aea48c3c270318b7ba011fc6fce0db7a) )
	ROM_LOAD( "ic9.rom",  0x2000, 0x2000, CRC(d3dc9da3) SHA1(0d98d6e993b5a4845a23802751023b7a593dce29) )
	ROM_LOAD( "ic10.rom", 0x4000, 0x2000, CRC(d10b2eed) SHA1(3809a0adf935a119f9ee0d4c24f1456c35d2a6fa) )
	ROM_LOAD( "ic11.rom", 0x6000, 0x2000, CRC(00ca6b3d) SHA1(08ce5b13d5ebc79cc803949f4ba9e630e6cd92b8) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "74s287.p6", 0x0000, 0x0100, CRC(50030af0) SHA1(e748ae0b8702b7d20fb65c254dceee23246b3d13) )	/* palette blue bits   */
	ROM_LOAD( "74s287.o6", 0x0100, 0x0100, CRC(e8f34e11) SHA1(8f438561b8d46ffff00747ed8baf0ebb6a081615) )	/* palette green bits */
	ROM_LOAD( "74s287.q5", 0x0200, 0x0100, CRC(e9643b8b) SHA1(7bbb92a42e7c3effb701fc7b2c24f2470f31b063) )	/* palette red bits  */
	ROM_LOAD( "82s191n",   0x0300, 0x0800, CRC(93c891e3) SHA1(643a0107717b6a434432dda73a0102e6e8adbca7) )	/* lookup table */
ROM_END

ROM_START( retofin1 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "roi.02",  0x0000, 0x2000, CRC(d98fd462) SHA1(fd35e13b7dee58639a01b040b8f84d42bb40b633) )
	ROM_LOAD( "roi.01b", 0x2000, 0x2000, CRC(3379f930) SHA1(c67d687a10b6240bd6e2fbdb15e1b7d276e6fc07) )
	ROM_LOAD( "roi.01",  0x4000, 0x2000, CRC(57679062) SHA1(4f121101ab1cb8de8e693e5984ef23fa587fe696) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for code */
	ROM_LOAD( "ic62.rom", 0x0000, 0x2000, CRC(d2899cc1) SHA1(fdbec743b06f4cdcc134ef863e4e71337ad0b2c5) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for sound cpu */
	ROM_LOAD( "ic17.rom", 0x0000, 0x2000, CRC(9025abea) SHA1(2f03e8572f23624d7cd1215a55109e97fd66e271) )

	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ic61.rom", 0x0000, 0x2000, CRC(4e3f501c) SHA1(2d832f4038ae65bfdeedfab870f6f1176ec6b676) )

	ROM_REGION( 0x04000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ic55.rom", 0x0000, 0x2000, CRC(ef7f8651) SHA1(2d91057501e5e9c4255e0d55fff0d99c2a5be7e8) )
	ROM_LOAD( "ic56.rom", 0x2000, 0x2000, CRC(03b40905) SHA1(c10d87796e8a6e6a2a37c6fb713821cc87299cc8) )

	ROM_REGION( 0x08000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "ic8.rom",  0x0000, 0x2000, CRC(6afdeec8) SHA1(782fe0a8aea48c3c270318b7ba011fc6fce0db7a) )
	ROM_LOAD( "ic9.rom",  0x2000, 0x2000, CRC(d3dc9da3) SHA1(0d98d6e993b5a4845a23802751023b7a593dce29) )
	ROM_LOAD( "ic10.rom", 0x4000, 0x2000, CRC(d10b2eed) SHA1(3809a0adf935a119f9ee0d4c24f1456c35d2a6fa) )
	ROM_LOAD( "ic11.rom", 0x6000, 0x2000, CRC(00ca6b3d) SHA1(08ce5b13d5ebc79cc803949f4ba9e630e6cd92b8) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "74s287.p6", 0x0000, 0x0100, CRC(50030af0) SHA1(e748ae0b8702b7d20fb65c254dceee23246b3d13) )	/* palette blue bits   */
	ROM_LOAD( "74s287.o6", 0x0100, 0x0100, CRC(e8f34e11) SHA1(8f438561b8d46ffff00747ed8baf0ebb6a081615) )	/* palette green bits */
	ROM_LOAD( "74s287.q5", 0x0200, 0x0100, CRC(e9643b8b) SHA1(7bbb92a42e7c3effb701fc7b2c24f2470f31b063) )	/* palette red bits  */
	ROM_LOAD( "82s191n",   0x0300, 0x0800, CRC(93c891e3) SHA1(643a0107717b6a434432dda73a0102e6e8adbca7) )	/* lookup table */
ROM_END

ROM_START( retofin2 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "ri-c.1e", 0x0000, 0x2000, CRC(e3c31260) SHA1(cc8774251c567da2e4a54091223927c95f497fe8) )
	ROM_LOAD( "roi.01b", 0x2000, 0x2000, CRC(3379f930) SHA1(c67d687a10b6240bd6e2fbdb15e1b7d276e6fc07) )
	ROM_LOAD( "ri-a.1c", 0x4000, 0x2000, CRC(3ae7c530) SHA1(5d1be375494fa07124071067661c4bfc2d724d54) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for code */
	ROM_LOAD( "ic62.rom", 0x0000, 0x2000, CRC(d2899cc1) SHA1(fdbec743b06f4cdcc134ef863e4e71337ad0b2c5) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for sound cpu */
	ROM_LOAD( "ic17.rom", 0x0000, 0x2000, CRC(9025abea) SHA1(2f03e8572f23624d7cd1215a55109e97fd66e271) )

	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ic61.rom", 0x0000, 0x2000, CRC(4e3f501c) SHA1(2d832f4038ae65bfdeedfab870f6f1176ec6b676) )

	ROM_REGION( 0x04000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ic55.rom", 0x0000, 0x2000, CRC(ef7f8651) SHA1(2d91057501e5e9c4255e0d55fff0d99c2a5be7e8) )
	ROM_LOAD( "ic56.rom", 0x2000, 0x2000, CRC(03b40905) SHA1(c10d87796e8a6e6a2a37c6fb713821cc87299cc8) )

	ROM_REGION( 0x08000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "ic8.rom",  0x0000, 0x2000, CRC(6afdeec8) SHA1(782fe0a8aea48c3c270318b7ba011fc6fce0db7a) )
	ROM_LOAD( "ic9.rom",  0x2000, 0x2000, CRC(d3dc9da3) SHA1(0d98d6e993b5a4845a23802751023b7a593dce29) )
	ROM_LOAD( "ic10.rom", 0x4000, 0x2000, CRC(d10b2eed) SHA1(3809a0adf935a119f9ee0d4c24f1456c35d2a6fa) )
	ROM_LOAD( "ic11.rom", 0x6000, 0x2000, CRC(00ca6b3d) SHA1(08ce5b13d5ebc79cc803949f4ba9e630e6cd92b8) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "74s287.p6", 0x0000, 0x0100, CRC(50030af0) SHA1(e748ae0b8702b7d20fb65c254dceee23246b3d13) )	/* palette blue bits   */
	ROM_LOAD( "74s287.o6", 0x0100, 0x0100, CRC(e8f34e11) SHA1(8f438561b8d46ffff00747ed8baf0ebb6a081615) )	/* palette green bits */
	ROM_LOAD( "74s287.q5", 0x0200, 0x0100, CRC(e9643b8b) SHA1(7bbb92a42e7c3effb701fc7b2c24f2470f31b063) )	/* palette red bits  */
	ROM_LOAD( "82s191n",   0x0300, 0x0800, CRC(93c891e3) SHA1(643a0107717b6a434432dda73a0102e6e8adbca7) )	/* lookup table */
ROM_END



GAME( 1985, retofinv, 0,        retofinv, retofinv, 0, ROT270, "Taito Corporation", "Return of the Invaders" )
GAME( 1985, retofin1, retofinv, retofinb, retofinv, 0, ROT270, "bootleg", "Return of the Invaders (bootleg set 1)" )
GAME( 1985, retofin2, retofinv, retofinb, retofinv, 0, ROT270, "bootleg", "Return of the Invaders (bootleg set 2)" )
