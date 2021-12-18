/***************************************************************************
	Midway/Williams Audio Boards
	----------------------------
	6809 MEMORY MAP
	Function                                  Address     R/W  Data
	---------------------------------------------------------------
	Program RAM                               0000-07FF   R/W  D0-D7
	Music (YM-2151)                           2000-2001   R/W  D0-D7
	6821 PIA                                  4000-4003   R/W  D0-D7
	HC55516 clock low, digit latch            6000        W    D0
	HC55516 clock high                        6800        W    xx
	Bank select                               7800        W    D0-D2
	Banked Program ROM                        8000-FFFF   R    D0-D7
****************************************************************************/

#include "driver.h"
#include "machine/6821pia.h"
#include "cpu/m6809/m6809.h"
#include "williams.h"
#include "ost_samples.h"



/***************************************************************************
	STATIC GLOBALS
****************************************************************************/

int		m_nba_last_offset;
int		m_nba_start_counter;

UINT8 williams_sound_int_state;

static INT8 sound_cpunum;
static INT8 soundalt_cpunum;
static UINT8 williams_pianum;

static UINT8 adpcm_bank_count;



/***************************************************************************
	PROTOTYPES
****************************************************************************/

static void init_audio_state(void);

static void cvsd_ym2151_irq(int state);
static void adpcm_ym2151_irq(int state);
static void cvsd_irqa(int state);
static void cvsd_irqb(int state);

static READ_HANDLER( cvsd_pia_r );

static WRITE_HANDLER( cvsd_pia_w );
static WRITE_HANDLER( cvsd_bank_select_w );

static READ_HANDLER( adpcm_command_r );
static WRITE_HANDLER( adpcm_bank_select_w );
static WRITE_HANDLER( adpcm_6295_bank_select_w );

static READ_HANDLER( narc_command_r );
static READ_HANDLER( narc_command2_r );
static WRITE_HANDLER( narc_command2_w );
static WRITE_HANDLER( narc_master_bank_select_w );
static WRITE_HANDLER( narc_slave_bank_select_w );



/***************************************************************************
	PROCESSOR STRUCTURES
****************************************************************************/

/* CVSD readmem/writemem structures */
MEMORY_READ_START( williams_cvsd_readmem )
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x2000, 0x2001, YM2151_status_port_0_r },
	{ 0x4000, 0x4003, cvsd_pia_r },
	{ 0x8000, 0xffff, MRA_BANK6 },
MEMORY_END


MEMORY_WRITE_START( williams_cvsd_writemem )
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x4000, 0x4003, cvsd_pia_w },
	{ 0x6000, 0x6000, hc55516_0_digit_clock_clear_w },
	{ 0x6800, 0x6800, hc55516_0_clock_set_w },
	{ 0x7800, 0x7800, cvsd_bank_select_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END



/* ADPCM readmem/writemem structures */
MEMORY_READ_START( williams_adpcm_readmem )
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2401, 0x2401, YM2151_status_port_0_r },
	{ 0x2c00, 0x2c00, OKIM6295_status_0_r },
	{ 0x3000, 0x3000, adpcm_command_r },
	{ 0x4000, 0xbfff, MRA_BANK6 },
	{ 0xc000, 0xffff, MRA_ROM },
MEMORY_END


MEMORY_WRITE_START( williams_adpcm_writemem )
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, adpcm_bank_select_w },
	{ 0x2400, 0x2400, YM2151_register_port_0_w },
	{ 0x2401, 0x2401, YM2151_data_port_0_w },
	{ 0x2800, 0x2800, DAC_0_data_w },
	{ 0x2c00, 0x2c00, OKIM6295_data_0_w },
	{ 0x3400, 0x3400, adpcm_6295_bank_select_w },
	{ 0x3c00, 0x3c00, MWA_NOP },/*mk_sound_talkback_w }, -- talkback port? */
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END



/* NARC master readmem/writemem structures */
MEMORY_READ_START( williams_narc_master_readmem )
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2001, 0x2001, YM2151_status_port_0_r },
	{ 0x3000, 0x3000, MRA_NOP },
	{ 0x3400, 0x3400, narc_command_r },
	{ 0x4000, 0xbfff, MRA_BANK6 },
	{ 0xc000, 0xffff, MRA_ROM },
