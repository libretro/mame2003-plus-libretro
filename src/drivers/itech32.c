/***************************************************************************

	Incredible Technologies/Strata system
	(32-bit blitter variant)

    driver by Aaron Giles

    Games supported:
		* Time Killers (2 sets)
		* Bloodstorm (4 sets)
		* Hard Yardage (2 sets)
		* Pairs
		* Driver's Edge (not working)
		* World Class Bowling (4 sets)
		* Street Fighter: The Movie (4 sets)
		* Shuffleshot (2 sets)

	Games not supported because IT is still selling them:
		* World Class Bowling Deluxe
		* Golden Tee 3D Golf  (8 sets)
		* Golden Tee Golf '97 (6 sets)
		* Golden Tee Golf '98 (4 sets)
		* Golden Tee Golf '99 (3 Sets)
		* Golden Tee Golf 2K  (2 Sets)
		* Golden Tee Classic  (2 Sets)

****************************************************************************

	Memory map TBD

***************************************************************************/


#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m68000/m68000.h"
#include "machine/ticket.h"
#include "vidhrdw/generic.h"
#include "itech32.h"
#include <math.h>


#define FULL_LOGGING	0

#define CLOCK_8MHz		(8000000)
#define CLOCK_12MHz		(12000000)
#define CLOCK_25MHz		(25000000)



/*************************************
 *
 *	Static data
 *
 *************************************/

static UINT8 vint_state;
static UINT8 xint_state;
static UINT8 qint_state;

static data8_t sound_data;
static UINT8 sound_int_state;

static data8_t *via6522;
static data16_t via6522_timer_count[2];
static void *via6522_timer[2];
static data8_t via6522_int_state;

static data16_t *main_rom;
static data16_t *main_ram;
static size_t main_ram_size;
static data32_t *nvram;
static size_t nvram_size;

static offs_t itech020_prot_address;

static data8_t *sound_speedup_data;
static data16_t sound_speedup_pc;

static UINT8 is_drivedge;



/*************************************
 *
 *	Interrupt handling
 *
 *************************************/

INLINE int determine_irq_state(int vint, int xint, int qint)
{
	int level = 0;

	/* update the states */
	if (vint != -1) vint_state = vint;
	if (xint != -1) xint_state = xint;
	if (qint != -1) qint_state = qint;

	/* determine which level is active */
	if (vint_state) level = 1;
	if (xint_state) level = 2;
	if (qint_state) level = 3;

	/* Driver's Edge shifts the interrupts a bit */
	if (is_drivedge && level) level += 2;

	return level;
}


