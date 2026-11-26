/*
    Metal Freezer (c) 1989 Seibu

    driver by Angelo Salese, based off initial work by David Haywood
    thanks to Peter Wilhelmsen for the decryption

    HW seems the natural evolution of Dark Mist type.

    TODO:
    - A few video register bits still needs sorting out
    - sprites are ahead of 1/2 frames, needs sprite DMA fixed;
    - Flip screen support;
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/t5182.h"

static UINT8 *vram;
static UINT8 *vregs;
static UINT8 *workram;
static int fg_tilebank;
static int rowscroll_enable;


static WRITE_HANDLER( output_w )
{
	/* bit 6-5: coin lockouts, bit 4: tilemap ROM banking, bit 3-2: z80 ROM banking, bit 1: rowscroll enable */
	coin_lockout_w(1, (data >> 6) & 1);
	coin_lockout_w(0, (data >> 5) & 1);
	fg_tilebank = (data & 0x10) >> 4;
	cpu_setbank(1, memory_region(REGION_CPU1) + 0x10000 + ((data & 0xc) >> 2) * 0x4000);
	rowscroll_enable = (data >> 1) & 1;
}

static MEMORY_READ_START( main_readmem )
  { 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xcfff, MRA_RAM },
	{ 0xd000, 0xd1ff, MRA_RAM },
	{ 0xd200, 0xd3ff, MRA_RAM },
	{ 0xd400, 0xd47f, t5182shared_r },
	{ 0xd600, 0xd600, input_port_0_r },
	{ 0xd601, 0xd601, input_port_1_r },
	{ 0xd602, 0xd602, input_port_2_r },
	{ 0xd603, 0xd603, input_port_3_r },
	{ 0xd604, 0xd604, input_port_4_r },
  { 0xd711, 0xd711, t5182_sharedram_semaphore_snd_r },
	{ 0xd800, 0xdfff, MRA_RAM },
	{ 0xe000, 0xefff, MRA_RAM },
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( main_writemem )
  { 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM, &vram },
	{ 0xd000, 0xd1ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0xd200, 0xd3ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0xd400, 0xd47f, t5182shared_w },
	{ 0xd600, 0xd61f, MWA_RAM, &vregs },
	{ 0xd700, 0xd700, output_w },
	{ 0xd710, 0xd710, t5182_sound_irq_w },
	{ 0xd712, 0xd712, t5182_sharedram_semaphore_main_release_w },
	{ 0xd713, 0xd713, t5182_sharedram_semaphore_main_acquire_w },
	{ 0xd800, 0xdfff, MWA_RAM },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf000, 0xffff, MWA_RAM, &workram },
MEMORY_END


INPUT_PORTS_START( metlfrzr )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "2-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, "2-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x04, 0x04, "2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x08, 0x08, "2-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_DIPNAME( 0x40, 0x40, "2-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "2-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "A" )
	PORT_DIPSETTING(    0x03, "B" )
	PORT_DIPSETTING(    0x01, "C" )
	PORT_DIPSETTING(    0x00, "D" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "20k, 50k, 100k" )
	PORT_DIPSETTING(    0x0c, "30k, 80k, 150k" )
	PORT_DIPSETTING(    0x04, "50k, 100k, 200k" )
	PORT_DIPSETTING(    0x00, "100k, 200k, 400k" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x40, 0x40, "Level_Select" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static struct GfxLayout tile_layout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 4, 8, 12 },
	{ 19, 18, 17, 16, 3, 2, 1, 0 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout sprite_layout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 4, 8, 12 },
	{ 0, 1, 2, 3, 16, 17, 18, 19, 64*8+0, 64*8+1, 64*8+2, 64*8+3, 64*8+16, 64*8+17, 64*8+18, 64*8+19 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	128*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tile_layout,   0x100, 16 },
	{ REGION_GFX2, 0, &tile_layout,   0x100, 16 },
	{ REGION_GFX3, 0, &sprite_layout, 0x000, 16 },
	{ REGION_GFX4, 0, &sprite_layout, 0x000, 16 },
	{ -1 }
};

