/*********************************************************************

	OST sample support

*********************************************************************/


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


static INLINE void generate_ost_sound_ddragon(void)
{
	int a = 0;
	int o_max_samples = 23;
	int sa_left = 0;
	int sa_right = 1;
	int sa_volume = 40;
	bool sa_loop = 1; // --> 1 == loop, 0 == do not loop.
	bool sa_play_sample = false;
	bool sa_play_original = false;
	bool ddragon_do_nothing = false;
	bool ddragon_stop_samples = false;
	bool ddragon_play_default = false;

	switch(data) {
		// Use for a counter flag on the title screen and stopping music.
		case 0xFF:
			// We are at the title screen.
			if(ddragon_current_music == 10 && ddragon_stage != 4) {
					// A coin has been inserted, lets stop the title music, about to start the first stage.
					if(d_title_counter > 5) {
						ddragon_stop_samples = true;
						d_title_counter = 0;
					}
					else
						d_title_counter++;
			}
			else {
				ddragon_stop_samples = true;
				ddragon_stage = 0;
				d_title_counter = 0;
			}
		break;

		// Title screen.
		case 0x1:
			if(ddragon_current_music != 10 && ddragon_stage != 4) {
				ddragon_current_music = 10;
				sa_play_sample = true;
				sa_left = 0;
				sa_right = 1;
			}
			else if(ddragon_stage == 4 && ddragon_current_music != 15) { // Final boss fight.
				ddragon_current_music = 15;
				sa_play_sample = true;
				sa_left = 22;
				sa_right = 23;
			}
			else
				ddragon_do_nothing = true;

			d_title_counter = 0;
		break;

		// Stage 1.
		case 0x9:
			ddragon_stage = 1;
			ddragon_current_music = 1;
			sa_play_sample = true;
			sa_left = 2;
			sa_right = 3;
		break;

		// Stage 2.
		case 0x7:
			ddragon_stage = 2;
			ddragon_current_music = 2;
			sa_play_sample = true;
			sa_left = 4;
			sa_right = 5;
		break;

		// Stage 3.
		case 0xA:
			if(ddragon_current_music != 3 && ddragon_stage != 3) {
				ddragon_stage = 3;
				ddragon_current_music = 3;
				sa_play_sample = true;
				sa_left = 6;
				sa_right = 7;
			}
			else if(ddragon_stage == 3) {
				sa_play_sample = true;
				sa_left = 8;
				sa_right = 9;
			}
			else
				ddragon_do_nothing;
		break;

		// Stage 4.
		case 0xD:
			ddragon_stage = 4;
			ddragon_current_music = 4;
			sa_play_sample = true;
			sa_left = 10;
			sa_right = 11;
		break;

		// Credits.
		case 0x6:
			sa_volume = 100;
			sa_loop = 0;
			ddragon_stage = 5;
			ddragon_current_music = 5;
			sa_play_sample = true;
			sa_left = 12;
			sa_right = 13;
		break;

		// Level finished.
		case 0xE:
			ddragon_current_music = 11;
			sa_loop = 0;
			sa_play_sample = true;
			sa_left = 14;
			sa_right = 15;
		break;

		// Short diddy after boss battle.
		case 0xC:
			ddragon_current_music = 12;
			sa_loop = 0;
			sa_play_sample = true;
			sa_left = 16;
			sa_right = 17;
		break;

		// Boss battle music.
		case 0x3:
			if(ddragon_stage == 3) {
				ddragon_current_music = 14;
				sa_play_sample = true;
				sa_left = 20;
				sa_right = 21;
			}
			else {
				ddragon_current_music = 13;
				sa_play_sample = true;
				sa_left = 18;
				sa_right = 19;
			}
		break;

		default:
			soundlatch_w( 0, data );
			cpu_set_irq_line( snd_cpu, sound_irq, (sound_irq == IRQ_LINE_NMI) ? PULSE_LINE : HOLD_LINE );
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
			sample_set_stereo_volume(1, sa_volume, sa_volume);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 0) { // Left channel only. Lets make it play in both speakers.
			sample_set_stereo_volume(0, sa_volume, sa_volume);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 1) { // Both left and right channels. Lets make them play in there respective speakers.
			sample_set_stereo_volume(0, sa_volume, 0);
			sample_set_stereo_volume(1, 0, sa_volume);
		}
		else if(sample_playing(0) == 0 && sample_playing(1) == 0 && ddragon_do_nothing == false) { // No sample playing, revert to the default sound.
			sa_play_original = false;
			ddragon_current_music = 0;
			soundlatch_w( 0, data );
			cpu_set_irq_line( snd_cpu, sound_irq, (sound_irq == IRQ_LINE_NMI) ? PULSE_LINE : HOLD_LINE );
		}

		if(sa_play_original == true) {
			ddragon_current_music = 0;
			soundlatch_w( 0, data );
			cpu_set_irq_line( snd_cpu, sound_irq, (sound_irq == IRQ_LINE_NMI) ? PULSE_LINE : HOLD_LINE );
		}
	}
	else if(ddragon_do_nothing == true) {
		// --> Do nothing.
	}
	else if(ddragon_stop_samples == true) {
		ddragon_current_music = 0;
		a = 0;

		for(a = 0; a <= o_max_samples; a++) {
			sample_stop(a);
		}

		// Now play the default sound.
		soundlatch_w( 0, data );
		cpu_set_irq_line( snd_cpu, sound_irq, (sound_irq == IRQ_LINE_NMI) ? PULSE_LINE : HOLD_LINE );
	}
	else if(ddragon_play_default == true) {
		ddragon_current_music = 0;
		soundlatch_w( 0, data );
		cpu_set_irq_line( snd_cpu, sound_irq, (sound_irq == IRQ_LINE_NMI) ? PULSE_LINE : HOLD_LINE );
	}
}

