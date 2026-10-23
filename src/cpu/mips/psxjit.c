/***************************************************************************

    psxjit.c

    Optional recompiler for the PSX CPU (R3000A), built on the bundled
    sljit (src/lib/sljit/sljit_mame.h; included by psx_cpu.c).

    Straight-line runs of supported instructions, ending at a jump (with
    its delay slot) or at the first unsupported instruction, become
    native code. A block only runs when the timeslice has enough cycles
    left for all of it, so the instruction timing is exactly the
    interpreter's; everything else stays with the interpreter.

    - ALU, shifts, immediates, HI/LO moves, MULT/MULTU and branches are
      native code. ADD/ADDI/SUB leave the block on overflow and the
      interpreter raises the exception.
    - Loads and stores go straight to fast RAM. Any other address, a
      misaligned access, or a store over translated code leaves the block
      just before that instruction ("side exit") and the interpreter runs
      it, with its memory handlers, PC and timing.
    - DIV/DIVU, the GTE (MFC2/CFC2/MTC2/CTC2, commands, LWC2/SWC2),
      LWL/LWR/SWL/SWR and MFC0 call the interpreter for that instruction,
      once the block has checked it cannot raise an exception or touch
      anything but fast RAM.
    - As in the interpreter, a load writes its register at once, except a
      load in the delay slot of a taken branch, which stays pending
      (delayr/delayv) until the first instruction at the branch target.
    - A loop that only re-reads memory and recomputes the same registers
      (a polling loop: every pass leaves the same state) skips the whole
      passes that still fit in the timeslice once one pass has run.

    Blocks only run in kernel mode with the cache not isolated and no
    load or branch pending. Only code in fast RAM is translated; writes
    that touch translated code (CPU stores, DMA, driver patches) flush
    the translation cache, and so does a state load.

***************************************************************************/

/* the sljit build shared by the recompilers decides where this one runs */
#include "../../lib/sljit/sljit_mame.h"

#define PSXJIT_SUPPORTED		SLJIT_MAME_SUPPORTED

#if PSXJIT_SUPPORTED

#if (defined SLJIT_64BIT_ARCHITECTURE && SLJIT_64BIT_ARCHITECTURE)
#define JIT_HOST64		1
#else
#define JIT_HOST64		0
#endif

#define JIT_TABLE_BITS		18
#define JIT_TABLE_SIZE		(1 << JIT_TABLE_BITS)
#define JIT_MAX_BLOCK		64				/* instructions */
#define JIT_CODEMAP_SHIFT	2				/* host address granules of 4 bytes (one word), one byte each */
#define JIT_CODEMAP_BITS	22
#define JIT_CODEMAP_SIZE	(1 << JIT_CODEMAP_BITS)
#define JIT_MAX_EXITS		(JIT_MAX_BLOCK * 4)

struct jit_block
{
	void *		code;
	void *		entry;			/* where a block leaving for this one jumps in (after the prologue), NULL if none */
	UINT32		pc;				/* address of the first instruction */
	UINT16		len;			/* instructions (cycles) in the block, 0 = not translatable */
	UINT8		valid;
	UINT8		stores;			/* has stores (debug verification) */
	UINT8		ramop;			/* len 0 in fast RAM: op0 is the word that was not translatable */
	UINT32		op0;
};

typedef sljit_u32 (SLJIT_FUNC *jit_func)(void);

static void SLJIT_FUNC loop_skip_helper( sljit_u32 l );	/* psxloop.c */

/* the GTE, in psx_cpu.c */
static UINT32 getcp2dr( int n_reg );
static void setcp2dr( int n_reg, UINT32 n_value );
static void docop2( int gteop );
static void setcp2cr( int n_reg, UINT32 n_value );

static int jit_enabled;
static struct jit_block *jit_table;
static UINT8 *jit_codemap;
static int jit_code_present;
static UINT32 jit_flushes;
static size_t jit_code_bytes;		/* native code generated since the last flush */
static int jit_nomem_logged;

#define JIT_CODE_BUDGET		( 32 * 1024 * 1024 )

static INLINE void mips_step( void );


static INLINE UINT32 jit_codemap_index( const void *host )
{
	return (UINT32)( ( (size_t)host >> JIT_CODEMAP_SHIFT ) & ( JIT_CODEMAP_SIZE - 1 ) );
}

static void jit_flush( void )
{
	int i;

	if( !jit_table )
	{
		return;
	}
	for( i = 0; i < JIT_TABLE_SIZE; i++ )
	{
		if( jit_table[ i ].code )
		{
			sljit_free_code( jit_table[ i ].code, NULL );
		}
		jit_table[ i ].code = NULL;
		jit_table[ i ].entry = NULL;
		jit_table[ i ].valid = 0;
	}
	memset( jit_codemap, 0, JIT_CODEMAP_SIZE );
	jit_code_present = 0;
	jit_code_bytes = 0;
	jit_flushes++;
}

/*
	A CPU store over translated code (self-modifying code: some games
	patch an instruction of a routine every frame). Only the blocks that
	can contain the word are dropped: the ones starting at most
	JIT_MAX_BLOCK - 1 words before it, at each address the word is seen
	at (the RAM mirrors). The chains to a block go through the table, so
	they see it gone; the store itself is done by the interpreter (the
	recompiled stores leave the block when they hit translated code).
	The word stays marked in the codemap: the next store there comes
	back here.
*/
static INLINE UINT32 jit_table_index( UINT32 pc );

static void jit_invalidate_host( const void *host )
{
	int i;
	int k;

	for( i = 0; i < fastram_count; i++ )
	{
		const struct psx_fastram *r = &fastram[ i ];
		if( (const UINT8 *)host >= r->base && (const UINT8 *)host <= r->base + ( r->end - r->start ) )
		{
			UINT32 addr = ( r->start + (UINT32)( (const UINT8 *)host - r->base ) ) & ~3;
			for( k = 0; k < JIT_MAX_BLOCK; k++ )
			{
				UINT32 pc = addr - k * 4;
				struct jit_block *block = &jit_table[ jit_table_index( pc ) ];
				if( block->valid && block->pc == pc )
				{
					if( block->code )
					{
						sljit_free_code( block->code, NULL );
					}
					block->code = NULL;
					block->entry = NULL;
					block->valid = 0;
				}
			}
		}
	}
}

/* called by the fast RAM write handlers */
static INLINE void psx_jit_check_write( const void *host )
{
	if( jit_code_present && jit_codemap[ jit_codemap_index( host ) ] )
	{
		jit_invalidate_host( host );
	}
}

void psxcpu_ram_written( const void *host, UINT32 bytes )
{
	size_t first, last, g;

	if( !jit_code_present || bytes == 0 )
	{
		return;
	}
	first = (size_t)host >> JIT_CODEMAP_SHIFT;
	last = ( (size_t)host + bytes - 1 ) >> JIT_CODEMAP_SHIFT;
	if( last - first >= JIT_CODEMAP_SIZE )
	{
		jit_flush();
		return;
	}
	for( g = first; g <= last; g++ )
	{
		if( jit_codemap[ g & ( JIT_CODEMAP_SIZE - 1 ) ] )
		{
			jit_flush();
			return;
		}
	}
}

static void jit_exit( void )
{
	jit_flush();
	free( jit_table );
	free( jit_codemap );
	jit_table = NULL;
	jit_codemap = NULL;
	jit_enabled = 0;
}

static void jit_init( void )
{
	jit_table = calloc( JIT_TABLE_SIZE, sizeof( jit_table[ 0 ] ) );
	jit_codemap = calloc( JIT_CODEMAP_SIZE, 1 );
	if( !jit_table || !jit_codemap )
	{
		free( jit_table );
		free( jit_codemap );
		jit_table = NULL;
		jit_codemap = NULL;
		jit_enabled = 0;
		return;
	}
	jit_code_present = 0;
	jit_flushes = 0;
}


/*************************************
 *  Instruction decoding
 *************************************/

enum
{
	JIT_NONE = 0,		/* not translatable */
	JIT_SIMPLE,			/* native, falls through */
	JIT_MEM,			/* native load or store, falls through (may side exit) */
	JIT_HELPER,			/* the interpreter runs it, falls through */
	JIT_HELPER_MEM,		/* the interpreter runs it, after an address check */
	JIT_BRANCH			/* jump or branch (needs a delay slot) */
};

struct jit_op
{
	int			kind;
	UINT32		reads;			/* general registers read (bit mask) */
	UINT32		writes;			/* general registers written (bit mask, r0 excluded) */
	int			dload;			/* register of a delayed load (load, MFC0/MFC2/CFC2), or 0 */
	int			size;			/* memory access size (aligned word for LWL & co) */
	int			align;			/* address bits that must be clear */
	int			store;			/* writes memory */
	int			cop2;			/* needs COP2 enabled */
	int			trap;			/* ADD/ADDI/SUB: overflow exception */
	int			hilo;			/* writes HI/LO */
};

#define JBIT( r )		( (UINT32)1 << ( r ) )

