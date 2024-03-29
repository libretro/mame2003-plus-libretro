/***************************************************************************

Atari Fire Truck + Super Bug + Monte Carlo driver

***************************************************************************/

#include "driver.h"
#include "firetrk.h"
#include "artwork.h"

/* from src\sndhrdw\ataridis.c */
extern struct discrete_sound_block firetrk_sound_interface[];
extern struct discrete_sound_block superbug_sound_interface[];
extern struct discrete_sound_block montecar_sound_interface[];
int firetrk_game;

static int steer_dir[2];
static int steer_flag[2];
static int attract;
static int gear;

#define OVERLAY_GREEN           MAKE_ARGB(0x04,0x20,0xff,0x20)
#define OVERLAY_YELLOW          MAKE_ARGB(0x04,0xff,0xff,0x20)

/* Overlay comes from these sources: */
/* https://www.youtube.com/watch?v=qLV6xV3rhbE */
/* https://www.youtube.com/watch?v=N61Lacr9IGs */
/* https://flyers.arcade-museum.com/?page=flyer&db=videodb&id=3856&image=2 */
/* The overlay on the car is originally slightly spikey and larger */
/* than the car, but this is really larger due to difficulty */
/* putting physical overlays in the right spot on a monitor. */
/* We do a little better here by making it just fit, so */
/* there is less bleed close to edges when driving. */
OVERLAY_START( superbug_overlay )
    OVERLAY_RECT( 0, 0, 4*8, 30*8, OVERLAY_GREEN )
    OVERLAY_RECT( 36*8, 0, 40*8, 30*8, OVERLAY_GREEN )
    OVERLAY_DISK( 20*8, 15*8, 2*8+1, OVERLAY_YELLOW )
OVERLAY_END

static DRIVER_INIT( firetrk )
{
	firetrk_game = 1;
	/* Note that fire truck does not have an overlay */
	/* on the real machine. It was purely B&W. */
}

static DRIVER_INIT( superbug )
{
	firetrk_game = 2;
	artwork_set_overlay(superbug_overlay);
}

static DRIVER_INIT( montecar )
{
	firetrk_game = 3;
}


static INTERRUPT_GEN( firetrk_interrupt )
{
	/* interrupts are disabled during service mode */

	if (GAME_IS_FIRETRUCK)
	{
		if (readinputport(4) & 0x80)
			return;
	}
	if (GAME_IS_MONTECARLO)
	{
		if (readinputport(6) & 0x04)
			return;
	}
	if (GAME_IS_SUPERBUG)
	{
		discrete_sound_w(7, 0);	/* ASR */
	}

	cpu_set_nmi_line(0, PULSE_LINE);
}


static void periodic_callback(int scanline)
{
	cpu_set_irq_line(0, 0, PULSE_LINE);

	/* IRQs are generated by inverse 16V signal */

	scanline += 32;

	if (scanline > 256)
	{
		scanline = 0;
	}

	timer_set(cpu_getscanlinetime(scanline), scanline, periodic_callback);
}


static void frame_callback(int dummy)
{
	static UINT8 dial[2];

	int i;

	/* handle steering wheels */

	for (i = 0; i < 2; i++)
	{
		signed char delta = readinputport(i) - dial[i];

		if (delta < 0)
		{
			steer_flag[i] = 1;
			steer_dir[i] = 0;
		}
		if (delta > 0)
		{
			steer_flag[i] = 1;
			steer_dir[i] = 1;
		}

		dial[i] += delta;
	}


	if (GAME_IS_FIRETRUCK)
	{
		/* watchdog is disabled during service mode */
		if (readinputport(4) & 0x80)
		{
			watchdog_reset_w(0, 0);
		}

		/* map horn button onto discrete sound emulation */
		discrete_sound_w(1, readinputport(7));
	}

	/* update gear shift */

	if (GAME_IS_SUPERBUG || GAME_IS_MONTECARLO)
	{
		switch (readinputport(7) & 15)
		{
		case 1: gear = 1; break;
		case 2: gear = 2; break;
		case 4: gear = 3; break;
		case 8: gear = 4; break;
		}
	}
}


static void write_output(UINT8 flags)
{
	if (GAME_IS_FIRETRUCK)
	{
		/* BIT0 => START1 LAMP */
		/* BIT1 => START2 LAMP */
		/* BIT2 => FLASH       */
		/* BIT3 => TRACK LAMP  */
		/* BIT4 => ATTRACT     */
		/* BIT5 => START3 LAMP */
		/* BIT6 => UNUSED      */
		/* BIT7 => BELL OUT    */

		set_led_status(0, !(flags & 0x01));
		set_led_status(1, !(flags & 0x02));
		set_led_status(2, !(flags & 0x20));
		set_led_status(3, !(flags & 0x08));

		attract = flags & 0x10;

		discrete_sound_w(5, (flags & 0x80) ? 1 : 0);	/* Bell Sound */
		discrete_sound_w(6, (flags & 0x10) ? 1 : 0);	/* Attract */

		coin_lockout_w(0, !attract);
		coin_lockout_w(1, !attract);

		firetrk_set_flash(flags & 0x04);
	}

	if (GAME_IS_SUPERBUG)
	{
		/* BIT0 => START LAMP */
		/* BIT1 => ATTRACT    */
		/* BIT2 => FLASH      */
		/* BIT3 => TRACK LAMP */

		attract = flags & 0x02;

		set_led_status(0, flags & 0x01);
		set_led_status(1, flags & 0x08);

		discrete_sound_w(6, (flags & 0x02) ? 1 : 0);	/* Attract */

		coin_lockout_w(0, !attract);
		coin_lockout_w(1, !attract);

		firetrk_set_flash(flags & 0x04);
	}

	if (GAME_IS_MONTECARLO)
	{
		/* BIT0 => START LAMP    */
		/* BIT1 => TRACK LAMP    */
		/* BIT2 => ATTRACT       */
		/* BIT3 => UNUSED        */
		/* BIT4 => UNUSED        */
		/* BIT5 => COIN3 COUNTER */
		/* BIT6 => COIN2 COUNTER */
		/* BIT7 => COIN1 COUNTER */

		set_led_status(0, !(flags & 0x01));
		set_led_status(1, !(flags & 0x02));

		discrete_sound_w(6, (flags & 0x04) ? 1 : 0);	/* Attract */

		coin_counter_w(0, flags & 0x80);
		coin_counter_w(1, flags & 0x40);
		coin_counter_w(2, flags & 0x20);
	}
}


