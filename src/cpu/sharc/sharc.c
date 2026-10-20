/***************************************************************************

    Analog Devices ADSP-21062 "SHARC", from current MAME
    (src/devices/cpu/sharc: sharc.cpp, sharcops.hxx, sharcops_table.cpp,
    compute.hxx, sharcdma.hxx, sharcinternal.ipp; copyright Ville Linde,
    BSD-3-Clause), translated to C for MAME 2003. Interpreter only (the
    MAME recompiler is not ported).

    Memory: the internal memory (two blocks of 32K 32-bit words, seen as
    48-bit program words, 32-bit data words and 16-bit short words, as
    the pgm_2m / data_2m maps of MAME) and the IOP registers live in the
    core. Every other data space (DM) address goes to the data_r/data_w
    callbacks of struct sharc_config (MDRV_CPU_CONFIG), every other
    program space address to the optional pgm_r/pgm_w callbacks.

    Port notes (not in MAME):
    - single instance: the live state is the static 'core' and is always
      authoritative; set_context does not copy anything in, so the
      helpers (sharc_external_dma_write, ...) may be called while
      another CPU is active.
    - sharc_stall(), for driver callbacks that cannot complete (FIFOs):
      see sharcops.c; the instruction is rewound (fetch stage and loop
      stack state) and executed again on the next timeslice.
    - MAME fatal errors (unimplemented features, stack overflows) are
      logged with logerror and the operation is skipped.
    - the delayed IOP write (DMA channel 6/7 start) uses a mame2003
      timer, as MAME uses an emu timer.

***************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "driver.h"
#include "cpuintrf.h"
#include "state.h"
#include "mamedbg.h"
#include "log.h"
#include "sharc.h"


/***************************************************************************
    CONSTANTS
***************************************************************************/

/* ASTAT shifts */
#define AZ_SHIFT    0
#define AV_SHIFT    1
#define AN_SHIFT    2
#define AC_SHIFT    3
#define AS_SHIFT    4
#define AI_SHIFT    5
#define MN_SHIFT    6
#define MV_SHIFT    7
#define MU_SHIFT    8
#define MI_SHIFT    9
#define AF_SHIFT    10
#define SV_SHIFT    11
#define SZ_SHIFT    12
#define SS_SHIFT    13
#define BTF_SHIFT   18
#define FLG0_SHIFT  19
#define FLG1_SHIFT  20
#define FLG2_SHIFT  21
#define FLG3_SHIFT  22

/* ASTAT flags */
#define AZ          0x00000001U     /* ALU result zero */
#define AV          0x00000002U     /* ALU overflow */
#define AN          0x00000004U     /* ALU result negative */
#define AC          0x00000008U     /* ALU fixed-point carry */
#define AS          0x00000010U     /* ALU X input sign */
#define AI          0x00000020U     /* ALU floating-point invalid operation */
#define MN          0x00000040U     /* Multiplier result negative */
#define MV          0x00000080U     /* Multiplier overflow */
#define MU          0x00000100U     /* Multiplier underflow */
#define MI          0x00000200U     /* Multiplier floating-point invalid operation */
#define AF          0x00000400U     /* Last ALU operation was a floating-point operation */
#define SV          0x00000800U     /* Shifter overflow */
#define SZ          0x00001000U     /* Shifter result zero */
#define SS          0x00002000U     /* Shifter input sign */
#define BTF         0x00040000U     /* Bit Test Flag */
#define FLG0        0x00080000U     /* FLAG0 */
#define FLG1        0x00100000U     /* FLAG1 */
#define FLG2        0x00200000U     /* FLAG2 */
#define FLG3        0x00400000U     /* FLAG3 */

/* STKY flags */
#define AUS         0x00000001U     /* ALU floating-point underflow */
#define AVS         0x00000002U     /* ALU floating-point overflow */
#define AOS         0x00000004U     /* ALU fixed-point overflow */
#define AIS         0x00000020U     /* ALU floating-point invalid operation */
#define MOS         0x00000040U     /* Multiplier fixed-point overflow */
#define MVS         0x00000080U     /* Multiplier floating-point overflow */
#define MUS         0x00000100U     /* Multiplier underflow */
#define MIS         0x00000200U     /* Multiplier floating-point invalid operation */
#define PCFL        0x00200000U     /* PC stack full */
#define PCEM        0x00400000U     /* PC stack empty */
#define SSOV        0x00800000U     /* Status stack overflow */
#define SSEM        0x01000000U     /* Status stack empty */
#define LSOV        0x02000000U     /* Loop stacks overflow */
#define LSEM        0x04000000U     /* Loop stacks empty */

/* MODE1 flags */
#define MODE1_BR8       0x00000001U /* Bit-reverse for I8 */
#define MODE1_BR0       0x00000002U /* Bit-reverse for I0 */
#define MODE1_SRCU      0x00000004U /* Alternate register select for computational units */
#define MODE1_SRD1H     0x00000008U /* DAG alternate register select (7-4) */
#define MODE1_SRD1L     0x00000010U /* DAG alternate register select (3-0) */
#define MODE1_SRD2H     0x00000020U /* DAG alternate register select (15-12) */
#define MODE1_SRD2L     0x00000040U /* DAG alternate register select (11-8) */
#define MODE1_SRRFH     0x00000080U /* Register file alternate select for R(15-8) */
#define MODE1_SRRFL     0x00000400U /* Register file alternate select for R(7-0) */
#define MODE1_NESTM     0x00000800U /* Interrupt nesting enable */
#define MODE1_IRPTEN    0x00001000U /* Global interrupt enable */
#define MODE1_ALUSAT    0x00002000U /* Enable ALU fixed-point saturation */
#define MODE1_SSE       0x00004000U /* Enable short word sign extension */
#define MODE1_TRUNCATE  0x00008000U /* (1) Floating-point truncation / (0) round to nearest */
#define MODE1_RND32     0x00010000U /* (1) 32-bit floating-point rounding / (0) 40-bit rounding */
#define MODE1_CSEL      0x00060000U /* CSelect */

