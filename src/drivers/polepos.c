/***************************************************************************
  Pole Position memory map (preliminary)

driver by Ernesto Corvi, Juergen Buchmueller, Alex Pasadyn, Aaron Giles


Z80
----------------------------------------
0000-2fff (R) ROM
3000-37ff (R/W) Battery Backup RAM
4000-43ff (R/W) Motion Object memory
	(4380-43ff Vertical and Horizontal position)
4400-47ff (R/W) Motion Object memory
	(4780-47ff Vertical and Horizontal position)
4800-4bff (R/W) Road Memory
	(4800-49ff Character)
	(4b80-4bff Horizontal Scroll)
4c00-57ff (R/W) Alphanumeric Memory
	(4c00-4fff) Alphanumeric
	(5000-53ff) View character
8000-83ff (R/W) Sound Memory
	(83c0-83ff Sound)
9000-90ff (R/W) 4 bit CPU data
9100-9100 (R/W) 4 bit CPU controller
a000-a000 (R/W) Input/Output
		  on WRITE: IRQ Enable ( 1 = enable, 0 = disable )
		  on READ: bit0 = Not Used, bit1 = 128V, bit2 = Power-Line Sense, bit3 = ADC End Flag
a001-a001 (W) 4 bit CPU Enable
a002-a002 (W) Sound enable
a003-a003 (W) ADC Input Select
a004-a004 (W) CPU 1 Enable
a005-a005 (W) CPU 2 Enable
a006-a006 (W) Start Switch
a007-a007 (W) Color Enable
a100-a100 (W) Watchdog reset
a200-a200 (W) Car Sound ( Lower Nibble )
a300-a300 (W) Car Sound ( Upper Nibble )

Z8002 #1 & #2 (they share the ram)
----------------------------------------
0000-3fff ROM
6000-6003 NMI-Enable
	(6000-6001 CPU1 NMI enable)
	(6002-6003 CPU2 NMI enable)
8000-8fff Motion Object Memory
	(8700-87ff Horizontal and Vertical position)
	(8f00-8fff Character, Color, Vertical size, Horizontal size)
9000-97ff Road Memory
	(9000-93ff Character)
	(9700-97ff Horizontal scroll)
9800-9fff Alphanumeric Memory (video RAM #1)
a000-afff View character memory (I think it refers to 'View' as the background)
c000-c000 View horizontal position
c100-c100 Road vertical position

NOTES:
- Pole Position II reports 'Manual Start' on the Test Mode. This is ok,
because they had to accomodate the hardware from Pole Position I to allow
track selection.

Change POLEPOS_TOGGLE to 0 if you are using the original gearshift

***************************************************************************/

#include "driver.h"

#define POLEPOS_TOGGLE	IPF_TOGGLE


/* from machine */
MACHINE_INIT( polepos );
WRITE_HANDLER( polepos_z80_irq_enable_w );
WRITE16_HANDLER( polepos_z8002_nvi_enable_w );
INTERRUPT_GEN( polepos_z8002_1_interrupt );
INTERRUPT_GEN( polepos_z8002_2_interrupt );
WRITE_HANDLER( polepos_z8002_enable_w );
WRITE_HANDLER( polepos_adc_select_w );
READ_HANDLER( polepos_adc_r );
READ_HANDLER( polepos_io_r );
WRITE_HANDLER( polepos_mcu_enable_w );
READ_HANDLER( polepos_mcu_control_r );
WRITE_HANDLER( polepos_mcu_control_w );
READ_HANDLER( polepos_mcu_data_r );
WRITE_HANDLER( polepos_mcu_data_w );
WRITE_HANDLER( polepos_start_w );
READ16_HANDLER( polepos2_ic25_r );

/* from sndhrdw */
int polepos_sh_start(const struct MachineSound *msound);
void polepos_sh_stop(void);
void polepos_sh_update(void);
WRITE_HANDLER( polepos_engine_sound_lsb_w );
WRITE_HANDLER( polepos_engine_sound_msb_w );

/* from vidhrdw */
extern data16_t *polepos_view16_memory;
extern data16_t *polepos_road16_memory;
extern data16_t *polepos_alpha16_memory;
extern data16_t *polepos_sprite16_memory;

VIDEO_START( polepos );
PALETTE_INIT( polepos );
VIDEO_UPDATE( polepos );

WRITE16_HANDLER( polepos_view16_w );
WRITE16_HANDLER( polepos_road16_w );
WRITE16_HANDLER( polepos_alpha16_w );
WRITE16_HANDLER( polepos_sprite16_w );
WRITE_HANDLER( polepos_view_w );
WRITE_HANDLER( polepos_road_w );
WRITE_HANDLER( polepos_alpha_w );
WRITE_HANDLER( polepos_sprite_w );

READ16_HANDLER( polepos_view16_r );
READ16_HANDLER( polepos_road16_r );
READ16_HANDLER( polepos_alpha16_r );
READ16_HANDLER( polepos_sprite16_r );
READ_HANDLER( polepos_view_r );
READ_HANDLER( polepos_road_r );
READ_HANDLER( polepos_alpha_r );
READ_HANDLER( polepos_sprite_r );

WRITE16_HANDLER( polepos_view16_hscroll_w );
WRITE16_HANDLER( polepos_road16_vscroll_w );



/*********************************************************************
 * CPU memory structures
 *********************************************************************/

static MEMORY_READ_START( z80_readmem )
	{ 0x0000, 0x2fff, MRA_ROM },				/* ROM */
	{ 0x3000, 0x37ff, MRA_RAM },				/* Battery Backup */
	{ 0x4000, 0x47ff, polepos_sprite_r },		/* Motion Object */
	{ 0x4800, 0x4bff, polepos_road_r }, 		/* Road Memory */
	{ 0x4c00, 0x4fff, polepos_alpha_r },		/* Alphanumeric (char ram) */
	{ 0x5000, 0x57ff, polepos_view_r }, 		/* Background Memory */
	{ 0x8000, 0x83ff, MRA_RAM },				/* Sound Memory */
	{ 0x9000, 0x90ff, polepos_mcu_data_r }, 	/* 4 bit CPU data */
	{ 0x9100, 0x9100, polepos_mcu_control_r },	/* 4 bit CPU control */
	{ 0xa000, 0xa000, polepos_io_r },			/* IO */
