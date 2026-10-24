/***************************************************************************

  Seibu SPI hardware
  Seibu SYS386I

  Backported from current MAME src/mame/seibu/seibuspi.cpp (Ville Linde,
  hap, Nicola Salmoria and others); the video is in vidhrdw/seibuspi.c and
  the graphics decryption in machine/seibuspi.c.

  SPI: a main board (Intel 386DX 25MHz, Z80, YMF271, two flash ROMs for the
  sound samples, a DS2404 RTC) and a cartridge with the game. The main CPU
  uploads the Z80 program, then talks to it through two FIFOs; the Z80 drives
  the YMF271, whose external memory is the flash pair (programmed by the
  game when the cartridge is changed) and the sample ROMs of the cartridge,
  read by the main CPU at 0xa00000.

  Single boards (SXX2E, SXX2F, SXX2G): the Z80 program in a ROM, one FIFO,
  the samples in ROM; SXX2F and SXX2G have a 93C46 EEPROM instead of the
  DS2404. SYS386I (40MHz 386): two MSM6295 instead of the Z80 sound system.

  Games:
    Senkyu / Battle Balls
    Viper Phase 1
    Raiden Fighters
    Raiden Fighters 2 - Operation Hell Dive
    Raiden Fighters Jet

  As in MAME: the alpha blending is preliminary (a table of pens), and
  the JP1 jumper of the SXX2C main board is not hooked up.

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "cpu/i386/i386.h"
#include "machine/eeprom.h"
#include "machine/intelfsh.h"
#include "machine/ds2404.h"
#include "includes/seibuspi.h"

/* the boards */
enum
{
	SPI_SXX2C = 0,		/* SPI main board and cartridge, SEI252 sprites (0x50e) */
	SPI_SXX2C_RISE,		/* the same, RISE10 / RISE11 sprites (0x562) */
	SPI_SXX2E,			/* single board, SEI252, DS2404 */
	SPI_SXX2F,			/* single board, RISE, EEPROM (SXX2G too) */
	SPI_SYS386I			/* RISE, EEPROM, two MSM6295 */
};

static int spi_board;

static UINT8 z80_ram[0x2000];
static UINT32 z80_prg_transfer_pos;
static UINT8 z80_lastbank;
static UINT8 sb_coin_latch;
static UINT8 oki_bank;


/*****************************************************************************

  The sound FIFOs (IDT7201, 512 x 9 bits): 0 from the main CPU to the Z80,
  1 back (SPI main board only)

*****************************************************************************/

#define FIFO_SIZE	512

struct spi_fifo
{
	UINT16 data[FIFO_SIZE];
	UINT32 rpos, wpos, count;
};

static struct spi_fifo spi_fifo[2];

static void fifo_reset(struct spi_fifo *f)
{
	memset(f, 0, sizeof(*f));
}

static void fifo_write(struct spi_fifo *f, UINT8 data)
{
	if (f->count == FIFO_SIZE)
	{
		logerror("SPI: fifo overflow\n");
		return;
	}
	f->data[f->wpos] = data;
	f->wpos = (f->wpos + 1) % FIFO_SIZE;
	f->count++;
}

static UINT8 fifo_read(struct spi_fifo *f)
{
	UINT8 ret;

	if (f->count == 0)
	{
		logerror("SPI: fifo underflow\n");
		return 0xff;
	}
	ret = f->data[f->rpos];
	f->rpos = (f->rpos + 1) % FIFO_SIZE;
	f->count--;
	return ret;
}

/* the flags as the pins read: 1 while not empty / not full */
#define FIFO_EF(f)	((f)->count != 0)
#define FIFO_FF(f)	((f)->count != FIFO_SIZE)

static UINT8 sound_fifo_status(void)
{
	/* d0: fifo full flag (z80) */
	/* d1: fifo empty flag (main) */
	int d1 = (spi_board <= SPI_SXX2C_RISE) ? FIFO_EF(&spi_fifo[1]) << 1 : 0;
	return d1 | FIFO_FF(&spi_fifo[0]);
}


/*****************************************************************************

  The main CPU

*****************************************************************************/

static void z80_prg_transfer_w(UINT8 data)
{
	if (z80_prg_transfer_pos < memory_region_length(REGION_CPU2))
	{
		memory_region(REGION_CPU2)[z80_prg_transfer_pos] = data;
		z80_prg_transfer_pos++;
	}
}

static void z80_enable_w(UINT8 data)
{
	/* d0: reset z80 */
	z80_prg_transfer_pos = 0;
	cpu_set_reset_line(1, (data & 1) ? CLEAR_LINE : ASSERT_LINE);
}

static void eeprom_w(UINT8 data)
{
	EEPROM_write_bit((data >> 7) & 1);
	EEPROM_set_clock_line((data & 0x40) ? ASSERT_LINE : CLEAR_LINE);
	EEPROM_set_cs_line((data & 0x20) ? CLEAR_LINE : ASSERT_LINE);
}

static UINT32 system_r(void)
{
	UINT32 ret = 0xffffff00 | readinputport(1);

	/* the EEPROM data out on the single boards with one */
	if (spi_board >= SPI_SXX2F)
		ret = (ret & ~0x40) | (EEPROM_read_bit() ? 0x40 : 0);
	return ret;
}

/* the I/O at 0x400-0x7ff, over the RAM: the addresses with nothing read */
/* and write the RAM, as the RAM of MAME is under them */
static data32_t spi_io_r(offs_t offset, data32_t mem_mask)
{
	offs_t address = 0x400 + offset * 4;
	data32_t ret = spi_mainram[address / 4];

	if (address < 0x440)
		return spi_crtc_r(offset, mem_mask);

	switch (address)
	{
	case 0x600:
		/* d0: unknown status, waits for it to be set, video/dma related? */
		ret = (ret & ~0xff) | 0x01;
		break;

	case 0x604:
		ret = 0xffff0000 | readinputport(0);
		break;

	case 0x608:
		ret = 0xffffffff;
		break;

	case 0x60c:
		ret = system_r();
		break;

	case 0x680:
		if (spi_board == SPI_SYS386I)
			break;
		if (!(mem_mask & 0xff))
		{
			UINT8 d;
			if (spi_board <= SPI_SXX2C_RISE)
				d = fifo_read(&spi_fifo[1]);
			else
			{
				/* the coin latch of the single boards */
				d = sb_coin_latch;
				sb_coin_latch = 0;
			}
			ret = (ret & ~0xff) | d;
		}
		break;

	case 0x684:
		if (spi_board != SPI_SYS386I)
			ret = (ret & ~0xff) | sound_fifo_status();
		break;

	case 0x688:
		if (spi_board == SPI_SXX2E || spi_board == SPI_SXX2F)
			ret = 0;
		break;

	case 0x6dc:
		if (spi_board <= SPI_SXX2E)
		{
			/* d0, d1, d2 of 0x6dd: unknown, waits for them to be cleared */
			ret &= 0xffff0000;
			if (!(mem_mask & 0xff))
				ret |= ds2404_data_r(0);
		}
		break;
	}
	return ret;
}

static void spi_io_w(offs_t offset, data32_t data, data32_t mem_mask)
{
	offs_t address = 0x400 + offset * 4;
	int sei252 = (spi_board == SPI_SXX2C || spi_board == SPI_SXX2E);

	if (address < 0x440)
	{
		spi_crtc_w(offset, data, mem_mask);
		return;
	}

	switch (address)
	{
	case 0x480:
		spi_tilemap_dma_start_w(0, data, mem_mask);
		return;

	case 0x484:
		spi_palette_dma_start_w(0, data, mem_mask);
		return;

	case 0x490:
		spi_video_dma_length_w(0, data, mem_mask);
		return;

	case 0x494:
		spi_video_dma_address_w(0, data, mem_mask);
		return;

	case 0x498:		/* ? dma address high bits? (always writes 0) */
		return;

	case 0x50c:		/* SEI252: sprite DMA start at 0x50e */
		if (sei252)
		{
			if (!(mem_mask & 0xffff0000))
				spi_sprite_dma_start();
			return;
		}
		break;

	case 0x524: case 0x528: case 0x530: case 0x534: case 0x53c:
		if (sei252)
			return;	/* SEI252 sprite decryption keys */
		break;

	case 0x54c:
		if (!sei252)
			return;	/* RISE10/11 sprite decryption key */
		break;

	case 0x560:		/* RISE: sprite DMA start at 0x562 */
		if (!sei252)
		{
			if (!(mem_mask & 0xffff0000))
				spi_sprite_dma_start();
			return;
		}
		break;

	case 0x600:
		if (spi_board <= SPI_SXX2C_RISE)
			return;
		break;

	case 0x680:
		if (spi_board == SPI_SYS386I)
			break;
		if (!(mem_mask & 0xff))
			fifo_write(&spi_fifo[0], data & 0xff);
		return;

	case 0x688:
		if (spi_board <= SPI_SXX2C_RISE)
		{
			if (!(mem_mask & 0xff))
				z80_prg_transfer_w(data & 0xff);
			return;
		}
		if (spi_board != SPI_SYS386I)
			return;
		break;

	case 0x68c:
		if (spi_board <= SPI_SXX2C_RISE)
		{
			if (!(mem_mask & 0xff))
				z80_enable_w(data & 0xff);
			if (!(mem_mask & 0xff0000))
				spi_rf2_layer_bank_w((data >> 16) & 0xff);
			return;
		}
		if (spi_board == SPI_SXX2E)
			return;
		/* SXX2F, SYS386I: low bits the tile banks, high bits the EEPROM */
		if (!(mem_mask & 0xff0000))
		{
			spi_rf2_layer_bank_w((data >> 16) & 0xff);
			eeprom_w((data >> 16) & 0xff);
		}
		if (spi_board == SPI_SYS386I && !(mem_mask & 0xff000000))
		{
			oki_bank = (data >> 26) & 1;
			OKIM6295_set_bank_base(1, oki_bank * 0x40000);
		}
		return;

	case 0x690:
		if (spi_board == SPI_SXX2F)
			return;
		break;

	case 0x6d0:
	case 0x6d4:
	case 0x6d8:
		if (spi_board <= SPI_SXX2E)
		{
			if (!(mem_mask & 0xff))
			{
				if (address == 0x6d0)
					ds2404_1w_reset_w(0, data & 0xff);
				else if (address == 0x6d4)
					ds2404_data_w(0, data & 0xff);
				else
					ds2404_clk_w(0, data & 0xff);
			}
			return;
		}
		break;
	}

	COMBINE_DATA(&spi_mainram[address / 4]);
}

static READ32_HANDLER( spi_io32_r ) { return spi_io_r(offset, mem_mask); }
static WRITE32_HANDLER( spi_io32_w ) { spi_io_w(offset, data, mem_mask); }

static READ32_HANDLER( oki_0_r ) { return OKIM6295_status_0_r(0); }
static READ32_HANDLER( oki_1_r ) { return OKIM6295_status_1_r(0); }
static WRITE32_HANDLER( oki_0_w ) { if (!(mem_mask & 0xff)) OKIM6295_data_0_w(0, data & 0xff); }
static WRITE32_HANDLER( oki_1_w ) { if (!(mem_mask & 0xff)) OKIM6295_data_1_w(0, data & 0xff); }

static MEMORY_READ32_START( spi_readmem )
	{ 0x00000400, 0x000007ff, spi_io32_r },
	{ 0x00000000, 0x0003ffff, MRA32_BANK1 },
	{ 0x00200000, 0x003fffff, MRA32_BANK2 },
	{ 0x00a00000, 0x013fffff, MRA32_BANK4 },
	{ 0xffe00000, 0xffffffff, MRA32_BANK3 },	/* ROM location in real-mode */
MEMORY_END

static MEMORY_WRITE32_START( spi_writemem )
	{ 0x00000400, 0x000007ff, spi_io32_w },
	{ 0x00000000, 0x0003ffff, MWA32_BANK1 },
	{ 0x00200000, 0x003fffff, MWA32_NOP },
	{ 0x00a00000, 0x013fffff, MWA32_NOP },
	{ 0xffe00000, 0xffffffff, MWA32_NOP },
MEMORY_END

static MEMORY_READ32_START( sxx2e_readmem )
	{ 0x00000400, 0x000007ff, spi_io32_r },
	{ 0x00000000, 0x0003ffff, MRA32_BANK1 },
	{ 0x00200000, 0x003fffff, MRA32_BANK2 },
	{ 0xffe00000, 0xffffffff, MRA32_BANK3 },
MEMORY_END

static MEMORY_WRITE32_START( sxx2e_writemem )
	{ 0x00000400, 0x000007ff, spi_io32_w },
	{ 0x00000000, 0x0003ffff, MWA32_BANK1 },
	{ 0x00200000, 0x003fffff, MWA32_NOP },
	{ 0xffe00000, 0xffffffff, MWA32_NOP },
MEMORY_END

static MEMORY_READ32_START( sys386i_readmem )
	{ 0x00000400, 0x000007ff, spi_io32_r },
	{ 0x00000000, 0x0003ffff, MRA32_BANK1 },
	{ 0x00200000, 0x003fffff, MRA32_BANK2 },
	{ 0x01200000, 0x01200003, oki_0_r },
	{ 0x01200004, 0x01200007, oki_1_r },
	{ 0xffe00000, 0xffffffff, MRA32_BANK3 },
MEMORY_END

static MEMORY_WRITE32_START( sys386i_writemem )
	{ 0x00000400, 0x000007ff, spi_io32_w },
	{ 0x00000000, 0x0003ffff, MWA32_BANK1 },
	{ 0x00200000, 0x003fffff, MWA32_NOP },
	{ 0x01200000, 0x01200003, oki_0_w },
	{ 0x01200004, 0x01200007, oki_1_w },
	{ 0xffe00000, 0xffffffff, MWA32_NOP },
MEMORY_END


/*****************************************************************************

  The Z80

*****************************************************************************/

static READ_HANDLER( z80_soundfifo_r )
{
	return fifo_read(&spi_fifo[0]);
}

static WRITE_HANDLER( z80_soundfifo_w )
{
	if (spi_board <= SPI_SXX2C_RISE)
		fifo_write(&spi_fifo[1], data);
}

static READ_HANDLER( z80_soundfifo_status_r )
{
	/* d0: fifo full flag (main) */
	/* d1: fifo empty flag (z80) */
	int d0 = (spi_board <= SPI_SXX2C_RISE) ? FIFO_FF(&spi_fifo[1]) : 0;
	return d0 | FIFO_EF(&spi_fifo[0]) << 1;
}

static WRITE_HANDLER( z80_bank_w )
{
	/* d0-d2: bank @ 8000 */
	/* d3: watchdog? */
	UINT8 bank = data & 7;

	if (bank != z80_lastbank)
	{
		z80_lastbank = bank;
		cpu_setbank(5, memory_region(REGION_CPU2) + bank * 0x8000);
	}
}

static WRITE_HANDLER( spi_coin_w )
{
	coin_counter_w(0, data & 1);
	coin_counter_w(1, (data >> 1) & 1);

	/* coin latch used by single boards */
	if (data)
		sb_coin_latch = 0xa0 | data;
	else
		sb_coin_latch = 0;
}

static READ_HANDLER( z80_jumpers_r )
{
	/* JP1 (SXX2C only) */
	return (spi_board <= SPI_SXX2C_RISE) ? readinputport(3) : 0;
}