static INLINE void generate_ost_sound_ffight(void)
{
	switch (data) {
		/* stage 1 upper level music*/
		case 0x40:
			/* Play the left channel.*/
			sample_start(0, 0, 1);

			/* Play the right channel.*/
			sample_start(1, 1, 1);
		break;

		/* stage #1: basement*/
		case 0x41:
			sample_start(0, 2, 1);
			sample_start(1, 3, 1);
		break;

		/* stage #2: subway intro*/
		case 0x42:
			/* play the normal version of the song unless playAlternateSong is true*/
			if (ff_play_alternate_song == false) {
				sample_start(0, 4, 1);
				sample_start(1, 5, 1);
			}
			else {
				sample_start(0, 40, 1);
				sample_start(1, 41, 1);
			}
		break;

		/* stage #2 exiting subway/alley*/
		case 0x43:
			sample_start(0, 6, 1);
			sample_start(1, 7, 1);
		break;

		/* double andore cage fight music*/
		case 0x44:
			sample_start(0, 8, 1);
			sample_start(1, 9, 1);
		break;

		/* bay area sea side theme*/
		case 0x45:
			sample_start(0, 10, 1);
			sample_start(1, 11, 1);

			/* we'll provision the alternate songs if they're not already*/
			if (ff_provision_alt_song == false) {
				ff_provision_alt_song = true;
			}
		break;

		/* bathroom music for bay area*/
		case 0x46:
			sample_start(0, 12, 1);
			sample_start(1, 13, 1);
		break;

		/* bay area post-bathroom ending/boss / final boss room entrance*/
		case 0x47:
			/* play the normal version of the song unless playAlternateSong is true*/
			if (ff_provision_alt_song == false) {
				sample_start(0, 14, 1);
				sample_start(1, 15, 1);
			}
			else {
				sample_start(0, 36, 1);
				sample_start(1, 37, 1);
			}
		break;

		/* bonus stage music*/
		case 0x4c:
			sample_start(0, 20, 1);
			sample_start(1, 21, 1);
		break;

		/* industrial music theme*/
		case 0x48:
			sample_start(0, 16, 1);
			sample_start(1, 17, 1);
		break;

		/* industrial zone elevator ride music*/
		case 0x49:
			sample_start(0, 18, 1);
			sample_start(1, 19, 1);
		break;

		/* game start ditty*/
		case 0x50:
			sample_start(0, 22, 0);
			sample_start(1, 23, 0);

			/* when the game starts, we'll reset all the alternate songs*/
			ff_provision_alt_song = false;
			ff_play_alternate_song = false;
		break;

		/* post explosion ditty*/
		case 0x51:
			sample_start(0, 24, 0);
			sample_start(1, 25, 0);
		break;

		/* opening cinematic song*/
		case 0x52:
			sample_start(0, 46, 0);
			sample_start(1, 47, 0);
		break;

		/* continue/dynamite song*/
		case 0x53:
			sample_start(0, 32, 1);
			sample_start(1, 33, 1);
		break;

		/* homosexual cheesy ending music*/
		case 0x54:
			sample_start(0, 48, 1);
			sample_start(1, 49, 1);
		break;

		/* player select song*/
		case 0x55:
			sample_start(0, 30, 0);
			sample_start(1, 31, 0);
		break;

		/* stage end/victory song*/
		case 0x57:
			sample_start(0, 28, 0);
			sample_start(1, 29, 0);

			/* when we beat a stage after the alternate songs are provisioned, we know that we should be playing the alternate songs*/
			if (ff_provision_alt_song == true) {
				ff_play_alternate_song = true;
			}
		break;

		/* final stage clear ditty*/
		case 0x58:
			sample_start(0, 26, 0);
			sample_start(1, 27, 0);

			ff_provision_alt_song = false;
			ff_play_alternate_song = false;
		break;

		default:
			if(ACCESSING_LSB)
				soundlatch_w(0,data & 0xff);

			/* Lets stop the Final Fight sample music.*/
			if(data == 0xf0 || data == 0xf2 || data == 0xf7) {
				int a = 0;

				for(a = 0; a <= 50; a++) {
					sample_stop(a);
				}
			}
		break;
	}

	/* Determine how we should mix these samples together.*/
	if(sample_playing(0) == 0 && sample_playing(1) == 1) { /* Right channel only. Lets make it play in both speakers.*/
		sample_set_stereo_volume(1, 100, 100);
	}
	else if(sample_playing(0) == 1 && sample_playing(1) == 0) { /* Left channel only. Lets make it play in both speakers.*/
		sample_set_stereo_volume(0, 100, 100);
	}
	else if(sample_playing(0) == 1 && sample_playing(1) == 1) { /* Both left and right channels. Lets make them play in there respective speakers.*/
		sample_set_stereo_volume(0, 100, 0);
		sample_set_stereo_volume(1, 0, 100);
	}
	else if(sample_playing(0) == 0 && sample_playing(1) == 0) { /* No sample playing, revert to the default sound.*/
		if(ACCESSING_LSB) {
			soundlatch_w(0,data & 0xff);
		}
	}
}

