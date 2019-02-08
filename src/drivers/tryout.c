/***************************************************************

 Pro Yakyuu Nyuudan Test Tryout (JPN Ver.)
 (c) 1985 Data East

 preliminary driver by Pierpaolo Prazzoli

 To Do:
	- Unmapped reads
	- Unknown memory areas
	- Why does it write to rom area?
	- Sprites
	- Colors
	- Sound
	- Inputs

****************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static struct tilemap *fg_tilemap;

static void get_tile_info(int tile_index)
{
	int code, attr;

	code = videoram[tile_index];
	attr = videoram[tile_index + 0x400];

	code |= ((attr & 0x03) << 8);

/*	attr & 0x0c ?*/

	SET_TILE_INFO(0, code, attr >> 4, 0)
}

static WRITE_HANDLER( tryout_videoram_w )
{
	if( videoram[offset] != data )
	{
		videoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap, offset & 0x3ff);
	}
}

static WRITE_HANDLER( tryout_nmi_reset_w )
{
	cpu_set_nmi_line( 0, CLEAR_LINE );
}

static WRITE_HANDLER( tryout_sound_command_w )
{
	soundlatch_w(0,data);
	
	cpu_set_irq_line(1, 0, PULSE_LINE );
}

static MEMORY_READ_START( readmem )
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x2000, 0x2fff, MRA_RAM },
	{ 0x3000, 0x3fff, MRA_RAM },
	{ 0x4000, 0xcfff, MRA_ROM },
	{ 0xe000, 0xe000, input_port_0_r },
	{ 0xe002, 0xe002, input_port_1_r },
	{ 0xe003, 0xe003, input_port_2_r },
	{ 0xf000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( writemem )
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x1000, 0x17ff, tryout_videoram_w , &videoram },
	{ 0x4000, 0xcfff, MWA_ROM },
	{ 0xd000, 0xd7ff, MWA_RAM }, /* ?*/
	{ 0xe301, 0xe301, MWA_RAM },
	{ 0xe302, 0xe302, MWA_RAM },
	{ 0xe400, 0xe404, MWA_RAM },
	{ 0xe410, 0xe410, MWA_RAM },
	{ 0xe414, 0xe414, tryout_sound_command_w }, /* maybe*/
	{ 0xe417, 0xe417, tryout_nmi_reset_w }, /* maybe*/
	{ 0xf000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x4000, 0x4000, YM2203_status_port_0_r },
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0xc000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x4000, 0x4000, YM2203_control_port_0_w },
	{ 0x4001, 0x4001, YM2203_write_port_0_w },
	{ 0xc000, 0xffff, MWA_ROM },
MEMORY_END

INPUT_PORTS_START( tryout )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_BUTTON1 )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_BUTTON2 )

	PORT_START
	PORT_BIT( 0x7f, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START
	PORT_BIT( 0x3f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN1, 1 )

INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	RGN_FRAC(1,2),
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 3, 2, 1, 0, RGN_FRAC(1,2)+3, RGN_FRAC(1,2)+2, RGN_FRAC(1,2)+1, RGN_FRAC(1,2)+0 },
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(0,3), RGN_FRAC(1,3), RGN_FRAC(2,3) },
	{ 7, 6, 5, 4, 3, 2, 1, 0,
			16*8+7, 16*8+6, 16*8+5, 16*8+4, 16*8+3, 16*8+2, 16*8+1, 16*8+0 },
	{ 15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8,
			7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 0x20 },
	{ REGION_GFX2, 0, &spritelayout, 0, 0x20 },
	{ -1 }
};

UINT32 get_fg_memory_offset_tryout( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows )
{
	return (row ^ 0x1f) + (col << 5);
}

VIDEO_START( tryout )
{	
	fg_tilemap = tilemap_create(get_tile_info,get_fg_memory_offset_tryout,TILEMAP_OPAQUE,8,8,32,32);

/*	tilemap_set_transparent_pen(fg_tilemap,0);*/

	return 0;
}

VIDEO_UPDATE( tryout )
{
	tilemap_draw(bitmap,cliprect,fg_tilemap,0,0);
}

static void tryout_snd_irq(int linestate)
{
	cpu_set_irq_line(1,0,linestate);
}

static struct YM2203interface ym2203_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz */
	{ YM2203_VOL(50,50) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ tryout_snd_irq },
};

