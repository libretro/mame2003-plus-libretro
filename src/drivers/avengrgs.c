/***************************************************************************

	Avengers In Galactic Storm		(c) 1996 Data East Corporation


	This is a 'MLC' hardware game, however with this system the main cpu
	is on the game board, not motherboard, so it's not quite the same
	as the other MLC games which use an encrypted ARM processor rather
	than a SH2.

	Todo:
		Sprite scaling.
		Sprite X flip seems wrong/inconsistent (cpu bug?)
		Backgrounds (tilemaps stored in ROM).
		Fix 'corrupt' sprites - these 'sprites' may actually
		be pointers to the backgrounds in ROM, and the backgrounds
		just treated as large sprites.  There is a special bit
		set in all the 'corrupt' entries.
		Text tilemap is strange and implementation probably wrong.
		Sound seems bad.

    Driver by Bryan McPhail, thank you to Avedis and The Guru.



	WIP Notes (delete later):

	0000333e:  Test_r 0
		Tests 0x00000040
		Tests 0x01000000
		Tests 0x00000080
		Tests 0x00004000


	000123c8:  Test2_r 1
	000123c8:  Test2_r 1


	000034aa:  Test2_r 0
			Test VBL in bit 0x10 (200070)

	000034c4:  Test2_r 1 (200004)
			Read
			OR 0xc0
			Write back

	000034d8:  Test2_r 3
		Moved into 108c44

	0000350e:  Test2_r 1
	000035ae:  Test2_r 0
		bit 0 tested and bsr taken if set

	200074 read at start of irq
	masked to 0xff

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"

VIDEO_START( avengrgs );
VIDEO_UPDATE( avengrgs );
VIDEO_STOP(avengrgs);

extern data32_t *avengrgs_vram,*avengrgs_ram1, *avengrgs_ram2;
static data32_t *avengrgs_ram;

/***************************************************************************/

static READ32_HANDLER( avengrs_control_r )
{
	return (readinputport(1)<<16) | (EEPROM_read_bit()<<23) | readinputport(0);
}

static READ32_HANDLER(test2_r)
{
/*	log_cb(RETRO_LOG_ERROR, LOGPRE "%08x:  Test2_r %d\n",activecpu_get_pc(),offset);*/
	return 0xffffffff;
}
static READ32_HANDLER(test3_r)
{
/*	log_cb(RETRO_LOG_ERROR, LOGPRE "%08x:  Test3_r %d\n",activecpu_get_pc(),offset);*/
	return 0xffffffff;
}

static WRITE32_HANDLER( avengrs_eprom_w )
{
	if (mem_mask==0xffff00ff) {
		data8_t ebyte=(data>>8)&0xff;
		if (ebyte&0x80) {
			EEPROM_set_clock_line((ebyte & 0x2) ? ASSERT_LINE : CLEAR_LINE);
			EEPROM_write_bit(ebyte & 0x1);
			EEPROM_set_cs_line((ebyte & 0x4) ? CLEAR_LINE : ASSERT_LINE);
		}
	}
	else if (mem_mask==0xffffff00) {
		/*volume control todo*/
	}
	else
		log_cb(RETRO_LOG_ERROR, LOGPRE "%08x:  eprom_w %08x mask %08x\n",activecpu_get_pc(),data,mem_mask);
}

static WRITE32_HANDLER( avengrs_palette_w )
{
	int r,g,b;

	COMBINE_DATA(&paletteram32[offset]);

	/* x bbbbb ggggg rrrrr - Todo verify what x is, if anything!  Used in places */
	b = (paletteram32[offset] >> 10) & 0x1f;
	g = (paletteram32[offset] >> 5) & 0x1f;
	r = (paletteram32[offset] >> 0) & 0x1f;

	palette_set_color(offset,r*8,g*8,b*8);
}

static READ32_HANDLER( avengrs_sound_r )
{
	if (mem_mask==0x00ffffff) {
		return YMZ280B_status_0_r(0)<<24;
	} else {
		log_cb(RETRO_LOG_ERROR, LOGPRE "%08x:  non-byte read from sound mask %08x\n",activecpu_get_pc(),mem_mask);
	}

	return 0;
}

static WRITE32_HANDLER( avengrs_sound_w )
{
	if (mem_mask==0x00ffffff) {
		if (offset)
			YMZ280B_data_0_w(0,data>>24);
		else
			YMZ280B_register_0_w(0,data>>24);
	} else {
		log_cb(RETRO_LOG_ERROR, LOGPRE "%08x:  non-byte written to sound %08x mask %08x\n",activecpu_get_pc(),data,mem_mask);
	}
}