/* MODE2 flags */
#define MODE2_IRQ0E     0x00000001U /* IRQ0 (1) Edge sens. / (0) Level sens. */
#define MODE2_IRQ1E     0x00000002U /* IRQ1 (1) Edge sens. / (0) Level sens. */
#define MODE2_IRQ2E     0x00000004U /* IRQ2 (1) Edge sens. / (0) Level sens. */
#define MODE2_CADIS     0x00000010U /* Cache disable */
#define MODE2_TIMEN     0x00000020U /* Timer enable */
#define MODE2_BUSLK     0x00000040U /* External bus lock */
#define MODE2_FLG0O     0x00008000U /* FLAG0 (1) Output / (0) Input */
#define MODE2_FLG1O     0x00010000U /* FLAG1 (1) Output / (0) Input */
#define MODE2_FLG2O     0x00020000U /* FLAG2 (1) Output / (0) Input */
#define MODE2_FLG3O     0x00040000U /* FLAG3 (1) Output / (0) Input */
#define MODE2_CAFRZ     0x00080000U /* Cache freeze */

/* constants for IEEE754 single-precision float format */
#define FLOAT_CANONICAL_NAN     0xffffffffU
#define FLOAT_INFINITY          0x7f800000U
#define FLOAT_SIGN_MASK         0x80000000U
#define FLOAT_EXPONENT_MASK     0x7f800000U
#define FLOAT_MANTISSA_MASK     0x007fffffU
#define FLOAT_SIGN_SHIFT        31
#define FLOAT_EXPONENT_SHIFT    23
#define FLOAT_EXPONENT_BITS     8
#define FLOAT_EXPONENT_BIAS     127

#define SHARC_U64(hi, lo)       ((((UINT64)(hi)) << 32) | (UINT32)(lo))


/***************************************************************************
    STATE
***************************************************************************/

typedef union
{
	INT32 r;
	float f;
} SHARC_REG;

typedef struct
{
	UINT32 i[8];
	UINT32 m[8];
	UINT32 b[8];
	UINT32 l[8];
} SHARC_DAG;

typedef struct
{
	UINT32 addr;
	UINT32 code;
	UINT32 loop_type;
} SHARC_LADDR;

typedef struct
{
	UINT32 control;
	UINT32 int_index;
	UINT32 int_modifier;
	UINT32 int_count;
	UINT32 chain_ptr;
	UINT32 gen_purpose;
	UINT32 ext_index;
	UINT32 ext_modifier;
	UINT32 ext_count;
} SHARC_DMA_REGS;

typedef struct
{
	UINT32 src;
	UINT32 dst;
	UINT32 chain_ptr;
	INT32 src_modifier;
	INT32 dst_modifier;
	INT32 src_count;
	INT32 dst_count;
	INT32 pmode;
	INT32 chained_direction;
	INT32 active;
	INT32 chained;
} SHARC_DMA_OP;

typedef struct
{
	UINT32 mode1;
	UINT32 astat;
} SHARC_STATUS;

/* sharc_internal_state of MAME, interpreter fields; no pointers in here
   (it is saved as a whole) */
typedef struct
{
	SHARC_REG r[16];
	SHARC_REG reg_alt[16];

	UINT32 pc;
	UINT64 mrf;
	UINT64 mrb;

	UINT32 pcstack[30];
	UINT32 lcstack[6];
	UINT32 lastack[6];
	UINT32 lstkp;

	UINT32 faddr;
	UINT32 daddr;
	UINT32 pcstk;
	UINT32 pcstkp;
	SHARC_LADDR laddr;
	UINT32 curlcntr;
	UINT32 lcntr;
	UINT32 extdma_shift;
	UINT32 iop_write_num;
	UINT32 iop_data;

	/* Data Address Generator (DAG) */
	SHARC_DAG dag1;     /* (DM bus) */
	SHARC_DAG dag2;     /* (PM bus) */
	SHARC_DAG dag1_alt;
	SHARC_DAG dag2_alt;

	SHARC_DMA_REGS dma[12];

	/* System registers */
	UINT32 mode1;
	UINT32 mode2;
	UINT32 astat;
	UINT32 stky;
	UINT32 irptl;
	UINT32 imask;
	UINT32 imaskp;
	UINT32 ustat1;
	UINT32 ustat2;

	UINT32 flag[4];

	UINT32 syscon;
	UINT32 sysstat;

	SHARC_STATUS status_stack[5];
	INT32 status_stkp;

	UINT64 px;

	UINT64 opcode;

	UINT32 nfaddr;

	INT32 idle;
	INT32 irq_pending;
	INT32 active_irq_num;

	SHARC_DMA_OP dma_op[12];
	UINT32 dma_status;
	INT32 write_stalled;

	INT32 interrupt_active;

	UINT32 iop_delayed_reg;
	UINT32 iop_delayed_data;

	UINT32 delay_slot1, delay_slot2;

	INT32 systemreg_latency_cycles;
	INT32 systemreg_latency_reg;
	UINT32 systemreg_latency_data;
	UINT32 systemreg_previous_data;

	UINT32 astat_old;
	UINT32 astat_old_old;
	UINT32 astat_old_old_old;

	/* port: MAME's m_dm_short_view (selected = sign-extended short words) */
	INT32 dm_short_se;

	/* port: stall support */
	INT32 stalled;
	INT32 pf_rvalid;
	UINT32 pf_raddr;
	UINT32 pf_rdata;
	INT32 pf_wvalid;
	UINT32 pf_waddr;
} sharc_regs;

/* fetch-stage state saved before each instruction, to rewind it on a stall */
typedef struct
{
	UINT32 pc, daddr, faddr, nfaddr;
	UINT32 astat, astat_old, astat_old_old, astat_old_old_old;
	int loop;                   /* loop end was processed: the fields below are valid */
	UINT32 curlcntr, lcntr, lstkp, pcstk, pcstkp, stky;
	SHARC_LADDR laddr;
	UINT32 pcstack[30];
	UINT32 lcstack[6];
	UINT32 lastack[6];
} sharc_fetch_snapshot;

static sharc_regs core;
#define m_core (&core)

int sharc_icount;

/* internal memory: two blocks of 0x8000 32-bit words (MAME m_blocks) */
static UINT32 sharc_block[2][0x8000];

static struct sharc_config *sharc_cfg;
static mame_timer *sharc_iop_timer;
static int sharc_cpunum;
static int (*sharc_irq_callback)(int irqline);
static void (*sharc_op[512])(void);
static sharc_fetch_snapshot snap;


/***************************************************************************
    HELPERS
***************************************************************************/

#define REG(x)      (m_core->r[x].r)
#define UIREG(x)    ((UINT32)m_core->r[x].r)
#define FREG(x)     (m_core->r[x].f)

#ifndef BIT
#define BIT(x, n)       (((x) >> (n)) & 1)
#endif
#define BITS(x, n, w)   (((x) >> (n)) & ((1U << (w)) - 1))

