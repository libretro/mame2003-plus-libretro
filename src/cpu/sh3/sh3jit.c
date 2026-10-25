/***************************************************************************

    sh3jit.c

    Optional recompiler for the SH-3 (SH7709S), built on the bundled sljit
    (src/lib/sljit/sljit_mame.h; included by sh3.c).

    It gives the same results as the interpreter, cycle for cycle: the
    machine state after a timeslice is the same with it or without it.

    - Straight runs of instructions become native code, up to an
      unconditional branch (with its delay slot) or an instruction the
      interpreter has to run on its own (RTE, TRAPA, SLEEP, a branch whose
      delay slot is not allowed there). Conditional branches leave the
      block, or jump inside it when their target is there.
    - Before every instruction but the first, the block leaves when the
      timeslice is used up, as the interpreter's loop does. The cycles are
      taken in the interpreter's order around everything that reads them
      (the bus timing, the memory handlers).
    - ALU, shifts, compares, moves of the control registers and branches
      are native code. Loads and stores compute their address natively
      and call the interpreter's accessors (RB/RW/RL/WB/WW/WL: memory
      handlers and the cache and bus timing). The other instructions call
      the interpreter for that instruction (execute_one).
    - After an instruction that called the interpreter, the block leaves
      when an interrupt became pending (and takes it as the interpreter
      does) or when a write touched translated code.

    Blocks run in the fast memory regions (code ROM, work RAM), with no
    interrupt pending and the CPU awake. Writes over translated code in
    RAM (CPU stores, DMA) flush the translation cache before the next
    block, and so does a state load.

***************************************************************************/

/* the sljit build shared by the recompilers decides where this one runs */
#include "../../lib/sljit/sljit_mame.h"

#define SH3JIT_SUPPORTED		SLJIT_MAME_SUPPORTED

#if SH3JIT_SUPPORTED

#include <stddef.h>
#include <stdlib.h>

#define JIT_TABLE_BITS		19
#define JIT_TABLE_SIZE		(1 << JIT_TABLE_BITS)
#define JIT_MAX_BLOCK		64				/* instructions */
#define JIT_MAX_STUBS		(JIT_MAX_BLOCK * 4)
#define JIT_CODE_BUDGET		(32 * 1024 * 1024)

struct jit_block
{
	void *		code;
	UINT32		pc;				/* address of the first instruction */
	UINT16		len;			/* instructions in the block, 0 = not translatable */
	UINT8		valid;
};

typedef sljit_u32 (SLJIT_FUNC *jit_func)(void);

static int jit_enabled;
static struct jit_block *jit_table;
static UINT32 jit_flushes;
static size_t jit_code_bytes;
static int jit_nomem_logged;

/* set by the helpers: leave the block after this instruction */
static sljit_u32 jit_stop;

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
	if (jit_codemap)
		memset(jit_codemap, 0, JIT_CODEMAP_SIZE);
	jit_code_present = 0;
	jit_flush_pending = 0;
	jit_code_bytes = 0;
	jit_flushes++;
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
	jit_table = (struct jit_block *)calloc(JIT_TABLE_SIZE, sizeof(jit_table[0]));
	jit_codemap = (UINT8 *)calloc(JIT_CODEMAP_SIZE, 1);
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
	jit_flush_pending = 0;
	jit_flushes = 0;
}


/***************************************************************************
	decoding: what the block does with each instruction
***************************************************************************/

enum
{
	JK_NONE = 0,		/* the interpreter runs it: the block stops before it */
	JK_NATIVE,			/* native code, no call */
	JK_LOAD,			/* native address, the interpreter's accessor */
	JK_STORE,
	JK_HELPER,			/* the interpreter runs it (execute_one) */
	JK_BCOND,			/* BT, BF */
	JK_BDELAY_COND,		/* BT/S, BF/S */
	JK_BDELAY			/* BRA, BSR, BRAF, BSRF, JMP, JSR, RTS */
};