void itech32_update_interrupts(int vint, int xint, int qint)
{
	int level = determine_irq_state(vint, xint, qint);

	/* update it */
	if (level)
		cpu_set_irq_line(0, level, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static INTERRUPT_GEN( generate_int1 )
{
	/* signal the NMI */
	itech32_update_interrupts(1, -1, -1);
	if (FULL_LOGGING) logerror("------------ VBLANK (%d) --------------", cpu_getscanline());
}


static WRITE16_HANDLER( int1_ack_w )
{
	itech32_update_interrupts(0, -1, -1);
}



/*************************************
 *
 *	Machine initialization
 *
 *************************************/

static void via6522_timer_callback(int which);

static MACHINE_INIT( itech32 )
{
	vint_state = xint_state = qint_state = 0;
	sound_data = 0;
	sound_int_state = 0;

	/* reset the VIA chip (if used) */
	via6522_timer_count[0] = via6522_timer_count[1] = 0;
	via6522_timer[0] = timer_alloc(via6522_timer_callback);
	via6522_timer[1] = 0;
	via6522_int_state = 0;

	/* reset the ticket dispenser */
	ticket_dispenser_init(200, TICKET_MOTOR_ACTIVE_HIGH, TICKET_STATUS_ACTIVE_HIGH);

	/* map the mirrored RAM in Driver's Edge */
	if (is_drivedge)
		cpu_setbank(2, main_ram);
}



/*************************************
 *
 *	Input handlers
 *
 *************************************/

static READ16_HANDLER( special_port3_r )
{
	int result = readinputport(3);
	if (sound_int_state) result ^= 0x08;
	return result;
}


static READ16_HANDLER( special_port4_r )
{
	int result = readinputport(4);
	if (sound_int_state) result ^= 0x08;
	return result;
}


static READ16_HANDLER( trackball_r )
{
	int lower = readinputport(6);
	int upper = readinputport(7);

	return (lower & 15) | ((upper & 15) << 4);
}


static READ32_HANDLER( trackball32_8bit_r )
{
	int lower = readinputport(6);
	int upper = readinputport(7);

	return (lower & 255) | ((upper & 255) << 8);
}


static READ32_HANDLER( trackball32_4bit_r )
{
	static int effx, effy;
	static int lastresult;
	static double lasttime;
	double curtime = timer_get_time();

	if (curtime - lasttime > cpu_getscanlineperiod())
	{
		int upper, lower;
		int dx, dy;

		int curx = readinputport(6);
		int cury = readinputport(7);

		dx = curx - effx;
		if (dx < -0x80) dx += 0x100;
		else if (dx > 0x80) dx -= 0x100;
		if (dx > 7) dx = 7;
		else if (dx < -7) dx = -7;
		effx = (effx + dx) & 0xff;
		lower = effx & 15;

		dy = cury - effy;
		if (dy < -0x80) dy += 0x100;
		else if (dy > 0x80) dy -= 0x100;
		if (dy > 7) dy = 7;
		else if (dy < -7) dy = -7;
		effy = (effy + dy) & 0xff;
		upper = effy & 15;

		lastresult = lower | (upper << 4);
	}

	lasttime = curtime;
	return lastresult | (lastresult << 16);
}


static READ32_HANDLER( trackball32_4bit_p2_r )
{
	static int effx, effy;
	static int lastresult;
	static double lasttime;
	double curtime = timer_get_time();

	if (curtime - lasttime > cpu_getscanlineperiod())
	{
		int upper, lower;
		int dx, dy;

		int curx = readinputport(8);
		int cury = readinputport(9);

		dx = curx - effx;
		if (dx < -0x80) dx += 0x100;
		else if (dx > 0x80) dx -= 0x100;
		if (dx > 7) dx = 7;
		else if (dx < -7) dx = -7;
		effx = (effx + dx) & 0xff;
		lower = effx & 15;

		dy = cury - effy;
		if (dy < -0x80) dy += 0x100;
		else if (dy > 0x80) dy -= 0x100;
		if (dy > 7) dy = 7;
		else if (dy < -7) dy = -7;
		effy = (effy + dy) & 0xff;
		upper = effy & 15;

		lastresult = lower | (upper << 4);
	}

	lasttime = curtime;
	return lastresult | (lastresult << 16);
}



/*************************************
 *
 *	Protection?
 *
 *************************************/

static READ16_HANDLER( wcbowl_prot_result_r )
{
	return main_ram[0x111d/2];
}


static READ32_HANDLER( itech020_prot_result_r )
{
	data32_t result = ((data32_t *)main_ram)[itech020_prot_address >> 2];
	result >>= (~itech020_prot_address & 3) * 8;
	return (result & 0xff) << 8;
}



/*************************************
 *
 *	Sound banking
 *
 *************************************/

static WRITE_HANDLER( sound_bank_w )
{
	logerror("sound bank = %02x", data);
	cpu_setbank(1, &memory_region(REGION_CPU2)[0x10000 + data * 0x4000]);
}



/*************************************
 *
 *	Sound communication
 *
 *************************************/

static void delayed_sound_data_w(int data)
{
	sound_data = data;
	sound_int_state = 1;
	cpu_set_irq_line(1, M6809_IRQ_LINE, ASSERT_LINE);
	logerror("sound_data_w() = %02x", sound_data);
}


static WRITE16_HANDLER( sound_data_w )
{
	if (ACCESSING_LSB)
		timer_set(TIME_NOW, data & 0xff, delayed_sound_data_w);
}


static WRITE32_HANDLER( sound_data32_w )
{
	if (!(mem_mask & 0x00ff0000))
		timer_set(TIME_NOW, (data >> 16) & 0xff, delayed_sound_data_w);
}


static READ_HANDLER( sound_data_r )
{
	logerror("sound_data_r() = %02x", sound_data);
	cpu_set_irq_line(1, M6809_IRQ_LINE, CLEAR_LINE);
	sound_int_state = 0;
	return sound_data;
}


static READ_HANDLER( sound_data_buffer_r )
{
	return 0;
}



/*************************************
 *
 *	Sound I/O port handling
 *
 *************************************/

static WRITE_HANDLER( pia_portb_out )
{
	logerror("PIA port B write = %02x", data);

	/* bit 4 controls the ticket dispenser */
	/* bit 5 controls the coin counter */
	/* bit 6 controls the diagnostic sound LED */
	ticket_dispenser_w(0, (data & 0x10) << 3);
	coin_counter_w(0, (data & 0x20) >> 5);
}


static WRITE_HANDLER( sound_output_w )
{
	logerror("sound output write = %02x", data);

	coin_counter_w(0, (~data & 0x20) >> 5);
}



/*************************************
 *
 *	Sound 6522 VIA handling
 *
 *************************************/

INLINE void update_via_int(void)
{
	/* if interrupts are enabled and one is pending, set the line */
	if ((via6522[14] & 0x80) && (via6522_int_state & via6522[14]))
		cpu_set_irq_line(1, M6809_FIRQ_LINE, ASSERT_LINE);
	else
		cpu_set_irq_line(1, M6809_FIRQ_LINE, CLEAR_LINE);
}


static void via6522_timer_callback(int which)
{
	via6522_int_state |= 0x40 >> which;
	update_via_int();
}


static WRITE_HANDLER( via6522_w )
{
	double period;

	/* update the data */
	via6522[offset] = data;

	/* switch off the offset */
	switch (offset)
	{
		case 0:		/* write to port B */
			pia_portb_out(0, data);
			break;

		case 5:		/* write into high order timer 1 */
			via6522_timer_count[0] = (via6522[5] << 8) | via6522[4];
			period = TIME_IN_HZ(CLOCK_8MHz/4) * (double)via6522_timer_count[0];
			timer_adjust(via6522_timer[0], period, 0, period);

			via6522_int_state &= ~0x40;
			update_via_int();
			break;

		case 13:	/* write interrupt flag register */
			via6522_int_state &= ~data;
			update_via_int();
			break;

		default:	/* log everything else */
			if (FULL_LOGGING) logerror("VIA write(%02x) = %02x", offset, data);
			break;
	}

}


static READ_HANDLER( via6522_r )
{
	int result = 0;

	/* switch off the offset */
	switch (offset)
	{
		case 4:		/* read low order timer 1 */
			via6522_int_state &= ~0x40;
			update_via_int();
			break;

		case 13:	/* interrupt flag register */
			result = via6522_int_state & 0x7f;
			if (via6522_int_state & via6522[14]) result |= 0x80;
			break;
	}

	if (FULL_LOGGING) logerror("VIA read(%02x) = %02x", offset, result);
	return result;
}



/*************************************
 *
 *	Additional sound code
 *
 *************************************/

static WRITE_HANDLER( firq_clear_w )
{
	cpu_set_irq_line(1, M6809_FIRQ_LINE, CLEAR_LINE);
}



/*************************************
 *
 *	Speedups
 *
 *************************************/

static READ_HANDLER( sound_speedup_r )
{
	if (sound_speedup_data[0] == sound_speedup_data[1] && activecpu_get_previouspc() == sound_speedup_pc)
		cpu_spinuntil_int();
	return sound_speedup_data[0];
}


static WRITE32_HANDLER( itech020_watchdog_w )
{
	watchdog_reset_w(0,0);
}



/*************************************
 *
 *	32-bit shunts
 *
 *************************************/

static READ32_HANDLER( input_port_0_msw_r )
{
	return input_port_0_word_r(offset,0) << 16;
}

static READ32_HANDLER( input_port_1_msw_r )
{
	return input_port_1_word_r(offset,0) << 16;
}

static READ32_HANDLER( input_port_2_msw_r )
{
	return input_port_2_word_r(offset,0) << 16;
}

static READ32_HANDLER( input_port_3_msw_r )
{
	return input_port_3_word_r(offset,0) << 16;
}

static READ32_HANDLER( input_port_4_msw_r )
{
	return special_port4_r(offset,0) << 16;
}

static READ32_HANDLER( input_port_5_msw_r )
{
	return input_port_5_word_r(offset,0) << 16;
}

static WRITE32_HANDLER( int1_ack32_w )
{
	int1_ack_w(offset, data, mem_mask);
}



/*************************************
 *
 *	NVRAM read/write
 *
 *************************************/

static NVRAM_HANDLER( itech32 )
{
	int i;

	if (read_or_write)
		mame_fwrite(file, main_ram, main_ram_size);
	else if (file)
		mame_fread(file, main_ram, main_ram_size);
	else
		for (i = 0x80; i < main_ram_size; i++)
			((UINT8 *)main_ram)[i] = rand();
}


static NVRAM_HANDLER( itech020 )
{
	int i;

	if (read_or_write)
		mame_fwrite(file, nvram, nvram_size);
	else if (file)
		mame_fread(file, nvram, nvram_size);
	else
		for (i = 0; i < nvram_size; i++)
			((UINT8 *)nvram)[i] = rand();
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

/*------ Time Killers memory layout ------*/
static MEMORY_READ16_START( timekill_readmem )
	{ 0x000000, 0x003fff, MRA16_RAM },
	{ 0x040000, 0x040001, input_port_0_word_r },
	{ 0x048000, 0x048001, input_port_1_word_r },
	{ 0x050000, 0x050001, input_port_2_word_r },
	{ 0x058000, 0x058001, special_port3_r },
	{ 0x080000, 0x08007f, itech32_video_r },
	{ 0x0c0000, 0x0c7fff, MRA16_RAM },
	{ 0x100000, 0x17ffff, MRA16_ROM },
MEMORY_END


static MEMORY_WRITE16_START( timekill_writemem )
	{ 0x000000, 0x003fff, MWA16_RAM, &main_ram, &main_ram_size },
	{ 0x050000, 0x050001, timekill_intensity_w },
	{ 0x058000, 0x058001, watchdog_reset16_w },
	{ 0x060000, 0x060001, timekill_colora_w },
	{ 0x068000, 0x068001, timekill_colorbc_w },
	{ 0x070000, 0x070001, MWA16_NOP },	/* noisy */
	{ 0x078000, 0x078001, sound_data_w },
	{ 0x080000, 0x08007f, itech32_video_w, &itech32_video },
	{ 0x0a0000, 0x0a0001, int1_ack_w },
	{ 0x0c0000, 0x0c7fff, timekill_paletteram_w, &paletteram16 },
	{ 0x100000, 0x17ffff, MWA16_ROM, &main_rom },
MEMORY_END


/*------ BloodStorm and later games memory layout ------*/
static MEMORY_READ16_START( bloodstm_readmem )
	{ 0x000000, 0x00ffff, MRA16_RAM },
	{ 0x080000, 0x080001, input_port_0_word_r },
	{ 0x100000, 0x100001, input_port_1_word_r },
	{ 0x180000, 0x180001, input_port_2_word_r },
	{ 0x200000, 0x200001, input_port_3_word_r },
	{ 0x280000, 0x280001, special_port4_r },
	{ 0x500000, 0x5000ff, bloodstm_video_r },
	{ 0x580000, 0x59ffff, MRA16_RAM },
	{ 0x780000, 0x780001, input_port_5_word_r },
	{ 0x800000, 0x87ffff, MRA16_ROM },
MEMORY_END


static MEMORY_WRITE16_START( bloodstm_writemem )
	{ 0x000000, 0x00ffff, MWA16_RAM, &main_ram, &main_ram_size },
	{ 0x080000, 0x080001, int1_ack_w },
	{ 0x200000, 0x200001, watchdog_reset16_w },
	{ 0x300000, 0x300001, bloodstm_color1_w },
	{ 0x380000, 0x380001, bloodstm_color2_w },
	{ 0x400000, 0x400001, watchdog_reset16_w },
	{ 0x480000, 0x480001, sound_data_w },
	{ 0x500000, 0x5000ff, bloodstm_video_w, &itech32_video },
	{ 0x580000, 0x59ffff, bloodstm_paletteram_w, &paletteram16 },
	{ 0x700000, 0x700001, bloodstm_plane_w },
	{ 0x800000, 0x87ffff, MWA16_ROM, &main_rom },
MEMORY_END


/*------ Pairs memory layout ------*/
static MEMORY_READ16_START( pairs_readmem )
	{ 0x000000, 0x00ffff, MRA16_RAM },
	{ 0x080000, 0x080001, input_port_0_word_r },
	{ 0x100000, 0x100001, input_port_1_word_r },
	{ 0x180000, 0x180001, input_port_2_word_r },
	{ 0x200000, 0x200001, input_port_3_word_r },
	{ 0x280000, 0x280001, special_port4_r },
	{ 0x500000, 0x5000ff, bloodstm_video_r },
	{ 0x580000, 0x59ffff, MRA16_RAM },
	{ 0x780000, 0x780001, input_port_5_word_r },
	{ 0xd00000, 0xd7ffff, MRA16_ROM },
MEMORY_END


static MEMORY_WRITE16_START( pairs_writemem )
	{ 0x000000, 0x00ffff, MWA16_RAM, &main_ram, &main_ram_size },
	{ 0x080000, 0x080001, int1_ack_w },
	{ 0x200000, 0x200001, watchdog_reset16_w },
	{ 0x300000, 0x300001, bloodstm_color1_w },
	{ 0x380000, 0x380001, bloodstm_color2_w },
	{ 0x400000, 0x400001, watchdog_reset16_w },
	{ 0x480000, 0x480001, sound_data_w },
	{ 0x500000, 0x5000ff, bloodstm_video_w, &itech32_video },
	{ 0x580000, 0x59ffff, bloodstm_paletteram_w, &paletteram16 },
	{ 0x700000, 0x700001, bloodstm_plane_w },
	{ 0xd00000, 0xd7ffff, MWA16_ROM, &main_rom },
MEMORY_END


/*------ Driver's Edge memory layout ------*/
static MEMORY_READ32_START( drivedge_readmem )
	{ 0x000000, 0x03ffff, MRA32_RAM },
	{ 0x040000, 0x07ffff, MRA32_BANK2 },
	{ 0x08c000, 0x08c003, input_port_0_msw_r },
	{ 0x08e000, 0x08e003, input_port_1_msw_r },
	{ 0x1a0000, 0x1bffff, MRA32_RAM },
	{ 0x1e0000, 0x1e00ff, itech020_video_r },
	{ 0x200000, 0x200003, input_port_2_msw_r },
	{ 0x280000, 0x280fff, MRA32_RAM },
	{ 0x300000, 0x300fff, MRA32_RAM },
	{ 0x600000, 0x607fff, MRA32_ROM },
MEMORY_END


static MEMORY_WRITE32_START( drivedge_writemem )
	{ 0x000000, 0x03ffff, MWA32_RAM, (data32_t **)&main_ram, &main_ram_size },
	{ 0x040000, 0x07ffff, MWA32_BANK2 },
	{ 0x084000, 0x084003, sound_data32_w },
//	{ 0x100000, 0x10000f, ???_w },	= 4 longwords (TMS control?)
	{ 0x180000, 0x180003, drivedge_color0_w },
	{ 0x1a0000, 0x1bffff, itech020_paletteram_w, &paletteram32 },
//	{ 0x1c0000, 0x1c0001, ???_w },	= 0x64
	{ 0x1e0000, 0x1e00ff, itech020_video_w, (data32_t **)&itech32_video },
//	{ 0x1e4000, 0x1e4003, ???_w },	= 0x1ffff
	{ 0x280000, 0x280fff, MWA32_RAM },	// initialized to zero
	{ 0x300000, 0x300fff, MWA32_RAM },	// initialized to zero
	{ 0x380000, 0x380003, MWA32_NOP },	// watchdog
	{ 0x600000, 0x607fff, MWA32_ROM, (data32_t **)&main_rom },
MEMORY_END

// 0x10000c/0/4/8 = $8000/$0/$0/$ffff1e
// 0x100008/c     = $ffffff/$8000


/*------ 68EC020-based memory layout ------*/
static MEMORY_READ32_START( itech020_readmem )
	{ 0x000000, 0x007fff, MRA32_RAM },
	{ 0x080000, 0x080003, input_port_0_msw_r },
	{ 0x100000, 0x100003, input_port_1_msw_r },
	{ 0x180000, 0x180003, input_port_2_msw_r },
	{ 0x200000, 0x200003, input_port_3_msw_r },
	{ 0x280000, 0x280003, input_port_4_msw_r },
	{ 0x500000, 0x5000ff, itech020_video_r },
	{ 0x578000, 0x57ffff, MRA32_NOP },				/* touched by protection */
	{ 0x580000, 0x59ffff, MRA32_RAM },
	{ 0x600000, 0x603fff, MRA32_RAM },
	{ 0x680000, 0x680003, itech020_prot_result_r },
	{ 0x800000, 0x9fffff, MRA32_ROM },
MEMORY_END


static MEMORY_WRITE32_START( itech020_writemem )
	{ 0x000000, 0x007fff, MWA32_RAM, (data32_t **)&main_ram, &main_ram_size },
	{ 0x080000, 0x080003, int1_ack32_w },
	{ 0x300000, 0x300003, itech020_color1_w },
	{ 0x380000, 0x380003, itech020_color2_w },
	{ 0x400000, 0x400003, itech020_watchdog_w },
	{ 0x480000, 0x480003, sound_data32_w },
	{ 0x500000, 0x5000ff, itech020_video_w, (data32_t **)&itech32_video },
	{ 0x580000, 0x59ffff, itech020_paletteram_w, &paletteram32 },
	{ 0x600000, 0x603fff, MWA32_RAM, &nvram, &nvram_size },
	{ 0x680000, 0x680003, MWA32_NOP },				/* written by protection */
	{ 0x700000, 0x700003, itech020_plane_w },
	{ 0x800000, 0x9fffff, MWA32_ROM, (data32_t **)&main_rom },
MEMORY_END



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

/*------ Rev 1 sound board memory layout ------*/
static MEMORY_READ_START( sound_readmem )
	{ 0x0400, 0x0400, sound_data_r },
	{ 0x0800, 0x083f, ES5506_data_0_r },
	{ 0x0880, 0x08bf, ES5506_data_0_r },
	{ 0x1400, 0x140f, via6522_r },
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END


static MEMORY_WRITE_START( sound_writemem )
	{ 0x0800, 0x083f, ES5506_data_0_w },
	{ 0x0880, 0x08bf, ES5506_data_0_w },
	{ 0x0c00, 0x0c00, sound_bank_w },
	{ 0x1000, 0x1000, MWA_NOP },	/* noisy */
	{ 0x1400, 0x140f, via6522_w, &via6522 },
	{ 0x2000, 0x3fff, MWA_RAM },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END


/*------ Rev 2 sound board memory layout ------*/
static MEMORY_READ_START( sound_020_readmem )
	{ 0x0000, 0x0000, sound_data_r },
	{ 0x0400, 0x0400, sound_data_r },
	{ 0x0800, 0x083f, ES5506_data_0_r },
	{ 0x0880, 0x08bf, ES5506_data_0_r },
	{ 0x1800, 0x1800, sound_data_buffer_r },
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END


static MEMORY_WRITE_START( sound_020_writemem )
	{ 0x0800, 0x083f, ES5506_data_0_w },
	{ 0x0880, 0x08bf, ES5506_data_0_w },
	{ 0x0c00, 0x0c00, sound_bank_w },
	{ 0x1400, 0x1400, firq_clear_w },
	{ 0x1800, 0x1800, MWA_NOP },
	{ 0x1c00, 0x1c00, sound_output_w },
	{ 0x2000, 0x3fff, MWA_RAM },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( timekill )
	PORT_START	/* 40000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	/* 48000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_START	/* 50000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 58000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_DIPNAME( 0x0010, 0x0000, "Video Sync" )
	PORT_DIPSETTING(      0x0000, "-" )
	PORT_DIPSETTING(      0x0010, "+" )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0000, "Violence" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0080, DEF_STR( On ))
INPUT_PORTS_END


INPUT_PORTS_START( bloodstm )
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_START	/* 180000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_DIPNAME( 0x0010, 0x0000, "Video Sync" )
	PORT_DIPSETTING(      0x0000, "-" )
	PORT_DIPSETTING(      0x0010, "+" )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0000, "Violence" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0080, DEF_STR( On ))

	PORT_START	/* 780000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER2 )
	PORT_BIT( 0x00c0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( hardyard )
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_START	/* 180000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 )

	PORT_START	/* 200000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER4 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_DIPNAME( 0x0010, 0x0000, "Video Sync" )
	PORT_DIPSETTING(      0x0000, "-" )
	PORT_DIPSETTING(      0x0010, "+" )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Cabinet ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ))
	PORT_DIPNAME( 0x0040, 0x0000, "Players" )
	PORT_DIPSETTING(      0x0000, "4" )
	PORT_DIPSETTING(      0x0040, "2" )
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0080, DEF_STR( On ))

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( pairs )
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_START	/* 180000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_DIPNAME( 0x0010, 0x0000, "Video Sync" )
	PORT_DIPSETTING(      0x0000, "-" )
	PORT_DIPSETTING(      0x0010, "+" )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Flip_Screen ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0000, "Modesty" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0040, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0080, DEF_STR( On ))

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( wcbowl )
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_START	/* 180000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Flip_Screen ) )	/* Verified */
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0040, DEF_STR( On ))
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0080, DEF_STR( On ))

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1 | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1, 25, 32, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( wcbowln ) /* WCB version 1.66 supports cocktail mode */
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 | IPF_COCKTAIL )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 | IPF_COCKTAIL )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 180000 */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x00fb, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0010, 0x0000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Cabinet ) )	// v1.66 Rom sets support Cocktail mode (verified)
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0040, 0x0000, "Freeze Screen" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1 | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2 | IPF_COCKTAIL | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER2 | IPF_COCKTAIL, 25, 32, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( drivedge )
	PORT_START	/* 40000 */
	PORT_BIT ( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT ( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BITX( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON7 | IPF_PLAYER1, "Gear 1", KEYCODE_Z, IP_JOY_DEFAULT )
	PORT_BITX( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON8 | IPF_PLAYER1, "Gear 2", KEYCODE_X, IP_JOY_DEFAULT )
	PORT_BITX( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON9 | IPF_PLAYER1, "Gear 3", KEYCODE_C, IP_JOY_DEFAULT )
	PORT_BITX( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON10 | IPF_PLAYER1, "Gear 4", KEYCODE_V, IP_JOY_DEFAULT )
	PORT_SERVICE_NO_TOGGLE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT ( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 48000 */
	PORT_BITX( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1, "Fan",         KEYCODE_F, IP_JOY_DEFAULT )
	PORT_BITX( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1, "Tow Truck",   KEYCODE_T, IP_JOY_DEFAULT )
	PORT_BITX( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1, "Horn",        KEYCODE_SPACE, IP_JOY_DEFAULT )
	PORT_BITX( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1, "Turbo Boost", KEYCODE_Z, IP_JOY_DEFAULT )
	PORT_BITX( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1, "Network On",  KEYCODE_N, IP_JOY_DEFAULT )
	PORT_BITX( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1, "Key",         KEYCODE_K, IP_JOY_DEFAULT )
	PORT_BIT ( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT ( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 48000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER3 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 )

	PORT_START	/* 50000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 58000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_DIPNAME( 0x0070, 0x0000, "Network Number" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0020, "3" )
	PORT_DIPSETTING(      0x0030, "4" )
	PORT_DIPSETTING(      0x0040, "5" )
	PORT_DIPSETTING(      0x0050, "6" )
	PORT_DIPSETTING(      0x0060, "7" )
	PORT_DIPSETTING(      0x0070, "8" )
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( sftm )
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )

	PORT_START	/* 180000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON4        | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5        | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON6        | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON6        | IPF_PLAYER2 )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0010, 0x0000, "Video Sync" )
	PORT_DIPSETTING(      0x0000, "-" )
	PORT_DIPSETTING(      0x0010, "+" )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0000, "Freeze Screen" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( shufshot ) /* ShuffleShot version 1.39 supports cocktail mode */
	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 | IPF_COCKTAIL )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 | IPF_COCKTAIL )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 180000 */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x00fb, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0010, 0x0000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Cabinet ) )	/* Verified */
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1 | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2 | IPF_COCKTAIL | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER2 | IPF_COCKTAIL, 25, 32, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( shufbowl )
	/*
	Earlier versions of Shuffleshot & World Class Bowling share the same input
	port set up. IE: "Freeze Screen" and no support for a cocktail mode
	*/

	PORT_START	/* 080000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 100000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 | IPF_COCKTAIL )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 | IPF_COCKTAIL )
	PORT_BIT( 0x00f0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 180000 */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x00fb, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 200000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 280000 */
	PORT_SERVICE_NO_TOGGLE( 0x0001, IP_ACTIVE_LOW )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x0010, 0x0000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0000, DEF_STR( Flip_Screen ) )	/* Verified */
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0000, "Freeze Screen" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0000, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )

	PORT_START	/* 780000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER1 | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_PLAYER2 | IPF_COCKTAIL | IPF_REVERSE, 25, 32, 0, 255 )

	PORT_START	/* analog */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER2 | IPF_COCKTAIL, 25, 32, 0, 255 )
INPUT_PORTS_END


