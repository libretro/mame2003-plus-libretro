/***************************************************************************

    Capcom CPS-3

***************************************************************************/

#ifndef CPS3_H
#define CPS3_H

/* sndhrdw/cps3_sndhrdw.c */
void cps3_sound_set_base(const UINT8 *base, UINT32 length);
READ32_HANDLER( cps3_sound_r );
WRITE32_HANDLER( cps3_sound_w );
extern struct CustomSound_interface cps3_custom_interface;

#endif
