/***************************************************************************

    ADSP-21062 SHARC DMA operations, from current MAME
    (src/devices/cpu/sharc/sharcdma.hxx, copyright Ville Linde,
    BSD-3-Clause), translated to C for MAME 2003.
    Included from sharc.c, not compiled on its own.

    Only the interpreter paths are translated: MAME uses the DMA timer
    (sharc_dma_callback) with the recompiler only; the interpreter
    transfers one DMA word per instruction (dma_run_cycle).

***************************************************************************/

#define DMA_PMODE_NO_PACKING        0
#define DMA_PMODE_16_32             1
#define DMA_PMODE_16_48             2
#define DMA_PMODE_32_48             3
#define DMA_PMODE_8_48              4

static void schedule_chained_dma_op(int channel, UINT32 dma_chain_ptr, int chained_direction)
{
	UINT32 op_ptr = 0x20000 + (dma_chain_ptr & 0x1ffff);

	UINT32 int_index        = dm_read32(op_ptr - 0);
	UINT32 int_modifier     = dm_read32(op_ptr - 1);
	UINT32 int_count        = dm_read32(op_ptr - 2);
	UINT32 chain_ptr        = dm_read32(op_ptr - 3);
	/*UINT32 gen_purpose      = dm_read32(op_ptr - 4);*/
	UINT32 ext_index        = dm_read32(op_ptr - 5);
	UINT32 ext_modifier     = dm_read32(op_ptr - 6);
	UINT32 ext_count        = dm_read32(op_ptr - 7);

	if (m_core->dma_op[channel].active)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: schedule_chained_dma_op: DMA operation already scheduled at %08X!\n", m_core->pc);
	}

	if (chained_direction)      /* Transmit to external */
	{
		m_core->dma_op[channel].dst           = ext_index;
		m_core->dma_op[channel].dst_modifier  = (INT32)ext_modifier;
		m_core->dma_op[channel].dst_count     = (INT32)ext_count;
		m_core->dma_op[channel].src           = int_index;
		m_core->dma_op[channel].src_modifier  = (INT32)int_modifier;
		m_core->dma_op[channel].src_count     = (INT32)int_count;
	}
	else                        /* Receive from external */
	{
		m_core->dma_op[channel].src           = ext_index;
		m_core->dma_op[channel].src_modifier  = (INT32)ext_modifier;
		m_core->dma_op[channel].src_count     = (INT32)ext_count;
		m_core->dma_op[channel].dst           = int_index;
		m_core->dma_op[channel].dst_modifier  = (INT32)int_modifier;
		m_core->dma_op[channel].dst_count     = (INT32)int_count;
	}

	m_core->dma_op[channel].pmode = 0;
	m_core->dma_op[channel].chain_ptr = chain_ptr;
	m_core->dma_op[channel].chained_direction = chained_direction;

	m_core->dma_op[channel].chained = 1;
	m_core->dma_op[channel].active = 1;

	/* enable busy flag */
	m_core->dma_status |= (1 << channel);
}

static void schedule_dma_op(int channel, UINT32 src, UINT32 dst, int src_modifier, int dst_modifier, int src_count, int dst_count, int pmode)
{
	if (m_core->dma_op[channel].active)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: schedule_dma_op: DMA operation already scheduled at %08X!\n", m_core->pc);
	}

	m_core->dma_op[channel].src = src;
	m_core->dma_op[channel].dst = dst;
	m_core->dma_op[channel].src_modifier = src_modifier;
	m_core->dma_op[channel].dst_modifier = dst_modifier;
	m_core->dma_op[channel].src_count = src_count;
	m_core->dma_op[channel].dst_count = dst_count;
	m_core->dma_op[channel].pmode = pmode;
	m_core->dma_op[channel].chain_ptr = 0;

	m_core->dma_op[channel].chained = 0;
	m_core->dma_op[channel].active = 1;

	/* enable busy flag */
	m_core->dma_status |= (1 << channel);
}

/* one DMA transfer; returns 1 if a driver callback stalled it (port:
   nothing is committed then, and the same transfer is retried later) */
