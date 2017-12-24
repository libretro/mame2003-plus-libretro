/*
  "Simple" 156 based Data East Hardware
 Data East
  DE-0409-1
  DE-0491-1
 Mitchell
  MT5601-0 (slightly different component arrangement to Deco PCBs)
 Games Supported
  Data East:
    Joe & Mac Returns (Japanese version called Caveman Ninja 2 might exist)

  The DECO 156 is a 32-bit custom encrypted ARM5 chip connected to
  16-bit hardware. Only ROM and System Work RAM is accessed via all
  32 data lines.
  Info from Charles MacDonald:
  - The sound effects 6295 is clocked at exactly half the rate of the
  music 6295.
  - Both have the SS pin pulled high to select the sample rate of the
  ADPCM data. Depends on the input clock though, the rates are described
  in the data sheet.
  - Both are connected directly to their ROMs with no swapping on the
  address or data lines. The music ROM is a 16-bit ROM in byte mode.
  The 156 data bus has pull-up resistors so reading unused locations will
  return $FFFF.
  I traced out all the connections and confirmed that both video chips (52
  and 141) really are on the lower 16 bits of the 32-bit data bus, same
  with the palette RAM. Just the program ROM and 4K internal RAM to the
  223 should be accessed as 32-bit. Not sure why that part isn't working
  right though.
  Each game has a 512K block of memory that is decoded in the same way.
  One of the PALs controls where this block starts at, for example
  0x380000 for Magical Drop and 0x180000 for Osman:
    000000-00FFFF : Main RAM (16K)
    010000-01FFFF : Sprite RAM (8K)
    020000-02FFFF : Palette RAM (4K)
    030000-03FFFF : Read player inputs, write EEPROM and OKI banking
    040000-04FFFF : PF1,2 control registers
    050000-05FFFF : PF1,2 name tables
    060000-06FFFF : PF1,2 row scroll
    070000-07FFFF : Control register
  The ordering of items within the block does not change and the size of
  each region is always 64K. If any RAM or other I/O has to be mirrored,
  it likely fills out the entire 64K range.
  The control register (marked as MWA_NOP in the driver) pulls one of the
  DE156 pins high for a brief moment and low again. Perhaps it triggers an
  interrupt or reset? It doesn't seem to be connected to anything else, at
  least on my board.
  The sprite chip has 16K RAM but the highest address line is tied to
  ground, so only 8K is available. This is correctly implemented in the
  driver, I'm mentioning it to confirm it isn't banked or anything like that.
*/
#include "driver.h"
#include "decocrpt.h"
#include "cpu/arm/arm.h"
#include "machine/eeprom.h"
#include "deco16ic.h"
#include "vidhrdw/generic.h"

static UINT32 *simpl156_systemram;
static const UINT8 *simpl156_default_eeprom = NULL;

static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int offs;


	for (offs = (0x1400/4)-4;offs >= 0;offs -= 4) // 0x1400 for charlien
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult, pri;

		sprite = spriteram32[offs+1]&0xffff;

		y = spriteram32[offs]&0xffff;
		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		x = spriteram32[offs+2]&0xffff;
		colour = (x >>9) & 0x1f;

		pri = (x&0xc000); // 2 bits or 1?

		switch (pri&0xc000) {
		case 0x0000: pri=0; break;
		case 0x4000: pri=0xf0; break;
		case 0x8000: pri=0xf0|0xcc; break;
		case 0xc000: pri=0xf0|0xcc; break; /*  or 0xf0|0xcc|0xaa ? */
		}

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
		x = 304 - x;

		if (x>320) continue;

		sprite &= ~multi;
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (1) // flip screen iq_132
		{
			y=240-y;
			x=304-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else 
			mult=-16;

		while (multi >= 0)
		{
			pdrawgfx(bitmap,Machine->gfx[2],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					&Machine->visible_area,TRANSPARENCY_PEN,0,pri);

			multi--;
		}
	}
}


