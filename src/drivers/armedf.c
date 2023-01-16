/**********************************************************************

Legion
(c)1986 Nichibutsu

Terra Force
(c)1987 Nichibutsu

Kodure Ookami
(c)1987 Nichibutsu

Crazy Climber 2
(c)1988 Nichibutsu

Armed Formation
(c)1988 Nichibutsu

68000 + Z80

TODO:
- simulate the mcu/blitter (particularly needed in terrafu and legion)


Stephh's notes (based on the games M68000 code and some tests) :

1a) 'legion'

  - The ROM test (code at 0x00046e) checks range 0x000102-0x01ffff (!)
    and reports an error if the checksum isn't correct.

  - 3 Dip Switches which are told to be unused have an effect if
    [0x062d53] != 0x00 (check code at 0x00d9b2).
    As this value is ALWAYS set to 0x00 via code at 0x0001d4, I've
    added a #define LEGION_HACK you need to change to 1 if you want
    get the benefits of these Dip Switches.
    Here is what these "unused" Dip Switch do :

      * 1st unused Dip Switch (DSW0 bit 6) remains unused
      * 2nd unused Dip Switch (DSW0 bit 7) determines if the "Invulnerability"
        Dip Switches will be tested
      * 3rd unused Dip Switch (DSW1 bit 6) gives invulnerability or not
        to player 1 (DSW0 bit 7 must be ON too !)
      * 4th unused Dip Switch (DSW1 bit 7) gives invulnerability or not
        to player 2 (DSW0 bit 7 must be ON too !)

    Also note bit 1 of 0x062d53 is also tested but I haven't been able
    to find its purpose (any more infos are welcome)


1b) 'legiono'

  - The ROM test (code at 0x000466) checks range 0x000102-0x03ffff
    but NEVER reports an error if the checksum isn't correct due
    to the instruction at 0x000480 (see where it branches) :

	000466: 7000                     moveq   #$0, D0
	000468: 41FA FC98                lea     (-$368,PC), A0; ($102)
	00046C: D058                     add.w   (A0)+, D0
	00046E: B1FC 0004 0000           cmpa.l  #$40000, A0
	000474: 66F6                     bne     46c
	000476: 33C0 0006 2CAE           move.w  D0, $62cae.l
	00047C: B078 0100                cmp.w   $100.w, D0
	000480: 6600 0002                bne     484
	000484: 41FA FF86                lea     (-$7a,PC), A0; ($40c)

  - 3 Dip Switches which are told to be unused have an effect if
    [0x062d53] != 0x00 (check code at 0x00d7ea).
    As this value is ALWAYS set to 0x00 via code at 0x0001d4, I've
    added a #define LEGION_HACK you need to change to 1 if you want
    get the benefits of these Dip Switches.
    Here is what these "unused" Dip Switch do :

      * 1st unused Dip Switch (DSW0 bit 6) remains unused
      * 2nd unused Dip Switch (DSW0 bit 7) determines if the "Invulnerability"
        Dip Switches will be tested
      * 3rd unused Dip Switch (DSW1 bit 6) gives invulnerability or not
        to player 1 (DSW0 bit 7 must be ON too !)
      * 4th unused Dip Switch (DSW1 bit 7) gives invulnerability or not
        to player 2 (DSW0 bit 7 must be ON too !)

    Also note bit 1 of 0x062d53 is also tested but I haven't been able
    to find its purpose (any more infos are welcome)


2a) 'terraf'

  - The ROM test (code at 0x000292) ALWAYS displays "OK", but memory is
    in fact NEVER scanned ! Original behaviour or is the game a bootleg ?

	000292: 45F8 0000                lea     $0.w, A2
	000296: 303C 7FFF                move.w  #$7fff, D0
	00029A: 7200                     moveq   #$0, D1
	00029C: D29A                     add.l   (A2)+, D1
	00029E: 0C81 0000 0000           cmpi.l  #$0, D1
	0002A4: 4E71                     nop
	0002A6: 23FC 004F 004B 0006 83AA move.l  #$4f004b, $683aa.l
	0002B0: 4EF9 0000 0124           jmp     $124.l
	...
	0002C2: 4EF9 0000 0124           jmp     $124.l


2b) 'terrafu'

  - The ROM test (code at 0x000292) NEVER displays "OK", but memory is
    in fact NEVER scanned ! Original behaviour or is the game a bootleg ?

	000292: 45F8 0000                lea     $0.w, A2
	000296: 303C 7FFF                move.w  #$7fff, D0
	00029A: 7200                     moveq   #$0, D1
	00029C: D29A                     add.l   (A2)+, D1
	00029E: 0C81 0000 0000           cmpi.l  #$0, D1
	0002A4: 661C                     bne     2c2
	0002A6: 23FC 004F 004B 0006 83AA move.l  #$4f004b, $683aa.l
	0002B0: 4EF9 0000 0124           jmp     $124.l
	...
	0002C2: 4EF9 0000 0124           jmp     $124.l


3)  'kodure'

  - The ROM test (code at 0x004fac) checks range 0x000000-0x05ffff
    and reports an error if the checksum isn't correct.


4)  'cclimbr2'

  - The ROM test (code at 0x012f6e) checks ranges 0x000100-0x014fff,
    and 0x020000-0x024fff, and reports an error if the checksum isn't
    correct.


5)  'armedf'

  - The ROM test (code at 0x00df5e) checks ranges 0x000100-0x014fff,
    0x020000-0x024fff and 0x040000-0x04ffff, and reports an error if
    the checksum isn't correct.

***********************************************************************/

/*

	2003-06-01	Added cocktail support to all games

*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"

#define LEGION_HACK	0


extern void armedf_setgfxtype( int type );

VIDEO_UPDATE( armedf );
VIDEO_EOF( armedf );
VIDEO_START( armedf );
VIDEO_START( legion );

WRITE16_HANDLER( armedf_bg_videoram_w );
WRITE16_HANDLER( armedf_fg_videoram_w );
WRITE16_HANDLER( armedf_text_videoram_w );
WRITE16_HANDLER( terraf_fg_scrollx_w );
WRITE16_HANDLER( terraf_fg_scrolly_w );
WRITE16_HANDLER( terraf_fg_scroll_msb_arm_w );
WRITE16_HANDLER( armedf_fg_scrollx_w );
WRITE16_HANDLER( armedf_fg_scrolly_w );
WRITE16_HANDLER( armedf_bg_scrollx_w );
WRITE16_HANDLER( armedf_bg_scrolly_w );
WRITE16_HANDLER( armedf_mcu_cmd );

extern data16_t armedf_vreg;
extern data16_t *spr_pal_clut;
extern data16_t *armedf_bg_videoram;
extern data16_t *armedf_fg_videoram;
extern data16_t *terraf_text_videoram;

extern data16_t *legion_cmd;
extern struct tilemap *armedf_tx_tilemap;

static WRITE16_HANDLER( io_w )
{
	COMBINE_DATA(&armedf_vreg);
	/* bits 0 and 1 of armedf_vreg are coin counters */
	/* bit 12 seems to handle screen flipping */
	flip_screen_set(armedf_vreg & 0x1000);
}