static void jit_decode( UINT32 op, struct jit_op *o )
{
	int rs = INS_RS( op ), rt = INS_RT( op ), rd = INS_RD( op );

	memset( o, 0, sizeof( *o ) );
	switch( INS_OP( op ) )
	{
	case OP_SPECIAL:
		switch( INS_FUNCT( op ) )
		{
		case FUNCT_SLL: case FUNCT_SRL: case FUNCT_SRA:
			o->kind = JIT_SIMPLE; o->reads = JBIT( rt ); o->writes = JBIT( rd );
			break;
		case FUNCT_SLLV: case FUNCT_SRLV: case FUNCT_SRAV:
			o->kind = JIT_SIMPLE; o->reads = JBIT( rt ) | JBIT( rs ); o->writes = JBIT( rd );
			break;
		case FUNCT_JR:
			if( rd == 0 ) { o->kind = JIT_BRANCH; o->reads = JBIT( rs ); }
			break;
		case FUNCT_JALR:
			o->kind = JIT_BRANCH; o->reads = JBIT( rs ); o->writes = JBIT( rd );
			break;
		case FUNCT_MFHI: case FUNCT_MFLO:
			o->kind = JIT_SIMPLE; o->writes = JBIT( rd );
			break;
		case FUNCT_MTHI: case FUNCT_MTLO:
			if( rd == 0 ) { o->kind = JIT_SIMPLE; o->reads = JBIT( rs ); o->hilo = 1; }
			break;
		case FUNCT_MULT: case FUNCT_MULTU:
			if( rd == 0 ) { o->kind = JIT_SIMPLE; o->reads = JBIT( rs ) | JBIT( rt ); o->hilo = 1; }
			break;
		case FUNCT_DIV: case FUNCT_DIVU:
			if( rd == 0 ) { o->kind = JIT_HELPER; o->reads = JBIT( rs ) | JBIT( rt ); o->hilo = 1; }
			break;
		case FUNCT_ADD: case FUNCT_SUB:
			o->trap = 1;
			/* fall through */
		case FUNCT_ADDU: case FUNCT_SUBU: case FUNCT_AND: case FUNCT_OR:
		case FUNCT_XOR: case FUNCT_NOR: case FUNCT_SLT: case FUNCT_SLTU:
			o->kind = JIT_SIMPLE; o->reads = JBIT( rs ) | JBIT( rt ); o->writes = JBIT( rd );
			break;
		}
		break;

	case OP_REGIMM:
		switch( rt )
		{
		case RT_BLTZ: case RT_BGEZ:
			o->kind = JIT_BRANCH; o->reads = JBIT( rs );
			break;
		case RT_BLTZAL: case RT_BGEZAL:
			o->kind = JIT_BRANCH; o->reads = JBIT( rs ); o->writes = JBIT( 31 );
			break;
		}
		break;

	case OP_J:
		o->kind = JIT_BRANCH;
		break;
	case OP_JAL:
		o->kind = JIT_BRANCH; o->writes = JBIT( 31 );
		break;
	case OP_BEQ: case OP_BNE:
		o->kind = JIT_BRANCH; o->reads = JBIT( rs ) | JBIT( rt );
		break;
	case OP_BLEZ: case OP_BGTZ:
		/* rt != 0 is a reserved instruction for the interpreter */
		if( rt == 0 ) { o->kind = JIT_BRANCH; o->reads = JBIT( rs ); }
		break;

	case OP_ADDI:
		o->trap = 1;
		/* fall through */
	case OP_ADDIU: case OP_SLTI: case OP_SLTIU:
	case OP_ANDI: case OP_ORI: case OP_XORI:
		o->kind = JIT_SIMPLE; o->reads = JBIT( rs ); o->writes = JBIT( rt );
		break;
	case OP_LUI:
		o->kind = JIT_SIMPLE; o->writes = JBIT( rt );
		break;

	case OP_COP0:
		if( rs == RS_MFC )
		{
			o->kind = JIT_HELPER; o->writes = JBIT( rt ); o->dload = rt;
		}
		break;

	case OP_COP2:
		o->cop2 = 1;
		switch( rs )
		{
		case RS_MFC: case RS_CFC:
			o->kind = JIT_HELPER; o->writes = JBIT( rt ); o->dload = rt;
			break;
		case RS_MTC: case RS_CTC:
			o->kind = JIT_HELPER; o->reads = JBIT( rt );
			break;
		default:
			if( INS_CO( op ) )
			{
				o->kind = JIT_HELPER;
			}
			break;
		}
		break;

	case OP_LB: case OP_LBU:
		o->kind = JIT_MEM; o->reads = JBIT( rs ); o->writes = JBIT( rt ); o->dload = rt; o->size = 1;
		break;
	case OP_LH: case OP_LHU:
		o->kind = JIT_MEM; o->reads = JBIT( rs ); o->writes = JBIT( rt ); o->dload = rt; o->size = 2; o->align = 1;
		break;
	case OP_LW:
		o->kind = JIT_MEM; o->reads = JBIT( rs ); o->writes = JBIT( rt ); o->dload = rt; o->size = 4; o->align = 3;
		break;
	case OP_LWL: case OP_LWR:
		o->kind = JIT_HELPER_MEM; o->reads = JBIT( rs ) | JBIT( rt ); o->writes = JBIT( rt ); o->dload = rt; o->size = 4;
		break;
	case OP_SB:
		o->kind = JIT_MEM; o->reads = JBIT( rs ) | JBIT( rt ); o->size = 1; o->store = 1;
		break;
	case OP_SH:
		o->kind = JIT_MEM; o->reads = JBIT( rs ) | JBIT( rt ); o->size = 2; o->align = 1; o->store = 1;
		break;
	case OP_SW:
		o->kind = JIT_MEM; o->reads = JBIT( rs ) | JBIT( rt ); o->size = 4; o->align = 3; o->store = 1;
		break;
	case OP_SWL: case OP_SWR:
		o->kind = JIT_HELPER_MEM; o->reads = JBIT( rs ) | JBIT( rt ); o->size = 4; o->store = 1;
		break;
	case OP_LWC2:
		o->kind = JIT_HELPER_MEM; o->reads = JBIT( rs ); o->size = 4; o->align = 3; o->cop2 = 1;
		break;
	case OP_SWC2:
		o->kind = JIT_HELPER_MEM; o->reads = JBIT( rs ); o->size = 4; o->align = 3; o->store = 1; o->cop2 = 1;
		break;
	}
	o->writes &= ~1;
}


/*************************************
 *  Code generation helpers
 *************************************/

#define JMEM( off )		SLJIT_MEM1( SLJIT_S0 ), (sljit_sw)( off )
#define JREG( n )		JMEM( offsetof( mips_cpu_context, r ) + ( n ) * sizeof( UINT32 ) )
#define JHI				JMEM( offsetof( mips_cpu_context, hi ) )
#define JLO				JMEM( offsetof( mips_cpu_context, lo ) )
#define JDELAYR			JMEM( offsetof( mips_cpu_context, delayr ) )
#define JDELAYV			JMEM( offsetof( mips_cpu_context, delayv ) )
#define JSR				JMEM( offsetof( mips_cpu_context, cp0r ) + CP0_SR * sizeof( UINT32 ) )

#define OPC_SIMM( o )	( (sljit_sw)(INT16)( o ) )
#define OPC_UIMM( o )	( (sljit_sw)(UINT16)( o ) )

/* translation context of one block */
struct jit_ctx
{
	struct sljit_compiler *	C;
	UINT32					pc;				/* address of the first instruction */
	UINT32					count;			/* instructions in the block */
	UINT32					acct;			/* instruction up to which cycles are already taken */
	UINT8					target[ JIT_MAX_BLOCK ];	/* instruction n is a jump target in the block */
	UINT8					idle[ JIT_MAX_BLOCK ];		/* branch n closes a polling loop */
	struct sljit_label *	label[ JIT_MAX_BLOCK ];
	int						npending;
	struct
	{
		struct sljit_jump *	jump;
		UINT32				index;
	} pending[ JIT_MAX_BLOCK * 2 ];			/* jumps to labels not emitted yet */
	int						nexits;
	int						overflow;		/* an exit did not fit: the block is not translated */
	struct
	{
		struct sljit_jump *	jump;
		UINT32				index;			/* instruction to resume at */
		UINT32				cycles;			/* cycles still to take */
	} exits[ JIT_MAX_EXITS ];
};

/* leave before instruction 'index' and let the interpreter run it */
static void jit_add_exit( struct jit_ctx *ctx, struct sljit_jump *jump, UINT32 index )
{
	if( ctx->nexits < JIT_MAX_EXITS )
	{
		ctx->exits[ ctx->nexits ].jump = jump;
		ctx->exits[ ctx->nexits ].index = index;
		ctx->exits[ ctx->nexits ].cycles = index - ctx->acct;
		ctx->nexits++;
	}
	else
	{
		ctx->overflow = 1;
	}
}

/* take the cycles of the instructions up to 'index' from the timeslice */
static void jit_emit_cycles_to( struct jit_ctx *ctx, UINT32 index )
{
	UINT32 cycles = index - ctx->acct;
	if( cycles )
	{
		sljit_emit_op2( ctx->C, SLJIT_SUB32, SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_IMM, cycles );
	}
}

/* 0/1 from a comparison into a register */
static void jit_setcc( struct sljit_compiler *C, int reg, sljit_s32 cond,
	sljit_s32 src1, sljit_sw src1w, sljit_s32 src2, sljit_sw src2w )
{
	sljit_emit_op2u( C, SLJIT_SUB32 | SLJIT_SET( cond & ~1 ), src1, src1w, src2, src2w );
	sljit_emit_op_flags( C, SLJIT_MOV32, SLJIT_R0, 0, cond );
	sljit_emit_op1( C, SLJIT_MOV32, JREG( reg ), SLJIT_R0, 0 );
}