VIDEO_UPDATE( simpl156 )
{
	fillbitmap(priority_bitmap,0,cliprect);

	deco16_pf12_update(deco16_pf1_rowscroll,deco16_pf2_rowscroll);

	fillbitmap(bitmap,256,cliprect);

	deco16_tilemap_2_draw(bitmap,cliprect,0,2);
	deco16_tilemap_1_draw(bitmap,cliprect,0,4);

	draw_sprites(bitmap,cliprect);
}

static int simpl156_bank_callback(const int bank)
{
	return ((bank>>4)&0x7) * 0x1000;
}

VIDEO_START( simpl156 )
{
	deco16_pf1_data = (data16_t*)auto_malloc(0x2000);
	deco16_pf2_data = (data16_t*)auto_malloc(0x2000);
	deco16_pf1_rowscroll = (data16_t*)auto_malloc(0x800);
	deco16_pf2_rowscroll = (data16_t*)auto_malloc(0x800);
	deco16_pf12_control = (data16_t*)auto_malloc(0x10);

	paletteram16 =  (data16_t*)auto_malloc(0x1000);

	deco16_1_video_init();

	deco16_set_tilemap_bank_callback(0, simpl156_bank_callback);
	deco16_set_tilemap_bank_callback(1, simpl156_bank_callback);

	return 0;
}


INPUT_PORTS_START( simpl156 )
	PORT_START	/* 16bit */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE_NO_TOGGLE( 0x0008, IP_ACTIVE_LOW )
	PORT_BIT( 0x00f0, IP_ACTIVE_HIGH, IPT_VBLANK ) // all bits? check..
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_SPECIAL ) // eeprom?..

	PORT_START	/* 16bit */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )
INPUT_PORTS_END


static READ32_HANDLER( simpl156_inputs_read )
{
	// use a fake vblank switch otherwise the games will crap out...

	UINT32 returndata;
	static UINT32 vblank_fake;

	vblank_fake ^= 0xf0;

	returndata = readinputport(0) | 0xffff0000;
	returndata |= (EEPROM_read_bit() << 8);
	returndata &= ~0xf0;
	returndata |= vblank_fake;

	return returndata;
}

