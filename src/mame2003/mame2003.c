/*********************************************************************

	mame2003.c

    an updated port of Xmame 0.78 to the libretro API

*********************************************************************/

#include <stdint.h>
#include <string/stdstring.h>
#include <libretro.h>
#include <file/file_path.h>
#include <math.h>


#include "mame.h"
#include "driver.h"
#include "state.h"
#include "log.h"
#include "input.h"
#include "inptport.h"
#include "fileio.h"
#include "controls.h"
#include "usrintrf.h"

int  pressure_check =  1.28 * 20;
int convert_analog_scale(int input);

int gotFrame;
static const struct GameDriver  *game_driver;
static float              delta_samples;
int                       samples_per_frame = 0;
int                       orig_samples_per_frame =0;
short*                    samples_buffer;
short*                    conversion_buffer;
int                       usestereo = 1;
int16_t                   prev_pointer_x;
int16_t                   prev_pointer_y;
unsigned                  retroColorMode;
unsigned long             lastled = 0;

extern const struct KeyboardInfo retroKeys[];
extern int          retroKeyState[512];
int                 retroJsState[156]= {0}; // initialise to zero - we are only reading 4 players atm map for 6 (6*26)
int16_t             mouse_x[4]= {0};
int16_t             mouse_y[4]= {0};
int16_t             analogjoy[4][4]= {0};
struct ipd          *default_inputs; /* pointer the array of structs with default MAME input mappings and labels */
int                 running = 0;
int                 control_flag=-1;
int                 legacy_flag=-1;    
static struct retro_input_descriptor empty[] = { { 0 } };

retro_log_printf_t                 log_cb;
static struct retro_message        frontend_message;

struct                             retro_perf_callback perf_cb;
retro_environment_t                environ_cb                    = NULL;
retro_video_refresh_t              video_cb                      = NULL;
static retro_input_poll_t          poll_cb                       = NULL;
static retro_input_state_t         input_cb                      = NULL;
static retro_audio_sample_batch_t  audio_batch_cb                = NULL;
retro_set_led_state_t              led_state_cb                  = NULL;


/******************************************************************************

Core options

******************************************************************************/

enum CORE_OPTIONS/* controls the order in which core options appear. common, important, and content-specific options should go earlier on the list */
{
  OPT_4WAY = 0,
  OPT_MOUSE_DEVICE,
  OPT_CROSSHAIR_ENABLED,
  OPT_SKIP_DISCLAIMER,
  OPT_SKIP_WARNINGS,
  OPT_DISPLAY_SETUP,
  OPT_NEOGEO_BIOS,
  OPT_STV_BIOS,
  OPT_USE_ALT_SOUND,
  OPT_SHARE_DIAL,
  OPT_DPAD_ANALOG,
  OPT_DEADZONE,
  OPT_VECTOR_RESOLUTION,
  OPT_VECTOR_ANTIALIAS,
  OPT_VECTOR_BEAM,
  OPT_VECTOR_TRANSLUCENCY,
  OPT_VECTOR_FLICKER,
  OPT_VECTOR_INTENSITY,
  OPT_DCS_SPEEDHACK,
  OPT_CORE_SYS_SUBFOLDER,
  OPT_CORE_SAVE_SUBFOLDER,
  OPT_TATE_MODE,
  OPT_BRIGHTNESS,
  OPT_GAMMA,
  OPT_FRAMESKIP,
  OPT_SAMPLE_RATE,
  OPT_INPUT_INTERFACE,
  OPT_MAME_REMAPPING,
  OPT_ARTWORK,
  OPT_ART_RESOLUTION,
  OPT_NVRAM_BOOTSTRAP,
  OPT_Cheat_Input_Ports,
  OPT_Machine_Timing,
  OPT_end /* dummy last entry */
};

static struct retro_variable_default  default_options[OPT_end + 1];    /* need the plus one for the NULL entries at the end */
static struct retro_variable          current_options[OPT_end + 1];


/******************************************************************************

  private function prototypes

******************************************************************************/
static void   set_content_flags(void);
static void   init_core_options(void);
       void   init_default(struct retro_variable_default *option, const char *key, const char *value);
static void   update_variables(bool first_time);
static void   set_variables(bool first_time);
static struct retro_variable_default *spawn_effective_option(int option_index);
static void   check_system_specs(void);
       void   retro_describe_controls(void);
       int    get_mame_ctrl_id(int display_idx, int retro_ID);
       void   change_control_type(void);


/******************************************************************************

	frontend message interface

******************************************************************************/
void frontend_message_cb(const char *message_string, unsigned frames_to_display)
{
  frontend_message.msg    = message_string;
  frontend_message.frames = frames_to_display;
  environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &frontend_message);
}

/******************************************************************************

  implementation of key libretro functions

******************************************************************************/

void retro_init (void)
{
  struct retro_log_callback log;
  if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
    log_cb = log.log;
  else
    log_cb = NULL;

#ifdef LOG_PERFORMANCE
  environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);
#endif

  check_system_specs();
}

