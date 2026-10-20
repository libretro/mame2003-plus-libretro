/***************************************************************************

    MPEG audio layer I and II decoder, from current MAME
    (src/devices/sound/mpeg_audio.cpp; copyright Olivier Galibert,
    BSD-3-Clause), in C89. Used by the Sega Digital Sound Boards.

***************************************************************************/

#ifndef MPEG_AUDIO_H
#define MPEG_AUDIO_H

#include <setjmp.h>

#define MPEG_AUDIO_L1	1
#define MPEG_AUDIO_L2	2

struct mpeg_audio
{
	const unsigned char *base;
	int accepted;
	int sampling_rate, last_frame_number;
	int param_index;
	int channel_count, total_bands, joint_bands;
	int band_param[2][32];
	int scfsi[2][32];
	int scf[2][3][32];
	double amp_values[2][3][32];
	double bdata[2][3][32];
	double subbuffer[2][32];
	double audio_buffer[2][32*32];	/* synthesis history: part of the state */
	int audio_buffer_pos[2];
	double cos_cache[32][32];
	int current_pos, current_limit;
	jmp_buf limit_hit;
};

void mpeg_audio_init( struct mpeg_audio *m, const void *base, int accepted );
void mpeg_audio_clear( struct mpeg_audio *m );

/* decodes one frame (384 or 1152 samples per channel) from bit position *pos
   up to limit (in bits); returns 1 and moves *pos on success, 0 otherwise */
int mpeg_audio_decode_buffer( struct mpeg_audio *m, int *pos, int limit, short *output,
		int *output_samples, int *sample_rate, int *channels );

#endif
