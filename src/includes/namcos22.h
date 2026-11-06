/***************************************************************************

    Namco System 22 / Super System 22

    Backported from current MAME (Phil Stroffolino, hap, R. Belmont and
    others, BSD-3-Clause).

***************************************************************************/

#ifndef NAMCOS22_H
#define NAMCOS22_H

enum
{
	NAMCOS22_AIR_COMBAT22,
	NAMCOS22_ALPINE_RACER,
	NAMCOS22_CYBER_COMMANDO,
	NAMCOS22_CYBER_CYCLES,
	NAMCOS22_PROP_CYCLE,
	NAMCOS22_RAVE_RACER,
	NAMCOS22_RIDGE_RACER,
	NAMCOS22_RIDGE_RACER2,
	NAMCOS22_TIME_CRISIS,
	NAMCOS22_VICTORY_LAP,
	NAMCOS22_ACE_DRIVER,
	NAMCOS22_ALPINE_RACER_2,
	NAMCOS22_ALPINE_SURFER,
	NAMCOS22_TOKYO_WARS,
	NAMCOS22_AQUA_JET,
	NAMCOS22_DIRT_DASH,
	NAMCOS22_ARMADILLO_RACING
};

/* big endian views of the 32-bit RAMs (stored as native data32_t) */
#define NTHBYTE32(base, n)	(((base)[(n) >> 2] >> (8 * (~(n) & 3))) & 0xff)
#define NTHWORD32(base, n)	(((base)[(n) >> 1] >> (16 * (~(n) & 1))) & 0xffff)

#define SIGNED12(v)	((INT32)((UINT32)(v) << 20) >> 20)
#define SIGNED18(v)	((INT32)((UINT32)(v) << 14) >> 14)
#define SIGNED24(v)	((INT32)((UINT32)(v) << 8) >> 8)

/* drivers/namcos22.c */
extern int namcos22_gametype;
extern int namcos22_is_ss22;
extern data32_t *namcos22_polygonram;		/* 0x8000 dwords, 24 bits connected */
extern data32_t *namcos22_spriteram;
extern data32_t *namcos22_vics_data;
extern data32_t *namcos22_vics_control;
extern data32_t *namcos22_mixer;
extern data32_t *namcos22_paletteram;
extern data32_t *namcos22_cgram;			/* text layer tiles; the tilemap is at 0x1e000 */
extern data32_t *namcos22_czram;			/* System 22 */
extern data16_t *namcos22_shareram;
extern data8_t *namcos22_gamma_proms;		/* System 22 */
extern int namcos22_pdp_render_done;
extern int namcos22_render_refresh;
extern int namcos22_slave_simulation_active;
extern UINT32 namcos22_pdp_frame;
extern INT32 namcos22_point_read(offs_t offs);
extern void namcos22_pdp_handle_commands(UINT16 offs);
extern UINT8 namcos22_syscontrol[0x20];
extern int namcos22_irq_enabled, namcos22_irq_state;
void namcos22_draw_crosshairs(struct mame_bitmap *bitmap, const struct rectangle *cliprect);

/* vidhrdw/namcos22_vidhrdw.c */
VIDEO_START( namcos22s );
VIDEO_START( namcos22 );
VIDEO_UPDATE( namcos22s );
VIDEO_UPDATE( namcos22 );
VIDEO_STOP( namcos22 );
int namcos22_video_threads_available(void);
void namcos22_video_reset(void);
void namcos22_draw_direct_poly(const UINT16 *src);

WRITE32_HANDLER( namcos22_paletteram_w );
WRITE32_HANDLER( namcos22_cgram_w );
READ32_HANDLER( namcos22_tilemapattr_r );
WRITE32_HANDLER( namcos22_tilemapattr_w );
READ32_HANDLER( namcos22s_czattr_r );
WRITE32_HANDLER( namcos22s_czattr_w );
READ32_HANDLER( namcos22s_czram_r );
WRITE32_HANDLER( namcos22s_czram_w );
READ32_HANDLER( namcos22s_spotram_r );
WRITE32_HANDLER( namcos22s_spotram_w );
READ32_HANDLER( namcos22s_vics_control_r );
WRITE32_HANDLER( namcos22s_vics_control_w );
WRITE32_HANDLER( namcos22s_chipselect_w );

#endif
