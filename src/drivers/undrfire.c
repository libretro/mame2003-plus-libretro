/***************************************************************************

	Underfire  							(c) 1993 Taito
    Chase Bombers                       (c) 1994 Taito

	Driver by Bryan McPhail & David Graves.

	Board Info (Underfire):

		TC0470LIN : ?
		TC0480SCP : known tilemap chip
		TC0510NIO : known input chip
		TC0570SPC : must be the object chip (next to spritemap and OBJ roms)
		TC0590PIV : Piv tilemaps
		TC0620SCC : lightgun ??? pivot port ???
		TC0650FDA : palette ? (Slapshot and F3 games also have one)

	M43E0278A
	K1100744A Main Board

	2018 2088           43256    43256   68020-25
	2018 2088           D67-23   D67-17                    93C46
	2018 2088           43256    43256
	2018                D67-18   D67-19                TC0510NIO
	2018
	2018 TC0570 SPC                      43256
		                                     43256
	 D67-13                              43256  TC0650FDA
	          D67-07                            2018
	          D67-06
	TC0470LIN D67-05
	          D67-04                      43256
	TC0590PIV D67-03    43256    D67-10   43256
	                    43256    D67-11
	   D67-09    TC0480SCP       D67-12   TC0620SCC
	   D67-08

	  MB8421
	  MB8421   43256             EnsoniqOTIS
	           D67-20    D67-01
	           43256                             EnsoniqESP-R6
	 68000-12  D67-21    D67-02     EnsoniqSuperGlu

	           40MHz            16MHz   30.476MHz    68681


	Under Fire combines the sprite system used in Taito Z games with
	the TC0480SCP tilemap chip plus some features from the Taito F3 system.
	It has an extra tilemap chip which is a dead ringer for the TC0100SCN
	(check the inits). Why did Taito give it a different name in this
	incarnation?


	Game misbehaviours
	------------------

	(i) Sprites on some rounds had sprite/tile priority issues.
	Solved by upping sprite priority while TC0480SCP row zoom is
	enabled - kludge.


	Todo
	----

	This game needs a fake aim target!

	What does the 0xb00000 area do... alpha blending ??

	What is the unknown hardware at 0x600000... an alternative
	or legacy lightgun hookup?

	Pivot port which may be used for rotation: but not
	seen changing except in game inits. Perhaps only used
	in later levels?


	Gun calibration
	---------------

	The values below work well (set speed down to 4 so you can enter
	them). They give a little reloading margin all around screen.

	Use X=0x2000  Y=0x100 for top center
	Use X=0x3740  (same Y) top left
	Use X=0x8c0   (same Y) top right
	Then for the points from left to right near bottom (all Y=0x3400):
	X=0x3f00,0x3740,0x2f80,0x27c0,0x2000,0x1840,0x1080,0x8c0,0x100


Code documentation
------------------

$17b6e2: loop which keeps displaying the trail of aim dots in test mode

$181826: routine which calls subs to derive aim coords for P1 and P2 and
pokes them in the game's internal sprite table format into RAM - along
with the nine blue "flag point" sprites on the calibration screen
which seem to have fixed coordinates. [It also refreshes some green
text on screen - the calls to $1bfa.]

$18141a sub appears to be doing all the calculations - including an
indirected subroutine so there may be quite a lot of possible code.
It is called with parameter of 1 for player 2 (and 0 for player 1).

$1821c8 sub is called just after - this is simpler and seems to be
copying the calculation results (modified slightly by 3 pixels in each
direction - to adjust for size of aim sprite?) into the table in ram.

(Subsequently a standard conversion routine turns the table on the fly
into dwords that are actually poked into spriteram. To locate the code
do a watchpoint on the first sprite in spriteram - the P1 aim point.)

In-game: $18141a is called 3 times when you hit fire - 3 bullets - and
once when you hit shotgun. Like Spacegun it is only doing the aim
calculations when it needs to, so to provide an artificial target we
need to reproduce the $18141a calculations.


Info (Chase Bombers)

Chase Bombers
Taito, 1994

Runs on hardware similar to Ground Effects


PCB Layout
----------

MAIN PCB-D
K1100809A
J1100342A
|----------------------------------------------------------------------------------------------|
|           C5                              C6                             SMC_COM20020  LANOUT|
| 68EC020   61256     68EC000  61256            68EC000      61256      MB8421                 |
|           61256              61256                         61256                       LANIN |
|           61256                     61256                  PAL                               |
|           61256    PAL              61256                  PAL                               |
|                    PAL                                     PAL        MB8421                 |
|                    40MHz                                                                     |
|                                                                  MC68681                     |
|                                           TC511664-80     MB3771                             |
|                                                                                            P1|
|  MACH120   MACH120                                                                           |
|                                     ENSONIQ         30.4761MHz  16MHz   ADC0809              |
|                                     ESP-R6                                                   |
|                                                       ENSONIQ                                |
|                                     ENSONIQ           5701     DSW1(8)  TC0510NIO            |
|                                     OTIS-R2                                                 Z|
|                                                         93C46                                |
|           C3                              C4                                                 |
|                                                                                              |
|  61256                                                                                     |-|
|  61256                                                                                     |
|                                                                                            |-|
|                TC0480SCP        TC0620SCC           TC0360PRI                                |
|                                                                                              |
|                                                                                TC0650FDA     |
|  2018                                                                  61256                 |
|                                                                        61256                 |
|  2018     2088             61256                                       61256                G|
|                            61256                    TC0580PIV                                |
|  2018     2088                                                                               |
|                                                                                              |
|  2018                                                             TL074    TL074   TDA1543   |
|           TC0570SPC         TC0470LIN               514256  514256                           |
|  2018                                               514256  514256        TA8221   TD62064 |-|
|                                                     514256  514256                 TD62064 |
|  2018                                                               MB87078        TD62064 |-|
|           C1                              C2                                                 |
|----------------------------------------------------------------------------------------------|
Notes:
      ROM board plugs into C* connectors
      No clocks for now, PCB has light corrosion and will need extensive cleaning before it can be powered up.

ROM Board
---------

PCB Numbers - ROM.PCB
              K9100508A
              J9100367A

Board contains only 29 ROMs and not much else.



***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/taitoic.h"
#include "sndhrdw/taitosnd.h"
#include "machine/eeprom.h"

VIDEO_START( undrfire );
VIDEO_UPDATE( undrfire );
VIDEO_START( cbombers );
VIDEO_UPDATE( cbombers );

/* F3 sound */
READ16_HANDLER(f3_68000_share_r);
WRITE16_HANDLER(f3_68000_share_w);
READ16_HANDLER(f3_68681_r);
WRITE16_HANDLER(f3_68681_w);
READ16_HANDLER(es5510_dsp_r);
WRITE16_HANDLER(es5510_dsp_w);
WRITE16_HANDLER(f3_volume_w);
WRITE16_HANDLER(f3_es5505_bank_w);
void f3_68681_reset(void);
extern data32_t *f3_shared_ram;

static UINT16 coin_word;
static UINT16 port_sel = 0;
extern UINT16 undrfire_rotate_ctrl[8];
static int frame_counter=0;

static UINT8 Contrast_LUT[0x100];
static UINT8 Brightness_LUT[0x100];

data32_t *undrfire_ram;	/* will be read in vidhrdw for gun target calcs */
UINT32 *shared_ram;

/***********************************************************
				COLOR RAM

Extract a standard version of this
("taito_8bpg_palette_word_w"?) to Taitoic.c ?
***********************************************************/

static void calc_brightness_lut(int brightness)
{
	int col;
	for (col = 0; col < 0x100; col++) {
		int mcol = col * brightness / 100;

		if (mcol < 0) mcol = 0;
		if (mcol > 255) mcol = 255;

		Brightness_LUT[col] = mcol;
	}
}

static void calc_contrast_lut(double contrast)
{
	int col;
	double c = (100.0 + contrast) / 100.0;
	c *= c;

	for (col = 0; col < 0x100; col++) {
		double color = ((double)col / 255.0) - 0.5;
		color = ((color * c) + 0.5) * 255.0;

		if (color < 0) color = 0;
		if (color > 255) color = 255;

		Contrast_LUT[col] = color;
	}
}

static WRITE32_HANDLER( color_ram_w )
{
	int a,r,g,b;
	COMBINE_DATA(&paletteram32[offset]);

	{
		a = paletteram32[offset];
		r = (a &0xff0000) >> 16;
		g = (a &0xff00) >> 8;
		b = (a &0xff);

		/* color wash-out fix */
		r = Contrast_LUT[Brightness_LUT[r]];
		g = Contrast_LUT[Brightness_LUT[g]];
		b = Contrast_LUT[Brightness_LUT[b]];

		palette_set_color(offset,r,g,b);
	}
}


/***********************************************************
				INTERRUPTS
***********************************************************/

void undrfire_interrupt5(int x)
{
	cpu_set_irq_line(0,5,HOLD_LINE);
}


/**********************************************************
				EPROM
**********************************************************/

