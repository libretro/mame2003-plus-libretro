/******************************************************************

Shark Attack
(C) 1980 PACIFIC NOVELTY MFG. INC.

Thief
(C) 1981 PACIFIC NOVELTY MFG. INC.

NATO Defense
(C) 1982 PACIFIC NOVELTY MFG. INC.

Credits:
	Shark Driver by Victor Trucco and Mike Balfour
	Driver for Thief and NATO Defense by Phil Stroffolino

- 8255 emulation (ports 0x30..0x3f) could be better abstracted

- TMS9927 VTAC: do we need to emulate this?
	The video controller registers effect screen size (currently
	hard-coded on a per-game basis).

- minor blitting glitches in playfield of Thief (XOR vs copy?)

- Nato Defense gfx ROMs may be hooked up wrong;
	see screenshots from flyers

******************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "vidhrdw/generic.h"

static UINT8 thief_input_select;

READ_HANDLER( thief_context_ram_r );
WRITE_HANDLER( thief_context_ram_w );
WRITE_HANDLER( thief_context_bank_w );
WRITE_HANDLER( thief_video_control_w );
WRITE_HANDLER( thief_vtcsel_w );
WRITE_HANDLER( thief_color_map_w );
WRITE_HANDLER( thief_color_plane_w );
READ_HANDLER( thief_videoram_r );
WRITE_HANDLER( thief_videoram_w );
WRITE_HANDLER( thief_blit_w );
READ_HANDLER( thief_coprocessor_r );
WRITE_HANDLER( thief_coprocessor_w );

VIDEO_START( thief );
VIDEO_UPDATE( thief );


static INTERRUPT_GEN( thief_interrupt )
{
	/* SLAM switch causes an NMI if it's pressed */
	if( (input_port_3_r(0) & 0x10) == 0 )
		cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
	else
		cpu_set_irq_line(0, 0, HOLD_LINE);
}

/**********************************************************/


/*	Following is an attempt to simulate the behavior of the
**	cassette tape used in several Pacific Novelty games.
**
**	It is a leaderless tape that is constructed so that it will
**	loop continuously.  The IO controller can start and stop the
**	tape player's motor, and enable/disable each of two audio
**	tracks.
*/

enum
{
	kTalkTrack, kCrashTrack
};

static void tape_set_audio( int track, int bOn )
{
	sample_set_volume( track, bOn?100:0 );
}

static void tape_set_motor( int bOn )
{
  if( bOn )
  {
    /* Start if not playing */
    if (!sample_playing( kTalkTrack ))
      sample_start( 0, kTalkTrack, 1 );

    /* Resume */
    sample_set_pause( kTalkTrack, 0 );

    /* Start if not playing */
    if (!sample_playing( kCrashTrack ))
      sample_start( 1, kCrashTrack, 1 );

    /* Resume */
    sample_set_pause( kCrashTrack, 0 );

  }
  else
  {
    /* Pause */
    sample_set_pause( kTalkTrack, 1 );
    sample_set_pause( kCrashTrack, 1 );
  }
}

/***********************************************************/

static WRITE_HANDLER( thief_input_select_w )
{
	thief_input_select = data;
}

static WRITE_HANDLER( tape_control_w )
{
	switch( data )
	{
	case 0x02: /* coin meter on */
		break;

	case 0x03: /* nop */
		break;

	case 0x04: /* coin meter off */
		break;

	case 0x08: /* talk track on */
		tape_set_audio( kTalkTrack, 1 );
		break;

	case 0x09: /* talk track off */
		sample_set_volume( kTalkTrack, 0 );
		break;

	case 0x0a: /* motor on */
		tape_set_motor( 1 );
		break;

	case 0x0b: /* motor off */
		tape_set_motor( 0 );
		break;

	case 0x0c: /* crash track on */
		tape_set_audio( kCrashTrack, 1 );
		break;

	case 0x0d: /* crash track off */
		tape_set_audio( kCrashTrack, 0 );
		break;
	}
}

static READ_HANDLER( thief_io_r )
{
	switch( thief_input_select )
	{
		case 0x01: return readinputport(0); /* dsw#1 */
		case 0x02: return readinputport(1); /* dsw#2 */
		case 0x04: return readinputport(2); /* inp#1 */
		case 0x08: return readinputport(3); /* inp#2 */
	}
	return 0x00;
}