static MACHINE_INIT( firetrk )
{
	timer_pulse(1. / 60, 0, frame_callback);

	if (GAME_IS_MONTECARLO)
	{
		write_output(0);
	}

	timer_set(0, 0, periodic_callback);
}


static PALETTE_INIT( firetrk )
{
	static const UINT16 colortable_source[] =
	{
		0, 0, 1, 0,
		2, 0, 3, 0,
		3, 3, 2, 3,
		1, 3, 0, 3,
		0, 0, 1, 0,
		2, 0, 0, 3,
		3, 0, 0, 3
	};

	palette_set_color(0, 0x00, 0x00, 0x00);
	palette_set_color(1, 0x5b, 0x5b, 0x5b);
	palette_set_color(2, 0xa4, 0xa4, 0xa4);
	palette_set_color(3, 0xff, 0xff, 0xff);

	memcpy(colortable, colortable_source, sizeof(colortable_source));
}


static void prom_to_palette(int number, UINT8 val)
{
	palette_set_color(number,
		(val & 4) ? 0xff : 0x00,
		(val & 2) ? 0xff : 0x00,
		(val & 1) ? 0xff : 0x00);
}


static PALETTE_INIT( montecar )
{
	static const UINT16 colortable_source[] =
	{
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x02, 0x00, 0x03,
		0x03, 0x03, 0x03, 0x02,
		0x03, 0x01, 0x03, 0x00,
		0x00, 0x00, 0x02, 0x00,
		0x02, 0x01, 0x02, 0x02,
		0x00, 0x05, 0x06, 0x07,
		0x00, 0x09, 0x0A, 0x0B,
		0x00, 0x0D, 0x0E, 0x0F,
		0x00, 0x11, 0x12, 0x13,
		0x00, 0x15, 0x16, 0x17,
		0x18, 0x19
	};

	/*
	 * The color PROM is addressed as follows:
	 *
	 *   A0 => PLAYFIELD 1
	 *   A1 => PLAYFIELD 2
	 *   A2 => DRONE 1
	 *   A3 => DRONE 2
	 *   A4 => CAR 1
	 *   A5 => CAR 2
	 *   A6 => DRONE COLOR 1
	 *   A7 => DRONE COLOR 2
	 *   A8 => PLAYFIELD WINDOW
	 *
	 * This driver hard-codes some behavior which actually depends
	 * on the PROM, like priorities, clipping and transparency.
	 *
	 */

	const UINT8* p = memory_region(REGION_PROMS);

	int number = 0;

	prom_to_palette(number++, p[0x100]);
	prom_to_palette(number++, p[0x101]);
	prom_to_palette(number++, p[0x102]);
	prom_to_palette(number++, p[0x103]);

	prom_to_palette(number++, p[0x100]);
	prom_to_palette(number++, p[0x110]);
	prom_to_palette(number++, p[0x120]);
	prom_to_palette(number++, p[0x130]);

	prom_to_palette(number++, p[0x100]);
	prom_to_palette(number++, p[0x104]);
	prom_to_palette(number++, p[0x108]);
	prom_to_palette(number++, p[0x10C]);

	prom_to_palette(number++, p[0x140]);
	prom_to_palette(number++, p[0x144]);
	prom_to_palette(number++, p[0x148]);
	prom_to_palette(number++, p[0x14C]);

	prom_to_palette(number++, p[0x180]);
	prom_to_palette(number++, p[0x184]);
	prom_to_palette(number++, p[0x188]);
	prom_to_palette(number++, p[0x18C]);

	prom_to_palette(number++, p[0x1C0]);
	prom_to_palette(number++, p[0x1C4]);
	prom_to_palette(number++, p[0x1C8]);
	prom_to_palette(number++, p[0x1CC]);

	palette_set_color(number++, 0x00, 0x00, 0x00);
	palette_set_color(number++, 0xff, 0xff, 0xff);

	memcpy(colortable, colortable_source, sizeof(colortable_source));
}


static READ_HANDLER( firetrk_zeropage_r )
{
	return memory_region(REGION_CPU1)[offset & 0xff];
}


static READ_HANDLER( firetrk_playfield_r )
{
	return firetrk_playfield_ram[offset & 0xff];
}


static READ_HANDLER( firetrk_dip_r )
{
	UINT8 val0 = readinputport(2);
	UINT8 val1 = readinputport(3);

	if (GAME_IS_FIRETRUCK || GAME_IS_SUPERBUG)
	{
		if (val1 & (1 << (2 * offset + 0))) val0 |= 1;
		if (val1 & (1 << (2 * offset + 1))) val0 |= 2;
	}
	if (GAME_IS_MONTECARLO)
	{
		if (val1 & (1 << (3 - offset))) val0 |= 1;
		if (val1 & (1 << (7 - offset))) val0 |= 2;
	}

	return val0;
}


static READ_HANDLER( firetrk_input_r )
{
	UINT8 val = 0;

	UINT8 bit0 = readinputport(4);
	UINT8 bit6 = readinputport(5);
	UINT8 bit7 = readinputport(6);

	if (GAME_IS_FIRETRUCK)
	{
		if (!steer_dir[0])
			bit0 |= 0x04;
		if (!steer_flag[0])
			bit7 |= 0x04;
		if (firetrk_skid[0] || firetrk_skid[1])
			bit0 |= 0x40;
		if (firetrk_crash[0] || firetrk_crash[1])
			bit7 |= 0x40;
		if (!steer_dir[1])
			bit0 |= 0x08;
		if (!steer_flag[1])
			bit7 |= 0x08;
	}

	if (GAME_IS_SUPERBUG)
	{
		if (!steer_dir[0])
			bit0 |= 0x04;
		if (!steer_flag[0])
			bit7 |= 0x04;
		if (firetrk_skid[0])
			bit0 |= 0x40;
		if (firetrk_crash[0])
			bit7 |= 0x40;
		if (gear == 1)
			bit7 |= 0x02;
		if (gear == 2)
			bit0 |= 0x01;
		if (gear == 3)
			bit7 |= 0x01;
	}

	if (GAME_IS_MONTECARLO)
	{
		if (!steer_dir[0])
			bit6 |= 0x40;
		if (!steer_flag[0])
			bit7 |= 0x40;
		if (gear == 1)
			bit6 |= 0x01;
		if (gear == 2)
			bit6 |= 0x02;
		if (gear == 3)
			bit6 |= 0x04;
		if (firetrk_skid[0])
			bit7 |= 0x80;
		if (firetrk_skid[1])
			bit6 |= 0x80;
		if (firetrk_crash[0])
			val |= 0x02;
		if (firetrk_crash[1])
			val |= 0x01;
	}

	if (bit0 & (1 << offset)) val |= 0x01;
	if (bit6 & (1 << offset)) val |= 0x40;
	if (bit7 & (1 << offset)) val |= 0x80;

	return val;
}


