/***************************************************************************

  Dallas DS2404 EconoRAM Time Chip (3-wire interface)

  Backported from current MAME src/devices/machine/ds2404.cpp

***************************************************************************/

#ifndef DS2404_H
#define DS2404_H

#include "driver.h"

/* the reference date the RTC counts the seconds from */
void ds2404_init( int ref_year, int ref_month, int ref_day );

WRITE_HANDLER( ds2404_1w_reset_w );
WRITE_HANDLER( ds2404_3w_reset_w );
READ_HANDLER( ds2404_data_r );
WRITE_HANDLER( ds2404_data_w );
WRITE_HANDLER( ds2404_clk_w );

/* the 512 bytes of SRAM; no file: zeroes */
void ds2404_nvram_load( mame_file *file );
void ds2404_nvram_save( mame_file *file );

#endif