/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct ES5506interface es5506_interface =
{
	1,
	{ 16000000 },
	{ REGION_SOUND1 },
	{ REGION_SOUND2 },
	{ REGION_SOUND3 },
	{ REGION_SOUND4 },
	{ YM3012_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },
	{ 0 }
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static MACHINE_DRIVER_START( timekill )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, CLOCK_12MHz)
	MDRV_CPU_MEMORY(timekill_readmem,timekill_writemem)
	MDRV_CPU_VBLANK_INT(generate_int1,1)

	MDRV_CPU_ADD_TAG("sound", M6809, CLOCK_8MHz/4)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION((int)(((263. - 240.) / 263.) * 1000000. / 60.))

	MDRV_MACHINE_INIT(itech32)
	MDRV_NVRAM_HANDLER(itech32)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_BEFORE_VBLANK)
	MDRV_SCREEN_SIZE(384,256)
	MDRV_VISIBLE_AREA(0, 383, 0, 239)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(itech32)
	MDRV_VIDEO_UPDATE(itech32)

	/* sound hardware */
	MDRV_SOUND_ADD(ES5506, es5506_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( bloodstm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(timekill)

	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(bloodstm_readmem,bloodstm_writemem)

	/* video hardware */
	MDRV_PALETTE_LENGTH(32768)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( pairs )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(bloodstm)

	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(pairs_readmem,pairs_writemem)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( wcbowl )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(bloodstm)

	/* video hardware */
	MDRV_VISIBLE_AREA(0, 383, 0, 254)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( drivedge )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(bloodstm)

	MDRV_CPU_REPLACE("main", M68EC020, CLOCK_25MHz)
	MDRV_CPU_MEMORY(drivedge_readmem,drivedge_writemem)
	MDRV_CPU_VBLANK_INT(NULL,0)

	MDRV_NVRAM_HANDLER(itech020)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( sftm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(bloodstm)

	MDRV_CPU_REPLACE("main", M68EC020, CLOCK_25MHz)
	MDRV_CPU_MEMORY(itech020_readmem,itech020_writemem)

	MDRV_CPU_MODIFY("sound")
	MDRV_CPU_MEMORY(sound_020_readmem,sound_020_writemem)
	MDRV_CPU_VBLANK_INT(irq1_line_assert,4)

	MDRV_NVRAM_HANDLER(itech020)

	/* video hardware */
	MDRV_VISIBLE_AREA(0, 383, 0, 254)
MACHINE_DRIVER_END



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( timekill )
	ROM_REGION( 0x4000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "tk00v132.u54", 0x00000, 0x40000, CRC(68c74b81) SHA1(acdf677f82d7428acc6cf01076d43dd6330e9cb3) )
	ROM_LOAD16_BYTE( "tk01v132.u53", 0x00001, 0x40000, CRC(2158d8ef) SHA1(14aa66e020a9fa890fadbaf0936dfdc4e272f543) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "tksndv41.u17", 0x10000, 0x18000, CRC(c699af7b) SHA1(55863513a1c27dcb257dbc20e522cfafa9b92c9d) )
	ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "tk0_rom.1", 0x000000, 0x200000, CRC(94cbf6f8) SHA1(dac5c4d9c8e42336c236ecc3c72b3b1f8282dc2f) )
	ROM_LOAD32_BYTE( "tk1_rom.2", 0x000001, 0x200000, CRC(c07dea98) SHA1(dd8b88beb9781579eb0a17231ad2a274b70ae1bc) )
	ROM_LOAD32_BYTE( "tk2_rom.3", 0x000002, 0x200000, CRC(183eed2a) SHA1(3905268fe45ecc47cd4d349666b4d33efda2140b) )
	ROM_LOAD32_BYTE( "tk3_rom.4", 0x000003, 0x200000, CRC(b1da1058) SHA1(a1d483701c661d69cecc9d073b23683b119f5ef1) )
	ROM_LOAD32_BYTE( "tkgrom.01", 0x800000, 0x020000, CRC(b030c3d9) SHA1(f5c21285ec8ff4f74205e0cf18da67e733e31183) )
	ROM_LOAD32_BYTE( "tkgrom.02", 0x800001, 0x020000, CRC(e98492a4) SHA1(fe8fb4bd3900109f3872f2930e8ddc9d19f599fd) )
	ROM_LOAD32_BYTE( "tkgrom.03", 0x800002, 0x020000, CRC(6088fa64) SHA1(a3eee10bdef48fefec3836f551172dbe0819acf6) )
	ROM_LOAD32_BYTE( "tkgrom.04", 0x800003, 0x020000, CRC(95be2318) SHA1(60580c87d63a114df44e2580e138128388ff447b) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "tksrom00.u18", 0x000000, 0x80000, CRC(79d8b83a) SHA1(78934b4d0ccca8fefcf8277e4296eb1d59cd575b) )
	ROM_LOAD16_BYTE( "tksrom01.u20", 0x100000, 0x80000, CRC(ec01648c) SHA1(b83c66cf22db5d89b9ed79b79861b79429d8380c) )
	ROM_LOAD16_BYTE( "tksrom02.u26", 0x200000, 0x80000, CRC(051ced3e) SHA1(6b63c4837e709806ffea9a37d93933635d356a6e) )
ROM_END


ROM_START( timek131 )
	ROM_REGION( 0x4000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "tk00v131.u54", 0x00000, 0x40000, CRC(e09ae32b) SHA1(b090a38600d0499f7b4cb80a2715f27216d408b0) )
	ROM_LOAD16_BYTE( "tk01v131.u53", 0x00001, 0x40000, CRC(c29137ec) SHA1(4dcfba13b6f865a256bcb0406b6c83c309b17313) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "tksnd.u17", 0x10000, 0x18000, CRC(ab1684c3) SHA1(cc7e591fd160b259f8aecddb2c5a3c36e4e37b2f) )
	ROM_CONTINUE(          0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "tk0_rom.1", 0x000000, 0x200000, CRC(94cbf6f8) SHA1(dac5c4d9c8e42336c236ecc3c72b3b1f8282dc2f) )
	ROM_LOAD32_BYTE( "tk1_rom.2", 0x000001, 0x200000, CRC(c07dea98) SHA1(dd8b88beb9781579eb0a17231ad2a274b70ae1bc) )
	ROM_LOAD32_BYTE( "tk2_rom.3", 0x000002, 0x200000, CRC(183eed2a) SHA1(3905268fe45ecc47cd4d349666b4d33efda2140b) )
	ROM_LOAD32_BYTE( "tk3_rom.4", 0x000003, 0x200000, CRC(b1da1058) SHA1(a1d483701c661d69cecc9d073b23683b119f5ef1) )
	ROM_LOAD32_BYTE( "tkgrom.01", 0x800000, 0x020000, CRC(b030c3d9) SHA1(f5c21285ec8ff4f74205e0cf18da67e733e31183) )
	ROM_LOAD32_BYTE( "tkgrom.02", 0x800001, 0x020000, CRC(e98492a4) SHA1(fe8fb4bd3900109f3872f2930e8ddc9d19f599fd) )
	ROM_LOAD32_BYTE( "tkgrom.03", 0x800002, 0x020000, CRC(6088fa64) SHA1(a3eee10bdef48fefec3836f551172dbe0819acf6) )
	ROM_LOAD32_BYTE( "tkgrom.04", 0x800003, 0x020000, CRC(95be2318) SHA1(60580c87d63a114df44e2580e138128388ff447b) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "tksrom00.u18", 0x000000, 0x80000, CRC(79d8b83a) SHA1(78934b4d0ccca8fefcf8277e4296eb1d59cd575b) )
	ROM_LOAD16_BYTE( "tksrom01.u20", 0x100000, 0x80000, CRC(ec01648c) SHA1(b83c66cf22db5d89b9ed79b79861b79429d8380c) )
	ROM_LOAD16_BYTE( "tksrom02.u26", 0x200000, 0x80000, CRC(051ced3e) SHA1(6b63c4837e709806ffea9a37d93933635d356a6e) )
ROM_END


ROM_START( bloodstm )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "bld02.bin", 0x00000, 0x40000, CRC(95f36db6) SHA1(72ec5ca93aed8fb12d5e5b7ff3d07c5cf1dab4bb) )
	ROM_LOAD16_BYTE( "bld01.bin", 0x00001, 0x40000, CRC(fcc04b93) SHA1(7029d68f20196b6c2c30339500c7da54f2b5b054) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "bldsnd.u17", 0x10000, 0x18000, CRC(dddeedbb) SHA1(f8ea786836630fc44bba968845fd2cb42cd81592) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "bsgrom0.bin",  0x000000, 0x080000, CRC(4e10b8c1) SHA1(b80f7dbf32faa97829c735da6dc0ee424dc9ecec) )
	ROM_LOAD32_BYTE( "bsgrom5.bin",  0x000001, 0x080000, CRC(6333b6ce) SHA1(53b884e09d198f8f53449bd011ba743c28b2c934) )
	ROM_LOAD32_BYTE( "bsgrom10.bin", 0x000002, 0x080000, CRC(a972a65c) SHA1(7772c2e0aaa0b3183c6287125b95fd1cd0c3775a) )
	ROM_LOAD32_BYTE( "bsgrom15.bin", 0x000003, 0x080000, CRC(9a8f54aa) SHA1(3a2f99c28324ba1fcfb652bdc596d94c90637b72) )
	ROM_LOAD32_BYTE( "bsgrom1.bin",  0x200000, 0x080000, CRC(10abf660) SHA1(e5b28ee12972abbcd883d702f496feae62d19e07) )
	ROM_LOAD32_BYTE( "bsgrom6.bin",  0x200001, 0x080000, CRC(06a260d5) SHA1(f506b799bbb5d4465a4ab564cfd276877b8a52e9) )
	ROM_LOAD32_BYTE( "bsgrom11.bin", 0x200002, 0x080000, CRC(f2cab3c7) SHA1(eecb1f2f8a3f3b1663a51c343e28e1b2078da5f4) )
	ROM_LOAD32_BYTE( "bsgrom16.bin", 0x200003, 0x080000, CRC(403aef7b) SHA1(0a2a61480cddd020bfbad01c6c328dfa6760bddd) )
	ROM_LOAD32_BYTE( "bsgrom2.bin",  0x400000, 0x080000, CRC(488200b1) SHA1(ae25bb5e9a836ae362d88de4e392ff46ff93c636) )
	ROM_LOAD32_BYTE( "bsgrom7.bin",  0x400001, 0x080000, CRC(5bb19727) SHA1(fd8aa73ff0606ac86f054794f6abc42f99b4f856) )
	ROM_LOAD32_BYTE( "bsgrom12.bin", 0x400002, 0x080000, CRC(b10d674f) SHA1(f5c713480deac44b86173ca9ac3aeea8a4d2ac85) )
	ROM_LOAD32_BYTE( "bsgrom17.bin", 0x400003, 0x080000, CRC(7119df7e) SHA1(67acb0525f21d5cd174528fb0fa72c7101deb5eb) )
	ROM_LOAD32_BYTE( "bsgrom3.bin",  0x600000, 0x080000, CRC(2378792e) SHA1(8cf1040a86ce7b6be3f56d144d4a17b7a888a648) )
	ROM_LOAD32_BYTE( "bsgrom8.bin",  0x600001, 0x080000, CRC(3640ca2e) SHA1(1fbc8306c7310ab23d40887c16f80e2dc3d730f8) )
	ROM_LOAD32_BYTE( "bsgrom13.bin", 0x600002, 0x080000, CRC(bd4a071d) SHA1(f25483591659d7424a4c62d0093fa56e5d337bf3) )
	ROM_LOAD32_BYTE( "bsgrom18.bin", 0x600003, 0x080000, CRC(12959bb8) SHA1(f74d407004ccbf461f749672e4e57a5f0b6b549f) )
	ROM_FILL(                        0x800000, 0x080000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "enssnd2m.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "bssrom0.bin",  0x000000, 0x80000, CRC(ee4570c8) SHA1(73dd292224bf182770b3cc2d90eb52b7d7b24378) )
	ROM_LOAD16_BYTE( "bssrom1.bin",  0x100000, 0x80000, CRC(b0f32ec5) SHA1(666f904b31cdef12cbf1dbb43a7d3ff7c2903260) )
	ROM_LOAD16_BYTE( "bssrom2.bin",  0x300000, 0x40000, CRC(8aee1e77) SHA1(f949fa89ee7d59f457ce89c72d461cecd0cface3) )
ROM_END


ROM_START( bloods22 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "bld00v22.u83", 0x00000, 0x40000, CRC(904e9208) SHA1(12e96027724b905140250db969130d90b1afec83) )
	ROM_LOAD16_BYTE( "bld01v22.u88", 0x00001, 0x40000, CRC(78336a7b) SHA1(76002ce4a2d83ceae10d9c9c123013832a081150) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "bldsnd.u17", 0x10000, 0x18000, CRC(dddeedbb) SHA1(f8ea786836630fc44bba968845fd2cb42cd81592) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "bsgrom0.bin",  0x000000, 0x080000, CRC(4e10b8c1) SHA1(b80f7dbf32faa97829c735da6dc0ee424dc9ecec) )
	ROM_LOAD32_BYTE( "bsgrom5.bin",  0x000001, 0x080000, CRC(6333b6ce) SHA1(53b884e09d198f8f53449bd011ba743c28b2c934) )
	ROM_LOAD32_BYTE( "bsgrom10.bin", 0x000002, 0x080000, CRC(a972a65c) SHA1(7772c2e0aaa0b3183c6287125b95fd1cd0c3775a) )
	ROM_LOAD32_BYTE( "bsgrom15.bin", 0x000003, 0x080000, CRC(9a8f54aa) SHA1(3a2f99c28324ba1fcfb652bdc596d94c90637b72) )
	ROM_LOAD32_BYTE( "bsgrom1.bin",  0x200000, 0x080000, CRC(10abf660) SHA1(e5b28ee12972abbcd883d702f496feae62d19e07) )
	ROM_LOAD32_BYTE( "bsgrom6.bin",  0x200001, 0x080000, CRC(06a260d5) SHA1(f506b799bbb5d4465a4ab564cfd276877b8a52e9) )
	ROM_LOAD32_BYTE( "bsgrom11.bin", 0x200002, 0x080000, CRC(f2cab3c7) SHA1(eecb1f2f8a3f3b1663a51c343e28e1b2078da5f4) )
	ROM_LOAD32_BYTE( "bsgrom16.bin", 0x200003, 0x080000, CRC(403aef7b) SHA1(0a2a61480cddd020bfbad01c6c328dfa6760bddd) )
	ROM_LOAD32_BYTE( "bsgrom2.bin",  0x400000, 0x080000, CRC(488200b1) SHA1(ae25bb5e9a836ae362d88de4e392ff46ff93c636) )
	ROM_LOAD32_BYTE( "bsgrom7.bin",  0x400001, 0x080000, CRC(5bb19727) SHA1(fd8aa73ff0606ac86f054794f6abc42f99b4f856) )
	ROM_LOAD32_BYTE( "bsgrom12.bin", 0x400002, 0x080000, CRC(b10d674f) SHA1(f5c713480deac44b86173ca9ac3aeea8a4d2ac85) )
	ROM_LOAD32_BYTE( "bsgrom17.bin", 0x400003, 0x080000, CRC(7119df7e) SHA1(67acb0525f21d5cd174528fb0fa72c7101deb5eb) )
	ROM_LOAD32_BYTE( "bsgrom3.bin",  0x600000, 0x080000, CRC(2378792e) SHA1(8cf1040a86ce7b6be3f56d144d4a17b7a888a648) )
	ROM_LOAD32_BYTE( "bsgrom8.bin",  0x600001, 0x080000, CRC(3640ca2e) SHA1(1fbc8306c7310ab23d40887c16f80e2dc3d730f8) )
	ROM_LOAD32_BYTE( "bsgrom13.bin", 0x600002, 0x080000, CRC(bd4a071d) SHA1(f25483591659d7424a4c62d0093fa56e5d337bf3) )
	ROM_LOAD32_BYTE( "bsgrom18.bin", 0x600003, 0x080000, CRC(12959bb8) SHA1(f74d407004ccbf461f749672e4e57a5f0b6b549f) )
	ROM_FILL(                        0x800000, 0x080000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "enssnd2m.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "bssrom0.bin",  0x000000, 0x80000, CRC(ee4570c8) SHA1(73dd292224bf182770b3cc2d90eb52b7d7b24378) )
	ROM_LOAD16_BYTE( "bssrom1.bin",  0x100000, 0x80000, CRC(b0f32ec5) SHA1(666f904b31cdef12cbf1dbb43a7d3ff7c2903260) )
	ROM_LOAD16_BYTE( "bssrom2.bin",  0x300000, 0x40000, CRC(8aee1e77) SHA1(f949fa89ee7d59f457ce89c72d461cecd0cface3) )
ROM_END


ROM_START( bloods21 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "bld00v21.u83", 0x00000, 0x40000, CRC(71215c8e) SHA1(ee0f94c3a2619d7e3cc1ba5e1888a97b0c75a3ae) )
	ROM_LOAD16_BYTE( "bld01v21.u88", 0x00001, 0x40000, CRC(da403da6) SHA1(0f09f38ae932acb4ddbb6323bce58be7284cb24b) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "bldsnd.u17", 0x10000, 0x18000, CRC(dddeedbb) SHA1(f8ea786836630fc44bba968845fd2cb42cd81592) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "bsgrom0.bin",  0x000000, 0x080000, CRC(4e10b8c1) SHA1(b80f7dbf32faa97829c735da6dc0ee424dc9ecec) )
	ROM_LOAD32_BYTE( "bsgrom5.bin",  0x000001, 0x080000, CRC(6333b6ce) SHA1(53b884e09d198f8f53449bd011ba743c28b2c934) )
	ROM_LOAD32_BYTE( "bsgrom10.bin", 0x000002, 0x080000, CRC(a972a65c) SHA1(7772c2e0aaa0b3183c6287125b95fd1cd0c3775a) )
	ROM_LOAD32_BYTE( "bsgrom15.bin", 0x000003, 0x080000, CRC(9a8f54aa) SHA1(3a2f99c28324ba1fcfb652bdc596d94c90637b72) )
	ROM_LOAD32_BYTE( "bsgrom1.bin",  0x200000, 0x080000, CRC(10abf660) SHA1(e5b28ee12972abbcd883d702f496feae62d19e07) )
	ROM_LOAD32_BYTE( "bsgrom6.bin",  0x200001, 0x080000, CRC(06a260d5) SHA1(f506b799bbb5d4465a4ab564cfd276877b8a52e9) )
	ROM_LOAD32_BYTE( "bsgrom11.bin", 0x200002, 0x080000, CRC(f2cab3c7) SHA1(eecb1f2f8a3f3b1663a51c343e28e1b2078da5f4) )
	ROM_LOAD32_BYTE( "bsgrom16.bin", 0x200003, 0x080000, CRC(403aef7b) SHA1(0a2a61480cddd020bfbad01c6c328dfa6760bddd) )
	ROM_LOAD32_BYTE( "bsgrom2.bin",  0x400000, 0x080000, CRC(488200b1) SHA1(ae25bb5e9a836ae362d88de4e392ff46ff93c636) )
	ROM_LOAD32_BYTE( "bsgrom7.bin",  0x400001, 0x080000, CRC(5bb19727) SHA1(fd8aa73ff0606ac86f054794f6abc42f99b4f856) )
	ROM_LOAD32_BYTE( "bsgrom12.bin", 0x400002, 0x080000, CRC(b10d674f) SHA1(f5c713480deac44b86173ca9ac3aeea8a4d2ac85) )
	ROM_LOAD32_BYTE( "bsgrom17.bin", 0x400003, 0x080000, CRC(7119df7e) SHA1(67acb0525f21d5cd174528fb0fa72c7101deb5eb) )
	ROM_LOAD32_BYTE( "bsgrom3.bin",  0x600000, 0x080000, CRC(2378792e) SHA1(8cf1040a86ce7b6be3f56d144d4a17b7a888a648) )
	ROM_LOAD32_BYTE( "bsgrom8.bin",  0x600001, 0x080000, CRC(3640ca2e) SHA1(1fbc8306c7310ab23d40887c16f80e2dc3d730f8) )
	ROM_LOAD32_BYTE( "bsgrom13.bin", 0x600002, 0x080000, CRC(bd4a071d) SHA1(f25483591659d7424a4c62d0093fa56e5d337bf3) )
	ROM_LOAD32_BYTE( "bsgrom18.bin", 0x600003, 0x080000, CRC(12959bb8) SHA1(f74d407004ccbf461f749672e4e57a5f0b6b549f) )
	ROM_FILL(                        0x800000, 0x080000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "enssnd2m.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "bssrom0.bin",  0x000000, 0x80000, CRC(ee4570c8) SHA1(73dd292224bf182770b3cc2d90eb52b7d7b24378) )
	ROM_LOAD16_BYTE( "bssrom1.bin",  0x100000, 0x80000, CRC(b0f32ec5) SHA1(666f904b31cdef12cbf1dbb43a7d3ff7c2903260) )
	ROM_LOAD16_BYTE( "bssrom2.bin",  0x300000, 0x40000, CRC(8aee1e77) SHA1(f949fa89ee7d59f457ce89c72d461cecd0cface3) )
ROM_END


ROM_START( bloods11 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "bld00-11.u83", 0x00000, 0x40000, CRC(4fff8f9b) SHA1(90f450497935322b0082a70e10abf758fc441dd0) )
	ROM_LOAD16_BYTE( "bld01-11.u88", 0x00001, 0x40000, CRC(59ce23ea) SHA1(6aa02fff07f5ec6dff4f6db9ea7878a722079f81) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "bldsnd.u17", 0x10000, 0x18000, CRC(dddeedbb) SHA1(f8ea786836630fc44bba968845fd2cb42cd81592) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "bsgrom0.bin",  0x000000, 0x080000, CRC(4e10b8c1) SHA1(b80f7dbf32faa97829c735da6dc0ee424dc9ecec) )
	ROM_LOAD32_BYTE( "bsgrom5.bin",  0x000001, 0x080000, CRC(6333b6ce) SHA1(53b884e09d198f8f53449bd011ba743c28b2c934) )
	ROM_LOAD32_BYTE( "bsgrom10.bin", 0x000002, 0x080000, CRC(a972a65c) SHA1(7772c2e0aaa0b3183c6287125b95fd1cd0c3775a) )
	ROM_LOAD32_BYTE( "bsgrom15.bin", 0x000003, 0x080000, CRC(9a8f54aa) SHA1(3a2f99c28324ba1fcfb652bdc596d94c90637b72) )
	ROM_LOAD32_BYTE( "bsgrom1.bin",  0x200000, 0x080000, CRC(10abf660) SHA1(e5b28ee12972abbcd883d702f496feae62d19e07) )
	ROM_LOAD32_BYTE( "bsgrom6.bin",  0x200001, 0x080000, CRC(06a260d5) SHA1(f506b799bbb5d4465a4ab564cfd276877b8a52e9) )
	ROM_LOAD32_BYTE( "bsgrom11.bin", 0x200002, 0x080000, CRC(f2cab3c7) SHA1(eecb1f2f8a3f3b1663a51c343e28e1b2078da5f4) )
	ROM_LOAD32_BYTE( "bsgrom16.bin", 0x200003, 0x080000, CRC(403aef7b) SHA1(0a2a61480cddd020bfbad01c6c328dfa6760bddd) )
	ROM_LOAD32_BYTE( "bsgrom2.bin",  0x400000, 0x080000, CRC(488200b1) SHA1(ae25bb5e9a836ae362d88de4e392ff46ff93c636) )
	ROM_LOAD32_BYTE( "bsgrom7.bin",  0x400001, 0x080000, CRC(5bb19727) SHA1(fd8aa73ff0606ac86f054794f6abc42f99b4f856) )
	ROM_LOAD32_BYTE( "bsgrom12.bin", 0x400002, 0x080000, CRC(b10d674f) SHA1(f5c713480deac44b86173ca9ac3aeea8a4d2ac85) )
	ROM_LOAD32_BYTE( "bsgrom17.bin", 0x400003, 0x080000, CRC(7119df7e) SHA1(67acb0525f21d5cd174528fb0fa72c7101deb5eb) )
	ROM_LOAD32_BYTE( "bsgrom3.bin",  0x600000, 0x080000, CRC(2378792e) SHA1(8cf1040a86ce7b6be3f56d144d4a17b7a888a648) )
	ROM_LOAD32_BYTE( "bsgrom8.bin",  0x600001, 0x080000, CRC(3640ca2e) SHA1(1fbc8306c7310ab23d40887c16f80e2dc3d730f8) )
	ROM_LOAD32_BYTE( "bsgrom13.bin", 0x600002, 0x080000, CRC(bd4a071d) SHA1(f25483591659d7424a4c62d0093fa56e5d337bf3) )
	ROM_LOAD32_BYTE( "bsgrom18.bin", 0x600003, 0x080000, CRC(12959bb8) SHA1(f74d407004ccbf461f749672e4e57a5f0b6b549f) )
	ROM_FILL(                        0x800000, 0x080000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "enssnd2m.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "bssrom0.bin",  0x000000, 0x80000, CRC(ee4570c8) SHA1(73dd292224bf182770b3cc2d90eb52b7d7b24378) )
	ROM_LOAD16_BYTE( "bssrom1.bin",  0x100000, 0x80000, CRC(b0f32ec5) SHA1(666f904b31cdef12cbf1dbb43a7d3ff7c2903260) )
	ROM_LOAD16_BYTE( "bssrom2.bin",  0x300000, 0x40000, CRC(8aee1e77) SHA1(f949fa89ee7d59f457ce89c72d461cecd0cface3) )
ROM_END


ROM_START( hardyard )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "fb00v12.u83", 0x00000, 0x40000, CRC(c7497692) SHA1(6c11535cf011e15dd7ffb5eba8e8da557c38277e) )
	ROM_LOAD16_BYTE( "fb00v12.u88", 0x00001, 0x40000, CRC(3320c79a) SHA1(d1d32048c541782e60c525d9789fe12607a6df3a) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "fbsndv11.u17", 0x10000, 0x18000, CRC(d221b121) SHA1(06f351274a9dcb522f67f58499c9dc2ef5f06c07) )
	ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "itfb0.bin",  0x000000, 0x100000, CRC(0b7781af) SHA1(0e6b617a5d9a2d0d50a3839231177f2934177b87) )
	ROM_LOAD32_BYTE( "itfb1.bin",  0x000001, 0x100000, CRC(178d0f9b) SHA1(2c13be9063742c24a4b8409fe1d16f6c989f20e0) )
	ROM_LOAD32_BYTE( "itfb2.bin",  0x000002, 0x100000, CRC(0a17231e) SHA1(1499783ac32c3c6956d4084d623a432aecfd7769) )
	ROM_LOAD32_BYTE( "itfb3.bin",  0x000003, 0x100000, CRC(104456af) SHA1(6b6adca80f663cdc8fcbdf58c033d32193e91b4b) )
	ROM_LOAD32_BYTE( "itfb4.bin",  0x400000, 0x100000, CRC(2cb6f454) SHA1(e3af2809d43ddb4f17342a0b63848bf9a579b1eb) )
	ROM_LOAD32_BYTE( "itfb5.bin",  0x400001, 0x100000, CRC(9b19b873) SHA1(4393dce2fd6e1f3c2b855759a985e1e068959e0a) )
	ROM_LOAD32_BYTE( "itfb6.bin",  0x400002, 0x100000, CRC(58694394) SHA1(9b0742d136de9870f50a1f47347071a21283067b) )
	ROM_LOAD32_BYTE( "itfb7.bin",  0x400003, 0x100000, CRC(9b7a2d1a) SHA1(e4aa8d5f76b26d16cabaf88dfa1bfba8052fe99d) )
	ROM_LOAD32_BYTE( "itfb8.bin",  0x800000, 0x020000, CRC(a1656bf8) SHA1(4df05a1cdf5d636956d1c3d1f4f1988b254608d5) )
	ROM_LOAD32_BYTE( "itfb9.bin",  0x800001, 0x020000, CRC(2afa9e10) SHA1(d422447fd2fc2f9350af472eb1f1223383a1a259) )
	ROM_LOAD32_BYTE( "itfb10.bin", 0x800002, 0x020000, CRC(d5d15b38) SHA1(f414c19d8d88f916fbfa24fc3e16cea2e0acce08) )
	ROM_LOAD32_BYTE( "itfb11.bin", 0x800003, 0x020000, CRC(cd4f0df0) SHA1(632eb0cf27d7bf3df09d03f373a3195dd5a702b8) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbrom0.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbsrom00.bin", 0x000000, 0x080000, CRC(b0a76ad2) SHA1(d1125cf96f6b9613840b8d22afa59748fb32ab90) )
	ROM_LOAD16_BYTE( "fbsrom01.bin", 0x100000, 0x080000, CRC(9fbf6a02) SHA1(90c86a94767a383895183a25b15084ed62891518) )
ROM_END


ROM_START( hardyd10 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "hrdyrd.u83", 0x00000, 0x40000, CRC(f839393c) SHA1(ba06172bc4781f7738ce43019031715fee4b344c) )
	ROM_LOAD16_BYTE( "hrdyrd.u88", 0x00001, 0x40000, CRC(ca444702) SHA1(49bcc0994da9cd2c31c0cd78b822aceeaffd035f) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "hy_fbsnd.u17", 0x10000, 0x18000, CRC(6c6db5b8) SHA1(925e7c7cc7c3d290f4a334f24eef574aaac3150c) )
	ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "itfb0.bin",  0x000000, 0x100000, CRC(0b7781af) SHA1(0e6b617a5d9a2d0d50a3839231177f2934177b87) )
	ROM_LOAD32_BYTE( "itfb1.bin",  0x000001, 0x100000, CRC(178d0f9b) SHA1(2c13be9063742c24a4b8409fe1d16f6c989f20e0) )
	ROM_LOAD32_BYTE( "itfb2.bin",  0x000002, 0x100000, CRC(0a17231e) SHA1(1499783ac32c3c6956d4084d623a432aecfd7769) )
	ROM_LOAD32_BYTE( "itfb3.bin",  0x000003, 0x100000, CRC(104456af) SHA1(6b6adca80f663cdc8fcbdf58c033d32193e91b4b) )
	ROM_LOAD32_BYTE( "itfb4.bin",  0x400000, 0x100000, CRC(2cb6f454) SHA1(e3af2809d43ddb4f17342a0b63848bf9a579b1eb) )
	ROM_LOAD32_BYTE( "itfb5.bin",  0x400001, 0x100000, CRC(9b19b873) SHA1(4393dce2fd6e1f3c2b855759a985e1e068959e0a) )
	ROM_LOAD32_BYTE( "itfb6.bin",  0x400002, 0x100000, CRC(58694394) SHA1(9b0742d136de9870f50a1f47347071a21283067b) )
	ROM_LOAD32_BYTE( "itfb7.bin",  0x400003, 0x100000, CRC(9b7a2d1a) SHA1(e4aa8d5f76b26d16cabaf88dfa1bfba8052fe99d) )
	ROM_LOAD32_BYTE( "itfb8.bin",  0x800000, 0x020000, CRC(a1656bf8) SHA1(4df05a1cdf5d636956d1c3d1f4f1988b254608d5) )
	ROM_LOAD32_BYTE( "itfb9.bin",  0x800001, 0x020000, CRC(2afa9e10) SHA1(d422447fd2fc2f9350af472eb1f1223383a1a259) )
	ROM_LOAD32_BYTE( "itfb10.bin", 0x800002, 0x020000, CRC(d5d15b38) SHA1(f414c19d8d88f916fbfa24fc3e16cea2e0acce08) )
	ROM_LOAD32_BYTE( "itfb11.bin", 0x800003, 0x020000, CRC(cd4f0df0) SHA1(632eb0cf27d7bf3df09d03f373a3195dd5a702b8) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbrom0.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbsrom00.bin", 0x000000, 0x080000, CRC(b0a76ad2) SHA1(d1125cf96f6b9613840b8d22afa59748fb32ab90) )
	ROM_LOAD16_BYTE( "fbsrom01.bin", 0x100000, 0x080000, CRC(9fbf6a02) SHA1(90c86a94767a383895183a25b15084ed62891518) )
ROM_END


ROM_START( pairs )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x40000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "pair0.u83", 0x00000, 0x20000, CRC(a9c761d8) SHA1(2618c9c3f336cf30f760fd88f12c09985cfd4ee7) )
	ROM_LOAD16_BYTE( "pair1.u88", 0x00001, 0x20000, CRC(5141eb86) SHA1(3bb10d588e6334a33e5c2c468651699e84f46cdc) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "snd.u17", 0x10000, 0x18000, CRC(7a514cfd) SHA1(ef5bc74c9560d2c058298051070fa748e58f07e1) )
	ROM_CONTINUE(        0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "grom0",  0x000000, 0x80000, CRC(baf1c2dd) SHA1(4de50001bce294ea5eea581cee9ca5a966701176) )
	ROM_LOAD32_BYTE( "grom5",  0x000001, 0x80000, CRC(30e993f3) SHA1(fe32aabacbe9d6d9419410faafe048c01988ac78) )
	ROM_LOAD32_BYTE( "grom10", 0x000002, 0x80000, CRC(3f52f50d) SHA1(abb7ec8fa1af0876dacfe04d76fbc8fc18a2b610) )
	ROM_LOAD32_BYTE( "grom15", 0x000003, 0x80000, CRC(fd38aa36) SHA1(7c65b2a42bb45b81b841792c475ea391c03a4eb2) )
	ROM_LOAD32_BYTE( "grom1",  0x200000, 0x40000, CRC(b0bd7008) SHA1(29cb334e9af73f7aef4bf55eae79d8cc05412164) )
	ROM_LOAD32_BYTE( "grom6",  0x200001, 0x40000, CRC(f7b20a47) SHA1(5a68add24b0f9cfb56b3e7aedc28382c8ead81a1) )
	ROM_LOAD32_BYTE( "grom11", 0x200002, 0x40000, CRC(1e5f2523) SHA1(c6c362bc0bb303e271176ce8c2a49990be1834cd) )
	ROM_LOAD32_BYTE( "grom16", 0x200003, 0x40000, CRC(b2975259) SHA1(aa82f8e855f2ebf1d7178a46f2b515d7c9a26299) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbrom0.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "srom0.bin", 0x000000, 0x80000, CRC(19a857f9) SHA1(2515b4c127191d52d3b5a72477384847d8cabad3) )
ROM_END

ROM_START( pairsa )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x40000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "pair0", 0x00000, 0x20000, CRC(774995a3) SHA1(93df91378b56802d14c105f7f48ed8a4f7bafffd) )
	ROM_LOAD16_BYTE( "pair1", 0x00001, 0x20000, CRC(85d0b73a) SHA1(48a6ac6de94be13e407da13e3e2440d858714b4b) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "snd.u17", 0x10000, 0x18000, CRC(7a514cfd) SHA1(ef5bc74c9560d2c058298051070fa748e58f07e1) )
	ROM_CONTINUE(        0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "grom0",  0x000000, 0x80000, CRC(baf1c2dd) SHA1(4de50001bce294ea5eea581cee9ca5a966701176) )
	ROM_LOAD32_BYTE( "grom5",  0x000001, 0x80000, CRC(30e993f3) SHA1(fe32aabacbe9d6d9419410faafe048c01988ac78) )
	ROM_LOAD32_BYTE( "grom10", 0x000002, 0x80000, CRC(3f52f50d) SHA1(abb7ec8fa1af0876dacfe04d76fbc8fc18a2b610) )
	ROM_LOAD32_BYTE( "grom15", 0x000003, 0x80000, CRC(fd38aa36) SHA1(7c65b2a42bb45b81b841792c475ea391c03a4eb2) )
	ROM_LOAD32_BYTE( "grom1",  0x200000, 0x40000, CRC(b0bd7008) SHA1(29cb334e9af73f7aef4bf55eae79d8cc05412164) )
	ROM_LOAD32_BYTE( "grom6",  0x200001, 0x40000, CRC(f7b20a47) SHA1(5a68add24b0f9cfb56b3e7aedc28382c8ead81a1) )
	ROM_LOAD32_BYTE( "grom11", 0x200002, 0x40000, CRC(1e5f2523) SHA1(c6c362bc0bb303e271176ce8c2a49990be1834cd) )
	ROM_LOAD32_BYTE( "grom16", 0x200003, 0x40000, CRC(b2975259) SHA1(aa82f8e855f2ebf1d7178a46f2b515d7c9a26299) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbrom0.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "srom0", 0x000000, 0x80000, CRC(1d96c581) SHA1(3b7c84b7db3b098ec28c7058c16f97e9cf0e4733) )
ROM_END


ROM_START( wcbowl )		/* Version 1.66 (PCB P/N 1082 Rev 2) */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "wcb_prm0.166", 0x00000, 0x20000, CRC(f6774112) SHA1(cb09bb3e40490b3cdc3a5f7d18168384b5b29d85) )
	ROM_LOAD32_BYTE( "wcb_prm1.166", 0x00001, 0x20000, CRC(931821ae) SHA1(328cd78ba70fe3cb0bdbc53833fe6fb153aceaea) )
	ROM_LOAD32_BYTE( "wcb_prm2.166", 0x00002, 0x20000, CRC(c54f5e40) SHA1(2cd92ba1db74b24e908d10f733757801db180dd0) )
	ROM_LOAD32_BYTE( "wcb_prm3.166", 0x00003, 0x20000, CRC(dd72c796) SHA1(4c1542c51848a88a663e56ae0b47bf9d2d9f7d54) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "wcb_snd.u88", 0x10000, 0x18000, CRC(194a51d7) SHA1(c67b042008ff2a2713562d3789e5bc3a312fae17) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "wcb_grom.0_0", 0x000000, 0x080000, CRC(6fcb4246) SHA1(91fb5d18ea9494b08251d1e611c80414df3aad66) )
	ROM_LOAD32_BYTE( "wcb_grom.0_1", 0x000001, 0x080000, CRC(2ae31f45) SHA1(85218aa9a7ca7c6870427ffbd08b78255813ff90) )
	ROM_LOAD32_BYTE( "wcb_grom.0_2", 0x000002, 0x080000, CRC(bccc0f35) SHA1(2389662e881e86f8cdb36eb2a082923d976676c8) )
	ROM_LOAD32_BYTE( "wcb_grom.0_3", 0x000003, 0x080000, CRC(ab1da462) SHA1(3f3a4a083483d2d95d5ef540eea077cad799fcb7) )
	ROM_LOAD32_BYTE( "wcb_grom.1_0", 0x200000, 0x080000, CRC(bdfafd1f) SHA1(bc0e6fe83d3f8e88c2e55ba3a436875d5470de5b) )
	ROM_LOAD32_BYTE( "wcb_grom.1_1", 0x200001, 0x080000, CRC(7d6baa2e) SHA1(c47854b064aa96d2581c23afe13cd05a36f9dae3) )
	ROM_LOAD32_BYTE( "wcb_grom.1_2", 0x200002, 0x080000, CRC(7513d3de) SHA1(b17650ed5210860c3bde53647a30f8fce67aaa38) )
	ROM_LOAD32_BYTE( "wcb_grom.1_3", 0x200003, 0x080000, CRC(e46877e6) SHA1(f50c904ec5b2b8cbc92f2b28641433c91ee17af5) )
	ROM_FILL(                        0x400000, 0x480000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srom.0",  0x000000, 0x080000, CRC(c3821cb5) SHA1(3c2c27d1e577201cbd0d28cc48fc80ae7747faa1) )
	ROM_LOAD16_BYTE( "wcb_srom.1",  0x200000, 0x080000, CRC(afa24888) SHA1(169eaedd09e0214ac72c932903a11bbb2ebc5bf1) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND2, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srom.2",  0x000000, 0x080000, CRC(f82c08fd) SHA1(8f6f47f5a4b68a31df4c2eb330dc95a9963e55c1) )
	ROM_LOAD16_BYTE( "wcb_srom.3",  0x200000, 0x020000, CRC(1c2efdee) SHA1(d306c9e7f9c4c2662561401170439a10a9ee89ed) )
ROM_END


ROM_START( wcbwl165 )	/* Version 1.65 (PCB P/N 1082 Rev 2) */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "wcb_prm0.165", 0x00000, 0x20000, CRC(cf0f6c25) SHA1(90685288994dce73d5b1070a55fca3f1713c5bb6) )
	ROM_LOAD32_BYTE( "wcb_prm1.165", 0x00001, 0x20000, CRC(076ab158) SHA1(e6d8a6726e27ba6916d4711dff88f26f1dc162e1) )
	ROM_LOAD32_BYTE( "wcb_prm2.165", 0x00002, 0x20000, CRC(47259009) SHA1(78a6e70e747030a5ed43d49384061e53f4a77675) )
	ROM_LOAD32_BYTE( "wcb_prm3.165", 0x00003, 0x20000, CRC(4c6b4e4f) SHA1(77f5f4b632dd1919ae210bbdc75042bdbebf6660) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "wcb_snd.u88", 0x10000, 0x18000, CRC(194a51d7) SHA1(c67b042008ff2a2713562d3789e5bc3a312fae17) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "wcb_grom.0_0", 0x000000, 0x080000, CRC(6fcb4246) SHA1(91fb5d18ea9494b08251d1e611c80414df3aad66) )
	ROM_LOAD32_BYTE( "wcb_grom.0_1", 0x000001, 0x080000, CRC(2ae31f45) SHA1(85218aa9a7ca7c6870427ffbd08b78255813ff90) )
	ROM_LOAD32_BYTE( "wcb_grom.0_2", 0x000002, 0x080000, CRC(bccc0f35) SHA1(2389662e881e86f8cdb36eb2a082923d976676c8) )
	ROM_LOAD32_BYTE( "wcb_grom.0_3", 0x000003, 0x080000, CRC(ab1da462) SHA1(3f3a4a083483d2d95d5ef540eea077cad799fcb7) )
	ROM_LOAD32_BYTE( "wcb_grom.1_0", 0x200000, 0x080000, CRC(bdfafd1f) SHA1(bc0e6fe83d3f8e88c2e55ba3a436875d5470de5b) )
	ROM_LOAD32_BYTE( "wcb_grom.1_1", 0x200001, 0x080000, CRC(7d6baa2e) SHA1(c47854b064aa96d2581c23afe13cd05a36f9dae3) )
	ROM_LOAD32_BYTE( "wcb_grom.1_2", 0x200002, 0x080000, CRC(7513d3de) SHA1(b17650ed5210860c3bde53647a30f8fce67aaa38) )
	ROM_LOAD32_BYTE( "wcb_grom.1_3", 0x200003, 0x080000, CRC(e46877e6) SHA1(f50c904ec5b2b8cbc92f2b28641433c91ee17af5) )
	ROM_FILL(                        0x400000, 0x480000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srom.0",  0x000000, 0x080000, CRC(c3821cb5) SHA1(3c2c27d1e577201cbd0d28cc48fc80ae7747faa1) )
	ROM_LOAD16_BYTE( "wcb_srom.1",  0x200000, 0x080000, CRC(afa24888) SHA1(169eaedd09e0214ac72c932903a11bbb2ebc5bf1) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND2, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srom.2",  0x000000, 0x080000, CRC(f82c08fd) SHA1(8f6f47f5a4b68a31df4c2eb330dc95a9963e55c1) )
	ROM_LOAD16_BYTE( "wcb_srom.3",  0x200000, 0x020000, CRC(1c2efdee) SHA1(d306c9e7f9c4c2662561401170439a10a9ee89ed) )
ROM_END


ROM_START( wcbwl161 )	/* Version 1.61 (PCB P/N 1082 Rev 2) */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "wcb_prm0.161", 0x00000, 0x20000, CRC(b879d4a7) SHA1(8b5af3f4d3522bdb8e1d6092b2e311fbfaec2bd0) )
	ROM_LOAD32_BYTE( "wcb_prm1.161", 0x00001, 0x20000, CRC(49f3ed6a) SHA1(6c6857bd3fbfe0cfeaf0e512bbbd795376a21472) )
	ROM_LOAD32_BYTE( "wcb_prm2.161", 0x00002, 0x20000, CRC(47259009) SHA1(78a6e70e747030a5ed43d49384061e53f4a77675) )
	ROM_LOAD32_BYTE( "wcb_prm3.161", 0x00003, 0x20000, CRC(e5081f85) SHA1(a5513b8dd917a35f1c8b7f833c2d5622353d39f0) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "wcb_snd.u88", 0x10000, 0x18000, CRC(194a51d7) SHA1(c67b042008ff2a2713562d3789e5bc3a312fae17) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "wcb_grom.0_0", 0x000000, 0x080000, CRC(6fcb4246) SHA1(91fb5d18ea9494b08251d1e611c80414df3aad66) )
	ROM_LOAD32_BYTE( "wcb_grom.0_1", 0x000001, 0x080000, CRC(2ae31f45) SHA1(85218aa9a7ca7c6870427ffbd08b78255813ff90) )
	ROM_LOAD32_BYTE( "wcb_grom.0_2", 0x000002, 0x080000, CRC(bccc0f35) SHA1(2389662e881e86f8cdb36eb2a082923d976676c8) )
	ROM_LOAD32_BYTE( "wcb_grom.0_3", 0x000003, 0x080000, CRC(ab1da462) SHA1(3f3a4a083483d2d95d5ef540eea077cad799fcb7) )
	ROM_LOAD32_BYTE( "wcb_grom.1_0", 0x200000, 0x080000, CRC(bdfafd1f) SHA1(bc0e6fe83d3f8e88c2e55ba3a436875d5470de5b) )
	ROM_LOAD32_BYTE( "wcb_grom.1_1", 0x200001, 0x080000, CRC(7d6baa2e) SHA1(c47854b064aa96d2581c23afe13cd05a36f9dae3) )
	ROM_LOAD32_BYTE( "wcb_grom.1_2", 0x200002, 0x080000, CRC(7513d3de) SHA1(b17650ed5210860c3bde53647a30f8fce67aaa38) )
	ROM_LOAD32_BYTE( "wcb_grom.1_3", 0x200003, 0x080000, CRC(e46877e6) SHA1(f50c904ec5b2b8cbc92f2b28641433c91ee17af5) )
	ROM_FILL(                        0x400000, 0x480000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srom.0",  0x000000, 0x080000, CRC(c3821cb5) SHA1(3c2c27d1e577201cbd0d28cc48fc80ae7747faa1) )
	ROM_LOAD16_BYTE( "wcb_srom.1",  0x200000, 0x080000, CRC(afa24888) SHA1(169eaedd09e0214ac72c932903a11bbb2ebc5bf1) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND2, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srom.2",  0x000000, 0x080000, CRC(f82c08fd) SHA1(8f6f47f5a4b68a31df4c2eb330dc95a9963e55c1) )
	ROM_LOAD16_BYTE( "wcb_srom.3",  0x200000, 0x020000, CRC(1c2efdee) SHA1(d306c9e7f9c4c2662561401170439a10a9ee89ed) )
ROM_END


ROM_START( wcbwl12 )	/* Version 1.2 (unique Hardware, 3-tier type board) */
	/* v1.3 & v1.5 for this platform has been confirmed, but not dumped */
	ROM_REGION( 0x10000, REGION_CPU1, 0 )

	ROM_REGION16_BE( 0x40000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD16_BYTE( "wcb_prgm.u83", 0x00000, 0x20000, CRC(0602c5ce) SHA1(4339f77301f9c607c6f1dc81270d03681e874e69) )
	ROM_LOAD16_BYTE( "wcb_prgm.u88", 0x00001, 0x20000, CRC(49573493) SHA1(42813573f4ab951cd830193c0ffe2ce7d79c354b) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "wcb_snd.u17", 0x10000, 0x18000, CRC(c14907ba) SHA1(e52fb87c1f9b5847efc0ef15eb7e6c04dcf38110) )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x880000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "wcb_grm0.0", 0x000000, 0x080000, CRC(5d79aaae) SHA1(e1bf5c46843f69b8bac41dde73d89ba59b4c8b7f) )
	ROM_LOAD32_BYTE( "wcb_grm0.1", 0x000001, 0x080000, CRC(e26dcedb) SHA1(15441b97dd3d50d28007062fe28841fa3f762ec9) )
	ROM_LOAD32_BYTE( "wcb_grm0.2", 0x000002, 0x080000, CRC(32735875) SHA1(4017a8577d8efa8c5b95bd30723ebbf6ecaeba2b) )
	ROM_LOAD32_BYTE( "wcb_grm0.3", 0x000003, 0x080000, CRC(019d0ab8) SHA1(3281eada296ad746da80ef6e5909c50b03b90d08) )
	ROM_LOAD32_BYTE( "wcb_grm1.0", 0x200000, 0x080000, CRC(8bd31762) SHA1(a7274c8173b4fb04a6aed0b6a622b52a811a8c83) )
	ROM_LOAD32_BYTE( "wcb_grm1.1", 0x200001, 0x080000, CRC(b3f761fc) SHA1(5880ca1423cea9a7ca3d0875c8db33787f4056d4) )
	ROM_LOAD32_BYTE( "wcb_grm1.2", 0x200002, 0x080000, CRC(c22f44ad) SHA1(b25b11346ee1812b2be79105faf64faa0302c105) )
	ROM_LOAD32_BYTE( "wcb_grm1.3", 0x200003, 0x080000, CRC(036084c4) SHA1(6d2e402d2f4565e037a2676ba676e4b1da2b5dfe) )
	ROM_FILL(                      0x400000, 0x480000, 0xff )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_rom.0",   0x000000, 0x200000, CRC(0814ab80) SHA1(e92525f7cf58cf480510c278fea705f67225e58f) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "wcb_srm.0",  0x000000, 0x080000, CRC(115bcd1f) SHA1(c321bf3145c11de1351c8f9cd554ab3d6600e854) )
	ROM_LOAD16_BYTE( "wcb_srm.1",  0x100000, 0x080000, CRC(87a4a4d8) SHA1(60db2f466686481857eb39b90ac7a19d0a96adac) )
ROM_END


ROM_START( drivedge )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x8000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD( "de-u130.bin", 0x00000, 0x8000, CRC(873579b0) SHA1(ce7fcbea780aee376c2f4c659a75fcf6b7abbdb4) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "desndu17.bin", 0x10000, 0x18000, CRC(6e8ca8bc) SHA1(98ad36877b40123b0396943754234df8de183687) )
	ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )
	ROM_LOAD( "de-u7net.bin", 0x08000, 0x08000, CRC(dea8b9de) SHA1(46ba3a549522d7e6a32792814a04fd34839c7e55) )

	ROM_REGION( 0xa00000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "de-grom0.bin", 0x000000, 0x80000, CRC(7fe5b01b) SHA1(b31e48971253d77e2277434b1b1590cbbea2209f) )
	ROM_LOAD32_BYTE( "de-grom5.bin", 0x000001, 0x80000, CRC(5ea6dbc2) SHA1(c2de55ec6a527d0555504070a7ecb43b8aa797ea) )
	ROM_LOAD32_BYTE( "de-grm10.bin", 0x000002, 0x80000, CRC(76be06cd) SHA1(b533a07853b531e318c5a85139a74ca3edb9089f) )
	ROM_LOAD32_BYTE( "de-grm15.bin", 0x000003, 0x80000, CRC(119bf46b) SHA1(67f5434581d5f0042c7acd36d2c64ffe69efaa76) )
	ROM_LOAD32_BYTE( "de-grom1.bin", 0x200000, 0x80000, CRC(5b88e8b7) SHA1(04f05d9e811697c28a5671df6a9530594978decc) )
	ROM_LOAD32_BYTE( "de-grom6.bin", 0x200001, 0x80000, CRC(2cb76b9a) SHA1(0db32cb572121c8a751dcce55b94adc48f9be738) )
	ROM_LOAD32_BYTE( "de-grm11.bin", 0x200002, 0x80000, CRC(5d29018c) SHA1(11f346afedfac4f7b0d5d4995dd38ec2d7fc7777) )
	ROM_LOAD32_BYTE( "de-grm16.bin", 0x200003, 0x80000, CRC(476940fb) SHA1(00dab9aeb0d5cc23e4f78f15cb976ddcafa63b42) )
	ROM_LOAD32_BYTE( "de-grom2.bin", 0x400000, 0x80000, CRC(5ccc4c62) SHA1(fc49bba2208a1157fe0948fcadac79c597f382c4) )
	ROM_LOAD32_BYTE( "de-grom7.bin", 0x400001, 0x80000, CRC(45367070) SHA1(c7cf074f95cf287c4caf70d2286608c50ad01044) )
	ROM_LOAD32_BYTE( "de-grm12.bin", 0x400002, 0x80000, CRC(b978ef5a) SHA1(d1fce9c7966b8324ce1a4a99d92cd69ec32f5c47) )
	ROM_LOAD32_BYTE( "de-grm17.bin", 0x400003, 0x80000, CRC(eff8abac) SHA1(83da116368fae05a0c3c12ea72656681912a1175) )
	ROM_LOAD32_BYTE( "de-grom3.bin", 0x600000, 0x20000, CRC(9cd252c9) SHA1(7db6bdeeb2967154cd104ac2e20761cb99046d70) )
	ROM_LOAD32_BYTE( "de-grom8.bin", 0x600001, 0x20000, CRC(43f10ca4) SHA1(9eb0e2fd1adc25b334f86582be8e5960de0caba7) )
	ROM_LOAD32_BYTE( "de-grm13.bin", 0x600002, 0x20000, CRC(431d131e) SHA1(efe5a4aa65fde1f094adc6e701db8be94a4b625c) )
	ROM_LOAD32_BYTE( "de-grm18.bin", 0x600003, 0x20000, CRC(b09e0d9c) SHA1(b14ff39b028c0070ccca601c21542896168bd0b7) )
	ROM_LOAD32_BYTE( "de-grom4.bin", 0x800000, 0x20000, CRC(c499cdfa) SHA1(acec47fb606f999f9d88fdce1b5860d5afcd5106) )
	ROM_LOAD32_BYTE( "de-grom9.bin", 0x800001, 0x20000, CRC(e5f21566) SHA1(ce41c7e808799eea217e14e9aabe6ce617f87287) )
	ROM_LOAD32_BYTE( "de-grm14.bin", 0x800002, 0x20000, CRC(09dbe382) SHA1(a85ecba433eb9bb75b4060d1b6391f66f4c8146c) )
	ROM_LOAD32_BYTE( "de-grm19.bin", 0x800003, 0x20000, CRC(4ced78e1) SHA1(7995c8684ca28cbdf620d10297850463fa473fe8) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "fbrom0.bin", 0x000000, 0x200000, CRC(9fdc4825) SHA1(71e5255c66d9010be7e6f27916b605441fc53839) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND3, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "de-srom0.bin", 0x000000, 0x80000, CRC(649c685f) SHA1(95d8f257cac621c8bd4abaa88ea5f7b3b8adea4c) )
	ROM_LOAD16_BYTE( "de-srom1.bin", 0x100000, 0x80000, CRC(df4fff97) SHA1(3c43623bfc176639417e86a037b92026e84a5dce) )
ROM_END


ROM_START( sftm )	/* Version 1.12 */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "sftmrom0.112", 0x00000, 0x40000, CRC(9d09355c) SHA1(ca8c31d580e4b18b630c38e4ac1c353cf27ab4a2) )
	ROM_LOAD32_BYTE( "sftmrom1.112", 0x00001, 0x40000, CRC(a58ac6a9) SHA1(a481a789c397151efcbec7ad9983daa30f289d4e) )
	ROM_LOAD32_BYTE( "sftmrom2.112", 0x00002, 0x40000, CRC(2f21a4f6) SHA1(66b158c40375a0f729d44fd4c888cf6a5bbe2bf1) )
	ROM_LOAD32_BYTE( "sftmrom3.112", 0x00003, 0x40000, CRC(d26648d9) SHA1(9e3e1fa104da680c4a704d10d6518eea6382f039) )

	ROM_REGION( 0x48000, REGION_CPU2, 0 )
	ROM_LOAD( "sfmsndv1.u23", 0x10000, 0x38000, CRC(10d85366) SHA1(10d539c3ba37e277642c0c5888cb1886fb0f55fc) )
	ROM_CONTINUE(            0x08000, 0x08000 )

	ROM_REGION( 0x2080000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "rm0-0.bin",  0x0000000, 0x400000, CRC(09ef29cb) SHA1(430da5b00554582391478849d5b1547fe12eedbe) )
	ROM_LOAD32_BYTE( "rm0-1.bin",  0x0000001, 0x400000, CRC(6f5910fa) SHA1(1979d19dd36a9118dfaf021e05302982be5dbe69) )
	ROM_LOAD32_BYTE( "rm0-2.bin",  0x0000002, 0x400000, CRC(b8a2add5) SHA1(62e5bef936f014ac836c0cd5322eaba7018496b4) )
	ROM_LOAD32_BYTE( "rm0-3.bin",  0x0000003, 0x400000, CRC(6b6ff867) SHA1(72bc95ef361f9238602f0e03aed0adac8b59d227) )
	ROM_LOAD32_BYTE( "rm1-0.bin",  0x1000000, 0x400000, CRC(d5d65f77) SHA1(0bbb83bb42a442ef157472f3243ab44efa0c0aa0) )
	ROM_LOAD32_BYTE( "rm1-1.bin",  0x1000001, 0x400000, CRC(90467e27) SHA1(217561664871c60b0193337e34020ddd336b8f15) )
	ROM_LOAD32_BYTE( "rm1-2.bin",  0x1000002, 0x400000, CRC(903e56c2) SHA1(843ed9855ffdf37b100b3c5614139d552fd9cd6d) )
	ROM_LOAD32_BYTE( "rm1-3.bin",  0x1000003, 0x400000, CRC(fac35686) SHA1(ba99ab265620575c14c46806dc543d1f9fd24462) )
	ROM_LOAD32_BYTE( "sfmgrm.3_0", 0x2000000, 0x020000, CRC(3e1f76f7) SHA1(8aefe376e7248a583a6af02e5f9b2a4b48cc91d7) )
	ROM_LOAD32_BYTE( "sfmgrm.3_1", 0x2000001, 0x020000, CRC(578054b6) SHA1(99201959de28dbfd7692cedea4485751d3d4788f) )
	ROM_LOAD32_BYTE( "sfmgrm.3_2", 0x2000002, 0x020000, CRC(9af2f698) SHA1(e679728d8eba9f09379e503fa380202cd9adfde1) )
	ROM_LOAD32_BYTE( "sfmgrm.3_3", 0x2000003, 0x020000, CRC(cd38d1d6) SHA1(0cea60d6897b34eeb13997030f6ee7e1dfb3c833) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.0", 0x000000, 0x200000, CRC(6ca1d3fc) SHA1(904f4c55a1bc83531a6d87ff706afd8cdfaee83b) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND4, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.3",  0x000000, 0x080000, CRC(4f181534) SHA1(e858a33b22558665427146ec79dfba48edc20c2c) )
ROM_END


ROM_START( sftm111 )	/* Version 1.11 */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "sftmrom0.111", 0x00000, 0x40000, CRC(28187ddc) SHA1(7e4fa285be9389c913fca849098a7c0d9404df7a) )
	ROM_LOAD32_BYTE( "sftmrom1.111", 0x00001, 0x40000, CRC(ec2ce6fa) SHA1(b79aebb73ba77c2ebe081142853e81473743ac46) )
	ROM_LOAD32_BYTE( "sftmrom2.111", 0x00002, 0x40000, CRC(be20510e) SHA1(52e154fe4b77e461961fa23593383ef9b6dfb92f) )
	ROM_LOAD32_BYTE( "sftmrom3.111", 0x00003, 0x40000, CRC(eead342f) SHA1(b6df89527b527543df5535ef00945e64ff321e09) )

	ROM_REGION( 0x48000, REGION_CPU2, 0 )
	ROM_LOAD( "sfmsndv1.u23", 0x10000, 0x38000, CRC(10d85366) SHA1(10d539c3ba37e277642c0c5888cb1886fb0f55fc) )
	ROM_CONTINUE(            0x08000, 0x08000 )

	ROM_REGION( 0x2080000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "rm0-0.bin",  0x0000000, 0x400000, CRC(09ef29cb) SHA1(430da5b00554582391478849d5b1547fe12eedbe) )
	ROM_LOAD32_BYTE( "rm0-1.bin",  0x0000001, 0x400000, CRC(6f5910fa) SHA1(1979d19dd36a9118dfaf021e05302982be5dbe69) )
	ROM_LOAD32_BYTE( "rm0-2.bin",  0x0000002, 0x400000, CRC(b8a2add5) SHA1(62e5bef936f014ac836c0cd5322eaba7018496b4) )
	ROM_LOAD32_BYTE( "rm0-3.bin",  0x0000003, 0x400000, CRC(6b6ff867) SHA1(72bc95ef361f9238602f0e03aed0adac8b59d227) )
	ROM_LOAD32_BYTE( "rm1-0.bin",  0x1000000, 0x400000, CRC(d5d65f77) SHA1(0bbb83bb42a442ef157472f3243ab44efa0c0aa0) )
	ROM_LOAD32_BYTE( "rm1-1.bin",  0x1000001, 0x400000, CRC(90467e27) SHA1(217561664871c60b0193337e34020ddd336b8f15) )
	ROM_LOAD32_BYTE( "rm1-2.bin",  0x1000002, 0x400000, CRC(903e56c2) SHA1(843ed9855ffdf37b100b3c5614139d552fd9cd6d) )
	ROM_LOAD32_BYTE( "rm1-3.bin",  0x1000003, 0x400000, CRC(fac35686) SHA1(ba99ab265620575c14c46806dc543d1f9fd24462) )
	ROM_LOAD32_BYTE( "sfmgrm.3_0", 0x2000000, 0x020000, CRC(3e1f76f7) SHA1(8aefe376e7248a583a6af02e5f9b2a4b48cc91d7) )
	ROM_LOAD32_BYTE( "sfmgrm.3_1", 0x2000001, 0x020000, CRC(578054b6) SHA1(99201959de28dbfd7692cedea4485751d3d4788f) )
	ROM_LOAD32_BYTE( "sfmgrm.3_2", 0x2000002, 0x020000, CRC(9af2f698) SHA1(e679728d8eba9f09379e503fa380202cd9adfde1) )
	ROM_LOAD32_BYTE( "sfmgrm.3_3", 0x2000003, 0x020000, CRC(cd38d1d6) SHA1(0cea60d6897b34eeb13997030f6ee7e1dfb3c833) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.0", 0x000000, 0x200000, CRC(6ca1d3fc) SHA1(904f4c55a1bc83531a6d87ff706afd8cdfaee83b) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND4, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.3",  0x000000, 0x080000, CRC(4f181534) SHA1(e858a33b22558665427146ec79dfba48edc20c2c) )
ROM_END


ROM_START( sftm110 )	/* Version 1.10 */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "sftmrom0.110", 0x00000, 0x40000, CRC(00c0c63c) SHA1(39f614cca51fe7843c2158b6d9abdc52dc1b0bef) )
	ROM_LOAD32_BYTE( "sftmrom1.110", 0x00001, 0x40000, CRC(d4d2a67e) SHA1(88069caf171bb9c5602bc493f1f1dafa26d2fc78) )
	ROM_LOAD32_BYTE( "sftmrom2.110", 0x00002, 0x40000, CRC(d7b36c92) SHA1(fbdb6f3636b84b76cf42351392492b791429a0e4) )
	ROM_LOAD32_BYTE( "sftmrom3.110", 0x00003, 0x40000, CRC(be3efdbd) SHA1(169aff265d1520031988e51083d1f208cf2529b4) )

	ROM_REGION( 0x48000, REGION_CPU2, 0 )
	ROM_LOAD( "sfmsndv1.u23", 0x10000, 0x38000, CRC(10d85366) SHA1(10d539c3ba37e277642c0c5888cb1886fb0f55fc) )
	ROM_CONTINUE(            0x08000, 0x08000 )

	ROM_REGION( 0x2080000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "rm0-0.bin",  0x0000000, 0x400000, CRC(09ef29cb) SHA1(430da5b00554582391478849d5b1547fe12eedbe) )
	ROM_LOAD32_BYTE( "rm0-1.bin",  0x0000001, 0x400000, CRC(6f5910fa) SHA1(1979d19dd36a9118dfaf021e05302982be5dbe69) )
	ROM_LOAD32_BYTE( "rm0-2.bin",  0x0000002, 0x400000, CRC(b8a2add5) SHA1(62e5bef936f014ac836c0cd5322eaba7018496b4) )
	ROM_LOAD32_BYTE( "rm0-3.bin",  0x0000003, 0x400000, CRC(6b6ff867) SHA1(72bc95ef361f9238602f0e03aed0adac8b59d227) )
	ROM_LOAD32_BYTE( "rm1-0.bin",  0x1000000, 0x400000, CRC(d5d65f77) SHA1(0bbb83bb42a442ef157472f3243ab44efa0c0aa0) )
	ROM_LOAD32_BYTE( "rm1-1.bin",  0x1000001, 0x400000, CRC(90467e27) SHA1(217561664871c60b0193337e34020ddd336b8f15) )
	ROM_LOAD32_BYTE( "rm1-2.bin",  0x1000002, 0x400000, CRC(903e56c2) SHA1(843ed9855ffdf37b100b3c5614139d552fd9cd6d) )
	ROM_LOAD32_BYTE( "rm1-3.bin",  0x1000003, 0x400000, CRC(fac35686) SHA1(ba99ab265620575c14c46806dc543d1f9fd24462) )
	ROM_LOAD32_BYTE( "sfmgrm.3_0", 0x2000000, 0x020000, CRC(3e1f76f7) SHA1(8aefe376e7248a583a6af02e5f9b2a4b48cc91d7) )
	ROM_LOAD32_BYTE( "sfmgrm.3_1", 0x2000001, 0x020000, CRC(578054b6) SHA1(99201959de28dbfd7692cedea4485751d3d4788f) )
	ROM_LOAD32_BYTE( "sfmgrm.3_2", 0x2000002, 0x020000, CRC(9af2f698) SHA1(e679728d8eba9f09379e503fa380202cd9adfde1) )
	ROM_LOAD32_BYTE( "sfmgrm.3_3", 0x2000003, 0x020000, CRC(cd38d1d6) SHA1(0cea60d6897b34eeb13997030f6ee7e1dfb3c833) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.0", 0x000000, 0x200000, CRC(6ca1d3fc) SHA1(904f4c55a1bc83531a6d87ff706afd8cdfaee83b) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND4, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.3",  0x000000, 0x080000, CRC(4f181534) SHA1(e858a33b22558665427146ec79dfba48edc20c2c) )
ROM_END


ROM_START( sftmj )	/* Version 1.12N (Japan) */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "sfmprom0.12n", 0x00000, 0x40000, CRC(640a04a8) SHA1(adc7f5880962cbcc5f9f28e72a84070da6e2ec36) )
	ROM_LOAD32_BYTE( "sfmprom1.12n", 0x00001, 0x40000, CRC(2a27b690) SHA1(f63c3665ec030ecc2d7a10ead182941ade1c79d0) )
	ROM_LOAD32_BYTE( "sfmprom2.12n", 0x00002, 0x40000, CRC(cec1dd7b) SHA1(4c4cf14bc17ddef216d16a7fbcef2e4694b45eb4) )
	ROM_LOAD32_BYTE( "sfmprom3.12n", 0x00003, 0x40000, CRC(48fa60f4) SHA1(2d8bd4b5e3279af188feb3fb5e52a3d234bedd0a) )

	ROM_REGION( 0x48000, REGION_CPU2, 0 )
	ROM_LOAD( "snd_v111.u23", 0x10000, 0x38000, CRC(004854ed) SHA1(7ecb74dc3f45b038cc9904fea5c89d3e74fcbcf3) )
	ROM_CONTINUE(             0x08000, 0x08000 )

	ROM_REGION( 0x2080000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "rm0-0.bin",  0x0000000, 0x400000, CRC(09ef29cb) SHA1(430da5b00554582391478849d5b1547fe12eedbe) )
	ROM_LOAD32_BYTE( "rm0-1.bin",  0x0000001, 0x400000, CRC(6f5910fa) SHA1(1979d19dd36a9118dfaf021e05302982be5dbe69) )
	ROM_LOAD32_BYTE( "rm0-2.bin",  0x0000002, 0x400000, CRC(b8a2add5) SHA1(62e5bef936f014ac836c0cd5322eaba7018496b4) )
	ROM_LOAD32_BYTE( "rm0-3.bin",  0x0000003, 0x400000, CRC(6b6ff867) SHA1(72bc95ef361f9238602f0e03aed0adac8b59d227) )
	ROM_LOAD32_BYTE( "rm1-0.bin",  0x1000000, 0x400000, CRC(d5d65f77) SHA1(0bbb83bb42a442ef157472f3243ab44efa0c0aa0) )
	ROM_LOAD32_BYTE( "rm1-1.bin",  0x1000001, 0x400000, CRC(90467e27) SHA1(217561664871c60b0193337e34020ddd336b8f15) )
	ROM_LOAD32_BYTE( "rm1-2.bin",  0x1000002, 0x400000, CRC(903e56c2) SHA1(843ed9855ffdf37b100b3c5614139d552fd9cd6d) )
	ROM_LOAD32_BYTE( "rm1-3.bin",  0x1000003, 0x400000, CRC(fac35686) SHA1(ba99ab265620575c14c46806dc543d1f9fd24462) )
	ROM_LOAD32_BYTE( "sfmgrm.3_0", 0x2000000, 0x020000, CRC(3e1f76f7) SHA1(8aefe376e7248a583a6af02e5f9b2a4b48cc91d7) )
	ROM_LOAD32_BYTE( "sfmgrm.3_1", 0x2000001, 0x020000, CRC(578054b6) SHA1(99201959de28dbfd7692cedea4485751d3d4788f) )
	ROM_LOAD32_BYTE( "sfmgrm.3_2", 0x2000002, 0x020000, CRC(9af2f698) SHA1(e679728d8eba9f09379e503fa380202cd9adfde1) )
	ROM_LOAD32_BYTE( "sfmgrm.3_3", 0x2000003, 0x020000, CRC(cd38d1d6) SHA1(0cea60d6897b34eeb13997030f6ee7e1dfb3c833) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.0", 0x000000, 0x200000, CRC(6ca1d3fc) SHA1(904f4c55a1bc83531a6d87ff706afd8cdfaee83b) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND4, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "sfm_srom.3",  0x000000, 0x080000, CRC(4f181534) SHA1(e858a33b22558665427146ec79dfba48edc20c2c) )
ROM_END


ROM_START( shufshot )	/* Version 1.39 (PCB P/N 1082 Rev 2) */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "shotprm0.139", 0x00000, 0x20000, CRC(e811fc4a) SHA1(9e1d8f64ac89ac865929f6a23f66d95eeeda3ac9) )
	ROM_LOAD32_BYTE( "shotprm1.139", 0x00001, 0x20000, CRC(f9d120c5) SHA1(f94216f1fb6d810ddee98479e83f0719b30b768f) )
	ROM_LOAD32_BYTE( "shotprm2.139", 0x00002, 0x20000, CRC(9f12414d) SHA1(c1120079173f7ed6118f7105443afd7d38d8af94) )
	ROM_LOAD32_BYTE( "shotprm3.139", 0x00003, 0x20000, CRC(108a69be) SHA1(1b2ebe4767be084707522a90f009d3a70e03d578) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "shotsnd.u88", 0x10000, 0x18000, CRC(e37d599d) SHA1(105f91e968ecf553d910a97726ddc536289bbb2b) )
	ROM_CONTINUE(        0x08000, 0x08000 )

	ROM_REGION( 0x800000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "shf_grom.0_0", 0x000000, 0x80000, CRC(832a3d6a) SHA1(443328fa61b79c93ec6c9d24893b2ec38358a905) )
	ROM_LOAD32_BYTE( "shf_grom.0_1", 0x000001, 0x80000, CRC(155e48a2) SHA1(187d65423ff9a3d6b6c34c885a1b2397fa5371cf) )
	ROM_LOAD32_BYTE( "shf_grom.0_2", 0x000002, 0x80000, CRC(9f2b470d) SHA1(012e917856042cbe00d476e3220a7f9c841bd199) )
	ROM_LOAD32_BYTE( "shf_grom.0_3", 0x000003, 0x80000, CRC(3855a16a) SHA1(f8c03efab87ddcb6940f657ad1f0138ceaa2118e) )
	ROM_LOAD32_BYTE( "shf_grom.1_0", 0x200000, 0x80000, CRC(ed140389) SHA1(f438a887b44a277f81e954bef73ac478eaff58c8) )
	ROM_LOAD32_BYTE( "shf_grom.1_1", 0x200001, 0x80000, CRC(bd2ffbca) SHA1(667692ce61a4896ceecf2a2bb37f742f175a6152) )
	ROM_LOAD32_BYTE( "shf_grom.1_2", 0x200002, 0x80000, CRC(c6de4187) SHA1(4854604330bb14f862abe22d755e08b54e0b1a04) )
	ROM_LOAD32_BYTE( "shf_grom.1_3", 0x200003, 0x80000, CRC(0c707aa2) SHA1(1da83523e04eeae4dbc8748a31a074331bf975d1) )
	ROM_LOAD32_BYTE( "shf_grom.2_0", 0x400000, 0x80000, CRC(529b4259) SHA1(4f98f28c83c3f8f822ea45d31be526af4a504cbc) )
	ROM_LOAD32_BYTE( "shf_grom.2_1", 0x400001, 0x80000, CRC(4b52ab1a) SHA1(5c438df7f2edea8f4d8734408fd94acf9d340755) )
	ROM_LOAD32_BYTE( "shf_grom.2_2", 0x400002, 0x80000, CRC(f45fad03) SHA1(3ff062928ef5bcdce8748ddd972c5da67207227a) )
	ROM_LOAD32_BYTE( "shf_grom.2_3", 0x400003, 0x80000, CRC(1bcb26c8) SHA1(49e730c56c4a3171a2962fa65f3b16481590c636) )
	ROM_LOAD32_BYTE( "shf_grom.3_0", 0x600000, 0x80000, CRC(a29763db) SHA1(e2154fb3e400aba300f1a23d53504588426dfbfe) )
	ROM_LOAD32_BYTE( "shf_grom.3_1", 0x600001, 0x80000, CRC(c757084c) SHA1(6f78ee13c68afd635aa3871cddf7207a19d4039b) )
	ROM_LOAD32_BYTE( "shf_grom.3_2", 0x600002, 0x80000, CRC(2971cb25) SHA1(5f0508ebff1bf66ee0f78addfa09a43f8fff9a36) )
	ROM_LOAD32_BYTE( "shf_grom.3_3", 0x600003, 0x80000, CRC(4fcbee51) SHA1(4e2ec4475986c3916c4566b2bc007f41a8c13609) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "shf_srom.0", 0x000000, 0x80000, CRC(9a3cb6c9) SHA1(2af3ce3b1cd7688199195a66cf01bb83775d42fa) )
	ROM_LOAD16_BYTE( "shf_srom.1", 0x200000, 0x80000, CRC(8c89948a) SHA1(1054eca5de352c17f34f31ef16297ba6177a37ba) )
