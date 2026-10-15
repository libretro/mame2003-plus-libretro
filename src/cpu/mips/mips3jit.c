/***************************************************************************

    mips3jit.c

    Optional MIPS3 recompiler built on sljit (included by mips3.c).

    Straight-line runs of supported instructions (integer ALU, loads and
    stores), ending at a branch (with its delay slot) or at the first
    unsupported instruction, are translated into native code. Everything
    else stays with the interpreter, which also runs whenever fewer cycles
    are left in the timeslice than a block needs, so the instruction
    timing is exactly the interpreter's.

    Loads and stores go straight to fast RAM. Any other address, or a
    store over translated code, leaves the block just before that
    instruction ("side exit") and the interpreter runs it, with its
    memory handlers, PC and timing exactly as without the recompiler.

    Only code in fast RAM (mips3_add_fastram) is translated; writes to
    fast RAM that touch translated code flush the translation cache.

    On 32-bit hosts each 64-bit MIPS register is handled as two 32-bit
    halves (low word first, as the little-endian host stores it); the few
    64-bit instructions without a simple pair translation (variable
    doubleword shifts) are left to the interpreter.

    Off unless a driver enables it with mips3_set_jit().

***************************************************************************/

#if MIPS3_JIT_SUPPORTED

#define JIT_TABLE_BITS		16
#define JIT_TABLE_SIZE		(1 << JIT_TABLE_BITS)
#define JIT_MAX_BLOCK		64				/* instructions */
#define JIT_CODEMAP_BITS	20				/* host address granules of 64 bytes, one byte each */
#define JIT_CODEMAP_SIZE	(1 << JIT_CODEMAP_BITS)
#define JIT_MAX_EXITS		(JIT_MAX_BLOCK * 4)

struct jit_block
{
	UINT32		pc;				/* MIPS address of the first instruction */
	UINT32		len;			/* instructions (cycles) in the block, 0 = not translatable */
	UINT32		lastpc;			/* MIPS address of the last instruction executed */
	UINT8		valid;
	void *		code;
};

typedef sljit_u32 (SLJIT_FUNC *jit_func)(void);

static int jit_enabled;
static struct jit_block *jit_table;
static UINT8 *jit_codemap;
static int jit_code_present;
static UINT32 jit_flushes;


static INLINE UINT32 jit_codemap_index(const void *host)
{
	return (UINT32)(((size_t)host >> 6) & (JIT_CODEMAP_SIZE - 1));
}

static void jit_flush(void)
{
	int i;

	if (!jit_table)
		return;
	for (i = 0; i < JIT_TABLE_SIZE; i++)
	{
		if (jit_table[i].code)
			sljit_free_code(jit_table[i].code, NULL);
		jit_table[i].code = NULL;
		jit_table[i].valid = 0;
	}
	memset(jit_codemap, 0, JIT_CODEMAP_SIZE);
	jit_code_present = 0;
	jit_flushes++;
}

/* called by the fast RAM write handlers */
static INLINE void jit_check_write(const void *host)
{
	if (jit_code_present)
	{
		if (jit_codemap[jit_codemap_index(host)])
			jit_flush();
	}
}

static void jit_exit(void)
{
	jit_flush();
	free(jit_table);
	free(jit_codemap);
	jit_table = NULL;
	jit_codemap = NULL;
	jit_enabled = 0;
}

static void jit_init(void)
{
	jit_table = calloc(JIT_TABLE_SIZE, sizeof(jit_table[0]));
	jit_codemap = calloc(JIT_CODEMAP_SIZE, 1);
	if (!jit_table || !jit_codemap)
	{
		free(jit_table);
		free(jit_codemap);
		jit_table = NULL;
		jit_codemap = NULL;
		jit_enabled = 0;
		return;
	}
	jit_code_present = 0;
	jit_flushes = 0;
}


/*************************************
 *  Code generation helpers
 *************************************/

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define JIT_HOST64		1
#else
#define JIT_HOST64		0
#endif

/* register, HI and LO in mips3_regs (on 32-bit hosts: their low word; */
/* the high word is at +4, as the host is little-endian) */
#define JOFF_R(n)		((sljit_sw)(offsetof(mips3_regs, r) + (n) * sizeof(UINT64)))
#define JOFF_HI			((sljit_sw)offsetof(mips3_regs, hi))
#define JOFF_LO			((sljit_sw)offsetof(mips3_regs, lo))
#define JMEM(off)		SLJIT_MEM1(SLJIT_S0), (off)
#define JREG(n)			JMEM(JOFF_R(n))
#define JREGH(n)		JMEM(JOFF_R(n) + 4)
#define JHI				JMEM(JOFF_HI)
#define JLO				JMEM(JOFF_LO)

#define OPC_RS(o)		(((o) >> 21) & 31)
#define OPC_RT(o)		(((o) >> 16) & 31)
#define OPC_RD(o)		(((o) >> 11) & 31)
#define OPC_SA(o)		(((o) >> 6) & 31)
#define OPC_SIMM(o)		((sljit_sw)(INT16)(o))
#define OPC_UIMM(o)		((sljit_sw)(UINT16)(o))

/*
	Register helpers. Scratch registers R0-R3 only: S1 and S2 hold a
	branch condition and a delay slot's host address around these.
*/

#if JIT_HOST64

/* result of a 32-bit operation in R0, sign extended into a register */
static void jit_store32(struct sljit_compiler *C, int reg)
{
	sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), SLJIT_R0, 0);
}

/* unsigned 32-bit value in R0 (or an immediate), zero extended into a register */
static void jit_store_u32(struct sljit_compiler *C, int reg, sljit_s32 src, sljit_sw srcw)
{
	sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R0, 0, src, srcw);
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), SLJIT_R0, 0);
}

/* 64-bit copy between register file slots */
static void jit_move64(struct sljit_compiler *C, sljit_sw dst, sljit_sw src)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JMEM(src));
	sljit_emit_op1(C, SLJIT_MOV, JMEM(dst), SLJIT_R0, 0);
}

/* 0/1 from a 64-bit comparison of two operands into a register */
static void jit_setcc(struct sljit_compiler *C, int reg, sljit_s32 cond,
	int rs, sljit_s32 src2, sljit_sw src2w, sljit_sw src2hw)
{
	(void)src2hw;
	sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET(cond & ~1), JREG(rs), src2, src2w);
	sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R0, 0, cond);
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), SLJIT_R0, 0);
}

