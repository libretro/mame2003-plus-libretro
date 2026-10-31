/***************************************************************************

    Sega ST-V: interface between the driver and the video (VDP1, VDP2)

***************************************************************************/

#ifndef STV_H
#define STV_H

/* vidhrdw/stvvid.c */
extern UINT16 *vdp2_regs;
extern UINT32 *vdp2_vram;
extern UINT32 *vdp2_cram;
extern UINT32 *vdp1_vram;
extern UINT16 *vdp1_regs;

READ32_HANDLER( stv_vdp1_regs_r );
WRITE32_HANDLER( stv_vdp1_regs_w );
READ32_HANDLER( stv_vdp1_vram_r );
WRITE32_HANDLER( stv_vdp1_vram_w );
READ32_HANDLER( stv_vdp1_framebuffer0_r );
WRITE32_HANDLER( stv_vdp1_framebuffer0_w );
READ32_HANDLER( stv_vdp2_vram_r );
WRITE32_HANDLER( stv_vdp2_vram_w );
READ32_HANDLER( stv_vdp2_cram_r );
WRITE32_HANDLER( stv_vdp2_cram_w );
READ32_HANDLER( stv_vdp2_regs_r );
WRITE32_HANDLER( stv_vdp2_regs_w );

VIDEO_START( stv_vdp2 );
VIDEO_STOP( stv_vdp2 );
VIDEO_UPDATE( stv_vdp2 );

/* frame timing, from the driver's scanline interrupt */
int stv_vdp2_vblank_start_line(void);	/* the line vblank-in happens on */
void stv_vdp2_scanline(int line);	/* the start of each line, 0 to 262 */
void stv_vdp1_video_update(void);	/* at vblank-in: frame change, automatic draw */
void stv_vdp1_vblank_erase(void);	/* one line after vblank-in */

#endif
