/***************************************************************************

  Seibu SPI hardware

***************************************************************************/

#ifndef SEIBUSPI_H
#define SEIBUSPI_H

/* machine/seibuspi.c: the graphics decryption */
void seibuspi_text_decrypt(UINT8 *rom);
void seibuspi_bg_decrypt(UINT8 *rom, int size);
void seibuspi_rdft2_text_decrypt(UINT8 *rom);
void seibuspi_rdft2_bg_decrypt(UINT8 *rom, int size);
void seibuspi_rfjet_text_decrypt(UINT8 *rom);
void seibuspi_rfjet_bg_decrypt(UINT8 *rom, int size);
void seibuspi_sprite_decrypt(UINT8 *src, int rom_size);
void seibuspi_rise10_sprite_decrypt(UINT8 *rom, int size);
void seibuspi_rise11_sprite_decrypt_rfjet(UINT8 *rom, int size);

/* vidhrdw/seibuspi.c */
extern data32_t *spi_mainram;	/* 0x40000 bytes, the video DMA source */

VIDEO_START( spi );
VIDEO_UPDATE( spi );

READ32_HANDLER( spi_crtc_r );
WRITE32_HANDLER( spi_crtc_w );
WRITE32_HANDLER( spi_tilemap_dma_start_w );
WRITE32_HANDLER( spi_palette_dma_start_w );
WRITE32_HANDLER( spi_video_dma_length_w );
WRITE32_HANDLER( spi_video_dma_address_w );
void spi_sprite_dma_start(void);
void spi_rf2_layer_bank_w(UINT8 data);

#endif
