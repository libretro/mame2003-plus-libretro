/***************************************************************************

	Sega Mega Play

	A Mega Drive (Genesis) based board with a Z80 BIOS (Sega Master System
	VDP for its screens, an SN76489, battery-backed RAM) that handles the
	coins and credits, the game instructions and the game selection.

	Moved here from drivers/segac2.c, as in current MAME (megaplay.cpp);
	the Mega Drive part (68000, sound Z80, I/O, interrupts) is shared with
	the other Mega Drive based boards (includes/segac2.h), the VDP is in
	vidhrdw/segavdp.c and the video glue in vidhrdw/segac2_vidhrdw.c.

	Set names as in current MAME (mp_twc is now mp_twcup).

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "segac2.h"

/******************************************************************************
	The BIOS
******************************************************************************/

#define MP_ROM  0x10
#define MP_GAME 0

static unsigned int bios_bank;				/* ROM bank selection */
static unsigned short game_banksel;			/* game bank selection */
static unsigned int bios_mode = MP_ROM;		/* whether ROM banks or game data are read from 0x8000-0xffff */
static unsigned int bios_width;				/* determines the way the game info ROM is read */
static unsigned char bios_6600;
static unsigned char bios_6403;
static unsigned char bios_6404;
static unsigned char *ic3_ram;				/* battery-backed RAM (credits, bookkeeping, high scores) */
static unsigned char ic37_ram[0x8000];
static unsigned int readpos = 1;			/* serial bank selection position (9-bit) */
static data16_t *ic36_ram;					/* the BIOS RAM, also seen by the 68000 at 0xa02000 */

static void megaplay_state_register(void)
{
	state_save_register_UINT32("megaplay", 0, "bios_bank", (UINT32 *)&bios_bank, 1);
	state_save_register_UINT16("megaplay", 0, "game_banksel", (UINT16 *)&game_banksel, 1);
	state_save_register_UINT32("megaplay", 0, "bios_mode", (UINT32 *)&bios_mode, 1);
	state_save_register_UINT32("megaplay", 0, "bios_width", (UINT32 *)&bios_width, 1);
	state_save_register_UINT32("megaplay", 0, "readpos", (UINT32 *)&readpos, 1);
	state_save_register_UINT8 ("megaplay", 0, "bios_6600", &bios_6600, 1);
	state_save_register_UINT8 ("megaplay", 0, "bios_6403", &bios_6403, 1);
	state_save_register_UINT8 ("megaplay", 0, "bios_6404", &bios_6404, 1);
	state_save_register_UINT8 ("megaplay", 0, "ic37_ram", ic37_ram, 0x8000);
}

/* the 68000 side of the Z80 area and of the I/O chip */
static WRITE16_HANDLER ( megaplay_68k_to_z80_w )
{
	offset *= 2;
	offset &= 0x7fff;

	/* Shared Ram */
	if ((offset >= 0x0000) && (offset <= 0x1fff))
	{
		offset &=0x1fff;

	if (ACCESSING_LSB) genesis_z80_ram[offset+1] = data & 0xff;
	if (ACCESSING_MSB) genesis_z80_ram[offset] = (data >> 8) & 0xff;
	}

	if ((offset >= 0x2000) && (offset <= 0x3fff))
	{
		offset &=0x1fff;

	if (ACCESSING_LSB) ic36_ram[offset+1] = data & 0xff;
	if (ACCESSING_MSB) ic36_ram[offset] = (data >> 8) & 0xff;
	}


	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0:
			if (ACCESSING_MSB)	YM2612_control_port_0_A_w	(0,	(data >> 8) & 0xff);
			else 				YM2612_data_port_0_A_w		(0,	(data >> 0) & 0xff);
			break;
		case 2:
			if (ACCESSING_MSB)	YM2612_control_port_0_B_w	(0,	(data >> 8) & 0xff);
			else 				YM2612_data_port_0_B_w		(0,	(data >> 0) & 0xff);
			break;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{

	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{
		offset &= 0x1f;

		if ( (offset >= 0x10) && (offset <=0x17) )
		{
			if (ACCESSING_LSB) SN76496_0_w(0, data & 0xff);
			if (ACCESSING_MSB) SN76496_0_w(0, (data >>8) & 0xff);
		}

	}
}

static READ16_HANDLER ( megaplay_68k_to_z80_r )
{
	offset *= 2;
	offset &= 0x7fff;

	/* Shared Ram */
	if ((offset >= 0x0000) && (offset <= 0x1fff))
	{
		offset &=0x1fff;
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "soundram_r returning %x\n",(gen_z80_shared[offset] << 8) + gen_z80_shared[offset+1]); */
		return (genesis_z80_ram[offset] << 8) + genesis_z80_ram[offset+1];
	}

	if ((offset >= 0x2000) && (offset <= 0x3fff))
	{
		offset &=0x1fff;
/*		if(offset == 0) */
/*			return (readinputport(8) << 8) ^ 0xff00; */
		return (ic36_ram[offset] << 8) + ic36_ram[offset+1];
	}


	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0:
			if (ACCESSING_MSB)	 return YM2612_status_port_0_A_r(0) << 8;
			else 				 return YM2612_read_port_0_r(0);
			break;
		case 2:
			if (ACCESSING_MSB)	return YM2612_status_port_0_B_r(0) << 8;
			else 				return 0;
			break;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{

	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{

	}

	return 0x0000;
}

READ16_HANDLER ( megaplay_genesis_io_r )
{
	/* 8-bit only, data is mirrored in both halves */

	UINT8 return_value = 0;

	switch (offset)
	{
		case 0:
		/* Charles MacDonald ( http://cgfm2.emuviews.com/ )
		    D7 : Console is 1= Export (USA, Europe, etc.) 0= Domestic (Japan)
		    D6 : Video type is 1= PAL, 0= NTSC
		    D5 : Sega CD unit is 1= not present, 0= connected.
		    D4 : Unused (always returns zero)
		    D3 : Bit 3 of version number
		    D2 : Bit 2 of version number
		    D1 : Bit 1 of version number
		    D0 : Bit 0 of version number
		*/
			return_value = 0x80; /* ? megatech is usa? */
			break;

		case 1: /* port A data (joypad 1) */

			if (genesis_io_ram[offset] & 0x40)
				return_value = readinputport(1) & (genesis_io_ram[4]^0xff);
			else
			{
				/* the pad's Start line is driven by the BIOS (bit 4 of $6203,
				   active low), not by the Start button: the BIOS reads the button
				   on its own port and passes it on only when the game may start
				   (Mazin Wars expects exactly this). Same as current MAME. */
				return_value = (readinputport(2) & ~0x20 & 0xff) | ((bios_bank & 0x10) ? 0x20 : 0x00);
				return_value &= (genesis_io_ram[4]^0xff);
				return_value |= readinputport(1) & 0x03;
			}
			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "reading joypad 1 , type %02x %02x\n",genesis_io_ram[offset] & 0xb0, return_value &0x7f); */
			break;

		case 2: /* port B data (joypad 2) */

			if (genesis_io_ram[offset] & 0x40)
				return_value = readinputport(3) & (genesis_io_ram[5]^0xff);
			else
			{
				/* P2 Start from the BIOS too (bit 5 of $6203) */
				return_value = (readinputport(4) & ~0x20 & 0xff) | ((bios_bank & 0x20) ? 0x20 : 0x00);
				return_value &= (genesis_io_ram[5]^0xff);
				return_value |= readinputport(3) & 0x03;
			}
			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "reading joypad 2 , type %02x %02x\n",genesis_io_ram[offset] & 0xb0, return_value &0x7f); */
			break;

/*		case 3: */ /* port C data */ 
/*			return_value = bios_6402 << 3; */
/*			break; */

	default:
			return_value = genesis_io_ram[offset];

	}
	return return_value | return_value << 8;
}


