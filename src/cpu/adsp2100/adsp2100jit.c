/***************************************************************************

    adsp2100jit.c

    Optional ADSP-2100 family recompiler built on sljit (included by
    adsp2100.c). Blocks of up to 32 instructions, decoded once:

    - the common instructions in native code: data register loads and
      moves, data memory reads and writes through the DAGs (and at fixed
      addresses) when they hit RAM, the ALU/MAC/shifter operations called
      directly; the others call the interpreter's C for their class
      (adsp2100_execute_op, specialized for it);
    - DO loops counted by CNTR end in the block, and jump back to the
      body inside it when the body is there;
    - PC, PPC and the cycle count are brought up to date before any
      instruction that can see them (jumps, calls, interrupts, I/O
      handlers, control registers), exactly as the interpreter has them,
      and the block leaves after such an instruction when it changed the
      PC, used up the timeslice or wrote program memory.

    Data memory goes through a direct pointer only where the memory map
    gives RAM of this CPU; handlers stay handlers (reads the driver marks
    as free of side effects, adsp2100_jit_pure_read, are called without
    the bookkeeping). Program memory writes drop the translations over
    the word written; boot loads, reset and a loaded state drop them all.

    Off unless a driver enables it with adsp2100_set_jit(); used by
    Killer Instinct only. The interpreter is unchanged: its code is the
    same functions with fast = 0.

***************************************************************************/

#if ADSP2100_JIT_SUPPORTED

#define ADSP_JIT_MAX_BLOCK	32

static int adsp_jit_enabled;
/* adsp_jit_code (declared in adsp2100.c): translation of the block at each PC, or NULL */

/*
	Possible ends of DO loops: the addresses DO instructions in program
	memory (or the loop stack) name. Only there does a block check the
	loop address. Written program memory adds what a new DO names.
*/
static UINT8 adsp_jit_loopend[0x4000];

static void adsp_jit_scan_loops(void)
{
	int a;
	memset(adsp_jit_loopend, 0, sizeof(adsp_jit_loopend));
	for (a = 0; a < 0x4000; a++)
	{
		UINT32 op = RWORD_PGM(a);
		if ((op >> 18) == 0x05)		/* 0x14-0x17: DO */
			adsp_jit_loopend[(op >> 4) & 0x3fff] = 1;
	}
	for (a = 0; a < LOOP_STACK_DEPTH; a++)
		adsp_jit_loopend[(adsp2100.loop_stack[a] >> 4) & 0x3fff] = 1;
	adsp_jit_loopend[adsp2100.loop & 0x3fff] = 1;
}

/*
	Data memory through direct pointers: the words the memory map gives as
	RAM of this CPU (its region), so handlers (latches, control registers,
	the banked ROM, speedups installed over RAM) are still called. Built
	once the CPU runs, as the map is complete then.
*/
static int adsp_dm_built;

static void adsp_jit_build_dm(void)
{
	int cpunum = cpu_getactivecpu();
	UINT8 *base = memory_region(REGION_CPU1 + cpunum);
	size_t len = memory_region_length(REGION_CPU1 + cpunum);
	UINT32 a;

	for (a = 0; a < 0x4000; a++)
	{
		UINT8 *r = (UINT8 *)memory_get_read_ptr(cpunum, ADSP2100_DATA_OFFSET + (a << 1));
		UINT8 *w = (UINT8 *)memory_get_write_ptr(cpunum, ADSP2100_DATA_OFFSET + (a << 1));
		adsp_dm_rptr[a] = (r && base && r >= base && r + 2 <= base + len && !((size_t)r & 1)) ? (UINT16 *)r : NULL;
		adsp_dm_wptr[a] = (w && base && w >= base && w + 2 <= base + len && !((size_t)w & 1)) ? (UINT16 *)w : NULL;
	}
	adsp_dm_built = 1;
}

/* translations dropped while a block may be running: freed by the dispatcher */
#define ADSP_JIT_MAX_DEAD	64
static void *adsp_jit_dead[ADSP_JIT_MAX_DEAD];
static int adsp_jit_ndead;

static void adsp_jit_free_dead(void)
{
	while (adsp_jit_ndead > 0)
		sljit_free_code(adsp_jit_dead[--adsp_jit_ndead], NULL);
}

static void adsp_jit_flush(void)
{
	int i;
	if (!adsp_jit_code)
		return;
	adsp_jit_free_dead();
	for (i = 0; i < 0x4000; i++)
		if (adsp_jit_code[i])
		{
			sljit_free_code(adsp_jit_code[i], NULL);
			adsp_jit_code[i] = NULL;
		}
	adsp2100_pgm_dirty = 0;
	adsp_jit_flush_all = 0;
	adsp_jit_scan_loops();
}

/* a loaded state: everything goes at the next stop (the ADSP may not be the active CPU now) */
static void adsp_jit_postload(void)
{
	adsp_jit_flush_all = adsp2100_pgm_dirty = 1;
}

/*
	An instruction wrote program memory at 'addr': the blocks over that
	word are dropped (a block holds at most ADSP_JIT_MAX_BLOCK words from
	its start), and the running block leaves after the instruction.
*/
static void adsp_jit_pgm_write(UINT32 addr, UINT32 data)
{
	int s;
	addr &= 0x3fff;
	/* a new DO: its loop end needs the check, so the blocks over it go too */
	if (((data >> 18) & 0x3f) == 0x05 && !adsp_jit_loopend[(data >> 4) & 0x3fff])
	{
		adsp_jit_loopend[(data >> 4) & 0x3fff] = 1;
		adsp_jit_pgm_write((data >> 4) & 0x3fff, 0);
	}
	for (s = (int)addr - (ADSP_JIT_MAX_BLOCK - 1); s <= (int)addr; s++)
		if (s >= 0 && adsp_jit_code[s])
		{
			if (adsp_jit_ndead == ADSP_JIT_MAX_DEAD)
			{
				adsp_jit_flush_all = 1;		/* too many: drop everything at the next stop */
				adsp2100_pgm_dirty = 1;
				return;
			}
			adsp_jit_dead[adsp_jit_ndead++] = adsp_jit_code[s];
			adsp_jit_code[s] = NULL;
			adsp2100_pgm_dirty = 1;
		}
}

