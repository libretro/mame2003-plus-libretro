#ifndef __I386_H__
#define __I386_H__

#include "driver.h"
#include "i386.h"

#ifdef MSB_FIRST
#define NATIVE_ENDIAN_VALUE_LE_BE(le,be) (be)
#else
#define NATIVE_ENDIAN_VALUE_LE_BE(le,be) (le)
#endif

#define I386OP(XX)		i386_##XX
#define I486OP(XX)		i486_##XX
#define PENTIUMOP(XX)	pentium_##XX
#define MMXOP(XX)		mmx_##XX


typedef enum { ES, CS, SS, DS, FS, GS } SREGS;

typedef enum
{
	AL = NATIVE_ENDIAN_VALUE_LE_BE(0,3),
	AH = NATIVE_ENDIAN_VALUE_LE_BE(1,2),
	CL = NATIVE_ENDIAN_VALUE_LE_BE(4,7),
	CH = NATIVE_ENDIAN_VALUE_LE_BE(5,6),
	DL = NATIVE_ENDIAN_VALUE_LE_BE(8,11),
	DH = NATIVE_ENDIAN_VALUE_LE_BE(9,10),
	BL = NATIVE_ENDIAN_VALUE_LE_BE(12,15),
	BH = NATIVE_ENDIAN_VALUE_LE_BE(13,14)
} BREGS;

typedef enum
{
	AX = NATIVE_ENDIAN_VALUE_LE_BE(0,1),
	CX = NATIVE_ENDIAN_VALUE_LE_BE(2,3),
	DX = NATIVE_ENDIAN_VALUE_LE_BE(4,5),
	BX = NATIVE_ENDIAN_VALUE_LE_BE(6,7),
	SP = NATIVE_ENDIAN_VALUE_LE_BE(8,9),
	BP = NATIVE_ENDIAN_VALUE_LE_BE(10,11),
	SI = NATIVE_ENDIAN_VALUE_LE_BE(12,13),
	DI = NATIVE_ENDIAN_VALUE_LE_BE(14,15)
} WREGS;

typedef enum { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI } DREGS;

enum
{
	I386_PC = 0,

	/* 8-bit registers */
	I386_AL,
	I386_AH,
	I386_BL,
	I386_BH,
	I386_CL,
	I386_CH,
	I386_DL,
	I386_DH,

	/* 16-bit registers */
	I386_AX,
	I386_BX,
	I386_CX,
	I386_DX,
	I386_BP,
	I386_SP,
	I386_SI,
	I386_DI,
	I386_IP,

	/* 32-bit registers */
	I386_EAX,
	I386_ECX,
	I386_EDX,
	I386_EBX,
	I386_EBP,
	I386_ESP,
	I386_ESI,
	I386_EDI,
	I386_EIP,

	/* segment registers */
	I386_CS,
	I386_CS_BASE,
	I386_CS_LIMIT,
	I386_CS_FLAGS,
	I386_SS,
	I386_SS_BASE,
	I386_SS_LIMIT,
	I386_SS_FLAGS,
	I386_DS,
	I386_DS_BASE,
	I386_DS_LIMIT,
	I386_DS_FLAGS,
	I386_ES,
	I386_ES_BASE,
	I386_ES_LIMIT,
	I386_ES_FLAGS,
	I386_FS,
	I386_FS_BASE,
	I386_FS_LIMIT,
	I386_FS_FLAGS,
	I386_GS,
	I386_GS_BASE,
	I386_GS_LIMIT,
	I386_GS_FLAGS,

	/* other */
	I386_EFLAGS,

	I386_CR0,
	I386_CR1,
	I386_CR2,
	I386_CR3,
	I386_CR4,

	I386_DR0,
	I386_DR1,
	I386_DR2,
	I386_DR3,
	I386_DR4,
	I386_DR5,
	I386_DR6,
	I386_DR7,

	I386_TR6,
	I386_TR7,

	I386_GDTR_BASE,
	I386_GDTR_LIMIT,
	I386_IDTR_BASE,
	I386_IDTR_LIMIT,
	I386_TR,
	I386_TR_BASE,
	I386_TR_LIMIT,
	I386_TR_FLAGS,
	I386_LDTR,
	I386_LDTR_BASE,
	I386_LDTR_LIMIT,
	I386_LDTR_FLAGS,

	X87_CTRL,
	X87_STATUS,
	X87_ST0,
	X87_ST1,
	X87_ST2,
	X87_ST3,
	X87_ST4,
	X87_ST5,
	X87_ST6,
	X87_ST7,
};

typedef struct {
	UINT16 selector;
	UINT16 flags;
	UINT32 base;
	UINT32 limit;
	int d;		/* Operand size */
} I386_SREG;

