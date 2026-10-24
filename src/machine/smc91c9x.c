/*************************************************************************

	SMC91C94 ethernet controller
	(C89 port of the current MAME device by Aaron Giles and Ted Green)

	There is no network: transmitted frames complete as if sent on an
	empty cable, and are looped back to the receiver when the game asks
	for it (TCR LOOP, EPH_LOOP or FDUPLX), like current MAME.

**************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "machine/smc91c9x.h"
#include <compat/zlib.h>


/* registers: bank * 8 + register */
#define B0_TCR			(0 * 8 + 0)
#define B0_EPH_STATUS	(0 * 8 + 1)
#define B0_RCR			(0 * 8 + 2)
#define B0_COUNTER		(0 * 8 + 3)
#define B0_MIR			(0 * 8 + 4)
#define B0_MCR			(0 * 8 + 5)
#define B0_BANK			(0 * 8 + 7)
#define B1_CONFIG		(1 * 8 + 0)
#define B1_BASE			(1 * 8 + 1)
#define B1_IA0_1		(1 * 8 + 2)
#define B1_IA2_3		(1 * 8 + 3)
#define B1_IA4_5		(1 * 8 + 4)
#define B1_GENERAL_PURP	(1 * 8 + 5)
#define B1_CONTROL		(1 * 8 + 6)
#define B2_MMU_COMMAND	(2 * 8 + 0)
#define B2_PNR_ARR		(2 * 8 + 1)
#define B2_FIFO_PORTS	(2 * 8 + 2)
#define B2_POINTER		(2 * 8 + 3)
#define B2_DATA_0		(2 * 8 + 4)
#define B2_DATA_1		(2 * 8 + 5)
#define B2_INTERRUPT	(2 * 8 + 6)
#define B3_MT0_1		(3 * 8 + 0)
#define B3_MT2_3		(3 * 8 + 1)
#define B3_MT4_5		(3 * 8 + 2)
#define B3_MT6_7		(3 * 8 + 3)
#define B3_MGMT			(3 * 8 + 4)
#define B3_REVISION		(3 * 8 + 5)
#define B3_ERCV			(3 * 8 + 6)

/* MMU commands */
#define ECMD_NOP						0
#define ECMD_ALLOCATE					2
#define ECMD_RESET_MMU					4
#define ECMD_REMOVE_TOPFRAME_RX			6
#define ECMD_REMOVE_TOPFRAME_TX			7
#define ECMD_REMOVE_RELEASE_TOPFRAME_RX	8
#define ECMD_RELEASE_PACKET				10
#define ECMD_ENQUEUE_PACKET				12
#define ECMD_RESET_FIFOS				14

/* interrupt bits */
#define EINT_RCV		0x01
#define EINT_TX			0x02
#define EINT_TX_EMPTY	0x04
#define EINT_ALLOC		0x08
#define EINT_RX_OVRN	0x10
#define EINT_EPH		0x20
#define EINT_ERCV		0x40

/* address filter results */
#define ADDR_NOMATCH	0
#define ADDR_UNICAST	1
#define ADDR_BROADCAST	2
#define ADDR_MULTICAST	3

/* buffer control byte */
#define EBUF_RX_ALWAYS	0x40
#define EBUF_ODD		0x20
#define EBUF_CRC		0x10

/* receive status */
#define BRODCAST		0x4000
#define ODDFRM			0x1000
#define MULTCAST		0x0001

/* EPH status */
#define LINK_OK			0x4000
#define LOST_CARR		0x0400
#define LATCOL			0x0200
#define LTX_BRD			0x0040
#define SQET			0x0020
#define E16COL			0x0010
#define TX_SUC			0x0001

/* control */
#define AUTO_RELEASE	0x0800
#define LE_ENABLE		0x0080

/* transmit control */
#define FDSE			0x8000
#define EPH_LOOP		0x2000
#define FDUPLX			0x0800
#define NOCRC			0x0100
#define PAD_EN			0x0080
#define LOOP			0x0002
#define TXENA			0x0001

