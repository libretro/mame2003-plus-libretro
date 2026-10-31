/***************************************************************************

    sh2jit.c

    Optional recompiler for the SH-2, built on the bundled sljit
    (src/lib/sljit/sljit_mame.h; included by sh2.c).

    Straight-line runs of instructions, ending at an unconditional jump
    (with its delay slot) or at the first instruction left to the
    interpreter, become native code. A block only runs when the
    timeslice has enough cycles left for the whole of it (taking every
    conditional branch), so every instruction it runs would have run in
    the interpreter too, with the same cycles: the cycles of each
    instruction are exactly the interpreter's.

    - The usual ALU, shift, compare, move and multiply instructions and
      all the branches are native code. The rarer ALU instructions
      (ADDC, SUBC, ADDV, SUBV, NEGC, DIV0S, DIV1, DMULS, DMULU, ROTCL,
      ROTCR, CMP/STR) call the interpreter for that instruction.
    - Loads and stores go straight to the fast RAM the driver declares
      (sh2_jit_add_fastram). Any other address, a misaligned access, an
      address a CPU has a memory handler on, or a store over translated
      code leaves the block just before that instruction ("side exit")
      and the interpreter runs it, with its memory handlers, cycles and
      timing. MAC.L, MAC.W, AND.B/OR.B/XOR.B/TST.B @(R0,GBR) and TAS.B
      call the interpreter once the block has checked their addresses.
    - LDC to SR, RTE, TRAPA and SLEEP (they change the interrupt state or
      the flow) and the two busy loops the interpreter shortens (BRA $,
      DT / BF $-2) end the block: the interpreter runs them. A delay slot
      is only translated when it is a plain instruction that does not
      read the PC.

    Blocks only start with no delay slot pending and no interrupt test
    pending. Only code in fast RAM is translated. Writes over translated
    code flush the translation cache, whoever makes them: the
    interpreter (WB/WW/WL), the SH-2 DMA, the driver's DMA (through
    sh2_jit_ram_written), and a state load.

***************************************************************************/

/* the sljit build shared by the recompilers decides where this one runs */
#include "../../lib/sljit/sljit_mame.h"

#define SH2JIT_SUPPORTED		SLJIT_MAME_SUPPORTED

int sh2_jit_available(void)
{
	return SH2JIT_SUPPORTED;
}

#if SH2JIT_SUPPORTED

#include <stddef.h>

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define JIT_HOST64		1
#else
#define JIT_HOST64		0
#endif

#define JIT_TABLE_BITS		16
#define JIT_TABLE_SIZE		(1 << JIT_TABLE_BITS)
#define JIT_MAX_BLOCK		64				/* instructions */
#define JIT_MAX_EXITS		(JIT_MAX_BLOCK * 4)
#define JIT_MAX_FASTRAM		8
#define JIT_CODE_BUDGET		(32 * 1024 * 1024)

/* fast RAM granules of 4 bytes: which hold translated code */

struct jit_fastram
{
	offs_t		start, end;			/* SH-2 addresses (masked with AM) */
	UINT8 *		base;				/* host memory of 'start', as the memory system keeps it */
	int			readonly;
	UINT32		mapoff;				/* first granule of the region in the maps */
};

struct jit_block
{
	void *		code;
	void *		entry;				/* where a block leaving for this one jumps in (after the prologue), NULL if none */
	UINT32		pc;					/* address of the first instruction */
	UINT16		cost;				/* most cycles the block can take, 0 = not translatable */
	UINT8		valid;
	UINT8		len;				/* instructions */
	UINT8		idle;				/* the SGL idle counting loop starts here (see jit_idle_skip) */
	UINT16		op0;				/* first opcode, to retry an untranslatable block once it changes */
};

typedef sljit_u32 (SLJIT_FUNC *jit_func)(void);

static UINT16 jit_fetch(UINT32 pc);

static int jit_enabled;
static struct jit_fastram jit_ram[JIT_MAX_FASTRAM];
static int jit_ram_count;
static UINT32 jit_granules;
static UINT8 *jit_codemap;			/* a granule holds translated code */
/*
	Per CPU page tables (4 KB pages of the masked address space): the
	host memory of a page the blocks may read (write) directly, NULL for
	one that goes through the memory system. A page is direct when it
	lies in one fast region and no memory handler of that CPU covers any
	of it; for writes also when it holds no translated code (wbase is
	the same without that condition, to restore the table on a flush).
*/
#define JIT_PAGE_SHIFT		12
#define JIT_PAGES			((AM + 1) >> JIT_PAGE_SHIFT)
static UINT8 **jit_rpage[MAX_CPU];
static UINT8 **jit_wpage[MAX_CPU];
static UINT8 **jit_wbase[MAX_CPU];
static UINT8 *jit_codepage;			/* a page holds translated code */
static struct jit_block *jit_table;
int sh2_jit_code_present;
static size_t jit_code_bytes;
static int jit_nomem_logged;


/*************************************
 *  Fast RAM and translated code maps
 *************************************/

/* the region holding 'size' bytes at 'address' (masked), or NULL */
static INLINE struct jit_fastram *jit_find_ram(UINT32 address, UINT32 size)
{
	int i;
	for (i = 0; i < jit_ram_count; i++)
		if (address >= jit_ram[i].start && address + size - 1 <= jit_ram[i].end)
			return &jit_ram[i];
	return NULL;
}

static int jit_in_block;			/* a memory handler runs inside a block */
static int jit_flush_pending;		/* flush once that block has returned */

static void jit_flush(void)
{
	int i;

	if (!jit_table)
		return;
	/* the running block's code must stay until it returns */
	if (jit_in_block)
	{
		jit_flush_pending = 1;
		for (i = 0; i < JIT_TABLE_SIZE; i++)
			jit_table[i].entry = NULL;
		return;
	}
	for (i = 0; i < JIT_TABLE_SIZE; i++)
	{
		if (jit_table[i].code)
			sljit_free_code(jit_table[i].code, NULL);
		jit_table[i].code = NULL;
		jit_table[i].entry = NULL;
		jit_table[i].valid = 0;
	}
	memset(jit_codemap, 0, jit_granules);
	memset(jit_codepage, 0, JIT_PAGES);
	for (i = 0; i < MAX_CPU; i++)
		if (jit_wpage[i])
			memcpy(jit_wpage[i], jit_wbase[i], JIT_PAGES * sizeof(UINT8 *));
	sh2_jit_code_present = 0;
	jit_code_bytes = 0;
}

/* something wrote 'bytes' bytes at 'address' (masked with AM): flush if it was translated code */
void sh2_jit_ram_written(UINT32 address, UINT32 bytes)
{
	struct jit_fastram *ram;
	UINT32 first, last, g;

	if (!sh2_jit_code_present || bytes == 0)
		return;
	address &= AM;
	ram = jit_find_ram(address, 1);
	if (!ram)
		return;
	first = ram->mapoff + ((address - ram->start) >> 2);
	last = ram->mapoff + ((address + bytes - 1 - ram->start) >> 2);
	if (last > ram->mapoff + ((ram->end - ram->start) >> 2))
		last = ram->mapoff + ((ram->end - ram->start) >> 2);
	for (g = first; g <= last; g++)
	{
		if (jit_codemap[g])
		{
			jit_flush();
			return;
		}
	}
}

/*
	What this CPU may access directly: the memory system must map a page
	straight to the host memory of the region, as a RAM bank. Memory
	handlers a driver installs over the RAM (speedups, protection) keep
	their pages on the memory system, so they still run.
*/
static int jit_build_pages(int cpunum)
{
	UINT32 p;

	jit_rpage[cpunum] = calloc(JIT_PAGES, sizeof(UINT8 *));
	jit_wpage[cpunum] = calloc(JIT_PAGES, sizeof(UINT8 *));
	jit_wbase[cpunum] = calloc(JIT_PAGES, sizeof(UINT8 *));
	if (!jit_rpage[cpunum] || !jit_wpage[cpunum] || !jit_wbase[cpunum])
		return 0;
	for (p = 0; p < JIT_PAGES; p++)
	{
		UINT32 start = p << JIT_PAGE_SHIFT, a;
		struct jit_fastram *ram = jit_find_ram(start, 1 << JIT_PAGE_SHIFT);
		int rd = 1, wr;
		UINT8 *host;

		if (!ram)
			continue;
		host = ram->base + (start - ram->start);
		wr = !ram->readonly;
		for (a = 0; a < (1 << JIT_PAGE_SHIFT) && (rd || wr); a += 4)
		{
			if ((UINT8 *)memory_get_read_ptr(cpunum, start + a) != host + a || (UINT8 *)memory_get_read_ptr(cpunum, start + a + 3) != host + a + 3)
				rd = 0;
			if (wr && ((UINT8 *)memory_get_write_ptr(cpunum, start + a) != host + a || (UINT8 *)memory_get_write_ptr(cpunum, start + a + 3) != host + a + 3))
				wr = 0;
		}
		if (rd)
			jit_rpage[cpunum][p] = host;
		if (wr)
			jit_wbase[cpunum][p] = jit_wpage[cpunum][p] = jit_codepage[p] ? NULL : host;
	}
	return 1;
}

void sh2_jit_add_fastram(offs_t start, offs_t end, int readonly, void *base)
{
	if (jit_ram_count >= JIT_MAX_FASTRAM || (start & 3) || ((end + 1) & 3))
		return;
	jit_ram[jit_ram_count].start = start & AM;
	jit_ram[jit_ram_count].end = end & AM;
	jit_ram[jit_ram_count].base = (UINT8 *)base;
	jit_ram[jit_ram_count].readonly = readonly;
	jit_ram[jit_ram_count].mapoff = jit_granules;
	jit_granules += (end - start + 1) >> 2;
	jit_ram_count++;
}

static void jit_exit_all(void)
{
	int i;
	jit_flush();
	free(jit_table);
	free(jit_codemap);
	jit_table = NULL;
	jit_codemap = NULL;
	free(jit_codepage);
	jit_codepage = NULL;
	for (i = 0; i < MAX_CPU; i++)
	{
		free(jit_rpage[i]);
		free(jit_wpage[i]);
		free(jit_wbase[i]);
		jit_rpage[i] = jit_wpage[i] = jit_wbase[i] = NULL;
	}
	jit_enabled = 0;
}

