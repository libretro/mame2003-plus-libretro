#ifndef SEGA315_5838_H
#define SEGA315_5838_H

/* Sega 315-5838 / 317-0229 compression and encryption (317-0229 variant) */
void sega315_5838_init( UINT16 (*source_r)( UINT32 offset ) );
void sega315_5838_reset( void );
void sega315_5838_source_w( UINT32 data );
void sega315_5838_table_w( int offset, UINT16 data );
UINT16 sega315_5838_data_r( void );

#endif
