/***************************************************************************

							  -= Unico Games =-

					driver by	Luca Elia (l.elia@tin.it)


CPU			:	M68000 or M68EC020
Sound Chips	:	OKI M6295 (AD-65) + YM3812 (K-666) or YM2151
Video Chips	:	3 x Actel A1020B (Square 84 Pin Socketed) [Or A40MX04-F]
				MACH211 (Square 44 Pin Socketed) [Or MACH210-15JC]


---------------------------------------------------------------------------
Year + Game			PCB				Notes
---------------------------------------------------------------------------
97	Burglar X		?
98	Zero Point		ZPM1001A/B		Has Light Guns.
99	Zero Point 2	UZP21001A/B		Has Light Guns.
---------------------------------------------------------------------------


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"
#include "unico.h"

/* Variables needed by vidhrdw: */

int unico_has_lightgun;

/***************************************************************************


								Memory Maps


***************************************************************************/

READ16_HANDLER ( YM3812_status_port_0_msb_r )	{	return YM3812_status_port_0_r(0) << 8;	}
WRITE16_HANDLER( YM3812_register_port_0_msb_w )	{	if (ACCESSING_MSB)	YM3812_control_port_0_w(0,data >> 8);	}
WRITE16_HANDLER( YM3812_data_port_0_msb_w )		{	if (ACCESSING_MSB)	YM3812_write_port_0_w(0,data >> 8);		}


/*
 Lines starting with an empty comment in the following MemoryReadAddress
 arrays are there for debug (e.g. the game does not read from those ranges
 AFAIK)
*/

/***************************************************************************
								Burglar X
***************************************************************************/

static WRITE16_HANDLER( burglarx_sound_bank_w )
{
	if (ACCESSING_MSB)
	{
		int bank = (data >> 8 ) & 1;
		OKIM6295_set_bank_base(0, 0x40000 * bank );
	}
}

static MEMORY_READ16_START( readmem_burglarx )
	{ 0x000000, 0x0fffff, MRA16_ROM						},	/* ROM*/
	{ 0xff0000, 0xffffff, MRA16_RAM						},	/* RAM*/
	{ 0x800000, 0x800001, input_port_0_word_r			},	/* P1 + P2*/
	{ 0x800018, 0x800019, input_port_1_word_r			},	/* Buttons*/
	{ 0x80001a, 0x80001b, input_port_2_word_r			},	/* DSW*/
	{ 0x80001c, 0x80001d, input_port_3_word_r			},	/* DSW*/
	{ 0x800188, 0x800189, OKIM6295_status_0_lsb_r		},	/* Sound*/
	{ 0x80018c, 0x80018d, YM3812_status_port_0_msb_r	},	/**/
/**/{ 0x904000, 0x907fff, MRA16_RAM						},	/* Layers*/
/**/{ 0x908000, 0x90bfff, MRA16_RAM						},	/**/
/**/{ 0x90c000, 0x90ffff, MRA16_RAM						},	/**/
/**/{ 0x920000, 0x923fff, MRA16_RAM						},	/* ? 0*/
/**/{ 0x930000, 0x9307ff, MRA16_RAM						},	/* Sprites*/
/**/{ 0x940000, 0x947fff, MRA16_RAM						},	/* Palette*/
MEMORY_END

static MEMORY_WRITE16_START( writemem_burglarx )
	{ 0x000000, 0x0fffff, MWA16_ROM							},	/* ROM*/
	{ 0xff0000, 0xffffff, MWA16_RAM							},	/* RAM*/
	{ 0x800030, 0x800031, MWA16_NOP							},	/* ? 0*/
	{ 0x80010c, 0x80010d, MWA16_RAM, &unico_scrollx_0		},	/* Scroll*/
	{ 0x80010e, 0x80010f, MWA16_RAM, &unico_scrolly_0		},	/**/
	{ 0x800110, 0x800111, MWA16_RAM, &unico_scrolly_2		},	/**/
	{ 0x800114, 0x800115, MWA16_RAM, &unico_scrollx_2		},	/**/
	{ 0x800116, 0x800117, MWA16_RAM, &unico_scrollx_1		},	/**/
	{ 0x800120, 0x800121, MWA16_RAM, &unico_scrolly_1		},	/**/
	{ 0x800188, 0x800189, OKIM6295_data_0_lsb_w				},	/* Sound*/
	{ 0x80018a, 0x80018b, YM3812_data_port_0_msb_w			},	/**/
	{ 0x80018c, 0x80018d, YM3812_register_port_0_msb_w		},	/**/
	{ 0x80018e, 0x80018f, burglarx_sound_bank_w				},	/**/
	{ 0x8001e0, 0x8001e1, MWA16_RAM							},	/* ? IRQ Ack*/
	{ 0x904000, 0x907fff, unico_vram_1_w, &unico_vram_1	},	/* Layers*/
	{ 0x908000, 0x90bfff, unico_vram_2_w, &unico_vram_2	},	/**/
	{ 0x90c000, 0x90ffff, unico_vram_0_w, &unico_vram_0	},	/**/
	{ 0x920000, 0x923fff, MWA16_RAM							},	/* ? 0*/
	{ 0x930000, 0x9307ff, MWA16_RAM, &spriteram16, &spriteram_size	},	/* Sprites*/
	{ 0x940000, 0x947fff, unico_palette_w, &paletteram16	},	/* Palette*/
MEMORY_END



/***************************************************************************
								Zero Point
***************************************************************************/

static WRITE16_HANDLER( zeropnt_sound_bank_w )
{
	if (ACCESSING_MSB)
	{
		/* Banked sound samples. The 3rd quarter of the ROM
		   contains garbage. Indeed, only banks 0&1 are used */

		int bank = (data >> 8 ) & 1;
		unsigned char *dst	= memory_region(REGION_SOUND1);
		unsigned char *src	= dst + 0x80000 + 0x20000 + 0x20000 * bank;
		memcpy(dst + 0x20000, src, 0x20000);

		coin_counter_w(0,data & 0x1000);
		set_led_status(0,data & 0x0800);	/* Start 1*/
		set_led_status(1,data & 0x0400);	/* Start 2*/
	}
}

