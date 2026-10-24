/*
    Intel 386 emulator

    Written by Ville Linde

    Currently supports:
        Intel 386
        Intel 486
        Intel Pentium
        Cyrix MediaGX
*/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "i386priv.h"
#include "i386.h"
#include "mamedbg.h"

int i386_ICount;

/* the 0.139 core stops on what it does not implement; here it is logged once */
static void i386_fatal(const char *fmt, ...)
{
	static int reported;
	if (!reported)
	{
		char buf[256];
		va_list va;
		va_start(va, fmt);
		vsprintf(buf, fmt, va);
		va_end(va);
		log_cb(RETRO_LOG_ERROR, LOGPRE "%s\n", buf);
		reported = 1;
	}
}

/* seems to be defined on mingw-gcc */
#undef i386

int i386_parity_table[256];
MODRM_TABLE i386_MODRM_table[256];

/*************************************************************************/

#define INT_DEBUG	1

static void i386_load_protected_mode_segment(i386_state *cpustate, I386_SREG *seg )
{
	UINT32 v1,v2;
	UINT32 base, limit;
	int entry;

	if ( seg->selector & 0x4 )
	{
		base = cpustate->ldtr.base;
		limit = cpustate->ldtr.limit;
	} else {
		base = cpustate->gdtr.base;
		limit = cpustate->gdtr.limit;
	}

	if (limit == 0 || seg->selector + 7 > limit)
		return;
	entry = seg->selector & ~0x7;

	v1 = READ32(cpustate, base + entry );
	v2 = READ32(cpustate, base + entry + 4 );

	seg->flags = (v2 >> 8) & 0xf0ff;
	seg->base = (v2 & 0xff000000) | ((v2 & 0xff) << 16) | ((v1 >> 16) & 0xffff);
	seg->limit = (v2 & 0xf0000) | (v1 & 0xffff);
	if (seg->flags & 0x8000)
		seg->limit = (seg->limit << 12) | 0xfff;
	seg->d = (seg->flags & 0x4000) ? 1 : 0;
}

static void i386_load_segment_descriptor(i386_state *cpustate, int segment )
{
	if (PROTECTED_MODE)
	{
		if (!V8086_MODE)
			i386_load_protected_mode_segment(cpustate, &cpustate->sreg[segment] );
		else
		{
			cpustate->sreg[segment].base = cpustate->sreg[segment].selector << 4;
			cpustate->sreg[segment].limit = 0xffff;
			cpustate->sreg[segment].flags = (segment == CS) ? 0x009a : 0x0092;
		}
	}
	else
	{
		cpustate->sreg[segment].base = cpustate->sreg[segment].selector << 4;

		if( segment == CS && !cpustate->performed_intersegment_jump )
			cpustate->sreg[segment].base |= 0xfff00000;
	}
}

static UINT32 get_flags(i386_state *cpustate)
{
	UINT32 f = 0x2;
	f |= cpustate->CF;
	f |= cpustate->PF << 2;
	f |= cpustate->AF << 4;
	f |= cpustate->ZF << 6;
	f |= cpustate->SF << 7;
	f |= cpustate->TF << 8;
	f |= cpustate->IF << 9;
	f |= cpustate->DF << 10;
	f |= cpustate->OF << 11;
	f |= cpustate->IOP1 << 12;
	f |= cpustate->IOP2 << 13;
	f |= cpustate->NT << 14;
	return (cpustate->eflags & cpustate->eflags_mask) | (f & 0xffff);
}

static void set_flags(i386_state *cpustate, UINT32 f )
{
	cpustate->CF = (f & 0x1) ? 1 : 0;
	cpustate->PF = (f & 0x4) ? 1 : 0;
	cpustate->AF = (f & 0x10) ? 1 : 0;
	cpustate->ZF = (f & 0x40) ? 1 : 0;
	cpustate->SF = (f & 0x80) ? 1 : 0;
	cpustate->TF = (f & 0x100) ? 1 : 0;
	cpustate->IF = (f & 0x200) ? 1 : 0;
	cpustate->DF = (f & 0x400) ? 1 : 0;
	cpustate->OF = (f & 0x800) ? 1 : 0;
	cpustate->IOP1 = (f & 0x1000) ? 1 : 0;
	cpustate->IOP2 = (f & 0x2000) ? 1 : 0;
	cpustate->NT = (f & 0x4000) ? 1 : 0;
	cpustate->eflags = f & cpustate->eflags_mask;
}