/* receive control */
#define SOFT_RST		0x8000
#define STRIP_CRC		0x0200
#define RXEN			0x0100
#define ALMUL			0x0004
#define PRMS			0x0002

/* pointer */
#define PTR_RCV			0x8000
#define PTR_AUTO_INCR	0x4000
#define PTR_MASK		0x07ff

#define ETHER_BUFFER_SIZE	(256 * 6)
#define NUM_EBUF			18			/* SMC91C94 */
#define FIFO_SIZE			32

/* 10 Mbit/s, as the MAME network interface: bytes per second */
#define BYTES_PER_SECOND	(10 << 17)

static const UINT8 eth_broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static struct
{
	void		(*irq_callback)(int state);
	mame_timer	*tx_poll;
	mame_timer	*tx_done;
	UINT16		reg[64];
	UINT16		regmask[64];
	UINT8		irq_state;
	UINT8		buffer[ETHER_BUFFER_SIZE * NUM_EBUF];
	UINT32		alloc_rx, alloc_tx;
	INT32		tx_active, rx_active, tx_poll_pending;
	INT32		loopback_result;
	UINT8		rx_hash;
	UINT8		queued_tx[FIFO_SIZE];
	INT32		queued_tx_h, queued_tx_t;
	UINT8		completed_tx[FIFO_SIZE];
	INT32		completed_tx_h, completed_tx_t;
	UINT8		completed_rx[FIFO_SIZE];
	INT32		completed_rx_h, completed_rx_t;
} smc;


/* circular FIFOs, power of 2 size, no over/under run checking */
#define FIFO_RESET(n)	(smc.n##_h = smc.n##_t = 0)
#define FIFO_EMPTY(n)	(smc.n##_h == smc.n##_t)
#define FIFO_CURR(n)	(smc.n[smc.n##_t])
#define FIFO_PUSH(n, v)	do { smc.n[smc.n##_h++] = (v); smc.n##_h &= FIFO_SIZE - 1; } while (0)

static int fifo_pop(UINT8 *fifo, INT32 *tail)
{
	int val = fifo[*tail];
	*tail = (*tail + 1) & (FIFO_SIZE - 1);
	return val;
}

#define FIFO_POP(n)		fifo_pop(smc.n, &smc.n##_t)


static void send_complete(int result);


static void update_ethernet_irq(void)
{
	UINT8 new_state;

	if (FIFO_EMPTY(completed_tx))
		smc.reg[B2_INTERRUPT] &= ~EINT_TX;
	else
		smc.reg[B2_INTERRUPT] |= EINT_TX;

	if (FIFO_EMPTY(completed_rx))
		smc.reg[B2_INTERRUPT] &= ~EINT_RCV;
	else
		smc.reg[B2_INTERRUPT] |= EINT_RCV;

	new_state = (smc.reg[B2_INTERRUPT] >> 8) & smc.reg[B2_INTERRUPT] & 0xff;
	if (smc.irq_state ^ new_state)
	{
		smc.irq_state = new_state;
		if (smc.irq_callback)
			(*smc.irq_callback)(smc.irq_state ? ASSERT_LINE : CLEAR_LINE);
	}
}


static void stop_tx_poll(void)
{
	timer_adjust(smc.tx_poll, TIME_NEVER, 0, 0);
	smc.tx_poll_pending = 0;
}


static void start_tx_poll(double delay)
{
	timer_adjust(smc.tx_poll, delay, 0, 0);
	smc.tx_poll_pending = 1;
}


static void reset_tx_fifos(void)
{
	stop_tx_poll();
	FIFO_RESET(queued_tx);
	FIFO_RESET(completed_tx);
}


static void mmu_reset(void)
{
	smc.alloc_rx = 0;
	smc.alloc_tx = 0;
	smc.reg[B0_MIR] &= 0xff;
	smc.reg[B0_MIR] |= smc.reg[B0_MIR] << 8;
	reset_tx_fifos();
	FIFO_RESET(completed_rx);
	update_ethernet_irq();
}


