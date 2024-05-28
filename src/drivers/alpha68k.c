/***************************************************************************

	SNK/Alpha 68000 based games:

	(Game)                  (PCB Number)     (Manufacturer)

	Super Stingray          ? (Early)        Alpha 1986?
	Kyros                   ? (Early)        World Games Inc 1987
	Paddle Mania            Alpha 68K-96 I   SNK 1988
	Time Soldiers (Ver 3)   Alpha 68K-96 II  SNK/Romstar 1987
	Time Soldiers (Ver 1)   Alpha 68K-96 II  SNK/Romstar 1987
	Battlefield (Ver 1)     Alpha 68K-96 II  SNK 1987
	Sky Soldiers            Alpha 68K-96 II  SNK/Romstar 1988
	Gold Medalist           Alpha 68K-96 II  SNK 1988
	Gold Medalist           (Bootleg)        SNK 1988
	Sky Adventure           Alpha 68K-96 V   SNK 1989
	Gang Wars               Alpha 68K-96 V   Alpha 1989
	Gang Wars               (Bootleg)        Alpha 1989
	Super Champion Baseball (V board?)       SNK/Alpha/Romstar/Sega 1989
	The Next Space          A8004-1 PIC      SNK 1989

General notes:

	All II & V games are 68000, z80 plus a custom Alpha microcontroller,
	the microcontroller is able to write to anywhere within main memory.

	Gold Medalist (bootleg) has a 68705 in place of the Alpha controller.

	V boards have more memory and double the amount of colours as II boards.

	Time Soldiers - make the ROM writable and the game will enter a 'debug'
	kind of mode, probably from the development system used.

	Time Soldiers - Title screen is corrupt when set to 'Japanese language',
	the real board does this too!  (Battlefield is corrupt when set to English
	too).

	The Next Space is not an Alpha game, but the video hardware is identical
	to Paddlemania.

	Emulation by Bryan McPhail, mish@tendril.co.uk


Stephh's additional notes (based on the games M68000 code and some tests) :

 1)  'sstingry'

  - You can test the ports (inputs, Dip Switches and sound) by pressing
    player 1 buttons 1 and 2 during the P.O.S.T.

 2)  'kyros'

  - You can enter sort of "test mode" by pressing player 1 buttons 1 and 2
    when you reset the game.

 3)  'paddlema'

  - "Game Time" Dip Switch is the time for match type A. Here is what you
    have to add for games B to E :

      Match Type       B        C        D        E
      Time to add    00:10    01:00    01:30    02:00

  - When "Game Mode" Dip Switch is set to "Win Match Against CPU", this has
    an effect on matches types A and B : player is awarded 99 points at the
    round, which is enough to win all matches then see the ending credits.

  - "Button A" and "Button B" do the same thing : they BOTH start a game
    and select difficulty/court. I've mapped them this way because you
    absolutely need the 2 buttons when you are in the "test mode".

 4)  'timesold', 'timesol1' and 'btlfield'

  - The "Unused" Dip Switch is sort of "Debug Mode" Dip Switch and has an
    effect ONLY if 0x008fff.b is writable (as Bryan mentioned it).
    Its role seems only to be limited to display some coordonates.

 5)  'skysoldr'

  - As in "Time Soldiers / Battle Field" there is a something that is sort
    of "Debug Mode" Dip Switch : this is the "Manufacturer" Dip Switch when
    it is set to "Romstar". Again, it has an effect only if 0x000074.w is
    writable and its role seems only to be limited to display some coordonates.

 7)  'skyadvnt', 'skyadvnu' and 'skyadvnj'

  - As in 'skysoldr', you can access to some "hidden features" if 0x000074.w
    is writable :

      * bit 4 (when "Unused" Dip Switch is set to "On") determines invulnerability
      * bit 6 (when "Difficulty" Dip Switch is set to "Hard" or "Hardest")
        determines if some coordonates are displayed.

 8)  'gangwarb'

  - When "Coin Slots" Dip Switch is set to "Common", COIN2 only adds ONE credit
    and this has nothing to do with the microcontroller stuff.
  - There is no Dip Switch to determine if you are allowed to continue a game
    or not, so you ALWAYS have the possibility to continue a game.

 9)  'sbasebal'

  - IMO, there must exist a Japan version of this game which is currently
    undumped ! Set the SBASEBAL_HACK to 1 and you'll notice the following
    differences :

      * different manufacturer (no more SNK licence)
      * different coinage (check code at 0x00035c) and additional COIN2
      * different game time (check code at 0x001d20)
      * different table for "Unknown" Dip Switch


Stephh's log (2002.06.19) :

  - Create macros for players inputs and "Coinage" Dip Switch
  - Full check and fix of Dip Switches and Inputs in ALL games (PHEW ! :p)
    Read MY additional notes for unknown issues though ...
  - Improve coin handler for all games (thanks Acho A. Tang for the sample
    for 'sstingry', even if I had to rewrite it !)
  - Fix screen flipping in 'sbasebal' (this was a Dip Switch issue)
  - Add READ16_HANDLER( *_cycle_r ) for the following games :
      * timesol1  (based on the one from 'timesold')
      * btlfield  (based on the one from 'timesold')
      * gangwarb  (I splitted the one from 'gangwars')
      * skyadvnt, skyadvnu and skyadvnj
  - Change manufacturer for the following games :
      * timesold
      * timesol1
      * btlfield
      * skysoldr
    I can send you the ending pics where "Alpha Denshi Co." is mentioned.
  - microcontroller_id is no more static to be used in vidhrdw/alpha68k.c
    (in which I've removed the no more needed 'game_id' and 'strcmp')


Revision History:

Acho A. Tang, xx-xx-2002

[Super Stingray]
- improved color, added sound, game timer and coin handler, corrected DIP settings
note: CLUT and color remap PROMs missing

[Kyros]
- fixed color, added sound and coin handler, corrected DIP settings

[Paddle Mania]
- reactivated driver
- improved color, added sound, fixed control and priority, corrected DIP settings

[Time Soldiers]
- fixed priority

[Gold Medalist]
- fixed gameplay, control and priority, corrected DIP settings
- fixed garbled voices using sound ROMs from the bootleg set

[Sky Adventure]
- fixed sprite position and priority

[Gang Wars]
- fixed color in the 2nd last graphics bank

[The Next Space]
- fixed color and sprite glitches, added sound, filled DIP settings

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

#define SBASEBAL_HACK	0

VIDEO_START( alpha68k );
VIDEO_UPDATE( kyros );
VIDEO_UPDATE( sstingry );
VIDEO_UPDATE( alpha68k_I );
VIDEO_UPDATE( tnexspce );
PALETTE_INIT( kyros );
PALETTE_INIT( paddlem );
VIDEO_UPDATE( alpha68k_II );
WRITE16_HANDLER( alpha68k_II_video_bank_w );
VIDEO_UPDATE( alpha68k_V );
VIDEO_UPDATE( alpha68k_V_sb );
void alpha68k_V_video_bank_w(int bank);
void alpha68k_flipscreen_w(int flip);
void (*alpha68k_video_banking)(int *bank, int data);
void kyros_video_banking(int *bank, int data);
WRITE16_HANDLER( alpha68k_V_video_control_w );
WRITE16_HANDLER( alpha68k_paletteram_w );
WRITE16_HANDLER( alpha68k_videoram_w );

static data16_t *shared_ram;
/*static unsigned char *sound_ram; */ /*AT: not needed*/
static int invert_controls;
int microcontroller_id, coin_id;

static unsigned trigstate=0, deposits1=0, deposits2=0, credits=0;

/******************************************************************************/

/* resets the values related to the microcontroller */
MACHINE_INIT( common )
{
	trigstate = 0;
	deposits1 = 0;
	deposits2 = 0;
	credits   = 0;
}

MACHINE_INIT( tnexspce )
{
	alpha68k_flipscreen_w(0);
}

/******************************************************************************/

WRITE16_HANDLER( tnexspce_unknown_w )
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "tnexspce_unknown_w : PC = %04x - offset = %04x - data = %04x\n",activecpu_get_pc(),offset,data);
	if (offset==0x0000 && ACCESSING_LSB)
	{
		alpha68k_flipscreen_w(data & 1);
	}
}

static WRITE16_HANDLER( alpha_microcontroller_w )
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x:  Alpha write trigger at %04x (%04x)\n",activecpu_get_pc(),offset,data);
	/* 0x44 = coin clear signal to microcontroller? */
	if (offset==0x2d && ACCESSING_LSB)
		alpha68k_flipscreen_w(data & 1);
}

/******************************************************************************/

static READ16_HANDLER( kyros_dip_r )
{
	return readinputport(1)<<8;
}

static READ16_HANDLER( control_1_r )
{
	if (invert_controls)
		return ~(readinputport(0) + (readinputport(1) << 8));

	return (readinputport(0) + (readinputport(1) << 8));
}

static READ16_HANDLER( control_2_r )
{
	if (invert_controls)
		return ~(readinputport(3) + ((~(1 << (readinputport(5) * 12 / 256))) << 8));

	return readinputport(3) + /* Low byte of CN1 */
		((~(1 << (readinputport(5) * 12 / 256))) << 8);
}

static READ16_HANDLER( control_2_V_r )
{
	return readinputport(3);
}

static READ16_HANDLER( control_3_r )
{
	if (invert_controls)
		return ~((( ~(1 << (readinputport(6) * 12 / 256)) )<<8)&0xff00);

	return (( ~(1 << (readinputport(6) * 12 / 256)) )<<8)&0xff00;
}

/* High 4 bits of CN1 & CN2 */
static READ16_HANDLER( control_4_r )
{
	if (invert_controls)
		return ~(((( ~(1 << (readinputport(6) * 12 / 256))  ) <<4)&0xf000)
		 + ((( ~(1 << (readinputport(5) * 12 / 256))  )    )&0x0f00));

	return ((( ~(1 << (readinputport(6) * 12 / 256))  ) <<4)&0xf000)
		 + ((( ~(1 << (readinputport(5) * 12 / 256))  )    )&0x0f00);
}

/******************************************************************************/

static WRITE16_HANDLER( kyros_sound_w )
{
	if(ACCESSING_MSB)
		soundlatch_w(0, (data>>8)&0xff);
}

static WRITE16_HANDLER( alpha68k_II_sound_w )
{
	if(ACCESSING_LSB)
		soundlatch_w(0, data&0xff);
}

static WRITE16_HANDLER( alpha68k_V_sound_w )
{
	/* Sound & fix bank select are in the same word */
	if(ACCESSING_LSB)
		soundlatch_w(0,data&0xff);
	if(ACCESSING_MSB)
		alpha68k_V_video_bank_w((data>>8)&0xff);
}
/*AT*/
static WRITE16_HANDLER( paddlema_soundlatch_w )
{
	if (ACCESSING_LSB)
	{
		soundlatch_w(0, data);
		cpu_set_irq_line(1, 0, HOLD_LINE);
	}
}

static WRITE16_HANDLER( tnexspce_soundlatch_w )
{
	if (ACCESSING_LSB)
	{
		soundlatch_w(0, data);
		cpu_set_nmi_line(1, PULSE_LINE);
	}
}
/*ZT*/
/******************************************************************************/

/* Kyros, Super Stingray */
static READ16_HANDLER( kyros_alpha_trigger_r )
{
	/* possible jump codes:
           - Kyros          : 0x22
	     - Super Stingray : 0x21,0x22,0x23,0x24,0x34,0x37,0x3a,0x3d,0x40,0x43,0x46,0x49
	*/
	static unsigned coinvalue=0, microcontroller_data=0;
	static UINT8 coinage1[8][2]={{1,1},{1,5},{1,3},{2,3},{1,2},{1,6},{1,4},{3,2}};
	static UINT8 coinage2[8][2]={{1,1},{5,1},{3,1},{7,1},{2,1},{6,1},{4,1},{8,1}};

	static int latch;
	int source=shared_ram[offset];

	switch (offset) {
		case 0x22: /* Coin value */
			shared_ram[0x22] = (source&0xff00)|(credits&0x00ff);	/* (source&0xff00)|0x1 */
			return 0;
		case 0x29: /* Query microcontroller for coin insert */
			trigstate++;
			if ((readinputport(2)&0x3)==3) latch=0;
			if ((readinputport(2)&0x1)==0 && !latch)
			{
				shared_ram[0x29] = (source&0xff00)|(coin_id&0xff);	/* coinA*/
				shared_ram[0x22] = (source&0xff00)|0x0;
				latch=1;

				coinvalue = (~readinputport(1)>>1) & 7;
				deposits1++;
				if (deposits1 == coinage1[coinvalue][0])
				{
					credits = coinage1[coinvalue][1];
					deposits1 = 0;
				}
				else
					credits = 0;
			}
			else if ((readinputport(2)&0x2)==0 && !latch)
			{
				shared_ram[0x29] = (source&0xff00)|(coin_id>>8);	/* coinB*/
				shared_ram[0x22] = (source&0xff00)|0x0;
				latch=1;

				coinvalue = (~readinputport(1)>>1) & 7;
				deposits2++;
				if (deposits2 == coinage2[coinvalue][0])
				{
					credits = coinage2[coinvalue][1];
					deposits2 = 0;
				}
				else
					credits = 0;
			}
			else
			{
				if (microcontroller_id == 0x00ff)		/* Super Stingry */
				{
					if (trigstate >= 12)	/* arbitrary value ! */
					{
						trigstate = 0;
						microcontroller_data = 0x21;			/* timer*/
					}
					else
						microcontroller_data = 0x00;
				}
				else
					microcontroller_data = 0x00;
				shared_ram[0x29] = (source&0xff00)|microcontroller_data;
			}

			return 0;
		case 0xff:  /* Custom check, only used at bootup */
			shared_ram[0xff] = (source&0xff00)|microcontroller_id;
			break;
	}

	log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x:  Alpha read trigger at %04x\n",activecpu_get_pc(),offset);

	return 0; /* Values returned don't matter */
}