static data8_t default_eeprom[128]=
{
	0x02,0x01,0x11,0x12,0x01,0x01,0x01,0x00,0x80,0x80,0x30,0x01,0x00,0x00,0x62,0x45,
	0xe0,0xa0,0xff,0x28,0xff,0xff,0xfa,0xd7,0x33,0x28,0x00,0x00,0x33,0x28,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0xa0,0xff,0x28,0xff,0xff,0xff,0xff,0xfa,0xd7,
	0x33,0x28,0x00,0x00,0x33,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

static struct EEPROM_interface undrfire_eeprom_interface =
{
	6,				/* address bits */
	16,				/* data bits */
	"0110",			/* read command */
	"0101",			/* write command */
	"0111",			/* erase command */
	"0100000000",	/* unlock command */
	"0100110000",	/* lock command */
};

static NVRAM_HANDLER( undrfire )
{
	if (read_or_write)
		EEPROM_save(file);
	else {
		EEPROM_init(&undrfire_eeprom_interface);
		if (file)
			EEPROM_load(file);
		else
			EEPROM_set_data(default_eeprom,128);  /* Default the gun setup values */
	}
}


/**********************************************************
			GAME INPUTS
**********************************************************/

static READ32_HANDLER( undrfire_input_r )
{
	switch (offset)
	{
		case 0x00:
		{
			return (input_port_0_word_r(0,0) << 16) | input_port_1_word_r(0,0) |
				  (EEPROM_read_bit() << 7) | frame_counter;
		}

		case 0x01:
		{
			return input_port_2_word_r(0,0) | (coin_word << 16);
		}
 	}

	return 0xffffffff;
}

static WRITE32_HANDLER( undrfire_input_w )
{
	switch (offset)
	{
		case 0x00:
		{
			if (ACCESSING_MSB32)	/* $500000 is watchdog */
			{
				watchdog_reset_w(0,data >> 24);
			}

			if (ACCESSING_LSB32)
			{
				EEPROM_set_clock_line((data & 0x20) ? ASSERT_LINE : CLEAR_LINE);
				EEPROM_write_bit(data & 0x40);
				EEPROM_set_cs_line((data & 0x10) ? CLEAR_LINE : ASSERT_LINE);
				return;
			}

			return;
		}

		case 0x01:
		{
			if (ACCESSING_MSB32)
			{
				coin_lockout_w(0,~data & 0x01000000);
				coin_lockout_w(1,~data & 0x02000000);
				coin_counter_w(0, data & 0x04000000);
				coin_counter_w(1, data & 0x08000000);
				coin_word = (data >> 16) &0xffff;
			}
		}
	}
}


static READ16_HANDLER( shared_ram_r )
{
	if ((offset&1)==0) return (shared_ram[offset/2]&0xffff0000)>>16;
	return (shared_ram[offset/2]&0x0000ffff);
}


static WRITE16_HANDLER( shared_ram_w )
{
	if ((offset&1)==0) {
		if (ACCESSING_MSB)
			shared_ram[offset/2]=(shared_ram[offset/2]&0x00ffffff)|((data&0xff00)<<16);
		if (ACCESSING_LSB)
			shared_ram[offset/2]=(shared_ram[offset/2]&0xff00ffff)|((data&0x00ff)<<16);
	} else {
		if (ACCESSING_MSB)
			shared_ram[offset/2]=(shared_ram[offset/2]&0xffff00ff)|((data&0xff00)<< 0);
		if (ACCESSING_LSB)
			shared_ram[offset/2]=(shared_ram[offset/2]&0xffffff00)|((data&0x00ff)<< 0);
	}
}


/* Some unknown hardware byte mapped at $600002-5 */

static READ32_HANDLER( unknown_hardware_r )
{
	switch (offset)	/* four single bytes are read in sequence at $156e */
	{
		case 0x00:	/* $600002-3 */
		{
			return 0xffff;	/* no idea what they should be*/
		}

		case 0x01:	/* $600004-5 */
		{
			return 0xffff0000;	/* no idea what they should be*/
		}
	}

	return 0x0;
}


static WRITE32_HANDLER( unknown_int_req_w )
{
	/* 10000 cycle delay is arbitrary */
	timer_set(TIME_IN_CYCLES(10000,0),0, undrfire_interrupt5);
}


static READ32_HANDLER( undrfire_lightgun_r )
{
	int x,y;

	switch (offset)
	{
		/* NB we are raising the raw inputs by an arbitrary amount,
		   but presumably the guns on the original will not have had
		   full 0-0xffff travel. We don't center around 0x8000... but
		   who knows if the real machine does. */

		case 0x00:	/* P1 */
		{
			x = input_port_3_word_r(0,0) << 6;
			y = input_port_4_word_r(0,0) << 6;

			return ((x << 24) &0xff000000) | ((x << 8) &0xff0000)
				 | ((y << 8) &0xff00) | ((y >> 8) &0xff) ;
		}

		case 0x01:	/* P2 */
		{
			x = input_port_5_word_r(0,0) << 6;
			y = input_port_6_word_r(0,0) << 6;

			return ((x << 24) &0xff000000) | ((x << 8) &0xff0000)
				 | ((y << 8) &0xff00) | ((y >> 8) &0xff) ;
		}
	}

log_cb(RETRO_LOG_DEBUG, LOGPRE "CPU #0 PC %06x: warning - read unmapped lightgun offset %06x\n",activecpu_get_pc(),offset);

	return 0x0;
}


static WRITE32_HANDLER( rotate_control_w )	/* only a guess that it's rotation */
{
		if (ACCESSING_LSW32)
		{
			undrfire_rotate_ctrl[port_sel] = data;
			return;
		}

		if (ACCESSING_MSW32)
		{
			port_sel = (data &0x70000) >> 16;
		}
}


static WRITE32_HANDLER( motor_control_w )
{
/*
	Standard value poked is 0x00910200 (we ignore lsb and msb
	which seem to be always zero)

	0x0, 0x8000 and 0x9100 are written at startup

	Two bits are written in test mode to this middle word
	to test gun vibration:

	........ .x......   P1 gun vibration
	........ x.......   P2 gun vibration
*/
}

static WRITE32_HANDLER( cbombers_cpua_ctrl_w )
{
/*
    ........ ..xxxxxx   Lamp 1-6 enables
    ........ .x......   Vibration
*/

	cpu_set_reset_line(2,(data & 0x1000) ? CLEAR_LINE : ASSERT_LINE);
}

static READ32_HANDLER( cbombers_adc_r )
{
	return (input_port_3_word_r(0,0) << 24 );
}

static WRITE32_HANDLER( cbombers_adc_w )
{
	/* One interrupt per input port (4 per frame, though only 2 used).
        1000 cycle delay is arbitrary */
	/* TODO: hook it up to offset 0 only otherwise cbomber proto keeps sending irqs.
	         Could or could not be right. */
	if(offset == 0)
		timer_set(TIME_IN_CYCLES(10000,0),0, undrfire_interrupt5);
}

static WRITE32_HANDLER( trampoline_32_8_w)
{
	int index, shift;

	if      (mem_mask == 0x00ffffff) { index = 0; shift = 24; }
	else if (mem_mask == 0xff00ffff) { index = 1; shift = 16; }
	else if (mem_mask == 0xffff00ff) { index = 2; shift =  8; }
	else if (mem_mask == 0xffffff00) { index = 3; shift =  0; }

	TC0360PRI_w(index+4*offset, (data >> shift) & 0xff);
}

/***********************************************************
			 MEMORY STRUCTURES
***********************************************************/

static MEMORY_READ32_START( undrfire_readmem )
	{ 0x000000, 0x1fffff, MRA32_ROM },
	{ 0x200000, 0x21ffff, MRA32_RAM },	/* main CPUA ram */
	{ 0x300000, 0x303fff, MRA32_RAM },	/* sprite ram */
/*	{ 0x304000, 0x304003, MRA32_RAM },	*/ /* debugging*/
/*	{ 0x304400, 0x304403, MRA32_RAM },	*/ /* debugging*/
	{ 0x500000, 0x500007, undrfire_input_r },
	{ 0x600000, 0x600007, unknown_hardware_r },	/* unknown byte reads at $156e */
	{ 0x700000, 0x7007ff, MRA32_RAM },
	{ 0x800000, 0x80ffff, TC0480SCP_long_r },	  /* tilemaps */
	{ 0x830000, 0x83002f, TC0480SCP_ctrl_long_r },	/* debugging*/
	{ 0x900000, 0x90ffff, TC0100SCN_long_r },	/* piv tilemaps */
	{ 0x920000, 0x92000f, TC0100SCN_ctrl_long_r },
	{ 0xa00000, 0xa0ffff, MRA32_RAM },	/* palette ram */
	{ 0xb00000, 0xb003ff, MRA32_RAM },	/* ?? single bytes*/
	{ 0xf00000, 0xf00007, undrfire_lightgun_r },	/* stick coords read at $11b2-bc */
MEMORY_END

static MEMORY_WRITE32_START( undrfire_writemem )
	{ 0x000000, 0x1fffff, MWA32_ROM },
	{ 0x200000, 0x21ffff, MWA32_RAM, &undrfire_ram },
	{ 0x300000, 0x303fff, MWA32_RAM, &spriteram32, &spriteram_size },
/*	{ 0x304000, 0x304003, MWA32_RAM },	*/ /* ??? doesn't change*/
/*	{ 0x304400, 0x304403, MWA32_RAM },	*/ /* ??? doesn't change*/
	{ 0x400000, 0x400003, motor_control_w },	/* gun vibration */
	{ 0x500000, 0x500007, undrfire_input_w },	/* eerom etc. */
	{ 0x600000, 0x600007, unknown_int_req_w },	/* int request for unknown hardware */
	{ 0x700000, 0x7007ff, MWA32_RAM, &f3_shared_ram },
	{ 0x800000, 0x80ffff, TC0480SCP_long_w },	  /* tilemaps */
	{ 0x830000, 0x83002f, TC0480SCP_ctrl_long_w },
	{ 0x900000, 0x90ffff, TC0100SCN_long_w },	/* piv tilemaps */
	{ 0x920000, 0x92000f, TC0100SCN_ctrl_long_w },
	{ 0xa00000, 0xa0ffff, color_ram_w, &paletteram32 },
	{ 0xb00000, 0xb003ff, MWA32_RAM },	/* single bytes, blending ??*/
	{ 0xd00000, 0xd00003, rotate_control_w },	/* perhaps port based rotate control? */
MEMORY_END

static MEMORY_READ32_START( cbombers_readmem )
	{ 0x000000, 0x1fffff, MRA32_ROM },
	{ 0x200000, 0x21ffff, MRA32_RAM },
	{ 0x300000, 0x303fff, MRA32_RAM },
	{ 0x500000, 0x500007, undrfire_input_r },
	{ 0x600000, 0x600007, cbombers_adc_r },
	{ 0x700000, 0x7007ff, MRA32_RAM },
	{ 0x800000, 0x80ffff, TC0480SCP_long_r },		/* tilemaps */
	{ 0x830000, 0x83002f, TC0480SCP_ctrl_long_r },
	{ 0x900000, 0x90ffff, TC0100SCN_long_r },		/* piv tilemaps */
	{ 0x920000, 0x92000f, TC0100SCN_ctrl_long_r },
	{ 0xa00000, 0xa0ffff, MRA32_RAM },	/* palette ram */
	{ 0xb00000, 0xb0000f, MRA32_RAM }, /* priority read - unused */
	{ 0xc00000, 0xc00007, MRA32_RAM }, /* LAN controller? */
	{ 0xe00000, 0xe0ffff, MRA32_RAM },
MEMORY_END

static MEMORY_WRITE32_START( cbombers_writemem )
	{ 0x000000, 0x1fffff, MWA32_ROM },
	{ 0x200000, 0x21ffff, MWA32_RAM },
	{ 0x300000, 0x303fff, MWA32_RAM, &spriteram32, &spriteram_size },
	{ 0x400000, 0x400003, cbombers_cpua_ctrl_w },
	{ 0x500000, 0x500007, undrfire_input_w },
	{ 0x600000, 0x600007, cbombers_adc_w },
	{ 0x700000, 0x7007ff, MWA32_RAM, &f3_shared_ram },
	{ 0x800000, 0x80ffff, TC0480SCP_long_w },		/* tilemaps */
	{ 0x830000, 0x83002f, TC0480SCP_ctrl_long_w },
	{ 0x900000, 0x90ffff, TC0100SCN_long_w },		/* piv tilemaps */
	{ 0x920000, 0x92000f, TC0100SCN_ctrl_long_w },
	{ 0xa00000, 0xa0ffff, color_ram_w, &paletteram32 },
	{ 0xb00000, 0xb0000f, trampoline_32_8_w }, /* priority */
	{ 0xc00000, 0xc00007, MWA32_RAM },/* LAN controller? */
	{ 0xd00000, 0xd00003, rotate_control_w },	/* perhaps port based rotate control? */
	{ 0xe00000, 0xe0ffff, MWA32_RAM, &shared_ram },
MEMORY_END

static MEMORY_READ16_START( cbombers_sub_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, MRA16_RAM	}, /* local ram */
/*	{ 0x600000, 0x60ffff, MRA16_RAM	},*/
	{ 0x800000, 0x80ffff, shared_ram_r },
MEMORY_END

static MEMORY_WRITE16_START( cbombers_sub_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, MWA16_RAM	}, /* local ram */
/*	{ 0x600000, 0x60ffff, TC0480SCP_word_w	}, Only written upon errors */
	{ 0x800000, 0x80ffff, shared_ram_w },
MEMORY_END


/******************************************************************************/

static MEMORY_READ16_START( sound_readmem )
	{ 0x000000, 0x03ffff, MRA16_RAM },
	{ 0x140000, 0x140fff, f3_68000_share_r },
	{ 0x200000, 0x20001f, ES5505_data_0_r },
	{ 0x260000, 0x2601ff, es5510_dsp_r },
	{ 0x280000, 0x28001f, f3_68681_r },
	{ 0xc00000, 0xcfffff, MRA16_BANK1 },
	{ 0xff8000, 0xffffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( sound_writemem )
	{ 0x000000, 0x03ffff, MWA16_RAM },
	{ 0x140000, 0x140fff, f3_68000_share_w },
	{ 0x200000, 0x20001f, ES5505_data_0_w },
	{ 0x260000, 0x2601ff, es5510_dsp_w },
	{ 0x280000, 0x28001f, f3_68681_w },
	{ 0x300000, 0x30003f, f3_es5505_bank_w },
	{ 0x340000, 0x340003, f3_volume_w }, /* 8 channel volume control */
	{ 0xc00000, 0xcfffff, MWA16_ROM },
	{ 0xff8000, 0xffffff, MWA16_RAM },
MEMORY_END


/***********************************************************
			 INPUT PORTS (dips in eprom)
***********************************************************/

INPUT_PORTS_START( undrfire )
	PORT_START      /* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER1 )	/* ? where is freeze input */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* Frame counter */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* reserved for EEROM */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_START2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BITX(0x01, IP_ACTIVE_LOW,  IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	/* Gun inputs (real range is 0-0xffff: we use standard 0-255 and shift later) */

	PORT_START	/* IN 3, P1X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_REVERSE | IPF_PLAYER1, 20, 25, 0, 0xff)

	PORT_START	/* IN 4, P1Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 20, 25, 0, 0xff)

	PORT_START	/* IN 5, P2X */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_REVERSE | IPF_PLAYER2, 20, 25, 0, 0xff)

	PORT_START	/* IN 6, P2Y */
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 20, 25, 0, 0xff)
INPUT_PORTS_END


INPUT_PORTS_START( cbombers )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_BUTTON5 ) /* ? where is freeze input */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BITX(0x0100, IP_ACTIVE_LOW,  IPT_BUTTON4 | IPF_TOGGLE, "Gear Shift", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x0200, IP_ACTIVE_LOW,  IPT_BUTTON3, "Fire", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x0400, IP_ACTIVE_LOW,  IPT_BUTTON1, "Accelerator", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x0800, IP_ACTIVE_LOW,  IPT_BUTTON2, "Brake", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* Frame counter */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* reserved for EEROM */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_START2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START
	PORT_BITX(0x01, IP_ACTIVE_LOW,  IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START	/* IN 3, steering wheel */
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_REVERSE | IPF_PLAYER1, 25, 15, 0, 0xff)
INPUT_PORTS_END


/**********************************************************
				GFX DECODING
**********************************************************/

static struct GfxLayout tile16x16_layout =
{
	16,16,	/* 16*16 sprites */
	RGN_FRAC(1,2),
	5,	/* 5 bits per pixel */
	{ RGN_FRAC(1,2), 0, 8, 16, 24 },
	{ 32, 33, 34, 35, 36, 37, 38, 39, 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*64, 1*64,  2*64,  3*64,  4*64,  5*64,  6*64,  7*64,
	  8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	64*16	/* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout charlayout =
{
	16,16,    /* 16*16 characters */
	RGN_FRAC(1,1),
	4,        /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 5*4, 4*4, 3*4, 2*4, 7*4, 6*4, 9*4, 8*4, 13*4, 12*4, 11*4, 10*4, 15*4, 14*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64, 8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8     /* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout pivlayout =
{
	8,8,    /* 8*8 characters */
	RGN_FRAC(1,2),
	6,      /* 4 bits per pixel */
	{ RGN_FRAC(1,2), RGN_FRAC(1,2)+1, 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo undrfire_gfxdecodeinfo[] =
{
	{ REGION_GFX2, 0x0, &tile16x16_layout,  0, 512 },
	{ REGION_GFX1, 0x0, &charlayout,        0, 512 },
	{ REGION_GFX3, 0x0, &pivlayout,         0, 512 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo cbombers_gfxdecodeinfo[] =
{
	{ REGION_GFX2, 0x0, &tile16x16_layout,  0, 512 },
	{ REGION_GFX1, 0x0, &charlayout,        0x1000, 512 },
	{ REGION_GFX3, 0x0, &pivlayout,         0, 512 },
	{ -1 } /* end of array */
};


/***********************************************************
			     MACHINE DRIVERS
***********************************************************/


static MACHINE_INIT( undrfire )
{
	/* Sound cpu program loads to 0xc00000 so we use a bank */
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU2);
	cpu_setbank(1,&RAM[0x80000]);

	RAM[0]=RAM[0x80000]; /* Stack and Reset vectors */
	RAM[1]=RAM[0x80001];
	RAM[2]=RAM[0x80002];
	RAM[3]=RAM[0x80003];

	f3_68681_reset();

	/* color wash-out fix */
	calc_contrast_lut(0x1a);
	calc_brightness_lut(0x57);
}

static MACHINE_INIT( cbombers )
{
	/* Sound cpu program loads to 0xc00000 so we use a bank */
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU2);
	cpu_setbank(1,&RAM[0x80000]);

	RAM[0]=RAM[0x80000]; /* Stack and Reset vectors */
	RAM[1]=RAM[0x80001];
	RAM[2]=RAM[0x80002];
	RAM[3]=RAM[0x80003];

	f3_68681_reset();

	/* color wash-out fix */
	calc_contrast_lut(0x26);
	calc_brightness_lut(0x45);
}

static struct ES5505interface es5505_interface =
{
	1,					/* total number of chips */
	{ 30476000 / 2 },	/* freq */
	{ REGION_SOUND1 },	/* Bank 0: Unused by F3 games? */
	{ REGION_SOUND1 },	/* Bank 1: All games seem to use this */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },		/* master volume */
};

static INTERRUPT_GEN( undrfire_interrupt )
{
	frame_counter^=1;
	cpu_set_irq_line(0, 4, HOLD_LINE);
}

static MACHINE_DRIVER_START( undrfire )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68EC020, 16000000)	/* 16 MHz */
	MDRV_CPU_MEMORY(undrfire_readmem,undrfire_writemem)
	MDRV_CPU_VBLANK_INT(undrfire_interrupt,1)

	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(undrfire)
	MDRV_NVRAM_HANDLER(undrfire)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN)
	MDRV_SCREEN_SIZE(40*8, 32*8)
	MDRV_VISIBLE_AREA(0, 40*8-1, 3*8, 32*8-1)
	MDRV_GFXDECODE(undrfire_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(16384)

	MDRV_VIDEO_START(undrfire)
	MDRV_VIDEO_UPDATE(undrfire)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(ES5505, es5505_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( cbombers )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68EC020, 16000000)	/* 16 MHz */
	MDRV_CPU_MEMORY(cbombers_readmem, cbombers_writemem)
	MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_CPU_ADD(M68000, 16000000)  /* 16 MHz */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_CPU_ADD(M68000, 16000000)	/* 16 MHz */
	MDRV_CPU_MEMORY(cbombers_sub_readmem, cbombers_sub_writemem)
	MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_INTERLEAVE(5)	/* CPU slices - Need to interleave Cpu's 1 & 3 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(cbombers)
	MDRV_NVRAM_HANDLER(undrfire)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_NEEDS_6BITS_PER_GUN)
	MDRV_SCREEN_SIZE(40*8, 32*8)
	MDRV_VISIBLE_AREA(0, 40*8-1, 3*8, 32*8-1)
	MDRV_GFXDECODE(cbombers_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(16384)

	MDRV_VIDEO_START(cbombers)
	MDRV_VIDEO_UPDATE(cbombers)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(ES5505, es5505_interface)
MACHINE_DRIVER_END

/***************************************************************************
					DRIVERS
***************************************************************************/

ROM_START( undrfire )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )	/* 2048K for 68020 code (CPU A) */
	ROM_LOAD32_BYTE( "d67-19", 0x00000, 0x80000, CRC(1d88fa5a) SHA1(5e498efb9535a8f4e82b5525390b8bde7c45c07e) )
	ROM_LOAD32_BYTE( "d67-18", 0x00001, 0x80000, CRC(f41ae7fd) SHA1(bdd0df01b11205c263d2fa280746826b831d58bc) )
	ROM_LOAD32_BYTE( "d67-17", 0x00002, 0x80000, CRC(34e030b7) SHA1(62c270c817199a56e647ea74849fe5c07717ac18) )
	ROM_LOAD32_BYTE( "d67-23", 0x00003, 0x80000, CRC(28e84e0a) SHA1(74c73c6df07d33ef4c0a29f8c1ee1a33eee922da) )

	ROM_REGION( 0x140000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "d67-20", 0x100000, 0x20000,  CRC(974ebf69) SHA1(8a5de503c514bf0da0c956e2dfdf0cfb83ea1f72) )
	ROM_LOAD16_BYTE( "d67-21", 0x100001, 0x20000,  CRC(8fc6046f) SHA1(28522ce5c5900f74d3faa86710256a7201b32500) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d67-08", 0x000000, 0x200000, CRC(56730d44) SHA1(110872714b3c26a82473c7b80c120918b91b1b4b) )	/* SCR 16x16 tiles */
	ROM_LOAD16_BYTE( "d67-09", 0x000001, 0x200000, CRC(3c19f9e3) SHA1(7ba8475d37cbf8bf38029124afdf62c915c8668d) )

	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "d67-03", 0x000003, 0x200000, CRC(3b6e99a9) SHA1(1e0e66763ddfa18a2d291626b245633555092959) )	/* OBJ 16x16 tiles */
	ROM_LOAD32_BYTE( "d67-04", 0x000002, 0x200000, CRC(8f2934c9) SHA1(ead95b34eec3a6df27199edcbdd5595bc6555a50) )
	ROM_LOAD32_BYTE( "d67-05", 0x000001, 0x200000, CRC(e2e7dcf3) SHA1(185dbd0489931123a295139dc0a045ad239018fb) )
	ROM_LOAD32_BYTE( "d67-06", 0x000000, 0x200000, CRC(a2a63488) SHA1(a1ed140cc3757c3c05a0a822089c6efc83bf4805) )
	ROM_LOAD32_BYTE( "d67-07", 0x800000, 0x200000, CRC(189c0ee5) SHA1(de85b39dc67f31ef80800ff6ec9a391652eb12e4) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d67-10", 0x000000, 0x100000, CRC(d79e6ce9) SHA1(8b38302971816d599cdaa3279cb6395441373c6f) )	/* PIV 8x8 tiles, 6bpp */
	ROM_LOAD16_BYTE( "d67-11", 0x000001, 0x100000, CRC(7a401bb3) SHA1(47257a6a4b37ec1ceb4e974b776ee3ea30db06fa) )
	ROM_LOAD       ( "d67-12", 0x300000, 0x100000, CRC(67b16fec) SHA1(af0f9f50516331780ef6cfab1e12a23edf87daa7) )
	ROM_FILL       (           0x200000, 0x100000, 0 )

	ROM_REGION16_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD16_WORD( "d67-13", 0x00000,  0x80000,  CRC(42e7690d) SHA1(5f00f3f814653733bf9a5cb010675799de02fa76) )	/* STY, spritemap */

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "d67-01", 0x000000, 0x200000, CRC(a2f18122) SHA1(640014c6e6d66c59fe0accf370ad3bab9f40429a) )	/* Ensoniq samples */
	ROM_LOAD16_BYTE( "d67-02", 0xc00000, 0x200000, CRC(fceb715e) SHA1(9326513acb0696669d4f2345649ab37c8c6ed171) )
ROM_END


ROM_START( undrfiru )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )	/* 2048K for 68020 code (CPU A) */
	ROM_LOAD32_BYTE( "d67-19", 0x00000, 0x80000, CRC(1d88fa5a) SHA1(5e498efb9535a8f4e82b5525390b8bde7c45c07e) )
	ROM_LOAD32_BYTE( "d67-18", 0x00001, 0x80000, CRC(f41ae7fd) SHA1(bdd0df01b11205c263d2fa280746826b831d58bc) )
	ROM_LOAD32_BYTE( "d67-17", 0x00002, 0x80000, CRC(34e030b7) SHA1(62c270c817199a56e647ea74849fe5c07717ac18) )
	ROM_LOAD32_BYTE( "d67-22", 0x00003, 0x80000, CRC(5fef7e9c) SHA1(03a6ea0715ce8705d74550186b22940f8a49c088) )

	ROM_REGION( 0x140000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "d67-20", 0x100000, 0x20000,  CRC(974ebf69) SHA1(8a5de503c514bf0da0c956e2dfdf0cfb83ea1f72) )
	ROM_LOAD16_BYTE( "d67-21", 0x100001, 0x20000,  CRC(8fc6046f) SHA1(28522ce5c5900f74d3faa86710256a7201b32500) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d67-08", 0x000000, 0x200000, CRC(56730d44) SHA1(110872714b3c26a82473c7b80c120918b91b1b4b) )	/* SCR 16x16 tiles */
	ROM_LOAD16_BYTE( "d67-09", 0x000001, 0x200000, CRC(3c19f9e3) SHA1(7ba8475d37cbf8bf38029124afdf62c915c8668d) )

	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "d67-03", 0x000003, 0x200000, CRC(3b6e99a9) SHA1(1e0e66763ddfa18a2d291626b245633555092959) )	/* OBJ 16x16 tiles */
	ROM_LOAD32_BYTE( "d67-04", 0x000002, 0x200000, CRC(8f2934c9) SHA1(ead95b34eec3a6df27199edcbdd5595bc6555a50) )
	ROM_LOAD32_BYTE( "d67-05", 0x000001, 0x200000, CRC(e2e7dcf3) SHA1(185dbd0489931123a295139dc0a045ad239018fb) )
	ROM_LOAD32_BYTE( "d67-06", 0x000000, 0x200000, CRC(a2a63488) SHA1(a1ed140cc3757c3c05a0a822089c6efc83bf4805) )
	ROM_LOAD32_BYTE( "d67-07", 0x800000, 0x200000, CRC(189c0ee5) SHA1(de85b39dc67f31ef80800ff6ec9a391652eb12e4) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d67-10", 0x000000, 0x100000, CRC(d79e6ce9) SHA1(8b38302971816d599cdaa3279cb6395441373c6f) )	/* PIV 8x8 tiles, 6bpp */
	ROM_LOAD16_BYTE( "d67-11", 0x000001, 0x100000, CRC(7a401bb3) SHA1(47257a6a4b37ec1ceb4e974b776ee3ea30db06fa) )
	ROM_LOAD       ( "d67-12", 0x300000, 0x100000, CRC(67b16fec) SHA1(af0f9f50516331780ef6cfab1e12a23edf87daa7) )
	ROM_FILL       (           0x200000, 0x100000, 0 )

	ROM_REGION16_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD16_WORD( "d67-13", 0x00000,  0x80000,  CRC(42e7690d) SHA1(5f00f3f814653733bf9a5cb010675799de02fa76) )	/* STY, spritemap */

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "d67-01", 0x000000, 0x200000, CRC(a2f18122) SHA1(640014c6e6d66c59fe0accf370ad3bab9f40429a) )	/* Ensoniq samples */
	ROM_LOAD16_BYTE( "d67-02", 0xc00000, 0x200000, CRC(fceb715e) SHA1(9326513acb0696669d4f2345649ab37c8c6ed171) )
