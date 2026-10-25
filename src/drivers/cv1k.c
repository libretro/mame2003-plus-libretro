/***************************************************************************

	Cave CV1000 hardware

	From current MAME (src/mame/cave/cv1k.cpp: David Haywood, Luca Elia,
	MetalliC; the NAND flash of src/devices/machine/nandflash.cpp and the
	RTC9701 of src/devices/machine/rtc9701.cpp).

	CPU:   Hitachi SH7709S (SH-3) at 102.4 MHz
	Sound: Yamaha YMZ770C-F at 16.384 MHz
	Video: Altera Cyclone EP1C12 FPGA blitter (vidhrdw/cv1k_vidhrdw.c)

	U4:  program ROM (the code is copied to work RAM and runs there)
	U2:  128 MB NAND flash with the graphics (and the bad block table)
	U23, U24: sound flashes
	U10: RTC9701, serial RTC and 256 x 16 bit EEPROM

	To enter service mode hold Service (the test button) for a few
	seconds. Common game codes:
	 - during boot hold P1 Right+A+B+C and P2 Left+A+B+C: initialise the
	   non volatile data (EEPROM or NAND settings area)
	 - during boot hold P1 A and P2 A: reset the random number generator
	   at each game start

***************************************************************************/

#include <time.h>
#include <string.h>
#include "driver.h"
#include "state.h"
#include "cpu/sh3/sh3.h"
#include "includes/cv1k.h"

data32_t *cv1k_ram;
UINT32 cv1k_ram_size;

static UINT32 cv1k_idleramoffs, cv1k_idlepc;


/***************************************************************************
	NAND flash: Samsung K9F1G08U0M (nandflash.cpp)
***************************************************************************/

#define NAND_PAGE_DATA_SIZE		2048
#define NAND_PAGE_TOTAL_SIZE	(2048 + 64)
#define NAND_LOG2_PAGES_PER_BLOCK	6
#define NAND_NUM_PAGES			(64 * 1024)
#define NAND_NUM_BLOCKS			(NAND_NUM_PAGES >> NAND_LOG2_PAGES_PER_BLOCK)
#define NAND_BLOCK_SIZE			(NAND_PAGE_TOTAL_SIZE << NAND_LOG2_PAGES_PER_BLOCK)
#define NAND_COL_CYCLES			2
#define NAND_ROW_CYCLES			2
#define NAND_READ_TIME			TIME_IN_USEC(25)

enum
{
	SM_M_INIT, SM_M_READ, SM_M_PROGRAM, SM_M_ERASE, SM_M_READSTATUS, SM_M_READID,
	SM_M_30, SM_M_RANDOM_DATA_INPUT, SM_M_RANDOM_DATA_OUTPUT
};

enum { SM_PM_A, SM_PM_B, SM_PM_C };

static const UINT8 nand_id[4] = { 0xec, 0xf1, 0x00, 0x15 };

static UINT8 *nand_data;
static UINT8 nand_dirty[NAND_NUM_BLOCKS];
static UINT8 nand_pagereg[NAND_PAGE_TOTAL_SIZE];
static UINT8 nand_mode, nand_pointer_mode, nand_status, nand_accumulated_status, nand_mode_3065;
static UINT32 nand_page_addr, nand_byte_addr, nand_addr_load_ptr, nand_program_byte_count;
static double nand_busy_until;		/* < 0: never */

static void nand_reset(void)
{
	nand_mode = SM_M_INIT;
	nand_pointer_mode = SM_PM_A;
	nand_page_addr = 0;
	nand_byte_addr = 0;
	nand_busy_until = -1;
	nand_accumulated_status = 0;
	nand_mode_3065 = 0;
	nand_status = 0xc0;
	memset(nand_pagereg, 0, sizeof(nand_pagereg));
}

static int nand_is_busy(void)
{
	if (nand_busy_until >= 0 && timer_get_time() < nand_busy_until)
		return 1;
	return (nand_status & 0x40) == 0;
}

static void nand_command_w(data8_t data)
{
	int i;

	switch (data)
	{
		case 0xff:	/* reset */
			nand_busy_until = -1;
			nand_mode = SM_M_INIT;
			nand_pointer_mode = SM_PM_A;
			nand_status = (nand_status & 0x80) | 0x40;
			nand_accumulated_status = 0;
			nand_mode_3065 = 0;
			break;

		case 0x00:	/* read, 1st cycle */
			nand_mode = SM_M_READ;
			nand_pointer_mode = SM_PM_A;
			nand_addr_load_ptr = 0;
			break;

		case 0x01:	/* upper half: not on 2048 byte pages */
		case 0x50:	/* spare area: not on large pages */
			nand_mode = SM_M_INIT;
			break;

		case 0x80:	/* page program, 1st cycle */
			nand_mode = SM_M_PROGRAM;
			nand_addr_load_ptr = 0;
			nand_program_byte_count = 0;
			memset(nand_pagereg, 0xff, sizeof(nand_pagereg));
			break;

		case 0x10:	/* page program, 2nd cycle */
		case 0x15:
			if (nand_mode != SM_M_PROGRAM && nand_mode != SM_M_RANDOM_DATA_INPUT)
				nand_mode = SM_M_INIT;
			else
			{
				nand_status = (nand_status & 0x80) | nand_accumulated_status;
				if (nand_page_addr < NAND_NUM_PAGES)
				{
					for (i = 0; i < NAND_PAGE_TOTAL_SIZE; i++)
						nand_data[nand_page_addr * NAND_PAGE_TOTAL_SIZE + i] &= nand_pagereg[i];
					nand_dirty[nand_page_addr >> NAND_LOG2_PAGES_PER_BLOCK] = 1;
				}
				nand_status |= 0x40;
				if (data == 0x15)
					nand_accumulated_status = nand_status & 0x1f;
				else
					nand_accumulated_status = 0;
				nand_mode = SM_M_INIT;
			}
			break;

		case 0x60:	/* block erase, 1st cycle */
			nand_mode = SM_M_ERASE;
			nand_page_addr = 0;
			nand_addr_load_ptr = 0;
			break;

		case 0xd0:	/* block erase, 2nd cycle */
			if (nand_mode != SM_M_ERASE)
				nand_mode = SM_M_INIT;
			else
			{
				UINT32 block = (nand_page_addr >> NAND_LOG2_PAGES_PER_BLOCK) & (NAND_NUM_BLOCKS - 1);
				nand_status &= 0x80;
				memset(nand_data + block * NAND_BLOCK_SIZE, 0xff, NAND_BLOCK_SIZE);
				nand_dirty[block] = 1;
				nand_status |= 0x40;
				nand_mode = SM_M_INIT;
				if (nand_pointer_mode == SM_PM_B)
					nand_pointer_mode = SM_PM_A;
			}
			break;

		case 0x70:	/* read status */
			nand_mode = SM_M_READSTATUS;
			break;

		case 0x90:	/* read ID */
			nand_mode = SM_M_READID;
			nand_addr_load_ptr = 0;
			break;

		case 0x30:	/* read, 2nd cycle */
			if (nand_mode != SM_M_READ)
				nand_mode = SM_M_INIT;
			else if (nand_addr_load_ptr < NAND_COL_CYCLES + NAND_ROW_CYCLES)
				nand_mode = SM_M_INIT;
			else
				nand_busy_until = timer_get_time() + NAND_READ_TIME;
			break;

		case 0x65:
			if (nand_mode != SM_M_30)
				nand_mode = SM_M_INIT;
			else
				nand_mode_3065 = 1;
			break;

		case 0x05:	/* random data output, 1st cycle */
			if (nand_mode != SM_M_READ && nand_mode != SM_M_RANDOM_DATA_OUTPUT)
				nand_mode = SM_M_INIT;
			else
			{
				nand_mode = SM_M_RANDOM_DATA_OUTPUT;
				nand_addr_load_ptr = 0;
			}
			break;

		case 0xe0:	/* random data output, 2nd cycle */
			if (nand_mode != SM_M_RANDOM_DATA_OUTPUT)
				nand_mode = SM_M_INIT;
			else
				nand_busy_until = timer_get_time() + NAND_READ_TIME;
			break;

		case 0x85:	/* random data input */
			if (nand_mode != SM_M_PROGRAM && nand_mode != SM_M_RANDOM_DATA_INPUT)
				nand_mode = SM_M_INIT;
			else
			{
				nand_mode = SM_M_RANDOM_DATA_INPUT;
				nand_addr_load_ptr = 0;
				nand_program_byte_count = 0;
			}
			break;

		default:
			nand_mode = SM_M_INIT;
			break;
	}
}

