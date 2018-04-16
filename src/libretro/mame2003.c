/*********************************************************************

	mame2003.c
    
    a port of mame 0.78 to the libretro API
    
*********************************************************************/    

#include <stdint.h>
#include <libretro.h>
#include <file_path.h>

#include "mame.h"
#include "driver.h"
#include "state.h"


extern int framerate_test;

static int driverIndex; /* Index of mame game loaded */
extern struct osd_create_params videoConfig;

static float delta_samples;
int samples_per_frame = 0;
short *samples_buffer;
short *conversion_buffer;
int usestereo = 1;

extern const struct KeyboardInfo retroKeys[];
extern int retroKeyState[512];
extern int retroJsState[72];
extern int16_t mouse_x[4];
extern int16_t mouse_y[4];
int16_t prev_pointer_x;
int16_t prev_pointer_y;
extern int16_t analogjoy[4][4];

struct retro_perf_callback perf_cb;
unsigned retroColorMode;

retro_environment_t environ_cb = NULL;
retro_log_printf_t log_cb = NULL;
retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;

unsigned long lastled = 0;
retro_set_led_state_t led_state_cb = NULL;

int16_t XsoundBuffer[2048];


#ifdef _3DS /* TODO: convert this strcasecmp wrapper to libretro-common/compat functions */
int stricmp(const char *string1, const char *string2)
{
    return strcasecmp(string1, string2); /* Wrapper to build MAME on 3DS. It doesn't have stricmp. */
}
#endif

/******************************************************************************

Sound

******************************************************************************/

int osd_start_audio_stream(int stereo)
{
	delta_samples = 0.0f;
	usestereo = stereo ? 1 : 0;

	/* determine the number of samples per frame */
	samples_per_frame = Machine->sample_rate / Machine->drv->frames_per_second;

	if (Machine->sample_rate == 0) return 0;

	samples_buffer = (short *) calloc(samples_per_frame, 2 + usestereo * 2);
	if (!usestereo) conversion_buffer = (short *) calloc(samples_per_frame, 4);
	
	return samples_per_frame;


}

int osd_update_audio_stream(INT16 *buffer)
{
	memcpy(samples_buffer, buffer, samples_per_frame * (usestereo ? 4 : 2));
   	delta_samples += (Machine->sample_rate / Machine->drv->frames_per_second) - samples_per_frame;
	if (delta_samples >= 1.0f)
	{
		int integer_delta = (int)delta_samples;
		samples_per_frame += integer_delta;
		delta_samples -= integer_delta;
	}

	return samples_per_frame;
}



void osd_stop_audio_stream(void)
{
}


/******************************************************************************

Miscellaneous

******************************************************************************/

void mame_frame(void);
void mame_done(void);

unsigned retro_get_region (void) {return RETRO_REGION_NTSC;}
void *retro_get_memory_data(unsigned type) {return 0;}
size_t retro_get_memory_size(unsigned type) {return 0;}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info){return false;}
void retro_cheat_reset(void){}
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2){}
void retro_set_controller_port_device(unsigned in_port, unsigned device){}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { APPNAME"-frameskip", "Frameskip; 0|1|2|3|4|5" },
      { APPNAME"-dcs-speedhack","MK2/MK3 DCS Speedhack; enabled|disabled"},
      { APPNAME"-skip_disclaimer", "Skip Disclaimer; enabled|disabled" },
      { APPNAME"-skip_warnings", "Skip Warnings; disabled|enabled" },
      { APPNAME"-sample_rate", "Sample Rate (KHz); 48000|8000|11025|22050|44100" },
      { APPNAME"-external_hiscore", "Use external hiscore.dat; disabled|enabled" },      
      { APPNAME"-dialsharexy", "Share 2 player dial controls across one X/Y device; disabled|enabled" },
#if defined(__IOS__)
      { APPNAME"-mouse_device", "Mouse Device; pointer|mouse|disabled" },
#else
      { APPNAME"-mouse_device", "Mouse Device; mouse|pointer|disabled" },
