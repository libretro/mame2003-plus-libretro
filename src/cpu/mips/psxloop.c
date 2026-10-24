/***************************************************************************

    psxloop.c

    Exact skip of the PSX CPU's wait loops (included by psx_cpu.c).

    Within a timeslice nothing but the CPU itself changes main RAM or the
    CPU state: interrupts, timers, DMA and the other CPUs only act between
    timeslices. A loop that waits for an interrupt therefore repeats the
    same work until the timeslice ends, and the whole passes that still fit
    can be done at once: registers, memory and cycles end up exactly as if
    they had run (one cycle per instruction, as the interpreter counts),
    and the last pass runs normally. Three kinds of loops are recognised:

    LOOP_PURE   a short loop of loads and register arithmetic, no stores, no
                other branch, where no register is read before the pass
                writes it: every pass leaves the same state (for example
                "lhu v0,x(v1); nop; beqz v0,loop"). The recompiler has its
                own version of this one (jit_is_idle_loop).
    LOOP_COUNT  "b<cond> r,self" with "addiu r,r,-1" in the delay slot: a
                plain countdown.
    LOOP_VSYNC  the wait in the PsyQ libraries' VSync(): a timeout counter on
                the stack, decremented and stored every pass, and a RAM word
                compared with a target (used by most System 11/12 games).
    LOOP_RNG    the VBLANK wait of Tekken 3 / Tekken Tag Tournament: every pass
                steps two random number generators in RAM (x = 5x + 1 and
                x = (x + 1) * 69069) while a flag in RAM stays 0.

    The loop's code is checked word by word every time before a skip, so
    a changed program is never skipped by mistake. Main RAM and the BIOS
    must be fast RAM (psxcpu_add_fastram) for any of this to apply; loads
    from anything else (I/O, shared RAM) are never skipped over.

***************************************************************************/

#define LOOP_PURE		1
#define LOOP_COUNT		2
#define LOOP_VSYNC		3
#define LOOP_RNG		4

/* build with -DPSX_LOOPSKIP=0 to compare against the plain interpreter */
#ifndef PSX_LOOPSKIP
#define PSX_LOOPSKIP	1
#endif
static int loopskip_enabled = PSX_LOOPSKIP;

/* a word of code or data in fast RAM, or 0 if it is not there */
static int loop_word( UINT32 address, UINT32 *word )
{
	struct psx_fastram *r = find_fastram( address, 4 );
	if( r == NULL || ( address & 3 ) != 0 )
	{
		return 0;
	}
	*word = *(UINT32 *)&r->base[ address - r->start ];
	return 1;
}

/* host pointer of a writable RAM word away from translated code, or NULL */
static UINT32 *loop_data( UINT32 address )
{
	struct psx_fastram *r = find_fastram( address, 4 );
	UINT8 *host;
	if( r == NULL || r->readonly || ( address & 3 ) != 0 )
	{
		return NULL;
	}
	host = &r->base[ address - r->start ];
#if PSXJIT_SUPPORTED
	if( jit_code_present && jit_codemap[ jit_codemap_index( host ) ] )
	{
		return NULL;
	}
#endif
	return (UINT32 *)host;
}

static UINT32 loop_branch_target( UINT32 op, UINT32 pc )
{
	return pc + 4 + ( (INT32)(INT16)op << 2 );
}


/*************************************
 *  LOOP_PURE
 *************************************/

/* registers read and written by an allowed loop instruction; 0 if not allowed */
static int loop_pure_op( UINT32 op, UINT32 *reads, UINT32 *writes, int *load )
{
	UINT32 rs = INS_RS( op ), rt = INS_RT( op ), rd = INS_RD( op );

	*reads = 0; *writes = 0; *load = 0;
	switch( INS_OP( op ) )
	{
	case 0x00:	/* SPECIAL */
		switch( op & 63 )
		{
		case 0x00: case 0x02: case 0x03:						/* SLL SRL SRA */
			*reads = 1 << rt; *writes = 1 << rd; return 1;
		case 0x04: case 0x06: case 0x07:						/* SLLV SRLV SRAV */
		case 0x21: case 0x23: case 0x24: case 0x25:				/* ADDU SUBU AND OR */
		case 0x26: case 0x27: case 0x2a: case 0x2b:				/* XOR NOR SLT SLTU */
			*reads = ( 1 << rs ) | ( 1 << rt ); *writes = 1 << rd; return 1;
		}
		return 0;
	case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e:	/* ADDIU SLTI SLTIU ANDI ORI XORI */
		*reads = 1 << rs; *writes = 1 << rt; return 1;
	case 0x0f:	/* LUI */
		*writes = 1 << rt; return 1;
	case 0x20: case 0x21: case 0x23: case 0x24: case 0x25:			/* LB LH LW LBU LHU */
		*reads = 1 << rs; *writes = 1 << rt; *load = 1; return 1;
	}
	return 0;
}

