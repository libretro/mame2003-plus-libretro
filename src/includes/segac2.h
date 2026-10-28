/*************************************************************************

	Sega System C/C2 Driver

**************************************************************************/

#include "vidhrdw/segavdp.h"

/*----------- defined in drivers/segac2.c -----------*/

void segac2_vdp_irq_update(void);

/*----------- defined in vidhrdw/segac2_vidhrdw.c -----------*/

VIDEO_START( puckpkmn );
VIDEO_START( megatech );
VIDEO_START( megaplay );

VIDEO_START( segac2 );

VIDEO_EOF( segac2 );

VIDEO_UPDATE( segac2 );
VIDEO_UPDATE( megatech );
VIDEO_UPDATE( megaplay );
