/*** Video *******************************************************************/
/*
	PGM (IGS023) video, as the igs023_video device of current MAME:

	- sprites with the zoom / shrink tables (0xb01000), drawn from the end of
	  the list so that earlier entries are on top, with a priority bitmap:
	  a sprite pixel is not drawn over one already drawn, and a sprite with
	  the priority bit set ("under the background") is hidden where the
	  background is opaque
	- the control register (0xb0e000): bit 0 sprite DMA, bit 2 IRQ4 (every
	  62 lines), bit 3 IRQ6 (vblank), bit 11 text layer off, bit 12
	  background off, bit 13 high priority sprites off
	- the sprite list is copied by the DMA at the end of the frame, only when
	  enabled, with the per-word masks of the hardware; a sprite whose size
	  word (bits 0-14) is zero ends the list
	- the screen is filled with pen 0x3ff
*/

#include "driver.h"
#include "state.h"

extern data16_t *pgm_mainram, *pgm_bg_videoram, *pgm_tx_videoram, *pgm_videoregs, *pgm_rowscrollram;
static struct tilemap *pgm_tx_tilemap, *pgm_bg_tilemap;
static data16_t *pgm_spritebufferram; /* buffered spriteram (256 sprites, 8 words each) */

extern data8_t *pgm_sprite_a_region;   /* sprite colour data, one byte per pixel */
extern size_t	pgm_sprite_a_region_allocate;

#define PGM_CTRL	( pgm_videoregs[ 0xe000 / 2 ] )

/*** Sprites *****************************************************************/

struct pgm_sprite
{
	int x, y, flip, color, pri, width, height;
	UINT32 offs;
	UINT32 xzoom, yzoom;
	int xgrow, ygrow;
};

static struct pgm_sprite pgm_spritelist[ 256 ];
static int pgm_num_sprites;

/* current sprite data pointers */
static const data8_t *pgm_bdata;
static UINT32 pgm_bmask;
static const data8_t *pgm_adata;
static UINT32 pgm_amask;
static UINT32 pgm_aoffset;	/* pixel index into the expanded colour data */
static UINT32 pgm_boffset;	/* word index into the mask data */

static const struct rectangle *pgm_clip;

static INLINE UINT16 pgm_mask_word( UINT32 index )
{
	UINT32 b = ( index * 2 ) & pgm_bmask;
	return pgm_bdata[ b ] | ( pgm_bdata[ ( b + 1 ) & pgm_bmask ] << 8 );
}

static INLINE void pgm_draw_pix( int xdrawpos, int pri, UINT16 *dest, UINT8 *destpri, UINT16 srcdat )
{
	if( xdrawpos >= pgm_clip->min_x && xdrawpos <= pgm_clip->max_x )
	{
		if( !( destpri[ xdrawpos ] & 1 ) )
		{
			if( !pri || !( destpri[ xdrawpos ] & 2 ) )
				dest[ xdrawpos ] = Machine->pens[ srcdat ];
		}
		destpri[ xdrawpos ] |= 1;
	}
}

static INLINE UINT16 pgm_sprite_pix( void )
{
	return pgm_adata[ pgm_aoffset++ & pgm_amask ];
}

static void pgm_draw_sprite_line( int wide, UINT16 *dest, UINT8 *destpri, UINT32 xzoom, int xgrow, int flip, int xpos, int pri, int realxsize, int palt, int draw )
{
	int xcnt, x, i;
	int xoffset = 0;
	int xcntdraw = 0;

	for( xcnt = 0; xcnt < wide; xcnt++ )
	{
		UINT16 msk = pgm_mask_word( pgm_boffset );

		for( x = 0; x < 16; x++ )
		{
			int xzoombit = ( xzoom >> ( xoffset & 0x1f ) ) & 1;
			xoffset++;

			if( !( msk & 1 ) )
			{
				UINT16 srcdat = pgm_sprite_pix() + palt * 32;

				if( xgrow || !xzoombit )
				{
					int pixel_column = xzoombit ? 2 : 1;
					for( i = 0; i < pixel_column; i++ )
					{
						if( draw )
							pgm_draw_pix( ( flip & 1 ) ? xpos + realxsize - xcntdraw : xpos + xcntdraw, pri, dest, destpri, srcdat );
						xcntdraw++;
					}
				}
			}
			else
			{
				if( xgrow || !xzoombit )
					xcntdraw += xzoombit ? 2 : 1;
			}

			msk >>= 1;
		}

		pgm_boffset++;
	}
}

