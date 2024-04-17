/***************************************************************************

Pole Position    (c) 1982 Namco
Pole Position II (c) 1983 Namco

driver by Ernesto Corvi, Juergen Buchmueller, Alex Pasadyn, Aaron Giles, Nicola Salmoria


Custom ICs:
----------
CPU board:
06XX     interface to custom 5xXX
07XX     clock divider
08XX(x2) bus controller
10XX(x4) Z8002 bus controller
51XX     I/O
52XX     sample player
53XX     I/O
54XX     explosion sound generator

Video board:
02XX(x3) gfx data shifter and mixer (16-bit in, 4-bit out)
03XX(x2) ?
04XX     sprite address generator
07XX     clock divider
09XX     address bus interface


Memory maps:
-----------
Part of the address decoding is done by PALs so it is inferred by program behaviour

Z80:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
000xxxxxxxxxxxxx R   xxxxxxxx ROM 7H    program ROM
0010xxxxxxxxxxxx R   xxxxxxxx ROM 7F    program ROM
0011-xxxxxxxxxxx R/W xxxxxxxx CMOSRAMCS battery back-up RAM
010000xxxxxxxxxx R/W xxxxxxxx CSMB      work RAM                                      [1]
010000111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (POSI)       [1]
010001xxxxxxxxxx R/W xxxxxxxx CSMD      work RAM                                      [1]
010001111xxxxxxx R/W xxxxxxxx           portion holding sprite registers (SIZE, DATA) [1]
01001xxxxxxxxxxx R/W xxxxxxxx RAM 3F    1st half is road, 2nd half alpha tilemap      [1]
01010xxxxxxxxxxx R/W xxxxxxxx RAM 3E    background tilemap (1st half only)            [1]
1000--xxxxxxxxxx R/W xxxxxxxx RAMCS     work RAM
1000--1111xxxxxx R/W xxxxxxxx           portion holding the sound registers
1001---0-------- R/W xxxxxxxx IODBENBL  custom 06XX data
1001---1-------- R/W xxxxxxxx IODBENBL  custom 06XX control
1010--00-------- R   -------x READY     +5V
1010--00-------- R   ------x- READY     128V
1010--00-------- R   -----x-- READY     PWRUP (power line sense)
1010--00-------- R   ----x--- READY     ADC0804 INTR (end flag)
1010--01-------- R            n.c.
1010--10-------- R            n.c.
1010--11-------- R            n.c.
1010--00-----000   W -------x IRQON     Z80 IRQ enable/acknowledge
1010--00-----001   W -------x IOSEL     reset 5xXX chips
1010--00-----010   W -------x CLSON     sound enable [2]
1010--00-----011   W -------x GASEL     accelerator/brake select
1010--00-----100   W -------x RESB      reset Z8002 #1
1010--00-----101   W -------x RESA      reset Z8002 #2
1010--00-----110   W -------x SB0       start (goes to 51XX start button input)
1010--00-----111   W -------x CHACL     alpha layer enable color and msb
1010--01--------   W -------- WDR       watchdog reset
1010--10--------   W -------x XSOUND    engine enable
1010--10--------   W --xxxxx- XSOUND    engine pitch lsb
1010--11--------   W --xxxxxx XSON      engine pitch msb

[1] shared with the two Z8002, but the Z80 can only access the low 8 bits of these
    16-bit areas
[2] affects wave and engine, but not 54XX and 52XX. Note that for the engine, this
    clears the XSOUND and XSON latches.


Z80 I/O:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
---------------- R   xxxxxxxx           ADC0804 (accelerator/brake pedals)


Z8002 #1:

Address          Dir Data             Name      Description
---------------- --- ---------------- --------- -----------------------
00xxxxxxxxxxxxx- R   xxxxxxxxxxxxxxxx ROM 4L/3L program ROM
01xxxxxxxxxxxxx- R   xxxxxxxxxxxxxxxx ROM 4K/3K program ROM
011-------------   W ---------------x NMIACKB   Z8002 #2 NVI enable/acknowledge [1]
the rest of the memory map is common to the other Z8002


Z8002 #2:

Address          Dir Data             Name      Description
---------------- --- ---------------- --------- -----------------------
00xxxxxxxxxxxxx- R   xxxxxxxxxxxxxxxx ROM 4E/3E program ROM
01xxxxxxxxxxxxx- R   xxxxxxxxxxxxxxxx ROM 4D/3D program ROM
011-------------   W ---------------x NMIACKA   Z8002 #1 NVI enable/acknowledge [1]
the rest of the memory map is common to the other Z8002

[1] One Z8002 writes at $6000 and the other at $6002, but they did it only for clarity
    because the low address bits are ignored and the location is not shared.


Z8002 (common):

Address          Dir Data             Name      Description
---------------- --- ---------------- --------- -----------------------
10000xxxxxxxxxx- R/W xxxxxxxxxxxxxxxx CSMA/CSMB work RAM
10000111xxxxxxx- R/W xxxxxxxxxxxxxxxx           portion holding sprite registers (POSI)
10001xxxxxxxxxx- R/W xxxxxxxxxxxxxxxx CSMC/CSMD work RAM
10001111xxxxxxx- R/W xxxxxxxxxxxxxxxx           portion holding sprite registers (SIZE, DATA)
1001xxxxxxxxxxx- R/W xxxxxxxxxxxxxxxx RAM 4F/3F 1st half is road, 2nd half alpha tilemap
1010xxxxxxxxxxx- R/W xxxxxxxxxxxxxxxx RAM 4E/3E background tilemap (1st half only)
11---000--------   W ------xxxxxxxxxx VHP       background horizontal position
11---001--------   W ----xxxxxxxxxxxx RVP       road vertical position
11---010--------   W                  n.c.
11---011--------   W                  n.c.
11---100--------   W                  n.c.
11---101--------   W                  n.c.
11---110--------   W                  n.c.
11---111--------   W                  n.c.


Namco vs Atari ROM names and locations
--------------------------------------
* = not present

Location  ID (PP1)    ID (PP2)    Location  ID (PP1)                  ID (PP2)
--------  ----------  ----------  --------  ------------------------  ----------
CPU 8M    PP1-1       PP4-1       CPU 3L    136014-101                136014-176
CPU 8L    PP1-2       PP4-2       CPU 4L    136014-102                136014-177
   ?      PP1-3*      PP4-3*      CPU 3K    136014-112*               *
   ?      PP1-4*      PP4-4*      CPU 4K    136014-113*               *
CPU 4M    PP1-5       PP4-5       CPU 3E    136014-103                136014-178
CPU 4L    PP1-6       PP4-6       CPU 4E    136014-104                136014-179
CPU 3M    PP1-7*      PP4-7       CPU 3D    136014-114*               136014-184
CPU 3L    PP1-8*      PP4-8       CPU 4D    136014-115*               136014-185
CPU 6H    PP1-9       PP4-9       CPU 7H    136014-105 or 136014-160  136014-180
CPU 5H    PP1-10      PP4-10      CPU 7F    136014-116                136014-183
CPU 2E    PP1-11      <--         CPU 9C    136014-106 or 136014-147  <--
CPU 2F    PP1-12      <--         CPU 9A    136014-108*               *
CPU 1E    PP1-13      <--         CPU 8C    136014-107*               *
CPU 1F    PP1-14      <--         CPU 8A    136014-109*               *
CPU 6A    PP1-15      PP4-15      CPU 12F   136014-110 or 136014-148  136014-181
CPU 5A    PP1-16      PP4-16      CPU 12E   136014-111 or 136014-149  136014-182
   ?      PP1-1[pal]  <--         CPU 5C    PAL-1                     <--
   ?      PP1-2[pal]  <--         CPU 2N    PAL-1                     <--
   ?      PP1-3[pal]  <--         CPU 7C    PAL-3                     <--
CPU 9H    PP1-4[bpr]  <--         CPU 7L    136014-117                <--
   ?      PP1-5[bpr]  <--         CPU 11D   136014-118                <--

VID 5N    PP1-17      <--         VID 13J   136014-119 == 136014-150  <--
VID 5M    PP1-18      <--         VID 12J   136014-120 == 136014-151  <--
VID 4N    PP1-19      <--         VID 13K   136014-121 or 136014-152  136014-166
VID 4M    PP1-20      <--         VID 12K   136014-122 or 136014-153  136014-167
VID 3N    PP1-21      <--         VID 13L   136014-123 or 136014-154  136014-168
VID 3M    PP1-22      <--         VID 12L   136014-124 or 136014-155  136014-169
VID 2N    PP1-23*     PP4-23      VID 13M   136014-129*               136014-175
VID 2M    PP1-24*     PP4-24      VID 12M   136014-130*               136014-174
VID 1N    PP1-25      PP4-25      VID 13N   136014-125 or 136014-156  136014-170
VID 1M    PP1-26      PP4-26      VID 12N   136014-126 or 136014-157  136014-171
   ?      PP1-27      <--         VID 11N   136014-131                <--
VID 1F    PP1-28      PP4-28      VID 7N    136014-132 or 137205-001? 136014-172
VID 1E    PP1-29      PP4-29      VID 6N    136014-133 or 137205-001? 136014-173
VID 3A    PP1-30      <--         VID 2L    136014-127 == 136014-158  <--
VID 2A    PP1-31      <--         VID 2M    136014-128 == 136014-159  <--
VID 1A    PP1-32      <--         VID 2N    136014-134 or 137205-001? <--
VID 6M    PP1-6[bpr]  PP4-6[bpr]  VID 12H   136014-146                136014-192
VID 8L    PP1-7[bpr]  PP4-7[bpr]  VID 11E   136014-137                136014-186
VID 9L    PP1-8[bpr]  PP4-8[bpr]  VID 11D   136014-138                136014-187
VID 10L   PP1-9[bpr]  PP4-9[bpr]  VID 11C   136014-139                136014-188
VID 2H    PP1-10[bpr] PP4-10[bpr] VID 8M    136014-140                136014-189
VID 4D    PP1-11[bpr] PP4-11[bpr] VID 5K    136014-141                136014-190
VID 3C    PP1-12[bpr] PP4-12[bpr] VID 4L    136014-145                136014-191
VID 8E    PP1-13[bpr] <--         VID 6E    136014-135                <--
VID 9E    PP1-14[bpr] <--         VID 6D    136014-136                <--
   ?      PP1-15[bpr] <--         VID 2D    136014-142                <--
   ?      PP1-16[bpr] <--         VID 2C    136014-143                <--
VID 11A   PP1-17[bpr] <--         VID 2B    136014-144                <--


Notes:
-----
- Easter egg (both Pole Position and Pole Position II):
  - enter service mode
  - turn wheel to 04; change the shifter from LO to HI
  - turn wheel to 45; change the shifter from LO to HI
  - turn wheel to 55; change the shifter from LO to HI
  - turn wheel to 56; change the shifter from LO to HI
  - turn wheel to 91; change the shifter from LO to HI
  (c) 1982 NAMCO LTD. will appear on the screen.

- To reset the high score table, enter service mode, press the accelerator and
  change the shifter from LO to HI

- Pole Position II reports 'Manual Start' on the Test Mode. This is ok,
  because they had to accomodate the hardware from Pole Position I to allow
  track selection.

- Change POLEPOS_TOGGLE to 0 if you are using the original gearshift.

- The old version of the vertical scaling ROM, 136014-131, has (apart from
  some irrelevant differences) one bad bit.
  This seems to be a genuine error on Atari's part, since they replaced it with
  136014-231 which matches Namco's PP1-27. The bad bit should cause a tiny gfx
  glitch, though it's difficult to notice.


***************************************************************************/

