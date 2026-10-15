/***************************************************************************

    mips3jit.c

    Optional MIPS3 recompiler built on sljit (included by mips3.c).

    Straight-line runs of supported integer instructions, ending at a
    branch (with its delay slot) or at the first unsupported instruction,
    are translated into native code. Everything else stays with the
    interpreter, which also runs whenever fewer cycles are left in the
    timeslice than a block needs, so the instruction timing is exactly
    the interpreter's.

    Only code in fast RAM (mips3_add_fastram) is translated; writes to
    fast RAM that touch translated code flush the translation cache.

    Off unless a driver enables it with mips3_set_jit().

***************************************************************************/

#if MIPS3_JIT_SUPPORTED

#define JIT_TABLE_BITS		16
#define JIT_TABLE_SIZE		(1 << JIT_TABLE_BITS)
#define JIT_MAX_BLOCK		64				/* instructions */
#define JIT_CODEMAP_BITS	20				/* host address granules of 64 bytes */
#define JIT_CODEMAP_SIZE	(1 << JIT_CODEMAP_BITS)

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
	memset(jit_codemap, 0, JIT_CODEMAP_SIZE / 8);
	jit_code_present = 0;
	jit_flushes++;
}

/* called by the fast RAM write handlers */
static INLINE void jit_check_write(const void *host)
{
	if (jit_code_present)
	{
		UINT32 index = jit_codemap_index(host);
		if (jit_codemap[index >> 3] & (1 << (index & 7)))
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
	jit_codemap = calloc(JIT_CODEMAP_SIZE / 8, 1);
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

#define JREG(n)			SLJIT_MEM1(SLJIT_S0), (sljit_sw)(offsetof(mips3_regs, r) + (n) * sizeof(UINT64))
#define JHI				SLJIT_MEM1(SLJIT_S0), (sljit_sw)offsetof(mips3_regs, hi)
#define JLO				SLJIT_MEM1(SLJIT_S0), (sljit_sw)offsetof(mips3_regs, lo)

#define OPC_RS(o)		(((o) >> 21) & 31)
#define OPC_RT(o)		(((o) >> 16) & 31)
#define OPC_RD(o)		(((o) >> 11) & 31)
#define OPC_SA(o)		(((o) >> 6) & 31)
#define OPC_SIMM(o)		((sljit_sw)(INT16)(o))
#define OPC_UIMM(o)		((sljit_sw)(UINT16)(o))

/* result of a 32-bit operation in R0, sign extended into a register */
static void jit_store32(struct sljit_compiler *C, int reg)
{
	sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), SLJIT_R0, 0);
}

/* 0/1 from a comparison of two operands into a register */
static void jit_setcc(struct sljit_compiler *C, int reg, sljit_s32 cond,
	sljit_s32 src1, sljit_sw src1w, sljit_s32 src2, sljit_sw src2w)
{
	sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET(cond & ~1), src1, src1w, src2, src2w);
	sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R0, 0, cond);
	sljit_emit_op1(C, SLJIT_MOV, JREG(reg), SLJIT_R0, 0);
}

