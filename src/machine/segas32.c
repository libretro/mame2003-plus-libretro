/* Sega System 32 Protection related functions */

#include "driver.h"
#include "includes/segas32.h"


/******************************************************************************
 ******************************************************************************
  Memory helper prototypes
 ******************************************************************************
 ******************************************************************************/

static UINT16 MemRead16_16(offs_t address)
{
	if (!(address & 1))
		return cpu_readmem24lew_word(address);
	else
	{
		UINT16 result = cpu_readmem24lew(address);
		return result | cpu_readmem24lew(address + 1) << 8;
	}
}

static void MemWrite16_16(offs_t address, UINT16 data)
{
	if (!(address & 1))
		cpu_writemem24lew_word(address, data);
	else
	{
		cpu_writemem24lew(address, data);
		cpu_writemem24lew(address + 1, data >> 8);
	}
}


#define program_read_byte  cpu_readmem24lew
#define program_write_byte cpu_writemem24lew
#define program_read_word   MemRead16_16
#define program_write_word MemWrite16_16


/******************************************************************************
 ******************************************************************************
  Golden Axe 2 (Revenge of Death Adder)
 ******************************************************************************
 ******************************************************************************/

READ16_HANDLER(ga2_sprite_protection_r)
{
	static unsigned int prot[16] =
	{
		0x0a, 0,
		0xc5, 0,
		0x11, 0,
		0x11, 0,
		0x18, 0,
		0x18, 0,
		0x1f, 0,
		0xc6, 0,
	};

	return prot[offset];
}

READ16_HANDLER(ga2_wakeup_protection_r)
{
	static const char *prot =
		"wake up! GOLDEN AXE The Revenge of Death-Adder! ";
	return prot[offset];
}


/******************************************************************************
 ******************************************************************************
  Sonic Arcade protection
 ******************************************************************************
 ******************************************************************************/

// This code duplicates the actions of the protection device used in SegaSonic
// arcade revision C, allowing the game to run correctly.
#define CLEARED_LEVELS			0xE5C4
#define CURRENT_LEVEL			0xF06E
#define CURRENT_LEVEL_STATUS		0xF0BC
#define LEVEL_ORDER_ARRAY		0x263A

WRITE16_HANDLER(sonic_level_load_protection)
{
	UINT16 level;
/*Perform write*/
	COMBINE_DATA(&system32_workram[CLEARED_LEVELS / 2]);

/*Refresh current level*/
		if (system32_workram[CLEARED_LEVELS / 2] == 0)
		{
			level = 0x0007;
		}
		else
		{
			const UINT8 *ROM = memory_region(REGION_CPU1);
			level =  *((ROM + LEVEL_ORDER_ARRAY) + (system32_workram[CLEARED_LEVELS / 2] * 2) - 1);
			level |= *((ROM + LEVEL_ORDER_ARRAY) + (system32_workram[CLEARED_LEVELS / 2] * 2) - 2) << 8;
		}
		system32_workram[CURRENT_LEVEL / 2] = level;

/*Reset level status*/
		system32_workram[CURRENT_LEVEL_STATUS / 2] = 0x0000;
		system32_workram[(CURRENT_LEVEL_STATUS + 2) / 2] = 0x0000;
}


/******************************************************************************
 ******************************************************************************
  The J.League 1994 (Japan)
 ******************************************************************************
 ******************************************************************************/

WRITE16_HANDLER( jleague_protection_w )
{
	COMBINE_DATA( &system32_workram[0xf700/2 + offset ] );

	switch( offset )
	{
		/* Map team browser selection to opponent browser selection*/
		/* using same lookup table that V60 uses for sound sample mapping.*/
		case 0:
			cpu_writemem24lew( 0x20f708, cpu_readmem24lew_word( 0x7bbc0 + data*2 ) );
			break;

		/* move on to team browser*/
		case 4/2:
			cpu_writemem24lew( 0x200016, data & 0xff );
			break;

		default:
			break;
	}
}


/******************************************************************************
 ******************************************************************************
  Burning Rival
 ******************************************************************************
 ******************************************************************************/

READ16_HANDLER(brival_protection_r)
{
	if (!mem_mask) /* only trap on word-wide reads*/
	{
		switch (offset)
		{
			case 0:
			case 2:
			case 3:
				return 0;
		}
	}

	return system32_workram[0xba00/2 + offset];
}