/* Light Gun - need to wiggle the input slightly otherwise fire doesn't work */
static READ16_HANDLER( unico_gunx_0_msb_r )
{
	int x=readinputport(4);

	x=x*384/256; /* On screen pixel X */
	if (x<0x160) x=0x30 + (x*0xd0/0x15f);
	else x=((x-0x160) * 0x20)/0x1f;

	return ((x&0xff) ^ (cpu_getcurrentframe()&1))<<8;
}

static READ16_HANDLER( unico_guny_0_msb_r )
{
	int y=readinputport(3);

	y=0x18+((y*0xe0)/0xff);

	return ((y&0xff) ^ (cpu_getcurrentframe()&1))<<8;
}

static READ16_HANDLER( unico_gunx_1_msb_r )
{
	int x=readinputport(6);

	x=x*384/256; /* On screen pixel X */
	if (x<0x160) x=0x30 + (x*0xd0/0x15f);
	else x=((x-0x160) * 0x20)/0x1f;

	return ((x&0xff) ^ (cpu_getcurrentframe()&1))<<8;
}

static READ16_HANDLER( unico_guny_1_msb_r )
{
	int y=readinputport(5);

	y=0x18+((y*0xe0)/0xff);

	return ((y&0xff) ^ (cpu_getcurrentframe()&1))<<8;
}

static MEMORY_READ16_START( readmem_zeropnt )
	{ 0x000000, 0x0fffff, MRA16_ROM						},	/* ROM*/
	{ 0xef0000, 0xefffff, MRA16_RAM						},	/* RAM*/
	{ 0x800018, 0x800019, input_port_0_word_r			},	/* Buttons*/
	{ 0x80001a, 0x80001b, input_port_1_word_r			},	/* DSW*/
	{ 0x80001c, 0x80001d, input_port_2_word_r			},	/* DSW*/
	{ 0x800170, 0x800171, unico_guny_0_msb_r			},	/* Light Guns*/
	{ 0x800174, 0x800175, unico_gunx_0_msb_r			},	/**/
	{ 0x800178, 0x800179, unico_guny_1_msb_r			},	/**/
	{ 0x80017c, 0x80017d, unico_gunx_1_msb_r			},	/**/
	{ 0x800188, 0x800189, OKIM6295_status_0_lsb_r		},	/* Sound*/
	{ 0x80018c, 0x80018d, YM3812_status_port_0_msb_r	},	/**/
/**/{ 0x904000, 0x907fff, MRA16_RAM						},	/* Layers*/
/**/{ 0x908000, 0x90bfff, MRA16_RAM						},	/**/
/**/{ 0x90c000, 0x90ffff, MRA16_RAM						},	/**/
/**/{ 0x920000, 0x923fff, MRA16_RAM						},	/* ? 0*/
/**/{ 0x930000, 0x9307ff, MRA16_RAM						},	/* Sprites*/
/**/{ 0x940000, 0x947fff, MRA16_RAM						},	/* Palette*/
MEMORY_END

static MEMORY_WRITE16_START( writemem_zeropnt )
	{ 0x000000, 0x0fffff, MWA16_ROM							},	/* ROM*/
	{ 0xef0000, 0xefffff, MWA16_RAM							},	/* RAM*/
	{ 0x800030, 0x800031, MWA16_NOP							},	/* ? 0*/
	{ 0x80010c, 0x80010d, MWA16_RAM, &unico_scrollx_0		},	/* Scroll*/
	{ 0x80010e, 0x80010f, MWA16_RAM, &unico_scrolly_0		},	/**/
	{ 0x800110, 0x800111, MWA16_RAM, &unico_scrolly_2		},	/**/
	{ 0x800114, 0x800115, MWA16_RAM, &unico_scrollx_2		},	/**/
	{ 0x800116, 0x800117, MWA16_RAM, &unico_scrollx_1		},	/**/
	{ 0x800120, 0x800121, MWA16_RAM, &unico_scrolly_1		},	/**/
	{ 0x800188, 0x800189, OKIM6295_data_0_lsb_w				},	/* Sound*/
	{ 0x80018a, 0x80018b, YM3812_data_port_0_msb_w			},	/**/
	{ 0x80018c, 0x80018d, YM3812_register_port_0_msb_w		},	/**/
	{ 0x80018e, 0x80018f, zeropnt_sound_bank_w				},	/**/
	{ 0x8001e0, 0x8001e1, MWA16_RAM							},	/* ? IRQ Ack*/
	{ 0x904000, 0x907fff, unico_vram_1_w, &unico_vram_1	},	/* Layers*/
	{ 0x908000, 0x90bfff, unico_vram_2_w, &unico_vram_2	},	/**/
	{ 0x90c000, 0x90ffff, unico_vram_0_w, &unico_vram_0	},	/**/
	{ 0x920000, 0x923fff, MWA16_RAM							},	/* ? 0*/
	{ 0x930000, 0x9307ff, MWA16_RAM, &spriteram16, &spriteram_size	},	/* Sprites*/
	{ 0x940000, 0x947fff, unico_palette_w, &paletteram16	},	/* Palette*/
MEMORY_END


/***************************************************************************
								Zero Point 2
***************************************************************************/

static READ32_HANDLER( zeropnt2_coins_r )			{ return (readinputport(0) << 16) | 0xffff; }
static READ32_HANDLER( zeropnt2_dsw1_r )			{ return (readinputport(1) << 16) | 0xffff; }
static READ32_HANDLER( zeropnt2_dsw2_r )			{ return (readinputport(2) << 16) | 0xffff; }
static READ32_HANDLER( zeropnt2_buttons_r )			{ return ((readinputport(7) | ((EEPROM_read_bit() & 0x01) << 7)) << 16) | 0xffff; }

static READ32_HANDLER( zeropnt2_gunx_0_msb_r )		{ return (unico_gunx_0_msb_r(0,0)-0x0800) << 16; }
static READ32_HANDLER( zeropnt2_guny_0_msb_r )		{ return (unico_guny_0_msb_r(0,0)+0x0800) << 16; }
static READ32_HANDLER( zeropnt2_gunx_1_msb_r )		{ return (unico_gunx_1_msb_r(0,0)-0x0800) << 16; }
static READ32_HANDLER( zeropnt2_guny_1_msb_r )		{ return (unico_guny_1_msb_r(0,0)+0x0800) << 16; }

