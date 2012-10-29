/***************************************************************************

							-= Kaneko 16 Bit Games =-

					driver by	Luca Elia (l.elia@tin.it)


CPU    :  68000  +  MCU [Optional]

SOUND  :  OKI-M6295 x (1 | 2) + YM2149 x (0 | 2)
   OR  :  Z80  +  YM2151

OTHER  :  93C46 EEPROM [Optional]

CUSTOM :  VU-001 046A                  (48pin PQFP)
          VU-002 052 151021            (160pin PQFP)	<- Sprites
          VU-003 										<- High Colour Background
          VIEW2-CHIP 23160-509 9047EAI (144pin PQFP)	<- Tilemaps
          MUX2-CHIP                    (64pin PQFP)
          HELP1-CHIP
          IU-001 9045KP002             (44pin PQFP)
          I/O JAMMA MC-8282 047        (46pin)			<- Inputs


---------------------------------------------------------------------------
Year + Game					PCB			Notes
---------------------------------------------------------------------------
91	The Berlin Wall
	Magical Crystals		Z00FC-02
92	Bakuretsu Breaker					Incomplete dump (gfx+sfx missing)
	Blaze On							2 Sprites Chips !?
	Sand Scorpion (by Face)				MCU protection (collision detection etc.)
	Shogun Warriors						MCU protection (68k code snippets, NOT WORKING)
    B.Rap Boys                          MCU protection (not working, game can be
                                                        run on a shoggwar board ok)
94	Great 1000 Miles Rally				MCU protection (EEPROM handling etc.)
	Bonks Adventure			Z09AF-003	MCU protection
95	Great 1000 Miles Rally 2			MCU protection (EEPROM handling etc.)
---------------------------------------------------------------------------

Note: gtmr manual shows "Compatible with AX Kaneko System Board"

To Do:

[gtmr]

- Stage 4: The layers' scrolling is very jerky for a couple of seconds
  in the middle of this level (probably interrupt related)

- The layers' colours are not initialised when showing the self test
  screen and the very first screen (with the Kaneko logo in the middle).
  They're probably supposed to be disabled in those occasions, but the
  relevant registers aren't changed throughout the game (?)

[gtmr2]

- Finish the Inputs (different wheels and pedals)

- Find infos about the communication stuff (even if it won't be supported)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"
#include "machine/random.h"
#include "kaneko16.h"

/* Variables only used here: */

static int shogwarr_mcu_status, shogwarr_mcu_command_offset;
static data16_t *mcu_ram, gtmr_mcu_com[4];


/***************************************************************************


							Machine Initialisation


***************************************************************************/

MACHINE_INIT( kaneko16 )
{
	kaneko16_sprite_type  = 0;

	kaneko16_sprite_xoffs = 0;
	kaneko16_sprite_yoffs = 0;

/*
	Sx = Sprites with priority x, x = tiles with priority x,
	Sprites - Tiles Order (bottom -> top):

	0	S0	1	2	3
	0	1	S1	2	3
	0	1	2	S2	3
	0	1	2	3	S3
*/

	kaneko16_priority.tile[0] = 0;
	kaneko16_priority.tile[1] = 1;
	kaneko16_priority.tile[2] = 2;
	kaneko16_priority.tile[3] = 3;

	kaneko16_priority.sprite[0] = 0xfffc;	// above tile[0],   below the others
	kaneko16_priority.sprite[1] = 0xfff0;	// above tile[0-1], below the others
	kaneko16_priority.sprite[2] = 0xff00;	// above tile[0-2], below the others
	kaneko16_priority.sprite[3] = 0x0000;	// above all
}

static MACHINE_INIT( berlwall )
{
	machine_init_kaneko16();

	kaneko16_sprite_type = 2;	// like type 0, but using 16 instead of 8 bytes
}

static MACHINE_INIT( blazeon )
{
	machine_init_kaneko16();

	kaneko16_sprite_xoffs = 0x10000 - 0x680;
	kaneko16_sprite_yoffs = 0x000;

/*
	Sx = Sprites with priority x, x = tiles with priority x,
	Sprites - Tiles Order (bottom -> top):

	0	S0	1	2	3
	0	1	S1	2	3
	0	1	2	3	S2
	0	1	2	3	S3
*/
	kaneko16_priority.sprite[0] = 0xfffc;	// above tile[0], below the others
	kaneko16_priority.sprite[1] = 0xfff0;	// above tile[0-1], below the others
	kaneko16_priority.sprite[2] = 0x0000;	// above all
	kaneko16_priority.sprite[3] = 0x0000;	// ""
}

static MACHINE_INIT( bloodwar )
{
	/* Priorities unknown */
	kaneko16_priority.tile[0] = 0;
	kaneko16_priority.tile[1] = 1;
	kaneko16_priority.tile[2] = 2;
	kaneko16_priority.tile[3] = 3;

	kaneko16_priority.sprite[0] = 0x0000;	// above all
	kaneko16_priority.sprite[1] = 0x0000;	// above all
	kaneko16_priority.sprite[2] = 0x0000;	// above all
	kaneko16_priority.sprite[3] = 0x0000;	// above all

	kaneko16_sprite_type = 1;

	memset(gtmr_mcu_com, 0, 4 * sizeof( data16_t) );
}

static MACHINE_INIT( bakubrkr )
{
	machine_init_kaneko16();

	kaneko16_priority.tile[0] = 0;
	kaneko16_priority.tile[1] = 1;
	kaneko16_priority.tile[2] = 2;
	kaneko16_priority.tile[3] = 3;

	kaneko16_priority.sprite[0] = 0x0000;	// above all
	kaneko16_priority.sprite[1] = 0x0000;	// above all
	kaneko16_priority.sprite[2] = 0x0000;	// above all
	kaneko16_priority.sprite[3] = 0x0000;	// above all
}

static MACHINE_INIT( gtmr )
{
	machine_init_kaneko16();

	kaneko16_sprite_type = 1;

	memset(gtmr_mcu_com, 0, 4 * sizeof( data16_t) );
}

static MACHINE_INIT( mgcrystl )
{
	machine_init_kaneko16();
/*
	Sx = Sprites with priority x, x = tiles with priority x,
	Sprites - Tiles Order:

	S0:	below 0 2

	S1:	over  2
		below 0

	S2:	over 0 2

	S3:	over all

	tiles of the 2nd VIEW2 chip always behind sprites?

*/
	kaneko16_priority.tile[0] = 2;	// priorty mask = 1
	kaneko16_priority.tile[1] = 0;	// priorty mask = 2
	kaneko16_priority.tile[2] = 3;	// priorty mask = 4
	kaneko16_priority.tile[3] = 1;	// priorty mask = 8

	kaneko16_priority.sprite[0] = 0xfffe;	// below all
	kaneko16_priority.sprite[1] = 0xfffc;	// above tile[0], below the others
	kaneko16_priority.sprite[2] = 0x0000;	// above all
	kaneko16_priority.sprite[3] = 0x0000;	// ""
}

static MACHINE_INIT( sandscrp )
{
	machine_init_kaneko16();

	kaneko16_sprite_type = 3;	// "different" sprites layout

	watchdog_reset16_r(0,0);	// start with an armed watchdog
}

static MACHINE_INIT( shogwarr )
{
	machine_init_kaneko16();

	shogwarr_mcu_status = 0;
	shogwarr_mcu_command_offset = 0;
}


/***************************************************************************


							MCU Code Simulation


***************************************************************************/

/***************************************************************************
								Blood Warrior
***************************************************************************/

void bloodwar_mcu_run(void)
{
	data16_t mcu_command	=	mcu_ram[0x0010/2];
	data16_t mcu_offset		=	mcu_ram[0x0012/2] / 2;
	data16_t mcu_data		=	mcu_ram[0x0014/2];

	logerror("CPU #0 PC %06X : MCU executed command: %04X %04X %04X\n",activecpu_get_pc(),mcu_command,mcu_offset*2,mcu_data);

	switch (mcu_command >> 8)
	{
#if 0
		case 0x02:	// TEST
		{
			/* MCU writes the string " ATOP 1993.12 " to shared ram */
			mcu_ram[mcu_offset + 0x70/2 + 0] = 0x2041;
			mcu_ram[mcu_offset + 0x70/2 + 1] = 0x544F;
			mcu_ram[mcu_offset + 0x70/2 + 2] = 0x5020;
			mcu_ram[mcu_offset + 0x70/2 + 3] = 0x3139;
			mcu_ram[mcu_offset + 0x70/2 + 4] = 0x3933;
			mcu_ram[mcu_offset + 0x70/2 + 5] = 0x2E31;
			mcu_ram[mcu_offset + 0x70/2 + 6] = 0x3220;
			mcu_ram[mcu_offset + 0x70/2 + 7] = 0xff00;

			mcu_ram[mcu_offset + 0x10/2 + 0] = 0x0000;
			mcu_ram[mcu_offset + 0x12/2 + 0] = 0x0000;
		}
		break;
#endif

		case 0x02:	// Read from NVRAM
		{
			mame_file *f;
			if ((f = mame_fopen(Machine->gamedrv->name,0,FILETYPE_NVRAM,0)) != 0)
			{
				mame_fread(f,&mcu_ram[mcu_offset], 128);
				mame_fclose(f);
			}
			else
				memcpy(&mcu_ram[mcu_offset],memory_region(REGION_USER1),128);
		}
		break;

		case 0x42:	// Write to NVRAM
		{
			mame_file *f;
			if ((f = mame_fopen(Machine->gamedrv->name,0,FILETYPE_NVRAM,1)) != 0)
			{
				mame_fwrite(f,&mcu_ram[mcu_offset], 128);
				mame_fclose(f);
			}
		}
		break;

		case 0x03:	// DSW
		{
			mcu_ram[mcu_offset] = readinputport(4);
		}
		break;

		case 0x04:	// Protection
		{
			switch(mcu_data)
			{
				// unknown data
				case 0x01:
				case 0x02:
				case 0x03:
				case 0x04:
				case 0x05:
				case 0x06:
				case 0x07:
				case 0x08:
				case 0x09:
				break;

				// palette data
				case 0x0a:
				case 0x0b:
				case 0x0c:
				case 0x0d:
				case 0x0e:
				case 0x0f:
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
				case 0x14:
				case 0x15:
				case 0x16:
				case 0x17:
				case 0x18:
				case 0x19:
				{
					mcu_ram[mcu_offset + 0] = 0x0001;	// number of palettes (>=1)
					// palette data follows (each palette is 0x200 byes long)
					mcu_ram[mcu_offset + 1] = 0x8000;	// a negative word will end the palette
				}
				break;

				// tilemap data
				case 0x1c:
				case 0x1d:
				case 0x1e:
				case 0x1f:
				case 0x20:
				case 0x21:
				case 0x22:
				case 0x23:
				case 0x24:
				{
					// tile data (ff means no tiles) followed by routine index
					mcu_ram[mcu_offset + 0] = 0xff00;
				}
				break;

				// unknown long
				case 0x25:
				case 0x26:
				case 0x27:
				case 0x28:
				case 0x29:
				case 0x2a:
				case 0x2b:
				case 0x2c:
				case 0x2d:
				{
					mcu_ram[mcu_offset + 0] = 0x0000;
					mcu_ram[mcu_offset + 1] = 0x0000;
				}
				break;

				default:
					logerror("UNKNOWN PARAMETER %02X TO COMMAND 4\n",mcu_data);
			}
		}
		break;

		default:
			logerror("UNKNOWN COMMAND\n");
		break;
	}
}

