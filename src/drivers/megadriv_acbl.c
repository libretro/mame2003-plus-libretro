/***************************************************************************

	Sega Mega Drive / Genesis based bootlegs

	The coin-op bootlegs of Mega Drive games, as in current MAME
	(megadriv_acbl.cpp). Moved here from drivers/segac2.c; the Mega Drive
	part (68000, sound Z80, I/O, interrupts) is shared with the other Mega
	Drive based boards (machine/megadriv.c), the VDP is in vidhrdw/segavdp.c.

	With a PIC16C57 for the coins and the DIP switches:
		Aladdin, Bare Knuckle II (barek2mb), Bare Knuckle III (barek3mba),
		Jurassic Park, Sonic The Hedgehog 2 and 3, Twinkle Tale
	With the ROMs scrambled (Actel):
		Bare Knuckle, Bare Knuckle II (barek2ch), Bare Knuckle III,
		Mortal Kombat 3, Sunset Riders, Super Street Fighter II,
		Bare Knuckle III / Sunset Riders (2 in 1)

	The PIC of barek2mb is emulated, as in MAME. The PIC dumps of aladmdb
	and jparkmb are not part of these sets: their PIC is simulated (coins
	and DIP switches), as are the undumped ones of sonic3mb, twinktmb and
	barek3mba.

	Mortal Kombat 3, Super Street Fighter II, Bare Knuckle and the 2 in 1
	have 6 button pads (MAME's TH counter, reset after 1.5ms with TH high).

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "megadriv.h"
#include "cpu/pic16c5x/pic16c5x.h"

/******************************************************************************
	I/O: the version register, 6 button pads, the 3rd port (coins)
*******************************************************************************

	The pads are input ports 0 and 1 in the usual layout of the Mega Drive
	boards (0x10 A, 0x20 Start, 0x40 B, 0x80 C); the extra buttons of the
	6 button pads (Z, Y, X, Mode in bits 0-3) are in acbl_xyz_port and the
	next one, the 3rd I/O port (coins) in acbl_exp_port.

******************************************************************************/

static UINT8 acbl_version;		/* version register: 0x20 Japan, 0xa0 export */
static int acbl_6button;		/* 6 button pads */
static int acbl_xyz_port;		/* input port of player 1 X, Y, Z */
static int acbl_exp_port;		/* input port of the 3rd I/O port, -1 = none */
static int acbl_iack;			/* interrupts acknowledged by the 68000, gated by the VDP enables */

static UINT8 pad_th[2];			/* TH as seen by the pads */
static UINT8 pad_phase[2];		/* 6 button pad: TH rising edges */
static UINT32 pad_rise[2];		/* when the last rising edge came */
static UINT32 acbl_frame;		/* frames, for the pad timeout */

/* the time in scanlines: the 6 button pad goes back to phase 0 when TH
   stays high for 1.5ms (24 lines) after a rising edge */
#define PAD_TIMEOUT_LINES	24

static UINT32 acbl_line(void)
{
	return acbl_frame * 262 + cpu_getscanline();
}

static void pad_check_timeout(int n)
{
	if (pad_th[n] && pad_phase[n] && acbl_line() - pad_rise[n] >= PAD_TIMEOUT_LINES)
		pad_phase[n] = 0;
}

/* the 6 data lines of a 6 button pad (active low) */
static int pad6_lines(int n, int th)
{
	int p = readinputport(n);
	int bc = (p & 0xc0) >> 2;		/* B, C */
	int as = p & 0x30;				/* A, Start */

	pad_check_timeout(n);

	switch (pad_phase[n])
	{
		case 2:
			if (!th)
				return as;										/* SA0000: 6 button pad */
			break;

		case 3:
			if (th)
				return bc | (readinputport(acbl_xyz_port + n) & 0x0f);	/* CBMXYZ */
			return as | 0x0f;
	}

	return th ? ((p & 0x0f) | bc) : ((p & 0x03) | as);
}

static READ16_HANDLER( acbl_io_r )
{
	int port, ctrl, data;

	switch (offset)
	{
		case 0x00:
			return acbl_version | (acbl_version << 8);

		case 0x01:
		case 0x02:
			if (!acbl_6button)
				break;
			port = offset - 1;
			ctrl = genesis_io_ram[0x04 + port];
			data = pad6_lines(port, genesis_io_ram[offset] & 0x40);
			if (genesis_io_ram[offset] & 0x40)
				data |= 0x40;
			data = (data & ((ctrl ^ 0xff) & 0x7f)) | (genesis_io_ram[offset] & (ctrl | 0x80));
			return data | (data << 8);

		case 0x03:
			if (acbl_exp_port < 0)
				break;
			ctrl = genesis_io_ram[0x06];
			data = readinputport(acbl_exp_port) & 0x7f;
			data = (data & ((ctrl ^ 0xff) & 0x7f)) | (genesis_io_ram[offset] & (ctrl | 0x80));
			return data | (data << 8);
	}

	return genesis_68000_io_r(offset, mem_mask);
}

static WRITE16_HANDLER( acbl_io_w )
{
	genesis_68000_io_w(offset, data, mem_mask);

	/* TH of the pads */
	if (offset == 0x01 || offset == 0x02)
	{
		int n = offset - 1;
		int th = (genesis_io_ram[offset] & 0x40) ? 1 : 0;

		if (acbl_6button && th && !pad_th[n])
		{
			pad_check_timeout(n);
			pad_phase[n] = (pad_phase[n] + 1) & 3;
			pad_rise[n] = acbl_line();
		}
		pad_th[n] = th;
	}
}

/* the DIP switches of the scrambled boards: 0x770070-0x770075 */
static READ16_HANDLER( acbl_dsw_r )
{
	return readinputport(3 + offset);
}

static INTERRUPT_GEN( acbl_vblank_interrupt )
{
	acbl_frame++;
	genesis_vblank_interrupt();
}


/******************************************************************************
	PIC16C57 (barek2mb), as in current MAME
******************************************************************************/

static UINT8 mcu_porta;
static UINT8 mcu_portc;
static UINT8 mcu_in_latch_msb, mcu_in_latch_lsb;
static UINT8 mcu_out_latch_msb, mcu_out_latch_lsb;

static READ16_HANDLER( mcu_r )
{
	return (mcu_out_latch_msb << 8) | mcu_out_latch_lsb;
}

static WRITE16_HANDLER( mcu_w )
{
	/* the latches take the whole data bus: a 68000 byte write drives the
	   byte on both halves (barek2mb writes the commands with move.b) */
	if (!ACCESSING_LSB)
		data = (data & 0xff00) | (data >> 8);
	else if (!ACCESSING_MSB)
		data = (data & 0x00ff) | (data << 8);

	mcu_in_latch_lsb = data & 0xff;
	mcu_in_latch_msb = data >> 8;
}

static WRITE_HANDLER( mcu_porta_w )
{
	/* 3---  select dsw (0) or latches (1)
	   -2--  latch enable
	   --1-  select input (0) or output (1) latches
	   ---0  select lsb or msb latches */
	if (!(data & 0x04) && (data & 0x02))
	{
		if (!(data & 0x01))
			mcu_out_latch_lsb = mcu_portc;
		else
			mcu_out_latch_msb = mcu_portc;
	}

	mcu_porta = data;
}

static READ_HANDLER( mcu_portb_r )
{
	/* bit 0: coin */
	return readinputport(3);
}

static WRITE_HANDLER( mcu_portb_w )
{
}

static READ_HANDLER( mcu_portc_r )
{
	UINT8 data = 0xff;

	/* the DIP switches */
	if (!(mcu_porta & 0x08))
		data &= readinputport(2);

	/* the input latches */
	if (!(mcu_porta & 0x02))
		data &= (mcu_porta & 0x01) ? mcu_in_latch_msb : mcu_in_latch_lsb;

	return data;
}

static WRITE_HANDLER( mcu_portc_w )
{
	mcu_portc = data;
}

static MEMORY_READ_START( mcu_readmem )
	{ PIC16C57_MEMORY_READ },
MEMORY_END

static MEMORY_WRITE_START( mcu_writemem )
	{ PIC16C57_MEMORY_WRITE },
MEMORY_END

