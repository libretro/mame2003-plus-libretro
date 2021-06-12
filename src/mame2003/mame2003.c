/*********************************************************************

	mame2003.c

    an updated port of Xmame 0.78 to the libretro API

*********************************************************************/

#include <stdint.h>
#include <string/stdstring.h>
#include <libretro.h>
#include <file/file_path.h>
#include <math.h>

#if (HAS_DRZ80 || HAS_CYCLONE)
#include "frontend_list.h"
#endif

#include "mame.h"
#include "driver.h"
#include "state.h"
#include "log.h"
#include "input.h"
#include "inptport.h"
#include "fileio.h"
#include "controls.h"
#include "usrintrf.h"


static const struct GameDriver  *game_driver;

int            retro_running = 0;
int            gotFrame;
static float   delta_samples;
int            samples_per_frame = 0;
int            orig_samples_per_frame =0;
short*         samples_buffer;
short*         conversion_buffer;
int            usestereo = 1;

/* comment needed: what does this legacy flag do exactly */
int legacy_flag = -1;

struct ipd  *default_inputs; /* pointer the array of structs with default MAME input mappings and labels */

/* data structures to store and translate keyboard state */
const struct KeyboardInfo  retroKeys[]; /* MAME data structure keymapping */

/* data structures for joystick/retropad state */
int retroJsState[MAX_PLAYER_COUNT][OSD_INPUT_CODES_PER_PLAYER]= {{0}}; /* initialise to zero, polled in retro_run */

/* temporary variables to convert absolute coordinates polled by pointer fallback, which is used
 * as a fallback for libretro frontends without DEVICE_RETRO_MOUSE implementations */
int16_t  prev_pointer_x;
int16_t  prev_pointer_y;

/* data structures to store position data for analog joysticks */
int16_t  analogjoy[MAX_PLAYER_COUNT][4]= {0};

retro_log_printf_t                 log_cb;
static struct retro_message        frontend_message;

struct                             retro_perf_callback perf_cb;
retro_environment_t                environ_cb                    = NULL;
retro_video_refresh_t              video_cb                      = NULL;
static retro_input_poll_t          poll_cb                       = NULL;
static retro_input_state_t         input_cb                      = NULL;
static retro_audio_sample_batch_t  audio_batch_cb                = NULL;
retro_set_led_state_t              led_state_cb                  = NULL;

#ifdef _MSC_VER
#if _MSC_VER < 1800
double round(double number)
{
  return (number >= 0) ? (int)(number + 0.5) : (int)(number - 0.5);
}
#endif
#endif

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
  OPT_DEADZONE,
  OPT_VECTOR_RESOLUTION,
  OPT_VECTOR_ANTIALIAS,
  OPT_VECTOR_BEAM,
  OPT_VECTOR_TRANSLUCENCY,
  OPT_VECTOR_FLICKER,
  OPT_VECTOR_INTENSITY,
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
  OPT_ART_OVERLAY_OPACITY,
  OPT_NVRAM_BOOTSTRAP,
  OPT_CHEAT_INPUT_PORTS,
  OPT_MACHINE_TIMING,
  OPT_DIGITAL_JOY_CENTERING,
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
   unsigned   get_device_parent(unsigned device_id);
        int   get_retropad_code(unsigned osd_code);
        int   get_retromouse_code(unsigned osd_code);
        int   get_retrogun_code(unsigned osd_code);
   unsigned   get_ctrl_ipt_code(unsigned player_number, unsigned standard_code);
   unsigned   encode_osd_joycode(unsigned player_number, unsigned joycode);
   unsigned   decode_osd_joycode(unsigned joycode);
   unsigned   calc_player_number(unsigned joycode);
        int   rescale_analog(int libretro_coordinate);
        int   analog_deadzone_rescale(int input);
static void   remove_slash (char* temp);


/******************************************************************************

  external function prototypes

******************************************************************************/

/* mame2003_video_get_geometry is found in video.c */
extern void mame2003_video_get_geometry(struct retro_game_geometry *geom);


/******************************************************************************
 *
 * Data structures for libretro controllers
 *
 ******************************************************************************/

/* the first of our controllers can use the base retropad type and rename it,
 * while any layout variations must subclass the type.
 */

#define PAD_CLASSIC       RETRO_DEVICE_JOYPAD
#define PAD_FIGHTSTICK    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define PAD_8BUTTON       RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define PAD_6BUTTON       RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)

const struct retro_controller_description controllers[] = {
  { "Gamepad",    PAD_CLASSIC    },
  { "Fightstick", PAD_FIGHTSTICK },
  { "8-Button",   PAD_8BUTTON    },
  { "6-Button",   PAD_6BUTTON    },
};

const struct retro_controller_description unsupported_controllers[] = {
  { "UNSUPPORTED (Gamepad)",    PAD_CLASSIC    },
  { "UNSUPPORTED (Fightstick)", PAD_FIGHTSTICK },
  { "UNSUPPORTED (8-Button)",   PAD_8BUTTON    },
  { "UNSUPPORTED (6-Button)",   PAD_6BUTTON    },
};

struct retro_controller_info input_subdevice_ports[] = {
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { controllers, IDX_NUMBER_OF_INPUT_TYPES },
  { 0, 0 },
};

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
  init_default(&default_options[OPT_FRAMESKIP],              APPNAME"_frameskip",              "Frameskip; 0|1|2|3|4|5");
  init_default(&default_options[OPT_CORE_SYS_SUBFOLDER],     APPNAME"_core_sys_subfolder",     "Locate system files within a subfolder; enabled|disabled"); /* This should be probably handled by the frontend and not by cores per discussions in Fall 2018 but RetroArch for example doesn't provide this as an option. */
  init_default(&default_options[OPT_CORE_SAVE_SUBFOLDER],    APPNAME"_core_save_subfolder",    "Locate save files within a subfolder; enabled|disabled"); /* This is already available as an option in RetroArch although it is left enabled by default as of November 2018 for consistency with past practice. At least for now.*/
  init_default(&default_options[OPT_CHEAT_INPUT_PORTS],      APPNAME"_cheat_input_ports",      "Dip switch/Cheat input ports; disabled|enabled");
  init_default(&default_options[OPT_MACHINE_TIMING],         APPNAME"_machine_timing",         "Bypass audio skew (Restart core); enabled|disabled");
  init_default(&default_options[OPT_DIGITAL_JOY_CENTERING],  APPNAME"_digital_joy_centering",  "Center joystick axis for digital controls; enabled|disabled");
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
      log_cb(RETRO_LOG_INFO, LOGPRE "Sample timing rate too high for framerate required dropping to %f\n",  Machine->drv->frames_per_second * 1000);
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