static WRITE_HANDLER( firetrk_zeropage_w )
{
	memory_region(REGION_CPU1)[offset & 0xff] = data;
}


static WRITE_HANDLER( firetrk_arrow_off_w )
{
	firetrk_set_blink(1);
}


static WRITE_HANDLER( firetrk_car_reset_w )
{
	firetrk_crash[0] = 0;
	firetrk_skid[0] = 0;
}


static WRITE_HANDLER( firetrk_drone_reset_w )
{
	firetrk_crash[1] = 0;
	firetrk_skid[1] = 0;
}


static WRITE_HANDLER( firetrk_steer_reset_w )
{
	steer_flag[0] = 0;
	steer_flag[1] = 0;
}


static WRITE_HANDLER( firetrk_crash_reset_w )
{
	firetrk_crash[0] = 0;
	firetrk_crash[1] = 0;
}


static WRITE_HANDLER( firetrk_skid_reset_w )
{
	if (GAME_IS_FIRETRUCK || GAME_IS_SUPERBUG)
	{
		firetrk_skid[0] = 0;
		firetrk_skid[1] = 0;
	}

	discrete_sound_w(4, 1);
}


static WRITE_HANDLER( firetrk_crash_snd_w )
{
	/* invert data here to make life easier on the sound system */
	discrete_sound_w(3, ((~data) >> 4)& 0x0f);
}


static WRITE_HANDLER( firetrk_skid_snd_w )
{
	discrete_sound_w(4, 0);
}


static WRITE_HANDLER( firetrk_motor_snd_w )
{
	if (GAME_IS_FIRETRUCK || GAME_IS_MONTECARLO)
	{
		discrete_sound_w(2, data / 16);		/* Fire Truck - Siren frequency */
							/* Monte Carlo - Drone Motor frequency */
	}

	if (GAME_IS_SUPERBUG)
		discrete_sound_w(0, (~data) & 0x0f);    /* motor frequency */
	else
	discrete_sound_w(0, data & 0x0f);    /* motor frequency */
}


static WRITE_HANDLER( firetrk_xtndply_w )
{
	discrete_sound_w(7, !(data & 1));
}


static WRITE_HANDLER( firetrk_out_w )
{
	if (GAME_IS_FIRETRUCK || GAME_IS_MONTECARLO)
	{
		write_output(data);
	}
	if (GAME_IS_SUPERBUG)
	{
		write_output(offset);
	}
}


static WRITE_HANDLER( firetrk_out2_w )
{
	firetrk_set_flash(data & 0x80);

	if (GAME_IS_MONTECARLO)
	{
		discrete_sound_w(7, !(data & 0x10));	/* Beep */
		discrete_sound_w(5, data & 0x0f);	/* Drone Motor Volume */
	}
}


static WRITE_HANDLER( firetrk_asr_w )
{
	if (GAME_IS_SUPERBUG)
	{
		discrete_sound_w(7, 1);	/* ASR */
	}
}


static MEMORY_READ_START( firetrk_readmem )
	{ 0x0000, 0x00ff, MRA_RAM },
	{ 0x0100, 0x07ff, firetrk_zeropage_r },
	{ 0x0800, 0x08ff, MRA_RAM },
	{ 0x0900, 0x0fff, firetrk_playfield_r },
	{ 0x1800, 0x1807, firetrk_input_r },
	{ 0x1c00, 0x1c03, firetrk_dip_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },
MEMORY_END


static MEMORY_WRITE_START( firetrk_writemem )
	{ 0x0000, 0x00ff, MWA_RAM, &firetrk_alpha_num_ram },
	{ 0x0100, 0x07ff, firetrk_zeropage_w },
	{ 0x0800, 0x0fff, firetrk_playfield_w, &firetrk_playfield_ram },
	{ 0x1000, 0x1000, firetrk_vert_w },
	{ 0x1020, 0x1020, firetrk_horz_w },
	{ 0x1040, 0x104f, firetrk_crash_reset_w },
	{ 0x1060, 0x1060, firetrk_skid_reset_w },
	{ 0x1080, 0x1080, firetrk_car_rot_w },
	{ 0x10a0, 0x10a0, firetrk_steer_reset_w },
	{ 0x10c0, 0x10c0, watchdog_reset_w },
	{ 0x10e0, 0x10e0, firetrk_arrow_off_w },
	{ 0x1400, 0x1400, firetrk_motor_snd_w },
	{ 0x1420, 0x1420, firetrk_crash_snd_w },
	{ 0x1440, 0x1440, firetrk_skid_snd_w },
	{ 0x1460, 0x1460, firetrk_drone_hpos_w },
	{ 0x1480, 0x1480, firetrk_drone_vpos_w },
	{ 0x14a0, 0x14a0, firetrk_drone_rot_w },
	{ 0x14c0, 0x14c0, firetrk_out_w },
	{ 0x14e0, 0x14e0, firetrk_xtndply_w },
	{ 0x1800, 0x1807, MWA_NOP },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_ROM },
MEMORY_END


static MEMORY_READ_START( superbug_readmem )
	{ 0x0000, 0x00ff, MRA_RAM },
	{ 0x0200, 0x0207, firetrk_input_r },
	{ 0x0240, 0x0243, firetrk_dip_r },
	{ 0x0400, 0x041f, MRA_RAM },
	{ 0x0500, 0x05ff, MRA_RAM },
	{ 0x0800, 0x1fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },
MEMORY_END


static MEMORY_WRITE_START( superbug_writemem )
	{ 0x0000, 0x00ff, MWA_RAM },
	{ 0x0100, 0x0100, firetrk_vert_w },
	{ 0x0120, 0x0120, firetrk_horz_w },
	{ 0x0140, 0x0140, firetrk_crash_reset_w },
	{ 0x0160, 0x0160, firetrk_skid_reset_w },
	{ 0x0180, 0x0180, firetrk_car_rot_w },
	{ 0x01a0, 0x01a0, firetrk_steer_reset_w },
	{ 0x01c0, 0x01c0, watchdog_reset_w },
	{ 0x01e0, 0x01e0, firetrk_arrow_off_w },
	{ 0x0220, 0x0220, firetrk_asr_w },
	{ 0x0260, 0x026f, firetrk_out_w },
	{ 0x0280, 0x0280, firetrk_motor_snd_w },
	{ 0x02a0, 0x02a0, firetrk_crash_snd_w },
	{ 0x02c0, 0x02c0, firetrk_skid_snd_w },
	{ 0x0400, 0x041f, MWA_RAM, &firetrk_alpha_num_ram },
	{ 0x0500, 0x05ff, firetrk_playfield_w, &firetrk_playfield_ram },
	{ 0x0800, 0x1fff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_ROM },
MEMORY_END


static MEMORY_READ_START( montecar_readmem )
	{ 0x0000, 0x00ff, MRA_RAM },
	{ 0x0100, 0x07ff, firetrk_zeropage_r },
	{ 0x0800, 0x08ff, MRA_RAM },
	{ 0x0900, 0x0fff, firetrk_playfield_r },
	{ 0x1800, 0x1807, firetrk_input_r },
	{ 0x1c00, 0x1c03, firetrk_dip_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },
MEMORY_END


static MEMORY_WRITE_START( montecar_writemem )
	{ 0x0000, 0x00ff, MWA_RAM, &firetrk_alpha_num_ram },
	{ 0x0100, 0x07ff, firetrk_zeropage_w },
	{ 0x0800, 0x0fff, firetrk_playfield_w, &firetrk_playfield_ram },
	{ 0x1000, 0x1000, firetrk_vert_w },
	{ 0x1020, 0x1020, firetrk_horz_w },
	{ 0x1040, 0x1040, firetrk_drone_reset_w },
	{ 0x1060, 0x1060, firetrk_car_reset_w },
	{ 0x1080, 0x1080, firetrk_car_rot_w },
	{ 0x10a0, 0x10a0, firetrk_steer_reset_w },
	{ 0x10c0, 0x10c0, watchdog_reset_w },
	{ 0x10e0, 0x10e0, firetrk_skid_reset_w },
	{ 0x1400, 0x1400, firetrk_motor_snd_w },
	{ 0x1420, 0x1420, firetrk_crash_snd_w },
	{ 0x1440, 0x1440, firetrk_skid_snd_w },
	{ 0x1460, 0x1460, firetrk_drone_hpos_w },
	{ 0x1480, 0x1480, firetrk_drone_vpos_w },
	{ 0x14a0, 0x14a0, firetrk_drone_rot_w },
	{ 0x14c0, 0x14c0, firetrk_out_w },
	{ 0x14e0, 0x14e0, firetrk_out2_w },
	{ 0x1800, 0x1807, MWA_NOP },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_ROM },
MEMORY_END


