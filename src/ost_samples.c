/*********************************************************************

	OST sample support

*********************************************************************/

#include "driver.h"
#include "ost_samples.h"


/* ost configuration */
static int  sa_left;
static int  sa_right;
static int  sa_volume;
static bool sa_loop;
static bool sa_play;
static bool sa_stop;
static bool schedule_default_sound;


/* game specific - initialized by the driver */
bool     ddragon_playing = false;
int      ddragon_current_music;
int      ddragon_stage;
int      d_title_counter;

bool     ff_playing_final_fight = false;
bool     ff_provision_alt_song;
bool     ff_play_alternate_song;

bool     mk_playing_mortal_kombat = false;
bool     mk_playing_mortal_kombat_t = false;

bool     moonwalker_playing = false;
bool     moon_diddy;
int      mj_current_music;
int      mj_fade = 30; /* static value */

bool     nba_jam_playing = false;
bool     nba_jam_title_screen;
bool     nba_jam_select_screen;
bool     nba_jam_intermission;
bool     nba_jam_in_game;
bool     nba_jam_boot_up;
bool     nba_jam_playing_title_music;

bool     outrun_playing = false;
bool     outrun_start;
bool     outrun_diddy;
bool     outrun_title_diddy;
bool     outrun_title;
bool     outrun_lastwave;
int      outrun_start_counter;


/* ost functions */
static void ost_stop_samples(void);
static bool ost_mix_samples(void);
static void ost_default_config(void);


const char *const ddragon_sample_set_names[] =
{
	"*ddragon",
	"title-01",
	"title-02",
	"stage1-01",
	"stage1-02",
	"stage2-01",
	"stage2-02",
	"stage3-01",
	"stage3-02",
	"stage3-alt-01",
	"stage3-alt-02",
	"stage4-01",
	"stage4-02",
	"credits-01",
	"credits-02",
	"diddy-01",
	"diddy-02",
	"complete-01",
	"complete-02",
	"boss-01",
	"boss-02",
	"boss-alt-01",
	"boss-alt-02",
	"finalboss-01",
	"finalboss-02",
	0
};

const char *const ffight_sample_set_names[] =
{
	"*ffight",
	"track02-01",
	"track02-02",
	"track03-01",
	"track03-02",
	"track04-01",
	"track04-02",
	"track05-01",
	"track05-02",
	"track06-01",
	"track06-02",
	"track07-01",
	"track07-02",
	"track08-01",
	"track08-02",
	"track09-01",
	"track09-02",
	"track10-01",
	"track10-02",
	"track11-01",
	"track11-02",
	"track12-01",
	"track12-02",
	"track13-01",
	"track13-02",
	"track14-01",
	"track14-02",
	"track15-01",
	"track15-02",
	"track16-01",
	"track16-02",
	"track17-01",
	"track17-02",
	"track18-01",
	"track18-02",
	"track19-01",
	"track19-02",
	"track20-01",
	"track20-02",
	"track21-01",
	"track21-02",
	"track22-01",
	"track22-02",
	"track23-01",
	"track23-02",
	"track24-01",
	"track24-02",
	"track25-01",
	"track25-02",
	"track26-01",
	"track26-02",
	0
};

const char *const mk_sample_set_names[] =
{
	"*mk",
	"title-01",
	"title-02",
	"c-select-01",
	"c-select-02",
	"battle-menu-01",
	"battle-menu-02",
	"continue-01",
	"continue-02",
	"fatality-01",
	"fatality-02",
	"courtyard-01",
	"courtyard-02",
	"courtyard-end-01",
	"courtyard-end-02",
	"courtyard-finish-him-01",
	"courtyard-finish-him-02",
	"test-your-might-01",
	"test-your-might-02",
	"test-your-might-end-01",
	"test-your-might-end-02",
	"gameover-01",
	"gameover-02",
	"warriors-shrine-01",
	"warriors-shrine-02",
	"warriors-shrine-end-01",
	"warriors-shrine-end-02",
	"warriors-shrine-finish-him-01",
	"warriors-shrine-finish-him-02",
	"pit-01",
	"pit-02",
	"pit-end-01",
	"pit-end-02",
	"pit-finish-him-01",
	"pit-finish-him-02",
	"throne-room-01",
	"throne-room-02",
	"throne-room-end-01",
	"throne-room-end-02",
	"throne-room-finish-him-01",
	"throne-room-finish-him-02",
	"goros-lair-01",
	"goros-lair-02",
	"goros-lair-end-01",
	"goros-lair-end-02",
	"goros-lair-finish-him-01",
	"goros-lair-finish-him-02",
	"endurance-switch-01",
	"endurance-switch-02",
	"victory-01",
	"victory-02",
	"palace-gates-01",
	"palace-gates-02",
	"palace-gates-end-01",
	"palace-gates-end-02",
	"palace-gates-finish-him-01",
	"palace-gates-finish-him-02",
	0
};

