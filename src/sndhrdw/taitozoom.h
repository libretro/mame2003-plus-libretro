/***************************************************************************

  Taito Zoom ZSG-2 sound board (Taito FX-1B)

  MN10200 sound CPU (CPU 1), ZOOM ZSG-2 PCM chip, TMS57002 effects DSP.
  Backported from current MAME src/mame/sony/taito_zm.cpp (Olivier
  Galibert, hap, superctr, cam900).

***************************************************************************/

#ifndef TAITOZOOM_H
#define TAITOZOOM_H

/* main CPU side, 32-bit bus */
WRITE32_HANDLER( taitozoom_reg_w );			/* 0x1fb80000: data at +0, address at +2 */
WRITE32_HANDLER( taitozoom_sound_irq_w );	/* 0x1fba0000 */
READ32_HANDLER( taitozoom_sound_irq_r );	/* 0x1fbc0000 */
READ32_HANDLER( taitozoom_shared_r );		/* 0x1fbe0000-0x1fbe01ff, lanes 0 and 2 */
WRITE32_HANDLER( taitozoom_shared_w );

/* the MN10200 map */
extern const struct Memory_ReadAddress16 taitozoom_mn_readmem[];
extern const struct Memory_WriteAddress16 taitozoom_mn_writemem[];

extern struct CustomSound_interface taitozoom_custom_interface;

void taitozoom_init( void );	/* driver init */
void taitozoom_reset( void );	/* machine init */

#endif