/* 64-bit AND/OR/XOR (NOR with 'invert') rd = rs op rt */
static void jit_logic64(struct sljit_compiler *C, sljit_s32 op, int rd, int rs, int rt, int invert)
{
	sljit_emit_op2(C, op, SLJIT_R0, 0, JREG(rs), JREG(rt));
	if (invert)
		sljit_emit_op2(C, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
}

/* ORI/XORI: rt = rs op zero-extended immediate */
static void jit_logic_imm(struct sljit_compiler *C, sljit_s32 op, int rt, int rs, sljit_sw imm)
{
	sljit_emit_op2(C, op, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, imm);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0);
}

/* DADDU/DSUBU rd = rs +/- rt */
static void jit_add64(struct sljit_compiler *C, int sub, int rd, int rs, int rt)
{
	sljit_emit_op2(C, sub ? SLJIT_SUB : SLJIT_ADD, SLJIT_R0, 0, JREG(rs), JREG(rt));
	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
}

/* DADDIU rt = rs + sign-extended immediate */
static void jit_addi64(struct sljit_compiler *C, int rt, int rs, sljit_sw imm)
{
	sljit_emit_op2(C, SLJIT_ADD, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, imm);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0);
}

/* DSLL/DSRL/DSRA by 0-63 */
static void jit_shift64(struct sljit_compiler *C, sljit_s32 op, int rd, int rt, int amount)
{
	sljit_emit_op2(C, op, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, amount);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
}

/* 64-bit value at 'host' into a register (LD) */
static void jit_load64(struct sljit_compiler *C, int rt, sljit_s32 host)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(host), 0);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0);
}

/* a register to 'host' (SD) */
static void jit_store64(struct sljit_compiler *C, int rt, sljit_s32 host)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rt));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(host), 0, SLJIT_R0, 0);
}

#else	/* 32-bit host: low and high words */

static void jit_store32(struct sljit_compiler *C, int reg)
{
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), SLJIT_R0, 0);
	sljit_emit_op2(C, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(reg), SLJIT_R0, 0);
}

static void jit_store_u32(struct sljit_compiler *C, int reg, sljit_s32 src, sljit_sw srcw)
{
	if (src != SLJIT_R0 && src != SLJIT_IMM)
	{
		sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, src, srcw);
		src = SLJIT_R0;
		srcw = 0;
	}
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), src, srcw);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(reg), SLJIT_IMM, 0);
}

static void jit_move64(struct sljit_compiler *C, sljit_sw dst, sljit_sw src)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JMEM(src));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JMEM(src + 4));
	sljit_emit_op1(C, SLJIT_MOV, JMEM(dst), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JMEM(dst + 4), SLJIT_R1, 0);
}

/*
	64-bit comparison: the high words decide (signed for SLT, unsigned
	for SLTU) unless they are equal, then the low words (unsigned).
	src2 is a register (src2w its low word offset, src2hw its high word
	offset) or an immediate (src2w low word, src2hw high word).
*/
static void jit_setcc(struct sljit_compiler *C, int reg, sljit_s32 cond,
	int rs, sljit_s32 src2, sljit_sw src2w, sljit_sw src2hw)
{
	struct sljit_jump *differ, *done;

	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREGH(rs));
	if (src2 == SLJIT_IMM)
		sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, src2hw);
	else
		sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, src2, src2hw);
	differ = sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);

	/* equal high words: the low words, unsigned */
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs));
	if (src2 == SLJIT_IMM)
		sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, src2w);
	else
		sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, src2, src2w);
	sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_LESS, SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_LESS);
	done = sljit_emit_jump(C, SLJIT_JUMP);

	sljit_set_label(differ, sljit_emit_label(C));
	sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET(cond & ~1), SLJIT_R0, 0, SLJIT_R1, 0);
	sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R2, 0, cond);

	sljit_set_label(done, sljit_emit_label(C));
	jit_store_u32(C, reg, SLJIT_R2, 0);
}

static void jit_logic64(struct sljit_compiler *C, sljit_s32 op, int rd, int rs, int rt, int invert)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rs));
	sljit_emit_op2(C, op, SLJIT_R0, 0, SLJIT_R0, 0, JREG(rt));
	sljit_emit_op2(C, op, SLJIT_R1, 0, SLJIT_R1, 0, JREGH(rt));
	if (invert)
	{
		sljit_emit_op2(C, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
		sljit_emit_op2(C, SLJIT_XOR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -1);
	}
	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(rd), SLJIT_R1, 0);
}

static void jit_logic_imm(struct sljit_compiler *C, sljit_s32 op, int rt, int rs, sljit_sw imm)
{
	sljit_emit_op2(C, op, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, imm);
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rs));
	sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(rt), SLJIT_R1, 0);
}

/* the carry pair works on registers only, so nothing between can touch the flags */
static void jit_add64(struct sljit_compiler *C, int sub, int rd, int rs, int rt)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rs));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, JREG(rt));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, JREGH(rt));
	sljit_emit_op2(C, (sub ? SLJIT_SUB : SLJIT_ADD) | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
	sljit_emit_op2(C, sub ? SLJIT_SUBC : SLJIT_ADDC, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R3, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(rd), SLJIT_R1, 0);
}

static void jit_addi64(struct sljit_compiler *C, int rt, int rs, sljit_sw imm)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rs));
	sljit_emit_op2(C, SLJIT_ADD | SLJIT_SET_CARRY, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, imm);
	sljit_emit_op2(C, SLJIT_ADDC, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, (imm < 0) ? -1 : 0);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(rt), SLJIT_R1, 0);
}

/* R0 = low word, R1 = high word, shifted by a constant 0-63 */
static void jit_shift64(struct sljit_compiler *C, sljit_s32 op, int rd, int rt, int amount)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rt));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rt));

	if (amount >= 32)
	{
		amount -= 32;
		if (op == SLJIT_SHL)
		{
			sljit_emit_op2(C, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, amount);
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 0);
		}
		else
		{
			sljit_emit_op2(C, op, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_IMM, amount);
			if (op == SLJIT_ASHR)
				sljit_emit_op2(C, SLJIT_ASHR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 31);
			else
				sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 0);
		}
	}
	else if (amount > 0)
	{
		if (op == SLJIT_SHL)
		{
			sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_IMM, 32 - amount);
			sljit_emit_op2(C, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, amount);
			sljit_emit_op2(C, SLJIT_OR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
			sljit_emit_op2(C, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, amount);
		}
		else
		{
			sljit_emit_op2(C, SLJIT_SHL, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, 32 - amount);
			sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, amount);
			sljit_emit_op2(C, SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
			sljit_emit_op2(C, op, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, amount);
		}
	}

	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(rd), SLJIT_R1, 0);
}

