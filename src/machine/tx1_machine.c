/***************************************************************************

	Tatsumi TX-1 / Buggy Boy machine hardware: the arithmetic unit

	From current MAME (src/mame/tatsumi/tx1_m.cpp, Philip Bennett)

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "cpu/i86/i86.h"
#include "includes/tx1.h"


struct tx1_math_t tx1_math;
struct tx1_sn74s516_t tx1_sn74s516;

/* the data ROM and the PROM of the arithmetic unit */
/* the au data region is 16-bit (host order words), from an aligned allocation */
#define AU_DATA		((UINT16 *)(void *)memory_region(REGION_USER3))

/*
	Helper functions
*/
#define INC_PROM_ADDR		( tx1_math.promaddr = (tx1_math.promaddr + 1) & 0x1ff )
#define ROR16(val, shift)	( (UINT16)(((UINT16)(val) >> (shift)) | ((UINT16)(val) << (16 - (shift)))) )
#define ROL16(val, shift)	( (UINT16)(((UINT16)(val) << (shift)) | ((UINT16)(val) >> (16 - (shift)))) )
#define SWAP16(val)			( (UINT16)((((UINT16)(val) << 8) & 0xff00) | ((UINT16)(val) >> 8)) )

/* the two halves of the 32-bit result register */
#define ZW_Z				((INT16)((UINT32)tx1_sn74s516.ZW32 >> 16))
#define ZW_W				((UINT16)tx1_sn74s516.ZW32)
#define SET_ZW_Z(v)			( tx1_sn74s516.ZW32 = (INT32)(((UINT32)tx1_sn74s516.ZW32 & 0x0000ffff) | ((UINT32)(UINT16)(v) << 16)) )
#define SET_ZW_W(v)			( tx1_sn74s516.ZW32 = (INT32)(((UINT32)tx1_sn74s516.ZW32 & 0xffff0000) | (UINT16)(v)) )

static INLINE UINT8 reverse_nibble(UINT8 nibble)
{
	return	(nibble & 1) << 3 |
			(nibble & 2) << 1 |
			(nibble & 4) >> 1 |
			(nibble & 8) >> 3;
}


