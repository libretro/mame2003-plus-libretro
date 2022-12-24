/***************************************************************************

Mario Bros memory map (preliminary):

driver by Mirko Buffoni


0000-5fff ROM
6000-6fff RAM
7000-73ff ?
7400-77ff Video RAM
f000-ffff ROM

read:
7c00      IN0
7c80      IN1
7f80      DSW

*
 * IN0 (bits NOT inverted)
 * bit 7 : TEST
 * bit 6 : START 2
 * bit 5 : START 1
 * bit 4 : JUMP player 1
 * bit 3 : ? DOWN player 1 ?
 * bit 2 : ? UP player 1 ?
 * bit 1 : LEFT player 1
 * bit 0 : RIGHT player 1
 *
*
 * IN1 (bits NOT inverted)
 * bit 7 : ?
 * bit 6 : COIN 2
 * bit 5 : COIN 1
 * bit 4 : JUMP player 2
 * bit 3 : ? DOWN player 2 ?
 * bit 2 : ? UP player 2 ?
 * bit 1 : LEFT player 2
 * bit 0 : RIGHT player 2
 *
*
 * DSW (bits NOT inverted)
 * bit 7 : \ difficulty
 * bit 6 : / 00 = easy  01 = medium  10 = hard  11 = hardest
 * bit 5 : \ bonus
 * bit 4 : / 00 = 20000  01 = 30000  10 = 40000  11 = none
 * bit 3 : \ coins per play
 * bit 2 : /
 * bit 1 : \ 00 = 3 lives  01 = 4 lives
 * bit 0 : / 10 = 5 lives  11 = 6 lives
 *

write:
7d00      vertical scroll (pow)
7d80      ?
7e00      sound
7e80-7e82 ?
7e83      sprite palette bank select
7e84      interrupt enable
7e85      ?
7f00-7f07 sound triggers


I/O ports

write:
00        ?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/i8039/i8039.h"

static int p[8] = { 0,0xf0,0,0,0,0,0,0 };
static int t[2] = { 0,0 };

extern WRITE_HANDLER( mario_videoram_w );
extern WRITE_HANDLER( mario_gfxbank_w );
extern WRITE_HANDLER( mario_palettebank_w );
extern WRITE_HANDLER( mario_scroll_w );

extern PALETTE_INIT( mario );
extern VIDEO_START( mario );
extern VIDEO_UPDATE( mario );

/*
 *  from sndhrdw/mario.c
 */
extern WRITE_HANDLER( mario_sh_w );
extern WRITE_HANDLER( mario_sh1_w );
extern WRITE_HANDLER( mario_sh2_w );
extern WRITE_HANDLER( mario_sh3_w );


#define ACTIVELOW_PORT_BIT(P,A,D)   ((P & (~(1 << A))) | ((D ^ 1) << A))
#define ACTIVEHIGH_PORT_BIT(P,A,D)   ((P & (~(1 << A))) | (D << A))


WRITE_HANDLER( mario_sh_getcoin_w )    { t[0] = data; }
WRITE_HANDLER( mario_sh_crab_w )       { p[1] = ACTIVEHIGH_PORT_BIT(p[1],0,data); }
WRITE_HANDLER( mario_sh_turtle_w )     { p[1] = ACTIVEHIGH_PORT_BIT(p[1],1,data); }
WRITE_HANDLER( mario_sh_fly_w )        { p[1] = ACTIVEHIGH_PORT_BIT(p[1],2,data); }
static WRITE_HANDLER( mario_sh_tuneselect_w ) { soundlatch_w(offset,data); }

static READ_HANDLER( mario_sh_p1_r )   { return p[1]; }
static READ_HANDLER( mario_sh_p2_r )   { return p[2]; }
static READ_HANDLER( mario_sh_t0_r )   { return t[0]; }
static READ_HANDLER( mario_sh_t1_r )   { return t[1]; }
static READ_HANDLER( mario_sh_tune_r ) { return soundlatch_r(offset); }

