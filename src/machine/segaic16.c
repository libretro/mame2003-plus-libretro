/***************************************************************************

	Sega 16-bit common hardware

***************************************************************************/

#include "driver.h"
#include "segaic16.h"
#include "vidhrdw/res_net.h"

extern void fd1094_machine_init(void);


/*************************************
 *
 *	Debugging
 *
 *************************************/

#define LOG_MEMORY_MAP	(0)
#define LOG_MULTIPLY	(0)
#define LOG_DIVIDE		(0)
#define LOG_COMPARE		(0)

struct multiply_chip
{
	UINT16	regs[4];
};

struct divide_chip
{
	UINT16 	regs[8];
};

struct compare_timer_chip
{
	UINT16	regs[16];
	UINT16	counter;
	UINT8	bit;
	void	(*sound_w)(data8_t);
	void	(*timer_ack)(void);
};



/*************************************
 *
 *	Statics
 *
 *************************************/

static struct multiply_chip multiply[3];
static struct divide_chip divide[3];
static struct compare_timer_chip compare_timer[2];

/*************************************
 *
 *	Multiply chip
 *
 *************************************/

static data16_t multiply_r(int which, offs_t offset, data16_t mem_mask)
{
	offset &= 3;
	switch (offset)
	{
		case 0:	return multiply[which].regs[0];
		case 1:	return multiply[which].regs[1];
		case 2:	return ((INT16)multiply[which].regs[0] * (INT16)multiply[which].regs[1]) >> 16;
		case 3:	return ((INT16)multiply[which].regs[0] * (INT16)multiply[which].regs[1]) & 0xffff;
	}
	return 0xffff;
}


static void multiply_w(int which, offs_t offset, data16_t data, data16_t mem_mask)
{
	offset &= 3;
	switch (offset)
	{
		case 0:	COMBINE_DATA(&multiply[which].regs[0]);	break;
		case 1:	COMBINE_DATA(&multiply[which].regs[1]);	break;
		case 2:	COMBINE_DATA(&multiply[which].regs[0]);	break;
		case 3:	COMBINE_DATA(&multiply[which].regs[1]);	break;
	}
}


READ16_HANDLER( segaic16_multiply_0_r )  { return multiply_r(0, offset, mem_mask); }
READ16_HANDLER( segaic16_multiply_1_r )  { return multiply_r(1, offset, mem_mask); }
READ16_HANDLER( segaic16_multiply_2_r )  { return multiply_r(2, offset, mem_mask); }
WRITE16_HANDLER( segaic16_multiply_0_w ) { multiply_w(0, offset, data, mem_mask); }
WRITE16_HANDLER( segaic16_multiply_1_w ) { multiply_w(1, offset, data, mem_mask); }
WRITE16_HANDLER( segaic16_multiply_2_w ) { multiply_w(2, offset, data, mem_mask); }



/*************************************
 *
 *	Divide chip
 *
 *************************************/

static void update_divide(int which, int mode)
{
	/* clear the flags by default */
	divide[which].regs[6] = 0;
	
	/* if mode 0, store quotient/remainder */
	if (mode == 0)
	{
		INT32 dividend = (INT32)((divide[which].regs[0] << 16) | divide[which].regs[1]);
		INT32 divisor = (INT16)divide[which].regs[2];
		INT32 quotient, remainder;

		/* perform signed divide */
	if (divisor == 0)
		{
			quotient = dividend;//((INT32)(dividend ^ divisor) < 0) ? 0x8000 : 0x7fff;
		divide[which].regs[6] |= 0x4000;
		}
	else
		quotient = dividend / divisor;
		remainder = dividend - quotient * divisor;
		
			/* clamp to 16-bit signed */
			if (quotient < -32768)
			{
				quotient = -32768;
				divide[which].regs[6] |= 0x8000;
			}
			else if (quotient > 32767)
			{
				quotient = 32767;
				divide[which].regs[6] |= 0x8000;
			}
		
		/* store quotient and remainder */
			divide[which].regs[4] = quotient;
			divide[which].regs[5] = remainder;
		}
		
		/* if mode 1, store 32-bit quotient */
		else
		{
		UINT32 dividend = (UINT32)((divide[which].regs[0] << 16) | divide[which].regs[1]);
		UINT32 divisor = (UINT16)divide[which].regs[2];
		UINT32 quotient;

		/* perform unsigned divide */
		if (divisor == 0)
		{
			quotient = dividend;//0x7fffffff;
			divide[which].regs[6] |= 0x4000;
		}
		else
			quotient = dividend / divisor;

		/* store 32-bit quotient */
			divide[which].regs[4] = quotient >> 16;
			divide[which].regs[5] = quotient & 0xffff;
		}
	}