/* 64-bit ALU op rd = rs op rt */
static void jit_op64(struct sljit_compiler *C, sljit_s32 op, int rd, int rs, int rt)
{
	sljit_emit_op2(C, op, SLJIT_R0, 0, JREG(rs), JREG(rt));
	sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
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
	JIT_BRANCH			/* translatable branch or jump (needs a delay slot) */
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
				case 0x14: case 0x16: case 0x17:
				case 0x20: case 0x21: case 0x22: case 0x23:
				case 0x24: case 0x25: case 0x26: case 0x27:
				case 0x2a: case 0x2b:
				case 0x2c: case 0x2d: case 0x2e: case 0x2f:
				case 0x38: case 0x3a: case 0x3b: case 0x3c: case 0x3e: case 0x3f:
					return JIT_SIMPLE;
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
			return JIT_SIMPLE;
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
					if (rd) { sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JHI); sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0); }
					break;
				case 0x11:	/* MTHI */
					sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs)); sljit_emit_op1(C, SLJIT_MOV, JHI, SLJIT_R0, 0);
					break;
				case 0x12:	/* MFLO */
					if (rd) { sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JLO); sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0); }
					break;
				case 0x13:	/* MTLO */
					sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, JREG(rs)); sljit_emit_op1(C, SLJIT_MOV, JLO, SLJIT_R0, 0);
					break;
				case 0x14:	/* DSLLV */
					if (rd) jit_op64(C, SLJIT_MSHL, rd, rt, rs);
					break;
				case 0x16:	/* DSRLV */
					if (rd) jit_op64(C, SLJIT_MLSHR, rd, rt, rs);
					break;
				case 0x17:	/* DSRAV */
					if (rd) jit_op64(C, SLJIT_MASHR, rd, rt, rs);
					break;
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
					if (rd) jit_op64(C, SLJIT_AND, rd, rs, rt);
					break;
				case 0x25:	/* OR */
					if (rd) jit_op64(C, SLJIT_OR, rd, rs, rt);
					break;
				case 0x26:	/* XOR */
					if (rd) jit_op64(C, SLJIT_XOR, rd, rs, rt);
					break;
				case 0x27:	/* NOR */
					if (rd)
					{
						sljit_emit_op2(C, SLJIT_OR, SLJIT_R0, 0, JREG(rs), JREG(rt));
						sljit_emit_op2(C, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1);
						sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
					}
					break;
				case 0x2a:	/* SLT */
					if (rd) jit_setcc(C, rd, SLJIT_SIG_LESS, JREG(rs), JREG(rt));
					break;
				case 0x2b:	/* SLTU */
					if (rd) jit_setcc(C, rd, SLJIT_LESS, JREG(rs), JREG(rt));
					break;
				case 0x2c:	/* DADD (as DADDU, but writes even r0) */
					jit_op64(C, SLJIT_ADD, rd, rs, rt);
					break;
				case 0x2d:	/* DADDU */
					if (rd) jit_op64(C, SLJIT_ADD, rd, rs, rt);
					break;
				case 0x2e:	/* DSUB (as DSUBU, but writes even r0) */
					jit_op64(C, SLJIT_SUB, rd, rs, rt);
					break;
				case 0x2f:	/* DSUBU */
					if (rd) jit_op64(C, SLJIT_SUB, rd, rs, rt);
					break;
				case 0x38:	/* DSLL */
				case 0x3c:	/* DSLL32 */
					if (rd)
					{
						sljit_emit_op2(C, SLJIT_SHL, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, sa + ((op & 4) ? 32 : 0));
						sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
					}
					break;
				case 0x3a:	/* DSRL */
				case 0x3e:	/* DSRL32 */
					if (rd)
					{
						sljit_emit_op2(C, SLJIT_LSHR, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, sa + ((op & 4) ? 32 : 0));
						sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
					}
					break;
				case 0x3b:	/* DSRA */
				case 0x3f:	/* DSRA32 */
					if (rd)
					{
						sljit_emit_op2(C, SLJIT_ASHR, SLJIT_R0, 0, JREG(rt), SLJIT_IMM, sa + ((op & 4) ? 32 : 0));
						sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_R0, 0);
					}
					break;
			}
			break;

		case 0x08:	/* ADDI (no overflow trap) */
		case 0x09:	/* ADDIU */
			if (rt) { sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_SIMM(op)); jit_store32(C, rt); }
			break;
		case 0x0a:	/* SLTI */
			if (rt) jit_setcc(C, rt, SLJIT_SIG_LESS, JREG(rs), SLJIT_IMM, OPC_SIMM(op));
			break;
		case 0x0b:	/* SLTIU (against the sign-extended immediate) */
			if (rt) jit_setcc(C, rt, SLJIT_LESS, JREG(rs), SLJIT_IMM, OPC_SIMM(op));
			break;
		case 0x0c:	/* ANDI */
			if (rt) { sljit_emit_op2(C, SLJIT_AND, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_UIMM(op)); sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0); }
			break;
		case 0x0d:	/* ORI */
			if (rt) { sljit_emit_op2(C, SLJIT_OR, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_UIMM(op)); sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0); }
			break;
		case 0x0e:	/* XORI */
			if (rt) { sljit_emit_op2(C, SLJIT_XOR, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_UIMM(op)); sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0); }
			break;
		case 0x0f:	/* LUI */
			if (rt) sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_IMM, (sljit_sw)(INT32)(OPC_UIMM(op) << 16));
			break;
		case 0x19:	/* DADDIU */
			if (rt) { sljit_emit_op2(C, SLJIT_ADD, SLJIT_R0, 0, JREG(rs), SLJIT_IMM, OPC_SIMM(op)); sljit_emit_op1(C, SLJIT_MOV, JREG(rt), SLJIT_R0, 0); }
			break;
	}
}

