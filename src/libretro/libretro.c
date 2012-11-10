#include <stdint.h>

#include "libretro.h"
#include "performance.h"

#include "mame.h"
#include "driver.h"
#include "state.h"

#ifdef DEBUG_LOG
# define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
# define LOG(msg)
#endif

void mame_frame(void);
void mame_done(void);

//

static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_environment_t environ_cb = NULL;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }


//

#ifndef PATH_SEPARATOR
# if defined(WINDOWS_PATH_STYLE) || defined(_WIN32)
#  define PATH_SEPARATOR '\\'
# else
#  define PATH_SEPARATOR '/'
# endif
#endif

static char* normalizePath(char* aPath)
{
    static const char replaced = (PATH_SEPARATOR == '\\') ? '/' : '\\';

    for(char* tok = strchr(aPath, replaced); tok; tok = strchr(aPath, replaced))
    {
        *tok = PATH_SEPARATOR;
    }
    
    return aPath;
}

static int getDriverIndex(const char* aPath)
{
    char driverName[128];

    // Get all chars after the last slash
    char* path = normalizePath(strdup(aPath ? aPath : "."));
    char* last = strrchr(path, PATH_SEPARATOR);
    memset(driverName, 0, sizeof(driverName));
    strncpy(driverName, last ? last + 1 : path, sizeof(driverName) - 1);
    free(path);
    
    // Remove extension    
    char* const firstDot = strchr(driverName, '.');
    if(firstDot)
    {
        *firstDot = 0;
    }

    // Search list
    for(int i = 0; drivers[i]; i ++)
    {
        if(0 == strcmp(driverName, drivers[i]->name))
        {
            return i;
        }
    }
    
    return -1;
}

static char* peelPathItem(char* aPath)
{
    char* last = strrchr(aPath, PATH_SEPARATOR);
    if(last)
    {
        *last = 0;
    }
    
    return aPath;
}

static int driverIndex; //< Index of mame game loaded

//

#if USE_RETRO_KEYBOARD == 1
extern const struct KeyboardInfo retroKeys[];
extern int retroKeyState[512];
#endif

extern int retroJsState[64];
extern struct osd_create_params videoConfig;

unsigned retroColorMode;
int16_t XsoundBuffer[2048];
uint16_t videoBuffer[1024*1024];
unsigned videoBufferWidth;
unsigned videoBufferHeight;
char* systemDir;
char* romDir;

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "MAME";
   info->library_version = "0.78";
   info->valid_extensions = "zip";
   info->need_fullpath = true;   
   info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    const int orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
    const bool rotated = ((ROT90 == orientation) || (ROT270 == orientation));
    
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

void retro_init (void)
{
    // Get color mode
    retroColorMode = RETRO_PIXEL_FORMAT_XRGB8888;
    if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode))
    {
        retroColorMode = RETRO_PIXEL_FORMAT_RGB565;
        if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode))
        {
            retroColorMode = RETRO_PIXEL_FORMAT_0RGB1555;
        }
    }
}

void retro_deinit(void)
{
#ifdef PERF_TEST
    rarch_perf_log();
#endif
}

void retro_reset (void)
{
    machine_reset();
}

void retro_run (void)
{
    poll_cb();
    
    // Keyboard
#if USE_RETRO_KEYBOARD == 1
    const struct KeyboardInfo* thisInput = retroKeys;
    while(thisInput->name)
    {
        retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
        thisInput ++;
    }
#endif

    // Joystick
    int* jsState = retroJsState;
    for(int i = 0; i != 4; i ++)
    {
        for(int j = 0; j != 16; j ++)
        {
            *jsState++ = input_cb(i, RETRO_DEVICE_JOYPAD, 0, j);
        }
    }
    
    mame_frame();
    
    const void* gotFrame = (videoBufferWidth && videoBufferHeight) ? videoBuffer : 0;
    video_cb(gotFrame, videoBufferWidth, videoBufferHeight, videoBufferWidth * ((RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode) ? 4 : 2));
    audio_batch_cb(XsoundBuffer, Machine->sample_rate / Machine->drv->frames_per_second);
}


bool retro_load_game(const struct retro_game_info *game)
{
    // Find game index
    driverIndex = getDriverIndex(game->path);
    
    if(0 <= driverIndex)
    {
        // Get MAME Directory
        systemDir = normalizePath(strdup(game->path));
        systemDir = peelPathItem(systemDir);
        systemDir = peelPathItem(systemDir);       

        // Get ROM directory
        romDir    = normalizePath(strdup(game->path));
        romDir    = peelPathItem(romDir);

        // Setup Rotation
        const int orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
        unsigned rotateMode = 0;
        static const int uiModes[] = {ROT0, ROT90, ROT180, ROT270};
        
        rotateMode = (ROT270 == orientation) ? 1 : rotateMode;
        rotateMode = (ROT180 == orientation) ? 2 : rotateMode;
        rotateMode = (ROT90 == orientation) ? 3 : rotateMode;
        
        environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotateMode);

        // Set all options before starting the game
        options.samplerate = 48000;            
        options.ui_orientation = uiModes[rotateMode];
        options.vector_intensity = 1.5f;

        // Boot the emulator
        return 0 == run_game(driverIndex);
    }
    else
    {
        return false;
    }
}

void retro_unload_game(void)
{
    mame_done();
    
    free(systemDir);
    systemDir = 0;
}

size_t retro_serialize_size(void)
{
    extern size_t state_get_dump_size(void);
    
    return state_get_dump_size();
}

bool retro_serialize(void *data, size_t size)
{
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
		for(int cpunum = 0; cpunum < cpu_gettotalcpu(); cpunum++)
		{
			cpuintrf_push_context(cpunum);

			/* make sure banking is set */
			activecpu_reset_banking();

			/* save the CPU data */
			state_save_set_current_tag(cpunum + 1);
			if(state_save_save_continue())
			{
			    return false;
			}

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
	/* if successful, load it */
	if (retro_serialize_size() && data && size && !state_save_load_begin((void*)data, size))
	{
        /* read tag 0 */
        state_save_set_current_tag(0);
        if(state_save_load_continue())
        {
            return false;
        }

        /* loop over CPUs */
        for(int cpunum = 0; cpunum < cpu_gettotalcpu(); cpunum++)
        {
            cpuintrf_push_context(cpunum);

            /* make sure banking is set */
            activecpu_reset_banking();

            /* load the CPU data */
            state_save_set_current_tag(cpunum + 1);
            if(state_save_load_continue())
            {
                return false;
            }

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