#define BLOODWAR_MCU_COM_W(_n_) \
WRITE16_HANDLER( bloodwar_mcu_com##_n_##_w ) \
{ \
	COMBINE_DATA(&gtmr_mcu_com[_n_]); \
	if (gtmr_mcu_com[0] != 0xFFFF)	return; \
	if (gtmr_mcu_com[1] != 0xFFFF)	return; \
	if (gtmr_mcu_com[2] != 0xFFFF)	return; \
	if (gtmr_mcu_com[3] != 0xFFFF)	return; \
\
	memset(gtmr_mcu_com, 0, 4 * sizeof( data16_t ) ); \
	bloodwar_mcu_run(); \
}

BLOODWAR_MCU_COM_W(0)
BLOODWAR_MCU_COM_W(1)
BLOODWAR_MCU_COM_W(2)
BLOODWAR_MCU_COM_W(3)

/***************************************************************************
							Great 1000 Miles Rally
***************************************************************************/

const struct GameDriver driver_gtmr;
const struct GameDriver driver_gtmre;
const struct GameDriver driver_gtmrusa;
const struct GameDriver driver_gtmr2;

/*

	MCU Tasks:

	- Write and ID string to shared RAM.
	- Access the EEPROM
	- Read the DSWs

*/

void gtmr_mcu_run(void)
{
	data16_t mcu_command	=	mcu_ram[0x0010/2];
	data16_t mcu_offset		=	mcu_ram[0x0012/2] / 2;
	data16_t mcu_data		=	mcu_ram[0x0014/2];

	logerror("CPU #0 PC %06X : MCU executed command: %04X %04X %04X\n",activecpu_get_pc(),mcu_command,mcu_offset*2,mcu_data);

	switch (mcu_command >> 8)
	{

		case 0x02:	// Read from NVRAM
		{
			mame_file *f;
			if ((f = mame_fopen(Machine->gamedrv->name,0,FILETYPE_NVRAM,0)) != 0)
			{
				mame_fread(f,&mcu_ram[mcu_offset], 128);
				mame_fclose(f);
			}
		}
		break;

		case 0x42:	// Write to NVRAM
		{
			mame_file *f;
			if ((f = mame_fopen(Machine->gamedrv->name,0,FILETYPE_NVRAM,1)) != 0)
			{
				mame_fwrite(f,&mcu_ram[mcu_offset], 128);
				mame_fclose(f);
			}
		}
		break;

		case 0x03:	// DSW
		{
			mcu_ram[mcu_offset] = readinputport(4);
		}
		break;

		case 0x04:	// TEST (2 versions)
		{
			if (Machine->gamedrv == &driver_gtmr)
			{
				/* MCU writes the string "MM0525-TOYBOX199" to shared ram */
				mcu_ram[mcu_offset+0] = 0x4d4d;
				mcu_ram[mcu_offset+1] = 0x3035;
				mcu_ram[mcu_offset+2] = 0x3235;
				mcu_ram[mcu_offset+3] = 0x2d54;
				mcu_ram[mcu_offset+4] = 0x4f59;
				mcu_ram[mcu_offset+5] = 0x424f;
				mcu_ram[mcu_offset+6] = 0x5831;
				mcu_ram[mcu_offset+7] = 0x3939;
			}
			else if ( (Machine->gamedrv == &driver_gtmre)  ||
					  (Machine->gamedrv == &driver_gtmrusa) ||
					  (Machine->gamedrv == &driver_gtmr2) )
			{
				/* MCU writes the string "USMM0713-TB1994 " to shared ram */
				mcu_ram[mcu_offset+0] = 0x5553;
				mcu_ram[mcu_offset+1] = 0x4d4d;
				mcu_ram[mcu_offset+2] = 0x3037;
				mcu_ram[mcu_offset+3] = 0x3133;
				mcu_ram[mcu_offset+4] = 0x2d54;
				mcu_ram[mcu_offset+5] = 0x4231;
				mcu_ram[mcu_offset+6] = 0x3939;
				mcu_ram[mcu_offset+7] = 0x3420;
			}
		}
		break;
	}

}


#define GTMR_MCU_COM_W(_n_) \
WRITE16_HANDLER( gtmr_mcu_com##_n_##_w ) \
{ \
	COMBINE_DATA(&gtmr_mcu_com[_n_]); \
	if (gtmr_mcu_com[0] != 0xFFFF)	return; \
	if (gtmr_mcu_com[1] != 0xFFFF)	return; \
	if (gtmr_mcu_com[2] != 0xFFFF)	return; \
	if (gtmr_mcu_com[3] != 0xFFFF)	return; \
\
	memset(gtmr_mcu_com, 0, 4 * sizeof( data16_t ) ); \
	gtmr_mcu_run(); \
}

GTMR_MCU_COM_W(0)
GTMR_MCU_COM_W(1)
GTMR_MCU_COM_W(2)
GTMR_MCU_COM_W(3)


/***************************************************************************
								Sand Scorpion
***************************************************************************/

/*

	MCU Tasks:

	- Collision detection (test if 2 rectangles overlap)
	- Multiply 2 words, obtaining a long word
	- Return a random value?

*/

static READ16_HANDLER( sandscrp_mcu_ram_r )
{
	switch( offset )
	{
		case 0x04/2:	// Bit 0: collision detection
		{
			/* First rectangle */
			int x_10		=	mcu_ram[0x00/2];
			int x_11		=	mcu_ram[0x02/2] + x_10;
			int y_10		=	mcu_ram[0x04/2];
			int y_11		=	mcu_ram[0x06/2] + y_10;

			/* Second rectangle */
			int x_20		=	mcu_ram[0x08/2];
			int x_21		=	mcu_ram[0x0a/2] + x_20;
			int y_20		=	mcu_ram[0x0c/2];
			int y_21		=	mcu_ram[0x0e/2] + y_20;

			/* Sign extend the words */
			x_10 = (x_10 & 0x7fff) - (x_10 & 0x8000);
			x_11 = (x_11 & 0x7fff) - (x_11 & 0x8000);
			y_10 = (y_10 & 0x7fff) - (y_10 & 0x8000);
			y_11 = (y_11 & 0x7fff) - (y_11 & 0x8000);
			x_20 = (x_20 & 0x7fff) - (x_20 & 0x8000);
			x_21 = (x_21 & 0x7fff) - (x_21 & 0x8000);
			y_20 = (y_20 & 0x7fff) - (y_20 & 0x8000);
			y_21 = (y_21 & 0x7fff) - (y_21 & 0x8000);

			/* Check if they overlap */
			if	(	( x_10 > x_21 ) || ( x_11 < x_20 ) ||
					( y_10 > y_21 ) || ( y_11 < y_20 )	)
				return 0;
			else
				return 1;
		}
		break;

		case 0x10/2:	// Multiply 2 words, obtain a long word.
		case 0x12/2:
		{
			int res = mcu_ram[0x10/2] * mcu_ram[0x12/2];
			if (offset == 0x10/2)	return (res >> 16) & 0xffff;
			else					return (res >>  0) & 0xffff;
		}
		break;

		case 0x14/2:	// Random?
			return (mame_rand() & 0xffff);
	}

	logerror("CPU #0 PC %06X : Unknown MCU word %04X read\n",activecpu_get_pc(),offset*2);
	return mcu_ram[offset];
}

static WRITE16_HANDLER( sandscrp_mcu_ram_w )
{
	COMBINE_DATA(&mcu_ram[offset]);
}


/***************************************************************************
								Shogun Warriors
***************************************************************************/

/* Preliminary simulation: the game doesn't work */

/*

	MCU Tasks:

	- Read the DSWs
	- Supply code snippets to the 68000

*/

void shogwarr_mcu_run(void)
{
	data16_t mcu_command;

	if ( shogwarr_mcu_status != (1|2|4|8) )	return;

	mcu_command = mcu_ram[shogwarr_mcu_command_offset + 0];

	if (mcu_command == 0) return;

	logerror("CPU #0 PC %06X : MCU executed command at %04X: %04X\n",
	 	activecpu_get_pc(),shogwarr_mcu_command_offset*2,mcu_command);

	switch (mcu_command)
	{

		case 0x00ff:
		{
			int param1 = mcu_ram[shogwarr_mcu_command_offset + 1];
			int param2 = mcu_ram[shogwarr_mcu_command_offset + 2];
			int param3 = mcu_ram[shogwarr_mcu_command_offset + 3];
//			int param4 = mcu_ram[shogwarr_mcu_command_offset + 4];
			int param5 = mcu_ram[shogwarr_mcu_command_offset + 5];
//			int param6 = mcu_ram[shogwarr_mcu_command_offset + 6];
//			int param7 = mcu_ram[shogwarr_mcu_command_offset + 7];

			// clear old command (handshake to main cpu)
			mcu_ram[shogwarr_mcu_command_offset] = 0x0000;

			// execute the command:

			mcu_ram[param1 / 2] = ~readinputport(4);	// DSW
			mcu_ram[param2 / 2] = 0xffff;				// ? -1 / anything else

			shogwarr_mcu_command_offset = param3 / 2;	// where next command will be written?
			// param 4?
			mcu_ram[param5 / 2] = 0x8ee4;				// MCU Rom Checksum!
			// param 6&7 = address.l
/*

First code snippet provided by the MCU:

207FE0: 48E7 FFFE                movem.l D0-D7/A0-A6, -(A7)

207FE4: 3039 00A8 0000           move.w  $a80000.l, D0
207FEA: 4279 0020 FFFE           clr.w   $20fffe.l

207FF0: 41F9 0020 0000           lea     $200000.l, A0
207FF6: 7000                     moveq   #$0, D0

207FF8: 43E8 01C6                lea     ($1c6,A0), A1
207FFC: 7E02                     moveq   #$2, D7
207FFE: D059                     add.w   (A1)+, D0
208000: 51CF FFFC                dbra    D7, 207ffe

208004: 43E9 0002                lea     ($2,A1), A1
208008: 7E04                     moveq   #$4, D7
20800A: D059                     add.w   (A1)+, D0
20800C: 51CF FFFC                dbra    D7, 20800a

208010: 4640                     not.w   D0
208012: 5340                     subq.w  #1, D0
208014: 0068 0030 0216           ori.w   #$30, ($216,A0)

20801A: B07A 009A                cmp.w   ($9a,PC), D0; ($2080b6)
20801E: 670A                     beq     20802a

208020: 0268 000F 0216           andi.w  #$f, ($216,A0)
208026: 4268 0218                clr.w   ($218,A0)

20802A: 5468 0216                addq.w  #2, ($216,A0)
20802E: 42A8 030C                clr.l   ($30c,A0)
208032: 117C 0020 030C           move.b  #$20, ($30c,A0)

208038: 3E3C 0001                move.w  #$1, D7

20803C: 0C68 0008 0218           cmpi.w  #$8, ($218,A0)
208042: 6C00 0068                bge     2080ac

208046: 117C 0080 0310           move.b  #$80, ($310,A0)
20804C: 117C 0008 0311           move.b  #$8, ($311,A0)
208052: 317C 7800 0312           move.w  #$7800, ($312,A0)
208058: 5247                     addq.w  #1, D7
20805A: 0C68 0040 0216           cmpi.w  #$40, ($216,A0)
208060: 6D08                     blt     20806a

208062: 5468 0218                addq.w  #2, ($218,A0)
208066: 6000 0044                bra     2080ac

20806A: 117C 0041 0314           move.b  #$41, ($314,A0)

208070: 0C39 0001 0010 2E12      cmpi.b  #$1, $102e12.l
208078: 6606                     bne     208080

20807A: 117C 0040 0314           move.b  #$40, ($314,A0)

208080: 117C 000C 0315           move.b  #$c, ($315,A0)
208086: 317C 7000 0316           move.w  #$7000, ($316,A0)
20808C: 5247                     addq.w  #1, D7

20808E: 0839 0001 0010 2E15      btst    #$1, $102e15.l	; service mode
208096: 6714                     beq     2080ac

208098: 117C 0058 0318           move.b  #$58, ($318,A0)
20809E: 117C 0006 0319           move.b  #$6, ($319,A0)
2080A4: 317C 6800 031A           move.w  #$6800, ($31a,A0)
2080AA: 5247                     addq.w  #1, D7

2080AC: 3147 030A                move.w  D7, ($30a,A0)
2080B0: 4CDF 7FFF                movem.l (A7)+, D0-D7/A0-A6
2080B4: 4E73                     rte

2080B6: C747
*/
		}
		break;


		case 0x0001:
		{
//			int param1 = mcu_ram[shogwarr_mcu_command_offset + 1];
			int param2 = mcu_ram[shogwarr_mcu_command_offset + 2];

			// clear old command (handshake to main cpu)
			mcu_ram[shogwarr_mcu_command_offset] = 0x0000;

			// execute the command:

			// param1 ?
			mcu_ram[param2/2 + 0] = 0x0000;		// ?
			mcu_ram[param2/2 + 1] = 0x0000;		// ?
			mcu_ram[param2/2 + 2] = 0x0000;		// ?
			mcu_ram[param2/2 + 3] = 0x0000;		// ? addr.l
			mcu_ram[param2/2 + 4] = 0x00e0;		// 0000e0: 4e73 rte

		}
		break;


		case 0x0002:
		{
//			int param1 = mcu_ram[shogwarr_mcu_command_offset + 1];
//			int param2 = mcu_ram[shogwarr_mcu_command_offset + 2];
//			int param3 = mcu_ram[shogwarr_mcu_command_offset + 3];
//			int param4 = mcu_ram[shogwarr_mcu_command_offset + 4];
//			int param5 = mcu_ram[shogwarr_mcu_command_offset + 5];
//			int param6 = mcu_ram[shogwarr_mcu_command_offset + 6];
//			int param7 = mcu_ram[shogwarr_mcu_command_offset + 7];

			// clear old command (handshake to main cpu)
			mcu_ram[shogwarr_mcu_command_offset] = 0x0000;

			// execute the command:

		}
		break;

	}

}



WRITE16_HANDLER( shogwarr_mcu_ram_w )
{
	COMBINE_DATA(&mcu_ram[offset]);
	shogwarr_mcu_run();
}



#define SHOGWARR_MCU_COM_W(_n_) \
WRITE16_HANDLER( shogwarr_mcu_com##_n_##_w ) \
{ \
	shogwarr_mcu_status |= (1 << _n_); \
	shogwarr_mcu_run(); \
}

SHOGWARR_MCU_COM_W(0)
SHOGWARR_MCU_COM_W(1)
SHOGWARR_MCU_COM_W(2)
SHOGWARR_MCU_COM_W(3)


/***************************************************************************


						Misc Machine Emulation Routines


***************************************************************************/

READ16_HANDLER( kaneko16_rnd_r )
{
	return mame_rand() & 0xffff;
}

WRITE16_HANDLER( kaneko16_coin_lockout_w )
{
	if (ACCESSING_MSB)
	{
		coin_counter_w(0,   data  & 0x0100);
		coin_counter_w(1,   data  & 0x0200);
		coin_lockout_w(0, (~data) & 0x0400 );
		coin_lockout_w(1, (~data) & 0x0800 );
	}
}

/* Sand Scorpion */

static UINT8 sprite_irq;
static UINT8 unknown_irq;
static UINT8 vblank_irq;


/* Update the IRQ state based on all possible causes */
static void update_irq_state(void)
{
	if (vblank_irq || sprite_irq || unknown_irq)
		cpu_set_irq_line(0, 1, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 1, CLEAR_LINE);
}


/* Called once/frame to generate the VBLANK interrupt */
static INTERRUPT_GEN( sandscrp_interrupt )
{
	vblank_irq = 1;
	update_irq_state();
}


static VIDEO_EOF( sandscrp )
{
	sprite_irq = 1;
	update_irq_state();
}

/* Reads the cause of the interrupt */
static READ16_HANDLER( sandscrp_irq_cause_r )
{
	return 	( sprite_irq  ?  0x08  : 0 ) |
			( unknown_irq ?  0x10  : 0 ) |
			( vblank_irq  ?  0x20  : 0 ) ;
}


/* Clear the cause of the interrupt */
static WRITE16_HANDLER( sandscrp_irq_cause_w )
{
	if (ACCESSING_LSB)
	{
		kaneko16_sprite_flipx	=	data & 1;
		kaneko16_sprite_flipy	=	data & 1;

		if (data & 0x08)	sprite_irq  = 0;
		if (data & 0x10)	unknown_irq = 0;
		if (data & 0x20)	vblank_irq  = 0;
	}

	update_irq_state();
}


/***************************************************************************


									Sound


***************************************************************************/

WRITE16_HANDLER( kaneko16_soundlatch_w )
{
	if (ACCESSING_MSB)
	{
		soundlatch_w(0, (data & 0xff00) >> 8 );
		cpu_set_nmi_line(1,PULSE_LINE);
	}
}

/* Two identically mapped YM2149 chips */

READ16_HANDLER( kaneko16_YM2149_0_r )
{
	/* Each 2149 register is mapped to a different address */
	AY8910_control_port_0_w(0,offset);
	return AY8910_read_port_0_r(0);
}
READ16_HANDLER( kaneko16_YM2149_1_r )
{
	/* Each 2149 register is mapped to a different address */
	AY8910_control_port_1_w(0,offset);
	return AY8910_read_port_1_r(0);
}

WRITE16_HANDLER( kaneko16_YM2149_0_w )
{
	/* Each 2149 register is mapped to a different address */
	AY8910_control_port_0_w(0,offset);
	/* The registers are mapped to odd addresses, except one! */
	if (ACCESSING_LSB)	AY8910_write_port_0_w(0, data       & 0xff);
	else				AY8910_write_port_0_w(0,(data >> 8) & 0xff);
}
WRITE16_HANDLER( kaneko16_YM2149_1_w )
{
	/* Each 2149 register is mapped to a different address */
	AY8910_control_port_1_w(0,offset);
	/* The registers are mapped to odd addresses, except one! */
	if (ACCESSING_LSB)	AY8910_write_port_1_w(0, data       & 0xff);
	else				AY8910_write_port_1_w(0,(data >> 8) & 0xff);
}


/***************************************************************************


									EEPROM


***************************************************************************/

READ_HANDLER( kaneko16_eeprom_r )
{
	return EEPROM_read_bit() & 1;
}

WRITE_HANDLER( kaneko16_eeprom_reset_w )
{
	// reset line asserted: reset.
	EEPROM_set_cs_line((data & 0x01) ? CLEAR_LINE : ASSERT_LINE );
}

WRITE16_HANDLER( kaneko16_eeprom_w )
{
	if (ACCESSING_LSB)
	{
		// latch the bit
		EEPROM_write_bit(data & 0x02);

		// reset line asserted: reset.
//		EEPROM_set_cs_line((data & 0x00) ? CLEAR_LINE : ASSERT_LINE );

		// clock line asserted: write latch or select next bit to read
		EEPROM_set_clock_line((data & 0x01) ? ASSERT_LINE : CLEAR_LINE );
	}
}


/***************************************************************************


							Memory Maps - Main CPU


***************************************************************************/

/***************************************************************************
								The Berlin Wall
***************************************************************************/

static MEMORY_READ16_START( berlwall_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM					},	// ROM
	{ 0x200000, 0x20ffff, MRA16_RAM					},	// Work RAM
	{ 0x30e000, 0x30ffff, MRA16_RAM					},	// Sprites
	{ 0x400000, 0x400fff, MRA16_RAM					},	// Palette
//	{ 0x480000, 0x480001, MRA16_RAM					},	// ?
	{ 0x500000, 0x500001, kaneko16_bg15_reg_r		},	// High Color Background
	{ 0x580000, 0x580001, kaneko16_bg15_select_r	},
	{ 0x600000, 0x60003f, MRA16_RAM					},	// Sprites Regs
	{ 0x680000, 0x680001, input_port_0_word_r		},	// Inputs
	{ 0x680002, 0x680003, input_port_1_word_r		},
	{ 0x680004, 0x680005, input_port_2_word_r		},
//	{ 0x680006, 0x680007, input_port_3_word_r		},
	{ 0x780000, 0x780001, watchdog_reset16_r		},	// Watchdog
	{ 0x800000, 0x80001f, kaneko16_YM2149_0_r		},	// Sound
	{ 0x800200, 0x80021f, kaneko16_YM2149_1_r		},
	{ 0x800400, 0x800401, OKIM6295_status_0_lsb_r	},
	{ 0xc00000, 0xc03fff, MRA16_RAM					},	// Layers
	{ 0xd00000, 0xd0001f, MRA16_RAM					},	// Layers Regs
MEMORY_END

static MEMORY_WRITE16_START( berlwall_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM											},	// ROM
	{ 0x200000, 0x20ffff, MWA16_RAM											},	// Work RAM
	{ 0x30e000, 0x30ffff, MWA16_RAM, &spriteram16, &spriteram_size			},	// Sprites
	{ 0x400000, 0x400fff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
//	{ 0x480000, 0x480001, MWA16_RAM											},	// ?
	{ 0x500000, 0x500001, kaneko16_bg15_reg_w,     &kaneko16_bg15_reg		},	// High Color Background
	{ 0x580000, 0x580001, kaneko16_bg15_select_w,  &kaneko16_bg15_select	},
	{ 0x600000, 0x60003f, kaneko16_sprites_regs_w, &kaneko16_sprites_regs	},	// Sprites Regs
	{ 0x700000, 0x700001, kaneko16_coin_lockout_w							},	// Coin Lockout
	{ 0x800000, 0x80001f, kaneko16_YM2149_0_w								},	// Sound
	{ 0x800200, 0x80021f, kaneko16_YM2149_1_w								},
	{ 0x800400, 0x800401, OKIM6295_data_0_lsb_w								},
	{ 0xc00000, 0xc00fff, kaneko16_vram_1_w, &kaneko16_vram_1				},	// Layers
	{ 0xc01000, 0xc01fff, kaneko16_vram_0_w, &kaneko16_vram_0				},	//
	{ 0xc02000, 0xc02fff, MWA16_RAM, &kaneko16_vscroll_1,					},	//
	{ 0xc03000, 0xc03fff, MWA16_RAM, &kaneko16_vscroll_0,					},	//
	{ 0xd00000, 0xd0001f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers Regs
MEMORY_END


/***************************************************************************
							Bakuretsu Breaker
***************************************************************************/

static MEMORY_READ16_START( bakubrkr_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA16_RAM					},	// Work RAM
	{ 0x400000, 0x40001f, kaneko16_YM2149_0_r		},	// Sound
	{ 0x400200, 0x40021f, kaneko16_YM2149_1_r		},	//
	{ 0x400400, 0x400401, OKIM6295_status_0_lsb_r	},	//
	{ 0x500000, 0x503fff, MRA16_RAM					},	// Layers 1
	{ 0x580000, 0x583fff, MRA16_RAM					},	// Layers 0
	{ 0x600000, 0x601fff, MRA16_RAM					},	// Sprites
	{ 0x700000, 0x700fff, MRA16_RAM					},	// Palette
	{ 0x800000, 0x80001f, MRA16_RAM					},	// Layers 0 Regs
	{ 0x900000, 0x90001f, MRA16_RAM					},	// Sprites Regs
	{ 0xa80000, 0xa80001, watchdog_reset16_r		},	// Watchdog
	{ 0xb00000, 0xb0001f, MRA16_RAM					},	// Layers 1 Regs
	{ 0xe00000, 0xe00001, input_port_0_word_r		},	// Inputs
	{ 0xe00002, 0xe00003, input_port_1_word_r		},
	{ 0xe00004, 0xe00005, input_port_2_word_r		},
	{ 0xe00006, 0xe00007, input_port_3_word_r		},
MEMORY_END

static MEMORY_WRITE16_START( bakubrkr_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM											},	// ROM
	{ 0x100000, 0x10ffff, MWA16_RAM											},	// Work RAM
	{ 0x400000, 0x40001f, kaneko16_YM2149_0_w								},	// Sound
	{ 0x400200, 0x40021f, kaneko16_YM2149_1_w								},	//
	{ 0x400400, 0x400401, OKIM6295_data_0_lsb_w	},	//
	{ 0x500000, 0x500fff, kaneko16_vram_1_w, &kaneko16_vram_1				},	// Layers 0
	{ 0x501000, 0x501fff, kaneko16_vram_0_w, &kaneko16_vram_0				},	//
	{ 0x502000, 0x502fff, MWA16_RAM, &kaneko16_vscroll_1,					},	//
	{ 0x503000, 0x503fff, MWA16_RAM, &kaneko16_vscroll_0,					},	//
	{ 0x580000, 0x580fff, kaneko16_vram_3_w, &kaneko16_vram_3				},	// Layers 1
	{ 0x581000, 0x581fff, kaneko16_vram_2_w, &kaneko16_vram_2				},	//
	{ 0x582000, 0x582fff, MWA16_RAM, &kaneko16_vscroll_3,					},	//
	{ 0x583000, 0x583fff, MWA16_RAM, &kaneko16_vscroll_2,					},	//
	{ 0x600000, 0x601fff, MWA16_RAM, &spriteram16, &spriteram_size			},	// Sprites
	{ 0x700000, 0x700fff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0x900000, 0x90001f, kaneko16_sprites_regs_w,  &kaneko16_sprites_regs	},	// Sprites Regs
	{ 0x800000, 0x80001f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers 1 Regs
	{ 0xb00000, 0xb0001f, kaneko16_layers_1_regs_w, &kaneko16_layers_1_regs	},	// Layers 0 Regs
	{ 0xd00000, 0xd00001, kaneko16_eeprom_w									},	// EEPROM
MEMORY_END


/***************************************************************************
									Blaze On
***************************************************************************/

static MEMORY_READ16_START( blazeon_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM				},	// ROM
	{ 0x300000, 0x30ffff, MRA16_RAM				},	// Work RAM
	{ 0x500000, 0x500fff, MRA16_RAM				},	// Palette
	{ 0x600000, 0x603fff, MRA16_RAM				},	// Layers 0
	{ 0x700000, 0x700fff, MRA16_RAM				},	// Sprites
/**/{ 0x800000, 0x80001f, MRA16_RAM				},	// Layers 0 Regs
/**/{ 0x900000, 0x90001f, MRA16_RAM				},	// Sprites Regs #1
/**/{ 0x980000, 0x98001f, MRA16_RAM				},	// Sprites Regs #2
	{ 0xc00000, 0xc00001, input_port_0_word_r	},	// Inputs
	{ 0xc00002, 0xc00003, input_port_1_word_r	},
	{ 0xc00004, 0xc00005, input_port_2_word_r	},
	{ 0xc00006, 0xc00007, input_port_3_word_r	},
	{ 0xe00000, 0xe00001, MRA16_NOP				},	// IRQ Ack ?
	{ 0xe40000, 0xe40001, MRA16_NOP				},	// IRQ Ack ?
//	{ 0xe80000, 0xe80001, MRA16_NOP				},	// IRQ Ack ?
	{ 0xec0000, 0xec0001, MRA16_NOP				},	// Lev 4 IRQ Ack ?
MEMORY_END

static MEMORY_WRITE16_START( blazeon_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM											},	// ROM
	{ 0x300000, 0x30ffff, MWA16_RAM											},	// Work RAM
	{ 0x500000, 0x500fff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0x600000, 0x600fff, kaneko16_vram_1_w, &kaneko16_vram_1				},	// Layers 0
	{ 0x601000, 0x601fff, kaneko16_vram_0_w, &kaneko16_vram_0				},	//
	{ 0x602000, 0x602fff, MWA16_RAM, &kaneko16_vscroll_1,					},	//
	{ 0x603000, 0x603fff, MWA16_RAM, &kaneko16_vscroll_0,					},	//
	{ 0x700000, 0x700fff, MWA16_RAM, &spriteram16,	&spriteram_size			},	// Sprites
	{ 0x800000, 0x80001f, kaneko16_layers_0_regs_w,	&kaneko16_layers_0_regs	},	// Layers 1 Regs
	{ 0x900000, 0x90001f, kaneko16_sprites_regs_w,	&kaneko16_sprites_regs	},	// Sprites Regs #1
	{ 0x980000, 0x98001f, MWA16_RAM											},	// Sprites Regs #2
	{ 0xd00000, 0xd00001, kaneko16_coin_lockout_w							},	// Coin Lockout
	{ 0xe00000, 0xe00001, kaneko16_soundlatch_w								},
MEMORY_END


/***************************************************************************
								Blood Warrior
***************************************************************************/

static WRITE16_HANDLER( bloodwar_oki_0_bank_w )
{
	if (ACCESSING_LSB)
	{
		OKIM6295_set_bank_base(0, 0x40000 * (data & 0x3) );
//		logerror("CPU #0 PC %06X : OKI0  bank %08X\n",activecpu_get_pc(),data);
	}
}

static WRITE16_HANDLER( bloodwar_oki_1_bank_w )
{
	if (ACCESSING_LSB)
	{
		OKIM6295_set_bank_base(1, 0x40000 * (data & 0x3) );
//		logerror("CPU #0 PC %06X : OKI1  bank %08X\n",activecpu_get_pc(),data);
	}
}

static WRITE16_HANDLER( bloodwar_coin_lockout_w )
{
	if (ACCESSING_MSB)
	{
		coin_counter_w(0, data & 0x0100);

		coin_lockout_w(0, data & 0x8000 );
		coin_lockout_w(1, data & 0x8000 );
	}
}

static MEMORY_READ16_START( bloodwar_readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA16_RAM					},	// Work RAM
	{ 0x200000, 0x20ffff, MRA16_RAM					},	// Shared With MCU
	{ 0x300000, 0x30ffff, MRA16_RAM					},	// Palette
	{ 0x400000, 0x401fff, MRA16_RAM					},	// Sprites
	{ 0x500000, 0x503fff, MRA16_RAM					},	// Layers 0
	{ 0x580000, 0x583fff, MRA16_RAM					},	// Layers 1
	{ 0x600000, 0x60000f, MRA16_RAM					},	// Layers 0 Regs
	{ 0x680000, 0x68000f, MRA16_RAM					},	// Layers 1 Regs
	{ 0x700000, 0x70001f, MRA16_RAM					},	// Sprites Regs
	{ 0x800000, 0x800001, OKIM6295_status_0_lsb_r	},
	{ 0x880000, 0x880001, OKIM6295_status_1_lsb_r	},
	{ 0x900014, 0x900015, kaneko16_rnd_r			},	// Random Number ?
	{ 0xa00000, 0xa00001, watchdog_reset16_r		},	// Watchdog
	{ 0xb00000, 0xb00001, input_port_0_word_r		},	// Inputs
	{ 0xb00002, 0xb00003, input_port_1_word_r		},	//
	{ 0xb00004, 0xb00005, input_port_2_word_r		},	//
	{ 0xb00006, 0xb00007, input_port_3_word_r		},	//
	{ 0xd00000, 0xd00001, MRA16_NOP					},	// ? (bit 0)
MEMORY_END

static MEMORY_WRITE16_START( bloodwar_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM											},	// ROM
	{ 0x100000, 0x10ffff, MWA16_RAM											},	// Work RAM
	{ 0x200000, 0x20ffff, MWA16_RAM, &mcu_ram								},	// Shared With MCU
	{ 0x2a0000, 0x2a0001, bloodwar_mcu_com0_w								},	// To MCU ?
	{ 0x2b0000, 0x2b0001, bloodwar_mcu_com1_w								},
	{ 0x2c0000, 0x2c0001, bloodwar_mcu_com2_w								},
	{ 0x2d0000, 0x2d0001, bloodwar_mcu_com3_w								},
	{ 0x300000, 0x30ffff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0x400000, 0x401fff, MWA16_RAM, &spriteram16, &spriteram_size			},	// Sprites

	{ 0x580000, 0x580fff, kaneko16_vram_1_w, &kaneko16_vram_1				},	// Layers 0
	{ 0x581000, 0x581fff, kaneko16_vram_0_w, &kaneko16_vram_0				},	//
	{ 0x582000, 0x582fff, MWA16_RAM, &kaneko16_vscroll_1,					},	//
	{ 0x583000, 0x583fff, MWA16_RAM, &kaneko16_vscroll_0,					},	//

	{ 0x500000, 0x500fff, kaneko16_vram_3_w, &kaneko16_vram_3				},	// Layers 1
	{ 0x501000, 0x501fff, kaneko16_vram_2_w, &kaneko16_vram_2				},	//
	{ 0x502000, 0x502fff, MWA16_RAM, &kaneko16_vscroll_3,					},	//
	{ 0x503000, 0x503fff, MWA16_RAM, &kaneko16_vscroll_2,					},	//

	{ 0x680000, 0x68001f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers 0 Regs
	{ 0x600000, 0x60001f, kaneko16_layers_1_regs_w, &kaneko16_layers_1_regs	},	// Layers 1 Regs

	{ 0x700000, 0x70001f, kaneko16_sprites_regs_w,  &kaneko16_sprites_regs	},	// Sprites Regs
	{ 0x800000, 0x800001, OKIM6295_data_0_lsb_w								},
	{ 0x880000, 0x880001, OKIM6295_data_1_lsb_w								},
	{ 0xa00000, 0xa00001, watchdog_reset16_w				},	// Watchdog
	{ 0xb80000, 0xb80001, bloodwar_coin_lockout_w			},	// Coin Lockout
	{ 0xc00000, 0xc00001, MWA16_NOP							},	// ?
	{ 0xe00000, 0xe00001, bloodwar_oki_0_bank_w				},
	{ 0xe80000, 0xe80001, bloodwar_oki_1_bank_w				},
MEMORY_END


/***************************************************************************
							Great 1000 Miles Rally
***************************************************************************/


READ16_HANDLER( gtmr_wheel_r )
{
	if ( (readinputport(4) & 0x1800) == 0x10)	// DSW setting
		return	readinputport(5)<<8;			// 360° Wheel
	else
		return	readinputport(5);				// 270° Wheel
}

static int bank0;
WRITE16_HANDLER( gtmr_oki_0_bank_w )
{
	if (ACCESSING_LSB)
	{
		OKIM6295_set_bank_base(0, 0x10000 * (data & 0xF) );
		bank0 = (data & 0xF);
//		logerror("CPU #0 PC %06X : OKI0 bank %08X\n",activecpu_get_pc(),data);
	}
}

WRITE16_HANDLER( gtmr_oki_1_bank_w )
{
	if (ACCESSING_LSB)
	{
		OKIM6295_set_bank_base(1, 0x40000 * (data & 0x1) );
//		logerror("CPU #0 PC %06X : OKI1 bank %08X\n",activecpu_get_pc(),data);
	}
}

/*
	If you look at the samples ROM for the OKI chip #0, you'll see
	it's divided into 16 chunks, each chunk starting with the header
	holding the samples	addresses. But, except for chunk 0, the first
	$100 bytes ($20 samples) of each chunk are empty, and despite that,
	samples in the range $0-1f are played. So, whenever a samples in
	this range is requested, we use the address and sample from chunk 0,
	otherwise we use those from the selected bank. By using this scheme
	the sound improves, but I wouldn't bet it's correct..
*/

WRITE16_HANDLER( gtmr_oki_0_data_w )
{
	static int pend = 0;

	if (ACCESSING_LSB)
	{

		if (pend)	pend = 0;
		else
		{
			if (data & 0x80)
			{
				int samp = data &0x7f;

				pend = 1;
				if (samp < 0x20)
				{
					OKIM6295_set_bank_base(0, 0);
//					logerror("Setting OKI0 bank to zero\n");
				}
				else
					OKIM6295_set_bank_base(0, 0x10000 * bank0 );
			}
		}

		OKIM6295_data_0_w(0,data);
//		logerror("CPU #0 PC %06X : OKI0 <- %08X\n",activecpu_get_pc(),data);

	}

}

WRITE16_HANDLER( gtmr_oki_1_data_w )
{
	if (ACCESSING_LSB)
	{
		OKIM6295_data_1_w(0,data);
//		logerror("CPU #0 PC %06X : OKI1 <- %08X\n",activecpu_get_pc(),data);
	}
}


static MEMORY_READ16_START( gtmr_readmem )
	{ 0x000000, 0x0ffffd, MRA16_ROM					},	// ROM
	{ 0x0ffffe, 0x0fffff, gtmr_wheel_r				},	// Wheel Value
	{ 0x100000, 0x10ffff, MRA16_RAM					},	// Work RAM
	{ 0x200000, 0x20ffff, MRA16_RAM					},	// Shared With MCU
	{ 0x300000, 0x30ffff, MRA16_RAM					},	// Palette
	{ 0x310000, 0x327fff, MRA16_RAM					},	//
	{ 0x400000, 0x401fff, MRA16_RAM					},	// Sprites
	{ 0x500000, 0x503fff, MRA16_RAM					},	// Layers 0
	{ 0x580000, 0x583fff, MRA16_RAM					},	// Layers 1
	{ 0x600000, 0x60000f, MRA16_RAM					},	// Layers 0 Regs
	{ 0x680000, 0x68000f, MRA16_RAM					},	// Layers 1 Regs
	{ 0x700000, 0x70001f, kaneko16_sprites_regs_r	},	// Sprites Regs
	{ 0x800000, 0x800001, OKIM6295_status_0_lsb_r	},	// Samples
	{ 0x880000, 0x880001, OKIM6295_status_1_lsb_r	},
	{ 0x900014, 0x900015, kaneko16_rnd_r			},	// Random Number ?
	{ 0xa00000, 0xa00001, watchdog_reset16_r		},	// Watchdog
	{ 0xb00000, 0xb00001, input_port_0_word_r		},	// Inputs
	{ 0xb00002, 0xb00003, input_port_1_word_r		},
	{ 0xb00004, 0xb00005, input_port_2_word_r		},
	{ 0xb00006, 0xb00007, input_port_3_word_r		},
	{ 0xd00000, 0xd00001, MRA16_NOP					},	// ? (bit 0)
MEMORY_END

static MEMORY_WRITE16_START( gtmr_writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MWA16_RAM					},	// Work RAM
	{ 0x200000, 0x20ffff, MWA16_RAM, &mcu_ram		},	// Shared With MCU
	{ 0x2a0000, 0x2a0001, gtmr_mcu_com0_w			},	// To MCU ?
	{ 0x2b0000, 0x2b0001, gtmr_mcu_com1_w			},
	{ 0x2c0000, 0x2c0001, gtmr_mcu_com2_w			},
	{ 0x2d0000, 0x2d0001, gtmr_mcu_com3_w			},
	{ 0x300000, 0x30ffff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0x310000, 0x327fff, MWA16_RAM												},	//
	{ 0x400000, 0x401fff, MWA16_RAM, &spriteram16, &spriteram_size			},	// Sprites
	{ 0x500000, 0x500fff, kaneko16_vram_1_w, &kaneko16_vram_1				},	// Layers 0
	{ 0x501000, 0x501fff, kaneko16_vram_0_w, &kaneko16_vram_0				},	//
	{ 0x502000, 0x502fff, MWA16_RAM, &kaneko16_vscroll_1,					},	//
	{ 0x503000, 0x503fff, MWA16_RAM, &kaneko16_vscroll_0,					},	//
	{ 0x580000, 0x580fff, kaneko16_vram_3_w, &kaneko16_vram_3				},	// Layers 1
	{ 0x581000, 0x581fff, kaneko16_vram_2_w, &kaneko16_vram_2				},	//
	{ 0x582000, 0x582fff, MWA16_RAM, &kaneko16_vscroll_3,					},	//
	{ 0x583000, 0x583fff, MWA16_RAM, &kaneko16_vscroll_2,					},	//
	{ 0x600000, 0x60000f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers 0 Regs
	{ 0x680000, 0x68000f, kaneko16_layers_1_regs_w, &kaneko16_layers_1_regs	},	// Layers 1 Regs
	{ 0x700000, 0x70001f, kaneko16_sprites_regs_w, &kaneko16_sprites_regs	},	// Sprites Regs
	{ 0x800000, 0x800001, gtmr_oki_0_data_w			},	// Samples
	{ 0x880000, 0x880001, gtmr_oki_1_data_w			},
	{ 0xa00000, 0xa00001, watchdog_reset16_w		},	// Watchdog
	{ 0xb80000, 0xb80001, kaneko16_coin_lockout_w	},	// Coin Lockout
//	{ 0xc00000, 0xc00001, MWA16_NOP					},	// ?
	{ 0xe00000, 0xe00001, gtmr_oki_0_bank_w			},	// Samples Bankswitching
	{ 0xe80000, 0xe80001, gtmr_oki_1_bank_w			},
MEMORY_END


/***************************************************************************
							Great 1000 Miles Rally 2
***************************************************************************/


READ16_HANDLER( gtmr2_wheel_r )
{
	switch (readinputport(4) & 0x1800)
	{
		case 0x0000:	// 270° A. Wheel
			return	(readinputport(5));
			break;
		case 0x1000:	// 270° D. Wheel
			return	(readinputport(6) << 8);
			break;
		case 0x0800:	// 360° Wheel
			return	(readinputport(7) << 8);
			break;
		default:
			logerror("gtmr2_wheel_r : read at %06x with joystick\n", activecpu_get_pc());
			return	(~0);
			break;
	}
}

READ16_HANDLER( gtmr2_IN1_r )
{
	return	(readinputport(1) & (readinputport(8) | ~0x7100));
}

static MEMORY_READ16_START( gtmr2_readmem )
	{ 0x000000, 0x0ffffd, MRA16_ROM					},	// ROM
	{ 0x0ffffe, 0x0fffff, gtmr2_wheel_r				},	// Wheel Value
	{ 0x100000, 0x10ffff, MRA16_RAM					},	// Work RAM
	{ 0x200000, 0x20ffff, MRA16_RAM					},	// Shared With MCU
	{ 0x300000, 0x30ffff, MRA16_RAM					},	// Palette
	{ 0x310000, 0x327fff, MRA16_RAM					},	//
	{ 0x400000, 0x401fff, MRA16_RAM					},	// Sprites
	{ 0x500000, 0x503fff, MRA16_RAM					},	// Layers 0
	{ 0x580000, 0x583fff, MRA16_RAM					},	// Layers 1
	{ 0x600000, 0x60000f, MRA16_RAM					},	// Layers 0 Regs
	{ 0x680000, 0x68000f, MRA16_RAM					},	// Layers 1 Regs
	{ 0x700000, 0x70001f, kaneko16_sprites_regs_r	},	// Sprites Regs
	{ 0x800000, 0x800001, OKIM6295_status_0_lsb_r	},	// Samples
	{ 0x880000, 0x880001, OKIM6295_status_1_lsb_r	},
	{ 0x900014, 0x900015, kaneko16_rnd_r			},	// Random Number ?
	{ 0xa00000, 0xa00001, watchdog_reset16_r		},	// Watchdog
	{ 0xb00000, 0xb00001, input_port_0_word_r		},	// Inputs
//	{ 0xb00002, 0xb00003, input_port_1_word_r		},
	{ 0xb00002, 0xb00003, gtmr2_IN1_r		},
	{ 0xb00004, 0xb00005, input_port_2_word_r		},
	{ 0xb00006, 0xb00007, input_port_3_word_r		},
	{ 0xd00000, 0xd00001, MRA16_NOP					},	// ? (bit 0)
MEMORY_END

/***************************************************************************
								Magical Crystal
***************************************************************************/

static MEMORY_READ16_START( mgcrystl_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM					},	// ROM
	{ 0x300000, 0x30ffff, MRA16_RAM					},	// Work RAM
	{ 0x400000, 0x40001f, kaneko16_YM2149_0_r		},	// Sound
	{ 0x400200, 0x40021f, kaneko16_YM2149_1_r		},
	{ 0x400400, 0x400401, OKIM6295_status_0_lsb_r	},
	{ 0x500000, 0x500fff, MRA16_RAM					},	// Palette
	{ 0x600000, 0x603fff, MRA16_RAM					},	// Layers 0
	{ 0x680000, 0x683fff, MRA16_RAM					},	// Layers 1
	{ 0x700000, 0x701fff, MRA16_RAM					},	// Sprites
	{ 0x800000, 0x80000f, MRA16_RAM					},	// Layers 0 Regs
	{ 0x900000, 0x90001f, MRA16_RAM					},	// Sprites Regs
	{ 0xb00000, 0xb0000f, MRA16_RAM					},	// Layers 1 Regs
	{ 0xa00000, 0xa00001, watchdog_reset16_r		},	// Watchdog
	{ 0xc00000, 0xc00001, input_port_0_word_r		},	// Inputs
	{ 0xc00002, 0xc00003, input_port_1_word_r		},	//
	{ 0xc00004, 0xc00005, input_port_2_word_r		},	//
MEMORY_END

static MEMORY_WRITE16_START( mgcrystl_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM											},	// ROM
	{ 0x300000, 0x30ffff, MWA16_RAM											},	// Work RAM
	{ 0x400000, 0x40001f, kaneko16_YM2149_0_w								},	// Sound
	{ 0x400200, 0x40021f, kaneko16_YM2149_1_w								},
	{ 0x400400, 0x400401, OKIM6295_data_0_lsb_w								},
	{ 0x500000, 0x500fff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0x600000, 0x600fff, kaneko16_vram_1_w, &kaneko16_vram_1				},	// Layers 0
	{ 0x601000, 0x601fff, kaneko16_vram_0_w, &kaneko16_vram_0				},	//
	{ 0x602000, 0x602fff, MWA16_RAM, &kaneko16_vscroll_1,					},	//
	{ 0x603000, 0x603fff, MWA16_RAM, &kaneko16_vscroll_0,					},	//
	{ 0x680000, 0x680fff, kaneko16_vram_3_w, &kaneko16_vram_3				},	// Layers 1
	{ 0x681000, 0x681fff, kaneko16_vram_2_w, &kaneko16_vram_2				},	//
	{ 0x682000, 0x682fff, MWA16_RAM, &kaneko16_vscroll_3,					},	//
	{ 0x683000, 0x683fff, MWA16_RAM, &kaneko16_vscroll_2,					},	//
	{ 0x700000, 0x701fff, MWA16_RAM, &spriteram16, &spriteram_size			},	// Sprites
	{ 0x800000, 0x80001f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers 0 Regs
	{ 0x900000, 0x90001f, kaneko16_sprites_regs_w,  &kaneko16_sprites_regs	},	// Sprites Regs
	{ 0xb00000, 0xb0001f, kaneko16_layers_1_regs_w, &kaneko16_layers_1_regs	},	// Layers 1 Regs
	{ 0xd00000, 0xd00001, kaneko16_eeprom_w									},	// EEPROM
MEMORY_END


/***************************************************************************
								Sand Scorpion
***************************************************************************/

WRITE16_HANDLER( sandscrp_coin_counter_w )
{
	if (ACCESSING_LSB)
	{
		coin_counter_w(0,   data  & 0x0001);
		coin_counter_w(1,   data  & 0x0002);
	}
}

static data8_t latch1_full;
static data8_t latch2_full;

static READ16_HANDLER( sandscrp_latchstatus_word_r )
{
	return	(latch1_full ? 0x80 : 0) |
			(latch2_full ? 0x40 : 0) ;
}

static WRITE16_HANDLER( sandscrp_latchstatus_word_w )
{
	if (ACCESSING_LSB)
	{
		latch1_full = data & 0x80;
		latch2_full = data & 0x40;
	}
}

static READ16_HANDLER( sandscrp_soundlatch_word_r )
{
	latch2_full = 0;
	return soundlatch2_r(0);
}

static WRITE16_HANDLER( sandscrp_soundlatch_word_w )
{
	if (ACCESSING_LSB)
	{
		latch1_full = 1;
		soundlatch_w(0, data & 0xff);
		cpu_set_nmi_line(1,PULSE_LINE);
		cpu_spinuntil_time(TIME_IN_USEC(100));	// Allow the other cpu to reply
	}
}

static MEMORY_READ16_START( sandscrp_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM						},	// ROM
	{ 0x700000, 0x70ffff, MRA16_RAM						},	// RAM
	{ 0x200000, 0x20001f, sandscrp_mcu_ram_r			},	// Protection
	{ 0x300000, 0x30000f, MRA16_RAM						},	// Scroll
	{ 0x400000, 0x401fff, MRA16_RAM						},	// Layers
	{ 0x402000, 0x403fff, MRA16_RAM						},	//
	{ 0x500000, 0x501fff, MRA16_RAM						},	// Sprites
	{ 0x600000, 0x600fff, MRA16_RAM						},	// Palette
	{ 0xb00000, 0xb00001, input_port_0_word_r			},	// Inputs
	{ 0xb00002, 0xb00003, input_port_1_word_r			},	//
	{ 0xb00004, 0xb00005, input_port_2_word_r			},	//
	{ 0xb00006, 0xb00007, input_port_3_word_r			},	//
	{ 0xec0000, 0xec0001, watchdog_reset16_r			},	//
	{ 0x800000, 0x800001, sandscrp_irq_cause_r			},	// IRQ Cause
	{ 0xe00000, 0xe00001, sandscrp_soundlatch_word_r	},	// From Sound CPU
	{ 0xe40000, 0xe40001, sandscrp_latchstatus_word_r	},	//
MEMORY_END

static MEMORY_WRITE16_START( sandscrp_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM								},	// ROM
	{ 0x200000, 0x20001f, sandscrp_mcu_ram_w, &mcu_ram			},	// Protection
	{ 0x700000, 0x70ffff, MWA16_RAM								},	// RAM
	{ 0x300000, 0x30000f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers 0 Regs
	{ 0x100000, 0x100001, sandscrp_irq_cause_w					},	// IRQ Ack
	{ 0x400000, 0x400fff, kaneko16_vram_1_w, &kaneko16_vram_1	},	// Layers 0
	{ 0x401000, 0x401fff, kaneko16_vram_0_w, &kaneko16_vram_0	},	//
	{ 0x402000, 0x402fff, MWA16_RAM, &kaneko16_vscroll_1,		},	//
	{ 0x403000, 0x403fff, MWA16_RAM, &kaneko16_vscroll_0,		},	//
	{ 0x500000, 0x501fff, MWA16_RAM, &spriteram16, &spriteram_size	},	// Sprites
	{ 0x600000, 0x600fff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0xa00000, 0xa00001, sandscrp_coin_counter_w				},	// Coin Counters (Lockout unused)
	{ 0xe00000, 0xe00001, sandscrp_soundlatch_word_w			},	// To Sound CPU
	{ 0xe40000, 0xe40001, sandscrp_latchstatus_word_w			},	//
MEMORY_END


/***************************************************************************
								Shogun Warriors
***************************************************************************/

/* Untested */
WRITE16_HANDLER( shogwarr_oki_bank_w )
{
	if (ACCESSING_LSB)
	{
		OKIM6295_set_bank_base(0, 0x10000 * ((data >> 0) & 0x3) );
		OKIM6295_set_bank_base(1, 0x10000 * ((data >> 4) & 0x3) );
	}
}

static MEMORY_READ16_START( shogwarr_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM					},	// ROM
	{ 0x100000, 0x10ffff, MRA16_RAM					},	// Work RAM
	{ 0x200000, 0x20ffff, MRA16_RAM					},	// Shared With MCU
	{ 0x380000, 0x380fff, MRA16_RAM					},	// Palette
	{ 0x400000, 0x400001, OKIM6295_status_0_lsb_r	},	// Samples
	{ 0x480000, 0x480001, OKIM6295_status_1_lsb_r	},
	{ 0x580000, 0x581fff, MRA16_RAM					},	// Sprites
	{ 0x600000, 0x603fff, MRA16_RAM					},	// Layers 0
	{ 0x800000, 0x80000f, MRA16_RAM					},	// Layers 0 Regs
	{ 0x900000, 0x90001f, MRA16_RAM					},	// Sprites Regs
	{ 0xa00014, 0xa00015, kaneko16_rnd_r			},	// Random Number ?
	{ 0xa80000, 0xa80001, watchdog_reset16_r		},	// Watchdog
	{ 0xb80000, 0xb80001, input_port_0_word_r		},	// Inputs
	{ 0xb80002, 0xb80003, input_port_1_word_r		},
	{ 0xb80004, 0xb80005, input_port_2_word_r		},
	{ 0xb80006, 0xb80007, input_port_3_word_r		},
	{ 0xd00000, 0xd00001, MRA16_NOP					},	// ? (bit 0)
MEMORY_END

static MEMORY_WRITE16_START( shogwarr_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM								},	// ROM
	{ 0x100000, 0x10ffff, MWA16_RAM								},	// Work RAM
	{ 0x200000, 0x20ffff, shogwarr_mcu_ram_w, &mcu_ram			},	// Shared With MCU
	{ 0x280000, 0x280001, shogwarr_mcu_com0_w					},	// To MCU ?
	{ 0x290000, 0x290001, shogwarr_mcu_com1_w					},
	{ 0x2b0000, 0x2b0001, shogwarr_mcu_com2_w					},
	{ 0x2d0000, 0x2d0001, shogwarr_mcu_com3_w					},
	{ 0x380000, 0x380fff, paletteram16_xGGGGGRRRRRBBBBB_word_w, &paletteram16	},	// Palette
	{ 0x400000, 0x400001, OKIM6295_data_0_lsb_w					},	// Samples
	{ 0x480000, 0x480001, OKIM6295_data_1_lsb_w					},
	{ 0x580000, 0x581fff, MWA16_RAM, &spriteram16, &spriteram_size	},	// Sprites
	{ 0x600000, 0x600fff, kaneko16_vram_1_w, &kaneko16_vram_1		},	// Layers 0
	{ 0x601000, 0x601fff, kaneko16_vram_0_w, &kaneko16_vram_0		},	//
	{ 0x602000, 0x602fff, MWA16_RAM, &kaneko16_vscroll_1,			},	//
	{ 0x603000, 0x603fff, MWA16_RAM, &kaneko16_vscroll_0,			},	//
	{ 0x800000, 0x80000f, kaneko16_layers_0_regs_w, &kaneko16_layers_0_regs	},	// Layers 1 Regs
	{ 0x900000, 0x90001f, kaneko16_sprites_regs_w, &kaneko16_sprites_regs		},	// Sprites Regs
	{ 0xa80000, 0xa80001, watchdog_reset16_w					},	// Watchdog
	{ 0xd00000, 0xd00001, MWA16_NOP								},	// ?
	{ 0xe00000, 0xe00001, shogwarr_oki_bank_w					},	// Samples Bankswitching
MEMORY_END



/***************************************************************************


							Memory Maps - Sound CPU


***************************************************************************/

/***************************************************************************
									Blaze On
***************************************************************************/

#if 0
static WRITE_HANDLER( blazeon_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bank = data & 7;
	cpu_setbank(15, &RAM[bank * 0x10000 + 0x1000]);
}
#endif

static MEMORY_READ_START( blazeon_sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0xc000, 0xdfff, MRA_RAM					},	// RAM
MEMORY_END
static MEMORY_WRITE_START( blazeon_sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0xc000, 0xdfff, MWA_RAM					},	// RAM
MEMORY_END

static PORT_READ_START( blazeon_sound_readport )
	{ 0x03, 0x03, YM2151_status_port_0_r	},
	{ 0x06, 0x06, soundlatch_r				},
PORT_END
static PORT_WRITE_START( blazeon_sound_writeport )
	{ 0x02, 0x02, YM2151_register_port_0_w	},
	{ 0x03, 0x03, YM2151_data_port_0_w		},
PORT_END


/***************************************************************************
								Sand Scorpion
***************************************************************************/

WRITE_HANDLER( sandscrp_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bank = data & 0x07;

	if ( bank != data )	logerror("CPU #1 - PC %04X: Bank %02X\n",activecpu_get_pc(),data);

	if (bank < 3)	RAM = &RAM[0x4000 * bank];
	else			RAM = &RAM[0x4000 * (bank-3) + 0x10000];

	cpu_setbank(1, RAM);
}

static READ_HANDLER( sandscrp_latchstatus_r )
{
	return	(latch2_full ? 0x80 : 0) |	// swapped!?
			(latch1_full ? 0x40 : 0) ;
}

static READ_HANDLER( sandscrp_soundlatch_r )
{
	latch1_full = 0;
	return soundlatch_r(0);
}

static WRITE_HANDLER( sandscrp_soundlatch_w )
{
	latch2_full = 1;
	soundlatch2_w(0,data);
}

static MEMORY_READ_START( sandscrp_sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1					},	// Banked ROM
	{ 0xc000, 0xdfff, MRA_RAM					},	// RAM
MEMORY_END
static MEMORY_WRITE_START( sandscrp_sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM					},	// Banked ROM
	{ 0xc000, 0xdfff, MWA_RAM					},	// RAM
MEMORY_END

static PORT_READ_START( sandscrp_sound_readport )
	{ 0x02, 0x02, YM2203_status_port_0_r		},	// YM2203
	{ 0x03, 0x03, YM2203_read_port_0_r			},	// PORTA/B read
	{ 0x07, 0x07, sandscrp_soundlatch_r			},	//
	{ 0x08, 0x08, sandscrp_latchstatus_r		},	//
PORT_END
static PORT_WRITE_START( sandscrp_sound_writeport )
	{ 0x00, 0x00, sandscrp_bankswitch_w		},	// ROM Bank
	{ 0x02, 0x02, YM2203_control_port_0_w	},	// YM2203
	{ 0x03, 0x03, YM2203_write_port_0_w		},	//
	{ 0x04, 0x04, OKIM6295_data_0_w			},	// OKIM6295
	{ 0x06, 0x06, sandscrp_soundlatch_w		},	//
PORT_END




/***************************************************************************


								Input Ports


***************************************************************************/

/***************************************************************************
							Bakuretsu Breaker
***************************************************************************/

INPUT_PORTS_START( bakubrkr )
	PORT_START	// IN0 - Player 1 + DSW - e00000.w
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_SERVICE( 0x0002, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0004, 0x0004, "Unknown 1-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0008, 0x0008, "Unknown 1-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0010, 0x0010, "Unknown 1-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown 1-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown 1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0080, 0x0080, "Unknown 1-7" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2 - e00002.b
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - Coins - e00004.b
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT		)	// pause
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN	)

	PORT_START	// IN3 - Seems unused ! - e00006.b
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


/***************************************************************************
							The Berlin Wall (set 1)
***************************************************************************/

INPUT_PORTS_START( berlwall )
	PORT_START	// IN0 - Player 1 - 680000.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2 - 680002.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - Coins - 680004.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT		)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN	)

	PORT_START	// IN3 - ? - 680006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW 1 - $200018.b <- ! $80001d.b
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Reserved" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START	// IN5 - DSW 2 - $200019.b <- $80001f.b
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy"    )
	PORT_DIPSETTING(    0x03, "Normal"  )
	PORT_DIPSETTING(    0x01, "Hard"    )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )	// 1p lives at 202982.b
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, "Country"   )
	PORT_DIPSETTING(    0x30, "England" )
	PORT_DIPSETTING(    0x20, "Italy"   )
	PORT_DIPSETTING(    0x10, "Germany" )
	PORT_DIPSETTING(    0x00, "Freeze Screen" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END


/***************************************************************************
							The Berlin Wall (set 2)
***************************************************************************/

//	Same as berlwall, but for a different lives setting

INPUT_PORTS_START( berlwalt )
	PORT_START	// IN0 - Player 1 - 680000.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2 - 680002.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - Coins - 680004.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT		)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN	)

	PORT_START	// IN3 - ? - 680006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW 1 - $80001d.b
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Reserved" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START	// IN5 - DSW 2 - $80001f.b
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy"    )
	PORT_DIPSETTING(    0x03, "Normal"  )
	PORT_DIPSETTING(    0x01, "Hard"    )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPNAME( 0x30, 0x30, "Country"   )
	PORT_DIPSETTING(    0x30, "England" )
	PORT_DIPSETTING(    0x20, "Italy"   )
	PORT_DIPSETTING(    0x10, "Germany" )
	PORT_DIPSETTING(    0x00, "Freeze Screen" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END


/***************************************************************************
									Blaze On
***************************************************************************/

INPUT_PORTS_START( blazeon )
	PORT_START	// IN0 - Player 1 + DSW - c00000.w
	PORT_DIPNAME( 0x0003, 0x0003, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0002, "Easy"    )
	PORT_DIPSETTING(      0x0003, "Normal"  )
	PORT_DIPSETTING(      0x0001, "Hard"    )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x000c, "3" )
	PORT_DIPSETTING(      0x0008, "4" )
	PORT_DIPSETTING(      0x0004, "5" )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "Unknown 1-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Unknown 1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_START1                       )
	PORT_BIT_IMPULSE( 0x8000, IP_ACTIVE_LOW, IPT_COIN1, 2 )

	PORT_START	// IN1 - Player 2 - c00002.w
	PORT_DIPNAME( 0x000f, 0x000f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0005, "6 Coins/3 Credits" )
	PORT_DIPSETTING(      0x0009, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x000f, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0002, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_3C ) )
//	PORT_DIPSETTING(      0x0001, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x000d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x000b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 1C_6C ) )

	PORT_DIPNAME( 0x00f0, 0x00f0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0070, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0050, "6 Coins/3 Credits" )
	PORT_DIPSETTING(      0x0090, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(      0x00f0, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, "5 Coins/6 Credits" )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_3C ) )
//	PORT_DIPSETTING(      0x0010, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(      0x00e0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x00d0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x00b0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x00a0, DEF_STR( 1C_6C ) )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_START2                       )
	PORT_BIT_IMPULSE( 0x8000, IP_ACTIVE_LOW, IPT_COIN2, 2 )

	PORT_START	// IN2 - ? - c00004.w
	PORT_BIT(  0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )	// unused?

	PORT_START	// IN3 - Other Buttons - c00006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x2000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_TILT  )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_SERVICE1 )
INPUT_PORTS_END

/***************************************************************************
								Blood Warrior
***************************************************************************/

INPUT_PORTS_START( bloodwar )
	PORT_START	// IN0 - Player 2 - b00000.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		|	IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN		|	IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT		|	IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	|	IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1			|	IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2			|	IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3			|	IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_BUTTON4			|	IPF_PLAYER1 )

	PORT_START	// IN1 - Player 2 - b00002.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP		|	IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN		|	IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT		|	IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	|	IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1			|	IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2			|	IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3			|	IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_BUTTON4			|	IPF_PLAYER2 )

	PORT_START	// IN2 - Coins - b00004.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT		)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_SERVICE2	)	// tested

	PORT_START	// IN3 - ? - b00006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )	// tested
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )	// tested
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW from the MCU - $10497e.b <- $208000.b
	PORT_DIPNAME( 0x0100, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0200, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x3800, 0x3800, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x3800, "1 Easy" )
	PORT_DIPSETTING(      0x3000, "2" )
	PORT_DIPSETTING(      0x2800, "3" )
	PORT_DIPSETTING(      0x2000, "4" )
	PORT_DIPSETTING(      0x1800, "5" )
	PORT_DIPSETTING(      0x1000, "6" )
	PORT_DIPSETTING(      0x0800, "7" )
	PORT_DIPSETTING(      0x0000, "8 Hard" )
	PORT_DIPNAME( 0x4000, 0x4000, "Join During Game" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "Allow Continue" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END


/***************************************************************************
							Great 1000 Miles Rally
***************************************************************************/

INPUT_PORTS_START( gtmr )
	PORT_START	// IN0 - Player 1 - b00000.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 ) // swapped for consistency:
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 ) // button1 is usually accel.
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2 - b00002.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) // swapped for consistency:
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) // button1 is usually accel.
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - Coins - b00004.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT		)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN	)

	PORT_START	// IN3 - Seems unused ! - b00006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW from the MCU - 101265.b <- 206000.b
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Cabinet )  )
	PORT_DIPSETTING(      0x0400, DEF_STR( Upright )  )
	PORT_DIPSETTING(      0x0000, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x1800, 0x1800, "Controls"    )
	PORT_DIPSETTING(      0x1800, "1 Joystick"  )
	PORT_DIPSETTING(      0x0800, "2 Joysticks" )
	PORT_DIPSETTING(      0x1000, "Wheel (360)" )
	PORT_DIPSETTING(      0x0000, "Wheel (270)" )
	PORT_DIPNAME( 0x2000, 0x2000, "Use Brake"    )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On )  )
	PORT_DIPNAME( 0xc000, 0xc000, "National Anthem & Flag" )
	PORT_DIPSETTING(      0xc000, "Use Memory"  )
	PORT_DIPSETTING(      0x8000, "Anthem Only" )
	PORT_DIPSETTING(      0x4000, "Flag Only"   )
	PORT_DIPSETTING(      0x0000, "None"        )

	PORT_START	// IN5 - Wheel - 100015.b <- ffffe.b
	PORT_ANALOG ( 0x00ff, 0x0080, IPT_AD_STICK_X | IPF_CENTER, 30, 1, 0x00, 0xff )