/* 'host' is R1 or S2, so the words go through R0 and R2 */
static void jit_load64(struct sljit_compiler *C, int rt, sljit_s32 host)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(host), 0);
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(host), 4);
	sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREGH(rt), SLJIT_R2, 0);
}

static void jit_store64(struct sljit_compiler *C, int rt, sljit_s32 host)
{
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rt));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(host), 0, SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREGH(rt));
	sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(host), 4, SLJIT_R0, 0);
}

/* MULT/DIV results: LO from R0, HI from R1, both sign extended */
static void jit_store_hilo(struct sljit_compiler *C)
{
	sljit_emit_op1(C, SLJIT_MOV, JLO, SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JHI, SLJIT_R1, 0);
	sljit_emit_op2(C, SLJIT_ASHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
	sljit_emit_op2(C, SLJIT_ASHR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 31);
	sljit_emit_op1(C, SLJIT_MOV, JMEM(JOFF_LO + 4), SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JMEM(JOFF_HI + 4), SLJIT_R1, 0);
}

#endif	/* JIT_HOST64 */

/* JAL/JALR link: the return address, zero extended (as the interpreter) */
static void jit_store_link(struct sljit_compiler *C, int reg, UINT32 value)
{
	jit_store_u32(C, reg, SLJIT_IMM, (sljit_sw)value);
}


/*************************************
 *  Instruction translation
 *
 *  Each case mirrors the interpreter in mips3.c exactly, including
 *  which instructions skip a write to r0 and which do not.
 *************************************/

enum
{
	JIT_NONE = 0,		/* not translatable */
	JIT_SIMPLE,			/* translatable, falls through */
	JIT_MEM,			/* load or store, falls through (may side exit) */
	JIT_BRANCH			/* translatable branch or jump (needs a delay slot) */
};

/* translation context of one block */
struct jit_ctx
{
	struct sljit_compiler *	C;
	UINT32					pc;				/* address of the first instruction */
	UINT32					count;			/* instructions on the straight path */
	UINT32					cost[JIT_MAX_BLOCK + 1];	/* cycles before instruction n (prefix sums) */
	UINT32					acct;			/* instruction up to which cycles are already taken */
	UINT8					target[JIT_MAX_BLOCK];		/* instruction n is a jump target in the block */
	struct sljit_label *	label[JIT_MAX_BLOCK];
	int						npending;
	struct
	{
		struct sljit_jump *	jump;
		UINT32				index;
	} pending[JIT_MAX_BLOCK * 2];			/* jumps to labels not emitted yet */
	int						nexits;
	struct
	{
		struct sljit_jump *	jump;
		UINT32				index;			/* instruction to resume at */
		UINT32				cycles;			/* cycles still to take */
	} exits[JIT_MAX_EXITS];
};

static int jit_classify(UINT32 op)
{
	switch (op >> 26)
	{
		case 0x00:	/* SPECIAL */
			switch (op & 63)
			{
				case 0x00: case 0x02: case 0x03: case 0x04: case 0x06: case 0x07:
				case 0x0f:
				case 0x10: case 0x11: case 0x12: case 0x13:
				case 0x18: case 0x19: case 0x1a: case 0x1b:		/* MULT MULTU DIV DIVU */
				case 0x20: case 0x21: case 0x22: case 0x23:
				case 0x24: case 0x25: case 0x26: case 0x27:
				case 0x2a: case 0x2b:
				case 0x2c: case 0x2d: case 0x2e: case 0x2f:
				case 0x38: case 0x3a: case 0x3b: case 0x3c: case 0x3e: case 0x3f:
					return JIT_SIMPLE;
				case 0x14: case 0x16: case 0x17:		/* DSLLV DSRLV DSRAV */
					return JIT_HOST64 ? JIT_SIMPLE : JIT_NONE;
				case 0x08: case 0x09:		/* JR, JALR */
					return JIT_BRANCH;
			}
			return JIT_NONE;

		case 0x01:	/* REGIMM */
			if (OPC_RT(op) == 0x00 || OPC_RT(op) == 0x01)		/* BLTZ, BGEZ */
				return JIT_BRANCH;
			return JIT_NONE;

		case 0x02: case 0x03:			/* J, JAL */
		case 0x04: case 0x05: case 0x06: case 0x07:	/* BEQ, BNE, BLEZ, BGTZ */
			return JIT_BRANCH;

		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		case 0x19:
		case 0x2f:	/* CACHE */
		case 0x33:	/* PREF */
			return JIT_SIMPLE;

		/* LB LH LW LBU LHU LWU SB SH SW LD SD, little-endian CPUs only */
		case 0x20: case 0x21: case 0x23: case 0x24: case 0x25: case 0x27:
		case 0x28: case 0x29: case 0x2b: case 0x37: case 0x3f:
		/* LWL LWR SWL SWR LDL LDR SDL SDR, through the interpreter's helpers */
		case 0x22: case 0x26: case 0x2a: case 0x2e:
		case 0x1a: case 0x1b: case 0x2c: case 0x2d:
			return mips3.bigendian ? JIT_NONE : JIT_MEM;
	}
	return JIT_NONE;
}

static void jit_emit_simple(struct sljit_compiler *C, UINT32 op)
{
	int rs = OPC_RS(op), rt = OPC_RT(op), rd = OPC_RD(op), sa = OPC_SA(op);

	switch (op >> 26)
	{
		case 0x00:	/* SPECIAL */
			switch (op & 63)
			{
				case 0x00:	/* SLL */
					if (rd) { sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, sa); jit_store32(C, rd); }
					break;
				case 0x02:	/* SRL */
					if (rd) { sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, sa); jit_store32(C, rd); }
					break;
				case 0x03:	/* SRA */
					if (rd) { sljit_emit_op2(C, SLJIT_ASHR32, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, sa); jit_store32(C, rd); }
					break;
				case 0x04:	/* SLLV */
					if (rd) { sljit_emit_op2(C, SLJIT_MSHL32, SLJIT_R0, 0, JREG(rt), JREG(rs)); jit_store32(C, rd); }
					break;
				case 0x06:	/* SRLV */
					if (rd) { sljit_emit_op2(C, SLJIT_MLSHR32, SLJIT_R0, 0, JREG(rt), JREG(rs)); jit_store32(C, rd); }
					break;
				case 0x07:	/* SRAV */
					if (rd) { sljit_emit_op2(C, SLJIT_MASHR32, SLJIT_R0, 0, JREG(rt), JREG(rs)); jit_store32(C, rd); }
					break;
				case 0x0f:	/* SYNC */
					break;
				case 0x10:	/* MFHI */
					if (rd) jit_move64(C, JOFF_R(rd), JOFF_HI);
					break;
				case 0x11:	/* MTHI */
					jit_move64(C, JOFF_HI, JOFF_R(rs));
					break;
				case 0x12:	/* MFLO */
					if (rd) jit_move64(C, JOFF_R(rd), JOFF_LO);
					break;
				case 0x13:	/* MTLO */
					jit_move64(C, JOFF_LO, JOFF_R(rs));
					break;
#if JIT_HOST64
				case 0x14:	/* DSLLV */
				case 0x16:	/* DSRLV */
				case 0x17:	/* DSRAV */
					if (rd)
					{
						sljit_s32 shift = ((op & 63) == 0x14) ? SLJIT_MSHL : ((op & 63) == 0x16) ? SLJIT_MLSHR : SLJIT_MASHR;
						sljit_emit_op2(C, shift, SLJIT_R0, 0, JREG(rt), JREG(rs));
						sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
					}
					break;
#endif
				case 0x18:	/* MULT */
				case 0x19:	/* MULTU */
#if JIT_HOST64
					sljit_emit_op1(C, ((op & 63) == 0x18) ? SLJIT_MOV_S32 : SLJIT_MOV_U32, SLJIT_R0, 0, JREG(rs));
					sljit_emit_op1(C, ((op & 63) == 0x18) ? SLJIT_MOV_S32 : SLJIT_MOV_U32, SLJIT_R1, 0, JREG(rt));
					sljit_emit_op2(C, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
					sljit_emit_op2(C, SLJIT_ASHR, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
					sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
					sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
					sljit_emit_op1(C, SLJIT_MOV, JLO, SLJIT_R0, 0);
					sljit_emit_op1(C, SLJIT_MOV, JHI, SLJIT_R1, 0);
#else
					/* 32x32 -> 64 into R1:R0, then both halves sign extended */
					sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs));
					sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREG(rt));
					sljit_emit_op0(C, ((op & 63) == 0x18) ? SLJIT_LMUL_SW : SLJIT_LMUL_UW);
					jit_store_hilo(C);
#endif
					break;
				case 0x1a:	/* DIV */
				case 0x1b:	/* DIVU: nothing happens on a zero divisor */
				{
					struct sljit_jump *zero;
					sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JREG(rt));
					zero = sljit_emit_cmp(C, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 0);
					sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JREG(rs));
					sljit_emit_op0(C, ((op & 63) == 0x1a) ? SLJIT_DIVMOD_S32 : SLJIT_DIVMOD_U32);
