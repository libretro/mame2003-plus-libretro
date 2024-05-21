/*

    T.S. 17.12.2005:
    Yamato:
    -------
     Added temporary bg gradient (bad colors/offset).
     Gradient table are stored in two(?) ROMs.
     Each table is 256 bytes long: 128 for normal
     and 128 bytes for flipped screen.
     Color format is unknown - probably direct RGB
     mapping of 8 or 16 (both roms) bits. Also table
     selection source is unknown.
     TODO:
      - bg gradient color decode & table selection
 Top Roller:
 ----------
     It's made by the same developers as Yamato and use
     probably the same encrypted SEGA cpu as Yamato.
     lives - $6155
	 
     TODO:
       - COINB DSW is missing
       - few issues in cocktail mode
       - wrong colors (fg text layer) - game sometimes ("round" text , lives) updates only even columns of cell attribs...
-------------------------------------------------------------
 Top Roller
 Jaleco
 Hardware : Original Jaleco board no 8307-B/8307-A(redump)
 Main CPU : Encrypted Z80 (probably 315-5018)
 Sound : AY-3-8910
 ROMS CRC32 + positions :
 [9894374d]  d5
 [ef789f00]  f5
 [d45494ba]  h5
 [1cb48ea0]  k5
 [84139f46]  l5
 [e30c1dd8]  m5
 [904fffb6]  d3
 [94371cfb]  f3
 [8a8032a7]  h3
 [1e8914a6]  k3
 [b20a9fa2]  l3
 [7f989dc9]  p3
 [89327329]  a4 bottom board 89327329
 [7a945733]  c4 bottom board
 [5f2c2a78]  h4 bottom board  bad dump / [1d9e3325] (8307-A)
 [ce3afe26]  j4 bottom board

*/


#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/segacrpt.h"



extern unsigned char *cclimber_bsvideoram;
extern size_t cclimber_bsvideoram_size;
extern unsigned char *cclimber_bigspriteram;
extern unsigned char *cclimber_column_scroll;

extern UINT8 *toprollr_videoram2;
extern UINT8 *toprollr_videoram3;
extern UINT8 *toprollr_videoram4;

WRITE_HANDLER( cclimber_colorram_w );
WRITE_HANDLER( cclimber_bigsprite_videoram_w );
PALETTE_INIT( cclimber );
VIDEO_UPDATE( yamato );
VIDEO_START( toprollr );
VIDEO_UPDATE( toprollr );

extern struct AY8910interface cclimber_ay8910_interface;
extern struct CustomSound_interface cclimber_custom_interface;
WRITE_HANDLER( cclimber_sample_trigger_w );
WRITE_HANDLER( cclimber_sample_rate_w );
WRITE_HANDLER( cclimber_sample_volume_w );


