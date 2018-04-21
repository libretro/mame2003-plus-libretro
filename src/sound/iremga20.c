/*********************************************************

Irem GA20 PCM Sound Chip

It's not currently known whether this chip is stereo.


Revisions:

04-15-2002 Acho A. Tang
- rewrote channel mixing
- added prelimenary volume and sample rate emulation

05-30-2002 Acho A. Tang
- applied hyperbolic gain control to volume and used
  a musical-note style progression in sample rate
  calculation(still very inaccurate)

*********************************************************/
#include <math.h>
#include "driver.h"
#include "iremga20.h"
#include "state.h"

//AT
#define MAX_VOL 256
#define NUM_STEPS 8
#define NUM_OCTAVES 8

// International standard: 435, U.S.standard: 440
#define A4 435/2

// starts from "A#" in the fourth octave(tentative)
#define BASE_KEY 14*4+1

#define MIX_CH(CH) \
	if (play[CH]) \
	{ \
		eax = pos[CH]; \
		ebx = eax; \
		eax >>= 8; \
		eax = *(char *)(esi + eax); \
		eax *= vol[CH]; \
		ebx += rate[CH]; \
		pos[CH] = ebx; \
		ebx = (ebx < end[CH]); \
		play[CH] = ebx; \
		edx += eax; \
	}
//ZT

static struct IremGA20_chip_def
{
	const struct IremGA20_interface *intf;
	unsigned char *rom;
	int rom_size;
	int channel;
	int mode;
	int regs[0x40];
} IremGA20_chip;

static struct IremGA20_channel_def
{
	unsigned long rate;
	unsigned long size;
	unsigned long start;
	unsigned long pos;
	unsigned long end;
	unsigned long volume;
	unsigned long pan;
	unsigned long effect;
	unsigned long play;
} IremGA20_channel[4];

//AT
static float *sr_table;
static int *sr_xlat;

void IremGA20_update( int param, INT16 **buffer, int length )
{
	unsigned long rate[4], pos[4], end[4], vol[4], play[4];
	unsigned long edi, ebp, esi;
	int eax, ebx, ecx, edx;

	if (!Machine->sample_rate) return;

	/* precache some values */
	for (ecx=0; ecx<4; ecx++)
	{
		rate[ecx] = IremGA20_channel[ecx].rate;
		pos[ecx] = IremGA20_channel[ecx].pos;
		end[ecx] = (IremGA20_channel[ecx].end - 0x20) << 8;
		vol[ecx] = IremGA20_channel[ecx].volume;
		play[ecx] = IremGA20_channel[ecx].play;
	}

	ecx = length << 1;
	esi = (unsigned long)IremGA20_chip.rom;
	edi = (unsigned long)buffer[0];
	ebp = (unsigned long)buffer[1];
	edi += ecx;
	ebp += ecx;
	ecx = -ecx;
	for (; ecx; ecx+=2)
	{
		edx ^= edx;

		MIX_CH(0);
		MIX_CH(1);
		MIX_CH(2);
		MIX_CH(3);

		edx >>= 2;
		*(short *)(edi + ecx) = (short)edx;
		*(short *)(ebp + ecx) = (short)edx;
	}

	/* update the regs now */
	for (ecx=0; ecx< 4; ecx++)
	{
		IremGA20_channel[ecx].pos = pos[ecx];
		IremGA20_channel[ecx].play = play[ecx];
	}
}
//ZT

