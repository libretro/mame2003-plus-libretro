/***************************************************************************

	Puckman Pockimon and the other games on the Genie / Y-BOX hardware,
	as in current MAME (megadriv_ybox.cpp). Moved here from drivers/segac2.c.

	A Mega Drive with no real Sega parts (as MAME, emulated as a Mega Drive:
	68000, Z80, YM3438, the PSG and the 315-5313 VDP of vidhrdw/segavdp.c)
	plus an OKI M6295. The inputs and DIP switches are at 0x700010, the
	pads are not used.

	Supported:
		Puckman Pockimon Genie 2000 (and its bootleg, the old puckpkmn set)
		Jingling Jiazu Genie 2000
		Juezhan Tianhuang
		Songjiangyanyi Final
		Shuihu Feng Yun Zhuan

	The ROM data lines are scrambled (DRIVER_INIT( puckpkmn )).

	Songjiangyanyi and Shuihu Feng Yun Zhuan read back values they write at
	0x400000 (two latches), and Shuihu Feng Yun Zhuan remaps the ROM while
	a value is written at 0x600000: it calls a routine with some address
	lines changed (see sj_rom_ovl_w).

	Not included (no ROMs): xmvsfmb, the X-Men vs. Street Fighter bootleg of
	MAME on the same board (not working there).

***************************************************************************/

/***************************************************************************
Puckman Pokemon Genie 2000
(c) 2000?  Manufacturer ?

Hardware looks bootleg-ish, but is newly manufactured.

CPU: ? (one of the SMD chips)
SND: OKI6295, U6612 (probably YM3812), U6614B (Probably YM3014B)
XTAL: 3.579545MHz, 4.0000MHz
OSC: 53.693175MHz
Other Chips: Possible CPU: TA-06SD 9933 B816453 128 pin square SMD
             GFX support chips: Y-BOX TA891945 100 pin SMD
                                TV16B 0010 ME251271 160 pin SMD

There are 13 PAL's on the PCB !

RAM: 62256 x 3, D41264 x 2 (ZIP Ram)
DIPS: 2 x 8 position
SW1:
               			1	2	3	4	5	6	7	8
coins   1coin 1 Cred.  	off	off	off
		2c 1c			on	off	off
		3c 1c			off	on	off
		4c 1c			on	on	off
		5c 1c			off	off	on
		1c 2c			on	off	on
		1c 3c			off	on	on
		1c 4c			on	on	on

players	1							off	off	off
 		2							on	off	off
		3							off	on	off
		4							on	on	off
		5							off	off	on
		6							on	off	on
		7							off	on	on
		8							on	on	on

diffic-
ulty	v.easy									off	off
		normal									on	off
		diffic.									off	on
		v. diffic.								on	on


SW2

note position 3-8 not used

               		1	2	3	4	5	6	7	8
test mode	no		off
			yes		on

demo sound	yes			off
			no			on


ROMS:
PUCKPOKE.U3	M5M27C201	Sound
PUCKPOKE.U4	27C040--\
PUCKPOKE.U5	27C040---\
PUCKPOKE.U7	27C040----- Main program & GFX
PUCKPOKE.U8	27C4001---/

ROM sockets U63 & U64 empty

Hi-res PCB scan available on request.
Screenshots available on my site at http://unemulated.emuunlim.com (under PCB Shop Raid #2)


****************************************************************************/

#include "driver.h"
#include "state.h"
#include "megadriv.h"

/******************************************************************************
	The Mega Drive part (as MAME's md_ntsc: 68000, Z80, YM2612, PSG, VDP)
*******************************************************************************

	The board is a Mega Drive: the 68000 drives the YM3438 at 0xa04000
	through the Z80 bus (genesis_68k_to_z80_r/w), and songjang and shuifeng
	run a Z80 program. The pads are not used (ybox_io_r): the inputs and
	DIP switches are at 0x700010. Version register: Japan (init_megadrij).

******************************************************************************/