/* one instruction class each: adsp2100_execute_op with a constant class */
#define ADSP_JIT_OP(k)	static void adsp_jit_op_##k(sljit_u32 op) { adsp2100_execute_op(op, 0x##k, 1); }
ADSP_JIT_OP(00)
ADSP_JIT_OP(01)
ADSP_JIT_OP(02)
ADSP_JIT_OP(03)
ADSP_JIT_OP(04)
ADSP_JIT_OP(05)
ADSP_JIT_OP(06)
ADSP_JIT_OP(07)
ADSP_JIT_OP(08)
ADSP_JIT_OP(09)
ADSP_JIT_OP(0a)
ADSP_JIT_OP(0b)
ADSP_JIT_OP(0c)
ADSP_JIT_OP(0d)
ADSP_JIT_OP(0e)
ADSP_JIT_OP(0f)
ADSP_JIT_OP(10)
ADSP_JIT_OP(11)
ADSP_JIT_OP(12)
ADSP_JIT_OP(13)
ADSP_JIT_OP(14)
ADSP_JIT_OP(15)
ADSP_JIT_OP(16)
ADSP_JIT_OP(17)
ADSP_JIT_OP(18)
ADSP_JIT_OP(19)
ADSP_JIT_OP(1a)
ADSP_JIT_OP(1b)
ADSP_JIT_OP(1c)
ADSP_JIT_OP(1d)
ADSP_JIT_OP(1e)
ADSP_JIT_OP(1f)
ADSP_JIT_OP(20)
ADSP_JIT_OP(21)
ADSP_JIT_OP(22)
ADSP_JIT_OP(23)
ADSP_JIT_OP(24)
ADSP_JIT_OP(25)
ADSP_JIT_OP(26)
ADSP_JIT_OP(27)
ADSP_JIT_OP(28)
ADSP_JIT_OP(29)
ADSP_JIT_OP(2a)
ADSP_JIT_OP(2b)
ADSP_JIT_OP(2c)
ADSP_JIT_OP(2d)
ADSP_JIT_OP(2e)
ADSP_JIT_OP(2f)
ADSP_JIT_OP(30)
ADSP_JIT_OP(31)
ADSP_JIT_OP(32)
ADSP_JIT_OP(33)
ADSP_JIT_OP(34)
ADSP_JIT_OP(35)
ADSP_JIT_OP(36)
ADSP_JIT_OP(37)
ADSP_JIT_OP(38)
ADSP_JIT_OP(39)
ADSP_JIT_OP(3a)
ADSP_JIT_OP(3b)
ADSP_JIT_OP(3c)
ADSP_JIT_OP(3d)
ADSP_JIT_OP(3e)
ADSP_JIT_OP(3f)
ADSP_JIT_OP(40)
ADSP_JIT_OP(41)
ADSP_JIT_OP(42)
ADSP_JIT_OP(43)
ADSP_JIT_OP(44)
ADSP_JIT_OP(45)
ADSP_JIT_OP(46)
ADSP_JIT_OP(47)
ADSP_JIT_OP(48)
ADSP_JIT_OP(49)
ADSP_JIT_OP(4a)
ADSP_JIT_OP(4b)
ADSP_JIT_OP(4c)
ADSP_JIT_OP(4d)
ADSP_JIT_OP(4e)
ADSP_JIT_OP(4f)
ADSP_JIT_OP(50)
ADSP_JIT_OP(51)
ADSP_JIT_OP(52)
ADSP_JIT_OP(53)
ADSP_JIT_OP(54)
ADSP_JIT_OP(55)
ADSP_JIT_OP(56)
ADSP_JIT_OP(57)
ADSP_JIT_OP(58)
ADSP_JIT_OP(59)
ADSP_JIT_OP(5a)
ADSP_JIT_OP(5b)
ADSP_JIT_OP(5c)
ADSP_JIT_OP(5d)
ADSP_JIT_OP(5e)
ADSP_JIT_OP(5f)
ADSP_JIT_OP(60)
ADSP_JIT_OP(61)
ADSP_JIT_OP(62)
ADSP_JIT_OP(63)
ADSP_JIT_OP(64)
ADSP_JIT_OP(65)
ADSP_JIT_OP(66)
ADSP_JIT_OP(67)
ADSP_JIT_OP(68)
ADSP_JIT_OP(69)
ADSP_JIT_OP(6a)
ADSP_JIT_OP(6b)
ADSP_JIT_OP(6c)
ADSP_JIT_OP(6d)
ADSP_JIT_OP(6e)
ADSP_JIT_OP(6f)
ADSP_JIT_OP(70)
ADSP_JIT_OP(71)
ADSP_JIT_OP(72)
ADSP_JIT_OP(73)
ADSP_JIT_OP(74)
ADSP_JIT_OP(75)
ADSP_JIT_OP(76)
ADSP_JIT_OP(77)
ADSP_JIT_OP(78)
ADSP_JIT_OP(79)
ADSP_JIT_OP(7a)
ADSP_JIT_OP(7b)
ADSP_JIT_OP(7c)
ADSP_JIT_OP(7d)
ADSP_JIT_OP(7e)
ADSP_JIT_OP(7f)
ADSP_JIT_OP(80)
ADSP_JIT_OP(81)
ADSP_JIT_OP(82)
ADSP_JIT_OP(83)
ADSP_JIT_OP(84)
ADSP_JIT_OP(85)
ADSP_JIT_OP(86)
ADSP_JIT_OP(87)
ADSP_JIT_OP(88)
ADSP_JIT_OP(89)
ADSP_JIT_OP(8a)
ADSP_JIT_OP(8b)
ADSP_JIT_OP(8c)
ADSP_JIT_OP(8d)
ADSP_JIT_OP(8e)
ADSP_JIT_OP(8f)
ADSP_JIT_OP(90)
ADSP_JIT_OP(91)
ADSP_JIT_OP(92)
ADSP_JIT_OP(93)
ADSP_JIT_OP(94)
ADSP_JIT_OP(95)
ADSP_JIT_OP(96)
ADSP_JIT_OP(97)
ADSP_JIT_OP(98)
ADSP_JIT_OP(99)
ADSP_JIT_OP(9a)
ADSP_JIT_OP(9b)
ADSP_JIT_OP(9c)
ADSP_JIT_OP(9d)
ADSP_JIT_OP(9e)
ADSP_JIT_OP(9f)
ADSP_JIT_OP(a0)
ADSP_JIT_OP(a1)
ADSP_JIT_OP(a2)
ADSP_JIT_OP(a3)
ADSP_JIT_OP(a4)
ADSP_JIT_OP(a5)
ADSP_JIT_OP(a6)
ADSP_JIT_OP(a7)
ADSP_JIT_OP(a8)
ADSP_JIT_OP(a9)
ADSP_JIT_OP(aa)
ADSP_JIT_OP(ab)
ADSP_JIT_OP(ac)
ADSP_JIT_OP(ad)
ADSP_JIT_OP(ae)
ADSP_JIT_OP(af)
ADSP_JIT_OP(b0)
ADSP_JIT_OP(b1)
ADSP_JIT_OP(b2)
ADSP_JIT_OP(b3)
ADSP_JIT_OP(b4)
ADSP_JIT_OP(b5)
ADSP_JIT_OP(b6)
ADSP_JIT_OP(b7)
ADSP_JIT_OP(b8)
ADSP_JIT_OP(b9)
ADSP_JIT_OP(ba)
ADSP_JIT_OP(bb)
ADSP_JIT_OP(bc)
ADSP_JIT_OP(bd)
ADSP_JIT_OP(be)
ADSP_JIT_OP(bf)
ADSP_JIT_OP(c0)
ADSP_JIT_OP(c1)
ADSP_JIT_OP(c2)
ADSP_JIT_OP(c3)
ADSP_JIT_OP(c4)
ADSP_JIT_OP(c5)
ADSP_JIT_OP(c6)
ADSP_JIT_OP(c7)
ADSP_JIT_OP(c8)
ADSP_JIT_OP(c9)
ADSP_JIT_OP(ca)
ADSP_JIT_OP(cb)
ADSP_JIT_OP(cc)
ADSP_JIT_OP(cd)
ADSP_JIT_OP(ce)
ADSP_JIT_OP(cf)
ADSP_JIT_OP(d0)
ADSP_JIT_OP(d1)
ADSP_JIT_OP(d2)
ADSP_JIT_OP(d3)
ADSP_JIT_OP(d4)
ADSP_JIT_OP(d5)
ADSP_JIT_OP(d6)
ADSP_JIT_OP(d7)
ADSP_JIT_OP(d8)
ADSP_JIT_OP(d9)
ADSP_JIT_OP(da)
ADSP_JIT_OP(db)
ADSP_JIT_OP(dc)
ADSP_JIT_OP(dd)
ADSP_JIT_OP(de)
ADSP_JIT_OP(df)
ADSP_JIT_OP(e0)
ADSP_JIT_OP(e1)
ADSP_JIT_OP(e2)
ADSP_JIT_OP(e3)
ADSP_JIT_OP(e4)
ADSP_JIT_OP(e5)
ADSP_JIT_OP(e6)
ADSP_JIT_OP(e7)
ADSP_JIT_OP(e8)
ADSP_JIT_OP(e9)
ADSP_JIT_OP(ea)
ADSP_JIT_OP(eb)
ADSP_JIT_OP(ec)
ADSP_JIT_OP(ed)
ADSP_JIT_OP(ee)
ADSP_JIT_OP(ef)
ADSP_JIT_OP(f0)
ADSP_JIT_OP(f1)
ADSP_JIT_OP(f2)
ADSP_JIT_OP(f3)
ADSP_JIT_OP(f4)
ADSP_JIT_OP(f5)
ADSP_JIT_OP(f6)
ADSP_JIT_OP(f7)
ADSP_JIT_OP(f8)
ADSP_JIT_OP(f9)
ADSP_JIT_OP(fa)
ADSP_JIT_OP(fb)
ADSP_JIT_OP(fc)
ADSP_JIT_OP(fd)
ADSP_JIT_OP(fe)
ADSP_JIT_OP(ff)
#undef ADSP_JIT_OP