const char *const moonwalker_sample_set_names[] =
{
	"*moonwalk",
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

const char *const nba_jam_sample_set_names[] =
{
	"*nbajam",
	"main-theme-01",
	"main-theme-02",
	"team-select-01",
	"team-select-02",
	"ingame-01", /* First & third quarter*/
	"ingame-02",
	"ingame-03", /* Second & fourth quarter*/
	"ingame-04",
	"intermission-01",
	"intermission-02",
	"halftime-01",
	"halftime-02",
	"theme-end-01",
	"theme-end-02",
	0
};

const char *const outrun_sample_set_names[] =
{
	"*outrun",
	"intro-01",
	"intro-02",
	"title-cut-01",
	"title-cut-02",
	"map-01",
	"map-02",
	"track1-01",
	"track1-02",
	"track3-01",
	"track3-02",
	"track4-01",
	"track4-02",
	0
};


struct Samplesinterface ost_ddragon =
{
	2,	/* 2 channels*/
	100, /* volume*/
	ddragon_sample_set_names
};

struct Samplesinterface ost_ffight =
{
	2,	/* 2 channels*/
	100, /* volume*/
	ffight_sample_set_names
};

struct Samplesinterface ost_mk =
{
	2,	/* 2 channels*/
	100, /* volume*/
	mk_sample_set_names
};

struct Samplesinterface ost_moonwalker =
{
	2,	/* 2 channels*/
	100, /* volume*/
	moonwalker_sample_set_names
};

struct Samplesinterface ost_nba_jam =
{
	2,	/* 2 channels*/
	100, /* volume*/
	nba_jam_sample_set_names
};

struct Samplesinterface ost_outrun =
{
	2,	/* 2 channels*/
	100, /* volume*/
	outrun_sample_set_names
};


static void ost_stop_samples(void)
{
  sample_stop(0);
  sample_stop(1);
}


static bool ost_mix_samples(void)
{
  bool ost_sample_is_playing = 1;

  /* Determine how we should mix these samples together.*/
  if(sample_playing(0) == 0 && sample_playing(1) == 1) { /* Right channel only. Lets make it play in both speakers.*/
    sample_set_stereo_volume(1, sa_volume, sa_volume);
  }

  else if(sample_playing(0) == 1 && sample_playing(1) == 0) { /* Left channel only. Lets make it play in both speakers.*/
    sample_set_stereo_volume(0, sa_volume, sa_volume);
  }

  else if(sample_playing(0) == 1 && sample_playing(1) == 1) { /* Both left and right channels. Lets make them play in there respective speakers.*/
    sample_set_stereo_volume(0, sa_volume, 0);
    sample_set_stereo_volume(1, 0, sa_volume);
  }

  else if(sample_playing(0) == 0 && sample_playing(1) == 0) { /* No sample playing, revert to the default sound.*/
    schedule_default_sound = true;
    ost_sample_is_playing = 0;
  }

  return ost_sample_is_playing;
}


static void ost_default_config(void)
{
  sa_volume = 100;
  sa_loop = 1;
  sa_play = false;
  sa_stop = false;
  schedule_default_sound = false;
}


bool generate_ost_sound_ddragon(int data)
{
	/* initialize game config */
	ost_default_config();
	sa_left = 0;
	sa_right = 1;
	sa_volume = 40;

	switch(data) {
		// Use for a counter flag on the title screen and stopping music.
		case 0xFF:
			// We are at the title screen.
			if(ddragon_current_music == 10 && ddragon_stage != 4) {
				// A coin has been inserted, lets stop the title music, about to start the first stage.
				if(d_title_counter > 5) {
					sa_stop = true;
					d_title_counter = 0;
				}
				else
					d_title_counter++;
			}
			else {
				sa_stop = true;
				ddragon_stage = 0;
				d_title_counter = 0;
			}
		break;

		// Title screen.
		case 0x1:
			if(ddragon_current_music != 10 && ddragon_stage != 4) {
				ddragon_current_music = 10;
				sa_play = true;
				sa_left = 0;
				sa_right = 1;
			}
			else if(ddragon_stage == 4 && ddragon_current_music != 15) { // Final boss fight.
				ddragon_current_music = 15;
				sa_play = true;
				sa_left = 22;
				sa_right = 23;
			}
			else
				return 0; /* do nothing */

			d_title_counter = 0;
		break;

		// Stage 1.
		case 0x9:
			ddragon_stage = 1;
			ddragon_current_music = 1;
			sa_play = true;
			sa_left = 2;
			sa_right = 3;
		break;

		// Stage 2.
		case 0x7:
			ddragon_stage = 2;
			ddragon_current_music = 2;
			sa_play = true;
			sa_left = 4;
			sa_right = 5;
		break;

		// Stage 3.
		case 0xA:
			if(ddragon_current_music != 3 && ddragon_stage != 3) {
				ddragon_stage = 3;
				ddragon_current_music = 3;
				sa_play = true;
				sa_left = 6;
				sa_right = 7;
			}
			else if(ddragon_stage == 3) {
				sa_play = true;
				sa_left = 8;
				sa_right = 9;
			}
			else
				return 0; /* do nothing */
		break;

		// Stage 4.
		case 0xD:
			ddragon_stage = 4;
			ddragon_current_music = 4;
			sa_play = true;
			sa_left = 10;
			sa_right = 11;
		break;

		// Credits.
		case 0x6:
			sa_volume = 100;
			sa_loop = 0;
			ddragon_stage = 5;
			ddragon_current_music = 5;
			sa_play = true;
			sa_left = 12;
			sa_right = 13;
		break;

		// Level finished.
		case 0xE:
			ddragon_current_music = 11;
			sa_loop = 0;
			sa_play = true;
			sa_left = 14;
			sa_right = 15;
		break;

		// Short diddy after boss battle.
		case 0xC:
			ddragon_current_music = 12;
			sa_loop = 0;
			sa_play = true;
			sa_left = 16;
			sa_right = 17;
		break;

		// Boss battle music.
		case 0x3:
			if(ddragon_stage == 3) {
				ddragon_current_music = 14;
				sa_play = true;
				sa_left = 20;
				sa_right = 21;
			}
			else {
				ddragon_current_music = 13;
				sa_play = true;
				sa_left = 18;
				sa_right = 19;
			}
		break;

		default:
			schedule_default_sound = true;
		break;
	}

	if(sa_play == true) {
		ost_stop_samples();

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);

		if( ost_mix_samples() == 0 ) {
			ddragon_current_music = 0;
		}
	}

	else if(sa_stop == true) {
		ddragon_current_music = 0;
		ost_stop_samples();

		// Now play the default sound.
		schedule_default_sound = true;
	}

	return schedule_default_sound;
}