static int alloc_req(int tx, int *packet_num)
{
	UINT32 curr_alloc = smc.alloc_rx | smc.alloc_tx;
	int index;

	for (index = 0; index < NUM_EBUF; index++)
		if (!(curr_alloc & (1 << index)))
		{
			*packet_num = index;
			if (tx)
				smc.alloc_tx |= 1 << index;
			else
				smc.alloc_rx |= 1 << index;
			return 1;
		}
	return 0;
}


static void alloc_release(int packet_num)
{
	UINT32 clear_mask = ~(1 << packet_num);
	smc.alloc_tx &= clear_mask;
	smc.alloc_rx &= clear_mask;
}


static int address_filter(const UINT8 *buf)
{
	if (smc.reg[B0_RCR] & PRMS)
		return ADDR_UNICAST;

	if (buf[0] & 1)
	{
		UINT32 crc;
		int i;

		if (!memcmp(eth_broadcast, buf, 6))
			return ADDR_BROADCAST;

		/* multicast: the upper 6 bits of the address FCS index the 64-bit table */
		if (smc.reg[B0_RCR] & ALMUL)
			return ADDR_MULTICAST;
		crc = crc32(0, buf, 6);
		smc.rx_hash = 0;
		for (i = 0; i < 6; i++)
			smc.rx_hash |= (((~crc) >> i) & 1) << (5 - i);
		if ((smc.reg[B3_MT0_1 + (smc.rx_hash >> 4)] >> (smc.rx_hash & 15)) & 1)
			return ADDR_MULTICAST;
	}
	else
	{
		UINT8 ia[6];
		ia[0] = smc.reg[B1_IA0_1] & 0xff; ia[1] = smc.reg[B1_IA0_1] >> 8;
		ia[2] = smc.reg[B1_IA2_3] & 0xff; ia[3] = smc.reg[B1_IA2_3] >> 8;
		ia[4] = smc.reg[B1_IA4_5] & 0xff; ia[5] = smc.reg[B1_IA4_5] >> 8;
		if (!memcmp(ia, buf, 6))
			return ADDR_UNICAST;
	}
	return ADDR_NOMATCH;
}


/* receive a frame into a buffer: returns the packet number + 1, or < 0 */
static int receive(const UINT8 *buf, int length)
{
	UINT8 *packet;
	UINT16 rx_status, buf_length;
	int filter, packet_num;

	if (!(smc.reg[B0_RCR] & RXEN))
		return -1;

	filter = address_filter(buf);
	if (filter == ADDR_NOMATCH)
		return -1;

	if (!alloc_req(0, &packet_num))
		return -2;

	smc.rx_active = 1;
	packet = &smc.buffer[packet_num * ETHER_BUFFER_SIZE];

	if (smc.reg[B0_RCR] & STRIP_CRC)
		length -= 4;
	memmove(&packet[4], buf, length);

	rx_status = 0;
	if (filter == ADDR_BROADCAST)
		rx_status |= BRODCAST;
	if (filter == ADDR_MULTICAST)
		rx_status |= (smc.rx_hash << 1) | MULTCAST;

	if (length & 1)
	{
		rx_status |= ODDFRM;
		packet[length + 4] = EBUF_RX_ALWAYS | EBUF_ODD;
		buf_length = length + 5;
	}
	else
	{
		packet[length + 4] = 0x00;
		packet[length + 5] = EBUF_RX_ALWAYS;
		buf_length = length + 6;
	}
	packet[0] = rx_status & 0xff;
	packet[1] = rx_status >> 8;
	packet[2] = buf_length & 0xff;
	packet[3] = buf_length >> 8;
	return packet_num + 1;
}


static void recv_complete(int result)
{
	if (result > 0)
		FIFO_PUSH(completed_rx, result - 1);
	else if (result == -2)
		smc.reg[B2_INTERRUPT] |= EINT_ALLOC;
	update_ethernet_irq();
	smc.rx_active = 0;
}


