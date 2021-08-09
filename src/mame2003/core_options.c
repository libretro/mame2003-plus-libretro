/*********************************************************************

  core_options.c

  core option configurations used by mame2003-plus

*********************************************************************/

#include "mame.h"
#include "driver.h"
#include <libretro.h>
#include <string/stdstring.h>

static struct retro_variable_default  default_options[OPT_end + 1];    /* need the plus one for the NULL entries at the end */
static struct retro_variable          current_options[OPT_end + 1];


/******************************************************************************

  private function prototypes

******************************************************************************/

static void   init_default(struct retro_variable_default *option, const char *key, const char *value);
static void   set_variables(bool first_time);
static struct retro_variable_default *spawn_effective_option(int option_index);


/******************************************************************************

  external function prototypes and variables

******************************************************************************/

/* found in mame2003.c */
extern void   retro_set_audio_buff_status_cb(void);
extern void   retro_describe_controls(void);

extern const struct GameDriver *game_driver;
extern retro_set_led_state_t led_state_cb;


/* used in inptport.c when saving input port settings */
int legacy_flag = -1;


/*
 * Note that core options are not presented in order they are initialized here,
 * but rather by their order in the OPT_ enum found in mame2003.h
 */
void init_core_options(void)
{
  init_default(&default_options[OPT_4WAY],                   APPNAME"_four_way_emulation",     "4-way joystick emulation on 8-way joysticks; disabled|enabled");
#if defined(__IOS__)
  init_default(&default_options[OPT_MOUSE_DEVICE],           APPNAME"_mouse_device",           "X-Y Device; pointer|mouse|lightgun|disabled");
#else
  init_default(&default_options[OPT_MOUSE_DEVICE],           APPNAME"_mouse_device",           "X-Y Device; mouse|pointer|lightgun|disabled");
#endif
  init_default(&default_options[OPT_CROSSHAIR_ENABLED],      APPNAME"_crosshair_enabled",      "Show Lightgun crosshairs; enabled|disabled");
  init_default(&default_options[OPT_SKIP_DISCLAIMER],        APPNAME"_skip_disclaimer",        "Skip Disclaimer; disabled|enabled");
  init_default(&default_options[OPT_SKIP_WARNINGS],          APPNAME"_skip_warnings",          "Skip Warnings; disabled|enabled");
  init_default(&default_options[OPT_DISPLAY_SETUP],          APPNAME"_display_setup",          "Display MAME menu; disabled|enabled");
  init_default(&default_options[OPT_BRIGHTNESS],             APPNAME"_brightness",             "Brightness; 1.0|0.2|0.3|0.4|0.5|0.6|0.7|0.8|0.9|1.1|1.2|1.3|1.4|1.5|1.6|1.7|1.8|1.9|2.0");
  init_default(&default_options[OPT_GAMMA],                  APPNAME"_gamma",                  "Gamma correction; 1.0|0.5|0.6|0.7|0.8|0.9|1.1|1.2|1.3|1.4|1.5|1.6|1.7|1.8|1.9|2.0");
  init_default(&default_options[OPT_ARTWORK],                APPNAME"_display_artwork",        "Display artwork (Restart core); enabled|disabled");
  init_default(&default_options[OPT_ART_RESOLUTION],         APPNAME"_art_resolution",         "Artwork resolution multiplier (Restart core); 1|2|3|4|5|6|7|8");
  init_default(&default_options[OPT_ART_OVERLAY_OPACITY],    APPNAME"_art_overlay_opacity",    "Artwork hardcoded overlay opacity (Restart core); default|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|50|70");
  init_default(&default_options[OPT_NEOGEO_BIOS],            APPNAME"_neogeo_bios",            "Specify Neo Geo BIOS (Restart core); default|euro|euro-s1|us|us-e|asia|japan|japan-s2|unibios40|unibios33|unibios20|unibios13|unibios11|unibios10|debug|asia-aes");
  init_default(&default_options[OPT_STV_BIOS],               APPNAME"_stv_bios",               "Specify Sega ST-V BIOS (Restart core); default|japan|japana|us|japan_b|taiwan|europe");
  init_default(&default_options[OPT_USE_ALT_SOUND],          APPNAME"_use_alt_sound",          "Use CD soundtrack (Restart core); disabled|enabled");
  init_default(&default_options[OPT_SHARE_DIAL],             APPNAME"_dialsharexy",            "Share 2 player dial controls across one X/Y device; disabled|enabled");
  init_default(&default_options[OPT_DEADZONE],               APPNAME"_deadzone",               "Analog deadzone; 20|0|5|10|15|25|30|35|40|45|50|55|60|65|70|75|80|85|90|95");
  init_default(&default_options[OPT_TATE_MODE],              APPNAME"_tate_mode",              "TATE Mode - Rotating display (Restart core); disabled|enabled");
  init_default(&default_options[OPT_VECTOR_RESOLUTION],      APPNAME"_vector_resolution",      "Vector resolution (Restart core); 1024x768|640x480|1280x960|1440x1080|1600x1200|1707x1280|original");
  init_default(&default_options[OPT_VECTOR_ANTIALIAS],       APPNAME"_vector_antialias",       "Vector antialiasing; enabled|disabled");
  init_default(&default_options[OPT_VECTOR_BEAM],            APPNAME"_vector_beam_width",      "Vector beam width (only with antialiasing); 2|1|1.2|1.4|1.6|1.8|2.5|3|4|5|6|7|8|9|10|11|12");
  init_default(&default_options[OPT_VECTOR_TRANSLUCENCY],    APPNAME"_vector_translucency",    "Vector translucency; enabled|disabled");
  init_default(&default_options[OPT_VECTOR_FLICKER],         APPNAME"_vector_flicker",         "Vector flicker; 20|0|10|30|40|50|60|70|80|90|100");
  init_default(&default_options[OPT_VECTOR_INTENSITY],       APPNAME"_vector_intensity",       "Vector intensity; 1.5|0.5|1|2|2.5|3");
  init_default(&default_options[OPT_NVRAM_BOOTSTRAP],        APPNAME"_nvram_bootstraps",       "NVRAM Bootstraps; enabled|disabled");
  init_default(&default_options[OPT_SAMPLE_RATE],            APPNAME"_sample_rate",            "Sample Rate (KHz); 48000|8000|11025|22050|30000|44100|");
  init_default(&default_options[OPT_INPUT_INTERFACE],        APPNAME"_input_interface",        "Input interface; simultaneous|retropad|keyboard");
  init_default(&default_options[OPT_MAME_REMAPPING],         APPNAME"_mame_remapping",         "Legacy Remapping (Restart core); enabled|disabled");
  init_default(&default_options[OPT_FRAMESKIP],              APPNAME"_frameskip",              "Frameskip; disabled|1|2|3|4|5|6|7|9|10|11|auto|auto_aggressive|auto_max");
  init_default(&default_options[OPT_CORE_SYS_SUBFOLDER],     APPNAME"_core_sys_subfolder",     "Locate system files within a subfolder; enabled|disabled"); /* This should be probably handled by the frontend and not by cores per discussions in Fall 2018 but RetroArch for example doesn't provide this as an option. */
  init_default(&default_options[OPT_CORE_SAVE_SUBFOLDER],    APPNAME"_core_save_subfolder",    "Locate save files within a subfolder; enabled|disabled"); /* This is already available as an option in RetroArch although it is left enabled by default as of November 2018 for consistency with past practice. At least for now.*/
  init_default(&default_options[OPT_AUTOSAVE_HISCORE],       APPNAME"_autosave_hiscore",       "Autosave hiscore; default|recursively");
  init_default(&default_options[OPT_CHEAT_INPUT_PORTS],      APPNAME"_cheat_input_ports",      "Dip switch/Cheat input ports; disabled|enabled");
  init_default(&default_options[OPT_MACHINE_TIMING],         APPNAME"_machine_timing",         "Bypass audio skew (Restart core); enabled|disabled");
  init_default(&default_options[OPT_DIGITAL_JOY_CENTERING],  APPNAME"_digital_joy_centering",  "Center joystick axis for digital controls; enabled|disabled");
  init_default(&default_options[OPT_CPU_CLOCK_SCALE],        APPNAME"_cpu_clock_scale",        "CPU clock scale; default|25|30|35|40|45|50|55|60|65|70|75|80|85|90|95|105|110|115|120|125");
#if (HAS_CYCLONE || HAS_DRZ80)
  init_default(&default_options[OPT_CYCLONE_MODE],           APPNAME"_cyclone_mode",           "Cyclone mode (Restart core); default|disabled|Cyclone|DrZ80|Cyclone+DrZ80|DrZ80(snd)|Cyclone+DrZ80(snd)");
#endif
  init_default(&default_options[OPT_end], NULL, NULL);
  set_variables(true);
}

