/* Cross Pang

sample rom says 'Oksan' no indication of year etc.

sound not hooked up (should be simple)
some sprite glitches (should be simple)

driver by Pierpaolo Prazzoli
some bits by David Haywood

*/


#include "driver.h"
#include "vidhrdw/generic.h"

static struct tilemap *bg_layer,*fg_layer;
data16_t *bg_videoram,*fg_videoram;

static WRITE16_HANDLER ( crospang_soundlatch_w )
{
	if(ACCESSING_LSB)
	{
		soundlatch_w(0,data & 0xff);
		cpu_set_irq_line(1,0,HOLD_LINE);
	}
}

static WRITE16_HANDLER ( crospang_scroll_w )
{
	tilemap_set_scrolly(bg_layer,0,data+8);
}

static WRITE16_HANDLER( fg_videoram_w )
{
	COMBINE_DATA(&fg_videoram[offset]);
	tilemap_mark_all_tiles_dirty(fg_layer);
}


static WRITE16_HANDLER( bg_videoram_w )
{
	COMBINE_DATA(&bg_videoram[offset]);
	tilemap_mark_all_tiles_dirty(bg_layer);
}

static MEMORY_READ16_START( crospang_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x120000, 0x1207ff, MRA16_RAM },
	{ 0x122000, 0x1227ff, MRA16_RAM },
	{ 0x200000, 0x2005ff, MRA16_RAM },
	{ 0x210000, 0x2107ff, MRA16_RAM },
	{ 0x280000, 0x280001, input_port_0_word_r },
	{ 0x280002, 0x280003, input_port_1_word_r },
	{ 0x280004, 0x280005, input_port_2_word_r },
	{ 0x320000, 0x32ffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( crospang_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x100000, 0x100005, MWA16_RAM },
	{ 0x100006, 0x100007, crospang_scroll_w },
	{ 0x100008, 0x100009, MWA16_RAM },
	{ 0x10000e, 0x10000f, MWA16_RAM },
	{ 0x120000, 0x1207ff, fg_videoram_w, &fg_videoram },
	{ 0x122000, 0x1227ff, bg_videoram_w, &bg_videoram },
	{ 0x200000, 0x2005ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0x210000, 0x2107ff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x270000, 0x270001, crospang_soundlatch_w },
	{ 0x280000, 0x280001, MWA16_RAM },
	{ 0x320000, 0x32ffff, MWA16_RAM },
MEMORY_END

/* sound cpu */

static MEMORY_READ_START( crospang_sound_readmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( crospang_sound_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
MEMORY_END

static PORT_READ_START( crospang_sound_readport )
	{ 0x00, 0x00, soundlatch_r },
	{ 0x02, 0x02, OKIM6295_status_0_r },
	{ 0x06, 0x06, MRA_NOP  },
MEMORY_END

static PORT_WRITE_START( crospang_sound_writeport )
	{ 0x00, 0x00, MWA_NOP },
	{ 0x01, 0x01, MWA_NOP },
	{ 0x02, 0x02, OKIM6295_data_0_w },
MEMORY_END

INPUT_PORTS_START( crospang )
	PORT_START	/* DSW */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	 | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	 | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	 | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	 | IPF_4WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_DIPNAME( 0x0040, 0x0040, "Unused SW 0-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	 | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN   | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	 | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT  | IPF_4WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_DIPNAME( 0x4000, 0x4000, "Unused SW 0-14" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* DSW */
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown SW 1-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "Unused SW 1-1" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "Unused SW 1-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "Unused SW 1-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Unused SW 1-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "Unused SW 1-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Unused SW 1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Unused SW 1-7" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_DIPNAME( 0x0400, 0x0400, "Unused SW 1-10" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "Unused SW 1-11" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "Unused SW 1-12" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "Unused SW 1-13" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Unused SW 1-14" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "Unused SW 1-15" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START	/* DSW */
	PORT_DIPNAME( 0x0001, 0x0001, "Unknown SW 2-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "Unused SW 2-1" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "Unused SW 2-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "Unused SW 2-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "Unused SW 2-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "Unused SW 2-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Unused SW 2-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Unused SW 2-7" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, "Unknown SW 2-8" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "Unused SW 2-9" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "Unused SW 2-10" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "Unused SW 2-11" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "Unused SW 2-12" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "Unused SW 2-13" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x4000, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x8000, 0x8000, "Unused SW 2-15" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

static struct GfxLayout layout_16x16x4a =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(0,4),RGN_FRAC(1,4),RGN_FRAC(2,4),RGN_FRAC(3,4) },
	{ 128,129,130,131,132,133,134,135, 0,1,2,3,4,5,6,7 },
	{ 8*0, 8*1, 8*2, 8*3, 8*4, 8*5, 8*6, 8*7, 8*8, 8*9, 8*10, 8*11, 8*12, 8*13, 8*14, 8*15 },
	8*32
};

static struct GfxLayout layout_16x16x4 =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(2,4),RGN_FRAC(0,4),RGN_FRAC(3,4),RGN_FRAC(1,4) },
	{ 0,1,2,3,4,5,6,7, 128,129,130,131,132,133,134,135 },
	{ 8*0, 8*1, 8*2, 8*3, 8*4, 8*5, 8*6, 8*7, 8*8, 8*9, 8*10, 8*11, 8*12, 8*13, 8*14, 8*15 },
	8*32
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4a, 0x0000, 0x40 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4,  0x0000, 0x40 }, // [1] Tiles
	{ -1 }
};

/* todo : is this correct? */

static struct OKIM6295interface okim6295_interface =
{
	1,
	{ 6000 },	/* ? guess */
	{ REGION_SOUND1 },
	{ 60 }
};
static void get_bg_tile_info(int tile_index)
{
	int data  = bg_videoram[tile_index];
	int tile  = data & 0xfff;
	int color = (data >> 12) & 0x0f;

	SET_TILE_INFO(1,tile,color + 0x20,0)
}

static void get_fg_tile_info(int tile_index)
{
	int data  = fg_videoram[tile_index];
	int tile  = data & 0xfff;
	int color = (data >> 12) & 0x0f;

	SET_TILE_INFO(1,tile,color + 0x10,0)
}

/*

 offset

	  0		-------yyyyyyyyy  y
			-----hh---------  dy ?
			-??????---------  ?????
			f---------------  flip x

	  1		--ssssssssssssss  sprite
			??--------------  unused

	  2		-------xxxxxxxxx  x
			---cccc---------  colors
			???-------------

	  3		----------------  unused

*/
//static int scrol=0;
//static int scrol2=0;
//static int scrol3=0;

/* todo fix x co-ord */
static void draw_sprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	int offs,fx,fy,x,y,color,sprite,attr,dy,ay;

	for (offs = 0; offs < spriteram_size/2; offs += 4)
	{
		y = spriteram16[offs+0];
		x = spriteram16[offs+2];

		sprite = spriteram16[offs+1];

		attr = spriteram16[offs+3];

		fy = 0;

		dy = ((y & 0x0600) >> 9);

		switch (dy)
		{
			case 0:
			dy = 1;
			break;
			case 1:
			dy = 2;
			break;
			case 2:
			dy = 4;
			break;
			case 3:
			dy = 8;
			break;
		}
		color = (x & 0x1e00) >> 9;
		fx = ((y & 0x8000) >> 15);


		x &= 0x1ff;
		y &= 0x1ff;

		if (x & 0x100) x-= 0x200;
		if (y & 0x100) y-= 0x200;

		x-=44;
		y+=8;

		sprite &= 0x3fff;



		y+= dy*16;

		for (ay=0; ay<dy; ay++)
		{
			drawgfx(bitmap,Machine->gfx[0],
				sprite++,
				color,(1-fx),fy,0x100-x, (0x100-(y - ay * 16)),
				cliprect,TRANSPARENCY_PEN,0);
		}
	}
}