MEMORY_END


MEMORY_WRITE_START( williams_narc_master_writemem )
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x2800, 0x2800, MWA_NOP },/*mk_sound_talkback_w }, -- talkback port? */
	{ 0x2c00, 0x2c00, narc_command2_w },
	{ 0x3000, 0x3000, DAC_0_data_w },
	{ 0x3800, 0x3800, narc_master_bank_select_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END



/* NARC slave readmem/writemem structures */
MEMORY_READ_START( williams_narc_slave_readmem )
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x3000, 0x3000, MRA_NOP },
	{ 0x3400, 0x3400, narc_command2_r },
	{ 0x4000, 0xbfff, MRA_BANK5 },
	{ 0xc000, 0xffff, MRA_ROM },
MEMORY_END


MEMORY_WRITE_START( williams_narc_slave_writemem )
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, hc55516_0_clock_set_w },
	{ 0x2400, 0x2400, hc55516_0_digit_clock_clear_w },
	{ 0x3000, 0x3000, DAC_1_data_w },
	{ 0x3800, 0x3800, narc_slave_bank_select_w },
	{ 0x3c00, 0x3c00, MWA_NOP },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END



/* PIA structure */
static struct pia6821_interface cvsd_pia_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ 0, 0, 0, 0, 0, 0,
	/*outputs: A/B,CA/B2       */ DAC_0_data_w, 0, 0, 0,
	/*irqs   : A/B             */ cvsd_irqa, cvsd_irqb
};



/***************************************************************************
	AUDIO STRUCTURES
****************************************************************************/

/* YM2151 structure (CVSD variant) */
static struct YM2151interface cvsd_ym2151_interface =
{
	1,			/* 1 chip */
	3579580,
	{ YM3012_VOL(10,MIXER_PAN_CENTER,10,MIXER_PAN_CENTER) },
	{ cvsd_ym2151_irq }
};


/* YM2151 structure (ADPCM variant) */
static struct YM2151interface adpcm_ym2151_interface =
{
	1,			/* 1 chip */
	3579580,
	{ YM3012_VOL(10,MIXER_PAN_CENTER,10,MIXER_PAN_CENTER) },
	{ adpcm_ym2151_irq }
};


/* DAC structure (single DAC variant) */
static struct DACinterface single_dac_interface =
{
	1,
	{ 50 }
};


/* DAC structure (double DAC variant) */
static struct DACinterface double_dac_interface =
{
	2,
	{ 50, 50 }
};


/* CVSD structure */
static struct hc55516_interface cvsd_interface =
{
	1,			/* 1 chip */
	{ 80 }
};


/* OKIM6295 structure(s) */
static struct OKIM6295interface adpcm_6295_interface =
{
	1,          	/* 1 chip */
	{ 8000 },       /* 8000 Hz frequency */
	{ REGION_SOUND1 },  /* memory */
	{ 50 }
};



/***************************************************************************
	MACHINE DRIVERS
****************************************************************************/

MACHINE_DRIVER_START( williams_cvsd_sound )
	MDRV_CPU_ADD_TAG("cvsd", M6809, 8000000/4)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(williams_cvsd_readmem,williams_cvsd_writemem)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2151, cvsd_ym2151_interface)
	MDRV_SOUND_ADD(DAC,    single_dac_interface)
	MDRV_SOUND_ADD(HC55516,cvsd_interface)
MACHINE_DRIVER_END


MACHINE_DRIVER_START( williams_adpcm_sound )
	MDRV_CPU_ADD_TAG("adpcm", M6809, 8000000/4)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(williams_adpcm_readmem,williams_adpcm_writemem)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2151,  adpcm_ym2151_interface)
	MDRV_SOUND_ADD(DAC,     single_dac_interface)
	MDRV_SOUND_ADD(OKIM6295,adpcm_6295_interface)
MACHINE_DRIVER_END