static INLINE void generate_ost_sound_mk(void)
{
	int a = 0;
	bool mk_do_nothing = false;
	bool sa_play_sample = false;
	bool sa_play_original = false;
	bool mk_stop_samples = false;

	int sa_left = 0;
	int sa_right = 1;
	bool sa_loop = 1; /* --> 1 == loop, 0 == do not loop.*/

	switch (data) {
		/* Intro title screen diddy*/
		case 0xFD13:
			/* --> Do nothing.*/
			mk_do_nothing = true;
			break;

		/* Intro title screen diddy*/
		case 0xFF13:
			sa_play_sample = true;
			sa_loop = 0;

			sa_left = 0; /* Left channel.*/
			sa_right = 1; /* Right channel/*/
			break;

		/* Second player joining diddy*/
		case 0xFD18:
			/* --> Do nothing.*/
			mk_do_nothing = true;
			break;

		/* Second player joining diddy*/
		case 0xFF18:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 0;
			sa_right = 1;
			break;				

		/* Character selection screen.*/
		case 0xFD01:
			mk_do_nothing = true; /* --> Do nothing.*/
			break;
					
		/* Character selection screen.*/
		case 0xFF01:
			sa_play_sample = true;
			sa_left = 2;
			sa_right = 3;			
			break;

		/* Scrolling character map*/
		case 0xFD12:
			mk_do_nothing = true;	
			break;

		/* Scrolling character map*/
		case 0xFF12:
			sa_play_sample = true;
					
			sa_left = 4;
			sa_right = 5;
			break;

		/* Scrolling character map end*/
		case 0xFD1E:
			mk_do_nothing = true;
			break;

		/* Scrolling character map end*/
		case 0xFF1E:
			mk_do_nothing = true;
			break;

		/* Continue music*/
		case 0xFD06:
			mk_do_nothing = true;	
			break;

		/* Continue music*/
		case 0xFF06:
			sa_play_sample = true;
					
			sa_left = 6;
			sa_right = 7;
			break;

		/* Game over music*/
		case 0xFD02:
			mk_do_nothing = true;	
			break;

		/* Game over music*/
		case 0xFF02:
			sa_play_sample = true;
					
			sa_left = 20;
			sa_right = 21;
			break;
					
		/* Test your might music.*/
		case 0xFD19:
			mk_do_nothing = true;
			break;

		/* Test your might music.*/
		case 0xFF19:
			sa_play_sample = true;
					
			sa_left = 16;
			sa_right = 17;
			break;

		/* Test your end (fail).*/
		case 0xFD1A:
			mk_do_nothing = true;
			break;

		/* Test your end (fail).*/
		case 0xFF1A:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 18;
			sa_right = 19;
			break;
				
		/* Fatality music*/
		case 0xFDEE:
			mk_do_nothing = true;	
			break;

		/* Fatality music*/
		case 0xFFEE:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 8;
			sa_right = 9;
			break;

		/* Fatality music echo loop*/
		case 0xFDDE:
			mk_do_nothing = true;
			break;

		/* Fatality music echo loop*/
		case 0xFFDE:
			mk_do_nothing = true;
			break;
									
		/* Courtyard music*/
		case 0xFD03:
			mk_do_nothing = true;	
			break;

		/* Courtyard music*/
		case 0xFF03:
			sa_play_sample = true;
					
			sa_left = 10;
			sa_right = 11;
			break;

		/* Courtyard end music*/
		case 0xFD05:
			mk_do_nothing = true;
			break;

		/* Courtyard end music*/
		case 0xFF05:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 12;
			sa_right = 13;
			break;

		/* Courtyard finish him music*/
		case 0xFD04:
			mk_do_nothing = true;	
			break;

		/* Courtyard finish him music*/
		case 0xFF04:
			sa_play_sample = true;
					
			sa_left = 14;
			sa_right = 15;
			break;

		/* Warrior Shrine music*/
		case 0xFD0A:
			mk_do_nothing = true;	
			break;

		/* Warrior Shrine music*/
		case 0xFF0A:
			sa_play_sample = true;
					
			sa_left = 22;
			sa_right = 23;
			break;

		/* Warrior Shrine end music*/
		case 0xFD0C:
			mk_do_nothing = true;
			break;

		/* Warrior Shrine end music*/
		case 0xFF0C:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 24;
			sa_right = 25;
			break;

		/* Warrior Shrine finish him music*/
		case 0xFD0B:
			mk_do_nothing = true;	
			break;

		/* Warrior Shrine finish him music*/
		case 0xFF0B:
			sa_play_sample = true;
					
			sa_left = 26;
			sa_right = 27;
			break;

		/* The Pit music*/
		case 0xFD0D:
			mk_do_nothing = true;	
			break;

		/* The Pit music*/
		case 0xFF0D:
			sa_play_sample = true;
					
			sa_left = 28;
			sa_right = 29;
			break;

		/* The Pit end music*/
		case 0xFD0F:
			mk_do_nothing = true;
			break;

		/* The Pit end music*/
		case 0xFF0F:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 30;
			sa_right = 31;
			break;

		/* The Pit finish him music*/
		case 0xFD0E:
			mk_do_nothing = true;	
			break;

		/* The Pit finish him music*/
		case 0xFF0E:
			sa_play_sample = true;
					
			sa_left = 32;
			sa_right = 33;
			break;

		/* Throne Room music*/
		case 0xFD1B:
			mk_do_nothing = true;	
			break;

		/* Throne Room music*/
		case 0xFF1B:
			sa_play_sample = true;
					
			sa_left = 34;
			sa_right = 35;
			break;

		/* Throne Room end music*/
		case 0xFD1D:
			mk_do_nothing = true;
			break;

		/* Throne Room end music*/
		case 0xFF1D:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 36;
			sa_right = 37;
			break;

		/* Throne Room finish him music*/
		case 0xFD1C:
			mk_do_nothing = true;	
			break;

		/* Throne Room finish him music*/
		case 0xFF1C:
			sa_play_sample = true;
					
			sa_left = 38;
			sa_right = 39;
			break;

		/* Goro's Lair music*/
		case 0xFD14:
			mk_do_nothing = true;	
			break;

		/* Goro's Lair music*/
		case 0xFF14:
			sa_play_sample = true;
					
			sa_left = 40;
			sa_right = 41;
			break;

		/* Goro's Lair end music*/
		case 0xFD17:
			mk_do_nothing = true;
			break;

		/* Goro's Lair end music*/
		case 0xFF17:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 42;
			sa_right = 43;
			break;

		/* Goro's Lair finish him music*/
		case 0xFD16:
			mk_do_nothing = true;	
			break;

		/* Goro's Lair finish him music*/
		case 0xFF16:
			sa_play_sample = true;
					
			sa_left = 44;
			sa_right = 45;
			break;
		
		/* Endurance switch characters chime*/
		case 0xFD10:
			mk_do_nothing = true;	
			break;

		/* Endurance switch characters chime*/
		case 0xFF10:
			sa_play_sample = true;
					
			sa_left = 46;
			sa_right = 47;
			break;
					
		/* Victory music*/
		case 0xFD1F:
			mk_do_nothing = true;	
			break;

		/* Victory music*/
		case 0xFF1F:
			sa_play_sample = true;
					
			sa_left = 48;
			sa_right = 49;
			break;

		/* Palace gates music*/
		case 0xFD07:
			mk_do_nothing = true;	
			break;

		/* Palace gates music*/
		case 0xFF07:
			sa_play_sample = true;
					
			sa_left = 50;
			sa_right = 51;
			break;

		/* Palace Gates end music*/
		case 0xFD09:
			mk_do_nothing = true;
			break;

		/* Palace Gates end music*/
		case 0xFF09:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 52;
			sa_right = 53;
			break;

		/* Palace Gates finish him music*/
		case 0xFD08:
			mk_do_nothing = true;	
			break;

		/* Palace Gates finish him music*/
		case 0xFF08:
			sa_play_sample = true;
			sa_left = 54;
			sa_right = 55;
			break;								
																
		default:
			soundlatch_w(0, data & 0xff);

			/* Time to stop the Mortal Kombat music samples.*/
			if(data == 0xFD00 || data == 0xFF00) {
				a = 0;

				/* Lets stop the Mortal Kombat sample music as we are starting up a new sample to play.*/
				for(a = 0; a <= 55; a++) {
					sample_stop(a);
				}
			}
			break;
	}

	if(sa_play_sample == true) {
		a = 0;

		/* Lets stop the Mortal Kombat sample music as we are starting up a new sample to play.*/
		for(a = 0; a <= 55; a++) {
			sample_stop(a);
		}

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);
			
		/* Determine how we should mix these samples together.*/
		if(sample_playing(0) == 0 && sample_playing(1) == 1) { /* Right channel only. Lets make it play in both speakers.*/
			sample_set_stereo_volume(1, 100, 100);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 0) { /* Left channel only. Lets make it play in both speakers.*/
			sample_set_stereo_volume(0, 100, 100);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 1) { /* Both left and right channels. Lets make them play in there respective speakers.*/
			sample_set_stereo_volume(0, 100, 0);
			sample_set_stereo_volume(1, 0, 100);
		}
		else if(sample_playing(0) == 0 && sample_playing(1) == 0 && mk_do_nothing == false) { /* No sample playing, revert to the default sound.*/
			sa_play_original = false;
			soundlatch_w(0, data & 0xff);
		}

		if(sa_play_original == true)
			soundlatch_w(0, data & 0xff);
	}
	else if(mk_stop_samples == true) {
		a = 0;

		/* Lets stop the Mortal Kombat sample music as we are starting up a new sample to play.*/
		for(a = 0; a <= 55; a++) {
			sample_stop(a);
		}

		/* Now play the default sound.*/
		soundlatch_w(0, data & 0xff);
	}
}

