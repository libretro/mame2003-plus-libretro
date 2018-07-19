/*	System18 Hardware
**
**	MC68000 + Z80
**	2xYM3438 + Custom PCM
**
**	Ace Attacker
**	Alien Storm
**	Bloxeed
**	Clutch Hitter
**	D.D. Crew
**	Laser Ghost
**	Michael Jackson's Moonwalker
**	Shadow Dancer
**	Search Wally
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "cpu/i8039/i8039.h"
#include "system16.h"

/***************************************************************************/

bool		moonwalker_playing = false;
bool		moon_diddy = false;
int			mj_current_music = 0;

const char *const moonwalker_samples_set_names[] =
{
	"*moonwalker",
	"bad-01",
	"bad-02",
	"smoothcriminal-01",
	"smoothcriminal-02",	
	"beatit-01",
	"beatit-02",
	"thriller-01",
	"thriller-02",	
	"billiejean-01",
	"billiejean-02",
	"title-01",
	"title-02",
	0
};

static struct Samplesinterface moonwalker_samples_set =
{
	2,	// 2 channels
	100, // volume
	moonwalker_samples_set_names
};

static data16_t sys16_coinctrl;

static WRITE16_HANDLER( sys18_refreshenable_w ){
	if( ACCESSING_LSB ){
		sys16_coinctrl = data&0xff;
		sys16_refreshenable = sys16_coinctrl & 0x02;
		/* bit 2 is also used (0 in shadow dancer) */
		/* shadow dancer also sets bit 7 */
	}
}

/***************************************************************************/

static void set_fg_page( int data ){
	sys16_fg_page[0] = data>>12;
	sys16_fg_page[1] = (data>>8)&0xf;
	sys16_fg_page[2] = (data>>4)&0xf;
	sys16_fg_page[3] = data&0xf;
}

static void set_bg_page( int data ){
	sys16_bg_page[0] = data>>12;
	sys16_bg_page[1] = (data>>8)&0xf;
	sys16_bg_page[2] = (data>>4)&0xf;
	sys16_bg_page[3] = data&0xf;
}

static void set_fg2_page( int data ){
	sys16_fg2_page[0] = data>>12;
	sys16_fg2_page[1] = (data>>8)&0xf;
	sys16_fg2_page[2] = (data>>4)&0xf;
	sys16_fg2_page[3] = data&0xf;
}

static void set_bg2_page( int data ){
	sys16_bg2_page[0] = data>>12;
	sys16_bg2_page[1] = (data>>8)&0xf;
	sys16_bg2_page[2] = (data>>4)&0xf;
	sys16_bg2_page[3] = data&0xf;
}

/***************************************************************************/

static UINT8 *sys18_SoundMemBank;

static READ_HANDLER( system18_bank_r ){
	return sys18_SoundMemBank[offset];
}

static MEMORY_READ_START( sound_readmem_18 )
	{ 0x0000, 0x9fff, MRA_ROM },
	{ 0xa000, 0xbfff, system18_bank_r },
	/**** D/A register ****/
	{ 0xd000, 0xdfff, RF5C68_r },
	{ 0xe000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem_18 )
	{ 0x0000, 0xbfff, MWA_ROM },
	/**** D/A register ****/
	{ 0xc000, 0xc008, RF5C68_reg_w },
	{ 0xd000, 0xdfff, RF5C68_w },
	{ 0xe000, 0xffff, MWA_RAM },	//??
MEMORY_END

static WRITE_HANDLER( sys18_soundbank_w ){
	/* select access bank for a000~bfff */
	unsigned char *RAM = memory_region(REGION_CPU2);
	int Bank=0;
	switch( data&0xc0 ){
		case 0x00:
			Bank = data<<13;
			break;
		case 0x40:
			Bank = ((data&0x1f) + 128/8)<<13;
			break;
		case 0x80:
			Bank = ((data&0x1f) + (256+128)/8)<<13;
			break;
		case 0xc0:
			Bank = ((data&0x1f) + (512+128)/8)<<13;
			break;
	}
	sys18_SoundMemBank = &RAM[Bank+0x10000];
}

static PORT_READ_START( sound_readport_18 )
	{ 0x80, 0x80, YM2612_status_port_0_A_r },
//	{ 0x82, 0x82, YM2612_status_port_0_B_r },
//	{ 0x90, 0x90, YM2612_status_port_1_A_r },
//	{ 0x92, 0x92, YM2612_status_port_1_B_r },
	{ 0xc0, 0xc0, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_writeport_18 )
	{ 0x80, 0x80, YM2612_control_port_0_A_w },
	{ 0x81, 0x81, YM2612_data_port_0_A_w },
	{ 0x82, 0x82, YM2612_control_port_0_B_w },
	{ 0x83, 0x83, YM2612_data_port_0_B_w },
	{ 0x90, 0x90, YM2612_control_port_1_A_w },
	{ 0x91, 0x91, YM2612_data_port_1_A_w },
	{ 0x92, 0x92, YM2612_control_port_1_B_w },
	{ 0x93, 0x93, YM2612_data_port_1_B_w },
	{ 0xa0, 0xa0, sys18_soundbank_w },
PORT_END

static WRITE16_HANDLER( sound_command_nmi_w ){

	if( ACCESSING_LSB ){
		if(moonwalker_playing == true) {
			int a = 0;
			int o_max_samples = 12;
			int sa_left = 0;
			int sa_right = 1;
			int mj_fade = 30;
			bool sa_loop = 1; // --> 1 == loop, 0 == do not loop.
			bool sa_play_sample = false;
			bool sa_play_original = false;
			bool moonwalker_do_nothing = false;
			bool moonwalker_stop_samples = false;
			bool moonwalker_play_default = false;

			switch (data) {
				// Reset music. Title screen.
				case 0x0:
					moonwalker_stop_samples = true;
					mj_current_music = 0;
					moon_diddy = false;
					
					break;

				// Title screen stuff.
				case 0x85:
					if(mj_current_music != 85) {
						mj_current_music = 85;
						sa_loop = 0;
						sa_play_sample = true;
						sa_left = 10;
						sa_right = 11;
					}
					else
						moonwalker_do_nothing = true;

					break;

				// Title screen magic.
				case 0x86:
					if(mj_current_music == 85)
						moonwalker_do_nothing = true;
					else
						sa_play_original = true;
						
					break;

				// Title screen magic.
				case 0x87:
					if(mj_current_music == 85)
						moonwalker_do_nothing = true;
					else
						sa_play_original = true;
						
					break;
										
				// Stage 1 and Stage 5. Bad.
				case 0x81:
					if(mj_current_music != 81) {
						mj_current_music = 81;
						sa_play_sample = true;
						sa_left = 0;
						sa_right = 1;
					}
					else
						moonwalker_do_nothing = true;

					break;

				// Stage 2. Smooth Criminal.
				case 0x82:
					if(mj_current_music != 82) {
						mj_current_music = 82;
						sa_play_sample = true;
						sa_left = 2;
						sa_right = 3;
					}
					else
						moonwalker_do_nothing = true;						
					break;

				// Stage 3. Beat It.
				case 0x84:
					if(mj_current_music != 83) {
						mj_current_music = 83;
						sa_play_sample = true;
						sa_left = 4;
						sa_right = 5;
					}
					else
						moonwalker_do_nothing = true;						
					break;

				// Stage 4. Thriller.
				case 0x8A:
					if(mj_current_music != 8) {
						mj_current_music = 8;
						sa_play_sample = true;
						sa_left = 6;
						sa_right = 7;
					}
					else
						moonwalker_do_nothing = true;						
					break;

				// Ending. Billie Jean.
				case 0x89:
					if(mj_current_music != 89) {
						mj_current_music = 89;
						sa_play_sample = true;
						sa_left = 8;
						sa_right = 9;
					}
					else
						moonwalker_do_nothing = true;						
					break;

				// First boss music
				case 0x8B:
						moonwalker_do_nothing = true;
					break;

				// Second boss music
				case 0x83:
						moonwalker_do_nothing = true;
					break;

				// Third boss music
				case 0x8E:
						moonwalker_do_nothing = true;
					break;										
							
				// Special move music diddy.
				case 0xFA:
						moonwalker_play_default = true;
						moon_diddy = true;

						// While the special move is playing, lets adjust the level music volume lower temporary to 30%.
						if(sample_playing(0) == 0 && sample_playing(1) == 1) {
							sample_set_stereo_volume(1, mj_fade, mj_fade);
						}
						else if(sample_playing(0) == 1 && sample_playing(1) == 0) {
							sample_set_stereo_volume(0, mj_fade, mj_fade);
						}
						else if(sample_playing(0) == 1 && sample_playing(1) == 1) {
							sample_set_stereo_volume(0, mj_fade, 0);
							sample_set_stereo_volume(1, 0, mj_fade);
						}
					break;

				// Special move music diddy.
				case 0xFB:
						moonwalker_play_default = true;
						moon_diddy = true;

						// While the special move is playing, lets adjust the level music volume lower temporary to 30%.
						if(sample_playing(0) == 0 && sample_playing(1) == 1) {
							sample_set_stereo_volume(1, mj_fade, mj_fade);
						}
						else if(sample_playing(0) == 1 && sample_playing(1) == 0) {
							sample_set_stereo_volume(0, mj_fade, mj_fade);
						}
						else if(sample_playing(0) == 1 && sample_playing(1) == 1) {
							sample_set_stereo_volume(0, mj_fade, 0);
							sample_set_stereo_volume(1, 0, mj_fade);
						}						
					break;

				// Special move music diddy.
				case 0xF6:
						moonwalker_play_default = true;
						moon_diddy = true;

						// While the special move is playing, lets adjust the level music volume lower temporary to 30%.
						if(sample_playing(0) == 0 && sample_playing(1) == 1) {
							sample_set_stereo_volume(1, mj_fade, mj_fade);
						}
						else if(sample_playing(0) == 1 && sample_playing(1) == 0) {
							sample_set_stereo_volume(0, mj_fade, mj_fade);
						}
						else if(sample_playing(0) == 1 && sample_playing(1) == 1) {
							sample_set_stereo_volume(0, mj_fade, 0);
							sample_set_stereo_volume(1, 0, mj_fade);
						}						
					break;									

				// Special move "owww" sound effect. This plays after the special move has always finished.
				case 0xC3:
						moonwalker_play_default = true;
						
						if(moon_diddy == true) {
							moon_diddy = false;

							// The special move is finished, lets return the level music volume back to 100%.
							if(sample_playing(0) == 0 && sample_playing(1) == 1) {
								sample_set_stereo_volume(1, 100, 100);
							}
							else if(sample_playing(0) == 1 && sample_playing(1) == 0) {
								sample_set_stereo_volume(0, 100, 100);
							}
							else if(sample_playing(0) == 1 && sample_playing(1) == 1) {
								sample_set_stereo_volume(0, 100, 0);
								sample_set_stereo_volume(1, 0, 100);
							}							
						}						
					break;
				
				default:
					soundlatch_w( 0,data&0xff );
					cpu_set_nmi_line(1, PULSE_LINE);

					break;
			}

			if(sa_play_sample == true) {
				a = 0;
					
				for(a = 0; a <= o_max_samples; a++) {
					sample_stop(a);
				}
				
				sample_start(0, sa_left, sa_loop);
				sample_start(1, sa_right, sa_loop);
				
				// Determine how we should mix these samples together.
				if(sample_playing(0) == 0 && sample_playing(1) == 1) { // Right channel only. Lets make it play in both speakers.
					sample_set_stereo_volume(1, 100, 100);
				}
				else if(sample_playing(0) == 1 && sample_playing(1) == 0) { // Left channel only. Lets make it play in both speakers.
					sample_set_stereo_volume(0, 100, 100);
				}
				else if(sample_playing(0) == 1 && sample_playing(1) == 1) { // Both left and right channels. Lets make them play in there respective speakers.
					sample_set_stereo_volume(0, 100, 0);
					sample_set_stereo_volume(1, 0, 100);
				}
				else if(sample_playing(0) == 0 && sample_playing(1) == 0 && moonwalker_do_nothing == false) { // No sample playing, revert to the default sound.
					sa_play_original = false;
					mj_current_music = 0;
					soundlatch_w( 0,data&0xff );
					cpu_set_nmi_line(1, PULSE_LINE);
				}

				if(sa_play_original == true) {
					mj_current_music = 0;
					soundlatch_w( 0,data&0xff );
					cpu_set_nmi_line(1, PULSE_LINE);
				}
			}
			else if(moonwalker_do_nothing == true) {
				// --> Do nothing.
			}
			else if(moonwalker_stop_samples == true) {
				mj_current_music = 0;
				a = 0;

				for(a = 0; a <= o_max_samples; a++) {
					sample_stop(a);
				}

				// Now play the default sound.
				soundlatch_w( 0,data&0xff );
				cpu_set_nmi_line(1, PULSE_LINE);
			}
			else if(moonwalker_play_default == true) {
				mj_current_music = 0;
				soundlatch_w( 0,data&0xff );
				cpu_set_nmi_line(1, PULSE_LINE);
			}			
		}
		else {
			soundlatch_w( 0,data&0xff );
			cpu_set_nmi_line(1, PULSE_LINE);
		}
	}
}

