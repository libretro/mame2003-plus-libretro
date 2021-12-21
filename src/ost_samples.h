/*********************************************************************

	OST sample support

*********************************************************************/


extern bool     schedule_default_sound;
extern int      m_nba_last_offset;
extern int      m_nba_start_counter;


extern bool     ddragon_playing;
extern bool     ff_playing_final_fight;
extern bool     mk_playing_mortal_kombat;
extern bool     mk_playing_mortal_kombat_t;
extern bool     moonwalker_playing;
extern bool     nba_jam_playing;
extern bool     outrun_playing;


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
