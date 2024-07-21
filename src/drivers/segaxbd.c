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
static UINT8 gprider_hack;
static data8_t (*iochip_custom_io_r[2])(offs_t offset, data8_t portdata);
static void (*iochip_custom_io_w[2])(offs_t offset, data8_t data);
static UINT8 adc_reverse[8];
static data16_t *backupram1, *backupram2, *segaic16_shared_ram0, *segaic16_shared_ram1; 



WRITE16_HANDLER(SYS16IC_MWA16_ROADRAM_SHARE ) { offset &=0xfff; COMBINE_DATA( &segaic16_roadram_0[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_ROADRAM_SHARE ) { offset &=0xfff; return segaic16_roadram_0[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_BACKUPRAM1 ) { offset &=0x3fff; COMBINE_DATA( &backupram1[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_BACKUPRAM1 ) { offset &=0x3fff;  return backupram1[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_BACKUPRAM2 ) { offset &=0x3fff; COMBINE_DATA( &backupram2[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_BACKUPRAM2 ) { offset &=0x3fff; return backupram2[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_SHAREDRAM0 ) { offset &= 0x3fff; COMBINE_DATA( &segaic16_shared_ram0[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_SHAREDRAM0 ) { offset &= 0x3fff; return segaic16_shared_ram0[offset]; }

WRITE16_HANDLER(SYS16IC_MWA16_SHAREDRAM1 ) { offset &= 0x3fff; COMBINE_DATA( &segaic16_shared_ram1[offset] ); }
READ16_HANDLER( SYS16IC_MRA16_SHAREDRAM1 ) { offset &= 0x3fff; return segaic16_shared_ram1[offset]; }

READ16_HANDLER( SYS16IC_MRA16_SPRITERAM ){ offset &=0xfff; return segaic16_spriteram_0[offset]; }
WRITE16_HANDLER(SYS16IC_MWA16_SPRITERAM ){ offset &=0xfff;COMBINE_DATA( &segaic16_spriteram_0[offset] ); }

static void update_main_irqs(void)
{
	int irq = 0;

	/* the IRQs are effectively ORed together */
	if (vblank_irq_state)
		irq |= 4;
	if (timer_irq_state)
		irq |= 2;

	if (gprider_hack && irq > 4)
		irq = 4;

	/* assert the lines that are live, or clear everything if nothing is live */
	if (irq != 0)
	{
	cpu_set_irq_line(0, irq, ASSERT_LINE);
	//cpunum_set_input_line(0, irq, ASSERT_LINE);
		cpu_boost_interleave(0, TIME_IN_USEC(100));
	}
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
		//cpunum_set_input_line(0, 7, CLEAR_LINE);
}


static void scanline_callback(int scanline)
{
	int next_scanline = (scanline + 2) % 262;
	int update = 0;

	/* clock the timer and set the IRQ if something happened */
	if ((scanline % 2) != 0 && segaic16_compare_timer_clock(0))
		timer_irq_state = update = 1;

	/* set VBLANK on scanline 223 */
	if (scanline == 223)
	{
		vblank_irq_state = update = 1;
		cpu_set_irq_line(1, 4, ASSERT_LINE);
		//cpunum_set_input_line(1, 4, ASSERT_LINE);
		next_scanline = scanline + 1;
	}

	/* clear VBLANK on scanline 224 */
	else if (scanline == 224)
	{
		vblank_irq_state = 0;
		update = 1;
		cpu_set_irq_line(1, 4,  CLEAR_LINE);
		//cpunum_set_input_line(1, 4, CLEAR_LINE);
		next_scanline = scanline + 1;
	}

	/* update IRQs on the main CPU */
	if (update)
		update_main_irqs();

	/* come back in 2 scanlines */
	timer_set(cpu_getscanlinetime(next_scanline), next_scanline, scanline_callback);
}


static void timer_ack_callback(void)
{
	/* clear the timer IRQ */
	timer_irq_state = 0;
	update_main_irqs();
}


/*************************************
 *
 *	Sound communication
 *
 *************************************/

static void sound_data_w(data8_t data)
{
	soundlatch_w(0, data);
	cpu_set_irq_line(2, IRQ_LINE_NMI, PULSE_LINE);
	//cpunum_set_input_line(2, INPUT_LINE_NMI, PULSE_LINE);
}

/*************************************
 *
 *	Sound definitions for new intterupts xboard machine driver only
 *
 *************************************/

static void sound_cpu_irq(int state)
{
	cpu_set_irq_line(2, 0, state);
}


static void xboard_reset(void)
{
 	cpu_set_reset_line(1, PULSE_LINE );
	cpu_boost_interleave(0, TIME_IN_USEC(100));
}


VIDEO_START( xboard );
VIDEO_UPDATE( xboard );
void xboard_set_road_priority(int priority);



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


INLINE data16_t iochip_r(int which, int port, int inputval)
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
			//sound_global_enable(data & 0x80);
			return;
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
		/* Racing Hero and ABCop set this and fouls up their output ports */
		/*iochip_force_input = data & 1;*/
	}
}



/*************************************
 *
 *	Line of Fire Custom I/O
 *
 *************************************/

static data16_t *loffire_sync;

static WRITE16_HANDLER( loffire_sync0_w )
{
	COMBINE_DATA(&loffire_sync[offset]);
	cpu_boost_interleave(0, TIME_IN_USEC(10));
}


static VIDEO_UPDATE( loffire )
{
  /* these are correct i think */
  int x1 = readinputport(6);
	int y1 = readinputport(7);
	int x2 = readinputport(8);
	int y2 = readinputport(9);
	video_update_xboard(bitmap, cliprect);
	draw_crosshair(bitmap, x1 * (Machine->drv->screen_width - 1) / 255, y1 * (Machine->drv->screen_height - 1) / 255, cliprect);
	draw_crosshair(bitmap, x2 * (Machine->drv->screen_width - 1) / 255, y2 * (Machine->drv->screen_height - 1) / 255, cliprect);

}


READ16_HANDLER( SYS16IC_CPU2ROM16_r ){
	const data16_t *pMem = (data16_t *)memory_region(REGION_CPU2);
	return pMem[offset];
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
	{ 0x080000, 0x09ffff, SYS16IC_MRA16_BACKUPRAM1 },
	//AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(2) AM_BASE(&backupram2)
	{ 0x0a0000, 0x0bffff, SYS16IC_MRA16_BACKUPRAM2 },
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
	{ 0x100000, 0x10efff, SYS16IC_MRA16_SPRITERAM },

	//AM_RANGE(0x120000, 0x123fff) AM_MIRROR(0x00c000) AM_READWRITE(MRA16_RAM, segaic16_paletteram_w) AM_BASE(&paletteram16)
	{ 0x120000, 0x123fff, MRA16_RAM },
	//AM_RANGE(0x130000, 0x13ffff) AM_READWRITE(adc_r, adc_w)
	{ 0x130000, 0x13ffff, adc_r },
	//AM_RANGE(0x140000, 0x14ffff) AM_READWRITE(iochip_0_r, iochip_0_w)
	{ 0x140000, 0x14ffff, iochip_0_r },
	//AM_RANGE(0x150000, 0x15ffff) AM_READWRITE(iochip_1_r, iochip_1_w)
	{ 0x150000, 0x15ffff, iochip_1_r },
	//AM_RANGE(0x200000, 0x27ffff) AM_ROM AM_REGION(REGION_CPU2, 0x00000)
	{ 0x200000, 0x27ffff, SYS16IC_CPU2ROM16_r   },		/* CPU2 ROM == CPU3 in this core  */
	//AM_RANGE(0x280000, 0x283fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x280000, 0x29ffff, SYS16IC_MRA16_SHAREDRAM0 },
	//AM_RANGE(0x2a0000, 0x2a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{0x2a0000, 0x2bffff, SYS16IC_MRA16_SHAREDRAM1 },
	//AM_RANGE(0x2e0000, 0x2e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x2e0000, 0x2e0007, segaic16_multiply_1_r  },
	//AM_RANGE(0x2e4000, 0x2e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x2e4000, 0x2e401f, segaic16_divide_1_r },
	//	AM_RANGE(0x2e8000, 0x2e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x2e8000, 0x2e800f, segaic16_compare_timer_1_r },
	//AM_RANGE(0x2ec000, 0x2ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5) AM_BASE(&segaic16_roadram_0)
	{ 0x2ec000, 0x2edfff, SYS16IC_MRA16_ROADRAM_SHARE },
	//AM_RANGE(0x2ee000, 0x2effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x2ee000, 0x2effff, segaic16_road_control_0_r },
	//AM_RANGE(0x3f8000, 0x3fbfff) AM_RAM AM_SHARE(1)
	{ 0xff8000, 0xffbfff, SYS16IC_MRA16_BACKUPRAM1 },
	{ 0xffc000, 0xffffff, SYS16IC_MRA16_BACKUPRAM2 },
MEMORY_END

static MEMORY_WRITE16_START( xboard_writemem )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
//	{ 0x000000, 0x07ffff, MWA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(1) AM_BASE(&backupram1)
	{ 0x080000, 0x09ffff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },
	//AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(2) AM_BASE(&backupram2)
	{ 0x0a0000, 0x0bffff, SYS16IC_MWA16_BACKUPRAM2,&backupram2 },
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
	{ 0x100000, 0x10efff, SYS16IC_MWA16_SPRITERAM, &segaic16_spriteram_0  },
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
	{ 0x280000, 0x29ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	//AM_RANGE(0x2a0000, 0x2a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{ 0x2a0000, 0x2bbfff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1 },
	//AM_RANGE(0x2e0000, 0x2e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x2e0000, 0x2e0007, segaic16_multiply_1_w },
	//AM_RANGE(0x2e4000, 0x2e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x2e4000, 0x2e401f, segaic16_divide_1_w },
	//AM_RANGE(0x2e8000, 0x2e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x2e8000, 0x2e800f, segaic16_compare_timer_1_w  },		/* includes sound latch! */
	//AM_RANGE(0x2ec000, 0x2ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5) AM_BASE(&segaic16_roadram_0)
	{ 0x2ec000, 0x2edfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	//AM_RANGE(0x2ee000, 0x2effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x2ee000, 0x2effff, segaic16_road_control_0_w },
	//AM_RANGE(0x3f8000, 0x3fbfff) AM_RAM AM_SHARE(1)
	{ 0xff8000, 0xffbfff, SYS16IC_MWA16_BACKUPRAM1,&backupram1 },// AMEF_ABITS(22)
	//AM_RANGE(0x3fc000, 0x3fffff) AM_RAM AM_SHARE(2)
	{ 0xffc000, 0xffffff, SYS16IC_MWA16_BACKUPRAM2,&backupram2 },// AMEF_ABITS(22)
MEMORY_END

static MEMORY_READ16_START( xboard_readmem2 )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
	{ 0x000000, 0x07ffff, MRA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x080000, 0x09ffff, SYS16IC_MRA16_SHAREDRAM0 },
//	AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{ 0x0a0000, 0x0bffff, SYS16IC_MRA16_SHAREDRAM1 },
	//AM_RANGE(0x0e0000, 0x0e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x0e0000, 0x0e0007, segaic16_multiply_1_r },
	//AM_RANGE(0x0e4000, 0x0e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x0e4000, 0x0e401f, segaic16_divide_1_r },
	//AM_RANGE(0x0e8000, 0x0e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x0e8000, 0x0e800f, segaic16_compare_timer_1_r },
	//AM_RANGE(0x0ec000, 0x0ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5)
	{ 0x0ec000, 0x0edfff, SYS16IC_MRA16_ROADRAM_SHARE },
	//AM_RANGE(0x0ee000, 0x0effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x0ee000, 0x0effff, segaic16_road_control_0_r },
	{ 0x200000, 0x27ffff, SYS16IC_CPU2ROM16_r },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x280000, 0x29ffff, SYS16IC_MRA16_SHAREDRAM0 },
	{ 0x2a0000, 0x2bffff, SYS16IC_MRA16_SHAREDRAM1 },
	{ 0x2ec000, 0x2edfff, SYS16IC_MRA16_ROADRAM_SHARE },
	{ 0x2ee000, 0x2effff, segaic16_road_control_0_r },
MEMORY_END

static MEMORY_WRITE16_START( xboard_writemem2 )
	//AM_RANGE(0x000000, 0x07ffff) AM_ROM
	//{ 0x000000, 0x07ffff, MWA16_ROM },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x080000, 0x09ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },
	//AM_RANGE(0x0a0000, 0x0a3fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(4)
	{ 0x0a0000, 0x0bffff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	//AM_RANGE(0x0e0000, 0x0e0007) AM_MIRROR(0x003ff8) AM_READWRITE(segaic16_multiply_1_r, segaic16_multiply_1_w)
	{ 0x0e0000, 0x0e0007, segaic16_multiply_1_w  },
	//AM_RANGE(0x0e4000, 0x0e401f) AM_MIRROR(0x003fe0) AM_READWRITE(segaic16_divide_1_r, segaic16_divide_1_w)
	{ 0x0e4000, 0x0e401f, segaic16_divide_1_w },
	//AM_RANGE(0x0e8000, 0x0e800f) AM_MIRROR(0x003ff0) AM_READWRITE(segaic16_compare_timer_1_r, segaic16_compare_timer_1_w)
	{ 0x0e8000, 0x0e800f, segaic16_compare_timer_1_w  },
	//AM_RANGE(0x0ec000, 0x0ecfff) AM_MIRROR(0x001000) AM_RAM AM_SHARE(5)
	{ 0x0ec000, 0x0edfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	//AM_RANGE(0x0ee000, 0x0effff) AM_READWRITE(segaic16_road_control_0_r, segaic16_road_control_0_w)
	{ 0x0ee000, 0x0effff, segaic16_road_control_0_w  },
	//AM_RANGE(0x080000, 0x083fff) AM_MIRROR(0x01c000) AM_RAM AM_SHARE(3)
	{ 0x280000, 0x29ffff, SYS16IC_MWA16_SHAREDRAM0, &segaic16_shared_ram0 },  ///AMEF_ABITS(20)
	{ 0x2a0000, 0x2bffff, SYS16IC_MWA16_SHAREDRAM1, &segaic16_shared_ram1  },
	{ 0x2ec000, 0x2edfff, SYS16IC_MWA16_ROADRAM_SHARE, &segaic16_roadram_0 },
	{ 0x2ee000, 0x2effff, segaic16_road_control_0_w  },
MEMORY_END

static MEMORY_READ_START( aburner_sound_readmem )
    { 0x0000, 0xefff, MRA_ROM },
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

static MEMORY_READ_START(  smgp_comm_readmem )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x3fff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( smgp_comm_writemem )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x3fff, MWA_RAM },
MEMORY_END

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

INPUT_PORTS_START( abcop )
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
	PORT_DIPNAME( 0x01, 0x01, "Credits" )
	PORT_DIPSETTING(    0x01, "1 to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x00, "2 to Start, 1 to Continue" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Allow_Continue" )
	PORT_DIPSETTING(    0x04, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Time" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START//_TAG("ADC0")	/* steering */
	PORT_ANALOG( 0xff, 0x7f, IPT_PADDLE | IPF_REVERSE, 100, 4, 0x00,0xff)

	PORT_START//_TAG("ADC1")	/* accelerator */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL, 10, 15, 0x00,0xff)
INPUT_PORTS_END

INPUT_PORTS_START( rachero )
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

	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON8, "Move to Center", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_BUTTON9, "Suicide", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
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
	PORT_DIPNAME( 0x01, 0x01, "Credits" )
	PORT_DIPSETTING(    0x01, "1 to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x00, "2 to Start, 1 to Continue" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Allow_Continue" )
	PORT_DIPSETTING(    0x04, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Time" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Very_Hard" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0x00, "Very_Hard" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START//_TAG("ADC0")	/* steering */
	PORT_ANALOG( 0xff, 0x7f, IPT_PADDLE | IPF_REVERSE, 100, 4, 0x00,0xff)

	PORT_START//_TAG("ADC1")	/* gas pedal */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL, 10, 15, 0x00,0xff)

	PORT_START//_TAG("ADC2")	/* brake */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL2, 10, 25, 0x00,0xff)
INPUT_PORTS_END

INPUT_PORTS_START( smgp )
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
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x07, 0x07, "Machine ID" )
	PORT_DIPSETTING(    0x07, "1" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x38, 0x38, "Number of Machines" )
	PORT_DIPSETTING(    0x38, "1" )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x28, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPSETTING(    0x10, "6" )
	PORT_DIPSETTING(    0x08, "7" )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, "Cockpit" )
	PORT_DIPSETTING(    0xc0, "Deluxe" )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
//	PORT_DIPSETTING(    0x00, "Deluxe" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START//_TAG("ADC0")	/* steering */
	PORT_ANALOG( 0xff, 0x7f, IPT_PADDLE, 50, 4, 0x00,0xff)

	PORT_START//_TAG("ADC1")	/* gas pedal */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL, 10, 15, 0x00,0xff)

	PORT_START//_TAG("ADC2")	/* brake */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL2, 10, 25, 0x00,0xff)
INPUT_PORTS_END

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

INPUT_PORTS_START( loffire )
	PORT_START//_TAG("IO0PORTA")
	PORT_BIT( 0x3f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SPECIAL )	/* /INTR of ADC0804 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START//_TAG("IO0PORTB")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START//_TAG("IO1PORTA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
//	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
//	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x30, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START//_TAG("IO1PORTB")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1)

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
	PORT_DIPNAME( 0x01, 0x00, "Language" )
	PORT_DIPSETTING(    0x01, "Japanese" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, "Cockpit" )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPNAME( 0x04, 0x04, "2 Credits to Start" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x00, "Allow_Continue" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Coin Chute" )
	PORT_DIPSETTING(    0x80, DEF_STR( Single ) )
	PORT_DIPSETTING(    0x00, "Twin" )
	
	PORT_START//_TAG("ADC0")
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X, 50, 5, 0, 255)

	PORT_START//_TAG("ADC1")
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y, 50, 5, 0, 255)

	PORT_START//_TAG("ADC2")
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_X | IPF_PLAYER2, 50, 5, 0, 255)

	PORT_START//_TAG("ADC3")
	PORT_ANALOG( 0xff, 0x80, IPT_LIGHTGUN_Y | IPF_PLAYER2, 50, 5, 0, 255)
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

	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* sound CPU */
	ROM_LOAD( "epr-12798.17", 0x00000, 0x10000, CRC(0587738d) SHA1(24c79b0c73616d5532a49a2c9121dfabe3a80c7d) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 2nd 68000 code */
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



	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* sound CPU */
	ROM_LOAD( "epr-11396.ic17", 0x00000, 0x10000, CRC(d37b54a4) SHA1(c230fe7241a1f13ca13506d1492f348f506c40a7) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 ) /* Sega PCM sound data */
	ROM_LOAD( "epr-11317.ic11", 0x00000, 0x20000, CRC(d4e7ac1f) SHA1(ec5d6e4949938adf56e5613801ae56ff2c3dede5) )
	ROM_LOAD( "epr-11318.ic12", 0x20000, 0x20000, CRC(70d3f02c) SHA1(391aac2bc5673e06150de27e19c7c6359da8ca82) )
	ROM_LOAD( "epr-11319.ic13", 0x40000, 0x20000, CRC(50d9242e) SHA1(a106371bf680c3088ec61f07fc5c4ce467973c15) )

	ROM_REGION( 0x100000, REGION_CPU2, 0 ) /* 2nd 68000 code */
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

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) // 2nd 68000 code
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

	ROM_REGION( 0x10000, REGION_CPU3, 0 ) // sound CPU
	ROM_LOAD( "epr-11312.ic17",   0x00000, 0x10000, CRC(3b974ed2) SHA1(cf18a2d0f01643c747a884bf00e5b7037ba2e64a) )

	ROM_REGION( 0x80000, REGION_SOUND1, ROMREGION_ERASEFF ) // Sega PCM sound data
	ROM_LOAD( "epr-11317.ic11", 0x00000, 0x20000, CRC(d4e7ac1f) SHA1(ec5d6e4949938adf56e5613801ae56ff2c3dede5) )
	ROM_LOAD( "epr-11318.ic12", 0x20000, 0x20000, CRC(70d3f02c) SHA1(391aac2bc5673e06150de27e19c7c6359da8ca82) )
	ROM_LOAD( "epr-11319.ic13", 0x40000, 0x20000, CRC(50d9242e) SHA1(a106371bf680c3088ec61f07fc5c4ce467973c15) )
ROM_END

/**************************************************************************************************************************
 **************************************************************************************************************************
 **************************************************************************************************************************
	AB Cop, Sega X-board
	CPU: FD1094 (317-0169b)
*/
ROM_START( abcop )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr13568b.ic58", 0x00000, 0x20000, CRC(3c367a01) SHA1(ddb37a399a67818c5b14c4fac1f25d0e660a7b0f) )
	ROM_LOAD16_BYTE( "bootleg_epr13556b.ic63", 0x00001, 0x20000, CRC(1078246e) SHA1(3490f46c1f52d41f96fb449bdcee5fbec871aaca) )
	ROM_LOAD16_BYTE( "epr13559.57",  0x40000, 0x20000, CRC(4588bf19) SHA1(6a8b3d4450ac0bc41b46e6a4e1b44d82112fcd64) )
	ROM_LOAD16_BYTE( "epr13558.62",  0x40001, 0x20000, CRC(11259ed4) SHA1(e7de174a0bdb1d1111e5e419f1d501ab5be1d32d) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) /* 2nd 68000 code */
	ROM_LOAD16_BYTE( "epr13566.20", 0x00000, 0x20000, CRC(22e52f32) SHA1(c67a4ccb88becc58dddcbfea0a1ac2017f7b2929) )
	ROM_LOAD16_BYTE( "epr13565.29", 0x00001, 0x20000, CRC(a21784bd) SHA1(b40ba0ef65bbfe514625253f6aeec14bf4bcf08c) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "opr13553.154", 0x00000, 0x10000, CRC(8c418837) SHA1(e325db39fae768865e20d2cd1ee2b91a9b0165f5) )
	ROM_LOAD( "opr13554.153", 0x10000, 0x10000, CRC(4e3df9f0) SHA1(8b481c2cd25c58612ac8ac3ffb7eeae9ca247d2e) )
	ROM_LOAD( "opr13555.152", 0x20000, 0x10000, CRC(6c4a1d42) SHA1(6c37b045b21173f1e2f7bd19d01c00979b8107fb) )

	ROM_REGION32_LE( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD32_BYTE( "opr13552.90",  0x000000, 0x20000, CRC(cc2cf706) SHA1(ad39c22e652ebcd90ffb5e17ae35985645f93c71) )
	ROM_LOAD32_BYTE( "opr13551.94",  0x000001, 0x20000, CRC(d6f276c1) SHA1(9ec68157ea460e09ef4b69aa8ea17687dc47ea59) )
	ROM_LOAD32_BYTE( "opr13550.98",  0x000002, 0x20000, CRC(f16518dd) SHA1(a5f1785cd28f03069cb238ac92c6afb5a26cbd37) )
	ROM_LOAD32_BYTE( "opr13549.102", 0x000003, 0x20000, CRC(cba407a7) SHA1(e7684d3b40baa6d832b887fd85ad67fbad8aa7de) )
	ROM_LOAD32_BYTE( "opr13548.91",  0x080000, 0x20000, CRC(080fd805) SHA1(e729565815a3a37462cfee460b7392d2f08e96e5) )
	ROM_LOAD32_BYTE( "opr13547.95",  0x080001, 0x20000, CRC(42d4dd68) SHA1(6ae1f3585ebb20fd2908456d6fa41a893261277e) )
	ROM_LOAD32_BYTE( "opr13546.99",  0x080002, 0x20000, CRC(ca6fbf3d) SHA1(49c3516d87f1546fa7efe785fc5c064d90b1cb8e) )
	ROM_LOAD32_BYTE( "opr13545.103", 0x080003, 0x20000, CRC(c9e58dd2) SHA1(ace2e1630d8df2454183ffdbe26d8cb6d199e940) )
	ROM_LOAD32_BYTE( "opr13544.92",  0x100000, 0x20000, CRC(9c1436d9) SHA1(5156e1b5c7461f6dc0d449b86b6b72153b290a4c) )
	ROM_LOAD32_BYTE( "opr13543.96",  0x100001, 0x20000, CRC(2c1c8f0e) SHA1(19c9fd4272a3db18381f435ed6cd01f994c655e7) )
	ROM_LOAD32_BYTE( "opr13542.100", 0x100002, 0x20000, CRC(01fd52b8) SHA1(b4ab13c7b2b2ffcfdab37d8e4855d5ef8823f1cc) )
	ROM_LOAD32_BYTE( "opr13541.104", 0x100003, 0x20000, CRC(a45c547b) SHA1(d93aaa850d14a7699a1b0411e823088a9bce7553) )
	ROM_LOAD32_BYTE( "opr13540.93",  0x180000, 0x20000, CRC(84b42ab0) SHA1(d24ba7fe23463fc5813ef26e0395951559d6d162) )
	ROM_LOAD32_BYTE( "opr13539.97",  0x180001, 0x20000, CRC(cd6e524f) SHA1(e6df2552a84b2da95301486379c78679b0297634) )
	ROM_LOAD32_BYTE( "opr13538.101", 0x180002, 0x20000, CRC(bf9a4586) SHA1(6013dee83375d72d262c8c04c2e668afea2e216c) )
	ROM_LOAD32_BYTE( "opr13537.105", 0x180003, 0x20000, CRC(fa14ed3e) SHA1(d684496ade2517696a56c1423dd4686d283c133f) )

	ROM_REGION( 0x10000, REGION_GFX3, 0 ) /* ground data */
	ROM_LOAD( "opr13564.40",	 0x00000, 0x10000, CRC(e70ba138) SHA1(85eb6618f408642227056d278f10dec8dcc5a80d) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* sound CPU */
	ROM_LOAD( "epr13560.17",    0x00000, 0x10000, CRC(83050925) SHA1(118710e5789c7999bb7326df4d7bd207cbffdfd4) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 ) /* Sega PCM sound data */
	ROM_LOAD( "opr13563.11",    0x00000, 0x20000, CRC(4083e74f) SHA1(e48c7ce0aa3406af0bbf79c169a8157693c97041) )
	ROM_LOAD( "opr13562.12",    0x20000, 0x20000, CRC(3cc3968f) SHA1(d25647f6a3fa939ba30e03e7334362ef0749b23a) )
	ROM_LOAD( "opr13561.13",    0x40000, 0x20000, CRC(80a7c02a) SHA1(7e8c1b9ba270d8657dbe90ed8be2e4b6463e5928) )
ROM_END

/**************************************************************************************************************************
 **************************************************************************************************************************
 **************************************************************************************************************************
	Racing Hero, Sega X-board
	CPU: FD1094 (317-0144)
*/
ROM_START( rachero )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-13129.ic58", 0x00000, 0x20000, CRC(82ee7312) SHA1(4d011529b538885bbc3bb1cb23048b785d3be318) )
	ROM_LOAD16_BYTE( "bootleg_epr-13130.ic63", 0x00001, 0x20000, CRC(53fb8649) SHA1(8b66d6e2018f92c7c992944ed5d4a685d9f13a6d) )
	ROM_LOAD16_BYTE( "epr12855.57", 0x40000, 0x20000,CRC(cecf1e73) SHA1(3f8631379f32dbfda7720ef345276f9be23ada06) )
	ROM_LOAD16_BYTE( "epr12856.62", 0x40001, 0x20000,CRC(da900ebb) SHA1(595ed65248185ddf8666b3f30ad6329162116448) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) /* 2nd 68000 code */
	ROM_LOAD16_BYTE( "epr12857.20", 0x00000, 0x20000, CRC(8a2328cc) SHA1(c34498428ddfb3eeb986f4153a6165a685d8fc8a) )
	ROM_LOAD16_BYTE( "epr12858.29", 0x00001, 0x20000, CRC(38a248b7) SHA1(a17672123665403c1c56fedab6c8abf44b1131f9) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12879.154", 0x00000, 0x10000, CRC(c1a9de7a) SHA1(2425456a9d4ba92e1f2da6c2f164a6d5a5dee7c7) )
	ROM_LOAD( "epr12880.153", 0x10000, 0x10000, CRC(27ff04a5) SHA1(b554a6e060f4803100be8efa52977b503eb0f31d) )
	ROM_LOAD( "epr12881.152", 0x20000, 0x10000, CRC(72f14491) SHA1(b7a6cbd08470a5edda77cdd0337abd502c4905fd) )

	ROM_REGION32_LE( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD32_BYTE( "epr12872.90",  0x000000, 0x20000, CRC(68d56139) SHA1(b5f32edbda10c31d52f90defea2bae226676069f) )
	ROM_LOAD32_BYTE( "epr12873.94",  0x000001, 0x20000, CRC(3d3ec450) SHA1(ac96ad8c7b365478bd1e5826a073e242f1208247) )
	ROM_LOAD32_BYTE( "epr12874.98",  0x000002, 0x20000, CRC(7d6bde23) SHA1(88b12ec6386cdad60b0028b72033a0037a0cdbdb) )
	ROM_LOAD32_BYTE( "epr12875.102", 0x000003, 0x20000, CRC(e33092bf) SHA1(31e211e25adac0a98befb459093f23c905fbc1e6) )
	ROM_LOAD32_BYTE( "epr12868.91",  0x080000, 0x20000, CRC(96289583) SHA1(4d37e67860bc0e6ef69f0a0775c28f6f2fd6875e) )
	ROM_LOAD32_BYTE( "epr12869.95",  0x080001, 0x20000, CRC(2ef0de02) SHA1(11ee3d77df2cddd3156da52e50565505f95f4cd4) )
	ROM_LOAD32_BYTE( "epr12870.99",  0x080002, 0x20000, CRC(c76630e1) SHA1(7b76e4819990e147639d6b930b17b6fa10df191c) )
	ROM_LOAD32_BYTE( "epr12871.103", 0x080003, 0x20000, CRC(23401b1a) SHA1(eaf465ffda84bdb83cc85daf781275bada396aab) )
	ROM_LOAD32_BYTE( "epr12864.92",  0x100000, 0x20000, CRC(77d6cff4) SHA1(1e625204801d03369311844efb26d22216253ac4) )
	ROM_LOAD32_BYTE( "epr12865.96",  0x100001, 0x20000, CRC(1e7e685b) SHA1(532fe361357383aa9dada833cbe31716c58001e5) )
	ROM_LOAD32_BYTE( "epr12866.100", 0x100002, 0x20000, CRC(fdf31329) SHA1(9c229a0f9d8b8114acfe4f17b45a9b8640560b3e) )
	ROM_LOAD32_BYTE( "epr12867.104", 0x100003, 0x20000, CRC(b25e37fd) SHA1(fef5bfe4690b3203b83fd565d883b2c63f439633) )
	ROM_LOAD32_BYTE( "epr12860.93",  0x180000, 0x20000, CRC(86b64119) SHA1(d39aedad0f05e500e33af888126bd2fc22539141) )
	ROM_LOAD32_BYTE( "epr12861.97",  0x180001, 0x20000, CRC(bccff19b) SHA1(32c3f7802a12be02a114b78cd898c46fcb1c0a61) )
	ROM_LOAD32_BYTE( "epr12862.101", 0x180002, 0x20000, CRC(7d4c3b05) SHA1(4e25a077b403549c681c5047912d0e28f4c07720) )
	ROM_LOAD32_BYTE( "epr12863.105", 0x180003, 0x20000, CRC(85095053) SHA1(f93194ecc0300956280cc0515b3e3ba2c9f71364) )

	ROM_REGION( 0x10000, REGION_GFX3, 0 ) /* ground data */
	/* none */

	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* sound CPU */
	ROM_LOAD( "epr12859.17",    0x00000, 0x10000, CRC(d57881da) SHA1(75b7f331ea8c2e33d6236e0c8fc8dabe5eef8160) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 ) /* Sega PCM sound data */
	ROM_LOAD( "epr12876.11",    0x00000, 0x20000, CRC(f72a34a0) SHA1(28f7d077c24352557da3a91a7e49b0c5b79f2a2e) )
	ROM_LOAD( "epr12877.12",    0x20000, 0x20000, CRC(18c1b6d2) SHA1(860cbb96999ab76c40ce96996bba70c42d845abc) )
	ROM_LOAD( "epr12878.13",    0x40000, 0x20000, CRC(7c212c15) SHA1(360b332d2fb32d88949ff8b357a863ffaaca39c2) )
ROM_END

 /**************************************************************************************************************************
 **************************************************************************************************************************
 **************************************************************************************************************************
	Super Monaco GP, Sega X-board
	CPU: FD1094 (317-0125a)
*/
ROM_START( smgp )
	ROM_REGION( 0x80000, REGION_CPU1, 0 ) /* 68000 code */
	ROM_LOAD16_BYTE( "bootleg_epr-12563b.58", 0x00000, 0x20000, CRC(af30e3cd) SHA1(b05a4f8be701fada6d55a042079f4b2067b52cb2) )
	ROM_LOAD16_BYTE( "bootleg_epr-12564b.63", 0x00001, 0x20000, CRC(eb7cadfe) SHA1(58c2d05cd21795c1d5d603179decc3b861ef438f) )

	ROM_REGION( 0x80000, REGION_CPU2, 0 ) /* 2nd 68000 code */
	ROM_LOAD16_BYTE( "epr12576a.20", 0x00000, 0x20000, CRC(2c9599c1) SHA1(79206f38c2976bd9299ed37bf62ac26dd3fba801) )
	ROM_LOAD16_BYTE( "epr12577a.29", 0x00001, 0x20000, CRC(abf9a50b) SHA1(e367b305cd45900aae4849af4904543f05456dc6) )

	ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
	ROM_LOAD( "epr12429.154", 0x00000, 0x10000, CRC(5851e614) SHA1(3dc97237ede2c6125e92ea6efc68a748d0ec69be) )
	ROM_LOAD( "epr12430.153", 0x10000, 0x10000, CRC(05e00134) SHA1(8baaa80815d5dabd38dc8600e357975b96d23b95) )
	ROM_LOAD( "epr12431.152", 0x20000, 0x10000, CRC(35572f4a) SHA1(d66456ecf7b59f81736fb873c553926b56bb3977))

	ROM_REGION32_LE( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD32_BYTE( "mpr12425.90",  0x000000, 0x20000, CRC(14bf2a15) SHA1(84db3ac09e4a8fe470ac051d8d5de1814b48bc72) )
	ROM_LOAD32_BYTE( "mpr12426.94",  0x000001, 0x20000, CRC(28b60dc0) SHA1(ad69d449434853445a076319a55a29014217a100) )
	ROM_LOAD32_BYTE( "mpr12427.98",  0x000002, 0x20000, CRC(0a367928) SHA1(bcb558b7c23906397e66a7f046b09eb5036c0888) )
	ROM_LOAD32_BYTE( "mpr12428.102", 0x000003, 0x20000, CRC(efa80ad5) SHA1(9bc7c3fb60cc076f29a0af487d58e5b48f1c4b06) )
	ROM_LOAD32_BYTE( "mpr12421.91",  0x080000, 0x20000, CRC(25f46140) SHA1(ea75e364cf52636d100158f79be627e36da8c327) )
	ROM_LOAD32_BYTE( "mpr12422.95",  0x080001, 0x20000, CRC(cb51c8f6) SHA1(5af56ae1916c3212b8d5b9e4bccbbe1916694f89) )
	ROM_LOAD32_BYTE( "mpr12423.99",  0x080002, 0x20000, CRC(0be9818e) SHA1(637a8201416e73d53f7e2502ea0a5277e43c167d) )
	ROM_LOAD32_BYTE( "mpr12424.103", 0x080003, 0x20000, CRC(0ce00dfc) SHA1(3b1990977ec7ad4c3bea66527707cff2cd8d5a98) )
	ROM_LOAD32_BYTE( "mpr12417.92",  0x100000, 0x20000, CRC(a806eabf) SHA1(1a61a2135d92b42ee131fd3240bc8a17a96696ab) )
	ROM_LOAD32_BYTE( "mpr12418.96",  0x100001, 0x20000, CRC(ed1a0f2b) SHA1(1aa87292ca0465fa129d6be81d95dbb77332ecab) )
	ROM_LOAD32_BYTE( "mpr12419.100", 0x100002, 0x20000, CRC(ce4568cb) SHA1(1ed66e74ce94d41593b498827d9cc243f775d4ba) )
	ROM_LOAD32_BYTE( "mpr12420.104", 0x100003, 0x20000, CRC(679442eb) SHA1(f88ef0219497f955d8db6783f3636dad52928f46) )
	ROM_LOAD32_BYTE( "epr12609.93",  0x180000, 0x20000, CRC(a867812f) SHA1(f8950bf794b6c2ec767ffff837d28917b636dbe7) ) //
	ROM_LOAD32_BYTE( "epr12610.97",  0x180001, 0x20000, CRC(53b99417) SHA1(ab72d35c88695c777d24c5557e5d3ea2d446e51b) ) //
	ROM_LOAD32_BYTE( "epr12611.101", 0x180002, 0x20000, CRC(bd5c6ab0) SHA1(7632dc4daa8eabe74769369856a8ba451e5bd420) ) // these differ from japan set
	ROM_LOAD32_BYTE( "epr12612.105", 0x180003, 0x20000, CRC(ac86e890) SHA1(7720c1c8df6de5de50254e97772c15161b796031) ) //

	ROM_REGION( 0x10000, REGION_GFX3, 0 ) /* road gfx */
	/* none?? */

	ROM_REGION( 0x10000, REGION_CPU3, 0 ) /* sound CPU */
	ROM_LOAD( "epr12436.17",    0x00000, 0x10000, CRC(16ec5f0a) SHA1(307b7388b5c36fd4bc2a61f7941db44858e03c5c) )

	ROM_REGION( 0x60000, REGION_SOUND1, 0 ) /* Sega PCM sound data */
	ROM_LOAD( "mpr12437.11",    0x00000, 0x20000, CRC(a1c7e712) SHA1(fa7fa8c39690ae5dab8b28af5aeed5ffae2cd6de) )
	ROM_LOAD( "mpr12438.12",    0x20000, 0x20000, CRC(6573d46b) SHA1(c4a4a0ea35250eff28a5bfd5e9cd372f52fd1308) )
	ROM_LOAD( "mpr12439.13",    0x40000, 0x20000, CRC(13bf6de5) SHA1(92228a05ec33d606491a1da98c4989f69cddbb49) )

	ROM_REGION( 0x10000, REGION_CPU4, 0 ) /* comms */
	ROM_LOAD( "epr12587.14",    0x00000, 0x8000, CRC(2afe648b) SHA1(b5bf86f3acbcc23c136185110acecf2c971294fa) )
ROM_END

static MACHINE_INIT( xboard ){

	//cpu_set_irq_callback(0, main_irq_callback);
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

static READ16_HANDLER( smgp_excs_r )
{
	logerror("%06X:smgp_excs_r(%04X)\n", activecpu_get_pc(), offset*2);
	return 0xffff;
}


static WRITE16_HANDLER( smgp_excs_w )
{
	logerror("%06X:smgp_excs_w(%04X) = %04X & %04X\n", activecpu_get_pc(), offset*2, data, mem_mask ^ 0xffff);
}

static DRIVER_INIT ( smgp )
{
	install_mem_read16_handler (0, 0x2f0000, 0x2f3fff, smgp_excs_r);
	install_mem_write16_handler(0, 0x2f0000, 0x2f3fff, smgp_excs_w);
}

static DRIVER_INIT( loffire )
{
	/* install extra synchronization on core shared memory */
	loffire_sync = install_mem_write16_handler(0, 0x29c000, 0x29c011, loffire_sync0_w);
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

	MDRV_CPU_ADD(M68000, 12500000)
	MDRV_CPU_MEMORY(xboard_readmem2,xboard_writemem2)
	//MDRV_CPU_VBLANK_INT(irq4_line_hold,1)

	MDRV_CPU_ADD(Z80, 4000000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(aburner_sound_readmem, aburner_sound_writemem)
	MDRV_CPU_PORTS(aburner_sound_readport, aburner_sound_writeport)

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

MACHINE_DRIVER_START( smgp )
	MDRV_IMPORT_FROM(xboard)
//	MDRV_CPU_ADD_TAG("comm", Z80, 4000000)
//	MDRV_CPU_MEMORY(smgp_comm_readmem, smgp_comm_writemem)
	//MDRV_CPU_PORTS(smgp_comm_readport, smgp_comm_writeport)
MACHINE_DRIVER_END

MACHINE_DRIVER_START( loffire )
	MDRV_IMPORT_FROM(xboard)
	MDRV_VIDEO_UPDATE(loffire)
MACHINE_DRIVER_END

GAMEX(1990, abcop,     0,        xboard,  abcop,    0,       ROT0, "Sega", "A.B. Cop (World) (bootleg of FD1094 317-0169b set)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1989, rachero,   0,        xboard,  rachero,  0,       ROT0, "Sega", "Racing Hero (bootleg of FD1094 317-0144 set)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1989, smgp,      0,        smgp,    smgp,     smgp,    ROT0, "Sega", "Super Monaco GP (World, Rev B) (bootleg of FD1094 317-0126a set)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1989, loffire,   0,        loffire, loffire,  loffire, ROT0, "Sega", "Line of Fire", GAME_NOT_WORKING )
GAMEX(1987, thndrbld,  0,        xboard,  thndrbld, 0,       ROT0, "Sega", "Thunder Blade (upright) (bootleg of FD1094 317-0056 set)", GAME_IMPERFECT_GRAPHICS )
GAMEX(1987, thndrbld1, thndrbld, xboard,  thndrbld, 0,       ROT0, "Sega", "Thunder Blade (deluxe/standing) (unprotected)", GAME_IMPERFECT_GRAPHICS )