static void nand_address_w(data8_t data)
{
	switch (nand_mode)
	{
		case SM_M_READ:
		case SM_M_PROGRAM:
			if (nand_addr_load_ptr == 0)
				nand_page_addr = 0;
			if (nand_addr_load_ptr < NAND_COL_CYCLES)
			{
				nand_byte_addr &= ~(0xff << (nand_addr_load_ptr * 8));
				nand_byte_addr |= (data << (nand_addr_load_ptr * 8));
			}
			else if (nand_addr_load_ptr < NAND_COL_CYCLES + NAND_ROW_CYCLES)
			{
				nand_page_addr &= ~(0xff << ((nand_addr_load_ptr - NAND_COL_CYCLES) * 8));
				nand_page_addr |= (data << ((nand_addr_load_ptr - NAND_COL_CYCLES) * 8));
			}
			nand_addr_load_ptr++;
			break;

		case SM_M_ERASE:
			if (nand_addr_load_ptr < NAND_ROW_CYCLES)
			{
				nand_page_addr &= ~(0xff << (nand_addr_load_ptr * 8));
				nand_page_addr |= (data << (nand_addr_load_ptr * 8));
			}
			nand_addr_load_ptr++;
			break;

		case SM_M_RANDOM_DATA_INPUT:
		case SM_M_RANDOM_DATA_OUTPUT:
			if (nand_addr_load_ptr < NAND_COL_CYCLES)
			{
				nand_byte_addr &= ~(0xff << (nand_addr_load_ptr * 8));
				nand_byte_addr |= (data << (nand_addr_load_ptr * 8));
			}
			nand_addr_load_ptr++;
			break;

		case SM_M_READID:
			if (nand_addr_load_ptr == 0)
				nand_byte_addr = data;
			nand_addr_load_ptr++;
			break;

		default:
			break;
	}
}

static data8_t nand_data_r(void)
{
	data8_t reply = 0;

	switch (nand_mode)
	{
		case SM_M_READ:
		case SM_M_RANDOM_DATA_OUTPUT:
			if (!nand_mode_3065)
			{
				if (nand_byte_addr < NAND_PAGE_TOTAL_SIZE && nand_page_addr < NAND_NUM_PAGES)
					reply = nand_data[nand_page_addr * NAND_PAGE_TOTAL_SIZE + nand_byte_addr];
				else
					reply = 0xff;
			}
			else
				reply = 0xff;
			nand_byte_addr++;	/* no sequential row read on this chip */
			break;

		case SM_M_READSTATUS:
			reply = nand_status & 0xc1;
			if (nand_is_busy())
				reply &= ~0x40;
			break;

		case SM_M_READID:
			if (nand_byte_addr < 4)
				reply = nand_id[nand_byte_addr];
			else
				reply = 0;
			nand_byte_addr++;
			break;

		default:
			break;
	}
	return reply;
}

static void nand_data_w(data8_t data)
{
	switch (nand_mode)
	{
		case SM_M_PROGRAM:
		case SM_M_RANDOM_DATA_INPUT:
			if (nand_program_byte_count++ < NAND_PAGE_TOTAL_SIZE && nand_byte_addr < NAND_PAGE_TOTAL_SIZE)
				nand_pagereg[nand_byte_addr] = data;
			nand_byte_addr++;
			if (nand_byte_addr == NAND_PAGE_TOTAL_SIZE)
				nand_byte_addr = (nand_pointer_mode != SM_PM_C) ? 0 : NAND_PAGE_DATA_SIZE;
			break;

		default:
			break;
	}
}


/***************************************************************************
	RTC9701: serial RTC and EEPROM (rtc9701.cpp)
***************************************************************************/

enum { RTC_CMD_WAIT = 0, RTC_READ, RTC_WRITE, RTC_EEPROM_READ, RTC_EEPROM_WRITE, RTC_AFTER_WRITE_ENABLE };

static INT32 rtc_latch, rtc_reset_line, rtc_clock_line;
static INT32 rtc_state, rtc_cmd_stream_pos, rtc_current_cmd;
static INT32 rtc_address_pos, rtc_current_address, rtc_data_pos;
static UINT16 rtc_current_data;
static UINT16 rtc_eeprom[0x100];
static UINT8 rtc_sec, rtc_min, rtc_hour, rtc_day, rtc_wday, rtc_month, rtc_year;
static UINT8 rtc_out_old[3];		/* the last levels written to DI, CLK, CS */

static void rtc_timer_callback(int param)
{
	static const UINT8 dpm[12] = { 0x31, 0x28, 0x31, 0x30, 0x31, 0x30, 0x31, 0x31, 0x30, 0x31, 0x30, 0x31 };
	int dpm_count;

	rtc_sec++;
	if ((rtc_sec & 0x0f) >= 0x0a)				{ rtc_sec += 0x10; rtc_sec &= 0xf0; }
	if ((rtc_sec & 0xf0) >= 0x60)				{ rtc_min++; rtc_sec = 0; }
	if ((rtc_min & 0x0f) >= 0x0a)				{ rtc_min += 0x10; rtc_min &= 0xf0; }
	if ((rtc_min & 0xf0) >= 0x60)				{ rtc_hour++; rtc_min = 0; }
	if ((rtc_hour & 0x0f) >= 0x0a)				{ rtc_hour += 0x10; rtc_hour &= 0xf0; }
	if ((rtc_hour & 0xff) >= 0x24)				{ rtc_day++; rtc_wday <<= 1; rtc_hour = 0; }
	if (rtc_wday & 0x80)						{ rtc_wday = 1; }
	if ((rtc_day & 0x0f) >= 0x0a)				{ rtc_day += 0x10; rtc_day &= 0xf0; }

	dpm_count = (rtc_month & 0xf) + (((rtc_month & 0x10) >> 4) * 10) - 1;
	if (dpm_count < 0 || dpm_count > 11)
		dpm_count = 0;
	if (((rtc_year % 4) == 0) && rtc_month == 2)
	{
		if ((rtc_day & 0xff) >= dpm[dpm_count] + 1 + 1)
			{ rtc_month++; rtc_day = 0x01; }
	}
	else if ((rtc_day & 0xff) >= dpm[dpm_count] + 1)	{ rtc_month++; rtc_day = 0x01; }
	if ((rtc_month & 0x0f) >= 0x0a)				{ rtc_month = 0x10; }
	if (rtc_month >= 0x13)						{ rtc_year++; rtc_month = 1; }
	if ((rtc_year & 0x0f) >= 0x0a)				{ rtc_year += 0x10; rtc_year &= 0xf0; }
	if ((rtc_year & 0xf0) >= 0xa0)				{ rtc_year = 0; }
}

static void rtc_set_from_system(void)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	int year, month, day, wday, hour, minute, second;

	if (tm)
	{
		year = tm->tm_year + 1900; month = tm->tm_mon + 1; day = tm->tm_mday;
		wday = tm->tm_wday + 1; hour = tm->tm_hour; minute = tm->tm_min; second = tm->tm_sec;
	}
	else
	{
		year = 2004; month = 1; day = 1; wday = 5; hour = 0; minute = 0; second = 0;
	}

	rtc_day = ((day / 10) << 4) | ((day % 10) & 0xf);
	rtc_month = ((month / 10) << 4) | ((month % 10) & 0xf);
	rtc_wday = 1 << (wday - 1);
	rtc_year = (((year % 100) / 10) << 4) | ((year % 10) & 0xf);
	rtc_hour = ((hour / 10) << 4) | ((hour % 10) & 0xf);
	rtc_min = ((minute / 10) << 4) | ((minute % 10) & 0xf);
	rtc_sec = ((second / 10) << 4) | ((second % 10) & 0xf);
}

static UINT8 rtc_read(UINT8 offset)
{
	switch (offset)
	{
		case 0: return rtc_sec;
		case 1: return rtc_min;
		case 2: return rtc_hour;
		case 3: return rtc_wday;
		case 4: return rtc_day;
		case 5: return rtc_month;
		case 6: return rtc_year;
		case 7: return 0x20;
	}
	return 0;
}

static void rtc_write(UINT8 offset, UINT8 data)
{
	switch (offset)
	{
		case 0: rtc_sec = data; break;
		case 1: rtc_min = data; break;
		case 2: rtc_hour = data; break;
		case 3: rtc_wday = data; break;
		case 4: rtc_day = data; break;
		case 5: rtc_month = data; break;
		case 6: rtc_year = data; break;
	}
}