static UINT32 make_bitmask32(int n)
{
	return (n >= 32) ? 0xffffffffU : ((1U << n) - 1);
}

/* util::sext */
static INT32 sext32(UINT32 v, int bits)
{
	if (bits >= 32)
		return (INT32)v;
	v &= (1U << bits) - 1;
	if (v & (1U << (bits - 1)))
		v |= ~((1U << bits) - 1);
	return (INT32)v;
}

/* std::rotl */
static UINT32 rotl32(UINT32 x, int s)
{
	unsigned r = (unsigned)s & 31;
	if (r == 0)
		return x;
	return (x << r) | (x >> (32 - r));
}

#define float_get_unbiased_exponent(f)  ((int)(((UINT32)(f) >> FLOAT_EXPONENT_SHIFT) & 0xff) - FLOAT_EXPONENT_BIAS)
#define float_make_biased_exponent(e)   ((((UINT32)((e) + FLOAT_EXPONENT_BIAS)) & 0xff) << FLOAT_EXPONENT_SHIFT)

/* opcode fields */
#define op_get_subop(op)        ((int)(((op) >> 40) & 0x1f))
#define op_get_compute(op)      ((UINT32)((op) & 0x7fffff))
#define op_get_cond(op)         ((int)(((op) >> 33) & 0x1f))
#define op_get_ureg_src(op)     ((int)(((op) >> 36) & 0xff))
#define op_get_cond_ureg(op)    ((int)(((op) >> 31) & 0x1f))
#define op_get_ureg_dst(op)     ((int)(((op) >> 23) & 0xff))
#define op_get_reladdr(op)      (sext32((UINT32)((op) >> 27), 6))
#define op_get_dmi(op)          ((int)(((op) >> 41) & 0x7))
#define op_get_dmm(op)          ((int)(((op) >> 38) & 0x7))
#define op_get_pmi(op)          ((int)(((op) >> 30) & 0x7))
#define op_get_pmm(op)          ((int)(((op) >> 27) & 0x7))
#define op_get_jump_b(op)       ((int)(((op) >> 39) & 1))
#define op_get_jump_la(op)      ((int)(((op) >> 38) & 1))
#define op_get_jump_j(op)       ((int)(((op) >> 26) & 1))
#define op_get_jump_e(op)       ((int)(((op) >> 25) & 1))
#define op_get_jump_ci(op)      ((int)(((op) >> 24) & 1))
#define op_get_rs(op)           ((int)(((op) >> 12) & 0xf))
#define op_get_ra(op)           ((int)(((op) >> 8) & 0xf))

static UINT32 laddr_pack(const SHARC_LADDR *l)
{
	return (l->loop_type << 30) | (l->code << 24) | l->addr;
}

static void laddr_unpack(SHARC_LADDR *l, UINT32 in)
{
	l->addr = in & 0xffffff;
	l->code = (in >> 24) & 0x1f;
	l->loop_type = (in >> 30) & 3;
}

static void sharc_error(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	logerror("%s", buf);
}

static void sharc_flag_out(int flag, int state)
{
	if (sharc_cfg && sharc_cfg->flag_out)
		sharc_cfg->flag_out(flag, state);
}

static void CHANGE_PC(UINT32 newpc)
{
	m_core->pc = newpc;
	m_core->daddr = newpc;
	m_core->faddr = newpc+1;
	m_core->nfaddr = newpc+2;
}

static void CHANGE_PC_DELAYED(UINT32 newpc)
{
	m_core->nfaddr = newpc;

	m_core->delay_slot1 = m_core->pc;
	m_core->delay_slot2 = m_core->daddr;
}

/* forward declarations */
static void check_interrupts(void);
static void systemreg_write_latency_effect(void);
static void sharc_dma_exec(int channel);


/***************************************************************************
    MEMORY
***************************************************************************/

/*
   48-bit program words in a 32-bit block (MAME pm_r / pm_w):
   0 012 0h 0l 1h
   1 453 2h 2l 1l
   2 678 3h 3l 4h
   3 ab9 5h 5l 4l
   4 cde 6h 6l 7h
*/

/* returns the block for an internal program address and its block offset, or NULL */
static UINT32 *pm_internal(UINT32 address, UINT32 *base, int *odd)
{
	UINT32 offset, slot;

	/* pgm_2m: 0x20000-0x24fff block 0, mirrors (0x18000) of it block 1 */
	if (address < 0x20000 || address >= 0x40000 || (address & 0x7fff) >= 0x5000)
		return NULL;

	offset = address & 0x7fff;
	slot = offset >> 12;
	*base = (offset & 0xfff) + (slot >> 1) * (3 << 12);
	*odd = slot & 1;
	return sharc_block[(address & 0x18000) ? 1 : 0];
}

static UINT64 pm_read48(UINT32 address)
{
	UINT32 base;
	int odd;
	UINT32 *blk;

	address &= 0xffffff;
	blk = pm_internal(address, &base, &odd);
	if (blk)
	{
		if (odd)
			return ((UINT64)blk[base + 0x2000] << 16) | (blk[base + 0x1000] & 0xffff);
		else
			return ((UINT64)blk[base] << 16) | (blk[base + 0x1000] >> 16);
	}

	if (sharc_cfg && sharc_cfg->pgm_r)
		return sharc_cfg->pgm_r(address);
	return 0;
}

static void pm_write48(UINT32 address, UINT64 data)
{
	UINT32 base;
	int odd;
	UINT32 *blk;

	address &= 0xffffff;
	blk = pm_internal(address, &base, &odd);
	if (blk)
	{
		if (odd)
		{
			blk[base + 0x1000] = (blk[base + 0x1000] & 0xffff0000) | (UINT32)(data & 0xffff);
			blk[base + 0x2000] = (UINT32)(data >> 16);
		}
		else
		{
			blk[base] = (UINT32)(data >> 16);
			blk[base + 0x1000] = (blk[base + 0x1000] & 0xffff) | ((UINT32)(data & 0xffff) << 16);
		}
		return;
	}

	if (sharc_cfg && sharc_cfg->pgm_w)
		sharc_cfg->pgm_w(address, data & SHARC_U64(0xffff, 0xffffffff));
}

/* When PM bus is used to transfer 32-bit data, it is aligned to the upper 32 bits of the bus */
static UINT32 pm_read32(UINT32 address)
{
	return (UINT32)(pm_read48(address) >> 16);
}