INPUT_PORTS_START( firetrk )
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER1, 25, 10, 0, 0 )

	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER2, 25, 10, 0, 0 )

	PORT_START
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED) /* other DIPs connect here */
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_UNUSED) /* other DIPs connect here */
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Coinage ))
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unused ))
	PORT_DIPSETTING(    0x10, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ))
	PORT_DIPSETTING(    0x20, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

	PORT_START
	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x01, "French" )
	PORT_DIPSETTING(    0x02, "Spanish" )
	PORT_DIPSETTING(    0x03, "German" )
	PORT_DIPNAME( 0x0c, 0x04, "Play Time" )
	PORT_DIPSETTING(    0x00, "60 Seconds" )
	PORT_DIPSETTING(    0x04, "90 Seconds" )
	PORT_DIPSETTING(    0x08, "120 Seconds" )
	PORT_DIPSETTING(    0x0c, "150 Seconds" )
	PORT_DIPNAME( 0x30, 0x20, "Extended Play" )
	PORT_DIPSETTING(    0x10, "Liberal" )
	PORT_DIPSETTING(    0x20, "Medium" )
	PORT_DIPSETTING(    0x30, "Conservative" )
	PORT_DIPSETTING(    0x00, "Never" )

	PORT_START /* bit 0 */
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1, "Gas", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BIT (0x04, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER DIR 1 */
	PORT_BIT (0x08, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER DIR 2 */
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2, "Bell", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BIT (0x20, IP_ACTIVE_HIGH, IPT_TILT)
	PORT_BIT (0x40, IP_ACTIVE_HIGH, IPT_UNUSED) /* SKID */
	PORT_SERVICE(0x80, IP_ACTIVE_HIGH)

	PORT_START /* bit 6 */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_START1, "Front Player Start", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_START2, "Back Player Start", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BITX(0x04, IP_ACTIVE_HIGH, IPT_START3, "Both Players Start", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER1, "Track Select", KEYCODE_SPACE, IP_JOY_DEFAULT)
	PORT_BIT (0x10, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT (0x20, IP_ACTIVE_LOW,  IPT_VBLANK)
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ))
	PORT_DIPSETTING(    0x00, "Smokey Joe (1 Player)" )
	PORT_DIPSETTING(    0x40, "Fire Truck (2 Players)" )
	PORT_BITX(0x80, IP_ACTIVE_HIGH,	IPT_SERVICE, "Diag Hold", KEYCODE_NONE, IP_JOY_NONE)

	PORT_START /* bit 7 */
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT (0x04, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER FLAG 1 */
	PORT_BIT (0x08, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER FLAG 2 */
	PORT_BIT (0x10, IP_ACTIVE_HIGH, IPT_COIN1)
	PORT_BIT (0x20, IP_ACTIVE_HIGH, IPT_COIN2)
	PORT_BIT (0x40, IP_ACTIVE_HIGH, IPT_UNUSED) /* CRASH */
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_SERVICE, "Diag Step", KEYCODE_F1, IP_JOY_NONE)

	PORT_START
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1, "Horn", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_START		/* 8 */
	PORT_ADJUSTER( 85, "Motor Frequency" )
INPUT_PORTS_END


INPUT_PORTS_START( superbug )
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 25, 10, 0, 0 )

	PORT_START
	PORT_BIT (0xff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED) /* other DIPs connect here */
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_UNUSED) /* other DIPs connect here */

	PORT_START
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Coinage ))
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0c, 0x04, "Play Time" )
	PORT_DIPSETTING(    0x00, "60 seconds" )
	PORT_DIPSETTING(    0x04, "90 seconds" )
	PORT_DIPSETTING(    0x08, "120 seconds" )
	PORT_DIPSETTING(    0x0c, "150 seconds" )
	PORT_DIPNAME( 0x30, 0x20, "Extended Play" )
	PORT_DIPSETTING(    0x10, "Liberal" )
	PORT_DIPSETTING(    0x20, "Medium" )
	PORT_DIPSETTING(    0x30, "Conservative" )
	PORT_DIPSETTING(    0x00, "Never" )
	PORT_DIPNAME( 0xc0, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "French" )
	PORT_DIPSETTING(    0x80, "Spanish" )
	PORT_DIPSETTING(    0xc0, "German" )

	PORT_START /* bit 0 */
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED) /* GEAR 2 */
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON1, "Gas", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BIT (0x04, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER DIR */
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON7, "Hiscore Reset", KEYCODE_H, IP_JOY_DEFAULT)
	PORT_BIT (0x10, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_SERVICE( 0x20, IP_ACTIVE_HIGH)
	PORT_BIT (0x40, IP_ACTIVE_HIGH, IPT_UNUSED) /* SKID */
	PORT_BIT (0x80, IP_ACTIVE_HIGH, IPT_TILT)

	PORT_START /* bit 6 */
	PORT_BIT(0xff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START /* bit 7 */
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED) /* GEAR 3 */
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_UNUSED) /* GEAR 1 */
	PORT_BIT (0x04, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER FLAG */
	PORT_BIT (0x08, IP_ACTIVE_HIGH, IPT_COIN1)
	PORT_BIT (0x10, IP_ACTIVE_HIGH, IPT_COIN2)
	PORT_BIT (0x20, IP_ACTIVE_HIGH, IPT_START1)
	PORT_BIT (0x40, IP_ACTIVE_HIGH, IPT_UNUSED) /* CRASH */
	PORT_BITX(0x80, IP_ACTIVE_HIGH, IPT_BUTTON6, "Track Select", KEYCODE_SPACE, IP_JOY_DEFAULT )

	PORT_START
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_BUTTON2, "Gear 1", KEYCODE_Z, IP_JOY_DEFAULT )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON3, "Gear 2", KEYCODE_X, IP_JOY_DEFAULT )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, IPT_BUTTON4, "Gear 3", KEYCODE_C, IP_JOY_DEFAULT )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON5, "Gear 4", KEYCODE_V, IP_JOY_DEFAULT )
	PORT_START		/* 8 */
	PORT_ADJUSTER( 85, "Motor Frequency" )
INPUT_PORTS_END


INPUT_PORTS_START( montecar )
	PORT_START
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER1, 25, 10, 0, 0 )

	PORT_START
	PORT_BIT (0xff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED) /* other DIPs connect here */
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_UNUSED) /* other DIPs connect here */
	PORT_DIPNAME( 0x0c, 0x0c, "Coin 3 Multiplier" )
	PORT_DIPSETTING(    0x0c, "1" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x10, 0x10, "Coin 2 Multiplier" )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unused ))
	PORT_DIPSETTING(    0x20, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))

	PORT_START
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Coinage ))
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(    0x03, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x0c, 0x08, "Extended Play" )
	PORT_DIPSETTING(    0x04, "Liberal" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x00, "Conservative" )
	PORT_DIPSETTING(    0x0c, "Never" )
	PORT_DIPNAME( 0x30, 0x20, "Play Time" )
	PORT_DIPSETTING(    0x30, "60 Seconds" )
	PORT_DIPSETTING(    0x10, "90 Seconds" )
	PORT_DIPSETTING(    0x20, "120 Seconds" )
	PORT_DIPSETTING(    0x00, "150 Seconds" )
	PORT_DIPNAME( 0xc0, 0xc0, "Language" )
	PORT_DIPSETTING(    0xc0, "English" )
	PORT_DIPSETTING(    0x80, "Spanish" )
	PORT_DIPSETTING(    0x40, "French" )
	PORT_DIPSETTING(    0x00, "German" )

	PORT_START /* bit 0 */
	PORT_BIT(0xff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START /* bit 6 */
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_UNUSED) /* GEAR 1 */
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_UNUSED) /* GEAR 2 */
	PORT_BIT (0x04, IP_ACTIVE_HIGH, IPT_UNUSED) /* GEAR 3 */
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON6, "Track Select", KEYCODE_SPACE, IP_JOY_DEFAULT )
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_BUTTON1, "Gas", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT (0x20, IP_ACTIVE_HIGH, IPT_VBLANK)
	PORT_BIT (0x40, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER DIR */
	PORT_BIT (0x80, IP_ACTIVE_HIGH, IPT_UNUSED) /* SKID 1 */

	PORT_START /* bit 7 */
	PORT_BIT (0x01, IP_ACTIVE_HIGH, IPT_START1)
	PORT_BIT (0x02, IP_ACTIVE_HIGH, IPT_TILT)
	PORT_SERVICE(0x04, IP_ACTIVE_HIGH)
	PORT_BIT (0x08, IP_ACTIVE_HIGH, IPT_COIN1)
	PORT_BIT (0x10, IP_ACTIVE_HIGH, IPT_COIN2)
	PORT_BIT (0x20, IP_ACTIVE_HIGH, IPT_COIN3)
	PORT_BIT (0x40, IP_ACTIVE_HIGH, IPT_UNUSED) /* STEER FLAG */
	PORT_BIT (0x80, IP_ACTIVE_HIGH, IPT_UNUSED) /* SKID 0 */

	PORT_START
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_BUTTON2, "Gear 1", KEYCODE_Z, IP_JOY_DEFAULT )
	PORT_BITX(0x02, IP_ACTIVE_HIGH, IPT_BUTTON3, "Gear 2", KEYCODE_X, IP_JOY_DEFAULT )
	PORT_BITX(0x04, IP_ACTIVE_HIGH, IPT_BUTTON4, "Gear 3", KEYCODE_C, IP_JOY_DEFAULT )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_BUTTON5, "Gear 4", KEYCODE_V, IP_JOY_DEFAULT )
	PORT_START		/* 8 */
	PORT_ADJUSTER( 85, "Motor Frequency" )

	PORT_START		/* 9 */
	PORT_ADJUSTER( 80, "Motor Frequency" )
