/*****************************************************************************

	sh3.c

	Hitachi SH-3 (SH7709S) CPU core, from current MAME:
	- the instructions of sh.cpp (common SH-2/SH-3/SH-4) and sh4.cpp
	  (the SH-3/SH-4 ones), by R. Belmont, David Haywood and others;
	- the exceptions and interrupts of sh4comn.cpp, the timer unit of
	  sh4tmu.cpp and the SH7709S registers of sh3comn.cpp (R. Belmont);
	- the SH7709S cache and bus timing of sh7709s.cpp (Jesus Ramos).

	The cycle counts are the ones of MAME's recompiler (sh_fe.cpp), which
	is what MAME runs the CV1000 with: it is what sets the speed and the
	slowdowns of the games. Like there, the PC-relative loads (MOV.W/MOV.L
	@(disp,PC)) and the loads of MAC, LDC.L @Rm+,SR and the exceptions cost
	no bus cycles, every other data access goes through the cache model.

	The delay slot runs with its own PC (MAME's interpreter runs it with
	the branch target as PC; MAME's recompiler does it right).

	No FPU (the FPU opcodes do nothing) and no MMU.

*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "state.h"
#include "mamedbg.h"
#include "sh3.h"

#ifdef MAME_DEBUG
extern unsigned DasmSH2(char *buffer, unsigned pc);
#endif

/* SR bits */
#define SR_T		0x00000001
#define SR_S		0x00000002
#define SR_I		0x000000f0
#define SR_Q		0x00000100
#define SR_M		0x00000200
#define SR_FD		0x00008000
#define SR_BL		0x10000000
#define SR_RB		0x20000000
#define SR_MD		0x40000000
#define SR_FLAGS	(SR_MD | SR_RB | SR_BL | SR_FD | SR_M | SR_Q | SR_I | SR_S | SR_T)

#define AM			0x1fffffff

#define Rn			((opcode >> 8) & 15)
#define Rm			((opcode >> 4) & 15)

/* the exceptions and interrupts, as numbered in current MAME */
enum
{
	INTC_TRAP = 21,
	INTC_NMI = 23,
	INTC_IRLn0,
	INTC_IRL0 = INTC_IRLn0 + 15,
	INTC_IRL1, INTC_IRL2, INTC_IRL3,
	INTC_HUDI, INTC_GPOI,
	INTC_DMTE0, INTC_DMTE1, INTC_DMTE2, INTC_DMTE3,
	INTC_DMTE4, INTC_DMTE5, INTC_DMTE6, INTC_DMTE7,
	INTC_DMAE,
	INTC_TUNI3, INTC_TUNI4, INTC_TUNI0, INTC_TUNI1, INTC_TUNI2, INTC_TICPI2,
	INTC_ATI, INTC_PRI, INTC_CUI,
	INTC_SCI1ERI, INTC_SCI1RXI, INTC_SCI1TXI, INTC_SCI1TEI,
	INTC_SCIFERI, INTC_SCIFRXI, INTC_SCIFBRI, INTC_SCIFTXI,
	INTC_ITI, INTC_RCMI, INTC_ROVI,
	INTC_COUNT
};

#define EXPPRI(pl, po, p, n)	(((4 - (pl)) << 24) | ((15 - (po)) << 16) | ((p) << 8) | (255 - (n)))
#define INTPRI(p, n)			EXPPRI(4, 2, p, n)

/* the SH7709S cache */
#define CACHE_LINE_SIZE		16
#define CACHE_ASSOC			4
#define CACHE_BLOCKS		((0x4000 / CACHE_LINE_SIZE) / CACHE_ASSOC)

typedef struct
{
	UINT32	ppc, pc, pr, sr, gbr, vbr, mach, macl;
	UINT32	r[16];
	UINT32	rbnk[2][8];
	UINT32	ssr, spc, sgr, dbr, fpscr, fpul;
	UINT32	ea;
	UINT32	sleep_mode;

	/* bus timing (first: the fields of every access near the start) */
	int		cache_timing;
	UINT32	cache_port_free;
	UINT32	burst_continuation_remaining_cycles;
	UINT32	last_access_address;
	INT32	last_op_rel;		/* see last_op_cycles() */
	UINT32	wb_address;
	UINT32	last_area_accessed;
	UINT32	last_area_accessed_was_write;
	UINT32	wb_active_cycles;
	UINT32	last_op_cycles_lo, last_op_cycles_hi;	/* 64-bit cycle counts, saved */
	UINT32	total_cycles_lo, total_cycles_hi;

	/* interrupts */
	UINT32	pending_irq;
	UINT32	test_irq;
	UINT8	exception_requesting[INTC_COUNT];
	UINT32	exception_priority[INTC_COUNT];
	INT32	irq_line_state[4];
	int		(*irq_callback)(int irqline);

	/* the internal registers, big endian: 0xfffffe80-0xffffffff and
	   0x04000000-0x040002ff (physical) */
	UINT8	regp4[0x180];
	UINT8	rega1[0x300];

	/* timer unit */
	UINT32	tcor[3], tcnt[3], tcr[3];
	UINT8	tstr;
	void	*timer[3];

	/* DMA controller */
	UINT8	dma_timer_active[4];
	void	*dma_timer[4];

	/* configuration */
	int		cpunum;
	int		pm_clock;
	data8_t	(*port_r)(int port);
	void	(*port_w)(int port, data8_t data);

	/* the cache: the most recently used way of each block (the one with
	   LRU 3) or CACHE_ASSOC, and its tag (CACHE_NO_TAG for no way or a tag
	   of 0); kept with cache_lru and cache_tag, rebuilt from them when
	   the cache is cleared or comes from a state */
	UINT32	cache_mru_tag[CACHE_BLOCKS];
	UINT8	cache_mru[CACHE_BLOCKS];
	UINT32	cache_tag[CACHE_BLOCKS][CACHE_ASSOC];
	UINT8	cache_lru[CACHE_BLOCKS][CACHE_ASSOC];
	UINT8	cache_dirty[CACHE_BLOCKS][CACHE_ASSOC];
} SH3;

int sh3_ICount;
static SH3 sh3;

/* the cycles run before this call of sh3_execute, and the ones asked to it */
static UINT64 sh3_exec_base;
static int sh3_exec_cycles;

/* fast memory */
typedef struct
{
	UINT32 start, end;
	int readonly;
	UINT8 *base;
} sh3_fastram;

static sh3_fastram fastram[SH3_FASTRAM_MAX];
static int fastram_count;

/* the region of each 64 KB page of the physical space: its index, -1 for
   none, -2 for a page that is not all in one region (looked for one by one) */
#define FASTRAM_PAGE_SHIFT	16
static INT8 fastram_page[(AM + 1) >> FASTRAM_PAGE_SHIFT];

/* the recompiler (sh3jit.c): where its code comes from in RAM, one byte
   per 4-byte granule of the physical address; a write there flushes it */
#define JIT_CODEMAP_SIZE	(1 << 22)
static UINT8 *jit_codemap;
static int jit_code_present;
static int jit_flush_pending;

static INLINE void jit_check_write(UINT32 a)
{
	if (jit_code_present && jit_codemap[(a >> 2) & (JIT_CODEMAP_SIZE - 1)])
		jit_flush_pending = 1;
}

static void sh3_timer_callback(int which);
static void sh3_cache_invalidate_penalty(void);


/***************************************************************************
	cycle counts
***************************************************************************/

static INLINE UINT64 total_cycles(void)
{
	return sh3_exec_base + (sh3_exec_cycles - sh3_ICount);
}

/* the cycle count of the last bus access, kept in 32 bits: relative to
   sh3_exec_base, or LAST_OP_FAR when it is more than 2^30 cycles away from
   it. The bus timing only compares the cycles elapsed since that access
   with small counts, so far is the same as any large count. The exact
   64-bit value is in sh3.last_op_cycles_lo/hi for the save states, and
   while far. */
#define LAST_OP_NEAR		(1 << 30)
#define LAST_OP_FAR			((INT32)0x80000000)

/* the cycles from the last bus access to 'now' (sh3_exec_cycles -
   sh3_ICount): exact up to 2^30, and at least 2^30 when more (far: 2^31
   and more) or when the access is ahead */
static INLINE UINT32 elapsed_since_last_op(INT32 now)
{
	return (UINT32)now - (UINT32)sh3.last_op_rel;
}

static UINT64 last_op_cycles(void)
{
	if (sh3.last_op_rel == LAST_OP_FAR)
		return ((UINT64)sh3.last_op_cycles_hi << 32) | sh3.last_op_cycles_lo;
	return sh3_exec_base + (INT64)sh3.last_op_rel;
}

static void set_last_op_cycles(UINT64 c)
{
	INT64 d = (INT64)(c - sh3_exec_base);
	sh3.last_op_cycles_lo = (UINT32)c;
	sh3.last_op_cycles_hi = (UINT32)(c >> 32);
	sh3.last_op_rel = (d < -LAST_OP_NEAR || d > LAST_OP_NEAR) ? LAST_OP_FAR : (INT32)d;
}


/***************************************************************************
	internal registers
***************************************************************************/

static INLINE UINT8 *reg_ptr(UINT32 a)
{
	if (a >= 0xfffffe80)
		return &sh3.regp4[a - 0xfffffe80];
	if (a >= 0x04000000 && a < 0x04000300)
		return &sh3.rega1[a - 0x04000000];
	return NULL;
}

