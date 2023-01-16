/***************************************************************************

Various Video System Co. games using the C7-01 GGA, VS8803, VS8904, VS8905
video chips.
C7-01 GGA is used in a lot of games, some of them without sprites. So it
either controls tilemaps, or the video signal, or both.
I think 8904/8905 handle sprites, don't know about 8803.
tail2nos doesn't have the 8904/8905, and indeed it has a different sprite
system.

Driver by Nicola Salmoria


Notes:
- Sprite zoom is probably not 100% accurate.
  In pspikes, the zooming text during attract mode is horrible.

pspikes/turbofrc/aerofgtb write to two addresses which look like control
registers for a video generator. Maybe they control the display size/position.
aerofgt is different, it writes to consecutive memory addresses and the values
it writes don't seem to be related to these ones.

                  00 01 02 03 04 05  08 09 0a 0b 0c 0d
                  ------------------------------------
pspikes  352x240? 57 63 69 71 1f 00  77 79 7b 7f 1f 00
karatblz 352x240  57 63 69 71 1f 00  77 79 7b 7f 1f 00
turbofrc 352x240  57 63 69 71 1f 00  77 79 7b 7f 1f 00
spinlbrk 352x240  57 68 6f 75 ff 01  77 78 7b 7f ff 00
aerofgtb 320x224  4f 5d 63 71 1f 00  6f 70 72 7c 1f 02
tail2nos 320x240  4f 5e 64 71 1f 09  7a 7c 7e 7f 1f 02
f1gp     320x240  4f 5e 64 71 1f 09  7a 7c 7e 7f 1f 02
welltris 352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00

games with 8x4 tiles:

pipedrm  352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff
hatris   352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff
idolmj   352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00
mjnatsu  352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff
mfunclub 352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff
daiyogen 352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff
nmsengen 352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff
fromance 352x240  57 63 69 71 1f 00  7a 7b 7e 7f 1f 00 * register 0b also briefly toggled to ff

register 00 could be screen width / 4 (hblank start?)
register 08 could be screen height / 2 (vblank start?)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


extern data16_t *aerofgt_rasterram;
extern data16_t *aerofgt_bg1videoram,*aerofgt_bg2videoram;
extern data16_t *aerofgt_spriteram1,*aerofgt_spriteram2,*aerofgt_spriteram3;
extern size_t aerofgt_spriteram1_size,aerofgt_spriteram2_size,aerofgt_spriteram3_size;

WRITE16_HANDLER( aerofgt_bg1videoram_w );
WRITE16_HANDLER( aerofgt_bg2videoram_w );
WRITE16_HANDLER( pspikes_gfxbank_w );
WRITE16_HANDLER( karatblz_gfxbank_w );
WRITE16_HANDLER( spinlbrk_gfxbank_w );
WRITE16_HANDLER( turbofrc_gfxbank_w );
WRITE16_HANDLER( aerofgt_gfxbank_w );
WRITE16_HANDLER( aerofgt_bg1scrollx_w );
WRITE16_HANDLER( aerofgt_bg1scrolly_w );
WRITE16_HANDLER( aerofgt_bg2scrollx_w );
WRITE16_HANDLER( aerofgt_bg2scrolly_w );
WRITE16_HANDLER( pspikes_palette_bank_w );
VIDEO_START( pspikes );
VIDEO_START( karatblz );
VIDEO_START( spinlbrk );
VIDEO_START( turbofrc );
VIDEO_UPDATE( pspikes );
VIDEO_UPDATE( karatblz );
VIDEO_UPDATE( spinlbrk );
VIDEO_UPDATE( turbofrc );
VIDEO_UPDATE( aerofgt );




static int pending_command;

static WRITE16_HANDLER( sound_command_w )
{
	if (ACCESSING_LSB)
	{
		pending_command = 1;
		soundlatch_w(offset,data & 0xff);
		cpu_set_irq_line(1, IRQ_LINE_NMI, PULSE_LINE);
	}
}

static WRITE16_HANDLER( turbofrc_sound_command_w )
{
	if (ACCESSING_MSB)
	{
		pending_command = 1;
		soundlatch_w(offset,(data >> 8) & 0xff);
		cpu_set_irq_line(1, IRQ_LINE_NMI, PULSE_LINE);
	}
}

static READ16_HANDLER( pending_command_r )
{
	return pending_command;
}

static WRITE_HANDLER( pending_command_clear_w )
{
	pending_command = 0;
}

static WRITE_HANDLER( aerofgt_sh_bankswitch_w )
{
	data8_t *rom = memory_region(REGION_CPU2) + 0x10000;

	cpu_setbank(1,rom + (data & 0x03) * 0x8000);
}

static MACHINE_INIT( aerofgt )
{
	aerofgt_sh_bankswitch_w(0,0);	/* needed by spinlbrk */
}



static MEMORY_READ16_START( pspikes_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x100000, 0x10ffff, MRA16_RAM },
	{ 0x200000, 0x203fff, MRA16_RAM },
	{ 0xff8000, 0xff8fff, MRA16_RAM },
	{ 0xffd000, 0xffdfff, MRA16_RAM },
	{ 0xffe000, 0xffefff, MRA16_RAM },
	{ 0xfff000, 0xfff001, input_port_0_word_r },
	{ 0xfff002, 0xfff003, input_port_1_word_r },
	{ 0xfff004, 0xfff005, input_port_2_word_r },
	{ 0xfff006, 0xfff007, pending_command_r },
MEMORY_END

static MEMORY_WRITE16_START( pspikes_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x100000, 0x10ffff, MWA16_RAM },	/* work RAM */
	{ 0x200000, 0x203fff, MWA16_RAM, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0xff8000, 0xff8fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0xffc000, 0xffc3ff, MWA16_RAM, &aerofgt_spriteram3, &aerofgt_spriteram3_size },
	{ 0xffd000, 0xffdfff, MWA16_RAM, &aerofgt_rasterram },	/* bg1 scroll registers */
	{ 0xffe000, 0xffefff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0xfff000, 0xfff001, pspikes_palette_bank_w },
	{ 0xfff002, 0xfff003, pspikes_gfxbank_w },
	{ 0xfff004, 0xfff005, aerofgt_bg1scrolly_w },
	{ 0xfff006, 0xfff007, sound_command_w },
MEMORY_END

static MEMORY_READ16_START( karatblz_readmem )
	MEMORY_ADDRESS_BITS(20)
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x080000, 0x081fff, MRA16_RAM },
	{ 0x082000, 0x083fff, MRA16_RAM },
	{ 0x0a0000, 0x0affff, MRA16_RAM },
	{ 0x0b0000, 0x0bffff, MRA16_RAM },
	{ 0x0c0000, 0x0cffff, MRA16_RAM },	/* work RAM */
	{ 0x0f8000, 0x0fbfff, MRA16_RAM },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, MRA16_RAM },
	{ 0x0fe000, 0x0fe7ff, MRA16_RAM },
	{ 0x0ff000, 0x0ff001, input_port_0_word_r },
	{ 0x0ff002, 0x0ff003, input_port_1_word_r },
	{ 0x0ff004, 0x0ff005, input_port_2_word_r },
	{ 0x0ff006, 0x0ff007, input_port_3_word_r },
	{ 0x0ff008, 0x0ff009, input_port_4_word_r },
	{ 0x0ff00a, 0x0ff00b, pending_command_r },
MEMORY_END

static MEMORY_WRITE16_START( karatblz_writemem )
	MEMORY_ADDRESS_BITS(20)
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x080000, 0x081fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x082000, 0x083fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x0a0000, 0x0affff, MWA16_RAM, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x0b0000, 0x0bffff, MWA16_RAM, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x0c0000, 0x0cffff, MWA16_RAM },	/* work RAM */
	{ 0x0f8000, 0x0fbfff, MWA16_RAM },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, MWA16_RAM, &aerofgt_spriteram3, &aerofgt_spriteram3_size },
	{ 0x0fe000, 0x0fe7ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0x0ff002, 0x0ff003, karatblz_gfxbank_w },
	{ 0x0ff006, 0x0ff007, sound_command_w },
	{ 0x0ff008, 0x0ff009, aerofgt_bg1scrollx_w },
	{ 0x0ff00a, 0x0ff00b, aerofgt_bg1scrolly_w },
	{ 0x0ff00c, 0x0ff00d, aerofgt_bg2scrollx_w },
	{ 0x0ff00e, 0x0ff00f, aerofgt_bg2scrolly_w },
MEMORY_END

