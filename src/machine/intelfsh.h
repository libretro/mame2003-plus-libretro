/***************************************************************************

  Intel / Sharp 16-bit flash memories, the Intel command set

  Backported from current MAME src/devices/machine/intelfsh.cpp (R. Belmont
  and others), the parts the 16-bit Intel chips use: read array, read ID,
  read / clear status, word program, block erase (with its time), master
  lock. The contents are host order 16-bit words, so a driver can point a
  CPU or a sound chip straight at them.

***************************************************************************/

#ifndef INTELFSH_H
#define INTELFSH_H

#include "driver.h"

#define INTELFSH_CHIPS_MAX		( 8 )

enum
{
	INTELFSH_INTEL_E28F400B = 0,	/* 512KB, Intel 0x89 / 0x4471, bottom boot blocks */
	INTELFSH_INTEL_TE28F160			/* 2MB, Sharp 0xb0 / 0xd0, 64KB blocks */
};

/* data: the contents (size bytes, host order words), NULL to allocate them; */
/* they start erased. Chip 0 is the first one a driver sets up */
void intelfsh_init( int chip, int type, UINT16 *data );
UINT16 *intelfsh_data( int chip );
UINT32 intelfsh_size( int chip );	/* in bytes */

UINT16 intelfsh_read( int chip, UINT32 offset );	/* word offset */
void intelfsh_write( int chip, UINT32 offset, UINT16 data );
void intelfsh_reset( int chip );	/* the reset pin: back to read array mode */

/* the contents in the NVRAM file, as little endian words */
void intelfsh_nvram_load( int chip, mame_file *file );
void intelfsh_nvram_save( int chip, mame_file *file );

#endif