static void pgm_draw_sprite( struct mame_bitmap *bitmap, const struct pgm_sprite *s )
{
	int ycnt, xcnt, ycntdraw, ydrawpos;
	int realysize = 0, realxsize = 0;
	int high = s->height, wide = s->width;
	UINT32 xzoom = s->xzoom, yzoom = s->yzoom;
	int xgrow = s->xgrow, ygrow = s->ygrow;

	/* the first two mask words hold the offset of the colour data */
	pgm_aoffset = ( ( pgm_mask_word( pgm_boffset + 1 ) << 16 ) | pgm_mask_word( pgm_boffset ) ) >> 2;
	pgm_aoffset *= 3;	/* three 5-bit pixels per colour word, expanded to bytes */
	pgm_boffset += 2;

	/* where drawing ends, for the flipped cases: the data must be drawn in ROM order */
	for( ycnt = 0; ycnt < high; ycnt++ )
	{
		int yzoombit = ( yzoom >> ( ycnt & 0x1f ) ) & 1;
		if( ygrow || !yzoombit )
			realysize += yzoombit ? 2 : 1;
	}
	realysize--;

	for( xcnt = 0; xcnt < wide * 16; xcnt++ )
	{
		int xzoombit = ( xzoom >> ( xcnt & 0x1f ) ) & 1;
		if( xgrow || !xzoombit )
			realxsize += xzoombit ? 2 : 1;
	}
	realxsize--;

	ycntdraw = 0;
	for( ycnt = 0; ycnt < high; ycnt++ )
	{
		int yzoombit = ( yzoom >> ( ycnt & 0x1f ) ) & 1;
		int times, t;

		if( yzoombit && !ygrow )
		{
			/* shrink: this line is skipped */
			pgm_draw_sprite_line( wide, NULL, NULL, xzoom, xgrow, s->flip, s->x, s->pri, realxsize, s->color, 0 );
			continue;
		}

		times = ( yzoombit && ygrow ) ? 2 : 1;	/* grow: this line twice */
		for( t = 0; t < times; t++ )
		{
			UINT32 save_a = pgm_aoffset, save_b = pgm_boffset;

			ydrawpos = ( s->flip & 2 ) ? s->y + realysize - ycntdraw : s->y + ycntdraw;

			if( ydrawpos >= pgm_clip->min_y && ydrawpos <= pgm_clip->max_y )
			{
				UINT16 *dest = (UINT16 *)bitmap->line[ ydrawpos ];
				UINT8 *destpri = (UINT8 *)priority_bitmap->line[ ydrawpos ];
				pgm_draw_sprite_line( wide, dest, destpri, xzoom, xgrow, s->flip, s->x, s->pri, realxsize, s->color, 1 );
			}
			else
			{
				pgm_draw_sprite_line( wide, NULL, NULL, xzoom, xgrow, s->flip, s->x, s->pri, realxsize, s->color, 0 );
				/* past the bottom (or the top when flipped): nothing more to draw */
				if( ( !( s->flip & 2 ) && ydrawpos > pgm_clip->max_y ) || ( ( s->flip & 2 ) && ydrawpos < pgm_clip->min_y ) )
					return;
			}
			ycntdraw++;

			if( t + 1 < times )
			{
				pgm_aoffset = save_a;
				pgm_boffset = save_b;
			}
		}
	}
}