/* Time Soldiers, Sky Soldiers, Gold Medalist */
static READ16_HANDLER( alpha_II_trigger_r )
{
	/* possible jump codes:
	     - Time Soldiers : 0x21,0x22,0x23,0x24,0x34,0x37,0x3a,0x3d,0x40,0x43,0x46,0x49
	     - Sky Soldiers  : 0x21,0x22,0x23,0x24,0x34,0x37,0x3a,0x3d,0x40,0x43,0x46,0x49
	     - Gold Medalist : 0x21,0x23,0x24,0x5b
	*/
	static unsigned coinvalue=0, microcontroller_data=0;
	static UINT8 coinage1[8][2]={{1,1},{1,2},{1,3},{1,4},{1,5},{1,6},{2,3},{3,2}};
	static UINT8 coinage2[8][2]={{1,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{8,1}};

	static int latch;
	int source=shared_ram[offset];

	switch (offset)
	{
		case 0: /* Dipswitch 2 */
			shared_ram[0] = (source&0xff00)|readinputport(4);
			return 0;

		case 0x22: /* Coin value */
			shared_ram[0x22] = (source&0xff00)|(credits&0x00ff);	/* (source&0xff00)|0x1 */
			return 0;

		case 0x29: /* Query microcontroller for coin insert */
			if ((readinputport(2)&0x3)==3) latch=0;
			if ((readinputport(2)&0x1)==0 && !latch)
			{
				shared_ram[0x29] = (source&0xff00)|(coin_id&0xff);	/* coinA*/
				shared_ram[0x22] = (source&0xff00)|0x0;
				latch=1;

				if ((coin_id&0xff) == 0x22)
				{
					coinvalue = (~readinputport(4)>>0) & 7;
					deposits1++;
					if (deposits1 == coinage1[coinvalue][0])
					{
						credits = coinage1[coinvalue][1];
						deposits1 = 0;
					}
					else
						credits = 0;
				}
			}
			else if ((readinputport(2)&0x2)==0 && !latch)
			{
				shared_ram[0x29] = (source&0xff00)|(coin_id>>8);	/* coinB*/
				shared_ram[0x22] = (source&0xff00)|0x0;
				latch=1;

				if ((coin_id>>8) == 0x22)
				{
					coinvalue = (~readinputport(4)>>0) & 7;
					deposits2++;
					if (deposits2 == coinage2[coinvalue][0])
					{
						credits = coinage2[coinvalue][1];
						deposits2 = 0;
					}
					else
						credits = 0;
				}
			}
			else
			{
				if (microcontroller_id == 0x8803)		/* Gold Medalist */
					microcontroller_data = 0x21;				/* timer*/
				else
					microcontroller_data = 0x00;
				shared_ram[0x29] = (source&0xff00)|microcontroller_data;
			}

			return 0;
		case 0xfe:  /* Custom ID check, same for all games */
			shared_ram[0xfe] = (source&0xff00)|0x87;
			break;
		case 0xff:  /* Custom ID check, same for all games */
			shared_ram[0xff] = (source&0xff00)|0x13;
			break;
	}

	log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x:  Alpha read trigger at %04x\n",activecpu_get_pc(),offset);

	return 0; /* Values returned don't matter */
}

/* Sky Adventure, Gang Wars, Super Champion Baseball */
static READ16_HANDLER( alpha_V_trigger_r )
{
	/* possible jump codes:
	     - Sky Adventure           : 0x21,0x22,0x23,0x24,0x34,0x37,0x3a,0x3d,0x40,0x43,0x46,0x49
	     - Gang Wars               : 0x21,0x23,0x24,0x54
	     - Super Champion Baseball : 0x21,0x23,0x24
	*/
	static unsigned coinvalue=0, microcontroller_data=0;
	static UINT8 coinage1[8][2]={{1,1},{1,5},{1,3},{2,3},{1,2},{1,6},{1,4},{3,2}};
	static UINT8 coinage2[8][2]={{1,1},{5,1},{3,1},{7,1},{2,1},{6,1},{4,1},{8,1}};

	static int latch;
	int source=shared_ram[offset];

	switch (offset)
	{
		case 0: /* Dipswitch 1 */
			shared_ram[0] = (source&0xff00)|readinputport(4);
			return 0;
		case 0x22: /* Coin value */
			shared_ram[0x22] = (source&0xff00)|(credits&0x00ff);	/* (source&0xff00)|0x1 */
			return 0;
		case 0x29: /* Query microcontroller for coin insert */
			if ((readinputport(2)&0x3)==3) latch=0;
			if ((readinputport(2)&0x1)==0 && !latch)
			{
				shared_ram[0x29] = (source&0xff00)|(coin_id&0xff);	/* coinA*/
				shared_ram[0x22] = (source&0xff00)|0x0;
				latch=1;

				if ((coin_id&0xff) == 0x22)
				{
					coinvalue = (~readinputport(4)>>1) & 7;
					deposits1++;
					if (deposits1 == coinage1[coinvalue][0])
					{
						credits = coinage1[coinvalue][1];
						deposits1 = 0;
					}
					else
						credits = 0;
				}
			}
			else if ((readinputport(2)&0x2)==0 && !latch)
			{
				shared_ram[0x29] = (source&0xff00)|(coin_id>>8);	/* coinB*/
				shared_ram[0x22] = (source&0xff00)|0x0;
				latch=1;

				if ((coin_id>>8) == 0x22)
				{
					coinvalue = (~readinputport(4)>>1) & 7;
					deposits2++;
					if (deposits2 == coinage2[coinvalue][0])
					{
						credits = coinage2[coinvalue][1];
						deposits2 = 0;
					}
					else
						credits = 0;
				}
			}
			else
			{
				microcontroller_data = 0x00;
				shared_ram[0x29] = (source&0xff00)|microcontroller_data;
			}

			return 0;
		case 0xfe:  /* Custom ID check */
			shared_ram[0xfe] = (source&0xff00)|(microcontroller_id>>8);
			break;
		case 0xff:  /* Custom ID check */
			shared_ram[0xff] = (source&0xff00)|(microcontroller_id&0xff);
			break;

		case 0x1f00: /* Dipswitch 1 */
			shared_ram[0x1f00] = (source&0xff00)|readinputport(4);
			return 0;
		case 0x1f29: /* Query microcontroller for coin insert */
			if ((readinputport(2)&0x3)==3) latch=0;
			if ((readinputport(2)&0x1)==0 && !latch)
			{
				shared_ram[0x1f29] = (source&0xff00)|(coin_id&0xff);	/* coinA*/
				shared_ram[0x1f22] = (source&0xff00)|0x0;
				latch=1;

				if ((coin_id&0xff) == 0x22)
				{
					coinvalue = (~readinputport(4)>>1) & 7;
					deposits1++;
					if (deposits1 == coinage1[coinvalue][0])
					{
						credits = coinage1[coinvalue][1];
						deposits1 = 0;
					}
					else
						credits = 0;
				}
			}
			else if ((readinputport(2)&0x2)==0 && !latch)
			{
				shared_ram[0x1f29] = (source&0xff00)|(coin_id>>8);	/* coinB*/
				shared_ram[0x1f22] = (source&0xff00)|0x0;
				latch=1;

				if ((coin_id>>8) == 0x22)
				{
					coinvalue = (~readinputport(4)>>1) & 7;
					deposits2++;
					if (deposits2 == coinage2[coinvalue][0])
					{
						credits = coinage2[coinvalue][1];
						deposits2 = 0;
					}
					else
						credits = 0;
				}
			}
			else
			{
				microcontroller_data = 0x00;
				shared_ram[0x1f29] = (source&0xff00)|microcontroller_data;
			}

			/* Gang Wars expects the first dip to appear in RAM at 0x02c6,
			   the microcontroller supplies it (it does for all the other games,
			   but usually to 0x0 in RAM) when 0x21 is read (code at 0x009332) */
			source=shared_ram[0x0163];
			shared_ram[0x0163] = (source&0x00ff)|(readinputport(4)<<8);

			return 0;
		case 0x1ffe:  /* Custom ID check */
			shared_ram[0x1ffe] = (source&0xff00)|(microcontroller_id>>8);
			break;
		case 0x1fff:  /* Custom ID check */
			shared_ram[0x1fff] = (source&0xff00)|(microcontroller_id&0xff);
			break;
	}

	log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x:  Alpha read trigger at %04x\n",activecpu_get_pc(),offset);

	return 0; /* Values returned don't matter */
}

/******************************************************************************/

static MEMORY_READ16_START( kyros_readmem )
	{ 0x000000, 0x01ffff, MRA16_ROM }, /* main program*/
	{ 0x020000, 0x020fff, MRA16_RAM }, /* work RAM*/
	{ 0x040000, 0x041fff, MRA16_RAM }, /* sprite RAM*/
	{ 0x060000, 0x060001, MRA16_RAM }, /* MSB: watchdog*/
	{ 0x080000, 0x0801ff, kyros_alpha_trigger_r },
	{ 0x0c0000, 0x0c0001, input_port_0_word_r },
	{ 0x0e0000, 0x0e0001, kyros_dip_r },
MEMORY_END

static MEMORY_WRITE16_START( kyros_writemem )
	{ 0x000000, 0x01ffff, MWA16_ROM },
	{ 0x020000, 0x020fff, MWA16_RAM, &shared_ram },
	{ 0x040000, 0x041fff, MWA16_RAM, &spriteram16 },
	{ 0x060000, 0x060001, MWA16_RAM, &videoram16 }, /* LSB: BGC*/
	{ 0x080000, 0x0801ff, MWA16_NOP },
	{ 0x080000, 0x0801ff, alpha_microcontroller_w },
	{ 0x0e0000, 0x0e0001, kyros_sound_w },
MEMORY_END

static MEMORY_READ16_START( alpha68k_I_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM }, /* main program*/
	{ 0x080000, 0x083fff, MRA16_RAM }, /* work RAM*/
	{ 0x100000, 0x103fff, MRA16_RAM }, /* video RAM*/
	{ 0x180000, 0x180001, input_port_3_word_r }, /* LSB: DSW0*/
	{ 0x180008, 0x180009, input_port_4_word_r }, /* LSB: DSW1*/
	{ 0x300000, 0x300001, input_port_0_word_r }, /* joy1, joy2*/
	{ 0x340000, 0x340001, input_port_1_word_r }, /* coin, start, service*/
	{ 0x380000, 0x380001, input_port_2_word_r }, /* joy3, joy4*/
MEMORY_END

static MEMORY_WRITE16_START( alpha68k_I_writemem )
	{ 0x000000, 0x03ffff, MWA16_NOP },
	{ 0x080000, 0x083fff, MWA16_RAM },
	{ 0x100000, 0x103fff, MWA16_RAM, &spriteram16 },
	{ 0x180000, 0x180001, MWA16_NOP }, /* MSB: watchdog(?)*/
	{ 0x380000, 0x380001, paddlema_soundlatch_w }, /* LSB: sound latch write and RST38 trigger*/
MEMORY_END

static MEMORY_READ16_START( alpha68k_II_readmem )
	{ 0x000000, 0x03ffff, MRA16_RAM },
	{ 0x040000, 0x040fff, MRA16_RAM },
	{ 0x080000, 0x080001, control_1_r }, /* Joysticks */
	{ 0x0c0000, 0x0c0001, control_2_r }, /* CN1 & Dip 1 */
	{ 0x0c8000, 0x0c8001, control_3_r }, /* Bottom of CN2 */
	{ 0x0d0000, 0x0d0001, control_4_r }, /* Top of CN1 & CN2 */
	{ 0x0d8000, 0x0d8001, MRA16_NOP }, /* IRQ ack? */
	{ 0x0e0000, 0x0e0001, MRA16_NOP }, /* IRQ ack? */
	{ 0x0e8000, 0x0e8001, MRA16_NOP }, /* watchdog? */
	{ 0x100000, 0x100fff, MRA16_RAM },
	{ 0x200000, 0x207fff, MRA16_RAM },
	{ 0x300000, 0x3001ff, alpha_II_trigger_r },
	{ 0x400000, 0x400fff, MRA16_RAM },
	{ 0x800000, 0x83ffff, MRA16_BANK8 }, /* Extra code bank */
MEMORY_END

static MEMORY_WRITE16_START( alpha68k_II_writemem )
	{ 0x000000, 0x03ffff, MWA16_NOP },
	{ 0x040000, 0x040fff, MWA16_RAM, &shared_ram },
	{ 0x080000, 0x080001, alpha68k_II_sound_w },
	{ 0x0c0000, 0x0c00ff, alpha68k_II_video_bank_w },
	{ 0x100000, 0x100fff, alpha68k_videoram_w, &videoram16 },
	{ 0x200000, 0x207fff, MWA16_RAM, &spriteram16 },
	{ 0x300000, 0x3001ff, alpha_microcontroller_w },
	{ 0x400000, 0x400fff, alpha68k_paletteram_w, &paletteram16 },
MEMORY_END

static MEMORY_READ16_START( alpha68k_V_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x040000, 0x043fff, MRA16_RAM },
	{ 0x080000, 0x080001, control_1_r }, /* Joysticks */
	{ 0x0c0000, 0x0c0001, control_2_V_r }, /* Dip 2 */
	{ 0x0d8000, 0x0d8001, MRA16_NOP }, /* IRQ ack? */
	{ 0x0e0000, 0x0e0001, MRA16_NOP }, /* IRQ ack? */
	{ 0x0e8000, 0x0e8001, MRA16_NOP }, /* watchdog? */
	{ 0x100000, 0x100fff, MRA16_RAM },
	{ 0x200000, 0x207fff, MRA16_RAM },
	{ 0x300000, 0x303fff, alpha_V_trigger_r },
	{ 0x400000, 0x401fff, MRA16_RAM },
	{ 0x800000, 0x83ffff, MRA16_BANK8 },
MEMORY_END

static MEMORY_WRITE16_START( alpha68k_V_writemem )
	{ 0x000000, 0x03ffff, MWA16_NOP },
	{ 0x040000, 0x043fff, MWA16_RAM, &shared_ram },
	{ 0x080000, 0x080001, alpha68k_V_sound_w },
	{ 0x0c0000, 0x0c00ff, alpha68k_V_video_control_w },
	{ 0x100000, 0x100fff, alpha68k_videoram_w, &videoram16 },
	{ 0x200000, 0x207fff, MWA16_RAM, &spriteram16 },
	{ 0x300000, 0x3001ff, alpha_microcontroller_w },
	{ 0x303e00, 0x303fff, alpha_microcontroller_w }, /* Gang Wars mirror */
	{ 0x400000, 0x401fff, alpha68k_paletteram_w, &paletteram16 },
MEMORY_END

static READ16_HANDLER(sound_cpu_r) { return 1; }

static MEMORY_READ16_START( tnexspce_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x070000, 0x073fff, MRA16_RAM },
	{ 0x0a0000, 0x0a3fff, MRA16_RAM },
	{ 0x0e0000, 0x0e0001, input_port_0_word_r },
	{ 0x0e0002, 0x0e0003, input_port_1_word_r },
	{ 0x0e0004, 0x0e0005, input_port_2_word_r },
	{ 0x0e0008, 0x0e0009, input_port_3_word_r },
	{ 0x0e000a, 0x0e000b, input_port_4_word_r },
	{ 0x0e0018, 0x0e0019, sound_cpu_r },
MEMORY_END

static MEMORY_WRITE16_START( tnexspce_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x070000, 0x073fff, MWA16_RAM },
	{ 0x0a0000, 0x0a3fff, MWA16_RAM, &spriteram16 },
	{ 0x0d0000, 0x0d0001, MWA16_NOP }, /* unknown write port (0)*/
	{ 0x0e0006, 0x0e0007, MWA16_NOP }, /* unknown write port (0)*/
	{ 0x0e000e, 0x0e000f, MWA16_NOP }, /* unknown write port (0)*/
	{ 0x0f0008, 0x0f0009, tnexspce_soundlatch_w },
	{ 0x0f0000, 0x0f000f, tnexspce_unknown_w },
MEMORY_END

/******************************************************************************/

static WRITE_HANDLER( sound_bank_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU2);

	bankaddress = 0x10000 + (data) * 0x4000;
	cpu_setbank(7,&RAM[bankaddress]);
}

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xc000, 0xffff, MRA_BANK7 },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
MEMORY_END
/*AT*/
static MEMORY_READ_START( kyros_sound_readmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
MEMORY_END

static MEMORY_WRITE_START( kyros_sound_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xe002, 0xe002, soundlatch_clear_w },
	{ 0xe004, 0xe004, DAC_0_signed_data_w },
	{ 0xe006, 0xe00e, MWA_NOP }, /* soundboard I/O's, ignored*/
/* reference only
	{ 0xe006, 0xe006, MWA_NOP }, // NMI: diminishing saw-tooth
	{ 0xe008, 0xe008, MWA_NOP }, // NMI: 00
	{ 0xe00a, 0xe00a, MWA_NOP }, // RST38: 20
	{ 0xe00c, 0xe00c, MWA_NOP }, // RST30: 00 on entry
	{ 0xe00e, 0xe00e, MWA_NOP }, // RST30: 00,02,ff on exit(0x1d88)
*/
MEMORY_END

static MEMORY_READ_START( sstingry_sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xc100, 0xc100, soundlatch_r },
MEMORY_END

static MEMORY_WRITE_START( sstingry_sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xc102, 0xc102, soundlatch_clear_w },
	{ 0xc104, 0xc104, DAC_0_signed_data_w },
	{ 0xc106, 0xc10e, MWA_NOP }, /* soundboard I/O's, ignored*/
MEMORY_END

static MEMORY_READ_START( alpha68k_I_s_readmem )
	{ 0x0000, 0x9fff, MRA_ROM }, /* sound program*/
	{ 0xe000, 0xe000, soundlatch_r },
	{ 0xe800, 0xe800, YM3812_status_port_0_r },
	{ 0xf000, 0xf7ff, MRA_RAM }, /* work RAM*/
	{ 0xfc00, 0xfc00, MRA_RAM }, /* unknown port*/
MEMORY_END

static MEMORY_WRITE_START( alpha68k_I_s_writemem )
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xe000, 0xe000, soundlatch_clear_w },
	{ 0xe800, 0xe800, YM3812_control_port_0_w },
	{ 0xec00, 0xec00, YM3812_write_port_0_w },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xfc00, 0xfc00, MWA_RAM }, /* unknown port*/
MEMORY_END
/*ZT*/

static MEMORY_READ_START( tnexspce_sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xf800, soundlatch_r }, /*AT*/
MEMORY_END

static MEMORY_WRITE_START( tnexspce_sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf800, soundlatch_clear_w }, /*AT*/
MEMORY_END

static PORT_READ_START( sound_readport )
	{ 0x00, 0x00, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_writeport )
	{ 0x00, 0x00, soundlatch_clear_w },
	{ 0x08, 0x08, DAC_0_signed_data_w },
	{ 0x0a, 0x0a, YM2413_register_port_0_w },
	{ 0x0b, 0x0b, YM2413_data_port_0_w },
	{ 0x0c, 0x0c, YM2203_control_port_0_w },
	{ 0x0d, 0x0d, YM2203_write_port_0_w },
	{ 0x0e, 0x0e, sound_bank_w },
PORT_END

static PORT_WRITE_START( kyros_sound_writeport )
	{ 0x10, 0x10, YM2203_control_port_0_w },
	{ 0x11, 0x11, YM2203_write_port_0_w },
	{ 0x80, 0x80, YM2203_write_port_1_w },
	{ 0x81, 0x81, YM2203_control_port_1_w },
	{ 0x90, 0x90, YM2203_write_port_2_w },
	{ 0x91, 0x91, YM2203_control_port_2_w },
PORT_END

static PORT_READ_START( tnexspce_sound_readport ) /*AT*/
	{ 0x00, 0x00, YM3812_status_port_0_r },
	{ 0x3b, 0x3b, MRA_NOP }, /* unknown read port*/
	{ 0x3d, 0x3d, MRA_NOP }, /* unknown read port*/
	{ 0x7b, 0x7b, MRA_NOP }, /* unknown read port*/
PORT_END

static PORT_WRITE_START( tnexspce_sound_writeport )
	{ 0x00, 0x00, YM3812_control_port_0_w },
	{ 0x20, 0x20, YM3812_write_port_0_w },
PORT_END

/******************************************************************************/

#define ALPHA68K_PLAYER_INPUT_LSB( player, button3, start, active ) \
	PORT_BIT( 0x0001, active, IPT_JOYSTICK_UP    | player ) \
	PORT_BIT( 0x0002, active, IPT_JOYSTICK_DOWN  | player ) \
	PORT_BIT( 0x0004, active, IPT_JOYSTICK_LEFT  | player ) \
	PORT_BIT( 0x0008, active, IPT_JOYSTICK_RIGHT | player ) \
	PORT_BIT( 0x0010, active, IPT_BUTTON1        | player ) \
	PORT_BIT( 0x0020, active, IPT_BUTTON2        | player ) \
	PORT_BIT( 0x0040, active, button3            | player ) \
	PORT_BIT( 0x0080, active, start )

#define ALPHA68K_PLAYER_INPUT_MSB( player, button3, start, active ) \
	PORT_BIT( 0x0100, active, IPT_JOYSTICK_UP    | player ) \
	PORT_BIT( 0x0200, active, IPT_JOYSTICK_DOWN  | player ) \
	PORT_BIT( 0x0400, active, IPT_JOYSTICK_LEFT  | player ) \
	PORT_BIT( 0x0800, active, IPT_JOYSTICK_RIGHT | player ) \
	PORT_BIT( 0x1000, active, IPT_BUTTON1        | player ) \
	PORT_BIT( 0x2000, active, IPT_BUTTON2        | player ) \
	PORT_BIT( 0x4000, active, button3            | player ) \
	PORT_BIT( 0x8000, active, start )