#include "driver.h"
#include "machine/namcoio.h"
#include "sound/namco.h"
#include "sound/namco52.h"
#include "sound/namco54.h"


#define POLEPOS_TOGGLE	IPF_TOGGLE


/* from sndhrdw */
int polepos_sh_start(const struct MachineSound *msound);
void polepos_sh_stop(void);
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
WRITE_HANDLER( polepos_chacl_w );

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


/*************************************************************************************/
/* Pole Position II protection														 */
/*************************************************************************************/

READ16_HANDLER( polepos2_ic25_r )
{
	int result;
	/* protection states */
	static INT16 last_result;
	static INT8 last_signed;
	static UINT8 last_unsigned;

	offset = offset & 0x1ff;
	if (offset < 0x100)
	{
		last_signed = offset & 0xff;
		result = last_result & 0xff;
	}
	else
	{
		last_unsigned = offset & 0xff;
		result = (last_result >> 8) & 0xff;
		last_result = (INT8)last_signed * (UINT8)last_unsigned;
	}

/*	logerror("%04X: read IC25 @ %04X = %02X\n", activecpu_get_pc(), offset, result); */

	return result | (result << 8);
}


static int adc_input;
static int auto_start_mask;


static READ_HANDLER( polepos_adc_r )
{
	return readinputport(3 + adc_input);
}

static READ_HANDLER( polepos_ready_r )
{
	int ret = 0xff;

	if (cpu_getscanline() >= 128)
		ret ^= 0x02;

	ret ^= 0x08; /* ADC End Flag */

	return ret;
}


static WRITE_HANDLER( polepos_latch_w )
{
	int bit = data & 1;

	switch (offset)
	{
		case 0x00:	/* IRQON */
			cpu_interrupt_enable(0,bit);
			if (!bit)
				cpu_set_irq_line(0, 0, CLEAR_LINE);
			break;

		case 0x01:	/* IOSEL */
/*polepos_mcu_enable_w(offset,data); */
			break;

		case 0x02:	/* CLSON */
			polepos_sound_enable(bit);
			if (!bit)
			{
				polepos_engine_sound_lsb_w(0,0);
				polepos_engine_sound_msb_w(0,0);
			}
			break;

		case 0x03:	/* GASEL */
			adc_input = bit;
			break;

		case 0x04:	/* RESB */
			cpu_set_reset_line(1,bit ? CLEAR_LINE : ASSERT_LINE);
			break;

		case 0x05:	/* RESA */
			cpu_set_reset_line(2,bit ? CLEAR_LINE : ASSERT_LINE);
			break;

		case 0x06:	/* SB0 */
			auto_start_mask = 0xfb | (bit << 2);
			break;

		case 0x07:	/* CHACL */
			polepos_chacl_w(offset,data);
			break;
	}
}

static WRITE16_HANDLER( polepos_z8002_nvi_enable_w )
{
	int which = cpu_getactivecpu();

	data &= 1;

	cpu_interrupt_enable(which,data);
	if (!data)
		cpu_set_irq_line(which, 0, CLEAR_LINE);
}


static READ_HANDLER( in0_l )	{ return readinputport(0) & auto_start_mask; }	/* fire and start buttons */
static READ_HANDLER( in0_h )	{ return readinputport(0) >> 4; }	/* coins */
static READ_HANDLER( dipA_l )	{ return readinputport(1); }		/* dips A */
static READ_HANDLER( dipA_h )	{ return readinputport(1) >> 4; }	/* dips A */
static READ_HANDLER( dipB_l )	{ return readinputport(2); }		/* dips B */
static READ_HANDLER( dipB_h )	{ return readinputport(2) >> 4; }	/* dips B */
static READ_HANDLER( in1_l )	{ return readinputport(5); }		/* wheel */
static READ_HANDLER( in1_h )	{ return readinputport(5) >> 4; }	/* wheel */
static WRITE_HANDLER( out_0 )
{
/* no start lamps in pole position */
/*	set_led_status(1,data & 1); */
/*	set_led_status(0,data & 2); */
	coin_counter_w(1,~data & 4);
	coin_counter_w(0,~data & 8);
}
static WRITE_HANDLER( out_1 )
{
	coin_lockout_global_w(data & 1);
}

static struct namcoio_interface intf0 =
{
	{ in0_l, in0_h, dipB_l, dipB_h },	/* port read handlers */
	{ out_0, out_1 }					/* port write handlers */
};
static struct namcoio_interface intf1 =
{
	{ in1_l, in1_h, dipA_l, dipA_h, },	/* port read handlers */
	{ NULL, NULL }						/* port write handlers */
};