static WRITE_HANDLER( mario_sh_sound_w )
{
	DAC_data_w(0,data);
}
static WRITE_HANDLER( mario_sh_p1_w )
{
	p[1] = data;
}
static WRITE_HANDLER( mario_sh_p2_w )
{
	p[2] = data;
}
WRITE_HANDLER( masao_sh_irqtrigger_w )
{
	static int last;


	if (last == 1 && data == 0)
	{
		/* setting bit 0 high then low triggers IRQ on the sound CPU */
		cpu_set_irq_line_and_vector(1,0,HOLD_LINE,0xff);
	}

	last = data;
}

static MEMORY_READ_START( readmem )
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_RAM },
	{ 0x7400, 0x77ff, MRA_RAM },	/* video RAM */
	{ 0x7c00, 0x7c00, input_port_0_r },	/* IN0 */
	{ 0x7c80, 0x7c80, input_port_1_r },	/* IN1 */
	{ 0x7f80, 0x7f80, input_port_2_r },	/* DSW */
	{ 0xf000, 0xffff, MRA_ROM },
MEMORY_END


static MEMORY_WRITE_START( writemem )
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x7400, 0x77ff, mario_videoram_w, &videoram },
	{ 0x7c00, 0x7c00, mario_sh1_w }, /* Mario run sample */
	{ 0x7c80, 0x7c80, mario_sh2_w }, /* Luigi run sample */
	{ 0x7d00, 0x7d00, mario_scroll_w },
	{ 0x7e80, 0x7e80, mario_gfxbank_w },
	{ 0x7e83, 0x7e83, mario_palettebank_w },
	{ 0x7e84, 0x7e84, interrupt_enable_w },
	{ 0x7f00, 0x7f00, mario_sh_w },	/* death */
	{ 0x7f01, 0x7f01, mario_sh_getcoin_w },
	{ 0x7f03, 0x7f03, mario_sh_crab_w },
	{ 0x7f04, 0x7f04, mario_sh_turtle_w },
	{ 0x7f05, 0x7f05, mario_sh_fly_w },
	{ 0x7f00, 0x7f07, mario_sh3_w }, /* Misc discrete samples */
	{ 0x7e00, 0x7e00, mario_sh_tuneselect_w },
	{ 0x7000, 0x73ff, MWA_NOP },	/* ??? */
/*	{ 0x7e85, 0x7e85, MWA_RAM },	 // Sets alternative 1 and 0 /*/
	{ 0xf000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_WRITE_START( masao_writemem )
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x68ff, MWA_RAM },
	{ 0x6a80, 0x6fff, MWA_RAM },
	{ 0x6900, 0x6a7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x7400, 0x77ff, mario_videoram_w, &videoram },
	{ 0x7d00, 0x7d00, mario_scroll_w },
	{ 0x7e00, 0x7e00, soundlatch_w },
	{ 0x7e80, 0x7e80, mario_gfxbank_w },
	{ 0x7e83, 0x7e83, mario_palettebank_w },
	{ 0x7e84, 0x7e84, interrupt_enable_w },
	{ 0x7000, 0x73ff, MWA_NOP },	/* ??? */
	{ 0x7f00, 0x7f00, masao_sh_irqtrigger_w },
	{ 0xf000, 0xffff, MWA_ROM },
MEMORY_END

static PORT_WRITE_START( mario_writeport )
	{ 0x00,   0x00,   IOWP_NOP },  /* unknown... is this a trigger? */
PORT_END

static MEMORY_READ_START( readmem_sound )
	{ 0x0000, 0x0fff, MRA_ROM },
MEMORY_END
static MEMORY_WRITE_START( writemem_sound )
	{ 0x0000, 0x0fff, MWA_ROM },
MEMORY_END
static PORT_READ_START( readport_sound )
	{ 0x00,     0xff,     mario_sh_tune_r },
	{ I8039_p1, I8039_p1, mario_sh_p1_r },
	{ I8039_p2, I8039_p2, mario_sh_p2_r },
	{ I8039_t0, I8039_t0, mario_sh_t0_r },
	{ I8039_t1, I8039_t1, mario_sh_t1_r },