typedef struct {
	UINT32 base;
	UINT16 limit;
} I386_SYS_TABLE;

typedef struct {
	UINT16 segment;
	UINT16 flags;
	UINT32 base;
	UINT32 limit;
} I386_SEG_DESC;

typedef union {
	UINT32 d[8];
	UINT16 w[16];
	UINT8 b[32];
} I386_GPR;

typedef union {
	UINT64 i;
	double f;
} X87_REG;

typedef struct _i386_state i386_state;
struct _i386_state
{
	I386_GPR reg;
	I386_SREG sreg[6];
	UINT32 eip;
	UINT32 pc;
	UINT32 prev_eip;
	UINT32 eflags;
	UINT32 eflags_mask;
	UINT8 CF;
	UINT8 DF;
	UINT8 SF;
	UINT8 OF;
	UINT8 ZF;
	UINT8 PF;
	UINT8 AF;
	UINT8 IF;
	UINT8 TF;
	UINT8 IOP1;
	UINT8 IOP2;
	UINT8 NT;

	UINT8 performed_intersegment_jump;

	UINT32 cr[5];		/* Control registers */
	UINT32 dr[8];		/* Debug registers */
	UINT32 tr[8];		/* Test registers */

	I386_SYS_TABLE gdtr;	/* Global Descriptor Table Register */
	I386_SYS_TABLE idtr;	/* Interrupt Descriptor Table Register */
	I386_SEG_DESC task;		/* Task register */
	I386_SEG_DESC ldtr;		/* Local Descriptor Table Register */

	int halted;

	int operand_size;
	int address_size;

	int segment_prefix;
	int segment_override;

	int base_cycles;
	UINT8 opcode;

	UINT8 irq_state;
	int (*irq_callback)(int irqline);
	UINT32 a20_mask;

	int cpuid_max_input_value_eax;
	UINT32 cpuid_id0, cpuid_id1, cpuid_id2;
	UINT32 cpu_version;
	UINT32 feature_flags;
	UINT64 tsc;

	/* the fast memory cache, by kind of access (not saved: rebuilt on a miss) */
	UINT32 fast_start[3], fast_size[3];
	UINT8 *fast_base[3];

	/* FPU */
	X87_REG fpu_reg[8];
	UINT16 fpu_control_word;
	UINT16 fpu_status_word;
	UINT16 fpu_tag_word;
	UINT64 fpu_data_ptr;
	UINT64 fpu_inst_ptr;
	UINT16 fpu_opcode;
	int fpu_top;

	void (*opcode_table1_16[256])(i386_state *cpustate);
	void (*opcode_table1_32[256])(i386_state *cpustate);
	void (*opcode_table2_16[256])(i386_state *cpustate);
	void (*opcode_table2_32[256])(i386_state *cpustate);

	UINT8 *cycle_table_pm;
	UINT8 *cycle_table_rm;
};


extern int i386_parity_table[256];

#define PROTECTED_MODE		(cpustate->cr[0] & 0x1)
#define STACK_32BIT			(cpustate->sreg[SS].d)
#define V8086_MODE			(cpustate->eflags & 0x00020000)

#define SetOF_Add32(r,s,d)	(cpustate->OF = (((r) ^ (s)) & ((r) ^ (d)) & 0x80000000) ? 1: 0)
#define SetOF_Add16(r,s,d)	(cpustate->OF = (((r) ^ (s)) & ((r) ^ (d)) & 0x8000) ? 1 : 0)
#define SetOF_Add8(r,s,d)	(cpustate->OF = (((r) ^ (s)) & ((r) ^ (d)) & 0x80) ? 1 : 0)

#define SetOF_Sub32(r,s,d)	(cpustate->OF = (((d) ^ (s)) & ((d) ^ (r)) & 0x80000000) ? 1 : 0)
#define SetOF_Sub16(r,s,d)	(cpustate->OF = (((d) ^ (s)) & ((d) ^ (r)) & 0x8000) ? 1 : 0)
#define SetOF_Sub8(r,s,d)	(cpustate->OF = (((d) ^ (s)) & ((d) ^ (r)) & 0x80) ? 1 : 0)

#define SetCF8(x)			{cpustate->CF = ((x) & 0x100) ? 1 : 0; }
#define SetCF16(x)			{cpustate->CF = ((x) & 0x10000) ? 1 : 0; }
#define SetCF32(x)			{cpustate->CF = ((x) & (((UINT64)1) << 32)) ? 1 : 0; }