/* the class of an instruction, as op0000/op0100/execute_one dispatch it */
static int jit_kind(UINT16 op)
{
	switch (op >> 12)
	{
		case 0:
			switch (op & 0xff)
			{
				case 0x52: case 0x62: case 0x43: case 0x63: case 0xe3:
				case 0x68: case 0xe8: case 0x4a: case 0xca:
				case 0x93: case 0xa3: case 0xb3:
				case 0x82: case 0x92: case 0xa2: case 0xb2:
				case 0xc2: case 0xd2: case 0xe2: case 0xf2:
				case 0x32: case 0x42: case 0x83: case 0xc3:
				case 0x38: case 0xb8: case 0x48: case 0xc8: case 0x58: case 0xd8:
				case 0x3a: case 0xba: case 0x5a: case 0xda: case 0x6a: case 0xea: case 0x7a: case 0xfa:
					return JK_HELPER;
			}
			switch (op & 0x3f)
			{
				case 0x03: case 0x23: case 0x0b:
					return JK_BDELAY;
				case 0x04: case 0x14: case 0x24: case 0x34:
				case 0x05: case 0x15: case 0x25: case 0x35:
				case 0x06: case 0x16: case 0x26: case 0x36:
					return JK_STORE;
				case 0x0c: case 0x1c: case 0x2c: case 0x3c:
				case 0x0d: case 0x1d: case 0x2d: case 0x3d:
				case 0x0e: case 0x1e: case 0x2e: case 0x3e:
					return JK_LOAD;
				case 0x0f: case 0x1f: case 0x2f: case 0x3f:
					return JK_HELPER;					/* MAC.L */
				case 0x1b: case 0x2b:
					return JK_NONE;						/* SLEEP, RTE */
				default:
					return JK_NATIVE;
			}

		case 1:
			return JK_STORE;

		case 2:
			switch (op & 15)
			{
				case 0: case 1: case 2: case 4: case 5: case 6:
					return JK_STORE;
				case 7: case 12:
					return JK_HELPER;					/* DIV0S, CMP/STR */
				default:
					return JK_NATIVE;
			}

		case 3:
			switch (op & 15)
			{
				case 4: case 5: case 10: case 11: case 13: case 14: case 15:
					return JK_HELPER;					/* DIV1, DMULU, SUBC, SUBV, DMULS, ADDC, ADDV */
				default:
					return JK_NATIVE;
			}

		case 4:
			switch (op & 0xff)
			{
				case 0x0c: case 0x1c: case 0x2c: case 0x3c: case 0x4c: case 0x5c: case 0x6c: case 0x7c:
				case 0x8c: case 0x9c: case 0xac: case 0xbc: case 0xcc: case 0xdc: case 0xec: case 0xfc:
				case 0x0d: case 0x1d: case 0x2d: case 0x3d: case 0x4d: case 0x5d: case 0x6d: case 0x7d:
				case 0x8d: case 0x9d: case 0xad: case 0xbd: case 0xcd: case 0xdd: case 0xed: case 0xfd:
					return JK_NATIVE;					/* SHAD, SHLD */
				case 0x42: case 0x46: case 0x4a: case 0x53: case 0x57: case 0x5e:
				case 0x63: case 0x67: case 0x6e: case 0x82: case 0x86: case 0x8a:
				case 0x92: case 0x96: case 0x9a: case 0xa2: case 0xa6: case 0xaa:
				case 0xc2: case 0xc6: case 0xca: case 0xd2: case 0xd6: case 0xda:
				case 0xe2: case 0xe6: case 0xea:
				case 0x8e: case 0x9e: case 0xae: case 0xbe: case 0xce: case 0xde: case 0xee: case 0xfe:
				case 0x83: case 0x93: case 0xa3: case 0xb3: case 0xc3: case 0xd3: case 0xe3: case 0xf3:
				case 0x87: case 0x97: case 0xa7: case 0xb7: case 0xc7: case 0xd7: case 0xe7: case 0xf7:
				case 0x32: case 0x33: case 0x37: case 0x3e: case 0x43: case 0x47: case 0x4e:
				case 0x52: case 0x56: case 0x5a: case 0x62: case 0x66: case 0x6a:
				case 0xf2: case 0xf6: case 0xfa:
					return JK_HELPER;
			}
			switch (op & 0x3f)
			{
				case 0x0b: case 0x2b:
					return JK_BDELAY;					/* JSR, JMP */
				case 0x00: case 0x01: case 0x04: case 0x05: case 0x08: case 0x09: case 0x0a:
				case 0x10: case 0x11: case 0x15: case 0x18: case 0x19: case 0x1a: case 0x1e:
				case 0x20: case 0x21: case 0x24: case 0x25: case 0x28: case 0x29: case 0x2a: case 0x2e:
					return JK_NATIVE;
				case 0x02: case 0x03: case 0x06: case 0x07: case 0x0e:
				case 0x0f: case 0x1f: case 0x2f: case 0x3f:
				case 0x12: case 0x13: case 0x16: case 0x17: case 0x1b:
				case 0x22: case 0x23: case 0x26: case 0x27:
					return JK_HELPER;
				default:
					return JK_NATIVE;					/* nothing */
			}

		case 5:
			return JK_LOAD;

		case 6:
			switch (op & 15)
			{
				case 0: case 1: case 2: case 4: case 5: case 6:
					return JK_LOAD;
				case 10:
					return JK_HELPER;					/* NEGC */
				default:
					return JK_NATIVE;
			}

		case 7:
			return JK_NATIVE;

		case 8:
			switch ((op >> 8) & 15)
			{
				case 0: case 1:
					return JK_STORE;
				case 4: case 5:
					return JK_LOAD;
				case 9: case 11:
					return JK_BCOND;
				case 13: case 15:
					return JK_BDELAY_COND;
				default:
					return JK_NATIVE;
			}

		case 9:
			return JK_NATIVE;

		case 10: case 11:
			return JK_BDELAY;

		case 12:
			switch ((op >> 8) & 15)
			{
				case 0: case 1: case 2:
					return JK_STORE;
				case 3:
					return JK_NONE;						/* TRAPA */
				case 4: case 5: case 6:
					return JK_LOAD;
				case 12: case 13: case 14: case 15:
					return JK_HELPER;					/* TST.B, AND.B, XOR.B, OR.B @(R0,GBR) */
				default:
					return JK_NATIVE;
			}

		case 13: case 14: case 15:
			return JK_NATIVE;
	}
	return JK_NONE;
}

/* a delay slot the block can run: no branch, nothing the interpreter must run alone */
static int jit_slot_ok(UINT16 slot)
{
	int k = jit_kind(slot);
	return k == JK_NATIVE || k == JK_LOAD || k == JK_STORE || k == JK_HELPER;
}

/* the cycles an instruction takes besides the loop's one, when they don't
   depend on the run (execute_one and the instruction functions); the
   instructions that call the interpreter take theirs there */
static int jit_extra_cycles(UINT16 op)
{
	if ((op >> 12) == 0 && ((op & 0xff) & 0x0f) == 0x07)
	{
		/* MUL.L (the low byte cases of op0000 come first: none ends in 7) */
		return 1;
	}
	return 0;
}


/***************************************************************************
	code generation
***************************************************************************/

#define JMEM(off)		SLJIT_MEM1(SLJIT_S0), (sljit_sw)(off)
#define JR(n)			JMEM(offsetof(SH3, r) + (n) * sizeof(UINT32))
#define JSRREG			JMEM(offsetof(SH3, sr))
#define JPC				JMEM(offsetof(SH3, pc))
#define JPPC			JMEM(offsetof(SH3, ppc))
#define JPR				JMEM(offsetof(SH3, pr))
#define JGBR			JMEM(offsetof(SH3, gbr))
#define JVBR			JMEM(offsetof(SH3, vbr))
#define JMACH			JMEM(offsetof(SH3, mach))
#define JMACL			JMEM(offsetof(SH3, macl))
#define JICOUNT			SLJIT_MEM0(), (sljit_sw)&sh3_ICount
#define JSTOP			SLJIT_MEM0(), (sljit_sw)&jit_stop

enum
{
	STUB_LEAVE,			/* set PC and PPC, return */
	STUB_STOP			/* after an instruction: the interpreter's interrupt check, its loop cycle, return */
};

struct jit_stub
{
	struct sljit_jump *jump;
	int		type;
	int		cycles;		/* to take before leaving */
	int		pc_in_s1;	/* the new PC is in S1 (computed branch) */
	UINT32	pc, ppc;
};

struct jit_ctx
{
	struct sljit_compiler *C;
	UINT32	pc;							/* first instruction */
	UINT32	count;
	int		pending;					/* cycles not yet taken from sh3_ICount */
	struct sljit_label *label[JIT_MAX_BLOCK];	/* instruction starts, after the time check */
	UINT8	target[JIT_MAX_BLOCK];		/* a branch in the block goes there */
	struct { struct sljit_jump *jump; UINT32 index; } fwd[JIT_MAX_BLOCK];
	int		nfwd;
	struct jit_stub stub[JIT_MAX_STUBS];
	int		nstubs;
	int		overflow;
};

static UINT16 jit_ops[JIT_MAX_BLOCK + 2];

static struct jit_stub *jit_new_stub(struct jit_ctx *ctx, struct sljit_jump *jump, int type, UINT32 pc, UINT32 ppc)
{
	struct jit_stub *s;
	if (ctx->nstubs >= JIT_MAX_STUBS)
	{
		ctx->overflow = 1;
		return NULL;
	}
	s = &ctx->stub[ctx->nstubs++];
	s->jump = jump;
	s->type = type;
	s->cycles = ctx->pending;
	s->pc_in_s1 = 0;
	s->pc = pc;
	s->ppc = ppc;
	return s;
}