#define ALPHA68K_PLAYER_INPUT_SWAP_LR_LSB( player, button3, start, active ) \
	PORT_BIT( 0x0001, active, IPT_JOYSTICK_UP    | player ) \
	PORT_BIT( 0x0002, active, IPT_JOYSTICK_DOWN  | player ) \
	PORT_BIT( 0x0004, active, IPT_JOYSTICK_RIGHT | player ) \
	PORT_BIT( 0x0008, active, IPT_JOYSTICK_LEFT  | player ) \
	PORT_BIT( 0x0010, active, IPT_BUTTON1        | player ) \
	PORT_BIT( 0x0020, active, IPT_BUTTON2        | player ) \
	PORT_BIT( 0x0040, active, button3            | player ) \
	PORT_BIT( 0x0080, active, start )

#define ALPHA68K_PLAYER_INPUT_SWAP_LR_MSB( player, button3, start, active ) \
	PORT_BIT( 0x0100, active, IPT_JOYSTICK_UP    | player ) \
	PORT_BIT( 0x0200, active, IPT_JOYSTICK_DOWN  | player ) \
	PORT_BIT( 0x0400, active, IPT_JOYSTICK_RIGHT | player ) \
	PORT_BIT( 0x0800, active, IPT_JOYSTICK_LEFT  | player ) \
	PORT_BIT( 0x1000, active, IPT_BUTTON1        | player ) \
	PORT_BIT( 0x2000, active, IPT_BUTTON2        | player ) \
	PORT_BIT( 0x4000, active, button3            | player ) \
	PORT_BIT( 0x8000, active, start )

#define ALPHA68K_COINAGE_BITS_0TO2 \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )	\
	PORT_DIPSETTING(    0x07, "A 1C/1C B 1C/1C" )	\
	PORT_DIPSETTING(    0x06, "A 1C/2C B 2C/1C" )	\
	PORT_DIPSETTING(    0x05, "A 1C/3C B 3C/1C" )	\
	PORT_DIPSETTING(    0x04, "A 1C/4C B 4C/1C" )	\
	PORT_DIPSETTING(    0x03, "A 1C/5C B 5C/1C" )	\
	PORT_DIPSETTING(    0x02, "A 1C/6C B 6C/1C" )	\
	PORT_DIPSETTING(    0x01, "A 2C/3C B 7C/1C" )	\
	PORT_DIPSETTING(    0x00, "A 3C/2C B 8C/1C" )	\

#define ALPHA68K_COINAGE_BITS_1TO3 \
	PORT_DIPNAME( 0x0e, 0x0e, DEF_STR( Coinage ) )	\
	PORT_DIPSETTING(    0x0e, "A 1C/1C B 1C/1C" )	\
	PORT_DIPSETTING(    0x06, "A 1C/2C B 2C/1C" )	\
	PORT_DIPSETTING(    0x0a, "A 1C/3C B 3C/1C" )	\
	PORT_DIPSETTING(    0x02, "A 1C/4C B 4C/1C" )	\
	PORT_DIPSETTING(    0x0c, "A 1C/5C B 5C/1C" )	\
	PORT_DIPSETTING(    0x04, "A 1C/6C B 6C/1C" )	\
	PORT_DIPSETTING(    0x08, "A 2C/3C B 7C/1C" )	\
	PORT_DIPSETTING(    0x00, "A 3C/2C B 8C/1C" )

#define ALPHA68K_COINAGE_BITS_2TO4 \
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coinage ) )	\
	PORT_DIPSETTING(    0x1c, "A 1C/1C B 1C/1C" )	\
	PORT_DIPSETTING(    0x18, "A 1C/2C B 2C/1C" )	\
	PORT_DIPSETTING(    0x14, "A 1C/3C B 3C/1C" )	\
	PORT_DIPSETTING(    0x10, "A 1C/4C B 4C/1C" )	\
	PORT_DIPSETTING(    0x0c, "A 1C/5C B 5C/1C" )	\
	PORT_DIPSETTING(    0x08, "A 1C/6C B 6C/1C" )	\
	PORT_DIPSETTING(    0x04, "A 2C/3C B 7C/1C" )	\
	PORT_DIPSETTING(    0x00, "A 3C/2C B 8C/1C" )


INPUT_PORTS_START( sstingry )
	PORT_START
	ALPHA68K_PLAYER_INPUT_SWAP_LR_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_HIGH )
	ALPHA68K_PLAYER_INPUT_SWAP_LR_MSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_HIGH )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	ALPHA68K_COINAGE_BITS_1TO3
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPSETTING(    0x30, "6" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
INPUT_PORTS_END

INPUT_PORTS_START( kyros )
	PORT_START
	ALPHA68K_PLAYER_INPUT_SWAP_LR_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_HIGH )
	ALPHA68K_PLAYER_INPUT_SWAP_LR_MSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_HIGH )

	PORT_START  /* dipswitches */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	ALPHA68K_COINAGE_BITS_1TO3
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPSETTING(    0x30, "6" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
INPUT_PORTS_END

INPUT_PORTS_START( paddlema )
	PORT_START	/* control port 0 (bottom players)*/
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_UNKNOWN, IP_ACTIVE_LOW )
	ALPHA68K_PLAYER_INPUT_MSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_UNKNOWN, IP_ACTIVE_LOW )

	PORT_START	/* control port 1*/
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x0100, IP_ACTIVE_LOW, IPT_START1, "Button A (Start)", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x0200, IP_ACTIVE_LOW, IPT_START2, "Button B (Start)", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_SERVICE2 )			/* "Test" ?*/
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* control port 2 (top players)*/
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER3, IPT_UNKNOWN, IPT_UNKNOWN, IP_ACTIVE_LOW )
	ALPHA68K_PLAYER_INPUT_MSB( IPF_PLAYER4, IPT_UNKNOWN, IPT_UNKNOWN, IP_ACTIVE_LOW )

	PORT_START	/* DSW0*/
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x30, 0x30, "Game Time" )				/* See notes*/
	PORT_DIPSETTING(    0x00, "1:00" )
	PORT_DIPSETTING(    0x20, "1:10" )
	PORT_DIPSETTING(    0x10, "1:20" )
	PORT_DIPSETTING(    0x30, "1:30" )
	PORT_DIPNAME( 0xc0, 0x40, "Match Type" )
	PORT_DIPSETTING(    0x80, "A to B" )
	PORT_DIPSETTING(    0x00, "A to C" )
	PORT_DIPSETTING(    0x40, "A to E" )
/*	PORT_DIPSETTING(    0xc0, "A to B" )				*/ /* Possibility of "A only" in another version ?*/

	PORT_START	/* DSW1*/
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, "Game Mode" )
	PORT_DIPSETTING(    0x20, "Demo Sounds Off" )
	PORT_DIPSETTING(    0x00, "Demo Sounds On" )
	PORT_BITX( 0,       0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Win Match Against CPU", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x30, "Freeze" )
	PORT_DIPNAME( 0x40, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "Japanese" )
	PORT_DIPNAME( 0x80, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( timesold )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
/*	PORT_DIPSETTING(    0x08, "Normal" )*/
	PORT_DIPSETTING(    0x10, "Hard" )					/* "Difficult"*/
	PORT_DIPNAME( 0x20, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x20, "Japanese" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )			/* See notes*/
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	ALPHA68K_COINAGE_BITS_0TO2
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* player 1 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 8, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START  /* player 2 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 8, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )
INPUT_PORTS_END

/* Same as 'timesold' but different default settings for the "Language" Dip Switch */
INPUT_PORTS_START( btlfield )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
/*	PORT_DIPSETTING(    0x08, "Normal" )*/
	PORT_DIPSETTING(    0x10, "Hard" )					/* "Difficult"*/
	PORT_DIPNAME( 0x20, 0x20, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x20, "Japanese" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )			/* See notes*/
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	ALPHA68K_COINAGE_BITS_0TO2
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* player 1 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 8, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START  /* player 2 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 8, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( skysoldr )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )					/* "1"*/
	PORT_DIPSETTING(    0x10, "Normal" )				/* "2"*/
	PORT_DIPSETTING(    0x18, "Hard" )					/* "3"*/
	PORT_DIPSETTING(    0x00, "Hardest" )				/* "4"*/
	PORT_DIPNAME( 0x20, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x20, "Japanese" )
	PORT_DIPNAME( 0x40, 0x40, "Manufacturer" )			/* See notes*/
	PORT_DIPSETTING(    0x40, "SNK" )
	PORT_DIPSETTING(    0x00, "Romstar" )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	ALPHA68K_COINAGE_BITS_0TO2
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* player 1 12-way rotary control */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* player 2 12-way rotary control */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( goldmedl )
	PORT_START  /* 3 buttons per player, no joystick */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* START3 is mapped elsewhere */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START  /* 3 buttons per player, no joystick */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x04, 0x00, "Event Select" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START3 )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )		/* Called before "100m Dash" - Code at 0x005860*/
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )		/* Table at 0x02ce34*/
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )			/* Table at 0x02ce52*/
	PORT_DIPNAME( 0x40, 0x40, "Watch Computer Play" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Maximum Players" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x80, "4" )

	PORT_START /* A 6 way dip switch */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	ALPHA68K_COINAGE_BITS_2TO4
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* player 1 12-way rotary control */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START  /* player 2 12-way rotary control */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( skyadvnt )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )		/* See notes*/
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )	/* See notes*/
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	ALPHA68K_COINAGE_BITS_1TO3
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* Same as 'skyadvnt' but bits 0-3 of 2nd set of Dip Switches are different */
INPUT_PORTS_START( skyadvnu )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )		/* See notes*/
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )	/* See notes*/
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	PORT_DIPNAME( 0x01, 0x00, "Price to Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Same as Start" )
	PORT_DIPNAME( 0x02, 0x02, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( gangwars )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_BUTTON3, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_BUTTON3, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x10, 0x00, "Timer Speed" )		/* Check code at 0x01923a*/
	PORT_DIPSETTING(    0x00, "Normal" )		/* 1 second = 0x01ff*/
	PORT_DIPSETTING(    0x10, "Fast" )			/* 1 second = 0x013f*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	PORT_DIPNAME( 0x01, 0x00, "Price to Continue" )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Same as Start" )
	PORT_DIPNAME( 0x02, 0x02, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* Same as 'gangwars' but bits 0-3 of 2nd set of Dip Switches are different */
INPUT_PORTS_START( gangwarb )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_BUTTON3, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_BUTTON3, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )	/* See notes*/

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x10, 0x00, "Timer Speed" )		/* Check code at 0x01923a*/
	PORT_DIPSETTING(    0x00, "Normal" )		/* 1 second = 0x01ff*/
	PORT_DIPSETTING(    0x10, "Fast" )			/* 1 second = 0x013f*/
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START /* A 6 way dip switch */
	PORT_DIPNAME( 0x01, 0x00, "Coin Slots" )
	PORT_DIPSETTING(    0x00, "Common" )
	PORT_DIPSETTING(    0x01, "Individual" )
	PORT_DIPNAME( 0x0e, 0x0e, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Freeze" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( sbasebal )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_BUTTON3, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_BUTTON3, IPT_START2, IP_ACTIVE_LOW )

	PORT_START  /* Coin input to microcontroller */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
#if SBASEBAL_HACK
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
#else
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )	/* COIN2 - unused due to code at 0x0002b4*/
#endif

	PORT_START  /* Service + dip */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	/* 2 physical sets of _6_ dip switches */
	PORT_DIPNAME( 0x04, 0x04, "Freeze" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )	/* Check code at 0x0089e6*/
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, "Game Time" )
#if SBASEBAL_HACK
	PORT_DIPSETTING(    0x00, "4:30" )
	PORT_DIPSETTING(    0x80, "4:00" )
	PORT_DIPSETTING(    0x40, "3:30" )
	PORT_DIPSETTING(    0xc0, "3:00" )
#else
	PORT_DIPSETTING(    0x00, "3:30" )
	PORT_DIPSETTING(    0x80, "3:00" )
	PORT_DIPSETTING(    0x40, "2:30" )
	PORT_DIPSETTING(    0xc0, "2:00" )
#endif

	PORT_START /* A 6 way dip switch */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )	/* Check code at 0x009d3a*/
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
#if SBASEBAL_HACK
	ALPHA68K_COINAGE_BITS_2TO4
#else
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x00, "Price to Continue" )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Same as Start" )
#endif
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( tnexspce )
	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER1, IPT_UNKNOWN, IPT_START1, IP_ACTIVE_LOW )

	PORT_START
	ALPHA68K_PLAYER_INPUT_LSB( IPF_PLAYER2, IPT_UNKNOWN, IPT_START2, IP_ACTIVE_LOW )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Bonus Life Occurence" )
	PORT_DIPSETTING(    0x04, "1st and 2nd only" )
	PORT_DIPSETTING(    0x00, "1st, 2nd, then every 2nd" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x30, "A 1C/1C B 1C/2C" )
	PORT_DIPSETTING(    0x20, "A 2C/1C B 1C/3C" )
	PORT_DIPSETTING(    0x10, "A 3C/1C B 1C/5C" )
	PORT_DIPSETTING(    0x00, "A 4C/1C B 1C/6C" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x00, "5" )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Game Mode" )
	PORT_DIPSETTING(    0x08, "Demo Sounds Off" )
	PORT_DIPSETTING(    0x0c, "Demo Sounds On" )
	PORT_BITX( 0,       0x04, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, "Freeze" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "100000 200000" )
	PORT_DIPSETTING(    0x20, "150000 300000" )
	PORT_DIPSETTING(    0x10, "300000 500000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 chars */
	2048,
	4,      /* 4 bits per pixel  */
	{ 0, 4, 0x8000*8, (0x8000*8)+4 },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 8 consecutive bytes */
};