bool retro_load_game(const struct retro_game_info *game)
{
  int   driverIndex    = 0;
  int   port_index;
  char  *driver_lookup = NULL;

  if(string_is_empty(game->path))
  {
    log_cb(RETRO_LOG_ERROR, LOGPRE "Content path is not set. Exiting!\n");
    return false;
  }

  log_cb(RETRO_LOG_INFO, LOGPRE "Full content path %s\n", game->path);
  if(!path_is_valid(game->path))
  {
    log_cb(RETRO_LOG_ERROR, LOGPRE "Content path is not valid. Exiting!");
    return false;
  }
  log_cb(RETRO_LOG_INFO, LOGPRE "Git Version %s\n",GIT_VERSION);
  driver_lookup = strdup(path_basename(game->path));
  path_remove_extension(driver_lookup);

  log_cb(RETRO_LOG_INFO, LOGPRE "Content lookup name: %s\n", driver_lookup);

  for (driverIndex = 0; driverIndex < total_drivers; driverIndex++)
  {
    const struct GameDriver *needle = drivers[driverIndex];

    if ( strcasecmp(driver_lookup, needle->name) == 0 )
    {
      log_cb(RETRO_LOG_INFO, LOGPRE "Driver index counter: %d. Matched game driver: %s\n",  driverIndex, needle->name);
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

  #if (HAS_CYCLONE || HAS_DRZ80)
   int i;
   int use_cyclone = 1;
   int use_drz80 = 1;
   int use_drz80_snd = 1;

	for (i=0;i<NUMGAMES;i++)
 	{
		if (strcmp(drivers[driverIndex]->name,fe_drivers[i].name)==0)
		{
			/* ASM cores: 0=None,1=Cyclone,2=DrZ80,3=Cyclone+DrZ80,4=DrZ80(snd),5=Cyclone+DrZ80(snd) */
			switch (fe_drivers[i].cores)
			{
				case 0:
					use_cyclone = 0;
					use_drz80_snd = 0;
					use_drz80 = 0;
					break;
				case 1:
					use_drz80_snd = 0;
					use_drz80 = 0;
					break;
				case 2:
					use_cyclone = 0;
					break;
				case 4:
					use_cyclone = 0;
					use_drz80 = 0;
					break;
				case 5:
					use_drz80 = 0;
					break;
				default:
					break;
			}

			break;
		}
	}

   /* Replace M68000 by CYCLONE */
#if (HAS_CYCLONE)
   if (use_cyclone)
   {
	   for (i=0;i<MAX_CPU;i++)
	   {
		   unsigned int *type=(unsigned int *)&(Machine->drv->cpu[i].cpu_type);
#ifdef NEOMAME
		   if (*type==CPU_M68000)
#else
			   if (*type==CPU_M68000 || *type==CPU_M68010 )
#endif
			   {
				   *type=CPU_CYCLONE;
                   log_cb(RETRO_LOG_INFO, LOGPRE "Replaced CPU_CYCLONE\n");
			   }
        if(!(*type)){
          break;
        }
	   }
   }
#endif

#if (HAS_DRZ80)
	/* Replace Z80 by DRZ80 */
	if (use_drz80)
	{
		for (i=0;i<MAX_CPU;i++)
		{
			unsigned int *type=(unsigned int *)&(Machine->drv->cpu[i].cpu_type);
			if (type==CPU_Z80)
			{
				*type=CPU_DRZ80;
        log_cb(RETRO_LOG_INFO, LOGPRE "Replaced Z80\n");
			}
		}
	}

	/* Replace Z80 with DRZ80 only for sound CPUs */
	if (use_drz80_snd)
	{
		for (i=0;i<MAX_CPU;i++)
		{
			int *type=(int*)&(Machine->drv->cpu[i].cpu_type);
			if (type==CPU_Z80 && Machine->drv->cpu[i].cpu_flags&CPU_AUDIO_CPU)
			{
				*type=CPU_DRZ80;
        log_cb(RETRO_LOG_INFO, LOGPRE "Replaced Z80 sound\n");

			}
		}
	}
#endif

#endif

  set_content_flags();

  options.activate_dcs_speedhack = true; /* formerly a core option, now always on. */

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
      log_cb(RETRO_LOG_INFO,  LOGPRE "libretro save path not set by frontend, using content path\n");
      options.libretro_save_path = options.libretro_content_path;
  }

  /* Remove trailing slashes for specified systems */
  remove_slash(options.libretro_content_path);
  remove_slash(options.libretro_system_path);
  remove_slash(options.libretro_save_path);

  log_cb(RETRO_LOG_INFO, LOGPRE "content path: %s\n", options.libretro_content_path);
  log_cb(RETRO_LOG_INFO, LOGPRE " system path: %s\n", options.libretro_system_path);
  log_cb(RETRO_LOG_INFO, LOGPRE "   save path: %s\n", options.libretro_save_path);


  init_core_options();

  update_variables(true);

  /* Not all drivers support the maximum number of players; start at the highest index and decrement
   * until the highest supported index, designating the unsupported indexes during the loop.
   */
  for(port_index = MAX_PLAYER_COUNT - 1; port_index >= options.content_flags[CONTENT_CTRL_COUNT]; port_index--)
  {
    input_subdevice_ports[port_index].types       = &unsupported_controllers[0];
    input_subdevice_ports[port_index].num_types   = IDX_NUMBER_OF_INPUT_TYPES;
  }

  environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)input_subdevice_ports);

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
  int port = 0;
  bool updated = false;
  poll_cb(); /* execute input callback */

  if (retro_running == 0) /* first time through the loop */
  {
    retro_running = 1;
    log_cb(RETRO_LOG_DEBUG, LOGPRE "Entering retro_run() for the first time.\n");
  }

  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
    update_variables(false);

  /* begin by blanking the old values */
  for(port = 0; port < MAX_PLAYER_COUNT; port++)
  {
    int code_idx = 0;
    for(code_idx = 0; code_idx < OSD_INPUT_CODES_PER_PLAYER; code_idx++)
      retroJsState[port][code_idx] = 0;

    analogjoy[port][0] = 0;
    analogjoy[port][1] = 0;
    analogjoy[port][2] = 0;
    analogjoy[port][3] = 0;
  }

  for(port = 0; port < MAX_PLAYER_COUNT; port++)
  {
    int device_type          = options.active_control_type[port];
    int device_parent        = get_device_parent(device_type);

    if(device_type == RETRO_DEVICE_NONE) continue;

    /* Analog joystick - read as analog axis and rescale for MAME value range */
    analogjoy[port][0] = analog_deadzone_rescale( input_cb(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_X) );
    analogjoy[port][1] = analog_deadzone_rescale( input_cb(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,  RETRO_DEVICE_ID_ANALOG_Y) );
    analogjoy[port][2] = analog_deadzone_rescale( input_cb(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) );
    analogjoy[port][3] = analog_deadzone_rescale( input_cb(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) );

    /* Analog joystick - read as digital button */
    /* If the analog value (normalized for MAME to the range -128, 128) is greater in absolute   */
    /* terms than INPUT_BUTTON_AXIS_THRESHOLD, record it as a binary/digital signal.             */
    retroJsState[port][OSD_ANALOG_LEFT_NEGATIVE_X]  = (analogjoy[port][0] < -INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][0] : 0;
    retroJsState[port][OSD_ANALOG_LEFT_POSITIVE_X]  = (analogjoy[port][0] >  INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][0] : 0;
    retroJsState[port][OSD_ANALOG_LEFT_NEGATIVE_Y]  = (analogjoy[port][1] < -INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][1] : 0;
    retroJsState[port][OSD_ANALOG_LEFT_POSITIVE_Y]  = (analogjoy[port][1] >  INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][1] : 0;
    retroJsState[port][OSD_ANALOG_RIGHT_NEGATIVE_X] = (analogjoy[port][2] < -INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][2] : 0;
    retroJsState[port][OSD_ANALOG_RIGHT_POSITIVE_X] = (analogjoy[port][2] >  INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][2] : 0;
    retroJsState[port][OSD_ANALOG_RIGHT_NEGATIVE_Y] = (analogjoy[port][3] < -INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][3] : 0;
    retroJsState[port][OSD_ANALOG_RIGHT_POSITIVE_Y] = (analogjoy[port][3] >  INPUT_BUTTON_AXIS_THRESHOLD) ? analogjoy[port][3] : 0;
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
   *
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
   *
   *
   * PAD_FIGHTSTICK
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
   */


/* libretro presents "Player 1", "Player 2", "Player 3", etc while internally using indexed data starting at 0, 1, 2 */
/* The core presents "Player 1", "Player 2," "Player 3", and indexes them via enum values like JOYCODE_1_BUTTON1,        */
/* JOYCODE_2_BUTTON1, JOYCODE_3_BUTTON1 or with #define-d masks IPF_PLAYER1, IPF_PLAYER2, IPF_PLAYER3.               */
/*                                                                                                                   */
/* We are by convention passing "display" value used for mapping to core enums and player # masks to these macros.   */
/* (Display Index - 1) can be used for indexed data structures.                                                      */

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
  options.active_control_type[in_port] = device;
  log_cb(RETRO_LOG_DEBUG, LOGPRE "Preparing to connect input    in_port: %i    device: %i\n", in_port, device);
  internal_code_update();     /* update MAME data structures for controls */
  retro_describe_controls();  /* update libretro data structures for controls */
}