static PORT_READ_START( mcu_readport )
	{ 0x00, 0x00, IORP_NOP },
	{ 0x01, 0x01, mcu_portb_r },
	{ 0x02, 0x02, mcu_portc_r },
	{ PIC16C5x_T0, PIC16C5x_T0, IORP_NOP },		/* T0 is not connected */
PORT_END

static PORT_WRITE_START( mcu_writeport )
	{ 0x00, 0x00, mcu_porta_w },
	{ 0x01, 0x01, mcu_portb_w },
	{ 0x02, 0x02, mcu_portc_w },
PORT_END


/******************************************************************************
	Machine init and save states
******************************************************************************/

static void acbl_state_register(void)
{
	state_save_register_UINT8 ("acbl", 0, "pad_th", pad_th, 2);
	state_save_register_UINT8 ("acbl", 0, "pad_phase", pad_phase, 2);
	state_save_register_UINT32("acbl", 0, "pad_rise", pad_rise, 2);
	state_save_register_UINT32("acbl", 0, "frame", &acbl_frame, 1);
	state_save_register_UINT8 ("acbl", 0, "mcu_porta", &mcu_porta, 1);
	state_save_register_UINT8 ("acbl", 0, "mcu_portc", &mcu_portc, 1);
	state_save_register_UINT8 ("acbl", 0, "mcu_in_msb", &mcu_in_latch_msb, 1);
	state_save_register_UINT8 ("acbl", 0, "mcu_in_lsb", &mcu_in_latch_lsb, 1);
	state_save_register_UINT8 ("acbl", 0, "mcu_out_msb", &mcu_out_latch_msb, 1);
	state_save_register_UINT8 ("acbl", 0, "mcu_out_lsb", &mcu_out_latch_lsb, 1);
}

static MACHINE_INIT( acbl )
{
	machine_init_genesis();

	/* as on the real 315-5313: V-INT and H-INT pending until the 68000
	   takes them, and only while enabled in the VDP registers */
	if (acbl_iack)
		genesis_enable_iack();

	pad_th[0] = pad_th[1] = 0;
	pad_phase[0] = pad_phase[1] = 0;
	pad_rise[0] = pad_rise[1] = 0;
	acbl_frame = 0;
	mcu_porta = mcu_portc = 0;
	mcu_in_latch_msb = mcu_in_latch_lsb = 0;
	mcu_out_latch_msb = mcu_out_latch_lsb = 0;

	/* save states (repeated registrations are ignored) */
	acbl_state_register();
}

/* the board configuration, called by the driver inits */
static void acbl_config(UINT8 version, int six_button, int exp_port, int xyz_port)
{
	acbl_iack = 0;
	acbl_version = version;
	acbl_6button = six_button;
	acbl_exp_port = exp_port;
	acbl_xyz_port = xyz_port;
}


/******************************************************************************
	Memory maps
******************************************************************************/

/* the PCB with the PIC (sonic2mb, sonic3mb, jparkmb, twinktmb) and barek2ch:
   a 2MB ROM area and some RAM at 0x200000 */
static MEMORY_READ16_START( md_bootleg_readmem )
	{ 0x000000, 0x1fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0x202000, 0x2023ff, MRA16_RAM },
	{ 0xa10000, 0xa1001f, acbl_io_r },					/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xe00000, 0xe1ffff, MRA16_BANK3 },
	{ 0xfe0000, 0xfeffff, MRA16_BANK4 },
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( md_bootleg_writemem )
	{ 0x000000, 0x1fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0x200000, 0x2023ff, MWA16_RAM }, /* tested */
	{ 0xa10000, 0xa1001f, acbl_io_w, &genesis_io_ram },	/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK4 },
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram }, /* Main Ram */
MEMORY_END

/* the plain cartridge map */
static MEMORY_READ16_START( megadrvb_readmem )
	{ 0x000000, 0x3fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, acbl_io_r },					/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xfe0000, 0xfeffff, MRA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( megadrvb_writemem )
	{ 0x000000, 0x3fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, acbl_io_w, &genesis_io_ram },	/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },/* Main Ram */
MEMORY_END

/* Super Street Fighter II: 6MB of ROM in a linear map, the cart mapper
   (0xa130f0) is disabled */
static MEMORY_READ16_START( ssf2mdb_readmem )
	{ 0x000000, 0x5fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0x770070, 0x770075, acbl_dsw_r },
	{ 0xa10000, 0xa1001f, acbl_io_r },					/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xfe0000, 0xfeffff, MRA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( ssf2mdb_writemem )
	{ 0x000000, 0x5fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, acbl_io_w, &genesis_io_ram },	/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa130f0, 0xa130ff, MWA16_NOP },					/* no cart mapper */
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },/* Main Ram */
MEMORY_END

/* the PCB with an emulated PIC (barek2mb) */
static MEMORY_READ16_START( md_boot_mcu_readmem )
	{ 0x000000, 0x1fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0x330000, 0x330001, mcu_r },
	{ 0xa10000, 0xa1001f, acbl_io_r },					/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },					/* VDP Access */
	{ 0xfe0000, 0xfeffff, MRA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( md_boot_mcu_writemem )
	{ 0x000000, 0x1fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0x220000, 0x220001, mcu_w },
	{ 0xa10000, 0xa1001f, acbl_io_w, &genesis_io_ram },	/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa00000, 0xa0ffff, genesis_68k_to_z80_w },
	{ 0xc00000, 0xc0000f, segavdp_w },					/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },/* Main Ram */
MEMORY_END


/******************************************************************************
	Machine drivers
******************************************************************************/

static MACHINE_DRIVER_START( md_bootleg )
	MDRV_IMPORT_FROM( genesis )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(md_bootleg_readmem, md_bootleg_writemem)
	MDRV_MACHINE_INIT(acbl)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( megadrvb )
	MDRV_IMPORT_FROM( genesis )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(megadrvb_readmem, megadrvb_writemem)
	MDRV_MACHINE_INIT(acbl)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( megadrvb_6b )
	MDRV_IMPORT_FROM( megadrvb )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_VBLANK_INT(acbl_vblank_interrupt, 1)	/* counts the frames for the pad timeout */
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ssf2mdb )
	MDRV_IMPORT_FROM( megadrvb_6b )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(ssf2mdb_readmem, ssf2mdb_writemem)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( md_boot_mcu )
	MDRV_IMPORT_FROM( megadrvb )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(md_boot_mcu_readmem, md_boot_mcu_writemem)

	MDRV_CPU_ADD(PIC16C57, 4000000 / PIC16C5x_CLOCK_DIVIDER)	/* unknown clock, as in MAME */
	MDRV_CPU_MEMORY(mcu_readmem, mcu_writemem)
	MDRV_CPU_PORTS(mcu_readport, mcu_writeport)
MACHINE_DRIVER_END


/******************************************************************************
	Input ports
******************************************************************************/

INPUT_PORTS_START( aladbl )
	PORT_START /* Joypad 1 (3 button + start) NOT READ DIRECTLY */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT_NAME( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "P1 Throw" )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_NAME( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "P1 Sword" )
	PORT_BIT_NAME( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "P1 Jump" )


	PORT_START /* Joypad 2 (3 button + start) Not used */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 3rd I/O port */

	/* As I don't know how it is on real hardware, this is more a guess than anything */
	PORT_START /* MCU hooked up via readinputport (3) */
	PORT_DIPNAME( 0x07, 0x01, DEF_STR( Coinage ) )          /* code at 0x1b2a50 - unsure if there are so many settings */
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_7C ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )         /* to avoid it being changed and corrupting Coinage settings */
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) )       /* code at 0x1b2680 */
	PORT_DIPSETTING(    0x10, "Easy" )             /* "PRACTICE" */
	PORT_DIPSETTING(    0x00, "Normal" )           /* "NORMAL" */
	PORT_DIPSETTING(    0x20, "Hard" )             /* "DIFFICULT" */
	PORT_BIT_IMPULSE(   0x100, IP_ACTIVE_HIGH, IPT_COIN1, 1 ) /* needed to avoid credits getting mad */
INPUT_PORTS_END

