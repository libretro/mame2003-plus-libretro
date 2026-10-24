/***************************************************************************

    Sega Model 1

***************************************************************************/

#ifndef MODEL1_H
#define MODEL1_H

#include "cpu/mb86233/mb86233.h"

/* machine: TGP */
#define MODEL1_TGP_HLE			0
#define MODEL1_TGP_LLE			1
#define MODEL1_TGP_ORACLE		2

#define MODEL1_TGP_TABLE_VR		0	/* 315-5573: Virtua Racing, Virtua Formula */
#define MODEL1_TGP_TABLE_SWA	1	/* 315-5711: Star Wars Arcade, Wing War, NetMerc */
#define MODEL1_TGP_TABLE_VF		2	/* 315-5724: Virtua Fighter */

extern int model1_tgp_mode;
extern int model1_tgp_table;
extern int model1_tgp_cpu;
extern struct mb86233_config model1_tgp_config;

void model1_tgp_init( void );
void model1_tgp_reset( void );
void model1_tgp_oracle_frame( void );
void model1_tgp_oracle_report( void );

READ16_HANDLER( model1_tgp_copro_adr_r );
WRITE16_HANDLER( model1_tgp_copro_adr_w );
READ16_HANDLER( model1_tgp_copro_ram_r );
WRITE16_HANDLER( model1_tgp_copro_ram_w );
READ16_HANDLER( model1_tgp_copro_r );
WRITE16_HANDLER( model1_tgp_copro_w );
READ16_HANDLER( model1_fifoin_status_r );

/* V60 */
void v60_stall( void );

/* video */
extern UINT16 *model1_display_list0, *model1_display_list1;
extern UINT16 *model1_color_xlat;
extern UINT16 *model1_mr, *model1_mr2;
extern int model1_sound_irq;

READ16_HANDLER( model1_listctl_r );
WRITE16_HANDLER( model1_listctl_w );
WRITE16_HANDLER( model1_paletteram_w );
VIDEO_START( model1 );
VIDEO_UPDATE( model1 );
VIDEO_EOF( model1 );
void model1_video_vblank_start( void );

#endif