static void check_system_specs(void)
{
   /* Should we set level variably like the API asks? Are there any frontends that implement this? */
   unsigned level = 10; /* For stub purposes, set to the highest level */
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

void retro_set_environment(retro_environment_t cb)
{
  environ_cb = cb;
}


/* static void init_core_options(void)
 *
 * Note that core options are not presented in order they are initialized here,
 * but rather by their order in the OPT_ enum
 */
static void init_core_options(void)
{
  init_default(&default_options[OPT_4WAY],                APPNAME"_four_way_emulation",  "4-way joystick emulation on 8-way joysticks; disabled|enabled");
#if defined(__IOS__)
  init_default(&default_options[OPT_MOUSE_DEVICE],        APPNAME"_mouse_device",        "Mouse Device; pointer|mouse|disabled");
#else
  init_default(&default_options[OPT_MOUSE_DEVICE],        APPNAME"_mouse_device",        "Mouse Device; mouse|pointer|disabled");
#endif
  init_default(&default_options[OPT_CROSSHAIR_ENABLED],   APPNAME"_crosshair_enabled",   "Show Lightgun crosshair; enabled|disabled");
  init_default(&default_options[OPT_SKIP_DISCLAIMER],     APPNAME"_skip_disclaimer",     "Skip Disclaimer; disabled|enabled");
  init_default(&default_options[OPT_SKIP_WARNINGS],       APPNAME"_skip_warnings",       "Skip Warnings; disabled|enabled");
  init_default(&default_options[OPT_DISPLAY_SETUP],       APPNAME"_display_setup",       "Display MAME menu; disabled|enabled");
  init_default(&default_options[OPT_BRIGHTNESS],          APPNAME"_brightness",          "Brightness; 1.0|0.2|0.3|0.4|0.5|0.6|0.7|0.8|0.9|1.1|1.2|1.3|1.4|1.5|1.6|1.7|1.8|1.9|2.0");
  init_default(&default_options[OPT_GAMMA],               APPNAME"_gamma",               "Gamma correction; 1.0|0.5|0.6|0.7|0.8|0.9|1.1|1.2|1.3|1.4|1.5|1.6|1.7|1.8|1.9|2.0");
  init_default(&default_options[OPT_ARTWORK],             APPNAME"_display_artwork",     "Display artwork (Restart core); enabled|disabled");
  init_default(&default_options[OPT_ART_RESOLUTION],      APPNAME"_art_resolution",      "Artwork resolution multiplier (Restart core); 1|2");
  init_default(&default_options[OPT_NEOGEO_BIOS],         APPNAME"_neogeo_bios",         "Specify Neo Geo BIOS (Restart core); default|euro|euro-s1|us|us-e|asia|japan|japan-s2|unibios33|unibios20|unibios13|unibios11|unibios10|debug|asia-aes");
  init_default(&default_options[OPT_STV_BIOS],            APPNAME"_stv_bios",            "Specify Sega ST-V BIOS (Restart core); default|japan|japana|us|japan_b|taiwan|europe");
  init_default(&default_options[OPT_USE_ALT_SOUND],       APPNAME"_use_alt_sound",       "Use CD soundtrack (Restart core); enabled|disabled");
  init_default(&default_options[OPT_SHARE_DIAL],          APPNAME"_dialsharexy",         "Share 2 player dial controls across one X/Y device; disabled|enabled");
  init_default(&default_options[OPT_DPAD_ANALOG],         APPNAME"_analog",              "Control mapping ; analog|digital");
  init_default(&default_options[OPT_DEADZONE],            APPNAME"_deadzone",            "Analog deadzone; 20|0|5|10|15|25|30|35|40|45|50|55|60|65|70|75|80|85|90|95");
  init_default(&default_options[OPT_TATE_MODE],           APPNAME"_tate_mode",           "TATE Mode - Rotating display (Restart core); disabled|enabled");
  init_default(&default_options[OPT_VECTOR_RESOLUTION],   APPNAME"_vector_resolution",   "Vector resolution (Restart core); 1024x768|640x480|1280x960|1440x1080|1600x1200|original");
  init_default(&default_options[OPT_VECTOR_ANTIALIAS],    APPNAME"_vector_antialias",    "Vector antialiasing; enabled|disabled");
  init_default(&default_options[OPT_VECTOR_BEAM],         APPNAME"_vector_beam_width",   "Vector beam width (only with antialiasing); 2|1|1.2|1.4|1.6|1.8|2.5|3|4|5|6|7|8|9|10|11|12");
  init_default(&default_options[OPT_VECTOR_TRANSLUCENCY], APPNAME"_vector_translucency", "Vector translucency; enabled|disabled");
  init_default(&default_options[OPT_VECTOR_FLICKER],      APPNAME"_vector_flicker",      "Vector flicker; 20|0|10|30|40|50|60|70|80|90|100");
  init_default(&default_options[OPT_VECTOR_INTENSITY],    APPNAME"_vector_intensity",    "Vector intensity; 1.5|0.5|1|2|2.5|3");
  init_default(&default_options[OPT_NVRAM_BOOTSTRAP],     APPNAME"_nvram_bootstraps",    "NVRAM Bootstraps; enabled|disabled");
  init_default(&default_options[OPT_SAMPLE_RATE],         APPNAME"_sample_rate",         "Sample Rate (KHz); 48000|8000|11025|22050|30000|44100|");
  init_default(&default_options[OPT_DCS_SPEEDHACK],       APPNAME"_dcs_speedhack",       "DCS Speedhack; enabled|disabled");
  init_default(&default_options[OPT_INPUT_INTERFACE],     APPNAME"_input_interface",     "Input interface; retropad|keyboard|simultaneous");
  init_default(&default_options[OPT_MAME_REMAPPING],      APPNAME"_mame_remapping",      "Legacy Remapping (restart); enabled|disabled");
  init_default(&default_options[OPT_FRAMESKIP],           APPNAME"_frameskip",           "Frameskip; 0|1|2|3|4|5");
  init_default(&default_options[OPT_CORE_SYS_SUBFOLDER],  APPNAME"_core_sys_subfolder",  "Locate system files within a subfolder; enabled|disabled"); /* This should be probably handled by the frontend and not by cores per discussions in Fall 2018 but RetroArch for example doesn't provide this as an option. */
  init_default(&default_options[OPT_CORE_SAVE_SUBFOLDER], APPNAME"_core_save_subfolder", "Locate save files within a subfolder; enabled|disabled"); /* This is already available as an option in RetroArch although it is left enabled by default as of November 2018 for consistency with past practice. At least for now.*/
  init_default(&default_options[OPT_Cheat_Input_Ports],   APPNAME"_cheat_input_ports",   "Dip switch/Cheat input ports; disabled|enabled");
  init_default(&default_options[OPT_Machine_Timing],      APPNAME"_machine_timing",      "Bypass audio skew (Restart core); enabled|disabled");
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
      case OPT_DCS_SPEEDHACK:
         if(!options.content_flags[CONTENT_DCS_SPEEDHACK])
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

void init_default(struct retro_variable_default *def, const char *key, const char *label_and_values)
{
  def->key = key;
  def->defaults_string = label_and_values;
}

static void update_variables(bool first_time)
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
          else
            options.mouse_device = 0;
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

          /* TODO: Add overclock option. Below is the code from the old MAME osd to help process the core option.*/
          /*

          double overclock;
          int cpu, doallcpus = 0, oc;

          if (code_pressed(KEYCODE_LSHIFT) || code_pressed(KEYCODE_RSHIFT))
            doallcpus = 1;
          if (!code_pressed(KEYCODE_LCONTROL) && !code_pressed(KEYCODE_RCONTROL))
            increment *= 5;
          if( increment :
            overclock = timer_get_overclock(arg);
            overclock += 0.01 * increment;
            if (overclock < 0.01) overclock = 0.01;
            if (overclock > 2.0) overclock = 2.0;
            if( doallcpus )
              for( cpu = 0; cpu < cpu_gettotalcpu(); cpu++ )
                timer_set_overclock(cpu, overclock);
            else
              timer_set_overclock(arg, overclock);
          }

          oc = 100 * timer_get_overclock(arg) + 0.5;

          if( doallcpus )
            sprintf(buf,"%s %s %3d%%", ui_getstring (UI_allcpus), ui_getstring (UI_overclock), oc);
          else
            sprintf(buf,"%s %s%d %3d%%", ui_getstring (UI_overclock), ui_getstring (UI_cpu), arg, oc);
          displayosd(bitmap,buf,oc/2,100/2);
        */

        case OPT_ARTWORK:
          if(strcmp(var.value, "enabled") == 0)
            options.use_artwork = ARTWORK_USE_ALL;
          else
            options.use_artwork = ARTWORK_USE_NONE;
          break;

        case OPT_ART_RESOLUTION:
          options.artwork_res = atoi(var.value);
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

      case OPT_DPAD_ANALOG:
          if(strcmp(var.value, "analog") == 0)
          {
            if( options.analog !=1 && control_flag !=-1) control_flag =1;
            options.analog = 1;
            if (running && control_flag == 1) 
            {
              change_control_type(); 
              control_flag =0;
            }
          }
          else
          {
            if(options.analog !=0 && control_flag !=-1)  control_flag =1;
            options.analog = 0;
            if (running && control_flag == 1) 
            {
              change_control_type(); 
              control_flag =0;
            }
          }
          //skip the first set then there so it doesnt change input general every time you open the options menu if using none legacy mode
          if(control_flag ==-1) control_flag = 0;
          break;


        case OPT_DEADZONE:
            options.deadzone = atoi(var.value);
          break;

        case OPT_TATE_MODE:
          if(strcmp(var.value, "enabled") == 0)
            options.tate_mode = 1;
          else
            options.tate_mode = 0;
          break;

        case OPT_VECTOR_RESOLUTION:
          if(strcmp(var.value, "640x480") == 0)
          {
            options.vector_width=640;
            options.vector_height=480; 
          }
          else if(strcmp(var.value, "1024x768") == 0)
          {
            options.vector_width=1024;
            options.vector_height=768; 
          }
          else if(strcmp(var.value, "1280x960") == 0)
          {
            options.vector_width=1280;
            options.vector_height=960; 
          }
          else if(strcmp(var.value, "1440x1080") == 0)
          {
            options.vector_width=1440;
            options.vector_height=1080; 
          }
          else if(strcmp(var.value, "1600x1200") == 0)
          {
            options.vector_width=1600;
            options.vector_height=1200; 
          }
          else 
          {
            options.vector_width=0; // mame will set this from the driver resolution set
            options.vector_height=0; 
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
          options.vector_flicker = (int)(2.55 * atof(var.value)); /* why 2.55? must be an old family recipe */
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

        case OPT_DCS_SPEEDHACK:
          if(strcmp(var.value, "enabled") == 0)
            options.activate_dcs_speedhack = 1;
          else
            options.activate_dcs_speedhack = 0;
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
          options.frameskip = atoi(var.value);
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

	    case OPT_Cheat_Input_Ports:
          if(strcmp(var.value, "enabled") == 0)
            options.cheat_input_ports = true;
          else
            options.cheat_input_ports = false;
          break;		
	    case OPT_Machine_Timing:
          if(strcmp(var.value, "enabled") == 0)
            options.machine_timing = true;
          else
            options.machine_timing = false;
          break;	
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


void retro_get_system_av_info(struct retro_system_av_info *info)
{
  mame2003_video_get_geometry(&info->geometry);  
  if(options.machine_timing)
  {
    if (Machine->drv->frames_per_second < 60.0 )
      info->timing.fps = 60.0; 
    else 
      info->timing.fps = Machine->drv->frames_per_second; /* qbert is 61 fps */

    if ( (Machine->drv->frames_per_second * 1000 < options.samplerate) || ( Machine->drv->frames_per_second < 60) ) 
    {
      info->timing.sample_rate = Machine->drv->frames_per_second * 1000;
      log_cb(RETRO_LOG_INFO, LOGPRE "Sample timing rate too high for framerate required dropping to %f",  Machine->drv->frames_per_second * 1000);
    }       

    else
    {
      info->timing.sample_rate = options.samplerate;
      log_cb(RETRO_LOG_INFO, LOGPRE "Sample rate set to %d\n",options.samplerate); 
    }
  }

  else
  {
    info->timing.fps = Machine->drv->frames_per_second;

    if ( Machine->drv->frames_per_second * 1000 < options.samplerate)
     info->timing.sample_rate = 22050;

    else 
     info->timing.sample_rate = options.samplerate;
  }

}

unsigned retro_api_version(void)
{
  return RETRO_API_VERSION;
}


void retro_get_system_info(struct retro_system_info *info)
{
   /* this must match the 'corename' field in mame2003_plus_libretro.info
    * in order for netplay to work. */
  info->library_name = "MAME 2003-Plus";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
  info->library_version = GIT_VERSION;
  info->valid_extensions = "zip";
  info->need_fullpath = true;
  info->block_extract = true;
}

#define PAD_MODERN  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define PAD_8BUTTON RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define PAD_6BUTTON RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)

static struct retro_controller_description controllers[] = {
  { "Classic Gamepad",    RETRO_DEVICE_JOYPAD },
  { "Modern Fightstick",  PAD_MODERN  },
  { "8-Button",           PAD_8BUTTON },
  { "6-Button",           PAD_6BUTTON },
};

static struct retro_controller_description unsupported_controllers[] = {
  { "UNSUPPORTED (Classic Gamepad)",    RETRO_DEVICE_JOYPAD },
  { "UNSUPPORTED (Modern Fightstick)",  PAD_MODERN  },
  { "UNSUPPORTED (8-Button)",           PAD_8BUTTON },
  { "UNSUPPORTED (6-Button)",           PAD_6BUTTON },
};

static struct retro_controller_info retropad_subdevice_ports[] = {
  { controllers, 4 },
  { controllers, 4 },
  { controllers, 4 },
  { controllers, 4 },
  { controllers, 4 },
  { controllers, 4 },
  { 0 },
};

bool retro_load_game(const struct retro_game_info *game)
{
  int              driverIndex    = 0;
  int              port_index;
  char             *driver_lookup = NULL;

  if(string_is_empty(game->path))
  {
    log_cb(RETRO_LOG_ERROR, LOGPRE "Content path is not set. Exiting!\n");
    return false;
  }

  log_cb(RETRO_LOG_INFO, LOGPRE "Content path: %s.\n", game->path);
  if(!path_is_valid(game->path))
  {
    log_cb(RETRO_LOG_ERROR, LOGPRE "Content path is not valid. Exiting!");
    return false;
  }

  driver_lookup = strdup(path_basename(game->path));
  path_remove_extension(driver_lookup);

  log_cb(RETRO_LOG_INFO, LOGPRE "Content lookup name: %s.\n", driver_lookup);

  for (driverIndex = 0; driverIndex < total_drivers; driverIndex++)
  {
    const struct GameDriver *needle = drivers[driverIndex];

    if ((strcasecmp(driver_lookup, needle->description) == 0)
      || (strcasecmp(driver_lookup, needle->name) == 0) )
    {
      log_cb(RETRO_LOG_INFO, LOGPRE "Driver index counter: %d. Matched game driver: %s.\n",  driverIndex, needle->name);
      game_driver = needle;
      options.romset_filename_noext = driver_lookup;
      break;
    }
	if(driverIndex == total_drivers -2) // we could fix the total drives in drivers c but the it pointless its taken into account here
	{
      log_cb(RETRO_LOG_ERROR, LOGPRE "Driver index counter: %d. Game driver not found for %s!\n", driverIndex, driver_lookup);
      return false;
	}
 }

   if(!init_game(driverIndex))
    return false;

  set_content_flags();

  options.libretro_content_path = strdup(game->path);
  path_basedir(options.libretro_content_path);

  /* Get system directory from frontend */
  options.libretro_system_path = NULL;
  environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&options.libretro_system_path);
  if (options.libretro_system_path == NULL || options.libretro_system_path[0] == '\0')
  {
      log_cb(RETRO_LOG_INFO, LOGPRE "libretro system path not set by frontend, using content path\n");
      options.libretro_system_path = options.libretro_content_path;
  }

  /* Get save directory from frontend */
  options.libretro_save_path = NULL;
  environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY,&options.libretro_save_path);
  if (options.libretro_save_path == NULL || options.libretro_save_path[0] == '\0')
  {
      log_cb(RETRO_LOG_INFO,  LOGPRE "libretro save path not set by frontent, using content path\n");
      options.libretro_save_path = options.libretro_content_path;
  }

  log_cb(RETRO_LOG_INFO, LOGPRE "content path: %s\n", options.libretro_content_path);
  log_cb(RETRO_LOG_INFO, LOGPRE " system path: %s\n", options.libretro_system_path);
  log_cb(RETRO_LOG_INFO, LOGPRE "   save path: %s\n", options.libretro_save_path);


  init_core_options();
  update_variables(true);

  for(port_index = DISP_PLAYER6 - 1; port_index > (options.content_flags[CONTENT_CTRL_COUNT] - 1); port_index--)
  {
    retropad_subdevice_ports[port_index].types       = &unsupported_controllers[0];
    retropad_subdevice_ports[port_index].num_types   = 4;
  }

  environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)retropad_subdevice_ports);

  if(!run_game(driverIndex))
    return true;

  return false;
}