INPUT_PORTS_START( barek2ch )
	PORT_START	/* IN0 player 1 controller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )

	PORT_START	/* IN1 player 2 controller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )

	PORT_START
	PORT_BIT(  0x3f, IP_ACTIVE_LOW, IPT_UNUSED ) /* apparently no use for these */
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x08, 0x08, "SW1:4" )
    PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x10, 0x10, "SW1:5" )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x20, 0x20, "SW1:6" )
    PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x40, 0x40, "SW1:7" )
    PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x80, 0x80, "SW1:8" )
    PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "SW2:1" ) /* at least some of the first 3 seem to control difficulty (enemies attack later / less frequently by switching these) */
    PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x02, 0x02, "SW2:2" )
    PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x04, 0x04, "SW2:3" )
    PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xe0, 0xe0, "Starting Level" )
	PORT_DIPSETTING(    0xe0, "1" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x60, "5" )
	PORT_DIPSETTING(    0x40, "6" )
	PORT_DIPSETTING(    0x20, "7" )
	PORT_DIPSETTING(    0x00, "8" )

	PORT_START /* present on PCB but there doesn't seem to be any read for them */
	PORT_DIPNAME( 0x01, 0x01, "SW3:1" )
    PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x02, 0x02, "SW3:2" )
    PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x04, 0x04, "SW3:3" )
    PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x08, 0x08, "SW3:4" )
    PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x10, 0x10, "SW3:5" )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x20, 0x20, "SW3:6")
    PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x40, 0x40, "SW3:7" )
    PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x80, 0x80, "SW3:8" )
    PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( barek3 )
	PORT_START	/* IN0 player 1 controller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT_NAME( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "P1 Button A" )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_NAME( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "P1 Button B" )
	PORT_BIT_NAME( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "P1 Button C" )

	PORT_START	/* IN1 player 2 controller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT_NAME( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2, "P2 Button A" )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_NAME( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2, "P2 Button B" )
	PORT_BIT_NAME( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2, "P2 Button C" )

	PORT_START
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Very_Easy" )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0xe0, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Very_Hard" )
INPUT_PORTS_END

INPUT_PORTS_START( sonic2mb )
    PORT_START  /* IN0 player 1 controller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START /* Joypad 2 (3 button + start) Not used */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

    PORT_START /* 3rd I/O port */

	PORT_START /* DSW via readinputport 3 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x00fc, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME(          0x0300, 0x0200, DEF_STR( Lives ) )
	PORT_DIPSETTING(       0x0000, "1" )
	PORT_DIPSETTING(       0x0100, "2" )
	PORT_DIPSETTING(       0x0200, "3" )
	PORT_DIPSETTING(       0x0300, "4" )
	PORT_DIPNAME(  0x3c00, 0x2000, "Timer Speed" ) 
	PORT_DIPSETTING(       0x3c00, "0 (Slowest)" )
	PORT_DIPSETTING(       0x3800, "1" )
	PORT_DIPSETTING(       0x3400, "2" )
	PORT_DIPSETTING(       0x3000, "3" )
	PORT_DIPSETTING(       0x2c00, "4" )
	PORT_DIPSETTING(       0x2800, "5" )
	PORT_DIPSETTING(       0x2400, "6" )
	PORT_DIPSETTING(       0x2000, "7" )
	PORT_DIPSETTING(       0x1c00, "8" )
	PORT_DIPSETTING(       0x1800, "9" )
	PORT_DIPSETTING(       0x1400, "10" )
	PORT_DIPSETTING(       0x1000, "11" )
	PORT_DIPSETTING(       0x0c00, "12" )
	PORT_DIPSETTING(       0x0800, "13" )
	PORT_DIPSETTING(       0x0400, "14" )
	PORT_DIPSETTING(       0x0000, "15 (Fastest)" )
    PORT_DIPNAME( 0x4000,  0x4000, "SW1:7" )
    PORT_DIPSETTING(       0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(       0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x8000,  0x8000, "SW1:8" )
    PORT_DIPSETTING(       0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(       0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( sonic3mb )
	PORT_START /* Joypad 1 (3 button + start) NOT READ DIRECTLY */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT_NAME( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "P1 Throw" )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_NAME( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "P1 Sword" )
	PORT_BIT_NAME( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "P1 Jump" )


	PORT_START /* Joypad 2 (3 button + start) Not used */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 3rd I/O port */

	/* As I don't know how it is on real hardware, this is more a guess than anything */
	PORT_START /* MCU hooked up via readinputport (3) */
	/* TODO: actual diplocations */
	/* lower 4 bits is for coinage? Not read by 68k */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xc0, 0x00, "Time Limit" )
	PORT_DIPSETTING(    0xc0, "1:00" )
	PORT_DIPSETTING(    0x80, "2:00" )
	PORT_DIPSETTING(    0x40, "3:00" )
	PORT_DIPSETTING(    0x00, "4:00" )
	
	PORT_START /* coins hooked up via readinputport (4) */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0xff00, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( jparkmb )
	PORT_START /* Joypad 1 (3 button + start) NOT READ DIRECTLY */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT_NAME( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "P1 Throw" )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_NAME( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "P1 Sword" )
	PORT_BIT_NAME( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "P1 Jump" )


	PORT_START /* Joypad 2 (3 button + start) Not used */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 3rd I/O port */

	/* As I don't know how it is on real hardware, this is more a guess than anything */
	PORT_START 
	PORT_DIPNAME( 0x01,   0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02,   0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04,   0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08,   0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10,   0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20,   0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40,   0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80,   0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )
	
	PORT_START /* coins hooked up via readinputport (4) */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0xff00, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


/* a pad in the layout of the Mega Drive boards; A, B, C as in MAME */
#define ACBL_PAD(player, start, button_a, button_b, button_c) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | player ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | player ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | player ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | player ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, button_a ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, start ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, button_b ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, button_c )

/* the extra buttons of a 6 button pad: Z, Y, X (no Mode button) */
#define ACBL_PAD_XYZ(player) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON6 | player ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 | player ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON4 | player ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )

#define ACBL_EXP_COINS \
	PORT_START	/* 3rd I/O port */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

#define ACBL_DIP_UNKNOWN(mask, name) \
	PORT_DIPNAME( mask, mask, name ) \
	PORT_DIPSETTING(    mask, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

#define ACBL_DIP_BANK_UNKNOWN(bank) \
	PORT_START \
	ACBL_DIP_UNKNOWN( 0x01, bank ":1" ) \
	ACBL_DIP_UNKNOWN( 0x02, bank ":2" ) \
	ACBL_DIP_UNKNOWN( 0x04, bank ":3" ) \
	ACBL_DIP_UNKNOWN( 0x08, bank ":4" ) \
	ACBL_DIP_UNKNOWN( 0x10, bank ":5" ) \
	ACBL_DIP_UNKNOWN( 0x20, bank ":6" ) \
	ACBL_DIP_UNKNOWN( 0x40, bank ":7" ) \
	ACBL_DIP_UNKNOWN( 0x80, bank ":8" )

/* ports: 0, 1 pads; 2 3rd I/O port; 3-5 DIP switches; 6, 7 X Y Z */
INPUT_PORTS_START( ssf2mdb )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1, IPT_BUTTON3 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2, IPT_BUTTON3 | IPF_PLAYER2 )
	ACBL_EXP_COINS

	PORT_START	/* DSWA */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )

	PORT_START	/* DSWB */
	PORT_DIPNAME( 0x07, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "0 (Easiest)" )
	PORT_DIPSETTING(    0x06, "1" )
	PORT_DIPSETTING(    0x05, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x01, "6" )
	PORT_DIPSETTING(    0x00, "7 (Hardest)" )

	PORT_START	/* DSWC */
	PORT_DIPNAME( 0x0f, 0x0b, "Speed" )
	PORT_DIPSETTING(    0x0f, "0 (Slowest)" )
	PORT_DIPSETTING(    0x0e, "1" )
	PORT_DIPSETTING(    0x0d, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x0b, "4" )
	PORT_DIPSETTING(    0x0a, "5" )
	PORT_DIPSETTING(    0x09, "6" )
	PORT_DIPSETTING(    0x08, "7" )
	PORT_DIPSETTING(    0x07, "8" )
	PORT_DIPSETTING(    0x06, "9" )
	PORT_DIPSETTING(    0x05, "10 (Fastest)" )

	ACBL_PAD_XYZ( IPF_PLAYER1 )
	ACBL_PAD_XYZ( IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( mk3mdb )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1, IPT_BUTTON3 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2, IPT_BUTTON3 | IPF_PLAYER2 )
	ACBL_EXP_COINS

	PORT_START	/* DSWA */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )

	PORT_START	/* DSWB */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "Easiest" )
	PORT_DIPSETTING(    0x06, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x05, "Medium" )
	PORT_DIPSETTING(    0x04, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Blood" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSWC, not read */

	ACBL_PAD_XYZ( IPF_PLAYER1 )
	ACBL_PAD_XYZ( IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( srmdb )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1, IPT_UNUSED )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2, IPT_UNUSED )
	ACBL_EXP_COINS

	PORT_START	/* DSWA */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )

	PORT_START	/* DSWB */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START	/* DSWC */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