static MEMORY_READ16_START( spinlbrk_readmem )
	{ 0x000000, 0x03ffff, MRA16_ROM },
	{ 0x080000, 0x080fff, MRA16_RAM },
	{ 0x082000, 0x082fff, MRA16_RAM },
	{ 0xff8000, 0xffbfff, MRA16_RAM },
	{ 0xffc000, 0xffc7ff, MRA16_RAM },
	{ 0xffd000, 0xffd1ff, MRA16_RAM },
	{ 0xffe000, 0xffe7ff, MRA16_RAM },
	{ 0xfff000, 0xfff001, input_port_0_word_r },
	{ 0xfff002, 0xfff003, input_port_1_word_r },
	{ 0xfff004, 0xfff005, input_port_2_word_r },
MEMORY_END

static MEMORY_WRITE16_START( spinlbrk_writemem )
	{ 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x080000, 0x080fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x082000, 0x082fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0xff8000, 0xffbfff, MWA16_RAM },	/* work RAM */
	{ 0xffc000, 0xffc7ff, MWA16_RAM, &aerofgt_spriteram3, &aerofgt_spriteram3_size },
	{ 0xffd000, 0xffd1ff, MWA16_RAM, &aerofgt_rasterram },	/* bg1 scroll registers */
	{ 0xffe000, 0xffe7ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0xfff000, 0xfff001, spinlbrk_gfxbank_w },
	{ 0xfff002, 0xfff003, aerofgt_bg2scrollx_w },
	{ 0xfff006, 0xfff007, sound_command_w },
MEMORY_END

static MEMORY_READ16_START( turbofrc_readmem )
	MEMORY_ADDRESS_BITS(20)
	{ 0x000000, 0x0bffff, MRA16_ROM },
	{ 0x0c0000, 0x0cffff, MRA16_RAM },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, MRA16_RAM },
	{ 0x0d2000, 0x0d3fff, MRA16_RAM },
	{ 0x0e0000, 0x0e3fff, MRA16_RAM },
	{ 0x0e4000, 0x0e7fff, MRA16_RAM },
	{ 0x0f8000, 0x0fbfff, MRA16_RAM },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, MRA16_RAM },
	{ 0x0fd000, 0x0fdfff, MRA16_RAM },
	{ 0x0fe000, 0x0fe7ff, MRA16_RAM },
	{ 0x0ff000, 0x0ff001, input_port_0_word_r },
	{ 0x0ff002, 0x0ff003, input_port_1_word_r },
	{ 0x0ff004, 0x0ff005, input_port_2_word_r },
	{ 0x0ff006, 0x0ff007, pending_command_r },
	{ 0x0ff008, 0x0ff009, input_port_3_word_r },
MEMORY_END

static MEMORY_WRITE16_START( turbofrc_writemem )
	MEMORY_ADDRESS_BITS(20)
	{ 0x000000, 0x0bffff, MWA16_ROM },
	{ 0x0c0000, 0x0cffff, MWA16_RAM },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x0d2000, 0x0d3fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x0e0000, 0x0e3fff, MWA16_RAM, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x0e4000, 0x0e7fff, MWA16_RAM, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x0f8000, 0x0fbfff, MWA16_RAM },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, MWA16_RAM, &aerofgt_spriteram3, &aerofgt_spriteram3_size },
	{ 0x0fd000, 0x0fdfff, MWA16_RAM, &aerofgt_rasterram },	/* bg1 scroll registers */
	{ 0x0fe000, 0x0fe7ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0x0ff002, 0x0ff003, aerofgt_bg1scrolly_w },
	{ 0x0ff004, 0x0ff005, aerofgt_bg2scrollx_w },
	{ 0x0ff006, 0x0ff007, aerofgt_bg2scrolly_w },
	{ 0x0ff008, 0x0ff00b, turbofrc_gfxbank_w },
	{ 0x0ff00c, 0x0ff00d, MWA16_NOP },	/* related to bg2 (written together with the scroll registers) */
	{ 0x0ff00e, 0x0ff00f, turbofrc_sound_command_w },
MEMORY_END

static MEMORY_READ16_START( aerofgtb_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x0c0000, 0x0cffff, MRA16_RAM },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, MRA16_RAM },
	{ 0x0d2000, 0x0d3fff, MRA16_RAM },
	{ 0x0e0000, 0x0e3fff, MRA16_RAM },
	{ 0x0e4000, 0x0e7fff, MRA16_RAM },
	{ 0x0f8000, 0x0fbfff, MRA16_RAM },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, MRA16_RAM },
	{ 0x0fd000, 0x0fd7ff, MRA16_RAM },
	{ 0x0fe000, 0x0fe001, input_port_0_word_r },
	{ 0x0fe002, 0x0fe003, input_port_1_word_r },
	{ 0x0fe004, 0x0fe005, input_port_2_word_r },
	{ 0x0fe006, 0x0fe007, pending_command_r },
	{ 0x0fe008, 0x0fe009, input_port_3_word_r },
	{ 0x0ff000, 0x0fffff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( aerofgtb_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x0c0000, 0x0cffff, MWA16_RAM },	/* work RAM */
	{ 0x0d0000, 0x0d1fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x0d2000, 0x0d3fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x0e0000, 0x0e3fff, MWA16_RAM, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x0e4000, 0x0e7fff, MWA16_RAM, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x0f8000, 0x0fbfff, MWA16_RAM },	/* work RAM */
	{ 0x0fc000, 0x0fc7ff, MWA16_RAM, &aerofgt_spriteram3, &aerofgt_spriteram3_size },
	{ 0x0fd000, 0x0fd7ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0x0fe002, 0x0fe003, aerofgt_bg1scrolly_w },
	{ 0x0fe004, 0x0fe005, aerofgt_bg2scrollx_w },
	{ 0x0fe006, 0x0fe007, aerofgt_bg2scrolly_w },
	{ 0x0fe008, 0x0fe00b, turbofrc_gfxbank_w },
	{ 0x0fe00e, 0x0fe00f, turbofrc_sound_command_w },
	{ 0x0ff000, 0x0fffff, MWA16_RAM, &aerofgt_rasterram },	/* used only for the scroll registers */
MEMORY_END

static MEMORY_READ16_START( aerofgt_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x1a0000, 0x1a07ff, MRA16_RAM },
	{ 0x1b0000, 0x1b07ff, MRA16_RAM },
	{ 0x1b0800, 0x1b0801, MRA16_NOP },	/* ??? */
	{ 0x1b0ff0, 0x1b0fff, MRA16_RAM },	/* stack area during boot */
	{ 0x1b2000, 0x1b3fff, MRA16_RAM },
	{ 0x1b4000, 0x1b5fff, MRA16_RAM },
	{ 0x1c0000, 0x1c3fff, MRA16_RAM },
	{ 0x1c4000, 0x1c7fff, MRA16_RAM },
	{ 0x1d0000, 0x1d1fff, MRA16_RAM },
	{ 0xfef000, 0xffefff, MRA16_RAM },	/* work RAM */
	{ 0xffffa0, 0xffffa1, input_port_0_word_r },
	{ 0xffffa2, 0xffffa3, input_port_1_word_r },
	{ 0xffffa4, 0xffffa5, input_port_2_word_r },
	{ 0xffffa6, 0xffffa7, input_port_3_word_r },
	{ 0xffffa8, 0xffffa9, input_port_4_word_r },
	{ 0xffffac, 0xffffad, pending_command_r },
	{ 0xffffae, 0xffffaf, input_port_5_word_r },
MEMORY_END

static MEMORY_WRITE16_START( aerofgt_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x1a0000, 0x1a07ff, paletteram16_xRRRRRGGGGGBBBBB_word_w, &paletteram16 },
	{ 0x1b0000, 0x1b07ff, MWA16_RAM, &aerofgt_rasterram },	/* used only for the scroll registers */
	{ 0x1b0800, 0x1b0801, MWA16_NOP },	/* ??? */
	{ 0x1b0ff0, 0x1b0fff, MWA16_RAM },	/* stack area during boot */
	{ 0x1b2000, 0x1b3fff, aerofgt_bg1videoram_w, &aerofgt_bg1videoram },
	{ 0x1b4000, 0x1b5fff, aerofgt_bg2videoram_w, &aerofgt_bg2videoram },
	{ 0x1c0000, 0x1c3fff, MWA16_RAM, &aerofgt_spriteram1, &aerofgt_spriteram1_size },
	{ 0x1c4000, 0x1c7fff, MWA16_RAM, &aerofgt_spriteram2, &aerofgt_spriteram2_size },
	{ 0x1d0000, 0x1d1fff, MWA16_RAM, &aerofgt_spriteram3, &aerofgt_spriteram3_size },
	{ 0xfef000, 0xffefff, MWA16_RAM },	/* work RAM */
	{ 0xffff80, 0xffff87, aerofgt_gfxbank_w },
	{ 0xffff88, 0xffff89, aerofgt_bg1scrolly_w },	/* + something else in the top byte */
	{ 0xffff90, 0xffff91, aerofgt_bg2scrolly_w },	/* + something else in the top byte */
	{ 0xffffac, 0xffffad, MWA16_NOP },	/* ??? */
	{ 0xffffc0, 0xffffc1, sound_command_w },
MEMORY_END



static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x77ff, MRA_ROM },
	{ 0x7800, 0x7fff, MRA_RAM },
	{ 0x8000, 0xffff, MRA_BANK1 },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x77ff, MWA_ROM },
	{ 0x7800, 0x7fff, MWA_RAM },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

