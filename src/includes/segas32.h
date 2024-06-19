/***************************************************************************

    Sega System 32/Multi 32 hardware

***************************************************************************/


/*----------- defined in vidhrdw/segas32.c -----------*/

extern data16_t *system32_videoram;
extern data16_t *system32_spriteram;
extern data16_t *system32_paletteram[2];
extern data16_t system32_displayenable[2];
extern data16_t system32_tilebank_external;

extern void system32_set_vblank(int state);

READ16_HANDLER( system32_spriteram_r );
WRITE16_HANDLER( system32_spriteram_w );
READ32_HANDLER( multi32_spriteram_r );
WRITE32_HANDLER( multi32_spriteram_w );


READ16_HANDLER( system32_videoram_r );
WRITE16_HANDLER( system32_videoram_w );
READ32_HANDLER( multi32_videoram_r );
WRITE32_HANDLER( multi32_videoram_w );

READ16_HANDLER( system32_paletteram_r );
WRITE16_HANDLER( system32_paletteram_w );
READ32_HANDLER( multi32_paletteram_0_r );
WRITE32_HANDLER( multi32_paletteram_0_w );
READ32_HANDLER( multi32_paletteram_1_r );
WRITE32_HANDLER( multi32_paletteram_1_w );

READ16_HANDLER( system32_mixer_r );
WRITE16_HANDLER( system32_mixer_w );
/* should have mixer reads too */
WRITE16_HANDLER( multi32_mixer_0_w );
WRITE16_HANDLER( multi32_mixer_1_w );

READ16_HANDLER( system32_sprite_control_r );
WRITE16_HANDLER( system32_sprite_control_w );
READ32_HANDLER( multi32_sprite_control_r );
WRITE32_HANDLER( multi32_sprite_control_w );

VIDEO_START( system32 );
VIDEO_START( multi32 );

VIDEO_UPDATE( system32 );
VIDEO_UPDATE( multi32 );