/* R0 = rs op src2, into rd (r0 discarded) */
static void jit_alu( struct sljit_compiler *C, sljit_s32 op, int rd, int rs, sljit_s32 src2, sljit_sw src2w )
{
	if( rd )
	{
		sljit_emit_op2( C, op, SLJIT_R0, 0, JREG( rs ), src2, src2w );
		sljit_emit_op1( C, SLJIT_MOV32, JREG( rd ), SLJIT_R0, 0 );
	}
}

/*
	ADD/ADDI/SUB: leave before the instruction on overflow (the
	interpreter raises the exception), otherwise as ADDU/ADDIU/SUBU,
	r0 discarded.
*/
static void jit_alu_trap( struct jit_ctx *ctx, sljit_s32 op, int rd, int rs, sljit_s32 src2, sljit_sw src2w, UINT32 index )
{
	struct sljit_compiler *C = ctx->C;
	sljit_emit_op2( C, op | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, JREG( rs ), src2, src2w );
	jit_add_exit( ctx, sljit_emit_jump( C, SLJIT_OVERFLOW ), index );
	if( rd )
	{
		sljit_emit_op1( C, SLJIT_MOV32, JREG( rd ), SLJIT_R0, 0 );
	}
}

/* COP2 instructions raise an exception when COP2 is disabled: leave first */
static void jit_check_cop2( struct jit_ctx *ctx, UINT32 index )
{
	struct sljit_compiler *C = ctx->C;
	sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JSR );
	sljit_emit_op2u( C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, SR_CU2 );
	jit_add_exit( ctx, sljit_emit_jump( C, SLJIT_ZERO ), index );
}

/* the interpreter runs one instruction (no exception possible, fast RAM only) */
static void SLJIT_FUNC jit_helper_step( sljit_u32 op )
{
	mipscpu.op = op;
	mips_step();
}

static void jit_emit_helper( struct sljit_compiler *C, UINT32 op )
{
	sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)op );
	sljit_emit_icall( C, SLJIT_CALL, SLJIT_ARGS1V( 32 ), SLJIT_IMM, SLJIT_FUNC_ADDR( jit_helper_step ) );
}

/*
	The GTE instructions without the trip through mips_step. Inside a
	block no load or branch is pending (delayr is 0) and COP2 is known to
	be usable (jit_check_cop2), so the interpreter's cases reduce to these.
	Moves of the data registers that getcp2dr/setcp2dr only store are
	native code; the others, and the commands, call those functions.
*/
#define JCP2DR( n )		JMEM( offsetof( mips_cpu_context, cp2dr ) + ( n ) * sizeof( PAIR ) )
#define JCP2CR( n )		JMEM( offsetof( mips_cpu_context, cp2cr ) + ( n ) * sizeof( PAIR ) )

static void SLJIT_FUNC jit_gte_mfc2( sljit_u32 op )
{
	UINT32 v = getcp2dr( INS_RD( op ) );
	if( INS_RT( op ) != 0 )
	{
		mipscpu.r[ INS_RT( op ) ] = v;
	}
}

static void SLJIT_FUNC jit_gte_mtc2( sljit_u32 op )
{
	setcp2dr( INS_RD( op ), mipscpu.r[ INS_RT( op ) ] );
}

static void SLJIT_FUNC jit_gte_ctc2( sljit_u32 op )
{
	setcp2cr( INS_RD( op ), mipscpu.r[ INS_RT( op ) ] );
}

static void SLJIT_FUNC jit_gte_cmd( sljit_u32 op )
{
	docop2( INS_COFUN( op ) );
}

static void SLJIT_FUNC jit_gte_lwc2( sljit_u32 op, sljit_up host )
{
	setcp2dr( INS_RT( op ), *(UINT32 *)host );
}

static void SLJIT_FUNC jit_gte_swc2( sljit_u32 op, sljit_up host )
{
	*(UINT32 *)host = getcp2dr( INS_RT( op ) );
}

/* getcp2dr / setcp2dr do more than read / store this data register */
static int jit_gte_dr_special_get( int n )
{
	return n == 1 || n == 3 || n == 5 || n == 7 || ( n >= 8 && n <= 11 ) || ( n >= 15 && n <= 19 ) || n == 28 || n == 29;
}
static int jit_gte_dr_special_set( int n )
{
	return n == 15 || n == 28 || n == 30 || n == 31;
}
/* setcp2cr sign extends or masks this control register */
static int jit_gte_cr_special_set( int n )
{
	return n == 4 || n == 12 || n == 20 || n == 26 || n == 27 || n == 29 || n == 30 || n == 31;
}

static void jit_emit_call1( struct sljit_compiler *C, UINT32 op, void *fn )
{
	sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)op );
	sljit_emit_icall( C, SLJIT_CALL, SLJIT_ARGS1V( 32 ), SLJIT_IMM, SLJIT_FUNC_ADDR( fn ) );
}

/*
	A GTE instruction, or one of the other helpers (jit_emit_helper).
	'host' holds the checked fast RAM address of LWC2/SWC2.
*/
static void jit_emit_helper_op( struct sljit_compiler *C, UINT32 op, sljit_s32 host )
{
	int rt = INS_RT( op ), rd = INS_RD( op );

	if( INS_OP( op ) == OP_COP2 )
	{
		switch( INS_RS( op ) )
		{
		case RS_MFC:
			if( jit_gte_dr_special_get( rd ) )
			{
				jit_emit_call1( C, op, (void *)jit_gte_mfc2 );
			}
			else if( rt )
			{
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JCP2DR( rd ) );
				sljit_emit_op1( C, SLJIT_MOV32, JREG( rt ), SLJIT_R0, 0 );
			}
			return;
		case RS_CFC:
			if( rt )
			{
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JCP2CR( rd ) );
				sljit_emit_op1( C, SLJIT_MOV32, JREG( rt ), SLJIT_R0, 0 );
			}
			return;
		case RS_MTC:
			if( jit_gte_dr_special_set( rd ) )
			{
				jit_emit_call1( C, op, (void *)jit_gte_mtc2 );
			}
			else
			{
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rt ) );
				sljit_emit_op1( C, SLJIT_MOV32, JCP2DR( rd ), SLJIT_R0, 0 );
			}
			return;
		case RS_CTC:
			if( jit_gte_cr_special_set( rd ) )
			{
				jit_emit_call1( C, op, (void *)jit_gte_ctc2 );
			}
			else
			{
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rt ) );
				sljit_emit_op1( C, SLJIT_MOV32, JCP2CR( rd ), SLJIT_R0, 0 );
			}
			return;
		default:
			if( INS_RS( op ) >= 16 && INS_CO( op ) == 1 )
			{
				jit_emit_call1( C, op, (void *)jit_gte_cmd );
				return;
			}
			break;
		}
	}
	else if( INS_OP( op ) == OP_LWC2 || INS_OP( op ) == OP_SWC2 )
	{
		if( host != SLJIT_R1 )
		{
			sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_R1, 0, host, 0 );
		}
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_s32)op );
		if( INS_OP( op ) == OP_LWC2 )
			sljit_emit_icall( C, SLJIT_CALL, SLJIT_ARGS2V( 32, P ), SLJIT_IMM, SLJIT_FUNC_ADDR( jit_gte_lwc2 ) );
		else
			sljit_emit_icall( C, SLJIT_CALL, SLJIT_ARGS2V( 32, P ), SLJIT_IMM, SLJIT_FUNC_ADDR( jit_gte_swc2 ) );
		return;
	}
	jit_emit_helper( C, op );
}


/*************************************
 *  Instruction translation
 *
 *  Each case mirrors the interpreter in psx_cpu.c exactly.
 *************************************/

static void jit_emit_simple( struct jit_ctx *ctx, UINT32 op, UINT32 index )
{
	struct sljit_compiler *C = ctx->C;
	int rs = INS_RS( op ), rt = INS_RT( op ), rd = INS_RD( op ), sa = INS_SHAMT( op );

	switch( INS_OP( op ) )
	{
	case OP_SPECIAL:
		switch( INS_FUNCT( op ) )
		{
		case FUNCT_SLL:		jit_alu( C, SLJIT_SHL32, rd, rt, SLJIT_IMM, sa ); break;
		case FUNCT_SRL:		jit_alu( C, SLJIT_LSHR32, rd, rt, SLJIT_IMM, sa ); break;
		case FUNCT_SRA:		jit_alu( C, SLJIT_ASHR32, rd, rt, SLJIT_IMM, sa ); break;
		case FUNCT_SLLV:	jit_alu( C, SLJIT_MSHL32, rd, rt, JREG( rs ) ); break;
		case FUNCT_SRLV:	jit_alu( C, SLJIT_MLSHR32, rd, rt, JREG( rs ) ); break;
		case FUNCT_SRAV:	jit_alu( C, SLJIT_MASHR32, rd, rt, JREG( rs ) ); break;
		case FUNCT_MFHI:
			if( rd ) { sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JHI ); sljit_emit_op1( C, SLJIT_MOV32, JREG( rd ), SLJIT_R0, 0 ); }
			break;
		case FUNCT_MFLO:
			if( rd ) { sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JLO ); sljit_emit_op1( C, SLJIT_MOV32, JREG( rd ), SLJIT_R0, 0 ); }
			break;
		case FUNCT_MTHI:
			sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rs ) ); sljit_emit_op1( C, SLJIT_MOV32, JHI, SLJIT_R0, 0 );
			break;
		case FUNCT_MTLO:
			sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rs ) ); sljit_emit_op1( C, SLJIT_MOV32, JLO, SLJIT_R0, 0 );
			break;
		case FUNCT_MULT:
		case FUNCT_MULTU:
