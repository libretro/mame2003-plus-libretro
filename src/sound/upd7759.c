/************************************************************

 NEC uPD7759 ADPCM Speech Processor
 by: Juergen Buchmueller, Mike Balfour, Howie Cohen and
     Olivier Galibert


 Description:
 The uPD7759 is a speech processing LSI that utilizes ADPCM to produce
 speech or other sampled sounds.  It can directly address up to 1Mbit
 (128k) of external data ROM, or the host CPU can control the speech
 data transfer.  The uPD7759 is always hooked up to a 640 kHz clock and
 has one 8-bit input port, a start pin, a busy pin, and a clock output.

 The chip is composed of 3 parts:
 - a clock divider
 - a rom-reading engine
 - an adpcm engine
 - a 4-to-9 bit adpcm converter

 The clock divider takes the base 640KHz clock and divides it first
 by a fixed divisor of 4 and then by a value between 9 and 32.  The
 result gives a clock between 5KHz and 17.78KHz.  It's probably
 possible, but not recommended and certainly out-of-spec, to push the
 chip harder by reducing the divider.

 The rom-reading engine reads one byte every two divided clock cycles.
 The factor two comes from the fact that a byte has two nibbles, i.e.
 two samples.

 The apdcm engine takes bytes and interprets them as commands:
 00000000           sample end(?)
 00dddddd           silence
 01-fffff           send the 256 following nibbles to the converter
 10-fffff nnnnnnnn  send the n+1 following nibbles to the converter
 11111111           start/stop engine ? (in slave mode)
 11------           unknown

 "fffff" is sent to the clock divider to be the base clock for the
 adpcm converter.  I.e., it's the sampling rate.  If the number of
 nibbles to send is odd the last nibble is ignored.  The commands
 are always 8-bits aligned.

 "dddddd" is the duration of the silence.  The base speed is unknown,
 1ms sounds reasonably.  It does not seem linked to the adpcm clock
 speed because there often is a silence before any 01 or 10 command.


 The adpcm converter converts nibbles into 9-bit DAC values.  It has
 an internal state of 4 bits that's used in conjunction with the
 nibble to lookup which of the 256 possible steps is used.  Then
 the state is changed according to the nibble value.  Essentially, the
 higher the state, the bigger the steps are, and using big steps
 increase the state.  Conversely, using small steps reduces the state.
 This allows the engine to be a little more adaptative than a
 classical ADPCM algorithm.


 The uPD7759 can run in two modes, master (also known as standalone)
 and slave.  The mode is selected through the "md" pin.  No known
 game changes modes on the fly, and it's unsure if that's even
 possible to do.


   Master mode:

 The output of the rom reader is directly connected to the adpcm
 converter.  The controlling cpu only sends a sample number and the
 7759 plays it.

 The sample rom has an header at the beginning of the form
                   nn 5a a5 69 55
 where nn is the number of the last sample.  This is then followed by
 a vector of 2-bytes msb-first values, one per sample.  Multiplying
 them by two gives the sample start offset in the rom.  A 0x00 marks
 the end of each sample.

 It seems that the upd7759 reads at least part of the rom header at
 startup.  Games doing rom banking are careful to reset the chip after
 each change.


   Slave mode:

 The rom reader is completely disconnected.  The input port is
 connected directly to the adpcm engine.  The first write to the input
 port activates the engine (the value itself is ignored).  The engine
 activates the clock output and waits for commands.  The clock speed
 is unknown, but its probably a divider of 640KHz.  We use 40KHz here
 because 80KHz crashes altbeast.  The chip probably has an internal
 fifo to the converter and suspends the clock when the fifo is full.
 The first command is always 0xFF.  A second 0xFF marks the end of the
 sample and the engine stops.  OTOH, there is a 0x00 at the end too.
 Go figure.


 Unknowns:
 - Real rom and slave mode clock speeds
 - Start pin effect in slave mode
 - 0x00 vs. 0xff, sample on/off vs. drq on/off ?

 *************************************************************/
