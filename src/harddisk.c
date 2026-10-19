/***************************************************************************

	Hard disk images

	The CHD is read through libchdr, which handles every CHD version
	(v1-v5) and the compression formats current MAME uses (zlib, LZMA,
	Huffman, FLAC). libchdr cannot write, so the sectors the game writes
	go to a differencing file:

		offset	size	field
		0		8		tag "M2PDIFF1"
		8		4		hunk size in bytes
		12		4		total hunks of the disk
		16		4		number of records that follow the header
		20		20		SHA1 of the disk data (MD5 for CHD v1/v2)
		40		24		reserved, zero
		64		...		records: hunk number, then the hunk data

	All values are big-endian. Each hunk the game writes has one record,
	which is rewritten in place on later writes. The record count is
	updated after the record itself, so an interrupted write leaves the
	file consistent.

***************************************************************************/

#include "harddisk.h"
#include "hash.h"
#include "lib/libchdr/include/libchdr/chd.h"



/*************************************
 *
 *	Constants
 *
 *************************************/

#define DIFF_HEADER_SIZE	64
#define DIFF_ID_BYTES		20

static const char diff_tag[8] = { 'M','2','P','D','I','F','F','1' };



/*************************************
 *
 *	Type definitions
 *
 *************************************/

struct hard_disk_file
{
	struct hard_disk_file *	next;				/* next open disk */
	chd_file *				chd;				/* the CHD, read through libchdr */
	mame_file *				diff;				/* differencing file, NULL if read-only */
	UINT32 *				diffmap;			/* per hunk: record number + 1, 0 if not written */
	UINT32					diffcount;			/* number of records in the differencing file */
	struct hard_disk_info	info;				/* hard disk info */
	UINT32					hunkbytes;			/* bytes per hunk */
	UINT32					totalhunks;			/* hunks in the disk */
	UINT32					hunksectors;		/* sectors per hunk */
	UINT32					cachehunk;			/* which hunk is cached */
	UINT8 *					cache;				/* cache of the current hunk */
	UINT8					diskid[DIFF_ID_BYTES];	/* identifies the disk data */
	char					datahash[HASH_BUF_SIZE];	/* checksums of the disk data */
	char					imagehash[HASH_BUF_SIZE];	/* checksum of the whole image */
};

/* libchdr reads the CHD through this wrapper around a mame_file */
struct chd_mame_file
{
	core_file				core;				/* must be first */
	mame_file *				file;
};



/*************************************
 *
 *	Local variables
 *
 *************************************/

static struct hard_disk_file *first_disk;



/*************************************
 *
 *	libchdr file interface
 *
 *************************************/

static uint64_t chd_mame_fsize(core_file *file)
{
	return mame_fsize(((struct chd_mame_file *)file)->file);
}


static size_t chd_mame_fread(void *buffer, size_t size, size_t count, core_file *file)
{
	if (size == 0 || count == 0)
		return 0;
	return mame_fread(((struct chd_mame_file *)file)->file, buffer, (UINT32)(size * count)) / size;
}


static int chd_mame_fclose(core_file *file)
{
	mame_fclose(((struct chd_mame_file *)file)->file);
	free(file);
	return 0;
}


static int chd_mame_fseek(core_file *file, int64_t offset, int whence)
{
	return mame_fseek(((struct chd_mame_file *)file)->file, offset, whence);
}



/*************************************
 *
 *	Big-endian helpers
 *
 *************************************/

static UINT32 get_be32(const UINT8 *base)
{
	return ((UINT32)base[0] << 24) | ((UINT32)base[1] << 16) | ((UINT32)base[2] << 8) | base[3];
}


static void put_be32(UINT8 *base, UINT32 value)
{
	base[0] = value >> 24;
	base[1] = value >> 16;
	base[2] = value >> 8;
	base[3] = value;
}



/*************************************
 *
 *	Differencing file
 *
 *************************************/

static UINT64 diff_record_offset(struct hard_disk_file *file, UINT32 record)
{
	return DIFF_HEADER_SIZE + (UINT64)record * (4 + file->hunkbytes);
}


static int diff_write_count(struct hard_disk_file *file)
{
	UINT8 count[4];

	put_be32(count, file->diffcount);
	mame_fseek(file->diff, 16, SEEK_SET);
	return mame_fwrite(file->diff, count, 4) == 4;
}


