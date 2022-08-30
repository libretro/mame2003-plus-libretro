/*********************************************************************

	OST sample support

*********************************************************************/

#include "driver.h"
#include "ost_samples.h"


/* ost configuration */
static int  sa_volume;
static bool schedule_default_sound;


/* game specific variables */
int      ost_support = OST_SUPPORT_DISABLED;

int      ddragon_current_music;
int      ddragon_stage;
int      d_title_counter;

bool     ff_alternate_song_1;
bool     ff_alternate_song_2;

bool     moon_diddy;
int      mj_current_music;

bool     nba_jam_title_screen;
bool     nba_jam_select_screen;
bool     nba_jam_intermission;
bool     nba_jam_in_game;
bool     nba_jam_boot_up;
bool     nba_jam_playing_title_music;
int      m_nba_last_offset;
int      m_nba_start_counter;

bool     outrun_start;
bool     outrun_diddy;
bool     outrun_title_diddy;
bool     outrun_lastwave;
int      outrun_start_counter;

bool     sf1_start;
bool     sf1_won;

bool     fadingMusic;


/* ost functions */
static void ost_start_samples(int sa_left, int sa_right, int sa_loop);
static void ost_stop_samples(void);
static void ost_mix_samples(void);


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

const char *const ikari_sample_set_names[] =
{
	"*ikari",
	"title-01",
	"title-02",
	"credit-01",
	"credit-02",
	"landing-01",
	"landing-02",
	"theme-01",
	"theme-02",
	"gate-01",
	"gate-02",
	"victory-01",
	"victory-02",
	"glory-01",
	"glory-02",
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

const char *const sf2_sample_set_names[] =
{
	"*sf2",
	"ryuslow-01",
	"ryuslow-02",
	"ryufast-01",
	"ryufast-02",
	"blankaslow-01",
	"blankaslow-02",
	"blankafast-01",
	"blankafast-02",
	"chunlislow-01",
	"chunlislow-02",
	"chunlifast-01",
	"chunlifast-02",
	"ehondaslow-01",
	"ehondaslow-02",
	"ehondafast-01",
	"ehondafast-02",
	"guileslow-01",
	"guileslow-02",
	"guilefast-01",
	"guilefast-02",
	"dhalsimslow-01",
	"dhalsimslow-02",
	"dhalsimfast-01",
	"dhalsimfast-02",
	"balrogslow-01",
	"balrogslow-02",
	"balrogfast-01",
	"balrogfast-02",
	"sagatslow-01",
	"sagatslow-02",
	"sagatfast-01",
	"sagatfast-02",
	"mbisonslow-01",
	"mbisonslow-02",
	"mbisonfast-01",
	"mbisonfast-02",
	"versus-01",
	"versus-02",
	"endfight-01",
	"endfight-02",
	"continue-01",
	"continue-02",
	"highscore-01",
	"highscore-02",
	"intro-01",
	"intro-02",
	"playerjoin-01",
	"playerjoin-02",
	"playerselect-01",
	"playerselect-02",
	"gameover-01",
	"gameover-02",
	"kenslow-01",
	"kenslow-02",
	"kenfast-01",
	"kenfast-02",
	"zangiefslow-01",
	"zangiefslow-02",
	"zangieffast-01",
	"zangieffast-02",
	"vegaslow-01",
	"vegaslow-02",
	"vegafast-01",
	"vegafast-02",
	"bonusstage-01",
	"bonusstage-02",
	"mbisonending-01",
	"mbisonending-02",
	"kenendinga-01",
	"kenendinga-02",
	"kenendingb-01",
	"kenendingb-02",
	"ehondaending-01",
	"ehondaending-02",
	"blankaending-01",
	"blankaending-02",
	"guileending-01",
	"guileending-02",
	"zangiefending-01",
	"zangiefending-02",
	"specialending-01",
	"specialending-02",
	"ryuending-01",
	"ryuending-02",
	"dhalsimending-01",
	"dhalsimending-02",
	"chunliendinga-01",
	"chunliendinga-02",
	"chunliendingb-01",
	"chunliendingb-02",
	0
};

const char *const sf1_sample_set_names[] =
{
	"*sf1",
	"retsu-01",
	"retsu-02",
	"geki-01",
	"geki-02",
	"joe-01",
	"joe-02",
	"mike-01",
	"mike-02",
	"birdie-01",
	"birdie-02",
	"eagle-01",
	"eagle-02",
	"lee-01",
	"lee-02",
	"gen-01",
	"gen-02",
	"adon-01",
	"adon-02",
	"sagat-01",
	"sagat-02",
	"ending-01",
	"ending-02",
	"vs-01",
	"vs-02",
	"select-01",
	"select-02",
	"credits-01",
	"credits-02",
	"bonus1-01",
	"bonus1-02",
	"bonus2-01",
	"bonus2-02",
	"score-01",
	"score-02",
	"theend-01",
	"theend-02",
	"won-01",
	"won-02",
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

struct Samplesinterface ost_ikari =
{
	2,	/* 2 channels*/
	100, /* volume*/
	ikari_sample_set_names
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

struct Samplesinterface ost_sf1 =
{
	2,	/* 2 channels*/
	100, /* volume*/
	sf1_sample_set_names
};

struct Samplesinterface ost_sf2 =
{
	2,	/* 2 channels*/
	100, /* volume*/
	sf2_sample_set_names
};


bool ost_support_enabled(int ost)
{
  if (!options.use_samples) return false;
  if (!options.use_alt_sound) return false;
  if (!options.content_flags[CONTENT_ALT_SOUND]) return false;

  if (ost_support == ost) return true;

  return false;
}


void install_ost_support(struct InternalMachineDriver *machine, int ost)
{
  /* set */
  ost_support = ost;

  switch(ost)
  {
    case OST_SUPPORT_DDRAGON:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_ddragon)
      ddragon_current_music = 0;
      ddragon_stage = 0;
      d_title_counter = 0;
      break;

    case OST_SUPPORT_FFIGHT:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_ffight)
      ff_alternate_song_1 = false;
      ff_alternate_song_2 = false;
      break;

    case OST_SUPPORT_MK:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_mk)
      /* no settings */
      break;

    case OST_SUPPORT_MK_T:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_mk)
      /* no settings */
      break;

    case OST_SUPPORT_MOONWALKER:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_moonwalker)
      moon_diddy = false;
      mj_current_music = 0;
      break;

    case OST_SUPPORT_NBA_JAM:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_nba_jam)
      nba_jam_title_screen = false;
      nba_jam_select_screen = false;
      nba_jam_intermission = false;
      nba_jam_in_game = false;
      nba_jam_boot_up	= true;
      nba_jam_playing_title_music = false;
      m_nba_last_offset = 0;
      m_nba_start_counter = 0;
      break;

    case OST_SUPPORT_OUTRUN:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_outrun)
      outrun_start = true;
      outrun_diddy = false;
      outrun_title_diddy = false;
      outrun_lastwave = false;
      outrun_start_counter = 0;
      break;

    case OST_SUPPORT_SF1:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_sf1)
      sf1_start = true;
      sf1_won = false;
      break;

    case OST_SUPPORT_SF2:
      MDRV_SOUND_ADD_TAG("OST Samples", SAMPLES, ost_sf2)
      fadingMusic = false;
      break;
  }
}