MEMORY_END

static MEMORY_WRITE_START( z80_writemem )
	{ 0x0000, 0x2fff, MWA_ROM },						/* ROM */
	{ 0x3000, 0x37ff, MWA_RAM, &generic_nvram, &generic_nvram_size },	/* Battery Backup */
	{ 0x4000, 0x47ff, polepos_sprite_w },				/* Motion Object */
	{ 0x4800, 0x4bff, polepos_road_w }, 				/* Road Memory */
	{ 0x4c00, 0x4fff, polepos_alpha_w },				/* Alphanumeric (char ram) */
	{ 0x5000, 0x57ff, polepos_view_w }, 				/* Background Memory */
	{ 0x8000, 0x83bf, MWA_RAM },						/* Sound Memory */
	{ 0x83c0, 0x83ff, polepos_sound_w, &polepos_soundregs },/* Sound data */
	{ 0x9000, 0x90ff, polepos_mcu_data_w }, 			/* 4 bit CPU data */
	{ 0x9100, 0x9100, polepos_mcu_control_w },			/* 4 bit CPU control */
	{ 0xa000, 0xa000, polepos_z80_irq_enable_w },		/* NMI enable */
	{ 0xa001, 0xa001, polepos_mcu_enable_w },			/* 4 bit CPU enable */
	{ 0xa002, 0xa002, MWA_NOP },						/* Sound Enable */
	{ 0xa003, 0xa003, polepos_adc_select_w },			/* ADC Input select */
	{ 0xa004, 0xa005, polepos_z8002_enable_w }, 		/* CPU 1/2 enable */
	{ 0xa006, 0xa006, polepos_start_w },				/* Start Switch */
	{ 0xa007, 0xa007, MWA_NOP },						/* Color Enable */
	{ 0xa100, 0xa100, watchdog_reset_w },				/* Watchdog */
	{ 0xa200, 0xa200, polepos_engine_sound_lsb_w }, 	/* Car Sound ( Lower Nibble ) */
	{ 0xa300, 0xa300, polepos_engine_sound_msb_w }, 	/* Car Sound ( Upper Nibble ) */
MEMORY_END

static PORT_READ_START( z80_readport )
	{ 0x00, 0x00, polepos_adc_r },
PORT_END

static PORT_WRITE_START( z80_writeport )
	{ 0x00, 0x00, IOWP_NOP }, /* ??? */
PORT_END

static MEMORY_READ16_START( z8002_readmem )
	{ 0x0000, 0x7fff, MRA16_ROM },			/* ROM */
	{ 0x8000, 0x8fff, polepos_sprite16_r }, /* Motion Object */
	{ 0x9000, 0x97ff, polepos_road16_r },	/* Road Memory */
	{ 0x9800, 0x9fff, polepos_alpha16_r },	/* Alphanumeric (char ram) */
	{ 0xa000, 0xafff, polepos_view16_r },	/* Background memory */
MEMORY_END

static MEMORY_WRITE16_START( z8002_writemem )
	{ 0x6000, 0x6003, polepos_z8002_nvi_enable_w }, 					/* NVI enable */
	{ 0x0000, 0x7fff, MWA16_ROM },										/* ROM */
	{ 0x8000, 0x8fff, polepos_sprite16_w, &polepos_sprite16_memory },	/* Motion Object */
	{ 0x9000, 0x97ff, polepos_road16_w, &polepos_road16_memory },		/* Road Memory */
	{ 0x9800, 0x9fff, polepos_alpha16_w, &polepos_alpha16_memory }, 	/* Alphanumeric (char ram) */
	{ 0xa000, 0xafff, polepos_view16_w, &polepos_view16_memory },		/* Background memory */
	{ 0xc000, 0xc001, polepos_view16_hscroll_w },						/* Background horz scroll position */
	{ 0xc100, 0xc101, polepos_road16_vscroll_w },						/* Road vertical position */
MEMORY_END


/*********************************************************************
 * Input port definitions
 *********************************************************************/

INPUT_PORTS_START( polepos )
	PORT_START	/* IN0 - Mostly Fake - Handled by the MCU */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | POLEPOS_TOGGLE, "Gear Change", KEYCODE_SPACE, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x08, 0x08, "Display Shift" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, "Nr. of Laps" )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x01, "4" )
	PORT_DIPNAME( 0x06, 0x06, "Game Time" )
	PORT_DIPSETTING(	0x00, "90 secs." )
	PORT_DIPSETTING(	0x04, "100 secs." )
	PORT_DIPSETTING(	0x02, "110 secs." )
	PORT_DIPSETTING(	0x06, "120 secs." )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ))
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Speed Unit" )
	PORT_DIPSETTING(	0x02, "MPH" )
	PORT_DIPSETTING(	0x00, "KPH" )
	PORT_DIPNAME( 0x1c, 0x08, "Extended Rank" )
	PORT_DIPSETTING(	0x00, "A" )
	PORT_DIPSETTING(	0x10, "B" )
	PORT_DIPSETTING(	0x08, "C" )
	PORT_DIPSETTING(	0x18, "D" )
	PORT_DIPSETTING(	0x04, "E" )
	PORT_DIPSETTING(	0x14, "F" )
	PORT_DIPSETTING(	0x0c, "G" )
	PORT_DIPSETTING(	0x1c, "H" )
	PORT_DIPNAME( 0xe0, 0x40, "Practice Rank" )
	PORT_DIPSETTING(	0x00, "A" )
	PORT_DIPSETTING(	0x80, "B" )
	PORT_DIPSETTING(	0x40, "C" )
	PORT_DIPSETTING(	0xc0, "D" )
	PORT_DIPSETTING(	0x20, "E" )
	PORT_DIPSETTING(	0xa0, "F" )
	PORT_DIPSETTING(	0x60, "G" )
	PORT_DIPSETTING(	0xe0, "H" )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL2, 100, 50, 0, 0xff, KEYCODE_LALT, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, KEYCODE_LCONTROL, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x80, IPT_DIAL, 60, 1, 0x00, 0xff )
INPUT_PORTS_END


