#include <stdint.h>

#include "libretro.h"
#include "libco/libco.c" //

#include "mame.h"
#include "driver.h"

#if 0
# define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
# define LOG(msg)
#endif

//

static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t environ_cb = NULL;

//

static bool FRONTENDwantsExit;
extern const struct KeyboardInfo retroKeys[];
extern int retroKeyState[512];
extern struct osd_create_params videoConfig;

cothread_t mainThread;
cothread_t emuThread;

int driverIndex; //< Index of mame game loaded

int16_t XsoundBuffer[2048];
uint16_t videoBuffer[1024*1024];
char* systemDir;

void retro_wrap_emulator()
{
    run_game(driverIndex);
    
    // Exit comes from emulator, tell the frontend
    if(!FRONTENDwantsExit)
    {
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0);
    }
        
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

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_cb = cb;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
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
        info->timing.sample_rate = 44100.0;
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
        
        const struct KeyboardInfo* thisInput = retroKeys;
        while(thisInput->name)
        {
            retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
            thisInput ++;
        }
    
        co_switch(emuThread);
        
        video_cb(videoBuffer, videoConfig.width, videoConfig.height, videoConfig.width * 2);
        audio_batch_cb(XsoundBuffer, 735);
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
        char* gameName = strdup(strrchr(game->path, '/') + 1);
        *strrchr(gameName, '.') = 0;

        for(driverIndex = 0;; driverIndex ++)
        {
            if(drivers[driverIndex] && (0 == strcmp(gameName, drivers[driverIndex]->name)))
            {
                break;
            }
        }
    
        if(!drivers[driverIndex])
        {
            return false;
        }
    
        // Load emu
        co_switch(emuThread);
        
        // Setup Rotation
        const int orientation = drivers[driverIndex]->flags & ORIENTATION_MASK;
        unsigned rotateMode = 0;
        rotateMode = (ROT270 == orientation) ? 1 : rotateMode;
        rotateMode = (ROT180 == orientation) ? 2 : rotateMode;
        rotateMode = (ROT90 == orientation) ? 3 : rotateMode;
        environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotateMode);
        
        return true;
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
        // TODO
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