ROM_END

ROM_START( undrfirj )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )	/* 2048K for 68020 code (CPU A) */
	ROM_LOAD32_BYTE( "d67-19", 0x00000, 0x80000, CRC(1d88fa5a) SHA1(5e498efb9535a8f4e82b5525390b8bde7c45c07e) )
	ROM_LOAD32_BYTE( "d67-18", 0x00001, 0x80000, CRC(f41ae7fd) SHA1(bdd0df01b11205c263d2fa280746826b831d58bc) )
	ROM_LOAD32_BYTE( "d67-17", 0x00002, 0x80000, CRC(34e030b7) SHA1(62c270c817199a56e647ea74849fe5c07717ac18) )
	ROM_LOAD32_BYTE( "d67-16", 0x00003, 0x80000, CRC(c6e62f26) SHA1(6a430916f829a4b0240ccf8477dcbb1f39a26e90) )

	ROM_REGION( 0x140000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "d67-20", 0x100000, 0x20000,  CRC(974ebf69) SHA1(8a5de503c514bf0da0c956e2dfdf0cfb83ea1f72) )
	ROM_LOAD16_BYTE( "d67-21", 0x100001, 0x20000,  CRC(8fc6046f) SHA1(28522ce5c5900f74d3faa86710256a7201b32500) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d67-08", 0x000000, 0x200000, CRC(56730d44) SHA1(110872714b3c26a82473c7b80c120918b91b1b4b) )	/* SCR 16x16 tiles */
	ROM_LOAD16_BYTE( "d67-09", 0x000001, 0x200000, CRC(3c19f9e3) SHA1(7ba8475d37cbf8bf38029124afdf62c915c8668d) )

	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "d67-03", 0x000003, 0x200000, CRC(3b6e99a9) SHA1(1e0e66763ddfa18a2d291626b245633555092959) )	/* OBJ 16x16 tiles */
	ROM_LOAD32_BYTE( "d67-04", 0x000002, 0x200000, CRC(8f2934c9) SHA1(ead95b34eec3a6df27199edcbdd5595bc6555a50) )
	ROM_LOAD32_BYTE( "d67-05", 0x000001, 0x200000, CRC(e2e7dcf3) SHA1(185dbd0489931123a295139dc0a045ad239018fb) )
	ROM_LOAD32_BYTE( "d67-06", 0x000000, 0x200000, CRC(a2a63488) SHA1(a1ed140cc3757c3c05a0a822089c6efc83bf4805) )
	ROM_LOAD32_BYTE( "d67-07", 0x800000, 0x200000, CRC(189c0ee5) SHA1(de85b39dc67f31ef80800ff6ec9a391652eb12e4) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d67-10", 0x000000, 0x100000, CRC(d79e6ce9) SHA1(8b38302971816d599cdaa3279cb6395441373c6f) )	/* PIV 8x8 tiles, 6bpp */
	ROM_LOAD16_BYTE( "d67-11", 0x000001, 0x100000, CRC(7a401bb3) SHA1(47257a6a4b37ec1ceb4e974b776ee3ea30db06fa) )
	ROM_LOAD       ( "d67-12", 0x300000, 0x100000, CRC(67b16fec) SHA1(af0f9f50516331780ef6cfab1e12a23edf87daa7) )
	ROM_FILL       (           0x200000, 0x100000, 0 )

	ROM_REGION16_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD16_WORD( "d67-13", 0x00000,  0x80000,  CRC(42e7690d) SHA1(5f00f3f814653733bf9a5cb010675799de02fa76) )	/* STY, spritemap */

	ROM_REGION16_BE( 0x1000000, REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "d67-01", 0x000000, 0x200000, CRC(a2f18122) SHA1(640014c6e6d66c59fe0accf370ad3bab9f40429a) )	/* Ensoniq samples */
	ROM_LOAD16_BYTE( "d67-02", 0xc00000, 0x200000, CRC(fceb715e) SHA1(9326513acb0696669d4f2345649ab37c8c6ed171) )
ROM_END

ROM_START( cbombers )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )    /* 2048K for 68020 code (CPU A) */
	ROM_LOAD32_BYTE( "d83_39.ic17", 0x00000, 0x80000, CRC(b9f48284) SHA1(acc5d412e8900dda483a89a1ac1febd6d5735f3c) )
	ROM_LOAD32_BYTE( "d83_41.ic4",  0x00001, 0x80000, CRC(a2f4c8be) SHA1(0f8f3b5ecff34d8c35af1ab11bb5528b52e30109) )
	ROM_LOAD32_BYTE( "d83_40.ic3",  0x00002, 0x80000, CRC(b05f59ea) SHA1(e46a31737f44be2a3d478b8010fe0d6383290e03) )
	ROM_LOAD32_BYTE( "d83_38.ic16", 0x00003, 0x80000, CRC(0a10616c) SHA1(c9cfc8c870f8a989f004d2db4f6fb76e5b7b7f9b) )

	ROM_REGION( 0x140000, REGION_CPU2, 0 )   /* Sound cpu */
	ROM_LOAD16_BYTE( "d83_26.ic37", 0x100000, 0x20000, CRC(4f49b484) SHA1(96daa3cb7fa4aae3aedc91ec27d85945311dfcc9) )
	ROM_LOAD16_BYTE( "d83_27.ic38", 0x100001, 0x20000, CRC(2aa1a237) SHA1(b809f75bbbbb4eb5d0df725aaa31aae8a6fba552) )

	ROM_REGION( 0x40000, REGION_CPU3, 0 ) /* 256K for 68000 code (CPU B) */
	ROM_LOAD16_BYTE( "d83_28.ic26", 0x00001, 0x20000, CRC(06328ef7) SHA1(90a14649e56221e47b87958896f6eae4556265c2) )
	ROM_LOAD16_BYTE( "d83_29.ic27", 0x00000, 0x20000, CRC(771b4080) SHA1(a47c3a6abc07a6a61b694d32baa0ad4c25045841) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d83_04.ic8", 0x000000, 0x200000, CRC(79f36cce) SHA1(2c8dc4cd5c4aa335c1e45888f5947acf94fa628a) )
	ROM_LOAD16_BYTE( "d83_05.ic7", 0x000001, 0x200000, CRC(7787e495) SHA1(1758de5fdd1d12727368d08d7d4752c3756fc23e) )

	ROM_REGION( 0x1800000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "d83_06.ic28", 0x000003, 0x200000, CRC(4b71944e) SHA1(e8ed190280c7378fb4edcb192cef0d4d62582ad5) )
	ROM_LOAD32_BYTE( "d83_07.ic30", 0x000002, 0x200000, CRC(29861b61) SHA1(76562b0243c1bc38623c0ef9d20de7572a979e37) )
	ROM_LOAD32_BYTE( "d83_08.ic32", 0x000001, 0x200000, CRC(a0e81e01) SHA1(96ad8cfc849caaf85350cfc7cf23ad23635a3813) )
	ROM_LOAD32_BYTE( "d83_09.ic45", 0x000000, 0x200000, CRC(7e4dec50) SHA1(4d8c1be739d425d8ded07774094b775f35a915bf) )
	ROM_LOAD32_BYTE( "d83_11.ic41", 0x800003, 0x100000, CRC(a790e490) SHA1(9c57405ef2ef3368eb0958a3e43601110c1cc90d) )
	ROM_LOAD32_BYTE( "d83_12.ic29", 0x800002, 0x100000, CRC(2f237b0d) SHA1(2ecb947671d263a77510bfebda03f883b55b8df4) )
	ROM_LOAD32_BYTE( "d83_13.ic31", 0x800001, 0x100000, CRC(c2cceeb6) SHA1(3ec932655326caed13a40394bbf8e8baf836de2a) )
	ROM_LOAD32_BYTE( "d83_14.ic44", 0x800000, 0x100000, CRC(8b6f4f12) SHA1(6a28004d287f00627622376aa3d6704f2684a6f3) )
	ROM_LOAD32_BYTE( "d83_10.ic43", 0xc00000, 0x200000, CRC(36c440a0) SHA1(31685d3cdf4e39e1365df7e6a588c28f95d7e0a8) )
	ROM_LOAD32_BYTE( "d83_15.ic42", 0x1400000, 0x100000, CRC(1b71175e) SHA1(60ad38ce97fd7995ff2f29d6b1a3b873dc2f0eb3) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d83_16.ic19", 0x000000, 0x100000, CRC(d364cf1e) SHA1(ee43f50edf50ec840acfb98b1314140ee9693839) )
	ROM_LOAD16_BYTE( "d83_17.ic5",  0x000001, 0x100000, CRC(0ffe737c) SHA1(5923a4edf9d0c8339f793840c2bdc691e2c651e6) )
	ROM_LOAD       ( "d83_18.ic6",  0x300000, 0x100000, CRC(87979155) SHA1(0ffafa970f9f9c98f8938104b97e63d2b5757804) )
	ROM_FILL       (                0x200000, 0x100000, 0 )

	ROM_REGION16_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD16_BYTE( "d83_31.ic10", 0x000001, 0x40000, CRC(85c37961) SHA1(15ea5c4904d910575e984e146c8941dff913d45f) )
	ROM_LOAD16_BYTE( "d83_32.ic11", 0x000000, 0x40000, CRC(b0db2559) SHA1(2bfae2dbe164b42e95d0a93fab82b7040c3fbc56) )

	ROM_REGION( 0x40000, REGION_USER2, 0 )
	ROM_LOAD( "d83_30.ic9", 0x00000,  0x40000,  CRC(eb86dc67) SHA1(31c7b6f30ff912fafed4b87ce8bf603ee17d1664) )

	ROM_REGION16_BE( 0x1000000,  REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "d83_01.ic40", 0xc00000, 0x200000, CRC(912799f4) SHA1(22f69e61519d2cddcfc4e4c9601e78a9d5265d5b) )
	ROM_LOAD16_BYTE( "d83_02.ic39", 0x000000, 0x200000, CRC(2abca020) SHA1(3491a95651ca89b7fe6d040b8576fa7646bfe84b) )
	ROM_RELOAD     (                0x400000, 0x200000 )
	ROM_LOAD16_BYTE( "d83_03.ic18", 0x800000, 0x200000, CRC(1b2d9ec3) SHA1(ead6b5542ad3987ef0f9ea01ce7f960abc9119b3) )
ROM_END

ROM_START( cbombersj )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )    /* 2048K for 68020 code (CPU A) */
	ROM_LOAD32_BYTE( "d83_34.ic17", 0x00000, 0x80000, CRC(78e85cb1) SHA1(96219ee6eba04f6b490bb46dab99147036104791) )
	ROM_LOAD32_BYTE( "d83_36.ic4",  0x00001, 0x80000, CRC(fdc936cf) SHA1(834c5a50387a63098d3f9b8ff0baf5d72d3c319c) )
	ROM_LOAD32_BYTE( "d83_35.ic3",  0x00002, 0x80000, CRC(b0379b1e) SHA1(fc398f5f67d1347b594563a3703db4c0e01d5dff) )
	ROM_LOAD32_BYTE( "d83_33.ic16", 0x00003, 0x80000, CRC(72fb42a7) SHA1(7c0b02be604b8b90c444a973962d4b29a5b8b047) )

	ROM_REGION( 0x140000, REGION_CPU2, 0 )   /* Sound cpu */
	ROM_LOAD16_BYTE( "d83_26.ic37", 0x100000, 0x20000, CRC(4f49b484) SHA1(96daa3cb7fa4aae3aedc91ec27d85945311dfcc9) )
	ROM_LOAD16_BYTE( "d83_27.ic38", 0x100001, 0x20000, CRC(2aa1a237) SHA1(b809f75bbbbb4eb5d0df725aaa31aae8a6fba552) )

	ROM_REGION( 0x40000, REGION_CPU3, 0 ) /* 256K for 68000 code (CPU B) */
	ROM_LOAD16_BYTE( "d83_28.ic26", 0x00001, 0x20000, CRC(06328ef7) SHA1(90a14649e56221e47b87958896f6eae4556265c2) )
	ROM_LOAD16_BYTE( "d83_29.ic27", 0x00000, 0x20000, CRC(771b4080) SHA1(a47c3a6abc07a6a61b694d32baa0ad4c25045841) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d83_04.ic8", 0x000000, 0x200000, CRC(79f36cce) SHA1(2c8dc4cd5c4aa335c1e45888f5947acf94fa628a) )
	ROM_LOAD16_BYTE( "d83_05.ic7", 0x000001, 0x200000, CRC(7787e495) SHA1(1758de5fdd1d12727368d08d7d4752c3756fc23e) )

	ROM_REGION( 0x1800000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "d83_06.ic28", 0x000003, 0x200000, CRC(4b71944e) SHA1(e8ed190280c7378fb4edcb192cef0d4d62582ad5) )
	ROM_LOAD32_BYTE( "d83_07.ic30", 0x000002, 0x200000, CRC(29861b61) SHA1(76562b0243c1bc38623c0ef9d20de7572a979e37) )
	ROM_LOAD32_BYTE( "d83_08.ic32", 0x000001, 0x200000, CRC(a0e81e01) SHA1(96ad8cfc849caaf85350cfc7cf23ad23635a3813) )
	ROM_LOAD32_BYTE( "d83_09.ic45", 0x000000, 0x200000, CRC(7e4dec50) SHA1(4d8c1be739d425d8ded07774094b775f35a915bf) )
	ROM_LOAD32_BYTE( "d83_11.ic41", 0x800003, 0x100000, CRC(a790e490) SHA1(9c57405ef2ef3368eb0958a3e43601110c1cc90d) )
	ROM_LOAD32_BYTE( "d83_12.ic29", 0x800002, 0x100000, CRC(2f237b0d) SHA1(2ecb947671d263a77510bfebda03f883b55b8df4) )
	ROM_LOAD32_BYTE( "d83_13.ic31", 0x800001, 0x100000, CRC(c2cceeb6) SHA1(3ec932655326caed13a40394bbf8e8baf836de2a) )
	ROM_LOAD32_BYTE( "d83_14.ic44", 0x800000, 0x100000, CRC(8b6f4f12) SHA1(6a28004d287f00627622376aa3d6704f2684a6f3) )
	ROM_LOAD32_BYTE( "d83_10.ic43", 0xc00000, 0x200000, CRC(36c440a0) SHA1(31685d3cdf4e39e1365df7e6a588c28f95d7e0a8) )
	ROM_LOAD32_BYTE( "d83_15.ic42", 0x1400000, 0x100000, CRC(1b71175e) SHA1(60ad38ce97fd7995ff2f29d6b1a3b873dc2f0eb3) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "d83_16.ic19", 0x000000, 0x100000, CRC(d364cf1e) SHA1(ee43f50edf50ec840acfb98b1314140ee9693839) )
	ROM_LOAD16_BYTE( "d83_17.ic5",  0x000001, 0x100000, CRC(0ffe737c) SHA1(5923a4edf9d0c8339f793840c2bdc691e2c651e6) )
	ROM_LOAD       ( "d83_18.ic6",  0x300000, 0x100000, CRC(87979155) SHA1(0ffafa970f9f9c98f8938104b97e63d2b5757804) )
	ROM_FILL       (                0x200000, 0x100000, 0 )

	ROM_REGION16_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD16_BYTE( "d83_31.ic10", 0x000001, 0x40000, CRC(85c37961) SHA1(15ea5c4904d910575e984e146c8941dff913d45f) )
	ROM_LOAD16_BYTE( "d83_32.ic11", 0x000000, 0x40000, CRC(b0db2559) SHA1(2bfae2dbe164b42e95d0a93fab82b7040c3fbc56) )

	ROM_REGION( 0x40000, REGION_USER2, 0 )
	ROM_LOAD( "d83_30.ic9", 0x00000,  0x40000,  CRC(eb86dc67) SHA1(31c7b6f30ff912fafed4b87ce8bf603ee17d1664) )

	ROM_REGION16_BE( 0x1000000,  REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "d83_01.ic40", 0xc00000, 0x200000, CRC(912799f4) SHA1(22f69e61519d2cddcfc4e4c9601e78a9d5265d5b) )
	ROM_LOAD16_BYTE( "d83_02.ic39", 0x000000, 0x200000, CRC(2abca020) SHA1(3491a95651ca89b7fe6d040b8576fa7646bfe84b) )
	ROM_RELOAD     (                0x400000, 0x200000 )
	ROM_LOAD16_BYTE( "d83_03.ic18", 0x800000, 0x200000, CRC(1b2d9ec3) SHA1(ead6b5542ad3987ef0f9ea01ce7f960abc9119b3) )
ROM_END



ROM_START( cbombersp )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )    // 2048K for 68020 code (CPU A)
	ROM_LOAD32_BYTE( "hh.bin",  0x00000, 0x80000, CRC(8ee7b6c2) SHA1(033bc2c207b458113ff9f81cb90d22b7984f49de) )
	ROM_LOAD32_BYTE( "hl.bin",  0x00001, 0x80000, CRC(63683ed8) SHA1(e6b57300f712fac3cf54f70ca255e514018a1c81) )
	ROM_LOAD32_BYTE( "lh.bin",  0x00002, 0x80000, CRC(77ad9acb) SHA1(73bc156c377d42e7c752d71686ca5ad3dc506053) )
	ROM_LOAD32_BYTE( "ll.bin",  0x00003, 0x80000, CRC(54c060a5) SHA1(7623eb418734d2adc4e913b5a99d4caeb857b3ac) )

	ROM_REGION( 0x140000, REGION_CPU2, 0 )   // Sound cpu
	ROM_LOAD16_BYTE( "ic47_eprg_h_up_c5c5.bin", 0x100000, 0x20000, CRC(cb0d11b1) SHA1(0ac192f31f531defe088ed899aadbaa0798df4d6) )
	ROM_LOAD16_BYTE( "ic46_eprg_l_low_d5e.bin", 0x100001, 0x20000, CRC(567ae215) SHA1(32d40d1c24e014ffb52a81df4a0d5adb20f0048c) )

	ROM_REGION( 0x40000, REGION_CPU3, 0 ) // 256K for 68000 code (CPU B)
	ROM_LOAD16_BYTE( "5-l.bin", 0x00001, 0x20000, CRC(aed4c3c0) SHA1(004f83ce0739cb2839022eb4d83f82e54776914f) )
	ROM_LOAD16_BYTE( "5-h.bin", 0x00000, 0x20000, CRC(c6ec60e4) SHA1(554f19926e050ff2b8c56c30f174aca5a3fff845) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "scp0ll_ic7.bin",          0x000000, 0x80000, CRC(b1af439d) SHA1(a13fb4242808d1f3fc629988912c8186a99fb878) )
	ROM_LOAD32_BYTE( "scp0hl_ic9.bin",          0x000001, 0x80000, CRC(5b6e413e) SHA1(7eaee158e985a20b5c228b476ee102f88311423a) )
	ROM_LOAD32_BYTE( "scp0lh_ic22.bin",         0x000002, 0x80000, CRC(d5109bca) SHA1(c4c5b8dbc1139718d2aa73413b1b206f9df10fed) )
	ROM_LOAD32_BYTE( "scp0aa_2b04_ic35.bin",    0x000003, 0x80000, CRC(b8ec56bd) SHA1(00191fd4b2e315a5a18f2e690a45b6f6a6ebb3d2) )
	ROM_LOAD32_BYTE( "scp1ll_ic8.bin",          0x200000, 0x80000, CRC(eaa5839a) SHA1(80c7bb1151253a23934b65110db973641f7a073e) )
	ROM_LOAD32_BYTE( "scp1hl_ic24.bin",         0x200001, 0x80000, CRC(46d198ba) SHA1(d9c9ddb23ad8f2abbd0ab2322d31d929085f0591) )
	ROM_LOAD32_BYTE( "scp1lh_ic23.bin",         0x200002, 0x80000, CRC(7c9f0035) SHA1(a5632bd11426ba2cf0016847a3c08a2b90498271) )
	ROM_LOAD32_BYTE( "scp1hh_ic36.bin",         0x200003, 0x80000, CRC(24f545d8) SHA1(c5ae0e714ed4765f3416cb58bc9cfccfbf78081c) )

	ROM_REGION( 0x1800000, REGION_GFX2, ROMREGION_DISPOSE )
	// tiles 0x00000 - 0x07fff
	ROMX_LOAD( "obj0l_ic29.bin",   0x0000003, 0x80000, CRC(4b954950) SHA1(cafd9ba3128aa2e7dbde959a705aff8db6c311fa) , ROM_SKIP(7) ) // bp 1
	ROMX_LOAD( "obj16l_ic20.bin",  0x0000002, 0x80000, CRC(b53932c0) SHA1(94ea6ccc29bd7b7e94d7494aaf0cc19b67c4ce72) , ROM_SKIP(7) ) // bp 2
	ROMX_LOAD( "obj32l_ic51.bin",  0x0000001, 0x80000, CRC(f23f7253) SHA1(cbff5aee79d1b4990a35d5fc55e348aa81b2b5d3) , ROM_SKIP(7) ) // bp 3
	ROMX_LOAD( "obj48l_ic53.bin",  0x0000000, 0x80000, CRC(85bb6b75) SHA1(ccff9c331b54e3c3618d205ffd4b999fcf861e09) , ROM_SKIP(7) ) // bp 4
	ROMX_LOAD( "obj8l_ic4.bin",    0x0000007, 0x80000, CRC(d26140bb) SHA1(c882150c2914f23c5a27b43a228c62401cdb1a73) , ROM_SKIP(7) ) // bp 1
	ROMX_LOAD( "obj24l_ic50.bin",  0x0000006, 0x80000, CRC(27c76f27) SHA1(c3e96015abc042420632e489e202684c358a5f42) , ROM_SKIP(7) ) // bp 2
	ROMX_LOAD( "obj40l_ic52.bin",  0x0000005, 0x80000, CRC(09aaf6c5) SHA1(f2838fbf5ede241939562a65ecb908d882f267e2) , ROM_SKIP(7) ) // bp 3
	ROMX_LOAD( "obj56l_ic54.bin",  0x0000004, 0x80000, CRC(8b6bacdf) SHA1(fe22f700147f641339ef754b3e5896279b5c2800) , ROM_SKIP(7) ) // bp 4
	ROMX_LOAD( "obj64l_ic55.bin",  0x0c00000, 0x80000, CRC(18c976f1) SHA1(4fa53bbfaacd8ca9c7bec3aeff13481302aeefdc) , ROM_SKIP(7) ) // bp 5
	ROMX_LOAD( "obj72l_ic56.bin",  0x0c00004, 0x80000, CRC(6a1b5ebc) SHA1(632d19d73c8cf75e3c428f1870e75c07b6846be5) , ROM_SKIP(7) ) // bp 5
	// tiles 0x08000 - 0x0ffff
	ROMX_LOAD( "obj0h_ic30.bin",   0x0400003, 0x80000, CRC(4c436ad2) SHA1(34fb6bb5aa7b131dda2d167a5dacc09090698dd1) , ROM_SKIP(7) ) // bp 1
	ROMX_LOAD( "obj16h_ic21.bin",  0x0400002, 0x80000, CRC(5406b71e) SHA1(5e10db4d9052b903987629aa1df34bd4380ad29b) , ROM_SKIP(7) ) // bp 2
	ROMX_LOAD( "ic65_4b57.bin",    0x0400001, 0x80000, CRC(6a1a8054) SHA1(20a001b1957fa67297959b86e20a0c2a417c2452) , ROM_SKIP(7) ) // bp 3
	ROMX_LOAD( "ic67_0956.bin",    0x0400000, 0x80000, CRC(abe445dd) SHA1(22ad1ace2ee80a2297e4a004b6f5cd52c7c9b187) , ROM_SKIP(7) ) // bp 4
	ROMX_LOAD( "obj8l_ic5.bin",    0x0400007, 0x80000, CRC(46b028eb) SHA1(02bb36108636695bc6db523991b34713599b2716) , ROM_SKIP(7) ) // bp 1
	ROMX_LOAD( "ic64_5aba.bin",    0x0400006, 0x80000, CRC(0912b766) SHA1(62a8c0f2341b7e1f7b7ba0e194eda39f502209e6) , ROM_SKIP(7) ) // bp 2
	ROMX_LOAD( "ic66_4ae9.bin",    0x0400005, 0x80000, CRC(77aafe1a) SHA1(35392526d5b6d974bff3bda316677fa0890b76a8) , ROM_SKIP(7) ) // bp 3
	ROMX_LOAD( "ic68_1429.bin",    0x0400004, 0x80000, CRC(2e5857e5) SHA1(04bdd2e7cc89b14c7d7d72de45a7cce74cbcf7bd) , ROM_SKIP(7) ) // bp 4
	ROMX_LOAD( "ic69_43b4.bin",    0x1000000, 0x80000, CRC(d08643be) SHA1(aacc324276c92b5786298b474ad6277fb802c470) , ROM_SKIP(7) ) // bp 5
	ROMX_LOAD( "ic70_4504.bin",    0x1000004, 0x80000, CRC(3cf5d9d7) SHA1(35cc8b403550fb9928003308139de05ceb7bd5bb) , ROM_SKIP(7) ) // bp 5
	// tiles 0x10000 - 0x17fff
	ROMX_LOAD( "obj0l_ic31.bin",   0x0800003, 0x80000, CRC(9a20d601) SHA1(f3e561ed8687b65c6328c10d69c74615e693a83f) , ROM_SKIP(7) ) // bp 1
	ROMX_LOAD( "obj16l_ic33.bin",  0x0800002, 0x80000, CRC(ea9df360) SHA1(4b0b50679ae4438f117b631b76b02a5170cc292a) , ROM_SKIP(7) ) // bp 2
	ROMX_LOAD( "ic77_36ac.bin",    0x0800001, 0x80000, CRC(75628014) SHA1(62f8ba94be047e54c3577e6ecd68b3e613905a9f) , ROM_SKIP(7) ) // bp 3
	ROMX_LOAD( "ic79_ef40.bin",    0x0800000, 0x80000, CRC(6af34bbf) SHA1(7bd021fc041465abe8dc6ac1ada634ae7314fec9) , ROM_SKIP(7) ) // bp 4
	ROMX_LOAD( "obj8l_ic6.bin",    0x0800007, 0x80000, CRC(2037aad5) SHA1(0c837a39d92e99536bcd949c80fbdd4c37ad7e34) , ROM_SKIP(7) ) // bp 1
	ROMX_LOAD( "ic76_443a.bin",    0x0800006, 0x80000, CRC(e5820610) SHA1(0eefb098766187983d88017863ce3a3418807428) , ROM_SKIP(7) ) // bp 2
	ROMX_LOAD( "ic78_989c.bin",    0x0800005, 0x80000, CRC(23ec2896) SHA1(b5fcc88d2428ff9341690a0e0a1fc25e42684680) , ROM_SKIP(7) ) // bp 3
	ROMX_LOAD( "ic80_d511.bin",    0x0800004, 0x80000, CRC(37da5baf) SHA1(a78ac413de08a1ff70ab14561b75df633a9e5be8) , ROM_SKIP(7) ) // bp 4
	ROMX_LOAD( "ic81_e150.bin",    0x1400000, 0x80000, CRC(48dbc4fb) SHA1(acec207d05a8ea615f27216fbfd567cc630e5191) , ROM_SKIP(7) ) // bp 5
	ROMX_LOAD( "ic82_3d3d.bin",    0x1400004, 0x80000, CRC(3e62970e) SHA1(82970accb4ce29034e7b97b74c831ec0314c5a8f) , ROM_SKIP(7) ) // bp 5

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "ic44_scc1.bin",  0x000001, 0x080000, CRC(868d0d3d) SHA1(29251d545548856296b5ae32a96f2eeef2418dc4) )
	ROM_LOAD16_BYTE( "ic43_scc4.bin",  0x000000, 0x080000, CRC(2f170ee4) SHA1(2b8f07186c9f7589e1af131b8c377443a29bd149) )
	ROM_LOAD       ( "ic45_5cc2.bin",  0x300000, 0x080000, CRC(7ae48d63) SHA1(2a8b291f0a683ed5b0c39d221737956b6fc72fa5) )
	ROM_FILL       (                   0x200000, 0x080000, 0 )
	ROM_LOAD16_BYTE( "ic58_f357.bin",  0x100001, 0x080000, CRC(16486967) SHA1(c2fd6c9f21232656b52ab589ac61f94aa728524e) )
	ROM_LOAD16_BYTE( "ic57_1a62.bin",  0x100000, 0x080000, CRC(afd45e35) SHA1(6d7c0729c7d2b204473679b97923130e289f429d) )
	ROM_LOAD       ( "ic59_7cce.bin",  0x380000, 0x080000, CRC(ee762199) SHA1(d56e96feeedba8b77f8f18cb380d2902ca3f1e50) )
	ROM_FILL       (                   0x280000, 0x080000, 0 )

	ROM_REGION16_LE( 0x80000, REGION_USER1, 0 )
	ROM_LOAD16_BYTE( "st8_ic2.bin", 0x000001, 0x40000, CRC(d74254d8) SHA1(f4a4f9d95f70edf74d937be067d6a9f68a955ea7) )
	ROM_LOAD16_BYTE( "st0_ic1.bin", 0x000000, 0x40000, CRC(c414c479) SHA1(e585502fcfa6ae2a36a66927ba2c49e49317f149) )

	ROM_REGION( 0x40000, REGION_USER2, 0 )
	ROM_LOAD( "st16_ic3.bin", 0x00000,  0x40000, CRC(c4ff6b2f) SHA1(65795bcb3749cce9c291204cd64fafa529317e14) )

	ROM_REGION16_BE( 0x1000000,  REGION_SOUND1, ROMREGION_SOUNDONLY | ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE("ic84_0816_wave0.bin", 0x000000, 0x080000, CRC(c30c71fd) SHA1(4240a23120b85f9daf6a462770185614fa758e4d) )
	ROM_LOAD16_BYTE("ic85_8058_wave1.bin", 0x100000, 0x080000, CRC(fe37d544) SHA1(75c23b4e4e2efbbda1724a557f68c3bf1e5016c6) )
	ROM_LOAD16_BYTE("ic86_9e88_wave2.bin", 0x200000, 0x080000, CRC(d6dcb45d) SHA1(ec69fb0a9fc6f7e72850775656e9fcd185889825) )
	ROM_LOAD16_BYTE("ic87_42e7_wave3.bin", 0x300000, 0x080000, CRC(fe52856b) SHA1(fc68301d8b514a142cb38c3aa7a081674bdba6ca) )
	ROM_LOAD16_BYTE("ic88_2704_wave4.bin", 0x400000, 0x080000, CRC(cba55d36) SHA1(944188bb3d9466ba246cabc57db6b965f2a902a0) )
	// wave 5 not populated
	// wave 6 not populated
	// wave 7 not populated
	// wave 8 not populated
	// wave 9 not populated
	// wave 10 not populated
	// wave 11 not populated
	// wave 12 not populated
	// wave 13 not populated
	ROM_LOAD16_BYTE("ic107_3a9c_wave14.bin", 0xe00000, 0x080000, CRC(26312451) SHA1(9f947a11592fd8420fc581914bf16e7ade75390c) )    // -std-
	ROM_LOAD16_BYTE("ic108_a148_wave15.bin", 0xf00000, 0x080000, CRC(2edaa9dc) SHA1(72fead505c4f44e5736ff7d545d72dfa37d613e2) )    // -std-
ROM_END


static READ32_HANDLER( main_cycle_r )
{
	int ptr;
	if ((activecpu_get_sp()&2)==0) ptr=undrfire_ram[(activecpu_get_sp()&0x1ffff)/4];
	else ptr=(((undrfire_ram[(activecpu_get_sp()&0x1ffff)/4])&0x1ffff)<<16) |
	(undrfire_ram[((activecpu_get_sp()&0x1ffff)/4)+1]>>16);

	if (activecpu_get_pc()==0x682 && ptr==0x1156)
		cpu_spinuntil_int();

	return undrfire_ram[0x4f8/4];
}

DRIVER_INIT( undrfire )
{
	unsigned int offset,i;
	UINT8 *gfx = memory_region(REGION_GFX3);
	int size=memory_region_length(REGION_GFX3);
	int data;

	/* Speedup handlers */
	install_mem_read32_handler(0, 0x2004f8, 0x2004fb, main_cycle_r);

	/* make piv tile GFX format suitable for gfxdecode */
	offset = size/2;
	for (i = size/2+size/4; i<size; i++)
	{
		int d1,d2,d3,d4;

		/* Expand 2bits into 4bits format */
		data = gfx[i];
		d1 = (data>>0) & 3;
		d2 = (data>>2) & 3;
		d3 = (data>>4) & 3;
		d4 = (data>>6) & 3;

		gfx[offset] = (d1<<2) | (d2<<6);
		offset++;

		gfx[offset] = (d3<<2) | (d4<<6);
		offset++;
	}
}


DRIVER_INIT( cbombers )
{
	unsigned int offset,i;
	UINT8 *gfx = memory_region(REGION_GFX3);
	int size=memory_region_length(REGION_GFX3);
	int data;

	
	/* make piv tile GFX format suitable for gfxdecode */
	offset = size/2;
	for (i = size/2+size/4; i<size; i++)
	{
		int d1,d2,d3,d4;

		/* Expand 2bits into 4bits format */
		data = gfx[i];
		d1 = (data>>0) & 3;
		d2 = (data>>2) & 3;
		d3 = (data>>4) & 3;
		d4 = (data>>6) & 3;

		gfx[offset] = (d1<<2) | (d2<<6);
		offset++;

		gfx[offset] = (d3<<2) | (d4<<6);
		offset++;
	}
}

GAME( 1993, undrfire,  0,        undrfire, undrfire, undrfire, ROT0, "Taito Corporation Japan", "Under Fire (World)" )
GAME( 1993, undrfiru,  undrfire, undrfire, undrfire, undrfire, ROT0, "Taito America Corporation", "Under Fire (US)" )
GAME( 1993, undrfirj,  undrfire, undrfire, undrfire, undrfire, ROT0, "Taito Corporation", "Under Fire (Japan)" )
GAME( 1994, cbombers,  0,        cbombers, cbombers, cbombers, ROT0, "Taito Corporation Japan", "Chase Bombers (World)" )
GAME( 1994, cbombersj, 0,        cbombers, cbombers, cbombers, ROT0, "Taito Corporation", "Chase Bombers (Japan)" )
GAME( 1994, cbombersp, 0,        cbombers, cbombers, cbombers, ROT0, "Taito Corporation", "Chase Bombers (Japan Prototype)" )
