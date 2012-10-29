/***************************************************************************

Xevious

driver by Mirko Buffoni


general map
-----------+---+-----------------+-------------------------
   hex     |r/w| D D D D D D D D |
 location  |   | 7 6 5 4 3 2 1 0 | function
-----------+---+-----------------+-------------------------
0000-3FFF  | R | D D D D D D D D | CPU 1 master rom (16k)
0000-1FFF  | R | D D D D D D D D | CPU 2 motion rom (8k)
0000-0FFF  | R | D D D D D D D D | CPU 3 sound  rom (4k)
-----------+---+-----------------+-------------------------
6800-680F  | W | - - - - D D D D | Audio control
6810-681F  | W | - - - - D D D D | Audio control
-----------+---+-----------------+-------------------------
6820       | W | - - - - - - - D | 0 = Reset IRQ1(latched)
6821       | W | - - - - - - - D | 0 = Reset IRQ2(latched)
6822       | W | - - - - - - - D | 0 = Reset NMI3(latched)
6823       | W | - - - - - - - D | 0 = Reset #2,#3 CPU
-----------+---+-----------------+-------------------------
6830       | W |                 | watchdog reset
-----------+---+-----------------+-------------------------
7000       |R/W| D D D D D D D D | custom 06 Data
7100       |R/W| D D D D D D D D | custom 06 Command
7???                               CPU #1 NMI REQUEST
-----------+---+-----------------+-------------------------
7800-7FFF  |R/W| D D D D D D D D | 2k work ram
-----------+---+-----------------+-------------------------
8000-87FF  |R/W| D D D D D D D D | 2k playfeild RAM
-----------+---+-----------------+-------------------------
8000-877F  |R/W| D D D D D D D D | RAM
8780-87FF  |R/W| D D D D D D D D | 2k sprite RAM (X-POS)
9000-977F  |R/W| D D D D D D D D | RAM
9780-97FF  |R/W| D D D D D D D D | 2k sprite RAM (HSIZE,MODE ??)
A000-A77F  |R/W| D D D D D D D D | RAM
A780-A7FF  |R/W| D D D D D D D D | 2k sprite RAM (PIC)
-----------+---+-----------------+-------------------------
B000-BFFF  |R/W| D D D D D D D D | 4k playfeild RAM ( ATTRIB)
C000-CFFF  |R/W| D D D D D D D D | 4k playfeild RAM ( PIC )
-----------+---+-----------------+-------------------------
D000-D00F  | W | D D D D D D D D | A0->D8:background Y scroll position
D010-D01F  | W | D D D D D D D D | A0->D8:font Y scroll position ??
D020-D02F  | W | D D D D D D D D | A0->D8:background X scroll position ??
D030-D03F  | W | D D D D D D D D | A0->D8:font X scroll position ?
D070-D07F  | W |               D | display flip mode ?
-----------+---+-----------------+-------------------------
F000       | W | D D D D D D D D | planet map position low ?
F001       | W | D D D D D D D D | planet map position high ?
F000       | R | D D D D D D D D | planet map data low ?
F001       | R | D D D D D D D D | planet map data high ?
-----------+---+-----------------+-------------------------
Xevious memory map (preliminary)

  Z80-1:MASTER CPU
    0000H-3FFFH R   P-ROM
    4000H-FFFFH R/W shared area
  Z80-2:MOTION CUP
    0000H-1FFFH R   P-ROM
    4000H-FFFFH R/W shared area
  Z80-3:SOUND CPU
    0000H-1FFFH R   P-ROM
    4000H-FFFFH R/W shared area
  shared area.
    6800H-6807H R   DIP SWITCH READ
                      A0-2 : bit select(6800H=bit0 , 6801=bit1,6807=bit7)
                      D0   : DIP SW.A bit read
                      D1   : DIP SW.B bit read
                    DIP SW.A
                      Bit0 = BLASTER SWITCH
    6800H-680FH R   shadow(6800H-6807H)
    6800H-681FH W   Sound RAM0 (same to DIGDUG ?)
                      D0-3:data
    6810H-681FH W   Sound RAM1
                      D0-3:data
    6820H       W   MASTER-INTERRUPT CLEAR & ENABLE
                      D0 = 0:CLEAR & DISABLE / 1:ENABLE
    6821H       W   MOTION-INTERRUPT CLEAR & ENABLE
                      D0 = 0:CLEAR & DISABLE / 1:ENABLE
    6822H       W   SOUND -NMI CONTROLL
                      D0 = 0:NMI ON / 1:NMI OFF
    6823H       W   CPU 2,3 RESET CONTROLL
                      D0 = 0:RESET
    6830H-683F  W   WDR Watch dock timer clear
    6840H-6FFFH R/W shadow(6800H-603FH)
    7000H       R   custom-io data read ( after CPU #1 NMI )
    7000H       W   custom-io data write( after CPU #1 NMI )
    7100H       R   custom-io timming port
                     bit7  :1=busy ?
                     bit6  :1=busy ?
                     bit5  :1=busy ?
    7100H       W   custom-io command write
    7???H       W   sound ganarator (controll by custom_ic ?)
    7800H-7FFFH R/W S-RAM
    8000H-87FFh R/W ram ( master cpu )
    8780H-87FFH R/W sprite X position ( 80-bf:used by master,c0-ff:used by motion)
                      A0=0:sprite Y position (OFF = 0xef )
                      A0=1:sprite X position
    8800H-8FFFH R/W shadow(8000H-87FFH)
    9000H-97FFH R/W ram ( motion cpu )
    9780H-97FFH R/W sprite attribute
                      A0=0:attribute Y
                        bit0  :HSIZE  :sprite Y size ?
                        bit1  :HUNDZ  :?
                        bit2  :FLOP   :Y flip
                        bit3  :HUKAIRS:?
                        bit4-6:MD4-6  :?
                        bit7  :BIT3   :select sprite set (0=4M,4P,4R 3bit:1=4N 2bit )
                      A0=0:attribute X
                        bit0  :HSIZE  :sprite X size ?
                        bit1  :HUNDZ  :?
                        bit2  :FLOP   :X flip
                        bit3  :HUKAIRS:?
                        bit4-6:MD4-6  :?
                        bit7  :BIT3   :?
    9800H-9FFFH R/W shadow(9000H-97FFH)
    A000H-A7FFH R/W ram ( sound cpu , master cpu )
    A780H-a7FFH R/W sprite character nnumber
                      A0=0 character pattern name
                      A0=1 color map select
    A800H-AFFFH R/W shadow(A000H-A7FFH)
    B000H-BFFFH R/W background attrivute ( have 2 scroll planes ? )
                    D0-D1:COL0,1 palette set ?
                    D1-D5:ANI0-3 color code  ?( font,bg use )
                    D6   :ANF    X flip
                    D7   :PFF    Y flip
    C000H-CFFFH R/W background character ( have 2 scroll planes ? )
    D000H-D07FH  W  CRTC access? (custom-13)
    D???H        W  display flip select (TABLE 1P/2P mode)
                      bit0  :FLIP
    F000H        W  BS0 xevious planet map select low ?
    F001H        W  BB1 xeviosu planet map select high ?
    F000H        R  BB0 xevious planet map get low ?
    F001H        R  BS1 xevious planet map get high ?
    F002-FFFFH      shodow(F000H-F001H)

3)schematic diagram block.

   Sheet 4A : main cpu , address decoder(4000h-7fffh) , sram
   Sheet 4B : motion cpu
   Sheet 5A : sound cpu
   Sheet 5B : clock generator , 'digdug' sound , irq controller
   Sheet 6A : joystick read , dip switch , audio(TONE?) , NMI to master cpu
   Sheet 6B : address decoder (8000h-ffffh) , sprite ram r/w , disp. flip latch
   Sheet 7A : sprite drawing engine
   Sheet 7B : background ram , CRTC ?,graphic drawing engene ?
   Sheet 8A : background(&font) drawing engene
   Sheet 8B : display line buffer ?
   Sheet 9A : video dac ( palette rom )
   Sheet 9B : xevious planet map rom ?

P-ROMS in schematic

1M  master cpu rom 0000H-1FFFH
1L  master cpu rom 2000H-3FFFH
4C  motion cpu rom 0000H-1FFFH
2C  sound  cpu rom 0000H-1FFFH
8M  sound pcm rom ?
6M  sound pcm decode rom ?
4M  spright pattern low (BIT3=0,000-127)
4P  spright pattern low (BIT3=0,128-255)
4R  spright pattern high(BIT3=0)
      D0-3:000-127
      D4-7:128-255
4N  spright pattern (BIT3=1)
3L  spright color map table
3M  spright color map table
3D  background pattern bit0
3C  background pattern bit1
3B  background font pattern
4H  background color map table
4F  background color map table
6E  palette rom blue
6D  palette rom green
6A  palette rom red
2A  xevious planet map table ??
2B  xevious planet map table ??
2C  xevious planet map table ??

S-RAMS in schematic

1H  sram           7800H-7FFFH
7L  sound sram0    6800H-680FH
7K  sound sram1    6810H-681FH
2S  spright line   8000H-87FFH
2A  spright att    9000H-97FFH
2P  spright chr    A000H-A7FFH
2J  background ram B000H-B7FFH
2H  background ram B800H-BFFFH
2F  background ram C000H-C7FFH
2E  background ram C800H-CFFFH
5N,5M sprite display line buffer ? (even disp,odd draw)
6N,6M sprite display line buffer ? (even draw,odd disp)

*****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


/* XEVIOUS */
extern unsigned char *xevious_sharedram;
READ_HANDLER( xevious_sharedram_r );
WRITE_HANDLER( xevious_sharedram_w );
READ_HANDLER( xevious_dsw_r );
WRITE_HANDLER( xevious_interrupt_enable_1_w );
WRITE_HANDLER( xevious_interrupt_enable_2_w );
WRITE_HANDLER( xevious_interrupt_enable_3_w );
WRITE_HANDLER( xevious_bs_w );
READ_HANDLER( xevious_bb_r );
READ_HANDLER( xevious_customio_r );
READ_HANDLER( xevious_customio_data_r );
WRITE_HANDLER( xevious_customio_w );
WRITE_HANDLER( xevious_customio_data_w );
WRITE_HANDLER( xevious_halt_w );
INTERRUPT_GEN( xevious_interrupt_1 );
INTERRUPT_GEN( xevious_interrupt_2 );
INTERRUPT_GEN( xevious_interrupt_3 );
MACHINE_INIT( xevious );