static void set_variables(bool first_time)
{
  static struct retro_variable_default  effective_defaults[OPT_end + 1];
  static unsigned effective_options_count;         /* the number of core options in effect for the current content */
  int option_index   = 0;

  for(option_index = 0; option_index < (OPT_end + 1); option_index++)
  {
    switch(option_index)
    {
      case OPT_4WAY:
         if(options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] != 4)
           continue;
         break;
      case OPT_CROSSHAIR_ENABLED:
         if(!options.content_flags[CONTENT_LIGHTGUN])
           continue;
         break;
      case OPT_STV_BIOS:
         if(!options.content_flags[CONTENT_STV])
           continue; /* only offer BIOS selection when it is relevant */
         break;
      case OPT_NEOGEO_BIOS:
          if(!options.content_flags[CONTENT_NEOGEO])
            continue; /* only offer BIOS selection when it is relevant */
          break;
      case OPT_USE_ALT_SOUND:
         if(!options.content_flags[CONTENT_ALT_SOUND])
           continue;
         break;
      case OPT_SHARE_DIAL:
         if(!options.content_flags[CONTENT_DIAL])
           continue;
         break;
      case OPT_VECTOR_RESOLUTION:
      case OPT_VECTOR_ANTIALIAS:
      case OPT_VECTOR_TRANSLUCENCY:
      case OPT_VECTOR_BEAM:
      case OPT_VECTOR_FLICKER:
      case OPT_VECTOR_INTENSITY:
         if(!options.content_flags[CONTENT_VECTOR])
           continue;
         break;
      case OPT_NVRAM_BOOTSTRAP:
         if(!options.content_flags[CONTENT_NVRAM_BOOTSTRAP])
           continue;
         break;


   }
   effective_defaults[effective_options_count] = first_time ? default_options[option_index] : *spawn_effective_option(option_index);
   effective_options_count++;
  }

  environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)effective_defaults);

}

