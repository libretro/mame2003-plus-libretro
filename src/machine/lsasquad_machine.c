#include "driver.h"
#include "cpu/z80/z80.h"
#include "random.h"


/* coin inputs are inverted in storming */
int lsasquad_invertcoin;


/***************************************************************************

 main <-> sound CPU communication

***************************************************************************/

static int sound_nmi_enable,pending_nmi,sound_cmd,sound_result;
int lsasquad_sound_pending;

static void nmi_callback(int param)
{
	if (sound_nmi_enable) cpu_set_irq_line(1,IRQ_LINE_NMI,PULSE_LINE);
	else pending_nmi = 1;
}

WRITE_HANDLER( lsasquad_sh_nmi_disable_w )
{
	sound_nmi_enable = 0;
}

WRITE_HANDLER( lsasquad_sh_nmi_enable_w )
{
	sound_nmi_enable = 1;
	if (pending_nmi)
	{
		cpu_set_irq_line(1,IRQ_LINE_NMI,PULSE_LINE);
		pending_nmi = 0;
	}
}

WRITE_HANDLER( lsasquad_sound_command_w )
{
	lsasquad_sound_pending |= 0x01;
	sound_cmd = data;
/*logerror("%04x: sound cmd %02x\n",activecpu_get_pc(),data);*/
	timer_set(TIME_NOW,data,nmi_callback);
}

READ_HANDLER( lsasquad_sh_sound_command_r )
{
	lsasquad_sound_pending &= ~0x01;
/*logerror("%04x: read sound cmd %02x\n",activecpu_get_pc(),sound_cmd);*/
	return sound_cmd;
}

WRITE_HANDLER( lsasquad_sh_result_w )
{
	lsasquad_sound_pending |= 0x02;
/*logerror("%04x: sound res %02x\n",activecpu_get_pc(),data);*/
	sound_result = data;
}

READ_HANDLER( lsasquad_sound_result_r )
{
	lsasquad_sound_pending &= ~0x02;
/*logerror("%04x: read sound res %02x\n",activecpu_get_pc(),sound_result);*/
	return sound_result;
}

READ_HANDLER( lsasquad_sound_status_r )
{
	/* bit 0: message pending for sound cpu */
	/* bit 1: message pending for main cpu */
	return lsasquad_sound_pending;
}

READ_HANDLER( daikaiju_sh_sound_command_r )
{
	lsasquad_sound_pending &= ~0x01;
	lsasquad_sound_pending |= 0x02;
/* logerror("%04x: read sound cmd %02x\n",activecpu_get_pc(),sound_cmd); */
	return sound_cmd;
}

READ_HANDLER( daikaiju_sound_status_r )
{
	/* bit 0: message pending for sound cpu */
	/* bit 1: message pending for main cpu */
	return lsasquad_sound_pending^3;
}



/***************************************************************************

 LSA Squad 68705 protection interface

 The following is ENTIRELY GUESSWORK!!!

***************************************************************************/

static unsigned char from_main,from_mcu;
static int mcu_sent = 0,main_sent = 0;

static unsigned char portA_in,portA_out,ddrA;