static void sib_byte(i386_state *cpustate,UINT8 mod, UINT32* out_ea, UINT8* out_segment)
{
	UINT32 ea = 0;
	UINT8 segment = 0;
	UINT8 scale, i, base;
	UINT8 sib = FETCH(cpustate);
	scale = (sib >> 6) & 0x3;
	i = (sib >> 3) & 0x7;
	base = sib & 0x7;

	switch( base )
	{
		case 0: ea = REG32(EAX); segment = DS; break;
		case 1: ea = REG32(ECX); segment = DS; break;
		case 2: ea = REG32(EDX); segment = DS; break;
		case 3: ea = REG32(EBX); segment = DS; break;
		case 4: ea = REG32(ESP); segment = SS; break;
		case 5:
			if( mod == 0 ) {
				ea = FETCH32(cpustate);
				segment = DS;
			} else if( mod == 1 ) {
				ea = REG32(EBP);
				segment = SS;
			} else if( mod == 2 ) {
				ea = REG32(EBP);
				segment = SS;
			}
			break;
		case 6: ea = REG32(ESI); segment = DS; break;
		case 7: ea = REG32(EDI); segment = DS; break;
	}
	switch( i )
	{
		case 0: ea += REG32(EAX) * (1 << scale); break;
		case 1: ea += REG32(ECX) * (1 << scale); break;
		case 2: ea += REG32(EDX) * (1 << scale); break;
		case 3: ea += REG32(EBX) * (1 << scale); break;
		case 4: break;
		case 5: ea += REG32(EBP) * (1 << scale); break;
		case 6: ea += REG32(ESI) * (1 << scale); break;
		case 7: ea += REG32(EDI) * (1 << scale); break;
	}
	*out_ea = ea;
	*out_segment = segment;
}

static void modrm_to_EA(i386_state *cpustate,UINT8 mod_rm, UINT32* out_ea, UINT8* out_segment)
{
	INT8 disp8;
	INT16 disp16;
	INT32 disp32;
	UINT8 mod = (mod_rm >> 6) & 0x3;
	UINT8 rm = mod_rm & 0x7;
	UINT32 ea;
	UINT8 segment;

	if( mod_rm >= 0xc0 )
		i386_fatal("i386: Called modrm_to_EA with modrm value %02X !",mod_rm);

	if( cpustate->address_size ) {
		switch( rm )
		{
			default:
			case 0: ea = REG32(EAX); segment = DS; break;
			case 1: ea = REG32(ECX); segment = DS; break;
			case 2: ea = REG32(EDX); segment = DS; break;
			case 3: ea = REG32(EBX); segment = DS; break;
			case 4: sib_byte(cpustate, mod, &ea, &segment ); break;
			case 5:
				if( mod == 0 ) {
					ea = FETCH32(cpustate); segment = DS;
				} else {
					ea = REG32(EBP); segment = SS;
				}
				break;
			case 6: ea = REG32(ESI); segment = DS; break;
			case 7: ea = REG32(EDI); segment = DS; break;
		}
		if( mod == 1 ) {
			disp8 = FETCH(cpustate);
			ea += (INT32)disp8;
		} else if( mod == 2 ) {
			disp32 = FETCH32(cpustate);
			ea += disp32;
		}

		if( cpustate->segment_prefix )
			segment = cpustate->segment_override;

		*out_ea = ea;
		*out_segment = segment;

	} else {
		switch( rm )
		{
			default:
			case 0: ea = REG16(BX) + REG16(SI); segment = DS; break;
			case 1: ea = REG16(BX) + REG16(DI); segment = DS; break;
			case 2: ea = REG16(BP) + REG16(SI); segment = SS; break;
			case 3: ea = REG16(BP) + REG16(DI); segment = SS; break;
			case 4: ea = REG16(SI); segment = DS; break;
			case 5: ea = REG16(DI); segment = DS; break;
			case 6:
				if( mod == 0 ) {
					ea = FETCH16(cpustate); segment = DS;
				} else {
					ea = REG16(BP); segment = SS;
				}
				break;
			case 7: ea = REG16(BX); segment = DS; break;
		}
		if( mod == 1 ) {
			disp8 = FETCH(cpustate);
			ea += (INT32)disp8;
		} else if( mod == 2 ) {
			disp16 = FETCH16(cpustate);
			ea += (INT32)disp16;
		}

		if( cpustate->segment_prefix )
			segment = cpustate->segment_override;

		*out_ea = ea & 0xffff;
		*out_segment = segment;
	}
}