static VIDEO_START( metlfrzr )
{
	fg_tilebank = 0;
	rowscroll_enable = 0;
	return 0;
}

static VIDEO_UPDATE( metlfrzr )
{
	int count;

	fillbitmap(bitmap, Machine->pens[0], cliprect);

	/* background */
	{
		const struct GfxElement *gfx = Machine->gfx[fg_tilebank];
		int x_scroll_value = vregs[0x17] + ((vregs[0x06] & 1) << 8);
		int x_scroll_base = (x_scroll_value >> 3) * 32;

		for (count = 0; count < 32 * 33; ++count)
		{
			int tile_base = count;
			int y = count & 0x1f;
			int x = count >> 5;
			int x_scroll_shift;
			int tile, color;

			if (y > 7 || !rowscroll_enable)
			{
				tile_base += x_scroll_base;
				x_scroll_shift = x_scroll_value & 7;
			}
			else
				x_scroll_shift = 0;
			tile_base &= 0x7ff;

			tile = vram[tile_base * 2 + 0] + ((vram[tile_base * 2 + 1] & 0xf0) << 4);
			color = vram[tile_base * 2 + 1] & 0xf;

			drawgfx(bitmap, gfx, tile, color, 0, 0, x * 8 - x_scroll_shift, y * 8, cliprect, TRANSPARENCY_PEN, 0xf);
		}
	}

	/* sprites */
	{
		const UINT8 *spriteram = workram + 0xe00;

		for (count = 0x200 - 4; count >= 0; count -= 4)
		{
			const struct GfxElement *gfx = Machine->gfx[(spriteram[count + 1] & 0x40) ? 3 : 2];
			int tile = spriteram[count] | (((spriteram[count + 1] & 0x30) >> 4) << 8);
			int color = spriteram[count + 1] & 0xf;
			int y = spriteram[count + 2];
			int x = spriteram[count + 3];

			if (spriteram[count + 1] & 0x80)
				x -= 256;

			drawgfx(bitmap, gfx, tile, color, 0, 0, x, y, cliprect, TRANSPARENCY_PEN, 0xf);
		}
	}
}

static INTERRUPT_GEN( metlfrzr_interrupt )
{
	if (cpu_getiloops())
		cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, 0xd7); /* RST 10h, vblank-out */
	else
		cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, 0xcf); /* RST 08h */
}

static MACHINE_DRIVER_START( metlfrzr )
	MDRV_CPU_ADD(Z80, 12000000 / 2)
	MDRV_CPU_MEMORY(main_readmem, main_writemem)
	MDRV_CPU_VBLANK_INT(metlfrzr_interrupt, 2)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 16, 256-16-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(0x200)

	MDRV_VIDEO_START(metlfrzr)
	MDRV_VIDEO_UPDATE(metlfrzr)

	/* sound hardware */
	MDRV_IMPORT_FROM(t5182_audio)
MACHINE_DRIVER_END


