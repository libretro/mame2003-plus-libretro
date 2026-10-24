/***************************************************************************

  ATA flash PC card behind a Ricoh RF5C296 PC Card controller (Taito G-NET)

  Backported from current MAME src/devices/bus/pccard/ataflash.cpp,
  src/devices/machine/rf5c296.cpp and the ATA mass storage device
  (atahle.cpp, atastorage.cpp; Aaron Giles, smf, Ville Linde and others).

  One card. The disk is a CHD: its CIS ('CIS ' metadata), IDENTIFY data
  ('IDNT') and Taito unlock key ('KEY ') come from the CHD, as in MAME.

***************************************************************************/

#ifndef ATAFLASH_H
#define ATAFLASH_H

#include "driver.h"
#include "harddisk.h"

enum
{
	ATAFLASH_GENERIC = 0,		/* ATA flash PC card */
	ATAFLASH_TAITO_PCCARD1,		/* Taito, unlocked by the key in attribute memory */
	ATAFLASH_TAITO_PCCARD2,		/* Taito, unlocked by two ATA commands */
	ATAFLASH_TAITO_CF			/* Taito Compact Flash, unlocked by an ATA command */
};

/* driver init; disk can be NULL (no card) */
void ataflash_init( int type, struct hard_disk_file *disk );
void ataflash_reset( void );	/* machine init */

/* the RF5C296, on a 16-bit bus: offsets in words, masks with the */
/* bits that are accessed set (as current MAME) */
UINT16 rf5c296_io_r( offs_t offset, UINT16 mem_mask );	/* I/O space: the ATA registers */
void rf5c296_io_w( offs_t offset, UINT16 data, UINT16 mem_mask );
UINT16 rf5c296_mem_r( offs_t offset, UINT16 mem_mask );	/* attribute memory: CIS, card registers */
void rf5c296_mem_w( offs_t offset, UINT16 data, UINT16 mem_mask );

#endif