static UINT32 GetNonTranslatedEA(i386_state *cpustate,UINT8 modrm)
{
	UINT8 segment;
	UINT32 ea;
	modrm_to_EA(cpustate, modrm, &ea, &segment );
	return ea;
}

static UINT32 GetEA(i386_state *cpustate,UINT8 modrm)
{
	UINT8 segment;
	UINT32 ea;
	modrm_to_EA(cpustate, modrm, &ea, &segment );
	return i386_translate(cpustate, segment, ea );
}

static void i386_trap(i386_state *cpustate,int irq, int irq_gate)
{
	/*  I386 Interrupts/Traps/Faults:
     *
     *  0x00    Divide by zero
     *  0x01    Debug exception
     *  0x02    NMI
     *  0x03    Int3
     *  0x04    Overflow
     *  0x05    Array bounds check
     *  0x06    Illegal Opcode
     *  0x07    FPU not available
     *  0x08    Double fault
     *  0x09    Coprocessor segment overrun
     *  0x0a    Invalid task state
     *  0x0b    Segment not present
     *  0x0c    Stack exception
     *  0x0d    General Protection Fault
     *  0x0e    Page fault
     *  0x0f    Reserved
     *  0x10    Coprocessor error
     */
	UINT32 v1, v2;
	UINT32 offset;
	UINT16 segment;
	int entry = irq * (PROTECTED_MODE ? 8 : 4);

	/* Check if IRQ is out of IDTR's bounds */
	if( entry > cpustate->idtr.limit ) {
		i386_fatal("I386 Interrupt: IRQ out of IDTR bounds (IRQ: %d, IDTR Limit: %d)", irq, cpustate->idtr.limit);
	}

	if( !(PROTECTED_MODE) )
	{
		/* 16-bit */
		PUSH16(cpustate, get_flags(cpustate) & 0xffff );
		PUSH16(cpustate, cpustate->sreg[CS].selector );
		PUSH16(cpustate, cpustate->eip );

		cpustate->sreg[CS].selector = READ16(cpustate, cpustate->idtr.base + entry + 2 );
		cpustate->eip = READ16(cpustate, cpustate->idtr.base + entry );

		/* Interrupts that vector through either interrupt gates or trap gates cause TF */
		/* (the trap flag) to be reset after the current value of TF is saved on the stack as part of EFLAGS. */
		cpustate->TF = 0;

		if (irq_gate)
		{
			cpustate->IF = 0;
		}

	}
	else
	{
		int type;
		/* 32-bit */

		v1 = READ32(cpustate, cpustate->idtr.base + entry );
		v2 = READ32(cpustate, cpustate->idtr.base + entry + 4 );
		offset = (v2 & 0xffff0000) | (v1 & 0xffff);
		segment = (v1 >> 16) & 0xffff;
		type = (v2>>8) & 0x1F;

		if(type != 0x0e && type != 0x0f)  /* if not 386 interrupt or trap gate */
		{
			PUSH16(cpustate, get_flags(cpustate) & 0xffff );
			PUSH16(cpustate, cpustate->sreg[CS].selector );
			PUSH16(cpustate, cpustate->eip );
		}
		else
		{
			PUSH32(cpustate, get_flags(cpustate) & 0x00fcffff );
			PUSH32(cpustate, cpustate->sreg[CS].selector );
			PUSH32(cpustate, cpustate->eip );
		}

		cpustate->sreg[CS].selector = segment;
		cpustate->eip = offset;

		/* Interrupts that vector through either interrupt gates or trap gates cause TF */
		/* (the trap flag) to be reset after the current value of TF is saved on the stack as part of EFLAGS. */
		if ((type == 14) || (type==15))
			cpustate->TF = 0;

		if (type == 14)
		{
			cpustate->IF = 0;
		}

	}

	i386_load_segment_descriptor(cpustate,CS);
	CHANGE_PC(cpustate,cpustate->eip);

}

