/*************************************************************************

	SMC91C94 ethernet controller
	(C89 port of the current MAME device by Aaron Giles and Ted Green)

	There is no network: transmitted frames complete as if sent on an
	empty cable, and are looped back to the receiver when the game asks
	for it (TCR LOOP, EPH_LOOP or FDUPLX), like current MAME.

**************************************************************************/

#ifndef SMC91C9X_H
#define SMC91C9X_H

void smc91c94_init(void (*irq_callback)(int state));
void smc91c94_reset(void);

/* offset is the 16-bit register (0-7); mem_mask has the accessed bits set */
data16_t smc91c94_r(int offset, data16_t mem_mask);
void smc91c94_w(int offset, data16_t data, data16_t mem_mask);

#endif