#if JIT_HOST64
			sljit_emit_op1( C, ( INS_FUNCT( op ) == FUNCT_MULT ) ? SLJIT_MOV_S32 : SLJIT_MOV_U32, SLJIT_R0, 0, JREG( rs ) );
			sljit_emit_op1( C, ( INS_FUNCT( op ) == FUNCT_MULT ) ? SLJIT_MOV_S32 : SLJIT_MOV_U32, SLJIT_R1, 0, JREG( rt ) );
			sljit_emit_op2( C, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0 );
			sljit_emit_op2( C, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 32 );
#else
			sljit_emit_op1( C, SLJIT_MOV, SLJIT_R0, 0, JREG( rs ) );
			sljit_emit_op1( C, SLJIT_MOV, SLJIT_R1, 0, JREG( rt ) );
			sljit_emit_op0( C, ( INS_FUNCT( op ) == FUNCT_MULT ) ? SLJIT_LMUL_SW : SLJIT_LMUL_UW );
#endif
			sljit_emit_op1( C, SLJIT_MOV32, JLO, SLJIT_R0, 0 );
			sljit_emit_op1( C, SLJIT_MOV32, JHI, SLJIT_R1, 0 );
			break;
		case FUNCT_ADD:		jit_alu_trap( ctx, SLJIT_ADD32, rd, rs, JREG( rt ), index ); break;
		case FUNCT_ADDU:	jit_alu( C, SLJIT_ADD32, rd, rs, JREG( rt ) ); break;
		case FUNCT_SUB:		jit_alu_trap( ctx, SLJIT_SUB32, rd, rs, JREG( rt ), index ); break;
		case FUNCT_SUBU:	jit_alu( C, SLJIT_SUB32, rd, rs, JREG( rt ) ); break;
		case FUNCT_AND:		jit_alu( C, SLJIT_AND32, rd, rs, JREG( rt ) ); break;
		case FUNCT_OR:		jit_alu( C, SLJIT_OR32, rd, rs, JREG( rt ) ); break;
		case FUNCT_XOR:		jit_alu( C, SLJIT_XOR32, rd, rs, JREG( rt ) ); break;
		case FUNCT_NOR:
			if( rd )
			{
				sljit_emit_op2( C, SLJIT_OR32, SLJIT_R0, 0, JREG( rs ), JREG( rt ) );
				sljit_emit_op2( C, SLJIT_XOR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, -1 );
				sljit_emit_op1( C, SLJIT_MOV32, JREG( rd ), SLJIT_R0, 0 );
			}
			break;
		case FUNCT_SLT:
			if( rd ) jit_setcc( C, rd, SLJIT_SIG_LESS, JREG( rs ), JREG( rt ) );
			break;
		case FUNCT_SLTU:
			if( rd ) jit_setcc( C, rd, SLJIT_LESS, JREG( rs ), JREG( rt ) );
			break;
		}
		break;

	case OP_ADDI:	jit_alu_trap( ctx, SLJIT_ADD32, rt, rs, SLJIT_IMM, OPC_SIMM( op ), index ); break;
	case OP_ADDIU:	jit_alu( C, SLJIT_ADD32, rt, rs, SLJIT_IMM, OPC_SIMM( op ) ); break;
	case OP_SLTI:
		if( rt ) jit_setcc( C, rt, SLJIT_SIG_LESS, JREG( rs ), SLJIT_IMM, OPC_SIMM( op ) );
		break;
	case OP_SLTIU:	/* against the sign-extended immediate */
		if( rt ) jit_setcc( C, rt, SLJIT_LESS, JREG( rs ), SLJIT_IMM, OPC_SIMM( op ) );
		break;
	case OP_ANDI:	jit_alu( C, SLJIT_AND32, rt, rs, SLJIT_IMM, OPC_UIMM( op ) ); break;
	case OP_ORI:	jit_alu( C, SLJIT_OR32, rt, rs, SLJIT_IMM, OPC_UIMM( op ) ); break;
	case OP_XORI:	jit_alu( C, SLJIT_XOR32, rt, rs, SLJIT_IMM, OPC_UIMM( op ) ); break;
	case OP_LUI:
		if( rt ) sljit_emit_op1( C, SLJIT_MOV32, JREG( rt ), SLJIT_IMM, (sljit_s32)( OPC_UIMM( op ) << 16 ) );
		break;
	}
}


/*************************************
 *  Loads and stores
 *************************************/

/*
	Host address of the access of 'op' into R1, or a side exit with
	'index' instructions completed: the whole access must be in one fast
	RAM region (writable for a store) and aligned as the interpreter
	requires; a store must not touch translated code. LWL & co check their
	aligned word.
*/
static void jit_emit_mem_address( struct jit_ctx *ctx, UINT32 op, const struct jit_op *o, UINT32 index )
{
	struct sljit_compiler *C = ctx->C;
	struct sljit_jump *found[ PSX_MAX_FASTRAM ];
	struct sljit_label *ready;
	int i, n = 0;

	/* R0 = rs + simm */
	sljit_emit_op2( C, SLJIT_ADD32, SLJIT_R0, 0, JREG( INS_RS( op ) ), SLJIT_IMM, OPC_SIMM( op ) );
	if( o->align )
	{
		sljit_emit_op2u( C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, o->align );
		jit_add_exit( ctx, sljit_emit_jump( C, SLJIT_NOT_ZERO ), index );
	}
	sljit_emit_op1( C, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_R0, 0 );
	if( o->kind == JIT_HELPER_MEM && !o->align )
	{
		sljit_emit_op2( C, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0xfffffffc );
	}

	/* R1 = host pointer, from the first region that holds the whole access */
	for( i = 0; i < fastram_count; i++ )
	{
		struct sljit_jump *miss;
		if( fastram[ i ].end - fastram[ i ].start + 1 < (UINT32)o->size )
		{
			continue;
		}
		if( fastram[ i ].readonly && o->store )
		{
			continue;
		}
		sljit_emit_op2( C, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)fastram[ i ].start );
		sljit_emit_op1( C, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0 );
		miss = sljit_emit_cmp( C, SLJIT_GREATER, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)( fastram[ i ].end - fastram[ i ].start - ( o->size - 1 ) ) );
		sljit_emit_op2( C, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)fastram[ i ].base );
		found[ n++ ] = sljit_emit_jump( C, SLJIT_JUMP );
		sljit_set_label( miss, sljit_emit_label( C ) );
	}

	/* not fast RAM: let the interpreter do it */
	jit_add_exit( ctx, sljit_emit_jump( C, SLJIT_JUMP ), index );

	ready = sljit_emit_label( C );
	for( i = 0; i < n; i++ )
	{
		sljit_set_label( found[ i ], ready );
	}

	/* a store over translated code: let the interpreter do it (and flush) */
	if( o->store )
	{
		sljit_emit_op2( C, SLJIT_LSHR, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, JIT_CODEMAP_SHIFT );
		sljit_emit_op2( C, SLJIT_AND, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, JIT_CODEMAP_SIZE - 1 );
		sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)jit_codemap );
		sljit_emit_op1( C, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM2( SLJIT_R0, SLJIT_R2 ), 0 );
		jit_add_exit( ctx, sljit_emit_cmp( C, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0 ), index );
	}
}

#ifdef PSXJIT_VERIFY
static void SLJIT_FUNC jit_verify_log_store( sljit_up host, sljit_u32 size )
{
	if( jit_vlog_count < PSXJIT_VLOG )
	{
		jit_vlog[ jit_vlog_count ].p = (UINT8 *)host;
		jit_vlog[ jit_vlog_count ].size = size;
		memcpy( &jit_vlog[ jit_vlog_count ].old, (void *)host, size );
		jit_vlog_count++;
	}
}

/* debug: log the store about to go through 'host'; the pointer moves to S3 */
static void jit_emit_verify_log( struct sljit_compiler *C, sljit_s32 host, int size )
{
	sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_S3, 0, host, 0 );
	sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_S3, 0 );
	sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, size );
	sljit_emit_icall( C, SLJIT_CALL, SLJIT_ARGS2V( P, 32 ), SLJIT_IMM, SLJIT_FUNC_ADDR( jit_verify_log_store ) );
}
#endif

/*
	The access through the host pointer in 'host'. A load writes its
	register (r0 discarded), or, with 'pending', only leaves the value
	in R0 (for a branch to decide).
*/
static void jit_emit_mem_access( struct sljit_compiler *C, UINT32 op, sljit_s32 host, int pending )
{
	int rt = INS_RT( op );
	sljit_s32 load = 0;

	switch( INS_OP( op ) )
	{
	case OP_LB:		load = SLJIT_MOV32_S8;	break;
	case OP_LH:		load = SLJIT_MOV32_S16;	break;
	case OP_LW:		load = SLJIT_MOV32;		break;
	case OP_LBU:	load = SLJIT_MOV32_U8;	break;
	case OP_LHU:	load = SLJIT_MOV32_U16;	break;

#ifdef PSXJIT_VERIFY
	case OP_SB: case OP_SH: case OP_SW:
		jit_emit_verify_log( C, host, ( INS_OP( op ) == OP_SB ) ? 1 : ( INS_OP( op ) == OP_SH ) ? 2 : 4 );
		host = SLJIT_S3;
		break;
	}
	switch( INS_OP( op ) )
	{
#endif
	case OP_SB:
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rt ) );
		sljit_emit_op1( C, SLJIT_MOV32_U8, SLJIT_MEM1( host ), 0, SLJIT_R0, 0 );
		return;
	case OP_SH:
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rt ) );
		sljit_emit_op1( C, SLJIT_MOV32_U16, SLJIT_MEM1( host ), 0, SLJIT_R0, 0 );
		return;
	case OP_SW:
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, JREG( rt ) );
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_MEM1( host ), 0, SLJIT_R0, 0 );
		return;
	}

	sljit_emit_op1( C, load, SLJIT_R0, 0, SLJIT_MEM1( host ), 0 );
	if( !pending && rt )
	{
		sljit_emit_op1( C, SLJIT_MOV32, JREG( rt ), SLJIT_R0, 0 );
	}
}