static void tx_poll(int param)
{
	smc.tx_poll_pending = 0;

	if (smc.rx_active && !(smc.reg[B0_TCR] & FDSE))
		start_tx_poll(TIME_IN_USEC(40));

	else if ((smc.reg[B0_TCR] & TXENA) && !FIFO_EMPTY(queued_tx))
	{
		int packet_num = FIFO_CURR(queued_tx);
		UINT8 *tx_buffer = &smc.buffer[packet_num * ETHER_BUFFER_SIZE];
		int length = (tx_buffer[2] | (tx_buffer[3] << 8)) & 0x7ff;
		UINT8 control = tx_buffer[length - 1];

		if (control & EBUF_ODD)
			length -= 1;
		else
			length -= 2;

		/* pad to the minimum frame */
		while (length < 64 && (smc.reg[B0_TCR] & PAD_EN))
			tx_buffer[length++] = 0x00;

		/* append the FCS */
		if ((control & EBUF_CRC) || !(smc.reg[B0_TCR] & NOCRC))
		{
			UINT32 crc = crc32(0, tx_buffer + 4, length - 4);
			tx_buffer[length + 0] = crc;
			tx_buffer[length + 1] = crc >> 8;
			tx_buffer[length + 2] = crc >> 16;
			tx_buffer[length + 3] = crc >> 24;
			length += 4;
		}

		/* remove the header */
		length -= 4;

		smc.reg[B0_EPH_STATUS] &= LINK_OK;
		smc.tx_active = 1;
		stop_tx_poll();

		if (smc.reg[B0_TCR] & (EPH_LOOP | LOOP | FDUPLX))
			smc.loopback_result = receive(&tx_buffer[4], length);
		else
			smc.loopback_result = 0;

		/* local loopback isn't sent on the cable; else the frame takes its time on an empty cable */
		if (smc.reg[B0_TCR] & (EPH_LOOP | LOOP))
			send_complete(length);
		else
			timer_adjust(smc.tx_done, (double)length / BYTES_PER_SECOND, 0, 0);
	}
}


static void send_complete(int result)
{
	int packet_num = FIFO_POP(queued_tx);
	UINT8 *tx_buffer = &smc.buffer[packet_num * ETHER_BUFFER_SIZE];

	smc.reg[B0_EPH_STATUS] |= TX_SUC;

	if (!memcmp(eth_broadcast, &tx_buffer[4], 6))
		smc.reg[B0_EPH_STATUS] |= LTX_BRD;

	if (FIFO_EMPTY(queued_tx))
		smc.reg[B2_INTERRUPT] |= EINT_TX_EMPTY;

	if (smc.reg[B1_CONTROL] & AUTO_RELEASE)
		alloc_release(packet_num);
	else
	{
		tx_buffer[0] = smc.reg[B0_EPH_STATUS] & 0xff;
		tx_buffer[1] = smc.reg[B0_EPH_STATUS] >> 8;
		FIFO_PUSH(completed_tx, packet_num);
	}

	update_ethernet_irq();

	if (smc.loopback_result)
		recv_complete(smc.loopback_result);

	if ((smc.reg[B0_TCR] & TXENA) && !FIFO_EMPTY(queued_tx))
		start_tx_poll(TIME_IN_USEC(10));

	smc.tx_active = 0;
}


static void process_command(UINT16 data)
{
	switch ((data >> 4) & 0xf)
	{
		case ECMD_NOP:
			break;

		case ECMD_ALLOCATE:
		{
			int packet_num;
			if (alloc_req(1, &packet_num))
			{
				smc.reg[B2_PNR_ARR] &= ~0xff00;
				smc.reg[B2_PNR_ARR] |= packet_num << 8;
				smc.reg[B2_INTERRUPT] |= EINT_ALLOC;
				update_ethernet_irq();
			}
			break;
		}

		case ECMD_RESET_MMU:
			mmu_reset();
			break;

		case ECMD_REMOVE_TOPFRAME_TX:
			if (!FIFO_EMPTY(completed_tx))
				FIFO_POP(completed_tx);
			break;

		case ECMD_REMOVE_RELEASE_TOPFRAME_RX:
			alloc_release(FIFO_CURR(completed_rx));
			/* fall through */
		case ECMD_REMOVE_TOPFRAME_RX:
			if (!FIFO_EMPTY(completed_rx))
				FIFO_POP(completed_rx);
			update_ethernet_irq();
			break;

		case ECMD_RELEASE_PACKET:
			alloc_release(smc.reg[B2_PNR_ARR] & 0xff);
			break;

		case ECMD_ENQUEUE_PACKET:
			if (smc.reg[B0_TCR] & TXENA)
			{
				FIFO_PUSH(queued_tx, smc.reg[B2_PNR_ARR] & 0xff);
				if (!smc.tx_active && !smc.tx_poll_pending)
					start_tx_poll(TIME_IN_USEC(10));
			}
			break;

		case ECMD_RESET_FIFOS:
			reset_tx_fifos();
			break;
	}
	smc.reg[B2_MMU_COMMAND] |= 0x0001;
}