/*
	State transition table

	A little different to the real thing in that
	there are no states between final input and
	multiplication/division.
*/
static const UINT8 state_table[16][8] =
{
	{  4,  4,  4,  4,  5,  1,  1,  0 },
	{  4,  4,  4,  4,  5,  5,  3,  0 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{  4,  4,  4,  4,  5,  5, 11,  0 },
	{  8,  8,  8,  8,  8,  8,  8,  8 },
	{ 10, 10, 10, 10, 10, 10, 10, 10 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{  4,  4,  4,  4,  5,  0,  1,  0 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{  4,  4,  4,  4,  4,  5,  1,  0 },
	{  4,  4,  4,  4,  5,  5,  1,  0 },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
	{0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf },
};

static void sn74s516_multiply(void)
{
	INT32 X = tx1_sn74s516.X;
	INT32 Y = tx1_sn74s516.Y;

	switch (tx1_sn74s516.code)
	{
	case 0:
		tx1_sn74s516.ZW32 = X * Y;
		break;
	case 2:
		tx1_sn74s516.ZW32 += X * Y;
		break;
	case 3:
		tx1_sn74s516.ZW32 += -X * Y;
		break;
	case 0x60:
		tx1_sn74s516.ZW32 = X * Y;
		break;
	case 0x61:
		tx1_sn74s516.ZW32 = -X * Y;
		break;
	case 0x62:
		tx1_sn74s516.ZW32 += X * Y;
		break;
	case 0x63:
		tx1_sn74s516.ZW32 += -X * Y;
		break;
	case 0x660:
		tx1_sn74s516.ZW32 = (INT32)((UINT32)(X * Y) + ((UINT32)tx1_sn74s516.ZW32 & 0xffff0000));
		break;
	case 0x661:
		tx1_sn74s516.ZW32 = (INT32)((UINT32)(-X * Y) + ((UINT32)tx1_sn74s516.ZW32 & 0xffff0000));
		break;
	case 0x662:
		tx1_sn74s516.ZW32 = (INT32)((UINT32)(-X * Y) + ((UINT32)tx1_sn74s516.ZW32 & 0xffff0000));
		break;
	case 0x6660:
		tx1_sn74s516.ZW32 += (X * Y);
		break;
	default:
		break;
	}

	/* Seems a good enough place to clear it. */
	tx1_sn74s516.ZWfl = 0;
}

static void sn74s516_divide(void)
{
	INT32 X = tx1_sn74s516.X;
	INT32 Z = 0;
	INT32 W = 0;

	if (X == 0)
	{
		SET_ZW_Z(0xffff);
		SET_ZW_W(0xffff);
		tx1_sn74s516.ZWfl = 0;
		return;
	}

	switch (tx1_sn74s516.code)
	{
	case 4:
		Z = tx1_sn74s516.ZW32 / X;
		W = tx1_sn74s516.ZW32 % X;
		break;
	case 0x664:
		Z = tx1_sn74s516.ZW32 / X;
		W = tx1_sn74s516.ZW32 % X;
		break;
	case 0x6664:
		Z = (INT32)ZW_W / X;
		W = (INT32)ZW_W % X;
		break;
	default:
		break;
	}

	/* Divide overflow Only happens during chip test anyway */
	if (Z > 0xffff)
		Z |= 0xff00;

	SET_ZW_Z(Z);
	SET_ZW_W(W);
	tx1_sn74s516.ZWfl = 0;
}

static void sn74s516_update(int ins)
{
	tx1_sn74s516.state = state_table[tx1_sn74s516.state][ins];

	if (tx1_sn74s516.state == 4)
	{
		sn74s516_multiply();
		tx1_sn74s516.state = 8;
	}
	else if (tx1_sn74s516.state == 5)
	{
		sn74s516_divide();
		tx1_sn74s516.state = 10;
	}
}

static void kick_sn74s516(UINT16 *data, int ins)
{
#define LOAD_X		(tx1_sn74s516.X = (INT16)*data)
#define LOAD_Y		(tx1_sn74s516.Y = (INT16)*data)
#define LOAD_Z		SET_ZW_Z(*data)
#define LOAD_W		SET_ZW_W(*data)
#define READ_ZW		*data = tx1_sn74s516.ZWfl ? ZW_W : (UINT16)ZW_Z; \
					tx1_sn74s516.ZWfl ^= 1;

#define UPDATE_SEQUENCE	(tx1_sn74s516.code = (tx1_sn74s516.code << 4) | ins)
#define CLEAR_SEQUENCE	(tx1_sn74s516.code = 0)

	/*
		Remember to change the Z/W flag.
	*/
	switch (tx1_sn74s516.state)
	{
	case 0:
		CLEAR_SEQUENCE;
		UPDATE_SEQUENCE;

		if (ins < 4)
		{
			LOAD_Y;
			sn74s516_update(ins);
		}
		else if (ins == 4)
		{
			sn74s516_update(ins);
		}
		else if (ins < 7)
		{
			LOAD_X;
			sn74s516_update(ins);
		}
		else if (ins == 7)
		{
			READ_ZW;
			break;
		}

		break;
	case 8:
	case 10:
		CLEAR_SEQUENCE;
		UPDATE_SEQUENCE;

		if (ins < 4)
		{
			LOAD_Y;
			sn74s516_update(ins);
		}
		else if (ins == 4)
		{
			sn74s516_update(ins);
		}
		else if (ins == 5)
		{
			/* Rounding */
			/* Operation */
			sn74s516_update(ins);
		}
		else if (ins == 6)
		{
			LOAD_X;
			sn74s516_update(ins);
		}
		else if (ins == 7)
		{
			READ_ZW;
			sn74s516_update(ins);
		}
		break;
	case 1:
		/* TODO: 6666 represents an incomplete state - clear it. */
		if (tx1_sn74s516.code == 0x6666)
			CLEAR_SEQUENCE;

		UPDATE_SEQUENCE;
		if (ins < 4)
		{
			LOAD_Y;
			sn74s516_update(ins);
		}
		else if (ins < 6)
		{
			sn74s516_update(ins);
		}
		else if (ins == 6)
		{
			LOAD_Z;
			sn74s516_update(ins);
		}
		else if (ins == 7)
		{
			/* Pointless operation. */
			sn74s516_update(ins);
		}

		break;
	case 3:
		UPDATE_SEQUENCE;
		if (ins < 4)
		{
			LOAD_Y;
			sn74s516_update(ins);
		}
		else if (ins == 4)
		{
			LOAD_W;
			sn74s516_update(ins);
		}
		else if (ins == 5)
		{
			sn74s516_update(ins);
		}
		else if (ins == 6)
		{
			LOAD_W;
			sn74s516_update(ins);
		}
		else if (ins == 7)
		{
			READ_ZW;
			sn74s516_update(ins);
		}
		break;
	case 11:
		UPDATE_SEQUENCE;
		if (ins < 4)
		{
			LOAD_Y;
			sn74s516_update(ins);
		}
		else if (ins < 6)
		{
			sn74s516_update(ins);
		}
		else if (ins == 6)
		{
			/* CHECK: Incomplete state */
			sn74s516_update(ins);
		}
		else if (ins == 7)
		{
			/* 6667 = Load X, Load Z, Load W, Clear Z */
			SET_ZW_Z(0);
			sn74s516_update(ins);
		}
		break;
	default:
		break;
	}

#undef LOAD_X
#undef LOAD_Y
#undef LOAD_Z
#undef LOAD_W
#undef READ_ZW
#undef UPDATE_SEQUENCE
#undef CLEAR_SEQUENCE
}


/*
	The arithmetic unit's data latches: MAME passes uninitialised locals
	when a step only writes them; here they start at 0, so that the
	results don't depend on the host.
*/

#define LHIEN(a)	!((a) & 0x80)
#define LLOEN(a)	!((a) & 0x40)
#define GO_EN(a)	!((a) & 0x4000)


/***************************************************************************

  TX-1

  Preliminary

***************************************************************************/

/* Same mapping as Buggy Boy actually */
#define TX1_INSLD		0x100
#define TX1_CNTST		0x80
#define TX1_RADCHG		0x20
#define TX1_DSEL		0x03

enum
{
	TX1_SEL_MULEN = 0x00,
	TX1_SEL_PPSEN,
	TX1_SEL_PSSEN,
	TX1_SEL_LMSEL,
	TX1_SEL_DSELOE,
	TX1_SEL_INSCL = 0x06,
	TX1_SEL_ILDEN
};

#define TX1_SET_INS0_BIT	do { if (!(ins & 0x4) && tx1_math.i0ff) ins |= tx1_math.i0ff; } while(0)

static INLINE UINT16 get_datarom_addr(void)
{
	UINT16 addr = ((tx1_math.inslatch & 0x1c00) << 1) | (tx1_math.ppshift & 0xff);

	if ((tx1_math.inslatch >> 8) & TX1_RADCHG)
		addr |= (tx1_math.ppshift & 0x0700);
	else
		addr |= (tx1_math.promaddr << 3) & 0x0700;

	return addr & 0x3fff;
}

/* O4 of the latch selection PAL */
static INLINE int tx1_o4(int dsel)
{
	int tfad = (tx1_math.inslatch & 0x1c00) << 1;
	int sd   = tx1_math.ppshift;

	return
		(!BIT(sd, 9) && !BIT(sd,10)) ||
		( BIT(sd, 7) &&  BIT(sd,10)) ||
		(!BIT(sd, 8) &&  BIT(sd, 9)) ||
		(!BIT(sd, 7) &&  BIT(sd, 8)) ||
		!BIT(dsel, 1) || BIT(tfad, 13) || BIT(tfad, 12) || BIT(tfad, 11);
}

static void tx1_update_state(void)
{
	const UINT16 *prom = AU_DATA + (0x8000 >> 1);

	for (;;)
	{
		int go = 0;

		if (!GO_EN(tx1_math.inslatch) && GO_EN(prom[tx1_math.promaddr]))
			go = 1;
		/*
			Example:
			120 /GO /LHIEN
			121 /GO        /LLOEN
			Both 120 and 121 are used.
		*/
		else if ((GO_EN(tx1_math.inslatch) && GO_EN(prom[tx1_math.promaddr])) && (LHIEN(tx1_math.inslatch) && LLOEN(prom[tx1_math.promaddr])))
			go = 1;

		/* Now update the latch */
		tx1_math.inslatch = prom[tx1_math.promaddr] & 0x7fff;
		tx1_math.mux = (tx1_math.inslatch >> 3) & 7;

		if (tx1_math.mux == TX1_SEL_INSCL)
		{
			tx1_math.i0ff = 0;
		}

		/* TODO */
		if (go)
		{
			int ins = tx1_math.inslatch & 7;

			TX1_SET_INS0_BIT;

			if (tx1_math.mux == TX1_SEL_DSELOE)
			{
				int		dsel = (tx1_math.inslatch >> 8) & TX1_DSEL;
				int		o4 = tx1_o4(dsel);
				UINT16	data = 0;

				dsel = (dsel & 2) | ((dsel & o4) ^ 1);

				if (dsel == 0)
					data = tx1_math.muxlatch;
				else if (dsel == 1)
					data = AU_DATA[get_datarom_addr()];
				else if (dsel == 2)
					data = ROL16(tx1_math.muxlatch, 4);
				else if (dsel == 3)
					data = ROL16(SWAP16(tx1_math.muxlatch), 3);

				kick_sn74s516(&data, ins);
			}
			/*
				TODO: Changed ppshift to muxlatch for TX-1

				/TMPLD1: /LHIEN
				/TMPLD2: /LLOEN.!O4 + (/LHIEN.O4)
				/TMPLD3: /LLOEN
				     O4: !SD9.!SD10./LMSEL + SD7.SD10./LMSEL +
				         !SD8.SD9./LMSEL + !SD7.SD8./LMSEL +
				         /LMSEL./DSEL1 + /LMSEL.TFAD13 + /LMSEL.TFAD12 + /LMSEL.TFAD11
			*/
			else if (LHIEN(tx1_math.inslatch) || LLOEN(tx1_math.inslatch))
			{
				UINT16 data = 0;

				kick_sn74s516(&data, ins);

				/* All latches enabled */
				if (LHIEN(tx1_math.inslatch) && LLOEN(tx1_math.inslatch))
				{
					tx1_math.muxlatch = data;
				}
				else if (tx1_math.mux == TX1_SEL_LMSEL) /* O4 = 0 */
				{
					/* TMPLD2/TMPLD3 15-5 */
					if (LLOEN(tx1_math.inslatch))
					{
						tx1_math.muxlatch &= 0x001f;
						tx1_math.muxlatch |= data & 0xffe0;
					}
					/* TMLPD1 4-0??????? */
					else if (LHIEN(tx1_math.inslatch))
					{
						tx1_math.muxlatch &= 0xffe0;
						tx1_math.muxlatch |= data & 0x001f;
					}
				}
				else
				{
					int dsel = (tx1_math.inslatch >> 8) & TX1_DSEL;
					int o4 = tx1_o4(dsel);

					if (LLOEN(tx1_math.inslatch))
					{
						tx1_math.muxlatch &= 0x0fff;
						tx1_math.muxlatch |= data & 0xf000;

						if (!o4)
						{
							/* TMPLD11-5 */
							tx1_math.muxlatch &= 0xf01f;
							tx1_math.muxlatch |= data & 0x0fe0;
						}
					}
					else if (LHIEN(tx1_math.inslatch))
					{
						tx1_math.muxlatch &= 0xffe0;
						tx1_math.muxlatch |= data & 0x001f;

						if (o4)
						{
							/* TMPLD11-5 */
							tx1_math.muxlatch &= 0xf01f;
							tx1_math.muxlatch |= data & 0x0fe0;
						}
					}
				}
			}
			else
			{
				if (tx1_math.mux == TX1_SEL_PPSEN)
				{
					kick_sn74s516(&tx1_math.ppshift, ins);
				}
				else
				{
					/* Bus pullups give 0xffff */
					UINT16 data = 0xffff;
					kick_sn74s516(&data, ins);
				}
			}
		}

		/* Is there another instruction in the sequence? */
		if (prom[tx1_math.promaddr] & 0x8000)
			break;
		else
			INC_PROM_ADDR;
	}
}

READ16_HANDLER( tx1_math_r )
{
	offset = offset << 1;

	/* /MLPCS */
	if (offset < 0x400)
	{
		int ins;

		if (offset & 0x200)
		{
			ins = tx1_math.inslatch & 7;
			TX1_SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		/* TODO What do we return? */
		kick_sn74s516(&tx1_math.retval, ins);
	}
	/* /PPSEN */
	else if (offset < 0x800)
	{
		/* Unused - just pullups? */
		tx1_math.retval = 0xffff;
	}
	/* /MUXCS */
	else if ((offset & 0xc00) == 0xc00)
	{
		int dsel = (tx1_math.inslatch >> 8) & TX1_DSEL;
		int o4;

		if (tx1_math.mux == TX1_SEL_LMSEL)
			o4 = 0;
		else
			o4 = tx1_o4(dsel);

		dsel = (dsel & 2) | ((dsel & o4) ^ 1);

		if (dsel == 0)
			tx1_math.retval = tx1_math.muxlatch;
		else if (dsel == 1)
			tx1_math.retval = AU_DATA[get_datarom_addr()];
		else if (dsel == 2)
			tx1_math.retval = ROL16(tx1_math.muxlatch, 4);
		else if (dsel == 3)
			tx1_math.retval = ROL16(SWAP16(tx1_math.muxlatch), 3);

		/* TODO for TX-1: This is /SPCS region? */
		if (offset < 0xe00)
		{
			/* Load the PP with retval?????? */
			if (tx1_math.mux == TX1_SEL_PPSEN)
			{
				tx1_math.ppshift = tx1_math.retval & 0x3fff;
			}
			else if (tx1_math.mux == TX1_SEL_PSSEN)
			{
				/* WRONG!!!! */
				tx1_math.ppshift = tx1_math.retval;
			}

			if (tx1_math.mux != TX1_SEL_ILDEN)
			{
				INC_PROM_ADDR;
				tx1_update_state();

				/* MUST RETURN HERE? */
				return tx1_math.retval;
			}
		}
	}
	else
	{
		if (tx1_math.mux == TX1_SEL_PPSEN)
			tx1_math.retval = tx1_math.ppshift & 0x3fff;
		else
			/* Nothing is mapped - read from pull up resistors! */
			tx1_math.retval = 0xffff;
	}

	if (offset & TX1_INSLD)
	{
		tx1_math.promaddr = (offset << 2) & 0x1ff;
		tx1_update_state();
	}
	else if (offset & TX1_CNTST)
	{
		INC_PROM_ADDR;
		tx1_update_state();
	}

	return tx1_math.retval;
}

/* the parallel to serial shifter load: TX-1 shifts in and out */
static UINT16 tx1_shift(UINT16 val, UINT16 cpulatch)
{
	int shift;

	if (cpulatch & 0x3800)
	{
		shift = (cpulatch >> 11) & 0x7;

		while (shift)
		{
			val >>= 1;
			shift >>= 1;
		}
	}
	else
	{
		shift = (cpulatch >> 7) & 0xf;
		shift = reverse_nibble(shift);
		shift >>= 1;

		while (shift)
		{
			val <<= 1;
			shift >>= 1;
		}
	}
	return val;
}

WRITE16_HANDLER( tx1_math_w )
{
	tx1_math.cpulatch = data;
	offset <<= 1;

	/* /MLPCS */
	if (offset < 0x400)
	{
		int ins;

		if (offset & 0x200)
		{
			ins = tx1_math.inslatch & 7;
			TX1_SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		kick_sn74s516(&tx1_math.cpulatch, ins);
	}
	/* /PPSEN */
	else if ((offset & 0xc00) == 0x400)
	{
		/* Input is 14 bits */
		tx1_math.ppshift = tx1_math.cpulatch & 0x3fff;
	}
	/* /PSSEN */
	else if ((offset & 0xc00) == 0x800)
	{
		tx1_math.ppshift = tx1_shift(tx1_math.ppshift, tx1_math.cpulatch);
	}
	/* /MUXCS */
	else if ((offset & 0xc00) == 0xc00)
	{
		/*
			/TMPLD1: 0
			/TMPLD2: 0
			/TMPLD3: 0
		*/
		tx1_math.muxlatch = tx1_math.cpulatch;
	}

	if (offset & TX1_INSLD)
	{
		tx1_math.promaddr = (offset << 2) & 0x1ff;
		tx1_update_state();
	}
	else if (offset & TX1_CNTST)
	{
		INC_PROM_ADDR;
		tx1_update_state();
	}
}

/* the /SPCS reads of the math CPU (ROM and RAM): the data goes to the unit too */
static UINT16 tx1_spcs(UINT16 data)
{
	tx1_math.cpulatch = data;

	if (tx1_math.mux == TX1_SEL_ILDEN)
	{
		tx1_math.i0ff = tx1_math.cpulatch & (1 << 14) ? 1 : 0;
	}
	else if (tx1_math.mux == TX1_SEL_MULEN)
	{
		int ins = tx1_math.inslatch & 7;

		TX1_SET_INS0_BIT;
		kick_sn74s516(&tx1_math.cpulatch, ins);
	}
	else if (tx1_math.mux == TX1_SEL_PPSEN)
	{
		tx1_math.ppshift = tx1_math.cpulatch;
	}
	else if (tx1_math.mux == TX1_SEL_PSSEN)
	{
		tx1_math.ppshift = tx1_shift(tx1_math.ppshift, tx1_math.cpulatch) & 0x7ff;
	}

	if (tx1_math.mux != TX1_SEL_ILDEN)
	{
		INC_PROM_ADDR;
		tx1_update_state();
	}

	return tx1_math.cpulatch;
}

/* a word of the math CPU ROM at 0x5000 (the CPU region is in bus order:
   little endian bytes, whatever the host) */
static UINT16 math_rom_word(offs_t offset)
{
	const UINT8 *rom = memory_region(REGION_CPU2) + 0x05000 + offset * 2;
	return rom[0] | (rom[1] << 8);
}

/* the math CPU ROM, 0x5000-0x7fff */
READ16_HANDLER( tx1_spcs_rom_r )
{
	return tx1_spcs(math_rom_word(offset));
}

READ16_HANDLER( tx1_spcs_ram_r )
{
	return tx1_spcs(tx1_math_ram[offset]);
}

/* Should never occur */
WRITE16_HANDLER( tx1_spcs_ram_w )
{
	COMBINE_DATA(&tx1_math_ram[offset]);
}


/***************************************************************************

  Buggy Boy

***************************************************************************/
#define BB_INSLD		0x100
#define BB_CNTST		0x80
#define BB_RADCHG		0x20
#define BB_DSEL			0x03

enum
{
	BB_MUX_MULEN = 0x00,
	BB_MUX_PPSEN,
	BB_MUX_PSSEN,
	BB_MUX_LMSEL,
	BB_MUX_DPROE,
	BB_MUX_PPOE,
	BB_MUX_INSCL,
	BB_MUX_ILDEN
};

#define BB_SET_INS0_BIT	do { if (!(ins & 0x4) && tx1_math.i0ff) ins |= tx1_math.i0ff;} while(0)

static INLINE UINT16 get_bb_datarom_addr(void)
{
	UINT16 addr = ((tx1_math.inslatch & 0x1c00) << 1) | (tx1_math.ppshift & 0xff);

	if ((tx1_math.inslatch >> 8) & BB_RADCHG)
		addr |= (tx1_math.ppshift & 0x0700);
	else
		addr |= (tx1_math.promaddr << 3) & 0x0700;

	return addr & 0x3fff;
}

static void buggyboy_update_state(void)
{
	const UINT16 *prom = AU_DATA + (0x8000 >> 1);

	for (;;)
	{
		int go = 0;

		if (!GO_EN(tx1_math.inslatch) && GO_EN(prom[tx1_math.promaddr]))
			go = 1;
		else if ((GO_EN(tx1_math.inslatch) && GO_EN(prom[tx1_math.promaddr])) && (LHIEN(tx1_math.inslatch) && LLOEN(prom[tx1_math.promaddr])))
			go = 1;

		/* Now update the latch */
		tx1_math.inslatch = prom[tx1_math.promaddr] & 0x7fff;
		tx1_math.mux = (tx1_math.inslatch >> 3) & 7;

		if (tx1_math.mux == BB_MUX_INSCL)
			tx1_math.i0ff = 0;

		/* TODO */
		if (go)
		{
			int ins = tx1_math.inslatch & 7;

			BB_SET_INS0_BIT;

			if (tx1_math.mux == BB_MUX_DPROE)
			{
				/* the unit reads the data ROM: a read of the result
				   writes to it in MAME; the ROM is left unchanged here */
				UINT16 data = AU_DATA[get_bb_datarom_addr()];
				kick_sn74s516(&data, ins);
			}
			else if (tx1_math.mux == BB_MUX_PPOE)
			{
				kick_sn74s516(&tx1_math.ppshift, ins);
			}
			/* This is quite tricky. */
			/* It can either be a read operation or */
			/* What if /LHIEN and /LLOEN? */
			else if (LHIEN(tx1_math.inslatch) || LLOEN(tx1_math.inslatch))
			{
				UINT16 data = 0;

				kick_sn74s516(&data, ins);

				if (LHIEN(tx1_math.inslatch) && LLOEN(tx1_math.inslatch))
				{
					tx1_math.ppshift = data;
				}
				else if (tx1_math.mux == BB_MUX_LMSEL)
				{
					if (LLOEN(tx1_math.inslatch))
					{
						tx1_math.ppshift &= 0x000f;
						tx1_math.ppshift |= data & 0xfff0;
					}
					else if (LHIEN(tx1_math.inslatch))
					{
						tx1_math.ppshift &= 0xfff0;
						tx1_math.ppshift |= data & 0x000f;
					}
				}
				else
				{
					if (LLOEN(tx1_math.inslatch))
					{
						tx1_math.ppshift &= 0x0fff;
						tx1_math.ppshift |= data & 0xf000;
					}
					else if (LHIEN(tx1_math.inslatch))
					{
						tx1_math.ppshift &= 0xf000;
						tx1_math.ppshift |= data & 0x0fff;
					}
				}
			}
			else
			{
				if (tx1_math.mux == BB_MUX_PPSEN)
				{
					kick_sn74s516(&tx1_math.ppshift, ins);
				}
				else
				{
					/* Bus pullups give 0xffff */
					UINT16 data = 0xffff;
					kick_sn74s516(&data, ins);
				}
			}
		}

		/* Handle rotation */
		if (((tx1_math.inslatch >> 8) & BB_DSEL) == 1)
		{
			tx1_math.ppshift = ROR16(tx1_math.ppshift, 4);
		}
		else if (((tx1_math.inslatch >> 8) & BB_DSEL) == 2)
		{
			tx1_math.ppshift = ROL16(tx1_math.ppshift, 4);
		}

		/* Is there another instruction in the sequence? */
		if (prom[tx1_math.promaddr] & 0x8000)
			break;
		else
			INC_PROM_ADDR;
	}
}

READ16_HANDLER( buggyboy_math_r )
{
	offset = offset << 1;

	/* /MLPCS */
	if (offset < 0x400)
	{
		int ins;

		if (offset & 0x200)
		{
			ins = tx1_math.inslatch & 7;
			BB_SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		/* TODO What do we return? */
		kick_sn74s516(&tx1_math.retval, ins);
	}
	/* /PPSEN */
	else if (offset < 0x800)
	{
		tx1_math.retval = tx1_math.ppshift;
	}
	/* /DPROE */
	else if ((offset & 0xc00) == 0xc00)
	{
		UINT16 addr = get_bb_datarom_addr();

		tx1_math.retval = AU_DATA[addr];

		/* This is necessary */
		if (tx1_math.mux == BB_MUX_PPSEN)
			tx1_math.ppshift = AU_DATA[addr];

		/* This is /SPCS region? Necessary anyway */
		if (offset < 0xe00)
		{
			if (tx1_math.mux != BB_MUX_ILDEN)
			{
				INC_PROM_ADDR;
				buggyboy_update_state();
			}
		}
	}
	else
	{
		if (tx1_math.mux == BB_MUX_PPSEN)
			tx1_math.retval = tx1_math.ppshift;
		else
			/* Nothing is mapped - read from pull up resistors! */
			tx1_math.retval = 0xffff;
	}

	if (offset & BB_INSLD)
	{
		tx1_math.promaddr = (offset << 2) & 0x1ff;
		buggyboy_update_state();
	}
	else if (offset & BB_CNTST)
	{
		INC_PROM_ADDR;
		buggyboy_update_state();
	}

	return tx1_math.retval;
}

/* the parallel to serial shifter load: Buggy Boy rotates */
static UINT16 bb_shift(UINT16 val, UINT16 cpulatch)
{
	int shift;

	if (cpulatch & 0x3800)
	{
		shift = (cpulatch >> 11) & 0x7;

		while (shift)
		{
			val = ROR16(val, 1);
			shift >>= 1;
		}
	}
	else
	{
		shift = (cpulatch >> 7) & 0xf;
		shift = reverse_nibble(shift);
		shift >>= 1;

		while (shift)
		{
			val = ROL16(val, 1);
			shift >>= 1;
		}
	}
	return val;
}

WRITE16_HANDLER( buggyboy_math_w )
{
	tx1_math.cpulatch = data;

	offset <<= 1;

	/* /MLPCS */
	if (offset < 0x400)
	{
		int ins;

		if (offset & 0x200)
		{
			ins = tx1_math.inslatch & 7;
			BB_SET_INS0_BIT;
		}
		else
		{
			ins = (offset >> 1) & 7;
		}

		kick_sn74s516(&tx1_math.cpulatch, ins);
	}
	/* /PPSEN */
	else if ((offset & 0xc00) == 0x400)
	{
		tx1_math.ppshift = tx1_math.cpulatch;
	}
	/* /PSSEN */
	else if ((offset & 0xc00) == 0x800)
	{
		if (((tx1_math.inslatch >> 8) & BB_DSEL) == 3)
			tx1_math.ppshift = bb_shift(tx1_math.ppshift, tx1_math.cpulatch);
	}

	if (offset & BB_INSLD)
	{
		tx1_math.promaddr = (offset << 2) & 0x1ff;
		buggyboy_update_state();
	}
	else if (offset & BB_CNTST)
	{
		INC_PROM_ADDR;
		buggyboy_update_state();
	}
}

static UINT16 bb_spcs(UINT16 data)
{
	tx1_math.cpulatch = data;

	if (tx1_math.mux == BB_MUX_ILDEN)
	{
		tx1_math.i0ff = tx1_math.cpulatch & (1 << 14) ? 1 : 0;
	}
	else if (tx1_math.mux == BB_MUX_MULEN)
	{
		int ins = tx1_math.inslatch & 7;

		BB_SET_INS0_BIT;
		kick_sn74s516(&tx1_math.cpulatch, ins);
	}
	else if (tx1_math.mux == BB_MUX_PPSEN)
	{
		tx1_math.ppshift = tx1_math.cpulatch;
	}
	else if (tx1_math.mux == BB_MUX_PSSEN)
	{
		if (((tx1_math.inslatch >> 8) & BB_DSEL) == 3)
			tx1_math.ppshift = bb_shift(tx1_math.ppshift, tx1_math.cpulatch);
	}

	if (tx1_math.mux != BB_MUX_ILDEN)
	{
		INC_PROM_ADDR;
		buggyboy_update_state();
	}

	return tx1_math.cpulatch;
}

/*
	This is for ROM range 0x5000-0x7fff
*/
READ16_HANDLER( buggyboy_spcs_rom_r )
{
	return bb_spcs(math_rom_word(offset));
}

WRITE16_HANDLER( buggyboy_spcs_ram_w )
{
	COMBINE_DATA(&tx1_math_ram[offset]);
}

READ16_HANDLER( buggyboy_spcs_ram_r )
{
	return bb_spcs(tx1_math_ram[offset]);
}


/*************************************
 *
 *  Machine Reset
 *
 *************************************/

MACHINE_INIT( tx1 )
{
	/* TODO: This is connected to the /BUSACK line of the Z80 */
	cpu_set_irq_line(0, TX1_TEST_LINE, ASSERT_LINE);

	memset(&tx1_math, 0, sizeof(tx1_math));

	tx1_sn74s516.state = 0;
}

void tx1_machine_state_register(void)
{
	state_save_register_UINT16("tx1_math", 0, "cpulatch",  &tx1_math.cpulatch, 1);
	state_save_register_UINT16("tx1_math", 0, "promaddr",  &tx1_math.promaddr, 1);
	state_save_register_UINT16("tx1_math", 0, "inslatch",  &tx1_math.inslatch, 1);
	state_save_register_UINT32("tx1_math", 0, "mux",       &tx1_math.mux, 1);
	state_save_register_UINT16("tx1_math", 0, "ppshift",   &tx1_math.ppshift, 1);
	state_save_register_UINT32("tx1_math", 0, "i0ff",      &tx1_math.i0ff, 1);
	state_save_register_UINT16("tx1_math", 0, "retval",    &tx1_math.retval, 1);
	state_save_register_UINT16("tx1_math", 0, "muxlatch",  &tx1_math.muxlatch, 1);
	state_save_register_INT16 ("tx1_math", 0, "X",         &tx1_sn74s516.X, 1);
	state_save_register_INT16 ("tx1_math", 0, "Y",         &tx1_sn74s516.Y, 1);
	state_save_register_INT32 ("tx1_math", 0, "ZW",        &tx1_sn74s516.ZW32, 1);
	state_save_register_INT32 ("tx1_math", 0, "code",      &tx1_sn74s516.code, 1);
	state_save_register_INT32 ("tx1_math", 0, "state",     &tx1_sn74s516.state, 1);
	state_save_register_INT32 ("tx1_math", 0, "ZWfl",      &tx1_sn74s516.ZWfl, 1);
}