INPUT_PORTS_END

INPUT_PORTS_START( barekch )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON3 | IPF_PLAYER1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON3 | IPF_PLAYER2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2 )
	ACBL_EXP_COINS

	ACBL_DIP_BANK_UNKNOWN( "SW1" )
	ACBL_DIP_BANK_UNKNOWN( "SW2" )

	PORT_START	/* DSWC */
	ACBL_DIP_UNKNOWN( 0x01, "SW3:1" )
	ACBL_DIP_UNKNOWN( 0x02, "SW3:2" )
	ACBL_DIP_UNKNOWN( 0x04, "SW3:3" )
	ACBL_DIP_UNKNOWN( 0x08, "SW3:4" )
	ACBL_DIP_UNKNOWN( 0x10, "SW3:5" )
	ACBL_DIP_UNKNOWN( 0x20, "SW3:6" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0x00, "6" )

	ACBL_PAD_XYZ( IPF_PLAYER1 )
	ACBL_PAD_XYZ( IPF_PLAYER2 )
INPUT_PORTS_END

/* ports: 0, 1 pads; 2 coins (0x770078); 3-5 DIP switches */
INPUT_PORTS_START( biohzdmb )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1, IPT_BUTTON3 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2, IPT_BUTTON3 | IPF_PLAYER2 )

	PORT_START	/* coins */
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN1 )

	/* only three of the four banks of 8 switches are populated */
	ACBL_DIP_BANK_UNKNOWN( "DSWA" )
	ACBL_DIP_BANK_UNKNOWN( "DSWB" )
	ACBL_DIP_BANK_UNKNOWN( "DSWC" )
INPUT_PORTS_END

INPUT_PORTS_START( bk3ssrmb )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON3 | IPF_PLAYER1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON3 | IPF_PLAYER2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2 )
	ACBL_EXP_COINS

	PORT_START	/* DSWA */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Very Easy" )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, "Very Hard" )

	ACBL_DIP_BANK_UNKNOWN( "DSWB" )
	ACBL_DIP_BANK_UNKNOWN( "DSWC" )

	PORT_START	/* no X, Y, Z buttons */
	PORT_START
INPUT_PORTS_END

/* ports: 0, 1 pads; 2 DIP switches; 3 PIC port B (coin) */
INPUT_PORTS_START( barek2 )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1, IPT_BUTTON3 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2, IPT_BUTTON3 | IPF_PLAYER2 )

	PORT_START	/* DSW, read by the PIC */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, "8 Coins/3 Credits" )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x02, "5 Coins/3 Credits" )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x80, "Medium" )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START	/* PIC port B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* ports: 0, 1 pads; 2 coins (simulated PIC); 3 DSW */
INPUT_PORTS_START( barek3a )
	ACBL_PAD( IPF_PLAYER1, IPT_START1, IPT_BUTTON1 | IPF_PLAYER1, IPT_BUTTON2 | IPF_PLAYER1, IPT_BUTTON3 | IPF_PLAYER1 )
	ACBL_PAD( IPF_PLAYER2, IPT_START2, IPT_BUTTON1 | IPF_PLAYER2, IPT_BUTTON2 | IPF_PLAYER2, IPT_BUTTON3 | IPF_PLAYER2 )

	PORT_START	/* COINS */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Very Easy" )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, "Very Hard" )
INPUT_PORTS_END

/******************************************************************************
	ROM definitions
******************************************************************************/

ROM_START( aladmdb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "m1.bin", 0x000001, 0x080000,  CRC(5e2671e4) SHA1(54705c7614fc7b5a1065478fa41f51dd1d8045b7) )
	ROM_LOAD16_BYTE( "m2.bin", 0x000000, 0x080000,  CRC(142a0366) SHA1(6c94aa9936cd11ccda503b52019a6721e64a32f0) )
	ROM_LOAD16_BYTE( "m3.bin", 0x100001, 0x080000,  CRC(0feeeb19) SHA1(bd567a33077ab9997871d21736066140d50e3d70) )
	ROM_LOAD16_BYTE( "m4.bin", 0x100000, 0x080000,  CRC(bc712661) SHA1(dfd554d000399e17b4ddc69761e572195ed4e1f0))
ROM_END

ROM_START( barek2ch ) /* all 27c4001 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "u14", 0x000001, 0x080000, CRC(b0ee177f) SHA1(d63e6ee30fe7f4aaab098d3920eabc456730b2c5) )
	ROM_LOAD16_BYTE( "u15", 0x000000, 0x080000, CRC(09264195) SHA1(c5439731d932c90a57d68c4d82c9ebed8a01bd53) )
	ROM_LOAD16_BYTE( "u16", 0x100001, 0x080000, CRC(6c814fc4) SHA1(edaf5117b19d3fb40218c5f7c4b5099c9189f1be) )
	ROM_LOAD16_BYTE( "u17", 0x100000, 0x080000, CRC(cae1922e) SHA1(811c2164b6c467a49af4b0d22f151cd13c9efbc9) )
ROM_END

ROM_START( barek3mb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "6.u19", 0x000000, 0x080000,  CRC(2de19519) SHA1(f5fcef1da8b5370e399f0451382e3c6e7754c9c8) )
	ROM_LOAD16_BYTE( "3.u18", 0x000001, 0x080000,  CRC(db900e82) SHA1(172a4fe01a0ffd1ea3aed74f2c58234fd55b876d) )
	ROM_LOAD16_BYTE( "4.u15", 0x100000, 0x080000,  CRC(6353b4b1) SHA1(9f89a2f02170496ca798b89e37e1f2bae0e9155d) )
	ROM_LOAD16_BYTE( "1.u14", 0x100001, 0x080000,  CRC(24d31e12) SHA1(64c1b968e1ee5d0355d902e280f33e4466f27b07) )
	ROM_LOAD16_BYTE( "5.u17", 0x200000, 0x080000,  CRC(0feb974f) SHA1(ed1a25b6f1669dc6061d519985b6373fa89176c7) )
	ROM_LOAD16_BYTE( "2.u16", 0x200001, 0x080000,  CRC(bba4a585) SHA1(32c59729943d7b4c1a39f2a2b0dae9ce16991e9c) )
ROM_END

ROM_START( sonic2mb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "m1", 0x000001, 0x080000,  CRC(7b40aa24) SHA1(247882cd1f412366d61aeb4d85bbeefd5f108e1d) )
	ROM_LOAD16_BYTE( "m2", 0x000000, 0x080000,  CRC(84b3f758) SHA1(19846b9d951db6f78f3e155d33f1b6349fb87f1a) )
ROM_END

ROM_START( sonic3mb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "sonic3-4.bin", 0x000000, 0x080000, CRC(b7318bb8) SHA1(1707b563794c3ab4a1f04cb449efdd6f817317fb) )
	ROM_LOAD16_BYTE( "sonic3-3.bin", 0x000001, 0x080000, CRC(1898479f) SHA1(5f1c581157959e11979882d2180ae4b98c6a89d5) )
	ROM_LOAD16_BYTE( "sonic3-2.bin", 0x100000, 0x080000, CRC(02232f45) SHA1(8cdcb156603108ac9d3ef888f75adb5327abce1a) )
	ROM_LOAD16_BYTE( "sonic3-1.bin", 0x100001, 0x080000, CRC(cee2f679) SHA1(4cc7a8a228f7fc4f7a38c69a65585765751a49e5) )

	ROM_REGION( 0x1000, REGION_CPU2, ROMREGION_ERASE00 )
/* ROM_LOAD( "pic16c57xtp", 0x0000, 0x1000, NO_DUMP ) */
ROM_END