void retro_describe_controls(void)
{
  int port_number = 0;
  static struct retro_input_descriptor empty_input_descriptor[] = { { 0 } };
  struct retro_input_descriptor desc[(MAX_PLAYER_COUNT * OSD_INPUT_CODES_PER_PLAYER) +  1]; /* + 1 for the final zeroed record. */
  struct retro_input_descriptor *needle = &desc[0];

  for(port_number = 0; port_number < options.content_flags[CONTENT_CTRL_COUNT]; port_number++)
  {
    unsigned osd_index   = 0;
    unsigned device_code = options.active_control_type[port_number];

    log_cb(RETRO_LOG_INFO, "port_number: %i | active device type: %i\n", port_number, device_code);

    if(device_code == RETRO_DEVICE_NONE)  continue; /* move on to the next player */

    for(osd_index = OSD_JOYPAD_B; osd_index < OSD_INPUT_CODES_PER_PLAYER; osd_index++)
    {
      unsigned osd_code         = 0;      /* the unique code (including across players) created by the libretro OSD */
      unsigned standard_code    = 0;      /* standard code is the MAME term for the internal input code, associated with a controller */
      unsigned ctrl_ipt_code    = 0;      /* input code connects an input port with standard input code */
      unsigned retro_code       = 0;      /* #define code from the libretro.h input API scheme */
      const char *control_name  = NULL;

      osd_code = encode_osd_joycode(port_number + 1, osd_index);
      if(osd_code == INT_MAX) continue;

      standard_code = oscode_find(osd_code, CODE_TYPE_JOYSTICK);
      if(standard_code == CODE_NONE) continue;

      ctrl_ipt_code = get_ctrl_ipt_code(port_number + 1, standard_code) & ~IPF_PLAYERMASK; /* discard the player mask, although later we may want to distinguish control names by player number */
      if(ctrl_ipt_code == CODE_NONE) continue;

      if(ctrl_ipt_code >= IPT_BUTTON1 && ctrl_ipt_code <= IPT_BUTTON10)
        if((ctrl_ipt_code - IPT_BUTTON1 + 1) > options.content_flags[CONTENT_BUTTON_COUNT])
          continue; /* button has a higher index than supported by the driver */

      if(get_device_parent(device_code) == RETRO_DEVICE_JOYPAD)
      {
        /* try to get the corresponding ID for this control in libretro.h  */
        /* from the retropad section, or INT_MAX if not valid */
        retro_code = get_retropad_code(osd_index);
        if(retro_code != INT_MAX)
        {
          switch(retro_code) /* universal default mappings */
          {
            case RETRO_DEVICE_ID_JOYPAD_LEFT:   control_name = "Left";  break;
            case RETRO_DEVICE_ID_JOYPAD_RIGHT:  control_name = "Right"; break;
            case RETRO_DEVICE_ID_JOYPAD_UP:     control_name = "Up";    break;
            case RETRO_DEVICE_ID_JOYPAD_DOWN:   control_name = "Down";  break;
            case RETRO_DEVICE_ID_JOYPAD_SELECT: control_name = "Coin";  break;
            case RETRO_DEVICE_ID_JOYPAD_START:  control_name = "Start"; break;
          }
        }

        /* try to get the corresponding ID for this control in libretro.h  */
        /* from the lightgun section, or INT_MAX if not valid              */
        else 
        {
			retro_code = get_retrogun_code(osd_index);
			if (retro_code != INT_MAX)
			{
				switch(retro_code)
				{
					case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:  control_name = "Left";  break;
					case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT: control_name = "Right"; break;
					case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:    control_name = "Up";    break;
					case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:  control_name = "Down";  break;
					case RETRO_DEVICE_ID_LIGHTGUN_SELECT:     control_name = "Coin";  break;
					case RETRO_DEVICE_ID_LIGHTGUN_START:      control_name = "Start"; break;
				}
			}

        //continue; /* no matching codes found */
		}
	  }
      if(string_is_empty(control_name))  control_name = game_driver->ctrl_dat->get_name(ctrl_ipt_code);
      if(string_is_empty(control_name))  continue;
      
      /* With regard to the device number, we refer to the input polling comments in 
       * libretro.h which says we "should only poll input based on the base input device
       * types". That seems to be true in here too, because using the result of 
       * RETRO_DEVICE_SUBCLASS does not work in RetroArch in April 2021 when passed as part
       * of the descriptions. Therefore, get_device_parent() is used.
       */
      needle->port         = port_number;
      needle->device       = get_device_parent(device_code);
      needle->index        = 0;
      needle->id           = retro_code;
      needle->description  = control_name;
      log_cb(RETRO_LOG_INFO, LOGPRE "Describing controls for port_number: %i | device type: %i | parent type: %i | osd_code: %i | standard code: %i | retro id: %i | desc: %s\n", port_number, device_code, get_device_parent(device_code), osd_code, standard_code, needle->id, needle->description);
      needle++;
    }
  }

  /* the extra final record remains zeroed to indicate the end of the description to the frontend */
  /* which specifically looks at the description field to be NULL per libretro.h */
  needle->port        = 0;
  needle->device      = 0;
  needle->index       = 0;
  needle->id          = 0;
  needle->description = NULL;


  needle = &desc[0];
  log_cb(RETRO_LOG_INFO, LOGPRE "Beginning of description list.\n");
  while(needle->description != NULL)
  {
    log_cb(RETRO_LOG_INFO, LOGPRE "Description || port: %i | device: %i | index: %i | id: %i \t| name: %s\n", needle->port, needle->device, needle->index, needle->id, needle->description);
    needle++;
  }
  log_cb(RETRO_LOG_INFO, LOGPRE "End of description list.\n");

  environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, empty_input_descriptor); /* flush descriptions, per the sample code */
  environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

/* get_device_parent
 * returns the "base" libretro device type, which we may sometimes need
 */
unsigned get_device_parent(unsigned device_id)
{
  if(device_id == RETRO_DEVICE_NONE) return RETRO_DEVICE_NONE;

  switch(device_id)
  {
    case PAD_CLASSIC:
    case PAD_FIGHTSTICK:
    case PAD_8BUTTON:
    case PAD_6BUTTON:
      return RETRO_DEVICE_JOYPAD;
  }

  return INT_MAX;
}

