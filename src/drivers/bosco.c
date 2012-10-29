/***************************************************************************

Bosconian

driver by Martin Scragg


CPU #1:
0000-3fff ROM
CPU #2:
0000-1fff ROM
CPU #3:
0000-1fff ROM
ALL CPUS:
8000-83ff Video RAM
8400-87ff Color RAM
8b80-8bff sprite code/color
9380-93ff sprite position
9b80-9bff sprite control
8800-9fff RAM

read:
6800-6807 dip switches (only bits 0 and 1 are used - bit 0 is DSW1, bit 1 is DSW2)
	  dsw1:
	    bit 6-7 lives
	    bit 3-5 bonus
	    bit 0-2 coins per play
		  dsw2: (bootleg version, the original version is slightly different)
		    bit 7 cocktail/upright (1 = upright)
	    bit 6 ?
	    bit 5 RACK TEST
	    bit 4 pause (0 = paused, 1 = not paused)
	    bit 3 ?
	    bit 2 ?
	    bit 0-1 difficulty
7000-	  custom IO chip return values
7100	  custom IO chip status ($10 = command executed)

write:
6805	  sound voice 1 waveform (nibble)
6811-6813 sound voice 1 frequency (nibble)
6815	  sound voice 1 volume (nibble)
680a	  sound voice 2 waveform (nibble)
6816-6818 sound voice 2 frequency (nibble)
681a	  sound voice 2 volume (nibble)
680f	  sound voice 3 waveform (nibble)
681b-681d sound voice 3 frequency (nibble)
681f	  sound voice 3 volume (nibble)
6820	  cpu #1 irq acknowledge/enable
6821	  cpu #2 irq acknowledge/enable
6822	  cpu #3 nmi acknowledge/enable
6823	  if 0, halt CPU #2 and #3
6830	  Watchdog reset?
7000-	  custom IO chip parameters
7100	  custom IO chip command (see machine/bosco.c for more details)
a000-a002 starfield scroll direction/speed (only bit 0 is significant)
a003-a005 starfield blink?
a007	  flip screen

Interrupts:
CPU #1 IRQ mode 1
       NMI is triggered by the custom IO chip to signal the CPU to read/write
	       parameters
CPU #2 IRQ mode 1
CPU #3 NMI (@120Hz)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *bosco_sharedram;
READ_HANDLER( bosco_sharedram_r );
WRITE_HANDLER( bosco_sharedram_w );
READ_HANDLER( bosco_dsw_r );
WRITE_HANDLER( bosco_interrupt_enable_1_w );
WRITE_HANDLER( bosco_interrupt_enable_2_w );
WRITE_HANDLER( bosco_interrupt_enable_3_w );
WRITE_HANDLER( bosco_halt_w );
READ_HANDLER( bosco_customio_1_r );
READ_HANDLER( bosco_customio_2_r );
WRITE_HANDLER( bosco_customio_1_w );
WRITE_HANDLER( bosco_customio_2_w );
READ_HANDLER( bosco_customio_data_1_r );
READ_HANDLER( bosco_customio_data_2_r );
WRITE_HANDLER( bosco_customio_data_1_w );
WRITE_HANDLER( bosco_customio_data_2_w );
INTERRUPT_GEN( bosco_interrupt_1 );
INTERRUPT_GEN( bosco_interrupt_2 );
INTERRUPT_GEN( bosco_interrupt_3 );
MACHINE_INIT( bosco );

WRITE_HANDLER( bosco_cpu_reset_w );
VIDEO_START( bosco );
VIDEO_UPDATE( bosco );
PALETTE_INIT( bosco );

extern unsigned char *bosco_videoram2,*bosco_colorram2;
extern unsigned char *bosco_radarx,*bosco_radary,*bosco_radarattr;
extern size_t bosco_radarram_size;
extern unsigned char *bosco_staronoff;
extern unsigned char *bosco_starblink;
WRITE_HANDLER( bosco_videoram2_w );
WRITE_HANDLER( bosco_colorram2_w );
WRITE_HANDLER( bosco_flipscreen_w );
WRITE_HANDLER( bosco_scrollx_w );
WRITE_HANDLER( bosco_scrolly_w );
WRITE_HANDLER( bosco_starcontrol_w );
VIDEO_START( bosco );
VIDEO_UPDATE( bosco );

WRITE_HANDLER( pengo_sound_w );
int  bosco_sh_start(const struct MachineSound *msound);
void bosco_sh_stop(void);
extern unsigned char *pengo_soundregs;


static MEMORY_READ_START( readmem_cpu1 )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x6800, 0x6807, bosco_dsw_r },
	{ 0x7000, 0x700f, bosco_customio_data_1_r },
	{ 0x7100, 0x7100, bosco_customio_1_r },
	{ 0x7800, 0x97ff, bosco_sharedram_r },
MEMORY_END

static MEMORY_READ_START( readmem_cpu2 )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x6800, 0x6807, bosco_dsw_r },
	{ 0x9000, 0x900f, bosco_customio_data_2_r },
	{ 0x9100, 0x9100, bosco_customio_2_r },
	{ 0x7800, 0x97ff, bosco_sharedram_r },
MEMORY_END

static MEMORY_READ_START( readmem_cpu3 )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x6800, 0x6807, bosco_dsw_r },
	{ 0x7800, 0x97ff, bosco_sharedram_r },
MEMORY_END

static MEMORY_WRITE_START( writemem_cpu1 )
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x6800, 0x681f, pengo_sound_w, &pengo_soundregs },
	{ 0x6820, 0x6820, bosco_interrupt_enable_1_w },
	{ 0x6822, 0x6822, bosco_interrupt_enable_3_w },
	{ 0x6823, 0x6823, bosco_halt_w },
	{ 0x6830, 0x6830, watchdog_reset_w },
	{ 0x7000, 0x700f, bosco_customio_data_1_w },
	{ 0x7100, 0x7100, bosco_customio_1_w },

	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, bosco_videoram2_w, &bosco_videoram2 },
	{ 0x8800, 0x8bff, colorram_w, &colorram },
	{ 0x8c00, 0x8fff, bosco_colorram2_w, &bosco_colorram2 },

	{ 0x7800, 0x97ff, bosco_sharedram_w, &bosco_sharedram },

	{ 0x83d4, 0x83df, MWA_RAM, &spriteram, &spriteram_size },	/* these are here just to initialize */
	{ 0x8bd4, 0x8bdf, MWA_RAM, &spriteram_2 },			/* the pointers. */
	{ 0x83f4, 0x83ff, MWA_RAM, &bosco_radarx, &bosco_radarram_size },	/* ditto */
	{ 0x8bf4, 0x8bff, MWA_RAM, &bosco_radary },

	{ 0x9810, 0x9810, bosco_scrollx_w },
	{ 0x9820, 0x9820, bosco_scrolly_w },
	{ 0x9830, 0x9830, bosco_starcontrol_w },
	{ 0x9840, 0x9840, MWA_RAM, &bosco_staronoff },
	{ 0x9870, 0x9870, bosco_flipscreen_w },
	{ 0x9804, 0x980f, MWA_RAM, &bosco_radarattr },