static READ_HANDLER( megaplay_bios_banksel_r )
{
	return bios_bank;
}

static WRITE_HANDLER( megaplay_bios_banksel_w )
{
/*	Multi-slot note:
	Bits 0 and 1 appear to determine the selected game slot.
	It should be possible to multiplex different game ROMs at
	0x000000-0x3fffff based on these bits.
*/
	bios_bank = data;
	bios_mode = MP_ROM;
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: ROM bank %i selected [0x%02x]\n",bios_bank >> 6, data); */
}

static READ_HANDLER( megaplay_bios_gamesel_r )
{
	return bios_6403;
}

static WRITE_HANDLER( megaplay_bios_gamesel_w )
{
	bios_6403 = data;

/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: 0x6403 write: 0x%02x\n",data); */
	bios_mode = data & 0x10;
}

static READ_HANDLER( bank_r )
{
	data8_t* bank = memory_region(REGION_CPU3);
	data8_t* game = memory_region(REGION_CPU1);

	if(game_banksel == 0x142) /* Genesis I/O*/
		return megaplay_genesis_io_r((offset/2) & 0x1f, 0xffff);

	if(bios_mode & MP_ROM)
	{
		int sel = (bios_bank >> 6) & 0x03;

/*		usrintf_showmessage("Reading from Bank %i",sel);*/

		if(sel == 0)
			return 0xff;
		else
			return bank[0x10000 + (sel-1)*0x8000 + offset];
	}
	else
	{
		if(game_banksel == 0x60 || game_banksel == 0x61)  /* read game info ROM */
			if(bios_width & 0x08)
			{
				if(offset >= 0x2000)
					return ic36_ram[offset - 0x2000];
				else
					return ic37_ram[(0x2000 * (bios_bank & 0x03)) + offset];
			}
			else
				return game[((game_banksel)*0x8000 + offset)];
		else
			return game[(game_banksel*0x8000 + (offset ^ 0x01))];
	}
}

static WRITE_HANDLER ( bank_w )
{
	if(game_banksel == 0x142) /* Genesis I/O*/
		genesis_io_w((offset/2) & 0x1f, data, 0xffff);

	if(offset <= 0x1fff && (bios_width & 0x08))
		ic37_ram[(0x2000 * (bios_bank & 0x03)) + offset] = data;

	if(offset >= 0x2000 && (bios_width & 0x08))
/*		ic36_ram[offset] = data; */
		ic36_ram[offset - 0x2000] = data;
}


static READ_HANDLER( megaplay_bios_6402_r )
{
	return genesis_io_ram[3];/* & 0xfe; */
/*	return bios_6402; & 0xfe;*/
}

static WRITE_HANDLER( megaplay_bios_6402_w )
{
	genesis_io_ram[3] = (genesis_io_ram[3] & 0x07) | ((data & 0x70) >> 1);
/*	bios_6402 = (data >> 4) & 0x07; */
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: 0x6402 write: 0x%02x\n",data); */
}

static READ_HANDLER( megaplay_bios_6404_r )
{
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: 0x6404 read: returned 0x%02x\n",bios_6404 | (bios_6403 & 0x10) >> 4); */
	return (bios_6404 & 0xfe) | ((bios_6403 & 0x10) >> 4);
/*	return bios_6404 | (bios_6403 & 0x10) >> 4; */
}

static WRITE_HANDLER( megaplay_bios_6404_w )
{
	if(((bios_6404 & 0x0c) == 0x00) && ((data & 0x0c) == 0x0c))
		cpu_set_reset_line(0, PULSE_LINE);
	bios_6404 = data;

/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: 0x6404 write: 0x%02x\n",data); */
}

static READ_HANDLER( megaplay_bios_6204_r )
{
	return (genesis_io_ram[3]);
/*	return (bios_width & 0xf8) + (bios_6204 & 0x07); */
}