#if JIT_HOST64
					sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
					sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
					sljit_emit_op1(C, SLJIT_MOV, JLO, SLJIT_R0, 0);
					sljit_emit_op1(C, SLJIT_MOV, JHI, SLJIT_R1, 0);
#else
					jit_store_hilo(C);
#endif
					sljit_set_label(zero, sljit_emit_label(C));
					break;
				}
				case 0x20:	/* ADD (no overflow trap; writes even r0, like the interpreter) */
					sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(rs), JREG(rt)); jit_store32(C, rd);
					break;
				case 0x21:	/* ADDU */
					if (rd) { sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(rs), JREG(rt)); jit_store32(C, rd); }
					break;
				case 0x22:	/* SUB (as ADD) */
					sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, JREG(rs), JREG(rt)); jit_store32(C, rd);
					break;
				case 0x23:	/* SUBU */
					if (rd) { sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, JREG(rs), JREG(rt)); jit_store32(C, rd); }
					break;
				case 0x24:	/* AND */
					if (rd) jit_logic64(C, SLJIT_AND, rd, rs, rt, 0);
					break;
				case 0x25:	/* OR */
					if (rd) jit_logic64(C, SLJIT_OR, rd, rs, rt, 0);
					break;
				case 0x26:	/* XOR */
					if (rd) jit_logic64(C, SLJIT_XOR, rd, rs, rt, 0);
					break;
				case 0x27:	/* NOR */
					if (rd) jit_logic64(C, SLJIT_OR, rd, rs, rt, 1);
					break;
				case 0x2a:	/* SLT */
					if (rd) jit_setcc(C, rd, SLJIT_SIG_LESS, rs, JREG(rt), JOFF_R(rt) + 4);
					break;
				case 0x2b:	/* SLTU */
					if (rd) jit_setcc(C, rd, SLJIT_LESS, rs, JREG(rt), JOFF_R(rt) + 4);
					break;
				case 0x2c:	/* DADD (as DADDU, but writes even r0) */
					jit_add64(C, 0, rd, rs, rt);
					break;
				case 0x2d:	/* DADDU */
					if (rd) jit_add64(C, 0, rd, rs, rt);
					break;
				case 0x2e:	/* DSUB (as DSUBU, but writes even r0) */
					jit_add64(C, 1, rd, rs, rt);
					break;
				case 0x2f:	/* DSUBU */
					if (rd) jit_add64(C, 1, rd, rs, rt);
					break;
				case 0x38:	/* DSLL */
				case 0x3c:	/* DSLL32 */
					if (rd) jit_shift64(C, SLJIT_SHL, rd, rt, sa + ((op & 4) ? 32 : 0));
					break;
				case 0x3a:	/* DSRL */
				case 0x3e:	/* DSRL32 */
					if (rd) jit_shift64(C, SLJIT_LSHR, rd, rt, sa + ((op & 4) ? 32 : 0));
					break;
				case 0x3b:	/* DSRA */
				case 0x3f:	/* DSRA32 */
					if (rd) jit_shift64(C, SLJIT_ASHR, rd, rt, sa + ((op & 4) ? 32 : 0));
					break;
			}
			break;

		case 0x08:	/* ADDI (no overflow trap) */
		case 0x09:	/* ADDIU */
			if (rt) { sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_SIMM(op)); jit_store32(C, rt); }
			break;
		case 0x0a:	/* SLTI */
			if (rt) jit_setcc(C, rt, SLJIT_SIG_LESS, rs, SLJIT_IMM, OPC_SIMM(op), (OPC_SIMM(op) < 0) ? -1 : 0);
			break;
		case 0x0b:	/* SLTIU (against the sign-extended immediate) */
			if (rt) jit_setcc(C, rt, SLJIT_LESS, rs, SLJIT_IMM, OPC_SIMM(op), (OPC_SIMM(op) < 0) ? -1 : 0);
			break;
		case 0x0c:	/* ANDI */
			if (rt) { sljit_emit_op2(C, SLJIT_AND, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_UIMM(op)); jit_store_u32(C, rt, SLJIT_R0, 0); }
			break;
		case 0x0d:	/* ORI */
			if (rt) jit_logic_imm(C, SLJIT_OR, rt, rs, OPC_UIMM(op));
			break;
		case 0x0e:	/* XORI */
			if (rt) jit_logic_imm(C, SLJIT_XOR, rt, rs, OPC_UIMM(op));
			break;
		case 0x0f:	/* LUI */
			if (rt) { sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(INT32)(OPC_UIMM(op) << 16)); jit_store32(C, rt); }
			break;
		case 0x19:	/* DADDIU */
			if (rt) jit_addi64(C, rt, rs, OPC_SIMM(op));
			break;
		case 0x2f:	/* CACHE */
		case 0x33:	/* PREF */
			break;
	}
}


