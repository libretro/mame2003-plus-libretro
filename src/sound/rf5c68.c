/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#include "driver.h"
#include <math.h>


struct rf5c58 {
	UINT8 regs[8][7];
	UINT8 sel;
	UINT8 keyon;
	UINT8 *ram;
	UINT32 addr[8];
	int clock;
	double ratio;
	int stream;
} rpcm;

static void RF5C68_update( int num, INT16 **buffer, int length )
{
	int ch;
	memset(buffer[0], 0, length*2);
	memset(buffer[1], 0, length*2);

	for(ch=0; ch<8; ch++)
		if(!(rpcm.keyon & (1<<ch))) {
			int voll = ( rpcm.regs[ch][1]       & 0xf)*rpcm.regs[ch][0];
			int volr = ((rpcm.regs[ch][1] >> 4) & 0xf)*rpcm.regs[ch][0];
			UINT32 addr = rpcm.addr[ch];
			UINT32 step = ((rpcm.regs[ch][3] << 8) | rpcm.regs[ch][2])*rpcm.ratio;
			int i;

			for(i=0; i<length; i++) {
				INT8 v;
				v = rpcm.ram[addr >> 16];
				if(v == (INT8)0xff) {
					addr = (rpcm.regs[ch][5] << 24) | (rpcm.regs[ch][4] << 16);
					v = rpcm.ram[addr >> 16];
				}
				if(v<0)
					v = 127-(UINT8)v;

				buffer[0][i] += (v*voll) >> 5;
				buffer[1][i] += (v*volr) >> 5;
				addr += step;
			}
			rpcm.addr[ch] = addr;
		}
}

int RF5C68_sh_start( const struct MachineSound *msound )
{
	struct RF5C68interface *intf = msound->sound_interface;
	const char *name[2];
	int vol[2];

	rpcm.ram = malloc(0x10000);

	if(!rpcm.ram)
		return 1;

	rpcm.clock = intf->clock;
	rpcm.ratio = (double)rpcm.clock/(8*Machine->sample_rate);
	memset(rpcm.regs, 0, sizeof(rpcm.regs));
	rpcm.sel = 0;
	rpcm.keyon = 0xff;

	name[0] = "RF5C58 L";
	name[1] = "RF5C68 R";
	vol[0] = (MIXER_PAN_LEFT<<8)  | (intf->volume & 0xff);
	vol[1] = (MIXER_PAN_RIGHT<<8) | (intf->volume & 0xff);
	rpcm.stream = stream_init_multi(2, name, vol, Machine->sample_rate, 0, RF5C68_update );

	return 0;

}

void RF5C68_sh_stop( void )
{
}

WRITE_HANDLER( RF5C68_reg_w )
{
	switch(offset) {
	case 7:
		rpcm.sel = data;
		break;
	case 8: {
		UINT8 map = (~rpcm.keyon)|data;
		if(map != 0xff) {
			int i;
			for(i=0; i<8; i++)
				if(!(map & (1<<i)))
					rpcm.addr[i] = rpcm.regs[i][6] << 24;
		}
		rpcm.keyon = data;
		break;
	}
	default:
		rpcm.regs[rpcm.sel & 7][offset] = data;
		break;
	}
}

static int RF5C68_pcm_bank(void)
{
	if(rpcm.sel & 0x40)
		return rpcm.regs[rpcm.sel & 7][6] << 8;
	else
		return (rpcm.sel & 15) << 12;
}

READ_HANDLER( RF5C68_r )
{
	return rpcm.ram[(RF5C68_pcm_bank() + offset) & 0xffff];
}

WRITE_HANDLER( RF5C68_w )
{
	rpcm.ram[(RF5C68_pcm_bank() + offset) & 0xffff] = data;
}