#define SetSF(x)			(cpustate->SF = (x))
#define SetZF(x)			(cpustate->ZF = (x))
#define SetAF(x,y,z)		(cpustate->AF = (((x) ^ ((y) ^ (z))) & 0x10) ? 1 : 0)
#define SetPF(x)			(cpustate->PF = i386_parity_table[(x) & 0xFF])

#define SetSZPF8(x)			{cpustate->ZF = ((UINT8)(x)==0);  cpustate->SF = ((x)&0x80) ? 1 : 0; cpustate->PF = i386_parity_table[x & 0xFF]; }
#define SetSZPF16(x)		{cpustate->ZF = ((UINT16)(x)==0);  cpustate->SF = ((x)&0x8000) ? 1 : 0; cpustate->PF = i386_parity_table[x & 0xFF]; }
#define SetSZPF32(x)		{cpustate->ZF = ((UINT32)(x)==0);  cpustate->SF = ((x)&0x80000000) ? 1 : 0; cpustate->PF = i386_parity_table[x & 0xFF]; }

/***********************************************************************************/

typedef struct {
	struct {
		int b;
		int w;
		int d;
	} reg;
	struct {
		int b;
		int w;
		int d;
	} rm;
} MODRM_TABLE;

extern MODRM_TABLE i386_MODRM_table[256];

#define REG8(x)			(cpustate->reg.b[x])
#define REG16(x)		(cpustate->reg.w[x])
#define REG32(x)		(cpustate->reg.d[x])

#define LOAD_REG8(x)	(REG8(i386_MODRM_table[x].reg.b))
#define LOAD_REG16(x)	(REG16(i386_MODRM_table[x].reg.w))
#define LOAD_REG32(x)	(REG32(i386_MODRM_table[x].reg.d))
#define LOAD_RM8(x)		(REG8(i386_MODRM_table[x].rm.b))
#define LOAD_RM16(x)	(REG16(i386_MODRM_table[x].rm.w))
#define LOAD_RM32(x)	(REG32(i386_MODRM_table[x].rm.d))

#define STORE_REG8(x, value)	(REG8(i386_MODRM_table[x].reg.b) = value)
#define STORE_REG16(x, value)	(REG16(i386_MODRM_table[x].reg.w) = value)
#define STORE_REG32(x, value)	(REG32(i386_MODRM_table[x].reg.d) = value)
#define STORE_RM8(x, value)		(REG8(i386_MODRM_table[x].rm.b) = value)
#define STORE_RM16(x, value)	(REG16(i386_MODRM_table[x].rm.w) = value)
#define STORE_RM32(x, value)	(REG32(i386_MODRM_table[x].rm.d) = value)

#define SWITCH_ENDIAN_32(x) (((((x) << 24) & (0xff << 24)) | (((x) << 8) & (0xff << 16)) | (((x) >> 8) & (0xff << 8)) | (((x) >> 24) & (0xff << 0))))

/***********************************************************************************/

static INLINE UINT32 i386_translate(i386_state *cpustate, int segment, UINT32 ip)
{
	/* TODO: segment limit */
	return cpustate->sreg[segment].base + ip;
}

static INLINE int translate_address(i386_state *cpustate, UINT32 *address)
{
	UINT32 a = *address;
	UINT32 pdbr = cpustate->cr[3] & 0xfffff000;
	UINT32 directory = (a >> 22) & 0x3ff;
	UINT32 table = (a >> 12) & 0x3ff;
	UINT32 offset = a & 0xfff;
	UINT32 page_entry;

	/* TODO: 4MB pages */
	UINT32 page_dir = cpu_readmem32ledw_dword(pdbr + directory * 4);
	if (!(cpustate->cr[4] & 0x10)) {
		page_entry = cpu_readmem32ledw_dword((page_dir & 0xfffff000) + (table * 4));
		*address = (page_entry & 0xfffff000) | offset;
	} else {
		if (page_dir & 0x80)
			*address = (page_dir & 0xffc00000) | (a & 0x003fffff);
		else {
			page_entry = cpu_readmem32ledw_dword((page_dir & 0xfffff000) + (table * 4));
			*address = (page_entry & 0xfffff000) | offset;
		}
	}
	return 1;
}

/* the fast memory: plain RAM and ROM the driver registers with */
/* i386_set_fastmem, fetched, read and written straight from the host */
/* memory instead of through the memory handlers (little endian hosts). */
/* A one-region cache for each kind of access; a miss looks the regions up */
#define I386_FAST_FETCH	0
#define I386_FAST_READ	1
#define I386_FAST_WRITE	2

static UINT8 *i386_fast_lookup(i386_state *cpustate, UINT32 address, UINT32 len, int kind);

