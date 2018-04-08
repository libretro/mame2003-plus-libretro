/***************************************************************************

	Midway/Williams Audio Boards

****************************************************************************/

MACHINE_DRIVER_EXTERN( williams_cvsd_sound );
MACHINE_DRIVER_EXTERN( williams_adpcm_sound );
MACHINE_DRIVER_EXTERN( williams_narc_sound );

void williams_cvsd_init(int cpunum, int pianum);
void williams_cvsd_data_w(int data);
void williams_cvsd_reset_w(int state);

void williams_adpcm_init(int cpunum);
void williams_adpcm_data_w(int data);
void williams_adpcm_reset_w(int state);

void williams_narc_init(int cpunum);
void williams_narc_data_w(int data);
void williams_narc_reset_w(int state);

bool	mk_playing_mortal_kombat; // For Mortal Kombat music hack. For Midway Y-Unit versions.
bool	mk_playing_mortal_kombat_t; // For Mortal Kombat music hack. For Midway T-Unit versions.