/* cycles the interpreter takes for an instruction */
static UINT32 jit_cycles(UINT32 op)
{
	if ((op >> 26) == 0x00)
		switch (op & 63)
		{
			case 0x18: case 0x19:	return 1 + 3;		/* MULT MULTU */
			case 0x1a: case 0x1b:	return 1 + 35;		/* DIV DIVU */
		}
	return 1;
}


/*************************************
 *  Loads and stores
 *************************************/

static int jit_mem_size(UINT32 op)
{
	switch (op >> 26)
	{
		case 0x20: case 0x24: case 0x28:			return 1;
		case 0x21: case 0x25: case 0x29:			return 2;
		case 0x23: case 0x27: case 0x2b:			return 4;
		case 0x22: case 0x26: case 0x2a: case 0x2e:	return 4;	/* aligned word */
	}
	return 8;
}

/* LWL/LWR/SWL/SWR/LDL/LDR/SDL/SDR: the interpreter's helper, or NULL */
static void (*jit_mem_helper(UINT32 op))(UINT32)
{
	switch (op >> 26)
	{
		case 0x22:	return lwl_le;
		case 0x26:	return lwr_le;
		case 0x2a:	return swl_le;
		case 0x2e:	return swr_le;
		case 0x1a:	return ldl_le;
		case 0x1b:	return ldr_le;
		case 0x2c:	return sdl_le;
		case 0x2d:	return sdr_le;
	}
	return NULL;
}

static int jit_mem_is_store(UINT32 op)
{
	return (op >> 26) >= 0x28 && (op >> 26) != 0x37;
}

/* leave before instruction 'index' and let the interpreter run it */
static void jit_add_exit(struct jit_ctx *ctx, struct sljit_jump *jump, UINT32 index)
{
	if (ctx->nexits < JIT_MAX_EXITS)
	{
		ctx->exits[ctx->nexits].jump = jump;
		ctx->exits[ctx->nexits].index = index;
		ctx->exits[ctx->nexits].cycles = ctx->cost[index] - ctx->cost[ctx->acct];
		ctx->nexits++;
	}
}

/*
	Host address of a load/store into R1, or a side exit with 'index'
	instructions completed. The same conditions as the interpreter's fast
	RAM handlers: the whole access inside one region; stores also leave
	if they would touch translated code.
*/
static int jit_emit_mem_address(struct jit_ctx *ctx, UINT32 op, UINT32 index)
{
	struct sljit_compiler *C = ctx->C;
	struct sljit_jump *found[MIPS3_MAX_FASTRAM];
	struct sljit_label *ready;
	int size = jit_mem_size(op);
	int i, n = 0;

	/* R0 = (UINT32)(rs + simm), aligned for the unaligned access helpers */
	sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(OPC_RS(op)), SLJIT_IMM, OPC_SIMM(op));
	sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0);
	if (jit_mem_helper(op))
		sljit_emit_op2(C, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, ~(sljit_sw)(size - 1) & 0xffffffff);

	/* R1 = host pointer, from the first region that holds the whole access */
	for (i = 0; i < fastram_count; i++)
	{
		struct sljit_jump *miss;
		if (fastram[i].end - fastram[i].start + 1 < (UINT32)size)
			continue;
		if (fastram[i].readonly && jit_mem_is_store(op))
			continue;
		sljit_emit_op2(C, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)fastram[i].start);
		miss = sljit_emit_cmp(C, SLJIT_GREATER, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)(fastram[i].end - fastram[i].start - (size - 1)));
		sljit_emit_op2(C, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)fastram[i].base);
		found[n++] = sljit_emit_jump(C, SLJIT_JUMP);
		sljit_set_label(miss, sljit_emit_label(C));
	}

	/* not fast RAM: let the interpreter do it */
	jit_add_exit(ctx, sljit_emit_jump(C, SLJIT_JUMP), index);

	ready = sljit_emit_label(C);
	for (i = 0; i < n; i++)
		sljit_set_label(found[i], ready);

	/* a store over translated code: let the interpreter do it (and flush) */
	if (jit_mem_is_store(op))
	{
		int g;
		for (g = 0; g < ((size == 8) ? 2 : 1); g++)
		{
			if (g)
				sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, 7);
			else
				sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_R1, 0);
			sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 6);
			sljit_emit_op2(C, SLJIT_AND, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, JIT_CODEMAP_SIZE - 1);
			sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)jit_codemap);
			sljit_emit_op1(C, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM2(SLJIT_R0, SLJIT_R2), 0);
			jit_add_exit(ctx, sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0), index);
		}
	}
	return 1;
}