/** : debug-note tags(AT)*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "driver.h"
#include "upd7759.h"
#include "streams.h"

#define MAX_UPD7759 2
#define DRQ_FREQUENCY 40000
#define SILENCE_FREQUENCY 1000

static struct {
	const struct UPD7759_interface *intf;
	struct UPD7759_chip {
		int channel;

		const UINT8 *rom;
		const UINT8 *cur_rombank;

		UINT8 reset, start, port;
		UINT8 playing;
		UINT8 buffer[512];
		int buffer_ptr;
		UINT32 rr_pos;

		int play_length;
		void *timer;

		int param_mode, drq, suspend, waiting, wait_samples;

		int freq;
		UINT32 step, pos;

		int sample, state, nibble, skip_last_nibble, next_skip_last_nibble, started;
	} chip[MAX_UPD7759];
} UPD7759_chips;

const static int UPD7759_step[16][16] = {
	{ 0,  0,  1,  2,  3,   5,   7,  10,  0,   0,  -1,  -2,  -3,   -5,   -7,  -10 },
	{ 0,  1,  2,  3,  4,   6,   8,  13,  0,  -1,  -2,  -3,  -4,   -6,   -8,  -13 },
	{ 0,  1,  2,  4,  5,   7,  10,  15,  0,  -1,  -2,  -4,  -5,   -7,  -10,  -15 },
	{ 0,  1,  3,  4,  6,   9,  13,  19,  0,  -1,  -3,  -4,  -6,   -9,  -13,  -19 },
	{ 0,  2,  3,  5,  8,  11,  15,  23,  0,  -2,  -3,  -5,  -8,  -11,  -15,  -23 },
	{ 0,  2,  4,  7, 10,  14,  19,  29,  0,  -2,  -4,  -7, -10,  -14,  -19,  -29 },
	{ 0,  3,  5,  8, 12,  16,  22,  33,  0,  -3,  -5,  -8, -12,  -16,  -22,  -33 },
	{ 1,  4,  7, 10, 15,  20,  29,  43, -1,  -4,  -7, -10, -15,  -20,  -29,  -43 },
	{ 1,  4,  8, 13, 18,  25,  35,  53, -1,  -4,  -8, -13, -18,  -25,  -35,  -53 },
	{ 1,  6, 10, 16, 22,  31,  43,  64, -1,  -6, -10, -16, -22,  -31,  -43,  -64 },
	{ 2,  7, 12, 19, 27,  37,  51,  76, -2,  -7, -12, -19, -27,  -37,  -51,  -76 },
	{ 2,  9, 16, 24, 34,  46,  64,  96, -2,  -9, -16, -24, -34,  -46,  -64,  -96 },
	{ 3, 11, 19, 29, 41,  57,  79, 117, -3, -11, -19, -29, -41,  -57,  -79, -117 },
	{ 4, 13, 24, 36, 50,  69,  96, 143, -4, -13, -24, -36, -50,  -69,  -96, -143 },
	{ 4, 16, 29, 44, 62,  85, 118, 175, -4, -16, -29, -44, -62,  -85, -118, -175 },
	{ 6, 20, 36, 54, 76, 104, 144, 214, -6, -20, -36, -54, -76, -104, -144, -214 },
};

const static int UPD7759_state[16] = { -1, -1, 0, 0, 1, 2, 2, 3, -1, -1, 0, 0, 1, 2, 2, 3 };

static void UPD7759_standalone_load_data(int chip);

/*  Generate the sound*/
static void UPD7759_update(int chip, INT16 *buffer, int length)
{
	int sample, state, nibble, count;
	UINT32 pos, step;
	const unsigned char *data;
	int i;
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	if(!ch->playing) {
		memset(buffer, 0, 2*length);
		return;
	}

	sample = ch->sample;
	state  = ch->state;
	pos    = ch->pos;
	step   = ch->step;
	nibble = ch->nibble;

	data   = ch->buffer;
	count  = ch->buffer_ptr;

	if(!step)
		step = 0x10000;

	for(i=0; i<length; i++) {
	mode_change:
		if(ch->waiting) {
			ch->wait_samples--;
			if(ch->wait_samples == 0)
				ch->waiting = 0;
		} else {
			pos += step;
			while(pos & ~0xffff) {
				int adpcm;
			get_nibble:
				if(!nibble) {
					if(!count) {
					no_more_data:
						ch->nibble = nibble = 0;
						ch->skip_last_nibble = 0;
						if(ch->wait_samples) {
							ch->waiting = 1;
							goto mode_change;
						}
						if(ch->drq) {
							if(UPD7759_chips.intf->mode == UPD7759_STANDALONE_MODE) {
								ch->suspend = 0;
								ch->buffer_ptr = 0;
								UPD7759_standalone_load_data(chip);
								data   = ch->buffer;
								count  = ch->buffer_ptr;
								goto get_nibble;
							}
							while(i<length) {
								*buffer++ = sample << 7;
								i++;
							}
							ch->skip_last_nibble = 0;
							goto end;
						} else {
							ch->playing = 0;
							memset(buffer, 0, (length-i)*2);
							return;
						}
					}
					adpcm = (*data) >> 4;
				} else {
					adpcm = (*data) & 0xf;
					data++;
					count--;
					if(!count && ch->skip_last_nibble)
						goto no_more_data;
				}
				nibble = !nibble;
				sample += UPD7759_step[state][adpcm];
				state  += UPD7759_state[adpcm];
				if(state < 0)
					state = 0;
				else if(state > 15)
					state = 15;
				pos -= 0x10000;
			}
		}
		*buffer++ = sample << 7;
	}

 end:

	ch->sample      = sample;
	ch->state       = state;
	ch->pos         = pos;
	ch->nibble      = nibble;
	if(data != ch->buffer && count)
		memmove(ch->buffer, data, count);
	ch->buffer_ptr = count;

	if(ch->suspend && (ch->buffer_ptr < sizeof(ch->buffer)) && (ch->waiting || !ch->wait_samples))
		ch->suspend = 0;
}