static void pm_write32(UINT32 address, UINT32 data)
{
	UINT32 base;
	int odd;
	UINT32 *blk;

	/* MAME writes with mem_mask 0x0000ffffffff0000: the low 16 bits are kept */
	address &= 0xffffff;
	blk = pm_internal(address, &base, &odd);
	if (blk)
	{
		if (odd)
			blk[base + 0x2000] = data;
		else
			blk[base] = data;
		return;
	}

	if (sharc_cfg && sharc_cfg->pgm_w)
		sharc_cfg->pgm_w(address, (UINT64)data << 16);
}

static UINT32 iop_r(UINT32 offset);
static void iop_w(UINT32 offset, UINT32 data);

/* data space addresses handled by the driver callbacks */
static int dm_is_external(UINT32 address)
{
	return !(address < 0x100 || (address >= 0x20000 && address < 0x80000));
}

static UINT32 dm_read32(UINT32 address)
{
	if (m_core->pf_rvalid && address == m_core->pf_raddr)
	{
		/* already read by DM_PRE_R */
		m_core->pf_rvalid = 0;
		return m_core->pf_rdata;
	}

	if (address < 0x100)
		return iop_r(address);

	if (address >= 0x20000 && address < 0x40000)
	{
		/* data_2m: 0x20000-0x27fff block 0, mirrors (0x18000) of it block 1 */
		return sharc_block[(address & 0x18000) ? 1 : 0][address & 0x7fff];
	}

	if (address >= 0x40000 && address < 0x80000)
	{
		/* short words: 0x40000-0x4ffff block 0, mirrors (0x30000) of it block 1 */
		UINT32 half = address & 0xffff;
		UINT32 word = sharc_block[(address & 0x30000) ? 1 : 0][half >> 1];
		UINT32 v = (half & 1) ? (word >> 16) : (word & 0xffff);
		if (m_core->dm_short_se)
			v = (UINT32)(INT32)(INT16)v;
		return v;
	}

	if (sharc_cfg && sharc_cfg->data_r)
		return sharc_cfg->data_r(address);
	return 0;
}

static void dm_write32(UINT32 address, UINT32 data)
{
	if (m_core->pf_wvalid && address == m_core->pf_waddr)
	{
		/* already written by DM_PRE_W */
		m_core->pf_wvalid = 0;
		return;
	}

	if (address < 0x100)
	{
		iop_w(address, data);
		return;
	}

	if (address >= 0x20000 && address < 0x40000)
	{
		sharc_block[(address & 0x18000) ? 1 : 0][address & 0x7fff] = data;
		return;
	}

	if (address >= 0x40000 && address < 0x80000)
	{
		UINT32 half = address & 0xffff;
		UINT32 *word = &sharc_block[(address & 0x30000) ? 1 : 0][half >> 1];
		if (half & 1)
			*word = (*word & 0x0000ffff) | ((data & 0xffff) << 16);
		else
			*word = (*word & 0xffff0000) | (data & 0xffff);
		return;
	}

	if (sharc_cfg && sharc_cfg->data_w)
		sharc_cfg->data_w(address, data);
}

/* stall support: external DM accesses are done first by the opcodes;
   return 1 if the driver stalled the access */
static int dm_pre_r(UINT32 address)
{
	UINT32 data;

	if (!dm_is_external(address) || !sharc_cfg || !sharc_cfg->data_r)
		return 0;

	m_core->stalled = 0;
	data = sharc_cfg->data_r(address);
	if (m_core->stalled)
		return 1;

	m_core->pf_rvalid = 1;
	m_core->pf_raddr = address;
	m_core->pf_rdata = data;
	return 0;
}

static int dm_pre_w(UINT32 address, UINT32 data)
{
	if (!dm_is_external(address) || !sharc_cfg || !sharc_cfg->data_w)
		return 0;

	m_core->stalled = 0;
	sharc_cfg->data_w(address, data);
	if (m_core->stalled)
		return 1;

	m_core->pf_wvalid = 1;
	m_core->pf_waddr = address;
	return 0;
}


/***************************************************************************
    IOP REGISTERS
***************************************************************************/

static void sharc_iop_delayed_write_callback(int param)
{
	switch (m_core->iop_delayed_reg)
	{
		case 0x1c:
		{
			if (m_core->iop_delayed_data & 0x1)
			{
				sharc_dma_exec(6);
			}
			break;
		}

		case 0x1d:
		{
			if (m_core->iop_delayed_data & 0x1)
			{
				sharc_dma_exec(7);
			}
			break;
		}

		default:
			sharc_error("SHARC: sharc_iop_delayed_write: unknown IOP register %02X\n", m_core->iop_delayed_reg);
			break;
	}

	timer_adjust(sharc_iop_timer, TIME_NEVER, 0, 0);
}

static void sharc_iop_delayed_w(UINT32 reg, UINT32 data, int cycles)
{
	m_core->iop_delayed_reg = reg;
	m_core->iop_delayed_data = data;

	timer_adjust(sharc_iop_timer, TIME_IN_CYCLES(cycles, sharc_cpunum), 0, 0);
}

static UINT32 iop_r(UINT32 offset)
{
	switch (offset)
	{
		case 0x00: return 0;    /* System configuration */

		case 0x37:      /* DMA status */
		{
			return m_core->dma_status;
		}
		default:
			sharc_error("SHARC: sharc_iop_r: Unimplemented IOP reg %02X at %08X\n", offset, m_core->pc);
			return 0;
	}
}