static PORT_READ_START( turbofrc_sound_readport )
	{ 0x14, 0x14, soundlatch_r },
	{ 0x18, 0x18, YM2610_status_port_0_A_r },
	{ 0x1a, 0x1a, YM2610_status_port_0_B_r },
PORT_END

static PORT_WRITE_START( turbofrc_sound_writeport )
	{ 0x00, 0x00, aerofgt_sh_bankswitch_w },
	{ 0x14, 0x14, pending_command_clear_w },
	{ 0x18, 0x18, YM2610_control_port_0_A_w },
	{ 0x19, 0x19, YM2610_data_port_0_A_w },
	{ 0x1a, 0x1a, YM2610_control_port_0_B_w },
	{ 0x1b, 0x1b, YM2610_data_port_0_B_w },
PORT_END

static PORT_READ_START( aerofgt_sound_readport )
	{ 0x00, 0x00, YM2610_status_port_0_A_r },
	{ 0x02, 0x02, YM2610_status_port_0_B_r },
	{ 0x0c, 0x0c, soundlatch_r },
PORT_END

static PORT_WRITE_START( aerofgt_sound_writeport )
	{ 0x00, 0x00, YM2610_control_port_0_A_w },
	{ 0x01, 0x01, YM2610_data_port_0_A_w },
	{ 0x02, 0x02, YM2610_control_port_0_B_w },
	{ 0x03, 0x03, YM2610_data_port_0_B_w },
	{ 0x04, 0x04, aerofgt_sh_bankswitch_w },
	{ 0x08, 0x08, pending_command_clear_w },
PORT_END



INPUT_PORTS_START( pspikes )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0003, 0x0003, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_2C ) )
	/* the following two select country in the Chinese version (ROMs not available) */
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0600, 0x0600, "1 Player Starting Score" )
	PORT_DIPSETTING(      0x0600, "12-12" )
	PORT_DIPSETTING(      0x0400, "11-11" )
	PORT_DIPSETTING(      0x0200, "11-12" )
	PORT_DIPSETTING(      0x0000, "10-12" )
	PORT_DIPNAME( 0x1800, 0x1800, "2 Players Starting Score" )
	PORT_DIPSETTING(      0x1800, "9-9" )
	PORT_DIPSETTING(      0x1000, "7-7" )
	PORT_DIPSETTING(      0x0800, "5-5" )
	PORT_DIPSETTING(      0x0000, "0-0" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x2000, "Normal" )
	PORT_DIPSETTING(      0x0000, "Hard" )
	PORT_DIPNAME( 0x4000, 0x4000, "2 Players Time per Credit" )
	PORT_DIPSETTING(      0x4000, "3 min" )
	PORT_DIPSETTING(      0x0000, "2 min" )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( karatblz )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER4 )

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0008, 0x0008, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPNAME( 0x0060, 0x0020, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0060, "2 Players" )
	PORT_DIPSETTING(      0x0040, "3 Players" )
	PORT_DIPSETTING(      0x0020, "4 Players" )
	PORT_DIPSETTING(      0x0000, "4 Players (Team)" )
	/*  With 4 player (Team) selected and Same Coin Slot:
		Coin A & B credit together for use by _only_ player 1 or player 2
		Coin C & D credit together for use by _only_ player 3 or player 4
		Otherwise with Individual selected, everyone is seperate  */
	PORT_DIPNAME( 0x0080, 0x0080, "Coin Slot" )
	PORT_DIPSETTING(      0x0080, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_SERVICE( 0x0100, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0600, 0x0600, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0400, "Easy" )
	PORT_DIPSETTING(      0x0600, "Normal" )
	PORT_DIPSETTING(      0x0200, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "Freeze" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( spinlbrk )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x000f, 0x000f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x000f, "1 Credit 1 Health Pack" )	/* I chose "Health Packs" as the actual value can change */
	PORT_DIPSETTING(      0x000e, "1 Credit 2 Health Packs" )	/*  via dipswitch 2-7 (0x4000) see below */
	PORT_DIPSETTING(      0x000d, "1 Credit 3 Health Packs" )
	PORT_DIPSETTING(      0x000c, "1 Credit 4 Health Packs" )
	PORT_DIPSETTING(      0x000b, "1 Credit 5 Health Packs" )
	PORT_DIPSETTING(      0x000a, "1 Credit 6 Health Packs" )
	PORT_DIPSETTING(      0x0009, "2 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0008, "3 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0007, "4 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0006, "5 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0005, "2 Credits 2 Health Packs" )
	PORT_DIPSETTING(      0x0004, "2-1-1C  1-1-1 HPs" )
	PORT_DIPSETTING(      0x0003, "2-2C 1-2 HPs" )
	PORT_DIPSETTING(      0x0002, "1-1-1-1-1C 1-1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0001, "1-1-1-1C 1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0000, "1-1C 1-2 HPs" )
/* The last 5 Coin/Credit selections are cycles:
	Example: 0x0004 = 2-1-1C 1-1-1 HPs:
	2 Credits for the 1st Health Pack, 1 Credit for the 2nd Health Pack, 1 Credit
	for the 3rd Health Pack... Then back to 2 Credits agian for 1 HP, then 1 credit
	and 1 credit.... on and on.  With all Coin/Credit dips set to on, it's 1 Health
	Pack for odd credits, 2 Health Packs for even credits :p
	*/
	PORT_DIPNAME( 0x00f0, 0x00f0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x00f0, "1 Credit 1 Health Pack" )
	PORT_DIPSETTING(      0x00e0, "1 Credit 2 Health Packs" )
	PORT_DIPSETTING(      0x00d0, "1 Credit 3 Health Packs" )
	PORT_DIPSETTING(      0x00c0, "1 Credit 4 Health Packs" )
	PORT_DIPSETTING(      0x00b0, "1 Credit 5 Health Packs" )
	PORT_DIPSETTING(      0x00a0, "1 Credit 6 Health Packs" )
	PORT_DIPSETTING(      0x0090, "2 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0080, "3 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0070, "4 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0060, "5 Credits 1 Health Pack" )
	PORT_DIPSETTING(      0x0050, "2 Credits 2 Health Packs" )
	PORT_DIPSETTING(      0x0040, "2-1-1C  1-1-1 HPs" )
	PORT_DIPSETTING(      0x0030, "2-2C 1-2 HPs" )
	PORT_DIPSETTING(      0x0020, "1-1-1-1-1C 1-1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0010, "1-1-1-1C 1-1-1-2 HPs" )
	PORT_DIPSETTING(      0x0000, "1-1C 1-2 HPs" )
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0300, "Normal" )
	PORT_DIPSETTING(      0x0200, "Easy" )
	PORT_DIPSETTING(      0x0100, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0400, 0x0400, "Coin Slot" )		/* Japan ver acts like "Same" is always selected */
	PORT_DIPSETTING(      0x0000, "Same" )
	PORT_DIPSETTING(      0x0400, "Individual" )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "Player Lever Check?" ) /* Possibly Demo Sound??? but causes lever error??? */
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x2000, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x4000, 0x4000, "Health Pack" )
	PORT_DIPSETTING(      0x4000, "20 Hitpoints" )			/* World and Japan ver this value is 32 */
	PORT_DIPSETTING(      0x0000, "32 Hitpoints" )			/* World and Japan ver this value is 40 */
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )			/* This DIP has no effect that I can see */
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )			/*  most likely "Bonus Life" */
INPUT_PORTS_END