static void UPD7759_slave_tick(int chip)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];
	if(ch->drq) {
		if(ch->suspend)
			stream_update(ch->channel, 0);
		else if(UPD7759_chips.intf->irqcallback[chip])
			UPD7759_chips.intf->irqcallback[chip](chip);
	}
}


/*  Start emulation of several ADPCM output streams*/
int UPD7759_sh_start(const struct MachineSound *msound)
{
	int chip;
	const struct UPD7759_interface *intf = msound->sound_interface;

	if(!Machine->sample_rate)
		return 0;

	memset(&UPD7759_chips, 0, sizeof(UPD7759_chips));
    /* copy the interface pointer to a global */
	UPD7759_chips.intf = intf;

	for(chip=0; chip<intf->num; chip++) {
		struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];
		char name[20];

		ch->rom          = memory_region(intf->region[chip]);
		ch->cur_rombank  = ch->rom;
		ch->reset        = 0; /** active low, should be zero because we're initializing*/
		ch->start        = 0; /** active high, initial state should be low - not started*/
		ch->port         = 0;
		ch->playing      = 0;
		ch->drq          = 0;
		ch->suspend      = 0;
		ch->waiting      = 0;
		ch->wait_samples = 0;
		ch->buffer_ptr   = 0;
		ch->rr_pos       = 0;
		ch->freq         = 0;
		ch->step         = 0;
		ch->nibble       = 0;
		ch->pos          = 0;
		ch->play_length  = 0;
		ch->param_mode   = 0;
		ch->started      = 0;
		ch->timer        = timer_alloc(UPD7759_slave_tick);

		sprintf(name, "uPD7759 #%d", chip);

		ch->channel = stream_init(name, intf->volume[chip], Machine->sample_rate, chip, UPD7759_update);

		UPD7759_reset_w(chip, 0); /** more comprehensive init*/
	}
	return 0;
}

