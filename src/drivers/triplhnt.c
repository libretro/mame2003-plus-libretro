/***************************************************************************

Atari Triple Hunt Driver

  Calibrate controls in service mode the first time you run this game.

***************************************************************************/

#include "driver.h"

extern VIDEO_START( triplhnt );
extern VIDEO_UPDATE( triplhnt );

extern UINT8* triplhnt_playfield_ram;
extern UINT8* triplhnt_vpos_ram;
extern UINT8* triplhnt_hpos_ram;
extern UINT8* triplhnt_code_ram;
extern UINT8* triplhnt_orga_ram;

extern int triplhnt_sprite_zoom;
extern int triplhnt_sprite_bank;

static UINT8 triplhnt_cmos[16];
static UINT8 triplhnt_da_latch;
static UINT8 triplhnt_misc_flags;
static UINT8 triplhnt_cmos_latch;
static UINT8 triplhnt_hit_code;


static DRIVER_INIT( triplhnt )
{
	generic_nvram = triplhnt_cmos;
	generic_nvram_size = sizeof triplhnt_cmos;
}


void triplhnt_hit_callback(int code)
{
	triplhnt_hit_code = code;

	cpu_set_irq_line(0, 0, HOLD_LINE);
}


static void triplhnt_update_misc(int offset)
{
	UINT8 bit = offset >> 1;

	/* BIT0 => UNUSED      */
	/* BIT1 => LAMP        */
	/* BIT2 => SCREECH     */
	/* BIT3 => LOCKOUT     */
	/* BIT4 => SPRITE ZOOM */
	/* BIT5 => CMOS WRITE  */
	/* BIT6 => TAPE CTRL   */
	/* BIT7 => SPRITE BANK */

	if (offset & 1)
	{
		triplhnt_misc_flags |= 1 << bit;

		if (bit == 5)
		{
			triplhnt_cmos[triplhnt_cmos_latch] = triplhnt_da_latch;
		}
	}
	else
	{
		triplhnt_misc_flags &= ~(1 << bit);
	}

	triplhnt_sprite_zoom = (triplhnt_misc_flags >> 4) & 1;
	triplhnt_sprite_bank = (triplhnt_misc_flags >> 7) & 1;

	set_led_status(0, triplhnt_misc_flags & 0x02);

	coin_lockout_w(0, !(triplhnt_misc_flags & 0x08));
	coin_lockout_w(1, !(triplhnt_misc_flags & 0x08));

	discrete_sound_w(3, (triplhnt_misc_flags >> 2) & 1);	// screech
	discrete_sound_w(4, (~triplhnt_misc_flags >> 1) & 1);	// Lamp is used to reset noise
	discrete_sound_w(1, (~triplhnt_misc_flags >> 7) & 1);	// bear
}


WRITE_HANDLER( triplhnt_misc_w )
{
	triplhnt_update_misc(offset);
}


WRITE_HANDLER( triplhnt_zeropage_w )
{
	memory_region(REGION_CPU1)[offset & 0xff] = data;
}


READ_HANDLER( triplhnt_zeropage_r )
{
	return memory_region(REGION_CPU1)[offset & 0xff];
}


READ_HANDLER( triplhnt_cmos_r )
{
	triplhnt_cmos_latch = offset;

	return triplhnt_cmos[triplhnt_cmos_latch] ^ 15;
}


READ_HANDLER( triplhnt_input_port_4_r )
{
	watchdog_reset_w(0, 0);

	return readinputport(4);
}


READ_HANDLER( triplhnt_misc_r )
{
	triplhnt_update_misc(offset);

	return readinputport(7) | triplhnt_hit_code;
}


READ_HANDLER( triplhnt_da_latch_r )
{
	int cross_x = readinputport(8);
	int cross_y = readinputport(9);

	triplhnt_da_latch = offset;

	/* the following is a slight simplification */

	return (offset & 1) ? cross_x : cross_y;
}


