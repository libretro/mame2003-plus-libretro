/***************************************************************************

	Mustache Boy
	(c)1987 March Electronics

	(there are also Seibu and Taito logos/copyrights in the ROMs)

 driver by Tomasz Slanina dox@space.pl

 The hardware similar to Knuckle Joe.

		TODO - Sound
		   - YM2151 + YM 3012
       - custom  T5182 cpu
       - 27256 ROM with data/code (?)  (can be replaced with 27512)
                     ______________________
                   _|*                     |_
             GND  |_|1                   50|_| Vcc
                   _|                      |_
              A8  |_|2                   49|_| A7
                   _|                      |_
              A9  |_|3                   48|_| A6
                   _|                      |_
             A10  |_|4                   47|_| A5
                   _|                      |_
             A11  |_|5                   46|_| A4
                   _|       TOSHIBA        |_
             A12  |_|6       T5182       45|_| A3
                   _|                      |_
             A13  |_|7                   44|_| A2
                   _|     JAPAN  8612      |_
             A14  |_|8                   43|_| A1
                   _|                      |_
            A15?  |_|9                   42|_| A0
                   _|                      |_
       ROM/YM D4  |_|10                  41|_| D3 ROM/YM
                   _|                      |_
       ROM/YM D5  |_|11                  40|_| D2 ROM/YM
                   _|                      |_
       ROM/YM D6  |_|12                  39|_| D1 ROM/YM
                   _|                      |_
       ROM/YM D7  |_|13                  38|_| D0 ROM/YM
                   _|                      |_
                  |_|14                  37|_|
                   _|                      |_   __
                  |_|15                  36|_|  CS YM2151
                   _|                      |_
                  |_|16                  35|_|
                   _|                      |_
                  |_|17                  34|_|
                   _|                      |_
                  |_|18                  33|_|
                   _|                      |_   __
                  |_|19                  32|_|  IC  YM2151
                   _|                      |_
                  |_|20                  31|_|
                   _|                      |_
                  |_|21                  30|_|
                   _|                      |_
                  |_|22                  29|_|
                   _|                      |_
                  |_|23                  28|_|
                   _|                      |_
                  |_|24                  27|_|
                   _|                      |_
             GND  |_|25                  26|_|  Vcc
                    |______________________|

Based on sketch made by Tormod

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/seibu.h"

WRITE_HANDLER( mustache_videoram_w );
WRITE_HANDLER( mustache_scroll_w );
WRITE_HANDLER ( mustache_video_control_w);
VIDEO_START( mustache );
VIDEO_UPDATE( mustache );
PALETTE_INIT( mustache );


static int read_coins=0;

READ_HANDLER ( mustache_coin_hack_r )
{
	return (read_coins)?((offset&1	)?(input_port_5_r(0)<<5)|(input_port_5_r(0)<<7):(input_port_5_r(0)<<4)):0;
}


static MEMORY_READ_START( readmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, videoram_r },		/* videoram */
	{ 0xd001, 0xd001, MRA_RAM }, /* T5182 ? */
	{ 0xd400, 0xd4ff, MRA_RAM }, /* shared with T5182 ?*/
	{ 0xd800, 0xd800, input_port_0_r }, /* IN 0 */
	{ 0xd801, 0xd801, input_port_1_r }, /* IN 1 */
	{ 0xd802, 0xd802, input_port_2_r }, /* IN 2 */
	{ 0xd803, 0xd803, input_port_3_r },	/* DSW A */
	{ 0xd804, 0xd804, input_port_4_r },	/* DSW B */
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, mustache_videoram_w, &videoram },
	{ 0xd000, 0xd003, MWA_RAM }, /* T5182 ? */
	{ 0xd400, 0xd4ff, MWA_RAM }, /* shared with T5182 ?*/
	{ 0xd806, 0xd806, mustache_scroll_w },
	{ 0xd807, 0xd807, mustache_video_control_w },
	{ 0xe800, 0xefff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf000, 0xffff, MWA_RAM },
MEMORY_END

/******************************************************************************/

INPUT_PORTS_START( mustache )
	PORT_START	/* IN 1 */

	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP  )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START	/* IN 2 */

	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY| IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY| IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY| IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT| IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1| IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2| IPF_COCKTAIL )

	PORT_START	/* IN 3 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT( 0xf9, IP_ACTIVE_LOW, IPT_UNUSED  )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_DIPNAME( 0x06, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x06, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x18, "3" )
	PORT_DIPSETTING(    0x00, "5" )

	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* DSW B */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )

	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )

	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )

INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(0,3), RGN_FRAC(1,3),RGN_FRAC(2,3)},
	{STEP8(7,-1)},
	{STEP8(0,8)},
	8*8
};
static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,4),
	4,
	{ RGN_FRAC(1,4), RGN_FRAC(3,4),RGN_FRAC(0,4),RGN_FRAC(2,4)},
	{STEP16(15,-1)},
	{STEP16(0,16)},
	16*16
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0x00, 16 },
	{ REGION_GFX2, 0, &spritelayout, 0x80, 8 },
	{ -1 } /* end of array */
};


INTERRUPT_GEN( mustache_interrupt)
{
	read_coins^=1;
	cpu_set_irq_line(0, 0, PULSE_LINE);
}