INPUT_PORTS_END


static struct GfxLayout firetrk_text_layout =
{
	16, 16, /* width, height */
	32,     /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x1C, 0x1D, 0x1E, 0x1F, 0x04, 0x05, 0x06, 0x07,
		0x0C, 0x0D, 0x0E, 0x0F, 0x14, 0x15, 0x16, 0x17
	},
	{
		0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0,
		0x100, 0x120, 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0
	},
	0x200
};


static struct GfxLayout superbug_text_layout =
{
	16, 16, /* width, height */
	32,     /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x0C, 0x0D, 0x0E, 0x0F, 0x14, 0x15, 0x16, 0x17,
		0x1C, 0x1D, 0x1E, 0x1F, 0x04, 0x05, 0x06, 0x07
	},
	{
		0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0,
		0x100, 0x120, 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0
	},
	0x200
};


static struct GfxLayout montecar_text_layout =
{
	8, 8,   /* width, height */
	64,     /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0xC, 0xD, 0xE, 0xF, 0x4, 0x5, 0x6, 0x7
	},
	{
		0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70
	},
	0x80
};


static struct GfxLayout firetrk_tile_layout =
{
	16, 16, /* width, height */
	64,     /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
	},
	{
		0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
		0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0
	},
	0x100
};


static struct GfxLayout superbug_tile_layout =
{
	16, 16, /* width, height */
	64,     /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x07, 0x06, 0x05, 0x04, 0x0F, 0x0E, 0x0D, 0x0C,
		0x17, 0x16, 0x15, 0x14, 0x1F, 0x1E, 0x1D, 0x1C
	},
	{
		0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0,
		0x100, 0x120, 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0
	},
	0x200
};


static struct GfxLayout firetrk_car_layout1 =
{
	32, 32, /* width, height */
	4,      /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x000, 0x040, 0x080, 0x0C0, 0x100, 0x140, 0x180, 0x1C0,
		0x200, 0x240, 0x280, 0x2C0, 0x300, 0x340, 0x380, 0x3C0,
		0x400, 0x440, 0x480, 0x4C0, 0x500, 0x540, 0x580, 0x5C0,
		0x600, 0x640, 0x680, 0x6C0, 0x700, 0x740, 0x780, 0x7C0
	},
	{
		0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F,
		0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F,
		0x24, 0x25, 0x26, 0x27, 0x2C, 0x2D, 0x2E, 0x2F,
		0x34, 0x35, 0x36, 0x37, 0x3C, 0x3D, 0x3E, 0x3B
	},
	0x800
};


static struct GfxLayout superbug_car_layout1 =
{
	32, 32, /* width, height */
	4,      /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
		0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00,
		0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
		0x1800, 0x1900, 0x1A00, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x1F00
	},
	{
		0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C,
		0x44, 0x4C, 0x54, 0x5C, 0x64, 0x6C, 0x74, 0x7C,
		0x84, 0x8C, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC,
		0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC
	},
	0x001
};


static struct GfxLayout montecar_car_layout =
{
	32, 32, /* width, height */
	8,      /* total         */
	2,      /* planes        */
	        /* plane offsets */
	{ 1, 0 },
	{
		0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
		0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
		0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E,
		0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E
	},
	{
		0x000, 0x040, 0x080, 0x0C0, 0x100, 0x140, 0x180, 0x1C0,
		0x200, 0x240, 0x280, 0x2C0, 0x300, 0x340, 0x380, 0x3C0,
		0x400, 0x440, 0x480, 0x4C0, 0x500, 0x540, 0x580, 0x5C0,
		0x600, 0x640, 0x680, 0x6C0, 0x700, 0x740, 0x780, 0x7C0
	},
	0x800
};


static struct GfxLayout firetrk_car_layout2 =
{
	32, 32, /* width, height */
	4,      /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F,
		0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F,
		0x24, 0x25, 0x26, 0x27, 0x2C, 0x2D, 0x2E, 0x2F,
		0x34, 0x35, 0x36, 0x37, 0x3C, 0x3D, 0x3E, 0x3B
	},
	{
		0x000, 0x040, 0x080, 0x0C0, 0x100, 0x140, 0x180, 0x1C0,
		0x200, 0x240, 0x280, 0x2C0, 0x300, 0x340, 0x380, 0x3C0,
		0x400, 0x440, 0x480, 0x4C0, 0x500, 0x540, 0x580, 0x5C0,
		0x600, 0x640, 0x680, 0x6C0, 0x700, 0x740, 0x780, 0x7C0
	},
	0x800
};


static struct GfxLayout superbug_car_layout2 =
{
	32, 32, /* width, height */
	4,      /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C,
		0x44, 0x4C, 0x54, 0x5C, 0x64, 0x6C, 0x74, 0x7C,
		0x84, 0x8C, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC,
		0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC
	},
	{
		0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
		0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00,
		0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
		0x1800, 0x1900, 0x1A00, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x1F00
	},
	0x001
};