static void tx_done_callback(int param)
{
	send_complete(0);
}


void smc91c94_reset(void)
{
	smc.irq_state = 0;
	smc.tx_active = 0;
	smc.rx_active = 0;
	smc.loopback_result = 0;

	smc.reg[B0_TCR]          = 0x0000;   smc.regmask[B0_TCR]          = 0xbd87;
	smc.reg[B0_EPH_STATUS]   = 0x4000;   smc.regmask[B0_EPH_STATUS]   = 0x0000;
	smc.reg[B0_RCR]          = 0x0000;   smc.regmask[B0_RCR]          = 0xc307;
	smc.reg[B0_COUNTER]      = 0x0000;   smc.regmask[B0_COUNTER]      = 0x0000;
	smc.reg[B0_MCR]          = 0x3300;   smc.regmask[B0_MCR]          = 0x00ff;
	smc.reg[B0_BANK]         = 0x3330;   smc.regmask[B0_BANK]         = 0x0007;

	smc.reg[B1_GENERAL_PURP] = 0x0000;   smc.regmask[B1_GENERAL_PURP] = 0xffff;
	smc.reg[B1_CONTROL]      = 0x0100;   smc.regmask[B1_CONTROL]      = 0x68e7;

	smc.reg[B2_MMU_COMMAND]  = 0x0000;   smc.regmask[B2_MMU_COMMAND]  = 0x00e7;
	smc.reg[B2_PNR_ARR]      = 0x8000;   smc.regmask[B2_PNR_ARR]      = 0x00ff;
	smc.reg[B2_FIFO_PORTS]   = 0x8080;   smc.regmask[B2_FIFO_PORTS]   = 0x0000;
	smc.reg[B2_POINTER]      = 0x0000;   smc.regmask[B2_POINTER]      = 0xf7ff;
	smc.reg[B2_DATA_0]       = 0x0000;   smc.regmask[B2_DATA_0]       = 0xffff;
	smc.reg[B2_DATA_1]       = 0x0000;   smc.regmask[B2_DATA_1]       = 0xffff;
	smc.reg[B2_INTERRUPT]    = 0x0004;   smc.regmask[B2_INTERRUPT]    = 0x7f00;

	smc.reg[B3_MT0_1]        = 0x0000;   smc.regmask[B3_MT0_1]        = 0xffff;
	smc.reg[B3_MT2_3]        = 0x0000;   smc.regmask[B3_MT2_3]        = 0xffff;
	smc.reg[B3_MT4_5]        = 0x0000;   smc.regmask[B3_MT4_5]        = 0xffff;
	smc.reg[B3_MT6_7]        = 0x0000;   smc.regmask[B3_MT6_7]        = 0xffff;
	smc.reg[B3_MGMT]         = 0x3030;   smc.regmask[B3_MGMT]         = 0x0f0f;
	smc.reg[B3_ERCV]         = 0x331f;   smc.regmask[B3_ERCV]         = 0x009f;

	timer_adjust(smc.tx_done, TIME_NEVER, 0, 0);
	update_ethernet_irq();
	mmu_reset();
}