/* take the pending cycles from the timeslice */
static void jit_sync(struct jit_ctx *ctx)
{
	if (ctx->pending)
	{
		sljit_emit_op2(ctx->C, SLJIT_SUB32, JICOUNT, JICOUNT, SLJIT_IMM, ctx->pending);
		ctx->pending = 0;
	}
}

/* the interpreter's loop test: leave when the timeslice is used up */
static void jit_time_check(struct jit_ctx *ctx, UINT32 pc, UINT32 ppc, int pc_in_s1)
{
	struct jit_stub *s;
	jit_sync(ctx);
	s = jit_new_stub(ctx, sljit_emit_cmp(ctx->C, SLJIT_SIG_LESS_EQUAL | SLJIT_32, JICOUNT, SLJIT_IMM, 0), STUB_LEAVE, pc, ppc);
	if (s)
		s->pc_in_s1 = pc_in_s1;
}

/* T from a condition of the flags just set */
static void jit_set_t(struct sljit_compiler *C, sljit_s32 cond)
{
	sljit_emit_op_flags(C, SLJIT_MOV32, SLJIT_R1, 0, cond);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R2, 0, JSRREG, SLJIT_IMM, ~SR_T);
	sljit_emit_op2(C, SLJIT_OR32, JSRREG, SLJIT_R2, 0, SLJIT_R1, 0);
}

/* T = a compare of two operands */
static void jit_cmp_t(struct sljit_compiler *C, sljit_s32 cond, sljit_s32 a, sljit_sw aw, sljit_s32 b, sljit_sw bw)
{
	sljit_s32 setf = (cond == SLJIT_EQUAL) ? SLJIT_SET_Z : SLJIT_SET(cond & ~1);
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, a, aw);
	sljit_emit_op2u(C, SLJIT_SUB32 | setf, SLJIT_R0, 0, b, bw);
	jit_set_t(C, cond);
}

/* T = low bit of R1 (0 or 1 already) */
static void jit_t_from_r1(struct sljit_compiler *C)
{
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R2, 0, JSRREG, SLJIT_IMM, ~SR_T);
	sljit_emit_op2(C, SLJIT_OR32, JSRREG, SLJIT_R2, 0, SLJIT_R1, 0);
}

/* the accessors of the interpreter, with the bus timing */
static sljit_u32 SLJIT_FUNC jit_rb(sljit_u32 a)
{
	sljit_u32 v = (sljit_u32)(INT32)(INT8)RB(a);
	jit_stop = sh3.test_irq | jit_flush_pending;
	return v;
}

static sljit_u32 SLJIT_FUNC jit_rw(sljit_u32 a)
{
	sljit_u32 v = (sljit_u32)(INT32)(INT16)RW(a);
	jit_stop = sh3.test_irq | jit_flush_pending;
	return v;
}

static sljit_u32 SLJIT_FUNC jit_rl(sljit_u32 a)
{
	sljit_u32 v = RL(a);
	jit_stop = sh3.test_irq | jit_flush_pending;
	return v;
}

static void SLJIT_FUNC jit_wb(sljit_u32 a, sljit_u32 d)
{
	WB(a, (UINT8)d);
	jit_stop = sh3.test_irq | jit_flush_pending;
}

static void SLJIT_FUNC jit_ww(sljit_u32 a, sljit_u32 d)
{
	WW(a, (UINT16)d);
	jit_stop = sh3.test_irq | jit_flush_pending;
}

static void SLJIT_FUNC jit_wl(sljit_u32 a, sljit_u32 d)
{
	WL(a, d);
	jit_stop = sh3.test_irq | jit_flush_pending;
}

/* PC-relative loads: no bus timing */
static sljit_u32 SLJIT_FUNC jit_rw_notime(sljit_u32 a)
{
	sljit_u32 v = (sljit_u32)(INT32)(INT16)read_word(a);
	jit_stop = sh3.test_irq | jit_flush_pending;
	return v;
}

static sljit_u32 SLJIT_FUNC jit_rl_notime(sljit_u32 a)
{
	sljit_u32 v = read_long(a);
	jit_stop = sh3.test_irq | jit_flush_pending;
	return v;
}

/* the interpreter runs one instruction */
static void SLJIT_FUNC jit_step(sljit_u32 op)
{
	execute_one((UINT16)op);
	jit_stop = sh3.test_irq | jit_flush_pending;
}

/* the interpreter's interrupt check after an instruction */
static sljit_u32 SLJIT_FUNC jit_after(void)
{
	if (sh3.test_irq)
		check_pending_irq();
	return sh3.pc;
}

/* a call to the interpreter: its PC is the one after the instruction */
static void jit_pre_call(struct jit_ctx *ctx, UINT32 addr)
{
	jit_sync(ctx);
	sljit_emit_op1(ctx->C, SLJIT_MOV32, JPC, SLJIT_IMM, (sljit_s32)(addr + 2));
}

static void jit_call_load(struct jit_ctx *ctx, int size)
{
	sljit_sw fn;
	if (size == 1)
		fn = SLJIT_FUNC_ADDR(jit_rb);
	else if (size == 2)
		fn = SLJIT_FUNC_ADDR(jit_rw);
	else
		fn = SLJIT_FUNC_ADDR(jit_rl);
	sljit_emit_icall(ctx->C, SLJIT_CALL, SLJIT_ARGS1(32, 32), SLJIT_IMM, fn);
}

static void jit_call_store(struct jit_ctx *ctx, int size)
{
	sljit_sw fn;
	if (size == 1)
		fn = SLJIT_FUNC_ADDR(jit_wb);
	else if (size == 2)
		fn = SLJIT_FUNC_ADDR(jit_ww);
	else
		fn = SLJIT_FUNC_ADDR(jit_wl);
	sljit_emit_icall(ctx->C, SLJIT_CALL, SLJIT_ARGS2V(32, 32), SLJIT_IMM, fn);
}

/* a PC-relative load: straight from fast memory when the literal is there */
static int jit_literal(struct jit_ctx *ctx, UINT32 addr, UINT32 ea, int size, int n)
{
	struct sljit_compiler *C = ctx->C;
	UINT32 a = ea & (size == 2 ? ~1 : ~3);
	sh3_fastram *f = (a < 0xe0000000) ? find_fastram(a & AM) : NULL;

	if (f)
	{
		UINT32 off = (a & AM) - f->start;
		if (size == 2)
			sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)&f->base[WORD_XOR_BE(off)]);
		else
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)&f->base[off]);
		sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0);
		return 0;
	}
	jit_pre_call(ctx, addr);
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)ea);
	if (size == 2)
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1(32, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_rw_notime));
	else
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1(32, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_rl_notime));
	sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0);
	return 1;
}