static void i386_check_irq_line(i386_state *cpustate)
{
	/* Check if the interrupts are enabled */
	if ( (cpustate->irq_state) && cpustate->IF )
	{
		i386_ICount -= 2;
		i386_trap(cpustate,cpustate->irq_callback(0), 1);
	}
}

#include "cycles.h"

static UINT8 *cycle_table_rm[X86_NUM_CPUS];
static UINT8 *cycle_table_pm[X86_NUM_CPUS];

#define CYCLES_NUM(x)	(i386_ICount -= (x))

static INLINE void CYCLES(i386_state *cpustate,int x)
{
	if (PROTECTED_MODE)
	{
		i386_ICount -= cpustate->cycle_table_pm[x];
	}
	else
	{
		i386_ICount -= cpustate->cycle_table_rm[x];
	}
}

static INLINE void CYCLES_RM(i386_state *cpustate,int modrm, int r, int m)
{
	if (modrm >= 0xc0)
	{
		if (PROTECTED_MODE)
		{
			i386_ICount -= cpustate->cycle_table_pm[r];
		}
		else
		{
			i386_ICount -= cpustate->cycle_table_rm[r];
		}
	}
	else
	{
		if (PROTECTED_MODE)
		{
			i386_ICount -= cpustate->cycle_table_pm[m];
		}
		else
		{
			i386_ICount -= cpustate->cycle_table_rm[m];
		}
	}
}

static UINT8 cycle_tables[X86_NUM_CPUS][2][CYCLES_NUM_OPCODES];

static void build_cycle_table(void)
{
	int i, j;
	for (j=0; j < X86_NUM_CPUS; j++)
	{
		cycle_table_rm[j] = cycle_tables[j][0];
		cycle_table_pm[j] = cycle_tables[j][1];

		for (i=0; i < sizeof(x86_cycle_table)/sizeof(X86_CYCLE_TABLE); i++)
		{
			int opcode = x86_cycle_table[i].op;
			cycle_table_rm[j][opcode] = x86_cycle_table[i].cpu_cycles[j][0];
			cycle_table_pm[j][opcode] = x86_cycle_table[i].cpu_cycles[j][1];
		}
	}
}

/* Forward declarations */
static void I386OP(decode_opcode)(i386_state *cpustate);
static void I386OP(decode_two_byte)(i386_state *cpustate);



#include "i386ops.c"
#include "i386op16.c"
#include "i386op32.c"
#include "i486ops.c"
#include "pentops.c"
#include "x87ops.c"
#include "i386ops.h"

static void I386OP(decode_opcode)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table1_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table1_16[cpustate->opcode](cpustate);
}

/* Two-byte opcode prefix */
static void I386OP(decode_two_byte)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table2_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table2_16[cpustate->opcode](cpustate);
}

/*************************************************************************/