WRITE16_HANDLER(brival_protboard_w)
{
	static const int protAddress[6][2] =
	{
		{ 0x109517, 0x00/2 },
		{ 0x109597, 0x10/2 },
		{ 0x109597, 0x20/2 },
		{ 0x109597, 0x30/2 },
		{ 0x109597, 0x40/2 },
		{ 0x109617, 0x50/2 },
	};
	char ret[32];
	int curProtType;
	UINT8 *ROM = memory_region(REGION_CPU1);

	switch (offset)
	{
		case 0x800/2:
			curProtType = 0;
			break;
		case 0x802/2:
			curProtType = 1;
			break;
		case 0x804/2:
			curProtType = 2;
			break;
		case 0x806/2:
			curProtType = 3;
			break;
		case 0x808/2:
			curProtType = 4;
			break;
		case 0x80a/2:
			curProtType = 5;
			break;
		default:
			if (offset >= 0xa00/2 && offset < 0xc00/2)
				return;
			log_cb(RETRO_LOG_DEBUG, LOGPRE "brival_protboard_w: UNKNOWN WRITE: offset %x value %x\n", offset, data);
			return;
	}

	memcpy(ret, &ROM[protAddress[curProtType][0]], 16);
	ret[16] = '\0';

	memcpy(&segas32_protram[protAddress[curProtType][1]], ret, 16);
}


/******************************************************************************
 ******************************************************************************
  Arabian Fight
 ******************************************************************************
 ******************************************************************************/

/* protection ram is 8-bits wide and only occupies every other address*/
READ16_HANDLER(arabfgt_protboard_r)
{
	int PC = activecpu_get_pc();
	int cmpVal;

	if (PC == 0xfe0325 || PC == 0xfe01e5 || PC == 0xfe035e || PC == 0xfe03cc)
	{
		cmpVal = activecpu_get_reg(1);

		/* R0 always contains the value the protection is supposed to return (!)*/
		return cmpVal;
	}
	else
	{
		usrintf_showmessage("UNKONWN ARF PROTECTION READ PC=%x\n", PC);
	}

	return 0;
}

WRITE16_HANDLER(arabfgt_protboard_w)
{
}

READ16_HANDLER(arf_wakeup_protection_r)
{
	static const char *prot =
		"wake up! ARF!                                   ";
	return prot[offset];
}


/******************************************************************************
 ******************************************************************************
  F1 Super Lap
 ******************************************************************************
 ******************************************************************************/

void f1lap_fd1149_vblank(void)
{
	data8_t val;

	cpu_writemem24lew(0x20F7C6, 0);

	/* needed to start a game */
	val = cpu_readmem24lew(0x20EE81);
	if (val == 0xff) cpu_writemem24lew(0x20EE81,0);
}


/******************************************************************************
 ******************************************************************************
  Air Rescue
 ******************************************************************************
 ******************************************************************************/

static UINT16 arescue_dsp_io[6] = {0,0,0,0,0,0};
READ16_HANDLER( arescue_dsp_r )
{
	if( offset == 4/2 )
	{
		switch( arescue_dsp_io[0] )
		{
			case 0:
			case 1:
			case 2:
				break;

			case 3:
				arescue_dsp_io[0] = 0x8000;
				arescue_dsp_io[2/2] = 0x0001;
				break;

			case 6:
				arescue_dsp_io[0] = 4 * arescue_dsp_io[2/2];
				break;

			default:
				log_cb(RETRO_LOG_DEBUG, LOGPRE "Unhandled DSP cmd %04x (%04x).\n", arescue_dsp_io[0], arescue_dsp_io[1] );
				break;
		}
	}

	return arescue_dsp_io[offset];
}

WRITE16_HANDLER( arescue_dsp_w )
{
	COMBINE_DATA(&arescue_dsp_io[offset]);
}


/******************************************************************************
 ******************************************************************************
  Dark Edge
 ******************************************************************************
 ******************************************************************************/

void darkedge_fd1149_vblank(void)
{
	 program_write_word(0x20f072, 0);
	 program_write_word(0x20f082, 0);

	if(  program_read_byte(0x20a12c) != 0 )
	{
		 program_write_byte(0x20a12c, program_read_byte(0x20a12c)-1 );

		if( program_read_byte(0x20a12c) == 0 )
			program_write_byte(0x20a12e, 1);
	}
}

WRITE16_HANDLER( darkedge_protection_w )
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "%06x:darkedge_prot_w(%06X) = %04X & %04X\n",
		activecpu_get_pc(), 0xa00000 + 2*offset, data, mem_mask ^ 0xffff);
}

READ16_HANDLER( darkedge_protection_r )
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "%06x:darkedge_prot_r(%06X) & %04X\n",
		activecpu_get_pc(), 0xa00000 + 2*offset, mem_mask ^ 0xffff);
	return 0xffff;
}


/******************************************************************************
 ******************************************************************************
  DBZ VRVS
 ******************************************************************************
 ******************************************************************************/

WRITE16_HANDLER( dbzvrvs_protection_w )
{
	program_write_word( 0x2080c8, program_read_word( 0x200044 ) );
}

READ16_HANDLER( dbzvrvs_protection_r )
{
	return 0xffff;
}