/* SHAD, SHLD */
static void jit_dyn_shift(struct sljit_compiler *C, int m, int n, int arith)
{
	struct sljit_jump *neg, *zero, *done1, *done2;

	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JR(m));
	neg = sljit_emit_cmp(C, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 0);
	/* left by Rm & 31 */
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 0x1f);
	sljit_emit_op2(C, SLJIT_SHL32, JR(n), JR(n), SLJIT_R1, 0);
	done1 = sljit_emit_jump(C, SLJIT_JUMP);
	sljit_set_label(neg, sljit_emit_label(C));
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, 0x1f);
	zero = sljit_emit_cmp(C, SLJIT_EQUAL | SLJIT_32, SLJIT_R2, 0, SLJIT_IMM, 0);
	/* right by (~Rm & 31) + 1 */
	sljit_emit_op2(C, SLJIT_XOR32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, -1);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 0x1f);
	sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_op2(C, arith ? SLJIT_ASHR32 : SLJIT_LSHR32, JR(n), JR(n), SLJIT_R1, 0);
	done2 = sljit_emit_jump(C, SLJIT_JUMP);
	sljit_set_label(zero, sljit_emit_label(C));
	if (arith)
		sljit_emit_op2(C, SLJIT_ASHR32, JR(n), JR(n), SLJIT_IMM, 31);
	else
		sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_IMM, 0);
	sljit_set_label(done1, sljit_emit_label(C));
	sljit_set_label(done2, sljit_emit_label(C));
}