/* You wouldn't believe how long it took me to figure this one out.. */
static struct GfxLayout charlayout_V =
{
	8,8,
	2048,
	4,  /* 4 bits per pixel */
	{ 0,1,2,3 },
	{ 16*8+4, 16*8+0, 24*8+4, 24*8+0, 4, 0, 8*8+4, 8*8+0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	32*8    /* every sprite takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	4096*4,
	4,      /* 4 bits per pixel */
	{ 0, 0x80000*8, 0x100000*8, 0x180000*8 },
	{ 16*8+7, 16*8+6, 16*8+5, 16*8+4, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
	  7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*32    /* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout_V =
{
	16,16,  /* 16*16 sprites */
	0x5000,
	4,      /* 4 bits per pixel */
	{ 0, 0xa0000*8, 0x140000*8, 0x1e0000*8 },
	{ 16*8+7, 16*8+6, 16*8+5, 16*8+4, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
	  7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*32    /* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout tnexspce_layout =
{
	8,8,    /* 8x8 */
	RGN_FRAC(1,1),  /* Number of tiles */
	4,      /* 4 bits per pixel */
	{ 8,12,0,4 }, /*AT: changed bit plane sequence*/
	{ 8*16+3, 8*16+2, 8*16+1, 8*16+0, 3, 2, 1, 0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	32*8 /* every char takes 32 consecutive bytes */
};

static struct GfxLayout paddle_layout =
{
	8,8,    /* 8*8 chars */
	0x4000,
	4,      /* 4 bits per pixel */
	{ 0, 4, 0x40000*8, 0x40000*8+4 },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout sting_layout1 =
{
	8,8,    /* 8*8 chars */
	1024,
	3,      /* 3 bits per pixel */
	{ 4, 0+(0x10000*4), 4+(0x8000*8) },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout sting_layout2 =
{
	8,8,    /* 8*8 chars */
	1024,
	3,      /* 3 bits per pixel */
	{ 0, 0+(0x28000*8), 4+(0x28000*8) },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout sting_layout3 =
{
	8,8,    /* 8*8 chars */
	1024,
	3,      /* 3 bits per pixel */
	{ 0, 0+(0x10000*8), 4+(0x10000*8) },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout kyros_char_layout1 =
{
	8,8,    /* 8*8 chars */
	0x8000/16,
	3,  /* 3 bits per pixel */
	{ 4,0x8000*8,0x8000*8+4 },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout kyros_char_layout2 =
{
	8,8,    /* 8*8 chars */
	0x8000/16,
	3,  /* 3 bits per pixel */
	{ 0,0x10000*8,0x10000*8+4 },
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 consecutive bytes */
};

/******************************************************************************/

static struct GfxDecodeInfo alpha68k_II_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0,  16 },
	{ REGION_GFX2, 0, &spritelayout, 0, 128 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo alpha68k_V_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_V,    0,  16 },
	{ REGION_GFX2, 0, &spritelayout_V,  0, 256 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo paddle_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &paddle_layout,  0, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo tnexspce_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tnexspce_layout,  0, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo sstingry_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &sting_layout1,  0, 32 },
	{ REGION_GFX1, 0x00000, &sting_layout2,  0, 32 },
	{ REGION_GFX1, 0x10000, &sting_layout1,  0, 32 },
	{ REGION_GFX1, 0x10000, &sting_layout3,  0, 32 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo kyros_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &kyros_char_layout1,  0, 32 },
	{ REGION_GFX1, 0x00000, &kyros_char_layout2,  0, 32 },
	{ REGION_GFX1, 0x18000, &kyros_char_layout1,  0, 32 },
	{ REGION_GFX1, 0x18000, &kyros_char_layout2,  0, 32 },
	{ REGION_GFX1, 0x30000, &kyros_char_layout1,  0, 32 },
	{ REGION_GFX1, 0x30000, &kyros_char_layout2,  0, 32 },
	{ REGION_GFX1, 0x48000, &kyros_char_layout1,  0, 32 },
	{ REGION_GFX1, 0x48000, &kyros_char_layout2,  0, 32 },
	{ -1 } /* end of array */
};

/******************************************************************************/
static struct YM2413interface ym2413_interface=
{
	1,
	8000000,    /* ??? */
	{ YM2413_VOL(100,MIXER_PAN_CENTER,100,MIXER_PAN_CENTER) }
};

static struct YM2203interface ym2203_interface =
{
	1,
	3000000,    /* ??? */
	{ YM2203_VOL(65,65) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};
/*AT*/
#if 0 /* not needed*/
static struct AY8910interface ay8910_interface =
{
	2, /* 2 chips */
	1500000,
	{ 60, 60 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};
#endif

static struct YM2203interface kyros_ym2203_interface =
{
	3,
	3000000,
	{ YM2203_VOL(35,35), YM2203_VOL(35,35), YM2203_VOL(90,90) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM2203interface sstingry_ym2203_interface =
{
	3,
	3000000,
	{ YM2203_VOL(35,35), YM2203_VOL(35,35), YM2203_VOL(50,50) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static void YM3812_irq(int param)
{
	log_cb(RETRO_LOG_DEBUG, LOGPRE "irq\n");

	cpu_set_irq_line(1, 0, (param) ? HOLD_LINE : CLEAR_LINE);
}

static struct YM3812interface ym3812_interface =
{
	1,
	4000000,
	{ 100 },
	{ YM3812_irq },
};

static INTERRUPT_GEN( goldmedl_interrupt )
{
	if (cpu_getiloops() == 0)
		cpu_set_irq_line(0, 1, HOLD_LINE);
	else
		cpu_set_irq_line(0, 2, HOLD_LINE);
}
/*ZT*/

static struct DACinterface dac_interface =
{
	1,
	{ 75 }
};

static INTERRUPT_GEN( kyros_interrupt )
{
	if (cpu_getiloops() == 0)
		cpu_set_irq_line(0, 1, HOLD_LINE);
	else
		cpu_set_irq_line(0, 2, HOLD_LINE);
}

/******************************************************************************/


static MACHINE_DRIVER_START( sstingry )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 6000000) /* 24MHz/4? */
	MDRV_CPU_MEMORY(kyros_readmem,kyros_writemem)
	MDRV_CPU_VBLANK_INT(kyros_interrupt,2)

	MDRV_CPU_ADD(Z80, 3579545)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU) /* ? */
	MDRV_CPU_MEMORY(sstingry_sound_readmem,sstingry_sound_writemem)
	MDRV_CPU_PORTS(0,kyros_sound_writeport)

	MDRV_CPU_VBLANK_INT(irq0_line_hold, 2)
	MDRV_CPU_PERIODIC_INT(nmi_line_pulse, 4000)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(common)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(sstingry_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)

	MDRV_COLORTABLE_LENGTH(256)
	MDRV_PALETTE_INIT(kyros)

	MDRV_VIDEO_UPDATE(sstingry)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, sstingry_ym2203_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( kyros )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 6000000) /* 24MHz/4? */
	MDRV_CPU_MEMORY(kyros_readmem,kyros_writemem)
	MDRV_CPU_VBLANK_INT(kyros_interrupt,2)

	MDRV_CPU_ADD(Z80, 3579545)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU) /* ? */
	MDRV_CPU_MEMORY(kyros_sound_readmem,kyros_sound_writemem)
	MDRV_CPU_PORTS(0,kyros_sound_writeport)

	MDRV_CPU_VBLANK_INT(irq0_line_hold, 2)
	MDRV_CPU_PERIODIC_INT(nmi_line_pulse, 4000)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(common)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(kyros_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(256)

	MDRV_PALETTE_INIT(kyros)
	MDRV_VIDEO_UPDATE(kyros)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, kyros_ym2203_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( alpha68k_I )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 6000000) /* 24MHz/4? */
	MDRV_CPU_MEMORY(alpha68k_I_readmem,alpha68k_I_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* VBL */

	MDRV_CPU_ADD(Z80, 4000000) /* 4Mhz seems to yield the correct tone*/
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(alpha68k_I_s_readmem, alpha68k_I_s_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(paddle_gfxdecodeinfo)
/*AT*/
	/*MDRV_PALETTE_INIT(RRRR_GGGG_BBBB)*/
	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(1024)
	MDRV_PALETTE_INIT(paddlem)
/*ZT*/
	MDRV_VIDEO_UPDATE(alpha68k_I)

	/* sound hardware */
	/*MDRV_SOUND_ADD(YM2203, ym2203_interface)*/
	MDRV_SOUND_ADD(YM3812, ym3812_interface) /*AT*/
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( alpha68k_II )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* Correct */
	MDRV_CPU_MEMORY(alpha68k_II_readmem,alpha68k_II_writemem)
	MDRV_CPU_VBLANK_INT(irq3_line_hold,1)/* VBL */

	MDRV_CPU_ADD(Z80, /*3579545*/3579545*2) /* Unlikely but needed to stop nested NMI's */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU) /* Correct?? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)
	/*MDRV_CPU_VBLANK_INT(nmi_line_pulse,116)*/
	MDRV_CPU_PERIODIC_INT(nmi_line_pulse, 7500) /*AT*/

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(common)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(alpha68k_II_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(alpha68k)
	MDRV_VIDEO_UPDATE(alpha68k_II)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM2413, ym2413_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

/*AT*/
static MACHINE_DRIVER_START( alpha68k_II_gm )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000)
	MDRV_CPU_MEMORY(alpha68k_II_readmem, alpha68k_II_writemem)
	MDRV_CPU_VBLANK_INT(goldmedl_interrupt, 4)

	MDRV_CPU_ADD(Z80, 4000000*2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem, sound_writemem)
	MDRV_CPU_PORTS(sound_readport, sound_writeport)
	MDRV_CPU_PERIODIC_INT(nmi_line_pulse, 7500)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(common)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(alpha68k_II_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(alpha68k)
	MDRV_VIDEO_UPDATE(alpha68k_II)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM2413, ym2413_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END
/*ZT*/

static MACHINE_DRIVER_START( alpha68k_V )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 10000000) /* ? */
	MDRV_CPU_MEMORY(alpha68k_V_readmem,alpha68k_V_writemem)
	MDRV_CPU_VBLANK_INT(irq3_line_hold,1)/* VBL */

	MDRV_CPU_ADD(Z80, /*3579545*/3579545*2) /* Unlikely but needed to stop nested NMI's */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)
	/*MDRV_CPU_VBLANK_INT(nmi_line_pulse,148)*/
	MDRV_CPU_PERIODIC_INT(nmi_line_pulse, 8500) /*AT*/

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(common)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(alpha68k_V_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(4096)

	MDRV_VIDEO_START(alpha68k)
	MDRV_VIDEO_UPDATE(alpha68k_V)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM2413, ym2413_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( alpha68k_V_sb )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 10000000) /* ? */
	MDRV_CPU_MEMORY(alpha68k_V_readmem,alpha68k_V_writemem)
	MDRV_CPU_VBLANK_INT(irq3_line_hold,1)/* VBL */

	MDRV_CPU_ADD(Z80, /*3579545*/3579545*2) /* Unlikely but needed to stop nested NMI's */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)
	/*MDRV_CPU_VBLANK_INT(nmi_line_pulse,112)*/
	MDRV_CPU_PERIODIC_INT(nmi_line_pulse, 8500) /*AT*/

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(common)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(alpha68k_V_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(4096)

	MDRV_VIDEO_START(alpha68k)
	MDRV_VIDEO_UPDATE(alpha68k_V_sb)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM2413, ym2413_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( tnexspce )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 9000000) /* Confirmed 18 MHz/2 */
	MDRV_CPU_MEMORY(tnexspce_readmem,tnexspce_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* VBL */

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(tnexspce_sound_readmem, tnexspce_sound_writemem)
	MDRV_CPU_PORTS(tnexspce_sound_readport,tnexspce_sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(tnexspce)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(tnexspce_gfxdecodeinfo)

	MDRV_PALETTE_LENGTH(256)
	MDRV_COLORTABLE_LENGTH(1024)
	MDRV_PALETTE_INIT(paddlem)
	MDRV_VIDEO_UPDATE(alpha68k_I)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
MACHINE_DRIVER_END


/******************************************************************************/

ROM_START( sstingry )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )     /* 68000 code */
	ROM_LOAD16_BYTE( "ss_05.rom",  0x0000,  0x4000, CRC(bfb28d53) SHA1(64a1b8627529ed13074bb949cb104077eb3eac1f) )
	ROM_LOAD16_BYTE( "ss_07.rom",  0x0001,  0x4000, CRC(eb1b65c5) SHA1(cffc4df82b7950358dd28f6a492e0aefaff73048) )
	ROM_LOAD16_BYTE( "ss_04.rom",  0x8000,  0x4000, CRC(2e477a79) SHA1(0af9238979c8a740ba49776cd65ffbc024339621) )
	ROM_LOAD16_BYTE( "ss_06.rom",  0x8001,  0x4000, CRC(597620cb) SHA1(5549df4843e029df17ce5de2159cc82bd985804b) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )      /* sound cpu */
	ROM_LOAD( "ss_01.rom",       0x0000,  0x4000, CRC(fef09a92) SHA1(77b6aded1eed1bd5e6ffb25b56b62b10b7b9a304) )
	ROM_LOAD( "ss_02.rom",       0x4000,  0x4000, CRC(ab4e8c01) SHA1(d96e7f97945fff48fb7b4661fdb575ac7ff77445) )

	ROM_REGION( 0x0400, REGION_CPU3, 0 )	  /* 8748 MCU code */
	ROM_LOAD( "d8748.bin",       0x0000, 0x0400, CRC(7fcbfc30) SHA1(6d087a3d44e475b6c8260a5134952097f26459b7) )

	ROM_REGION( 0x60000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ss_12.rom",       0x00000, 0x4000, CRC(74caa9e9) SHA1(9f0874b2fcdf45acb941bd56b44bf2b9b08641e9) )
	ROM_LOAD( "ss_08.rom",       0x08000, 0x4000, CRC(32368925) SHA1(af26f73d33936410063de3164ec80f45bed487c7) )
	ROM_LOAD( "ss_13.rom",       0x10000, 0x4000, CRC(13da6203) SHA1(afa778c26da1adfdc8b2e2a1c7b2b46944b5d008) )
	ROM_LOAD( "ss_10.rom",       0x18000, 0x4000, CRC(2903234a) SHA1(552295ec60469227883eafb6756c86abc20455b5) )
	ROM_LOAD( "ss_11.rom",       0x20000, 0x4000, CRC(d134302e) SHA1(4c020ff41458c738596ab7a094d4a33a6dda64bf) )
	ROM_LOAD( "ss_09.rom",       0x28000, 0x4000, CRC(6f9d938a) SHA1(eb6934f8eaa7b22441ec4470280f228fe5f134a3) )

	ROM_REGION( 0x500, REGION_PROMS, 0 )
	ROM_LOAD( "ic92",            0x0000, 0x0100, CRC(e7ce1179) SHA1(36835c46c1c3f820df39c59c16c362db07b32dc9) )
	ROM_LOAD( "ic93",            0x0100, 0x0100, CRC(9af8a375) SHA1(abb8b094a2df41acea688f87004207dc35233db5) )
	ROM_LOAD( "ic91",            0x0200, 0x0100, CRC(c3965079) SHA1(6b1f22afd2a849f0003ddcad344079e8043681f9) )
	ROM_LOAD( "ssprom2.bin",     0x0300, 0x0100, CRC(c2205b71) SHA1(a7db60ac7d559fe53a35264fab17f1d5e48d3f10) )
	ROM_LOAD( "ssprom1.bin",     0x0400, 0x0100, CRC(1003186c) SHA1(e50b60036d6b32a4d524c92d35c4d9901ee7ec0e) )
ROM_END

ROM_START( kyros )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "2.10c", 0x00000,  0x4000, CRC(4bd030b1) SHA1(e503dae8e12995ab0a551022a848a62315908e8b) )
	ROM_CONTINUE   (          0x10000,  0x4000 )
	ROM_LOAD16_BYTE( "1.13c", 0x00001,  0x4000, CRC(75cfbc5e) SHA1(2a70c56fd7192279157df8294743038a7ed7e68d) )
	ROM_CONTINUE   (          0x10001,  0x4000 )
	ROM_LOAD16_BYTE( "4.10b", 0x08000,  0x4000, CRC(be2626c2) SHA1(c3b01ec4b65172560a993b37421df6a61b780e43) )
	ROM_CONTINUE   (          0x18000,  0x4000 )
	ROM_LOAD16_BYTE( "3.13b", 0x08001,  0x4000, CRC(fb25e71a) SHA1(fab8fcbd2c5a8600d6e8577de4875e409cad723b) )
	ROM_CONTINUE   (          0x18001,  0x4000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "2s.1f",      0x00000, 0x4000, CRC(800ceb27) SHA1(4daa1b8adcad7a90cfd5d20704a7c431673c4995) )
	ROM_LOAD( "1s.1d",      0x04000, 0x8000, CRC(87d3e719) SHA1(4b8b1b600c7c1de3a77030001e7e6f0ff118f294) )

	ROM_REGION( 0x60000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "8.9pr",      0x00000, 0x8000, CRC(c5290944) SHA1(ec97482dc59220002780ae4d02be4cd172cf65ac) )
	ROM_LOAD( "11.11m",     0x08000, 0x8000, CRC(fbd44f1e) SHA1(d095544ea76674a7ad17c1b8c88614e65890281c) )
	ROM_LOAD( "12.11n", 0x10000, 0x8000, CRC(10fed501) SHA1(71c0b4b94f86046745105307938f6e2c5661e2a1) )
	ROM_LOAD( "9.9s",   0x18000, 0x8000, CRC(dd40ca33) SHA1(91a1d8b6b69fb0d27ed315cd2591f352360bc8e7) )
	ROM_LOAD( "13.11p", 0x20000, 0x8000, CRC(e6a02030) SHA1(0de58f8cc69dc76d4b0a45fba04972634a4021a6) )
	ROM_LOAD( "14.11r", 0x28000, 0x8000, CRC(722ad23a) SHA1(0e1be976c5a406e33236def5a0dce73911ebac28) )
	ROM_LOAD( "15.3t",  0x30000, 0x8000, CRC(045fdda4) SHA1(ac25368e446e6dcfb3ed244e7d6d699f917c202d) )
	ROM_LOAD( "17.7t",  0x38000, 0x8000, CRC(7618ec00) SHA1(7346ba41fd2b04e404225726ede2e42e62ca7901) )
	ROM_LOAD( "18.9t",  0x40000, 0x8000, CRC(0ee74171) SHA1(1fda8a1066eb7dafeaeffebfe718b408b34f1767) )
	ROM_LOAD( "16.5t",  0x48000, 0x8000, CRC(2cf14824) SHA1(07f6b232c4ca6c42b3f75443b0328653f5a3f71d) )
	ROM_LOAD( "19.11t", 0x50000, 0x8000, CRC(4f336306) SHA1(83e0c021d2732d3199c70ac433a31863075a5a72) )
	ROM_LOAD( "20.13t", 0x58000, 0x8000, CRC(a165d06b) SHA1(ef7f38a71c2f3fc836b28f0eac1c14a3877f0802) )

	ROM_REGION( 0x500, REGION_PROMS, 0 )
	ROM_LOAD( "mb7114l.5r", 0x000, 0x100, CRC(3628bf36) SHA1(33b7b873a6e5da59f535d754f8c8257c0f4d0a31) )
	ROM_LOAD( "mb7114l.4r", 0x100, 0x100, CRC(850704e4) SHA1(8a9da9efc7bc6a037d4cd27152b853a7839ccd67) )
	ROM_LOAD( "mb7114l.6r", 0x200, 0x100, CRC(a54f60d7) SHA1(af039dee847913cb79f85d7abf4846322bba2e5b) )
	ROM_LOAD( "mb7114l.5p", 0x300, 0x100, CRC(1cc53765) SHA1(2b665f3e24ddb3ab591273f027ff7740f1c97e27) )
	ROM_LOAD( "mb7114l.6p", 0x400, 0x100, CRC(b0d6971f) SHA1(4aa2e9a89f9ea7487433e54ef4aa95a632477c4f) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )
	ROM_LOAD( "0.1t",      0x0000,0x2000, CRC(5d0acb4c) SHA1(52fcdcb2bf6d6ada04aa447b5526c39848bf587f) )
ROM_END

ROM_START( kyrosj )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "2j.10c", 0x00000,  0x4000, CRC(b324c11b) SHA1(9330ee0db8555a3623118c7bc5363b4f6fa87dbc) )
	ROM_CONTINUE   (          0x10000,  0x4000 )
	ROM_LOAD16_BYTE( "1j.13c", 0x00001,  0x4000, CRC(8496241b) SHA1(474cdce735dcc2ff2111ae2f4cd11c0d27a4b4fc) )
	ROM_CONTINUE   (          0x10001,  0x4000 )
	ROM_LOAD16_BYTE( "4.10a", 0x08000,  0x4000, CRC(0187f59d) SHA1(3bc1b811cb29aa33c38bc8c76e066c8b37104167) )
	ROM_CONTINUE   (          0x18000,  0x4000 )
	ROM_LOAD16_BYTE( "3.13a", 0x08001,  0x4000, CRC(ab97941d) SHA1(014a55540e1777de5bee23e59773dbbd7efa8f91) )
	ROM_CONTINUE   (          0x18001,  0x4000 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "2s.1f",      0x00000, 0x4000, CRC(800ceb27) SHA1(4daa1b8adcad7a90cfd5d20704a7c431673c4995) )
	ROM_LOAD( "1s.1d",      0x04000, 0x8000, CRC(87d3e719) SHA1(4b8b1b600c7c1de3a77030001e7e6f0ff118f294) )

	ROM_REGION( 0x60000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "8.9r",   0x00000, 0x8000, CRC(d8203284) SHA1(7dede410239be6b674644fa76c91dd01837f841f) )
	ROM_LOAD( "11.12m", 0x08000, 0x8000, CRC(a2f9738c) SHA1(31be81274bf70674bf0c32fcddbacf0f58d8f897) )
	ROM_LOAD( "12.11n", 0x10000, 0x8000, CRC(10fed501) SHA1(71c0b4b94f86046745105307938f6e2c5661e2a1) )
	ROM_LOAD( "9j.9s",  0x18000, 0x8000, CRC(3e725349) SHA1(79c431d83a0f0d5e0d69086f54f6e60a42b69e14) )
	ROM_LOAD( "13.11p", 0x20000, 0x8000, CRC(e6a02030) SHA1(0de58f8cc69dc76d4b0a45fba04972634a4021a6) )
	ROM_LOAD( "14.12r", 0x28000, 0x8000, CRC(39d07db9) SHA1(05c0785eea29bc6329892f1e8f0bd37327163080) )
	ROM_LOAD( "15.3t",  0x30000, 0x8000, CRC(045fdda4) SHA1(ac25368e446e6dcfb3ed244e7d6d699f917c202d) )
	ROM_LOAD( "17.7t",  0x38000, 0x8000, CRC(7618ec00) SHA1(7346ba41fd2b04e404225726ede2e42e62ca7901) )
	ROM_LOAD( "18.9t",  0x40000, 0x8000, CRC(0ee74171) SHA1(1fda8a1066eb7dafeaeffebfe718b408b34f1767) )
	ROM_LOAD( "16j.5t", 0x48000, 0x8000, CRC(e1566679) SHA1(3653c3160798bea203e1c7713043729b356a7358) )
	ROM_LOAD( "19.11t", 0x50000, 0x8000, CRC(4f336306) SHA1(83e0c021d2732d3199c70ac433a31863075a5a72) )
	ROM_LOAD( "20j.13t",0x58000, 0x8000, CRC(0624b4c0) SHA1(a8ebdb1f9b7fd0b78102b54523e8680aaa8bcf42) )

	ROM_REGION( 0x500, REGION_PROMS, 0 )
	ROM_LOAD( "mb7114l.5r", 0x000, 0x100, CRC(3628bf36) SHA1(33b7b873a6e5da59f535d754f8c8257c0f4d0a31) )
	ROM_LOAD( "mb7114l.4r", 0x100, 0x100, CRC(850704e4) SHA1(8a9da9efc7bc6a037d4cd27152b853a7839ccd67) )
	ROM_LOAD( "mb7114l.6r", 0x200, 0x100, CRC(a54f60d7) SHA1(af039dee847913cb79f85d7abf4846322bba2e5b) )
	ROM_LOAD( "mb7114l.5p", 0x300, 0x100, CRC(1cc53765) SHA1(2b665f3e24ddb3ab591273f027ff7740f1c97e27) )
	ROM_LOAD( "mb7114l.6p", 0x400, 0x100, CRC(b0d6971f) SHA1(4aa2e9a89f9ea7487433e54ef4aa95a632477c4f) )

	ROM_REGION( 0x2000, REGION_USER1, 0 )
	ROM_LOAD( "0j.1t",      0x0000,0x2000, CRC(a34ecb29) SHA1(60a0b0cfcd2d9830bc112774bac700ded40d4afb) )
ROM_END

ROM_START( paddlema )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "padlem.6g",  0x00000, 0x10000, CRC(c227a6e8) SHA1(9c98be6e82a0dd76fd5b786601456b060407c57f) )
	ROM_LOAD16_BYTE( "padlem.3g",  0x00001, 0x10000, CRC(f11a21aa) SHA1(6eda9ff99f2aa8832fff1e2a054c5ffb6dae7ae3) )
	ROM_LOAD16_BYTE( "padlem.6h",  0x20000, 0x10000, CRC(8897555f) SHA1(7d30aa56a727700a6e02af92b065ed982a39ccc2) )
	ROM_LOAD16_BYTE( "padlem.3h",  0x20001, 0x10000, CRC(f0fe9b9d) SHA1(2e7a80dc25c549e57b7698052f53562a9a608205) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "padlem.18c", 0x000000, 0x10000, CRC(9269778d) SHA1(bdc9100827f2e018db943d9f7d81b7936c155bf0) )

	ROM_REGION( 0x80000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "padlem.16m",      0x00000, 0x10000, CRC(0984fb4d) SHA1(6bc529db93fad277f286e4a380812c40c7f42301) )
	ROM_LOAD( "padlem.16n",      0x10000, 0x10000, CRC(4249e047) SHA1(9f35b316b5de65f8b1878fca283c9d534bb8ae25) )
	ROM_LOAD( "padlem.13m",      0x20000, 0x10000, CRC(fd9dbc27) SHA1(c01f512afef7686c64cc0766c235084cc8e2f5fc) )
	ROM_LOAD( "padlem.13n",      0x30000, 0x10000, CRC(1d460486) SHA1(4ade817a036447e7e6d4fe56fa2c5712f198c625) )
	ROM_LOAD( "padlem.9m",       0x40000, 0x10000, CRC(4ee4970d) SHA1(d57d9178129236dfb3a18688e8544e5e555ce559) )
	ROM_LOAD( "padlem.9n",       0x50000, 0x10000, CRC(a1756f15) SHA1(1220075e34c482e38eead9ea5e63b53b822e87de) )
	ROM_LOAD( "padlem.6m",       0x60000, 0x10000, CRC(3f47910c) SHA1(429d425dc57fbd868bc39c3d799bbaebcf313cc0) )
	ROM_LOAD( "padlem.6n",       0x70000, 0x10000, CRC(fe337655) SHA1(ac04124642b245d6a530c72d0dea1b1585b5cebd) )

	ROM_REGION( 0x1000, REGION_PROMS, 0 )
	ROM_LOAD( "padlem.a",        0x0000,  0x0100,  CRC(cae6bcd6) SHA1(da3b3bdcdc7fefae80b0ef8365565bbe5ff0d5d2) ) /* R */
	ROM_LOAD( "padlem.b",        0x0100,  0x0100,  CRC(b6df8dcb) SHA1(318ca20fab6608aa2956ec3bb82e8ae77c250d51) ) /* G */
	ROM_LOAD( "padlem.c",        0x0200,  0x0100,  CRC(39ca9b86) SHA1(8b8d7aae85830e69366e86f8b6cccfb8140cd526) ) /* B */
	ROM_LOAD( "padlem.17j",      0x0300,  0x0400,  CRC(86170069) SHA1(8e2ad7afa50453e9a2dc89386ce02d10e7c89fbc) ) /* Clut low nibble */
	ROM_LOAD( "padlem.16j",      0x0700,  0x0400,  CRC(8da58e2c) SHA1(6012715a2d3ba4cf8bc5a8250e7f28cb59913092) ) /* Clut high nibble */

	ROM_REGION( 0x8000, REGION_USER1, 0 )
	ROM_LOAD( "padlem.18n",      0x0000,  0x8000,  CRC(06506200) SHA1(d43337e5611cb0d3432942539ccf04bff2bdd345) ) /* Colour lookup */
ROM_END

ROM_START( timesold )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bf.3",       0x00000,  0x10000, CRC(a491e533) SHA1(e7a2e866e574ea4eb23c1c4cbd312a87c9f81b5e) )
	ROM_LOAD16_BYTE( "bf.4",       0x00001,  0x10000, CRC(34ebaccc) SHA1(dda5350d01cffee51d070eb518beecbaec7e4b21) )
	ROM_LOAD16_BYTE( "bf.1",       0x20000,  0x10000, CRC(158f4cb3) SHA1(48335a1e68afda24e1cca8cce5f869f30c6bda9c) )
	ROM_LOAD16_BYTE( "bf.2",       0x20001,  0x10000, CRC(af01a718) SHA1(588fda345b5ebd75d03d78c431227f220932ee46) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "bf.7",            0x00000,  0x08000, CRC(f8b293b5) SHA1(d326763628d7cbe864abc15d6db7fa7fe4381f31) )
	ROM_CONTINUE(                0x18000,  0x08000 )
	ROM_LOAD( "bf.8",            0x30000,  0x10000, CRC(8a43497b) SHA1(c64519b2aced8b072efdd1a6286f082094a50e61) )
	ROM_LOAD( "bf.9",            0x50000,  0x10000, CRC(1408416f) SHA1(d7a32de156791f923635d7fdddc8db97f66bfb2a) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "bf.5",            0x00000,  0x08000, CRC(3cec2f55) SHA1(e4fca0c8193680385c7dd4d6c599492c9e0dd4af) )
	ROM_LOAD( "bf.6",            0x08000,  0x08000, CRC(086a364d) SHA1(b008d4b351ada4240dd6c82c45405a2489e36019) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "bf.10",           0x000000, 0x20000, CRC(613313ba) SHA1(4940ddc5f7f4e3165a830dbfa6a65ddb23a33e12) )
	ROM_LOAD( "bf.14",           0x020000, 0x20000, CRC(efda5c45) SHA1(482d855bd0aa8cf28bb2e5ae096a7fa9491a26c8) )
	ROM_LOAD( "bf.18",           0x040000, 0x20000, CRC(e886146a) SHA1(cb4b1a002fe0c139d698fb9fd633cc9ff5daf017) )
	ROM_LOAD( "bf.11",           0x080000, 0x20000, CRC(92b42eba) SHA1(0f76d9fedaced65829a19105bb5cdfbf31c48427) )
	ROM_LOAD( "bf.15",           0x0a0000, 0x20000, CRC(ba3b9f5a) SHA1(2461f3a862889d31eee6c1572b1f47b987ac99bd) )
	ROM_LOAD( "bf.19",           0x0c0000, 0x20000, CRC(8994bf10) SHA1(424ed2c4009250bdf5cb1ad5015d0b62a2f2a990) )
	ROM_LOAD( "bf.12",           0x100000, 0x20000, CRC(7ca8bb32) SHA1(bb7747319bebb04965e536b729d76d4c7c5304e1) )
	ROM_LOAD( "bf.16",           0x120000, 0x20000, CRC(2aa74125) SHA1(8323669101ccd2c1b785e27c6a7ea43d6d61a622) )
	ROM_LOAD( "bf.20",           0x140000, 0x20000, CRC(bab6a7c5) SHA1(983f18d58cbb6852adad8723a815168c17a8c82a) )
	ROM_LOAD( "bf.13",           0x180000, 0x20000, CRC(56a3a26a) SHA1(d8485f629df98155c706ab0f725dd5fe475f1272) )
	ROM_LOAD( "bf.17",           0x1a0000, 0x20000, CRC(6b37d048) SHA1(bc7b7abd971313e50ac5f69d7dbec9de8a354537) )
	ROM_LOAD( "bf.21",           0x1c0000, 0x20000, CRC(bc3b3944) SHA1(6c99d2b093e5cb04dc3422c2f0f81a20f5a504b5) )
ROM_END

ROM_START( timesol1 )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "3",          0x00000,  0x10000, CRC(bc069a29) SHA1(891a6809931871a1da0a5a4d313623a8b92326e3) )
	ROM_LOAD16_BYTE( "4",          0x00001,  0x10000, CRC(ac7dca56) SHA1(4322d601ea5abe222f2d707fbfbfb3b207509760) )
	ROM_LOAD16_BYTE( "bf.1",       0x20000,  0x10000, CRC(158f4cb3) SHA1(48335a1e68afda24e1cca8cce5f869f30c6bda9c) )
	ROM_LOAD16_BYTE( "bf.2",       0x20001,  0x10000, CRC(af01a718) SHA1(588fda345b5ebd75d03d78c431227f220932ee46) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "bf.7",            0x00000,  0x08000, CRC(f8b293b5) SHA1(d326763628d7cbe864abc15d6db7fa7fe4381f31) )
	ROM_CONTINUE(                0x18000,  0x08000 )
	ROM_LOAD( "bf.8",            0x30000,  0x10000, CRC(8a43497b) SHA1(c64519b2aced8b072efdd1a6286f082094a50e61) )
	ROM_LOAD( "bf.9",            0x50000,  0x10000, CRC(1408416f) SHA1(d7a32de156791f923635d7fdddc8db97f66bfb2a) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "bf.5",            0x00000,  0x08000, CRC(3cec2f55) SHA1(e4fca0c8193680385c7dd4d6c599492c9e0dd4af) )
	ROM_LOAD( "bf.6",            0x08000,  0x08000, CRC(086a364d) SHA1(b008d4b351ada4240dd6c82c45405a2489e36019) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "bf.10",           0x000000, 0x20000, CRC(613313ba) SHA1(4940ddc5f7f4e3165a830dbfa6a65ddb23a33e12) )
	ROM_LOAD( "bf.14",           0x020000, 0x20000, CRC(efda5c45) SHA1(482d855bd0aa8cf28bb2e5ae096a7fa9491a26c8) )
	ROM_LOAD( "bf.18",           0x040000, 0x20000, CRC(e886146a) SHA1(cb4b1a002fe0c139d698fb9fd633cc9ff5daf017) )
	ROM_LOAD( "bf.11",           0x080000, 0x20000, CRC(92b42eba) SHA1(0f76d9fedaced65829a19105bb5cdfbf31c48427) )
	ROM_LOAD( "bf.15",           0x0a0000, 0x20000, CRC(ba3b9f5a) SHA1(2461f3a862889d31eee6c1572b1f47b987ac99bd) )
	ROM_LOAD( "bf.19",           0x0c0000, 0x20000, CRC(8994bf10) SHA1(424ed2c4009250bdf5cb1ad5015d0b62a2f2a990) )
	ROM_LOAD( "bf.12",           0x100000, 0x20000, CRC(7ca8bb32) SHA1(bb7747319bebb04965e536b729d76d4c7c5304e1) )
	ROM_LOAD( "bf.16",           0x120000, 0x20000, CRC(2aa74125) SHA1(8323669101ccd2c1b785e27c6a7ea43d6d61a622) )
	ROM_LOAD( "bf.20",           0x140000, 0x20000, CRC(bab6a7c5) SHA1(983f18d58cbb6852adad8723a815168c17a8c82a) )
	ROM_LOAD( "bf.13",           0x180000, 0x20000, CRC(56a3a26a) SHA1(d8485f629df98155c706ab0f725dd5fe475f1272) )
	ROM_LOAD( "bf.17",           0x1a0000, 0x20000, CRC(6b37d048) SHA1(bc7b7abd971313e50ac5f69d7dbec9de8a354537) )
	ROM_LOAD( "bf.21",           0x1c0000, 0x20000, CRC(bc3b3944) SHA1(6c99d2b093e5cb04dc3422c2f0f81a20f5a504b5) )
ROM_END

ROM_START( btlfield )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "bfv1_03.bin", 0x00000, 0x10000, CRC(8720af0d) SHA1(3a26dc06d98c16600b9fa0b1a12f703feac48c9d) )
	ROM_LOAD16_BYTE( "bfv1_04.bin", 0x00001, 0x10000, CRC(7dcccbe6) SHA1(33b69c139c94a9d292c93b4f148441e1bda5aba5) )
	ROM_LOAD16_BYTE( "bf.1",        0x20000, 0x10000, CRC(158f4cb3) SHA1(48335a1e68afda24e1cca8cce5f869f30c6bda9c) )
	ROM_LOAD16_BYTE( "bf.2",        0x20001, 0x10000, CRC(af01a718) SHA1(588fda345b5ebd75d03d78c431227f220932ee46) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "bf.7",            0x00000,  0x08000, CRC(f8b293b5) SHA1(d326763628d7cbe864abc15d6db7fa7fe4381f31) )
	ROM_CONTINUE(                0x18000,  0x08000 )
	ROM_LOAD( "bf.8",            0x30000,  0x10000, CRC(8a43497b) SHA1(c64519b2aced8b072efdd1a6286f082094a50e61) )
	ROM_LOAD( "bf.9",            0x50000,  0x10000, CRC(1408416f) SHA1(d7a32de156791f923635d7fdddc8db97f66bfb2a) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "bfv1_05.bin",     0x00000,  0x08000, CRC(be269dbf) SHA1(3240badbf65e076cc1f7caaec1081df9a4371d47) )
	ROM_LOAD( "bfv1_06.bin",     0x08000,  0x08000, CRC(022b9de9) SHA1(5a736a4cfe05e7681c78ab816dfe04074fe0293d) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "bf.10",           0x000000, 0x20000, CRC(613313ba) SHA1(4940ddc5f7f4e3165a830dbfa6a65ddb23a33e12) )
	ROM_LOAD( "bf.14",           0x020000, 0x20000, CRC(efda5c45) SHA1(482d855bd0aa8cf28bb2e5ae096a7fa9491a26c8) )
	ROM_LOAD( "bf.18",           0x040000, 0x20000, CRC(e886146a) SHA1(cb4b1a002fe0c139d698fb9fd633cc9ff5daf017) )
	ROM_LOAD( "bf.11",           0x080000, 0x20000, CRC(92b42eba) SHA1(0f76d9fedaced65829a19105bb5cdfbf31c48427) )
	ROM_LOAD( "bf.15",           0x0a0000, 0x20000, CRC(ba3b9f5a) SHA1(2461f3a862889d31eee6c1572b1f47b987ac99bd) )
	ROM_LOAD( "bf.19",           0x0c0000, 0x20000, CRC(8994bf10) SHA1(424ed2c4009250bdf5cb1ad5015d0b62a2f2a990) )
	ROM_LOAD( "bf.12",           0x100000, 0x20000, CRC(7ca8bb32) SHA1(bb7747319bebb04965e536b729d76d4c7c5304e1) )
	ROM_LOAD( "bf.16",           0x120000, 0x20000, CRC(2aa74125) SHA1(8323669101ccd2c1b785e27c6a7ea43d6d61a622) )
	ROM_LOAD( "bf.20",           0x140000, 0x20000, CRC(bab6a7c5) SHA1(983f18d58cbb6852adad8723a815168c17a8c82a) )
	ROM_LOAD( "bf.13",           0x180000, 0x20000, CRC(56a3a26a) SHA1(d8485f629df98155c706ab0f725dd5fe475f1272) )
	ROM_LOAD( "bf.17",           0x1a0000, 0x20000, CRC(6b37d048) SHA1(bc7b7abd971313e50ac5f69d7dbec9de8a354537) )
	ROM_LOAD( "bf.21",           0x1c0000, 0x20000, CRC(bc3b3944) SHA1(6c99d2b093e5cb04dc3422c2f0f81a20f5a504b5) )
ROM_END

ROM_START( skysoldr )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ss.3",       0x00000, 0x10000, CRC(7b88aa2e) SHA1(17ed682fb67e8fa05a1309e87ac29c09adcd7474) )
	ROM_CONTINUE ( 0x40000,      0x10000 )
	ROM_LOAD16_BYTE( "ss.4",       0x00001, 0x10000, CRC(f0283d43) SHA1(bfbc7235c9ff52b9ab269247e9c4a9d574ba25e2) )
	ROM_CONTINUE ( 0x40001,      0x10000 )
	ROM_LOAD16_BYTE( "ss.1",       0x20000, 0x10000, CRC(20e9dbc7) SHA1(632e5c7348a88620b85f968501a33609cc993972) )
	ROM_CONTINUE ( 0x60000,      0x10000 )
	ROM_LOAD16_BYTE( "ss.2",       0x20001, 0x10000, CRC(486f3432) SHA1(56b6c74031001bccb98e73f228e697556e8111d4) )
	ROM_CONTINUE ( 0x60001,      0x10000 )

	ROM_REGION( 0x80000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "ss.7",            0x00000, 0x08000, CRC(b711fad4) SHA1(0a9515cb36b8d03ee5f7e0669a9948571b4ec34e) )
	ROM_CONTINUE(                0x18000, 0x08000 )
	ROM_LOAD( "ss.8",            0x30000, 0x10000, CRC(e5cf7b37) SHA1(770ee80a1cc0f877486c6b47812db2b1118651d9) )
	ROM_LOAD( "ss.9",            0x50000, 0x10000, CRC(76124ca2) SHA1(5b87178ab663cd8aa67670f0c14c9cbb8616b04d) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "ss.5",            0x00000, 0x08000, CRC(928ba287) SHA1(c415c5b84b83ee0e5e0aa60eb33132145fcd7487) )
	ROM_LOAD( "ss.6",            0x08000, 0x08000, CRC(93b30b55) SHA1(51cacc48f4a298131852d41da80126bda5988920) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "ss.10",          0x000000, 0x20000, CRC(e48c1623) SHA1(1181e16d0d36d246ce4401b5cdacb0780acf835c) )
	ROM_LOAD( "ss.14",          0x020000, 0x20000, CRC(190c8704) SHA1(88b12720d0aa01f9df5792f8f3229e582995e4a1) )
	ROM_LOAD( "ss.18",          0x040000, 0x20000, CRC(cb6ff33a) SHA1(b1e24b11e0c5da12d21282120e3aab8773854e11) )
	ROM_LOAD( "ss.22",          0x060000, 0x20000, CRC(e69b4485) SHA1(041fcadb84d92bca4650768ad954fe47cfa00e62) )
	ROM_LOAD( "ss.11",          0x080000, 0x20000, CRC(6c63e9c5) SHA1(cb349a8e0a2a1bcd098c59a879d8e4cbb51adee6) )
	ROM_LOAD( "ss.15",          0x0a0000, 0x20000, CRC(55f71ab1) SHA1(2cb69d5532b35915b1fe5653a351508eccabd945) )
	ROM_LOAD( "ss.19",          0x0c0000, 0x20000, CRC(312a21f5) SHA1(aef32d805cd338c3980cb282e86157590fbfe791) )
	ROM_LOAD( "ss.23",          0x0e0000, 0x20000, CRC(923c19c2) SHA1(8ca88e49dc246798fd0c59cff2669a15aaa933a6) )
	ROM_LOAD( "ss.12",          0x100000, 0x20000, CRC(63bb4e89) SHA1(98b5d61469c18830459ccf8aaa51d101e4ae0a4f) )
	ROM_LOAD( "ss.16",          0x120000, 0x20000, CRC(138179f7) SHA1(2eb02077ffa4a87feed2c0503b294730f67ac484) )
	ROM_LOAD( "ss.20",          0x140000, 0x20000, CRC(268cc7b4) SHA1(a9fe6b3302bafd52214a7eb0ad8a8a238eb29542) )
	ROM_LOAD( "ss.24",          0x160000, 0x20000, CRC(f63b8417) SHA1(6a629ae6dd2b15c840f778a0f2ce9476ef578d5e) )
	ROM_LOAD( "ss.13",          0x180000, 0x20000, CRC(3506c06b) SHA1(434f12523973de0d08ae30320d833be445c36e3f) )
	ROM_LOAD( "ss.17",          0x1a0000, 0x20000, CRC(a7f524e0) SHA1(97e3e637d754a8c8c25c9b8bf8f67b1f8776e460) )
	ROM_LOAD( "ss.21",          0x1c0000, 0x20000, CRC(cb7bf5fe) SHA1(8ded0be6b7d3ac4478234df9e72e6cad95f36d21) )
	ROM_LOAD( "ss.25",          0x1e0000, 0x20000, CRC(65138016) SHA1(871b0ba39710b1094519cd808339e80ea366a016) )

	ROM_REGION16_BE( 0x80000, REGION_USER1, 0 ) /* Reload the code here for upper bank */
	ROM_LOAD16_BYTE( "ss.3",      0x00000, 0x10000, CRC(7b88aa2e) SHA1(17ed682fb67e8fa05a1309e87ac29c09adcd7474) )
	ROM_CONTINUE   ( 0x40000,     0x10000 )
	ROM_LOAD16_BYTE( "ss.4",      0x00001, 0x10000, CRC(f0283d43) SHA1(bfbc7235c9ff52b9ab269247e9c4a9d574ba25e2) )
	ROM_CONTINUE   ( 0x40001,     0x10000 )
	ROM_LOAD16_BYTE( "ss.1",      0x20000, 0x10000, CRC(20e9dbc7) SHA1(632e5c7348a88620b85f968501a33609cc993972) )
	ROM_CONTINUE   ( 0x60000,     0x10000 )
	ROM_LOAD16_BYTE( "ss.2",      0x20001, 0x10000, CRC(486f3432) SHA1(56b6c74031001bccb98e73f228e697556e8111d4) )
	ROM_CONTINUE   ( 0x60001,     0x10000 )
ROM_END

ROM_START( goldmedl )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "gm.3",      0x00000,  0x10000, CRC(ddf0113c) SHA1(1efe39da1e25e7a556c48243a15d95388bc67e69) )
	ROM_LOAD16_BYTE( "gm.4",      0x00001,  0x10000, CRC(16db4326) SHA1(7c82afcdabbb9ce082025b444ad967817ba36879) )
	ROM_LOAD16_BYTE( "gm.1",      0x20000,  0x10000, CRC(54a11e28) SHA1(5e36c86b4d30b07539d9d00c682cbc3d88b6ba01) )
	ROM_LOAD16_BYTE( "gm.2",      0x20001,  0x10000, CRC(4b6a13e4) SHA1(fb6bd4690f4f7aa7ae082c31c366c09e1eda801d) )
/*AT*/
#if 0 /* old ROM map*/
	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "goldsnd0.c47",   0x00000,  0x08000, BAD_DUMP CRC(031d27dc) ) /* bad dump*/
	ROM_CONTINUE(               0x10000,  0x78000 )
#endif

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) /* banking is slightly different from other Alpha68kII games*/
	ROM_LOAD( "38.bin",          0x00000,  0x08000, CRC(4bf251b8) SHA1(d69a6607e92dbe8081c7c66b6853f02d578ef73f) ) /* we use the bootleg set instead*/
	ROM_CONTINUE(                0x18000,  0x08000 )
	ROM_LOAD( "39.bin",          0x20000,  0x10000, CRC(1d92be86) SHA1(9b6e7141653ee7b7b1915a545d381419aec4e483) )
	ROM_LOAD( "40.bin",          0x30000,  0x10000, CRC(8dafc4e8) SHA1(7d4898557ad638ab8461060bc7ae406d7d24c5a4) )
	ROM_LOAD( "1.bin",           0x40000,  0x10000, CRC(1e78062c) SHA1(821c037edf32eb8b03e5c487d3bab0622337e80b) )
/*ZT*/
	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "gm.5",           0x000000, 0x08000, CRC(667f33f1) SHA1(6d05603b49927f09c9bb34e787b003eceaaf7062) )
	ROM_LOAD( "gm.6",           0x008000, 0x08000, CRC(56020b13) SHA1(17e176a9c82ed0d6cb5c4014034ce4e16b8ef4fb) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "goldchr3.c46",   0x000000, 0x80000, CRC(6faaa07a) SHA1(8c81ac35220835691d7620b334e83f1fb4f79a52) )
	ROM_LOAD( "goldchr2.c45",   0x080000, 0x80000, CRC(e6b0aa2c) SHA1(88d852803d92147d75853f0e7efa0f2a71820ac6) )
	ROM_LOAD( "goldchr1.c44",   0x100000, 0x80000, CRC(55db41cd) SHA1(15fa192ea2b829dc6dc0cb88fc2c5e5a30af6c91) )
	ROM_LOAD( "goldchr0.c43",   0x180000, 0x80000, CRC(76572c3f) SHA1(e7a1abf4240510810a0f9663295c0fbab9e55a63) )
ROM_END

/*AT: the bootleg set has strong resemblance of "goldmed7" on an Alpha-68K96III system board*/
ROM_START( goldmedb )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "l_3.bin",   0x00000,  0x10000, CRC(5e106bcf) SHA1(421ddfdd5ef1e9b5b7c45617fd690df982d63c4b) )
	ROM_LOAD16_BYTE( "l_4.bin",   0x00001,  0x10000, CRC(e19966af) SHA1(a2523627fcc9f5e4a82b4ebec937880fc0e0e9f3) )
	ROM_LOAD16_BYTE( "l_1.bin",   0x20000,  0x08000, CRC(7eec7ee5) SHA1(4fbb0832f50a83e5060c6891aacccc8f28a84086) )
	ROM_LOAD16_BYTE( "l_2.bin",   0x20001,  0x08000, CRC(bf59e4f9) SHA1(76c276c54f0f1cc08db7f0169fb7a1357278a1fd) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) /*AT: looks identical to goldsnd0.c47*/
	ROM_LOAD( "38.bin",          0x00000,  0x08000, CRC(4bf251b8) SHA1(d69a6607e92dbe8081c7c66b6853f02d578ef73f) )
	ROM_CONTINUE(                0x18000,  0x08000 )
	ROM_LOAD( "39.bin",          0x20000,  0x10000, CRC(1d92be86) SHA1(9b6e7141653ee7b7b1915a545d381419aec4e483) )
	ROM_LOAD( "40.bin",          0x30000,  0x10000, CRC(8dafc4e8) SHA1(7d4898557ad638ab8461060bc7ae406d7d24c5a4) )
	ROM_LOAD( "1.bin",           0x40000,  0x10000, CRC(1e78062c) SHA1(821c037edf32eb8b03e5c487d3bab0622337e80b) )

	ROM_REGION( 0x010000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "gm.5",           0x000000, 0x08000, CRC(667f33f1) SHA1(6d05603b49927f09c9bb34e787b003eceaaf7062) )
	ROM_LOAD( "gm.6",           0x008000, 0x08000, CRC(56020b13) SHA1(17e176a9c82ed0d6cb5c4014034ce4e16b8ef4fb) )
