/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

unsigned char *xevious_sharedram;
static unsigned char interrupt_enable_1,interrupt_enable_2,interrupt_enable_3;

static unsigned char *rom2a;
static unsigned char *rom2b;
static unsigned char *rom2c;
static int xevious_bs[2];

static void *nmi_timer;

WRITE_HANDLER( xevious_halt_w );
void xevious_nmi_generate (int param);

/* namco stick number array */
/*
  Input bitmap
    bit0 = UP	 KEY
    bit1 = RIGHT KEY
    bit2 = DOWN  KEY
    bit3 = LEFT  KEY

  Output direction
	      0
	    7	1
	  6   8   2
	    5	3
	      4
 */
unsigned char namco_key[16] =
/*  LDRU,LDR,LDU,LD ,LRU,LR ,LU , L ,DRU,DR ,DU , D ,RU , R , U ,NON  */
  {   5 , 5 , 5 , 5 , 7 , 6 , 7 , 6 , 3 , 3 , 4 , 4 , 1 , 2 , 0 , 8 };



MACHINE_INIT( xevious )
{
	rom2a = memory_region(REGION_GFX4);
	rom2b = memory_region(REGION_GFX4)+0x1000;
	rom2c = memory_region(REGION_GFX4)+0x3000;

	nmi_timer = timer_alloc(xevious_nmi_generate);

	xevious_halt_w (0, 0);
}

/* emulation for schematic 9B */
WRITE_HANDLER( xevious_bs_w )
{
	xevious_bs[offset & 0x01] = data;
}

READ_HANDLER( xevious_bb_r )
{
	int adr_2b,adr_2c;
	int dat1,dat2;


	/* get BS to 12 bit data from 2A,2B */
	adr_2b = ((xevious_bs[1]&0x7e)<<6)|((xevious_bs[0]&0xfe)>>1);
	if( adr_2b & 1 ){
		/* high bits select */
		dat1 = ((rom2a[adr_2b>>1]&0xf0)<<4)|rom2b[adr_2b];
	}else{
	    /* low bits select */
	    dat1 = ((rom2a[adr_2b>>1]&0x0f)<<8)|rom2b[adr_2b];
	}
	adr_2c = (dat1 & 0x1ff)<<2;
	if( offset & 0x01 )
		adr_2c += (1<<11);	/* signal 4H to A11 */
	if( (xevious_bs[0]&1) ^ ((dat1>>10)&1) )
		adr_2c |= 1;
	if( (xevious_bs[1]&1) ^ ((dat1>>9)&1) )
		adr_2c |= 2;
	if( offset & 0x01 ){
		/* return BB1 */
		dat2 = rom2c[adr_2c];
	}else{
		/* return BB0 */
		dat2 =rom2c[adr_2c];
		/* swap bit 6 & 7 */
		dat2 = (dat2 & 0x3f) | ((dat2 & 0x80) >> 1) | ((dat2 & 0x40) << 1);
		/* flip x & y */
		dat2 ^= (dat1 >> 4) & 0x40;
		dat2 ^= (dat1 >> 2) & 0x80;
	}
	return dat2;
}

READ_HANDLER( xevious_sharedram_r )
{
	return xevious_sharedram[offset];
}

WRITE_HANDLER( xevious_sharedram_w )
{
	xevious_sharedram[offset] = data;
}



READ_HANDLER( xevious_dsw_r )
{
	int bit0,bit1;

	bit0 = (input_port_0_r(0) >> offset) & 1;
	bit1 = (input_port_1_r(0) >> offset) & 1;

	return bit0 | (bit1 << 1);
}

/***************************************************************************

 Emulate the custom IO chip.

***************************************************************************/
static int customio_command;
static int mode,credits,start_enable;
static int auxcoinpercred,auxcredpercoin;
static int leftcoinpercred,leftcredpercoin;
static int rightcoinpercred,rightcredpercoin;
static unsigned char customio[16];