static int rtc_read_bit(void)
{
	if (rtc_state == RTC_READ || rtc_state == RTC_EEPROM_READ)
		return (rtc_current_data >> (rtc_data_pos - 1)) & 1;
	return 0;
}

static void rtc_set_cs_line(int state)
{
	rtc_reset_line = state;
	if (rtc_reset_line != CLEAR_LINE)
	{
		rtc_state = RTC_CMD_WAIT;
		rtc_cmd_stream_pos = 0;
		rtc_current_cmd = 0;
		rtc_address_pos = 0;
		rtc_current_address = 0;
		rtc_current_data = 0;
		rtc_data_pos = 0;
	}
}

static void rtc_set_clock_line(int state)
{
	if (rtc_reset_line != CLEAR_LINE || state != 1)
		return;

	switch (rtc_state)
	{
		case RTC_CMD_WAIT:
			rtc_current_cmd = (rtc_current_cmd << 1) | (rtc_latch & 1);
			rtc_cmd_stream_pos++;
			if (rtc_cmd_stream_pos == 4)
			{
				rtc_cmd_stream_pos = 0;
				if (rtc_current_cmd == 0x00 || rtc_current_cmd == 0x02 || rtc_current_cmd == 0x08 || rtc_current_cmd == 0x0a)
				{
					rtc_state = (rtc_current_cmd == 0x00) ? RTC_WRITE :
								(rtc_current_cmd == 0x02) ? RTC_EEPROM_WRITE :
								(rtc_current_cmd == 0x08) ? RTC_READ : RTC_EEPROM_READ;
					rtc_address_pos = 0;
					rtc_current_address = 0;
					rtc_data_pos = 0;
					rtc_current_data = 0;
				}
				else if (rtc_current_cmd == 0x06)
					rtc_state = RTC_AFTER_WRITE_ENABLE;
				rtc_current_cmd = 0;
			}
			break;

		case RTC_AFTER_WRITE_ENABLE:
			rtc_cmd_stream_pos++;
			if (rtc_cmd_stream_pos == 12)
			{
				rtc_cmd_stream_pos = 0;
				rtc_state = RTC_CMD_WAIT;
			}
			break;

		case RTC_WRITE:
			rtc_cmd_stream_pos++;
			if (rtc_cmd_stream_pos <= 4)
			{
				rtc_address_pos++;
				rtc_current_address = (rtc_current_address << 1) | (rtc_latch & 1);
			}
			if (rtc_cmd_stream_pos > 4)
			{
				rtc_data_pos++;
				rtc_current_data = (rtc_current_data << 1) | (rtc_latch & 1);
			}
			if (rtc_cmd_stream_pos == 12)
			{
				rtc_cmd_stream_pos = 0;
				rtc_write(rtc_current_address, rtc_current_data);
				rtc_state = RTC_CMD_WAIT;
			}
			break;

		case RTC_EEPROM_READ:
			rtc_cmd_stream_pos++;
			if (rtc_cmd_stream_pos <= 12)
			{
				rtc_address_pos++;
				rtc_current_address = (rtc_current_address << 1) | (rtc_latch & 1);
				if (rtc_cmd_stream_pos == 12)
				{
					rtc_current_data = rtc_eeprom[(rtc_current_address >> 1) & 0xff];
					rtc_data_pos = 16;
				}
			}
			if (rtc_cmd_stream_pos > 12)
				rtc_data_pos--;
			if (rtc_cmd_stream_pos == 28)
				rtc_cmd_stream_pos = 0;
			break;

		case RTC_EEPROM_WRITE:
			rtc_cmd_stream_pos++;
			if (rtc_cmd_stream_pos <= 12)
			{
				rtc_address_pos++;
				rtc_current_address = (rtc_current_address << 1) | (rtc_latch & 1);
			}
			if (rtc_cmd_stream_pos > 12)
			{
				rtc_data_pos++;
				rtc_current_data = (rtc_current_data << 1) | (rtc_latch & 1);
			}
			if (rtc_cmd_stream_pos == 28)
			{
				rtc_cmd_stream_pos = 0;
				rtc_eeprom[(rtc_current_address >> 1) & 0xff] = rtc_current_data;
				rtc_state = RTC_CMD_WAIT;
			}
			break;

		case RTC_READ:
			rtc_cmd_stream_pos++;
			if (rtc_cmd_stream_pos <= 4)
			{
				rtc_address_pos++;
				rtc_current_address = (rtc_current_address << 1) | (rtc_latch & 1);
				if (rtc_cmd_stream_pos == 4)
				{
					rtc_current_data = rtc_read(rtc_current_address);
					rtc_data_pos = 8;
				}
			}
			if (rtc_cmd_stream_pos > 4)
				rtc_data_pos--;
			if (rtc_cmd_stream_pos == 12)
				rtc_cmd_stream_pos = 0;
			break;
	}
}

/* the EEPROMOUT port of current MAME: bit 0 DI, bit 1 CLK (active high),
   bit 2 CS (active low); a line handler runs only when its level changes */
static void rtc_port_w(data8_t data)
{
	UINT8 di = data & 1;
	UINT8 clk = (data >> 1) & 1;
	UINT8 cs = ((data >> 2) & 1) ^ 1;

	if (di != rtc_out_old[0])
	{
		rtc_out_old[0] = di;
		rtc_latch = di;
	}
	if (clk != rtc_out_old[1])
	{
		rtc_out_old[1] = clk;
		rtc_set_clock_line(clk);
	}
	if (cs != rtc_out_old[2])
	{
		rtc_out_old[2] = cs;
		rtc_set_cs_line(cs);
	}
}


/***************************************************************************
	memory handlers
***************************************************************************/

/* the byte lanes of a 32-bit big-endian access: lane 0 is bits 31-24 */
#define LANE_ACCESSED(lane)		((mem_mask & (0xff000000 >> ((lane) * 8))) == 0)
#define LANE_DATA(lane)			((data >> (24 - (lane) * 8)) & 0xff)

static READ32_HANDLER( flash_io_r )
{
	data32_t result = 0;
	int lane;

	for (lane = 0; lane < 4; lane++)
	{
		if (LANE_ACCESSED(lane))
		{
			UINT32 byte_offset = offset * 4 + lane;
			data8_t d = 0xff;
			sh3_update_access_cycles(0xb0000000 + byte_offset, 0);
			if (byte_offset == 0)
				d = nand_data_r();
			result |= d << (24 - lane * 8);
		}
	}
	return result;
}

static WRITE32_HANDLER( flash_io_w )
{
	int lane;

	for (lane = 0; lane < 4; lane++)
	{
		if (LANE_ACCESSED(lane))
		{
			UINT32 byte_offset = offset * 4 + lane;
			sh3_update_access_cycles(0xb0000000 + byte_offset, 1);
			switch (byte_offset)
			{
				case 0: nand_data_w(LANE_DATA(lane)); break;
				case 1: nand_command_w(LANE_DATA(lane)); break;
				case 2: nand_address_w(LANE_DATA(lane)); break;
				default: break;
			}
		}
	}
}

static WRITE32_HANDLER( sound_w )
{
	int lane;

	for (lane = 0; lane < 4; lane++)
	{
		if (LANE_ACCESSED(lane))
		{
			UINT32 byte_offset = offset * 4 + lane;
			sh3_update_access_cycles(0xb0400000 + byte_offset, 1);
			ymz770_write(byte_offset, LANE_DATA(lane));
		}
	}
}

static READ32_HANDLER( serial_rtc_eeprom_r )
{
	data32_t result = 0;
	int lane;

	for (lane = 0; lane < 4; lane++)
	{
		if (LANE_ACCESSED(lane))
		{
			UINT32 byte_offset = offset * 4 + lane;
			data8_t d = 0;
			sh3_update_access_cycles(0xb0c00000 + byte_offset, 0);
			if (byte_offset == 1)
				d = 0xfe | rtc_read_bit();
			result |= d << (24 - lane * 8);
		}
	}
	return result;
}

static WRITE32_HANDLER( serial_rtc_eeprom_w )
{
	int lane;

	for (lane = 0; lane < 4; lane++)
	{
		if (LANE_ACCESSED(lane))
		{
			UINT32 byte_offset = offset * 4 + lane;
			sh3_update_access_cycles(0xb0c00000 + byte_offset, 1);
			if (byte_offset == 1)
				rtc_port_w(LANE_DATA(lane));
		}
	}
}