WRITE_HANDLER( xevious_vh_latch_w );

extern unsigned char *xevious_fg_videoram,*xevious_fg_colorram;
extern unsigned char *xevious_bg_videoram,*xevious_bg_colorram;
WRITE_HANDLER( xevious_fg_videoram_w );
WRITE_HANDLER( xevious_fg_colorram_w );
WRITE_HANDLER( xevious_bg_videoram_w );
WRITE_HANDLER( xevious_bg_colorram_w );
VIDEO_START( xevious );
PALETTE_INIT( xevious );
VIDEO_UPDATE( xevious );

WRITE_HANDLER( pengo_sound_w );
extern unsigned char *pengo_soundregs;


/* BATTLES */
extern unsigned char *battles_sharedram;
READ_HANDLER( battles_sharedram_r );
READ_HANDLER( battles_customio0_r );
READ_HANDLER( battles_customio_data0_r );
READ_HANDLER( battles_customio3_r );
READ_HANDLER( battles_customio_data3_r );
READ_HANDLER( battles_input_port_r );

WRITE_HANDLER( battles_halt_w );
WRITE_HANDLER( battles_sharedram_w );
WRITE_HANDLER( battles_customio0_w );
WRITE_HANDLER( battles_customio_data0_w );
WRITE_HANDLER( battles_customio3_w );
WRITE_HANDLER( battles_customio_data3_w );
WRITE_HANDLER( battles_CPU4_4000_w );
WRITE_HANDLER( battles_noise_sound_w );