static MEMORY_READ_START( spi_sound_readmem )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x3fff, MRA_BANK6 },
	{ 0x4008, 0x4008, z80_soundfifo_r },
	{ 0x4009, 0x4009, z80_soundfifo_status_r },
	{ 0x400a, 0x400a, z80_jumpers_r },
	{ 0x4013, 0x4013, input_port_2_r },
	{ 0x6000, 0x600f, YMF271_0_r },
	{ 0x8000, 0xffff, MRA_BANK5 },
MEMORY_END

static MEMORY_WRITE_START( spi_sound_writemem )
	{ 0x0000, 0x1fff, MWA_NOP },
	{ 0x2000, 0x3fff, MWA_BANK6 },
	{ 0x4002, 0x4003, MWA_NOP },
	{ 0x4004, 0x4004, spi_coin_w },
	{ 0x4008, 0x4008, z80_soundfifo_w },
	{ 0x400b, 0x400b, MWA_NOP },
	{ 0x401b, 0x401b, z80_bank_w },
	{ 0x6000, 0x600f, YMF271_0_w },
MEMORY_END


/*****************************************************************************

  The sound: the YMF271 reads the two flash ROMs on the SPI main board

*****************************************************************************/

static READ_HANDLER( spi_flash_r )
{
	offset &= 0x1fffff;
	return intelfsh_read(offset >> 20, offset & 0xfffff);
}

static WRITE_HANDLER( spi_flash_w )
{
	offset &= 0x1fffff;
	intelfsh_write(offset >> 20, offset & 0xfffff, data);
}

static void spi_ymf271_irq(int state)
{
	/* Z80 IM0, RST10 */
	cpu_set_irq_line_and_vector(1, 0, state ? ASSERT_LINE : CLEAR_LINE, 0xd7);
}

static struct YMF271interface spi_ymf271_interface =
{
	1,
	{ REGION_SOUND1 },
	{ YM3012_VOL(100, MIXER_PAN_LEFT, 100, MIXER_PAN_RIGHT) },
	{ spi_flash_r },
	{ spi_flash_w },
	{ spi_ymf271_irq },
	{ 16934400 }
};

/* single boards: mono, the samples in ROM */
static struct YMF271interface sxx2e_ymf271_interface =
{
	1,
	{ REGION_SOUND1 },
	{ YM3012_VOL(100, MIXER_PAN_CENTER, 100, MIXER_PAN_CENTER) },
	{ 0 },
	{ 0 },
	{ spi_ymf271_irq },
	{ 16934400 }
};

static struct YMF271interface sxx2g_ymf271_interface =
{
	1,
	{ REGION_SOUND1 },
	{ YM3012_VOL(100, MIXER_PAN_CENTER, 100, MIXER_PAN_CENTER) },
	{ 0 },
	{ 0 },
	{ spi_ymf271_irq },
	{ 16384000 }
};

/* SYS386I: 28.636363MHz / 20, pin 7 high */
static struct OKIM6295interface sys386i_okim6295_interface =
{
	2,
	{ 28636363 / 20 / 132, 28636363 / 20 / 132 },
	{ REGION_SOUND1, REGION_SOUND2 },
	{ 50, 50 }
};


/*****************************************************************************

  Input ports

*****************************************************************************/

#define SPI_PLAYER( player, button3 ) \
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER##player ) \
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER##player ) \
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER##player ) \
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER##player ) \
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER##player ) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER##player ) \
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, button3 | IPF_PLAYER##player )

#define SPI_INPUTS( button3, flip ) \
	PORT_START	/* INPUTS */ \
	SPI_PLAYER( 1, button3 ) \
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) \
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) \
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, button3 | IPF_PLAYER2 ) \
	flip

#define SPI_FLIP_DIP \
	PORT_DIPNAME( 0x8000, 0x0000, DEF_STR( Flip_Screen ) ) \
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) ) \
	PORT_DIPSETTING(      0x8000, DEF_STR( On ) )

#define SPI_SYSTEM( coins ) \
	PORT_START	/* SYSTEM */ \
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BITX( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	coins \
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

#define SPI_SYSTEM_UNKNOWN \
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define SPI_SYSTEM_COINS \
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_COIN2 )

#define SPI_COIN \
	PORT_START	/* COIN (Z80) */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )

/* JP1 is for SXX2C only */
#define SPI_JUMPERS \
	PORT_START	/* JUMPERS (Z80) */ \
	PORT_DIPNAME( 0x03, 0x03, "JP1" ) /* "Only used when game-board is changed with a new game" in manual */ \
	PORT_DIPSETTING(    0x03, "Update" ) /* "Changing game" in manual */ \
	PORT_DIPSETTING(    0x00, "Normal" ) \
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )

INPUT_PORTS_START( spi_3button )
	SPI_INPUTS( IPT_BUTTON3, SPI_FLIP_DIP )
	SPI_SYSTEM( SPI_SYSTEM_UNKNOWN )
	SPI_COIN
	SPI_JUMPERS
INPUT_PORTS_END

INPUT_PORTS_START( spi_2button )
	SPI_INPUTS( IPT_UNUSED, SPI_FLIP_DIP )
	SPI_SYSTEM( SPI_SYSTEM_UNKNOWN )
	SPI_COIN
	SPI_JUMPERS
INPUT_PORTS_END

INPUT_PORTS_START( sxx2e )
	SPI_INPUTS( IPT_BUTTON3, SPI_FLIP_DIP )
	SPI_SYSTEM( SPI_SYSTEM_UNKNOWN )
	SPI_COIN
INPUT_PORTS_END

/* bit 0x40 of SYSTEM: the EEPROM */
INPUT_PORTS_START( sxx2f )
	SPI_INPUTS( IPT_UNUSED, SPI_FLIP_DIP )
	SPI_SYSTEM( SPI_SYSTEM_UNKNOWN )
	SPI_COIN
INPUT_PORTS_END

INPUT_PORTS_START( sys386i )
	SPI_INPUTS( IPT_UNUSED, PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_UNUSED ) )
	SPI_SYSTEM( SPI_SYSTEM_COINS )
	PORT_START	/* COIN: unused */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


/*****************************************************************************

  Machine

*****************************************************************************/

/* this is a 93C46 but with reset delay */
static struct EEPROM_interface spi_eeprom_interface =
{
	6,				/* address bits */
	16,				/* data bits */
	"*110",			/*  read command */
	"*101",			/* write command */
	"*111",			/* erase command */
	"*10000xxxx",	/* lock command */
	"*10011xxxx",	/* unlock command */
	1,				/* enable_multi_read */
	1				/* reset_delay */
};

static INTERRUPT_GEN( spi_interrupt )
{
	/* where is ack? */
	cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, 0x20);
}

static void spi_postload(void)
{
	cpu_setbank(5, memory_region(REGION_CPU2) + z80_lastbank * 0x8000);
	if (spi_board == SPI_SYS386I)
		OKIM6295_set_bank_base(1, oki_bank * 0x40000);
}

/* the board, the memory banks, the chips and the save states; from every */
/* DRIVER_INIT, rise: the RISE10 / RISE11 sprite chip instead of the SEI252; */
/* speedup: the address of the idle loop counter, read by its handler */
static void spi_common_init(int rise, offs_t speedup)
{
	const struct Memory_ReadAddress32 *map = (const struct Memory_ReadAddress32 *)Machine->drv->cpu[0].memory_read;

	if (map == sys386i_readmem)
		spi_board = SPI_SYS386I;
	else if (map == sxx2e_readmem)
		spi_board = rise ? SPI_SXX2F : SPI_SXX2E;
	else
		spi_board = rise ? SPI_SXX2C_RISE : SPI_SXX2C;

	spi_mainram = auto_malloc(0x40000);
	memset(spi_mainram, 0, 0x40000);
	cpu_setbank(1, spi_mainram);
	cpu_setbank(2, memory_region(REGION_CPU1));
	cpu_setbank(3, memory_region(REGION_CPU1));
	if (spi_board <= SPI_SXX2C_RISE)
		cpu_setbank(4, memory_region(REGION_USER1));
	if (spi_board != SPI_SYS386I)
	{
		cpu_setbank(5, memory_region(REGION_CPU2));
		cpu_setbank(6, z80_ram);
	}

	/* the 386 fetches and reads the ROM and the RAM directly; not the I/O */
	/* at 0x400-0x7ff, and the idle loop counter is read by its handler */
	i386_set_fastmem(0x00200000, 0x003fffff, memory_region(REGION_CPU1), I386_FASTMEM_FETCH | I386_FASTMEM_READ);
	i386_set_fastmem(0x00000800, speedup - 1, (UINT8 *)spi_mainram + 0x800, I386_FASTMEM_FETCH | I386_FASTMEM_READ | I386_FASTMEM_WRITE);
	i386_set_fastmem(speedup + 4, 0x0003ffff, (UINT8 *)spi_mainram + speedup + 4, I386_FASTMEM_FETCH | I386_FASTMEM_READ | I386_FASTMEM_WRITE);
	i386_set_fastmem(speedup, speedup + 3, (UINT8 *)spi_mainram + speedup, I386_FASTMEM_FETCH | I386_FASTMEM_WRITE);
	i386_set_fastmem(0x00000000, 0x000003ff, (UINT8 *)spi_mainram, I386_FASTMEM_FETCH | I386_FASTMEM_READ | I386_FASTMEM_WRITE);
	i386_set_fastmem(0xffe00000, 0xffffffff, memory_region(REGION_CPU1), I386_FASTMEM_FETCH | I386_FASTMEM_READ);
	if (spi_board <= SPI_SXX2C_RISE)
		i386_set_fastmem(0x00a00000, 0x013fffff, memory_region(REGION_USER1), I386_FASTMEM_READ);

	if (spi_board <= SPI_SXX2C_RISE)
	{
		intelfsh_init(0, INTELFSH_INTEL_E28F008SA, NULL);	/* Sharp LH28F008 on newer mainboard revision */
		intelfsh_init(1, INTELFSH_INTEL_E28F008SA, NULL);
		memcpy(intelfsh_data(0), memory_region(REGION_USER2), 0x100000);
	}
	if (spi_board <= SPI_SXX2E)
		ds2404_init(1995, 1, 1);
	if (spi_board >= SPI_SXX2F)
		EEPROM_init(&spi_eeprom_interface);

	memset(z80_ram, 0, sizeof(z80_ram));
	memset(spi_fifo, 0, sizeof(spi_fifo));
	z80_prg_transfer_pos = 0;
	z80_lastbank = 0;
	sb_coin_latch = 0;
	oki_bank = 0;

	state_save_register_UINT32("spi", 0, "mainram", spi_mainram, 0x40000 / 4);
	state_save_register_UINT8("spi", 0, "z80_ram", z80_ram, sizeof(z80_ram));
	if (spi_board <= SPI_SXX2C_RISE)
		state_save_register_UINT8("spi", 0, "z80_rom", memory_region(REGION_CPU2), memory_region_length(REGION_CPU2));
	state_save_register_UINT32("spi", 0, "z80_prg_transfer_pos", &z80_prg_transfer_pos, 1);
	state_save_register_UINT8("spi", 0, "z80_lastbank", &z80_lastbank, 1);
	state_save_register_UINT8("spi", 0, "sb_coin_latch", &sb_coin_latch, 1);
	state_save_register_UINT8("spi", 0, "oki_bank", &oki_bank, 1);
	state_save_register_UINT16("spi", 0, "fifo0_data", spi_fifo[0].data, FIFO_SIZE);
	state_save_register_UINT32("spi", 0, "fifo0_rpos", &spi_fifo[0].rpos, 1);
	state_save_register_UINT32("spi", 0, "fifo0_wpos", &spi_fifo[0].wpos, 1);
	state_save_register_UINT32("spi", 0, "fifo0_count", &spi_fifo[0].count, 1);
	state_save_register_UINT16("spi", 0, "fifo1_data", spi_fifo[1].data, FIFO_SIZE);
	state_save_register_UINT32("spi", 0, "fifo1_rpos", &spi_fifo[1].rpos, 1);
	state_save_register_UINT32("spi", 0, "fifo1_wpos", &spi_fifo[1].wpos, 1);
	state_save_register_UINT32("spi", 0, "fifo1_count", &spi_fifo[1].count, 1);
	state_save_register_func_postload(spi_postload);
}

static MACHINE_INIT( spi )
{
	fifo_reset(&spi_fifo[0]);
	fifo_reset(&spi_fifo[1]);
	z80_lastbank = 0;
	sb_coin_latch = 0;
	if (spi_board != SPI_SYS386I)
		cpu_setbank(5, memory_region(REGION_CPU2));

	if (spi_board <= SPI_SXX2C_RISE)
	{
		/* the Z80 waits for its program */
		cpu_set_reset_line(1, ASSERT_LINE);
		z80_prg_transfer_pos = 0;

		/* Hack: fix the magic ID byte so users can't "brick" the machine */
		((UINT8 *)intelfsh_data(0))[0] = memory_region(REGION_USER2)[0];
	}
	else if (spi_board == SPI_SYS386I)
	{
		oki_bank = 0;
		OKIM6295_set_bank_base(1, 0);
	}
}

/* the two flash ROMs of the sound samples and the DS2404 */
static NVRAM_HANDLER( spi )
{
	if (read_or_write)
	{
		intelfsh_nvram_save(0, file);
		intelfsh_nvram_save(1, file);
		ds2404_nvram_save(file);
	}
	else if (file)
	{
		intelfsh_nvram_load(0, file);
		intelfsh_nvram_load(1, file);
		ds2404_nvram_load(file);
	}
}

static NVRAM_HANDLER( sxx2e )
{
	if (read_or_write)
		ds2404_nvram_save(file);
	else if (file)
		ds2404_nvram_load(file);
}

/* the EEPROM; no file: from its ROM if there is one, else blank. The */
/* words of the ROM are little endian as MAME reads them (the checksum of */
/* rfjets says so), the EEPROM here wants the high byte first */
static NVRAM_HANDLER( sxx2f )
{
	if (read_or_write)
		EEPROM_save(file);
	else if (file)
		EEPROM_load(file);
	else if (memory_region(REGION_USER3))
	{
		UINT8 data[0x80];
		const UINT8 *rom = memory_region(REGION_USER3);
		int i;

		for (i = 0; i < 0x80; i += 2)
		{
			data[i] = rom[i + 1];
			data[i + 1] = rom[i];
		}
		EEPROM_set_data(data, 0x80);
	}
}

static MACHINE_DRIVER_START( spi )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", I386, 50000000/2)	/* AMD or Intel 386DX, 25MHz */
	MDRV_CPU_MEMORY(spi_readmem, spi_writemem)
	MDRV_CPU_VBLANK_INT(spi_interrupt, 1)

	MDRV_CPU_ADD_TAG("sound", Z80, 28636363/4)	/* Z84C0008PEC, 7.159MHz */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(spi_sound_readmem, spi_sound_writemem)

	/* 28.636363MHz / 4, 448 x 296 pixels */
	MDRV_FRAMES_PER_SECOND(28636363.0 / 4 / 448 / 296)
	MDRV_VBLANK_DURATION(3504)	/* 56 lines */
	MDRV_INTERLEAVE(222)	/* the quantum of MAME, 12000Hz */

	MDRV_MACHINE_INIT(spi)
	MDRV_NVRAM_HANDLER(spi)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT | VIDEO_NEEDS_6BITS_PER_GUN)
	MDRV_SCREEN_SIZE(320, 240)
	MDRV_VISIBLE_AREA(0, 319, 0, 239)
	MDRV_PALETTE_LENGTH(6144)

	MDRV_VIDEO_START(spi)
	MDRV_VIDEO_UPDATE(spi)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD_TAG("ymf", YMF271, spi_ymf271_interface)
MACHINE_DRIVER_END