static READ16_HANDLER( ybox_io_r )
{
	switch (offset)
	{
		case 0x00:
			return 0x2020;

		case 0x01:
		case 0x02:
		{
			/* no pad: the lines read high, TH as written */
			int ctrl = genesis_io_ram[0x03 + offset];
			int data = 0x3f | (genesis_io_ram[offset] & 0x40);
			data = (data & ((ctrl ^ 0xff) & 0x7f)) | (genesis_io_ram[offset] & (ctrl | 0x80));
			return data | (data << 8);
		}
	}
	return genesis_68000_io_r(offset, mem_mask);
}

/* the main RAM is mirrored in 0xe00000-0xffffff */
static READ16_HANDLER( ybox_ram_r )
{
	return genesis_68k_ram[offset & 0x7fff];
}

static WRITE16_HANDLER( ybox_ram_w )
{
	COMBINE_DATA(&genesis_68k_ram[offset & 0x7fff]);
}

static struct OKIM6295interface ybox_m6295_intf =
{
	1,
	{ 8000 },	/* 1MHz / 132 */
	{ REGION_SOUND1 },
	{ 50 }
};

/* the base map of the board (MAME's puckpkmn_base_map) */
#define YBOX_READ_BASE \
	{ 0x000000, 0x3fffff, MRA16_ROM },					/* Main 68k Program Roms */ \
	{ 0x700010, 0x700011, input_port_0_word_r },		/* Input (P2) */ \
	{ 0x700012, 0x700013, input_port_1_word_r },		/* Input (P1) */ \
	{ 0x700014, 0x700015, input_port_2_word_r },		/* Input (?) */ \
	{ 0x700016, 0x700017, input_port_3_word_r },		/* Input (DSW1) */ \
	{ 0x700018, 0x700019, input_port_4_word_r },		/* Input (DSW2) */ \
	{ 0x700022, 0x700023, OKIM6295_status_0_lsb_r },	/* M6295 */ \
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },		/* Z80 bus: RAM, YM3438 */ \
	{ 0xa10000, 0xa1001f, ybox_io_r },					/* Mega Drive I/O (no pads) */ \
	{ 0xa11000, 0xa11203, genesis_ctrl_r },				/* Z80 bus request / reset */ \
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */ \
	{ 0xe00000, 0xfeffff, ybox_ram_r },					/* Main Ram mirrors */ \
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */

#define YBOX_WRITE_BASE \
	{ 0x000000, 0x3fffff, MWA16_ROM },					/* Main 68k Program Roms */ \
	{ 0x700022, 0x700023, OKIM6295_data_0_lsb_w },		/* M6295 */ \
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },		/* Z80 bus: RAM, YM3438 */ \
	{ 0xa10000, 0xa1001f, genesis_68000_io_w, &genesis_io_ram },	/* Mega Drive I/O */ \
	{ 0xa11000, 0xa11203, genesis_ctrl_w },				/* Z80 bus request / reset */ \
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */ \
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */ \
	{ 0xe00000, 0xfeffff, ybox_ram_w },					/* Main Ram mirrors */ \
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },	/* Main Ram */


/******************************************************************************
	Puckman Pockimon, Jingling Jiazu
******************************************************************************/

/* the ID the game checks at boot */
static READ16_HANDLER( puckpkmn_4b2476_r ) { return 0x3100; }
static READ16_HANDLER( jingling_4b2476_r ) { return 0x3400; }
static READ16_HANDLER( ybox_70001c_0e_r ) { return 0x000e; }

/* the bootleg set: the unknown accesses of MAME's puckpkmnb_map (the Z80
   is held in reset, its RAM is not written) */
#define PUCKPKMNB_READ \
	{ 0xa10002, 0xa10005, MRA16_NOP },					/* ? alternative way of reading inputs ? */ \
	{ 0xa11100, 0xa11101, MRA16_NOP },					/* ? */

