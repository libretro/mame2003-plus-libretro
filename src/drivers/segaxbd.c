#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "cpu/i8039/i8039.h"
#include "system16.h"
#include "vidhrdw/segaic16.h"
#include "machine/segaic16.h"
#include "cpu/m68000/m68000.h"

/*************************************
 *
 *	Initialization & interrupts
 *
 *************************************/

static UINT8 vblank_irq_state;
static UINT8 timer_irq_state;
static UINT8 iochip_regs[2][8];
static UINT8 iochip_force_input;
static data8_t (*iochip_custom_io_r[2])(offs_t offset, data8_t portdata);
static void (*iochip_custom_io_w[2])(offs_t offset, data8_t data);
static UINT8 adc_reverse[8];
static data16_t *backupram1, *backupram2, *segaic16_shared_ram0, *segaic16_shared_ram1, *segaic16_workingram1;



WRITE16_HANDLER(SYS16IC_MWA16_ROADRAM_SHARE ){ COMBINE_DATA( &segaic16_roadram_0[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_ROADRAM_SHARE ){ return segaic16_roadram_0[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_BACKUPRAM1 ){ COMBINE_DATA( &backupram1[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_BACKUPRAM1  ){ return backupram1[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_BACKUPRAM2 ){ COMBINE_DATA( &backupram2[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_BACKUPRAM2  ){ return backupram2[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_SHAREDRAM0 ){ COMBINE_DATA( &segaic16_shared_ram0[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_SHAREDRAM0  ){ return segaic16_shared_ram0[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_SHAREDRAM1 ){ COMBINE_DATA( &segaic16_shared_ram1[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_SHAREDRAM1  ){ return segaic16_shared_ram1[offset]; }

READ16_HANDLER( SYS16IC_MRA16_WORKINGRAM1_SHARE ){ return segaic16_workingram1[offset]; }
WRITE16_HANDLER(SYS16IC_MWA16_WORKINGRAM1_SHARE ){ COMBINE_DATA( &segaic16_workingram1[offset] ); }


static void update_main_irqs(void)
{
	int irq = 0;

	/* the IRQs are effectively ORed together */
	if (vblank_irq_state)
		irq |= 4;
	if (timer_irq_state)
		irq |= 2;

	/* assert the lines that are live, or clear everything if nothing is live */
	if (irq != 0)
	{
	cpu_set_irq_line(0, irq, ASSERT_LINE);
	//	cpunum_set_input_line(0, irq, ASSERT_LINE);
	cpu_boost_interleave(0, TIME_IN_USEC(50));
	}
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
		//cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void scanline_callback(int scanline)
{
	/* clock the timer and set the IRQ if something happened */
	if (segaic16_compare_timer_clock(0))
		timer_irq_state = 1;

	/* on scanline 223 generate VBLANK for both CPUs */
	if (scanline == 223)
	{
		vblank_irq_state = 1;
		cpu_set_irq_line(2, 4, HOLD_LINE);
		//cpunum_set_input_line(1, 4, HOLD_LINE);
		cpu_boost_interleave(0, TIME_IN_USEC(50));
	}

	/* update IRQs on the main CPU */
	update_main_irqs();

	/* come back in 2 scanlines */
	scanline = (scanline + 2) % 262;
	timer_set(cpu_getscanlinetime(scanline), scanline, scanline_callback);
}


static void timer_ack_callback(void)
{
	timer_irq_state = 0;
	update_main_irqs();
}


static void sound_data_w(data8_t data)
{
	soundlatch_w(0, data);
	cpu_set_irq_line(1, IRQ_LINE_NMI, PULSE_LINE);
}


static void xboard_reset(void)
{
 	cpu_set_reset_line(2, PULSE_LINE );
	//cpunum_set_input_line(1, INPUT_LINE_RESET, PULSE_LINE);
}


static int main_irq_callback(int irq)
{
	extern int fd1094_int_callback(int irq);

	if (irq & 4)
	{
		vblank_irq_state = 0;
		update_main_irqs();
	}

		return MC68000_INT_ACK_AUTOVECTOR;
}


VIDEO_START( xboard );
VIDEO_UPDATE( xboard );
void xboard_set_road_priority(int priority);




/*************************************
 *
 *	Sound definitions for new intterupts xboard machine driver only
 *
 *************************************/

static void sound_cpu_irq(int state)
{
	cpu_set_irq_line(1, 0, state);
}


/*************************************
 *
 *	Input handlers
 *
 *************************************/

static READ16_HANDLER( adc_r )
{
	static const char *ports[] = { "ADC0", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6", "ADC7" };
	int which = (iochip_regs[0][2] >> 2) & 7;

	/* on the write, latch the selected input port and stash the value */
	int value = readinputport(which+6);
	//int value = readinputportbytag_safe(ports[which], 0x0010);

	/* reverse some port values */
	if (adc_reverse[which])
		value = 255 - value;

	/* return the previously latched value */
	return value;
}


static WRITE16_HANDLER( adc_w )
{
}


static INLINE data16_t iochip_r(int which, int port, int inputval)
{
	data16_t result = iochip_regs[which][port];

	/* if there's custom I/O, do that to get the input value */
	if (iochip_custom_io_r[which])
		inputval = (*iochip_custom_io_r[which])(port, inputval);

	/* for ports 0-3, the direction is controlled 4 bits at a time by register 6 */
	if (port <= 3)
	{
		if (iochip_force_input || ((iochip_regs[which][6] >> (2*port+0)) & 1))
			result = (result & ~0x0f) | (inputval & 0x0f);
		if (iochip_force_input || ((iochip_regs[which][6] >> (2*port+1)) & 1))
			result = (result & ~0xf0) | (inputval & 0xf0);
	}

	/* for port 4, the direction is controlled 1 bit at a time by register 7 */
	else
	{
		if ((iochip_regs[which][7] >> 0) & 1)
			result = (result & ~0x01) | (inputval & 0x01);
		if ((iochip_regs[which][7] >> 1) & 1)
			result = (result & ~0x02) | (inputval & 0x02);
		if ((iochip_regs[which][7] >> 2) & 1)
			result = (result & ~0x04) | (inputval & 0x04);
		if ((iochip_regs[which][7] >> 3) & 1)
			result = (result & ~0x08) | (inputval & 0x08);
		result &= 0x0f;
	}
	return result;
}


static READ16_HANDLER( iochip_0_r )
{
	switch (offset & 7)
	{
		case 0:
			/* Input port:
				D7: (Not connected)
				D6: /INTR of ADC0804
				D5-D0: CN C pin 24-19 (switch state 0= open, 1= closed)
			*/
			return iochip_r(0, 0, readinputport(0));

		case 1:
			/* I/O port: CN C pins 17,15,13,11,9,7,5,3 */
			return iochip_r(0, 1, readinputport(1));

		case 2:
			/* Output port */
			return iochip_r(0, 2, 0);

		case 3:
			/* Output port */
			return iochip_r(0, 3, 0);

		case 4:
			/* Unused */
			return iochip_r(0, 4, 0);
	}

	/* everything else returns 0 */
	return 0;
}


static WRITE16_HANDLER( iochip_0_w )
{
	UINT8 oldval;

	/* access is via the low 8 bits */
	if (!ACCESSING_LSB)
		return;
	offset &= 7;
	data &= 0xff;

	/* swap in the new value and remember the previous value */
	oldval = iochip_regs[0][offset];
	iochip_regs[0][offset] = data;

	/* certain offsets have common effects */
	switch (offset)
	{
		case 2:
			/* Output port:
				D7: (Not connected)
				D6: (/WDC) - watchdog reset
				D5: Screen display (1= blanked, 0= displayed)
				D4-D2: (ADC2-0)
				D1: (CONT) - affects sprite hardware
				D0: Sound section reset (1= normal operation, 0= reset)
			*/
			segaic16_set_display_enable((data >> 5) & 1);
//			if ((oldval ^ data) & 2) printf("CONT = %d\n", (data >> 1) & 1);
//			if ((oldval ^ data) & 1) cpunum_set_input_line(2, INPUT_LINE_RESET, PULSE_LINE);
			break;

		case 3:
			/* Output port:
				D7: Amplifier mute control (1= sounding, 0= muted)
				D6-D0: CN D pin A17-A23 (output level 1= high, 0= low)
			*/
			break;
	}

	if (offset <= 4)
		logerror("I/O chip 0, port %c write = %02X\n", 'A' + offset, data);
}


static READ16_HANDLER( iochip_1_r )
{
	switch (offset & 7)
	{
		case 0:
			/* Input port: switches, CN D pin A1-8 (switch state 1= open, 0= closed) */
			return iochip_r(1, 0, readinputport(2));

		case 1:
			/* Input port: switches, CN D pin A9-16 (switch state 1= open, 0= closed) */
			return iochip_r(1, 1, readinputport(3));

		case 2:
			/* Input port: DIP switches (1= off, 0= on) */
			return iochip_r(1, 2, readinputport(4));

		case 3:
			/* Input port: DIP switches (1= off, 0= on) */
			return iochip_r(1, 3, readinputport(5));

		case 4:
			/* Unused */
			return iochip_r(1, 4, 0);
	}

	/* everything else returns 0 */
	return 0;
}


static WRITE16_HANDLER( iochip_1_w )
{
	UINT8 oldval;

	/* access is via the low 8 bits */
	if (!ACCESSING_LSB)
		return;
	offset &= 7;
	data &= 0xff;

	/* swap in the new value and remember the previous value */
	oldval = iochip_regs[1][offset];
	iochip_regs[1][offset] = data;

	if (offset <= 4)
		logerror("I/O chip 1, port %c write = %02X\n", 'A' + offset, data);
}


static WRITE16_HANDLER( iocontrol_w )
{
	if (ACCESSING_LSB)
	{
		logerror("I/O chip force input = %d\n", data & 1);
		iochip_force_input = data & 1;
	}
}

static struct YM2151interface ym2151_interface =
{
	1,
	4000000,
	{ YM3012_VOL(43,MIXER_PAN_LEFT,43,MIXER_PAN_RIGHT) },
	{ sound_cpu_irq }
};

static MEMORY_READ16_START( xboard_readmem )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
	{ 0x000000, 0x07ffff, MRA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(1) AM_BASE(&backupram1)
	{ 0x080000, 0x083fff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x084000, 0x087fff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x088000, 0x08bfff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x08c000, 0x08ffff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x090000, 0x093fff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x094000, 0x097fff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x098000, 0x09bfff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x09c000, 0x09ffff, SYS16IC_MRA16_BACKUPRAM1 },
	//AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(2) AM_BASE(&backupram2)
	{ 0x0a0000, 0x0a3fff, SYS16IC_MRA16_BACKUPRAM2 }, //MRA16_BackupRam2
	{ 0x0a0000, 0x0a3fff, SYS16IC_MRA16_BACKUPRAM2 },
	{ 0x0a0000, 0x0a3fff, SYS16IC_MRA16_BACKUPRAM2 },
	{ 0x0a4000, 0x0a7fff, SYS16IC_MRA16_BACKUPRAM2 },
	{ 0x0a8000, 0x0abfff, SYS16IC_MRA16_BACKUPRAM2},
	{ 0x0ac000, 0x0affff, SYS16IC_MRA16_BACKUPRAM2},
	{ 0x0b0000, 0x0b3fff, SYS16IC_MRA16_BACKUPRAM2},
	{ 0x0b4000, 0x0b7fff, SYS16IC_MRA16_BACKUPRAM2},
	{ 0x0b8000, 0x0bbfff, SYS16IC_MRA16_BACKUPRAM2},
	{ 0x0bc000, 0x0bffff, SYS16IC_MRA16_BACKUPRAM2},
	//AM_RANGE(0x0c0000, 0x0cffff) AM_READWRITE(MRA16_RAM, segaic16_tileram_0_w) AM_BASE(&segaic16_tileram_0)
	{ 0x0c0000, 0x0cffff, segaic16_tileram_r  },			/* 16 tilemaps */
	//AM_RANGE(0x0d0000, 0x0d0fff) AM_MIRROR(0x00f000) AM_READWRITE(MRA16_RAM, segaic16_textram_0_w) AM_BASE(&segaic16_textram_0)
	{ 0x0d0000, 0x0d0fff, segaic16_textram_r },
	//AM_RANGE(0x0e0000, 0x0e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_0_r, segaic16_multiply_0_w)
	{ 0x0e0000, 0x0e0007, segaic16_multiply_0_r },
	//AM_RANGE(0x0e4000, 0x0e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_0_r, segaic16_divide_0_w)
	{ 0x0e4000, 0x0e401f, segaic16_divide_0_r },
	//AM_RANGE(0x0e8000, 0x0e801f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_compare_timer_0_r, segaic16_compare_timer_0_w)
	{ 0x0e8000, 0x0e801f, segaic16_compare_timer_0_r  },
	//AM_RANGE(0x100000, 0x100fff) AM_MIRROR(0x00f000) AM_RAM AM_BASE(&segaic16_spriteram_0)
	{ 0x100000, 0x100fff, segaic16_spriteram_r },
	//AM_RANGE(0x120000, 0x123fff) AM_MIRROR(0x00c000) AM_READWRITE(MRA16_RAM, segaic16_paletteram_w) AM_BASE(&paletteram16)
	{ 0x120000, 0x123fff, MRA16_RAM },
	//AM_RANGE(0x130000, 0x13ffff) AM_READWRITE(adc_r, adc_w)
	{ 0x130000, 0x13ffff, adc_r },
	//AM_RANGE(0x140000, 0x14ffff) AM_READWRITE(iochip_0_r, iochip_0_w)
	{ 0x140000, 0x14ffff, iochip_0_r },
	//AM_RANGE(0x150000, 0x15ffff) AM_READWRITE(iochip_1_r, iochip_1_w)
	{ 0x150000, 0x15ffff, iochip_1_r },
	//AM_RANGE(0x200000, 0x27ffff) AM_ROM AM_REGION(REGION_CPU2, 0x00000)
	{ 0x200000, 0x27ffff, SYS16_CPU3ROM16_r   },		/* CPU2 ROM == CPU3 in this core  */
	//AM_RANGE(0x280000, 0x283fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x280000, 0x283fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x284000, 0x287fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x288000, 0x28bfff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x28c000, 0x28ffff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x290000, 0x293fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x294000, 0x297fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x298000, 0x29bfff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x29c000, 0x29ffff, SYS16IC_MRA16_SHAREDRAM0 },
	//AM_RANGE(0x2a0000, 0x2a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{0x2a0000, 0x2a3fff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2a4000, 0x2a7fff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2a8000, 0x2abfff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2ac000, 0x2affff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2b0000, 0x2b3fff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2b4000, 0x2b7fff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2b8000, 0x2bbfff, SYS16IC_MRA16_SHAREDRAM1 },
	{0x2bc000, 0x2bffff, SYS16IC_MRA16_SHAREDRAM1 },
	//AM_RANGE(0x2e0000, 0x2e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x2e0000, 0x2e0007, segaic16_multiply_1_r  },
	//AM_RANGE(0x2e4000, 0x2e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x2e4000, 0x2e401f, segaic16_divide_1_r },
	//	AM_RANGE(0x2e8000, 0x2e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x2e8000, 0x2e800f, segaic16_compare_timer_1_r },
	//AM_RANGE(0x2ec000, 0x2ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5) AM_BASE(&segaic16_roadram_0)
	{ 0x2ec000, 0x2ecfff, SYS16IC_MRA16_ROADRAM_SHARE },
	{ 0x2ed000, 0x2edfff, SYS16IC_MRA16_ROADRAM_SHARE },
	//AM_RANGE(0x2ee000, 0x2effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x2ee000, 0x2effff, segaic16_road_control_0_r },
	{ 0x3f8000, 0x3fbfff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0x3fc000, 0x3fffff, SYS16IC_MRA16_BACKUPRAM2 },
	{ 0xff8000, 0xffffff, SYS16IC_MRA16_WORKINGRAM1_SHARE },
MEMORY_END
/*
	AM_RANGE(0x3f8000, 0x3fbfff) AM_RAM AM_SHARE(1)
	AM_RANGE(0x3fc000, 0x3fffff) AM_RAM AM_SHARE(2)
*/
static MEMORY_WRITE16_START( xboard_writemem )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
	{ 0x000000, 0x07ffff, MWA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(1) AM_BASE(&backupram1)
	{ 0x080000, 0x083fff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x080000, 0x083fff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x084000, 0x087fff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x088000, 0x08bfff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x08c000, 0x08ffff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x090000, 0x093fff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x094000, 0x097fff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x098000, 0x09bfff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x09c000, 0x09ffff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	//AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(2) AM_BASE(&backupram2)
	{ 0x0a0000, 0x0a3fff, SYS16IC_MWA16_BACKUPRAM2,&backupram2 },
	{ 0x0a0000, 0x0a3fff, SYS16IC_MWA16_BACKUPRAM2,&backupram2 },
	{ 0x0a4000, 0x0a7fff, SYS16IC_MWA16_BACKUPRAM2,&backupram2 },
	{ 0x0a8000, 0x0abfff, SYS16IC_MWA16_BACKUPRAM2,&backupram2},
	{ 0x0ac000, 0x0affff, SYS16IC_MWA16_BACKUPRAM2,&backupram2},
	{ 0x0b0000, 0x0b3fff, SYS16IC_MWA16_BACKUPRAM2,&backupram2},
	{ 0x0b4000, 0x0b7fff, SYS16IC_MWA16_BACKUPRAM2,&backupram2},
	{ 0x0b8000, 0x0bbfff, SYS16IC_MWA16_BACKUPRAM2,&backupram2},
	{ 0x0bc000, 0x0bffff, SYS16IC_MWA16_BACKUPRAM2,&backupram2},
	//AM_RANGE(0x0c0000, 0x0cffff) AM_READWRITE(MRA16_RAM, segaic16_tileram_0_w) AM_BASE(&segaic16_tileram_0)
	{ 0x0c0000, 0x0cffff, segaic16_tileram_0_w, &segaic16_tileram_0 },
	//AM_RANGE(0x0d0000, 0x0d0fff) AM_MIRROR(0x00f000) AM_READWRITE(MRA16_RAM, segaic16_textram_0_w) AM_BASE(&segaic16_textram_0)
	{ 0x0d0000, 0x0d0fff, segaic16_textram_0_w, &segaic16_textram_0 },
	//AM_RANGE(0x0e0000, 0x0e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_0_r, segaic16_multiply_0_w)
	{ 0x0e0000, 0x0e0007, segaic16_multiply_0_w },
	//AM_RANGE(0x0e4000, 0x0e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_0_r, segaic16_divide_0_w)
	{ 0x0e4000, 0x0e401f, segaic16_divide_0_w },
	//AM_RANGE(0x0e8000, 0x0e801f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_compare_timer_0_r, segaic16_compare_timer_0_w)
	{ 0x0e8000, 0x0e801f, segaic16_compare_timer_0_w },
	//AM_RANGE(0x100000, 0x100fff) AM_MIRROR(0x00f000) AM_RAM AM_BASE(&segaic16_spriteram_0)
	{ 0x100000, 0x100fff, SYS16_MWA16_SPRITERAM, &segaic16_spriteram_0  },
	//AM_RANGE(0x110000, 0x11ffff) AM_WRITE(segaic16_sprites_draw_0_w)
	{ 0x110000, 0x11ffff, segaic16_sprites_draw_0_w},
	//AM_RANGE(0x120000, 0x123fff) AM_MIRROR(0x00c000) AM_READWRITE(MRA16_RAM, segaic16_paletteram_w) AM_BASE(&paletteram16)
	{ 0x120000, 0x123fff, segaic16_paletteram_w, &paletteram16  },
	//AM_RANGE(0x130000, 0x13ffff) AM_READWRITE(adc_r, adc_w)
	{ 0x130000, 0x13ffff, adc_w },
	//AM_RANGE(0x140000, 0x14ffff) AM_READWRITE(iochip_0_r, iochip_0_w)
	{ 0x140000, 0x14ffff, iochip_0_w },
	//AM_RANGE(0x150000, 0x15ffff) AM_READWRITE(iochip_1_r, iochip_1_w)
	{ 0x150000, 0x15ffff, iochip_1_w },
	//AM_RANGE(0x160000, 0x16ffff) AM_WRITE(iocontrol_w)
	{ 0x160000, 0x16ffff, iocontrol_w},
	//AM_RANGE(0x200000, 0x27ffff) AM_ROM AM_REGION(REGION_CPU2, 0x00000)

	//AM_RANGE(0x280000, 0x283fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x280000, 0x283fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x284000, 0x287fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x288000, 0x28bfff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x28c000, 0x28ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x290000, 0x293fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x294000, 0x297fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x298000, 0x29bfff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x29c000, 0x29ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	//AM_RANGE(0x2a0000, 0x2a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{0x2a0000, 0x2a3fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2a4000, 0x2a7fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2a8000, 0x2abfff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2ac000, 0x2affff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2b0000, 0x2b3fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2b4000, 0x2b7fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2b8000, 0x2bbfff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	{0x2bc000, 0x2bffff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	//AM_RANGE(0x2e0000, 0x2e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x2e0000, 0x2e0007, segaic16_multiply_1_w },
	//AM_RANGE(0x2e4000, 0x2e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x2e4000, 0x2e401f, segaic16_divide_1_w },
	//AM_RANGE(0x2e8000, 0x2e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x2e8000, 0x2e800f, segaic16_compare_timer_1_w  },		/* includes sound latch! */
	//AM_RANGE(0x2ec000, 0x2ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5) AM_BASE(&segaic16_roadram_0)
	{0x2ec000, 0x2ecfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	{0x2ed000, 0x2edfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	//AM_RANGE(0x2ee000, 0x2effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x2ee000, 0x2effff, segaic16_road_control_0_w },
	{ 0xff8000, 0xffffff, SYS16IC_MWA16_WORKINGRAM1_SHARE, &segaic16_workingram1 }, /*need to inspect the memory map not sure where this is comming from*/
	{ 0x3f8000, 0x3fbfff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	{ 0x3fc000, 0x3fffff, SYS16IC_MWA16_BACKUPRAM2,&backupram2 },
	
MEMORY_END

static MEMORY_READ16_START( xboard_readmem2 )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
	{ 0x000000, 0x07ffff, MRA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x080000, 0x083fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x080000, 0x083fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x084000, 0x087fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x088000, 0x08bfff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x08c000, 0x08ffff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x090000, 0x093fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x094000, 0x097fff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x098000, 0x09bfff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x09c000, 0x09ffff, SYS16IC_MRA16_SHAREDRAM0 },
//	AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{ 0x0a0000, 0x0a3fff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0a0000, 0x0a3fff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0a4000, 0x0a7fff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0a8000, 0x0abfff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0ac000, 0x0affff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0b0000, 0x0b3fff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0b4000, 0x0b7fff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0b8000, 0x0bbfff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x0bc000, 0x0bffff, SYS16IC_MRA16_SHAREDRAM1 },
	//AM_RANGE(0x0e0000, 0x0e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x0e0000, 0x0e0007, segaic16_multiply_1_r },
	//AM_RANGE(0x0e4000, 0x0e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x0e4000, 0x0e401f, segaic16_divide_1_r },
	//AM_RANGE(0x0e8000, 0x0e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x0e8000, 0x0e800f, segaic16_compare_timer_1_r },
	//AM_RANGE(0x0ec000, 0x0ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5)
	{ 0x0ec000, 0x0ecfff, SYS16IC_MRA16_ROADRAM_SHARE },
	{ 0x0ed000, 0x0edfff, SYS16IC_MRA16_ROADRAM_SHARE },
	//AM_RANGE(0x0ee000, 0x0effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x0ee000, 0x0effff, segaic16_road_control_0_r },
	{ 0x200000, 0x27ffff, SYS16_CPU3ROM16_r   },
	{ 0x29c000, 0x29ffff, SYS16IC_MRA16_BACKUPRAM1 },
MEMORY_END
/*
static ADDRESS_MAP_START( sub_map, ADDRESS_SPACE_PROGRAM, 16 )
	ADDRESS_MAP_FLAGS( AMEF_UNMAP(1) | AMEF_ABITS(20) )
//	AM_RANGE(0x0f0000, 0x0f3fff) AM_READWRITE(excs_r, excs_w)
ADDRESS_MAP_END
*/
static MEMORY_WRITE16_START( xboard_writemem2 )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
	{ 0x000000, 0x07ffff, MWA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x080000, 0x083fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x080000, 0x083fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x084000, 0x087fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x088000, 0x08bfff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x08c000, 0x08ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x090000, 0x093fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x094000, 0x097fff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x098000, 0x09bfff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	{ 0x09c000, 0x09ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	//AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{ 0x0a0000, 0x0a3fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0a0000, 0x0a3fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0a4000, 0x0a7fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0a8000, 0x0abfff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0ac000, 0x0affff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0b0000, 0x0b3fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0b4000, 0x0b7fff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0b8000, 0x0bbfff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x0bc000, 0x0bffff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	//AM_RANGE(0x0e0000, 0x0e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x0e0000, 0x0e0007, segaic16_multiply_1_w  },
	//AM_RANGE(0x0e4000, 0x0e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x0e4000, 0x0e401f, segaic16_divide_1_w },
	//AM_RANGE(0x0e8000, 0x0e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x0e8000, 0x0e800f, segaic16_compare_timer_1_w  },
	//AM_RANGE(0x0ec000, 0x0ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5)
	{ 0x0ec000, 0x0ecfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	{ 0x0ed000, 0x0edfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	//AM_RANGE(0x0ee000, 0x0effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x0ee000, 0x0effff, segaic16_road_control_0_w  },
	{ 0x29c000, 0x29ffff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
MEMORY_END
/*
 * missing some mirror or something from cpu2 will need to dig in when i have more time.
 
[libretro DEBUG] [MAME 2003+] cpu #0 (PC=00000492): unmapped memory word write to 002F0006 = 0080 & 00FF
[libretro DEBUG] [MAME 2003+] cpu #0 (PC=0000049A): unmapped memory word write to 002F0000 = 00FF & 00FF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000009BC): unmapped memory word read from 0029C0F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000009BC): unmapped memory word write to 0029C0F0 = 0001 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=00000EA8): unmapped memory word read from 0029C0F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=00000EA8): unmapped memory word write to 0029C0F0 = 0001 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000137E): unmapped memory word read from 0029C0F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000137E): unmapped memory word write to 0029C0F0 = 0001 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000015A0): unmapped memory word write to 0029C040 = 0054 & 00FF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000015A0): unmapped memory word write to 0029C040 = 4D00 & FF00
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000015A0): unmapped memory word write to 0029C042 = 004D & 00FF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000015A0): unmapped memory word write to 0029C042 = 0054 & 00FF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000015A0): unmapped memory word write to 0029C042 = 4D00 & FF00
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000015A0): unmapped memory word write to 0029C042 = 5600 & FF00
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000307C): unmapped memory word read from 0029E110 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3B4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3B6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3D4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3D6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3D8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3DA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3E0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3E2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3E8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3EA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3EC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3EE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A3FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A40C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A40E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A414 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A416 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A41C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A41E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A428 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A42A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A42C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A42E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A434 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A436 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A438 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A43A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A43C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A43E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A440 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A442 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A448 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A44A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A450 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A452 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A458 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A45A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A45C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A45E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A460 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A462 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A468 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A46A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A470 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A472 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A47C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A47E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A488 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A48A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A490 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A492 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A498 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A49A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A49C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A49E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4A0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4A2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4A8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4AA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4BC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4BE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4E8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4EA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4F8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4FA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A4FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A500 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A502 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A508 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A50A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A510 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A512 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A518 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A51A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A51C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A51E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A520 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A522 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A524 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A526 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A530 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A532 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A53C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A53E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A540 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A542 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A544 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A546 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A548 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A54A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A550 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A552 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A558 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A55A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A560 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A562 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A564 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A566 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A568 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A56A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A56C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A56E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A570 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A572 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A574 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A576 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A578 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A57A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A57C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A57E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A580 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A582 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A584 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A586 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A588 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A58A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A58C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A58E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A590 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A592 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A594 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A596 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A598 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A59A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A59C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A59E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5A0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5A2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5A4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5A6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5A8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5AA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5AC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5AE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5BC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5BE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5C4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5C6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5CC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5CE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5D8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5DA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5E0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5E2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5E4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5E6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5EC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5EE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A5FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A600 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A602 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A604 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A606 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A608 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A60A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A610 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A612 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A618 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A61A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A620 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A622 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A628 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A62A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A630 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A632 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A638 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A63A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A63C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A63E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A640 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A642 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A644 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A646 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A650 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A652 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A658 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A65A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A65C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A65E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A660 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A662 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A664 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A666 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A668 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A66A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A66C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A66E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A670 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A672 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A674 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A676 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A678 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A67A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A67C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A67E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A680 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A682 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A684 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A686 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A688 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A68A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A68C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A68E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A690 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A692 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A694 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A696 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A698 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A69A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A69C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A69E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6A0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6A2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6A4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6A6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6A8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6AA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6AC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6AE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6B4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6B6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6BC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6BE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6C4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6C6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6CC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6CE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6D4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6D6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6D8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6DA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6E0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6E2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6E4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6E6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6E8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6EA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6EC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6EE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6F4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6F6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6F8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6FA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A6FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A700 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A702 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A704 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A706 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A708 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A70A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A70C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A70E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A710 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A712 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A714 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A716 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A718 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A71A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A71C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A71E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A720 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A722 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A724 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A726 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A728 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A72A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A730 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A732 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A734 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A736 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A738 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A73A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A73C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A73E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A740 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A742 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A744 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A746 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A748 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A74A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A750 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A752 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A754 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A756 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A758 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A75A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A75C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A75E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A764 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A766 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A768 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A76A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A76C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A76E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A770 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A772 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A774 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A776 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A778 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A77A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A77C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A77E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A780 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A782 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A784 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A786 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A788 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A78A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A78C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A78E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A790 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A792 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A794 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A796 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A798 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A79A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7A0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7A2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7A4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7A6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7A8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7AA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7AC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7AE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7B4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7B6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7BC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7BE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7C4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7C6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7CC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7CE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7D4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7D6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7D8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7DA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7E0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7E2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7E8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7EA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7EC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7EE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7F4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7F6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7F8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7FA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A7FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A800 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A802 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A804 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A806 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A808 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A80A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A80C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A80E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A810 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A812 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A814 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A816 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A818 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A81A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A81C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A81E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A824 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A826 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A828 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A82A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A82C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A82E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A830 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A832 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A834 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A836 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A838 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A83A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A83C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A83E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A840 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A842 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A844 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A846 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A848 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A84A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A84C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A84E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A850 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A852 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A854 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A856 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A858 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A85A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A85C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A85E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A860 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A862 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A864 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A866 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A868 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A86A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A86C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A86E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A870 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A872 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A874 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A876 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A878 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A87A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A87C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A87E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A880 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A882 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A884 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A886 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A888 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A88A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A88C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A88E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A890 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A892 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A894 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A896 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A898 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A89A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A89C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A89E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8A0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8A2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8A4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8A6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8A8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8AA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8AC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8AE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8B4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8B6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8BC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8BE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8C4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8C6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8CC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8CE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8D4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8D6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8D8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8DA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8E0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8E2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8E4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8E6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8E8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8EA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8EC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8EE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8F4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8F6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8F8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8FA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A8FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A900 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A902 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A904 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A906 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A908 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A90A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A90C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A90E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A910 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A912 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A914 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A916 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A918 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A91A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A91C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A91E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A920 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A922 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A924 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A926 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A928 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A92A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A92C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A92E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A930 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A932 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A934 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A936 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A938 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A93A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A93C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A93E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A940 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A942 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A944 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A946 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A948 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A94A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A94C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A94E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A954 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A956 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A958 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A95A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A95C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A95E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A960 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A962 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A964 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A966 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A968 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A96A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A96C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A96E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A970 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A972 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A974 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A976 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A978 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A97A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A97C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A97E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A984 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A986 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A988 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A98A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A98C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A98E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A990 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A992 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A994 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A996 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A998 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A99A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A99C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A99E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9A0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9A2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9A4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9A6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9A8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9AA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9AC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9AE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9B0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9B2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9B4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9B6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9B8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9BA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9BC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9BE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9C0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9C2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9C4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9C6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9C8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9CA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9CC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9CE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9D0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9D2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9D4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9D6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9D8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9DA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9DC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9DE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9E0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9E2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9E4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9E6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9E8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9EA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9EC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9EE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9F2 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9F4 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9F6 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9F8 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9FA & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9FC & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024A9FE & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA00 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA02 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA04 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA06 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA08 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA0A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA0C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA0E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA14 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA16 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA18 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA1A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA1C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA1E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA20 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA22 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA24 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA26 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA28 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA2A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA2C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA2E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA30 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA32 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA34 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA36 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA38 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA3A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA3C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA3E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA40 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA42 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA44 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA46 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA48 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA4A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA4C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA4E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA50 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA52 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA54 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA56 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA58 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA5A & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA5C & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA5E & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA60 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=000092DE): unmapped memory word read from 0024AA62 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000933E): unmapped memory word read from 00254000 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000933E): unmapped memory word read from 00255000 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000933E): unmapped memory word read from 00256000 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000933E): unmapped memory word read from 00259000 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000933E): unmapped memory word read from 0025B000 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=0000933E): unmapped memory word read from 0025C000 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=00009444): unmapped memory word read from 0029C0F0 & FFFF
[libretro DEBUG] [MAME 2003+] cpu #2 (PC=00009444): unmapped memory word write to 0029C0F0 = 0001 & FFFF
*/

static MEMORY_READ_START( aburner_sound_readmem )
    { 0x0000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xf0ff, SegaPCM_r },
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( aburner_sound_writemem )
    { 0x0000, 0x7fff, MWA_ROM },
	{ 0xf000, 0xf0ff, SegaPCM_w },
	{ 0xf000, 0xffff, MWA_RAM },
MEMORY_END

static PORT_READ_START( aburner_sound_readport )
    { 0x01, 0x01, YM2151_status_port_0_r },
	{ 0x40, 0x40, soundlatch_r },
PORT_END

static PORT_WRITE_START( aburner_sound_writeport )
    { 0x00, 0x00, YM2151_register_port_0_w },
	{ 0x01, 0x01, YM2151_data_port_0_w },
PORT_END



/*************************************
 *
 *	Generic port definitions
 *
 *************************************/

/*
	aburner chip 0, port A: motor status (R)
			chip 0, port B: motor power (W)
			chip 0, port C: unknown (W)
			chip 0, port D: lamp (W)
			chip 1, port A: buttons
			chip 1, port B: ---
			chip 2, port C: DIPs
			chip 3, port D: DIPs
*/

	INPUT_PORTS_START( thndrbld )
	PORT_START//_TAG("IO0PORTA")
	PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SPECIAL )	/* /INTR of ADC0804 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START//_TAG("IO0PORTB")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START//_TAG("IO1PORTA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START//_TAG("IO1PORTB")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START//_TAG("IO1PORTC")
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x05, "6 Coins/4 Credits" )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, "5 Coins/6 Credits" )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin B too) or 1/1" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x50, "6 Coins/4 Credits" )
	PORT_DIPSETTING(    0x40, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, "5 Coins/6 Credits" )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin A too) or 1/1" )

	PORT_START//_TAG("IO1PORTD")
	PORT_DIPNAME( 0x01, 0x01, "Cabinet Type" )
	PORT_DIPSETTING(    0x01, "Econ Upright" )
	PORT_DIPSETTING(    0x00, "Mini Upright" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Time" )
	PORT_DIPSETTING(    0x04, "30 sec" )
	PORT_DIPSETTING(    0x00, "0 sec" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, "Number of Ships" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x18, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Allow_Continue ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START//_TAG("ADC0")	/* stick X */
//	PORT_BIT(    0xff, 0x7f,  IPT_AD_STICK_X ) PORT_MINMAX(0x00,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(4) PORT_REVERSE
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_X | IPF_REVERSE, 100, 4, 0x00, 0xff )

	PORT_START//_TAG("ADC1")	/* "slottle" */
//	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Z ) PORT_MINMAX(0x00,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(79)
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Z, 100, 79, 0x00, 0xff )

	PORT_START//_TAG("ADC2")	/* stick Y */
//	PORT_BIT( 0xff, 0x7f, IPT_AD_STICK_Y ) PORT_MINMAX(0x00,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(4)
	PORT_ANALOG( 0xff, 0x7f, IPT_AD_STICK_Y, 100, 4, 0x00, 0xff )
INPUT_PORTS_END


/*****************************************************************************/
/* Line of Fire */
ROM_START( loffire )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-12849.58", 0x000000, 0x20000, CRC(dfd1ab45) SHA1(dac358b6f50999deaed422578c2dcdfb492c81c9) )
	ROM_LOAD16_BYTE( "bootleg_epr-12850.63", 0x000001, 0x20000, CRC(90889ae9) SHA1(254f8934e8a0329e28a38c71c4bd628ef7237ca8) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "opr-12791.154", 0x00000, 0x10000, CRC(acfa69ba) SHA1(353c43dda6c2282a785646b0a58c90cfd173cd7b) )
	ROM_LOAD( "opr-12792.153", 0x10000, 0x10000, CRC(e506723c) SHA1(d04dc29686fe348f8f715d14c027de0e508c770f) )
	ROM_LOAD( "opr-12793.152", 0x20000, 0x10000, CRC(0ce8cce3) SHA1(1a6b1af2b0b9e8240e681f7b15e9d08595753fe6) )

	ROM_REGION32_LE( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD32_BYTE( "epr-12787.90",  0x000000, 0x20000, CRC(6431a3a6) SHA1(63a732b7dfd2b83fe7684d47fea26063c4ece099) )
	ROM_LOAD32_BYTE( "epr-12788.94",  0x000001, 0x20000, CRC(1982a0ce) SHA1(e4756f31b0094e0e9ddb2df53a5c938ac5559230) )
	ROM_LOAD32_BYTE( "epr-12789.98",  0x000002, 0x20000, CRC(97d03274) SHA1(b4b9921db53949bc8e91f8a2992e89c172fe8893) )
	ROM_LOAD32_BYTE( "epr-12790.102", 0x000003, 0x20000, CRC(816e76e6) SHA1(34d2a662af96f40f40a77497cbc0a3374fe9a34f) )
	ROM_LOAD32_BYTE( "epr-12783.91",  0x080000, 0x20000, CRC(c13feea9) SHA1(c0c3097903079deec22b0f8de76927f7570ac0f6) )
	ROM_LOAD32_BYTE( "epr-12784.95",  0x080001, 0x20000, CRC(39b94c65) SHA1(4deae3bf7bb4e04b011d23292a0c68471758e7ec) )
	ROM_LOAD32_BYTE( "epr-12785.99",  0x080002, 0x20000, CRC(05ed0059) SHA1(b7404a0f4f15ffdbd08673683cea22340de3f5f9) )
	ROM_LOAD32_BYTE( "epr-12786.103", 0x080003, 0x20000, CRC(a4123165) SHA1(024597dcfbd3be932626b84dbd6e7d38a7a0195d) )
	ROM_LOAD32_BYTE( "epr-12779.92",  0x100000, 0x20000, CRC(ae58af7c) SHA1(8c57f2d0b6584dd606afc5ecff039479e5068420) )
	ROM_LOAD32_BYTE( "epr-12780.96",  0x100001, 0x20000, CRC(ee670c1e) SHA1(8a9e0808d40e210abf6c49ef5c0774d8c0d6602b) )
	ROM_LOAD32_BYTE( "epr-12781.100", 0x100002, 0x20000, CRC(538f6bc5) SHA1(4f294ef0aa9c7e2ac7e92518d938f0870f2e46d1) )
	ROM_LOAD32_BYTE( "epr-12782.104", 0x100003, 0x20000, CRC(5acc34f7) SHA1(ef27ab818f50e59a122b9fc65b13442d9fee307c) )
	ROM_LOAD32_BYTE( "epr-12775.93",  0x180000, 0x20000, CRC(693056ec) SHA1(82d10d960441811b9369295bbb60fa7bfc5457a3) )
	ROM_LOAD32_BYTE( "epr-12776.97",  0x180001, 0x20000, CRC(61efbdfd) SHA1(67f267e0673c64ce77669826ea1d11cb79d0ccc1) )
	ROM_LOAD32_BYTE( "epr-12777.101", 0x180002, 0x20000, CRC(29d5b953) SHA1(0c932a67e2aecffa7a1dbaa587c96214e1a2cc7f) )
	ROM_LOAD32_BYTE( "epr-12778.105", 0x180003, 0x20000, CRC(2fb68e07) SHA1(8685e72aed115cbc9c6c7511217996a573b30d16) )


	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_ERASE00 ) /* ???? */
	// none

	ROM_REGION( 0x68000, REGION_SOUND1, 0 ) /* Sega PCM sound data */
	ROM_LOAD( "epr-12799.11", 0x00000, 0x20000, CRC(bc60181c) SHA1(3c89161348db7cafb5636ab4eaba91fbd3541f90) )
	ROM_LOAD( "epr-12800.12", 0x20000, 0x20000, CRC(1158c1a3) SHA1(e1d664a203eed5a0130b39ced7bea8328f06f107) )
	ROM_LOAD( "epr-12801.13", 0x40000, 0x20000, CRC(2d6567c4) SHA1(542be9d8e91cf2df18d95f4e259cfda0560697cb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr-12798.17", 0x00000, 0x10000, CRC(0587738d) SHA1(24c79b0c73616d5532a49a2c9121dfabe3a80c7d) )

	ROM_REGION( 0x100000, REGION_CPU3, 0 ) /* 2nd 68000 code */
	ROM_LOAD16_BYTE( "epr-12804.20", 0x000000, 0x20000, CRC(b853480e) SHA1(de0889e99251da7ea50316282ebf6f434cc2db11) )
	ROM_LOAD16_BYTE( "epr-12805.29", 0x000001, 0x20000, CRC(4a7200c3) SHA1(3e6febed36a55438e0d24441b68f2b7952791584) )
	ROM_LOAD16_BYTE( "epr-12802.21", 0x040000, 0x20000, CRC(d746bb39) SHA1(08dc8cf565997c7e52329961bf7a229a15900cff) )
	ROM_LOAD16_BYTE( "epr-12803.30", 0x040001, 0x20000, CRC(c1d9e751) SHA1(98b3d0b3b31702f6234b5fea2b82d512fc5d3ad2) )
ROM_END

/*****************************************************************************/
/* Thunder Blade*/
ROM_START( thndrbld )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-11405.ic58", 0x000000, 0x20000, CRC(1642fd59) SHA1(92b95d97b1eef770983c993d357e06ecf6a2b29c) )
	ROM_LOAD16_BYTE( "bootleg_epr-11406.ic63", 0x000001, 0x20000, CRC(aa87dd75) SHA1(4c61dfef69a68d9cab8fed0d2cbb28b751319049) )
	ROM_LOAD16_BYTE( "epr-11306.ic57", 0x040000, 0x20000, CRC(4b95f2b4) SHA1(9e0ff898a2af05c35db3551e52c7485748698c28) )
	ROM_LOAD16_BYTE( "epr-11307.ic62", 0x040001, 0x20000, CRC(2d6833e4) SHA1(b39a744370014237121f0010d18897e63f7058cf) )


	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr-11314.ic154", 0x00000, 0x10000, CRC(d4f954a9) SHA1(93ee8cf8fcf4e1d0dd58329bba9b594431193449) )
	ROM_LOAD( "epr-11315.ic153", 0x10000, 0x10000, CRC(35813088) SHA1(ea1ec982d1509efb26e7b6a150825a6a905efed9) )
	ROM_LOAD( "epr-11316.ic152", 0x20000, 0x10000, CRC(84290dff) SHA1(c13fb6ef12a991f79a95072f953a02b5c992aa2d) )

	ROM_REGION32_LE( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD32_BYTE( "epr-11323.ic90",  0x000000, 0x20000, CRC(27e40735) SHA1(284ddb88efe741fb78199ea619c9b230ee689803) )
	ROM_LOAD32_BYTE( "epr-11322.ic94",  0x000001, 0x20000, CRC(10364d74) SHA1(393b19a972b5d8817ffd438f13ded73cd58ebe56) )
	ROM_LOAD32_BYTE( "epr-11321.ic98",  0x000002, 0x20000, CRC(8e738f58) SHA1(9f2dceebf01e582cf60f072ae411000d8503894b) )
	ROM_LOAD32_BYTE( "epr-11320.ic102", 0x000003, 0x20000, CRC(a95c76b8) SHA1(cda62f3c25b9414a523c2fc5d109031ed560069e) )
	ROM_LOAD32_BYTE( "epr-11327.ic91",  0x080000, 0x20000, CRC(deae90f1) SHA1(c73c23bab949041242302cec13d653dcc71bb944) )
	ROM_LOAD32_BYTE( "epr-11326.ic95",  0x080001, 0x20000, CRC(29198403) SHA1(3ecf315a0e6b3ed5005f8bdcb2e2a884c8b176c7) )
	ROM_LOAD32_BYTE( "epr-11325.ic99",  0x080002, 0x20000, CRC(b9e98ae9) SHA1(c4932e2590b10d54fa8ded94593dc4203fccc60d) )
	ROM_LOAD32_BYTE( "epr-11324.ic103", 0x080003, 0x20000, CRC(9742b552) SHA1(922032264d469e943dfbcaaf57464efc638fcf73) )
	ROM_LOAD32_BYTE( "epr-11331.ic92",  0x100000, 0x20000, CRC(3a2c042e) SHA1(c296ff222d156d3bdcb42bef321831f502830fd6) )
	ROM_LOAD32_BYTE( "epr-11330.ic96",  0x100001, 0x20000, CRC(aa7c70c5) SHA1(b6fea17392b7821b8b3bba78002f9c1604f09edc) )
	ROM_LOAD32_BYTE( "epr-11329.ic100", 0x100002, 0x20000, CRC(31b20257) SHA1(7ce10a94bce67b2d15d7b576b0f7d47389dc8948) )
	ROM_LOAD32_BYTE( "epr-11328.ic104", 0x100003, 0x20000, CRC(da39e89c) SHA1(526549ce9112754c82743552eeebec63fe7ad968) )
	ROM_LOAD32_BYTE( "epr-11395.ic93",  0x180000, 0x20000, CRC(90775579) SHA1(15a86071a105da40ec9c0c0074e342231fc030d0) ) //
	ROM_LOAD32_BYTE( "epr-11394.ic97",  0x180001, 0x20000, CRC(5f2783be) SHA1(424510153a91902901f321f39738a862d6fba8e7) ) // different numbers?
	ROM_LOAD32_BYTE( "epr-11393.ic101", 0x180002, 0x20000, CRC(525e2e1d) SHA1(6fd09f775e7e6cad8078513d1af0a8ff40fb1360) ) // replaced from original rev?
	ROM_LOAD32_BYTE( "epr-11392.ic105", 0x180003, 0x20000, CRC(b4a382f7) SHA1(c03a05ba521f654db1a9c5f5717b7a15e5a29d4e) ) //



	ROM_REGION( 0x10000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD( "epr-11396.ic17", 0x00000, 0x10000, CRC(d37b54a4) SHA1(c230fe7241a1f13ca13506d1492f348f506c40a7) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 ) /* Sega PCM sound data */
	ROM_LOAD( "epr-11317.ic11", 0x00000, 0x20000, CRC(d4e7ac1f) SHA1(ec5d6e4949938adf56e5613801ae56ff2c3dede5) )
	ROM_LOAD( "epr-11318.ic12", 0x20000, 0x20000, CRC(70d3f02c) SHA1(391aac2bc5673e06150de27e19c7c6359da8ca82) )
	ROM_LOAD( "epr-11319.ic13", 0x40000, 0x20000, CRC(50d9242e) SHA1(a106371bf680c3088ec61f07fc5c4ce467973c15) )

	ROM_REGION( 0x100000, REGION_CPU3, 0 ) /* 2nd 68000 code */
	ROM_LOAD16_BYTE( "epr-11390.ic20", 0x000000, 0x20000, CRC(ed988fdb) SHA1(b809b0b7dabd5cb29f5387522c6dfb993d1d0271) )
	ROM_LOAD16_BYTE( "epr-11391.ic29", 0x000001, 0x20000, CRC(12523bc1) SHA1(54635d6c4cc97cf4148dcac3bb2056fc414252f7) )
	ROM_LOAD16_BYTE( "epr-11310.ic21", 0x040000, 0x20000, CRC(5d9fa02c) SHA1(0ca71e35cf9740e38a52960f7d1ef96e7e1dda94) )
	ROM_LOAD16_BYTE( "epr-11311.ic30", 0x040001, 0x20000, CRC(483de21b) SHA1(871f0e856dcc81dcef1d9846261b3c011fa26dde) )
	ROM_REGION( 0x10000, REGION_GFX3, 0 ) /* ???? */
	ROM_LOAD( "epr-11313.ic29", 0x00000, 0x10000, CRC(6a56c4c3) SHA1(c1b8023cb2ba4e96be052031c24b6ae424225c71) )
ROM_END

/* Thunder Blade Japan*/
ROM_START( thndrbld1 )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) // 68000 code
	ROM_LOAD16_BYTE( "epr-11304.ic58", 0x000000, 0x20000, CRC(a90630ef) SHA1(8f29e020119b2243b1c95e15546af1773327ae85) ) // patched?
	ROM_LOAD16_BYTE( "epr-11305.ic63", 0x000001, 0x20000, CRC(9ba3ef61) SHA1(f75748b37ce35b0ef881804f73417643068dfbb2) ) // patched?
	ROM_LOAD16_BYTE( "epr-11306.ic57", 0x040000, 0x20000, CRC(4b95f2b4) SHA1(9e0ff898a2af05c35db3551e52c7485748698c28) )
	ROM_LOAD16_BYTE( "epr-11307.ic62", 0x040001, 0x20000, CRC(2d6833e4) SHA1(b39a744370014237121f0010d18897e63f7058cf) )

	ROM_REGION( 0x80000, REGION_CPU3, 0 ) // 2nd 68000 code
	ROM_LOAD16_BYTE( "epr-11308.ic20", 0x00000, 0x20000, CRC(7956c238) SHA1(4608225cfd6ea3d38317cbe970f26a5fc2f8e320) )
	ROM_LOAD16_BYTE( "epr-11309.ic29", 0x00001, 0x20000, CRC(c887f620) SHA1(644c47cc2cf75cbe489ea084c13c59d94631e83f) )
	ROM_LOAD16_BYTE( "epr-11310.ic21", 0x040000, 0x20000, CRC(5d9fa02c) SHA1(0ca71e35cf9740e38a52960f7d1ef96e7e1dda94) )
	ROM_LOAD16_BYTE( "epr-11311.ic30", 0x040001, 0x20000, CRC(483de21b) SHA1(871f0e856dcc81dcef1d9846261b3c011fa26dde) )

	ROM_REGION( 0x30000, REGION_GFX1, 0 ) // tiles
	ROM_LOAD( "epr-11314.ic154", 0x00000, 0x10000, CRC(d4f954a9) SHA1(93ee8cf8fcf4e1d0dd58329bba9b594431193449) )
	ROM_LOAD( "epr-11315.ic153", 0x10000, 0x10000, CRC(35813088) SHA1(ea1ec982d1509efb26e7b6a150825a6a905efed9) )
	ROM_LOAD( "epr-11316.ic152", 0x20000, 0x10000, CRC(84290dff) SHA1(c13fb6ef12a991f79a95072f953a02b5c992aa2d) )

	ROM_REGION32_LE( 0x200000, REGION_GFX2, 0 ) // sprites
	ROM_LOAD32_BYTE( "epr-11323.ic90",  0x000000, 0x20000, CRC(27e40735) SHA1(284ddb88efe741fb78199ea619c9b230ee689803) )
	ROM_LOAD32_BYTE( "epr-11322.ic94",  0x000001, 0x20000, CRC(10364d74) SHA1(393b19a972b5d8817ffd438f13ded73cd58ebe56) )
	ROM_LOAD32_BYTE( "epr-11321.ic98",  0x000002, 0x20000, CRC(8e738f58) SHA1(9f2dceebf01e582cf60f072ae411000d8503894b) )
	ROM_LOAD32_BYTE( "epr-11320.ic102", 0x000003, 0x20000, CRC(a95c76b8) SHA1(cda62f3c25b9414a523c2fc5d109031ed560069e) )
	ROM_LOAD32_BYTE( "epr-11327.ic91",  0x080000, 0x20000, CRC(deae90f1) SHA1(c73c23bab949041242302cec13d653dcc71bb944) )
	ROM_LOAD32_BYTE( "epr-11326.ic95",  0x080001, 0x20000, CRC(29198403) SHA1(3ecf315a0e6b3ed5005f8bdcb2e2a884c8b176c7) )
	ROM_LOAD32_BYTE( "epr-11325.ic99",  0x080002, 0x20000, CRC(b9e98ae9) SHA1(c4932e2590b10d54fa8ded94593dc4203fccc60d) )
	ROM_LOAD32_BYTE( "epr-11324.ic103", 0x080003, 0x20000, CRC(9742b552) SHA1(922032264d469e943dfbcaaf57464efc638fcf73) )
	ROM_LOAD32_BYTE( "epr-11331.ic92",  0x100000, 0x20000, CRC(3a2c042e) SHA1(c296ff222d156d3bdcb42bef321831f502830fd6) )
	ROM_LOAD32_BYTE( "epr-11330.ic96",  0x100001, 0x20000, CRC(aa7c70c5) SHA1(b6fea17392b7821b8b3bba78002f9c1604f09edc) )
	ROM_LOAD32_BYTE( "epr-11329.ic100", 0x100002, 0x20000, CRC(31b20257) SHA1(7ce10a94bce67b2d15d7b576b0f7d47389dc8948) )
	ROM_LOAD32_BYTE( "epr-11328.ic104", 0x100003, 0x20000, CRC(da39e89c) SHA1(526549ce9112754c82743552eeebec63fe7ad968) )
	ROM_LOAD32_BYTE( "epr-11335.ic93",  0x180000, 0x20000, CRC(f19b3e86) SHA1(40e8ba10cd5020782b82279974d13330a9c015e5) )
	ROM_LOAD32_BYTE( "epr-11334.ic97",  0x180001, 0x20000, CRC(348f91c7) SHA1(03da6a4fee1fdea76058be4bc5ffcde7a79e5948) )
	ROM_LOAD32_BYTE( "epr-11333.ic101", 0x180002, 0x20000, CRC(05a2333f) SHA1(70f213945fa7fe056fe17a02558638e87f2c001e) )
	ROM_LOAD32_BYTE( "epr-11332.ic105", 0x180003, 0x20000, CRC(dc089ec6) SHA1(d72390c45138a507e79af112addbc015560fc248) )

	ROM_REGION( 0x10000, REGION_GFX3, ROMREGION_ERASE00 ) // Road Data
	ROM_LOAD( "epr-11313.ic29", 0x00000, 0x10000, CRC(6a56c4c3) SHA1(c1b8023cb2ba4e96be052031c24b6ae424225c71) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 ) // sound CPU
	ROM_LOAD( "epr-11312.ic17",   0x00000, 0x10000, CRC(3b974ed2) SHA1(cf18a2d0f01643c747a884bf00e5b7037ba2e64a) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_ERASEFF ) // Sega PCM sound data
	ROM_LOAD( "epr-11317.ic11", 0x00000, 0x20000, CRC(d4e7ac1f) SHA1(ec5d6e4949938adf56e5613801ae56ff2c3dede5) )
	ROM_LOAD( "epr-11318.ic12", 0x20000, 0x20000, CRC(70d3f02c) SHA1(391aac2bc5673e06150de27e19c7c6359da8ca82) )
	ROM_LOAD( "epr-11319.ic13", 0x40000, 0x20000, CRC(50d9242e) SHA1(a106371bf680c3088ec61f07fc5c4ce467973c15) )
ROM_END

static MACHINE_INIT( xboard ){

	cpu_set_irq_callback(0, main_irq_callback);
	segaic16_compare_timer_init(0, sound_data_w, timer_ack_callback);
	segaic16_compare_timer_init(1, NULL, NULL);
	timer_set(cpu_getscanlinetime(1), 1, scanline_callback);

	memset(iochip_custom_io_r, 0, sizeof(iochip_custom_io_r));
	memset(iochip_custom_io_w, 0, sizeof(iochip_custom_io_w));
	memset(adc_reverse, 0, sizeof(adc_reverse));

	segaic16_tilemap_reset(0);
	xboard_set_road_priority(1);
}


static DRIVER_INIT( thndrbld ){
//segaic16_set_display_enable(1); // this shouldnt be here move to io


}


static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,3),
	3,
	{ RGN_FRAC(2,3), RGN_FRAC(1,3), RGN_FRAC(0,3) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};


static struct GfxDecodeInfo newgfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,	0, 1024 },
	{ -1 }
};

static MACHINE_DRIVER_START( xboard )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000, 12500000)
	MDRV_CPU_MEMORY(xboard_readmem, xboard_writemem)
	//MDRV_CPU_VBLANK_INT(aburner_interrupt,7)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(aburner_sound_readmem, aburner_sound_writemem)
	MDRV_CPU_PORTS(aburner_sound_readport, aburner_sound_writeport)

	MDRV_CPU_ADD(M68000, 12500000)
	MDRV_CPU_MEMORY(xboard_readmem2,xboard_writemem2)
	//MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(1000000 * (262 - 224) / (262 * 60))

	//MDRV_INTERLEAVE(1750)
	MDRV_INTERLEAVE(100)
	MDRV_MACHINE_INIT(xboard)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(40*8, 28*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
	MDRV_GFXDECODE(newgfxdecodeinfo)
	MDRV_PALETTE_LENGTH(8192*3)


	MDRV_VIDEO_START(xboard )
	MDRV_VIDEO_UPDATE(xboard )

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YM2151, ym2151_interface)
	MDRV_SOUND_ADD(SEGAPCM, sys16_segapcm_interface_15k_512)
MACHINE_DRIVER_END

GAMEX(19??, loffire,  0,         xboard,   thndrbld, 0, ROT0, "Sega", "Line of Fire", GAME_NOT_WORKING )
GAMEX(19?? ,thndrbld, 0,         xboard,   thndrbld, 0, ROT0, "Sega", "Thunder Blade (upright) (bootleg of FD1094 317-0056 set)", GAME_IMPERFECT_GRAPHICS )
GAMEX(19??, thndrbld1, thndrbld, xboard,   thndrbld, 0, ROT0, "Sega", "Thunder Blade (deluxe/standing) (unprotected)", GAME_IMPERFECT_GRAPHICS )