static void (*const adsp_jit_ops[256])(sljit_u32) =
{
	adsp_jit_op_00, adsp_jit_op_01, adsp_jit_op_02, adsp_jit_op_03, adsp_jit_op_04, adsp_jit_op_05, adsp_jit_op_06, adsp_jit_op_07,
	adsp_jit_op_08, adsp_jit_op_09, adsp_jit_op_0a, adsp_jit_op_0b, adsp_jit_op_0c, adsp_jit_op_0d, adsp_jit_op_0e, adsp_jit_op_0f,
	adsp_jit_op_10, adsp_jit_op_11, adsp_jit_op_12, adsp_jit_op_13, adsp_jit_op_14, adsp_jit_op_15, adsp_jit_op_16, adsp_jit_op_17,
	adsp_jit_op_18, adsp_jit_op_19, adsp_jit_op_1a, adsp_jit_op_1b, adsp_jit_op_1c, adsp_jit_op_1d, adsp_jit_op_1e, adsp_jit_op_1f,
	adsp_jit_op_20, adsp_jit_op_21, adsp_jit_op_22, adsp_jit_op_23, adsp_jit_op_24, adsp_jit_op_25, adsp_jit_op_26, adsp_jit_op_27,
	adsp_jit_op_28, adsp_jit_op_29, adsp_jit_op_2a, adsp_jit_op_2b, adsp_jit_op_2c, adsp_jit_op_2d, adsp_jit_op_2e, adsp_jit_op_2f,
	adsp_jit_op_30, adsp_jit_op_31, adsp_jit_op_32, adsp_jit_op_33, adsp_jit_op_34, adsp_jit_op_35, adsp_jit_op_36, adsp_jit_op_37,
	adsp_jit_op_38, adsp_jit_op_39, adsp_jit_op_3a, adsp_jit_op_3b, adsp_jit_op_3c, adsp_jit_op_3d, adsp_jit_op_3e, adsp_jit_op_3f,
	adsp_jit_op_40, adsp_jit_op_41, adsp_jit_op_42, adsp_jit_op_43, adsp_jit_op_44, adsp_jit_op_45, adsp_jit_op_46, adsp_jit_op_47,
	adsp_jit_op_48, adsp_jit_op_49, adsp_jit_op_4a, adsp_jit_op_4b, adsp_jit_op_4c, adsp_jit_op_4d, adsp_jit_op_4e, adsp_jit_op_4f,
	adsp_jit_op_50, adsp_jit_op_51, adsp_jit_op_52, adsp_jit_op_53, adsp_jit_op_54, adsp_jit_op_55, adsp_jit_op_56, adsp_jit_op_57,
	adsp_jit_op_58, adsp_jit_op_59, adsp_jit_op_5a, adsp_jit_op_5b, adsp_jit_op_5c, adsp_jit_op_5d, adsp_jit_op_5e, adsp_jit_op_5f,
	adsp_jit_op_60, adsp_jit_op_61, adsp_jit_op_62, adsp_jit_op_63, adsp_jit_op_64, adsp_jit_op_65, adsp_jit_op_66, adsp_jit_op_67,
	adsp_jit_op_68, adsp_jit_op_69, adsp_jit_op_6a, adsp_jit_op_6b, adsp_jit_op_6c, adsp_jit_op_6d, adsp_jit_op_6e, adsp_jit_op_6f,
	adsp_jit_op_70, adsp_jit_op_71, adsp_jit_op_72, adsp_jit_op_73, adsp_jit_op_74, adsp_jit_op_75, adsp_jit_op_76, adsp_jit_op_77,
	adsp_jit_op_78, adsp_jit_op_79, adsp_jit_op_7a, adsp_jit_op_7b, adsp_jit_op_7c, adsp_jit_op_7d, adsp_jit_op_7e, adsp_jit_op_7f,
	adsp_jit_op_80, adsp_jit_op_81, adsp_jit_op_82, adsp_jit_op_83, adsp_jit_op_84, adsp_jit_op_85, adsp_jit_op_86, adsp_jit_op_87,
	adsp_jit_op_88, adsp_jit_op_89, adsp_jit_op_8a, adsp_jit_op_8b, adsp_jit_op_8c, adsp_jit_op_8d, adsp_jit_op_8e, adsp_jit_op_8f,
	adsp_jit_op_90, adsp_jit_op_91, adsp_jit_op_92, adsp_jit_op_93, adsp_jit_op_94, adsp_jit_op_95, adsp_jit_op_96, adsp_jit_op_97,
	adsp_jit_op_98, adsp_jit_op_99, adsp_jit_op_9a, adsp_jit_op_9b, adsp_jit_op_9c, adsp_jit_op_9d, adsp_jit_op_9e, adsp_jit_op_9f,
	adsp_jit_op_a0, adsp_jit_op_a1, adsp_jit_op_a2, adsp_jit_op_a3, adsp_jit_op_a4, adsp_jit_op_a5, adsp_jit_op_a6, adsp_jit_op_a7,
	adsp_jit_op_a8, adsp_jit_op_a9, adsp_jit_op_aa, adsp_jit_op_ab, adsp_jit_op_ac, adsp_jit_op_ad, adsp_jit_op_ae, adsp_jit_op_af,
	adsp_jit_op_b0, adsp_jit_op_b1, adsp_jit_op_b2, adsp_jit_op_b3, adsp_jit_op_b4, adsp_jit_op_b5, adsp_jit_op_b6, adsp_jit_op_b7,
	adsp_jit_op_b8, adsp_jit_op_b9, adsp_jit_op_ba, adsp_jit_op_bb, adsp_jit_op_bc, adsp_jit_op_bd, adsp_jit_op_be, adsp_jit_op_bf,
	adsp_jit_op_c0, adsp_jit_op_c1, adsp_jit_op_c2, adsp_jit_op_c3, adsp_jit_op_c4, adsp_jit_op_c5, adsp_jit_op_c6, adsp_jit_op_c7,
	adsp_jit_op_c8, adsp_jit_op_c9, adsp_jit_op_ca, adsp_jit_op_cb, adsp_jit_op_cc, adsp_jit_op_cd, adsp_jit_op_ce, adsp_jit_op_cf,
	adsp_jit_op_d0, adsp_jit_op_d1, adsp_jit_op_d2, adsp_jit_op_d3, adsp_jit_op_d4, adsp_jit_op_d5, adsp_jit_op_d6, adsp_jit_op_d7,
	adsp_jit_op_d8, adsp_jit_op_d9, adsp_jit_op_da, adsp_jit_op_db, adsp_jit_op_dc, adsp_jit_op_dd, adsp_jit_op_de, adsp_jit_op_df,
	adsp_jit_op_e0, adsp_jit_op_e1, adsp_jit_op_e2, adsp_jit_op_e3, adsp_jit_op_e4, adsp_jit_op_e5, adsp_jit_op_e6, adsp_jit_op_e7,
	adsp_jit_op_e8, adsp_jit_op_e9, adsp_jit_op_ea, adsp_jit_op_eb, adsp_jit_op_ec, adsp_jit_op_ed, adsp_jit_op_ee, adsp_jit_op_ef,
	adsp_jit_op_f0, adsp_jit_op_f1, adsp_jit_op_f2, adsp_jit_op_f3, adsp_jit_op_f4, adsp_jit_op_f5, adsp_jit_op_f6, adsp_jit_op_f7,
	adsp_jit_op_f8, adsp_jit_op_f9, adsp_jit_op_fa, adsp_jit_op_fb, adsp_jit_op_fc, adsp_jit_op_fd, adsp_jit_op_fe, adsp_jit_op_ff
};