static void UPD7759_set_frequency(struct UPD7759_chip *ch, UINT8 data)
{
	ch->freq = (640000/4) / ((data & 0x1f)+1);
	ch->step = (((UINT64)(640000/4))<<16)/(Machine->sample_rate*((data & 0x1f)+1));
}

static void UPD7759_start_play(int chip, int length)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];
	ch->play_length = (length+1)/2;
	ch->next_skip_last_nibble = length&1;
}

static void UPD7759_cmd_w(int chip, UINT8 data)
{
	enum { PARAM_8x = 1 };

	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	if(!ch->drq && UPD7759_chips.intf->mode == UPD7759_SLAVE_MODE)
	{
		ch->playing    = 1;
		ch->state      = 0;
		ch->sample     = 0;
		ch->nibble     = 0;
		ch->pos        = 0;
		ch->buffer_ptr = 0;
		ch->started    = 0;
		ch->drq        = 1;
		timer_adjust(ch->timer, TIME_IN_HZ(DRQ_FREQUENCY), chip, TIME_IN_HZ(DRQ_FREQUENCY));
		return;
	}

	if(ch->play_length)
	{
		if(ch->skip_last_nibble)
		{
			ch->buffer[ch->buffer_ptr-1] = (ch->buffer[ch->buffer_ptr-1] & 0xf0) | (data >> 4);
			ch->buffer[ch->buffer_ptr++] = data << 4;
		} else
			ch->buffer[ch->buffer_ptr++] = data;

		ch->play_length--;

		if(!ch->play_length)
		{
			if(ch->skip_last_nibble && ch->next_skip_last_nibble)
				ch->buffer_ptr--;
			ch->skip_last_nibble ^= ch->next_skip_last_nibble;
			ch->next_skip_last_nibble = 0;
		}

		if(ch->buffer_ptr == sizeof(ch->buffer))
			ch->suspend = 1;
		return;
	}

	if(ch->param_mode)
	{
		switch(ch->param_mode)
		{
			case PARAM_8x:
				ch->param_mode = 0;
				UPD7759_start_play(chip, data + 1);
				return;

			default:
				log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759.%d Unknown parameter mode %d ?\n", chip, ch->param_mode);
				ch->param_mode = 0;
		}
	}

	if(!ch->started && (data != 0x00 && data != 0xff))
		ch->started = 1;

	switch(data & 0xc0)
	{
		case 0x00:
			if(data == 0x00)
			{
				if(ch->started) {
					if(UPD7759_chips.intf->mode == UPD7759_SLAVE_MODE)
						timer_adjust(ch->timer, TIME_NEVER, 0, 0);
					ch->drq = 0;
				}
			}
			else
			{
				ch->wait_samples += (Machine->sample_rate * data)/SILENCE_FREQUENCY;
				ch->suspend = 1;
			}
		break;

		case 0x40:
			UPD7759_set_frequency(ch, data & 0x1f);
			UPD7759_start_play(chip, 256);
		break;

		case 0x80:
			UPD7759_set_frequency(ch, data & 0x1f);
			ch->param_mode = PARAM_8x;
		break;

		case 0xc0:
			if(data == 0xff) {
				if(UPD7759_chips.intf->mode == UPD7759_STANDALONE_MODE)
					log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759.%d: cmd_w 0xff in standalone mode\n", chip);
				if(ch->drq && ch->started)
				{
					ch->drq = 0;
					timer_adjust(ch->timer, TIME_NEVER, 0, 0);
				}
			}
			else
				log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759.%d: Unknown command %02x\n", chip, data);
		break;
	}
}

static void UPD7759_standalone_load_data(int chip)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	while(ch->drq && !ch->suspend) {
		UPD7759_cmd_w(chip, ch->cur_rombank[ch->rr_pos]);
		ch->rr_pos = (ch->rr_pos + 1) & 0x1ffff;
	}
}