static int dma_run_cycle(int channel)
{
	if (m_core->dma_status & (1 << channel))
	{
		UINT32 src = m_core->dma_op[channel].src;
		UINT32 dst = m_core->dma_op[channel].dst;
		int src_modifier = m_core->dma_op[channel].src_modifier;
		int dst_modifier = m_core->dma_op[channel].dst_modifier;
		int src_count = m_core->dma_op[channel].src_count;
		int pmode = m_core->dma_op[channel].pmode;

		m_core->stalled = 0;

		switch (pmode)
		{
			case DMA_PMODE_NO_PACKING:
			{
				UINT32 data = dm_read32(src);
				if (m_core->stalled) break;
				dm_write32(dst, data);
				src += src_modifier;
				dst += dst_modifier;
				src_count--;
				break;
			}
			case DMA_PMODE_16_32:
			{
				UINT32 data = (dm_read32(src + 0) & 0xffff) << 16;
				data |= dm_read32(src + 1) & 0xffff;
				if (m_core->stalled) break;

				dm_write32(dst, data);
				src += src_modifier * 2;
				dst += dst_modifier;
				src_count -= 2;
				break;
			}
			case DMA_PMODE_8_48:
			{
				UINT64 data = ((UINT64)(dm_read32(src + 0) & 0xff) << 0);
				data |= ((UINT64)(dm_read32(src + 1) & 0xff) << 8);
				data |= ((UINT64)(dm_read32(src + 2) & 0xff) << 16);
				data |= ((UINT64)(dm_read32(src + 3) & 0xff) << 24);
				data |= ((UINT64)(dm_read32(src + 4) & 0xff) << 32);
				data |= ((UINT64)(dm_read32(src + 5) & 0xff) << 40);
				if (m_core->stalled) break;

				pm_write48(dst, data);
				src += src_modifier * 6;
				dst += dst_modifier;
				src_count -= 6;
				break;
			}
			default:
			{
				/* MAME: fatal error */
				sharc_error("SHARC: dma_op: unimplemented packing mode %d\n", pmode);
				/* drop the transfer instead of looping on it forever */
				src_count = 0;
				break;
			}
		}

		if (m_core->stalled)
		{
			m_core->stalled = 0;
			return 1;
		}

		m_core->dma_op[channel].src_count = src_count;
		m_core->dma_op[channel].src = src;
		m_core->dma_op[channel].dst = dst;

		if (src_count <= 0)
		{
			/* clear busy flag */
			m_core->dma_status &= ~(1 << channel);

			m_core->dma_op[channel].active = 0;

			/* raise interrupt if
			    * non-chained DMA
			    * chained-DMA with PCI set in the next chain pointer */
			if (!m_core->dma_op[channel].chained || (m_core->dma_op[channel].chained && (m_core->dma_op[channel].chain_ptr & 0x20000)))
			{
				m_core->irptl |= (1 << (channel + 10));

				/* DMA interrupt */
				if (m_core->imask & (1 << (channel + 10)))
				{
					m_core->irq_pending |= 1 << (channel + 10);
				}
			}

			/* schedule chained op if chaining */
			if ((m_core->dma_op[channel].chain_ptr & 0x1ffff) != 0)
			{
				schedule_chained_dma_op(channel, m_core->dma_op[channel].chain_ptr, m_core->dma_op[channel].chained_direction);
			}
		}
	}
	return 0;
}