/* an instruction at the end of a DO loop: the interpreter's steps */
static void adsp_jit_loop_end(sljit_u32 op)
{
	adsp2100.ppc = adsp2100.pc;
	adsp2100_advance(op);
	adsp2100_execute_op(op, op >> 16, 1);
	adsp2100_icount--;
}

/*
	The end of a DO loop counted by CNTR (CE) at 'a', its body starting at
	't' (or anywhere, t = ~0): the interpreter's loop step, before the
	instruction runs. Returns 1 (back to the body, PC = its start), 0 (the
	loop is over, PC = a + 1), or -1 (another case: nothing done, the
	interpreter's code does it all).
*/
static sljit_sw adsp_jit_loop_ce(sljit_u32 a, sljit_u32 t)
{
	UINT32 top = pc_stack_top();
	if (adsp2100.loop_condition != 14 || (t != ~0U && top != t) || top == a)
		return -1;
	adsp2100.ppc = a;
	if ((INT32)--adsp2100.cntr > 0)
	{
		adsp2100.pc = top;
		return 1;
	}
	cntr_stack_pop();
	loop_stack_pop();
	pc_stack_pop_val();
	adsp2100.pc = a + 1;
	return 0;
}

/* one instruction through the interpreter */
static void adsp_jit_step(void)
{
	UINT32 op;
	adsp2100.ppc = adsp2100.pc;
	op = ROPCODE();
	adsp2100_advance(op);
	adsp2100_execute_op(op, op >> 16, 1);
	adsp2100_icount--;
}

#define AJ_MEM(field)	SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, field)

/*
	Instructions that only work on registers (ALU, MAC, shifter, data and
	DAG register moves and loads, without memory, stacks, PC or the
	interrupt and mode registers): nothing outside reads PC, PPC or the
	cycle count while they run, and they cannot change those, so the
	block runs them back to back and brings PC, PPC and the cycle count
	up to date before the next instruction that can see them (and where
	it leaves).
*/
static int adsp_jit_pure(UINT32 op)
{
	UINT32 k = op >> 16;
	if (k == 0x00 || k == 0x05 || k == 0x06 || k == 0x07 || k == 0x0e || k == 0x0f || k == 0x10)
		return 1;
	if (k >= 0x20 && k <= 0x3b)		/* ... and loads of data and DAG registers */
		return 1;
	if (k >= 0x40 && k <= 0x4f)
		return 1;
	if (k == 0x0d && ((op >> 10) & 3) <= 2 && ((op >> 8) & 3) <= 2)		/* data and DAG registers */
		return 1;
	return 0;
}

static UINT8 adsp_jit_len[0x4000];		/* instructions in the block at each PC */

/*
	Data memory instructions through a DAG: when the address register
	points at RAM (adsp_dm_rptr/wptr) the access is plain memory, and the
	instruction is as pure as the others; the block checks that before
	the call (and otherwise runs it as the interpreter, in its state).
	Returns 0 (not one), or the DAG (1, 2) with *write set.
*/
static int adsp_jit_dmop(UINT32 op, int *write)
{
	UINT32 k = op >> 16;
	*write = 0;
	if (k == 0x12 || k == 0x13)
	{
		*write = (op & 0x8000) != 0;
		return (k == 0x12) ? 1 : 2;
	}
	if (k >= 0x60 && k <= 0x67) return 1;
	if (k >= 0x68 && k <= 0x6f) { *write = 1; return 1; }
	if (k >= 0x70 && k <= 0x77) return 2;
	if (k >= 0x78 && k <= 0x7f) { *write = 1; return 2; }
	if (k >= 0xa0 && k <= 0xaf) { *write = 1; return 1; }
	if (k >= 0xb0 && k <= 0xbf) { *write = 1; return 2; }
	if (k >= 0xc0) return 1;
	return 0;
}