PALETTE_INIT( yamato )
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + (offs)])


	/* chars - 12 bits RGB */
	for (i = 0;i < 64;i++)
	{
		int bit0,bit1,bit2,bit3,r,g,b;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[64] >> 0) & 0x01;
		bit1 = (color_prom[64] >> 1) & 0x01;
		bit2 = (color_prom[64] >> 2) & 0x01;
		bit3 = (color_prom[64] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		palette_set_color(i,r,g,b);
		color_prom++;
	}
	color_prom += 64;

	/* big sprite - 8 bits RGB */
	for (i = 0;i < 32;i++)
	{
		int bit0,bit1,bit2,r,g,b;


		/* red component */
		bit0 = (*color_prom >> 0) & 0x01;
		bit1 = (*color_prom >> 1) & 0x01;
		bit2 = (*color_prom >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (*color_prom >> 3) & 0x01;
		bit1 = (*color_prom >> 4) & 0x01;
		bit2 = (*color_prom >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (*color_prom >> 6) & 0x01;
		bit2 = (*color_prom >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		palette_set_color(i+64,r,g,b);
		color_prom++;
	}


	/* character and sprite lookup table */
	/* they use colors 0-63 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
	{
		/* pen 0 always uses color 0 (background in River Patrol and Silver Land) */
		if (i % 4 == 0) COLOR(0,i) = 0;
		else COLOR(0,i) = i;
	}

	/* big sprite lookup table */
	/* it uses colors 64-95 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		if (i % 4 == 0) COLOR(2,i) = 0;
		else COLOR(2,i) = i + 64;
	}

	/* fake colors for bg gradient */
	for (i = 0;i < 256;i++)
	{
		palette_set_color(i+16*4+8*4,0,0,i);
	}
}

PALETTE_INIT( toprollr )
{
	int i;

	for (i = 0;i < 32*5;i++)
	{
		int bit0,bit1,bit2,r,g,b;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;


		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (color_prom[0] >> 3) & 0x01;
		bit1 = (color_prom[0] >> 4) & 0x01;
		bit2 = (color_prom[0] >> 5) & 0x01;

		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[0] >> 6) & 0x01;
		bit2 = (color_prom[0] >> 7) & 0x01;

		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;


		palette_set_color(i,r,g,b);
		color_prom++;
	}

}


static int p0,p1;

static WRITE_HANDLER( p0_w )
{
	p0 = data;
}
static WRITE_HANDLER( p1_w )
{
	p1 = data;
}
static READ_HANDLER( p0_r )
{
	return p0;
}
static READ_HANDLER( p1_r )
{
	return p1;
}

static WRITE_HANDLER( flip_screen_x_w )
{
	flip_screen_x_set(data);
}

static WRITE_HANDLER( flip_screen_y_w )
{
	flip_screen_y_set(data);
}


static MEMORY_READ_START( yamato_readmem )
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_RAM },
	{ 0x7000, 0x7fff, MRA_ROM },
	{ 0x8800, 0x8bff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },	/* video RAM */
	{ 0x9800, 0x9bff, MRA_RAM },	/* column scroll registers */
	{ 0x9c00, 0x9fff, MRA_RAM },	/* color RAM */
	{ 0xa000, 0xa000, input_port_0_r },     /* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },     /* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },     /* DSW */
	{ 0xb800, 0xb800, input_port_3_r },     /* IN2 */
	{ 0xba00, 0xba00, input_port_4_r },     /* IN3 (maybe a mirror of b800) */
MEMORY_END

static MEMORY_WRITE_START( yamato_writemem )
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x6fff, MWA_RAM },
	{ 0x7000, 0x7fff, MWA_ROM },
	{ 0x8800, 0x88ff, cclimber_bigsprite_videoram_w, &cclimber_bsvideoram, &cclimber_bsvideoram_size },
	{ 0x8900, 0x8bff, MWA_RAM },  /* not used, but initialized */
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
/*{ 0x9400, 0x97ff, videoram_w },  // mirror address, used by Crazy Climber to draw windows /*/
	/* 9800-9bff and 9c00-9fff share the same RAM, interleaved */
	/* (9800-981f for scroll, 9c20-9c3f for color RAM, and so on) */
	{ 0x9800, 0x981f, MWA_RAM, &cclimber_column_scroll },
	{ 0x9880, 0x989f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x98dc, 0x98df, MWA_RAM, &cclimber_bigspriteram },
	{ 0x9800, 0x9bff, MWA_RAM },  /* not used, but initialized */
	{ 0x9c00, 0x9fff, cclimber_colorram_w, &colorram },
	{ 0xa000, 0xa000, interrupt_enable_w },
	{ 0xa001, 0xa001, flip_screen_x_w },
	{ 0xa002, 0xa002, flip_screen_y_w },
/*{ 0xa004, 0xa004, cclimber_sample_trigger_w },*/
/*{ 0xa800, 0xa800, cclimber_sample_rate_w },*/
/*{ 0xb000, 0xb000, cclimber_sample_volume_w },*/
MEMORY_END