static void iop_w(UINT32 offset, UINT32 data)
{
	switch (offset)
	{
		case 0x00: m_core->syscon = data; break;
		case 0x02: break;       /* External Memory Wait State Configuration */
		case 0x04: /* External port DMA buffer 0 */
		{
			sharc_external_dma_write(m_core->extdma_shift, data);
			m_core->extdma_shift++;
			if (m_core->extdma_shift == 3)
				m_core->extdma_shift = 0;
			break;
		}

		case 0x08: break;       /* Message Register 0 */
		case 0x09: break;       /* Message Register 1 */
		case 0x0a: break;       /* Message Register 2 */
		case 0x0b: break;       /* Message Register 3 */
		case 0x0c: break;       /* Message Register 4 */
		case 0x0d: break;       /* Message Register 5 */
		case 0x0e: break;       /* Message Register 6 */
		case 0x0f: break;       /* Message Register 7 */

		case 0x14: /* reserved??? written by Last Bronx */
		case 0x17: break;

		/* DMA 6 */
		case 0x1c:
		{
			m_core->dma[6].control = data;
			if (data & 0x1)
			{
				sharc_iop_delayed_w(0x1c, data, 1);
			}
			break;
		}

		case 0x20: break;

		case 0x40: m_core->dma[6].int_index = data; return;
		case 0x41: m_core->dma[6].int_modifier = data; return;
		case 0x42: m_core->dma[6].int_count = data; return;
		case 0x43: m_core->dma[6].chain_ptr = data; return;
		case 0x44: m_core->dma[6].gen_purpose = data; return;
		case 0x45: m_core->dma[6].ext_index = data; return;
		case 0x46: m_core->dma[6].ext_modifier = data; return;
		case 0x47: m_core->dma[6].ext_count = data; return;

		/* DMA 7 */
		case 0x1d:
		{
			m_core->dma[7].control = data;
			if (data & 0x1)
			{
				sharc_iop_delayed_w(0x1d, data, 30);
			}
			break;
		}

		case 0x48: m_core->dma[7].int_index = data; return;
		case 0x49: m_core->dma[7].int_modifier = data; return;
		case 0x4a: m_core->dma[7].int_count = data; return;
		case 0x4b: m_core->dma[7].chain_ptr = data; return;
		case 0x4c: m_core->dma[7].gen_purpose = data; return;
		case 0x4d: m_core->dma[7].ext_index = data; return;
		case 0x4e: m_core->dma[7].ext_modifier = data; return;
		case 0x4f: m_core->dma[7].ext_count = data; return;

		default:
			sharc_error("SHARC: sharc_iop_w: Unimplemented IOP reg %02X, %08X at %08X\n", offset, data, m_core->pc);
			break;
	}
}


#include "sharcdma.c"
/* port: set by a jump to itself on a FLAG input condition (a wait on the
   outside, see sharc_execute) */
static int sharc_flag_spin;

static int sharc_cond_is_flag(int cond)
{
	return (cond >= 0x09 && cond <= 0x0c) || (cond >= 0x19 && cond <= 0x1c);
}

#include "sharcops.c"


/***************************************************************************
    OPCODE TABLE
***************************************************************************/

static void build_opcode_table(void)
{
	int i, j;
	int num_ops = sizeof(s_sharc_opcode_table) / sizeof(s_sharc_opcode_table[0]);

	for (i = 0; i < 512; i++)
	{
		UINT16 op = (UINT16)(i << 7);

		sharc_op[i] = sharcop_unimplemented;

		j = 0;
		while (j < num_ops)
		{
			const SHARC_OP *opcode = &s_sharc_opcode_table[j++];
			if ((opcode->op_mask & op) == opcode->op_bits)
			{
				sharc_op[i] = opcode->handler;
				break;
			}
		}
		while (j < num_ops)
		{
			const SHARC_OP *opcode = &s_sharc_opcode_table[j++];
			if ((opcode->op_mask & op) == opcode->op_bits)
			{
				sharc_error("SHARC: build_opcode_table: table already filled! (i=%04X, j=%d)\n", i, j);
			}
		}
	}
}


/***************************************************************************
    HOST INTERFACE
***************************************************************************/

void sharc_external_iop_write(UINT32 address, UINT32 data)
{
	/* host packing mode used to determine width of external host bus width (16/32)
	   if writing to external port DMA buffer, just write the data directly;
	   external_dma_write() handles packed data */
	if (m_core->syscon & 0x10 && address != 0x04)
	{
		if ((m_core->iop_write_num++ & 1) == 0)
		{
			m_core->iop_data = data & 0xffff;
			return;
		}
		else
		{
			m_core->iop_data |= (data & 0xffff) << 16;
		}
	}
	else
	{
		m_core->iop_data = data;
	}

	if (address == 0x1c)
	{
		m_core->dma[6].control = m_core->iop_data;
	}
	else
	{
		dm_write32(address, m_core->iop_data);
	}
}

void sharc_external_dma_write(UINT32 address, UINT64 data)
{
	/*
	All addresses in the 17-bit index registers are offset by 0x0002 0000, the
	first internal RAM location, before they are used by the DMA controller.
	*/

	UINT32 index = (m_core->dma[6].int_index & 0x1ffff) | 0x20000;
	unsigned mswf = BIT(m_core->dma[6].control, 8);
	unsigned pmode = BITS(m_core->dma[6].control, 6, 2);
	unsigned dtype = BIT(m_core->dma[6].control, 5);
	switch (pmode)
	{
		case 0:         /* no packing */
		{
			if (dtype)
				pm_write32(index, (UINT32)data);
			else
				dm_write32(index, (UINT32)data);

			m_core->dma[6].int_index += m_core->dma[6].int_modifier;
			break;
		}
		case 2:         /* 16/48 packing */
		{
			/* FIXME: honour DTYPE */
			unsigned word = address % 3;
			unsigned shift = (mswf ? (2 - word) : word) * 16;

			UINT64 r = pm_read48(index);
			r &= ~((UINT64)0xffff << shift);
			r |= (data & 0xffff) << shift;

			pm_write48(index, r);

			if (word == 2)
			{
				m_core->dma[6].int_index += m_core->dma[6].int_modifier;
			}
			break;
		}
		default:
		{
			/* MAME: fatal error */
			sharc_error("SHARC: sharc_external_dma_write: unimplemented packing mode %d\n", pmode);
			break;
		}
	}
}

void sharc_set_flag_input(int flag_num, int state)
{
	if (flag_num < 0 || flag_num > 3)
		return;

	/* Check if flag is set to input in MODE2 (bit == 0) */
	if (BIT(m_core->mode2, flag_num + 15))
	{
		/* MAME: fatal error */
		sharc_error("SHARC: sharc_set_flag_input: flag %d is set output!\n", flag_num);
		return;
	}

	m_core->flag[flag_num] = state ? 1 : 0;
}

void sharc_write_stall(int state)
{
	m_core->write_stalled = state ? 1 : 0;
}

void sharc_stall(void)
{
	m_core->stalled = 1;
}


/***************************************************************************
    EXECUTION
***************************************************************************/