bool generate_ost_sound_ffight(int data)
{
	/* initialize game config */
	ost_default_config();

	switch (data) {
		/* stage 1 upper level music*/
		case 0x40:
			/* Play the left channel.*/
			sample_start(0, 0, sa_loop);

			/* Play the right channel.*/
			sample_start(1, 1, sa_loop);
		break;

		/* stage #1: basement*/
		case 0x41:
			sample_start(0, 2, sa_loop);
			sample_start(1, 3, sa_loop);
		break;

		/* stage #2: subway intro*/
		case 0x42:
			/* play the normal version of the song unless playAlternateSong is true*/
			if (ff_play_alternate_song == false) {
				sample_start(0, 4, sa_loop);
				sample_start(1, 5, sa_loop);
			}
			else {
				sample_start(0, 40, sa_loop);
				sample_start(1, 41, sa_loop);
			}
		break;

		/* stage #2 exiting subway/alley*/
		case 0x43:
			sample_start(0, 6, sa_loop);
			sample_start(1, 7, sa_loop);
		break;

		/* double andore cage fight music*/
		case 0x44:
			sample_start(0, 8, sa_loop);
			sample_start(1, 9, sa_loop);
		break;

		/* bay area sea side theme*/
		case 0x45:
			sample_start(0, 10, sa_loop);
			sample_start(1, 11, sa_loop);

			/* we'll provision the alternate songs if they're not already*/
			if (ff_provision_alt_song == false) {
				ff_provision_alt_song = true;
			}
		break;

		/* bathroom music for bay area*/
		case 0x46:
			sample_start(0, 12, sa_loop);
			sample_start(1, 13, sa_loop);
		break;

		/* bay area post-bathroom ending/boss / final boss room entrance*/
		case 0x47:
			/* play the normal version of the song unless playAlternateSong is true*/
			if (ff_provision_alt_song == false) {
				sample_start(0, 14, sa_loop);
				sample_start(1, 15, sa_loop);
			}
			else {
				sample_start(0, 36, sa_loop);
				sample_start(1, 37, sa_loop);
			}
		break;

		/* bonus stage music*/
		case 0x4c:
			sample_start(0, 20, sa_loop);
			sample_start(1, 21, sa_loop);
		break;

		/* industrial music theme*/
		case 0x48:
			sample_start(0, 16, sa_loop);
			sample_start(1, 17, sa_loop);
		break;

		/* industrial zone elevator ride music*/
		case 0x49:
			sample_start(0, 18, sa_loop);
			sample_start(1, 19, sa_loop);
		break;

		/* game start ditty*/
		case 0x50:
			sa_loop = 0;
			sample_start(0, 22, sa_loop);
			sample_start(1, 23, sa_loop);

			/* when the game starts, we'll reset all the alternate songs*/
			ff_provision_alt_song = false;
			ff_play_alternate_song = false;
		break;

		/* post explosion ditty*/
		case 0x51:
			sa_loop = 0;
			sample_start(0, 24, sa_loop);
			sample_start(1, 25, sa_loop);
		break;

		/* opening cinematic song*/
		case 0x52:
			sa_loop = 0;
			sample_start(0, 46, sa_loop);
			sample_start(1, 47, sa_loop);
		break;

		/* continue/dynamite song*/
		case 0x53:
			sample_start(0, 32, sa_loop);
			sample_start(1, 33, sa_loop);
		break;

		/* homosexual cheesy ending music*/
		case 0x54:
			sample_start(0, 48, sa_loop);
			sample_start(1, 49, sa_loop);
		break;

		/* player select song*/
		case 0x55:
			sa_loop = 0;
			sample_start(0, 30, sa_loop);
			sample_start(1, 31, sa_loop);
		break;

		/* stage end/victory song*/
		case 0x57:
			sa_loop = 0;
			sample_start(0, 28, sa_loop);
			sample_start(1, 29, sa_loop);

			/* when we beat a stage after the alternate songs are provisioned, we know that we should be playing the alternate songs*/
			if (ff_provision_alt_song == true) {
				ff_play_alternate_song = true;
			}
		break;

		/* final stage clear ditty*/
		case 0x58:
			sa_loop = 0;
			sample_start(0, 26, sa_loop);
			sample_start(1, 27, sa_loop);

			ff_provision_alt_song = false;
			ff_play_alternate_song = false;
		break;

		default:
			schedule_default_sound = true;

			/* Lets stop the Final Fight sample music.*/
			if(data == 0xf0 || data == 0xf2 || data == 0xf7) {
				ost_stop_samples();
			}
		break;
	}

	if( ost_mix_samples() == 0 ) {
		/* samples not playing */
	}

	return schedule_default_sound;
}