#include "cpu/z8000/z8000.h"
static MACHINE_INIT( polepos )
{
	int i;

	/* Reset all latches */
	for (i = 0;i < 8;i++)
		polepos_latch_w(i,0);

	namco_06xx_init(0, 0,
		NAMCOIO_51XX, &intf0,
		NAMCOIO_53XX_POLEPOS, &intf1,
		NAMCOIO_52XX, NULL,
		NAMCOIO_54XX, NULL);

	/* set the interrupt vectors (this shouldn't be needed) */
	cpu_irq_line_vector_w(1, 0, Z8000_NVI);
	cpu_irq_line_vector_w(2, 0, Z8000_NVI);
}



/*********************************************************************
 * CPU memory structures
 *********************************************************************/

static MEMORY_READ_START( z80_readmem )
    { 0x0000, 0x2fff, MRA_ROM },
	{ 0x3000, 0x37ff, MRA_RAM },	                    /* Battery Backup */
	{ 0x4000, 0x47ff, polepos_sprite_r },				/* Motion Object */
	{ 0x4800, 0x4bff, polepos_road_r }, 				/* Road Memory */
	{ 0x4c00, 0x4fff, polepos_alpha_r },				/* Alphanumeric (char ram) */
    { 0x5000, 0x57ff, polepos_view_r }, 				/* Background Memory */
	{ 0x8000, 0x83ff, MRA_RAM },						/* Sound Memory */
	{ 0x9000, 0x90ff, namco_06xx_0_data_r },
	{ 0x9100, 0x9100, namco_06xx_0_ctrl_r },
	{ 0xa000, 0xa000, polepos_ready_r },				/* READY */
MEMORY_END

static MEMORY_WRITE_START( z80_writemem )
    { 0x0000, 0x2fff, MWA_ROM },
	{ 0x3000, 0x37ff, MWA_RAM, &generic_nvram, &generic_nvram_size },	/* Battery Backup */
	{ 0x4000, 0x47ff, polepos_sprite_w },				/* Motion Object */
	{ 0x4800, 0x4bff, polepos_road_w },				/* Road Memory */
	{ 0x4c00, 0x4fff, polepos_alpha_w },				/* Alphanumeric (char ram) */
	{ 0x5000, 0x57ff, polepos_view_w },				/* Background Memory */
	{ 0x8000, 0x83bf, MWA_RAM },						/* Sound Memory */
	{ 0x83c0, 0x83ff, polepos_sound_w, &polepos_soundregs }, /* Sound data */
	{ 0x9000, 0x90ff, namco_06xx_0_data_w },
	{ 0x9100, 0x9100, namco_06xx_0_ctrl_w },
	{ 0xa000, 0xa007, polepos_latch_w },				/* misc latches */
	{ 0xa100, 0xa100, watchdog_reset_w },				/* Watchdog */
	{ 0xa200, 0xa200, polepos_engine_sound_lsb_w }, 	/* Car Sound ( Lower Nibble ) */
	{ 0xa300, 0xa300, polepos_engine_sound_msb_w },	/* Car Sound ( Upper Nibble ) */
MEMORY_END

static PORT_READ_START( z80_readport )
    { 0x00, 0x00, polepos_adc_r },
PORT_END

static PORT_WRITE_START( z80_writeport )
    { 0x00, 0x00, MWA_NOP },
PORT_END


/* the same memory map is used by both Z8002 CPUs; all RAM areas are shared */
static MEMORY_READ16_START( z8002_readmem )
    { 0x0000, 0x7fff, MRA16_ROM },
	{ 0x8000, 0x8fff, polepos_sprite16_r },	/* Motion Object */
	{ 0x9000, 0x97ff, polepos_road16_r },		/* Road Memory */
	{ 0x9800, 0x9fff, polepos_alpha16_r }, 	/* Alphanumeric (char ram) */
	{ 0xa000, 0xafff, polepos_view16_r },		/* Background memory */
MEMORY_END

/* the same memory map is used by both Z8002 CPUs; all RAM areas are shared */
static MEMORY_WRITE16_START( z8002_writemem )
        { 0x6000, 0x6003, polepos_z8002_nvi_enable_w },	/* NVI enable - *NOT* shared by the two CPUs */
        { 0x0000, 0x7fff, MWA16_ROM },
	{ 0x8000, 0x8fff, polepos_sprite16_w, &polepos_sprite16_memory },	/* Motion Object */
	{ 0x9000, 0x97ff, polepos_road16_w, &polepos_road16_memory },	/* Road Memory */
	{ 0x9800, 0x9fff, polepos_alpha16_w, &polepos_alpha16_memory },	/* Alphanumeric (char ram) */
	{ 0xa000, 0xafff, polepos_view16_w, &polepos_view16_memory },	/* Background memory */
	{ 0xc000, 0xc001, polepos_view16_hscroll_w },					/* Background horz scroll position */
	{ 0xc100, 0xc101, polepos_road16_vscroll_w },						/* Road vertical position */
MEMORY_END



/*********************************************************************
 * Input port definitions
 *********************************************************************/

INPUT_PORTS_START( polepos )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | POLEPOS_TOGGLE, "Gear Change", IP_KEY_DEFAULT, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* start 1, program controlled */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x60, 0x60, "Game Time" )
	PORT_DIPSETTING(	0x60, "90 secs." )
	PORT_DIPSETTING(	0x20, "100 secs." )
	PORT_DIPSETTING(	0x40, "110 secs." )
	PORT_DIPSETTING(	0x00, "120 secs." )
	PORT_DIPNAME( 0x80, 0x80, "Nr. of Laps" )
	PORT_DIPSETTING(	0x80, "3" )
	PORT_DIPSETTING(	0x00, "4" )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x07, 0x07, "Extended Rank" )
	PORT_DIPSETTING(	0x07, "A" )
	PORT_DIPSETTING(	0x03, "B" )
	PORT_DIPSETTING(	0x05, "C" )
	PORT_DIPSETTING(	0x01, "D" )
	PORT_DIPSETTING(	0x06, "E" )
	PORT_DIPSETTING(	0x02, "F" )
	PORT_DIPSETTING(	0x04, "G" )
	PORT_DIPSETTING(	0x00, "H" )
	PORT_DIPNAME( 0x38, 0x38, "Practice Rank" )
	PORT_DIPSETTING(	0x38, "A" )
	PORT_DIPSETTING(	0x18, "B" )
	PORT_DIPSETTING(	0x28, "C" )
	PORT_DIPSETTING(	0x08, "D" )
	PORT_DIPSETTING(	0x30, "E" )
	PORT_DIPSETTING(	0x10, "F" )
	PORT_DIPSETTING(	0x20, "G" )
	PORT_DIPSETTING(	0x00, "H" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ))
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL2, 100, 16, 0, 0x90, IP_KEY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, IP_KEY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x00, IPT_DIAL, 30, 4, 0, 0 )
INPUT_PORTS_END


INPUT_PORTS_START( poleposa )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | POLEPOS_TOGGLE, "Gear Change", IP_KEY_DEFAULT, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* start 1, program controlled */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x06, 0x06, "Game Time" )
	PORT_DIPSETTING(	0x06, "90 secs." )
	PORT_DIPSETTING(	0x02, "100 secs." )
	PORT_DIPSETTING(	0x04, "110 secs." )
	PORT_DIPSETTING(	0x00, "120 secs." )
	PORT_DIPNAME( 0x01, 0x01, "Nr. of Laps" )
	PORT_DIPSETTING(	0x01, "3" )
	PORT_DIPSETTING(	0x00, "4" )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0xe0, 0xe0, "Practice Rank" )
	PORT_DIPSETTING(	0xe0, "A" )
	PORT_DIPSETTING(	0x60, "B" )
	PORT_DIPSETTING(	0xa0, "C" )
	PORT_DIPSETTING(	0x20, "D" )
	PORT_DIPSETTING(	0xc0, "E" )
	PORT_DIPSETTING(	0x40, "F" )
	PORT_DIPSETTING(	0x80, "G" )
	PORT_DIPSETTING(	0x00, "H" )
	PORT_DIPNAME( 0x1c, 0x1c, "Extended Rank" )
	PORT_DIPSETTING(	0x1c, "A" )
	PORT_DIPSETTING(	0x0c, "B" )
	PORT_DIPSETTING(	0x14, "C" )
	PORT_DIPSETTING(	0x04, "D" )
	PORT_DIPSETTING(	0x18, "E" )
	PORT_DIPSETTING(	0x08, "F" )
	PORT_DIPSETTING(	0x10, "G" )
	PORT_DIPSETTING(	0x00, "H" )
	PORT_DIPNAME( 0x02, 0x02, "Speed Unit" )
	PORT_DIPSETTING(	0x00, "mph" )
	PORT_DIPSETTING(	0x02, "km/h" )
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ))
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL2, 100, 16, 0, 0x90, IP_KEY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, IP_KEY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x00, IPT_DIAL, 30, 4, 0, 0 )
INPUT_PORTS_END


