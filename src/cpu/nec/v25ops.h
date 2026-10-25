/* Generated from MAME 0.264 src/devices/cpu/nec (necmacro.h, necea.h, necmodrm.h,
   necinstr.hxx, v25instr.hxx, v25instr.h) for the C V25 core: do not edit
   by hand, edit v25.c or regenerate. license:BSD-3-Clause
   copyright-holders:Bryan McPhail, Alex W. Jackson */

static void i_add_br8(void);
static void i_add_wr16(void);
static void i_add_r8b(void);
static void i_add_r16w(void);
static void i_add_ald8(void);
static void i_add_axd16(void);
static void i_push_es(void);
static void i_pop_es(void);
static void i_or_br8(void);
static void i_or_wr16(void);
static void i_or_r8b(void);
static void i_or_r16w(void);
static void i_or_ald8(void);
static void i_or_axd16(void);
static void i_push_cs(void);
static void i_adc_br8(void);
static void i_adc_wr16(void);
static void i_adc_r8b(void);
static void i_adc_r16w(void);
static void i_adc_ald8(void);
static void i_adc_axd16(void);
static void i_push_ss(void);
static void i_pop_ss(void);
static void i_sbb_br8(void);
static void i_sbb_wr16(void);
static void i_sbb_r8b(void);
static void i_sbb_r16w(void);
static void i_sbb_ald8(void);
static void i_sbb_axd16(void);
static void i_push_ds(void);
static void i_pop_ds(void);
static void i_and_br8(void);
static void i_and_wr16(void);
static void i_and_r8b(void);
static void i_and_r16w(void);
static void i_and_ald8(void);
static void i_and_axd16(void);
static void i_es(void);
static void i_daa(void);
static void i_sub_br8(void);
static void i_sub_wr16(void);
static void i_sub_r8b(void);
static void i_sub_r16w(void);
static void i_sub_ald8(void);
static void i_sub_axd16(void);
static void i_cs(void);
static void i_das(void);
static void i_xor_br8(void);
static void i_xor_wr16(void);
static void i_xor_r8b(void);
static void i_xor_r16w(void);
static void i_xor_ald8(void);
static void i_xor_axd16(void);
static void i_ss(void);
static void i_aaa(void);
static void i_cmp_br8(void);
static void i_cmp_wr16(void);
static void i_cmp_r8b(void);
static void i_cmp_r16w(void);
static void i_cmp_ald8(void);
static void i_cmp_axd16(void);
static void i_ds(void);
static void i_aas(void);
static void i_inc_ax(void);
static void i_inc_cx(void);
static void i_inc_dx(void);
static void i_inc_bx(void);
static void i_inc_sp(void);
static void i_inc_bp(void);
static void i_inc_si(void);
static void i_inc_di(void);
static void i_dec_ax(void);
static void i_dec_cx(void);
static void i_dec_dx(void);
static void i_dec_bx(void);
static void i_dec_sp(void);
static void i_dec_bp(void);
static void i_dec_si(void);
static void i_dec_di(void);
static void i_push_ax(void);
static void i_push_cx(void);
static void i_push_dx(void);
static void i_push_bx(void);
static void i_push_sp(void);
static void i_push_bp(void);
static void i_push_si(void);
static void i_push_di(void);
static void i_pop_ax(void);
static void i_pop_cx(void);
static void i_pop_dx(void);
static void i_pop_bx(void);
static void i_pop_sp(void);
static void i_pop_bp(void);
static void i_pop_si(void);
static void i_pop_di(void);
static void i_pusha(void);
static void i_popa(void);
static void i_chkind(void);
static void i_repnc(void);
static void i_repc(void);
static void i_push_d16(void);
static void i_imul_d16(void);
static void i_push_d8(void);
static void i_imul_d8(void);
static void i_insb(void);
static void i_insw(void);
static void i_outsb(void);
static void i_outsw(void);
static void i_jo(void);
static void i_jno(void);
static void i_jc(void);
static void i_jnc(void);
static void i_jz(void);
static void i_jnz(void);
static void i_jce(void);
static void i_jnce(void);
static void i_js(void);
static void i_jns(void);
static void i_jp(void);
static void i_jnp(void);
static void i_jl(void);
static void i_jnl(void);
static void i_jle(void);
static void i_jnle(void);
static void i_80pre(void);
static void i_81pre(void);
static void i_82pre(void);
static void i_83pre(void);
static void i_test_br8(void);
static void i_test_wr16(void);
static void i_xchg_br8(void);
static void i_xchg_wr16(void);
static void i_mov_br8(void);
static void i_mov_wr16(void);
static void i_mov_r8b(void);
static void i_mov_r16w(void);
static void i_mov_wsreg(void);
static void i_lea(void);
static void i_mov_sregw(void);
static void i_popw(void);
static void i_nop(void);
static void i_xchg_axcx(void);
static void i_xchg_axdx(void);
static void i_xchg_axbx(void);
static void i_xchg_axsp(void);
static void i_xchg_axbp(void);
static void i_xchg_axsi(void);
static void i_xchg_axdi(void);
static void i_cbw(void);
static void i_cwd(void);
static void i_call_far(void);
static void i_wait(void);
static void i_pushf(void);
static void i_popf(void);
static void i_sahf(void);
static void i_lahf(void);
static void i_mov_aldisp(void);
static void i_mov_axdisp(void);
static void i_mov_dispal(void);
static void i_mov_dispax(void);
static void i_movsb(void);
static void i_movsw(void);
static void i_cmpsb(void);
static void i_cmpsw(void);
static void i_test_ald8(void);
static void i_test_axd16(void);
static void i_stosb(void);
static void i_stosw(void);
static void i_lodsb(void);
static void i_lodsw(void);
static void i_scasb(void);
static void i_scasw(void);
static void i_mov_ald8(void);
static void i_mov_cld8(void);
static void i_mov_dld8(void);
static void i_mov_bld8(void);
static void i_mov_ahd8(void);
static void i_mov_chd8(void);
static void i_mov_dhd8(void);
static void i_mov_bhd8(void);
static void i_mov_axd16(void);
static void i_mov_cxd16(void);
static void i_mov_dxd16(void);
static void i_mov_bxd16(void);
static void i_mov_spd16(void);
static void i_mov_bpd16(void);
static void i_mov_sid16(void);
static void i_mov_did16(void);
static void i_rotshft_bd8(void);
static void i_rotshft_wd8(void);
static void i_ret_d16(void);
static void i_ret(void);
static void i_les_dw(void);
static void i_lds_dw(void);
static void i_mov_bd8(void);
static void i_mov_wd16(void);
static void i_enter(void);
static void i_leave(void);
static void i_retf_d16(void);
static void i_retf(void);
static void i_int3(void);
static void i_int(void);
static void i_into(void);
static void i_iret(void);
static void i_rotshft_b(void);
static void i_rotshft_w(void);
static void i_rotshft_bcl(void);
static void i_rotshft_wcl(void);
static void i_aam(void);
static void i_aad(void);
static void i_setalc(void);
static void i_trans(void);
static void i_fpo(void);
static void i_loopne(void);
static void i_loope(void);
static void i_loop(void);
static void i_jcxz(void);
static void i_inal(void);
static void i_inax(void);
static void i_outal(void);
static void i_outax(void);
static void i_call_d16(void);
static void i_jmp_d16(void);
static void i_jmp_far(void);
static void i_jmp_d8(void);
static void i_inaldx(void);
static void i_inaxdx(void);
static void i_outdxal(void);
static void i_outdxax(void);
static void i_lock(void);
static void i_repne(void);
static void i_repe(void);
static void i_hlt(void);
static void i_cmc(void);
static void i_f6pre(void);
static void i_f7pre(void);
static void i_clc(void);
static void i_stc(void);
static void i_di(void);
static void i_ei(void);
static void i_cld(void);
static void i_std(void);
static void i_fepre(void);
static void i_ffpre(void);
static void i_pre_v25(void);
static void i_brkn(void);
static void i_brks(void);
static void i_invalid(void);

static UINT32 EA_000(void);
static UINT32 EA_001(void);
static UINT32 EA_002(void);
static UINT32 EA_003(void);
static UINT32 EA_004(void);
static UINT32 EA_005(void);
static UINT32 EA_006(void);
static UINT32 EA_007(void);
static UINT32 EA_100(void);
static UINT32 EA_101(void);
static UINT32 EA_102(void);
static UINT32 EA_103(void);
static UINT32 EA_104(void);
static UINT32 EA_105(void);
static UINT32 EA_106(void);
static UINT32 EA_107(void);
static UINT32 EA_200(void);
static UINT32 EA_201(void);
static UINT32 EA_202(void);
static UINT32 EA_203(void);
static UINT32 EA_204(void);
static UINT32 EA_205(void);
static UINT32 EA_206(void);
static UINT32 EA_207(void);

/* the tables, defined at the end (not static, for the forward declarations) */
extern void (*const v25_instruction[256])(void);
extern UINT32 (*const v25_GetEA[192])(void);

/* parameter x = result, y = source 1, z = source 2 */

#define SetTF(x)        (V.TF = (x))
#define SetIF(x)        (V.IF = (x))
#define SetDF(x)        (V.DF = (x))
#define SetMD(x)        (V.MF = (x))   /* OB [19.07.99] Mode Flag V30 */

#define SetCFB(x)       (V.CarryVal = (x) & 0x100)
#define SetCFW(x)       (V.CarryVal = (x) & 0x10000)
#define SetAF(x,y,z)    (V.AuxVal = ((x) ^ ((y) ^ (z))) & 0x10)
#define SetSF(x)        (V.SignVal = (x))
#define SetZF(x)        (V.ZeroVal = (x))
#define SetPF(x)        (V.ParityVal = (x))

#define SetSZPF_Byte(x) (V.SignVal=V.ZeroVal=V.ParityVal=(INT8)(x))
#define SetSZPF_Word(x) (V.SignVal=V.ZeroVal=V.ParityVal=(INT16)(x))

#define SetOFW_Add(x,y,z)   (V.OverVal = ((x) ^ (y)) & ((x) ^ (z)) & 0x8000)
#define SetOFB_Add(x,y,z)   (V.OverVal = ((x) ^ (y)) & ((x) ^ (z)) & 0x80)
#define SetOFW_Sub(x,y,z)   (V.OverVal = ((z) ^ (y)) & ((z) ^ (x)) & 0x8000)
#define SetOFB_Sub(x,y,z)   (V.OverVal = ((z) ^ (y)) & ((z) ^ (x)) & 0x80)

#define ADDB { UINT32 res=dst+src; SetCFB(res); SetOFB_Add(res,src,dst); SetAF(res,src,dst); SetSZPF_Byte(res); dst=(BYTE)res; }
#define ADDW { UINT32 res=dst+src; SetCFW(res); SetOFW_Add(res,src,dst); SetAF(res,src,dst); SetSZPF_Word(res); dst=(WORD)res; }

#define SUBB { UINT32 res=dst-src; SetCFB(res); SetOFB_Sub(res,src,dst); SetAF(res,src,dst); SetSZPF_Byte(res); dst=(BYTE)res; }
#define SUBW { UINT32 res=dst-src; SetCFW(res); SetOFW_Sub(res,src,dst); SetAF(res,src,dst); SetSZPF_Word(res); dst=(WORD)res; }

#define ORB dst|=src; V.CarryVal=V.OverVal=V.AuxVal=0; SetSZPF_Byte(dst)
#define ORW dst|=src; V.CarryVal=V.OverVal=V.AuxVal=0; SetSZPF_Word(dst)

#define ANDB dst&=src; V.CarryVal=V.OverVal=V.AuxVal=0; SetSZPF_Byte(dst)
#define ANDW dst&=src; V.CarryVal=V.OverVal=V.AuxVal=0; SetSZPF_Word(dst)

#define XORB dst^=src; V.CarryVal=V.OverVal=V.AuxVal=0; SetSZPF_Byte(dst)
#define XORW dst^=src; V.CarryVal=V.OverVal=V.AuxVal=0; SetSZPF_Word(dst)

#define IncWordReg(Reg)                     \
	unsigned tmp = (unsigned)Wreg(Reg); \
	unsigned tmp1 = tmp+1;                  \
	V.OverVal = (tmp == 0x7fff);           \
	SetAF(tmp1,tmp,1);                      \
	SetSZPF_Word(tmp1);                     \
	Wreg(Reg)=tmp1

#define DecWordReg(Reg)                     \
	unsigned tmp = (unsigned)Wreg(Reg); \
	unsigned tmp1 = tmp-1;                  \
	V.OverVal = (tmp == 0x8000);           \
	SetAF(tmp1,tmp,1);                      \
	SetSZPF_Word(tmp1);                     \
	Wreg(Reg)=tmp1

#define IncByteReg(Reg)                     \
	unsigned tmp = (unsigned)Breg(Reg); \
	unsigned tmp1 = tmp+1;                  \
	V.OverVal = (tmp == 0x7f);           \
	SetAF(tmp1,tmp,1);                      \
	SetSZPF_Byte(tmp1);                     \
	Breg(Reg)=tmp1

#define DecByteReg(Reg)                     \
	unsigned tmp = (unsigned)Breg(Reg); \
	unsigned tmp1 = tmp-1;                  \
	V.OverVal = (tmp == 0x80);           \
	SetAF(tmp1,tmp,1);                      \
	SetSZPF_Byte(tmp1);                     \
	Breg(Reg)=tmp1