ROM_START( jparkmb ) /* same PCB as twinktmb, JPA-028 label */
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "f24.bin", 0x000000, 0x080000,  CRC(bdd851d2) SHA1(1a75922e848fd5c7cd4ab102c99effcfcf382097) )
	ROM_LOAD16_BYTE( "f23.bin", 0x000001, 0x080000,  CRC(8dc66c71) SHA1(a2741ffa583a4b779b7be3e3ae628e97f792ee3d) )
	ROM_LOAD16_BYTE( "f22.bin", 0x100000, 0x080000,  CRC(36337d06) SHA1(d537cff2c8ed58da146faf390c09252be359ccd1) )
	ROM_LOAD16_BYTE( "f21.bin", 0x100001, 0x080000,  CRC(6ede6b6b) SHA1(cf29300d9278ea03f54cf54ea582bdd8b9bbdbbd) )
	
	ROM_REGION( 0x2000, REGION_CPU2, ROMREGION_ERASE00 )
/*	ROM_LOAD( "pic16c57xtp", 0x0000, 0x2000, NO_DUMP ) */
ROM_END

ROM_START( twinktmb ) /* same PCB as sonic2mb, but in this one the PIC is populated */
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "m2.bin", 0x000000, 0x080000,  CRC(44424f8f) SHA1(e16318bfdf869765c821c264cf9a7e6c728f7073) )
	ROM_LOAD16_BYTE( "m1.bin", 0x000001, 0x080000,  CRC(69aa916e) SHA1(7ea6b571fd0b6494051d5846ee9b4564b7692766) )

	ROM_REGION( 0x2000, REGION_CPU2, ROMREGION_ERASE00 )
/*	ROM_LOAD( "pic16c57xtp", 0x0000, 0x2000, NO_DUMP ) */
ROM_END  


ROM_START( mk3mdb ) /* ROMs are scrambled: the address descramble is in the ROM load, the data descramble in the init */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASE00 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "1.u1", 0x080001, 0x020000,  CRC(0dc01b23) SHA1(f1aa7ac88c8e3deb5a0a065862722e9d27b87b4c) )
	ROM_CONTINUE(            0x000001, 0x020000)
	ROM_CONTINUE(            0x0c0001, 0x020000)
	ROM_CONTINUE(            0x040001, 0x020000)
	ROM_LOAD16_BYTE( "2.u3", 0x180001, 0x040000,  CRC(50250235) SHA1(9f9e06f26163b92c76397fde43b38b3536bcb637) )
	ROM_CONTINUE(            0x100001, 0x040000)
	ROM_LOAD16_BYTE( "3.u9", 0x280001, 0x040000,  CRC(493404c1) SHA1(73f4bd1eeeee3f175f4378ab406a97f94f88880b) )
	ROM_CONTINUE(            0x200001, 0x040000)
	ROM_LOAD16_BYTE( "4.u11",0x380001, 0x040000,  CRC(a52156b8) SHA1(0990ef1fb3427a5d3c262e264feb25c1db75ed33) )
	ROM_CONTINUE(            0x300001, 0x040000)
	ROM_LOAD16_BYTE( "6.u2", 0x080000, 0x020000,  CRC(9852fd6f) SHA1(348befeca5129c5ea2c142760ec93511f98f23cc) )
	ROM_CONTINUE(            0x000000, 0x020000)
	ROM_CONTINUE(            0x0c0000, 0x020000)
	ROM_CONTINUE(            0x040000, 0x020000)
	ROM_LOAD16_BYTE( "5.u4", 0x180000, 0x040000,  CRC(ed6a6d13) SHA1(eaab912ee035ece03f7cfceb1b546004399daad5) )
	ROM_CONTINUE(            0x100000, 0x040000)
	ROM_LOAD16_BYTE( "7.u10",0x280000, 0x040000,  CRC(a124d8d1) SHA1(d391b130992701d0fae7e827ba314b8368d809de) )
	ROM_CONTINUE(            0x200000, 0x040000)
	ROM_LOAD16_BYTE( "8.u12",0x380000, 0x040000,  CRC(8176f7cc) SHA1(375e1e982b97ba709fb160b04f56f6aa2d580104) )
	ROM_CONTINUE(            0x300000, 0x040000)
ROM_END

ROM_START( ssf2mdb ) /* custom PCB, linear ROM mapping of 6MB */
	ROM_REGION( 0x600000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "rom_a", 0x000000, 0x200000,  CRC(59726521) SHA1(3120bac17f56c01ffb9d3f9e31efa0263e3774af) )
	ROM_LOAD16_BYTE( "rom_b", 0x000001, 0x200000,  CRC(7dad5540) SHA1(9279068b2218d239fdd557dd959ac70e74853178) )
	ROM_LOAD16_BYTE( "rom_c", 0x400000, 0x080000,  CRC(deb48624) SHA1(39ffa7de7b808e0b95cb039bb381705d77420933) )
	ROM_LOAD16_BYTE( "rom_d", 0x400001, 0x080000,  CRC(b99f6a5b) SHA1(adbe28a7522024bc66328ac86fecf9ded3310e8e) )
ROM_END

ROM_START( srmdb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "u1", 0x000001, 0x020000,  CRC(c59f33bd) SHA1(bd5bce7698a70ea005b79ab34bcdb056872ef980) )
	ROM_LOAD16_BYTE( "u2", 0x000000, 0x020000,  CRC(9125c054) SHA1(c73bdeb6b11c59d2b5f5968959b02697957ca894) )
	ROM_LOAD16_BYTE( "u3", 0x040001, 0x020000,  CRC(0fee0fbe) SHA1(001e0fda12707512aad537e533acf28e726e6107) )
	ROM_LOAD16_BYTE( "u4", 0x040000, 0x020000,  CRC(fc2aed41) SHA1(27eb3957f5ed26ee5276523b1df46fa7eb298e1f) )
ROM_END

ROM_START( barek2mb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "m1.bin", 0x000001, 0x080000,  CRC(1c1fa718) SHA1(393488f7747478728eb4f20c10b0cfce3b188719) )
	ROM_LOAD16_BYTE( "m2.bin", 0x000000, 0x080000,  CRC(59ee0905) SHA1(0e9f1f6e17aae2dd99bf9d7f640568b48ba699c7) )
	ROM_LOAD16_BYTE( "m3.bin", 0x100001, 0x080000,  CRC(6ec5af5d) SHA1(9088a2d4cff5e7eb439ebaa91ad3bfff11366127) )
	ROM_LOAD16_BYTE( "m4.bin", 0x100000, 0x080000,  CRC(d8c61e0d) SHA1(3d06e656f6621bb0741211f80c1ecff1669475ee) )

	ROM_REGION( 0x2000, REGION_CPU3, 0 ) /* PIC16C57 */
	ROM_LOAD( "bk_pic16c57rcp.bin", PIC16C57_PGM_OFFSET, 0x1000, CRC(434ad1b7) SHA1(9241554793c7375cf58239e762481a4b80a51df6) ) /* unprotected */
ROM_END

ROM_START( barek3mba )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "u4", 0x000000, 0x080000,  CRC(6b116813) SHA1(df9811c5da9f58f2bba462447b792f3067b10411) )
	ROM_LOAD16_BYTE( "u3", 0x000001, 0x080000,  CRC(8c891e5a) SHA1(aa1fb1aa2c68b1ae24e1ac30a82bc4e454b952b5) )
	ROM_LOAD16_BYTE( "u6", 0x100000, 0x080000,  CRC(a2ec29e2) SHA1(8e8eee64554396070455a737f4009d33d2ffa535) )
	ROM_LOAD16_BYTE( "u5", 0x100001, 0x080000,  CRC(f79e0028) SHA1(54022eadc6e345c049b7357b17b636b31d2af914) )
	ROM_LOAD16_BYTE( "u8", 0x200000, 0x080000,  CRC(55be9542) SHA1(f78c273858f3ae77b36f1229797e80a4ab102a03) )
	ROM_LOAD16_BYTE( "u7", 0x200001, 0x080000,  CRC(bba4a585) SHA1(32c59729943d7b4c1a39f2a2b0dae9ce16991e9c) )

	/* the PIC (bk_pic16c57rcp.bin) is not dumped, it is simulated */