ROM_END


ROM_START( sshot137 )	/* Version 1.37 (PCB P/N 1082 Rev 2) */
	ROM_REGION( 0x8000, REGION_CPU1, 0 )

	ROM_REGION32_BE( 0x80000, REGION_USER1, ROMREGION_DISPOSE )
	ROM_LOAD32_BYTE( "shotprm0.137", 0x00000, 0x20000, CRC(6499c76f) SHA1(60fdaefb09088ac609addd40569bd7fab12593bc) )
	ROM_LOAD32_BYTE( "shotprm1.137", 0x00001, 0x20000, CRC(64fb47a4) SHA1(32ce9d91b16b8aaf545c0a22842ad8d806727a17) )
	ROM_LOAD32_BYTE( "shotprm2.137", 0x00002, 0x20000, CRC(e0df3025) SHA1(edff5c5c4486981ac0783f337a0845854d0217f0) )
	ROM_LOAD32_BYTE( "shotprm3.137", 0x00003, 0x20000, CRC(efa66ad8) SHA1(d8dc754529284e6c06b912e226c8a4520aab49fc) )

	ROM_REGION( 0x28000, REGION_CPU2, 0 )
	ROM_LOAD( "shotsnd.u88", 0x10000, 0x18000, CRC(e37d599d) SHA1(105f91e968ecf553d910a97726ddc536289bbb2b) )
	ROM_CONTINUE(        0x08000, 0x08000 )

	ROM_REGION( 0x800000, REGION_GFX1, 0 )
	ROM_LOAD32_BYTE( "shf_grom.0_0", 0x000000, 0x80000, CRC(832a3d6a) SHA1(443328fa61b79c93ec6c9d24893b2ec38358a905) )
	ROM_LOAD32_BYTE( "shf_grom.0_1", 0x000001, 0x80000, CRC(155e48a2) SHA1(187d65423ff9a3d6b6c34c885a1b2397fa5371cf) )
	ROM_LOAD32_BYTE( "shf_grom.0_2", 0x000002, 0x80000, CRC(9f2b470d) SHA1(012e917856042cbe00d476e3220a7f9c841bd199) )
	ROM_LOAD32_BYTE( "shf_grom.0_3", 0x000003, 0x80000, CRC(3855a16a) SHA1(f8c03efab87ddcb6940f657ad1f0138ceaa2118e) )
	ROM_LOAD32_BYTE( "shf_grom.1_0", 0x200000, 0x80000, CRC(ed140389) SHA1(f438a887b44a277f81e954bef73ac478eaff58c8) )
	ROM_LOAD32_BYTE( "shf_grom.1_1", 0x200001, 0x80000, CRC(bd2ffbca) SHA1(667692ce61a4896ceecf2a2bb37f742f175a6152) )
	ROM_LOAD32_BYTE( "shf_grom.1_2", 0x200002, 0x80000, CRC(c6de4187) SHA1(4854604330bb14f862abe22d755e08b54e0b1a04) )
	ROM_LOAD32_BYTE( "shf_grom.1_3", 0x200003, 0x80000, CRC(0c707aa2) SHA1(1da83523e04eeae4dbc8748a31a074331bf975d1) )
	ROM_LOAD32_BYTE( "shf_grom.2_0", 0x400000, 0x80000, CRC(529b4259) SHA1(4f98f28c83c3f8f822ea45d31be526af4a504cbc) )
	ROM_LOAD32_BYTE( "shf_grom.2_1", 0x400001, 0x80000, CRC(4b52ab1a) SHA1(5c438df7f2edea8f4d8734408fd94acf9d340755) )
	ROM_LOAD32_BYTE( "shf_grom.2_2", 0x400002, 0x80000, CRC(f45fad03) SHA1(3ff062928ef5bcdce8748ddd972c5da67207227a) )
	ROM_LOAD32_BYTE( "shf_grom.2_3", 0x400003, 0x80000, CRC(1bcb26c8) SHA1(49e730c56c4a3171a2962fa65f3b16481590c636) )
	ROM_LOAD32_BYTE( "shf_grom.3_0", 0x600000, 0x80000, CRC(a29763db) SHA1(e2154fb3e400aba300f1a23d53504588426dfbfe) )
	ROM_LOAD32_BYTE( "shf_grom.3_1", 0x600001, 0x80000, CRC(c757084c) SHA1(6f78ee13c68afd635aa3871cddf7207a19d4039b) )
	ROM_LOAD32_BYTE( "shf_grom.3_2", 0x600002, 0x80000, CRC(2971cb25) SHA1(5f0508ebff1bf66ee0f78addfa09a43f8fff9a36) )
	ROM_LOAD32_BYTE( "shf_grom.3_3", 0x600003, 0x80000, CRC(4fcbee51) SHA1(4e2ec4475986c3916c4566b2bc007f41a8c13609) )

	ROM_REGION16_BE( 0x400000, REGION_SOUND1, ROMREGION_ERASE00 )
	ROM_LOAD16_BYTE( "shf_srom.0", 0x000000, 0x80000, CRC(9a3cb6c9) SHA1(2af3ce3b1cd7688199195a66cf01bb83775d42fa) )
	ROM_LOAD16_BYTE( "shf_srom.1", 0x200000, 0x80000, CRC(8c89948a) SHA1(1054eca5de352c17f34f31ef16297ba6177a37ba) )