/* the idle loop: the game polls a variable in work RAM

	idlepc - 6	mov.l	@(disp,pc),r2
	idlepc - 4	mov.l	@r2,r1
	idlepc - 2	mov.l	@r2,r0
	idlepc		sub		r1,r0
	idlepc + 2	cmp/hi	r0,r4
	idlepc + 4	bt/s	idlepc
	idlepc + 6	mov.l	@r2,r0		(the read of every pass)

   the PC here is the one after the instruction. MAME's recompiler reports
   the PC it stores at idlepc + 2 for the read in the delay slot too, so it
   spins there on every pass: the delay slot read (idlepc + 8) spins here.

   MAME's spin_until_time(10 us) skips 10 us of the CPU's time. The spin of
   this core's scheduler doesn't (a CPU resumed within the timeslice that
   suspended it never eats its cycles: it falls behind on every spin and
   the next timeslices give it more and more cycles), so the 10 us are
   eaten from the timeslice: 1024 cycles at 102.4 MHz */
static READ32_HANDLER( cv1k_speedup_r )
{
	offs_t pc = activecpu_get_pc();

	if (pc == cv1k_idlepc || pc == cv1k_idlepc + 2 || pc == cv1k_idlepc + 8)
		activecpu_eat_cycles(1024);

	return cv1k_ram[cv1k_idleramoffs / 4 + offset];
}


static MEMORY_READ32_START( cv1k_readmem )
	{ 0x00000000, 0x003fffff, MRA32_ROM },
	{ 0x0c000000, 0x0c7fffff, MRA32_RAM },
	{ 0x10000000, 0x10000007, flash_io_r },
	{ 0x10400000, 0x10400007, MRA32_NOP },
	{ 0x10c00000, 0x10c00007, serial_rtc_eeprom_r },
	{ 0x18000000, 0x18000057, cv1k_blitter_r },
MEMORY_END

static MEMORY_WRITE32_START( cv1k_writemem )
	{ 0x00000000, 0x003fffff, MWA32_NOP },
	{ 0x0c000000, 0x0c7fffff, MWA32_RAM, &cv1k_ram },
	{ 0x10000000, 0x10000007, flash_io_w },
	{ 0x10400000, 0x10400007, sound_w },
	{ 0x10c00000, 0x10c00007, serial_rtc_eeprom_w },
	{ 0x18000000, 0x18000057, cv1k_blitter_w },
MEMORY_END

static MEMORY_READ32_START( cv1k_d_readmem )
	{ 0x00000000, 0x003fffff, MRA32_ROM },
	{ 0x0c000000, 0x0cffffff, MRA32_RAM },
	{ 0x10000000, 0x10000007, flash_io_r },
	{ 0x10400000, 0x10400007, MRA32_NOP },
	{ 0x10c00000, 0x10c00007, serial_rtc_eeprom_r },
	{ 0x18000000, 0x18000057, cv1k_blitter_r },
MEMORY_END

static MEMORY_WRITE32_START( cv1k_d_writemem )
	{ 0x00000000, 0x003fffff, MWA32_NOP },
	{ 0x0c000000, 0x0cffffff, MWA32_RAM, &cv1k_ram },
	{ 0x10000000, 0x10000007, flash_io_w },
	{ 0x10400000, 0x10400007, sound_w },
	{ 0x10c00000, 0x10c00007, serial_rtc_eeprom_w },
	{ 0x18000000, 0x18000057, cv1k_blitter_w },
MEMORY_END


/***************************************************************************
	I/O ports of the SH-3
***************************************************************************/

static data8_t cv1k_port_r(int port)
{
	switch (port)
	{
		case SH3_PORT_C: return readinputport(1);
		case SH3_PORT_D: return readinputport(2);
		case SH3_PORT_E: return (!nand_is_busy() ? 0x20 : 0x00) | 0xdf;
		case SH3_PORT_F: return readinputport(3);
		case SH3_PORT_L: return readinputport(4);
		case SH3_PORT_J: return cv1k_fpga_r();
	}
	return 0;
}

static void cv1k_port_w(int port, data8_t data)
{
	if (port == SH3_PORT_J)
		cv1k_fpga_w(data);
}

static struct sh3_config cv1k_sh3_config =
{
	1,					/* the SH7709S cache and bus timing */
	102400000 / 4,		/* peripheral clock */
	cv1k_port_r,
	cv1k_port_w
};


/***************************************************************************
	input ports
***************************************************************************/

#define CV1K_INPUTS( service_bit_f )																\
	PORT_START	/* port C */																		\
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )	/* service coin */								\
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE2 )	/* test button on the JAMMA edge */				\
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )														\
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )														\
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )														\
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )														\
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNKNOWN )													\
																									\
	PORT_START	/* port D */																		\
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )						\
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )						\
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )						\
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )					\
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )										\
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )										\
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )										\
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )										\
																									\
	PORT_START	/* port F */																		\
	service_bit_f																					\
	PORT_BIT( 0xfd, IP_ACTIVE_LOW, IPT_UNKNOWN )													\
																									\
	PORT_START	/* port L */																		\
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )						\
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )						\
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )						\
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )					\
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )										\
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )										\
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )										\
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

INPUT_PORTS_START( cv1k )
	PORT_START	/* DSW: S2, read through the blitter at 18000050 */
	PORT_DIPNAME( 0x01, 0x00, "S2:1 (Unused)" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "S2:2 (Unused)" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "S2:3 (Unused)" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "S2:4 (Unused)" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	CV1K_INPUTS( PORT_SERVICE_NO_TOGGLE( 0x02, IP_ACTIVE_LOW ) )	/* S3 test push button */
INPUT_PORTS_END

INPUT_PORTS_START( cv1ks )
	PORT_START	/* DSW: S2, read through the blitter at 18000050 */
	PORT_SERVICE( 0x01, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x02, 0x00, "Special Mode" )	/* also hold P1 A+B while booting */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "S2:3 (Unused)" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "S2:4 (Unused)" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	CV1K_INPUTS( PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 ) )	/* S3 test push button */
INPUT_PORTS_END


/***************************************************************************
	machine
***************************************************************************/

static NVRAM_HANDLER( cv1k )
{
	int i;

	if (read_or_write)
	{
		UINT8 buf[0x200];
		UINT32 count = 0;
		UINT8 cbuf[4];

		for (i = 0; i < 0x100; i++)
		{
			buf[i * 2 + 0] = rtc_eeprom[i] >> 8;
			buf[i * 2 + 1] = rtc_eeprom[i] & 0xff;
		}
		mame_fwrite(file, buf, 0x200);

		/* the NAND blocks the game erased or programmed */
		for (i = 0; i < NAND_NUM_BLOCKS; i++)
			if (nand_dirty[i])
				count++;
		cbuf[0] = count >> 24; cbuf[1] = count >> 16; cbuf[2] = count >> 8; cbuf[3] = count;
		mame_fwrite(file, cbuf, 4);
		for (i = 0; i < NAND_NUM_BLOCKS; i++)
			if (nand_dirty[i])
			{
				cbuf[0] = i >> 24; cbuf[1] = i >> 16; cbuf[2] = i >> 8; cbuf[3] = i;
				mame_fwrite(file, cbuf, 4);
				mame_fwrite(file, nand_data + i * NAND_BLOCK_SIZE, NAND_BLOCK_SIZE);
			}
	}
	else
	{
		for (i = 0; i < 0x100; i++)
			rtc_eeprom[i] = 0xffff;
		memset(nand_dirty, 0, sizeof(nand_dirty));
		nand_data = memory_region(REGION_USER1);

		if (file)
		{
			UINT8 buf[0x200];
			UINT8 cbuf[4];
			UINT32 count, n;

			if (mame_fread(file, buf, 0x200) == 0x200)
				for (i = 0; i < 0x100; i++)
					rtc_eeprom[i] = (buf[i * 2] << 8) | buf[i * 2 + 1];
			if (mame_fread(file, cbuf, 4) == 4)
			{
				count = (cbuf[0] << 24) | (cbuf[1] << 16) | (cbuf[2] << 8) | cbuf[3];
				for (n = 0; n < count; n++)
				{
					UINT32 block;
					if (mame_fread(file, cbuf, 4) != 4)
						break;
					block = (cbuf[0] << 24) | (cbuf[1] << 16) | (cbuf[2] << 8) | cbuf[3];
					if (block >= NAND_NUM_BLOCKS)
						break;
					if (mame_fread(file, nand_data + block * NAND_BLOCK_SIZE, NAND_BLOCK_SIZE) != NAND_BLOCK_SIZE)
						break;
					nand_dirty[block] = 1;
				}
			}
		}
	}
}

