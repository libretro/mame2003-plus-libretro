/***************************************************************************

	Tatsumi TX-1/Buggy Boy hardware

	driver by Phil Bennett, from current MAME (src/mame/tatsumi/tx1.cpp)

	Games supported:
		* TX-1 (1983) [3 sets]
		* Buggy Boy (1985) [2 sets]
		* Buggy Boy Junior (1986)

	Notes:
		* TX-1 tire screech noises are not implemented yet.
		* The cockpit sets (TX-1, Buggy Boy) have three screens: they are
		  shown side by side, or only the middle one with the core option
		  "TX-1 / Buggy Boy Screens".

	The two 8086 CPUs have a 16-bit data bus, and the arithmetic unit and
	the video registers see one access for an aligned word: the i86 core
	accesses memory a byte at a time, so the driver turns on its 16-bit bus
	(i86_bus16) and joins the two byte accesses of a word (BUS16_READ and
	BUS16_WRITE below).

****************************************************************************

	Buggy Boy Error Codes          TX-1 Error Codes
	=====================          ================

	1  Main CPU RAM                1  Main microprocessor RAM
	2  Video (character) RAM       2  Video RAM
	3  Road/common RAM             3  Common RAM
	4  Sound RAM                   4  Sound RAM
	5  Main CPU ROM                5  Main microprocessor ROM
	6  Sound ROM                   6  Sound ROM
	8  Auxillary ROM               10 Interface ROM (time-out error)
	12 Arithmetic unit             11 Common RAM (access for arithmetic CPU)
	22 Main 8086-Z80 time-out      12 Common RAM (access for arithmetic CPU)
	                               13 Arithmetic RAM
	                               14 Common RAM (access for arithmetic CPU)
	                               15 Object RAM
	                               16 Arithmetic ROM
	                               17 Data ROM (checksum)
	                               18 Arithmetic unit

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "cpu/i86/i86intf.h"
#include "machine/8255ppi.h"
#include "includes/tx1.h"


/*************************************
 *
 *  The 16-bit data bus of the 8086s
 *
 *************************************/

/* the RAMs shared with the video hardware, in host order words */
static data16_t tx1_vram_words[0x2000 / 2];
static data16_t tx1_rcram_words[0x1000 / 2];
static data16_t tx1_math_ram_words[0x800 / 2];
static data16_t tx1_objram_words[0x300 / 2];

/* the high byte of a word read, the low byte of a word write */
static data16_t bus16_rlatch;
static data8_t bus16_wlatch;

/* a 16-bit read handler on the byte bus: an aligned word is one read */
#define BUS16_READ(name, func)												\
static READ_HANDLER( name )													\
{																			\
	if (offset & 1)															\
	{																		\
		if (i86_bus16_word)													\
			return bus16_rlatch >> 8;										\
		return func(offset >> 1, 0x00ff) >> 8;								\
	}																		\
	if (i86_bus16_word)														\
	{																		\
		bus16_rlatch = func(offset >> 1, 0);								\
		return bus16_rlatch & 0xff;											\
	}																		\
	return func(offset >> 1, 0xff00) & 0xff;								\
}

/* a 16-bit write handler on the byte bus: an aligned word is one write */
#define BUS16_WRITE(name, func)												\
static WRITE_HANDLER( name )												\
{																			\
	if (offset & 1)															\
	{																		\
		if (i86_bus16_word)													\
			func(offset >> 1, bus16_wlatch | (data << 8), 0);				\
		else																\
			func(offset >> 1, data << 8, 0x00ff);							\
	}																		\
	else if (i86_bus16_word)												\
		bus16_wlatch = data;												\
	else																	\
		func(offset >> 1, data, 0xff00);									\
}

/* a RAM of host order words on the byte bus */
#define WORD_RAM(rname, wname, ram)											\
static READ_HANDLER( rname )												\
{																			\
	data16_t w = ram[offset >> 1];											\
	return (offset & 1) ? (w >> 8) : (w & 0xff);							\
}																			\
static WRITE_HANDLER( wname )												\
{																			\
	if (offset & 1)															\
		ram[offset >> 1] = (ram[offset >> 1] & 0x00ff) | (data << 8);		\
	else																	\
		ram[offset >> 1] = (ram[offset >> 1] & 0xff00) | data;				\
}

WORD_RAM( vram_r,     vram_w,     tx1_vram_words )
WORD_RAM( rcram_r,    rcram_w,    tx1_rcram_words )
WORD_RAM( math_ram_r, math_ram_w, tx1_math_ram_words )
WORD_RAM( objram_r,   objram_w,   tx1_objram_words )

BUS16_READ ( crtc_r8,           tx1_crtc_r )
BUS16_WRITE( crtc_w8,           tx1_crtc_w )
BUS16_READ ( dipswitches_r8,    tx1_dipswitches_r )
BUS16_WRITE( z80_busreq_w8,     tx1_z80_busreq_w )
BUS16_READ ( z80_shared_r8,     tx1_z80_shared_r )
BUS16_WRITE( z80_shared_w8,     tx1_z80_shared_w )
BUS16_WRITE( tx1_scolst_w8,     tx1_scolst_w )
BUS16_WRITE( slincs_w8,         tx1_slincs_w )
BUS16_WRITE( slock_w8,          tx1_slock_w )
BUS16_WRITE( bb_scolst_w8,      buggyboy_scolst_w )
BUS16_WRITE( bb_sky_w8,         buggyboy_sky_w )

BUS16_READ ( tx1_spcs_ram_r8,   tx1_spcs_ram_r )
BUS16_WRITE( tx1_spcs_ram_w8,   tx1_spcs_ram_w )
BUS16_WRITE( bankcs_w8,         tx1_bankcs_w )
BUS16_WRITE( flgcs_w8,          tx1_flgcs_w )
BUS16_READ ( tx1_math_r8,       tx1_math_r )
BUS16_WRITE( tx1_math_w8,       tx1_math_w )
BUS16_READ ( tx1_spcs_rom_r8,   tx1_spcs_rom_r )

BUS16_READ ( bb_spcs_ram_r8,    buggyboy_spcs_ram_r )
BUS16_WRITE( bb_spcs_ram_w8,    buggyboy_spcs_ram_w )
BUS16_WRITE( gas_w8,            buggyboy_gas_w )
BUS16_READ ( bb_math_r8,        buggyboy_math_r )
BUS16_WRITE( bb_math_w8,        buggyboy_math_w )
BUS16_READ ( bb_spcs_rom_r8,    buggyboy_spcs_rom_r )


/*************************************
 *
 *  CPU Control
 *
 *************************************/

static WRITE_HANDLER( resume_math_w )
{
	cpu_set_irq_line(1, TX1_TEST_LINE, ASSERT_LINE);
}

static WRITE_HANDLER( halt_math_w )
{
	cpu_set_irq_line(1, TX1_TEST_LINE, CLEAR_LINE);
}

/* the RAM mirrors of TX-1 */
static READ_HANDLER( tx1_ram0_mirror_r )   { return memory_region(REGION_CPU1)[0x0000 + offset]; }
static WRITE_HANDLER( tx1_ram0_mirror_w )  { memory_region(REGION_CPU1)[0x0000 + offset] = data; }
static READ_HANDLER( tx1_ram1_mirror_r )   { return memory_region(REGION_CPU1)[0x2000 + offset]; }
static WRITE_HANDLER( tx1_ram1_mirror_w )  { memory_region(REGION_CPU1)[0x2000 + offset] = data; }
static READ_HANDLER( tx1_nvram_mirror_r )  { return memory_region(REGION_CPU1)[0x4000 + offset]; }
static WRITE_HANDLER( tx1_nvram_mirror_w ) { memory_region(REGION_CPU1)[0x4000 + offset] = data; }

/* the Z80 RAM mirror of TX-1 */
static READ_HANDLER( tx1_z80_ram_mirror_r )  { return tx1_z80_ram[offset]; }
static WRITE_HANDLER( tx1_z80_ram_mirror_w ) { tx1_z80_ram[offset] = data; }


/*************************************
 *
 *  TX-1 Memory Maps
 *
 *************************************/