MEMORY_END

static MEMORY_WRITE_START( writemem_cpu2 )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x6821, 0x6821, bosco_interrupt_enable_2_w },

	{ 0x8000, 0x83ff, videoram_w },
	{ 0x8400, 0x87ff, bosco_videoram2_w },
	{ 0x8800, 0x8bff, colorram_w },
	{ 0x8c00, 0x8fff, bosco_colorram2_w },
	{ 0x9000, 0x900f, bosco_customio_data_2_w },
	{ 0x9100, 0x9100, bosco_customio_2_w },
	{ 0x7800, 0x97ff, bosco_sharedram_w },

	{ 0x9810, 0x9810, bosco_scrollx_w },
	{ 0x9820, 0x9820, bosco_scrolly_w },
	{ 0x9830, 0x9830, bosco_starcontrol_w },
	{ 0x9874, 0x9875, MWA_RAM, &bosco_starblink },
MEMORY_END

static MEMORY_WRITE_START( writemem_cpu3 )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x6800, 0x681f, pengo_sound_w },
	{ 0x6822, 0x6822, bosco_interrupt_enable_3_w },

	{ 0x8000, 0x83ff, videoram_w },
	{ 0x8400, 0x87ff, bosco_videoram2_w },
	{ 0x8800, 0x8bff, colorram_w },
	{ 0x8c00, 0x8fff, bosco_colorram2_w },
	{ 0x7800, 0x97ff, bosco_sharedram_w },
