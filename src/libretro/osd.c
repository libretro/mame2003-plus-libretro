#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "libretro.h"
#include "osdepend.h"

#include "fileio.h"
#include "palette.h"
#include "common.h"
#include "mame.h"
#include "driver.h"

extern int16_t XsoundBuffer[2048];

extern char* systemDir;
extern char* saveDir;
extern char* romDir;
const char* parentDir = "mame2003"; /* groups mame dirs together to avoid conflicts in shared dirs */
#if defined(_WIN32)
char slash = '\\';
#else
char slash = '/';
#endif

extern retro_log_printf_t log_cb;

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <unistd.h> //stat() is defined here
#define S_ISDIR(x) (x & CELL_FS_S_IFDIR)
#endif

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
	int(*mess_printf_output)(const char *fmt, va_list arg);
	int disable_normal_ui;

	int		min_width;		/* minimum width for the display */
	int		min_height;		/* minimum height for the display */
#endif
};
#endif

int osd_create_directory(const char *dir)
{
	/* test to see if directory exists */
	struct stat statbuf;
	int err = stat(dir, &statbuf);
	if (-1 == err) {
		if (ENOENT == errno) {
			/* does not exist */
			log_cb(RETRO_LOG_WARN, "Directory %s not found - creating...\n", dir);
			/* don't care if already exists) */
#if defined(_WIN32)
			int mkdirok = _mkdir(dir);
#else 
			int mkdirok = mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

			if (mkdirok != 0 && errno != EEXIST)
			{
				log_cb(RETRO_LOG_WARN, "Error creating directory %s ERRNO %d (%s)\n", dir, errno, strerror(errno));
				return 0;
			}
		}
	}
	return 1;
}

int osd_init(void)
{
	/* ensure parent dir for various mame dirs is created */
	char buffer[1024];
	snprintf(buffer, 1024, "%s%c%s", saveDir, slash, parentDir);
	osd_create_directory(buffer);
	snprintf(buffer, 1024, "%s%c%s", systemDir, slash, parentDir);
	osd_create_directory(buffer);

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
	return (Machine->sample_rate / Machine->drv->frames_per_second);
}

int osd_update_audio_stream(INT16 *buffer)
{
	int i, samplerate_buffer_size;

	samplerate_buffer_size = (Machine->sample_rate / Machine->drv->frames_per_second);

	if (stereo)
		memcpy(XsoundBuffer, buffer, samplerate_buffer_size * 4);
	else
	{
		for (i = 0; i < samplerate_buffer_size; i++)
		{
			XsoundBuffer[i * 2 + 0] = buffer[i];
			XsoundBuffer[i * 2 + 1] = buffer[i];
		}
	}

	return (Machine->sample_rate / Machine->drv->frames_per_second);
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
	memset(XsoundBuffer, 0, sizeof(XsoundBuffer));
}



/******************************************************************************

File I/O

******************************************************************************/
static const char* const paths[] = { "raw", "rom", "image", "diff", "samples", "artwork", "nvram", "hi", "hsdb", "cfg", "inp", "memcard", "snap", "history", "cheat", "lang", "ctrlr", "ini" };

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
	char currDir[1024];
	struct stat statbuf;

	switch (pathtype)
	{
	case FILETYPE_ROM: /* ROM */
	case FILETYPE_IMAGE:
		/* removes the stupid restriction where we need to have roms in a 'rom' folder */
		strcpy(currDir, romDir);
		break;
	case FILETYPE_IMAGE_DIFF:
	case FILETYPE_NVRAM:
	case FILETYPE_HIGHSCORE:
	case FILETYPE_CONFIG:
	case FILETYPE_INPUTLOG:
	case FILETYPE_MEMCARD:
	case FILETYPE_SCREENSHOT:
		/* user generated content goes in Retroarch save directory */
		snprintf(currDir, 1024, "%s%c%s%c%s", saveDir, slash, parentDir, slash, paths[pathtype]);
		break;
	case FILETYPE_HIGHSCORE_DB:
	case FILETYPE_HISTORY:
	case FILETYPE_CHEAT:
		/* .dat files go directly in the Retroarch system directory */
		snprintf(currDir, 1024, "%s%c%s", systemDir, slash, parentDir);
		break;
	default:
		/* additonal core content goes in Retroarch system directory */
		snprintf(currDir, 1024, "%s%c%s%c%s", systemDir, slash, parentDir, slash, paths[pathtype]);
	}

	snprintf(buffer, 1024, "%s%c%s", currDir, slash, filename);

#ifdef DEBUG_LOG
	fprintf(stderr, "osd_get_path_info (buffer = [%s]), (directory: [%s]), (path type dir: [%s]), (path type: [%d]), (filename: [%s]) \n", buffer, currDir, paths[pathtype], pathtype, filename);
#endif

	if (stat(buffer, &statbuf) == 0)
		return (S_ISDIR(statbuf.st_mode)) ? PATH_IS_DIRECTORY : PATH_IS_FILE;

	return PATH_NOT_FOUND;
}

osd_file *osd_fopen(int pathtype, int pathindex, const char *filename, const char *mode)
{
	char buffer[1024];
	char currDir[1024];
	osd_file *out;

	switch (pathtype)
	{
	case 1:  /* ROM */
	case 2:  /* IMAGE */
			 /* removes the stupid restriction where we need to have roms in a 'rom' folder */
		strcpy(currDir, romDir);
		break;
	case 3:  /* IMAGE DIFF */
	case 6:  /* NVRAM */
	case 7:  /* HIGHSCORE */
	case 9:  /* CONFIG */
	case 10: /* INPUT LOG */
	case 11: /* MEMORY CARD */
	case 12: /* SCREENSHOT */
			 /* user generated content goes in Retroarch save directory */
		snprintf(currDir, 1024, "%s%c%s%c%s", saveDir, slash, parentDir, slash, paths[pathtype]);
		break;
	case 8:  /* HIGHSCORE DB */
	case 13: /* HISTORY */
	case 14: /* CHEAT */
			 /* .dat files go directly in the Retroarch system directory */
		snprintf(currDir, 1024, "%s%c%s", systemDir, slash, parentDir);
		break;
	default:
		/* additonal core content goes in Retroarch system directory */
		snprintf(currDir, 1024, "%s%c%s%c%s", systemDir, slash, parentDir, slash, paths[pathtype]);
	}

	snprintf(buffer, 1024, "%s%c%s", currDir, slash, filename);

	if (log_cb)
		log_cb(RETRO_LOG_INFO, "osd_fopen (buffer = [%s]), (directory: [%s]), (path type dir: [%s]), (path type: [%d]), (filename: [%s]) \n", buffer, currDir, paths[pathtype], pathtype, filename);

	osd_create_directory(currDir);

	out = (osd_file*)malloc(sizeof(osd_file));

	out->file = fopen(buffer, mode);

	if (out->file == 0)
	{
		free(out);
		return 0;
	}
	return out;
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

Miscellaneous

******************************************************************************/

int osd_display_loading_rom_message(const char *name, struct rom_load_data *romdata) { return 0; }
void osd_pause(int paused) {}

void CLIB_DECL osd_die(const char *text, ...)
{
	if (log_cb)
		log_cb(RETRO_LOG_INFO, text);

	// TODO: Don't abort, switch back to main thread and exit cleanly: This is only used if a malloc fails in src/cpu/z80/z80.c so not too high a priority
	abort();
}