INPUT_PORTS_START( turbofrc )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE )	/* "TEST" */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )/*START1 ) */

	PORT_START
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coinage ) ) /* Coins 1, 2 & 3 */
	PORT_DIPSETTING(      0x0004, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0008, 0x0008, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0000, "Coin Slot" )
	PORT_DIPSETTING(      0x0010, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x0020, 0x0000, "Max Players" )
	PORT_DIPSETTING(      0x0020, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0e00, 0x0800, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0e00, "1 (Easiest)")
	PORT_DIPSETTING(      0x0c00, "2" )
	PORT_DIPSETTING(      0x0a00, "3" )
	PORT_DIPSETTING(      0x0800, "4 (Normal)" )
	PORT_DIPSETTING(      0x0600, "5" )
	PORT_DIPSETTING(      0x0400, "6" )
	PORT_DIPSETTING(      0x0200, "7" )
	PORT_DIPSETTING(      0x0000, "8 (Hardest)" )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x1000, "3" )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x2000, "200000" )
	PORT_DIPSETTING(      0x0000, "300000" )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START3 )
INPUT_PORTS_END

INPUT_PORTS_START( aerofgtb )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Coin Slot" )
	PORT_DIPSETTING(      0x0001, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x000e, 0x000e, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0070, 0x0070, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0050, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0070, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0080, 0x0080, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0800, "Easy" )
	PORT_DIPSETTING(      0x0c00, "Normal" )
	PORT_DIPSETTING(      0x0400, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x2000, "1" )
	PORT_DIPSETTING(      0x1000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x4000, "200000" )
	PORT_DIPSETTING(      0x0000, "300000" )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0000, "Country" )
	PORT_DIPSETTING(      0x0000, "Japan" )
	PORT_DIPSETTING(      0x0001, "Taiwan" )
	/* TODO: there are others in the table at 11910 */
	/* this port is checked at 1b080 */
INPUT_PORTS_END

INPUT_PORTS_START( aerofgt )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Coin Slot" )
	PORT_DIPSETTING(      0x0001, "Same" )
	PORT_DIPSETTING(      0x0000, "Individual" )
	PORT_DIPNAME( 0x000e, 0x000e, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x000a, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x000c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x000e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0070, 0x0070, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0050, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0060, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0070, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0080, 0x0080, "2 Coins to Start, 1 to Continue" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x000c, 0x000c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0008, "Easy" )
	PORT_DIPSETTING(      0x000c, "Normal" )
	PORT_DIPSETTING(      0x0004, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0020, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(      0x0040, "200000" )
	PORT_DIPSETTING(      0x0000, "300000" )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )

	PORT_START
	PORT_DIPNAME( 0x000f, 0x0000, "Country" )
	PORT_DIPSETTING(      0x0000, "Any" )
	PORT_DIPSETTING(      0x000f, "USA" )
	PORT_DIPSETTING(      0x000e, "Korea" )
	PORT_DIPSETTING(      0x000d, "Hong Kong" )
	PORT_DIPSETTING(      0x000b, "Taiwan" )
INPUT_PORTS_END



static struct GfxLayout pspikes_charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout aerofgt_charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout pspikes_spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 3*4, 2*4, RGN_FRAC(1,2)+1*4, RGN_FRAC(1,2)+0*4, RGN_FRAC(1,2)+3*4, RGN_FRAC(1,2)+2*4,
			5*4, 4*4, 7*4, 6*4, RGN_FRAC(1,2)+5*4, RGN_FRAC(1,2)+4*4, RGN_FRAC(1,2)+7*4, RGN_FRAC(1,2)+6*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxLayout aerofgtb_spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 1, 2, 3 },
	{ 3*4, 2*4, 1*4, 0*4, RGN_FRAC(1,2)+3*4, RGN_FRAC(1,2)+2*4, RGN_FRAC(1,2)+1*4, RGN_FRAC(1,2)+0*4,
			7*4, 6*4, 5*4, 4*4, RGN_FRAC(1,2)+7*4, RGN_FRAC(1,2)+6*4, RGN_FRAC(1,2)+5*4, RGN_FRAC(1,2)+4*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxLayout aerofgt_spritelayout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4,
			10*4, 11*4, 8*4, 9*4, 14*4, 15*4, 12*4, 13*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8
};

static struct GfxDecodeInfo pspikes_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pspikes_charlayout,      0, 64 },	/* colors    0-1023 in 8 banks */
	{ REGION_GFX2, 0, &pspikes_spritelayout, 1024, 64 },	/* colors 1024-2047 in 4 banks */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo turbofrc_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pspikes_charlayout,     0, 16 },
	{ REGION_GFX2, 0, &pspikes_charlayout,   256, 16 },
	{ REGION_GFX3, 0, &pspikes_spritelayout, 512, 16 },
	{ REGION_GFX4, 0, &pspikes_spritelayout, 768, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo aerofgtb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pspikes_charlayout,      0, 16 },
	{ REGION_GFX2, 0, &pspikes_charlayout,    256, 16 },
	{ REGION_GFX3, 0, &aerofgtb_spritelayout, 512, 16 },
	{ REGION_GFX4, 0, &aerofgtb_spritelayout, 768, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo aerofgt_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &aerofgt_charlayout,     0, 16 },
	{ REGION_GFX1, 0, &aerofgt_charlayout,   256, 16 },
	{ REGION_GFX2, 0, &aerofgt_spritelayout, 512, 16 },
	{ REGION_GFX3, 0, &aerofgt_spritelayout, 768, 16 },
	{ -1 } /* end of array */
};



static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2610interface ym2610_interface =
{
	1,
	8000000,	/* 8 MHz??? */
	{ 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler },
	{ REGION_SOUND1 },
	{ REGION_SOUND2 },
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) }
};