static MEMORY_READ16_START( puckpkmnb_readmem )
	PUCKPKMNB_READ
	YBOX_READ_BASE
MEMORY_END

static MEMORY_WRITE16_START( puckpkmnb_writemem )
	{ 0xa00000, 0xa00551, MWA16_NOP },					/* ? */
	{ 0xa10002, 0xa10005, MWA16_NOP },					/* ? alternative way of reading inputs ? */
	{ 0xa11100, 0xa11101, MWA16_NOP },					/* ? */
	{ 0xa11200, 0xa11201, MWA16_NOP },					/* ? */
	YBOX_WRITE_BASE
MEMORY_END

static MEMORY_READ16_START( puckpkmn_readmem )
	{ 0x4b2476, 0x4b2477, puckpkmn_4b2476_r },
	{ 0x70001c, 0x70001d, ybox_70001c_0e_r },
	PUCKPKMNB_READ
	YBOX_READ_BASE
MEMORY_END

static MEMORY_READ16_START( jingling_readmem )
	{ 0x4b2476, 0x4b2477, jingling_4b2476_r },
	{ 0x70001c, 0x70001d, ybox_70001c_0e_r },
	PUCKPKMNB_READ
	YBOX_READ_BASE
MEMORY_END


/******************************************************************************
	Juezhan Tianhuang
******************************************************************************/

/* protection: the value is read from 0x710000 after a series of writes and
   stored at 0xff0007; without it the game erases the VDP address */
static int jzth_protcount;

static WRITE16_HANDLER( jzth_710000_w )
{
	jzth_protcount++;
}

static READ16_HANDLER( jzth_710000_r )
{
	UINT16 ret;

	switch (jzth_protcount)
	{
		case 4:  ret = 0x4; break;
		case 5:  ret = 0x5; break;
		case 6:  ret = 0xe; break;
		default: ret = 0xf; break;
	}

	jzth_protcount = 0;
	return ret;
}

static MEMORY_READ16_START( jzth_readmem )
	{ 0x710000, 0x710001, jzth_710000_r },				/* protection */
	YBOX_READ_BASE
MEMORY_END

static MEMORY_WRITE16_START( jzth_writemem )
	{ 0x710000, 0x710001, jzth_710000_w },				/* protection */
	YBOX_WRITE_BASE
MEMORY_END


/******************************************************************************
	Songjiangyanyi, Shuihu Feng Yun Zhuan
*******************************************************************************

	Protection read back, as in current MAME: the games only use byte
	accesses on D8-D15, writing with A1 = 1 and reading with A1 = 0; A2
	selects one of two latches, which is consistent with every write / read
	back pair the games use.

	ROM address remapping: shuifeng (and xmvsfmb in MAME) copies a routine
	to RAM, masks the interrupts, writes a byte to 0x6xxxxx, jsrs into ROM
	and writes 0 to 0x6xxxxx. The code it needs is elsewhere in ROM, at the
	call target with some address lines changed; it is modelled as an
	address XOR per value (the only one the game uses):
		shuifeng 0xfc: 0x25000 -> 0x65000
	While a value is latched the ROM is read through sj_rom_ovl_r and the
	opcodes are fetched through sj_opbase (OP_ROM moved by the XOR, a 4K page
	at a time).

******************************************************************************/

static UINT8 sj_prot_latch[2];
static UINT8 sj_rom_ovl;
static UINT32 sj_rom_ovl_xor;
static UINT16 sj_70001c;				/* the value the board returns at 0x70001c */
static UINT32 (*sj_ovl_xor)(UINT8 value);

static UINT32 sj_no_ovl_xor(UINT8 value) { return 0; }

static UINT32 shuifeng_ovl_xor(UINT8 value)
{
	switch (value)
	{
		case 0xfc: return 0x40000;	/* 0x12b2 -> jsr 0x25000 */
		default:   return 0;
	}
}

