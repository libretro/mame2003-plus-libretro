/*********************************************************************

	OST sample support

*********************************************************************/

enum
{
  OST_SUPPORT_DISABLED = 0,
  OST_SUPPORT_DDRAGON,
  OST_SUPPORT_FFIGHT,
  OST_SUPPORT_MK,
  OST_SUPPORT_MK_T,
  OST_SUPPORT_MOONWALKER,
  OST_SUPPORT_NBA_JAM,
  OST_SUPPORT_OUTRUN,
  OST_SUPPORT_SF1,
  OST_SUPPORT_SF2
};


extern bool ost_support_enabled (int ost);
extern void install_ost_support (struct InternalMachineDriver *machine, int ost);

#define MDRV_INSTALL_OST_SUPPORT(ost)		\
	install_ost_support(machine, ost);		\

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