static MACHINE_INIT( cv1k )
{
	nand_reset();
	cv1k_blitter_reset();
	ymz770_reset_chip();
}

static void cv1k_save_state(void)
{
	state_save_register_UINT8("cv1k_nand", 0, "mode", &nand_mode, 1);
	state_save_register_UINT8("cv1k_nand", 0, "pointer_mode", &nand_pointer_mode, 1);
	state_save_register_UINT8("cv1k_nand", 0, "status", &nand_status, 1);
	state_save_register_UINT8("cv1k_nand", 0, "accumulated_status", &nand_accumulated_status, 1);
	state_save_register_UINT8("cv1k_nand", 0, "mode_3065", &nand_mode_3065, 1);
	state_save_register_UINT32("cv1k_nand", 0, "page_addr", &nand_page_addr, 1);
	state_save_register_UINT32("cv1k_nand", 0, "byte_addr", &nand_byte_addr, 1);
	state_save_register_UINT32("cv1k_nand", 0, "addr_load_ptr", &nand_addr_load_ptr, 1);
	state_save_register_UINT32("cv1k_nand", 0, "program_byte_count", &nand_program_byte_count, 1);
	state_save_register_double("cv1k_nand", 0, "busy_until", &nand_busy_until, 1);
	state_save_register_UINT8("cv1k_nand", 0, "pagereg", nand_pagereg, sizeof(nand_pagereg));

	state_save_register_INT32("cv1k_rtc", 0, "latch", &rtc_latch, 1);
	state_save_register_INT32("cv1k_rtc", 0, "reset_line", &rtc_reset_line, 1);
	state_save_register_INT32("cv1k_rtc", 0, "clock_line", &rtc_clock_line, 1);
	state_save_register_INT32("cv1k_rtc", 0, "state", &rtc_state, 1);
	state_save_register_INT32("cv1k_rtc", 0, "cmd_stream_pos", &rtc_cmd_stream_pos, 1);
	state_save_register_INT32("cv1k_rtc", 0, "current_cmd", &rtc_current_cmd, 1);
	state_save_register_INT32("cv1k_rtc", 0, "address_pos", &rtc_address_pos, 1);
	state_save_register_INT32("cv1k_rtc", 0, "current_address", &rtc_current_address, 1);
	state_save_register_UINT16("cv1k_rtc", 0, "current_data", &rtc_current_data, 1);
	state_save_register_INT32("cv1k_rtc", 0, "data_pos", &rtc_data_pos, 1);
	state_save_register_UINT16("cv1k_rtc", 0, "eeprom", rtc_eeprom, 0x100);
	state_save_register_UINT8("cv1k_rtc", 0, "sec", &rtc_sec, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "min", &rtc_min, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "hour", &rtc_hour, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "day", &rtc_day, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "wday", &rtc_wday, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "month", &rtc_month, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "year", &rtc_year, 1);
	state_save_register_UINT8("cv1k_rtc", 0, "out_old", rtc_out_old, 3);
}