#endif
      { APPNAME"-crosshair_enabled", "Show Lightgun crosshair; enabled|disabled" },
      { APPNAME"-rstick_to_btns", "Right Stick to Buttons; enabled|disabled" },
      { APPNAME"-tate_mode", "TATE Mode; disabled|enabled" },
      { APPNAME"-skip-rom-verify", "EXPERIMENTAL: Skip ROM verification; disabled|enabled" }, 
      { APPNAME"-vector-resolution-multiplier", "EXPERIMENTAL: Vector resolution multiplier; 1|2|3|4|5|6" },      
      { APPNAME"-vector-antialias", "EXPERIMENTAL: Vector antialias; disabled" },
      { APPNAME"-vector-translucency", "Vector translucency; enabled|disabled" },
      { APPNAME"-vector-beam-width", "Vector beam width; 1|2|3|4|5" },
      { APPNAME"-vector-flicker", "Vector flicker; 20|0|10|20|30|40|50|60|70|80|90|100" },
      { APPNAME"-vector-intensity", "Vector intensity; 1.5|0.5|1|2|2.5|3" },
      { NULL, NULL },
   };
   environ_cb = cb;

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
}

#ifndef PATH_SEPARATOR
# if defined(WINDOWS_PATH_STYLE) || defined(_WIN32)
#  define PATH_SEPARATOR '\\'
# else
#  define PATH_SEPARATOR '/'
# endif
#endif

static char* normalizePath(char* aPath)
{
   char *tok;
   static const char replaced = (PATH_SEPARATOR == '\\') ? '/' : '\\';

   for (tok = strchr(aPath, replaced); tok; tok = strchr(aPath, replaced))
      *tok = PATH_SEPARATOR;

   return aPath;
}

static int getDriverIndex(const char* aPath)
{
    char driverName[128];
    char *path, *last;
    char *firstDot;
    int i;

    /* Get all chars after the last slash */
    path = normalizePath(strdup(aPath ? aPath : "."));
    last = strrchr(path, PATH_SEPARATOR);
    memset(driverName, 0, sizeof(driverName));
    strncpy(driverName, last ? last + 1 : path, sizeof(driverName) - 1);
    free(path);
    
    /* Remove extension */
    firstDot = strchr(driverName, '.');

    if(firstDot)
       *firstDot = 0;

    /* Search list */
    for (i = 0; drivers[i]; i++)
    {
       if(strcmp(driverName, drivers[i]->name) == 0)
       {
          if (log_cb)
             log_cb(RETRO_LOG_INFO, "Found game: %s [%s].\n", driverName, drivers[i]->name);
          return i;
       }
    }
    
    return -1;
}

static char* peelPathItem(char* aPath)
{
    char* last = strrchr(aPath, PATH_SEPARATOR);
    if(last)
       *last = 0;
    
    return aPath;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "MAME 2003-plus";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version = "0.78" GIT_VERSION;
   info->valid_extensions = "zip";
   info->need_fullpath = true;   
   info->block_extract = true;
}