MEMORY_END



INPUT_PORTS_START( bosco )
	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x08, "Bonus Fighter" )
	PORT_DIPSETTING(    0x30, "15K 50K" )
	PORT_DIPSETTING(    0x38, "20K 70K" )
	PORT_DIPSETTING(    0x08, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "15K 50K 50K" )
	PORT_DIPSETTING(    0x18, "15K 70K 70K" )
	PORT_DIPSETTING(    0x20, "20K 70K 70K" )
	PORT_DIPSETTING(    0x28, "30K 100K 100K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x02, "Hardest" )
	PORT_DIPSETTING(    0x00, "Auto" )
	PORT_DIPNAME( 0x04, 0x04, "2 Credits Game" )
	PORT_DIPSETTING(    0x00, "1 Player" )
	PORT_DIPSETTING(    0x04, "2 Players" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Freeze" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, "Test ????" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* FAKE */
	/* The player inputs are not memory mapped, they are handled by an I/O chip. */
	/* These fake input ports are read by galaga_customio_data_r() */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	/* the button here is used to trigger the sound in the test screen */
	PORT_BITX(0x03, IP_ACTIVE_LOW, IPT_BUTTON1,	0, IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( boscomd )
	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x38, 0x08, "Bonus Fighter" )
	PORT_DIPSETTING(    0x30, "15K 50K" )
	PORT_DIPSETTING(    0x38, "20K 70K" )
	PORT_DIPSETTING(    0x08, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "15K 50K 50K" )
	PORT_DIPSETTING(    0x18, "15K 70K 70K" )
	PORT_DIPSETTING(    0x20, "20K 70K 70K" )
	PORT_DIPSETTING(    0x28, "30K 100K 100K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0xc0, "5" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits Game" )
	PORT_DIPSETTING(    0x00, "1 Player" )
	PORT_DIPSETTING(    0x01, "2 Players" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x06, "Medium" )
	PORT_DIPSETTING(    0x04, "Hardest" )
	PORT_DIPSETTING(    0x00, "Auto" )
	PORT_DIPNAME( 0x08, 0x08, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Test ????" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START	/* FAKE */
	/* The player inputs are not memory mapped, they are handled by an I/O chip. */
	/* These fake input ports are read by galaga_customio_data_r() */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 1 )
	PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL, 0, IP_KEY_PREVIOUS, IP_JOY_PREVIOUS )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* FAKE */
	/* the button here is used to trigger the sound in the test screen */
	PORT_BITX(0x03, IP_ACTIVE_LOW, IPT_BUTTON1,	0, IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },      /* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },   /* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },   /* characters are rotated 90 degrees */
	16*8	       /* every char takes 16 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,		/* 16*16 sprites */
	64,		/* 128 sprites */
	2,		/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3  },
	{ 0 * 8, 1 * 8, 2 * 8, 3 * 8, 4 * 8, 5 * 8, 6 * 8, 7 * 8,
			32 * 8, 33 * 8, 34 * 8, 35 * 8, 36 * 8, 37 * 8, 38 * 8, 39 * 8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxLayout dotlayout =
{
	4,4,	/* 4*4 characters */
	8,	/* 8 characters */
	2,	/* 2 bits per pixel */
	{ 6, 7 },
	{ 3*8, 2*8, 1*8, 0*8 },
	{ 3*32, 2*32, 1*32, 0*32 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,	        0, 64 },
	{ REGION_GFX2, 0, &spritelayout,	 64*4, 64 },
	{ REGION_GFX3, 0, &dotlayout,    64*4+64*4,	1 },
	{ -1 } /* end of array */
};



static struct namco_interface namco_interface =
{
	3072000/32,	/* sample rate */
	3,			/* number of voices */
	50,			/* playback volume */
	REGION_SOUND1	/* memory region */
};


static const char *bosco_sample_names[] =
{
	"*bosco",
	"midbang.wav",
	"bigbang.wav",
	"shot.wav",
	0	/* end of array */
};

static struct Samplesinterface samples_interface =
{
	3,	/* 3 channels */
	80,	/* volume */
	bosco_sample_names
};


static struct CustomSound_interface custom_interface =
{
	bosco_sh_start,
	bosco_sh_stop,
	0
};


static MACHINE_DRIVER_START( bosco )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3125000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu1,writemem_cpu1)
	MDRV_CPU_VBLANK_INT(bosco_interrupt_1,1)

	MDRV_CPU_ADD(Z80, 3125000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu2,writemem_cpu2)
	MDRV_CPU_VBLANK_INT(bosco_interrupt_2,1)

	MDRV_CPU_ADD(Z80, 3125000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu3,writemem_cpu3)
	MDRV_CPU_VBLANK_INT(bosco_interrupt_3,2)

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(bosco)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(32+64)
	MDRV_COLORTABLE_LENGTH(64*4+64*4+4)	/* 32 for the characters, 64 for the stars */

	MDRV_PALETTE_INIT(bosco)
	MDRV_VIDEO_START(bosco)
	MDRV_VIDEO_UPDATE(bosco)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO, namco_interface)
	MDRV_SOUND_ADD(CUSTOM, custom_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface)