static WRITE_HANDLER( megaplay_bios_width_w )
{
	bios_width = data;
	genesis_io_ram[3] = (genesis_io_ram[3] & 0x07) | ((data & 0xf8));

/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: 0x6204 - Width write: %02x\n",data); */
}

static READ_HANDLER( megaplay_bios_6600_r )
{
/*	Multi-slot note:
	0x6600 appears to be used to check for extra slots being used.
	Enter the following line in place of the return statement in this
	function to make the BIOS check all 4 slots (3 and 4 will be "not used")
		return (bios_6600 & 0xfe) | (bios_bank & 0x01);
*/
	return bios_6600;/* & 0xfe; */
}

static WRITE_HANDLER( megaplay_bios_6600_w )
{
	bios_6600 = data;
/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS: 0x6600 write: 0x%02x\n",data); */
}

static WRITE_HANDLER( megaplay_game_w )
{
	if(readpos == 1)
		game_banksel = 0;
	game_banksel |= (1 << (readpos-1)) * (data & 0x01);

	readpos++;
	if(readpos > 9)
	{
		bios_mode = MP_GAME;
		readpos = 1;
/*		usrintf_showmessage("Game bank selected: 0x%03x",game_banksel); */
		log_cb(RETRO_LOG_DEBUG, LOGPRE "BIOS [0x%04x]: 68K address space bank selected: 0x%03x\n",activecpu_get_previouspc(),game_banksel);
	}
}

/******************************************************************************
	Memory maps
******************************************************************************/

static MEMORY_READ16_START( megaplay_genesis_readmem )
	{ 0x000000, 0x3fffff, MRA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, megaplay_genesis_io_r },				/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa00000, 0xa0ffff, megaplay_68k_to_z80_r },
	{ 0xc00000, 0xc0001f, segavdp_r },				/* VDP Access */
	{ 0xfe0000, 0xfeffff, MRA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MRA16_RAM },					/* Main Ram */
MEMORY_END

static MEMORY_WRITE16_START( megaplay_genesis_writemem )
	{ 0x000000, 0x3fffff, MWA16_ROM },					/* Cartridge Program Rom */
	{ 0xa10000, 0xa1001f, genesis_io_w, &genesis_io_ram },				/* Genesis Input */
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa00000, 0xa0ffff, megaplay_68k_to_z80_w, &ic36_ram }, /* ok.?? */
	{ 0xc00000, 0xc0000f, segavdp_w },				/* VDP Access */
	{ 0xc00010, 0xc00017, sn76489_w },					/* SN76489 Access */
	{ 0xfe0000, 0xfeffff, MWA16_BANK3 },				/* Main Ram */
	{ 0xff0000, 0xffffff, MWA16_RAM, &genesis_68k_ram },/* Main Ram */
MEMORY_END

static MEMORY_READ_START( megaplay_z80_readmem )
	{ 0x0000, 0x1fff, MRA_BANK1 },
 	{ 0x2000, 0x3fff, MRA_BANK2 },
	{ 0x4000, 0x7fff, genesis_z80_r },
	{ 0x8000, 0xffff, genesis_z80_bank_r },
MEMORY_END

static MEMORY_WRITE_START( megaplay_z80_writemem )
	{ 0x0000, 0x1fff, MWA_BANK1, &genesis_z80_ram },
 	{ 0x2000, 0x3fff, MWA_BANK2 },
	{ 0x4000, 0x7fff, genesis_z80_w },
 /*	{ 0x8000, 0xffff, genesis_z80_bank_w }, */
MEMORY_END

static MEMORY_READ_START(megaplay_bios_readmem)
 	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x4fff, MRA_RAM },
	{ 0x5000, 0x5fff, MRA_RAM },
	{ 0x6200, 0x6200, input_port_7_r },
	{ 0x6201, 0x6201, input_port_8_r },
	{ 0x6400, 0x6400, input_port_5_r },
	{ 0x6401, 0x6401, input_port_6_r },
	{ 0x6204, 0x6204, megaplay_bios_6204_r },
	{ 0x6203, 0x6203, megaplay_bios_banksel_r },
	{ 0x6402, 0x6402, megaplay_bios_6402_r },
	{ 0x6403, 0x6403, megaplay_bios_gamesel_r },
	{ 0x6404, 0x6404, megaplay_bios_6404_r },
	{ 0x6600, 0x6600, megaplay_bios_6600_r },
	{ 0x6800, 0x77ff, MRA_RAM },
	{ 0x8000, 0xffff, bank_r },
MEMORY_END

static MEMORY_WRITE_START(megaplay_bios_writemem)
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x4fff, MWA_RAM },
	{ 0x5000, 0x5fff, MWA_RAM },
	{ 0x6000, 0x6000, megaplay_game_w },
	{ 0x6203, 0x6203, megaplay_bios_banksel_w },
	{ 0x6204, 0x6204, megaplay_bios_width_w },
	{ 0x6402, 0x6402, megaplay_bios_6402_w },
	{ 0x6403, 0x6403, megaplay_bios_gamesel_w },
	{ 0x6404, 0x6404, megaplay_bios_6404_w },
	{ 0x6600, 0x6600, megaplay_bios_6600_w },
	{ 0x6001, 0x67ff, MWA_RAM },
	{ 0x6800, 0x77ff, MWA_RAM, &ic3_ram },
	{ 0x8000, 0xffff, bank_w },
MEMORY_END

static PORT_READ_START( megaplay_bios_readport )
/*	{ 0xdc, 0xdc, megatech_bios_port_dc_r },  */ /* player inputs*/
/*	{ 0xdd, 0xdd, megatech_bios_port_dd_r },  */ /* other player 2 inputs*/
	{ 0xbe, 0xbf, megatech_bios_port_be_bf_r },			/* VDP */
PORT_END