/* data memory at a fixed address (registers of groups 0 and 1): pure when it is RAM */
static int adsp_jit_direct_ram(UINT32 op)
{
	UINT32 k = op >> 16;
	UINT32 addr = (op >> 4) & 0x3fff;
	if (k >= 0x80 && k <= 0x8b)
		return adsp_dm_rptr[addr] != NULL;
	if (k >= 0x90 && k <= 0x9b)
		return adsp_dm_wptr[addr] != NULL;
	return 0;
}

enum { AJ_OTHER, AJ_PURE, AJ_DM };

static int adsp_jit_kind(UINT32 op)
{
	int w;
	if (adsp_jit_pure(op) || adsp_jit_direct_ram(op))
		return AJ_PURE;
	if (adsp_jit_dmop(op, &w))
		return AJ_DM;
	return AJ_OTHER;
}

/* DAG1 in bit-reversed mode (MSTAT): the address in R0 through reverse_table, as the accessors */
static void adsp_jit_emit_reverse(struct sljit_compiler *C)
{
	struct sljit_jump *plain;
	sljit_emit_op2u(C, SLJIT_AND32 | SLJIT_SET_Z, AJ_MEM(mstat), SLJIT_IMM, MSTAT_REVERSE);
	plain = sljit_emit_jump(C, SLJIT_ZERO);
	sljit_emit_op2(C, SLJIT_AND32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 0x3fff);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)reverse_table);
	sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_R2, SLJIT_R0), 1);
	sljit_set_label(plain, sljit_emit_label(C));
}

/*
	Data memory read through a handler the driver says has no side
	effects and depends on nothing but the address and its own state (a
	banked ROM): the block calls it without bringing PC and the cycle
	count up to date (adsp2100_jit_pure_read).
*/
static UINT8 adsp_dm_rpure[0x4000];

void adsp2100_jit_pure_read(UINT32 start, UINT32 end)
{
	UINT32 a;
	for (a = start; a <= end && a < 0x4000; a++)
		adsp_dm_rpure[a] = 1;
}

/* the checks that the DAG of a data memory instruction points at RAM: jumps out */
/* when not (to *purecall, when not NULL, for a read through a pure handler) */
static int adsp_jit_emit_dmcheck_p(struct sljit_compiler *C, UINT32 op, struct sljit_jump **slow, struct sljit_jump **purecall)
{
	int write, dag = adsp_jit_dmop(op, &write), n = 0;
	int ireg = ((dag == 1) ? 0 : 4) + ((op >> 2) & 3);
	sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, i) + ireg * sizeof(UINT32));
	if (dag == 1)
		adsp_jit_emit_reverse(C);
	slow[n++] = sljit_emit_cmp(C, SLJIT_GREATER, SLJIT_R0, 0, SLJIT_IMM, 0x3fff);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R1, 0, SLJIT_MEM2(write ? SLJIT_S4 : SLJIT_S3, SLJIT_R0), SLJIT_WORD_SHIFT);
	if (purecall && !write)
	{
		struct sljit_jump *ram = sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
		sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)adsp_dm_rpure);
		sljit_emit_op1(C, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM2(SLJIT_R2, SLJIT_R0), 0);
		slow[n++] = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0);
		*purecall = sljit_emit_jump(C, SLJIT_JUMP);
		sljit_set_label(ram, sljit_emit_label(C));
	}
	else
	{
		if (purecall)
			*purecall = NULL;
		slow[n++] = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
	}
	return n;
}

static int adsp_jit_emit_dmcheck(struct sljit_compiler *C, UINT32 op, struct sljit_jump **slow)
{
	return adsp_jit_emit_dmcheck_p(C, op, slow, NULL);
}

/*
	Native code for the common instructions, exactly what their C does:
	the data registers (group 0) read and written in place, the DAG
	registers and the ALU/MAC/shifter operations through their C
	functions (called directly), RAM through the pointer the check found.
*/
static const sljit_sw adsp_jit_dreg_off[16] =
{
	offsetof(adsp2100_Regs, core.ax0), offsetof(adsp2100_Regs, core.ax1), offsetof(adsp2100_Regs, core.mx0), offsetof(adsp2100_Regs, core.mx1),
	offsetof(adsp2100_Regs, core.ay0), offsetof(adsp2100_Regs, core.ay1), offsetof(adsp2100_Regs, core.my0), offsetof(adsp2100_Regs, core.my1),
	offsetof(adsp2100_Regs, core.si), offsetof(adsp2100_Regs, core.se), offsetof(adsp2100_Regs, core.ar), offsetof(adsp2100_Regs, core.mr.mrx.mr0),
	offsetof(adsp2100_Regs, core.mr.mrx.mr1), offsetof(adsp2100_Regs, core.mr.mrx.mr2), offsetof(adsp2100_Regs, core.sr.srx.sr0), offsetof(adsp2100_Regs, core.sr.srx.sr1)
};

/* READ_REG(grp, reg) into 'dst' (INT32) */
static void adsp_jit_emit_rdreg(struct sljit_compiler *C, int grp, int reg, sljit_s32 dst)
{
	if (grp == 0)
		sljit_emit_op1(C, SLJIT_MOV_S16, dst, 0, SLJIT_MEM1(SLJIT_S0), adsp_jit_dreg_off[reg]);
	else
	{
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS0(32), SLJIT_IMM, SLJIT_FUNC_ADDR(rd_reg[grp][reg]));
		if (dst != SLJIT_R0)
			sljit_emit_op1(C, SLJIT_MOV32, dst, 0, SLJIT_R0, 0);
	}
}

/* WRITE_REG(grp, reg, src) (src a register other than R1-R3, or an immediate) */
static void adsp_jit_emit_wrreg(struct sljit_compiler *C, int grp, int reg, sljit_s32 src, sljit_sw srcw)
{
	if (grp != 0)
	{
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, src, srcw);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(wr_reg[grp][reg]));
		return;
	}
	switch (reg)
	{
		case 9:		/* SE: (INT8)val */
		case 13:	/* MR2: (INT8)val */
			sljit_emit_op1(C, SLJIT_MOV_S8, SLJIT_R2, 0, src, srcw);
			sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S0), adsp_jit_dreg_off[reg], SLJIT_R2, 0);
			break;
		case 12:	/* MR1, and MR2 = (INT16)val >> 15 */
			sljit_emit_op1(C, SLJIT_MOV_S16, SLJIT_R2, 0, src, srcw);
			sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S0), adsp_jit_dreg_off[12], SLJIT_R2, 0);
			sljit_emit_op2(C, SLJIT_ASHR32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 15);
			sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S0), adsp_jit_dreg_off[13], SLJIT_R2, 0);
			break;
		default:
			sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_S0), adsp_jit_dreg_off[reg], src, srcw);
			break;
	}
}