static READ32_HANDLER ( zeropnt2_oki0_r )			{ return OKIM6295_status_0_r(0) << 16; }
static READ32_HANDLER ( zeropnt2_oki1_r )			{ return OKIM6295_status_1_r(0) << 16; }
static WRITE32_HANDLER( zeropnt2_oki0_w )			{ if ((mem_mask & 0x00ff0000) == 0)	OKIM6295_data_0_w(0,(data >> 16) & 0xff); }
static WRITE32_HANDLER( zeropnt2_oki1_w )			{ if ((mem_mask & 0x00ff0000) == 0)	OKIM6295_data_1_w(0,(data >> 16) & 0xff); }

static READ32_HANDLER( zeropnt2_ym2151_status_r )	{ return YM2151_status_port_0_r(0) << 16; }
static WRITE32_HANDLER( zeropnt2_ym2151_reg_w )		{ if ((mem_mask & 0x00ff0000) == 0)	YM2151_register_port_0_w(0,(data >> 16) & 0xff); }
static WRITE32_HANDLER( zeropnt2_ym2151_data_w )	{ if ((mem_mask & 0x00ff0000) == 0)	YM2151_data_port_0_w(0,(data >> 16) & 0xff); }

static WRITE32_HANDLER( zeropnt2_sound_bank_w )
{
	if (ACCESSING_MSB32)
	{
		int bank = ((data >> 24) & 3) % 4;
		unsigned char *dst	= memory_region(REGION_SOUND1);
		unsigned char *src	= dst + 0x80000 + 0x20000 + 0x20000 * bank;
		memcpy(dst + 0x20000, src, 0x20000);
	}
}

static WRITE32_HANDLER( zeropnt2_leds_w )
{
	if ((mem_mask & 0x00ff0000) == 0)
	{
		coin_counter_w(0,data & 0x00010000);
		set_led_status(0,data & 0x00800000);	/* Start 1*/
		set_led_status(1,data & 0x00400000);	/* Start 2*/
	}
}

static WRITE32_HANDLER( zeropnt2_eeprom_w )
{
	if (data & ~0xfe00000)
		log_cb(RETRO_LOG_DEBUG, LOGPRE "CPU #0 PC: %06X - Unknown EEPROM bit written %04X\n",activecpu_get_pc(),data);

	if ( ACCESSING_MSB32 )
	{
		/* latch the bit*/
		EEPROM_write_bit(data & 0x04000000);

		/* reset line asserted: reset.*/
		EEPROM_set_cs_line((data & 0x01000000) ? CLEAR_LINE : ASSERT_LINE);

		/* clock line asserted: write latch or select next bit to read*/
		EEPROM_set_clock_line((data & 0x02000000) ? ASSERT_LINE : CLEAR_LINE );
	}
}

static MEMORY_READ32_START( readmem_zeropnt2 )
	{ 0x000000, 0x1fffff, MRA32_ROM						},	/* ROM*/
	{ 0x800018, 0x80001b, zeropnt2_coins_r				},	/* Coins*/
	{ 0x800024, 0x800027, zeropnt2_oki0_r				},	/* Sound*/
	{ 0x80002c, 0x80002f, zeropnt2_ym2151_status_r		},	/**/
	{ 0x800030, 0x800033, zeropnt2_oki1_r				},	/**/
	{ 0x800140, 0x800143, zeropnt2_guny_0_msb_r			},	/* Light Guns*/
	{ 0x800144, 0x800147, zeropnt2_gunx_0_msb_r			},	/**/
	{ 0x800148, 0x80014b, zeropnt2_guny_1_msb_r			},	/**/
	{ 0x80014c, 0x80014f, zeropnt2_gunx_1_msb_r			},	/**/
	{ 0x800150, 0x800153, zeropnt2_dsw1_r				},	/* DSW*/
	{ 0x800154, 0x800157, zeropnt2_dsw2_r				},	/* DSW*/
	{ 0x80015c, 0x80015f, zeropnt2_buttons_r			},	/* Buttons*/
/**/{ 0x904000, 0x907fff, MRA32_RAM						},	/* Layers*/
/**/{ 0x908000, 0x90bfff, MRA32_RAM						},	/**/
/**/{ 0x90c000, 0x90ffff, MRA32_RAM						},	/**/
/**/{ 0x920000, 0x923fff, MRA32_RAM						},	/* ? 0*/
/**/{ 0x930000, 0x9307ff, MRA32_RAM						},	/* Sprites*/
/**/{ 0x940000, 0x947fff, MRA32_RAM						},	/* Palette*/
	{ 0xfe0000, 0xffffff, MRA32_RAM						},	/* RAM*/
MEMORY_END

static MEMORY_WRITE32_START( writemem_zeropnt2 )
	{ 0x000000, 0x1fffff, MWA32_ROM							},	/* ROM*/
	{ 0x800024, 0x800027, zeropnt2_oki0_w					},	/* Sound*/
	{ 0x800028, 0x80002b, zeropnt2_ym2151_reg_w				},	/**/
	{ 0x80002c, 0x80002f, zeropnt2_ym2151_data_w			},	/**/
	{ 0x800030, 0x800033, zeropnt2_oki1_w					},	/**/
	{ 0x800034, 0x800037, zeropnt2_sound_bank_w				},	/**/
	{ 0x800038, 0x80003b, zeropnt2_leds_w					},	/* ?*/
	{ 0x80010c, 0x800123, MWA32_RAM, &unico_scroll32		},	/* Scroll*/
	{ 0x8001e0, 0x8001e3, MWA32_RAM							},	/* ? IRQ Ack*/
	{ 0x8001f0, 0x8001f3, zeropnt2_eeprom_w					},	/* EEPROM*/
	{ 0x904000, 0x907fff, unico_vram32_1_w, &unico_vram32_1	},	/* Layers*/
	{ 0x908000, 0x90bfff, unico_vram32_2_w, &unico_vram32_2	},	/**/
	{ 0x90c000, 0x90ffff, unico_vram32_0_w, &unico_vram32_0	},	/**/
	{ 0x920000, 0x923fff, MWA32_RAM							},	/* ? 0*/
	{ 0x930000, 0x9307ff, MWA32_RAM, &spriteram32, &spriteram_size	},	/* Sprites*/
	{ 0x940000, 0x947fff, unico_palette32_w, &paletteram32	},	/* Palette*/
	{ 0xfe0000, 0xffffff, MWA32_RAM							},	/* RAM*/
