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

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <unistd.h> //stat() is defined here
#define S_ISDIR(x) (x & CELL_FS_S_IFDIR)
#endif

static float delta_samples;
int samples_per_frame = 0;
short *samples_buffer;
short *conversion_buffer;
int usestereo = 1;

#if defined(_WIN32)
const char slash = '\\';
#else
const char slash = '/';
#endif

extern retro_log_printf_t log_cb;

int osd_create_directory(const char *dir)
{
	/* test to see if directory exists */
	struct stat statbuf;
	int err = stat(dir, &statbuf);
	if (err == -1)
   {
      if (errno == ENOENT)
      {
         int mkdirok;

         /* does not exist */
         log_cb(RETRO_LOG_WARN, "Directory %s not found - creating...\n", dir);
         /* don't care if already exists) */
#if defined(_WIN32)
         mkdirok = _mkdir(dir);
#elif defined(VITA) || defined(PSP)
         mkdirok = sceIoMkdir(dir, 0777);
#else 
         mkdirok = mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
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
	snprintf(buffer, 1024, "%s%c%s", options.libretro_save_path, slash, APPNAME);
	osd_create_directory(buffer);
	snprintf(buffer, 1024, "%s%c%s", options.libretro_system_path, slash, APPNAME);
	osd_create_directory(buffer);

	return 0;
}


/******************************************************************************

Sound

******************************************************************************/

static float delta_samples;

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

File I/O

******************************************************************************/
static const char* const paths[] = { "raw", "rom", "image", "diff", "samples", "artwork", "nvram", "hi", "hsdb", "cfg", "inp", "memcard", "history", "cheat", "lang", "ctrlr" };

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
      case FILETYPE_ROM:
      case FILETYPE_IMAGE:
         strcpy(currDir, options.libretro_content_path);
         break;
      case FILETYPE_IMAGE_DIFF:
      case FILETYPE_NVRAM:
      case FILETYPE_HIGHSCORE:
      case FILETYPE_CONFIG:
      case FILETYPE_INPUTLOG:
      case FILETYPE_MEMCARD:
      case FILETYPE_SAMPLE:
         /* user generated content goes in mam2003 save directory subfolders */
         snprintf(currDir, 1024, "%s%c%s%c%s", options.libretro_save_path, slash, APPNAME, slash, paths[pathtype]);
         break;
      default:
         /* .dat files and additional core content goes in mame2003 system directory */
         snprintf(currDir, 1024, "%s%c%s", options.libretro_system_path, slash, APPNAME);
   }

   snprintf(buffer, 1024, "%s%c%s", currDir, slash, filename);

#ifdef DEBUG_LOG
   fprintf(stderr, "osd_get_path_info (buffer = [%s]), (directory: [%s]), (path type dir: [%s]), (path type: [%d]), (filename: [%s]) \n", buffer, currDir, paths[pathtype], pathtype, filename);
#endif

   if (stat(buffer, &statbuf) == 0)
      return (S_ISDIR(statbuf.st_mode)) ? PATH_IS_DIRECTORY : PATH_IS_FILE;

   return PATH_NOT_FOUND;
}

FILE* osd_fopen(int pathtype, int pathindex, const char *filename, const char *mode)
{
   char buffer[1024];
   char currDir[1024];
   FILE* out;

   switch (pathtype)
   {
      case FILETYPE_ROM:
      case FILETYPE_IMAGE:
         strcpy(currDir, options.libretro_content_path);
         break;
      case FILETYPE_IMAGE_DIFF:
      case FILETYPE_NVRAM:
      case FILETYPE_HIGHSCORE:
      case FILETYPE_CONFIG:
      case FILETYPE_INPUTLOG:
      case FILETYPE_MEMCARD:
      case FILETYPE_SAMPLE:
         /* user generated content goes in mam2003 save directory subfolders */
         snprintf(currDir, 1024, "%s%c%s%c%s", options.libretro_save_path, slash, APPNAME, slash, paths[pathtype]);
         break;
      default:
         /* .dat files and additional core content goes in mame2003 system directory */
         snprintf(currDir, 1024, "%s%c%s", options.libretro_system_path, slash, APPNAME);
   }

   snprintf(buffer, 1024, "%s%c%s", currDir, slash, filename);

   osd_create_directory(currDir);

   out = fopen(buffer, mode);

   if (out == 0)
   {
      return 0;
   }
   return out;
}

UINT32 osd_fread(FILE* file, void *buffer, UINT32 length)
{
	return fread(buffer, 1, length, file);
}

UINT32 osd_fwrite(FILE* file, const void *buffer, UINT32 length)
{
	return fwrite(buffer, 1, length, file);
}


/******************************************************************************

Miscellaneous

******************************************************************************/

void CLIB_DECL osd_die(const char *text, ...)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, text);

   /* TODO: Don't abort, switch back to main thread and exit cleanly: 
    * This is only used if a malloc fails in src/cpu/z80/z80.c so not too high a priority */
   abort();
}