static void build_opcode_table(i386_state *cpustate, UINT32 features)
{
	int i;
	for (i=0; i < 256; i++)
	{
		cpustate->opcode_table1_16[i] = I386OP(invalid);
		cpustate->opcode_table1_32[i] = I386OP(invalid);
		cpustate->opcode_table2_16[i] = I386OP(invalid);
		cpustate->opcode_table2_32[i] = I386OP(invalid);
	}

	for (i=0; i < sizeof(x86_opcode_table)/sizeof(X86_OPCODE); i++)
	{
		const X86_OPCODE *op = &x86_opcode_table[i];

		if ((op->flags & features))
		{
			if (op->flags & OP_2BYTE)
			{
				cpustate->opcode_table2_32[op->opcode] = op->handler32;
				cpustate->opcode_table2_16[op->opcode] = op->handler16;
			}
			else
			{
				cpustate->opcode_table1_32[op->opcode] = op->handler32;
				cpustate->opcode_table1_16[op->opcode] = op->handler16;
			}
		}
	}
}


/*************************************************************************

    The interface of the CPU core (MAME 0.78 style): one context, copied
    in and out by the CPU switch

*************************************************************************/

static i386_state I;

static void i386_postload(void)
{
	i386_state *cpustate = &I;
	int i;
	for (i = 0; i < 6; i++)
		i386_load_segment_descriptor(cpustate,i);
	CHANGE_PC(cpustate,cpustate->eip);
}

/* the fast memory regions of the driver */
static struct
{
	UINT32 start, end;
	UINT8 *base;
	int flags;
} i386_fastmem[I386_FASTMEM_MAX];
static int i386_fastmem_count;

void i386_set_fastmem(UINT32 start, UINT32 end, UINT8 *base, int flags)
{
	if (i386_fastmem_count < I386_FASTMEM_MAX)
	{
		i386_fastmem[i386_fastmem_count].start = start;
		i386_fastmem[i386_fastmem_count].end = end;
		i386_fastmem[i386_fastmem_count].base = base;
		i386_fastmem[i386_fastmem_count].flags = flags;
		i386_fastmem_count++;
	}
	/* empty the caches */
	memset(I.fast_size, 0, sizeof(I.fast_size));
}

/* a miss of the cache: the region of the address, cached; NULL if none */
static UINT8 *i386_fast_lookup(i386_state *cpustate, UINT32 address, UINT32 len, int kind)
{
	int i;

	for (i = 0; i < i386_fastmem_count; i++)
	{
		if ((i386_fastmem[i].flags & (1 << kind)) && address >= i386_fastmem[i].start &&
			address <= i386_fastmem[i].end && i386_fastmem[i].end - address >= len - 1)
		{
			cpustate->fast_start[kind] = i386_fastmem[i].start;
			cpustate->fast_size[kind] = i386_fastmem[i].end - i386_fastmem[i].start + 1;
			cpustate->fast_base[kind] = i386_fastmem[i].base;
			return i386_fastmem[i].base + (address - i386_fastmem[i].start);
		}
	}
	return NULL;
}