static MACHINE_DRIVER_START( cv1k )
	MDRV_CPU_ADD_TAG("main", SH3, 102400000)
	MDRV_CPU_CONFIG(cv1k_sh3_config)
	MDRV_CPU_MEMORY(cv1k_readmem, cv1k_writemem)
	MDRV_CPU_VBLANK_INT(irq2_line_hold, 1)

	/* 6.4 MHz pixel clock, 407 x 262 total: 60.0184 Hz */
	MDRV_FRAMES_PER_SECOND(6400000.0 / (262 * 407))
	MDRV_VBLANK_DURATION((int)(1000000.0 * 22 * 407 / 6400000.0))

	MDRV_MACHINE_INIT(cv1k)
	MDRV_NVRAM_HANDLER(cv1k)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(320, 240)
	MDRV_VISIBLE_AREA(0, 320-1, 0, 240-1)
	MDRV_PALETTE_LENGTH(32768)

	MDRV_VIDEO_START(cv1k)
	MDRV_VIDEO_UPDATE(cv1k)

	/* sound hardware: only the right output is connected */
	MDRV_SOUND_ADD(CUSTOM, ymz770_custom_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( cv1k_d )
	MDRV_IMPORT_FROM(cv1k)
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(cv1k_d_readmem, cv1k_d_writemem)
MACHINE_DRIVER_END


/***************************************************************************
	ROMs: all are flash ROMs with no labels; the ones that differ are
	prefixed with the set names, as in MAME
***************************************************************************/

ROM_START( mushisam )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "mushisam_u4", 0x000000, 0x200000, CRC(15321b30) SHA1(d2cd714ff2299eeab6f9a7c219dfb559c8f98b45) ) /* (2004/10/12.MASTER VER.) */
	ROM_RELOAD( 0x200000, 0x200000 )

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )	/* NAND */
	ROM_LOAD( "mushisam_u2", 0x000000, 0x8400000, CRC(4f0a842a) SHA1(33f3550ec676a7088b6348cd72c16cc6594afb84) ) /* (2004/10/12.MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(138e2050) SHA1(9e86489a4e65af5efb5495adf6d4b3e01d5b2816) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e3d05c9f) SHA1(130c3d62317da1729c85bd178bd51500edd73ada) )
ROM_END

ROM_START( mushisama )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "mushisama_u4", 0x000000, 0x200000, CRC(0b5b30b2) SHA1(35fd1bb1561c30b311b4325bc8f4628f2fccd20b) ) /* (2004/10/12 MASTER VER.) */
	ROM_RELOAD( 0x200000, 0x200000 )

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )	/* NAND */
	ROM_LOAD( "mushisam_u2", 0x000000, 0x8400000, CRC(4f0a842a) SHA1(33f3550ec676a7088b6348cd72c16cc6594afb84) ) /* (2004/10/12.MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(138e2050) SHA1(9e86489a4e65af5efb5495adf6d4b3e01d5b2816) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e3d05c9f) SHA1(130c3d62317da1729c85bd178bd51500edd73ada) )
ROM_END

ROM_START( mushisamb )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "mushisamb_u4", 0x000000, 0x200000, CRC(9f1c7f51) SHA1(f82ae72ec03687904ca7516887080be92365a5f3) ) /* (2004/10/12 MASTER VER) */
	ROM_RELOAD( 0x200000, 0x200000 )

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )	/* NAND */
	ROM_LOAD( "mushisam_u2", 0x000000, 0x8400000, CRC(4f0a842a) SHA1(33f3550ec676a7088b6348cd72c16cc6594afb84) ) /* (2004/10/12.MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(138e2050) SHA1(9e86489a4e65af5efb5495adf6d4b3e01d5b2816) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e3d05c9f) SHA1(130c3d62317da1729c85bd178bd51500edd73ada) )
ROM_END


ROM_START( espgal2 )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "espgal2_u4", 0x000000, 0x200000, CRC(2cb37c03) SHA1(da438efc497f72aa345d2cdb0143d269e51576d3) ) /* (2005/11/14.MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(222f58c7) SHA1(d47a5085a1debd9cb8c61d88cd39e4f5036d1797) ) /* (2005/11/14 MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(b9a10c22) SHA1(4561f95c6018c9716077224bfe9660e61fb84681) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(c76b1ec4) SHA1(b98a53d41a995d968e0432ed824b0b06d93dcea8) )
ROM_END

ROM_START( espgal2a )
	/* newer CV1000-B PCB revision, updated FPGA firmware, no changes in game code or data */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "espgal2a_u4", 0x000000, 0x200000, CRC(843608b8) SHA1(2f5fcd38e76df531a923cd9956104cef5185aaa9) ) /* (2005/11/14 MASTER VER) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(222f58c7) SHA1(d47a5085a1debd9cb8c61d88cd39e4f5036d1797) ) /* (2005/11/14 MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(b9a10c22) SHA1(4561f95c6018c9716077224bfe9660e61fb84681) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(c76b1ec4) SHA1(b98a53d41a995d968e0432ed824b0b06d93dcea8) )
ROM_END

ROM_START( espgal2b )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "espgal2b_u4", 0x000000, 0x200000, CRC(09c908bb) SHA1(7d6031fd3542b3e1d296ff218feb40502fd78694) ) /* (2005/11/14 MASTER VER) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(222f58c7) SHA1(d47a5085a1debd9cb8c61d88cd39e4f5036d1797) ) /* (2005/11/14 MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(b9a10c22) SHA1(4561f95c6018c9716077224bfe9660e61fb84681) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(c76b1ec4) SHA1(b98a53d41a995d968e0432ed824b0b06d93dcea8) )
ROM_END

ROM_START( mushitam )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("mushitam_u4", 0x000000, 0x200000, CRC(c49eb6b1) SHA1(c40ee5de89e3f1cb49ac19687657dd2b42a88d81) ) /* (2005/09/09.MASTER VER) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD("mushitam_u2", 0x000000, 0x8400000, CRC(8ba498ab) SHA1(459c0b4ab831bbe019bdd5b0ac56955948b9e3a6) ) /* (2005/09/09.MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("u23", 0x000000, 0x400000, CRC(701a912a) SHA1(85c198946fb693d99928ea2595c84ba4d9dc8157) )
	ROM_LOAD16_WORD_SWAP("u24", 0x400000, 0x400000, CRC(6feeb9a1) SHA1(992711c80e660c32f97b343c2ce8184fddd7364e) )
ROM_END

ROM_START( mushitama )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("mushitama_u4", 0x000000, 0x200000, CRC(4a23e6c8) SHA1(d44c287bb88e6d413a8d35d75bc1b4928ad52cdf) ) /* (2005/09/09 MASTER VER) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
/* ROM_LOAD("mushitama_u2", 0x000000, 0x8400000, CRC(3f93ff82) SHA1(6f6c250aa7134016ffb288d056bc937ea311f538) ) // recycled ROM - only unused areas differ */
	ROM_LOAD("mushitam_u2", 0x000000, 0x8400000, CRC(8ba498ab) SHA1(459c0b4ab831bbe019bdd5b0ac56955948b9e3a6) ) /* (2005/09/09.MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("u23", 0x000000, 0x400000, CRC(701a912a) SHA1(85c198946fb693d99928ea2595c84ba4d9dc8157) )
	ROM_LOAD16_WORD_SWAP("u24", 0x400000, 0x400000, CRC(6feeb9a1) SHA1(992711c80e660c32f97b343c2ce8184fddd7364e) )
ROM_END

ROM_START( futari15 )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("futari15_u4", 0x000000, 0x200000, CRC(e8c5f128) SHA1(45fb8066fdbecb83fdc2e14555c460d0c652cd5f) ) /* (2006/12/8.MAST VER. 1.54.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD("futari15_u2", 0x000000, 0x8400000, CRC(b9eae1fc) SHA1(410f8e7cfcbfd271b41fb4f8d049a13a3191a1f9) ) /* (2006/12/8.MAST VER. 1.54.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("u23", 0x000000, 0x400000, CRC(39f1e1f4) SHA1(53d12f59a56df35c705408c76e6e02118da656f1) )
	ROM_LOAD16_WORD_SWAP("u24", 0x400000, 0x400000, CRC(c631a766) SHA1(8bb6934a2f5b8a9841c3dcf85192b1743773dd8b) )
ROM_END

ROM_START( futari15a )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("futari15a_u4", 0x000000, 0x200000, CRC(a609cf89) SHA1(56752fae9f42fa852af8ee2eae79e25ec7f17953) ) /* (2006/12/8 MAST VER 1.54) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
/* ROM_LOAD("futari15a_u2", 0x000000, 0x8400000, CRC(b9d815f9) SHA1(6b6f668b0bbb087ffac65e4f0d8bd9d5b28eeb28) )  // recycled ROM - only unused areas differ */
	ROM_LOAD("futari15_u2", 0x000000, 0x8400000, CRC(b9eae1fc) SHA1(410f8e7cfcbfd271b41fb4f8d049a13a3191a1f9) ) /* (2006/12/8.MAST VER. 1.54.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP("u23", 0x000000, 0x400000, CRC(39f1e1f4) SHA1(53d12f59a56df35c705408c76e6e02118da656f1) )
	ROM_LOAD16_WORD_SWAP("u24", 0x400000, 0x400000, CRC(c631a766) SHA1(8bb6934a2f5b8a9841c3dcf85192b1743773dd8b) )
ROM_END

ROM_START( futari10 )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "futari10_u4", 0x000000, 0x200000, CRC(b127dca7) SHA1(e1f518bc72fc1cdf69aefa89eafa4edaf4e84778) ) /* (2006/10/23 MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "futari10_u2", 0x000000, 0x8400000, CRC(78ffcd0c) SHA1(0e2937edec15ce3f5741b72ebd3bbaaefffb556e) ) /* (2006/10/23 MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(39f1e1f4) SHA1(53d12f59a56df35c705408c76e6e02118da656f1) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(c631a766) SHA1(8bb6934a2f5b8a9841c3dcf85192b1743773dd8b) )
ROM_END

ROM_START( futaribl )
	/* Title screen shows (c) 2007 despite the 2009 "master" date - Also prints "Another Ver" to the title screen - reworked & re-released for the Chinese market */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "futaribli_u4", 0x000000, 0x200000, CRC(1971dd16) SHA1(e75993f2978cbaaf925b4b8bb33d094a5a7cebf0) ) /* (2009/11/27 INTERNATIONAL BL) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "futariblk_u2", 0x000000, 0x8400000, CRC(08c6fd62) SHA1(e1fc386b2b0e41906c724287cbf82304297e0150) )

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(39f1e1f4) SHA1(53d12f59a56df35c705408c76e6e02118da656f1) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(c631a766) SHA1(8bb6934a2f5b8a9841c3dcf85192b1743773dd8b) )
ROM_END

ROM_START( futariblj )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "futariblk_u4", 0x000000, 0x200000, CRC(b9467b6d) SHA1(64782807fe69acb4ae028e36c5c689d8bd9b7857) ) /* (2007/12/11 BLACK LABEL VER) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "futariblk_u2", 0x000000, 0x8400000, CRC(08c6fd62) SHA1(e1fc386b2b0e41906c724287cbf82304297e0150) ) /* (2007/12/11 BLACK LABEL VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(39f1e1f4) SHA1(53d12f59a56df35c705408c76e6e02118da656f1) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(c631a766) SHA1(8bb6934a2f5b8a9841c3dcf85192b1743773dd8b) )
ROM_END

ROM_START( ibara )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "ibara_u4", 0x000000, 0x200000, CRC(d5fb6657) SHA1(4b2c9057cc765054232f520ee1c93495bc6b301e) ) /* '06. 3. 7 ver. shown at service menu in special mode, still same as ibarao (2005/03/22 MASTER VER..) text at normal boot */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(55840976) SHA1(4982bdce84f9603adfed7a618f18bc80359ab81e) ) /* (2005/03/22 MASTER VER..) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(ee5e585d) SHA1(7eeba4ee693060e927f8c46b16e39227c6a62392) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(f0aa3cb6) SHA1(f9d137cd879e718811b2d21a0af2a9c6b7dca2f9) )
ROM_END

ROM_START( ibarao )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "ibarao_u4", 0x000000, 0x200000, CRC(8e6c155d) SHA1(38ac2107dc7824836e2b4e04c7180d5ae43c9b79) ) /* (2005/03/22 MASTER VER..) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(55840976) SHA1(4982bdce84f9603adfed7a618f18bc80359ab81e) ) /* (2005/03/22 MASTER VER..) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(ee5e585d) SHA1(7eeba4ee693060e927f8c46b16e39227c6a62392) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(f0aa3cb6) SHA1(f9d137cd879e718811b2d21a0af2a9c6b7dca2f9) )
ROM_END

ROM_START( ibarablk )
	/* Title screen shows (c) 2005 despite the 2006 "master" date */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "ibarablk_u4", 0x000000, 0x200000, CRC(ee1f1f77) SHA1(ac276f3955aa4dde2544af4912819a7ae6bcf8dd) ) /* (2006/02/06. MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "ibarablk_u2", 0x000000, 0x8400000, CRC(5e46be44) SHA1(bed5f1bf452f2cac58747ecabec3c4392566a3a7) ) /* (2006/02/06. MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(a436bb22) SHA1(0556e771cc02638bf8814315ba671c2d442594f1) ) /* (2006/02/06 MASTER VER.) */
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(d11ab6b6) SHA1(2132191cbe847e2560423e4545c969f21f8ff825) ) /* (2006/02/06 MASTER VER.) */
ROM_END

ROM_START( ibarablka )
	/* Title screen shows (c) 2005 despite the 2006 "master" date */
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "ibarablka_u4", 0x000000, 0x200000, CRC(a9d43839) SHA1(507696e616608c05893c7ac2814b3365e9cb0720) ) /* (2006/02/06 MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "ibarablka_u2", 0x000000, 0x8400000, CRC(33400d96) SHA1(09c22b5431ac3726bf88c56efd970f56793f825a) ) /* (2006/02/06 MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(a436bb22) SHA1(0556e771cc02638bf8814315ba671c2d442594f1) ) /* (2006/02/06 MASTER VER.) */
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(d11ab6b6) SHA1(2132191cbe847e2560423e4545c969f21f8ff825) ) /* (2006/02/06 MASTER VER.) */
ROM_END