VIDEO_START(crospang)
{
	bg_layer = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,16,16,32,32);
	fg_layer = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);

	if(!bg_layer || !fg_layer)
		return 1;

	tilemap_set_transparent_pen(fg_layer,0);

	return 0;
}

VIDEO_UPDATE(crospang)
{
/*
	if(keyboard_pressed(KEYCODE_X))
	{
		scrol++;
		printf("scrol = %d\n",scrol);
	}

	if(keyboard_pressed(KEYCODE_C))
	{
		scrol--;
		printf("scrol = %d\n",scrol);
	}

	if(keyboard_pressed(KEYCODE_V))
	{
		scrol2++;
		printf("scrol2 = %d\n",scrol2);
	}

	if(keyboard_pressed(KEYCODE_B))
	{
		scrol2--;
		printf("scrol2 = %d\n",scrol2);
	}

	if(keyboard_pressed(KEYCODE_Q))
	{
		scrol3++;
		printf("scrol2 = %d\n",scrol3);
	}

	if(keyboard_pressed(KEYCODE_W))
	{
		scrol3--;
		printf("scrol3 = %d\n",scrol3);
	}
*/
	tilemap_draw(bitmap,cliprect,bg_layer,0,0);
	tilemap_draw(bitmap,cliprect,fg_layer,0,0);

	draw_sprites(bitmap,cliprect);
}