static WRITE16_HANDLER( kodure_io_w )
{
	COMBINE_DATA(&armedf_vreg);
	/* bits 0 and 1 of armedf_vreg are coin counters */
	/* bit 12 seems to handle screen flipping */
	flip_screen_set(armedf_vreg & 0x1000);

	/* This is a temporary condition specification. */
	if (!(armedf_vreg & 0x0080))
	{
		int i;
		for (i = 0; i < 0x1000; i++)
		{
			armedf_text_videoram_w(i, ' ', 0);
		}
	}
}

static WRITE16_HANDLER( terraf_io_w )
{
	COMBINE_DATA(&armedf_vreg);
	/* bits 0 and 1 of armedf_vreg are coin counters */
	/* bit 12 seems to handle screen flipping */
	flip_screen_set(armedf_vreg & 0x1000);

	if ((armedf_vreg & 0x4000) && !(armedf_vreg & 0x0100))
	{
		int i;
		for (i = 0x10; i < 0x1000; i++)
		{
			terraf_text_videoram[i]=0x20;
		}
		tilemap_mark_all_tiles_dirty( armedf_tx_tilemap );
		/*logerror("vreg WIPE TX\n"); */
	}
	/*logerror("VReg = %04x\n", armedf_vreg); */
}

static WRITE16_HANDLER( legion_command_c )
{
	COMBINE_DATA(&legion_cmd[offset]);
	/*logerror("Legion CMD %04x=%04x", offset, data); */
}

static WRITE16_HANDLER( sound_command_w )
{
	if (ACCESSING_LSB)
		soundlatch_w(0,((data & 0x7f) << 1) | 1);
}



static MEMORY_READ16_START( terraf_readmem )
	{ 0x000000, 0x04ffff, MRA16_ROM },
	{ 0x060000, 0x063fff, MRA16_RAM },
	{ 0x064000, 0x064fff, MRA16_RAM },
	{ 0x068000, 0x069fff, MRA16_RAM },
	{ 0x06a000, 0x06a9ff, MRA16_RAM },
	{ 0x06C000, 0x06Cfff, MRA16_RAM },
	{ 0x070000, 0x070fff, MRA16_RAM },
	{ 0x074000, 0x074fff, MRA16_RAM },
	{ 0x078000, 0x078001, input_port_0_word_r },
	{ 0x078002, 0x078003, input_port_1_word_r },
	{ 0x078004, 0x078005, input_port_2_word_r },
	{ 0x078006, 0x078007, input_port_3_word_r },
MEMORY_END

static MEMORY_WRITE16_START( terraf_writemem )
	{ 0x000000, 0x04ffff, MWA16_ROM },
	{ 0x060000, 0x0603ff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x060400, 0x063fff, MWA16_RAM },
	{ 0x064000, 0x064fff, paletteram16_xxxxRRRRGGGGBBBB_word_w, &paletteram16 },
	{ 0x068000, 0x069fff, armedf_text_videoram_w, &terraf_text_videoram },
	{ 0x06a000, 0x06a9ff, MWA16_RAM },
	{ 0x06C000, 0x06Cfff, MWA16_RAM, &spr_pal_clut },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x07c000, 0x07c001, terraf_io_w },
	{ 0x07c002, 0x07c003, armedf_bg_scrollx_w },
	{ 0x07c004, 0x07c005, armedf_bg_scrolly_w },
	{ 0x07c006, 0x07c007, terraf_fg_scrollx_w },    /* not use in terrafu, 0x07c008 neither */
	{ 0x07c008, 0x07c009, terraf_fg_scrolly_w },	/* written twice, lsb and msb */
	{ 0x07c00a, 0x07c00b, sound_command_w },
	{ 0x07c00c, 0x07c00d, MWA16_NOP },		    /* Watchdog ? cycle 0000 -> 0100 -> 0200 back to 0000 */
	{ 0x07c00e, 0x07c00f, armedf_mcu_cmd },		/* MCU Command ? */
	{ 0x0c0000, 0x0c0001, terraf_fg_scroll_msb_arm_w }, /* written between two consecutive writes to 7c008 */
MEMORY_END

static MEMORY_READ16_START( kodure_readmem )
	{ 0x000000, 0x05ffff, MRA16_ROM },
	{ 0x060000, 0x063fff, MRA16_RAM },
	{ 0x064000, 0x064fff, MRA16_RAM },
	{ 0x068000, 0x069fff, MRA16_RAM },
	{ 0x06a000, 0x06a9ff, MRA16_RAM },
	{ 0x06C000, 0x06Cfff, MRA16_RAM },
	{ 0x070000, 0x070fff, MRA16_RAM },
	{ 0x074000, 0x074fff, MRA16_RAM },
	{ 0x078000, 0x078001, input_port_0_word_r },
	{ 0x078002, 0x078003, input_port_1_word_r },
	{ 0x078004, 0x078005, input_port_2_word_r },
	{ 0x078006, 0x078007, input_port_3_word_r },
MEMORY_END

static MEMORY_WRITE16_START( kodure_writemem )
	{ 0x000000, 0x05ffff, MWA16_ROM },
	{ 0x060000, 0x060fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x061000, 0x063fff, MWA16_RAM },
	{ 0x064000, 0x064fff, paletteram16_xxxxRRRRGGGGBBBB_word_w, &paletteram16 },
	{ 0x068000, 0x069fff, armedf_text_videoram_w, &terraf_text_videoram },
	{ 0x06a000, 0x06a9ff, MWA16_RAM },
	{ 0x06C000, 0x06Cfff, MWA16_RAM, &spr_pal_clut },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x07c000, 0x07c001, kodure_io_w },
	{ 0x07c002, 0x07c003, armedf_bg_scrollx_w },
	{ 0x07c004, 0x07c005, armedf_bg_scrolly_w },
	{ 0x07c00a, 0x07c00b, sound_command_w },
	{ 0x0c0000, 0x0c0001, MWA16_NOP }, /* watchdog? */
	{ 0xffd000, 0xffd001, MWA16_NOP }, /* ? */
MEMORY_END

static MEMORY_READ16_START( cclimbr2_readmem )
	{ 0x000000, 0x05ffff, MRA16_ROM },
	{ 0x060000, 0x063fff, MRA16_RAM },
	{ 0x064000, 0x064fff, MRA16_RAM },
	{ 0x068000, 0x069fff, MRA16_RAM },
	{ 0x06a000, 0x06a9ff, MRA16_RAM },
	{ 0x06c000, 0x06cfff, MRA16_RAM },
	{ 0x070000, 0x070fff, MRA16_RAM },
	{ 0x074000, 0x074fff, MRA16_RAM },
	{ 0x078000, 0x078001, input_port_0_word_r },
	{ 0x078002, 0x078003, input_port_1_word_r },
	{ 0x078004, 0x078005, input_port_2_word_r },
	{ 0x078006, 0x078007, input_port_3_word_r },
MEMORY_END