static void update_variables(void)
{
   struct retro_led_interface ledintf;
   struct retro_variable var;

   var.value = NULL;
   var.key = APPNAME"-frameskip";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      options.frameskip = atoi(var.value);

   var.value = NULL;
   var.key = APPNAME"-dcs-speedhack";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.activate_dcs_speedhack = 1;
      else
         options.activate_dcs_speedhack = 0;
   }
   else
      options.activate_dcs_speedhack = 0;

   var.value = NULL;
   var.key = APPNAME"-skip_disclaimer";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.skip_disclaimer = 1;
      else
         options.skip_disclaimer = 0;
   }
   else
      options.skip_disclaimer = 0;

   var.value = NULL;
   var.key = APPNAME"-skip_warnings";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.skip_warnings = 1;
      else
         options.skip_warnings = 0;
   }
   else
      options.skip_warnings = 0;
   
   var.value = NULL;
   var.key = APPNAME"-sample_rate";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      options.samplerate = atoi(var.value);
   else
      options.samplerate = 48000;

   var.value = NULL;
   var.key = APPNAME"-external_hiscore";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.use_external_hiscore = 1;
      else
         options.use_external_hiscore = 0;
   }
   else
      options.use_external_hiscore = 0;  


   var.value = NULL;
   
   var.key = APPNAME"-dialsharexy";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.dial_share_xy = 1;
      else
         options.dial_share_xy = 0;
   }
   else
      options.dial_share_xy = 0;

   var.value = NULL;
   
   var.key = APPNAME"-mouse_device";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value)
   {
      if(strcmp(var.value, "pointer") == 0)
         options.mouse_device = RETRO_DEVICE_POINTER;
      else if(strcmp(var.value, "mouse") == 0)
         options.mouse_device = RETRO_DEVICE_MOUSE;
      else
         options.mouse_device = 0;
   }
   else
      options.mouse_device = 0;

   var.value = NULL;
   
   var.key = APPNAME"-crosshair_enabled";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.crosshair_enable = 1;
      else
         options.crosshair_enable = 0;
   }
   else
      options.crosshair_enable = 0;

   var.value = NULL;
   
   var.key = APPNAME"-rstick_to_btns";  
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.rstick_to_btns = 1;
      else
         options.rstick_to_btns = 0;
   }
   else
      options.rstick_to_btns = 0;

   var.value = NULL;
   
   var.key = APPNAME"-tate_mode";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.tate_mode = 1;
      else
         options.tate_mode = 0;
   }
   else
      options.tate_mode = 0;

   var.value = NULL;
   
   var.key = APPNAME"-skip-rom-verify"; 
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 1)
         options.skip_rom_verify = 1;
      else
         options.skip_rom_verify = 0;
   }
   else
      options.skip_rom_verify = 0;  

   var.value = NULL;
   
   var.key = APPNAME"-vector-resolution-multiplier";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      options.vector_resolution_multiplier = atoi(var.value);
 
   var.value = NULL;
   
   var.key = APPNAME"-vector-antialias";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.antialias = 1; /* integer: 1 to enable antialiasing on vectors */
      else
         options.antialias = 0;
   }
  
   var.value = NULL;
   
   var.key = APPNAME"-vector-translucency";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(strcmp(var.value, "enabled") == 0)
         options.translucency = 1; /* integer: 1 to enable translucency on vectors */
      else 
         options.translucency = 0;          
   }
  
   var.value = NULL;
   
   var.key = APPNAME"-vector-beam-width";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      options.beam = atoi(var.value); /* integer: vector beam width */
   }
 
   var.value = NULL;
   
   var.key = APPNAME"-vector-flicker"; 
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      options.vector_flicker = atof(var.value); /* float: vector beam flicker effect control */
   }   

   var.value = NULL;

   var.key = APPNAME"-vector-intensity";   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      options.vector_intensity = atof(var.value); /* float: vector beam intensity */
   }
    
   {
       struct retro_led_interface ledintf;
       ledintf.set_led_state = NULL;
       
       environ_cb(RETRO_ENVIRONMENT_GET_LED_INTERFACE, &ledintf);
       led_state_cb = ledintf.set_led_state;
   }
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   const int orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
   const bool rotated = ((orientation == ROT90) || (orientation == ROT270));
   
   const int width = rotated ? videoConfig.height : videoConfig.width;
   const int height = rotated ? videoConfig.width : videoConfig.height;
   
   info->geometry.base_width = width;
   info->geometry.base_height = height;
   info->geometry.max_width = width;
   info->geometry.max_height = height;
   info->geometry.aspect_ratio = (rotated && !options.tate_mode) ? (float)videoConfig.aspect_y / (float)videoConfig.aspect_x : (float)videoConfig.aspect_x / (float)videoConfig.aspect_y;
   info->timing.fps = Machine->drv->frames_per_second; /* sets the core timing does any game go above 60fps? */
   info->timing.sample_rate = options.samplerate;  /* please note if you want bally games to work properly set the sample rate to 22050 you cant go below 48 frames with the default that is set you will need to restart the core */
}

static void check_system_specs(void)
{
   /* TODO - set variably */
   /* Midway DCS - Mortal Kombat/NBA Jam etc. require level 9 */
   unsigned level = 10;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
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

   update_variables();
   check_system_specs();
}

void retro_deinit(void)
{
#ifdef LOG_PERFORMANCE
   perf_cb.perf_log();
#endif
}