WRITE_HANDLER( IremGA20_w )
{
	int channel;

	//logerror("GA20:  Offset %02x, data %04x\n",offset,data);

	if (!Machine->sample_rate)
		return;

	stream_update(IremGA20_chip.channel, 0);

	channel = offset >> 4;

	IremGA20_chip.regs[offset] = data;

	switch (offset & 0xf)
	{
	case 0: /* start address low */
		IremGA20_channel[channel].start = ((IremGA20_channel[channel].start)&0xff000) | (data<<4);
	break;

	case 2: /* start address high */
		IremGA20_channel[channel].start = ((IremGA20_channel[channel].start)&0x00ff0) | (data<<12);
	break;

	case 4: /* end address low */
		IremGA20_channel[channel].end = ((IremGA20_channel[channel].end)&0xff000) | (data<<4);
	break;

	case 6: /* end address high */
		IremGA20_channel[channel].end = ((IremGA20_channel[channel].end)&0x00ff0) | (data<<12);
	break;

	case 8: //AT: frequencies are snapped to half-note boundaries
		IremGA20_channel[channel].rate = sr_table[sr_xlat[data>>3]] / Machine->sample_rate;
	break;

	case 0xa: //AT: gain control
		IremGA20_channel[channel].volume = (data * MAX_VOL) / (data + 10);
	break;

	case 0xc: //AT: this is always written 2(enabling both channels?)
		IremGA20_channel[channel].play = data;
		IremGA20_channel[channel].pos = IremGA20_channel[channel].start << 8;
	break;
	}
}

READ_HANDLER( IremGA20_r )
{
	/* Todo - Looks like there is a status bit to show whether each channel is playing */
	return 0xff;
}

static void IremGA20_reset( void )
{
	int i;

	for( i = 0; i < 4; i++ ) {
	IremGA20_channel[i].rate = 0;
	IremGA20_channel[i].size = 0;
	IremGA20_channel[i].start = 0;
	IremGA20_channel[i].pos = 0;
	IremGA20_channel[i].end = 0;
	IremGA20_channel[i].volume = 0;
	IremGA20_channel[i].pan = 0;
	IremGA20_channel[i].effect = 0;
	IremGA20_channel[i].play = 0;
	}
}

int IremGA20_sh_start(const struct MachineSound *msound)
{
	const char *names[2];
	char ch_names[2][40];
//AT
	float fx;
	float *root_table, *fptr;
	int *iptr, i, j, k;
	int key_progress[14]={0,1,2,2,3,4,5,6,7,7,8,9,10,11};
//ZT

	if (!Machine->sample_rate) return 0;

	/* Initialize our chip structure */
	IremGA20_chip.intf = msound->sound_interface;
	IremGA20_chip.mode = 0;
	IremGA20_chip.rom = memory_region(IremGA20_chip.intf->region);
	IremGA20_chip.rom_size = memory_region_length(IremGA20_chip.intf->region);

//AT
	sr_table = auto_malloc(sizeof(float)*NUM_STEPS*NUM_OCTAVES*12);
	root_table = auto_malloc(sizeof(float)*NUM_STEPS*12);
	sr_xlat = auto_malloc(sizeof(int)*256);

	for (i=0; i<NUM_STEPS*12; i++)
		root_table[i] = pow(2.0, (float)i/(12.0*NUM_STEPS));

	fptr = sr_table;
	for (i=0; i<NUM_OCTAVES; i++)
	{
		fx = A4<<(i + 8);
		for (j=0; j<NUM_STEPS*12; j++)
			*fptr++ = fx * root_table[j];
	}

	iptr = sr_xlat;
	for (i=BASE_KEY; i<BASE_KEY+32; i++)
	{
		j = i / 14;
		k = i % 14;
		*iptr++ = (j * 12 + key_progress[k]) * NUM_STEPS;
	}

	// change sinage of PCM samples in advance
	for (i=0; i<IremGA20_chip.rom_size; i++)
		IremGA20_chip.rom[i] -= 0x80;
//ZT

	IremGA20_reset();

	for ( i = 0; i < 0x40; i++ )
	IremGA20_chip.regs[i] = 0;

	for ( i = 0; i < 2; i++ ) {
	names[i] = ch_names[i];
	sprintf(ch_names[i],"%s Ch %d",sound_name(msound),i);
	}

	IremGA20_chip.channel = stream_init_multi( 2, names,
			IremGA20_chip.intf->mixing_level, Machine->sample_rate,
			0, IremGA20_update );

	state_save_register_UINT8("sound", 0, "IremGA20_channel", (UINT8*) IremGA20_channel, sizeof(IremGA20_channel));
	state_save_register_UINT8("sound", 0, "IremGA20_chip",    (UINT8*) &IremGA20_chip,   sizeof(IremGA20_chip));

	return 0;
}

void IremGA20_sh_stop( void )
{
}