ROM_END


/*************************************
 *
 *	Driver-specific init
 *
 *************************************/

static void init_program_rom(void)
{
	memcpy(main_rom, memory_region(REGION_USER1), memory_region_length(REGION_USER1));
	memcpy(memory_region(REGION_CPU1), memory_region(REGION_USER1), 0x80);
}


static void init_sound_speedup(offs_t offset, offs_t pc)
{
	sound_speedup_data = install_mem_read_handler(1, offset, offset, sound_speedup_r);
	sound_speedup_pc = pc;
}


static DRIVER_INIT( timekill )
{
	init_program_rom();
	init_sound_speedup(0x2010, 0x8c54);
	itech32_vram_height = 512;
	itech32_planes = 2;
	is_drivedge = 0;
}


static DRIVER_INIT( hardyard )
{
	init_program_rom();
	init_sound_speedup(0x2010, 0x8e16);
	itech32_vram_height = 1024;
	itech32_planes = 1;
	is_drivedge = 0;
}


static DRIVER_INIT( bloodstm )
{
	init_program_rom();
	init_sound_speedup(0x2011, 0x8ebf);
	itech32_vram_height = 1024;
	itech32_planes = 1;
	is_drivedge = 0;
}


static DRIVER_INIT( wcbowl )
{
	init_program_rom();
	init_sound_speedup(0x2011, 0x8f93);
	itech32_vram_height = 1024;
	itech32_planes = 1;

	install_mem_read16_handler(0, 0x680000, 0x680001, trackball_r);

	install_mem_read16_handler(0, 0x578000, 0x57ffff, MRA16_NOP);
	install_mem_read16_handler(0, 0x680080, 0x680081, wcbowl_prot_result_r);
	install_mem_write16_handler(0, 0x680080, 0x680081, MWA16_NOP);
}