/*	ROM_LOAD( "33.bin",         0x000000, 0x10000, CRC(05600b13) )*/

	/* I haven't yet verified if these are the same as the bootleg */

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "goldchr3.c46",   0x000000, 0x80000, CRC(6faaa07a) SHA1(8c81ac35220835691d7620b334e83f1fb4f79a52) )
	ROM_LOAD( "goldchr2.c45",   0x080000, 0x80000, CRC(e6b0aa2c) SHA1(88d852803d92147d75853f0e7efa0f2a71820ac6) )
	ROM_LOAD( "goldchr1.c44",   0x100000, 0x80000, CRC(55db41cd) SHA1(15fa192ea2b829dc6dc0cb88fc2c5e5a30af6c91) )
	ROM_LOAD( "goldchr0.c43",   0x180000, 0x80000, CRC(76572c3f) SHA1(e7a1abf4240510810a0f9663295c0fbab9e55a63) )

	ROM_REGION16_BE( 0x10000, REGION_USER1, 0 )
	ROM_LOAD16_BYTE( "l_1.bin",   0x00000,  0x08000, CRC(7eec7ee5) SHA1(4fbb0832f50a83e5060c6891aacccc8f28a84086) )
	ROM_LOAD16_BYTE( "l_2.bin",   0x00001,  0x08000, CRC(bf59e4f9) SHA1(76c276c54f0f1cc08db7f0169fb7a1357278a1fd) )

	ROM_REGION( 0x10000, REGION_USER2, 0 ) /*AT: banked data for the main 68k code?*/
	ROM_LOAD( "l_5.bin",   0x00000,  0x10000, CRC(77c601a3) SHA1(5db88b0000fa5e460aa431ca7b75e8fcf629e31e) ) /* identical to gm5-1.bin in "goldmed7"*/
