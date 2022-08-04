/*********************************************************************

	OST sample support

*********************************************************************/

enum
{
  OST_SUPPORT_DDRAGON = 1,
  OST_SUPPORT_FFIGHT,
  OST_SUPPORT_MK,
  OST_SUPPORT_MK_T,
  OST_SUPPORT_MOONWALKER,
  OST_SUPPORT_NBA_JAM,
  OST_SUPPORT_OUTRUN,
  OST_SUPPORT_SF1,
  OST_SUPPORT_SF2
};


extern bool    ddragon_playing;
extern bool    ff_playing_final_fight;
extern bool    mk_playing_mortal_kombat;
extern bool    mk_playing_mortal_kombat_t;
extern bool    moonwalker_playing;
extern bool    nba_jam_playing;
extern bool    outrun_playing;
extern bool    sf1_playing;
extern bool    sf2_playing_street_fighter;


extern void init_ost_settings(int ost);


extern struct Samplesinterface ost_ddragon;
extern struct Samplesinterface ost_ffight;
extern struct Samplesinterface ost_mk;
extern struct Samplesinterface ost_moonwalker;
extern struct Samplesinterface ost_nba_jam;
extern struct Samplesinterface ost_outrun;
extern struct Samplesinterface ost_sf1;
extern struct Samplesinterface ost_sf2;


extern bool generate_ost_sound_ddragon    (int data);
extern bool generate_ost_sound_ffight     (int data);
extern bool generate_ost_sound_mk         (int data);
extern bool generate_ost_sound_mk_tunit   (int data);
extern bool generate_ost_sound_moonwalker (int data);
extern bool generate_ost_sound_nba_jam    (int data);
extern bool generate_ost_sound_outrun     (int data);
extern bool generate_ost_sound_sf1        (int data);
extern bool generate_ost_sound_sf2        (int data);


extern void ost_fade_volume (void);