static void check_interrupts(void)
{
	if ((m_core->imask & m_core->irq_pending) && (m_core->mode1 & MODE1_IRPTEN) && !m_core->interrupt_active &&
		m_core->pc != m_core->delay_slot1 && m_core->pc != m_core->delay_slot2)
	{
		int which = 0;
		int i;
		for (i = 0; i < 32; i++)
		{
			if (BIT((UINT32)m_core->irq_pending, i))
				break;
			which++;
		}

		PUSH_PC();
		if (m_core->idle)
			m_core->pcstk = m_core->pc + 1;
		else
			m_core->pcstk = m_core->daddr;

		m_core->irptl |= 1U << which;

		/* TODO: timer and VIRPT interrupts also push the status stack */
		if (which >= 6 && which <= 8)
			PUSH_STATUS_STACK();

		CHANGE_PC(0x20000 + (which * 0x4));

		/* TODO: alter IMASKP */

		m_core->active_irq_num = which;
		m_core->irq_pending &= ~(1 << which);

		m_core->interrupt_active = 1;

		/* port: acknowledge the IRQ0-2 pins (for HOLD_LINE) */
		if (which >= 6 && which <= 8 && sharc_irq_callback)
			(*sharc_irq_callback)(8 - which);
	}
}

static void snapshot_loop_state(void)
{
	snap.loop = 1;
	snap.curlcntr = m_core->curlcntr;
	snap.lcntr = m_core->lcntr;
	snap.lstkp = m_core->lstkp;
	snap.pcstk = m_core->pcstk;
	snap.pcstkp = m_core->pcstkp;
	snap.stky = m_core->stky;
	snap.laddr = m_core->laddr;
	memcpy(snap.pcstack, m_core->pcstack, sizeof(snap.pcstack));
	memcpy(snap.lcstack, m_core->lcstack, sizeof(snap.lcstack));
	memcpy(snap.lastack, m_core->lastack, sizeof(snap.lastack));
}

/* undo the fetch stage of a stalled instruction */
static void rewind_instruction(void)
{
	m_core->pc = snap.pc;
	m_core->daddr = snap.daddr;
	m_core->faddr = snap.faddr;
	m_core->nfaddr = snap.nfaddr;
	m_core->astat = snap.astat;
	m_core->astat_old = snap.astat_old;
	m_core->astat_old_old = snap.astat_old_old;
	m_core->astat_old_old_old = snap.astat_old_old_old;

	if (snap.loop)
	{
		m_core->curlcntr = snap.curlcntr;
		m_core->lcntr = snap.lcntr;
		m_core->lstkp = snap.lstkp;
		m_core->pcstk = snap.pcstk;
		m_core->pcstkp = snap.pcstkp;
		m_core->stky = snap.stky;
		m_core->laddr = snap.laddr;
		memcpy(m_core->pcstack, snap.pcstack, sizeof(snap.pcstack));
		memcpy(m_core->lcstack, snap.lcstack, sizeof(snap.lcstack));
		memcpy(m_core->lastack, snap.lastack, sizeof(snap.lastack));
	}
}

int sharc_execute(int cycles)
{
	sharc_icount = cycles;

	if (m_core->write_stalled)
		sharc_icount = 0;

	if (m_core->idle && m_core->irq_pending == 0)
	{
		int dma_count = sharc_icount;

		/* run active DMAs even while idling */
		while ((dma_count > 0) && (m_core->dma_status & ((1 << 6) | (1 << 7))))
		{
			if (!m_core->write_stalled)
			{
				int dma_stalled = dma_run_cycle(6);
				dma_stalled |= dma_run_cycle(7);
				if (dma_stalled)
					break;
			}
			dma_count--;
		}

		sharc_icount = 0;
	}
	if (m_core->irq_pending != 0)
	{
		check_interrupts();
		m_core->idle = 0;
	}

	while (sharc_icount > 0 && !m_core->idle && !m_core->write_stalled)
	{
		int dma_stalled = 0;
		int flag_wait = 0;

		snap.pc = m_core->pc;
		snap.daddr = m_core->daddr;
		snap.faddr = m_core->faddr;
		snap.nfaddr = m_core->nfaddr;
		snap.astat = m_core->astat;
		snap.astat_old = m_core->astat_old;
		snap.astat_old_old = m_core->astat_old_old;
		snap.astat_old_old_old = m_core->astat_old_old_old;
		snap.loop = 0;

		m_core->pc = m_core->daddr;
		m_core->daddr = m_core->faddr;
		m_core->faddr = m_core->nfaddr;
		m_core->nfaddr++;

		m_core->astat_old_old_old = m_core->astat_old_old;
		m_core->astat_old_old = m_core->astat_old;
		m_core->astat_old = m_core->astat;

		CALL_MAME_DEBUG;

		m_core->opcode = pm_read48(m_core->pc);

		/* handle looping */
		if (!(m_core->stky & LSEM) && (m_core->pc == m_core->laddr.addr))
		{
			snapshot_loop_state();

			switch (m_core->laddr.loop_type)
			{
			case 0:     /* arithmetic condition-based */
			{
				if ((m_core->pc - TOP_PC()) > 2)
				{
					m_core->astat = m_core->astat_old_old_old;
				}

				if (DO_CONDITION_CODE(m_core->laddr.code))
				{
					POP_LOOP();
					POP_PC();
				}
				else
				{
					/* port: a one-instruction NOP loop until a FLAG input
					   changes waits on the outside (the Model 2B FIFO flags).
					   No other CPU runs during this timeslice, so the flag
					   cannot change before it ends: give it up below */
					if (m_core->opcode == 0 && TOP_PC() == m_core->pc && sharc_cond_is_flag(m_core->laddr.code))
						flag_wait = 1;
					CHANGE_PC(TOP_PC());
				}

				m_core->astat = m_core->astat_old;
				break;
			}
			case 1:     /* counter-based, length 1 */
			case 2:     /* counter-based, length 2 */
			case 3:     /* counter-based, length >2 */
			{
				--m_core->curlcntr;
				if (m_core->curlcntr == 0)
				{
					POP_LOOP();
					POP_PC();
				}
				else
				{
					CHANGE_PC(TOP_PC());
				}
			}
			}
		}

		m_core->stalled = 0;
		sharc_flag_spin = 0;

		(*sharc_op[(m_core->opcode >> 39) & 0x1ff])();

		if (sharc_flag_spin)
			flag_wait = 1;

		m_core->pf_rvalid = 0;
		m_core->pf_wvalid = 0;

		if (m_core->stalled)
		{
			/* port: a driver callback could not complete the access;
			   nothing was committed: rewind and give up the timeslice */
			m_core->stalled = 0;
			rewind_instruction();
			sharc_icount = 0;
			break;
		}

		/* System register latency effect */
		if (m_core->systemreg_latency_cycles > 0)
		{
			--m_core->systemreg_latency_cycles;
			if (m_core->systemreg_latency_cycles <= 0)
			{
				systemreg_write_latency_effect();
			}
		}

		if (!m_core->write_stalled)
		{
			dma_stalled = dma_run_cycle(6);
			dma_stalled |= dma_run_cycle(7);
		}

		--sharc_icount;

		/* port: a stalled DMA transfer gives up the timeslice too */
		if (dma_stalled)
			sharc_icount = 0;

		/* port: waiting on a FLAG input (a NOP loop, see above, or a jump to
		   itself): the rest of the timeslice is the same wait; only the DMA
		   keeps running */
		if (flag_wait && sharc_icount > 0)
		{
			while (sharc_icount > 0 && (m_core->dma_status & ((1 << 6) | (1 << 7))) && !m_core->write_stalled)
			{
				dma_stalled = dma_run_cycle(6);
				dma_stalled |= dma_run_cycle(7);
				if (dma_stalled)
					break;
				--sharc_icount;
			}
			sharc_icount = 0;
		}
	}

	return cycles - sharc_icount;
}