void sh2_jit_set(int enable)
{
	if (enable && !jit_table)
	{
		if (!jit_ram_count)
			return;
		jit_table = calloc(JIT_TABLE_SIZE, sizeof(jit_table[0]));
		jit_codemap = calloc(jit_granules, 1);
		jit_codepage = calloc(JIT_PAGES, 1);
		if (!jit_table || !jit_codemap || !jit_codepage)
		{
			free(jit_table);
			free(jit_codemap);
			free(jit_codepage);
			jit_table = NULL;
			jit_codemap = NULL;
			jit_codepage = NULL;
			return;
		}
		jit_enabled = 1;
		sh2_jit_code_present = 0;
		/* a loaded state rewrites RAM behind our back */
		state_save_register_func_postload(jit_flush);
	}
	else if (!enable)
	{
		if (jit_table)
			jit_exit_all();
		/* the driver declares its fast RAM again for the next game */
		jit_ram_count = 0;
		jit_granules = 0;
	}
}


/*************************************
 *  Instruction decoding
 *************************************/

enum
{
	JIT_END = 0,		/* the interpreter runs it: the block stops before */
	JIT_SIMPLE,			/* native, falls through */
	JIT_MEM,			/* native load or store, falls through (may side exit) */
	JIT_HELPER,			/* the interpreter runs it (no memory), falls through */
	JIT_HELPER_MEM,		/* the interpreter runs it, after its addresses are checked */
	JIT_BRANCH			/* conditional branch (BT/BF) or jump with a delay slot */
};

struct jit_op
{
	int		kind;
	int		cost;		/* the interpreter's cycles (a taken branch for branches) */
	int		acct;		/* cycles the block takes for it (helpers take their own extras) */
	int		pcrel;		/* reads the PC (not allowed in a delay slot) */
	int		slot;		/* a branch with a delay slot */
	int		store;		/* writes memory */
};

#define OPN(op)		(((op) >> 8) & 15)
#define OPM(op)		(((op) >> 4) & 15)

static void jit_decode(UINT16 op, UINT16 next, struct jit_op *o)
{
	memset(o, 0, sizeof(*o));
	o->kind = JIT_SIMPLE;
	o->cost = o->acct = 1;

	switch ((op >> 12) & 15)
	{
	case 0:
		switch (op & 0x3f)
		{
		case 0x03: case 0x23:	/* BSRF, BRAF */
			o->kind = JIT_BRANCH; o->slot = 1; o->cost = o->acct = 2;
			break;
		case 0x04: case 0x05: case 0x06: case 0x14: case 0x15: case 0x16:
		case 0x24: case 0x25: case 0x26: case 0x34: case 0x35: case 0x36:
			o->kind = JIT_MEM; o->store = 1;	/* MOV.x Rm,@(R0,Rn) */
			break;
		case 0x0c: case 0x0d: case 0x0e: case 0x1c: case 0x1d: case 0x1e:
		case 0x2c: case 0x2d: case 0x2e: case 0x3c: case 0x3d: case 0x3e:
			o->kind = JIT_MEM;					/* MOV.x @(R0,Rm),Rn */
			break;
		case 0x07: case 0x17: case 0x27: case 0x37:	/* MUL.L */
			o->cost = o->acct = 2;
			break;
		case 0x0b:				/* RTS */
			o->kind = JIT_BRANCH; o->slot = 1; o->cost = o->acct = 2;
			break;
		case 0x0f: case 0x1f: case 0x2f: case 0x3f:	/* MAC.L */
			o->kind = JIT_HELPER_MEM; o->cost = 3;
			break;
		case 0x19:				/* DIV0U */
			break;
		case 0x1b:				/* SLEEP */
		case 0x2b:				/* RTE */
			o->kind = JIT_END;
			break;
		}
		break;

	case 1:		/* MOV.L Rm,@(disp,Rn) */
		o->kind = JIT_MEM; o->store = 1;
		break;

	case 2:
		switch (op & 15)
		{
		case 0: case 1: case 2: case 4: case 5: case 6:
			o->kind = JIT_MEM; o->store = 1;
			break;
		case 12:				/* CMP/STR */
			o->kind = JIT_HELPER;
			break;
		}
		break;

	case 3:
		switch (op & 15)
		{
		case 11: case 15:		/* SUBV, ADDV */
			o->kind = JIT_HELPER;
			break;
		case 5: case 13:		/* DMULU, DMULS */
			o->cost = o->acct = 2;
			break;
		}
		break;

	case 4:
		switch (op & 0x3f)
		{
		case 0x02: case 0x12: case 0x22:	/* STS.L MACH/MACL/PR,@-Rn */
			o->kind = JIT_MEM; o->store = 1;
			break;
		case 0x03: case 0x13: case 0x23:	/* STC.L SR/GBR/VBR,@-Rn */
			o->kind = JIT_MEM; o->store = 1; o->cost = o->acct = 2;
			break;
		case 0x06: case 0x16: case 0x26:	/* LDS.L @Rm+,MACH/MACL/PR */
			o->kind = JIT_MEM;
			break;
		case 0x07:				/* LDC.L @Rm+,SR */
		case 0x0e:				/* LDC Rm,SR */
			o->kind = JIT_END;
			break;
		case 0x17: case 0x27:	/* LDC.L @Rm+,GBR/VBR */
			o->kind = JIT_MEM; o->cost = o->acct = 3;
			break;
		case 0x0b:				/* JSR */
			o->kind = JIT_BRANCH; o->slot = 1; o->cost = o->acct = 2;
			break;
		case 0x2b:				/* JMP */
			o->kind = JIT_BRANCH; o->slot = 1;
			break;
		case 0x0f: case 0x1f: case 0x2f: case 0x3f:	/* MAC.W */
			o->kind = JIT_HELPER_MEM; o->cost = 3;
			break;
		case 0x10:				/* DT: the interpreter shortens DT / BF $-2 */
			if (next == 0x8bfd)
				o->kind = JIT_END;
			break;
		case 0x1b:				/* TAS.B */
			o->kind = JIT_HELPER_MEM; o->cost = 4; o->store = 1;
			break;
		}
		break;

	case 5:		/* MOV.L @(disp,Rm),Rn */
		o->kind = JIT_MEM;
		break;

	case 6:
		switch (op & 15)
		{
		case 0: case 1: case 2: case 4: case 5: case 6:
			o->kind = JIT_MEM;
			break;
		}
		break;

	case 8:
		switch (OPN(op))
		{
		case 0: case 1:
			o->kind = JIT_MEM; o->store = 1;	/* MOV.B/W R0,@(disp,Rn) */
			break;
		case 4: case 5:
			o->kind = JIT_MEM;					/* MOV.B/W @(disp,Rm),R0 */
			break;
		case 9: case 11:		/* BT, BF */
			o->kind = JIT_BRANCH; o->cost = 3;
			break;
		case 13: case 15:		/* BT/S, BF/S */
			o->kind = JIT_BRANCH; o->slot = 1; o->cost = 2;
			break;
		}
		break;

	case 9: case 13:	/* MOV.W/L @(disp,PC),Rn */
		o->kind = JIT_MEM; o->pcrel = 1;
		break;

	case 10:	/* BRA: the interpreter shortens BRA $ / NOP */
		if ((op & 0xfff) == 0xffe)
			o->kind = JIT_END;
		else
		{
			o->kind = JIT_BRANCH; o->slot = 1; o->cost = o->acct = 2;
		}
		break;
	case 11:	/* BSR */
		o->kind = JIT_BRANCH; o->slot = 1; o->cost = o->acct = 2;
		break;

	case 12:
		switch (OPN(op))
		{
		case 0: case 1: case 2:
			o->kind = JIT_MEM; o->store = 1;	/* MOV.x R0,@(disp,GBR) */
			break;
		case 3:					/* TRAPA */
			o->kind = JIT_END;
			break;
		case 4: case 5: case 6:
			o->kind = JIT_MEM;					/* MOV.x @(disp,GBR),R0 */
			break;
		case 7:					/* MOVA */
			o->pcrel = 1;
			break;
		case 11:				/* OR #imm,R0 takes 3 cycles here */
			o->cost = o->acct = 3;
			break;
		case 12:				/* TST.B */
			o->kind = JIT_HELPER_MEM; o->cost = 3;
			break;
		case 13: case 14:		/* AND.B, XOR.B */
			o->kind = JIT_HELPER_MEM; o->cost = 3; o->store = 1;
			break;
		case 15:				/* OR.B (one cycle in the interpreter) */
			o->kind = JIT_HELPER_MEM; o->store = 1;
			break;
		}
		break;
	}

	/* helpers take their own extra cycles */
	if (o->kind == JIT_HELPER || o->kind == JIT_HELPER_MEM)
		o->acct = 1;
}


/*************************************
 *  Code generation helpers
 *************************************/

#define JMEM(off)		SLJIT_MEM1(SLJIT_S0), (sljit_sw)(off)
#define JREG(n)			JMEM(offsetof(SH2, r) + (n) * sizeof(UINT32))
#define JSR_			JMEM(offsetof(SH2, sr))
#define JGBR			JMEM(offsetof(SH2, gbr))
#define JVBR			JMEM(offsetof(SH2, vbr))
#define JPR				JMEM(offsetof(SH2, pr))
#define JMACH			JMEM(offsetof(SH2, mach))
#define JMACL			JMEM(offsetof(SH2, macl))
#define JRPAGE			JMEM(offsetof(SH2, jit_rpage))
#define JWPAGE			JMEM(offsetof(SH2, jit_wpage))

/* translation context of one block */
struct jit_ctx
{
	struct sljit_compiler *	C;
	UINT32					pc;
	UINT32					count;
	UINT32					pre[JIT_MAX_BLOCK + 2];	/* cycles the block takes for the instructions before n */
	int						nexits;
	int						overflow;
	struct
	{
		struct sljit_jump *	jump;
		UINT32				index;
	} exits[JIT_MAX_EXITS];
};

/* leave before instruction 'index': the interpreter runs it */
static void jit_add_exit(struct jit_ctx *ctx, struct sljit_jump *jump, UINT32 index)
{
	if (ctx->nexits < JIT_MAX_EXITS)
	{
		ctx->exits[ctx->nexits].jump = jump;
		ctx->exits[ctx->nexits].index = index;
		ctx->nexits++;
	}
	else
		ctx->overflow = 1;
}

static void jit_emit_sub_cycles(struct sljit_compiler *C, UINT32 cycles)
{
	if (cycles)
		sljit_emit_op2(C, SLJIT_SUB32, SLJIT_MEM0(), (sljit_sw)&sh2_icount, SLJIT_MEM0(), (sljit_sw)&sh2_icount, SLJIT_IMM, cycles);
}

