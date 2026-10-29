/***************************************************************************

	Sega 315-5313 VDP (the Mega Drive / Genesis video chip)

	Used by the Sega System C/C2, Mega-Tech and Mega Play boards, the Mega
	Drive based bootlegs (vidhrdw/segac2_vidhrdw.c) and by System 18
	(vidhrdw/segas18.c). Each user starts the chip with its own
	configuration, so the hardware families share the VDP code but no
	state or behaviour.

***************************************************************************/

#ifndef SEGAVDP_H
#define SEGAVDP_H

/* where the CRAM (VDP palette) writes go */
#define SEGAVDP_CRAM_PALETTERAM	0	/* paletteram16[address / 2] (System C/C2, Mega Drive boards) */
#define SEGAVDP_CRAM_DIRECT		1	/* the 64 colors set directly from cram_base (System 18) */

struct segavdp_interface
{
	int		cram_mode;			/* SEGAVDP_CRAM_xxx */
	int		cram_base;			/* first palette entry of the VDP colors (SEGAVDP_CRAM_DIRECT) */
	int		bg_palbase;			/* initial base of the background palette */
	int		sp_palbase;			/* initial base of the sprite palette */
	int		scanbase;			/* screen row of the first VDP line (partial updates) */
	int		line_scale;			/* screen scanlines per VDP line (Mega-Tech: 2, the screen
								   has the BIOS display too), 0 = 1 */
	int		display_enable;		/* initial state of the board display enable */
	int		set_visible_area;	/* register 12 (H32/H40) resizes the screen */
	void	(*irq_update)(void);	/* called after a write to register 0 or 1, or NULL */
};

int  segavdp_start(const struct segavdp_interface *intf);

/* memory handlers (0xC00000-0xC0001F) */
READ16_HANDLER( segavdp_r );
WRITE16_HANDLER( segavdp_w );

/* VBLANK tracking: segavdp_eof() from VIDEO_EOF marks the start of VBLANK
   and ends it at scanline 0; segavdp_set_vblank() raises the flag early */
void segavdp_eof(void);
void segavdp_set_vblank(void);

/* Mega Play: the H-INT/V-INT are acknowledged by the 68000, and the HBLANK
   status bit covers the end of the line in H40 mode too */
void segavdp_set_iack_mode(int enable);

/* board controls */
void segavdp_enable_display(int enable);
void segavdp_set_palbases(int bg_palbase, int sp_palbase);
int  segavdp_get_bg_palbase(void);
int  segavdp_get_sp_palbase(void);
void segavdp_set_palbank(int palbank);
int  segavdp_get_palbank(void);
void segavdp_set_alt_palette(int enable);
int  segavdp_get_reg(int regnum);
int  segavdp_get_scanbase(void);

/* the VDP line timing, the cpu_getscanline() family scaled by line_scale */
int    segavdp_scanline(void);
int    segavdp_horzbeampos(void);
double segavdp_scanlinetime(int line);
double segavdp_scanlineperiod(void);

/* rendering: one line into a 320 pixel buffer; bgfill is the background
   pen (0 = the VDP background color, 0xffff = transparent) */
void segavdp_draw_line(UINT16 *dest, int line, int bgfill);

#endif