/* a native instruction (JK_NATIVE); returns 1 if it called the interpreter */
static int jit_emit_native(struct jit_ctx *ctx, UINT16 op, UINT32 addr)
{
	struct sljit_compiler *C = ctx->C;
	int n = (op >> 8) & 15, m = (op >> 4) & 15;
	UINT32 i = op & 0xff;

	switch (op >> 12)
	{
		case 0:
			switch (op & 0x3f)
			{
				case 0x02: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JSRREG); return 0;		/* STC SR,Rn */
				case 0x07: case 0x17: case 0x27: case 0x37:								/* MUL.L */
					sljit_emit_op2(C, SLJIT_MUL32, SLJIT_R0, 0, JR(n), JR(m));
					sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_R0, 0);
					return 0;
				case 0x08: sljit_emit_op2(C, SLJIT_AND32, JSRREG, JSRREG, SLJIT_IMM, ~SR_T); return 0;
				case 0x0a: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JMACH); return 0;
				case 0x12: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JGBR); return 0;
				case 0x18: sljit_emit_op2(C, SLJIT_OR32, JSRREG, JSRREG, SLJIT_IMM, SR_T); return 0;
				case 0x19: sljit_emit_op2(C, SLJIT_AND32, JSRREG, JSRREG, SLJIT_IMM, ~(SR_M | SR_Q | SR_T)); return 0;
				case 0x1a: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JMACL); return 0;
				case 0x22: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JVBR); return 0;
				case 0x28:
					sljit_emit_op1(C, SLJIT_MOV32, JMACH, SLJIT_IMM, 0);
					sljit_emit_op1(C, SLJIT_MOV32, JMACL, SLJIT_IMM, 0);
					return 0;
				case 0x29: sljit_emit_op2(C, SLJIT_AND32, JR(n), JSRREG, SLJIT_IMM, SR_T); return 0;
				case 0x2a: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JPR); return 0;
				default: return 0;																/* NOP and nothing */
			}

		case 2:
			switch (op & 15)
			{
				case 8:																		/* TST */
					sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JR(n));
					sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, JR(m));
					jit_set_t(C, SLJIT_ZERO);
					return 0;
				case 9: sljit_emit_op2(C, SLJIT_AND32, JR(n), JR(n), JR(m)); return 0;
				case 10: sljit_emit_op2(C, SLJIT_XOR32, JR(n), JR(n), JR(m)); return 0;
				case 11: sljit_emit_op2(C, SLJIT_OR32, JR(n), JR(n), JR(m)); return 0;
				case 13:																	/* XTRCT */
					sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R0, 0, JR(n), SLJIT_IMM, 16);
					sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R1, 0, JR(m), SLJIT_IMM, 16);
					sljit_emit_op2(C, SLJIT_OR32, JR(n), SLJIT_R0, 0, SLJIT_R1, 0);
					return 0;
				case 14:																	/* MULU.W */
					sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R0, 0, JR(n));
					sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R1, 0, JR(m));
					sljit_emit_op2(C, SLJIT_MUL32, JMACL, SLJIT_R0, 0, SLJIT_R1, 0);
					return 0;
				case 15:																	/* MULS.W */
					sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R0, 0, JR(n));
					sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R1, 0, JR(m));
					sljit_emit_op2(C, SLJIT_MUL32, JMACL, SLJIT_R0, 0, SLJIT_R1, 0);
					return 0;
				default: return 0;															/* 3: nothing */
			}

		case 3:
			switch (op & 15)
			{
				case 0: jit_cmp_t(C, SLJIT_EQUAL, JR(n), JR(m)); return 0;
				case 2: jit_cmp_t(C, SLJIT_GREATER_EQUAL, JR(n), JR(m)); return 0;
				case 3: jit_cmp_t(C, SLJIT_SIG_GREATER_EQUAL, JR(n), JR(m)); return 0;
				case 6: jit_cmp_t(C, SLJIT_GREATER, JR(n), JR(m)); return 0;
				case 7: jit_cmp_t(C, SLJIT_SIG_GREATER, JR(n), JR(m)); return 0;
				case 8: sljit_emit_op2(C, SLJIT_SUB32, JR(n), JR(n), JR(m)); return 0;
				case 12: sljit_emit_op2(C, SLJIT_ADD32, JR(n), JR(n), JR(m)); return 0;
				default: return 0;															/* 1, 9: nothing */
			}

		case 4:
			if ((op & 0x0f) == 0x0c) { jit_dyn_shift(C, m, n, 1); return 0; }
			if ((op & 0x0f) == 0x0d) { jit_dyn_shift(C, m, n, 0); return 0; }
			switch (op & 0x3f)
			{
				case 0x00: case 0x20:														/* SHLL, SHAL */
					sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 31);
					jit_t_from_r1(C);
					sljit_emit_op2(C, SLJIT_SHL32, JR(n), JR(n), SLJIT_IMM, 1);
					return 0;
				case 0x01:																	/* SHLR */
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 1);
					jit_t_from_r1(C);
					sljit_emit_op2(C, SLJIT_LSHR32, JR(n), JR(n), SLJIT_IMM, 1);
					return 0;
				case 0x21:																	/* SHAR */
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 1);
					jit_t_from_r1(C);
					sljit_emit_op2(C, SLJIT_ASHR32, JR(n), JR(n), SLJIT_IMM, 1);
					return 0;
				case 0x04:																	/* ROTL */
					sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 31);
					jit_t_from_r1(C);
					sljit_emit_op2(C, SLJIT_ROTL32, JR(n), JR(n), SLJIT_IMM, 1);
					return 0;
				case 0x05:																	/* ROTR */
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 1);
					jit_t_from_r1(C);
					sljit_emit_op2(C, SLJIT_ROTR32, JR(n), JR(n), SLJIT_IMM, 1);
					return 0;
				case 0x24:																	/* ROTCL */
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, JSRREG, SLJIT_IMM, SR_T);
					sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 31);
					sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R2, 0, JR(n), SLJIT_IMM, 1);
					sljit_emit_op2(C, SLJIT_OR32, JR(n), SLJIT_R2, 0, SLJIT_R0, 0);
					jit_t_from_r1(C);
					return 0;
				case 0x25:																	/* ROTCR */
					sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R0, 0, JSRREG, SLJIT_IMM, 31);
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, JR(n), SLJIT_IMM, 1);
					sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R2, 0, JR(n), SLJIT_IMM, 1);
					sljit_emit_op2(C, SLJIT_OR32, JR(n), SLJIT_R2, 0, SLJIT_R0, 0);
					jit_t_from_r1(C);
					return 0;
				case 0x08: sljit_emit_op2(C, SLJIT_SHL32, JR(n), JR(n), SLJIT_IMM, 2); return 0;
				case 0x09: sljit_emit_op2(C, SLJIT_LSHR32, JR(n), JR(n), SLJIT_IMM, 2); return 0;
				case 0x18: sljit_emit_op2(C, SLJIT_SHL32, JR(n), JR(n), SLJIT_IMM, 8); return 0;
				case 0x19: sljit_emit_op2(C, SLJIT_LSHR32, JR(n), JR(n), SLJIT_IMM, 8); return 0;
				case 0x28: sljit_emit_op2(C, SLJIT_SHL32, JR(n), JR(n), SLJIT_IMM, 16); return 0;
				case 0x29: sljit_emit_op2(C, SLJIT_LSHR32, JR(n), JR(n), SLJIT_IMM, 16); return 0;
				case 0x0a: sljit_emit_op1(C, SLJIT_MOV32, JMACH, JR(n)); return 0;			/* LDS Rn,MACH */
				case 0x1a: sljit_emit_op1(C, SLJIT_MOV32, JMACL, JR(n)); return 0;
				case 0x2a: sljit_emit_op1(C, SLJIT_MOV32, JPR, JR(n)); return 0;
				case 0x1e: sljit_emit_op1(C, SLJIT_MOV32, JGBR, JR(n)); return 0;			/* LDC Rn,GBR */
				case 0x2e: sljit_emit_op1(C, SLJIT_MOV32, JVBR, JR(n)); return 0;
				case 0x10:																	/* DT */
					sljit_emit_op2(C, SLJIT_SUB32 | SLJIT_SET_Z, SLJIT_R0, 0, JR(n), SLJIT_IMM, 1);
					sljit_emit_op_flags(C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_ZERO);
					sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0);
					jit_t_from_r1(C);
					return 0;
				case 0x11: jit_cmp_t(C, SLJIT_SIG_GREATER_EQUAL, JR(n), SLJIT_IMM, 0); return 0;	/* CMP/PZ */
				case 0x15: jit_cmp_t(C, SLJIT_SIG_GREATER, JR(n), SLJIT_IMM, 0); return 0;		/* CMP/PL */
				default: return 0;
			}

		case 6:
			switch (op & 15)
			{
				case 3: sljit_emit_op1(C, SLJIT_MOV32, JR(n), JR(m)); return 0;
				case 7:
					sljit_emit_op2(C, SLJIT_XOR32, JR(n), JR(m), SLJIT_IMM, -1);
					return 0;
				case 8:																		/* SWAP.B */
					sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JR(m));
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)0xffff0000);
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_IMM, 0xff);
					sljit_emit_op2(C, SLJIT_SHL32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 8);
					sljit_emit_op2(C, SLJIT_OR32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
					sljit_emit_op2(C, SLJIT_LSHR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
					sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xff);
					sljit_emit_op2(C, SLJIT_OR32, JR(n), SLJIT_R0, 0, SLJIT_R1, 0);
					return 0;
				case 9: sljit_emit_op2(C, SLJIT_ROTL32, JR(n), JR(m), SLJIT_IMM, 16); return 0;	/* SWAP.W */
				case 11: sljit_emit_op2(C, SLJIT_SUB32, JR(n), SLJIT_IMM, 0, JR(m)); return 0;
				/* a narrow move to memory stores only that width: through R0 */
				case 12: sljit_emit_op1(C, SLJIT_MOV32_U8, SLJIT_R0, 0, JR(m)); sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0); return 0;
				case 13: sljit_emit_op1(C, SLJIT_MOV32_U16, SLJIT_R0, 0, JR(m)); sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0); return 0;
				case 14: sljit_emit_op1(C, SLJIT_MOV32_S8, SLJIT_R0, 0, JR(m)); sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0); return 0;
				case 15: sljit_emit_op1(C, SLJIT_MOV32_S16, SLJIT_R0, 0, JR(m)); sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0); return 0;
				default: return 0;
			}

		case 7: sljit_emit_op2(C, SLJIT_ADD32, JR(n), JR(n), SLJIT_IMM, (sljit_s32)(INT8)i); return 0;

		case 8:
			if (((op >> 8) & 15) == 8)
				jit_cmp_t(C, SLJIT_EQUAL, JR(0), SLJIT_IMM, (sljit_s32)(INT8)i);			/* CMP/EQ #imm,R0 */
			return 0;																			/* others: nothing */

		case 9:																				/* MOV.W @(disp,PC),Rn */
			return jit_literal(ctx, addr, addr + 4 + i * 2, 2, n);

		case 12:
			switch ((op >> 8) & 15)
			{
				case 7:																		/* MOVA */
					sljit_emit_op1(C, SLJIT_MOV32, JR(0), SLJIT_IMM, (sljit_s32)(((addr + 4) & ~3) + i * 4));
					return 0;
				case 8:																		/* TST #imm,R0 */
					sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JR(0));
					sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, i);
					jit_set_t(C, SLJIT_ZERO);
					return 0;
				case 9: sljit_emit_op2(C, SLJIT_AND32, JR(0), JR(0), SLJIT_IMM, i); return 0;
				case 10: sljit_emit_op2(C, SLJIT_XOR32, JR(0), JR(0), SLJIT_IMM, i); return 0;
				case 11: sljit_emit_op2(C, SLJIT_OR32, JR(0), JR(0), SLJIT_IMM, i); return 0;
				default: return 0;
			}

		case 13:																			/* MOV.L @(disp,PC),Rn */
			return jit_literal(ctx, addr, ((addr + 4) & ~3) + i * 4, 4, n);

		case 14: sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_IMM, (sljit_s32)(INT8)i); return 0;

		default: return 0;																	/* 15: no FPU */
	}
}