ROM_END

ROM_START( skyadvnt )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "sa1.bin",   0x00000,  0x20000, CRC(c2b23080) SHA1(d72430ae43137e3ecbaa327f37e4f3f028690a83) )
	ROM_LOAD16_BYTE( "sa2.bin",   0x00001,  0x20000, CRC(06074e72) SHA1(363b468fad5de0637baf8bb7b92798bfb81a07c5) )

	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "sa.3",           0x00000,  0x08000, CRC(3d0b32e0) SHA1(b845dc7b887f16dd010b9d43860b6af334995199) )
	ROM_CONTINUE(               0x18000,  0x08000 )
	ROM_LOAD( "sa.4",           0x30000,  0x10000, CRC(c2e3c30c) SHA1(43e1b489d879950bce3568433a4781377c3eebe7) )
	ROM_LOAD( "sa.5",           0x50000,  0x10000, CRC(11cdb868) SHA1(6cd9c7952b4789e819272cbe0623f3e6f607b7eb) )
	ROM_LOAD( "sa.6",           0x70000,  0x08000, CRC(237d93fd) SHA1(4c65169e4ce6a9be229410bbfd9b823060a829d7) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "sa.7",           0x000000, 0x08000, CRC(ea26e9c5) SHA1(13cb5a5955c813cd48f98f62f045a4cbc61806a1) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "sachr3",         0x000000, 0x80000, CRC(a986b8d5) SHA1(e8e2f3e0f85b9565243eab7dc8606168811f41e4) )
	ROM_LOAD( "sachr2",         0x0a0000, 0x80000, CRC(504b07ae) SHA1(ba74f74c1cb04dd1ab4acf518099605ec9c71f94) )
	ROM_LOAD( "sachr1",         0x140000, 0x80000, CRC(e734dccd) SHA1(24258dd5994f1b14600fc354b0ab36f870967afc) )
	ROM_LOAD( "sachr0",         0x1e0000, 0x80000, CRC(e281b204) SHA1(50a041c701970013b84826d67c8002ccd291bfdd) )
ROM_END

ROM_START( skyadvnu )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "sa_v3.1",   0x00000,  0x20000, CRC(862393b5) SHA1(6c9176a6ae286854f2fa7512c293984a3b952f10) )
	ROM_LOAD16_BYTE( "sa_v3.2",   0x00001,  0x20000, CRC(fa7a14d1) SHA1(d941042cff726f02e1e645a158b6a2484869464b) )

	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "sa.3",           0x00000,  0x08000, CRC(3d0b32e0) SHA1(b845dc7b887f16dd010b9d43860b6af334995199) )
	ROM_CONTINUE(               0x18000,  0x08000 )
	ROM_LOAD( "sa.4",           0x30000,  0x10000, CRC(c2e3c30c) SHA1(43e1b489d879950bce3568433a4781377c3eebe7) )
	ROM_LOAD( "sa.5",           0x50000,  0x10000, CRC(11cdb868) SHA1(6cd9c7952b4789e819272cbe0623f3e6f607b7eb) )
	ROM_LOAD( "sa.6",           0x70000,  0x08000, CRC(237d93fd) SHA1(4c65169e4ce6a9be229410bbfd9b823060a829d7) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "sa.7",           0x000000, 0x08000, CRC(ea26e9c5) SHA1(13cb5a5955c813cd48f98f62f045a4cbc61806a1) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "sachr3",         0x000000, 0x80000, CRC(a986b8d5) SHA1(e8e2f3e0f85b9565243eab7dc8606168811f41e4) )
	ROM_LOAD( "sachr2",         0x0a0000, 0x80000, CRC(504b07ae) SHA1(ba74f74c1cb04dd1ab4acf518099605ec9c71f94) )
	ROM_LOAD( "sachr1",         0x140000, 0x80000, CRC(e734dccd) SHA1(24258dd5994f1b14600fc354b0ab36f870967afc) )
	ROM_LOAD( "sachr0",         0x1e0000, 0x80000, CRC(e281b204) SHA1(50a041c701970013b84826d67c8002ccd291bfdd) )
ROM_END

ROM_START( skyadvnj )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "saj1.c19",  0x00000,  0x20000, CRC(662cb4b8) SHA1(853ad557ee7942cef542253f0e643955e27f0ed2) )
	ROM_LOAD16_BYTE( "saj2.e19",  0x00001,  0x20000, CRC(06d6130a) SHA1(3411ac90e3039e46887451fc97ec2a22ad0f18fe) )

	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "sa.3",           0x00000,  0x08000, CRC(3d0b32e0) SHA1(b845dc7b887f16dd010b9d43860b6af334995199) )
	ROM_CONTINUE(               0x18000,  0x08000 )
	ROM_LOAD( "sa.4",           0x30000,  0x10000, CRC(c2e3c30c) SHA1(43e1b489d879950bce3568433a4781377c3eebe7) )
	ROM_LOAD( "sa.5",           0x50000,  0x10000, CRC(11cdb868) SHA1(6cd9c7952b4789e819272cbe0623f3e6f607b7eb) )
	ROM_LOAD( "sa.6",           0x70000,  0x08000, CRC(237d93fd) SHA1(4c65169e4ce6a9be229410bbfd9b823060a829d7) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "sa.7",           0x000000, 0x08000, CRC(ea26e9c5) SHA1(13cb5a5955c813cd48f98f62f045a4cbc61806a1) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "sachr3",         0x000000, 0x80000, CRC(a986b8d5) SHA1(e8e2f3e0f85b9565243eab7dc8606168811f41e4) )
	ROM_LOAD( "sachr2",         0x0a0000, 0x80000, CRC(504b07ae) SHA1(ba74f74c1cb04dd1ab4acf518099605ec9c71f94) )
	ROM_LOAD( "sachr1",         0x140000, 0x80000, CRC(e734dccd) SHA1(24258dd5994f1b14600fc354b0ab36f870967afc) )
	ROM_LOAD( "sachr0",         0x1e0000, 0x80000, CRC(e281b204) SHA1(50a041c701970013b84826d67c8002ccd291bfdd) )