INPUT_PORTS_START( polepos2 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | POLEPOS_TOGGLE, "Gear Change", IP_KEY_DEFAULT, IP_JOY_DEFAULT ) /* Gear */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )	/* start 1, program controlled */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* DSW A */
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x80, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(	0x18, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x04, 0x04, "Speed Unit" )
	PORT_DIPSETTING(	0x00, "mph" )
	PORT_DIPSETTING(	0x04, "km/h" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ))
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )	/* docs say "freeze", but it doesn't seem to work */
	PORT_DIPSETTING(	0x01, DEF_STR( Off ))
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START	/* DSW B */
	PORT_DIPNAME( 0x80, 0x80, "Game Time" )
	PORT_DIPSETTING(	0x80, "90 secs." )
	PORT_DIPSETTING(	0x00, "120 secs." )
	PORT_DIPNAME( 0x60, 0x60, "Practice Rank" )
	PORT_DIPSETTING(	0x20, "A" )
	PORT_DIPSETTING(	0x60, "B" )
	PORT_DIPSETTING(	0x40, "C" )
	PORT_DIPSETTING(	0x00, "D" )
	PORT_DIPNAME( 0x18, 0x18, "Extended Rank" )
	PORT_DIPSETTING(	0x08, "A" )
	PORT_DIPSETTING(	0x18, "B" )
	PORT_DIPSETTING(	0x10, "C" )
	PORT_DIPSETTING(	0x00, "D" )
	PORT_DIPNAME( 0x06, 0x06, "Goal" )
	PORT_DIPSETTING(	0x02, "3" )
	PORT_DIPSETTING(	0x06, "4" )
	PORT_DIPSETTING(	0x04, "5" )
	PORT_DIPSETTING(	0x00, "6" )
	PORT_DIPNAME( 0x01, 0x01, "Speed" )
	PORT_DIPSETTING(	0x01, "Average" )
	PORT_DIPSETTING(	0x00, "High" )

	PORT_START /* IN1 - Brake */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL2, 100, 16, 0, 0x90, IP_KEY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN2 - Accel */
	PORT_ANALOGX( 0xff, 0x00, IPT_PEDAL, 100, 16, 0, 0x90, IP_KEY_DEFAULT, IP_KEY_DEFAULT, IP_JOY_DEFAULT, IP_JOY_DEFAULT )

	PORT_START /* IN3 - Steering */
	PORT_ANALOG ( 0xff, 0x00, IPT_DIAL, 30, 4, 0, 0 )
INPUT_PORTS_END



/*********************************************************************
 * Graphics layouts
 *********************************************************************/

static struct GfxLayout charlayout_2bpp =
{
	8,8,
	RGN_FRAC(1,1),
	2,
	{ 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8*2
};

static struct GfxLayout bigspritelayout =
{
	32,32,
	RGN_FRAC(1,2),
	4,
	{ 0, 4, RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4 },
	{  0,  1,  2,  3,  8,  9, 10, 11,
	  16, 17, 18, 19, 24, 25, 26, 27,
	  32, 33, 34, 35, 40, 41, 42, 43,
	  48, 49, 50, 51, 56, 57, 58, 59},
	{  0*64,  1*64,  2*64,	3*64,  4*64,  5*64,  6*64,	7*64,
		8*64,  9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64,
	  16*64, 17*64, 18*64, 19*64, 20*64, 21*64, 22*64, 23*64,
	  24*64, 25*64, 26*64, 27*64, 28*64, 29*64, 30*64, 31*64 },
	32*64
};

static struct GfxLayout smallspritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 4, RGN_FRAC(1,2), RGN_FRAC(1,2)+4
	},
	{  0,  1,  2,  3,  8,  9, 10, 11,
	  16, 17, 18, 19, 24, 25, 26, 27 },
	{ 0*32,  1*32,  2*32,  3*32,  4*32,  5*32,  6*32,  7*32,
	  8*32,	 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	16*32
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_2bpp,   0x0000, 128 },
	{ REGION_GFX2, 0, &charlayout_2bpp,   0x0200,  64 },
	{ REGION_GFX3, 0, &smallspritelayout, 0x0300, 128 },
	{ REGION_GFX4, 0, &bigspritelayout,   0x0300, 128 },
	{ -1 } /* end of array */
};


/*********************************************************************
 * Sound interfaces
 *********************************************************************/

static struct namco_interface namco_interface =
{
	24576000/512, 	/* sample rate */
	8,				/* number of voices */
	100, 			/* playback volume */
	REGION_SOUND1,	/* memory region */
	1				/* stereo */
};

static struct namco_52xx_interface namco_52xx_interface =
{
	24576000/16,	/* 1.536 MHz */
	25,				/* volume */
	REGION_SOUND3	/* memory region */
};

static struct namco_54xx_interface namco_54xx_interface =
{
	24576000/16,		/* 1.536 MHz */
	{ 100, 100, 100 }	/* volume of the three outputs */
};

static struct CustomSound_interface custom_interface =
{
	polepos_sh_start,
	polepos_sh_stop,
	NULL
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
	MDRV_CPU_ADD(Z80, 24576000/8)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(z80_readmem,z80_writemem)
	MDRV_CPU_PORTS(z80_readport,z80_writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,2)	/* 64V */

	MDRV_CPU_ADD(Z8000, 24576000/8)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(z8002_readmem,z8002_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

	MDRV_CPU_ADD(Z8000, 24576000/8)	/* 3.072 MHz */
	MDRV_CPU_MEMORY(z8002_readmem,z8002_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_assert,1)

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
	MDRV_COLORTABLE_LENGTH(0x0f00)

	MDRV_PALETTE_INIT(polepos)
	MDRV_VIDEO_START(polepos)
	MDRV_VIDEO_UPDATE(polepos)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(NAMCO_15XX, namco_interface)
	MDRV_SOUND_ADD(NAMCO_52XX, namco_52xx_interface)
	MDRV_SOUND_ADD(NAMCO_54XX, namco_54xx_interface)
	MDRV_SOUND_ADD(CUSTOM, custom_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface)
MACHINE_DRIVER_END


/*********************************************************************
 * ROM definitions
 *********************************************************************/

ROM_START( polepos )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "pp1_9b.6h",    0x0000, 0x2000, CRC(94436b70) SHA1(7495c2a8c3928c59146760d19e672afee01c5b17) )
	ROM_LOAD( "136014.116",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "pp1_1b.8m",     0x0001, 0x2000, CRC(361c56dd) SHA1(6e4abf98b10077c6980e8aa3861f0233135ea68f) )
	ROM_LOAD16_BYTE( "pp1_2b.8l",     0x0000, 0x2000, CRC(582b530a) SHA1(4fc38aa8b70816e14b321ec778090f6c7e7f1640) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "pp1_5b.4m",     0x0001, 0x2000, CRC(5cdf5294) SHA1(dbdf327a541fd71aadafda9c925fa4cf7f7c4a24) )
	ROM_LOAD16_BYTE( "pp1_6b.4l",     0x0000, 0x2000, CRC(81696272) SHA1(27041a7c24297a6f317537c44922b51d2b2278a6) )