static INLINE UINT8 *i386_fast_ptr(i386_state *cpustate, UINT32 address, UINT32 len, int kind)
{
#ifndef MSB_FIRST
	UINT32 off = address - cpustate->fast_start[kind];

	if (off < cpustate->fast_size[kind] && cpustate->fast_size[kind] - off >= len)
		return cpustate->fast_base[kind] + off;
	return i386_fast_lookup(cpustate, address, len, kind);
#else
	return NULL;
#endif
}

static INLINE void CHANGE_PC(i386_state *cpustate, UINT32 pc)
{
	UINT32 address;
	cpustate->pc = i386_translate(cpustate, CS, pc );

	address = cpustate->pc;

	if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
	{
		translate_address(cpustate,&address);
	}
}

static INLINE void NEAR_BRANCH(i386_state *cpustate, INT32 offs)
{
	UINT32 address;
	/* TODO: limit */
	cpustate->eip += offs;
	cpustate->pc += offs;

	address = cpustate->pc;

	if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
	{
		translate_address(cpustate,&address);
	}
}

static INLINE UINT8 FETCH(i386_state *cpustate);

static UINT8 FETCH_slow(i386_state *cpustate)
{
	UINT8 value;
	UINT32 address = cpustate->pc;

	if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
	{
		translate_address(cpustate,&address);
	}

	address &= cpustate->a20_mask;
	{
		UINT8 *p = i386_fast_ptr(cpustate, address, 1, I386_FAST_FETCH);
		value = p ? *p : cpu_readmem32ledw(address);
	}
	cpustate->eip++;
	cpustate->pc++;
	return value;
}
static UINT16 FETCH16_slow(i386_state *cpustate)
{
	UINT16 value;
	UINT32 address = cpustate->pc;

	if( address & 0x1 ) {		/* Unaligned read */
		value = (FETCH(cpustate) << 0) |
				(FETCH(cpustate) << 8);
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}
		address &= cpustate->a20_mask;
		{
			UINT8 *p = i386_fast_ptr(cpustate, address, 2, I386_FAST_FETCH);
			value = p ? *(UINT16 *)p : cpu_readmem32ledw_word(address);
		}
		cpustate->eip += 2;
		cpustate->pc += 2;
	}
	return value;
}
static UINT32 FETCH32_slow(i386_state *cpustate)
{
	UINT32 value;
	UINT32 address = cpustate->pc;

	if( cpustate->pc & 0x3 ) {		/* Unaligned read */
		value = (FETCH(cpustate) << 0) |
				(FETCH(cpustate) << 8) |
				(FETCH(cpustate) << 16) |
				(FETCH(cpustate) << 24);
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		{
			UINT8 *p = i386_fast_ptr(cpustate, address, 4, I386_FAST_FETCH);
			value = p ? *(UINT32 *)p : cpu_readmem32ledw_dword(address);
		}
		cpustate->eip += 4;
		cpustate->pc += 4;
	}
	return value;
}

/* the fetches: straight from the fast memory when the address is in the */
/* cached region and paging is off, else through the handlers */
#ifndef MSB_FIRST
#define FAST_FETCH_OFFSET(cs, n) \
	UINT32 off = ((cs)->pc & (cs)->a20_mask) - (cs)->fast_start[I386_FAST_FETCH]; \
	if (off < (cs)->fast_size[I386_FAST_FETCH] && (cs)->fast_size[I386_FAST_FETCH] - off >= (n) && !((cs)->cr[0] & 0x80000000))
#endif

static INLINE UINT8 FETCH(i386_state *cpustate)
{
#ifndef MSB_FIRST
	FAST_FETCH_OFFSET(cpustate, 1)
	{
		cpustate->eip++;
		cpustate->pc++;
		return cpustate->fast_base[I386_FAST_FETCH][off];
	}
#endif
	return FETCH_slow(cpustate);
}

static INLINE UINT16 FETCH16(i386_state *cpustate)
{
#ifndef MSB_FIRST
	FAST_FETCH_OFFSET(cpustate, 2)
	{
		const UINT8 *p = cpustate->fast_base[I386_FAST_FETCH] + off;
		cpustate->eip += 2;
		cpustate->pc += 2;
		return p[0] | (p[1] << 8);
	}
#endif
	return FETCH16_slow(cpustate);
}

static INLINE UINT32 FETCH32(i386_state *cpustate)
{
#ifndef MSB_FIRST
	FAST_FETCH_OFFSET(cpustate, 4)
	{
		const UINT8 *p = cpustate->fast_base[I386_FAST_FETCH] + off;
		cpustate->eip += 4;
		cpustate->pc += 4;
		return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
	}
#endif
	return FETCH32_slow(cpustate);
}