/* a load (JK_LOAD): R0 = the address, the call, the register */
static void jit_emit_load(struct jit_ctx *ctx, UINT16 op, UINT32 addr)
{
	struct sljit_compiler *C = ctx->C;
	int n = (op >> 8) & 15, m = (op >> 4) & 15;
	UINT32 d = op & 0x0f, i = op & 0xff;
	int size = 4, dst = n, postinc = 0;

	jit_pre_call(ctx, addr);
	switch (op >> 12)
	{
		case 0:																			/* MOV.x @(R0,Rm),Rn */
			size = ((op & 0x0f) == 0x0c) ? 1 : ((op & 0x0f) == 0x0d) ? 2 : 4;
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JR(m), JR(0));
			break;
		case 5:																			/* MOV.L @(disp,Rm),Rn */
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JR(m), SLJIT_IMM, d * 4);
			break;
		case 6:																			/* MOV.x @Rm,Rn / @Rm+,Rn */
			size = (op & 3) == 0 ? 1 : (op & 3) == 1 ? 2 : 4;
			postinc = (op & 4) && n != m;
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JR(m));
			break;
		case 8:																			/* MOV.x @(disp,Rm),R0 */
			size = ((op >> 8) & 15) == 4 ? 1 : 2;
			dst = 0;
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JR(m), SLJIT_IMM, d * size);
			break;
		case 12:																		/* MOV.x @(disp,GBR),R0 */
			size = ((op >> 8) & 15) == 4 ? 1 : ((op >> 8) & 15) == 5 ? 2 : 4;
			dst = 0;
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JGBR, SLJIT_IMM, i * size);
			break;
	}
	jit_call_load(ctx, size);
	sljit_emit_op1(C, SLJIT_MOV32, JR(dst), SLJIT_R0, 0);
	if (postinc)
		sljit_emit_op2(C, SLJIT_ADD32, JR(m), JR(m), SLJIT_IMM, size);
}

/* a store (JK_STORE): R0 = the address, R1 = the data */
static void jit_emit_store(struct jit_ctx *ctx, UINT16 op, UINT32 addr)
{
	struct sljit_compiler *C = ctx->C;
	int n = (op >> 8) & 15, m = (op >> 4) & 15;
	UINT32 d = op & 0x0f, i = op & 0xff;
	int size = 4;

	jit_pre_call(ctx, addr);
	switch (op >> 12)
	{
		case 0:																			/* MOV.x Rm,@(R0,Rn) */
			size = ((op & 0x0f) == 0x04) ? 1 : ((op & 0x0f) == 0x05) ? 2 : 4;
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JR(m));
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JR(n), JR(0));
			break;
		case 1:																			/* MOV.L Rm,@(disp,Rn) */
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JR(m));
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JR(n), SLJIT_IMM, d * 4);
			break;
		case 2:
			size = (op & 3) == 0 ? 1 : (op & 3) == 1 ? 2 : 4;
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JR(m));
			if (op & 4)
			{
				/* MOV.x Rm,@-Rn: the data is read before Rn changes */
				sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R0, 0, JR(n), SLJIT_IMM, size);
				sljit_emit_op1(C, SLJIT_MOV32, JR(n), SLJIT_R0, 0);
			}
			else
				sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, JR(n));
			break;
		case 8:																			/* MOV.x R0,@(disp,Rn) (Rn in the m field) */
			size = ((op >> 8) & 15) == 0 ? 1 : 2;
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JR(0));
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JR(m), SLJIT_IMM, d * size);
			break;
		case 12:																		/* MOV.x R0,@(disp,GBR) */
			size = ((op >> 8) & 15) == 0 ? 1 : ((op >> 8) & 15) == 1 ? 2 : 4;
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, JR(0));
			sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R0, 0, JGBR, SLJIT_IMM, i * size);
			break;
	}
	jit_call_store(ctx, size);
}

static void jit_emit_helper(struct jit_ctx *ctx, UINT16 op, UINT32 addr)
{
	jit_pre_call(ctx, addr);
	sljit_emit_op1(ctx->C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, op);
	sljit_emit_icall(ctx->C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_step));
}

/* one instruction that is not a branch; returns 1 if it called the interpreter */
static int jit_emit_body(struct jit_ctx *ctx, UINT16 op, UINT32 addr)
{
	switch (jit_kind(op))
	{
		case JK_NATIVE:
		{
			/* the PC-relative loads outside fast memory call the interpreter */
			int called = jit_emit_native(ctx, op, addr);
			ctx->pending += jit_extra_cycles(op);
			return called;
		}
		case JK_LOAD:
			jit_emit_load(ctx, op, addr);
			return 1;
		case JK_STORE:
			jit_emit_store(ctx, op, addr);
			return 1;
		case JK_HELPER:
			jit_emit_helper(ctx, op, addr);
			return 1;
	}
	return 0;
}

/* after an instruction that called the interpreter: leave on a pending
   interrupt or a write over translated code (cycles up to before the
   loop's own one) */
static void jit_stop_check(struct jit_ctx *ctx, UINT32 next, UINT32 ppc)
{
	jit_new_stub(ctx, sljit_emit_cmp(ctx->C, SLJIT_NOT_EQUAL | SLJIT_32, JSTOP, SLJIT_IMM, 0), STUB_STOP, next, ppc);
}

/* go to the instruction at 'target' (inside the block when it is an
   instruction start there), the timeslice checked first */
static void jit_goto(struct jit_ctx *ctx, UINT32 target, UINT32 ppc)
{
	UINT32 t = (target - ctx->pc) / 2;
	jit_time_check(ctx, target, ppc, 0);
	if (target >= ctx->pc && !(target & 1) && t < ctx->count && ctx->target[t])
	{
		if (ctx->nfwd < JIT_MAX_BLOCK)
		{
			ctx->fwd[ctx->nfwd].jump = sljit_emit_jump(ctx->C, SLJIT_JUMP);
			ctx->fwd[ctx->nfwd].index = t;
			ctx->nfwd++;
		}
		else
			ctx->overflow = 1;
		return;
	}
	/* leave for it */
	{
		struct jit_stub *s = jit_new_stub(ctx, sljit_emit_jump(ctx->C, SLJIT_JUMP), STUB_LEAVE, target, ppc);
		(void)s;
	}
}

/* the delay slot and the jump of a taken delayed branch: the target is a
   constant, or in S1 */
