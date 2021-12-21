/*********************************************************************

	OST sample support

*********************************************************************/


extern int      m_nba_last_offset;
extern int      m_nba_start_counter;

extern bool     ddragon_playing;
extern int      ddragon_current_music;
extern int      ddragon_stage;
extern int      d_title_counter;

extern bool     ff_playing_final_fight;
extern bool     ff_provision_alt_song;
extern bool     ff_play_alternate_song;

extern bool     mk_playing_mortal_kombat;
extern bool     mk_playing_mortal_kombat_t;

extern bool     moonwalker_playing;
extern bool     moon_diddy;
extern int      mj_current_music;

extern bool     nba_jam_playing;
extern bool     nba_jam_title_screen;
extern bool     nba_jam_select_screen;
extern bool     nba_jam_intermission;
extern bool     nba_jam_in_game;
extern bool     nba_jam_boot_up;
extern bool     nba_jam_playing_title_music;

extern bool     outrun_playing;
extern bool     outrun_start;
extern bool     outrun_diddy;
extern bool     outrun_title_diddy;
extern bool     outrun_title;
extern bool     outrun_lastwave;
extern int      outrun_start_counter;


extern struct Samplesinterface ost_ddragon;
extern struct Samplesinterface ost_ffight;
extern struct Samplesinterface ost_mk;
extern struct Samplesinterface ost_moonwalker;
extern struct Samplesinterface ost_nba_jam;
extern struct Samplesinterface ost_outrun;


extern bool generate_ost_sound_ddragon    (int data);
extern bool generate_ost_sound_ffight     (int data);
extern bool generate_ost_sound_mk         (int data);
extern bool generate_ost_sound_mk_tunit   (int data);
extern bool generate_ost_sound_moonwalker (int data);
extern bool generate_ost_sound_nba_jam    (int data);
extern bool generate_ost_sound_outrun     (int data);