static PORT_WRITE_START( megaplay_bios_writeport )
/*	{ 0x3f, 0x3f, megatech_bios_port_ctrl_w },*/
	{ 0x7f, 0x7f, SN76496_1_w },	/* SN76489 */
	{ 0xbe, 0xbf, megatech_bios_port_be_bf_w },			/* VDP */
PORT_END

/******************************************************************************
	Machine init
******************************************************************************/

static MACHINE_INIT( megaplay )
{
/*	unsigned char* ram = memory_region(REGION_CPU3); */

	/* mirroring of ram etc. */
	cpu_setbank(1, &genesis_z80_ram[0]);
	cpu_setbank(2, &ic36_ram[0]);
	cpu_setbank(3, &genesis_68k_ram[0]);

	machine_init_genesis();
	genesis_enable_iack();

	/* save states (repeated registrations are ignored) */
	megaplay_state_register();
}

/******************************************************************************
	Input ports
******************************************************************************/

#define MEGAPLAY_TEST \
	PORT_START \
    PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_SERVICE2, "Select", KEYCODE_0, JOYCODE_NONE ) \
    PORT_BITX( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN, "0x6400 bit 1", KEYCODE_W, JOYCODE_NONE ) \
    PORT_BITX( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN, "0x6400 bit 2", KEYCODE_E, JOYCODE_NONE ) \
    PORT_BITX( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN, "0x6400 bit 3", KEYCODE_R, JOYCODE_NONE ) \
    PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6400 bit 4", KEYCODE_T, JOYCODE_NONE ) \
    PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6400 bit 5", KEYCODE_Y, JOYCODE_NONE ) \
	PORT_BITX( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN, "0x6400 bit 6", KEYCODE_U, JOYCODE_NONE ) \
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_SERVICE, "Test", KEYCODE_F2, JOYCODE_NONE )

#define MEGAPLAY_COIN \
	PORT_START \
 	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2 ) \
    PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN ) \
    PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN ) \
    PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
    PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
    PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_START1 ) \
    PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_START2 )

#define MEGAPLAY_DSWA \
	PORT_START \
	PORT_DIPNAME( 0x0f, 0x0f, "Coin slot 1" ) \
    PORT_DIPSETTING( 0x07, DEF_STR( 4C_1C ) ) \
    PORT_DIPSETTING( 0x08, DEF_STR( 3C_1C ) ) \
    PORT_DIPSETTING( 0x09, DEF_STR( 2C_1C ) ) \
    PORT_DIPSETTING( 0x05, "2 coins/1 credit - 5 coins/3 credits - 6 coins/4 credits" ) \
    PORT_DIPSETTING( 0x04, "2 coins/1 credit - 4 coins/3 credits" ) \
    PORT_DIPSETTING( 0x0f, DEF_STR( 1C_1C ) ) \
    PORT_DIPSETTING( 0x06, DEF_STR( 2C_3C ) ) \
    PORT_DIPSETTING( 0x0e, DEF_STR( 1C_2C ) ) \
    PORT_DIPSETTING( 0x0d, DEF_STR( 1C_3C ) ) \
    PORT_DIPSETTING( 0x0c, DEF_STR( 1C_4C ) ) \
    PORT_DIPSETTING( 0x0b, DEF_STR( 1C_5C ) ) \
    PORT_DIPSETTING( 0x0a, DEF_STR( 1C_6C ) ) \
    PORT_DIPSETTING( 0x03, "1 coin/1 credit - 5 coins/6 credits" ) \
    PORT_DIPSETTING( 0x02, "1 coin/1 credit - 4 coins/5 credits" ) \
    PORT_DIPSETTING( 0x01, "1 coin/1 credit - 2 coins/3 credits" ) \
    PORT_DIPSETTING( 0x00, DEF_STR( Free_Play ) )

#define MEGAPLAY_DSWB \
	PORT_DIPNAME( 0xf0, 0xf0, "Coin slot 2" ) \
    PORT_DIPSETTING( 0x70, DEF_STR( 4C_1C ) ) \
    PORT_DIPSETTING( 0x80, DEF_STR( 3C_1C ) ) \
    PORT_DIPSETTING( 0x90, DEF_STR( 2C_1C ) ) \
    PORT_DIPSETTING( 0x50, "2 coins/1 credit - 5 coins/3 credits - 6 coins/4 credits" ) \
    PORT_DIPSETTING( 0x40, "2 coins/1 credit - 4 coins/3 credits" ) \
    PORT_DIPSETTING( 0xf0, DEF_STR( 1C_1C ) ) \
    PORT_DIPSETTING( 0x60, DEF_STR( 2C_3C ) ) \
    PORT_DIPSETTING( 0xe0, DEF_STR( 1C_2C ) ) \
    PORT_DIPSETTING( 0xd0, DEF_STR( 1C_3C ) ) \
    PORT_DIPSETTING( 0xc0, DEF_STR( 1C_4C ) ) \
    PORT_DIPSETTING( 0xb0, DEF_STR( 1C_5C ) ) \
    PORT_DIPSETTING( 0xa0, DEF_STR( 1C_6C ) ) \
    PORT_DIPSETTING( 0x30, "1 coin/1 credit - 5 coins/6 credits" ) \
    PORT_DIPSETTING( 0x20, "1 coin/1 credit - 4 coins/5 credits" ) \
    PORT_DIPSETTING( 0x10, "1 coin/1 credit - 2 coins/3 credits" ) \
    PORT_DIPSETTING( 0x00, " 1 coin/1 credit" )

INPUT_PORTS_START ( megaplay )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

INPUT_PORTS_END