static data16_t divide_r(int which, offs_t offset, data16_t mem_mask)
{
	/* 8 effective read registers */
	offset &= 7;
	switch (offset)
	{
		case 0:	return divide[which].regs[0];	/* dividend high */
		case 1:	return divide[which].regs[1];	/* dividend low */
		case 2:	return divide[which].regs[2];	/* divisor */
		case 4: return divide[which].regs[4];	/* quotient (mode 0) or quotient high (mode 1) */
		case 5:	return divide[which].regs[5];	/* remainder (mode 0) or quotient low (mode 1) */
		case 6: return divide[which].regs[6];	/* flags */
	}
	return 0xffff;
}


static void divide_w(int which, offs_t offset, data16_t data, data16_t mem_mask)
{
	int a4 = offset & 8;
	int a3 = offset & 4;

	if (LOG_DIVIDE) logerror("%06X:divide%d_w(%X) = %04X\n", activecpu_get_pc(), which, offset, data);

	/* only 4 effective write registers */
	offset &= 3;
	switch (offset)
	{
		case 0:	COMBINE_DATA(&divide[which].regs[0]); break;	/* dividend high */
		case 1:	COMBINE_DATA(&divide[which].regs[1]); break;	/* dividend low */
		case 2:	COMBINE_DATA(&divide[which].regs[2]); break;	/* divisor/trigger */
		case 3:	break;
	}

	/* if a4 line is high, divide, using a3 as the mode */
	if (a4) update_divide(which, a3);
}


READ16_HANDLER( segaic16_divide_0_r )  { return divide_r(0, offset, mem_mask); }
READ16_HANDLER( segaic16_divide_1_r )  { return divide_r(1, offset, mem_mask); }
READ16_HANDLER( segaic16_divide_2_r )  { return divide_r(2, offset, mem_mask); }
WRITE16_HANDLER( segaic16_divide_0_w ) { divide_w(0, offset, data, mem_mask); }
WRITE16_HANDLER( segaic16_divide_1_w ) { divide_w(1, offset, data, mem_mask); }
WRITE16_HANDLER( segaic16_divide_2_w ) { divide_w(2, offset, data, mem_mask); }



/*************************************
 *
 *	Compare/timer chip
 *
 *************************************/

void segaic16_compare_timer_init(int which, void (*sound_write_callback)(data8_t), void (*timer_ack_callback)(void))
{
	compare_timer[which].sound_w = sound_write_callback;
	compare_timer[which].timer_ack = timer_ack_callback;
	compare_timer[which].counter = 0;
}


int segaic16_compare_timer_clock(int which)
{
	int old_counter = compare_timer[which].counter;
	int result = 0;
	
	/* if we're enabled, clock the upcounter */
	if (compare_timer[which].regs[10] & 1)
		compare_timer[which].counter++;

	/* regardless of the enable, a value of 0xfff will generate the IRQ */
	if (old_counter == 0xfff)
	{
		result = 1;
		compare_timer[which].counter = compare_timer[which].regs[8] & 0xfff;
	}
	return result;
}