ROM_START( metlfrzr )
	ROM_REGION( 0x20000 * 2, REGION_CPU1, 0 )
	ROM_LOAD( "1.15j", 0x00000, 0x08000, CRC(f59b5fa2) SHA1(6033967dad5e64f45afbcb1b45c8eb79e0787afb) )
	ROM_LOAD( "2.14j", 0x10000, 0x10000, CRC(21ecc248) SHA1(2fccf7db73890faf7c489bfc43c88ded54d5052d) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* Toshiba T5182 module */
	ROM_LOAD( "t5182.rom", 0x0000, 0x2000, CRC(d354c8fc) SHA1(a1c9e1ac293f107f69cc5788cf6abc3db1646e33) )
	ROM_LOAD( "3.4h",      0x8000, 0x8000, CRC(36f88e54) SHA1(5cbea56c7e547c353ae2f9256caaceb20e5e8503) )

	ROM_REGION( 0x20000, REGION_GFX1, 0 )
	ROM_LOAD16_BYTE( "10.5a", 0x00001, 0x10000, CRC(3313e74a) SHA1(8622dfb5c013173d5bb037254f4c23b1282404e1) )
	ROM_LOAD16_BYTE( "12.7a", 0x00000, 0x10000, CRC(6da5fda9) SHA1(9d7b0b26598f31da589fece3535a4d1405b03fc2) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 )
	ROM_LOAD16_BYTE( "11.6a", 0x00001, 0x10000, CRC(fa6490b8) SHA1(9a4c1e09b9e8fb256fec0a5ed120fece8a12e1c8) )
	ROM_LOAD16_BYTE( "13.9a", 0x00000, 0x10000, CRC(a4f689ec) SHA1(e58bfede3fabf4cfca76c20aafb3e9fb604777c9) )

	ROM_REGION( 0x20000, REGION_GFX3, 0 )
	ROM_LOAD16_BYTE( "14.13a", 0x00001, 0x10000, CRC(a9cd5225) SHA1(f3d5e29ee08fb563fdc1af3c64128f2cd2feb987) )
	ROM_LOAD16_BYTE( "16.11a", 0x00000, 0x10000, CRC(92f2cb49) SHA1(498021d94b0fde216207076491702af2324a2dcc) )

	ROM_REGION( 0x20000, REGION_GFX4, 0 )
	ROM_LOAD16_BYTE( "15.12a", 0x00001, 0x10000, CRC(ce5c4c8b) SHA1(2351d66ba51e80097ce53bfd448ac24901844cda) )
	ROM_LOAD16_BYTE( "17.10a", 0x00000, 0x10000, CRC(3fec33f7) SHA1(af086ba30fc4521a0114da2824f5baa04d225a89) )

	ROM_REGION( 0x200, REGION_PROMS, 0 )
	ROM_LOAD( "n8s129a.7f",  0x000, 0x100, CRC(c849d60b) SHA1(0022fb71b3d777cadac7005e6156725df9bcaf90) )
	ROM_LOAD( "n82s135n.9c", 0x100, 0x100, CRC(7bbd52db) SHA1(b9bab5fb515579d0270aea8b992a16eeb878f242) )

	ROM_REGION( 0x300, REGION_USER1, 0 )
	ROM_LOAD( "pld3.14h.bin", 0x000, 0x149, CRC(8183f7f0) SHA1(3cec53838120064374ecf4ebee048409c6f34081) )
	ROM_LOAD( "pld8.4d.bin",  0x149, 0x149, CRC(f1e35034) SHA1(527faddbf2ac905fa59ebda8ea327e6e6a7c1fb6) )
ROM_END

static DRIVER_INIT( metlfrzr )
{
  int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;

	memory_set_opcode_base(0,rom+diff);
	cpu_setbank(1, rom + 0x10000);

  /* the banked ROM is not encrypted: same bytes for opcodes and data */
	memcpy(rom + diff + 0x8000, rom + 0x8000, diff - 0x8000);
	
	for (A = 0x0000; A < 0x8000; A++)
	{
		/* decode the opcodes */
      rom[A+diff] = rom[A];

		if (BIT(A,5) && !BIT(A,3))
			rom[A+diff] ^= 0x40;

		if (BIT(A,10) && !BIT(A,9) && BIT(A,3))
			rom[A+diff] ^= 0x20;

		if ((BIT(A,10) ^ BIT(A,9)) && BIT(A,1))
			rom[A+diff] ^= 0x02;

		if (BIT(A,9) || !BIT(A,5) || BIT(A,3))
			rom[A+diff] = BITSWAP8(rom[A+diff],7,6,1,4,3,2,5,0);

		/* decode the data */
		if (BIT(A,5))
			rom[A] ^= 0x40;

		if (BIT(A,9) || !BIT(A,5))
			rom[A] = BITSWAP8(rom[A],7,6,1,4,3,2,5,0);
	}
}

GAME( 1989, metlfrzr, 0, metlfrzr, metlfrzr, metlfrzr, ROT270, "Seibu Kaihatsu", "Metal Freezer (Japan)" )