INPUT_PORTS_START ( mp_sonic )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings)*/
	PORT_DIPNAME( 0x03, 0x01, "Initial Players" )
    PORT_DIPSETTING( 0x00, "4" )
    PORT_DIPSETTING( 0x01, "3" )
    PORT_DIPSETTING( 0x02, "2" )
    PORT_DIPSETTING( 0x03, "1" )

	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
    PORT_DIPSETTING( 0x00, "Hardest" )
    PORT_DIPSETTING( 0x04, "Hard" )
    PORT_DIPSETTING( 0x08, "Easy" )
    PORT_DIPSETTING( 0x0c, "Normal" )
    /* Who knows...*/
/*	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 4", KEYCODE_G, JOYCODE_NONE )*/
/*  PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 5", KEYCODE_H, JOYCODE_NONE )*/
/*	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 6", KEYCODE_J, JOYCODE_NONE )*/
/*	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 7", KEYCODE_K, JOYCODE_NONE )*/

INPUT_PORTS_END

INPUT_PORTS_START ( mp_gaxe2 )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings)*/
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Difficulty ) )
    PORT_DIPSETTING( 0x01, "Normal" )
    PORT_DIPSETTING( 0x00, "Hard" )

	PORT_DIPNAME( 0x02, 0x00, "Life" )
    PORT_DIPSETTING( 0x02, "1" )
    PORT_DIPSETTING( 0x00, "2" )

	PORT_DIPNAME( 0x04, 0x04, "Initial Players" )
    PORT_DIPSETTING( 0x00, "1" )
    PORT_DIPSETTING( 0x04, "2" )

	PORT_DIPNAME( 0x08, 0x00, "Timer" )
    PORT_DIPSETTING( 0x08, DEF_STR( Off )  )
    PORT_DIPSETTING( 0x00, DEF_STR( On ) )

    /* Who knows...*/
/*	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 4", KEYCODE_G, JOYCODE_NONE )*/
/*  PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 5", KEYCODE_H, JOYCODE_NONE )*/
/*	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 6", KEYCODE_J, JOYCODE_NONE )*/
/*	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN, "0x6201 bit 7", KEYCODE_K, JOYCODE_NONE )*/

INPUT_PORTS_END

INPUT_PORTS_START ( mp_twcup )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x01, 0x01, "Time" )
    PORT_DIPSETTING( 0x01, "Normal" )
    PORT_DIPSETTING( 0x00, "Short" )

	PORT_DIPNAME( 0x0e, 0x08, "Level" )
    PORT_DIPSETTING( 0x00, "0" )
    PORT_DIPSETTING( 0x02, "0" )
    PORT_DIPSETTING( 0x04, "5" )
    PORT_DIPSETTING( 0x06, "4" )
    PORT_DIPSETTING( 0x08, "3" )
    PORT_DIPSETTING( 0x0a, "2" )
    PORT_DIPSETTING( 0x0c, "1" )
    PORT_DIPSETTING( 0x0e, "0" )

INPUT_PORTS_END

INPUT_PORTS_START ( mp_soni2 )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x01, "Initial Players (Normal mode)" )
    PORT_DIPSETTING( 0x00, "4" )
    PORT_DIPSETTING( 0x01, "3" )
    PORT_DIPSETTING( 0x02, "2" )
    PORT_DIPSETTING( 0x03, "1" )

	PORT_DIPNAME( 0x0c, 0x0c, "Initial Players (Dual mode)" )
    PORT_DIPSETTING( 0x00, "4" )
    PORT_DIPSETTING( 0x04, "2" )
    PORT_DIPSETTING( 0x08, "1" )
    PORT_DIPSETTING( 0x0c, "3" )

INPUT_PORTS_END

INPUT_PORTS_START ( mp_sor2 )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
    PORT_DIPSETTING( 0x00, "4" )
    PORT_DIPSETTING( 0x01, "3" )
    PORT_DIPSETTING( 0x02, "2" )
    PORT_DIPSETTING( 0x03, "1" )

	PORT_DIPNAME( 0xc, 0x0c, DEF_STR ( Difficulty ) )
    PORT_DIPSETTING( 0x00, "Hardest" )
    PORT_DIPSETTING( 0x04, "Hard" )
    PORT_DIPSETTING( 0x08, "Easy" )
    PORT_DIPSETTING( 0x0c, "Normal" )

INPUT_PORTS_END

INPUT_PORTS_START ( mp_bio )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
    PORT_DIPSETTING( 0x00, "5" )
    PORT_DIPSETTING( 0x01, "4" )
    PORT_DIPSETTING( 0x02, "2" )
    PORT_DIPSETTING( 0x03, "3" )

	PORT_DIPNAME( 0xc, 0x0c, DEF_STR ( Difficulty ) )
    PORT_DIPSETTING( 0x00, "Hardest" )
    PORT_DIPSETTING( 0x04, "Hard" )
    PORT_DIPSETTING( 0x08, "Easy" )
    PORT_DIPSETTING( 0x0c, "Normal" )

INPUT_PORTS_END

INPUT_PORTS_START ( mp_shnb3 )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
    PORT_DIPSETTING( 0x00, "4" )
    PORT_DIPSETTING( 0x01, "3" )
    PORT_DIPSETTING( 0x02, "2" )
    PORT_DIPSETTING( 0x03, "1" )

	PORT_DIPNAME( 0xc, 0x0c, DEF_STR ( Difficulty ) )
    PORT_DIPSETTING( 0x00, "Expert" )
    PORT_DIPSETTING( 0x04, "Hard" )
    PORT_DIPSETTING( 0x08, "Easy" )
    PORT_DIPSETTING( 0x0c, "Normal" )
INPUT_PORTS_END

INPUT_PORTS_START ( mp_gunhe )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x01, "Initial Players" )
    PORT_DIPSETTING( 0x00, "4" )
    PORT_DIPSETTING( 0x01, "3" )
    PORT_DIPSETTING( 0x02, "2" )
    PORT_DIPSETTING( 0x03, "1" )

	PORT_DIPNAME( 0xc, 0x0c, DEF_STR ( Difficulty ) )
    PORT_DIPSETTING( 0x00, "Expert" )
    PORT_DIPSETTING( 0x04, "Hard" )
    PORT_DIPSETTING( 0x08, "Easy" )
    PORT_DIPSETTING( 0x0c, "Normal" )
