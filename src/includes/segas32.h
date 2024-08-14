/***************************************************************************

    Sega System 32/Multi 32 hardware

***************************************************************************/


/*----------- defined in driver/segas32.c -----------*/
extern data16_t *system32_protram;
extern data16_t *system32_workram;


/*----------- defined in vidhrdw/segas32.c -----------*/

extern UINT16 *system32_videoram;
extern UINT16 *system32_spriteram;
extern UINT16 *system32_paletteram[2];
extern UINT16 system32_displayenable[2];
extern UINT16 system32_tilebank_external;

extern bool opaquey_hack;

extern void system32_set_vblank(int state);

READ16_HANDLER( system32_spriteram_r );
WRITE16_HANDLER( system32_spriteram_w );
READ32_HANDLER( multi32_spriteram_r );
WRITE32_HANDLER( multi32_spriteram_w );

READ16_HANDLER( system32_videoram_r );
WRITE16_HANDLER( system32_videoram_w );
READ32_HANDLER( multi32_videoram_r );
WRITE32_HANDLER( multi32_videoram_w );

READ16_HANDLER( system32_paletteram_r );
WRITE16_HANDLER( system32_paletteram_w );
READ16_HANDLER( multi32_paletteram_0_r );
WRITE16_HANDLER( multi32_paletteram_0_w );
READ16_HANDLER( multi32_paletteram_1_r );
WRITE16_HANDLER( multi32_paletteram_1_w );

READ16_HANDLER( system32_mixer_r );
WRITE16_HANDLER( system32_mixer_w );
READ16_HANDLER( multi32_mixer_0_r );
WRITE16_HANDLER( multi32_mixer_0_w );
READ16_HANDLER( multi32_mixer_1_r );
WRITE16_HANDLER( multi32_mixer_1_w );

READ16_HANDLER( system32_sprite_control_r );
WRITE16_HANDLER( system32_sprite_control_w );
READ32_HANDLER( multi32_sprite_control_r );
WRITE32_HANDLER( multi32_sprite_control_w );

VIDEO_START( system32 );
VIDEO_START( multi32 );

VIDEO_UPDATE( system32 );
VIDEO_UPDATE( multi32 );


/*************************************
 *
 *  Protection - machine/segas32.c
 *
 *************************************/

READ16_HANDLER(ga2_sprite_protection_r);
READ16_HANDLER(ga2_wakeup_protection_r);

WRITE16_HANDLER(sonic_level_load_protection);

WRITE16_HANDLER( jleague_protection_w );

READ16_HANDLER(brival_protection_r);
WRITE16_HANDLER(brival_protboard_w);

READ16_HANDLER(arabfgt_protboard_r);
WRITE16_HANDLER(arabfgt_protboard_w);
READ16_HANDLER(arf_wakeup_protection_r);

void f1lap_fd1149_vblank(void);

READ16_HANDLER( arescue_dsp_r );
WRITE16_HANDLER( arescue_dsp_w );

void darkedge_fd1149_vblank(void);
WRITE16_HANDLER( darkedge_protection_w );
READ16_HANDLER( darkedge_protection_r );

WRITE16_HANDLER( dbzvrvs_protection_w );
READ16_HANDLER( dbzvrvs_protection_r );


/*************************************
 *
 *  ROM definition(s)
 *
 *************************************/

#define ROM_LOAD_x2(name, base, length, crc) \
	ROM_LOAD( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 1 * length, length )

#define ROM_LOAD_x4(name, base, length, crc) \
	ROM_LOAD( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 1 * length, length ) \
	ROM_RELOAD(     base + 2 * length, length ) \
	ROM_RELOAD(     base + 3 * length, length )

#define ROM_LOAD_x8(name, base, length, crc) \
	ROM_LOAD( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 1 * length, length ) \
	ROM_RELOAD(     base + 2 * length, length ) \
	ROM_RELOAD(     base + 3 * length, length ) \
	ROM_RELOAD(     base + 4 * length, length ) \
	ROM_RELOAD(     base + 5 * length, length ) \
	ROM_RELOAD(     base + 6 * length, length ) \
	ROM_RELOAD(     base + 7 * length, length )

#define ROM_LOAD_x16(name, base, length, crc) \
	ROM_LOAD( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 1 * length, length ) \
	ROM_RELOAD(     base + 2 * length, length ) \
	ROM_RELOAD(     base + 3 * length, length ) \
	ROM_RELOAD(     base + 4 * length, length ) \
	ROM_RELOAD(     base + 5 * length, length ) \
	ROM_RELOAD(     base + 6 * length, length ) \
	ROM_RELOAD(     base + 7 * length, length ) \
	ROM_RELOAD(     base + 8 * length, length ) \
	ROM_RELOAD(     base + 9 * length, length ) \
	ROM_RELOAD(     base + 10 * length, length ) \
	ROM_RELOAD(     base + 11 * length, length ) \
	ROM_RELOAD(     base + 12 * length, length ) \
	ROM_RELOAD(     base + 13 * length, length ) \
	ROM_RELOAD(     base + 14 * length, length ) \
	ROM_RELOAD(     base + 15 * length, length )

#define ROM_LOAD16_BYTE_x2(name, base, length, crc) \
	ROM_LOAD16_BYTE( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 2 * length, length )

#define ROM_LOAD16_BYTE_x4(name, base, length, crc) \
	ROM_LOAD16_BYTE( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 2 * length, length ) \
	ROM_RELOAD(     base + 4 * length, length ) \
	ROM_RELOAD(     base + 6 * length, length )

#define ROM_LOAD32_WORD_x2(name, base, length, crc) \
	ROM_LOAD32_WORD( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 2 * length, length )

#define ROM_LOAD32_WORD_x4(name, base, length, crc) \
	ROM_LOAD32_WORD( name, base + 0 * length, length, crc ) \
	ROM_RELOAD(     base + 2 * length, length ) \
	ROM_RELOAD(     base + 4 * length, length ) \
	ROM_RELOAD(     base + 6 * length, length )


/*************************************
 *
 *  Utility Macro
 *
 *************************************/

#define SEXT(value, width) (((int64_t)(value & ((1ULL << width) - 1)) ^ (int64_t)(1ULL << (width - 1))) - (1ULL << (width - 1)))