static PORT_WRITE_START( yamato_writeport )
	{ 0x00, 0x00, p0_w },	/* ??? */
	{ 0x01, 0x01, p1_w },	/* ??? */
PORT_END

static MEMORY_READ_START( yamato_sound_readmem )
	{ 0x0000, 0x07ff, MRA_ROM },
	{ 0x5000, 0x53ff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( yamato_sound_writemem )
	{ 0x0000, 0x07ff, MWA_ROM },
	{ 0x5000, 0x53ff, MWA_RAM },
MEMORY_END

static PORT_READ_START( yamato_sound_readport )
	{ 0x04, 0x04, p0_r },	/* ??? */
	{ 0x08, 0x08, p1_r },	/* ??? */
PORT_END

static PORT_WRITE_START( yamato_sound_writeport )
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x02, 0x02, AY8910_control_port_1_w },
	{ 0x03, 0x03, AY8910_write_port_1_w },
PORT_END

/* Top Roller */
static int jaleco_rombank=0;
extern UINT8 *top_decrypted_opcodes;
static WRITE_HANDLER(rombank_w)
{
	UINT8 *bank_rom = memory_region(REGION_USER1);
	jaleco_rombank&=~(1<<offset);
	jaleco_rombank|=(data&1)<<offset;

	if(jaleco_rombank<3)
	{
		memcpy(memory_region(REGION_CPU1)  ,&bank_rom[0x6000 * jaleco_rombank], 0x6000);
		memcpy(&top_decrypted_opcodes[0x0],  &bank_rom[0x12000+ (0x6000 * jaleco_rombank)], 0x6000);
	///	printf("bank %d\n",jaleco_rombank);
	}
}