static INLINE UINT32 reg32(UINT32 a)
{
	UINT8 *p = reg_ptr(a);
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static INLINE UINT16 reg16(UINT32 a)
{
	UINT8 *p = reg_ptr(a);
	return (p[0] << 8) | p[1];
}

static INLINE void set_reg32(UINT32 a, UINT32 d)
{
	UINT8 *p = reg_ptr(a);
	p[0] = d >> 24; p[1] = d >> 16; p[2] = d >> 8; p[3] = d;
}

static INLINE void set_reg16(UINT32 a, UINT16 d)
{
	UINT8 *p = reg_ptr(a);
	p[0] = d >> 8; p[1] = d;
}

/* registers used by the core */
#define REG_TRA			0xffffffd0
#define REG_EXPEVT		0xffffffd4
#define REG_INTEVT		0xffffffd8
#define REG_CCR			0xffffffec
#define REG_BCR2		0xffffff62
#define REG_WCR1		0xffffff64
#define REG_WCR2		0xffffff66
#define REG_MCR			0xffffff68
#define REG_WTCNT		0xffffff84
#define REG_WTCSR		0xffffff86
#define REG_IPRA		0xfffffee2
#define REG_INTEVT2		0x04000000
#define REG_IRR0		0x04000004
#define REG_IPRC		0x04000016
#define REG_SCSSR		0xfffffe88


/***************************************************************************
	exceptions and interrupts (sh4comn.cpp)
***************************************************************************/

static const int exception_codes[INTC_COUNT] =
{
	0x000, 0x020, 0x000, 0x140, 0x140, 0x1E0, 0x0E0, 0x040, 0x0A0, 0x180,
	0x1A0, 0x800, 0x820, 0x0E0, 0x100, 0x040, 0x060, 0x0A0, 0x0C0, 0x120,
	0x080, 0x160, 0x1E0, 0x1C0,
	0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
	0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0,
	0x240, 0x2A0, 0x300, 0x360,
	0x600, 0x620, 0x640, 0x660, 0x680, 0x6A0,
	0x780, 0x7A0, 0x7C0, 0x7E0,
	0x6C0,
	0xB00, 0xB80, 0x400, 0x420, 0x440, 0x460, 0x480, 0x4A0, 0x4C0,
	0x4E0, 0x500, 0x520, 0x540,
	0x700, 0x720, 0x740, 0x760, 0x560, 0x580, 0x5A0
};

static const int sh3_intevt2_exception_codes[INTC_COUNT] =
{
	0x000, 0x020, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 0x160, -1, 0x1C0,
	0x200, 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0,
	0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0, 0x3C0,
	0x600, 0x620, 0x640, 0x660,
	-1, -1, 0x800, 0x820, 0x840, 0x860,
	-1, -1, -1, -1,
	-1,
	-1, -1, 0x400, 0x420, 0x440, 0x460, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1
};

static void change_register_bank(int to)
{
	int s;

	if (to)
	{
		for (s = 0; s < 8; s++)
		{
			sh3.rbnk[0][s] = sh3.r[s];
			sh3.r[s] = sh3.rbnk[1][s];
		}
	}
	else
	{
		for (s = 0; s < 8; s++)
		{
			sh3.rbnk[1][s] = sh3.r[s];
			sh3.r[s] = sh3.rbnk[0][s];
		}
	}
}

static void default_exception_priorities(void)
{
	static const UINT32 exception_priority_default[INTC_NMI + 1] =
	{
		EXPPRI(1, 1, 0, 0), EXPPRI(1, 2, 0, 1), EXPPRI(1, 1, 0, 2), EXPPRI(1, 3, 0, 3),
		EXPPRI(1, 4, 0, 4), EXPPRI(2, 0, 0, 5), EXPPRI(2, 1, 0, 6), EXPPRI(2, 2, 0, 7),
		EXPPRI(2, 3, 0, 8), EXPPRI(2, 4, 0, 9), EXPPRI(2, 4, 0, 10), EXPPRI(2, 4, 0, 11),
		EXPPRI(2, 4, 0, 12), EXPPRI(2, 5, 0, 13), EXPPRI(2, 5, 0, 14), EXPPRI(2, 6, 0, 15),
		EXPPRI(2, 6, 0, 16), EXPPRI(2, 7, 0, 17), EXPPRI(2, 7, 0, 18), EXPPRI(2, 8, 0, 19),
		EXPPRI(2, 9, 0, 20), EXPPRI(2, 4, 0, 21), EXPPRI(2, 10, 0, 22),
		EXPPRI(3, 0, 16, INTC_NMI)
	};
	int a;

	for (a = 0; a <= INTC_NMI; a++)
		sh3.exception_priority[a] = exception_priority_default[a];
	for (a = INTC_IRLn0; a <= INTC_IRLn0 + 14; a++)
		sh3.exception_priority[a] = INTPRI(15 - (a - INTC_IRLn0), a);
	sh3.exception_priority[INTC_IRL0] = INTPRI(13, INTC_IRL0);
	sh3.exception_priority[INTC_IRL1] = INTPRI(10, INTC_IRL1);
	sh3.exception_priority[INTC_IRL2] = INTPRI(7, INTC_IRL2);
	sh3.exception_priority[INTC_IRL3] = INTPRI(4, INTC_IRL3);
	for (a = INTC_HUDI; a <= INTC_ROVI; a++)
		sh3.exception_priority[a] = INTPRI(0, a);
}

static void exception_recompute(void)
{
	int z, a;

	sh3.test_irq = 0;
	if (!sh3.pending_irq || ((sh3.sr & SR_BL) && sh3.exception_requesting[INTC_NMI] == 0))
		return;
	z = (sh3.sr >> 4) & 15;
	for (a = 0; a < INTC_COUNT; a++)
	{
		if (sh3.exception_requesting[a])
		{
			int pri = ((int)sh3.exception_priority[a] >> 8) & 255;
			if (pri > z)
			{
				sh3.test_irq = 1;
				break;
			}
		}
	}
}

static void exception_request(int exception)
{
	if (!sh3.exception_requesting[exception])
	{
		sh3.exception_requesting[exception] = 1;
		sh3.pending_irq++;
		exception_recompute();
	}
}

static void exception_unrequest(int exception)
{
	if (sh3.exception_requesting[exception])
	{
		sh3.exception_requesting[exception] = 0;
		sh3.pending_irq--;
		exception_recompute();
	}
}

static void exception_process(int exception, UINT32 vector)
{
	/* sh4_exception_checkunrequest */
	if (exception == INTC_NMI ||
		exception == INTC_DMTE0 || exception == INTC_DMTE1 || exception == INTC_DMTE2 || exception == INTC_DMTE3)
		exception_unrequest(exception);

	sh3.spc = sh3.pc;
	sh3.ssr = sh3.sr;
	sh3.sgr = sh3.r[15];

	sh3.sr |= SR_MD;
	if (!(sh3.sr & SR_RB))
		change_register_bank(1);
	sh3.sr |= SR_RB;
	sh3.sr |= SR_BL;
	exception_recompute();

	sh3.pc = sh3.vbr + vector;
	if (sh3.sleep_mode == 1)
		sh3.sleep_mode = 2;
}

/* sh4_exception, SH-3 part: interrupts only */
static void sh3_exception(int exception)
{
	UINT32 intevt;

	if (exception <= INTC_NMI)
		return;
	if (sh3.sr & SR_BL)
		return;
	if (((sh3.exception_priority[exception] >> 8) & 255) <= ((sh3.sr >> 4) & 15))
		return;

	if (exception >= INTC_IRL0 && exception <= INTC_IRL3)
	{
		if (sh3.irq_callback)
			(*sh3.irq_callback)(exception - INTC_IRL0);
	}
	else if (sh3.irq_callback)
		(*sh3.irq_callback)(SH3_IRL3 + 1);

	if (sh3_intevt2_exception_codes[exception] == -1)
	{
		log_cb(RETRO_LOG_ERROR, LOGPRE "SH-3: interrupt %d has no INTEVT2 code\n", exception);
		return;
	}

	set_reg32(REG_INTEVT2, sh3_intevt2_exception_codes[exception]);
	set_reg32(REG_EXPEVT, exception_codes[exception]);
	if (sh3_intevt2_exception_codes[exception] >= 0x600)
		intevt = 0x3e0 - ((sh3.exception_priority[exception] >> 8) & 255) * 0x20;
	else
		intevt = sh3_intevt2_exception_codes[exception];
	set_reg32(REG_INTEVT, intevt);

	exception_process(exception, 0x600);
}

static void check_pending_irq(void)
{
	int irq = 0;
	int z = -1;
	int a;

	for (a = 0; a < INTC_COUNT; a++)
	{
		if (sh3.exception_requesting[a])
		{
			if ((int)sh3.exception_priority[a] > z)
			{
				z = sh3.exception_priority[a];
				irq = a;
			}
		}
	}
	if (z >= 0)
		sh3_exception(irq);
}


/***************************************************************************
	cache and bus timing (sh7709s.cpp)
***************************************************************************/

static INLINE int is_cacheable(UINT32 address)
{
	UINT32 region = address >> 29;
	return region != 0x5 && region != 0x7;
}

static INLINE UINT32 get_area(UINT32 address)
{
	return (address & AM) >> 26;
}

/* hardcoded for the CV1000: area 3 is the SDRAM */
static INLINE int is_sdram_region(UINT32 address)
{
	return get_area(address) == 3;
}

#define CACHE_NO_TAG		0xffffffff

static INLINE void cache_set_mru(UINT32 block, UINT32 way)
{
	UINT32 tag = sh3.cache_tag[block][way];
	sh3.cache_mru[block] = way;
	sh3.cache_mru_tag[block] = tag ? tag : CACHE_NO_TAG;
}

static void cache_mru_rebuild(void)
{
	UINT32 b, i;
	for (b = 0; b < CACHE_BLOCKS; b++)
	{
		sh3.cache_mru[b] = CACHE_ASSOC;
		sh3.cache_mru_tag[b] = CACHE_NO_TAG;
		for (i = 0; i < CACHE_ASSOC; i++)
			if (sh3.cache_lru[b][i] == CACHE_ASSOC - 1)
				cache_set_mru(b, i);
	}
}

static int cache_access(UINT32 address, int write)
{
	UINT32 cache_address, cache_block;
	int i, j;

	if (!is_cacheable(address))
		return 0;

	address &= AM;
	cache_address = address / CACHE_LINE_SIZE;
	cache_block = cache_address % CACHE_BLOCKS;

	for (i = 0; i < CACHE_ASSOC; i++)
	{
		if (sh3.cache_tag[cache_block][i] == cache_address)
		{
			if (write)
				sh3.cache_dirty[cache_block][i] = 1;
			if (sh3.cache_lru[cache_block][i] != CACHE_ASSOC - 1)
			{
				for (j = 0; j < CACHE_ASSOC; j++)
					if (sh3.cache_lru[cache_block][j] > sh3.cache_lru[cache_block][i])
						sh3.cache_lru[cache_block][j]--;
				sh3.cache_lru[cache_block][i] = CACHE_ASSOC - 1;
				cache_set_mru(cache_block, i);
			}
			return 1;
		}
	}

	/* a miss: evict the least recently used line */
	for (i = 0; i < CACHE_ASSOC; i++)
	{
		if (sh3.cache_lru[cache_block][i] == 0)
		{
			if (sh3.cache_dirty[cache_block][i])
				sh3.wb_address = sh3.cache_tag[cache_block][i] * CACHE_LINE_SIZE;
			sh3.cache_tag[cache_block][i] = cache_address;
			sh3.cache_lru[cache_block][i] = CACHE_ASSOC - 1;
			cache_set_mru(cache_block, i);
			sh3.cache_dirty[cache_block][i] = write;
			for (j = 0; j < CACHE_ASSOC; j++)
				if (i != j && sh3.cache_lru[cache_block][j] != 0)
					sh3.cache_lru[cache_block][j]--;
			break;
		}
	}

	return 0;
}

static UINT32 get_wcr1_timing(UINT32 area)
{
	UINT32 area_val;

	if (area > 6 || area == 1)
		return 0;
	area_val = (reg16(REG_WCR1) >> (area * 2)) & 0x3;
	if (area_val == 0)
		return 1;
	return area_val;
}

static UINT32 get_wcr2_timing(UINT32 address)
{
	static const UINT32 waits[8] = { 0, 1, 2, 3, 4, 6, 8, 10 };
	UINT32 area = get_area(address);
	UINT16 wcr2 = reg16(REG_WCR2);
	UINT32 area_val;

	if (area > 6 || area == 1)
		return 0;

	if (area == 0)
		area_val = wcr2 & 0x7;
	else if (area == 2 || area == 3)
	{
		wcr2 >>= 3;
		if (area == 3)
			wcr2 >>= 2;
		area_val = wcr2 & 0x3;
		if (area_val == 0)
			return 1;
		return area_val;
	}
	else
	{
		wcr2 >>= 7 + ((area - 4) * 3);
		area_val = wcr2 & 0x7;
	}
	return waits[area_val];
}

#define MCR_TPC()	(((reg16(REG_MCR) >> 14) & 0x3) + 1)
#define MCR_RCD()	(((reg16(REG_MCR) >> 12) & 0x3) + 1)
#define MCR_TRWL()	(((reg16(REG_MCR) >> 10) & 0x3) + 1)

static UINT32 cache_line_fetch_count(UINT32 address)
{
	UINT32 area = get_area(address);
	UINT32 bcr2_val;

	/* hardcoded for area 0, the 16-bit code ROM of the CV1000 */
	if (area == 0)
		return 8;
	if (area == 1)
		return 0;
	bcr2_val = (reg16(REG_BCR2) >> (area * 2)) & 0x3;
	return CACHE_LINE_SIZE >> (bcr2_val - 1);
}

/* the bus state controller registers decoded per area; they only change
   through writes to 0xffffff60-0xffffff6f, which clear bsc_valid */
static int bsc_valid;
static UINT32 bsc_wcr1[8], bsc_wcr2[8], bsc_fetch[8], bsc_rcd, bsc_trwl, bsc_tpc;

static void bsc_decode(void)
{
	UINT32 a;
	for (a = 0; a < 8; a++)
	{
		bsc_wcr1[a] = get_wcr1_timing(a);
		bsc_wcr2[a] = get_wcr2_timing(a << 26);
		bsc_fetch[a] = cache_line_fetch_count(a << 26);
	}
	bsc_rcd = MCR_RCD();
	bsc_trwl = MCR_TRWL();
	bsc_tpc = MCR_TPC();
	bsc_valid = 1;
}

static INLINE UINT32 remaining_cycles(UINT32 elapsed, UINT32 cycles)
{
	return (elapsed < cycles) ? (cycles - elapsed) : 0;
}

static UINT32 access_penalty(UINT32 address, int write, INT32 now)
{
	int is_in_cache;
	UINT32 elapsed_cycles;
	UINT32 cpu_penalty = 0;
	UINT32 area, bus_penalty = 0;

	if (!bsc_valid)
		bsc_decode();
	is_in_cache = cache_access(address, write);
	elapsed_cycles = elapsed_since_last_op(now);

	if (!sh3.cache_port_free)
		cpu_penalty++;
	sh3.cache_port_free = !sh3.cache_port_free;

	if (is_in_cache)
	{
		/* a fill in the background: wait for the words still to come */
		if (elapsed_cycles < 6 && sh3.burst_continuation_remaining_cycles >= 6)
		{
			UINT32 stall_fetch = sh3.burst_continuation_remaining_cycles - (UINT32)elapsed_cycles;
			sh3.burst_continuation_remaining_cycles -= 6;
			return stall_fetch + cpu_penalty;
		}
		return cpu_penalty;
	}

	area = get_area(address);
	cpu_penalty += is_cacheable(address) ? 1 : 0;

	if (is_sdram_region(address))
	{
		if (is_cacheable(address) || !write)
			bus_penalty += 1 + bsc_rcd + bsc_wcr2[area];
		else
			bus_penalty += 1 + bsc_rcd + bsc_trwl;
	}
	else
	{
		if (is_cacheable(address))
			bus_penalty += (2 + bsc_wcr2[area]) * bsc_fetch[area];
		else
			bus_penalty += 2 + bsc_wcr2[area];
	}

	if (area != sh3.last_area_accessed || (!sh3.last_area_accessed_was_write && !is_cacheable(address) && write))
	{
		UINT32 wcr1_idle_cycles = bsc_wcr1[area] * 2;
		UINT32 wcr1_idle_cycles_left = remaining_cycles(elapsed_cycles, wcr1_idle_cycles);
		cpu_penalty += (UINT32)wcr1_idle_cycles_left;
		elapsed_cycles -= (elapsed_cycles < wcr1_idle_cycles) ? elapsed_cycles : wcr1_idle_cycles;
	}

	sh3.last_area_accessed = area;
	sh3.last_area_accessed_was_write = (!is_cacheable(address) && write);

	if (sh3.burst_continuation_remaining_cycles > 0)
	{
		UINT32 burst_cycles_left = remaining_cycles(elapsed_cycles, sh3.burst_continuation_remaining_cycles);
		cpu_penalty += (UINT32)burst_cycles_left;
		elapsed_cycles -= (elapsed_cycles < sh3.burst_continuation_remaining_cycles) ? elapsed_cycles : sh3.burst_continuation_remaining_cycles;
		sh3.burst_continuation_remaining_cycles = 0;
	}

	if (sh3.wb_active_cycles > 0)
	{
		UINT32 wb_cycles_left = remaining_cycles(elapsed_cycles, sh3.wb_active_cycles);
		cpu_penalty += (UINT32)wb_cycles_left;
		elapsed_cycles -= (elapsed_cycles < sh3.wb_active_cycles) ? elapsed_cycles : sh3.wb_active_cycles;
		sh3.wb_active_cycles = 0;
	}

	if (sh3.wb_address != 0)
	{
		if (is_sdram_region(sh3.wb_address))
		{
			sh3.wb_active_cycles += bsc_tpc * 2;
			sh3.wb_active_cycles += 3 * 2;
			sh3.burst_continuation_remaining_cycles = 0;
			sh3.wb_active_cycles += (1 + bsc_rcd + 4 + bsc_trwl + bsc_wcr1[area] + bsc_tpc) * 2;
		}
		else
		{
			sh3.wb_active_cycles += bsc_wcr1[area] * 2;
			sh3.wb_active_cycles += ((2 + bsc_wcr2[get_area(sh3.wb_address)]) * bsc_fetch[get_area(sh3.wb_address)]) * 2;
		}
		sh3.wb_address = 0;
		sh3.last_area_accessed_was_write = 1;
	}
	else if (is_sdram_region(address) && is_cacheable(address))
		sh3.burst_continuation_remaining_cycles = (3 + bsc_tpc) * 2;

	return cpu_penalty + (bus_penalty * 2);
}

/* access_penalty() for a hit in the most recently used way of its block,
   the most frequent case: the same result without the LRU update (the way
   is already the most recent one) and the miss path. A tag of 0 can be in
   more than one way (lines never filled), and cache_access() takes the
   first one: those go through access_penalty(). */
static INLINE UINT32 access_penalty_fast(UINT32 address, int write, INT32 now)
{
	if (is_cacheable(address))
	{
		UINT32 cache_address = (address & AM) / CACHE_LINE_SIZE;
		UINT32 cache_block = cache_address % CACHE_BLOCKS;
		if (sh3.cache_mru_tag[cache_block] == cache_address)
		{
			UINT32 elapsed_cycles = elapsed_since_last_op(now);
			UINT32 cpu_penalty = sh3.cache_port_free ? 0 : 1;

			if (write)
				sh3.cache_dirty[cache_block][sh3.cache_mru[cache_block]] = 1;
			sh3.cache_port_free = !sh3.cache_port_free;
			if (elapsed_cycles < 6 && sh3.burst_continuation_remaining_cycles >= 6)
			{
				UINT32 stall_fetch = sh3.burst_continuation_remaining_cycles - (UINT32)elapsed_cycles;
				sh3.burst_continuation_remaining_cycles -= 6;
				return stall_fetch + cpu_penalty;
			}
			return cpu_penalty;
		}
	}
	return access_penalty(address, write, now);
}

static INLINE void update_access_cycles(UINT32 address, int write)
{
	/* the instruction fetch, sampled at the data accesses */
	INT32 icount = sh3_ICount;
	INT32 now = sh3_exec_cycles - icount;
	UINT32 penalty;
	if (is_cacheable(sh3.pc))
	{
		penalty = access_penalty_fast(sh3.pc, 0, now);
		icount -= penalty;
		now += penalty;
		sh3.last_op_rel = now;
	}
	penalty = access_penalty_fast(address, write, now);
	icount -= penalty;
	sh3.last_op_rel = now + penalty;
	sh3_ICount = icount;
}

void sh3_update_access_cycles(UINT32 address, int write)
{
	if (!sh3.cache_timing)
		return;
	update_access_cycles(address, write);
}

/* a data access of the recompiler: the address is the one of the last
   access below 0xe0000000 (current MAME doesn't store it for P4) */
static INLINE void data_access_timing(UINT32 address, int write)
{
	if (!sh3.cache_timing)
		return;
	if (address < 0xe0000000)
		sh3.last_access_address = address;
	update_access_cycles(sh3.last_access_address, write);
}

/* CCR write with CF: invalidate the cache */
static void sh3_cache_invalidate_penalty(void)
{
	memset(sh3.cache_tag, 0, sizeof(sh3.cache_tag));
	memset(sh3.cache_lru, 0, sizeof(sh3.cache_lru));
	memset(sh3.cache_dirty, 0, sizeof(sh3.cache_dirty));
	cache_mru_rebuild();
	if (sh3.cache_timing)
	{
		/* the uncached fetches of the flush loop, as current MAME */
		sh3_ICount -= (2 * 2 * 1024) * (1 + MCR_RCD() + get_wcr2_timing(sh3.pc) + MCR_TPC());
	}
}

static UINT32 cache_address_array_r(UINT32 offset)
{
	UINT32 cache_entry_index = offset / 4;
	UINT32 entry_block = cache_entry_index % CACHE_BLOCKS;
	UINT32 way = (cache_entry_index / CACHE_BLOCKS) & (CACHE_ASSOC - 1);
	int v = sh3.cache_tag[entry_block][way] != 0;
	int u = sh3.cache_dirty[entry_block][way];
	return (sh3.cache_tag[entry_block][way] << 10) | (u << 1) | v;
}

static void cache_address_array_w(UINT32 offset, UINT32 data)
{
	UINT32 cache_entry_index = offset / 4;

	/* only the U bit: 0 writes the line back */
	if (!(data & 2))
	{
		UINT32 entry_block = cache_entry_index % CACHE_BLOCKS;
		UINT32 way = (cache_entry_index / CACHE_BLOCKS) & (CACHE_ASSOC - 1);
		if (sh3.cache_dirty[entry_block][way])
		{
			UINT32 wb_address = sh3.cache_tag[entry_block][way] * CACHE_LINE_SIZE;
			sh3.cache_dirty[entry_block][way] = 0;
			if (sh3.cache_timing)
				sh3_ICount -= (2 + MCR_RCD() + 4 + MCR_TRWL() + MCR_TPC() + get_wcr1_timing(3)) * 2;
			sh3.last_area_accessed = get_area(wb_address);
		}
	}
}


/***************************************************************************
	timer unit (sh4tmu.cpp)
***************************************************************************/

static const int tcnt_div[8] = { 4, 16, 64, 256, 1024, 1, 1, 1 };

/* a count from the time left; -1 (an expired timer) gives 0xffffffff on
   every host, as the conversion does on x86-64 (a double that doesn't fit
   an unsigned has no defined conversion in C) */
static UINT32 ticks_left(int which, int tcr)
{
	double ret = ((timer_timeleft(sh3.timer[which]) * (double)sh3.pm_clock) / (double)tcnt_div[tcr & 7]) - 1;
	return (UINT32)(INT64)ret;
}

static UINT32 compute_ticks_timer(int which)
{
	return ticks_left(which, sh3.tcr[which]);
}

static void timer_recompute(int which)
{
	double period = TIME_IN_HZ(sh3.pm_clock) * tcnt_div[sh3.tcr[which] & 7];
	/* one multiply: a product and a sum would become a fused multiply-add on
	   some hosts (AArch64, PowerPC) and round differently from x86 */
	timer_adjust(sh3.timer[which], period * ((double)sh3.tcnt[which] + 1.0), which, 0);
}

/* the timers run on the machine time, as in current MAME: no CPU context
   here (with one, timer_adjust would start from the CPU's local time, which
   stays behind while the CPU is suspended by a spin: the timer would then
   fire again at once, over and over) */
static void sh3_timer_callback(int which)
{

	sh3.tcnt[which] = sh3.tcor[which];
	timer_recompute(which);
	sh3.tcr[which] |= 0x100;
	if (sh3.tcr[which] & 0x20)
		exception_request(INTC_TUNI0 + which);
}

static void tstr_w(UINT8 data)
{
	UINT32 chg = sh3.tstr ^ data;
	int which;

	sh3.tstr = data;
	for (which = 0; which < 3; which++)
	{
		if (chg & (1 << which))
		{
			if ((sh3.tstr & (1 << which)) == 0)
			{
				sh3.tcnt[which] = compute_ticks_timer(which);
				timer_adjust(sh3.timer[which], TIME_NEVER, which, 0);
			}
			else
				timer_recompute(which);
		}
	}
}

static UINT32 tcnt_r(int which)
{
	if (sh3.tstr & (1 << which))
		return compute_ticks_timer(which);
	return sh3.tcnt[which];
}

static void tcnt_w(int which, UINT32 data)
{
	sh3.tcnt[which] = data;
	if (sh3.tstr & (1 << which))
		timer_recompute(which);
}

static void tcr_w(int which, UINT16 data)
{
	UINT32 old = sh3.tcr[which];

	sh3.tcr[which] = data;
	if (sh3.tstr & (1 << which))
	{
		sh3.tcnt[which] = ticks_left(which, old);
		timer_recompute(which);
	}
	if (!(sh3.tcr[which] & 0x20) || !(sh3.tcr[which] & 0x100))
		exception_unrequest(INTC_TUNI0 + which);
}



/***************************************************************************
	DMA controller (sh4dmac.cpp): the transfer is done at once, the end
	comes after 2 cycles per unit
***************************************************************************/

#define CHCR_DM		0x0000c000
#define CHCR_SM		0x00003000
#define CHCR_RS		0x00000f00
#define CHCR_IE		0x00000004
#define CHCR_TE		0x00000002
#define DMAOR_DDT	0x8000
#define DMAOR_AE	0x0004
#define DMAOR_NMIF	0x0002
#define DMAOR_DME	0x0001

#define REG_SAR(ch)		(0x04000020 + (ch) * 0x10)
#define REG_DAR(ch)		(0x04000024 + (ch) * 0x10)
#define REG_DMATCR(ch)	(0x04000028 + (ch) * 0x10)
#define REG_CHCR(ch)	(0x0400002c + (ch) * 0x10)
#define REG_DMAOR		0x04000060

static INLINE sh3_fastram *find_fastram(UINT32 a);
static INLINE UINT8 read_byte(UINT32 a);
static INLINE UINT16 read_word(UINT32 a);
static INLINE UINT32 read_long(UINT32 a);
static INLINE void write_byte(UINT32 a, UINT8 d);
static INLINE void write_word(UINT32 a, UINT16 d);
static INLINE void write_long(UINT32 a, UINT32 d);

static void sh3_dma_callback(int channel)
{

	sh3.dma_timer_active[channel] = 0;
	set_reg32(REG_DMATCR(channel), 0);
	set_reg32(REG_CHCR(channel), reg32(REG_CHCR(channel)) | CHCR_TE);
	if (reg32(REG_CHCR(channel)) & CHCR_IE)
		exception_request(INTC_DMTE0 + channel);
}

static void dma_transfer(int channel, UINT32 chcr, UINT32 src, UINT32 dst, UINT32 count)
{
	static const int sh3_dmasize[4] = { 1, 2, 4, 16 };
	int incd = (chcr & CHCR_DM) >> 14;
	int incs = (chcr & CHCR_SM) >> 12;
	int size = sh3_dmasize[(chcr >> 3) & 3];

	if (incd == 3 || incs == 3)
		return;
	if (!count)
		count = 0x1000000;

	sh3.dma_timer_active[channel] = 1;
	timer_adjust(sh3.dma_timer[channel], TIME_IN_CYCLES(2 * count + 1, sh3.cpunum), channel, 0);

	src &= AM;
	dst &= AM;

	/* MAME doesn't do the 16-byte transfers on the SH-3 */
	switch (size)
	{
		case 1:
			/* to fast RAM, forward (the NAND page reads): the same stores
			   without looking for the region of each byte */
			if (incd == 1 && incs != 2)
			{
				sh3_fastram *fd = find_fastram(dst);
				if (fd && !fd->readonly && dst + (count - 1) >= dst && dst + (count - 1) <= fd->end)
				{
					UINT8 *base = fd->base;
					UINT32 start = fd->start;
					for (; count > 0; count--)
					{
						base[BYTE4_XOR_BE(dst - start)] = read_byte(src & AM);
						jit_check_write(dst);
						if (incs == 1) src++;
						dst++;
					}
					break;
				}
			}
			for (; count > 0; count--)
			{
				if (incs == 2) src--;
				if (incd == 2) dst--;
				write_byte(dst & AM, read_byte(src & AM));
				if (incs == 1) src++;
				if (incd == 1) dst++;
			}
			break;
		case 2:
			src &= ~1;
			dst &= ~1;
			for (; count > 0; count--)
			{
				if (incs == 2) src -= 2;
				if (incd == 2) dst -= 2;
				write_word(dst & AM, read_word(src & AM));
				if (incs == 1) src += 2;
				if (incd == 1) dst += 2;
			}
			break;
		case 4:
			src &= ~3;
			dst &= ~3;
			for (; count > 0; count--)
			{
				if (incs == 2) src -= 4;
				if (incd == 2) dst -= 4;
				write_long(dst & AM, read_long(src & AM));
				if (incs == 1) src += 4;
				if (incd == 1) dst += 4;
			}
			break;
	}
	/* MAME works on copies: SAR, DAR and DMATCR keep their values until
	   the end clears DMATCR */
}

static void dmac_check(int channel)
{
	UINT32 chcr = reg32(REG_CHCR(channel));
	UINT32 dmaor = reg16(REG_DMAOR);

	if (chcr & dmaor & DMAOR_DME)
	{
		int rs = (chcr & CHCR_RS) >> 8;
		if (rs < 2 || rs > 6)
			return;
		if (!sh3.dma_timer_active[channel] && !(chcr & CHCR_TE) && !(dmaor & (DMAOR_AE | DMAOR_NMIF)))
		{
			if (rs > 3)
				dma_transfer(channel, chcr, reg32(REG_SAR(channel)), reg32(REG_DAR(channel)), reg32(REG_DMATCR(channel)));
			else if ((dmaor & DMAOR_DDT) == 0)
				log_cb(RETRO_LOG_ERROR, LOGPRE "SH-3: DMA %d from a device, not done\n", channel);
		}
	}
	else if (sh3.dma_timer_active[channel])
	{
		timer_adjust(sh3.dma_timer[channel], TIME_NEVER, channel, 0);
		sh3.dma_timer_active[channel] = 0;
	}
}


/***************************************************************************
	internal register accesses
***************************************************************************/

/* the registers with a behaviour, at the width current MAME maps them;
   the other registers only keep what is written. An access is split over
   the registers it covers, a part of a register is merged into it, as the
   memory system of MAME does */
enum
{
	SPR_CCR, SPR_WTCNT, SPR_WTCSR, SPR_IPRA, SPR_IPRC, SPR_IRR0, SPR_SCSSR,
	SPR_TSTR, SPR_TCOR0, SPR_TCNT0, SPR_TCR0, SPR_TCOR1, SPR_TCNT1, SPR_TCR1,
	SPR_TCOR2, SPR_TCNT2, SPR_TCR2, SPR_CHCR0, SPR_CHCR1, SPR_CHCR2, SPR_CHCR3, SPR_DMAOR,
	SPR_PORT_A,
	SPR_COUNT = SPR_PORT_A + SH3_PORT_COUNT
};

static const struct { UINT32 addr; int size; } specials[SPR_COUNT] =
{
	{ 0xffffffec, 4 }, { 0xffffff84, 2 }, { 0xffffff86, 2 }, { 0xfffffee2, 2 }, { 0x04000016, 2 },
	{ 0x04000004, 1 }, { 0xfffffe88, 1 }, { 0xfffffe92, 1 },
	{ 0xfffffe94, 4 }, { 0xfffffe98, 4 }, { 0xfffffe9c, 2 },
	{ 0xfffffea0, 4 }, { 0xfffffea4, 4 }, { 0xfffffea8, 2 },
	{ 0xfffffeac, 4 }, { 0xfffffeb0, 4 }, { 0xfffffeb4, 2 },
	{ 0x0400002c, 4 }, { 0x0400003c, 4 }, { 0x0400004c, 4 }, { 0x0400005c, 4 }, { 0x04000060, 2 },
	{ 0x04000120, 1 }, { 0x04000122, 1 }, { 0x04000124, 1 }, { 0x04000126, 1 }, { 0x04000128, 1 },
	{ 0x0400012a, 1 }, { 0x0400012c, 1 }, { 0x0400012e, 1 }, { 0x04000130, 1 }, { 0x04000132, 1 },
	{ 0x04000134, 1 }
};

/* the special register at each byte of the register arrays, -1 for none */
static signed char special_p4[0x180], special_a1[0x300];

static void build_special_tables(void)
{
	UINT32 a;
	int i;
	memset(special_p4, -1, sizeof(special_p4));
	memset(special_a1, -1, sizeof(special_a1));
	for (i = 0; i < SPR_COUNT; i++)
		for (a = specials[i].addr; a < specials[i].addr + specials[i].size; a++)
		{
			if (a >= 0xfffffe80)
				special_p4[a - 0xfffffe80] = i;
			else
				special_a1[a - 0x04000000] = i;
		}
}

static INLINE int find_special(UINT32 a)
{
	if (a >= 0xfffffe80)
		return special_p4[a - 0xfffffe80];
	if (a >= 0x04000000 && a < 0x04000300)
		return special_a1[a - 0x04000000];
	return -1;
}

static UINT32 special_read(int r)
{
	switch (r)
	{
		case SPR_TCNT0: return tcnt_r(0);
		case SPR_TCNT1: return tcnt_r(1);
		case SPR_TCNT2: return tcnt_r(2);
		case SPR_TCOR0: return sh3.tcor[0];
		case SPR_TCOR1: return sh3.tcor[1];
		case SPR_TCOR2: return sh3.tcor[2];
		case SPR_TCR0: return sh3.tcr[0] & 0xffff;
		case SPR_TCR1: return sh3.tcr[1] & 0xffff;
		case SPR_TCR2: return sh3.tcr[2] & 0xffff;
		case SPR_TSTR: return sh3.tstr;
		case SPR_WTCNT: return sh3.regp4[REG_WTCNT - 0xfffffe80] << 8;	/* 8-bit, the other byte unmapped */
		case SPR_WTCSR: return sh3.regp4[REG_WTCSR - 0xfffffe80] << 8;
	}
	if (r >= SPR_PORT_A)
		return sh3.port_r ? (*sh3.port_r)(r - SPR_PORT_A) : 0;
	if (specials[r].size == 4)
		return reg32(specials[r].addr);
	if (specials[r].size == 2)
		return reg16(specials[r].addr);
	return *reg_ptr(specials[r].addr);
}

static void special_write(int r, UINT32 data)
{
	switch (r)
	{
		case SPR_CCR:
			if (data & 0x8)
				sh3_cache_invalidate_penalty();
			set_reg32(REG_CCR, (reg32(REG_CCR) & 0x8) | (data & ~0x8));
			return;

		case SPR_WTCNT:
			if ((data & 0xff00) == 0x5a00)
				sh3.regp4[REG_WTCNT - 0xfffffe80] = data;
			return;

		case SPR_WTCSR:
			if ((data & 0xff00) == 0xa500)
				sh3.regp4[REG_WTCSR - 0xfffffe80] = data;
			return;

		case SPR_IPRA:
		{
			UINT16 ipra = data;
			set_reg16(REG_IPRA, ipra);
			sh3.exception_priority[INTC_ATI] = INTPRI(ipra & 0x000f, INTC_ATI);
			sh3.exception_priority[INTC_PRI] = INTPRI(ipra & 0x000f, INTC_PRI);
			sh3.exception_priority[INTC_CUI] = INTPRI(ipra & 0x000f, INTC_CUI);
			sh3.exception_priority[INTC_TUNI2] = INTPRI((ipra & 0x00f0) >> 4, INTC_TUNI2);
			sh3.exception_priority[INTC_TICPI2] = INTPRI((ipra & 0x00f0) >> 4, INTC_TICPI2);
			sh3.exception_priority[INTC_TUNI1] = INTPRI((ipra & 0x0f00) >> 8, INTC_TUNI1);
			sh3.exception_priority[INTC_TUNI0] = INTPRI((ipra & 0xf000) >> 12, INTC_TUNI0);
			exception_recompute();
			return;
		}

		case SPR_IPRC:
		{
			UINT16 iprc = data;
			set_reg16(REG_IPRC, iprc);
			sh3.exception_priority[INTC_IRL0] = INTPRI((iprc & 0x000f) >> 0, INTC_IRL0);
			sh3.exception_priority[INTC_IRL1] = INTPRI((iprc & 0x00f0) >> 4, INTC_IRL1);
			sh3.exception_priority[INTC_IRL2] = INTPRI((iprc & 0x0f00) >> 8, INTC_IRL2);
			sh3.exception_priority[INTC_IRL3] = INTPRI((iprc & 0xf000) >> 12, INTC_IRL3);
			exception_recompute();
			return;
		}

		case SPR_IRR0:
			sh3.rega1[REG_IRR0 - 0x04000000] = data;
			if (!(data & 0x01)) sh3_set_irq_line(0, CLEAR_LINE);
			if (!(data & 0x02)) sh3_set_irq_line(1, CLEAR_LINE);
			if (!(data & 0x04)) sh3_set_irq_line(2, CLEAR_LINE);
			if (!(data & 0x08)) sh3_set_irq_line(3, CLEAR_LINE);
			return;

		case SPR_SCSSR:
		{
			UINT8 st = sh3.regp4[REG_SCSSR - 0xfffffe80];
			st = (st | (data & 1)) & (data | 6);
			if (!(st & 0x80))
				st |= 0x80;
			sh3.regp4[REG_SCSSR - 0xfffffe80] = st;
			return;
		}

		case SPR_TSTR: tstr_w(data); return;
		case SPR_TCOR0: sh3.tcor[0] = data; return;
		case SPR_TCOR1: sh3.tcor[1] = data; return;
		case SPR_TCOR2: sh3.tcor[2] = data; return;
		case SPR_TCNT0: tcnt_w(0, data); return;
		case SPR_TCNT1: tcnt_w(1, data); return;
		case SPR_TCNT2: tcnt_w(2, data); return;
		case SPR_TCR0: tcr_w(0, data); return;
		case SPR_TCR1: tcr_w(1, data); return;
		case SPR_TCR2: tcr_w(2, data); return;

		case SPR_CHCR0: case SPR_CHCR1: case SPR_CHCR2: case SPR_CHCR3:
			set_reg32(specials[r].addr, data);
			dmac_check(r - SPR_CHCR0);
			return;

		case SPR_DMAOR:
		{
			UINT16 old = reg16(REG_DMAOR);
			UINT16 d = data;
			if ((d & DMAOR_AE) && (~old & DMAOR_AE))
				d &= ~DMAOR_AE;
			if ((d & DMAOR_NMIF) && (~old & DMAOR_NMIF))
				d &= ~DMAOR_NMIF;
			set_reg16(REG_DMAOR, d);
			dmac_check(0);
			dmac_check(1);
			dmac_check(2);
			dmac_check(3);
			return;
		}
	}

	if (r >= SPR_PORT_A)
	{
		sh3.rega1[specials[r].addr - 0x04000000] = data;
		if (sh3.port_w)
			(*sh3.port_w)(r - SPR_PORT_A, data);
	}
}

/* the value of a special register as stored, to merge a narrower write
   into (COMBINE_DATA of MAME) */
static UINT32 special_stored(int r)
{
	switch (r)
	{
		case SPR_TCNT0: return sh3.tcnt[0];
		case SPR_TCNT1: return sh3.tcnt[1];
		case SPR_TCNT2: return sh3.tcnt[2];
		case SPR_WTCNT: return sh3.regp4[REG_WTCNT - 0xfffffe80];
		case SPR_WTCSR: return sh3.regp4[REG_WTCSR - 0xfffffe80];
	}
	if (r >= SPR_PORT_A)
		return sh3.rega1[specials[r].addr - 0x04000000];
	return special_read(r);
}

static UINT32 internal_read(UINT32 a, int size)
{
	UINT32 result = 0;
	int i;

	/* cache address array */
	if (a >= 0xf0000000 && a <= 0xf0ffffff)
	{
		UINT32 d = cache_address_array_r((a & 0x00ffffff) & ~3);
		if (size == 4) return d;
		if (size == 2) return (d >> ((~a & 2) * 8)) & 0xffff;
		return (d >> ((~a & 3) * 8)) & 0xff;
	}

	if (!reg_ptr(a) || !reg_ptr(a + size - 1))
	{
		/* outside the registers (SDMR, unmapped) */
		if (a >= 0xe0000000)
			return 0;
		if (size == 1) return cpu_readmem32bedw(a);
		if (size == 2) return cpu_readmem32bedw_word(a);
		return cpu_readmem32bedw_dword(a);
	}

	for (i = 0; i < size; )
	{
		UINT32 b = a + i;
		int r = find_special(b);
		if (r >= 0)
		{
			UINT32 v = special_read(r);
			int rs = specials[r].size;
			int k;
			/* the bytes of the register inside this access */
			for (k = b - specials[r].addr; k < rs && i < size; k++, i++)
				result = (result << 8) | ((v >> ((rs - 1 - k) * 8)) & 0xff);
		}
		else
		{
			result = (result << 8) | *reg_ptr(b);
			i++;
		}
	}
	return result;
}

static void internal_write(UINT32 a, UINT32 data, int size)
{
	int i;

	/* cache address array */
	if (a >= 0xf0000000 && a <= 0xf0ffffff)
	{
		if (size == 4)
			cache_address_array_w((a & 0x00ffffff) & ~3, data);
		else if (size == 2)
			cache_address_array_w((a & 0x00ffffff) & ~3, data << ((~a & 2) * 8));
		else
			cache_address_array_w((a & 0x00ffffff) & ~3, data << ((~a & 3) * 8));
		return;
	}

	if (!reg_ptr(a) || !reg_ptr(a + size - 1))
	{
		if (a >= 0xe0000000)
			return;
		if (size == 1) cpu_writemem32bedw(a, data);
		else if (size == 2) cpu_writemem32bedw_word(a, data);
		else cpu_writemem32bedw_dword(a, data);
		return;
	}

	for (i = 0; i < size; )
	{
		UINT32 b = a + i;
		int r = find_special(b);
		if (r >= 0)
		{
			int rs = specials[r].size;
			UINT32 v = (rs == size && b == specials[r].addr) ? data : special_stored(r);
			int k;
			if (!(rs == size && b == specials[r].addr))
			{
				for (k = b - specials[r].addr; k < rs && i < size; k++, i++)
				{
					UINT32 byte = (data >> ((size - 1 - i) * 8)) & 0xff;
					UINT32 shift = (rs - 1 - k) * 8;
					v = (v & ~(0xff << shift)) | (byte << shift);
				}
			}
			else
				i += rs;
			special_write(r, v);
		}
		else
		{
			*reg_ptr(b) = (data >> ((size - 1 - i) * 8)) & 0xff;
			if (b >= 0xffffff60 && b < 0xffffff70)
				bsc_valid = 0;
			i++;
		}
	}
}


/***************************************************************************
	memory accesses
***************************************************************************/

static void fastram_build_pages(void)
{
	UINT32 p;
	int i;
	for (p = 0; p < sizeof(fastram_page); p++)
	{
		UINT32 first = p << FASTRAM_PAGE_SHIFT, last = first + (1 << FASTRAM_PAGE_SHIFT) - 1;
		INT8 v = -1;
		for (i = 0; i < fastram_count; i++)
		{
			if (fastram[i].end < first || fastram[i].start > last)
				continue;
			if (v == -1 && fastram[i].start <= first && fastram[i].end >= last)
				v = (INT8)i;
			else
				v = -2;
		}
		fastram_page[p] = v;
	}
}

void sh3_set_fastram(UINT32 start, UINT32 end, int readonly, data32_t *base)
{
	if (fastram_count < SH3_FASTRAM_MAX)
	{
		fastram[fastram_count].start = start;
		fastram[fastram_count].end = end;
		fastram[fastram_count].readonly = readonly;
		fastram[fastram_count].base = (UINT8 *)base;
		fastram_count++;
	}
	fastram_build_pages();
}

/* the regions don't overlap: the one of the previous access first */
static int fastram_last;

static INLINE sh3_fastram *find_fastram(UINT32 a)
{
	int i;
	if (a <= AM)
	{
		i = fastram_page[a >> FASTRAM_PAGE_SHIFT];
		if (i >= 0)
			return &fastram[i];
		if (i == -1)
			return NULL;
	}
	if (fastram_last < fastram_count && a >= fastram[fastram_last].start && a <= fastram[fastram_last].end)
		return &fastram[fastram_last];
	for (i = 0; i < fastram_count; i++)
		if (a >= fastram[i].start && a <= fastram[i].end)
		{
			fastram_last = i;
			return &fastram[i];
		}
	return NULL;
}

/* without the bus timing */
static INLINE UINT8 read_byte(UINT32 a)
{
	sh3_fastram *f;
	if (a >= 0xe0000000)
		return internal_read(a, 1);
	a &= AM;
	f = find_fastram(a);
	if (f)
		return f->base[BYTE4_XOR_BE(a - f->start)];
	if ((a >> 24) == 0x04)
		return internal_read(a, 1);
	return cpu_readmem32bedw(a);
}

static INLINE UINT16 read_word(UINT32 a)
{
	sh3_fastram *f;
	a &= ~1;
	if (a >= 0xe0000000)
		return internal_read(a, 2);
	a &= AM;
	f = find_fastram(a);
	if (f)
		return *(UINT16 *)&f->base[WORD_XOR_BE(a - f->start)];
	if ((a >> 24) == 0x04)
		return internal_read(a, 2);
	return cpu_readmem32bedw_word(a);
}

static INLINE UINT32 read_long(UINT32 a)
{
	sh3_fastram *f;
	a &= ~3;
	if (a >= 0xe0000000)
		return internal_read(a, 4);
	a &= AM;
	f = find_fastram(a);
	if (f)
		return *(UINT32 *)&f->base[a - f->start];
	if ((a >> 24) == 0x04)
		return internal_read(a, 4);
	return cpu_readmem32bedw_dword(a);
}

static INLINE void write_byte(UINT32 a, UINT8 d)
{
	sh3_fastram *f;
	if (a >= 0xe0000000)
	{
		internal_write(a, d, 1);
		return;
	}
	a &= AM;
	f = find_fastram(a);
	if (f)
	{
		if (!f->readonly)
		{
			f->base[BYTE4_XOR_BE(a - f->start)] = d;
			jit_check_write(a);
		}
		return;
	}
	if ((a >> 24) == 0x04)
		internal_write(a, d, 1);
	else
		cpu_writemem32bedw(a, d);
}

static INLINE void write_word(UINT32 a, UINT16 d)
{
	sh3_fastram *f;
	a &= ~1;
	if (a >= 0xe0000000)
	{
		internal_write(a, d, 2);
		return;
	}
	a &= AM;
	f = find_fastram(a);
	if (f)
	{
		if (!f->readonly)
		{
			*(UINT16 *)&f->base[WORD_XOR_BE(a - f->start)] = d;
			jit_check_write(a);
		}
		return;
	}
	if ((a >> 24) == 0x04)
		internal_write(a, d, 2);
	else
		cpu_writemem32bedw_word(a, d);
}

static INLINE void write_long(UINT32 a, UINT32 d)
{
	sh3_fastram *f;
	a &= ~3;
	if (a >= 0xe0000000)
	{
		internal_write(a, d, 4);
		return;
	}
	a &= AM;
	f = find_fastram(a);
	if (f)
	{
		if (!f->readonly)
		{
			*(UINT32 *)&f->base[a - f->start] = d;
			jit_check_write(a);
		}
		return;
	}
	if ((a >> 24) == 0x04)
		internal_write(a, d, 4);
	else
		cpu_writemem32bedw_dword(a, d);
}

/* with the bus timing (the accessors of MAME's recompiler) */
static INLINE UINT8 RB(UINT32 a)
{
	UINT8 d = read_byte(a);
	data_access_timing(a, 0);
	return d;
}

static INLINE UINT16 RW(UINT32 a)
{
	UINT16 d = read_word(a);
	data_access_timing(a, 0);
	return d;
}

static INLINE UINT32 RL(UINT32 a)
{
	UINT32 d = read_long(a);
	data_access_timing(a, 0);
	return d;
}

static INLINE void WB(UINT32 a, UINT8 d)
{
	write_byte(a, d);
	data_access_timing(a, 1);
}

static INLINE void WW(UINT32 a, UINT16 d)
{
	write_word(a, d);
	data_access_timing(a, 1);
}

static INLINE void WL(UINT32 a, UINT32 d)
{
	write_long(a, d);
	data_access_timing(a, 1);
}

/* opcode fetch */
static sh3_fastram *fetch_region;

static INLINE UINT16 fetch(UINT32 pc)
{
	UINT32 a = pc & AM;
	if (!fetch_region || a < fetch_region->start || a > fetch_region->end)
		fetch_region = find_fastram(a);
	if (fetch_region)
		return *(UINT16 *)&fetch_region->base[WORD_XOR_BE(a - fetch_region->start)];
	return read_word(pc);
}


/***************************************************************************
	instructions (sh.cpp, sh4.cpp)
***************************************************************************/

#define SEXT8(x)	((INT32)(INT8)(x))
#define SEXT12(x)	(((INT32)((x) << 20)) >> 20)
#define SEXT16(x)	((INT32)(INT16)(x))

/* the delay slot of a branch */
static UINT32 sh3_delay;

static INLINE void ADD(UINT32 m, UINT32 n) { sh3.r[n] += sh3.r[m]; }
static INLINE void ADDI(UINT32 i, UINT32 n) { sh3.r[n] += SEXT8(i); }

static INLINE void ADDC(UINT32 m, UINT32 n)
{
	UINT32 tmp0, tmp1;
	tmp1 = sh3.r[n] + sh3.r[m];
	tmp0 = sh3.r[n];
	sh3.r[n] = tmp1 + (sh3.sr & SR_T);
	if (tmp0 > tmp1)
		sh3.sr |= SR_T;
	else
		sh3.sr &= ~SR_T;
	if (tmp1 > sh3.r[n])
		sh3.sr |= SR_T;
}

static INLINE void ADDV(UINT32 m, UINT32 n)
{
	INT32 dest = (sh3.r[n] >> 31) & 1;
	INT32 src = ((sh3.r[m] >> 31) & 1) + dest;
	INT32 ans;
	sh3.r[n] += sh3.r[m];
	ans = ((sh3.r[n] >> 31) & 1) + dest;
	if (src != 1)
	{
		if (ans == 1)
			sh3.sr |= SR_T;
		else
			sh3.sr &= ~SR_T;
	}
	else
		sh3.sr &= ~SR_T;
}

static INLINE void AND(UINT32 m, UINT32 n) { sh3.r[n] &= sh3.r[m]; }
static INLINE void ANDI(UINT32 i) { sh3.r[0] &= i; }

static INLINE void ANDM(UINT32 i)
{
	UINT32 temp;
	sh3.ea = sh3.gbr + sh3.r[0];
	temp = i & RB(sh3.ea);
	WB(sh3.ea, temp);
	sh3_ICount -= 2;
}

static INLINE void BF(UINT32 d)
{
	if ((sh3.sr & SR_T) == 0)
	{
		sh3.pc = sh3.ea = sh3.pc + SEXT8(d) * 2 + 2;
		sh3_ICount -= 2;
	}
}

static INLINE void BFS(UINT32 d)
{
	if ((sh3.sr & SR_T) == 0)
	{
		sh3_delay = sh3.ea = sh3.pc + SEXT8(d) * 2 + 2;
		sh3_ICount--;
	}
}

static INLINE void BRA(UINT32 d)
{
	sh3_delay = sh3.ea = sh3.pc + SEXT12(d) * 2 + 2;
	sh3_ICount--;
}

static INLINE void BRAF(UINT32 m)
{
	sh3_delay = sh3.pc + sh3.r[m] + 2;
	sh3_ICount--;
}

static INLINE void BSR(UINT32 d)
{
	sh3.pr = sh3.pc + 2;
	sh3_delay = sh3.ea = sh3.pc + SEXT12(d) * 2 + 2;
	sh3_ICount--;
}

static INLINE void BSRF(UINT32 m)
{
	sh3.pr = sh3.pc + 2;
	sh3_delay = sh3.pc + sh3.r[m] + 2;
	sh3_ICount--;
}

static INLINE void BT(UINT32 d)
{
	if ((sh3.sr & SR_T) != 0)
	{
		sh3.pc = sh3.ea = sh3.pc + SEXT8(d) * 2 + 2;
		sh3_ICount -= 2;
	}
}

static INLINE void BTS(UINT32 d)
{
	if ((sh3.sr & SR_T) != 0)
	{
		sh3_delay = sh3.ea = sh3.pc + SEXT8(d) * 2 + 2;
		sh3_ICount--;
	}
}

static INLINE void CLRMAC(void) { sh3.mach = 0; sh3.macl = 0; }
static INLINE void CLRT(void) { sh3.sr &= ~SR_T; }

#define SETT_IF(cond)	do { if (cond) sh3.sr |= SR_T; else sh3.sr &= ~SR_T; } while (0)

static INLINE void CMPEQ(UINT32 m, UINT32 n) { SETT_IF(sh3.r[n] == sh3.r[m]); }
static INLINE void CMPGE(UINT32 m, UINT32 n) { SETT_IF((INT32)sh3.r[n] >= (INT32)sh3.r[m]); }
static INLINE void CMPGT(UINT32 m, UINT32 n) { SETT_IF((INT32)sh3.r[n] > (INT32)sh3.r[m]); }
static INLINE void CMPHI(UINT32 m, UINT32 n) { SETT_IF(sh3.r[n] > sh3.r[m]); }
static INLINE void CMPHS(UINT32 m, UINT32 n) { SETT_IF(sh3.r[n] >= sh3.r[m]); }
static INLINE void CMPPL(UINT32 n) { SETT_IF((INT32)sh3.r[n] > 0); }
static INLINE void CMPPZ(UINT32 n) { SETT_IF((INT32)sh3.r[n] >= 0); }

static INLINE void CMPSTR(UINT32 m, UINT32 n)
{
	UINT32 temp = sh3.r[n] ^ sh3.r[m];
	if ((temp >> 24) & 0xff && (temp >> 16) & 0xff && (temp >> 8) & 0xff && temp & 0xff)
		sh3.sr &= ~SR_T;
	else
		sh3.sr |= SR_T;
}

static INLINE void CMPIM(UINT32 i) { SETT_IF(sh3.r[0] == (UINT32)SEXT8(i)); }

static INLINE void DIV0S(UINT32 m, UINT32 n)
{
	if (!(sh3.r[n] & 0x80000000))
		sh3.sr &= ~SR_Q;
	else
		sh3.sr |= SR_Q;
	if (!(sh3.r[m] & 0x80000000))
		sh3.sr &= ~SR_M;
	else
		sh3.sr |= SR_M;
	SETT_IF((sh3.r[m] ^ sh3.r[n]) & 0x80000000);
}

static INLINE void DIV0U(void) { sh3.sr &= ~(SR_M | SR_Q | SR_T); }

static INLINE void DIV1(UINT32 m, UINT32 n)
{
	UINT32 tmp;
	UINT32 old_q = sh3.sr & SR_Q;

	if (0x80000000 & sh3.r[n])
		sh3.sr |= SR_Q;
	else
		sh3.sr &= ~SR_Q;

	sh3.r[n] = (sh3.r[n] << 1) | (sh3.sr & SR_T);

	if (!old_q)
	{
		if (!(sh3.sr & SR_M))
		{
			tmp = sh3.r[n];
			sh3.r[n] -= sh3.r[m];
			if (!(sh3.sr & SR_Q))
			{
				if (sh3.r[n] > tmp) sh3.sr |= SR_Q; else sh3.sr &= ~SR_Q;
			}
			else
			{
				if (sh3.r[n] > tmp) sh3.sr &= ~SR_Q; else sh3.sr |= SR_Q;
			}
		}
		else
		{
			tmp = sh3.r[n];
			sh3.r[n] += sh3.r[m];
			if (!(sh3.sr & SR_Q))
			{
				if (sh3.r[n] < tmp) sh3.sr &= ~SR_Q; else sh3.sr |= SR_Q;
			}
			else
			{
				if (sh3.r[n] < tmp) sh3.sr |= SR_Q; else sh3.sr &= ~SR_Q;
			}
		}
	}
	else
	{
		if (!(sh3.sr & SR_M))
		{
			tmp = sh3.r[n];
			sh3.r[n] += sh3.r[m];
			if (!(sh3.sr & SR_Q))
			{
				if (sh3.r[n] < tmp) sh3.sr |= SR_Q; else sh3.sr &= ~SR_Q;
			}
			else
			{
				if (sh3.r[n] < tmp) sh3.sr &= ~SR_Q; else sh3.sr |= SR_Q;
			}
		}
		else
		{
			tmp = sh3.r[n];
			sh3.r[n] -= sh3.r[m];
			if (!(sh3.sr & SR_Q))
			{
				if (sh3.r[n] > tmp) sh3.sr &= ~SR_Q; else sh3.sr |= SR_Q;
			}
			else
			{
				if (sh3.r[n] > tmp) sh3.sr |= SR_Q; else sh3.sr &= ~SR_Q;
			}
		}
	}

	tmp = sh3.sr & (SR_Q | SR_M);
	SETT_IF(tmp == 0 || tmp == 0x300);
}

/* the 32 x 32 -> 64 multiply of DMULS/DMULU/MAC.L, as MAME does it */
static void mul64(UINT32 tempn, UINT32 tempm, UINT32 *hi, UINT32 *lo)
{
	UINT32 rn_l = tempn & 0x0000ffff;
	UINT32 rn_h = tempn >> 16;
	UINT32 rm_l = tempm & 0x0000ffff;
	UINT32 rm_h = tempm >> 16;
	UINT32 temp0 = rm_l * rn_l;
	UINT32 temp1 = rm_h * rn_l;
	UINT32 temp2 = rm_l * rn_h;
	UINT32 temp3 = rm_h * rn_h;
	UINT32 res2 = 0, res1, res0;

	res1 = temp1 + temp2;
	if (res1 < temp1)
		res2 += 0x00010000;
	temp1 = res1 << 16;
	res0 = temp0 + temp1;
	if (res0 < temp0)
		res2++;
	res2 = res2 + (res1 >> 16) + temp3;
	*hi = res2;
	*lo = res0;
}

static INLINE void DMULS(UINT32 m, UINT32 n)
{
	INT32 tempn = (INT32)sh3.r[n];
	INT32 tempm = (INT32)sh3.r[m];
	int fnlml = ((tempn ^ tempm) >> 31) & 1;
	UINT32 res2, res0;

	if (tempn < 0) tempn = 0 - tempn;
	if (tempm < 0) tempm = 0 - tempm;
	mul64((UINT32)tempn, (UINT32)tempm, &res2, &res0);
	if (fnlml)
	{
		res2 = ~res2;
		if (res0 == 0)
			res2++;
		else
			res0 = (~res0) + 1;
	}
	sh3.mach = res2;
	sh3.macl = res0;
	sh3_ICount--;
}

static INLINE void DMULU(UINT32 m, UINT32 n)
{
	mul64(sh3.r[n], sh3.r[m], &sh3.mach, &sh3.macl);
	sh3_ICount--;
}

static INLINE void DT(UINT32 n)
{
	sh3.r[n]--;
	SETT_IF(sh3.r[n] == 0);
}

static INLINE void EXTSB(UINT32 m, UINT32 n) { sh3.r[n] = SEXT8(sh3.r[m]); }
static INLINE void EXTSW(UINT32 m, UINT32 n) { sh3.r[n] = SEXT16(sh3.r[m]); }
static INLINE void EXTUB(UINT32 m, UINT32 n) { sh3.r[n] = sh3.r[m] & 0x000000ff; }
static INLINE void EXTUW(UINT32 m, UINT32 n) { sh3.r[n] = sh3.r[m] & 0x0000ffff; }

static INLINE void JMP(UINT32 m)
{
	sh3_delay = sh3.ea = sh3.r[m];
	sh3_ICount--;
}

static INLINE void JSR(UINT32 m)
{
	sh3.pr = sh3.pc + 2;
	sh3_delay = sh3.ea = sh3.r[m];
	sh3_ICount--;
}

static INLINE void LDCGBR(UINT32 m) { sh3.gbr = sh3.r[m]; }
static INLINE void LDCVBR(UINT32 m) { sh3.vbr = sh3.r[m]; }

static INLINE void LDCMGBR(UINT32 m)
{
	sh3.ea = sh3.r[m];
	sh3.gbr = RL(sh3.ea);
	sh3.r[m] += 4;
	sh3_ICount -= 2;
}

static INLINE void LDCMVBR(UINT32 m)
{
	sh3.ea = sh3.r[m];
	sh3.vbr = RL(sh3.ea);
	sh3.r[m] += 4;
	sh3_ICount -= 2;
}

static INLINE void LDSMACH(UINT32 m) { sh3.mach = sh3.r[m]; }
static INLINE void LDSMACL(UINT32 m) { sh3.macl = sh3.r[m]; }
static INLINE void LDSPR(UINT32 m) { sh3.pr = sh3.r[m]; }

static INLINE void LDSMMACH(UINT32 m)
{
	sh3.ea = sh3.r[m];
	sh3.mach = RL(sh3.ea);
	sh3.r[m] += 4;
}

static INLINE void LDSMMACL(UINT32 m)
{
	sh3.ea = sh3.r[m];
	sh3.macl = RL(sh3.ea);
	sh3.r[m] += 4;
}

static INLINE void LDSMPR(UINT32 m)
{
	sh3.ea = sh3.r[m];
	sh3.pr = RL(sh3.ea);
	sh3.r[m] += 4;
}

/* MAC.L @Rm+,@Rn+ (a C helper in the recompiler: no bus timing) */
static void MAC_L(UINT32 m, UINT32 n)
{
	INT32 tempn, tempm;
	int fnlml;
	UINT32 res2, res0;

	tempn = (INT32)read_long(sh3.r[n]);
	sh3.r[n] += 4;
	tempm = (INT32)read_long(sh3.r[m]);
	sh3.r[m] += 4;

	fnlml = ((tempn ^ tempm) >> 31) & 1;
	if (tempn < 0) tempn = 0 - tempn;
	if (tempm < 0) tempm = 0 - tempm;
	mul64((UINT32)tempn, (UINT32)tempm, &res2, &res0);
	if (fnlml)
	{
		res2 = ~res2;
		if (res0 == 0)
			res2++;
		else
			res0 = (~res0) + 1;
	}

	if (sh3.sr & SR_S)
	{
		/* full 64-bit accumulate, saturated to 48 bits with the sign of the product */
		UINT64 sum = (((UINT64)sh3.mach << 32) | sh3.macl) + (((UINT64)res2 << 32) | res0);
		UINT64 maxpos = ((UINT64)0x00007fff << 32) | 0xffffffff;
		UINT64 maxneg = (UINT64)0xffff8000 << 32;
		if (sum > maxpos && sum < maxneg)
			sum = fnlml ? maxneg : maxpos;
		sh3.mach = (UINT32)(sum >> 32);
		sh3.macl = (UINT32)sum;
	}
	else
	{
		res0 = sh3.macl + res0;
		if (sh3.macl > res0)
			res2++;
		res2 += sh3.mach;
		sh3.mach = res2;
		sh3.macl = res0;
	}
	sh3_ICount -= 2;
}

/* MAC.W @Rm+,@Rn+ (a C helper in the recompiler: no bus timing) */
static void MAC_W(UINT32 m, UINT32 n)
{
	INT32 tempn, tempm, dest, src, ans;
	UINT32 templ;

	tempn = (INT32)(INT16)read_word(sh3.r[n]);
	sh3.r[n] += 2;
	tempm = (INT32)(INT16)read_word(sh3.r[m]);
	sh3.r[m] += 2;

	templ = sh3.macl;
	tempm = (INT32)((UINT32)tempm * (UINT32)tempn);

	dest = (sh3.macl >> 31) & 1;
	src = (((UINT32)tempm >> 31) & 1) + dest;
	tempn = (tempm < 0) ? -1 : 0;

	sh3.macl += tempm;
	ans = ((sh3.macl >> 31) & 1) + dest;

	if (sh3.sr & SR_S)
	{
		if (ans == 1)
		{
			if (src == 0)
			{
				sh3.macl = 0x7fffffff;
				sh3.mach |= 1;
			}
			else if (src == 2)
			{
				sh3.macl = 0x80000000;
				sh3.mach |= 1;
			}
		}
	}
	else
	{
		sh3.mach += tempn;
		if (templ > sh3.macl)
			sh3.mach += 1;
	}
	sh3_ICount -= 2;
}

static INLINE void MOV(UINT32 m, UINT32 n) { sh3.r[n] = sh3.r[m]; }

static INLINE void MOVBS(UINT32 m, UINT32 n) { sh3.ea = sh3.r[n]; WB(sh3.ea, sh3.r[m] & 0xff); }
static INLINE void MOVWS(UINT32 m, UINT32 n) { sh3.ea = sh3.r[n]; WW(sh3.ea, sh3.r[m] & 0xffff); }
static INLINE void MOVLS(UINT32 m, UINT32 n) { sh3.ea = sh3.r[n]; WL(sh3.ea, sh3.r[m]); }

static INLINE void MOVBL(UINT32 m, UINT32 n) { sh3.ea = sh3.r[m]; sh3.r[n] = SEXT8(RB(sh3.ea)); }
static INLINE void MOVWL(UINT32 m, UINT32 n) { sh3.ea = sh3.r[m]; sh3.r[n] = SEXT16(RW(sh3.ea)); }
static INLINE void MOVLL(UINT32 m, UINT32 n) { sh3.ea = sh3.r[m]; sh3.r[n] = RL(sh3.ea); }

static INLINE void MOVBM(UINT32 m, UINT32 n)
{
	UINT8 data = sh3.r[m];
	sh3.r[n] -= 1;
	WB(sh3.r[n], data);
}

static INLINE void MOVWM(UINT32 m, UINT32 n)
{
	UINT16 data = sh3.r[m];
	sh3.r[n] -= 2;
	WW(sh3.r[n], data);
}

static INLINE void MOVLM(UINT32 m, UINT32 n)
{
	UINT32 data = sh3.r[m];
	sh3.r[n] -= 4;
	WL(sh3.r[n], data);
}

static INLINE void MOVBP(UINT32 m, UINT32 n)
{
	sh3.r[n] = SEXT8(RB(sh3.r[m]));
	if (n != m)
		sh3.r[m] += 1;
}

static INLINE void MOVWP(UINT32 m, UINT32 n)
{
	sh3.r[n] = SEXT16(RW(sh3.r[m]));
	if (n != m)
		sh3.r[m] += 2;
}

static INLINE void MOVLP(UINT32 m, UINT32 n)
{
	sh3.r[n] = RL(sh3.r[m]);
	if (n != m)
		sh3.r[m] += 4;
}

static INLINE void MOVBS0(UINT32 m, UINT32 n) { sh3.ea = sh3.r[n] + sh3.r[0]; WB(sh3.ea, sh3.r[m]); }
static INLINE void MOVWS0(UINT32 m, UINT32 n) { sh3.ea = sh3.r[n] + sh3.r[0]; WW(sh3.ea, sh3.r[m]); }
static INLINE void MOVLS0(UINT32 m, UINT32 n) { sh3.ea = sh3.r[n] + sh3.r[0]; WL(sh3.ea, sh3.r[m]); }
static INLINE void MOVBL0(UINT32 m, UINT32 n) { sh3.ea = sh3.r[m] + sh3.r[0]; sh3.r[n] = SEXT8(RB(sh3.ea)); }
static INLINE void MOVWL0(UINT32 m, UINT32 n) { sh3.ea = sh3.r[m] + sh3.r[0]; sh3.r[n] = SEXT16(RW(sh3.ea)); }
static INLINE void MOVLL0(UINT32 m, UINT32 n) { sh3.ea = sh3.r[m] + sh3.r[0]; sh3.r[n] = RL(sh3.ea); }

static INLINE void MOVI(UINT32 i, UINT32 n) { sh3.r[n] = SEXT8(i); }

/* PC-relative loads: folded to constants by the recompiler, no bus timing */
static INLINE void MOVWI(UINT32 d, UINT32 n)
{
	sh3.ea = sh3.pc + (d & 0xff) * 2 + 2;
	sh3.r[n] = SEXT16(read_word(sh3.ea));
}

static INLINE void MOVLI(UINT32 d, UINT32 n)
{
	sh3.ea = ((sh3.pc + 2) & ~3) + (d & 0xff) * 4;
	sh3.r[n] = read_long(sh3.ea);
}

static INLINE void MOVBLG(UINT32 d) { sh3.ea = sh3.gbr + (d & 0xff); sh3.r[0] = SEXT8(RB(sh3.ea)); }
static INLINE void MOVWLG(UINT32 d) { sh3.ea = sh3.gbr + (d & 0xff) * 2; sh3.r[0] = SEXT16(RW(sh3.ea)); }
static INLINE void MOVLLG(UINT32 d) { sh3.ea = sh3.gbr + (d & 0xff) * 4; sh3.r[0] = RL(sh3.ea); }
static INLINE void MOVBSG(UINT32 d) { sh3.ea = sh3.gbr + (d & 0xff); WB(sh3.ea, sh3.r[0]); }
static INLINE void MOVWSG(UINT32 d) { sh3.ea = sh3.gbr + (d & 0xff) * 2; WW(sh3.ea, sh3.r[0]); }
static INLINE void MOVLSG(UINT32 d) { sh3.ea = sh3.gbr + (d & 0xff) * 4; WL(sh3.ea, sh3.r[0]); }

static INLINE void MOVBS4(UINT32 d, UINT32 n) { sh3.ea = sh3.r[n] + (d & 0x0f); WB(sh3.ea, sh3.r[0]); }
static INLINE void MOVWS4(UINT32 d, UINT32 n) { sh3.ea = sh3.r[n] + (d & 0x0f) * 2; WW(sh3.ea, sh3.r[0]); }
static INLINE void MOVLS4(UINT32 m, UINT32 d, UINT32 n) { sh3.ea = sh3.r[n] + (d & 0x0f) * 4; WL(sh3.ea, sh3.r[m]); }
static INLINE void MOVBL4(UINT32 m, UINT32 d) { sh3.ea = sh3.r[m] + (d & 0x0f); sh3.r[0] = SEXT8(RB(sh3.ea)); }
static INLINE void MOVWL4(UINT32 m, UINT32 d) { sh3.ea = sh3.r[m] + (d & 0x0f) * 2; sh3.r[0] = SEXT16(RW(sh3.ea)); }
static INLINE void MOVLL4(UINT32 m, UINT32 d, UINT32 n) { sh3.ea = sh3.r[m] + (d & 0x0f) * 4; sh3.r[n] = RL(sh3.ea); }

static INLINE void MOVA(UINT32 d)
{
	sh3.ea = ((sh3.pc + 2) & ~3) + (d & 0xff) * 4;
	sh3.r[0] = sh3.ea;
}

static INLINE void MOVT(UINT32 n) { sh3.r[n] = sh3.sr & SR_T; }

static INLINE void MULL(UINT32 m, UINT32 n)
{
	sh3.macl = sh3.r[n] * sh3.r[m];
	sh3_ICount--;
}

static INLINE void MULS(UINT32 m, UINT32 n) { sh3.macl = (INT32)(INT16)sh3.r[n] * (INT32)(INT16)sh3.r[m]; }
static INLINE void MULU(UINT32 m, UINT32 n) { sh3.macl = (UINT32)(UINT16)sh3.r[n] * (UINT32)(UINT16)sh3.r[m]; }
static INLINE void NEG(UINT32 m, UINT32 n) { sh3.r[n] = 0 - sh3.r[m]; }

static INLINE void NEGC(UINT32 m, UINT32 n)
{
	UINT32 temp = sh3.r[m];
	sh3.r[n] = 0 - temp - (sh3.sr & SR_T);
	SETT_IF(temp || (sh3.sr & SR_T));
}

static INLINE void NOT(UINT32 m, UINT32 n) { sh3.r[n] = ~sh3.r[m]; }
static INLINE void OR(UINT32 m, UINT32 n) { sh3.r[n] |= sh3.r[m]; }
static INLINE void ORI(UINT32 i) { sh3.r[0] |= i; }

static INLINE void ORM(UINT32 i)
{
	sh3.ea = sh3.gbr + sh3.r[0];
	WB(sh3.ea, RB(sh3.ea) | (UINT8)i);
	sh3_ICount -= 2;
}

static INLINE void ROTCL(UINT32 n)
{
	UINT32 temp = (sh3.r[n] >> 31) & SR_T;
	sh3.r[n] = (sh3.r[n] << 1) | (sh3.sr & SR_T);
	sh3.sr = (sh3.sr & ~SR_T) | temp;
}

static INLINE void ROTCR(UINT32 n)
{
	UINT32 temp = (sh3.sr & SR_T) << 31;
	SETT_IF(sh3.r[n] & SR_T);
	sh3.r[n] = (sh3.r[n] >> 1) | temp;
}

static INLINE void ROTL(UINT32 n)
{
	sh3.sr = (sh3.sr & ~SR_T) | ((sh3.r[n] >> 31) & SR_T);
	sh3.r[n] = (sh3.r[n] << 1) | (sh3.r[n] >> 31);
}

static INLINE void ROTR(UINT32 n)
{
	sh3.sr = (sh3.sr & ~SR_T) | (sh3.r[n] & SR_T);
	sh3.r[n] = (sh3.r[n] >> 1) | (sh3.r[n] << 31);
}

static INLINE void RTS(void)
{
	sh3_delay = sh3.ea = sh3.pr;
	sh3_ICount--;
}

static INLINE void SETT(void) { sh3.sr |= SR_T; }

static INLINE void SHAL(UINT32 n)
{
	sh3.sr = (sh3.sr & ~SR_T) | ((sh3.r[n] >> 31) & SR_T);
	sh3.r[n] <<= 1;
}

static INLINE void SHAR(UINT32 n)
{
	sh3.sr = (sh3.sr & ~SR_T) | (sh3.r[n] & SR_T);
	sh3.r[n] = (UINT32)((INT32)sh3.r[n] >> 1);
}

static INLINE void SHLL(UINT32 n) { SHAL(n); }
static INLINE void SHLL2(UINT32 n) { sh3.r[n] <<= 2; }
static INLINE void SHLL8(UINT32 n) { sh3.r[n] <<= 8; }
static INLINE void SHLL16(UINT32 n) { sh3.r[n] <<= 16; }

static INLINE void SHLR(UINT32 n)
{
	sh3.sr = (sh3.sr & ~SR_T) | (sh3.r[n] & SR_T);
	sh3.r[n] >>= 1;
}

static INLINE void SHLR2(UINT32 n) { sh3.r[n] >>= 2; }
static INLINE void SHLR8(UINT32 n) { sh3.r[n] >>= 8; }
static INLINE void SHLR16(UINT32 n) { sh3.r[n] >>= 16; }

static INLINE void STCSR(UINT32 n) { sh3.r[n] = sh3.sr; }
static INLINE void STCGBR(UINT32 n) { sh3.r[n] = sh3.gbr; }
static INLINE void STCVBR(UINT32 n) { sh3.r[n] = sh3.vbr; }

static INLINE void STCMSR(UINT32 n)
{
	sh3.r[n] -= 4;
	sh3.ea = sh3.r[n];
	WL(sh3.ea, sh3.sr);
	sh3_ICount--;
}

static INLINE void STCMGBR(UINT32 n)
{
	sh3.r[n] -= 4;
	sh3.ea = sh3.r[n];
	WL(sh3.ea, sh3.gbr);
	sh3_ICount--;
}

static INLINE void STCMVBR(UINT32 n)
{
	sh3.r[n] -= 4;
	sh3.ea = sh3.r[n];
	WL(sh3.ea, sh3.vbr);
	sh3_ICount--;
}

static INLINE void STSMACH(UINT32 n) { sh3.r[n] = sh3.mach; }
static INLINE void STSMACL(UINT32 n) { sh3.r[n] = sh3.macl; }
static INLINE void STSPR(UINT32 n) { sh3.r[n] = sh3.pr; }

static INLINE void STSMMACH(UINT32 n) { sh3.r[n] -= 4; sh3.ea = sh3.r[n]; WL(sh3.ea, sh3.mach); }
static INLINE void STSMMACL(UINT32 n) { sh3.r[n] -= 4; sh3.ea = sh3.r[n]; WL(sh3.ea, sh3.macl); }
static INLINE void STSMPR(UINT32 n) { sh3.r[n] -= 4; sh3.ea = sh3.r[n]; WL(sh3.ea, sh3.pr); }

static INLINE void SUB(UINT32 m, UINT32 n) { sh3.r[n] -= sh3.r[m]; }

static INLINE void SUBC(UINT32 m, UINT32 n)
{
	UINT32 tmp1 = sh3.r[n] - sh3.r[m];
	UINT32 tmp0 = sh3.r[n];
	sh3.r[n] = tmp1 - (sh3.sr & SR_T);
	SETT_IF(tmp0 < tmp1);
	if (tmp1 < sh3.r[n])
		sh3.sr |= SR_T;
}

static INLINE void SUBV(UINT32 m, UINT32 n)
{
	INT32 dest = (sh3.r[n] >> 31) & 1;
	INT32 src = ((sh3.r[m] >> 31) & 1) + dest;
	INT32 ans;
	sh3.r[n] -= sh3.r[m];
	ans = ((sh3.r[n] >> 31) & 1) + dest;
	SETT_IF(src == 1 && ans == 1);
}

static INLINE void SWAPB(UINT32 m, UINT32 n)
{
	UINT32 temp = sh3.r[m] & 0xffff0000;
	temp |= (sh3.r[m] & 0x000000ff) << 8;
	sh3.r[n] = ((sh3.r[m] >> 8) & 0xff) | temp;
}

static INLINE void SWAPW(UINT32 m, UINT32 n)
{
	UINT32 temp = sh3.r[m] >> 16;
	sh3.r[n] = (sh3.r[m] << 16) | temp;
}

static INLINE void TAS(UINT32 n)
{
	UINT32 temp;
	sh3.ea = sh3.r[n];
	temp = RB(sh3.ea);
	SETT_IF(temp == 0);
	temp |= 0x80;
	WB(sh3.ea, temp);
	sh3_ICount -= 3;
}

static INLINE void TST(UINT32 m, UINT32 n) { SETT_IF((sh3.r[n] & sh3.r[m]) == 0); }
static INLINE void TSTI(UINT32 i) { SETT_IF(((i & 0xff) & sh3.r[0]) == 0); }

static INLINE void TSTM(UINT32 i)
{
	sh3.ea = sh3.gbr + sh3.r[0];
	SETT_IF(((i & 0xff) & RB(sh3.ea)) == 0);
	sh3_ICount -= 2;
}

static INLINE void XOR(UINT32 m, UINT32 n) { sh3.r[n] ^= sh3.r[m]; }
static INLINE void XORI(UINT32 i) { sh3.r[0] ^= i & 0xff; }

static INLINE void XORM(UINT32 i)
{
	sh3.ea = sh3.gbr + sh3.r[0];
	WB(sh3.ea, RB(sh3.ea) ^ (UINT8)i);
	sh3_ICount -= 2;
}

static INLINE void XTRCT(UINT32 m, UINT32 n) { sh3.r[n] = (sh3.r[n] >> 16) | (sh3.r[m] << 16); }

static INLINE void SLEEP(void)
{
	if (sh3.sleep_mode != 2)
		sh3.pc -= 2;
	sh3_ICount -= 2;
	if (sh3.sleep_mode == 0)
		sh3.sleep_mode = 1;
	else if (sh3.sleep_mode == 2)
		sh3.sleep_mode = 0;
}

/* SH-3 */

static INLINE void LDCSR(UINT32 opcode)
{
	UINT32 reg = sh3.r[Rn];
	if ((reg & SR_RB) != (sh3.sr & SR_RB))
		change_register_bank(reg & SR_RB ? 1 : 0);
	sh3.sr = reg & SR_FLAGS;
	exception_recompute();
}

/* LDC.L @Rm+,SR (a C helper in the recompiler: no bus timing) */
static INLINE void LDCMSR(UINT32 opcode)
{
	UINT32 old = sh3.sr;
	sh3.ea = sh3.r[Rn];
	sh3.sr = read_long(sh3.ea) & SR_FLAGS;
	if ((old & SR_RB) != (sh3.sr & SR_RB))
		change_register_bank(sh3.sr & SR_RB ? 1 : 0);
	sh3.r[Rn] += 4;
	sh3_ICount -= 2;
	exception_recompute();
}

static INLINE void RTE(void)
{
	sh3_delay = sh3.ea = sh3.spc;
	if ((sh3.ssr & SR_RB) != (sh3.sr & SR_RB))
		change_register_bank(sh3.ssr & SR_RB ? 1 : 0);
	sh3.sr = sh3.ssr;
	sh3_ICount--;
	exception_recompute();
}

static INLINE void TRAPA(UINT32 i)
{
	set_reg32(REG_TRA, (i & 0xff) << 2);

	sh3.ssr = sh3.sr;
	sh3.spc = sh3.pc;
	sh3.sgr = sh3.r[15];

	sh3.sr |= SR_MD;
	if (!(sh3.sr & SR_RB))
		change_register_bank(1);
	sh3.sr |= SR_RB;
	sh3.sr |= SR_BL;
	exception_recompute();

	set_reg32(REG_EXPEVT, 0x00000160);
	sh3.pc = sh3.vbr + 0x00000100;
	sh3_ICount -= 7;
}

#define BANK_INDEX	((sh3.sr & SR_RB) ? 0 : 1)

static INLINE void STCRBANK(UINT32 opcode) { sh3.r[Rn] = sh3.rbnk[BANK_INDEX][Rm & 7]; }

static INLINE void STCMRBANK(UINT32 opcode)
{
	UINT32 m = Rm, n = Rn;
	sh3.r[n] -= 4;
	sh3.ea = sh3.r[n];
	WL(sh3.ea, sh3.rbnk[BANK_INDEX][m & 7]);
	sh3_ICount--;
}

static INLINE void LDCRBANK(UINT32 opcode) { sh3.rbnk[BANK_INDEX][Rm & 7] = sh3.r[Rn]; }

static INLINE void LDCMRBANK(UINT32 opcode)
{
	UINT32 m = Rm, n = Rn;
	sh3.ea = sh3.r[n];
	sh3.rbnk[BANK_INDEX][m & 7] = RL(sh3.ea);
	sh3.r[n] += 4;
}

static INLINE void SHAD(UINT32 opcode)
{
	UINT32 m = Rm, n = Rn;
	if ((sh3.r[m] & 0x80000000) == 0)
		sh3.r[n] = sh3.r[n] << (sh3.r[m] & 0x1f);
	else if ((sh3.r[m] & 0x1f) == 0)
		sh3.r[n] = (sh3.r[n] & 0x80000000) ? 0xffffffff : 0;
	else
		sh3.r[n] = (UINT32)((INT32)sh3.r[n] >> ((~sh3.r[m] & 0x1f) + 1));
}

static INLINE void SHLD(UINT32 opcode)
{
	UINT32 m = Rm, n = Rn;
	if ((sh3.r[m] & 0x80000000) == 0)
		sh3.r[n] = sh3.r[n] << (sh3.r[m] & 0x1f);
	else if ((sh3.r[m] & 0x1f) == 0)
		sh3.r[n] = 0;
	else
		sh3.r[n] = sh3.r[n] >> ((~sh3.r[m] & 0x1f) + 1);
}

static INLINE void STORE_M(UINT32 opcode, UINT32 value)
{
	UINT32 n = Rn;
	sh3.r[n] -= 4;
	sh3.ea = sh3.r[n];
	WL(sh3.ea, value);
}

static INLINE UINT32 LOAD_M(UINT32 opcode)
{
	UINT32 value;
	sh3.ea = sh3.r[Rn];
	value = RL(sh3.ea);
	sh3.r[Rn] += 4;
	return value;
}


/***************************************************************************
	dispatch
***************************************************************************/

static void op0000(UINT16 opcode)
{
	switch (opcode & 0xff)
	{
		/* SH-3/SH-4 */
		case 0x52: case 0x62: case 0x43: case 0x63: case 0xe3:
		case 0x68: case 0xe8: case 0x4a: case 0xca:
			return;								/* illegal */
		case 0x93: case 0xa3: case 0xb3:
			return;								/* not done */
		case 0x82: case 0x92: case 0xa2: case 0xb2:
		case 0xc2: case 0xd2: case 0xe2: case 0xf2:
			STCRBANK(opcode); return;
		case 0x32: sh3.r[Rn] = sh3.ssr; return;		/* STC SSR,Rn */
		case 0x42: sh3.r[Rn] = sh3.spc; return;		/* STC SPC,Rn */
		case 0x83: return;							/* PREF @Rn */
		case 0xc3: sh3.ea = sh3.r[Rn]; WL(sh3.ea, sh3.r[0]); return;	/* MOVCA.L */
		case 0x38: case 0xb8: return;				/* LDTLB */
		case 0x48: case 0xc8: sh3.sr &= ~SR_S; return;	/* CLRS */
		case 0x58: case 0xd8: sh3.sr |= SR_S; return;	/* SETS */
		case 0x3a: case 0xba: sh3.r[Rn] = sh3.sgr; return;
		case 0x5a: case 0xda: sh3.r[Rn] = sh3.fpul; return;
		case 0x6a: case 0xea: sh3.r[Rn] = sh3.fpscr & 0x003fffff; return;
		case 0x7a: case 0xfa: sh3.r[Rn] = sh3.dbr; return;
	}

	switch (opcode & 0x3f)
	{
		case 0x02: STCSR(Rn); break;
		case 0x03: BSRF(Rn); break;
		case 0x04: case 0x14: case 0x24: case 0x34: MOVBS0(Rm, Rn); break;
		case 0x05: case 0x15: case 0x25: case 0x35: MOVWS0(Rm, Rn); break;
		case 0x06: case 0x16: case 0x26: case 0x36: MOVLS0(Rm, Rn); break;
		case 0x07: case 0x17: case 0x27: case 0x37: MULL(Rm, Rn); break;
		case 0x08: CLRT(); break;
		case 0x0a: STSMACH(Rn); break;
		case 0x0b: RTS(); break;
		case 0x0c: case 0x1c: case 0x2c: case 0x3c: MOVBL0(Rm, Rn); break;
		case 0x0d: case 0x1d: case 0x2d: case 0x3d: MOVWL0(Rm, Rn); break;
		case 0x0e: case 0x1e: case 0x2e: case 0x3e: MOVLL0(Rm, Rn); break;
		case 0x0f: case 0x1f: case 0x2f: case 0x3f: MAC_L(Rm, Rn); break;
		case 0x12: STCGBR(Rn); break;
		case 0x18: SETT(); break;
		case 0x19: DIV0U(); break;
		case 0x1a: STSMACL(Rn); break;
		case 0x1b: SLEEP(); break;
		case 0x22: STCVBR(Rn); break;
		case 0x23: BRAF(Rn); break;
		case 0x28: CLRMAC(); break;
		case 0x29: MOVT(Rn); break;
		case 0x2a: STSPR(Rn); break;
		case 0x2b: RTE(); break;
		default: break;							/* NOP, illegal */
	}
}

static void op0100(UINT16 opcode)
{
	switch (opcode & 0xff)
	{
		/* SH-3/SH-4 */
		case 0x42: case 0x46: case 0x4a: case 0x53: case 0x57: case 0x5e:
		case 0x63: case 0x67: case 0x6e: case 0x82: case 0x86: case 0x8a:
		case 0x92: case 0x96: case 0x9a: case 0xa2: case 0xa6: case 0xaa:
		case 0xc2: case 0xc6: case 0xca: case 0xd2: case 0xd6: case 0xda:
		case 0xe2: case 0xe6: case 0xea:
			return;								/* illegal */
		case 0x0c: case 0x1c: case 0x2c: case 0x3c: case 0x4c: case 0x5c: case 0x6c: case 0x7c:
		case 0x8c: case 0x9c: case 0xac: case 0xbc: case 0xcc: case 0xdc: case 0xec: case 0xfc:
			SHAD(opcode); return;
		case 0x0d: case 0x1d: case 0x2d: case 0x3d: case 0x4d: case 0x5d: case 0x6d: case 0x7d:
		case 0x8d: case 0x9d: case 0xad: case 0xbd: case 0xcd: case 0xdd: case 0xed: case 0xfd:
			SHLD(opcode); return;
		case 0x8e: case 0x9e: case 0xae: case 0xbe: case 0xce: case 0xde: case 0xee: case 0xfe:
			LDCRBANK(opcode); return;
		case 0x83: case 0x93: case 0xa3: case 0xb3: case 0xc3: case 0xd3: case 0xe3: case 0xf3:
			STCMRBANK(opcode); return;
		case 0x87: case 0x97: case 0xa7: case 0xb7: case 0xc7: case 0xd7: case 0xe7: case 0xf7:
			LDCMRBANK(opcode); return;
		case 0x32: STORE_M(opcode, sh3.sgr); return;
		case 0x33: STORE_M(opcode, sh3.ssr); return;
		case 0x37: sh3.ssr = LOAD_M(opcode); return;
		case 0x3e: sh3.ssr = sh3.r[Rn]; return;
		case 0x43: STORE_M(opcode, sh3.spc); return;
		case 0x47: sh3.spc = LOAD_M(opcode); return;
		case 0x4e: sh3.spc = sh3.r[Rn]; return;
		case 0x52: STORE_M(opcode, sh3.fpul); return;
		case 0x56: sh3.fpul = LOAD_M(opcode); return;
		case 0x5a: sh3.fpul = sh3.r[Rn]; return;
		case 0x62: STORE_M(opcode, sh3.fpscr & 0x003fffff); return;
		case 0x66: sh3.fpscr = LOAD_M(opcode) & 0x003fffff; return;
		case 0x6a: sh3.fpscr = sh3.r[Rn] & 0x003fffff; return;
		case 0xf2: STORE_M(opcode, sh3.dbr); return;
		case 0xf6: sh3.dbr = LOAD_M(opcode); return;
		case 0xfa: sh3.dbr = sh3.r[Rn]; return;
	}

	switch (opcode & 0x3f)
	{
		case 0x00: SHLL(Rn); break;
		case 0x01: SHLR(Rn); break;
		case 0x02: STSMMACH(Rn); break;
		case 0x03: STCMSR(Rn); break;
		case 0x04: ROTL(Rn); break;
		case 0x05: ROTR(Rn); break;
		case 0x06: LDSMMACH(Rn); break;
		case 0x07: LDCMSR(opcode); break;
		case 0x08: SHLL2(Rn); break;
		case 0x09: SHLR2(Rn); break;
		case 0x0a: LDSMACH(Rn); break;
		case 0x0b: JSR(Rn); break;
		case 0x0e: LDCSR(opcode); break;
		case 0x0f: case 0x1f: case 0x2f: case 0x3f: MAC_W(Rm, Rn); break;
		case 0x10: DT(Rn); break;
		case 0x11: CMPPZ(Rn); break;
		case 0x12: STSMMACL(Rn); break;
		case 0x13: STCMGBR(Rn); break;
		case 0x15: CMPPL(Rn); break;
		case 0x16: LDSMMACL(Rn); break;
		case 0x17: LDCMGBR(Rn); break;
		case 0x18: SHLL8(Rn); break;
		case 0x19: SHLR8(Rn); break;
		case 0x1a: LDSMACL(Rn); break;
		case 0x1b: TAS(Rn); break;
		case 0x1e: LDCGBR(Rn); break;
		case 0x20: SHAL(Rn); break;
		case 0x21: SHAR(Rn); break;
		case 0x22: STSMPR(Rn); break;
		case 0x23: STCMVBR(Rn); break;
		case 0x24: ROTCL(Rn); break;
		case 0x25: ROTCR(Rn); break;
		case 0x26: LDSMPR(Rn); break;
		case 0x27: LDCMVBR(Rn); break;
		case 0x28: SHLL16(Rn); break;
		case 0x29: SHLR16(Rn); break;
		case 0x2a: LDSPR(Rn); break;
		case 0x2b: JMP(Rn); break;
		case 0x2e: LDCVBR(Rn); break;
		default: break;
	}
}

/* runs one instruction; the cycles past the first are charged here, the
   cycles of MAME's recompiler (sh_fe.cpp) */
static void execute_one(UINT16 opcode)
{
	switch (opcode >> 12)
	{
		case 0:
			op0000(opcode);
			switch (opcode & 0x3f)
			{
				case 0x07: case 0x17: case 0x27: case 0x37:	/* MUL.L: 2, charged */
				case 0x03: case 0x23: case 0x0b:			/* BSRF, BRAF, RTS: 2, charged */
					break;
				case 0x0f: case 0x1f: case 0x2f: case 0x3f:	/* MAC.L: 3 + 2 */
					sh3_ICount -= 2;
					break;
				case 0x1b:									/* SLEEP: 3 */
					break;
				case 0x2b:									/* RTE: 4 + 1 */
					sh3_ICount -= 3;
					break;
			}
			break;

		case 1: MOVLS4(Rm, opcode & 0x0f, Rn); break;

		case 2:
			switch (opcode & 15)
			{
				case  0: MOVBS(Rm, Rn); break;
				case  1: MOVWS(Rm, Rn); break;
				case  2: MOVLS(Rm, Rn); break;
				case  4: MOVBM(Rm, Rn); break;
				case  5: MOVWM(Rm, Rn); break;
				case  6: MOVLM(Rm, Rn); break;
				case  7: DIV0S(Rm, Rn); break;
				case  8: TST(Rm, Rn); break;
				case  9: AND(Rm, Rn); break;
				case 10: XOR(Rm, Rn); break;
				case 11: OR(Rm, Rn); break;
				case 12: CMPSTR(Rm, Rn); break;
				case 13: XTRCT(Rm, Rn); break;
				case 14: MULU(Rm, Rn); break;
				case 15: MULS(Rm, Rn); break;
			}
			break;

		case 3:
			switch (opcode & 15)
			{
				case  0: CMPEQ(Rm, Rn); break;
				case  2: CMPHS(Rm, Rn); break;
				case  3: CMPGE(Rm, Rn); break;
				case  4: DIV1(Rm, Rn); break;
				case  5: DMULU(Rm, Rn); break;
				case  6: CMPHI(Rm, Rn); break;
				case  7: CMPGT(Rm, Rn); break;
				case  8: SUB(Rm, Rn); break;
				case 10: SUBC(Rm, Rn); break;
				case 11: SUBV(Rm, Rn); break;
				case 12: ADD(Rm, Rn); break;
				case 13: DMULS(Rm, Rn); break;
				case 14: ADDC(Rm, Rn); break;
				case 15: ADDV(Rm, Rn); break;
			}
			break;

		case 4:
			op0100(opcode);
			switch (opcode & 0xff)
			{
				case 0x07: case 0x17: case 0x27:		/* LDC.L @Rm+: 3 (+ 2 for SR) */
					if ((opcode & 0xff) != 0x07)
						break;
					sh3_ICount -= 2;
					break;
				case 0x0f: case 0x1f: case 0x2f: case 0x3f:	/* MAC.W: 3 + 2 */
					sh3_ICount -= 2;
					break;
			}
			break;

		case 5: MOVLL4(Rm, opcode & 0x0f, Rn); break;

		case 6:
			switch (opcode & 15)
			{
				case  0: MOVBL(Rm, Rn); break;
				case  1: MOVWL(Rm, Rn); break;
				case  2: MOVLL(Rm, Rn); break;
				case  3: MOV(Rm, Rn); break;
				case  4: MOVBP(Rm, Rn); break;
				case  5: MOVWP(Rm, Rn); break;
				case  6: MOVLP(Rm, Rn); break;
				case  7: NOT(Rm, Rn); break;
				case  8: SWAPB(Rm, Rn); break;
				case  9: SWAPW(Rm, Rn); break;
				case 10: NEGC(Rm, Rn); break;
				case 11: NEG(Rm, Rn); break;
				case 12: EXTUB(Rm, Rn); break;
				case 13: EXTUW(Rm, Rn); break;
				case 14: EXTSB(Rm, Rn); break;
				case 15: EXTSW(Rm, Rn); break;
			}
			break;

		case 7: ADDI(opcode & 0xff, Rn); break;

		case 8:
			switch ((opcode >> 8) & 15)
			{
				case  0: MOVBS4(opcode & 0x0f, Rm); break;
				case  1: MOVWS4(opcode & 0x0f, Rm); break;
				case  4: MOVBL4(Rm, opcode & 0x0f); break;
				case  5: MOVWL4(Rm, opcode & 0x0f); break;
				case  8: CMPIM(opcode & 0xff); break;
				case  9: BT(opcode & 0xff); break;
				case 11: BF(opcode & 0xff); break;
				case 13: BTS(opcode & 0xff); break;
				case 15: BFS(opcode & 0xff); break;
				default: break;
			}
			break;

		case 9: MOVWI(opcode & 0xff, Rn); break;
		case 10: BRA(opcode & 0xfff); break;
		case 11: BSR(opcode & 0xfff); break;

		case 12:
			switch ((opcode >> 8) & 15)
			{
				case  0: MOVBSG(opcode & 0xff); break;
				case  1: MOVWSG(opcode & 0xff); break;
				case  2: MOVLSG(opcode & 0xff); break;
				case  3: TRAPA(opcode & 0xff); break;	/* 8 + 7 */
				case  4: MOVBLG(opcode & 0xff); break;
				case  5: MOVWLG(opcode & 0xff); break;
				case  6: MOVLLG(opcode & 0xff); break;
				case  7: MOVA(opcode & 0xff); break;
				case  8: TSTI(opcode & 0xff); break;
				case  9: ANDI(opcode & 0xff); break;
				case 10: XORI(opcode & 0xff); break;
				case 11: ORI(opcode & 0xff); break;
				case 12: TSTM(opcode & 0xff); break;
				case 13: ANDM(opcode & 0xff); break;
				case 14: XORM(opcode & 0xff); break;
				case 15: ORM(opcode & 0xff); break;
			}
			if (((opcode >> 8) & 15) == 3)
				sh3_ICount -= 7;
			break;

		case 13: MOVLI(opcode & 0xff, Rn); break;
		case 14: MOVI(opcode & 0xff, Rn); break;
		case 15: break;								/* FPU: none on the SH7709S */
	}
}


#include "sh3jit.c"


/***************************************************************************
	execution
***************************************************************************/

int sh3_execute(int cycles)
{
	UINT64 last_op = last_op_cycles();

	sh3_ICount = cycles;
	sh3_exec_cycles = cycles;
	sh3_exec_base = ((UINT64)sh3.total_cycles_hi << 32) | sh3.total_cycles_lo;
	set_last_op_cycles(last_op);

	/* asleep with nothing to wake up: the cycles go */
	if (sh3.sleep_mode == 1 && !sh3.test_irq)
		sh3_ICount = 0;

	while (sh3_ICount > 0)
	{
		UINT16 opcode;

		if (jit_enabled && jit_run())
			continue;

		sh3.ppc = sh3.pc;
		CALL_MAME_DEBUG;
		opcode = fetch(sh3.pc);
		sh3.pc += 2;
		sh3_delay = 0;
		execute_one(opcode);

		/* the delay slot, with its own PC; a branch there doesn't branch */
		if (sh3_delay)
		{
			UINT32 target = sh3_delay;
			sh3.ppc = sh3.pc;
			opcode = fetch(sh3.pc);
			sh3.pc += 2;
			sh3_delay = 0;
			execute_one(opcode);
			sh3.pc = target;
			sh3_delay = 0;
			sh3_ICount--;
		}

		if (sh3.test_irq)
			check_pending_irq();

		sh3_ICount--;

		if (sh3.sleep_mode == 1 && !sh3.test_irq && sh3_ICount > 0)
			sh3_ICount = 0;
	}

	{
		UINT64 total = sh3_exec_base + (cycles - sh3_ICount);
		last_op = last_op_cycles();
		sh3.total_cycles_lo = (UINT32)total;
		sh3.total_cycles_hi = (UINT32)(total >> 32);
		sh3_exec_base = total;
		sh3_exec_cycles = sh3_ICount = 0;
		set_last_op_cycles(last_op);
	}
	return cycles - sh3_ICount;
}

UINT32 sh3_total_cycles_lo(void)
{
	return (UINT32)total_cycles();
}

/* sleep: when asleep the CPU stops at the SLEEP and burns its cycles; the
   interrupt that wakes it is taken in the next call */


/***************************************************************************
	interface
***************************************************************************/

void sh3_set_irq_line(int irqline, int state)
{
	if (irqline < 0 || irqline > 3)
		return;
	if (sh3.irq_line_state[irqline] == state)
		return;
	sh3.irq_line_state[irqline] = state;

	if (state == CLEAR_LINE)
		exception_unrequest(INTC_IRL0 + irqline);
	else
		exception_request(INTC_IRL0 + irqline);
}

void sh3_set_irq_callback(int (*callback)(int irqline))
{
	sh3.irq_callback = callback;
}

static void sh3_presave(void)
{
	set_last_op_cycles(last_op_cycles());
}

static void sh3_postload(void)
{
	set_last_op_cycles(((UINT64)sh3.last_op_cycles_hi << 32) | sh3.last_op_cycles_lo);
	fetch_region = NULL;
	bsc_valid = 0;
	cache_mru_rebuild();
	jit_flush_pending = 1;
}

void sh3_init(void)
{
	int cpu = cpu_getactivecpu();
	int i;

	memset(&sh3, 0, sizeof(sh3));
	cache_mru_rebuild();
	set_last_op_cycles(0);
	sh3.cpunum = cpu;
	fastram_count = 0;
	fastram_build_pages();
	fetch_region = NULL;
	build_special_tables();

	for (i = 0; i < 3; i++)
		sh3.timer[i] = timer_alloc(sh3_timer_callback);
	for (i = 0; i < 4; i++)
		sh3.dma_timer[i] = timer_alloc(sh3_dma_callback);

	state_save_register_UINT32("sh3", cpu, "ppc", &sh3.ppc, 1);
	state_save_register_UINT32("sh3", cpu, "pc", &sh3.pc, 1);
	state_save_register_UINT32("sh3", cpu, "pr", &sh3.pr, 1);
	state_save_register_UINT32("sh3", cpu, "sr", &sh3.sr, 1);
	state_save_register_UINT32("sh3", cpu, "gbr", &sh3.gbr, 1);
	state_save_register_UINT32("sh3", cpu, "vbr", &sh3.vbr, 1);
	state_save_register_UINT32("sh3", cpu, "mach", &sh3.mach, 1);
	state_save_register_UINT32("sh3", cpu, "macl", &sh3.macl, 1);
	state_save_register_UINT32("sh3", cpu, "r", sh3.r, 16);
	state_save_register_UINT32("sh3", cpu, "rbnk0", sh3.rbnk[0], 8);
	state_save_register_UINT32("sh3", cpu, "rbnk1", sh3.rbnk[1], 8);
	state_save_register_UINT32("sh3", cpu, "ssr", &sh3.ssr, 1);
	state_save_register_UINT32("sh3", cpu, "spc", &sh3.spc, 1);
	state_save_register_UINT32("sh3", cpu, "sgr", &sh3.sgr, 1);
	state_save_register_UINT32("sh3", cpu, "dbr", &sh3.dbr, 1);
	state_save_register_UINT32("sh3", cpu, "fpscr", &sh3.fpscr, 1);
	state_save_register_UINT32("sh3", cpu, "fpul", &sh3.fpul, 1);
	state_save_register_UINT32("sh3", cpu, "sleep_mode", &sh3.sleep_mode, 1);
	state_save_register_UINT32("sh3", cpu, "pending_irq", &sh3.pending_irq, 1);
	state_save_register_UINT32("sh3", cpu, "test_irq", &sh3.test_irq, 1);
	state_save_register_UINT8("sh3", cpu, "exception_requesting", sh3.exception_requesting, INTC_COUNT);
	state_save_register_UINT32("sh3", cpu, "exception_priority", sh3.exception_priority, INTC_COUNT);
	state_save_register_INT32("sh3", cpu, "irq_line_state", sh3.irq_line_state, 4);
	state_save_register_UINT8("sh3", cpu, "regp4", sh3.regp4, sizeof(sh3.regp4));
	state_save_register_UINT8("sh3", cpu, "rega1", sh3.rega1, sizeof(sh3.rega1));
	state_save_register_UINT32("sh3", cpu, "tcor", sh3.tcor, 3);
	state_save_register_UINT32("sh3", cpu, "tcnt", sh3.tcnt, 3);
	state_save_register_UINT32("sh3", cpu, "tcr", sh3.tcr, 3);
	state_save_register_UINT8("sh3", cpu, "tstr", &sh3.tstr, 1);
	state_save_register_UINT8("sh3", cpu, "dma_timer_active", sh3.dma_timer_active, 4);
	state_save_register_UINT32("sh3", cpu, "cache_tag", &sh3.cache_tag[0][0], CACHE_BLOCKS * CACHE_ASSOC);
	state_save_register_UINT8("sh3", cpu, "cache_lru", &sh3.cache_lru[0][0], CACHE_BLOCKS * CACHE_ASSOC);
	state_save_register_UINT8("sh3", cpu, "cache_dirty", &sh3.cache_dirty[0][0], CACHE_BLOCKS * CACHE_ASSOC);
	state_save_register_UINT32("sh3", cpu, "wb_address", &sh3.wb_address, 1);
	state_save_register_UINT32("sh3", cpu, "last_area", &sh3.last_area_accessed, 1);
	state_save_register_UINT32("sh3", cpu, "last_area_write", &sh3.last_area_accessed_was_write, 1);
	state_save_register_UINT32("sh3", cpu, "wb_active", &sh3.wb_active_cycles, 1);
	state_save_register_UINT32("sh3", cpu, "burst_cont", &sh3.burst_continuation_remaining_cycles, 1);
	state_save_register_UINT32("sh3", cpu, "cache_port_free", &sh3.cache_port_free, 1);
	state_save_register_UINT32("sh3", cpu, "last_access", &sh3.last_access_address, 1);
	state_save_register_UINT32("sh3", cpu, "last_op_lo", &sh3.last_op_cycles_lo, 1);
	state_save_register_UINT32("sh3", cpu, "last_op_hi", &sh3.last_op_cycles_hi, 1);
	state_save_register_UINT32("sh3", cpu, "total_lo", &sh3.total_cycles_lo, 1);
	state_save_register_UINT32("sh3", cpu, "total_hi", &sh3.total_cycles_hi, 1);
	state_save_register_func_presave(sh3_presave);
	state_save_register_func_postload(sh3_postload);
}

void sh3_reset(void *param)
{
	struct sh3_config *conf = param;
	void *timer[3], *dma_timer[4];
	int (*irq_callback)(int irqline);
	UINT32 total_lo = sh3.total_cycles_lo, total_hi = sh3.total_cycles_hi;
	int cpunum = sh3.cpunum;
	int i;

	for (i = 0; i < 3; i++)
	{
		timer[i] = sh3.timer[i];
		timer_adjust(timer[i], TIME_NEVER, i, 0);
	}
	for (i = 0; i < 4; i++)
	{
		dma_timer[i] = sh3.dma_timer[i];
		timer_adjust(dma_timer[i], TIME_NEVER, i, 0);
	}
	irq_callback = sh3.irq_callback;

	memset(&sh3, 0, sizeof(sh3));
	cache_mru_rebuild();
	for (i = 0; i < 3; i++)
		sh3.timer[i] = timer[i];
	for (i = 0; i < 4; i++)
		sh3.dma_timer[i] = dma_timer[i];
	sh3.irq_callback = irq_callback;
	sh3.cpunum = cpunum;
	sh3.total_cycles_lo = total_lo;
	sh3.total_cycles_hi = total_hi;
	set_last_op_cycles(0);

	if (conf)
	{
		sh3.cache_timing = conf->cache_timing;
		sh3.pm_clock = conf->pm_clock;
		sh3.port_r = conf->port_r;
		sh3.port_w = conf->port_w;
	}
	if (sh3.pm_clock == 0)
		sh3.pm_clock = 25600000;

	/* TMU */
	for (i = 0; i < 3; i++)
	{
		sh3.tcor[i] = 0xffffffff;
		sh3.tcnt[i] = 0xffffffff;
	}

	/* SCI, CMT, AD, DA, ports, IRDA, UDI: the reset values of current MAME */
	sh3.regp4[0xfffffe82 - 0xfffffe80] = 0xff;		/* SCBRR */
	sh3.regp4[0xfffffe86 - 0xfffffe80] = 0xff;		/* SCTDR */
	sh3.regp4[REG_SCSSR - 0xfffffe80] = 0x84;
	set_reg16(0x04000076, 0xffff);					/* CMCOR */
	sh3.rega1[0x92] = 0x07;							/* ADCR */
	sh3.rega1[0xa4] = 0x1f;							/* DADCR */
	set_reg16(0x04000104, 0xaaaa);					/* PCCR */
	set_reg16(0x04000106, 0xaa8a);					/* PDCR */
	set_reg16(0x04000108, 0xaaaa);					/* PECR */
	set_reg16(0x0400010a, 0xaaaa);					/* PFCR */
	set_reg16(0x0400010c, 0xaaaa);					/* PGCR */
	set_reg16(0x0400010e, 0xaaaa);					/* PHCR */
	set_reg16(0x04000116, 0xa888);					/* SCPCR */
	sh3.rega1[0x142] = 0xff;						/* SCBRR1 */
	set_reg16(0x04000148, 0x0060);					/* SCSSR1 */
	set_reg16(0x04000200, 0xffff);					/* SDIR */

	default_exception_priorities();

	sh3.cache_port_free = 1;
	sh3.pc = 0xa0000000;
	sh3.ppc = sh3.pc;
	sh3.r[15] = read_long(4);
	sh3.sr = 0x700000f0;
	sh3.fpscr = 0x00040001;
	fetch_region = NULL;
	jit_flush_pending = 1;
	bsc_valid = 0;
}

void sh3_exit(void)
{
	jit_exit();
}

unsigned sh3_get_context(void *dst)
{
	if (dst)
		memcpy(dst, &sh3, sizeof(sh3));
	return sizeof(sh3);
}

void sh3_set_context(void *src)
{
	if (src)
		memcpy(&sh3, src, sizeof(sh3));
	fetch_region = NULL;
	bsc_valid = 0;
	cache_mru_rebuild();
}

unsigned sh3_get_reg(int regnum)
{
	switch (regnum)
	{
		case REG_PREVIOUSPC:	return sh3.ppc;
		case REG_PC:
		case SH3_PC:			return sh3.pc;
		case REG_SP:			return sh3.r[15];
		case SH3_PR:			return sh3.pr;
		case SH3_SR:			return sh3.sr;
		case SH3_GBR:			return sh3.gbr;
		case SH3_VBR:			return sh3.vbr;
		case SH3_MACH:			return sh3.mach;
		case SH3_MACL:			return sh3.macl;
		case SH3_SSR:			return sh3.ssr;
		case SH3_SPC:			return sh3.spc;
		case SH3_EA:			return sh3.ea;
	}
	if (regnum >= SH3_R0 && regnum <= SH3_R15)
		return sh3.r[regnum - SH3_R0];
	return 0;
}

void sh3_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
		case REG_PC:
		case SH3_PC:	sh3.pc = val; fetch_region = NULL; return;
		case REG_SP:	sh3.r[15] = val; return;
		case SH3_PR:	sh3.pr = val; return;
		case SH3_SR:	sh3.sr = val; exception_recompute(); return;
		case SH3_GBR:	sh3.gbr = val; return;
		case SH3_VBR:	sh3.vbr = val; return;
		case SH3_MACH:	sh3.mach = val; return;
		case SH3_MACL:	sh3.macl = val; return;
		case SH3_SSR:	sh3.ssr = val; return;
		case SH3_SPC:	sh3.spc = val; return;
		case SH3_EA:	sh3.ea = val; return;
	}
	if (regnum >= SH3_R0 && regnum <= SH3_R15)
		sh3.r[regnum - SH3_R0] = val;
}