static void jit_emit_delayed(struct jit_ctx *ctx, UINT32 addr, UINT16 slot, UINT32 target, int in_s1)
{
	struct sljit_compiler *C = ctx->C;
	UINT32 saddr = addr + 2;
	int called;

	ctx->pending += 1;								/* the branch's own cycle */
	called = jit_emit_body(ctx, slot, saddr);
	ctx->pending += 1;								/* the delay slot */
	if (called)
	{
		struct jit_stub *s;
		jit_sync(ctx);
		s = jit_new_stub(ctx, sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, JSTOP, SLJIT_IMM, 0), STUB_STOP, target, saddr);
		if (s)
			s->pc_in_s1 = in_s1;
	}
	ctx->pending += 1;								/* the loop's cycle */

	if (in_s1)
	{
		struct jit_stub *s;
		jit_time_check(ctx, 0, saddr, 1);
		s = jit_new_stub(ctx, sljit_emit_jump(C, SLJIT_JUMP), STUB_LEAVE, 0, saddr);
		if (s)
			s->pc_in_s1 = 1;
	}
	else
		jit_goto(ctx, target, saddr);
}

/* a branch at index k (and its slot); returns the instructions used */
static void jit_emit_branch(struct jit_ctx *ctx, UINT16 op, UINT16 slot, UINT32 addr)
{
	struct sljit_compiler *C = ctx->C;
	int n = (op >> 8) & 15;
	UINT32 target;
	int kind = jit_kind(op);

	if (kind == JK_BCOND)
	{
		/* BT, BF: taken costs 2 more, no slot */
		struct sljit_jump *skip;
		int bt = ((op >> 8) & 15) == 9;
		target = addr + 4 + (INT32)(INT8)(op & 0xff) * 2;
		sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, JSRREG, SLJIT_IMM, SR_T);
		skip = sljit_emit_jump(C, bt ? SLJIT_ZERO : SLJIT_NOT_ZERO);
		{
			int save = ctx->pending;
			ctx->pending += 3;
			jit_goto(ctx, target, addr);
			ctx->pending = save;
		}
		sljit_set_label(skip, sljit_emit_label(C));
		ctx->pending += 1;
		return;
	}

	if (kind == JK_BDELAY_COND)
	{
		/* BT/S, BF/S: taken runs the slot then jumps; not taken goes on
		   with the next instruction as a plain one */
		struct sljit_jump *skip;
		int bt = ((op >> 8) & 15) == 13;
		target = addr + 4 + (INT32)(INT8)(op & 0xff) * 2;
		sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, JSRREG, SLJIT_IMM, SR_T);
		skip = sljit_emit_jump(C, bt ? SLJIT_ZERO : SLJIT_NOT_ZERO);
		{
			int save = ctx->pending;
			jit_emit_delayed(ctx, addr, slot, target, 0);
			ctx->pending = save;
		}
		sljit_set_label(skip, sljit_emit_label(C));
		ctx->pending += 1;
		return;
	}

	/* unconditional delayed branches */
	switch (op >> 12)
	{
		case 10:																		/* BRA */
			target = addr + 4 + (((INT32)((op & 0xfff) << 20)) >> 20) * 2;
			jit_emit_delayed(ctx, addr, slot, target, 0);
			return;
		case 11:																		/* BSR */
			target = addr + 4 + (((INT32)((op & 0xfff) << 20)) >> 20) * 2;
			sljit_emit_op1(C, SLJIT_MOV32, JPR, SLJIT_IMM, (sljit_s32)(addr + 4));
			jit_emit_delayed(ctx, addr, slot, target, 0);
			return;
		case 0:
			switch (op & 0x3f)
			{
				case 0x03:																/* BSRF Rn */
					sljit_emit_op1(C, SLJIT_MOV32, JPR, SLJIT_IMM, (sljit_s32)(addr + 4));
					sljit_emit_op2(C, SLJIT_ADD32, SLJIT_S1, 0, JR(n), SLJIT_IMM, (sljit_s32)(addr + 4));
					break;
				case 0x23:																/* BRAF Rn */
					sljit_emit_op2(C, SLJIT_ADD32, SLJIT_S1, 0, JR(n), SLJIT_IMM, (sljit_s32)(addr + 4));
					break;
				default:																/* RTS */
					sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S1, 0, JPR);
					break;
			}
			jit_emit_delayed(ctx, addr, slot, 0, 1);
			return;
		case 4:
			if ((op & 0x3f) == 0x0b)													/* JSR @Rn */
			{
				sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S1, 0, JR(n));
				sljit_emit_op1(C, SLJIT_MOV32, JPR, SLJIT_IMM, (sljit_s32)(addr + 4));
			}
			else																		/* JMP @Rn */
				sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S1, 0, JR(n));
			jit_emit_delayed(ctx, addr, slot, 0, 1);
			return;
	}
}

static INLINE UINT32 jit_table_index(UINT32 pc)
{
	return (pc >> 1) & (JIT_TABLE_SIZE - 1);
}