INPUT_PORTS_END


/***************************************************************************
							Great 1000 Miles Rally 2
***************************************************************************/

INPUT_PORTS_START( gtmr2 )
	PORT_START	// IN0 - Player 1 - 100004.w <- b00000.w (cpl)
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 ) // swapped for consistency:
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 ) // button1 is usually accel.
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2 - 10000c.w <- b00002.w (cpl) - for "test mode" only
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) // swapped for consistency:
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) // button1 is usually accel.
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - Coins - 100014.w <- b00004.w (cpl)
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)	// only in "test mode"
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT	)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN	)

	PORT_START	// IN3 - 100017.w <- b00006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3, "IN 3-6", IP_KEY_DEFAULT, IP_JOY_NONE )	// Code at 0x002236 - Centers 270D wheel ?
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW from the MCU - 1016f7.b <- 206000.b
	PORT_DIPNAME( 0x0700, 0x0700, "Communication" )
	PORT_DIPSETTING(      0x0700, "None" )
	PORT_DIPSETTING(      0x0600, "Machine 1" )
	PORT_DIPSETTING(      0x0500, "Machine 2" )
	PORT_DIPSETTING(      0x0400, "Machine 3" )
	PORT_DIPSETTING(      0x0300, "Machine 4" )
	/* 0x0000 to 0x0200 : "Machine 4"
	PORT_DIPSETTING(      0x0200, "Machine 4 (0x0200)" )
	PORT_DIPSETTING(      0x0100, "Machine 4 (0x0100)" )
	PORT_DIPSETTING(      0x0000, "Machine 4 (0x0000)" )
	*/
	PORT_DIPNAME( 0x1800, 0x1800, "Controls" )
	PORT_DIPSETTING(      0x1800, "Joystick" )
	PORT_DIPSETTING(      0x0800, "Wheel (360)" )			// Not working correctly in race
	PORT_DIPSETTING(      0x1000, "Wheel (270D)" )			// Not working correctly !
	PORT_DIPSETTING(      0x0000, "Wheel (270A)" )			// Not working correctly in race
	PORT_DIPNAME( 0x2000, 0x2000, "Pedal Function" )
	PORT_DIPSETTING(      0x2000, "Microswitch" )