ROM_END

ROM_START( srssf2mb ) /* the order of the ROMs is not known (as in MAME) */
	ROM_REGION( 0x700000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "1.u15", 0x000000, 0x040000, CRC(b40982ca) SHA1(fa7b266d346e2a79936984b2b989f3e8b6a90223) ) /* the first 0x80000 of SSF2, with differences */
	ROM_LOAD16_BYTE( "3.u14", 0x000001, 0x040000, CRC(81ac9700) SHA1(cdc7d49d75b8d4a5ff41181a063e1917483afd75) )
	ROM_LOAD16_BYTE( "2.u13", 0x080000, 0x040000, CRC(e5f1ab97) SHA1(0f4c527043f1272e75a996f4f7270c6ea4ed3c4d) ) /* Sunset Riders */
	ROM_LOAD16_BYTE( "4.u12", 0x080001, 0x040000, CRC(32ee1048) SHA1(1b135c200b4440e95a7d1766b4b404ddd238872d) )
	ROM_LOAD16_WORD_SWAP( "ys104-01.u5", 0x100000, 0x200000, CRC(055dea8b) SHA1(663005c6c87046e955c6295bf25379d258dd4066) ) /* a version of SSF2 very similar to ssf2mdb */
	ROM_LOAD16_WORD_SWAP( "ys104-02.u6", 0x300000, 0x200000, CRC(6fcf8db5) SHA1(319f320110966058ccec8da640877e8a82c3e4b4) )
	ROM_LOAD16_WORD_SWAP( "ys104-03.u7", 0x500000, 0x200000, CRC(0771d570) SHA1(6bb93a1fd8f0f2a1a12e9cd3eec762ac46912632) ) /* 1st and 2nd half identical */
ROM_END

ROM_START( biohzdmb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "u14", 0x000001, 0x080000, CRC(b96cf28c) SHA1(0a49cf2fc0a2712b423b4e1f95a4befe3bf5c746) )
	ROM_LOAD16_BYTE( "u15", 0x000000, 0x080000, CRC(41a8eae8) SHA1(a9db565f4ca4d71c81fbb44fd429221951887bab) )
ROM_END

ROM_START( bk3ssrmb )
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "1.u15", 0x000000, 0x080000,  CRC(120a4b64) SHA1(7084fde0d08143f48f83d2afef30522d75c7889c) )
	ROM_LOAD16_BYTE( "5.u14", 0x000001, 0x080000,  CRC(1eb625d3) SHA1(8f67ab874643b3eafe91768df84ea4a3b8a5fa82) )
	ROM_LOAD16_BYTE( "2.u13", 0x100000, 0x080000,  CRC(af439685) SHA1(5cc55608355e11096c9fadb8d2460cf66704deec) )
	ROM_LOAD16_BYTE( "6.u12", 0x100001, 0x080000,  CRC(24d31e12) SHA1(64c1b968e1ee5d0355d902e280f33e4466f27b07) )
	ROM_LOAD16_BYTE( "3.u11", 0x200000, 0x080000,  CRC(dfa5c478) SHA1(079a9cad5c2252b2d65aa3c4dc9cba331078eeb9) )
	ROM_LOAD16_BYTE( "7.u10", 0x200001, 0x080000,  CRC(bba4a585) SHA1(32c59729943d7b4c1a39f2a2b0dae9ce16991e9c) )
	ROM_LOAD16_BYTE( "4.u9",  0x300000, 0x040000,  CRC(e5f1ab97) SHA1(0f4c527043f1272e75a996f4f7270c6ea4ed3c4d) )
	ROM_LOAD16_BYTE( "8.u8",  0x300001, 0x040000,  CRC(32ee1048) SHA1(1b135c200b4440e95a7d1766b4b404ddd238872d) )
ROM_END

ROM_START( barekch ) /* all 27c010 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "1.u1", 0x000001, 0x020000, CRC(a119b5ef) SHA1(710ef6dc340a2c3741af69cd9a3d16e5fdd73be6) )
	ROM_LOAD16_BYTE( "2.u2", 0x000000, 0x020000, CRC(7d4ad276) SHA1(9ab2a28356cc5c36eee8dba40c04a64cf5d2cfde) )
	ROM_LOAD16_BYTE( "3.u3", 0x040001, 0x020000, CRC(af6a9122) SHA1(0f2bac1ad20f5918b04dd5a503121445029e4c84) )
	ROM_LOAD16_BYTE( "4.u4", 0x040000, 0x020000, CRC(98245384) SHA1(f4f96f369764a7d204ec414f053b25da662ff401) )
ROM_END

/******************************************************************************
	Driver inits and protection
******************************************************************************/

static WRITE16_HANDLER( aladbl_w )
{
    /*
    Values returned from the log file :
      - aladbl_w : 1b2a6c - data = 6600 (each time a coin is inserted)
      - aladbl_w : 1b2a82 - data = 0000 (each time a coin is inserted)
      - aladbl_w : 1b2d18 - data = aa00 (only once on reset)
      - aladbl_w : 1b2d42 - data = 0000 (only once on reset)
    */
	log_cb(RETRO_LOG_DEBUG, LOGPRE "aladbl_w : %06x - data = %04x\n",activecpu_get_pc(),data);
}

static READ16_HANDLER( aladbl_r )
{
	if (activecpu_get_pc()==0x1b2a56)
	{
		static UINT16 mcu_port;

		mcu_port = readinputport(3);

		if(mcu_port & 0x100)
			return ((mcu_port & 0x0f) | 0x100); /* coin inserted, calculate the number of coins */
		else
			return (0x100); /* MCU status, needed if you fall into a pitfall */
	}
	if (activecpu_get_pc()==0x1b2a72) return 0x0000;
	if (activecpu_get_pc()==0x1b2d24) return (readinputport(3) & 0x00f0) | 0x1200;    /* difficulty */
	if (activecpu_get_pc()==0x1b2d4e) return 0x0000;

	log_cb(RETRO_LOG_DEBUG, LOGPRE "aladbl_r : %06x\n",activecpu_get_pc());

	return 0x0000;
}

#define ENERGY_CONSOLE_MODE 1

static DRIVER_INIT( aladbl )
{
/*
Game does a check @ 1afc00 with work ram fff57c that makes it play like it was intended
(i.e. 8 energy hits instead of 2) enabled for MAME2003 as it makes the game sweeter to play.
*/
	#if ENERGY_CONSOLE_MODE
	UINT16 *rom = (UINT16 *)memory_region(REGION_CPU1);
	rom[0x1afc08/2] = 0x6600;
	#endif

	/* 220000 = writes to mcu? 330000 = reads? */
	install_mem_write16_handler(0, 0x220000, 0x220001, aladbl_w);
	install_mem_read16_handler(0, 0x330000, 0x330001, aladbl_r);

	acbl_config(0x20, 0, -1, 0);
}

static DRIVER_INIT( barek2ch )
{
	UINT16 *src = (UINT16 *)memory_region(REGION_CPU1);
	int i;

	for (i = 0x000000; i < 0x200000 / 2; i++)
		src[i] = BITSWAP16(src[i], 8, 11, 10, 13, 12, 14, 15, 9, 7, 6, 5, 4, 3, 2, 1, 0);

	src[0x06 / 2] = 0x0210; /* TODO: why is this needed? */

	install_mem_read16_handler(0, 0x380070, 0x380071, input_port_2_word_r );
	install_mem_read16_handler(0, 0x380078, 0x380079, input_port_3_word_r );
	install_mem_read16_handler(0, 0x38007a, 0x38007b, input_port_4_word_r );

	cpu_setbank(3, memory_region(REGION_CPU1) );
	cpu_setbank(4, &genesis_68k_ram[0]);

	acbl_config(0x20, 0, -1, 0);
}