static READ32_HANDLER( simpl156_palette_r )
{
	return paletteram16[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_palette_w )
{
	UINT16 dat;
	int color;

	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	COMBINE_DATA(&paletteram16[offset]);
	color = offset;

	dat = paletteram16[offset]&0xffff;

#define pal5bit(n)	((((n) & 0x1f) << 3) | (((n) & 0x1f) >> 2))

	palette_set_color(color,pal5bit(dat >> 0),pal5bit(dat >> 5),pal5bit(dat >> 10));

#undef pal5bit
}


static READ32_HANDLER(  simpl156_system_r )
{
	UINT32 returndata;

	returndata = readinputport(1);

	return returndata;
}

static WRITE32_HANDLER( simpl156_eeprom_w )
{
	OKIM6295_set_bank_base(1, 0x40000 * (data & 0x7) );

	EEPROM_set_clock_line((data & 0x20) ? ASSERT_LINE : CLEAR_LINE);
	EEPROM_write_bit(data & 0x10);
	EEPROM_set_cs_line((data & 0x40) ? CLEAR_LINE : ASSERT_LINE);
}

/* we need to throw away bits for all ram accesses as the devices are connected as 16-bit */

static READ32_HANDLER( simpl156_spriteram_r )
{
	return spriteram32[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_spriteram_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	COMBINE_DATA(&spriteram32[offset]);
}

static UINT32*simpl156_mainram;


static READ32_HANDLER( simpl156_mainram_r )
{
	return simpl156_mainram[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_mainram_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	COMBINE_DATA(&simpl156_mainram[offset]);
}

static READ32_HANDLER( simpl156_pf1_rowscroll_r )
{
	return deco16_pf1_rowscroll[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_pf1_rowscroll_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	COMBINE_DATA(&deco16_pf1_rowscroll[offset]);
}

static READ32_HANDLER( simpl156_pf2_rowscroll_r )
{
	return deco16_pf2_rowscroll[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_pf2_rowscroll_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	COMBINE_DATA(&deco16_pf2_rowscroll[offset]);
}

static READ32_HANDLER ( simpl156_pf12_control_r )
{
	return deco16_pf12_control[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_pf12_control_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	COMBINE_DATA(&deco16_pf12_control[offset]);
}

static READ32_HANDLER( simpl156_pf1_data_r )
{
	return deco16_pf1_data[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_pf1_data_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;

	deco16_pf1_data_w(offset,data,mem_mask);
}

static READ32_HANDLER( simpl156_pf2_data_r )
{
	return deco16_pf2_data[offset]^0xffff0000;
}

static WRITE32_HANDLER( simpl156_pf2_data_w )
{
	data &=0x0000ffff;
	mem_mask &=0x0000ffff;
	deco16_pf2_data_w(offset,data,mem_mask);
}

READ32_HANDLER( simpl156_6295_0_r )
{
	return OKIM6295_status_0_r(0);
}

READ32_HANDLER( simpl156_6295_1_r )
{
	return OKIM6295_status_1_r(0);
}

// write?
WRITE32_HANDLER( simpl156_6295_0_w )
{
	if( ACCESSING_LSB32 ) {
		OKIM6295_data_0_w(0, data & 0xff);
	}
}

WRITE32_HANDLER( simpl156_6295_1_w )
{
	if( ACCESSING_LSB32 ) {
		OKIM6295_data_1_w(0, data & 0xff);
	}
}

static MEMORY_READ32_START( joemacr_readmem )
    { 0x000000, 0x07ffff, MRA32_ROM },
	{ 0x100000, 0x107fff, simpl156_mainram_r }, // main ram
	{ 0x110000, 0x111fff, simpl156_spriteram_r },
	{ 0x120000, 0x120fff, simpl156_palette_r },
	{ 0x130000, 0x130003, simpl156_system_r }, // eeprom
	{ 0x140000, 0x14001f, simpl156_pf12_control_r },
	{ 0x150000, 0x151fff, simpl156_pf1_data_r },
	{ 0x152000, 0x153fff, simpl156_pf1_data_r },
	{ 0x154000, 0x155fff, simpl156_pf2_data_r },
	{ 0x160000, 0x161fff, simpl156_pf1_rowscroll_r },
	{ 0x164000, 0x165fff, simpl156_pf2_rowscroll_r },
	{ 0x180000, 0x180003, simpl156_6295_0_r },
	{ 0x1c0000, 0x1c0003, simpl156_6295_1_r },
	{ 0x200000, 0x200003, simpl156_inputs_read },
	{ 0x201000, 0x201fff, MRA32_RAM },  // work ram (32-bit)
MEMORY_END

static MEMORY_WRITE32_START( joemacr_writemem )
    { 0x000000, 0x07ffff, MWA32_ROM },
	{ 0x100000, 0x107fff, simpl156_mainram_w, &simpl156_mainram },// main ram
	{ 0x110000, 0x111fff, simpl156_spriteram_w, &spriteram32, &spriteram_size },
	{ 0x120000, 0x120fff, simpl156_palette_w },
	{ 0x130000, 0x130003, simpl156_eeprom_w },
	{ 0x140000, 0x14001f, simpl156_pf12_control_w },
	{ 0x150000, 0x151fff, simpl156_pf1_data_w },
	{ 0x152000, 0x153fff, simpl156_pf1_data_w },
	{ 0x154000, 0x155fff, simpl156_pf2_data_w },
	{ 0x160000, 0x161fff, simpl156_pf1_rowscroll_w },
	{ 0x164000, 0x164fff, simpl156_pf2_rowscroll_w },
	{ 0x170000, 0x170003, MWA32_NOP }, // ?
	{ 0x180000, 0x180003, simpl156_6295_0_w },
	{ 0x1c0000, 0x1c0003, simpl156_6295_1_w },
	{ 0x201000, 0x201fff, MWA32_RAM, &simpl156_systemram }, // work ram (32-bit)
MEMORY_END



static struct GfxLayout tile_8x8_layout =
{
	8,8,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+8,RGN_FRAC(1,2)+0,RGN_FRAC(0,2)+8,RGN_FRAC(0,2)+0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16
};

static struct GfxLayout tile_16x16_layout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+8,RGN_FRAC(1,2)+0,RGN_FRAC(0,2)+8,RGN_FRAC(0,2)+0 },
	{ 256,257,258,259,260,261,262,263,0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,8*16,9*16,10*16,11*16,12*16,13*16,14*16,15*16 },
	32*16
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 24,8,16,0 },
	{ 512,513,514,515,516,517,518,519, 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
	  8*32, 9*32,10*32,11*32,12*32,13*32,14*32,15*32},
	32*32
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tile_8x8_layout,     0, 32 },
	{ REGION_GFX1, 0, &tile_16x16_layout,     0, 32 },
	{ REGION_GFX2, 0, &spritelayout,     0x200, 32 },
	{ -1 }	/* end of array */
};

static NVRAM_HANDLER( simpl156 )
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface_93C46);// 93c45

		if (file) EEPROM_load(file);
		else
		{
			if (simpl156_default_eeprom)	{ /* Set the EEPROM to Factory Defaults */

				data8_t eeprom_data[0x100]; // lazy
				memcpy (eeprom_data, simpl156_default_eeprom, 0x80);

				EEPROM_set_data(eeprom_data,0x100);
			}
		}
	}
}