WRITE_HANDLER( xevious_customio_data_w )
{
	customio[offset] = data;

logerror("%04x: custom IO offset %02x data %02x\n",activecpu_get_pc(),offset,data);

	switch (customio_command & 0x0f)
	{
		case 0x01:
			if (offset == 0)
			{
				switch ( data & 0x0f )
				{
					case 0x00:
						/* nop */
						break;
					case 0x01:
						/* credit info set */
						credits = 0;		/* this is a good time to reset the credits counter */
						mode = 0;			/* go into credit mode */
						start_enable = 1;
						break;
					case 0x02:
						start_enable = 1;
						break;
					case 0x03:
						mode = 1;			/* go into switch mode */
						break;
					case 0x04:
						mode = 0;			/* go into credit mode */
						break;
					case 0x05:
						start_enable = 0;	/* Initialize */
						mode = 1;			/* Initialize */
						break;
				}
			}
			else if (offset == 7)
			{
				/* 0x01 */
				auxcoinpercred = customio[1];
				auxcredpercoin = customio[2];
				leftcoinpercred = customio[3];
				leftcredpercoin = customio[4];
				rightcoinpercred = customio[5];
				rightcredpercoin = customio[6];
			}
			break;

		case 0x04:
			/* ??? */
			break;

		case 0x08:
			if (offset == 6)
			{
				/* it is not known how the parameters control the explosion. */
				/* We just use samples. */
				if (memcmp(customio,"\x40\x40\x40\x01\xff\x00\x20",7) == 0)
				{
					sample_start (0, 0, 0);
				}
				else if (memcmp(customio,"\x30\x40\x00\x02\xdf\x00\x10",7) == 0)
				{
					sample_start (0, 1, 0);
				}
				else if (memcmp(customio,"\x30\x10\x00\x80\xff\x00\x10",7) == 0)
				{
					sample_start (0, 2, 0);
				}
				else if (memcmp(customio,"\x30\x80\x80\x01\xff\x00\x10",7) == 0)
				{
					sample_start (0, 3, 0);
				}
				else
				{
					logerror("%04x: custom IO offset %02x\n",activecpu_get_pc(),offset);
					logerror("data[0]=%02x\n",customio[0]);
					logerror("data[1]=%02x\n",customio[1]);
					logerror("data[2]=%02x\n",customio[2]);
					logerror("data[3]=%02x\n",customio[3]);
					logerror("data[4]=%02x\n",customio[4]);
					logerror("data[5]=%02x\n",customio[5]);
					logerror("data[6]=%02x\n",customio[6]);
				}
			}
			break;
	}
}


READ_HANDLER( xevious_customio_data_r )
{
	if (customio_command != 0x71)
		logerror("%04x: custom IO read offset %02x\n",activecpu_get_pc(),offset);

	switch (customio_command & 0x0f)
	{
		case 0x01:	/* read input */
			if (offset == 0)
			{
				if (mode)	/* switch mode */
				{
					/* bit 7 is the service switch */
					return readinputport(4);
				}
				else	/* credits mode: return number of credits in BCD format */
				{
					int in;
					static int leftcoininserted;
					static int rightcoininserted;
					static int auxcoininserted;

					in = readinputport(4);

					/* check if the user inserted a coin */
					if (leftcoinpercred > 0)
					{
						if ((in & 0x10) == 0 && credits < 99)
						{
							leftcoininserted++;
							if (leftcoininserted >= leftcoinpercred)
							{
								credits += leftcredpercoin;
								leftcoininserted = 0;
							}
						}
						if ((in & 0x20) == 0 && credits < 99)
						{
							rightcoininserted++;
							if (rightcoininserted >= rightcoinpercred)
							{
								credits += rightcredpercoin;
								rightcoininserted = 0;
							}
						}
						if ((in & 0x40) == 0 && credits < 99)
						{
							auxcoininserted++;
							if (auxcoininserted >= auxcoinpercred)
							{
								credits += auxcredpercoin;
								auxcoininserted = 0;
							}
						}
					}
					else credits = 2;

					if (start_enable == 1)
					{
						/* check for 1 player start button */
						if ((in & 0x04) == 0)
						{
							if (credits >= 1)
							{
								credits--;
								start_enable = 0;
							}
						}

						/* check for 2 players start button */
						if ((in & 0x08) == 0)
						{
							if (credits >= 2)
							{
								credits -= 2;
								start_enable = 0;
							}
						}
					}
					return (credits / 10) * 16 + credits % 10;
				}
			}
			else if (offset == 1)
			{
				int in;

				in = readinputport(2);	/* player 1 input */
				if (mode == 0)	/* convert joystick input only when in credits mode */
					in = namco_key[in & 0x0f] | (in & 0xf0);
				return in;
			}
			else if (offset == 2)
			{
				int in;

				in = readinputport(3);	/* player 2 input */
				if (mode == 0)	/* convert joystick input only when in credits mode */
					in = namco_key[in & 0x0f] | (in & 0xf0);
				return in;
			}

			break;

		case 0x04:		/* protect data read ? */
			if (offset == 3)
			{
				if (customio[0] == 0x80 || customio[0] == 0x10)
					return 0x05;	/* 1st check */
				else
					return 0x95;  /* 2nd check */
			}
			else return 0x00;
			break;
	}

	return -1;
}