static MEMORY_WRITE16_START( cclimbr2_writemem )
	{ 0x000000, 0x05ffff, MWA16_ROM },
	{ 0x060000, 0x060fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x061000, 0x063fff, MWA16_RAM },
	{ 0x064000, 0x064fff, paletteram16_xxxxRRRRGGGGBBBB_word_w, &paletteram16 },
	{ 0x068000, 0x069fff, armedf_text_videoram_w, &terraf_text_videoram },
	{ 0x06a000, 0x06a9ff, MWA16_RAM },
	{ 0x06ca00, 0x06cbff, MWA16_RAM },
	{ 0x06c000, 0x06cfff, MWA16_RAM, &spr_pal_clut },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x07c000, 0x07c001, io_w },
	{ 0x07c002, 0x07c003, armedf_bg_scrollx_w },
	{ 0x07c004, 0x07c005, armedf_bg_scrolly_w },
	{ 0x07c00a, 0x07c00b, sound_command_w },
	{ 0x07c00e, 0x07c00f, MWA16_NOP },		/* ? */
	{ 0x07c00c, 0x07c00d, MWA16_NOP },		/* Watchdog ? cycle 0000 -> 0100 -> 0200 back to 0000 */
MEMORY_END

static MEMORY_WRITE16_START( legion_writemem )
    { 0x000000, 0x05ffff, MWA16_ROM },
	{ 0x060000, 0x060fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x061000, 0x063fff, MWA16_RAM },
	{ 0x064000, 0x064fff, paletteram16_xxxxRRRRGGGGBBBB_word_w, &paletteram16 },
	{ 0x068000, 0x069fff, armedf_text_videoram_w, &terraf_text_videoram },
    { 0x06a000, 0x06a9ff, MWA16_RAM },
	{ 0x06ca00, 0x06cbff, MWA16_RAM },
	{ 0x06c000, 0x06cfff, MWA16_RAM, &spr_pal_clut },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x07c000, 0x07c001, terraf_io_w },
	{ 0x07c002, 0x07c003, armedf_bg_scrollx_w },
	{ 0x07c004, 0x07c005, armedf_bg_scrolly_w },
	{ 0x07c00a, 0x07c00b, sound_command_w },
	{ 0x07c00e, 0x07c00f, armedf_mcu_cmd },	/* MCU Command ? */
	{ 0x07c00c, 0x07c00d, MWA16_NOP },		/* Watchdog ? cycle 0000 -> 0100 -> 0200 back to 0000 */
MEMORY_END

static MEMORY_WRITE16_START( legiono_writemem )
    { 0x000000, 0x03ffff, MWA16_ROM },
	{ 0x040000, 0x04003f, MWA16_RAM, &legion_cmd },
	{ 0x040040, 0x05ffff, MWA16_ROM },
	{ 0x060000, 0x060fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x061000, 0x063fff, MWA16_RAM },
	{ 0x064000, 0x064fff, paletteram16_xxxxRRRRGGGGBBBB_word_w, &paletteram16 },
    { 0x068000, 0x069fff, armedf_text_videoram_w, &terraf_text_videoram },
	{ 0x06a000, 0x06a9ff, MWA16_RAM },
	{ 0x06ca00, 0x06cbff, MWA16_RAM },
	{ 0x06c000, 0x06cfff, MWA16_RAM, &spr_pal_clut },
	{ 0x070000, 0x070fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x074000, 0x074fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x07c000, 0x07c001, terraf_io_w },
	{ 0x07c002, 0x07c003, armedf_bg_scrollx_w },
	{ 0x07c004, 0x07c005, armedf_bg_scrolly_w },
	{ 0x07c00a, 0x07c00b, sound_command_w },
MEMORY_END

static MEMORY_READ16_START( armedf_readmem )
	{ 0x000000, 0x05ffff, MRA16_ROM },
	{ 0x060000, 0x065fff, MRA16_RAM },
	{ 0x066000, 0x066fff, MRA16_RAM },
	{ 0x067000, 0x067fff, MRA16_RAM },
	{ 0x068000, 0x069fff, MRA16_RAM },
	{ 0x06a000, 0x06afff, MRA16_RAM },
	{ 0x06b000, 0x06bfff, MRA16_RAM },
	{ 0x06c000, 0x06c001, input_port_0_word_r },
	{ 0x06c002, 0x06c003, input_port_1_word_r },
	{ 0x06c004, 0x06c005, input_port_2_word_r },
	{ 0x06c006, 0x06c007, input_port_3_word_r },
	{ 0x06c008, 0x06c7ff, MRA16_RAM },
MEMORY_END

static MEMORY_WRITE16_START( armedf_writemem )
	{ 0x000000, 0x05ffff, MWA16_ROM },
	{ 0x060000, 0x060fff, MWA16_RAM, &spriteram16, &spriteram_size },
	{ 0x061000, 0x065fff, MWA16_RAM },
	{ 0x066000, 0x066fff, armedf_bg_videoram_w, &armedf_bg_videoram },
	{ 0x067000, 0x067fff, armedf_fg_videoram_w, &armedf_fg_videoram },
	{ 0x068000, 0x069fff, armedf_text_videoram_w, &terraf_text_videoram },
	{ 0x06a000, 0x06afff, paletteram16_xxxxRRRRGGGGBBBB_word_w, &paletteram16 },
	{ 0x06b000, 0x06bfff, MWA16_RAM, &spr_pal_clut },
	{ 0x06c000, 0x06c7ff, MWA16_RAM },
	{ 0x06d000, 0x06d001, io_w },
	{ 0x06d002, 0x06d003, armedf_bg_scrollx_w },
	{ 0x06d004, 0x06d005, armedf_bg_scrolly_w },
	{ 0x06d006, 0x06d007, armedf_fg_scrollx_w },
	{ 0x06d008, 0x06d009, armedf_fg_scrolly_w },
	{ 0x06d00a, 0x06d00b, sound_command_w },
MEMORY_END


static MEMORY_READ_START( soundreadmem )
	{ 0x0000, 0xf7ff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( soundwritemem )
	{ 0x0000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_RAM },
MEMORY_END

static MEMORY_READ_START( cclimbr2_soundreadmem )
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( cclimbr2_soundwritemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xffff, MWA_RAM },
MEMORY_END

static READ_HANDLER( soundlatch_clear_r )
{
	soundlatch_clear_w(0,0);
	return 0;
}

static PORT_READ_START( readport )
	{ 0x4, 0x4, soundlatch_clear_r },
	{ 0x6, 0x6, soundlatch_r },
PORT_END

static PORT_WRITE_START( writeport )
	{ 0x0, 0x0, YM3812_control_port_0_w },
	{ 0x1, 0x1, YM3812_write_port_0_w },
  	{ 0x2, 0x2, DAC_0_signed_data_w },
  	{ 0x3, 0x3, DAC_1_signed_data_w },
PORT_END


/****************
   Dip Switches
*****************/

#define NIHON_SINGLE_JOYSTICK(_n_) \
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER##_n_) \
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define NIHON_COINS \
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0xf000, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define NIHON_SYSTEM \
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	PORT_SERVICE( 0x0200, IP_ACTIVE_LOW ) \
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_TILT )     /* Tilt */ \
	PORT_BIT( 0xf800, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define NIHON_COINAGE_A \
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )

