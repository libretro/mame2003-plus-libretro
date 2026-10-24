/*************************************************************************

	Epos games

**************************************************************************/

/*----------- defined in vidhrdw/epos.c -----------*/

PALETTE_INIT( epos );
VIDEO_START( epos );

WRITE_HANDLER( epos_videoram_w );
WRITE_HANDLER( epos_port_1_w );
WRITE_HANDLER( epos_palette_w );
void epos_set_flipscreen(int flip);
int epos_get_flipscreen(void);

VIDEO_UPDATE( epos );