//	PORT_DIPSETTING(      0x0000, "Potentiometer" )			// Not implemented yet
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On )  )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START	// IN5 - Wheel (270A) - 100019.b <- fffff.b
	PORT_ANALOG ( 0x00ff, 0x0080, IPT_AD_STICK_X | IPF_CENTER, 30, 1, 0x00, 0xff )

	PORT_START	// IN6 - Wheel (270D) - 100019.b <- ffffe.b
	PORT_ANALOG ( 0x00ff, 0x0080, IPT_AD_STICK_X, 30, 1, 0x00, 0xff )

	PORT_START	// IN7 - Wheel (360) - 100019.b <- ffffe.b
	PORT_ANALOGX( 0x00ff, 0x0080, IPT_DIAL, 30, 1, 0, 0, KEYCODE_LEFT, KEYCODE_RIGHT, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START	// Fake IN1 - To be pressed during boot sequence - Code at 0x000c9e
	PORT_BITX( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON5, "IN 1-0", KEYCODE_H, IP_JOY_NONE )	// "sound test"
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON6, "IN 1-4", KEYCODE_J, IP_JOY_NONE )	// "view tiles"
	PORT_BITX( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON7, "IN 1-5", KEYCODE_K, IP_JOY_NONE )	// "view memory"
	PORT_BITX( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON8, "IN 1-6", KEYCODE_L, IP_JOY_NONE )	// "view sprites ?"
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


/***************************************************************************
								Magical Crystal
***************************************************************************/

INPUT_PORTS_START( mgcrystl )
	PORT_START	// IN0 - Player 1 + DSW - c00000.w
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0002, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )	// TESTED!
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - Player 2 - c00002.b
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - Other Buttons - c00004.b
	PORT_BIT ( 0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT ( 0x0100, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT ( 0x0200, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT ( 0x2000, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT ( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT ( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN  )
INPUT_PORTS_END


/***************************************************************************
								Sand Scorpion
***************************************************************************/

INPUT_PORTS_START( sandscrp )
	PORT_START	// IN0 - $b00000.w
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	| IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - $b00002.w
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	| IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - $b00004.w
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN1    )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_COIN2    )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN3 - $b00006.w
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW 1 read by the Z80 through the sound chip
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, "Bombs" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Easy"    )
	PORT_DIPSETTING(    0x20, "Normal"  )
	PORT_DIPSETTING(    0x10, "Hard"    )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x80, "100K, 300K" )
	PORT_DIPSETTING(    0xc0, "200K, 500K" )
	PORT_DIPSETTING(    0x40, "500K, 1000K" )
	PORT_DIPSETTING(    0x00, "1000K, 3000K" )

	PORT_START	// IN5 - DSW 2 read by the Z80 through the sound chip
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 8C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END


/***************************************************************************
								Shogun Warriors
***************************************************************************/

INPUT_PORTS_START( shogwarr )
	PORT_START	// IN0 - - b80000.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? tested

	PORT_START	// IN1 - - b80002.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? tested

	PORT_START	// IN2 - Coins - b80004.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_START1	)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_START2	)
	PORT_BIT_IMPULSE( 0x0400, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x0800, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BITX( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_TILT		)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_SERVICE1	)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN	)	// ? tested

	PORT_START	// IN3 - ? - b80006.w
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW from the MCU - 102e15.b <- 200059.b
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x38, "1" )	// easy
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x28, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPSETTING(    0x10, "6" )
	PORT_DIPSETTING(    0x08, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0x40, 0x40, "Can Join During Game" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )	//	2 credits		winner vs computer
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )	//	1 credit		game over
	PORT_DIPNAME( 0x80, 0x80, "Special Continue Mode" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



/***************************************************************************


								Graphics Layouts


***************************************************************************/


/*
	16x16x4 made of 4 8x8x4 blocks arrenged like:		 	01
 	(nibbles are swapped for tiles, not for sprites)		23
*/
static struct GfxLayout layout_16x16x4 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP8(8*8*4*0,4),   STEP8(8*8*4*1,4)   },
	{ STEP8(8*8*4*0,8*4), STEP8(8*8*4*2,8*4) },
	16*16*4
};

/*
	16x16x8 made of 4 8x8x8 blocks arrenged like:	01
													23
*/
static struct GfxLayout layout_16x16x8 =
{
	16,16,
	RGN_FRAC(1,1),
	8,
	{ STEP8(0,1) },
	{ STEP8(0,8),   STEP8(8*8*8*1,8)   },
	{ STEP8(0,8*8), STEP8(8*8*8*2,8*8) },
	16*16*8
};