static struct retro_variable_default *spawn_effective_option(int option_index)
{
  static struct retro_variable_default *encoded_option = NULL;

  /* implementing this function will allow the core to change a core option within the core and then report that that change to the frontend
   * currently core options only flow one way: from the frontend to mame2003-plus
   *
   * search for the string "; " as the delimiter between the option display name and the values
   * stringify the current value for this option
   * see if the current option string is already listed first in the original default --
   *    if the current selected option is not in the original defaults string at all
   *      log an error message and bail. that shouldn't be possible.
   *    is the currently selected option the first in the default pipe-delimited list?
   *      if so, just return default_options[option_index]
   *    else
   *       create a copy of default_options[option_index].defaults_string.
   *       First add the stringified current option as the first in the pipe-delimited list for this copied string
   *       then remove the option from wherever it was originally in the defaults string
   */
  return encoded_option;
}

static void init_default(struct retro_variable_default *def, const char *key, const char *label_and_values)
{
  def->key = key;
  def->defaults_string = label_and_values;
}

void update_variables(bool first_time)
{
  struct retro_led_interface ledintf;
  struct retro_variable var;
  int index;
  bool reset_control_descriptions = false;

  for(index = 0; index < OPT_end; index++)
  {
    var.value = NULL;
    var.key = default_options[index].key;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && !string_is_empty(var.value)) /* the frontend sends a value for this core option */
    {
      current_options[index].value = var.value; /* keep the state of core options matched with the frontend */

      switch(index)
      {
        case OPT_INPUT_INTERFACE:
          if(strcmp(var.value, "retropad") == 0)
            options.input_interface = RETRO_DEVICE_JOYPAD;
          else if(strcmp(var.value, "keyboard") == 0)
            options.input_interface = RETRO_DEVICE_KEYBOARD;
          else
            options.input_interface = RETRO_DEVICE_KEYBOARD + RETRO_DEVICE_JOYPAD;
          break;

        case OPT_4WAY:
          if( (strcmp(var.value, "enabled") == 0) && (options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] == 4) )
          {
            if(!options.restrict_4_way)           /* the option has just been toggled to "enabled" */
            {
              options.restrict_4_way = true;
              reset_control_descriptions = true;  /* games with rotated joysticks send different control descriptions in 4-way restrictor mode */
            }
          }
          else
          {
            if(options.restrict_4_way)            /* the option has just been toggled to "disabled" */
            {
              options.restrict_4_way = false;
              reset_control_descriptions = true;  /* games with rotated joysticks send different control descriptions in 4-way restrictor mode */
            }
          }
          break;

        case OPT_MOUSE_DEVICE:
          if(strcmp(var.value, "pointer") == 0)
            options.mouse_device = RETRO_DEVICE_POINTER;
          else if(strcmp(var.value, "mouse") == 0)
            options.mouse_device = RETRO_DEVICE_MOUSE;
          else if(strcmp(var.value, "lightgun") == 0)
            options.mouse_device = RETRO_DEVICE_LIGHTGUN;
          else
            options.mouse_device = RETRO_DEVICE_NONE;
          break;

        case OPT_CROSSHAIR_ENABLED:
          if(strcmp(var.value, "enabled") == 0)
            options.crosshair_enable = 1;
          else
            options.crosshair_enable = 0;
          break;

        case OPT_SKIP_DISCLAIMER:
          if(strcmp(var.value, "enabled") == 0)
            options.skip_disclaimer = true;
          else
            options.skip_disclaimer = false;
          break;

        case OPT_SKIP_WARNINGS:
          if(strcmp(var.value, "enabled") == 0)
            options.skip_warnings = true;
          else
            options.skip_warnings = false;
          break;

        case OPT_DISPLAY_SETUP:
          if(strcmp(var.value, "enabled") == 0)
            options.display_setup = 1;
          else
            options.display_setup = 0;
          break;

        case OPT_BRIGHTNESS:
          options.brightness = atof(var.value);
          if(!first_time)
            palette_set_global_brightness(options.brightness);
          break;

        case OPT_GAMMA:
          options.gamma = atof(var.value);
          if(!first_time)
            palette_set_global_gamma(options.gamma);
          break;

        case OPT_ARTWORK:
          if(strcmp(var.value, "enabled") == 0)
            options.use_artwork = ARTWORK_USE_ALL;
          else
            options.use_artwork = ARTWORK_USE_NONE;
          break;

        case OPT_ART_RESOLUTION:
          options.artwork_res = atoi(var.value);
          break;

        case OPT_ART_OVERLAY_OPACITY:
          if(strcmp(var.value, "default") == 0)
            options.overlay_opacity = ARTWORK_OVERLAY_DEFAULT;
          else {
            options.overlay_opacity = atoi(var.value);
            if (options.overlay_opacity < 0 )
              options.overlay_opacity = 0;
            else if (options.overlay_opacity > 255)
              options.overlay_opacity = 255;
          }
          break;

        case OPT_STV_BIOS:
          if(!options.content_flags[CONTENT_STV])
            break;
          if(options.content_flags[CONTENT_DIEHARD]) /* catch required bios for this one game. */
            options.bios = "us";
          else
            options.bios = (strcmp(var.value, "default") == 0) ? NULL : var.value;
          break;

        case OPT_NEOGEO_BIOS:
          if(!options.content_flags[CONTENT_NEOGEO])
            break;
          options.bios = (strcmp(var.value, "default") == 0) ? NULL : var.value;
          break;

        case OPT_USE_ALT_SOUND:
          if(options.content_flags[CONTENT_ALT_SOUND])
          {
            if(strcmp(var.value, "enabled") == 0)
              options.use_samples = true;
            else
              options.use_samples = false;
          }
          break;

        case OPT_SHARE_DIAL:
          if(options.content_flags[CONTENT_DIAL])
          {
            if(strcmp(var.value, "enabled") == 0)
              options.dial_share_xy = 1;
            else
              options.dial_share_xy = 0;
            break;
          }
          else
          {
            options.dial_share_xy = 0;
            break;
          }

        case OPT_DEADZONE:
            options.deadzone = atoi(var.value);
          break;

        case OPT_TATE_MODE:
          if(strcmp(var.value, "enabled") == 0)
            options.tate_mode = 1;
          else
            options.tate_mode = 0;
          break;

        case OPT_DIGITAL_JOY_CENTERING:
          if(strcmp(var.value, "enabled") == 0)
            options.digital_joy_centering = 1;
          else
            options.digital_joy_centering = 0;
          break;

        case OPT_VECTOR_RESOLUTION:
          {
            int width = 0;
            int height = 0;
            sscanf(var.value, "%dx%d", &width, &height);
            // if they are still 0, mame will set from driver resolution set
            options.vector_width = width;
            options.vector_height = height;
          }
          break;

        case OPT_VECTOR_ANTIALIAS:
          if(strcmp(var.value, "enabled") == 0)
            options.antialias = 1; /* integer: 1 to enable antialiasing on vectors _ does not work as of 2018/04/17*/
          else
            options.antialias = 0;
          break;

        case OPT_VECTOR_BEAM:
          options.beam = atof(var.value); /* float: vector beam width */
          break;

        case OPT_VECTOR_TRANSLUCENCY:
          if(strcmp(var.value, "enabled") == 0)
            options.translucency = 1; /* integer: 1 to enable translucency on vectors */
          else
            options.translucency = 0;
          break;

        case OPT_VECTOR_FLICKER:
          options.vector_flicker = atof(var.value);
          break;

        case OPT_VECTOR_INTENSITY:
          options.vector_intensity_correction = atof(var.value); /* float: vector beam intensity */
          break;

        case OPT_NVRAM_BOOTSTRAP:
          if(strcmp(var.value, "enabled") == 0)
            options.nvram_bootstrap = true;
          else
            options.nvram_bootstrap = false;
          break;

        case OPT_SAMPLE_RATE:
          options.samplerate = atoi(var.value);
          break;

        case OPT_MAME_REMAPPING:
          if(strcmp(var.value, "enabled") == 0)
          {
            if( !options.mame_remapping && legacy_flag != -1) legacy_flag =0;
            options.mame_remapping = true;
          }
          else
          {
            if( options.mame_remapping && legacy_flag != -1) legacy_flag =0;
            options.mame_remapping = false;
          }
          if(!first_time)
            setup_menu_init();
          if(legacy_flag ==-1) legacy_flag = 1;
          break;

        case OPT_FRAMESKIP:
          if (strcmp(var.value, "auto") == 0)
            options.frameskip = 12;
          else if (strcmp(var.value, "auto_aggressive") == 0)
            options.frameskip = 13;
          else if(strcmp(var.value, "auto_max") == 0)
            options.frameskip = 14;
          else
            options.frameskip = atoi(var.value);

          retro_set_audio_buff_status_cb();
          break;

        case OPT_CPU_CLOCK_SCALE:
          if(strcmp(var.value, "default") == 0)
            options.cpu_clock_scale = 1;
          else
            options.cpu_clock_scale = (double) atoi(var.value) / 100;
          break;

        case OPT_CORE_SYS_SUBFOLDER:
          if(strcmp(var.value, "enabled") == 0)
            options.system_subfolder = true;
          else
            options.system_subfolder = false;
          break;

        case OPT_CORE_SAVE_SUBFOLDER:
          if(strcmp(var.value, "enabled") == 0)
            options.save_subfolder = true;
          else
            options.save_subfolder = false;
          break;

        case OPT_AUTOSAVE_HISCORE:
          if(strcmp(var.value, "recursively") == 0)
            options.autosave_hiscore = true;
          else
            options.autosave_hiscore = false;
          break;

        case OPT_CHEAT_INPUT_PORTS:
          if(strcmp(var.value, "enabled") == 0)
            options.cheat_input_ports = true;
          else
            options.cheat_input_ports = false;
          break;

        case OPT_MACHINE_TIMING:
          if(strcmp(var.value, "enabled") == 0)
            options.machine_timing = true;
          else
            options.machine_timing = false;
          break;

#if (HAS_CYCLONE || HAS_DRZ80)
        case OPT_CYCLONE_MODE:
          if(strcmp(var.value, "default") == 0)
            options.cyclone_mode = 1;
          else if(strcmp(var.value, "Cyclone") == 0)
            options.cyclone_mode = 2;
          else if(strcmp(var.value, "DrZ80") == 0)
            options.cyclone_mode = 3;
          else if(strcmp(var.value, "Cyclone+DrZ80") == 0)
            options.cyclone_mode = 4;
          else if(strcmp(var.value, "DrZ80(snd)") == 0)
            options.cyclone_mode = 5;
          else if(strcmp(var.value, "Cyclone+DrZ80(snd)") == 0)
            options.cyclone_mode = 6;
          else /* disabled */
            options.cyclone_mode = 0;
          break;
#endif
      }
    }
  }

  if(!options.content_flags[CONTENT_ALT_SOUND])
    options.use_samples = true;

  ledintf.set_led_state = NULL;
  environ_cb(RETRO_ENVIRONMENT_GET_LED_INTERFACE, &ledintf);
  led_state_cb = ledintf.set_led_state;

  if(reset_control_descriptions) /* one of the option changes has flagged a need to re-describe the controls */
  {
    retro_describe_controls();
    reset_control_descriptions = false;
  }
}

