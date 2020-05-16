#include "mame.h"
#include <libretro.h>
#include "driver.h"

static float delta_samples;
int samples_per_frame = 0;
int orig_samples_per_frame = 0;
short *samples_buffer;
short *conversion_buffer;
int usestereo = 1;

extern retro_audio_sample_batch_t audio_batch_cb;
extern void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

/******************************************************************************
*
*  Sound
*
*  osd_start_audio_stream() is called at the start of the emulation to initialize
*  the output stream, then osd_update_audio_stream() is called every frame to
*  feed new data. osd_stop_audio_stream() is called when the emulation is stopped.
*
*  The sample rate is fixed at Machine->sample_rate. Samples are 16-bit, signed.
*  When the stream is stereo, left and right samples are alternated in the
*  stream.
*
*  osd_start_audio_stream() and osd_update_audio_stream() must return the number
*  of samples (or couples of samples, when using stereo) required for next frame.
*  This will be around Machine->sample_rate / Machine->drv->frames_per_second,
*  the code may adjust it by SMALL AMOUNTS to keep timing accurate and to
*  maintain audio and video in sync when using vsync. Note that sound emulation,
*  especially when DACs are involved, greatly depends on the number of samples
*  per frame to be roughly constant, so the returned value must always stay close
*  to the reference value of Machine->sample_rate / Machine->drv->frames_per_second.
*  Of course that value is not necessarily an integer so at least a +/- 1
*  adjustment is necessary to avoid drifting over time.
*
******************************************************************************/
int osd_start_audio_stream(int stereo)
{
	if (options.machine_timing) {
		if ((Machine->drv->frames_per_second * 1000 < options.samplerate) || (Machine->drv->frames_per_second < 60))
			Machine->sample_rate = Machine->drv->frames_per_second * 1000;

		else Machine->sample_rate = options.samplerate;
	} else {
		if (Machine->drv->frames_per_second * 1000 < options.samplerate)
			Machine->sample_rate = 22050;

		else
			Machine->sample_rate = options.samplerate;
	}

	delta_samples = 0.0f;
	usestereo = stereo ? 1 : 0;

	/* determine the number of samples per frame */
	samples_per_frame = Machine->sample_rate / Machine->drv->frames_per_second;
	orig_samples_per_frame = samples_per_frame;

	if (Machine->sample_rate == 0) return 0;

	samples_buffer = (short *)calloc(samples_per_frame + 16, 2 + usestereo * 2);
	if (!usestereo) conversion_buffer = (short *)calloc(samples_per_frame + 16, 4);

	return samples_per_frame;
}


int osd_update_audio_stream(INT16 *buffer)
{
	int i, j;

	if (Machine->sample_rate != 0 && buffer) {
		memcpy(samples_buffer, buffer, samples_per_frame * (usestereo ? 4 : 2));
		if (usestereo) {
			audio_batch_cb(samples_buffer, samples_per_frame);
		} else {
			for (i = 0, j = 0; i < samples_per_frame; i++) {
				conversion_buffer[j++] = samples_buffer[i];
				conversion_buffer[j++] = samples_buffer[i];
			}
			audio_batch_cb(conversion_buffer, samples_per_frame);
		}


		//process next frame

		if (samples_per_frame != orig_samples_per_frame) samples_per_frame = orig_samples_per_frame;

		// dont drop any sample frames some games like mk will drift with time

		delta_samples += (Machine->sample_rate / Machine->drv->frames_per_second) - orig_samples_per_frame;
		if (delta_samples >= 1.0f) {
			int integer_delta = (int)delta_samples;
			if (integer_delta <= 16) {
				log_cb(RETRO_LOG_DEBUG, "sound: Delta added value %d added to frame\n", integer_delta);
				samples_per_frame += integer_delta;
			} else if (integer_delta >= 16) {
				log_cb(RETRO_LOG_INFO, "sound: Delta not added to samples_per_frame too large integer_delta:%d\n", integer_delta);
			} else {
				log_cb(RETRO_LOG_DEBUG, "sound(delta) no contitions met\n");
			}
			delta_samples -= integer_delta;
		}
	}
	return samples_per_frame;
}


void osd_stop_audio_stream(void)
{
}