static READ16_HANDLER( sj_prot_r )
{
	return sj_prot_latch[(offset >> 1) & 1] << 8;
}

static WRITE16_HANDLER( sj_prot_w )
{
	if (ACCESSING_MSB)
		sj_prot_latch[(offset >> 1) & 1] = data >> 8;
}

static READ16_HANDLER( sj_rom_ovl_r )
{
	const UINT16 *rom = (const UINT16 *)memory_region(REGION_CPU1);
	UINT32 mask = memory_region_length(REGION_CPU1) / 2 - 1;

	return rom[(offset ^ (sj_rom_ovl_xor >> 1)) & mask];
}

static OPBASE_HANDLER( sj_opbase )
{
	UINT8 *rom = memory_region(REGION_CPU1);

	if (!sj_rom_ovl_xor || address > 0x3fffff)
		return address;

	/* the ROM seen with the address lines changed, a 4K page at a time (the
	   XOR value only changes bits above it); every branch comes back here */
	OP_ROM = OP_RAM = rom + ((address ^ sj_rom_ovl_xor) & ~0xfff) - (address & ~0xfff);
	OP_MEM_MIN = address & ~0xfff;
	OP_MEM_MAX = address | 0xfff;
	catch_nextBranch();
	return ~0;
}

static void sj_rom_ovl_select(void)
{
	if (sj_rom_ovl_xor)
		install_mem_read16_handler(0, 0x000000, 0x3fffff, sj_rom_ovl_r);
	else
		install_mem_read16_handler(0, 0x000000, 0x3fffff, MRA16_ROM);
}

static WRITE16_HANDLER( sj_rom_ovl_w )
{
	if (ACCESSING_MSB)
	{
		UINT32 old = sj_rom_ovl_xor;

		sj_rom_ovl = data >> 8;
		sj_rom_ovl_xor = sj_rom_ovl ? (*sj_ovl_xor)(sj_rom_ovl) : 0;
		if (sj_rom_ovl_xor != old)
		{
			sj_rom_ovl_select();
			catch_nextBranch();
		}
	}
}

static READ16_HANDLER( sj_70001c_r )
{
	return sj_70001c;
}

static void sj_postload(void)
{
	sj_rom_ovl_select();
}

static MEMORY_READ16_START( songjang_readmem )
	{ 0x400000, 0x4fffff, sj_prot_r },					/* protection read back */
	{ 0x70001c, 0x70001d, sj_70001c_r },
	YBOX_READ_BASE										/* the ROM: see sj_rom_ovl_w */
MEMORY_END

static MEMORY_WRITE16_START( songjang_writemem )
	{ 0x01c000, 0x01cfff, MWA16_NOP },					/* writes to the ROM area, buggy code? */
	{ 0x400000, 0x4fffff, sj_prot_w },					/* protection read back */
	{ 0x600000, 0x6fffff, sj_rom_ovl_w },				/* ROM address remapping */
	{ 0x70001a, 0x70001d, MWA16_NOP },					/* written once at boot */
	YBOX_WRITE_BASE
MEMORY_END


/******************************************************************************
	Input ports
******************************************************************************/

#define YBOX_PLAYER(player, start, button) \
	PORT_BIT(  0x04, IP_ACTIVE_LOW, start ) \
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | player ) \
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | player ) \
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | player ) \
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | player ) \
	PORT_BIT(  0x80, IP_ACTIVE_LOW, button | player )

