/* Jaleco MegaSystem 32: shared between drivers/ms32.c and vidhrdw/ms32_vidhrdw.c */

extern UINT8  *ms32_priram;
extern UINT16 *ms32_palram;
extern UINT16 *ms32_rozram;
extern UINT16 *ms32_lineram;
extern UINT16 *ms32_sprram;
extern UINT16 *ms32_txram;
extern UINT16 *ms32_bgram;
extern data32_t ms32_roz_ctrl[0x60/4];
extern data32_t ms32_tx_scroll[0x18/4];
extern data32_t ms32_bg_scroll[0x18/4];
extern data32_t ms32_sprite_ctrl[0x80/4];
extern data32_t ms32_tilemaplayoutcontrol;

extern UINT16 *ms32_road_vram;
extern UINT16 *ms32_road_lineram;
extern data32_t ms32_road_ctrl[0x60/4];
extern int ms32_is_f1superb;
extern int ms32_palette_dirty;

WRITE32_HANDLER( ms32_brightness_w );
void ms32_video_init_ram(void);
void ms32_latch_sprites(void);
void ms32_f1superb_latch_tx(void);
VIDEO_START( ms32 );
VIDEO_UPDATE( ms32 );