static MACHINE_DRIVER_START( mustache )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 18432000/4) /* maybe 12000000/3 - two xtals (18.432 and 12.xxx) near cpu*/
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_VBLANK_INT(mustache_interrupt,2)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(1*8, 31*8-1, 2*8, 31*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8*16+16*8)

	MDRV_PALETTE_INIT(mustache)
	MDRV_VIDEO_START(mustache)
	MDRV_VIDEO_UPDATE(mustache)
MACHINE_DRIVER_END

ROM_START( mustache )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "mustache.h18", 0x0000, 0x8000, CRC(123bd9b8) SHA1(33a7cba5c3a54b0b1a15dd1e24d298b6f7274321) )
	ROM_LOAD( "mustache.h16", 0x8000, 0x4000, CRC(62552beb) SHA1(ee10991d7de0596608fa1db48805781cbfbbdb9f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )  /* T5182 */
	ROM_LOAD( "mustache.e5",0x0000, 0x8000, CRC(efbb1943) SHA1(3320e9eaeb776d09ed63f7dedc79e720674e6718) )

	ROM_REGION( 0x0c000, REGION_GFX1,0)	/* BG tiles  */
	ROM_LOAD( "mustache.a13", 0x0000,  0x4000, CRC(9baee4a7) SHA1(31bcec838789462e67e54ebe7256db9fc4e51b69) )
	ROM_LOAD( "mustache.a14", 0x4000,  0x4000, CRC(8155387d) SHA1(5f0a394c7671442519a831b0eeeaba4eecd5a406) )
	ROM_LOAD( "mustache.a16", 0x8000,  0x4000, CRC(4db4448d) SHA1(50a94fd65c263d95fd24b4009dbb87707929fdcb) )

	ROM_REGION( 0x20000, REGION_GFX2,0 )	/* sprites */
	ROM_LOAD( "mustache.a4", 0x00000,  0x8000, CRC(d5c3bbbf) SHA1(914e3feea54246476701f492c31bd094ad9cea10) )
	ROM_LOAD( "mustache.a7", 0x08000,  0x8000, CRC(e2a6012d) SHA1(4e4cd1a186870c8a88924d5bff917c6889da953d) )
	ROM_LOAD( "mustache.a5", 0x10000,  0x8000, CRC(c975fb06) SHA1(4d166bd79e19c7cae422673de3e095ad8101e013) )
	ROM_LOAD( "mustache.a8", 0x18000,  0x8000, CRC(2e180ee4) SHA1(a5684a25c337aeb4effeda7982164d35bc190af9) )

	ROM_REGION( 0x1300, REGION_PROMS,0 )	/* proms */
	ROM_LOAD( "mustache.c3",0x0000, 0x0100, CRC(68575300) SHA1(bc93a38df91ad8c2f335f9bccc98b52376f9b483) )
	ROM_LOAD( "mustache.c2",0x0100, 0x0100, CRC(eb008d62) SHA1(a370fbd1affaa489210ea36eb9e365263fb4e232) )
	ROM_LOAD( "mustache.c1",0x0200, 0x0100, CRC(65da3604) SHA1(e4874d4152a57944d4e47306250833ea5cd0d89b) )

	ROM_LOAD( "mustache.b6",0x0300, 0x1000, CRC(5f83fa35) SHA1(cb13e63577762d818e5dcbb52b8a53f66e284e8f) ) /* 63S281N near SEI0070BU */
ROM_END

static DRIVER_INIT( mustache )
{
	int i;

	#define G1 (memory_region_length(REGION_GFX1)/3)
	#define G2 (memory_region_length(REGION_GFX2)/2)

	UINT8 *buf=auto_malloc(G2*2);

	/* BG data lines */
	for (i=0;i<G1; i++)
	{
		UINT16 w;

		buf[i] = BITSWAP8(memory_region(REGION_GFX1)[i], 0,5,2,6,4,1,7,3);

		w = (memory_region(REGION_GFX1)[i+G1] << 8) | memory_region(REGION_GFX1)[i+G1*2];
		w = BITSWAP16(w, 14,1,13,5,9,2,10,6, 3,8,4,15,0,11,12,7);

		buf[i+G1]   = w >> 8;
		buf[i+G1*2] = w & 0xff;
	}

	/* BG address lines */
	for (i = 0; i < 3*G1; i++)
		memory_region(REGION_GFX1)[i] = buf[BITSWAP16(i,15,14,13,2,1,0,12,11,10,9,8,7,6,5,4,3)];

	/* SPR data lines */
	for (i=0;i<G2; i++)
	{
		UINT16 w;

		w = (memory_region(REGION_GFX2)[i] << 8) | memory_region(REGION_GFX2)[i+G2];
		w = BITSWAP16(w, 5,7,11,4,15,10,3,14, 9,2,13,8,1,12,0,6 );

		buf[i]    = w >> 8;
		buf[i+G2] = w & 0xff;
	}

	/* SPR address lines */
	for (i = 0; i < 2*G2; i++)
		memory_region(REGION_GFX2)[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,12,11,10,9,8,7,6,5,4,13,14,3,2,1,0)];

	seibu_sound_decrypt(REGION_CPU1,0x8000);

	install_mem_read_handler( 0, 0xd400, 0xd401, mustache_coin_hack_r);
}


GAMEX( 1987, mustache, 0, mustache, mustache, mustache, ROT90, "[Seibu Kaihatsu] (March license)", "Mustache Boy", GAME_NO_SOUND )