const char *sh3_info(void *context, int regnum)
{
	static char buffer[16][47 + 1];
	static int which = 0;
	SH3 *r = context ? (SH3 *)context : &sh3;

	which = (which + 1) % 16;
	buffer[which][0] = '\0';

	switch (regnum)
	{
		case CPU_INFO_REG + SH3_PC:		sprintf(buffer[which], "PC  :%08X", r->pc); break;
		case CPU_INFO_REG + SH3_SR:		sprintf(buffer[which], "SR  :%08X", r->sr); break;
		case CPU_INFO_REG + SH3_PR:		sprintf(buffer[which], "PR  :%08X", r->pr); break;
		case CPU_INFO_REG + SH3_GBR:	sprintf(buffer[which], "GBR :%08X", r->gbr); break;
		case CPU_INFO_REG + SH3_VBR:	sprintf(buffer[which], "VBR :%08X", r->vbr); break;
		case CPU_INFO_REG + SH3_MACH:	sprintf(buffer[which], "MACH:%08X", r->mach); break;
		case CPU_INFO_REG + SH3_MACL:	sprintf(buffer[which], "MACL:%08X", r->macl); break;
		case CPU_INFO_REG + SH3_SSR:	sprintf(buffer[which], "SSR :%08X", r->ssr); break;
		case CPU_INFO_REG + SH3_SPC:	sprintf(buffer[which], "SPC :%08X", r->spc); break;
		case CPU_INFO_FLAGS:
			sprintf(buffer[which], "%c%c%d%c%c",
					r->sr & SR_M ? 'M':'.',
					r->sr & SR_Q ? 'Q':'.',
					(r->sr & SR_I) >> 4,
					r->sr & SR_S ? 'S':'.',
					r->sr & SR_T ? 'T':'.');
			break;
		case CPU_INFO_NAME:		return "SH-3";
		case CPU_INFO_FAMILY:	return "Hitachi SH7700";
		case CPU_INFO_VERSION:	return "1.0";
		case CPU_INFO_FILE:		return __FILE__;
		case CPU_INFO_CREDITS:	return "Current MAME (R. Belmont, David Haywood, Jesus Ramos and others)";
		default:
			if (regnum >= CPU_INFO_REG + SH3_R0 && regnum <= CPU_INFO_REG + SH3_R15)
				sprintf(buffer[which], "R%-3d:%08X", regnum - (CPU_INFO_REG + SH3_R0), r->r[regnum - (CPU_INFO_REG + SH3_R0)]);
			break;
	}
	return buffer[which];
}

unsigned sh3_dasm(char *buffer, unsigned pc)
{
#ifdef MAME_DEBUG
	return DasmSH2(buffer, pc);
#else
	sprintf(buffer, "$%04X", read_word(pc));
	return 2;
#endif
}