static DRIVER_INIT( barek3 )
{
	data8_t *rom	=	memory_region(REGION_CPU1);
  int x;

	for (x = 0x00001; x < 0x300000; x += 2)
	{
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 6,2,4,0,7,1,3,5);
	}

	install_mem_read16_handler(0, 0x380070, 0x380071, input_port_2_word_r );
	install_mem_read16_handler(0, 0x380078, 0x380079, input_port_3_word_r );

	acbl_config(0x20, 0, -1, 0);
}

static DRIVER_INIT( sonic2mb )
{
   /* 100000 = writes to unpopulated MCU? */
	install_mem_write16_handler(0, 0x100000, 0x100001, aladbl_w);
    install_mem_read16_handler(0, 0x300000, 0x300001, input_port_3_word_r);

	cpu_setbank(3, memory_region(REGION_CPU1) );
	cpu_setbank(4, &genesis_68k_ram[0]);

	acbl_config(0x20, 0, -1, 0);
}

static UINT8 prot_cmd;
static WRITE16_HANDLER( sonic3mb_prot_w )
{
	prot_cmd = data >> 8;
}

static READ16_HANDLER( sonic3mb_prot_r )
{
	UINT16 res = 0;
	switch (prot_cmd)
	{
		/*
    POST, upper 8-bits part is fixed and needed for booting game,
		lower is DSW, cfr. PC=0x16c0/0x16c6 subroutines, lower 4 bits not actually handled by 68k side
    */
		case 0x33: res = 0x0300 | (readinputport(3) & 0xff); break; 
		case 0x00:
			/*
      TODO: coinage
			lower 8-bits is adder for coins (i.e. with 0x202 will add 2 credits to the counter),
			bit 9 is coin state, active high
      */
			res = readinputport(4) & 0x88 ? 0x201 : 0;
			break;
		case 0x66:
			/* 
      handshake or coin status after reading from commands 0x33 or 0x00,
			if bit 0 is high will tight loop until it's low
			we currently go the coin status route to not bother handling coin off manually.
      */
			res = readinputport(4) ? 1 : 0;
			break;
		default:
			log_cb(RETRO_LOG_DEBUG, LOGPRE "Unhandled %04x prot command\n", prot_cmd);
			break;
	}

	return res;
}

static DRIVER_INIT( sonic3mb )
{
	state_save_register_UINT8("genesis", 0, "prot_cmd", &prot_cmd, 1);

   /* 100000 = writes to unpopulated MCU? */
	install_mem_write16_handler(0, 0x200000, 0x200000, sonic3mb_prot_w);
  install_mem_read16_handler (0, 0x300000, 0x300001, sonic3mb_prot_r);

	cpu_setbank(3, memory_region(REGION_CPU1) );
	cpu_setbank(4, &genesis_68k_ram[0]);

	acbl_config(0x20, 0, -1, 0);
}

static READ16_HANDLER( jparkmb_r )
{
	if (activecpu_get_pc()==0x1e327a)
		return (readinputport(4)) ? 0x88 : 0; /* coin hack - should be using PIC instead */
	if (activecpu_get_pc()==0x1e3254) return 0x0000; /* what's this? dips? */
	/* logerror("jparkmb_r : %06x\n",m_maincpu->pc()); */
	return 0x0000;
}

static DRIVER_INIT( jparkmb )
{
   /* 100000 = writes to unpopulated MCU? */
	install_mem_write16_handler(0, 0x100000, 0x100001, aladbl_w);
    install_mem_read16_handler(0, 0x300000, 0x300001, jparkmb_r);

	cpu_setbank(3, memory_region(REGION_CPU1) );
	cpu_setbank(4, &genesis_68k_ram[0]);

	acbl_config(0x20, 0, -1, 0);
}

static READ16_HANDLER( twinktmb_r )
{
	if (activecpu_get_pc()==0x02f81e)
		return (readinputport(4)) ? 0x88 : 0; /* coin hack - should be using PIC instead */

	if (activecpu_get_pc()==0x02f84e) return 0x0000; /* what's this? dips? */

	/* logerror("twinktmb_r : %06x\n",m_maincpu->pc()); */

	return 0x0000;
}

static DRIVER_INIT( twinktmb )
{
	/* boot vectors don't seem to be valid, so they are patched... */
	data8_t *rom = memory_region(REGION_CPU1);
	rom[BYTE_XOR_LE(0x01)] = 0x00;

	rom[BYTE_XOR_LE(0x04)] = 0x00;
	rom[BYTE_XOR_LE(0x07)] = 0x46;
	rom[BYTE_XOR_LE(0x06)] = 0xcc;
	
    /* 100000 = writes to unpopulated MCU? */
	install_mem_write16_handler(0, 0x100000, 0x100001, aladbl_w);
    install_mem_read16_handler(0, 0x300000, 0x300001, twinktmb_r);

	cpu_setbank(3, memory_region(REGION_CPU1) );
	cpu_setbank(4, &genesis_68k_ram[0]);

	acbl_config(0x20, 0, -1, 0);
}


/* The byte descrambles and patches below index the ROM as stored on a
   little endian host, like MAME: BYTE_XOR_LE() keeps them right on big
   endian hosts too. */

/* the scrambled boards: the DIP switches at 0x770070 */
static void acbl_install_dsw(void)
{
	install_mem_read16_handler(0, 0x770070, 0x770075, acbl_dsw_r);
}

/* This should be correct, the areas of the ROM that differ to the original
   after this decode look like intentional changes */
static DRIVER_INIT( mk3mdb )
{
	UINT8 *rom = memory_region(REGION_CPU1);
	int x;

	for (x = 0x000001; x < 0x100001; x += 2)
	{
		if (x & 0x80000)
			rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 0,3,2,5,4,6,7,1);
		else
			rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 4,0,7,1,3,6,2,5);
	}

	for (x = 0x100001; x < 0x400000; x += 2)
	{
		if (x & 0x80000)
			rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 2,7,5,4,1,0,3,6);
		else
			rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 6,1,4,2,7,0,3,5);
	}

	/* HACK: boot vectors don't seem to be valid, so they are patched */
	rom[BYTE_XOR_LE(0x01)] = 0x01;
	rom[BYTE_XOR_LE(0x00)] = 0x00;
	rom[BYTE_XOR_LE(0x03)] = 0x00;
	rom[BYTE_XOR_LE(0x02)] = 0x00;
	rom[BYTE_XOR_LE(0x05)] = 0x00;
	rom[BYTE_XOR_LE(0x04)] = 0x00;
	rom[BYTE_XOR_LE(0x07)] = 0x02;
	rom[BYTE_XOR_LE(0x06)] = 0x10;

	acbl_install_dsw();
	acbl_config(0xa0, 1, 2, 6);
	acbl_iack = 1;
}

static DRIVER_INIT( ssf2mdb )
{
	acbl_config(0x20, 1, 2, 6);
	acbl_iack = 1;
}

static DRIVER_INIT( srmdb )
{
	UINT8 *rom = memory_region(REGION_CPU1);
	int x;

	for (x = 0x00001; x < 0x40000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 5,1,6,2,4,3,7,0);

	for (x = 0x40001; x < 0x80000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 2,6,1,5,0,7,3,4);

	/* HACK: boot vectors don't seem to be valid, so they are patched */
	rom[BYTE_XOR_LE(0x01)] = 0x01;
	rom[BYTE_XOR_LE(0x00)] = 0x00;
	rom[BYTE_XOR_LE(0x03)] = 0x00;
	rom[BYTE_XOR_LE(0x02)] = 0x00;

	rom[BYTE_XOR_LE(0x06)] = 0xd2;
	rom[BYTE_XOR_LE(0x07)] = 0x00;

	acbl_install_dsw();
	acbl_config(0xa0, 0, 2, 0);
	acbl_iack = 1;
}