static MACHINE_DRIVER_START( pspikes )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,20000000/2)	/* 10 MHz (?) */
	MDRV_CPU_MEMORY(pspikes_readmem,pspikes_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* all irq vectors are the same */

	MDRV_CPU_ADD(Z80,8000000/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 4 MHz ??? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(turbofrc_sound_readport,turbofrc_sound_writeport)
								/* IRQs are triggered by the YM2610 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(aerofgt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8+4, 44*8+4-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(pspikes_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(pspikes)
	MDRV_VIDEO_UPDATE(pspikes)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( karatblz )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,20000000/2)	/* 10 MHz (?) */
	MDRV_CPU_MEMORY(karatblz_readmem,karatblz_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80,8000000/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 4 MHz ??? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(turbofrc_sound_readport,turbofrc_sound_writeport)
								/* IRQs are triggered by the YM2610 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(aerofgt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(1*8, 45*8-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(turbofrc_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(karatblz)
	MDRV_VIDEO_UPDATE(karatblz)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( spinlbrk )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,20000000/2)	/* 10 MHz (?) */
	MDRV_CPU_MEMORY(spinlbrk_readmem,spinlbrk_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* there are vectors for 3 and 4 too */

	MDRV_CPU_ADD(Z80,8000000/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 4 MHz ??? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(turbofrc_sound_readport,turbofrc_sound_writeport)
								/* IRQs are triggered by the YM2610 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(aerofgt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(1*8, 45*8-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(turbofrc_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(spinlbrk)
	MDRV_VIDEO_UPDATE(spinlbrk)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( turbofrc )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,20000000/2)	/* 10 MHz (?) */
	MDRV_CPU_MEMORY(turbofrc_readmem,turbofrc_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* all irq vectors are the same */

	MDRV_CPU_ADD(Z80,8000000/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 4 MHz ??? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(turbofrc_sound_readport,turbofrc_sound_writeport)
								/* IRQs are triggered by the YM2610 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(aerofgt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 44*8-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(turbofrc_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(turbofrc)
	MDRV_VIDEO_UPDATE(turbofrc)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( aerofgtb )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,20000000/2)	/* 10 MHz (?) */
	MDRV_CPU_MEMORY(aerofgtb_readmem,aerofgtb_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* all irq vectors are the same */

	MDRV_CPU_ADD(Z80,8000000/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 4 MHz ??? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(aerofgt_sound_readport,aerofgt_sound_writeport)
								/* IRQs are triggered by the YM2610 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(500)
				/* wrong but improves sprite-background synchronization */

	MDRV_MACHINE_INIT(aerofgt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8+12, 40*8-1+12, 0*8, 28*8-1)
	MDRV_GFXDECODE(aerofgtb_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(turbofrc)
	MDRV_VIDEO_UPDATE(turbofrc)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( aerofgt )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,20000000/2)	/* 10 MHz (?) */
	MDRV_CPU_MEMORY(aerofgt_readmem,aerofgt_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)/* all irq vectors are the same */

	MDRV_CPU_ADD(Z80,8000000/2)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 4 MHz ??? */
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(aerofgt_sound_readport,aerofgt_sound_writeport)
								/* IRQs are triggered by the YM2610 */
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(400)
				/* wrong but improves sprite-background synchronization */

	MDRV_MACHINE_INIT(aerofgt)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(aerofgt_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(turbofrc)
	MDRV_VIDEO_UPDATE(aerofgt)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2610, ym2610_interface)
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( pspikes )
	ROM_REGION( 0xc0000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "pspikes2.bin", 0x00000, 0x40000, CRC(ec0c070e) SHA1(4ddcc184e835a2f9d15f01aaa03734fd75fe797e) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "19",           0x00000, 0x20000, CRC(7e8ed6e5) SHA1(eeb1a1e1989fad8fc1e741928422efaec0598868) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "g7h",          0x000000, 0x80000, CRC(74c23c3d) SHA1(c0ac57d1f05c42556f97154ce1a08f465948546b) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "g7j",          0x000000, 0x80000, CRC(0b9e4739) SHA1(64b440a5026735aafe1a7cc2806fe0d78f4a6fba) )
	ROM_LOAD( "g7l",          0x080000, 0x80000, CRC(943139ff) SHA1(59065f9c3b3a47159c5968df199bdcb1b4f51f29) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "a47",          0x00000, 0x40000, CRC(c6779dfa) SHA1(ea7adefdb0da02755428aac9a6f86c908fc11253) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "o5b",          0x000000, 0x100000, CRC(07d6cbac) SHA1(d3d5778dbaca7b6cdceae959d0847d56df7b5cc1) )
ROM_END

ROM_START( pspikesk )
	ROM_REGION( 0xc0000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "20",           0x00000, 0x40000, CRC(75cdcee2) SHA1(272a08c46c1d0989f9fbb156e28e6a7ffa9c0a53) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "19",           0x00000, 0x20000, CRC(7e8ed6e5) SHA1(eeb1a1e1989fad8fc1e741928422efaec0598868) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "g7h",          0x000000, 0x80000, CRC(74c23c3d) SHA1(c0ac57d1f05c42556f97154ce1a08f465948546b) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "g7j",          0x000000, 0x80000, CRC(0b9e4739) SHA1(64b440a5026735aafe1a7cc2806fe0d78f4a6fba) )
	ROM_LOAD( "g7l",          0x080000, 0x80000, CRC(943139ff) SHA1(59065f9c3b3a47159c5968df199bdcb1b4f51f29) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "a47",          0x00000, 0x40000, CRC(c6779dfa) SHA1(ea7adefdb0da02755428aac9a6f86c908fc11253) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "o5b",          0x000000, 0x100000, CRC(07d6cbac) SHA1(d3d5778dbaca7b6cdceae959d0847d56df7b5cc1) )
ROM_END

ROM_START( svolly91 )
	ROM_REGION( 0xc0000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "u11.jpn",      0x00000, 0x40000, CRC(ea2e4c82) SHA1(f9cf9122499d9b1e54221fb8b6ef9c12004ca85e) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "19",           0x00000, 0x20000, CRC(7e8ed6e5) SHA1(eeb1a1e1989fad8fc1e741928422efaec0598868) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "g7h",          0x000000, 0x80000, CRC(74c23c3d) SHA1(c0ac57d1f05c42556f97154ce1a08f465948546b) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "g7j",          0x000000, 0x80000, CRC(0b9e4739) SHA1(64b440a5026735aafe1a7cc2806fe0d78f4a6fba) )
	ROM_LOAD( "g7l",          0x080000, 0x80000, CRC(943139ff) SHA1(59065f9c3b3a47159c5968df199bdcb1b4f51f29) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "a47",          0x00000, 0x40000, CRC(c6779dfa) SHA1(ea7adefdb0da02755428aac9a6f86c908fc11253) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "o5b",          0x000000, 0x100000, CRC(07d6cbac) SHA1(d3d5778dbaca7b6cdceae959d0847d56df7b5cc1) )
ROM_END

ROM_START( spinlbrk )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "ic98",    0x00000, 0x10000, CRC(36c2bf70) SHA1(f627d0e7dad1760bcc95af4a6346050a1a277048) )
	ROM_LOAD16_BYTE( "ic104",   0x00001, 0x10000, CRC(34a7e158) SHA1(5884570c1be38bfedffca3fd38089d0ae3391d4f) )
	ROM_LOAD16_BYTE( "ic93",    0x20000, 0x10000, CRC(726f4683) SHA1(65aff0548333571d47a96d4bf5a7857f12399cc7) )
	ROM_LOAD16_BYTE( "ic94",    0x20001, 0x10000, CRC(c4385e03) SHA1(6683eed812fa8a5430125b14e8647f8e9024bbdd) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "ic117",        0x00000, 0x08000, CRC(625ada41) SHA1(2dd0674c68ea382431115c155afbf880f5b9deb2) )
	ROM_LOAD( "ic118",        0x10000, 0x10000, CRC(1025f024) SHA1(3e497c74c950d2cd2a0931cf2ae9b0124d11ca6a) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ic15",         0x000000, 0x80000, CRC(e318cf3a) SHA1(d634001a0029566ce7b8fa30075970919eb5f44e) )
	ROM_LOAD( "ic9",          0x080000, 0x80000, CRC(e071f674) SHA1(b6d98d7fcc28516d937d8c655d07305515be8a20) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ic17",         0x000000, 0x80000, CRC(a63d5a55) SHA1(a942651a206a2abe4f60f0717e1d5d8e89b993d4) )
	ROM_LOAD( "ic11",         0x080000, 0x80000, CRC(7dcc913d) SHA1(527bae5020581d1ac322ea25c8e0994d54bbc051) )
	ROM_LOAD( "ic16",         0x100000, 0x80000, CRC(0d84af7f) SHA1(07356ee61c84c4c4ccb49c8dfe8c468990580041) )	/*FIRST AND SECOND HALF IDENTICAL*/

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "ic12",         0x000000, 0x80000, CRC(d63fac4e) SHA1(bb96d2e41334d136b9208dbe7e88a45e3bbc6542) )
	ROM_LOAD( "ic18",         0x080000, 0x80000, CRC(5a60444b) SHA1(62c418aedd1087dac82dcb44830cce00278103dd) )

	ROM_REGION( 0x200000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "ic14",         0x000000, 0x80000, CRC(1befd0f3) SHA1(7ab6fb5bf814ef3ae9a306a0d32d1078ee594461) )
	ROM_LOAD( "ic20",         0x080000, 0x80000, CRC(c2f84a61) SHA1(1dce538ced54a61c43ed25e1d71b5ac1c8935cc5) )
	ROM_LOAD( "ic35",         0x100000, 0x80000, CRC(eba8e1a3) SHA1(976ef30437df9aba6fa6d5cd11728476f34eb05b) )
	ROM_LOAD( "ic40",         0x180000, 0x80000, CRC(5ef5aa7e) SHA1(8d4b0f2348c536c6781c8ba25722301673aca289) )

	ROM_REGION16_BE( 0x24000, REGION_GFX5, 0 )	/* hardcoded sprite maps */
	ROM_LOAD16_BYTE( "ic19",    0x00000, 0x10000, CRC(db24eeaa) SHA1(300dd1ce81dd258b265bc3a64b8542ed152ed2cf) )
	ROM_LOAD16_BYTE( "ic13",    0x00001, 0x10000, CRC(97025bf4) SHA1(0519f0c94f3d417bf8ff0124a3a137035a4013dc) )
	/* 20000-23fff empty space, filled in vh_startup */

	/* no REGION_SOUND1 */

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "ic166",        0x000000, 0x80000, CRC(6e0d063a) SHA1(313983e69f9625814de033fef7f6e9564694117a) )
	ROM_LOAD( "ic163",        0x080000, 0x80000, CRC(e6621dfb) SHA1(85ee77c4720b7eb20ecf293c16b3105c8dcb1114) )	/*FIRST AND SECOND HALF IDENTICAL*/
ROM_END

ROM_START( spinlbru )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "ic98.u5", 0x00000, 0x10000, CRC(3a0f7667) SHA1(55d5fa1a325c17532ed83d231032bdbe9fb84d85) )
	ROM_LOAD16_BYTE( "ic104.u6",0x00001, 0x10000, CRC(a0e0af31) SHA1(21f6c3246bb7be2fd926324fd6d041e319a4e214) )
	ROM_LOAD16_BYTE( "ic93.u4", 0x20000, 0x10000, CRC(0cf73029) SHA1(e1346b759a41f9eec9536dc90671778582e595b4) )
	ROM_LOAD16_BYTE( "ic94.u3", 0x20001, 0x10000, CRC(5cf7c426) SHA1(b201da40c4511d2845004dff72d36adbb8a4fab9) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "ic117",        0x00000, 0x08000, CRC(625ada41) SHA1(2dd0674c68ea382431115c155afbf880f5b9deb2) )
	ROM_LOAD( "ic118",        0x10000, 0x10000, CRC(1025f024) SHA1(3e497c74c950d2cd2a0931cf2ae9b0124d11ca6a) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ic15",         0x000000, 0x80000, CRC(e318cf3a) SHA1(d634001a0029566ce7b8fa30075970919eb5f44e) )
	ROM_LOAD( "ic9",          0x080000, 0x80000, CRC(e071f674) SHA1(b6d98d7fcc28516d937d8c655d07305515be8a20) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ic17",         0x000000, 0x80000, CRC(a63d5a55) SHA1(a942651a206a2abe4f60f0717e1d5d8e89b993d4) )
	ROM_LOAD( "ic11",         0x080000, 0x80000, CRC(7dcc913d) SHA1(527bae5020581d1ac322ea25c8e0994d54bbc051) )
	ROM_LOAD( "ic16",         0x100000, 0x80000, CRC(0d84af7f) SHA1(07356ee61c84c4c4ccb49c8dfe8c468990580041) )	/*FIRST AND SECOND HALF IDENTICAL*/

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "ic12",         0x000000, 0x80000, CRC(d63fac4e) SHA1(bb96d2e41334d136b9208dbe7e88a45e3bbc6542) )
	ROM_LOAD( "ic18",         0x080000, 0x80000, CRC(5a60444b) SHA1(62c418aedd1087dac82dcb44830cce00278103dd) )

	ROM_REGION( 0x200000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "ic14",         0x000000, 0x80000, CRC(1befd0f3) SHA1(7ab6fb5bf814ef3ae9a306a0d32d1078ee594461) )
	ROM_LOAD( "ic20",         0x080000, 0x80000, CRC(c2f84a61) SHA1(1dce538ced54a61c43ed25e1d71b5ac1c8935cc5) )
	ROM_LOAD( "ic35",         0x100000, 0x80000, CRC(eba8e1a3) SHA1(976ef30437df9aba6fa6d5cd11728476f34eb05b) )
	ROM_LOAD( "ic40",         0x180000, 0x80000, CRC(5ef5aa7e) SHA1(8d4b0f2348c536c6781c8ba25722301673aca289) )

	ROM_REGION16_BE( 0x24000, REGION_GFX5, 0 )	/* hardcoded sprite maps */
	ROM_LOAD16_BYTE( "ic19",    0x00000, 0x10000, CRC(db24eeaa) SHA1(300dd1ce81dd258b265bc3a64b8542ed152ed2cf) )
	ROM_LOAD16_BYTE( "ic13",    0x00001, 0x10000, CRC(97025bf4) SHA1(0519f0c94f3d417bf8ff0124a3a137035a4013dc) )
	/* 20000-23fff empty space, filled in vh_startup */

	/* no REGION_SOUND1 */

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "ic166",        0x000000, 0x80000, CRC(6e0d063a) SHA1(313983e69f9625814de033fef7f6e9564694117a) )
	ROM_LOAD( "ic163",        0x080000, 0x80000, CRC(e6621dfb) SHA1(85ee77c4720b7eb20ecf293c16b3105c8dcb1114) )	/*FIRST AND SECOND HALF IDENTICAL*/
ROM_END

ROM_START( spinlbrj )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "j5",      0x00000, 0x10000, CRC(6a3d690e) SHA1(4ac1985ea0a73b8fc12105ff75121718595dd171) )
	ROM_LOAD16_BYTE( "j6",      0x00001, 0x10000, CRC(869593fa) SHA1(5821b011d42113f247bd100cecf140bbfc1e969c) )
	ROM_LOAD16_BYTE( "j4",      0x20000, 0x10000, CRC(33e33912) SHA1(d6d052cd8dbedfd254bdf5e82ad770e4bf241777) )
	ROM_LOAD16_BYTE( "j3",      0x20001, 0x10000, CRC(16ca61d0) SHA1(5d99a1261251412c3c758af751997fe31026c0d6) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "ic117",        0x00000, 0x08000, CRC(625ada41) SHA1(2dd0674c68ea382431115c155afbf880f5b9deb2) )
	ROM_LOAD( "ic118",        0x10000, 0x10000, CRC(1025f024) SHA1(3e497c74c950d2cd2a0931cf2ae9b0124d11ca6a) )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ic15",         0x000000, 0x80000, CRC(e318cf3a) SHA1(d634001a0029566ce7b8fa30075970919eb5f44e) )
	ROM_LOAD( "ic9",          0x080000, 0x80000, CRC(e071f674) SHA1(b6d98d7fcc28516d937d8c655d07305515be8a20) )

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "ic17",         0x000000, 0x80000, CRC(a63d5a55) SHA1(a942651a206a2abe4f60f0717e1d5d8e89b993d4) )
	ROM_LOAD( "ic11",         0x080000, 0x80000, CRC(7dcc913d) SHA1(527bae5020581d1ac322ea25c8e0994d54bbc051) )
	ROM_LOAD( "ic16",         0x100000, 0x80000, CRC(0d84af7f) SHA1(07356ee61c84c4c4ccb49c8dfe8c468990580041) )	/*FIRST AND SECOND HALF IDENTICAL*/

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "ic12",         0x000000, 0x80000, CRC(d63fac4e) SHA1(bb96d2e41334d136b9208dbe7e88a45e3bbc6542) )
	ROM_LOAD( "ic18",         0x080000, 0x80000, CRC(5a60444b) SHA1(62c418aedd1087dac82dcb44830cce00278103dd) )

	ROM_REGION( 0x200000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "ic14",         0x000000, 0x80000, CRC(1befd0f3) SHA1(7ab6fb5bf814ef3ae9a306a0d32d1078ee594461) )
	ROM_LOAD( "ic20",         0x080000, 0x80000, CRC(c2f84a61) SHA1(1dce538ced54a61c43ed25e1d71b5ac1c8935cc5) )
	ROM_LOAD( "ic35",         0x100000, 0x80000, CRC(eba8e1a3) SHA1(976ef30437df9aba6fa6d5cd11728476f34eb05b) )
	ROM_LOAD( "ic40",         0x180000, 0x80000, CRC(5ef5aa7e) SHA1(8d4b0f2348c536c6781c8ba25722301673aca289) )

	ROM_REGION16_BE( 0x24000, REGION_GFX5, 0 )	/* hardcoded sprite maps */
	ROM_LOAD16_BYTE( "ic19",    0x00000, 0x10000, CRC(db24eeaa) SHA1(300dd1ce81dd258b265bc3a64b8542ed152ed2cf) )
	ROM_LOAD16_BYTE( "ic13",    0x00001, 0x10000, CRC(97025bf4) SHA1(0519f0c94f3d417bf8ff0124a3a137035a4013dc) )
	/* 20000-23fff empty space, filled in vh_startup */

	/* no REGION_SOUND1 */

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "ic166",        0x000000, 0x80000, CRC(6e0d063a) SHA1(313983e69f9625814de033fef7f6e9564694117a) )
	ROM_LOAD( "ic163",        0x080000, 0x80000, CRC(e6621dfb) SHA1(85ee77c4720b7eb20ecf293c16b3105c8dcb1114) )	/*FIRST AND SECOND HALF IDENTICAL*/
ROM_END

ROM_START( karatblz )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "rom2v3",  0x00000, 0x40000, CRC(01f772e1) SHA1(f87f19a82d75839b5671f23ce14218d7b910eabc) )
	ROM_LOAD16_WORD_SWAP( "1.u15",   0x40000, 0x40000, CRC(d16ee21b) SHA1(d454cdf22b72a537b9d7ae73deb8136a4f09da47) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "5.u92",        0x00000, 0x20000, CRC(97d67510) SHA1(1ffd419e3dec7de1099cd5819b0309f7dd0df80e) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x80000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gha.u55",      0x00000, 0x80000, CRC(3e0cea91) SHA1(bab41715f106d364013b64649441d280bc6893cf) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gh9.u61",      0x00000, 0x80000, CRC(5d1676bd) SHA1(6227d489c9c6259a0ac2bef62821fbf94efca8c6) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "u42",          0x000000, 0x100000, CRC(65f0da84) SHA1(0bfbc6f4b87583703246704eb9fa13b1b3e6f90e) )
	ROM_LOAD( "3.u44",        0x100000, 0x020000, CRC(34bdead2) SHA1(99f9a8cac807fcd599db55d2dc624ed92a3862ef) )
	ROM_LOAD( "u43",          0x200000, 0x100000, CRC(7b349e5d) SHA1(8590a328f403e2c697a8d698c08d4adaf01fff62) )
	ROM_LOAD( "4.u45",        0x300000, 0x020000, CRC(be4d487d) SHA1(6d19c91d0498c43017219f0c10f4845a51ccfa7f) )

	ROM_REGION( 0x100000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "u59.ghb",      0x000000, 0x80000, CRC(158c9cde) SHA1(a2c1b404d40e6c2627691f5c7a3f63484bd5d2de) )
	ROM_LOAD( "ghd.u60",      0x080000, 0x80000, CRC(73180ae3) SHA1(e4eaf6693826d9e72032d0a0e25938a23ab7d792) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "u105.gh8",     0x000000, 0x080000, CRC(7a68cb1b) SHA1(1bdd0000c2d68019b9e5bf8f7ad84a6ae1af8443) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "u104",         0x000000, 0x100000, CRC(5795e884) SHA1(a4178497ad0a1e60ceb87612b218d77b36d2a11b) )
ROM_END

ROM_START( karatblu )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "2.u14",   0x00000, 0x40000, CRC(202e6220) SHA1(2605511a0574cbc39fdf3d8ae27a0aa9b43345fb) )
	ROM_LOAD16_WORD_SWAP( "1.u15",   0x40000, 0x40000, CRC(d16ee21b) SHA1(d454cdf22b72a537b9d7ae73deb8136a4f09da47) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "5.u92",        0x00000, 0x20000, CRC(97d67510) SHA1(1ffd419e3dec7de1099cd5819b0309f7dd0df80e) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x80000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "gha.u55",      0x00000, 0x80000, CRC(3e0cea91) SHA1(bab41715f106d364013b64649441d280bc6893cf) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "gh9.u61",      0x00000, 0x80000, CRC(5d1676bd) SHA1(6227d489c9c6259a0ac2bef62821fbf94efca8c6) )

	ROM_REGION( 0x400000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "u42",          0x000000, 0x100000, CRC(65f0da84) SHA1(0bfbc6f4b87583703246704eb9fa13b1b3e6f90e) )
	ROM_LOAD( "3.u44",        0x100000, 0x020000, CRC(34bdead2) SHA1(99f9a8cac807fcd599db55d2dc624ed92a3862ef) )
	ROM_LOAD( "u43",          0x200000, 0x100000, CRC(7b349e5d) SHA1(8590a328f403e2c697a8d698c08d4adaf01fff62) )
	ROM_LOAD( "4.u45",        0x300000, 0x020000, CRC(be4d487d) SHA1(6d19c91d0498c43017219f0c10f4845a51ccfa7f) )

	ROM_REGION( 0x100000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "u59.ghb",      0x000000, 0x80000, CRC(158c9cde) SHA1(a2c1b404d40e6c2627691f5c7a3f63484bd5d2de) )
	ROM_LOAD( "ghd.u60",      0x080000, 0x80000, CRC(73180ae3) SHA1(e4eaf6693826d9e72032d0a0e25938a23ab7d792) )

	ROM_REGION( 0x080000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "u105.gh8",     0x000000, 0x080000, CRC(7a68cb1b) SHA1(1bdd0000c2d68019b9e5bf8f7ad84a6ae1af8443) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "u104",         0x000000, 0x100000, CRC(5795e884) SHA1(a4178497ad0a1e60ceb87612b218d77b36d2a11b) )
ROM_END

ROM_START( turbofrc )
	ROM_REGION( 0xc0000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "tfrc2.bin",    0x00000, 0x40000, CRC(721300ee) SHA1(79ab32fdfd377592a0bdbd1c4794cfd529a3eb7b) )
	ROM_LOAD16_WORD_SWAP( "tfrc1.bin",    0x40000, 0x40000, CRC(6cd5312b) SHA1(57b109fe268fb963e981c91b6d288667a3c9a665) )
	ROM_LOAD16_WORD_SWAP( "tfrc3.bin",    0x80000, 0x40000, CRC(63f50557) SHA1(f8dba8c9ba412c9a67457ec31a804c57593ab20b) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "tfrcu166.bin", 0x00000, 0x20000, CRC(2ca14a65) SHA1(95f6e7b4fa7ca26872ff472d7e6fb75fd4f281d5) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x0a0000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "tfrcu94.bin",  0x000000, 0x80000, CRC(baa53978) SHA1(7f103122dd0bf675226ccf309fba73f645e0c79b) )
	ROM_LOAD( "tfrcu95.bin",  0x080000, 0x20000, CRC(71a6c573) SHA1(f14ebca676d85fabcde27631145933abc376dd12) )

	ROM_REGION( 0x0a0000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "tfrcu105.bin", 0x000000, 0x80000, CRC(4de4e59e) SHA1(571396dadb8aac043319cabe24e629210e442d57) )
	ROM_LOAD( "tfrcu106.bin", 0x080000, 0x20000, CRC(c6479eb5) SHA1(47a58f082c73bc9dae3970e760ba46478ce6a190) )

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "tfrcu116.bin", 0x000000, 0x80000, CRC(df210f3b) SHA1(990ac43e4a46fee6b929c5b27d317cdadf179b8b) )
	ROM_LOAD( "tfrcu118.bin", 0x080000, 0x40000, CRC(f61d1d79) SHA1(2b8e33912c05c26170afd2fced0ff06cb7a097fa) )
	ROM_LOAD( "tfrcu117.bin", 0x100000, 0x80000, CRC(f70812fd) SHA1(1964e1134940825211cd4825fdd3f13b8242192d) )
	ROM_LOAD( "tfrcu119.bin", 0x180000, 0x40000, CRC(474ea716) SHA1(67753e96fa4fc8cd689a8bddeb60dbde259cacaa) )

	ROM_REGION( 0x100000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "tfrcu134.bin", 0x000000, 0x80000, CRC(487330a2) SHA1(0bd36c1f5776ba2773f621e9bcb22f56ed1d84ec) )
	ROM_LOAD( "tfrcu135.bin", 0x080000, 0x80000, CRC(3a7e5b6d) SHA1(0079ffaa1bf93a5087c75615c78ec596b28c9a32) )

	ROM_REGION( 0x20000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "tfrcu180.bin",   0x00000, 0x20000, CRC(39c7c7d5) SHA1(66ee9f7cbc18ffab2c70f77ab0edead6bb018ca9) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "tfrcu179.bin", 0x000000, 0x100000, CRC(60ca0333) SHA1(28b94edc98d360386759780ccd1122d43ffa5279) )