/* i[ireg] += m[mreg], kept in its circular buffer (as the DAG accessors) */
static void adsp_jit_emit_dag_step(struct sljit_compiler *C, int ireg, int mreg)
{
	struct sljit_jump *above, *done, *inside;
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, i) + ireg * 4);
	sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, m) + mreg * 4);
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, base) + ireg * 4);
	above = sljit_emit_cmp(C, SLJIT_GREATER_EQUAL | SLJIT_32, SLJIT_R2, 0, SLJIT_R3, 0);
	sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, l) + ireg * 4);
	done = sljit_emit_jump(C, SLJIT_JUMP);
	sljit_set_label(above, sljit_emit_label(C));
	sljit_emit_op2(C, SLJIT_ADD32, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, l) + ireg * 4);
	inside = sljit_emit_cmp(C, SLJIT_LESS | SLJIT_32, SLJIT_R2, 0, SLJIT_R3, 0);
	sljit_emit_op2(C, SLJIT_SUB32, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, l) + ireg * 4);
	sljit_set_label(done, sljit_emit_label(C));
	sljit_set_label(inside, sljit_emit_label(C));
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, i) + ireg * 4, SLJIT_R2, 0);
}

/* the ALU/MAC operation of classes 0x60-0x7f by its two low class bits */
static void adsp_jit_emit_aluop(struct sljit_compiler *C, UINT32 op, int which)
{
	static void (*const fn[4])(int) = { mac_op_mr, alu_op_ar, mac_op_mf, alu_op_af };
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, op);
	sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(fn[which & 3]));
}

/*
	A data memory instruction on RAM (the check's pointer in R1): native
	for the DAG classes 0x60-0x7f and 0xa0-0xbf. Returns 0 when it has no
	native form (the caller then calls its C).
*/
static int adsp_jit_emit_native_dm(struct sljit_compiler *C, UINT32 op)
{
	UINT32 k = op >> 16;
	int dag2 = (k >= 0x70 && k <= 0x7f) || (k >= 0xb0 && k <= 0xbf);
	int ireg = (dag2 ? 4 : 0) + ((op >> 2) & 3), mreg = (dag2 ? 4 : 0) + (op & 3);
	int reg = (op >> 4) & 15;

	if ((k >= 0x60 && k <= 0x67) || (k >= 0x70 && k <= 0x77))
	{
		/* ALU/MAC, then the register from RAM (read now: the operation does not touch memory) */
		sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_S5, 0, SLJIT_MEM1(SLJIT_R1), 0);
		adsp_jit_emit_aluop(C, op, (k >> 1) & 3);
		adsp_jit_emit_wrreg(C, 0, reg, SLJIT_S5, 0);
		adsp_jit_emit_dag_step(C, ireg, mreg);
		return 1;
	}
	if ((k >= 0x68 && k <= 0x6f) || (k >= 0x78 && k <= 0x7f))
	{
		/* the register to RAM, then ALU/MAC */
		adsp_jit_emit_rdreg(C, 0, reg, SLJIT_R0);
		sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
		adsp_jit_emit_dag_step(C, ireg, mreg);
		adsp_jit_emit_aluop(C, op, (k >> 1) & 3);
		return 1;
	}
	if (k >= 0xa0 && k <= 0xbf)
	{
		/* an immediate to RAM */
		sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_IMM, (op >> 4) & 0xffff);
		adsp_jit_emit_dag_step(C, ireg, mreg);
		return 1;
	}
	return 0;
}

/* a pure instruction natively; 0 when it has no native form */
static int adsp_jit_emit_native(struct sljit_compiler *C, UINT32 op)
{
	UINT32 k = op >> 16;

	if (k >= 0x40 && k <= 0x4f)			/* data register = immediate */
	{
		adsp_jit_emit_wrreg(C, 0, op & 15, SLJIT_IMM, (op >> 4) & 0xffff);
		return 1;
	}
	if (k == 0x0d)						/* register move (groups 0-2) */
	{
		adsp_jit_emit_rdreg(C, (op >> 8) & 3, op & 15, SLJIT_S5);
		adsp_jit_emit_wrreg(C, (op >> 10) & 3, (op >> 4) & 15, SLJIT_S5, 0);
		return 1;
	}
	if (k >= 0x20 && k <= 0x27 && (op & 15) == 15)		/* unconditional ALU/MAC */
	{
		static void (*const fn[4])(int) = { mac_op_mr, alu_op_ar, mac_op_mf, alu_op_af };
		sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, op);
		sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(fn[(k >> 1) & 3]));
		return 1;
	}
	if (k >= 0x28 && k <= 0x2f)			/* ALU/MAC with a data register move */
	{
		adsp_jit_emit_rdreg(C, 0, op & 15, SLJIT_S5);
		adsp_jit_emit_aluop(C, op, (k >> 1) & 3);
		adsp_jit_emit_wrreg(C, 0, (op >> 4) & 15, SLJIT_S5, 0);
		return 1;
	}
	if (k >= 0x80 && k <= 0x8b)			/* register (groups 0-2) from RAM at a fixed address */
	{
		UINT16 *p = adsp_dm_rptr[(op >> 4) & 0x3fff];
		sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_S5, 0, SLJIT_MEM0(), (sljit_sw)p);
		adsp_jit_emit_wrreg(C, (k >> 2) & 3, op & 15, SLJIT_S5, 0);
		return 1;
	}
	if (k >= 0x90 && k <= 0x9b)			/* register (groups 0-2) to RAM at a fixed address */
	{
		UINT16 *p = adsp_dm_wptr[(op >> 4) & 0x3fff];
		adsp_jit_emit_rdreg(C, (k >> 2) & 3, op & 15, SLJIT_R0);
		sljit_emit_op1(C, SLJIT_MOV_U16, SLJIT_MEM0(), (sljit_sw)p, SLJIT_R0, 0);
		return 1;
	}
	return 0;
}

/* the RAM pointer again (R1), once the check passed and nothing it looks at changed */
static void adsp_jit_emit_dmptr(struct sljit_compiler *C, UINT32 op)
{
	int write, dag = adsp_jit_dmop(op, &write);
	int ireg = ((dag == 1) ? 0 : 4) + ((op >> 2) & 3);
	sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(adsp2100_Regs, i) + ireg * sizeof(UINT32));
	if (dag == 1)
		adsp_jit_emit_reverse(C);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_R1, 0, SLJIT_MEM2(write ? SLJIT_S4 : SLJIT_S3, SLJIT_R0), SLJIT_WORD_SHIFT);
}

static void adsp_jit_emit_call(struct sljit_compiler *C, UINT32 op)
{
	sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, op);
	sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(adsp_jit_ops[op >> 16]));
}

#define AJ_SUBCOUNT(c)	do { if (c) sljit_emit_op2(C, SLJIT_SUB32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, (c)); } while (0)
#define AJ_ADDCOUNT(c)	do { if (c) sljit_emit_op2(C, SLJIT_ADD32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, (c)); } while (0)