static INLINE void generate_ost_sound_mk_tunit(void)
{
	int a = 0;
	bool mk_do_nothing = false;
	bool sa_play_sample = false;
	bool sa_play_original = false;
	bool mk_stop_samples = false;
		
	int sa_left = 0;
	int sa_right = 1;
	bool sa_loop = 1; /* --> 1 == loop, 0 == do not loop.*/

	switch (data) {
		/* Intro title screen diddy*/
		case 0x13:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 0; /* Left channel.*/
			sa_right = 1; /* Right channel/*/
			break;

		/* Second player joining diddy*/
		case 0x18:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 0;
			sa_right = 1;
			break;				

		/* Character selection screen.*/
		case 0x1:
			sa_play_sample = true;
			sa_left = 2;
			sa_right = 3;			
			break;

		/* Scrolling character map*/
		case 0x12:
			sa_play_sample = true;
					
			sa_left = 4;
			sa_right = 5;
			break;

		/* Scrolling character map end*/
		case 0x1E:
			mk_do_nothing = true;
			break;

		/* Continue music*/
		case 0x6:
			sa_play_sample = true;
					
			sa_left = 6;
			sa_right = 7;
			break;

		/* Game over music*/
		case 0x2:
			sa_play_sample = true;
					
			sa_left = 20;
			sa_right = 21;
			break;

		/* Test your might music.*/
		case 0x19:
			sa_play_sample = true;
					
			sa_left = 16;
			sa_right = 17;
			break;

		/* Test your end (fail).*/
		case 0x1A:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 18;
			sa_right = 19;
			break;

		/* Fatality music*/
		case 0xEE:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 8;
			sa_right = 9;
			break;

		/* Fatality music echo loop*/
		case 0xDE:
			mk_do_nothing = true;
			break;
							
		/* Courtyard music*/
		case 0x3:
			sa_play_sample = true;
					
			sa_left = 10;
			sa_right = 11;
			break;

		/* Courtyard end music*/
		case 0x5:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 12;
			sa_right = 13;
			break;

		/* Courtyard finish him music*/
		case 0x4:
			sa_play_sample = true;
					
			sa_left = 14;
			sa_right = 15;
			break;

		/* Warrior Shrine music*/
		case 0xA:
			sa_play_sample = true;
					
			sa_left = 22;
			sa_right = 23;
			break;

		/* Warrior Shrine end music*/
		case 0xC:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 24;
			sa_right = 25;
			break;

		/* Warrior Shrine finish him music*/
		case 0xB:
			sa_play_sample = true;
					
			sa_left = 26;
			sa_right = 27;
			break;

		/* The Pit music*/
		case 0xD:
			sa_play_sample = true;
					
			sa_left = 28;
			sa_right = 29;
			break;

		/* The Pit end music*/
		case 0xF:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 30;
			sa_right = 31;
			break;

		/* The Pit finish him music*/
		case 0xE:
			sa_play_sample = true;
					
			sa_left = 32;
			sa_right = 33;
			break;

		/* Throne Room music*/
		case 0x1B:
			sa_play_sample = true;
					
			sa_left = 34;
			sa_right = 35;
			break;

		/* Throne Room end music*/
		case 0x1D:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 36;
			sa_right = 37;
			break;

		/* Throne Room finish him music*/
		case 0x1C:
			sa_play_sample = true;
					
			sa_left = 38;
			sa_right = 39;
			break;

		/* Goro's Lair music*/
		case 0x14:
			sa_play_sample = true;
					
			sa_left = 40;
			sa_right = 41;
			break;

		/* Goro's Lair end music*/
		case 0x17:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 42;
			sa_right = 43;
			break;

		/* Goro's Lair finish him music*/
		case 0x16:
			sa_play_sample = true;
					
			sa_left = 44;
			sa_right = 45;
			break;

		/* Endurance switch characters chime*/
		case 0x10:
			sa_play_sample = true;
					
			sa_left = 46;
			sa_right = 47;
			break;

		/* Victory music*/
		case 0x1F:
			sa_play_sample = true;
					
			sa_left = 48;
			sa_right = 49;
			break;

		/* Palace gates music*/
		case 0x7:
			sa_play_sample = true;
					
			sa_left = 50;
			sa_right = 51;
			break;

		/* Palace Gates end music*/
		case 0x9:
			sa_play_sample = true;
			sa_loop = 0;
					
			sa_left = 52;
			sa_right = 53;
			break;

		/* Palace Gates finish him music*/
		case 0x8:
			sa_play_sample = true;
			sa_left = 54;
			sa_right = 55;
			break;								
																
		default:
			soundlatch_w(0, data & 0xff);

			/* Time to stop the Mortal Kombat music samples.*/
			if(data == 0x0) {
				a = 0;
						
				/* Lets stop the Mortal Kombat sample music as we are starting up a new sample to play.*/
				for(a = 0; a <= 55; a++) {
					sample_stop(a);
				}
			}		
			break;
	}

	if(sa_play_sample == true) {
		a = 0;

		/* Lets stop the Mortal Kombat sample music as we are starting up a new sample to play.*/
		for(a = 0; a <= 55; a++) {
			sample_stop(a);
		}

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);
			
		/* Determine how we should mix these samples together.*/
		if(sample_playing(0) == 0 && sample_playing(1) == 1) { /* Right channel only. Lets make it play in both speakers.*/
			sample_set_stereo_volume(1, 100, 100);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 0) { /* Left channel only. Lets make it play in both speakers.*/
			sample_set_stereo_volume(0, 100, 100);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 1) { /* Both left and right channels. Lets make them play in there respective speakers.*/
			sample_set_stereo_volume(0, 100, 0);
			sample_set_stereo_volume(1, 0, 100);
		}
		else if(sample_playing(0) == 0 && sample_playing(1) == 0 && mk_do_nothing == false) { /* No sample playing, revert to the default sound.*/
			sa_play_original = false;
			soundlatch_w(0, data & 0xff);
		}

		if(sa_play_original == true)
			soundlatch_w(0, data & 0xff);
	}
	else if(mk_stop_samples == true) {
		a = 0;

		/* Lets stop the Mortal Kombat sample music as we are starting up a new sample to play.*/
		for(a = 0; a <= 55; a++) {
			sample_stop(a);
		}

		/* Now play the default sound.*/
		soundlatch_w(0, data & 0xff);
	}
}