#define JMP(flag)                           \
	int tmp;                                \
	EMPTY_PREfetch();                       \
	tmp = (int)((INT8)fetch());             \
	if (flag)                               \
	{                                       \
		static const UINT8 table[3]={3,10,10};  \
		V.ip = (WORD)(V.ip+tmp);          \
		v25_ICount-=table[V.chip_type/8];   \
		CHANGE_PC;                          \
		return;                             \
	}

#define ADJ4(param1,param2)                 \
	if (AF || ((Breg(AL) & 0xf) > 9))   \
	{                                       \
		UINT16 tmp;                         \
		tmp = Breg(AL) + param1;        \
		Breg(AL) = tmp;                 \
		V.AuxVal = 1;                      \
		V.CarryVal |= tmp & 0x100;         \
	}                                       \
	if (CF || (Breg(AL)>0x9f))          \
	{                                       \
		Breg(AL) += param2;             \
		V.CarryVal = 1;                        \
	}                                       \
	SetSZPF_Byte(Breg(AL))

#define ADJB(param1,param2)                 \
	if (AF || ((Breg(AL) & 0xf) > 9))   \
	{                                       \
		Breg(AL) += param1;             \
		Breg(AH) += param2;             \
		V.AuxVal = 1;                      \
		V.CarryVal = 1;                        \
	}                                       \
	else                                    \
	{                                       \
		V.AuxVal = 0;                      \
		V.CarryVal = 0;                        \
	}                                       \
	Breg(AL) &= 0x0F

#define BITOP_BYTE                          \
	ModRM = fetch();                            \
	if (ModRM >= 0xc0) {                    \
		tmp=Breg(Mod_RM.RM.b[ModRM]);   \
	}                                       \
	else {                                  \
		v25_GetEA[ModRM]();                 \
		tmp=read_mem_byte(V.EA);                  \
	}

#define BITOP_WORD                          \
	ModRM = fetch();                            \
	if (ModRM >= 0xc0) {                    \
		tmp=Wreg(Mod_RM.RM.w[ModRM]);   \
	}                                       \
	else {                                  \
		v25_GetEA[ModRM]();                 \
		tmp=read_mem_word(V.EA);                  \
	}

#define BIT_NOT                             \
	if (tmp & (1<<tmp2))                    \
		tmp &= ~(1<<tmp2);                  \
	else                                    \
		tmp |= (1<<tmp2)

#define XchgAWReg(Reg)                      \
	WORD tmp;                               \
	tmp = Wreg(Reg);                    \
	Wreg(Reg) = Wreg(AW);           \
	Wreg(AW) = tmp

#define ROL_BYTE V.CarryVal = dst & 0x80; dst = (dst << 1)+CF
#define ROL_WORD V.CarryVal = dst & 0x8000; dst = (dst << 1)+CF
#define ROR_BYTE V.CarryVal = dst & 0x1; dst = (dst >> 1)+(CF<<7)
#define ROR_WORD V.CarryVal = dst & 0x1; dst = (dst >> 1)+(CF<<15)
#define ROLC_BYTE dst = (dst << 1) + CF; SetCFB(dst)
#define ROLC_WORD dst = (dst << 1) + CF; SetCFW(dst)
#define RORC_BYTE dst = (CF<<8)+dst; V.CarryVal = dst & 0x01; dst >>= 1
#define RORC_WORD dst = (CF<<16)+dst; V.CarryVal = dst & 0x01; dst >>= 1
#define SHL_BYTE(c) v25_ICount-=c; dst <<= c;    SetCFB(dst); SetSZPF_Byte(dst); PutbackRMByte(ModRM,(BYTE)dst)
#define SHL_WORD(c) v25_ICount-=c; dst <<= c;    SetCFW(dst); SetSZPF_Word(dst); PutbackRMWord(ModRM,(WORD)dst)
#define SHR_BYTE(c) v25_ICount-=c; dst >>= c-1; V.CarryVal = dst & 0x1; dst >>= 1; SetSZPF_Byte(dst); PutbackRMByte(ModRM,(BYTE)dst)
#define SHR_WORD(c) v25_ICount-=c; dst >>= c-1; V.CarryVal = dst & 0x1; dst >>= 1; SetSZPF_Word(dst); PutbackRMWord(ModRM,(WORD)dst)
#define SHRA_BYTE(c) v25_ICount-=c; dst = ((INT8)dst) >> (c-1);  V.CarryVal = dst & 0x1;    dst = ((INT8)((BYTE)dst)) >> 1; SetSZPF_Byte(dst); PutbackRMByte(ModRM,(BYTE)dst)
#define SHRA_WORD(c) v25_ICount-=c; dst = ((INT16)dst) >> (c-1); V.CarryVal = dst & 0x1;    dst = ((INT16)((WORD)dst)) >> 1; SetSZPF_Word(dst); PutbackRMWord(ModRM,(WORD)dst)

#define DIVUB                                               \
	uresult = Wreg(AW);                                 \
	uresult2 = uresult % tmp;                               \
	if ((uresult /= tmp) > 0xff) {                          \
		nec_interrupt(NEC_DIVIDE_VECTOR, BRK); break;                            \
	} else {                                                \
		Breg(AL) = uresult;                             \
		Breg(AH) = uresult2;                            \
	}

#define DIVB                                                \
	result = (INT16)Wreg(AW);                           \
	result2 = result % (INT16)((INT8)tmp);                  \
	if ((result /= (INT16)((INT8)tmp)) > 0xff) {            \
		nec_interrupt(NEC_DIVIDE_VECTOR, BRK); break;                            \
	} else {                                                \
		Breg(AL) = result;                              \
		Breg(AH) = result2;                             \
	}

#define DIVUW                                               \
	uresult = (((UINT32)Wreg(DW)) << 16) | Wreg(AW);\
	uresult2 = uresult % tmp;                               \
	if ((uresult /= tmp) > 0xffff) {                        \
		nec_interrupt(NEC_DIVIDE_VECTOR, BRK); break;                            \
	} else {                                                \
		Wreg(AW)=uresult;                               \
		Wreg(DW)=uresult2;                              \
	}

#define DIVW                                                \
	result = ((UINT32)Wreg(DW) << 16) + Wreg(AW);   \
	result2 = result % (INT32)((INT16)tmp);                 \
	if ((result /= (INT32)((INT16)tmp)) > 0xffff) {         \
		nec_interrupt(NEC_DIVIDE_VECTOR, BRK); break;                            \
	} else {                                                \
		Wreg(AW)=result;                                \
		Wreg(DW)=result2;                               \
	}

#define ADD4S {                                             \
	int i,v1,v2,result;                                     \
	int count = (Breg(CL)+1)/2;                         \
	unsigned di = Wreg(IY);                             \
	unsigned si = Wreg(IX);                             \
	static const UINT8 table[3]={18,19,19};                 \
	if (V.seg_prefix) logerror("%06x: Warning: seg_prefix defined for add4s\n",PC()); \
	V.ZeroVal = V.CarryVal = 0;                               \
	for (i=0;i<count;i++) {                                 \
		v25_ICount-=table[V.chip_type/8];                   \
		tmp = GetMemB(DS0, si);                             \
		tmp2 = GetMemB(DS1, di);                            \
		v1 = (tmp>>4)*10 + (tmp&0xf);                       \
		v2 = (tmp2>>4)*10 + (tmp2&0xf);                     \
		result = v1+v2+V.CarryVal;                         \
		V.CarryVal = result > 99 ? 1 : 0;                  \
		result = result % 100;                              \
		v1 = ((result/10)<<4) | (result % 10);              \
		PutMemB(DS1, di,v1);                                \
		if (v1) V.ZeroVal = 1;                             \
		si++;                                               \
		di++;                                               \
	}                                                       \
}

#define SUB4S {                                             \
	int count = (Breg(CL)+1)/2;                         \
	int i,v1,v2,result;                                     \
	unsigned di = Wreg(IY);                             \
	unsigned si = Wreg(IX);                             \
	static const UINT8 table[3]={18,19,19};                 \
	if (V.seg_prefix) logerror("%06x: Warning: seg_prefix defined for sub4s\n",PC()); \
	V.ZeroVal = V.CarryVal = 0;                               \
	for (i=0;i<count;i++) {                                 \
		v25_ICount-=table[V.chip_type/8];                   \
		tmp = GetMemB(DS1, di);                             \
		tmp2 = GetMemB(DS0, si);                            \
		v1 = (tmp>>4)*10 + (tmp&0xf);                       \
		v2 = (tmp2>>4)*10 + (tmp2&0xf);                     \
		if (v1 < (v2+V.CarryVal)) {                            \
			v1+=100;                                        \
			result = v1-(v2+V.CarryVal);                   \
			V.CarryVal = 1;                                    \
		} else {                                            \
			result = v1-(v2+V.CarryVal);                   \
			V.CarryVal = 0;                                    \
		}                                                   \
		v1 = ((result/10)<<4) | (result % 10);              \
		PutMemB(DS1, di,v1);                                \
		if (v1) V.ZeroVal = 1;                             \
		si++;                                               \
		di++;                                               \
	}                                                       \
}

#define CMP4S {                                             \
	int count = (Breg(CL)+1)/2;                         \
	int i,v1,v2,result;                                     \
	unsigned di = Wreg(IY);                             \
	unsigned si = Wreg(IX);                             \
	static const UINT8 table[3]={14,19,19};                 \
	if (V.seg_prefix) logerror("%06x: Warning: seg_prefix defined for cmp4s\n",PC()); \
	V.ZeroVal = V.CarryVal = 0;                               \
	for (i=0;i<count;i++) {                                 \
		v25_ICount-=table[V.chip_type/8];                   \
		tmp = GetMemB(DS1, di);                             \
		tmp2 = GetMemB(DS0, si);                            \
		v1 = (tmp>>4)*10 + (tmp&0xf);                       \
		v2 = (tmp2>>4)*10 + (tmp2&0xf);                     \
		if (v1 < (v2+V.CarryVal)) {                            \
			v1+=100;                                        \
			result = v1-(v2+V.CarryVal);                   \
			V.CarryVal = 1;                                    \
		} else {                                            \
			result = v1-(v2+V.CarryVal);                   \
			V.CarryVal = 0;                                    \
		}                                                   \
		v1 = ((result/10)<<4) | (result % 10);              \
		if (v1) V.ZeroVal = 1;                             \
		si++;                                               \
		di++;                                               \
	}                                                       \
}