bool generate_ost_sound_mk(int data)
{
	/* initialize game config */
	ost_default_config();
	sa_left = 0;
	sa_right = 1;

	switch (data) {
		/* Intro title screen diddy*/
		case 0xFD13:
			return 0; /* do nothing */
			break;

		/* Intro title screen diddy*/
		case 0xFF13:
			sa_play = true;
			sa_loop = 0;

			sa_left = 0; /* Left channel.*/
			sa_right = 1; /* Right channel/*/
			break;

		/* Second player joining diddy*/
		case 0xFD18:
			return 0; /* do nothing */
			break;

		/* Second player joining diddy*/
		case 0xFF18:
			sa_play = true;
			sa_loop = 0;

			sa_left = 0;
			sa_right = 1;
			break;

		/* Character selection screen.*/
		case 0xFD01:
			return 0; /* do nothing */
			break;

		/* Character selection screen.*/
		case 0xFF01:
			sa_play = true;
			sa_left = 2;
			sa_right = 3;
			break;

		/* Scrolling character map*/
		case 0xFD12:
			return 0; /* do nothing */
			break;

		/* Scrolling character map*/
		case 0xFF12:
			sa_play = true;

			sa_left = 4;
			sa_right = 5;
			break;

		/* Scrolling character map end*/
		case 0xFD1E:
			return 0; /* do nothing */
			break;

		/* Scrolling character map end*/
		case 0xFF1E:
			return 0; /* do nothing */
			break;

		/* Continue music*/
		case 0xFD06:
			return 0; /* do nothing */
			break;

		/* Continue music*/
		case 0xFF06:
			sa_play = true;

			sa_left = 6;
			sa_right = 7;
			break;

		/* Game over music*/
		case 0xFD02:
			return 0; /* do nothing */
			break;

		/* Game over music*/
		case 0xFF02:
			sa_play = true;

			sa_left = 20;
			sa_right = 21;
			break;

		/* Test your might music.*/
		case 0xFD19:
			return 0; /* do nothing */
			break;

		/* Test your might music.*/
		case 0xFF19:
			sa_play = true;

			sa_left = 16;
			sa_right = 17;
			break;

		/* Test your end (fail).*/
		case 0xFD1A:
			return 0; /* do nothing */
			break;

		/* Test your end (fail).*/
		case 0xFF1A:
			sa_play = true;
			sa_loop = 0;

			sa_left = 18;
			sa_right = 19;
			break;

		/* Fatality music*/
		case 0xFDEE:
			return 0; /* do nothing */
			break;

		/* Fatality music*/
		case 0xFFEE:
			sa_play = true;
			sa_loop = 0;

			sa_left = 8;
			sa_right = 9;
			break;

		/* Fatality music echo loop*/
		case 0xFDDE:
			return 0; /* do nothing */
			break;

		/* Fatality music echo loop*/
		case 0xFFDE:
			return 0; /* do nothing */
			break;

		/* Courtyard music*/
		case 0xFD03:
			return 0; /* do nothing */
			break;

		/* Courtyard music*/
		case 0xFF03:
			sa_play = true;

			sa_left = 10;
			sa_right = 11;
			break;

		/* Courtyard end music*/
		case 0xFD05:
			return 0; /* do nothing */
			break;

		/* Courtyard end music*/
		case 0xFF05:
			sa_play = true;
			sa_loop = 0;

			sa_left = 12;
			sa_right = 13;
			break;

		/* Courtyard finish him music*/
		case 0xFD04:
			return 0; /* do nothing */
			break;

		/* Courtyard finish him music*/
		case 0xFF04:
			sa_play = true;

			sa_left = 14;
			sa_right = 15;
			break;

		/* Warrior Shrine music*/
		case 0xFD0A:
			return 0; /* do nothing */
			break;

		/* Warrior Shrine music*/
		case 0xFF0A:
			sa_play = true;

			sa_left = 22;
			sa_right = 23;
			break;

		/* Warrior Shrine end music*/
		case 0xFD0C:
			return 0; /* do nothing */
			break;

		/* Warrior Shrine end music*/
		case 0xFF0C:
			sa_play = true;
			sa_loop = 0;

			sa_left = 24;
			sa_right = 25;
			break;

		/* Warrior Shrine finish him music*/
		case 0xFD0B:
			return 0; /* do nothing */
			break;

		/* Warrior Shrine finish him music*/
		case 0xFF0B:
			sa_play = true;

			sa_left = 26;
			sa_right = 27;
			break;

		/* The Pit music*/
		case 0xFD0D:
			return 0; /* do nothing */
			break;

		/* The Pit music*/
		case 0xFF0D:
			sa_play = true;

			sa_left = 28;
			sa_right = 29;
			break;

		/* The Pit end music*/
		case 0xFD0F:
			return 0; /* do nothing */
			break;

		/* The Pit end music*/
		case 0xFF0F:
			sa_play = true;
			sa_loop = 0;

			sa_left = 30;
			sa_right = 31;
			break;

		/* The Pit finish him music*/
		case 0xFD0E:
			return 0; /* do nothing */
			break;

		/* The Pit finish him music*/
		case 0xFF0E:
			sa_play = true;

			sa_left = 32;
			sa_right = 33;
			break;

		/* Throne Room music*/
		case 0xFD1B:
			return 0; /* do nothing */
			break;

		/* Throne Room music*/
		case 0xFF1B:
			sa_play = true;

			sa_left = 34;
			sa_right = 35;
			break;

		/* Throne Room end music*/
		case 0xFD1D:
			return 0; /* do nothing */
			break;

		/* Throne Room end music*/
		case 0xFF1D:
			sa_play = true;
			sa_loop = 0;

			sa_left = 36;
			sa_right = 37;
			break;

		/* Throne Room finish him music*/
		case 0xFD1C:
			return 0; /* do nothing */
			break;

		/* Throne Room finish him music*/
		case 0xFF1C:
			sa_play = true;

			sa_left = 38;
			sa_right = 39;
			break;

		/* Goro's Lair music*/
		case 0xFD14:
			return 0; /* do nothing */
			break;

		/* Goro's Lair music*/
		case 0xFF14:
			sa_play = true;

			sa_left = 40;
			sa_right = 41;
			break;

		/* Goro's Lair end music*/
		case 0xFD17:
			return 0; /* do nothing */
			break;

		/* Goro's Lair end music*/
		case 0xFF17:
			sa_play = true;
			sa_loop = 0;

			sa_left = 42;
			sa_right = 43;
			break;

		/* Goro's Lair finish him music*/
		case 0xFD16:
			return 0; /* do nothing */
			break;

		/* Goro's Lair finish him music*/
		case 0xFF16:
			sa_play = true;

			sa_left = 44;
			sa_right = 45;
			break;

		/* Endurance switch characters chime*/
		case 0xFD10:
			return 0; /* do nothing */
			break;

		/* Endurance switch characters chime*/
		case 0xFF10:
			sa_play = true;

			sa_left = 46;
			sa_right = 47;
			break;

		/* Victory music*/
		case 0xFD1F:
			return 0; /* do nothing */
			break;

		/* Victory music*/
		case 0xFF1F:
			sa_play = true;

			sa_left = 48;
			sa_right = 49;
			break;

		/* Palace gates music*/
		case 0xFD07:
			return 0; /* do nothing */
			break;

		/* Palace gates music*/
		case 0xFF07:
			sa_play = true;

			sa_left = 50;
			sa_right = 51;
			break;

		/* Palace Gates end music*/
		case 0xFD09:
			return 0; /* do nothing */
			break;

		/* Palace Gates end music*/
		case 0xFF09:
			sa_play = true;
			sa_loop = 0;

			sa_left = 52;
			sa_right = 53;
			break;

		/* Palace Gates finish him music*/
		case 0xFD08:
			return 0; /* do nothing */
			break;

		/* Palace Gates finish him music*/
		case 0xFF08:
			sa_play = true;
			sa_left = 54;
			sa_right = 55;
			break;

		default:
			schedule_default_sound = true;

			/* Time to stop the Mortal Kombat music samples.*/
			if(data == 0xFD00 || data == 0xFF00) {
				ost_stop_samples();
			}
			break;
	}

	if(sa_play == true) {
		ost_stop_samples();

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);

		if( ost_mix_samples() == 0 ) {
			/* samples not playing */
		}
	}
	else if(sa_stop == true) {
		ost_stop_samples();

		/* Now play the default sound.*/
		schedule_default_sound = true;
	}

	return schedule_default_sound;
}