static INLINE UINT8 READ8(i386_state *cpustate,UINT32 ea);
static INLINE void WRITE8(i386_state *cpustate,UINT32 ea, UINT8 value);

static UINT8 READ8_slow(i386_state *cpustate,UINT32 ea)
{
	UINT32 address = ea;

	if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
	{
		translate_address(cpustate,&address);
	}

	address &= cpustate->a20_mask;
	{
		UINT8 *p = i386_fast_ptr(cpustate, address, 1, I386_FAST_READ);
		return p ? *p : cpu_readmem32ledw(address);
	}
}
static UINT16 READ16_slow(i386_state *cpustate,UINT32 ea)
{
	UINT16 value;
	UINT32 address = ea;

	if( ea & 0x1 ) {		/* Unaligned read */
		value = (READ8( cpustate, address+0 ) << 0) |
				(READ8( cpustate, address+1 ) << 8);
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		{
			UINT8 *p = i386_fast_ptr(cpustate, address, 2, I386_FAST_READ);
			value = p ? *(UINT16 *)p : cpu_readmem32ledw_word(address);
		}
	}
	return value;
}
static UINT32 READ32_slow(i386_state *cpustate,UINT32 ea)
{
	UINT32 value;
	UINT32 address = ea;

	if( ea & 0x3 ) {		/* Unaligned read */
		value = (READ8( cpustate, address+0 ) << 0) |
				(READ8( cpustate, address+1 ) << 8) |
				(READ8( cpustate, address+2 ) << 16) |
				(READ8( cpustate, address+3 ) << 24);
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		{
			UINT8 *p = i386_fast_ptr(cpustate, address, 4, I386_FAST_READ);
			value = p ? *(UINT32 *)p : cpu_readmem32ledw_dword(address);
		}
	}
	return value;
}

static INLINE UINT64 READ64(i386_state *cpustate,UINT32 ea)
{
	UINT64 value;
	UINT32 address = ea;

	if( ea & 0x7 ) {		/* Unaligned read */
		value = (((UINT64) READ8( cpustate, address+0 )) << 0) |
				(((UINT64) READ8( cpustate, address+1 )) << 8) |
				(((UINT64) READ8( cpustate, address+2 )) << 16) |
				(((UINT64) READ8( cpustate, address+3 )) << 24) |
				(((UINT64) READ8( cpustate, address+4 )) << 32) |
				(((UINT64) READ8( cpustate, address+5 )) << 40) |
				(((UINT64) READ8( cpustate, address+6 )) << 48) |
				(((UINT64) READ8( cpustate, address+7 )) << 56);
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		value = (((UINT64) cpu_readmem32ledw_dword(address+0 )) << 0) |
				(((UINT64) cpu_readmem32ledw_dword(address+4 )) << 32);
	}
	return value;
}

static void WRITE8_slow(i386_state *cpustate,UINT32 ea, UINT8 value)
{
	UINT32 address = ea;

	if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
	{
		translate_address(cpustate,&address);
	}

	address &= cpustate->a20_mask;
	{
		UINT8 *p = i386_fast_ptr(cpustate, address, 1, I386_FAST_WRITE);
		if (p)
			*p = value;
		else
			cpu_writemem32ledw(address, value);
	}
}
static void WRITE16_slow(i386_state *cpustate,UINT32 ea, UINT16 value)
{
	UINT32 address = ea;

	if( ea & 0x1 ) {		/* Unaligned write */
		WRITE8( cpustate, address+0, value & 0xff );
		WRITE8( cpustate, address+1, (value >> 8) & 0xff );
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		{
			UINT8 *p = i386_fast_ptr(cpustate, address, 2, I386_FAST_WRITE);
			if (p)
				*(UINT16 *)p = value;
			else
				cpu_writemem32ledw_word(address, value);
		}
	}
}
static void WRITE32_slow(i386_state *cpustate,UINT32 ea, UINT32 value)
{
	UINT32 address = ea;

	if( ea & 0x3 ) {		/* Unaligned write */
		WRITE8( cpustate, address+0, value & 0xff );
		WRITE8( cpustate, address+1, (value >> 8) & 0xff );
		WRITE8( cpustate, address+2, (value >> 16) & 0xff );
		WRITE8( cpustate, address+3, (value >> 24) & 0xff );
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		{
			UINT8 *p = i386_fast_ptr(cpustate, address, 4, I386_FAST_WRITE);
			if (p)
				*(UINT32 *)p = value;
			else
				cpu_writemem32ledw_dword(address, value);
		}
	}
}