static int diff_create(struct hard_disk_file *file)
{
	UINT8 header[DIFF_HEADER_SIZE];

	memset(header, 0, sizeof(header));
	memcpy(&header[0], diff_tag, sizeof(diff_tag));
	put_be32(&header[8], file->hunkbytes);
	put_be32(&header[12], file->totalhunks);
	put_be32(&header[16], 0);
	memcpy(&header[20], file->diskid, DIFF_ID_BYTES);

	file->diffcount = 0;
	memset(file->diffmap, 0, file->totalhunks * sizeof(file->diffmap[0]));
	mame_fseek(file->diff, 0, SEEK_SET);
	return mame_fwrite(file->diff, header, sizeof(header)) == sizeof(header);
}


static int diff_open(struct hard_disk_file *file)
{
	UINT8 header[DIFF_HEADER_SIZE];
	UINT64 size = mame_fsize(file->diff);
	UINT32 records, record;

	/* a new (empty) file, or one that is not a differencing file of this disk: start over */
	mame_fseek(file->diff, 0, SEEK_SET);
	if (size < DIFF_HEADER_SIZE || mame_fread(file->diff, header, sizeof(header)) != sizeof(header) ||
		memcmp(&header[0], diff_tag, sizeof(diff_tag)) != 0 ||
		get_be32(&header[8]) != file->hunkbytes || get_be32(&header[12]) != file->totalhunks ||
		memcmp(&header[20], file->diskid, DIFF_ID_BYTES) != 0)
	{
		if (size >= 8 && memcmp(&header[0], "MComprHD", 8) == 0)
			log_cb(RETRO_LOG_WARN, LOGPRE "Differencing file in the old CHD format, starting a new one\n");
		else if (size != 0)
			log_cb(RETRO_LOG_WARN, LOGPRE "Differencing file does not belong to this disk, starting a new one\n");
		return diff_create(file);
	}

	/* only complete records count */
	records = get_be32(&header[16]);
	if (diff_record_offset(file, records) > size)
		records = (UINT32)((size - DIFF_HEADER_SIZE) / (4 + file->hunkbytes));

	/* map the hunks they hold */
	for (record = 0; record < records; record++)
	{
		UINT8 hunknum[4];
		UINT32 hunk;

		mame_fseek(file->diff, diff_record_offset(file, record), SEEK_SET);
		if (mame_fread(file->diff, hunknum, 4) != 4)
			break;
		hunk = get_be32(hunknum);
		if (hunk < file->totalhunks)
			file->diffmap[hunk] = record + 1;
	}
	file->diffcount = record;
	log_cb(RETRO_LOG_INFO, LOGPRE "Differencing file: %u changed hunks\n", file->diffcount);
	return 1;
}



/*************************************
 *
 *	Hunk access
 *
 *************************************/

static int read_hunk(struct hard_disk_file *file, UINT32 hunknum, UINT8 *buffer)
{
	if (hunknum >= file->totalhunks)
		return 0;

	/* written by the game: from the differencing file */
	if (file->diffmap && file->diffmap[hunknum])
	{
		mame_fseek(file->diff, diff_record_offset(file, file->diffmap[hunknum] - 1) + 4, SEEK_SET);
		return mame_fread(file->diff, buffer, file->hunkbytes) == file->hunkbytes;
	}

	return chd_read(file->chd, hunknum, buffer) == CHDERR_NONE;
}


static int write_hunk(struct hard_disk_file *file, UINT32 hunknum, const UINT8 *buffer)
{
	if (!file->diff || hunknum >= file->totalhunks)
		return 0;

	/* already in the differencing file: rewrite it in place */
	if (file->diffmap[hunknum])
	{
		mame_fseek(file->diff, diff_record_offset(file, file->diffmap[hunknum] - 1) + 4, SEEK_SET);
		return mame_fwrite(file->diff, buffer, file->hunkbytes) == file->hunkbytes;
	}

	/* otherwise append a record, then count it */
	{
		UINT8 hunk[4];

		put_be32(hunk, hunknum);
		mame_fseek(file->diff, diff_record_offset(file, file->diffcount), SEEK_SET);
		if (mame_fwrite(file->diff, hunk, 4) != 4 || mame_fwrite(file->diff, buffer, file->hunkbytes) != file->hunkbytes)
			return 0;
		file->diffcount++;
		file->diffmap[hunknum] = file->diffcount;
		return diff_write_count(file);
	}
}