/*************************************
 *  Branches
 *************************************/

static UINT32 jit_branch_target( UINT32 op, UINT32 pc )
{
	if( INS_OP( op ) == OP_J || INS_OP( op ) == OP_JAL )
	{
		return ( ( pc + 4 ) & 0xf0000000 ) + ( INS_TARGET( op ) << 2 );
	}
	return pc + 4 + ( MIPS_WORD_EXTEND( INS_IMMEDIATE( op ) ) << 2 );
}

static int jit_branch_is_conditional( UINT32 op )
{
	switch( INS_OP( op ) )
	{
	case OP_REGIMM: case OP_BEQ: case OP_BNE: case OP_BLEZ: case OP_BGTZ:
		return 1;
	}
	return 0;
}

/* can 'slot' be translated as the delay slot of 'op'? */
static int jit_slot_ok( UINT32 op, UINT32 slot )
{
	struct jit_op b, s;
	jit_decode( op, &b );
	jit_decode( slot, &s );
	switch( s.kind )
	{
	case JIT_SIMPLE:
		/* an overflow is checked before the branch, so no link register input */
		return !s.trap || !( s.reads & b.writes );
	case JIT_MEM:
		/* the address is checked before the branch */
		return !( s.reads & b.writes );
	case JIT_HELPER:
		/* a delayed load from a helper could not stay pending */
		return s.dload == 0;
	case JIT_HELPER_MEM:
		return s.dload == 0 && !( s.reads & b.writes );
	}
	return 0;
}

/* target address -> instruction index inside the block, or -1 */
static int jit_target_index( struct jit_ctx *ctx, UINT32 target )
{
	if( target >= ctx->pc && target < ctx->pc + ctx->count * 4 && !( target & 3 ) )
	{
		return ( target - ctx->pc ) / 4;
	}
	return -1;
}

/*
	Block chaining. Leaving for an address whose block is translated and
	fits in what is left of the timeslice, jump straight into it instead
	of returning to the dispatcher (jit_run), which would do the same:
	no load is pending and the mode is unchanged on the way out of a
	block, so it would look the block up, check that its cycles fit and
	run it. All blocks share one stack frame (same sljit_emit_enter), so
	the jump lands after the prologue and the block's own return goes
	back to the dispatcher. The check reads the block table when it runs:
	blocks compiled, replaced or flushed later are seen, and a block that
	is not there or does not fit is simply a return to the dispatcher.
*/
static INLINE UINT32 jit_table_index( UINT32 pc )
{
	return ( ( pc >> 2 ) ^ ( pc >> 20 ) ) & ( JIT_TABLE_SIZE - 1 );
}

static void jit_emit_chain( struct sljit_compiler *C, sljit_s32 src, sljit_sw srcw )
{
#ifndef PSXJIT_VERIFY
	struct sljit_jump *miss[ 3 ];
	struct sljit_label *out;
	int i;

	if( src == SLJIT_IMM )
	{
		UINT32 target = (UINT32)srcw;
		if( target & 3 )
		{
			return;
		}
		sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)&jit_table[ jit_table_index( target ) ] );
		miss[ 0 ] = sljit_emit_cmp( C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM1( SLJIT_R1 ), offsetof( struct jit_block, pc ), SLJIT_IMM, (sljit_sw)target );
	}
	else
	{
		/* R1 = &jit_table[ ( ( pc >> 2 ) ^ ( pc >> 20 ) ) & mask ] */
		sljit_emit_op2( C, SLJIT_LSHR32, SLJIT_R1, 0, src, srcw, SLJIT_IMM, 2 );
		sljit_emit_op2( C, SLJIT_LSHR32, SLJIT_R2, 0, src, srcw, SLJIT_IMM, 20 );
		sljit_emit_op2( C, SLJIT_XOR32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0 );
		sljit_emit_op2( C, SLJIT_AND32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, JIT_TABLE_SIZE - 1 );
		/* upper bits of a 32-bit result are undefined for word operations */
		sljit_emit_op1( C, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0 );
		sljit_emit_op2( C, SLJIT_MUL, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, sizeof( struct jit_block ) );
		sljit_emit_op2( C, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)jit_table );
		miss[ 0 ] = sljit_emit_cmp( C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM1( SLJIT_R1 ), offsetof( struct jit_block, pc ), src, srcw );
	}
	/* entry is NULL for a block that is flushed, being compiled or not translatable */
	sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_R2, 0, SLJIT_MEM1( SLJIT_R1 ), offsetof( struct jit_block, entry ) );
	miss[ 1 ] = sljit_emit_cmp( C, SLJIT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0 );
	sljit_emit_op1( C, SLJIT_MOV32_U16, SLJIT_R1, 0, SLJIT_MEM1( SLJIT_R1 ), offsetof( struct jit_block, len ) );
	miss[ 2 ] = sljit_emit_cmp( C, SLJIT_SIG_GREATER | SLJIT_32, SLJIT_R1, 0, SLJIT_MEM0(), (sljit_sw)&mips_ICount );
	sljit_emit_ijump( C, SLJIT_JUMP, SLJIT_R2, 0 );
	out = sljit_emit_label( C );
	for( i = 0; i < 3; i++ )
	{
		sljit_set_label( miss[ i ], out );
	}
#endif
}

/*
	Leave for 'target' after the instructions up to 'index'. A target
	inside the block is reached in native code while the timeslice has
	room for the rest of the block from there, exactly as the dispatcher
	would enter a block there; otherwise we return to the dispatcher.
	At the end of a polling loop the passes that still fit are skipped
	first (they would leave the same state). 'branch' is the branch
	instruction, or JIT_LEAVE_PLAIN, or JIT_LEAVE_OUT to always return
	(a load left pending for the interpreter).
*/
#define JIT_LEAVE_PLAIN		-1
#define JIT_LEAVE_OUT		-2

static void jit_emit_leave( struct jit_ctx *ctx, UINT32 index, sljit_s32 src, sljit_sw srcw, int branch )
{
	struct sljit_compiler *C = ctx->C;

	jit_emit_cycles_to( ctx, index );
	if( src == SLJIT_IMM && branch != JIT_LEAVE_OUT )
	{
		int t = jit_target_index( ctx, (UINT32)srcw );
		if( t >= 0 && ctx->target[ t ] && ctx->npending < JIT_MAX_BLOCK * 2 )
		{
			struct sljit_jump *jump;
			if( branch >= 0 && ctx->idle[ branch ] )
			{
				/* icount -= ((icount - 1) / pass) * pass, while icount > pass */
				UINT32 pass = index - t;
				struct sljit_jump *few;
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_MEM0(), (sljit_sw)&mips_ICount );
				few = sljit_emit_cmp( C, SLJIT_SIG_LESS_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, pass );
				sljit_emit_op2( C, SLJIT_SUB32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1 );
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, pass );
				sljit_emit_op0( C, SLJIT_DIVMOD_U32 );
				/* R1 = (icount - 1) % pass: icount becomes R1 + 1 */
				sljit_emit_op2( C, SLJIT_ADD32, SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_R1, 0, SLJIT_IMM, 1 );
				sljit_set_label( few, sljit_emit_label( C ) );
			}
			jump = sljit_emit_cmp( C, SLJIT_SIG_GREATER_EQUAL | SLJIT_32,
				SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_IMM, ctx->count - t );
			if( ctx->label[ t ] )
			{
				sljit_set_label( jump, ctx->label[ t ] );
			}
			else
			{
				ctx->pending[ ctx->npending ].jump = jump;
				ctx->pending[ ctx->npending ].index = t;
				ctx->npending++;
			}
		}
	}
	if( branch != JIT_LEAVE_OUT )
	{
		jit_emit_chain( C, src, srcw );
	}
	sljit_emit_return( C, SLJIT_MOV32, src, srcw );
}