static MEMORY_READ_START( tr_readmem )
    { 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6bff, MRA_RAM },
	{ 0x8c00, 0x8fff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9400, 0x97ff, MRA_RAM },
	{ 0x9800, 0x987f, MRA_RAM }, /* unused ? */
	{ 0x9880, 0x995f, MRA_RAM },
	{ 0x99dc, 0x99df, MRA_RAM },
	{ 0x9c00, 0x9fff, MRA_RAM },
	{ 0xa000, 0xa000, input_port_0_r },  /* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },  /* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },  /* DSW */
	{ 0xb800, 0xb800, input_port_3_r },  /* IN2 */
	{ 0xc000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( tr_writemem )
    { 0x6000, 0x6bff, MWA_RAM },
	{ 0x8800, 0x88ff, cclimber_bigsprite_videoram_w, &cclimber_bsvideoram, &cclimber_bsvideoram_size },
	{ 0x8c00, 0x8fff, MWA_RAM, &toprollr_videoram3 },
	{ 0x9000, 0x93ff, MWA_RAM, &videoram },
	{ 0x9400, 0x97ff, MWA_RAM, &toprollr_videoram4 },
	{ 0x9800, 0x987f, MWA_RAM }, /* unused ? */
	{ 0x9880, 0x995f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x99dc, 0x99df, MWA_RAM, &cclimber_bigspriteram },
	{ 0x9c00, 0x9fff, MWA_RAM, &toprollr_videoram2 },
	{ 0xa000, 0xa000, interrupt_enable_w },
	{ 0xa001, 0xa001, flip_screen_x_w },
	{ 0xa002, 0xa002, flip_screen_y_w },
    { 0xa004, 0xa004, cclimber_sample_trigger_w },
	{ 0xa005, 0xa006, rombank_w },
	{ 0xa800, 0xa800, cclimber_sample_rate_w },
	{ 0xb000, 0xb000, cclimber_sample_volume_w }, 
	{ 0xc000, 0xffff, MWA_ROM },
MEMORY_END

static PORT_READ_START( tr_readport )
    { 0x0d, 0x0d, MRA_NOP },
PORT_END

static PORT_WRITE_START( tr_writeport )
    { 0x08, 0x08, AY8910_control_port_0_w },
	{ 0x09, 0x09, AY8910_write_port_0_w },
	{ 0x0d, 0x0d, MWA_NOP },
PORT_END


INPUT_PORTS_START( yamato )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* DSW */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Speed" )
	PORT_DIPSETTING(    0x00, "Slow" )
	PORT_DIPSETTING(    0x40, "Fast" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )	/* set 1 only */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN3 )	/* set 1 only */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( toprollr )
	PORT_START      /* IN0 */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* DSW */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "Every 30000" )
	PORT_DIPSETTING(    0x20, "Every 50000" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	512,    /* 512 characters (256 in Crazy Climber) */
	2,      /* 2 bits per pixel */
	{ 0, 512*8*8 }, /* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout bscharlayout =
{
	8,8,    /* 8*8 characters */
	512,/*256,     // 256 characters /*/
	2,      /* 2 bits per pixel */
	{ 0, 512*8*8 }, /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },     /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	128,    /* 128 sprites (64 in Crazy Climber) */
	2,      /* 2 bits per pixel */
	{ 0, 128*16*16 },       /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,       /* pretty straightforward layout */
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout trcharlayout =
{
	8,8,
	RGN_FRAC(1,2),
	2,
	{ RGN_FRAC(1,2),0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout trspritelayout =
{
	16,16,  /* 16*16 sprites */
	RGN_FRAC(1,2),    /* 128 sprites (64 in Crazy Climber) */
	2,      /* 2 bits per pixel */
	{ RGN_FRAC(1,2),0 },       /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,       /* pretty straightforward layout */
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,      0, 16 }, /* char set #1 */
	{ REGION_GFX1, 0x2000, &charlayout,      0, 16 }, /* char set #2 */
	{ REGION_GFX2, 0x0000, &bscharlayout, 16*4,  8 }, /* big sprite char set */
	{ REGION_GFX1, 0x0000, &spritelayout,    0, 16 }, /* sprite set #1 */
	{ REGION_GFX1, 0x2000, &spritelayout,    0, 16 }, /* sprite set #2 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo tr_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &trcharlayout,      0, 40 },
	{ REGION_GFX2, 0x0000, &trcharlayout,      0, 40 },
	{ REGION_GFX1, 0x0000, &trspritelayout,    0, 40 },
	{ REGION_GFX3, 0x0000, &trcharlayout,   	 0, 40 },
	{ -1 } /* end of array */
};



static struct AY8910interface yamato_ay8910_interface =
{
	2,      /* 2 chips */
	1536000,	/* 1.536 MHz??? */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static MACHINE_DRIVER_START( yamato )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)	/* 3.072 MHz ? */
	MDRV_CPU_MEMORY(yamato_readmem,yamato_writemem)
	MDRV_CPU_PORTS(0,yamato_writeport)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz ? */
	MDRV_CPU_MEMORY(yamato_sound_readmem,yamato_sound_writemem)
	MDRV_CPU_PORTS(yamato_sound_readport,yamato_sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(96+256)
	MDRV_COLORTABLE_LENGTH(16*4+8*4)

	MDRV_PALETTE_INIT(yamato)
	MDRV_VIDEO_START(generic)
	MDRV_VIDEO_UPDATE(yamato)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, yamato_ay8910_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( toprollr )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(tr_readmem,tr_writemem)
	MDRV_CPU_PORTS(tr_readport,tr_writeport)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(1*8, 31*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(tr_gfxdecodeinfo)
    MDRV_PALETTE_LENGTH(32*5)
	MDRV_PALETTE_INIT(toprollr)

	MDRV_VIDEO_START(toprollr)
	MDRV_VIDEO_UPDATE(toprollr)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, cclimber_ay8910_interface)
	MDRV_SOUND_ADD(CUSTOM, cclimber_custom_interface)
MACHINE_DRIVER_END

ROM_START( yamato )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "2.5de",        0x0000, 0x2000, CRC(20895096) SHA1(af76786e3c519e710899f143d46c53087e9817c7) )
	ROM_LOAD( "3.5f",         0x2000, 0x2000, CRC(57a696f9) SHA1(28ea80fb100ac92295fc3eb318617d7cb014408d) )
	ROM_LOAD( "4.5jh",        0x4000, 0x2000, CRC(59a468e8) SHA1(a79cdee6efefd87a356cc8d710f8050bc12e07c3) )
	/* hole at 6000-6fff */
	ROM_LOAD( "11.5a",        0x7000, 0x1000, CRC(35987485) SHA1(1f0cb545bbd52982cbf801bc1dd2c4087af2f5f7) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for sound cpu */
	ROM_LOAD( "1.5v",         0x0000, 0x0800, CRC(3aad9e3c) SHA1(37b0414b265397881bb45b166ecab85880d1358d) )

	ROM_REGION( 0x4000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "10.11k",       0x0000, 0x1000, CRC(161121f5) SHA1(017c5c6b773b0ae1d0be52e4bac90b699ea196dd) )
	ROM_CONTINUE(             0x2000, 0x1000 )
	ROM_LOAD( "9.11h",        0x1000, 0x1000, CRC(56e84cc4) SHA1(c48e0e5460376d6b34173c42a27907ef12218182) )
	ROM_CONTINUE(             0x3000, 0x1000 )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "8.11c",        0x0000, 0x1000, CRC(28024d9a) SHA1(c871c4d74be72a8bfea99e89d43f91922f4b734b) )
	ROM_LOAD( "7.11a",        0x1000, 0x1000, CRC(4a179790) SHA1(7fb6b033de939ff8bd13055c073311dca2c1a6fe) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )
	ROM_LOAD( "5.5lm",        0x0000, 0x1000, CRC(7761ad24) SHA1(98878b19addd142d35718080eece05eaaee0388d) )	/* ?? */
	ROM_LOAD( "6.5n",         0x1000, 0x1000, CRC(da48444c) SHA1(a43e672ce262eb817fb4e5715ef4fb304a6a2815) )

	ROM_REGION( 0x00a0, REGION_PROMS, 0 )
	ROM_LOAD( "1.bpr",        0x0000, 0x0020, CRC(ef2053ab) SHA1(2006cbf003f90a8e75f39047a88a3bba85d78e80) )
	ROM_LOAD( "2.bpr",        0x0020, 0x0020, CRC(2281d39f) SHA1(e9b568bdacf7ab611801cf42ea5c7624f5440ef6) )
	ROM_LOAD( "3.bpr",        0x0040, 0x0020, CRC(9e6341e3) SHA1(2e7a4d3c1f40d6089735734b9d9de2ca57fb73c7) )
	ROM_LOAD( "4.bpr",        0x0060, 0x0020, CRC(1c97dc0b) SHA1(fe8e0a91172abdd2d14b199da144306a9b944372) )
	ROM_LOAD( "5.bpr",        0x0080, 0x0020, CRC(edd6c05f) SHA1(b95db8aaf74fe175d1179f0d85f79242b16f5fb4) )
ROM_END

ROM_START( yamato2 )
	ROM_REGION( 2*0x10000, REGION_CPU1, 0 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "2-2.5de",      0x0000, 0x2000, CRC(93da1d52) SHA1(21b72856ebbd969e4e075b52719e6acdbd1bc4c5) )
	ROM_LOAD( "3-2.5f",       0x2000, 0x2000, CRC(31e73821) SHA1(e582c9fcea1b29d43f65b6aa67e1895c38d2736c) )
	ROM_LOAD( "4-2.5jh",      0x4000, 0x2000, CRC(fd7bcfc3) SHA1(5037170cb3a9824794e90d74def92b0b25d45caa) )
	/* hole at 6000-6fff */
	/* 7000-7fff not present here */

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for sound cpu */
	ROM_LOAD( "1.5v",         0x0000, 0x0800, CRC(3aad9e3c) SHA1(37b0414b265397881bb45b166ecab85880d1358d) )

	ROM_REGION( 0x4000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "10.11k",       0x0000, 0x1000, CRC(161121f5) SHA1(017c5c6b773b0ae1d0be52e4bac90b699ea196dd) )
	ROM_CONTINUE(             0x2000, 0x1000 )
	ROM_LOAD( "9.11h",        0x1000, 0x1000, CRC(56e84cc4) SHA1(c48e0e5460376d6b34173c42a27907ef12218182) )
	ROM_CONTINUE(             0x3000, 0x1000 )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "8.11c",        0x0000, 0x1000, CRC(28024d9a) SHA1(c871c4d74be72a8bfea99e89d43f91922f4b734b) )
	ROM_LOAD( "7.11a",        0x1000, 0x1000, CRC(4a179790) SHA1(7fb6b033de939ff8bd13055c073311dca2c1a6fe) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )
	ROM_LOAD( "5.5lm",        0x0000, 0x1000, CRC(7761ad24) SHA1(98878b19addd142d35718080eece05eaaee0388d) )	/* ?? */
	ROM_LOAD( "6.5n",         0x1000, 0x1000, CRC(da48444c) SHA1(a43e672ce262eb817fb4e5715ef4fb304a6a2815) )

	ROM_REGION( 0x00a0, REGION_PROMS, 0 )
	ROM_LOAD( "1.bpr",        0x0000, 0x0020, CRC(ef2053ab) SHA1(2006cbf003f90a8e75f39047a88a3bba85d78e80) )
	ROM_LOAD( "2.bpr",        0x0020, 0x0020, CRC(2281d39f) SHA1(e9b568bdacf7ab611801cf42ea5c7624f5440ef6) )
	ROM_LOAD( "3.bpr",        0x0040, 0x0020, CRC(9e6341e3) SHA1(2e7a4d3c1f40d6089735734b9d9de2ca57fb73c7) )
	ROM_LOAD( "4.bpr",        0x0060, 0x0020, CRC(1c97dc0b) SHA1(fe8e0a91172abdd2d14b199da144306a9b944372) )
	ROM_LOAD( "5.bpr",        0x0080, 0x0020, CRC(edd6c05f) SHA1(b95db8aaf74fe175d1179f0d85f79242b16f5fb4) )