static INLINE void generate_ost_sound_moonwalker(void)
{
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

static INLINE void generate_ost_sound_nba_jam(void)
{
	int a = 0;
	bool nba_jam_do_nothing = false;
	bool sa_play_sample = false;
	bool sa_play_original = false;
	bool nba_jam_stop_samples = false;
	bool nba_jam_play_default = false;

	int sa_left = 0;
	int sa_right = 1;
	bool sa_loop = 1; /* --> 1 == loop, 0 == do not loop.		*/

	switch (data) {
		case 0x8C:
			nba_jam_do_nothing = true;
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
					sa_play_sample = true;
					nba_jam_select_screen = false;
					nba_jam_intermission = false;
					nba_jam_playing_title_music = true;

					sa_left = 0; /* Left channel.*/
					sa_right = 1; /* Right channel.*/
				}
				else if(nba_jam_title_screen == true && nba_jam_playing_title_music == true && nba_jam_intermission == false)
					nba_jam_do_nothing = true;
				else
					nba_jam_stop_samples = true;
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
					sa_play_sample = true;
					nba_jam_select_screen = false;
					nba_jam_intermission = false;
					nba_jam_playing_title_music = true;

					sa_left = 0; /* Left channel.*/
					sa_right = 1; /* Right channel.*/
				}
				else if(nba_jam_title_screen == true && nba_jam_playing_title_music == true && nba_jam_intermission == false)
					nba_jam_do_nothing = true;
				else
					nba_jam_stop_samples = true;
			}
			break;

		/* Doesn't seem to do anything? Appears after title screen demo game. Showing high scores. Replay the NBA Jam title music?*/
		case 0x7E:
			nba_jam_intermission = false;
			if(nba_jam_title_screen == true && nba_jam_playing_title_music == false && nba_jam_in_game == false) {
				sa_play_sample = true;

				sa_left = 0;
				sa_right = 1;
			}
			break;				
							
		/* Team select.*/
		case 0x1:
			sa_play_sample = true;
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
			sa_play_sample = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 4;
			sa_right = 5;
			break;

		/* 2nd quarter.*/
		case 0x6:
			sa_play_sample = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 6;
			sa_right = 7;
			break;				

		/* Half time report.*/
		case 0x4:
			sa_play_sample = true;
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
			sa_play_sample = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 4;
			sa_right = 5;
			break;

		/* 4th quarter.*/
		case 0x8:
			sa_play_sample = true;
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
			nba_jam_do_nothing = true;
			break;
			
		/* Game stats after playing a full game.*/
		case 0x3:
			sa_play_sample = true;
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
			sa_play_sample = true;
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
			sa_play_sample = true;
			nba_jam_select_screen = false;
			nba_jam_title_screen = false;
			nba_jam_intermission = false;
			nba_jam_playing_title_music = false;

			sa_left = 6;
			sa_right = 7;
			break;
			
		/* NBA Jam halftime report.*/
		case 0x71:
			nba_jam_do_nothing = true;
			break;

		/* Altitude with a attitude.*/
		case 0xCC:
			nba_jam_do_nothing = true;
			break;
							
		/* Welcome to NBA Jam.*/
		case 0xCB:
			if(nba_jam_select_screen == true)
				nba_jam_do_nothing = true;
			else
				nba_jam_play_default = true;
			break;
			
		default:
			soundlatch_w(0, data & 0xff);

			/* Time to stop the NBA Jam music samples.*/
			if(data == 0x0 && nba_jam_title_screen == false) {
				a = 0;

				/* Lets stop the NBA Jam sample music as we are starting up a new sample to play.*/
				for(a = 0; a <= 13; a++) {
					sample_stop(a);
				}
			}
			break;
	}

	if(sa_play_sample == true) {
		a = 0;

		/* Lets stop the NBA Jam sample music as we are starting up a new sample to play.*/
		for(a = 0; a <= 13; a++) {
			sample_stop(a);
		}

		sample_start(0, sa_left, sa_loop);
		sample_start(1, sa_right, sa_loop);
			
		/* Determine how we should mix these samples together.*/
		if(sample_playing(0) == 0 && sample_playing(1) == 1) { /* Right channel only. Lets make it play in both speakers.*/
			sample_set_stereo_volume(1, 100, 100);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 0) { /* Left channel only. Lets make it play in both speakers.*/
			sample_set_stereo_volume(0, 100, 100);
		}
		else if(sample_playing(0) == 1 && sample_playing(1) == 1) { /* Both left and right channels. Lets make them play in there respective speakers.*/
			sample_set_stereo_volume(0, 100, 0);
			sample_set_stereo_volume(1, 0, 100);
		}
		else if(sample_playing(0) == 0 && sample_playing(1) == 0 && nba_jam_do_nothing == false) { /* No sample playing, revert to the default sound.*/
			sa_play_original = false;
			soundlatch_w(0, data & 0xff);
		}

		if(sa_play_original == true)
			soundlatch_w(0, data & 0xff);
	}
	else if(nba_jam_do_nothing == true) {
		/* --> Do nothing.*/
	}
	else if(nba_jam_stop_samples == true) {
		a = 0;

		/* Lets stop the NBA Jam sample music as we are starting up a new sample to play.*/
		for(a = 0; a <= 13; a++) {
			sample_stop(a);
		}

		/* Now play the default sound.*/
		soundlatch_w(0, data & 0xff);
	}
	else if(nba_jam_play_default == true)
		soundlatch_w(0, data & 0xff);

	m_nba_last_offset = data;
}