static struct GfxDecodeInfo kaneko16_gfx_1x4bit_1x4bit[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4, 0,			0x40 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4, 0x40 * 16,	0x40 }, // [1] Layers
	{ -1 }
};
static struct GfxDecodeInfo kaneko16_gfx_1x4bit_2x4bit[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4, 0,			0x40 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4, 0x40 * 16,	0x40 }, // [1] Layers
	{ REGION_GFX3, 0, &layout_16x16x4, 0x40 * 16,	0x40 }, // [2] Layers
	{ -1 }
};
static struct GfxDecodeInfo kaneko16_gfx_1x8bit_2x4bit[] =
{
	{ REGION_GFX1, 0, &layout_16x16x8,	0x40 * 256,	0x40 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4,	0,			0x40 }, // [1] Layers
	{ REGION_GFX3, 0, &layout_16x16x4,	0,			0x40 }, // [2] Layers
	{ -1 }
};

/* 16x16x4 tiles (made of four 8x8 tiles) */
static struct GfxLayout layout_16x16x4_2 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP4(8*8*4*0 + 3*4, -4), STEP4(8*8*4*0 + 7*4, -4),
	  STEP4(8*8*4*1 + 3*4, -4), STEP4(8*8*4*1 + 7*4, -4) },
	{ STEP8(8*8*4*0, 8*4),     STEP8(8*8*4*2, 8*4) },
	16*16*4
};
static struct GfxDecodeInfo sandscrp_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4,   0x000, 0x10 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4_2, 0x400, 0x40 }, // [1] Layers
	{ -1 }
};


/***************************************************************************


								Machine Drivers


***************************************************************************/

#define KANEKO16_INTERRUPTS_NUM	3
INTERRUPT_GEN( kaneko16_interrupt )
{
	switch ( cpu_getiloops() )
	{
		case 2:  cpu_set_irq_line(0, 3, HOLD_LINE);	break;
		case 1:  cpu_set_irq_line(0, 4, HOLD_LINE); break;
		case 0:  cpu_set_irq_line(0, 5, HOLD_LINE); break;
	}
}

static struct OKIM6295interface okim6295_intf_8kHz =
{
	1,
	{ 8000 },
	{ REGION_SOUND1 },
	{ 100 }
};
static struct OKIM6295interface okim6295_intf_12kHz =
{
	1,
	{ 12000000/6/165 }, /* 2MHz -> 6295 (mode A) */
	{ REGION_SOUND1 },
	{ 100 }
};
static struct OKIM6295interface okim6295_intf_15kHz =
{
	1,
	{ 12000000/6/132 }, /* 2MHz -> 6295 (mode B) */
	{ REGION_SOUND1 },
	{ 100 }
};
static struct OKIM6295interface okim6295_intf_18kHz =
{
	1,
	{ 12000000/4/165 }, /* 3MHz -> 6295 (mode A) */
	{ REGION_SOUND1 },
	{ 100 }
};
static struct OKIM6295interface okim6295_intf_2x12kHz =
{
	2,
	{ 12000, 12000 },
	{ REGION_SOUND1, REGION_SOUND2 },
	{ 100, 100 }
};

static struct AY8910interface ay8910_intf_2x1MHz_DSW =
{
	2,
	1000000,	/* ? */
	{ MIXER(100,MIXER_PAN_LEFT), MIXER(100,MIXER_PAN_RIGHT) },
	{ input_port_4_r, 0 },	/* input A: DSW 1 */
	{ input_port_5_r, 0 },	/* input B: DSW 2 */
	{ 0, 0 },
	{ 0, 0 }
};
static struct AY8910interface ay8910_intf_2x2MHz_EEPROM =
{
	2,
	2000000,	/* ? */
	{ MIXER(100,MIXER_PAN_LEFT), MIXER(100,MIXER_PAN_RIGHT) },
	{ 0, kaneko16_eeprom_r },		/* inputs  A:  0,EEPROM bit read */
	{ 0, 0 },						/* inputs  B */
	{ 0, 0 },						/* outputs A */
	{ 0, kaneko16_eeprom_reset_w }	/* outputs B:  0,EEPROM reset */
};

static struct YM2151interface ym2151_intf_blazeon =
{
	1,
	4000000,			/* ? */
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ 0 },				/* irq handler */
	{ 0 }				/* port_write */
};


/***************************************************************************
								The Berlin Wall
***************************************************************************/

/*
	Berlwall interrupts:

	1-3]	e8c:
	4]		e54:
	5]		de4:
	6-7]	rte
*/

static MACHINE_DRIVER_START( berlwall )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)	/* MC68000P12 */
	MDRV_CPU_MEMORY(berlwall_readmem,berlwall_writemem)
	MDRV_CPU_VBLANK_INT(kaneko16_interrupt,KANEKO16_INTERRUPTS_NUM)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(berlwall)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK)	// mangled sprites otherwise
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 16, 240-1)
	MDRV_GFXDECODE(kaneko16_gfx_1x4bit_1x4bit)
	MDRV_PALETTE_LENGTH(2048 + 32768)	/* 32768 static colors for the bg */

	MDRV_PALETTE_INIT(berlwall)
	MDRV_VIDEO_START(berlwall)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(AY8910, ay8910_intf_2x1MHz_DSW)
	MDRV_SOUND_ADD(OKIM6295, okim6295_intf_12kHz)
MACHINE_DRIVER_END


/***************************************************************************
							Bakuretsu Breaker
***************************************************************************/

static MACHINE_DRIVER_START( bakubrkr )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)	/* TMP68HC000-12 */
	MDRV_CPU_MEMORY(bakubrkr_readmem,bakubrkr_writemem)
	MDRV_CPU_VBLANK_INT(kaneko16_interrupt,KANEKO16_INTERRUPTS_NUM)

	MDRV_FRAMES_PER_SECOND(59)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(bakubrkr)
	MDRV_NVRAM_HANDLER(93C46)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK)	// mangled sprites otherwise
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 16, 240-1)
	MDRV_GFXDECODE(kaneko16_gfx_1x4bit_2x4bit)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(kaneko16_2xVIEW2)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_intf_2x2MHz_EEPROM)
	MDRV_SOUND_ADD(OKIM6295, okim6295_intf_8kHz)
MACHINE_DRIVER_END


/***************************************************************************
									Blaze On
***************************************************************************/

/*
	Blaze On:
		1]		busy loop
		2]		does nothing
		3]		rte
		4]		drives the game
		5]		== 2
		6-7]	busy loop
*/

static MACHINE_DRIVER_START( blazeon )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,12000000)	/* TMP68HC000-12 */
	MDRV_CPU_MEMORY(blazeon_readmem,blazeon_writemem)
	MDRV_CPU_VBLANK_INT(kaneko16_interrupt,KANEKO16_INTERRUPTS_NUM)

	MDRV_CPU_ADD(Z80,4000000)	/* D780C-2 */
	MDRV_CPU_MEMORY(blazeon_sound_readmem,blazeon_sound_writemem)
	MDRV_CPU_PORTS(blazeon_sound_readport,blazeon_sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(blazeon)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(320, 240)
	MDRV_VISIBLE_AREA(0, 320-1, 0, 240-1 -8)
	MDRV_GFXDECODE(kaneko16_gfx_1x4bit_1x4bit)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(kaneko16_1xVIEW2)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2151, ym2151_intf_blazeon)
MACHINE_DRIVER_END



/***************************************************************************
							Great 1000 Miles Rally
***************************************************************************/

/*
	gtmr interrupts:

	3] 476:			time, input ports, scroll registers
	4] 466->258e:	set sprite ram
	5] 438:			set sprite colors

	VIDEO_UPDATE_AFTER_VBLANK fixes the mangled/wrong colored sprites
*/

static MACHINE_DRIVER_START( gtmr )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("gtmr", M68000, 16000000)	/* ? Most likely a 68000-HC16 */
	MDRV_CPU_MEMORY(gtmr_readmem,gtmr_writemem)
	MDRV_CPU_VBLANK_INT(kaneko16_interrupt,KANEKO16_INTERRUPTS_NUM)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(gtmr)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(320, 240)
	MDRV_VISIBLE_AREA(0, 320-1, 0, 240-1)
	MDRV_GFXDECODE(kaneko16_gfx_1x8bit_2x4bit)
	MDRV_PALETTE_LENGTH(32768)

	MDRV_VIDEO_START(kaneko16_2xVIEW2)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ADD(OKIM6295, okim6295_intf_2x12kHz)
MACHINE_DRIVER_END

/***************************************************************************
								Blood Warrior
***************************************************************************/

static MACHINE_DRIVER_START( bloodwar )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(gtmr)
	MDRV_CPU_MODIFY("gtmr")
	MDRV_CPU_MEMORY(bloodwar_readmem,bloodwar_writemem)

	MDRV_MACHINE_INIT( bloodwar )

MACHINE_DRIVER_END

/***************************************************************************
							Great 1000 Miles Rally 2
***************************************************************************/

static MACHINE_DRIVER_START( gtmr2 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(gtmr)
	MDRV_CPU_MODIFY("gtmr")
	MDRV_CPU_MEMORY(gtmr2_readmem,gtmr_writemem)
MACHINE_DRIVER_END

/***************************************************************************
							Bonks Adventure
***************************************************************************/

static MACHINE_DRIVER_START( bonkadv )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(gtmr)
	MDRV_NVRAM_HANDLER(93C46)
MACHINE_DRIVER_END

/***************************************************************************
								Magical Crystal
***************************************************************************/

static MACHINE_DRIVER_START( mgcrystl )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)
	MDRV_CPU_MEMORY(mgcrystl_readmem,mgcrystl_writemem)
	MDRV_CPU_VBLANK_INT(kaneko16_interrupt,KANEKO16_INTERRUPTS_NUM)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(mgcrystl)
	MDRV_NVRAM_HANDLER(93C46)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 0+16, 256-16-1)
	MDRV_GFXDECODE(kaneko16_gfx_1x4bit_2x4bit)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(kaneko16_2xVIEW2)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_intf_2x2MHz_EEPROM)
	MDRV_SOUND_ADD(OKIM6295, okim6295_intf_18kHz)
MACHINE_DRIVER_END


/***************************************************************************
								Sand Scorpion
***************************************************************************/

/* YM3014B + YM2203C */

static void irq_handler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_intf_sandscrp =
{
	1,
	4000000,	/* ? */
	{ YM2203_VOL(100,100) },
	{ input_port_4_r },	/* Port A Read - DSW 1 */
	{ input_port_5_r },	/* Port B Read - DSW 2 */
	{ 0 },	/* Port A Write */
	{ 0 },	/* Port B Write */
	{ irq_handler },	/* IRQ handler */
};


static MACHINE_DRIVER_START( sandscrp )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,12000000)	/* TMP68HC000N-12 */
	MDRV_CPU_MEMORY(sandscrp_readmem,sandscrp_writemem)
	MDRV_CPU_VBLANK_INT(sandscrp_interrupt,1)

	MDRV_CPU_ADD(Z80,4000000)	/* Z8400AB1, Reads the DSWs: it can't be disabled */
	MDRV_CPU_MEMORY(sandscrp_sound_readmem,sandscrp_sound_writemem)
	MDRV_CPU_PORTS(sandscrp_sound_readport,sandscrp_sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)	// eof callback

	MDRV_MACHINE_INIT(sandscrp)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 0+16, 256-16-1)
	MDRV_GFXDECODE(sandscrp_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(sandscrp_1xVIEW2)
	MDRV_VIDEO_EOF(sandscrp)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(OKIM6295, okim6295_intf_15kHz)
	MDRV_SOUND_ADD(YM2203, ym2203_intf_sandscrp)
MACHINE_DRIVER_END


/***************************************************************************
								Shogun Warriors
***************************************************************************/

/*
	shogwarr interrupts:

	2] 100:	rte
	3] 102:
	4] 136:
		movem.l D0-D7/A0-A6, -(A7)
		movea.l $207808.l, A0	; from mcu?
		jmp     ($4,A0)

	other: busy loop
*/
#define SHOGWARR_INTERRUPTS_NUM	3
INTERRUPT_GEN( shogwarr_interrupt )
{
	switch ( cpu_getiloops() )
	{
		case 2:  cpu_set_irq_line(0, 2, HOLD_LINE); break;
		case 1:  cpu_set_irq_line(0, 3, HOLD_LINE); break;
//		case 0:  cpu_set_irq_line(0, 4, HOLD_LINE); break;
	}
}

static MACHINE_DRIVER_START( shogwarr )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12000000)
	MDRV_CPU_MEMORY(shogwarr_readmem,shogwarr_writemem)
	MDRV_CPU_VBLANK_INT(shogwarr_interrupt,SHOGWARR_INTERRUPTS_NUM)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(shogwarr)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(320, 240)
	MDRV_VISIBLE_AREA(0, 320-1, 0, 240-1)
	MDRV_GFXDECODE(kaneko16_gfx_1x4bit_1x4bit)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(kaneko16_1xVIEW2)
	MDRV_VIDEO_UPDATE(kaneko16)

	/* sound hardware */
	MDRV_SOUND_ADD(OKIM6295, okim6295_intf_2x12kHz)
MACHINE_DRIVER_END


/***************************************************************************


								ROMs Loading


***************************************************************************/

/*
 Sprites and tiles are stored in the ROMs using the same layout. But tiles
 have the even and odd pixels swapped. So we use this function to untangle
 them and have one single gfxlayout for both tiles and sprites.
*/
void kaneko16_unscramble_tiles(int region)
{
	unsigned char *RAM	=	memory_region(region);
	int size			=	memory_region_length(region);
	int i;

	if (RAM == NULL)	return;

	for (i = 0; i < size; i ++)
	{
		RAM[i] = ((RAM[i] & 0xF0)>>4) + ((RAM[i] & 0x0F)<<4);
	}
}

DRIVER_INIT( kaneko16 )
{
	kaneko16_unscramble_tiles(REGION_GFX2);
	kaneko16_unscramble_tiles(REGION_GFX3);
}

DRIVER_INIT( berlwall )
{
	kaneko16_unscramble_tiles(REGION_GFX2);
}


/***************************************************************************

								Bakuretsu Breaker

	USES TOSHIBA 68000 CPU W/TWO YM2149 SOUND

	LOCATION    TYPE
	------------------
	U38         27C040
	U37         "
	U36         27C020
	U19         "
	U18         "

Bakuretsu Breaker
Kaneko, 1992

PCB Layout
----------

ZOOFC-02
|------------------------------------------|
| PAL    TS020.U33                PAL      |
| 6264                            6264     |
| 6264    VIEW2-CHIP              6264     |
| 4464 4464           VIEW2-CHIP  TS010.U4 |
| 4464 4464                                |
| 4464 4464                       699206P  |
|                         5116    (QFP44)  |
|                                          |
|         TS002J.U36 MUX2-CHIP    699205P  |
|VU-002   TS001J.U37      5116    (QFP44) J|
|(QFP160) TS000J.U38                      A|
|                   62256 TS100J.U18      M|
|        6116 6116  62256 TS101J.U19      M|
|        PAL                     PAL      A|
|16MHz   PAL  PAL  TMP68HC000N-12          |
|12MHz        PAL                IU-001    |
|   VU-001                      (QFP44)    |
|   (QFP48)  YM2149      PAL               |
|93C46       YM2149  TS030.U5  M6295       |
|    DSW1(4)                               |
|------------------------------------------|

Notes:
       68000 clock: 12.000MHz
      YM2149 clock: 2.000MHz
       M6295 clock: 2.000MHz, sample rate = clock /132
             VSync: 59Hz
             HSync: 15.68kHz


***************************************************************************/

ROM_START( bakubrkr )
 	ROM_REGION( 0x080000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "ts100j.u18", 0x000000, 0x040000, CRC(8cc0a4fd) SHA1(e7e18b5ea236522a79ba9db8f573ac8f7ade504b) )
	ROM_LOAD16_BYTE( "ts101j.u19", 0x000001, 0x040000, CRC(aea92195) SHA1(e89f964e7e936fd7774f21956eb4ff5c9104837b) )

	ROM_REGION( 0x240000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "ts001j.u37",  0x000000, 0x080000, CRC(70b66e7e) SHA1(307ba27b623f67ee4b4023179870c270bac8ea22) )
	ROM_RELOAD(       0x100000, 0x080000             )
	ROM_LOAD( "ts000j.u38",  0x080000, 0x080000, CRC(a7a94143) SHA1(d811a7597402c161850ddf98cdb00661ea506c7d) )
	ROM_RELOAD(       0x180000, 0x080000             )
	ROM_LOAD( "ts002j.u36",  0x200000, 0x040000, CRC(611271e6) SHA1(811c21822b074fbb4bb809fed29d48bbd51d57a0) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles */
	ROM_LOAD( "ts010.u4",  0x000000, 0x100000, CRC(df935324) SHA1(73b7aff8800a4e88a47ad426190b73dabdfbf142) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles */
	ROM_LOAD( "ts020.u33",  0x000000, 0x100000, CRC(eb58c35d) SHA1(762c5219de6f729a0fc1df90fce09cdf711c2a1e) )

	ROM_REGION( 0x0100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "ts030.u5",  0x000000, 0x100000, CRC(1d68e9d1) SHA1(aaa64a8e8d7cd7f91d2be346fafb9d1f29b40eda) )
ROM_END


/***************************************************************************

								The Berlin Wall

The Berlin Wall, Kaneko 1991, BW-002

----

BW-004 BW-008                    VU-003
BW-005 BW-009                    VU-003
BW-006 BW-00A                    VU-003
BW-007 BW-00B                          6116-90
                                       6116-90
BW-003                           52256  52256
                                 BW101A BW100A
5864
5864                   MUX2      68000
            VIEW2
BW300
BW-002
BW-001                      42101
                            42101
41464 41464      VU-002
41464 41464                      YM2149  IU-004
41464 41464                      YM2149
                           SWB             BW-000  6295
                           SWA


PALs : BW-U47, BW-U48 (backgrounds encryption)

***************************************************************************/

ROM_START( berlwall )
 	ROM_REGION( 0x040000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "bw100a", 0x000000, 0x020000, CRC(e6bcb4eb) SHA1(220b8fddc79230b4f6a8cf33e1035355c485e8d1) )
	ROM_LOAD16_BYTE( "bw101a", 0x000001, 0x020000, CRC(38056fb2) SHA1(48338b9a5ebea872286541a3c45016673c4af76b) )

	ROM_REGION( 0x120000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "bw001",  0x000000, 0x080000, CRC(bc927260) SHA1(44273a8b6a041504d54da4a7897adf23e3e9db10) )
	ROM_LOAD( "bw002",  0x080000, 0x080000, CRC(223f5465) SHA1(6ed077514ab4370a215a4a60c3aecc8b72ed1c97) )
	ROM_LOAD( "bw300",  0x100000, 0x020000, CRC(b258737a) SHA1(b5c8fe44a8dcfc19bccba896bdb73030c5843544) )

	ROM_REGION( 0x080000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "bw003",  0x000000, 0x080000, CRC(fbb4b72d) SHA1(07a0590f18b3bba1843ef6a89a5c214e8e605cc3) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )	/* High Color Background */
	ROM_LOAD16_BYTE( "bw004",  0x000000, 0x080000, CRC(5300c34d) SHA1(ccb12ea05f89ef68bcfe003faced2ffea24c4bf0) )
	ROM_LOAD16_BYTE( "bw008",  0x000001, 0x080000, CRC(9aaf2f2f) SHA1(1352856159e19f07e8e30f9c44b21347103ce024) ) // FIXED BITS (xxxxxxx0)
	ROM_LOAD16_BYTE( "bw005",  0x100000, 0x080000, CRC(16db6d43) SHA1(0158d0278d085487400ad4384b8cc9618503319e) )
	ROM_LOAD16_BYTE( "bw009",  0x100001, 0x080000, CRC(1151a0b0) SHA1(584a0da7eb7f06450f95e76faa20d19f053cb74c) ) // FIXED BITS (xxxxxxx0)
	ROM_LOAD16_BYTE( "bw006",  0x200000, 0x080000, CRC(73a35d1f) SHA1(af919cf858c5923aea45e0d8d91493e6284cb99e) )
	ROM_LOAD16_BYTE( "bw00a",  0x200001, 0x080000, CRC(f447dfc2) SHA1(1254eafea92e8e416deedf21cb01990ffc4f896c) ) // FIXED BITS (xxxxxxx0)
	ROM_LOAD16_BYTE( "bw007",  0x300000, 0x080000, CRC(97f85c87) SHA1(865e076e098c49c96639f62be793f2de24b4926b) )
	ROM_LOAD16_BYTE( "bw00b",  0x300001, 0x080000, CRC(b0a48225) SHA1(de256bb6e2a824114274bff0c6c1234934c31c49) ) // FIXED BITS (xxxxxxx0)

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "bw000",  0x000000, 0x040000, CRC(d8fe869d) SHA1(75e9044c4164ca6db9519fcff8eca6c8a2d8d5d1) )
ROM_END

ROM_START( berlwalt )
 	ROM_REGION( 0x040000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "u23_01.bin", 0x000000, 0x020000, CRC(76b526ce) SHA1(95ba7cccbe88fd695c28b6a7c25a1afd130c1aa6) )
	ROM_LOAD16_BYTE( "u39_01.bin", 0x000001, 0x020000, CRC(78fa7ef2) SHA1(8392de6e307dcd2bf5bcbeb37d578d33246acfcf) )

	ROM_REGION( 0x120000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "bw001",  0x000000, 0x080000, CRC(bc927260) SHA1(44273a8b6a041504d54da4a7897adf23e3e9db10) )
	ROM_LOAD( "bw002",  0x080000, 0x080000, CRC(223f5465) SHA1(6ed077514ab4370a215a4a60c3aecc8b72ed1c97) )
	ROM_LOAD( "bw300",  0x100000, 0x020000, CRC(b258737a) SHA1(b5c8fe44a8dcfc19bccba896bdb73030c5843544) )

	ROM_REGION( 0x080000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "bw003",  0x000000, 0x080000, CRC(fbb4b72d) SHA1(07a0590f18b3bba1843ef6a89a5c214e8e605cc3) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )	/* High Color Background */
	ROM_LOAD16_BYTE( "bw004",  0x000000, 0x080000, CRC(5300c34d) SHA1(ccb12ea05f89ef68bcfe003faced2ffea24c4bf0) )
	ROM_LOAD16_BYTE( "bw008",  0x000001, 0x080000, CRC(9aaf2f2f) SHA1(1352856159e19f07e8e30f9c44b21347103ce024) ) // FIXED BITS (xxxxxxx0)
	ROM_LOAD16_BYTE( "bw005",  0x100000, 0x080000, CRC(16db6d43) SHA1(0158d0278d085487400ad4384b8cc9618503319e) )
	ROM_LOAD16_BYTE( "bw009",  0x100001, 0x080000, CRC(1151a0b0) SHA1(584a0da7eb7f06450f95e76faa20d19f053cb74c) ) // FIXED BITS (xxxxxxx0)
	ROM_LOAD16_BYTE( "bw006",  0x200000, 0x080000, CRC(73a35d1f) SHA1(af919cf858c5923aea45e0d8d91493e6284cb99e) )
	ROM_LOAD16_BYTE( "bw00a",  0x200001, 0x080000, CRC(f447dfc2) SHA1(1254eafea92e8e416deedf21cb01990ffc4f896c) ) // FIXED BITS (xxxxxxx0)
	ROM_LOAD16_BYTE( "bw007",  0x300000, 0x080000, CRC(97f85c87) SHA1(865e076e098c49c96639f62be793f2de24b4926b) )
	ROM_LOAD16_BYTE( "bw00b",  0x300001, 0x080000, CRC(b0a48225) SHA1(de256bb6e2a824114274bff0c6c1234934c31c49) ) // FIXED BITS (xxxxxxx0)

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "bw000",  0x000000, 0x040000, CRC(d8fe869d) SHA1(75e9044c4164ca6db9519fcff8eca6c8a2d8d5d1) )
ROM_END