void i386_init(void)
{
	int i, j;
	static const int regs8[8] = {AL,CL,DL,BL,AH,CH,DH,BH};
	static const int regs16[8] = {AX,CX,DX,BX,SP,BP,SI,DI};
	static const int regs32[8] = {EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI};
	int cpu = cpu_getactivecpu();
	i386_state *cpustate = &I;
	static const char *const sregname[6] = { "es", "cs", "ss", "ds", "fs", "gs" };

	i386_fastmem_count = 0;
	memset(I.fast_size, 0, sizeof(I.fast_size));
	build_cycle_table();

	for( i=0; i < 256; i++ ) {
		int c=0;
		for( j=0; j < 8; j++ ) {
			if( i & (1 << j) )
				c++;
		}
		i386_parity_table[i] = ~(c & 0x1) & 0x1;
	}

	for( i=0; i < 256; i++ ) {
		i386_MODRM_table[i].reg.b = regs8[(i >> 3) & 0x7];
		i386_MODRM_table[i].reg.w = regs16[(i >> 3) & 0x7];
		i386_MODRM_table[i].reg.d = regs32[(i >> 3) & 0x7];

		i386_MODRM_table[i].rm.b = regs8[i & 0x7];
		i386_MODRM_table[i].rm.w = regs16[i & 0x7];
		i386_MODRM_table[i].rm.d = regs32[i & 0x7];
	}

	state_save_register_UINT32("i386", cpu, "reg", cpustate->reg.d, 8);
	for (i = 0; i < 6; i++)
	{
		char name[16];
		sprintf(name, "%s_sel", sregname[i]);
		state_save_register_UINT16("i386", cpu, name, &cpustate->sreg[i].selector, 1);
		sprintf(name, "%s_flags", sregname[i]);
		state_save_register_UINT16("i386", cpu, name, &cpustate->sreg[i].flags, 1);
		sprintf(name, "%s_base", sregname[i]);
		state_save_register_UINT32("i386", cpu, name, &cpustate->sreg[i].base, 1);
		sprintf(name, "%s_limit", sregname[i]);
		state_save_register_UINT32("i386", cpu, name, &cpustate->sreg[i].limit, 1);
		sprintf(name, "%s_d", sregname[i]);
		state_save_register_int("i386", cpu, name, &cpustate->sreg[i].d);
	}
	state_save_register_UINT32("i386", cpu, "eip", &cpustate->eip, 1);
	state_save_register_UINT32("i386", cpu, "pc", &cpustate->pc, 1);
	state_save_register_UINT32("i386", cpu, "prev_eip", &cpustate->prev_eip, 1);
	state_save_register_UINT32("i386", cpu, "eflags", &cpustate->eflags, 1);
	state_save_register_UINT32("i386", cpu, "eflags_mask", &cpustate->eflags_mask, 1);
	state_save_register_UINT8("i386", cpu, "CF", &cpustate->CF, 1);
	state_save_register_UINT8("i386", cpu, "DF", &cpustate->DF, 1);
	state_save_register_UINT8("i386", cpu, "SF", &cpustate->SF, 1);
	state_save_register_UINT8("i386", cpu, "OF", &cpustate->OF, 1);
	state_save_register_UINT8("i386", cpu, "ZF", &cpustate->ZF, 1);
	state_save_register_UINT8("i386", cpu, "PF", &cpustate->PF, 1);
	state_save_register_UINT8("i386", cpu, "AF", &cpustate->AF, 1);
	state_save_register_UINT8("i386", cpu, "IF", &cpustate->IF, 1);
	state_save_register_UINT8("i386", cpu, "TF", &cpustate->TF, 1);
	state_save_register_UINT8("i386", cpu, "IOP1", &cpustate->IOP1, 1);
	state_save_register_UINT8("i386", cpu, "IOP2", &cpustate->IOP2, 1);
	state_save_register_UINT8("i386", cpu, "NT", &cpustate->NT, 1);
	state_save_register_UINT32("i386", cpu, "cr", cpustate->cr, 5);
	state_save_register_UINT32("i386", cpu, "dr", cpustate->dr, 8);
	state_save_register_UINT32("i386", cpu, "tr", cpustate->tr, 8);
	state_save_register_UINT32("i386", cpu, "idtr_base", &cpustate->idtr.base, 1);
	state_save_register_UINT16("i386", cpu, "idtr_limit", &cpustate->idtr.limit, 1);
	state_save_register_UINT32("i386", cpu, "gdtr_base", &cpustate->gdtr.base, 1);
	state_save_register_UINT16("i386", cpu, "gdtr_limit", &cpustate->gdtr.limit, 1);
	state_save_register_UINT32("i386", cpu, "task_base", &cpustate->task.base, 1);
	state_save_register_UINT16("i386", cpu, "task_segment", &cpustate->task.segment, 1);
	state_save_register_UINT32("i386", cpu, "task_limit", &cpustate->task.limit, 1);
	state_save_register_UINT16("i386", cpu, "task_flags", &cpustate->task.flags, 1);
	state_save_register_UINT32("i386", cpu, "ldtr_base", &cpustate->ldtr.base, 1);
	state_save_register_UINT16("i386", cpu, "ldtr_segment", &cpustate->ldtr.segment, 1);
	state_save_register_UINT32("i386", cpu, "ldtr_limit", &cpustate->ldtr.limit, 1);
	state_save_register_UINT16("i386", cpu, "ldtr_flags", &cpustate->ldtr.flags, 1);
	state_save_register_int("i386", cpu, "halted", &cpustate->halted);
	state_save_register_UINT8("i386", cpu, "irq_state", &cpustate->irq_state, 1);
	state_save_register_UINT8("i386", cpu, "performed_intersegment_jump", &cpustate->performed_intersegment_jump, 1);
	state_save_register_UINT32("i386", cpu, "a20_mask", &cpustate->a20_mask, 1);
	state_save_register_func_postload(i386_postload);
}