static DRIVER_INIT( barekch )
{
	UINT16 *src = (UINT16 *)memory_region(REGION_CPU1);
	int i;

	for (i = 0x000000; i < 0x80000 / 2; i++)
		src[i] = BITSWAP16(src[i] ^ 0xff00, 15, 9, 12, 8, 14, 13, 11, 10, 7, 6, 5, 4, 3, 2, 1, 0);

	acbl_install_dsw();
	acbl_config(0x20, 1, 2, 6);
	acbl_iack = 1;
}

static DRIVER_INIT( bk3ssrmb )
{
	UINT8 *rom = memory_region(REGION_CPU1);
	int x;

	for (x = 0x00001; x < 0x80000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 3, 1, 6, 4, 7, 0, 2, 5);

	for (x = 0x80001; x < 0x100000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 3, 7, 0, 5, 1, 6, 2, 4);

	for (x = 0x100001; x < 0x300000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 1, 7, 6, 4, 5, 2, 3, 0);

	for (x = 0x300001; x < 0x380000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 3, 1, 6, 4, 7, 0, 2, 5);

	acbl_install_dsw();
	acbl_config(0x20, 1, 2, 6);
	acbl_iack = 1;
}

static DRIVER_INIT( srssf2mb )
{
	UINT8 *rom = memory_region(REGION_CPU1);
	int x;

	for (x = 0x00001; x < 0x100000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 3, 1, 6, 4, 7, 0, 2, 5);

	for (x = 0x100001; x < 0x180000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 2, 4, 0, 7, 1, 3, 5, 6);

	for (x = 0x180001; x < 0x200000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 3, 7, 0, 5, 1, 6, 2, 4);

	for (x = 0x200001; x < 0x700000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 1, 7, 6, 4, 5, 2, 3, 0);

	acbl_install_dsw();
	acbl_config(0x20, 1, 2, 6);
	acbl_iack = 1;
}

/* the coins of biohzdmb at 0x770078 */
static READ16_HANDLER( biohzdmb_coins_r )
{
	return readinputport(2);
}

static DRIVER_INIT( biohzdmb )
{
	UINT8 *rom = memory_region(REGION_CPU1);
	int x;

	for (x = 0x00001; x < 0x80000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)] ^ 0xff, 0, 3, 2, 5, 4, 6, 7, 1);

	for (x = 0x80001; x < 0x100000; x += 2)
		rom[BYTE_XOR_LE(x)] = BITSWAP8(rom[BYTE_XOR_LE(x)], 6, 4, 0, 5, 1, 3, 2, 7);

	/* HACK: boot vectors don't seem to be valid, so they are patched (as in MAME) */
	rom[BYTE_XOR_LE(0x04)] = 0x00;
	rom[BYTE_XOR_LE(0x05)] = 0x00;
	rom[BYTE_XOR_LE(0x06)] = 0x04;
	rom[BYTE_XOR_LE(0x07)] = 0x02;

	acbl_install_dsw();
	install_mem_read16_handler(0, 0x770078, 0x770079, biohzdmb_coins_r);
	acbl_config(0x20, 0, -1, 0);
	acbl_iack = 1;
}

static DRIVER_INIT( barek2mb )
{
#ifdef MSB_FIRST
	/* the PIC core fetches its 12 bit opcodes as native 16 bit words, but
	   its 8 bit data bus leaves the ROM region in the file's (little
	   endian) byte order */
	UINT8 *pic = memory_region(REGION_CPU3) + PIC16C57_PGM_OFFSET;
	int i;

	for (i = 0; i < 0x1000; i += 2)
	{
		UINT8 t = pic[i];
		pic[i] = pic[i + 1];
		pic[i + 1] = t;
	}
#endif

	acbl_config(0x20, 0, -1, 0);
	acbl_iack = 1;
}

/* missing PIC dump, simulated as in MAME */
static READ16_HANDLER( barek3mba_r )
{
	if (activecpu_get_pc() == 0x4dbc6)
		return 0x0300;

	if (activecpu_get_pc() == 0x4dc34)
		return 0x0201 ^ ((readinputport(2) & 0x01) << 9); /* as in MAME it gives 9 credits at start up */

	/* handshake flag for the previous command */
	if (activecpu_get_pc() == 0x4dc5a)
		return (readinputport(2) & 0x01) ^ 1;

	return 0x0000;
}

static DRIVER_INIT( barek3a )
{
	install_mem_read16_handler(0, 0x300000, 0x300001, barek3mba_r);

	acbl_config(0x20, 0, -1, 0);
	acbl_iack = 1;
}

/******************************************************************************
	Game drivers
******************************************************************************/

/* with PIC protection */
GAME ( 1993, aladmdb,   0,        megadrvb,    aladbl,   aladbl,   ROT0, "bootleg / Sega",   "Aladdin (bootleg of Mega Drive version)" )
GAME ( 1993, sonic2mb,  0,        md_bootleg,  sonic2mb, sonic2mb, ROT0, "bootleg / Sega",   "Sonic The Hedgehog 2 (bootleg of Mega Drive version)" )
GAMEX( 1993, sonic3mb,  0,        md_bootleg,  sonic3mb, sonic3mb, ROT0, "bootleg / Sega",   "Sonic The Hedgehog 3 (bootleg of Mega Drive version)", GAME_IMPERFECT_GRAPHICS )
GAME ( 1994, barek2mb,  0,        md_boot_mcu, barek2,   barek2mb, ROT0, "bootleg / Sega",   "Bare Knuckle II (bootleg of Mega Drive version)" )
GAMEX( 1994, barek3mba, barek3mb, megadrvb,    barek3a,  barek3a,  ROT0, "bootleg / Sega",   "Bare Knuckle III (bootleg of Mega Drive version)", GAME_NOT_WORKING )
GAME ( 1993, twinktmb,  0,        md_bootleg,  jparkmb,  twinktmb, ROT0, "bootleg / Sega",   "Twinkle Tale (bootleg of Mega Drive version)" )
GAME ( 1993, jparkmb,   0,        md_bootleg,  jparkmb,  jparkmb,  ROT0, "bootleg / Sega",   "Jurassic Park (bootleg of Mega Drive version)" )

/* with scrambled ROMs */
GAME ( 1994, barekch,   0,        megadrvb_6b, barekch,  barekch,  ROT0, "bootleg",          "Bare Knuckle (scrambled bootleg of Mega Drive version)" )
GAMEX( 1994, bk3ssrmb,  0,        megadrvb_6b, bk3ssrmb, bk3ssrmb, ROT0, "bootleg / Sega",   "Bare Knuckle III / Sunset Riders (scrambled bootleg of Mega Drive versions)", GAME_NOT_WORKING ) /* boots as Bare Knuckle III, the game switch is not emulated (as in MAME) */
GAMEX( 1994, srssf2mb,  0,        megadrvb_6b, bk3ssrmb, srssf2mb, ROT0, "bootleg / Sega",   "Sunset Riders / Super Street Fighter II - The New Challengers (scrambled bootleg of Mega Drive versions)", GAME_NOT_WORKING ) /* the game switch is not emulated (as in MAME) */
GAME ( 1995, biohzdmb,  0,        megadrvb,    biohzdmb, biohzdmb, ROT0, "bootleg / Sega",   "Bio-Hazard Battle (scrambled bootleg of Mega Drive version)" )
GAME ( 1996, mk3mdb,    0,        megadrvb_6b, mk3mdb,   mk3mdb,   ROT0, "bootleg / Midway", "Mortal Kombat 3 (scrambled bootleg of Mega Drive version)" )
GAME ( 1994, ssf2mdb,   0,        ssf2mdb,     ssf2mdb,  ssf2mdb,  ROT0, "bootleg / Capcom", "Super Street Fighter II - The New Challengers (scrambled bootleg of Mega Drive version)" )
GAME ( 1993, srmdb,     0,        megadrvb,    srmdb,    srmdb,    ROT0, "bootleg / Konami", "Sunset Riders (scrambled bootleg of Mega Drive version)" )
GAME ( 1994, barek3mb,  0,        megadrvb,    barek3,   barek3,   ROT0, "bootleg / Sega",   "Bare Knuckle III (scrambled bootleg of Mega Drive version)" )
GAME ( 1994, barek2ch,  0,        md_bootleg,  barek2ch, barek2ch, ROT0, "bootleg",          "Bare Knuckle II (scrambled bootleg of Mega Drive version)" )