/***************************************************************************

							Blaze On (Japan version)

CPU:          TMP68HC000-12/D780C-2(Z80)
SOUND:        YM2151
OSC:          13.3330/16.000MHz
CUSTOM:       KANEKO VU-002 x2
              KANEKO 23160-509 VIEW2-CHIP
              KANEKO MUX2-CHIP
              KANEKO HELP1-CHIP

---------------------------------------------------
 filemanes          devices       kind
---------------------------------------------------
 BZ_PRG1.U80        27C020        68000 main prg.
 BZ_PRG2.U81        27C020        68000 main prg.
 3.U45              27C010        Z80 sound prg.
 BZ_BG.U2           57C8200       BG CHR
 BZ_SP1.U20         27C8001       OBJ
 BZ_SP2.U21         27C8001       OBJ
 BZ_SP1.U68 ( == BZ_SP1.U20)
 BZ_SP2.U86 ( == BZ_SP2.U21)

***************************************************************************/

ROM_START( blazeon )
 	ROM_REGION( 0x080000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "bz_prg1.u80", 0x000000, 0x040000, CRC(8409e31d) SHA1(a9dfc299f4b457df190314401aef309adfaf9bae) )
	ROM_LOAD16_BYTE( "bz_prg2.u81", 0x000001, 0x040000, CRC(b8a0a08b) SHA1(5f275b98d3e49a834850b45179d26e8c2f9fd604) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* Z80 Code */
	ROM_LOAD( "3.u45", 0x000000, 0x020000, CRC(52fe4c94) SHA1(896230e4627503292575bbd84edc3cf9cb18b27e) )	// 1xxxxxxxxxxxxxxxx = 0xFF

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "bz_sp1.u20", 0x000000, 0x100000, CRC(0d5809a1) SHA1(e72669f95b050d1967d10a865bab8f3634c9daad) )
	ROM_LOAD( "bz_sp2.u21", 0x100000, 0x100000, CRC(56ead2bd) SHA1(463723f3c533603ce3a95310e9ce12b4e582b52d) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "bz_bg.u2", 0x000000, 0x100000, CRC(fc67f19f) SHA1(f5d9e037a736b0932efbfb48587de08bec93df5d) )
ROM_END

/***************************************************************************

								Blood Warrior

Kaneko 1994

TOP BOARD

CPU - Toshiba TMP68HC000N - 16
SOUND - OKI M6295  x2
QUARTZ OSCILLATORS AT 27.0000MHz, 16.0000MHz, 20.0000MHz and 33.3330MHz
RAM - LC3664 x6, 424260 x2, LH52B256D x6, D42101C x2


PCU11.u11 - 18CV8PC    \
PCU15.u15 - 18CV8PC     | NEAR OKI
Z091P016.u16 - 18CV8PC /
Z091P013.u13 - 18CV8PC \
PCU17.u17 - 18CV8PC     |
PCU14.u14 - 18CV8PC     | NEAR 68000
PCU94.u94 - 18CV8PC     |
PCU92.u92 - 18CV8PC     |
Z092P093.u93 - 18CV8PC /
ALL ABOVE NOT DUMPED

Custom Chips

231609-509 View2-Chip x2
KC-002 L0002 023 9321EK702
9343T - 44 pin PQFP (NEAR JAMMMA CONNECTOR)

BOTTOM BOARD

TBS0P01 452 9339PK001 - MCU

ofs1g006.u6 - GAL16V8B
ofs1g007.u7 - GAL16V8B
ofs1p059.u59 - 18CV8PC
ofs1p511.u511 - 18CV8PC
ALL ABOVE NOT DUMPED

ROMS

9346.u126 - 93C46
ofdox3.U124 - 27C010
ofp1f3.U513 - 27C4000
ofpof3.U514 - 27C4000
of101f0223.U101 - 27C800
of20902011.u17 - 27C040
of210000213.u19 - 27C040
of2110215.u21 - 27C040
of21200217.u23 - 27C040
of21300219.u25 - 27C040
of21400221.u27 - 27C040
of1000222.u99 - 27C800
of0010226.u55 - 27C800
of3000225.u51 - 27C800
of2080209.u28 - 27V160
of2060207.u14 - 27V160
of2070208.u15 - 27V160
of2050206.u13 - 27V160
of2040205.u12 - 27V160
of2030204.u11 - 27V160
of2020203.u10 - 27V160
of2010202.u9 - 27V160
of2000201.u8 - 27V160
of209e0210.u16 - 27C040
of210e0212.u18 - 27C040
of211e0214.u20 - 27C040
of212e0216.u22 - 27C040
of213e0218.u24 - 27C040
of214e0220.u26 - 27C040

***************************************************************************/

ROM_START( bloodwar )
 	ROM_REGION( 0x100000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "pof3.514", 0x000000, 0x080000, CRC(0c93da15) SHA1(65b6b1b4acfc32c551ae4fbe6a13f7f2b8554dbf) )
	ROM_LOAD16_BYTE( "p1f3.513", 0x000001, 0x080000, CRC(894ecbe5) SHA1(bf403d19e6315266114ac742a08cac903e7b54b5) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "dox3.124",  0x000000, 0x020000, CRC(399f2005) SHA1(ff0370724770c35963953fd9596d9f808ba87d8f) )

 	ROM_REGION16_BE( 0x0080, REGION_USER1, 0 )			/* EEPROM */
	ROM_LOAD16_WORD( "9346.126",  0x0000, 0x0080, CRC(1579db94) SHA1(acb842676946efea29b73bdc9ecb266f49d2f5a8) )

	ROM_REGION( 0x1e00000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD       ( "2000201.8",   0x0000000, 0x200000, CRC(bba63025) SHA1(daec5285469ee953f6f838fe3cb3903524e9ac39) )
	ROM_LOAD       ( "2010202.9",   0x0200000, 0x200000, CRC(4ffd9ddc) SHA1(62bc8c0ed2efab407fc2956c514c3e732bcc47ee) )
	ROM_LOAD       ( "2020203.10",  0x0400000, 0x200000, CRC(fbcc5363) SHA1(9eff48c29d5c887d39e4db442c6ee51ec879521e) )
	ROM_LOAD       ( "2030204.11",  0x0600000, 0x200000, CRC(8e818ce9) SHA1(bc37d35247edfc563400cd67459d455b1fea6eab) )
	ROM_LOAD       ( "2040205.12",  0x0800000, 0x200000, CRC(70c4a76b) SHA1(01b17bda156f2e6f480bdc976927c8bba47c1186) )
	ROM_LOAD       ( "2050206.13",  0x0a00000, 0x200000, CRC(80c667bb) SHA1(7edf33c713c8448ff73fa84b9f7684dd4d46eed1) )
	ROM_LOAD       ( "2060207.14",  0x0c00000, 0x200000, CRC(c2028c97) SHA1(ac3b73ff34f84015432ceb22cf9c57ab0ff07a70) )
	ROM_LOAD       ( "2070208.15",  0x0e00000, 0x200000, CRC(b1f30c61) SHA1(2ae010c10b7a2ae09df904f7ea81425e80389622) )
	ROM_LOAD       ( "2080209.28",  0x1000000, 0x200000, CRC(a8f29545) SHA1(5d018147aa71207f679909343104deaa0f08fd9d) )

	ROM_LOAD16_BYTE( "209e0210.16", 0x1800000, 0x080000, CRC(fbdfee1e) SHA1(0d5543e27d15e59c8f51f3458dd5e27b4811aa62) )
	ROM_LOAD16_BYTE( "20902011.17", 0x1800001, 0x080000, CRC(2baefdb2) SHA1(3fffa276051f0ab490cf60e026d2dfdb6e684ef9) )
	ROM_LOAD16_BYTE( "210e0212.18", 0x1900000, 0x080000, CRC(d59cb3c0) SHA1(8833e1fd5b69df7515157eb6c15b812e029bc570) )
	ROM_LOAD16_BYTE( "21000213.19", 0x1900001, 0x080000, CRC(62cb5c8b) SHA1(21e7b7eed074f780ef0d4b85db024a1fe4fd94d3) )
	ROM_LOAD16_BYTE( "211e0214.20", 0x1a00000, 0x080000, CRC(3305015e) SHA1(b002a8b86a4f79f5266413dcc42368171b90a89e) )
	ROM_LOAD16_BYTE( "21100215.21", 0x1a00001, 0x080000, CRC(5e3a13bd) SHA1(fd9017c05e1c71e4980e1db48855392f8b888a64) )
	ROM_LOAD16_BYTE( "212e0216.22", 0x1b00000, 0x080000, CRC(2836073b) SHA1(ccbf4954af86c6ffbdaa122770f64f72d5e23e63) )
	ROM_LOAD16_BYTE( "21200217.23", 0x1b00001, 0x080000, CRC(267b8ee0) SHA1(4e97a970dcd331016c754753913e07e5cd9be3d3) )
	ROM_LOAD16_BYTE( "213e0218.24", 0x1c00000, 0x080000, CRC(7b202f6c) SHA1(7acae3a7c8cfa39f49814addc9aec30f18deff0c) )
	ROM_LOAD16_BYTE( "21300219.25", 0x1c00001, 0x080000, CRC(09f16222) SHA1(5b2789143be84e963ca3b4b1cbab6ab5f9fbcade) )
	ROM_LOAD16_BYTE( "214e0220.26", 0x1d00000, 0x080000, CRC(bfcb86aa) SHA1(601fa1f4a6fc483befbaefbf97cd7072e1b68c8d) )
	ROM_LOAD16_BYTE( "21400221.27", 0x1d00001, 0x080000, CRC(90682117) SHA1(93d2036c17bcb4021fb3edbc9e08595059d6295d) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "3010226.55", 0x000000, 0x100000, CRC(fcf215de) SHA1(83015f10e62b917efd6e3edfbd45fb8f9b35db2b) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "3000225.51", 0x000000, 0x100000, CRC(fbc3c08a) SHA1(0ba52b381e7a10fb1513244b394438b440950af3) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "101f0223.101", 0x000000, 0x100000, CRC(295f3c93) SHA1(558698f1d04b23dd2a73e2eae5ecce598defb228) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "1000222.99",   0x000000, 0x100000, CRC(42b12269) SHA1(f9d9c42057e176710f09e8db0bfcbf603c15ca11) )
ROM_END


/***************************************************************************

							Great 1000 Miles Rally

GMMU2+1	512K * 2	68k
GMMU23	1M		OKI6295: 00000-2ffff + chunks of 0x10000 with headers
GMMU24	1M		OKI6295: chunks of 0x40000 with headers - FIRST AND SECOND HALF IDENTICAL

GMMU27	2M		sprites
GMMU28	2M		sprites
GMMU29	2M		sprites
GMMU30	512k	sprites

GMMU64	1M		sprites - FIRST AND SECOND HALF IDENTICAL
GMMU65	1M		sprites - FIRST AND SECOND HALF IDENTICAL

GMMU52	2M		tiles


---------------------------------------------------------------------------
								Game code
---------------------------------------------------------------------------

100000.b	<- (!b00000.b) & 7f	[1p]
    01.b	previous value of the above
    02.b	bits gone high

100008.b	<- (!b00002.b) & 7f	[2p]

100010.b	<- !b00004.b [coins]
    11.b	previous value of the above
    12.b	bits gone high

100013.b	<- b00006.b	(both never accessed again?)

100015.b	<- wheel value

600000.w	<- 100a20.w + 100a30.w		600002.w	<- 100a22.w + 100a32.w
600004.w	<- 100a24.w + 100a34.w		600006.w	<- 100a26.w + 100a36.w

680000.w	<- 100a28.w + 100a38.w		680002.w	<- 100a2a.w + 100a3a.w
680004.w	<- 100a2c.w + 100a3c.w		680006.w	<- 100a2e.w + 100a3e.w

101265.b	<- DSW (from 206000)
101266		<- Settings from NVRAM (0x80 bytes from 208000)

1034f8.b	credits
103502.b	coins x ..
103503.b	.. credits

1035ec.l	*** Time (BCD: seconds * 10000) ***
103e64.w	*** Speed << 4 ***

10421a.b	bank for the oki mapped at 800000
104216.b	last value of the above

10421c.b	bank for the oki mapped at 880000
104218.b	last value of the above

ROUTINES:

dd6	print string: a2->scr ; a1->string ; d1.l = xpos.w<<6|ypos.w<<6

Trap #2 = 43a0 ; d0.w = routine index ; (where not specified: 43c0):
1:  43C4	2:  43F8	3:  448E	4:  44EE
5:  44D2	6:  4508	7:  453A	10: 0AF6
18: 4580	19: 4604
20> 2128	writes 700000-70001f
21: 21F6
24> 2346	clears 400000-401407 (641*8 = $281*8)
30> 282A	writes 600008/9/b/e-f, 680008/9/b/e-f
31: 295A
32> 2B36	100a30-f <- 100a10-f
34> 2B4C	clears 500000-503fff, 580000-583fff
35> 2B9E	d1.w = selects between:	500000;501000;580000;581000.
			Fill 0x1000 bytes from there with d2.l

70: 2BCE>	11d8a
71: 2BD6
74: 2BDE	90: 3D44
91> 3D4C	wait for bit 0 of d00000 to be 0
92> 3D5C	200010.w<-D1	200012.w<-D2	200014.w<-D3
f1: 10F6

***************************************************************************/

/*	This version displays:

	tb05mm-eu "1000 miglia"
	master up= 94/07/18 15:12:35			*/

ROM_START( gtmr )
 	ROM_REGION( 0x100000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "u2.bin", 0x000000, 0x080000, CRC(031799f7) SHA1(a59a9635002d139247828e3b74f6cf2fbdd5e569) )
	ROM_LOAD16_BYTE( "u1.bin", 0x000001, 0x080000, CRC(6238790a) SHA1(a137fd581138804534f3193068f117611a982004) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "mcu_code.u12",  0x000000, 0x020000, NO_DUMP )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	/* fill the 0x700000-7fffff range first, with the second of the identical halves */
//	ROM_LOAD16_BYTE( "gmmu64.bin",  0x600000, 0x100000, CRC(57d77b33) SHA1(f7ae28ae889be4442b7b236705943eaad1f0c84e) )	// HALVES IDENTICAL
//	ROM_LOAD16_BYTE( "gmmu65.bin",  0x600001, 0x100000, CRC(05b8bdca) SHA1(44471d66787d5b48ae8b13676f42f27af44e5c6a) )	// HALVES IDENTICAL
	ROM_LOAD( "gmmu27.bin",  0x000000, 0x200000, CRC(c0ab3efc) SHA1(e6cd15480977b036234d91e6f3a6e21b7f0a3c3e) )
	ROM_LOAD( "gmmu28.bin",  0x200000, 0x200000, CRC(cf6b23dc) SHA1(ccfd0b17507e091e55c169361cd6a6b19641b717) )
	ROM_LOAD( "gmmu29.bin",  0x400000, 0x200000, CRC(8f27f5d3) SHA1(219a86446ce2556682009d8aff837480f040a01e) )
	ROM_LOAD( "gmmu30.bin",  0x600000, 0x080000, CRC(e9747c8c) SHA1(2507102ec34755c6f110eadb3444e6d3a3474051) )
	/* codes 6800-7fff are explicitly skipped */
	/* wrong tiles: 	gtmr	77e0 ; gtmralt	81c4 81e0 81c4 */
	ROM_LOAD( "sprites",     0x700000, 0x100000, NO_DUMP )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "gmmu52.bin",  0x000000, 0x200000, CRC(b15f6b7f) SHA1(5e84919d788add53fc87f4d85f437df413b1dbc5) )

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "gmmu52.bin",  0x000000, 0x200000, CRC(b15f6b7f) SHA1(5e84919d788add53fc87f4d85f437df413b1dbc5) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "gmmu23.bin",  0x000000, 0x100000, CRC(b9cbfbee) SHA1(051d48a68477ef9c29bd5cc0bb7955d513a0ab94) )	// 16 x $10000

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "gmmu24.bin",  0x000000, 0x100000, CRC(380cdc7c) SHA1(ba7f51201b0f2bf15e66557e45bb2af5cf797779) )	//  2 x $40000 - HALVES IDENTICAL
ROM_END


/*	This version displays:

	tb05mm-eu "1000 miglia"
	master up= 94/09/06 14:49:19			*/

ROM_START( gtmre )
 	ROM_REGION( 0x100000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "gmmu2.bin", 0x000000, 0x080000, CRC(36dc4aa9) SHA1(0aea4dc169d7aad2ea957a1de698d1fa12c71556) )
	ROM_LOAD16_BYTE( "gmmu1.bin", 0x000001, 0x080000, CRC(8653c144) SHA1(a253a01327a9443337a55a13c063ea5096444c4c) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "mcu_code.u12",  0x000000, 0x020000, NO_DUMP )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	/* fill the 0x700000-7fffff range first, with the second of the identical halves */
	ROM_LOAD16_BYTE( "gmmu64.bin",  0x600000, 0x100000, CRC(57d77b33) SHA1(f7ae28ae889be4442b7b236705943eaad1f0c84e) )	// HALVES IDENTICAL
	ROM_LOAD16_BYTE( "gmmu65.bin",  0x600001, 0x100000, CRC(05b8bdca) SHA1(44471d66787d5b48ae8b13676f42f27af44e5c6a) )	// HALVES IDENTICAL
	ROM_LOAD( "gmmu27.bin",  0x000000, 0x200000, CRC(c0ab3efc) SHA1(e6cd15480977b036234d91e6f3a6e21b7f0a3c3e) )
	ROM_LOAD( "gmmu28.bin",  0x200000, 0x200000, CRC(cf6b23dc) SHA1(ccfd0b17507e091e55c169361cd6a6b19641b717) )
	ROM_LOAD( "gmmu29.bin",  0x400000, 0x200000, CRC(8f27f5d3) SHA1(219a86446ce2556682009d8aff837480f040a01e) )
	ROM_LOAD( "gmmu30.bin",  0x600000, 0x080000, CRC(e9747c8c) SHA1(2507102ec34755c6f110eadb3444e6d3a3474051) )
	/* codes 6800-6fff are explicitly skipped */

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "gmmu52.bin",  0x000000, 0x200000, CRC(b15f6b7f) SHA1(5e84919d788add53fc87f4d85f437df413b1dbc5) )

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "gmmu52.bin",  0x000000, 0x200000, CRC(b15f6b7f) SHA1(5e84919d788add53fc87f4d85f437df413b1dbc5) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "gmmu23.bin",  0x000000, 0x100000, CRC(b9cbfbee) SHA1(051d48a68477ef9c29bd5cc0bb7955d513a0ab94) )	// 16 x $10000

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "gmmu24.bin",  0x000000, 0x100000, CRC(380cdc7c) SHA1(ba7f51201b0f2bf15e66557e45bb2af5cf797779) )	//  2 x $40000 - HALVES IDENTICAL
ROM_END


