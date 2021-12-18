/*********************************************************************

	OST sample support

*********************************************************************/

#include "driver.h"


bool     ddragon_playing = false;
int      ddragon_current_music = 0;
int      ddragon_stage = 0;
int      d_title_counter = 0;

bool     ff_playing_final_fight = false;
bool     ff_provision_alt_song;
bool     ff_play_alternate_song;

bool     mk_playing_mortal_kombat = false;
bool     mk_playing_mortal_kombat_t = false;

bool     moonwalker_playing = false;
bool     moon_diddy = false;
int      mj_current_music = 0;

bool     nba_jam_playing = false;
bool     nba_jam_title_screen;
bool     nba_jam_select_screen;
bool     nba_jam_intermission;
bool     nba_jam_in_game;
bool     nba_jam_boot_up;
bool     nba_jam_playing_title_music;

bool     outrun_playing = false;
bool     outrun_start = false;
bool     outrun_diddy = false;
bool     outrun_title_diddy = false;
bool     outrun_title = false;
bool     outrun_lastwave = false;
int      outrun_start_counter = 0;


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