static INTERRUPT_GEN( tryout_interrupt )
{
	if (input_port_3_r(0) & 0xc0)
		cpu_set_nmi_line(0, ASSERT_LINE);
/*		nmi_line_pulse();*/
}

PALETTE_INIT( tryout )
{
	int i;

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,r,g,b;

		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		palette_set_color(i,r,g,b);
	}
}

static MACHINE_DRIVER_START( tryout )
	MDRV_CPU_ADD(M6502, 2000000)		 /* ?? */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(tryout_interrupt,1)

	MDRV_CPU_ADD(M6502, 1500000)		/* ?? */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	
	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 0*8, 32*8-1)

	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(0x20)
	MDRV_PALETTE_INIT(tryout)

	MDRV_VIDEO_START(tryout)
	MDRV_VIDEO_UPDATE(tryout)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
MACHINE_DRIVER_END

ROM_START( tryout )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) 
	ROM_LOAD( "ch10-1.bin",   0x04000, 0x4000, CRC(d046231b) SHA1(145f9e9b0707824f7ae6d1587754b28c17907807) ) 
	ROM_LOAD( "ch12.bin",     0x08000, 0x4000, CRC(bcd221be) SHA1(69869de8b5d56a97e2cd15fa275527aa767f1e44) ) 
	ROM_LOAD( "ch11.bin",     0x0c000, 0x4000, CRC(4d00b6f0) SHA1(cc1e700b8547672d7dd1d262c6181a5c321fbf72) ) 

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "ch00-1.bin",   0x0c000, 0x4000, CRC(8b33d968) SHA1(cf44529e5577d09978b87dc2bbe1415babbf36a0) ) 

	ROM_REGION( 0x4000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ch13.bin",     0x00000, 0x4000, CRC(a9619c58) SHA1(92528b1c4afc95394ac8cad5b37f23da0c6a5310) ) 

	ROM_REGION( 0x24000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ch09.bin",     0x00000, 0x4000, CRC(9c5e275b) SHA1(83b29996573d85c73bb4b63086c7a624fad19bde) ) 
	ROM_LOAD( "ch08.bin",     0x04000, 0x4000, CRC(88396abb) SHA1(2865a265ddfb91c2ad2770da5e0d84a544f3c419) ) 
	ROM_LOAD( "ch07.bin",     0x08000, 0x4000, CRC(901b5f5e) SHA1(f749b5ec0c51c66655798e8a37c887870370991e) ) 
	ROM_LOAD( "ch06.bin",     0x0c000, 0x4000, CRC(d937e326) SHA1(5870a82b02438f2fdae089f6d1b8e9ce13d213a6) ) 
	ROM_LOAD( "ch05.bin",     0x10000, 0x4000, CRC(27f0e7be) SHA1(5fa2bd666d012addfb836d009f962f89e4a00b2d) ) 
	ROM_LOAD( "ch04.bin",     0x14000, 0x4000, CRC(019e0b75) SHA1(4bfd7cd6c28ec6dfaf8e9bf009716e92759f06c2) ) 	
	ROM_LOAD( "ch03.bin",     0x18000, 0x4000, CRC(b87e2464) SHA1(0089c0ff421929345a1d21951789a6374e0019ff) ) 
	ROM_LOAD( "ch02.bin",     0x1c000, 0x4000, CRC(62369772) SHA1(89f360003e916bee76d74b7e046bf08349726fda) ) 
	ROM_LOAD( "ch01.bin",     0x20000, 0x4000, CRC(ee6d57b5) SHA1(7dd2f3b962f088fcbc40fcb74c0a56783857fb7b) ) 

	ROM_REGION( 0x20, REGION_PROMS, 0 ) 
	ROM_LOAD( "ch14.bpr",     0x00000, 0x0020, CRC(8ce19925) SHA1(12f8f6022f1148b6ba1d019a34247452637063a7) ) 
ROM_END

DRIVER_INIT( tryout )
{
	/* ?*/
	install_mem_write_handler( 0, 0xc800, 0xcfff, MWA_NOP );

	/* interrupt_enable_w ?*/
	install_mem_write_handler( 1, 0xd000, 0xd000, MWA_NOP );
}

GAMEX( 1985, tryout, 0, tryout, tryout, tryout, ROT90, "Data East", "Pro Yakyuu Nyuudan Test Tryout (JPN Ver.)", GAME_NOT_WORKING | GAME_WRONG_COLORS | GAME_IMPERFECT_GRAPHICS | GAME_NO_SOUND )