/*	This version displays:

	tb05mm-eu "1000 miglia"
	master up= 94/09/06 20:30:39			*/

ROM_START( gtmrusa )
 	ROM_REGION( 0x100000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "gtmrusa.u2", 0x000000, 0x080000, CRC(5be615c4) SHA1(c14d11a5bf6e025a65b932039165302ff407c4e1) )
	ROM_LOAD16_BYTE( "gtmrusa.u1", 0x000001, 0x080000, CRC(ae853e4e) SHA1(31eaa73b0c5ddab1292f521ceec43b202653efe9) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code? */
	ROM_LOAD( "gtmrusa.u12",  0x000000, 0x020000, CRC(2e1a06ff) SHA1(475a7555653eefac84307492a385895b839cab0d) )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	/* fill the 0x700000-7fffff range first, with the second of the identical halves */
	ROM_LOAD16_BYTE( "gmmu64.bin",  0x600000, 0x100000, CRC(57d77b33) SHA1(f7ae28ae889be4442b7b236705943eaad1f0c84e) )	// HALVES IDENTICAL
	ROM_LOAD16_BYTE( "gmmu65.bin",  0x600001, 0x100000, CRC(05b8bdca) SHA1(44471d66787d5b48ae8b13676f42f27af44e5c6a) )	// HALVES IDENTICAL
	ROM_LOAD( "gmmu27.bin",  0x000000, 0x200000, CRC(c0ab3efc) SHA1(e6cd15480977b036234d91e6f3a6e21b7f0a3c3e) )
	ROM_LOAD( "gmmu28.bin",  0x200000, 0x200000, CRC(cf6b23dc) SHA1(ccfd0b17507e091e55c169361cd6a6b19641b717) )
	ROM_LOAD( "gmmu29.bin",  0x400000, 0x200000, CRC(8f27f5d3) SHA1(219a86446ce2556682009d8aff837480f040a01e) )
	ROM_LOAD( "gmmu30.bin",  0x600000, 0x080000, CRC(e9747c8c) SHA1(2507102ec34755c6f110eadb3444e6d3a3474051) )
	/* codes 6800-6fff are explicitly skipped */

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "gmmu52.bin",  0x000000, 0x200000, CRC(b15f6b7f) SHA1(5e84919d788add53fc87f4d85f437df413b1dbc5) )

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "gmmu52.bin",  0x000000, 0x200000, CRC(b15f6b7f) SHA1(5e84919d788add53fc87f4d85f437df413b1dbc5) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "gmmu23.bin",  0x000000, 0x100000, CRC(b9cbfbee) SHA1(051d48a68477ef9c29bd5cc0bb7955d513a0ab94) )	// 16 x $10000

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "gmmu24.bin",  0x000000, 0x100000, CRC(380cdc7c) SHA1(ba7f51201b0f2bf15e66557e45bb2af5cf797779) )	//  2 x $40000 - HALVES IDENTICAL
ROM_END


/***************************************************************************

						Great 1000 Miles Rally 2

Bootup displays "TB06MM2EX 1000 Miglia 2"
                "Master Up 95-04-04 19:39:01"

Top board
---------
PCB ID : M201F00138  KANEKO AX-SYSTEM BOARD
CPU    : TMP68HC000N-16 (68000)
SOUND  : Oki M6295 (x2)
OSC    : 27.000MHz, 16.000MHz, 20.000MHz, 33.3330MHz
RAM    : LC3664 (28 pin SOIC, x4)
         UT6264 (28 pin SOIC, x2)
         LH628256 (28 pin DIP, x6)
         D42101 (24 pin DIP, x2)
         424260 (40 pin SOIC, x2)
PALs   : AXOP048, AXOP049, AXOP050 (GAL16V8, near M6295's)
         AXOP021 \
         AXOP022  |
         AXOP062  |
         AXOP063  |  (18CV8     )
         AXOP064  |  (near 68000)
         AXOP070  |
         AXOP071  |
         AXOP089 /

OTHER  : Custom chips
                      Kaneko Japan 9448 TA (44 pin PQFP, near JAMMA connector)
                      Kaneko VIEW2-CHIP (x2, 144 pin PQFP)
                      Kaneko KC002 L0002 023 9339EK706 (208 pin PQFP)

ROMs   : None


Bottom board
------------
PCB ID : AX09S00138  KANEKO AX-SYSTEM BOARD ROM-08
DIP    : 8 position (x1)
RAM    : 6116 (x4)
PALs   : COMUX4, COMUX4 (GAL16V8, near U21)
         MMs4P067 (18CV8, near U47)
         MMs6G095 (GAL16V8, near U94)
         MMs4G084 (GAL16V8, near U50)
         COMUX2, COMUX3 (GAL16V8, near U33)
         COMUX1, MMS4P004 (18CV8, near U33)

OTHER  : 93C46 EEPROM
         KANEKO TBSOP02 454 9451MK002 (74 pin PQFP, Custom MCU?)

ROMs   :  (filename is ROM Label, extension is PCB 'u' location)

***************************************************************************/

ROM_START( gtmr2 )
 	ROM_REGION( 0x100000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "m2p0x1.u8",  0x000000, 0x080000, CRC(525f6618) SHA1(da8008cc7768b4e8c0091aa3ea21752d0ca33691) )
	ROM_LOAD16_BYTE( "m2p1x1.u7",  0x000001, 0x080000, CRC(914683e5) SHA1(dbb2140f7de86073647abc6e73ba739ea201dd30) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code? */
	ROM_LOAD( "m2d0x0.u31",        0x000000, 0x020000, CRC(2e1a06ff) SHA1(475a7555653eefac84307492a385895b839cab0d) )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "m2-200-0.u49",      0x000000, 0x400000, CRC(93aafc53) SHA1(1d28b6e3bd61ce9c938fc5303aeabcdefa549852) )
	ROM_LOAD( "m2-201-0.u50",      0x400000, 0x200000, CRC(39b60a83) SHA1(aa7b37c7c92bbcf685f4fec84cc6d8a77d26433c) )
	ROM_LOAD( "m2-202-0.u51",      0x600000, 0x200000, CRC(fd06b339) SHA1(5de0af7d23147f6eb403700eabd66794198f3641) )
	ROM_LOAD16_BYTE( "m2s0x1.u32", 0x700000, 0x080000, CRC(4069d6c7) SHA1(2ed1cbb7ebde8347e0359cd56ee3a0d4d42d551f) )
	ROM_LOAD16_BYTE( "m2s1x1.u33", 0x700001, 0x080000, CRC(c53fe269) SHA1(e6c485bbaea4b67f074b89e047f686f107805713) )

	ROM_REGION( 0x440000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "m2-300-0.u89",      0x000000, 0x200000, CRC(4dc42fbb) SHA1(f14c287bc60f561eb9a57db4e3390aae9a81c392) )
	ROM_LOAD( "m2-301-0.u90",      0x200000, 0x200000, CRC(f4e894f2) SHA1(1f983a1d93845fe298afba60d4dacdd1a10cab7f) )
	ROM_LOAD16_BYTE( "m2b0x0.u93", 0x400000, 0x020000, CRC(e023d51b) SHA1(3c9f591f3ca2ee8e1100b83ae8eb593e11e6eac7) )
	ROM_LOAD16_BYTE( "m2b1x0.u94", 0x400001, 0x020000, CRC(03c48bdb) SHA1(f5ba45d026530d46f760cf06d02a1ffcca89aa3c) )

	ROM_REGION( 0x440000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "m2-300-0.u89",      0x000000, 0x200000, CRC(4dc42fbb) SHA1(f14c287bc60f561eb9a57db4e3390aae9a81c392) )
	ROM_LOAD( "m2-301-0.u90",      0x200000, 0x200000, CRC(f4e894f2) SHA1(1f983a1d93845fe298afba60d4dacdd1a10cab7f) )
	ROM_LOAD16_BYTE( "m2b0x0.u93", 0x400000, 0x020000, CRC(e023d51b) SHA1(3c9f591f3ca2ee8e1100b83ae8eb593e11e6eac7) )
	ROM_LOAD16_BYTE( "m2b1x0.u94", 0x400001, 0x020000, CRC(03c48bdb) SHA1(f5ba45d026530d46f760cf06d02a1ffcca89aa3c) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "m2-100-0.u48",      0x000000, 0x100000, CRC(5250fa45) SHA1(b1ad4660906997faea0aa89866de01a0e9f2b61d) )

	ROM_REGION( 0x080000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "m2w1x0.u47",        0x040000, 0x040000, CRC(1b0513c5) SHA1(8c9ddef19297e1b39d900297005203b7ff28667e) )
ROM_END


/***************************************************************************

								Magical Crystals

(c)1991 Kaneko/Atlus
Z00FC-02

CPU  : TMP68HC000N-12
Sound: YM2149Fx2 M6295
OSC  : 16.0000MHz(X1) 12.0000MHz(X2)

ROMs:
mc100j.u18 - Main programs
mc101j.u19 /

mc000.u38  - Graphics (32pin mask)
mc001.u37  | (32pin mask)
mc002j.u36 / (27c010)

mc010.u04 - Graphics (42pin mask)

mc020.u33 - Graphics (42pin mask)

mc030.u32 - Samples (32pin mask)

PALs (18CV8PC):
u08, u20, u41, u42, u50, u51, u54

Custom chips:
KANEKO VU-001 046A (u53, 48pin PQFP)
KANEKO VU-002 052 151021 (u60, 160pin PQFP)
KANEKO 23160-509 9047EAI VIEW2-CHIP (u24 & u34, 144pin PQFP)
KANEKO MUX2-CHIP (u28, 64pin PQFP)
KANEKO IU-001 9045KP002 (u22, 44pin PQFP)
KANEKO I/O JAMMA MC-8282 047 (u5, 46pin)
699206p (u09, 44pin PQFP)
699205p (u10, 44pin PQFP)

Other:
93C46 EEPROM

DIP settings:
1: Flip screen
2: Test mode
3: Unused
4: Unused

***************************************************************************/

ROM_START( mgcrystl )
 	ROM_REGION( 0x040000*2, REGION_CPU1, ROMREGION_ERASE )			/* 68000 Code */
	ROM_LOAD16_BYTE( "magcrstl.u18", 0x000000, 0x020000, CRC(c7456ba7) SHA1(96c25c3432069373fa86d7af3e093e02e39aea34) )
	ROM_LOAD16_BYTE( "magcrstl.u19", 0x000001, 0x040000, CRC(ea8f9300) SHA1(0cd0d448805aa45986b63befca00b08fe066dbb2) ) //!!

	ROM_REGION( 0x280000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "mc000.u38",    0x000000, 0x100000, CRC(28acf6f4) SHA1(6647ad90ea580b65ed28772f9d65352b06833d0c) )
	ROM_LOAD( "mc001.u37",    0x100000, 0x080000, CRC(005bc43d) SHA1(6f6cd99e8e60562fa86581008455a6d9d646fa95) )
	ROM_RELOAD(               0x180000, 0x080000             )
	ROM_LOAD( "magcrstl.u36", 0x200000, 0x020000, CRC(22729037) SHA1(de4e1bdab57aa617411b6327f3db4856970e8953) )
	ROM_RELOAD(               0x220000, 0x020000             )
	ROM_RELOAD(               0x240000, 0x020000             )
	ROM_RELOAD(               0x260000, 0x020000             )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "mc010.u04",  0x000000, 0x100000, CRC(85072772) SHA1(25e903cc2c893d61db791d1fe60a1205a4395667) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "mc020.u34",  0x000000, 0x100000, CRC(1ea92ff1) SHA1(66ec53e664b2a5a751a280a538aaeceafc187ceb) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "mc030.u32",  0x000000, 0x040000, CRC(c165962e) SHA1(f7e130db387ae9dcb7223f7ad6e51270d3033bc9) )
ROM_END

ROM_START( mgcrystj )
 	ROM_REGION( 0x040000*2, REGION_CPU1, ROMREGION_ERASE )			/* 68000 Code */
	ROM_LOAD16_BYTE( "mc100j.u18", 0x000000, 0x020000, CRC(afe5882d) SHA1(176e6e12e3df63c08d7aff781f5e5a9bd83ec293) )
	ROM_LOAD16_BYTE( "mc101j.u19", 0x000001, 0x040000, CRC(60da5492) SHA1(82b90a617d355825624ce9fb30bddf4714bd0d18) )	//!!

	ROM_REGION( 0x280000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "mc000.u38",  0x000000, 0x100000, CRC(28acf6f4) SHA1(6647ad90ea580b65ed28772f9d65352b06833d0c) )
	ROM_LOAD( "mc001.u37",  0x100000, 0x080000, CRC(005bc43d) SHA1(6f6cd99e8e60562fa86581008455a6d9d646fa95) )
	ROM_RELOAD(             0x180000, 0x080000             )
	ROM_LOAD( "mc002j.u36", 0x200000, 0x020000, CRC(27ac1056) SHA1(34b07c1a0d403ca45c9849d3d8d311012f787df6) )
	ROM_RELOAD(             0x220000, 0x020000             )
	ROM_RELOAD(             0x240000, 0x020000             )
	ROM_RELOAD(             0x260000, 0x020000             )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "mc010.u04",  0x000000, 0x100000, CRC(85072772) SHA1(25e903cc2c893d61db791d1fe60a1205a4395667) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (Scrambled) */
	ROM_LOAD( "mc020.u34",  0x000000, 0x100000, CRC(1ea92ff1) SHA1(66ec53e664b2a5a751a280a538aaeceafc187ceb) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "mc030.u32",  0x000000, 0x040000, CRC(c165962e) SHA1(f7e130db387ae9dcb7223f7ad6e51270d3033bc9) )
ROM_END


/***************************************************************************

								Sand Scorpion

(C) FACE
68HC000N-12
Z8400AB1
OKI6295, YM2203C
OSC:  16.000mhz,   12.000mhz

SANDSC03.BIN     27C040
SANDSC04.BIN     27C040
SANDSC05.BIN     27C040
SANDSC06.BIN     27C040
SANDSC07.BIN     27C2001
SANDSC08.BIN     27C1001
SANDSC11.BIN     27C2001
SANDSC12.BIN     27C2001

***************************************************************************/

ROM_START( sandscrp )
	ROM_REGION( 0x080000, REGION_CPU1, 0 )		/* 68000 Code */
	ROM_LOAD16_BYTE( "sandsc11.bin", 0x000000, 0x040000, CRC(9b24ab40) SHA1(3187422dbe8b15d8053be4cb20e56d3e6afbd5f2) )
	ROM_LOAD16_BYTE( "sandsc12.bin", 0x000001, 0x040000, CRC(ad12caee) SHA1(83267445b89c3cf4dc317106aa68763d2f29eff7) )

	ROM_REGION( 0x24000, REGION_CPU2, 0 )		/* Z80 Code */
	ROM_LOAD( "sandsc08.bin", 0x00000, 0x0c000, CRC(6f3e9db1) SHA1(06a04fa17f44319986913bff70433510c89e38f1) )
	ROM_CONTINUE(             0x10000, 0x14000             )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "sandsc05.bin", 0x000000, 0x080000, CRC(9bb675f6) SHA1(c3f6768cfd99a0e19ca2224fff9aa4e27ec0da24) )
	ROM_LOAD( "sandsc06.bin", 0x080000, 0x080000, CRC(7df2f219) SHA1(e2a59e201bfededa92d6c86f8dc1b212527ef66f) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Layers */
	ROM_LOAD16_BYTE( "sandsc04.bin", 0x000000, 0x080000, CRC(b9222ff2) SHA1(a445da3f7f5dea5ff64bb0b048f624f947875a39) )
	ROM_LOAD16_BYTE( "sandsc03.bin", 0x000001, 0x080000, CRC(adf20fa0) SHA1(67a7a2be774c86916cbb97e4c9b16c2e48125780) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sandsc07.bin", 0x000000, 0x040000, CRC(9870ab12) SHA1(5ea3412cbc57bfaa32a1e2552b2eb46f4ceb5fa8) )
ROM_END


/***************************************************************************

								Shogun Warriors

Shogun Warriors, Kaneko 1992

   fb010.u65           fb040.u33
   fb011.u66
   rb012.u67
   rb013.u68

                         fb001.u43
     68000-12            fb000.u42  m6295
    51257     fb030.u61  fb002.u44  m6295
    51257     fb031.u62  fb003.u45


                fb021a.u3
                fb021b.u4
                fb022a.u5
   fb023.u7     fb022b.u6
   fb020a.u1    fb020b.u2



---------------------------------------------------------------------------
								Game code
---------------------------------------------------------------------------

102e04-7	<- !b80004-7
102e18.w	-> $800000
102e1c.w	-> $800002 , $800006
102e1a.w	-> $800004
102e20.w	-> $800008

ROUTINES:

6622	print ($600000)

***************************************************************************/

ROM_START( shogwarr )
 	ROM_REGION( 0x040000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "fb030a.u61", 0x000000, 0x020000, CRC(a04106c6) SHA1(95ab084f2e709be7cec2964cb09bcf5a8d3aacdf) )
	ROM_LOAD16_BYTE( "fb031a.u62", 0x000001, 0x020000, CRC(d1def5e2) SHA1(f442de4433547e52b483549aca5786e4597a7122) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "fb040a.u33",  0x000000, 0x020000, CRC(4b62c4d9) SHA1(35c943dde70438a411714070e42a84366db5ef83) )

	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "fb020a.u1",  0x000000, 0x080000, CRC(da1b7373) SHA1(89510901848f1798fb76dd82d1cd9ac97c41521d) )
	ROM_LOAD( "fb022a.u5",  0x080000, 0x080000, CRC(60aa1282) SHA1(4648816016e00df3256226ba5134f6e5bb429909) )
	ROM_LOAD( "fb020b.u2",  0x100000, 0x100000, CRC(276b9d7b) SHA1(7a154f65b4737f2b6ac8effa3352711079f571dc) )
	ROM_LOAD( "fb021a.u3",  0x200000, 0x100000, CRC(7da15d37) SHA1(345cf2242e8210a697294a45197f2b3b974de885) )
	ROM_LOAD( "fb021b.u4",  0x300000, 0x100000, CRC(6a512d7b) SHA1(7fc3002d23262a9a590a283ea9e111e38d889ef2) )
	ROM_LOAD( "fb023.u7",   0x400000, 0x100000, CRC(132794bd) SHA1(bcc73c3183c59a4b66f79d04774773b8a9239501) )
	ROM_LOAD( "fb022b.u6",  0x500000, 0x080000, CRC(cd05a5c8) SHA1(9f000cca8d31e19fdc4b38c00c3ed13f71e5541c) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "fb010.u65",  0x000000, 0x100000, CRC(296ffd92) SHA1(183a28e4594c428deb4726ed22d5166592b94b60) )
	ROM_LOAD( "fb011.u66",  0x100000, 0x080000, CRC(500a0367) SHA1(6dc5190f81b21f59ee56a3b2332c8d86d6599782) )	// ?!
	ROM_LOAD( "rb012.u67",  0x200000, 0x100000, CRC(bfdbe0d1) SHA1(3abc5398ee8ee1871b4d081f9b748539d69bcdba) )
	ROM_LOAD( "rb013.u68",  0x300000, 0x100000, CRC(28c37fe8) SHA1(e10dd1a810983077328b44e6e33ce2e899c506d2) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "fb000e.u42",  0x000000, 0x080000, CRC(969f1465) SHA1(4f56d1ad341b08f4db41b7ab2498740612ff7c3d) )	// 2 x $40000
	ROM_LOAD( "fb001e.u43",  0x080000, 0x080000, CRC(f524aaa1) SHA1(006a886f9df2e57c51b61c6cea70a6574fc20304) )	// 2 x $40000

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "fb002.u44",   0x000000, 0x080000, CRC(05d7c2a9) SHA1(e34d395985caec10139a22daa179bb185df157d6) )	// 2 x $40000
	ROM_LOAD( "fb003.u45",   0x080000, 0x080000, CRC(405722e9) SHA1(92e51093d50f74f650ba137f5fc2910e0f85337e) )	// 2 x $40000