/***************************************************************************/

#if 0
static READ16_HANDLER( shdancer_skip_r ){
	if (activecpu_get_pc()==0x2f76) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0];
}
#endif

static MEMORY_READ16_START( shdancer_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MRA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0xc00000, 0xc00007, SYS16_MRA16_EXTRAM },
	{ 0xe4000a, 0xe4000b, input_port_3_word_r }, // dip1
	{ 0xe4000c, 0xe4000d, input_port_4_word_r }, // dip2
	{ 0xe40000, 0xe40001, input_port_0_word_r }, // player1
	{ 0xe40002, 0xe40003, input_port_1_word_r }, // player2
	{ 0xe40008, 0xe40009, input_port_2_word_r }, // service
	{ 0xe43034, 0xe43035, MRA16_NOP },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( shdancer_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MWA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MWA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MWA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MWA16_PALETTERAM },
	{ 0xc00000, 0xc00007, SYS16_MWA16_EXTRAM },
	{ 0xe4001c, 0xe4001d, sys18_refreshenable_w },
	{ 0xe43034, 0xe43035, MWA16_NOP },
	{ 0xfe0006, 0xfe0007, sound_command_nmi_w },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM },
MEMORY_END

/***************************************************************************/

static void shdancer_update_proc( void ){
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];/*?*/

	set_fg_page( sys16_textram[0x0e80/2] );
	set_bg_page( sys16_textram[0x0e82/2] );

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	set_fg2_page( sys16_textram[0x0e84/2] );
	set_bg2_page( sys16_textram[0x0e86/2] );

	sys18_bg2_active=0;
	sys18_fg2_active=0;

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2]) sys18_fg2_active=1;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2]) sys18_bg2_active=1;

	{
		data16_t data = sys16_extraram[0/2];
		sys16_tile_bank0 = data&0xf;
		sys16_tile_bank1 = (data>>4)&0xf;
	}
}

static MACHINE_INIT( shdancer ){
	sys16_spritelist_end=0x8000;
	sys16_update_proc = shdancer_update_proc;
}

static DRIVER_INIT( shdancer ){
	unsigned char *RAM = memory_region(REGION_CPU2);
	machine_init_sys16_onetime();
	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];
//	install_mem_read_handler(0, 0xffc000, 0xffc001, shdancer_skip_r );
	sys16_MaxShadowColors=0; // doesn't seem to use transparent shadows

	memcpy(RAM,&RAM[0x10000],0xa000);
}

/***************************************************************************/

/*
static READ_HANDLER( shdancer_skip_r ){
	if (activecpu_get_pc()==0x2f76) {cpu_spinuntil_int(); return 0xffff;}
	return (*(UINT16 *)(&sys16_workingram[0x0000]));
}
*/

static MEMORY_READ16_START( shdancbl_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MRA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0xc00000, 0xc00007, SYS16_MRA16_EXTRAM },
	{ 0xc40000, 0xc40001, input_port_3_word_r }, // dip1
	{ 0xc40002, 0xc40003, input_port_4_word_r }, // dip2
	{ 0xc41002, 0xc41003, input_port_0_word_r }, // player1
	{ 0xc41004, 0xc41005, input_port_1_word_r }, // player2
	{ 0xc41000, 0xc41001, input_port_2_word_r }, // service
//	{ 0xc40000, 0xc4ffff, MRA16_EXTRAM3 },
	{ 0xe43034, 0xe43035, MRA16_NOP },
//	{ 0xffc000, 0xffc001, shdancer_skip_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( shdancbl_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MWA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MWA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MWA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MWA16_PALETTERAM },
	{ 0xc00000, 0xc00007, SYS16_MWA16_EXTRAM },
//	{ 0xc40000, 0xc4ffff, SYS16_MWA16_EXTRAM3 },
	{ 0xe4001c, 0xe4001d, sys18_refreshenable_w },
	{ 0xe43034, 0xe43035, MWA16_NOP },
	{ 0xfe0006, 0xfe0007, sound_command_nmi_w },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM },
MEMORY_END

/***************************************************************************/

static void shdancbl_update_proc( void ){
	// this is all wrong and needs re-doing
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];

	set_fg_page( sys16_textram[0x0e80/2] );
	set_bg_page( sys16_textram[0x0e82/2] );

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	set_fg2_page( sys16_textram[0x0e84/2] );
	set_bg2_page( sys16_textram[0x0e82/2] );

	sys18_bg2_active=0;
	sys18_fg2_active=0;

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2])
		sys18_fg2_active=1;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2])
		sys18_bg2_active=1;

	{
		data16_t data = sys16_extraram[0/2];
		sys16_tile_bank0 = data&0xf;
		sys16_tile_bank1 = (data>>4)&0xf;
	}
}


static MACHINE_INIT( shdancbl ){
	sys16_spritelist_end=0x8000;
	sys16_sprxoffset = -0xbc+0x77;

	sys16_update_proc = shdancbl_update_proc;
}

static DRIVER_INIT( shdancbl ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	int i;

	machine_init_sys16_onetime();
	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];
//	install_mem_read_handler(0, 0xffc000, 0xffc001, shdancer_skip_r );
	sys16_MaxShadowColors=0;		// doesn't seem to use transparent shadows

	memcpy(RAM,&RAM[0x10000],0xa000);

	/* invert the graphics bits on the tiles */
	for (i = 0; i < 0xc0000; i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;
}

/***************************************************************************/
#if 0
static READ16_HANDLER( shdancrj_skip_r ){
	if (activecpu_get_pc()==0x2f70) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0xc000/2];
}
#endif

static MACHINE_INIT( shdancrj ){
	sys16_spritelist_end=0x8000;
	sys16_patch_code(0x6821, 0xdf);
	sys16_update_proc = shdancer_update_proc;
}

static DRIVER_INIT( shdancrj ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	machine_init_sys16_onetime();
	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];
//	install_mem_read_handler(0, 0xffc000, 0xffc001, shdancrj_skip_r );

	memcpy(RAM,&RAM[0x10000],0xa000);
}

/***************************************************************************/

static READ16_HANDLER( moonwlkb_skip_r ){
	if (activecpu_get_pc()==0x308a) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0x202c/2];
}