void i386_reset(void *param)
{
	i386_state *cpustate = &I;
	int (*save_irqcallback)(int);

	save_irqcallback = cpustate->irq_callback;
	memset( cpustate, 0, sizeof(*cpustate) );
	cpustate->irq_callback = save_irqcallback;

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x7ffffff0; /* reserved bits set to 1 */
	cpustate->eflags = 0;
	cpustate->eflags_mask = 0x00030000;
	cpustate->eip = 0xfff0;

	/* Family 3 (386), Model 0 (DX), Stepping 8 (D1) */
	REG32(EAX) = 0;
	REG32(EDX) = (3 << 8) | (0 << 4) | (8);

	build_opcode_table(cpustate, OP_I386);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_I386];
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_I386];

	CHANGE_PC(cpustate,cpustate->eip);
}

void i386_exit(void)
{
}

int i386_execute(int cycles)
{
	i386_state *cpustate = &I;

	i386_ICount = cycles;
	cpustate->base_cycles = cycles;
	CHANGE_PC(cpustate,cpustate->eip);

	if (cpustate->halted)
	{
		cpustate->tsc += cycles;
		i386_ICount = 0;
		return cycles;
	}

	while( i386_ICount > 0 )
	{
		cpustate->operand_size = cpustate->sreg[CS].d;
		cpustate->address_size = cpustate->sreg[CS].d;

		cpustate->segment_prefix = 0;
		cpustate->prev_eip = cpustate->eip;

		CALL_MAME_DEBUG;

		i386_check_irq_line(cpustate);
		I386OP(decode_opcode)(cpustate);
	}
	cpustate->tsc += (cycles - i386_ICount);
	return cycles - i386_ICount;
}

unsigned i386_get_context(void *dst)
{
	if (dst)
		*(i386_state *)dst = I;
	return sizeof(i386_state);
}

void i386_set_context(void *src)
{
	if (src)
		I = *(i386_state *)src;
}

unsigned i386_get_reg(int regnum)
{
	i386_state *cpustate = &I;

	switch (regnum)
	{
	case REG_PC:			return cpustate->pc;
	case REG_PREVIOUSPC:	return cpustate->sreg[CS].base + cpustate->prev_eip;
	case REG_SP:			return cpustate->sreg[SS].base + REG32(ESP);
	case I386_EIP:			return cpustate->eip;
	case I386_EAX:			return REG32(EAX);
	case I386_EBX:			return REG32(EBX);
	case I386_ECX:			return REG32(ECX);
	case I386_EDX:			return REG32(EDX);
	case I386_ESI:			return REG32(ESI);
	case I386_EDI:			return REG32(EDI);
	case I386_EBP:			return REG32(EBP);
	case I386_ESP:			return REG32(ESP);
	case I386_EFLAGS:		return get_flags(cpustate);
	case I386_CS:			return cpustate->sreg[CS].selector;
	case I386_SS:			return cpustate->sreg[SS].selector;
	case I386_DS:			return cpustate->sreg[DS].selector;
	case I386_ES:			return cpustate->sreg[ES].selector;
	case I386_FS:			return cpustate->sreg[FS].selector;
	case I386_GS:			return cpustate->sreg[GS].selector;
	case I386_CR0:			return cpustate->cr[0];
	}
	return 0;
}

