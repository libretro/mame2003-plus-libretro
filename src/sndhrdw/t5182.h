#define CPUTAG_T5182 "T5182"

MACHINE_DRIVER_EXTERN( t5182_audio );

WRITE_HANDLER( t5182_sound_irq_w );
READ_HANDLER(t5182_sharedram_semaphore_snd_r);
WRITE_HANDLER(t5182_sharedram_semaphore_main_acquire_w);
WRITE_HANDLER(t5182_sharedram_semaphore_main_release_w);

extern UINT8 *t5182_sharedram;

extern const struct YM2151interface t5182_ym2151_interface;