static void *adsp_jit_compile(UINT32 pc)
{
	struct sljit_compiler *C = sljit_create_compiler(NULL);
	struct sljit_jump *exits[ADSP_JIT_MAX_BLOCK * 4], *loops[ADSP_JIT_MAX_BLOCK][4];
	struct sljit_jump *backs[ADSP_JIT_MAX_BLOCK];
	struct sljit_label *labels[ADSP_JIT_MAX_BLOCK];
	UINT32 ops[ADSP_JIT_MAX_BLOCK];
	INT32 body[ADSP_JIT_MAX_BLOCK];			/* the loop body start of a DO ending here, in the block (index), or -1 */
	UINT8 islabel[ADSP_JIT_MAX_BLOCK], kind[ADSP_JIT_MAX_BLOCK], nloops[ADSP_JIT_MAX_BLOCK];
	int n, i, nexits = 0, pending = 0;
	void *code;

	if (!C)
		return NULL;

	n = 0x4000 - pc;
	if (n > ADSP_JIT_MAX_BLOCK)
		n = ADSP_JIT_MAX_BLOCK;

	/* the DO loops whose body and end are both in the block: their end can */
	/* jump back to the body's start inside the block */
	memset(islabel, 0, sizeof(islabel));
	for (i = 0; i < n; i++)
	{
		UINT32 a = pc + i;
		INT32 x;
		ops[i] = RWORD_PGM(a);
		kind[i] = adsp_jit_kind(ops[i]);
		body[i] = -1;
		if (!adsp_jit_loopend[a] || kind[i] == AJ_OTHER)
			continue;
		for (x = (INT32)a - 1; x >= (INT32)pc - 1 && x >= 0; x--)
		{
			UINT32 o = RWORD_PGM(x);
			if ((o >> 18) == 0x05 && ((o >> 4) & 0x3fff) == a)
			{
				if (x + 1 >= (INT32)pc && x + 1 < (INT32)a)
				{
					body[i] = x + 1 - pc;
					islabel[body[i]] = 1;
				}
				break;
			}
		}
	}

	sljit_emit_enter(C, 0, SLJIT_ARGS0V(), 4, 6, 0);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S0, 0, SLJIT_IMM, (sljit_sw)&adsp2100);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S1, 0, SLJIT_IMM, (sljit_sw)&adsp2100_icount);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S3, 0, SLJIT_IMM, (sljit_sw)adsp_dm_rptr);
	sljit_emit_op1(C, SLJIT_MOV_P, SLJIT_S4, 0, SLJIT_IMM, (sljit_sw)adsp_dm_wptr);

	for (i = 0; i < n; i++)
	{
		UINT32 a = pc + i;
		UINT32 op = ops[i];
		int last = (i == n - 1);
		nloops[i] = 0;
		labels[i] = NULL;
		backs[i] = NULL;

		/* a loop body start: everything accounted, so jumps from its end land here */
		if (islabel[i])
		{
			AJ_SUBCOUNT(pending);
			pending = 0;
			labels[i] = sljit_emit_label(C);
		}

		if (adsp_jit_loopend[a])
		{
			if (kind[i] != AJ_OTHER)
			{
				/* counted by CNTR, the common case: the loop step here, then the */
				/* instruction, then back to the body (in the block when it fits); */
				/* any other case goes through the interpreter's code (the stub) */
				struct sljit_jump *noloop, *done;
				AJ_SUBCOUNT(pending);
				pending = 0;
				noloop = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, AJ_MEM(loop), SLJIT_IMM, a);
				if (kind[i] == AJ_DM)
					nloops[i] = adsp_jit_emit_dmcheck(C, op, loops[i]);
				sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, a);
				sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (body[i] >= 0) ? (sljit_sw)(pc + body[i]) : (sljit_sw)0xffffffff);
				sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, 32, 32), SLJIT_IMM, SLJIT_FUNC_ADDR(adsp_jit_loop_ce));
				loops[i][nloops[i]++] = sljit_emit_cmp(C, SLJIT_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 0);
				sljit_emit_op1(C, SLJIT_MOV, SLJIT_S2, 0, SLJIT_R0, 0);
				if (kind[i] == AJ_DM)
				{
					adsp_jit_emit_dmptr(C, op);
					if (!adsp_jit_emit_native_dm(C, op))
						adsp_jit_emit_call(C, op);
				}
				else if (!adsp_jit_emit_native(C, op))
					adsp_jit_emit_call(C, op);
				done = sljit_emit_cmp(C, SLJIT_EQUAL, SLJIT_S2, 0, SLJIT_IMM, 0);
				/* back to the body: its cycle now, then the body when the rest of the block fits */
				AJ_SUBCOUNT(1);
				if (body[i] >= 0)
					backs[i] = sljit_emit_cmp(C, SLJIT_SIG_GREATER_EQUAL | SLJIT_32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, n - body[i]);
				sljit_emit_return_void(C);		/* PC and PPC set by the loop step */
				sljit_set_label(noloop, sljit_emit_label(C));
				/* the instruction as anywhere else, below; 'done' joins after it */
				if (kind[i] == AJ_PURE)
				{
					if (!adsp_jit_emit_native(C, op))
						adsp_jit_emit_call(C, op);
					sljit_set_label(done, sljit_emit_label(C));
					pending++;
					continue;
				}
				/* data memory: 'done' (RAM, checked) joins the fast path's end */
				{
					struct sljit_jump *slow[4], *conv;
					int nslow = adsp_jit_emit_dmcheck(C, op, slow), k;
					if (!adsp_jit_emit_native_dm(C, op))
						adsp_jit_emit_call(C, op);
					sljit_set_label(done, sljit_emit_label(C));
					conv = sljit_emit_jump(C, SLJIT_JUMP);
					for (k = 0; k < nslow; k++)
						sljit_set_label(slow[k], sljit_emit_label(C));
					sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(ppc), SLJIT_IMM, a);
					sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(pc), SLJIT_IMM, a + 1);
					adsp_jit_emit_call(C, op);
					AJ_SUBCOUNT(1);
					if (!last)
					{
						exits[nexits++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, AJ_MEM(pc), SLJIT_IMM, a + 1);
						exits[nexits++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM0(), (sljit_sw)&adsp2100_pgm_dirty, SLJIT_IMM, 0);
						exits[nexits++] = sljit_emit_cmp(C, SLJIT_SIG_LESS | SLJIT_32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, n - 1 - i);
					}
					AJ_ADDCOUNT(1);		/* counted with the others, as on the fast path */
					sljit_set_label(conv, sljit_emit_label(C));
					pending++;
					continue;
				}
			}
			/* the interpreter's code for the loop step and the instruction */
			AJ_SUBCOUNT(pending);
			pending = 0;
			nloops[i] = 1;
			loops[i][0] = sljit_emit_cmp(C, SLJIT_EQUAL | SLJIT_32, AJ_MEM(loop), SLJIT_IMM, a);
		}

		if (kind[i] == AJ_PURE)
		{
			if (!adsp_jit_emit_native(C, op))
				adsp_jit_emit_call(C, op);
			pending++;
			continue;
		}

		if (kind[i] == AJ_DM)
		{
			/* RAM: as a pure one; else as the interpreter, with the cycles so far */
			/* taken, then given back so the fast and slow paths join */
			struct sljit_jump *slow[4], *conv, *purecall, *conv2 = NULL;
			int nslow = adsp_jit_emit_dmcheck_p(C, op, slow, &purecall), k;
			if (!adsp_jit_emit_native_dm(C, op))
				adsp_jit_emit_call(C, op);
			conv = sljit_emit_jump(C, SLJIT_JUMP);
			if (purecall)
			{
				sljit_set_label(purecall, sljit_emit_label(C));
				adsp_jit_emit_call(C, op);
				conv2 = sljit_emit_jump(C, SLJIT_JUMP);
			}
			for (k = 0; k < nslow; k++)
				sljit_set_label(slow[k], sljit_emit_label(C));
			AJ_SUBCOUNT(pending);
			sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(ppc), SLJIT_IMM, a);
			sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(pc), SLJIT_IMM, a + 1);
			adsp_jit_emit_call(C, op);
			AJ_SUBCOUNT(1);
			if (!last)
			{
				exits[nexits++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, AJ_MEM(pc), SLJIT_IMM, a + 1);
				exits[nexits++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM0(), (sljit_sw)&adsp2100_pgm_dirty, SLJIT_IMM, 0);
				exits[nexits++] = sljit_emit_cmp(C, SLJIT_SIG_LESS | SLJIT_32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, n - 1 - i);
			}
			AJ_ADDCOUNT(pending + 1);
			{
				struct sljit_label *cl = sljit_emit_label(C);
				sljit_set_label(conv, cl);
				if (conv2)
					sljit_set_label(conv2, cl);
			}
			pending++;
			continue;
		}

		/* everything up to date, as the interpreter has it while it runs the instruction */
		AJ_SUBCOUNT(pending);
		pending = 0;
		sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(ppc), SLJIT_IMM, a);
		sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(pc), SLJIT_IMM, a + 1);
		adsp_jit_emit_call(C, op);
		AJ_SUBCOUNT(1);

		/* leave when the PC changed, program memory was written, or the */
		/* rest of the block no longer fits in the timeslice (the interpreter */
		/* stops as soon as the count reaches 0) */
		if (!last)
		{
			exits[nexits++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, AJ_MEM(pc), SLJIT_IMM, a + 1);
			exits[nexits++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_MEM0(), (sljit_sw)&adsp2100_pgm_dirty, SLJIT_IMM, 0);
			exits[nexits++] = sljit_emit_cmp(C, SLJIT_SIG_LESS | SLJIT_32, SLJIT_MEM1(SLJIT_S1), 0, SLJIT_IMM, n - 1 - i);
		}
	}

	/* the end of the block */
	if (pending)
	{
		AJ_SUBCOUNT(pending);
		sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(ppc), SLJIT_IMM, pc + n - 1);
		sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(pc), SLJIT_IMM, pc + n);
	}
	{
		struct sljit_label *out = sljit_emit_label(C);
		for (i = 0; i < nexits; i++)
			sljit_set_label(exits[i], out);
		sljit_emit_return_void(C);
	}

	/* the ends of DO loops through the interpreter's own steps (PPC, the loop, */
	/* the instruction, the cycle); the cycles before are all taken there */
	for (i = 0; i < n; i++)
		if (nloops[i])
		{
			int k;
			struct sljit_label *l = sljit_emit_label(C);
			for (k = 0; k < nloops[i]; k++)
				sljit_set_label(loops[i][k], l);
			sljit_emit_op1(C, SLJIT_MOV32, AJ_MEM(pc), SLJIT_IMM, pc + i);
			sljit_emit_op1(C, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, ops[i]);
			sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(32), SLJIT_IMM, SLJIT_FUNC_ADDR(adsp_jit_loop_end));
			sljit_emit_return_void(C);
		}

	for (i = 0; i < n; i++)
		if (backs[i])
			sljit_set_label(backs[i], labels[body[i]]);

	code = sljit_generate_code(C, 0, NULL);
	sljit_free_compiler(C);
	if (code)
		adsp_jit_len[pc] = n;
	return code;
}