/***************************************************************************
    CPU INTERFACE
***************************************************************************/

void sharc_init(void)
{
	int cpu = cpu_getactivecpu();

	sharc_cpunum = cpu;

	memset(&core, 0, sizeof(core));
	memset(sharc_block, 0, sizeof(sharc_block));
	memset(&snap, 0, sizeof(snap));

	build_opcode_table();

	sharc_iop_timer = timer_alloc(sharc_iop_delayed_write_callback);
	timer_adjust(sharc_iop_timer, TIME_NEVER, 0, 0);

	/* the whole state struct (no pointers in it) and the internal memory */
	state_save_register_UINT8("sharc", cpu, "core", (UINT8 *)&core, sizeof(core));
	state_save_register_UINT32("sharc", cpu, "block0", sharc_block[0], 0x8000);
	state_save_register_UINT32("sharc", cpu, "block1", sharc_block[1], 0x8000);
}

void sharc_reset(void *param)
{
	int boot_mode;

	if (param)
		sharc_cfg = (struct sharc_config *)param;

	boot_mode = sharc_cfg ? sharc_cfg->boot_mode : SHARC_BOOT_MODE_HOST;

	memset(sharc_block, 0, sizeof(sharc_block));

	switch (boot_mode)
	{
		case SHARC_BOOT_MODE_EPROM:
		{
			m_core->dma[6].int_index      = 0x20000;
			m_core->dma[6].int_modifier   = 1;
			m_core->dma[6].int_count      = 0x100;
			m_core->dma[6].ext_index      = 0x400000;
			m_core->dma[6].ext_modifier   = 1;
			m_core->dma[6].ext_count      = 0x600;
			m_core->dma[6].control        = 0x2a1;

			sharc_dma_exec(6);
			dma_op(6);
			break;
		}

		case SHARC_BOOT_MODE_HOST:
		{
			m_core->dma[6].int_index      = 0x20000;
			m_core->dma[6].int_modifier   = 1;
			m_core->dma[6].int_count      = 0x100;
			m_core->dma[6].ext_index      = 0x400000;
			m_core->dma[6].ext_modifier   = 1;
			m_core->dma[6].ext_count      = 0x600;
			m_core->dma[6].control        = 0xa1;
			break;
		}

		default:
			/* MAME: fatal error */
			sharc_error("SHARC: Unimplemented boot mode %d\n", boot_mode);
			break;
	}

	m_core->pc = 0x20004;
	m_core->extdma_shift = 0;
	m_core->daddr = m_core->pc + 1;
	m_core->faddr = m_core->daddr + 1;
	m_core->nfaddr = m_core->faddr+1;

	m_core->idle = 0;
	m_core->mode1 = 0x00000000;
	m_core->mode2 &= 0xf0000000;
	m_core->astat &= FLG0 | FLG1 | FLG2 | FLG3;
	m_core->stky = PCEM | SSEM | LSEM;
	m_core->irptl = 0x0000;
	m_core->imask = 0x0003;
	m_core->ustat1 = 0x0000;
	m_core->ustat2 = 0x0000;

	m_core->pcstkp = 0;
	m_core->lstkp = 0;
	m_core->pcstk = 0x00ffffff;
	m_core->curlcntr = 0xffffffff;
	m_core->lcntr = m_core->lcstack[0];
	laddr_unpack(&m_core->laddr, 0xffffffff);
	m_core->status_stkp = 0;
	m_core->interrupt_active = 0;

	m_core->syscon = 0x00000010;
	m_core->sysstat &= 0x00000ff0;
	m_core->iop_write_num = 0;
	m_core->iop_data = 0;

	m_core->stalled = 0;
	m_core->pf_rvalid = 0;
	m_core->pf_wvalid = 0;
}

void sharc_exit(void)
{
}

/* single instance: the static state is always the live one. get_context
   gives a copy (for the debugger / info); set_context never copies
   back, so a stale buffer cannot clobber changes made through the host
   helpers while another CPU was active */
unsigned sharc_get_context(void *dst)
{
	if (dst)
		*(sharc_regs *)dst = core;
	return sizeof(sharc_regs);
}

void sharc_set_context(void *src)
{
}

unsigned sharc_get_reg(int regnum)
{
	switch (regnum)
	{
	case REG_PC:
	case REG_PREVIOUSPC:
	case SHARC_PC:          return m_core->pc;
	case REG_SP:
	case SHARC_PCSTKP:      return m_core->pcstkp;
	case SHARC_PCSTK:       return m_core->pcstk;
	case SHARC_LSTKP:       return m_core->lstkp;
	case SHARC_FADDR:       return m_core->faddr;
	case SHARC_DADDR:       return m_core->daddr;
	case SHARC_MODE1:       return m_core->mode1;
	case SHARC_MODE2:       return m_core->mode2;
	case SHARC_ASTAT:       return m_core->astat;
	case SHARC_STKY:        return m_core->stky;
	case SHARC_IRPTL:       return m_core->irptl;
	case SHARC_IMASK:       return m_core->imask;
	case SHARC_USTAT1:      return m_core->ustat1;
	case SHARC_USTAT2:      return m_core->ustat2;
	case SHARC_LCNTR:       return m_core->lcntr;
	case SHARC_CURLCNTR:    return m_core->curlcntr;
	case SHARC_STSTKP:      return (unsigned)m_core->status_stkp;
	}
	if (regnum >= SHARC_R0 && regnum <= SHARC_R15)
		return (unsigned)m_core->r[regnum - SHARC_R0].r;
	if (regnum >= SHARC_I0 && regnum <= SHARC_I15)
	{
		int i = regnum - SHARC_I0;
		return (i < 8) ? m_core->dag1.i[i] : m_core->dag2.i[i & 7];
	}
	return 0;
}

