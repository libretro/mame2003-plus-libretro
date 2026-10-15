/***************************************************************************

    psxjit.c

    Optional recompiler for the PSX CPU (R3000A), built on sljit
    (included by psx_cpu.c).

***************************************************************************/

static INLINE void psx_jit_check_write( const void *host )
{
}

/* the driver or DMA wrote 'bytes' of fast RAM behind the CPU's back */
void psxcpu_ram_written( const void *host, UINT32 bytes )
{
}

int psxcpu_jit_available( void )
{
	return 0;
}

void psxcpu_set_jit( int enable )
{
}
