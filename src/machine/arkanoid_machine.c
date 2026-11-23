/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/m6805/m6805.h"


int arkanoid_paddle_select;

/* semaphore flags and data latches between the Z80 and the 68705 */
static UINT8 host_flag, mcu_flag;
static UINT8 host_latch, mcu_latch;
static UINT8 latch_driven, reset_input;

/* 68705 ports. latch is the value written by the MCU, out the value seen by the glue logic */
static UINT8 portA_latch, portA_in, portA_out, ddrA;
static UINT8 portC_latch, portC_out, ddrC;

FILE *thelog;

MACHINE_INIT( arkanoid )
{
	host_flag = mcu_flag = 0;
	host_latch = mcu_latch = 0xff;
	latch_driven = reset_input = 0;
	portA_latch = portA_in = portA_out = 0xff;
	portC_latch = portC_out = 0xff;
	ddrA = ddrC = 0;
}

static UINT8 pa_value(void)
{
	return portA_out & (latch_driven ? host_latch : 0xff);
}

static void update_irq(void)
{
	cpu_set_irq_line(1, M68705_IRQ_LINE, host_flag ? ASSERT_LINE : CLEAR_LINE);
}

/* bit 7 of the Z80 register at d008 resets the MCU and the semaphore flip-flops */
void arkanoid_mcu_reset_w(int run)
{	
    reset_input = !run;
	if (run)
	{
		host_flag = mcu_flag = 0;
		update_irq();
	}
	else
	{
		/* the ports are inputs after a reset */
		ddrA = ddrC = 0;
	}
	cpu_set_reset_line(1, run ? CLEAR_LINE : ASSERT_LINE);
}

READ_HANDLER( arkanoid_Z80_mcu_r )
{
	/* return the last value the 68705 wrote, and clear its semaphore */
	mcu_flag = 0;
	return mcu_latch;
}

WRITE_HANDLER( arkanoid_Z80_mcu_w )
{
	if (!reset_input)
		host_flag = 1;
	host_latch = data;
	if (latch_driven)
		portA_in = data;
	update_irq();
	/* boost the interleave for a few usecs to make sure it is read successfully */
	cpu_boost_interleave(0, TIME_IN_USEC(10));
}

READ_HANDLER( arkanoid_68705_portA_r )
{
	return (portA_latch & ddrA) | (portA_in & ~ddrA);
}

WRITE_HANDLER( arkanoid_68705_portA_w )
{
	UINT8 diff = portA_latch ^ data;
	portA_latch = data;
	if (diff & ddrA)
		portA_out = data;
}

WRITE_HANDLER( arkanoid_68705_ddrA_w )
{
	if (data != ddrA)
	{
		ddrA = data;
		portA_out = portA_latch;
	}
}


READ_HANDLER( arkanoid_68705_portC_r )
{
	/* bit 0 is the host semaphore flag (active high), bit 1 is the MCU semaphore flag (active low) */
	UINT8 res = (host_flag ? 0x01 : 0x00) | (mcu_flag ? 0x00 : 0x02) | 0xfc;

    return (portC_latch & ddrC) | (res & ~ddrC);
}

static void portC_update(void)
{
	UINT8 data = portC_latch;
	UINT8 old_pa = pa_value();

	/* PC2 rising edge clears the host semaphore flag, when low the latch drives port A */
	if (data & 0x04)
	{
		latch_driven = 0;
		portA_in = 0xff;
		if (!(portC_out & 0x04))
		{
			host_flag = 0;
			update_irq();
		}
	}
	else
	{
		latch_driven = 1;
		portA_in = host_latch;
	}

	/* PC3 low sets the MCU semaphore flag, the data is latched on the falling edge */
	if (!(data & 0x08))
	{
		if (!reset_input)
			mcu_flag = 1;
		if (portC_out & 0x08)
			mcu_latch = old_pa;
	}

	portC_out = data;
}

WRITE_HANDLER( arkanoid_68705_portC_w )
{
	UINT8 diff = portC_latch ^ data;
	portC_latch = data;
	if (diff & ddrC)
		portC_update();
}

WRITE_HANDLER( arkanoid_68705_ddrC_w )
{
	if (data != ddrC)
	{
		ddrC = data;
		portC_update();
	}
}


READ_HANDLER( arkanoid_68705_input_0_r )
{
	int res = input_port_0_r(offset) & 0x3f;

    /* bit 0x40 is the host semaphore flag, bit 0x80 the MCU semaphore flag (both active low) */
	if (!host_flag) res |= 0x40;
	if (!mcu_flag) res |= 0x80;

	return res;
}

READ_HANDLER( arkanoid_input_2_r )
{
	if (arkanoid_paddle_select)
	{
		return input_port_3_r(offset);
	}
	else
	{
		return input_port_2_r(offset);
	}
}