/* T from the flag 'cond' of the operation just emitted */
static void jit_set_t(struct sljit_compiler *C, sljit_s32 cond)
{
	sljit_emit_op_flags(C, SLJIT_MOV32, SLJIT_R1, 0, cond);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, JSR_, SLJIT_IMM, ~T);
	sljit_emit_op2(C, SLJIT_OR32, JSR_, SLJIT_R0, 0, SLJIT_R1, 0);
}

/* T from bit 'bit' of R2 (before the register changes) */
static void jit_set_t_bit(struct sljit_compiler *C, int bit)
{
	if (bit)
		sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, bit);
	else
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R2, 0);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, T);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, JSR_, SLJIT_IMM, ~T);
	sljit_emit_op2(C, SLJIT_OR32, JSR_, SLJIT_R0, 0, SLJIT_R1, 0);
}

/* r[n] = r[n] op src */
static void jit_alu(struct sljit_compiler *C, sljit_s32 op, int n, sljit_s32 src, sljit_sw srcw)
{
	sljit_emit_op2(C, op, SLJIT_R0, 0, JREG(n), src, srcw);
	sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0);
}

/* T = (src1 cmp src2). The zero flag is asked with SLJIT_SET_Z: SLJIT_SET(SLJIT_EQUAL)
   is 0, no flag at all, and the AArch64 back end then does not compare */
static void jit_cmp(struct sljit_compiler *C, sljit_s32 cond, sljit_s32 src1, sljit_sw src1w, sljit_s32 src2, sljit_sw src2w)
{
	sljit_s32 setf = (cond == SLJIT_EQUAL) ? SLJIT_SET_Z : SLJIT_SET(cond & ~1);
	sljit_emit_op2u(C, SLJIT_SUB32 | setf, src1, src1w, src2, src2w);
	jit_set_t(C, cond);
}

/* the interpreter runs one instruction (no exception, no memory but fast RAM) */
static void sh2_dispatch(UINT16 opcode);

static void SLJIT_FUNC jit_helper(sljit_u32 op)
{
	sh2_dispatch((UINT16)op);
}

static void jit_emit_helper(struct sljit_compiler *C, UINT16 op)
{
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, op);
	sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper));
}


/*
	DIV1 Rm,Rn. With Qn the old bit 31 of Rn, Rn = Rn << 1 | T, then Rm
	is subtracted when the old Q equals M, added otherwise; c is the
	borrow or carry. The interpreter's four cases reduce to
	Q = Qn ^ c ^ M and T = !(Qn ^ c).
*/
static void jit_emit_div1(struct sljit_compiler *C, int n, int m)
{
	struct sljit_jump *add, *done;

	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R3, 0, JSR_);
	/* R1 = Rn << 1 | T */
	sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, SLJIT_R3, 0, SLJIT_IMM, T);
	sljit_emit_op2(C, SLJIT_OR32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	/* R2 = Qn */
	sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 31);
	/* R0 = (old Q ^ M) & 1: 1 adds, 0 subtracts */
	sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R3, 0, SLJIT_IMM, 1);
	sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R3, 0);
	sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, Q);
	add = sljit_emit_jump(C, SLJIT_NOT_ZERO);
	sljit_emit_op2(C, SLJIT_SUB32 | SLJIT_SET_CARRY, SLJIT_R1, 0, SLJIT_R1, 0, JREG(m));
	sljit_emit_op_flags(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_CARRY);
	done = sljit_emit_jump(C, SLJIT_JUMP);
	sljit_set_label(add, sljit_emit_label(C));
	sljit_emit_op2(C, SLJIT_ADD32 | SLJIT_SET_CARRY, SLJIT_R1, 0, SLJIT_R1, 0, JREG(m));
	sljit_emit_op_flags(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_CARRY);
	sljit_set_label(done, sljit_emit_label(C));
	sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R1, 0);
	/* R0 = Qn ^ c; T = R0 ^ 1; Q = R0 ^ M */
	sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, SLJIT_R3, 0, SLJIT_IMM, 9);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
	sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 8);
	sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, ~(Q | T));
	sljit_emit_op2(C, SLJIT_OR32, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_R1, 0);
	sljit_emit_op2(C, SLJIT_OR32, JSR_, SLJIT_R3, 0, SLJIT_R0, 0);
}

/* DMULS.L / DMULU.L: the 64-bit product into MACH:MACL */
static void jit_emit_dmul(struct sljit_compiler *C, int n, int m, int sign)
{
#if JIT_HOST64
	sljit_emit_op1(C, sign ? SLJIT_MOV_S32 : SLJIT_MOV_U32, SLJIT_R0, 0, JREG(n));
	sljit_emit_op1(C, sign ? SLJIT_MOV_S32 : SLJIT_MOV_U32, SLJIT_R1, 0, JREG(m));
	sljit_emit_op2(C, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
#else
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(n));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREG(m));
	sljit_emit_op0(C, sign ? SLJIT_LMUL_SW : SLJIT_LMUL_UW);
#endif
	sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV32, JMACH, SLJIT_R1, 0);
}

/*
	ADDC / SUBC: Rn = Rn +- Rm +- T, T = the carry (borrow) of either
	step, as the interpreter's two comparisons.
*/
static void jit_emit_carry(struct sljit_compiler *C, int n, int m, int sub)
{
	sljit_s32 op = sub ? SLJIT_SUB32 : SLJIT_ADD32;
	sljit_emit_op2(C, op | SLJIT_SET_CARRY, SLJIT_R0, 0, JREG(n), JREG(m));
	sljit_emit_op_flags(C, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_CARRY);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, JSR_, SLJIT_IMM, T);
	sljit_emit_op2(C, op | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(C, SLJIT_OR32, SLJIT_R2, 0, SLJIT_CARRY);
	sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, JSR_, SLJIT_IMM, ~T);
	sljit_emit_op2(C, SLJIT_OR32, JSR_, SLJIT_R1, 0, SLJIT_R2, 0);
}

/*************************************
 *  Plain instructions
 *
 *  Each case mirrors the interpreter in sh2.c exactly.
 *************************************/