ROM_END

ROM_START( aerofgt )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_WORD_SWAP( "1.u4",         0x00000, 0x80000, CRC(6fdff0a2) SHA1(7cc9529b426091027aa3e23586cb7d162376c0ff) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "2.153",        0x00000, 0x20000, CRC(a1ef64ec) SHA1(fa3e434738bf4e742ad68882c1e914100ce0f761) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "538a54.124",   0x000000, 0x80000, CRC(4d2c4df2) SHA1(f51c2b3135f0a921ac1a79e63d6878c03cb6254b) )
	ROM_LOAD( "1538a54.124",  0x080000, 0x80000, CRC(286d109e) SHA1(3a5f3d2d89cf58f6ef15e4bd3f570b84e8e695b2) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "538a53.u9",    0x000000, 0x100000, CRC(630d8e0b) SHA1(5a0c252ccd53c5199a695909d25ecb4e53dc15b9) )

	ROM_REGION( 0x080000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "534g8f.u18",   0x000000, 0x80000, CRC(76ce0926) SHA1(5ef4cec215d4dd600d8fcd1bd9a4c09081d59e33) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, CRC(6d42723d) SHA1(57c59234e9925430a4c687733682efed06d7eed1) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, CRC(cdbbdb1d) SHA1(067c816545f246ff1fd4c821d70df1e7eb47938c) )