static MEMORY_READ_START( triplhnt_readmem )
	{ 0x0000, 0x00ff, MRA_RAM },
	{ 0x0100, 0x03ff, triplhnt_zeropage_r },
	{ 0x0c00, 0x0c00, input_port_0_r },
	{ 0x0c08, 0x0c08, input_port_1_r },
	{ 0x0c09, 0x0c09, input_port_2_r },
	{ 0x0c0a, 0x0c0a, input_port_3_r },
	{ 0x0c0b, 0x0c0b, triplhnt_input_port_4_r },
	{ 0x0c10, 0x0c1f, triplhnt_da_latch_r },
	{ 0x0c20, 0x0c2f, triplhnt_cmos_r },
	{ 0x0c30, 0x0c3f, triplhnt_misc_r },
	{ 0x0c40, 0x0c40, input_port_5_r },
	{ 0x0c48, 0x0c48, input_port_6_r },
	{ 0x7000, 0x7fff, MRA_ROM }, /* program */
	{ 0xf800, 0xffff, MRA_ROM }, /* program mirror */
MEMORY_END

static MEMORY_WRITE_START( triplhnt_writemem )
	{ 0x0000, 0x00ff, MWA_RAM },
	{ 0x0100, 0x03ff, triplhnt_zeropage_w },
	{ 0x0400, 0x04ff, MWA_RAM, &triplhnt_playfield_ram },
	{ 0x0800, 0x080f, MWA_RAM, &triplhnt_vpos_ram },
	{ 0x0810, 0x081f, MWA_RAM, &triplhnt_hpos_ram },
	{ 0x0820, 0x082f, MWA_RAM, &triplhnt_orga_ram },
	{ 0x0830, 0x083f, MWA_RAM, &triplhnt_code_ram },
	{ 0x0c30, 0x0c3f, triplhnt_misc_w },
	{ 0x7000, 0x7fff, MWA_ROM }, /* program */
	{ 0xf800, 0xffff, MWA_ROM }, /* program mirror */
MEMORY_END


INPUT_PORTS_START( triplhnt )
	PORT_START /* 0C00 */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START /* 0C08 */
	PORT_DIPNAME( 0xc0, 0x00, "Play Time" )
	PORT_DIPSETTING( 0x00, "32 seconds / 16 raccoons" )
	PORT_DIPSETTING( 0x40, "64 seconds / 32 raccoons" )
	PORT_DIPSETTING( 0x80, "96 seconds / 48 raccoons" )
	PORT_DIPSETTING( 0xc0, "128 seconds / 64 raccoons" )

	PORT_START /* 0C09 */
	PORT_DIPNAME( 0xc0, 0x40, "Game Select" )
	PORT_DIPSETTING( 0x00, "Hit the Bear" )
	PORT_DIPSETTING( 0x40, "Witch Hunt" )
	PORT_DIPSETTING( 0xc0, "Raccoon Hunt" )

	PORT_START /* 0C0A */
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coinage ))
	PORT_DIPSETTING( 0x40, DEF_STR( 2C_1C ))
	PORT_DIPSETTING( 0x00, DEF_STR( 1C_1C ))
	PORT_DIPSETTING( 0x80, DEF_STR( 1C_2C ))

	PORT_START /* 0C0B */
	PORT_DIPNAME( 0x80, 0x00, "Extended Play" )
	PORT_DIPSETTING( 0x80, DEF_STR( Off ))
	PORT_DIPSETTING( 0x00, DEF_STR( On ))

	PORT_START /* 0C40 */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN2 )

	PORT_START /* 0C48 */
/* default to service enabled to make users calibrate gun*/
/*	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )*/
	PORT_BITX(    0x40, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X, 25, 15, 0x00, 0xff)

	PORT_START
	PORT_ANALOG( 0xff, 0x78, IPT_LIGHTGUN_Y, 25, 15, 0x00, 0xef)

	PORT_START		/* 10 */
	PORT_ADJUSTER( 50, "Bear Roar Frequency" )
INPUT_PORTS_END