static struct GfxLayout firetrk_trailer_layout =
{
	64, 64, /* width, height */
	8,      /* total         */
	1,      /* planes        */
	{ 0 },  /* plane offsets */
	{
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
	},
	{
		0x000, 0x040, 0x080, 0x0C0, 0x100, 0x140, 0x180, 0x1C0,
		0x200, 0x240, 0x280, 0x2C0, 0x300, 0x340, 0x380, 0x3C0,
		0x400, 0x440, 0x480, 0x4C0, 0x500, 0x540, 0x580, 0x5C0,
		0x600, 0x640, 0x680, 0x6C0, 0x700, 0x740, 0x780, 0x7C0,
		0x800, 0x840, 0x880, 0x8C0, 0x900, 0x940, 0x980, 0x9C0,
		0xA00, 0xA40, 0xA80, 0xAC0, 0xB00, 0xB40, 0xB80, 0xBC0,
		0xC00, 0xC40, 0xC80, 0xCC0, 0xD00, 0xD40, 0xD80, 0xDC0,
		0xE00, 0xE40, 0xE80, 0xEC0, 0xF00, 0xF40, 0xF80, 0xFC0
	},
	0x1000
};


static struct GfxDecodeInfo firetrk_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &firetrk_text_layout, 26, 1 },
	{ REGION_GFX2, 0, &firetrk_tile_layout, 0, 8 },
	{ REGION_GFX2, 0, &firetrk_tile_layout, 16, 3 },
	{ REGION_GFX3, 0, &firetrk_car_layout1, 22, 2 },
	{ REGION_GFX3, 0, &firetrk_car_layout2, 22, 2 },
	{ REGION_GFX4, 0, &firetrk_trailer_layout, 22, 2 },
	{ -1 }
};


static struct GfxDecodeInfo superbug_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &superbug_text_layout, 26, 1 },
	{ REGION_GFX2, 0, &superbug_tile_layout, 0, 8 },
	{ REGION_GFX2, 0, &superbug_tile_layout, 16, 3 },
	{ REGION_GFX3, 0, &superbug_car_layout1, 22, 2 },
	{ REGION_GFX3, 0, &superbug_car_layout2, 22, 2 },
	{ -1 }
};


static struct GfxDecodeInfo montecar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &montecar_text_layout, 44, 1 },
	{ REGION_GFX2, 0, &firetrk_tile_layout, 0, 8 },
	{ REGION_GFX2, 0, &firetrk_tile_layout, 16, 4 },
	{ REGION_GFX3, 0, &montecar_car_layout, 24, 1 },
	{ REGION_GFX4, 0, &montecar_car_layout, 28, 4 },
	{ -1 }
};