/* get_retropad_code
 * converts from OSD_ in mame2003.h to the codes from libretro.h
 * returns INT_MAX if the code is not valid
 */
int get_retropad_code(unsigned osd_id)
{
  switch(osd_id)
  {
    case  OSD_JOYPAD_B:       return RETRO_DEVICE_ID_JOYPAD_B;
    case  OSD_JOYPAD_Y:       return RETRO_DEVICE_ID_JOYPAD_Y;
    case  OSD_JOYPAD_SELECT:  return RETRO_DEVICE_ID_JOYPAD_SELECT;
    case  OSD_JOYPAD_START:   return RETRO_DEVICE_ID_JOYPAD_START;
    case  OSD_JOYPAD_UP:      return RETRO_DEVICE_ID_JOYPAD_UP;
    case  OSD_JOYPAD_DOWN:    return RETRO_DEVICE_ID_JOYPAD_DOWN;
    case  OSD_JOYPAD_LEFT:    return RETRO_DEVICE_ID_JOYPAD_LEFT;
    case  OSD_JOYPAD_RIGHT:   return RETRO_DEVICE_ID_JOYPAD_RIGHT;
    case  OSD_JOYPAD_A:       return RETRO_DEVICE_ID_JOYPAD_A;
    case  OSD_JOYPAD_X:       return RETRO_DEVICE_ID_JOYPAD_X;
    case  OSD_JOYPAD_L:       return RETRO_DEVICE_ID_JOYPAD_L;
    case  OSD_JOYPAD_R:       return RETRO_DEVICE_ID_JOYPAD_R;
    case  OSD_JOYPAD_L2:      return RETRO_DEVICE_ID_JOYPAD_L2;
    case  OSD_JOYPAD_R2:      return RETRO_DEVICE_ID_JOYPAD_R2;
    case  OSD_JOYPAD_L3:      return RETRO_DEVICE_ID_JOYPAD_L3;
    case  OSD_JOYPAD_R3:      return RETRO_DEVICE_ID_JOYPAD_R3;
  }
  return INT_MAX; /* no match found */
}

/* converts from OSD_ in mame2003.h to the codes from libretro.h
 * returns INT_MAX if the code is not valid
 */
int get_retromouse_code(unsigned osd_id)
{
  switch(osd_id)
  {
    case  OSD_MOUSE_BUTTON_1:  return RETRO_DEVICE_ID_MOUSE_LEFT;
    case  OSD_MOUSE_BUTTON_2:  return RETRO_DEVICE_ID_MOUSE_RIGHT;
    case  OSD_MOUSE_BUTTON_3:  return RETRO_DEVICE_ID_MOUSE_MIDDLE;
    case  OSD_MOUSE_BUTTON_4:  return RETRO_DEVICE_ID_MOUSE_BUTTON_4;
    case  OSD_MOUSE_BUTTON_5:  return RETRO_DEVICE_ID_MOUSE_BUTTON_5;
  }
  return INT_MAX; /* no match found */
}

/* converts from OSD_ in mame2003.h to the codes from libretro.h
 * returns INT_MAX if the code is not valid
 */
int get_retrogun_code(unsigned osd_id)
{
  switch(osd_id)
  {
    case  OSD_LIGHTGUN_IS_OFFSCREEN:    return RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN;
    case  OSD_LIGHTGUN_IS_TRIGGER:      return RETRO_DEVICE_ID_LIGHTGUN_TRIGGER;       /*Status Check*/
    case  OSD_LIGHTGUN_RELOAD:          return RETRO_DEVICE_ID_LIGHTGUN_RELOAD;        /*Forced off-screen shot*/
    case  OSD_LIGHTGUN_AUX_A:           return RETRO_DEVICE_ID_LIGHTGUN_AUX_A;
    case  OSD_LIGHTGUN_AUX_B:           return RETRO_DEVICE_ID_LIGHTGUN_AUX_B;
    case  OSD_LIGHTGUN_START:           return RETRO_DEVICE_ID_LIGHTGUN_START;
    case  OSD_LIGHTGUN_SELECT:          return RETRO_DEVICE_ID_LIGHTGUN_SELECT;
    case  OSD_LIGHTGUN_AUX_C:           return RETRO_DEVICE_ID_LIGHTGUN_AUX_C;
    case  OSD_LIGHTGUN_DPAD_UP:         return RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP;
    case  OSD_LIGHTGUN_DPAD_DOWN:       return RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN;
    case  OSD_LIGHTGUN_DPAD_LEFT:       return RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT;
    case  OSD_LIGHTGUN_DPAD_RIGHT:      return RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT;
  }
  return INT_MAX; /* no match found */
}

/* surely there is a MAME function equivalent already for JOYCODE_BUTTON_COMPARE, 
 * MOUSECODE_BUTTON_COMPARE, and get_ctrl_ipt_code(), etc. but I haven't found it.
 */

#define BUTTON_CODE_COMPARE(BUTTON_NO)                \
  switch(standard_code)                               \
  {                                                   \
    case JOYCODE_1_BUTTON##BUTTON_NO:                 \
    case JOYCODE_2_BUTTON##BUTTON_NO:                 \
    case JOYCODE_3_BUTTON##BUTTON_NO:                 \
    case JOYCODE_4_BUTTON##BUTTON_NO:                 \
    case JOYCODE_5_BUTTON##BUTTON_NO:                 \
    case JOYCODE_6_BUTTON##BUTTON_NO:                 \
    case JOYCODE_7_BUTTON##BUTTON_NO:                 \
    case JOYCODE_8_BUTTON##BUTTON_NO:                 \
      return player_flag | IPT_BUTTON##BUTTON_NO;     \
  }                                                   \

#define MOUSE_CODE_COMPARE(BUTTON_NO)                 \
  switch(standard_code)                               \
  {                                                   \
    case JOYCODE_MOUSE_1_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_2_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_3_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_4_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_5_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_6_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_7_BUTTON##BUTTON_NO:           \
    case JOYCODE_MOUSE_8_BUTTON##BUTTON_NO:           \
      return player_flag | IPT_BUTTON##BUTTON_NO;     \
  }                                                   \

#define GUN_CODE_COMPARE(BUTTON_NO)                 \
  switch(standard_code)                               \
  {                                                   \
    case JOYCODE_GUN_1_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_2_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_3_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_4_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_5_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_6_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_7_BUTTON##BUTTON_NO:           \
    case JOYCODE_GUN_8_BUTTON##BUTTON_NO:           \
      return player_flag | IPT_BUTTON##BUTTON_NO;     \
  }                                                   \

#define COMMON_CONTROLS_COMPARE(PLAYER_NUMBER)                                        \
  switch(standard_code)                                                               \
  {                                                                                   \
    case JOYCODE_##PLAYER_NUMBER##_SELECT: return IPT_COIN##PLAYER_NUMBER;            \
    case JOYCODE_##PLAYER_NUMBER##_START:  return IPT_START##PLAYER_NUMBER;           \
    case JOYCODE_##PLAYER_NUMBER##_UP:     return player_flag | IPT_JOYSTICK_UP;      \
    case JOYCODE_##PLAYER_NUMBER##_DOWN:   return player_flag | IPT_JOYSTICK_DOWN;    \
    case JOYCODE_##PLAYER_NUMBER##_LEFT:   return player_flag | IPT_JOYSTICK_LEFT;    \
    case JOYCODE_##PLAYER_NUMBER##_RIGHT:  return player_flag | IPT_JOYSTICK_RIGHT;   \
  }                                                                                   \

