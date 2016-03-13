#include <stdint.h>

#include "libretro.h"

#include "mame.h"
#include "driver.h"
#include "state.h"

// Wrapper to build MAME on 3DS. It doesn't have stricmp.
#ifdef _3DS
int stricmp(const char *string1, const char *string2)
{
    return strcasecmp(string1, string2);
}
#endif

void mame_frame(void);
void mame_done(void);

#if defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_XBOX)
unsigned activate_dcs_speedhack = 1;
#else
unsigned activate_dcs_speedhack = 0;
#endif

struct retro_perf_callback perf_cb;

retro_log_printf_t log_cb = NULL;
retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t environ_cb = NULL;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { "mame2003-frameskip", "Frameskip; 0|1|2|3|4|5" },
      { "mame2003-dcs-speedhack",
#if defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_XBOX)
         "MK2/MK3 DCS Speedhack; disabled|enabled"
#else
         "MK2/MK3 DCS Speedhack; enabled|disabled"
#endif
      },
      { "mame2003-skip_disclaimer", "Skip Disclaimer; disabled|enabled" },
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

    // Get all chars after the last slash
    path = normalizePath(strdup(aPath ? aPath : "."));
    last = strrchr(path, PATH_SEPARATOR);
    memset(driverName, 0, sizeof(driverName));
    strncpy(driverName, last ? last + 1 : path, sizeof(driverName) - 1);
    free(path);
    
    // Remove extension    
    firstDot = strchr(driverName, '.');

    if(firstDot)
       *firstDot = 0;

    // Search list
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

static int driverIndex; //< Index of mame game loaded

//

extern const struct KeyboardInfo retroKeys[];
extern int retroKeyState[512];

extern int retroJsState[64];
extern struct osd_create_params videoConfig;

unsigned retroColorMode;
int16_t XsoundBuffer[2048];
char *fallbackDir;
char *systemDir;
char *romDir;
char *saveDir;

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "MAME 2003";
   info->library_version = "0.78";
   info->valid_extensions = "zip";
   info->need_fullpath = true;   
   info->block_extract = true;
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
    info->geometry.aspect_ratio = (float)videoConfig.aspect_x / (float)videoConfig.aspect_y;
    info->timing.fps = Machine->drv->frames_per_second;
    info->timing.sample_rate = 48000.0;
}

extern int frameskip;
unsigned skip_disclaimer = 0;

static void update_variables(void)
{
   struct retro_variable var;
   
   var.value = NULL;
   var.key = "mame2003-frameskip";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value)
      frameskip = atoi(var.value);

   var.value = NULL;
   var.key = "mame2003-dcs-speedhack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value)
   {
       if(strcmp(var.value, "enabled") == 0)
          activate_dcs_speedhack = 1;
       else
          activate_dcs_speedhack = 0;
   }
   else
      activate_dcs_speedhack = 0;
    
   var.value = NULL;
   var.key = "mame2003-skip_disclaimer";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value)
   {
       if(strcmp(var.value, "enabled") == 0)
          skip_disclaimer = 1;
       else
          skip_disclaimer = 0;
   }
   else
      skip_disclaimer = 0;
}

static void check_system_specs(void)
{
   // TODO - set variably
   // Midway DCS - Mortal Kombat/NBA Jam etc. require level 9
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

void retro_run (void)
{
   int i, j;
   int *jsState;
   const struct KeyboardInfo *thisInput;
	bool updated = false;

   poll_cb();

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		update_variables();

   // Keyboard
   thisInput = retroKeys;
   while(thisInput->name)
   {
      retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
      thisInput ++;
   }

   // Joystick
   jsState = retroJsState;
   for (i = 0; i < 4; i ++)
   {
      for (j = 0; j < 16; j ++)
         *jsState++ = input_cb(i, RETRO_DEVICE_JOYPAD, 0, j);
   }

   mame_frame();

   audio_batch_cb(XsoundBuffer, Machine->sample_rate / Machine->drv->frames_per_second);
}


bool retro_load_game(const struct retro_game_info *game)
{
    // Find game index
    driverIndex = getDriverIndex(game->path);
    
    if(driverIndex)
    {
        fallbackDir = strdup(game->path);
        //const char *fallbackDir = game->path;
        int orientation;
        unsigned rotateMode;
        static const int uiModes[] = {ROT0, ROT90, ROT180, ROT270};
        
        /* Get system directory from frontend */
        environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&systemDir);
        if (systemDir == NULL || systemDir[0] == 0)
        {
            /* if non set, use old method */
            systemDir = normalizePath(fallbackDir);
            systemDir = peelPathItem(systemDir);
        }
        
        /* Get save directory from frontend */
        environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY,&saveDir);
        if (saveDir == NULL || saveDir[0] == 0)
        {
            /* if non set, use old method */
            saveDir = normalizePath(fallbackDir);
            saveDir = peelPathItem(saveDir);
        }

        // Get ROM directory
        romDir = normalizePath(fallbackDir);
        romDir = peelPathItem(romDir);

        // Setup Rotation
        orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
        rotateMode = 0;
        
        rotateMode = (orientation == ROT270) ? 1 : rotateMode;
        rotateMode = (orientation == ROT180) ? 2 : rotateMode;
        rotateMode = (orientation == ROT90) ? 3 : rotateMode;
        
        environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotateMode);

        // Set all options before starting the game
        options.samplerate = 48000;            
        options.ui_orientation = uiModes[rotateMode];
        options.vector_intensity = 1.5f;
        options.skip_disclaimer = skip_disclaimer;
        options.use_samples = 1;

        // Boot the emulator
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
    
    free(fallbackDir);
    systemDir = 0;
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


// Stubs
unsigned retro_get_region (void) {return RETRO_REGION_NTSC;}
void *retro_get_memory_data(unsigned type) {return 0;}
size_t retro_get_memory_size(unsigned type) {return 0;}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info){return false;}
void retro_cheat_reset(void){}
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2){}
void retro_set_controller_port_device(unsigned in_port, unsigned device){}