/*************************************
 *
 *	Open a hard disk
 *
 *************************************/

struct hard_disk_file *hard_disk_open(mame_file *chd, mame_file *diff, const char **error)
{
	struct chd_mame_file *chdfile;
	struct hard_disk_file *file;
	const chd_header *header;
	char metadata[256];
	UINT32 metalength, metatag;
	UINT8 metaflags;
	int cylinders, heads, sectors, sectorbytes;
	chd_error err;

	*error = NULL;
	if (!chd)
	{
		if (diff)
			mame_fclose(diff);
		*error = "file not found";
		return NULL;
	}

	/* allocate memory for the hard disk file */
	file = malloc(sizeof(*file));
	chdfile = malloc(sizeof(*chdfile));
	if (!file || !chdfile)
	{
		free(file);
		free(chdfile);
		mame_fclose(chd);
		if (diff)
			mame_fclose(diff);
		*error = "out of memory";
		return NULL;
	}
	memset(file, 0, sizeof(*file));
	memset(chdfile, 0, sizeof(*chdfile));
	file->diff = diff;
	file->cachehunk = ~0;

	/* open the CHD; from now on libchdr owns the file and closes it */
	chdfile->core.argp = chdfile;
	chdfile->core.fsize = chd_mame_fsize;
	chdfile->core.fread = chd_mame_fread;
	chdfile->core.fclose = chd_mame_fclose;
	chdfile->core.fseek = chd_mame_fseek;
	chdfile->file = chd;
	err = chd_open_core_file(&chdfile->core, CHD_OPEN_READ, NULL, &file->chd);
	if (err != CHDERR_NONE)
	{
		*error = chd_error_string(err);
		file->chd = NULL;
		goto cleanup;
	}
	header = chd_get_header(file->chd);
	file->hunkbytes = header->hunkbytes;
	file->totalhunks = header->totalhunks;

	/* read and parse the hard disk metadata */
	if (chd_get_metadata(file->chd, HARD_DISK_METADATA_TAG, 0, metadata, sizeof(metadata) - 1, &metalength, &metatag, &metaflags) != CHDERR_NONE)
	{
		*error = "not a hard disk image";
		goto cleanup;
	}
	metadata[metalength < sizeof(metadata) ? metalength : sizeof(metadata) - 1] = 0;
	if (sscanf(metadata, HARD_DISK_METADATA_FORMAT, &cylinders, &heads, &sectors, &sectorbytes) != 4 ||
		sectorbytes <= 0 || file->hunkbytes % sectorbytes != 0)
	{
		*error = "invalid hard disk metadata";
		goto cleanup;
	}
	file->info.cylinders = cylinders;
	file->info.heads = heads;
	file->info.sectors = sectors;
	file->info.sectorbytes = sectorbytes;
	file->hunksectors = file->hunkbytes / sectorbytes;

	/* checksums: CHD v1-v3 have the MD5 and (v3) the SHA1 of the data, */
	/* v4/v5 the SHA1 of the data and the SHA1 of the whole image */
	hash_data_clear(file->datahash);
	hash_data_clear(file->imagehash);
	if (header->version <= 3)
	{
		hash_data_insert_binary_checksum(file->datahash, HASH_MD5, (unsigned char *)header->md5);
		if (header->version == 3)
			hash_data_insert_binary_checksum(file->datahash, HASH_SHA1, (unsigned char *)header->sha1);
		memcpy(file->diskid, header->version == 3 ? header->sha1 : header->md5, header->version == 3 ? CHD_SHA1_BYTES : CHD_MD5_BYTES);
	}
	else
	{
		hash_data_insert_binary_checksum(file->datahash, HASH_SHA1, (unsigned char *)header->rawsha1);
		hash_data_insert_binary_checksum(file->imagehash, HASH_SHA1, (unsigned char *)header->sha1);
		memcpy(file->diskid, header->rawsha1, CHD_SHA1_BYTES);
	}

	/* allocate a cache */
	file->cache = malloc(file->hunkbytes);
	if (!file->cache)
	{
		*error = "out of memory";
		goto cleanup;
	}

