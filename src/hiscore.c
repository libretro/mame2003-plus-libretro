/*	hiscore.c
**	generalized high score save/restore support
*/

#include <stdio.h>

#include "driver.h"
#include "hiscore.h"
#include "../precompile/hiscore_dat.h"

#define MAX_CONFIG_LINE_SIZE 48

const char *db_filename = "hiscore.dat"; /* high score definition file */

static struct
{
	int hiscores_have_been_loaded;

	struct mem_range
	{
		UINT32 cpu, addr, num_bytes, start_value, end_value;
		struct mem_range *next;
	} *mem_range;
} state;

/*****************************************************************************/

static void copy_to_memory (int cpu, int addr, const UINT8 *source, int num_bytes)
{
	int i;
	for (i=0; i<num_bytes; i++)
	{
		cpunum_write_byte (cpu, addr+i, source[i]);
	}
}

static void copy_from_memory (int cpu, int addr, UINT8 *dest, int num_bytes)
{
	int i;
	for (i=0; i<num_bytes; i++)
	{
		dest[i] = cpunum_read_byte (cpu, addr+i);
	}
}

/*****************************************************************************/

/*	hexstr2num extracts and returns the value of a hexadecimal field from the
	character buffer pointed to by pString.

	When hexstr2num returns, *pString points to the character following
	the first non-hexadecimal digit, or NULL if an end-of-string marker
	(0x00) is encountered.

*/
static UINT32 hexstr2num (const char **pString)
{
	const char *string = *pString;
	UINT32 result = 0;
	if (string)
	{
		for(;;)
		{
			char c = *string++;
			int digit;

			if (c>='0' && c<='9')
			{
				digit = c-'0';
			}
			else if (c>='a' && c<='f')
			{
				digit = 10+c-'a';
			}
			else if (c>='A' && c<='F')
			{
				digit = 10+c-'A';
			}
			else
			{
				/* not a hexadecimal digit */
				/* safety check for premature EOL */
				if (!c) string = NULL;
				break;
			}
			result = result*16 + digit;
		}
		*pString = string;
	}
	return result;
}

/*	given a line in the hiscore.dat file, determine if it encodes a
	memory range (or a game name).
	For now we assume that CPU number is always a decimal digit, and
	that no game name starts with a decimal digit.
*/
static int is_mem_range (const char *pBuf)
{
	char c;
	for(;;)
	{
		c = *pBuf++;
		if (c == 0) return 0; /* premature EOL */
		if (c == ':') break;
	}
	c = *pBuf; /* character following first ':' */

	return	(c>='0' && c<='9') ||
			(c>='a' && c<='f') ||
			(c>='A' && c<='F');
}

/*	matching_game_name is used to skip over lines until we find <gamename>: */
static int matching_game_name (const char *pBuf, const char *name)
{
	while (*name)
	{
		if (*name++ != *pBuf++) return 0;
	}
	return (*pBuf == ':');
}

/*****************************************************************************/

/* safe_to_load checks the start and end values of each memory range */
static int safe_to_load (void)
{
	struct mem_range *mem_range = state.mem_range;
	while (mem_range)
	{
		if (cpunum_read_byte (mem_range->cpu, mem_range->addr) !=
			mem_range->start_value)
		{
			return 0;
		}
		if (cpunum_read_byte (mem_range->cpu, mem_range->addr + mem_range->num_bytes - 1) !=
			mem_range->end_value)
		{
			return 0;
		}
		mem_range = mem_range->next;
	}
	return 1;
}

/* hs_free disposes of the mem_range linked list */
static void hs_free (void)
{
	struct mem_range *mem_range = state.mem_range;
	while (mem_range)
	{
		struct mem_range *next = mem_range->next;
		free (mem_range);
		mem_range = next;
	}
	state.mem_range = NULL;
}

static void hs_load (void)
{
	mame_file *f = mame_fopen (Machine->gamedrv->name, 0, FILETYPE_HIGHSCORE, 0);
	state.hiscores_have_been_loaded = 1;
    
	if (f)
	{
		struct mem_range *mem_range = state.mem_range;
		log_cb(RETRO_LOG_INFO, "[MAME 2003] loading %s.hi hiscore memory file...\n", Machine->gamedrv->name);
        
		while (mem_range)
		{
			UINT8 *data = malloc (mem_range->num_bytes);
			if (data)
			{
				/*	this buffer will almost certainly be small
					enough to be dynamically allocated, but let's
					avoid memory trashing just in case
				*/
				mame_fread (f, data, mem_range->num_bytes);
				copy_to_memory (mem_range->cpu, mem_range->addr, data, mem_range->num_bytes);
				free (data);
			}
			mem_range = mem_range->next;
		}
		mame_fclose (f);
	}
}

static void hs_save (void)
{
	mame_file *f = mame_fopen (Machine->gamedrv->name, 0, FILETYPE_HIGHSCORE, 1);
	if (f)
	{
		struct mem_range *mem_range = state.mem_range;
		log_cb(RETRO_LOG_INFO, "[MAME 2003] saving %s.hi hiscore memory file...\n", Machine->gamedrv->name);
		while (mem_range)
		{
			UINT8 *data = malloc (mem_range->num_bytes);
			if (data)
			{
				/*	this buffer will almost certainly be small
					enough to be dynamically allocated, but let's
					avoid memory trashing just in case
				*/
				copy_from_memory (mem_range->cpu, mem_range->addr, data, mem_range->num_bytes);
				mame_fwrite(f, data, mem_range->num_bytes);
			}
			mem_range = mem_range->next;
		}
		mame_fclose(f);
	}
}