ROM_START( deathsml )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u4", 0x000000, 0x200000, CRC(1a7b98bf) SHA1(07798a4a846e5802756396b34df47d106895c1f1) ) /* (2007/10/09 MASTER VER) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(59ef5d78) SHA1(426e506b6d88948aa55aec71c0db6e91da3d490d) ) /* (2007/10/09 MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(aab718c8) SHA1(0e636c46d06151abd6f73232bc479dafcafe5327) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(83881d84) SHA1(6e2294b247dfcbf0ced155dc45c706f29052775d) )
ROM_END

ROM_START( mmpork )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u4", 0x000000, 0x200000, CRC(d06cfa42) SHA1(5707feb4b3e5265daf5926f38c38612b24106f1f) ) /* (2007/ 4/17 MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(1ee961b8) SHA1(81a2eba704ac1cf7fc44fa7c6a3f50e3570c104f) ) /* (2007/ 4/17 MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(4a4b36df) SHA1(5db5ce6fa47e5ca3263d4bd19315890c6d29df66) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(ce83d07b) SHA1(a5947467c8f5b7c4b0ad8e32df2ee29b787e355f) )
ROM_END

ROM_START( pinkswts )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "pinkswts_u4", 0x0000, 0x200000, CRC(5d812c9e) SHA1(db821ec3892fd150513749d64a8b60bf147f3275) ) /* (2006/04/06 MASTER VER....) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "pinkswts_u2", 0x000000, 0x8400000, CRC(a2fa5363) SHA1(5be327534840871592df523ac82ee1927bd79d67) ) /* (2006/04/06 MASTER VER....) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(4b82d250) SHA1(ee98dbc3f791efb6d58f3945bcb2044667ae7978) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e93f0627) SHA1(6f5ec0ade87f7fc42a58a8f125557a4d1f3f187d) )
ROM_END

ROM_START( pinkswtsa )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "pnkswtsa_u4", 0x0000, 0x200000, CRC(ee3339b2) SHA1(995988d370731a7074b49ce8752525dadf06a954) ) /* (2006/04/06 MASTER VER...) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "pnkswtsa_u2", 0x000000, 0x8400000, CRC(829a862e) SHA1(8c0ee2a0eb33b68869252fd68aed74820a904287) ) /* (2006/04/06 MASTER VER...) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(4b82d250) SHA1(ee98dbc3f791efb6d58f3945bcb2044667ae7978) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e93f0627) SHA1(6f5ec0ade87f7fc42a58a8f125557a4d1f3f187d) )
ROM_END

ROM_START( pinkswtsb )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "pnkswtsb_u4", 0x0000, 0x200000, CRC(68bcc009) SHA1(2fef544b93c61161a37365f868b431d8262e4b21) ) /* (2006/04/06 MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
/* ROM_LOAD( "pnkswtsb_u2", 0x000000, 0x8400000, BAD_DUMP CRC(a5666ed9) SHA1(682e06c84990225bc6bb0c9f38b5f46c4e36b430) ) // (2006/04/06 MASTER VER.) */
	ROM_LOAD( "pnkswtsx_u2", 0x000000, 0x8400000, CRC(91e4deb2) SHA1(893cb10d6f805df7cb4a1bb709a3ea6de147b7e9) ) /* (2006/xx/xx MASTER VER.) and (2006/04/06 MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(4b82d250) SHA1(ee98dbc3f791efb6d58f3945bcb2044667ae7978) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e93f0627) SHA1(6f5ec0ade87f7fc42a58a8f125557a4d1f3f187d) )
ROM_END

ROM_START( pinkswtsx )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "pnkswtsx_u4", 0x0000, 0x200000, CRC(8fe05bf0) SHA1(5cd45ac0e74690787c83d3fb383a65ed7cd47104) ) /* (2006/xx/xx MASTER VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "pnkswtsx_u2", 0x000000, 0x8400000, CRC(91e4deb2) SHA1(893cb10d6f805df7cb4a1bb709a3ea6de147b7e9) ) /* (2006/xx/xx MASTER VER.) and (2006/04/06 MASTER VER.) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(4b82d250) SHA1(ee98dbc3f791efb6d58f3945bcb2044667ae7978) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e93f0627) SHA1(6f5ec0ade87f7fc42a58a8f125557a4d1f3f187d) )
ROM_END

ROM_START( pinkswtssc )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "suicideclub.u4", 0x0000, 0x200000, CRC(5e03662f) SHA1(b974204b8dcd55fc1b7775f7c1806150919caff3) ) /* (2017/10/31 SUICIDECLUB VER.) */
	ROM_RELOAD(0x200000,0x200000)

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "suicideclub.u2", 0x000000, 0x8400000, CRC(32324608) SHA1(cec1416c943520cb3f91eb295e2ba864a0db7d45) )

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(4b82d250) SHA1(ee98dbc3f791efb6d58f3945bcb2044667ae7978) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(e93f0627) SHA1(6f5ec0ade87f7fc42a58a8f125557a4d1f3f187d) )
ROM_END

ROM_START( ddpdfk )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "ddpdfk_u4", 0x0000, 0x400000, CRC(9976d699) SHA1(9dfe9d1daf6f638cafce8cdc5230209e2bcb7522) ) /* (2008/06/23  MASTER VER 1.5) */

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "ddpdfk_u2", 0x000000, 0x8400000, CRC(84a51a4f) SHA1(291a6279c0746d2eb8630e7d6d886043f0cfdd94) ) /* (2008/06/23  MASTER VER 1.5) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(27032cde) SHA1(5b58d0140d72b91db4e763ca4af93060d36ac74d) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(a6178c2c) SHA1(7742ad1de2e4b0d0206ad11d467ea35da36352df) )
ROM_END

ROM_START( ddpdfk10 )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "ddpdfk10_u4", 0x0000, 0x400000, CRC(a3d650b2) SHA1(46a7551760e18c2cecd372c3f4be16f6600efc2c) ) /* (2008/05/16  MASTER VER) */

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "ddpdfk10_u2", 0x000000, 0x8400000, CRC(d349cb2a) SHA1(c364c36b69b93f8f62390f185d044f51056669ff) ) /* (2008/05/16  MASTER VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(27032cde) SHA1(5b58d0140d72b91db4e763ca4af93060d36ac74d) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(a6178c2c) SHA1(7742ad1de2e4b0d0206ad11d467ea35da36352df) )
ROM_END

ROM_START( dsmbl )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u4", 0x000000, 0x400000, CRC(77fc5ad1) SHA1(afe044fc16e9494143c876879b033caccd08cf22) ) /* (2008/10/06 MEGABLACK LABEL VER) */

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(d6b85b7a) SHA1(4674f6ad07f6a03904ca3d05060816b8fe061add) ) /* (2008/10/06 MEGABLACK LABEL VER) */

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(a9536a6a) SHA1(4b9dcaf6803b1fcfdf73ae9daabc4508fec71631) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(3b673326) SHA1(1ae847eb4e752fef1d72081d82344f0ad0537c31) )
ROM_END

