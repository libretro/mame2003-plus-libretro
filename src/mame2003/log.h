#ifndef LOG_H
#define LOG_H

#include <libretro.h>

/******************************************************************************

	Shared libretro log interface
    set in mame2003.c

******************************************************************************/

#define LOGPRE          "[MAME 2003+] "

extern retro_log_printf_t log_cb;

/* logerror is a holdover from the MAME log system. MAME 0.78 was evidently
   trying to standardize on logerror but the standardization was not complete,
   with a dozen variations on indicating loging verbosity via defines,
   some code still using printf, etc.

   Due to the fact that the logging in MAME was not internally consistent,
   and that MAME's logging levels don't necessarily correspond to libretro's
   levels of RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, and RETRO_LOG_ERROR,
   there is manual correction to the logging levels needed in some cases.

   TODO: Remove logerror once the conversion to libretro logging is "done.
*/

#ifdef __GNUC__
static INLINE void CLIB_DECL logerror(const char *text,...) __attribute__ ((format (printf, 1, 2)));
#endif

static INLINE void CLIB_DECL logerror(const char *text,...)
{
	#if MAMELOGERROR_ENABLE
	static char log_buffer[2048];
	va_list arg;
	va_start(arg,text);
	vsprintf(log_buffer,text,arg);
	va_end(arg);
	log_cb(RETRO_LOG_DEBUG, "(LOGERROR) %s",log_buffer);
	#endif
}

#ifdef __GNUC__
static INLINE int CLIB_DECL fatalerror(const char *string,...) __attribute__ ((format (printf, 1, 2)));
#endif

/*-------------------------------------------------
	fatalerror - display an error message and
	exit immediately
-------------------------------------------------*/

static INLINE int CLIB_DECL fatalerror(const char *string, ...)
{
	static char log_buffer[2048];
	va_list arg;
	va_start(arg,string);
	vsprintf(log_buffer,string,arg);
	va_end(arg);
	log_cb(RETRO_LOG_DEBUG, "(LOGERROR) %s",log_buffer);
	exit(1);
}
#define osd_die fatalerror
#endif /* LOG_H */
