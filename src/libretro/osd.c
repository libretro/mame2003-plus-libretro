#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>

#include "libretro.h"
#include "osdepend.h"

#include "fileio.h"
#include "palette.h"
#include "common.h"
#include "mame.h"


extern int16_t XsoundBuffer[2048];
extern char* systemDir;

#if 0
struct GameOptions
{
	mame_file *	record;			/* handle to file to record input to */
	mame_file *	playback;		/* handle to file to playback input from */
	mame_file *	language_file;	/* handle to file for localization */

	int		mame_debug;		/* 1 to enable debugging */
	int		cheat;			/* 1 to enable cheating */
	int 	gui_host;		/* 1 to tweak some UI-related things for better GUI integration */
	int 	skip_disclaimer;	/* 1 to skip the disclaimer screen at startup */
	int 	skip_gameinfo;		/* 1 to skip the game info screen at startup */

	int		samplerate;		/* sound sample playback rate, in Hz */
	int		use_samples;	/* 1 to enable external .wav samples */
	int		use_filter;		/* 1 to enable FIR filter on final mixer output */

	float	brightness;		/* brightness of the display */
	float	pause_bright;		/* additional brightness when in pause */
	float	gamma;			/* gamma correction of the display */
	int		color_depth;	/* 15, 16, or 32, any other value means auto */
	int		vector_width;	/* requested width for vector games; 0 means default (640) */
	int		vector_height;	/* requested height for vector games; 0 means default (480) */
	int		ui_orientation;	/* orientation of the UI relative to the video */

	int		beam;			/* vector beam width */
	float	vector_flicker;	/* vector beam flicker effect control */
	float	vector_intensity;/* vector beam intensity */
	int		translucency;	/* 1 to enable translucency on vectors */
	int 	antialias;		/* 1 to enable antialiasing on vectors */

	int		use_artwork;	/* bitfield indicating which artwork pieces to use */
	int		artwork_res;	/* 1 for 1x game scaling, 2 for 2x */
	int		artwork_crop;	/* 1 to crop artwork to the game screen */

	char	savegame;		/* character representing a savegame to load */
	int     crc_only;       /* specify if only CRC should be used as checksum */
	char *	bios;			/* specify system bios (if used), 0 is default */

	int		debug_width;	/* requested width of debugger bitmap */
	int		debug_height;	/* requested height of debugger bitmap */
	int		debug_depth;	/* requested depth of debugger bitmap */

	#ifdef MESS
	UINT32 ram;
	struct ImageFile image_files[MAX_IMAGES];
	int		image_count;
	int		(*mess_printf_output)(const char *fmt, va_list arg);
	int disable_normal_ui;

	int		min_width;		/* minimum width for the display */
	int		min_height;		/* minimum height for the display */
	#endif
};
#endif

int osd_init(void)
{
    return 0;
}

void osd_exit(void)
{

}


/******************************************************************************

	Sound

******************************************************************************/

static bool stereo;

int osd_start_audio_stream(int aStereo)
{
    stereo = (aStereo != 0);
    return 800;
}

int osd_update_audio_stream(INT16 *buffer)
{
    if(stereo)
    {
        memcpy(XsoundBuffer, buffer, 800 * 4);
    }
    else
    {
        for(int i = 0; i != 800; i ++)
        {
            XsoundBuffer[i * 2 + 0] = buffer[i];
            XsoundBuffer[i * 2 + 1] = buffer[i];
        }    
    }

    return 800;
}

void osd_stop_audio_stream(void)
{
}

void osd_set_mastervolume(int attenuation)
{
}

int osd_get_mastervolume(void)
{
    return 0;
}

void osd_sound_enable(int enable)
{
}



/******************************************************************************

	File I/O

******************************************************************************/
static const char* const paths[] = {"raw", "rom", "image", "image_diff", "sample", "artwork", "nvram", "hs", "hsdb", "config", "inputlog", "state", "memcard", "ss", "history", "cheat", "lang", "ctrlr", "ini"};

struct _osd_file
{
    FILE* file;
};

int osd_get_path_count(int pathtype)
{
    return 1;
}

int osd_get_path_info(int pathtype, int pathindex, const char *filename)
{
    char buffer[1024];
    struct stat statbuf;
 
    snprintf(buffer, 1024, "%s/%s/%s", systemDir, paths[pathtype], filename);
    if(0 == stat(buffer, &statbuf))
    {
        return (S_ISDIR(statbuf.st_mode)) ? PATH_IS_DIRECTORY : PATH_IS_FILE;
    }
    
    return PATH_NOT_FOUND;
}

osd_file *osd_fopen(int pathtype, int pathindex, const char *filename, const char *mode)
{
    char buffer[1024];
    snprintf(buffer, 1024, "%s/%s/%s", systemDir, paths[pathtype], filename);

    osd_file* out = malloc(sizeof(osd_file));
    out->file = fopen(buffer, mode);
    
    if(0 == out->file)
    {
        free(out);
        return 0;
    }
    else
    {
        return out;
    }
}

int osd_fseek(osd_file *file, INT64 offset, int whence)
{
    return fseek(file->file, offset, whence);
}

UINT64 osd_ftell(osd_file *file)
{
    return ftell(file->file);
}

int osd_feof(osd_file *file)
{
    return feof(file->file);
}

UINT32 osd_fread(osd_file *file, void *buffer, UINT32 length)
{
    return fread(buffer, 1, length, file->file);
}

UINT32 osd_fwrite(osd_file *file, const void *buffer, UINT32 length)
{
    return fwrite(buffer, 1, length, file->file);
}

void osd_fclose(osd_file *file)
{
    fclose(file->file);
    free(file);
}



/******************************************************************************

	Timing

******************************************************************************/
cycles_t osd_cycles(void)
{
    struct timeval t;
    gettimeofday(&t, 0);
    
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

cycles_t osd_cycles_per_second(void)
{
    return 1000000;
}

cycles_t osd_profiling_ticks(void)
{
    return osd_cycles();
}



/******************************************************************************

	Miscellaneous

******************************************************************************/

int osd_display_loading_rom_message(const char *name,struct rom_load_data *romdata){return 0;}
void osd_pause(int paused){}

void CLIB_DECL osd_die(const char *text,...)
{
#ifdef DEBUG_LOG
    va_list args;
    va_start (args, text);
    vfprintf (stderr, text, args);
    va_end (args);
#endif

    abort();
}

void CLIB_DECL logerror(const char *text,...)
{
#ifdef DEBUG_LOG
    va_list args;
    va_start (args, text);
    vfprintf (stderr, text, args);
    va_end (args);
#endif
}