static void update_compare(int which, int update_history)
{
	int bound1 = (INT16)compare_timer[which].regs[0];
	int bound2 = (INT16)compare_timer[which].regs[1];
	int value = (INT16)compare_timer[which].regs[2];
	int min = (bound1 < bound2) ? bound1 : bound2;
	int max = (bound1 > bound2) ? bound1 : bound2;

	if (value < min)
	{
		compare_timer[which].regs[7] = min;
		compare_timer[which].regs[3] = 0x8000;
	}
	else if (value > max)
	{
		compare_timer[which].regs[7] = max;
		compare_timer[which].regs[3] = 0x4000;
	}
	else
	{
		compare_timer[which].regs[7] = value;
		compare_timer[which].regs[3] = 0x0000;
	}

	if (update_history)
		compare_timer[which].regs[4] |= (compare_timer[which].regs[3] == 0) << compare_timer[which].bit++;
}


static void timer_interrupt_ack(int which)
{
	if (compare_timer[which].timer_ack)
		(*compare_timer[which].timer_ack)();
}


static data16_t compare_timer_r(int which, offs_t offset, data16_t mem_mask)
{
	offset &= 0xf;
	if (LOG_COMPARE) logerror("%06X:compare%d_r(%X) = %04X\n", activecpu_get_pc(), which, offset, compare_timer[which].regs[offset]);
	switch (offset)
	{
		case 0x0:	return compare_timer[which].regs[0];
		case 0x1:	return compare_timer[which].regs[1];
		case 0x2:	return compare_timer[which].regs[2];
		case 0x3:	return compare_timer[which].regs[3];
		case 0x4:	return compare_timer[which].regs[4];
		case 0x5:	return compare_timer[which].regs[1];
		case 0x6:	return compare_timer[which].regs[2];
		case 0x7:	return compare_timer[which].regs[7];
		case 0x9:
		case 0xd:	timer_interrupt_ack(which); break;
	}
	return 0xffff;
}


static void compare_timer_w(int which, offs_t offset, data16_t data, data16_t mem_mask)
{
	offset &= 0xf;
	if (LOG_COMPARE) logerror("%06X:compare%d_w(%X) = %04X\n", activecpu_get_pc(), which, offset, data);
	switch (offset)
	{
		case 0x0:	COMBINE_DATA(&compare_timer[which].regs[0]); update_compare(which, 0); break;
		case 0x1:	COMBINE_DATA(&compare_timer[which].regs[1]); update_compare(which, 0); break;
		case 0x2:	COMBINE_DATA(&compare_timer[which].regs[2]); update_compare(which, 1); break;
		case 0x4:	compare_timer[which].regs[4] = 0; compare_timer[which].bit = 0; break;
		case 0x6:	COMBINE_DATA(&compare_timer[which].regs[2]); update_compare(which, 0); break;
		case 0x8:	
		case 0xc:	COMBINE_DATA(&compare_timer[which].regs[8]); break;
		case 0x9:
		case 0xd:	timer_interrupt_ack(which); break;
		case 0xa:	
		case 0xe:	COMBINE_DATA(&compare_timer[which].regs[10]); break;
		case 0xb:	
		case 0xf:
			COMBINE_DATA(&compare_timer[which].regs[11]);
			if (compare_timer[which].sound_w)
				(*compare_timer[which].sound_w)(compare_timer[which].regs[11]);
			break;
	}
}


READ16_HANDLER( segaic16_compare_timer_0_r )  { return compare_timer_r(0, offset, mem_mask); }
READ16_HANDLER( segaic16_compare_timer_1_r )  { return compare_timer_r(1, offset, mem_mask); }
WRITE16_HANDLER( segaic16_compare_timer_0_w ) { compare_timer_w(0, offset, data, mem_mask); }
WRITE16_HANDLER( segaic16_compare_timer_1_w ) { compare_timer_w(1, offset, data, mem_mask); }
