#ifndef __MultiPCM_H__
#define __MultiPCM_H__

#define MAX_MULTIPCM	(2)	/* max # of multipcm chips*/

#ifndef VOL_YM3012
#define YM3012_VOL(LVol,LPan,RVol,RPan) (MIXER(LVol,LPan)|(MIXER(RVol,RPan) << 16))
#endif

struct MultiPCM_interface
{
	int chips;
	int clock[MAX_MULTIPCM];
	int region[MAX_MULTIPCM];
	int mixing_level[MAX_MULTIPCM];
};


int MultiPCM_sh_start(const struct MachineSound *msound);
void MultiPCM_sh_stop(void);
void MultiPCM_sh_reset(void);

WRITE_HANDLER( MultiPCM_reg_0_w );
READ_HANDLER( MultiPCM_reg_0_r);
WRITE_HANDLER( MultiPCM_reg_1_w );
READ_HANDLER( MultiPCM_reg_1_r);

void multipcm_set_bank(int which, UINT32 leftoffs, UINT32 rightoffs);

#endif
