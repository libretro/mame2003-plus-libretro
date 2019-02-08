/*************************************************************************

  Volfied C-Chip Protection
  =========================

*************************************************************************/

#include "driver.h"
#include "state.h"

static int current_bank = 0;
static int current_data = 0;
static int current_flag = 0;

static UINT8 cc_port = 0;


/*************************************
 *
 * Writes to C-Chip - Important Bits
 *
 *************************************/

WRITE16_HANDLER( volfied_cchip_w )
{
	if (offset == 0x600)
	{
		current_bank = data;
	}

	if (current_bank == 0)
	{
		if (offset == 0x008)
		{
			cc_port = data;

			coin_lockout_w(1, data & 0x80);
			coin_lockout_w(0, data & 0x40);
			coin_counter_w(1, data & 0x20);
			coin_counter_w(0, data & 0x10);
		}

		if (offset == 0x3fe)
		{
			/*******************

			round 01 => data $0A
			round 02 => data $01
			round 03 => data $03
			round 04 => data $08
			round 05 => data $05
			round 06 => data $04
			round 07 => data $0B
			round 08 => data $09
			round 09 => data $07
			round 10 => data $06
			round 11 => data $0E
			round 12 => data $0D
			round 13 => data $02
			round 14 => data $0C
			round 15 => data $0F
			round 16 => data $10
			final    => data $11

			********************/

			current_data = data;
		}

		if (offset == 0x3ff)
		{
			current_flag = data;
		}
	}
}


/*************************************
 *
 * Reads from C-Chip
 *
 *************************************/

READ16_HANDLER( volfied_cchip_r )
{
	/* C-chip identification */

	if (offset == 0x401)
	{
		return 0x01;
	}

	/* Check for input ports */

	if (current_bank == 0)
	{
		switch (offset)
		{
		case 0x003: return input_port_2_word_r(offset, mem_mask);
		case 0x004: return input_port_3_word_r(offset, mem_mask);
		case 0x005: return input_port_4_word_r(offset, mem_mask);
		case 0x006: return input_port_5_word_r(offset, mem_mask);
		case 0x008: return cc_port;
		}
	}

	/* Further non-standard offsets */

	if (current_bank == 2 && offset == 0x005)
	{
		return 0x7c;                /* makes worm in round 1 appear */
	}
	if (current_bank == 0 && offset == 0x3fe)
	{
		return 0x00;                /* signals color data ready */
	}
	if (current_bank == 0 && offset == 0x3ff)
	{
		return 2 * current_flag;    /* fixes freeze after shield runs out */
	}
	if (current_bank == 0 && offset == 0x023 && current_data > 0x80)
	{
		return 0x00;                /* mystery color index */
	}
	if (current_bank == 0 && offset >= 0x010 && offset <= 0x0af && current_data < 0x80)
	{
		return ((offset + current_data) * 37) & 0xff;	/* enemy colors */
	}

	return 0x00;
}


/*************************************
 *
 * C-Chip State Saving
 *
 *************************************/

void volfied_cchip_init(void)
{
	state_save_register_int  ("volfied", 0, "cc_bank", &current_bank);
	state_save_register_int  ("volfied", 0, "cc_data", &current_data);
	state_save_register_int  ("volfied", 0, "cc_flag", &current_flag);
	state_save_register_UINT8("volfied", 0, "cc_port", &cc_port, 1);
}
