#include <stdint.h>

#include "libretro.h"
#include "libco/libco.c" //

#include "mame.h"
#include "driver.h"

#if 1
# define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
# define LOG(msg)
#endif

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
static int getDriverIndex(const char* aPath)
{
    char driverName[128];
    const char* lastSlash = strrchr(aPath, '/');
    const char* const lastSlashWindows = strrchr(aPath, '\\');
    
    if(!lastSlash && lastSlashWindows)
    {
        lastSlash = lastSlashWindows;
    }
    else if(lastSlash && lastSlashWindows && (lastSlashWindows > lastSlash))
    {
        lastSlash = lastSlashWindows;
    }

    memset(driverName, 0, sizeof(driverName));

    if(NULL != lastSlash)
    {
        strncpy(driverName, lastSlash + 1, sizeof(driverName) - 1);
        
        char* const firstDot = strchr(driverName, '.');
        if(firstDot)
        {
            *firstDot = 0;
        }
                
        for(int i = 0; drivers[i]; i ++)
        {
            if(0 == strcmp(driverName, drivers[i]->name))
            {
                return i;
            }
        }
    }
    
    return -1;
}
static int driverIndex; //< Index of mame game loaded

//

int FRONTENDwantsExit;
static bool hasExited;
extern const struct KeyboardInfo retroKeys[];
extern int retroKeyState[512];
extern int retroJsState[64];
extern struct osd_create_params videoConfig;

cothread_t mainThread;
cothread_t emuThread;

unsigned retroColorMode;
int16_t XsoundBuffer[2048];
uint16_t videoBuffer[1024*1024];
unsigned videoBufferWidth;
unsigned videoBufferHeight;
char* systemDir;

// TODO: If run_game returns during load_game, tag it so load_game can return false.

void retro_wrap_emulator(void)
{
    run_game(driverIndex);
    
    // Exit comes from emulator, tell the frontend
    if(!FRONTENDwantsExit)
    {
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0);
    }
        
    hasExited = 1;
        
    // We're done here
    co_switch(mainThread);
        
    // Dead emulator, but libco says not to return
    while(true)
    {
        LOG("Running a dead emulator.");
        co_switch(mainThread);
    }
}

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
    if(emuThread)
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
        info->timing.fps = videoConfig.fps;
        info->timing.sample_rate = 48000.0;
    }
    else
    {
        LOG("retro_get_system_av_info called when there is no emulator thread.");
    }

}

void retro_init (void)
{
    if(!emuThread && !mainThread)
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

        // Get System Directory: This likely won't work if not specified
        const char* retroSysDir = 0;
        const bool gotSysDir = environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &retroSysDir);
        systemDir = strdup(gotSysDir ? retroSysDir : ".");
    
        mainThread = co_active();
        emuThread = co_create(65536*sizeof(void*), retro_wrap_emulator);
    }
    else
    {
        LOG("retro_init called more than once.");
    }
}

void retro_deinit(void)
{
    if(emuThread)
    {
        if(!hasExited)
        {
            LOG("retro_deinit called before MAME has shutdown.");
        }
    
        co_delete(emuThread);
        emuThread = 0;
    
        free(systemDir);
        systemDir = 0;
    }
    else
    {
        LOG("retro_deinit called when there is no emulator thread.");
    }
}

void retro_reset (void)
{
    if(emuThread)
    {
        machine_reset();
    }
    else
    {
        LOG("retro_reset called when there is no emulator thread.");
    }
}

void retro_run (void)
{
    if(emuThread)
    {
        poll_cb();
        
        // Keyboard
        const struct KeyboardInfo* thisInput = retroKeys;
        while(thisInput->name)
        {
            retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
            thisInput ++;
        }

        // Joystick
        int* jsState = retroJsState;
        for(int i = 0; i != 4; i ++)
        {
            for(int j = 0; j != 16; j ++)
            {
                *jsState++ = input_cb(i, RETRO_DEVICE_JOYPAD, 0, j);
            }
        }
    
        co_switch(emuThread);
        
        if(!hasExited && videoBufferWidth && videoBufferHeight)
        {
            video_cb(videoBuffer, videoBufferWidth, videoBufferHeight, videoBufferWidth * ((RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode) ? 4 : 2));
            audio_batch_cb(XsoundBuffer, 800);
        }
    }
    else
    {
        LOG("retro_run called when there is no emulator thread.");
    }    
}


bool retro_load_game(const struct retro_game_info *game)
{
    if(emuThread)
    {
        // Find game index
        driverIndex = getDriverIndex(game->path);
        
        if(0 <= driverIndex)
        {            
            // Setup Rotation
            const int orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
            unsigned rotateMode = 0;
            static const int uiModes[] = {ROT0, ROT90, ROT180, ROT270};
            
            rotateMode = (ROT270 == orientation) ? 1 : rotateMode;
            rotateMode = (ROT180 == orientation) ? 2 : rotateMode;
            rotateMode = (ROT90 == orientation) ? 3 : rotateMode;
            
            environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotateMode);

            // Set all options before starting the game thread, after the first co_switch it's too late.
            options.samplerate = 48000;            
            options.ui_orientation = uiModes[rotateMode];

            // Boot the emulator
            co_switch(emuThread);
            
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        LOG("retro_load_game called when there is no emulator thread.");
        return false;
    }
}

void retro_unload_game(void)
{
    if(emuThread)
    {
        FRONTENDwantsExit = true;
        co_switch(emuThread);
    }
    else
    {
        LOG("retro_unload_game called when there is no emulator thread.");
    }
}


// Stubs
unsigned retro_get_region (void) {return RETRO_REGION_NTSC;}
void *retro_get_memory_data(unsigned type) {return 0;}
size_t retro_get_memory_size(unsigned type) {return 0;}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info){return false;}
size_t retro_serialize_size(void){return 0;}
bool retro_serialize(void *data, size_t size){return false;}
bool retro_unserialize(const void * data, size_t size){return false;}
void retro_cheat_reset(void){}
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2){}
void retro_set_controller_port_device(unsigned in_port, unsigned device){}