bool generate_ost_sound_mk_tunit(int data)
{
	/* initialize game config */
	ost_default_config();
	sa_left = 0;
	sa_right = 1;

	switch (data) {
		/* Intro title screen diddy*/
		case 0x13:
			sa_play = true;
			sa_loop = 0;

			sa_left = 0; /* Left channel.*/
			sa_right = 1; /* Right channel/*/
			break;

		/* Second player joining diddy*/
		case 0x18:
			sa_play = true;
			sa_loop = 0;

			sa_left = 0;
			sa_right = 1;
			break;

		/* Character selection screen.*/
		case 0x1:
			sa_play = true;
			sa_left = 2;
			sa_right = 3;
			break;

		/* Scrolling character map*/
		case 0x12:
			sa_play = true;

			sa_left = 4;
			sa_right = 5;
			break;

		/* Scrolling character map end*/
		case 0x1E:
			return 0; /* do nothing */
			break;

		/* Continue music*/
		case 0x6:
			sa_play = true;

			sa_left = 6;
			sa_right = 7;
			break;

		/* Game over music*/
		case 0x2:
			sa_play = true;

			sa_left = 20;
			sa_right = 21;
			break;

		/* Test your might music.*/
		case 0x19:
			sa_play = true;

			sa_left = 16;
			sa_right = 17;
			break;

		/* Test your end (fail).*/
		case 0x1A:
			sa_play = true;
			sa_loop = 0;

			sa_left = 18;
			sa_right = 19;
			break;

		/* Fatality music*/
		case 0xEE:
			sa_play = true;
			sa_loop = 0;

			sa_left = 8;
			sa_right = 9;
			break;

		/* Fatality music echo loop*/
		case 0xDE:
			return 0; /* do nothing */
			break;

		/* Courtyard music*/
		case 0x3:
			sa_play = true;

			sa_left = 10;
			sa_right = 11;
			break;

		/* Courtyard end music*/
		case 0x5:
			sa_play = true;
			sa_loop = 0;

			sa_left = 12;
			sa_right = 13;
			break;

		/* Courtyard finish him music*/
		case 0x4:
			sa_play = true;

			sa_left = 14;
			sa_right = 15;
			break;

		/* Warrior Shrine music*/
		case 0xA:
			sa_play = true;

			sa_left = 22;
			sa_right = 23;
			break;

		/* Warrior Shrine end music*/
		case 0xC:
			sa_play = true;
			sa_loop = 0;

			sa_left = 24;
			sa_right = 25;
			break;

		/* Warrior Shrine finish him music*/
		case 0xB:
			sa_play = true;

			sa_left = 26;
			sa_right = 27;
			break;

		/* The Pit music*/
		case 0xD:
			sa_play = true;

			sa_left = 28;
			sa_right = 29;
			break;

		/* The Pit end music*/
		case 0xF:
			sa_play = true;
			sa_loop = 0;

			sa_left = 30;
			sa_right = 31;
			break;

		/* The Pit finish him music*/
		case 0xE:
			sa_play = true;

			sa_left = 32;
			sa_right = 33;
			break;

		/* Throne Room music*/
		case 0x1B:
			sa_play = true;

			sa_left = 34;
			sa_right = 35;
			break;

		/* Throne Room end music*/
		case 0x1D:
			sa_play = true;
			sa_loop = 0;

			sa_left = 36;
			sa_right = 37;
			break;

		/* Throne Room finish him music*/
		case 0x1C:
			sa_play = true;

			sa_left = 38;
			sa_right = 39;
			break;

		/* Goro's Lair music*/
		case 0x14:
			sa_play = true;

			sa_left = 40;
			sa_right = 41;
			break;

		/* Goro's Lair end music*/
		case 0x17:
			sa_play = true;
			sa_loop = 0;

			sa_left = 42;
			sa_right = 43;
			break;

		/* Goro's Lair finish him music*/
		case 0x16:
			sa_play = true;

			sa_left = 44;
			sa_right = 45;
			break;

		/* Endurance switch characters chime*/
		case 0x10:
			sa_play = true;

			sa_left = 46;
			sa_right = 47;
			break;

		/* Victory music*/
		case 0x1F:
			sa_play = true;

			sa_left = 48;
			sa_right = 49;
			break;

		/* Palace gates music*/
		case 0x7:
			sa_play = true;

			sa_left = 50;
			sa_right = 51;
			break;

		/* Palace Gates end music*/
		case 0x9:
			sa_play = true;
			sa_loop = 0;

			sa_left = 52;
			sa_right = 53;
			break;

		/* Palace Gates finish him music*/
		case 0x8:
			sa_play = true;
			sa_left = 54;
			sa_right = 55;
			break;

		default:
			schedule_default_sound = true;

			/* Time to stop the Mortal Kombat music samples.*/
			if(data == 0x0) {
				ost_stop_samples();
			}
			break;
	}

	if(sa_play == true) {
		ost_stop_samples();

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);

		if( ost_mix_samples() == 0 ) {
			/* samples not playing */
		}
	}
	else if(sa_stop == true) {
		ost_stop_samples();

		/* Now play the default sound.*/
		schedule_default_sound = true;
	}

	return schedule_default_sound;
}