unsigned get_ctrl_ipt_code(unsigned player_number, unsigned standard_code)
{
  int player_flag = 0;

  switch(player_number)
  {
    case 1: player_flag = IPF_PLAYER1; break;
    case 2: player_flag = IPF_PLAYER2; break;
    case 3: player_flag = IPF_PLAYER3; break;
    case 4: player_flag = IPF_PLAYER4; break;
    case 5: player_flag = IPF_PLAYER5; break;
    case 6: player_flag = IPF_PLAYER6; break;
    case 7: player_flag = IPF_PLAYER7; break;
    case 8: player_flag = IPF_PLAYER8; break;
    default: player_flag = IPF_PLAYER1; break;
  }
  /* use macros to hide simplistic and verbose implementation */
  BUTTON_CODE_COMPARE(1) /* look for "button 1" */
  BUTTON_CODE_COMPARE(2) /* button 2 */
  BUTTON_CODE_COMPARE(3) /* button 3 */
  BUTTON_CODE_COMPARE(4)
  BUTTON_CODE_COMPARE(5)
  BUTTON_CODE_COMPARE(6)
  BUTTON_CODE_COMPARE(7)
  BUTTON_CODE_COMPARE(8)
  BUTTON_CODE_COMPARE(9)
  BUTTON_CODE_COMPARE(10)

  MOUSE_CODE_COMPARE(1) /* look for "mouse button 1" */
  MOUSE_CODE_COMPARE(2) /* mouse button 2 */
  MOUSE_CODE_COMPARE(3) /* mouse button 3 */
  MOUSE_CODE_COMPARE(4)
  MOUSE_CODE_COMPARE(5)
  MOUSE_CODE_COMPARE(6)

  GUN_CODE_COMPARE(1)
  GUN_CODE_COMPARE(2)
  GUN_CODE_COMPARE(3)
  GUN_CODE_COMPARE(4)

  COMMON_CONTROLS_COMPARE(1) /* player 1 */
  COMMON_CONTROLS_COMPARE(2) /* player 2 */
  COMMON_CONTROLS_COMPARE(3) /* player 3 */
  COMMON_CONTROLS_COMPARE(4) /* player 4 */
  COMMON_CONTROLS_COMPARE(5) /* player 5 */
  COMMON_CONTROLS_COMPARE(6) /* player 6 */
  COMMON_CONTROLS_COMPARE(7) /* player 7 */
  COMMON_CONTROLS_COMPARE(8) /* player 8 */

  return CODE_NONE; /* not any of the BUTTON joycodes or mouse codes */
}


/*
 * Each line created by these emitters composes a JoystickInfo struct.
 *
 * struct JoystickInfo
 * {
 *   const char *name;        // OS dependant name; 0 terminates the list
 *   unsigned code;           // OS dependant code
 *   InputCode standardcode;	// CODE_xxx equivalent from list in input.h, or CODE_OTHER if n/a
 * };
 *
 * In the context of MAME 2003+, the 'OS' is the libretro, so we determine the unique codes for
 * our input. When the control mappings are emitted, the input codes are incremented by 1000 * the
 * player number so that simple arithmetic can determine which is the associated player.
 */