READ_HANDLER( lsasquad_68705_portA_r )
{
/*logerror("%04x: 68705 port A read %02x\n",activecpu_get_pc(),portA_in);*/
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

WRITE_HANDLER( lsasquad_68705_portA_w )
{
/*logerror("%04x: 68705 port A write %02x\n",activecpu_get_pc(),data);*/
	portA_out = data;
}

WRITE_HANDLER( lsasquad_68705_ddrA_w )
{
	ddrA = data;
}



/*
 *  Port B connections:
 *
 *  all bits are logical 1 when read (+5V pullup)
 *
 *  1   W  when 1->0, enables latch which brings the command from main CPU (read from port A)
 *  2   W  when 0->1, copies port A to the latch for the main CPU
 */

static unsigned char portB_in,portB_out,ddrB;

READ_HANDLER( lsasquad_68705_portB_r )
{
	return (portB_out & ddrB) | (portB_in & ~ddrB);
}

WRITE_HANDLER( lsasquad_68705_portB_w )
{
/*logerror("%04x: 68705 port B write %02x\n",activecpu_get_pc(),data);*/

	if ((ddrB & 0x02) && (~data & 0x02) && (portB_out & 0x02))
	{
		portA_in = from_main;
		if (main_sent) cpu_set_irq_line(2,0,CLEAR_LINE);
		main_sent = 0;
/*logerror("read command %02x from main cpu\n",portA_in);*/
	}
	if ((ddrB & 0x04) && (data & 0x04) && (~portB_out & 0x04))
	{
/*logerror("send command %02x to main cpu\n",portA_out);*/
		from_mcu = portA_out;
		mcu_sent = 1;
	}

	portB_out = data;
}

WRITE_HANDLER( lsasquad_68705_ddrB_w )
{
	ddrB = data;
}

WRITE_HANDLER( lsasquad_mcu_w )
{
/*logerror("%04x: mcu_w %02x\n",activecpu_get_pc(),data);*/
	from_main = data;
	main_sent = 1;
	cpu_set_irq_line(2,0,ASSERT_LINE);
}

READ_HANDLER( lsasquad_mcu_r )
{
/*logerror("%04x: mcu_r %02x\n",activecpu_get_pc(),from_mcu);*/
	mcu_sent = 0;
	return from_mcu;
}

READ_HANDLER( lsasquad_mcu_status_r )
{
	int res = input_port_3_r(0);

	/* bit 0 = when 1, mcu is ready to receive data from main cpu */
	/* bit 1 = when 0, mcu has sent data to the main cpu */
/*logerror("%04x: mcu_status_r\n",activecpu_get_pc());*/
	if (!main_sent) res |= 0x01;
	if (!mcu_sent) res |= 0x02;

	return res ^ lsasquad_invertcoin;
}

/* DAIKAIJU */

#define ID_DAIKAIJU 0x5a

#define MCU_UNKNOWN   0x19
#define MCU_WATCHDOG  0x1d
#define MCU_ADDRESS1  0x31
#define MCU_ADDRESS2  0x32
#define MCU_CONTROLS1 0x36
#define MCU_MESSAGE   0x40
#define MCU_CONTROLS2 0x44
#define MCU_ID 				0x60

#define MCU_ADDRESS1_LENGTH  2
#define MCU_ADDRESS2_LENGTH  2
#define MCU_CONTROLS1_LENGTH 2
#define MCU_MESSAGE_LENGTH   3
#define MCU_CONTROLS2_LENGTH 3
#define MCU_ID_LENGTH				 0

#define SET_COMMAND(n)	daikaiju_command=data; \
			daikaiju_length=n;\
			daikaiju_cnt=0;\
			daikaiju_cntr=0;


extern int lsasquad_sound_pending;
static int daikaiju_xor, daikaiju_command, daikaiju_length, daikaiju_prev, daikaiju_cnt, daikaiju_cntr;

static int daikaiju_buffer[256];

static int xortable[]=
{
 0xF5, 0xD5, 0x6A, 0x26, 0x00, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x16, 0x00, 0xCB, 0x23, 0x19,
 0x11, 0x00, 0xC0, 0x19, 0xD1, 0xF1, 0xC9, -1
};

MACHINE_INIT(daikaiju)
{
	daikaiju_xor=-1;
	daikaiju_command=0;
	daikaiju_length=0;
}

WRITE_HANDLER( daikaiju_mcu_w )
{
  if(daikaiju_xor <0)
	{
		/* reset */
		daikaiju_xor=0;
		data^=0xc3;
	}
	else
	{
		data^=xortable[daikaiju_xor];
		/* check for end of table */
		if(xortable[++daikaiju_xor]<0)
		{
			daikaiju_xor=0;
		}
	}
	daikaiju_prev=data;

	if(daikaiju_length == 0) /* new command */
	{
		switch(data)
		{
			case MCU_UNKNOWN:
			case MCU_WATCHDOG: break;
			case MCU_ADDRESS1:  SET_COMMAND(MCU_ADDRESS1_LENGTH); break;
			case MCU_ADDRESS2:  SET_COMMAND(MCU_ADDRESS2_LENGTH); break;
			case MCU_CONTROLS1: SET_COMMAND(MCU_CONTROLS1_LENGTH); break;
			case MCU_MESSAGE: 	SET_COMMAND(MCU_MESSAGE_LENGTH); break;
			case MCU_CONTROLS2: SET_COMMAND(MCU_CONTROLS2_LENGTH); break;
			case MCU_ID: 				SET_COMMAND(MCU_ID_LENGTH); break;
			default:
				daikaiju_command=data;
					log_cb(RETRO_LOG_DEBUG, LOGPRE "Unknown MCU command W %x %x \n",data,activecpu_get_pc());
		}
	}
	else
	{
		--daikaiju_length;
		daikaiju_buffer[daikaiju_cnt++]=data;
	}
}

READ_HANDLER( daikaiju_mcu_r )
{
	switch(daikaiju_command)
	{
		case MCU_ID:
			return ID_DAIKAIJU;

		case MCU_CONTROLS1:
 		{
			int n;
			switch( (daikaiju_buffer[0]&0xf)^0xf)
			{
				case 0x00:	n = (daikaiju_buffer[1]&(~8))&0xf; break;
				case 0x08:	n = 0 | 8; break;
				case 0x0a:	n = 1 | 8; break;
				case 0x02:	n = 2 | 8; break;
				case 0x06:	n = 3 | 8; break;
				case 0x04:	n = 4 | 8; break;
				case 0x05:	n = 5 | 8; break;
				case 0x01:	n = 6 | 8; break;
				case 0x09:	n = 7 | 8; break;

				default:		n = 0; break;
			}

			if( !(daikaiju_buffer[0]&0x10) ) /* button 1 pressed */
			{
				if(daikaiju_buffer[1]&0x10) /* previous status */
				{
					n|=0x40;	/* button 0->1 */
				}
				else
				{
					n|=0; /* button 1->1 */
				}
			}
			else
			{
				if(daikaiju_buffer[1]&0x10) /* previous status */
				{
					n|=0x40+0x10;	/* button 0->0 */
				}
				else
				{
					n|=0x10; /* button 1->0 */
				}
			}

			if( !(daikaiju_buffer[0]&0x20) ) /* button 2 pressed */
			{
				if(daikaiju_buffer[1]&0x20) /* previous status */
				{
					n|=0x80;	/* button 0->1 */
				}
				else
				{
					n|=0; /* button 1->1 */
				}
			}
			else
			{
				if(daikaiju_buffer[1]&0x20) /* previous status */
				{
					n|=0x80+0x20;	/* button 0->0 */
				}
				else
				{
					n|=0x20; /* button 1->0 */
				}
			}
			return n;
		}
		return 0;

		case MCU_ADDRESS1:
		{
			int address=daikaiju_buffer[1]*2+64*daikaiju_buffer[0];

			address&=0x0fff; /* ? */
			address+=0xd000;

			if(daikaiju_cntr==0)
			{
				daikaiju_cntr++;
				return address&0xff;
			}
			else
			{
				return address>>8;
			}
		}
		return 0;

		case MCU_ADDRESS2:
		{
			int address=daikaiju_buffer[1]*2+64*daikaiju_buffer[0];

			address&=0x1fff; /* ? */
			address+=0xc000;

			if(daikaiju_cntr==0)
			{
				daikaiju_cntr++;
				return address&0xff;
			}
			else
			{
				return address>>8;
			}
		}
		return 0;

		case MCU_MESSAGE:
			return daikaiju_buffer[1];

		case MCU_CONTROLS2:
		{
			int n;
			int dest=(daikaiju_buffer[0]&0x7)<<1;
			int prev=(daikaiju_buffer[1]&0xf);

			if(daikaiju_buffer[2]==2)
			{
				prev&=0xfe;
			}
			if(prev!=dest)
			{
				int diff=(dest-prev);

				if((diff>8 )|| (diff<0 && diff >-8))
				{
					n=(prev-daikaiju_buffer[2])&0xf;
				}
				else
				{
					n=(prev+daikaiju_buffer[2])&0xf;
				}
			}
			else
			{
				n=prev;
			}
			prev&=0xf;
			if(prev!=n)
			{
				n|=0x10;
			}
			return n;
		}
	}
		log_cb(RETRO_LOG_DEBUG, LOGPRE "Unknown MCU R %x %x %x %x\n",activecpu_get_pc(), daikaiju_command, daikaiju_length, daikaiju_prev);
	return 0xff;
}

READ_HANDLER( daikaiju_mcu_status_r )
{
	int res = input_port_3_r(0);

	res^=mame_rand()&3;
	res |=((lsasquad_sound_pending & 0x02)^2)<<3; /* inverted flag */
	lsasquad_sound_pending &= ~0x02;
	return res;
}
