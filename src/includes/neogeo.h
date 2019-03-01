/*************************************************************************

	SNK NeoGeo hardware

*************************************************************************/

/*----------- defined in drivers/neogeo.c -----------*/

extern unsigned int neogeo_frame_counter;
extern unsigned int neogeo_frame_counter_speed;
extern int neogeo_has_trackball;

void neogeo_set_cpu1_second_bank(UINT32 bankaddress);
void neogeo_init_cpu2_setbank(void);
void neogeo_register_main_savestate(void);

/*----------- defined in machine/neogeo.c -----------*/

extern data16_t *neogeo_ram16;
extern data16_t *neogeo_sram16;

extern int mcd_action;
extern int mcd_number;
extern int memcard_status;
extern int memcard_number;
extern int memcard_manager;
extern UINT8 *neogeo_memcard;

extern data8_t *neogeo_game_vectors;

MACHINE_INIT( neogeo );
DRIVER_INIT( neogeo );

WRITE16_HANDLER( neogeo_sram16_lock_w );
WRITE16_HANDLER( neogeo_sram16_unlock_w );
READ16_HANDLER( neogeo_sram16_r );
WRITE16_HANDLER( neogeo_sram16_w );

NVRAM_HANDLER( neogeo );

READ16_HANDLER( neogeo_memcard16_r );
WRITE16_HANDLER( neogeo_memcard16_w );
int neogeo_memcard_load(int);
void neogeo_memcard_save(void);
void neogeo_memcard_eject(void);
int neogeo_memcard_create(int);


/*----------- defined in machine/neocrypt.c -----------*/

extern int neogeo_fix_bank_type;

void kof99_neogeo_gfx_decrypt(int extra_xor);
void kof2000_neogeo_gfx_decrypt(int extra_xor);


/*----------- defined in vidhrdw/neogeo.c -----------*/

VIDEO_START( neogeo_mvs );

WRITE16_HANDLER( neogeo_setpalbank0_16_w );
WRITE16_HANDLER( neogeo_setpalbank1_16_w );
READ16_HANDLER( neogeo_paletteram16_r );
WRITE16_HANDLER( neogeo_paletteram16_w );

WRITE16_HANDLER( neogeo_vidram16_offset_w );
READ16_HANDLER( neogeo_vidram16_data_r );
WRITE16_HANDLER( neogeo_vidram16_data_w );
WRITE16_HANDLER( neogeo_vidram16_modulo_w );
READ16_HANDLER( neogeo_vidram16_modulo_r );
WRITE16_HANDLER( neo_board_fix_16_w );
WRITE16_HANDLER( neo_game_fix_16_w );
WRITE16_HANDLER (neogeo_select_bios_vectors);
WRITE16_HANDLER (neogeo_select_game_vectors);

VIDEO_UPDATE( neogeo );
VIDEO_UPDATE( neogeo_raster );
void neogeo_vh_raster_partial_refresh(struct mame_bitmap *bitmap,int current_line);