static void jit_emit_simple(struct jit_ctx *ctx, UINT16 op, UINT32 pc)
{
	struct sljit_compiler *C = ctx->C;
	int n = OPN(op), m = OPM(op);
	sljit_sw imm8 = (sljit_sw)(INT32)(INT8)(op & 0xff);
	sljit_sw uimm8 = (sljit_sw)(op & 0xff);

	switch ((op >> 12) & 15)
	{
	case 0:
		switch (op & 0x3f)
		{
		case 0x02: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JSR_);  sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* STC SR */
		case 0x12: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JGBR);  sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* STC GBR */
		case 0x22: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JVBR);  sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* STC VBR */
		case 0x0a: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JMACH); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* STS MACH */
		case 0x1a: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JMACL); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* STS MACL */
		case 0x2a: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JPR);   sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* STS PR */
		case 0x07: case 0x17: case 0x27: case 0x37:		/* MUL.L */
			sljit_emit_op2(C, SLJIT_MUL32, SLJIT_R0, 0, JREG(n), JREG(m));
			sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_R0, 0);
			break;
		case 0x08:		/* CLRT */
			sljit_emit_op2(C, SLJIT_AND32, JSR_, JSR_, SLJIT_IMM, ~T);
			break;
		case 0x18:		/* SETT */
			sljit_emit_op2(C, SLJIT_OR32, JSR_, JSR_, SLJIT_IMM, T);
			break;
		case 0x19:		/* DIV0U */
			sljit_emit_op2(C, SLJIT_AND32, JSR_, JSR_, SLJIT_IMM, ~(M | Q | T));
			break;
		case 0x28:		/* CLRMAC */
			sljit_emit_op1(C, SLJIT_MOV32, JMACH, SLJIT_IMM, 0);
			sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_IMM, 0);
			break;
		case 0x29:		/* MOVT */
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, JSR_, SLJIT_IMM, T);
			sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0);
			break;
		default:		/* NOP and the unused codes */
			break;
		}
		break;

	case 2:
		switch (op & 15)
		{
		case 8:		/* TST Rm,Rn */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n));
			sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, JREG(m));
			jit_set_t(C, SLJIT_ZERO);
			break;
		case 7:		/* DIV0S: Q = Rn bit 31, M = Rm bit 31, T = Q ^ M */
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, JREG(n), SLJIT_IMM, 31);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R2, 0, JREG(m), SLJIT_IMM, 31);
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, JSR_, SLJIT_IMM, ~(M | Q | T));
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R3, 0, SLJIT_R1, 0, SLJIT_IMM, 8);
			sljit_emit_op2(C, SLJIT_OR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R3, 0);
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R3, 0, SLJIT_R2, 0, SLJIT_IMM, 9);
			sljit_emit_op2(C, SLJIT_OR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R3, 0);
			sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
			sljit_emit_op2(C, SLJIT_OR32, JSR_, SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 9:  jit_alu(C, SLJIT_AND32, n, JREG(m)); break;
		case 10: jit_alu(C, SLJIT_XOR32, n, JREG(m)); break;
		case 11: jit_alu(C, SLJIT_OR32, n, JREG(m)); break;
		case 13:	/* XTRCT */
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R0, 0, JREG(m), SLJIT_IMM, 16);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, JREG(n), SLJIT_IMM, 16);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 14:	/* MULU */
			sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R0, 0, JREG(n));
			sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R1, 0, JREG(m));
			sljit_emit_op2(C, SLJIT_MUL32, JMACL, SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 15:	/* MULS */
			sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R0, 0, JREG(n));
			sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R1, 0, JREG(m));
			sljit_emit_op2(C, SLJIT_MUL32, JMACL, SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		default:	/* 3: NOP */
			break;
		}
		break;

	case 3:
		switch (op & 15)
		{
		case 0:  jit_cmp(C, SLJIT_EQUAL, JREG(n), JREG(m)); break;
		case 2:  jit_cmp(C, SLJIT_GREATER_EQUAL, JREG(n), JREG(m)); break;
		case 3:  jit_cmp(C, SLJIT_SIG_GREATER_EQUAL, JREG(n), JREG(m)); break;
		case 6:  jit_cmp(C, SLJIT_GREATER, JREG(n), JREG(m)); break;
		case 7:  jit_cmp(C, SLJIT_SIG_GREATER, JREG(n), JREG(m)); break;
		case 8:  jit_alu(C, SLJIT_SUB32, n, JREG(m)); break;
		case 12: jit_alu(C, SLJIT_ADD32, n, JREG(m)); break;
		case 4:  jit_emit_div1(C, n, m); break;
		case 5:  jit_emit_dmul(C, n, m, 0); break;
		case 13: jit_emit_dmul(C, n, m, 1); break;
		case 10: jit_emit_carry(C, n, m, 1); break;	/* SUBC */
		case 14: jit_emit_carry(C, n, m, 0); break;	/* ADDC */
		default: break;		/* 1, 9: NOP */
		}
		break;

	case 4:
		switch (op & 0x3f)
		{
		case 0x00: case 0x20:	/* SHLL, SHAL */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			jit_set_t_bit(C, 31);
			sljit_emit_op2(C, SLJIT_SHL32, JREG(n), SLJIT_R2, 0, SLJIT_IMM, 1);
			break;
		case 0x01:				/* SHLR */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			jit_set_t_bit(C, 0);
			sljit_emit_op2(C, SLJIT_LSHR32, JREG(n), SLJIT_R2, 0, SLJIT_IMM, 1);
			break;
		case 0x21:				/* SHAR */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			jit_set_t_bit(C, 0);
			sljit_emit_op2(C, SLJIT_ASHR32, JREG(n), SLJIT_R2, 0, SLJIT_IMM, 1);
			break;
		case 0x04:				/* ROTL */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			jit_set_t_bit(C, 31);
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 31);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 0x05:				/* ROTR */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			jit_set_t_bit(C, 0);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 31);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 0x24:				/* ROTCL: T in at bit 0, bit 31 out to T */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R3, 0, JSR_, SLJIT_IMM, T);
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R3, 0);
			jit_set_t_bit(C, 31);
			break;
		case 0x25:				/* ROTCR: T in at bit 31, bit 0 out to T */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(n));
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R3, 0, JSR_, SLJIT_IMM, 31);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R3, 0);
			jit_set_t_bit(C, 0);
			break;
		case 0x08: jit_alu(C, SLJIT_SHL32, n, SLJIT_IMM, 2); break;
		case 0x09: jit_alu(C, SLJIT_LSHR32, n, SLJIT_IMM, 2); break;
		case 0x18: jit_alu(C, SLJIT_SHL32, n, SLJIT_IMM, 8); break;
		case 0x19: jit_alu(C, SLJIT_LSHR32, n, SLJIT_IMM, 8); break;
		case 0x28: jit_alu(C, SLJIT_SHL32, n, SLJIT_IMM, 16); break;
		case 0x29: jit_alu(C, SLJIT_LSHR32, n, SLJIT_IMM, 16); break;
		case 0x0a: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n)); sljit_emit_op1(C, SLJIT_MOV32, JMACH, SLJIT_R0, 0); break;	/* LDS MACH */
		case 0x1a: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n)); sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_R0, 0); break;	/* LDS MACL */
		case 0x2a: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n)); sljit_emit_op1(C, SLJIT_MOV32, JPR, SLJIT_R0, 0); break;		/* LDS PR */
		case 0x1e: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n)); sljit_emit_op1(C, SLJIT_MOV32, JGBR, SLJIT_R0, 0); break;		/* LDC GBR */
		case 0x2e: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n)); sljit_emit_op1(C, SLJIT_MOV32, JVBR, SLJIT_R0, 0); break;		/* LDC VBR */
		case 0x10:				/* DT */
			sljit_emit_op2(C, SLJIT_SUB32 | SLJIT_SET_Z, SLJIT_R0, 0, JREG(n), SLJIT_IMM, 1);
			sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0);
			sljit_emit_op2u(C, SLJIT_SUB32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, 0);
			jit_set_t(C, SLJIT_ZERO);
			break;
		case 0x11:				/* CMP/PZ */
			jit_cmp(C, SLJIT_SIG_GREATER_EQUAL, JREG(n), SLJIT_IMM, 0);
			break;
		case 0x15:				/* CMP/PL */
			jit_cmp(C, SLJIT_SIG_GREATER, JREG(n), SLJIT_IMM, 0);
			break;
		default:				/* NOP codes */
			break;
		}
		break;

	case 6:
		switch (op & 15)
		{
		case 3:  sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(m)); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* MOV */
		case 7:  sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R0, 0, JREG(m), SLJIT_IMM, -1); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* NOT */
		case 8:		/* SWAP.B */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(m));
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)(INT32)0xffff0000);
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 0xff);
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 8);
			sljit_emit_op2(C, SLJIT_OR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 8);
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 0xff);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 9:		/* SWAP.W */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(m));
			sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 16);
			sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_IMM, 16);
			sljit_emit_op2(C, SLJIT_OR32, JREG(n), SLJIT_R0, 0, SLJIT_R1, 0);
			break;
		case 11: sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, SLJIT_IMM, 0, JREG(m)); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* NEG */
		case 10:	/* NEGC: Rn = 0 - Rm - T, T = Rm != 0 || T */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(m));
			sljit_emit_op2(C, SLJIT_AND32, SLJIT_R3, 0, JSR_, SLJIT_IMM, T);
			sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R2, 0);
			sljit_emit_op2(C, SLJIT_SUB32, JREG(n), SLJIT_R0, 0, SLJIT_R3, 0);
			sljit_emit_op2(C, SLJIT_OR32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R3, 0);
			sljit_emit_op2u(C, SLJIT_SUB32 | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_IMM, 0);
			jit_set_t(C, SLJIT_NOT_ZERO);
			break;
		case 12: sljit_emit_op1(C, SLJIT_MOV32_U8, SLJIT_R0, 0, JREG(m)); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* EXTU.B */
		case 13: sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R0, 0, JREG(m)); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* EXTU.W */
		case 14: sljit_emit_op1(C, SLJIT_MOV32_S8, SLJIT_R0, 0, JREG(m)); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* EXTS.B */
		case 15: sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R0, 0, JREG(m)); sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R0, 0); break;	/* EXTS.W */
		}
		break;

	case 7:		/* ADD #imm,Rn */
		jit_alu(C, SLJIT_ADD32, n, SLJIT_IMM, imm8);
		break;

	case 8:
		switch (OPN(op))
		{
		case 8:		/* CMP/EQ #imm,R0 */
			jit_cmp(C, SLJIT_EQUAL, JREG(0), SLJIT_IMM, imm8);
			break;
		default:	/* NOP codes */
			break;
		}
		break;

	case 12:
		switch (OPN(op))
		{
		case 7:		/* MOVA */
			sljit_emit_op1(C, SLJIT_MOV32, JREG(0), SLJIT_IMM, (sljit_sw)(INT32)(((pc + 4) & ~3) + uimm8 * 4));
			break;
		case 8:		/* TST #imm,R0 */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(0));
			sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, uimm8);
			jit_set_t(C, SLJIT_ZERO);
			break;
		case 9:  jit_alu(C, SLJIT_AND32, 0, SLJIT_IMM, uimm8); break;
		case 10: jit_alu(C, SLJIT_XOR32, 0, SLJIT_IMM, uimm8); break;
		case 11: jit_alu(C, SLJIT_OR32, 0, SLJIT_IMM, uimm8); break;
		}
		break;

	case 14:	/* MOV #imm,Rn */
		sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_IMM, imm8);
		break;

	default:	/* 15: NOP */
		break;
	}
}


/*************************************
 *  Loads and stores
 *************************************/

/*
	The address of the access into R0 (32 bits). Post-increment and
	pre-decrement registers change only after the access, in
	jit_emit_mem_finish.
*/
struct jit_mem
{
	int		size;
	int		store;
	int		sext;		/* sign extend a load */
};

static void jit_mem_info(UINT16 op, struct jit_mem *mi)
{
	static const int sizes[4] = { 1, 2, 4, 0 };
	mi->size = 4;
	mi->store = 0;
	mi->sext = 1;
	switch ((op >> 12) & 15)
	{
	case 0:  mi->size = sizes[op & 3]; mi->store = !(op & 8); break;
	case 1:  mi->size = 4; mi->store = 1; break;
	case 2:  mi->size = sizes[op & 3]; mi->store = 1; break;
	case 4:
		mi->size = 4;
		mi->store = ((op & 15) == 2 || (op & 15) == 3);
		break;
	case 5:  mi->size = 4; break;
	case 6:  mi->size = sizes[op & 3]; break;
	case 8:  mi->size = sizes[OPN(op) & 1]; mi->store = OPN(op) < 4; break;
	case 9:  mi->size = 2; break;
	case 12: mi->size = sizes[OPN(op) & 3]; mi->store = OPN(op) < 4; break;
	case 13: mi->size = 4; break;
	}
}

static void jit_emit_mem_ea(struct sljit_compiler *C, UINT16 op, UINT32 pc, const struct jit_mem *mi)
{
	int n = OPN(op), m = OPM(op);
	UINT32 d4 = op & 15, d8 = op & 0xff;

	switch ((op >> 12) & 15)
	{
	case 0:		/* @(R0,Rn) / @(R0,Rm) */
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(mi->store ? n : m), JREG(0));
		break;
	case 1:		/* @(disp,Rn) */
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(n), SLJIT_IMM, d4 * 4);
		break;
	case 2:		/* @Rn, @-Rn */
		if (op & 4)
			sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, JREG(n), SLJIT_IMM, mi->size);
		else
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n));
		break;
	case 4:		/* STS.L / STC.L @-Rn, LDS.L / LDC.L @Rm+ */
		if (mi->store)
			sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, JREG(n), SLJIT_IMM, 4);
		else
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n));
		break;
	case 5:		/* @(disp,Rm) */
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(m), SLJIT_IMM, d4 * 4);
		break;
	case 6:		/* @Rm, @Rm+ */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(m));
		break;
	case 8:		/* @(disp,Rn) / @(disp,Rm), byte or word */
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(m), SLJIT_IMM, d4 * mi->size);
		break;
	case 9:		/* @(disp,PC), word */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(INT32)(pc + 4 + d8 * 2));
		break;
	case 12:	/* @(disp,GBR) */
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JGBR, SLJIT_IMM, d8 * mi->size);
		break;
	case 13:	/* @(disp,PC), long */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(INT32)(((pc + 4) & ~3) + d8 * 4));
		break;
	}
}