static DRIVER_INIT( drivedge )
{
	init_program_rom();
//	init_sound_speedup(0x2011, 0x8ebf);
	itech32_vram_height = 1024;
	itech32_planes = 1;
	is_drivedge = 1;
}


static void init_sftm_common(int prot_addr, int sound_pc)
{
	init_program_rom();
	init_sound_speedup(0x2011, sound_pc);
	itech32_vram_height = 1024;
	itech32_planes = 1;
	is_drivedge = 0;

	itech020_prot_address = prot_addr;

	install_mem_write32_handler(0, 0x300000, 0x300003, itech020_color2_w);
	install_mem_write32_handler(0, 0x380000, 0x380003, itech020_color1_w);
}


static DRIVER_INIT( sftm )
{
	init_sftm_common(0x7a6a, 0x905f);
}


static DRIVER_INIT( sftm110 )
{
	init_sftm_common(0x7a66, 0x9059);
}


static void init_shuffle_bowl_common(int prot_addr, int sound_pc)
{
	/*
		The newest versions of World Class Bowling are on the same exact
		platform as Shuffle Shot. So We'll use the same general INIT
		routine for these two programs.  IE: PCB P/N 1082 Rev 2
	*/
	init_program_rom();
	init_sound_speedup(0x2011, sound_pc);
	itech32_vram_height = 1024;
	itech32_planes = 1;
	is_drivedge = 0;

	itech020_prot_address = prot_addr;

	install_mem_write32_handler(0, 0x300000, 0x300003, itech020_color2_w);
	install_mem_write32_handler(0, 0x380000, 0x380003, itech020_color1_w);
	install_mem_read32_handler(0, 0x180800, 0x180803, trackball32_4bit_r);
	install_mem_read32_handler(0, 0x181000, 0x181003, trackball32_4bit_p2_r);
}

