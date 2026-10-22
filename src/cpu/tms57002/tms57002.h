/***************************************************************************

  TMS57002 "DASP" emulator

  Backported to mame2003-plus from current MAME src/devices/cpu/tms57002
  (Olivier Galibert). Not a scheduled CPU here: it runs one sample period
  at a time from the sound stream of the board that has it, as in MAME
  where each stream sample syncs it.

***************************************************************************/

#ifndef TMS57002_H
#define TMS57002_H

#include "osd_cpu.h"

void tms57002dsp_init( void );
void tms57002dsp_reset( void );
void tms57002dsp_state_register( void );

/* host port */
UINT8 tms57002dsp_data_r( void );
void tms57002dsp_data_w( UINT8 data );
void tms57002dsp_pload_w( int state );
void tms57002dsp_cload_w( int state );
int tms57002dsp_empty_r( void );
int tms57002dsp_fifo_count( void );	/* coefficient updates waiting */
/* called with the "update FIFO empty" line when it changes */
void tms57002dsp_set_empty_callback( void (*callback)( int state ) );

/*
	One sample period: the four inputs (16-bit samples), the four outputs
	of the previous period (16-bit), then the DSP runs its program for the
	next one (cycles: instruction clock / sample rate).
*/
void tms57002dsp_sample( const INT32 *in, INT32 *out, int cycles );

#endif
