/*************************************************************************

	Atari Escape hardware

*************************************************************************/

/*----------- defined in vidhrdw/eprom.c -----------*/

VIDEO_START( eprom );
VIDEO_UPDATE( eprom );
VIDEO_START( guts );
VIDEO_UPDATE( guts );
void eprom_scanline_update(int scanline);