INTERRUPT_GEN( battles_interrupt_4 );

MACHINE_INIT( battles );
PALETTE_INIT( battles );



static MEMORY_READ_START( readmem_cpu1 )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x6800, 0x6807, xevious_dsw_r },
	{ 0x7000, 0x700f, xevious_customio_data_r },
	{ 0x7100, 0x7100, xevious_customio_r },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
	{ 0xf000, 0xffff, xevious_bb_r },
MEMORY_END

static MEMORY_READ_START( readmem_cpu2 )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x6800, 0x6807, xevious_dsw_r },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
	{ 0xf000, 0xffff, xevious_bb_r },
MEMORY_END

static MEMORY_READ_START( readmem_cpu3 )
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
MEMORY_END

static MEMORY_WRITE_START( writemem_cpu1 )
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x6820, 0x6820, xevious_interrupt_enable_1_w },
	{ 0x6821, 0x6821, xevious_interrupt_enable_2_w },
	{ 0x6822, 0x6822, xevious_interrupt_enable_3_w },
	{ 0x6823, 0x6823, xevious_halt_w },			/* reset controll */
	{ 0x6830, 0x683f, MWA_NOP },				/* watch dock reset */
	{ 0x7000, 0x700f, xevious_customio_data_w },
	{ 0x7100, 0x7100, xevious_customio_w },
	{ 0x7800, 0xafff, xevious_sharedram_w, &xevious_sharedram },
	{ 0xb000, 0xb7ff, xevious_fg_colorram_w, &xevious_fg_colorram },
	{ 0xb800, 0xbfff, xevious_bg_colorram_w, &xevious_bg_colorram },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_w, &xevious_fg_videoram },
	{ 0xc800, 0xcfff, xevious_bg_videoram_w, &xevious_bg_videoram },
	{ 0xd000, 0xd07f, xevious_vh_latch_w }, /* ?? */
	{ 0xf000, 0xffff, xevious_bs_w },
	{ 0x8780, 0x87ff, MWA_RAM, &spriteram_2 },	/* here only */
	{ 0x9780, 0x97ff, MWA_RAM, &spriteram_3 },	/* to initialize */
	{ 0xa780, 0xa7ff, MWA_RAM, &spriteram, &spriteram_size },	/* the pointers */
MEMORY_END

static MEMORY_WRITE_START( writemem_cpu2 )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x6830, 0x683f, MWA_NOP },				/* watch dog reset */
	{ 0x7800, 0xafff, xevious_sharedram_w },
	{ 0xb000, 0xb7ff, xevious_fg_colorram_w },
	{ 0xb800, 0xbfff, xevious_bg_colorram_w },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_w },
	{ 0xc800, 0xcfff, xevious_bg_videoram_w },
	{ 0xd000, 0xd07f, xevious_vh_latch_w }, /* ?? */
	{ 0xf000, 0xffff, xevious_bs_w },
MEMORY_END

static MEMORY_WRITE_START( writemem_cpu3 )
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x6800, 0x681f, pengo_sound_w, &pengo_soundregs },
	{ 0x6822, 0x6822, xevious_interrupt_enable_3_w },
	{ 0x7800, 0xcfff, xevious_sharedram_w },
MEMORY_END