/* whole transfer at once (used by the EPROM boot) */
static void dma_op(int channel)
{
	int i;
	UINT32 src          = m_core->dma_op[channel].src;
	UINT32 dst          = m_core->dma_op[channel].dst;
	int src_modifier    = m_core->dma_op[channel].src_modifier;
	int dst_modifier    = m_core->dma_op[channel].dst_modifier;
	int src_count       = m_core->dma_op[channel].src_count;
	int pmode           = m_core->dma_op[channel].pmode;

	switch (pmode)
	{
		case DMA_PMODE_NO_PACKING:
		{
			for (i=0; i < src_count; i++)
			{
				UINT32 data = dm_read32(src);
				dm_write32(dst, data);
				src += src_modifier;
				dst += dst_modifier;
			}
			break;
		}
		case DMA_PMODE_16_32:
		{
			int length = src_count/2;
			for (i=0; i < length; i++)
			{
				UINT32 data = (dm_read32(src+0) & 0xffff) << 16;
				data |= dm_read32(src+1) & 0xffff;

				dm_write32(dst, data);
				src += src_modifier * 2;
				dst += dst_modifier;
			}
			break;
		}
		case DMA_PMODE_8_48:
		{
			int length = src_count/6;
			for (i=0; i < length; i++)
			{
				UINT64 data = ((UINT64)(dm_read32(src+0) & 0xff) <<  0);
				data |= ((UINT64)(dm_read32(src+1) & 0xff) <<  8);
				data |= ((UINT64)(dm_read32(src+2) & 0xff) << 16);
				data |= ((UINT64)(dm_read32(src+3) & 0xff) << 24);
				data |= ((UINT64)(dm_read32(src+4) & 0xff) << 32);
				data |= ((UINT64)(dm_read32(src+5) & 0xff) << 40);

				pm_write48(dst, data);
				src += src_modifier * 6;
				dst += dst_modifier;
			}
			break;
		}
		default:
		{
			/* MAME: fatal error */
			sharc_error("SHARC: dma_op: unimplemented packing mode %d\n", pmode);
			break;
		}
	}

	if (channel == 6)
	{
		m_core->irptl |= (1 << (channel+10));

		/* DMA interrupt */
		if (m_core->imask & (1 << (channel+10)))
		{
			m_core->irq_pending |= 1 << (channel+10);
		}
	}

	/* clear busy flag */
	m_core->dma_status &= ~(1 << channel);

	m_core->dma_op[channel].active = 0;
}

static void sharc_dma_exec(int channel)
{
	UINT32 src, dst;
	UINT32 src_count, dst_count;
	UINT32 src_modifier, dst_modifier;
	int chen, tran, dtype, pmode, ishake, intio;

	chen = (m_core->dma[channel].control >> 1) & 0x1;
	tran = (m_core->dma[channel].control >> 2) & 0x1;
	dtype = (m_core->dma[channel].control >> 5) & 0x1;
	pmode = (m_core->dma[channel].control >> 6) & 0x3;
	ishake = (m_core->dma[channel].control >> 10) & 0x1;
	intio = (m_core->dma[channel].control >> 11) & 0x1;

	/* MAME: fatal errors */
	if (ishake)
		sharc_error("SHARC: dma_exec: handshake not supported\n");
	if (intio)
		sharc_error("SHARC: dma_exec: single-word interrupt enable not supported\n");


	if (chen)       /* Chained DMA */
	{
		UINT32 dma_chain_ptr = m_core->dma[channel].chain_ptr & 0x1ffff;

		schedule_chained_dma_op(channel, dma_chain_ptr, tran);
	}
	else
	{
		if (tran)       /* Transmit to external */
		{
			dst = m_core->dma[channel].ext_index;
			dst_modifier = m_core->dma[channel].ext_modifier;
			dst_count = m_core->dma[channel].ext_count;
			src = (m_core->dma[channel].int_index & 0x1ffff) | 0x20000;
			src_modifier = m_core->dma[channel].int_modifier;
			src_count = m_core->dma[channel].int_count;
		}
		else            /* Receive from external */
		{
			src = m_core->dma[channel].ext_index;
			src_modifier = m_core->dma[channel].ext_modifier;
			src_count = m_core->dma[channel].ext_count;
			dst = (m_core->dma[channel].int_index & 0x1ffff) | 0x20000;
			dst_modifier = m_core->dma[channel].int_modifier;
			dst_count = m_core->dma[channel].int_count;
		}

		if (dtype)
		{
			pmode = DMA_PMODE_8_48;
		}

		schedule_dma_op(channel, src, dst, (INT32)src_modifier, (INT32)dst_modifier, (INT32)src_count, (INT32)dst_count, pmode);
	}
}