/* registers read by the closing branch, or -1 if 'op' is not a usable branch to 'target' */
static int loop_pure_branch( UINT32 op, UINT32 pc, UINT32 target, UINT32 *reads )
{
	UINT32 rs = INS_RS( op ), rt = INS_RT( op );
	switch( INS_OP( op ) )
	{
	case 0x01:	/* BLTZ BGEZ */
		if( rt != 0x00 && rt != 0x01 ) return -1;
		*reads = 1 << rs;
		break;
	case 0x04: case 0x05:	/* BEQ BNE */
		*reads = ( 1 << rs ) | ( 1 << rt );
		break;
	case 0x06: case 0x07:	/* BLEZ BGTZ */
		if( rt != 0 ) return -1;
		*reads = 1 << rs;
		break;
	default:
		return -1;
	}
	return loop_branch_target( op, pc ) == target ? 0 : -1;
}

/*
	The loop at 'l' as LOOP_PURE: the branch closing it in *branch, or 0.
	Same rules as the recompiler's jit_is_idle_loop, plus no instruction
	may use a register loaded by the one before it (the load delay would
	make it see the previous pass's value).
*/
static int loop_is_pure( UINT32 l, UINT32 *branch )
{
	UINT32 ops[ 18 ], reads[ 18 ], writes[ 18 ], all = 0, written = 0, b;
	int load[ 18 ], n, i, found = -1;

	for( n = 0; n < 16; n++ )
	{
		UINT32 r;
		if( !loop_word( l + n * 4, &ops[ n ] ) )
		{
			return 0;
		}
		if( loop_pure_branch( ops[ n ], l + n * 4, l, &r ) == 0 )
		{
			found = n;
			break;
		}
	}
	if( found < 0 || !loop_word( l + ( found + 1 ) * 4, &ops[ found + 1 ] ) )
	{
		return 0;
	}
	b = l + found * 4;
	for( i = 0; i <= found + 1; i++ )
	{
		if( i == found )
		{
			loop_pure_branch( ops[ i ], b, l, &reads[ i ] );
			writes[ i ] = 0;
			load[ i ] = 0;
		}
		else if( !loop_pure_op( ops[ i ], &reads[ i ], &writes[ i ], &load[ i ] ) )
		{
			return 0;
		}
		all |= writes[ i ];
	}
	for( i = 0; i <= found + 1; i++ )
	{
		if( reads[ i ] & all & ~written & ~1 )
		{
			return 0;
		}
		if( i > 0 && load[ i - 1 ] && ( reads[ i ] & writes[ i - 1 ] & ~1 ) )
		{
			return 0;
		}
		written |= writes[ i ];
	}
	/* the slot's load must not reach into the next pass */
	if( load[ found + 1 ] )
	{
		return 0;
	}
	*branch = b;
	return 1;
}


/*************************************
 *  LOOP_COUNT
 *************************************/

/* "b<cond> r,self" + "addiu r,r,-1": the register, or -1 */
static int loop_is_count( UINT32 l )
{
	UINT32 op, slot, r;
	if( !loop_word( l, &op ) || !loop_word( l + 4, &slot ) )
	{
		return -1;
	}
	if( loop_branch_target( op, l ) != l )
	{
		return -1;
	}
	r = INS_RS( op );
	if( r == 0 || slot != ( 0x24000000 | ( r << 21 ) | ( r << 16 ) | 0xffff ) )
	{
		return -1;
	}
	if( ( INS_OP( op ) == 0x01 && INS_RT( op ) == 0x01 ) ||		/* BGEZ */
		( INS_OP( op ) == 0x07 && INS_RT( op ) == 0x00 ) ||		/* BGTZ */
		( INS_OP( op ) == 0x05 && INS_RT( op ) == 0x00 ) )		/* BNE r,zero */
	{
		return (int)r;
	}
	return -1;
}


/*************************************
 *  LOOP_VSYNC
 *************************************/