static MEMORY_READ_START( sharkatt_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },			/* 2114 (working RAM) */
	{ 0xc000, 0xdfff, thief_videoram_r },	/* 4116 */
MEMORY_END

static MEMORY_WRITE_START( sharkatt_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },			/* 2114 */
	{ 0xc000, 0xdfff, thief_videoram_w },	/* 4116 */
MEMORY_END

static MEMORY_READ_START( thief_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },			/* 2114 (working RAM) */
	{ 0xa000, 0xafff, MRA_ROM },			/* NATO Defense diagnostic ROM */
	{ 0xc000, 0xdfff, thief_videoram_r },	/* 4116 */
	{ 0xe000, 0xe008, thief_coprocessor_r },
	{ 0xe010, 0xe02f, MRA_ROM },
	{ 0xe080, 0xe0bf, thief_context_ram_r },
MEMORY_END

static MEMORY_WRITE_START( thief_writemem )
	{ 0x0000, 0x0000, thief_blit_w },
	{ 0x0001, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },			/* 2114 */
	{ 0xc000, 0xdfff, thief_videoram_w },	/* 4116 */
	{ 0xe000, 0xe008, thief_coprocessor_w },
	{ 0xe010, 0xe02f, MWA_ROM },
	{ 0xe080, 0xe0bf, thief_context_ram_w },
	{ 0xe0c0, 0xe0c0, thief_context_bank_w },
MEMORY_END

static PORT_READ_START( readport )
	{ 0x31, 0x31, thief_io_r }, /* 8255*/
	{ 0x41, 0x41, AY8910_read_port_0_r },
	{ 0x43, 0x43, AY8910_read_port_1_r },
PORT_END

static PORT_WRITE_START( writeport )
	{ 0x00, 0x00, MWA_NOP }, /* watchdog */
	{ 0x10, 0x10, thief_video_control_w },
	{ 0x30, 0x30, thief_input_select_w }, /* 8255*/
	{ 0x33, 0x33, tape_control_w },
	{ 0x40, 0x40, AY8910_control_port_0_w },
	{ 0x41, 0x41, AY8910_write_port_0_w },
	{ 0x42, 0x42, AY8910_control_port_1_w },
	{ 0x43, 0x43, AY8910_write_port_1_w },
	{ 0x50, 0x50, thief_color_plane_w },
	{ 0x60, 0x6f, thief_vtcsel_w },
	{ 0x70, 0x7f, thief_color_map_w },
PORT_END



/**********************************************************/

INPUT_PORTS_START( sharkatt )
	PORT_START      /* IN0 */
	PORT_DIPNAME( 0x7f, 0x7f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x7f, DEF_STR( 1C_1C ) ) /* if any are set*/
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x01, "4" )
	PORT_DIPSETTING(	0x02, "5" )
/*	PORT_DIPSETTING(	0x03, "5" )*/
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Yes ) )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
INPUT_PORTS_END