/*
	Branch at address 'pc' with its delay slot 'slot'.
	The condition (or jump target) is computed into S1 before the delay
	slot runs, and links are written before it too, as in the interpreter.
	Returns the next PC in R0.
*/
static void jit_emit_branch(struct sljit_compiler *C, UINT32 op, UINT32 slot, UINT32 pc)
{
	int rs = OPC_RS(op), rt = OPC_RT(op), rd = OPC_RD(op);
	UINT32 next = pc + 8;
	UINT32 target = pc + 4 + ((INT32)OPC_SIMM(op) << 2);
	struct sljit_jump *taken;
	int conditional = 1;

	switch (op >> 26)
	{
		case 0x00:	/* JR / JALR: target is the low 32 bits of rs */
			sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_S1, 0, JREG(rs));
			if ((op & 63) == 0x09)		/* JALR links into rd, even r0 */
				sljit_emit_op1(C, SLJIT_MOV, JREG(rd), SLJIT_IMM, (sljit_sw)(UINT32)next);
			conditional = -1;
			break;
		case 0x01:	/* BLTZ / BGEZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_LESS, JREG(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_S1, 0, (rt == 0x00) ? SLJIT_SIG_LESS : SLJIT_SIG_GREATER_EQUAL);
			break;
		case 0x02:	/* J */
		case 0x03:	/* JAL */
			target = ((pc + 4) & 0xf0000000) | ((op & 0x03ffffff) << 2);
			if ((op >> 26) == 0x03)
				sljit_emit_op1(C, SLJIT_MOV, JREG(31), SLJIT_IMM, (sljit_sw)(UINT32)next);
			conditional = 0;
			break;
		case 0x04:	/* BEQ */
		case 0x05:	/* BNE */
			sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, JREG(rs));
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R1, 0, JREG(rt));
			sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_S1, 0, ((op >> 26) == 0x04) ? SLJIT_EQUAL : SLJIT_NOT_EQUAL);
			break;
		case 0x06:	/* BLEZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_LESS_EQUAL, JREG(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_S1, 0, SLJIT_SIG_LESS_EQUAL);
			break;
		case 0x07:	/* BGTZ */
			sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_SIG_GREATER, JREG(rs), SLJIT_IMM, 0);
			sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_S1, 0, SLJIT_SIG_GREATER);
			break;
	}

	/* the delay slot */
	jit_emit_simple(C, slot);

	/* where to go */
	if (conditional < 0)
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_S1, 0);
	else if (conditional == 0)
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)target);
	else
	{
		taken = sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_S1, 0, SLJIT_IMM, 0);
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)next);
		sljit_set_label(taken, sljit_emit_label(C));
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)target);
	}
}


/*************************************
 *  Block translation
 *************************************/

static void jit_compile(struct jit_block *block, UINT32 pc)
{
	struct mips3_fastram *region;
	struct sljit_compiler *C;
	const UINT8 *host;
	UINT32 addr, count = 0, op, maxcount;
	UINT32 i;
	int kind = JIT_NONE;

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

	/* find how much we can translate */
	for (addr = pc; count < maxcount; addr += 4, count++)
	{
		op = *(const UINT32 *)(host + (addr - pc));
		kind = jit_classify(op);
		if (kind == JIT_SIMPLE)
			continue;
		if (kind == JIT_BRANCH)
		{
			/* the delay slot must be a plain instruction in the same region */
			if (addr + 7 <= region->end &&
				jit_classify(*(const UINT32 *)(host + (addr + 4 - pc))) == JIT_SIMPLE)
				break;
		}
		kind = JIT_NONE;
		break;
	}
	if (count == 0 && kind != JIT_BRANCH)
		return;

	C = sljit_create_compiler(NULL);
	if (!C)
		return;
	sljit_emit_enter(C, 0, SLJIT_ARGS0(32), 2, 2, 0);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&mips3);

	for (i = 0; i < count; i++)
		jit_emit_simple(C, *(const UINT32 *)(host + i * 4));

	if (kind == JIT_BRANCH)
	{
		jit_emit_branch(C, *(const UINT32 *)(host + count * 4), *(const UINT32 *)(host + count * 4 + 4), pc + count * 4);
		block->len = count + 2;
		block->lastpc = pc + count * 4 + 4;
	}
	else
	{
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)(pc + count * 4));
		block->len = count;
		block->lastpc = pc + count * 4 - 4;
	}

	block->code = sljit_generate_code(C, 0, NULL);
	sljit_free_compiler(C);
	if (!block->code)
	{
		block->len = 0;
		return;
	}

	/* remember where translated code lives */
	for (i = 0; i < block->len; i++)
	{
		UINT32 index = jit_codemap_index(host + i * 4);
		jit_codemap[index >> 3] |= 1 << (index & 7);
	}
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