static MEMORY_READ16_START( moonwalk_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MRA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
	{ 0xc00000, 0xc0ffff, SYS16_MRA16_EXTRAM },
	{ 0xc40000, 0xc40001, input_port_3_word_r }, // dip1
	{ 0xc40002, 0xc40003, input_port_4_word_r }, // dip2
	{ 0xc41002, 0xc41003, input_port_0_word_r }, // player1
	{ 0xc41004, 0xc41005, input_port_1_word_r }, // player2
	{ 0xc41006, 0xc41007, input_port_5_word_r }, // player3
	{ 0xc41000, 0xc41001, input_port_2_word_r }, // service
	{ 0xe40000, 0xe4ffff, SYS16_MRA16_EXTRAM2 },
	{ 0xfe0000, 0xfeffff, SYS16_MRA16_EXTRAM4 },
	{ 0xffe02c, 0xffe02d, moonwlkb_skip_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( moonwalk_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x400000, 0x40ffff, SYS16_MWA16_TILERAM },
	{ 0x410000, 0x410fff, SYS16_MWA16_TEXTRAM },
	{ 0x440000, 0x440fff, SYS16_MWA16_SPRITERAM },
	{ 0x840000, 0x840fff, SYS16_MWA16_PALETTERAM },
	{ 0xc00000, 0xc0ffff, SYS16_MWA16_EXTRAM },
	{ 0xc40006, 0xc40007, sound_command_nmi_w },
	{ 0xc46600, 0xc46601, sys18_refreshenable_w },
	{ 0xc46800, 0xc46801, SYS16_MWA16_EXTRAM3 },
	{ 0xe40000, 0xe4ffff, SYS16_MWA16_EXTRAM2 },
	{ 0xfe0000, 0xfeffff, SYS16_MWA16_EXTRAM4 },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM },
MEMORY_END

/***************************************************************************/

static void moonwalk_update_proc( void ){
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];

	set_fg_page( sys16_textram[0x0e80/2] );
	set_bg_page( sys16_textram[0x0e82/2] );

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	set_fg2_page( sys16_textram[0x0e84/2] );
	set_bg2_page( sys16_textram[0x0e86/2] );

	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2])
		sys18_fg2_active=1;
	else
		sys18_fg2_active=0;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2])
		sys18_bg2_active=1;
	else
		sys18_bg2_active=0;

	{
		data16_t data = sys16_extraram3[0/2];
		sys16_tile_bank0 = data&0xf;
		sys16_tile_bank1 = (data>>4)&0xf;
	}
}

static MACHINE_INIT( moonwalk ){
	sys16_bg_priority_value=0x1000;
	sys16_sprxoffset = -0x238;
	sys16_spritelist_end=0x8000;

	sys16_patch_code( 0x70116, 0x4e);
	sys16_patch_code( 0x70117, 0x71);

	sys16_patch_code( 0x314a, 0x46);
	sys16_patch_code( 0x314b, 0x42);

	sys16_patch_code( 0x311b, 0x3f);

	sys16_patch_code( 0x70103, 0x00);
	sys16_patch_code( 0x70109, 0x00);
	sys16_patch_code( 0x07727, 0x00);
	sys16_patch_code( 0x07729, 0x00);
	sys16_patch_code( 0x0780d, 0x00);
	sys16_patch_code( 0x0780f, 0x00);
	sys16_patch_code( 0x07861, 0x00);
	sys16_patch_code( 0x07863, 0x00);
	sys16_patch_code( 0x07d47, 0x00);
	sys16_patch_code( 0x07863, 0x00);
	sys16_patch_code( 0x08533, 0x00);
	sys16_patch_code( 0x08535, 0x00);
	sys16_patch_code( 0x085bd, 0x00);
	sys16_patch_code( 0x085bf, 0x00);
	sys16_patch_code( 0x09a4b, 0x00);
	sys16_patch_code( 0x09a4d, 0x00);
	sys16_patch_code( 0x09b2f, 0x00);
	sys16_patch_code( 0x09b31, 0x00);
	sys16_patch_code( 0x0a05b, 0x00);
	sys16_patch_code( 0x0a05d, 0x00);
	sys16_patch_code( 0x0a23f, 0x00);
	sys16_patch_code( 0x0a241, 0x00);
	sys16_patch_code( 0x10159, 0x00);
	sys16_patch_code( 0x1015b, 0x00);
	sys16_patch_code( 0x109fb, 0x00);
	sys16_patch_code( 0x109fd, 0x00);

	// * SEGA mark
	sys16_patch_code( 0x70212, 0x4e);
	sys16_patch_code( 0x70213, 0x71);

	sys16_update_proc = moonwalk_update_proc;
}

static DRIVER_INIT( moonwalk ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	machine_init_sys16_onetime();
	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];

	memcpy(RAM,&RAM[0x10000],0xa000);
}

/***************************************************************************/

static READ16_HANDLER( astorm_skip_r ){
	if (activecpu_get_pc()==0x3d4c) {cpu_spinuntil_int(); return 0xffff;}
	return sys16_workingram[0x2c2c/2];
}