/* condition of a conditional branch into 'dst' (0/1) */
static void jit_emit_condition( struct sljit_compiler *C, UINT32 op, sljit_s32 dst )
{
	int rs = INS_RS( op ), rt = INS_RT( op );
	sljit_s32 cond;

	switch( INS_OP( op ) )
	{
	case OP_REGIMM:		/* BLTZ / BGEZ / BLTZAL / BGEZAL */
		cond = ( rt == RT_BLTZ || rt == RT_BLTZAL ) ? SLJIT_SIG_LESS : SLJIT_SIG_GREATER_EQUAL;
		sljit_emit_op2u( C, SLJIT_SUB32 | SLJIT_SET( cond & ~1 ), JREG( rs ), SLJIT_IMM, 0 );
		sljit_emit_op_flags( C, SLJIT_MOV, dst, 0, cond );
		break;
	case OP_BEQ:
	case OP_BNE:
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R1, 0, JREG( rs ) );
		sljit_emit_op2u( C, SLJIT_SUB32 | SLJIT_SET_Z, SLJIT_R1, 0, JREG( rt ) );
		sljit_emit_op_flags( C, SLJIT_MOV, dst, 0, ( INS_OP( op ) == OP_BEQ ) ? SLJIT_EQUAL : SLJIT_NOT_EQUAL );
		break;
	case OP_BLEZ:
		sljit_emit_op2u( C, SLJIT_SUB32 | SLJIT_SET_SIG_LESS_EQUAL, JREG( rs ), SLJIT_IMM, 0 );
		sljit_emit_op_flags( C, SLJIT_MOV, dst, 0, SLJIT_SIG_LESS_EQUAL );
		break;
	case OP_BGTZ:
		sljit_emit_op2u( C, SLJIT_SUB32 | SLJIT_SET_SIG_GREATER, JREG( rs ), SLJIT_IMM, 0 );
		sljit_emit_op_flags( C, SLJIT_MOV, dst, 0, SLJIT_SIG_GREATER );
		break;
	}
}

/* checks of the delay slot done before the branch: side exits leave before the branch */
static void jit_emit_slot_checks( struct jit_ctx *ctx, UINT32 slot, const struct jit_op *s, UINT32 index )
{
	struct sljit_compiler *C = ctx->C;

	if( s->cop2 )
	{
		jit_check_cop2( ctx, index );
	}
	if( s->kind == JIT_MEM || s->kind == JIT_HELPER_MEM )
	{
		jit_emit_mem_address( ctx, slot, s, index );
		sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_S2, 0, SLJIT_R1, 0 );
	}
	else if( s->trap )
	{
		/* ADD/SUB/ADDI: the overflow test alone */
		switch( INS_OP( slot ) )
		{
		case OP_ADDI:
			sljit_emit_op2u( C, SLJIT_ADD32 | SLJIT_SET_OVERFLOW, JREG( INS_RS( slot ) ), SLJIT_IMM, OPC_SIMM( slot ) );
			break;
		default:
			sljit_emit_op2u( C, ( ( INS_FUNCT( slot ) == FUNCT_ADD ) ? SLJIT_ADD32 : SLJIT_SUB32 ) | SLJIT_SET_OVERFLOW,
				JREG( INS_RS( slot ) ), JREG( INS_RT( slot ) ) );
			break;
		}
		jit_add_exit( ctx, sljit_emit_jump( C, SLJIT_OVERFLOW ), index );
	}
}

/* the delay slot itself, after its checks ('pending': a load only leaves its value in R0) */
static void jit_emit_slot( struct jit_ctx *ctx, UINT32 slot, const struct jit_op *s, int pending )
{
	struct sljit_compiler *C = ctx->C;

	switch( s->kind )
	{
	case JIT_MEM:
		jit_emit_mem_access( C, slot, SLJIT_S2, pending );
		break;
	case JIT_HELPER:
	case JIT_HELPER_MEM:
#ifdef PSXJIT_VERIFY
		if( s->store ) jit_emit_verify_log( C, SLJIT_S2, 4 );
#endif
		jit_emit_helper_op( C, slot, SLJIT_S2 );
		break;
	default:
		if( s->trap )
		{
			/* known not to overflow */
			switch( INS_OP( slot ) )
			{
			case OP_ADDI:	jit_alu( C, SLJIT_ADD32, INS_RT( slot ), INS_RS( slot ), SLJIT_IMM, OPC_SIMM( slot ) ); break;
			default:		jit_alu( C, ( INS_FUNCT( slot ) == FUNCT_ADD ) ? SLJIT_ADD32 : SLJIT_SUB32, INS_RD( slot ), INS_RS( slot ), JREG( INS_RT( slot ) ) ); break;
			}
		}
		else
		{
			jit_emit_simple( ctx, slot, 0 );
		}
		break;
	}
}

/*
	Branch at instruction 'index' with its delay slot. The slot is checked
	first, then the condition (or jump target) goes into S1 and the link
	is written, as in the interpreter, then the slot runs. A load in the
	slot of a taken branch stays pending (delayr/delayv) for the
	interpreter, which runs the first instruction at the target.
*/
static void jit_emit_branch( struct jit_ctx *ctx, UINT32 op, UINT32 slot, UINT32 index )
{
	struct sljit_compiler *C = ctx->C;
	struct jit_op s;
	UINT32 pc = ctx->pc + index * 4;
	UINT32 next = pc + 8;
	UINT32 target = jit_branch_target( op, pc );
	int dload;

	jit_decode( slot, &s );
	dload = ( s.kind == JIT_MEM ) ? s.dload : 0;		/* r0 too: delayr = 0 then */

	jit_emit_slot_checks( ctx, slot, &s, index );

	switch( INS_OP( op ) )
	{
	case OP_SPECIAL:	/* JR / JALR: an unaligned target raises an exception */
		sljit_emit_op1( C, SLJIT_MOV32, SLJIT_S1, 0, JREG( INS_RS( op ) ) );
		sljit_emit_op2u( C, SLJIT_AND32 | SLJIT_SET_Z, SLJIT_S1, 0, SLJIT_IMM, 3 );
		jit_add_exit( ctx, sljit_emit_jump( C, SLJIT_NOT_ZERO ), index );
		if( INS_FUNCT( op ) == FUNCT_JALR && INS_RD( op ) != 0 )
		{
			sljit_emit_op1( C, SLJIT_MOV32, JREG( INS_RD( op ) ), SLJIT_IMM, (sljit_s32)next );
		}
		break;
	case OP_J:
		break;
	case OP_JAL:
		sljit_emit_op1( C, SLJIT_MOV32, JREG( 31 ), SLJIT_IMM, (sljit_s32)next );
		break;
	default:
		jit_emit_condition( C, op, SLJIT_S1 );
		if( INS_OP( op ) == OP_REGIMM && ( INS_RT( op ) == RT_BLTZAL || INS_RT( op ) == RT_BGEZAL ) )
		{
			/* written whether taken or not */
			sljit_emit_op1( C, SLJIT_MOV32, JREG( 31 ), SLJIT_IMM, (sljit_s32)next );
		}
		break;
	}

	if( INS_OP( op ) == OP_SPECIAL )
	{
		/* always taken */
		jit_emit_slot( ctx, slot, &s, dload != 0 );
		if( dload )
		{
			sljit_emit_op1( C, SLJIT_MOV32, JDELAYV, SLJIT_R0, 0 );
			sljit_emit_op1( C, SLJIT_MOV32, JDELAYR, SLJIT_IMM, dload );
		}
		jit_emit_leave( ctx, index + 2, SLJIT_S1, 0, dload ? JIT_LEAVE_OUT : JIT_LEAVE_PLAIN );
	}
	else if( !jit_branch_is_conditional( op ) )
	{
		jit_emit_slot( ctx, slot, &s, dload != 0 );
		if( dload )
		{
			sljit_emit_op1( C, SLJIT_MOV32, JDELAYV, SLJIT_R0, 0 );
			sljit_emit_op1( C, SLJIT_MOV32, JDELAYR, SLJIT_IMM, dload );
			jit_emit_leave( ctx, index + 2, SLJIT_IMM, (sljit_sw)target, JIT_LEAVE_OUT );
		}
		else
		{
			jit_emit_leave( ctx, index + 2, SLJIT_IMM, (sljit_sw)target, index );
		}
	}
	else
	{
		struct sljit_jump *fallthrough;
		jit_emit_slot( ctx, slot, &s, dload != 0 );
		if( dload )
		{
			/* not taken: the load completes at once; taken: it stays pending */
			struct sljit_jump *taken = sljit_emit_cmp( C, SLJIT_NOT_EQUAL, SLJIT_S1, 0, SLJIT_IMM, 0 );
			sljit_emit_op1( C, SLJIT_MOV32, JREG( dload ), SLJIT_R0, 0 );
			fallthrough = sljit_emit_jump( C, SLJIT_JUMP );
			sljit_set_label( taken, sljit_emit_label( C ) );
			sljit_emit_op1( C, SLJIT_MOV32, JDELAYV, SLJIT_R0, 0 );
			sljit_emit_op1( C, SLJIT_MOV32, JDELAYR, SLJIT_IMM, dload );
			jit_emit_leave( ctx, index + 2, SLJIT_IMM, (sljit_sw)target, JIT_LEAVE_OUT );
		}
		else
		{
			fallthrough = sljit_emit_cmp( C, SLJIT_EQUAL, SLJIT_S1, 0, SLJIT_IMM, 0 );
			jit_emit_leave( ctx, index + 2, SLJIT_IMM, (sljit_sw)target, index );
		}
		sljit_set_label( fallthrough, sljit_emit_label( C ) );
	}
}

/* a plain instruction at 'index' */
static void jit_emit_one( struct jit_ctx *ctx, UINT32 op, UINT32 index )
{
	struct jit_op o;
	jit_decode( op, &o );

	if( o.cop2 )
	{
		jit_check_cop2( ctx, index );
	}
	switch( o.kind )
	{
	case JIT_MEM:
		jit_emit_mem_address( ctx, op, &o, index );
		jit_emit_mem_access( ctx->C, op, SLJIT_R1, 0 );
		break;
	case JIT_HELPER_MEM:
		jit_emit_mem_address( ctx, op, &o, index );
#ifdef PSXJIT_VERIFY
		if( o.store ) jit_emit_verify_log( ctx->C, SLJIT_R1, 4 );
#endif
		jit_emit_helper_op( ctx->C, op, SLJIT_R1 );
		break;
	case JIT_HELPER:
		jit_emit_helper_op( ctx->C, op, 0 );
		break;
	default:
		jit_emit_simple( ctx, op, index );
		break;
	}
}