void i386_set_reg(int regnum, unsigned val)
{
	i386_state *cpustate = &I;

	switch (regnum)
	{
	case REG_PC:
		cpustate->eip = val - cpustate->sreg[CS].base;
		CHANGE_PC(cpustate,cpustate->eip);
		break;
	case REG_SP:			REG32(ESP) = val - cpustate->sreg[SS].base; break;
	case I386_EIP:			cpustate->eip = val; CHANGE_PC(cpustate,cpustate->eip); break;
	case I386_EAX:			REG32(EAX) = val; break;
	case I386_EBX:			REG32(EBX) = val; break;
	case I386_ECX:			REG32(ECX) = val; break;
	case I386_EDX:			REG32(EDX) = val; break;
	case I386_ESI:			REG32(ESI) = val; break;
	case I386_EDI:			REG32(EDI) = val; break;
	case I386_EBP:			REG32(EBP) = val; break;
	case I386_ESP:			REG32(ESP) = val; break;
	case I386_EFLAGS:		set_flags(cpustate, val); break;
	}
}

void i386_set_irq_line(int irqline, int state)
{
	i386_state *cpustate = &I;

	if (state != CLEAR_LINE && cpustate->halted)
	{
		cpustate->halted = 0;
	}

	if ( irqline == IRQ_LINE_NMI )
	{
		/* NMI (I do not think that this is 100% right) */
		if ( state )
			i386_trap(cpustate,2, 1);
	}
	else if ( irqline == I386_INPUT_LINE_A20 )
	{
		cpustate->a20_mask = state ? ~0 : ~(1 << 20);
	}
	else
	{
		cpustate->irq_state = state;
	}
}

void i386_set_irq_callback(int (*callback)(int irqline))
{
	I.irq_callback = callback;
}

const char *i386_info(void *context, int regnum)
{
	static char buffer[16][47+1];
	static int which = 0;
	i386_state *r = context ? (i386_state *)context : &I;

	which = (which + 1) % 16;
	buffer[which][0] = '\0';

	switch( regnum )
	{
	case CPU_INFO_REG+I386_EIP:	sprintf(buffer[which], "EIP:%08X", r->eip); break;
	case CPU_INFO_REG+I386_EAX:	sprintf(buffer[which], "EAX:%08X", r->reg.d[EAX]); break;
	case CPU_INFO_REG+I386_EBX:	sprintf(buffer[which], "EBX:%08X", r->reg.d[EBX]); break;
	case CPU_INFO_REG+I386_ECX:	sprintf(buffer[which], "ECX:%08X", r->reg.d[ECX]); break;
	case CPU_INFO_REG+I386_EDX:	sprintf(buffer[which], "EDX:%08X", r->reg.d[EDX]); break;
	case CPU_INFO_REG+I386_ESI:	sprintf(buffer[which], "ESI:%08X", r->reg.d[ESI]); break;
	case CPU_INFO_REG+I386_EDI:	sprintf(buffer[which], "EDI:%08X", r->reg.d[EDI]); break;
	case CPU_INFO_REG+I386_EBP:	sprintf(buffer[which], "EBP:%08X", r->reg.d[EBP]); break;
	case CPU_INFO_REG+I386_ESP:	sprintf(buffer[which], "ESP:%08X", r->reg.d[ESP]); break;
	case CPU_INFO_FLAGS:		sprintf(buffer[which], "%08X", r->eflags); break;
	case CPU_INFO_NAME:			return "I386";
	case CPU_INFO_FAMILY:		return "Intel 386";
	case CPU_INFO_VERSION:		return "1.0";
	case CPU_INFO_FILE:			return __FILE__;
	case CPU_INFO_CREDITS:		return "Copyright Ville Linde";
	}
	return buffer[which];
}

unsigned i386_dasm(char *buffer, unsigned pc)
{
	sprintf(buffer, "$%02X", cpu_readmem32ledw(pc));
	return 1;
}
