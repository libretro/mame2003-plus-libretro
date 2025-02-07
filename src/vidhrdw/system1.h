#ifndef _system1_H_
#define _system1_H_

#include "driver.h"
#include "vidhrdw/generic.h"

#define SPR_Y_TOP		0
#define SPR_Y_BOTTOM	1
#define SPR_X_LO		2
#define SPR_X_HI		3
#define SPR_SKIP_LO		4
#define SPR_SKIP_HI		5
#define SPR_GFXOFS_LO	6
#define SPR_GFXOFS_HI	7

#define system1_BACKGROUND_MEMORY_SINGLE 0
#define system1_BACKGROUND_MEMORY_BANKED 1

extern unsigned char 	*system1_scroll_y;
extern unsigned char 	*system1_scroll_x;
extern unsigned char 	*system1_videoram;
extern unsigned char 	*system1_backgroundram;
extern unsigned char 	*system1_sprites_collisionram;
extern unsigned char 	*system1_background_collisionram;
extern unsigned char 	*system1_scrollx_ram;
extern size_t system1_videoram_size;
extern size_t system1_backgroundram_size;


VIDEO_START( system1 );
void system1_define_background_memory(int Mode);

READ_HANDLER( wbml_videoram_bank_latch_r );
WRITE_HANDLER( wbml_videoram_bank_latch_w );
READ_HANDLER( wbml_paged_videoram_r );
WRITE_HANDLER( wbml_paged_videoram_w );
WRITE_HANDLER( system1_background_collisionram_w );
WRITE_HANDLER( system1_sprites_collisionram_w );
WRITE_HANDLER( system1_videoram_w );
WRITE_HANDLER( system1_paletteram_w );
WRITE_HANDLER( system1_backgroundram_w );
VIDEO_UPDATE( system1 );
PALETTE_INIT( system1 );
WRITE_HANDLER( system1_videomode_w );
READ_HANDLER( system1_videomode_r );

WRITE_HANDLER( choplifter_scroll_x_w );
VIDEO_UPDATE( choplifter );
VIDEO_UPDATE( shtngmst );
VIDEO_UPDATE( wbml );
VIDEO_UPDATE( ufosensi );
VIDEO_UPDATE( blockgal );

#endif