void UPD7759_reset_w(int chip, UINT8 data)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	if(Machine->sample_rate == 0)
		return;

	if(chip >= UPD7759_chips.intf->num) {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759_reset_w() called with channel = %d, but only %d channels allocated\n", chip, UPD7759_chips.intf->num);
		return;
	}

	ch->reset = data;
	if(!data) {
		stream_update(ch->channel, 0);
		ch->playing      = 0;
		ch->drq          = 0;
		ch->suspend      = 0;
		ch->rr_pos       = 0;
		ch->param_mode   = 0;
		ch->play_length  = 0;
		ch->waiting      = 0;
		ch->wait_samples = 0;
				
		timer_adjust(ch->timer, TIME_NEVER, 0, 0); /** more comprehensive reset*/
	}
}

void UPD7759_start_w(int chip, UINT8 data)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];
	int old_start;

	if(Machine->sample_rate == 0)
		return;

	if(chip >= UPD7759_chips.intf->num) {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759_start_w() called with channel = %d, but only %d channels allocated\n", chip, UPD7759_chips.intf->num);
		return;
	}

	old_start = ch->start;
	ch->start = data;

	/* Start sample on rising edge, but ignore if already playing*/
	if(!old_start && data && !ch->playing && UPD7759_chips.intf->mode == UPD7759_STANDALONE_MODE) {
		UINT8 scount;

		if(memcmp(ch->cur_rombank+1, "\x5A\xA5\x69\x55", 4))
			log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759.%d: Header check failure on sample start\n", chip);

		scount = ch->cur_rombank[0];

		if(ch->port > scount) {
			log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759.%d: Sample number %x is higher than rom sample number (%x)\n", chip, ch->port, scount);
			return;
		}

		ch->rr_pos = ((ch->cur_rombank[5+ch->port*2]<<8)|ch->cur_rombank[6+ch->port*2])*2;

		ch->playing    = 1;
		ch->drq        = 1;
		ch->state      = 0;
		ch->sample     = 0;
		ch->nibble     = 0;
		ch->pos        = 0;
		ch->buffer_ptr = 0;
		ch->started    = 0;
		stream_update(ch->channel, 0);
	}
}

void UPD7759_port_w(int chip, UINT8 data)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	if(Machine->sample_rate == 0)
		return;

	if(chip >= UPD7759_chips.intf->num) {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759_port_w() called with channel = %d, but only %d channels allocated\n", chip, UPD7759_chips.intf->num);
		return;
	}

	ch->port = data;

	if(UPD7759_chips.intf->mode == UPD7759_SLAVE_MODE) {
		timer_adjust(ch->timer, TIME_IN_HZ(DRQ_FREQUENCY), chip, TIME_IN_HZ(DRQ_FREQUENCY));
		UPD7759_cmd_w(chip, data);
	}
}

int UPD7759_busy_r(int chip)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	if(Machine->sample_rate == 0)
		return 0;

	if(chip >= UPD7759_chips.intf->num) {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759_busy_r() called with channel = %d, but only %d channels allocated\n", chip, UPD7759_chips.intf->num);
		return 0;
	}

	return !ch->playing;
}

void UPD7759_set_bank_base(int chip, UINT32 base)
{
	struct UPD7759_chip *ch = &UPD7759_chips.chip[chip];

	if(Machine->sample_rate == 0)
		return;

	if(chip >= UPD7759_chips.intf->num) {
		log_cb(RETRO_LOG_DEBUG, LOGPRE "UPD7759_set_bank_base() called with channel = %d, but only %d channels allocated\n", chip, UPD7759_chips.intf->num);
		return;
	}

	ch->cur_rombank = ch->rom + base;
}

WRITE_HANDLER(UPD7759_0_start_w)
{
	UPD7759_start_w(0,data);
}

WRITE_HANDLER(UPD7759_0_reset_w)
{
	UPD7759_reset_w(0,data);
}

WRITE_HANDLER(UPD7759_0_port_w)
{
	UPD7759_port_w(0,data);
}

READ_HANDLER(UPD7759_0_busy_r)
{
	return UPD7759_busy_r(0);
}