MACHINE_DRIVER_START( williams_narc_sound )
	MDRV_CPU_ADD_TAG("narc1", M6809, 8000000/4)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(williams_narc_master_readmem,williams_narc_master_writemem)

	MDRV_CPU_ADD_TAG("narc2", M6809, 8000000/4)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(williams_narc_slave_readmem,williams_narc_slave_writemem)

	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2151, adpcm_ym2151_interface)
	MDRV_SOUND_ADD(DAC,    double_dac_interface)
	MDRV_SOUND_ADD(HC55516,cvsd_interface)
MACHINE_DRIVER_END



/***************************************************************************
static INLINES
****************************************************************************/

static INLINE UINT8 *get_cvsd_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1 + sound_cpunum);
	int bank = data & 3;
	int quarter = (data >> 2) & 3;
	if (bank == 3) bank = 0;
	return &RAM[0x10000 + (bank * 0x20000) + (quarter * 0x8000)];
}


static INLINE UINT8 *get_adpcm_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1 + sound_cpunum);
	int bank = data & 7;
	return &RAM[0x10000 + (bank * 0x8000)];
}


static INLINE UINT8 *get_narc_master_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1 + sound_cpunum);
	int bank = data & 3;
	if (!(data & 4)) bank = 0;
	return &RAM[0x10000 + (bank * 0x8000)];
}


static INLINE UINT8 *get_narc_slave_bank_base(int data)
{
	UINT8 *RAM = memory_region(REGION_CPU1 + soundalt_cpunum);
	int bank = data & 7;
	return &RAM[0x10000 + (bank * 0x8000)];
}



/***************************************************************************
	INITIALIZATION
****************************************************************************/

void williams_cvsd_init(int cpunum, int pianum)
{
	/* configure the CPU */
	sound_cpunum = mame_find_cpu_index("cvsd");
	soundalt_cpunum = -1;

	/* configure the PIA */
	williams_pianum = pianum;
	pia_config(pianum, PIA_STANDARD_ORDERING, &cvsd_pia_intf);

	/* initialize the global variables */
	init_audio_state();

	/* reset the chip */
	williams_cvsd_reset_w(1);
	williams_cvsd_reset_w(0);

	/* reset the IRQ state */
	pia_set_input_ca1(williams_pianum, 1);
}


void williams_adpcm_init(int cpunum)
{
	UINT8 *RAM;
	int i;

	/* configure the CPU */
	sound_cpunum = mame_find_cpu_index("adpcm");
	soundalt_cpunum = -1;

	/* install the fixed ROM */
	RAM = memory_region(REGION_CPU1 + sound_cpunum);
	memcpy(&RAM[0xc000], &RAM[0x4c000], 0x4000);

	/* initialize the global variables */
	init_audio_state();

	/* reset the chip */
	williams_adpcm_reset_w(1);
	williams_adpcm_reset_w(0);

	/* find the number of banks in the ADPCM space */
	for (i = 0; i < MAX_SOUND; i++)
		if (Machine->drv->sound[i].sound_type == SOUND_OKIM6295)
		{
			struct OKIM6295interface *intf = (struct OKIM6295interface *)Machine->drv->sound[i].sound_interface;
			adpcm_bank_count = memory_region_length(intf->region[0]) / 0x40000;
		}
}


void williams_narc_init(int cpunum)
{
	UINT8 *RAM;

	/* configure the CPU */
	sound_cpunum = mame_find_cpu_index("narc1");
	soundalt_cpunum = mame_find_cpu_index("narc2");

	/* install the fixed ROM */
	RAM = memory_region(REGION_CPU1 + sound_cpunum);
	memcpy(&RAM[0xc000], &RAM[0x2c000], 0x4000);
	RAM = memory_region(REGION_CPU1 + soundalt_cpunum);
	memcpy(&RAM[0xc000], &RAM[0x4c000], 0x4000);

	/* initialize the global variables */
	init_audio_state();

	/* reset the chip */
	williams_narc_reset_w(1);
	williams_narc_reset_w(0);
}