static struct GfxLayout triplhnt_small_sprite_layout =
{
	32, 32,   /* width, height */
	16,       /* total         */
	2,        /* planes        */
	          /* plane offsets */
	{ 0x0000, 0x4000 },
	{
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
	},
	{
		0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0,
		0x100, 0x120, 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0,
		0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
		0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0, 0x3E0
	},
	0x400     /* increment */
};


static struct GfxLayout triplhnt_large_sprite_layout =
{
	64, 64,   /* width, height */
	16,       /* total         */
	2,        /* planes        */
	          /* plane offsets */
	{ 0x0000, 0x4000 },
	{
		0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03,
		0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07, 0x07,
		0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B,
		0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0E, 0x0F, 0x0F,
		0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13,
		0x14, 0x14, 0x15, 0x15, 0x16, 0x16, 0x17, 0x17,
		0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
		0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F
	},
	{
		0x000, 0x000, 0x020, 0x020, 0x040, 0x040, 0x060, 0x060,
		0x080, 0x080, 0x0A0, 0x0A0, 0x0C0, 0x0C0, 0x0E0, 0x0E0,
		0x100, 0x100, 0x120, 0x120, 0x140, 0x140, 0x160, 0x160,
		0x180, 0x180, 0x1A0, 0x1A0, 0x1C0, 0x1C0, 0x1E0, 0x1E0,
		0x200, 0x200, 0x220, 0x220, 0x240, 0x240, 0x260, 0x260,
		0x280, 0x280, 0x2A0, 0x2A0, 0x2C0, 0x2C0, 0x2E0, 0x2E0,
		0x300, 0x300, 0x320, 0x320, 0x340, 0x340, 0x360, 0x360,
		0x380, 0x380, 0x3A0, 0x3A0, 0x3C0, 0x3C0, 0x3E0, 0x3E0
	},
	0x400     /* increment */
};


static struct GfxLayout triplhnt_tile_layout =
{
	16, 16,   /* width, height */
	64,       /* total         */
	1,        /* planes        */
	{ 0 },    /* plane offsets */
	{
		0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7
	},
	{
		0x00, 0x00, 0x08, 0x08, 0x10, 0x10, 0x18, 0x18,
		0x20, 0x20, 0x28, 0x28, 0x30, 0x30, 0x38, 0x38
	},
	0x40      /* increment */
};


static struct GfxDecodeInfo triplhnt_gfx_decode_info[] =
{
	{ REGION_GFX1, 0, &triplhnt_small_sprite_layout, 0, 1 },
	{ REGION_GFX1, 0, &triplhnt_large_sprite_layout, 0, 1 },
	{ REGION_GFX2, 0, &triplhnt_tile_layout, 4, 2 },
	{ -1 } /* end of array */
};


static PALETTE_INIT( triplhnt )
{
	palette_set_color(0, 0xAF, 0xAF, 0xAF);  /* sprites */
	palette_set_color(1, 0x00, 0x00, 0x00);
	palette_set_color(2, 0xFF, 0xFF, 0xFF);
	palette_set_color(3, 0x50, 0x50, 0x50);
	palette_set_color(4, 0x00, 0x00, 0x00);  /* tiles */
	palette_set_color(5, 0x3F, 0x3F, 0x3F);
	palette_set_color(6, 0x00, 0x00, 0x00);
	palette_set_color(7, 0x3F, 0x3F, 0x3F);
}


/************************************************************************/
/* triplhnt Sound System Analog emulation                               */
/* Feb 2004, Derrick Renaud                                             */
/************************************************************************/
const struct discrete_lfsr_desc triplhnt_lfsr={
	16,			/* Bit Length */
	0,			/* Reset Value */
	0,			/* Use Bit 0 as XOR input 0 */
	14,			/* Use Bit 14 as XOR input 1 */
	DISC_LFSR_XNOR,		/* Feedback stage1 is XNOR */
	DISC_LFSR_OR,		/* Feedback stage2 is just stage 1 output OR with external feed */
	DISC_LFSR_REPLACE,	/* Feedback stage3 replaces the shifted register contents */
	0x000001,		/* Everything is shifted into the first bit only */
	0,			/* Output not inverted */
	15			/* Output bit */
};