ROM_END

ROM_START( toprollr )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	
	ROM_LOAD( "10.k3", 0xc000, 0x2000, CRC(1e8914a6) SHA1(ec17f185f890d04ce75a5d8edf8b32da60e7a8d8) )
	ROM_LOAD( "11.l3", 0xe000, 0x2000, CRC(b20a9fa2) SHA1(accd3296447eca002b0808e7b02832f5e35407e8) )

	ROM_REGION( 0x04000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "15.h4", 0x0000, 0x2000, CRC(b6fe97f2) SHA1(00c58f693dda0aa3ea4893dcaae90b1b63054789) )
	ROM_LOAD( "16.j4", 0x2000, 0x2000, CRC(ce3afe26) SHA1(7de00720f091537c64cc0fec687c061de3a8b1a3) )

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5.l5",  0x0000, 0x1000, CRC(84139f46) SHA1(976f781fb279dd540778708174b942a263f16443) )
	ROM_LOAD( "6.m5",  0x1000, 0x1000, CRC(e30c1dd8) SHA1(1777bf98625153c9b191020860e4e1839b46b998) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "13.a4", 0x0000, 0x2000, CRC(89327329) SHA1(555331a3136aa8c5bb35b97dd54bc59da067be57) )
	ROM_LOAD( "14.c4", 0x2000, 0x2000, CRC(7a945733) SHA1(14187ba303aecf0a812c425c34d8edda3deaa2b5) )

	ROM_REGION( 0x12000 * 2, REGION_USER1, 0 )
	ROM_LOAD( "2.f5",	0x00000, 0x02000, CRC(ef789f00) SHA1(424d69584d391ee7b9ad5db7ee6ced97d69897d4) )
	ROM_LOAD( "8.f3",	0x02000, 0x02000, CRC(94371cfb) SHA1(cb501c36b213c995a4048b3a96c85848c556cd05) )
	ROM_LOAD( "4.k5",	0x04000, 0x02000, CRC(1cb48ea0) SHA1(fdc75075112042ec84a7d1b3e5b5a6db1d1cb871) )
	ROM_LOAD( "3.h5",	0x06000, 0x02000, CRC(d45494ba) SHA1(6e235b34f9457acadad6d4e27799978bc2e3db08) )
	ROM_LOAD( "9.h3",	0x08000, 0x02000, CRC(8a8032a7) SHA1(d6642d72645c613c21f65bbbe1560d0437d41f43) )
	ROM_COPY( REGION_USER1, 0x04000, 0x0a000, 0x02000 )
	ROM_LOAD( "1.d5",	0x0c000, 0x02000, CRC(9894374d) SHA1(173de4abbc3fb5d522aa6d6d5caf8e4d54f2a598) )
	ROM_LOAD( "7.d3",	0x0e000, 0x02000, CRC(904fffb6) SHA1(5528bc2a4d2fe8672428fd4725644265f0d57ded) )
	ROM_COPY( REGION_USER1, 0x04000, 0x10000, 0x02000 )

	ROM_REGION( 0x2000, REGION_SOUND1, 0 )
	ROM_LOAD( "12.p3",  0x0000, 0x2000, CRC(7f989dc9) SHA1(3b4d18cbb992872b3cf8f5eaf5381ed3a9468cc1) )

	ROM_REGION( 0x01a0, REGION_PROMS, 0 )
	ROM_LOAD( "prom.a1",  0x0000, 0x0020, CRC(7d626d6c) SHA1(7c7202d0ec5bf0381e7104eef53afa5fa4596a29) ) //00-07 big sprites
	ROM_LOAD( "prom.p2",  0x0020, 0x0020, CRC(42e828fa) SHA1(81250b1f7c3956b3902324adbbaf3b5989e854ee) ) //08-0f sprites + fg (wrong?)
	ROM_LOAD( "prom.r2",  0x0040, 0x0020, CRC(99b87eed) SHA1(06c3164d681fe4aff0338c0dad1a921f7fe7369d) ) //10-17 sprites
	ROM_LOAD( "prom.p9",  0x0060, 0x0020, CRC(eb399c02) SHA1(bf3d6c6dd982cb54446cf8a010b7adb949514bdb) ) //18-1f bg
	ROM_LOAD( "prom.n9",  0x0080, 0x0020, CRC(fb03ea99) SHA1(4dcef86106cef713dfcbd965072bfa8fe4b68e15) ) //20-27 bg
	ROM_LOAD( "prom.s9",  0x00a0, 0x0100, CRC(abf4c5fb) SHA1(a953f14642d4b72328293b36bc3c65b13491ffff) ) //unknown prom (filled with 2 bit vals)

ROM_END
static DRIVER_INIT( yamato )
{
	yamato_decode();
}

static DRIVER_INIT( toprollr )
{
	toprollr_decode();
}

GAME( 1983, yamato,   0,      yamato,   yamato,   yamato,   ROT90, "Sega", "Yamato (US)" )
GAME( 1983, yamato2,  yamato, yamato,   yamato,   yamato,   ROT90, "Sega", "Yamato (World?)" )
GAME(1983, toprollr, 0,      toprollr, toprollr, toprollr, ROT90, "Jaleco", "Top Roller" )