static void init_audio_state(void)
{
	/* reset the YM2151 state */
	YM2151_sh_reset();

	/* clear all the interrupts */
	williams_sound_int_state = 0;
	if (sound_cpunum != -1)
	{
		cpu_set_irq_line(sound_cpunum, M6809_FIRQ_LINE, CLEAR_LINE);
		cpu_set_irq_line(sound_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
		cpu_set_nmi_line(sound_cpunum, CLEAR_LINE);
	}
	if (soundalt_cpunum != -1)
	{
		cpu_set_irq_line(soundalt_cpunum, M6809_FIRQ_LINE, CLEAR_LINE);
		cpu_set_irq_line(soundalt_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
		cpu_set_nmi_line(soundalt_cpunum, CLEAR_LINE);
	}
}


#if 0
static void locate_audio_hotspot(UINT8 *base, UINT16 start)
{
	int i;
	/* search for the loop that kills performance so we can optimize it */
	for (i = start; i < 0x10000; i++)
	{
		if (base[i + 0] == 0x1a && base[i + 1] == 0x50 &&			/* 1A 50       ORCC  #$0050  */
			base[i + 2] == 0x93 &&									/* 93 xx       SUBD  $xx     */
			base[i + 4] == 0xe3 && base[i + 5] == 0x4c &&			/* E3 4C       ADDD  $000C,U */
			base[i + 6] == 0x9e && base[i + 7] == base[i + 3] &&	/* 9E xx       LDX   $xx     */
			base[i + 8] == 0xaf && base[i + 9] == 0x4c &&			/* AF 4C       STX   $000C,U */
			base[i +10] == 0x1c && base[i +11] == 0xaf)				/* 1C AF       ANDCC #$00AF  */
		{
/*			counter.hotspot_start = i;*/
/*			counter.hotspot_stop = i + 12;*/
			log_cb(RETRO_LOG_DEBUG, LOGPRE "Found hotspot @ %04X", i);
			return;
		}
	}
	log_cb(RETRO_LOG_DEBUG, LOGPRE "Found no hotspot!");
}
#endif



/***************************************************************************
	CVSD IRQ GENERATION CALLBACKS
****************************************************************************/

static void cvsd_ym2151_irq(int state)
{
	pia_set_input_ca1(williams_pianum, !state);
}


static void cvsd_irqa(int state)
{
	cpu_set_irq_line(sound_cpunum, M6809_FIRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}


static void cvsd_irqb(int state)
{
	cpu_set_nmi_line(sound_cpunum, state ? ASSERT_LINE : CLEAR_LINE);
}



/***************************************************************************
	ADPCM IRQ GENERATION CALLBACKS
****************************************************************************/

static void adpcm_ym2151_irq(int state)
{
	cpu_set_irq_line(sound_cpunum, M6809_FIRQ_LINE, state ? ASSERT_LINE : CLEAR_LINE);
}



/***************************************************************************
	CVSD BANK SELECT
****************************************************************************/

static WRITE_HANDLER( cvsd_bank_select_w )
{
	cpu_setbank(6, get_cvsd_bank_base(data));
}



/***************************************************************************
	ADPCM BANK SELECT
****************************************************************************/

static WRITE_HANDLER( adpcm_bank_select_w )
{
	cpu_setbank(6, get_adpcm_bank_base(data));
}


static WRITE_HANDLER( adpcm_6295_bank_select_w )
{
	if (adpcm_bank_count <= 3)
	{
		if (!(data & 0x04))
			OKIM6295_set_bank_base(0, 0x00000);
		else if (data & 0x01)
			OKIM6295_set_bank_base(0, 0x40000);
		else
			OKIM6295_set_bank_base(0, 0x80000);
	}
	else
	{
		data &= 7;
		if (data != 0)
			OKIM6295_set_bank_base(0, (data - 1) * 0x40000);
	}
}



/***************************************************************************
	NARC BANK SELECT
****************************************************************************/

static WRITE_HANDLER( narc_master_bank_select_w )
{
	cpu_setbank(6, get_narc_master_bank_base(data));
}


static WRITE_HANDLER( narc_slave_bank_select_w )
{
	cpu_setbank(5, get_narc_slave_bank_base(data));
}



/***************************************************************************
	PIA INTERFACES
****************************************************************************/

static READ_HANDLER( cvsd_pia_r )
{
	return pia_read(williams_pianum, offset);
}


static WRITE_HANDLER( cvsd_pia_w )
{
	pia_write(williams_pianum, offset, data);
}



/***************************************************************************
	CVSD COMMUNICATIONS
****************************************************************************/

static void williams_cvsd_delayed_data_w(int param)
{
	pia_set_input_b(williams_pianum, param & 0xff);
	pia_set_input_cb1(williams_pianum, param & 0x100);
	pia_set_input_cb2(williams_pianum, param & 0x200);
}


void williams_cvsd_data_w(int data)
{
	timer_set(TIME_NOW, data, williams_cvsd_delayed_data_w);
}


void williams_cvsd_reset_w(int state)
{
	/* going high halts the CPU */
	if (state)
	{
		cvsd_bank_select_w(0, 0);
		init_audio_state();
		cpu_set_reset_line(sound_cpunum, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
		cpu_set_reset_line(sound_cpunum, CLEAR_LINE);
}



/***************************************************************************
	ADPCM COMMUNICATIONS
****************************************************************************/

static READ_HANDLER( adpcm_command_r )
{
	cpu_set_irq_line(sound_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
	williams_sound_int_state = 0;
	return soundlatch_r(0);
}

void williams_adpcm_data_w(int data)
{
	if(nba_jam_playing && options.use_samples) {
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
	else if( (mk_playing_mortal_kombat || mk_playing_mortal_kombat_t) && options.use_samples ) {
		int a = 0;
		bool mk_do_nothing = false;
		bool sa_play_sample = false;
		bool sa_play_original = false;
		bool mk_stop_samples = false;
		
		int sa_left = 0;
		int sa_right = 1;
		bool sa_loop = 1; /* --> 1 == loop, 0 == do not loop.*/

		/* If we are playing the T-Unit version of Mortal Kombat.*/
		if(mk_playing_mortal_kombat_t == true) {
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
		}
		else {
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
	else
		soundlatch_w(0, data & 0xff);
		
	if (!(data & 0x200))
	{
		cpu_set_irq_line(sound_cpunum, M6809_IRQ_LINE, ASSERT_LINE);
		williams_sound_int_state = 1;
	}
}


void williams_adpcm_reset_w(int state)
{
	/* going high halts the CPU */
	if (state)
	{
		adpcm_bank_select_w(0, 0);
		init_audio_state();
		cpu_set_reset_line(sound_cpunum, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
		cpu_set_reset_line(sound_cpunum, CLEAR_LINE);
}



/***************************************************************************
	NARC COMMUNICATIONS
****************************************************************************/

static READ_HANDLER( narc_command_r )
{
	cpu_set_nmi_line(sound_cpunum, CLEAR_LINE);
	cpu_set_irq_line(sound_cpunum, M6809_IRQ_LINE, CLEAR_LINE);
	williams_sound_int_state = 0;
	return soundlatch_r(0);
}


void williams_narc_data_w(int data)
{
	soundlatch_w(0, data & 0xff);
	if (!(data & 0x100))
		cpu_set_nmi_line(sound_cpunum, ASSERT_LINE);
	if (!(data & 0x200))
	{
		cpu_set_irq_line(sound_cpunum, M6809_IRQ_LINE, ASSERT_LINE);
		williams_sound_int_state = 1;
	}
}


void williams_narc_reset_w(int state)
{
	/* going high halts the CPU */
	if (state)
	{
		narc_master_bank_select_w(0, 0);
		narc_slave_bank_select_w(0, 0);
		init_audio_state();
		cpu_set_reset_line(sound_cpunum, ASSERT_LINE);
		cpu_set_reset_line(soundalt_cpunum, ASSERT_LINE);
	}
	/* going low resets and reactivates the CPU */
	else
	{
		cpu_set_reset_line(sound_cpunum, CLEAR_LINE);
		cpu_set_reset_line(soundalt_cpunum, CLEAR_LINE);
	}
}


static READ_HANDLER( narc_command2_r )
{
	cpu_set_irq_line(soundalt_cpunum, M6809_FIRQ_LINE, CLEAR_LINE);
	return soundlatch2_r(0);
}


static WRITE_HANDLER( narc_command2_w )
{
	soundlatch2_w(0, data & 0xff);
	cpu_set_irq_line(soundalt_cpunum, M6809_FIRQ_LINE, ASSERT_LINE);
}
