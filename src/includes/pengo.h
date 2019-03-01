/*************************************************************************

	Sega Pengo

**************************************************************************/

/*----------- defined in vidhrdw/pengo.c -----------*/

PALETTE_INIT( pacman );
PALETTE_INIT( pengo );

VIDEO_START( pacman );
VIDEO_START( pengo );

WRITE_HANDLER( pengo_gfxbank_w );
WRITE_HANDLER( pengo_flipscreen_w );

VIDEO_UPDATE( pengo );

WRITE_HANDLER( vanvan_bgcolor_w );
VIDEO_UPDATE( vanvan );

extern data8_t *sprite_bank, *tiles_bankram;
WRITE_HANDLER( s2650games_videoram_w );
WRITE_HANDLER( s2650games_colorram_w );
WRITE_HANDLER( s2650games_scroll_w );
WRITE_HANDLER( s2650games_tilesbank_w );
WRITE_HANDLER( s2650games_flipscreen_w );
VIDEO_START( s2650games );
VIDEO_UPDATE( s2650games );
