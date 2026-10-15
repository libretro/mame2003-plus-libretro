/***************************************************************************

    chd_v5.c

    Bridge between the legacy MAME CHD code and the bundled libchdr, used
    to read CHD v4/v5 files (see chd_v5.h).

    libchdr's chd.c is compiled here, with its public functions renamed,
    because their names clash with the legacy CHD API.

***************************************************************************/

/* libretro VFS for the (unused) stdio helpers: portable 64-bit seeks */
#ifndef USE_LIBRETRO_VFS
#define USE_LIBRETRO_VFS
#endif

#define chd_open_file				chdr_open_file
#define chd_open_core_file			chdr_open_core_file
#define chd_open					chdr_open
#define chd_precache				chdr_precache
#define chd_close					chdr_close
#define chd_core_file				chdr_core_file
#define chd_error_string			chdr_error_string
#define chd_get_header				chdr_get_header
#define chd_read_header_core_file	chdr_read_header_core_file
#define chd_read_header_file		chdr_read_header_file
#define chd_read_header				chdr_read_header
#define chd_read					chdr_read
#define chd_get_metadata			chdr_get_metadata
#define chd_codec_config			chdr_codec_config
#define chd_get_codec_name			chdr_get_codec_name

#include "../libchdr/src/libchdr_chd.c"

#include "chd_v5.h"


/* core_file wrapper around the caller's reader */
struct chd_v5_file
{
	core_file			core;		/* must be first */
	chd_v5_read_func	readfunc;
	void *				param;
	UINT64				length;
	UINT64				position;
};


static uint64_t chd_v5_fsize(core_file *file)
{
	return ((struct chd_v5_file *)file)->length;
}

static size_t chd_v5_fread(void *buffer, size_t size, size_t count, core_file *file)
{
	struct chd_v5_file *f = (struct chd_v5_file *)file;
	UINT64 bytes = (UINT64)size * count;
	UINT32 got;

	if (size == 0 || f->position >= f->length)
		return 0;
	if (bytes > f->length - f->position)
		bytes = f->length - f->position;

	got = f->readfunc(f->param, f->position, (UINT32)bytes, buffer);
	f->position += got;
	return got / size;
}

static int chd_v5_fclose(core_file *file)
{
	/* the underlying file belongs to the caller */
	free(file);
	return 0;
}

static int chd_v5_fseek(core_file *file, int64_t offset, int whence)
{
	struct chd_v5_file *f = (struct chd_v5_file *)file;
	int64_t newpos;

	switch (whence)
	{
		case SEEK_SET:	newpos = offset; break;
		case SEEK_CUR:	newpos = (int64_t)f->position + offset; break;
		case SEEK_END:	newpos = (int64_t)f->length + offset; break;
		default:		return -1;
	}
	if (newpos < 0)
		return -1;
	f->position = (UINT64)newpos;
	return 0;
}


void *chd_v5_open(chd_v5_read_func readfunc, void *param, UINT64 length, struct chd_v5_info *info)
{
	struct chd_v5_file *file;
	const chd_header *header;
	chd_file *chd = NULL;

	file = malloc(sizeof(*file));
	if (!file)
		return NULL;
	memset(file, 0, sizeof(*file));
	file->core.argp = file;
	file->core.fsize = chd_v5_fsize;
	file->core.fread = chd_v5_fread;
	file->core.fclose = chd_v5_fclose;
	file->core.fseek = chd_v5_fseek;
	file->readfunc = readfunc;
	file->param = param;
	file->length = length;

	/* on failure libchdr closes (frees) the core_file itself */
	if (chdr_open_core_file(&file->core, CHD_OPEN_READ, NULL, &chd) != CHDERR_NONE)
		return NULL;

	header = chdr_get_header(chd);
	info->version = header->version;
	info->hunkbytes = header->hunkbytes;
	info->totalhunks = header->totalhunks;
	info->logicalbytes = header->logicalbytes;
	memcpy(info->sha1, header->sha1, sizeof(info->sha1));
	memcpy(info->rawsha1, header->rawsha1, sizeof(info->rawsha1));
	return chd;
}


void chd_v5_close(void *handle)
{
	if (handle)
		chdr_close((chd_file *)handle);
}


int chd_v5_read_hunk(void *handle, UINT32 hunknum, void *buffer)
{
	return chdr_read((chd_file *)handle, hunknum, buffer) == CHDERR_NONE;
}


UINT32 chd_v5_get_metadata(void *handle, UINT32 *metatag, UINT32 metaindex, void *outputbuf, UINT32 outputlen)
{
	uint32_t resultlen = 0, resulttag = 0;
	uint8_t resultflags = 0;

	if (chdr_get_metadata((chd_file *)handle, *metatag, metaindex, outputbuf, outputlen, &resultlen, &resulttag, &resultflags) != CHDERR_NONE)
		return 0;
	*metatag = resulttag;
	return resultlen;
}
