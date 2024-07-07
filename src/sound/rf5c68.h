/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/
#ifndef __RF5C68_H__
#define __RF5C68_H__

struct RF5C68interface
{
	int clock;
	int volume;
};
/******************************************/
WRITE_HANDLER( RF5C68_reg_w );

READ_HANDLER( RF5C68_r );
WRITE_HANDLER( RF5C68_w );

int RF5C68_sh_start( const struct MachineSound *msound );
void RF5C68_sh_stop( void );
#endif
/**************** end of file ****************/
