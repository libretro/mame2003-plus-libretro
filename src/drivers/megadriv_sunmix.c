/***************************************************************************

	Sun Mixing boards based on the Mega Drive, as in current MAME
	(megadriv_sunmix.cpp). Super Bubble Bobble moved here from
	drivers/segac2.c.

	Supported:
		Top Shooter (1995)
		Super Bubble Bobble (1996)

	Hacked up Mega Drive clones (Top Shooter: an original cartridge based
	board with an external ROM board and a JAMMA connector). An AT89C51
	MCU reads the inputs and puts them in a shared RAM at 0x200000; it is
	not emulated (as in MAME, the dump of Super Bubble Bobble is missing):
	the inputs are mapped straight into that RAM.

***************************************************************************/

#include "driver.h"
#include "megadriv.h"

/******************************************************************************
	Memory maps
******************************************************************************/

static READ16_HANDLER( topshoot_200051_r )
{
	return -0x5b;
}

/* needed to boot, some kind of hardware ident? */
static READ16_HANDLER( sbubsm_400000_r )
{
	return 0x5500;
}

static READ16_HANDLER( sbubsm_400002_r )
{
	return 0x0f00;
}

static MEMORY_READ16_START( topshoot_readmem )
	{ 0x000000, 0x1fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0x200042, 0x200043, input_port_0_word_r },		/* shared RAM: the MCU puts the inputs here */
	{ 0x200044, 0x200045, input_port_1_word_r },
	{ 0x200046, 0x200047, input_port_2_word_r },
	{ 0x200048, 0x200049, input_port_3_word_r },
	{ 0x200050, 0x200051, topshoot_200051_r },
	{ 0x200000, 0x2023ff, MRA16_RAM },
	{ 0xa10000, 0xa1001f, genesis_68000_io_r },			/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xe00000, 0xe1ffff, MRA16_BANK3 },
	{ 0xfe0000, 0xfeffff, MRA16_BANK4 },
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_READ16_START( sbubsm_readmem )
	{ 0x000000, 0x1fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0x200042, 0x200043, input_port_0_word_r },		/* shared RAM: the MCU puts the inputs here */
	{ 0x200044, 0x200045, input_port_1_word_r },
	{ 0x200046, 0x200047, input_port_2_word_r },
	{ 0x200048, 0x200049, input_port_3_word_r },
	{ 0x200050, 0x200051, topshoot_200051_r },
	{ 0x20007e, 0x20007f, input_port_4_word_r },
	{ 0x200000, 0x2023ff, MRA16_RAM },
	{ 0x400000, 0x400001, sbubsm_400000_r },
	{ 0x400002, 0x400003, sbubsm_400002_r },
	{ 0xa10000, 0xa1001f, genesis_68000_io_r },			/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xe00000, 0xe1ffff, MRA16_BANK3 },
	{ 0xfe0000, 0xfeffff, MRA16_BANK4 },
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( topshoot_writemem )
	{ 0x000000, 0x1fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0x200000, 0x2023ff, MWA16_RAM },					/* tested */
	{ 0xa10000, 0xa1001f, genesis_68000_io_w, &genesis_io_ram },	/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK4 },
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },	/* Main Ram */
MEMORY_END


/******************************************************************************
	Input ports
******************************************************************************/

INPUT_PORTS_START( topshoot ) /* Top Shooter Input Ports */
	PORT_START	/* IN0 */
	PORT_BIT( 0x4f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2, 1 )	/* Bet */
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3, 1 )	/* Start */
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1, 1 )	/* Fire */

	PORT_START	/* IN1 */
	PORT_BIT( 0xe7, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x08, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4, 1 )	/* Test mode down */

	PORT_START	/* IN2 */
	PORT_BIT( 0xfd, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN1, 1 )

	PORT_START	/* IN3 */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( sbubsm )
	/* the bit ordering in the ports is strange here because this is being read through shared RAM, the MCU presumably reads the real inputs then scrambles them in RAM for the 68k to sort out */
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN4 */
	PORT_BIT( 0x07, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )
	/* no service mode here? */
INPUT_PORTS_END


/******************************************************************************
	Machine drivers
******************************************************************************/

static MACHINE_DRIVER_START( topshoot )
	MDRV_IMPORT_FROM( genesis )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(topshoot_readmem, topshoot_writemem)

	MDRV_VISIBLE_AREA(0, 319, 0, 223)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( sbubsm )
	MDRV_IMPORT_FROM( genesis )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(sbubsm_readmem, topshoot_writemem)

	MDRV_VISIBLE_AREA(0, 319, 0, 223)
MACHINE_DRIVER_END


/******************************************************************************
	ROM definitions
******************************************************************************/

ROM_START( topshoot ) /* Top Shooter (c)1995 Sun Mixing */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "tc574000ad_u11_2.bin", 0x000000, 0x080000, CRC(b235c4d9) SHA1(fbb308a5f6e769f3277824cb6a3b50c308969ac2) )
	ROM_LOAD16_BYTE( "tc574000ad_u12_1.bin", 0x000001, 0x080000, CRC(e826f6ad) SHA1(23ec8bb608f954d3b915f061e7076c0c63b8259e) )

	ROM_REGION( 0x1000, REGION_USER1, 0 ) /* AT89C51, not hooked up yet (as in MAME) */
	ROM_LOAD( "89c51.bin", 0x0000, 0x1000, CRC(595475c8) SHA1(8313819ba06cc92b54f88c1ca9f34be8d1ec94d0) )
ROM_END

ROM_START( sbubsm )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "u11.bin", 0x000000, 0x080000, CRC(4f9337ea) SHA1(b245eb615f80afd25e29b2efdddb7f61c1deff6b) )
	ROM_LOAD16_BYTE( "u12.bin", 0x000001, 0x080000, CRC(f5374835) SHA1(3a97910f5f7327ec7ad6425dfdfa72c86196ed33) )

	/* the AT89C51 (89c51.bin) is not dumped; could be the same as topshoot (same PCB) */
ROM_END


/******************************************************************************
	Driver inits
******************************************************************************/

static DRIVER_INIT( topshoot )
{
	cpu_setbank(3, memory_region(REGION_CPU1));
	cpu_setbank(4, &genesis_68k_ram[0]);

}


/******************************************************************************
	Game drivers
******************************************************************************/

GAME( 1995, topshoot, 0, topshoot, topshoot, topshoot, ROT0, "Sun Mixing", "Top Shooter" )
GAME( 1996, sbubsm,   0, sbubsm,   sbubsm,   topshoot, ROT0, "Sun Mixing", "Super Bubble Bobble (Sun Mixing, Mega Drive clone hardware)" )