#define YBOX_DSW1 \
	PORT_START	/* DSW 1 ($700017.b) */ \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 5C_1C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) ) \
	PORT_DIPNAME( 0x38, 0x28, DEF_STR( Lives ) ) \
	PORT_DIPSETTING(    0x38, "1" ) \
	PORT_DIPSETTING(    0x30, "2" ) \
	PORT_DIPSETTING(    0x28, "3" ) \
	PORT_DIPSETTING(    0x20, "4" ) \
	PORT_DIPSETTING(    0x18, "5" ) \
	PORT_DIPSETTING(    0x10, "6" ) \
	PORT_DIPSETTING(    0x08, "7" ) \
	PORT_DIPSETTING(    0x00, "8" ) \
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) ) \
	PORT_DIPSETTING(    0xc0, "Easy"    ) \
	PORT_DIPSETTING(    0x80, "Normal"  ) \
	PORT_DIPSETTING(    0x40, "Hard"    ) \
	PORT_DIPSETTING(    0x00, "Hardest" )

#define YBOX_DIP_UNUSED(mask) \
	PORT_DIPNAME( mask, mask, DEF_STR( Unused ) ) \
	PORT_DIPSETTING(    mask, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

#define YBOX_DSW2(demo_sounds) \
	PORT_START	/* DSW 2 ($700019.b) */ \
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW ) \
	demo_sounds \
	YBOX_DIP_UNUSED( 0x04 ) \
	YBOX_DIP_UNUSED( 0x08 ) \
	YBOX_DIP_UNUSED( 0x10 ) \
	YBOX_DIP_UNUSED( 0x20 ) \
	YBOX_DIP_UNUSED( 0x40 ) \
	YBOX_DIP_UNUSED( 0x80 )

#define YBOX_DEMO_SOUNDS \
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )

INPUT_PORTS_START( puckpkmn ) /* Puckman Pockimon Input Ports */
	PORT_START	/* Player 2 Controls ($700011.b) */
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	YBOX_PLAYER( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 )

	PORT_START	/* Player 1 Controls ($700013.b) */
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN1, 10 )
	YBOX_PLAYER( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 )

	PORT_START	/* ? ($700015.b) */

	YBOX_DSW1
	YBOX_DSW2( YBOX_DEMO_SOUNDS )
INPUT_PORTS_END

/* the 2nd and 3rd buttons of each player at $700015.b: P2 in bits 0-1 and
   P1 in bits 4-5 (jzth, shuifeng), or the other way round */
#define YBOX_BUTTONS_12(lo_player, hi_player) \
	PORT_START	/* $700015.b */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | lo_player ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | lo_player ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | hi_player ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | hi_player ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define YBOX_PLAYERS_3B \
	PORT_START	/* Player 2 Controls ($700011.b) */ \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	YBOX_PLAYER( IPF_PLAYER2, IPT_START2, IPT_BUTTON3 ) \
 \
	PORT_START	/* Player 1 Controls ($700013.b) */ \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN1, 10 ) \
	YBOX_PLAYER( IPF_PLAYER1, IPT_START1, IPT_BUTTON3 )

INPUT_PORTS_START( jzth )
	YBOX_PLAYERS_3B
	YBOX_BUTTONS_12( IPF_PLAYER2, IPF_PLAYER1 )
	YBOX_DSW1
	YBOX_DSW2( YBOX_DEMO_SOUNDS )
INPUT_PORTS_END

INPUT_PORTS_START( songjang )
	YBOX_PLAYERS_3B
	YBOX_BUTTONS_12( IPF_PLAYER1, IPF_PLAYER2 )
	YBOX_DSW1
	YBOX_DSW2( YBOX_DIP_UNUSED( 0x02 ) )	/* no Demo Sounds dip on this one */
INPUT_PORTS_END


/******************************************************************************
	Machine drivers
******************************************************************************/

/* V-INT / H-INT through the VDP interrupt enables and the HBLANK status
   bit over the whole blanking, as on the real hardware (songjang waits on
   HBLANK to write the scroll of each line) */
static MACHINE_INIT( ybox )
{
	machine_init_genesis();
	genesis_enable_iack();
}