void sharc_set_reg(int regnum, unsigned val)
{
	switch (regnum)
	{
	case REG_PC:
	case SHARC_PC:          CHANGE_PC(val & 0xffffff); return;
	case SHARC_MODE1:       m_core->mode1 = val; return;
	case SHARC_MODE2:       m_core->mode2 = val; return;
	case SHARC_ASTAT:       m_core->astat = val; return;
	case SHARC_STKY:        m_core->stky = val; return;
	case SHARC_IRPTL:       m_core->irptl = val; return;
	case SHARC_IMASK:       m_core->imask = val; return;
	case SHARC_USTAT1:      m_core->ustat1 = val; return;
	case SHARC_USTAT2:      m_core->ustat2 = val; return;
	}
	if (regnum >= SHARC_R0 && regnum <= SHARC_R15)
		m_core->r[regnum - SHARC_R0].r = (INT32)val;
	else if (regnum >= SHARC_I0 && regnum <= SHARC_I15)
	{
		int i = regnum - SHARC_I0;
		if (i < 8)
			m_core->dag1.i[i] = val;
		else
			m_core->dag2.i[i & 7] = val;
	}
}

void sharc_set_irq_line(int irqline, int state)
{
	/* as MAME execute_set_input: the IRQ0-2 pins are latched while
	   asserted (MODE2 edge/level sensitivity is not emulated by MAME) */
	if (irqline >= 0 && irqline <= 2)
	{
		if (state != CLEAR_LINE)
		{
			m_core->irq_pending |= 1 << (8-irqline);
		}
		else
		{
			m_core->irq_pending &= ~(1 << (8-irqline));
		}
	}
}

void sharc_set_irq_callback(int (*callback)(int irqline))
{
	sharc_irq_callback = callback;
}

const char *sharc_info(void *context, int regnum)
{
	static char buffer[ 8 ][ 47 + 1 ];
	static int which = 0;
	sharc_regs *r = context ? (sharc_regs *)context : &core;

	which = ( which + 1 ) % 8;
	buffer[ which ][ 0 ] = '\0';

	switch( regnum )
	{
	case CPU_INFO_REG + SHARC_PC:       sprintf( buffer[ which ], "PC:%06X", r->pc ); break;
	case CPU_INFO_REG + SHARC_PCSTK:    sprintf( buffer[ which ], "PCSTK:%06X", r->pcstk ); break;
	case CPU_INFO_REG + SHARC_PCSTKP:   sprintf( buffer[ which ], "PCSTKP:%02X", r->pcstkp ); break;
	case CPU_INFO_REG + SHARC_LSTKP:    sprintf( buffer[ which ], "LSTKP:%X", r->lstkp ); break;
	case CPU_INFO_REG + SHARC_FADDR:    sprintf( buffer[ which ], "FADDR:%08X", r->faddr ); break;
	case CPU_INFO_REG + SHARC_DADDR:    sprintf( buffer[ which ], "DADDR:%08X", r->daddr ); break;
	case CPU_INFO_REG + SHARC_MODE1:    sprintf( buffer[ which ], "MODE1:%08X", r->mode1 ); break;
	case CPU_INFO_REG + SHARC_MODE2:    sprintf( buffer[ which ], "MODE2:%08X", r->mode2 ); break;
	case CPU_INFO_REG + SHARC_ASTAT:    sprintf( buffer[ which ], "ASTAT:%08X", r->astat ); break;
	case CPU_INFO_REG + SHARC_STKY:     sprintf( buffer[ which ], "STKY:%08X", r->stky ); break;
	case CPU_INFO_REG + SHARC_IRPTL:    sprintf( buffer[ which ], "IRPTL:%08X", r->irptl ); break;
	case CPU_INFO_REG + SHARC_IMASK:    sprintf( buffer[ which ], "IMASK:%08X", r->imask ); break;
	case CPU_INFO_REG + SHARC_USTAT1:   sprintf( buffer[ which ], "USTAT1:%08X", r->ustat1 ); break;
	case CPU_INFO_REG + SHARC_USTAT2:   sprintf( buffer[ which ], "USTAT2:%08X", r->ustat2 ); break;
	case CPU_INFO_REG + SHARC_LCNTR:    sprintf( buffer[ which ], "LCNTR:%08X", r->lcntr ); break;
	case CPU_INFO_REG + SHARC_CURLCNTR: sprintf( buffer[ which ], "CURLCNTR:%08X", r->curlcntr ); break;
	case CPU_INFO_REG + SHARC_STSTKP:   sprintf( buffer[ which ], "STSTKP:%X", (unsigned)r->status_stkp ); break;
	case CPU_INFO_FLAGS:                sprintf( buffer[ which ], "%08X", r->astat ); break;
	case CPU_INFO_NAME:                 return "ADSP21062";
	case CPU_INFO_FAMILY:               return "Analog Devices SHARC";
	case CPU_INFO_VERSION:              return "3.0";
	case CPU_INFO_FILE:                 return __FILE__;
	case CPU_INFO_CREDITS:              return "Ville Linde (current MAME)";
	default:
		if (regnum >= CPU_INFO_REG + SHARC_R0 && regnum <= CPU_INFO_REG + SHARC_R15)
			sprintf( buffer[ which ], "R%d:%08X", regnum - (CPU_INFO_REG + SHARC_R0), (UINT32)r->r[regnum - (CPU_INFO_REG + SHARC_R0)].r );
		else if (regnum >= CPU_INFO_REG + SHARC_I0 && regnum <= CPU_INFO_REG + SHARC_I15)
		{
			int i = regnum - (CPU_INFO_REG + SHARC_I0);
			sprintf( buffer[ which ], "I%d:%08X", i, (i < 8) ? r->dag1.i[i] : r->dag2.i[i & 7] );
		}
		break;
	}
	return buffer[ which ];
}

unsigned sharc_dasm(char *buffer, unsigned pc)
{
	/* no disassembler: the raw 48-bit opcode */
	UINT64 op = pm_read48(pc);
	sprintf( buffer, "$%04X%08X", (UINT32)(op >> 32) & 0xffff, (UINT32)op );
	return 1;
}