MACHINE_DRIVER_END





/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( bosco )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "bos3_1.bin",   0x0000, 0x1000, CRC(96021267) SHA1(bd49b0caabcccf9df45a272d767456a4fc8a7c07) )
	ROM_LOAD( "bos1_2.bin",   0x1000, 0x1000, CRC(2d8f3ebe) SHA1(75de1cba7531ae4bf7fbbef7b8e37b9fec4ed0d0) )
	ROM_LOAD( "bos1_3.bin",   0x2000, 0x1000, CRC(c80ccfa5) SHA1(f2bbec2ea9846d4601f06c0b4242744447a88fda) )
	ROM_LOAD( "bos1_4b.bin",  0x3000, 0x1000, CRC(a3f7f4ab) SHA1(eb26184311bae0767c7a5593926e6eadcbcb680e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "bos1_5c.bin",  0x0000, 0x1000, CRC(a7c8e432) SHA1(3607be75daa10f1f98dbfd9e600c5ba513130d44) )
	ROM_LOAD( "bos3_6.bin",   0x1000, 0x1000, CRC(4543cf82) SHA1(50ad7d1ab6694eb8fab88d0fa79ee04f6984f3ca) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.1d",      0x0120, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )	/* ?? */
	ROM_LOAD( "prom.2r",      0x0220, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* ?? */
	ROM_LOAD( "prom.7h",      0x0240, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* ?? */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "bosco.spr",    0x0000, 0x0100, CRC(ee8ca3a8) SHA1(48d5d9e8c9ca966edad0d5198bb445f6ecceb037) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscoo )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "bos1_1.bin",   0x0000, 0x1000, CRC(0d9920e7) SHA1(e7633233f603ccb5b7a970ed5b58ef361ef2c94e) )
	ROM_LOAD( "bos1_2.bin",   0x1000, 0x1000, CRC(2d8f3ebe) SHA1(75de1cba7531ae4bf7fbbef7b8e37b9fec4ed0d0) )
	ROM_LOAD( "bos1_3.bin",   0x2000, 0x1000, CRC(c80ccfa5) SHA1(f2bbec2ea9846d4601f06c0b4242744447a88fda) )
	ROM_LOAD( "bos1_4b.bin",  0x3000, 0x1000, CRC(a3f7f4ab) SHA1(eb26184311bae0767c7a5593926e6eadcbcb680e) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "bos1_5c.bin",  0x0000, 0x1000, CRC(a7c8e432) SHA1(3607be75daa10f1f98dbfd9e600c5ba513130d44) )
	ROM_LOAD( "2800.3h",      0x1000, 0x1000, CRC(31b8c648) SHA1(de0db24d385d2361ec989bf32388df8202ad535c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.1d",      0x0120, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )	/* ?? */
	ROM_LOAD( "prom.2r",      0x0220, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* ?? */
	ROM_LOAD( "prom.7h",      0x0240, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* ?? */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "bosco.spr",    0x0000, 0x0100, CRC(ee8ca3a8) SHA1(48d5d9e8c9ca966edad0d5198bb445f6ecceb037) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscoo2 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "bos1_1.bin",   0x0000, 0x1000, CRC(0d9920e7) SHA1(e7633233f603ccb5b7a970ed5b58ef361ef2c94e) )
	ROM_LOAD( "bos1_2.bin",   0x1000, 0x1000, CRC(2d8f3ebe) SHA1(75de1cba7531ae4bf7fbbef7b8e37b9fec4ed0d0) )
	ROM_LOAD( "bos1_3.bin",   0x2000, 0x1000, CRC(c80ccfa5) SHA1(f2bbec2ea9846d4601f06c0b4242744447a88fda) )
	ROM_LOAD( "bos1_4.3k",    0x3000, 0x1000, CRC(7ebea2b8) SHA1(92fc66526ed77f3efd947b7d321b255aba4a0140) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "bos1_5b.3j",   0x0000, 0x1000, CRC(3d6955a8) SHA1(f89860d74865da5ced2f5b2196bdaa8eeb5e2322) )
	ROM_LOAD( "2800.3h",      0x1000, 0x1000, CRC(31b8c648) SHA1(de0db24d385d2361ec989bf32388df8202ad535c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.1d",      0x0120, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )	/* ?? */
	ROM_LOAD( "prom.2r",      0x0220, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* ?? */
	ROM_LOAD( "prom.7h",      0x0240, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* ?? */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "bosco.spr",    0x0000, 0x0100, CRC(ee8ca3a8) SHA1(48d5d9e8c9ca966edad0d5198bb445f6ecceb037) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscomd )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "3n",       0x0000, 0x1000, CRC(441b501a) SHA1(7b4921ff40b3c56950fd32aa0ec5563b02a00929) )
	ROM_LOAD( "3m",       0x1000, 0x1000, CRC(a3c5c7ef) SHA1(70a095a8dbca857245a70404f803916f519e0cbc) )
	ROM_LOAD( "3l",       0x2000, 0x1000, CRC(6ca9a0cf) SHA1(8f70e29beae921e63cd65689a618ca678dd14614) )
	ROM_LOAD( "3k",       0x3000, 0x1000, CRC(d83bacc5) SHA1(cf2fbfa81dabb9b6bcf436d61992e705723776fb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "3j",       0x0000, 0x1000, CRC(4374e39a) SHA1(7571fd5961f49a0e9ba4301ddd0aca52e94e2f8b) )
	ROM_LOAD( "3h",       0x1000, 0x1000, CRC(04e9fcef) SHA1(2115a9718d511854848704e2693f9efa1c80a307) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.1d",      0x0120, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )	/* ?? */
	ROM_LOAD( "prom.2r",      0x0220, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* ?? */
	ROM_LOAD( "prom.7h",      0x0240, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* ?? */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "bosco.spr",    0x0000, 0x0100, CRC(ee8ca3a8) SHA1(48d5d9e8c9ca966edad0d5198bb445f6ecceb037) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END

ROM_START( boscomdo )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for code for the first CPU  */
	ROM_LOAD( "2300.3n",      0x0000, 0x1000, CRC(db6128b0) SHA1(ddd285f7e00d5e58ab9b15838528e0020d47fcd2) )
	ROM_LOAD( "2400.3m",      0x1000, 0x1000, CRC(86907614) SHA1(3295ab6c5171a069875c2239b3325296c1df6031) )
	ROM_LOAD( "2500.3l",      0x2000, 0x1000, CRC(a21fae11) SHA1(dff38d90ee30558274d2d399edc3281c2ef5cb69) )
	ROM_LOAD( "2600.3k",      0x3000, 0x1000, CRC(11d6ae23) SHA1(f2f72f5c777b684f7ffd53b9c034560211113499) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "2700.3j",      0x0000, 0x1000, CRC(7254e65e) SHA1(c2ee29fcb5173e8d46a80a8a1b931a53dbdeae66) )
	ROM_LOAD( "2800.3h",      0x1000, 0x1000, CRC(31b8c648) SHA1(de0db24d385d2361ec989bf32388df8202ad535c) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the third CPU  */
	ROM_LOAD( "2900.3e",      0x0000, 0x1000, CRC(d45a4911) SHA1(547236adca9174f5cc0ec05b9649618bb92ba630) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5300.5d",      0x0000, 0x1000, CRC(a956d3c5) SHA1(c5a9d7b1f9b4acda8fb9762414e085cb5fb80c9e) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "5200.5e",      0x0000, 0x1000, CRC(e869219c) SHA1(425614cd0642743a82ef9c1aada29774a92203ea) )

	ROM_REGION( 0x0100, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "prom.2d",      0x0000, 0x0100, CRC(9b69b543) SHA1(47af3f67e50794e839b74fe61197af2228084efd) )	/* dots */

	ROM_REGION( 0x0260, REGION_PROMS, 0 )
	ROM_LOAD( "bosco.6b",     0x0000, 0x0020, CRC(d2b96fb0) SHA1(54c100ec9d173d7dd48a453ebed5f625053cb6e0) )	/* palette */
	ROM_LOAD( "bosco.4m",     0x0020, 0x0100, CRC(4e15d59c) SHA1(3542ead6421d169c3569e121ec2be304e108787c) )	/* lookup table */
	ROM_LOAD( "prom.1d",      0x0120, 0x0100, CRC(de2316c6) SHA1(0e55c56046331888d1d3f0d9823d2ceb203e7d3f) )	/* ?? */
	ROM_LOAD( "prom.2r",      0x0220, 0x0020, CRC(b88d5ba9) SHA1(7b97a38a540b7ca4b7d9ae338ec38b9b1a337846) )	/* ?? */
	ROM_LOAD( "prom.7h",      0x0240, 0x0020, CRC(87d61353) SHA1(c7493e52662c921625676a4a4e8cf4371bd938b7) )	/* ?? */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound prom */
	ROM_LOAD( "bosco.spr",    0x0000, 0x0100, CRC(ee8ca3a8) SHA1(48d5d9e8c9ca966edad0d5198bb445f6ecceb037) )
	ROM_LOAD( "prom.5c",      0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_SOUND2, 0 )	/* ROMs for digitised speech */
	ROM_LOAD( "4900.5n",      0x0000, 0x1000, CRC(09acc978) SHA1(2b264aaeb6eba70ad91593413dca733990e5467b) )
	ROM_LOAD( "5000.5m",      0x1000, 0x1000, CRC(e571e959) SHA1(9c81d7bec73bc605f7dd9a089171b0f34c4bb09a) )
	ROM_LOAD( "5100.5l",      0x2000, 0x1000, CRC(17ac9511) SHA1(266f3fae90d2fe38d109096d352863a52b379899) )
ROM_END



GAME( 1981, bosco,    0,     bosco, bosco,   0, ROT0, "Namco", "Bosconian (new version)" )
GAME( 1981, boscoo,   bosco, bosco, bosco,   0, ROT0, "Namco", "Bosconian (old version)" )
GAME( 1981, boscoo2,  bosco, bosco, bosco,   0, ROT0, "Namco", "Bosconian (older version)" )
GAME( 1981, boscomd,  bosco, bosco, boscomd, 0, ROT0, "[Namco] (Midway license)", "Bosconian (Midway, new version)" )
GAME( 1981, boscomdo, bosco, bosco, boscomd, 0, ROT0, "[Namco] (Midway license)", "Bosconian (Midway, old version)" )