static MACHINE_DRIVER_START( puckpkmnb )

	/* basic machine hardware: a Mega Drive */
	MDRV_IMPORT_FROM( genesis )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(puckpkmnb_readmem, puckpkmnb_writemem)
	MDRV_MACHINE_INIT(ybox)

	/* sound hardware */
	MDRV_SOUND_ADD(OKIM6295, ybox_m6295_intf)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( puckpkmn )
	MDRV_IMPORT_FROM( puckpkmnb )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(puckpkmn_readmem, puckpkmnb_writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( jingling )
	MDRV_IMPORT_FROM( puckpkmnb )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(jingling_readmem, puckpkmnb_writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( jzth )
	MDRV_IMPORT_FROM( puckpkmnb )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(jzth_readmem, jzth_writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( songjang )
	MDRV_IMPORT_FROM( puckpkmnb )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(songjang_readmem, songjang_writemem)
MACHINE_DRIVER_END


/******************************************************************************
	ROM definitions
******************************************************************************/

/* Puckman Pockimon (c)2000 Sun Mixing */
ROM_START( puckpkmn )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "200061.u5", 0x000000, 0x080000, CRC(502a5093) SHA1(6dc1c79d52ebb653cb2e4388f74fd975ec323566) )
	ROM_LOAD16_BYTE( "200060.u4", 0x000001, 0x080000, CRC(5f160c18) SHA1(5a5ce1b9a81afe836e435e9d6f16cf57b63cbd31) )
	ROM_LOAD16_BYTE( "200063.u8", 0x100000, 0x080000, CRC(0c29781e) SHA1(db442f9b588608b2ac04d65fd830103296599a6a) )
	ROM_LOAD16_BYTE( "200062.u7", 0x100001, 0x080000, CRC(00bbf9a9) SHA1(924c1ed85090c497ce89528082c15d1548a854a0) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )
	ROM_LOAD( "206295.u3", 0x00000, 0x40000, CRC(7b066bac) SHA1(429616e21c672b07e0705bc63234249cac3af56f) )
ROM_END

ROM_START( jingling )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "b2.u59", 0x000000, 0x080000, CRC(3fbea2c7) SHA1(89f3770ae92c62714f0795ddd2f311a9532eb25a) ) /* FIRST AND SECOND HALF IDENTICAL */
	ROM_IGNORE( 0x080000 )
	ROM_LOAD16_BYTE( "b1.u59", 0x000001, 0x080000, CRC(dc7b4254) SHA1(8ba5c5e8123e62e9af091971d0d0401d2df49350) ) /* FIRST AND SECOND HALF IDENTICAL */
	ROM_IGNORE( 0x080000 )
	ROM_LOAD16_BYTE( "b4.u59", 0x100000, 0x080000, CRC(375c9f80) SHA1(9b0eb729e95c22355e4117eec596f90e10282492) ) /* FIRST AND SECOND HALF IDENTICAL */
	ROM_IGNORE( 0x080000 )
	ROM_LOAD16_BYTE( "b3.u59", 0x100001, 0x080000, CRC(d5487df6) SHA1(d1d3d717e184a4e8e067665bbbe94e7cf45db478) ) /* FIRST AND SECOND HALF IDENTICAL */
	ROM_IGNORE( 0x080000 )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 )
	ROM_LOAD( "a.u3", 0x00000, 0x80000, CRC(77891c9b) SHA1(66f28b418a480a89ddb3fae3a7c2fe702c62364c) )
ROM_END

/* the old puckpkmn set */
ROM_START( puckpkmnb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "puckpoke.u5", 0x000000, 0x080000, CRC(fd334b91) SHA1(cf8bf6645a4082ea4392937e169b1686c9c7e246) )
	ROM_LOAD16_BYTE( "puckpoke.u4", 0x000001, 0x080000, CRC(839cc76b) SHA1(e15662a7175db7a8e222dda176a8ed92e0d56e9d) )
	ROM_LOAD16_BYTE( "puckpoke.u8", 0x100000, 0x080000, CRC(7936bec8) SHA1(4b350105abe514fbfeabae1c6f3aeee695c3d07a) )
	ROM_LOAD16_BYTE( "puckpoke.u7", 0x100001, 0x080000, CRC(96b66bdf) SHA1(3cc2861ad9bc232cbe683e01b58090f832d03db5) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )
	ROM_LOAD( "puckpoke.u3", 0x00000, 0x40000, CRC(7b066bac) SHA1(429616e21c672b07e0705bc63234249cac3af56f) )