static MEMORY_READ_START( battles_readmem_cpu1 )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x6800, 0x6807, xevious_dsw_r },
	{ 0x7000, 0x700f, battles_customio_data0_r },
	{ 0x7100, 0x7100, battles_customio0_r },
	{ 0x7800, 0xcfff, xevious_sharedram_r },
	{ 0xf000, 0xffff, xevious_bb_r },
MEMORY_END

static MEMORY_READ_START( battles_readmem_cpu4 )
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x4000, 0x4000, input_port_2_r },	/* IN2 */
	{ 0x4001, 0x4001, input_port_3_r },	/* IN3 */
	{ 0x4002, 0x4002, input_port_4_r },	/* IN4 */
	{ 0x4003, 0x4003, input_port_5_r },	/* IN5 */
	{ 0x4004, 0x400f, battles_input_port_r },
	{ 0x6000, 0x6000, battles_customio3_r },
	{ 0x7000, 0x700f, battles_customio_data3_r },
	{ 0x8000, 0x80ff, battles_sharedram_r },
MEMORY_END

static MEMORY_WRITE_START( battles_writemem_cpu1 )
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x6820, 0x6820, xevious_interrupt_enable_1_w },
	{ 0x6821, 0x6821, xevious_interrupt_enable_2_w },
	{ 0x6822, 0x6822, xevious_interrupt_enable_3_w },
	{ 0x6823, 0x6823, xevious_halt_w },			/* reset controll */
	{ 0x6830, 0x683f, MWA_NOP },				/* watch dock reset */
	{ 0x7000, 0x700f, battles_customio_data0_w },
	{ 0x7100, 0x7100, battles_customio0_w },
	{ 0x7800, 0xafff, xevious_sharedram_w, &xevious_sharedram },
	{ 0xb000, 0xb7ff, xevious_fg_colorram_w, &xevious_fg_colorram },
	{ 0xb800, 0xbfff, xevious_bg_colorram_w, &xevious_bg_colorram },
	{ 0xc000, 0xc7ff, xevious_fg_videoram_w, &xevious_fg_videoram },
	{ 0xc800, 0xcfff, xevious_bg_videoram_w, &xevious_bg_videoram },
	{ 0xd000, 0xd07f, xevious_vh_latch_w }, /* ?? */
	{ 0xf000, 0xffff, xevious_bs_w },
	{ 0x8780, 0x87ff, MWA_RAM, &spriteram_2 },	/* here only */
	{ 0x9780, 0x97ff, MWA_RAM, &spriteram_3 },	/* to initialize */
	{ 0xa780, 0xa7ff, MWA_RAM, &spriteram, &spriteram_size },	/* the pointers */
MEMORY_END

static MEMORY_WRITE_START( battles_writemem_cpu4 )
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x4000, 0x4005, battles_CPU4_4000_w }, /* ??? */
	{ 0x5000, 0x5000, battles_noise_sound_w },
	{ 0x6000, 0x6000, battles_customio3_w },
	{ 0x7000, 0x700f, battles_customio_data3_w },
	{ 0x8000, 0x80ff, battles_sharedram_w, &battles_sharedram },
MEMORY_END




INPUT_PORTS_START( xevious )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, "Freeze?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
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
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/* same as xevious but different "Coin B" Dip Switch and "Copyright" Dip Switch instead of "Freeze?" */
INPUT_PORTS_START( xeviousa )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	/* when switch is on Namco, high score names are 10 letters long */
	PORT_DIPNAME( 0x80, 0x80, "Copyright" )
	PORT_DIPSETTING(    0x00, "Namco" )
	PORT_DIPSETTING(    0x80, "Atari/Namco" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
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
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/* same as xevious but "Copyright" Dip Switch instead of "Freeze?" */
INPUT_PORTS_START( xeviousb )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	/* when switch is on Namco, high score names are 10 letters long */
	PORT_DIPNAME( 0x80, 0x80, "Copyright" )
	PORT_DIPSETTING(    0x00, "Namco" )
	PORT_DIPSETTING(    0x80, "Atari/Namco" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
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
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( battles )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, "Freeze?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )


	PORT_START	/* 4000 IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* 4001 IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* 4002 IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL)
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )


	PORT_START	/* 4003 IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )

	/*	PORT_SERVICE( 0x80, IP_ACTIVE_LOW ) */
INPUT_PORTS_END

/* same as xevious but different "Coin B" Dip Switch and inverted "Freeze?" Dip Switch */
INPUT_PORTS_START( sxevious )
	PORT_START	/* DSW0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x02, 0x02, "Flags Award Bonus Life" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, "Freeze?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	/* TODO: bonus scores are different for 5 lives */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 40K 40K" )
	PORT_DIPSETTING(    0x14, "10K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 50K 50K" )
	PORT_DIPSETTING(    0x1c, "20K 60K 60K" )
	PORT_DIPSETTING(    0x0c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x08, "20K 80K 80K" )
	PORT_DIPSETTING(    0x04, "20K 60K" )
	PORT_DIPSETTING(    0x00, "None" )
	/* Bonus scores for 5 lives
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "10K 50K 50K" )
	PORT_DIPSETTING(    0x14, "20K 50K 50K" )
	PORT_DIPSETTING(    0x10, "20K 60K 60K" )
	PORT_DIPSETTING(    0x1c, "20K 70K 70K" )
	PORT_DIPSETTING(    0x0c, "20K 80K 80K" )
	PORT_DIPSETTING(    0x08, "30K 100K 100K" )
	PORT_DIPSETTING(    0x04, "20K 80K" )
	PORT_DIPSETTING(    0x00, "None" )
	*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x40, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x60, "3" )
	PORT_DIPSETTING(    0x00, "5" )
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
	PORT_BIT( 0x03, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_START1, 1 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_START2, 1 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1, 1 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



/* foreground characters */
static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	1,	/* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
/* background tiles */
static struct GfxLayout bgcharlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 512*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
/* sprite set #1 */
static struct GfxLayout spritelayout1 =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 128*64*8+4, 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 128 consecutive bytes */
};
/* sprite set #2 */
static struct GfxLayout spritelayout2 =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 0, 128*64*8, 128*64*8+4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 128 consecutive bytes */
};
/* sprite set #3 */
static struct GfxLayout spritelayout3 =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	3,	/* 3 bits per pixel (one is always 0) */
	{ 64*64*8, 0, 4 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout, 128*4+64*8,  64 },
	{ REGION_GFX2, 0x0000, &bgcharlayout,        0, 128 },
	{ REGION_GFX3, 0x0000, &spritelayout1,   128*4,  64 },
	{ REGION_GFX3, 0x2000, &spritelayout2,   128*4,  64 },
	{ REGION_GFX3, 0x6000, &spritelayout3,   128*4,  64 },
	{ -1 } /* end of array */
};

