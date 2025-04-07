/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#include "driver.h"
#include "rf5c68.h"
#include <math.h>


#define  NUM_CHANNELS    (8)

struct pcm_channel
{
	UINT8		enable;
	UINT8		env;
	UINT8		pan;
	UINT8		start;
	UINT32		addr;
	UINT16		step;
	UINT16		loopst;
};


struct rf5c68pcm
{
	INT16 		stream;
	struct pcm_channel	chan[NUM_CHANNELS];
	UINT8				cbank;
	UINT8				wbank;
	UINT8				enable;
	UINT8				data[0x10000];
};

struct rf5c68pcm *chip;

INT32 Limit( INT32 val, INT32 max,INT32 min)
{
	val &= ~ 0x3f; //10bits output (use 0xffff if 16 bit output is required)
	if ( val > max )      val = max;
	else if ( val < min ) val = min;
	return val ;
}

/************************************************/
/*    RF5C68 stream update                      */
/************************************************/


static void rf5c68_update( int num, INT16 **buffer, int length )
{
	INT16 *left =  buffer[0];
	INT16 *right = buffer[1];
	int i, j;

	/* start with clean buffers */
	memset(left, 0, length * sizeof(*left));
	memset(right, 0, length * sizeof(*right));

	/* bail if not enabled */
	if (!chip->enable)
		return;

	/* loop over channels */
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		struct pcm_channel *chan = &chip->chan[i];

		/* if this channel is active, accumulate samples */
		if (chan->enable)
		{
			int lv = (chan->pan & 0x0f) * chan->env;
			int rv = ((chan->pan >> 4) & 0x0f) * chan->env;

			/* loop over the sample buffer */
			for (j = 0; j < length; j++)
			{
				int sample;

				/* fetch the sample and handle looping */
				sample = chip->data[(chan->addr >> 11) & 0xffff];
				if (sample == 0xff)
				{
					chan->addr = chan->loopst << 11;
					sample = chip->data[(chan->addr >> 11) & 0xffff];

					/* if we loop to a loop point, we're effectively dead */
					if (sample == 0xff)
						break;
				}
				chan->addr += chan->step;

				/* add to the buffer */
				if (sample & 0x80)
				{
					sample &= 0x7f;
					left[j] += (sample * lv) >> 6;
					right[j] += (sample * rv) >> 6;
				}
				else
				{
					left[j] -= (sample * lv) >> 6;
					right[j] -= (sample * rv) >> 6;
				}

			}
		}
	}
	/* now clamp and shift the result (output is only 10 bits) */
	for (j = 0; j < length; j++)
	{
		left[j] =  Limit(left[j] & ~ 0x3f, 32767, -32768);
 		right[j] = Limit(right[j] & ~ 0x3f, 32767, -32768);
 	
	}
}


/************************************************/
/*    RF5C68 stop                               */
/************************************************/
void RF5C68_sh_stop( void )
{
}



int RF5C68_sh_start( const struct MachineSound *msound )
{
	struct RF5C68interface *inintf = msound->sound_interface;
	char buf[2][40];
	const char *name[2];
	int  vol[2];
	int i;
	int  mixed_vol = inintf->volume;

	if (Machine->sample_rate == 0) return 0;

	chip = auto_malloc(sizeof(*chip));

	memset(chip, 0, sizeof(*chip));
    /* f1en fix bad sound if set initialized to 0xff fixed in mame0215*/
	for (i = 0; i < 0x10000; i++)
		chip->data[i]=0xff;

	//intf = inintf;

	name[0] = buf[0];
	name[1] = buf[1];
	sprintf( buf[0], "%s Left", sound_name(msound) );
	sprintf( buf[1], "%s Right", sound_name(msound) );
	vol[0] = mixed_vol&0xffff;
	vol[1] = mixed_vol >>= 16;
	chip->stream = stream_init_multi( 2, name, vol,  inintf->clock / 384 , 0, rf5c68_update );
	if(chip->stream == -1) return 1;

	return 0;
}



/************************************************/
/*    RF5C68 write register                     */
/************************************************/

WRITE_HANDLER( RF5C68_reg_w )
{
	struct pcm_channel *chan = &chip->chan[chip->cbank];
	int i;

	/* force the stream to update first */
	stream_update(chip->stream, 0);

	/* switch off the address */
	switch (offset)
	{
		case 0x00:	/* envelope */
			chan->env = data;
			break;

		case 0x01:	/* pan */
			chan->pan = data;
			break;

		case 0x02:	/* FDL */
			chan->step = (chan->step & 0xff00) | (data & 0x00ff);
			break;

		case 0x03:	/* FDH */
			chan->step = (chan->step & 0x00ff) | ((data << 8) & 0xff00);
			break;

		case 0x04:	/* LSL */
			chan->loopst = (chan->loopst & 0xff00) | (data & 0x00ff);
			break;

		case 0x05:	/* LSH */
			chan->loopst = (chan->loopst & 0x00ff) | ((data << 8) & 0xff00);
			break;

		case 0x06:	/* ST */
			chan->start = data;
			if (!chan->enable)
				chan->addr = chan->start << (8 + 11);
			break;

		case 0x07:	/* control reg */
			chip->enable = (data >> 7) & 1;
			if (data & 0x40)
				chip->cbank = data & 7;
			else
				chip->wbank = data & 15;
			break;

		case 0x08:	/* channel on/off reg */
			for (i = 0; i < 8; i++)
			{
				chip->chan[i].enable = (~data >> i) & 1;
				if (!chip->chan[i].enable)
					chip->chan[i].addr = chip->chan[i].start << (8 + 11);
			}
			break;
	}
}


/************************************************/
/*    RF5C68 read memory                        */
/************************************************/

READ_HANDLER( RF5C68_r )
{
	return chip->data[chip->wbank * 0x1000 + offset];
}


/************************************************/
/*    RF5C68 write memory                       */
/************************************************/

WRITE_HANDLER( RF5C68_w )
{
	chip->data[chip->wbank * 0x1000 + offset] = data;
}




/**************** end of file ****************/