/*
	Host address of the 'size' bytes at R0 into R1, through the page
	table of this CPU. Otherwise the access leaves before instruction
	'index' for the interpreter, or goes to the 'slow' jumps (the memory
	system in the block) when given: when it is misaligned (always a side
	exit), at or above the special areas (cache, internal registers), or
	in a page the memory system handles or, for a store, that holds
	translated code. R0 keeps the address.
*/
static void jit_emit_mem_check(struct jit_ctx *ctx, int size, int store, UINT32 index, struct sljit_jump **slow, int *nslow)
{
	struct sljit_compiler *C = ctx->C;
	struct sljit_jump *jump;

	if (size > 1)
	{
		sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, size - 1);
		jit_add_exit(ctx, sljit_emit_jump(C, SLJIT_NOT_ZERO), index);
	}
	/* 0x40000000 and up: the interpreter's special areas */
	sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(INT32)0xc0000000);
	jump = sljit_emit_jump(C, SLJIT_NOT_ZERO);
	if (slow)
		slow[(*nslow)++] = jump;
	else
		jit_add_exit(ctx, jump, index);

	/* R3 = the page's host memory */
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, AM);
	sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
	sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, JIT_PAGE_SHIFT);
	if (store)
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R3, 0, JWPAGE);
	else
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R3, 0, JRPAGE);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R3, 0, SLJIT_MEM2(SLJIT_R3, SLJIT_R2), SLJIT_WORD_SHIFT);
	jump = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_R3, 0, SLJIT_IMM, 0);
	if (slow)
		slow[(*nslow)++] = jump;
	else
		jit_add_exit(ctx, jump, index);

	/* the memory system keeps 32-bit big-endian words in host order */
	sljit_emit_op2(C, SLJIT_AND, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, (1 << JIT_PAGE_SHIFT) - 1);
	if (size == 1)
		sljit_emit_op2(C, SLJIT_XOR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 3);
	else if (size == 2)
		sljit_emit_op2(C, SLJIT_XOR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_op2(C, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R3, 0);
}

#ifdef SH2JIT_VERIFY
static void SLJIT_FUNC jit_verify_log_store(sljit_up host, sljit_u32 size);
#endif

/* a load's value (sign extended) is in R2: the register updates */
static void jit_emit_load_result(struct sljit_compiler *C, UINT16 op, const struct jit_mem *mi)
{
	int n = OPN(op), m = OPM(op);

	switch ((op >> 12) & 15)
	{
	case 4:		/* LDS.L / LDC.L @Rm+: the load, then Rm += 4 */
	{
		size_t field;
		switch (op & 0x3f)
		{
		case 0x06: field = offsetof(SH2, mach); break;
		case 0x16: field = offsetof(SH2, macl); break;
		case 0x26: field = offsetof(SH2, pr); break;
		case 0x17: field = offsetof(SH2, gbr); break;
		default:   field = offsetof(SH2, vbr); break;	/* 0x27 */
		}
		sljit_emit_op1(C, SLJIT_MOV32, JMEM(field), SLJIT_R2, 0);
		sljit_emit_op2(C, SLJIT_ADD32, JREG(n), JREG(n), SLJIT_IMM, 4);
		break;
	}
	case 6:		/* MOV.x @Rm,Rn / @Rm+,Rn: no increment when n == m */
		sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R2, 0);
		if ((op & 4) && n != m)
			sljit_emit_op2(C, SLJIT_ADD32, JREG(m), JREG(m), SLJIT_IMM, mi->size);
		break;
	case 8:		/* MOV.B/W @(disp,Rm),R0 */
	case 12:	/* MOV.x @(disp,GBR),R0 */
		sljit_emit_op1(C, SLJIT_MOV32, JREG(0), SLJIT_R2, 0);
		break;
	default:	/* MOV.x @(R0,Rm),Rn, MOV.L @(disp,Rm),Rn, MOV.x @(disp,PC),Rn */
		sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R2, 0);
		break;
	}
}

/* the access through the host pointer in R1, then the register updates */
static void jit_emit_mem_access(struct sljit_compiler *C, UINT16 op, const struct jit_mem *mi)
{
	int n = OPN(op), m = OPM(op);
	sljit_s32 ld = (mi->size == 1) ? SLJIT_MOV32_S8 : (mi->size == 2) ? SLJIT_MOV32_S16 : SLJIT_MOV32;
	sljit_s32 st = (mi->size == 1) ? SLJIT_MOV32_U8 : (mi->size == 2) ? SLJIT_MOV32_U16 : SLJIT_MOV32;

#ifdef SH2JIT_VERIFY
	if (mi->store)
	{
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S1, 0, SLJIT_R1, 0);
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_R1, 0);
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, mi->size);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_verify_log_store));
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R1, 0, SLJIT_S1, 0);
	}
#endif

	if (!mi->store)
	{
		sljit_emit_op1(C, ld, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
		jit_emit_load_result(C, op, mi);
		return;
	}

	switch ((op >> 12) & 15)
	{
	case 0:
		if (mi->store)		/* MOV.x Rm,@(R0,Rn) */
		{
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(m));
			sljit_emit_op1(C, st, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
		}
		else				/* MOV.x @(R0,Rm),Rn */
		{
			sljit_emit_op1(C, ld, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
			sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R2, 0);
		}
		break;
	case 1:		/* MOV.L Rm,@(disp,Rn) */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(m));
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
		break;
	case 2:		/* MOV.x Rm,@Rn / Rm,@-Rn: the data is read before Rn changes */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(m));
		sljit_emit_op1(C, st, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
		if (op & 4)
			sljit_emit_op2(C, SLJIT_SUB32, JREG(n), JREG(n), SLJIT_IMM, mi->size);
		break;
	case 4:
	{
		size_t field;
		switch (op & 0x3f)
		{
		case 0x02: field = offsetof(SH2, mach); break;
		case 0x12: field = offsetof(SH2, macl); break;
		case 0x22: field = offsetof(SH2, pr); break;
		case 0x03: field = offsetof(SH2, sr); break;
		case 0x13: field = offsetof(SH2, gbr); break;
		case 0x23: field = offsetof(SH2, vbr); break;
		case 0x06: field = offsetof(SH2, mach); break;
		case 0x16: field = offsetof(SH2, macl); break;
		case 0x26: field = offsetof(SH2, pr); break;
		case 0x17: field = offsetof(SH2, gbr); break;
		default:   field = offsetof(SH2, vbr); break;	/* 0x27 */
		}
		if (mi->store)		/* Rn -= 4, then the store */
		{
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JMEM(field));
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
			sljit_emit_op2(C, SLJIT_SUB32, JREG(n), JREG(n), SLJIT_IMM, 4);
		}
		else				/* the load, then Rm += 4 */
		{
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
			sljit_emit_op1(C, SLJIT_MOV32, JMEM(field), SLJIT_R2, 0);
			sljit_emit_op2(C, SLJIT_ADD32, JREG(n), JREG(n), SLJIT_IMM, 4);
		}
		break;
	}
	case 5:		/* MOV.L @(disp,Rm),Rn */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
		sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R2, 0);
		break;
	case 6:		/* MOV.x @Rm,Rn / @Rm+,Rn: no increment when n == m */
		sljit_emit_op1(C, ld, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
		sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R2, 0);
		if ((op & 4) && n != m)
			sljit_emit_op2(C, SLJIT_ADD32, JREG(m), JREG(m), SLJIT_IMM, mi->size);
		break;
	case 8:
		if (mi->store)		/* MOV.B/W R0,@(disp,Rn) */
		{
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(0));
			sljit_emit_op1(C, st, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
		}
		else				/* MOV.B/W @(disp,Rm),R0 */
		{
			sljit_emit_op1(C, ld, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
			sljit_emit_op1(C, SLJIT_MOV32, JREG(0), SLJIT_R2, 0);
		}
		break;
	case 9:		/* MOV.W @(disp,PC),Rn */
	case 13:	/* MOV.L @(disp,PC),Rn */
		sljit_emit_op1(C, ld, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
		sljit_emit_op1(C, SLJIT_MOV32, JREG(n), SLJIT_R2, 0);
		break;
	case 12:
		if (mi->store)		/* MOV.x R0,@(disp,GBR) */
		{
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, JREG(0));
			sljit_emit_op1(C, st, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R2, 0);
		}
		else				/* MOV.x @(disp,GBR),R0 */
		{
			sljit_emit_op1(C, ld, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), 0);
			sljit_emit_op1(C, SLJIT_MOV32, JREG(0), SLJIT_R2, 0);
		}
		break;
	}
}

/*
	A load the fast path cannot do (a memory handler, the internal
	registers): the memory system reads it here, as in the interpreter's
	step: sh2_icount has the cycles before the instruction and the PC is
	past it. The block leaves after the instruction when the handler
	took cycles (a spin or yield hack, the end of the timeslice), raised
	an interrupt or flushed the translated code.
*/
static sljit_u32 jit_slow_break;

static sljit_s32 SLJIT_FUNC jit_slow_read(sljit_u32 ea, sljit_u32 size)
{
	int before = sh2_icount;
	INT32 v;

	jit_in_block = 1;
	if (size == 1)
		v = (INT8)RB(ea);
	else if (size == 2)
		v = (INT16)RW(ea);
	else
		v = (INT32)RL(ea);
	jit_in_block = 0;
	jit_slow_break = (sh2_icount != before || sh2.test_irq || jit_flush_pending);
	return v;
}

static void SLJIT_FUNC jit_slow_write(sljit_u32 ea, sljit_u32 v, sljit_u32 size)
{
	int before = sh2_icount;

	jit_in_block = 1;
	if (size == 1)
		WB(ea, (UINT8)v);
	else if (size == 2)
		WW(ea, (UINT16)v);
	else
		WL(ea, v);
	jit_in_block = 0;
	jit_slow_break = (sh2_icount != before || sh2.test_irq || jit_flush_pending);
}

/* a store's data into R1 (before the registers change) */
static void jit_emit_store_data(struct sljit_compiler *C, UINT16 op)
{
	int m = OPM(op);

	switch ((op >> 12) & 15)
	{
	case 4:
		switch (op & 0x3f)
		{
		case 0x02: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JMEM(offsetof(SH2, mach))); break;
		case 0x12: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JMEM(offsetof(SH2, macl))); break;
		case 0x22: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JMEM(offsetof(SH2, pr))); break;
		case 0x03: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JMEM(offsetof(SH2, sr))); break;
		case 0x13: sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JMEM(offsetof(SH2, gbr))); break;
		default:   sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JMEM(offsetof(SH2, vbr))); break;	/* 0x23 */
		}
		break;
	case 8:		/* MOV.B/W R0,@(disp,Rn) */
	case 12:	/* MOV.x R0,@(disp,GBR) */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JREG(0));
		break;
	default:	/* Rm,@(R0,Rn), Rm,@(disp,Rn), Rm,@Rn, Rm,@-Rn */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JREG(m));
		break;
	}
}