/* the data accesses: straight from the fast memory when the address is */
/* in the cached region of the kind and paging is off, else the handlers */
#ifndef MSB_FIRST
#define FAST_DATA_OFFSET(cs, ea, n, kind) \
	UINT32 off = ((ea) & (cs)->a20_mask) - (cs)->fast_start[kind]; \
	if (off < (cs)->fast_size[kind] && (cs)->fast_size[kind] - off >= (n) && !((cs)->cr[0] & 0x80000000))
#endif

static INLINE UINT8 READ8(i386_state *cpustate,UINT32 ea)
{
#ifndef MSB_FIRST
	FAST_DATA_OFFSET(cpustate, ea, 1, I386_FAST_READ)
		return cpustate->fast_base[I386_FAST_READ][off];
#endif
	return READ8_slow(cpustate, ea);
}

static INLINE UINT16 READ16(i386_state *cpustate,UINT32 ea)
{
#ifndef MSB_FIRST
	FAST_DATA_OFFSET(cpustate, ea, 2, I386_FAST_READ)
	{
		const UINT8 *p = cpustate->fast_base[I386_FAST_READ] + off;
		return p[0] | (p[1] << 8);
	}
#endif
	return READ16_slow(cpustate, ea);
}

static INLINE UINT32 READ32(i386_state *cpustate,UINT32 ea)
{
#ifndef MSB_FIRST
	FAST_DATA_OFFSET(cpustate, ea, 4, I386_FAST_READ)
	{
		const UINT8 *p = cpustate->fast_base[I386_FAST_READ] + off;
		return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
	}
#endif
	return READ32_slow(cpustate, ea);
}

static INLINE void WRITE8(i386_state *cpustate,UINT32 ea, UINT8 value)
{
#ifndef MSB_FIRST
	FAST_DATA_OFFSET(cpustate, ea, 1, I386_FAST_WRITE)
	{
		cpustate->fast_base[I386_FAST_WRITE][off] = value;
		return;
	}
#endif
	WRITE8_slow(cpustate, ea, value);
}

static INLINE void WRITE16(i386_state *cpustate,UINT32 ea, UINT16 value)
{
#ifndef MSB_FIRST
	FAST_DATA_OFFSET(cpustate, ea, 2, I386_FAST_WRITE)
	{
		UINT8 *p = cpustate->fast_base[I386_FAST_WRITE] + off;
		p[0] = value;
		p[1] = value >> 8;
		return;
	}
#endif
	WRITE16_slow(cpustate, ea, value);
}

static INLINE void WRITE32(i386_state *cpustate,UINT32 ea, UINT32 value)
{
#ifndef MSB_FIRST
	FAST_DATA_OFFSET(cpustate, ea, 4, I386_FAST_WRITE)
	{
		UINT8 *p = cpustate->fast_base[I386_FAST_WRITE] + off;
		p[0] = value;
		p[1] = value >> 8;
		p[2] = value >> 16;
		p[3] = value >> 24;
		return;
	}
#endif
	WRITE32_slow(cpustate, ea, value);
}

static INLINE void WRITE64(i386_state *cpustate,UINT32 ea, UINT64 value)
{
	UINT32 address = ea;

	if( ea & 0x7 ) {		/* Unaligned write */
		WRITE8( cpustate, address+0, value & 0xff );
		WRITE8( cpustate, address+1, (value >> 8) & 0xff );
		WRITE8( cpustate, address+2, (value >> 16) & 0xff );
		WRITE8( cpustate, address+3, (value >> 24) & 0xff );
		WRITE8( cpustate, address+4, (value >> 32) & 0xff );
		WRITE8( cpustate, address+5, (value >> 40) & 0xff );
		WRITE8( cpustate, address+6, (value >> 48) & 0xff );
		WRITE8( cpustate, address+7, (value >> 56) & 0xff );
	} else {
		if (cpustate->cr[0] & 0x80000000)		/* page translation enabled */
		{
			translate_address(cpustate,&address);
		}

		address &= cpustate->a20_mask;
		cpu_writemem32ledw_dword(address+0, value & 0xffffffff);
		cpu_writemem32ledw_dword(address+4, (value >> 32) & 0xffffffff);
	}
}

/***********************************************************************************/

static INLINE UINT8 OR8(i386_state *cpustate,UINT8 dst, UINT8 src)
{
	UINT8 res = dst | src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF8(res);
	return res;
}
static INLINE UINT16 OR16(i386_state *cpustate,UINT16 dst, UINT16 src)
{
	UINT16 res = dst | src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF16(res);
	return res;
}
static INLINE UINT32 OR32(i386_state *cpustate,UINT32 dst, UINT32 src)
{
	UINT32 res = dst | src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF32(res);
	return res;
}