/*****************************************************************************/
/* public API */

/* call hs_open once after loading a game */
void hs_open (const char *name)
{   
	state.mem_range = NULL;

    char buffer[MAX_CONFIG_LINE_SIZE];
    enum { FIND_NAME, FIND_DATA, FETCH_DATA } mode;
    mode = FIND_NAME;
    
    if(options.use_external_hiscore)
    {
       mame_file *f = NULL;

       log_cb(RETRO_LOG_INFO, "[MAME 2003] Searching for %s hiscore memory map in external .dat.\n", name);
       f = mame_fopen (NULL, db_filename, FILETYPE_HIGHSCORE_DB, 0);

       if(f)
       {
          while (mame_fgets (buffer, MAX_CONFIG_LINE_SIZE, f))
          {
             if (mode==FIND_NAME)
             {
                if (matching_game_name (buffer, name))
                {
                   mode = FIND_DATA;
                   log_cb(RETRO_LOG_INFO, "[MAME 2003] %s hiscore memory map found in external hiscore.dat!\n", name);
                }
             }
             else if (is_mem_range (buffer))
             {
                const char            *pBuf = buffer;
                struct mem_range *mem_range = (struct mem_range*)
                   malloc(sizeof(struct mem_range));

                if (mem_range)
                {
                   mem_range->cpu = hexstr2num (&pBuf);
                   mem_range->addr = hexstr2num (&pBuf);
                   mem_range->num_bytes = hexstr2num (&pBuf);
                   mem_range->start_value = hexstr2num (&pBuf);
                   mem_range->end_value = hexstr2num (&pBuf);

                   mem_range->next = NULL;
                   {
                      struct mem_range *last = state.mem_range;
                      while (last && last->next) last = last->next;
                      if (last == NULL)
                         state.mem_range = mem_range;
                      else
                         last->next = mem_range;
                   }

                   mode = FETCH_DATA;
                }
                else
                {
                   hs_free();
                   break;
                }
             }
             else
             {
                /* line is a game name */
                if (mode == FETCH_DATA)
                   break;
             }
          }
          mame_fclose (f);
       }
    }
    else 
    {    
        int hiscoredat_index = 0;
        log_cb(RETRO_LOG_INFO, "[MAME 2003] Searching for %s hiscore memory map in internal .dat.\n", name);

        while(parse_hiscoredat(buffer, MAX_CONFIG_LINE_SIZE, &hiscoredat_index))
        {
            if (mode==FIND_NAME)
            {
                if (matching_game_name (buffer, name))
                {
                    mode = FIND_DATA;
                    log_cb(RETRO_LOG_INFO, "[MAME 2003] %s hiscore memory map found in internal hiscore.dat!\n", name);
                }
            }
            else if (is_mem_range (buffer))
            {
                const char *pBuf = buffer;
                struct mem_range *mem_range = (struct mem_range*)
                   malloc(sizeof(struct mem_range));
                if (mem_range)
                {
                    mem_range->cpu = hexstr2num (&pBuf);
                    mem_range->addr = hexstr2num (&pBuf);
                    mem_range->num_bytes = hexstr2num (&pBuf);
                    mem_range->start_value = hexstr2num (&pBuf);
                    mem_range->end_value = hexstr2num (&pBuf);

                    mem_range->next = NULL;
                    {
                        struct mem_range *last = state.mem_range;
                        while (last && last->next)
                           last = last->next;
                        if (last == NULL)
                            state.mem_range = mem_range;
                        else
                            last->next = mem_range;
                    }

                    mode = FETCH_DATA;
                }
                else
                {
                    hs_free();
                    break;
                }
            }
            else
            {
                /* line is a game name */
                if (mode == FETCH_DATA)
                   break;
            }
        }   
    }
}

char *parse_hiscoredat(char *s, int n, int *const index)
{
	char *cur = s;

	/* loop while we have characters */
	while (n > 0)
   {
      if (*index == hiscoredat_length)
         break;

      int c = hiscoredat[(*index)++];

      /* if there's a CR, look for an LF afterwards */
      if (c == 0x0d)
      {
         int c2 = hiscoredat[(*index)++];
         if (c2 != 0x0a)
            (*index)--;
         *cur++ = 0x0d;
         n--;
         break;
      }

      /* if there's an LF, reinterp as a CR for consistency */
      else if (c == 0x0a)
      {
         *cur++ = 0x0d;
         n--;
         break;
      }

      /* otherwise, pop the character in and continue */
      *cur++ = c;
      n--;
   }

	/* if we put nothing in, return NULL */
	if (cur == s)
		return NULL;

	/* otherwise, terminate */
	if (n > 0)
		*cur++ = 0;
	return s;
}

/* call hs_init when emulation starts, and when the game is reset */
void hs_init (void)
{
	struct mem_range *mem_range = state.mem_range;
	state.hiscores_have_been_loaded = 0;

	while (mem_range)
	{
		cpunum_write_byte(
			mem_range->cpu,
			mem_range->addr,
			~mem_range->start_value
		);

		cpunum_write_byte(
			mem_range->cpu,
			mem_range->addr + mem_range->num_bytes-1,
			~mem_range->end_value
		);
		mem_range = mem_range->next;
	}
}

/* call hs_update periodically (i.e. once per frame) */
void hs_update (void)
{
	if (state.mem_range)
	{
		if (!state.hiscores_have_been_loaded)
      {
         if (safe_to_load())
            hs_load();
      }
	}
}

/* call hs_close when done playing game */
void hs_close (void)
{
	if (state.hiscores_have_been_loaded)
      hs_save();
	hs_free();
}
