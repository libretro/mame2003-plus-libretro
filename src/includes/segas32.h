/***************************************************************************

    Sega System 32/Multi 32 hardware

***************************************************************************/


/*----------- defined in video/segas32.c -----------*/

extern UINT16 *system32_spriteram;
extern UINT16 *system32_paletteram[2];

READ16_HANDLER( system32_spriteram_r );
WRITE16_HANDLER( system32_spriteram_w );

READ16_HANDLER( system32_paletteram_r );
WRITE16_HANDLER( system32_paletteram_w );
READ16_HANDLER( multi32_paletteram_0_r );
WRITE16_HANDLER( multi32_paletteram_0_w );
READ16_HANDLER( multi32_paletteram_1_r );
WRITE16_HANDLER( multi32_paletteram_1_w );

READ16_HANDLER( system32_mixer_r );
WRITE16_HANDLER( system32_mixer_w );

READ16_HANDLER( multi32_mixer_0_r );
READ16_HANDLER( multi32_mixer_1_r );
WRITE16_HANDLER( multi32_mixer_0_w );
WRITE16_HANDLER( multi32_mixer_1_w );

READ16_HANDLER( system32_sprite_control_r );
WRITE16_HANDLER( system32_sprite_control_w );