/******************************************************************************/

static MEMORY_READ32_START( readmem )
	{ 0x0000000, 0x00fffff, MRA32_ROM },
	{ 0x0100000, 0x011ffff, MRA32_RAM },

/*	{ 0x0200000, 0x020000f, test2_r },*/
	{ 0x0200070, 0x020007f, test2_r }, /*vbl in 70 $10*/
	{ 0x0204000, 0x0206fff, MRA32_RAM },
/*	{ 0x0200000, 0x02000ff, MRA32_RAM },*/
	{ 0x0300000, 0x0307fff, MRA32_RAM },

	{ 0x0400000, 0x0400003, avengrs_control_r },
	{ 0x0440000, 0x044001f, test3_r },

	{ 0x2280000, 0x229ffff, MRA32_RAM },
	{ 0x22a0000, 0x22a7fff, MRA32_RAM },
	{ 0x2600004, 0x2600007, avengrs_sound_r },
MEMORY_END

static MEMORY_WRITE32_START( writemem )
	{ 0x0000000, 0x00fffff, MWA32_ROM },
	{ 0x0100000, 0x011ffff, MWA32_RAM, &avengrgs_ram },

	{ 0x0200080, 0x02000ff, MWA32_RAM },
	{ 0x0204000, 0x0206fff, MWA32_RAM, &spriteram32, &spriteram_size },
	{ 0x0300000, 0x0307fff, MWA32_RAM }, /*palette scratch pad?*/
	{ 0x044001c, 0x044001f, MWA32_NOP },
	{ 0x0500000, 0x0500003, avengrs_eprom_w },

	{ 0x2280000, 0x229ffff, MWA32_RAM, &avengrgs_ram1 }, /*index buffer*/
	{ 0x22a0000, 0x22a3fff, avengrs_palette_w, &paletteram32 },
	{ 0x22a4000, 0x22a7fff, MWA32_RAM, &avengrgs_ram2 },
	{ 0x2600000, 0x2600007, avengrs_sound_w },
	{ 0x4200008, 0x420000b, MWA32_NOP },
MEMORY_END

/******************************************************************************/

INPUT_PORTS_START( avengrgs )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* Eprom */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+16, RGN_FRAC(1,2)+0, 16, 0 },
	{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	16*32
};

static struct GfxLayout charlayout =
{
	8,8,
	255, /*todo*/
	4,
	{ RGN_FRAC(1,2)+16, RGN_FRAC(1,2)+0, 16, 0 },
	{ 15,13,11,9,7,5,3,1 },
	{ 0*32, 2*32, 4*32, 6*32, 8*32, 10*32, 12*32, 14*32 },
	16*32
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,   0, 256 },
	{ REGION_GFX2, 0, &spritelayout,   0, 256 },
	{ REGION_GFX3, 0, &spritelayout,   0, 256 },
	{ REGION_GFX1, 0, &charlayout,   0, 256 },
	{ -1 } /* end of array */
};

/******************************************************************************/

static void sound_irq_gen(int state)
{
	log_cb(RETRO_LOG_ERROR, LOGPRE "sound irq\n");
}

static struct YMZ280Binterface ymz280b_intf =
{
	1,
	{ 42000000 / 2 },
	{ REGION_SOUND1 },
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ sound_irq_gen }
};

static INTERRUPT_GEN(avengrgs_interrupt)
{
	cpu_set_irq_line(0, 1, HOLD_LINE);
}

static MACHINE_DRIVER_START( avengrgs )

	/* basic machine hardware */
	MDRV_CPU_ADD(SH2,42000000/2) /* 42 MHz clock */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(avengrgs_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_NVRAM_HANDLER(93C46) /* Actually 93c45 */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM | VIDEO_NEEDS_6BITS_PER_GUN)
	MDRV_SCREEN_SIZE(40*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(4096)

	MDRV_VIDEO_START(avengrgs)
	MDRV_VIDEO_UPDATE(avengrgs)
	MDRV_VIDEO_STOP(avengrgs)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YMZ280B, ymz280b_intf)
MACHINE_DRIVER_END

/***************************************************************************/