/*
	Is the loop from instruction t to the branch at 'index' (with its
	slot) a polling loop, where every pass leaves the same state? Only
	register ALU work and loads, no other branch, and no register read
	before the pass writes it if the pass writes it at all.
*/
static int jit_is_idle_loop( const UINT32 *ops, UINT32 t, UINT32 index )
{
	UINT32 written = 0, all = 0, i;
	struct jit_op o;

	if( t > index )
	{
		return 0;
	}
	for( i = t; i <= index + 1; i++ )
	{
		jit_decode( ops[ i ], &o );
		if( i != index && o.kind != JIT_SIMPLE && !( o.kind == JIT_MEM && !o.store ) )
		{
			return 0;
		}
		if( o.hilo || o.trap )
		{
			return 0;
		}
		all |= o.writes;
	}
	for( i = t; i <= index + 1; i++ )
	{
		jit_decode( ops[ i ], &o );
		if( o.reads & all & ~written & ~1 )
		{
			return 0;
		}
		written |= o.writes;
	}
	return 1;
}


/*************************************
 *  Block translation
 *************************************/

static void jit_compile( struct jit_block *block, UINT32 pc )
{
	static struct jit_ctx ctx;
	static UINT8 kind[ JIT_MAX_BLOCK ];		/* 0 one, 1 branch */
	static UINT8 loopskip[ JIT_MAX_BLOCK ];	/* instruction n is the head of a wait loop */
	static UINT32 ops[ JIT_MAX_BLOCK + 1 ];
	struct psx_fastram *region;
	struct sljit_compiler *C;
	struct sljit_label *entry;
	const UINT8 *host;
	UINT32 count = 0, maxcount, i;
	int ends = 0;

	block->pc = pc;
	block->len = 0;
	block->valid = 1;
	block->code = NULL;
	block->entry = NULL;
	block->ramop = 0;

	/* code must sit in one fast RAM region */
	region = find_fastram( pc, 4 );
	if( !region || ( pc & 3 ) )
	{
		return;
	}
	host = region->base + ( pc - region->start );
	block->op0 = *(const UINT32 *)(const void *)host;
	block->ramop = 1;
	maxcount = ( region->end - pc + 1 ) / 4;
	if( maxcount > JIT_MAX_BLOCK )
	{
		maxcount = JIT_MAX_BLOCK;
	}
	for( i = 0; i < maxcount; i++ )
	{
		ops[ i ] = *(const UINT32 *)( host + i * 4 );
	}

	memset( kind, 0, sizeof( kind ) );

	/* walk the straight path: conditional branches fall through, */
	/* jumps end the block */
	while( count < maxcount && !ends )
	{
		struct jit_op o;
		jit_decode( ops[ count ], &o );
		if( o.kind == JIT_NONE )
		{
			break;
		}
		if( o.kind != JIT_BRANCH )
		{
			kind[ count++ ] = 0;
			continue;
		}
		if( count + 2 > maxcount || !jit_slot_ok( ops[ count ], ops[ count + 1 ] ) )
		{
			break;
		}
		kind[ count ] = 1;
		if( !jit_branch_is_conditional( ops[ count ] ) )
		{
			ends = 1;
		}
		count += 2;
	}
	if( count == 0 )
	{
		return;
	}

	/* jump targets (instruction starts, never a delay slot) and polling loops */
	memset( &ctx.target, 0, sizeof( ctx.target ) );
	memset( &ctx.idle, 0, sizeof( ctx.idle ) );
	memset( &ctx.label, 0, sizeof( ctx.label ) );
	ctx.pc = pc;
	ctx.count = count;
	ctx.target[ 0 ] = 1;
	for( i = 0; i < count; i++ )
	{
		if( kind[ i ] )
		{
			UINT32 op = ops[ i ];
			if( INS_OP( op ) != OP_SPECIAL )
			{
				int t = jit_target_index( &ctx, jit_branch_target( op, pc + i * 4 ) );
				if( t >= 0 )
				{
					UINT32 k;
					int inslot = 0;
					for( k = 0; k < count; k++ )
					{
						if( kind[ k ] && (UINT32)t == k + 1 )
						{
							inslot = 1;
						}
					}
					if( !inslot )
					{
						ctx.target[ t ] = 1;
						if( jit_is_idle_loop( ops, (UINT32)t, i ) )
						{
							ctx.idle[ i ] = 1;
						}
					}
				}
			}
			i++;
		}
	}

	/* wait loop heads: block start or jump targets, not right after a load */
	memset( loopskip, 0, sizeof( loopskip ) );
	for( i = 0; i < count; i++ )
	{
		if( ctx.target[ i ] && loop_kind( pc + i * 4 ) != LOOP_NONE )
		{
			struct jit_op prev;
			if( i > 0 )
			{
				jit_decode( ops[ i - 1 ], &prev );
				if( prev.kind == JIT_MEM || prev.kind == JIT_HELPER_MEM )
				{
					continue;
				}
			}
			loopskip[ i ] = 1;
		}
	}

	C = sljit_create_compiler( NULL );
	if( !C )
	{
		return;
	}
	ctx.C = C;
	ctx.acct = 0;
	ctx.nexits = 0;
	ctx.overflow = 0;
	ctx.npending = 0;
#ifdef PSXJIT_VERIFY
	sljit_emit_enter( C, 0, SLJIT_ARGS0( 32 ), 4, 4, 0 );
#else
	sljit_emit_enter( C, 0, SLJIT_ARGS0( 32 ), 4, 3, 0 );
#endif
	sljit_emit_op1( C, SLJIT_MOV_P, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&mipscpu );
	entry = sljit_emit_label( C );

	for( i = 0; i < count; i++ )
	{
		/* a jump target: take the cycles so far, then the label */
		if( ctx.target[ i ] )
		{
			jit_emit_cycles_to( &ctx, i );
			ctx.acct = i;
			ctx.label[ i ] = sljit_emit_label( C );

			/* the head of a wait loop (psxloop.c): skip the passes that fit, */
			/* then go on only if the rest of the block still fits */
			if( loopskip[ i ] )
			{
				sljit_emit_op1( C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)( pc + i * 4 ) );
				sljit_emit_icall( C, SLJIT_CALL, SLJIT_ARGS1V( 32 ), SLJIT_IMM, SLJIT_FUNC_ADDR( loop_skip_helper ) );
				jit_add_exit( &ctx, sljit_emit_cmp( C, SLJIT_SIG_LESS | SLJIT_32,
					SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_IMM, count - i ), i );
			}
		}

		if( kind[ i ] )
		{
			jit_emit_branch( &ctx, ops[ i ], ops[ i + 1 ], i );
			i++;
		}
		else
		{
			jit_emit_one( &ctx, ops[ i ], i );
		}
	}

	/* straight off the end */
	if( !ends )
	{
		jit_emit_leave( &ctx, count, SLJIT_IMM, (sljit_sw)( pc + count * 4 ), JIT_LEAVE_PLAIN );
	}

	/* forward jumps inside the block */
	for( i = 0; i < (UINT32)ctx.npending; i++ )
	{
		sljit_set_label( ctx.pending[ i ].jump, ctx.label[ ctx.pending[ i ].index ] );
	}

	/* side exits: resume at that instruction with the interpreter */
	for( i = 0; i < (UINT32)ctx.nexits; i++ )
	{
		sljit_set_label( ctx.exits[ i ].jump, sljit_emit_label( C ) );
		if( ctx.exits[ i ].cycles )
		{
			sljit_emit_op2( C, SLJIT_SUB32, SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_MEM0(), (sljit_sw)&mips_ICount, SLJIT_IMM, ctx.exits[ i ].cycles );
		}
		sljit_emit_return( C, SLJIT_MOV32, SLJIT_IMM, (sljit_sw)( pc + ctx.exits[ i ].index * 4 ) );
	}

	block->len = count;
	block->stores = 0;
	for( i = 0; i < count; i++ )
	{
		struct jit_op o;
		jit_decode( ops[ i ], &o );
		if( o.store )
		{
			block->stores = 1;
		}
	}
	if( ctx.overflow )
	{
		/* a jump without a label must not reach sljit_generate_code */
		sljit_free_compiler( C );
		block->len = 0;
		return;
	}

	/* bounded code memory: start again once the budget is used */
	if( jit_code_bytes > JIT_CODE_BUDGET )
	{
		jit_flush();
		block->pc = pc;
		block->valid = 1;
	}
	block->code = sljit_generate_code( C, 0, NULL );
	if( block->code )
	{
		sljit_uw n_entry = sljit_get_label_addr( entry );
		block->entry = (void *)n_entry;
		jit_code_bytes += sljit_get_generated_code_size( C );
	}
	sljit_free_compiler( C );
	if( !block->code )
	{
		if( !jit_nomem_logged )
		{
			jit_nomem_logged = 1;
			log_cb( RETRO_LOG_WARN, LOGPRE "PSX recompiler: no executable memory, using the interpreter\n" );
		}
		block->len = 0;
		return;
	}

	/* remember where translated code lives */
	if( !region->readonly )
	{
		for( i = 0; i < count; i++ )
		{
			jit_codemap[ jit_codemap_index( host + i * 4 ) ] = 1;
		}
		jit_code_present = 1;
	}
}