static void set_content_flags(void)
{
  int i = 0;

  extern struct GameDriver driver_neogeo;
  extern struct GameDriver driver_stvbios;
  const struct InputPortTiny *input = game_driver->input_ports;

  extern const char* ost_drivers[];

  /************ DRIVERS WITH MULTIPLE BIOS OPTIONS ************/
  if (game_driver->clone_of == &driver_neogeo
   ||(game_driver->clone_of && game_driver->clone_of->clone_of == &driver_neogeo))
  {
    options.content_flags[CONTENT_NEOGEO] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as a Neo Geo game.\n");
  }
  else if (game_driver->clone_of == &driver_stvbios
   ||(game_driver->clone_of && game_driver->clone_of->clone_of == &driver_stvbios))
  {
    options.content_flags[CONTENT_STV] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as a ST-V game.\n");
  }

  /************ DIE HARD: ARCADE ************/
  if(strcasecmp(game_driver->name, "diehard") == 0)
  {
    options.content_flags[CONTENT_DIEHARD] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as \"Die Hard: Arcade\". BIOS will be set to \"us\".\n");
  }

  /************ DRIVERS WITH ALTERNATE SOUNDTRACKS ************/
  while(ost_drivers[i])
  {
    if(strcmp(ost_drivers[i], game_driver->name) == 0)
    {
      options.content_flags[CONTENT_ALT_SOUND] = true;
      log_cb(RETRO_LOG_INFO, LOGPRE "Content has an alternative audio option controlled via core option.\n");
      break;
    }
    i++;
  }

  /************ DRIVERS WITH VECTOR VIDEO DISPLAYS ************/
  if(Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
  {
    options.content_flags[CONTENT_VECTOR] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using a vector video display.\n");
  }

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
			}

      if (input->type & IPF_4WAY) /* original controls used a 4-way joystick */
      {
        options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] = 4;
      }

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
					log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using dual joystick controls controls.\n");
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
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using paddle controls.\n");
          break;
				case IPT_DIAL:
          options.content_flags[CONTENT_DIAL] = true;
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using dial controls.\n");
					break;
				case IPT_TRACKBALL_X:
				case IPT_TRACKBALL_Y:
          options.content_flags[CONTENT_TRACKBALL] = true;
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using trackball controls.\n");
					break;
				case IPT_AD_STICK_X:
				case IPT_AD_STICK_Y:
          options.content_flags[CONTENT_AD_STICK] = true;
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using Analog/Digital stick controls.\n");
					break;
				case IPT_LIGHTGUN_X:
				case IPT_LIGHTGUN_Y:
          options.content_flags[CONTENT_LIGHTGUN] = true;
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using Analog/Digital stick controls.\n");
					break;
				case IPT_SERVICE :
          options.content_flags[CONTENT_HAS_SERVICE] = true;
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as having a service button.\n");
					break;
				case IPT_TILT :
          options.content_flags[CONTENT_HAS_TILT] = true;
          log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as having a tilt feature.\n");
					break;
			}
		}
		++input;
	}

  if (options.content_flags[CONTENT_JOYSTICK_DIRECTIONS] == 4)
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using 4-way joystick controls.\n");
  else
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as using 8-way joystick controls.\n");

  /************ DRIVERS FLAGGED IN CONTROLS.C WITH 45-DEGREE JOYSTICK ROTATION ************/
  if(game_driver->ctrl_dat->rotate_joy_45)
  {
    options.content_flags[CONTENT_ROTATE_JOY_45] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified by controls.c as joysticks rotated 45-degrees with respect to the cabinet.\n");
  }
  else
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified by controls.c as having joysticks on axis with respect to the cabinet.\n");

  /************ DRIVERS FLAGGED IN CONTROLS.C WITH ALTERNATING CONTROLS ************/
  if(game_driver->ctrl_dat->alternating_controls)
  {
    options.content_flags[CONTENT_ALTERNATING_CTRLS] = true;
    /* there may or may not be some need to have a ctrl_count different than player_count, perhaps because of some
       alternating controls layout. this is a place to check some condition and make the two numbers different
       if that should ever prove useful. */
    if(true)
      options.content_flags[CONTENT_CTRL_COUNT] = options.content_flags[CONTENT_PLAYER_COUNT];
  }
  else
    options.content_flags[CONTENT_CTRL_COUNT] = options.content_flags[CONTENT_PLAYER_COUNT];

  log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as supporting %i players with %i distinct controls.\n", options.content_flags[CONTENT_PLAYER_COUNT], options.content_flags[CONTENT_CTRL_COUNT]);
  log_cb(RETRO_LOG_INFO, LOGPRE "Content identified as supporting %i button controls.\n", options.content_flags[CONTENT_BUTTON_COUNT]);


  /************ DRIVERS FLAGGED IN CONTROLS.C WITH MIRRORED CONTROLS ************/
  if(game_driver->ctrl_dat->mirrored_controls)
  {
    options.content_flags[CONTENT_MIRRORED_CTRLS] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified by controls.c as having mirrored multiplayer control labels.\n");
  }
  else
    log_cb(RETRO_LOG_INFO, LOGPRE "Content identified by controls.c as having non-mirrored multiplayer control labels.\n");


  /************ DCS DRIVERS WITH SPEEDDUP HACKS ************/
  while(/*dcs_drivers[i]*/true)
  {
    if(/*strcmp(dcs_drivers[i], game_driver->name) == 0*/true)
    {
      options.content_flags[CONTENT_DCS_SPEEDHACK] = true;
      /*log_cb(RETRO_LOG_INFO, LOGPRE "DCS content has a speedup hack controlled via core option.\n");*/
      break;
    }
    i++;
  }

  /************ DRIVERS WITH NVRAM BOOTSTRAP PATCHES ************/
  if(game_driver->bootstrap != NULL)
  {
    options.content_flags[CONTENT_NVRAM_BOOTSTRAP] = true;
    log_cb(RETRO_LOG_INFO, LOGPRE "Content has an NVRAM bootstrap controlled via core option.\n");
  }

}