MEMORY_END


/***************************************************************************


								Input Ports


***************************************************************************/

/***************************************************************************
								Burglar X
***************************************************************************/

INPUT_PORTS_START( burglarx )

	PORT_START	/* IN0 - $800000.w*/
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 - $800019.b*/
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_COIN1    )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_COIN2    )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START	/* IN2 - $80001a.b*/
	PORT_BIT(     0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown 1-2" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "Unknown 1-4" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0xe000, 0xe000, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_4C ) )

	PORT_START	/* IN3 - $80001c.b*/
	PORT_BIT(     0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0200, "None" )
	PORT_DIPSETTING(      0x0300, "A" )
	PORT_DIPSETTING(      0x0100, "B" )
	PORT_DIPSETTING(      0x0000, "C" )
	PORT_DIPNAME( 0x0400, 0x0400, "Unknown 2-2" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "Energy" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0800, "3" )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x2000, "Easy" )
	PORT_DIPSETTING(      0x3000, "Normal" )
	PORT_DIPSETTING(      0x1000, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x8000, "2" )
	PORT_DIPSETTING(      0xc000, "3" )
	PORT_DIPSETTING(      0x4000, "4" )
	PORT_DIPSETTING(      0x0000, "5" )

INPUT_PORTS_END



/***************************************************************************
								Zero Point
***************************************************************************/

INPUT_PORTS_START( zeropnt )

	PORT_START	/* IN0 - $800018.w*/
	PORT_BIT(  0x0001, IP_ACTIVE_HIGH, IPT_COIN1    )
	PORT_BIT(  0x0002, IP_ACTIVE_HIGH, IPT_COIN2    )
	PORT_BITX( 0x0004, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x0008, IP_ACTIVE_HIGH, IPT_UNKNOWN  )
	PORT_BIT(  0x0010, IP_ACTIVE_HIGH, IPT_START1   )
	PORT_BIT(  0x0020, IP_ACTIVE_HIGH, IPT_START2   )
	PORT_BIT(  0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_HIGH, IPT_SERVICE1 )

	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN  )


	PORT_START	/* IN1 - $80001a.b*/
	PORT_BIT(     0x00ff, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0000, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( On ) )
	PORT_DIPNAME( 0xe000, 0x0000, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 1C_4C ) )

	PORT_START	/* IN2 - $80001c.b*/
	PORT_BIT(     0x00ff, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0000, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( On ) )
	PORT_DIPNAME( 0x3000, 0x0000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x1000, "Easy" )
	PORT_DIPSETTING(      0x0000, "Normal" )
	PORT_DIPSETTING(      0x2000, "Hard" )
	PORT_DIPNAME( 0xc000, 0x0000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x4000, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPSETTING(      0x8000, "4" )
	PORT_DIPSETTING(      0xc000, "5" )

	PORT_START	/* IN3 - $800170.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 35, 15, 0, 0xff )

	PORT_START	/* IN4 - $800174.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 35, 15, 0, 0xff )

	PORT_START	/* IN5 - $800178.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 35, 15, 0, 0xff )

	PORT_START	/* IN6 - $80017c.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 35, 15, 0, 0xff )

INPUT_PORTS_END



/***************************************************************************
								Zero Point 2
***************************************************************************/

INPUT_PORTS_START( zeropnt2 )
	PORT_START	/* IN0 - $800019.b*/
	PORT_BIT(  0x0001, IP_ACTIVE_HIGH, IPT_COIN1    )
	PORT_BIT(  0x0002, IP_ACTIVE_HIGH, IPT_COIN2    )
	PORT_BITX( 0x0004, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0x0008, IP_ACTIVE_HIGH, IPT_UNKNOWN  )
	PORT_BIT(  0x0010, IP_ACTIVE_HIGH, IPT_START1   )
	PORT_BIT(  0x0020, IP_ACTIVE_HIGH, IPT_START2   )
	PORT_BIT(  0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN  )
	PORT_BIT(  0x0080, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT(  0xff00, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 - $800150.b*/
	PORT_BIT(     0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "? Coins To Continue ?" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Gun Reloading" )
	PORT_DIPSETTING(      0x0800, DEF_STR(No) )
	PORT_DIPSETTING(      0x0400, DEF_STR(Yes) )
	PORT_DIPSETTING(      0x0c00, "Factory Setting" )
/*	PORT_DIPSETTING(      0x0000, "unused?" )*/
	PORT_DIPNAME( 0x1000, 0x1000, "Language" )
	PORT_DIPSETTING(      0x1000, "English" )
	PORT_DIPSETTING(      0x0000, "Japanese" )
	PORT_DIPNAME( 0xe000, 0xe000, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x6000, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0xe000, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0xc000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0xa000, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( 1C_4C ) )

	PORT_START	/* IN2 - $800154.b*/
	PORT_BIT(     0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0100, 0x0100, "Korean Language" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c00, 0x1c00, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x1000, "2" )
	PORT_DIPSETTING(      0x0c00, "3" )
	PORT_DIPSETTING(      0x1c00, "4" )
	PORT_DIPSETTING(      0x1800, "5" )
	PORT_DIPSETTING(      0x1400, "6" )
/*	PORT_DIPSETTING(      0x0800, "4" )*/
/*	PORT_DIPSETTING(      0x0400, "4" )*/
/*	PORT_DIPSETTING(      0x0000, "4" )*/
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x8000, "Easy" )
	PORT_DIPSETTING(      0xc000, "Normal" )
	PORT_DIPSETTING(      0x4000, "Harder" )
	PORT_DIPSETTING(      0x0000, "Hardest" )

	PORT_START	/* IN3 - $800140.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 35, 15, 0, 0xff )

	PORT_START	/* IN4 - $800144.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 35, 15, 0, 0xff )

	PORT_START	/* IN5 - $800148.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER1, 35, 15, 0, 0xff )

	PORT_START	/* IN6 - $80014c.b*/
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER1, 35, 15, 0, 0xff )

	PORT_START	/* IN7 - $80015c.b*/
	PORT_BIT(  0x00ff, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x0100, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x1000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x2000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x4000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x8000, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* EEPROM*/
INPUT_PORTS_END



/***************************************************************************


							Graphics Layouts


***************************************************************************/

/* 16x16x8 */
static struct GfxLayout layout_16x16x8 =
{
	16,16,
	RGN_FRAC(1,4),
	8,
	{	RGN_FRAC(3,4)+8,	RGN_FRAC(3,4)+0,
		RGN_FRAC(2,4)+8,	RGN_FRAC(2,4)+0,
		RGN_FRAC(1,4)+8,	RGN_FRAC(1,4)+0,
		RGN_FRAC(0,4)+8,	RGN_FRAC(0,4)+0	},
	{	STEP8(0,1), 		STEP8(16,1)		},
	{	STEP16(0,16*2)						},
	16*16*2
};

static struct GfxDecodeInfo unico_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_16x16x8, 0x0, 0x20 }, /* [0] Sprites*/
	{ REGION_GFX2, 0, &layout_16x16x8, 0x0, 0x20 }, /* [1] Layers*/
	{ -1 }
};



/***************************************************************************


								Machine Drivers


***************************************************************************/

MACHINE_INIT( unico )
{
	unico_has_lightgun = 0;
}

static struct YM3812interface unico_ym3812_intf =
{
	1,
	3579545,		/* ? */ /*Would guess it as identical to zeropt2, any samples for confirmation?*/
	{ 40 },
	{ 0 },	/* IRQ Line */
};

static struct OKIM6295interface unico_m6295_intf =
{
	1,
	{ 8000 },		/* ? */
	{ REGION_SOUND1 },
	{ 80 }
};

static struct OKIM6295interface zeropnt2_m6295_intf =
{
	2,
	{ 8000, 30000 },		/* Appears to be spot on for both interfaces. */
	{ REGION_SOUND1, REGION_SOUND2 },
	{ MIXER(40,MIXER_PAN_LEFT), MIXER(20,MIXER_PAN_RIGHT) }
};

static struct YM2151interface zeropnt2_ym2151_intf =
{
	1,
	3579545, /* measured on actual PCB*/
	{ YM3012_VOL(70,MIXER_PAN_LEFT,70,MIXER_PAN_RIGHT) },
	{ 0 }
};


struct EEPROM_interface zeropnt2_eeprom_interface =
{
	7,				/* address bits	7*/
	8,				/* data bits	8*/
	"*110",			/* read			1 10 aaaaaaa*/
	"*101",			/* write		1 01 aaaaaaa dddddddd*/
	"*111",			/* erase		1 11 aaaaaaa*/
	"*10000xxxx",	/* lock			1 00 00xxxx*/
	"*10011xxxx",	/* unlock		1 00 11xxxx*/
/*	"*10001xxxx"	*/ /* write all	1 00 01xxxx dddddddd*/
/*	"*10010xxxx"	*/ /* erase all	1 00 10xxxx*/
};

void nvram_handler_zeropnt2(mame_file *file,int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&zeropnt2_eeprom_interface);
		if (file)	EEPROM_load(file);
	}
}


