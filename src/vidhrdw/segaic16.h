/***************************************************************************

	Sega 16-bit common hardware

***************************************************************************/

/* globals */
extern data16_t *segaic16_tileram;
extern data16_t *segaic16_textram;
extern data16_t *segaic16_spriteram;
extern data16_t *segaic16_roadram;
extern struct tilemap *segaic16_tilemaps[];
extern UINT8 segaic16_tilemap_page;

/* palette handling */
void segaic16_init_palette(int entries);
WRITE16_HANDLER( segaic16_paletteram_w );

/* tilemap helpers */
int segaic16_init_virtual_tilemaps(int numpages, int palette_offset, void (*tile_cb)(int));
void segaic16_draw_virtual_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT16 pages, UINT16 xscroll, UINT16 yscroll, UINT32 flags, UINT32 priority);
WRITE16_HANDLER( segaic16_tileram_w );

/* from vidhrdw/segas16b.c */
VIDEO_START( system16b );
VIDEO_START( timscanr );
VIDEO_UPDATE( system16b );

void system16b_reset_video(void);
void system16b_set_draw_enable(int enable);
void system16b_set_screen_flip(int flip);
void system16b_configure_sprite_banks(int use_default);
void system16b_set_tile_bank(int which, int bank);

WRITE16_HANDLER( system16b_textram_w );
