/***************************************************************************

    chd_v5.h

    Read-only access to CHD v4/v5 files through the bundled libchdr
    (src/lib/libchdr).  The legacy MAME CHD code (chd.c) handles v1-v3
    itself and calls into this bridge only for newer versions, so that a
    current MAME CHD can be used as the parent of the usual v3 differencing
    (.dif) file.

    The interface uses plain types only: libchdr and the legacy CHD code
    share function and type names and cannot be included together.

***************************************************************************/

#ifndef CHD_V5_H
#define CHD_V5_H

#include "osd_cpu.h"

/* random-access reader supplied by the caller */
typedef UINT32 (*chd_v5_read_func)(void *param, UINT64 offset, UINT32 count, void *buffer);

struct chd_v5_info
{
	UINT32	version;
	UINT32	hunkbytes;
	UINT32	totalhunks;
	UINT64	logicalbytes;
	UINT8	sha1[20];		/* overall SHA1 (data + metadata), as listed by current MAME */
	UINT8	rawsha1[20];	/* SHA1 of the raw data only */
};

/* returns an opaque handle, or NULL on error */
void *chd_v5_open(chd_v5_read_func readfunc, void *param, UINT64 length, struct chd_v5_info *info);
void chd_v5_close(void *handle);

/* returns non-zero on success */
int chd_v5_read_hunk(void *handle, UINT32 hunknum, void *buffer);

/* returns the full length of the entry (0 if not found); fills *metatag */
UINT32 chd_v5_get_metadata(void *handle, UINT32 *metatag, UINT32 metaindex, void *outputbuf, UINT32 outputlen);

#endif /* CHD_V5_H */