void retro_reset (void)
{
    machine_reset(); /* use internal core function */
}

/* get pointer axis vector from coord */
int16_t get_pointer_delta(int16_t coord, int16_t *prev_coord)
{
   int16_t delta = 0;
   if (*prev_coord == 0 || coord == 0)
   {
      *prev_coord = coord;
   }
   else
   {
      if (coord != *prev_coord)
      {
         delta = coord - *prev_coord;
         *prev_coord = coord;
      }
   }

   return delta;
}

void retro_run (void)
{
	int i;
	bool pointer_pressed;
	const struct KeyboardInfo *thisInput;
	bool updated = false;
    if (running == 0) running =1;
	poll_cb();

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		update_variables(false);

	/* Keyboard */
	thisInput = retroKeys;
	while(thisInput->name)
	{
		retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
		thisInput ++;
	}
   
	for (i = 0; i < 4; i ++)
	{
		unsigned int offset = (i * 26);

		/* Analog joystick */

		analogjoy[i][0] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X);
		analogjoy[i][1] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_Y);
		analogjoy[i][2] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
		analogjoy[i][3] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

		retroJsState[0 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
		retroJsState[1 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
		retroJsState[2 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
		retroJsState[3 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
		retroJsState[4 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
		retroJsState[5 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
		retroJsState[6 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
		retroJsState[7 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
		retroJsState[8 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
		retroJsState[9 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
		retroJsState[10 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L);
		retroJsState[11 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R);
		retroJsState[12 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
		retroJsState[13 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);
		retroJsState[14 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3);
		retroJsState[15 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3);
      
		if (options.mouse_device)
		{
			if (options.mouse_device == RETRO_DEVICE_MOUSE)
			{
				retroJsState[16 + offset] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
				retroJsState[17 + offset] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
				mouse_x[i] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
				mouse_y[i] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
			}
			else /* RETRO_DEVICE_POINTER */
			{
				pointer_pressed = input_cb(i, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
				retroJsState[16 + offset] = pointer_pressed;
				retroJsState[17 + offset] = 0; /* padding */
				mouse_x[i] = pointer_pressed ? get_pointer_delta(input_cb(i, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X), &prev_pointer_x) : 0;
				mouse_y[i] = pointer_pressed ? get_pointer_delta(input_cb(i, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y), &prev_pointer_y) : 0;
			}
		}
		else
		{
			retroJsState[16 + offset] = 0;
			retroJsState[17 + offset] = 0;
		}


		retroJsState[ 18 + offset] = 0;
		retroJsState[ 19 + offset] = 0;
		retroJsState[ 20 + offset] = 0;
		retroJsState[ 21 + offset] = 0;
		retroJsState[ 22 + offset] = 0;
		retroJsState[ 23 + offset] = 0;
		retroJsState[ 24 + offset] = 0;
		retroJsState[ 25 + offset] = 0;

		if (convert_analog_scale(analogjoy[i][0]) < -pressure_check) 
			retroJsState[ 18 + offset] = convert_analog_scale(analogjoy[i][0]); 

		if (convert_analog_scale(analogjoy[i][0]) >  pressure_check)
			retroJsState[ 19 + offset] = convert_analog_scale(analogjoy[i][0]);

		if (convert_analog_scale(analogjoy[i][1]) < -pressure_check)
			retroJsState[ 20 + offset] = convert_analog_scale(analogjoy[i][1]); 

		if (convert_analog_scale(analogjoy[i][1]) >  pressure_check)
			retroJsState[ 21 + offset] = convert_analog_scale(analogjoy[i][1]);
		
		if (convert_analog_scale(analogjoy[i][2]) < -pressure_check)
			retroJsState[ 22 + offset] = convert_analog_scale(analogjoy[i][2]); 

		if (convert_analog_scale(analogjoy[i][2]) >  pressure_check)
			retroJsState[ 23 + offset] = convert_analog_scale(analogjoy[i][2]);

		if (convert_analog_scale(analogjoy[i][3]) < -pressure_check) 
			retroJsState[ 24 + offset] = convert_analog_scale(analogjoy[i][3]); 
		if (convert_analog_scale(analogjoy[i][3]) >  pressure_check)
			retroJsState[ 25 + offset] = convert_analog_scale(analogjoy[i][3]);
	}
   mame_frame();
}

void retro_unload_game(void)
{
    mame_done();
    /* do we need to be freeing things here? */

    free(options.romset_filename_noext);
}

void retro_deinit(void)
{
#ifdef LOG_PERFORMANCE
   perf_cb.perf_log();
#endif
}

extern size_t state_get_dump_size(void);

size_t retro_serialize_size(void)
{
    return state_get_dump_size();
}

bool retro_serialize(void *data, size_t size)
{
   int cpunum;
	if(  retro_serialize_size() == size  && size   )
	{
		/* write the save state */
		state_save_save_begin(data);

		/* write tag 0 */
		state_save_set_current_tag(0);
		if(state_save_save_continue())
		{
		    return false;
		}

		/* loop over CPUs */
		for (cpunum = 0; cpunum < cpu_gettotalcpu(); cpunum++)
		{
			cpuintrf_push_context(cpunum);

			/* make sure banking is set */
			activecpu_reset_banking();

			/* save the CPU data */
			state_save_set_current_tag(cpunum + 1);
			if(state_save_save_continue())
			    return false;

			cpuintrf_pop_context();
		}

		/* finish and close */
		state_save_save_finish();
		
		return true;
	}

	return false;
}

bool retro_unserialize(const void * data, size_t size)
{
    int cpunum;
	/* if successful, load it */
	if ( (retro_serialize_size() ) && ( data ) && ( size ) && ( !state_save_load_begin((void*)data, size) ) )
	{
        /* read tag 0 */
        state_save_set_current_tag(0);
        if(state_save_load_continue())
            return false;

        /* loop over CPUs */
        for (cpunum = 0; cpunum < cpu_gettotalcpu(); cpunum++)
        {
            cpuintrf_push_context(cpunum);

            /* make sure banking is set */
            activecpu_reset_banking();

            /* load the CPU data */
            state_save_set_current_tag(cpunum + 1);
            if(state_save_load_continue())
                return false;

            cpuintrf_pop_context();
        }

        /* finish and close */
        state_save_load_finish();


        return true;
	}

	return false;
}

/******************************************************************************

  Sound

  osd_start_audio_stream() is called at the start of the emulation to initialize
  the output stream, then osd_update_audio_stream() is called every frame to
  feed new data. osd_stop_audio_stream() is called when the emulation is stopped.

  The sample rate is fixed at Machine->sample_rate. Samples are 16-bit, signed.
  When the stream is stereo, left and right samples are alternated in the
  stream.

  osd_start_audio_stream() and osd_update_audio_stream() must return the number
  of samples (or couples of samples, when using stereo) required for next frame.
  This will be around Machine->sample_rate / Machine->drv->frames_per_second,
  the code may adjust it by SMALL AMOUNTS to keep timing accurate and to
  maintain audio and video in sync when using vsync. Note that sound emulation,
  especially when DACs are involved, greatly depends on the number of samples
  per frame to be roughly constant, so the returned value must always stay close
  to the reference value of Machine->sample_rate / Machine->drv->frames_per_second.
  Of course that value is not necessarily an integer so at least a +/- 1
  adjustment is necessary to avoid drifting over time.

******************************************************************************/

int osd_start_audio_stream(int stereo)
{
  if (options.machine_timing)
  {
    if ( ( Machine->drv->frames_per_second * 1000 < options.samplerate) || (Machine->drv->frames_per_second < 60) ) 
      Machine->sample_rate = Machine->drv->frames_per_second * 1000;
    
    else Machine->sample_rate = options.samplerate;
  }

  else
  {
    if ( Machine->drv->frames_per_second * 1000 < options.samplerate)
      Machine->sample_rate=22050;

    else
      Machine->sample_rate = options.samplerate;
  }
  
  delta_samples = 0.0f;
  usestereo = stereo ? 1 : 0;

  /* determine the number of samples per frame */
  samples_per_frame = Machine->sample_rate / Machine->drv->frames_per_second;
  orig_samples_per_frame = samples_per_frame;

  if (Machine->sample_rate == 0) return 0;

  samples_buffer = (short *) calloc(samples_per_frame+16, 2 + usestereo * 2);
  if (!usestereo) conversion_buffer = (short *) calloc(samples_per_frame+16, 4);
  
  return samples_per_frame;
}


int osd_update_audio_stream(INT16 *buffer)
{
	int i,j;
	if ( Machine->sample_rate !=0 && buffer )
	{
   		memcpy(samples_buffer, buffer, samples_per_frame * (usestereo ? 4 : 2));
		if (usestereo)
			audio_batch_cb(samples_buffer, samples_per_frame);
		else
		{
			for (i = 0, j = 0; i < samples_per_frame; i++)
        		{
				conversion_buffer[j++] = samples_buffer[i];
				conversion_buffer[j++] = samples_buffer[i];
		        }
         		audio_batch_cb(conversion_buffer,samples_per_frame);
		}	
		
			
		//process next frame
			
		if ( samples_per_frame  != orig_samples_per_frame ) samples_per_frame = orig_samples_per_frame;
		
		// dont drop any sample frames some games like mk will drift with time

		delta_samples += (Machine->sample_rate / Machine->drv->frames_per_second) - orig_samples_per_frame;
		if ( delta_samples >= 1.0f )
		{
		
			int integer_delta = (int)delta_samples;
			if (integer_delta <= 16 )
                        {
				log_cb(RETRO_LOG_DEBUG,"sound: Delta added value %d added to frame\n",integer_delta);
				samples_per_frame += integer_delta;
			}
			else if(integer_delta >= 16) log_cb(RETRO_LOG_INFO, "sound: Delta not added to samples_per_frame too large integer_delta:%d\n", integer_delta);
			else log_cb(RETRO_LOG_DEBUG,"sound(delta) no contitions met\n");	
			delta_samples -= integer_delta;

		}
	}
        return samples_per_frame;
}


void osd_stop_audio_stream(void)
{
}



/******************************************************************************

Miscellaneous

******************************************************************************/

unsigned retro_get_region (void) {return RETRO_REGION_NTSC;}
void *retro_get_memory_data(unsigned type) {return 0;}
size_t retro_get_memory_size(unsigned type) {return 0;}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info){return false;}
void retro_cheat_reset(void){}
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2){}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }


/******************************************************************************

	RetroPad mapping

******************************************************************************/

  /* Assuming the standard RetroPad layout:
   *
   *   [L2]                                 [R2]
   *   [L]                                   [R]
   *
   *     [^]                               [X]
   *
   * [<]     [>]    [start] [selct]    [Y]     [A]
   *
   *     [v]                               [B]
   *
   *
   * or standard RetroPad fight stick layout:
   *
   *   [start] [selct]
   *                                         [X]  [R]  [L]
   *     [^]                            [Y]
   *
   * [<]     [>]                             [A]  [R2] [L2]
   *                                    [B]
   *     [v]
   *
   *
   *
   * key: [Button code/Street Fighter II move]
   *
   * PAD_GAMEPAD
   * ========================
   * Uses the fight stick & pad layout popularised by Street Figher IV.
   * Needs an 8+ button controller by default.
   *
   * [8/-]                                     [6/HK]  |
   * [7/-]                                     [3/HP]  |
   *                                                   |        [2/MP]  [3/HP]  [7/-]
   *     [^]                               [2/MP]      |  [1/LP]
   *                                                   |
   * [<]     [>]    [start] [selct]    [1/LP]  [5/MK]  |        [5/MK]  [6/HK]  [8/-]
   *                                                   |  [4/LK]
   *     [v]                               [4/LK]      |
   *                                                   |
   *
   * PAD_6BUTTON
   * ========================
   * Only needs a 6+ button controller by default, doesn't suit 8+ button fight sticks.
   *
   * [7/-]                                      [8/-]  |
   * [3/HP]                                    [6/HK]  |
   *                                                   |        [2/MP]  [6/HK]  [3/HP]
   *     [^]                               [2/MP]      |  [1/LP]
   *                                                   |
   * [<]     [>]    [start] [selct]    [1/LP]  [5/MK]  |        [5/MK]  [8/-]   [7/-]
   *                                                   |  [4/LK]
   *     [v]                               [4/LK]      |
   *                                                   |
   *
   * PAD_CLASSIC
   * ========================
   * Same as FB Alpha's "Classic" mapping.
   * Not sensible for 6 button fighters, but may suit other games.
   *
   * [7/-]                                     [8/-]   |
   * [5/MK]                                    [6/HK]  |
   *                                                   |        [4/WK]  [6/HK]  [5/MK]
   *     [^]                               [4/WK]      |  [3/HP]
   *                                                   |
   * [<]     [>]    [start] [selct]    [3/HP]  [2/MP]  |        [2/MP]  [8/-]   [7/-]
   *                                                   |  [1/LP]
   *     [v]                               [1/LP]      |
   *                                                   |
   */


/* libretro presents "Player 1", "Player 2", "Player 3", etc while internally using indexed data starting at 0, 1, 2 */
/* The core presents "Player 1", "Player 2," "Player 3", and indexes them via enum values like JOYCODE_1_BUTTON1,        */
/* JOYCODE_2_BUTTON1, JOYCODE_3_BUTTON1 or with #define-d masks IPF_PLAYER1, IPF_PLAYER2, IPF_PLAYER3.               */
/*                                                                                                                   */
/* We are by convention passing "display" value used for mapping to core enums and player # masks to these macros.   */
/* (Display Index - 1) can be used for indexed data structures.                                                      */

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
  environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, empty); /* is this necessary? it was in the sample code */
  options.retropad_layout[in_port] = device;
  retro_describe_controls();
}

#define NUMBER_OF_RETRO_TYPES (RETRO_DEVICE_ID_JOYPAD_R3 + 1)

void retro_describe_controls(void)
{
  int retro_type   = 0;
  int display_idx  = 0;

  struct retro_input_descriptor desc[(DISP_PLAYER6 * NUMBER_OF_RETRO_TYPES) +  1]; /* second + 1 for the final zeroed record. */
  struct retro_input_descriptor *needle = &desc[0];

  for(display_idx = DISP_PLAYER1; (display_idx <= options.content_flags[CONTENT_CTRL_COUNT] && display_idx <= DISP_PLAYER6); display_idx++)
  {
    for(retro_type = RETRO_DEVICE_ID_JOYPAD_B; retro_type < NUMBER_OF_RETRO_TYPES; retro_type++)
    {
      const char *control_name;
      int mame_ctrl_id = get_mame_ctrl_id(display_idx, retro_type) & ~IPF_PLAYERMASK ;

      if(mame_ctrl_id >= IPT_BUTTON1 && mame_ctrl_id <= IPT_BUTTON10)
      {
        if((mame_ctrl_id - IPT_BUTTON1 + 1) > options.content_flags[CONTENT_BUTTON_COUNT])
        {
          continue;
        }
      }

      switch(retro_type)
      {
        case RETRO_DEVICE_ID_JOYPAD_SELECT: control_name = "Coin";  break;
        case  RETRO_DEVICE_ID_JOYPAD_START: control_name = "Start"; break;
        default:                            control_name = game_driver->ctrl_dat->get_name(mame_ctrl_id  ); break;
      }

      if(string_is_empty(control_name))
        continue;

      needle->port = display_idx - 1;
      needle->device = RETRO_DEVICE_JOYPAD;
      needle->index = 0;
      needle->id = retro_type;
      needle->description = control_name;
      log_cb(RETRO_LOG_DEBUG, LOGPRE "Describing controls for: display_idx: %i | retro_type: %i | id: %i | desc: %s\n", display_idx, retro_type, needle->id, needle->description);
      needle++;
    }
  }

  /* the extra final record remains zeroed to indicate the end of the description to the frontend */
  needle->port = 0;
  needle->device = 0;
  needle->index = 0;
  needle->id = 0;
  needle->description = NULL;

  environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

}

int get_mame_ctrl_id(int display_idx, int retro_ID)
{
  int player_flag;

  {
    switch(display_idx)
    {
      case 1: player_flag = IPF_PLAYER1; break;
      case 2: player_flag = IPF_PLAYER2; break;
      case 3: player_flag = IPF_PLAYER3; break;
      case 4: player_flag = IPF_PLAYER4; break;
      case 5: player_flag = IPF_PLAYER5; break;
      case 6: player_flag = IPF_PLAYER6; break;
	  default: player_flag = IPF_PLAYER1; break;
    }
  }


  switch(retro_ID) /* universal default mappings */
  {
    case RETRO_DEVICE_ID_JOYPAD_LEFT:   return (player_flag | IPT_JOYSTICK_LEFT);
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return (player_flag | IPT_JOYSTICK_RIGHT);
    case RETRO_DEVICE_ID_JOYPAD_UP:     return (player_flag | IPT_JOYSTICK_UP);
    case RETRO_DEVICE_ID_JOYPAD_DOWN:   return (player_flag | IPT_JOYSTICK_DOWN);

    case RETRO_DEVICE_ID_JOYPAD_SELECT:
    {
      switch(display_idx)
      {
        case 1: return IPT_COIN1;
        case 2: return IPT_COIN2;
        case 3: return IPT_COIN3;
        case 4: return IPT_COIN4;
        case 5: return IPT_COIN5;
        case 6: return IPT_COIN6;
      }
    }
    case RETRO_DEVICE_ID_JOYPAD_START:
    {
      switch(display_idx)
      {
        case 1: return IPT_START1;
        case 2: return IPT_START2;
        case 3: return IPT_START3;
        case 4: return IPT_START4;
        case 5: return IPT_START5;
        case 6: return IPT_START6;
      }
    }
  }
  log_cb(RETRO_LOG_DEBUG, "display_idx: %i | options.retropad_layout[display_idx - 1]: %i\n", display_idx, options.retropad_layout[display_idx - 1]);
  switch(options.retropad_layout[display_idx - 1])
  {
    case RETRO_DEVICE_JOYPAD:
    {
      switch(retro_ID)
      {
        case RETRO_DEVICE_ID_JOYPAD_B:  return (player_flag | IPT_BUTTON1);
        case RETRO_DEVICE_ID_JOYPAD_Y:  return (player_flag | IPT_BUTTON3);
        case RETRO_DEVICE_ID_JOYPAD_X:  return (player_flag | IPT_BUTTON4);
        case RETRO_DEVICE_ID_JOYPAD_A:  return (player_flag | IPT_BUTTON2);
        case RETRO_DEVICE_ID_JOYPAD_L:  return (player_flag | IPT_BUTTON5);
        case RETRO_DEVICE_ID_JOYPAD_R:  return (player_flag | IPT_BUTTON6);
        case RETRO_DEVICE_ID_JOYPAD_L2: return (player_flag | IPT_BUTTON7);
        case RETRO_DEVICE_ID_JOYPAD_R2: return (player_flag | IPT_BUTTON8);
        case RETRO_DEVICE_ID_JOYPAD_L3: return (player_flag | IPT_BUTTON9);
        case RETRO_DEVICE_ID_JOYPAD_R3: return (player_flag | IPT_BUTTON10);
      }
      return 0;
    }
    case PAD_MODERN:
    {
      switch(retro_ID)
      {
        case RETRO_DEVICE_ID_JOYPAD_B:  return (player_flag | IPT_BUTTON4);
        case RETRO_DEVICE_ID_JOYPAD_Y:  return (player_flag | IPT_BUTTON1);
        case RETRO_DEVICE_ID_JOYPAD_X:  return (player_flag | IPT_BUTTON2);
        case RETRO_DEVICE_ID_JOYPAD_A:  return (player_flag | IPT_BUTTON5);
        case RETRO_DEVICE_ID_JOYPAD_L:  return (player_flag | IPT_BUTTON7);
        case RETRO_DEVICE_ID_JOYPAD_R:  return (player_flag | IPT_BUTTON3);
        case RETRO_DEVICE_ID_JOYPAD_L2: return (player_flag | IPT_BUTTON8);
        case RETRO_DEVICE_ID_JOYPAD_R2: return (player_flag | IPT_BUTTON6);
        case RETRO_DEVICE_ID_JOYPAD_L3: return (player_flag | IPT_BUTTON9);
        case RETRO_DEVICE_ID_JOYPAD_R3: return (player_flag | IPT_BUTTON10);
      }
      return 0;
    }
    case PAD_8BUTTON:
    {
      switch(retro_ID)
      {
        case RETRO_DEVICE_ID_JOYPAD_B:  return (player_flag | IPT_BUTTON4);
        case RETRO_DEVICE_ID_JOYPAD_Y:  return (player_flag | IPT_BUTTON1);
        case RETRO_DEVICE_ID_JOYPAD_X:  return (player_flag | IPT_BUTTON2);
        case RETRO_DEVICE_ID_JOYPAD_A:  return (player_flag | IPT_BUTTON5);
        case RETRO_DEVICE_ID_JOYPAD_L:  return (player_flag | IPT_BUTTON3);
        case RETRO_DEVICE_ID_JOYPAD_R:  return (player_flag | IPT_BUTTON7);
        case RETRO_DEVICE_ID_JOYPAD_L2: return (player_flag | IPT_BUTTON6);
        case RETRO_DEVICE_ID_JOYPAD_R2: return (player_flag | IPT_BUTTON8);
        case RETRO_DEVICE_ID_JOYPAD_L3: return (player_flag | IPT_BUTTON9);
        case RETRO_DEVICE_ID_JOYPAD_R3: return (player_flag | IPT_BUTTON10);
      }
      return 0;
    }
    case PAD_6BUTTON:
    {
      switch(retro_ID)
      {
        case RETRO_DEVICE_ID_JOYPAD_B:  return (player_flag | IPT_BUTTON4);
        case RETRO_DEVICE_ID_JOYPAD_Y:  return (player_flag | IPT_BUTTON1);
        case RETRO_DEVICE_ID_JOYPAD_X:  return (player_flag | IPT_BUTTON2);
        case RETRO_DEVICE_ID_JOYPAD_A:  return (player_flag | IPT_BUTTON5);
        case RETRO_DEVICE_ID_JOYPAD_L:  return (player_flag | IPT_BUTTON3);
        case RETRO_DEVICE_ID_JOYPAD_R:  return (player_flag | IPT_BUTTON6);
        case RETRO_DEVICE_ID_JOYPAD_L2: return (player_flag | IPT_BUTTON7);
        case RETRO_DEVICE_ID_JOYPAD_R2: return (player_flag | IPT_BUTTON8);
        case RETRO_DEVICE_ID_JOYPAD_L3: return (player_flag | IPT_BUTTON9);
        case RETRO_DEVICE_ID_JOYPAD_R3: return (player_flag | IPT_BUTTON10);
      }
      return 0;
    }

  }
  return 0;
}

#define DIRECTIONAL_COUNT           12  /* map Left, Right Up, Down as well as B, Y, A, X */
#define DIRECTIONAL_COUNT_NO_DBL    4
#define BUTTON_COUNT_PER           12  /* 10 buttons plus Start and Select          */
#define MOUSE_BUTTON_PER            2

#define PER_PLAYER_CTRL_COUNT                   (DIRECTIONAL_COUNT + BUTTON_COUNT_PER + MOUSE_BUTTON_PER)
#define PER_PLAYER_CTRL_COUNT_NO_DBL_NO_MOUSE   (DIRECTIONAL_COUNT_NO_DBL + BUTTON_COUNT_PER)

#define EMIT_RETROPAD_CLASSIC(DISPLAY_IDX) \
  {"RP"   #DISPLAY_IDX " HAT Left ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT +3000,   JOYCODE_##DISPLAY_IDX##_LEFT}, \
  {"RP"   #DISPLAY_IDX " HAT Right",    ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT +3000,  JOYCODE_##DISPLAY_IDX##_RIGHT}, \
  {"RP"   #DISPLAY_IDX " HAT Up   ",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_UP +3000,     JOYCODE_##DISPLAY_IDX##_UP}, \
  {"RP"   #DISPLAY_IDX " HAT Down ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN +3000,   JOYCODE_##DISPLAY_IDX##_DOWN}, \
  {"RP"   #DISPLAY_IDX " B",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_B +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON1}, \
  {"RP"   #DISPLAY_IDX " A",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_A +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " Y",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_Y +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON3}, \
  {"RP"   #DISPLAY_IDX " X",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_X +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON4}, \
  {"RP"   #DISPLAY_IDX " L",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON5}, \
  {"RP"   #DISPLAY_IDX " R",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON6}, \
  {"RP"   #DISPLAY_IDX " L2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON7}, \
  {"RP"   #DISPLAY_IDX " R2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON8}, \
  {"RP"   #DISPLAY_IDX " L3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON9}, \
  {"RP"   #DISPLAY_IDX " R3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  {"RP"   #DISPLAY_IDX " Start",        ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_START +1000,  JOYCODE_##DISPLAY_IDX##_START}, \
  {"RP"   #DISPLAY_IDX " Select",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT +1000, JOYCODE_##DISPLAY_IDX##_SELECT}, \
  {"Mouse" #DISPLAY_IDX " LClick",      ((DISPLAY_IDX - 1) * 26) + 16 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON1}, \
  {"Mouse" #DISPLAY_IDX " RClick",      ((DISPLAY_IDX - 1) * 26) + 17 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X-",    ((DISPLAY_IDX - 1) * 26) + 18 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X+",    ((DISPLAY_IDX - 1) * 26) + 19 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y-",    ((DISPLAY_IDX - 1) * 26) + 20 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y+",    ((DISPLAY_IDX - 1) * 26) + 21 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_DOWN}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X-",    ((DISPLAY_IDX - 1) * 26) + 22 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X+",    ((DISPLAY_IDX - 1) * 26) + 23 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y-",    ((DISPLAY_IDX - 1) * 26) + 24 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y+",    ((DISPLAY_IDX - 1) * 26) + 25 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_DOWN}, 

#define EMIT_RETROPAD_MODERN(DISPLAY_IDX) \
  {"RP"   #DISPLAY_IDX " HAT Left ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT +3000,   JOYCODE_##DISPLAY_IDX##_LEFT}, \
  {"RP"   #DISPLAY_IDX " HAT Right",    ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT +3000,  JOYCODE_##DISPLAY_IDX##_RIGHT}, \
  {"RP"   #DISPLAY_IDX " HAT Up   ",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_UP +3000,     JOYCODE_##DISPLAY_IDX##_UP}, \
  {"RP"   #DISPLAY_IDX " HAT Down ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN +3000,   JOYCODE_##DISPLAY_IDX##_DOWN}, \
  {"RP"   #DISPLAY_IDX " Y",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_Y  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON1}, \
  {"RP"   #DISPLAY_IDX " X",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_X  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " R",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON3}, \
  {"RP"   #DISPLAY_IDX " B",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_B  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON4}, \
  {"RP"   #DISPLAY_IDX " A",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_A  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON5}, \
  {"RP"   #DISPLAY_IDX " R2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON6}, \
  {"RP"   #DISPLAY_IDX " L",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON7}, \
  {"RP"   #DISPLAY_IDX " L2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON8}, \
  {"RP"   #DISPLAY_IDX " L3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON9}, \
  {"RP"   #DISPLAY_IDX " R3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  {"RP"   #DISPLAY_IDX " Start",        ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_START +1000,  JOYCODE_##DISPLAY_IDX##_START}, \
  {"RP"   #DISPLAY_IDX " Select",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT +1000, JOYCODE_##DISPLAY_IDX##_SELECT}, \
  {"Mouse" #DISPLAY_IDX " LClick",      ((DISPLAY_IDX - 1) * 26) + 16 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON1}, \
  {"Mouse" #DISPLAY_IDX " RClick",      ((DISPLAY_IDX - 1) * 26) + 17 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X-",    ((DISPLAY_IDX - 1) * 26) + 18 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X+",    ((DISPLAY_IDX - 1) * 26) + 19 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y-",    ((DISPLAY_IDX - 1) * 26) + 20 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y+",    ((DISPLAY_IDX - 1) * 26) + 21 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_DOWN}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X-",    ((DISPLAY_IDX - 1) * 26) + 22 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X+",    ((DISPLAY_IDX - 1) * 26) + 23 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y-",    ((DISPLAY_IDX - 1) * 26) + 24 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y+",    ((DISPLAY_IDX - 1) * 26) + 25 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_DOWN}, 
  
#define EMIT_RETROPAD_8BUTTON(DISPLAY_IDX) \
  {"RP"   #DISPLAY_IDX " HAT Left ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT +3000,   JOYCODE_##DISPLAY_IDX##_LEFT}, \
  {"RP"   #DISPLAY_IDX " HAT Right",    ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT +3000,  JOYCODE_##DISPLAY_IDX##_RIGHT}, \
  {"RP"   #DISPLAY_IDX " HAT Up   ",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_UP +3000,     JOYCODE_##DISPLAY_IDX##_UP}, \
  {"RP"   #DISPLAY_IDX " HAT Down ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN +3000,   JOYCODE_##DISPLAY_IDX##_DOWN}, \
  {"RP"   #DISPLAY_IDX " Y",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_Y +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON1}, \
  {"RP"   #DISPLAY_IDX " X",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_X +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " L",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON3}, \
  {"RP"   #DISPLAY_IDX " B",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_B  +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON4}, \
  {"RP"   #DISPLAY_IDX " A",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_A +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON5}, \
  {"RP"   #DISPLAY_IDX " L2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON6}, \
  {"RP"   #DISPLAY_IDX " R",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON7}, \
  {"RP"   #DISPLAY_IDX " R2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON8}, \
  {"RP"   #DISPLAY_IDX " L3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON9}, \
  {"RP"   #DISPLAY_IDX " R3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  {"RP"   #DISPLAY_IDX " Start",        ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_START +1000,  JOYCODE_##DISPLAY_IDX##_START}, \
  {"RP"   #DISPLAY_IDX " Select",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT +1000, JOYCODE_##DISPLAY_IDX##_SELECT}, \
  {"Mouse" #DISPLAY_IDX " LClick",      ((DISPLAY_IDX - 1) * 26) + 16 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON1}, \
  {"Mouse" #DISPLAY_IDX " RClick",      ((DISPLAY_IDX - 1) * 26) + 17 +1000 ,                           JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X-",    ((DISPLAY_IDX - 1) * 26) + 18 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X+",    ((DISPLAY_IDX - 1) * 26) + 19 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y-",    ((DISPLAY_IDX - 1) * 26) + 20 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y+",    ((DISPLAY_IDX - 1) * 26) + 21 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_DOWN}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X-",    ((DISPLAY_IDX - 1) * 26) + 22 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X+",    ((DISPLAY_IDX - 1) * 26) + 23 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y-",    ((DISPLAY_IDX - 1) * 26) + 24 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y+",    ((DISPLAY_IDX - 1) * 26) + 25 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_DOWN}, 

#define EMIT_RETROPAD_6BUTTON(DISPLAY_IDX) \
  {"RP"   #DISPLAY_IDX " HAT Left ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT +3000,   JOYCODE_##DISPLAY_IDX##_LEFT}, \
  {"RP"   #DISPLAY_IDX " HAT Right",    ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT +3000,  JOYCODE_##DISPLAY_IDX##_RIGHT}, \
  {"RP"   #DISPLAY_IDX " HAT Up   ",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_UP +3000,     JOYCODE_##DISPLAY_IDX##_UP}, \
  {"RP"   #DISPLAY_IDX " HAT Down ",     ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN +3000,   JOYCODE_##DISPLAY_IDX##_DOWN}, \
  {"RP"   #DISPLAY_IDX " Y",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_Y +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON1}, \
  {"RP"   #DISPLAY_IDX " X",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_X +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " L",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON3}, \
  {"RP"   #DISPLAY_IDX " B",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_B +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON4}, \
  {"RP"   #DISPLAY_IDX " A",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_A +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON5}, \
  {"RP"   #DISPLAY_IDX " R",            ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R +1000,      JOYCODE_##DISPLAY_IDX##_BUTTON6}, \
  {"RP"   #DISPLAY_IDX " L2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON7}, \
  {"RP"   #DISPLAY_IDX " R2",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R2 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON8}, \
  {"RP"   #DISPLAY_IDX " L3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_L3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON9}, \
  {"RP"   #DISPLAY_IDX " R3",           ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_R3 +1000,     JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  {"RP"   #DISPLAY_IDX " Start",        ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_START +1000,  JOYCODE_##DISPLAY_IDX##_START}, \
  {"RP"   #DISPLAY_IDX " Select",       ((DISPLAY_IDX - 1) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT +1000, JOYCODE_##DISPLAY_IDX##_SELECT}, \
  {"Mouse" #DISPLAY_IDX " LClick",      ((DISPLAY_IDX - 1) * 26) + 16 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON1}, \
  {"Mouse" #DISPLAY_IDX " RClick",      ((DISPLAY_IDX - 1) * 26) + 17 +1000,                            JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X-",    ((DISPLAY_IDX - 1) * 26) + 18 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 0 X+",    ((DISPLAY_IDX - 1) * 26) + 19 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y-",    ((DISPLAY_IDX - 1) * 26) + 20 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 1 Y+",    ((DISPLAY_IDX - 1) * 26) + 21 +2000,                            JOYCODE_##DISPLAY_IDX##_LEFT_DOWN}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X-",    ((DISPLAY_IDX - 1) * 26) + 22 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_LEFT}, \
  {"RP"   #DISPLAY_IDX " AXIS 2 X+",    ((DISPLAY_IDX - 1) * 26) + 23 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_RIGHT}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y-",    ((DISPLAY_IDX - 1) * 26) + 24 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_UP}, \
  {"RP"   #DISPLAY_IDX " AXIS 3 Y+",    ((DISPLAY_IDX - 1) * 26) + 25 +2000,                            JOYCODE_##DISPLAY_IDX##_RIGHT_DOWN}, 

struct JoystickInfo alternate_joystick_maps[PLAYER_COUNT][IDX_PAD_end][PER_PLAYER_CTRL_COUNT] =
{
  {{EMIT_RETROPAD_CLASSIC(1)},{EMIT_RETROPAD_MODERN(1)},{EMIT_RETROPAD_8BUTTON(1)},{EMIT_RETROPAD_6BUTTON(1)}},
  {{EMIT_RETROPAD_CLASSIC(2)},{EMIT_RETROPAD_MODERN(2)},{EMIT_RETROPAD_8BUTTON(2)},{EMIT_RETROPAD_6BUTTON(2)}},
  {{EMIT_RETROPAD_CLASSIC(3)},{EMIT_RETROPAD_MODERN(3)},{EMIT_RETROPAD_8BUTTON(3)},{EMIT_RETROPAD_6BUTTON(3)}},
  {{EMIT_RETROPAD_CLASSIC(4)},{EMIT_RETROPAD_MODERN(4)},{EMIT_RETROPAD_8BUTTON(4)},{EMIT_RETROPAD_6BUTTON(4)}},
  {{EMIT_RETROPAD_CLASSIC(5)},{EMIT_RETROPAD_MODERN(5)},{EMIT_RETROPAD_8BUTTON(5)},{EMIT_RETROPAD_6BUTTON(5)}},
  {{EMIT_RETROPAD_CLASSIC(6)},{EMIT_RETROPAD_MODERN(6)},{EMIT_RETROPAD_8BUTTON(6)},{EMIT_RETROPAD_6BUTTON(6)}},
};

/******************************************************************************

	Joystick & Mouse/Trackball

******************************************************************************/

struct JoystickInfo mame_joy_map[(PLAYER_COUNT * PER_PLAYER_CTRL_COUNT) + 1]; /* + 1 for final zeroed struct */

const struct JoystickInfo *osd_get_joy_list(void)
{
  int player_map_idx = 0;
  int overall_idx    = 0;
  int display_idx    = 0;

  for(display_idx = DISP_PLAYER1; display_idx <= DISP_PLAYER6; display_idx++)
  {
    for(player_map_idx = 0; player_map_idx < PER_PLAYER_CTRL_COUNT; player_map_idx++)
    {
      int data_idx     = display_idx - 1;
      int coded_layout = options.retropad_layout[data_idx];
      int layout_idx   = 0;

      switch(coded_layout)
      {
        case RETRO_DEVICE_JOYPAD: layout_idx = IDX_CLASSIC; break;
        case PAD_MODERN:          layout_idx = IDX_MODERN;  break;
        case PAD_8BUTTON:         layout_idx = IDX_8BUTTON; break;
        case PAD_6BUTTON:         layout_idx = IDX_6BUTTON; break;
      }

      mame_joy_map[overall_idx++] = alternate_joystick_maps[data_idx][layout_idx][player_map_idx];
    }
  }

  /* the extra final record remains zeroed to indicate the end of the description to the frontend */
  mame_joy_map[overall_idx].name         = NULL;
  mame_joy_map[overall_idx].code         = 0;
  mame_joy_map[overall_idx].standardcode = 0;

  return mame_joy_map;
}

int osd_is_joy_pressed(int joycode)
{
	if (options.input_interface == RETRO_DEVICE_KEYBOARD) return 0;
 
	if ( joycode  >=1000 &&  joycode  < 2000)
		return  retroJsState[joycode-1000]; 
		
	if ( joycode >= 2000 && joycode < 3000 )
	{
		if (retroJsState[joycode-2000] >= 64) return  retroJsState[joycode-2000];
		if (retroJsState[joycode-2000] <= -64) return retroJsState[joycode-2000];

	}
	// only send dpad when analog is set or we will get a double input
	if ( joycode >= 3000) return retroJsState[joycode-3000];

	return 0;
}

int osd_is_joystick_axis_code(int joycode)
{
if (joycode >= 2000  && joycode < 3000)  return 1;
	return 0;
}

void osd_lightgun_read(int player, int *deltax, int *deltay)
{

}

void osd_trak_read(int player, int *deltax, int *deltay)
{
    *deltax = mouse_x[player];
    *deltay = mouse_y[player];
}

#ifdef _MSC_VER
#if _MSC_VER < 1800
double round(double number)
{
  return (number >= 0) ? (int)(number + 0.5) : (int)(number - 0.5);
}
#endif
#endif

int convert_analog_scale(int input)
{
	static const int TRIGGER_MAX = 0x8000;
	int neg_test=0;
	float scale;
	int trigger_deadzone;
	
	trigger_deadzone = (32678 * options.deadzone) / 100;
	
	if (input < 0) { input =abs(input); neg_test=1; }
	scale = ((float)TRIGGER_MAX/(float)(TRIGGER_MAX - trigger_deadzone));

	if ( input > 0 && input > trigger_deadzone )
	{
		// Re-scale analog range
		float scaled = (input - trigger_deadzone)*scale;
    input = (int)round(scaled);

		if (input > +32767) 
		{
			input = +32767;
		}
		input = input / 326.78;
	}

	else
	{
		input = 0;
	}

	
	if (neg_test) input =-abs(input);
	return (int) input * 1.28;
}

void osd_analogjoy_read(int player,int analog_axis[MAX_ANALOG_AXES], InputCode analogjoy_input[MAX_ANALOG_AXES])
{
  int i;
  int value;

  for (i = 0; i < MAX_ANALOG_AXES; i ++)
  {
    int code;
    value = 0;
    if (analogjoy_input[i] != CODE_NONE)
    {  
      code = analogjoy_input[i];

      if ( code == (player * 26) + 18 + 2000 || code == (player * 26) + 19 + 2000 )
        value = convert_analog_scale(analogjoy[player][0]);

      else if ( code == (player * 26) + 20 + 2000 || code == (player * 26) + 21 + 2000 )
        value = convert_analog_scale(analogjoy[player][1]);

      else if ( code == (player * 26) + 22 + 2000 || code == (player * 23) + 23 + 2000 )
        value = convert_analog_scale(analogjoy[player][2]);

      else if ( code == (player * 26) + 24 + 2000 || code == (player * 25) + 25 + 2000 )
        value = convert_analog_scale(analogjoy[player][3]);

      analog_axis[i]=value;
    }
  }
}

void osd_customize_inputport_defaults(struct ipd *defaults)
{

}

/* These calibration functions should never actually be used (as long as needs_calibration returns 0 anyway).*/
int osd_joystick_needs_calibration(void) { return 0; }
void osd_joystick_start_calibration(void){ }
const char *osd_joystick_calibrate_next(void) { return 0; }
void osd_joystick_calibrate(void) { }
void osd_joystick_end_calibration(void) { }


/******************************************************************************

	Keyboard

******************************************************************************/

extern const struct KeyboardInfo retroKeys[];
int retroKeyState[512];

const struct KeyboardInfo *osd_get_key_list(void)
{
  return retroKeys;
}

int osd_is_key_pressed(int keycode)
{
	if (options.input_interface == RETRO_DEVICE_JOYPAD)
		return 0;

	if (keycode < 512 && keycode >= 0)
    return retroKeyState[keycode];

  return 0;
}


int osd_readkey_unicode(int flush)
{
  /* TODO*/
  return 0;
}

/******************************************************************************

	Keymapping

******************************************************************************/

/* Unassigned keycodes*/
/*	KEYCODE_OPENBRACE, KEYCODE_CLOSEBRACE, KEYCODE_BACKSLASH2, KEYCODE_STOP, KEYCODE_LWIN, KEYCODE_RWIN, KEYCODE_DEL_PAD, KEYCODE_PAUSE,*/

/* The format for each systems key constants is RETROK_$(TAG) and KEYCODE_$(TAG) */
/* EMIT1(TAG): The tag value is the same between libretro and the core           */
/* EMIT2(RTAG, MTAG): The tag value is different between the two                 */
/* EXITX(TAG): The core has no equivalent key.*/

#define EMIT2(RETRO, KEY) {(char*)#RETRO, RETROK_##RETRO, KEYCODE_##KEY}
#define EMIT1(KEY) {(char*)#KEY, RETROK_##KEY, KEYCODE_##KEY}
#define EMITX(KEY) {(char*)#KEY, RETROK_##KEY, KEYCODE_OTHER}

const struct KeyboardInfo retroKeys[] =
{
    EMIT1(BACKSPACE),
    EMIT1(TAB),
    EMITX(CLEAR),

    EMIT1(BACKSPACE),
    EMIT1(TAB),
    EMITX(CLEAR),
    EMIT2(RETURN, ENTER),
    EMITX(PAUSE),
    EMIT2(ESCAPE, ESC),
    EMIT1(SPACE),
    EMITX(EXCLAIM),
    EMIT2(QUOTEDBL, TILDE),
    EMITX(HASH),
    EMITX(DOLLAR),
    EMITX(AMPERSAND),
    EMIT1(QUOTE),
    EMITX(LEFTPAREN),
    EMITX(RIGHTPAREN),
    EMIT1(ASTERISK),
    EMIT2(PLUS, EQUALS),
    EMIT1(COMMA),
    EMIT1(MINUS),
    EMITX(PERIOD),
    EMIT1(SLASH),

    EMIT1(0), EMIT1(1), EMIT1(2), EMIT1(3), EMIT1(4), EMIT1(5), EMIT1(6), EMIT1(7), EMIT1(8), EMIT1(9),

    EMIT1(COLON),
    EMITX(SEMICOLON),
    EMITX(LESS),
    EMITX(EQUALS),
    EMITX(GREATER),
    EMITX(QUESTION),
    EMITX(AT),
    EMITX(LEFTBRACKET),
    EMIT1(BACKSLASH),
    EMITX(RIGHTBRACKET),
    EMITX(CARET),
    EMITX(UNDERSCORE),
    EMITX(BACKQUOTE),

    EMIT2(a, A), EMIT2(b, B), EMIT2(c, C), EMIT2(d, D), EMIT2(e, E), EMIT2(f, F),
    EMIT2(g, G), EMIT2(h, H), EMIT2(i, I), EMIT2(j, J), EMIT2(k, K), EMIT2(l, L),
    EMIT2(m, M), EMIT2(n, N), EMIT2(o, O), EMIT2(p, P), EMIT2(q, Q), EMIT2(r, R),
    EMIT2(s, S), EMIT2(t, T), EMIT2(u, U), EMIT2(v, V), EMIT2(w, W), EMIT2(x, X),
    EMIT2(y, Y), EMIT2(z, Z),

    EMIT2(DELETE, DEL),

    EMIT2(KP0, 0_PAD), EMIT2(KP1, 1_PAD), EMIT2(KP2, 2_PAD), EMIT2(KP3, 3_PAD),
    EMIT2(KP4, 4_PAD), EMIT2(KP5, 5_PAD), EMIT2(KP6, 6_PAD), EMIT2(KP7, 7_PAD),
    EMIT2(KP8, 8_PAD), EMIT2(KP9, 9_PAD),

    EMITX(KP_PERIOD),
    EMIT2(KP_DIVIDE, SLASH_PAD),
    EMITX(KP_MULTIPLY),
    EMIT2(KP_MINUS, MINUS_PAD),
    EMIT2(KP_PLUS, PLUS_PAD),
    EMIT2(KP_ENTER, ENTER_PAD),
    EMITX(KP_EQUALS),

    EMIT1(UP), EMIT1(DOWN), EMIT1(RIGHT), EMIT1(LEFT),
    EMIT1(INSERT), EMIT1(HOME), EMIT1(END), EMIT2(PAGEUP, PGUP), EMIT2(PAGEDOWN, PGDN),

    EMIT1(F1), EMIT1(F2), EMIT1(F3), EMIT1(F4), EMIT1(F5), EMIT1(F6),
    EMIT1(F7), EMIT1(F8), EMIT1(F9), EMIT1(F10), EMIT1(F11), EMIT1(F12),
    EMITX(F13), EMITX(F14), EMITX(F15),

    EMIT1(NUMLOCK),
    EMIT1(CAPSLOCK),
    EMIT2(SCROLLOCK, SCRLOCK),
    EMIT1(RSHIFT), EMIT1(LSHIFT), EMIT2(RCTRL, RCONTROL), EMIT2(LCTRL, LCONTROL), EMIT1(RALT), EMIT1(LALT),
    EMITX(RMETA), EMITX(LMETA), EMITX(LSUPER), EMITX(RSUPER),

    EMITX(MODE),
    EMITX(COMPOSE),

    EMITX(HELP),
    EMIT2(PRINT, PRTSCR),
    EMITX(SYSREQ),
    EMITX(BREAK),
    EMIT1(MENU),
    EMITX(POWER),
    EMITX(EURO),
    EMITX(UNDO),

    {0, 0, 0}
};