static void pgm_draw_sprites( struct mame_bitmap *bitmap )
{
	int n;

	pgm_bdata = memory_region( REGION_GFX4 );
	pgm_bmask = memory_region_length( REGION_GFX4 ) - 1;
	pgm_adata = pgm_sprite_a_region;
	pgm_amask = pgm_sprite_a_region_allocate - 1;

	/* from the end of the list: earlier entries end up on top */
	for( n = pgm_num_sprites - 1; n >= 0; n-- )
	{
		const struct pgm_sprite *s = &pgm_spritelist[ n ];

		if( ( PGM_CTRL & 0x2000 ) && !s->pri )
			continue;

		pgm_boffset = s->offs;
		pgm_draw_sprite( bitmap, s );
	}
}

/*
	Sprite list (5 words per sprite, 8 words apart in the buffer):

	0  x------- -------- horizontal zoom (1) / shrink (0)
	   -xxxx--- -------- horizontal zoom table entry
	   -----xxx xxxxxxxx X position (11 bit signed)
	1  x------- -------- vertical zoom / shrink
	   -xxxx--- -------- vertical zoom table entry
	   ------xx xxxxxxxx Y position (10 bit signed)
	2  -x------ -------- flip Y
	   --x----- -------- flip X
	   ---xxxxx -------- palette
	   -------- x------- priority (under the background)
	   -------- -xxxxxxx mask data address MSB
	3  xxxxxxxx xxxxxxxx mask data address LSB
	4  -xxxxxx- -------- width (16 pixel units)
	   -------x xxxxxxxx height
*/
static void pgm_get_sprites( void )
{
	int sprite_num;
	const data16_t *zoomram = &pgm_videoregs[ 0x1000 / 2 ];

	pgm_num_sprites = 0;
	for( sprite_num = 0; sprite_num < 256; sprite_num++ )
	{
		const data16_t *src = &pgm_spritebufferram[ sprite_num * 8 ];
		struct pgm_sprite *s = &pgm_spritelist[ pgm_num_sprites ];
		int xzom, yzom;

		if( ( src[ 4 ] & 0x7fff ) == 0 )
			break;

		s->xgrow = ( src[ 0 ] >> 15 ) & 1;
		xzom = ( src[ 0 ] & 0x7800 ) >> 11;
		s->x = src[ 0 ] & 0x07ff;
		if( s->x & 0x400 ) s->x -= 0x800;

		s->ygrow = ( src[ 1 ] >> 15 ) & 1;
		yzom = ( src[ 1 ] & 0x7800 ) >> 11;
		s->y = src[ 1 ] & 0x03ff;
		if( s->y & 0x200 ) s->y -= 0x400;

		s->flip = ( src[ 2 ] & 0x6000 ) >> 13;
		s->color = ( src[ 2 ] & 0x1f00 ) >> 8;
		s->pri = ( src[ 2 ] >> 7 ) & 1;
		s->offs = ( ( src[ 2 ] & 0x007f ) << 16 ) | src[ 3 ];

		s->width = ( src[ 4 ] & 0x7e00 ) >> 9;
		s->height = src[ 4 ] & 0x01ff;

		/* as MAME: 0x10 - entry when growing */
		if( s->xgrow ) xzom = 0x10 - xzom;
		if( s->ygrow ) yzom = 0x10 - yzom;

		/* the last table entry reads as 1 (ddp3 leaves it at zero) */
		s->xzoom = ( xzom < 0x10 ) ? ( ( xzom == 0xf ) ? 1 : ( ( (UINT32)zoomram[ xzom * 2 ] << 16 ) | zoomram[ xzom * 2 + 1 ] ) ) : 0;
		s->yzoom = ( yzom < 0x10 ) ? ( ( yzom == 0xf ) ? 1 : ( ( (UINT32)zoomram[ yzom * 2 ] << 16 ) | zoomram[ yzom * 2 + 1 ] ) ) : 0;

		pgm_num_sprites++;
	}
}