typedef void (SLJIT_FUNC *adsp_jit_func)(void);

/* the execution loop with the recompiler: as the interpreter's, a block at a time */
static void adsp_jit_run(void)
{
	do
	{
		UINT32 pc = adsp2100.pc;
		void *code;

		if (!adsp_dm_built)
			adsp_jit_build_dm();
		if (adsp2100_pgm_dirty)
		{
			/* a boot load, a reset or too many writes: everything; else what was dropped */
			if (adsp_jit_flush_all)
				adsp_jit_flush();
			adsp_jit_free_dead();
			adsp2100_pgm_dirty = 0;
		}
		if (pc > 0x3fff)
		{
			adsp_jit_step();
			continue;
		}
		code = adsp_jit_code[pc];
		if (!code)
			code = adsp_jit_code[pc] = adsp_jit_compile(pc);
		/* a block runs only when all of it fits in the timeslice */
		if (code && (int)adsp_jit_len[pc] <= adsp2100_icount)
			((adsp_jit_func)code)();
		else
			adsp_jit_step();
	} while (adsp2100_icount > 0);
}

static void adsp_jit_exit(void)
{
	adsp_jit_flush();
	free(adsp_jit_code);
	free(adsp_dm_rptr);
	free(adsp_dm_wptr);
	adsp_jit_code = NULL;
	adsp_dm_rptr = adsp_dm_wptr = NULL;
	adsp_jit_enabled = 0;
}

#else	/* !ADSP2100_JIT_SUPPORTED */

static const int adsp_jit_enabled = 0;
static void adsp_jit_pgm_write(UINT32 addr, UINT32 data) { }
void adsp2100_jit_pure_read(UINT32 start, UINT32 end) { }
static void adsp_jit_run(void) { }
static void adsp_jit_exit(void) { }

#endif	/* ADSP2100_JIT_SUPPORTED */


int adsp2100_jit_available(void)
{
	return ADSP2100_JIT_SUPPORTED;
}

void adsp2100_set_jit(int enable)
{
#if ADSP2100_JIT_SUPPORTED
	if (enable && !adsp_jit_code)
	{
		adsp_jit_code = calloc(0x4000, sizeof(adsp_jit_code[0]));
		adsp_dm_rptr = calloc(0x4000, sizeof(adsp_dm_rptr[0]));
		adsp_dm_wptr = calloc(0x4000, sizeof(adsp_dm_wptr[0]));
		adsp_dm_built = 0;
		if (!adsp_jit_code || !adsp_dm_rptr || !adsp_dm_wptr)
		{
			free(adsp_jit_code); free(adsp_dm_rptr); free(adsp_dm_wptr);
			adsp_jit_code = NULL; adsp_dm_rptr = adsp_dm_wptr = NULL;
		}
		adsp_jit_enabled = (adsp_jit_code != NULL);
		adsp_jit_flush_all = adsp2100_pgm_dirty = 1;		/* scans program memory first */
		/* a loaded state rewrites program memory behind our back */
		if (adsp_jit_enabled)
			state_save_register_func_postload(adsp_jit_postload);
	}
	else if (!enable && adsp_jit_code)
		adsp_jit_exit();
#endif
}
