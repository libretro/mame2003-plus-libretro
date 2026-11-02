/***************************************************************************

  Acclaim RAX sound board (sndhrdw/acclaim_rax.c)

***************************************************************************/

#ifndef ACCLAIM_RAX_H
#define ACCLAIM_RAX_H

#define RAX_CLOCK	16670000

/* the ADSP-2181 tagged "rax": its memory map, and the two DACs as a custom sound */
extern const struct Memory_ReadAddress16 acclaim_rax_readmem[];
extern const struct Memory_WriteAddress16 acclaim_rax_writemem[];
extern struct CustomSound_interface acclaim_rax_custom_interface;

void acclaim_rax_init( int cpunum, int region );	/* driver init: the ADSP CPU and its ROM region */
void acclaim_rax_reset( void );			/* machine init */
void acclaim_rax_data_w( UINT16 data );		/* a command from the host */

#endif
