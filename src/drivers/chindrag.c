/* China Dragon / Dragon World */

#include "driver.h"

VIDEO_START(chindrag)
{
	return 0;
}

VIDEO_UPDATE(chindrag)
{

}
static MEMORY_READ16_START( chindrag_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x100000, 0x103fff, MRA16_RAM },
	{ 0x200000, 0x2001ff, MRA16_RAM },
	{ 0x200600, 0x200fff, MRA16_RAM },
	{ 0x400000, 0x401fff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( chindrag_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x100000, 0x103fff, MWA16_RAM },
	{ 0x200000, 0x2001ff, MWA16_RAM },
	{ 0x200600, 0x200fff, MWA16_RAM },
	{ 0x400000, 0x401fff, MWA16_RAM },

	/* actually more addresses in this range .. quite strange, maybe just ram accessed in an odd way during the test */
	{ 0xA00000, 0xA00001, MWA16_NOP },
	{ 0xA08000, 0xA08001, MWA16_NOP },
	{ 0xA10000, 0xA10001, MWA16_NOP },
	{ 0xA18000, 0xA18001, MWA16_NOP },
	{ 0xA20000, 0xA20001, MWA16_NOP },
	{ 0xA28000, 0xA28001, MWA16_NOP },
	{ 0xA30000, 0xA30001, MWA16_NOP },
	{ 0xA38000, 0xA38001, MWA16_NOP },
	{ 0xA40000, 0xA40001, MWA16_NOP },
	{ 0xA48000, 0xA48001, MWA16_NOP },
	{ 0xA50000, 0xA50001, MWA16_NOP },
	{ 0xA58000, 0xA58001, MWA16_NOP },
	{ 0xA60000, 0xA60001, MWA16_NOP },
	{ 0xA68000, 0xA68001, MWA16_NOP },
	{ 0xA70000, 0xA70001, MWA16_NOP },
	{ 0xA78000, 0xA78001, MWA16_NOP },
	{ 0xA80000, 0xA80001, MWA16_NOP },
	{ 0xA88000, 0xA88001, MWA16_NOP },
	{ 0xA90000, 0xA90001, MWA16_NOP },
	{ 0xA98000, 0xA98001, MWA16_NOP },

MEMORY_END


static struct GfxLayout chindrag_charlayout =

{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0,1,2,3 },
	{ 4, 0,
	  12,  8,
	  20,16,
	  28,24,},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	8*32
};

static struct GfxLayout chindrag2_charlayout =

{
	8,8,
	RGN_FRAC(1,1),
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0,8,
	  16,24,
	  32,40,
	  48,56 },
	{ 0*32*2, 1*32*2, 2*32*2, 3*32*2, 4*32*2, 5*32*2, 6*32*2, 7*32*2 },
	8*32*2
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chindrag_charlayout,   0, 1  },
	{ REGION_GFX2, 0, &chindrag_charlayout,   0, 16 },
	{ REGION_GFX1, 0, &chindrag2_charlayout,   0, 1  },
	{ REGION_GFX2, 0, &chindrag2_charlayout,   0, 16 },
	{ -1 } /* end of array */
};


INPUT_PORTS_START( chindrag )
INPUT_PORTS_END

static MACHINE_DRIVER_START( chindrag )
	MDRV_CPU_ADD(M68000, 12000000)
	MDRV_CPU_MEMORY(chindrag_readmem,chindrag_writemem)
/*	MDRV_CPU_VBLANK_INT(irq6_line_hold,1)*/

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_GFXDECODE(gfxdecodeinfo)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(40*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 1*8, 30*8-1)
	MDRV_PALETTE_LENGTH(0x300)

	MDRV_VIDEO_START(chindrag)
	MDRV_VIDEO_UPDATE(chindrag)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
MACHINE_DRIVER_END


void dw_decrypt(void)
{
	int i;
	data16_t *src = (data16_t *) (memory_region(REGION_CPU1));

	int rom_size = 0x80000;

	for(i=0; i<rom_size/2; i++) {
		data16_t x = src[i];

	if((i & 0x2000) == 0x0000 || (i & 0x0004) == 0x0000 || (i & 0x0090) == 0x0000)
		x ^= 0x0004;

	if((i & 0x0100) == 0x0100 || (i & 0x0040) == 0x0040 || (i & 0x0012) == 0x0012)
		x ^= 0x0020;

	if((((i & 0x1000) == 0x1000) ^ ((i & 0x0100) == 0x0100))
		|| (i & 0x0880) == 0x0800 || (i & 0x0240) == 0x0240)
			x ^= 0x0200;

	if((x & 0x0024) == 0x0004 || (x & 0x0024) == 0x0020)
		x ^= 0x0024;

	src[i] = x;

	}
}

static DRIVER_INIT( chindrag )
{
	dw_decrypt();
}


ROM_START( chindrag )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_WORD_SWAP( "v-021j",         0x00000, 0x80000, CRC(2f87f6e4) SHA1(d43065b078fdd9605c121988ad3092dce6cf0bf1) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* GFX? */
	ROM_LOAD( "d0301",         0x00000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* GFX? */
	ROM_LOAD( "cg",         0x00000, 0x20000, CRC(2dda0be3) SHA1(587b7cab747d4336515c98eb3365341bb6c7e5e4) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* Samples? */
	ROM_LOAD( "sp",         0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) )
ROM_END

GAMEX( 1994, chindrag, 0, chindrag, chindrag, chindrag, ROT0, "IGS", "China Dragon - Dragon World", GAME_NO_SOUND | GAME_NOT_WORKING )