static MACHINE_DRIVER_START( crospang )
	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 14318180/2) /* ??? */
	MDRV_CPU_MEMORY(crospang_readmem,crospang_writemem)
	MDRV_CPU_VBLANK_INT(irq6_line_hold,1)

	MDRV_CPU_ADD(Z80, 14318180/4 )
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(crospang_sound_readmem,crospang_sound_writemem)
	MDRV_CPU_PORTS(crospang_sound_readport,crospang_sound_writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_pulse,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 64*8)
	MDRV_VISIBLE_AREA(0, 40*8-1, 0*8, 30*8-1)
//	MDRV_VISIBLE_AREA(0,64*8-1, 0, 64*8-1)


	MDRV_PALETTE_LENGTH(0x800)
	MDRV_GFXDECODE(gfxdecodeinfo)

	MDRV_VIDEO_START(crospang)
	MDRV_VIDEO_UPDATE(crospang)

	/* sound hardware */
	MDRV_SOUND_ADD(OKIM6295, okim6295_interface)
MACHINE_DRIVER_END


ROM_START( crospang )
	ROM_REGION( 0x40000, REGION_CPU1, 0 ) /* 68k */
	ROM_LOAD16_BYTE( "p1.bin", 0x00001, 0x20000, CRC(0bcbbaad) SHA1(807f07be340d7af0aad8d49461b5a7f0221ea3b7) )
	ROM_LOAD16_BYTE( "p2.bin", 0x00000, 0x20000, CRC(0947d204) SHA1(35e7e277c51888a66d305994bf05c3f6bfc3c29e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80  */
	ROM_LOAD( "s1.bin", 0x00000, 0x10000, CRC(d61a224c) SHA1(5cd1b2d136ad58ab550c7ba135558d6c8a4cd8f6) )/* rom s1 */

	ROM_REGION( 0x20000, REGION_SOUND1, 0 ) /* samples? */
	ROM_LOAD( "s2.bin", 0x00000, 0x20000, CRC(9f9ecd22) SHA1(631ffe14018ba39658c435b8ecb23b19a14569ee) ) /* rom s2 */  // sample rom contains oksan?

	ROM_REGION( 0x200000, REGION_GFX1, 0 ) /* sprites */
	ROM_LOAD( "rom6.bin", 0x000000, 0x80000, CRC(9c633082) SHA1(18b8591b695ee429c9c9855d8cbba6249a4bd809) )
	ROM_LOAD( "rom5.bin", 0x080000, 0x80000, CRC(53a34dc5) SHA1(2e5cf8093bf507e81d7447736b7727c3fd20c471) )
	ROM_LOAD( "rom4.bin", 0x100000, 0x80000, CRC(9a91d494) SHA1(1c6280f662f1cf53f7f6defb7e215da75b573fdf) )
	ROM_LOAD( "rom3.bin", 0x180000, 0x80000, CRC(cc6e1fce) SHA1(eb5b3ca7222f48916dc6206f987b2669fe7e7c6b) )

	ROM_REGION( 0x80000, REGION_GFX2, 0 ) /* bg tiles */
	ROM_LOAD( "rom1.bin", 0x00000, 0x40000, CRC(905042bb) SHA1(ed5b97e88d24e55f8fcfaaa34251582976cb2527) )
	ROM_LOAD( "rom2.bin", 0x40000, 0x40000, CRC(bc4381e9) SHA1(af0690c253bead3448db5ec8fb258d8284646e89) )
ROM_END

GAMEX( 199?, crospang, 0, crospang, crospang, 0, ROT0, "Oksan?", "Cross Pang", GAME_IMPERFECT_GRAPHICS | GAME_NO_SOUND)
