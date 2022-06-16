/***************************************************************************

	Sega 16-bit common hardware

***************************************************************************/

/* globals */
extern data16_t *segaic16_tileram;
extern data16_t *segaic16_textram;
extern data16_t *segaic16_spriteram;
extern struct tilemap *segaic16_tilemaps[];
extern UINT8 segaic16_tilemap_page;

/* palette handling */
void segaic16_init_palette(void);
WRITE16_HANDLER( segaic16_paletteram_w );

/* tilemap helpers */
int segaic16_init_virtual_tilemaps(int numpages, void (*tile_cb)(int));
void segaic16_draw_virtual_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT16 pages, UINT16 xscroll, UINT16 yscroll, UINT32 flags, UINT32 priority);
WRITE16_HANDLER( segaic16_tileram_w );