/*static struct GfxDecodeInfo battles_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,      128*4 + 128*8, 128 },
	{ REGION_GFX2, 0x0000, &bgcharlayout,                0, 128 },
	{ REGION_GFX3, 0x0000, &spritelayout1,   128*4        , 128 },
	{ REGION_GFX3, 0x2000, &spritelayout2,   128*4        , 128 },
	{ REGION_GFX3, 0x6000, &spritelayout3,   128*4        , 128 },
	{ -1 }
};
*/


static struct namco_interface namco_interface =
{
	3072000/32,	/* sample rate */
	3,			/* number of voices */
	100,		/* playback volume */
	REGION_SOUND1	/* memory region */
};

static const char *xevious_sample_names[] =
{
	"*xevious",
	"explo1.wav",	/* ground target explosion */
	"explo2.wav",	/* Solvalou explosion */
	"explo3.wav",	/* credit */
	"explo4.wav",	/* Garu Zakato explosion */
	0	/* end of array */
};

static const char *xevios_sample_names[] =
{
	"*xevios",
	"explo1.wav",	/* explosion */
	"explo1.wav",	/* explosion */
	"explo1.wav",	/* explosion */
	"explo1.wav",	/* explosion */
	0	/* end of array */
};

struct Samplesinterface samples_interface =
{
	1,	/* one channel */
	80,	/* volume */
	xevious_sample_names
};

struct Samplesinterface xevios_samples_interface =
{
	1,	/* one channel */
	80,	/* volume */
	xevios_sample_names
};


static const char *battles_sample_names[] =
{
	"*battles",
	"explo1.wav",	/* ground target explosion */
	"explo2.wav",	/* Solvalou explosion */
	0	/* end of array */
};

struct Samplesinterface battles_samples_interface =
{
	1,	/* one channel */
	80,	/* volume */
	battles_sample_names
};



static MACHINE_DRIVER_START( xevious )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)	/* 3.125 MHz (?) */
	MDRV_CPU_MEMORY(readmem_cpu1,writemem_cpu1)
	MDRV_CPU_VBLANK_INT(xevious_interrupt_1,1)

	MDRV_CPU_ADD(Z80, 3072000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu2,writemem_cpu2)
	MDRV_CPU_VBLANK_INT(xevious_interrupt_2,1)

	MDRV_CPU_ADD(Z80, 3072000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu3,writemem_cpu3)
	MDRV_CPU_PERIODIC_INT(xevious_interrupt_3,16000.0/128)

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(xevious)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(128+1)
	MDRV_COLORTABLE_LENGTH(128*4+64*8+64*2)

	MDRV_PALETTE_INIT(xevious)
	MDRV_VIDEO_START(xevious)
	MDRV_VIDEO_UPDATE(xevious)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO, namco_interface)
	MDRV_SOUND_ADD(SAMPLES, samples_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( xevios )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)	/* 3.125 MHz (?) */
	MDRV_CPU_MEMORY(readmem_cpu1,writemem_cpu1)
	MDRV_CPU_VBLANK_INT(xevious_interrupt_1,1)

	MDRV_CPU_ADD(Z80, 3072000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu2,writemem_cpu2)
	MDRV_CPU_VBLANK_INT(xevious_interrupt_2,1)

	MDRV_CPU_ADD(Z80, 3072000)	/* 3.125 MHz */
	MDRV_CPU_MEMORY(readmem_cpu3,writemem_cpu3)
	MDRV_CPU_PERIODIC_INT(xevious_interrupt_3,16000.0/128)