static void jit_emit_mem(struct jit_ctx *ctx, UINT16 op, UINT32 pc, UINT32 index)
{
	struct sljit_compiler *C = ctx->C;
	struct sljit_jump *slow[JIT_MAX_FASTRAM + 2];
	struct sljit_jump *done;
	struct sljit_label *label;
	struct jit_mem mi;
	int nslow = 0, i, slowread;

	jit_mem_info(op, &mi);
	/* not in a delay slot (the slot's address is not its block index) */
	slowread = pc == ctx->pc + index * 2;
#ifdef SH2JIT_VERIFY
	/* the check runs the block twice: a handler must not see a read twice */
	slowread = 0;
#endif
	jit_emit_mem_ea(C, op, pc, &mi);
	jit_emit_mem_check(ctx, mi.size, mi.store, index, slowread ? slow : NULL, &nslow);
	jit_emit_mem_access(C, op, &mi);
	if (!slowread)
		return;
	done = sljit_emit_jump(C, SLJIT_JUMP);

	label = sljit_emit_label(C);
	for (i = 0; i < nslow; i++)
		sljit_set_label(slow[i], label);
	/* R0 = the address: the memory system does the access as in the interpreter */
	jit_emit_sub_cycles(C, ctx->pre[index]);
	sljit_emit_op1(C, SLJIT_MOV32, JMEM(offsetof(SH2, pc)), SLJIT_IMM, (sljit_sw)(INT32)(pc + 2));
	sljit_emit_op1(C, SLJIT_MOV32, JMEM(offsetof(SH2, ppc)), SLJIT_IMM, (sljit_sw)(INT32)(pc + 2));
	if (mi.store)
	{
		jit_emit_store_data(C, op);
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_IMM, mi.size);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3V(32, 32, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_slow_write));
	}
	else
	{
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, mi.size);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(32, 32, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_slow_read));
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_RETURN_REG, 0);
	}
	if (ctx->pre[index])
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_MEM0(), (sljit_sw)&sh2_icount, SLJIT_MEM0(), (sljit_sw)&sh2_icount, SLJIT_IMM, ctx->pre[index]);
	if (mi.store)
	{
		/* @-Rn: Rn changes after the store (the interpreter's order makes no difference) */
		if ((((op >> 12) & 15) == 2 && (op & 4)) || ((op >> 12) & 15) == 4)
			sljit_emit_op2(C, SLJIT_SUB32, JREG(OPN(op)), JREG(OPN(op)), SLJIT_IMM, mi.size);
	}
	else
		jit_emit_load_result(C, op, &mi);
	jit_add_exit(ctx, sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM0(), (sljit_sw)&jit_slow_break, SLJIT_IMM, 0), index + 1);

	sljit_set_label(done, sljit_emit_label(C));
}

/*
	MAC.L, MAC.W, AND.B/OR.B/XOR.B/TST.B @(R0,GBR) and TAS.B: check the
	addresses the interpreter will use, then let it run the instruction
	(through fast RAM only, so no memory handler runs inside a block).
*/
static void jit_emit_helper_mem(struct jit_ctx *ctx, UINT16 op, UINT32 index)
{
	struct sljit_compiler *C = ctx->C;
	int n = OPN(op), m = OPM(op);
	int store = 0;

	if (((op >> 12) & 15) == 0 || (((op >> 12) & 15) == 4 && (op & 15) == 15))
	{
		/* MAC.L / MAC.W @Rm+,@Rn+: Rn is read first, then Rm (Rn + size when the same) */
		int size = (((op >> 12) & 15) == 0) ? 4 : 2;
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n));
		jit_emit_mem_check(ctx, size, 0, index, NULL, NULL);
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S1, 0, SLJIT_R1, 0);
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(m));
		if (m == n)
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, size);
		jit_emit_mem_check(ctx, size, 0, index, NULL, NULL);
#if JIT_HOST64
		if (size == 4)
		{
			/*
				MAC.L without saturation (S clear) in native code: the signed
				64-bit product added to MACH:MACL, wrapping; with S set the
				interpreter runs it. The 2 extra cycles are the helper's own
				on that path, so they are taken here on this one.
			*/
			struct sljit_jump *sat, *done;
			sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, JSR_, SLJIT_IMM, S);
			sat = sljit_emit_jump(C, SLJIT_NOT_ZERO);
			sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S1), 0);
			sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R1), 0);
			sljit_emit_op2(C, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
			sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R2, 0, JMACH);
			sljit_emit_op2(C, SLJIT_SHL, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 32);
			sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R3, 0, JMACL);
			sljit_emit_op2(C, SLJIT_OR, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R3, 0);
			sljit_emit_op2(C, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
			sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_R0, 0);
			sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
			sljit_emit_op1(C, SLJIT_MOV32, JMACH, SLJIT_R0, 0);
			sljit_emit_op2(C, SLJIT_ADD32, JREG(n), JREG(n), SLJIT_IMM, 4);
			sljit_emit_op2(C, SLJIT_ADD32, JREG(m), JREG(m), SLJIT_IMM, 4);
			jit_emit_sub_cycles(C, 2);
			done = sljit_emit_jump(C, SLJIT_JUMP);
			sljit_set_label(sat, sljit_emit_label(C));
			jit_emit_helper(C, op);
			sljit_set_label(done, sljit_emit_label(C));
			return;
		}
#endif
	}
	else if (((op >> 12) & 15) == 4)
	{
		/* TAS.B @Rn */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(n));
		jit_emit_mem_check(ctx, 1, 1, index, NULL, NULL);
		store = 1;
	}
	else
	{
		/* AND.B / OR.B / XOR.B / TST.B #imm,@(R0,GBR) */
		store = OPN(op) != 12;
		sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JGBR, JREG(0));
		jit_emit_mem_check(ctx, 1, store, index, NULL, NULL);
	}
#ifdef SH2JIT_VERIFY
	if (store)
	{
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_R1, 0);
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, 1);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_verify_log_store));
	}
#else
	(void)store;
#endif
	jit_emit_helper(C, op);
}

/* a plain instruction (not a branch) */
static void jit_emit_one(struct jit_ctx *ctx, UINT16 op, const struct jit_op *o, UINT32 pc, UINT32 index)
{
	switch (o->kind)
	{
	case JIT_MEM:
		jit_emit_mem(ctx, op, pc, index);
		break;
	case JIT_HELPER:
		jit_emit_helper(ctx->C, op);
		break;
	case JIT_HELPER_MEM:
		jit_emit_helper_mem(ctx, op, index);
		break;
	default:
		jit_emit_simple(ctx, op, pc);
		break;
	}
}


/*************************************
 *  Branches
 *************************************/

/* BSR, BSRF and JSR write PR */
static int jit_branch_links(UINT16 op)
{
	return ((op >> 12) & 15) == 11 || (op & 0xf0ff) == 0x0003 || (op & 0xf0ff) == 0x400b;
}

/* does the instruction read or write PR? */
static int jit_touches_pr(UINT16 op)
{
	return (op & 0xf0ff) == 0x002a ||	/* STS PR,Rn */
		(op & 0xf0ff) == 0x4022 ||		/* STS.L PR,@-Rn */
		(op & 0xf0ff) == 0x402a ||		/* LDS Rm,PR */
		(op & 0xf0ff) == 0x4026;		/* LDS.L @Rm+,PR */
}

/*
	Block chaining. Leaving for an address whose block is translated and
	fits in what is left of the timeslice, jump straight into it instead
	of returning to the dispatcher (sh2_jit_run), which would do the
	same: no delay slot or interrupt test is pending on the way out of a
	block, so it would look the block up, check that its cycles fit and
	run it. All blocks share one stack frame (same sljit_emit_enter), so
	the jump lands after the prologue and the block's own return goes
	back to the dispatcher. The check reads the block table when it runs:
	blocks compiled, replaced or flushed later are seen, and a block that
	is not there or does not fit is simply a return to the dispatcher.
*/
static INLINE UINT32 jit_table_index(UINT32 pc)
{
	return (pc >> 1) & (JIT_TABLE_SIZE - 1);
}

static void jit_emit_chain(struct sljit_compiler *C, sljit_s32 src, sljit_sw srcw)
{
#ifndef SH2JIT_VERIFY
	struct sljit_jump *miss[3];
	struct sljit_label *out;
	int i;

	if (src == SLJIT_IMM)
	{
		UINT32 target = (UINT32)srcw;
		if (target & 1)
			return;
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&jit_table[jit_table_index(target)]);
		miss[0] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM1(SLJIT_R1), offsetof(struct jit_block, pc), SLJIT_IMM, (sljit_sw)(INT32)target);
	}
	else
	{
		/* R1 = &jit_table[(pc >> 1) & mask] */
		sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, src, srcw, SLJIT_IMM, 1);
		sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, JIT_TABLE_SIZE - 1);
		/* upper bits of a 32-bit result are undefined for word operations */
		sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
		sljit_emit_op2(C, SLJIT_MUL, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, sizeof(struct jit_block));
		sljit_emit_op2(C, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)jit_table);
		miss[0] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM1(SLJIT_R1), offsetof(struct jit_block, pc), src, srcw);
	}
	/* entry is NULL for a block that is flushed, being compiled or not translatable */
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), offsetof(struct jit_block, entry));
	miss[1] = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0);
	sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R1), offsetof(struct jit_block, cost));
	miss[2] = sljit_emit_cmp(C, SLJIT_SIG_GREATER | SLJIT_32, SLJIT_R1, 0, SLJIT_MEM0(), (sljit_sw)&sh2_icount);
	sljit_emit_ijump(C, SLJIT_JUMP, SLJIT_R2, 0);
	out = sljit_emit_label(C);
	for (i = 0; i < 3; i++)
		sljit_set_label(miss[i], out);
#endif
}

/* leave the block for the address in src/srcw after taking 'cycles' */
static void jit_emit_leave(struct sljit_compiler *C, UINT32 cycles, sljit_s32 src, sljit_sw srcw)
{
	jit_emit_sub_cycles(C, cycles);
	jit_emit_chain(C, src, srcw);
	sljit_emit_return(C, SLJIT_MOV32, src, srcw);
}

/* leave for the interpreter at 'pc' (a side exit): no chaining */
static void jit_emit_exit(struct sljit_compiler *C, UINT32 cycles, UINT32 pc)
{
	jit_emit_sub_cycles(C, cycles);
	sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)(INT32)pc);
}

