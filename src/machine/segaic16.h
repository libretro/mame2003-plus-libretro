/***************************************************************************

	Sega 16-bit common hardware

***************************************************************************/

//void (*sound_w_callback)(data8_t), data8_t (*sound_r_callback)(void));
/* multiply chip */
READ16_HANDLER( segaic16_multiply_0_r );
READ16_HANDLER( segaic16_multiply_1_r );
READ16_HANDLER( segaic16_multiply_2_r );
WRITE16_HANDLER( segaic16_multiply_0_w );
WRITE16_HANDLER( segaic16_multiply_1_w );
WRITE16_HANDLER( segaic16_multiply_2_w );

/* divide chip */
READ16_HANDLER( segaic16_divide_0_r );
READ16_HANDLER( segaic16_divide_1_r );
READ16_HANDLER( segaic16_divide_2_r );
WRITE16_HANDLER( segaic16_divide_0_w );
WRITE16_HANDLER( segaic16_divide_1_w );
WRITE16_HANDLER( segaic16_divide_2_w );

/* compare/timer chip */
void segaic16_compare_timer_init(int which, void (*sound_write_callback)(data8_t), void (*timer_ack_callback)(void));
int segaic16_compare_timer_clock(int which);
READ16_HANDLER( segaic16_compare_timer_0_r );
READ16_HANDLER( segaic16_compare_timer_1_r );
WRITE16_HANDLER( segaic16_compare_timer_0_w );
WRITE16_HANDLER( segaic16_compare_timer_1_w );