static INTERRUPT_GEN( simpl156_vbl_interrupt )
{
	cpu_set_irq_line(0, ARM_IRQ_LINE, HOLD_LINE);
}

static struct OKIM6295interface adpcm_6295_interface =
{
	2,          	/* 2 chips */
	{ 1006875/132, 2013750/132 },      /* 10847 Hz frequency (1.431815MHz / 132) */
	{ REGION_SOUND1, REGION_SOUND2 },  /* memory */
	{ 100, 60 }
};


static MACHINE_DRIVER_START( joemacr )
	/* basic machine hardware */

	MDRV_CPU_ADD(ARM, 28000000 /* /4 */)	/*DE156*/ /* 7.000 MHz */ /* measured at 7.. seems to need 28? */
	MDRV_CPU_MEMORY(joemacr_readmem,joemacr_writemem)
	MDRV_CPU_VBLANK_INT(simpl156_vbl_interrupt,1)

	MDRV_NVRAM_HANDLER(simpl156) // 93C45

	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(TIME_IN_USEC(800))

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 1*8, 31*8-1)

	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(4096)

	MDRV_VIDEO_START(simpl156)
	MDRV_VIDEO_UPDATE(simpl156)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(OKIM6295, adpcm_6295_interface)
MACHINE_DRIVER_END


static void simpl156_common_init(void)
{
	UINT8 *rom = memory_region(REGION_SOUND2);
	int length = memory_region_length(REGION_SOUND2);
	UINT8 *buf1 = (UINT8*)malloc(length);

	UINT32 x;

	/* hmm low address line goes to banking chip instead? */
	for (x=0;x<length;x++)
	{
		UINT32 addr;

		addr = BITSWAP24 (x,23,22,21,0, 20,
		                    19,18,17,16,
		                    15,14,13,12,
		                    11,10,9, 8,
		                    7, 6, 5, 4,
		                    3, 2, 1 );

		buf1[addr] = rom[x];
	}

	memcpy(rom,buf1,length);

	free (buf1);

	deco56_decrypt(REGION_GFX1);

	deco156_decrypt();
}

/*

Joe and Mac Returns
Data East 1994

This is a higher quality bootleg made with genuine DECO chips/parts.

+-------------------------------+
|        07.u46           04.u12|
| VOL  M6296 M6295    52  03.u11|
|J  06.u45      62256           |
|A              62256     02.u8h|
|M    PAL  141            01.u8l|
|M     62256                    |
|A 223 62256  PAL  28MHz        |
|             62256         156 |
|  SW1        62256   223       |
|      93C45  05.u29            |
+-------------------------------+

All roms are socketted eproms, no labels, just a number in pencel.

05.u29  27c4096
01.u8l  27c4096
02.u8h  27c4096
03.u11  27c4096
04.u12  27c4096
06.u45  27c160
07.u46  27c020

*/