	/* set up the differencing file */
	if (file->diff)
	{
		file->diffmap = malloc(file->totalhunks * sizeof(file->diffmap[0]));
		if (!file->diffmap)
		{
			*error = "out of memory";
			goto cleanup;
		}
		memset(file->diffmap, 0, file->totalhunks * sizeof(file->diffmap[0]));
		if (!diff_open(file))
		{
			*error = "can't write the differencing file";
			goto cleanup;
		}
	}

	/* add it to the list of open disks */
	file->next = first_disk;
	first_disk = file;
	return file;

cleanup:
	if (file->chd)
		chd_close(file->chd);
	if (file->diff)
		mame_fclose(file->diff);
	free(file->diffmap);
	free(file->cache);
	free(file);
	return NULL;
}



/*************************************
 *
 *	Close a hard disk
 *
 *************************************/

void hard_disk_close(struct hard_disk_file *file)
{
	struct hard_disk_file **curr;

	/* remove it from the list */
	for (curr = &first_disk; *curr; curr = &(*curr)->next)
		if (*curr == file)
		{
			*curr = file->next;
			break;
		}

	chd_close(file->chd);
	if (file->diff)
		mame_fclose(file->diff);
	free(file->diffmap);
	free(file->cache);
	free(file);
}


void hard_disk_close_all(void)
{
	while (first_disk)
		hard_disk_close(first_disk);
}



/*************************************
 *
 *	Return the checksums
 *
 *************************************/

void hard_disk_get_hashes(struct hard_disk_file *file, char *datahash, char *imagehash)
{
	strcpy(datahash, file->datahash);
	strcpy(imagehash, file->imagehash);
}



/*************************************
 *
 *	Return hard disk specific info
 *
 *************************************/

struct hard_disk_info *hard_disk_get_info(struct hard_disk_file *file)
{
	return &file->info;
}



/*************************************
 *
 *	Read from a hard disk
 *
 *************************************/

UINT32 hard_disk_read(struct hard_disk_file *file, UINT32 lbasector, UINT32 numsectors, void *buffer)
{
	UINT32 hunknum = lbasector / file->hunksectors;
	UINT32 sectoroffs = lbasector % file->hunksectors;

	/* for now, just break down multisector reads into single sectors */
	if (numsectors > 1)
	{
		UINT32 total = 0;
		while (numsectors--)
		{
			if (hard_disk_read(file, lbasector++, 1, (UINT8 *)buffer + total * file->info.sectorbytes))
				total++;
			else
				break;
		}
		return total;
	}

	/* if we haven't cached this hunk, read it now */
	if (file->cachehunk != hunknum)
	{
		if (!read_hunk(file, hunknum, file->cache))
		{
			file->cachehunk = ~0;
			return 0;
		}
		file->cachehunk = hunknum;
	}

	/* copy out the requested sector */
	memcpy(buffer, &file->cache[sectoroffs * file->info.sectorbytes], file->info.sectorbytes);
	return 1;
}



/*************************************
 *
 *	Write to a hard disk
 *
 *************************************/

UINT32 hard_disk_write(struct hard_disk_file *file, UINT32 lbasector, UINT32 numsectors, const void *buffer)
{
	UINT32 hunknum = lbasector / file->hunksectors;
	UINT32 sectoroffs = lbasector % file->hunksectors;

	/* for now, just break down multisector writes into single sectors */
	if (numsectors > 1)
	{
		UINT32 total = 0;
		while (numsectors--)
		{
			if (hard_disk_write(file, lbasector++, 1, (const UINT8 *)buffer + total * file->info.sectorbytes))
				total++;
			else
				break;
		}
		return total;
	}

	/* a read-only disk */
	if (!file->diff)
		return 0;

	/* if we haven't cached this hunk, read it now */
	if (file->cachehunk != hunknum)
	{
		if (!read_hunk(file, hunknum, file->cache))
		{
			file->cachehunk = ~0;
			return 0;
		}
		file->cachehunk = hunknum;
	}

	/* copy in the requested data */
	memcpy(&file->cache[sectoroffs * file->info.sectorbytes], buffer, file->info.sectorbytes);

	/* write it back out; on failure the cache no longer matches the disk */
	if (!write_hunk(file, hunknum, file->cache))
	{
		file->cachehunk = ~0;
		return 0;
	}
	return 1;
}
