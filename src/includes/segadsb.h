/***************************************************************************

    Sega Digital Sound Boards (DSB, DSB2): sndhrdw/segadsb.c

***************************************************************************/

#ifndef SEGADSB_H
#define SEGADSB_H

#define SEGADSB_NONE	0
#define SEGADSB_Z80		1	/* DSB: Z80 */
#define SEGADSB_68K		2	/* DSB2: 68000 */

extern int segadsb_mpeg_region;			/* region of the MPEG data, REGION_SOUND2 by default */
void segadsb_init( int type, int cpu );	/* from DRIVER_INIT */
void segadsb_reset( void );
void segadsb_midi_in( UINT8 data );		/* a byte of the game's sound line */

READ_HANDLER( segadsb_z80_port_r );
WRITE_HANDLER( segadsb_z80_port_w );
READ16_HANDLER( segadsb_68k_r );
WRITE16_HANDLER( segadsb_68k_w );

extern struct CustomSound_interface segadsb_custom_interface;

#endif