/*
	L+0  lw   v0,16(sp)          X+0  lui  v0,hi
	L+1  nop                     X+1  lw   v0,lo(v0)
	L+2  addiu v0,v0,-1          X+2  nop
	L+3  sw   v0,16(sp)          X+3  slt  v0,v0,a0
	L+4  lw   v0,16(sp)          X+4  bnez v0,L
	L+5  nop                     X+5  nop
	L+6  bne  v0,v1,X
	L+7  nop
	One pass is 14 instructions; the RAM word at hi:lo is in *global.
*/
static int loop_is_vsync( UINT32 l, UINT32 *global )
{
	static const UINT32 head[ 8 ] = { 0x8fa20010, 0, 0x2442ffff, 0xafa20010, 0x8fa20010, 0, 0, 0 };
	UINT32 w[ 8 ], x, lui, lw, i;

	for( i = 0; i < 8; i++ )
	{
		if( !loop_word( l + i * 4, &w[ i ] ) || ( i != 6 && w[ i ] != head[ i ] ) )
		{
			return 0;
		}
	}
	if( ( w[ 6 ] & 0xffff0000 ) != 0x14430000 )
	{
		return 0;
	}
	x = loop_branch_target( w[ 6 ], l + 24 );
	if( x <= l + 28 || x - l > 64 * 4 )
	{
		return 0;
	}
	if( !loop_word( x, &lui ) || ( lui & 0xffff0000 ) != 0x3c020000 ||
		!loop_word( x + 4, &lw ) || ( lw & 0xffff0000 ) != 0x8c420000 ||
		!loop_word( x + 8, &w[ 0 ] ) || w[ 0 ] != 0 ||
		!loop_word( x + 12, &w[ 1 ] ) || w[ 1 ] != 0x0044102a ||
		!loop_word( x + 16, &w[ 2 ] ) || ( w[ 2 ] & 0xffff0000 ) != 0x14400000 || loop_branch_target( w[ 2 ], x + 16 ) != l ||
		!loop_word( x + 20, &w[ 3 ] ) || w[ 3 ] != 0 )
	{
		return 0;
	}
	*global = ( ( lui & 0xffff ) << 16 ) + (INT32)(INT16)lw;
	return 1;
}


/*************************************
 *  LOOP_RNG
 *************************************/

/*
	L+0   jal  F                 F+0  lw   v1,o1(gp)
	L+1   nop                    F+1  nop
	L+2   lui  v1,h2             F+2  sll  v0,v1,2
	L+3   lw   v1,o2(v1)         F+3  addu v0,v0,v1
	L+4   lw   a0,o3(gp)         F+4  addiu v0,v0,1
	L+5   addiu v1,v1,1          F+5  sw   v0,o1(gp)
	L+6.. v0 = v1 * 69069        F+6  jr   ra
	L+16  lui  at,h2             F+7  nop
	L+17  sw   v0,o2(at)
	L+18  beqz a0,L
	L+19  lui  v0,slot
	One pass is 28 instructions.
*/
static const UINT32 loop_rng_mul[ 10 ] =
{
	0x00031140, 0x00431021, 0x00021080, 0x00431023, 0x00021080,
	0x00431023, 0x00021080, 0x00431021, 0x00021940, 0x00431021
};

struct loop_rng
{
	UINT32 x1, x2, flag;		/* addresses: generator 1 (5x + 1), generator 2, flag */
	UINT32 at, v0;				/* the values left in at and v0 */
};

