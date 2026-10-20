/***************************************************************************

    Sega Model 2 (see drivers/model2.c)

***************************************************************************/

#ifndef MODEL2_H
#define MODEL2_H

/* drivers/model2.c */
extern data32_t *model2_bufferram;
extern data32_t *model2_textureram0;
extern data32_t *model2_textureram1;
extern data16_t *model2_palram;
extern data16_t *model2_colorxlat;
extern UINT8 *model2_lumaram;
extern UINT8 model2_gamma_table[ 256 ];
extern int model2_render_mode, model2_render_test_mode, model2_render_unk;
extern UINT32 model2_framenum;
void model2_raise_irq( UINT32 line );
UINT32 model2_irq_enabled( UINT32 line );
UINT32 model2_videocontrol( void );
void model2_palette_refresh( void );

/* machine/model2_machine.c: TGP coprocessor */
extern struct mb86233_config model2_tgp_config;
void model2_copro_init( void );
void model2_copro_reset( void );
READ32_HANDLER( model2_copro_ctl1_r );
WRITE32_HANDLER( model2_copro_ctl1_w );
READ32_HANDLER( model2_copro_fifo_r );
WRITE32_HANDLER( model2_copro_fifo_w );
WRITE32_HANDLER( model2_copro_function_port_w );
READ32_HANDLER( model2_fifo_control_r );

/* machine/model2_machine.c: sound board link (i8251 UART to the SCSP MIDI port) */
#define MODEL2_COPRO_TGP	0
#define MODEL2_COPRO_SHARC	1
#define MODEL2_COPRO_TGPX4	2
extern int model2_copro_type;
extern struct sharc_config model2_sharc_config;
extern struct mb86235_config model2_tgpx4_config;
extern int model2_geo_compact_objects;	/* vidhrdw: Top Skater geometrizer program */
READ32_HANDLER( model2_copro_status_r );
WRITE32_HANDLER( model2_copro_iop_w );
#define MODEL2_SND_SCSP	0
#define MODEL2_SND_M1	1
extern int model2_sound_board;
extern int model2_m1_extra;
void model2_sound_init( void );
void model2_sound_reset( void );
void model2_sound_irq_check( void );
void model2_sound_postload( void );
READ32_HANDLER( model2_uart_r );
WRITE32_HANDLER( model2_uart_w );
READ16_HANDLER( model2_m1snd_uart_r );
WRITE16_HANDLER( model2_m1snd_uart_w );

/* vidhrdw/model2_vidhrdw.c: geometrizer and rasterizer */
void model2_geo_reset( void );
void model2_geo_parse( void );
READ32_HANDLER( model2_geo_r );
WRITE32_HANDLER( model2_geo_w );
READ32_HANDLER( model2_geo_prg_r );
WRITE32_HANDLER( model2_geo_prg_w );
WRITE32_HANDLER( model2_geo_ctl1_w );
WRITE32_HANDLER( model2_3d_zclip_w );
READ32_HANDLER( model2_polygon_count_r );
void model2_horizontal_sync_w( UINT16 data );
void model2_vertical_sync_w( UINT16 data );
VIDEO_START( model2 );
VIDEO_UPDATE( model2 );


#endif