INPUT_PORTS_END

INPUT_PORTS_START ( mp_gslam )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x07, 0x04, "Game Time" )
    PORT_DIPSETTING( 0x07, "1:30" )
    PORT_DIPSETTING( 0x06, "2:00" )
    PORT_DIPSETTING( 0x05, "2:30" )
    PORT_DIPSETTING( 0x04, "3:00" )
    PORT_DIPSETTING( 0x03, "3:30" )
    PORT_DIPSETTING( 0x02, "4:00" )
    PORT_DIPSETTING( 0x01, "4:30" )
    PORT_DIPSETTING( 0x00, "5:00" )

	PORT_DIPNAME( 0x08, 0x08, "2P-Play Continue" )
    PORT_DIPSETTING( 0x00, "1 Credit" )
    PORT_DIPSETTING( 0x08, "2 Credits" )
INPUT_PORTS_END

INPUT_PORTS_START ( mp_mazin )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x01, "Initial Player" )
    PORT_DIPSETTING( 0x02, "1" )
    PORT_DIPSETTING( 0x03, "2" )
    PORT_DIPSETTING( 0x01, "3" )
    PORT_DIPSETTING( 0x00, "4" )

	PORT_DIPNAME( 0x04, 0x04, DEF_STR ( Difficulty ) )
    PORT_DIPSETTING( 0x04, "Normal" )
    PORT_DIPSETTING( 0x00, "Hard" )

	PORT_DIPNAME( 0x08, 0x08, "Title" )
    PORT_DIPSETTING( 0x08, "EUROPE" )
    PORT_DIPSETTING( 0x00, "U.S.A" )
INPUT_PORTS_END

INPUT_PORTS_START ( mp_addams )
	GENESIS_PORTS
	MEGAPLAY_TEST
	MEGAPLAY_COIN
	MEGAPLAY_DSWA
	MEGAPLAY_DSWB

	PORT_START
	/* DSW C  (per game settings) */
	PORT_DIPNAME( 0x03, 0x03, "1UP Every" )
	PORT_DIPSETTING( 0x03, "3 Pickups" )
	PORT_DIPSETTING( 0x02, "2 Pickups" )
	PORT_DIPSETTING( 0x01, "1 Pickup" )

	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING( 0x0c, "3" )
	PORT_DIPSETTING( 0x08, "5" )
	PORT_DIPSETTING( 0x04, "7" )
	PORT_DIPSETTING( 0x00, "99" )
INPUT_PORTS_END

/******************************************************************************
	Machine drivers
******************************************************************************/

static MACHINE_DRIVER_START( megaplay )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 53693100 / 7)
	MDRV_CPU_MEMORY(megaplay_genesis_readmem, megaplay_genesis_writemem)
	MDRV_CPU_VBLANK_INT(genesis_vblank_interrupt,1)

	MDRV_CPU_ADD_TAG("sound", Z80, 53693100 / 15)
	MDRV_CPU_MEMORY(megaplay_z80_readmem, megaplay_z80_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold, 1) /* from vdp at scanline 0xe0 */

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION((int)(((262. - 224.) / 262.) * 1000000. / 60.))

	MDRV_INTERLEAVE(100)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS)
	MDRV_SCREEN_SIZE(320,224)
	MDRV_VISIBLE_AREA(0, 319, 0, 223)
	MDRV_PALETTE_LENGTH(2048+32) /* +32 for megaplay bios vdp part */

	/* sound hardware */
	MDRV_SOUND_ADD(YM2612, gen_ym3438_intf )

/*	MDRV_CPU_PROGRAM_MAP(megaplay_genesis_readmem, genesis_writemem) */

	MDRV_VIDEO_START(megaplay)
	MDRV_VIDEO_EOF(segac2)	/* VBLANK tracking: without it there are no mid-screen partial updates */
	MDRV_VIDEO_UPDATE(megaplay)
	MDRV_MACHINE_INIT(megaplay)

	MDRV_CPU_ADD_TAG("megaplay_bios", Z80, 53693100 / 15) /* ?? */
	MDRV_CPU_MEMORY(megaplay_bios_readmem, megaplay_bios_writemem)
	MDRV_CPU_PORTS(megaplay_bios_readport,megaplay_bios_writeport)
	MDRV_SOUND_ADD(SN76496, megatech_sn76489_intf)
	MDRV_CPU_VBLANK_INT(megatech_irq, 262)
MACHINE_DRIVER_END

/* The BIOS keeps its credits, bookkeeping and each slot's 256-byte game
   area (high scores) in battery-backed RAM at Z80 0x6800-0x77ff. */
static NVRAM_HANDLER( megaplay )
{
	if (read_or_write)
		mame_fwrite(file, ic3_ram, 0x1000);
	else if (file)
		mame_fread(file, ic3_ram, 0x1000);
	else
		memset(ic3_ram, 0, 0x1000);
}

static MACHINE_DRIVER_START( megaplay_nvram )
	MDRV_IMPORT_FROM(megaplay)
	MDRV_NVRAM_HANDLER(megaplay)
MACHINE_DRIVER_END

/******************************************************************************
	ROM definitions
******************************************************************************/

/* MegaPlay Games - Modified Genesis games */

SYSTEM_BIOS_START( megaplay )
	SYSTEM_BIOS_ADD( 0, "ver1",       "Megaplay Bios (Ver. 1)" )
	SYSTEM_BIOS_ADD( 1, "ver2",       "Megaplay Bios (Ver. 2)" ) /* this one doesn't boot .. is it ok?*/
SYSTEM_BIOS_END

