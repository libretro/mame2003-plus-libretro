/***************************************************************************

	PSX SPU

	preliminary version by smf.

***************************************************************************/

#if !defined( PSX_SPU_H )

extern int PSX_sh_start( const struct MachineSound *msound );
extern void PSX_sh_stop( void );
extern void PSX_sh_update( void );
extern void PSX_sh_reset( void );
READ32_HANDLER( psx_spu_r );
WRITE32_HANDLER( psx_spu_w );
READ32_HANDLER( psx_spu_delay_r );
WRITE32_HANDLER( psx_spu_delay_w );

struct PSXSPUinterface
{
	int mixing_level;
};

#define PSX_SPU_H ( 1 )
#endif