ROM_START( dfkbl )
	ROM_REGION( 0x400000, REGION_CPU1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u4", 0x000000, 0x400000, CRC(8092ca9d) SHA1(75e16cd7c8d0f9c715115ce12da5c245fbcd2416) ) /* (2010/1/18 BLACK LABEL) */

	ROM_REGION( 0x8400000, REGION_USER1, ROMREGION_ERASEFF )
	ROM_LOAD( "u2", 0x000000, 0x8400000, CRC(29f9d73a) SHA1(ed978ab5e3ad8c05e7778a91bfb5aaa17b0f72d9) )

	ROM_REGION( 0x800000, REGION_SOUND1, ROMREGION_ERASEFF )
	ROM_LOAD16_WORD_SWAP( "u23", 0x000000, 0x400000, CRC(36d4093b) SHA1(4aed7e2f7c0d2c9bceeb110a9907d8d99d55f4c3) )
	ROM_LOAD16_WORD_SWAP( "u24", 0x400000, 0x400000, CRC(31f9eb0a) SHA1(322158779e969bb321241065dd49c1167b91ff6c) )
ROM_END


/***************************************************************************
	driver init: the idle loop speedups of current MAME
***************************************************************************/

static void install_speedups(UINT32 idleramoff, UINT32 idlepc, int is_typed)
{
	UINT32 ram_end = is_typed ? 0x00ffffff : 0x007fffff;

	cv1k_idleramoffs = idleramoff;
	cv1k_idlepc = idlepc;
	cv1k_ram_size = ram_end + 1;

	install_mem_read32_handler(0, 0x0c000000 + idleramoff, 0x0c000000 + idleramoff + 7, cv1k_speedup_r);

	sh3_set_fastram(0x00000000, 0x003fffff, 1, (data32_t *)memory_region(REGION_CPU1));
	sh3_set_fastram(0x0c000000, 0x0c000000 + idleramoff - 1, 0, cv1k_ram);
	sh3_set_fastram(0x0c000000 + idleramoff + 8, 0x0c000000 + ram_end, 0, cv1k_ram + (idleramoff + 8) / 4);

	nand_data = memory_region(REGION_USER1);
	rtc_set_from_system();
	timer_pulse(TIME_IN_SEC(1), 0, rtc_timer_callback);
	rtc_out_old[0] = 0;
	rtc_out_old[1] = 0;
	rtc_out_old[2] = 1;
	rtc_reset_line = CLEAR_LINE;
	rtc_state = RTC_CMD_WAIT;

	cv1k_save_state();

	/* the recompiler (core option; only on the hosts that support it) */
	sh3_set_jit(options.sh3_dynarec);
}

static DRIVER_INIT( mushisam ) { install_speedups(0x024d8, 0xc04a2aa, 0); }
static DRIVER_INIT( ibara )    { install_speedups(0x022f0, 0xc04a0aa, 0); }
static DRIVER_INIT( espgal2 )  { install_speedups(0x02310, 0xc05177a, 0); }
static DRIVER_INIT( mushitam ) { install_speedups(0x022f0, 0xc04a0da, 0); }
static DRIVER_INIT( pinkswts ) { install_speedups(0x02310, 0xc05176a, 0); }
static DRIVER_INIT( deathsml ) { install_speedups(0x02310, 0xc0519a2, 0); }
static DRIVER_INIT( ddpdfk )   { install_speedups(0x02310, 0xc1d1346, 1); }


/***************************************************************************
	game drivers
***************************************************************************/

/* the Black Label versions are not clones: they were re-releases with
   different game codes, not bug fixes */

/* CA011  Mushihime-Sama */
GAME( 2004, mushisam,  0,        cv1k, cv1k, mushisam, ROT270, "Cave (AMI license)", "Mushihime-Sama (Japan, 2004/10/12.MASTER VER.)" )
GAME( 2004, mushisama, mushisam, cv1k, cv1k, ibara,    ROT270, "Cave (AMI license)", "Mushihime-Sama (Japan, 2004/10/12 MASTER VER.)" )
GAME( 2004, mushisamb, mushisam, cv1k, cv1k, mushisam, ROT270, "Cave (AMI license)", "Mushihime-Sama (Japan, 2004/10/12 MASTER VER)" )

GAME( 2005, ibara,     0,        cv1k,   cv1ks, ibara,    ROT270, "Cave (AMI license)", "Ibara (Japan, 2005/03/22 MASTER VER.., '06. 3. 7 ver.)" )
GAME( 2005, ibarao,    ibara,    cv1k,   cv1ks, ibara,    ROT270, "Cave (AMI license)", "Ibara (Japan, 2005/03/22 MASTER VER..)" )
GAME( 2006, ibarablk,  0,        cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Ibara Kuro Black Label (Japan, 2006/02/06. MASTER VER.)" )
GAME( 2006, ibarablka, ibarablk, cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Ibara Kuro Black Label (Japan, 2006/02/06 MASTER VER.)" )
GAME( 2005, espgal2,   0,        cv1k,   cv1k,  espgal2,  ROT270, "Cave (AMI license)", "Espgaluda II (Japan, 2005/11/14.MASTER VER.)" )
GAME( 2005, espgal2a,  espgal2,  cv1k,   cv1k,  espgal2,  ROT270, "Cave (AMI license)", "Espgaluda II (Japan, 2005/11/14 MASTER VER, newer CV1000-B PCB)" )
GAME( 2005, espgal2b,  espgal2,  cv1k,   cv1k,  espgal2,  ROT270, "Cave (AMI license)", "Espgaluda II (Japan, 2005/11/14 MASTER VER, original CV1000-B PCB)" )
GAME( 2005, mushitam,  0,        cv1k,   cv1k,  mushitam, ROT0, "Cave (AMI license)", "Puzzle! Mushihime-Tama (Japan, 2005/09/09.MASTER VER)" )
GAME( 2005, mushitama, mushitam, cv1k,   cv1k,  mushitam, ROT0, "Cave (AMI license)", "Puzzle! Mushihime-Tama (Japan, 2005/09/09 MASTER VER)" )
GAME( 2006, pinkswts,  0,        cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Pink Sweets: Ibara Sorekara (Japan, 2006/04/06 MASTER VER....)" )
GAME( 2006, pinkswtsa, pinkswts, cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Pink Sweets: Ibara Sorekara (Japan, 2006/04/06 MASTER VER...)" )
GAME( 2006, pinkswtsb, pinkswts, cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Pink Sweets: Ibara Sorekara (Japan, 2006/04/06 MASTER VER.)" )
GAME( 2006, pinkswtsx, pinkswts, cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Pink Sweets: Ibara Sorekara (Japan, 2006/xx/xx MASTER VER.)" ) /* defaults to freeplay, possibly bootlegged from show/dev version? */
GAME( 2017, pinkswtssc, pinkswts, cv1k,   cv1ks, pinkswts, ROT270, "bootleg (Four Horsemen)", "Pink Sweets: Suicide Club (2017/10/31 SUICIDECLUB VER., bootleg)" ) /* dumped from bootleg with pre-patched hack */
GAME( 2006, futari15,  0,        cv1k,   cv1k,  pinkswts, ROT270, "Cave (AMI license)", "Mushihime-Sama Futari Ver 1.5 (Japan, 2006/12/8.MASTER VER. 1.54.)" )
GAME( 2006, futari15a, futari15, cv1k,   cv1k,  pinkswts, ROT270, "Cave (AMI license)", "Mushihime-Sama Futari Ver 1.5 (Japan, 2006/12/8 MASTER VER 1.54)" )
GAME( 2006, futari10,  futari15, cv1k,   cv1k,  pinkswts, ROT270, "Cave (AMI license)", "Mushihime-Sama Futari Ver 1.0 (Japan, 2006/10/23 MASTER VER.)" )
GAME( 2007, futaribl,  0,        cv1k,   cv1k,  pinkswts, ROT270, "Cave (AMI license)", "Mushihime-Sama Futari Black Label - Another Ver (World, 2009/11/27 INTERNATIONAL BL)" ) /* re-released for Chinese market */
GAME( 2007, futariblj, futaribl, cv1k,   cv1k,  pinkswts, ROT270, "Cave (AMI license)", "Mushihime-Sama Futari Black Label (Japan, 2007/12/11 BLACK LABEL VER)" )
GAME( 2007, mmpork,    0,        cv1k,   cv1ks, pinkswts, ROT270, "Cave (AMI license)", "Muchi Muchi Pork! (Japan, 2007/ 4/17 MASTER VER.)" )
GAME( 2007, deathsml,  0,        cv1k,   cv1k,  deathsml, ROT0, "Cave (AMI license)", "Deathsmiles (Japan, 2007/10/09 MASTER VER)" )
GAME( 2008, dsmbl,     0,        cv1k_d, cv1k,  ddpdfk,   ROT0, "Cave (AMI license)", "Deathsmiles MegaBlack Label (Japan, 2008/10/06 MEGABLACK LABEL VER)" )
GAME( 2008, ddpdfk,    0,        cv1k_d, cv1k,  ddpdfk,   ROT270, "Cave (AMI license)", "DoDonPachi Dai-Fukkatsu Ver 1.5 (Japan, 2008/06/23 MASTER VER 1.5)" )
GAME( 2008, ddpdfk10,  ddpdfk,   cv1k_d, cv1k,  ddpdfk,   ROT270, "Cave (AMI license)", "DoDonPachi Dai-Fukkatsu Ver 1.0 (Japan, 2008/05/16 MASTER VER)" )
GAME( 2010, dfkbl,     0,        cv1k_d, cv1k,  ddpdfk,   ROT270, "Cave", "DoDonPachi Dai-Fukkatsu Black Label (Japan, 2010/1/18 BLACK LABEL)" )