/* the RISE sprites: the same, the board is told by the DRIVER_INIT */
static MACHINE_DRIVER_START( rdft2 )
	MDRV_IMPORT_FROM(spi)
MACHINE_DRIVER_END

/* single boards */

static MACHINE_DRIVER_START( sxx2e )
	MDRV_IMPORT_FROM(spi)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(sxx2e_readmem, sxx2e_writemem)
	MDRV_NVRAM_HANDLER(sxx2e)

	/* Single PCBs only output mono sound, SXX2E : unverified */
	MDRV_SOUND_ATTRIBUTES(0)
	MDRV_SOUND_REPLACE("ymf", YMF271, sxx2e_ymf271_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( sxx2f )
	MDRV_IMPORT_FROM(sxx2e)
	MDRV_NVRAM_HANDLER(sxx2f)	/* no DS2404, an EEPROM */
	/* Z80 is Z84C0006PCS instead of Z84C0008PEC */
	/* clock is unknown, possibly slower than 7.159MHz */
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( sxx2g )	/* clocks differ, but otherwise same hw as sxx2f */
	MDRV_IMPORT_FROM(sxx2f)
	MDRV_CPU_REPLACE("main", I386, 28636363)	/* AMD AM386DX/DX-40, 28.63636MHz */
	MDRV_CPU_REPLACE("sound", Z80, 4951200)		/* Z84C0004PCS, 4.9152MHz (4.9512MHz in MAME) */
	MDRV_SOUND_REPLACE("ymf", YMF271, sxx2g_ymf271_interface)	/* 16.384MHz(!) */
MACHINE_DRIVER_END

/* SYS386I */

static MACHINE_DRIVER_START( sys386i )
	MDRV_IMPORT_FROM(spi)
	MDRV_CPU_REPLACE("main", I386, 40000000)	/* AMD 386DX, 40MHz */
	MDRV_CPU_MEMORY(sys386i_readmem, sys386i_writemem)
	MDRV_CPU_REMOVE("sound")
	MDRV_NVRAM_HANDLER(sxx2f)

	MDRV_SOUND_ATTRIBUTES(0)
	MDRV_SOUND_REPLACE("ymf", OKIM6295, sys386i_okim6295_interface)
MACHINE_DRIVER_END


/*****************************************************************************

  Driver inits: the graphics decryption and the speedups

*****************************************************************************/

#define SPEEDUP(name, address) \
static READ32_HANDLER( name ) \
{ \
	offs_t pc = activecpu_get_pc(); \
	if (name##_pc(pc)) cpu_spinuntil_int(); \
	return spi_mainram[address / 4]; \
}

static int senkyu_speedup_pc(offs_t pc) { return pc == 0x00305bb2; }
static int senkyua_speedup_pc(offs_t pc) { return pc == 0x0030582e; }
/* batlbalu, batlball */
static int batlball_speedup_pc(offs_t pc) { return pc == 0x00305996 || pc == 0x003058aa; }
/* viprp1, viprp1s, viprp1ot */
static int viprp1_speedup_pc(offs_t pc) { return pc == 0x0202769 || pc == 0x02027e9 || pc == 0x02026bd; }
/* viperp1o */
static int viprp1o_speedup_pc(offs_t pc) { return pc == 0x0201f99; }
/* rdft, rdftj?, rdftau, rdftja?, rdfta / rdftadi / rdftam / rdftit, rdftu, rdftauge */
static int rdft_speedup_pc(offs_t pc)
{
	return pc == 0x0203f06 || pc == 0x0203f0a || pc == 0x0203f16 || pc == 0x0203f22 ||
			pc == 0x0203f46 || pc == 0x0203f3a || pc == 0x0203f6e;
}
/* rdft22kc, rdft2 / rdft2j, rdft2us, rdft2a */
static int rf2_speedup_pc(offs_t pc) { return pc == 0x0203926 || pc == 0x0204372 || pc == 0x020420e || pc == 0x0204366; }
/* rfjet / rfjetu / rfjeta, rfjetj, rfjet2kc, rfjets, rfjetsa */
static int rfjet_speedup_pc(offs_t pc)
{
	return pc == 0x0206082 || pc == 0x0205f2e || pc == 0x0205c9e || pc == 0x0205b39 || pc == 0x0205bd9;
}

SPEEDUP( senkyu_speedup, 0x0018cb4 )
SPEEDUP( senkyua_speedup, 0x0018c9c )
SPEEDUP( batlball_speedup, 0x0018db4 )
SPEEDUP( viprp1_speedup, 0x001e2e0 )
SPEEDUP( viprp1o_speedup, 0x001d49c )
SPEEDUP( rdft_speedup, 0x00298d0 )
SPEEDUP( rf2_speedup, 0x00282ac )
SPEEDUP( rfjet_speedup, 0x002894c )

static void init_sei252(offs_t speedup)
{
	seibuspi_text_decrypt(memory_region(REGION_GFX1));
	seibuspi_bg_decrypt(memory_region(REGION_GFX2), memory_region_length(REGION_GFX2));
	seibuspi_sprite_decrypt(memory_region(REGION_GFX3), 0x400000);
	spi_common_init(0, speedup);
}

static DRIVER_INIT( senkyu )
{
	install_mem_read32_handler(0, 0x0018cb4, 0x0018cb7, senkyu_speedup);
	init_sei252(0x0018cb4);
}

static DRIVER_INIT( senkyua )
{
	install_mem_read32_handler(0, 0x0018c9c, 0x0018c9f, senkyua_speedup);
	init_sei252(0x0018c9c);
}

static DRIVER_INIT( batlball )
{
	install_mem_read32_handler(0, 0x0018db4, 0x0018db7, batlball_speedup);
	init_sei252(0x0018db4);
}

static DRIVER_INIT( viprp1 )
{
	install_mem_read32_handler(0, 0x001e2e0, 0x001e2e3, viprp1_speedup);
	init_sei252(0x001e2e0);
}

static DRIVER_INIT( viprp1o )
{
	install_mem_read32_handler(0, 0x001d49c, 0x001d49f, viprp1o_speedup);
	init_sei252(0x001d49c);
}

static DRIVER_INIT( rdft )
{
	install_mem_read32_handler(0, 0x00298d0, 0x00298d3, rdft_speedup);
	init_sei252(0x00298d0);
}

static DRIVER_INIT( rdft2 )
{
	install_mem_read32_handler(0, 0x00282ac, 0x00282af, rf2_speedup);
	seibuspi_rdft2_text_decrypt(memory_region(REGION_GFX1));
	seibuspi_rdft2_bg_decrypt(memory_region(REGION_GFX2), memory_region_length(REGION_GFX2));
	seibuspi_rise10_sprite_decrypt(memory_region(REGION_GFX3), 0x600000);
	spi_common_init(1, 0x00282ac);
}

static DRIVER_INIT( rfjet )
{
	install_mem_read32_handler(0, 0x002894c, 0x002894f, rfjet_speedup);
	seibuspi_rfjet_text_decrypt(memory_region(REGION_GFX1));
	seibuspi_rfjet_bg_decrypt(memory_region(REGION_GFX2), memory_region_length(REGION_GFX2));
	seibuspi_rise11_sprite_decrypt_rfjet(memory_region(REGION_GFX3), 0x800000);
	spi_common_init(1, 0x002894c);
}

static DRIVER_INIT( rdft22kc ) { init_rdft2(); }
static DRIVER_INIT( rfjet2kc ) { init_rfjet(); }


/*****************************************************************************/

#define ROM_LOAD24_BYTE(name,offset,length,hash)        ROMX_LOAD(name, offset, length, hash, ROM_SKIP(2))
#define ROM_LOAD24_WORD(name,offset,length,hash)        ROMX_LOAD(name, offset, length, hash, ROM_GROUPWORD | ROM_SKIP(1) | ROM_REVERSE)

/* SPI games */

ROM_START( senkyu )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("fb_1.211", 0x100000, 0x40000, CRC(20a3e5db) SHA1(f1109aeceac7993abc9093d09429718ffc292c77) )
	ROM_LOAD32_BYTE("fb_2.212", 0x100001, 0x40000, CRC(38e90619) SHA1(451ab5f4a5935bb779f9c245c1c4358e80d93c15) )
	ROM_LOAD32_BYTE("fb_3.210", 0x100002, 0x40000, CRC(226f0429) SHA1(69d0fe6671278d7fe215e455bb50abf631cdb484) )
	ROM_LOAD32_BYTE("fb_4.29",  0x100003, 0x40000, CRC(b46d66b7) SHA1(1acd0fea9384e1488b44661e0c99b9672a3f9803) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( senkyua )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("1.bin", 0x100000, 0x40000, CRC(6102c3fb) SHA1(4a55b41d916768f9601513db973b82077bca47c5) )
	ROM_LOAD32_BYTE("2.bin", 0x100001, 0x40000, CRC(d5b8ce46) SHA1(f6e4b8f51146179efb52ecb2b72fdeaee10b7282) )
	ROM_LOAD32_BYTE("3.bin", 0x100002, 0x40000, CRC(e27ceccd) SHA1(3d6b8e97e89939c72d1a5a4a3856025b5f548645) )
	ROM_LOAD32_BYTE("4.bin", 0x100003, 0x40000, CRC(7c6d4549) SHA1(efc6920a2e518afe849fb6fe191e7cd0bc483be5) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( batlball )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("1.211", 0x100000, 0x40000, CRC(d4e48f89) SHA1(10e43a9ff3f6f169de6352280a8a06e7f482271a) )
	ROM_LOAD32_BYTE("2.212", 0x100001, 0x40000, CRC(3077720b) SHA1(b65c3d02ac75eb56e0c5dc1bf6bb6a4e445a41cf) )
	ROM_LOAD32_BYTE("3.210", 0x100002, 0x40000, CRC(520d31e1) SHA1(998ae968113ab5b2891044187d93793903c13452) )
	ROM_LOAD32_BYTE("4.029", 0x100003, 0x40000, CRC(22419b78) SHA1(67475a654d4ad94e5dfda88cbe2f9c1b5ba6d2cc) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( batlballo )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1a.211", 0x100000, 0x40000, CRC(90340e8c) SHA1(303d3c5ffc1a64e1e4aa614105119d9d7768f516) )
	ROM_LOAD32_BYTE("seibu_2a.212", 0x100001, 0x40000, CRC(db655d3e) SHA1(bfd873e0d0daf3759778c76fe72fcf96e84250a4) )
	ROM_LOAD32_BYTE("seibu_3a.210", 0x100002, 0x40000, CRC(659a54a2) SHA1(a6c024e42b104a6198829f0f75baaa294fe9de6c) )
	ROM_LOAD32_BYTE("seibu_4a.029", 0x100003, 0x40000, CRC(51183421) SHA1(d97da1693e429cb7a061f08274070eb3e6966ec0) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu_6.413", 0x000000, 0x20000, CRC(338556f9) SHA1(dfab6e1562dd9c373aa094a3791ecd4cd3c9b6f5) )
	ROM_LOAD24_BYTE("seibu_5.48",  0x000002, 0x10000, CRC(6ccfb72e) SHA1(825b917ecd8495de23d55d2d2902d9d7c54ce4ed) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("seibu_7.216",   0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( batlballu )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("sen1.bin", 0x100000, 0x40000, CRC(13849bf0) SHA1(ffa829a8b8a05a8fbaf883a30759f2ad8071a85b) )
	ROM_LOAD32_BYTE("sen2.bin", 0x100001, 0x40000, CRC(2ae5f7e2) SHA1(cef9ddea8b1d21f20a48c2523c9420c1800720c8) )
	ROM_LOAD32_BYTE("sen3.bin", 0x100002, 0x40000, CRC(98e6f19f) SHA1(433f8463e63bba32730d3c098354f8c95257df3f) )
	ROM_LOAD32_BYTE("sen4.bin", 0x100003, 0x40000, CRC(1343ec56) SHA1(8ecc8d7b425ff6512ffa969a7f26423fa50ad258) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( batlballa )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("senkyua1.bin", 0x100000, 0x40000, CRC(ec3c4d4d) SHA1(6c57b8fbb77ce1615850842d06c054e88e240eef) )
	ROM_LOAD32_BYTE("2.212",        0x100001, 0x40000, CRC(3077720b) SHA1(b65c3d02ac75eb56e0c5dc1bf6bb6a4e445a41cf) )
	ROM_LOAD32_BYTE("3.210",        0x100002, 0x40000, CRC(520d31e1) SHA1(998ae968113ab5b2891044187d93793903c13452) )
	ROM_LOAD32_BYTE("4.029",        0x100003, 0x40000, CRC(22419b78) SHA1(67475a654d4ad94e5dfda88cbe2f9c1b5ba6d2cc) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region22.u1053", 0x000000, 0x100000, CRC(5fee8413) SHA1(6d6a62fa01293b4ba4b349a39820d024add6ea22) )
ROM_END

ROM_START( batlballe )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("1_10-16", 0x100000, 0x40000, CRC(6b1baa07) SHA1(29b8f4016e9bffdcdb6ec405cd443ca0a80de5d5) )
	ROM_LOAD32_BYTE("2_10-16", 0x100001, 0x40000, CRC(3c890639) SHA1(968c4a5efc5ebbe4e4cc81f834c286c02596c24e) )
	ROM_LOAD32_BYTE("3_10-16", 0x100002, 0x40000, CRC(8c30180e) SHA1(47b99b04e2e74f1ee5095aed3f45aba66cd3da3f) )
	ROM_LOAD32_BYTE("4_10-16", 0x100003, 0x40000, CRC(048c7aaa) SHA1(5eca2cdf4e6f077988509c3c42c86408b21ffbf1) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region22.u1053", 0x000000, 0x100000, CRC(5fee8413) SHA1(6d6a62fa01293b4ba4b349a39820d024add6ea22) )
ROM_END

ROM_START( batlballpt )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("senkyu_prog0_171195.u0211", 0x100000, 0x40000, CRC(dcc227b6) SHA1(10d72ebdd218887521226c82a61c2cdebcd9af99) )
	ROM_LOAD32_BYTE("2.u0212", 0x100001, 0x40000, CRC(03ab203f) SHA1(de5771883624440a9842b109b764b52811facfad) )
	ROM_LOAD32_BYTE("3.u0210", 0x100002, 0x40000, CRC(9eb9c8b4) SHA1(fc4f8feac3840776f842d03d86b37a669bba4f12) )
	ROM_LOAD32_BYTE("4.u029",  0x100003, 0x40000, CRC(c37ae2a5) SHA1(ee33f7baf852ef36b4f7f71a08d82acf1e7460b8) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("fb_6.413", 0x000000, 0x20000, CRC(b57115c9) SHA1(eb95f416f522032ca949bfb6348f1ff824101f2d) )
	ROM_LOAD24_BYTE("fb_5.48",  0x000002, 0x10000, CRC(440a9ae3) SHA1(3f57e6da91f0dac2d816c873759f1e1d3259caf1) )

	ROM_REGION( 0x300000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("fb_bg-1d.415", 0x000000, 0x200000, CRC(eae7a1fc) SHA1(26d8a9f4e554848977ec1f6a8aad8751b558a8d4) )
	ROM_LOAD24_BYTE("fb_bg-1p.410", 0x000002, 0x100000, CRC(b46e774e) SHA1(00b6c1d0b0ea37f4354acab543b270c0bf45896d) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("fb_obj-1.322", 0x000000, 0x400000, CRC(29f86f68) SHA1(1afe809ce00a25f8b27543e4188edc3e3e604951) )
	ROM_LOAD("fb_obj-2.324", 0x400000, 0x400000, CRC(c9e3130b) SHA1(12b5d5363142e8efb3b7fc44289c0afffa5011c6) )
	ROM_LOAD("fb_obj-3.323", 0x800000, 0x400000, CRC(f6c3bc49) SHA1(d0eb9c6aa3954d94e3a442a48e0fe6cc279f5513) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("fb_pcm-1.215",  0x000000, 0x080000, CRC(1d83891c) SHA1(09502437562275c14c0f3a0e62b19e91bedb4693) )
	ROM_CONTINUE(                    0x400000, 0x080000 )
	ROM_LOAD32_BYTE("fb_7.216",      0x800000, 0x080000, CRC(874d7b59) SHA1(0236753636c9a818780b23f5f506697b9f6d93c7) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region96.u1053", 0x000000, 0x100000, CRC(a0ebae75) SHA1(31f7955a529a4e2492b530e54878ed7a13f49c94) )
ROM_END

ROM_START( viprp1 )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu1.211", 0x000000, 0x80000, CRC(e5caf4ff) SHA1(7c87a4e8e8dacfb7cc0be8f778352bce2801e59b) )
	ROM_LOAD32_BYTE("seibu2.212", 0x000001, 0x80000, CRC(688a998e) SHA1(0c48374b6800cd00e3ee96c0fb12119a680b091d) )
	ROM_LOAD32_BYTE("seibu3.210", 0x000002, 0x80000, CRC(990fa76a) SHA1(7619a631d6f83b3677eb47f984aff684e9518d6d) )
	ROM_LOAD32_BYTE("seibu4.29",  0x000003, 0x80000, CRC(13e3e343) SHA1(aac0c7450059847f53b5081e4abf26303a50f999) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu5.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("seibu6.u048",  0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_regionbe.u1053", 0x000000, 0x100000, CRC(a4c181d0) SHA1(0aeea4cac4030f60ee77d62deca6b67c318c0866) )
ROM_END

ROM_START( viprp1k )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu1.211", 0x000000, 0x80000, CRC(5495c930) SHA1(056237965aefa4c0ea7782e0ee5ba1b58a045d7a) )
	ROM_LOAD32_BYTE("seibu2.212", 0x000001, 0x80000, CRC(e0ad22ae) SHA1(1911d17f0b462a9bada9efee85e531f2445e4ac6) )
	ROM_LOAD32_BYTE("seibu3.210", 0x000002, 0x80000, CRC(db7bcb90) SHA1(45f0a44e24d7b4b996d833e579f405bcf7584563) )
	ROM_LOAD32_BYTE("seibu4.29",  0x000003, 0x80000, CRC(c6188bf9) SHA1(746a205c428a080c36d0daf1d4f6b2b0f8efb977) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu5.u0413", 0x000000, 0x20000, CRC(1a35f2d8) SHA1(cd9b140f144a8c72756e18913eaef121963be341) )
	ROM_LOAD24_BYTE("seibu6.u048",  0x000002, 0x10000, CRC(e88bf049) SHA1(62f35840dc90b505118ed81ac0d75397a689783b) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region24.u1053", 0x000000, 0x100000, CRC(72a33dc4) SHA1(65a52f576ca4d240418fedd9a4922edcd6c0c8d1) )
ROM_END

ROM_START( viprp1u )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu1.u0211", 0x000000, 0x80000, CRC(3f412b80) SHA1(ccffce101d20971278c0f6c5f4efcf3ab687aba6) ) /* New version, "=U.S.A=" seems part of title */
	ROM_LOAD32_BYTE("seibu2.u0212", 0x000001, 0x80000, CRC(2e6c2376) SHA1(b6e660dc7c89cf565c6e055683e84ffcf8179709) )
	ROM_LOAD32_BYTE("seibu3.u0210", 0x000002, 0x80000, CRC(c38f7b4e) SHA1(d5bf2c7f2f6c812c65005facfd40ac6d3b61f29d) )
	ROM_LOAD32_BYTE("seibu4.u029",  0x000003, 0x80000, CRC(523cbef3) SHA1(5d15261b8fb108e0ba4dfd14d259984ef81ce877) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu5.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("seibu6.u048",  0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( viprp1ua )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibus_1", 0x000000, 0x80000, CRC(882c299c) SHA1(36309b99764c684bd17eb512e661bafd3f3298e2) ) /* New version, "=U.S.A=" seems part of title */
	ROM_LOAD32_BYTE("seibus_2", 0x000001, 0x80000, CRC(6ce586e9) SHA1(511731996638666cbe81a1d97affce855e255bf7) )
	ROM_LOAD32_BYTE("seibus_3", 0x000002, 0x80000, CRC(f9dd9128) SHA1(ff7460699424de9e9d953343c42e0ef0fa1f0e30) )
	ROM_LOAD32_BYTE("seibus_4", 0x000003, 0x80000, CRC(cb06440c) SHA1(c73647fb72c1579f05298fd884d8aeb3765bfff4) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu5.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("seibu6.u048",  0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( viprp1j )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("v_1-n.211", 0x000000, 0x80000, CRC(55f10b72) SHA1(2a1ebaa969f346bf3659ed8b0f469dce9eaf3b4b) )
	ROM_LOAD32_BYTE("v_2-n.212", 0x000001, 0x80000, CRC(0f888283) SHA1(7e5ac81279b9c7a06f07cb8ae76938cdd5c9beee) )
	ROM_LOAD32_BYTE("v_3-n.210", 0x000002, 0x80000, CRC(842434ac) SHA1(982d219c1d329122789c552208db2f4aaa4af7e4) )
	ROM_LOAD32_BYTE("v_4-n.29",  0x000003, 0x80000, CRC(a3948824) SHA1(fe076951427126c8b7fe81be84ecf0699597225b) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu5.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("seibu6.u048",  0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( viprp1s )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("viper_prg0.bin", 0x000000, 0x80000, CRC(ed9980b8) SHA1(bc324e9121ee1e55237bd91681f163ec7790de4c) )
	ROM_LOAD32_BYTE("viper_prg1.bin", 0x000001, 0x80000, CRC(9d4d3486) SHA1(ded6fa32b973046e50c40c40c446590b5f6d0b76) )
	ROM_LOAD32_BYTE("viper_prg2.bin", 0x000002, 0x80000, CRC(d7ea460b) SHA1(aed10adacd073f7d2b35f12ba4b7876e5c99d142) )
	ROM_LOAD32_BYTE("viper_prg3.bin", 0x000003, 0x80000, CRC(ca6df094) SHA1(921eec141ce2d449047172fa9cdf39d459b5cc7b) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu5.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("seibu6.u048",  0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region9c.u1053", 0x000000, 0x100000, CRC(d73d640c) SHA1(61a99af2a153de9d53e28872a2493e2ba797a325) )
ROM_END

ROM_START( viprp1h )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("viper_prg0_010995.u0211", 0x000000, 0x80000, CRC(e42fcc93) SHA1(5b2848a1da0e5d37e04ac646e67bbb84678c0292) ) /* same code as viprp1s, different region byte value */
	ROM_LOAD32_BYTE("viper_prg1_010995.u0212", 0x000001, 0x80000, CRC(9d4d3486) SHA1(ded6fa32b973046e50c40c40c446590b5f6d0b76) )
	ROM_LOAD32_BYTE("viper_prg2_010995.u0210", 0x000002, 0x80000, CRC(d7ea460b) SHA1(aed10adacd073f7d2b35f12ba4b7876e5c99d142) )
	ROM_LOAD32_BYTE("viper_prg3_010995.u029",  0x000003, 0x80000, CRC(ca6df094) SHA1(921eec141ce2d449047172fa9cdf39d459b5cc7b) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("viper_fix_010995.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("viper_fixp_010995.u048", 0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region90.u1053", 0x000000, 0x100000, CRC(8da617a2) SHA1(29c6ee05ed1c9a428a89d625b72692296c38424b) )
ROM_END

ROM_START( viprp1t )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("viper_prg0_010995.u0211", 0x000000, 0x80000, CRC(f998dcf7) SHA1(c2dc876e4dc51062caf3d0df7c3c9cc9a5201760) ) /* same code as viprp1s and viprp1h, different region byte value */
	ROM_LOAD32_BYTE("viper_prg1_010995.u0212", 0x000001, 0x80000, CRC(9d4d3486) SHA1(ded6fa32b973046e50c40c40c446590b5f6d0b76) )
	ROM_LOAD32_BYTE("viper_prg2_010995.u0210", 0x000002, 0x80000, CRC(d7ea460b) SHA1(aed10adacd073f7d2b35f12ba4b7876e5c99d142) )
	ROM_LOAD32_BYTE("viper_prg3_010995.u029",  0x000003, 0x80000, CRC(ca6df094) SHA1(921eec141ce2d449047172fa9cdf39d459b5cc7b) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("viper_fix_010995.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("viper_fixp_010995.u048", 0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( viprp1pt )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("viper_prg0_010995.u0211", 0x000000, 0x80000, CRC(0d4c69a6) SHA1(d46706419c280838050a44cc27d6e469c021f295) ) /* same code as viprp1s, viprp1h and viprp1t , different region byte value */
	ROM_LOAD32_BYTE("viper_prg1_010995.u0212", 0x000001, 0x80000, CRC(9d4d3486) SHA1(ded6fa32b973046e50c40c40c446590b5f6d0b76) )
	ROM_LOAD32_BYTE("viper_prg2_010995.u0210", 0x000002, 0x80000, CRC(d7ea460b) SHA1(aed10adacd073f7d2b35f12ba4b7876e5c99d142) )
	ROM_LOAD32_BYTE("viper_prg3_010995.u029",  0x000003, 0x80000, CRC(ca6df094) SHA1(921eec141ce2d449047172fa9cdf39d459b5cc7b) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("viper_fix_010995.u0413", 0x000000, 0x20000, CRC(5ece677c) SHA1(b782cf3296f866f79fafa69ff719211c9d4026df) )
	ROM_LOAD24_BYTE("viper_fixp_010995.u048", 0x000002, 0x10000, CRC(44844ef8) SHA1(bcbe24d2ffb64f9165ba4ab7de27f44b99b5ff5a) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region96.u1053", 0x000000, 0x100000, CRC(a0ebae75) SHA1(31f7955a529a4e2492b530e54878ed7a13f49c94) )
ROM_END

ROM_START( viprp1ot )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("ov1.bin", 0x000000, 0x80000, CRC(cbad0e28) SHA1(fbc9b3b243ae0d556f41e8bef5f09489bb9e302b) )
	ROM_LOAD32_BYTE("ov2.bin", 0x000001, 0x80000, CRC(0e2bbcb5) SHA1(5e53d60357fb0f9efa441261fac79e153eb35f3d) )
	ROM_LOAD32_BYTE("ov3.bin", 0x000002, 0x80000, CRC(0e86686b) SHA1(0af207ea77ef378364d80d20ecbfba2f043f2405) )
	ROM_LOAD32_BYTE("ov4.bin", 0x000003, 0x80000, CRC(9d7dd325) SHA1(550a8b5ed60e7ac50c40ec3eaa2cd6462be4a619) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("v_5-o.413", 0x000000, 0x20000, CRC(6d863acc) SHA1(3e3e14f51b9394b24d7cbf562f1cfffc9ec2216d) )
	ROM_LOAD24_BYTE("v_6-o.48",  0x000002, 0x10000, CRC(fe7cb8f7) SHA1(55c7ab977c3666c8770deb62718d535673ffd4f8) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( viprp1oj )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("v_1-o.211", 0x000000, 0x80000, CRC(4430be64) SHA1(96501a490042c289060d8510f6f79fbf64f79c1a) )
	ROM_LOAD32_BYTE("v_2-o.212", 0x000001, 0x80000, CRC(ffbd88f7) SHA1(cd7f291117dd18bd80fb1130eb87936ff7517ee3) )
	ROM_LOAD32_BYTE("v_3-o.210", 0x000002, 0x80000, CRC(6146db39) SHA1(04e68bfff320a3ffcb47686fa012a038538adc1a) )
	ROM_LOAD32_BYTE("v_4-o.29",  0x000003, 0x80000, CRC(dc8dd2b6) SHA1(20970706240c38c54084b4ae24b7ad23b31aa3de) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("v_5-o.413", 0x000000, 0x20000, CRC(6d863acc) SHA1(3e3e14f51b9394b24d7cbf562f1cfffc9ec2216d) )
	ROM_LOAD24_BYTE("v_6-o.48",  0x000002, 0x10000, CRC(fe7cb8f7) SHA1(55c7ab977c3666c8770deb62718d535673ffd4f8) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( viprp1hk )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1", 0x000000, 0x80000, CRC(283ba7b7) SHA1(28122e04b72f1163c69f3f845f6a493fdb6ed652) ) /* Old Version, "=HONG KONG=" seems part of title */
	ROM_LOAD32_BYTE("seibu_2", 0x000001, 0x80000, CRC(2c4db249) SHA1(a6372c9a3cde5f262ec5ef446945f6d3ad506e88) )
	ROM_LOAD32_BYTE("seibu_3", 0x000002, 0x80000, CRC(91989503) SHA1(8c215fac200cc693396dbd57e0939e7efe883342) )
	ROM_LOAD32_BYTE("seibu_4", 0x000003, 0x80000, CRC(12c9582d) SHA1(a79e26514e5ab8703a7a8c3ac39b359cfa4117c1) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("seibu_5", 0x000000, 0x20000, CRC(80920fed) SHA1(b35ed080925f6d0a0b6d2d1ab4fa919f625b1e6a) ) /* Different from both "new" & "old" versions */
	ROM_LOAD24_BYTE("seibu_6", 0x000002, 0x10000, CRC(e71a8722) SHA1(3e0133fe1f85058ca6d9ac59d731f342c6b50e92) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("v_bg-11.415",  0x000000, 0x200000, CRC(6fc96736) SHA1(12df47d8af2c1febc1bce5bcf3218766447885bd) )
	ROM_LOAD24_BYTE("v_bg-12.415",  0x000002, 0x100000, CRC(d3c7281c) SHA1(340bca1f31486609b3c34dd7830362a216ff648e) )
	ROM_LOAD24_WORD("v_bg-21.410",  0x300000, 0x100000, CRC(d65b4318) SHA1(6522970d95ffa7fa2f32e0b5b4f0eb69e0286b36) )
	ROM_LOAD24_BYTE("v_bg-22.416",  0x300002, 0x080000, CRC(24a0a23a) SHA1(0b0330717620e3f3274a25845d9edaf8023b9db2) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("v_obj-1.322",  0x000000, 0x400000, CRC(3be5b631) SHA1(fd1064428d28ca166a9267b968c0ba846cfed656) )
	ROM_LOAD("v_obj-2.324",  0x400000, 0x400000, CRC(924153b4) SHA1(db5dadcfb4cd5e6efe9d995085936ce4f4eb4254) )
	ROM_LOAD("v_obj-3.323",  0x800000, 0x400000, CRC(e9fb9062) SHA1(18e97b4c5cced2b529e6e72d8041c6f78fcec76e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_BYTE("v_pcm.215",  0x000000, 0x080000, CRC(e3111b60) SHA1(f7a7747f29c392876e43efcb4e6c0741454082f2) )
	ROM_CONTINUE(                 0x400000, 0x080000 )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region22.u1053", 0x000000, 0x100000, CRC(5fee8413) SHA1(6d6a62fa01293b4ba4b349a39820d024add6ea22) )
ROM_END

ROM_START( rdft )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("raiden-fi_prg0_121196.u0211", 0x000000, 0x80000, CRC(adcb5dbc) SHA1(3831becd1e052d81dd00ee098ee630fe35164df8) )
	ROM_LOAD32_BYTE("raiden-fi_prg1_121196.u0212", 0x000001, 0x80000, CRC(60c5b92e) SHA1(ca67f97f9e7d8a21667dc59e7d390dff91179b08) )
	ROM_LOAD32_BYTE("raiden-fi_prg2_121196.u0210", 0x000002, 0x80000, CRC(44b86db5) SHA1(bb05c6d27af86084cd3e17a189826c836f229c8b) )
	ROM_LOAD32_BYTE("raiden-fi_prg3_121196.u029",  0x000003, 0x80000, CRC(e70727ce) SHA1(2e4e896d50cf086e682054cb2e9c223af04cd0cb) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( rdftj )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("gd_1.211", 0x000000, 0x80000, CRC(f6b2cbdc) SHA1(040c4ff961c8be388c8279b06b777d528c2acc1b) )
	ROM_LOAD32_BYTE("gd_2.212", 0x000001, 0x80000, CRC(1982f812) SHA1(4f12fc3fd7f7a4beda4d29cc81e3a58d255e441f) )
	ROM_LOAD32_BYTE("gd_3.210", 0x000002, 0x80000, CRC(b0f59f44) SHA1(d44fe074ddab35cd0190535cd9fbd7f9e49312a4) )
	ROM_LOAD32_BYTE("gd_4.29",  0x000003, 0x80000, CRC(cd8705bd) SHA1(b19a1486d6b899a134d7b518863ddc8f07967e8b) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdftja )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("rf1.bin", 0x000000, 0x80000, CRC(46861b75) SHA1(079c589c490d49f7ec97a7e68c5b6e7e37872827) )
	ROM_LOAD32_BYTE("rf2.bin", 0x000001, 0x80000, CRC(6388ed11) SHA1(aebbccfb0f704cdceb45ea71216275dd83880e15) )
	ROM_LOAD32_BYTE("rf3.bin", 0x000002, 0x80000, CRC(beafcd24) SHA1(2dbc47ecef6f898a371a841df2c72151da9c5a8d) )
	ROM_LOAD32_BYTE("rf4.bin", 0x000003, 0x80000, CRC(5236f45f) SHA1(8b05d977d3d07796007a00a52d2396475dc2f7dc) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdftu )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("rdftu_gd_1.211", 0x000000, 0x80000, CRC(47810c48) SHA1(8dc8848d3e7467ea887c50fd5675fba2cc741121) )
	ROM_LOAD32_BYTE("rdftu_gd_2.212", 0x000001, 0x80000, CRC(13911750) SHA1(8899accb059ed84170924750bb39ae7383ebd959) )
	ROM_LOAD32_BYTE("rdftu_gd_3.210", 0x000002, 0x80000, CRC(10761b03) SHA1(e67db2e7c2176987419158fc4cee00fd9b99d03f) )
	ROM_LOAD32_BYTE("rdftu_gd_4.29",  0x000003, 0x80000, CRC(e5a3f01d) SHA1(5ca338f85a020d43d2618f88e798a076d13a5c7f) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( rdftauge )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu.1.u0211", 0x000000, 0x80000, CRC(3e79da3c) SHA1(8d33da1dadb791ff97b353532ca647eb462c2ae4) )
	ROM_LOAD32_BYTE("seibu.2.u0212", 0x000001, 0x80000, CRC(a6fbf98c) SHA1(ce0f5f6f1f5656dbac0a6b977795026276c8fa86) )
	ROM_LOAD32_BYTE("seibu.3.u0210", 0x000002, 0x80000, CRC(ad31cc17) SHA1(12b735519cad190887cdbc6680d879f791ab3726) )
	ROM_LOAD32_BYTE("seibu.4.u029",  0x000003, 0x80000, CRC(756d99ae) SHA1(38c234acf3e8204a29a9adb077328d37d97cfd6d) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0416", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217", 0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                         0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",      0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region9e.u1053", 0x000000, 0x100000, CRC(7ad6f17e) SHA1(9a2cc77a4f86c00208f739bd53aca4f55adf7ea7) )
ROM_END

ROM_START( rdftua )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211",        0x000000, 0x080000, CRC(ddbadc30) SHA1(d419847db8b7f4ca6f161bfa309314eafeea8b40) ) // socket is silkscreened on pcb PRG0 - sldh
	ROM_LOAD32_BYTE("raiden-f_prg2.u0212",  0x000001, 0x080000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_WORD("raiden-f_prg34.u0219", 0x000002, 0x100000, CRC(63f01d17) SHA1(74dbd0417b974583da87fc6c7a081b03fd4e16b8) ) // socket is silkscreened on pcb PRG23

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("raiden-f_fix.u0425", 0x000000, 0x20000, CRC(2be2936b) SHA1(9e719f7328a52af220b6f084c1e0990ca6e2d533) ) // socket is silkscreened on pcb FIX01
	ROM_LOAD24_BYTE("seibu_7.u048",       0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) ) // socket is silkscreened on pcb FIXP

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) ) // pads are silkscreened on pcb OBJ3

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("raiden-f_pcm2.u0217", 0x000000, 0x100000, CRC(3f8d4a48) SHA1(30664a2908daaeaee58f7e157516b522c952e48d) ) // pads are silkscreened SOUND0
	ROM_CONTINUE(                          0x400000, 0x100000 )
	/* SOUND1 socket is unpopulated */

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( rdftjb )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211",        0x000000, 0x080000, CRC(b70afcc2) SHA1(70ac545a9fc30df310254997674878fbc2c2d718) ) // socket is silkscreened on pcb PRG0 - sldh
	ROM_LOAD32_BYTE("raiden-f_prg2.u0212",  0x000001, 0x080000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_WORD("raiden-f_prg34.u0219", 0x000002, 0x100000, CRC(63f01d17) SHA1(74dbd0417b974583da87fc6c7a081b03fd4e16b8) ) // socket is silkscreened on pcb PRG23

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("raiden-f_fix.u0425", 0x000000, 0x20000, CRC(2be2936b) SHA1(9e719f7328a52af220b6f084c1e0990ca6e2d533) ) // socket is silkscreened on pcb FIX01
	ROM_LOAD24_BYTE("seibu_7.u048",       0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) ) // socket is silkscreened on pcb FIXP

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) ) // pads are silkscreened on pcb OBJ3

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("raiden-f_pcm2.u0217", 0x000000, 0x100000, CRC(3f8d4a48) SHA1(30664a2908daaeaee58f7e157516b522c952e48d) ) // pads are silkscreened SOUND0
	ROM_CONTINUE(                          0x400000, 0x100000 )
	/* SOUND1 socket is unpopulated */

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdftau )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("1.u0211", 0x000000, 0x80000, CRC(6339c60d) SHA1(871d5bc9fc695651ceb6fcfdab32084320fe239d) )
	ROM_LOAD32_BYTE("2.u0212", 0x000001, 0x80000, CRC(a88bda02) SHA1(27dc720d28f56cf443a4eb0bbaaf4bf3b194056d) )
	ROM_LOAD32_BYTE("3.u0210", 0x000002, 0x80000, CRC(a73e337e) SHA1(93323875c676f38eca3298fcf4a34911db2d78a8) )
	ROM_LOAD32_BYTE("4.u029",  0x000003, 0x80000, CRC(8cc628f0) SHA1(7534eae8a1ea461adad483002b3cecf132e0e325) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region9e.u1053", 0x000000, 0x100000, CRC(7ad6f17e) SHA1(9a2cc77a4f86c00208f739bd53aca4f55adf7ea7) )
ROM_END

ROM_START( rdftam )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211",        0x000000, 0x080000, CRC(156d8db0) SHA1(93662b3ee494e37a56428a7aa3dad7a957835950) ) // socket is silkscreened on pcb PRG0 - sldh
	ROM_LOAD32_BYTE("raiden-f_prg2.u0212",  0x000001, 0x080000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_WORD("raiden-f_prg34.u0219", 0x000002, 0x100000, CRC(63f01d17) SHA1(74dbd0417b974583da87fc6c7a081b03fd4e16b8) ) // socket is silkscreened on pcb PRG23

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("raiden-f_fix.u0425", 0x000000, 0x20000, CRC(2be2936b) SHA1(9e719f7328a52af220b6f084c1e0990ca6e2d533) ) // socket is silkscreened on pcb FIX01
	ROM_LOAD24_BYTE("seibu_7.u048",       0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) ) // socket is silkscreened on pcb FIXP

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) ) // pads are silkscreened on pcb OBJ3

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("raiden-f_pcm2.u0217", 0x000000, 0x100000, CRC(3f8d4a48) SHA1(30664a2908daaeaee58f7e157516b522c952e48d) ) // pads are silkscreened SOUND0
	ROM_CONTINUE(                          0x400000, 0x100000 )
	/* SOUND1 socket is unpopulated */

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region22.u1053", 0x000000, 0x100000, CRC(5fee8413) SHA1(6d6a62fa01293b4ba4b349a39820d024add6ea22) )
ROM_END

ROM_START( rdftadi )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211",        0x000000, 0x080000, CRC(fc0e2885) SHA1(79621155d992d504e993bd3ee0d6ff3903bd5415) ) // socket is silkscreened on pcb PRG0 - sldh
	ROM_LOAD32_BYTE("raiden-f_prg2.u0212",  0x000001, 0x080000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_WORD("raiden-f_prg34.u0219", 0x000002, 0x100000, CRC(63f01d17) SHA1(74dbd0417b974583da87fc6c7a081b03fd4e16b8) ) // socket is silkscreened on pcb PRG23

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("raiden-f_fix.u0425", 0x000000, 0x20000, CRC(2be2936b) SHA1(9e719f7328a52af220b6f084c1e0990ca6e2d533) ) // socket is silkscreened on pcb FIX01
	ROM_LOAD24_BYTE("seibu_7.u048",       0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) ) // socket is silkscreened on pcb FIXP

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) ) // pads are silkscreened on pcb OBJ3

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("raiden-f_pcm2.u0217", 0x000000, 0x100000, CRC(3f8d4a48) SHA1(30664a2908daaeaee58f7e157516b522c952e48d) ) // pads are silkscreened SOUND0
	ROM_CONTINUE(                          0x400000, 0x100000 )
	/* SOUND1 socket is unpopulated */

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region24.u1053", 0x000000, 0x100000, CRC(72a33dc4) SHA1(65a52f576ca4d240418fedd9a4922edcd6c0c8d1) )
ROM_END

ROM_START( rdfta )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211", 0x000000, 0x80000, CRC(c3bb2e58) SHA1(399ac4b387ba38f5fdad5c4172b2d3baeafd8773) )
	ROM_LOAD32_BYTE("seibu_2.u0212", 0x000001, 0x80000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) )
	ROM_LOAD32_BYTE("seibu_3.u0210", 0x000002, 0x80000, CRC(47fc3c96) SHA1(7378f8caa847f89f235b5be6779118721076873b) )
	ROM_LOAD32_BYTE("seibu_4.u029",  0x000003, 0x80000, CRC(271bdd4b) SHA1(0a805568cbd6a9c18bdb755a41972ff6bba9e6eb) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region82.u1053", 0x000000, 0x100000, CRC(4f463a87) SHA1(0e27904745da61a3ba7c48c5b4c7d45989bbd05b) )
ROM_END

ROM_START( rdftgb )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211", 0x000000, 0x80000, CRC(2403035f) SHA1(9763cca3864d6127050e1507b572efa68f664b3c) )
	ROM_LOAD32_BYTE("seibu_2.u0212", 0x000001, 0x80000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) )
	ROM_LOAD32_BYTE("seibu_3.u0210", 0x000002, 0x80000, CRC(47fc3c96) SHA1(7378f8caa847f89f235b5be6779118721076873b) )
	ROM_LOAD32_BYTE("seibu_4.u029",  0x000003, 0x80000, CRC(271bdd4b) SHA1(0a805568cbd6a9c18bdb755a41972ff6bba9e6eb) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region8c.u1053", 0x000000, 0x100000, CRC(b836dc5b) SHA1(80400429970f8997978ee723d3067a39ebc0e126) )
ROM_END

ROM_START( rdftgr )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211", 0x000000, 0x80000, CRC(ca0d6273) SHA1(f761d68ec9a49d66d9f3adc663663f716cdd3735) )
	ROM_LOAD32_BYTE("seibu_2.u0212", 0x000001, 0x80000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) )
	ROM_LOAD32_BYTE("seibu_3.u0210", 0x000002, 0x80000, CRC(47fc3c96) SHA1(7378f8caa847f89f235b5be6779118721076873b) )
	ROM_LOAD32_BYTE("seibu_4.u029",  0x000003, 0x80000, CRC(271bdd4b) SHA1(0a805568cbd6a9c18bdb755a41972ff6bba9e6eb) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region8e.u1053", 0x000000, 0x100000, CRC(15dd4929) SHA1(e0f68e9e4d775d70e85ea6b5dd29beeb0e940b1c) )
ROM_END

ROM_START( rdftit )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211", 0x000000, 0x80000, CRC(de0c3e3c) SHA1(b00225bad282e46b5825608f76eea6670bfe5527) )
	ROM_LOAD32_BYTE("seibu_2.u0212", 0x000001, 0x80000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) )
	ROM_LOAD32_BYTE("seibu_3.u0210", 0x000002, 0x80000, CRC(47fc3c96) SHA1(7378f8caa847f89f235b5be6779118721076873b) )
	ROM_LOAD32_BYTE("seibu_4.u029",  0x000003, 0x80000, CRC(271bdd4b) SHA1(0a805568cbd6a9c18bdb755a41972ff6bba9e6eb) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // some mask ROMs might be labeled GD BG1-D ect
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region92.u1053", 0x000000, 0x100000, CRC(204d82d0) SHA1(444f4aefa27d8f5d1a2f7f08f826ea84b0ccbd02) )
ROM_END

ROM_START( rdftadia )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211", 0x000000, 0x80000, CRC(fc0e2885) SHA1(79621155d992d504e993bd3ee0d6ff3903bd5415) )
	ROM_LOAD32_BYTE("seibu_2.u0212", 0x000001, 0x80000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) )
	ROM_LOAD32_BYTE("seibu_3.u0210", 0x000002, 0x80000, CRC(47fc3c96) SHA1(7378f8caa847f89f235b5be6779118721076873b) )
	ROM_LOAD32_BYTE("seibu_4.u029",  0x000003, 0x80000, CRC(271bdd4b) SHA1(0a805568cbd6a9c18bdb755a41972ff6bba9e6eb) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE )
	ROM_LOAD24_BYTE("seibu_5.u0423", 0x000000, 0x10000, CRC(8f8d4e14) SHA1(06c803975767ae98f40ba7ac5764a5bc8baa3a30) )
	ROM_LOAD24_BYTE("seibu_6.u0424", 0x000001, 0x10000, CRC(6ac64968) SHA1(ec395205c24c4f864a1f805bb0d4641562d4faa9) )
	ROM_LOAD24_BYTE("seibu_7.u048",  0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) )

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE )
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0415", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) )
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0410", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) )
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0424", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) )
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u049",  0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) )

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) )
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) )
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 )
	ROM_LOAD32_WORD("gun_dogs_pcm.u0217",  0x000000, 0x100000, CRC(31253ad7) SHA1(c81c8d50f8f287f5cbfaec77b30d969b01ce11a9) )
	ROM_CONTINUE(                          0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu_8.u0216",       0x800000, 0x080000, CRC(f88cb6e4) SHA1(fb35b41307b490d5d08e4b8a70f8ff4ce2ca8105) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region24.u1053", 0x000000, 0x100000, CRC(72a33dc4) SHA1(65a52f576ca4d240418fedd9a4922edcd6c0c8d1) )
ROM_END

ROM_START( rdft2 )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0.tun", 0x000000, 0x80000, CRC(3cb3fdca) SHA1(4b472dfd65c7bbbcb92a295aa73b0fa70581455b) )
	ROM_LOAD32_BYTE("prg1.bin", 0x000001, 0x80000, CRC(cab55d88) SHA1(246e13880d34b6c7c3f4ab5e18fa8a0547c03d9d) )
	ROM_LOAD32_BYTE("prg2.bin", 0x000002, 0x80000, CRC(83758b0e) SHA1(63adb2d09e7bd7dba47a55b3b579d543dfb553e3) )
	ROM_LOAD32_BYTE("prg3.bin", 0x000003, 0x80000, CRC(084fb5e4) SHA1(588bfe091662b88f02f528181a2f1d9c67c7b280) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( rdft2j )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0.sei", 0x000000, 0x80000, CRC(a60c4e7c) SHA1(7789b029d0ac084c7e5e662a7168edaed8f11633) )
	ROM_LOAD32_BYTE("prg1.bin", 0x000001, 0x80000, CRC(cab55d88) SHA1(246e13880d34b6c7c3f4ab5e18fa8a0547c03d9d) )
	ROM_LOAD32_BYTE("prg2.bin", 0x000002, 0x80000, CRC(83758b0e) SHA1(63adb2d09e7bd7dba47a55b3b579d543dfb553e3) )
	ROM_LOAD32_BYTE("prg3.bin", 0x000003, 0x80000, CRC(084fb5e4) SHA1(588bfe091662b88f02f528181a2f1d9c67c7b280) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdft2a )
	// The SUB8 board is also capable of having two 23C8100 roms at U0223 and U0219 for PRG instead of the four roms below.
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program, all are 27C040 */
	ROM_LOAD32_BYTE("seibu__1.u0211", 0x000000, 0x80000, CRC(046b3f0e) SHA1(033898f658d6007f891828835734422d4af36321) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_BYTE("seibu__2.u0212", 0x000001, 0x80000, CRC(cab55d88) SHA1(246e13880d34b6c7c3f4ab5e18fa8a0547c03d9d) ) // socket is silkscreened on pcb PRG2
	ROM_LOAD32_BYTE("seibu__3.u0221", 0x000002, 0x80000, CRC(83758b0e) SHA1(63adb2d09e7bd7dba47a55b3b579d543dfb553e3) ) // socket is silkscreened on pcb PRG3
	ROM_LOAD32_BYTE("seibu__4.u0220", 0x000003, 0x80000, CRC(084fb5e4) SHA1(588bfe091662b88f02f528181a2f1d9c67c7b280) ) // socket is silkscreened on pcb PRG4

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms - all are 27C512 */
	ROM_LOAD24_BYTE("seibu__5.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) ) // socket is silkscreened on pcb FIX0
	ROM_LOAD24_BYTE("seibu__6.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) ) // socket is silkscreened on pcb FIX1
	ROM_LOAD24_BYTE("seibu__7.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) ) // socket is silkscreened on pcb FIXP

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms - half are MX semiconductor MX23C3210MC, half are some sort of 23C1610 equivalent with no visible manufacturer name */
	ROM_LOAD24_WORD("raiden-f2bg-1d.u0535",   0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("raiden-f2__bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("raiden-f2bg-2d.u0536",   0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("raiden-f2__bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites - all are paired MX semconductor MX23C3210TC and MX23C1610TC mask roms */
	ROM_LOAD("raiden-f2obj-3.u0434", 0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) ) // pads are silkscreened on pcb OBJ3
	ROM_LOAD("raiden-f2obj-6.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) ) // pads are silkscreened on pcb OBJ3B
	ROM_LOAD("raiden-f2obj-2.u0431", 0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("raiden-f2obj-5.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) ) // pads are silkscreened on pcb OBJ2B
	ROM_LOAD("raiden-f2obj-1.u0429", 0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("raiden-f2obj-4.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) ) // pads are silkscreened on pcb OBJ1B

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms - sound0 is some sort of 23C1610 equivalent with no visible manufacturer name, sound1 is a 27C040 */
	ROM_LOAD32_WORD("raiden-f2__pcm.u0217", 0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) ) // pads are silkscreened on pcb SOUND0
	ROM_CONTINUE(                           0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu__8.u0222",       0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) ) // socket is silkscreened on pcb SOUND1

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region22.u1053", 0x000000, 0x100000, CRC(5fee8413) SHA1(6d6a62fa01293b4ba4b349a39820d024add6ea22) )
ROM_END

ROM_START( rdft2s )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program, all are 27C040 */
	ROM_LOAD32_BYTE("seibu__1.u0211", 0x000000, 0x80000, CRC(28b2a185) SHA1(a42adc166992629e96b1ad2fa0859643689fb5c4) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_BYTE("seibu__2.u0212", 0x000001, 0x80000, CRC(cab55d88) SHA1(246e13880d34b6c7c3f4ab5e18fa8a0547c03d9d) ) // socket is silkscreened on pcb PRG2
	ROM_LOAD32_BYTE("seibu__3.u0221", 0x000002, 0x80000, CRC(83758b0e) SHA1(63adb2d09e7bd7dba47a55b3b579d543dfb553e3) ) // socket is silkscreened on pcb PRG3
	ROM_LOAD32_BYTE("seibu__4.u0220", 0x000003, 0x80000, CRC(084fb5e4) SHA1(588bfe091662b88f02f528181a2f1d9c67c7b280) ) // socket is silkscreened on pcb PRG4

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms - all are 27C512 */
	ROM_LOAD24_BYTE("seibu__5.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) ) // socket is silkscreened on pcb FIX0
	ROM_LOAD24_BYTE("seibu__6.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) ) // socket is silkscreened on pcb FIX1
	ROM_LOAD24_BYTE("seibu__7.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) ) // socket is silkscreened on pcb FIXP

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms - half are MX semiconductor MX23C3210MC, half are some sort of 23C1610 equivalent with no visible manufacturer name */
	ROM_LOAD24_WORD("raiden-f2bg-1d.u0535",   0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("raiden-f2__bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("raiden-f2bg-2d.u0536",   0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("raiden-f2__bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites - all are paired MX semconductor MX23C3210TC and MX23C1610TC mask roms */
	ROM_LOAD("raiden-f2obj-3.u0434", 0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) ) // pads are silkscreened on pcb OBJ3
	ROM_LOAD("raiden-f2obj-6.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) ) // pads are silkscreened on pcb OBJ3B
	ROM_LOAD("raiden-f2obj-2.u0431", 0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("raiden-f2obj-5.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) ) // pads are silkscreened on pcb OBJ2B
	ROM_LOAD("raiden-f2obj-1.u0429", 0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("raiden-f2obj-4.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) ) // pads are silkscreened on pcb OBJ1B

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms - sound0 is some sort of 23C1610 equivalent with no visible manufacturer name, sound1 is a 27C040 */
	ROM_LOAD32_WORD("raiden-f2__pcm.u0217", 0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) ) // pads are silkscreened on pcb SOUND0
	ROM_CONTINUE(                           0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu__8.u0222",       0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) ) // socket is silkscreened on pcb SOUND1

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region9c.u1053", 0x000000, 0x100000, CRC(d73d640c) SHA1(61a99af2a153de9d53e28872a2493e2ba797a325) )
ROM_END

ROM_START( rdft2ja )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("rf2.1",     0x000000, 0x80000, CRC(391d5057) SHA1(a1849142cbf7344ac1279781597e27b3b8ae6127) )
	ROM_LOAD32_BYTE("rf2_2.bin", 0x000001, 0x80000, CRC(ec73a767) SHA1(83f3905afe49401793c0ea0193cb31d3ba1e1739) )
	ROM_LOAD32_BYTE("rf2_3.bin", 0x000002, 0x80000, CRC(e66243b2) SHA1(54e67af37a4586fd1afc79085ed433d599e1bb87) )
	ROM_LOAD32_BYTE("rf2_4.bin", 0x000003, 0x80000, CRC(92b7b73e) SHA1(128649b2a6a0616113bd0f9846fb6cf814ae326d) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rf2_5.bin", 0x000001, 0x10000, CRC(377cac2f) SHA1(f7c9323d79b77f6c8c02ba2c6cdca127d6e5cb5c) )
	ROM_LOAD24_BYTE("rf2_6.bin", 0x000000, 0x10000, CRC(42bd5372) SHA1(c38df85b25070db9640eac541f71c0511bab0c98) )
	ROM_LOAD24_BYTE("rf2_7.bin", 0x000002, 0x10000, CRC(1efaac7e) SHA1(8252af56dcb7a6306dc3422070176778e3c511c2) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdft2aa )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("rf2_1.bin", 0x000000, 0x80000, CRC(72198410) SHA1(ca4bc858f6bf247a343b0fdae1d1a3cdabc4a3c3) )
	ROM_LOAD32_BYTE("rf2_2.bin", 0x000001, 0x80000, CRC(ec73a767) SHA1(83f3905afe49401793c0ea0193cb31d3ba1e1739) )
	ROM_LOAD32_BYTE("rf2_3.bin", 0x000002, 0x80000, CRC(e66243b2) SHA1(54e67af37a4586fd1afc79085ed433d599e1bb87) )
	ROM_LOAD32_BYTE("rf2_4.bin", 0x000003, 0x80000, CRC(92b7b73e) SHA1(128649b2a6a0616113bd0f9846fb6cf814ae326d) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rf2_5.bin", 0x000001, 0x10000, CRC(377cac2f) SHA1(f7c9323d79b77f6c8c02ba2c6cdca127d6e5cb5c) )
	ROM_LOAD24_BYTE("rf2_6.bin", 0x000000, 0x10000, CRC(42bd5372) SHA1(c38df85b25070db9640eac541f71c0511bab0c98) )
	ROM_LOAD24_BYTE("rf2_7.bin", 0x000002, 0x10000, CRC(1efaac7e) SHA1(8252af56dcb7a6306dc3422070176778e3c511c2) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region24.u1053", 0x000000, 0x100000, CRC(72a33dc4) SHA1(65a52f576ca4d240418fedd9a4922edcd6c0c8d1) )
ROM_END

ROM_START( rdft2it )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu1.bin",0x000000, 0x80000, CRC(501b92a9) SHA1(3e1c5cc63906ec7b97a3478557ec2638c515d726) )
	ROM_LOAD32_BYTE("seibu2.bin",0x000001, 0x80000, CRC(ec73a767) SHA1(83f3905afe49401793c0ea0193cb31d3ba1e1739) )
	ROM_LOAD32_BYTE("seibu3.bin",0x000002, 0x80000, CRC(e66243b2) SHA1(54e67af37a4586fd1afc79085ed433d599e1bb87) )
	ROM_LOAD32_BYTE("seibu4.bin",0x000003, 0x80000, CRC(92b7b73e) SHA1(128649b2a6a0616113bd0f9846fb6cf814ae326d) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("seibu5.bin", 0x000001, 0x10000, CRC(377cac2f) SHA1(f7c9323d79b77f6c8c02ba2c6cdca127d6e5cb5c) )
	ROM_LOAD24_BYTE("seibu6.bin", 0x000000, 0x10000, CRC(42bd5372) SHA1(c38df85b25070db9640eac541f71c0511bab0c98) )
	ROM_LOAD24_BYTE("seibu7.bin", 0x000002, 0x10000, CRC(1efaac7e) SHA1(8252af56dcb7a6306dc3422070176778e3c511c2) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("seibu8.bin", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region92.u1053", 0x000000, 0x100000, CRC(204d82d0) SHA1(444f4aefa27d8f5d1a2f7f08f826ea84b0ccbd02) )
ROM_END

ROM_START( rdft2jb )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0.rom", 0x000000, 0x80000, CRC(fc42cab8) SHA1(2e33fc8d77fdc4ee58e93fc191d12f2fe9cc3c65) )
	ROM_LOAD32_BYTE("prg1.rom", 0x000001, 0x80000, CRC(a0f09dc5) SHA1(e8ad20be1f04752b0884571384d4490813ed82d9) )
	ROM_LOAD32_BYTE("prg2.rom", 0x000002, 0x80000, CRC(368580e0) SHA1(184036a0cbddbf79b62e388b93cb93b885faee88) )
	ROM_LOAD32_BYTE("prg3.rom", 0x000003, 0x80000, CRC(7ad45c01) SHA1(d782057658dd000c1cf0a4726a6ed821e6f2be67) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rf2_5.bin", 0x000001, 0x10000, CRC(377cac2f) SHA1(f7c9323d79b77f6c8c02ba2c6cdca127d6e5cb5c) )
	ROM_LOAD24_BYTE("rf2_6.bin", 0x000000, 0x10000, CRC(42bd5372) SHA1(c38df85b25070db9640eac541f71c0511bab0c98) )
	ROM_LOAD24_BYTE("rf2_7.bin", 0x000002, 0x10000, CRC(1efaac7e) SHA1(8252af56dcb7a6306dc3422070176778e3c511c2) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdft2jc )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0211", 0x000000, 0x80000, CRC(36b6407c) SHA1(b3f5bb3c582aca71e0c7d9da5951b30c7389cc24) )
	ROM_LOAD32_BYTE("seibu_2.u0212", 0x000001, 0x80000, CRC(65ee556e) SHA1(13311850aabba9fc373adfd5cd590c114505933f) )
	ROM_LOAD32_BYTE("seibu_3.u0221", 0x000002, 0x80000, CRC(d2458358) SHA1(18a9cfee77a6a09584bc3fb0073c822d12de5bf1) )
	ROM_LOAD32_BYTE("seibu_4.u0220", 0x000003, 0x80000, CRC(5c4412f9) SHA1(c72603bee3ce14f40d4bf5e3ae3f041b923edd57) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rdft2t )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0", 0x000000, 0x80000, CRC(7e8c3acc) SHA1(63f4f9f7df7fa028737d9f7dfae96795cde58541) )
	ROM_LOAD32_BYTE("prg1", 0x000001, 0x80000, CRC(22cb5b68) SHA1(35f86ad7771fe9aaac3904ed34a96d0cc10ef21c) )
	ROM_LOAD32_BYTE("prg2", 0x000002, 0x80000, CRC(3eca68dd) SHA1(98378654adf055d72ae685f90e36643c9d6419d7) )
	ROM_LOAD32_BYTE("prg3", 0x000003, 0x80000, CRC(4124daa4) SHA1(42f225c0328df59ffeacc215d37010f825bf507e) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rf2_5.bin", 0x000001, 0x10000, CRC(377cac2f) SHA1(f7c9323d79b77f6c8c02ba2c6cdca127d6e5cb5c) )
	ROM_LOAD24_BYTE("rf2_6.bin", 0x000000, 0x10000, CRC(42bd5372) SHA1(c38df85b25070db9640eac541f71c0511bab0c98) )
	ROM_LOAD24_BYTE("rf2_7.bin", 0x000002, 0x10000, CRC(1efaac7e) SHA1(8252af56dcb7a6306dc3422070176778e3c511c2) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region20.u1053", 0x000000, 0x100000, CRC(f2051161) SHA1(45cbd5fd9ae0ca0c5c3450bca5f6806ddce3c56f) )
ROM_END

ROM_START( rdft2u )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("1.bin", 0x000000, 0x80000, CRC(b7d6c866) SHA1(eefe63dfc641c3904dd150a10ffeb68137068725) )
	ROM_LOAD32_BYTE("2.bin", 0x000001, 0x80000, CRC(ff7747c5) SHA1(7481d0484001ff7367af56e8ea99f985cce405f2) )
	ROM_LOAD32_BYTE("3.bin", 0x000002, 0x80000, CRC(86e3d1a8) SHA1(2757cfda57c82dd0f66427caf54eb1f40e85740d) )
	ROM_LOAD32_BYTE("4.bin", 0x000003, 0x80000, CRC(2e409a76) SHA1(cf90aa14a07b5aa861f6f7cc9b1968171e532557) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u0434",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u0433", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u0431",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u0432", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u0429",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u0430", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm.u0217",    0x000000, 0x100000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(b7bd3703) SHA1(6427a7e6de10d6743d6e64b984a1d1c647f5643a) )

	ROM_REGION( 0x0345, REGION_USER4, ROMREGION_DISPOSE ) /* pals */
	ROM_LOAD("rm81.u0529.bin", 0x0000, 0x0117, CRC(acd55c8e) SHA1(b965e828fecd61b836aca337637e53d7360d9dc4) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm82.u0330.bin", 0x0117, 0x0117, CRC(64c71423) SHA1(1da3502bec0c843b7198d1d9ab60f9fd4b110a8e) ) // AMD PALCE16V8H-15SC/4
	ROM_LOAD("rm83.u0331.bin", 0x022e, 0x0117, CRC(6e10d66b) SHA1(995d2a0da680ec19ee253098c91a4780dd8403c6) ) // AMD PALCE16V8H-15SC/4

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( rfjet )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0.u0211", 0x000000, 0x80000, CRC(e5a3b304) SHA1(f7285f9c69c589fcc71082dc0b9225fdccec855f) )
	ROM_LOAD32_BYTE("prg1.u0212", 0x000001, 0x80000, CRC(395e6da7) SHA1(736f777cb1b6bf5541832b8ea89594738ca6d829) )
	ROM_LOAD32_BYTE("prg2.u0221", 0x000002, 0x80000, CRC(82f7a57e) SHA1(5300015e25d5f2f82eda3ed54bc105d645518498) )
	ROM_LOAD32_BYTE("prg3.u0220", 0x000003, 0x80000, CRC(cbdf100d) SHA1(c9efd11103429f7f36c1652cadb5384d925cb767) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) )

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0543", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0544", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0545", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0546", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u0442", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u0443", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u0444", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm-d.u0227",  0x000000, 0x100000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(d4fc3da1) SHA1(a03bd97e36a21d27a834b9691b27a7eb7ac51ff2) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region80.u1053", 0x000000, 0x100000, CRC(e2adaff5) SHA1(9297afaf78209724515d8f78de8cee7bc7cb796b) )
ROM_END

ROM_START( rfjetu )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0u.u0211", 0x000000, 0x80000, CRC(15ac2040) SHA1(7309a9dd9c91fef0e761dcf8639f421ce7abc97f) )
	ROM_LOAD32_BYTE("prg1.u0212",  0x000001, 0x80000, CRC(395e6da7) SHA1(736f777cb1b6bf5541832b8ea89594738ca6d829) )
	ROM_LOAD32_BYTE("prg2.u0221",  0x000002, 0x80000, CRC(82f7a57e) SHA1(5300015e25d5f2f82eda3ed54bc105d645518498) )
	ROM_LOAD32_BYTE("prg3.u0220",  0x000003, 0x80000, CRC(cbdf100d) SHA1(c9efd11103429f7f36c1652cadb5384d925cb767) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) )

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0543", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0544", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0545", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0546", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u0442", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u0443", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u0444", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm-d.u0227",  0x000000, 0x100000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(d4fc3da1) SHA1(a03bd97e36a21d27a834b9691b27a7eb7ac51ff2) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region10.u1053", 0x000000, 0x100000, CRC(4319d998) SHA1(a064ce647453a9b3bccf7f1d6d0d52b5a72e09dd) )
ROM_END

ROM_START( rfjetj )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0.bin", 0x000000, 0x80000, CRC(d82fb71f) SHA1(63a458fd007c353f4fae54a4882f5c565fe1efa4) )
	ROM_LOAD32_BYTE("prg1.bin", 0x000001, 0x80000, CRC(7e21c669) SHA1(731852e5925dccc9d0d1ae4bcafa238f157f4079) )
	ROM_LOAD32_BYTE("prg2.bin", 0x000002, 0x80000, CRC(2f402d55) SHA1(d0d852239abb6f4d73e263de5544fc0893e7a7ab) )
	ROM_LOAD32_BYTE("prg3.bin", 0x000003, 0x80000, CRC(d619e2ad) SHA1(9dbff1babf62c3c5478a84d2a82a428de5949154) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) )

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0543", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0544", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0545", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0546", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u0442", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u0443", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u0444", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm-d.u0227",  0x000000, 0x100000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(d4fc3da1) SHA1(a03bd97e36a21d27a834b9691b27a7eb7ac51ff2) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region01.u1053", 0x000000, 0x100000, CRC(7ae7ab76) SHA1(a2b196f470bf64af94002fc4e2640fadad00418f) )
ROM_END

ROM_START( rfjeta )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0a.u0211", 0x000000, 0x80000, CRC(3418d4f5) SHA1(f8766d7b3708a196de417ee757787220b2a9ced1) )
	ROM_LOAD32_BYTE("prg1.u0212",  0x000001, 0x80000, CRC(395e6da7) SHA1(736f777cb1b6bf5541832b8ea89594738ca6d829) )
	ROM_LOAD32_BYTE("prg2.u0221",  0x000002, 0x80000, CRC(82f7a57e) SHA1(5300015e25d5f2f82eda3ed54bc105d645518498) )
	ROM_LOAD32_BYTE("prg3.u0220",  0x000003, 0x80000, CRC(cbdf100d) SHA1(c9efd11103429f7f36c1652cadb5384d925cb767) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) )

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0543", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0544", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0545", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0546", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u0442", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u0443", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u0444", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm-d.u0227",  0x000000, 0x100000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(d4fc3da1) SHA1(a03bd97e36a21d27a834b9691b27a7eb7ac51ff2) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region24.u1053", 0x000000, 0x100000, CRC(72a33dc4) SHA1(65a52f576ca4d240418fedd9a4922edcd6c0c8d1) )
ROM_END

ROM_START( rfjett )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE( "prg0.u0211",   0x000000, 0x080000, CRC(a4734579) SHA1(dfbd8e2a3178c7cfd7bd3698999f14bc80f5212f) )
	ROM_LOAD32_BYTE( "prg1.u0212",   0x000001, 0x080000, CRC(5e4ad3a4) SHA1(ff66e16f48978b88b298c78e21309208ccb3ff15) )
	ROM_LOAD32_BYTE( "prg2.u0221",   0x000002, 0x080000, CRC(21c9942e) SHA1(ededa05a4b5dae2dec5c4409f22e9a66d2c8e98e) )
	ROM_LOAD32_BYTE( "prg3.u0220",   0x000003, 0x080000, CRC(ea3657f4) SHA1(2291e31243af7d2e79ae727d9b5484e8d49cc7d9) )

	ROM_REGION( 0x40000, REGION_CPU2, ROMREGION_ERASE00 ) /* 256K RAM, ROM from Z80 point-of-view */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) )

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0543", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0544", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0545", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0546", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u0442", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u0443", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u0444", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION32_LE( 0xa00000, REGION_USER1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD32_WORD("pcm-d.u0227",  0x000000, 0x100000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_CONTINUE(                   0x400000, 0x100000 )
	ROM_LOAD32_BYTE("sound1.u0222", 0x800000, 0x080000, CRC(d4fc3da1) SHA1(a03bd97e36a21d27a834b9691b27a7eb7ac51ff2) )

	ROM_REGION( 0x100000, REGION_USER2, 0 ) /* on SPI motherboard */
	ROM_LOAD("flash0_blank_region20.u1053", 0x000000, 0x100000, CRC(f2051161) SHA1(45cbd5fd9ae0ca0c5c3450bca5f6806ddce3c56f) )
ROM_END

ROM_START( rdfts )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("seibu_1.u0259",        0x000000, 0x080000, CRC(e278dddd) SHA1(fe54a0d0f9e8596268f7f37e85d71c5c2d8b2846) ) // socket is silkscreened on pcb PRG0
	ROM_LOAD32_BYTE("raiden-f_prg2.u0258",  0x000001, 0x080000, CRC(58ccb10c) SHA1(0cce4057bfada78121d9586574b98d46cdd7dd46) ) // socket is silkscreened on pcb PRG1
	ROM_LOAD32_WORD("raiden-f_prg34.u0262", 0x000002, 0x100000, CRC(63f01d17) SHA1(74dbd0417b974583da87fc6c7a081b03fd4e16b8) ) // socket is silkscreened on pcb PRG23

	ROM_REGION( 0x40000, REGION_CPU2, 0 ) /* 256K ROM for the Z80 */
	ROM_LOAD("seibu_zprg.u1139", 0x000000, 0x20000, CRC(c1fda3e8) SHA1(c1d3a7ba0601a80534ec32249de71d33a828a162) ) // pads are silkscreened ZPRG

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_WORD("raiden-f_fix.u0535", 0x000000, 0x20000, CRC(2be2936b) SHA1(9e719f7328a52af220b6f084c1e0990ca6e2d533) ) // socket is silkscreened on pcb FIX01
	ROM_LOAD24_BYTE("seibu_fix2.u0528",   0x000002, 0x10000, CRC(4d87e1ea) SHA1(3230e9b643fad773e61ab8ce09c0cd7d4d0558e3) ) // socket is silkscreened on pcb FIX2

	ROM_REGION( 0x600000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("gun_dogs_bg1-d.u0526", 0x000000, 0x200000, CRC(6a68054c) SHA1(5cbfc4ac90045f1401c2dda7a51936558c9de07e) ) // pads are silkscreened on pcb BG12
	ROM_LOAD24_BYTE("gun_dogs_bg1-p.u0531", 0x000002, 0x100000, CRC(3400794a) SHA1(719808f7442bac612cefd7b7fffcd665e6337ad0) ) // pads are silkscreened on pcb BG12P
	ROM_LOAD24_WORD("gun_dogs_bg2-d.u0534", 0x300000, 0x200000, CRC(61cd2991) SHA1(bb608e3948bf9ea35b5e1615d2ba6858d029dcbe) ) // pads are silkscreened on pcb BG3
	ROM_LOAD24_BYTE("gun_dogs_bg2-p.u0530", 0x300002, 0x100000, CRC(502d5799) SHA1(c3a0e1a4f5a7b35572ae1ff31315da4ed08aa2fe) ) // pads are silkscreened on pcb BG3P

	ROM_REGION( 0xc00000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("gun_dogs_obj-1.u0322", 0x000000, 0x400000, CRC(59d86c99) SHA1(d3c9241e7b51fe21f8351051b063f91dc69bf905) ) // pads are silkscreened on pcb OBJ1
	ROM_LOAD("gun_dogs_obj-2.u0324", 0x400000, 0x400000, CRC(1ceb0b6f) SHA1(97225a9b3e7be18080aa52f6570af2cce8f25c06) ) // pads are silkscreened on pcb OBJ2
	ROM_LOAD("gun_dogs_obj-3.u0323", 0x800000, 0x400000, CRC(36e93234) SHA1(51917a80b7da5c32a9434a1076fc2916d62e6a3e) ) // pads are silkscreened on pcb OBJ3

	ROM_REGION( 0x200000, REGION_SOUND1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD("raiden-f_pcm2.u0975", 0x000000, 0x200000, CRC(3f8d4a48) SHA1(30664a2908daaeaee58f7e157516b522c952e48d) ) // pads are silkscreened SOUND0
	/* SOUND1 socket is unpopulated */
ROM_END

ROM_START( rdft2us )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("prg0.u0259", 0x000000, 0x80000, CRC(ff3eeec1) SHA1(88c1741e4936db9a5b13e562061b0f1cc6fa6b36) )
	ROM_LOAD32_BYTE("prg1.u0258", 0x000001, 0x80000, CRC(e2cf77d6) SHA1(173cc0e304c9dadea4ed0812ebb64c6c83549912) )
	ROM_LOAD32_BYTE("prg2.u0265", 0x000002, 0x80000, CRC(cae87e1f) SHA1(e460aad693eb2702ae11f758b11d37f852d00790) )
	ROM_LOAD32_BYTE("prg3.u0264", 0x000003, 0x80000, CRC(83f4fb5f) SHA1(73f58daa1aae0c4978db409cedd736fb49b15f1c) )

	ROM_REGION( 0x40000, REGION_CPU2, 0 ) /* 256K ROM for the Z80 */
	ROM_LOAD("zprg.u091", 0x000000, 0x20000, CRC(cc543c4f) SHA1(6e5c93fd3d21c594571b071d4a830211e1f162b2) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.u0524", 0x000001, 0x10000, CRC(6fdf4cf6) SHA1(7e9d4a49e829dfdc373c0f5acfbe8c7a91ac115b) )
	ROM_LOAD24_BYTE("fix1.u0518", 0x000000, 0x10000, CRC(69b7899b) SHA1(d3cacd4ef4d2c95d803403101beb9d4be75fae61) )
	ROM_LOAD24_BYTE("fixp.u0514", 0x000002, 0x10000, CRC(99a5fece) SHA1(44ae95d650ed6e00202d3438f5f91a5e52e319cb) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.u0538", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.u075",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj3b.u078", 0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.u074",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj2b.u077", 0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.u073",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj1b.u076", 0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION( 0x280000, REGION_SOUND1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD("pcm.u0103",    0x000000, 0x200000, CRC(2edc30b5) SHA1(c25d690d633657fc3687636b9070f36bd305ae06) )
	ROM_LOAD("sound1.u0107", 0x200000, 0x080000, CRC(20384b0e) SHA1(9c5d725418543df740f9145974ed6ffbbabee1d0) ) /* Different sound1 than SPI carts */
ROM_END

ROM_START( rfjets )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("rfj-06.u0259", 0x000000, 0x80000, CRC(c835aa7a) SHA1(291eada97ceb907dfea15688ce6055e63b3aa675) ) /* PRG0 */
	ROM_LOAD32_BYTE("rfj-07.u0258", 0x000001, 0x80000, CRC(3b6ca1ca) SHA1(9db019c0ddecfb58e2be5c345d78352f700035bf) ) /* PRG1 */
	ROM_LOAD32_BYTE("rfj-08.u0265", 0x000002, 0x80000, CRC(1f5dd06c) SHA1(6f5a8c9035971a470212cd0a89b94181011602c3) ) /* PRG2 */
	ROM_LOAD32_BYTE("rfj-09.u0264", 0x000003, 0x80000, CRC(cc71c402) SHA1(b040e600744e7b3f52de0fa852ce3ae08ae49985) ) /* PRG3 */

	ROM_REGION( 0x40000, REGION_CPU2, 0 ) /* 256K ROM for the Z80 */
	ROM_LOAD("rfj-05.u091", 0x000000, 0x40000, CRC(a55e8799) SHA1(5d4ca9ae920ab54e23ee3b1b33db72711e744516) ) /* ZPRG */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rfj-01.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) ) /* FIX0 */
	ROM_LOAD24_BYTE("rfj-02.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) ) /* FIX1 */
	ROM_LOAD24_BYTE("rfj-03.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) ) /* FIXP */

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0545", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u073", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u074", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u075", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION( 0x280000, REGION_SOUND1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD("pcm-d.u0103",  0x000000, 0x200000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_LOAD("rfj-04.u0107", 0x200000, 0x080000, CRC(c050da03) SHA1(1002dac51a3a4932c4f0074c1f3d97a597d98755) ) /* SOUND1 */

	ROM_REGION16_BE( 0x80, REGION_USER3, 0 )
	ROM_LOAD16_WORD( "st93c46.bin", 0x0000, 0x0080, CRC(8fe8063b) SHA1(afb0141580e1b2bd149092a9cc9e8b4072b1ef10) )
ROM_END

ROM_START( rfjetsa )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_BYTE("rfj-06.u0259", 0x000000, 0x80000, CRC(b0c8d47e) SHA1(1dde30d25f9e8eaa301343ae1d272b5c0044bc1f) ) /* PRG0 */
	ROM_LOAD32_BYTE("rfj-07.u0258", 0x000001, 0x80000, CRC(17189b39) SHA1(6471170ae770d762e15f1503ef9a6832c202da6c) ) /* PRG1 */
	ROM_LOAD32_BYTE("rfj-08.u0265", 0x000002, 0x80000, CRC(ab6d724b) SHA1(ef7e42b1bf649a354fe22b0edd00475ced4151be) ) /* PRG2 */
	ROM_LOAD32_BYTE("rfj-09.u0264", 0x000003, 0x80000, CRC(b119a67c) SHA1(4fa7dd0e86a3f7c6efa6ae9cf72991b652c877b9) ) /* PRG3 */

	ROM_REGION( 0x40000, REGION_CPU2, 0 ) /* 256K ROM for the Z80 */
	ROM_LOAD("rfj-05.u091", 0x000000, 0x40000, CRC(a55e8799) SHA1(5d4ca9ae920ab54e23ee3b1b33db72711e744516) ) /* ZPRG */

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rfj-01.u0524", 0x000001, 0x10000, CRC(8bc080be) SHA1(ad296fb98242c963072346a8de289e704b445ad4) ) /* FIX0 */
	ROM_LOAD24_BYTE("rfj-02.u0518", 0x000000, 0x10000, CRC(bded85e7) SHA1(ccb8c438ce6b9a742e3ab15be970b1e636783626) ) /* FIX1 */
	ROM_LOAD24_BYTE("rfj-03.u0514", 0x000002, 0x10000, CRC(015d0748) SHA1(b1e8eaeba63a7914f1dc27d7e3ca5d0b6db202ed) ) /* FIXP */

	ROM_REGION( 0x900000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0545", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u073", 0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u074", 0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u075", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION( 0x280000, REGION_SOUND1, ROMREGION_ERASE00 ) /* sound roms */
	ROM_LOAD("pcm-d.u0103",  0x000000, 0x200000, CRC(8ee3ff45) SHA1(2801b23495866c91c8f8bebd37d5fcae7a625838) )
	ROM_LOAD("rfj-04.u0107", 0x200000, 0x080000, CRC(c050da03) SHA1(1002dac51a3a4932c4f0074c1f3d97a597d98755) ) /* SOUND1 */
ROM_END

ROM_START( rdft22kc )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_WORD("prg0-1.267", 0x000000, 0x100000, CRC(0d7d6eb8) SHA1(3a71e1e0ba5bb500dc026debbb6189723c0c2890) )
	ROM_LOAD32_WORD("prg2-3.266", 0x000002, 0x100000, CRC(ead53e69) SHA1(b0e2e06f403317054ecb48d2747034424245f129) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("fix0.524", 0x000001, 0x10000, CRC(ed11d043) SHA1(fd3a5a33aa4d795941d64c0d23f9d6f8222843e3) )
	ROM_LOAD24_BYTE("fix1.518", 0x000000, 0x10000, CRC(7036d70a) SHA1(3535b52c0fa1a1158cacc041f8aba2b9a1b43af5) )
	ROM_LOAD24_BYTE("fix2.514", 0x000002, 0x10000, CRC(29b465da) SHA1(644454ab5e0dc1028e9512f85adfe5d8adb757de) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.535", 0x000000, 0x400000, CRC(6143f576) SHA1(c034923d0663d9ef24357a03098b8cb81dbab9f8) )
	ROM_LOAD24_BYTE("bg-1p.544", 0x000002, 0x200000, CRC(55e64ef7) SHA1(aae991268948d07342ee8ba1b3761bd180aab8ec) )
	ROM_LOAD24_WORD("bg-2d.536", 0x600000, 0x400000, CRC(c607a444) SHA1(dc1aa96a42e9394ca6036359670a4ec6f830c96d) )
	ROM_LOAD24_BYTE("bg-2p.545", 0x600002, 0x200000, CRC(f0830248) SHA1(6075df96b49e70d2243fef691e096119e7a4d044) )

	ROM_REGION( 0x1200000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj3.075",  0x0000000, 0x400000, CRC(e08f42dc) SHA1(5188d71d4355eaf43ea8893b4cfc4fe80cc24f41) )
	ROM_LOAD("obj6.078",  0x0400000, 0x200000, CRC(1b6a523c) SHA1(99a420dbc8e22e7832ccda7cec9fa661a2a2687a) )
	ROM_LOAD("obj2.074",  0x0600000, 0x400000, CRC(7aeadd8e) SHA1(47103c0579240c5b1add4d0b164eaf76f5fa97f0) )
	ROM_LOAD("obj5.077",  0x0a00000, 0x200000, CRC(5d790a5d) SHA1(1ed5d4ad4c9a7e505ce35dcc90d184c26ce891dc) )
	ROM_LOAD("obj1.073",  0x0c00000, 0x400000, CRC(c2c50f02) SHA1(b81397b5800c6d49f58b7ac7ff6eac56da3c5257) )
	ROM_LOAD("obj4.076",  0x1000000, 0x200000, CRC(5259321f) SHA1(3c70c1147e49f81371d0f60f7108d9718d56faf4) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* sound data for MSM6295 */
	ROM_LOAD("pcm0.1022", 0x000000, 0x80000, CRC(fd599b35) SHA1(00c0307d1b503bd5ce02d7960ce5e1ad600a7290) )

	ROM_REGION( 0x80000, REGION_SOUND2, 0 ) /* sound data for MSM6295 */
	ROM_LOAD("pcm1.1023", 0x000000, 0x80000, CRC(8b716356) SHA1(42ee1896c02518cd1e9cb0dc130321834665a79e) )
ROM_END

ROM_START( rfjet2kc )
	ROM_REGION32_LE( 0x200000, REGION_CPU1, 0 ) /* i386 program */
	ROM_LOAD32_WORD("prg01.u267", 0x000000, 0x100000, CRC(36019fa8) SHA1(28baf0ed4a53b818c1e6986d5d3491373524eca1) )
	ROM_LOAD32_WORD("prg23.u266", 0x000002, 0x100000, CRC(65695dde) SHA1(1b25dde03bc9319414144fc13b34c455112f4076) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* text layer roms */
	ROM_LOAD24_BYTE("rfj-01.524", 0x000001, 0x10000, CRC(e9d53007) SHA1(29aa7b70d5d5eb5e31426ac84143be44bc0597aa) )
	ROM_LOAD24_BYTE("rfj-02.518", 0x000000, 0x10000, CRC(dd3eabd3) SHA1(31c8f7a0cd262096a77673b040326605db542ab8) )
	ROM_LOAD24_BYTE("rfj-03.514", 0x000002, 0x10000, CRC(0daa8aac) SHA1(08a98fb3079ea9f78aa5b950bfeb30b0a805bab7) )

	ROM_REGION( 0xc00000, REGION_GFX2, ROMREGION_ERASEFF | ROMREGION_DISPOSE ) /* background layer roms */
	ROM_LOAD24_WORD("bg-1d.u0535", 0x000000, 0x400000, CRC(edfd96da) SHA1(4813267f104619f569e5777e75b75304321abb49) )
	ROM_LOAD24_BYTE("bg-1p.u0537", 0x000002, 0x200000, CRC(a4cc4631) SHA1(cc1c4f4de8a078ca774f5a328a9a58291949b1fb) )
	ROM_LOAD24_WORD("bg-2d.u0536", 0x600000, 0x200000, CRC(731fbb59) SHA1(13cd29ec4d4c73582c5fb363218e737886826e5f) )
	ROM_LOAD24_BYTE("bg-2p.u0547", 0x600002, 0x100000, CRC(03652c25) SHA1(c0d77285111bc84e008362981ac02a246678ed0a) )

	ROM_REGION( 0x1800000, REGION_GFX3, ROMREGION_DISPOSE ) /* sprites */
	ROM_LOAD("obj-1.u073",  0x0000000, 0x800000, CRC(58a59896) SHA1(edeaaa69987bd5d08c47ed9bf47a3901e2dcc892) )
	ROM_LOAD("obj-2.u074",  0x0800000, 0x800000, CRC(a121d1e3) SHA1(1851ae81f2ae9d3404aadd9fbc0ed7f9230290b9) )
	ROM_LOAD("obj-3.u0749", 0x1000000, 0x800000, CRC(bc2c0c63) SHA1(c8d395722f7012c3be366a0fc9b224c537afabae) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 ) /* sound data for MSM6295 */
	ROM_LOAD("rfj-05.u1022", 0x000000, 0x80000, CRC(fd599b35) SHA1(00c0307d1b503bd5ce02d7960ce5e1ad600a7290) )

	ROM_REGION( 0x80000, REGION_SOUND2, 0 ) /* sound data for MSM6295 */
	ROM_LOAD("rfj-04.u1023", 0x000000, 0x80000, CRC(1d10cd08) SHA1(c431d3f1a7b580024b083dafb76c53b771c88726) )
ROM_END


/*****************************************************************************/

GAMEX( 1995, senkyu,    0,        spi,     spi_3button, senkyu,   ROT0, "Seibu Kaihatsu", "Senkyu (Japan, newer)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, senkyua,   senkyu,   spi,     spi_3button, senkyua,  ROT0, "Seibu Kaihatsu", "Senkyu (Japan, earlier)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, batlball,  senkyu,   spi,     spi_3button, batlball, ROT0, "Seibu Kaihatsu (Tuning license)", "Battle Balls (Germany, newer)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, batlballo, senkyu,   spi,     spi_3button, batlball, ROT0, "Seibu Kaihatsu (Tuning license)", "Battle Balls (Germany, earlier)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, batlballu, senkyu,   spi,     spi_3button, batlball, ROT0, "Seibu Kaihatsu (Fabtek license)", "Battle Balls (US)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, batlballa, senkyu,   spi,     spi_3button, batlball, ROT0, "Seibu Kaihatsu (Metrotainment license)", "Battle Balls (Hong Kong)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, batlballe, senkyu,   spi,     spi_3button, batlball, ROT0, "Seibu Kaihatsu (Metrotainment license)", "Battle Balls (Hong Kong, earlier)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, batlballpt, senkyu,   spi,     spi_3button, senkyua,  ROT0, "Seibu Kaihatsu", "Battle Balls (Portugal)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1,    0,        spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu", "Viper Phase 1 (New Version, World)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1k,   viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu (Dream Island license)", "Viper Phase 1 (New Version, Korea)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1u,   viprp1,   spi,     spi_3button, viprp1o,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Viper Phase 1 (New Version, US set 1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1ua,  viprp1,   spi,     spi_3button, viprp1o,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Viper Phase 1 (New Version, US set 2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1j,   viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu", "Viper Phase 1 (New Version, Japan)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1s,   viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu", "Viper Phase 1 (New Version, Switzerland)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1h,   viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu", "Viper Phase 1 (New Version, Holland)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1t,   viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu (Tuning license)", "Viper Phase 1 (New Version, Germany)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1pt,  viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu", "Viper Phase 1 (New Version, Portugal)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1ot,  viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu (Tuning license)", "Viper Phase 1 (Germany)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1oj,  viprp1,   spi,     spi_3button, viprp1o,  ROT270, "Seibu Kaihatsu", "Viper Phase 1 (Japan)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1995, viprp1hk,  viprp1,   spi,     spi_3button, viprp1,   ROT270, "Seibu Kaihatsu (Metrotainment license)", "Viper Phase 1 (Hong Kong)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdft,      0,        spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden Fighters (Germany)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftj,     rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Japan, earlier)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftja,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Japan, earliest)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftu,     rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden Fighters (US, earlier)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftauge,  rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden Fighters (Evaluation Software For Show, Germany)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftua,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden Fighters (US, newer)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftjb,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Japan, newer)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftau,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Australia)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftam,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Metrotainment license)", "Raiden Fighters (Hong Kong)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftadi,   rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Dream Island license)", "Raiden Fighters (Korea, SUB4 cart)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdfta,     rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Austria)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftgb,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Great Britain)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftgr,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Greece)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftit,    rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu", "Raiden Fighters (Italy)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdftadia,  rdft,     spi,     spi_3button, rdft,     ROT270, "Seibu Kaihatsu (Dream Island license)", "Raiden Fighters (Korea, SUB2 cart)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2,     0,        rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden Fighters 2 - Operation Hell Dive (Germany)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2j,    rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Japan set 1)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2a,    rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu (Metrotainment license)", "Raiden Fighters 2 - Operation Hell Dive (Hong Kong)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2s,    rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Switzerland)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2ja,   rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Japan set 2)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2aa,   rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu (Dream Island license)", "Raiden Fighters 2 - Operation Hell Dive (Korea)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2it,   rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Italy)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2jb,   rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Japan set 3)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2jc,   rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Japan set 4)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2t,    rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive (Taiwan)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2u,    rdft2,    rdft2,   spi_2button, rdft2,    ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden Fighters 2 - Operation Hell Dive (US)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, rfjet,     0,        rdft2,   spi_2button, rfjet,    ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden Fighters Jet (Germany)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, rfjetu,    rfjet,    rdft2,   spi_2button, rfjet,    ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden Fighters Jet (US)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, rfjetj,    rfjet,    rdft2,   spi_2button, rfjet,    ROT270, "Seibu Kaihatsu", "Raiden Fighters Jet (Japan)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, rfjeta,    rfjet,    rdft2,   spi_2button, rfjet,    ROT270, "Seibu Kaihatsu (Dream Island license)", "Raiden Fighters Jet (Korea)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1998, rfjett,    rfjet,    rdft2,   spi_2button, rfjet,    ROT270, "Seibu Kaihatsu", "Raiden Fighters Jet (Taiwan)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1996, rdfts,     rdft,     sxx2e,   sxx2e,       rdft,     ROT270, "Seibu Kaihatsu (Explorer System Corp. license)", "Raiden Fighters (Taiwan, single board)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1997, rdft2us,   rdft2,    sxx2f,   sxx2f,       rdft2,    ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden Fighters 2 - Operation Hell Dive (US, single board)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, rfjets,    rfjet,    sxx2g,   sxx2f,       rfjet,    ROT270, "Seibu Kaihatsu", "Raiden Fighters Jet (US, single board)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1999, rfjetsa,   rfjet,    sxx2g,   sxx2f,       rfjet,    ROT270, "Seibu Kaihatsu", "Raiden Fighters Jet (US, single board, test version?)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 2000, rdft22kc,  rdft2,    sys386i, sys386i,     rdft22kc, ROT270, "Seibu Kaihatsu", "Raiden Fighters 2 - Operation Hell Dive 2000 (China, SYS386I)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 2000, rfjet2kc,  rfjet,    sys386i, sys386i,     rfjet2kc, ROT270, "Seibu Kaihatsu", "Raiden Fighters Jet 2000 (China, SYS386I)", GAME_IMPERFECT_GRAPHICS )