static INLINE void generate_ost_sound_outrun(void)
{
	int a = 0;
	int o_max_samples = 12;
	int sa_left = 0;
	int sa_right = 1;
	bool sa_loop = 1; // --> 1 == loop, 0 == do not loop.
	bool sa_play_sample = false;
	bool sa_play_original = false;
	bool outrun_do_nothing = false;
	bool outrun_stop_samples = false;
	bool outrun_play_default = false;
				
	if(outrun_start == true) {
		sa_play_sample = true;
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
					sa_play_sample = true;
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
				sa_play_sample = true;

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
			sa_play_sample = true;
			sa_left = 8;
			sa_right = 9;				
			break;
						
		// 1. --> Splash wave
		case 0x82:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			sa_play_sample = true;
			sa_left = 10;
			sa_right = 11;				
			break;

		// 3 --> Magical Sound Shower
		case 0x85:
			outrun_diddy = false;
			outrun_title_diddy = false;
			outrun_lastwave = false;
			sa_play_sample = true;
			sa_left = 6;
			sa_right = 7;				
			break;

		// --> Last Wave
		case 0x93:
			if(outrun_lastwave == false) {
				outrun_diddy = false;
				outrun_title_diddy = false;
				outrun_lastwave = true;
				sa_play_sample = true;
				sa_left = 4;
				sa_right = 5;
			}
			else
				outrun_do_nothing = true;
			break;

		default:
			sound_shared_ram[0]=data&0xff;
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
		else if(sample_playing(0) == 0 && sample_playing(1) == 0 && outrun_do_nothing == false) { // No sample playing, revert to the default sound.
			sa_play_original = false;
			sound_shared_ram[0]=data&0xff;
		}

		if(sa_play_original == true)
			sound_shared_ram[0]=data&0xff;
	}
	else if(outrun_do_nothing == true) {
		// --> Do nothing.
	}
	else if(outrun_stop_samples == true) {
		a = 0;

		for(a = 0; a <= o_max_samples; a++) {
			sample_stop(a);
		}

		// Now play the default sound.
		sound_shared_ram[0]=data&0xff;
	}
	else if(outrun_play_default == true) {
		sound_shared_ram[0]=data&0xff;
	}
}