static MEMORY_READ16_START( astorm_readmem )
	{ 0x000000, 0x07ffff, MRA16_ROM },
	{ 0x100000, 0x10ffff, SYS16_MRA16_TILERAM },
	{ 0x110000, 0x110fff, SYS16_MRA16_TEXTRAM },
	{ 0x140000, 0x140fff, SYS16_MRA16_PALETTERAM },
	{ 0x200000, 0x200fff, SYS16_MRA16_SPRITERAM },
	{ 0xa00000, 0xa00001, input_port_3_word_r }, // dip1
	{ 0xa00002, 0xa00003, input_port_4_word_r }, // dip2
	{ 0xa01002, 0xa01003, input_port_0_word_r }, // player1
	{ 0xa01004, 0xa01005, input_port_1_word_r }, // player2
	{ 0xa01006, 0xa01007, input_port_5_word_r }, // player3
	{ 0xa01000, 0xa01001, input_port_2_word_r }, // service
	{ 0xa00000, 0xa0ffff, SYS16_MRA16_EXTRAM2 },
	{ 0xc00000, 0xc0ffff, SYS16_MRA16_EXTRAM },
	{ 0xffec2c, 0xffec2d, astorm_skip_r },
	{ 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( astorm_writemem )
	{ 0x000000, 0x07ffff, MWA16_ROM },
	{ 0x100000, 0x10ffff, SYS16_MWA16_TILERAM },
	{ 0x110000, 0x110fff, SYS16_MWA16_TEXTRAM },
	{ 0x140000, 0x140fff, SYS16_MWA16_PALETTERAM },
	{ 0x200000, 0x200fff, SYS16_MWA16_SPRITERAM },
	{ 0xa00006, 0xa00007, sound_command_nmi_w },
	{ 0xa00000, 0xa0ffff, SYS16_MWA16_EXTRAM2 },
	{ 0xc00000, 0xc0ffff, SYS16_MWA16_EXTRAM },
	{ 0xc46600, 0xc46601, sys18_refreshenable_w },
	{ 0xfe0020, 0xfe003f, MWA16_NOP },
	{ 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM },
MEMORY_END

/***************************************************************************/

static void astorm_update_proc( void ){
	data16_t data;
	sys16_fg_scrollx = sys16_textram[0x0e98/2];
	sys16_bg_scrollx = sys16_textram[0x0e9a/2];
	sys16_fg_scrolly = sys16_textram[0x0e90/2];
	sys16_bg_scrolly = sys16_textram[0x0e92/2];

	data = sys16_textram[0x0e80/2];
	sys16_fg_page[1] = data>>12;
	sys16_fg_page[3] = (data>>8)&0xf;
	sys16_fg_page[0] = (data>>4)&0xf;
	sys16_fg_page[2] = data&0xf;

	data = sys16_textram[0x0e82/2];
	sys16_bg_page[1] = data>>12;
	sys16_bg_page[3] = (data>>8)&0xf;
	sys16_bg_page[0] = (data>>4)&0xf;
	sys16_bg_page[2] = data&0xf;

	sys16_fg2_scrollx = sys16_textram[0x0e9c/2];
	sys16_bg2_scrollx = sys16_textram[0x0e9e/2];
	sys16_fg2_scrolly = sys16_textram[0x0e94/2];
	sys16_bg2_scrolly = sys16_textram[0x0e96/2];

	data = sys16_textram[0x0e84/2];
	sys16_fg2_page[1] = data>>12;
	sys16_fg2_page[3] = (data>>8)&0xf;
	sys16_fg2_page[0] = (data>>4)&0xf;
	sys16_fg2_page[2] = data&0xf;

	data = sys16_textram[0x0e86/2];
	sys16_bg2_page[1] = data>>12;
	sys16_bg2_page[3] = (data>>8)&0xf;
	sys16_bg2_page[0] = (data>>4)&0xf;
	sys16_bg2_page[2] = data&0xf;

// enable regs
	if(sys16_fg2_scrollx | sys16_fg2_scrolly | sys16_textram[0x0e84/2])
		sys18_fg2_active=1;
	else
		sys18_fg2_active=0;
	if(sys16_bg2_scrollx | sys16_bg2_scrolly | sys16_textram[0x0e86/2])
		sys18_bg2_active=1;
	else
		sys18_bg2_active=0;

	{
		data = sys16_extraram2[0xe/2]; // 0xa0000f
		sys16_tile_bank0 = data&0xf;
		sys16_tile_bank1 = (data>>4)&0xf;
	}
}

static MACHINE_INIT( astorm ){
	sys16_fgxoffset = sys16_bgxoffset = -9;

	sys16_patch_code( 0x2D6E, 0x32 );
	sys16_patch_code( 0x2D6F, 0x3c );
	sys16_patch_code( 0x2D70, 0x80 );
	sys16_patch_code( 0x2D71, 0x00 );
	sys16_patch_code( 0x2D72, 0x33 );
	sys16_patch_code( 0x2D73, 0xc1 );
	sys16_patch_code( 0x2ea2, 0x30 );
	sys16_patch_code( 0x2ea3, 0x38 );
	sys16_patch_code( 0x2ea4, 0xec );
	sys16_patch_code( 0x2ea5, 0xf6 );
	sys16_patch_code( 0x2ea6, 0x30 );
	sys16_patch_code( 0x2ea7, 0x80 );
	sys16_patch_code( 0x2e5c, 0x30 );
	sys16_patch_code( 0x2e5d, 0x38 );
	sys16_patch_code( 0x2e5e, 0xec );
	sys16_patch_code( 0x2e5f, 0xe2 );
	sys16_patch_code( 0x2e60, 0xc0 );
	sys16_patch_code( 0x2e61, 0x7c );

	sys16_patch_code( 0x4cd8, 0x02 );
	sys16_patch_code( 0x4cec, 0x03 );
	sys16_patch_code( 0x2dc6c, 0xe9 );
	sys16_patch_code( 0x2dc64, 0x10 );
	sys16_patch_code( 0x2dc65, 0x10 );
	sys16_patch_code( 0x3a100, 0x10 );
	sys16_patch_code( 0x3a101, 0x13 );
	sys16_patch_code( 0x3a102, 0x90 );
	sys16_patch_code( 0x3a103, 0x2b );
	sys16_patch_code( 0x3a104, 0x00 );
	sys16_patch_code( 0x3a105, 0x01 );
	sys16_patch_code( 0x3a106, 0x0c );
	sys16_patch_code( 0x3a107, 0x00 );
	sys16_patch_code( 0x3a108, 0x00 );
	sys16_patch_code( 0x3a109, 0x01 );
	sys16_patch_code( 0x3a10a, 0x66 );
	sys16_patch_code( 0x3a10b, 0x06 );
	sys16_patch_code( 0x3a10c, 0x42 );
	sys16_patch_code( 0x3a10d, 0x40 );
	sys16_patch_code( 0x3a10e, 0x54 );
	sys16_patch_code( 0x3a10f, 0x8b );
	sys16_patch_code( 0x3a110, 0x60 );
	sys16_patch_code( 0x3a111, 0x02 );
	sys16_patch_code( 0x3a112, 0x30 );
	sys16_patch_code( 0x3a113, 0x1b );
	sys16_patch_code( 0x3a114, 0x34 );
	sys16_patch_code( 0x3a115, 0xc0 );
	sys16_patch_code( 0x3a116, 0x34 );
	sys16_patch_code( 0x3a117, 0xdb );
	sys16_patch_code( 0x3a118, 0x24 );
	sys16_patch_code( 0x3a119, 0xdb );
	sys16_patch_code( 0x3a11a, 0x24 );
	sys16_patch_code( 0x3a11b, 0xdb );
	sys16_patch_code( 0x3a11c, 0x4e );
	sys16_patch_code( 0x3a11d, 0x75 );
	sys16_patch_code( 0xaf8e, 0x66 );

	/* fix missing credit text */
	sys16_patch_code( 0x3f9a, 0xec );
	sys16_patch_code( 0x3f9b, 0x36 );

	sys16_update_proc = astorm_update_proc;
}

static DRIVER_INIT( astorm ){
	unsigned char *RAM= memory_region(REGION_CPU2);
	machine_init_sys16_onetime();
	sys18_splittab_fg_x=&sys16_textram[0x0f80/2];
	sys18_splittab_bg_x=&sys16_textram[0x0fc0/2];

	memcpy(RAM,&RAM[0x10000],0xa000);
	sys16_MaxShadowColors = 0; // doesn't seem to use transparent shadows
}

/*****************************************************************************/

static MACHINE_DRIVER_START( system18 )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 10000000)
	MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_CPU_ADD_TAG("sound", Z80, 4096000*2) /* overclocked to fix sound, but wrong! */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem_18,sound_writemem_18)
	MDRV_CPU_PORTS(sound_readport_18,sound_writeport_18)

	MDRV_FRAMES_PER_SECOND(57.23)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(40*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(sys16_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048*ShadowColorsMultiplier)

	MDRV_VIDEO_START(system18)
	MDRV_VIDEO_UPDATE(system18)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD_TAG("3438", YM3438, sys18_ym3438_interface)
	MDRV_SOUND_ADD_TAG("5c68", RF5C68, sys18_rf5c68_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( astorm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(astorm_readmem,astorm_writemem)

	MDRV_MACHINE_INIT(astorm)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( moonwalk )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(moonwalk_readmem,moonwalk_writemem)

	MDRV_MACHINE_INIT(moonwalk)
	
	MDRV_SOUND_ADD(SAMPLES, moonwalker_samples_set)
	moonwalker_playing = true;
	moon_diddy = false;
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( shdancer )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(shdancer_readmem,shdancer_writemem)

	MDRV_MACHINE_INIT(shdancer)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( shdancbl )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(system18)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(shdancbl_readmem,shdancbl_writemem)

	MDRV_MACHINE_INIT(shdancbl)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( shdancrj )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(shdancer)

	MDRV_MACHINE_INIT(shdancrj)
MACHINE_DRIVER_END


/***************************************************************************/

INPUT_PORTS_START( astorm )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )
	SYS16_COINAGE
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easiest" )
	PORT_DIPSETTING(    0x08, "Easier" )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x1c, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x14, "Harder" )
	PORT_DIPSETTING(    0x18, "Hardest" )
	PORT_DIPSETTING(    0x00, "Special" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Chutes" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_START /* player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
INPUT_PORTS_END

INPUT_PORTS_START( moonwalk )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_START /* service */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_TILT, "Test", KEYCODE_T, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	SYS16_COINAGE
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x08, "Player Vitality" )
	PORT_DIPSETTING(    0x08, "Low" )
	PORT_DIPSETTING(    0x00, "High" )
	PORT_DIPNAME( 0x10, 0x00, "Play Mode" )
	PORT_DIPSETTING(    0x10, "2 Players" )
	PORT_DIPSETTING(    0x00, "3 Players" )
	PORT_DIPNAME( 0x20, 0x20, "Coin Mode" )
	PORT_DIPSETTING(    0x20, "Common" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_START /* player 3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
//	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
INPUT_PORTS_END

INPUT_PORTS_START( shdancer )
	PORT_START /* player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_START /* player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	SYS16_SERVICE
	SYS16_COINAGE
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "2 Credits to Start" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, "Time Adjust" )
	PORT_DIPSETTING(    0x00, "2.20" )
	PORT_DIPSETTING(    0x40, "2.40" )
	PORT_DIPSETTING(    0xc0, "3.00" )
	PORT_DIPSETTING(    0x80, "3.30" )
INPUT_PORTS_END

/*****************************************************************************/

// Ace Attacker
ROM_START( aceattac )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "11491.4a", 0x000000, 0x10000, CRC(77b820f1) SHA1(c33183c94c5029e2c4d6444f37404da66aacecc4) )
	ROM_LOAD16_BYTE( "11489.1a", 0x000001, 0x10000, CRC(bbe623c5) SHA1(6d047699c7b6df7ebb7a3c9bee032e2536eed84c) )
	ROM_LOAD16_BYTE( "11492.5a", 0x020000, 0x10000, CRC(d8bd3139) SHA1(54915d4e8a616e0e54135ca34daf4357b8bfa068) )
	ROM_LOAD16_BYTE( "11490.2a", 0x020001, 0x10000, CRC(38cb3a41) SHA1(1d74cc69907cdff2d85e965b80bf3f551465257e) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "11493.9b",  0x00000, 0x10000, CRC(654485d9) SHA1(b431270564c4e33fd70c8c85af1fcbff8b59ba49) )
	ROM_LOAD( "11494.10b", 0x10000, 0x10000, CRC(b67971ab) SHA1(95cb6927baf425bcc290832ea9741b19852c7a1b) )
	ROM_LOAD( "11495.11b", 0x20000, 0x10000, CRC(b687ab61) SHA1(b08130a9d777c918972895136b1bf520d7117114) )

	ROM_REGION( 0x80000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "11501.1b", 0x00001, 0x10000, CRC(09179ead) SHA1(3e6bf04e1e9ea867d087a47ff04ad0a064a8e299) )
	ROM_LOAD16_BYTE( "11502.2b", 0x00000, 0x10000, CRC(a3ee36b8) SHA1(bc946ad67b8ad09d947465ab73160885a4a57be5) )
	ROM_LOAD16_BYTE( "11503.3b", 0x20001, 0x10000, CRC(344c0692) SHA1(3125701f6bb91d8f64515e214b571e169c30a444) )
	ROM_LOAD16_BYTE( "11504.4b", 0x20000, 0x10000, CRC(7cae7920) SHA1(9f00e01d7cc86a0bf4f84e78a56b7efbb97c5591) )
	ROM_LOAD16_BYTE( "11505.5b", 0x40001, 0x10000, CRC(b67f1ecf) SHA1(3a26cdf91e5a1a11c1a8857e713a9e00cc1bfce0) )
	ROM_LOAD16_BYTE( "11506.6b", 0x40000, 0x10000, CRC(b0104def) SHA1(c81a66ec3a600c1d4c5d058caef15936c59b2574) )
	ROM_LOAD16_BYTE( "11507.7b", 0x60001, 0x10000, CRC(a2af710a) SHA1(1c8b75b72797146c2eb788511f8cb1b367fc3e0d) )
	ROM_LOAD16_BYTE( "11508.8b", 0x60000, 0x10000, CRC(5cbb833c) SHA1(dc7041b6a4fa75d050bfc2176d0f9e242b55a0b8) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "11496.7a",	 0x00000, 0x08000, CRC(82cb40a9) SHA1(daf2233438331ba6e6ff8bda4015e92d23e616c5) )
	ROM_LOAD( "11497.8a",    0x10000, 0x08000, CRC(b04f62cc) SHA1(29b468e5a565dc14e00c371913663eca66ccb44d) )
	ROM_LOAD( "11498.9a",    0x18000, 0x08000, CRC(97baf52b) SHA1(97800014250b0099c7e53d597b0ef02ae14e6dba) )
	ROM_LOAD( "11499.10a",   0x20000, 0x08000, CRC(ea332866) SHA1(eba0b422b39f7f3f81af1059043a87d944c4aff7) )
	ROM_LOAD( "11500.11a",   0x28000, 0x08000, CRC(2ddf1c31) SHA1(77b20edbbd801072b20d9dc5e8fa2f468e53d79e) )
ROM_END

// Alien Storm
ROM_START( astorm )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr13085.bin", 0x000000, 0x40000, CRC(15f74e2d) SHA1(30d9d099ec18907edd3d20df312565c3bd5a80de) )
	ROM_LOAD16_BYTE( "epr13084.bin", 0x000001, 0x40000, CRC(9687b38f) SHA1(cdeb5b4f06ad4ad8ca579392c1ec901487b08e76) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13083.bin", 0x10000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) )
	ROM_LOAD( "epr13076.bin", 0x30000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr13077.bin", 0x70000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr13078.bin", 0xb0000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astorm2p )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr13182.bin", 0x000000, 0x40000, CRC(e31f2a1c) SHA1(690ee10c36e5bb6175470fb5564527e0e4a94d2c) )
	ROM_LOAD16_BYTE( "epr13181.bin", 0x000001, 0x40000, CRC(78cd3b26) SHA1(a81b807c5da625d8e4648ae80c41e4ca3870c0fa) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "mpr13089.bin", 0x000000, 0x40000, CRC(2a4227f0) SHA1(47284dce8f896f8e8eace9c20302842cacb479c1) )
	ROM_LOAD16_BYTE( "mpr13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "ep13083a.bin", 0x10000, 0x20000, CRC(e7528e06) SHA1(1f4e618692c20aeb316d578c5ded12440eb072ab) )
	ROM_LOAD( "epr13076.bin", 0x30000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr13077.bin", 0x70000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr13078.bin", 0xb0000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

ROM_START( astormbl )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "astorm.a6", 0x000000, 0x40000, CRC(7682ed3e) SHA1(b857352ad9c66488e91f60989472638c483e4ae8) )
	ROM_LOAD16_BYTE( "astorm.a5", 0x000001, 0x40000, CRC(efe9711e) SHA1(496fd9e30941fde1658fab7292a669ef7964cecb) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr13073.bin", 0x00000, 0x40000, CRC(df5d0a61) SHA1(79ad71de348f280bad847566c507b7a31f022292) )
	ROM_LOAD( "epr13074.bin", 0x40000, 0x40000, CRC(787afab8) SHA1(a119042bb2dad54e9733bfba4eaab0ac5fc0f9e7) )
	ROM_LOAD( "epr13075.bin", 0x80000, 0x40000, CRC(4e01b477) SHA1(4178ce4a87ea427c3b0195e64acef6cddfb3485f) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13082.bin", 0x000001, 0x40000, CRC(a782b704) SHA1(ba15bdfbc267b8d86f03e5310ce60846ff846de3) )
	ROM_LOAD16_BYTE( "astorm.a11",   0x000000, 0x40000, CRC(7829c4f3) SHA1(3adb7aa7f70163d3848c98316e18b9783c41d663) )
	ROM_LOAD16_BYTE( "mpr13081.bin", 0x080001, 0x40000, CRC(eb510228) SHA1(4cd387b160ec7050e1300ebe708853742169e643) )
	ROM_LOAD16_BYTE( "mpr13088.bin", 0x080000, 0x40000, CRC(3b6b4c55) SHA1(970495c54b3e1893ee8060f6ca1338c2cbbd1074) )
	ROM_LOAD16_BYTE( "mpr13080.bin", 0x100001, 0x40000, CRC(e668eefb) SHA1(d4a087a238b4d3ac2d23fe148d6a73018e348a89) )
	ROM_LOAD16_BYTE( "mpr13087.bin", 0x100000, 0x40000, CRC(2293427d) SHA1(4fd07763ff060afd594e3f64fa4750577f56c80e) )
	ROM_LOAD16_BYTE( "epr13079.bin", 0x180001, 0x40000, CRC(de9221ed) SHA1(5e2e434d1aa547be1e5652fc906d2e18c5122023) )
	ROM_LOAD16_BYTE( "epr13086.bin", 0x180000, 0x40000, CRC(8c9a71c4) SHA1(40b774765ac888792aad46b6351a24b7ef40d2dc) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13083.bin", 0x10000, 0x20000, CRC(5df3af20) SHA1(e49105fcfd5bf37d14bd760f6adca5ce2412883d) )
	ROM_LOAD( "epr13076.bin", 0x30000, 0x40000, CRC(94e6c76e) SHA1(f99e58a9bf372c41af211bd9b9ea3ac5b924c6ed) )
	ROM_LOAD( "epr13077.bin", 0x70000, 0x40000, CRC(e2ec0d8d) SHA1(225b0d223b7282cba7710300a877fb4a2c6dbabb) )
	ROM_LOAD( "epr13078.bin", 0xb0000, 0x40000, CRC(15684dc5) SHA1(595051006de24f791dae937584e502ff2fa31d9c) )
ROM_END

// Bloxeed
ROM_START( bloxeed )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "rom-e.rom", 0x000000, 0x20000, CRC(a481581a) SHA1(5ce5a0a082622919d2fe0e7d52ec807b2e2c25a2) )
	ROM_LOAD16_BYTE( "rom-o.rom", 0x000001, 0x20000, CRC(dd1bc3bf) SHA1(c0d79862a349ea4dac103c17325633c5dd4a93d1) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "scr0.rom", 0x00000, 0x10000, CRC(e024aa33) SHA1(d734be240cd05031aaadf9735c0b1b00e8e6d4cb) )
	ROM_LOAD( "scr1.rom", 0x10000, 0x10000, CRC(8041b814) SHA1(29fa49ba9a73eed07865a86ea774e2c6a60aed5b) )
	ROM_LOAD( "scr2.rom", 0x20000, 0x10000, CRC(de32285e) SHA1(8994dc128d6a23763e5fcfca1868b336d4aa0a21) )

	ROM_REGION( 0x20000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "obj0-e.rom", 0x00001, 0x10000, CRC(90d31a8c) SHA1(1747652a5109ce65add197cf06535f2463a99fdc) )
	ROM_LOAD16_BYTE( "obj0-o.rom", 0x00000, 0x10000, CRC(f0c0f49d) SHA1(7ecd591265165f3149241e2ceb5059faab88360f) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "sound0.rom",	 0x00000, 0x20000, CRC(6f2fc63c) SHA1(3cce22c8f80013f05b5a2d36c42a61a81e4d6cbd) )
ROM_END

// Clutch Hitter
ROM_START( cltchitr )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "epr13795.6a", 0x000000, 0x40000, CRC(b0b60b67) SHA1(ee3325ddb7461008f556c1696157a766225b9ef5) )
	ROM_LOAD16_BYTE( "epr13751.4a", 0x000001, 0x40000, CRC(c8d80233) SHA1(69cdbb989f580abbb006820347becf8d233fa773) )
	ROM_LOAD16_BYTE( "epr13786.7a", 0x080000, 0x40000, CRC(3095dac0) SHA1(20edce74b6f2a82a3865613e601a0e212615d0e4) )
	ROM_LOAD16_BYTE( "epr13784.5a", 0x080001, 0x40000, CRC(80c8180d) SHA1(80e72ab7d97714009fd31b3d50176af84b0dcdb7) )

	ROM_REGION( 0x180000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13787.10a", 0x000000, 0x80000, CRC(f05c68c6) SHA1(b6a0535b6c734a0c89fdb6506c32ffe6ab3aa8cd) )
	ROM_LOAD( "mpr13788.11a", 0x080000, 0x80000, CRC(0106fea6) SHA1(e16e2a469ecbbc704021dee6264db30aa0898368) )
	ROM_LOAD( "mpr13789.12a", 0x100000, 0x80000, CRC(09ba8835) SHA1(72e83dd1793a7f4b2b881e71f262493e3d4992b3) )

	ROM_REGION( 0x300000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13773.1c",  0x000001, 0x80000, CRC(3fc600e5) SHA1(8bec4ecf6a4e4b38d13133960036a5a4800a668e) )
	ROM_LOAD16_BYTE( "mpr13774.2c",  0x000000, 0x80000, CRC(2411a824) SHA1(0e383ccc4e0830ffb395d5102e2950610c147007) )
	ROM_LOAD16_BYTE( "mpr13775.3c",  0x100001, 0x80000, CRC(cf527bf6) SHA1(1f9cf1f0e92709f0465dc97ebbdaa715a419f7a7) )
	ROM_LOAD16_BYTE( "mpr13779.10c", 0x100000, 0x80000, CRC(c707f416) SHA1(e6a9d89849f7f1c303a3ca29a629f81397945a2d) )
	ROM_LOAD16_BYTE( "mpr13780.11c", 0x200001, 0x80000, CRC(a4c341e0) SHA1(15a0b5a42b56465a7b7df98968cc2ed177ce6f59) )
	ROM_LOAD16_BYTE( "mpr13781.12c", 0x200000, 0x80000, CRC(f33b13af) SHA1(d3eb64dcf12d532454bf3cd6c86528c0f11ee316) )

	ROM_REGION( 0x180000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13793.7c",    0x000000, 0x80000, CRC(a3d31944) SHA1(44d17aa0598eacfac4b12c8955fd1067ca09abbd) )
	ROM_LOAD( "epr13791.5c",	0x080000, 0x80000, CRC(35c16d80) SHA1(7ed04600748c5efb63e25f066daa163e9c0d8038) )
	ROM_LOAD( "epr13792.6c",    0x100000, 0x80000, CRC(808f9695) SHA1(d50677d20083ad56dbf0864db05f76f93a4e9eba) )
ROM_END

// DD Crew
ROM_START( ddcrew )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "14153.6a", 0x000000, 0x40000, CRC(e01fae0c) SHA1(7166f955324f73e94d8ae6d2a5b2f4b497e62933) )
	ROM_LOAD16_BYTE( "14152.4a", 0x000001, 0x40000, CRC(69c7b571) SHA1(9fe4815a1cff0a46a754a6bdee12abaf7beb6501) )
	ROM_LOAD16_BYTE( "14141.7a", 0x080000, 0x40000, CRC(080a494b) SHA1(64522dccbf6ed856ab80aa185454183df87d7ae9) )
	ROM_LOAD16_BYTE( "14139.5a", 0x080001, 0x40000, CRC(06c31531) SHA1(d084cb72bf83578b34e959bb60a0695faf4161f8) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "14127.1c", 0x00000, 0x40000, CRC(2228cd88) SHA1(5774bb6a401c3da05c5f3c9d3996b20bb3713cb2) )
	ROM_LOAD( "14128.2c", 0x40000, 0x40000, CRC(edba8e10) SHA1(25a2833ead4ca363802ddc2eb97c40976502921a) )
	ROM_LOAD( "14129.3c", 0x80000, 0x40000, CRC(e8ecc305) SHA1(a26d0c5c7826cd315f8b2c27e5a503a2a7b535c4) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "14134.10c", 0x000001, 0x80000, CRC(4fda6a4b) SHA1(a9e582e494ab967e8f3ccf4d5844bb8ef889928c) )
	ROM_LOAD16_BYTE( "14142.10a", 0x000000, 0x80000, CRC(3cbf1f2a) SHA1(80b6b006936740087786acd538e28aca85fa6894) )
	ROM_LOAD16_BYTE( "14135.11c", 0x100001, 0x80000, CRC(e9c74876) SHA1(aff9d071e77f01c6937188bf67be38fa898343e6) )
	ROM_LOAD16_BYTE( "14143.11a", 0x100000, 0x80000, CRC(59022c31) SHA1(5e1409fe0f29284dc6a3ffacf69b761aae09f132) )
	ROM_LOAD16_BYTE( "14136.12c", 0x200001, 0x80000, CRC(720d9858) SHA1(8ebcb8b3e9555ca48b28908d47dcbbd654398b6f) )
	ROM_LOAD16_BYTE( "14144.12a", 0x200000, 0x80000, CRC(7775fdd4) SHA1(a03cac039b400b651a4bf2167a8f2338f488ce26) )
	ROM_LOAD16_BYTE( "14137.13c", 0x300001, 0x80000, CRC(846c4265) SHA1(58d0c213d085fb4dee18b7aefb05087d9d522950) )
	ROM_LOAD16_BYTE( "14145.13a", 0x300000, 0x80000, CRC(0e76c797) SHA1(9a44dc948e84e5acac36e80105c2349ee78e6cfa) )

	ROM_REGION( 0x1a0000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "14133.7c",	 0x000000, 0x20000, CRC(cff96665) SHA1(b4dc7f1a03415ebebdb99a82ae89328c345e7678) )
	ROM_LOAD( "14130.4c",    0x020000, 0x80000, CRC(948f34a1) SHA1(d4c6728d5eea06cee6ac15a34ec8cccb4cc4b982) )
	ROM_LOAD( "14131.5c",    0x0a0000, 0x80000, CRC(be5a7d0b) SHA1(c2c598b0cf711273fdd568f3401375e9772c1d61) )
	ROM_LOAD( "14132.6c",    0x120000, 0x80000, CRC(1fae0220) SHA1(8414c74318ea915816c6b67801ac7c8c3fc905f9) )
ROM_END

// Laser Ghost
ROM_START( lghost )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "13429", 0x000000, 0x20000, CRC(0e0ccf26) SHA1(072c39771f4d8806c05499bad9a0e7f63709333e) )
	ROM_LOAD16_BYTE( "13437", 0x000001, 0x20000, CRC(38b4dc2f) SHA1(28071d4bc1e658e97f6a63ac07aea5e38cbced24) )
	ROM_LOAD16_BYTE( "13411", 0x040000, 0x20000, CRC(c3aeae07) SHA1(922f6c6cd2cb2c191be221434e7a1bbff81b57cb) )
	ROM_LOAD16_BYTE( "13413", 0x040001, 0x20000, CRC(75f43e21) SHA1(a8f65972604bf4ad886d90ac2afffccfc27ac769) )

	ROM_REGION( 0x60000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "13414", 0x00000, 0x20000, CRC(82025f3b) SHA1(bc496ec3bea7eb61534b964f11f32c297b36bf10) )
	ROM_LOAD( "13415", 0x20000, 0x20000, CRC(a76852e9) SHA1(45b570e6b28678d98540a2b6c87f0fea1c98a471) )
	ROM_LOAD( "13416", 0x40000, 0x20000, CRC(e88db149) SHA1(a43a3682cbdcfd8dce22f2f0b6bdff8b3e26765e) )

	ROM_REGION( 0x100000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "13603", 0x00001, 0x20000, CRC(2e3cc07b) SHA1(520cd9a264860f8e9bbeeb19b203cceaa404fc4e) )
	ROM_LOAD16_BYTE( "13604", 0x00000, 0x20000, CRC(576388af) SHA1(699b9223d90ec69a56f3c7721315c0344d00696a) )
	ROM_LOAD16_BYTE( "13421", 0x40001, 0x20000, CRC(abee8771) SHA1(67cfbaefd3a5a45574bb127b5cdbd26a8537cde0) )
	ROM_LOAD16_BYTE( "13424", 0x40000, 0x20000, CRC(260ab077) SHA1(1d9ad0e80341f3993364e03dc0be5f4d0a2261af) )
	ROM_LOAD16_BYTE( "13422", 0x80001, 0x20000, CRC(36cef12c) SHA1(50d29ffd59f245a911b5116dbcac27d0ed467888) )
	ROM_LOAD16_BYTE( "13425", 0x80000, 0x20000, CRC(e0ff8807) SHA1(88b1d8d32662ba6d261a5a447418bdcd62ca7acb) )
	ROM_LOAD16_BYTE( "13423", 0xc0001, 0x20000, CRC(5b8e0053) SHA1(40b2b44f9956e36294194e3c26973b33556201a8) )
	ROM_LOAD16_BYTE( "13426", 0xc0000, 0x20000, CRC(c689853b) SHA1(3444c1a256531b4371bc79e93d37f6b0ff0ca2d9) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "13417",	 0x00000, 0x20000, CRC(cd7beb49) SHA1(2435ce000f1eefdd06b27ea93e22fd82c0e999d2) )
	ROM_LOAD( "13420",   0x20000, 0x20000, CRC(03199cbb) SHA1(e6195ff31a2fd5a298669995d7f8a174c750fdc6) )
	ROM_LOAD( "13419",   0x40000, 0x20000, CRC(a918ef68) SHA1(1e0394e77b175ab3a552c3e18351427c6e8cc64b) )
	ROM_LOAD( "13418",   0x60000, 0x20000, CRC(4006c9f1) SHA1(e9cecfafdbcfdf716aa2ba911273b18550faea98) )
ROM_END

ROM_START( moonwalk )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code - custom cpu 317-0159 */
	ROM_LOAD16_BYTE( "epr13235.a6", 0x000000, 0x40000, CRC(6983e129) SHA1(a8dd430620ab8ce11df46aa208d762d47f510464) )
	ROM_LOAD16_BYTE( "epr13234.a5", 0x000001, 0x40000, CRC(c9fd20f2) SHA1(9476e6481e6d8f223acd52f543fa04f408d48dc3) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x10000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr13219.b4", 0x30000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr13220.b5", 0x70000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr13249.b6", 0xb0000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )
ROM_END

ROM_START( moonwlka )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code - custom cpu 317-0158 */
	ROM_LOAD16_BYTE( "epr13233", 0x000000, 0x40000, CRC(f3dac671) SHA1(cd9d372c7e272d2371bc1f9fb0167831c804423f) )
	ROM_LOAD16_BYTE( "epr13232", 0x000001, 0x40000, CRC(541d8bdf) SHA1(6a99153fddca246ba070e93c4bacd145f15f76bf) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x10000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr13219.b4", 0x30000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr13220.b5", 0x70000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr13249.b6", 0xb0000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )
ROM_END

ROM_START( moonwlkb )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "moonwlkb.01", 0x000000, 0x10000, CRC(f49cdb16) SHA1(34b7e98d31c3b9db2f0f055d7b249b0e5e5cb746) )
	ROM_LOAD16_BYTE( "moonwlkb.05", 0x000001, 0x10000, CRC(c483f29f) SHA1(8fdfa764d8e49754844a9dc001400d439f9af9f0) )
	ROM_LOAD16_BYTE( "moonwlkb.02", 0x020000, 0x10000, CRC(0bde1896) SHA1(42731ae90d56918dc50c0dcb53d092dcfb957159) )
	ROM_LOAD16_BYTE( "moonwlkb.06", 0x020001, 0x10000, CRC(5b9fc688) SHA1(53d8143c3876548f63b392f0ea16c0e7c30a7917) )
	ROM_LOAD16_BYTE( "moonwlkb.03", 0x040000, 0x10000, CRC(0c5fe15c) SHA1(626e3f37f019448c3c96bf73b2d2b5fe4b3716c0) )
	ROM_LOAD16_BYTE( "moonwlkb.07", 0x040001, 0x10000, CRC(9e600704) SHA1(efd3d450b26f81dc2b74f44b4aaf906fa017e437) )
	ROM_LOAD16_BYTE( "moonwlkb.04", 0x060000, 0x10000, CRC(64692f79) SHA1(ad7f32997b78863e3aa3214018cdd24e3ec9c5cb) )
	ROM_LOAD16_BYTE( "moonwlkb.08", 0x060001, 0x10000, CRC(546ca530) SHA1(51f74878fdc221fee026e2e6a7ca96f290c8947f) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "mpr13216.b1", 0x00000, 0x40000, CRC(862d2c03) SHA1(3c5446d702a639b62a602c6d687f9875d8450218) )
	ROM_LOAD( "mpr13217.b2", 0x40000, 0x40000, CRC(7d1ac3ec) SHA1(8495357304f1df135bba77ef3b96e79a883b8ff0) )
	ROM_LOAD( "mpr13218.b3", 0x80000, 0x40000, CRC(56d3393c) SHA1(50a2d065060692c9ecaa56046a781cb21d93e554) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "mpr13224.b11", 0x000001, 0x40000, CRC(c59f107b) SHA1(10fa60fca6e34eda277c483bb1c0e81bb88c8a47) )
	ROM_LOAD16_BYTE( "mpr13231.a11", 0x000000, 0x40000, CRC(a5e96346) SHA1(a854f4dd5dc16975373255110fdb8ab3d121b1af) )
	ROM_LOAD16_BYTE( "mpr13223.b10", 0x080001, 0x40000, CRC(364f60ff) SHA1(9ac887ec0b2e32b504b7c6a5f3bb1ce3fe41a15a) )
	ROM_LOAD16_BYTE( "mpr13230.a10", 0x080000, 0x40000, CRC(9550091f) SHA1(bb6e898f7b540e130fd338c10f74609a7604cef4) )
	ROM_LOAD16_BYTE( "mpr13222.b9",  0x100001, 0x40000, CRC(523df3ed) SHA1(2e496125e75decd674c3a08404fbdb53791a965d) )
	ROM_LOAD16_BYTE( "mpr13229.a9",  0x100000, 0x40000, CRC(f40dc45d) SHA1(e9468cef428f52ecdf6837c6d9a9fea934e7676c) )
	ROM_LOAD16_BYTE( "epr13221.b8",  0x180001, 0x40000, CRC(9ae7546a) SHA1(5413b0131881b0b32bac8de51da9a299835014bb) )
	ROM_LOAD16_BYTE( "epr13228.a8",  0x180000, 0x40000, CRC(de3786be) SHA1(2279bb390aa3efab9aeee0a643e5cb6a4f5933b6) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr13225.a4", 0x10000, 0x20000, CRC(56c2e82b) SHA1(d5755a1bb6e889d274dc60e883d4d65f12fdc877) )
	ROM_LOAD( "mpr13219.b4", 0x30000, 0x40000, CRC(19e2061f) SHA1(2dcf1718a43dab4da53b4f67722664e70ddd2169) )
	ROM_LOAD( "mpr13220.b5", 0x70000, 0x40000, CRC(58d4d9ce) SHA1(725e73a656845b02702ef131b4c0aa2a73cdd02e) )
	ROM_LOAD( "mpr13249.b6", 0xb0000, 0x40000, CRC(623edc5d) SHA1(c32d9f818d40f311877fbe6532d9e95b6045c3c4) )
ROM_END

// Shadow Dancer
ROM_START( shdancer )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "shdancer.a6", 0x000000, 0x40000, CRC(3d5b3fa9) SHA1(370dd6e8ab9fb9e77eee9262d13fbdb4cf575abc) )
	ROM_LOAD16_BYTE( "shdancer.a5", 0x000001, 0x40000, CRC(2596004e) SHA1(1b993aa74e7559f7e99253fd2144db9449c04cce) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "sd12712.bin", 0x00000, 0x40000, CRC(9bdabe3d) SHA1(4bb30fa2d4cdefe4a864cef7153b516bc5b02c42) )
	ROM_LOAD( "sd12713.bin", 0x40000, 0x40000, CRC(852d2b1c) SHA1(8e5bc83d45e48b621ea3016207f2028fe41701e6) )
	ROM_LOAD( "sd12714.bin", 0x80000, 0x40000, CRC(448226ce) SHA1(3060e4a43311069e2691d659c1e0c1a48edfeedb) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "sd12719.bin",  0x000001, 0x40000, CRC(d6888534) SHA1(2201f1921a68cf39e5a94b487c90e48d032d630f) )
	ROM_LOAD16_BYTE( "sd12726.bin",  0x000000, 0x40000, CRC(ff344945) SHA1(2743778c42f53321f9691d60bbf94ea8baf1382f) )
	ROM_LOAD16_BYTE( "sd12718.bin",  0x080001, 0x40000, CRC(ba2efc0c) SHA1(459a1a280f870c94aefb70127ed007cb090ed203) )
	ROM_LOAD16_BYTE( "sd12725.bin",  0x080000, 0x40000, CRC(268a0c17) SHA1(2756054fa3c3aed30a1fce5e41acb0ceaebe90b5) )
	ROM_LOAD16_BYTE( "sd12717.bin",  0x100001, 0x40000, CRC(c81cc4f8) SHA1(22f364e85057ceef533e051c8d0755b9691c5ec4) )
	ROM_LOAD16_BYTE( "sd12724.bin",  0x100000, 0x40000, CRC(0f4903dc) SHA1(851bd60e877c9e39be23dc1fde91efc9da513c29) )
	ROM_LOAD16_BYTE( "sd12716.bin",  0x180001, 0x40000, CRC(a870e629) SHA1(29f6633240f9737ec19e16100decc7aa045b2060) )
	ROM_LOAD16_BYTE( "sd12723.bin",  0x180000, 0x40000, CRC(c606cf90) SHA1(cb53ae9a6da1eb31c584173d1fbbd1c8539fb54c) )

	ROM_REGION( 0x70000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "sd12720.bin", 0x10000, 0x20000, CRC(7a0d8de1) SHA1(eca5e2104e5b3e772d083a718171234f06ea8a55) )
	ROM_LOAD( "sd12715.bin", 0x30000, 0x40000, CRC(07051a52) SHA1(d48658497f4a34665d3e051f893ff057c38925ae) )
ROM_END

ROM_START( shdancbl )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "ic39", 0x000000, 0x10000, CRC(adc1781c) SHA1(b2ca2831a48779df7533e6b2a406ee539e1f650c) )
	ROM_LOAD16_BYTE( "ic53", 0x000001, 0x10000, CRC(1c1ac463) SHA1(21075f7afae372daef197f04f5f12d14479a8140) )
	ROM_LOAD16_BYTE( "ic38", 0x020000, 0x10000, CRC(cd6e155b) SHA1(e37b53cc431533091d26b37be9b8e30494de5faf) )
	ROM_LOAD16_BYTE( "ic52", 0x020001, 0x10000, CRC(bb3c49a4) SHA1(ab01a6de1a6d338d30f9cfea7b3bf80dda67f215) )
	ROM_LOAD16_BYTE( "ic37", 0x040000, 0x10000, CRC(1bd8d5c3) SHA1(4d663362c059e112ac6c742d80200be98d50d175) )
	ROM_LOAD16_BYTE( "ic51", 0x040001, 0x10000, CRC(ce2e71b4) SHA1(3e251319cd4c8c63c66e6b92b2eef514d79dba8e) )
	ROM_LOAD16_BYTE( "ic36", 0x060000, 0x10000, CRC(bb861290) SHA1(62ea8eec74c6b1f5530ee86f97ad821daeac26ad) )
	ROM_LOAD16_BYTE( "ic50", 0x060001, 0x10000, CRC(7f7b82b1) SHA1(675020b57ce689b2767ff83773e2b828cda5aeed) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "ic4",  0x00000, 0x20000, CRC(f0a016fe) SHA1(1426f3fbf50a04a8c5e998e071ca0e78d15f37a8) )
	ROM_LOAD( "ic18", 0x20000, 0x20000, CRC(f6bee053) SHA1(39ee5edfcc67bb4855217c7428254f3e8c862ba0) )
	ROM_LOAD( "ic3",  0x40000, 0x20000, CRC(e07e6b5d) SHA1(bdeb1193415049d0c9261ca261073bdd9e251b88) )
	ROM_LOAD( "ic17", 0x60000, 0x20000, CRC(f59deba1) SHA1(21188d22fe607281bb7da1e1f418a33d4a315695) )
	ROM_LOAD( "ic2",  0x80000, 0x20000, CRC(60095070) SHA1(913c2ee51fb6f838f3c6cbd27032bdf754fbadf1) )
	ROM_LOAD( "ic16", 0xa0000, 0x20000, CRC(0f0d5dd3) SHA1(76812e2f831256a8b6598257dd84a7f07443642e) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "ic73", 0x000001, 0x10000, CRC(59e77c96) SHA1(08da058529ac83352a4528d3792a21edda348f7a) )
	ROM_LOAD16_BYTE( "ic74", 0x000000, 0x10000, CRC(90ea5407) SHA1(4bdd93c86cb35822517433d491aa8be6857dd36c) )
	ROM_LOAD16_BYTE( "ic75", 0x020001, 0x10000, CRC(27d2fa61) SHA1(0ba3cd9448e54ce9fc9433f3edd28de9a4e451e9) )
	ROM_LOAD16_BYTE( "ic76", 0x020000, 0x10000, CRC(f36db688) SHA1(a527298ce9ca1d9f5aa7b9eac93985f34ca8119f) )
	ROM_LOAD16_BYTE( "ic58", 0x040001, 0x10000, CRC(9cd5c8c7) SHA1(54c2d0a683bda37eb9a75f90f4ca5e620c09c4cf) )
	ROM_LOAD16_BYTE( "ic59", 0x040000, 0x10000, CRC(ff40e872) SHA1(bd2c4aac427d106a46318f4cb2eb05c34d3c70b6) )
	ROM_LOAD16_BYTE( "ic60", 0x060001, 0x10000, CRC(826d7245) SHA1(bb3394de058bd63b9939cd05f22c925e0cdc840a) )
	ROM_LOAD16_BYTE( "ic61", 0x060000, 0x10000, CRC(dcf8068b) SHA1(9c78de224df76fc90fb90f1bbd9b22dad0874f69) )
	ROM_LOAD16_BYTE( "ic77", 0x080001, 0x10000, CRC(f93470b7) SHA1(1041afa43aa8d0589d6def9743721cdbda617f78) )
	ROM_LOAD16_BYTE( "ic78", 0x080000, 0x10000, CRC(4d523ea3) SHA1(053c30778017127dddeae0783af463aef17bcc9a) )
	ROM_LOAD16_BYTE( "ic95", 0x0a0001, 0x10000, CRC(828b8294) SHA1(f2cdb882fb0709a909e6ef98f0315aceeb8bf283) )
	ROM_LOAD16_BYTE( "ic94", 0x0a0000, 0x10000, CRC(542b2d1e) SHA1(1ce91aea6c49e6e365a91c30ca3049682c2162da) )
	ROM_LOAD16_BYTE( "ic62", 0x0c0001, 0x10000, CRC(50ca8065) SHA1(8c0d6ae34b9da6c376df387e8fc8b1068bcb4dcb) )
	ROM_LOAD16_BYTE( "ic63", 0x0c0000, 0x10000, CRC(d1866aa9) SHA1(524c82a12a1c484a246b8d49d9f05a774d008108) )
	ROM_LOAD16_BYTE( "ic90", 0x0e0001, 0x10000, CRC(3602b758) SHA1(d25b6c8420e07d0f2ac3e1d8717f14738466df16) )
	ROM_LOAD16_BYTE( "ic89", 0x0e0000, 0x10000, CRC(1ba4be93) SHA1(6f4fe2016e375be3df477436f5cde7508a24ecd1) )
	ROM_LOAD16_BYTE( "ic79", 0x100001, 0x10000, CRC(f22548ee) SHA1(723cb7604784c6715817daa8c86c18c6bcd1388d) )
	ROM_LOAD16_BYTE( "ic80", 0x100000, 0x10000, CRC(6209f7f9) SHA1(09b33c99d972a62af8ef56dacfa6262f002aba0c) )
	ROM_LOAD16_BYTE( "ic81", 0x120001, 0x10000, CRC(34692f23) SHA1(56126a81ac279662e3e3423da5205f65a62c4600) )
	ROM_LOAD16_BYTE( "ic82", 0x120000, 0x10000, CRC(7ae40237) SHA1(fae97cfcfd3cd557da3330158831e4727c438745) )
	ROM_LOAD16_BYTE( "ic64", 0x140001, 0x10000, CRC(7a8b7bcc) SHA1(00cbbbc4b3db48ca3ac65ff56b02c7d63a1b898a) )
	ROM_LOAD16_BYTE( "ic65", 0x140000, 0x10000, CRC(90ffca14) SHA1(00962e5309a79ce34c6f420036054bc607595dfe) )
	ROM_LOAD16_BYTE( "ic66", 0x160001, 0x10000, CRC(5d655517) SHA1(2a1c197dde62bd7946ca7b5f1c2833bdbc2e2e32) )
	ROM_LOAD16_BYTE( "ic67", 0x160000, 0x10000, CRC(0e5d0855) SHA1(3c15088f7fdda5c2bba9c89d244bbcff022f05fd) )
	ROM_LOAD16_BYTE( "ic83", 0x180001, 0x10000, CRC(a9040a32) SHA1(7b0b375285f528b2833c50892b55b0d4c550506d) )
	ROM_LOAD16_BYTE( "ic84", 0x180000, 0x10000, CRC(d6810031) SHA1(a82857a9ac442fbe076cdafcf7390765391ed136) )
	ROM_LOAD16_BYTE( "ic92", 0x1a0001, 0x10000, CRC(b57d5cb5) SHA1(636f1a07a84d37cecbe388a2f585893c4611436c) )
	ROM_LOAD16_BYTE( "ic91", 0x1a0000, 0x10000, CRC(49def6c8) SHA1(d8b2cc1993f0808553f87bf56fdbe47374576c5a) )
	ROM_LOAD16_BYTE( "ic68", 0x1c0001, 0x10000, CRC(8d684e53) SHA1(00e82ddaf875a7452ff978b7b7eb87a1a5a8fb64) )
	ROM_LOAD16_BYTE( "ic69", 0x1c0000, 0x10000, CRC(c47d32e2) SHA1(92b21f51abdd7950fb09d965b1d71b7bffac31ec) )
	ROM_LOAD16_BYTE( "ic88", 0x1e0001, 0x10000, CRC(9de140e1) SHA1(f1125e056a898a4fa519b49ae866c5c742e36bf7) )
	ROM_LOAD16_BYTE( "ic87", 0x1e0000, 0x10000, CRC(8172a991) SHA1(6d12b1533a19cb02613b473cc8ba73ece1f2a2fc) )

	ROM_REGION( 0x30000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "ic45", 0x10000, 0x10000, CRC(576b3a81) SHA1(b65356a3837ed3875634ab0cbcd61acce44f2bb9) )
	ROM_LOAD( "ic46", 0x20000, 0x10000, CRC(c84e8c84) SHA1(f57895bedb6152c30733e91e6f4795702a62ac3a) )
ROM_END

ROM_START( shdancrj )
	ROM_REGION( 0x080000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "sd12722b.bin", 0x000000, 0x40000, CRC(c00552a2) SHA1(74fddfe596bc00bc11c4a06e2103417e8fd334f6) )
	ROM_LOAD16_BYTE( "sd12721b.bin", 0x000001, 0x40000, CRC(653d351a) SHA1(1a03a154cb81a5a2f28c38aecdd6b5d107ea7ffa) )

	ROM_REGION( 0xc0000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "sd12712.bin",  0x00000, 0x40000, CRC(9bdabe3d) SHA1(4bb30fa2d4cdefe4a864cef7153b516bc5b02c42) )
	ROM_LOAD( "sd12713.bin",  0x40000, 0x40000, CRC(852d2b1c) SHA1(8e5bc83d45e48b621ea3016207f2028fe41701e6) )
	ROM_LOAD( "sd12714.bin",  0x80000, 0x40000, CRC(448226ce) SHA1(3060e4a43311069e2691d659c1e0c1a48edfeedb) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD16_BYTE( "sd12719.bin",  0x000001, 0x40000, CRC(d6888534) SHA1(2201f1921a68cf39e5a94b487c90e48d032d630f) )
	ROM_LOAD16_BYTE( "sd12726.bin",  0x000000, 0x40000, CRC(ff344945) SHA1(2743778c42f53321f9691d60bbf94ea8baf1382f) )
	ROM_LOAD16_BYTE( "sd12718.bin",  0x080001, 0x40000, CRC(ba2efc0c) SHA1(459a1a280f870c94aefb70127ed007cb090ed203) )
	ROM_LOAD16_BYTE( "sd12725.bin",  0x080000, 0x40000, CRC(268a0c17) SHA1(2756054fa3c3aed30a1fce5e41acb0ceaebe90b5) )
	ROM_LOAD16_BYTE( "sd12717.bin",  0x100001, 0x40000, CRC(c81cc4f8) SHA1(22f364e85057ceef533e051c8d0755b9691c5ec4) )
	ROM_LOAD16_BYTE( "sd12724.bin",  0x100000, 0x40000, CRC(0f4903dc) SHA1(851bd60e877c9e39be23dc1fde91efc9da513c29) )
	ROM_LOAD16_BYTE( "sd12716.bin",  0x180001, 0x40000, CRC(a870e629) SHA1(29f6633240f9737ec19e16100decc7aa045b2060) )
	ROM_LOAD16_BYTE( "sd12723.bin",  0x180000, 0x40000, CRC(c606cf90) SHA1(cb53ae9a6da1eb31c584173d1fbbd1c8539fb54c) )

	ROM_REGION( 0x70000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "sd12720.bin", 0x10000, 0x20000, CRC(7a0d8de1) SHA1(eca5e2104e5b3e772d083a718171234f06ea8a55) )
	ROM_LOAD( "sd12715.bin", 0x30000, 0x40000, CRC(07051a52) SHA1(d48658497f4a34665d3e051f893ff057c38925ae) )
ROM_END

/*****************************************************************************/

GAMEX(1990, astorm,   0,        astorm,   astorm,   astorm,   ROT0, "Sega",    "Alien Storm", GAME_NOT_WORKING )
GAMEX(1990, astorm2p, astorm,   astorm,   astorm,   astorm,   ROT0, "Sega",    "Alien Storm (2 Player)", GAME_NOT_WORKING )
GAME( 1990, astormbl, astorm,   astorm,   astorm,   astorm,   ROT0, "bootleg", "Alien Storm (bootleg)" )
GAMEX(1990, moonwalk, 0,        moonwalk, moonwalk, moonwalk, ROT0, "Sega",    "Michael Jackson's Moonwalker (Set 1)", GAME_NOT_WORKING )
GAMEX(1990, moonwlka, moonwalk, moonwalk, moonwalk, moonwalk, ROT0, "Sega",    "Michael Jackson's Moonwalker (Set 2)", GAME_NOT_WORKING )
GAME( 1990, moonwlkb, moonwalk, moonwalk, moonwalk, moonwalk, ROT0, "bootleg", "Michael Jackson's Moonwalker (bootleg)" )
GAME( 1989, shdancer, 0,        shdancer, shdancer, shdancer, ROT0, "Sega",    "Shadow Dancer (US)" )
GAMEX(1989, shdancbl, shdancer, shdancbl, shdancer, shdancbl, ROT0, "bootleg", "Shadow Dancer (bootleg)", GAME_NOT_WORKING )
GAME( 1989, shdancrj, shdancer, shdancrj, shdancer, shdancrj, ROT0, "Sega",    "Shadow Dancer (Japan)" )

GAMEX(19??, aceattac, 0,        shdancer, shdancer, shdancer, ROT0, "Sega", "Ace Attacker", GAME_NOT_WORKING )
GAMEX(1990, bloxeed,  0,        shdancer, shdancer, shdancer, ROT0, "Sega", "Bloxeed", GAME_NOT_WORKING )
GAMEX(19??, cltchitr, 0,        shdancer, shdancer, shdancer, ROT0, "Sega", "Clutch Hitter", GAME_NOT_WORKING )
GAMEX(19??, ddcrew,   0,        shdancer, shdancer, shdancer, ROT0, "Sega", "DD Crew", GAME_NOT_WORKING )
GAMEX(19??, lghost,   0,        shdancer, shdancer, shdancer, ROT0, "Sega", "Laser Ghost", GAME_NOT_WORKING )