#define NIHON_COINAGE_B \
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )

#define NIHON_COINAGE_B_ALT \
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )

#define NIHON_LIVES \
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) ) \
	PORT_DIPSETTING(    0x03, "3" ) \
	PORT_DIPSETTING(    0x02, "4" ) \
	PORT_DIPSETTING(    0x01, "5" ) \
	PORT_DIPSETTING(    0x00, "6" )


INPUT_PORTS_START( legion )
	PORT_START	/* IN0 */
	NIHON_SINGLE_JOYSTICK(1)
	NIHON_COINS

	PORT_START	/* IN1 */
	NIHON_SINGLE_JOYSTICK(2)
	NIHON_SYSTEM

	PORT_START	/* DSW0 */
	NIHON_LIVES
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "30k then every 100k" )
	PORT_DIPSETTING(    0x00, "50k only" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#if LEGION_HACK
	PORT_DIPNAME( 0x80, 0x80, "Allow Invulnerability" )		/* see notes*/
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
#else
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#endif

	PORT_START	/* DSW1 */
	NIHON_COINAGE_A
	NIHON_COINAGE_B
	PORT_DIPNAME( 0x10, 0x10, "Coin Slots" )
	PORT_DIPSETTING(    0x10, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
#if LEGION_HACK
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "P1 Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "P2 Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#else
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
#endif
INPUT_PORTS_END


INPUT_PORTS_START( terraf )
	PORT_START	/* IN0 */
	NIHON_SINGLE_JOYSTICK(1)
	NIHON_COINS

	PORT_START	/* IN1 */
	NIHON_SINGLE_JOYSTICK(2)
	NIHON_SYSTEM

	PORT_START	/* DSW0 */
	NIHON_LIVES
/*	PORT_DIPNAME( 0x04, 0x04, "1st Bonus Life" )*/
/*	PORT_DIPSETTING(    0x04, "20k" )*/
/*	PORT_DIPSETTING(    0x00, "50k" )*/
/*	PORT_DIPNAME( 0x08, 0x08, "2nd Bonus Life" )*/
/*	PORT_DIPSETTING(    0x08, "60k" )*/
/*	PORT_DIPSETTING(    0x00, "90k" )*/
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20k then every 60k" )
	PORT_DIPSETTING(    0x04, "20k then every 90k" )
	PORT_DIPSETTING(    0x08, "50k then every 60k" )
	PORT_DIPSETTING(    0x00, "50k then every 90k" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* DSW1 */
	NIHON_COINAGE_A
	NIHON_COINAGE_B
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0xc0, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, "3 Times" )
	PORT_DIPSETTING(    0x40, "5 Times" )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( kodure )
	PORT_START	/* IN0 */
	NIHON_SINGLE_JOYSTICK(1)
	NIHON_COINS

	PORT_START	/* IN1 */
	NIHON_SINGLE_JOYSTICK(2)
	NIHON_SYSTEM

	PORT_START	/* DSW0 */
	NIHON_LIVES
/*	PORT_DIPNAME( 0x04, 0x04, "1st Bonus Life" )*/
/*	PORT_DIPSETTING(    0x04, "00k" )*/
/*	PORT_DIPSETTING(    0x00, "50k" )*/
/*	PORT_DIPNAME( 0x08, 0x08, "2nd Bonus Life" )*/
/*	PORT_DIPSETTING(    0x08, "60k" )*/
/*	PORT_DIPSETTING(    0x00, "90k" )*/
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "50k then every 60k" )
	PORT_DIPSETTING(    0x00, "50k then every 90k" )
	PORT_DIPSETTING(    0x0c, "Every 60k" )
	PORT_DIPSETTING(    0x04, "Every 90k" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	NIHON_COINAGE_A
	NIHON_COINAGE_B_ALT
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
  PORT_DIPNAME( 0x80, 0x80, "Infinite Timer (Cheat)" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( cclimbr2 )
	PORT_START	/* IN0 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	NIHON_COINS

	PORT_START	/* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	NIHON_SYSTEM

	PORT_START	/* DSW0 */
	NIHON_LIVES
/*	PORT_DIPNAME( 0x04, 0x04, "1st Bonus Life" )*/
/*	PORT_DIPSETTING(    0x04, "30k" )*/
/*	PORT_DIPSETTING(    0x00, "60k" )*/
/*	PORT_DIPNAME( 0x08, 0x08, "2nd Bonus Life" )*/
/*	PORT_DIPSETTING(    0x08, "70k" )*/
/*	PORT_DIPSETTING(    0x00, "00k" )*/
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "30K and 100k" )
	PORT_DIPSETTING(    0x08, "60k and 130k" )
	PORT_DIPSETTING(    0x04, "30k only" )
	PORT_DIPSETTING(    0x00, "60k only" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x00, "Normal" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	NIHON_COINAGE_A
	NIHON_COINAGE_B
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, "3 Times" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Partial Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( armedf )
	PORT_START	/* IN0 */
	NIHON_SINGLE_JOYSTICK(1)
	NIHON_COINS

	PORT_START	/* IN1 */
	NIHON_SINGLE_JOYSTICK(2)
	NIHON_SYSTEM

	PORT_START	/* DSW0 */
	NIHON_LIVES
/*	PORT_DIPNAME( 0x04, 0x04, "1st Bonus Life" )*/
/*	PORT_DIPSETTING(    0x04, "20k" )*/
/*	PORT_DIPSETTING(    0x00, "40k" )*/
/*	PORT_DIPNAME( 0x08, 0x08, "2nd Bonus Life" )*/
/*	PORT_DIPSETTING(    0x08, "60k" )*/
/*	PORT_DIPSETTING(    0x00, "80k" )*/
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20k then every 60k" )
	PORT_DIPSETTING(    0x04, "20k then every 80k" )
	PORT_DIPSETTING(    0x08, "40k then every 60k" )
	PORT_DIPSETTING(    0x00, "40k then every 80k" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* DSW1 */
	NIHON_COINAGE_A
	NIHON_COINAGE_B
	PORT_DIPNAME( 0x30, 0x00, "Allow Continue" )		/* not in the "test mode"*/
	PORT_DIPSETTING(    0x30, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, "3 Times" )
	PORT_DIPSETTING(    0x10, "5 Times" )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )	/* not in the "test mode"*/
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout char_layout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxLayout tile_layout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24,
			32+4, 32+0, 32+12, 32+8, 32+20, 32+16, 32+28, 32+24 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8
};

static struct GfxLayout sprite_layout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, RGN_FRAC(1,2)+4, RGN_FRAC(1,2)+0, 12, 8, RGN_FRAC(1,2)+12, RGN_FRAC(1,2)+8,
			20, 16, RGN_FRAC(1,2)+20, RGN_FRAC(1,2)+16, 28, 24, RGN_FRAC(1,2)+28, RGN_FRAC(1,2)+24 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &char_layout,		 0*16,	32 },
	{ REGION_GFX2, 0, &tile_layout,		64*16,	32 },
	{ REGION_GFX3, 0, &tile_layout,		96*16,	32 },
	{ REGION_GFX4, 0, &sprite_layout,	32*16,	32 },
	{ -1 } /* end of array */
};