ROM_END

ROM_START( gangwars )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "u1",        0x00000, 0x20000, CRC(11433507) SHA1(df32c14d4105d3ad899dfa8e9dbc2a1fe51dfa6a) )
	ROM_LOAD16_BYTE( "u2",        0x00001, 0x20000, CRC(44cc375f) SHA1(38fc402014a816d9b1f7680407175adecfa39efe) )

	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "u12",            0x00000, 0x08000, CRC(2620caa1) SHA1(bd464abce0bedab68cb913321e76d83eb36ca374) )
	ROM_CONTINUE(               0x18000, 0x08000 )
	ROM_LOAD( "u9",             0x70000, 0x10000, CRC(9136745e) SHA1(d7a2bfeac69ab2dbd4565a5bd1abb1f3f1199b42) )
	ROM_LOAD( "u10",            0x50000, 0x10000, CRC(636978ae) SHA1(5d8093bc43192c89e230af318609222a69866b6e) )
	ROM_LOAD( "u11",            0x30000, 0x10000, CRC(2218ceb9) SHA1(69a843308cb0628ad856a09a33cd148f36ce0d24) )

/*

These roms are from the bootleg version, the original graphics
should be the same.  The original uses 4 512k mask roms and 4 128k
eeproms to store the same data.  The 512k roms are not dumped but
the 128k ones are and match these ones.

*/

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "gwb_ic.m19",     0x000000, 0x10000, CRC(b75bf1d0) SHA1(c22c0049274c45701be0a7be2afc0517620a3a10) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "gwb_ic.308",     0x000000, 0x10000, CRC(321a2fdd) SHA1(b2f37f14a13bc2c2f78b2b0e27fde18a23146e22) )
	ROM_LOAD( "gwb_ic.309",     0x010000, 0x10000, CRC(4d908f65) SHA1(6095a34ef4a6905d57c47af4a507dff3a04e5c07) )
	ROM_LOAD( "gwb_ic.310",     0x020000, 0x10000, CRC(fc888541) SHA1(e732a03209a88fc7a23b4e4ff69a437d6fbfc2d1) )
	ROM_LOAD( "gwb_ic.311",     0x030000, 0x10000, CRC(181b128b) SHA1(2646c9f9cca6277ddd764c07478798c8af3eb297) )
	ROM_LOAD( "gwb_ic.312",     0x040000, 0x10000, CRC(930665f3) SHA1(03af85c45acb9600b27dcdd6ec96d147046030e5) )
	ROM_LOAD( "gwb_ic.313",     0x050000, 0x10000, CRC(c18f4ca8) SHA1(f5cb666d5aa53f201b6664d1c18b89a211230e78) )
	ROM_LOAD( "gwb_ic.314",     0x060000, 0x10000, CRC(dfc44b60) SHA1(311422d4ea77118c0058e9f1a824f74cfa79cb87) )
	ROM_LOAD( "gwb_ic.307",     0x070000, 0x10000, CRC(28082a7f) SHA1(e30bade13e03bca49c1f7001c9440ce251ece15d) )
/*	ROM_LOAD( "gwb_ic.320",     0x080000, 0x10000, CRC(9a7b51d8) SHA1(0ab01972d838c938bfd07d7b4661a0ecd009b2cb) )*/
	ROM_LOAD( "gwb_ic.280",     0x080000, 0x10000, CRC(222b3dcd) SHA1(f9afe24c01daefe61939672efa2cb68bcc7235f0) ) /*AT*/
	ROM_LOAD( "gwb_ic.321",     0x090000, 0x10000, CRC(6b421c7b) SHA1(d96f91dc7e5f46990b05701483edf43a828a8879) )
	ROM_LOAD( "gwb_ic.300",     0x0a0000, 0x10000, CRC(f3fa0877) SHA1(7950ef86ee66d19693f0b7071a3a34d9200f5a19) )
	ROM_LOAD( "gwb_ic.301",     0x0b0000, 0x10000, CRC(f8c866de) SHA1(c6baa41bab35d4d9e80c5c52db74e9eb6b9605f5) )
	ROM_LOAD( "gwb_ic.302",     0x0c0000, 0x10000, CRC(5b0d587d) SHA1(852bec7d37d8cee33e5bc30080bf8a6a8d2472e5) )
	ROM_LOAD( "gwb_ic.303",     0x0d0000, 0x10000, CRC(d8c0e102) SHA1(f660876ab3457230b1c37835f5ad1ccd1e8e821a) )
	ROM_LOAD( "gwb_ic.304",     0x0e0000, 0x10000, CRC(b02bc9d8) SHA1(e0466b93c08363cceaba27c8d85e2609d12a10e7) )
	ROM_LOAD( "gwb_ic.305",     0x0f0000, 0x10000, CRC(5e04a9aa) SHA1(663330b467eb6406719d4d6cf7b05835b1600a37) )
	ROM_LOAD( "gwb_ic.306",     0x100000, 0x10000, CRC(e2172955) SHA1(af13776e6537e736815a1180a1f6bad385724b0c) )
	ROM_LOAD( "gwb_ic.299",     0x110000, 0x10000, CRC(e39f5599) SHA1(3c08a8163b528ebbcb627c511ccc2edacf0653c2) )
/*	ROM_LOAD( "gwb_ic.318",     0x120000, 0x10000, CRC(9aeaddf9) SHA1(d609314015376672be8147b9eabbfe4c5611ab73) )*/
	ROM_LOAD( "gwb_ic.320",     0x120000, 0x10000, CRC(9a7b51d8) SHA1(0ab01972d838c938bfd07d7b4661a0ecd009b2cb) ) /*AT*/
	ROM_LOAD( "gwb_ic.319",     0x130000, 0x10000, CRC(c5b862b7) SHA1(a48be3e32ae5a656d8d239796e6e7bddd4a0805b) )
	ROM_LOAD( "gwb_ic.292",     0x140000, 0x10000, CRC(c125f7be) SHA1(5d68abd91fa4fa18275c0597c51ce6d3e743d84d) )
	ROM_LOAD( "gwb_ic.293",     0x150000, 0x10000, CRC(c04fce8e) SHA1(499edd3b16770d20368f49e5c66c299740831ff0) )
	ROM_LOAD( "gwb_ic.294",     0x160000, 0x10000, CRC(4eda3df5) SHA1(574fef723ebd8fa116b4a379036ee5ec3eb10c90) )
	ROM_LOAD( "gwb_ic.295",     0x170000, 0x10000, CRC(6e60c475) SHA1(928494400bbdc3571cb5b1ccb51d39537c5fd904) )
	ROM_LOAD( "gwb_ic.296",     0x180000, 0x10000, CRC(99b2a557) SHA1(7a4053909cb5f4b2a32f3caac4e2ccdb64c2ce84) )
	ROM_LOAD( "gwb_ic.297",     0x190000, 0x10000, CRC(10373f63) SHA1(98ee65c68823530ad2eefd6e570db2f38b59c48e) )
	ROM_LOAD( "gwb_ic.298",     0x1a0000, 0x10000, CRC(df37ec4d) SHA1(d2670dde87970a6f33ca3cd81bdc9991d663bac6) )
	ROM_LOAD( "gwb_ic.291",     0x1b0000, 0x10000, CRC(beb07a2e) SHA1(f2751bef1850db7173f119fc0cfeefdf47ed7a86) )
/*	ROM_LOAD( "gwb_ic.316",     0x1c0000, 0x10000, CRC(655b1518) SHA1(d97fd911901f92786bc22dac8e085cf4fa0cb1e9) )*/
	ROM_LOAD( "gwb_ic.318",     0x1c0000, 0x10000, CRC(9aeaddf9) SHA1(d609314015376672be8147b9eabbfe4c5611ab73) ) /*AT*/
	ROM_LOAD( "gwb_ic.317",     0x1d0000, 0x10000, CRC(1622fadd) SHA1(240eaf117145773e388220513c2906ad2ac5d68b) )
	ROM_LOAD( "gwb_ic.284",     0x1e0000, 0x10000, CRC(4aa95d66) SHA1(e5bb51fd32a7e9dc23aa13de35b8757dc11f7908) )
	ROM_LOAD( "gwb_ic.285",     0x1f0000, 0x10000, CRC(3a1f3ce0) SHA1(edd8820111a3ef9558286280dc819c6d9f21212f) )
	ROM_LOAD( "gwb_ic.286",     0x200000, 0x10000, CRC(886e298b) SHA1(8e8b35a0b24c9c3a1d00079b60bdbaa6c8ce597b) )
	ROM_LOAD( "gwb_ic.287",     0x210000, 0x10000, CRC(b9542e6a) SHA1(e5762db1a44a966d2c2b7e8a92abab577e24172f) )
	ROM_LOAD( "gwb_ic.288",     0x220000, 0x10000, CRC(8e620056) SHA1(59ea29d681c4b001b656b4b8014b14ac78a69625) )
	ROM_LOAD( "gwb_ic.289",     0x230000, 0x10000, CRC(c754d69f) SHA1(e64b8e1f719f5a95b7bcab2d25a40c8b819f7d4f) )
	ROM_LOAD( "gwb_ic.290",     0x240000, 0x10000, CRC(306d1963) SHA1(2f19ba97b9bd1744b656095ae0244df2db03b09b) )
	ROM_LOAD( "gwb_ic.283",     0x250000, 0x10000, CRC(b46e5761) SHA1(3c4c13c5896186fe36ace8704afeef84b0a0cb78) )
/*	ROM_LOAD( "gwb_ic.280",     0x260000, 0x10000, CRC(222b3dcd) SHA1(f9afe24c01daefe61939672efa2cb68bcc7235f0) )*/
	ROM_LOAD( "gwb_ic.316",     0x260000, 0x10000, CRC(655b1518) SHA1(d97fd911901f92786bc22dac8e085cf4fa0cb1e9) ) /*AT*/
	ROM_LOAD( "gwb_ic.315",     0x270000, 0x10000, CRC(e7c9b103) SHA1(6f70ca9b6a7439f9250145477f682f7487e11710) )

	ROM_REGION16_BE( 0x40000, REGION_USER1, 0 ) /* Extra code bank */
	ROM_LOAD16_BYTE( "u3",        0x00000,  0x20000, CRC(de6fd3c0) SHA1(d957e8de3cb0eda1837376f687b8c272e97e1d11) )
	ROM_LOAD16_BYTE( "u4",        0x00001,  0x20000, CRC(43f7f5d3) SHA1(13ea03cfae97d0067dcfdc6febb53dbe268a91eb) )
ROM_END

ROM_START( gangwarb )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "gwb_ic.m15", 0x00000, 0x20000, CRC(7752478e) SHA1(7266dd0d2c57433191ae4d1d4e17b32c8c3c8c73) )
	ROM_LOAD16_BYTE( "gwb_ic.m16", 0x00001, 0x20000, CRC(c2f3b85e) SHA1(79c215d8b43ec7728e3745b359e64f6bb8240881) )

	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "gwb_ic.380",      0x00000, 0x08000, CRC(e6d6c9cf) SHA1(c35a7a385592e55bdfe232d042f2228f4f7e9ffa) )
	ROM_CONTINUE(                0x18000, 0x08000 )
	ROM_LOAD( "gwb_ic.419",      0x30000, 0x10000, CRC(84e5c946) SHA1(0b071d15b664a9c529713b1b896bdb5ebfa16c25) )
	ROM_LOAD( "gwb_ic.420",      0x50000, 0x10000, CRC(eb305d42) SHA1(93910cf60c1b8a87969888d8693c7d6782f1e799) )
	ROM_LOAD( "gwb_ic.421",      0x70000, 0x10000, CRC(7b9f2608) SHA1(8d61dfa32369450e396cc8a5d67c58eedb2167e6) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "gwb_ic.m19",     0x000000, 0x10000, CRC(b75bf1d0) SHA1(c22c0049274c45701be0a7be2afc0517620a3a10) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "gwb_ic.308",     0x000000, 0x10000, CRC(321a2fdd) SHA1(b2f37f14a13bc2c2f78b2b0e27fde18a23146e22) )
	ROM_LOAD( "gwb_ic.309",     0x010000, 0x10000, CRC(4d908f65) SHA1(6095a34ef4a6905d57c47af4a507dff3a04e5c07) )
	ROM_LOAD( "gwb_ic.310",     0x020000, 0x10000, CRC(fc888541) SHA1(e732a03209a88fc7a23b4e4ff69a437d6fbfc2d1) )
	ROM_LOAD( "gwb_ic.311",     0x030000, 0x10000, CRC(181b128b) SHA1(2646c9f9cca6277ddd764c07478798c8af3eb297) )
	ROM_LOAD( "gwb_ic.312",     0x040000, 0x10000, CRC(930665f3) SHA1(03af85c45acb9600b27dcdd6ec96d147046030e5) )
	ROM_LOAD( "gwb_ic.313",     0x050000, 0x10000, CRC(c18f4ca8) SHA1(f5cb666d5aa53f201b6664d1c18b89a211230e78) )
	ROM_LOAD( "gwb_ic.314",     0x060000, 0x10000, CRC(dfc44b60) SHA1(311422d4ea77118c0058e9f1a824f74cfa79cb87) )
	ROM_LOAD( "gwb_ic.307",     0x070000, 0x10000, CRC(28082a7f) SHA1(e30bade13e03bca49c1f7001c9440ce251ece15d) )
/*	ROM_LOAD( "gwb_ic.320",     0x080000, 0x10000, CRC(9a7b51d8) SHA1(0ab01972d838c938bfd07d7b4661a0ecd009b2cb) )*/
	ROM_LOAD( "gwb_ic.280",     0x080000, 0x10000, CRC(222b3dcd) SHA1(f9afe24c01daefe61939672efa2cb68bcc7235f0) ) /*AT*/
	ROM_LOAD( "gwb_ic.321",     0x090000, 0x10000, CRC(6b421c7b) SHA1(d96f91dc7e5f46990b05701483edf43a828a8879) )
	ROM_LOAD( "gwb_ic.300",     0x0a0000, 0x10000, CRC(f3fa0877) SHA1(7950ef86ee66d19693f0b7071a3a34d9200f5a19) )
	ROM_LOAD( "gwb_ic.301",     0x0b0000, 0x10000, CRC(f8c866de) SHA1(c6baa41bab35d4d9e80c5c52db74e9eb6b9605f5) )
	ROM_LOAD( "gwb_ic.302",     0x0c0000, 0x10000, CRC(5b0d587d) SHA1(852bec7d37d8cee33e5bc30080bf8a6a8d2472e5) )
	ROM_LOAD( "gwb_ic.303",     0x0d0000, 0x10000, CRC(d8c0e102) SHA1(f660876ab3457230b1c37835f5ad1ccd1e8e821a) )
	ROM_LOAD( "gwb_ic.304",     0x0e0000, 0x10000, CRC(b02bc9d8) SHA1(e0466b93c08363cceaba27c8d85e2609d12a10e7) )
	ROM_LOAD( "gwb_ic.305",     0x0f0000, 0x10000, CRC(5e04a9aa) SHA1(663330b467eb6406719d4d6cf7b05835b1600a37) )
	ROM_LOAD( "gwb_ic.306",     0x100000, 0x10000, CRC(e2172955) SHA1(af13776e6537e736815a1180a1f6bad385724b0c) )
	ROM_LOAD( "gwb_ic.299",     0x110000, 0x10000, CRC(e39f5599) SHA1(3c08a8163b528ebbcb627c511ccc2edacf0653c2) )
/*	ROM_LOAD( "gwb_ic.318",     0x120000, 0x10000, CRC(9aeaddf9) SHA1(d609314015376672be8147b9eabbfe4c5611ab73) )*/
	ROM_LOAD( "gwb_ic.320",     0x120000, 0x10000, CRC(9a7b51d8) SHA1(0ab01972d838c938bfd07d7b4661a0ecd009b2cb) ) /*AT*/
	ROM_LOAD( "gwb_ic.319",     0x130000, 0x10000, CRC(c5b862b7) SHA1(a48be3e32ae5a656d8d239796e6e7bddd4a0805b) )
	ROM_LOAD( "gwb_ic.292",     0x140000, 0x10000, CRC(c125f7be) SHA1(5d68abd91fa4fa18275c0597c51ce6d3e743d84d) )
	ROM_LOAD( "gwb_ic.293",     0x150000, 0x10000, CRC(c04fce8e) SHA1(499edd3b16770d20368f49e5c66c299740831ff0) )
	ROM_LOAD( "gwb_ic.294",     0x160000, 0x10000, CRC(4eda3df5) SHA1(574fef723ebd8fa116b4a379036ee5ec3eb10c90) )
	ROM_LOAD( "gwb_ic.295",     0x170000, 0x10000, CRC(6e60c475) SHA1(928494400bbdc3571cb5b1ccb51d39537c5fd904) )
	ROM_LOAD( "gwb_ic.296",     0x180000, 0x10000, CRC(99b2a557) SHA1(7a4053909cb5f4b2a32f3caac4e2ccdb64c2ce84) )
	ROM_LOAD( "gwb_ic.297",     0x190000, 0x10000, CRC(10373f63) SHA1(98ee65c68823530ad2eefd6e570db2f38b59c48e) )
	ROM_LOAD( "gwb_ic.298",     0x1a0000, 0x10000, CRC(df37ec4d) SHA1(d2670dde87970a6f33ca3cd81bdc9991d663bac6) )
	ROM_LOAD( "gwb_ic.291",     0x1b0000, 0x10000, CRC(beb07a2e) SHA1(f2751bef1850db7173f119fc0cfeefdf47ed7a86) )