/*
	Branch at instruction 'index' (address pc). BT/BF leave when taken.
	The others have a delay slot (instruction index + 1): its address
	checks come first (a side exit leaves before the branch), then the
	branch takes its target and writes PR as the interpreter does, then
	the slot runs. BT/S and BF/S not taken just go on with the slot as
	the next instruction.
*/
static void jit_emit_branch(struct jit_ctx *ctx, UINT16 op, UINT16 slot, UINT32 pc, UINT32 index)
{
	struct sljit_compiler *C = ctx->C;
	int n = OPN(op);
	INT32 disp8 = (INT32)(INT8)(op & 0xff);
	INT32 disp12 = ((INT32)(op & 0xfff) << 20) >> 20;
	struct jit_op so;
	struct sljit_jump *skip = NULL;
	int cond = 0, taken_if_t = 0;

	switch ((op >> 12) & 15)
	{
	case 8:
		switch (OPN(op))
		{
		case 9:  cond = 1; taken_if_t = 1; break;	/* BT */
		case 11: cond = 1; taken_if_t = 0; break;	/* BF */
		case 13: cond = 2; taken_if_t = 1; break;	/* BT/S */
		case 15: cond = 2; taken_if_t = 0; break;	/* BF/S */
		}
		break;
	}

	if (cond == 1)
	{
		/* taken: 3 cycles, not taken: 1 */
		sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, JSR_, SLJIT_IMM, T);
		skip = sljit_emit_jump(C, taken_if_t ? SLJIT_ZERO : SLJIT_NOT_ZERO);
		jit_emit_leave(C, ctx->pre[index] + 3, SLJIT_IMM, (sljit_sw)(INT32)(pc + 4 + disp8 * 2));
		sljit_set_label(skip, sljit_emit_label(C));
		return;
	}

	jit_decode(slot, 0, &so);

	if (cond == 2)
	{
		/* not taken: on with the slot as a plain instruction */
		sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, JSR_, SLJIT_IMM, T);
		skip = sljit_emit_jump(C, taken_if_t ? SLJIT_ZERO : SLJIT_NOT_ZERO);
		/* taken: the slot, then leave (2 cycles for the branch, the slot's own) */
		jit_emit_one(ctx, slot, &so, pc + 2, index);
		jit_emit_leave(C, ctx->pre[index] + 2 + so.acct, SLJIT_IMM, (sljit_sw)(INT32)(pc + 4 + disp8 * 2));
		sljit_set_label(skip, sljit_emit_label(C));
		return;
	}

	/* jumps: the target into S2 before the slot can change anything */
	switch ((op >> 12) & 15)
	{
	case 0:
		if ((op & 0x3f) == 0x0b)	/* RTS */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S2, 0, JPR);
		else						/* BSRF / BRAF: Rm + the branch + 4 */
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_S2, 0, JREG(n), SLJIT_IMM, (sljit_sw)(INT32)(pc + 4));
		break;
	case 4:		/* JSR / JMP */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S2, 0, JREG(n));
		break;
	default:	/* BRA / BSR */
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S2, 0, SLJIT_IMM, (sljit_sw)(INT32)(pc + 4 + disp12 * 2));
		break;
	}
	/* the slot (its side exits leave before the branch), then PR: the */
	/* block builder keeps a slot that touches PR out of linking jumps */
	jit_emit_one(ctx, slot, &so, pc + 2, index);
	if (jit_branch_links(op))
		sljit_emit_op1(C, SLJIT_MOV32, JPR, SLJIT_IMM, (sljit_sw)(INT32)(pc + 4));
	{
		struct jit_op bo;
		jit_decode(op, 0, &bo);
		jit_emit_leave(C, ctx->pre[index] + bo.acct + so.acct, SLJIT_S2, 0);
	}
}


/*************************************
 *  Debug: every block also runs through the interpreter
 *************************************/

#ifdef SH2JIT_VERIFY
#define SH2JIT_VLOG		512

static struct
{
	UINT8 *		p;
	int			size;
	UINT8		old[4];
} jit_vlog[SH2JIT_VLOG];
static int jit_vlog_count;
static struct
{
	UINT32		address;
	int			size;
} interp_vlog[SH2JIT_VLOG];
static int interp_vlog_count;
int sh2_jit_verify_logging;

static void SLJIT_FUNC jit_verify_log_store(sljit_up host, sljit_u32 size)
{
	if (jit_vlog_count < SH2JIT_VLOG)
	{
		jit_vlog[jit_vlog_count].p = (UINT8 *)host;
		jit_vlog[jit_vlog_count].size = size;
		memcpy(jit_vlog[jit_vlog_count].old, (void *)host, size);
		jit_vlog_count++;
	}
}

/* called by WB/WW/WL while the interpreter checks a block */
void sh2_jit_verify_store(UINT32 address, int size)
{
	if (interp_vlog_count < SH2JIT_VLOG)
	{
		interp_vlog[interp_vlog_count].address = address & AM;
		interp_vlog[interp_vlog_count].size = size;
		interp_vlog_count++;
	}
}

static UINT8 *jit_host(UINT32 address, int size)
{
	struct jit_fastram *ram = jit_find_ram(address & AM, size);
	UINT32 off;
	if (!ram)
		return NULL;
	off = (address & AM) - ram->start;
	return ram->base + (off ^ (size == 1 ? 3 : size == 2 ? 2 : 0));
}

static void sh2_step(void);

/* runs the block, then the interpreter from the same state, and compares */
static UINT32 jit_verify_block(struct jit_block *block)
{
	static SH2 save, jres;
	static UINT8 jnew[SH2JIT_VLOG][4];
	static unsigned long blocks;
	int before = sh2_icount, after, i, j, bad = 0;
	UINT32 jpc;

	save = sh2;
	jit_vlog_count = 0;
	jpc = ((jit_func)block->code)();
	after = sh2_icount;
	jres = sh2;
	for (i = 0; i < jit_vlog_count; i++)
		memcpy(jnew[i], jit_vlog[i].p, jit_vlog[i].size);
	for (i = jit_vlog_count - 1; i >= 0; i--)
		memcpy(jit_vlog[i].p, jit_vlog[i].old, jit_vlog[i].size);
	sh2 = save;
	sh2_icount = before;
	if (after == before)
		return jpc;

	interp_vlog_count = 0;
	sh2_jit_verify_logging = 1;
	while (before - sh2_icount < before - after)
		sh2_step();
	sh2_jit_verify_logging = 0;

	if (sh2.pc != jpc || sh2_icount != after || sh2.delay ||
		memcmp(sh2.r, jres.r, sizeof(sh2.r)) || sh2.sr != jres.sr || sh2.gbr != jres.gbr ||
		sh2.vbr != jres.vbr || sh2.pr != jres.pr || sh2.mach != jres.mach || sh2.macl != jres.macl)
		bad = 1;
	for (i = 0; i < jit_vlog_count; i++)
		if (memcmp(jnew[i], jit_vlog[i].p, jit_vlog[i].size))
			bad |= 2;
	for (i = 0; i < interp_vlog_count; i++)
	{
		int found = 0;
		UINT8 *h = jit_host(interp_vlog[i].address, interp_vlog[i].size);
		for (j = 0; j < jit_vlog_count; j++)
			if (h && jit_vlog[j].p == h)
				found = 1;
		if (!found)
			bad |= 4;
	}
	blocks++;
	if (jit_vlog_count >= SH2JIT_VLOG || interp_vlog_count >= SH2JIT_VLOG)
		bad = 0;
	if (bad)
	{
		fprintf(stderr, "SH2JITVERIFY mismatch %d at block %08x (%u instructions, cost %u), cpu %d, block #%lu\n",
			bad, save.pc, block->len, block->cost, cpu_getactivecpu(), blocks);
		fprintf(stderr, "SH2JITVERIFY pc i=%08x j=%08x icount before %d i=%d j=%d delay %08x\n", sh2.pc, jpc, before, sh2_icount, after, sh2.delay);
		fprintf(stderr, "SH2JITVERIFY sr %08x/%08x gbr %08x/%08x pr %08x/%08x mach %08x/%08x macl %08x/%08x\n",
			sh2.sr, jres.sr, sh2.gbr, jres.gbr, sh2.pr, jres.pr, sh2.mach, jres.mach, sh2.macl, jres.macl);
		for (i = 0; i < 16; i++)
			if (sh2.r[i] != jres.r[i] || save.r[i] != sh2.r[i])
				fprintf(stderr, "SH2JITVERIFY r%-2d before %08x interp %08x jit %08x%s\n", i, save.r[i], sh2.r[i], jres.r[i], sh2.r[i] != jres.r[i] ? "  <<<" : "");
		for (i = 0; i < interp_vlog_count; i++)
			fprintf(stderr, "SH2JITVERIFY interp store %08x size %d\n", interp_vlog[i].address, interp_vlog[i].size);
		for (i = 0; i < jit_vlog_count; i++)
			fprintf(stderr, "SH2JITVERIFY jit store %p size %d\n", (void *)jit_vlog[i].p, jit_vlog[i].size);
		for (i = 0; i < (int)block->len; i++)
			fprintf(stderr, "SH2JITVERIFY op %08x %04x\n", save.pc + i * 2, jit_fetch(save.pc + i * 2));
		exit(1);
	}
	return sh2.pc;
}
#endif


/*************************************
 *  Block translation
 *************************************/

/* the opcode the interpreter would run */
static UINT16 jit_fetch(UINT32 pc)
{
	return sh2.opcode_read ? sh2.opcode_read(pc) : RW(pc & AM);
}

/* can 'slot' be translated as the delay slot of 'op'? */
static int jit_slot_ok(UINT16 op, UINT16 slot)
{
	struct jit_op so;
	jit_decode(slot, 0, &so);
	if (so.kind == JIT_END || so.kind == JIT_BRANCH || so.pcrel)
		return 0;
	/* DT reads the next opcode for its busy loop: not from a slot */
	if ((slot & 0xf0ff) == 0x4010)
		return 0;
	if (jit_branch_links(op) && jit_touches_pr(slot))
		return 0;
	return 1;
}