	/* graphics data */
	ROM_REGION( 0x01000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD( "pp1_28.1f",     0x0000, 0x1000, CRC(5b277daf) SHA1(0b1feeb2c0c63a5db5ba9b0115aa1b2388636a70) )

	ROM_REGION( 0x01000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD( "pp1_29.1e",     0x0000, 0x1000, CRC(706e888a) SHA1(af1aa2199fcf73a3afbe760857ff117865350954) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "pp1_25.1n",     0x0000, 0x2000, CRC(ac8e28c1) SHA1(13bc2bf4be28d9ae987f79034f9532272b3a2543) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "pp1_26.1m",     0x2000, 0x2000, CRC(94443079) SHA1(413d7b762c8dff541675e96874be6ee0251d3581) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "136014.150",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "pp1_19.4n",    0x2000, 0x2000, CRC(43ff83e1) SHA1(8f830549a629b019125e59801e5027e4e4b3c0f2) )
	ROM_LOAD( "pp1_21.3n",    0x4000, 0x2000, CRC(5f958eb4) SHA1(b56d84e5e5e0ddeb0e71851ba66e5fa1b1409551) )
	ROM_LOAD( "136014.151",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "pp1_20.4m",    0xa000, 0x2000, CRC(ec18075b) SHA1(af7be549c5fa47551a8dca4c0a531552147fa50f) )
	ROM_LOAD( "pp1_22.3m",    0xc000, 0x2000, CRC(1d2f30b1) SHA1(1d88a3069e9b15febd2835dd63e5511b3b2a6b45) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.158",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.159",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.231",   0x0000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.137",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette */
	ROM_LOAD( "136014.138",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette */
	ROM_LOAD( "136014.139",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette */
	ROM_LOAD( "136014.140",   0x0300, 0x0100, CRC(1e8d0491) SHA1(e8bf1db5c1fb04a35763099965cf5c588240bde5) )    /* alpha color */
	ROM_LOAD( "136014.141",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.145",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color */
	ROM_LOAD( "pp1_6.bpr",    0x0c00, 0x0400, CRC(2f1079ee) SHA1(18a27998a78deff13dd198f3668a7e92f084f467) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.110",   0x0000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound */
	ROM_LOAD( "136014.111",   0x2000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound */

	ROM_REGION( 0x8000, REGION_SOUND3, 0 )
	ROM_LOAD( "pp1_11.2e",    0x0000, 0x2000, CRC(45b9bfeb) SHA1(ff8c690471944d414931fb88666594ef608997f8) )    /* voice */
	ROM_LOAD( "pp1_12.2f",    0x2000, 0x2000, CRC(a31b4be5) SHA1(38298093bb97ea8647fe187359cae05b65e1c616) )    /* voice */
	ROM_LOAD( "pp1_13.1e",    0x4000, 0x2000, CRC(a4237466) SHA1(88a397276038cc2fc05f2c18472e6b7cef167f2e) )    /* voice */
	ROM_LOAD( "pp1_14.1f",    0x6000, 0x2000, CRC(944580f9) SHA1(c76f529cae718674ce97a1a599a3c6eaf6bf561a) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleposa )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "136014.105",   0x0000, 0x2000, CRC(c918c043) SHA1(abc1aa3d7b670b5a65b4565dc646cd3c4edf4e6f) )
	ROM_LOAD( "136014.116",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "136014.101",   0x0001, 0x2000, CRC(8c2cf172) SHA1(57c774afab79599ac3f434113c3170fbb3d42620) )
	ROM_LOAD16_BYTE( "136014.102",   0x0000, 0x2000, CRC(51018857) SHA1(ed28d44d172a01f76461f556229d1fe3a1b779a7) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "136014.203",   0x0001, 0x2000, CRC(eedea6e7) SHA1(e1459c5e3f824e589e624c3acb18a183fd160df6) )
	ROM_LOAD16_BYTE( "136014.204",   0x0000, 0x2000, CRC(c52c98ed) SHA1(2e33c487deaf8afb941e07e511a9828d2d8f6b31) )

	/* graphics data */
	ROM_REGION( 0x01000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD( "136014.132",   0x0000, 0x1000, CRC(a949aa85) SHA1(2d6414196b6071101001128418233e585279ffb9) )

	ROM_REGION( 0x01000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "136014.133",   0x0000, 0x1000, CRC(3f0eb551) SHA1(39516d0f72f4e3b03df9451d2dbe081d6c71a508) )    /* 2bpp view layer */

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "136014.156",   0x0000, 0x2000, CRC(e7a09c93) SHA1(47cc5c6776333bba8454a3df9e2f6e7de4a465e1) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "136014.157",   0x2000, 0x2000, CRC(dee7d687) SHA1(ea34b51c91f6915b74a4a7b53ddb4ff36b72bf66) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "136014.150",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "136014.152",   0x2000, 0x2000, CRC(a7e3a1c6) SHA1(b7340318afaa4b5f416fe4444899579242cd36c2) )
	ROM_LOAD( "136014.154",   0x4000, 0x2000, CRC(8992d381) SHA1(3bf2544dbe88132137acec2c064a104a74139ec7) )
	ROM_LOAD( "136014.151",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "136014.153",   0xa000, 0x2000, CRC(6c5c6e68) SHA1(dce74ee0e69e0fc0a1942a489c2065381239f0f1) )
	ROM_LOAD( "136014.155",   0xc000, 0x2000, CRC(111896ad) SHA1(15032b4c859231373bebfa640421fdcc8ba9d211) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.158",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.159",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.231",   0x0000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.137",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette */
	ROM_LOAD( "136014.138",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette */
	ROM_LOAD( "136014.139",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette */
	ROM_LOAD( "136014.140",   0x0300, 0x0100, CRC(1e8d0491) SHA1(e8bf1db5c1fb04a35763099965cf5c588240bde5) )    /* alpha color */
	ROM_LOAD( "136014.141",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.145",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color */
	ROM_LOAD( "136014.146",   0x0c00, 0x0400, CRC(ca4ba741) SHA1(de93d738bd27e24dbc4a8378d2c120ef8388c261) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.110",   0x0000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound */
	ROM_LOAD( "136014.111",   0x2000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound */

	ROM_REGION( 0x6000, REGION_SOUND3, 0 )
	ROM_LOAD( "136014.106",   0x0000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( polepos1 )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "136014.105",   0x0000, 0x2000, CRC(c918c043) SHA1(abc1aa3d7b670b5a65b4565dc646cd3c4edf4e6f) )
	ROM_LOAD( "136014.116",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "136014.101",   0x0001, 0x2000, CRC(8c2cf172) SHA1(57c774afab79599ac3f434113c3170fbb3d42620) )
	ROM_LOAD16_BYTE( "136014.102",   0x0000, 0x2000, CRC(51018857) SHA1(ed28d44d172a01f76461f556229d1fe3a1b779a7) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "136014.103",   0x0001, 0x2000, CRC(af4fc019) SHA1(1bb6c0f3ffada2e1df72e1767581f8e8bb2b18f9) )
	ROM_LOAD16_BYTE( "136014.104",   0x0000, 0x2000, CRC(ba0045f3) SHA1(aedb8d8c56407963aa4ffb66243288c8fd6d845a) )

	/* graphics data */
	ROM_REGION( 0x01000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD( "136014.132",   0x0000, 0x1000, CRC(a949aa85) SHA1(2d6414196b6071101001128418233e585279ffb9) )

	ROM_REGION( 0x01000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD( "136014.133",   0x0000, 0x1000, CRC(3f0eb551) SHA1(39516d0f72f4e3b03df9451d2dbe081d6c71a508) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "136014.156",   0x0000, 0x2000, CRC(e7a09c93) SHA1(47cc5c6776333bba8454a3df9e2f6e7de4a465e1) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "136014.157",   0x2000, 0x2000, CRC(dee7d687) SHA1(ea34b51c91f6915b74a4a7b53ddb4ff36b72bf66) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "136014.150",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "136014.152",   0x2000, 0x2000, CRC(a7e3a1c6) SHA1(b7340318afaa4b5f416fe4444899579242cd36c2) )
	ROM_LOAD( "136014.154",   0x4000, 0x2000, CRC(8992d381) SHA1(3bf2544dbe88132137acec2c064a104a74139ec7) )
	ROM_LOAD( "136014.151",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "136014.153",   0xa000, 0x2000, CRC(6c5c6e68) SHA1(dce74ee0e69e0fc0a1942a489c2065381239f0f1) )
	ROM_LOAD( "136014.155",   0xc000, 0x2000, CRC(111896ad) SHA1(15032b4c859231373bebfa640421fdcc8ba9d211) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.158",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.159",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.131",   0x0000, 0x1000, CRC(5921777f) SHA1(4d9c91a26e0d84fbbe08f748d6e0364311ed6f73) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.137",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette */
	ROM_LOAD( "136014.138",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette */
	ROM_LOAD( "136014.139",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette */
	ROM_LOAD( "136014.140",   0x0300, 0x0100, CRC(1e8d0491) SHA1(e8bf1db5c1fb04a35763099965cf5c588240bde5) )    /* alpha color */
	ROM_LOAD( "136014.141",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.145",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color */
	ROM_LOAD( "136014.146",   0x0c00, 0x0400, CRC(ca4ba741) SHA1(de93d738bd27e24dbc4a8378d2c120ef8388c261) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.110",   0x0000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound */
	ROM_LOAD( "136014.111",   0x2000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound */

	ROM_REGION( 0x6000, REGION_SOUND3, 0 )
	ROM_LOAD( "136014.106",   0x0000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( topracer )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "pp1_9b.6h",    0x0000, 0x2000, CRC(94436b70) SHA1(7495c2a8c3928c59146760d19e672afee01c5b17) )
	ROM_LOAD( "136014.116",   0x2000, 0x1000, CRC(7174bcb7) SHA1(460326a6cea201db2df813013c95562a222ea95d) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "tr1b.bin",     0x0001, 0x2000, CRC(127f0750) SHA1(97ae6c6f8086187c7cdb8bff5fec94914791890b) )
	ROM_LOAD16_BYTE( "tr2b.bin",     0x0000, 0x2000, CRC(6bd4ff6b) SHA1(cf992de39a8cf7804961a8e6773fc4f7feb1878b) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "tr5b.bin",     0x0001, 0x2000, CRC(4e5f7b9c) SHA1(d26b1f24dd9ef00388987890bc5b95d4db403815) )
	ROM_LOAD16_BYTE( "tr6b.bin",     0x0000, 0x2000, CRC(9d038ada) SHA1(7a9496c3fb93fd1945393656f8510a0c6421a9ab) )

	/* graphics data */
	ROM_REGION( 0x01000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD( "tr28.bin",     0x0000, 0x1000, CRC(b8217c96) SHA1(aba311bc3c4b118ba322a00e33e2d5cbe7bc6e4a) )

	ROM_REGION( 0x01000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD( "tr29.bin",     0x0000, 0x1000, CRC(c6e15c21) SHA1(e2a70b3f7ce51a003068eb75d9fe82548f0206d7) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "trus25.bin",   0x0000, 0x2000, CRC(9e1a9c3b) SHA1(deca026c39093119985d1486ed61abc3e6e5705c) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "trus26.bin",   0x2000, 0x2000, CRC(3b39a176) SHA1(d04c9c2c9129c8dd7d7eab24c43502b67162407c) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "pp17.bin",     0x0000, 0x2000, CRC(613ab0df) SHA1(88aa4500275aae010fc9783c1d8d843feab89afa) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "tr19.bin",     0x2000, 0x2000, CRC(f8e7f551) SHA1(faa23c55bc43325e6f71936be970f2ca144697d8) )
	ROM_LOAD( "tr21.bin",     0x4000, 0x2000, CRC(17c798b0) SHA1(ae2047bc0e4e8c85e1de09c39c200ea8f7c6a72e) )
	ROM_LOAD( "pp18.bin",     0x8000, 0x2000, CRC(5fd933e3) SHA1(5b27a8519234c935308f943cd58abc1efc463726) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "tr20.bin",     0xa000, 0x2000, CRC(7053e219) SHA1(97700fbe887e2d11c9f9a0937147725f6787f081) )
	ROM_LOAD( "tr22.bin",     0xc000, 0x2000, CRC(f48917b2) SHA1(2823cfc33ae97ef979d92e2eeeb94c95f1f3d9f3) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.158",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.159",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.231",   0x0000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.137",   0x0000, 0x0100, CRC(f07ff2ad) SHA1(e1f3cb10a03d23f8c1d422acf271dba4e7b98cb1) )    /* red palette */
	ROM_LOAD( "136014.138",   0x0100, 0x0100, CRC(adbde7d7) SHA1(956ac5117c1e310f554ac705aa2dc24a796c36a5) )    /* green palette */
	ROM_LOAD( "136014.139",   0x0200, 0x0100, CRC(ddac786a) SHA1(d1860105bf91297533ccc4aa6775987df198d0fa) )    /* blue palette */
	ROM_LOAD( "10p.bin",      0x0300, 0x0100, CRC(5af3f710) SHA1(da13d17acf8abd0f6ebb4b51b23c3324c6197b7d) )    /* alpha color */
	ROM_LOAD( "136014.141",   0x0400, 0x0100, CRC(0e4fe8a0) SHA1(d330b1e5ebccf5bbefcf71486fd80d816de38196) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.145",   0x0800, 0x0400, CRC(7afc7cfc) SHA1(ba2407f6eff124e881b354f13205a4c058b7cf60) )    /* road color */
	ROM_LOAD( "pp1_6.bpr",    0x0c00, 0x0400, BAD_DUMP CRC(2f1079ee) SHA1(18a27998a78deff13dd198f3668a7e92f084f467) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.110",   0x0000, 0x2000, CRC(b5ad4d5f) SHA1(c07e77a050200d6fe9952031f971ca35f4d15ff8) )    /* engine sound */
	ROM_LOAD( "136014.111",   0x2000, 0x2000, CRC(8fdd2f6f) SHA1(3818dc94c60cd78c4212ab7a4367cf3d98166ee6) )    /* engine sound */

	ROM_REGION( 0x6000, REGION_SOUND3, 0 )
	ROM_LOAD( "136014.106",   0x0000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( polepos2 )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "pp4_9.6h",      0x0000, 0x2000, CRC(bcf87004) SHA1(0c60cbb777fe72dfd11c6f3e9da806a515cd0f8a) )
	ROM_LOAD( "136014.183",    0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

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
	ROM_LOAD( "pp4_28.1f",     0x0000, 0x2000, CRC(280dde7d) SHA1(b7c7fb3a5076aa4d0e0cf3256ece9a6194315626) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD( "136014.173",   0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "pp4_25.1n",     0x0000, 0x2000, CRC(fd098e65) SHA1(2c497f1d278ba6730752706a0d1b5a5a0fec3d5b) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "pp4_26.1m",     0x2000, 0x2000, CRC(35ac62b3) SHA1(21038a78eb73d520e3e1ae8e1c0047d06b94cdab) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "136014.119",  0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "pp1_19.4n",     0x2000, 0x2000, CRC(43ff83e1) SHA1(8f830549a629b019125e59801e5027e4e4b3c0f2) )
	ROM_LOAD( "pp1_21.3n",     0x4000, 0x2000, CRC(5f958eb4) SHA1(b56d84e5e5e0ddeb0e71851ba66e5fa1b1409551) )
	ROM_LOAD( "pp4_23.2n",     0x6000, 0x2000, CRC(9e056fcd) SHA1(8545e0a9b6ebf8c2903321ceb9c4d693db10d750) )
	ROM_LOAD( "136014.120",  0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "pp1_20.4m",     0xa000, 0x2000, CRC(ec18075b) SHA1(af7be549c5fa47551a8dca4c0a531552147fa50f) )
	ROM_LOAD( "pp1_22.3m",     0xc000, 0x2000, CRC(1d2f30b1) SHA1(1d88a3069e9b15febd2835dd63e5511b3b2a6b45) )
	ROM_LOAD( "pp4_24.2m",     0xe000, 0x2000, CRC(795268cf) SHA1(84136142ef4bdcd97ede2209ecb16745960ac393) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.127",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.128",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.231",   0x0000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.186",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette */
	ROM_LOAD( "136014.187",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette */
	ROM_LOAD( "136014.188",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette */
	ROM_LOAD( "136014.189",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color */
	ROM_LOAD( "136014.190",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.191",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color */
	ROM_LOAD( "pp4-6.6m",     0x0c00, 0x0400, CRC(647212b5) SHA1(ad58dfebd0ce8226285c2671c3b7797852c26d07) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.181",   0x0000, 0x2000, CRC(7d93bc1c) SHA1(dad7c0aa24aef593c84e21f7f8858ca7ada86364) )    /* engine sound */
	ROM_LOAD( "136014.182",   0x2000, 0x2000, CRC(7d93bc1c) SHA1(dad7c0aa24aef593c84e21f7f8858ca7ada86364) )    /* engine sound */

	ROM_REGION( 0x8000, REGION_SOUND3, 0 )
	ROM_LOAD( "pp1_11.2e",     0x0000, 0x2000, CRC(45b9bfeb) SHA1(ff8c690471944d414931fb88666594ef608997f8) )    /* voice */
	ROM_LOAD( "pp1_12.2f",     0x2000, 0x2000, CRC(a31b4be5) SHA1(38298093bb97ea8647fe187359cae05b65e1c616) )    /* voice */
	ROM_LOAD( "pp1_13.1e",     0x4000, 0x2000, CRC(a4237466) SHA1(88a397276038cc2fc05f2c18472e6b7cef167f2e) )    /* voice */
	ROM_LOAD( "pp1_14.1f",     0x6000, 0x2000, CRC(944580f9) SHA1(c76f529cae718674ce97a1a599a3c6eaf6bf561a) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleps2a )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "136014.180",   0x0000, 0x2000, CRC(f85212c4) SHA1(666e55a7662247e72393b105b3e719be4233f1ff) )
	ROM_LOAD( "136014.183",   0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "136014.176",   0x0001, 0x2000, CRC(8aeaec98) SHA1(76b3bbb64a17090bf28858f1e91d2206a3beaf5b) )
	ROM_LOAD16_BYTE( "136014.177",   0x0000, 0x2000, CRC(7051df35) SHA1(cf23118ab05f5af273d756f97e6453496a276c9a) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "136014.178",   0x0001, 0x2000, CRC(eac35cfa) SHA1(f96005b3b63d85fc30695ab746af79c60f2f1341) )
	ROM_LOAD16_BYTE( "136014.179",   0x0000, 0x2000, CRC(613e917d) SHA1(97c139f8aa7bd871a907e72980757b83f99fd8a0) )
	ROM_LOAD16_BYTE( "136014.184",   0x4001, 0x2000, CRC(d893c4ed) SHA1(60d39abefbb0c8df68864a30b1f5fcbf4780c86c) )
	ROM_LOAD16_BYTE( "136014.185",   0x4000, 0x2000, CRC(899de75e) SHA1(4a16535115e37a3d342b2cb53f610a87c0d0abe1) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD( "136014.172",   0x0000, 0x2000, CRC(fbe5e72f) SHA1(07965d6e98ac1332ac6192b5e9cc927dd9eb706f) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD( "136014.173",   0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "136014.170",   0x0000, 0x2000, CRC(455d79a0) SHA1(03ef7c58f3145d9a6a461ef1aea3b5a49e653f80) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "136014.171",   0x2000, 0x2000, CRC(78372b81) SHA1(5defaf2074c1ab4d13dc36a190c658ddf7f7931b) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "136014.119",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "136014.166",   0x2000, 0x2000, CRC(2b0517bd) SHA1(ebe447ba3dcd8a3b56f47d707483074f61953fec) )
	ROM_LOAD( "136014.168",   0x4000, 0x2000, CRC(4d7916d9) SHA1(052745f252f51bfdd456e54cf7b8d22ab3aace27) )
	ROM_LOAD( "136014.175",   0x6000, 0x2000, CRC(bd6df480) SHA1(58f39fa3ae43d94fe42dc51da341384a9c3879ae) )
	ROM_LOAD( "136014.120",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "136014.167",   0xa000, 0x2000, CRC(411e21b5) SHA1(9659ee429d819926b5e5b12c41b968ae6e7f186e) )
	ROM_LOAD( "136014.169",   0xc000, 0x2000, CRC(662ff24b) SHA1(4cf8509034742c2bec8a96c7a786dafdf5875e4f) )
	ROM_LOAD( "136014.174",   0xe000, 0x2000, CRC(f0c571dc) SHA1(9e6839e9e203fc120a0389f4e11c9d46a817dbdf) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.127",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.128",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.231",   0x0000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.186",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette */
	ROM_LOAD( "136014.187",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette */
	ROM_LOAD( "136014.188",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette */
	ROM_LOAD( "136014.189",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color */
	ROM_LOAD( "136014.190",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.191",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color */
	ROM_LOAD( "136014.192",   0x0c00, 0x0400, CRC(caddb0b0) SHA1(e41b89f2b40bf8f93546012f373ae63dcae870da) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.181",   0x0000, 0x2000, CRC(7d93bc1c) SHA1(dad7c0aa24aef593c84e21f7f8858ca7ada86364) )    /* engine sound */
	ROM_LOAD( "136014.182",   0x2000, 0x2000, CRC(7d93bc1c) SHA1(dad7c0aa24aef593c84e21f7f8858ca7ada86364) )    /* engine sound */

	ROM_REGION( 0x6000, REGION_SOUND3, 0 )
	ROM_LOAD( "136014.106",   0x0000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


ROM_START( poleps2b )
	/* Z80 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "136014.180",   0x0000, 0x2000, CRC(f85212c4) SHA1(666e55a7662247e72393b105b3e719be4233f1ff) )
	ROM_LOAD( "136014.183",   0x2000, 0x1000, CRC(a9d4c380) SHA1(6048a8e858824936901e8e3e6b65d7505ccd82b4) )

	/* Z8002 #1 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD16_BYTE( "3lcpu.rom",    0x0001, 0x2000, CRC(cf95a6b7) SHA1(6a8419af8a52d3a8c88663b67845e4cb18e35723) )
	ROM_LOAD16_BYTE( "4lcpu.rom",    0x0000, 0x2000, CRC(643483f7) SHA1(020822f623b8e65c6016492266b6e328f7637b68) )
	ROM_LOAD16_BYTE( "cpu-4k.rom",   0x4000, 0x1000, CRC(97a496b3) SHA1(fe79d2376c5fa9fe242905a841a1c894a5ccfba4) )

	/* Z8002 #2 memory/ROM data */
	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD16_BYTE( "136014.178",   0x0001, 0x2000, CRC(eac35cfa) SHA1(f96005b3b63d85fc30695ab746af79c60f2f1341) )
	ROM_LOAD16_BYTE( "136014.179",   0x0000, 0x2000, CRC(613e917d) SHA1(97c139f8aa7bd871a907e72980757b83f99fd8a0) )
	ROM_LOAD16_BYTE( "136014.184",   0x4001, 0x2000, CRC(d893c4ed) SHA1(60d39abefbb0c8df68864a30b1f5fcbf4780c86c) )
	ROM_LOAD16_BYTE( "136014.185",   0x4000, 0x2000, CRC(899de75e) SHA1(4a16535115e37a3d342b2cb53f610a87c0d0abe1) )

	/* graphics data */
	ROM_REGION( 0x02000, REGION_GFX1, ROMREGION_DISPOSE ) 	/* 2bpp alpha layer */
	ROM_LOAD( "136014.172",   0x0000, 0x2000, CRC(fbe5e72f) SHA1(07965d6e98ac1332ac6192b5e9cc927dd9eb706f) )

	ROM_REGION( 0x02000, REGION_GFX2, ROMREGION_DISPOSE ) 	/* 2bpp view layer */
	ROM_LOAD( "136014.173",   0x0000, 0x2000, CRC(ec3ec6e6) SHA1(ae905d0ae802d1010b2c1f1a13e88a1f0dbe57da) )

	ROM_REGION( 0x04000, REGION_GFX3, ROMREGION_DISPOSE ) 	/* 4bpp 16x16 sprites */
	ROM_LOAD( "136014.170",   0x0000, 0x2000, CRC(455d79a0) SHA1(03ef7c58f3145d9a6a461ef1aea3b5a49e653f80) )    /* 4bpp sm sprites, planes 0+1 */
	ROM_LOAD( "136014.171",   0x2000, 0x2000, CRC(78372b81) SHA1(5defaf2074c1ab4d13dc36a190c658ddf7f7931b) )    /* 4bpp sm sprites, planes 2+3 */

	ROM_REGION( 0x10000, REGION_GFX4, ROMREGION_DISPOSE ) 	/* 4bpp 32x32 sprites */
	ROM_LOAD( "136014.119",   0x0000, 0x2000, CRC(2e134b46) SHA1(0938f5f9f5cc6d7c1096c569449db78dbc42da01) )    /* 4bpp lg sprites, planes 0+1 */
	ROM_LOAD( "136014.166",   0x2000, 0x2000, CRC(2b0517bd) SHA1(ebe447ba3dcd8a3b56f47d707483074f61953fec) )
	ROM_LOAD( "136014.168",   0x4000, 0x2000, CRC(4d7916d9) SHA1(052745f252f51bfdd456e54cf7b8d22ab3aace27) )
	ROM_LOAD( "136014.175",   0x6000, 0x2000, CRC(bd6df480) SHA1(58f39fa3ae43d94fe42dc51da341384a9c3879ae) )
	ROM_LOAD( "136014.120",   0x8000, 0x2000, CRC(6f9997d2) SHA1(b26d505266ccf23bfd867f881756c3251c80f57b) )    /* 4bpp lg sprites, planes 2+3 */
	ROM_LOAD( "136014.167",   0xa000, 0x2000, CRC(411e21b5) SHA1(9659ee429d819926b5e5b12c41b968ae6e7f186e) )
	ROM_LOAD( "136014.169",   0xc000, 0x2000, CRC(662ff24b) SHA1(4cf8509034742c2bec8a96c7a786dafdf5875e4f) )
	ROM_LOAD( "136014.174",   0xe000, 0x2000, CRC(f0c571dc) SHA1(9e6839e9e203fc120a0389f4e11c9d46a817dbdf) )

	ROM_REGION( 0x5000, REGION_GFX5, 0 ) 	/* road generation ROMs needed at runtime */
	ROM_LOAD( "136014.127",   0x0000, 0x2000, CRC(ee6b3315) SHA1(9cc26c6d3604c0f60d716f86e67e9d9c0487f87d) )    /* road control */
	ROM_LOAD( "136014.128",   0x2000, 0x2000, CRC(6d1e7042) SHA1(90113ff0c93ed86d95067290088705bb5e6608d1) )    /* road bits 1 */
	ROM_LOAD( "136014.134",   0x4000, 0x1000, CRC(4e97f101) SHA1(f377d053821c74aee93ebcd30a4d43e6156f3cfe) )    /* road bits 2 */

	ROM_REGION( 0x1000, REGION_GFX6, 0 ) 	/* sprite scaling */
	ROM_LOAD( "136014.231",   0x0000, 0x1000, CRC(a61bff15) SHA1(f7a59970831cdaaa7bf59c2221a38e4746c54244) )    /* vertical scaling */

	/* graphics (P)ROM data */
	ROM_REGION( 0x1040, REGION_PROMS, 0 )
	ROM_LOAD( "136014.186",   0x0000, 0x0100, CRC(16d69c31) SHA1(f24b345448e4f4ef4e2f3b057b81d399cf427f88) )    /* red palette */
	ROM_LOAD( "136014.187",   0x0100, 0x0100, CRC(07340311) SHA1(3820d1fa99013ed18de5d9400ad376cc446d1217) )    /* green palette */
	ROM_LOAD( "136014.188",   0x0200, 0x0100, CRC(1efc84d7) SHA1(6946e1c209eec0a4b75778ae88111e6cb63c63fb) )    /* blue palette */
	ROM_LOAD( "136014.189",   0x0300, 0x0100, CRC(064d51a0) SHA1(d5baa29930530a8930b44a374e285de849c2a6ce) )    /* alpha color */
	ROM_LOAD( "136014.190",   0x0400, 0x0100, CRC(7880c5af) SHA1(e4388e354420be3f99594a10c091e3d2f745cc04) )    /* background color */
	ROM_LOAD( "136014.142",   0x0500, 0x0100, CRC(2d502464) SHA1(682b7dd22e51d5db52c0804b7e27e47641dfa6bd) )    /* vertical position low */
	ROM_LOAD( "136014.143",   0x0600, 0x0100, CRC(027aa62c) SHA1(c7030d8b64b80e107c446f6fbdd63f560c0a91c0) )    /* vertical position med */
	ROM_LOAD( "136014.144",   0x0700, 0x0100, CRC(1f8d0df3) SHA1(b8f17758f114f5e247b65b3f2922ca2660757e66) )    /* vertical position hi */
	ROM_LOAD( "136014.191",   0x0800, 0x0400, CRC(8b270902) SHA1(27b3ebc92d3a2a5c0432bde018a0e43669041d50) )    /* road color */
	ROM_LOAD( "136014.192",   0x0c00, 0x0400, CRC(caddb0b0) SHA1(e41b89f2b40bf8f93546012f373ae63dcae870da) )    /* sprite color */
	ROM_LOAD( "136014.135",   0x1000, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */
	ROM_LOAD( "136014.136",   0x1020, 0x0020, CRC(4330a51b) SHA1(9531d18ce2de4eda9913d47ef8c5cd8f05791716) )    /* video RAM address decoder (not used) */

	/* sound (P)ROM data */
	ROM_REGION( 0x0100, REGION_SOUND1, 0 )
	ROM_LOAD( "136014.118",   0x0000, 0x0100, CRC(8568decc) SHA1(0aac1fa082858d4d201e21511c609a989f9a1535) )    /* Namco sound */

	ROM_REGION( 0x4000, REGION_SOUND2, 0 )
	ROM_LOAD( "136014.181",   0x0000, 0x2000, CRC(7d93bc1c) SHA1(dad7c0aa24aef593c84e21f7f8858ca7ada86364) )    /* engine sound */
	ROM_LOAD( "136014.182",   0x2000, 0x2000, CRC(7d93bc1c) SHA1(dad7c0aa24aef593c84e21f7f8858ca7ada86364) )    /* engine sound */

	ROM_REGION( 0x6000, REGION_SOUND3, 0 )
	ROM_LOAD( "136014.106",   0x0000, 0x2000, CRC(5b4cf05e) SHA1(52342572940489175607bbf5b6cfd05ee9b0f004) )    /* voice */

	/* unknown or unused (P)ROM data */
	ROM_REGION( 0x0100, REGION_USER1, 0 )
	ROM_LOAD( "136014.117",   0x0000, 0x0100, CRC(2401c817) SHA1(8991b7994513a469e64392fa8f233af5e5f06d54) )    /* sync chain */
ROM_END


/*********************************************************************
 * Initialization routines
 *********************************************************************/

static DRIVER_INIT( polepos2 )
{
	/* note that the bootleg version doesn't need this custom IC; it has a hacked ROM in its place */
	install_mem_read16_handler(1, 0x4000, 0x5fff, polepos2_ic25_r);
}


/*********************************************************************
 * Game drivers
 *********************************************************************/

GAME( 1982, polepos,  0,		polepos, polepos,  0,		 ROT0, "Namco", "Pole Position" )
GAME( 1982, poleposa, polepos,	polepos, poleposa, 0,		 ROT0, "Namco (Atari license)", "Pole Position (Atari version 2)" )
GAME( 1982, polepos1, polepos,	polepos, poleposa, 0,		 ROT0, "[Namco] (Atari license)", "Pole Position (Atari version 1)" )
GAME( 1982, topracer, polepos,	polepos, polepos,  0,		 ROT0, "bootleg", "Top Racer" )
GAME( 1983, polepos2, 0,		polepos, polepos2, polepos2, ROT0, "Namco", "Pole Position II" )
GAME( 1983, poleps2a, polepos2, polepos, polepos2, polepos2, ROT0, "Namco (Atari license)", "Pole Position II (Atari)" )
GAME( 1983, poleps2b, polepos2, polepos, polepos2, 0,		 ROT0, "bootleg", "Pole Position II (bootleg)" )
