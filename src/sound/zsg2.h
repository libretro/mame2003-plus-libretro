/***************************************************************************

  ZOOM ZSG-2 custom wavetable synthesizer

  Backported to mame2003-plus from current MAME src/devices/sound/zsg2.cpp
  (Olivier Galibert, R. Belmont, hap, superctr). Not a sound interface of
  its own: the board that has it (Taito Zoom) runs it from its stream.

***************************************************************************/

#ifndef ZSG2_H
#define ZSG2_H

#include "osd_cpu.h"

/* the sample ROM, little-endian 32-bit words (any host) */
void zsg2_init(const UINT8 *rom, UINT32 rom_bytes);
void zsg2_reset(void);
/* offset in 16-bit words, 0x000-0x3ff */
UINT16 zsg2_read(offs_t offset);
void zsg2_write(offs_t offset, UINT16 data);
/* 4 outputs: 0 reverb send, 1 chorus send, 2 left direct, 3 right direct */
void zsg2_update(INT32 **outputs, int samples);
void zsg2_state_register(void);

#endif