static void jit_compile(struct jit_block *block, UINT32 pc)
{
	static struct jit_ctx ctx;
	static UINT8 isbranch[JIT_MAX_BLOCK + 2];
	sh3_fastram *region;
	struct sljit_compiler *C;
	UINT32 count = 0, maxcount, i, a;
	int ends = 0;

	block->pc = pc;
	block->len = 0;
	block->valid = 1;
	block->code = NULL;

	a = pc & AM;
	if ((pc & 1) || pc >= 0xe0000000)
		return;
	region = find_fastram(a);
	if (!region)
		return;
	maxcount = (region->end - a + 1) / 2;
	if (maxcount > JIT_MAX_BLOCK)
		maxcount = JIT_MAX_BLOCK;
	for (i = 0; i < maxcount; i++)
		jit_ops[i] = *(UINT16 *)&region->base[WORD_XOR_BE(a + i * 2 - region->start)];

	/* the code here is watched, translatable or not */
	if (!region->readonly)
	{
		jit_codemap[(a >> 2) & (JIT_CODEMAP_SIZE - 1)] = 1;
		jit_code_present = 1;
	}

	memset(isbranch, 0, sizeof(isbranch));
	while (count < maxcount && !ends)
	{
		int k = jit_kind(jit_ops[count]);
		if (k == JK_NONE)
			break;
		if (k == JK_BCOND)
		{
			isbranch[count++] = 1;
			continue;
		}
		if (k == JK_BDELAY || k == JK_BDELAY_COND)
		{
			if (count + 2 > maxcount || !jit_slot_ok(jit_ops[count + 1]))
				break;
			isbranch[count] = 1;
			if (k == JK_BDELAY)
			{
				ends = 1;
				count += 2;
			}
			else
				count++;		/* the slot is also the next plain instruction */
			continue;
		}
		count++;
	}
	if (count == 0)
		return;

	/* branch targets inside the block: instruction starts that are not the
	   delay slot of an unconditional branch */
	memset(ctx.target, 0, sizeof(ctx.target));
	memset(ctx.label, 0, sizeof(ctx.label));
	ctx.pc = pc;
	ctx.count = count;
	for (i = 0; i < count; i++)
	{
		int k = jit_kind(jit_ops[i]);
		if (isbranch[i] && (k == JK_BCOND || k == JK_BDELAY_COND || (jit_ops[i] >> 12) == 10 || (jit_ops[i] >> 12) == 11))
		{
			UINT32 t, target;
			if ((jit_ops[i] >> 12) >= 10)
				target = pc + i * 2 + 4 + (((INT32)((jit_ops[i] & 0xfff) << 20)) >> 20) * 2;
			else
				target = pc + i * 2 + 4 + (INT32)(INT8)(jit_ops[i] & 0xff) * 2;
			t = (target - pc) / 2;
			if (target >= pc && t < count && !(ends && t == count - 1))
				ctx.target[t] = 1;
		}
	}

	C = sljit_create_compiler(NULL);
	if (!C)
		return;
	ctx.C = C;
	ctx.pending = 0;
	ctx.nfwd = 0;
	ctx.nstubs = 0;
	ctx.overflow = 0;

	sljit_emit_enter(C, 0, SLJIT_ARGS0(32), 3, 2, 0);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&sh3);

	for (i = 0; i < count; i++)
	{
		UINT32 addr = pc + i * 2;
		UINT16 op = jit_ops[i];

		/* the loop's test (the dispatcher did it for the first one) */
		if (i > 0)
			jit_time_check(&ctx, addr, addr - 2, 0);
		ctx.label[i] = sljit_emit_label(C);

		if (isbranch[i])
		{
			jit_emit_branch(&ctx, op, jit_ops[i + 1], addr);
			if (jit_kind(op) == JK_BDELAY)
			{
				i++;
				break;
			}
			continue;
		}

		if (jit_emit_body(&ctx, op, addr))
		{
			jit_sync(&ctx);
			jit_stop_check(&ctx, addr + 2, addr);
		}
		ctx.pending += 1;
	}

	/* straight off the end */
	if (!ends)
	{
		UINT32 addr = pc + count * 2;
		jit_sync(&ctx);
		sljit_emit_op1(C, SLJIT_MOV32, JPC, SLJIT_IMM, (sljit_s32)addr);
		sljit_emit_op1(C, SLJIT_MOV32, JPPC, SLJIT_IMM, (sljit_s32)(addr - 2));
		sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_s32)addr);
	}

	/* jumps inside the block */
	for (i = 0; i < (UINT32)ctx.nfwd; i++)
		sljit_set_label(ctx.fwd[i].jump, ctx.label[ctx.fwd[i].index]);

	/* the ways out */
	for (i = 0; i < (UINT32)ctx.nstubs; i++)
	{
		struct jit_stub *s = &ctx.stub[i];
		sljit_set_label(s->jump, sljit_emit_label(C));
		if (s->cycles)
			sljit_emit_op2(C, SLJIT_SUB32, JICOUNT, JICOUNT, SLJIT_IMM, s->cycles);
		if (s->pc_in_s1)
			sljit_emit_op1(C, SLJIT_MOV32, JPC, SLJIT_S1, 0);
		else
			sljit_emit_op1(C, SLJIT_MOV32, JPC, SLJIT_IMM, (sljit_s32)s->pc);
		sljit_emit_op1(C, SLJIT_MOV32, JPPC, SLJIT_IMM, (sljit_s32)s->ppc);
		if (s->type == STUB_STOP)
		{
			sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS0(32), SLJIT_IMM, SLJIT_FUNC_ADDR(jit_after));
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_S1, 0, SLJIT_R0, 0);
			sljit_emit_op2(C, SLJIT_SUB32, JICOUNT, JICOUNT, SLJIT_IMM, 1);
			sljit_emit_return(C, SLJIT_MOV32, SLJIT_S1, 0);
		}
		else if (s->pc_in_s1)
			sljit_emit_return(C, SLJIT_MOV32, SLJIT_S1, 0);
		else
			sljit_emit_return(C, SLJIT_MOV32, SLJIT_IMM, (sljit_s32)s->pc);
	}

	if (ctx.overflow)
	{
		sljit_free_compiler(C);
		return;
	}

	if (jit_code_bytes > JIT_CODE_BUDGET)
	{
		jit_flush();
		block->pc = pc;
		block->valid = 1;
		if (!region->readonly)
		{
			jit_codemap[(a >> 2) & (JIT_CODEMAP_SIZE - 1)] = 1;
			jit_code_present = 1;
		}
	}
	block->code = sljit_generate_code(C, 0, NULL);
	if (block->code)
		jit_code_bytes += sljit_get_generated_code_size(C);
	sljit_free_compiler(C);
	if (!block->code)
	{
		if (!jit_nomem_logged)
		{
			jit_nomem_logged = 1;
			log_cb(RETRO_LOG_WARN, LOGPRE "SH-3 recompiler: no executable memory, using the interpreter\n");
		}
		return;
	}
	block->len = count;

	/* remember where translated code lives (the slot of a final branch too) */
	if (!region->readonly)
	{
		for (i = 0; i < count; i++)
			jit_codemap[((a + i * 2) >> 2) & (JIT_CODEMAP_SIZE - 1)] = 1;
		jit_code_present = 1;
	}
}

/* the block at pc, or NULL */
static INLINE struct jit_block *jit_lookup(UINT32 pc)
{
	struct jit_block *block = &jit_table[jit_table_index(pc)];

	if (!block->valid || block->pc != pc)
	{
		if (block->code)
		{
			sljit_free_code(block->code, NULL);
			block->code = NULL;
		}
		jit_compile(block, pc);
	}
	return block->len ? block : NULL;
}

/* runs a block at the PC if there is one; returns 0 if the interpreter has to step */
static INLINE int jit_run(void)
{
	struct jit_block *block;

	if (jit_flush_pending)
		jit_flush();
	if (sh3.test_irq || sh3.sleep_mode == 1)
		return 0;
	block = jit_lookup(sh3.pc);
	if (!block)
		return 0;
	jit_stop = 0;
	sh3.pc = ((jit_func)block->code)();
	return 1;
}

#else

static const int jit_enabled = 0;
static void jit_init(void) { }
static void jit_exit(void) { }
static INLINE int jit_run(void) { return 0; }

#endif

int sh3_jit_available(void)
{
	return SH3JIT_SUPPORTED;
}

void sh3_set_jit(int enable)
{
#if SH3JIT_SUPPORTED
	if (enable && !jit_table)
		jit_init();
	else if (!enable && jit_table)
		jit_exit();
	jit_enabled = enable && jit_table != NULL;
#else
	(void)enable;
#endif
}