/* sprite DMA: 256 sprites of 5 words from main RAM, with the bits each word keeps */
static void pgm_sprite_dma( void )
{
	static const data16_t sprite_ram_mask[ 5 ] = { 0xffff, 0xfbff, 0x7fff, 0xffff, 0xffff };
	int i, src, offs = 0;

	if( !( PGM_CTRL & 0x0001 ) )
		return;

	for( i = 0; i < 256; i++ )
	{
		for( src = 0; src < 5; src++ )
			pgm_spritebufferram[ i * 8 + src ] = pgm_mainram[ offs++ ] & sprite_ram_mask[ src ];
		if( ( pgm_spritebufferram[ i * 8 + 4 ] & 0x7fff ) == 0 )
			break;
	}
}

/*** TX Layer ****************************************************************/

WRITE16_HANDLER( pgm_tx_videoram_w )
{
	if (pgm_tx_videoram[offset] != data)
	{
		pgm_tx_videoram[offset] = data;
		tilemap_mark_tile_dirty(pgm_tx_tilemap,offset/2);
	}
}

static void get_pgm_tx_tilemap_tile_info(int tile_index)
{

/* 0x904000 - 0x90ffff is the Text Overlay Ram (pgm_tx_videoram)
	each tile uses 4 bytes, the tilemap is 64x128?

   the layer uses 4bpp 8x8 tiles from the 'T' roms
   colours from 0xA01000 - 0xA013FF

   scroll registers are at 0xB05000 (Y) and 0xB06000 (X)

	---- ---- ffpp ppp- nnnn nnnn nnnn nnnn

	n = tile number
	p = palette
	f = flip
*/
	int tileno,colour,flipyx;

	tileno = pgm_tx_videoram[tile_index *2] & 0xffff;
	colour = (pgm_tx_videoram[tile_index*2+1] & 0x3e) >> 1;
	flipyx = (pgm_tx_videoram[tile_index*2+1] & 0xc0) >> 6;

	if (tileno > 0xbfff) { tileno -= 0xc000 ; tileno += 0x20000; } /* not sure about this */

	SET_TILE_INFO(0,tileno,colour,TILE_FLIPYX(flipyx))
}

/*** BG Layer ****************************************************************/

WRITE16_HANDLER( pgm_bg_videoram_w )
{
	if (pgm_bg_videoram[offset] != data)
	{
		pgm_bg_videoram[offset] = data;
		tilemap_mark_tile_dirty(pgm_bg_tilemap,offset/2);
	}
}

static void get_pgm_bg_tilemap_tile_info(int tile_index)
{
	/* pretty much the same as tx layer */

	int tileno,colour,flipyx;

	tileno = pgm_bg_videoram[tile_index *2] & 0xffff;
	if (tileno > 0x7ff) tileno+=0x1000; /* Tiles 0x800+ come from the GAME Roms */
	colour = (pgm_bg_videoram[tile_index*2+1] & 0x3e) >> 1;
	flipyx = (pgm_bg_videoram[tile_index*2+1] & 0xc0) >> 6;

	SET_TILE_INFO(1,tileno,colour,TILE_FLIPYX(flipyx))
}

/*** Interrupts ****************************************************************/
/*
	IRQ6 at vblank and IRQ4 every 62 lines (a divider that runs on its own),
	each only while enabled in the control register; they stay asserted until
	their enable bit is cleared.
*/

static int pgm_irq4_pending, pgm_irq6_pending;

/* the 68000 interface keeps one interrupt level: a CLEAR_LINE on any line
   drops it, so only the highest pending level is set */
static void pgm_update_irqs( void )
{
	if( pgm_irq6_pending )
		cpu_set_irq_line( 0, 6, ASSERT_LINE );
	else if( pgm_irq4_pending )
		cpu_set_irq_line( 0, 4, ASSERT_LINE );
	else
		cpu_set_irq_line( 0, 6, CLEAR_LINE );
}

static void pgm_irq4_tick( int param )
{
	if( PGM_CTRL & 0x0004 )
	{
		pgm_irq4_pending = 1;
		pgm_update_irqs();
	}
}

void pgm_video_vblank( void )
{
	if( PGM_CTRL & 0x0008 )
	{
		pgm_irq6_pending = 1;
		pgm_update_irqs();
	}
}