ROM_START( avengrgs )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )
	ROM_LOAD32_WORD_SWAP( "sd_00-2.7k", 0x000002, 0x80000, CRC(136be46a) SHA1(7679f5f78f7983d43ecdb9bdd04e45792a13d9f2) )
	ROM_LOAD32_WORD_SWAP( "sd_01-2.7l", 0x000000, 0x80000, CRC(9d87f576) SHA1(dd20cd060d020d81f4e012be10d0211be7526641) )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "mcg-00.1j", 0x000001, 0x200000, CRC(99129d9a) SHA1(1d1574e2326dca1043e05c229b54497df6ed5a35) )
	ROM_LOAD16_BYTE( "mcg-02.1f", 0x000000, 0x200000, CRC(29af9866) SHA1(56531911f8724975a7f81e61b7dec7fa72d50747) )
	ROM_LOAD16_BYTE( "mcg-04.3j", 0x400001, 0x200000, CRC(a4954c0e) SHA1(897a7313505f562879578941931a39afd34c9eef) )
	ROM_LOAD16_BYTE( "mcg-06.3f", 0x400000, 0x200000, CRC(01571cf6) SHA1(74f85d523f2783374f041aa95abe6d1b8c872127) )

	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "mcg-01.1d", 0x000001, 0x200000, CRC(3638861b) SHA1(0896110acdb4442e4819f73285b9e725fc787b7a) )
	ROM_LOAD16_BYTE( "mcg-03.7m", 0x000000, 0x200000, CRC(4a0c965f) SHA1(b658ae5e6e2ff6f42b605bb6c49ad8a67507f2ab) )
	ROM_LOAD16_BYTE( "mcg-05.3d", 0x400001, 0x200000, CRC(182c2b49) SHA1(e53c06e95508e6c7e746f81668a4f7c08bfc6d36) )
	ROM_LOAD16_BYTE( "mcg-07.8m", 0x400000, 0x200000, CRC(d09a3635) SHA1(8e184f3a3046bd8401762bbb480f5832fde91dde) )

	ROM_REGION( 0x800000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "mcg-08.7p", 0x000001, 0x200000, CRC(c253943e) SHA1(b97a1d565ffbf2190ba0b25de5ef0bb3b9c9248b) )
	ROM_LOAD16_BYTE( "mcg-09.7n", 0x000000, 0x200000, CRC(8fb9870b) SHA1(046b6d07610cf09f008d1595605139071671d95c) )
	ROM_LOAD16_BYTE( "mcg-10.8p", 0x400001, 0x200000, CRC(1383f524) SHA1(eadd8b579cc21ae119b7439c7882e39f22ac3b8c) )
	ROM_LOAD16_BYTE( "mcg-11.8n", 0x400000, 0x200000, CRC(8f7fc281) SHA1(8cac51036088dbf4ff3c2b91ef88ef30a30b0be1) )

	ROM_REGION( 0x80000, REGION_GFX4, 0 )
	ROM_LOAD( "sd_02-0.6j", 0x000000, 0x80000, CRC(24fc2b3c) SHA1(805eaa8e8ba49320ba83bda6307cc1d15d619358) )

	ROM_REGION( 0x600000, REGION_SOUND1, 0 )
	ROM_LOAD16_WORD_SWAP( "mcg-12.5a",  0x400000, 0x200000, CRC(bef9b28f) SHA1(b7a2a0539ea4d22b48ce3f3eb367017f219da2c1) )
	ROM_LOAD16_WORD_SWAP( "mcg-13.9k",  0x200000, 0x200000, CRC(92301551) SHA1(a7891e7a3c8d7f165ca73f5d5a034501df46e9a2) )
	ROM_LOAD16_WORD_SWAP( "mcg-14.6a",  0x000000, 0x200000, CRC(c0d8b5f0) SHA1(08eecf6e7d0273e41cda3472709a67e2b16068c9) )
ROM_END

/***************************************************************************/

static READ32_HANDLER( avengrgs_speedup_r )
{
	data32_t a=avengrgs_ram[0x89a0/4];
/*	log_cb(RETRO_LOG_ERROR, LOGPRE "Read %08x\n",activecpu_get_pc());*/
	if (activecpu_get_pc()==0x3236 && (a&1)) cpu_spinuntil_int();

	return a;
}

static DRIVER_INIT( avengrgs )
{
	install_mem_read32_handler(0, 0x01089a0, 0x01089a3, avengrgs_speedup_r );
}

/***************************************************************************/

GAMEX( 1995, avengrgs, 0, avengrgs, avengrgs, avengrgs, ROT0, "Data East Corporation", "Avengers In Galactic Storm (Japan)", GAME_NOT_WORKING )