ROM_END


DRIVER_INIT( shogwarr )
{
	/* Code patches */
#if 0
	data16_t *RAM = memory_region(REGION_CPU1);
	RAM[0x0039a / 2] = 0x4e71;	// 200000 test
	RAM[0x003e6 / 2] = 0x4e71;	// 20030a test
	RAM[0x223a8 / 2] = 0x6000;	// rom test
#endif

	init_kaneko16();

/*
	ROM test at 2237e:

	the chksum of 00000-03fffd = $657f is added to ($200042).w
	[from shared ram]. The result must be $f463 [=($3fffe).w]

	Now, $f463-$657f = $8ee4 = byte sum of FB040A.U33 !!

	So, there's probably the MCU's code in there, though
	I can't id what kind of CPU should run it :-(
*/
}

/***************************************************************************

								Fujiyama Buster

Japan (c) 1992 Kaneko
This is the Japanese version of Shogun Warriors

Main PCB No: ZO1DK-002 (Same as B.Rap Boys)
 ROM PCB No: ZO5DP
        CPU: TMP68HC000N-12 (Toshiba)
        SND: OKI M6295 x 2
        OSC: 16.000MHz, 12.000MHz
        DIP: 1 x 8 POSITION

OTHER:
93C46 (8 Pin DIP, EEPROM, Linked to FB-040.U33 & CALC3 Chip)
KANEKO JAPAN 9152EV 175101 (160 Pin PQFP)
KANEKO VIEW2-CHIP (144 Pin PQFP)
KANEKO MUX2-CHIP (64 Pin PQFP)
KANEKO CALC3 508 (74 Pin PQFP, MCU, Linked to FB-040.U33)
KANEKO JAPAN 9203 T (44 PIN PQFP)

Differences from Shogun Warriors:

File Name    CRC32       Labelled As   ROM Type
===============================================
fb030j.u61   0x32ce7909  FB030J/U61-00   27C010 | 68000 CPU Code
fb031j.u62   0x000c8c08  FB031J/U62-00   27C010 /

fb040j.u33   0x299d0746  FB040J/U33-00   27C010 - MCU Code? 68000 Code snipets??

fb000j.u43   0xa7522555  FB000J/U43-00   27C040 | Japanese Sound Samples
fb001j_u.101 0x07d4e8e2  FB001J/U101-0   27C040 /

NOTE: U67 & U68 are empty on this Original board.

***************************************************************************/

ROM_START( fjbuster )	// Fujiyama Buster - Japan version of Shogun Warriors
 	ROM_REGION( 0x040000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "fb030j.u61", 0x000000, 0x020000, CRC(32ce7909) SHA1(02d87342706ac9547eb611bd542f8498ba41e34a) )
	ROM_LOAD16_BYTE( "fb031j.u62", 0x000001, 0x020000, CRC(000c8c08) SHA1(439daac1541c34557b5a4308ed69dfebb93abe13) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "fb040j.u33",  0x000000, 0x020000, CRC(299d0746) SHA1(67fe3a47ab01fa02ce2bb5836c2041986c19d875) )

	ROM_REGION( 0x600000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "fb020a.u1",  0x000000, 0x080000, CRC(da1b7373) SHA1(89510901848f1798fb76dd82d1cd9ac97c41521d) )
	ROM_LOAD( "fb022a.u5",  0x080000, 0x080000, CRC(60aa1282) SHA1(4648816016e00df3256226ba5134f6e5bb429909) )
	ROM_LOAD( "fb020b.u2",  0x100000, 0x100000, CRC(276b9d7b) SHA1(7a154f65b4737f2b6ac8effa3352711079f571dc) )
	ROM_LOAD( "fb021a.u3",  0x200000, 0x100000, CRC(7da15d37) SHA1(345cf2242e8210a697294a45197f2b3b974de885) )
	ROM_LOAD( "fb021b.u4",  0x300000, 0x100000, CRC(6a512d7b) SHA1(7fc3002d23262a9a590a283ea9e111e38d889ef2) )
	ROM_LOAD( "fb023.u7",   0x400000, 0x100000, CRC(132794bd) SHA1(bcc73c3183c59a4b66f79d04774773b8a9239501) )
	ROM_LOAD( "fb022b.u6",  0x500000, 0x080000, CRC(cd05a5c8) SHA1(9f000cca8d31e19fdc4b38c00c3ed13f71e5541c) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "fb010.u65",  0x000000, 0x100000, CRC(296ffd92) SHA1(183a28e4594c428deb4726ed22d5166592b94b60) )
	ROM_LOAD( "fb011.u66",  0x100000, 0x080000, CRC(500a0367) SHA1(6dc5190f81b21f59ee56a3b2332c8d86d6599782) )	// ?!
//	ROM_LOAD( "rb012.u67",  0x200000, 0x100000, CRC(bfdbe0d1) SHA1(3abc5398ee8ee1871b4d081f9b748539d69bcdba) )	Not used!  No ROMs here
//	ROM_LOAD( "rb013.u68",  0x300000, 0x100000, CRC(28c37fe8) SHA1(e10dd1a810983077328b44e6e33ce2e899c506d2) )	Not used!  No ROMs here

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "fb000j.u43",    0x000000, 0x080000, CRC(a7522555) SHA1(ea88d90dda20bc309f98a1924c41551e7708e6af) )	// 2 x $40000
	ROM_LOAD( "fb001j_u.101",  0x080000, 0x080000, CRC(07d4e8e2) SHA1(0de911f452ddeb54b0b435b9c1cf5d5881175d44) )	// 2 x $40000

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "fb002.u44",   0x000000, 0x080000, CRC(05d7c2a9) SHA1(e34d395985caec10139a22daa179bb185df157d6) )	// 2 x $40000
	ROM_LOAD( "fb003.u45",   0x080000, 0x080000, CRC(405722e9) SHA1(92e51093d50f74f650ba137f5fc2910e0f85337e) )	// 2 x $40000
ROM_END

DRIVER_INIT( fjbuster )
{
	/* Code patches */
#if 0
/*
	Need to find equivalent for Fujiyama Buster for these patches:

	data16_t *RAM = memory_region(REGION_CPU1);
	RAM[0x0039a / 2] = 0x4e71;	// 200000 test
	RAM[0x003e6 / 2] = 0x4e71;	// 20030a test
	RAM[0x223a8 / 2] = 0x6000;	// rom test
*/
#endif

	init_kaneko16();
}

/***************************************************************************

								B.Rap Boys

B.Rap Boys
Kaneko, 1992

Game is a beat-em up, where a bunch of rapping boys (The B.Rap Boys) beats
up anyone and anything that gets in their way, smashing shop windows
and other property with their fists, chairs, wooden bats and whatever else
they can lay their hands on!


Main PCB No: ZO1DK-002
ROM PCB No:  ZO1DK-EXROM
CPU: TMP68HC000N-12
SND: OKI M6295 x 2
OSC: 16.000MHz, 12.000MHz
DIP: 1 x 8 POSITION
SW1 - PCB location for 2 position DIP but location is unpopulated.
SW2:

					1	2	3	4	5	6	7	8
SCREEN FLIP		NORMAL		OFF
			FLIP		ON
MODE			NORMAL			OFF
			TEST			ON
SWITCH TEST [1]		NO				OFF
			YES				ON
POSITION #4		NOT USED				OFF
COIN TYPE	 	LOCAL COIN					OFF
			COMMON COIN					ON
GAME TYPE		3 PLAYERS	 					OFF
			2 PLAYERS						ON
DIFFICULTY [2]		EASY								ON	OFF
			NORMAL								OFF	OFF
			HARD								OFF	ON
			VERY HARD							ON	ON

[1] This additional test becomes available in test mode when this DIP is ON.
[2] Additional settings available in test mode via another on-screen menu.
Some text is written in Japanese. See scan in archive for details.

Control is via 8 Way Joystick and 2 buttons

There are two extra pin connectors near the JAMMA connector.
Pinouts are....

(A)
10 3P START SW
 9 3P COIN SW
 8 3P BUTTON 2
 7 3P BUTTON 1
 6 3P UP
 5 3P DOWN
 4 3P LEFT
 3 3P RIGHT
 2 GND
 1 GND

(B)
6 COIN COUNTER 3
5 COIN LOCKOUT 3
4 TOTAL COIN COUNTER
3 NC
2 NC
1 NC

RAM:
M5M4464 x 6, M51257AL x 2, KM6264BLS x 2, D42101C x 2, LH5116D x 2, CAT71C256 x 2

OTHER:
93C46 (8 PIN DIP, EEPROM, LINKED TO RB-006.U33)
KANEKO JAPAN 9152EV 175101 (160 PIN PQFP)
KANEKO VIEW2-CHIP (144 PIN PQFP)
KANEKO MUX2-CHIP (64 PIN PQFP)
KANEKO CALC3 508 (74 PIN PQFP, MCU, LINKED TO RB-006.U33)
KANEKO JAPAN 9204 T (44 PIN PQFP)
PALs (x 11, read protected, not dumped)

ROMs:
RB-004.U61	27C010	  \     Main program
RB-005.U62	27C010    /

RB-000.U43	2M mask	  \
RB-001.U44	4M mask	   |    Located near main program and OKI M6295 chips
RB-002.U45	4M mask	   |    Possibly sound related / OKI Samples etc..
RB-003.101	4M mask	  /

RB-006.U33	27C010	  	MCU program? (Linked to CALC3 508)

RB-010.U65	8M mask   \
RB-011.U66	8M mask	   |    GFX
RB-012.U67	8M mask    |
RB-013.U68	8M mask   /

RB-021.U76	4M mask   \
RB-022.U77	4M mask    |
RB-023.U78	4M mask	   |	GFX (located under a plug-in ROM PCB)
RB-024.U79	4M mask   /

RB-020.U2	4M mask   \
RB-025.U4	27C040	   |	GFX (located on a plug-in ROM PCB)
RB-026.U5	27C040    /


-----

Game can be ROM Swapped onto a Shogun Warriors board and works

***************************************************************************/

ROM_START( brapboys )
 	ROM_REGION( 0x040000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "rb-004.u61", 0x000000, 0x020000, CRC(5432442c) SHA1(f0f7328ece96ef25e6d4fd1958d734f64a9ef371) )
	ROM_LOAD16_BYTE( "rb-005.u62", 0x000001, 0x020000, CRC(118b3cfb) SHA1(1690ecf5c629879bd97131ff77029e152919e45d) )

 	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "rb-006.u33",  0x000000, 0x020000, CRC(f1d76b20) SHA1(c571b5f28e529589ee2d7697ef5d4b60ccb66e7a) )

	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	/* order is probably wrong, but until it does more we can't tell */
	ROM_LOAD( "rb-020.u2",  0x000000, 0x080000, CRC(b038440e) SHA1(9e32cb62358ab846470d9a75d4dab771d608a3cf) )
	ROM_LOAD( "rb-025.u4",  0x080000, 0x080000, CRC(aa795ba5) SHA1(c5256dcceded2e76f548b60c18e51d0dd0209d81) )
	ROM_LOAD( "rb-026.u5",  0x100000, 0x080000, CRC(bb7604d4) SHA1(57d51ce4ea2000f9a50bae326cfcb66ec494249f) )

	ROM_LOAD( "rb-021.u76", 0x200000, 0x080000, CRC(b7e2d362) SHA1(7e98e5b3d1ee972fc4cf9bebd33a3ca96a77357c) )
	ROM_LOAD( "rb-022.u77", 0x280000, 0x080000, CRC(8d40c97a) SHA1(353b0a4a508f2fff8eeed680b1f685c7fdc29a7d) ) // right pos. (text)
	ROM_LOAD( "rb-023.u78", 0x300000, 0x080000, CRC(dcf11c8d) SHA1(eed801f7cca3d3a941b1a4e4815cac9d20d970f7) )
	ROM_LOAD( "rb-024.u79", 0x380000, 0x080000, CRC(65fa6447) SHA1(551e540d7bf412753b4a7098e25e6f9d8774bcf4) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "rb-010.u65",  0x000000, 0x100000, CRC(ffd73f87) SHA1(1a661f71976be61c22d9b962850e738ba17f1d45) )
	ROM_LOAD( "rb-011.u66",  0x100000, 0x100000, CRC(d9325f78) SHA1(346832608664aa8f3ac9260a549903386b4125a8) )
	ROM_LOAD( "rb-012.u67",  0x200000, 0x100000, CRC(bfdbe0d1) SHA1(3abc5398ee8ee1871b4d081f9b748539d69bcdba) ) // same as shoggwar
	ROM_LOAD( "rb-013.u68",  0x300000, 0x100000, CRC(28c37fe8) SHA1(e10dd1a810983077328b44e6e33ce2e899c506d2) ) // same as shoggwar

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )	/* Samples */
	/* order is probably wrong, but until it does more we can't tell */
	ROM_LOAD( "rb-003.101",  0x000000, 0x080000, CRC(2cac25d7) SHA1(0412c317bf650a93051b9304d23035efde0c026a) )
	ROM_LOAD( "rb-000.u43",  0x080000, 0x040000, CRC(c7c848ac) SHA1(77754efd6efeac50a40dcd530de504090257d8b3) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )	/* Samples */
	/* order is probably wrong, but until it does more we can't tell */
	ROM_LOAD( "rb-001.u44",   0x000000, 0x080000, CRC(09c779e3) SHA1(c9c9bf939d58d329b87e0aa08ec3d35f7440d3c7) )
	ROM_LOAD( "rb-002.u45",   0x080000, 0x080000, CRC(55de7003) SHA1(cc349fd8671926cafed4d86d1a5cb671e591c408) )
ROM_END

/**********************************************************************

							Bonks Adventure

Bonks Adventure
Kaneko, 1994

PCB Layout
----------

Z09AF-003
|--------------------------------------------------------|
| LA4460  PC604109.101     PAL   3664 3664  PC500105.55  |
|  M6295   PC603108.102    PAL                           |
|  M6295   PC602107.100  PAL      VIEW2-CHIP             |
|          PC601106.99   PAL                             |
|                                           PC400104.51  |
|                                 3664  3664             |
|              62256                                     |
|J  KANEKO     62256              VIEW2-CHIP             |
|A  JAPAN                                                |
|M  9203T                                     424260     |
|M                                                       |
|A   62256    62256   6116                               |
|    PRG.7    PRG.8   6116      KANEKO    424260         |
|                    3364       KC-002    PAL            |
| 62256    68000     3364                                |
| 62256                                                  |
|          PAL      PAL  20MHz                           |
|          PAL      PAL  16MHz   PC600106.42             |
|                   PAL          PC700107.43             |
|        MCU.124                 PC200102.40             |
| DSW(8) KANEKO  93C46           PC100101.37 PC300103.38 |
|        TBSOP01 27MHz 33.3333MHz                        |
|--------------------------------------------------------|

Notes:
      68000 clock: 16.000MHz
      M6295 clock: 2.000MHz, Sample Rate: /165 (both)
      VSync: 60Hz
      HSync: 15.625kHz

      PC100-PC500: 16M MASK
      PC601-PC604: 8M MASK
      PC600-PC700: 27C4001
            PRG's: 27C4001
            MCU  : 27C010

**********************************************************************/

ROM_START( bonkadv )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )			/* 68000 Code */
	ROM_LOAD16_BYTE( "prg.8",        0x000000, 0x080000, CRC(af2e60f8) SHA1(406f79e155d1244b84f8c89c25b37188e1b4f4a6) )
	ROM_LOAD16_BYTE( "prg.7",        0x000001, 0x080000, CRC(a1cc6a78) SHA1(a9cea21a6a0dfd3b0952664681c057190aa27f8c) )

	ROM_REGION( 0x020000, REGION_CPU2, 0 )			/* MCU Code */
	ROM_LOAD( "mcu.124",			 0x000000, 0x020000, CRC(9d4e2724) SHA1(9dd43703265e39f876877020a0ac3875de6faa8d) )

	ROM_REGION( 0x700000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "pc100101.37",		 0x000000, 0x200000, CRC(c96e7c10) SHA1(607cc7745abc3ff820047e8a00060ece61646623) )
	ROM_LOAD( "pc300103.38",		 0x200000, 0x200000, CRC(07692b89) SHA1(ee8175ed90c860bd61ea0f4b7982e6edb4d78bed) )
	ROM_LOAD( "pc200102.40",		 0x400000, 0x200000, CRC(4268a831) SHA1(1d6a23e74802ceded59bed2814f0826bdcb87fb7) )
	ROM_LOAD16_BYTE( "pc600106.42",  0x600000, 0x080000, CRC(25877026) SHA1(96814d97e9f9284f98c35edfe5e76677ac50dd97) )
	ROM_LOAD16_BYTE( "pc700107.43",  0x600001, 0x080000, CRC(bfe21c44) SHA1(9900a6fe4182b720a90d64d368bd0fd08bf936a8) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "pc500105.55",		 0x000000, 0x200000, CRC(edd0da94) SHA1(17a1ad957bb12a07010beec74c3e1b59cc0ab397) )

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )	/* Tiles (scrambled) */
	ROM_LOAD( "pc400104.51",		 0x000000, 0x200000, CRC(3ad3af0f) SHA1(5ac4def8b1a42c13369d3505fa8e1f52f9451cd4) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "pc601106.99",		 0x000000, 0x100000, CRC(a893651c) SHA1(d221ce89f19a76be497724f6c16fab82c8a52661) )
	ROM_LOAD( "pc602107.100",		 0x100000, 0x100000, CRC(0fbb23aa) SHA1(69b620375c65246317d7105fbc414f3c36e02b2c) )

	ROM_REGION( 0x200000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "pc603108.102",		 0x000000, 0x100000, CRC(58458985) SHA1(9a846d604ba901eb2a59d2b6cd9c42e3b43adb6a) )
	ROM_LOAD( "pc604109.101",		 0x100000, 0x100000, CRC(76025530) SHA1(e0c8192d783057798eea084aa3e87938f6e01cb7) )
ROM_END

/***************************************************************************


								Game drivers


***************************************************************************/

/* Working games */

GAME( 1991, berlwall, 0,        berlwall, berlwall, berlwall, ROT0,  "Kaneko", "The Berlin Wall (set 1)" )
GAME( 1991, berlwalt, berlwall, berlwall, berlwalt, berlwall, ROT0,  "Kaneko", "The Berlin Wall (set 2)" )
GAME( 1991, mgcrystl, 0,        mgcrystl, mgcrystl, kaneko16, ROT0,  "Kaneko", "Magical Crystals (World)" )
GAME( 1991, mgcrystj, mgcrystl, mgcrystl, mgcrystl, kaneko16, ROT0,  "Kaneko (Atlus license)", "Magical Crystals (Japan)" )
GAME( 1992, blazeon,  0,        blazeon,  blazeon,  kaneko16, ROT0,  "Atlus",  "Blaze On (Japan)" )
GAME( 1992, sandscrp, 0,        sandscrp, sandscrp, 0,        ROT90, "Face",   "Sand Scorpion" )
GAME( 1994, gtmr,     0,        gtmr,     gtmr,     kaneko16, ROT0,  "Kaneko", "Great 1000 Miles Rally" )
GAME( 1994, gtmre,    gtmr,     gtmr,     gtmr,     kaneko16, ROT0,  "Kaneko", "Great 1000 Miles Rally (Evolution Model)" )
GAME( 1994, gtmrusa,  gtmr,     gtmr,     gtmr,     kaneko16, ROT0,  "Kaneko", "Great 1000 Miles Rally (USA)" )
GAME( 1995, gtmr2,    0,        gtmr2,    gtmr2,    kaneko16, ROT0,  "Kaneko", "Mille Miglia 2: Great 1000 Miles Rally" )

/* Non-working games (mainly due to protection) */

GAMEX(1992, bakubrkr, 0,        bakubrkr, bakubrkr, kaneko16, ROT90, "Kaneko", "Bakuretsu Breaker",       GAME_IMPERFECT_GRAPHICS )
GAMEX(1992, shogwarr, 0,        shogwarr, shogwarr, shogwarr, ROT0,  "Kaneko", "Shogun Warriors",         GAME_NOT_WORKING )
GAMEX(1992, fjbuster, shogwarr, shogwarr, shogwarr, fjbuster, ROT0,  "Kaneko", "Fujiyama Buster (Japan)", GAME_NOT_WORKING )
GAMEX(1992, brapboys, 0,        shogwarr, shogwarr, 0,        ROT0,  "Kaneko", "B.Rap Boys",              GAME_NOT_WORKING )
GAMEX(1994, bloodwar, 0,        bloodwar, bloodwar, kaneko16, ROT0,  "Kaneko", "Blood Warrior",           GAME_NOT_WORKING )
GAMEX(1994, bonkadv,  0,        bonkadv,  bakubrkr, 0,		  ROT0,  "Kaneko", "Bonks Adventure",		  GAME_NOT_WORKING )