#define ROM_LOAD_BIOS(bios,name,offset,length,hash) \
		ROMX_LOAD(name, offset, length, hash, ROM_BIOS(bios+1)) /* Note '+1' */

#define MEGAPLAY_BIOS \
	ROM_LOAD_BIOS( 0, "ep15294.ic2", 0x000000, 0x20000, CRC(aa8dc2d8) SHA1(96771ad7b79dc9c83a1594243250d65052d23176) ) \
	ROM_LOAD_BIOS( 1, "epr-a15294.ic2",0x000000, 0x20000, CRC(f97c68aa) SHA1(bcabc879950bca1ced11c550a484e697ec5706b2) ) \

ROM_START( megaplay )
	ROM_REGION( 0x20000, REGION_USER1, 0 )
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_bio ) /* Bio Hazard Battle */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "mpr-15699-f.ic1", 0x000000, 0x100000, CRC(4b193229) SHA1(f8629171ae9b4792f142f6957547d886e5cc6817) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-06.ic2", 0x000000, 0x08000, CRC(1ef64e41) SHA1(13984b714b014ea41963b70de74a5358ed223bc5) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_col3 ) /* Columns 3 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "3.ic2", 0x000000, 0x040000, CRC(a1602235) SHA1(38751b585849c8966acc3f508714937fe29dcf5c) )
	ROM_LOAD16_BYTE( "2.ic1", 0x000001, 0x040000, CRC(999b2fe6) SHA1(ad967a28e4eebd7b01273e4e04c35a0198ef834a) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "1.ic3", 0x000000, 0x08000,  CRC(dac9bf91) SHA1(0117972a7181f8aaf942a259cc8764b821031253) )

	ROM_REGION( 0x28000,  REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END


ROM_START( mp_gaxe2 ) /* Golden Axe 2, revision B */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ep15179b.ic2", 0x000000, 0x040000, CRC(00d97b84) SHA1(914bbf566ddf940aab67b92af237d251650ddadf) )
	ROM_LOAD16_BYTE( "ep15178b.ic1", 0x000001, 0x040000, CRC(2ea576db) SHA1(6d96b948243533de1f488b1f80e0d5431a4f1f53) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "ep15175-02b.ic3", 0x000000, 0x08000, CRC(3039b653) SHA1(b19874c74d0fc0cca1169f62e5e74f0e8ca83679) ) /* 15175-02b.ic3 */

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END


ROM_START( mp_gaxe2a ) /* Golden Axe 2 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15179.ic2", 0x000000, 0x040000, CRC(d35f1a35) SHA1(3105cd3b55f65337863703db04527fe298fc04e0) )
	ROM_LOAD16_BYTE( "epr-15178.ic1", 0x000001, 0x040000, CRC(2c6b6b76) SHA1(25577f49ecad451c217da9cacbd78ffca9dca24e) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-02.ic3", 0x000000, 0x08000, CRC(cfc87f91) SHA1(110609094aa6d848bec613faa0558db7ad272b77) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_shnb3 ) /* Shinobi 3 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "mpr-16197.ic1", 0x000000, 0x100000, CRC(48162361) SHA1(77d544509339b5ddf6d19941377e81d29e9e21dc) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 )/* Game Instructions */
	ROM_LOAD( "epr-15175-09.ic2", 0x000000, 0x08000, CRC(6254e45a) SHA1(8667922a6eade03c964ce224f7fa39ba871c60a4) )

	ROM_REGION( 0x28000,  REGION_CPU3, 0 )/* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_sonic ) /* Sonic */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ep15177.ic2", 0x000000, 0x040000, CRC(a389b03b) SHA1(8e9e1cf3dd65ddf08757f5a1ce472130c902ea2c) )
	ROM_LOAD16_BYTE( "ep15176.ic1", 0x000001, 0x040000, CRC(d180cc21) SHA1(62805cfaaa80c1da6146dd89fc2b49d819fd4f22) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "ep15175-01.ic3", 0x000000, 0x08000, CRC(99246889) SHA1(184aa3b7fdedcf578c5e34edb7ed44f57f832258) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_soni2 ) /* Sonic The Hedgehog 2 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "mpr-16011.ic1", 0x000000, 0x100000, CRC(3d7bf98a) SHA1(dce0e4e8f2573e0ffe851edaa235e4ed9e61ee2d) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-07.ic1", 0x000000, 0x08000, CRC(bb5f67f0) SHA1(33b7a5d14015a5fcf41976a8f648f8f48ce9bb03) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END


ROM_START( mp_sor2 ) /* Streets of Rage 2 */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "mpr-15425.ic1", 0x000000, 0x200000, CRC(cd6376af) SHA1(57ec210975e40505649f152b60ef54f99da31f0e) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-05.ic2", 0x000000, 0x08000, CRC(1df5347c) SHA1(faced2e875e1914392f61577b5256d006eebeef9) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_gunhe ) /* Gunstar Heroes */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "mpr-16390.ic1", 0x000000, 0x100000, CRC(d963a748) SHA1(adf231c5180a9307fd6675fe77fffd4c5bfa3d6a) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-10.ic2", 0x000000, 0x08000, CRC(e4f08233) SHA1(b7e0ad3f6ae1c56df6ec76375842050f08afcbef) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_gslam ) /* Grand Slam */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "epr-15181.ic2", 0x000000, 0x040000, CRC(642437c1) SHA1(cbf88e196c04b6d886bf9642b69bf165045510fe) )
	ROM_LOAD16_BYTE( "epr-15180.ic1", 0x000001, 0x040000, CRC(73bb48f1) SHA1(981b64f834d5618599352f5fad683bf232390ba3) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-03.ic3", 0x000000, 0x08000, CRC(70ea1aec) SHA1(0d9d82a1f8aa51d02707f7b343e7cfb6591efccd) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_mazin ) /* Mazin Wars */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "mpr-16460.ic1", 0x000000, 0x100000, CRC(e9635a83) SHA1(ab3afa11656f0ae3a50c957dce012fb15d3992e0) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-11.ic2", 0x000000, 0x08000, CRC(bb651120) SHA1(81cb736f2732373e260dde162249c1d29a3489c3) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_addams ) /* The Addams Family ARCADE (homebrew conversion of the Mega Drive game) */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_WORD_SWAP( "epr-26013.ic1", 0x000000, 0x200000, CRC(87ecace1) SHA1(d6529b4b692a9515ccf6722f26c011660154ae7a) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "epr-15175-20.ic2", 0x000000, 0x08000, CRC(dcfde4e7) SHA1(3f5a1b4de94fad4ccd404b95a53fdb2e1c4773e6) )

	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
	MEGAPLAY_BIOS
