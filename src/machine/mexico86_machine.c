#include "driver.h"


unsigned char *mexico86_protection_ram;

/*AT*/
/***************************************************************************

 Collision logic used by Kiki Kaikai (theoretical)

***************************************************************************/
#define KIKI_CL_OUT 0xa2
#define KIKI_CL_TRIGGER 0xa3
#define DCWIDTH 0
#define DCHEIGHT 0

static void kiki_clogic(int address, int latch)
{
	static UINT8 db[16]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x10,0x18,0x00,0x00,0x00,0x00};
	static UINT8 queue[64];
	static int qfront = 0, state = 0;
	int sy, sx, hw, i, qptr, diff1, diff2;

	if (address != KIKI_CL_TRIGGER) /* queue latched data*/
	{
		queue[qfront++] = latch;
		qfront &= 0x3f;
	}
	else if (state ^= 1) /* scan queue*/
	{
		sy = queue[(qfront-0x3a)&0x3f] + ((0x18-DCHEIGHT)>>1);
		sx = queue[(qfront-0x39)&0x3f] + ((0x18-DCWIDTH)>>1);

		for (i=0x38; i; i-=8)
		{
			qptr = qfront - i;
			if (!(hw = db[queue[qptr&0x3f]&0xf])) continue;

			diff1 = sx - (short)(queue[(qptr+6)&0x3f]<<8|queue[(qptr+7)&0x3f]) + DCWIDTH;
			diff2 = diff1 - (hw + DCWIDTH);
			if ((diff1^diff2)<0)
			{
				diff1 = sy - (short)(queue[(qptr+4)&0x3f]<<8|queue[(qptr+5)&0x3f]) + DCHEIGHT;
				diff2 = diff1 - (hw + DCHEIGHT);
				if ((diff1^diff2)<0)
					mexico86_protection_ram[KIKI_CL_OUT] = 1; /* we have a collision*/
			}
		}
	}
}
/*ZT*/

/***************************************************************************

 Mexico 86 68705 protection interface

 The following is ENTIRELY GUESSWORK!!!

***************************************************************************/

INTERRUPT_GEN( mexico86_m68705_interrupt )
{
	/* I don't know how to handle the interrupt line so I just toggle it every time. */
	if (cpu_getiloops() & 1)
		cpu_set_irq_line(2,0,CLEAR_LINE);
	else
		cpu_set_irq_line(2,0,ASSERT_LINE);
}



static unsigned char portA_in,portA_out,ddrA;

READ_HANDLER( mexico86_68705_portA_r )
{
/*logerror("%04x: 68705 port A read %02x\n",activecpu_get_pc(),portA_in);*/
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

WRITE_HANDLER( mexico86_68705_portA_w )
{
/*logerror("%04x: 68705 port A write %02x\n",activecpu_get_pc(),data);*/
	portA_out = data;
}

WRITE_HANDLER( mexico86_68705_ddrA_w )
{
	ddrA = data;
}



/*
 *  Port B connections:
 *
 *  all bits are logical 1 when read (+5V pullup)
 *
 *  0   W  enables latch which holds data from main Z80 memory
 *  1   W  loads the latch which holds the low 8 bits of the address of
 *               the main Z80 memory location to access
 *  2   W  0 = read input ports, 1 = access Z80 memory
 *  3   W  clocks main Z80 memory access
 *  4   W  selects Z80 memory access direction (0 = write 1 = read)
 *  5   W  clocks a flip-flop which causes IRQ on the main Z80
 *  6   W  not used?
 *  7   W  not used?
 */

static unsigned char portB_in,portB_out,ddrB;

READ_HANDLER( mexico86_68705_portB_r )
{
	return (portB_out & ddrB) | (portB_in & ~ddrB);
}

static int address,latch;

WRITE_HANDLER( mexico86_68705_portB_w )
{
/*logerror("%04x: 68705 port B write %02x\n",activecpu_get_pc(),data);*/

	if ((ddrB & 0x01) && (~data & 0x01) && (portB_out & 0x01))
	{
		portA_in = latch;
	}
	if ((ddrB & 0x02) && (data & 0x02) && (~portB_out & 0x02)) /* positive edge trigger */
	{
		address = portA_out;
/*if (address >= 0x80) log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: 68705 address %02x\n",activecpu_get_pc(),portA_out);*/
	}
	if ((ddrB & 0x08) && (~data & 0x08) && (portB_out & 0x08))
	{
		if (data & 0x10)    /* read */
		{
			if (data & 0x04)
			{
/*logerror("%04x: 68705 read %02x from address %04x\n",activecpu_get_pc(),shared[0x800+address],address);*/
				latch = mexico86_protection_ram[address];
				kiki_clogic(address, latch); /*AT*/
			}
			else
			{
/*logerror("%04x: 68705 read input port %04x\n",activecpu_get_pc(),address);*/
				latch = readinputport((address & 1) + 1);
			}
		}
		else    /* write */
		{
/*logerror("%04x: 68705 write %02x to address %04x\n",activecpu_get_pc(),portA_out,address);*/
				mexico86_protection_ram[address] = portA_out;
		}
	}
	if ((ddrB & 0x20) && (data & 0x20) && (~portB_out & 0x20))
	{
		cpu_irq_line_vector_w(0,0,mexico86_protection_ram[0]);
		/*cpu_set_irq_line(0,0,PULSE_LINE);*/
		cpu_set_irq_line(0, 0, HOLD_LINE); /*AT: HOLD_LINE works better in Z80 interrupt mode 1.*/
	}
	if ((ddrB & 0x40) && (~data & 0x40) && (portB_out & 0x40))
	{
log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: 68705 unknown port B bit %02x\n",activecpu_get_pc(),data);
	}
	if ((ddrB & 0x80) && (~data & 0x80) && (portB_out & 0x80))
	{
log_cb(RETRO_LOG_DEBUG, LOGPRE "%04x: 68705 unknown port B bit %02x\n",activecpu_get_pc(),data);
	}

	portB_out = data;
}

WRITE_HANDLER( mexico86_68705_ddrB_w )
{
	ddrB = data;
}