static DRIVER_INIT( shufshot )	/* PIC 16C54 labeled as ITSHF-1 */
{
	init_shuffle_bowl_common(0x111a, 0x906c);
}

static DRIVER_INIT( wcbowln )	/* PIC 16C54 labeled as ITBWL-3 */
{
	/* The security PROM is NOT interchangable between the Deluxe and "normal" versions. */
	init_shuffle_bowl_common(0x1116, 0x9067);
}




/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1992, timekill, 0,        timekill, timekill, timekill, ROT0, "Strata/Incredible Technologies", "Time Killers (v1.32)" )
GAME( 1992, timek131, timekill, timekill, timekill, timekill, ROT0, "Strata/Incredible Technologies", "Time Killers (v1.31)" )
GAME( 1993, hardyard, 0,        bloodstm, hardyard, hardyard, ROT0, "Strata/Incredible Technologies", "Hard Yardage (v1.20)" )
GAME( 1993, hardyd10, hardyard, bloodstm, hardyard, hardyard, ROT0, "Strata/Incredible Technologies", "Hard Yardage (v1.00)" )
GAME( 1994, bloodstm, 0,        bloodstm, bloodstm, bloodstm, ROT0, "Strata/Incredible Technologies", "Blood Storm (v2.22)" )
GAME( 1994, bloods22, bloodstm, bloodstm, bloodstm, bloodstm, ROT0, "Strata/Incredible Technologies", "Blood Storm (v2.20)" )
GAME( 1994, bloods21, bloodstm, bloodstm, bloodstm, bloodstm, ROT0, "Strata/Incredible Technologies", "Blood Storm (v2.10)" )
GAME( 1994, bloods11, bloodstm, bloodstm, bloodstm, bloodstm, ROT0, "Strata/Incredible Technologies", "Blood Storm (v1.10)" )
GAME( 1994, pairs,    0,        pairs,    pairs,    bloodstm, ROT0, "Strata/Incredible Technologies", "Pairs (V1.2, 09/30/94)" )
GAME( 1994, pairsa,   pairs,    pairs,    pairs,    bloodstm, ROT0, "Strata/Incredible Technologies", "Pairs (09/07/94)" )
GAMEX(1994, drivedge, 0,        drivedge, drivedge, drivedge, ROT0, "Strata/Incredible Technologies", "Driver's Edge", GAME_NOT_WORKING )
GAME( 1995, wcbowl,   0,        sftm,     wcbowln,  wcbowln,  ROT0, "Incredible Technologies", "World Class Bowling (v1.66)" ) /* PIC 16C54 labeled as ITBWL-3 */
GAME( 1995, wcbwl165, wcbowl,   sftm,     shufbowl, wcbowln,  ROT0, "Incredible Technologies", "World Class Bowling (v1.65)" ) /* PIC 16C54 labeled as ITBWL-3 */
GAME( 1995, wcbwl161, wcbowl,   sftm,     shufbowl, wcbowln,  ROT0, "Incredible Technologies", "World Class Bowling (v1.61)" ) /* PIC 16C54 labeled as ITBWL-3 */
GAME( 1995, wcbwl12,  wcbowl,   wcbowl,   wcbowl,   wcbowl,   ROT0, "Incredible Technologies", "World Class Bowling (v1.2)" ) /* PIC 16C54 labeled as ITBWL-1 */
GAME( 1995, sftm,     0,        sftm,     sftm,     sftm,     ROT0, "Capcom/Incredible Technologies", "Street Fighter: The Movie (v1.12)" )	/* PIC 16C54 labeled as ITSF-1 */
GAME( 1995, sftm111,  sftm,     sftm,     sftm,     sftm110,  ROT0, "Capcom/Incredible Technologies", "Street Fighter: The Movie (v1.11)" )	/* PIC 16C54 labeled as ITSF-1 */
GAME( 1995, sftm110,  sftm,     sftm,     sftm,     sftm110,  ROT0, "Capcom/Incredible Technologies", "Street Fighter: The Movie (v1.10)" )	/* PIC 16C54 labeled as ITSF-1 */
GAME( 1995, sftmj,    sftm,     sftm,     sftm,     sftm,     ROT0, "Capcom/Incredible Technologies", "Street Fighter: The Movie (v1.12N, Japan)" )	/* PIC 16C54 labeled as ITSF-1 */
GAME( 1997, shufshot, 0,        sftm,     shufshot, shufshot, ROT0, "Strata/Incredible Technologies", "Shuffleshot (v1.39)" ) /* PIC 16C54 labeled as ITSHF-1 */
GAME( 1997, sshot137, shufshot, sftm,     shufbowl, shufshot, ROT0, "Strata/Incredible Technologies", "Shuffleshot (v1.37)" ) /* PIC 16C54 labeled as ITSHF-1 */