/*
	The block at pc, or NULL if it cannot be translated. Near the end of
	a timeslice the interpreter steps through the last instructions one
	by one: a block is only translated when a whole one could run, so
	those steps do not leave blocks starting at every instruction.
*/
/*
	A block that could not be translated is retried once the code at its
	pc changes (code loaded there later, without a flush).
*/
static INLINE int jit_untranslatable_changed( const struct jit_block *block )
{
	struct psx_fastram *region;

	if( block->len != 0 || !block->ramop )
	{
		return 0;
	}
	region = find_fastram( block->pc, 4 );
	return region && *(const UINT32 *)(const void *)( region->base + ( block->pc - region->start ) ) != block->op0;
}

static INLINE struct jit_block *jit_lookup( UINT32 pc )
{
	struct jit_block *block = &jit_table[ jit_table_index( pc ) ];

	if( !block->valid || block->pc != pc || jit_untranslatable_changed( block ) )
	{
		if( mips_ICount < JIT_MAX_BLOCK )
		{
			return NULL;
		}
		/* the granules of a replaced block stay marked in the codemap (a byte */
		/* per granule, possibly shared): a later write there flushes more */
		/* than needed, never less */
		if( block->code )
		{
			sljit_free_code( block->code, NULL );
		}
		jit_compile( block, pc );
	}
	return block->len ? block : NULL;
}

#ifdef PSXJIT_VERIFY
/*
	Debug: every block also runs through the interpreter from the same
	state and the results are compared (registers, GTE, and every word
	either of them stored).
*/
static void jit_verify_block( struct jit_block *block, int before )
{
	static mips_cpu_context ctx_save, ctx_jit;
	static UINT32 jit_new[ PSXJIT_VLOG ];
	static unsigned long blocks;
	int n, i, j, bad = 0;

	ctx_save = mipscpu;
	jit_vlog_count = 0;
	mipscpu.pc = ( (jit_func)block->code )();
	n = before - mips_ICount;
	ctx_jit = mipscpu;
	/* the JIT's stores: keep the results, undo them (in reverse) */
	for( i = 0; i < jit_vlog_count; i++ )
	{
		memcpy( &jit_new[ i ], jit_vlog[ i ].p, jit_vlog[ i ].size );
	}
	for( i = jit_vlog_count - 1; i >= 0; i-- )
	{
		memcpy( jit_vlog[ i ].p, &jit_vlog[ i ].old, jit_vlog[ i ].size );
	}
	/* the interpreter, logging its stores */
	mipscpu = ctx_save;
	interp_vlog_count = 0;
	jit_verify_logging = 1;
	for( i = 0; i < n; i++ )
	{
		mipscpu.op = cpu_readop32( mipscpu.pc );
		mips_step();
	}
	jit_verify_logging = 0;
	mips_ICount = before - n;
	if( mipscpu.pc != ctx_jit.pc || mipscpu.hi != ctx_jit.hi || mipscpu.lo != ctx_jit.lo ||
		mipscpu.delayr != ctx_jit.delayr || ( mipscpu.delayr && mipscpu.delayv != ctx_jit.delayv ) ||
		memcmp( mipscpu.r, ctx_jit.r, sizeof( mipscpu.r ) ) || memcmp( mipscpu.cp2dr, ctx_jit.cp2dr, sizeof( mipscpu.cp2dr ) ) ||
		memcmp( mipscpu.cp2cr, ctx_jit.cp2cr, sizeof( mipscpu.cp2cr ) ) || mipscpu.cp0r[ CP0_SR ] != ctx_jit.cp0r[ CP0_SR ] )
	{
		bad = 1;
	}
	/* memory: what the JIT stored must be what is there now, and */
	/* whatever the interpreter changed the JIT must have stored too */
	for( i = 0; i < jit_vlog_count; i++ )
	{
		if( memcmp( &jit_new[ i ], jit_vlog[ i ].p, jit_vlog[ i ].size ) )
		{
			bad |= 2;
		}
	}
	for( i = 0; i < interp_vlog_count; i++ )
	{
		int found = 0;
		for( j = 0; j < jit_vlog_count; j++ )
		{
			if( jit_vlog[ j ].p <= interp_vlog[ i ].p && interp_vlog[ i ].p + interp_vlog[ i ].size <= jit_vlog[ j ].p + jit_vlog[ j ].size )
			{
				found = 1;
			}
		}
		if( !found && memcmp( &interp_vlog[ i ].old, interp_vlog[ i ].p, interp_vlog[ i ].size ) )
		{
			bad |= 4;
		}
	}
	blocks++;
	if( jit_vlog_count >= PSXJIT_VLOG || interp_vlog_count >= PSXJIT_VLOG )
	{
		fprintf( stderr, "JITVERIFY log full at block %08x, not checked\n", ctx_save.pc );
		bad = 0;
	}
	if( bad )
	{
		fprintf( stderr, "JITVERIFY mismatch %d at block %08x (%u instr), ran %d, block #%lu\n", bad, ctx_save.pc, block->len, n, blocks );
		fprintf( stderr, "JITVERIFY pc i=%08x j=%08x hi %08x/%08x lo %08x/%08x delayr %u/%u delayv %08x/%08x\n",
			mipscpu.pc, ctx_jit.pc, mipscpu.hi, ctx_jit.hi, mipscpu.lo, ctx_jit.lo, mipscpu.delayr, ctx_jit.delayr, mipscpu.delayv, ctx_jit.delayv );
		for( i = 0; i < 32; i++ )
		{
			if( mipscpu.r[ i ] != ctx_jit.r[ i ] || ctx_save.r[ i ] != mipscpu.r[ i ] )
			{
				fprintf( stderr, "JITVERIFY r%-2d before %08x interp %08x jit %08x%s\n", i, ctx_save.r[ i ], mipscpu.r[ i ], ctx_jit.r[ i ], mipscpu.r[ i ] != ctx_jit.r[ i ] ? "  <<<" : "" );
			}
		}
		for( i = 0; i < 32; i++ )
		{
			if( mipscpu.cp2dr[ i ].d != ctx_jit.cp2dr[ i ].d ) fprintf( stderr, "JITVERIFY cp2dr%d interp %08x jit %08x\n", i, mipscpu.cp2dr[ i ].d, ctx_jit.cp2dr[ i ].d );
			if( mipscpu.cp2cr[ i ].d != ctx_jit.cp2cr[ i ].d ) fprintf( stderr, "JITVERIFY cp2cr%d interp %08x jit %08x\n", i, mipscpu.cp2cr[ i ].d, ctx_jit.cp2cr[ i ].d );
		}
		for( i = 0; i < jit_vlog_count; i++ )
		{
			if( memcmp( &jit_new[ i ], jit_vlog[ i ].p, jit_vlog[ i ].size ) ) fprintf( stderr, "JITVERIFY jit store %p size %d differs\n", (void *)jit_vlog[ i ].p, jit_vlog[ i ].size );
		}
		for( i = 0; i < interp_vlog_count; i++ )
		{
			
		}
		for( i = 0; i < (int)block->len; i++ )
		{
			fprintf( stderr, "JITVERIFY op %08x %08x\n", ctx_save.pc + i * 4, cpu_readop32( ctx_save.pc + i * 4 ) );
		}
		exit( 1 );
	}
}
#endif

/*
	Runs the block at pc if there is one and it fits in the timeslice:
	returns 1 when it ran at least one instruction (mipscpu.pc and
	mips_ICount updated).
*/
static INLINE int jit_run( void )
{
	struct jit_block *block;
	int before;

	if( mipscpu.delayr != 0 || ( mipscpu.cp0r[ CP0_SR ] & ( SR_ISC | SR_KUC ) ) != 0 )
	{
		return 0;
	}
	block = jit_lookup( mipscpu.pc );
	if( !block || (int)block->len > mips_ICount )
	{
		return 0;
	}
	before = mips_ICount;
#ifdef PSXJIT_VERIFY
	jit_verify_block( block, before );
#else
	mipscpu.pc = ( (jit_func)block->code )();
#endif
	if( mips_ICount == before )
	{
		/* left before its first instruction: the interpreter runs it */
		return 0;
	}
	loop_pure_armed = 0;	/* the interpreter's loop tracking (psxloop.c) saw none of this */
	change_pc32ledw( mipscpu.pc );
	return 1;
}

static void jit_reset_flush( void )
{
	jit_flush();
}

#else	/* !PSXJIT_SUPPORTED */

static void jit_reset_flush( void ) { }

static const int jit_enabled = 0;
static INLINE void psx_jit_check_write( const void *host ) { }
static void jit_init( void ) { }
static void jit_exit( void ) { }
static INLINE int jit_run( void ) { return 0; }

void psxcpu_ram_written( const void *host, UINT32 bytes )
{
}

#endif	/* PSXJIT_SUPPORTED */


int psxcpu_jit_available( void )
{
	return PSXJIT_SUPPORTED;
}

void psxcpu_set_jit( int enable )
{
#if PSXJIT_SUPPORTED
	if( enable && !jit_table )
	{
		jit_enabled = 1;
		jit_init();
		/* a loaded state rewrites RAM behind our back */
		if( jit_enabled )
		{
			state_save_register_func_postload( jit_flush );
		}
	}
	else if( !enable && jit_table )
	{
		jit_exit();
	}
#endif
}