/***************************************************************************
								Burglar X
***************************************************************************/

static MACHINE_DRIVER_START( burglarx )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_MEMORY(readmem_burglarx,writemem_burglarx)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(unico)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(0x180, 0xe0)
	MDRV_VISIBLE_AREA(0, 0x180-1, 0, 0xe0-1)
	MDRV_GFXDECODE(unico_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(unico)
	MDRV_VIDEO_UPDATE(unico)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM3812, unico_ym3812_intf)
	MDRV_SOUND_ADD(OKIM6295, unico_m6295_intf)
MACHINE_DRIVER_END



/***************************************************************************
								Zero Point
***************************************************************************/

MACHINE_INIT( zeropt )
{
	machine_init_unico();
	unico_has_lightgun = 1;
}

static MACHINE_DRIVER_START( zeropnt )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_MEMORY(readmem_zeropnt,writemem_zeropnt)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(zeropt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(0x180, 0xe0)
	MDRV_VISIBLE_AREA(0, 0x180-1, 0, 0xe0-1)
	MDRV_GFXDECODE(unico_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(unico)
	MDRV_VIDEO_UPDATE(unico)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM3812, unico_ym3812_intf)
	MDRV_SOUND_ADD(OKIM6295, unico_m6295_intf)
MACHINE_DRIVER_END



/***************************************************************************
								Zero Point 2
***************************************************************************/

static MACHINE_DRIVER_START( zeropnt2 )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68EC020, 16000000)
	MDRV_CPU_MEMORY(readmem_zeropnt2,writemem_zeropnt2)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(zeropt)

	MDRV_NVRAM_HANDLER(zeropnt2)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(0x180, 0xe0)
	MDRV_VISIBLE_AREA(0, 0x180-1, 0, 0xe0-1)
	MDRV_GFXDECODE(unico_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(zeropnt2)
	MDRV_VIDEO_UPDATE(zeropnt2)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD( YM2151,		zeropnt2_ym2151_intf )
	MDRV_SOUND_ADD( OKIM6295,	zeropnt2_m6295_intf  )
MACHINE_DRIVER_END


/***************************************************************************


								ROMs Loading


***************************************************************************/



/***************************************************************************

								Burglar X

by Unico

68000-16MHz , MACH210-15JC, 3 x A1020B
14.31818 MHz, 32.000 MHz

***************************************************************************/

ROM_START( burglarx )

	ROM_REGION( 0x100000, REGION_CPU1, 0 )		/* 68000 Code */
	ROM_LOAD16_BYTE( "bx-rom2.pgm", 0x000000, 0x080000, CRC(f81120c8) SHA1(f0240cf9aceb755e3c920bc3bcae0a9de29fd8c1) )
	ROM_LOAD16_BYTE( "bx-rom3.pgm", 0x000001, 0x080000, CRC(080b4e82) SHA1(7eb08a7ea7684297e879123ae7ddc88d7fc1b87b) )

	/* Notice the weird ROMs order? Pretty much bit scrambling */
	ROM_REGION( 0x400000, REGION_GFX1, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Sprites */
	ROM_LOAD16_BYTE( "bx-rom4",  0x000000, 0x080000, CRC(f74ce31f) SHA1(bafe247a2fdc918318ccf7b11f0406c78909dcaa) )
	ROM_LOAD16_BYTE( "bx-rom10", 0x000001, 0x080000, CRC(6f56ca23) SHA1(5cfedda8d9fe4b575932a6a136d7b525d96e5454) )
	ROM_LOAD16_BYTE( "bx-rom9",  0x100000, 0x080000, CRC(33f29d79) SHA1(287d8412842887af5a5c7a0f5e5736a741c3c7db) )
	ROM_LOAD16_BYTE( "bx-rom8",  0x100001, 0x080000, CRC(24367092) SHA1(dc21d043a793cbc9fe94085c7884d684f1f80d74) )
	ROM_LOAD16_BYTE( "bx-rom7",  0x200000, 0x080000, CRC(aff6bdea) SHA1(3c050ec5e1bbc93b15435c7a6e66bade9a07445e) )
	ROM_LOAD16_BYTE( "bx-rom6",  0x200001, 0x080000, CRC(246afed2) SHA1(fcf08e968f11549546c47c1a67013c2427e0aad3) )
	ROM_LOAD16_BYTE( "bx-rom11", 0x300000, 0x080000, CRC(898d176a) SHA1(4c85948b7e639743d0f1676fdc463267f550f97c) )
	ROM_LOAD16_BYTE( "bx-rom5",  0x300001, 0x080000, CRC(fdee1423) SHA1(319610435b3dea61276d412e2bf6a3f32809ae19) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Layers */
	ROM_LOAD16_BYTE( "bx-rom14", 0x000000, 0x080000, CRC(30413373) SHA1(37bbc4d2943a32ee9f6bb268c823ffe162fe92a2) )
	ROM_LOAD16_BYTE( "bx-rom18", 0x000001, 0x080000, CRC(8e7fc99f) SHA1(81141e3c9111944aae97d27e5631b11eaf6f8734) )
	ROM_LOAD16_BYTE( "bx-rom19", 0x100000, 0x080000, CRC(d40eabcd) SHA1(e41d5e921a1648d6d4907f18e0256dbe3a01e9d3) )
	ROM_LOAD16_BYTE( "bx-rom15", 0x100001, 0x080000, CRC(78833c75) SHA1(93bd2e9ba98d99e36b99765ff576df4ca347daf3) )
	ROM_LOAD16_BYTE( "bx-rom17", 0x200000, 0x080000, CRC(f169633f) SHA1(3bb707110286890a740ef607fb2addeeaadedb08) )
	ROM_LOAD16_BYTE( "bx-rom12", 0x200001, 0x080000, CRC(71eb160f) SHA1(4fc8caabc5ee6c7771c76e704ffba675cf997dae) )
	ROM_LOAD16_BYTE( "bx-rom13", 0x300000, 0x080000, CRC(da34bbb5) SHA1(455c2412135b89670c2ecda9fd02f4da9b891ee4) )
	ROM_LOAD16_BYTE( "bx-rom16", 0x300001, 0x080000, CRC(55b28ef9) SHA1(48615d53ac955ba6aca86ad4f8b61f4d2675d840) )

	ROM_REGION( 0x80000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "bx-rom1.snd", 0x000000, 0x080000, CRC(8ae67138) SHA1(3ea44f805a1f978e0a1c1bb7f45507379b147bc0) )	/* 2 x 40000*/

ROM_END


/***************************************************************************

								Zero Point

(C) 1998 Unico

PCB Number: ZPM1001A
CPU: 68HC000P16
SND: AD-65, K-666
OSC: 14.31818MHz, 32.000MHz
RAM: 62256 x 5, 6116 x 8, 84256 x 2
DIPS: 2 x 8 position

Other Chips: 3 x Actel A1020B (square 84 pin socketed, Same video chip as Power Instinct and Blomby Car)
             MACH211 (square 44 pin socketed)

There is a small gun interface board (Number ZPT1001B) located near the 68000 which contains
another Actel A1020B chip, a 74HC14 TTL chip and a 4.9152MHz OSC.

ROMS:
zero2.BIN  \
zero3.BIN  / Main Program 4M Mask ROMs
zero1.BIN  -- Sound MX27C4000
zpobjz01.BIN -\
zpobjz02.BIN   \
zpobjz03.BIN    \
zpobjz04.BIN     \
zpscrz05.BIN      - GFX,16M Mask ROMs
zpscrz06.BIN     /
zpscrz07.BIN    /
zpscrz08.BIN  -/

***************************************************************************/

ROM_START( zeropnt )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )		/* 68000 Code */
	ROM_LOAD16_BYTE( "zero_2.bin", 0x000000, 0x080000, CRC(1e599509) SHA1(5a562a3c85700126b95fbdf21ef8c0ddd35d9037) )
	ROM_LOAD16_BYTE( "zero_3.bin", 0x000001, 0x080000, CRC(588aeef7) SHA1(0dfa22c9e7b1fe493c16160b1ac76fa4d3bb2e68) )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Sprites */
	ROM_LOAD( "zpobjz01.bin", 0x000000, 0x200000, CRC(1f2768a3) SHA1(75c83458afc527dda47bfbd86a8e9c5ded7a5444) )
	ROM_LOAD( "zpobjz02.bin", 0x200000, 0x200000, CRC(de34f33a) SHA1(b77c7d508942176585afaeeaea2f34f60326eeb1) )
	ROM_LOAD( "zpobjz03.bin", 0x400000, 0x200000, CRC(d7a657f7) SHA1(f1f9e6a01eef4d0c8c4b2e161136cc4438d770e2) )
	ROM_LOAD( "zpobjz04.bin", 0x600000, 0x200000, CRC(3aec2f8d) SHA1(6fb1cfabfb0bddf688d3bfb60f7538209efbd8f1) )

	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Layers */
	ROM_LOAD( "zpscrz06.bin", 0x000000, 0x200000, CRC(e1e53cf0) SHA1(b440e09f6229d486d1a8be476ac8a17adde1ff7e) )
	ROM_LOAD( "zpscrz05.bin", 0x200000, 0x200000, CRC(0d7d4850) SHA1(43f87d0461fe022b68b4e57e6c9542bcd78e301b) )
	ROM_LOAD( "zpscrz07.bin", 0x400000, 0x200000, CRC(bb178f32) SHA1(1354f4d90a8cec58d1f2b6809985776b309b96a8) )
	ROM_LOAD( "zpscrz08.bin", 0x600000, 0x200000, CRC(672f02e5) SHA1(8e8b28a8b2293950764d453a3c385d7083eb5a57) )

	ROM_REGION( 0x80000 * 2, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "zero_1.bin", 0x000000, 0x080000, CRC(fd2384fa) SHA1(8ae83665fe952c5d03bd62d2abb507c351cf0fb5) )
	ROM_RELOAD(            0x080000, 0x080000             )
ROM_END


ROM_START( zeropnta )
	ROM_REGION( 0x100000, REGION_CPU1, 0 )		/* 68000 Code */
	ROM_LOAD16_BYTE( "zpa2.bin", 0x000000, 0x080000, CRC(285fbca3) SHA1(61f8d48388a666ed9300c0688fbf844e316b8892) )
	ROM_LOAD16_BYTE( "zpa3.bin", 0x000001, 0x080000, CRC(ad7b3129) SHA1(d814b5d9336d011386aa0b316b11225e5ea799fc) )

	ROM_REGION( 0x800000, REGION_GFX1, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Sprites */
	ROM_LOAD( "zpobjz01.bin", 0x000000, 0x200000, CRC(1f2768a3) SHA1(75c83458afc527dda47bfbd86a8e9c5ded7a5444) )
	ROM_LOAD( "zpobjz02.bin", 0x200000, 0x200000, CRC(de34f33a) SHA1(b77c7d508942176585afaeeaea2f34f60326eeb1) )
	ROM_LOAD( "zpobjz03.bin", 0x400000, 0x200000, CRC(d7a657f7) SHA1(f1f9e6a01eef4d0c8c4b2e161136cc4438d770e2) )
	ROM_LOAD( "zpobjz04.bin", 0x600000, 0x200000, CRC(3aec2f8d) SHA1(6fb1cfabfb0bddf688d3bfb60f7538209efbd8f1) )

	ROM_REGION( 0x800000, REGION_GFX2, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Layers */
	ROM_LOAD( "zpscrz06.bin", 0x000000, 0x200000, CRC(e1e53cf0) SHA1(b440e09f6229d486d1a8be476ac8a17adde1ff7e) )
	ROM_LOAD( "zpscrz05.bin", 0x200000, 0x200000, CRC(0d7d4850) SHA1(43f87d0461fe022b68b4e57e6c9542bcd78e301b) )
	ROM_LOAD( "zpscrz07.bin", 0x400000, 0x200000, CRC(bb178f32) SHA1(1354f4d90a8cec58d1f2b6809985776b309b96a8) )
	ROM_LOAD( "zpscrz08.bin", 0x600000, 0x200000, CRC(672f02e5) SHA1(8e8b28a8b2293950764d453a3c385d7083eb5a57) )

	ROM_REGION( 0x80000 * 2, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "zero_1.bin", 0x000000, 0x080000, CRC(fd2384fa) SHA1(8ae83665fe952c5d03bd62d2abb507c351cf0fb5) )
	ROM_RELOAD(            0x080000, 0x080000             )
ROM_END

/***************************************************************************

									Zero Point 2

(c) 1999 Unico

PCB Number: UZP21001A
CPU: MC68EC020FG16
SND: AD-65 x 2
OSC: 3.579545MHz (near AD-65), 32.000MHz, 40.000MHz (near 68020)
RAM: 62256B x 9

Other Chips: 3 x Actel A40MX04-F (square 84 pin socketed)
             MACH211 (square 44 pin socketed)

There is a small gun interface board (Number UZP21001B) located above the 68020 which contains:

   OSC: 4.9152MHz
  DIPS: 2 x 8 position
EEPROM: ST 93C46
 OTHER: Actel A40MX04-F chip
        74HC14 TTL chip
        4-pin gun header x 2

ROMS:
D0-D15.3  \ Main Program 8M Mask ROMs
D16-D31.4 /
uzp2-1.bin - Sound 27C040
uzp2-2.bin - Sound 27C020

A0-A1ZP.205  -\
A2-A3ZP.206    \
A6-A7ZP.207     \
A4-A5ZP.208      \
DB0DB1ZP.209      - GFX,32M Mask ROMs
DB2DB3ZP.210     /
DB4DB5ZP.211    /
DB6DB7ZP.212  -/

                     Zero Point 2 board JAMMA Pinout

                          Main Jamma Connector
            Solder Side          |             Parts Side
------------------------------------------------------------------
             GND             | A | 1 |             GND
             GND             | B | 2 |             GND
             +5              | C | 3 |             +5
             +5              | D | 4 |             +5
                             | E | 5 |
             +12             | F | 6 |             +12
------------ KEY ------------| H | 7 |------------ KEY -----------
                             | J | 8 |      Coin Counter # 1
       Player 2 Lamp         | K | 9 |       Player 1 Lamp
        Speaker (L)          | L | 10|        Speaker (R)
                             | M | 11|
        Video Green          | N | 12|        Video Red
        Video Sync           | P | 13|        Video Blue
       Service Switch        | R | 14|        Video GND
                             | S | 15|        Test Switch
        Coin Switch 2        | T | 16|         Coin Switch 1
       Player 2 Start        | U | 17|        Player 1 Start
                             | V | 18|
                             | W | 19|
                             | X | 20|
                             | Y | 21|
                             | Z | 22|
                             | a | 23|
                             | b | 24|
                             | c | 25|
                             | d | 26|
             GND             | e | 27|             GND
             GND             | f | 28|             GND



SPECIAL NOTICE - Sound wiring change:

For cabinets with one speaker:
	JAMMA pin 10 goes to speaker (+)
	Run a ground to the negative side of speaker.
For cabinets with two speakers:
	JAMMA pin 10 goes to right speaker (+)
	JAMMA pin L goes to left speaker (+)
	Run a ground to the negative side of each speaker.

Using Original Unico Light Guns & connectors:

 1PLAY: Left (Red) Gun Connector Pinout*

   1| Gun OPTO - White Wire
   2| +5 Volts - Red Wire
   3| Trigger  - Green Wire
   4| Ground   - Black Wire

 2PLAY: Right (Blue) Gun Connector Pinout*

   1| Gun OPTO - White Wire
   2| +5 Volts - Red Wire
   3| Trigger  - Green Wire
   4| Ground   - Black Wire

* This is not the same as the HAPP Controls' 4-pin standard


DIPSW-A
------------------------------------------------------------------
    DipSwitch Title   | Function | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
------------------------------------------------------------------
    Free Play         |   Off    |off|                           |*
                      |   On     |on |                           |
------------------------------------------------------------------
  1 Coin to Continue  |   Off    |   |off|                       |*
                      |   On     |   |on |                       |
------------------------------------------------------------------
                      | Factory  |       |off|off|               |*
   Gun Loading Mode   |Not Reload|       |on |off|               |
                      | Reload   |       |off|on |               |
------------------------------------------------------------------
      Language        | English  |               |off|           |*
                      | Japanese |               |on |           |
------------------------------------------------------------------
                      | 1cn/1pl  |                   |off|off|off|*
                      | 1cn/2pl  |                   |on |off|off|
                      | 1cn/3pl  |                   |off|on |off|
        Coinage       | 1cn/4pl  |                   |on |on |off|
                      | 2cn/1pl  |                   |off|off|on |
                      | 3cn/1pl  |                   |on |off|on |
                      | 4cn/1pl  |                   |off|on |on |
                      | 5cn/1pl  |                   |on |on |on |
------------------------------------------------------------------

DIPSW-B
------------------------------------------------------------------
    DipSwitch Title   | Function | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
------------------------------------------------------------------
   Korean Language    |   Off    |off|                           |*
                      |   On     |on |                           |
------------------------------------------------------------------
     Demo Sounds      |   Off    |   |off|                       |
                      |   On     |   |on |                       |*
------------------------------------------------------------------
                      |    4     |       |off|off|off|           |*
   Player's Heart     |    5     |       |on |off|off|           |
       (Lives)        |    6     |       |off|on |off|           |
                      |    2     |       |on |on |off|           |
                      |    3     |       |off|off|on |           |
------------------------------------------------------------------
  Not Used / Always Off                              |off|       |*
------------------------------------------------------------------
                      |  Normal  |                       |off|off|*
      Difficulty      |   Easy   |                       |on |off|
        Level         |   Hard   |                       |off|on |
                      |  V.Hard  |                       |on |on |
------------------------------------------------------------------

* Denotes Factory Defualts


BrianT

***************************************************************************/

ROM_START( zeropnt2 )
	ROM_REGION( 0x200000, REGION_CPU1, 0 )		/* 68020 Code */
	ROM_LOAD32_WORD_SWAP( "d16-d31.4", 0x000000, 0x100000, CRC(48314fdb) SHA1(a5bdb6a3f520587ff5e73438dc414cfdff34167b) )
	ROM_LOAD32_WORD_SWAP( "d0-d15.3",  0x000002, 0x100000, CRC(5ec4151e) SHA1(f7c857bdb6a92f76f09a089b37def7e6cf24b65a) )

	ROM_REGION( 0x1000000, REGION_GFX1, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Sprites */
	ROM_LOAD( "db0db1zp.209", 0x000000, 0x400000, CRC(474b460c) SHA1(72104b7a00cb6d62b3cee2cfadc928669ca948c4) )
	ROM_LOAD( "db2db3zp.210", 0x400000, 0x400000, CRC(0a1d0a88) SHA1(b0a6ba9eba539fff417557c9af60d408c2912491) )
	ROM_LOAD( "db4db5zp.211", 0x800000, 0x400000, CRC(227169dc) SHA1(b03d8d46714e5aa3631fde7d65466334dafdc341) )
	ROM_LOAD( "db6db7zp.212", 0xc00000, 0x400000, CRC(a6306cdb) SHA1(da48c5981b72b87df40602e03e56a40a24728262) )

	ROM_REGION( 0x1000000, REGION_GFX2, ROMREGION_INVERT | ROMREGION_DISPOSE )	/* 16x16x8 Layers */
	ROM_LOAD( "a0-a1zp.205", 0x000000, 0x400000, CRC(f7ca9c0e) SHA1(541139b617ff34c378a506cf88fe97234c93ee20) )
	ROM_LOAD( "a2-a3zp.206", 0x400000, 0x400000, CRC(0581c8fe) SHA1(9bbffc9c758bbaba2b43a63811b725e51996268a) )
	ROM_LOAD( "a4-a5zp.208", 0x800000, 0x400000, CRC(ddd091ef) SHA1(c1751aef2546a35f2fdbfeca9647a88fd3e65cdd) )
	ROM_LOAD( "a6-a7zp.207", 0xc00000, 0x400000, CRC(3fd46113) SHA1(326684b92c258bde318693cd9b3a7660aed3cd6f) )

	ROM_REGION( 0x80000 * 2, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "uzp2-1.bin", 0x000000, 0x080000, CRC(ed0966ed) SHA1(a43b9c493f94d1fb11e1b189caaf37d3d792c730) )
	ROM_RELOAD(             0x080000, 0x080000             )

	ROM_REGION( 0x40000, REGION_SOUND2, 0 )	/* Samples */
	ROM_LOAD( "uzp2-2.bin", 0x000000, 0x040000, CRC(db8cb455) SHA1(6723b4018208d554bd1bf1e0640b72d2f4f47302) )
ROM_END


/***************************************************************************


								Game Drivers


***************************************************************************/

GAME( 1997, burglarx, 0,       burglarx, burglarx, 0, ROT0, "Unico", "Burglar X"  )
GAME( 1998, zeropnt,  0,       zeropnt,  zeropnt,  0, ROT0, "Unico", "Zero Point (set 1)" )
GAME( 1998, zeropnta, zeropnt, zeropnt,  zeropnt,  0, ROT0, "Unico", "Zero Point (set 2)" )
GAME( 1999, zeropnt2, 0,       zeropnt2, zeropnt2, 0, ROT0, "Unico", "Zero Point 2" )