static struct YM3812interface ym3812_interface =
{
	1,				/* 1 chip (no more supported) */
	4000000,        /* 4 MHz */
	{ 50 }         /* (not supported) */
};

static struct DACinterface dac_interface =
{
	2,	/* 2 channels */
	{ 100,100 },
};

static struct DACinterface cclimbr2_dac_interface =
{
	2,	/* 2 channels */
	{ 40, 40 },
};



static MACHINE_DRIVER_START( terraf )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* 8 MHz?? */
	MDRV_CPU_MEMORY(terraf_readmem,terraf_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz???? */
	MDRV_CPU_MEMORY(soundreadmem,soundwritemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,128)

	MDRV_FRAMES_PER_SECOND(57)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(12*8, (64-12)*8-1, 1*8, 31*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_EOF(armedf)
	MDRV_VIDEO_START(armedf)
	MDRV_VIDEO_UPDATE(armedf)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( kodure )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* 8 MHz?? */
	MDRV_CPU_MEMORY(kodure_readmem,kodure_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz???? */
	MDRV_CPU_MEMORY(soundreadmem,soundwritemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,128)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(14*8, (64-14)*8-1, 2*8, 30*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_EOF(armedf)
	MDRV_VIDEO_START(armedf)
	MDRV_VIDEO_UPDATE(armedf)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( armedf )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* 8 MHz?? */
	MDRV_CPU_MEMORY(armedf_readmem,armedf_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz???? */
	MDRV_CPU_MEMORY(soundreadmem,soundwritemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,128)

	MDRV_FRAMES_PER_SECOND(57)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(12*8, (64-12)*8-1, 1*8, 31*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_EOF(armedf)
	MDRV_VIDEO_START(armedf)
	MDRV_VIDEO_UPDATE(armedf)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( cclimbr2 )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* 8 MHz?? */
	MDRV_CPU_MEMORY(cclimbr2_readmem,cclimbr2_writemem)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz???? */
	MDRV_CPU_MEMORY(cclimbr2_soundreadmem,cclimbr2_soundwritemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,128)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(14*8, (64-14)*8-1, 2*8, 30*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_EOF(armedf)
	MDRV_VIDEO_START(armedf)
	MDRV_VIDEO_UPDATE(armedf)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, cclimbr2_dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( legion )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* 8 MHz?? */
	MDRV_CPU_MEMORY(cclimbr2_readmem,legion_writemem)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz???? */
	MDRV_CPU_MEMORY(cclimbr2_soundreadmem,cclimbr2_soundwritemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,128)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(14*8, (64-14)*8-1, 2*8, 30*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_EOF(armedf)
	MDRV_VIDEO_START(legion)
	MDRV_VIDEO_UPDATE(armedf)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, cclimbr2_dac_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( legiono )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 8000000) /* 8 MHz?? */
	MDRV_CPU_MEMORY(cclimbr2_readmem,legiono_writemem)
	MDRV_CPU_VBLANK_INT(irq2_line_hold,1)

	MDRV_CPU_ADD(Z80, 3072000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)	/* 3.072 MHz???? */
	MDRV_CPU_MEMORY(cclimbr2_soundreadmem,cclimbr2_soundwritemem)
	MDRV_CPU_PORTS(readport,writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,128)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_VISIBLE_AREA(14*8, (64-14)*8-1, 2*8, 30*8-1 )
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_EOF(armedf)
	MDRV_VIDEO_START(legion)
	MDRV_VIDEO_UPDATE(armedf)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, cclimbr2_dac_interface)
MACHINE_DRIVER_END

ROM_START( legion )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 64K*8 for 68000 code */
	ROM_LOAD16_BYTE( "lg1.bin", 0x000001, 0x010000, CRC(c4aeb724) SHA1(b4c0383f3b1fa6b1d5bdab0f3a5293c89a82a474) )
	ROM_LOAD16_BYTE( "lg3.bin", 0x000000, 0x010000, CRC(777e4935) SHA1(225766940059b4c12e69332ea77eb618dbd1467b) )
	ROM_LOAD16_BYTE( "legion.1b", 0x020001, 0x010000, CRC(c306660a) SHA1(31c6b868ba07677b5110c577335873354bff596f) ) /* lg2*/
	ROM_LOAD16_BYTE( "legion.1d", 0x020000, 0x010000, CRC(c2e45e1e) SHA1(95cc359145b1b03123262891feed358407ba105a) ) /* lg4*/

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "legion.1h", 0x00000, 0x04000, CRC(2ca4f7f0) SHA1(7cf997af9dd74ced9d28c047069ccfb67d72e257) ) /* lg9*/
	ROM_LOAD( "legion.1i", 0x04000, 0x08000, CRC(79f4a827) SHA1(25e4c1b5b8466627244b7226310e67e4261333b6) ) /* lg10*/

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "lg8.bin", 0x00000, 0x08000, CRC(e0596570) SHA1(68ddc950efc55a16e6abc699e3bad18ea19d579f) )

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1e", 0x00000, 0x10000, CRC(a9d70faf) SHA1(8b8b60ae49c55e931d6838e863463f6b2bf7adb0) ) /* lg5*/
	ROM_LOAD( "legion.1f", 0x18000, 0x08000, CRC(f018313b) SHA1(860bc9937202dc3a40c9fa7caad11c2c2aa19f5c) ) /* lg6*/

	ROM_REGION( 0x20000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1l", 0x00000, 0x10000, CRC(29b8adaa) SHA1(10338ebe7324960683de1f796dd311ed662e42b4) ) /* lg13*/

	ROM_REGION( 0x20000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1k", 0x000000, 0x010000, CRC(ff5a0db9) SHA1(9308deb363d3b7686cc69485ec14201dd68f9a97) ) /* lg12*/
	ROM_LOAD( "legion.1j", 0x010000, 0x010000, CRC(bae220c8) SHA1(392ae0fb0351dcad7b0e8e0ed4a1dc6e07f493df) ) /* lg11*/

	ROM_REGION( 0x4000, REGION_GFX5, 0 )	/* data for mcu/blitter */
	ROM_LOAD ( "lg7.bin", 0x0000, 0x4000, CRC(533e2b58) SHA1(a13ea4a530038760ffa87713903c59a932452717) )
ROM_END

ROM_START( legiono )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 64K*8 for 68000 code */
	ROM_LOAD16_BYTE( "legion.1a", 0x000001, 0x010000, CRC(8c0cda1d) SHA1(14b93d4fb4381ebc6a4ccdb480089bf69c6f474b) )
	ROM_LOAD16_BYTE( "legion.1c", 0x000000, 0x010000, CRC(21226660) SHA1(ee48812d6ec9d4dccc58684164916f91b71aabf2) )
	ROM_LOAD16_BYTE( "legion.1b", 0x020001, 0x010000, CRC(c306660a) SHA1(31c6b868ba07677b5110c577335873354bff596f) )
	ROM_LOAD16_BYTE( "legion.1d", 0x020000, 0x010000, CRC(c2e45e1e) SHA1(95cc359145b1b03123262891feed358407ba105a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "legion.1h", 0x00000, 0x04000, CRC(2ca4f7f0) SHA1(7cf997af9dd74ced9d28c047069ccfb67d72e257) )
	ROM_LOAD( "legion.1i", 0x04000, 0x08000, CRC(79f4a827) SHA1(25e4c1b5b8466627244b7226310e67e4261333b6) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1g", 0x00000, 0x08000, CRC(c50b0125) SHA1(83b5e9707152d97777fb65fa8820ba34ec2fac8d) )

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1e", 0x00000, 0x10000, CRC(a9d70faf) SHA1(8b8b60ae49c55e931d6838e863463f6b2bf7adb0) )
	ROM_LOAD( "legion.1f", 0x18000, 0x08000, CRC(f018313b) SHA1(860bc9937202dc3a40c9fa7caad11c2c2aa19f5c) )

	ROM_REGION( 0x20000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1l", 0x00000, 0x10000, CRC(29b8adaa) SHA1(10338ebe7324960683de1f796dd311ed662e42b4) )

	ROM_REGION( 0x20000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "legion.1k", 0x000000, 0x010000, CRC(ff5a0db9) SHA1(9308deb363d3b7686cc69485ec14201dd68f9a97) )
	ROM_LOAD( "legion.1j", 0x010000, 0x010000, CRC(bae220c8) SHA1(392ae0fb0351dcad7b0e8e0ed4a1dc6e07f493df) )
ROM_END

ROM_START( terraf )
	ROM_REGION( 0x50000, REGION_CPU1, 0 )	/* 64K*8 for 68000 code */
	ROM_LOAD16_BYTE( "terrafor.014", 0x00000, 0x10000, CRC(8e5f557f) SHA1(3462a58146c3f33bf8686adbd2ead25dae3804a8) )
	ROM_LOAD16_BYTE( "terrafor.011", 0x00001, 0x10000, CRC(5320162a) SHA1(eaffafcaf146cdddb03f40f92ce23dfd096eb89e) )
	ROM_LOAD16_BYTE( "terrafor.013", 0x20000, 0x10000, CRC(a86951e0) SHA1(804cc6f143993f5a9d5f3798e971d7abfe94c3a8) )
	ROM_LOAD16_BYTE( "terrafor.010", 0x20001, 0x10000, CRC(58b5f43b) SHA1(9df77235c0b7ac5af4258c04bd90d0a86ccc86b0) )
	ROM_LOAD16_BYTE( "terrafor.012", 0x40000, 0x08000, CRC(4f0e1d76) SHA1(b8636acde7547358663b94bdc8d49b5cc6b596eb) )
	ROM_LOAD16_BYTE( "terrafor.009", 0x40001, 0x08000, CRC(d1014280) SHA1(5ee8d71d77b31b25cce2bf1953c0a5166313a857) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "terrafor.001", 0x00000, 0x10000, CRC(eb6b4138) SHA1(04c53bf46d87a156d3fad86f051985d0df79bd20) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.008", 0x00000, 0x08000, CRC(bc6f7cbc) SHA1(20b8a34de4bfa0c2fdcd2f7743a0ab35141f4bf9) ) /* characters */

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.006", 0x00000, 0x10000, CRC(25d23dfd) SHA1(da32895c1aca403209b7fb181fa4fa23a8e74d32) ) /* foreground tiles */
	ROM_LOAD( "terrafor.007", 0x10000, 0x10000, CRC(b9b0fe27) SHA1(983c48239ba1524b517f89f281f2b70564bea1e9) )

	ROM_REGION( 0x20000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.004", 0x00000, 0x10000, CRC(2144d8e0) SHA1(ed89da11abf3d79753b478603009970c2600ab60) ) /* background tiles */
	ROM_LOAD( "terrafor.005", 0x10000, 0x10000, CRC(744f5c9e) SHA1(696223a087bb575c7cfaba11e682b221ada461e4) )

	ROM_REGION( 0x20000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.003", 0x00000, 0x10000, CRC(d74085a1) SHA1(3f6ba85dbd6e48a502c115b2d322a586fc4f56c9) ) /* sprites */
	ROM_LOAD( "terrafor.002", 0x10000, 0x10000, CRC(148aa0c5) SHA1(8d8a565540e91b384a9c154522501921b7da4d4e) )

	ROM_REGION( 0x4000, REGION_GFX5, 0 )	/* data for mcu/blitter */
	ROM_LOAD( "tf.10",        0x0000, 0x4000, CRC(ac705812) SHA1(65be46ee959d8478cb6dffb25e61f7742276997b) )

	ROM_REGION( 0x0100, REGION_PROMS, 0 )
	ROM_LOAD( "tf.clr",       0x0000, 0x0100, CRC(81244757) SHA1(6324f63e571f0f7a0bb9eb97f9994809db79493f) )	/* ??? */
ROM_END

ROM_START( terrafu )
	ROM_REGION( 0x50000, REGION_CPU1, 0 )	/* 64K*8 for 68000 code */
	ROM_LOAD16_BYTE( "tf.8",         0x00000, 0x10000, CRC(fea6dd64) SHA1(682eae338ce14808f134897f594fae1c69e75a1a) )
	ROM_LOAD16_BYTE( "tf.3",         0x00001, 0x10000, CRC(02f9d05a) SHA1(88985373bc3cffbc838e0b701ecd732a417975a1) )
	ROM_LOAD16_BYTE( "tf.7",         0x20000, 0x10000, CRC(fde8de7e) SHA1(6b0d27ec49c8c0609c110ad97938bec8c077ad18) )
	ROM_LOAD16_BYTE( "tf.2",         0x20001, 0x10000, CRC(db987414) SHA1(0a1734794c626cf9083d7854c9000c5daadfc3fd) )
	ROM_LOAD16_BYTE( "tf.6",         0x40000, 0x08000, CRC(b91e9ba3) SHA1(33e5272d1691859a2bb1f340eb4bdfdd5d73a5d4) )
	ROM_LOAD16_BYTE( "tf.1",         0x40001, 0x08000, CRC(d6e22375) SHA1(c84fc19700b65ee36b0c7d75cd7c97f86c7f719d) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "terrafor.001", 0x00000, 0x10000, CRC(eb6b4138) SHA1(04c53bf46d87a156d3fad86f051985d0df79bd20) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.008", 0x00000, 0x08000, CRC(bc6f7cbc) SHA1(20b8a34de4bfa0c2fdcd2f7743a0ab35141f4bf9) ) /* characters */

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.006", 0x00000, 0x10000, CRC(25d23dfd) SHA1(da32895c1aca403209b7fb181fa4fa23a8e74d32) ) /* foreground tiles */
	ROM_LOAD( "terrafor.007", 0x10000, 0x10000, CRC(b9b0fe27) SHA1(983c48239ba1524b517f89f281f2b70564bea1e9) )

	ROM_REGION( 0x20000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.004", 0x00000, 0x10000, CRC(2144d8e0) SHA1(ed89da11abf3d79753b478603009970c2600ab60) ) /* background tiles */
	ROM_LOAD( "terrafor.005", 0x10000, 0x10000, CRC(744f5c9e) SHA1(696223a087bb575c7cfaba11e682b221ada461e4) )

	ROM_REGION( 0x20000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "terrafor.003", 0x00000, 0x10000, CRC(d74085a1) SHA1(3f6ba85dbd6e48a502c115b2d322a586fc4f56c9) ) /* sprites */
	ROM_LOAD( "terrafor.002", 0x10000, 0x10000, CRC(148aa0c5) SHA1(8d8a565540e91b384a9c154522501921b7da4d4e) )

	ROM_REGION( 0x4000, REGION_GFX5, 0 )	/* data for mcu/blitter */
	ROM_LOAD( "tf.10",        0x0000, 0x4000, CRC(ac705812) SHA1(65be46ee959d8478cb6dffb25e61f7742276997b) )

	ROM_REGION( 0x0100, REGION_PROMS, 0 )
	ROM_LOAD( "tf.clr",       0x0000, 0x0100, CRC(81244757) SHA1(6324f63e571f0f7a0bb9eb97f9994809db79493f) )	/* ??? */
ROM_END

ROM_START( kodure )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 64K*8 for 68000 code */
	ROM_LOAD16_BYTE( "kodure8.6e", 0x00000, 0x10000, CRC(6bbfb1e6) SHA1(ffc8f835e28ff5f5b11f826b74ac2712c3018178) )
	ROM_LOAD16_BYTE( "kodure3.6h", 0x00001, 0x10000, CRC(f9178ec8) SHA1(68085b6030f4d1f89931263df68635b3b276e9f0) )
	ROM_LOAD16_BYTE( "kodure7.5e", 0x20000, 0x10000, CRC(a7ee09bb) SHA1(70ea2ab09b44e9188eb9500d9663d11e521caa1a) )
	ROM_LOAD16_BYTE( "kodure2.5h", 0x20001, 0x10000, CRC(236d820f) SHA1(e8784c0bbfe22e63a442a8eac18247b740f437a8) )
	ROM_LOAD16_BYTE( "kodure6.3e", 0x40000, 0x10000, CRC(9120e728) SHA1(af2ce368d66d01cbad136ae119b31b1701ad0595) )
	ROM_LOAD16_BYTE( "kodure1.3h", 0x40001, 0x10000, CRC(345fe7a5) SHA1(56ad809cf4a609447cce3e0181ff86e3f0e8966c) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "kodure11.17k", 0x00000, 0x10000, CRC(dba51e2d) SHA1(49e799d39d298cd3e01602ae5a2d123dfbfa9134) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "kodure9.11e", 0x00000, 0x08000, CRC(e041356e) SHA1(3e4c8564e7a8c940bbe72db11759903aa295287f) )	/* characters */

	ROM_REGION( 0x40000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "kodure5.15h", 0x00000, 0x20000, CRC(0b510258) SHA1(e7ec89faa574b755605ddb67d6c908a9f5f2d6ac) )	/* foreground tiles */
	ROM_LOAD( "kodure4.14h", 0x20000, 0x10000, CRC(fb8e13e6) SHA1(f2eafcf6d7362dc62e808f582a7bd2970e5e1ad1) )

	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "kodure14.8a", 0x00000, 0x10000, CRC(94a9c3d0) SHA1(7a5d810ea370d158b2099c17f4d656fbd3deeac8) )	/* background tiles */

	ROM_REGION( 0x40000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "kodure12.8d", 0x00000, 0x20000, CRC(15f4021d) SHA1(b2ba6fda1a7bdaae97de4b0157b9b656b4385e08) )	/* sprites */
	ROM_LOAD( "kodure13.9d", 0x20000, 0x20000, CRC(b3b6c753) SHA1(9ad061cac9558320b5cfd1ac1ac8d7f1788270cc) )

	ROM_REGION( 0x4000, REGION_GFX5, 0 )	/* data for mcu/blitter */
	ROM_LOAD( "kodure10.11c", 0x0000, 0x4000, CRC(f48be21d) SHA1(5d6db049f30cab98f672814a86a06609c1fa8fb4) )

	ROM_REGION( 0x0100, REGION_PROMS, 0 )
	ROM_LOAD( "tf.11j", 0x0000, 0x0100, CRC(81244757) SHA1(6324f63e571f0f7a0bb9eb97f9994809db79493f) )	/* ??? */
ROM_END

ROM_START( cclimbr2 )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 64K*8 for 68000 code */
	ROM_LOAD16_BYTE( "4.bin", 0x00000, 0x10000, CRC(7922ea14) SHA1(4395c1dfdeeba8173cce23b5453185c3ff976980) )
	ROM_LOAD16_BYTE( "1.bin", 0x00001, 0x10000, CRC(2ac7ed67) SHA1(5e9c3ca4f5b259ed7b08db8353be7d36ac947578) )
	ROM_LOAD16_BYTE( "6.bin", 0x20000, 0x10000, CRC(7905c992) SHA1(5e3ddb3b8127476b763578c3717ffe85aa6b342a) )
	ROM_LOAD16_BYTE( "5.bin", 0x20001, 0x10000, CRC(47be6c1e) SHA1(cbd928458087199e63020956c5b61925c3f055f4) )
	ROM_LOAD16_BYTE( "3.bin", 0x40000, 0x10000, CRC(1fb110d6) SHA1(a478096b4b075ff655d079e43151d6b8375f0caa) )
	ROM_LOAD16_BYTE( "2.bin", 0x40001, 0x10000, CRC(0024c15b) SHA1(0cd69a24139e878c09d4de37e4d102851765168f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "11.bin", 0x00000, 0x04000, CRC(fe0175be) SHA1(5c50fa07d8fa61d58a825bbc2cc5a7b85ff3e42e) )
	ROM_LOAD( "12.bin", 0x04000, 0x08000, CRC(5ddf18f2) SHA1(b66da5ad400d00b07160986e4841a309a3572bd1) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "10.bin", 0x00000, 0x08000, CRC(7f475266) SHA1(73d544731fcfd7266bca451880120c555d19ea5d) ) /* characters */

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "7.bin",  0x00000, 0x10000, CRC(cbdd3906) SHA1(0525599a5981f1e25ec6faf008e547da7a9ee2cb) ) /* foreground tiles */
	ROM_LOAD( "8.bin",  0x10000, 0x10000, CRC(b2a613c0) SHA1(1d92b85a0dd4b7e533677c454ec23359867defda) )

	ROM_REGION( 0x20000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "17.bin", 0x00000, 0x10000, CRC(e24bb2d7) SHA1(8f4b8410b77a50ae735d72f2f37e63784ebc10d9) ) /* background tiles */
	ROM_LOAD( "18.bin", 0x10000, 0x10000, CRC(56834554) SHA1(6d579c32fb57eb4eddc062cb2cc78b546f6607b2) )

	ROM_REGION( 0x40000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "15.bin", 0x00000, 0x10000, CRC(4bf838be) SHA1(6b1d7448caf406e47268a1276225bb0619b80cc9) ) /* sprites */
	ROM_LOAD( "16.bin", 0x10000, 0x10000, CRC(21a265c5) SHA1(a2b3a1e5aa545030d933c0f058f4f9a18e1af1c9) )
	ROM_LOAD( "13.bin", 0x20000, 0x10000, CRC(6b6ec999) SHA1(7749ce435f497732bd1b6958974cd95e960fc9fe) )
	ROM_LOAD( "14.bin", 0x30000, 0x10000, CRC(f426a4ad) SHA1(facccb21ca73c560d3a38e05e677782516d5b0c0) )

	ROM_REGION( 0x4000, REGION_GFX5, 0 )	/* data for mcu/blitter */
	ROM_LOAD( "9.bin",  0x0000, 0x4000, CRC(740d260f) SHA1(5b4487930c7a1fb0a796aec2243bec631b1b5104) )
ROM_END

ROM_START( armedf )
	ROM_REGION( 0x60000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "af_06.rom", 0x00000, 0x10000, CRC(c5326603) SHA1(135a8d96d792cf9b55e39e21160ee419be76d28a) )
	ROM_LOAD16_BYTE( "af_01.rom", 0x00001, 0x10000, CRC(458e9542) SHA1(22e4281eaac6b9b04af324cc96b5f3e4d1cefe43) )
	ROM_LOAD16_BYTE( "af_07.rom", 0x20000, 0x10000, CRC(cc8517f5) SHA1(93e4d3707a48551af89cadd0e016ddb65285a005) )
	ROM_LOAD16_BYTE( "af_02.rom", 0x20001, 0x10000, CRC(214ef220) SHA1(0c32349afc31fbcd825695679540a024f1e1acb2) )
	ROM_LOAD16_BYTE( "af_08.rom", 0x40000, 0x10000, CRC(d1d43600) SHA1(1a473b4958a02a33c0a02e7e72a70a9ee0c68c50) )
	ROM_LOAD16_BYTE( "af_03.rom", 0x40001, 0x10000, CRC(bbe1fe2d) SHA1(f47be23c7564b106d636d49d5f1da47daecd31df) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* Z80 code (sound) */
	ROM_LOAD( "af_10.rom", 0x00000, 0x10000, CRC(c5eacb87) SHA1(33af84b48fbda26729975b02cfb70f23c0bce6a2) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "af_09.rom", 0x00000, 0x08000, CRC(7025e92d) SHA1(e590682092c25bbfb674afeccbfc0e613c51d188) ) /* characters */

	ROM_REGION( 0x20000, REGION_GFX2, ROMREGION_DISPOSE )
	ROM_LOAD( "af_04.rom", 0x00000, 0x10000, CRC(44d3af4f) SHA1(0c2cb54357c314e43cec6f959fe9d4a2c8bc8834) ) /* foreground tiles */
	ROM_LOAD( "af_05.rom", 0x10000, 0x10000, CRC(92076cab) SHA1(f47424817373a6735da2b2049b53da5b38178cec) )

	ROM_REGION( 0x20000, REGION_GFX3, ROMREGION_DISPOSE )
	ROM_LOAD( "af_14.rom", 0x00000, 0x10000, CRC(8c5dc5a7) SHA1(758140ddb9e60fb3950fe58bf53c7aea769a1a94) ) /* background tiles */
	ROM_LOAD( "af_13.rom", 0x10000, 0x10000, CRC(136a58a3) SHA1(5481e3ce404881a0470f8740f0de6e42283bedf2) )

	ROM_REGION( 0x40000, REGION_GFX4, ROMREGION_DISPOSE )
	ROM_LOAD( "af_11.rom", 0x00000, 0x20000, CRC(b46c473c) SHA1(d8573225e2d8b133b3bdd4fa5a12d445c71d5e0e) ) /* sprites */
	ROM_LOAD( "af_12.rom", 0x20000, 0x20000, CRC(23cb6bfe) SHA1(34cb013827206bea71f5336b308ba92bee688506) )
ROM_END


DRIVER_INIT( terraf )
{
	armedf_setgfxtype(0);
}

DRIVER_INIT( terrafu )
{
	armedf_setgfxtype(5);
}

DRIVER_INIT( armedf )
{
	armedf_setgfxtype(1);
}

DRIVER_INIT( kodure )
{
  data16_t *ROM = (data16_t *)memory_region(REGION_CPU1);

	/* patch "time over" bug. */
	ROM[0x1016c/2] = 0x4e71;
	/* ROM check at POST. */
	ROM[0x04fc6/2] = 0x4e71;
  
	armedf_setgfxtype(2);
}

DRIVER_INIT( legion )
{
#if LEGION_HACK
	/* This is a hack to allow you to use the extra features
         of 3 of the "Unused" Dip Switches (see notes above). */
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU1);
	RAM[0x0001d6/2] = 0x0001;
	/* To avoid checksum error */
	RAM[0x000488/2] = 0x4e71;
#endif

	armedf_setgfxtype(3);
}

DRIVER_INIT( legiono )
{
#if LEGION_HACK
	/* This is a hack to allow you to use the extra features
         of 3 of the "Unused" Dip Switches (see notes above). */
	data16_t *RAM = (data16_t *)memory_region(REGION_CPU1);
	RAM[0x0001d6/2] = 0x0001;
	/* No need to patch the checksum routine (see notes) ! */
#endif

	armedf_setgfxtype(6);
}

DRIVER_INIT( cclimbr2 )
{
	armedf_setgfxtype(4);
}


/*     YEAR, NAME,   PARENT,   MACHINE,  INPUT,    INIT,     MONITOR, COMPANY,     FULLNAME, FLAGS */
GAMEX( 1987, legion,   0,      legion,   legion,   legion,   ROT270, "Nichibutsu", "Legion - Spinner-87 (World ver 2.03)",  GAME_IMPERFECT_GRAPHICS | GAME_UNEMULATED_PROTECTION )
GAMEX( 1987, legiono,  legion, legiono,  legion,   legiono,  ROT270, "Nichibutsu", "Chouji Meikyuu Legion (Japan bootleg ver 1.05)",  GAME_IMPERFECT_GRAPHICS ) /* blitter protection removed */
GAMEX( 1987, terraf,   0,      terraf,   terraf,   terraf,   ROT0,   "Nichibutsu", "Terra Force",  GAME_IMPERFECT_GRAPHICS | GAME_UNEMULATED_PROTECTION )
GAMEX( 1987, terrafu,  terraf, terraf,   terraf,   terrafu,  ROT0,   "Nichibutsu USA", "Terra Force (US)",  GAME_IMPERFECT_GRAPHICS | GAME_UNEMULATED_PROTECTION )
GAMEX( 1987, kodure,   0,      kodure,   kodure,   kodure,   ROT0,   "Nichibutsu", "Kodure Ookami (Japan)",  GAME_IMPERFECT_GRAPHICS | GAME_UNEMULATED_PROTECTION )
GAMEX( 1988, cclimbr2, 0,      cclimbr2, cclimbr2, cclimbr2, ROT0,   "Nichibutsu", "Crazy Climber 2 (Japan)", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1988, armedf,   0,      armedf,   armedf,   armedf,   ROT270, "Nichibutsu", "Armed Formation", GAME_IMPERFECT_GRAPHICS )