ROM_END

ROM_START( aerofgtb )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "v2",                0x00000, 0x40000, CRC(5c9de9f0) SHA1(93b62c59f0bc052c6fdbd5aae292a7ab2122dfd1) )
	ROM_LOAD16_BYTE( "v1",                0x00001, 0x40000, CRC(89c1dcf4) SHA1(41401d63049c140e4254dc791022d85c44271390) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "v3",           0x00000, 0x20000, CRC(cbb18cf4) SHA1(7119a7536cf710660ff06d1e7d2879c79ef12b3d) )
	ROM_RELOAD(               0x10000, 0x20000 )

	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-03",     0x000000, 0x80000, CRC(85eba1a4) SHA1(5691a95d6359fdab29be0d615066370c2b856c0a) )

	ROM_REGION( 0x080000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-02",     0x000000, 0x80000, CRC(4f57f8ba) SHA1(aaad548e9a7490dfd48a975135716225f416b6f6) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-04",     0x000000, 0x80000, CRC(3b329c1f) SHA1(279cb32d69ce1e71f42cfad93d395794a3e92bc6) )
	ROM_LOAD( "it-19-05",     0x080000, 0x80000, CRC(02b525af) SHA1(07f23d15938dfbdc4f0977ba1463a06090569026) )

	ROM_REGION( 0x080000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "g27",          0x000000, 0x40000, CRC(4d89cbc8) SHA1(93f248f3dc1a15c32d14a147b37d5d660d0e4337) )
	ROM_LOAD( "g26",          0x040000, 0x40000, CRC(8072c1d2) SHA1(c14634f5f2686cf616f415d9ea4a0c6490054beb) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, CRC(6d42723d) SHA1(57c59234e9925430a4c687733682efed06d7eed1) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, CRC(cdbbdb1d) SHA1(067c816545f246ff1fd4c821d70df1e7eb47938c) )