static UINT32 EA_000(void) { V.EO=Wreg(BW)+Wreg(IX); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_001(void) { V.EO=Wreg(BW)+Wreg(IY); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_002(void) { V.EO=Wreg(BP)+Wreg(IX); V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_003(void) { V.EO=Wreg(BP)+Wreg(IY); V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_004(void) { V.EO=Wreg(IX); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_005(void) { V.EO=Wreg(IY); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_006(void) { V.EO=fetch(); V.EO+=fetch()<<8; V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_007(void) { V.EO=Wreg(BW); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }

static UINT32 EA_100(void) { V.EO=(Wreg(BW)+Wreg(IX)+(INT8)fetch()); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_101(void) { V.EO=(Wreg(BW)+Wreg(IY)+(INT8)fetch()); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_102(void) { V.EO=(Wreg(BP)+Wreg(IX)+(INT8)fetch()); V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_103(void) { V.EO=(Wreg(BP)+Wreg(IY)+(INT8)fetch()); V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_104(void) { V.EO=(Wreg(IX)+(INT8)fetch()); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_105(void) { V.EO=(Wreg(IY)+(INT8)fetch()); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_106(void) { V.EO=(Wreg(BP)+(INT8)fetch()); V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_107(void) { V.EO=(Wreg(BW)+(INT8)fetch()); V.EA=DefaultBase(DS0)+V.EO; return V.EA; }

static UINT32 EA_200(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(BW)+Wreg(IX)+(INT16)V.E16; V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_201(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(BW)+Wreg(IY)+(INT16)V.E16; V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_202(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(BP)+Wreg(IX)+(INT16)V.E16; V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_203(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(BP)+Wreg(IY)+(INT16)V.E16; V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_204(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(IX)+(INT16)V.E16; V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_205(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(IY)+(INT16)V.E16; V.EA=DefaultBase(DS0)+V.EO; return V.EA; }
static UINT32 EA_206(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(BP)+(INT16)V.E16; V.EA=DefaultBase(SS)+V.EO; return V.EA; }
static UINT32 EA_207(void) { V.E16=fetch(); V.E16+=fetch()<<8; V.EO=Wreg(BW)+(INT16)V.E16; V.EA=DefaultBase(DS0)+V.EO; return V.EA; }

UINT32 (*const v25_GetEA[192])(void)=
{
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,
	EA_000, EA_001, EA_002, EA_003, EA_004, EA_005, EA_006, EA_007,

	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,
	EA_100, EA_101, EA_102, EA_103, EA_104, EA_105, EA_106, EA_107,

	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207,
	EA_200, EA_201, EA_202, EA_203, EA_204, EA_205, EA_206, EA_207
};

static struct {
	struct {
		WREGS w[256];
		BREGS b[256];
	} reg;
	struct {
		WREGS w[256];
		BREGS b[256];
	} RM;
} Mod_RM;

#define RegWord(ModRM) Wreg(Mod_RM.reg.w[ModRM])
#define RegByte(ModRM) Breg(Mod_RM.reg.b[ModRM])

#define GetRMWord(ModRM) \
	((ModRM) >= 0xc0 ? Wreg(Mod_RM.RM.w[ModRM]) : ( v25_GetEA[ModRM](), read_mem_word( V.EA ) ))

#define PutbackRMWord(ModRM,val)                 \
{                                \
	if (ModRM >= 0xc0) Wreg(Mod_RM.RM.w[ModRM])=val; \
	else write_mem_word(V.EA,val);  \
}

#define GetnextRMWord read_mem_word((V.EA&0xf0000)|((V.EA+2)&0xffff))

#define PutRMWord(ModRM,val)                \
{                           \
	if (ModRM >= 0xc0)              \
		Wreg(Mod_RM.RM.w[ModRM])=val;   \
	else {                      \
		v25_GetEA[ModRM]();         \
		write_mem_word( V.EA ,val);           \
	}                       \
}

#define PutImmRMWord(ModRM)                 \
{                           \
	WORD val;                   \
	if (ModRM >= 0xc0)              \
		Wreg(Mod_RM.RM.w[ModRM]) = fetchword(); \
	else {                      \
		v25_GetEA[ModRM]();         \
		val = fetchword();              \
		write_mem_word( V.EA , val);          \
	}                       \
}

#define GetRMByte(ModRM) \
	((ModRM) >= 0xc0 ? Breg(Mod_RM.RM.b[ModRM]) : read_mem_byte( v25_GetEA[ModRM]() ))

#define PutRMByte(ModRM,val)                \
{                           \
	if (ModRM >= 0xc0)              \
		Breg(Mod_RM.RM.b[ModRM])=val;   \
	else                        \
		write_mem_byte( v25_GetEA[ModRM]() ,val);   \
}

#define PutImmRMByte(ModRM)                 \
{                           \
	if (ModRM >= 0xc0)              \
		Breg(Mod_RM.RM.b[ModRM])=fetch();   \
	else {                      \
		v25_GetEA[ModRM]();         \
		write_mem_byte( V.EA , fetch() );     \
	}                       \
}

#define PutbackRMByte(ModRM,val)            \
{                           \
	if (ModRM >= 0xc0)              \
		Breg(Mod_RM.RM.b[ModRM])=val;   \
	else                        \
		write_mem_byte(V.EA,val);         \
}

#define DEF_br8                         \
	UINT32 ModRM = fetch(),src,dst;     \
	src = RegByte(ModRM);               \
	dst = GetRMByte(ModRM)

#define DEF_wr16                        \
	UINT32 ModRM = fetch(),src,dst;     \
	src = RegWord(ModRM);               \
	dst = GetRMWord(ModRM)

#define DEF_r8b                         \
	UINT32 ModRM = fetch(),src,dst;     \
	dst = RegByte(ModRM);               \
	src = GetRMByte(ModRM)

#define DEF_r16w                        \
	UINT32 ModRM = fetch(),src,dst;     \
	dst = RegWord(ModRM);               \
	src = GetRMWord(ModRM)

#define DEF_ald8                        \
	UINT32 src = fetch();                   \
	UINT32 dst = Breg(AL)

#define DEF_axd16                       \
	UINT32 src = fetch();               \
	UINT32 dst = Wreg(AW);          \
	src += (fetch() << 8)

#define OP(num,func_name) static void func_name(void)

OP( 0x00, i_add_br8  ) { DEF_br8;   ADDB;   PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x01, i_add_wr16 ) { DEF_wr16;  ADDW;   PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x02, i_add_r8b  ) { DEF_r8b;   ADDB;   RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x03, i_add_r16w ) { DEF_r16w;  ADDW;   RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x04, i_add_ald8 ) { DEF_ald8;  ADDB;   Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x05, i_add_axd16) { DEF_axd16; ADDW;   Wreg(AW)=dst;           CLKS(4,4,2);                }
OP( 0x06, i_push_es  ) { PUSH(Sreg(DS1));   CLKS(12,8,3);   }
OP( 0x07, i_pop_es   ) { POP(Sreg(DS1));    CLKS(12,8,5);   }

OP( 0x08, i_or_br8   ) { DEF_br8;   ORB;    PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x09, i_or_wr16  ) { DEF_wr16;  ORW;    PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x0a, i_or_r8b   ) { DEF_r8b;   ORB;    RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x0b, i_or_r16w  ) { DEF_r16w;  ORW;    RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x0c, i_or_ald8  ) { DEF_ald8;  ORB;    Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x0d, i_or_axd16 ) { DEF_axd16; ORW;    Wreg(AW)=dst;           CLKS(4,4,2);                }
OP( 0x0e, i_push_cs  ) { PUSH(Sreg(PS));    CLKS(12,8,3);   }
OP( 0x10, i_adc_br8  ) { DEF_br8;   src+=CF;    ADDB;   PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x11, i_adc_wr16 ) { DEF_wr16;  src+=CF;    ADDW;   PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x12, i_adc_r8b  ) { DEF_r8b;   src+=CF;    ADDB;   RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x13, i_adc_r16w ) { DEF_r16w;  src+=CF;    ADDW;   RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x14, i_adc_ald8 ) { DEF_ald8;  src+=CF;    ADDB;   Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x15, i_adc_axd16) { DEF_axd16; src+=CF;    ADDW;   Wreg(AW)=dst;           CLKS(4,4,2);                }
OP( 0x16, i_push_ss  ) { PUSH(Sreg(SS));        CLKS(12,8,3);   }
OP( 0x17, i_pop_ss   ) { POP(Sreg(SS));     CLKS(12,8,5);   V.no_interrupt=1; }

OP( 0x18, i_sbb_br8  ) { DEF_br8;   src+=CF;    SUBB;   PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x19, i_sbb_wr16 ) { DEF_wr16;  src+=CF;    SUBW;   PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x1a, i_sbb_r8b  ) { DEF_r8b;   src+=CF;    SUBB;   RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x1b, i_sbb_r16w ) { DEF_r16w;  src+=CF;    SUBW;   RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x1c, i_sbb_ald8 ) { DEF_ald8;  src+=CF;    SUBB;   Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x1d, i_sbb_axd16) { DEF_axd16; src+=CF;    SUBW;   Wreg(AW)=dst;           CLKS(4,4,2);    }
OP( 0x1e, i_push_ds  ) { PUSH(Sreg(DS0));       CLKS(12,8,3);   }
OP( 0x1f, i_pop_ds   ) { POP(Sreg(DS0));        CLKS(12,8,5);   }

OP( 0x20, i_and_br8  ) { DEF_br8;   ANDB;   PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x21, i_and_wr16 ) { DEF_wr16;  ANDW;   PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x22, i_and_r8b  ) { DEF_r8b;   ANDB;   RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x23, i_and_r16w ) { DEF_r16w;  ANDW;   RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x24, i_and_ald8 ) { DEF_ald8;  ANDB;   Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x25, i_and_axd16) { DEF_axd16; ANDW;   Wreg(AW)=dst;           CLKS(4,4,2);    }
OP( 0x26, i_es       ) { V.seg_prefix=1;    V.prefix_base=Sreg(DS1)<<4;    CLK(2);     v25_instruction[fetchop()](); V.seg_prefix=0; }
OP( 0x27, i_daa      ) { ADJ4(6,0x60);                                  CLKS(3,3,2);    }

OP( 0x28, i_sub_br8  ) { DEF_br8;   SUBB;   PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x29, i_sub_wr16 ) { DEF_wr16;  SUBW;   PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x2a, i_sub_r8b  ) { DEF_r8b;   SUBB;   RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x2b, i_sub_r16w ) { DEF_r16w;  SUBW;   RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x2c, i_sub_ald8 ) { DEF_ald8;  SUBB;   Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x2d, i_sub_axd16) { DEF_axd16; SUBW;   Wreg(AW)=dst;           CLKS(4,4,2);    }
OP( 0x2e, i_cs       ) { V.seg_prefix=1;    V.prefix_base=Sreg(PS)<<4; CLK(2);     v25_instruction[fetchop()](); V.seg_prefix=0; }
OP( 0x2f, i_das      ) { ADJ4(-6,-0x60);                                CLKS(3,3,2);    }

OP( 0x30, i_xor_br8  ) { DEF_br8;   XORB;   PutbackRMByte(ModRM,dst);   CLKM(2,2,2,16,16,7);        }
OP( 0x31, i_xor_wr16 ) { DEF_wr16;  XORW;   PutbackRMWord(ModRM,dst);   CLKR(24,24,11,24,16,7,2,V.EA);}
OP( 0x32, i_xor_r8b  ) { DEF_r8b;   XORB;   RegByte(ModRM)=dst;         CLKM(2,2,2,11,11,6);        }
OP( 0x33, i_xor_r16w ) { DEF_r16w;  XORW;   RegWord(ModRM)=dst;         CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x34, i_xor_ald8 ) { DEF_ald8;  XORB;   Breg(AL)=dst;           CLKS(4,4,2);                }
OP( 0x35, i_xor_axd16) { DEF_axd16; XORW;   Wreg(AW)=dst;           CLKS(4,4,2);    }
OP( 0x36, i_ss       ) { V.seg_prefix=1;    V.prefix_base=Sreg(SS)<<4; CLK(2);     v25_instruction[fetchop()](); V.seg_prefix=0; }
OP( 0x37, i_aaa      ) { ADJB(6, (Breg(AL) > 0xf9) ? 2 : 1);        CLKS(7,7,4);    }

OP( 0x38, i_cmp_br8  ) { DEF_br8;   SUBB;                   CLKM(2,2,2,11,11,6); }
OP( 0x39, i_cmp_wr16 ) { DEF_wr16;  SUBW;                   CLKR(15,15,8,15,11,6,2,V.EA);}
OP( 0x3a, i_cmp_r8b  ) { DEF_r8b;   SUBB;                   CLKM(2,2,2,11,11,6); }
OP( 0x3b, i_cmp_r16w ) { DEF_r16w;  SUBW;                   CLKR(15,15,8,15,11,6,2,V.EA); }
OP( 0x3c, i_cmp_ald8 ) { DEF_ald8;  SUBB;                   CLKS(4,4,2); }
OP( 0x3d, i_cmp_axd16) { DEF_axd16; SUBW;                   CLKS(4,4,2);    }
OP( 0x3e, i_ds       ) { V.seg_prefix=1;    V.prefix_base=Sreg(DS0)<<4;    CLK(2);     v25_instruction[fetchop()](); V.seg_prefix=0; }
OP( 0x3f, i_aas      ) { ADJB(-6, (Breg(AL) < 6) ? -2 : -1);        CLKS(7,7,4);    }

OP( 0x40, i_inc_ax  ) { IncWordReg(AW);                     CLK(2); }
OP( 0x41, i_inc_cx  ) { IncWordReg(CW);                     CLK(2); }
OP( 0x42, i_inc_dx  ) { IncWordReg(DW);                     CLK(2); }
OP( 0x43, i_inc_bx  ) { IncWordReg(BW);                     CLK(2); }
OP( 0x44, i_inc_sp  ) { IncWordReg(SP);                     CLK(2); }
OP( 0x45, i_inc_bp  ) { IncWordReg(BP);                     CLK(2); }
OP( 0x46, i_inc_si  ) { IncWordReg(IX);                     CLK(2); }
OP( 0x47, i_inc_di  ) { IncWordReg(IY);                     CLK(2); }

OP( 0x48, i_dec_ax  ) { DecWordReg(AW);                     CLK(2); }
OP( 0x49, i_dec_cx  ) { DecWordReg(CW);                     CLK(2); }
OP( 0x4a, i_dec_dx  ) { DecWordReg(DW);                     CLK(2); }
OP( 0x4b, i_dec_bx  ) { DecWordReg(BW);                     CLK(2); }
OP( 0x4c, i_dec_sp  ) { DecWordReg(SP);                     CLK(2); }
OP( 0x4d, i_dec_bp  ) { DecWordReg(BP);                     CLK(2); }
OP( 0x4e, i_dec_si  ) { DecWordReg(IX);                     CLK(2); }
OP( 0x4f, i_dec_di  ) { DecWordReg(IY);                     CLK(2); }

OP( 0x50, i_push_ax ) { PUSH(Wreg(AW));                 CLKS(12,8,3); }
OP( 0x51, i_push_cx ) { PUSH(Wreg(CW));                 CLKS(12,8,3); }
OP( 0x52, i_push_dx ) { PUSH(Wreg(DW));                 CLKS(12,8,3); }
OP( 0x53, i_push_bx ) { PUSH(Wreg(BW));                 CLKS(12,8,3); }
OP( 0x54, i_push_sp ) { PUSH(Wreg(SP));                 CLKS(12,8,3); }
OP( 0x55, i_push_bp ) { PUSH(Wreg(BP));                 CLKS(12,8,3); }
OP( 0x56, i_push_si ) { PUSH(Wreg(IX));                 CLKS(12,8,3); }
OP( 0x57, i_push_di ) { PUSH(Wreg(IY));                 CLKS(12,8,3); }

OP( 0x58, i_pop_ax  ) { POP(Wreg(AW));                  CLKS(12,8,5); }
OP( 0x59, i_pop_cx  ) { POP(Wreg(CW));                  CLKS(12,8,5); }
OP( 0x5a, i_pop_dx  ) { POP(Wreg(DW));                  CLKS(12,8,5); }
OP( 0x5b, i_pop_bx  ) { POP(Wreg(BW));                  CLKS(12,8,5); }
OP( 0x5c, i_pop_sp  ) { POP(Wreg(SP));                  CLKS(12,8,5); }
OP( 0x5d, i_pop_bp  ) { POP(Wreg(BP));                  CLKS(12,8,5); }
OP( 0x5e, i_pop_si  ) { POP(Wreg(IX));                  CLKS(12,8,5); }
OP( 0x5f, i_pop_di  ) { POP(Wreg(IY));                  CLKS(12,8,5); }

OP( 0x60, i_pusha  ) {
	unsigned tmp=Wreg(SP);
	PUSH(Wreg(AW));
	PUSH(Wreg(CW));
	PUSH(Wreg(DW));
	PUSH(Wreg(BW));
	PUSH(tmp);
	PUSH(Wreg(BP));
	PUSH(Wreg(IX));
	PUSH(Wreg(IY));
	CLKS(67,35,20);
}
static unsigned nec_popa_tmp;
OP( 0x61, i_popa  ) {
	POP(Wreg(IY));
	POP(Wreg(IX));
	POP(Wreg(BP));
	POP(nec_popa_tmp);
	POP(Wreg(BW));
	POP(Wreg(DW));
	POP(Wreg(CW));
	POP(Wreg(AW));
	CLKS(75,43,22);
}
OP( 0x62, i_chkind  ) {
	UINT32 low,high,tmp;
	GetModRM;
	low = GetRMWord(ModRM);
	high= GetnextRMWord;
	tmp= RegWord(ModRM);
	if (tmp<low || tmp>high) {
		nec_interrupt(NEC_CHKIND_VECTOR, BRK);
	}
	v25_ICount-=20;
	logerror("%06x: bound %04x high %04x low %04x tmp\n",PC(),high,low,tmp);
}
OP( 0x64, i_repnc  ) {  UINT32 next = fetchop();   UINT16 c = Wreg(CW);
	switch(next) { /* Segments */
		case 0x26:  V.seg_prefix=1; V.prefix_base=Sreg(DS1)<<4;    next = fetchop();  CLK(2); break;
		case 0x2e:  V.seg_prefix=1; V.prefix_base=Sreg(PS)<<4; next = fetchop();  CLK(2); break;
		case 0x36:  V.seg_prefix=1; V.prefix_base=Sreg(SS)<<4; next = fetchop();  CLK(2); break;
		case 0x3e:  V.seg_prefix=1; V.prefix_base=Sreg(DS0)<<4;    next = fetchop();  CLK(2); break;
	}

	switch(next) {
		case 0x6c:  CLK(2); if (c) do { i_insb();  c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0x6d:  CLK(2); if (c) do { i_insw();  c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0x6e:  CLK(2); if (c) do { i_outsb(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0x6f:  CLK(2); if (c) do { i_outsw(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xa4:  CLK(2); if (c) do { i_movsb(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xa5:  CLK(2); if (c) do { i_movsw(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xa6:  CLK(2); if (c) do { i_cmpsb(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xa7:  CLK(2); if (c) do { i_cmpsw(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xaa:  CLK(2); if (c) do { i_stosb(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xab:  CLK(2); if (c) do { i_stosw(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xac:  CLK(2); if (c) do { i_lodsb(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xad:  CLK(2); if (c) do { i_lodsw(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xae:  CLK(2); if (c) do { i_scasb(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		case 0xaf:  CLK(2); if (c) do { i_scasw(); c--; } while (c>0 && !CF); Wreg(CW)=c; break;
		default:    logerror("%06x: REPNC invalid\n",PC());    v25_instruction[next]();
	}
	V.seg_prefix=0;
}

OP( 0x65, i_repc  ) {   UINT32 next = fetchop();   UINT16 c = Wreg(CW);
	switch(next) { /* Segments */
		case 0x26:  V.seg_prefix=1; V.prefix_base=Sreg(DS1)<<4;    next = fetchop();  CLK(2); break;
		case 0x2e:  V.seg_prefix=1; V.prefix_base=Sreg(PS)<<4; next = fetchop();  CLK(2); break;
		case 0x36:  V.seg_prefix=1; V.prefix_base=Sreg(SS)<<4; next = fetchop();  CLK(2); break;
		case 0x3e:  V.seg_prefix=1; V.prefix_base=Sreg(DS0)<<4;    next = fetchop();  CLK(2); break;
	}

	switch(next) {
		case 0x6c:  CLK(2); if (c) do { i_insb();  c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0x6d:  CLK(2); if (c) do { i_insw();  c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0x6e:  CLK(2); if (c) do { i_outsb(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0x6f:  CLK(2); if (c) do { i_outsw(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xa4:  CLK(2); if (c) do { i_movsb(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xa5:  CLK(2); if (c) do { i_movsw(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xa6:  CLK(2); if (c) do { i_cmpsb(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xa7:  CLK(2); if (c) do { i_cmpsw(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xaa:  CLK(2); if (c) do { i_stosb(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xab:  CLK(2); if (c) do { i_stosw(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xac:  CLK(2); if (c) do { i_lodsb(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xad:  CLK(2); if (c) do { i_lodsw(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xae:  CLK(2); if (c) do { i_scasb(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		case 0xaf:  CLK(2); if (c) do { i_scasw(); c--; } while (c>0 && CF);   Wreg(CW)=c; break;
		default:    logerror("%06x: REPC invalid\n",PC()); v25_instruction[next]();
	}
	V.seg_prefix=0;
}

OP( 0x68, i_push_d16 ) { UINT32 tmp;    tmp = fetchword(); PUSH(tmp);   CLKW(12,12,5,12,8,5,Wreg(SP));  }
OP( 0x69, i_imul_d16 ) { UINT32 tmp;    DEF_r16w;   tmp = fetchword(); dst = (INT32)((INT16)src)*(INT32)((INT16)tmp); V.CarryVal = V.OverVal = (((INT32)dst) >> 15 != 0) && (((INT32)dst) >> 15 != -1);     RegWord(ModRM)=(WORD)dst;     v25_ICount-=(ModRM >=0xc0 )?38:47;}
OP( 0x6a, i_push_d8  ) { UINT32 tmp = (WORD)((INT16)((INT8)fetch()));   PUSH(tmp);  CLKW(11,11,5,11,7,3,Wreg(SP));  }
OP( 0x6b, i_imul_d8  ) { UINT32 src2; DEF_r16w; src2= (WORD)((INT16)((INT8)fetch())); dst = (INT32)((INT16)src)*(INT32)((INT16)src2); V.CarryVal = V.OverVal = (((INT32)dst) >> 15 != 0) && (((INT32)dst) >> 15 != -1); RegWord(ModRM)=(WORD)dst; v25_ICount-=(ModRM >=0xc0 )?31:39; }
OP( 0x6c, i_insb     ) { PutMemB(DS1,Wreg(IY),read_port_byte(Wreg(DW))); Wreg(IY)+= -2 * V.DF + 1; CLK(8); }
OP( 0x6d, i_insw     ) { PutMemW(DS1,Wreg(IY),read_port_word(Wreg(DW))); Wreg(IY)+= -4 * V.DF + 2; CLKS(18,10,8); }
OP( 0x6e, i_outsb    ) { write_port_byte(Wreg(DW),GetMemB(DS0,Wreg(IX))); Wreg(IX)+= -2 * V.DF + 1; CLK(8); }
OP( 0x6f, i_outsw    ) { write_port_word(Wreg(DW),GetMemW(DS0,Wreg(IX))); Wreg(IX)+= -4 * V.DF + 2; CLKS(18,10,8); }

OP( 0x70, i_jo      ) { JMP( OF);               CLKS(4,4,3); }
OP( 0x71, i_jno     ) { JMP(!OF);               CLKS(4,4,3); }
OP( 0x72, i_jc      ) { JMP( CF);               CLKS(4,4,3); }
OP( 0x73, i_jnc     ) { JMP(!CF);               CLKS(4,4,3); }
OP( 0x74, i_jz      ) { JMP( ZF);               CLKS(4,4,3); }
OP( 0x75, i_jnz     ) { JMP(!ZF);               CLKS(4,4,3); }
OP( 0x76, i_jce     ) { JMP(CF || ZF);          CLKS(4,4,3); }
OP( 0x77, i_jnce    ) { JMP(!(CF || ZF));       CLKS(4,4,3); }
OP( 0x78, i_js      ) { JMP( SF);               CLKS(4,4,3); }
OP( 0x79, i_jns     ) { JMP(!SF);               CLKS(4,4,3); }
OP( 0x7a, i_jp      ) { JMP( PF);               CLKS(4,4,3); }
OP( 0x7b, i_jnp     ) { JMP(!PF);               CLKS(4,4,3); }
OP( 0x7c, i_jl      ) { JMP((SF!=OF)&&(!ZF));   CLKS(4,4,3); }
OP( 0x7d, i_jnl     ) { JMP((ZF)||(SF==OF));    CLKS(4,4,3); }
OP( 0x7e, i_jle     ) { JMP((ZF)||(SF!=OF));    CLKS(4,4,3); }
OP( 0x7f, i_jnle    ) { JMP((SF==OF)&&(!ZF));   CLKS(4,4,3); }

OP( 0x80, i_80pre   ) { UINT32 dst, src; GetModRM; dst = GetRMByte(ModRM); src = fetch();
	if (ModRM >=0xc0 ) CLKS(4,4,2) else if ((ModRM & 0x38)==0x38) CLKS(13,13,6) else CLKS(18,18,7)
	switch (ModRM & 0x38) {
		case 0x00: ADDB;            PutbackRMByte(ModRM,dst);   break;
		case 0x08: ORB;             PutbackRMByte(ModRM,dst);   break;
		case 0x10: src+=CF; ADDB;   PutbackRMByte(ModRM,dst);   break;
		case 0x18: src+=CF; SUBB;   PutbackRMByte(ModRM,dst);   break;
		case 0x20: ANDB;            PutbackRMByte(ModRM,dst);   break;
		case 0x28: SUBB;            PutbackRMByte(ModRM,dst);   break;
		case 0x30: XORB;            PutbackRMByte(ModRM,dst);   break;
		case 0x38: SUBB;            break;  /* CMP */
	}
}

OP( 0x81, i_81pre   ) { UINT32 dst, src; GetModRM; dst = GetRMWord(ModRM); src = fetch(); src+= (fetch() << 8);
	if (ModRM >=0xc0 ) CLKS(4,4,2) else if ((ModRM & 0x38)==0x38) CLKW(17,17,8,17,13,6,V.EA) else CLKW(26,26,11,26,18,7,V.EA)
	switch (ModRM & 0x38) {
		case 0x00: ADDW;            PutbackRMWord(ModRM,dst);   break;
		case 0x08: ORW;             PutbackRMWord(ModRM,dst);   break;
		case 0x10: src+=CF; ADDW;   PutbackRMWord(ModRM,dst);   break;
		case 0x18: src+=CF; SUBW;   PutbackRMWord(ModRM,dst);   break;
		case 0x20: ANDW;            PutbackRMWord(ModRM,dst);   break;
		case 0x28: SUBW;            PutbackRMWord(ModRM,dst);   break;
		case 0x30: XORW;            PutbackRMWord(ModRM,dst);   break;
		case 0x38: SUBW;            break;  /* CMP */
	}
}

OP( 0x82, i_82pre   ) { UINT32 dst, src; GetModRM; dst = GetRMByte(ModRM); src = (BYTE)((INT8)fetch());
	if (ModRM >=0xc0 ) CLKS(4,4,2) else if ((ModRM & 0x38)==0x38) CLKS(13,13,6) else CLKS(18,18,7)
	switch (ModRM & 0x38) {
		case 0x00: ADDB;            PutbackRMByte(ModRM,dst);   break;
		case 0x08: ORB;             PutbackRMByte(ModRM,dst);   break;
		case 0x10: src+=CF; ADDB;   PutbackRMByte(ModRM,dst);   break;
		case 0x18: src+=CF; SUBB;   PutbackRMByte(ModRM,dst);   break;
		case 0x20: ANDB;            PutbackRMByte(ModRM,dst);   break;
		case 0x28: SUBB;            PutbackRMByte(ModRM,dst);   break;
		case 0x30: XORB;            PutbackRMByte(ModRM,dst);   break;
		case 0x38: SUBB;            break;  /* CMP */
	}
}

OP( 0x83, i_83pre   ) { UINT32 dst, src; GetModRM; dst = GetRMWord(ModRM); src = (WORD)((INT16)((INT8)fetch()));
	if (ModRM >=0xc0 ) CLKS(4,4,2) else if ((ModRM & 0x38)==0x38) CLKW(17,17,8,17,13,6,V.EA) else CLKW(26,26,11,26,18,7,V.EA)
	switch (ModRM & 0x38) {
		case 0x00: ADDW;            PutbackRMWord(ModRM,dst);   break;
		case 0x08: ORW;             PutbackRMWord(ModRM,dst);   break;
		case 0x10: src+=CF; ADDW;   PutbackRMWord(ModRM,dst);   break;
		case 0x18: src+=CF; SUBW;   PutbackRMWord(ModRM,dst);   break;
		case 0x20: ANDW;            PutbackRMWord(ModRM,dst);   break;
		case 0x28: SUBW;            PutbackRMWord(ModRM,dst);   break;
		case 0x30: XORW;            PutbackRMWord(ModRM,dst);   break;
		case 0x38: SUBW;            break;  /* CMP */
	}
}

OP( 0x84, i_test_br8  ) { DEF_br8;  ANDB;   CLKM(2,2,2,10,10,6);        }
OP( 0x85, i_test_wr16 ) { DEF_wr16; ANDW;   CLKR(14,14,8,14,10,6,2,V.EA); }
OP( 0x86, i_xchg_br8  ) { DEF_br8;  RegByte(ModRM)=dst; PutbackRMByte(ModRM,src); CLKM(3,3,3,16,18,8); }
OP( 0x87, i_xchg_wr16 ) { DEF_wr16; RegWord(ModRM)=dst; PutbackRMWord(ModRM,src); CLKR(24,24,12,24,16,8,3,V.EA); }

OP( 0x88, i_mov_br8   ) { UINT8  src; GetModRM; src = RegByte(ModRM);   PutRMByte(ModRM,src);   CLKM(2,2,2,9,9,3);          }
OP( 0x89, i_mov_wr16  ) { UINT16 src; GetModRM; src = RegWord(ModRM);   PutRMWord(ModRM,src);   CLKR(13,13,5,13,9,3,2,V.EA);  }
OP( 0x8a, i_mov_r8b   ) { UINT8  src; GetModRM; src = GetRMByte(ModRM); RegByte(ModRM)=src;     CLKM(2,2,2,11,11,5);        }
OP( 0x8b, i_mov_r16w  ) { UINT16 src; GetModRM; src = GetRMWord(ModRM); RegWord(ModRM)=src;     CLKR(15,15,7,15,11,5,2,V.EA);     }
OP( 0x8c, i_mov_wsreg ) { GetModRM;
	switch (ModRM & 0x38) {
		case 0x00: PutRMWord(ModRM,Sreg(DS1)); CLKR(14,14,5,14,10,3,2,V.EA); break;
		case 0x08: PutRMWord(ModRM,Sreg(PS)); CLKR(14,14,5,14,10,3,2,V.EA); break;
		case 0x10: PutRMWord(ModRM,Sreg(SS)); CLKR(14,14,5,14,10,3,2,V.EA); break;
		case 0x18: PutRMWord(ModRM,Sreg(DS0)); CLKR(14,14,5,14,10,3,2,V.EA); break;
		default:   logerror("%06x: MOV Sreg - Invalid register\n",PC());
	}
}
OP( 0x8d, i_lea       ) { UINT16 ModRM = fetch(); if (ModRM >= 0xc0) logerror("LDEA invalid mode %Xh\n", ModRM); else { (void)v25_GetEA[ModRM](); RegWord(ModRM)=V.EO; }  CLKS(4,4,2); }
OP( 0x8e, i_mov_sregw ) { UINT16 src; GetModRM; src = GetRMWord(ModRM); CLKR(15,15,7,15,11,5,2,V.EA);
	switch (ModRM & 0x38) {
		case 0x00: Sreg(DS1) = src; break; /* mov es,ew */
		case 0x08: Sreg(PS) = src; break; /* mov cs,ew */
		case 0x10: Sreg(SS) = src; break; /* mov ss,ew */
		case 0x18: Sreg(DS0) = src; break; /* mov ds,ew */
		default:   logerror("%06x: MOV Sreg - Invalid register\n",PC());
	}
	V.no_interrupt=1;
}
OP( 0x8f, i_popw ) { UINT16 tmp; GetModRM; POP(tmp); PutRMWord(ModRM,tmp); v25_ICount-=21; }
OP( 0x90, i_nop  ) { CLK(3); /* { if (V.MF == 0) printf("90 -> %06x: \n",PC()); }  */ }
OP( 0x91, i_xchg_axcx ) { XchgAWReg(CW); CLK(3); }
OP( 0x92, i_xchg_axdx ) { XchgAWReg(DW); CLK(3); }
OP( 0x93, i_xchg_axbx ) { XchgAWReg(BW); CLK(3); }
OP( 0x94, i_xchg_axsp ) { XchgAWReg(SP); CLK(3); }
OP( 0x95, i_xchg_axbp ) { XchgAWReg(BP); CLK(3); }
OP( 0x96, i_xchg_axsi ) { XchgAWReg(IX); CLK(3); }
OP( 0x97, i_xchg_axdi ) { XchgAWReg(IY); CLK(3); }

OP( 0x98, i_cbw       ) { Breg(AH) = (Breg(AL) & 0x80) ? 0xff : 0;      CLK(2); }
OP( 0x99, i_cwd       ) { Wreg(DW) = (Breg(AH) & 0x80) ? 0xffff : 0;    CLK(4); }
OP( 0x9a, i_call_far  ) { UINT32 tmp, tmp2; tmp = fetchword(); tmp2 = fetchword(); PUSH(Sreg(PS)); PUSH(V.ip); V.ip = (WORD)tmp; Sreg(PS) = (WORD)tmp2; CHANGE_PC; CLKW(29,29,13,29,21,9,Wreg(SP)); }
OP( 0x9b, i_wait      ) { if (!V.poll_state) V.ip--; CLK(5); }
OP( 0x9c, i_pushf     ) { UINT16 tmp = CompressFlags(); PUSH( tmp ); CLKS(12,8,3); }
OP( 0x9d, i_popf      ) { UINT32 tmp; POP(tmp); ExpandFlags(tmp); CLKS(12,8,5); if (V.TF) nec_trap(); }
OP( 0x9e, i_sahf      ) { UINT32 tmp = (CompressFlags() & 0xff00) | (Breg(AH) & 0xd5); ExpandFlags(tmp); CLKS(3,3,2); }
OP( 0x9f, i_lahf      ) { Breg(AH) = CompressFlags() & 0xff; CLKS(3,3,2); }

OP( 0xa0, i_mov_aldisp ) { UINT32 addr; addr = fetchword(); Breg(AL) = GetMemB(DS0, addr); CLKS(10,10,5); }
OP( 0xa1, i_mov_axdisp ) { UINT32 addr; addr = fetchword(); Wreg(AW) = GetMemW(DS0, addr); CLKW(14,14,7,14,10,5,addr); }
OP( 0xa2, i_mov_dispal ) { UINT32 addr; addr = fetchword(); PutMemB(DS0, addr, Breg(AL));  CLKS(9,9,3); }
OP( 0xa3, i_mov_dispax ) { UINT32 addr; addr = fetchword(); PutMemW(DS0, addr, Wreg(AW));  CLKW(13,13,5,13,9,3,addr); }
OP( 0xa4, i_movsb      ) { UINT32 tmp = GetMemB(DS0,Wreg(IX)); PutMemB(DS1,Wreg(IY), tmp); Wreg(IY) += -2 * V.DF + 1; Wreg(IX) += -2 * V.DF + 1; CLKS(8,8,6); }
OP( 0xa5, i_movsw      ) { UINT32 tmp = GetMemW(DS0,Wreg(IX)); PutMemW(DS1,Wreg(IY), tmp); Wreg(IY) += -4 * V.DF + 2; Wreg(IX) += -4 * V.DF + 2; CLKS(16,16,10); }
OP( 0xa6, i_cmpsb      ) { UINT32 src = GetMemB(DS1, Wreg(IY)); UINT32 dst = GetMemB(DS0, Wreg(IX)); SUBB; Wreg(IY) += -2 * V.DF + 1; Wreg(IX) += -2 * V.DF + 1; CLKS(14,14,14); }
OP( 0xa7, i_cmpsw      ) { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = GetMemW(DS0, Wreg(IX)); SUBW; Wreg(IY) += -4 * V.DF + 2; Wreg(IX) += -4 * V.DF + 2; CLKS(14,14,14); }

OP( 0xa8, i_test_ald8  ) { DEF_ald8;  ANDB; CLKS(4,4,2); }
OP( 0xa9, i_test_axd16 ) { DEF_axd16; ANDW; CLKS(4,4,2); }
OP( 0xaa, i_stosb      ) { PutMemB(DS1,Wreg(IY),Breg(AL));  Wreg(IY) += -2 * V.DF + 1; CLKS(4,4,3);  }
OP( 0xab, i_stosw      ) { PutMemW(DS1,Wreg(IY),Wreg(AW));  Wreg(IY) += -4 * V.DF + 2; CLKW(8,8,5,8,4,3,Wreg(IY)); }
OP( 0xac, i_lodsb      ) { Breg(AL) = GetMemB(DS0,Wreg(IX)); Wreg(IX) += -2 * V.DF + 1; CLKS(4,4,3);  }
OP( 0xad, i_lodsw      ) { Wreg(AW) = GetMemW(DS0,Wreg(IX)); Wreg(IX) += -4 * V.DF + 2; CLKW(8,8,5,8,4,3,Wreg(IX)); }
OP( 0xae, i_scasb      ) { UINT32 src = GetMemB(DS1, Wreg(IY)); UINT32 dst = Breg(AL); SUBB; Wreg(IY) += -2 * V.DF + 1; CLKS(4,4,3);  }
OP( 0xaf, i_scasw      ) { UINT32 src = GetMemW(DS1, Wreg(IY)); UINT32 dst = Wreg(AW); SUBW; Wreg(IY) += -4 * V.DF + 2; CLKW(8,8,5,8,4,3,Wreg(IY)); }

OP( 0xb0, i_mov_ald8  ) { Breg(AL) = fetch();   CLKS(4,4,2); }
OP( 0xb1, i_mov_cld8  ) { Breg(CL) = fetch(); CLKS(4,4,2); }
OP( 0xb2, i_mov_dld8  ) { Breg(DL) = fetch(); CLKS(4,4,2); }
OP( 0xb3, i_mov_bld8  ) { Breg(BL) = fetch(); CLKS(4,4,2); }
OP( 0xb4, i_mov_ahd8  ) { Breg(AH) = fetch(); CLKS(4,4,2); }
OP( 0xb5, i_mov_chd8  ) { Breg(CH) = fetch(); CLKS(4,4,2); }
OP( 0xb6, i_mov_dhd8  ) { Breg(DH) = fetch(); CLKS(4,4,2); }
OP( 0xb7, i_mov_bhd8  ) { Breg(BH) = fetch();   CLKS(4,4,2); }

OP( 0xb8, i_mov_axd16 ) { Breg(AL) = fetch();    Breg(AH) = fetch();    CLKS(4,4,2); }
OP( 0xb9, i_mov_cxd16 ) { Breg(CL) = fetch();    Breg(CH) = fetch();    CLKS(4,4,2); }
OP( 0xba, i_mov_dxd16 ) { Breg(DL) = fetch();    Breg(DH) = fetch();    CLKS(4,4,2); }
OP( 0xbb, i_mov_bxd16 ) { Breg(BL) = fetch();    Breg(BH) = fetch();    CLKS(4,4,2); }
OP( 0xbc, i_mov_spd16 ) { Wreg(SP) = fetchword();   CLKS(4,4,2); }
OP( 0xbd, i_mov_bpd16 ) { Wreg(BP) = fetchword();   CLKS(4,4,2); }
OP( 0xbe, i_mov_sid16 ) { Wreg(IX) = fetchword();   CLKS(4,4,2); }
OP( 0xbf, i_mov_did16 ) { Wreg(IY) = fetchword();   CLKS(4,4,2); }

OP( 0xc0, i_rotshft_bd8 ) {
	UINT32 src, dst; UINT8 c;
	GetModRM; src = (unsigned)GetRMByte(ModRM); dst=src;
	c=fetch();
	CLKM(7,7,2,19,19,6);
	if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_BYTE;  c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x08: do { ROR_BYTE;  c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x10: do { ROLC_BYTE; c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x18: do { RORC_BYTE; c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x20: SHL_BYTE(c); break;
		case 0x28: SHR_BYTE(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xc0 0x30 (SHLA)\n",PC()); break;
		case 0x38: SHRA_BYTE(c); break;
	}
}

OP( 0xc1, i_rotshft_wd8 ) {
	UINT32 src, dst;  UINT8 c;
	GetModRM; src = (unsigned)GetRMWord(ModRM); dst=src;
	c=fetch();
	CLKM(7,7,2,27,19,6);
	if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_WORD;  c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x08: do { ROR_WORD;  c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x10: do { ROLC_WORD; c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x18: do { RORC_WORD; c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x20: SHL_WORD(c); break;
		case 0x28: SHR_WORD(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xc1 0x30 (SHLA)\n",PC()); break;
		case 0x38: SHRA_WORD(c); break;
	}
}

OP( 0xc2, i_ret_d16  ) { UINT32 count = fetch(); count += fetch() << 8; POP(V.ip); Wreg(SP)+=count; CHANGE_PC; CLKS(24,24,10); }
OP( 0xc3, i_ret      ) { POP(V.ip); CHANGE_PC; CLKS(19,19,10); }
OP( 0xc4, i_les_dw   ) { GetModRM; WORD tmp = GetRMWord(ModRM); RegWord(ModRM)=tmp; Sreg(DS1) = GetnextRMWord; CLKW(26,26,14,26,18,10,V.EA); }
OP( 0xc5, i_lds_dw   ) { GetModRM; WORD tmp = GetRMWord(ModRM); RegWord(ModRM)=tmp; Sreg(DS0) = GetnextRMWord; CLKW(26,26,14,26,18,10,V.EA); }
OP( 0xc6, i_mov_bd8  ) { GetModRM; PutImmRMByte(ModRM); v25_ICount-=(ModRM >=0xc0 )?4:11; }
OP( 0xc7, i_mov_wd16 ) { GetModRM; PutImmRMWord(ModRM); v25_ICount-=(ModRM >=0xc0 )?4:15; }

OP( 0xc8, i_enter ) {
	UINT32 nb = fetch();
	UINT32 i,level;

	v25_ICount-=23;
	nb += fetch() << 8;
	level = fetch();
	PUSH(Wreg(BP));
	Wreg(BP)=Wreg(SP);
	Wreg(SP) -= nb;
	for (i=1;i<level;i++) {
		PUSH(GetMemW(SS,Wreg(BP)-i*2));
		v25_ICount-=16;
	}
	if (level) PUSH(Wreg(BP));
}
OP( 0xc9, i_leave ) {
	Wreg(SP)=Wreg(BP);
	POP(Wreg(BP));
	v25_ICount-=8;
}
OP( 0xca, i_retf_d16  ) { UINT32 count = fetch(); count += fetch() << 8; POP(V.ip); POP(Sreg(PS)); Wreg(SP)+=count; CHANGE_PC; CLKS(32,32,16); }
OP( 0xcb, i_retf      ) { POP(V.ip); POP(Sreg(PS)); CHANGE_PC; CLKS(29,29,16); }
OP( 0xcc, i_int3      ) { nec_interrupt(3, BRK); CLKS(50,50,24); }
OP( 0xcd, i_int       ) { nec_interrupt(fetch(), BRK); CLKS(50,50,24); }
OP( 0xce, i_into      ) { if (OF) { nec_interrupt(NEC_BRKV_VECTOR, BRK); CLKS(52,52,26); } else CLK(3); }
OP( 0xcf, i_iret      ) { POP(V.ip); POP(Sreg(PS)); i_popf(); CHANGE_PC; CLKS(39,39,19); }

OP( 0xd0, i_rotshft_b ) {
	UINT32 src, dst; GetModRM; src = (UINT32)GetRMByte(ModRM); dst=src;
	CLKM(6,6,2,16,16,7);
	switch (ModRM & 0x38) {
		case 0x00: ROL_BYTE;  PutbackRMByte(ModRM,(BYTE)dst); V.OverVal = (src^dst)&0x80; break;
		case 0x08: ROR_BYTE;  PutbackRMByte(ModRM,(BYTE)dst); V.OverVal = (src^dst)&0x80; break;
		case 0x10: ROLC_BYTE; PutbackRMByte(ModRM,(BYTE)dst); V.OverVal = (src^dst)&0x80; break;
		case 0x18: RORC_BYTE; PutbackRMByte(ModRM,(BYTE)dst); V.OverVal = (src^dst)&0x80; break;
		case 0x20: SHL_BYTE(1); V.OverVal = (src^dst)&0x80; break;
		case 0x28: SHR_BYTE(1); V.OverVal = (src^dst)&0x80; break;
		case 0x30: logerror("%06x: Undefined opcode 0xd0 0x30 (SHLA)\n",PC()); break;
		case 0x38: SHRA_BYTE(1); V.OverVal = 0; break;
	}
}

OP( 0xd1, i_rotshft_w ) {
	UINT32 src, dst; GetModRM; src = (UINT32)GetRMWord(ModRM); dst=src;
	CLKM(6,6,2,24,16,7);
	switch (ModRM & 0x38) {
		case 0x00: ROL_WORD;  PutbackRMWord(ModRM,(WORD)dst); V.OverVal = (src^dst)&0x8000; break;
		case 0x08: ROR_WORD;  PutbackRMWord(ModRM,(WORD)dst); V.OverVal = (src^dst)&0x8000; break;
		case 0x10: ROLC_WORD; PutbackRMWord(ModRM,(WORD)dst); V.OverVal = (src^dst)&0x8000; break;
		case 0x18: RORC_WORD; PutbackRMWord(ModRM,(WORD)dst); V.OverVal = (src^dst)&0x8000; break;
		case 0x20: SHL_WORD(1); V.OverVal = (src^dst)&0x8000;  break;
		case 0x28: SHR_WORD(1); V.OverVal = (src^dst)&0x8000;  break;
		case 0x30: logerror("%06x: Undefined opcode 0xd1 0x30 (SHLA)\n",PC()); break;
		case 0x38: SHRA_WORD(1); V.OverVal = 0; break;
	}
}

OP( 0xd2, i_rotshft_bcl ) {
	UINT32 src, dst; UINT8 c; GetModRM; src = (UINT32)GetRMByte(ModRM); dst=src;
	c=Breg(CL);
	CLKM(7,7,2,19,19,6);
	if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_BYTE;  c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x08: do { ROR_BYTE;  c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x10: do { ROLC_BYTE; c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x18: do { RORC_BYTE; c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break;
		case 0x20: SHL_BYTE(c); break;
		case 0x28: SHR_BYTE(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xd2 0x30 (SHLA)\n",PC()); break;
		case 0x38: SHRA_BYTE(c); break;
	}
}

OP( 0xd3, i_rotshft_wcl ) {
	UINT32 src, dst; UINT8 c; GetModRM; src = (UINT32)GetRMWord(ModRM); dst=src;
	c=Breg(CL);
	CLKM(7,7,2,27,19,6);
	if (c) switch (ModRM & 0x38) {
		case 0x00: do { ROL_WORD;  c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x08: do { ROR_WORD;  c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x10: do { ROLC_WORD; c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x18: do { RORC_WORD; c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break;
		case 0x20: SHL_WORD(c); break;
		case 0x28: SHR_WORD(c); break;
		case 0x30: logerror("%06x: Undefined opcode 0xd3 0x30 (SHLA)\n",PC()); break;
		case 0x38: SHRA_WORD(c); break;
	}
}

OP( 0xd4, i_aam    ) { fetch(); Breg(AH) = Breg(AL) / 10; Breg(AL) %= 10; SetSZPF_Word(Wreg(AW)); CLKS(15,15,12); }
OP( 0xd5, i_aad    ) { fetch(); Breg(AL) = Breg(AH) * 10 + Breg(AL); Breg(AH) = 0; SetSZPF_Byte(Breg(AL)); CLKS(7,7,8); }
OP( 0xd6, i_setalc ) { Breg(AL) = (CF)?0xff:0x00; v25_ICount-=3; logerror("%06x: Undefined opcode (SETALC)\n",PC()); }
OP( 0xd7, i_trans  ) { UINT32 dest = (Wreg(BW)+Breg(AL))&0xffff; Breg(AL) = GetMemB(DS0, dest); CLKS(9,9,5); }
OP( 0xd8, i_fpo    ) { GetModRM; GetRMByte(ModRM); v25_ICount-=2;  logerror("%06x: Unimplemented floating point control %04x\n",PC(),ModRM); }

OP( 0xe0, i_loopne ) { INT8 disp = (INT8)fetch(); Wreg(CW)--; if (!ZF && Wreg(CW)) { V.ip = (WORD)(V.ip+disp); /*CHANGE_PC;*/ CLKS(14,14,6); } else CLKS(5,5,3); }
OP( 0xe1, i_loope  ) { INT8 disp = (INT8)fetch(); Wreg(CW)--; if ( ZF && Wreg(CW)) { V.ip = (WORD)(V.ip+disp); /*CHANGE_PC;*/ CLKS(14,14,6); } else CLKS(5,5,3); }
OP( 0xe2, i_loop   ) { INT8 disp = (INT8)fetch(); Wreg(CW)--; if (Wreg(CW)) { V.ip = (WORD)(V.ip+disp); /*CHANGE_PC;*/ CLKS(13,13,6); } else CLKS(5,5,3); }
OP( 0xe3, i_jcxz   ) { INT8 disp = (INT8)fetch(); if (Wreg(CW) == 0) { V.ip = (WORD)(V.ip+disp); /*CHANGE_PC;*/ CLKS(13,13,6); } else CLKS(5,5,3); }
OP( 0xe4, i_inal   ) { UINT8 port = fetch(); Breg(AL) = read_port_byte(port); CLKS(9,9,5);  }
OP( 0xe5, i_inax   ) { UINT8 port = fetch(); Wreg(AW) = read_port_word(port); CLKW(13,13,7,13,9,5,port); }
OP( 0xe6, i_outal  ) { UINT8 port = fetch(); write_port_byte(port, Breg(AL)); CLKS(8,8,3);  }
OP( 0xe7, i_outax  ) { UINT8 port = fetch(); write_port_word(port, Wreg(AW)); CLKW(12,12,5,12,8,3,port);    }

OP( 0xe8, i_call_d16 ) { UINT32 tmp; tmp = fetchword(); PUSH(V.ip); V.ip = (WORD)(V.ip+(INT16)tmp); CHANGE_PC; v25_ICount-=24; }
OP( 0xe9, i_jmp_d16  ) { UINT32 tmp; tmp = fetchword(); V.ip = (WORD)(V.ip+(INT16)tmp); CHANGE_PC; v25_ICount-=15; }
OP( 0xea, i_jmp_far  ) { UINT32 tmp,tmp1; tmp = fetchword(); tmp1 = fetchword(); Sreg(PS) = (WORD)tmp1;     V.ip = (WORD)tmp; CHANGE_PC; v25_ICount-=27;  }
OP( 0xeb, i_jmp_d8   ) { int tmp = (int)((INT8)fetch()); v25_ICount-=12; V.ip = (WORD)(V.ip+tmp); }
OP( 0xec, i_inaldx   ) { Breg(AL) = read_port_byte(Wreg(DW)); CLKS(8,8,5);}
OP( 0xed, i_inaxdx   ) { Wreg(AW) = read_port_word(Wreg(DW)); CLKW(12,12,7,12,8,5,Wreg(DW)); }
OP( 0xee, i_outdxal  ) { write_port_byte(Wreg(DW), Breg(AL)); CLKS(8,8,3);  }
OP( 0xef, i_outdxax  ) { write_port_word(Wreg(DW), Wreg(AW)); CLKW(12,12,5,12,8,3,Wreg(DW)); }

OP( 0xf0, i_lock     ) { logerror("%06x: Warning - BUSLOCK\n",PC()); V.no_interrupt=1; CLK(2); }
OP( 0xf2, i_repne    ) { UINT32 next = fetchop(); UINT16 c = Wreg(CW);
	switch(next) { /* Segments */
		case 0x26:  V.seg_prefix=1; V.prefix_base=Sreg(DS1)<<4;    next = fetchop();  CLK(2); break;
		case 0x2e:  V.seg_prefix=1; V.prefix_base=Sreg(PS)<<4;     next = fetchop();  CLK(2); break;
		case 0x36:  V.seg_prefix=1; V.prefix_base=Sreg(SS)<<4;     next = fetchop();  CLK(2); break;
		case 0x3e:  V.seg_prefix=1; V.prefix_base=Sreg(DS0)<<4;    next = fetchop();  CLK(2); break;
	}

	switch(next) {
		case 0x6c:  CLK(2); if (c) do { i_insb();  c--; } while (c>0); Wreg(CW)=c; break;
		case 0x6d:  CLK(2); if (c) do { i_insw();  c--; } while (c>0); Wreg(CW)=c; break;
		case 0x6e:  CLK(2); if (c) do { i_outsb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0x6f:  CLK(2); if (c) do { i_outsw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xa4:  CLK(2); if (c) do { i_movsb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xa5:  CLK(2); if (c) do { i_movsw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xa6:  CLK(2); if (c) do { i_cmpsb(); c--; } while (c>0 && !ZF);    Wreg(CW)=c; break;
		case 0xa7:  CLK(2); if (c) do { i_cmpsw(); c--; } while (c>0 && !ZF);    Wreg(CW)=c; break;
		case 0xaa:  CLK(2); if (c) do { i_stosb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xab:  CLK(2); if (c) do { i_stosw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xac:  CLK(2); if (c) do { i_lodsb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xad:  CLK(2); if (c) do { i_lodsw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xae:  CLK(2); if (c) do { i_scasb(); c--; } while (c>0 && !ZF);    Wreg(CW)=c; break;
		case 0xaf:  CLK(2); if (c) do { i_scasw(); c--; } while (c>0 && !ZF);    Wreg(CW)=c; break;
		default:    logerror("%06x: REPNE invalid\n",PC());    v25_instruction[next]();
	}
	V.seg_prefix=0;
}
OP( 0xf3, i_repe     ) { UINT32 next = fetchop(); UINT16 c = Wreg(CW);
	switch(next) { /* Segments */
		case 0x26:  V.seg_prefix=1; V.prefix_base=Sreg(DS1)<<4;    next = fetchop();  CLK(2); break;
		case 0x2e:  V.seg_prefix=1; V.prefix_base=Sreg(PS)<<4; next = fetchop();  CLK(2); break;
		case 0x36:  V.seg_prefix=1; V.prefix_base=Sreg(SS)<<4; next = fetchop();  CLK(2); break;
		case 0x3e:  V.seg_prefix=1; V.prefix_base=Sreg(DS0)<<4;    next = fetchop();  CLK(2); break;
	}

	switch(next) {
		case 0x6c:  CLK(2); if (c) do { i_insb();  c--; } while (c>0); Wreg(CW)=c; break;
		case 0x6d:  CLK(2); if (c) do { i_insw();  c--; } while (c>0); Wreg(CW)=c; break;
		case 0x6e:  CLK(2); if (c) do { i_outsb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0x6f:  CLK(2); if (c) do { i_outsw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xa4:  CLK(2); if (c) do { i_movsb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xa5:  CLK(2); if (c) do { i_movsw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xa6:  CLK(2); if (c) do { i_cmpsb(); c--; } while (c>0 && ZF);    Wreg(CW)=c; break;
		case 0xa7:  CLK(2); if (c) do { i_cmpsw(); c--; } while (c>0 && ZF);    Wreg(CW)=c; break;
		case 0xaa:  CLK(2); if (c) do { i_stosb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xab:  CLK(2); if (c) do { i_stosw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xac:  CLK(2); if (c) do { i_lodsb(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xad:  CLK(2); if (c) do { i_lodsw(); c--; } while (c>0); Wreg(CW)=c; break;
		case 0xae:  CLK(2); if (c) do { i_scasb(); c--; } while (c>0 && ZF);    Wreg(CW)=c; break;
		case 0xaf:  CLK(2); if (c) do { i_scasw(); c--; } while (c>0 && ZF);    Wreg(CW)=c; break;
		default:    logerror("%06x: REPE invalid\n",PC()); v25_instruction[next]();
	}
	V.seg_prefix=0;
}
OP( 0xf4, i_hlt ) { V.halted=1; v25_ICount=0; }
OP( 0xf5, i_cmc ) { V.CarryVal = !CF; CLK(2); }
OP( 0xf6, i_f6pre ) { UINT32 tmp; UINT32 uresult,uresult2; INT32 result,result2;
	GetModRM; tmp = GetRMByte(ModRM);
	switch (ModRM & 0x38) {
		case 0x00: tmp &= fetch(); V.CarryVal = V.OverVal = 0; SetSZPF_Byte(tmp); v25_ICount-=(ModRM >=0xc0 )?4:11; break; /* TEST */
		case 0x08: logerror("%06x: Undefined opcode 0xf6 0x08\n",PC()); break;
		case 0x10: PutbackRMByte(ModRM,~tmp); v25_ICount-=(ModRM >=0xc0 )?2:16; break; /* NOT */
		case 0x18: V.CarryVal=(tmp!=0); tmp=(~tmp)+1; SetSZPF_Byte(tmp); PutbackRMByte(ModRM,tmp&0xff); v25_ICount-=(ModRM >=0xc0 )?2:16; break; /* NEG */
		case 0x20: uresult = Breg(AL)*tmp; Wreg(AW)=(WORD)uresult; V.CarryVal=V.OverVal=(Breg(AH)!=0); v25_ICount-=(ModRM >=0xc0 )?30:36; break; /* MULU */
		case 0x28: result = (INT16)((INT8)Breg(AL))*(INT16)((INT8)tmp); Wreg(AW)=(WORD)result; V.CarryVal=V.OverVal=(Breg(AH)!=0); v25_ICount-=(ModRM >=0xc0 )?30:36; break; /* MUL */
		case 0x30: if (tmp) { DIVUB; } else nec_interrupt(NEC_DIVIDE_VECTOR, BRK); v25_ICount-=(ModRM >=0xc0 )?43:53; break;
		case 0x38: if (tmp) { DIVB;  } else nec_interrupt(NEC_DIVIDE_VECTOR, BRK); v25_ICount-=(ModRM >=0xc0 )?43:53; break;
	}
}

OP( 0xf7, i_f7pre   ) { UINT32 tmp,tmp2; UINT32 uresult,uresult2; INT32 result,result2;
	GetModRM; tmp = GetRMWord(ModRM);
	switch (ModRM & 0x38) {
		case 0x00: tmp2 = fetchword(); tmp &= tmp2; V.CarryVal = V.OverVal = 0; SetSZPF_Word(tmp); v25_ICount-=(ModRM >=0xc0 )?4:11; break; /* TEST */
		case 0x08: logerror("%06x: Undefined opcode 0xf7 0x08\n",PC()); break;
		case 0x10: PutbackRMWord(ModRM,~tmp); v25_ICount-=(ModRM >=0xc0 )?2:16; break; /* NOT */
		case 0x18: V.CarryVal=(tmp!=0); tmp=(~tmp)+1; SetSZPF_Word(tmp); PutbackRMWord(ModRM,tmp&0xffff); v25_ICount-=(ModRM >=0xc0 )?2:16; break; /* NEG */
		case 0x20: uresult = Wreg(AW)*tmp; Wreg(AW)=uresult&0xffff; Wreg(DW)=((UINT32)uresult)>>16; V.CarryVal=V.OverVal=(Wreg(DW)!=0); v25_ICount-=(ModRM >=0xc0 )?30:36; break; /* MULU */
		case 0x28: result = (INT32)((INT16)Wreg(AW))*(INT32)((INT16)tmp); Wreg(AW)=result&0xffff; Wreg(DW)=result>>16; V.CarryVal=V.OverVal=(Wreg(DW)!=0); v25_ICount-=(ModRM >=0xc0 )?30:36; break; /* MUL */
		case 0x30: if (tmp) { DIVUW; } else nec_interrupt(NEC_DIVIDE_VECTOR, BRK); v25_ICount-=(ModRM >=0xc0 )?43:53; break;
		case 0x38: if (tmp) { DIVW;  } else nec_interrupt(NEC_DIVIDE_VECTOR, BRK); v25_ICount-=(ModRM >=0xc0 )?43:53; break;
	}
}

OP( 0xf8, i_clc   ) { V.CarryVal = 0;  CLK(2); }
OP( 0xf9, i_stc   ) { V.CarryVal = 1;  CLK(2); }
OP( 0xfa, i_di    ) { SetIF(0);         CLK(2); }
OP( 0xfb, i_ei    ) { SetIF(1);         CLK(2); }
OP( 0xfc, i_cld   ) { SetDF(0);         CLK(2); }
OP( 0xfd, i_std   ) { SetDF(1);         CLK(2); }
OP( 0xfe, i_fepre ) { UINT32 tmp, tmp1; GetModRM; tmp=GetRMByte(ModRM);
	switch(ModRM & 0x38) {
		case 0x00: tmp1 = tmp+1; V.OverVal = (tmp==0x7f); SetAF(tmp1,tmp,1); SetSZPF_Byte(tmp1); PutbackRMByte(ModRM,(BYTE)tmp1); CLKM(2,2,2,16,16,7); break; /* INC */
		case 0x08: tmp1 = tmp-1; V.OverVal = (tmp==0x80); SetAF(tmp1,tmp,1); SetSZPF_Byte(tmp1); PutbackRMByte(ModRM,(BYTE)tmp1); CLKM(2,2,2,16,16,7); break; /* DEC */
		default:   logerror("%06x: FE Pre with unimplemented mod\n",PC());
	}
}
OP( 0xff, i_ffpre ) { UINT32 tmp, tmp1; GetModRM; tmp=GetRMWord(ModRM);
	switch(ModRM & 0x38) {
		case 0x00: tmp1 = tmp+1; V.OverVal = (tmp==0x7fff); SetAF(tmp1,tmp,1); SetSZPF_Word(tmp1); PutbackRMWord(ModRM,(WORD)tmp1); CLKM(2,2,2,24,16,7); break; /* INC */
		case 0x08: tmp1 = tmp-1; V.OverVal = (tmp==0x8000); SetAF(tmp1,tmp,1); SetSZPF_Word(tmp1); PutbackRMWord(ModRM,(WORD)tmp1); CLKM(2,2,2,24,16,7); break; /* DEC */
		case 0x10: PUSH(V.ip); V.ip = (WORD)tmp; CHANGE_PC; v25_ICount-=(ModRM >=0xc0 )?16:20; break; /* CALL */
		case 0x18: tmp1 = Sreg(PS); Sreg(PS) = GetnextRMWord; PUSH(tmp1); PUSH(V.ip); V.ip = tmp; CHANGE_PC; v25_ICount-=(ModRM >=0xc0 )?16:26; break; /* CALL FAR */
		case 0x20: V.ip = tmp; CHANGE_PC; v25_ICount-=13; break; /* JMP */
		case 0x28: V.ip = tmp; Sreg(PS) = GetnextRMWord; CHANGE_PC; v25_ICount-=15; break; /* JMP FAR */
		case 0x30: PUSH(tmp); v25_ICount-=4; break;
		default:   logerror("%06x: FF Pre with unimplemented mod\n",PC());
	}
}

static void i_invalid(void)
{
	v25_ICount-=10;
	logerror("%06x: Invalid Opcode\n",PC());
}

#define GetRB   \
	ModRM = fetch();    \
	if (ModRM >= 0xc0)  \
		tmp = Wreg(Mod_RM.RM.w[ModRM]) & 0x7;   \
	else {                          \
		logerror("%06x: Invalid MODRM for register banking instruction\n",PC());   \
		tmp = 0;    \
	}

#define RETRBI  \
	tmp = (Wreg(PSW_SAVE) & 0x7000) >> 12;  \
	V.ip = Wreg(PC_SAVE);  \
	ExpandFlags(Wreg(PSW_SAVE));    \
	SetRB(tmp); \
	CHANGE_PC

#define TSKSW   \
	Wreg(PSW_SAVE) = CompressFlags();   \
	Wreg(PC_SAVE) = V.ip;  \
	SetRB(tmp); \
	V.ip = Wreg(PC_SAVE);  \
	ExpandFlags(Wreg(PSW_SAVE));    \
	CHANGE_PC

#define MOVSPA  \
	tmp = (Wreg(PSW_SAVE) & 0x7000) >> 8;   \
	Sreg(SS) = V.internal_ram[tmp+SS];    \
	Wreg(SP) = V.internal_ram[tmp+SP]

#define MOVSPB  \
	tmp <<= 4;  \
	V.internal_ram[tmp+SS] = Sreg(SS);    \
	V.internal_ram[tmp+SP] = Wreg(SP)

#define FINT    \
	for(tmp = 1; tmp < 0x100; tmp <<= 1) {  \
		if(V.ISPR & tmp) {     \
			V.ISPR &= ~tmp;    \
			break;  \
		}   \
	}

OP( 0x0f, i_pre_v25  ) { UINT32 ModRM, tmp, tmp2;
	switch (fetch()) {
		case 0x10 : BITOP_BYTE; CLKS(3,3,4); tmp2 = Breg(CL) & 0x7; V.ZeroVal = (tmp & (1<<tmp2)) ? 1 : 0; V.CarryVal=V.OverVal=0; break; /* Test */
		case 0x11 : BITOP_WORD; CLKS(3,3,4); tmp2 = Breg(CL) & 0xf; V.ZeroVal = (tmp & (1<<tmp2)) ? 1 : 0; V.CarryVal=V.OverVal=0; break; /* Test */
		case 0x12 : BITOP_BYTE; CLKS(5,5,4); tmp2 = Breg(CL) & 0x7; tmp &= ~(1<<tmp2);  PutbackRMByte(ModRM,tmp);   break; /* Clr */
		case 0x13 : BITOP_WORD; CLKS(5,5,4); tmp2 = Breg(CL) & 0xf; tmp &= ~(1<<tmp2);  PutbackRMWord(ModRM,tmp);   break; /* Clr */
		case 0x14 : BITOP_BYTE; CLKS(4,4,4); tmp2 = Breg(CL) & 0x7; tmp |= (1<<tmp2);   PutbackRMByte(ModRM,tmp);   break; /* Set */
		case 0x15 : BITOP_WORD; CLKS(4,4,4); tmp2 = Breg(CL) & 0xf; tmp |= (1<<tmp2);   PutbackRMWord(ModRM,tmp);   break; /* Set */
		case 0x16 : BITOP_BYTE; CLKS(4,4,4); tmp2 = Breg(CL) & 0x7; BIT_NOT;            PutbackRMByte(ModRM,tmp);   break; /* Not */
		case 0x17 : BITOP_WORD; CLKS(4,4,4); tmp2 = Breg(CL) & 0xf; BIT_NOT;            PutbackRMWord(ModRM,tmp);   break; /* Not */

		case 0x18 : BITOP_BYTE; CLKS(4,4,4); tmp2 = (fetch()) & 0x7;    V.ZeroVal = (tmp & (1<<tmp2)) ? 1 : 0; V.CarryVal=V.OverVal=0; break; /* Test */
		case 0x19 : BITOP_WORD; CLKS(4,4,4); tmp2 = (fetch()) & 0xf;    V.ZeroVal = (tmp & (1<<tmp2)) ? 1 : 0; V.CarryVal=V.OverVal=0; break; /* Test */
		case 0x1a : BITOP_BYTE; CLKS(6,6,4); tmp2 = (fetch()) & 0x7;    tmp &= ~(1<<tmp2);      PutbackRMByte(ModRM,tmp);   break; /* Clr */
		case 0x1b : BITOP_WORD; CLKS(6,6,4); tmp2 = (fetch()) & 0xf;    tmp &= ~(1<<tmp2);      PutbackRMWord(ModRM,tmp);   break; /* Clr */
		case 0x1c : BITOP_BYTE; CLKS(5,5,4); tmp2 = (fetch()) & 0x7;    tmp |= (1<<tmp2);       PutbackRMByte(ModRM,tmp);   break; /* Set */
		case 0x1d : BITOP_WORD; CLKS(5,5,4); tmp2 = (fetch()) & 0xf;    tmp |= (1<<tmp2);       PutbackRMWord(ModRM,tmp);   break; /* Set */
		case 0x1e : BITOP_BYTE; CLKS(5,5,4); tmp2 = (fetch()) & 0x7;    BIT_NOT;                PutbackRMByte(ModRM,tmp);   break; /* Not */
		case 0x1f : BITOP_WORD; CLKS(5,5,4); tmp2 = (fetch()) & 0xf;    BIT_NOT;                PutbackRMWord(ModRM,tmp);   break; /* Not */

		case 0x20 : ADD4S; CLKS(7,7,2); break;
		case 0x22 : SUB4S; CLKS(7,7,2); break;
		case 0x25 : MOVSPA; CLK(16); break;
		case 0x26 : CMP4S; CLKS(7,7,2); break;
		case 0x28 : ModRM = fetch(); tmp = GetRMByte(ModRM); tmp <<= 4; tmp |= Breg(AL) & 0xf; Breg(AL) = (Breg(AL) & 0xf0) | ((tmp>>8)&0xf); tmp &= 0xff; PutbackRMByte(ModRM,tmp); CLKM(13,13,9,28,28,15); break;
		case 0x2a : ModRM = fetch(); tmp = GetRMByte(ModRM); tmp2 = (Breg(AL) & 0xf)<<4; Breg(AL) = (Breg(AL) & 0xf0) | (tmp&0xf); tmp = tmp2 | (tmp>>4);   PutbackRMByte(ModRM,tmp); CLKM(17,17,13,32,32,19); break;
		case 0x2d : GetRB; nec_bankswitch(tmp); CLK(15); break;
		case 0x31 : ModRM = fetch(); ModRM=0; logerror("%06x: Unimplemented bitfield INS\n",PC()); break;
		case 0x33 : ModRM = fetch(); ModRM=0; logerror("%06x: Unimplemented bitfield EXT\n",PC()); break;
		case 0x91 : RETRBI; CLK(12); break;
		case 0x92 : FINT; CLK(2); V.no_interrupt = 1; break;
		case 0x94 : GetRB; TSKSW; CLK(20); break;
		case 0x95 : GetRB; MOVSPB; CLK(11); break;
		case 0x9e : logerror("%06x: STOP\n",PC()); v25_ICount=0; break;
		default:    logerror("%06x: Unknown V25 instruction\n",PC()); break;
	}
}

OP( 0x63, i_brkn   ) { nec_interrupt(fetch(), BRKN); CLKS(50,50,24); }
OP( 0xF1, i_brks   ) { nec_interrupt(fetch(), BRKS); CLKS(50,50,24); }

void (*const v25_instruction[256])(void) =
{
	i_add_br8,      /* 0x00 */
	i_add_wr16,     /* 0x01 */
	i_add_r8b,      /* 0x02 */
	i_add_r16w,     /* 0x03 */
	i_add_ald8,     /* 0x04 */
	i_add_axd16,    /* 0x05 */
	i_push_es,      /* 0x06 */
	i_pop_es,       /* 0x07 */
	i_or_br8,       /* 0x08 */
	i_or_wr16,      /* 0x09 */
	i_or_r8b,       /* 0x0a */
	i_or_r16w,      /* 0x0b */
	i_or_ald8,      /* 0x0c */
	i_or_axd16,     /* 0x0d */
	i_push_cs,      /* 0x0e */
	i_pre_v25,      /* 0x0f */
	i_adc_br8,      /* 0x10 */
	i_adc_wr16,     /* 0x11 */
	i_adc_r8b,      /* 0x12 */
	i_adc_r16w,     /* 0x13 */
	i_adc_ald8,     /* 0x14 */
	i_adc_axd16,    /* 0x15 */
	i_push_ss,      /* 0x16 */
	i_pop_ss,       /* 0x17 */
	i_sbb_br8,      /* 0x18 */
	i_sbb_wr16,     /* 0x19 */
	i_sbb_r8b,      /* 0x1a */
	i_sbb_r16w,     /* 0x1b */
	i_sbb_ald8,     /* 0x1c */
	i_sbb_axd16,    /* 0x1d */
	i_push_ds,      /* 0x1e */
	i_pop_ds,       /* 0x1f */
	i_and_br8,      /* 0x20 */
	i_and_wr16,     /* 0x21 */
	i_and_r8b,      /* 0x22 */
	i_and_r16w,     /* 0x23 */
	i_and_ald8,     /* 0x24 */
	i_and_axd16,    /* 0x25 */
	i_es,           /* 0x26 */
	i_daa,          /* 0x27 */
	i_sub_br8,      /* 0x28 */
	i_sub_wr16,     /* 0x29 */
	i_sub_r8b,      /* 0x2a */
	i_sub_r16w,     /* 0x2b */
	i_sub_ald8,     /* 0x2c */
	i_sub_axd16,    /* 0x2d */
	i_cs,           /* 0x2e */
	i_das,          /* 0x2f */
	i_xor_br8,      /* 0x30 */
	i_xor_wr16,     /* 0x31 */
	i_xor_r8b,      /* 0x32 */
	i_xor_r16w,     /* 0x33 */
	i_xor_ald8,     /* 0x34 */
	i_xor_axd16,    /* 0x35 */
	i_ss,           /* 0x36 */
	i_aaa,          /* 0x37 */
	i_cmp_br8,      /* 0x38 */
	i_cmp_wr16,     /* 0x39 */
	i_cmp_r8b,      /* 0x3a */
	i_cmp_r16w,     /* 0x3b */
	i_cmp_ald8,     /* 0x3c */
	i_cmp_axd16,    /* 0x3d */
	i_ds,           /* 0x3e */
	i_aas,          /* 0x3f */
	i_inc_ax,       /* 0x40 */
	i_inc_cx,       /* 0x41 */
	i_inc_dx,       /* 0x42 */
	i_inc_bx,       /* 0x43 */
	i_inc_sp,       /* 0x44 */
	i_inc_bp,       /* 0x45 */
	i_inc_si,       /* 0x46 */
	i_inc_di,       /* 0x47 */
	i_dec_ax,       /* 0x48 */
	i_dec_cx,       /* 0x49 */
	i_dec_dx,       /* 0x4a */
	i_dec_bx,       /* 0x4b */
	i_dec_sp,       /* 0x4c */
	i_dec_bp,       /* 0x4d */
	i_dec_si,       /* 0x4e */
	i_dec_di,       /* 0x4f */
	i_push_ax,      /* 0x50 */
	i_push_cx,      /* 0x51 */
	i_push_dx,      /* 0x52 */
	i_push_bx,      /* 0x53 */
	i_push_sp,      /* 0x54 */
	i_push_bp,      /* 0x55 */
	i_push_si,      /* 0x56 */
	i_push_di,      /* 0x57 */
	i_pop_ax,       /* 0x58 */
	i_pop_cx,       /* 0x59 */
	i_pop_dx,       /* 0x5a */
	i_pop_bx,       /* 0x5b */
	i_pop_sp,       /* 0x5c */
	i_pop_bp,       /* 0x5d */
	i_pop_si,       /* 0x5e */
	i_pop_di,       /* 0x5f */
	i_pusha,        /* 0x60 */
	i_popa,         /* 0x61 */
	i_chkind,       /* 0x62 */
	i_brkn,         /* 0x63 - V25S/V35S only */
	i_repnc,        /* 0x64 */
	i_repc,         /* 0x65 */
	i_invalid,      /* 0x66 */
	i_invalid,      /* 0x67 */
	i_push_d16,     /* 0x68 */
	i_imul_d16,     /* 0x69 */
	i_push_d8,      /* 0x6a */
	i_imul_d8,      /* 0x6b */
	i_insb,         /* 0x6c */
	i_insw,         /* 0x6d */
	i_outsb,        /* 0x6e */
	i_outsw,        /* 0x6f */
	i_jo,           /* 0x70 */
	i_jno,          /* 0x71 */
	i_jc,           /* 0x72 */
	i_jnc,          /* 0x73 */
	i_jz,           /* 0x74 */
	i_jnz,          /* 0x75 */
	i_jce,          /* 0x76 */
	i_jnce,         /* 0x77 */
	i_js,           /* 0x78 */
	i_jns,          /* 0x79 */
	i_jp,           /* 0x7a */
	i_jnp,          /* 0x7b */
	i_jl,           /* 0x7c */
	i_jnl,          /* 0x7d */
	i_jle,          /* 0x7e */
	i_jnle,         /* 0x7f */
	i_80pre,        /* 0x80 */
	i_81pre,        /* 0x81 */
	i_82pre,        /* 0x82 */
	i_83pre,        /* 0x83 */
	i_test_br8,     /* 0x84 */
	i_test_wr16,    /* 0x85 */
	i_xchg_br8,     /* 0x86 */
	i_xchg_wr16,    /* 0x87 */
	i_mov_br8,      /* 0x88 */
	i_mov_wr16,     /* 0x89 */
	i_mov_r8b,      /* 0x8a */
	i_mov_r16w,     /* 0x8b */
	i_mov_wsreg,    /* 0x8c */
	i_lea,          /* 0x8d */
	i_mov_sregw,    /* 0x8e */
	i_popw,         /* 0x8f */
	i_nop,          /* 0x90 */
	i_xchg_axcx,    /* 0x91 */
	i_xchg_axdx,    /* 0x92 */
	i_xchg_axbx,    /* 0x93 */
	i_xchg_axsp,    /* 0x94 */
	i_xchg_axbp,    /* 0x95 */
	i_xchg_axsi,    /* 0x96 */
	i_xchg_axdi,    /* 0x97 */
	i_cbw,          /* 0x98 */
	i_cwd,          /* 0x99 */
	i_call_far,     /* 0x9a */
	i_wait,         /* 0x9b */
	i_pushf,        /* 0x9c */
	i_popf,         /* 0x9d */
	i_sahf,         /* 0x9e */
	i_lahf,         /* 0x9f */
	i_mov_aldisp,   /* 0xa0 */
	i_mov_axdisp,   /* 0xa1 */
	i_mov_dispal,   /* 0xa2 */
	i_mov_dispax,   /* 0xa3 */
	i_movsb,        /* 0xa4 */
	i_movsw,        /* 0xa5 */
	i_cmpsb,        /* 0xa6 */
	i_cmpsw,        /* 0xa7 */
	i_test_ald8,    /* 0xa8 */
	i_test_axd16,   /* 0xa9 */
	i_stosb,        /* 0xaa */
	i_stosw,        /* 0xab */
	i_lodsb,        /* 0xac */
	i_lodsw,        /* 0xad */
	i_scasb,        /* 0xae */
	i_scasw,        /* 0xaf */
	i_mov_ald8,     /* 0xb0 */
	i_mov_cld8,     /* 0xb1 */
	i_mov_dld8,     /* 0xb2 */
	i_mov_bld8,     /* 0xb3 */
	i_mov_ahd8,     /* 0xb4 */
	i_mov_chd8,     /* 0xb5 */
	i_mov_dhd8,     /* 0xb6 */
	i_mov_bhd8,     /* 0xb7 */
	i_mov_axd16,    /* 0xb8 */
	i_mov_cxd16,    /* 0xb9 */
	i_mov_dxd16,    /* 0xba */
	i_mov_bxd16,    /* 0xbb */
	i_mov_spd16,    /* 0xbc */
	i_mov_bpd16,    /* 0xbd */
	i_mov_sid16,    /* 0xbe */
	i_mov_did16,    /* 0xbf */
	i_rotshft_bd8,  /* 0xc0 */
	i_rotshft_wd8,  /* 0xc1 */
	i_ret_d16,      /* 0xc2 */
	i_ret,          /* 0xc3 */
	i_les_dw,       /* 0xc4 */
	i_lds_dw,       /* 0xc5 */
	i_mov_bd8,      /* 0xc6 */
	i_mov_wd16,     /* 0xc7 */
	i_enter,        /* 0xc8 */
	i_leave,        /* 0xc9 */
	i_retf_d16,     /* 0xca */
	i_retf,         /* 0xcb */
	i_int3,         /* 0xcc */
	i_int,          /* 0xcd */
	i_into,         /* 0xce */
	i_iret,         /* 0xcf */
	i_rotshft_b,    /* 0xd0 */
	i_rotshft_w,    /* 0xd1 */
	i_rotshft_bcl,  /* 0xd2 */
	i_rotshft_wcl,  /* 0xd3 */
	i_aam,          /* 0xd4 */
	i_aad,          /* 0xd5 */
	i_setalc,       /* 0xd6 */
	i_trans,        /* 0xd7 */
	i_fpo,          /* 0xd8 */
	i_fpo,          /* 0xd9 */
	i_fpo,          /* 0xda */
	i_fpo,          /* 0xdb */
	i_fpo,          /* 0xdc */
	i_fpo,          /* 0xdd */
	i_fpo,          /* 0xde */
	i_fpo,          /* 0xdf */
	i_loopne,       /* 0xe0 */
	i_loope,        /* 0xe1 */
	i_loop,         /* 0xe2 */
	i_jcxz,         /* 0xe3 */
	i_inal,         /* 0xe4 */
	i_inax,         /* 0xe5 */
	i_outal,        /* 0xe6 */
	i_outax,        /* 0xe7 */
	i_call_d16,     /* 0xe8 */
	i_jmp_d16,      /* 0xe9 */
	i_jmp_far,      /* 0xea */
	i_jmp_d8,       /* 0xeb */
	i_inaldx,       /* 0xec */
	i_inaxdx,       /* 0xed */
	i_outdxal,      /* 0xee */
	i_outdxax,      /* 0xef */
	i_lock,         /* 0xf0 */
	i_brks,         /* 0xf1 */
	i_repne,        /* 0xf2 */
	i_repe,         /* 0xf3 */
	i_hlt,          /* 0xf4 */
	i_cmc,          /* 0xf5 */
	i_f6pre,        /* 0xf6 */
	i_f7pre,        /* 0xf7 */
	i_clc,          /* 0xf8 */
	i_stc,          /* 0xf9 */
	i_di,           /* 0xfa */
	i_ei,           /* 0xfb */
	i_cld,          /* 0xfc */
	i_std,          /* 0xfd */
	i_fepre,        /* 0xfe */
	i_ffpre         /* 0xff */
};