/* the access itself, through the host pointer in 'host' */
static void jit_emit_mem_access(struct sljit_compiler *C, UINT32 op, sljit_s32 host)
{
	int rt = OPC_RT(op);
	sljit_s32 load = 0;

	/* the unaligned ones: the interpreter's helper, now that the access is */
	/* known to be in fast RAM (and, for stores, away from translated code) */
	if (jit_mem_helper(op))
	{
		void (*helper)(UINT32) = jit_mem_helper(op);
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)op);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(helper));
		return;
	}

	switch (op >> 26)
	{
		case 0x37:	/* LD */
			if (rt)
				jit_load64(C, rt, host);
			return;
		case 0x20:	load = SLJIT_MOV_S8;	break;		/* LB */
		case 0x21:	load = SLJIT_MOV_S16;	break;		/* LH */
		case 0x23:	load = SLJIT_MOV_S32;	break;		/* LW */
		case 0x24:	load = SLJIT_MOV_U8;	break;		/* LBU */
		case 0x25:	load = SLJIT_MOV_U16;	break;		/* LHU */
		case 0x27:	load = SLJIT_MOV_U32;	break;		/* LWU */

		case 0x28:	/* SB */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rt));
			sljit_emit_op1(C, SLJIT_MOV_U8, SLJIT_MEM1(host), 0, SLJIT_R0, 0);
			return;
		case 0x29:	/* SH */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rt));
			sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(host), 0, SLJIT_R0, 0);
			return;
		case 0x2b:	/* SW */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rt));
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(host), 0, SLJIT_R0, 0);
			return;
		case 0x3f:	/* SD: low word first, as two little-endian words */
			jit_store64(C, rt, host);
			return;
	}

	/* loads into r0 are dropped, as in the interpreter */
	if (rt)
	{
		sljit_emit_op1(C, load, SLJIT_R0, 0, SLJIT_MEM1(host), 0);
		if (load == SLJIT_MOV_U8 || load == SLJIT_MOV_U16 || load == SLJIT_MOV_U32)
			jit_store_u32(C, rt, SLJIT_R0, 0);
		else
			jit_store32(C, rt);
	}
}

/* branch kinds */
static int jit_branch_is_conditional(UINT32 op)
{
	switch (op >> 26)
	{
		case 0x01: case 0x04: case 0x05: case 0x06: case 0x07:
			return 1;
	}
	return 0;
}

/* the register a branch links into, or -1 */
static int jit_branch_link(UINT32 op)
{
	if ((op >> 26) == 0x03)
		return 31;
	if ((op >> 26) == 0x00 && (op & 63) == 0x09)
		return OPC_RD(op);
	return -1;
}

/* can 'slot' be translated as the delay slot of 'op'? */
static int jit_slot_ok(UINT32 op, UINT32 slot)
{
	int kind = jit_classify(slot);
	if (kind == JIT_SIMPLE)
		return 1;
	/* a load/store is checked before the branch, so it must not use the link register */
	return kind == JIT_MEM && (int)OPC_RS(slot) != jit_branch_link(op);
}

/* take the cycles of the instructions up to 'index' from the timeslice */
static void jit_emit_cycles_to(struct jit_ctx *ctx, UINT32 index)
{
	UINT32 cycles = ctx->cost[index] - ctx->cost[ctx->acct];
	if (cycles)
		sljit_emit_op2(ctx->C, SLJIT_SUB32, SLJIT_MEM0(), (sljit_sw)&mips3_icount, SLJIT_MEM0(), (sljit_sw)&mips3_icount, SLJIT_IMM, cycles);
}

/* target address -> instruction index inside the block, or -1 */
static int jit_target_index(struct jit_ctx *ctx, UINT32 target)
{
	if (target >= ctx->pc && target < ctx->pc + ctx->count * 4 && !(target & 3))
		return (target - ctx->pc) / 4;
	return -1;
}

/*
	Leave for 'target' after the instructions up to 'index'. A target
	inside the block is reached in native code while the timeslice has
	room for the rest of the block from there, exactly as the dispatcher
	would enter a block there; otherwise we return to the dispatcher.
*/
static void jit_emit_leave(struct jit_ctx *ctx, UINT32 index, sljit_s32 src, sljit_sw srcw)
{
	struct sljit_compiler *C = ctx->C;

	jit_emit_cycles_to(ctx, index);
	if (src == SLJIT_IMM)
	{
		int t = jit_target_index(ctx, (UINT32)srcw);
		if (t >= 0 && ctx->target[t] && ctx->npending < JIT_MAX_BLOCK * 2)
		{
			struct sljit_jump *jump = sljit_emit_cmp(C, SLJIT_SIG_GREATER_EQUAL | SLJIT_32,
				SLJIT_MEM0(), (sljit_sw)&mips3_icount, SLJIT_IMM, ctx->cost[ctx->count] - ctx->cost[t]);
			if (ctx->label[t])
				sljit_set_label(jump, ctx->label[t]);
			else
			{
				ctx->pending[ctx->npending].jump = jump;
				ctx->pending[ctx->npending].index = t;
				ctx->npending++;
			}
		}
	}
	sljit_emit_return(C, SLJIT_MOV32, src, srcw);
}

/* condition of a conditional branch into 'dst' (0/1) */
static void jit_emit_condition(struct sljit_compiler *C, UINT32 op, sljit_s32 dst)
{
	int rs = OPC_RS(op), rt = OPC_RT(op);

#if !JIT_HOST64
	/* 32-bit host: the sign is in the high word, zero needs both */
	switch (op >> 26)
	{
		case 0x01:	/* BLTZ / BGEZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_LESS, JREGH(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, (rt == 0x00) ? SLJIT_SIG_LESS : SLJIT_SIG_GREATER_EQUAL);
			break;
		case 0x04:	/* BEQ */
		case 0x05:	/* BNE */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs));
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rs));
			sljit_emit_op2(C, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, JREG(rt));
			sljit_emit_op2(C, SLJIT_XOR, SLJIT_R1, 0, SLJIT_R1, 0, JREGH(rt));
			sljit_emit_op2(C, SLJIT_OR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, ((op >> 26) == 0x04) ? SLJIT_EQUAL : SLJIT_NOT_EQUAL);
			break;
		case 0x06:	/* BLEZ: negative, or zero */
		case 0x07:	/* BGTZ: the opposite */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREGH(rs));
			sljit_emit_op2(C, SLJIT_OR | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R1, 0, JREG(rs));
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, SLJIT_EQUAL);
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_R1, 0, SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_OR, dst, 0, SLJIT_SIG_LESS);
			if ((op >> 26) == 0x07)
				sljit_emit_op2(C, SLJIT_XOR, dst, 0, dst, 0, SLJIT_IMM, 1);
			break;
	}
	return;