static int loop_is_rng( UINT32 l, struct loop_rng *g )
{
	UINT32 w[ 20 ], f[ 8 ], fa, i, gp = mipscpu.r[ 28 ];

	for( i = 0; i < 20; i++ )
	{
		if( !loop_word( l + i * 4, &w[ i ] ) )
		{
			return 0;
		}
	}
	if( INS_OP( w[ 0 ] ) != 0x03 || w[ 1 ] != 0 ||
		( w[ 2 ] & 0xffff0000 ) != 0x3c030000 || ( w[ 3 ] & 0xffff0000 ) != 0x8c630000 ||
		( w[ 4 ] & 0xffff0000 ) != 0x8f840000 || w[ 5 ] != 0x24630001 ||
		( w[ 16 ] & 0xffff0000 ) != 0x3c010000 || ( w[ 16 ] & 0xffff ) != ( w[ 2 ] & 0xffff ) ||
		( w[ 17 ] & 0xffff0000 ) != 0xac220000 || ( w[ 17 ] & 0xffff ) != ( w[ 3 ] & 0xffff ) ||
		( w[ 18 ] & 0xffff0000 ) != 0x10800000 || loop_branch_target( w[ 18 ], l + 72 ) != l ||
		( w[ 19 ] & 0xffff0000 ) != 0x3c020000 )
	{
		return 0;
	}
	for( i = 0; i < 10; i++ )
	{
		if( w[ 6 + i ] != loop_rng_mul[ i ] )
		{
			return 0;
		}
	}
	fa = ( ( l + 4 ) & 0xf0000000 ) | ( ( w[ 0 ] & 0x03ffffff ) << 2 );
	for( i = 0; i < 8; i++ )
	{
		if( !loop_word( fa + i * 4, &f[ i ] ) )
		{
			return 0;
		}
	}
	if( ( f[ 0 ] & 0xffff0000 ) != 0x8f830000 || f[ 1 ] != 0 || f[ 2 ] != 0x00031080 || f[ 3 ] != 0x00431021 ||
		f[ 4 ] != 0x24420001 || ( f[ 5 ] & 0xffff0000 ) != 0xaf820000 || ( f[ 5 ] & 0xffff ) != ( f[ 0 ] & 0xffff ) ||
		f[ 6 ] != 0x03e00008 || f[ 7 ] != 0 )
	{
		return 0;
	}
	g->x1 = gp + (INT32)(INT16)f[ 0 ];
	g->x2 = ( ( w[ 2 ] & 0xffff ) << 16 ) + (INT32)(INT16)w[ 3 ];
	g->flag = gp + (INT32)(INT16)w[ 4 ];
	g->at = ( w[ 16 ] & 0xffff ) << 16;
	g->v0 = ( w[ 19 ] & 0xffff ) << 16;
	return 1;
}


/*************************************
 *  Skipping
 *************************************/

/* whole passes of 'pass' cycles that fit, leaving between 1 and 'pass' cycles */
static INLINE UINT32 loop_passes( UINT32 pass )
{
	if( mips_ICount <= (int)pass )
	{
		return 0;
	}
	return ( (UINT32)mips_ICount - 1 ) / pass;
}

static int loop_state_ok( void )
{
	return loopskip_enabled && mipscpu.delayr == 0 && ( mipscpu.cp0r[ CP0_SR ] & ( SR_ISC | SR_KUC ) ) == 0;
}

/* LOOP_COUNT, LOOP_VSYNC and LOOP_RNG at 'l' (the start of a pass) */
static void loop_skip_at( UINT32 l )
{
	UINT32 n, global;
	int r;
	struct loop_rng g;

	if( !loop_state_ok() )
	{
		return;
	}

	if( ( r = loop_is_count( l ) ) > 0 )
	{
		/* passes still taking the branch, counting the one starting now */
		UINT32 op = 0, v = mipscpu.r[ r ], left;
		loop_word( l, &op );
		if( INS_OP( op ) == 0x01 )			/* BGEZ: while v >= 0 */
			left = (INT32)v >= 0 ? v + 1 : 0;
		else								/* BGTZ, BNE zero: while v > 0 */
			left = (INT32)v > 0 ? v : 0;
		n = loop_passes( 2 );
		if( left < 1 )
			return;
		if( n > left - 1 )
			n = left - 1;
		if( n == 0 )
			return;
		mipscpu.r[ r ] = v - n;
		mips_ICount -= n * 2;
		return;
	}

	if( loop_is_vsync( l, &global ) )
	{
		UINT32 *counter = loop_data( mipscpu.r[ 29 ] + 16 ), value, until;
		if( counter == NULL || !loop_word( global, &value ) || global == mipscpu.r[ 29 ] + 16 )
		{
			return;
		}
		/* the RAM word must keep the loop going */
		if( !( (INT32)value < (INT32)mipscpu.r[ 4 ] ) )
		{
			return;
		}
		/* passes before the counter reaches v1 (the timeout), that pass not included */
		until = *counter - mipscpu.r[ 3 ];
		n = loop_passes( 14 );
		if( until != 0 && n > until - 1 )
		{
			n = until - 1;
		}
		if( n == 0 )
		{
			return;
		}
		*counter -= n;
		mipscpu.r[ 2 ] = 1;
		mips_ICount -= n * 14;
		return;
	}

	if( loop_is_rng( l, &g ) )
	{
		UINT32 *x1 = loop_data( g.x1 ), *x2 = loop_data( g.x2 ), flag, a, b, k, last = 0;
		if( x1 == NULL || x2 == NULL || !loop_word( g.flag, &flag ) || flag != 0 )
		{
			return;
		}
		/* the three words must be distinct, so the stores leave the flag alone */
		if( g.x1 == g.x2 || g.flag == g.x1 || g.flag == g.x2 )
		{
			return;
		}
		n = loop_passes( 28 );
		if( n == 0 )
		{
			return;
		}
		a = *x1;
		b = *x2;
		for( k = 0; k < n; k++ )
		{
			a = a * 5 + 1;
			last = b;
			b = ( b + 1 ) * 69069;
		}
		*x1 = a;
		*x2 = b;
		mipscpu.r[ 1 ] = g.at;
		mipscpu.r[ 2 ] = g.v0;
		mipscpu.r[ 3 ] = last + 1;
		mipscpu.r[ 4 ] = 0;
		mipscpu.r[ 31 ] = l + 8;
		mips_ICount -= n * 28;
		return;
	}
}