READ_HANDLER( xevious_customio_r )
{
	return customio_command;
}

void xevious_nmi_generate (int param)
{
	cpu_set_irq_line (0, IRQ_LINE_NMI, PULSE_LINE);
}


WRITE_HANDLER( xevious_customio_w )
{
	if (data != 0x10 && data != 0x71)
		logerror("%04x: custom IO command %02x\n",activecpu_get_pc(),data);

	customio_command = data;

	switch (data)
	{
		case 0x10:
			timer_adjust(nmi_timer, TIME_NEVER, 0, 0);
			return; /* nop */
	}
	timer_adjust(nmi_timer, TIME_IN_USEC(50), 0, TIME_IN_USEC(50));

}



WRITE_HANDLER( xevious_halt_w )
{
	if (data & 1)
	{
		cpu_set_reset_line(1,CLEAR_LINE);
		cpu_set_reset_line(2,CLEAR_LINE);
	}
	else
	{
		cpu_set_reset_line(1,ASSERT_LINE);
		cpu_set_reset_line(2,ASSERT_LINE);
	}
}



WRITE_HANDLER( xevious_interrupt_enable_1_w )
{
	interrupt_enable_1 = (data&1);
}



INTERRUPT_GEN( xevious_interrupt_1 )
{
	if (interrupt_enable_1)
		cpu_set_irq_line(0, 0, HOLD_LINE);
}



WRITE_HANDLER( xevious_interrupt_enable_2_w )
{
	interrupt_enable_2 = data & 1;
}



INTERRUPT_GEN( xevious_interrupt_2 )
{
	if (interrupt_enable_2)
		cpu_set_irq_line(1, 0, HOLD_LINE);
}



WRITE_HANDLER( xevious_interrupt_enable_3_w )
{
	interrupt_enable_3 = !(data & 1);
}



INTERRUPT_GEN( xevious_interrupt_3 )
{
	if (interrupt_enable_3)
		cpu_set_irq_line(2, IRQ_LINE_NMI, PULSE_LINE);
}



/***************************************************************************

 BATTLES CPU4(custum I/O Emulation) I/O Handlers

***************************************************************************/

unsigned char *battles_sharedram;
static char battles_customio_command;
static char battles_customio_prev_command;
static char battles_customio_command_count;
static char battles_customio_data;
static char battles_sound_played;

void battles_nmi_generate(int param);


MACHINE_INIT( battles )
{
	rom2a = memory_region(REGION_GFX4);
	rom2b = memory_region(REGION_GFX4)+0x1000;
	rom2c = memory_region(REGION_GFX4)+0x3000;

	battles_customio_command = 0;
	battles_customio_prev_command = 0;
	battles_customio_command_count = 0;
	battles_customio_data = 0;
	battles_sound_played = 0;
	nmi_timer = timer_alloc(battles_nmi_generate);

	xevious_halt_w (0, 0);
}


