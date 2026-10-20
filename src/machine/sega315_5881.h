#ifndef SEGA315_5881_H
#define SEGA315_5881_H

/* Sega 315-5881 decryption / decompression, Model 2 (little endian) interface */
void sega315_5881_init( UINT32 game_key, UINT16 (*read)( UINT32 addr ) );
void sega315_5881_reset( void );
UINT16 sega315_5881_ready_r( void );
void sega315_5881_addrlo_w( UINT16 data );
void sega315_5881_addrhi_w( UINT16 data );
void sega315_5881_subkey_w( UINT16 data );
UINT16 sega315_5881_decrypt_r( void );

#endif