static INLINE UINT8 AND8(i386_state *cpustate,UINT8 dst, UINT8 src)
{
	UINT8 res = dst & src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF8(res);
	return res;
}
static INLINE UINT16 AND16(i386_state *cpustate,UINT16 dst, UINT16 src)
{
	UINT16 res = dst & src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF16(res);
	return res;
}
static INLINE UINT32 AND32(i386_state *cpustate,UINT32 dst, UINT32 src)
{
	UINT32 res = dst & src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF32(res);
	return res;
}

static INLINE UINT8 XOR8(i386_state *cpustate,UINT8 dst, UINT8 src)
{
	UINT8 res = dst ^ src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF8(res);
	return res;
}
static INLINE UINT16 XOR16(i386_state *cpustate,UINT16 dst, UINT16 src)
{
	UINT16 res = dst ^ src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF16(res);
	return res;
}
static INLINE UINT32 XOR32(i386_state *cpustate,UINT32 dst, UINT32 src)
{
	UINT32 res = dst ^ src;
	cpustate->CF = cpustate->OF = 0;
	SetSZPF32(res);
	return res;
}

#define SUB8(cpu, dst, src) SBB8(cpu, dst, src, 0)
static INLINE UINT8 SBB8(i386_state *cpustate,UINT8 dst, UINT8 src, UINT8 b)
{
	UINT16 res = (UINT16)dst - (UINT16)src - (UINT8)b;
	SetCF8(res);
	SetOF_Sub8(res,src,dst);
	SetAF(res,src,dst);
	SetSZPF8(res);
	return (UINT8)res;
}

#define SUB16(cpu, dst, src) SBB16(cpu, dst, src, 0)
static INLINE UINT16 SBB16(i386_state *cpustate,UINT16 dst, UINT16 src, UINT16 b)
{
	UINT32 res = (UINT32)dst - (UINT32)src - (UINT32)b;
	SetCF16(res);
	SetOF_Sub16(res,src,dst);
	SetAF(res,src,dst);
	SetSZPF16(res);
	return (UINT16)res;
}

#define SUB32(cpu, dst, src) SBB32(cpu, dst, src, 0)
static INLINE UINT32 SBB32(i386_state *cpustate,UINT32 dst, UINT32 src, UINT32 b)
{
	UINT64 res = (UINT64)dst - (UINT64)src - (UINT64) b;
	SetCF32(res);
	SetOF_Sub32(res,src,dst);
	SetAF(res,src,dst);
	SetSZPF32(res);
	return (UINT32)res;
}

#define ADD8(cpu, dst, src) ADC8(cpu, dst, src, 0)
static INLINE UINT8 ADC8(i386_state *cpustate,UINT8 dst, UINT8 src, UINT8 c)
{
	UINT16 res = (UINT16)dst + (UINT16)src + (UINT16)c;
	SetCF8(res);
	SetOF_Add8(res,src,dst);
	SetAF(res,src,dst);
	SetSZPF8(res);
	return (UINT8)res;
}

#define ADD16(cpu, dst, src) ADC16(cpu, dst, src, 0)
static INLINE UINT16 ADC16(i386_state *cpustate,UINT16 dst, UINT16 src, UINT8 c)
{
	UINT32 res = (UINT32)dst + (UINT32)src + (UINT32)c;
	SetCF16(res);
	SetOF_Add16(res,src,dst);
	SetAF(res,src,dst);
	SetSZPF16(res);
	return (UINT16)res;
}

#define ADD32(cpu, dst, src) ADC32(cpu, dst, src, 0)
static INLINE UINT32 ADC32(i386_state *cpustate,UINT32 dst, UINT32 src, UINT32 c)
{
	UINT64 res = (UINT64)dst + (UINT64)src + (UINT64) c;
	SetCF32(res);
	SetOF_Add32(res,src,dst);
	SetAF(res,src,dst);
	SetSZPF32(res);
	return (UINT32)res;
}

static INLINE UINT8 INC8(i386_state *cpustate,UINT8 dst)
{
	UINT16 res = (UINT16)dst + 1;
	SetOF_Add8(res,1,dst);
	SetAF(res,1,dst);
	SetSZPF8(res);
	return (UINT8)res;
}
static INLINE UINT16 INC16(i386_state *cpustate,UINT16 dst)
{
	UINT32 res = (UINT32)dst + 1;
	SetOF_Add16(res,1,dst);
	SetAF(res,1,dst);
	SetSZPF16(res);
	return (UINT16)res;
}
static INLINE UINT32 INC32(i386_state *cpustate,UINT32 dst)
{
	UINT64 res = (UINT64)dst + 1;
	SetOF_Add32(res,1,dst);
	SetAF(res,1,dst);
	SetSZPF32(res);
	return (UINT32)res;
}