INPUT_PORTS_START( polepos2 )
	PORT_START	/* IN0 - Mostly Fake - Handled by the MCU */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | POLEPOS_TOGGLE, "Gear Change", KEYCODE_SPACE, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x08, 0x08, "Display Shift" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED ) /* TEST button */
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )	/* docs say "freeze", but it doesn't seem to work */
	PORT_DIPSETTING(	0x00, DEF_STR( Off ))
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ))
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Speed Unit" )
	PORT_DIPSETTING(	0x04, "MPH" )
	PORT_DIPSETTING(	0x00, "KPH" )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Speed" )
	PORT_DIPSETTING(	0x00, "Average" )
	PORT_DIPSETTING(	0x01, "High" )
	PORT_DIPNAME( 0x06, 0x00, "Goal" )
	PORT_DIPSETTING(	0x04, "3" )
	PORT_DIPSETTING(	0x00, "4" )
	PORT_DIPSETTING(	0x02, "5" )
	PORT_DIPSETTING(	0x06, "6" )
	PORT_DIPNAME( 0x18, 0x08, "Extended Rank" )
	PORT_DIPSETTING(	0x10, "A" )
	PORT_DIPSETTING(	0x00, "B" )
	PORT_DIPSETTING(	0x08, "C" )
	PORT_DIPSETTING(	0x18, "D" )
	PORT_DIPNAME( 0x60, 0x20, "Practice Rank" )
	PORT_DIPSETTING(	0x40, "A" )
	PORT_DIPSETTING(	0x00, "B" )
	PORT_DIPSETTING(	0x20, "C" )
	PORT_DIPSETTING(	0x60, "D" )
	PORT_DIPNAME( 0x80, 0x80, "Game Time" )
	PORT_DIPSETTING(	0x00, "90 secs." )
	PORT_DIPSETTING(	0x80, "120 secs." )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL2, 100, 50, 0, 0xff, KEYCODE_LALT, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, KEYCODE_LCONTROL, IP_JOY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x80, IPT_DIAL, 60, 1, 0x00, 0xff )
INPUT_PORTS_END



/*********************************************************************
 * Graphics layouts
 *********************************************************************/

static struct GfxLayout charlayout_2bpp =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	  /* 2 bits per pixel */
	{ 0, 4 }, /* the two bitplanes are packed */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8*2	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout bigspritelayout =
{
	32, 32, /* 32*32 sprites */
	128,	/* 128 sprites */
	4,		/* 4 bits per pixel */
	{ 0, 4, 0x8000*8+0, 0x8000*8+4 }, /* each two of the bitplanes are packed */
	{  0,  1,  2,  3,  8,  9, 10, 11,
	  16, 17, 18, 19, 24, 25, 26, 27,
	  32, 33, 34, 35, 40, 41, 42, 43,
	  48, 49, 50, 51, 56, 57, 58, 59},
	{  0*64,  1*64,  2*64,	3*64,  4*64,  5*64,  6*64,	7*64,
		8*64,  9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64,
	  16*64, 17*64, 18*64, 19*64, 20*64, 21*64, 22*64, 23*64,
	  24*64, 25*64, 26*64, 27*64, 28*64, 29*64, 30*64, 31*64 },
	32*32*2  /* each sprite takes 256 consecutive bytes */
};

static struct GfxLayout smallspritelayout =
{
	16,32,	/* 16*32 sprites (pixel doubled vertically) */
	128,	/* 128 sprites */
	4,		/* 4 bits per pixel */
	{ 0, 4, 0x2000*8+0, 0x2000*8+4 }, /* each two of the bitplanes are packed */
	{  0,  1,  2,  3,  8,  9, 10, 11,
	  16, 17, 18, 19, 24, 25, 26, 27 },
	{ 0*32,  0*32,	1*32,  1*32,  2*32,  2*32,	3*32,  3*32,
	  4*32,  4*32,	5*32,  5*32,  6*32,  6*32,	7*32,  7*32,
	  8*32,  8*32,	9*32,  9*32, 10*32, 10*32, 11*32, 11*32,
	 12*32, 12*32, 13*32, 13*32, 14*32, 14*32, 15*32, 15*32 },
	16*16*2  /* each sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_2bpp,   0x0000, 128 },
	{ REGION_GFX2, 0, &charlayout_2bpp,   0x0200, 128 },
	{ REGION_GFX3, 0, &smallspritelayout, 0x0400, 128 },
	{ REGION_GFX4, 0, &bigspritelayout,   0x0400, 128 },
	{ -1 } /* end of array */
};


/*********************************************************************
 * Sound interfaces
 *********************************************************************/

static struct namco_interface namco_interface =
{
	3125000/64, 	/* sample rate */
	6,				/* number of voices */
	95, 			/* playback volume */
	REGION_SOUND1,	/* memory region */
	1				/* stereo */
};

static struct CustomSound_interface custom_interface =
{
	polepos_sh_start,
	polepos_sh_stop,
	polepos_sh_update
};

static const char *polepos_sample_names[] =
{
	"*polepos",
	"pp2_17.wav",
	"pp2_18.wav",
	0	/* end of array */
};

static struct Samplesinterface samples_interface =
{
	2,	/* 2 channels */
	40, /* volume */
	polepos_sample_names
};



/*********************************************************************
 * Machine driver
 *********************************************************************/