PORT_END
static PORT_WRITE_START( writeport_sound )
	{ 0x00,     0xff,     mario_sh_sound_w },
	{ I8039_p1, I8039_p1, mario_sh_p1_w },
	{ I8039_p2, I8039_p2, mario_sh_p2_w },
PORT_END

INPUT_PORTS_START( mario )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20k" )
	PORT_DIPSETTING(    0x10, "30k" )
	PORT_DIPSETTING(    0x20, "40k" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x80, "Meduim" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( marioe )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )  /* doesn't work in game, but does in service mode */	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x10, "30000" )
	PORT_DIPSETTING(    0x20, "40000" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x80, "HMeduim" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( mariof )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20k 40k 20k+" )
	PORT_DIPSETTING(    0x10, "30k 50k 20k+" )
	PORT_DIPSETTING(    0x20, "40k 60k 20k+" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x80, "HMeduim" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END



INPUT_PORTS_START( mariojp )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )     /* doesn't work in game, but does in service mode */

	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20k 50k 30k+" )
	PORT_DIPSETTING(    0x40, "30k 60k 30k+" )
	PORT_DIPSETTING(    0x80, "40k 70k 30k+" )
	PORT_DIPSETTING(    0xC0, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x80, "HMeduim" )
	PORT_DIPSETTING(    0xc0, "Hardest" )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 512*8*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};


static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	3,	/* 3 bits per pixel */
	{ 2*256*16*16, 256*16*16, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,		/* the two halves of the sprite are separated */
			256*16*8+0, 256*16*8+1, 256*16*8+2, 256*16*8+3, 256*16*8+4, 256*16*8+5, 256*16*8+6, 256*16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,      0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 0, 32 },
	{ -1 } /* end of array */
};



static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};

static const char *mario_sample_names[] =
{
	"*mario",

	/* 7f01 - 7f07 sounds */
	"ice.wav",    /* 0x02 ice appears (formerly effect0.wav) */
	"coin.wav",   /* 0x06 coin appears (formerly effect1.wav) */
	"skid.wav",   /* 0x07 skid */

	/* 7c00 */
	"run.wav",        /* 03, 02, 01 - 0x1b */

	/* 7c80 */
	"luigirun.wav",   /* 03, 02, 01 - 0x1c */

    0	/* end of array */
};

static struct Samplesinterface samples_interface =
{
	3,	/* 3 channels */
	25,	/* volume */
	mario_sample_names
};

static struct AY8910interface ay8910_interface =
{
	1,      /* 1 chip */
	14318000/6,	/* ? */
	{ 50 },
	{ soundlatch_r },
	{ 0 },
	{ 0 },
	{ 0 }
};

static MEMORY_READ_START( masao_sound_readmem )
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x4000, 0x4000, AY8910_read_port_0_r },
MEMORY_END

static MEMORY_WRITE_START( masao_sound_writemem )
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x6000, 0x6000, AY8910_control_port_0_w },
	{ 0x4000, 0x4000, AY8910_write_port_0_w },
MEMORY_END