const struct discrete_dac_r1_ladder triplhnt_bear_roar_v_dac =
{
	4,		// size of ladder
	{1000000, 470000, 220000, 2200000, 0,0,0,0},	// R47, R50, R48, R51
	5,		// vBias
	68000,		// R44
	0,		// no rGnd
	0		// no smoothing cap
};

const struct discrete_dac_r1_ladder triplhnt_bear_roar_out_dac =
{
	3,		// size of ladder
	{100000, 33000, 100000, 0,0,0,0,0},	// R56, R58, R57
	0,		// no vBias
	0,		// no rBias
	0,		// no rGnd
	0		// no smoothing cap
};

const struct discrete_dac_r1_ladder triplhnt_shot_dac =
{
	4,		// size of ladder
	{8200, 3900, 2200, 1000, 0,0,0,0},	// R53, R54, R55, R52
	0,		// no vBias
	0,		// no rBias
	0,		// no rGnd
	0		// no smoothing cap
};

const struct discrete_555_cc_desc triplhnt_bear_roar_vco =
{
	DISC_555_OUT_DC | DISC_555_OUT_SQW,
	5,		// B+ voltage of 555
	5.0 - 1.7,	// High output voltage of 555 (Usually v555 - 1.7)
	5.0 * 2 / 3,	// threshold
	5.0 /3,		// trigger
	5,		// B+ voltage of the Constant Current source
	0.7		// Q2 junction voltage
};

const struct discrete_schmitt_osc_desc triplhnt_screech_osc =
{
	2200,	// R84
	330,	// R85
	1.e-6,	// C59
	1.7,	// Rise Threshold of 7414
	0.9,	// Fall Threshold of 7414
	3.4,	// Output high voltage of 7414
	1	// invert output using 7400 gate E7
};

const struct discrete_mixer_desc triplhnt_mixer =
{
	DISC_MIXER_IS_RESISTOR,
	3,					// 3 inputs
	{3300 + 19879.5, 47000, 27000 + 545.6, 0,0,0,0,0},	// R59 + (R56||R57||R58), R60 + (R52||R53||R54||R55), R61
	{0,0,0,0,0,0,0,0},			// No variable resistor nodes
	{0, 0, 0, 0, 0,0,0,0},			// No caps
	0,					// No rI
	1000,					// R78
	1e-7,					// C72
	1e-7,					// C44
	0,					// vBias not used for resistor network
	150000
};

/* Nodes - Inputs */
#define TRIPLHNT_BEAR_ROAR_DATA	NODE_01
#define TRIPLHNT_BEAR_EN	NODE_02
#define TRIPLHNT_SHOT_DATA	NODE_03
#define TRIPLHNT_SCREECH_EN	NODE_04
#define TRIPLHNT_LAMP_EN	NODE_05
/* Nodes - Sounds */
#define TRIPLHNT_NOISE		NODE_10
#define TRIPLHNT_BEAR_ROAR_SND	NODE_11
#define TRIPLHNT_SHOT_SND	NODE_12
#define TRIPLHNT_SCREECH_SND	NODE_13
#define POOLSHRK_SCORE_SND	NODE_14