static MACHINE_DRIVER_START( polepos )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3125000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(z80_readmem,z80_writemem)
	MDRV_CPU_PORTS(z80_readport,z80_writeport)

	MDRV_CPU_ADD(Z8000, 3125000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(z8002_readmem,z8002_writemem)
	MDRV_CPU_VBLANK_INT(polepos_z8002_1_interrupt,1)

	MDRV_CPU_ADD(Z8000, 3125000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(z8002_readmem,z8002_writemem)
	MDRV_CPU_VBLANK_INT(polepos_z8002_2_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* some interleaving */

	MDRV_MACHINE_INIT(polepos)
	MDRV_NVRAM_HANDLER(generic_1fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(128)
	MDRV_COLORTABLE_LENGTH(0x1400)

	MDRV_PALETTE_INIT(polepos)
	MDRV_VIDEO_START(polepos)
	MDRV_VIDEO_UPDATE(polepos)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(NAMCO, namco_interface)
	MDRV_SOUND_ADD(CUSTOM, custom_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface)
MACHINE_DRIVER_END


/*********************************************************************
 * ROM definitions
 *********************************************************************/

ROM_START( polepos )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "014-105.rom",   0x0000, 0x2000, CRC(c918c043) SHA1(abc1aa3d7b670b5a65b4565dc646cd3c4edf4e6f) )
	ROM_LOAD	 ( "014-116.rom",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "pp1_1b",        0x0001, 0x2000, CRC(361c56dd) SHA1(6e4abf98b10077c6980e8aa3861f0233135ea68f) )
	ROM_LOAD16_BYTE( "pp1_2b",        0x0000, 0x2000, CRC(582b530a) SHA1(4fc38aa8b70816e14b321ec778090f6c7e7f1640) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "pp1_5b",        0x0001, 0x2000, CRC(5cdf5294) SHA1(dbdf327a541fd71aadafda9c925fa4cf7f7c4a24) )
	ROM_LOAD16_BYTE( "pp1_6b",        0x0000, 0x2000, CRC(81696272) SHA1(27041a7c24297a6f317537c44922b51d2b2278a6) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "pp1_28",        0x0000, 0x1000, CRC(5b277daf) SHA1(0b1feeb2c0c63a5db5ba9b0115aa1b2388636a70) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "pp1_29",        0x0000, 0x1000, CRC(706e888a) SHA1(af1aa2199fcf73a3afbe760857ff117865350954) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "pp1_25",        0x0000, 0x2000, CRC(ac8e28c1) SHA1(13bc2bf4be28d9ae987f79034f9532272b3a2543) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "pp1_26",        0x2000, 0x2000, CRC(94443079) SHA1(413d7b762c8dff541675e96874be6ee0251d3581) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "014-150.rom",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "pp1_19",        0x2000, 0x2000, CRC(43ff83e1) SHA1(8f830549a629b019125e59801e5027e4e4b3c0f2) )
	ROM_LOAD	 ( "pp1_21",        0x4000, 0x2000, CRC(5f958eb4) SHA1(b56d84e5e5e0ddeb0e71851ba66e5fa1b1409551) )
	ROM_LOAD	 ( "014-151.rom",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "pp1_20",        0xa000, 0x2000, CRC(ec18075b) SHA1(af7be549c5fa47551a8dca4c0a531552147fa50f) )
	ROM_LOAD	 ( "pp1_22",        0xc000, 0x2000, CRC(1d2f30b1) SHA1(1d88a3069e9b15febd2835dd63e5511b3b2a6b45) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-137.bpr",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-140.bpr",   0x0300, 0x0100, CRC(1e8d0491) SHA1(e8bf1db5c1fb04a35763099965cf5c588240bde5) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color PROM */
	ROM_LOAD	 ( "pp1_6.bpr",     0x0c00, 0x0400, CRC(2f1079ee) SHA1(18a27998a78deff13dd198f3668a7e92f084f467) )    /* sprite color PROM */
	ROM_LOAD	 ( "131.11n",       0x1000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",   0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "014-159.rom",   0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",   0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "pp1_11",        0x5000, 0x2000, CRC(45b9bfeb) SHA1(ff8c690471944d414931fb88666594ef608997f8) )    /* voice PROM */
	ROM_LOAD	 ( "pp1_12",        0x7000, 0x2000, CRC(a31b4be5) SHA1(38298093bb97ea8647fe187359cae05b65e1c616) )    /* voice PROM */
	ROM_LOAD	 ( "pp1_13",        0x9000, 0x2000, CRC(a4237466) SHA1(88a397276038cc2fc05f2c18472e6b7cef167f2e) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleposa )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "014-105.rom",   0x0000, 0x2000, CRC(c918c043) SHA1(abc1aa3d7b670b5a65b4565dc646cd3c4edf4e6f) )
	ROM_LOAD	 ( "014-116.rom",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "014-101.rom",   0x0001, 0x2000, CRC(8c2cf172) SHA1(57c774afab79599ac3f434113c3170fbb3d42620) )
	ROM_LOAD16_BYTE( "014-102.rom",   0x0000, 0x2000, CRC(51018857) SHA1(ed28d44d172a01f76461f556229d1fe3a1b779a7) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "014-203.rom",   0x0001, 0x2000, CRC(eedea6e7) SHA1(e1459c5e3f824e589e624c3acb18a183fd160df6) )
	ROM_LOAD16_BYTE( "014-204.rom",   0x0000, 0x2000, CRC(c52c98ed) SHA1(2e33c487deaf8afb941e07e511a9828d2d8f6b31) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "014-132.rom",   0x0000, 0x1000, CRC(a949aa85) SHA1(2d6414196b6071101001128418233e585279ffb9) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD	 ( "014-133.rom",   0x0000, 0x1000, CRC(3f0eb551) SHA1(39516d0f72f4e3b03df9451d2dbe081d6c71a508) )    /* 2bpp view layer */

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "014-156.rom",   0x0000, 0x2000, CRC(e7a09c93) SHA1(47cc5c6776333bba8454a3df9e2f6e7de4a465e1) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "014-157.rom",   0x2000, 0x2000, CRC(dee7d687) SHA1(ea34b51c91f6915b74a4a7b53ddb4ff36b72bf66) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "014-150.rom",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "014-152.rom",   0x2000, 0x2000, CRC(a7e3a1c6) SHA1(b7340318afaa4b5f416fe4444899579242cd36c2) )
	ROM_LOAD	 ( "014-154.rom",   0x4000, 0x2000, CRC(8992d381) SHA1(3bf2544dbe88132137acec2c064a104a74139ec7) )
	ROM_LOAD	 ( "014-151.rom",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "014-153.rom",   0xa000, 0x2000, CRC(6c5c6e68) SHA1(dce74ee0e69e0fc0a1942a489c2065381239f0f1) )
	ROM_LOAD	 ( "014-155.rom",   0xc000, 0x2000, CRC(111896ad) SHA1(15032b4c859231373bebfa640421fdcc8ba9d211) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-137.bpr",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-140.bpr",   0x0300, 0x0100, CRC(1e8d0491) SHA1(e8bf1db5c1fb04a35763099965cf5c588240bde5) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color PROM */
	ROM_LOAD	 ( "014-146.bpr",   0x0c00, 0x0400, CRC(ca4ba741) SHA1(de93d738bd27e24dbc4a8378d2c120ef8388c261) )    /* sprite color PROM */
	ROM_LOAD	 ( "014-231.rom",   0x1000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",   0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "014-159.rom",   0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",   0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",   0x5000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( polepos1 )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "014-105.rom",   0x0000, 0x2000, CRC(c918c043) SHA1(abc1aa3d7b670b5a65b4565dc646cd3c4edf4e6f) )
	ROM_LOAD	 ( "014-116.rom",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "014-101.rom",   0x0001, 0x2000, CRC(8c2cf172) SHA1(57c774afab79599ac3f434113c3170fbb3d42620) )
	ROM_LOAD16_BYTE( "014-102.rom",   0x0000, 0x2000, CRC(51018857) SHA1(ed28d44d172a01f76461f556229d1fe3a1b779a7) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "103.3e",        0x0001, 0x2000, CRC(af4fc019) SHA1(1bb6c0f3ffada2e1df72e1767581f8e8bb2b18f9) )
	ROM_LOAD16_BYTE( "104.4e",        0x0000, 0x2000, CRC(ba0045f3) SHA1(aedb8d8c56407963aa4ffb66243288c8fd6d845a) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "014-132.rom",   0x0000, 0x1000, CRC(a949aa85) SHA1(2d6414196b6071101001128418233e585279ffb9) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "014-133.rom",   0x0000, 0x1000, CRC(3f0eb551) SHA1(39516d0f72f4e3b03df9451d2dbe081d6c71a508) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "014-156.rom",   0x0000, 0x2000, CRC(e7a09c93) SHA1(47cc5c6776333bba8454a3df9e2f6e7de4a465e1) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "014-157.rom",   0x2000, 0x2000, CRC(dee7d687) SHA1(ea34b51c91f6915b74a4a7b53ddb4ff36b72bf66) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "014-150.rom",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "014-152.rom",   0x2000, 0x2000, CRC(a7e3a1c6) SHA1(b7340318afaa4b5f416fe4444899579242cd36c2) )
	ROM_LOAD	 ( "014-154.rom",   0x4000, 0x2000, CRC(8992d381) SHA1(3bf2544dbe88132137acec2c064a104a74139ec7) )
	ROM_LOAD	 ( "014-151.rom",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "014-153.rom",   0xa000, 0x2000, CRC(6c5c6e68) SHA1(dce74ee0e69e0fc0a1942a489c2065381239f0f1) )
	ROM_LOAD	 ( "014-155.rom",   0xc000, 0x2000, CRC(111896ad) SHA1(15032b4c859231373bebfa640421fdcc8ba9d211) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-137.bpr",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-140.bpr",   0x0300, 0x0100, CRC(1e8d0491) SHA1(e8bf1db5c1fb04a35763099965cf5c588240bde5) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color PROM */
	ROM_LOAD	 ( "014-146.bpr",   0x0c00, 0x0400, CRC(ca4ba741) SHA1(de93d738bd27e24dbc4a8378d2c120ef8388c261) )    /* sprite color PROM */
	ROM_LOAD	 ( "131.11n",       0x1000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",   0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "014-159.rom",   0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",   0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",   0x5000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( topracer )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "tr9b.bin",      0x0000, 0x2000, CRC(94436b70) SHA1(7495c2a8c3928c59146760d19e672afee01c5b17) )
	ROM_LOAD	 ( "014-116.rom",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "tr1b.bin",      0x0001, 0x2000, CRC(127f0750) SHA1(97ae6c6f8086187c7cdb8bff5fec94914791890b) )
	ROM_LOAD16_BYTE( "tr2b.bin",      0x0000, 0x2000, CRC(6bd4ff6b) SHA1(cf992de39a8cf7804961a8e6773fc4f7feb1878b) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "tr5b.bin",      0x0001, 0x2000, CRC(4e5f7b9c) SHA1(d26b1f24dd9ef00388987890bc5b95d4db403815) )
	ROM_LOAD16_BYTE( "tr6b.bin",      0x0000, 0x2000, CRC(9d038ada) SHA1(7a9496c3fb93fd1945393656f8510a0c6421a9ab) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "tr28.bin",      0x0000, 0x1000, CRC(b8217c96) SHA1(aba311bc3c4b118ba322a00e33e2d5cbe7bc6e4a) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "tr29.bin",      0x0000, 0x1000, CRC(c6e15c21) SHA1(e2a70b3f7ce51a003068eb75d9fe82548f0206d7) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "trus25.bin",    0x0000, 0x2000, CRC(9e1a9c3b) SHA1(deca026c39093119985d1486ed61abc3e6e5705c) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "trus26.bin",    0x2000, 0x2000, CRC(3b39a176) SHA1(d04c9c2c9129c8dd7d7eab24c43502b67162407c) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "pp17.bin",      0x0000, 0x2000, CRC(613ab0df) SHA1(88aa4500275aae010fc9783c1d8d843feab89afa) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "tr19.bin",      0x2000, 0x2000, CRC(f8e7f551) SHA1(faa23c55bc43325e6f71936be970f2ca144697d8) )
	ROM_LOAD	 ( "tr21.bin",      0x4000, 0x2000, CRC(17c798b0) SHA1(ae2047bc0e4e8c85e1de09c39c200ea8f7c6a72e) )
	ROM_LOAD	 ( "pp18.bin",      0x8000, 0x2000, CRC(5fd933e3) SHA1(5b27a8519234c935308f943cd58abc1efc463726) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "tr20.bin",      0xa000, 0x2000, CRC(7053e219) SHA1(97700fbe887e2d11c9f9a0937147725f6787f081) )
	ROM_LOAD	 ( "tr22.bin",      0xc000, 0x2000, CRC(f48917b2) SHA1(2823cfc33ae97ef979d92e2eeeb94c95f1f3d9f3) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-137.bpr",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette PROM */
	ROM_LOAD	 ( "014-138.bpr",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette PROM */
	ROM_LOAD	 ( "014-139.bpr",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette PROM */
	ROM_LOAD	 ( "10p.bin",       0x0300, 0x0100, CRC(5af3f710) SHA1(da13d17acf8abd0f6ebb4b51b23c3324c6197b7d) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-141.bpr",   0x0400, 0x0100, BAD_DUMP CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196)  )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-145.bpr",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color PROM */
	ROM_LOAD	 ( "pp1_6.bpr",     0x0c00, 0x0400, CRC(2f1079ee) SHA1(18a27998a78deff13dd198f3668a7e92f084f467) )    /* sprite color PROM */
	ROM_LOAD	 ( "014-231.rom",   0x1000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "014-158.rom",   0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "014-159.rom",   0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "014-134.rom",   0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",   0x5000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( polepos2 )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "pp4_9.6h",      0x0000, 0x2000, CRC(bcf87004) SHA1(0c60cbb777fe72dfd11c6f3e9da806a515cd0f8a) )
	ROM_LOAD	 ( "183.7f",        0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "pp4_1.8m",      0x0001, 0x2000, CRC(3f6ac294) SHA1(414ea7e43e62a573ad8971a7045f61eb997cf94e) )
	ROM_LOAD16_BYTE( "pp4_2.8l",      0x0000, 0x2000, CRC(51b9a669) SHA1(563ba42098d330801a992cd9c008c4cbbb993530) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "pp4_5.4m",      0x0001, 0x2000, CRC(c3053cae) SHA1(f42cf61fe696dd7e282b29e2234ea7f487ec2372) )
	ROM_LOAD16_BYTE( "pp4_6.4l",      0x0000, 0x2000, CRC(38d04e0f) SHA1(5527cb1864248208b10d219a50ad742f286a119f) )
	ROM_LOAD16_BYTE( "pp4_7.3m",      0x4001, 0x1000, CRC(ad1c8994) SHA1(2877de9641516767170c0109900955cc7d1ff402) )
	ROM_LOAD16_BYTE( "pp4_8.3l",      0x4000, 0x1000, CRC(ef25a2ee) SHA1(45959355cad1a48f19ae14193374e03d4f9965c7) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "pp4_28.1f",     0x0000, 0x2000, CRC(280dde7d) SHA1(b7c7fb3a5076aa4d0e0cf3256ece9a6194315626) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",        0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "pp4_25.1n",     0x0000, 0x2000, CRC(fd098e65) SHA1(2c497f1d278ba6730752706a0d1b5a5a0fec3d5b) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "pp4_26.1m",     0x2000, 0x2000, CRC(35ac62b3) SHA1(21038a78eb73d520e3e1ae8e1c0047d06b94cdab) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",       0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "pp1_19.4n",     0x2000, 0x2000, CRC(43ff83e1) SHA1(8f830549a629b019125e59801e5027e4e4b3c0f2) )
	ROM_LOAD	 ( "pp1_21.3n",     0x4000, 0x2000, CRC(5f958eb4) SHA1(b56d84e5e5e0ddeb0e71851ba66e5fa1b1409551) )
	ROM_LOAD	 ( "pp4_23.2n",     0x6000, 0x2000, CRC(9e056fcd) SHA1(8545e0a9b6ebf8c2903321ceb9c4d693db10d750) )
	ROM_LOAD	 ( "120.12j",       0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "pp1_20.4m",     0xa000, 0x2000, CRC(ec18075b) SHA1(af7be549c5fa47551a8dca4c0a531552147fa50f) )
	ROM_LOAD	 ( "pp1_22.3m",     0xc000, 0x2000, CRC(1d2f30b1) SHA1(1d88a3069e9b15febd2835dd63e5511b3b2a6b45) )
	ROM_LOAD	 ( "pp4_24.2m",     0xe000, 0x2000, CRC(795268cf) SHA1(84136142ef4bdcd97ede2209ecb16745960ac393) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-186.bpr",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color PROM */
	ROM_LOAD	 ( "pp4-6.6m",      0x0c00, 0x0400, CRC(647212b5) SHA1(ad58dfebd0ce8226285c2671c3b7797852c26d07) )    /* sprite color PROM */
	ROM_LOAD	 ( "131.11n",       0x1000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",        0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "128.2m",        0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",        0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "pp1_11.2e",     0x5000, 0x2000, CRC(45b9bfeb) SHA1(ff8c690471944d414931fb88666594ef608997f8) )    /* voice PROM */
	ROM_LOAD	 ( "pp1_12.2f",     0x7000, 0x2000, CRC(a31b4be5) SHA1(38298093bb97ea8647fe187359cae05b65e1c616) )    /* voice PROM */
	ROM_LOAD	 ( "pp1_13.1e",     0x9000, 0x2000, CRC(a4237466) SHA1(88a397276038cc2fc05f2c18472e6b7cef167f2e) )    /* voice PROM */
	ROM_LOAD	 ( "pp1_14.1f",     0xb000, 0x2000, CRC(944580f9) SHA1(c76f529cae718674ce97a1a599a3c6eaf6bf561a) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleps2a )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "180.7h",        0x0000, 0x2000, CRC(f85212c4) SHA1(666e55a7662247e72393b105b3e719be4233f1ff) )
	ROM_LOAD	 ( "183.7f",        0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "176.3l",        0x0001, 0x2000, CRC(8aeaec98) SHA1(76b3bbb64a17090bf28858f1e91d2206a3beaf5b) )
	ROM_LOAD16_BYTE( "177.4l",        0x0000, 0x2000, CRC(7051df35) SHA1(cf23118ab05f5af273d756f97e6453496a276c9a) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "178.3e",        0x0001, 0x2000, CRC(eac35cfa) SHA1(f96005b3b63d85fc30695ab746af79c60f2f1341) )
	ROM_LOAD16_BYTE( "179.4e",        0x0000, 0x2000, CRC(613e917d) SHA1(97c139f8aa7bd871a907e72980757b83f99fd8a0) )
	ROM_LOAD16_BYTE( "184.3d",        0x4001, 0x2000, CRC(d893c4ed) SHA1(60d39abefbb0c8df68864a30b1f5fcbf4780c86c) )
	ROM_LOAD16_BYTE( "185.4d",        0x4000, 0x2000, CRC(899de75e) SHA1(4a16535115e37a3d342b2cb53f610a87c0d0abe1) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "172.7n",        0x0000, 0x2000, CRC(fbe5e72f) SHA1(07965d6e98ac1332ac6192b5e9cc927dd9eb706f) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",        0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "170.13n",       0x0000, 0x2000, CRC(455d79a0) SHA1(03ef7c58f3145d9a6a461ef1aea3b5a49e653f80) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "171.12n",       0x2000, 0x2000, CRC(78372b81) SHA1(5defaf2074c1ab4d13dc36a190c658ddf7f7931b) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",       0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "166.13k",       0x2000, 0x2000, CRC(2b0517bd) SHA1(ebe447ba3dcd8a3b56f47d707483074f61953fec) )
	ROM_LOAD	 ( "168.13l",       0x4000, 0x2000, CRC(4d7916d9) SHA1(052745f252f51bfdd456e54cf7b8d22ab3aace27) )
	ROM_LOAD	 ( "175.13m",       0x6000, 0x2000, CRC(bd6df480) SHA1(58f39fa3ae43d94fe42dc51da341384a9c3879ae) )
	ROM_LOAD	 ( "120.12j",       0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "167.12k",       0xa000, 0x2000, CRC(411e21b5) SHA1(9659ee429d819926b5e5b12c41b968ae6e7f186e) )
	ROM_LOAD	 ( "169.12l",       0xc000, 0x2000, CRC(662ff24b) SHA1(4cf8509034742c2bec8a96c7a786dafdf5875e4f) )
	ROM_LOAD	 ( "174.12m",       0xe000, 0x2000, CRC(f0c571dc) SHA1(9e6839e9e203fc120a0389f4e11c9d46a817dbdf) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-186.bpr",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color PROM */
	ROM_LOAD	 ( "014-192.bpr",   0x0c00, 0x0400, CRC(caddb0b0) SHA1(e41b89f2b40bf8f93546012f373ae63dcae870da) )    /* sprite color PROM */
	ROM_LOAD	 ( "131.11n",       0x1000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",        0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "128.2m",        0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",        0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",   0x5000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleps2b )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "180.7h",        0x0000, 0x2000, CRC(f85212c4) SHA1(666e55a7662247e72393b105b3e719be4233f1ff) )
	ROM_LOAD	 ( "183.7f",        0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "176-v2.3l",     0x0001, 0x2000, CRC(848ab742) SHA1(75ee8864daa1bab42ef92afbc37c1c818bbd1d14) )
	ROM_LOAD16_BYTE( "177-v2.4l",     0x0000, 0x2000, CRC(643483f7) SHA1(020822f623b8e65c6016492266b6e328f7637b68) )
	ROM_LOAD16_BYTE( "rom-v2.4k",     0x4000, 0x1000, CRC(2d70dce4) SHA1(81c8bac3435289de78cf2aa13faf211dda0fa827) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "178.3e",        0x0001, 0x2000, CRC(eac35cfa) SHA1(f96005b3b63d85fc30695ab746af79c60f2f1341) )
	ROM_LOAD16_BYTE( "179.4e",        0x0000, 0x2000, CRC(613e917d) SHA1(97c139f8aa7bd871a907e72980757b83f99fd8a0) )
	ROM_LOAD16_BYTE( "184.3d",        0x4001, 0x2000, CRC(d893c4ed) SHA1(60d39abefbb0c8df68864a30b1f5fcbf4780c86c) )
	ROM_LOAD16_BYTE( "185.4d",        0x4000, 0x2000, CRC(899de75e) SHA1(4a16535115e37a3d342b2cb53f610a87c0d0abe1) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "172.7n",        0x0000, 0x2000, CRC(fbe5e72f) SHA1(07965d6e98ac1332ac6192b5e9cc927dd9eb706f) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",        0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "170.13n",       0x0000, 0x2000, CRC(455d79a0) SHA1(03ef7c58f3145d9a6a461ef1aea3b5a49e653f80) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "171.12n",       0x2000, 0x2000, CRC(78372b81) SHA1(5defaf2074c1ab4d13dc36a190c658ddf7f7931b) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",       0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "166.13k",       0x2000, 0x2000, CRC(2b0517bd) SHA1(ebe447ba3dcd8a3b56f47d707483074f61953fec) )
	ROM_LOAD	 ( "168.13l",       0x4000, 0x2000, CRC(4d7916d9) SHA1(052745f252f51bfdd456e54cf7b8d22ab3aace27) )
	ROM_LOAD	 ( "175.13m",       0x6000, 0x2000, CRC(bd6df480) SHA1(58f39fa3ae43d94fe42dc51da341384a9c3879ae) )
	ROM_LOAD	 ( "120.12j",       0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "167.12k",       0xa000, 0x2000, CRC(411e21b5) SHA1(9659ee429d819926b5e5b12c41b968ae6e7f186e) )
	ROM_LOAD	 ( "169.12l",       0xc000, 0x2000, CRC(662ff24b) SHA1(4cf8509034742c2bec8a96c7a786dafdf5875e4f) )
	ROM_LOAD	 ( "174.12m",       0xe000, 0x2000, CRC(f0c571dc) SHA1(9e6839e9e203fc120a0389f4e11c9d46a817dbdf) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-186.bpr",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color PROM */
	ROM_LOAD	 ( "014-192.bpr",   0x0c00, 0x0400, CRC(caddb0b0) SHA1(e41b89f2b40bf8f93546012f373ae63dcae870da) )    /* sprite color PROM */
	ROM_LOAD	 ( "131.11n",       0x1000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",        0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "128.2m",        0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",        0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",   0x5000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleps2c )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD	 ( "180.7h",        0x0000, 0x2000, CRC(f85212c4) SHA1(666e55a7662247e72393b105b3e719be4233f1ff) )
	ROM_LOAD	 ( "183.7f",        0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "3lcpu.rom",     0x0001, 0x2000, CRC(cf95a6b7) SHA1(6a8419af8a52d3a8c88663b67845e4cb18e35723) )
	ROM_LOAD16_BYTE( "177-v2.4l",     0x0000, 0x2000, CRC(643483f7) SHA1(020822f623b8e65c6016492266b6e328f7637b68) )
	ROM_LOAD16_BYTE( "cpu-4k.rom",    0x4000, 0x1000, CRC(97a496b3) SHA1(fe79d2376c5fa9fe242905a841a1c894a5ccfba4) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "178.3e",        0x0001, 0x2000, CRC(eac35cfa) SHA1(f96005b3b63d85fc30695ab746af79c60f2f1341) )
	ROM_LOAD16_BYTE( "179.4e",        0x0000, 0x2000, CRC(613e917d) SHA1(97c139f8aa7bd871a907e72980757b83f99fd8a0) )
	ROM_LOAD16_BYTE( "184.3d",        0x4001, 0x2000, CRC(d893c4ed) SHA1(60d39abefbb0c8df68864a30b1f5fcbf4780c86c) )
	ROM_LOAD16_BYTE( "185.4d",        0x4000, 0x2000, CRC(899de75e) SHA1(4a16535115e37a3d342b2cb53f610a87c0d0abe1) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD	 ( "172.7n",        0x0000, 0x2000, CRC(fbe5e72f) SHA1(07965d6e98ac1332ac6192b5e9cc927dd9eb706f) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD	 ( "173.6n",        0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD	 ( "170.13n",       0x0000, 0x2000, CRC(455d79a0) SHA1(03ef7c58f3145d9a6a461ef1aea3b5a49e653f80) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD	 ( "171.12n",       0x2000, 0x2000, CRC(78372b81) SHA1(5defaf2074c1ab4d13dc36a190c658ddf7f7931b) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD	 ( "119.13j",       0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD	 ( "166.13k",       0x2000, 0x2000, CRC(2b0517bd) SHA1(ebe447ba3dcd8a3b56f47d707483074f61953fec) )
	ROM_LOAD	 ( "13lvid.rom",    0x4000, 0x2000, CRC(9ab89d7f) SHA1(e98c78f8cee8f72b14279cf4a9322cb97b007af5) )
	ROM_LOAD	 ( "175.13m",       0x6000, 0x2000, CRC(bd6df480) SHA1(58f39fa3ae43d94fe42dc51da341384a9c3879ae) )
	ROM_LOAD	 ( "120.12j",       0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD	 ( "12kvid.rom",    0xa000, 0x2000, CRC(fa131a9b) SHA1(c19ac9a9ac6daf582817bc5fde508a66c4b35663) )
	ROM_LOAD	 ( "169.12l",       0xc000, 0x2000, CRC(662ff24b) SHA1(4cf8509034742c2bec8a96c7a786dafdf5875e4f) )
	ROM_LOAD	 ( "174.12m",       0xe000, 0x2000, CRC(f0c571dc) SHA1(9e6839e9e203fc120a0389f4e11c9d46a817dbdf) )

	/* graphics (P)ROM data */
	ROM_REGION( 0x7000, REGION_PROMS, 0 )
	ROM_LOAD	 ( "014-186.bpr",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette PROM */
	ROM_LOAD	 ( "014-187.bpr",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette PROM */
	ROM_LOAD	 ( "014-188.bpr",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette PROM */
	ROM_LOAD	 ( "014-189.bpr",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color PROM */
	ROM_LOAD	 ( "014-190.bpr",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* view color PROM */
	ROM_LOAD	 ( "014-142.bpr",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low PROM */
	ROM_LOAD	 ( "014-143.bpr",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med PROM */
	ROM_LOAD	 ( "014-144.bpr",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi PROM */
	ROM_LOAD	 ( "014-191.bpr",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color PROM */
	ROM_LOAD	 ( "014-192.bpr",   0x0c00, 0x0400, CRC(caddb0b0) SHA1(e41b89f2b40bf8f93546012f373ae63dcae870da) )    /* sprite color PROM */
	ROM_LOAD	 ( "131.11n",       0x1000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling PROM */
	ROM_LOAD	 ( "127.2l",        0x2000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control PROM */
	ROM_LOAD	 ( "128.2m",        0x4000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 PROM */
	ROM_LOAD	 ( "134.2n",        0x6000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* read bits 2 PROM */

	/* sound (P)ROM data */
	ROM_REGION( 0xd000, REGION_SOUND1, 0 )
	ROM_LOAD	 ( "014-118.bpr",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound PROM */
	ROM_LOAD	 ( "014-110.rom",   0x1000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-111.rom",   0x3000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound PROM */
	ROM_LOAD	 ( "014-106.rom",   0x5000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice PROM */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD	 ( "014-117.bpr",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


/*********************************************************************
 * Initialization routines
 *********************************************************************/

static DRIVER_INIT( polepos2 )
{
	/* note that the bootleg versions don't need this custom IC; they have a hacked ROM in its place */
	install_mem_read16_handler(1, 0x4000, 0x5fff, polepos2_ic25_r);
}


/*********************************************************************
 * Game drivers
 *********************************************************************/

GAME( 1982, polepos,  0,		polepos, polepos,  0,		 ROT0, "Namco", "Pole Position" )
GAME( 1982, poleposa, polepos,	polepos, polepos,  0,		 ROT0, "Namco (Atari license)", "Pole Position (Atari version 2)" )
GAME( 1982, polepos1, polepos,	polepos, polepos,  0,		 ROT0, "[Namco] (Atari license)", "Pole Position (Atari version 1)" )
GAME( 1982, topracer, polepos,	polepos, polepos,  0,		 ROT0, "bootleg", "Top Racer" )
GAME( 1983, polepos2, 0,		polepos, polepos2, polepos2, ROT0, "Namco", "Pole Position II" )
GAME( 1983, poleps2a, polepos2, polepos, polepos2, polepos2, ROT0, "Namco (Atari license)", "Pole Position II (Atari)" )
GAME( 1983, poleps2b, polepos2, polepos, polepos2, 0,		 ROT0, "Namco (Atari license)", "Pole Position II (Atari bootleg 1)" )
GAME( 1983, poleps2c, polepos2, polepos, polepos2, 0,		 ROT0, "Namco (Atari license)", "Pole Position II (Atari bootleg 2)" )