INPUT_PORTS_START( thief )
	PORT_START
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x000, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPSETTING(    0x0c, "7" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )

	PORT_START
	PORT_DIPNAME( 0x0f, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00|0x0c, "10K" )
	PORT_DIPSETTING(    0x01|0x0c, "20K" )
	PORT_DIPSETTING(    0x02|0x0c, "30K" )
	PORT_DIPSETTING(    0x03|0x0c, "40K" )
	PORT_DIPSETTING(    0x00|0x08, "10K 10K" )
	PORT_DIPSETTING(    0x01|0x08, "20K 20K" )
	PORT_DIPSETTING(    0x02|0x08, "30K 30K" )
	PORT_DIPSETTING(    0x03|0x08, "40K 40K" )
	PORT_DIPSETTING(    0x00,      "None" )
	PORT_DIPNAME( 0xf0, 0x00, "Mode" )
	PORT_DIPSETTING(    0x00, "Normal" )
	PORT_DIPSETTING(    0x70, "Display Options" )
	PORT_DIPSETTING(    0x80|0x00, "Burn-in Test" )
	PORT_DIPSETTING(    0x80|0x10, "Color Bar Test" )
	PORT_DIPSETTING(    0x80|0x20, "Cross Hatch" )
	PORT_DIPSETTING(    0x80|0x30, "Color Map" )
	PORT_DIPSETTING(    0x80|0x40, "VIDSEL Test" )
	PORT_DIPSETTING(    0x80|0x50, "VIDBIT Test" )
	PORT_DIPSETTING(    0x80|0x60, "I/O Board Test" )
	PORT_DIPSETTING(    0x80|0x70, "Reserved" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( natodef )
	PORT_START
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x000, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPSETTING(    0x0c, "7" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x10, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x30, "Hardest" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, "Add a Coin?" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )

	PORT_START
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0b, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "10K" )
	PORT_DIPSETTING(    0x09, "20K" )
	PORT_DIPSETTING(    0x0a, "30K" )
	PORT_DIPSETTING(    0x0b, "40K" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xf0, 0x00, "Mode" )
	PORT_DIPSETTING(    0x00, "Normal" )
	PORT_DIPSETTING(    0x70, "Display Options" )
	PORT_DIPSETTING(    0x80|0x00, "Burn-in Test" )
	PORT_DIPSETTING(    0x80|0x10, "Color Bar Test" )
	PORT_DIPSETTING(    0x80|0x20, "Cross Hatch" )
	PORT_DIPSETTING(    0x80|0x30, "Color Map" )
	PORT_DIPSETTING(    0x80|0x40, "VIDSEL Test" )
	PORT_DIPSETTING(    0x80|0x50, "VIDBIT Test" )
	PORT_DIPSETTING(    0x80|0x60, "I/O Board Test" )
	PORT_DIPSETTING(    0x80|0x70, "Reserved" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/**********************************************************/

static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	4000000/4,	/* Z80 Clock / 4 */
	{ 50, 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

/***********************************************************/

static const char *sharkatt_sample_names[] =
{
	"*sharkatt",
	"talk.wav",
	"crash.wav",
	0	/* end of array */
};

static struct Samplesinterface sharkatt_samples_interface =
{
	2,	/* number of channels */
	50,	/* volume */
	sharkatt_sample_names
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static const char *thief_sample_names[] =
{
	"*thief",
	"talk.wav",
	"crash.wav",
	0	/* end of array */
};

static struct Samplesinterface thief_samples_interface =
{
	2,	/* number of channels */
	50,	/* volume */
	thief_sample_names
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static const char *natodef_sample_names[] =
{
	"*natodef",
	"talk.wav",
	"crash.wav",
	0	/* end of array */
};

static struct Samplesinterface natodef_samples_interface =
{
	2,	/* number of channels */
	50,	/* volume */
	natodef_sample_names
};



static MACHINE_DRIVER_START( sharkatt )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 4000000)        /* 4 MHz? */
	MDRV_CPU_MEMORY(sharkatt_readmem,sharkatt_writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(thief_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 0*8, 24*8-1)
	MDRV_PALETTE_LENGTH(16)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_VIDEO_START(thief)
	MDRV_VIDEO_UPDATE(thief)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
	MDRV_SOUND_ADD(SAMPLES, sharkatt_samples_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( thief )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 4000000) /* 4 MHz? */
	MDRV_CPU_MEMORY(thief_readmem,thief_writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(thief_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 0*8, 32*8-1)
	MDRV_PALETTE_LENGTH(16)

	MDRV_VIDEO_START(thief)
	MDRV_VIDEO_UPDATE(thief)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
	MDRV_SOUND_ADD(SAMPLES, thief_samples_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( natodef )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 4000000) /* 4 MHz? */
	MDRV_CPU_MEMORY(thief_readmem,thief_writemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(thief_interrupt,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 0*8, 32*8-1)
	MDRV_PALETTE_LENGTH(16)

	MDRV_VIDEO_START(thief)
	MDRV_VIDEO_UPDATE(thief)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
	MDRV_SOUND_ADD(SAMPLES, natodef_samples_interface)
MACHINE_DRIVER_END

/**********************************************************/

ROM_START( sharkatt )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )     /* 64k for code */
	ROM_LOAD( "sharkatt.0",   0x0000, 0x800, CRC(c71505e9) SHA1(068c92e9d797918f281fa509f3c86578b3f0de3a) )
	ROM_LOAD( "sharkatt.1",   0x0800, 0x800, CRC(3e3abf70) SHA1(ef69e72db583a22093a3c32ba437a6eaef4b132a) )
	ROM_LOAD( "sharkatt.2",   0x1000, 0x800, CRC(96ded944) SHA1(e60db225111423b0a481e85fe38a85c3ea844351) )
	ROM_LOAD( "sharkatt.3",   0x1800, 0x800, CRC(007283ae) SHA1(1c311c03729573a4aa6656972e193024364a2f2a) )
	ROM_LOAD( "sharkatt.4a",  0x2000, 0x800, CRC(5cb114a7) SHA1(4240fe1bcc1501b22da133dfb42746b6752b3aea) )
	ROM_LOAD( "sharkatt.5",   0x2800, 0x800, CRC(1d88aaad) SHA1(c81f6d75d88af067f33ff84c417908c450e9e280) )
	ROM_LOAD( "sharkatt.6",   0x3000, 0x800, CRC(c164bad4) SHA1(d72e896bd4b5b0863f2ef8e621e78dd324f9d2c8) )
	ROM_LOAD( "sharkatt.7",   0x3800, 0x800, CRC(d78c4b8b) SHA1(c0371dccfb997331b31893b54fe3c749632dc171) )
	ROM_LOAD( "sharkatt.8",   0x4000, 0x800, CRC(5958476a) SHA1(2063a9721a6eec5049191c69089c3d8cc3064b69) )
	ROM_LOAD( "sharkatt.9",   0x4800, 0x800, CRC(4915eb37) SHA1(56ec2745241afd76aeaa30fb0010cedfd55f307b) )
	ROM_LOAD( "sharkatt.10",  0x5000, 0x800, CRC(9d07cb68) SHA1(528a42e8e7696452bb9d376222f3cbfcb238c01d) )
	ROM_LOAD( "sharkatt.11",  0x5800, 0x800, CRC(21edc962) SHA1(8af23e471b6eb11fc55f331ec97a94e2e6c8be80) )
	ROM_LOAD( "sharkatt.12a", 0x6000, 0x800, CRC(5dd8785a) SHA1(4eaceb781271757c4f4f6f9a4647d394d1912d72) )
ROM_END

ROM_START( thief )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* Z80 code */
	ROM_LOAD( "t8a0ah0a",	0x0000, 0x1000, CRC(edbbf71c) SHA1(9f13841c54fbe5449280c24954a45517014a834e) )
	ROM_LOAD( "t2662h2",	0x1000, 0x1000, CRC(85b4f6ff) SHA1(8e007bfff2f27809e7a9881bc3b2587bf35cff6d) )
	ROM_LOAD( "tc162h4",	0x2000, 0x1000, CRC(70478a82) SHA1(547bad88a44c63657bf8f65f2877ab1323515521) )
	ROM_LOAD( "t0cb4h6",	0x3000, 0x1000, CRC(29de0425) SHA1(6614f3ee314ebf2a6469481e8c69c32a93fa8eb5) )
	ROM_LOAD( "tc707h8",	0x4000, 0x1000, CRC(ea8dd847) SHA1(eab24621abe3735902f03463ee536a0cbfeb7407) )
	ROM_LOAD( "t857bh10",	0x5000, 0x1000, CRC(403c33b7) SHA1(d1422e74c9ecdadbc238b155f853294f6bb83992) )
	ROM_LOAD( "t606bh12",	0x6000, 0x1000, CRC(4ca2748b) SHA1(07df2fac63471d716923f859105421e22e5e970e) )
	ROM_LOAD( "tae4bh14",	0x7000, 0x1000, CRC(22e7dcc3) SHA1(fd4302688905bbd47dfdc1d7cdb55212a5e99f81) ) /* diagnostics ROM */

	ROM_REGION( 0x400, REGION_CPU2, 0 ) /* coprocessor */
	ROM_LOAD( "b8",			0x000, 0x0200, CRC(fe865b2a) SHA1(b29144b05cb2846ea9c868ebf843d74d94c7bcc6) )
	/* B8 is a function dispatch table for the coprocessor (unused) */
	ROM_LOAD( "c8", 		0x200, 0x0200, CRC(7ed5c923) SHA1(35757d50bfa9ea3cf916576a148064a0f9be8732) )
	/* C8 is mapped (banked) in CPU1's address space; it contains Z80 code */

	ROM_REGION( 0x6000, REGION_GFX1, 0 ) /* image ROMs for coprocessor */
	ROM_LOAD16_BYTE( "t079ahd4" ,  0x0001, 0x1000, CRC(928bd8ef) SHA1(3a2de005176ef012c0411d7752a69c03fb165b28) )
	ROM_LOAD16_BYTE( "tdda7hh4" ,  0x0000, 0x1000, CRC(b48f0862) SHA1(c62ccf407e819fe7fa94a4353a17da47b91f0606) )
	/* next 0x4000 bytes are unmapped (used by Nato Defense) */
ROM_END

ROM_START( natodef )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* Z80 code */
	ROM_LOAD( "natodef.cp0",	0x0000, 0x1000, CRC(8397c787) SHA1(5957613f1ace7dc4612f28f6fba3a7374be905ac) )
	ROM_LOAD( "natodef.cp2",	0x1000, 0x1000, CRC(8cfbf26f) SHA1(a15f0d5d82cd96b80ee91dc91858b660c5895f34) )
	ROM_LOAD( "natodef.cp4",	0x2000, 0x1000, CRC(b4c90fb2) SHA1(3ff4691415433863bfe74d51b9f3aa428f3bf88f) )
	ROM_LOAD( "natodef.cp6",	0x3000, 0x1000, CRC(c6d0d35e) SHA1(d4f34b4930be6dc67d77af691d14ee3b797ec29d) )
	ROM_LOAD( "natodef.cp8",	0x4000, 0x1000, CRC(e4b6c21e) SHA1(cfdae66494bc2cc9ee414b9adcf8257b7c69bb40) )
	ROM_LOAD( "natodef.cpa",	0x5000, 0x1000, CRC(888ecd42) SHA1(5af638d7e299046d5803d2764bf42ea44a80374c) )
	ROM_LOAD( "natodef.cpc",	0x6000, 0x1000, CRC(cf713bc9) SHA1(0687755a6cfd76a920c210bf11530ef4c59d92b0) )
	ROM_LOAD( "natodef.cpe",	0x7000, 0x1000, CRC(4eef6bf4) SHA1(ab094198ea4d2267194ace5d382abb78d568983a) )
	ROM_LOAD( "natodef.cp5",	0xa000, 0x1000, CRC(65c3601b) SHA1(c7bf31e6cb781405b3665b3aa93644ed57616256) )	/* diagnostics ROM */

	ROM_REGION( 0x400, REGION_CPU2, 0 ) /* coprocessor */
	ROM_LOAD( "b8",			0x000, 0x0200, CRC(fe865b2a) SHA1(b29144b05cb2846ea9c868ebf843d74d94c7bcc6) )
	ROM_LOAD( "c8", 		0x200, 0x0200, CRC(7ed5c923) SHA1(35757d50bfa9ea3cf916576a148064a0f9be8732) )
	/* C8 is mapped (banked) in CPU1's address space; it contains Z80 code */

	ROM_REGION( 0x6000, REGION_GFX1, 0 ) /* image ROMs for coprocessor */
	ROM_LOAD16_BYTE( "natodef.o4",	0x0001, 0x1000, CRC(39a868f8) SHA1(870795f18cd8f831b714b809a380e30b5d323a5f) )
	ROM_LOAD16_BYTE( "natodef.e1",	0x0000, 0x1000, CRC(b6d1623d) SHA1(0aa15db0e1459a6cc7d2a5bc8e588fd514b71d85) )
	ROM_LOAD16_BYTE( "natodef.o2",	0x2001, 0x1000, CRC(77cc9cfd) SHA1(1bbed3cb834b844fb2d9d48a3a142edaeb33ccc6) )
	ROM_LOAD16_BYTE( "natodef.e3",	0x2000, 0x1000, CRC(5302410d) SHA1(e166c151d948f474c134802e3f891982bf370596) )
	ROM_LOAD16_BYTE( "natodef.o3",	0x4001, 0x1000, CRC(b217909a) SHA1(a26eb5bf2c92d79a75376deb6278710426b34cc5) )
	ROM_LOAD16_BYTE( "natodef.e2",	0x4000, 0x1000, CRC(886c3f05) SHA1(306c8621455d2d6b7b2f545500b27e56a7159a1b) )
ROM_END

ROM_START( natodefa )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* Z80 code */
	ROM_LOAD( "natodef.cp0",	0x0000, 0x1000, CRC(8397c787) SHA1(5957613f1ace7dc4612f28f6fba3a7374be905ac) )
	ROM_LOAD( "natodef.cp2",	0x1000, 0x1000, CRC(8cfbf26f) SHA1(a15f0d5d82cd96b80ee91dc91858b660c5895f34) )
	ROM_LOAD( "natodef.cp4",	0x2000, 0x1000, CRC(b4c90fb2) SHA1(3ff4691415433863bfe74d51b9f3aa428f3bf88f) )
	ROM_LOAD( "natodef.cp6",	0x3000, 0x1000, CRC(c6d0d35e) SHA1(d4f34b4930be6dc67d77af691d14ee3b797ec29d) )
	ROM_LOAD( "natodef.cp8",	0x4000, 0x1000, CRC(e4b6c21e) SHA1(cfdae66494bc2cc9ee414b9adcf8257b7c69bb40) )
	ROM_LOAD( "natodef.cpa",	0x5000, 0x1000, CRC(888ecd42) SHA1(5af638d7e299046d5803d2764bf42ea44a80374c) )
	ROM_LOAD( "natodef.cpc",	0x6000, 0x1000, CRC(cf713bc9) SHA1(0687755a6cfd76a920c210bf11530ef4c59d92b0) )
	ROM_LOAD( "natodef.cpe",	0x7000, 0x1000, CRC(4eef6bf4) SHA1(ab094198ea4d2267194ace5d382abb78d568983a) )
	ROM_LOAD( "natodef.cp5",	0xa000, 0x1000, CRC(65c3601b) SHA1(c7bf31e6cb781405b3665b3aa93644ed57616256) )	/* diagnostics ROM */

	ROM_REGION( 0x400, REGION_CPU2, 0 ) /* coprocessor */
	ROM_LOAD( "b8",			0x000, 0x0200, CRC(fe865b2a) SHA1(b29144b05cb2846ea9c868ebf843d74d94c7bcc6) )
	ROM_LOAD( "c8", 		0x200, 0x0200, CRC(7ed5c923) SHA1(35757d50bfa9ea3cf916576a148064a0f9be8732) )
	/* C8 is mapped (banked) in CPU1's address space; it contains Z80 code */

	ROM_REGION( 0x6000, REGION_GFX1, 0 ) /* image ROMs for coprocessor */
	ROM_LOAD16_BYTE( "natodef.o4",	0x0001, 0x1000, CRC(39a868f8) SHA1(870795f18cd8f831b714b809a380e30b5d323a5f) )
	ROM_LOAD16_BYTE( "natodef.e1",	0x0000, 0x1000, CRC(b6d1623d) SHA1(0aa15db0e1459a6cc7d2a5bc8e588fd514b71d85) )
	ROM_LOAD16_BYTE( "natodef.o3",	0x2001, 0x1000, CRC(b217909a) SHA1(a26eb5bf2c92d79a75376deb6278710426b34cc5) ) /* same ROMs as natodef, */
	ROM_LOAD16_BYTE( "natodef.e2",	0x2000, 0x1000, CRC(886c3f05) SHA1(306c8621455d2d6b7b2f545500b27e56a7159a1b) ) /* but in a different */
	ROM_LOAD16_BYTE( "natodef.o2",	0x4001, 0x1000, CRC(77cc9cfd) SHA1(1bbed3cb834b844fb2d9d48a3a142edaeb33ccc6) ) /* order to give */
	ROM_LOAD16_BYTE( "natodef.e3",	0x4000, 0x1000, CRC(5302410d) SHA1(e166c151d948f474c134802e3f891982bf370596) ) /* different mazes */
ROM_END


static DRIVER_INIT( thief )
{
	UINT8 *dest = memory_region( REGION_CPU1 );
	const UINT8 *source = memory_region( REGION_CPU2 );

	/* C8 is mapped (banked) in CPU1's address space; it contains Z80 code */
	memcpy( &dest[0xe010], &source[0x290], 0x20 );
}


GAME( 1980, sharkatt, 0,       sharkatt, sharkatt, 0,     ROT0, "Pacific Novelty", "Shark Attack" )
GAME( 1981, thief,    0,       thief,    thief,    thief, ROT0, "Pacific Novelty", "Thief" )
GAME( 1982, natodef,  0,       natodef,  natodef,  thief, ROT0, "Pacific Novelty", "NATO Defense"  )
GAME( 1982, natodefa, natodef, natodef,  natodef,  thief, ROT0, "Pacific Novelty", "NATO Defense (alternate mazes)"  )
