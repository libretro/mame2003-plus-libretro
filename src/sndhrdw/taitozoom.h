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

/* Taito G-NET: the ZSG-2 samples read by the driver (in flashes), as the */
/* use_flash mode of MAME; the MN10200 program is in REGION_CPU2 as on the */
/* FX-1B (the driver keeps its flash there). Set before the sound starts */
void taitozoom_set_flash( UINT32 (*samples_r)( UINT32 offset ) );
void taitozoom_device_reset( void );	/* the reset of the board (MAME device_reset) */

extern struct CustomSound_interface taitozoom_custom_interface;

void taitozoom_init( void );	/* driver init */
void taitozoom_reset( void );	/* machine init */

#endif