static MACHINE_DRIVER_START( mario )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)	/* 3.072 MHz (?) */
	MDRV_CPU_MEMORY(readmem,writemem)
	MDRV_CPU_PORTS(0,mario_writeport)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(I8039, 730000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)         /* 730 kHz */
	MDRV_CPU_MEMORY(readmem_sound,writemem_sound)
	MDRV_CPU_PORTS(readport_sound,writeport_sound)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(16*4+32*8)

	MDRV_PALETTE_INIT(mario)
	MDRV_VIDEO_START(mario)
	MDRV_VIDEO_UPDATE(mario)

	/* sound hardware */
	MDRV_SOUND_ADD(DAC, dac_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( masao )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 4000000)        /* 4.000 MHz (?) */
	MDRV_CPU_MEMORY(readmem,masao_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(Z80,24576000/16)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* ???? */
	MDRV_CPU_MEMORY(masao_sound_readmem,masao_sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(512)
	MDRV_COLORTABLE_LENGTH(512)

	MDRV_PALETTE_INIT(mario)
	MDRV_VIDEO_START(mario)
	MDRV_VIDEO_UPDATE(mario)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mario )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "tma1-c-7f_g.7f",     0x0000, 0x2000, CRC(c0c6e014) SHA1(36a04f9ca1c2a583477cb8a6f2ef94e044e08296) ) /* Unknown revision */
	ROM_LOAD( "tma1-c-7e_g.7e",     0x2000, 0x2000, CRC(116b3856) SHA1(e372f846d0e5a2b9b47ebd0330293fcc8a12363f) )
	ROM_LOAD( "tma1-c-7d_g.7d",     0x4000, 0x2000, CRC(dcceb6c1) SHA1(b19804e69ce2c98cf276c6055c3a250316b96b45) )
	ROM_LOAD( "tma1-c-7c_g.7c",     0xf000, 0x1000, CRC(4a63d96b) SHA1(b09060b2c84ab77cc540a27b8f932cb60ec8d442) )

	ROM_REGION( 0x1000, REGION_CPU2, 0 )	/* sound */
	ROM_LOAD( "tma1-c-6k_e.6k",   0x0000, 0x1000, CRC(06b9ff85) SHA1(111a29bcb9cda0d935675fa26eca6b099a88427f) )


	ROM_REGION( 0x2000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1-v-3f.3f",     0x0000, 0x1000, CRC(28b0c42c) SHA1(46749568aff88a28c3b6a1ac423abd1b90742a4d) )
	ROM_LOAD( "tma1-v-3j.3j",     0x1000, 0x1000, CRC(0c8cc04d) SHA1(15fae47d701dc1ef15c943cee6aa991776ecffdf) )

	ROM_REGION( 0x6000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1-v-7m.7m",     0x0000, 0x1000, CRC(22b7372e) SHA1(4a1c1e239cb6d483e76f50d7a3b941025963c6a3) )
	ROM_LOAD( "tma1-v-7n.7n",     0x1000, 0x1000, CRC(4f3a1f47) SHA1(0747d693b9482f6dd28b0bc484fd1d3e29d35654) )
	ROM_LOAD( "tma1-v-7p.7p",     0x2000, 0x1000, CRC(56be6ccd) SHA1(15a6e16c189d45f72761ebcbe9db5001bdecd659) )
	ROM_LOAD( "tma1-v-7s.7s",     0x3000, 0x1000, CRC(56f1d613) SHA1(9af6844dbaa3615433d0595e9e85e72493e31a54) )
	ROM_LOAD( "tma1-v-7t.7t",     0x4000, 0x1000, CRC(641f0008) SHA1(589fe108c7c11278fd897f2ded8f0498bc149cfd) )
	ROM_LOAD( "tma1-v-7u.7u",     0x5000, 0x1000, CRC(7baf5309) SHA1(d9194ff7b89a18273d37b47228fc7fb7e2a0ed1f) )

	ROM_REGION( 0x0200, REGION_PROMS, 0 )
	ROM_LOAD( "tma1-c-4p.4p",     0x0000, 0x0200, CRC(afc9bd41) SHA1(90b739c4c7f24a88b6ac5ca29b06c032906a2801) )
ROM_END

ROM_START( mariof )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "tma1-c-7f_f.7f",  0x0000, 0x2000, CRC(c0c6e014) SHA1(36a04f9ca1c2a583477cb8a6f2ef94e044e08296) )
	ROM_LOAD( "tma1-c-7e_f.7e",  0x2000, 0x2000, CRC(94fb60d6) SHA1(e74d74aa27f87a164bdd453ab0076efeeb7d4ea3) )
	ROM_LOAD( "tma1-c-7d_f.7d",  0x4000, 0x2000, CRC(dcceb6c1) SHA1(b19804e69ce2c98cf276c6055c3a250316b96b45) )
	ROM_LOAD( "tma1-c-7c_f.7c",  0xf000, 0x1000, CRC(4a63d96b) SHA1(b09060b2c84ab77cc540a27b8f932cb60ec8d442) )

	ROM_REGION( 0x1000, REGION_CPU2, 0 )	/* sound */
	ROM_LOAD( "tma1-c-6k_e.6k",   0x0000, 0x1000, CRC(06b9ff85) SHA1(111a29bcb9cda0d935675fa26eca6b099a88427f) )

	ROM_REGION( 0x2000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1-v-3f.3f",     0x0000, 0x1000, CRC(28b0c42c) SHA1(46749568aff88a28c3b6a1ac423abd1b90742a4d) )
	ROM_LOAD( "tma1-v-3j.3j",     0x1000, 0x1000, CRC(0c8cc04d) SHA1(15fae47d701dc1ef15c943cee6aa991776ecffdf) )

	ROM_REGION( 0x6000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1-v-7m.7m",     0x0000, 0x1000, CRC(22b7372e) SHA1(4a1c1e239cb6d483e76f50d7a3b941025963c6a3) )
	ROM_LOAD( "tma1-v-7n.7n",     0x1000, 0x1000, CRC(4f3a1f47) SHA1(0747d693b9482f6dd28b0bc484fd1d3e29d35654) )
	ROM_LOAD( "tma1-v-7p.7p",     0x2000, 0x1000, CRC(56be6ccd) SHA1(15a6e16c189d45f72761ebcbe9db5001bdecd659) )
	ROM_LOAD( "tma1-v-7s.7s",     0x3000, 0x1000, CRC(56f1d613) SHA1(9af6844dbaa3615433d0595e9e85e72493e31a54) )
	ROM_LOAD( "tma1-v-7t.7t",     0x4000, 0x1000, CRC(641f0008) SHA1(589fe108c7c11278fd897f2ded8f0498bc149cfd) )
	ROM_LOAD( "tma1-v-7u.7u",     0x5000, 0x1000, CRC(7baf5309) SHA1(d9194ff7b89a18273d37b47228fc7fb7e2a0ed1f) )

	ROM_REGION( 0x0200, REGION_PROMS, 0 )
	ROM_LOAD( "tma1-c-4p_1.4p", 0x0000, 0x0200, CRC(8187d286) SHA1(8a6d8e622599f1aacaeb10f7b1a39a23c8a840a0) ) /* BPROM was a MB7124E read as 82S147 */

ROM_END

ROM_START( marioe )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "tma1-c-7f_e-1.7f",     0x0000, 0x2000, CRC(c0c6e014) SHA1(36a04f9ca1c2a583477cb8a6f2ef94e044e08296) )
	ROM_LOAD( "tma1-c-7e_e-3.7e",     0x2000, 0x2000, CRC(b09ab857) SHA1(35b91cd1c4c3dd2d543a1ea8ff7b951715727792) )
	ROM_LOAD( "tma1-c-7d_e-1.7d",     0x4000, 0x2000, CRC(dcceb6c1) SHA1(b19804e69ce2c98cf276c6055c3a250316b96b45) )
	ROM_LOAD( "tma1-c-7c_e-3.7c",     0xf000, 0x1000, CRC(0d31bd1c) SHA1(a2e238470ba2ea3c81225fec687f61f047c68c59) )

	ROM_REGION( 0x1000, REGION_CPU2, 0 )	/* sound */
	/* internal rom */
	ROM_LOAD( "tma1-c-6k_e.6k",   0x0000, 0x1000, CRC(06b9ff85) SHA1(111a29bcb9cda0d935675fa26eca6b099a88427f) )
	/* second half always read */
	ROM_CONTINUE(             0x0800, 0x0800)

	ROM_REGION( 0x2000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1-v-3f.3f",     0x0000, 0x1000, CRC(28b0c42c) SHA1(46749568aff88a28c3b6a1ac423abd1b90742a4d) )
	ROM_LOAD( "tma1-v-3j.3j",     0x1000, 0x1000, CRC(0c8cc04d) SHA1(15fae47d701dc1ef15c943cee6aa991776ecffdf) )

	ROM_REGION( 0x6000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1-v.7m",     0x0000, 0x1000, CRC(d01c0e2c) SHA1(cd6cc9d69c36db15543601f5da2abf109cde43c9) )
	ROM_LOAD( "tma1-v-7n.7n",  0x1000, 0x1000, CRC(4f3a1f47) SHA1(0747d693b9482f6dd28b0bc484fd1d3e29d35654) )
	ROM_LOAD( "tma1-v-7p.7p",  0x2000, 0x1000, CRC(56be6ccd) SHA1(15a6e16c189d45f72761ebcbe9db5001bdecd659) )
	ROM_LOAD( "tma1-v.7s",     0x3000, 0x1000, CRC(ff856e6f) SHA1(2bc9ff18bb1842e8de2bc61ac828f1b175290bed) )
	ROM_LOAD( "tma1-v-7t.7t",  0x4000, 0x1000, CRC(641f0008) SHA1(589fe108c7c11278fd897f2ded8f0498bc149cfd) )
	ROM_LOAD( "tma1-v.7u",     0x5000, 0x1000, CRC(d2dbeb75) SHA1(676cf3e15252cd0d9e926ca15c3aa0caa39be269) )

	ROM_REGION( 0x0200, REGION_PROMS, 0 )
	ROM_LOAD( "tma1-c-4p_1.4p",     0x0000, 0x0200, CRC(8187d286) SHA1(8a6d8e622599f1aacaeb10f7b1a39a23c8a840a0) ) /* BPROM was a MB7124E read as 82S147 */

ROM_END

ROM_START( marioj )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "tma1c-a1.7f",  0x0000, 0x2000, CRC(b64b6330) SHA1(f7084251ac325bbfa3fb804da16a50622e1fd213) )
	ROM_LOAD( "tma1c-a2.7e",  0x2000, 0x2000, CRC(290c4977) SHA1(5af266be0ddc883c6548c90e4a9084024a1e91a0) )
	ROM_LOAD( "tma1c-a1.7d",  0x4000, 0x2000, CRC(f8575f31) SHA1(710d0e72fcfce700ed2a22fb9c7c392cc76b250b) )
	ROM_LOAD( "tma1c-a2.7c",  0xf000, 0x1000, CRC(a3c11e9e) SHA1(d0612b0f8c2ea4e798f551922a04a324f4ed5f3d) )

	ROM_REGION( 0x1000, REGION_CPU2, 0 )	/* sound */
	ROM_LOAD( "tma1c-a.6k",   0x0000, 0x1000, CRC(06b9ff85) SHA1(111a29bcb9cda0d935675fa26eca6b099a88427f) )

	ROM_REGION( 0x2000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1v-a.3f",   0x0000, 0x1000, CRC(adf49ee0) SHA1(11fc2cd197bfe3ecb6af55c3c7a326c94988d2bd) )
	ROM_LOAD( "tma1v-a.3j",   0x1000, 0x1000, CRC(a5318f2d) SHA1(e42f5e51804195c64a56addb18b7ad12c57bb09a) )

	ROM_REGION( 0x6000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1v-a.7m",   0x0000, 0x1000, CRC(186762f8) SHA1(711fdd37392656bdd5027e020d51d083ccd7c407) )
	ROM_LOAD( "tma1v-a.7n",   0x1000, 0x1000, CRC(e0e08bba) SHA1(315eba2c10d426c9c0bb4e36987bf8ebed7df9a0) )
	ROM_LOAD( "tma1v-a.7p",   0x2000, 0x1000, CRC(7b27c8c1) SHA1(3fb2613ce19e353fbcc77b6817927794fb35810f) )
	ROM_LOAD( "tma1v-a.7s",   0x3000, 0x1000, CRC(912ba80a) SHA1(351fb5b160216eb10e281815d05a7165ca0e5909) )
	ROM_LOAD( "tma1v-a.7t",   0x4000, 0x1000, CRC(5cbb92a5) SHA1(a78a378e6d3060143dc456e9c33a5068da648331) )
	ROM_LOAD( "tma1v-a.7u",   0x5000, 0x1000, CRC(13afb9ed) SHA1(b29dcd91cf5e639ee50b734afc7a3afce79634df) )

	ROM_REGION( 0x0200, REGION_PROMS, 0 )
	ROM_LOAD( "tma1-c-4p.4p",     0x0000, 0x0200, CRC(afc9bd41) SHA1(90b739c4c7f24a88b6ac5ca29b06c032906a2801) )

ROM_END

ROM_START( masao )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code */
	ROM_LOAD( "masao-4.rom",  0x0000, 0x2000, CRC(07a75745) SHA1(acc760242a8862d177e3cff90aa32c4f3dac4e65) )
	ROM_LOAD( "masao-3.rom",  0x2000, 0x2000, CRC(55c629b6) SHA1(1f5b5699821871aadacc511663cb4bd4e357e215) )
	ROM_LOAD( "masao-2.rom",  0x4000, 0x2000, CRC(42e85240) SHA1(bc8cdf867b743c5ee58fcacb63a44f826c8f8c1a) )
	ROM_LOAD( "masao-1.rom",  0xf000, 0x1000, CRC(b2817af9) SHA1(95e83752e544671a68df2107fae1010b187f04a6) )

	ROM_REGION( 0x1000, REGION_CPU2, 0 )	/* sound */
	ROM_LOAD( "masao-5.rom",  0x0000, 0x1000, CRC(bd437198) SHA1(ebae88461984afc97bbc103fc6d95bc3c1865eec) )

	ROM_REGION( 0x2000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "masao-6.rom",  0x0000, 0x1000, CRC(1c9e0be2) SHA1(b4a650412dad90c6f6d79e93cde49055703b7f3e) )
	ROM_LOAD( "masao-7.rom",  0x1000, 0x1000, CRC(747c1349) SHA1(54674f78edf86953b7d500b66393483d1a5ce8ab) )

	ROM_REGION( 0x6000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "tma1v-a.7m",   0x0000, 0x1000, CRC(186762f8) SHA1(711fdd37392656bdd5027e020d51d083ccd7c407) )
	ROM_LOAD( "masao-9.rom",  0x1000, 0x1000, CRC(50be3918) SHA1(73e22eee67a03732ff57e523f900f20c6aee0491) )
	ROM_LOAD( "mario.7p",     0x2000, 0x1000, CRC(56be6ccd) SHA1(15a6e16c189d45f72761ebcbe9db5001bdecd659) )
	ROM_LOAD( "tma1v-a.7s",   0x3000, 0x1000, CRC(912ba80a) SHA1(351fb5b160216eb10e281815d05a7165ca0e5909) )
	ROM_LOAD( "tma1v-a.7t",   0x4000, 0x1000, CRC(5cbb92a5) SHA1(a78a378e6d3060143dc456e9c33a5068da648331) )
	ROM_LOAD( "tma1v-a.7u",   0x5000, 0x1000, CRC(13afb9ed) SHA1(b29dcd91cf5e639ee50b734afc7a3afce79634df) )

	ROM_REGION( 0x0200, REGION_PROMS, 0 )
	ROM_LOAD( "tma1-c-4p.4p",     0x0000, 0x0200, CRC(afc9bd41) SHA1(90b739c4c7f24a88b6ac5ca29b06c032906a2801) )
ROM_END


/* todo check dipwitches for all set */
GAME( 1983, mario,   0,     mario, mario,   0, ROT180, "Nintendo of America", "Mario Bros. (US, Revision G)" )
GAME( 1983, marioe,  mario, mario, marioe,  0, ROT180, "Nintendo of America", "Mario Bros. (US, Revision E)" )
GAME( 1983, mariof,  mario, mario, mariof,  0, ROT180, "Nintendo of America", "Mario Bros. (US, Revision F)" )
GAME( 1983, marioj,  mario, mario, mariojp, 0, ROT180, "Nintendo", "Mario Bros. (Japan, Revision C)" )
GAME( 1983, masao,   mario, masao, mario,   0, ROT180, "bootleg", "Masao" )