void battles_nmi_generate(int param)
{

	battles_customio_prev_command = battles_customio_command;

	if( battles_customio_command & 0x10 ){
		if( battles_customio_command_count == 0 ){
			cpu_set_irq_line(3, IRQ_LINE_NMI, PULSE_LINE);
		}else{
			cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
			cpu_set_irq_line(3, IRQ_LINE_NMI, PULSE_LINE);
		}
	}else{
		cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
		cpu_set_irq_line(3, IRQ_LINE_NMI, PULSE_LINE);
	}
	battles_customio_command_count++;
}


READ_HANDLER( battles_sharedram_r )
{
	return battles_sharedram[offset];
}

WRITE_HANDLER( battles_sharedram_w )
{
	battles_sharedram[offset] = data;
}



READ_HANDLER( battles_customio0_r )
{
	logerror("CPU0 %04x: custom I/O Read = %02x\n",activecpu_get_pc(),battles_customio_command);
	return battles_customio_command;
}

READ_HANDLER( battles_customio3_r )
{
	int	return_data;

	if( activecpu_get_pc() == 0xAE ){
		/* CPU4 0xAA - 0xB9 : waiting for MB8851 ? */
		return_data =	( (battles_customio_command & 0x10) << 3)
						| 0x00
						| (battles_customio_command & 0x0f);
	}else{
		return_data =	( (battles_customio_prev_command & 0x10) << 3)
						| 0x60
						| (battles_customio_prev_command & 0x0f);
	}
	logerror("CPU3 %04x: custom I/O Read = %02x\n",activecpu_get_pc(),return_data);

	return return_data;
}


WRITE_HANDLER( battles_customio0_w )
{
	logerror("CPU0 %04x: custom I/O Write = %02x\n",activecpu_get_pc(),data);

	battles_customio_command = data;
	battles_customio_command_count = 0;

	switch (data)
	{
		case 0x10:
			timer_adjust(nmi_timer, TIME_NEVER, 0, 0);
			return; /* nop */
	}
	timer_adjust(nmi_timer, TIME_IN_USEC(166), 0, TIME_IN_USEC(166));

}

WRITE_HANDLER( battles_customio3_w )
{
	logerror("CPU3 %04x: custom I/O Write = %02x\n",activecpu_get_pc(),data);

	battles_customio_command = data;
}



READ_HANDLER( battles_customio_data0_r )
{
	logerror("CPU0 %04x: custom I/O parameter %02x Read = %02x\n",activecpu_get_pc(),offset,battles_customio_data);

	return battles_customio_data;
}

READ_HANDLER( battles_customio_data3_r )
{
	logerror("CPU3 %04x: custom I/O parameter %02x Read = %02x\n",activecpu_get_pc(),offset,battles_customio_data);
	return battles_customio_data;
}


WRITE_HANDLER( battles_customio_data0_w )
{
	logerror("CPU0 %04x: custom I/O parameter %02x Write = %02x\n",activecpu_get_pc(),offset,data);
	battles_customio_data = data;
	customio[offset] = data;
}

WRITE_HANDLER( battles_customio_data3_w )
{
	logerror("CPU3 %04x: custom I/O parameter %02x Write = %02x\n",activecpu_get_pc(),offset,data);
	battles_customio_data = data;
}


WRITE_HANDLER( battles_CPU4_4000_w )
{
	logerror("CPU3 %04x: 40%02x Write = %02x\n",activecpu_get_pc(),offset,data);

	set_led_status(0,data & 0x02);	/* Start 1*/
	set_led_status(1,data & 0x01);	/* Start 2*/

}


WRITE_HANDLER( battles_noise_sound_w )
{
	logerror("CPU3 %04x: 50%02x Write = %02x\n",activecpu_get_pc(),offset,data);
	if( (battles_sound_played == 0) && (data == 0xFF) ){
		if( customio[0] == 0x40 ){
			sample_start (0, 0, 0);
		}
		else{
			sample_start (0, 1, 0);
		}
	}
	battles_sound_played = data;
}


READ_HANDLER( battles_input_port_r )
{
	logerror("battles_input_port_r %04x: Read offset %02x\n",activecpu_get_pc(),offset);
	return 0xff;
}


INTERRUPT_GEN( battles_interrupt_4 )
{
	cpu_set_irq_line(3, 0, HOLD_LINE);
}

