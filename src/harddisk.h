/***************************************************************************

	Hard disk images

	The disk is a CHD file (any version, read through libchdr). The
	sectors the game writes go to a differencing file, so the CHD itself
	is never modified.

***************************************************************************/

#ifndef HARDDISK_H
#define HARDDISK_H

#include "driver.h"

struct hard_disk_file;

struct hard_disk_info
{
	UINT32			cylinders;
	UINT32			heads;
	UINT32			sectors;
	UINT32			sectorbytes;
};



/*************************************
 *
 *	Prototypes
 *
 *************************************/

/* chd is the CHD file, diff the differencing file (NULL for a read-only disk); */
/* the disk takes both files over. On failure, NULL is returned, both files are */
/* closed and *error describes the problem */
struct hard_disk_file *hard_disk_open(mame_file *chd, mame_file *diff, const char **error);
void hard_disk_close(struct hard_disk_file *file);
void hard_disk_close_all(void);

/* checksums of the disk data (MD5 for CHD v1-v3, SHA1 of the raw data), and */
/* of the whole image (SHA1 including the metadata, CHD v4/v5 only) */
void hard_disk_get_hashes(struct hard_disk_file *file, char *datahash, char *imagehash);
struct hard_disk_info *hard_disk_get_info(struct hard_disk_file *file);

UINT32 hard_disk_read(struct hard_disk_file *file, UINT32 lbasector, UINT32 numsectors, void *buffer);
UINT32 hard_disk_write(struct hard_disk_file *file, UINT32 lbasector, UINT32 numsectors, const void *buffer);

#endif /* HARDDISK_H */