void retro_reset (void)
{
    machine_reset();
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
   int i,j;
   bool pointer_pressed;
   const struct KeyboardInfo *thisInput;
   bool updated = false;

   poll_cb();

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   /* Keyboard */
   thisInput = retroKeys;
   while(thisInput->name)
   {
      retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
      thisInput ++;
   }
   
   for (i = 0; i < 4; i ++)
   {
      unsigned int offset = (i * 18);

      /* Analog joystick */
      analogjoy[i][0] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
      analogjoy[i][1] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
      analogjoy[i][2] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
      analogjoy[i][3] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
      
      /* Joystick */
      if (options.rstick_to_btns)
      {
         /* if less than 0.5 force, ignore and read buttons as usual */
         retroJsState[0 + offset] = analogjoy[i][3] >  0x4000 ? 1 : input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
         retroJsState[1 + offset] = analogjoy[i][2] < -0x4000 ? 1 : input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
      }
      else
      {
         retroJsState[0 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
         retroJsState[1 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
      }
      retroJsState[2 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
      retroJsState[3 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
      retroJsState[4 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
      retroJsState[5 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
      retroJsState[6 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
      retroJsState[7 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
      if (options.rstick_to_btns)
      {
         retroJsState[8 + offset] = analogjoy[i][2] >  0x4000 ? 1 : input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
         retroJsState[9 + offset] = analogjoy[i][3] < -0x4000 ? 1 : input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
      }
      else
      {
         retroJsState[8 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
         retroJsState[9 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
      }
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
   }

	if (framerate_test == 1)
	{
		struct retro_system_av_info info;
		retro_get_system_av_info(&info);
		printf("timing %d\n", (int) info.timing.sample_rate);
		info.timing.sample_rate=22050;
		
		environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
		framerate_test = 0;
	}

   mame_frame();
   if (samples_per_frame)
   {
      if (usestereo)
         audio_batch_cb(samples_buffer, samples_per_frame);
      else
      {
         for (i = 0, j = 0; i < samples_per_frame; i++)
         {
            conversion_buffer[j++] = samples_buffer[i];
            conversion_buffer[j++] = samples_buffer[i];
         }
         audio_batch_cb(conversion_buffer, samples_per_frame);
      }
   }
   

}


bool retro_load_game(const struct retro_game_info *game)
{
   if (!game)
      return false;

    /* Find game index */
    driverIndex = getDriverIndex(game->path);
    
    if(driverIndex)
    {
        int orientation;
        unsigned rotateMode;
        static const int uiModes[] = {ROT0, ROT90, ROT180, ROT270};
        #define describe_buttons(INDEX) \
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Joystick Left" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Joystick Right" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Joystick Up" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Joystick Down" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "Button 1" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Button 2" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Button 3" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "Button 4" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Button 5" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Button 6" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Button 7" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Button 8" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,     "Button 9" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "Button 10" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Insert Coin" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },

        struct retro_input_descriptor desc[] = {
            describe_buttons(0)
            describe_buttons(1)
            describe_buttons(2)
            describe_buttons(3)
            { 0, 0, 0, 0, NULL }
            };

        environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
        
        options.libretro_content_path = peelPathItem(normalizePath(strdup(game->path)));

        /* Get system directory from frontend */
        environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&options.libretro_system_path);
        if (options.libretro_system_path == NULL || options.libretro_system_path[0] == '\0')
        {
            /* error if not set */
            log_cb(RETRO_LOG_ERROR, "[MAME 2003] libretro system path not set!\n");
            options.libretro_system_path = options.libretro_content_path;
        }
        
        /* Get save directory from frontend */
        environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY,&options.libretro_save_path);
        if (options.libretro_save_path == NULL || options.libretro_save_path[0] == '\0')
        {
            /* error if not set */
            log_cb(RETRO_LOG_ERROR, "[MAME 2003] libretro save path not set!\n");
            options.libretro_save_path = options.libretro_content_path;
        }

        /* Setup Rotation */
        orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
        rotateMode = 0;
        
        rotateMode = (orientation == ROT270) ? 1 : rotateMode;
        rotateMode = (orientation == ROT180) ? 2 : rotateMode;
        rotateMode = (orientation == ROT90) ? 3 : rotateMode;
        
        environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotateMode);

        /* Set all remaining options before starting the game */
        options.ui_orientation = uiModes[rotateMode];
        
        options.use_samples = 1;
        options.cheat = 1;

        /* Boot the emulator */
        return run_game(driverIndex) == 0;
    }
    else
    {
        return false;
    }
}

void retro_unload_game(void)
{
    mame_done();
    
    /*free(fallbackDir);
    systemDir = 0;*/
    /* do we need to be freeing things here? */
}

size_t retro_serialize_size(void)
{
    extern size_t state_get_dump_size(void);
    
    return state_get_dump_size();
}

bool retro_serialize(void *data, size_t size)
{
   int cpunum;
	if(retro_serialize_size() && data && size)
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
	if (retro_serialize_size() && data && size && !state_save_load_begin((void*)data, size))
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