#define EMIT_RETROPAD_CLASSIC(DISPLAY_IDX) \
  {"RP"  #DISPLAY_IDX " B",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_B,   JOYCODE_##DISPLAY_IDX##_BUTTON1},  \
  {"RP"  #DISPLAY_IDX " A",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_A,   JOYCODE_##DISPLAY_IDX##_BUTTON2},  \
  {"RP"  #DISPLAY_IDX " Y",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_Y,   JOYCODE_##DISPLAY_IDX##_BUTTON3},  \
  {"RP"  #DISPLAY_IDX " X",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_X,   JOYCODE_##DISPLAY_IDX##_BUTTON4},  \
  {"RP"  #DISPLAY_IDX " L",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_L,   JOYCODE_##DISPLAY_IDX##_BUTTON5},  \
  {"RP"  #DISPLAY_IDX " R",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_R,   JOYCODE_##DISPLAY_IDX##_BUTTON6},  \
  {"RP"  #DISPLAY_IDX " L2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L2,  JOYCODE_##DISPLAY_IDX##_BUTTON7},  \
  {"RP"  #DISPLAY_IDX " R2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R2,  JOYCODE_##DISPLAY_IDX##_BUTTON8},  \
  {"RP"  #DISPLAY_IDX " L3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L3,  JOYCODE_##DISPLAY_IDX##_BUTTON9},  \
  {"RP"  #DISPLAY_IDX " R3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R3,  JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  EMIT_COMMON_CODES(DISPLAY_IDX)

#define EMIT_RETROPAD_FIGHTSTICK(DISPLAY_IDX) \
  {"RP"  #DISPLAY_IDX " Y",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_Y,   JOYCODE_##DISPLAY_IDX##_BUTTON1},  \
  {"RP"  #DISPLAY_IDX " X",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_X,   JOYCODE_##DISPLAY_IDX##_BUTTON2},  \
  {"RP"  #DISPLAY_IDX " R",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_R,   JOYCODE_##DISPLAY_IDX##_BUTTON3},  \
  {"RP"  #DISPLAY_IDX " B",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_B,   JOYCODE_##DISPLAY_IDX##_BUTTON4},  \
  {"RP"  #DISPLAY_IDX " A",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_A,   JOYCODE_##DISPLAY_IDX##_BUTTON5},  \
  {"RP"  #DISPLAY_IDX " R2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R2,  JOYCODE_##DISPLAY_IDX##_BUTTON6},  \
  {"RP"  #DISPLAY_IDX " L",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_L,   JOYCODE_##DISPLAY_IDX##_BUTTON7},  \
  {"RP"  #DISPLAY_IDX " L2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L2,  JOYCODE_##DISPLAY_IDX##_BUTTON8},  \
  {"RP"  #DISPLAY_IDX " L3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L3,  JOYCODE_##DISPLAY_IDX##_BUTTON9},  \
  {"RP"  #DISPLAY_IDX " R3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R3,  JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  EMIT_COMMON_CODES(DISPLAY_IDX)

#define EMIT_RETROPAD_8BUTTON(DISPLAY_IDX) \
  {"RP"  #DISPLAY_IDX " Y",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_Y,   JOYCODE_##DISPLAY_IDX##_BUTTON1},  \
  {"RP"  #DISPLAY_IDX " X",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_X,   JOYCODE_##DISPLAY_IDX##_BUTTON2},  \
  {"RP"  #DISPLAY_IDX " L",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_L,   JOYCODE_##DISPLAY_IDX##_BUTTON3},  \
  {"RP"  #DISPLAY_IDX " B",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_B,   JOYCODE_##DISPLAY_IDX##_BUTTON4},  \
  {"RP"  #DISPLAY_IDX " A",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_A,   JOYCODE_##DISPLAY_IDX##_BUTTON5},  \
  {"RP"  #DISPLAY_IDX " L2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L2,  JOYCODE_##DISPLAY_IDX##_BUTTON6},  \
  {"RP"  #DISPLAY_IDX " R",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_R,   JOYCODE_##DISPLAY_IDX##_BUTTON7},  \
  {"RP"  #DISPLAY_IDX " R2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R2,  JOYCODE_##DISPLAY_IDX##_BUTTON8},  \
  {"RP"  #DISPLAY_IDX " L3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L3,  JOYCODE_##DISPLAY_IDX##_BUTTON9},  \
  {"RP"  #DISPLAY_IDX " R3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R3,  JOYCODE_##DISPLAY_IDX##_BUTTON10}, \
  EMIT_COMMON_CODES(DISPLAY_IDX)

#define EMIT_RETROPAD_6BUTTON(DISPLAY_IDX) \
  {"RP"  #DISPLAY_IDX " Y",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_Y,   JOYCODE_##DISPLAY_IDX##_BUTTON1}, \
  {"RP"  #DISPLAY_IDX " X",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_X,   JOYCODE_##DISPLAY_IDX##_BUTTON2}, \
  {"RP"  #DISPLAY_IDX " L",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_L,   JOYCODE_##DISPLAY_IDX##_BUTTON3}, \
  {"RP"  #DISPLAY_IDX " B",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_B,   JOYCODE_##DISPLAY_IDX##_BUTTON4}, \
  {"RP"  #DISPLAY_IDX " A",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_A,   JOYCODE_##DISPLAY_IDX##_BUTTON5}, \
  {"RP"  #DISPLAY_IDX " R",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_R,   JOYCODE_##DISPLAY_IDX##_BUTTON6}, \
  {"RP"  #DISPLAY_IDX " L2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L2,  JOYCODE_##DISPLAY_IDX##_BUTTON7}, \
  {"RP"  #DISPLAY_IDX " R2",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R2,  JOYCODE_##DISPLAY_IDX##_BUTTON8}, \
  {"RP"  #DISPLAY_IDX " L3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_L3,  JOYCODE_##DISPLAY_IDX##_BUTTON9}, \
  {"RP"  #DISPLAY_IDX " R3",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_R3,  JOYCODE_##DISPLAY_IDX##_BUTTON10},\
  EMIT_COMMON_CODES(DISPLAY_IDX)

/* RetroPad-type input devices: */
/* The dpad, start, select, mouse, and analog axes are the same regardless of layout */
#define EMIT_COMMON_CODES(DISPLAY_IDX) \
\
  {"RP" #DISPLAY_IDX " HAT Left",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_LEFT,  JOYCODE_##DISPLAY_IDX##_LEFT},  \
  {"RP" #DISPLAY_IDX " HAT Right",  (DISPLAY_IDX * 1000) + OSD_JOYPAD_RIGHT, JOYCODE_##DISPLAY_IDX##_RIGHT}, \
  {"RP" #DISPLAY_IDX " HAT Up",     (DISPLAY_IDX * 1000) + OSD_JOYPAD_UP,    JOYCODE_##DISPLAY_IDX##_UP},    \
  {"RP" #DISPLAY_IDX " HAT Down",   (DISPLAY_IDX * 1000) + OSD_JOYPAD_DOWN,  JOYCODE_##DISPLAY_IDX##_DOWN},  \
\
  {"RP" #DISPLAY_IDX " Start",      (DISPLAY_IDX * 1000) + OSD_JOYPAD_START,  JOYCODE_##DISPLAY_IDX##_START},  \
  {"RP" #DISPLAY_IDX " Select",     (DISPLAY_IDX * 1000) + OSD_JOYPAD_SELECT, JOYCODE_##DISPLAY_IDX##_SELECT}, \
\
  {"RP" #DISPLAY_IDX " AXIS 0 X-",  (DISPLAY_IDX * 1000) + OSD_ANALOG_LEFT_NEGATIVE_X,  JOYCODE_##DISPLAY_IDX##_LEFT_LEFT},   \
  {"RP" #DISPLAY_IDX " AXIS 0 X+",  (DISPLAY_IDX * 1000) + OSD_ANALOG_LEFT_POSITIVE_X,  JOYCODE_##DISPLAY_IDX##_LEFT_RIGHT},  \
  {"RP" #DISPLAY_IDX " AXIS 1 Y-",  (DISPLAY_IDX * 1000) + OSD_ANALOG_LEFT_NEGATIVE_Y,  JOYCODE_##DISPLAY_IDX##_LEFT_UP},     \
  {"RP" #DISPLAY_IDX " AXIS 1 Y+",  (DISPLAY_IDX * 1000) + OSD_ANALOG_LEFT_POSITIVE_Y,  JOYCODE_##DISPLAY_IDX##_LEFT_DOWN},   \
  {"RP" #DISPLAY_IDX " AXIS 2 X-",  (DISPLAY_IDX * 1000) + OSD_ANALOG_RIGHT_NEGATIVE_X, JOYCODE_##DISPLAY_IDX##_RIGHT_LEFT},  \
  {"RP" #DISPLAY_IDX " AXIS 2 X+",  (DISPLAY_IDX * 1000) + OSD_ANALOG_RIGHT_POSITIVE_X, JOYCODE_##DISPLAY_IDX##_RIGHT_RIGHT}, \
  {"RP" #DISPLAY_IDX " AXIS 3 Y-",  (DISPLAY_IDX * 1000) + OSD_ANALOG_RIGHT_NEGATIVE_Y, JOYCODE_##DISPLAY_IDX##_RIGHT_UP},    \
  {"RP" #DISPLAY_IDX " AXIS 3 Y+",  (DISPLAY_IDX * 1000) + OSD_ANALOG_RIGHT_POSITIVE_Y, JOYCODE_##DISPLAY_IDX##_RIGHT_DOWN},  \
\
  {"Mouse" #DISPLAY_IDX " Button1", (DISPLAY_IDX * 1000) + OSD_MOUSE_BUTTON_1, JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON1}, \
  {"Mouse" #DISPLAY_IDX " Button2", (DISPLAY_IDX * 1000) + OSD_MOUSE_BUTTON_2, JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON2}, \
  {"Mouse" #DISPLAY_IDX " Button3", (DISPLAY_IDX * 1000) + OSD_MOUSE_BUTTON_3, JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON3}, \
  {"Mouse" #DISPLAY_IDX " Button4", (DISPLAY_IDX * 1000) + OSD_MOUSE_BUTTON_4, JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON4}, \
  {"Mouse" #DISPLAY_IDX " Button5", (DISPLAY_IDX * 1000) + OSD_MOUSE_BUTTON_5, JOYCODE_MOUSE_##DISPLAY_IDX##_BUTTON5}, \
\
  EMIT_LIGHTGUN(DISPLAY_IDX)

#define EMIT_LIGHTGUN(DISPLAY_IDX) \
  {"Gun" #DISPLAY_IDX " Trigger",     (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_IS_TRIGGER, JOYCODE_GUN_##DISPLAY_IDX##_BUTTON1    }, \
  {"Gun" #DISPLAY_IDX " Aux A",       (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_AUX_A,      JOYCODE_GUN_##DISPLAY_IDX##_BUTTON2    }, \
  {"Gun" #DISPLAY_IDX " Aux B",       (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_AUX_B,      JOYCODE_GUN_##DISPLAY_IDX##_BUTTON3    }, \
  {"Gun" #DISPLAY_IDX " Aux C",       (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_AUX_C,      JOYCODE_GUN_##DISPLAY_IDX##_BUTTON4    }, \
  {"Gun" #DISPLAY_IDX " Start",       (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_START,      JOYCODE_GUN_##DISPLAY_IDX##_START       }, \
  {"Gun" #DISPLAY_IDX " Select",      (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_SELECT,     JOYCODE_GUN_##DISPLAY_IDX##_SELECT      }, \
  {"Gun" #DISPLAY_IDX " DPad Left",   (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_DPAD_LEFT,  JOYCODE_GUN_##DISPLAY_IDX##_DPAD_LEFT   }, \
  {"Gun" #DISPLAY_IDX " DPad Right",  (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_DPAD_RIGHT, JOYCODE_GUN_##DISPLAY_IDX##_DPAD_RIGHT  }, \
  {"Gun" #DISPLAY_IDX " DPad Up",     (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_DPAD_UP,    JOYCODE_GUN_##DISPLAY_IDX##_DPAD_UP     }, \
  {"Gun" #DISPLAY_IDX " DPad Down",   (DISPLAY_IDX * 1000) + OSD_LIGHTGUN_DPAD_DOWN,  JOYCODE_GUN_##DISPLAY_IDX##_DPAD_DOWN   },

#define EMIT_JOYSTICK_OPTIONS(DISPLAY_IDX)      \
  {                                             \
    { EMIT_RETROPAD_CLASSIC(DISPLAY_IDX)    },     \
    { EMIT_RETROPAD_FIGHTSTICK(DISPLAY_IDX) },     \
    { EMIT_RETROPAD_8BUTTON(DISPLAY_IDX)    },     \
    { EMIT_RETROPAD_6BUTTON(DISPLAY_IDX)    },     \
  },

struct JoystickInfo alternate_joystick_maps[MAX_PLAYER_COUNT][IDX_NUMBER_OF_INPUT_TYPES][OSD_INPUT_CODES_PER_PLAYER] =
{
  EMIT_JOYSTICK_OPTIONS(1)
  EMIT_JOYSTICK_OPTIONS(2)
  EMIT_JOYSTICK_OPTIONS(3)
  EMIT_JOYSTICK_OPTIONS(4)
  EMIT_JOYSTICK_OPTIONS(5)
  EMIT_JOYSTICK_OPTIONS(6)
  EMIT_JOYSTICK_OPTIONS(7)
  EMIT_JOYSTICK_OPTIONS(8)
};

/******************************************************************************

	Joystick

******************************************************************************/

struct JoystickInfo mame_joy_map[(MAX_PLAYER_COUNT * OSD_INPUT_CODES_PER_PLAYER) + 1] = { { 0 } }; /* + 1 for final zeroed struct */

const struct JoystickInfo *osd_get_joy_list(void)
{
  unsigned needle      = 0;
  unsigned port_number = 0;

  for(port_number = 0; port_number < MAX_PLAYER_COUNT; port_number++)
  {
    int control_idx = 0;
    for(control_idx = 0; control_idx < OSD_INPUT_CODES_PER_PLAYER; control_idx++)
    {
      int layout_idx   = 0;
      switch(options.active_control_type[port_number])
      {
        case PAD_CLASSIC:      layout_idx = IDX_CLASSIC;      break;
        case PAD_FIGHTSTICK:   layout_idx = IDX_FIGHTSTICK;   break;
        case PAD_8BUTTON:      layout_idx = IDX_8BUTTON;      break;
        case PAD_6BUTTON:      layout_idx = IDX_6BUTTON;      break;
      }
      mame_joy_map[needle] = alternate_joystick_maps[port_number][layout_idx][control_idx];
      if(!string_is_empty(mame_joy_map[needle].name)) needle++;
    }
  }

  /* the extra final record remains zeroed to indicate the end of the description to the frontend */
  mame_joy_map[needle].name         = NULL;
  mame_joy_map[needle].code         = 0;
  mame_joy_map[needle].standardcode = 0;

  return mame_joy_map;
}

/*
 * When the control mappings are emitted, the input codes are incremented by
 * 1000, 2000, 3000, etc as a simple way to indicate the corresponding player index.
 */
int osd_is_joy_pressed(int joycode)
{
  if (!retro_running)                                   return 0; /* input callback has not yet been polled */
  if (options.input_interface == RETRO_DEVICE_KEYBOARD) return 0; /* disregard joystick input */

  unsigned player_number = calc_player_number(joycode);
  unsigned port          = player_number - 1;
  unsigned osd_code      = decode_osd_joycode(joycode);
  unsigned retro_code    = INT_MAX;

  /*log_cb(RETRO_LOG_DEBUG, "MAME is polling joysticks -- joycode: %i      player_number: %i      osd_code: %i\n", joycode, player_number, osd_code);*/

  /* standard retropad states */
  retro_code = get_retropad_code(osd_code);
  if (retro_code != INT_MAX)
    return input_cb(port, RETRO_DEVICE_JOYPAD, 0, retro_code);

  /* pointer, mouse, or lightgun states if selected by core option */
  if (options.mouse_device == RETRO_DEVICE_POINTER || options.mouse_device == RETRO_DEVICE_MOUSE)
  {
    retro_code = get_retromouse_code(osd_code);
    if (retro_code != INT_MAX)
    {
#ifdef __ANDROID__
      if (port > 0) return 0;
#endif
      if (options.mouse_device == RETRO_DEVICE_MOUSE)
        return input_cb(port, RETRO_DEVICE_MOUSE, 0, retro_code);
      if (options.mouse_device == RETRO_DEVICE_POINTER && retro_code == RETRO_DEVICE_ID_MOUSE_LEFT)
        return input_cb(port, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
    }
  }

  else if (options.mouse_device == RETRO_DEVICE_LIGHTGUN)
  {
    retro_code = get_retrogun_code(osd_code);
    if (retro_code != INT_MAX)
    {
#ifdef __ANDROID__
      if (port > 0) return 0;
#endif
      if (retro_code == RETRO_DEVICE_ID_LIGHTGUN_TRIGGER)
      {
        if (input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD))
          return 1; /* lightgun reload hack, report trigger as being pressed */
      }
      return input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, retro_code);
    }
  }

  /* Use the cached input states */
  return retroJsState[port][osd_code];
}


int osd_is_joystick_axis_code(unsigned joycode)
{
  unsigned osd_code = decode_osd_joycode(joycode);

  if(osd_code >= OSD_ANALOG_LEFT_NEGATIVE_X && osd_code <= OSD_ANALOG_RIGHT_POSITIVE_Y)
    return 1;

  return 0;
}

unsigned calc_player_number(unsigned joycode)
{
  return (joycode / 1000);
}

unsigned encode_osd_joycode(unsigned player_number, unsigned raw_code)
{
  if(raw_code >= OSD_INPUT_CODES_PER_PLAYER)
    return INT_MAX;

  return (raw_code + (player_number * 1000));
}

unsigned decode_osd_joycode(unsigned joycode)
{
  return (joycode - (calc_player_number(joycode) * 1000));
}

void osd_analogjoy_read(int player, int analog_axis[MAX_ANALOG_AXES], InputCode analogjoy_input[MAX_ANALOG_AXES])
{
  int i;
  int value;

  for(i = 0; i < MAX_ANALOG_AXES; i ++)
  {
    int osd_code;
    value = 0;
    if(analogjoy_input[i] != CODE_NONE)
    {
      osd_code = decode_osd_joycode(analogjoy_input[i]);

      if(osd_code == OSD_ANALOG_LEFT_NEGATIVE_X || osd_code == OSD_ANALOG_LEFT_POSITIVE_X)
        value = analogjoy[player][0];

      else if(osd_code == OSD_ANALOG_LEFT_NEGATIVE_Y || osd_code == OSD_ANALOG_LEFT_POSITIVE_Y)
        value = analogjoy[player][1];

      else if(osd_code == OSD_ANALOG_RIGHT_NEGATIVE_X || osd_code == OSD_ANALOG_RIGHT_POSITIVE_X)
        value = analogjoy[player][2];

      else if(osd_code == OSD_ANALOG_RIGHT_NEGATIVE_Y || osd_code == OSD_ANALOG_RIGHT_POSITIVE_Y)
        value = analogjoy[player][3];

      /* opposite when reversing axis mapping */
      if((osd_code % 2) == 0) /* if osd_code is an even number */
        value = -value;

      analog_axis[i]=value;
    }
  }
}


int analog_deadzone_rescale(int input)
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
		input = round(scaled);

		if (input > +32767)
		{
			input = +32767;
		}
		input = input / 327.68;
	}

	else
	{
		input = 0;
	}


	if (neg_test) input =-abs(input);
	return (int) input * 1.28;
}

/******************************************************************************
 *
 * Legacy joystick calibration functions
 *
 * As of March 2021: these MAME functions should not actually be used and will not be invoked
 * as long as needs_calibration always returns 0. The libretro frontend is reponsible for
 * providing calibrated position data.
 ******************************************************************************/

/* Joystick calibration routines BW 19981216 */
int osd_joystick_needs_calibration(void) { return 0; }

/* Preprocessing for joystick calibration. Returns 0 on success */
void osd_joystick_start_calibration(void){ }

/* Prepare the next calibration step. Return a description of this step. */
/* (e.g. "move to upper left") */
const char *osd_joystick_calibrate_next(void) { return 0; }

/* Get the actual joystick calibration data for the current position */
void osd_joystick_calibrate(void) { }

/* Postprocessing (e.g. saving joystick data to config) */
void osd_joystick_end_calibration(void) { }



/******************************************************************************

	Trackball, Spinner, Mouse, Pointer, Lightgun

******************************************************************************/

void osd_xy_device_read(int player, int *deltax, int *deltay)
{
#ifdef __ANDROID__
    if(player > 0)
    {
      *deltax = 0;
      *deltay = 0;
      return;
    }
#endif

  if (options.mouse_device == RETRO_DEVICE_POINTER)
  {
    bool pointer_pressed = input_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
    *deltax = pointer_pressed ? get_pointer_delta(input_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X), &prev_pointer_x) : 0;
    *deltay = pointer_pressed ? get_pointer_delta(input_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y), &prev_pointer_y) : 0;
  }

  else if (options.mouse_device == RETRO_DEVICE_MOUSE)
  {
    *deltax = input_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
    *deltay = input_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
  }

  else if (options.mouse_device == RETRO_DEVICE_LIGHTGUN)
  {
    /* simulated lightgun reload hack */
    if(input_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD))
    {
      *deltax = -128;
      *deltay = -128;
      return;
    }
    *deltax = rescale_analog(input_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X));
    *deltay = rescale_analog(input_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y));
  }

  else    /* RETRO_DEVICE_NONE */
  {
    *deltax = 0;
    *deltay = 0;
  }
}

/******************************************************************************
 * rescale_analog converts between the libretro coordinate system and the
 * MAME OSD coordinate system.
 * 
 * RETRO_DEVICE_LIGHTGUN report X/Y coordinates in the range [-0x8000, 0x7fff]
 * in both axes, with zero being center and -0x8000 being out of bounds.
 * RETRO_DEVICE_ANALOG uses the same [-0x8000, 0x7fff] range.
 * 
 * For lightguns, the MAME OSD uses delta from the middle of the screen when
 * the lightgun is fired, and 0 when the gun is inactive with a range of 
 * -128 to 128. MAME OSD uses this same range for analog joysticks.
 * 
 * Therefore we can use a common function to scale input from lightguns and
 * analog controls.
 ******************************************************************************/
int rescale_analog(int libretro_coordinate)
{
  static const float scale_factor = (float)MAME_ANALOG_MAX / LIBRETRO_ANALOG_MAX;

  if (libretro_coordinate == 0 || libretro_coordinate == LIBRETRO_ANALOG_MIN) return 0;

  return round(scale_factor * libretro_coordinate);
}

/******************************************************************************

	Keyboard

******************************************************************************/


const struct KeyboardInfo *osd_get_key_list(void)
{
  return retroKeys;
}

int osd_is_key_pressed(int keycode)
{
  if (!retro_running)                                  return 0; /* input callback has not yet been polled */
  if (options.input_interface == RETRO_DEVICE_JOYPAD)  return 0; /* core option is set to retropad/joystick only */

  if (keycode < RETROK_LAST && keycode >= 0)
    return input_cb(0, RETRO_DEVICE_KEYBOARD, 0, keycode);

  log_cb(RETRO_LOG_WARN, LOGPRE "Invalid OSD keycode received: %i\n", keycode); /* this should not happen when keycodes are properly registered with MAME */
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


/*  MAME requires that we populate an array of KeyboardInfo structs. The code value
 *  used is up to the OSD, which in this case is the libretro API. libretro.h provides
 *  a set of keycodes suitable for this purpose, so we use those for populating our
 *  KeyboardInfo structs with the help of #define emitters.
 *
 *  struct KeyboardInfo
 *  {
 *    const char *name;       // OS dependant name; 0 terminates the list
 *    unsigned code;          // OS dependant code
 *    InputCode standardcode;	// CODE_xxx equivalent from list below, or CODE_OTHER if n/a
 * };
 *
 * Unassigned keycodes
 *	KEYCODE_OPENBRACE, KEYCODE_CLOSEBRACE, KEYCODE_BACKSLASH2, KEYCODE_STOP, KEYCODE_LWIN,
 *  KEYCODE_RWIN, KEYCODE_DEL_PAD, KEYCODE_PAUSE
 *
 * The format for each systems key constants is RETROK_$(TAG) and KEYCODE_$(TAG)
 * EMIT1(TAG): The tag value is the same between libretro and the core
 * EMIT2(RTAG, MTAG): The tag value is different between the two
 * EXITX(TAG): The core has no equivalent key.
 */

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

/******************************************************************************

	Utility functions

******************************************************************************/

/* inptport.c defines general purpose defaults for key and joystick bindings which
 * may be further adjusted by the OS dependent code to better match the available
 * keyboard, e.g. one could map pause to the Pause key instead of P, or snapshot
 * to PrtScr instead of F12. Of course the user can further change the settings
 * to anything they like.
 *
 * osd_customize_inputport_defaults is called on startup, before reading the
 * configuration from disk. Scan the list, and change the keys/joysticks you want.
 */
void osd_customize_inputport_defaults(struct ipd *defaults){}


static void remove_slash (char* temp)
{
  int i;

  for(i=0; temp[i] != '\0'; ++i);

  if( (temp[i-1] == '/' || temp[i-1] == '\\') && (i > 1) )
  {
    temp[i-1] = 0;
    log_cb(RETRO_LOG_DEBUG, LOGPRE "Removed a trailing slash in path: %s\n", temp);
  }
  else
    log_cb(RETRO_LOG_DEBUG, LOGPRE "Trailing slash removal was not necessary path: %s.\n", temp);
}