#endif

	switch (op >> 26)
	{
		case 0x01:	/* BLTZ / BGEZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_LESS, JREG(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, (rt == 0x00) ? SLJIT_SIG_LESS : SLJIT_SIG_GREATER_EQUAL);
			break;
		case 0x04:	/* BEQ */
		case 0x05:	/* BNE */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREG(rs));
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R1, 0, JREG(rt));
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, ((op >> 26) == 0x04) ? SLJIT_EQUAL : SLJIT_NOT_EQUAL);
			break;
		case 0x06:	/* BLEZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_LESS_EQUAL, JREG(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, SLJIT_SIG_LESS_EQUAL);
			break;
		case 0x07:	/* BGTZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_GREATER, JREG(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, dst, 0, SLJIT_SIG_GREATER);
			break;
	}
}

static UINT32 jit_branch_target(UINT32 op, UINT32 pc)
{
	if ((op >> 26) == 0x02 || (op >> 26) == 0x03)
		return ((pc + 4) & 0xf0000000) | ((op & 0x03ffffff) << 2);
	return pc + 4 + ((INT32)(INT16)op << 2);
}

/* a plain instruction or a load/store, at instruction 'index' */
static void jit_emit_one(struct jit_ctx *ctx, UINT32 op, UINT32 index)
{
	if (jit_classify(op) == JIT_MEM)
	{
		jit_emit_mem_address(ctx, op, index);
		jit_emit_mem_access(ctx->C, op, SLJIT_R1);
	}
	else
		jit_emit_simple(ctx->C, op);
}

/*
	Branch at instruction 'index' with its delay slot.
	The condition (or jump target) goes into S1 before the delay slot
	runs, and links are written before it too, as in the interpreter.
	Unconditional jumps leave the block; conditional branches leave it
	(or jump inside it) when taken and fall through otherwise.
*/
static void jit_emit_branch(struct jit_ctx *ctx, UINT32 op, UINT32 slot, UINT32 index)
{
	struct sljit_compiler *C = ctx->C;
	UINT32 pc = ctx->pc + index * 4;
	int slot_mem = (jit_classify(slot) == JIT_MEM);
	UINT32 next = pc + 8;
	UINT32 target = jit_branch_target(op, pc);

	/* a load/store in the delay slot is checked first: a side exit then */
	/* hands the whole branch to the interpreter */
	if (slot_mem)
	{
		jit_emit_mem_address(ctx, slot, index);
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S2, 0, SLJIT_R1, 0);
	}

	switch (op >> 26)
	{
		case 0x00:	/* JR / JALR: target is the low 32 bits of rs */
			sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_S1, 0, JREG(OPC_RS(op)));
			if ((op & 63) == 0x09)		/* JALR links into rd, even r0 */
				jit_store_link(C, OPC_RD(op), next);
			break;
		case 0x02:	/* J */
			break;
		case 0x03:	/* JAL */
			jit_store_link(C, 31, next);
			break;
		default:
			jit_emit_condition(C, op, SLJIT_S1);
			break;
	}

	/* the delay slot */
	if (slot_mem && jit_mem_helper(slot))
		jit_emit_mem_access(C, slot, SLJIT_S2);
	else if (slot_mem)
		jit_emit_mem_access(C, slot, SLJIT_S2);
	else
		jit_emit_simple(C, slot);

	/* where to go */
	if ((op >> 26) == 0x00)
		jit_emit_leave(ctx, index + 2, SLJIT_S1, 0);
	else if (!jit_branch_is_conditional(op))
		jit_emit_leave(ctx, index + 2, SLJIT_IMM, (sljit_sw)target);
	else
	{
		/* not taken: carry on with the next instruction in this block */
		struct sljit_jump *fallthrough = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_S1, 0, SLJIT_IMM, 0);
		jit_emit_leave(ctx, index + 2, SLJIT_IMM, (sljit_sw)target);
		sljit_set_label(fallthrough, sljit_emit_label(C));
	}
}

/*
	The compiler idiom "bnez r, X / beqz r, X / d" (or beqz then bnez),
	a branch in a delay slot. Exactly one of the two is taken: the first
	one goes to X with the second as a no-op delay slot (2 instructions),
	otherwise the second goes to X after its delay slot d (3 instructions).
*/
static int jit_is_branch_pair(UINT32 op, UINT32 slot, UINT32 slot2)
{
	UINT32 a = op >> 26, b = slot >> 26;
	if (!((a == 0x04 && b == 0x05) || (a == 0x05 && b == 0x04)))
		return 0;
	if (OPC_RS(op) != OPC_RS(slot) || OPC_RT(op) != OPC_RT(slot))
		return 0;
	/* same target: offsets differ by one instruction */
	if ((INT16)slot != (INT16)op - 1)
		return 0;
	return jit_classify(slot2) == JIT_SIMPLE;
}

static void jit_emit_branch_pair(struct jit_ctx *ctx, UINT32 op, UINT32 slot2, UINT32 index)
{
	struct sljit_compiler *C = ctx->C;
	UINT32 target = jit_branch_target(op, ctx->pc + index * 4);
	struct sljit_jump *second;

	jit_emit_condition(C, op, SLJIT_S1);
	second = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_S1, 0, SLJIT_IMM, 0);
	jit_emit_leave(ctx, index + 2, SLJIT_IMM, (sljit_sw)target);
	sljit_set_label(second, sljit_emit_label(C));
	jit_emit_simple(C, slot2);
	jit_emit_leave(ctx, index + 3, SLJIT_IMM, (sljit_sw)target);
}


/*************************************
 *  Block translation
 *************************************/