bool generate_ost_sound_moonwalker(int data)
{
	/* initialize game config */
	ost_default_config();
	sa_left = 0;
	sa_right = 1;

	switch (data) {
		// Reset music. Title screen.
		case 0x0:
			sa_stop = true;
			mj_current_music = 0;
			moon_diddy = false;
			break;

		// Title screen stuff.
		case 0x85:
			if(mj_current_music != 85) {
				mj_current_music = 85;
				sa_loop = 0;
				sa_play = true;
				sa_left = 10;
				sa_right = 11;
			}
			else
				return 0; /* do nothing */
			break;

		// Title screen magic.
		case 0x86:
			if(mj_current_music == 85)
				return 0; /* do nothing */
			else {
				mj_current_music = 0;
				schedule_default_sound = true;
			}
			break;

		// Title screen magic.
		case 0x87:
			if(mj_current_music == 85)
				return 0; /* do nothing */
			else {
				mj_current_music = 0;
				schedule_default_sound = true;
			}
			break;

		// Stage 1 and Stage 5. Bad.
		case 0x81:
			if(mj_current_music != 81) {
				mj_current_music = 81;
				sa_play = true;
				sa_left = 0;
				sa_right = 1;
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 2. Smooth Criminal.
		case 0x82:
			if(mj_current_music != 82) {
				mj_current_music = 82;
				sa_play = true;
				sa_left = 2;
				sa_right = 3;
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 3. Beat It.
		case 0x84:
			if(mj_current_music != 83) {
				mj_current_music = 83;
				sa_play = true;
				sa_left = 4;
				sa_right = 5;
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 4. Thriller.
		case 0x8A:
			if(mj_current_music != 8) {
				mj_current_music = 8;
				sa_play = true;
				sa_left = 6;
				sa_right = 7;
			}
			else
				return 0; /* do nothing */
			break;

		// Ending. Billie Jean.
		case 0x89:
			if(mj_current_music != 89) {
				mj_current_music = 89;
				sa_play = true;
				sa_left = 8;
				sa_right = 9;
			}
			else
				return 0; /* do nothing */
			break;

		// First boss music
		case 0x8B:
			return 0; /* do nothing */
			break;

		// Second boss music
		case 0x83:
			return 0; /* do nothing */
			break;

		// Third boss music
		case 0x8E:
			return 0; /* do nothing */
			break;

		// Special move music diddy.
		case 0xFA:
			schedule_default_sound = true;
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
			schedule_default_sound = true;
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
			schedule_default_sound = true;
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
			schedule_default_sound = true;

			if(moon_diddy == true) {
				moon_diddy = false;

				// The special move is finished, lets return the level music volume back to 100%.
				if(sample_playing(0) == 0 && sample_playing(1) == 1) {
					sample_set_stereo_volume(1, sa_volume, sa_volume);
				}
				else if(sample_playing(0) == 1 && sample_playing(1) == 0) {
					sample_set_stereo_volume(0, sa_volume, sa_volume);
				}
				else if(sample_playing(0) == 1 && sample_playing(1) == 1) {
					sample_set_stereo_volume(0, sa_volume, 0);
					sample_set_stereo_volume(1, 0, sa_volume);
				}
			}
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	if(sa_play == true) {
		ost_stop_samples();

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);

		if( ost_mix_samples() == 0 ) {
			mj_current_music = 0;
		}
	}

	else if(sa_stop == true) {
		mj_current_music = 0;
		ost_stop_samples();

		// Now play the default sound.
		schedule_default_sound = true;
	}

	return schedule_default_sound;
}

bool generate_ost_sound_nba_jam(int data)
{
	/* initialize game config */
	ost_default_config();
	sa_left = 0;
	sa_right = 1;

	switch (data) {
		case 0x8C:
			return 0; /* do nothing */
			break;

		case 0x0:
			m_nba_start_counter++;

			/* Need to reset the intermission offset for game over.*/
			if(m_nba_last_offset == 0x23 || m_nba_last_offset == 0x29)
				nba_jam_intermission = false;

			if(nba_jam_boot_up == false) {
				if(m_nba_start_counter > 10)
					m_nba_start_counter = 4;

				if(m_nba_start_counter > 1 && nba_jam_in_game == false)
					nba_jam_title_screen = true;

				if(nba_jam_title_screen == true && nba_jam_playing_title_music == false && nba_jam_in_game == false && nba_jam_intermission == false) {
					sa_play = true;
					nba_jam_select_screen = false;
					nba_jam_intermission = false;
					nba_jam_playing_title_music = true;

					sa_left = 0; /* Left channel.*/
					sa_right = 1; /* Right channel.*/
				}
				else if(nba_jam_title_screen == true && nba_jam_playing_title_music == true && nba_jam_intermission == false)
					return 0; /* do nothing */
				else
					sa_stop = true;
			}
			else {
				if(m_nba_start_counter == 2) {
					nba_jam_boot_up = false;
					m_nba_start_counter = 0;
				}
			}
			break;

		/* Rev 2 calls this on title screen start. Rev 3 does not. Rev 3 does a extra 0 byte call, while Rev 2 does the FF byte instead.*/
		case 0xFF:
			nba_jam_intermission = false;

			if(m_nba_last_offset == 0) {
				m_nba_start_counter++;

				if(m_nba_start_counter > 10)
					m_nba_start_counter = 4;

				if(m_nba_start_counter > 1 && nba_jam_in_game == false && nba_jam_intermission == false)
					nba_jam_title_screen = true;

				if(nba_jam_title_screen == true && nba_jam_playing_title_music == false && nba_jam_in_game == false && nba_jam_intermission == false) {
					sa_play = true;
					nba_jam_select_screen = false;
					nba_jam_intermission = false;
					nba_jam_playing_title_music = true;

					sa_left = 0; /* Left channel.*/
					sa_right = 1; /* Right channel.*/
				}
				else if(nba_jam_title_screen == true && nba_jam_playing_title_music == true && nba_jam_intermission == false)
					return 0; /* do nothing */
				else
					sa_stop = true;
			}
			break;

		/* Doesn't seem to do anything? Appears after title screen demo game. Showing high scores. Replay the NBA Jam title music?*/
		case 0x7E:
			nba_jam_intermission = false;
			if(nba_jam_title_screen == true && nba_jam_playing_title_music == false && nba_jam_in_game == false) {
				sa_play = true;

				sa_left = 0;
				sa_right = 1;
			}
			break;

		/* Team select.*/
		case 0x1:
			sa_play = true;
			nba_jam_title_screen = false;
			nba_jam_select_screen = true;
			nba_jam_intermission = false;
			nba_jam_in_game = true;
			nba_jam_playing_title_music = false;

			sa_left = 2;
			sa_right = 3;
			break;

		/* 1st quarter.*/
		case 0x2:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 4;
			sa_right = 5;
			break;

		/* 2nd quarter.*/
		case 0x6:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 6;
			sa_right = 7;
			break;

		/* Half time report.*/
		case 0x4:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = true;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;

			sa_left = 10;
			sa_right = 11;
			break;

		/* 3rd quarter.*/
		case 0x7:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 4;
			sa_right = 5;
			break;

		/* 4th quarter.*/
		case 0x8:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 6;
			sa_right = 7;
			break;

		/* Game over and back to title screen. This plays the team select music. We will do nothing and reflag the title screen music to start playback soon after.*/
		case 0x9:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;
			return 0; /* do nothing */
			break;

		/* Game stats after playing a full game.*/
		case 0x3:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;

			sa_left = 12;
			sa_right = 13;
			break;

		/* Intermission.*/
		case 0xA:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = true;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;

			sa_left = 8;
			sa_right = 9;
			break;

		/* Overtime.*/
		case 0xB:
			sa_play = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 6;
			sa_right = 7;
			break;

		/* NBA Jam halftime report.*/
		case 0x71:
			return 0; /* do nothing */
			break;

		/* Altitude with a attitude.*/
		case 0xCC:
			return 0; /* do nothing */
			break;

		/* Welcome to NBA Jam.*/
		case 0xCB:
			if(nba_jam_select_screen == true)
				return 0; /* do nothing */
			else
				schedule_default_sound = true;
			break;

		default:
			schedule_default_sound = true;

			/* Time to stop the NBA Jam music samples.*/
			if(data == 0x0 && nba_jam_title_screen == false) {
				ost_stop_samples();
			}
			break;
	}

	if(sa_play == true) {
		ost_stop_samples();

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);

		if( ost_mix_samples() == 0 ) {
			/* samples not playing */
		}
	}

	else if(sa_stop == true) {
		ost_stop_samples();

		/* Now play the default sound.*/
		schedule_default_sound = true;
	}

	m_nba_last_offset = data;

	return schedule_default_sound;
}

bool generate_ost_sound_outrun(int data)
{
	/* initialize game config */
	ost_default_config();
	sa_left = 0;
	sa_right = 1;

	if(outrun_start == true) {
		sa_play = true;
		sa_left = 0;
		sa_right = 1;
		outrun_start = false;
		outrun_diddy = true;
		outrun_lastwave = false;
	}

	switch (data) {
		case 0x0:
			if(outrun_diddy == true) {
				outrun_start_counter++;

				if(outrun_start_counter == 2) {
					sa_play = true;
					sa_left = 2;
					sa_right = 3;
					outrun_diddy = false;
					outrun_title_diddy = true;
					outrun_lastwave = false;
					sa_loop = 0;
				}
			}
			else if(outrun_title_diddy == true) {
				outrun_diddy = false;
				outrun_start_counter++;

				if(outrun_start_counter > 5)
					outrun_title_diddy = false;
			}
			else if(outrun_diddy == false && outrun_title_diddy == false && outrun_title == false) {
				sa_play = true;

				outrun_diddy = true;
				sa_left = 0;
				sa_right = 1;
				outrun_start_counter = 1;

				outrun_lastwave = false;
			}
			break;

		// 2. --> Passing Breeze
		case 0x81:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			sa_play = true;
			sa_left = 8;
			sa_right = 9;
			break;

		// 1. --> Splash wave
		case 0x82:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			sa_play = true;
			sa_left = 10;
			sa_right = 11;
			break;

		// 3 --> Magical Sound Shower
		case 0x85:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			sa_play = true;
			sa_left = 6;
			sa_right = 7;
			break;

		// --> Last Wave
		case 0x93:
			if(outrun_lastwave == false) {
				outrun_diddy = false;
				outrun_title_diddy = false;
				outrun_lastwave = true;
				sa_play = true;
				sa_left = 4;
				sa_right = 5;
			}
			else
				return 0; /* do nothing */
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	if(sa_play == true) {
		ost_stop_samples();

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);

		if( ost_mix_samples() == 0 ) {
			/* samples not playing */
		}
	}

	else if(sa_stop == true) {
		ost_stop_samples();

		// Now play the default sound.
		schedule_default_sound = true;
	}

	return schedule_default_sound;
}