ROM_END

ROM_START( jzth )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "s.y.u5", 0x000000, 0x080000, CRC(a4a526b5) SHA1(85d0299caf91ff50b6870f845b9aacbd358ed81f) )
	ROM_LOAD16_BYTE( "s.y.u4", 0x000001, 0x080000, CRC(c16654eb) SHA1(dca4b772a3b9caa7be3fa01511c401b591c2e6f3) )
	ROM_LOAD16_BYTE( "s.y.u8", 0x100000, 0x080000, CRC(b62e1068) SHA1(2484ae49a4a2a2c551b3b84bbc0b4e40e5d281e7) )
	ROM_LOAD16_BYTE( "s.y.u7", 0x100001, 0x080000, CRC(27fe424c) SHA1(14bee8c16aac3d5b04123c994167531f817634fd) )
	ROM_LOAD16_BYTE( "s.y.u64", 0x200000, 0x080000, CRC(62f52886) SHA1(07fc9765274c03eff4a09f48a0b1b2b2afc6078e) )
	ROM_LOAD16_BYTE( "s.y.u63", 0x200001, 0x080000, CRC(a6a32c8c) SHA1(d0c779751e4af459e9bf63e55c5e2b19a243b70d) )
	ROM_LOAD16_BYTE( "s.y.u66", 0x300000, 0x080000, CRC(fa4a09f5) SHA1(67d77c91a994ecb8b29e7661c3a12e84a64eb837))
	ROM_LOAD16_BYTE( "s.y.u65", 0x300001, 0x080000, CRC(de64e526) SHA1(e3b3e5c95b8ae36c0c57f8c9a6f55084464c4c05) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* there are 2 banks in here, so find bank switch */
	ROM_LOAD( "s.y.u3", 0x00000, 0x40000, CRC(38eef2f2) SHA1(2f750dbf71fea0622e8493f0a8be7c43555ed5cf) )
	ROM_CONTINUE( 0x40000, 0x40000 )
ROM_END

ROM_START( songjang )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "rom.u5", 0x000000, 0x080000, CRC(3de4aea5) SHA1(a4fd0799a6fd86ba3f0c4b4277190f4c332e48d6) )
	ROM_LOAD16_BYTE( "rom.u4", 0x000001, 0x080000, CRC(dc6cfc29) SHA1(e15d6e2187befbbd791c5046cb6d156801095203) )
	ROM_LOAD16_BYTE( "rom.u8", 0x100000, 0x080000, CRC(d4d2b262) SHA1(2023128b187a8cd1a3f021861da559b5ad48e984) )
	ROM_LOAD16_BYTE( "rom.u7", 0x100001, 0x080000, CRC(1e24f2c9) SHA1(562249b08a8fe317a606c16ce07a8009a0c5418a) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* only one bank (2nd half is blank) */
	ROM_LOAD( "rom.u3", 0x00000, 0x80000, CRC(44287ab4) SHA1(42de2010e73c036a2a0e01c9dd3c1c8aef1a54a1) )
ROM_END

ROM_START( shuifeng )
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "shuihufengyunzhuan.u5", 0x000000, 0x080000, CRC(73208a1a) SHA1(1eff253fe48f9e553a238c4cfd41cc5e5839393f) )
	ROM_LOAD16_BYTE( "shuihufengyunzhuan.u4", 0x000001, 0x080000, CRC(e633c02c) SHA1(7ff1091a5b2e7d5e0e616509854fa0058d3cdcf7) )
	ROM_LOAD16_BYTE( "shuihufengyunzhuan.u8", 0x100000, 0x080000, CRC(31814eb0) SHA1(3a6b13931d02a0c3b8bd97dc51ac5771a97bd092) )
	ROM_LOAD16_BYTE( "shuihufengyunzhuan.u7", 0x100001, 0x080000, CRC(51caef9f) SHA1(252dbb29ae878aaf28cb4586e02d823296eaf378) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* only one bank (2nd half is blank) */
	ROM_LOAD( "shuihufengyunzhuan-oki.u3", 0x00000, 0x80000, CRC(44287ab4) SHA1(42de2010e73c036a2a0e01c9dd3c1c8aef1a54a1) )
ROM_END



/******************************************************************************
	Driver inits
******************************************************************************/

static DRIVER_INIT( puckpkmn )
{
	data8_t *rom = memory_region(REGION_CPU1);
	size_t len = memory_region_length(REGION_CPU1);
	size_t i;

	/* the data lines are scrambled: a byte permutation, endian neutral */
	for (i = 0; i < len; i++)
		rom[i] = BITSWAP8(rom[i], 1, 4, 2, 0, 7, 5, 3, 6);

}

static DRIVER_INIT( jzth )
{
	jzth_protcount = 0;
	state_save_register_int("ybox", 0, "jzth protcount", &jzth_protcount);

	init_puckpkmn();
}

static void sj_init(UINT16 value_70001c, UINT32 (*ovl_xor)(UINT8))
{
	sj_70001c = value_70001c;
	sj_ovl_xor = ovl_xor;
	sj_prot_latch[0] = sj_prot_latch[1] = 0;
	sj_rom_ovl = 0;
	sj_rom_ovl_xor = 0;

	memory_set_opbase_handler(0, sj_opbase);

	state_save_register_UINT8("ybox", 0, "prot latch", sj_prot_latch, 2);
	state_save_register_UINT8("ybox", 0, "rom ovl", &sj_rom_ovl, 1);
	state_save_register_UINT32("ybox", 0, "rom ovl xor", &sj_rom_ovl_xor, 1);
	state_save_register_func_postload(sj_postload);

	init_puckpkmn();
}

static DRIVER_INIT( songjang ) { sj_init(0x0002, sj_no_ovl_xor); }
static DRIVER_INIT( shuifeng ) { sj_init(0x0031, shuifeng_ovl_xor); }


/******************************************************************************
	Game drivers
******************************************************************************/

/* Genie Hardware (uses Genesis VDP) also has 'Sun Mixing Co' put into tile RAM */
GAME ( 2000, puckpkmn,  0,        puckpkmn,  puckpkmn, puckpkmn, ROT0, "Sun Mixing",          "Puckman Pockimon Genie 2000" )
GAME ( 2000, jingling,  puckpkmn, jingling,  puckpkmn, puckpkmn, ROT0, "IBS Co. Ltd",         "Jingling Jiazu Genie 2000" )
GAME ( 2000, puckpkmnb, puckpkmn, puckpkmnb, puckpkmn, puckpkmn, ROT0, "bootleg",             "Puckman Pockimon Genie 2000 (bootleg)" )
GAMEX( 2000, jzth,      0,        jzth,      jzth,     jzth,     ROT0, "<unknown>",           "Juezhan Tianhuang", GAME_IMPERFECT_SOUND )
GAMEX( 2000, songjang,  0,        songjang,  songjang, songjang, ROT0, "WAH LAP",             "Songjiangyanyi Final", GAME_IMPERFECT_GRAPHICS )	/* some corruption on the cutscenes between levels (as in MAME) */
GAMEX( 1999, shuifeng,  0,        songjang,  jzth,     shuifeng, ROT0, "WAH LAP",             "Shuihu Feng Yun Zhuan", GAME_IMPERFECT_GRAPHICS )
