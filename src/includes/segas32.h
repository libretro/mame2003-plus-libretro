/***************************************************************************

    Sega System 32/Multi 32 hardware

***************************************************************************/


/*----------- defined in video/segas32.c -----------*/

extern UINT16 *system32_paletteram[2];
extern UINT16 *system32_mixerregs[2];

READ16_HANDLER( system32_paletteram_r );
WRITE16_HANDLER( system32_paletteram_w );
READ32_HANDLER( multi32_paletteram_0_r );
WRITE32_HANDLER( multi32_paletteram_0_w );
READ32_HANDLER( multi32_paletteram_1_r );
WRITE32_HANDLER( multi32_paletteram_1_w );