/*	ROM_LOAD( "gwb_ic.316",     0x1c0000, 0x10000, CRC(655b1518) SHA1(d97fd911901f92786bc22dac8e085cf4fa0cb1e9) )*/
	ROM_LOAD( "gwb_ic.318",     0x1c0000, 0x10000, CRC(9aeaddf9) SHA1(d609314015376672be8147b9eabbfe4c5611ab73) ) /*AT*/
	ROM_LOAD( "gwb_ic.317",     0x1d0000, 0x10000, CRC(1622fadd) SHA1(240eaf117145773e388220513c2906ad2ac5d68b) )
	ROM_LOAD( "gwb_ic.284",     0x1e0000, 0x10000, CRC(4aa95d66) SHA1(e5bb51fd32a7e9dc23aa13de35b8757dc11f7908) )
	ROM_LOAD( "gwb_ic.285",     0x1f0000, 0x10000, CRC(3a1f3ce0) SHA1(edd8820111a3ef9558286280dc819c6d9f21212f) )
	ROM_LOAD( "gwb_ic.286",     0x200000, 0x10000, CRC(886e298b) SHA1(8e8b35a0b24c9c3a1d00079b60bdbaa6c8ce597b) )
	ROM_LOAD( "gwb_ic.287",     0x210000, 0x10000, CRC(b9542e6a) SHA1(e5762db1a44a966d2c2b7e8a92abab577e24172f) )
	ROM_LOAD( "gwb_ic.288",     0x220000, 0x10000, CRC(8e620056) SHA1(59ea29d681c4b001b656b4b8014b14ac78a69625) )
	ROM_LOAD( "gwb_ic.289",     0x230000, 0x10000, CRC(c754d69f) SHA1(e64b8e1f719f5a95b7bcab2d25a40c8b819f7d4f) )
	ROM_LOAD( "gwb_ic.290",     0x240000, 0x10000, CRC(306d1963) SHA1(2f19ba97b9bd1744b656095ae0244df2db03b09b) )
	ROM_LOAD( "gwb_ic.283",     0x250000, 0x10000, CRC(b46e5761) SHA1(3c4c13c5896186fe36ace8704afeef84b0a0cb78) )
/*	ROM_LOAD( "gwb_ic.280",     0x260000, 0x10000, CRC(222b3dcd) SHA1(f9afe24c01daefe61939672efa2cb68bcc7235f0) )*/
	ROM_LOAD( "gwb_ic.316",     0x260000, 0x10000, CRC(655b1518) SHA1(d97fd911901f92786bc22dac8e085cf4fa0cb1e9) ) /*AT*/
	ROM_LOAD( "gwb_ic.315",     0x270000, 0x10000, CRC(e7c9b103) SHA1(6f70ca9b6a7439f9250145477f682f7487e11710) )

	ROM_REGION16_BE( 0x40000, REGION_USER1, 0 ) /* Extra code bank */
	ROM_LOAD16_BYTE( "gwb_ic.m17", 0x00000, 0x20000, CRC(2a5fe86e) SHA1(0e668f51430983a17e1965143a0bf3aa4d3156ee) )
	ROM_LOAD16_BYTE( "gwb_ic.m18", 0x00001, 0x20000, CRC(c8b60c53) SHA1(352c3bcc18cf63bcb757d774c2c2247ce0c4e736) )
ROM_END

ROM_START( sbasebal )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "snksb1.bin", 0x00000, 0x20000, CRC(304fef2d) SHA1(03154e590807f7fd009068b403e1ea039272029d) )
	ROM_LOAD16_BYTE( "snksb2.bin", 0x00001, 0x20000, CRC(35821339) SHA1(2c4303bf799de7cb364cadac44ff28306088e2f4) )

	ROM_REGION( 0x90000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "snksb3.bin",      0x00000, 0x08000, CRC(89e12f25) SHA1(1c569958a7f5a91b54f1316c1d5ee027be8618d6) )
	ROM_CONTINUE(                0x18000, 0x08000 )
	ROM_LOAD( "snksb4.bin",      0x30000, 0x10000, CRC(cca2555d) SHA1(13c672331e8e5e5dd8fc3aa7829d46de6b8271f3) )
	ROM_LOAD( "snksb5.bin",      0x50000, 0x10000, CRC(f45ee36f) SHA1(cdfdf696e9fcd2827ab1dd6adc2a45085911333d) )
	ROM_LOAD( "snksb6.bin",      0x70000, 0x10000, CRC(651c9472) SHA1(bcff6679e22143cd6816c441c5a67b4956ee7ee0) )

	ROM_REGION( 0x020000, REGION_GFX1, ROMREGION_DISPOSE )  /* chars */
	ROM_LOAD( "snksb7.bin",      0x000000, 0x10000, CRC(8f3c2e25) SHA1(a5b3880f3079cce607678fd4ea5971560ce9ed8d) )

	ROM_REGION( 0x280000, REGION_GFX2, ROMREGION_DISPOSE )  /* sprites */
	ROM_LOAD( "kcbchr3.bin",     0x000000, 0x80000, CRC(719071c7) SHA1(47eded73eae25af04cf369f1a8ee657fd06b6480) )
	ROM_LOAD( "kcbchr2.bin",     0x0a0000, 0x80000, CRC(014f0f90) SHA1(e80594f06faf303c4034a711fe55dad046ebf9aa) )
	ROM_LOAD( "kcbchr1.bin",     0x140000, 0x80000, CRC(a5ce1e10) SHA1(c91cad45a918166155be3f93f4ed299389579f4a) )
	ROM_LOAD( "kcbchr0.bin",     0x1e0000, 0x80000, CRC(b8a1a088) SHA1(cb21a04387431b1810130abd86a2ebf78cf09a3b) )
ROM_END

ROM_START( tnexspce )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "ns_4.bin", 0x00000, 0x20000, CRC(4617cba3) SHA1(615a1e67fc1c76d2be004b19a965f423b8daaf5c) )
	ROM_LOAD16_BYTE( "ns_3.bin", 0x00001, 0x20000, CRC(a6c47fef) SHA1(b7e4a0fffd5c44ed0b138c1ad04c3b6644ec463b) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )   /* Sound CPU */
	ROM_LOAD( "ns_1.bin",  0x000000, 0x10000, CRC(fc26853c) SHA1(0118b048046a6125bba20dec081b936486eb1597) )

	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ns_5678.bin", 0x000000, 0x80000, CRC(22756451) SHA1(ce1d58a75ef4b09feb6fd9b3dd2de48b986070c0) )

	ROM_REGION( 0x8000, REGION_USER1, 0 )
	ROM_LOAD( "ns_2.bin",    0x0000,  0x8000,  CRC(05771d48) SHA1(9e9376b1449679f554eabf8cea023714dd1ed487) ) /* Colour lookup */

	ROM_REGION( 0x1000, REGION_PROMS, 0 ) /*AT: corrected PROM order*/
	ROM_LOAD( "ns_p2.prm", 0x0000,  0x0100,  CRC(1f388d48) SHA1(5e7dc37b4e177483f4fc65b801dca8ef132ac282) ) /* R */
	ROM_LOAD( "ns_p3.prm", 0x0100,  0x0100,  CRC(0254533a) SHA1(d0ec0d03ed78482cd9344661eab3305640e85682) ) /* G */
	ROM_LOAD( "ns_p1.prm", 0x0200,  0x0100,  CRC(488fd0e9) SHA1(cde18e9ca0b320ded821bea537c88424b02e8910) ) /* B */
	ROM_LOAD( "ns_p5.prm", 0x0300,  0x0400,  CRC(9c8527bf) SHA1(6b52ab37ea6c07a4814ed33deadd59d137b5fd4d) ) /* Clut high nibble */
	ROM_LOAD( "ns_p4.prm", 0x0700,  0x0400,  CRC(cc9ff769) SHA1(e9de0371fd8bae7f08924891d78799ace97902b1) ) /* Clut low nibble */
ROM_END

/******************************************************************************/

static READ16_HANDLER( timesold_cycle_r )
{
	int ret=shared_ram[0x4];

	if (activecpu_get_pc()==0x9ea2 && (ret&0xff00)==0) {
		cpu_spinuntil_int();
		return 0x100 | (ret&0xff);
	}

	return ret;
}

static READ16_HANDLER( timesol1_cycle_r )
{
	int ret=shared_ram[0x4];

	if (activecpu_get_pc()==0x9e20 && (ret&0xff00)==0) {
		cpu_spinuntil_int();
		return 0x100 | (ret&0xff);
	}

	return ret;
}

static READ16_HANDLER( btlfield_cycle_r )
{
	int ret=shared_ram[0x4];

	if (activecpu_get_pc()==0x9e1c && (ret&0xff00)==0) {
		cpu_spinuntil_int();
		return 0x100 | (ret&0xff);
	}

	return ret;
}

static READ16_HANDLER( skysoldr_cycle_r )
{
	int ret=shared_ram[0x4];

	if (activecpu_get_pc()==0x1f4e && (ret&0xff00)==0) {
		cpu_spinuntil_int();
		return 0x100 | (ret&0xff);
	}

	return ret;
}

static READ16_HANDLER( skyadvnt_cycle_r )
{
	int ret=shared_ram[0x4];

	if (activecpu_get_pc()==0x1f78 && (ret&0xff00)==0) {
		cpu_spinuntil_int();
		return 0x100 | (ret&0xff);
	}

	return ret;
}

static READ16_HANDLER( gangwars_cycle_r )
{
	int ret=shared_ram[0x103];

	if (activecpu_get_pc()==0xbbb6) {
		cpu_spinuntil_int();
		return (ret+2)&0xff;
	}

	return ret;
}

static READ16_HANDLER( gangwarb_cycle_r )
{
	int ret=shared_ram[0x103];

	if (activecpu_get_pc()==0xbbca) {
		cpu_spinuntil_int();
		return (ret+2)&0xff;
	}

	return ret;
}

/******************************************************************************/

static DRIVER_INIT( sstingry )
{
	invert_controls=0;
	microcontroller_id=0x00ff;
	coin_id=0x22|(0x22<<8);
}

static DRIVER_INIT( kyros )
{
	invert_controls=0;
	microcontroller_id=0x0012;
	coin_id=0x22|(0x22<<8);

	alpha68k_video_banking = kyros_video_banking;
}

static DRIVER_INIT( paddlema )
{
	microcontroller_id=0;
	coin_id=0;				/* Not needed !*/
}

static DRIVER_INIT( timesold )
{
	install_mem_read16_handler(0, 0x40008, 0x40009, timesold_cycle_r);
	invert_controls=0;
	microcontroller_id=0;
	coin_id=0x22|(0x22<<8);
}

static DRIVER_INIT( timesol1 )
{
	install_mem_read16_handler(0, 0x40008, 0x40009, timesol1_cycle_r);
	invert_controls=1;
	microcontroller_id=0;
	coin_id=0x22|(0x22<<8);
}

static DRIVER_INIT( btlfield )
{
	install_mem_read16_handler(0, 0x40008, 0x40009, btlfield_cycle_r);
	invert_controls=1;
	microcontroller_id=0;
	coin_id=0x22|(0x22<<8);
}

static DRIVER_INIT( skysoldr )
{
	install_mem_read16_handler(0, 0x40008, 0x40009, skysoldr_cycle_r);
	cpu_setbank(8, (memory_region(REGION_USER1))+0x40000);
	invert_controls=0;
	microcontroller_id=0;
	coin_id=0x22|(0x22<<8);
}

static DRIVER_INIT( goldmedl )
{
	invert_controls=0;
	microcontroller_id=0x8803; /*AT*/
	coin_id=0x23|(0x24<<8);
}

static DRIVER_INIT( goldmedb )
{
	cpu_setbank(8, memory_region(REGION_USER1));
	invert_controls=0;
	microcontroller_id=0x8803; /*Guess - routine to handle coinage is the same as in 'goldmedl'*/
	coin_id=0x23|(0x24<<8);
}

static DRIVER_INIT( skyadvnt )
{
	install_mem_read16_handler(0, 0x40008, 0x40009, skyadvnt_cycle_r);
	invert_controls=0;
	microcontroller_id=0x8814;
	coin_id=0x22|(0x22<<8);
}

static DRIVER_INIT( skyadvnu )
{
	install_mem_read16_handler(0, 0x40008, 0x40009, skyadvnt_cycle_r);
	invert_controls=0;
	microcontroller_id=0x8814;
	coin_id=0x23|(0x24<<8);
}

static DRIVER_INIT( gangwars )
{
	install_mem_read16_handler(0, 0x40206, 0x40207, gangwars_cycle_r);
	cpu_setbank(8, memory_region(REGION_USER1));
	invert_controls=0;
	microcontroller_id=0x8512;
	coin_id=0x23|(0x24<<8);
}

static DRIVER_INIT( gangwarb )
{
	install_mem_read16_handler(0, 0x40206, 0x40207, gangwarb_cycle_r);
	cpu_setbank(8, memory_region(REGION_USER1));
	invert_controls=0;
	microcontroller_id=0x8512;
	coin_id=0x23|(0x24<<8);
}

static DRIVER_INIT( sbasebal )
{
	data16_t *rom = (data16_t *)memory_region(REGION_CPU1);

	/* Game hangs on divide by zero?!  Patch it */
	rom[0xb672/2] = 0x4e71;

	/* And patch the ROM checksums */
	rom[0x44e/2] = 0x4e71;
	rom[0x450/2] = 0x4e71;
	rom[0x458/2] = 0x4e71;
	rom[0x45a/2] = 0x4e71;

#if SBASEBAL_HACK
	rom[0x2b4/2] = 0x4e71;
	rom[0x2b6/2] = 0x4e71;
#endif

	invert_controls=0;
	microcontroller_id=0x8512;	/* Same as 'gangwars' ?*/
	coin_id=0x23|(0x24<<8);
}

static DRIVER_INIT( tnexspce )
{
	invert_controls=0;
	microcontroller_id=0x890a;
	coin_id=0;				/* Not needed !*/
}

/******************************************************************************/

GAMEX(1986, sstingry, 0,        sstingry,      sstingry, sstingry, ROT90, "Alpha Denshi Co.",   "Super Stingray", GAME_NO_COCKTAIL )
GAMEX(1987, kyros,    0,        kyros,         kyros,    kyros,    ROT90, "World Games Inc",    "Kyros", GAME_NO_COCKTAIL )
GAMEX(1986, kyrosj,   kyros,    kyros,         kyros,    kyros,    ROT90, "Alpha Denshi Co.",    "Kyros No Yakata (Japan)", GAME_NO_COCKTAIL )
GAME( 1988, paddlema, 0,        alpha68k_I,    paddlema, paddlema, ROT90, "SNK",                "Paddle Mania" )
GAME( 1987, timesold, 0,        alpha68k_II,   timesold, timesold, ROT90, "[Alpha Denshi Co.] (SNK/Romstar license)", "Time Soldiers (US Rev 3)" )
GAME( 1987, timesol1, timesold, alpha68k_II,   timesold, timesol1, ROT90, "[Alpha Denshi Co.] (SNK/Romstar license)", "Time Soldiers (US Rev 1)" )
GAME( 1987, btlfield, timesold, alpha68k_II,   btlfield, btlfield, ROT90, "[Alpha Denshi Co.] (SNK license)", "Battle Field (Japan)" )
GAME( 1988, skysoldr, 0,        alpha68k_II,   skysoldr, skysoldr, ROT90, "[Alpha Denshi Co.] (SNK of America/Romstar license)", "Sky Soldiers (US)" )
GAME( 1988, goldmedl, 0,        alpha68k_II_gm,goldmedl, goldmedl, ROT0,  "SNK",                "Gold Medalist" )
GAME( 1988, goldmedb, goldmedl, alpha68k_II_gm,goldmedl, goldmedb, ROT0,  "bootleg",            "Gold Medalist (bootleg)" )
GAME( 1989, skyadvnt, 0,        alpha68k_V,    skyadvnt, skyadvnt, ROT90, "Alpha Denshi Co.",   "Sky Adventure (World)" )
GAME( 1989, skyadvnu, skyadvnt, alpha68k_V,    skyadvnu, skyadvnu, ROT90, "Alpha Denshi Co. (SNK of America license)", "Sky Adventure (US)" )
GAME( 1989, skyadvnj, skyadvnt, alpha68k_V,    skyadvnt, skyadvnt, ROT90, "Alpha Denshi Co.",   "Sky Adventure (Japan)" )
GAME( 1989, gangwars, 0,        alpha68k_V,    gangwars, gangwars, ROT0,  "Alpha Denshi Co.",   "Gang Wars (US)" )
GAME( 1989, gangwarb, gangwars, alpha68k_V,    gangwarb, gangwarb, ROT0,  "bootleg",            "Gang Wars (bootleg)" )
#if SBASEBAL_HACK
GAME( 1989, sbasebal, 0,        alpha68k_V_sb, sbasebal, sbasebal, ROT0,  "Alpha Denshi Co.",   "Super Champion Baseball (Japan)" )
#else
GAME( 1989, sbasebal, 0,        alpha68k_V_sb, sbasebal, sbasebal, ROT0,  "Alpha Denshi Co. (SNK of America license)", "Super Champion Baseball (US)" )
#endif
GAMEX(1989, tnexspce, 0,        tnexspce,      tnexspce, tnexspce, ROT90, "SNK",                "The Next Space", GAME_NO_COCKTAIL )