static DISCRETE_SOUND_START(triplhnt_sound_interface)
	/************************************************/
	/* Input register mapping for triplhnt          */
	/************************************************/
	/*              NODE                    ADDR  MASK     GAIN      OFFSET  INIT */
	DISCRETE_INPUT(TRIPLHNT_BEAR_ROAR_DATA, 0x00, 0x000f,                    15.0)
	DISCRETE_INPUT(TRIPLHNT_BEAR_EN,        0x01, 0x000f,                     1.0)
	DISCRETE_INPUT(TRIPLHNT_SHOT_DATA,      0x02, 0x000f,                     0.0)	// should init to 15 when hooked up
	DISCRETE_INPUT(TRIPLHNT_SCREECH_EN,     0x03, 0x000f,                     0.0)
	DISCRETE_INPUT(TRIPLHNT_LAMP_EN,        0x04, 0x000f,                     1.0)
	/************************************************/

	DISCRETE_LFSR_NOISE(TRIPLHNT_NOISE,			// Output A7 pin 13
				TRIPLHNT_LAMP_EN, TRIPLHNT_LAMP_EN,	// From gate A8 pin 10
				12096000.0/2/256,		// 256H signal
				3.4, 0, 3.4/2, &triplhnt_lfsr)	// TTL voltage level

	/************************************************/
	/* Bear Roar is a VCO with noise mixed in.      */
	/************************************************/
	DISCRETE_ADJUSTMENT(NODE_20, 1,
				10000,	// R86 + R88 @ 0
				260000,	// R86 + R88 @ max
				DISC_LOGADJ, 10)
	DISCRETE_DAC_R1(NODE_21, 1,			// base of Q2
			TRIPLHNT_BEAR_ROAR_DATA,	// IC B10, Q0-Q3
			3.4,				// TTL ON level
			&triplhnt_bear_roar_v_dac)
	DISCRETE_555_CC(NODE_22, 1,	// IC C11 pin 3, always enabled
			NODE_21,	// vIn
			NODE_20,	// current adjust
			1.e-8,		// C58
			0, 390000, 0,	// no rBias, R87, no rDis
			&triplhnt_bear_roar_vco)
	DISCRETE_COUNTER(NODE_23, 1, TRIPLHNT_BEAR_EN,	// IC B6, QB-QD
			NODE_22,			// from IC C11, pin 3
			5, 1, 0, 1)			// /6 counter on rising edge
	DISCRETE_TRANSFORM2(NODE_24, 1, NODE_23, 2, "01>")	// IC B6, pin 8
	DISCRETE_LOGIC_INVERT(NODE_25, 1, NODE_22)	// IC D9, pin 3
	DISCRETE_LOGIC_NAND(NODE_26, 1, NODE_25, TRIPLHNT_NOISE)	// IC D9, pin 11
	DISCRETE_LOGIC_XOR(NODE_27, 1, NODE_24, NODE_26)	// IC B8, pin 6
	DISCRETE_COUNTER(NODE_28, 1, TRIPLHNT_BEAR_EN,	// IC B6, pin 12
			NODE_27,	// from IC B8, pin 6
			1, 1, 0, 1)	// /2 counter on rising edge
	DISCRETE_TRANSFORM5(NODE_29, 1, NODE_24, NODE_28, NODE_26, 2, 4, "13*24*+0+")	// Mix the mess together in binary
	DISCRETE_DAC_R1(TRIPLHNT_BEAR_ROAR_SND, 1, NODE_29,
			3.4,				// TTL ON level
			&triplhnt_bear_roar_out_dac)

	/************************************************/
	/* Shot is just the noise amplitude modulated   */
	/* by an R1 DAC.                                */
	/************************************************/
	DISCRETE_SWITCH(NODE_40, 1,	// Gate A9, pins 6, 8, 11, 3
			TRIPLHNT_NOISE,	// noise enables the data which is then inverted
			1, TRIPLHNT_SHOT_DATA)	// the data has been previously inverted for ease of use
	DISCRETE_DAC_R1(TRIPLHNT_SHOT_SND, 1,
			NODE_40,
			3.4,			// TTL ON level
			&triplhnt_shot_dac)

	/************************************************/
	/* Screech is just the noise modulating a       */
	/* Schmitt VCO.                                 */
	/************************************************/
	DISCRETE_SCHMITT_OSCILLATOR(TRIPLHNT_SCREECH_SND, TRIPLHNT_SCREECH_EN, TRIPLHNT_NOISE, 3.4, &triplhnt_screech_osc)

	/************************************************/
	/* Final gain and ouput.                        */
	/************************************************/
	DISCRETE_MIXER3(NODE_90, 1, TRIPLHNT_BEAR_ROAR_SND, TRIPLHNT_SHOT_SND, TRIPLHNT_SCREECH_SND, &triplhnt_mixer)
	DISCRETE_OUTPUT(NODE_90, 100)
