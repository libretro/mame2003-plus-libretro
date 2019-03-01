/*************************************************************************

	Bally/Midway Jr. Pac-Man

**************************************************************************/

/*----------- defined in vidhrdw/jrpacman.c -----------*/

extern unsigned char *jrpacman_scroll,*jrpacman_bgpriority;
extern unsigned char *jrpacman_charbank,*jrpacman_spritebank;
extern unsigned char *jrpacman_palettebank,*jrpacman_colortablebank;

PALETTE_INIT( jrpacman );
VIDEO_START( jrpacman );

WRITE_HANDLER( jrpacman_videoram_w );
WRITE_HANDLER( jrpacman_palettebank_w );
WRITE_HANDLER( jrpacman_colortablebank_w );
WRITE_HANDLER( jrpacman_charbank_w );
WRITE_HANDLER( jrpacman_flipscreen_w );

VIDEO_UPDATE( jrpacman );