static INLINE UINT8 DEC8(i386_state *cpustate,UINT8 dst)
{
	UINT16 res = (UINT16)dst - 1;
	SetOF_Sub8(res,1,dst);
	SetAF(res,1,dst);
	SetSZPF8(res);
	return (UINT8)res;
}
static INLINE UINT16 DEC16(i386_state *cpustate,UINT16 dst)
{
	UINT32 res = (UINT32)dst - 1;
	SetOF_Sub16(res,1,dst);
	SetAF(res,1,dst);
	SetSZPF16(res);
	return (UINT16)res;
}
static INLINE UINT32 DEC32(i386_state *cpustate,UINT32 dst)
{
	UINT64 res = (UINT64)dst - 1;
	SetOF_Sub32(res,1,dst);
	SetAF(res,1,dst);
	SetSZPF32(res);
	return (UINT32)res;
}



static INLINE void PUSH16(i386_state *cpustate,UINT16 value)
{
	UINT32 ea;
	if( STACK_32BIT ) {
		REG32(ESP) -= 2;
		ea = i386_translate(cpustate, SS, REG32(ESP) );
		WRITE16(cpustate, ea, value );
	} else {
		REG16(SP) -= 2;
		ea = i386_translate(cpustate, SS, REG16(SP) );
		WRITE16(cpustate, ea, value );
	}
}
static INLINE void PUSH32(i386_state *cpustate,UINT32 value)
{
	UINT32 ea;
	if( STACK_32BIT ) {
		REG32(ESP) -= 4;
		ea = i386_translate(cpustate, SS, REG32(ESP) );
		WRITE32(cpustate, ea, value );
	} else {
		REG16(SP) -= 4;
		ea = i386_translate(cpustate, SS, REG16(SP) );
		WRITE32(cpustate, ea, value );
	}
}
static INLINE void PUSH8(i386_state *cpustate,UINT8 value)
{
	if( cpustate->operand_size ) {
		PUSH32(cpustate,(INT32)(INT8)value);
	} else {
		PUSH16(cpustate,(INT16)(INT8)value);
	}
}

static INLINE UINT8 POP8(i386_state *cpustate)
{
	UINT8 value;
	UINT32 ea;
	if( STACK_32BIT ) {
		ea = i386_translate(cpustate, SS, REG32(ESP) );
		value = READ8(cpustate, ea );
		REG32(ESP) += 1;
	} else {
		ea = i386_translate(cpustate, SS, REG16(SP) );
		value = READ8(cpustate, ea );
		REG16(SP) += 1;
	}
	return value;
}
static INLINE UINT16 POP16(i386_state *cpustate)
{
	UINT16 value;
	UINT32 ea;
	if( STACK_32BIT ) {
		ea = i386_translate(cpustate, SS, REG32(ESP) );
		value = READ16(cpustate, ea );
		REG32(ESP) += 2;
	} else {
		ea = i386_translate(cpustate, SS, REG16(SP) );
		value = READ16(cpustate, ea );
		REG16(SP) += 2;
	}
	return value;
}
static INLINE UINT32 POP32(i386_state *cpustate)
{
	UINT32 value;
	UINT32 ea;
	if( STACK_32BIT ) {
		ea = i386_translate(cpustate, SS, REG32(ESP) );
		value = READ32(cpustate, ea );
		REG32(ESP) += 4;
	} else {
		ea = i386_translate(cpustate, SS, REG16(SP) );
		value = READ32(cpustate, ea );
		REG16(SP) += 4;
	}
	return value;
}

static INLINE void BUMP_SI(i386_state *cpustate,int adjustment)
{
	if ( cpustate->address_size )
		REG32(ESI) += ((cpustate->DF) ? -adjustment : +adjustment);
	else
		REG16(SI) += ((cpustate->DF) ? -adjustment : +adjustment);
}

static INLINE void BUMP_DI(i386_state *cpustate,int adjustment)
{
	if ( cpustate->address_size )
		REG32(EDI) += ((cpustate->DF) ? -adjustment : +adjustment);
	else
		REG16(DI) += ((cpustate->DF) ? -adjustment : +adjustment);
}



/***********************************************************************************/

#define READPORT8(port)		    	(cpu_readport16ledw(port))
#define READPORT16(port)	    	(cpu_readport16ledw_word(port))
#define READPORT32(port)	    	(cpu_readport16ledw_dword(port))
#define WRITEPORT8(port, value)		(cpu_writeport16ledw(port, value))
#define WRITEPORT16(port, value)	(cpu_writeport16ledw_word(port, value))
#define WRITEPORT32(port, value)	(cpu_writeport16ledw_dword(port, value))

#endif /* __I386_H__ */