static void jit_compile(struct jit_block *block, UINT32 pc)
{
	static struct jit_ctx ctx;
	static UINT8 kind[JIT_MAX_BLOCK];		/* 0 one, 1 branch, 2 branch pair */
	struct mips3_fastram *region;
	struct sljit_compiler *C;
	const UINT8 *host;
	UINT32 count = 0, maxcount, op, i;
	int ends = 0;

	block->pc = pc;
	block->len = 0;
	block->valid = 1;
	block->code = NULL;

	/* code must sit in one fast RAM region */
	region = find_fastram(pc);
	if (!region || (pc & 3))
		return;
	host = region->base + (pc - region->start);
	maxcount = (region->end - pc + 1) / 4;
	if (maxcount > JIT_MAX_BLOCK)
		maxcount = JIT_MAX_BLOCK;

#define JIT_OP(n)	(*(const UINT32 *)(host + (n) * 4))

	memset(kind, 0, sizeof(kind));

	/* walk the straight path: conditional branches fall through, */
	/* unconditional jumps (and branch pairs) end the block */
	while (count < maxcount && !ends)
	{
		op = JIT_OP(count);
		switch (jit_classify(op))
		{
			case JIT_SIMPLE:
			case JIT_MEM:
				kind[count] = 0;
				count++;
				continue;

			case JIT_BRANCH:
				if (count + 3 <= maxcount && jit_is_branch_pair(op, JIT_OP(count + 1), JIT_OP(count + 2)))
				{
					kind[count] = 2;
					count += 3;
					ends = 1;
					continue;
				}
				if (count + 2 <= maxcount && jit_slot_ok(op, JIT_OP(count + 1)))
				{
					kind[count] = 1;
					count += 2;
					if (!jit_branch_is_conditional(op))
						ends = 1;
					continue;
				}
				break;
		}
		break;
	}
	if (count == 0)
		return;

	/* cycles and jump targets */
	memset(&ctx.target, 0, sizeof(ctx.target));
	memset(&ctx.label, 0, sizeof(ctx.label));
	ctx.pc = pc;
	ctx.count = count;
	ctx.cost[0] = 0;
	for (i = 0; i < count; i++)
		ctx.cost[i + 1] = ctx.cost[i] + jit_cycles(JIT_OP(i));
	ctx.target[0] = 1;
	for (i = 0; i < count; i++)
		if (kind[i])
		{
			op = JIT_OP(i);
			if ((op >> 26) != 0x00)
			{
				int t = jit_target_index(&ctx, jit_branch_target(op, pc + i * 4));
				/* only real instruction starts, never a delay slot */
				if (t >= 0)
				{
					UINT32 k;
					int inslot = 0;
					for (k = 0; k < count; k++)
						if (kind[k] && (UINT32)t > k && (UINT32)t <= k + (kind[k] == 2 ? 2 : 1))
							inslot = 1;
					if (!inslot)
						ctx.target[t] = 1;
				}
			}
			i += (kind[i] == 2) ? 2 : 1;
		}

	C = sljit_create_compiler(NULL);
	if (!C)
		return;
	ctx.C = C;
	ctx.acct = 0;
	ctx.nexits = 0;
	ctx.npending = 0;
	sljit_emit_enter(C, 0, SLJIT_ARGS0(32), 4, 3, 0);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&mips3);

	for (i = 0; i < count; i++)
	{
		/* a jump target: take the cycles so far, then the label */
		if (ctx.target[i])
		{
			jit_emit_cycles_to(&ctx, i);
			ctx.acct = i;
			ctx.label[i] = sljit_emit_label(C);
		}

		op = JIT_OP(i);
		switch (kind[i])
		{
			case 0:
				jit_emit_one(&ctx, op, i);
				break;
			case 1:
				jit_emit_branch(&ctx, op, JIT_OP(i + 1), i);
				i++;
				break;
			case 2:
				jit_emit_branch_pair(&ctx, op, JIT_OP(i + 2), i);
				i += 2;
				break;
		}
	}

	/* straight off the end */
	if (!ends)
		jit_emit_leave(&ctx, count, SLJIT_IMM, (sljit_sw)(pc + count * 4));

	/* forward jumps inside the block */
	for (i = 0; i < (UINT32)ctx.npending; i++)
		sljit_set_label(ctx.pending[i].jump, ctx.label[ctx.pending[i].index]);

	/* side exits: resume at that instruction with the interpreter */
	for (i = 0; i < (UINT32)ctx.nexits; i++)
	{
		sljit_set_label(ctx.exits[i].jump, sljit_emit_label(C));
		if (ctx.exits[i].cycles)
			sljit_emit_op2(C, SLJIT_SUB32, SLJIT_MEM0(), (sljit_sw)&mips3_icount, SLJIT_MEM0(), (sljit_sw)&mips3_icount, SLJIT_IMM, ctx.exits[i].cycles);
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)(pc + ctx.exits[i].index * 4));
	}

#undef JIT_OP

	block->len = ctx.cost[count];
	block->lastpc = pc + count * 4 - 4;
	block->code = sljit_generate_code(C, 0, NULL);
	sljit_free_compiler(C);
	if (!block->code)
	{
		block->len = 0;
		return;
	}

	/* remember where translated code lives */
	for (i = 0; i < count; i++)
		jit_codemap[jit_codemap_index(host + i * 4)] = 1;
	jit_code_present = 1;
}

/* returns the block at pc, or NULL if it cannot be translated */
static INLINE struct jit_block *jit_lookup(UINT32 pc)
{
	struct jit_block *block = &jit_table[(pc >> 2) & (JIT_TABLE_SIZE - 1)];

	if (!block->valid || block->pc != pc)
	{
		if (block->code)
			sljit_free_code(block->code, NULL);
		jit_compile(block, pc);
	}
	return block->len ? block : NULL;
}

#else	/* !MIPS3_JIT_SUPPORTED */

static const int jit_enabled = 0;
static INLINE void jit_check_write(const void *host) { }
static void jit_init(void) { }
static void jit_exit(void) { }

#endif	/* MIPS3_JIT_SUPPORTED */


int mips3_jit_available(void)
{
	return MIPS3_JIT_SUPPORTED;
}


void mips3_set_jit(int enable)
{
#if MIPS3_JIT_SUPPORTED
	if (enable && !jit_table)
	{
		jit_enabled = 1;
		jit_init();
		/* a loaded state rewrites RAM behind our back */
		if (jit_enabled)
			state_save_register_func_postload(jit_flush);
	}
	else if (!enable && jit_table)
		jit_exit();
#endif
}