/*
	The idle loop of Sega's SGL library (vfremix, twcup98 and others),
	where a CPU waits for a flag set by an interrupt, counting the
	iterations in or out of vblank:

	loop:	MOV.W	@R7,R2		; VDP2 TVSTAT
		EXTU.W	R2,R2
		AND	R1,R2			; R1 = 8: the vblank bit
		TST	R2,R2
		BT	out			; not in vblank
		MOV.L	@R5,R3			; ++*R5
		ADD	#1,R3
		MOV.L	R3,@R5
		BRA	next
		NOP
	out:	MOV.L	@R4,R2			; ++*R4
		ADD	#1,R2
		MOV.L	R2,@R4
	next:	MOV.L	@R6,R0
		CMP/EQ	#1,R0
		BT	loop			; while *R6 == 1

	Within a timeslice nothing can change *R6 (the other CPU and the
	interrupts run between timeslices) nor the vblank bit (the line
	counter changes between timeslices too), so every iteration takes
	the same path: 15 cycles out of vblank, 16 in. The whole iterations
	the timeslice has room for are done at once, leaving one and the
	rest to the code; registers and flags are those every iteration
	sets again.
*/
#define JIT_IDLE_LEN	16
static const UINT16 jit_idle_ops[JIT_IDLE_LEN] =
{
	0x6271, 0x622d, 0x2219, 0x2228, 0x8904, 0x6352, 0x7301, 0x2532,
	0xa003, 0x0009, 0x6242, 0x7201, 0x2422, 0x6062, 0x8801, 0x89ef
};

/* the host word of the RAM long at 'address', or NULL if it is not plain RAM of this CPU */
static INLINE UINT32 *jit_idle_ram(UINT32 address)
{
	UINT8 *page;
	if ((address & 3) || (address & 0xc0000000))
		return NULL;
	page = sh2.jit_wpage[(address & AM) >> JIT_PAGE_SHIFT];
	return page ? (UINT32 *)(page + (address & ((1 << JIT_PAGE_SHIFT) - 1))) : NULL;
}

static void jit_idle_skip(void)
{
	UINT32 *flag, *count;
	int cost, k;

	if (sh2.delay || sh2.test_irq || !sh2.jit_wpage || ((sh2.r[7] & AM) != 0x05f80004))
		return;
	flag = jit_idle_ram(sh2.r[6]);
	if (!flag || *flag != 1)
		return;
	if (((UINT16)RW(sh2.r[7]) & sh2.r[1]) == 0)
	{
		count = jit_idle_ram(sh2.r[4]);
		cost = 15;
	}
	else
	{
		count = jit_idle_ram(sh2.r[5]);
		cost = 16;
	}
	if (!count || count == flag)
		return;
	/* leave a whole iteration and the part of one that ends the timeslice to the code */
	k = (sh2_icount - 1) / cost - 1;
	if (k <= 0)
		return;
	*count += k;
	sh2_icount -= k * cost;
}

static void jit_compile(struct jit_block *block, UINT32 pc)
{
	static struct jit_ctx ctx;
	static UINT16 ops[JIT_MAX_BLOCK + 2];
	static struct jit_op dec[JIT_MAX_BLOCK + 2];
	struct jit_fastram *ram;
	struct sljit_compiler *C;
	struct sljit_label *entry;
	UINT32 count = 0, maxcount, i, cost = 0;
	int ends = 0;

	block->pc = pc;
	block->cost = 0;
	block->len = 0;
	block->valid = 1;
	block->code = NULL;
	block->entry = NULL;
	block->idle = 0;
	block->op0 = jit_fetch(pc);

	/* code must sit in one fast RAM region, below the special areas */
	if ((pc & 1) || (pc & 0xc0000000))
		return;
	ram = jit_find_ram(pc & AM, 2);
	if (!ram)
		return;
	maxcount = (ram->end - (pc & AM) + 1) / 2;
	if (maxcount > JIT_MAX_BLOCK)
		maxcount = JIT_MAX_BLOCK;
	for (i = 0; i < maxcount; i++)
		ops[i] = jit_fetch(pc + i * 2);

	ops[maxcount] = (maxcount < (ram->end - (pc & AM) + 1) / 2) ? jit_fetch(pc + maxcount * 2) : 0;

	if (maxcount >= JIT_IDLE_LEN && memcmp(ops, jit_idle_ops, sizeof(jit_idle_ops)) == 0)
		block->idle = 1;

	/* walk the straight path: conditional branches fall through, jumps end the block */
	while (count < maxcount && !ends)
	{
		jit_decode(ops[count], ops[count + 1], &dec[count]);
		if (dec[count].kind == JIT_END)
			break;
		if (dec[count].kind == JIT_BRANCH && dec[count].slot)
		{
			if (count + 2 > maxcount || !jit_slot_ok(ops[count], ops[count + 1]))
				break;
			jit_decode(ops[count + 1], 0, &dec[count + 1]);
			/* BT/S and BF/S go on with the slot as the next instruction */
			if (((ops[count] >> 12) & 15) != 8)
				ends = 1;
			count += 2;
			continue;
		}
		count++;
	}
	if (count == 0)
		return;

	/* cycles the block takes before each instruction, and at most in all */
	ctx.pre[0] = 0;
	for (i = 0; i < count; i++)
	{
		ctx.pre[i + 1] = ctx.pre[i] + dec[i].acct;
		cost += dec[i].cost;
	}

	C = sljit_create_compiler(NULL);
	if (!C)
		return;
	ctx.C = C;
	ctx.pc = pc;
	ctx.count = count;
	ctx.nexits = 0;
	ctx.overflow = 0;
#ifdef SH2JIT_VERIFY
	sljit_emit_enter(C, 0, SLJIT_ARGS0(32), 4, 3, 0);
#else
	sljit_emit_enter(C, 0, SLJIT_ARGS0(32), 4, 3, 0);
#endif
	/* the active SH-2's state (its context buffer, see sh2p) */
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S0, 0, SLJIT_MEM0(), (sljit_sw)&sh2p);
	entry = sljit_emit_label(C);

	for (i = 0; i < count; i++)
	{
		UINT32 ipc = pc + i * 2;
		if (dec[i].kind == JIT_BRANCH)
		{
			jit_emit_branch(&ctx, ops[i], ops[i + 1], ipc, i);
			/* a jump ran its slot too */
			if (dec[i].slot && ((ops[i] >> 12) & 15) != 8)
				i++;
		}
		else
			jit_emit_one(&ctx, ops[i], &dec[i], ipc, i);
	}

	/* straight off the end */
	if (!ends)
		jit_emit_leave(C, ctx.pre[count], SLJIT_IMM, (sljit_sw)(INT32)(pc + count * 2));

	/* side exits: resume at that instruction with the interpreter */
	for (i = 0; i < (UINT32)ctx.nexits; i++)
	{
		sljit_set_label(ctx.exits[i].jump, sljit_emit_label(C));
		jit_emit_exit(C, ctx.pre[ctx.exits[i].index], pc + ctx.exits[i].index * 2);
	}

	if (ctx.overflow)
	{
		/* a jump without a label must not reach sljit_generate_code */
		sljit_free_compiler(C);
		return;
	}

	/* bounded code memory: start again once the budget is used */
	if (jit_code_bytes > JIT_CODE_BUDGET)
	{
		jit_flush();
		block->pc = pc;
		block->valid = 1;
	}
	block->code = sljit_generate_code(C, 0, NULL);
	if (block->code)
	{
		sljit_uw n_entry = sljit_get_label_addr(entry);
		/* the idle loop goes through the dispatcher, where it is skipped */
		block->entry = block->idle ? NULL : (void *)n_entry;
		jit_code_bytes += sljit_get_generated_code_size(C);
	}
	sljit_free_compiler(C);
	if (!block->code)
	{
		if (!jit_nomem_logged)
		{
			jit_nomem_logged = 1;
			log_cb(RETRO_LOG_WARN, LOGPRE "SH-2 recompiler: no executable memory, using the interpreter\n");
		}
		return;
	}
	block->cost = cost;
	block->len = count;

	/* remember where translated code lives */
	if (!ram->readonly)
	{
		UINT32 first = ram->mapoff + (((pc & AM) - ram->start) >> 2);
		UINT32 last = ram->mapoff + (((pc & AM) + count * 2 - 1 - ram->start) >> 2);
		UINT32 p, c;
		for (i = first; i <= last; i++)
			jit_codemap[i] = 1;
		/* stores to these pages go through the memory system, which flushes on code */
		for (p = (pc & AM) >> JIT_PAGE_SHIFT; p <= ((pc & AM) + count * 2 - 1) >> JIT_PAGE_SHIFT; p++)
		{
			jit_codepage[p] = 1;
			for (c = 0; c < MAX_CPU; c++)
				if (jit_wpage[c])
					jit_wpage[c][p] = NULL;
		}
		sh2_jit_code_present = 1;
	}
}

/*
	Runs the block at the PC if there is one and the timeslice has room
	for all of it: returns 1 when it ran at least one instruction
	(sh2.pc and sh2_icount updated). Near the end of a timeslice the
	interpreter steps through the last instructions one by one: blocks
	are only translated when a long one could run, so those steps do not
	leave blocks starting at every instruction.
*/
static INLINE int sh2_jit_run(void)
{
	struct jit_block *block = &jit_table[jit_table_index(sh2.pc)];
	int before;

	if (!block->valid || block->pc != sh2.pc || (!block->cost && block->op0 != jit_fetch(sh2.pc)))
	{
		if (sh2_icount < JIT_MAX_BLOCK * 3)
			return 0;
		if (block->code)
			sljit_free_code(block->code, NULL);
		jit_compile(block, sh2.pc);
	}
	if (!block->cost)
		return 0;
	if (!sh2.jit_rpage)
	{
		int cpu = cpu_getactivecpu();
		if (!jit_rpage[cpu] && !jit_build_pages(cpu))
			return 0;
		sh2.jit_rpage = jit_rpage[cpu];
		sh2.jit_wpage = jit_wpage[cpu];
	}
	before = sh2_icount;
	if (block->idle)
		jit_idle_skip();
	if (block->cost > sh2_icount)
		return sh2_icount != before;	/* the iterations skipped, if any: the rest steps */
#ifdef SH2JIT_VERIFY
	sh2.pc = jit_verify_block(block);
#else
	sh2.pc = ((jit_func)block->code)();
#endif
	if (jit_flush_pending)
	{
		jit_flush_pending = 0;
		jit_flush();
	}
	if (sh2_icount == before)
		return 0;
	change_pc32bedw(sh2.pc & AM);
	/* an interrupt a memory handler raised is taken after its instruction */
	if (sh2.test_irq)
	{
		CHECK_PENDING_IRQ("sh2_jit_run");
		sh2.test_irq = 0;
	}
	return 1;
}

#else	/* !SH2JIT_SUPPORTED */

int sh2_jit_code_present;
static const int jit_enabled = 0;
static INLINE int sh2_jit_run(void) { return 0; }
static void jit_flush(void) { }
void sh2_jit_ram_written(UINT32 address, UINT32 bytes) { }
void sh2_jit_add_fastram(offs_t start, offs_t end, int readonly, void *base) { }
void sh2_jit_set(int enable) { }

#endif	/* SH2JIT_SUPPORTED */