WRITE16_HANDLER( pgm_videoregs_w )
{
	COMBINE_DATA( &pgm_videoregs[ offset ] );
	if( offset == 0xe000 / 2 )
	{
		if( !( PGM_CTRL & 0x0004 ) ) pgm_irq4_pending = 0;
		if( !( PGM_CTRL & 0x0008 ) ) pgm_irq6_pending = 0;
		pgm_update_irqs();
	}
}

static void *pgm_irq4_timer;

void pgm_video_reset( void )
{
	PGM_CTRL &= ~0x000c;
	pgm_irq4_pending = pgm_irq6_pending = 0;
	pgm_update_irqs();
}

static void pgm_video_postload( void )
{
	pgm_update_irqs();
}

/*** Video - Start / Update ****************************************************/

VIDEO_START( pgm )
{
	pgm_tx_tilemap= tilemap_create(get_pgm_tx_tilemap_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,32);
	if (!pgm_tx_tilemap) return 1;
	tilemap_set_transparent_pen(pgm_tx_tilemap,15);

	pgm_bg_tilemap = tilemap_create(get_pgm_bg_tilemap_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 32, 32,64,16);
	if (!pgm_bg_tilemap) return 1;
	tilemap_set_transparent_pen(pgm_bg_tilemap,31);
	tilemap_set_scroll_rows(pgm_bg_tilemap,16*32);

	pgm_spritebufferram = auto_malloc( 0x1000 );
	if (!pgm_spritebufferram) return 1;
	memset( pgm_spritebufferram, 0, 0x1000 );
	pgm_num_sprites = 0;

	/* IRQ4 divider: every 62 lines of 264, running all the time (as MAME) */
	pgm_irq4_timer = timer_alloc( pgm_irq4_tick );
	timer_adjust( pgm_irq4_timer, TIME_IN_HZ( Machine->drv->frames_per_second * 264.0 / 62.0 ), 0, TIME_IN_HZ( Machine->drv->frames_per_second * 264.0 / 62.0 ) );
	pgm_irq4_pending = pgm_irq6_pending = 0;

	state_save_register_UINT16( "pgm_video", 0, "spritebuffer", pgm_spritebufferram, 0x1000 / 2 );
	state_save_register_int( "pgm_video", 0, "irq4_pending", &pgm_irq4_pending );
	state_save_register_int( "pgm_video", 0, "irq6_pending", &pgm_irq6_pending );
	state_save_register_func_postload( pgm_video_postload );

	return 0;
}

VIDEO_UPDATE( pgm )
{
	int y;

	pgm_clip = cliprect;

	fillbitmap( bitmap, Machine->pens[ 0x3ff ], cliprect );	/* ddp2 IGS logo needs 0x3ff */
	fillbitmap( priority_bitmap, 0, cliprect );

	if( !( PGM_CTRL & 0x1000 ) )
	{
		tilemap_set_scrolly( pgm_bg_tilemap, 0, pgm_videoregs[ 0x2000 / 2 ] );
		for( y = cliprect->min_y; y <= cliprect->max_y; y++ )
			tilemap_set_scrollx( pgm_bg_tilemap, ( y + pgm_videoregs[ 0x2000 / 2 ] ) & 0x1ff, pgm_videoregs[ 0x3000 / 2 ] + pgm_rowscrollram[ y ] );
		tilemap_draw( bitmap, cliprect, pgm_bg_tilemap, 0, 2 );
	}

	pgm_get_sprites();
	pgm_draw_sprites( bitmap );

	if( !( PGM_CTRL & 0x0800 ) )
	{
		tilemap_set_scrolly( pgm_tx_tilemap, 0, pgm_videoregs[ 0x5000 / 2 ] );
		tilemap_set_scrollx( pgm_tx_tilemap, 0, pgm_videoregs[ 0x6000 / 2 ] );
		tilemap_draw( bitmap, cliprect, pgm_tx_tilemap, 0, 0 );
	}
}

VIDEO_EOF( pgm )
{
	/* the sprite list is copied by the DMA once a frame, when enabled */
	pgm_sprite_dma();
}