ROM_END

ROM_START( mp_twcup ) /* Tecmo World Cup */
	ROM_REGION( 0x400000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ep15183.ic2", 0x000000, 0x040000, CRC(8b79b861) SHA1(c72af72840513b82f2562409eccdf13b031bf3c0) )
	ROM_LOAD16_BYTE( "ep15182.ic1", 0x000001, 0x040000, CRC(eb8325c3) SHA1(bb21ac926c353e14184dd476222bc6a8714606e5) )
	/* Game Instruction rom copied to 0x300000 - 0x310000 (odd / even bytes equal) */

 	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* z80 */

 	ROM_REGION( 0x8000, REGION_USER1, 0 ) /* Game Instructions */
	ROM_LOAD( "ep15175-04.ic3", 0x000000, 0x08000, CRC(faf7c030) SHA1(16ef405335b4d3ecb0b7d97b088dafc4278d4726) )

 	ROM_REGION( 0x28000, REGION_CPU3, 0 ) /* Bios */
 	MEGAPLAY_BIOS
ROM_END

/******************************************************************************
	Driver init and game drivers
******************************************************************************/

static DRIVER_INIT (megaplay)
{
	data8_t *src = memory_region(REGION_CPU3);
	data8_t *instruction_rom = memory_region(REGION_USER1);
	data8_t *game_rom = memory_region(REGION_CPU1);
	int offs;

	memmove(src+0x10000,src+0x8000,0x18000); /* move bios.. */

	/* copy game instruction rom to main map.. maybe this should just be accessed
      through a handler instead?.. */
	for (offs=0;offs<0x8000;offs++)
	{
		data8_t dat;

		dat=instruction_rom[offs];

		game_rom[0x300000+offs*2] = dat;
		game_rom[0x300001+offs*2] = dat;

	}

	init_segac2();

}

/* -- */ GAMEBX(1993, megaplay, 0,        megaplay, megaplay, megaplay, megaplay, ROT0, "Sega",    "MegaPlay: BIOS", NOT_A_DRIVER, &generic_ctrl, NULL )
/* 01 */ GAMEB( 1993, mp_sonic, megaplay, megaplay, megaplay, mp_sonic, megaplay, ROT0, "Sega",    "Sonic The Hedgehog", &megaplay_ctrl, NULL  )
/* 02 */ GAMEB( 1993, mp_gaxe2, megaplay, megaplay, megaplay, mp_gaxe2, megaplay, ROT0, "Sega",    "Golden Axe II (revision B)", &megaplay_ctrl, NULL  )
/* 02 */ GAMEB( 1993, mp_gaxe2a, mp_gaxe2, megaplay, megaplay, mp_gaxe2, megaplay, ROT0, "Sega",    "Golden Axe II", &megaplay_ctrl, NULL  )
/* 03 */ GAMEB( 1993, mp_gslam, megaplay, megaplay, megaplay, mp_gslam, megaplay, ROT0, "Sega",    "Grand Slam", &megaplay_ctrl, NULL )
/* 03 */ GAMEB( 1993, mp_sor2,  megaplay, megaplay, megaplay, mp_sor2,	megaplay, ROT0, "Sega",    "Streets Of Rage II", &megaplay_ctrl, NULL  )
/* 04 */ GAMEB( 1993, mp_twcup, megaplay, megaplay, megaplay, mp_twcup, megaplay, ROT0, "Sega",    "Tecmo World Cup", &megaplay_ctrl, NULL  )
/* 05 */ GAMEB( 1993, mp_soni2, megaplay, megaplay, megaplay, mp_soni2, megaplay, ROT0, "Sega",    "Sonic The Hedgehog 2", &megaplay_ctrl, NULL  )
/* 06 */ GAMEB( 1993, mp_bio,   megaplay, megaplay, megaplay, mp_bio,	  megaplay, ROT0, "Sega",    "Bio-Hazard Battle", &megaplay_ctrl, NULL  )
/* 07 */ GAMEB( 1993, mp_shnb3, megaplay, megaplay, megaplay, mp_shnb3, megaplay, ROT0, "Sega",    "Shinobi III", &megaplay_ctrl, NULL )
/* 10 */ GAMEB( 1993, mp_gunhe, megaplay, megaplay, megaplay, mp_gunhe, megaplay, ROT0, "Sega",    "Gunstar Heroes", &megaplay_ctrl, NULL )
/* 11 */ GAMEB( 1993, mp_mazin, megaplay, megaplay, megaplay, mp_mazin, megaplay, ROT0, "Sega",    "Mazin Wars / Mazin Saga", &megaplay_ctrl, NULL )
/* ?? */ GAMEB( 1993, mp_col3,  megaplay, megaplay, megaplay, megaplay, megaplay, ROT0, "Sega",    "Columns III", &megaplay_ctrl, NULL )
/* -- */ GAMEB( 2026, mp_addams, megaplay, megaplay, megaplay_nvram, mp_addams, megaplay, ROT0, "hack", "The Addams Family ARCADE (Mega Play)", &megaplay_ctrl, NULL )