/* the kind of loop starting at 'l', for the recompiler (LOOP_PURE is not reported) */
static int loop_kind( UINT32 l )
{
	UINT32 global;
	struct loop_rng g;
	if( !loopskip_enabled )
	{
		return LOOP_NONE;
	}
	if( loop_is_count( l ) > 0 )
	{
		return LOOP_COUNT;
	}
	if( loop_is_vsync( l, &global ) )
	{
		return LOOP_VSYNC;
	}
	if( loop_is_rng( l, &g ) )
	{
		return LOOP_RNG;
	}
	return LOOP_NONE;
}


/*************************************
 *  Interpreter hook
 *************************************/

/*
	Called by the interpreter when the PC moves back to 'l' (a taken
	backward branch or jump) from the instruction at 'from'. LOOP_PURE
	needs one whole pass seen first: from the previous arrival at 'l' to
	the loop's own branch, touching nothing but fast RAM (psx_slow_access).
*/
#define LOOP_CACHE_SIZE		1024

static struct
{
	UINT32 pc, op, branch;
	UINT8 kind;
} loop_cache[ LOOP_CACHE_SIZE ];

static UINT32 loop_pure_pc;

static void loop_arrive( UINT32 l, UINT32 from )
{
	UINT32 op, idx = ( l >> 2 ) & ( LOOP_CACHE_SIZE - 1 );
	int armed = loop_pure_armed && loop_pure_pc == l && !psx_slow_access;

	loop_pure_armed = 0;
	if( !loop_word( l, &op ) )
	{
		return;
	}
	if( loop_cache[ idx ].pc != l || loop_cache[ idx ].op != op )
	{
		UINT32 branch;
		loop_cache[ idx ].pc = l;
		loop_cache[ idx ].op = op;
		loop_cache[ idx ].kind = loop_is_pure( l, &branch ) ? LOOP_PURE : LOOP_NONE;
		loop_cache[ idx ].branch = branch;
		if( loop_cache[ idx ].kind == LOOP_NONE )
		{
			/* the other kinds are told by their first words */
			UINT32 global;
			struct loop_rng g;
			if( loop_is_count( l ) > 0 || loop_is_vsync( l, &global ) || loop_is_rng( l, &g ) )
			{
				loop_cache[ idx ].kind = LOOP_COUNT;	/* "one of the checked kinds" */
			}
		}
	}

	switch( loop_cache[ idx ].kind )
	{
	case LOOP_PURE:
		{
			UINT32 branch;
			/* a whole pass ran since the last arrival: back here from the slot of the loop's own branch */
			if( armed && from == loop_cache[ idx ].branch + 4 && loop_state_ok() &&
				loop_is_pure( l, &branch ) && branch == loop_cache[ idx ].branch )
			{
				UINT32 pass = ( branch + 8 - l ) / 4;
				mips_ICount -= loop_passes( pass ) * pass;
			}
			loop_pure_pc = l;
			loop_pure_armed = 1;
			psx_slow_access = 0;
		}
		break;
	case LOOP_COUNT:
		loop_skip_at( l );
		break;
	}
}

/* recompiled code: the skip at a loop head inside a block */
#if PSXJIT_SUPPORTED
static void SLJIT_FUNC loop_skip_helper( sljit_u32 l )
{
	loop_skip_at( l );
}
#endif