void set_content_flags(void)
{
  int i = 0;

  extern struct GameDriver driver_neogeo;
  extern struct GameDriver driver_stvbios;
  const struct InputPortTiny *input = game_driver->input_ports;


  /************ DRIVERS WITH ALTERNATE SOUNDTRACKS ************/
  for( i = 0; Machine->drv->sound[i].sound_type && i < MAX_SOUND; i++ )
  {
    if (Machine->drv->sound[i].tag)
      if (strcmp("OST Samples",  Machine->drv->sound[i].tag) == 0)
      {
        options.content_flags[CONTENT_ALT_SOUND] = true;
      }
  }

  /************ DRIVERS WITH MULTIPLE BIOS OPTIONS ************/
  if (game_driver->clone_of == &driver_neogeo
   ||(game_driver->clone_of && game_driver->clone_of->clone_of == &driver_neogeo))
  {
    options.content_flags[CONTENT_NEOGEO] = true;
  }
  else if (game_driver->clone_of == &driver_stvbios
   ||(game_driver->clone_of && game_driver->clone_of->clone_of == &driver_stvbios))
  {
    options.content_flags[CONTENT_STV] = true;
  }

  /************ DIE HARD: ARCADE ************/
  if(strcasecmp(game_driver->name, "diehard") == 0)
    options.content_flags[CONTENT_DIEHARD] = true;

  /************ DRIVERS WITH VECTOR VIDEO DISPLAYS ************/
  if(Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
    options.content_flags[CONTENT_VECTOR] = true;

  /************ INPUT-BASED CONTENT FLAGS ************/
  options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] = 8; /* default behavior is 8-way joystick, even for 2-way games */

	while ((input->type & ~IPF_MASK) != IPT_END)
	{
		/* skip analog extension fields */
		if ((input->type & ~IPF_MASK) != IPT_EXTENSION)
		{
			switch (input->type & IPF_PLAYERMASK)
			{
				case IPF_PLAYER1:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 1) options.content_flags[CONTENT_PLAYER_COUNT] = 1;
					break;
				case IPF_PLAYER2:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 2) options.content_flags[CONTENT_PLAYER_COUNT] = 2;
					break;
				case IPF_PLAYER3:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 3) options.content_flags[CONTENT_PLAYER_COUNT] = 3;
					break;
				case IPF_PLAYER4:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 4) options.content_flags[CONTENT_PLAYER_COUNT] = 4;
					break;
				case IPF_PLAYER5:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 5) options.content_flags[CONTENT_PLAYER_COUNT] = 5;
					break;
				case IPF_PLAYER6:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 6) options.content_flags[CONTENT_PLAYER_COUNT] = 6;
					break;
				case IPF_PLAYER7:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 7) options.content_flags[CONTENT_PLAYER_COUNT] = 7;
					break;
				case IPF_PLAYER8:
					if (options.content_flags[CONTENT_PLAYER_COUNT] < 8) options.content_flags[CONTENT_PLAYER_COUNT] = 8;
					break;
			}

			if (input->type & IPF_4WAY) /* the controls use a 4-way joystick */
				options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] = 4;

			switch (input->type & ~IPF_MASK)
			{
				case IPT_JOYSTICKRIGHT_UP:
				case IPT_JOYSTICKRIGHT_DOWN:
				case IPT_JOYSTICKRIGHT_LEFT:
				case IPT_JOYSTICKRIGHT_RIGHT:
				case IPT_JOYSTICKLEFT_UP:
				case IPT_JOYSTICKLEFT_DOWN:
				case IPT_JOYSTICKLEFT_LEFT:
				case IPT_JOYSTICKLEFT_RIGHT:
					options.content_flags[CONTENT_DUAL_JOYSTICK] = true;
					break;
				case IPT_BUTTON1:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 1) options.content_flags[CONTENT_BUTTON_COUNT] = 1;
					break;
				case IPT_BUTTON2:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 2) options.content_flags[CONTENT_BUTTON_COUNT] = 2;
					break;
				case IPT_BUTTON3:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 3) options.content_flags[CONTENT_BUTTON_COUNT] = 3;
					break;
				case IPT_BUTTON4:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 4) options.content_flags[CONTENT_BUTTON_COUNT] = 4;
					break;
				case IPT_BUTTON5:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 5) options.content_flags[CONTENT_BUTTON_COUNT] = 5;
					break;
				case IPT_BUTTON6:
					if (options.content_flags[CONTENT_BUTTON_COUNT] <6 ) options.content_flags[CONTENT_BUTTON_COUNT] = 6;
					break;
				case IPT_BUTTON7:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 7) options.content_flags[CONTENT_BUTTON_COUNT] = 7;
					break;
				case IPT_BUTTON8:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 8) options.content_flags[CONTENT_BUTTON_COUNT] = 8;
					break;
				case IPT_BUTTON9:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 9) options.content_flags[CONTENT_BUTTON_COUNT] = 9;
					break;
				case IPT_BUTTON10:
					if (options.content_flags[CONTENT_BUTTON_COUNT] < 10) options.content_flags[CONTENT_BUTTON_COUNT] = 10;
					break;
				case IPT_PADDLE:
					options.content_flags[CONTENT_PADDLE] = true;
					break;
				case IPT_DIAL:
					options.content_flags[CONTENT_DIAL] = true;
					break;
				case IPT_TRACKBALL_X:
				case IPT_TRACKBALL_Y:
					options.content_flags[CONTENT_TRACKBALL] = true;
					break;
				case IPT_AD_STICK_X:
				case IPT_AD_STICK_Y:
					options.content_flags[CONTENT_AD_STICK] = true;
					break;
				case IPT_LIGHTGUN_X:
				case IPT_LIGHTGUN_Y:
					options.content_flags[CONTENT_LIGHTGUN] = true;
					break;
				case IPT_SERVICE :
					options.content_flags[CONTENT_HAS_SERVICE] = true;
					break;
				case IPT_TILT :
					options.content_flags[CONTENT_HAS_TILT] = true;
					break;
			}
		}
		++input;
	}

  /************ DRIVERS FLAGGED IN CONTROLS.C WITH ALTERNATING CONTROLS ************/
  if(game_driver->ctrl_dat->alternating_controls)
  {
    options.content_flags[CONTENT_ALTERNATING_CTRLS] = true;
  }

  /************ NUMBER OF DISTINCT CONTROL LAYOUTS ************/
  options.content_flags[CONTENT_CTRL_COUNT] = options.content_flags[CONTENT_PLAYER_COUNT];

  /* There may be a future need to have a ctrl_count different than player_count,
   * perhaps because of some alternating controls layout. This is a place to check
   * some condition and make the two numbers different if that should ever prove useful.
   */
  if(false/*options.content_flags[CONTENT_ALTERNATING_CTRLS]*/)
  {
    options.content_flags[CONTENT_CTRL_COUNT] = 1;
  }

  /************ DRIVERS FLAGGED IN CONTROLS.C WITH 45-DEGREE JOYSTICK ROTATION ************/
  if(game_driver->ctrl_dat->rotate_joy_45)
    options.content_flags[CONTENT_ROTATE_JOY_45] = true;

  /************ DRIVERS FLAGGED IN CONTROLS.C WITH MIRRORED CONTROLS ************/
  if(game_driver->ctrl_dat->mirrored_controls)
    options.content_flags[CONTENT_MIRRORED_CTRLS] = true;

  /************ DRIVERS WITH NVRAM BOOTSTRAP PATCHES ************/
  if(game_driver->bootstrap != NULL)
    options.content_flags[CONTENT_NVRAM_BOOTSTRAP] = true;


  /************ LOG THE STATE OF THE CONTENT FLAGS ************/

  log_cb(RETRO_LOG_INFO, LOGPRE "==== BEGIN DRIVER CONTENT ATTRIBUTES ====\n");

  if(options.content_flags[CONTENT_NEOGEO])     log_cb(RETRO_LOG_INFO, LOGPRE "* Neo Geo BIOS required.\n");
  if(options.content_flags[CONTENT_STV])        log_cb(RETRO_LOG_INFO, LOGPRE "* STV BIOS required.\n");
  if(options.content_flags[CONTENT_DIEHARD])    log_cb(RETRO_LOG_INFO, LOGPRE "* Die Hard: Arcade BIOS required.\n");
  if(options.content_flags[CONTENT_ALT_SOUND])  log_cb(RETRO_LOG_INFO, LOGPRE "* Alternative soundtrack available.\n");
  if(options.content_flags[CONTENT_VECTOR])     log_cb(RETRO_LOG_INFO, LOGPRE "* Vector display.\n");

  log_cb(RETRO_LOG_INFO, LOGPRE "* Supports %i players with %i distinct controls.\n", options.content_flags[CONTENT_PLAYER_COUNT], options.content_flags[CONTENT_CTRL_COUNT]);
  log_cb(RETRO_LOG_INFO, LOGPRE "* Supports %i distinct button controls.\n", options.content_flags[CONTENT_BUTTON_COUNT]);

  if(options.content_flags[CONTENT_DIAL])               log_cb(RETRO_LOG_INFO, LOGPRE "* Uses a dial.\n");
  if(options.content_flags[CONTENT_TRACKBALL])          log_cb(RETRO_LOG_INFO, LOGPRE "* Uses a trackball.\n");
  if(options.content_flags[CONTENT_LIGHTGUN])           log_cb(RETRO_LOG_INFO, LOGPRE "* Uses a lightgun.\n");
  if(options.content_flags[CONTENT_PADDLE])             log_cb(RETRO_LOG_INFO, LOGPRE "* Uses an paddle.\n");
  if(options.content_flags[CONTENT_AD_STICK])           log_cb(RETRO_LOG_INFO, LOGPRE "* Uses an analog joystick.\n");
  if(options.content_flags[CONTENT_HAS_SERVICE])        log_cb(RETRO_LOG_INFO, LOGPRE "* Uses a service button.\n");
  if(options.content_flags[CONTENT_HAS_TILT])           log_cb(RETRO_LOG_INFO, LOGPRE "* Uses a tilt function.\n");

  if(options.content_flags[CONTENT_ALTERNATING_CTRLS])  log_cb(RETRO_LOG_INFO, LOGPRE "* Uses alternating controls.\n");
  if(options.content_flags[CONTENT_MIRRORED_CTRLS])     log_cb(RETRO_LOG_INFO, LOGPRE "* Uses multiplayer control labels.\n");
  if(options.content_flags[CONTENT_ROTATE_JOY_45])      log_cb(RETRO_LOG_INFO, LOGPRE "* Uses joysticks rotated 45-degrees with respect to the cabinet.\n");
  if(options.content_flags[CONTENT_DUAL_JOYSTICK])      log_cb(RETRO_LOG_INFO, LOGPRE "* Uses dual joysticks.\n");
  if(options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] == 4)
      log_cb(RETRO_LOG_INFO, LOGPRE "* Uses 4-way joystick controls.\n");
  else
      log_cb(RETRO_LOG_INFO, LOGPRE "* Uses 8-way joystick controls.\n");

  if(options.content_flags[CONTENT_NVRAM_BOOTSTRAP])    log_cb(RETRO_LOG_INFO, LOGPRE "* Uses an NVRAM bootstrap controlled via core option.\n");

  log_cb(RETRO_LOG_INFO, LOGPRE "==== END DRIVER CONTENT ATTRIBUTES ====\n");
}