/*	MDRV_CPU_ADD(Z80, 3072000)										*/
/*	MDRV_CPU_MEMORY(xevios_readmem_cpu4,xevios_writemem_cpu4)		*/
/*	MDRV_CPU_VBLANK_INT(battles_interrupt_4,1)						*/

	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(xevious)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(128+1)
	MDRV_COLORTABLE_LENGTH(128*4+64*8+64*2)

	MDRV_PALETTE_INIT(xevious)
	MDRV_VIDEO_START(xevious)
	MDRV_VIDEO_UPDATE(xevious)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO, namco_interface)
	MDRV_SOUND_ADD(SAMPLES, xevios_samples_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( battles )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(battles_readmem_cpu1,battles_writemem_cpu1)
	MDRV_CPU_VBLANK_INT(xevious_interrupt_1,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem_cpu2,writemem_cpu2)
	MDRV_CPU_VBLANK_INT(xevious_interrupt_2,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(readmem_cpu3,writemem_cpu3)
	MDRV_CPU_PERIODIC_INT(xevious_interrupt_3,16000.0/128)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_MEMORY(battles_readmem_cpu4,battles_writemem_cpu4)
	MDRV_CPU_VBLANK_INT(battles_interrupt_4,1)


	MDRV_FRAMES_PER_SECOND(60.606060)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)	/* 100 CPU slices per frame - an high value to ensure proper */
							/* synchronization of the CPUs */
	MDRV_MACHINE_INIT(battles)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(36*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 36*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(128+1)
	MDRV_COLORTABLE_LENGTH(128*4+64*8+64*2)

	MDRV_PALETTE_INIT(battles)
	MDRV_VIDEO_START(xevious)
	MDRV_VIDEO_UPDATE(xevious)

	/* sound hardware */
	MDRV_SOUND_ADD(NAMCO, namco_interface)
	MDRV_SOUND_ADD(SAMPLES, battles_samples_interface)
MACHINE_DRIVER_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( xevious )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "xvi_1.3p",     0x0000, 0x1000, CRC(09964dda) SHA1(4882b25b0938a903f3a367455ba788a30759b5b0) )
	ROM_LOAD( "xvi_2.3m",     0x1000, 0x1000, CRC(60ecce84) SHA1(8adc60a5fcbca74092518dbc570ffff0f04c5b17) )
	ROM_LOAD( "xvi_3.2m",     0x2000, 0x1000, CRC(79754b7d) SHA1(c6a154858716e1f073b476824b183de20e06d093) )
	ROM_LOAD( "xvi_4.2l",     0x3000, 0x1000, CRC(c7d4bbf0) SHA1(4b846de204d08651253d3a141677c8a31626af07) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xvi_5.3f",     0x0000, 0x1000, CRC(c85b703f) SHA1(15f1c005b9d806a384ab1f2240b9c580bfe83893) )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, CRC(e18cdaad) SHA1(6b79efee1a9642edb9f752101737132401248aed) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xeviousa )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "xea-1m-a.bin", 0x0000, 0x2000, CRC(8c2b50ec) SHA1(f770873b711d838556dde67a8aac8a7f572fcc5b) )
	ROM_LOAD( "xea-1l-a.bin", 0x2000, 0x2000, CRC(0821642b) SHA1(c6c322c61d0985a2ac59f5e92d4e351107afb9eb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xea-4c-a.bin", 0x0000, 0x2000, CRC(14d8fa03) SHA1(e8114141394adda86184b146f2497cfeef7fc2eb) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xeviousb )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "1m.bin",     0x0000, 0x2000, CRC(e82a22f6) SHA1(6fd09a7fb263cda3d5268cc6d7bfe71a57ac4b47) )
	ROM_LOAD( "1l.bin",     0x2000, 0x2000, CRC(13831df9) SHA1(a7892d1d98868a83a5d1092976873b82577e9e94) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "4c.bin",     0x0000, 0x2000, CRC(827e7747) SHA1(d22645d71b164613834336e26e6942506a0e7eaa) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( xevios )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "4.7h",         0x0000, 0x1000, CRC(1f8ca4c0) SHA1(9fdaa2e0016c07e274544f8334778fe81b8344a5) )
	ROM_LOAD( "5.6h",         0x1000, 0x1000, CRC(2e47ce8f) SHA1(fb35dd086e98279a5f17036f624ef5294c777d84) )
	ROM_LOAD( "xvi_3.2m",     0x2000, 0x1000, CRC(79754b7d) SHA1(c6a154858716e1f073b476824b183de20e06d093) )
	ROM_LOAD( "w7.4h",        0x3000, 0x1000, CRC(17f48277) SHA1(ffe590acf07985355ef91fbe0fc3dcf6e8fd62fd) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "xvi_5.3f",     0x0000, 0x1000, CRC(c85b703f) SHA1(15f1c005b9d806a384ab1f2240b9c580bfe83893) )
	ROM_LOAD( "xvi_6.3j",     0x1000, 0x1000, CRC(e18cdaad) SHA1(6b79efee1a9642edb9f752101737132401248aed) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "16.8d",        0x2000, 0x2000, CRC(44262c04) SHA1(4291f83193d11064c2ba6a9af27951b93bb945c3) )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "10.1d",        0x0000, 0x1000, CRC(10baeebb) SHA1(c544c9e0bb7a1ef93b3f2c2c1397f659d5334373) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "12.3d",        0x3000, 0x1000, CRC(51a4e83b) SHA1(fbf3b1e47b75c5e0b297ee2cd6597b1dfd80bc6f) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */

	ROM_REGION( 0x3000, REGION_USER1, 0 )
	/* extra ROMs (function unknown, could be emulation of the custom I/O */
	/* chip with a Z80): */
	ROM_LOAD( "1.16j",        0x0000, 0x1000, CRC(2618f0ce) SHA1(54e8644b5609d6f6ec717a7469c76901eb79f26e) )
	ROM_LOAD( "2.17b",        0x1000, 0x2000, CRC(de359fac) SHA1(a55df9984bfffafeadae8a5a63b07f1fa9c5eebf) )
ROM_END

ROM_START( battles )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "b_1.bin",      0x0000, 0x2000, CRC(b6e4f4f3) SHA1(ceaaa63b50e75dcb05aeb68574336dfe56a8434a) )
	ROM_LOAD( "b_2.bin",      0x2000, 0x2000, CRC(47017bc8) SHA1(0da73ae079fb6a64eed56197e2c88609ef34166c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "b_3.bin",      0x0000, 0x2000, CRC(0ede5706) SHA1(65b235c5abe487612e11d0235410f1ca59b06e95) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x10000, REGION_CPU4, 0 )	/* 64k for the CUSTOM I/O Emulation CPU */
	ROM_LOAD( "b_5.bin",      0x0000, 0x1000, CRC(23107dfb) SHA1(74c49a5648faab632ae5ed8dd18a1d8b39837e2d) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "b_9.bin",      0x0000, 0x1000, CRC(5bd6e9ae) SHA1(f16c7eec39fce856c775b2b81ab55fb42376850e) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "b_10.bin",     0x0000, 0x1000, CRC(b43ea55d) SHA1(06f4c4e7fc71b9e173c3bdf91c40f47750051b5e) )	/* bg pattern B0 */
	ROM_LOAD( "b_11.bin",     0x1000, 0x1000, CRC(73603931) SHA1(1f7824b107a5a3d5c3434f02f17173a1f85fd29c) )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x1400, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "b_-bpr.bin",   0x0300, 0x0400, CRC(d2d208b1) SHA1(6c8d29912c03ee93759e24085bc66ab738768bcc) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "b_6bpr.bin",   0x0700, 0x0400, CRC(0260c041) SHA1(1a7516e8b18ffdd9789eec8b834c17b3ba312afe) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "b_4bpr.bin",   0x0b00, 0x0400, CRC(33764974) SHA1(567b048b8a93e30090ccee4f6aadc0353524d8d1) ) /* sprite lookup table low bits */
	ROM_LOAD( "b_5bpr.bin",   0x0f00, 0x0400, CRC(43674c7e) SHA1(94c19a9da81839cb1dfde3f11b2fd82ffe45efb9) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END

ROM_START( sxevious )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* 64k for the first CPU */
	ROM_LOAD( "cpu_3p.rom",   0x0000, 0x1000, CRC(1c8d27d5) SHA1(2c41303d8c74acb5840295a4b460a39a9a8e21bb) )
	ROM_LOAD( "cpu_3m.rom",   0x1000, 0x1000, CRC(fd04e615) SHA1(7169e7f3bd1e9cfae9671b89f2a45f56b968e1ff) )
	ROM_LOAD( "cpu_2m.rom",   0x2000, 0x1000, CRC(294d5404) SHA1(ecc39fb2c0065a36f20541747089b4e30dfb99b1) )
	ROM_LOAD( "cpu_2l.rom",   0x3000, 0x1000, CRC(6a44bf92) SHA1(0ca726f7f9528789f2a718df55e59406a283cdfa) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64k for the second CPU */
	ROM_LOAD( "cpu_3f.rom",   0x0000, 0x1000, CRC(d4bd3d81) SHA1(5831bb306bd650779207936bfd00f25864733abb) )
	ROM_LOAD( "cpu_3j.rom",   0x1000, 0x1000, CRC(af06be5f) SHA1(5a020822387ab8c69214db961180760fa9853e6e) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64k for the audio CPU */
	ROM_LOAD( "xvi_7.2c",     0x0000, 0x1000, CRC(dd35cf1c) SHA1(f8d1f8e019d8198308443c2e7e815d0d04b23d14) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_12.3b",    0x0000, 0x1000, CRC(088c8b26) SHA1(9c3b61dfca2f84673a78f7f66e363777a8f47a59) )	/* foreground characters */

	ROM_REGION( 0x2000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_13.3c",    0x0000, 0x1000, CRC(de60ba25) SHA1(32bc09be5ff8b52ee3a26e0ac3ebc2d4107badb7) )	/* bg pattern B0 */
	ROM_LOAD( "xvi_14.3d",    0x1000, 0x1000, CRC(535cdbbc) SHA1(fb9ffe5fc43e0213231267e98d605d43c15f61e8) )	/* bg pattern B1 */

	ROM_REGION( 0x8000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "xvi_15.4m",    0x0000, 0x2000, CRC(dc2c0ecb) SHA1(19ddbd9805f77f38c9a9a1bb30dba6c720b8609f) )	/* sprite set #1, planes 0/1 */
	ROM_LOAD( "xvi_18.4r",    0x2000, 0x2000, CRC(02417d19) SHA1(b5f830dd2cf25cf154308d2e640f0ecdcda5d8cd) )	/* sprite set #1, plane 2, set #2, plane 0 */
	ROM_LOAD( "xvi_17.4p",    0x4000, 0x2000, CRC(dfb587ce) SHA1(acff2bf5cde85a16cdc98a52cdea11f77fadf25a) )	/* sprite set #2, planes 1/2 */
	ROM_LOAD( "xvi_16.4n",    0x6000, 0x1000, CRC(605ca889) SHA1(3bf380ef76c03822a042ecc73b5edd4543c268ce) )	/* sprite set #3, planes 0/1 */
	/* 0xa000-0xafff empty space to decode sprite set #3 as 3 bits per pixel */

	ROM_REGION( 0x4000, REGION_GFX4, 0 )	/* background tilemaps */
	ROM_LOAD( "xvi_9.2a",     0x0000, 0x1000, CRC(57ed9879) SHA1(3106d1aacff06cf78371bd19967141072b32b7d7) )
	ROM_LOAD( "xvi_10.2b",    0x1000, 0x2000, CRC(ae3ba9e5) SHA1(49064b25667ffcd81137cd5e800df4b78b182a46) )
	ROM_LOAD( "xvi_11.2c",    0x3000, 0x1000, CRC(31e244dd) SHA1(3f7eac12863697a98e1122111801606759e44b2a) )

	ROM_REGION( 0x0b00, REGION_PROMS, 0 )
	ROM_LOAD( "xvi_8bpr.6a",  0x0000, 0x0100, CRC(5cc2727f) SHA1(0dc1e63a47a4cb0ba75f6f1e0c15e408bb0ee2a1) ) /* palette red component */
	ROM_LOAD( "xvi_9bpr.6d",  0x0100, 0x0100, CRC(5c8796cc) SHA1(63015e3c0874afc6b1ca032f1ffb8f90562c77c8) ) /* palette green component */
	ROM_LOAD( "xvi10bpr.6e",  0x0200, 0x0100, CRC(3cb60975) SHA1(c94d5a5dd4d8a08d6d39c051a4a722581b903f45) ) /* palette blue component */
	ROM_LOAD( "xvi_7bpr.4h",  0x0300, 0x0200, CRC(22d98032) SHA1(ec6626828c79350417d08b98e9631ad35edd4a41) ) /* bg tiles lookup table low bits */
	ROM_LOAD( "xvi_6bpr.4f",  0x0500, 0x0200, CRC(3a7599f0) SHA1(a4bdf58c190ca16fc7b976c97f41087a61fdb8b8) ) /* bg tiles lookup table high bits */
	ROM_LOAD( "xvi_4bpr.3l",  0x0700, 0x0200, CRC(fd8b9d91) SHA1(87ddf0b9d723aabb422d6d416aa9ec6bc246bf34) ) /* sprite lookup table low bits */
	ROM_LOAD( "xvi_5bpr.3m",  0x0900, 0x0200, CRC(bf906d82) SHA1(776168a73d3b9f0ce05610acc8a623deae0a572b) ) /* sprite lookup table high bits */

	ROM_REGION( 0x0200, REGION_SOUND1, 0 )	/* sound PROMs */
	ROM_LOAD( "xvi_2bpr.7n",  0x0000, 0x0100, CRC(550f06bc) SHA1(816a0fafa0b084ac11ae1af70a5186539376fc2a) )
	ROM_LOAD( "xvi_1bpr.5n",  0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )	/* timing - not used */
ROM_END



static DRIVER_INIT( xevios )
{
	int A;


	/* convert one of the sprite ROMs to the format used by Xevious */
	for (A = 0x2000;A < 0x4000;A++)
	{
		UINT8 *rom = memory_region(REGION_GFX3);

		rom[A] = BITSWAP8(rom[A],1,3,5,7,0,2,4,6);
	}

	/* convert one of tile map ROMs to the format used by Xevious */
	for (A = 0x0000;A < 0x1000;A++)
	{
		UINT8 *rom = memory_region(REGION_GFX4);

		rom[A] = BITSWAP8(rom[A],3,7,5,1,2,6,4,0);
	}
}



GAME( 1982, xevious,  0,       xevious, xevious,  0,      ROT90, "Namco", "Xevious (Namco)" )
GAME( 1982, xeviousa, xevious, xevious, xeviousa, 0,      ROT90, "Namco (Atari license)", "Xevious (Atari set 1)" )
GAME( 1982, xeviousb, xevious, xevious, xeviousb, 0,      ROT90, "Namco (Atari license)", "Xevious (Atari set 2)" )
GAME( 1982, xevios,   xevious, xevios,  xevious,  xevios, ROT90, "bootleg", "Xevios" )
GAME( 1982, battles,  xevious, battles, battles,  0,      ROT90, "bootleg", "Battles" )
GAME( 1984, sxevious, xevious, xevious, sxevious, 0,      ROT90, "Namco", "Super Xevious" )

