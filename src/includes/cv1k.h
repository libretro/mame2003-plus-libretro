/***************************************************************************

	Cave CV1000 (cv1k.c, vidhrdw/cv1k_vidhrdw.c, sound/ymz770.c)

***************************************************************************/

#ifndef CV1K_H
#define CV1K_H

/* drivers/cv1k.c */
extern data32_t *cv1k_ram;
extern UINT32 cv1k_ram_size;

/* vidhrdw/cv1k_vidhrdw.c: the blitter */
VIDEO_START( cv1k );
VIDEO_UPDATE( cv1k );
READ32_HANDLER( cv1k_blitter_r );
WRITE32_HANDLER( cv1k_blitter_w );
data8_t cv1k_fpga_r(void);
void cv1k_fpga_w(data8_t data);
void cv1k_blitter_reset(void);

/* sound/ymz770.c */
extern struct CustomSound_interface ymz770_custom_interface;
void ymz770_write(int offset, data8_t data);
void ymz770_reset_chip(void);

#endif