void smc91c94_init(void (*irq_callback)(int state))
{
	memset(&smc, 0, sizeof(smc));
	smc.irq_callback = irq_callback;
	smc.tx_poll = timer_alloc(tx_poll);
	smc.tx_done = timer_alloc(tx_done_callback);

	/* these registers aren't cleared on reset */
	smc.reg[B1_CONFIG]  = 0x0030;   smc.regmask[B1_CONFIG]  = 0x17c6;
	smc.reg[B1_BASE]    = 0x1866;   smc.regmask[B1_BASE]    = 0xfffe;
	smc.reg[B1_IA0_1]   = 0x0000;   smc.regmask[B1_IA0_1]   = 0xffff;
	smc.reg[B1_IA2_3]   = 0x0000;   smc.regmask[B1_IA2_3]   = 0xffff;
	smc.reg[B1_IA4_5]   = 0x0000;   smc.regmask[B1_IA4_5]   = 0xffff;
	smc.reg[B3_REVISION] = 0x3345;  smc.regmask[B3_REVISION] = 0x0000;
	smc.reg[B0_MIR]     = 0x1212;   smc.regmask[B0_MIR]     = 0x0000;

	state_save_register_UINT16("smc91c94", 0, "reg",          smc.reg, 64);
	state_save_register_UINT16("smc91c94", 0, "regmask",      smc.regmask, 64);
	state_save_register_UINT8 ("smc91c94", 0, "irq_state",    &smc.irq_state, 1);
	state_save_register_UINT8 ("smc91c94", 0, "buffer",       smc.buffer, sizeof(smc.buffer));
	state_save_register_UINT32("smc91c94", 0, "alloc_rx",     &smc.alloc_rx, 1);
	state_save_register_UINT32("smc91c94", 0, "alloc_tx",     &smc.alloc_tx, 1);
	state_save_register_INT32 ("smc91c94", 0, "tx_active",    &smc.tx_active, 1);
	state_save_register_INT32 ("smc91c94", 0, "rx_active",    &smc.rx_active, 1);
	state_save_register_INT32 ("smc91c94", 0, "tx_poll_pending", &smc.tx_poll_pending, 1);
	state_save_register_INT32 ("smc91c94", 0, "loopback",     &smc.loopback_result, 1);
	state_save_register_UINT8 ("smc91c94", 0, "rx_hash",      &smc.rx_hash, 1);
	state_save_register_UINT8 ("smc91c94", 0, "queued_tx",    smc.queued_tx, FIFO_SIZE);
	state_save_register_INT32 ("smc91c94", 0, "queued_tx_h",  &smc.queued_tx_h, 1);
	state_save_register_INT32 ("smc91c94", 0, "queued_tx_t",  &smc.queued_tx_t, 1);
	state_save_register_UINT8 ("smc91c94", 0, "completed_tx", smc.completed_tx, FIFO_SIZE);
	state_save_register_INT32 ("smc91c94", 0, "completed_tx_h", &smc.completed_tx_h, 1);
	state_save_register_INT32 ("smc91c94", 0, "completed_tx_t", &smc.completed_tx_t, 1);
	state_save_register_UINT8 ("smc91c94", 0, "completed_rx", smc.completed_rx, FIFO_SIZE);
	state_save_register_INT32 ("smc91c94", 0, "completed_rx_h", &smc.completed_rx_h, 1);
	state_save_register_INT32 ("smc91c94", 0, "completed_rx_t", &smc.completed_rx_t, 1);

	smc91c94_reset();
}