static MACHINE_DRIVER_START( firetrk )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M6800, 12096000 / 12)	/* 750Khz during service mode */
	MDRV_CPU_MEMORY(firetrk_readmem, firetrk_writemem)
	MDRV_CPU_VBLANK_INT(firetrk_interrupt, 1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION((int) ((22. * 1000000) / (262. * 60) + 0.5))

	MDRV_MACHINE_INIT(firetrk)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(320, 240)
	MDRV_VISIBLE_AREA(0, 319, 0, 239)
	MDRV_GFXDECODE(firetrk_gfxdecodeinfo)

	MDRV_PALETTE_INIT(firetrk)
	MDRV_PALETTE_LENGTH(4)
	MDRV_COLORTABLE_LENGTH(28)

	MDRV_VIDEO_START(firetrk)
	MDRV_VIDEO_EOF(firetrk)
	MDRV_VIDEO_UPDATE(firetrk)

	/* sound hardware */
	MDRV_SOUND_ADD_TAG("discrete", DISCRETE, firetrk_sound_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( superbug )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(firetrk)
	MDRV_CPU_REPLACE("main", M6800, 12096000 / 16)
	MDRV_CPU_MEMORY(superbug_readmem, superbug_writemem)

	/* video hardware */
	MDRV_GFXDECODE(superbug_gfxdecodeinfo)

	MDRV_PALETTE_INIT(firetrk)
	MDRV_PALETTE_LENGTH(4)
	MDRV_COLORTABLE_LENGTH(28)

	/* sound hardware */
	MDRV_SOUND_REPLACE("discrete", DISCRETE, superbug_sound_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( montecar )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(firetrk)
	MDRV_CPU_REPLACE("main", M6800, 12096000 / 12)	/* 750Khz during service mode */
	MDRV_CPU_MEMORY(montecar_readmem, montecar_writemem)

	/* video hardware */
	MDRV_GFXDECODE(montecar_gfxdecodeinfo)

	MDRV_PALETTE_INIT(montecar)
	MDRV_PALETTE_LENGTH(26)
	MDRV_COLORTABLE_LENGTH(46)

	/* sound hardware */
	MDRV_SOUND_REPLACE("discrete", DISCRETE, montecar_sound_interface)
MACHINE_DRIVER_END


ROM_START( firetrk )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD(          "32823-02.c1", 0x2000, 0x800, CRC(9570bdd3) SHA1(4d26a9490d05d53da55fc59459a4dce5bca6c761) )
	ROM_LOAD(          "32824-01.d1", 0x2800, 0x800, CRC(a5fc5629) SHA1(bf20510d8623eda2740ff296a7813a3e6f7ec76e) )
	ROM_LOAD_NIB_HIGH( "32816-01.k1", 0x3000, 0x800, CRC(c0535598) SHA1(15cb6985b0b22140b7fae1e050e0b63dd4d0f793) )
	ROM_LOAD_NIB_LOW ( "32820-01.k2", 0x3000, 0x800, CRC(5733f9ed) SHA1(0f19a40793dadfb7de2c2b54a44929b414d0f4ed) )
	ROM_LOAD_NIB_HIGH( "32815-01.j1", 0x3800, 0x800, CRC(506ee759) SHA1(d111356c84f3d9942a27fbe243e716d14c258a16) )
	ROM_RELOAD(                       0xF800, 0x800 )
	ROM_LOAD_NIB_LOW ( "32819-01.j2", 0x3800, 0x800, CRC(f1c3fa87) SHA1(d75cf4ad0bcac3289c068837fc24cfe84ce7542a) )
	ROM_RELOAD(                       0xF800, 0x800 )

	ROM_REGION( 0x0800, REGION_GFX1, ROMREGION_DISPOSE ) /* text */
	ROM_LOAD( "32827-01.r3", 0x000, 0x800, CRC(cca31d2b) SHA1(78235176c9cb2abd73a5778b54560b87634ca0e4) )

	ROM_REGION( 0x0800, REGION_GFX2, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "32828-02.f5", 0x000, 0x800, CRC(68ef5f19) SHA1(df227d6a57bba6298ebdeb5a118878da21d889f6) )

	ROM_REGION( 0x0400, REGION_GFX3, ROMREGION_DISPOSE ) /* car */
	ROM_LOAD( "32831-01.p7", 0x000, 0x400, CRC(bb8d144f) SHA1(9a1355ea6f88e96926c32e0e36ac0525b0243906) )

	ROM_REGION( 0x1000, REGION_GFX4, ROMREGION_DISPOSE ) /* trailer */
	ROM_LOAD( "32829-01.j5", 0x000, 0x800, CRC(e7267d71) SHA1(7132b98622e899227a378ba8c010dde39c479978) )
	ROM_LOAD( "32830-01.l5", 0x800, 0x800, CRC(e4d8b685) SHA1(30978658899c83e32dabdf554a13cf5e5235c725) )

	ROM_REGION( 0x100, REGION_PROMS, 0 )
	ROM_LOAD( "9114.prm", 0x0000, 0x100, CRC(b8094b4c) SHA1(82dc6799a19984f3b204ee3aeeb007e55afc8be3) ) /* sync */
ROM_END


ROM_START( superbug )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "9121.d1", 0x0800, 0x800, CRC(350df308) SHA1(b957c830bb95e0752ea9793e3edcfdd52235e0ab) )
	ROM_LOAD( "9122.c1", 0x1000, 0x800, CRC(eb6e3e37) SHA1(5237f6bd3a7a3eca737c728296230cf0d1f436b0) )
	ROM_LOAD( "9123.a1", 0x1800, 0x800, CRC(f42c6bbe) SHA1(41470984fe951eac9f6dc77862b00ecfe8aaa51d) )
	ROM_RELOAD(          0xF800, 0x800 )

	ROM_REGION( 0x0800, REGION_GFX1, ROMREGION_DISPOSE ) /* text */
	ROM_LOAD( "9124.m3", 0x0000, 0x400, CRC(f8af8dd5) SHA1(49ab85550f546f85048e2f73163837c602dde568) )
	ROM_LOAD( "9471.n3", 0x0400, 0x400, CRC(52250698) SHA1(cc55254c54dbcd3fd1465c82a715f2e567f44951) )

	ROM_REGION( 0x1000, REGION_GFX2, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "9126.f5", 0x0000, 0x400, CRC(ee695137) SHA1(295fdfef88e0c841fe8ad505151ca0837e77ef83) )
	ROM_LOAD( "9472.h5", 0x0400, 0x400, CRC(5ddb80ac) SHA1(bdbbbba6efdd4cca75630d203f7c7eaf41b1a32d) )
	ROM_LOAD( "9127.e5", 0x0800, 0x400, CRC(be1386b4) SHA1(17e92df58b25075ec7a383a958db02b42066578a) )
	ROM_RELOAD(          0x0C00, 0x400 )

	ROM_REGION( 0x0400, REGION_GFX3, ROMREGION_DISPOSE ) /* car */
	ROM_LOAD( "9125.k6", 0x0000, 0x400, CRC(a3c835df) SHA1(e9b6dba1919c389bb55a8fe3c074b6702322e4e5) )

	ROM_REGION( 0x0100, REGION_PROMS, 0 )
	ROM_LOAD( "9114.prm", 0x0000, 0x100, CRC(b8094b4c) SHA1(82dc6799a19984f3b204ee3aeeb007e55afc8be3) ) /* sync */
ROM_END


ROM_START( montecar )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "35766-01.h1", 0x2000, 0x800, CRC(d3695f09) SHA1(8aa3b3921acd0d2c3230d610843042613defcba9) )
    ROM_LOAD( "35765-01.f1", 0x2800, 0x800, CRC(9491a7ee) SHA1(712959c5f97be3db7be1d5bd70c780d4da2f6d47) )
    ROM_LOAD( "35764-01.d1", 0x3000, 0x800, CRC(899aaf4e) SHA1(84fab58d135ffc6e4b076d438b4d588b394364b6) )
	ROM_LOAD( "35763-01.c1", 0x3800, 0x800, CRC(378bfe47) SHA1(fd6b28907340a2ffc82a4e634273c3f03ab76642) )
	ROM_RELOAD(              0xF800, 0x800 )

	ROM_REGION( 0x0400, REGION_GFX1, ROMREGION_DISPOSE ) /* text */
	ROM_LOAD( "35778-01.m4", 0x0000, 0x400, CRC(294ee08e) SHA1(fbb0656468a027b2795073d811affc93c50994ec) )

	ROM_REGION( 0x0800, REGION_GFX2, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "35775-01.e6", 0x0000, 0x800, CRC(504106e9) SHA1(33eae2cf39b24eaf5b438a2af3060b2fdc0012b5) )

	ROM_REGION( 0x0800, REGION_GFX3, ROMREGION_DISPOSE ) /* car */
	ROM_LOAD( "35779-01.m6", 0x0000, 0x800, CRC(4fbb3fe1) SHA1(4267cd098a19892322d21f8fa7b55896158f8d6a) )

	ROM_REGION( 0x0800, REGION_GFX4, ROMREGION_DISPOSE ) /* drone */
	ROM_LOAD( "35780-01.b6", 0x0000, 0x800, CRC(9d0f1374) SHA1(52d1130d48dc877e1e47e26b2e4548633ed91b21) )

	ROM_REGION( 0x300, REGION_PROMS, 0 )
	ROM_LOAD( "35785-01.e7", 0x0000, 0x200, CRC(386c543a) SHA1(04edda180e6ff432b438947ffa46621ca0a823b4) ) /* color */
    ROM_LOAD( "9114.prm",    0x0200, 0x100, CRC(b8094b4c) SHA1(82dc6799a19984f3b204ee3aeeb007e55afc8be3) ) /* sync */
ROM_END


GAME( 1977, superbug, 0, superbug, superbug, superbug, ROT270, "Atari", "Super Bug" )
GAME( 1978, firetrk,  0, firetrk,  firetrk,  firetrk,  ROT270, "Atari", "Fire Truck" )
GAME( 1979, montecar, 0, montecar, montecar, montecar, ROT270, "Atari", "Monte Carlo" )