DISCRETE_SOUND_END


static MACHINE_DRIVER_START( triplhnt )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6800, 800000)
	MDRV_CPU_MEMORY(triplhnt_readmem, triplhnt_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold, 1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION((int) ((22. * 1000000) / (262. * 60) + 0.5))

	MDRV_NVRAM_HANDLER(generic_0fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 262)
	MDRV_VISIBLE_AREA(0, 255, 0, 239)
	MDRV_GFXDECODE(triplhnt_gfx_decode_info)
	MDRV_PALETTE_LENGTH(8)
	MDRV_PALETTE_INIT(triplhnt)
	MDRV_VIDEO_START(triplhnt)
	MDRV_VIDEO_UPDATE(triplhnt)

	/* sound hardware */
	MDRV_SOUND_ADD_TAG("discrete", DISCRETE, triplhnt_sound_interface)
MACHINE_DRIVER_END


ROM_START( triplhnt )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD_NIB_HIGH( "8404.f1", 0x7000, 0x400, CRC(abc8acd5) SHA1(bcef2abc5829829a01aa21776c3deb2e1bf1d4ac) )
	ROM_LOAD_NIB_LOW ( "8408.f2", 0x7000, 0x400, CRC(77fcdd3f) SHA1(ce0196abb8d6510aa9a5308f8efd6442e94272c4) )
	ROM_LOAD_NIB_HIGH( "8403.e1", 0x7400, 0x400, CRC(8d756fa1) SHA1(48a74f710b130d9af0c866483d6fc4ecce4a3ac5) )
	ROM_LOAD_NIB_LOW ( "8407.e2", 0x7400, 0x400, CRC(de268f4b) SHA1(937f97421ffb4f0f704402847892382ae8032b7c) )
	ROM_LOAD_NIB_HIGH( "8402.d1", 0x7800, 0x400, CRC(eb75c936) SHA1(48f9d4113a7ab8413a5aacd44b3506afc99d26ce) )
	ROM_LOAD_NIB_LOW ( "8406.d2", 0x7800, 0x400, CRC(e7ab1186) SHA1(7185fb837966bfb4aa70be3dd948d44f356b0452) )
	ROM_LOAD_NIB_HIGH( "8401.c1", 0x7C00, 0x400, CRC(7461b05e) SHA1(16573ae655c306a38ff0f29a3c3285d636907f38) )
	ROM_RELOAD(                   0xFC00, 0x400 )
	ROM_LOAD_NIB_LOW ( "8405.c2", 0x7C00, 0x400, CRC(ba370b97) SHA1(5d799ce6ae56c315ff0abedea7ad9204bacc266b) )
	ROM_RELOAD(                   0xFC00, 0x400 )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "8423.n1", 0x0000, 0x800, CRC(9937d0da) SHA1(abb906c2d9869b09be5172cc7639bb9cda38831b) )
	ROM_LOAD( "8422.r1", 0x0800, 0x800, CRC(803621dd) SHA1(ffbd7f87a86477e5eb94f12fc20a837128a02442) )

	ROM_REGION( 0x200, REGION_GFX2, ROMREGION_DISPOSE )   /* tiles */
	ROM_LOAD_NIB_HIGH( "8409.l3", 0x0000, 0x200, CRC(ec304172) SHA1(ccbf7e117fef7fa4288e3bf68f1a150b3a492ce6) )
	ROM_LOAD_NIB_LOW ( "8410.m3", 0x0000, 0x200, CRC(f75a1b08) SHA1(81b4733194462cd4cef7f4221ecb7abd1556b871) )
ROM_END


GAMEX( 1977, triplhnt, 0, triplhnt, triplhnt, triplhnt, 0, "Atari", "Triple Hunt", GAME_IMPERFECT_SOUND )