data16_t smc91c94_r(int offset, data16_t mem_mask)
{
	UINT32 result;

	/* the effective register */
	offset &= 7;
	if (offset != B0_BANK)
		offset += 8 * (smc.reg[B0_BANK] & 7);

	result = smc.reg[offset];

	switch (offset)
	{
		case B2_MMU_COMMAND:
			smc.reg[B2_MMU_COMMAND] &= ~0x0001;
			break;

		case B2_PNR_ARR:
			if (mem_mask & 0xff00)
			{
				smc.reg[B2_INTERRUPT] &= ~EINT_ALLOC;
				update_ethernet_irq();
			}
			break;

		case B2_FIFO_PORTS:
			result = 0;
			if (!FIFO_EMPTY(completed_tx))
				result |= FIFO_CURR(completed_tx);
			else
				result |= 0x80;
			if (!FIFO_EMPTY(completed_rx))
				result |= FIFO_CURR(completed_rx) << 8;
			else
				result |= 0x80 << 8;
			break;

		case B2_DATA_0:
		case B2_DATA_1:
		{
			UINT8 *buffer;
			int addr = smc.reg[B2_POINTER] & PTR_MASK;

			if (smc.reg[B2_POINTER] & PTR_RCV)
				buffer = &smc.buffer[FIFO_CURR(completed_rx) * ETHER_BUFFER_SIZE];
			else
				buffer = &smc.buffer[(smc.reg[B2_PNR_ARR] & 0x1f) * ETHER_BUFFER_SIZE];

			result = 0;
			if (mem_mask & 0x00ff)
				result = buffer[addr++];
			if (mem_mask & 0xff00)
				result |= buffer[addr++] << 8;
			if (smc.reg[B2_POINTER] & PTR_AUTO_INCR)
				smc.reg[B2_POINTER] = (smc.reg[B2_POINTER] & ~PTR_MASK) | (addr & PTR_MASK);
			break;
		}
	}
	return result;
}


void smc91c94_w(int offset, data16_t data, data16_t mem_mask)
{
	UINT16 old_reg, new_reg;

	/* the effective register */
	offset &= 7;
	if (offset != B0_BANK)
		offset += 8 * (smc.reg[B0_BANK] & 7);

	old_reg = smc.reg[offset];
	mem_mask &= smc.regmask[offset];
	smc.reg[offset] = (old_reg & ~mem_mask) | (data & mem_mask);
	new_reg = smc.reg[offset];

	switch (offset)
	{
		case B0_TCR:
			if (data & TXENA)
				if (smc.reg[B0_EPH_STATUS] & (LOST_CARR | SQET | LATCOL | E16COL))
				{
					smc.reg[B0_EPH_STATUS] &= ~(LOST_CARR | SQET | LATCOL | E16COL);
					smc.reg[B2_INTERRUPT] &= ~EINT_EPH;
					update_ethernet_irq();
				}
			break;

		case B0_RCR:
			if (data & SOFT_RST)
				smc91c94_reset();
			if ((old_reg & RXEN) && !(new_reg & RXEN))
				FIFO_RESET(completed_rx);
			if (data & RXEN)
				smc.reg[B0_EPH_STATUS] |= LINK_OK;
			break;

		case B1_CONTROL:
			if ((old_reg & LE_ENABLE) && !(new_reg & LE_ENABLE))
			{
				smc.reg[B2_INTERRUPT] &= ~EINT_EPH;
				update_ethernet_irq();
			}
			break;

		case B2_MMU_COMMAND:
			process_command(data);
			break;

		case B2_DATA_0:
		case B2_DATA_1:
		{
			UINT8 *buffer;
			int addr = smc.reg[B2_POINTER] & PTR_MASK;

			if (smc.reg[B2_POINTER] & PTR_RCV)
				buffer = &smc.buffer[FIFO_CURR(completed_rx) * ETHER_BUFFER_SIZE];
			else
				buffer = &smc.buffer[(smc.reg[B2_PNR_ARR] & 0x1f) * ETHER_BUFFER_SIZE];

			if (mem_mask & 0x00ff)
				buffer[addr++] = data;
			if (mem_mask & 0xff00)
				buffer[addr++] = data >> 8;
			if (smc.reg[B2_POINTER] & PTR_AUTO_INCR)
				smc.reg[B2_POINTER] = (smc.reg[B2_POINTER] & ~PTR_MASK) | (addr & PTR_MASK);
			break;
		}

		case B2_INTERRUPT:
			if (smc.reg[B2_INTERRUPT] & data & EINT_TX)
				if (!FIFO_EMPTY(completed_tx))
					FIFO_POP(completed_tx);
			smc.reg[B2_INTERRUPT] &= ~(data & (EINT_ERCV | EINT_RX_OVRN | EINT_TX_EMPTY | EINT_TX));
			update_ethernet_irq();
			break;
	}
}