static void ost_start_samples(int sa_left, int sa_right, int sa_loop)
{
  ost_stop_samples();

  sample_start(0, sa_left, sa_loop);
  sample_start(1, sa_right, sa_loop);
}


static void ost_stop_samples(void)
{
  sample_stop(0);
  sample_stop(1);
}


static void ost_mix_samples(void)
{
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

    ddragon_current_music = 0;
    mj_current_music = 0;
  }
}


void ost_fade_volume(void)
{
  static bool allow_fade = true;
  /*usrintf_showmessage("fadingMusic:%i  volume:%i", fadingMusic, sa_volume);*/

  if(fadingMusic == false) return;

  /* slow volume fading */
  allow_fade = (allow_fade) ? 0:1;
  if(allow_fade && sa_volume > 0) sa_volume -= 1;

  /* end fading */
  if(sa_volume == 0) fadingMusic = false;

  ost_mix_samples();
}


bool generate_ost_sound_ddragon(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 40;

	switch(data) {
		// Use for a counter flag on the title screen and stopping music.
		case 0xFF:
			// We are at the title screen.
			if(ddragon_current_music == 10 && ddragon_stage != 4) {
				// A coin has been inserted, lets stop the title music, about to start the first stage.
				if(d_title_counter > 5) {
					ost_stop_samples();
					d_title_counter = 0;
				}
				else
					d_title_counter++;
			}
			else {
				ost_stop_samples();
				ddragon_stage = 0;
				d_title_counter = 0;
			}
			break;

		// Title screen.
		case 0x1:
			if(ddragon_current_music != 10 && ddragon_stage != 4) {
				ddragon_current_music = 10;
				ost_start_samples(0, 1, 1);
			}
			else if(ddragon_stage == 4 && ddragon_current_music != 15) { // Final boss fight.
				ddragon_current_music = 15;
				ost_start_samples(22, 23, 1);
			}
			else
				return 0; /* do nothing */

			d_title_counter = 0;
			break;

		// Stage 1.
		case 0x9:
			ddragon_stage = 1;
			ddragon_current_music = 1;
			ost_start_samples(2, 3, 1);
			break;

		// Stage 2.
		case 0x7:
			ddragon_stage = 2;
			ddragon_current_music = 2;
			ost_start_samples(4, 5, 1);
			break;

		// Stage 3.
		case 0xA:
			if(ddragon_current_music != 3 && ddragon_stage != 3) {
				ddragon_stage = 3;
				ddragon_current_music = 3;
				ost_start_samples(6, 7, 1);
			}
			else if(ddragon_stage == 3) {
				ost_start_samples(8, 9, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 4.
		case 0xD:
			ddragon_stage = 4;
			ddragon_current_music = 4;
			ost_start_samples(10, 11, 1);
			break;

		// Credits.
		case 0x6:
			sa_volume = 100;
			ddragon_stage = 5;
			ddragon_current_music = 5;
			ost_start_samples(12, 13, 0);
			break;

		// Level finished.
		case 0xE:
			ddragon_current_music = 11;
			ost_start_samples(14, 15, 0);
			break;

		// Short diddy after boss battle.
		case 0xC:
			ddragon_current_music = 12;
			ost_start_samples(16, 17, 0);
			break;

		// Boss battle music.
		case 0x3:
			if(ddragon_stage == 3) {
				ddragon_current_music = 14;
				ost_start_samples(20, 21, 1);
			}
			else {
				ddragon_current_music = 13;
				ost_start_samples(18, 19, 1);
			}
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_ffight(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

	switch (data) {
		/* stage 1 upper level music*/
		case 0x40:
			ost_start_samples(0, 1, 1);
			break;

		/* stage #1: basement*/
		case 0x41:
			ost_start_samples(2, 3, 1);
			break;

		/* stage #2: subway intro*/
		case 0x42:
			if (ff_alternate_song_2 == false)
				ost_start_samples(4, 5, 1); /* play normal */
			else
				ost_start_samples(40, 41, 1); /* play alternate */
			break;

		/* stage #2 exiting subway/alley*/
		case 0x43:
			ost_start_samples(6, 7, 1);
			break;

		/* double andore cage fight music*/
		case 0x44:
			ost_start_samples(8, 9, 1);
			break;

		/* bay area sea side theme*/
		case 0x45:
			ost_start_samples(10, 11, 1);

			/* enable 1st alternate song */
			ff_alternate_song_1 = true;
			break;

		/* bathroom music for bay area*/
		case 0x46:
			ost_start_samples(12, 13, 1);
			break;

		/* bay area post-bathroom ending/boss / final boss room entrance*/
		case 0x47:
			if (ff_alternate_song_1 == false)
				ost_start_samples(14, 15, 1); /* play normal */
			else
				ost_start_samples(36, 37, 1); /* play alternate */
			break;

		/* bonus stage music*/
		case 0x4c:
			ost_start_samples(20, 21, 1);
			break;

		/* industrial music theme*/
		case 0x48:
			ost_start_samples(16, 17, 1);
			break;

		/* industrial zone elevator ride music*/
		case 0x49:
			ost_start_samples(18, 19, 1);
			break;

		/* game start ditty*/
		case 0x50:
			ost_start_samples(22, 23, 0);

			/* reset alternate songs */
			ff_alternate_song_1 = false;
			ff_alternate_song_2 = false;
			break;

		/* post explosion ditty*/
		case 0x51:
			ost_start_samples(24, 25, 0);
			break;

		/* opening cinematic song*/
		case 0x52:
			ost_start_samples(46, 47, 0);
			break;

		/* continue/dynamite song*/
		case 0x53:
			ost_start_samples(32, 33, 1);
			break;

		/* homosexual cheesy ending music*/
		case 0x54:
			ost_start_samples(48, 49, 1);
			break;

		/* player select song*/
		case 0x55:
			ost_start_samples(30, 31, 0);
			break;

		/* stage end/victory song*/
		case 0x57:
			ost_start_samples(28, 29, 0);

			/* enable 2nd alternate song if 1st has been enabled */
			if (ff_alternate_song_1 == true)
				ff_alternate_song_2 = true;
			break;

		/* final stage clear ditty*/
		case 0x58:
			ost_start_samples(26, 27, 0);
			ff_alternate_song_1 = false;
			ff_alternate_song_2 = false;
			break;

		/* Lets stop the Final Fight sample music.*/
		case 0xf0:
		case 0xf2:
		case 0xf7:
			ost_stop_samples();
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_ikari(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;
}

bool generate_ost_sound_mk(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

	switch (data) {
		/* Intro title screen diddy*/
		case 0xFD13:
			return 0; /* do nothing */
			break;

		/* Intro title screen diddy*/
		case 0xFF13:
			ost_start_samples(0, 1, 0);
			break;

		/* Second player joining diddy*/
		case 0xFD18:
			return 0; /* do nothing */
			break;

		/* Second player joining diddy*/
		case 0xFF18:
			ost_start_samples(0, 1, 0);
			break;

		/* Character selection screen.*/
		case 0xFD01:
			return 0; /* do nothing */
			break;

		/* Character selection screen.*/
		case 0xFF01:
			ost_start_samples(2, 3, 1);
			break;

		/* Scrolling character map*/
		case 0xFD12:
			return 0; /* do nothing */
			break;

		/* Scrolling character map*/
		case 0xFF12:
			ost_start_samples(4, 5, 1);
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
			ost_start_samples(6, 7, 1);
			break;

		/* Game over music*/
		case 0xFD02:
			return 0; /* do nothing */
			break;

		/* Game over music*/
		case 0xFF02:
			ost_start_samples(20, 21, 1);
			break;

		/* Test your might music.*/
		case 0xFD19:
			return 0; /* do nothing */
			break;

		/* Test your might music.*/
		case 0xFF19:
			ost_start_samples(16, 17, 1);
			break;

		/* Test your end (fail).*/
		case 0xFD1A:
			return 0; /* do nothing */
			break;

		/* Test your end (fail).*/
		case 0xFF1A:
			ost_start_samples(18, 19, 0);
			break;

		/* Fatality music*/
		case 0xFDEE:
			return 0; /* do nothing */
			break;

		/* Fatality music*/
		case 0xFFEE:
			ost_start_samples(8, 9, 0);
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
			ost_start_samples(10, 11, 1);
			break;

		/* Courtyard end music*/
		case 0xFD05:
			return 0; /* do nothing */
			break;

		/* Courtyard end music*/
		case 0xFF05:
			ost_start_samples(12, 13, 0);
			break;

		/* Courtyard finish him music*/
		case 0xFD04:
			return 0; /* do nothing */
			break;

		/* Courtyard finish him music*/
		case 0xFF04:
			ost_start_samples(14, 15, 1);
			break;

		/* Warrior Shrine music*/
		case 0xFD0A:
			return 0; /* do nothing */
			break;

		/* Warrior Shrine music*/
		case 0xFF0A:
			ost_start_samples(22, 23, 1);
			break;

		/* Warrior Shrine end music*/
		case 0xFD0C:
			return 0; /* do nothing */
			break;

		/* Warrior Shrine end music*/
		case 0xFF0C:
			ost_start_samples(24, 25, 0);
			break;

		/* Warrior Shrine finish him music*/
		case 0xFD0B:
			return 0; /* do nothing */
			break;

		/* Warrior Shrine finish him music*/
		case 0xFF0B:
			ost_start_samples(26, 27, 1);
			break;

		/* The Pit music*/
		case 0xFD0D:
			return 0; /* do nothing */
			break;

		/* The Pit music*/
		case 0xFF0D:
			ost_start_samples(28, 29, 1);
			break;

		/* The Pit end music*/
		case 0xFD0F:
			return 0; /* do nothing */
			break;

		/* The Pit end music*/
		case 0xFF0F:
			ost_start_samples(30, 31, 0);
			break;

		/* The Pit finish him music*/
		case 0xFD0E:
			return 0; /* do nothing */
			break;

		/* The Pit finish him music*/
		case 0xFF0E:
			ost_start_samples(32, 33, 1);
			break;

		/* Throne Room music*/
		case 0xFD1B:
			return 0; /* do nothing */
			break;

		/* Throne Room music*/
		case 0xFF1B:
			ost_start_samples(34, 35, 1);
			break;

		/* Throne Room end music*/
		case 0xFD1D:
			return 0; /* do nothing */
			break;

		/* Throne Room end music*/
		case 0xFF1D:
			ost_start_samples(36, 37, 0);
			break;

		/* Throne Room finish him music*/
		case 0xFD1C:
			return 0; /* do nothing */
			break;

		/* Throne Room finish him music*/
		case 0xFF1C:
			ost_start_samples(38, 39, 1);
			break;

		/* Goro's Lair music*/
		case 0xFD14:
			return 0; /* do nothing */
			break;

		/* Goro's Lair music*/
		case 0xFF14:
			ost_start_samples(40, 41, 1);
			break;

		/* Goro's Lair end music*/
		case 0xFD17:
			return 0; /* do nothing */
			break;

		/* Goro's Lair end music*/
		case 0xFF17:
			ost_start_samples(42, 43, 0);
			break;

		/* Goro's Lair finish him music*/
		case 0xFD16:
			return 0; /* do nothing */
			break;

		/* Goro's Lair finish him music*/
		case 0xFF16:
			ost_start_samples(44, 45, 1);
			break;

		/* Endurance switch characters chime*/
		case 0xFD10:
			return 0; /* do nothing */
			break;

		/* Endurance switch characters chime*/
		case 0xFF10:
			ost_start_samples(46, 47, 1);
			break;

		/* Victory music*/
		case 0xFD1F:
			return 0; /* do nothing */
			break;

		/* Victory music*/
		case 0xFF1F:
			ost_start_samples(48, 49, 1);
			break;

		/* Palace gates music*/
		case 0xFD07:
			return 0; /* do nothing */
			break;

		/* Palace gates music*/
		case 0xFF07:
			ost_start_samples(50, 51, 1);
			break;

		/* Palace Gates end music*/
		case 0xFD09:
			return 0; /* do nothing */
			break;

		/* Palace Gates end music*/
		case 0xFF09:
			ost_start_samples(52, 53, 0);
			break;

		/* Palace Gates finish him music*/
		case 0xFD08:
			return 0; /* do nothing */
			break;

		/* Palace Gates finish him music*/
		case 0xFF08:
			ost_start_samples(54, 55, 1);
			break;

		/* Time to stop the Mortal Kombat music samples.*/
		case 0xFD00:
		case 0xFF00:
			ost_stop_samples();
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_mk_tunit(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

	switch (data) {
		/* Intro title screen diddy*/
		case 0x13:
			ost_start_samples(0, 1, 0);
			break;

		/* Second player joining diddy*/
		case 0x18:
			ost_start_samples(0, 1, 0);
			break;

		/* Character selection screen.*/
		case 0x1:
			ost_start_samples(2, 3, 1);
			break;

		/* Scrolling character map*/
		case 0x12:
			ost_start_samples(4, 5, 1);
			break;

		/* Scrolling character map end*/
		case 0x1E:
			return 0; /* do nothing */
			break;

		/* Continue music*/
		case 0x6:
			ost_start_samples(6, 7, 1);
			break;

		/* Game over music*/
		case 0x2:
			ost_start_samples(20, 21, 1);
			break;

		/* Test your might music.*/
		case 0x19:
			ost_start_samples(16, 17, 1);
			break;

		/* Test your end (fail).*/
		case 0x1A:
			ost_start_samples(18, 19, 0);
			break;

		/* Fatality music*/
		case 0xEE:
			ost_start_samples(8, 9, 0);
			break;

		/* Fatality music echo loop*/
		case 0xDE:
			return 0; /* do nothing */
			break;

		/* Courtyard music*/
		case 0x3:
			ost_start_samples(10, 11, 1);
			break;

		/* Courtyard end music*/
		case 0x5:
			ost_start_samples(12, 13, 0);
			break;

		/* Courtyard finish him music*/
		case 0x4:
			ost_start_samples(14, 15, 1);
			break;

		/* Warrior Shrine music*/
		case 0xA:
			ost_start_samples(22, 23, 1);
			break;

		/* Warrior Shrine end music*/
		case 0xC:
			ost_start_samples(24, 25, 0);
			break;

		/* Warrior Shrine finish him music*/
		case 0xB:
			ost_start_samples(26, 27, 1);
			break;

		/* The Pit music*/
		case 0xD:
			ost_start_samples(28, 29, 1);
			break;

		/* The Pit end music*/
		case 0xF:
			ost_start_samples(30, 31, 0);
			break;

		/* The Pit finish him music*/
		case 0xE:
			ost_start_samples(32, 33, 1);
			break;

		/* Throne Room music*/
		case 0x1B:
			ost_start_samples(34, 35, 1);
			break;

		/* Throne Room end music*/
		case 0x1D:
			ost_start_samples(36, 37, 0);
			break;

		/* Throne Room finish him music*/
		case 0x1C:
			ost_start_samples(38, 39, 1);
			break;

		/* Goro's Lair music*/
		case 0x14:
			ost_start_samples(40, 41, 1);
			break;

		/* Goro's Lair end music*/
		case 0x17:
			ost_start_samples(42, 43, 0);
			break;

		/* Goro's Lair finish him music*/
		case 0x16:
			ost_start_samples(44, 45, 1);
			break;

		/* Endurance switch characters chime*/
		case 0x10:
			ost_start_samples(46, 47, 1);
			break;

		/* Victory music*/
		case 0x1F:
			ost_start_samples(48, 49, 1);
			break;

		/* Palace gates music*/
		case 0x7:
			ost_start_samples(50, 51, 1);
			break;

		/* Palace Gates end music*/
		case 0x9:
			ost_start_samples(52, 53, 0);
			break;

		/* Palace Gates finish him music*/
		case 0x8:
			ost_start_samples(54, 55, 1);
			break;

		/* Time to stop the Mortal Kombat music samples.*/
		case 0x0:
			ost_stop_samples();
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_moonwalker(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

	switch (data) {
		// Reset music. Title screen.
		case 0x0:
			ost_stop_samples();
			moon_diddy = false;
			break;

		// Title screen stuff.
		case 0x85:
			if(mj_current_music != 85) {
				mj_current_music = 85;
				ost_start_samples(10, 11, 0);
			}
			else
				return 0; /* do nothing */
			break;

		// Title screen magic.
		case 0x86:
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
				ost_start_samples(0, 1, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 2. Smooth Criminal.
		case 0x82:
			if(mj_current_music != 82) {
				mj_current_music = 82;
				ost_start_samples(2, 3, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 3. Beat It.
		case 0x84:
			if(mj_current_music != 83) {
				mj_current_music = 83;
				ost_start_samples(4, 5, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// Stage 4. Thriller.
		case 0x8A:
			if(mj_current_music != 8) {
				mj_current_music = 8;
				ost_start_samples(6, 7, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// Ending. Billie Jean.
		case 0x89:
			if(mj_current_music != 89) {
				mj_current_music = 89;
				ost_start_samples(8, 9, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// Boss music
		case 0x8B:  /* First */
		case 0x83:  /* Second */
		case 0x8E:  /* Third */
			return 0; /* do nothing */
			break;

		// Special move music diddy.
		case 0xFA:
		case 0xFB:
		case 0xF6:
			mj_current_music = 0;
			schedule_default_sound = true;
			moon_diddy = true;
			break;

		// Special move "owww" sound effect. This plays after the special move has always finished.
		case 0xC3:
			mj_current_music = 0;
			schedule_default_sound = true;
			moon_diddy = false; /* return volume back to 100%. */
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	/* While the special move is playing, lower volume to 30%. */
	if(moon_diddy) sa_volume = 30;

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_nba_jam(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

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
					nba_jam_select_screen = false;
					nba_jam_intermission = false;
					nba_jam_playing_title_music = true;
					ost_start_samples(0, 1, 1);
				}
				else if(nba_jam_title_screen == true && nba_jam_playing_title_music == true && nba_jam_intermission == false)
					return 0; /* do nothing */
				else
					ost_stop_samples();
			}
			else {
				if(m_nba_start_counter == 2) {
					nba_jam_boot_up = false;
					m_nba_start_counter = 0;
				}
			}

			/* Time to stop the NBA Jam music samples.*/
			if(nba_jam_title_screen == false) {
				ost_stop_samples();
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
					nba_jam_select_screen = false;
					nba_jam_intermission = false;
					nba_jam_playing_title_music = true;
					ost_start_samples(0, 1, 1);
				}
				else if(nba_jam_title_screen == true && nba_jam_playing_title_music == true && nba_jam_intermission == false)
					return 0; /* do nothing */
				else
					ost_stop_samples();
			}
			break;

		/* Doesn't seem to do anything? Appears after title screen demo game. Showing high scores. Replay the NBA Jam title music?*/
		case 0x7E:
			nba_jam_intermission = false;
			if(nba_jam_title_screen == true && nba_jam_playing_title_music == false && nba_jam_in_game == false) {
				ost_start_samples(0, 1, 1);
			}
			break;

		/* Team select.*/
		case 0x1:
			nba_jam_title_screen = false;
			nba_jam_select_screen = true;
			nba_jam_intermission = false;
			nba_jam_in_game = true;
			nba_jam_playing_title_music = false;
			ost_start_samples(2, 3, 1);
			break;

		/* 1st quarter.*/
		case 0x2:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(4, 5, 1);
			break;

		/* 2nd quarter.*/
		case 0x6:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(6, 7, 1);
			break;

		/* Half time report.*/
		case 0x4:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = true;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(10, 11, 1);
			break;

		/* 3rd quarter.*/
		case 0x7:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(4, 5, 1);
			break;

		/* 4th quarter.*/
		case 0x8:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(6, 7, 1);
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
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(12, 13, 1);
			break;

		/* Intermission.*/
		case 0xA:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = true;
			nba_jam_in_game = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(8, 9, 1);
			break;

		/* Overtime.*/
		case 0xB:
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;
			ost_start_samples(6, 7, 1);
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
			break;
	}

	ost_mix_samples();

	m_nba_last_offset = data;

	return schedule_default_sound;
}

bool generate_ost_sound_outrun(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

	if(outrun_start == true) {
		ost_start_samples(0, 1, 1);
		outrun_start = false;
		outrun_diddy = true;
		outrun_lastwave = false;
	}

	switch (data) {
		case 0x0:
			if(outrun_diddy == true) {
				outrun_start_counter++;

				if(outrun_start_counter == 2) {
					ost_start_samples(2, 3, 0);
					outrun_diddy = false;
					outrun_title_diddy = true;
					outrun_lastwave = false;
				}
			}
			else if(outrun_title_diddy == true) {
				outrun_diddy = false;
				outrun_start_counter++;

				if(outrun_start_counter > 5)
					outrun_title_diddy = false;
			}
			else if(outrun_diddy == false && outrun_title_diddy == false) {
				ost_start_samples(0, 1, 1);
				outrun_diddy = true;
				outrun_start_counter = 1;
				outrun_lastwave = false;
			}
			break;

		// 2. --> Passing Breeze
		case 0x81:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			ost_start_samples(8, 9, 1);
			break;

		// 1. --> Splash wave
		case 0x82:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			ost_start_samples(10, 11, 1);
			break;

		// 3 --> Magical Sound Shower
		case 0x85:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			ost_start_samples(6, 7, 1);
			break;

		// --> Last Wave
		case 0x93:
			if(outrun_lastwave == false) {
				outrun_diddy = false;
				outrun_title_diddy = false;
				outrun_lastwave = true;
				ost_start_samples(4, 5, 1);
			}
			else
				return 0; /* do nothing */
			break;

		// --> Enter Highscore
		case 0xA5:
			return 0; /* do nothing */
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_sf1(int data)
{
	/* initialize game config */
	schedule_default_sound = false;
	sa_volume = 100;

	if(sf1_start == true) {
		sf1_start = false;
		sf1_won = false;
		ost_start_samples(0, 1, 1);
	}

	switch (data) {
		// Retsu
		case 0x28:
			sf1_won = false;
			ost_start_samples(0, 1, 1);
			break;

		// Geki
		case 0x29:
			sf1_won = false;
			ost_start_samples(2, 3, 1);
			break;

		// Joe
		case 0x2A:
			sf1_won = false;
			ost_start_samples(4, 5, 1);
			break;

		// Mike
		case 0x2B:
			sf1_won = false;
			ost_start_samples(6, 7, 1);
			break;

		// Birdie
		case 0x2C:
			sf1_won = false;
			ost_start_samples(8, 9, 1);
			break;

		// Eagle
		case 0x2D:
			sf1_won = false;
			ost_start_samples(10, 11, 1);
			break;

		// Lee
		case 0x2E:
			sf1_won = false;
			ost_start_samples(12, 13, 1);
			break;

		// Gen
		case 0x2F:
			sf1_won = false;
			ost_start_samples(14, 15, 1);
			break;

		// Adon
		case 0x30:
			sf1_won = false;
			ost_start_samples(16, 17, 1);
			break;

		// Sagat
		case 0x31:
			sf1_won = false;
			ost_start_samples(18, 19, 1);
			break;

		// Ending
		case 0x32:
			sf1_won = false;
			ost_start_samples(20, 21, 1);
			break;

		// VS
		case 0x33:
			sf1_won = false;
			ost_start_samples(22, 23, 1);
			break;

		// Stage Select
		case 0x34:
			sf1_won = false;
			ost_start_samples(24, 25, 1);
			break;

		// Credits
		case 0x36:
			sf1_won = false;
			ost_start_samples(26, 27, 1);
			break;

		// Bonus 1
		case 0x3A:
			sf1_won = false;
			ost_start_samples(28, 29, 1);
			break;

		// Bonus 2
		case 0x3B:
			sf1_won = false;
			ost_start_samples(30, 31, 1);
			break;

		// Score
		case 0x3C:
			sf1_won = false;
			ost_start_samples(32, 33, 1);
			break;

		// the end?
		case 0x3E:
			sf1_won = false;
			ost_start_samples(34, 35, 1);
			break;

		// Won
		case 0x37:
			if(sf1_won == false) {
				sf1_won = true;
				ost_start_samples(36, 37, 1);
			}
			else
				return 0; /* do nothing */
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}

bool generate_ost_sound_sf2(int data)
{
	/* initialize game config */
	schedule_default_sound = false;

	switch (data)
	{
		case 0x1:
			// ryu music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(0, 1, 1);
			break;

		case 0x2:
			// e honda music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(12, 13, 1);
			break;

		case 0x3:
			// blanka music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(4, 5, 1);
			break;

		case 0x4:
			// ken music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(52, 53, 1);
			break;

		case 0x5:
			// guile music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(16, 17, 1);
			break;

		case 0x6:
			// chun li music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(8, 9, 1);
			break;

		case 0x7:
			// zangief music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(56, 57, 1);
			break;

		case 0x8:
			// dhalsim music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(20, 21, 1);
			break;

		case 0x9:
			// balrog music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(24, 25, 1);
			break;

		case 0xa:
			// vega music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(60, 61, 1);
			break;

		case 0xb:
			// sagat music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(28, 29, 1);
			break;

		case 0xc:
			// m. bison music slow
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(32, 33, 1);
			break;

		case 0xd:
			// bonus stage music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(64, 65, 1);
			break;

		case 0xe:
			// character select stage
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(48, 49, 1);
			break;

		case 0xf:
			// versus screen ditty
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(36, 37, 0);
			break;

		case 0x10:
			// end of fight dialog/resolution screen
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(38, 39, 0);
			break;

		case 0x11:
			// continue music, loop for ending credits
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(40, 41, 1);
			break;

		case 0x13:
			// game over
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(50, 51, 0);
			break;

		case 0x14:
			// high score screen
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(42, 43, 0);
			break;

		case 0x15:
			// player joined music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(46, 47, 0);
			break;

		case 0x16:
			// intro music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(44, 45, 1);
			break;

		case 0x18:
			// ryu ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(82, 83, 0);
			break;

		case 0x19:
			// e.honda ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(72, 73, 1);
			break;

		case 0x1a:
			// blanka ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(74, 75, 1);
			break;

		case 0x1b:
			// guile ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(76, 77, 1);
			break;

		case 0x1c:
			// ken ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(68, 69, 1);
			break;

		case 0x1d:
			// chun li ending music?
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(86, 87, 1);
			break;

		case 0x1e:
			// zangief ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(78, 79, 1);
			break;

		case 0x1f:
			// dhalsim ending music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(84, 85, 1);
			break;

		case 0x34:
			// wedding music -- ken ending
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(70, 71, 1);
			break;

		case 0x35:
			// chun li ending #2
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(88, 89, 1);
			break;

		case 0x79:
			// ryu music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(2, 3, 1);
			break;

		case 0x7a:
			// e. honda music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(14, 15, 1);
			break;

		case 0x7b:
			// blanka music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(6, 7, 1);
			break;

		case 0x7c:
			// guile music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(18, 19, 1);
			break;

		case 0x7d:
			// ken music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(54, 55, 1);
			break;

		case 0x7e:
			// fast chun li music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(10, 11, 1);
			break;

		case 0x7f:
			// zangief music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(58, 59, 1);
			break;

		case 0x80:
			// dhalsim music fast
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(22, 23, 1);
			break;

		case 0x81:
			// fast balrog music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(26, 27, 1);
			break;

		case 0x82:
			// fast vega music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(62, 63, 1);
			break;

		case 0x83:
			// fast sagat music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(30, 31, 1);
			break;

		case 0x84:
			// fast m. bison music
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(34, 35, 1);
			break;

		case 0x8c:
			// m. bison ending
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(66, 67, 1);
			break;

		case 0x8d:
			// special ending
			fadingMusic = false;
			sa_volume = 100;
			ost_start_samples(80, 81, 1);
			break;

		case 0x52: /* you win or lose */
		case 0x24: /* adding bonus points */
			fadingMusic = true;
			schedule_default_sound = true;
			break;

		/* Time to stop the Street Fighter 2 music samples.*/
		case 0xf0:
		case 0xf2:
		case 0xf7:
			ost_stop_samples();
			break;

		default:
			schedule_default_sound = true;
			break;
	}

	ost_mix_samples();

	return schedule_default_sound;
}