ROM_START( joemacr )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* DE156 code (encrypted) */
	ROM_LOAD( "05.u29",    0x000000, 0x080000,  CRC(74e9a158) SHA1(eee447303ac0884e152b89f59a9694afade87336) )

	ROM_REGION( 0x100000, REGION_GFX1, 0 )
	ROM_LOAD( "01.u8l",    0x000000, 0x080000,  CRC(4da4a2c1) SHA1(1ed4bd4337d8b185b56e326e662a8715e4d09e17) )
	ROM_LOAD( "02.u8h",    0x080000, 0x080000,  CRC(642c08db) SHA1(9a541fd56ae34c24f803e08869702be6fafd81d1) )

	ROM_REGION( 0x100000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mbn01",    0x000001, 0x080000, CRC(a3a37353) SHA1(c4509c8268afb647c20e71b42ae8ebd2bdf075e6) ) /* 03.u11 */
	ROM_LOAD16_BYTE( "mbn02",    0x000000, 0x080000, CRC(aa2230c5) SHA1(43b7ac5c69cde1840a5255a8897e1c5d5f89fd7b) ) /* 04.u12 */

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* Oki samples */
	ROM_LOAD( "mbn04",    0x00000, 0x40000,  CRC(dcbd4771) SHA1(2a1ab6b0fc372333c7eb17aab077fe1ca5ba1dea) ) /* 07.u46 */

	ROM_REGION( 0x200000, REGION_SOUND2, 0 ) /* samples? (banked?) */
	ROM_LOAD( "mbn03",    0x00000, 0x200000, CRC(70b71a2a) SHA1(45851b0692de73016fc9b913316001af4690534c) ) /* 06.u45 */
ROM_END

/*

Joe and Mac Returns
Data East 1994

DE-0491-1

156         MW00
      223              223
             141

  MBN00

  MBN01   52   MBN03  M6295
  MBN02        MBN04  M6295

*/

ROM_START( joemacra )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* DE156 code (encrypted) */
	ROM_LOAD( "mw00",    0x000000, 0x080000,  CRC(e1b78f40) SHA1(e611c317ada5a049a5e05d69c051e22a43fa2845) )

	ROM_REGION( 0x100000, REGION_GFX1, 0 ) // rebuilt with roms from other set
	ROM_LOAD( "mbn00",    0x000000, 0x100000, CRC(11b2dac7) SHA1(71a50f606caddeb0ef266e2d3df9e429a4873f21) )

	ROM_REGION( 0x100000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "mbn01",    0x000001, 0x080000, CRC(a3a37353) SHA1(c4509c8268afb647c20e71b42ae8ebd2bdf075e6) )
	ROM_LOAD16_BYTE( "mbn02",    0x000000, 0x080000, CRC(aa2230c5) SHA1(43b7ac5c69cde1840a5255a8897e1c5d5f89fd7b) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* Oki samples */
	ROM_LOAD( "mbn04",    0x00000, 0x40000,  CRC(dcbd4771) SHA1(2a1ab6b0fc372333c7eb17aab077fe1ca5ba1dea) )

	ROM_REGION( 0x200000, REGION_SOUND2, 0 ) /* samples? (banked?) */
	ROM_LOAD( "mbn03",    0x00000, 0x200000, CRC(70b71a2a) SHA1(45851b0692de73016fc9b913316001af4690534c) )
ROM_END


/* Everything seems more stable if we run the CPU speed x4 and use Idle skips.. maybe it has an internal multipler? */
static READ32_HANDLER( joemacr_speedup_r )
{
	if (activecpu_get_pc()==0x284)  cpu_spinuntil_time(TIME_IN_USEC(400));
	return simpl156_systemram[0x18/4];
}

static DRIVER_INIT (joemacr)
{
	install_mem_read32_handler(0, 0x0201018, 0x020101b, joemacr_speedup_r );
	simpl156_common_init();
}


/* Data East games running on the DE-0409-1 or DE-0491-1 PCB */
GAME( 1994, joemacr,  0,        joemacr,     simpl156, joemacr,  ROT0, "Data East Corporation", "Joe & Mac Returns" ) /* bootleg board with genuine DECO parts */
GAME( 1994, joemacra, joemacr,  joemacr,     simpl156, joemacr,  ROT0, "Data East Corporation", "Joe & Mac Returns" )