static MEMORY_READ_START( tx1_main_readmem )
	{ 0x00000, 0x00fff, MRA_RAM },
	{ 0x01000, 0x01fff, tx1_ram0_mirror_r },
	{ 0x02000, 0x02fff, MRA_RAM },
	{ 0x03000, 0x03fff, tx1_ram1_mirror_r },
	{ 0x04000, 0x04fff, MRA_RAM },
	{ 0x05000, 0x05fff, tx1_nvram_mirror_r },
	{ 0x06000, 0x06fff, crtc_r8 },
	{ 0x08000, 0x09fff, vram_r },
	{ 0x0a000, 0x0afff, rcram_r },
	{ 0x0b000, 0x0b001, dipswitches_r8 },
	{ 0x0f000, 0x0f001, MRA_NOP },		/* watchdog */
	{ 0x10000, 0x1ffff, z80_shared_r8 },
	/* the ROM and its mirrors (0x20000-0x2ffff, mirror 0xd0000) */
	{ 0x20000, 0x3ffff, MRA_ROM },
	{ 0x60000, 0x7ffff, MRA_ROM },
	{ 0xa0000, 0xbffff, MRA_ROM },
	{ 0xe0000, 0xfffff, MRA_ROM },
	{ 0x00000, 0xfffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( tx1_main_writemem )
	{ 0x00000, 0x00fff, MWA_RAM },
	{ 0x01000, 0x01fff, tx1_ram0_mirror_w },
	{ 0x02000, 0x02fff, MWA_RAM },
	{ 0x03000, 0x03fff, tx1_ram1_mirror_w },
	{ 0x04000, 0x04fff, MWA_RAM, &generic_nvram, &generic_nvram_size },
	{ 0x05000, 0x05fff, tx1_nvram_mirror_w },
	{ 0x06000, 0x06fff, crtc_w8 },
	{ 0x08000, 0x09fff, vram_w },
	{ 0x0a000, 0x0afff, rcram_w },
	{ 0x0b000, 0x0b001, z80_busreq_w8 },
	{ 0x0c000, 0x0c001, tx1_scolst_w8 },
	{ 0x0d000, 0x0d003, slincs_w8 },
	{ 0x0e000, 0x0e001, slock_w8 },
	{ 0x0f000, 0x0f001, resume_math_w },
	{ 0x10000, 0x1ffff, z80_shared_w8 },
	{ 0x20000, 0x3ffff, MWA_ROM },
	{ 0x60000, 0x7ffff, MWA_ROM },
	{ 0xa0000, 0xbffff, MWA_ROM },
	{ 0xe0000, 0xfffff, MWA_ROM },
	{ 0x00000, 0xfffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

/* the ROM is at 0x4000-0x7fff, mirrored (0xf8000): its 0x5000-0x7fff are
   read through the arithmetic unit, its mirrors are plain ROM */
static MEMORY_READ_START( tx1_math_readmem )
	{ 0x00000, 0x007ff, math_ram_r },
	{ 0x00800, 0x00fff, tx1_spcs_ram_r8 },
	{ 0x01000, 0x01fff, rcram_r },
	{ 0x02000, 0x022ff, objram_r },
	{ 0x03000, 0x03fff, tx1_math_r8 },
	{ 0x04000, 0x04fff, MRA_ROM },
	{ 0x05000, 0x07fff, tx1_spcs_rom_r8 },
	{ 0x08000, 0xfffff, MRA_ROM },
	{ 0x00000, 0xfffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( tx1_math_writemem )
	{ 0x00000, 0x007ff, math_ram_w },
	{ 0x00800, 0x00fff, tx1_spcs_ram_w8 },
	{ 0x01000, 0x01fff, rcram_w },
	{ 0x02000, 0x022ff, objram_w },
	{ 0x02400, 0x027ff, bankcs_w8 },
	{ 0x02800, 0x02bff, halt_math_w },
	{ 0x02c00, 0x02fff, flgcs_w8 },
	{ 0x03000, 0x03fff, tx1_math_w8 },
	{ 0x04000, 0xfffff, MWA_ROM },
	{ 0x00000, 0xfffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_READ_START( tx1_sound_readmem )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x3000, 0x37ff, MRA_RAM },
	{ 0x3800, 0x3fff, tx1_z80_ram_mirror_r },
	{ 0x5000, 0x5003, ppi8255_0_r },
	{ 0x6000, 0x6003, tx1_pit8253_r },
	{ 0xb000, 0xbfff, tx1_ts_r },
	{ 0x0000, 0xffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( tx1_sound_writemem )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x3000, 0x37ff, MWA_RAM, &tx1_z80_ram },
	{ 0x3800, 0x3fff, tx1_z80_ram_mirror_w },
	{ 0x4000, 0x4000, tx1_z80_intreq_w },
	{ 0x5000, 0x5003, ppi8255_0_w },
	{ 0x6000, 0x6003, tx1_pit8253_w },
	{ 0x7000, 0x7fff, tx1_ppi_latch_w },
	{ 0xb000, 0xbfff, tx1_ts_w },
	{ 0x0000, 0xffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static PORT_WRITE_START( tx1_sound_writeport )
	{ 0x40, 0x40, AY8910_write_port_0_w },
	{ 0x41, 0x41, AY8910_control_port_0_w },
PORT_END


/*************************************
 *
 *  Buggy Boy Memory Maps
 *
 *************************************/

static MEMORY_READ_START( buggyboy_main_readmem )
	{ 0x00000, 0x03fff, MRA_RAM },
	{ 0x04000, 0x04fff, crtc_r8 },
	{ 0x08000, 0x09fff, vram_r },
	{ 0x0a000, 0x0afff, rcram_r },
	{ 0x0b000, 0x0b001, dipswitches_r8 },
	{ 0x0f000, 0x0f003, MRA_NOP },		/* watchdog */
	{ 0x10000, 0x1ffff, z80_shared_r8 },
	{ 0x20000, 0x2ffff, MRA_ROM },
	{ 0xf0000, 0xfffff, MRA_ROM },
	{ 0x00000, 0xfffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( buggyboy_main_writemem )
	{ 0x00000, 0x03fff, MWA_RAM, &generic_nvram, &generic_nvram_size },
	{ 0x04000, 0x04fff, crtc_w8 },
	{ 0x08000, 0x09fff, vram_w },
	{ 0x0a000, 0x0afff, rcram_w },
	{ 0x0b000, 0x0b001, z80_busreq_w8 },
	{ 0x0c000, 0x0c001, bb_scolst_w8 },
	{ 0x0d000, 0x0d003, slincs_w8 },
	{ 0x0e000, 0x0e001, bb_sky_w8 },
	{ 0x0f000, 0x0f003, resume_math_w },
	{ 0x10000, 0x1ffff, z80_shared_w8 },
	{ 0x20000, 0x2ffff, MWA_ROM },
	{ 0xf0000, 0xfffff, MWA_ROM },
	{ 0x00000, 0xfffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_READ_START( buggybjr_main_readmem )
	{ 0x00000, 0x03fff, MRA_RAM },
	{ 0x04000, 0x04fff, crtc_r8 },
	{ 0x08000, 0x08fff, vram_r },
	{ 0x0a000, 0x0afff, rcram_r },
	{ 0x0b000, 0x0b001, dipswitches_r8 },
	{ 0x0f000, 0x0f003, MRA_NOP },		/* watchdog */
	{ 0x10000, 0x1ffff, z80_shared_r8 },
	{ 0x20000, 0x2ffff, MRA_ROM },
	{ 0xf0000, 0xfffff, MRA_ROM },
	{ 0x00000, 0xfffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( buggybjr_main_writemem )
	{ 0x00000, 0x03fff, MWA_RAM, &generic_nvram, &generic_nvram_size },
	{ 0x04000, 0x04fff, crtc_w8 },
	{ 0x08000, 0x08fff, vram_w },
	{ 0x0a000, 0x0afff, rcram_w },
	{ 0x0b000, 0x0b001, z80_busreq_w8 },
	{ 0x0c000, 0x0c001, bb_scolst_w8 },
	{ 0x0d000, 0x0d003, slincs_w8 },
	{ 0x0e000, 0x0e001, bb_sky_w8 },
	{ 0x0f000, 0x0f003, resume_math_w },
	{ 0x10000, 0x1ffff, z80_shared_w8 },
	{ 0x20000, 0x2ffff, MWA_ROM },
	{ 0xf0000, 0xfffff, MWA_ROM },
	{ 0x00000, 0xfffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_READ_START( buggyboy_math_readmem )
	{ 0x00000, 0x007ff, math_ram_r },
	{ 0x00800, 0x00fff, bb_spcs_ram_r8 },
	{ 0x01000, 0x01fff, rcram_r },
	{ 0x02000, 0x022ff, objram_r },
	{ 0x03000, 0x03fff, bb_math_r8 },
	{ 0x04000, 0x04fff, MRA_ROM },
	{ 0x05000, 0x07fff, bb_spcs_rom_r8 },
	{ 0x08000, 0xfffff, MRA_ROM },
	{ 0x00000, 0xfffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( buggyboy_math_writemem )
	{ 0x00000, 0x007ff, math_ram_w },
	{ 0x00800, 0x00fff, bb_spcs_ram_w8 },
	{ 0x01000, 0x01fff, rcram_w },
	{ 0x02000, 0x022ff, objram_w },
	{ 0x02400, 0x024ff, gas_w8 },
	{ 0x03000, 0x03fff, bb_math_w8 },
	{ 0x04000, 0xfffff, MWA_ROM },
	{ 0x00000, 0xfffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

/* Buggy Boy Sound PCB TC033A */
static MEMORY_READ_START( buggyboy_sound_readmem )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x6000, 0x6001, bb_analog_r },
	{ 0x6800, 0x6803, ppi8255_0_r },
	{ 0x7000, 0x7003, tx1_pit8253_r },
	{ 0xc000, 0xc7ff, tx1_ts_r },
	{ 0x0000, 0xffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( buggyboy_sound_writemem )
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM, &tx1_z80_ram },
	{ 0x6800, 0x6803, ppi8255_0_w },
	{ 0x7000, 0x7003, tx1_pit8253_w },
	{ 0x7800, 0x7800, tx1_z80_intreq_w },
	{ 0xc000, 0xc7ff, tx1_ts_w },
	{ 0x0000, 0xffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

/* Buggy Boy Jr Sound PCB TC043 */
static MEMORY_READ_START( buggybjr_sound_readmem )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x5000, 0x5003, tx1_pit8253_r },
	{ 0x6000, 0x6001, bbjr_analog_r },
	{ 0xc000, 0xc7ff, tx1_ts_r },
	{ 0x0000, 0xffff, MRA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

static MEMORY_WRITE_START( buggybjr_sound_writemem )
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM, &tx1_z80_ram },
	{ 0x5000, 0x5003, tx1_pit8253_w },
	{ 0x7000, 0x7000, tx1_z80_intreq_w },
	{ 0xc000, 0xc7ff, tx1_ts_w },
	{ 0x0000, 0xffff, MWA_NOP },	/* unmapped (not in the save states) */
MEMORY_END

/* Common */
static PORT_READ_START( buggyboy_sound_readport )
	{ 0x40, 0x40, AY8910_read_port_0_r },
	{ 0x80, 0x80, AY8910_read_port_1_r },
PORT_END

static PORT_WRITE_START( buggyboy_sound_writeport )
	{ 0x40, 0x40, AY8910_write_port_0_w },
	{ 0x41, 0x41, AY8910_control_port_0_w },
	{ 0x80, 0x80, AY8910_write_port_1_w },
	{ 0x81, 0x81, AY8910_control_port_1_w },
PORT_END


/*************************************
 *
 *  Input ports
 *
 *************************************/

INPUT_PORTS_START( tx1 )
	PORT_START	/* DSW */
	/* Dipswitch DS.2 is 6 switches but "maps" to switches 2 to 8 (at 6P according to the manual)  */
	PORT_DIPNAME( 0x000c, 0x0000, "Game Cost" )
	PORT_DIPSETTING(      0x0000, "1 Coin Unit for 1 Credit" )
	PORT_DIPSETTING(      0x0004, "2 Coin Units for 1 Credit" )
	PORT_DIPSETTING(      0x0008, "3 Coin Units for 1 Credit" )
	PORT_DIPSETTING(      0x000c, "4 Coin Units for 1 Credit" )

	PORT_DIPNAME( 0x0010, 0x0000, "Left Coin Mechanism" )
	PORT_DIPSETTING(      0x0000, "1 Coin for 1 Coin Unit" )
	PORT_DIPSETTING(      0x0010, "1 Coin for 2 Coin Units" )

	PORT_DIPNAME( 0x0060, 0x0000, "Right Coin Mechanism" )
	PORT_DIPSETTING(      0x0000, "1 Coin for 1 Coin Unit" )
	PORT_DIPSETTING(      0x0020, "1 Coin for 4 Coin Units" )
	PORT_DIPSETTING(      0x0040, "1 Coin for 5 Coin Units" )
	PORT_DIPSETTING(      0x0060, "1 Coin for 6 Coin Units" )

	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) ) /* Manual states switches 6 to 8 unused (physically it's only 6 switches) */
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	/* Dipswitch DS.1 is 8 switches (at 8P according to the manual) */
	PORT_DIPNAME( 0x0700, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0000, "A (Easiest)" )
	PORT_DIPSETTING(      0x0100, "B" )
	PORT_DIPSETTING(      0x0200, "C" )
	PORT_DIPSETTING(      0x0300, "D" )
	PORT_DIPSETTING(      0x0400, "E" )
	PORT_DIPSETTING(      0x0500, "F" )
	PORT_DIPSETTING(      0x0600, "G" )
	PORT_DIPSETTING(      0x0700, "H (Hardest)" )

	PORT_DIPNAME( 0x1800, 0x1000, "Game Time" )
	PORT_DIPSETTING(      0x0000, "A (Longest)" )
	PORT_DIPSETTING(      0x0800, "B" )
	PORT_DIPSETTING(      0x1000, "C" )
	PORT_DIPSETTING(      0x1800, "D (Shortest)" )

	PORT_DIPNAME( 0xe000, 0xe000, "Bonus Adder" )
	PORT_DIPSETTING(      0x0000, "No Bonus" )
	PORT_DIPSETTING(      0x2000, "2 Coin Units for 1 Credit" )
	PORT_DIPSETTING(      0x4000, "3 Coin Units for 1 Credit" )
	PORT_DIPSETTING(      0x6000, "4 Coin Units for 1 Credit" )
	PORT_DIPSETTING(      0x8000, "5 Coin Units for 1 Credit" )
	PORT_DIPSETTING(      0xa000, "4 Coin Units for 2 Credit" )
	PORT_DIPSETTING(      0xc000, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(      0xe000, "No Bonus" )

	PORT_START	/* AN_STEERING */
	PORT_ANALOG( 0x0f, 0x00, IPT_DIAL, 20, 6, 0, 0 )

	PORT_START	/* AN_ACCELERATOR */
	PORT_ANALOG( 0x1f, 0x00, IPT_PEDAL, 25, 10, 0x00, 0x1f )

	PORT_START	/* AN_BRAKE */
	PORT_ANALOG( 0x1f, 0x00, IPT_PEDAL2, 25, 10, 0x00, 0x1f )

	PORT_START	/* not used by TX-1 */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* PPI_PORTC */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_SERVICE( 0x04, IP_ACTIVE_HIGH )
	PORT_BITX( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_TOGGLE, "Gear Change", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
INPUT_PORTS_END

INPUT_PORTS_START( buggyboy )
	PORT_START	/* DSW */
	/* Dipswitch 0 is unconnected */
	PORT_DIPNAME( 0x0003, 0x0003, "Do not change DSW2 1&2" ) /* Listed in manual as "Do Not Change" */
	PORT_DIPSETTING(      0x0000, "0" )
	PORT_DIPSETTING(      0x0001, "1" )
	PORT_DIPSETTING(      0x0002, "2" )
	PORT_DIPSETTING(      0x0003, "3" )

	PORT_DIPNAME( 0x0004, 0x0004, "Language" ) /* Language of game instructions */
	PORT_DIPSETTING(      0x0004, "English" )
	PORT_DIPSETTING(      0x0000, "Japanese" )

	PORT_DIPNAME( 0x0008, 0x0008, "Do not Change DSW2 4" ) /* Listed in manual as "Do Not Change" */
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( On ) )

	PORT_DIPNAME( 0x0030, 0x0010, "Time Rank" )
	PORT_DIPSETTING(      0x0000, "A (Longest)" )
	PORT_DIPSETTING(      0x0010, "B" )
	PORT_DIPSETTING(      0x0020, "C" )
	PORT_DIPSETTING(      0x0030, "D (Shortest)" )

	PORT_DIPNAME( 0x00c0, 0x0040, "Game Rank" )
	PORT_DIPSETTING(      0x0000, "A (Easy)")
	PORT_DIPSETTING(      0x0040, "B" )
	PORT_DIPSETTING(      0x0080, "C" )
	PORT_DIPSETTING(      0x00c0, "D (Difficult)" )

	PORT_DIPNAME( 0xe000, 0x0000, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( Free_Play ) )

	PORT_DIPNAME( 0x1800, 0x0800, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x1800, DEF_STR( 1C_5C ) )

	PORT_DIPNAME( 0x0700, 0x0700, "Do not change DSW1 1-3" ) /* Listed in manual as "Do Not Change" */
	PORT_DIPSETTING(      0x0000, "0" )
	PORT_DIPSETTING(      0x0100, "1" )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0400, "4" )
	PORT_DIPSETTING(      0x0500, "5" )
	PORT_DIPSETTING(      0x0600, "6" )
	PORT_DIPSETTING(      0x0700, "7" )

	PORT_START	/* AN_STEERING */
	PORT_ANALOG( 0x0f, 0x00, IPT_DIAL, 25, 25, 0, 0 )

	PORT_START	/* AN_ACCELERATOR */
	PORT_ANALOG( 0x1f, 0x00, IPT_PEDAL, 25, 10, 0x00, 0x1f )

	PORT_START	/* AN_BRAKE */
	PORT_ANALOG( 0x1f, 0x00, IPT_PEDAL2, 25, 10, 0x00, 0x1f )

	PORT_START	/* PPI_PORTA */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x38, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_TOGGLE, "Gear Change", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START	/* PPI_PORTC */
	PORT_DIPNAME( 0xff, 0x80, "Sound PCB Jumper" )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x20, "Speed Buggy/Data East" )
	PORT_DIPSETTING(    0x40, "Buggy Boy/Taito" )
	PORT_DIPSETTING(    0x80, "Buggy Boy/Tatsumi" )
INPUT_PORTS_END

INPUT_PORTS_START( buggybjr )
	PORT_START	/* DSW */
	/* Dipswitch 0 is unconnected */
	PORT_DIPNAME( 0x0003, 0x0003, "Do not change DSW2 1&2" ) /* Listed in manual as "Do Not Change" */
	PORT_DIPSETTING(      0x0000, "0" )
	PORT_DIPSETTING(      0x0001, "1" )
	PORT_DIPSETTING(      0x0002, "2" )
	PORT_DIPSETTING(      0x0003, "3" )

	PORT_DIPNAME( 0x0004, 0x0004, "Language" ) /* Language of game instructions */
	PORT_DIPSETTING(      0x0004, "English" )
	PORT_DIPSETTING(      0x0000, "Japanese" )

	PORT_DIPNAME( 0x0008, 0x0008, "Do not Change DSW2 4" ) /* Listed in manual as "Do Not Change" */
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( On ) )

	PORT_DIPNAME( 0x0030, 0x0010, "Time Rank" )
	PORT_DIPSETTING(      0x0000, "A (Longest)" )
	PORT_DIPSETTING(      0x0010, "B" )
	PORT_DIPSETTING(      0x0020, "C" )
	PORT_DIPSETTING(      0x0030, "D (Shortest)" )

	PORT_DIPNAME( 0x00c0, 0x0040, "Game Rank" )
	PORT_DIPSETTING(      0x0000, "A (Easy)")
	PORT_DIPSETTING(      0x0040, "B" )
	PORT_DIPSETTING(      0x0080, "C" )
	PORT_DIPSETTING(      0x00c0, "D (Difficult)" )

	PORT_DIPNAME( 0xe000, 0x0000, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( Free_Play ) )

	PORT_DIPNAME( 0x1800, 0x0800, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x1800, DEF_STR( 1C_6C ) )

	PORT_DIPNAME( 0x0700, 0x0700, "Do not change DSW1 1-3" ) /* Listed in manual as "Do Not Change" */
	PORT_DIPSETTING(      0x0000, "0" )
	PORT_DIPSETTING(      0x0100, "1" )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0400, "4" )
	PORT_DIPSETTING(      0x0500, "5" )
	PORT_DIPSETTING(      0x0600, "6" )
	PORT_DIPSETTING(      0x0700, "7" )

	PORT_START	/* AN_STEERING */
	PORT_ANALOG( 0x0f, 0x00, IPT_DIAL, 25, 25, 0, 0 )

	PORT_START	/* AN_ACCELERATOR */
	PORT_ANALOG( 0x1f, 0x00, IPT_PEDAL, 25, 10, 0x00, 0x1f )

	PORT_START	/* AN_BRAKE */
	PORT_ANALOG( 0x1f, 0x00, IPT_PEDAL2, 25, 10, 0x00, 0x1f )

	PORT_START	/* YM2149_IC19_A */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x38, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BITX( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_TOGGLE, "Gear Change", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START	/* YM2149_IC19_B: wire jumper setting on sound PCB */
	PORT_DIPNAME( 0xff, 0x80, "Sound PCB Jumper" )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x20, "Speed Buggy/Data East" )
	PORT_DIPSETTING(    0x40, "Buggy Boy/Taito" )
	PORT_DIPSETTING(    0x80, "Buggy Boy/Tatsumi" )
INPUT_PORTS_END


/*************************************
 *
 *  Sound interfaces
 *
 *************************************/

static READ_HANDLER( tx1_ppi_portc_r ) { return readinputport(TX1_PORT_PPI_C); }
static READ_HANDLER( bb_ppi_porta_r )  { return readinputport(TX1_PORT_PPI_A); }

static ppi8255_interface tx1_ppi8255_intf =
{
	1,
	{ tx1_ppi_porta_r },
	{ tx1_ppi_portb_r },
	{ tx1_ppi_portc_r },
	{ 0 },
	{ 0 },
	{ tx1_coin_cnt_w }
};

/* Buggy Boy uses an 8255 PPI instead of YM2149 ports for inputs! */
static ppi8255_interface buggyboy_ppi8255_intf =
{
	1,
	{ bb_ppi_porta_r },
	{ 0 },
	{ tx1_ppi_portc_r },
	{ 0 },
	{ bb_coin_cnt_w },
	{ 0 }
};

/* the levels of MAME: the AY at 0.1, the engines at 0.2 */
static struct AY8910interface tx1_ay8910_interface =
{
	1,
	TX1_PIXEL_CLOCK / 8,
	{ MIXER(10, MIXER_PAN_CENTER) },
	{ 0 },
	{ 0 },
	{ tx1_ay8910_a_w },
	{ tx1_ay8910_b_w },
	{ 0 }
};

static struct CustomSound_interface tx1_custom_interface =
{
	tx1_sh_start,
	0,
	0
};

/* the two YM2149s: MAME routes them at 0.15 */
static struct AY8910interface buggyboy_ym2149_interface =
{
	2,
	BUGGYBOY_ZCLK / 4,
	{ MIXER(15, MIXER_PAN_LEFT), MIXER(15, MIXER_PAN_RIGHT) },
	{ 0, 0 },
	{ 0, 0 },
	{ bb_ym1_a_w, bb_ym2_a_w },
	{ 0, bb_ym2_b_w },
	{ 0, 0 }
};

static struct AY8910interface buggybjr_ym2149_interface =
{
	2,
	BUGGYBOY_ZCLK / 4,
	{ MIXER(15, MIXER_PAN_LEFT), MIXER(15, MIXER_PAN_RIGHT) },
	{ input_port_4_r, 0 },	/* YM2149 IC19 */
	{ input_port_5_r, 0 },
	{ 0, bb_ym2_a_w },		/* YM2149 IC24 */
	{ 0, bbjr_ym2_b_w },
	{ 0, 0 }
};

static struct CustomSound_interface buggyboy_custom_interface =
{
	buggyboy_sh_start,
	0,
	0
};


/*************************************
 *
 *  Machine initialisation
 *
 *************************************/

static void tx1_ram_state_register(void)
{
	state_save_register_UINT16("tx1_ram", 0, "vram",     tx1_vram_words,     sizeof(tx1_vram_words) / 2);
	state_save_register_UINT16("tx1_ram", 0, "rcram",    tx1_rcram_words,    sizeof(tx1_rcram_words) / 2);
	state_save_register_UINT16("tx1_ram", 0, "math_ram", tx1_math_ram_words, sizeof(tx1_math_ram_words) / 2);
	state_save_register_UINT16("tx1_ram", 0, "objram",   tx1_objram_words,   sizeof(tx1_objram_words) / 2);
	state_save_register_UINT16("tx1_ram", 0, "bus16_rlatch", &bus16_rlatch, 1);
	state_save_register_UINT8 ("tx1_ram", 0, "bus16_wlatch", &bus16_wlatch, 1);
}

static void tx1_common_init(void)
{
	memset(tx1_vram_words, 0, sizeof(tx1_vram_words));
	memset(tx1_rcram_words, 0, sizeof(tx1_rcram_words));
	memset(tx1_math_ram_words, 0, sizeof(tx1_math_ram_words));
	memset(tx1_objram_words, 0, sizeof(tx1_objram_words));

	tx1_vram     = tx1_vram_words;
	tx1_rcram    = tx1_rcram_words;
	tx1_math_ram = tx1_math_ram_words;
	tx1_objram   = tx1_objram_words;
	tx1_vram_size = sizeof(tx1_vram_words);

	/* the math CPU ROM (0x4000-0x7fff) is mirrored (0xf8000) */
	{
		UINT8 *rom = memory_region(REGION_CPU2);
		int addr;

		for (addr = 0x0c000; addr < 0x100000; addr += 0x8000)
			memcpy(rom + addr, rom + 0x4000, 0x4000);
	}

	tx1_ram_state_register();
}

static DRIVER_INIT( tx1 )
{
	/* the main CPU ROM (0x20000-0x2ffff) is mirrored (0xd0000) */
	UINT8 *rom = memory_region(REGION_CPU1);
	static const UINT32 mirrors[7] = { 0x30000, 0x60000, 0x70000, 0xa0000, 0xb0000, 0xe0000, 0xf0000 };
	int i;

	for (i = 0; i < 7; i++)
		memcpy(rom + mirrors[i], rom + 0x20000, 0x10000);

	tx1_common_init();
}

static DRIVER_INIT( buggyboy )
{
	tx1_common_init();
}

static MACHINE_INIT( tx1_main )
{
	/* the two 8086s have a 16-bit data bus */
	i86_bus16 = 1;

	machine_init_tx1();
	tx1_sh_reset();
}

static MACHINE_INIT( buggyboy_main )
{
	/* the two 8086s have a 16-bit data bus */
	i86_bus16 = 1;

	machine_init_tx1();
	buggyboy_sh_reset();
}

static MACHINE_INIT( tx1_ppi )
{
	ppi8255_init(&tx1_ppi8255_intf);
	machine_init_tx1_main();
}

static MACHINE_INIT( buggyboy_ppi )
{
	ppi8255_init(&buggyboy_ppi8255_intf);
	machine_init_buggyboy_main();
}


/*************************************
 *
 *  Machine drivers
 *
 *************************************/

/* the CPUs exchange through RAM and the TEST pin of the math CPU */
#define TX1_INTERLEAVE	100

static MACHINE_DRIVER_START( tx1 )

	/* basic machine hardware */
	MDRV_CPU_ADD(I86, CPU_MASTER_CLOCK / 3)
	MDRV_CPU_MEMORY(tx1_main_readmem, tx1_main_writemem)

	MDRV_CPU_ADD(I86, CPU_MASTER_CLOCK / 3)
	MDRV_CPU_MEMORY(tx1_math_readmem, tx1_math_writemem)

	MDRV_CPU_ADD(Z80, TX1_PIXEL_CLOCK / 2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(tx1_sound_readmem, tx1_sound_writemem)
	MDRV_CPU_PORTS(0, tx1_sound_writeport)
	MDRV_CPU_PERIODIC_INT(tx1_z80_irq, TX1_PIXEL_CLOCK / 4 / 2048 / 2)

	MDRV_FRAMES_PER_SECOND((double)TX1_PIXEL_CLOCK / (TX1_HTOTAL * TX1_VTOTAL))
	MDRV_VBLANK_DURATION((double)(TX1_VTOTAL - TX1_VBSTART) * TX1_HTOTAL * 1000000 / TX1_PIXEL_CLOCK)
	MDRV_INTERLEAVE(TX1_INTERLEAVE)

	MDRV_MACHINE_INIT(tx1_ppi)
	MDRV_NVRAM_HANDLER(generic_0fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(3 * 256, 240)
	MDRV_VISIBLE_AREA(0, 3 * 256 - 1, 0, 240 - 1)
	MDRV_ASPECT_RATIO(12, 3)
	MDRV_PALETTE_LENGTH(256)

	MDRV_PALETTE_INIT(tx1)
	MDRV_VIDEO_START(tx1)
	MDRV_VIDEO_EOF(tx1)
	MDRV_VIDEO_UPDATE(tx1)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(AY8910, tx1_ay8910_interface)
	MDRV_SOUND_ADD(CUSTOM, tx1_custom_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( buggyboy )

	/* basic machine hardware */
	MDRV_CPU_ADD(I86, CPU_MASTER_CLOCK / 3)
	MDRV_CPU_MEMORY(buggyboy_main_readmem, buggyboy_main_writemem)

	MDRV_CPU_ADD(I86, CPU_MASTER_CLOCK / 3)
	MDRV_CPU_MEMORY(buggyboy_math_readmem, buggyboy_math_writemem)

	MDRV_CPU_ADD(Z80, BUGGYBOY_ZCLK / 2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(buggyboy_sound_readmem, buggyboy_sound_writemem)
	MDRV_CPU_PORTS(buggyboy_sound_readport, buggyboy_sound_writeport)
	MDRV_CPU_PERIODIC_INT(tx1_z80_irq, BUGGYBOY_ZCLK / 2 / 4 / 2048)

	MDRV_FRAMES_PER_SECOND((double)BB_PIXEL_CLOCK / (BB_HTOTAL * BB_VTOTAL))
	MDRV_VBLANK_DURATION((double)(BB_VTOTAL - BB_VBSTART) * BB_HTOTAL * 1000000 / BB_PIXEL_CLOCK)
	MDRV_INTERLEAVE(TX1_INTERLEAVE)

	MDRV_MACHINE_INIT(buggyboy_ppi)
	MDRV_NVRAM_HANDLER(generic_0fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(3 * 256, 240)
	MDRV_VISIBLE_AREA(0, 3 * 256 - 1, 0, 240 - 1)
	MDRV_ASPECT_RATIO(12, 3)
	MDRV_PALETTE_LENGTH(256)

	MDRV_PALETTE_INIT(buggyboy)
	MDRV_VIDEO_START(buggyboy)
	MDRV_VIDEO_EOF(buggyboy)
	MDRV_VIDEO_UPDATE(buggyboy)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(AY8910, buggyboy_ym2149_interface)
	MDRV_SOUND_ADD(CUSTOM, buggyboy_custom_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( buggybjr )

	/* basic machine hardware */
	MDRV_CPU_ADD(I86, CPU_MASTER_CLOCK / 3)
	MDRV_CPU_MEMORY(buggybjr_main_readmem, buggybjr_main_writemem)

	MDRV_CPU_ADD(I86, CPU_MASTER_CLOCK / 3)
	MDRV_CPU_MEMORY(buggyboy_math_readmem, buggyboy_math_writemem)

	MDRV_CPU_ADD(Z80, BUGGYBOY_ZCLK / 2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(buggybjr_sound_readmem, buggybjr_sound_writemem)
	MDRV_CPU_PORTS(buggyboy_sound_readport, buggyboy_sound_writeport)
	MDRV_CPU_PERIODIC_INT(tx1_z80_irq, BUGGYBOY_ZCLK / 2 / 4 / 2048)

	MDRV_FRAMES_PER_SECOND((double)BB_PIXEL_CLOCK / (BB_HTOTAL * BB_VTOTAL))
	MDRV_VBLANK_DURATION((double)(BB_VTOTAL - BB_VBSTART) * BB_HTOTAL * 1000000 / BB_PIXEL_CLOCK)
	MDRV_INTERLEAVE(TX1_INTERLEAVE)

	MDRV_MACHINE_INIT(buggyboy_main)
	MDRV_NVRAM_HANDLER(generic_0fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 240)
	MDRV_VISIBLE_AREA(0, 256 - 1, 0, 240 - 1)
	MDRV_PALETTE_LENGTH(256)

	MDRV_PALETTE_INIT(buggyboy)
	MDRV_VIDEO_START(buggybjr)
	MDRV_VIDEO_EOF(buggyboy)
	MDRV_VIDEO_UPDATE(buggybjr)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(AY8910, buggybjr_ym2149_interface)
	MDRV_SOUND_ADD(CUSTOM, buggyboy_custom_interface)
MACHINE_DRIVER_END


/*************************************
 *
 *  ROM definition(s)
 *
 *************************************/

/*
	The TX-1 (Multi License (World)) game is comprised of three boards:
	- Sound Board (labeled TC013B, top small board, different then the below mentioned TC013A)
	- CPU Board   (labeled TC011A, middle board, uses 15.000 MHz xtal)
	- Video Board (labeled TC012A, bottom board, uses 18.000 MHz xtal)
*/
ROM_START( tx1 )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* main CPU */
	ROM_LOAD16_BYTE( "8412-136027-244.22", 0x20000, 0x4000, CRC(2e9cefa2) SHA1(4ca04eae446e8df08ab793488a79217ed1a27875) )
	ROM_LOAD16_BYTE( "8412-136027-245.29", 0x20001, 0x4000, CRC(ade7895c) SHA1(1c33a574cae46fddb4cadb85f5de17f02ae7a596) )

	ROM_LOAD16_BYTE( "8412-136027-249.45", 0x28000, 0x4000, CRC(9bcb82db) SHA1(d1528c9b9c4c2848bdba15e4632927476d544f40) )
	ROM_LOAD16_BYTE( "8412-136027-250.54", 0x28001, 0x4000, CRC(c8c9368f) SHA1(0972d54d506216eb2b204cf22ccdff9210fb7b10) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )	/* math CPU */
	ROM_LOAD16_BYTE( "8411-136027-152.146", 0x04000, 0x2000, CRC(b65eeea2) SHA1(b5f26e17520c598132b93c5cd7af7ebd03b10012) )
	ROM_LOAD16_BYTE( "8411-136027-151.132", 0x04001, 0x2000, CRC(0d63dadc) SHA1(0954174b25c08967d3efb31f5721fd05502d66dd) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* audio CPU */
	ROM_LOAD( "8411-136027-157.11", 0x00000, 0x2000, CRC(10ae3075) SHA1(69c5f62f2473aba848383eed3cecf15e273d86ca) )

	ROM_REGION( 0x8000, REGION_GFX1, 0 )	/* char tiles */
	ROM_LOAD( "8411-136027-156.204", 0x0000, 0x4000, CRC(60f3c616) SHA1(59c4361891e4274e27e6279c919e8fd6803af7cf) )
	ROM_LOAD( "8411-136027-155.174", 0x4000, 0x4000, CRC(e59a6b72) SHA1(c10efa77ab421ac60b97227a8d547f50f8415670) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 )	/* obj tiles */
	ROM_LOAD( "8411-136027-114.203", 0x0000, 0x4000, CRC(fc91328b) SHA1(e57fd2056b65d37cf2e1f0af56616c6555df3006) )
	ROM_LOAD( "8411-136027-116.258", 0x4000, 0x4000, CRC(5745f671) SHA1(6e471633cd6de9926b3361a84430c088e1f6a097) )
	ROM_LOAD( "8411-136027-115.173", 0x8000, 0x4000, CRC(720e5873) SHA1(151d9063c35b26f5876cf94bdf0c2665ec701bbd) )
	ROM_LOAD( "8411-136027-117.232", 0xc000, 0x4000, CRC(3c68d0bc) SHA1(2dbaf2a268b90214fd61c016ac945d4371057826) )

	ROM_REGION( 0x6000, REGION_GFX3, 0 )	/* road */
	ROM_LOAD( "8411-136027-146.56", 0x0000, 0x2000, CRC(5635b8c1) SHA1(5cc9437a2ff0843f1917f2451852d4561c240b24) )
	ROM_LOAD( "8411-136027-147.66", 0x2000, 0x2000, CRC(03d83cf8) SHA1(5c0cfc6bf02ad2b3f37e1ceb493f69eb9829ab1e) )
	ROM_LOAD( "8411-136027-148.76", 0x4000, 0x2000, CRC(ad56013a) SHA1(ae3a91f58f30daff334754476db33ad1d12569fc) )

	ROM_REGION16_LE( 0x10000, REGION_USER3, 0 )	/* au data */
	ROM_LOAD16_BYTE( "8411-136027-153.184", 0x0000, 0x4000, CRC(acf754e8) SHA1(06779e18636f0799efdaa09396e9ccd59f426257) )
	ROM_LOAD16_BYTE( "8411-136027-154.185", 0x0001, 0x4000, CRC(f89d3e20) SHA1(4b4cf679b7e3d63cded9989d2b667941f718ff57) )
	ROM_LOAD16_BYTE( "136027-143.ic223",    0x8000, 0x0200, CRC(22c77af6) SHA1(1be8585b95316b4fc5712cdaef699e676320cd4d) )
	ROM_LOAD16_BYTE( "136027-142.ic213",    0x8001, 0x0200, CRC(f6b8b70b) SHA1(b79374acf11d71db1e4ad3c494ac5f500a52677b) )

	ROM_REGION( 0x8000, REGION_USER1, 0 )	/* obj map */
	ROM_LOAD( "8411-136027-119.106", 0x0000, 0x4000, CRC(88eec0fb) SHA1(81d7a69dc1a4b3b81d7f28d97a3f80697cdcc6eb) )
	ROM_LOAD( "8411-136027-120.73",  0x4000, 0x4000, CRC(407cbe65) SHA1(e1c11b65f3c6abde6d55afeaffdb39cdd6d66377) )

	ROM_REGION( 0x6000, REGION_USER2, 0 )	/* obj luts */
	ROM_LOAD( "8411-136027-113.48",  0x0000, 0x2000, CRC(4b3d7956) SHA1(fc2432dd69f3be7007d4fd6f7c86c7c19453b1ba) )
	ROM_LOAD( "8411-136027-118.281", 0x2000, 0x4000, CRC(de418dc7) SHA1(1233e2f7499ec5a73a40ee336d3fe26c06187784) )

	ROM_REGION( 0x10000, REGION_PROMS, 0 )
	/* RGB palette (left) */
	ROM_LOAD( "136027-133.57", 0x0000, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "136027-134.58", 0x0100, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "136027-135.59", 0x0200, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* RGB palette (center) */
	ROM_LOAD( "136027-133.36", 0x0300, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "136027-134.37", 0x0400, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "136027-135.38", 0x0500, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* RGB palette (right) */
	ROM_LOAD( "136027-133.8",  0x0600, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "136027-134.9",  0x0700, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "136027-135.10", 0x0800, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* Character colour tables (L, C, R) */
	ROM_LOAD( "136027-124.85",  0x0900, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )
	ROM_LOAD( "136027-124.116", 0x0a00, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )
	ROM_LOAD( "136027-124.148", 0x0b00, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )

	/* Object colour table */
	ROM_LOAD( "136027-136.276", 0x0c00, 0x200, CRC(7b675b8b) SHA1(3a7617d8ca29aa5d9832e317736598646a466b8b) )
	ROM_LOAD( "136027-136.277", 0x0e00, 0x200, CRC(7b675b8b) SHA1(3a7617d8ca29aa5d9832e317736598646a466b8b) )

	/* Object tile lookup */
	ROM_LOAD( "136027-123.25", 0x1000, 0x100, CRC(616a7a85) SHA1(b7c1060ecb128154092441212de64dc304aa3fcd) )

	/* Road graphics */
	ROM_LOAD( "136027-138.ic33", 0x1100, 0x200, CRC(fafb6917) SHA1(30eb182c7623026dce7dba9e249bc8a9eb7a7f3e) )
	ROM_LOAD( "136027-139.ic40", 0x1300, 0x200, CRC(93deb894) SHA1(5ae9a21298c836fe649a52f3df2b4067f9012b91) )
	ROM_LOAD( "136027-140.ic49", 0x1500, 0x200, CRC(aa5ed232) SHA1(f33e7bc2dd33ac6d75fb06b93c4dd58e5d10010d) )

	/* Road stripes */
	ROM_LOAD( "136027-141.ic50", 0x1700, 0x200, CRC(6b424cea) SHA1(83127326c20116b0a4be1126e163f9c6755e19dc) )
ROM_END

/*
	The TX-1 (Japan Rev B) game is comprised of three boards:
	- Sound Board (labeled TC013A, top small board)
	- CPU Board   (labeled TC011A, middle board, uses 15.000 MHz xtal)
	- Video Board (labeled TC012A, bottom board, uses 18.000 MHz xtal)
*/
ROM_START( tx1jb )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* main CPU */
	ROM_LOAD16_BYTE( "tx1_1b.ic22", 0x20000, 0x4000, CRC(3dadb8fb) SHA1(ef99897161d559180abc7f4ce23e8f55fe3bb330) )
	ROM_LOAD16_BYTE( "tx1_2b.ic29", 0x20001, 0x4000, CRC(6b5a8f23) SHA1(0d82fdcadc29d9a1d7ae9b87dd1f94bd508da4b2) )

	ROM_LOAD16_BYTE( "tx1_3b.ic45", 0x28000, 0x4000, CRC(3a030996) SHA1(7dbbad310380247159c12e5c100ce57193d16e90) )
	ROM_LOAD16_BYTE( "tx1_4b.ic54", 0x28001, 0x4000, CRC(7aed82ed) SHA1(592d554d02a4b3806154dc008177de83a411ffc6) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )	/* math CPU */
	ROM_LOAD16_BYTE( "tx1_9b.ic146", 0x04000, 0x2000, CRC(af677ff4) SHA1(3968d7f3811ed7552efe1e1c5c416ec740503ee4) )
	ROM_LOAD16_BYTE( "tx1_8b.ic132", 0x04001, 0x2000, CRC(dd4356f8) SHA1(b666d2aa93e61f6bcdb8326b8b06635be743b64e) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* audio CPU */
	ROM_LOAD( "tx1_22h.ic9", 0x00000, 0x2000, CRC(66376232) SHA1(b8a026dae47173e7760eea4f52e67e525ad1b70b) )

	ROM_REGION( 0x8000, REGION_GFX1, 0 )	/* char tiles */
	ROM_LOAD( "tx1_21a.ic204", 0x0000, 0x4000, CRC(cd3441ad) SHA1(8e6597b3177b8aaa34ed3373d85fc4b6231e1333) )
	ROM_LOAD( "tx1_20a.ic174", 0x4000, 0x4000, CRC(dbe595fc) SHA1(1ed2f775f0a1b46a2ffbc056eb4ef732ed546d3c) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 )	/* obj tiles */
	ROM_LOAD( "tx1_16b.ic203", 0x0000, 0x4000, CRC(1141c965) SHA1(4b90c1428bcbd72d0449c064856a5596269b3fc6) )
	ROM_LOAD( "tx1_18b.ic258", 0x4000, 0x4000, CRC(0ad36d68) SHA1(fd5a65c56557c1bc9c0f3916f15f62500b52bfe0) )
	ROM_LOAD( "tx1_15b.ic173", 0x8000, 0x4000, CRC(30d1a8d5) SHA1(b4c585b7b8a8920bb3949d643e9e10c17d4009a0) )
	ROM_LOAD( "tx1_17b.ic232", 0xc000, 0x4000, CRC(364bb354) SHA1(a26581ca1088b979285471e2c6595048df84d75e) )

	ROM_REGION( 0x6000, REGION_GFX3, 0 )	/* road */
	ROM_LOAD( "tx1_5a.ic56", 0x0000, 0x2000, CRC(5635b8c1) SHA1(5cc9437a2ff0843f1917f2451852d4561c240b24) )
	ROM_LOAD( "tx1_6a.ic66", 0x2000, 0x2000, CRC(03d83cf8) SHA1(5c0cfc6bf02ad2b3f37e1ceb493f69eb9829ab1e) )
	ROM_LOAD( "tx1_7a.ic76", 0x4000, 0x2000, CRC(ad56013a) SHA1(ae3a91f58f30daff334754476db33ad1d12569fc) )

	ROM_REGION16_LE( 0x10000, REGION_USER3, 0 )	/* au data */
	ROM_LOAD16_BYTE( "tx1_10b.ic184", 0x0000, 0x4000, CRC(acf754e8) SHA1(06779e18636f0799efdaa09396e9ccd59f426257) )
	ROM_LOAD16_BYTE( "tx1_11b.ic185", 0x0001, 0x4000, CRC(f89d3e20) SHA1(4b4cf679b7e3d63cded9989d2b667941f718ff57) )
	ROM_LOAD16_BYTE( "xb02b.ic223",   0x8000, 0x0200, CRC(22c77af6) SHA1(1be8585b95316b4fc5712cdaef699e676320cd4d) )
	ROM_LOAD16_BYTE( "xb01b.ic213",   0x8001, 0x0200, CRC(f6b8b70b) SHA1(b79374acf11d71db1e4ad3c494ac5f500a52677b) )

	ROM_REGION( 0x8000, REGION_USER1, 0 )	/* obj map */
	ROM_LOAD( "tx1_14b.ic106", 0x0000, 0x4000, CRC(68c63d6e) SHA1(110e02b99c44d31041be588bd14642e26890ecbd) )
	ROM_LOAD( "tx1_13b.ic73",  0x4000, 0x4000, CRC(b0c581b2) SHA1(20926bc15e7c97045b219b828acfcdd99b8712a6) )

	ROM_REGION( 0x6000, REGION_USER2, 0 )	/* obj luts */
	ROM_LOAD( "tx1_12b.ic48",  0x0000, 0x2000, CRC(4b3d7956) SHA1(fc2432dd69f3be7007d4fd6f7c86c7c19453b1ba) )
	ROM_LOAD( "tx1_19b.ic281", 0x2000, 0x4000, CRC(cb250de6) SHA1(4bf3006986fb8cbb3dd4fa988e6471633614e4bb) )

	ROM_REGION( 0x10000, REGION_PROMS, 0 )
	/* RGB palette (left) */
	ROM_LOAD( "xb05a.ic57", 0x0000, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "xb06a.ic58", 0x0100, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "xb07a.ic59", 0x0200, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* RGB palette (center) */
	ROM_LOAD( "xb05a.ic36", 0x0300, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "xb06a.ic37", 0x0400, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "xb07a.ic38", 0x0500, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* RGB palette (right) */
	ROM_LOAD( "xb05a.ic8",  0x0600, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "xb06a.ic9",  0x0700, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "xb07a.ic10", 0x0800, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* Character colour tables (L, C, R) */
	ROM_LOAD( "xb08.ic85",  0x0900, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )
	ROM_LOAD( "xb08.ic116", 0x0a00, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )
	ROM_LOAD( "xb08.ic148", 0x0b00, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )

	/* Object colour table */
	ROM_LOAD( "xb04a.ic276",0x0c00, 0x200, CRC(92bf5533) SHA1(4d9127417325af66099234178ab2641d23ee9d22) )
	ROM_LOAD( "xb04a.ic277",0x0e00, 0x200, CRC(92bf5533) SHA1(4d9127417325af66099234178ab2641d23ee9d22) )

	/* Object tile lookup */
	ROM_LOAD( "xb03a.ic25", 0x1000, 0x100, CRC(616a7a85) SHA1(b7c1060ecb128154092441212de64dc304aa3fcd) )

	/* Road graphics */
	ROM_LOAD( "xb09.ic33",  0x1100, 0x200, CRC(fafb6917) SHA1(30eb182c7623026dce7dba9e249bc8a9eb7a7f3e) )
	ROM_LOAD( "xb10.ic40",  0x1300, 0x200, CRC(93deb894) SHA1(5ae9a21298c836fe649a52f3df2b4067f9012b91) )
	ROM_LOAD( "xb11.ic49",  0x1500, 0x200, CRC(aa5ed232) SHA1(f33e7bc2dd33ac6d75fb06b93c4dd58e5d10010d) )

	/* Road stripes */
	ROM_LOAD( "xb12.ic50",  0x1700, 0x200, CRC(6b424cea) SHA1(83127326c20116b0a4be1126e163f9c6755e19dc) )
ROM_END

ROM_START( tx1jc )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* main CPU */
	ROM_LOAD16_BYTE( "tx1_1c.ic22", 0x20000, 0x4000, CRC(eedcee83) SHA1(7fa0590b142fb13c6562126a9bdd5a1e032880c7) )
	ROM_LOAD16_BYTE( "tx1_2c.ic29", 0x20001, 0x4000, CRC(294bf5bf) SHA1(02b425caba8a187c58211bab27988205eb044558) )

	ROM_LOAD16_BYTE( "tx1_3c.ic45", 0x28000, 0x4000, CRC(21a8aa55) SHA1(21bc4adefb22a95fcd7a4e305bf0b05e2cb34129) )
	ROM_LOAD16_BYTE( "tx1_4c.ic54", 0x28001, 0x4000, CRC(15bb8ef2) SHA1(83968f010ec555fcd0548a80562fb23a892b5afb) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )	/* math CPU */
	ROM_LOAD16_BYTE( "tx1_9c.ic146", 0x04000, 0x2000, CRC(b65eeea2) SHA1(b5f26e17520c598132b93c5cd7af7ebd03b10012) )
	ROM_LOAD16_BYTE( "tx1_8c.ic132", 0x04001, 0x2000, CRC(0d63dadc) SHA1(0954174b25c08967d3efb31f5721fd05502d66dd) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* audio CPU: label was missing */
	ROM_LOAD( "8411-136027-157.11", 0x00000, 0x2000, CRC(10ae3075) SHA1(69c5f62f2473aba848383eed3cecf15e273d86ca) ) /* Unconfirmed TC013A or the later TC013B */

	ROM_REGION( 0x8000, REGION_GFX1, 0 )	/* char tiles */
	ROM_LOAD( "tx1_21a.ic204", 0x0000, 0x4000, CRC(cd3441ad) SHA1(8e6597b3177b8aaa34ed3373d85fc4b6231e1333) )
	ROM_LOAD( "tx1_20a.ic174", 0x4000, 0x4000, CRC(dbe595fc) SHA1(1ed2f775f0a1b46a2ffbc056eb4ef732ed546d3c) )

	ROM_REGION( 0x10000, REGION_GFX2, 0 )	/* obj tiles */
	ROM_LOAD( "tx1_16b.ic203", 0x0000, 0x4000, CRC(1141c965) SHA1(4b90c1428bcbd72d0449c064856a5596269b3fc6) )
	ROM_LOAD( "tx1_18b.ic258", 0x4000, 0x4000, CRC(0ad36d68) SHA1(fd5a65c56557c1bc9c0f3916f15f62500b52bfe0) )
	ROM_LOAD( "tx1_15b.ic173", 0x8000, 0x4000, CRC(30d1a8d5) SHA1(b4c585b7b8a8920bb3949d643e9e10c17d4009a0) )
	ROM_LOAD( "tx1_17b.ic232", 0xc000, 0x4000, CRC(364bb354) SHA1(a26581ca1088b979285471e2c6595048df84d75e) )

	ROM_REGION( 0x6000, REGION_GFX3, 0 )	/* road */
	ROM_LOAD( "tx1_5a.ic56", 0x0000, 0x2000, CRC(5635b8c1) SHA1(5cc9437a2ff0843f1917f2451852d4561c240b24) )
	ROM_LOAD( "tx1_6a.ic66", 0x2000, 0x2000, CRC(03d83cf8) SHA1(5c0cfc6bf02ad2b3f37e1ceb493f69eb9829ab1e) )
	ROM_LOAD( "tx1_7a.ic76", 0x4000, 0x2000, CRC(ad56013a) SHA1(ae3a91f58f30daff334754476db33ad1d12569fc) )

	ROM_REGION16_LE( 0x10000, REGION_USER3, 0 )	/* au data */
	ROM_LOAD16_BYTE( "tx1_10b.ic184", 0x0000, 0x4000, CRC(acf754e8) SHA1(06779e18636f0799efdaa09396e9ccd59f426257) )
	ROM_LOAD16_BYTE( "tx1_11b.ic185", 0x0001, 0x4000, CRC(f89d3e20) SHA1(4b4cf679b7e3d63cded9989d2b667941f718ff57) )
	ROM_LOAD16_BYTE( "xb02b.ic223",   0x8000, 0x0200, CRC(22c77af6) SHA1(1be8585b95316b4fc5712cdaef699e676320cd4d) )
	ROM_LOAD16_BYTE( "xb01b.ic213",   0x8001, 0x0200, CRC(f6b8b70b) SHA1(b79374acf11d71db1e4ad3c494ac5f500a52677b) )

	ROM_REGION( 0x8000, REGION_USER1, 0 )	/* obj map */
	ROM_LOAD( "tx1_14b.ic106", 0x0000, 0x4000, CRC(68c63d6e) SHA1(110e02b99c44d31041be588bd14642e26890ecbd) )
	ROM_LOAD( "tx1_13b.ic73",  0x4000, 0x4000, CRC(b0c581b2) SHA1(20926bc15e7c97045b219b828acfcdd99b8712a6) )

	ROM_REGION( 0x6000, REGION_USER2, 0 )	/* obj luts */
	ROM_LOAD( "tx1_12b.ic48",  0x0000, 0x2000, CRC(4b3d7956) SHA1(fc2432dd69f3be7007d4fd6f7c86c7c19453b1ba) )
	ROM_LOAD( "tx1_19b.ic281", 0x2000, 0x4000, CRC(cb250de6) SHA1(4bf3006986fb8cbb3dd4fa988e6471633614e4bb) )

	ROM_REGION( 0x10000, REGION_PROMS, 0 )
	/* RGB palette (left) */
	ROM_LOAD( "xb05a.ic57", 0x0000, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "xb06a.ic58", 0x0100, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "xb07a.ic59", 0x0200, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* RGB palette (center) */
	ROM_LOAD( "xb05a.ic36", 0x0300, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "xb06a.ic37", 0x0400, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "xb07a.ic38", 0x0500, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* RGB palette (right) */
	ROM_LOAD( "xb05a.ic8",  0x0600, 0x100, CRC(3b387d01) SHA1(1229548e3052ad34eeee9598743091d19f6b8f88) )
	ROM_LOAD( "xb06a.ic9",  0x0700, 0x100, CRC(f6f4d7d9) SHA1(866024b76b26d6942bd4e1d2494686299414f6be) )
	ROM_LOAD( "xb07a.ic10", 0x0800, 0x100, CRC(824e7532) SHA1(917ce74d2bae6af90f2c4e41d12a69f884320915) )

	/* Character colour tables (L, C, R) */
	ROM_LOAD( "xb08.ic85",  0x0900, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )
	ROM_LOAD( "xb08.ic116", 0x0a00, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )
	ROM_LOAD( "xb08.ic148", 0x0b00, 0x100, CRC(5aeef5cc) SHA1(e123bf01d556178b0cf9d495bcce445f3f8421cd) )

	/* Object colour table */
	ROM_LOAD( "xb04a.ic276",0x0c00, 0x200, CRC(92bf5533) SHA1(4d9127417325af66099234178ab2641d23ee9d22) )
	ROM_LOAD( "xb04a.ic277",0x0e00, 0x200, CRC(92bf5533) SHA1(4d9127417325af66099234178ab2641d23ee9d22) )

	/* Object tile lookup */
	ROM_LOAD( "xb03a.ic25", 0x1000, 0x100, CRC(616a7a85) SHA1(b7c1060ecb128154092441212de64dc304aa3fcd) )

	/* Road graphics */
	ROM_LOAD( "xb09.ic33",  0x1100, 0x200, CRC(fafb6917) SHA1(30eb182c7623026dce7dba9e249bc8a9eb7a7f3e) )
	ROM_LOAD( "xb10.ic40",  0x1300, 0x200, CRC(93deb894) SHA1(5ae9a21298c836fe649a52f3df2b4067f9012b91) )
	ROM_LOAD( "xb11.ic49",  0x1500, 0x200, CRC(aa5ed232) SHA1(f33e7bc2dd33ac6d75fb06b93c4dd58e5d10010d) )

	/* Road stripes */
	ROM_LOAD( "xb12.ic50",  0x1700, 0x200, CRC(6b424cea) SHA1(83127326c20116b0a4be1126e163f9c6755e19dc) )
ROM_END


/*
	The game is comprised of three boards:
	- Sound Board (labeled TC033A, top small board)
	- CPU Board (labeled TC031, middle board, uses 15.000 MHz xtal)
	- Video Board (labeled TC032, bottom board, uses 18.000 MHz xtal)
*/
ROM_START( buggyboy )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* main CPU */
	ROM_LOAD16_BYTE( "bug1a.230", 0x20000, 0x8000, CRC(92797c25) SHA1(8f7434abbd7f557d3202abb01b1e4899c82c67a5) )
	ROM_LOAD16_BYTE( "bug4a.173", 0x20001, 0x8000, CRC(40ce3930) SHA1(4bf62ebeea1549a13a21a32cb860717f064b186a) )

	ROM_LOAD16_BYTE( "bug2d.231", 0xf0000, 0x4000, CRC(f67bd593) SHA1(b6e3f9d5534b0addbba4aa4c813dda21a27cafa2) )
	ROM_LOAD16_BYTE( "bug5d.174", 0xf0001, 0x4000, CRC(d0dd6ffc) SHA1(377581ace86ea0f713aa7dc8f96f27cbcda1b2ea) )

	ROM_LOAD16_BYTE( "bug3b.232", 0xf8000, 0x4000, CRC(43cce3f0) SHA1(17de3728d809d386f6a7a330c8c8701975d4ebed) )
	ROM_LOAD16_BYTE( "bug6b.175", 0xf8001, 0x4000, CRC(8f000dfa) SHA1(5fd78a03a00f547bbb431839f78a8d10a4ba8e3e) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )	/* math CPU */
	ROM_LOAD16_BYTE( "bug8a.061", 0x04000, 0x2000, CRC(512291cd) SHA1(60f87133c86b88b982ba4680f96d0ac55970cb8d) )
	ROM_LOAD16_BYTE( "bug7a.060", 0x04001, 0x2000, CRC(d24dfdef) SHA1(37d05a8bf9567380523df01265afb9780e39ea2a) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* audio CPU */
	ROM_LOAD( "bug35.11", 0x00000, 0x4000,  CRC(7aa16e9e) SHA1(ea54e56270f70351a62a78fa32027bb41ef9861e) )

	ROM_REGION( 0x8000, REGION_GFX1, 0 )	/* char tiles */
	ROM_LOAD( "bug34a",     0x00000, 0x4000, CRC(8c64eb3c) SHA1(e0846f9a4811a0b6561d7a9aa7d50c23ced8d1d3) )
	ROM_LOAD( "bug33a.168", 0x04000, 0x4000, CRC(b500d683) SHA1(c9f92930c815a947fbd9a3846fd9580671da324f) )

	ROM_REGION( 0x40000, REGION_GFX2, 0 )	/* obj tiles */
	ROM_LOAD( "bug26.275", 0x00000, 0x4000, CRC(fc6fce64) SHA1(4cea5796c26980455bdc0f8fcdb8e719caa8041f) )
	ROM_LOAD( "bug27.245", 0x04000, 0x4000, CRC(54b9a9dd) SHA1(4f4f29a459991501013cdfffd34b50eed1cde850) )
	ROM_LOAD( "bug19.274", 0x08000, 0x4000, CRC(3392c2ef) SHA1(a4aa4f24517da16495be073aa590cda81d9fca49) )
	ROM_LOAD( "bug20.244", 0x0c000, 0x4000, CRC(9ee4c236) SHA1(51155193e470a82deb5e3f1979ce112ada39b705) )

	ROM_LOAD( "bug28.243", 0x10000, 0x4000, CRC(feef6c27) SHA1(61679d67a6ef85965078e3ddd11c178a1a55f223) )
	ROM_LOAD( "bug29.212", 0x14000, 0x4000, CRC(f570e00b) SHA1(ced4b9a4a324a4c92c08e088fa116469b2878f55) )
	ROM_LOAD( "bug21.242", 0x18000, 0x4000, CRC(088fef40) SHA1(a0c866c32857690915a33b0810219fb3cbf24f24) )
	ROM_LOAD( "bug22.211", 0x1c000, 0x4000, CRC(5ec02630) SHA1(75233de03461016d1bc7c4ece0502e16e53c3351) )

	ROM_LOAD( "bug30.186", 0x20000, 0x4000, CRC(5c2ecabf) SHA1(5be25ddc1e2aac4579a39e405f8eac919f4917bd) )
	ROM_RELOAD(            0x34000, 0x4000 )
	ROM_LOAD( "bug32.158", 0x24000, 0x4000, CRC(125461f2) SHA1(9dd94344cfc7a17670d6f512ecd5947f198154c0) )
	ROM_RELOAD(            0x30000, 0x4000 )
	ROM_LOAD( "bug23.185", 0x28000, 0x4000, CRC(cafb4d4a) SHA1(aa24becdf354abca507dbd77fa18d05bea685285) )
	ROM_RELOAD(            0x3c000, 0x4000 )
	ROM_LOAD( "bug25.157", 0x2c000, 0x4000, CRC(80c4e045) SHA1(be3b537d3ed3ee74fc51059aa744dca4d63431f6) )
	ROM_RELOAD(            0x38000, 0x4000 )

	ROM_REGION( 0x8000, REGION_GFX3, 0 )	/* road */
	ROM_LOAD( "bug12.58", 0x0000, 0x2000, CRC(bd34d55c) SHA1(05a719a6eff5af3aaaa1e0ee783b18597582ed64) )
	ROM_LOAD( "bug11.57", 0x2000, 0x2000, CRC(a44d43eb) SHA1(c4d68c7e123506acaa6adc353579cac19ecb3a9d) )
	ROM_LOAD( "bb3.137",  0x4000, 0x0200, CRC(ad76f3fb) SHA1(bf96f903b32e009a2592df0f28cc3e20b039f4d4) )
	ROM_LOAD( "bb4.138",  0x4200, 0x0200, CRC(e4ca4ea0) SHA1(0c8dd6f87bddcc709de42e0c4a59be3c19c5aa8a) )
	ROM_LOAD( "bb5.139",  0x4400, 0x0200, CRC(e2577a9a) SHA1(6408f815a1357712473c54a8603137a58431781b) )
	ROM_LOAD( "bb6.94",   0x4600, 0x0200, CRC(ad43e02a) SHA1(c50a398020508f52ddf8d45881f211d17d096fa1) )

	ROM_REGION16_LE( 0x10000, REGION_USER3, 0 )	/* au data */
	ROM_LOAD16_BYTE( "bug9.170",  0x0000, 0x4000, CRC(7d84135b) SHA1(3c669c4e796e83672aceeb6de1aeea28f9f2fef0) )
	ROM_LOAD16_BYTE( "bug10.171", 0x0001, 0x4000, CRC(b518dd6f) SHA1(7cefa2f9438306c81dc83cd260928c835eb9b712) )
	ROM_LOAD16_BYTE( "bb1.245",   0x8000, 0x0200, CRC(0ddbd36d) SHA1(7a08901a350c315d46ab8d0aa881db384b9f37d2) )
	ROM_LOAD16_BYTE( "bb2.220",   0x8001, 0x0200, CRC(71d47de1) SHA1(2da9aeb3f2ebb1114631c8042a37c4f4c18e741b) )

	ROM_REGION( 0x10000, REGION_USER1, 0 )	/* obj map */
	ROM_LOAD( "bug16.210", 0x0000, 0x4000, CRC(8b64409e) SHA1(1fb4c6923e6a9e1f2a63a2c335b63e2bdc44b61f) )
	ROM_LOAD( "bug14.209", 0x4000, 0x4000, CRC(4e765282) SHA1(f7d69d39823a8b33bd0e5b1bd78a5d68a293e221) )
	ROM_LOAD( "bug17.182", 0x8000, 0x4000, CRC(a5d84df6) SHA1(4e33ef0bee383e0d47b0c679cd2a54edb7ca0e3e) )
	ROM_LOAD( "bug15.181", 0xc000, 0x4000, CRC(d519de10) SHA1(535d05e11af65be65f3d9924b0c48faf8dcfd1bf) )

	ROM_REGION( 0x6000, REGION_USER2, 0 )	/* obj luts */
	ROM_LOAD( "bug13.124", 0x0000, 0x2000, CRC(53604d7a) SHA1(bfa304cd885162ece7a5f54988d9880fc541eb3a) )
	ROM_LOAD( "bug18.156", 0x2000, 0x4000, CRC(e58321a6) SHA1(81be87d3c6046bb375c74362dc940f0269b39d1d) )

	ROM_REGION( 0x10000, REGION_PROMS, 0 )
	ROM_LOAD( "bb10.191", 0x000,  0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.192", 0x100,  0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.193", 0x200,  0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.194", 0x300,  0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	ROM_LOAD( "bb10.104", 0x000,  0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.105", 0x100,  0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.106", 0x200,  0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.107", 0x300,  0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	ROM_LOAD( "bb10.49",  0x000,  0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.50",  0x100,  0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.51",  0x200,  0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.52",  0x300,  0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	ROM_LOAD( "bb14.199", 0x400,  0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )
	ROM_LOAD( "bb14.197", 0x400,  0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )
	ROM_LOAD( "bb14.137", 0x400,  0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )

	ROM_LOAD( "bb9.271",  0x500,  0x800, CRC(6fc807d1) SHA1(3442cbb21bbedf6291a3fe1747d479445f613d26) )
	ROM_LOAD( "bb9.238",  0xd00,  0x800, CRC(6fc807d1) SHA1(3442cbb21bbedf6291a3fe1747d479445f613d26) )

	ROM_LOAD( "bb7.16",   0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )
	ROM_LOAD( "bb7.18",   0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )
	ROM_LOAD( "bb7.20",   0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )

	ROM_LOAD( "bb8.152",  0x1600, 0x100, CRC(2330ff4f) SHA1(e86eb63ce47572bcbbf325f9bb749d10d96bf2e7) )

	/* TODO: PALs */
ROM_END

ROM_START( buggyboyb )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* main CPU */
	ROM_LOAD16_BYTE( "bug1a.230", 0x20000, 0x8000, CRC(92797c25) SHA1(8f7434abbd7f557d3202abb01b1e4899c82c67a5) )
	ROM_LOAD16_BYTE( "bug4a.173", 0x20001, 0x8000, CRC(40ce3930) SHA1(4bf62ebeea1549a13a21a32cb860717f064b186a) )

	ROM_LOAD16_BYTE( "bug2b.231", 0xf0000, 0x4000, CRC(44ebf66d) SHA1(507417754e3fc2b9e03a2c477da0ae3ee49a8e11) )
	ROM_LOAD16_BYTE( "bug5b.174", 0xf0001, 0x4000, CRC(d02dd360) SHA1(00ee44ff31b5dc978248356593ca797bc291f636) )

	ROM_LOAD16_BYTE( "bug3b.232", 0xf8000, 0x4000, CRC(43cce3f0) SHA1(17de3728d809d386f6a7a330c8c8701975d4ebed) )
	ROM_LOAD16_BYTE( "bug6b.175", 0xf8001, 0x4000, CRC(8f000dfa) SHA1(5fd78a03a00f547bbb431839f78a8d10a4ba8e3e) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )	/* math CPU */
	ROM_LOAD16_BYTE( "bug8a.061", 0x04000, 0x2000, CRC(512291cd) SHA1(60f87133c86b88b982ba4680f96d0ac55970cb8d) )
	ROM_LOAD16_BYTE( "bug7a.060", 0x04001, 0x2000, CRC(d24dfdef) SHA1(37d05a8bf9567380523df01265afb9780e39ea2a) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* audio CPU */
	ROM_LOAD( "bug35.11", 0x00000, 0x4000,  CRC(7aa16e9e) SHA1(ea54e56270f70351a62a78fa32027bb41ef9861e) )

	ROM_REGION( 0x8000, REGION_GFX1, 0 )	/* char tiles */
	ROM_LOAD( "bug34a",     0x00000, 0x4000, CRC(8c64eb3c) SHA1(e0846f9a4811a0b6561d7a9aa7d50c23ced8d1d3) )
	ROM_LOAD( "bug33a.168", 0x04000, 0x4000, CRC(b500d683) SHA1(c9f92930c815a947fbd9a3846fd9580671da324f) )

	ROM_REGION( 0x40000, REGION_GFX2, 0 )	/* obj tiles */
	ROM_LOAD( "bug26.275", 0x00000, 0x4000, CRC(fc6fce64) SHA1(4cea5796c26980455bdc0f8fcdb8e719caa8041f) )
	ROM_LOAD( "bug27.245", 0x04000, 0x4000, CRC(54b9a9dd) SHA1(4f4f29a459991501013cdfffd34b50eed1cde850) )
	ROM_LOAD( "bug19.274", 0x08000, 0x4000, CRC(3392c2ef) SHA1(a4aa4f24517da16495be073aa590cda81d9fca49) )
	ROM_LOAD( "bug20.244", 0x0c000, 0x4000, CRC(9ee4c236) SHA1(51155193e470a82deb5e3f1979ce112ada39b705) )

	ROM_LOAD( "bug28.243", 0x10000, 0x4000, CRC(feef6c27) SHA1(61679d67a6ef85965078e3ddd11c178a1a55f223) )
	ROM_LOAD( "bug29.212", 0x14000, 0x4000, CRC(f570e00b) SHA1(ced4b9a4a324a4c92c08e088fa116469b2878f55) )
	ROM_LOAD( "bug21.242", 0x18000, 0x4000, CRC(088fef40) SHA1(a0c866c32857690915a33b0810219fb3cbf24f24) )
	ROM_LOAD( "bug22.211", 0x1c000, 0x4000, CRC(5ec02630) SHA1(75233de03461016d1bc7c4ece0502e16e53c3351) )

	ROM_LOAD( "bug30.186", 0x20000, 0x4000, CRC(5c2ecabf) SHA1(5be25ddc1e2aac4579a39e405f8eac919f4917bd) )
	ROM_RELOAD(            0x34000, 0x4000 )
	ROM_LOAD( "bug32.158", 0x24000, 0x4000, CRC(125461f2) SHA1(9dd94344cfc7a17670d6f512ecd5947f198154c0) )
	ROM_RELOAD(            0x30000, 0x4000 )
	ROM_LOAD( "bug23.185", 0x28000, 0x4000, CRC(cafb4d4a) SHA1(aa24becdf354abca507dbd77fa18d05bea685285) )
	ROM_RELOAD(            0x3c000, 0x4000 )
	ROM_LOAD( "bug25.157", 0x2c000, 0x4000, CRC(80c4e045) SHA1(be3b537d3ed3ee74fc51059aa744dca4d63431f6) )
	ROM_RELOAD(            0x38000, 0x4000 )

	ROM_REGION( 0x8000, REGION_GFX3, 0 )	/* road */
	ROM_LOAD( "bug12.58", 0x0000, 0x2000, CRC(bd34d55c) SHA1(05a719a6eff5af3aaaa1e0ee783b18597582ed64) )
	ROM_LOAD( "bug11.57", 0x2000, 0x2000, CRC(a44d43eb) SHA1(c4d68c7e123506acaa6adc353579cac19ecb3a9d) )
	ROM_LOAD( "bb3.137",  0x4000, 0x0200, CRC(ad76f3fb) SHA1(bf96f903b32e009a2592df0f28cc3e20b039f4d4) )
	ROM_LOAD( "bb4.138",  0x4200, 0x0200, CRC(e4ca4ea0) SHA1(0c8dd6f87bddcc709de42e0c4a59be3c19c5aa8a) )
	ROM_LOAD( "bb5.139",  0x4400, 0x0200, CRC(e2577a9a) SHA1(6408f815a1357712473c54a8603137a58431781b) )
	ROM_LOAD( "bb6.94",   0x4600, 0x0200, CRC(ad43e02a) SHA1(c50a398020508f52ddf8d45881f211d17d096fa1) )

	ROM_REGION16_LE( 0x10000, REGION_USER3, 0 )	/* au data */
	ROM_LOAD16_BYTE( "bug9.170",  0x0000, 0x4000, CRC(7d84135b) SHA1(3c669c4e796e83672aceeb6de1aeea28f9f2fef0) )
	ROM_LOAD16_BYTE( "bug10.171", 0x0001, 0x4000, CRC(b518dd6f) SHA1(7cefa2f9438306c81dc83cd260928c835eb9b712) )
	ROM_LOAD16_BYTE( "bb1.245",   0x8000, 0x0200, CRC(0ddbd36d) SHA1(7a08901a350c315d46ab8d0aa881db384b9f37d2) )
	ROM_LOAD16_BYTE( "bb2.220",   0x8001, 0x0200, CRC(71d47de1) SHA1(2da9aeb3f2ebb1114631c8042a37c4f4c18e741b) )

	ROM_REGION( 0x10000, REGION_USER1, 0 )	/* obj map */
	ROM_LOAD( "bug16.210", 0x0000, 0x4000, CRC(8b64409e) SHA1(1fb4c6923e6a9e1f2a63a2c335b63e2bdc44b61f) )
	ROM_LOAD( "bug14.209", 0x4000, 0x4000, CRC(4e765282) SHA1(f7d69d39823a8b33bd0e5b1bd78a5d68a293e221) )
	ROM_LOAD( "bug17.182", 0x8000, 0x4000, CRC(a5d84df6) SHA1(4e33ef0bee383e0d47b0c679cd2a54edb7ca0e3e) )
	ROM_LOAD( "bug15.181", 0xc000, 0x4000, CRC(d519de10) SHA1(535d05e11af65be65f3d9924b0c48faf8dcfd1bf) )

	ROM_REGION( 0x6000, REGION_USER2, 0 )	/* obj luts */
	ROM_LOAD( "bug13.124", 0x0000, 0x2000, CRC(53604d7a) SHA1(bfa304cd885162ece7a5f54988d9880fc541eb3a) )
	ROM_LOAD( "bug18.156", 0x2000, 0x4000, CRC(e58321a6) SHA1(81be87d3c6046bb375c74362dc940f0269b39d1d) )

	ROM_REGION( 0x10000, REGION_PROMS, 0 )
	ROM_LOAD( "bb10.191", 0x000,  0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.192", 0x100,  0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.193", 0x200,  0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.194", 0x300,  0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	ROM_LOAD( "bb10.104", 0x000,  0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.105", 0x100,  0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.106", 0x200,  0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.107", 0x300,  0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	ROM_LOAD( "bb10.49",  0x000,  0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.50",  0x100,  0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.51",  0x200,  0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.52",  0x300,  0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	ROM_LOAD( "bb14.199", 0x400,  0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )
	ROM_LOAD( "bb14.197", 0x400,  0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )
	ROM_LOAD( "bb14.137", 0x400,  0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )

	ROM_LOAD( "bb9.271",  0x500,  0x800, CRC(6fc807d1) SHA1(3442cbb21bbedf6291a3fe1747d479445f613d26) )
	ROM_LOAD( "bb9.238",  0xd00,  0x800, CRC(6fc807d1) SHA1(3442cbb21bbedf6291a3fe1747d479445f613d26) )

	ROM_LOAD( "bb7.16",   0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )
	ROM_LOAD( "bb7.18",   0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )
	ROM_LOAD( "bb7.20",   0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )

	ROM_LOAD( "bb8.152",  0x1600, 0x100, CRC(2330ff4f) SHA1(e86eb63ce47572bcbbf325f9bb749d10d96bf2e7) )

	/* TODO: PALs */
ROM_END

/*
	The game is comprised of three boards:
	- Sound Board (labeled TC043-1, top small board)
	- CPU Board (labeled TC041, middle board, uses 15.000 MHz xtal)
	- Video Board (labeled TC042, bottom board, uses 18.000 MHz xtal)
*/
ROM_START( buggyboyjr )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )	/* main CPU */
	ROM_LOAD16_BYTE( "bug1a.214", 0x20000, 0x8000, CRC(92797c25) SHA1(8f7434abbd7f557d3202abb01b1e4899c82c67a5) )
	ROM_LOAD16_BYTE( "bug4a.175", 0x20001, 0x8000, CRC(40ce3930) SHA1(4bf62ebeea1549a13a21a32cb860717f064b186a) )

	ROM_LOAD16_BYTE( "bug2s.213", 0xf0000, 0x8000, CRC(abcc8ad2) SHA1(aeb695c3675d40a951fe8272cbf0f6673435dab8) )
	ROM_LOAD16_BYTE( "bug5s.174", 0xf0001, 0x8000, CRC(5e352d8d) SHA1(350c206b5241d5628e673ce1108f728c8c4f980c) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 )	/* math CPU */
	ROM_LOAD16_BYTE( "bug8s.26", 0x04000, 0x2000, CRC(efd66282) SHA1(8355422c0732c92951659930eb399129fe8d6230) )
	ROM_LOAD16_BYTE( "bug7s.25", 0x04001, 0x2000, CRC(bd75b5eb) SHA1(f2b55f84f4c968df177a56103924ac64705285cd) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* audio CPU */
	ROM_LOAD( "bug35s.21", 0x00000, 0x4000, CRC(65d9af57) SHA1(17b09404942d17e7254550c43b56ae96a8c55680) )

	ROM_REGION( 0x8000, REGION_GFX1, 0 )	/* char tiles */
	ROM_LOAD( "bug34s.46", 0x00000, 0x4000, CRC(8ea8fec4) SHA1(75e67c9a59a86fcdedf2a70fafd303baa552aa18) )
	ROM_LOAD( "bug33s.47", 0x04000, 0x4000, CRC(459c2b03) SHA1(ff62a86195042a349fbe799c638cf590fe9572bb) )

	ROM_REGION( 0x40000, REGION_GFX2, 0 )	/* obj tiles */
	ROM_LOAD( "bug26s.147", 0x00000, 0x8000, CRC(14033710) SHA1(e05afeb557ce14055fa8b4f6d8805307feaa1660) )
	ROM_LOAD( "bug19s.144", 0x08000, 0x8000, CRC(838e0697) SHA1(0e9aff2c4065d79350ddb55edff57a899c33ef1c) )
	ROM_LOAD( "bug28s.146", 0x10000, 0x8000, CRC(8b47d227) SHA1(a3e57594ad0085e8b1bd327c580eb36237f3e3d2) )
	ROM_LOAD( "bug21s.143", 0x18000, 0x8000, CRC(876a5666) SHA1(db485cdf35f63c080c919ee86374f63e577092c3) )
	ROM_LOAD( "bug30s.145", 0x20000, 0x8000, CRC(11d8e2a8) SHA1(9bf198229a12d331e8e7352b7ee3f39f6891f517) )
	ROM_LOAD( "bug23s.142", 0x28000, 0x8000, CRC(015db5d8) SHA1(39ef8b44f2eb9399fb1555cffa6763e06d59c181) )

	ROM_REGION( 0x8000, REGION_GFX3, 0 )	/* road */
	ROM_LOAD( "bug11s.225",0x0000, 0x4000, CRC(771af4e1) SHA1(a42b164dd0567c78c0d308ee48d63e5a284897bb) )
	ROM_LOAD( "bb3s.195",  0x4000, 0x0200, CRC(2ab3d5ff) SHA1(9f8359cb4ba2e7d15dbb9dc21cd71c0902cd2153) )
	ROM_LOAD( "bb4s.193",  0x4200, 0x0200, CRC(630f68a4) SHA1(d730f050353c688f81d090e33e00cd35e7b7b6fa) )
	ROM_LOAD( "bb5s.194",  0x4400, 0x0200, CRC(65925c9e) SHA1(d1ff1cb9f83c09e52a96632945e4edfedc335fd4) )
	ROM_LOAD( "bb6.224",   0x4600, 0x0200, CRC(ad43e02a) SHA1(c50a398020508f52ddf8d45881f211d17d096fa1) )

	ROM_REGION16_LE( 0x10000, REGION_USER3, 0 )	/* au data */
	ROM_LOAD16_BYTE( "bug9.138", 0x0000, 0x4000, CRC(7d84135b) SHA1(3c669c4e796e83672aceeb6de1aeea28f9f2fef0) )
	ROM_LOAD16_BYTE( "bug10.95", 0x0001, 0x4000, CRC(b518dd6f) SHA1(7cefa2f9438306c81dc83cd260928c835eb9b712) )
	ROM_LOAD16_BYTE( "bb1.163",  0x8000, 0x0200, CRC(0ddbd36d) SHA1(7a08901a350c315d46ab8d0aa881db384b9f37d2) )
	ROM_LOAD16_BYTE( "bb2.162",  0x8001, 0x0200, CRC(71d47de1) SHA1(2da9aeb3f2ebb1114631c8042a37c4f4c18e741b) )

	ROM_REGION( 0x10000, REGION_USER1, 0 )	/* obj map */
	ROM_LOAD( "bug16s.139", 0x0000, 0x8000, CRC(1903a9ad) SHA1(526c404c15e3f04b4afb27dee66e9deb0a6b9704) )
	ROM_LOAD( "bug17s.140", 0x8000, 0x8000, CRC(82cabdd4) SHA1(94324fcf83c373621fc40553473ae3cb552ab704) )

	ROM_REGION( 0x6000, REGION_USER2, 0 )	/* obj luts */
	ROM_LOAD( "bug13.32",   0x0000, 0x2000, CRC(53604d7a) SHA1(bfa304cd885162ece7a5f54988d9880fc541eb3a) )
	ROM_LOAD( "bug18s.141", 0x2000, 0x4000, CRC(67786327) SHA1(32cc1f5bc654497c968ddcd4af29720c6d659482) )

	ROM_REGION( 0x10000, REGION_PROMS, 0 )
	/* RGBI */
	ROM_LOAD( "bb10.41", 0x000, 0x100, CRC(f2368398) SHA1(53f28dba11bb494d033bb279abf138975c84b20d) )
	ROM_LOAD( "bb11.40", 0x100, 0x100, CRC(bf77f624) SHA1(b042d293d2094dbabb32d628fd9addd832f084ef) )
	ROM_LOAD( "bb12.39", 0x200, 0x100, CRC(10a2e8d1) SHA1(51a8c51ecbbb7bd04ae46fb5598d2c8de8097581) )
	ROM_LOAD( "bb13.42", 0x300, 0x100, CRC(40d10dfa) SHA1(e40b4c424827937fec6df1a27b19b8dc09d3274a) )

	/* Character colour LUT */
	ROM_LOAD( "bb14.19", 0x400, 0x100, CRC(0b821e0b) SHA1(b9401b9364fb99e15f562df91dcfdec1b989af2d) )

	/* Object colour LUTs (odd and even pixels) */
	ROM_LOAD( "bb9.190", 0x500, 0x800, CRC(6fc807d1) SHA1(3442cbb21bbedf6291a3fe1747d479445f613d26) )
	ROM_LOAD( "bb9.162", 0xd00, 0x800, CRC(6fc807d1) SHA1(3442cbb21bbedf6291a3fe1747d479445f613d26) )

	/* Object tile LUT */
	ROM_LOAD( "bb8.31", 0x1600, 0x100, CRC(2330ff4f) SHA1(e86eb63ce47572bcbbf325f9bb749d10d96bf2e7) )

	/* Road */
	ROM_LOAD( "bb7.188", 0x1500, 0x100, CRC(b57b609f) SHA1(2dea375437c62cb4c64b21d5e6ddc09397b6ab35) )

	/* PALs (sound, video and CPU boards): not dumped */
ROM_END


/*************************************
 *
 *  Game driver(s)
 *
 *************************************/

GAMEX( 1983, tx1,        0,        tx1,      tx1,      tx1,      ROT0, "Tatsumi (Atari/Namco/Taito license)", "TX-1 (World)",        GAME_IMPERFECT_SOUND )
GAMEX( 1983, tx1jb,      tx1,      tx1,      tx1,      tx1,      ROT0, "Tatsumi",                             "TX-1 (Japan rev. B)", GAME_IMPERFECT_SOUND )
GAMEX( 1983, tx1jc,      tx1,      tx1,      tx1,      tx1,      ROT0, "Tatsumi",                             "TX-1 (Japan rev. C)", GAME_IMPERFECT_SOUND )
GAME(  1985, buggyboy,   0,        buggyboy, buggyboy, buggyboy, ROT0, "Tatsumi",                             "Buggy Boy/Speed Buggy (cockpit, rev. D)" )
GAME(  1986, buggyboyb,  buggyboy, buggyboy, buggyboy, buggyboy, ROT0, "Tatsumi",                             "Buggy Boy/Speed Buggy (cockpit, rev. B)" )
GAME(  1986, buggyboyjr, buggyboy, buggybjr, buggybjr, buggyboy, ROT0, "Tatsumi",                             "Buggy Boy Junior/Speed Buggy (upright)" )