ROM_END

ROM_START( aerofgtc )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "v2.149",            0x00000, 0x40000, CRC(f187aec6) SHA1(8905af34f114ae22fbfbd3ae115f19280bdd4fb3) )
	ROM_LOAD16_BYTE( "v1.111",            0x00001, 0x40000, CRC(9e684b19) SHA1(b5e1e5b74ed9fd223c9315ee2d548e620224c102) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "2.153",        0x00000, 0x20000, CRC(a1ef64ec) SHA1(fa3e434738bf4e742ad68882c1e914100ce0f761) )
	ROM_RELOAD(               0x10000, 0x20000 )

	/* gfx ROMs were missing in this set, I'm using the aerofgtb ones */
	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-03",     0x000000, 0x80000, CRC(85eba1a4) SHA1(5691a95d6359fdab29be0d615066370c2b856c0a) )

	ROM_REGION( 0x080000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-02",     0x000000, 0x80000, CRC(4f57f8ba) SHA1(aaad548e9a7490dfd48a975135716225f416b6f6) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-04",     0x000000, 0x80000, CRC(3b329c1f) SHA1(279cb32d69ce1e71f42cfad93d395794a3e92bc6) )
	ROM_LOAD( "it-19-05",     0x080000, 0x80000, CRC(02b525af) SHA1(07f23d15938dfbdc4f0977ba1463a06090569026) )

	ROM_REGION( 0x080000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "g27",          0x000000, 0x40000, CRC(4d89cbc8) SHA1(93f248f3dc1a15c32d14a147b37d5d660d0e4337) )
	ROM_LOAD( "g26",          0x040000, 0x40000, CRC(8072c1d2) SHA1(c14634f5f2686cf616f415d9ea4a0c6490054beb) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, CRC(6d42723d) SHA1(57c59234e9925430a4c687733682efed06d7eed1) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, CRC(cdbbdb1d) SHA1(067c816545f246ff1fd4c821d70df1e7eb47938c) )
ROM_END

ROM_START( sonicwi )
	ROM_REGION( 0x80000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "2.149",        0x00000, 0x40000, CRC(3d1b96ba) SHA1(941be323c0cb15e05c92b897984617b05c5cf676) )
	ROM_LOAD16_BYTE( "1.111",        0x00001, 0x40000, CRC(a3d09f94) SHA1(a1064d659488878f5303edc2b8636312ab632a83) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 )	/* 64k for the audio CPU + banks */
	ROM_LOAD( "2.153",        0x00000, 0x20000, CRC(a1ef64ec) SHA1(fa3e434738bf4e742ad68882c1e914100ce0f761) )	/* 3.156*/
	ROM_RELOAD(               0x10000, 0x20000 )

	/* gfx ROMs were missing in this set, I'm using the aerofgtb ones */
	ROM_REGION( 0x080000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-03",     0x000000, 0x80000, CRC(85eba1a4) SHA1(5691a95d6359fdab29be0d615066370c2b856c0a) )

	ROM_REGION( 0x080000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-02",     0x000000, 0x80000, CRC(4f57f8ba) SHA1(aaad548e9a7490dfd48a975135716225f416b6f6) )

	ROM_REGION( 0x100000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "it-19-04",     0x000000, 0x80000, CRC(3b329c1f) SHA1(279cb32d69ce1e71f42cfad93d395794a3e92bc6) )
	ROM_LOAD( "it-19-05",     0x080000, 0x80000, CRC(02b525af) SHA1(07f23d15938dfbdc4f0977ba1463a06090569026) )

	ROM_REGION( 0x080000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "g27",          0x000000, 0x40000, CRC(4d89cbc8) SHA1(93f248f3dc1a15c32d14a147b37d5d660d0e4337) )
	ROM_LOAD( "g26",          0x040000, 0x40000, CRC(8072c1d2) SHA1(c14634f5f2686cf616f415d9ea4a0c6490054beb) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* sound samples */
	ROM_LOAD( "it-19-01",     0x00000, 0x40000, CRC(6d42723d) SHA1(57c59234e9925430a4c687733682efed06d7eed1) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 ) /* sound samples */
	ROM_LOAD( "it-19-06",     0x000000, 0x100000, CRC(cdbbdb1d) SHA1(067c816545f246ff1fd4c821d70df1e7eb47938c) )
ROM_END



GAMEX( 1990, spinlbrk, 0,        spinlbrk, spinlbrk, 0, ROT0,   "V-System Co.", "Spinal Breakers (World)", GAME_NO_COCKTAIL )
GAMEX( 1990, spinlbru, spinlbrk, spinlbrk, spinlbrk, 0, ROT0,   "V-System Co.", "Spinal Breakers (US)", GAME_NO_COCKTAIL )
GAMEX( 1990, spinlbrj, spinlbrk, spinlbrk, spinlbrk, 0, ROT0,   "V-System Co.", "Spinal Breakers (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1991, pspikes,  0,        pspikes,  pspikes,  0, ROT0,   "Video System Co.", "Power Spikes (World)", GAME_NO_COCKTAIL )
GAMEX( 1991, pspikesk, pspikes,  pspikes,  pspikes,  0, ROT0,   "Video System Co.", "Power Spikes (Korea)", GAME_NO_COCKTAIL )
GAMEX( 1991, svolly91, pspikes,  pspikes,  pspikes,  0, ROT0,   "Video System Co.", "Super Volley '91 (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1991, karatblz, 0,        karatblz, karatblz, 0, ROT0,   "Video System Co.", "Karate Blazers (World[Q])", GAME_NO_COCKTAIL )
GAMEX( 1991, karatblu, karatblz, karatblz, karatblz, 0, ROT0,   "Video System Co.", "Karate Blazers (US)", GAME_NO_COCKTAIL )
GAMEX( 1991, turbofrc, 0,        turbofrc, turbofrc, 0, ROT270, "Video System Co.", "Turbo Force", GAME_NO_COCKTAIL )
GAMEX( 1992, aerofgt,  0,        aerofgt,  aerofgt,  0, ROT270, "Video System Co.", "Aero Fighters", GAME_NO_COCKTAIL )
GAMEX( 1992, aerofgtb, aerofgt,  aerofgtb, aerofgtb, 0, ROT270, "Video System Co.", "Aero Fighters (Turbo Force hardware set 1)", GAME_NO_COCKTAIL )
GAMEX( 1992, aerofgtc, aerofgt,  aerofgtb, aerofgtb, 0, ROT270, "Video System Co.", "Aero Fighters (Turbo Force hardware set 2)", GAME_NO_COCKTAIL )
GAMEX( 1992, sonicwi,  aerofgt,  aerofgtb, aerofgtb, 0, ROT270, "Video System Co.", "Sonic Wings (Japan)", GAME_NO_COCKTAIL )
