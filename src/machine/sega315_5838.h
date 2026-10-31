#ifndef SEGA315_5838_H
#define SEGA315_5838_H

/* Sega 315-5838 / 317-0229 compression and encryption */
#define SEGA315_5838_VARIANT_5838	0	/* DecAthlete */
#define SEGA315_5838_VARIANT_0229	1	/* Dead or Alive (the default) */
#define SEGA315_5838_VARIANT_0230	2
#define SEGA315_5838_VARIANT_0231	3
void sega315_5838_set_variant( int variant );	/* after init */
void sega315_5838_init( UINT16 (*source_r)( UINT32 offset ) );
void sega315_5838_reset( void );
void sega315_5838_source_w( UINT32 data );
void sega315_5838_table_w( int offset, UINT16 data );
UINT16 sega315_5838_data_r( void );

#endif
