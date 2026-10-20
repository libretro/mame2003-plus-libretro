/***************************************************************************

    ADSP-21062 SHARC opcodes, from current MAME
    (src/devices/cpu/sharc/sharcops.hxx and sharcops_table.cpp,
    copyright Ville Linde, BSD-3-Clause), translated to C for MAME 2003.
    Included from sharc.c, not compiled on its own.

    Port note: every opcode with a data memory (DM) access first does
    the access through DM_PRE_R / DM_PRE_W when it goes to an external
    (driver) address, before any other side effect; if the driver
    stalls the access (sharc_stall) the opcode returns at once and the
    execute loop rewinds the fetch state, so the instruction is run
    again later from scratch. The access done at the MAME position
    then uses the prefetched value (reads) or is skipped (writes).

***************************************************************************/

#define PM_REG_I(x)         (m_core->dag2.i[x])
#define PM_REG_M(x)         (m_core->dag2.m[x])
#define PM_REG_B(x)         (m_core->dag2.b[x])
#define PM_REG_L(x)         (m_core->dag2.l[x])
#define DM_REG_I(x)         (m_core->dag1.i[x])
#define DM_REG_M(x)         (m_core->dag1.m[x])
#define DM_REG_B(x)         (m_core->dag1.b[x])
#define DM_REG_L(x)         (m_core->dag1.l[x])

#define UREG_PC             0x63
#define UREG_PCSTK          0x64
#define UREG_PCSTKP         0x65
#define UREG_LADDR          0x66
#define UREG_CURLCNTR       0x67
#define UREG_LCNTR          0x68
#define UREG_USTAT1         0x70
#define UREG_USTAT2         0x71
#define UREG_IRPTL          0x79
#define UREG_MODE2          0x7a
#define UREG_MODE1          0x7b
#define UREG_ASTAT          0x7c
#define UREG_IMASK          0x7d
#define UREG_STKY           0x7e
#define UREG_IMASKP         0x7f

/* external DM access done first (see above); return from the opcode on a stall */
#define DM_PRE_R(addr)          do { if (dm_pre_r(addr)) return; } while (0)
#define DM_PRE_W(addr, data)    do { if (dm_pre_w((addr), (data))) return; } while (0)


/* The ADSP-2106x circular buffer occupies [B, B+L), so a post-modified index that reaches B+L wraps. */
#define UPDATE_CIRCULAR_BUFFER_PM(x)                        \
	{                                                       \
		if (PM_REG_L(x) != 0)                               \
		{                                                   \
			if (PM_REG_I(x) >= PM_REG_B(x)+PM_REG_L(x))     \
			{                                               \
				PM_REG_I(x) -= PM_REG_L(x);                 \
			}                                               \
			else if (PM_REG_I(x) < PM_REG_B(x))             \
			{                                               \
				PM_REG_I(x) += PM_REG_L(x);                 \
			}                                               \
		}                                                   \
	}

#define UPDATE_CIRCULAR_BUFFER_DM(x)                        \
	{                                                       \
		if (DM_REG_L(x) != 0)                               \
		{                                                   \
			if (DM_REG_I(x) >= DM_REG_B(x)+DM_REG_L(x))     \
			{                                               \
				DM_REG_I(x) -= DM_REG_L(x);                 \
			}                                               \
			else if (DM_REG_I(x) < DM_REG_B(x))             \
			{                                               \
				DM_REG_I(x) += DM_REG_L(x);                 \
			}                                               \
		}                                                   \
	}

#define SHARC_COND_LT  ((m_core->astat & AF) ? ((m_core->astat & AN) && !(m_core->astat & AZ)) : ((int)((m_core->astat & AN) != 0) != (int)((m_core->astat & AV) && !(m_core->mode1 & MODE1_ALUSAT))))
#define SHARC_COND_LE  ((m_core->astat & AZ) || ((m_core->astat & AF) ? (m_core->astat & AN) : ((int)((m_core->astat & AN) != 0) != (int)((m_core->astat & AV) && !(m_core->mode1 & MODE1_ALUSAT)))))

/*****************************************************************************/

#define SWAP32(a, b)    do { UINT32 swap_tmp = (a); (a) = (b); (b) = swap_tmp; } while (0)

static void swap_dag_regs(SHARC_DAG *dag, SHARC_DAG *alt, int first)
{
	int i;
	for (i = first; i < first + 4; i++)
	{
		SWAP32(dag->i[i], alt->i[i]);
		SWAP32(dag->m[i], alt->m[i]);
		SWAP32(dag->l[i], alt->l[i]);
		SWAP32(dag->b[i], alt->b[i]);
	}
}

static void add_systemreg_write_latency_effect(int sysreg, UINT32 data, UINT32 prev_data)
{
	if (m_core->systemreg_latency_cycles > 0)
	{
		/* already scheduled: apply the old one now (as MAME) */
		systemreg_write_latency_effect();
	}

	m_core->systemreg_latency_cycles = 2;
	m_core->systemreg_latency_reg = sysreg;
	m_core->systemreg_latency_data = data;
	m_core->systemreg_previous_data = prev_data;
}

static void systemreg_write_latency_effect(void)
{
	UINT32 data = m_core->systemreg_latency_data;
	UINT32 old_data = m_core->systemreg_previous_data;

	switch (m_core->systemreg_latency_reg)
	{
		case 0xb:   /* MODE1 */
		{
			UINT32 diff = data ^ old_data;
			int i;
			m_core->mode1 = data;

			if (diff & MODE1_BR8)
			{
				sharc_error("SHARC: systemreg_latency_op: enable I8 bit-reversing\n");
			}
			if (diff & MODE1_BR0)
			{
				sharc_error("SHARC: systemreg_latency_op: enable I0 bit-reversing\n");
			}
			if (diff & MODE1_SRCU)
			{
				sharc_error("SHARC: systemreg_latency_op: enable MR alternate\n");
			}

			if (diff & MODE1_SRD1H) /* Switch DAG1 7-4 */
				swap_dag_regs(&m_core->dag1, &m_core->dag1_alt, 4);
			if (diff & MODE1_SRD1L) /* Switch DAG1 3-0 */
				swap_dag_regs(&m_core->dag1, &m_core->dag1_alt, 0);
			if (diff & MODE1_SRD2H) /* Switch DAG2 15-12 */
				swap_dag_regs(&m_core->dag2, &m_core->dag2_alt, 4);
			if (diff & MODE1_SRD2L) /* Switch DAG2 11-8 */
				swap_dag_regs(&m_core->dag2, &m_core->dag2_alt, 0);
			if (diff & MODE1_SRRFH)
			{
				for (i = 8; i < 16; i++)
				{
					INT32 t = m_core->r[i].r;
					m_core->r[i].r = m_core->reg_alt[i].r;
					m_core->reg_alt[i].r = t;
				}
			}
			if (diff & MODE1_SRRFL)
			{
				for (i = 0; i < 8; i++)
				{
					INT32 t = m_core->r[i].r;
					m_core->r[i].r = m_core->reg_alt[i].r;
					m_core->reg_alt[i].r = t;
				}
			}
			if (diff & MODE1_SSE)   /* Short word sign extension */
			{
				/* MAME selects / disables the sign-extending short word view */
				m_core->dm_short_se = (data & MODE1_SSE) ? 1 : 0;
			}
			break;
		}
		default:
			sharc_error("SHARC: systemreg_latency_op: unknown register %02X at %08X\n", m_core->systemreg_latency_reg, m_core->pc);
			break;
	}

	m_core->systemreg_latency_reg = -1;
}

static UINT32 GET_UREG(int ureg)
{
	int reg = ureg & 0xf;
	switch ((ureg >> 4) & 0xf)
	{
		case 0x0:       /* R0 - R15 */
		{
			return (UINT32)m_core->r[reg].r;
		}

		case 0x1:
		{
			if (reg & 0x8)      /* I8 - I15 */
			{
				return m_core->dag2.i[reg & 0x7];
			}
			else                /* I0 - I7 */
			{
				return m_core->dag1.i[reg & 0x7];
			}
		}

		case 0x2:
		{
			if (reg & 0x8)      /* M8 - M15 */
			{
				INT32 r = (INT32)m_core->dag2.m[reg & 0x7];
				if (r & 0x800000)   r |= (INT32)0xff000000;

				return (UINT32)r;
			}
			else                /* M0 - M7 */
			{
				return m_core->dag1.m[reg & 0x7];
			}
		}

		case 0x3:
		{
			if (reg & 0x8)      /* L8 - L15 */
			{
				return m_core->dag2.l[reg & 0x7];
			}
			else                /* L0 - L7 */
			{
				return m_core->dag1.l[reg & 0x7];
			}
		}

		case 0x4:
		{
			if (reg & 0x8)      /* B8 - B15 */
			{
				return m_core->dag2.b[reg & 0x7];
			}
			else                /* B0 - B7 */
			{
				return m_core->dag1.b[reg & 0x7];
			}
		}

		case 0x6:
		{
			switch (reg)
			{
				case 0x4:   return m_core->pcstk;                       /* PCSTK */
				case 0x5:   return m_core->pcstkp;                      /* PCSTKP */
				case 0x7:   return m_core->curlcntr;                    /* CURLCNTR */
				case 0x8:   return m_core->lcntr;                       /* LCNTR */
				default:    sharc_error("SHARC: GET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc); return 0;
			}
			break;
		}

		case 0x7:
		{
			switch (reg)
			{
				case 0x0:   return m_core->ustat1;        /* USTAT1 */
				case 0x1:   return m_core->ustat2;        /* USTAT2 */
				case 0x9:   return m_core->irptl;         /* IRPTL */
				case 0xa:   return m_core->mode2;         /* MODE2 */
				case 0xb:   return m_core->mode1;         /* MODE1 */
				case 0xc:                                 /* ASTAT */
				{
					UINT32 r = m_core->astat;
					unsigned i;
					r &= (BITS(m_core->mode2, 15, 4) << FLG0_SHIFT) | ~(FLG0 | FLG1 | FLG2 | FLG3);
					for (i = 0; 4 > i; ++i)
					{
						if (!BIT(m_core->mode2, i + 15))
							r |= m_core->flag[i] << (FLG0_SHIFT + i);
					}
					return r;
				}
				case 0xd:   return m_core->imask;         /* IMASK */
				case 0xe:   return m_core->stky;          /* STKY */
				default:    sharc_error("SHARC: GET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc); return 0;
			}
			break;
		}

		case 0xd:
		{
			switch (reg)
			{
				/* PX needs to be handled separately if the whole 48 bits are needed */
				case 0xb:   return (UINT32)(m_core->px);            /* PX */
				case 0xc:   return (UINT16)(m_core->px);            /* PX1 */
				case 0xd:   return (UINT32)(m_core->px >> 16);      /* PX2 */
				default:    sharc_error("SHARC: GET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc); return 0;
			}
			break;
		}

		default:
			sharc_error("SHARC: GET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc);
			return 0;
	}
}

static void SET_UREG(int ureg, UINT32 data)
{
	int reg = ureg & 0xf;
	switch ((ureg >> 4) & 0xf)
	{
		case 0x0:       /* R0 - R15 */
			m_core->r[reg].r = (INT32)data;
			break;

		case 0x1:
			if (reg & 0x8)      /* I8 - I15 */
			{
				m_core->dag2.i[reg & 0x7] = data;
			}
			else                /* I0 - I7 */
			{
				m_core->dag1.i[reg & 0x7] = data;
			}
			break;

		case 0x2:
			if (reg & 0x8)      /* M8 - M15 */
			{
				m_core->dag2.m[reg & 0x7] = data;
			}
			else                /* M0 - M7 */
			{
				m_core->dag1.m[reg & 0x7] = data;
			}
			break;

		case 0x3:
			if (reg & 0x8)      /* L8 - L15 */
			{
				m_core->dag2.l[reg & 0x7] = data;
			}
			else                /* L0 - L7 */
			{
				m_core->dag1.l[reg & 0x7] = data;
			}
			break;

		case 0x4:
			/* Note: loading B also loads the same value in I */
			if (reg & 0x8)      /* B8 - B15 */
			{
				m_core->dag2.b[reg & 0x7] = data;
				m_core->dag2.i[reg & 0x7] = data;
			}
			else                /* B0 - B7 */
			{
				m_core->dag1.b[reg & 0x7] = data;
				m_core->dag1.i[reg & 0x7] = data;
			}
			break;

		case 0x6:
			switch (reg)
			{
				case 0x4:                                     /* PCSTK */
					if ((m_core->pcstkp > 0) && (m_core->pcstkp < 31))
						m_core->pcstk = data & 0x00ffffff;
					break;

				case 0x5:                                     /* PCSTKP */
					if (m_core->pcstkp < 31)
					{
						/* TODO: this should take effect with a one-cycle delay */
						UINT32 prev = m_core->pcstkp;
						m_core->pcstkp = data & 0x1f;

						if (prev < m_core->pcstkp && m_core->pcstkp >= 31)
						{
							/* MAME: fatal error; keep the stack sane */
							sharc_error("SHARC: PC Stack overflow!\n");
							m_core->pcstkp = prev;
							break;
						}

						if (prev > 0)
							m_core->pcstack[prev - 1] = m_core->pcstk;

						if (prev < m_core->pcstkp)
						{
							m_core->pcstk = m_core->pcstack[m_core->pcstkp - 1];

							m_core->stky &= ~PCEM;
							if (m_core->pcstkp >= 30)
								m_core->stky |= PCFL;
						}
						else if (prev > m_core->pcstkp)
						{
							if (m_core->pcstkp > 0)
							{
								m_core->pcstk = m_core->pcstack[m_core->pcstkp - 1];
							}
							else
							{
								m_core->pcstk = 0x00ffffff;
								m_core->stky |= PCEM;
							}
							m_core->stky &= ~PCFL;
						}
					}
					break;

				case 0x7:                                     /* CURLCNTR */
					if ((m_core->lstkp > 0) && (m_core->lstkp < 7))
						m_core->curlcntr = data;
					break;

				case 0x8:                                     /* LCNTR */
					if (m_core->lstkp < 6)
						m_core->lcntr = data;
					break;

				default:
					sharc_error("SHARC: SET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc);
					break;
			}
			break;

		case 0x7:       /* system regs */
			switch (reg)
			{
				case 0x0:   m_core->ustat1 = data; break;     /* USTAT1 */
				case 0x1:   m_core->ustat2 = data; break;     /* USTAT2 */

				case 0x9:   m_core->irptl = data; break;      /* IRPTL */
				case 0xa:                                     /* MODE2 */
				{
					UINT32 set = ~m_core->mode2 & data;
					unsigned i;
					m_core->mode2 = data;
					for (i = 0; i < 4; i++)
					{
						if (BIT(set, 15 + i))
							sharc_flag_out(i, BIT(m_core->astat, FLG0_SHIFT + i));
					}
					break;
				}

				case 0xb:                                     /* MODE1 */
					add_systemreg_write_latency_effect(reg, data, m_core->mode1);
					m_core->mode1 = data;
					break;

				case 0xc:                                     /* ASTAT */
				{
					UINT32 flags = (m_core->astat ^ data) & (BITS(m_core->mode2, 15, 4) << FLG0_SHIFT);
					unsigned i;
					m_core->astat = data;
					for (i = 0; i < 4; i++)
					{
						if (BIT(flags, FLG0_SHIFT + i))
							sharc_flag_out(i, BIT(data, FLG0_SHIFT + i));
					}
					break;
				}

				case 0xd:                                     /* IMASK */
					check_interrupts();
					m_core->imask = data;
					break;

				case 0xe:                                     /* STKY */
					m_core->stky = (m_core->stky & (LSEM | LSOV | SSEM | SSOV | PCEM | PCFL)) | (data & ~(LSEM | LSOV | SSEM | SSOV | PCEM | PCFL));
					break;

				default:
					sharc_error("SHARC: SET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc);
					break;
			}
			break;

		case 0xd:
			switch (reg)
			{
				case 0xc:   m_core->px &= SHARC_U64(0xffffffff, 0xffff0000); m_core->px |= (data & 0xffff); break;        /* PX1 */
				case 0xd:   m_core->px &= 0xffff; m_core->px |= (UINT64)data << 16; break;     /* PX2 */
				default:    sharc_error("SHARC: SET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc); break;
			}
			break;

		default:
			sharc_error("SHARC: SET_UREG: unknown register %08X at %08X\n", ureg, m_core->pc);
			break;
	}
}

/*****************************************************************************/
#define SET_FLAG_SV_LSHIFT(x, shift)    if((x) & ((UINT32)0xffffffff << shift)) m_core->astat |= SV
#define SET_FLAG_SV_RSHIFT(x, shift)    if((x) & ((UINT32)0xffffffff >> shift)) m_core->astat |= SV

#define SET_FLAG_SZ(x)                  if((x) == 0) m_core->astat |= SZ

static void SHIFT_OPERATION_IMM(int shiftop, int data, int rn, int rx)
{
	INT8 shift = (INT8)(data & 0xff);
	int bit = data & 0x3f;
	int len = (data >> 6) & 0x3f;

	m_core->astat &= ~(SZ|SV|SS);

	switch (shiftop)
	{
		case 0x00:      /* LSHIFT Rx BY <data8>*/
		{
			if (shift < 0)
			{
				REG(rn) = (shift > -32 ) ? (INT32)(UIREG(rx) >> -shift) : 0;
			}
			else
			{
				REG(rn) = (shift < 32) ? (INT32)(UIREG(rx) << shift) : 0;
				if (shift > 0)
				{
					m_core->astat |= SV;
				}
			}
			SET_FLAG_SZ(REG(rn));
			break;
		}

		case 0x01:      /* ASHIFT Rx BY <data8> */
		{
			if (shift < 0)
			{
				REG(rn) = (INT32)REG(rx) >> ((shift > -32) ? -shift : 31);
			}
			else
			{
				REG(rn) = (shift < 32) ? (INT32)(UIREG(rx) << shift) : 0;
				if (shift > 0)
				{
					m_core->astat |= SV;
				}
			}
			SET_FLAG_SZ(REG(rn));
			break;
		}

		case 0x02:      /* ROT Rx BY <data8> */
		{
			REG(rn) = (INT32)rotl32(UIREG(rx), shift);
			SET_FLAG_SZ(REG(rn));
			break;
		}

		case 0x08:      /* Rn = Rn OR LSHIFT Rx BY <data8> */
		{
			UINT32 r = 0;
			if (shift < 0)
			{
				r = (shift > -32 ) ? (UIREG(rx) >> -shift) : 0;
			}
			else
			{
				r = (shift < 32) ? (UIREG(rx) << shift) : 0;
				if (shift > 0)
				{
					m_core->astat |= SV;
				}
			}

			SET_FLAG_SZ(r);

			REG(rn) = (INT32)(UIREG(rn) | r);
			break;
		}

		case 0x10:      /* FEXT Rx BY <bit6>:<len6> */
		{
			if (len == 0 || bit >= 32)
				REG(rn) = 0;
			else
				REG(rn) = (INT32)((UIREG(rx) >> bit) & make_bitmask32(len < 32 ? len : 32));

			SET_FLAG_SZ(REG(rn));
			if (bit+len > 32)
			{
				m_core->astat |= SV;
			}
			break;
		}

		case 0x11:      /* Rn = Rn FDEP Rx BY <bit6>:<len6> */
		{
			if (len == 0 || bit >= 32)
				REG(rn) = 0;
			else
				REG(rn) = (INT32)((UIREG(rx) & make_bitmask32(len < 32 ? len : 32)) << bit);

			SET_FLAG_SZ(REG(rn));
			if (bit+len > 32)
			{
				m_core->astat |= SV;
			}
			break;
		}

		case 0x12:      /* FEXT Rx BY <bit6>:<len6> (Sign Extended) */
		{
			if (len == 0 || bit >= 32)
				REG(rn) = 0;
			else if (bit+len > 32)
				REG(rn) = (INT32)(UIREG(rx) >> bit);
			else
				REG(rn) = sext32((UINT32)(REG(rx) >> bit), len < 32 ? len : 32);

			SET_FLAG_SZ(REG(rn));
			if (bit+len > 32)
			{
				m_core->astat |= SV;
			}
			break;
		}

		case 0x13:      /* FDEP Rx BY Ry <bit6>:<len6> (Sign Extended) */
		{
			if (len == 0 || bit >= 32)
				REG(rn) = 0;
			else
				REG(rn) = (INT32)((UINT32)sext32(UIREG(rx), len < 32 ? len : 32) << bit);

			SET_FLAG_SZ(REG(rn));
			if (bit+len > 32)
			{
				m_core->astat |= SV;
			}
			break;
		}

		case 0x19:      /* Rn = Rn OR FDEP Rx BY <bit6>:<len6> */
		{
			if (len != 0 && bit < 32)
				REG(rn) = (INT32)(UIREG(rn) | ((UIREG(rx) & make_bitmask32(len < 32 ? len : 32)) << bit));

			SET_FLAG_SZ(REG(rn));
			if (bit+len > 32)
			{
				m_core->astat |= SV;
			}
			break;
		}

		case 0x30:      /* BSET Rx BY <data8> */
		{
			REG(rn) = REG(rx);
			if (data >= 0 && data < 32)
			{
				REG(rn) = (INT32)(UIREG(rn) | (1U << data));
			}
			else
			{
				m_core->astat |= SV;
			}
			SET_FLAG_SZ(REG(rn));
			break;
		}

		case 0x31:      /* BCLR Rx BY <data8> */
		{
			REG(rn) = REG(rx);
			if (data >= 0 && data < 32)
			{
				REG(rn) = (INT32)(UIREG(rn) & ~(1U << data));
			}
			else
			{
				m_core->astat |= SV;
			}
			SET_FLAG_SZ(REG(rn));
			break;
		}

		case 0x32:      /* BTGL Rx BY <data8> */
		{
			REG(rn) = REG(rx);
			if (data >= 0 && data < 32)
			{
				REG(rn) = (INT32)(UIREG(rn) ^ (1U << data));
			}
			else
			{
				m_core->astat |= SV;
			}
			SET_FLAG_SZ(REG(rn));
			break;
		}

		case 0x33:      /* BTST Rx BY <data8> */
		{
			if (data < 32)
			{
				UINT32 r = UIREG(rx) & (1U << data);

				SET_FLAG_SZ(r);
			}
			else
			{
				m_core->astat |= SZ | SV;
			}
			break;
		}

		default:
			sharc_error("SHARC: unimplemented shift operation %02X at %08X\n", shiftop, m_core->pc);
			break;
	}
}

#include "compute.c"

static void COMPUTE(UINT32 opcode)
{
	int op = (opcode >> 12) & 0xff;
	int cu = (opcode >> 20) & 0x3;
	int rn = (opcode >> 8) & 0xf;
	int rx = (opcode >> 4) & 0xf;
	int ry = (opcode >> 0) & 0xf;

	if (opcode & 0x400000)       /* Multi-function opcode */
	{
		int fm = (opcode >> 12) & 0xf;
		int fa = (opcode >> 8) & 0xf;
		int fxm = (opcode >> 6) & 0x3;          /* registers 0 - 3 */
		int fym = ((opcode >> 4) & 0x3) + 4;    /* registers 4 - 7 */
		int fxa = ((opcode >> 2) & 0x3) + 8;    /* registers 8 - 11 */
		int fya = (opcode & 0x3) + 12;          /* registers 12 - 15 */

		int multiop = (opcode >> 16) & 0x3f;
		switch (multiop)
		{
			case 0x00:      compute_multi_mr_to_reg(op & 0xf, rn); break;
			case 0x01:      compute_multi_reg_to_mr(op & 0xf, rn); break;

			case 0x04:      /* Rm = Rxm * Rym (SSFR),   Ra = Rxa + Rya */
				compute_mul_ssfr_add(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x05:      /* Rm = Rxm * Rym (SSFR),   Ra = Rxa - Rya */
				compute_mul_ssfr_sub(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x18:      /* Fm = Fxm * Fym,   Fa = Fxa + Fya */
				compute_fmul_fadd(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x19:      /* Fm = Fxm * Fym,   Fa = Fxa - Fya */
				compute_fmul_fsub(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x1a:      /* Fm = Fxm * Fym,   Fa = FLOAT Fxa BY Rya */
				compute_fmul_float_scaled(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x1b:      /* Fm = Fxm * Fym,   Ra = FIX Fxa BY Rya */
				compute_fmul_fix_scaled(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x1c:      /* Fm = Fxm * Fym,   Fa = (Fxa + Fya) / 2 */
				compute_fmul_favg(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x1d:      /* Fm = Fxm * Fym,   Fa = ABS Fxa */
				compute_fmul_fabs(fm, fxm, fym, fa, fxa);
				break;

			case 0x1e:      /* Fm = Fxm * Fym,   Fa = MAX(Fxa, Fya) */
				compute_fmul_fmax(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x1f:      /* Fm = Fxm * Fym,   Fa = MIN(Fxa, Fya) */
				compute_fmul_fmin(fm, fxm, fym, fa, fxa, fya);
				break;

			case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
			case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
			{
				/* Parallel Multiplier & Dual Add/Subtract */
				/* Floating-point */
				int fs = (opcode >> 16) & 0xf;
				compute_fmul_dual_fadd_fsub(fm, fxm, fym, fa, fs, fxa, fya);
				break;
			}

			default:
				sharc_error("SHARC: compute: multi-function opcode %02X not implemented ! (%08X, %08X)\n", multiop, m_core->pc, opcode);
				break;
		}
	}
	else                        /* Single-function opcode */
	{
		switch (cu)
		{
			/* ALU operations */
			case 0:
			{
				switch (op)
				{
					case 0x01:      compute_add(rn, rx, ry); break;
					case 0x02:      compute_sub(rn, rx, ry); break;
					case 0x05:      compute_add_ci(rn, rx, ry); break;
					case 0x06:      compute_sub_ci(rn, rx, ry); break;
					case 0x0a:      compute_comp(rx, ry); break;
					case 0x21:      compute_pass(rn, rx); break;
					case 0x22:      compute_neg(rn, rx); break;
					case 0x25:      compute_add_ci1(rn, rx); break;
					case 0x26:      compute_sub_ci1(rn, rx); break;
					case 0x29:      compute_inc(rn, rx); break;
					case 0x2a:      compute_dec(rn, rx); break;
					case 0x30:      compute_abs(rn, rx); break;
					case 0x40:      compute_and(rn, rx, ry); break;
					case 0x41:      compute_or(rn, rx, ry); break;
					case 0x42:      compute_xor(rn, rx, ry); break;
					case 0x43:      compute_not(rn, rx); break;
					case 0x61:      compute_min(rn, rx, ry); break;
					case 0x62:      compute_max(rn, rx, ry); break;
					case 0x63:      compute_clip(rn, rx, ry); break;
					case 0x81:      compute_fadd(rn, rx, ry); break;
					case 0x82:      compute_fsub(rn, rx, ry); break;
					case 0x89:      compute_favg(rn, rx, ry); break;
					case 0x8a:      compute_fcomp(rx, ry); break;
					case 0x91:      compute_fadd_abs(rn, rx, ry); break;
					case 0x92:      compute_fsub_abs(rn, rx, ry); break;
					case 0xa1:      compute_fpass(rn, rx); break;
					case 0xa2:      compute_fneg(rn, rx); break;
					case 0xb0:      compute_fabs(rn, rx); break;
					case 0xbd:      compute_scalb(rn, rx, ry); break;
					case 0xc1:      compute_logb(rn, rx); break;
					case 0xc4:      compute_recips(rn, rx); break;
					case 0xc5:      compute_rsqrts(rn, rx); break;
					case 0xc9:      compute_fix(rn, rx); break;
					case 0xca:      compute_float(rn, rx); break;
					case 0xd9:      compute_fix_scaled(rn, rx, ry); break;
					case 0xda:      compute_float_scaled(rn, rx, ry); break;
					case 0xe0:      compute_fcopysign(rn, rx, ry); break;
					case 0xe1:      compute_fmin(rn, rx, ry); break;
					case 0xe2:      compute_fmax(rn, rx, ry); break;
					case 0xe3:      compute_fclip(rn, rx, ry); break;

					case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
					case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
					{
						/* Fixed-point Dual Add/Subtract */
						int rs = op_get_rs(opcode);
						int ra = op_get_ra(opcode);
						compute_dual_add_sub(ra, rs, rx, ry);
						break;
					}

					case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
					case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
					{
						/* Floating-point Dual Add/Subtract */
						int rs = op_get_rs(opcode);
						int ra = op_get_ra(opcode);
						compute_dual_fadd_fsub(ra, rs, rx, ry);
						break;
					}

					default:
						sharc_error("SHARC: compute: unimplemented ALU operation %02X (%08X, %08X)\n", op, m_core->pc, opcode);
						break;
				}
				break;
			}


			/* Multiplier operations */
			case 1:
			{
				switch (op)
				{
					case 0x14:      m_core->mrf = 0; break;
					case 0x16:      m_core->mrb = 0; break;

					case 0x30:      compute_fmul(rn, rx, ry); break;
					case 0x40:      compute_mul_uuin(rn, rx, ry); break;
					case 0x70:      compute_mul_ssin(rn, rx, ry); break;

					case 0xb0:      REG(rn) = (INT32)compute_mrf_plus_mul_ssin(rx, ry); break;
					case 0xb2:      REG(rn) = (INT32)compute_mrb_plus_mul_ssin(rx, ry); break;

					default:
						sharc_error("SHARC: compute: multiplier operation %02X not implemented ! (%08X, %08X)\n", op, m_core->pc, opcode);
						break;
				}
				break;
			}


			/* Shifter operations */
			case 2:
			{
				m_core->astat &= ~(SZ|SV|SS);

				switch (op >> 2)
				{
					case 0x00:      /* LSHIFT Rx BY Ry*/
					{
						int shift = REG(ry);
						if (shift < 0)
						{
							REG(rn) = (shift > -32 ) ? (INT32)(UIREG(rx) >> -shift) : 0;
						}
						else
						{
							REG(rn) = (shift < 32) ? (INT32)(UIREG(rx) << shift) : 0;
							if (shift > 0)
							{
								m_core->astat |= SV;
							}
						}
						SET_FLAG_SZ(REG(rn));
						break;
					}

					case 0x02:      /* ROT Rx BY Ry */
					{
						int shift = REG(ry);
						REG(rn) = (INT32)rotl32(UIREG(rx), shift);
						if (shift > 0)
						{
							m_core->astat |= SV;
						}
						SET_FLAG_SZ(REG(rn));
						break;
					}

					case 0x08:      /* Rn = Rn OR LSHIFT Rx BY Ry*/
					{
						INT8 shift = (INT8)REG(ry);
						if (shift < 0)
						{
							/* signed (arithmetic) shift, as MAME */
							REG(rn) = REG(rn) | ((shift > -32 ) ? (REG(rx) >> -shift) : 0);
						}
						else
						{
							REG(rn) = (INT32)(UIREG(rn) | ((shift < 32) ? (UIREG(rx) << shift) : 0));
							if (shift > 0)
							{
								m_core->astat |= SV;
							}
						}
						SET_FLAG_SZ(REG(rn));
						break;
					}

					case 0x10:      /* FEXT Rx BY Ry */
					{
						int bit = REG(ry) & 0x3f;
						int len = (REG(ry) >> 6) & 0x3f;
						if (len == 0 || bit >= 32)
							REG(rn) = 0;
						else
							REG(rn) = (INT32)((UIREG(rx) >> bit) & make_bitmask32(len < 32 ? len : 32));

						SET_FLAG_SZ(REG(rn));
						if (bit+len > 32)
						{
							m_core->astat |= SV;
						}
						break;
					}

					case 0x12:      /* FEXT Rx BY Ry (Sign Extended) */
					{
						int bit = REG(ry) & 0x3f;
						int len = (REG(ry) >> 6) & 0x3f;
						if (len == 0 || bit >= 32)
							REG(rn) = 0;
						else if (bit+len > 32)
							REG(rn) = (INT32)(UIREG(rx) >> bit);
						else
							REG(rn) = sext32((UINT32)(REG(rx) >> bit), len < 32 ? len : 32);

						SET_FLAG_SZ(REG(rn));
						if (bit+len > 32)
						{
							m_core->astat |= SV;
						}
						break;
					}

					case 0x19:      /* Rn = Rn OR FDEP Rx BY Ry */
					{
						int bit = REG(ry) & 0x3f;
						int len = (REG(ry) >> 6) & 0x3f;

						if (len != 0 && bit < 32)
							REG(rn) = (INT32)(UIREG(rn) | ((UIREG(rx) & make_bitmask32(len < 32 ? len : 32)) << bit));

						SET_FLAG_SZ(REG(rn));
						if (bit+len > 32)
						{
							m_core->astat |= SV;
						}
						break;
					}

					case 0x30:      /* BSET Rx BY Ry */
					{
						UINT32 shift = UIREG(ry);
						REG(rn) = REG(rx);
						if (shift < 32)
						{
							REG(rn) = (INT32)(UIREG(rn) | (1U << shift));
						}
						else
						{
							m_core->astat |= SV;
						}
						SET_FLAG_SZ(REG(rn));
						break;
					}

					case 0x31:      /* BCLR Rx BY Ry */
					{
						UINT32 shift = UIREG(ry);
						REG(rn) = REG(rx);
						if (shift < 32)
						{
							REG(rn) = (INT32)(UIREG(rn) & ~(1U << shift));
						}
						else
						{
							m_core->astat |= SV;
						}
						SET_FLAG_SZ(REG(rn));
						break;
					}

					case 0x33:      /* BTST Rx BY Ry */
					{
						UINT32 shift = UIREG(ry);
						if (shift < 32)
						{
							UINT32 r = UIREG(rx) & (1U << shift);

							SET_FLAG_SZ(r);
						}
						else
						{
							m_core->astat |= SZ | SV;
						}
						break;
					}

					default:
						sharc_error("SHARC: compute: shift operation %02X not implemented ! (%08X, %08X)\n", op >> 2, m_core->pc, opcode);
						break;
				}
				break;
			}

			default:
				sharc_error("SHARC: compute: invalid single-function operation %02X\n", cu);
				break;
		}
	}
}

static void PUSH_PC(void)
{
	if (m_core->pcstkp >= 30)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: PC Stack overflow!\n");
		return;
	}

	if (m_core->pcstkp > 0)
		m_core->pcstack[m_core->pcstkp - 1] = m_core->pcstk;

	m_core->pcstkp++;

	m_core->stky &= ~PCEM;
	if (m_core->pcstkp >= 30)
		m_core->stky |= PCFL;
}

static UINT32 POP_PC(void)
{
	UINT32 result;

	if (m_core->pcstkp == 0)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: PC Stack underflow!\n");
		return m_core->pcstk;
	}

	result = m_core->pcstk;

	m_core->pcstkp--;

	if (m_core->pcstkp < 30)
	{
		m_core->pcstack[m_core->pcstkp] = m_core->pcstk;
		if (m_core->pcstkp > 0)
		{
			m_core->pcstk = m_core->pcstack[m_core->pcstkp - 1];
		}
		else
		{
			m_core->pcstk = 0x00ffffff;
			m_core->stky |= PCEM;
		}
		m_core->stky &= ~PCFL;
	}

	return result;
}

#define TOP_PC()    (m_core->pcstk)

static void PUSH_LOOP(void)
{
	if (m_core->lstkp >= 6)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: Loop Stack overflow!\n");
		return;
	}

	if (m_core->lstkp > 0)
	{
		m_core->lcstack[m_core->lstkp - 1] = m_core->curlcntr;
		m_core->lastack[m_core->lstkp - 1] = laddr_pack(&m_core->laddr);
	}
	m_core->curlcntr = m_core->lcntr;
	laddr_unpack(&m_core->laddr, m_core->lastack[m_core->lstkp]);

	m_core->lstkp++;

	if (m_core->lstkp < 6)
		m_core->lcntr = m_core->lcstack[m_core->lstkp];
	else
		m_core->lcntr = 0xffffffff;

	m_core->stky &= ~LSEM;
}

static void POP_LOOP(void)
{
	if (m_core->lstkp == 0)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: Loop Stack underflow!\n");
		return;
	}

	m_core->lstkp--;

	m_core->lcntr = m_core->curlcntr;
	m_core->lastack[m_core->lstkp] = laddr_pack(&m_core->laddr);

	if (m_core->lstkp > 0)
	{
		m_core->curlcntr = m_core->lcstack[m_core->lstkp - 1];
		laddr_unpack(&m_core->laddr, m_core->lastack[m_core->lstkp - 1]);
	}
	else
	{
		m_core->curlcntr = 0xffffffff;
		laddr_unpack(&m_core->laddr, 0xffffffff);

		m_core->stky |= LSEM;
	}
}

static void PUSH_STATUS_STACK(void)
{
	m_core->status_stkp++;
	if (m_core->status_stkp >= 5)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: Status stack overflow!\n");
		m_core->status_stkp--;
		return;
	}

	m_core->status_stack[m_core->status_stkp - 1].mode1 = GET_UREG(UREG_MODE1);
	m_core->status_stack[m_core->status_stkp - 1].astat = GET_UREG(UREG_ASTAT);

	m_core->stky &= ~SSEM;
}

static void POP_STATUS_STACK(void)
{
	UINT32 flags_mask = FLG0 | FLG1 | FLG2 | FLG3;

	if (m_core->status_stkp <= 0)
	{
		/* MAME: fatal error */
		sharc_error("SHARC: Status stack underflow!\n");
		return;
	}

	m_core->status_stkp--;

	SET_UREG(UREG_MODE1, m_core->status_stack[m_core->status_stkp].mode1);
	SET_UREG(UREG_ASTAT, (m_core->astat & flags_mask) | (m_core->status_stack[m_core->status_stkp].astat & ~flags_mask));

	if (m_core->status_stkp == 0)
		m_core->stky |= SSEM;
}

static int IF_CONDITION_CODE(int cond)
{
	switch (cond)
	{
		case 0x00:  return m_core->astat & AZ;        /* EQ */
		case 0x01:  return SHARC_COND_LT;             /* LT */
		case 0x02:  return SHARC_COND_LE;             /* LE */
		case 0x03:  return (m_core->astat & AC);      /* AC */
		case 0x04:  return (m_core->astat & AV);      /* AV */
		case 0x05:  return (m_core->astat & MV);      /* MV */
		case 0x06:  return (m_core->astat & MN);      /* MS */
		case 0x07:  return (m_core->astat & SV);      /* SV */
		case 0x08:  return (m_core->astat & SZ);      /* SZ */
		case 0x09:  return (m_core->flag[0] != 0);    /* FLAG0 */
		case 0x0a:  return (m_core->flag[1] != 0);    /* FLAG1 */
		case 0x0b:  return (m_core->flag[2] != 0);    /* FLAG2 */
		case 0x0c:  return (m_core->flag[3] != 0);    /* FLAG3 */
		case 0x0d:  return (m_core->astat & BTF);     /* TF */
		case 0x0e:  return 0;                         /* BM */
		case 0x0f:  return (m_core->curlcntr != 1);   /* NOT LCE */
		case 0x10:  return !(m_core->astat & AZ);     /* NOT EQUAL */
		case 0x11:  return !SHARC_COND_LT;            /* GE */
		case 0x12:  return !SHARC_COND_LE;            /* GT */
		case 0x13:  return !(m_core->astat & AC);     /* NOT AC */
		case 0x14:  return !(m_core->astat & AV);     /* NOT AV */
		case 0x15:  return !(m_core->astat & MV);     /* NOT MV */
		case 0x16:  return !(m_core->astat & MN);     /* NOT MS */
		case 0x17:  return !(m_core->astat & SV);     /* NOT SV */
		case 0x18:  return !(m_core->astat & SZ);     /* NOT SZ */
		case 0x19:  return (m_core->flag[0] == 0);    /* NOT FLAG0 */
		case 0x1a:  return (m_core->flag[1] == 0);    /* NOT FLAG1 */
		case 0x1b:  return (m_core->flag[2] == 0);    /* NOT FLAG2 */
		case 0x1c:  return (m_core->flag[3] == 0);    /* NOT FLAG3 */
		case 0x1d:  return !(m_core->astat & BTF);    /* NOT TF */
		case 0x1e:  return 1;                         /* NOT BM */
		case 0x1f:  return 1;                         /* TRUE */
	}
	return 1;
}

static int DO_CONDITION_CODE(int cond)
{
	switch (cond)
	{
		case 0x00:  return m_core->astat & AZ;        /* EQ */
		case 0x01:  return SHARC_COND_LT;             /* LT */
		case 0x02:  return SHARC_COND_LE;             /* LE */
		case 0x03:  return (m_core->astat & AC);      /* AC */
		case 0x04:  return (m_core->astat & AV);      /* AV */
		case 0x05:  return (m_core->astat & MV);      /* MV */
		case 0x06:  return (m_core->astat & MN);      /* MS */
		case 0x07:  return (m_core->astat & SV);      /* SV */
		case 0x08:  return (m_core->astat & SZ);      /* SZ */
		case 0x09:  return (m_core->flag[0] != 0);    /* FLAG0 */
		case 0x0a:  return (m_core->flag[1] != 0);    /* FLAG1 */
		case 0x0b:  return (m_core->flag[2] != 0);    /* FLAG2 */
		case 0x0c:  return (m_core->flag[3] != 0);    /* FLAG3 */
		case 0x0d:  return (m_core->astat & BTF);     /* TF */
		case 0x0e:  return 0;                         /* BM */
		case 0x0f:  return (m_core->curlcntr == 1);   /* LCE */
		case 0x10:  return !(m_core->astat & AZ);     /* NOT EQUAL */
		case 0x11:  return !SHARC_COND_LT;            /* GE */
		case 0x12:  return !SHARC_COND_LE;            /* GT */
		case 0x13:  return !(m_core->astat & AC);     /* NOT AC */
		case 0x14:  return !(m_core->astat & AV);     /* NOT AV */
		case 0x15:  return !(m_core->astat & MV);     /* NOT MV */
		case 0x16:  return !(m_core->astat & MN);     /* NOT MS */
		case 0x17:  return !(m_core->astat & SV);     /* NOT SV */
		case 0x18:  return !(m_core->astat & SZ);     /* NOT SZ */
		case 0x19:  return (m_core->flag[0] == 0);    /* NOT FLAG0 */
		case 0x1a:  return (m_core->flag[1] == 0);    /* NOT FLAG1 */
		case 0x1b:  return (m_core->flag[2] == 0);    /* NOT FLAG2 */
		case 0x1c:  return (m_core->flag[3] == 0);    /* NOT FLAG3 */
		case 0x1d:  return !(m_core->astat & BTF);    /* NOT TF */
		case 0x1e:  return 1;                         /* NOT BM */
		case 0x1f:  return 0;                         /* FALSE (FOREVER) */
	}
	return 1;
}

/*****************************************************************************/
/* | 001xxxxxx | */

/* compute / dreg <-> DM / dreg <-> PM */
static void sharcop_compute_dreg_dm_dreg_pm(void)
{
	int pm_dreg = (int)((m_core->opcode >> 23) & 0xf);
	int pmm = op_get_pmm(m_core->opcode);
	int pmi = op_get_pmi(m_core->opcode);
	int dm_dreg = (int)((m_core->opcode >> 33) & 0xf);
	int dmm = op_get_dmm(m_core->opcode);
	int dmi = op_get_dmi(m_core->opcode);
	int pmd = (int)((m_core->opcode >> 37) & 0x1);
	int dmd = (int)((m_core->opcode >> 44) & 0x1);
	UINT32 compute = op_get_compute(m_core->opcode);

	/* due to parallelity issues, source DREGs must be saved */
	/* because the compute operation may change them */
	UINT32 parallel_pm_dreg = UIREG(pm_dreg);
	UINT32 parallel_dm_dreg = UIREG(dm_dreg);

	if (dmd)
		DM_PRE_W(DM_REG_I(dmi), parallel_dm_dreg);
	else
		DM_PRE_R(DM_REG_I(dmi));

	if (compute)
		COMPUTE(compute);

	if (pmd)        /* dreg -> PM */
	{
		pm_write32(PM_REG_I(pmi), parallel_pm_dreg);
		PM_REG_I(pmi) += PM_REG_M(pmm);
		UPDATE_CIRCULAR_BUFFER_PM(pmi);
	}
	else            /* PM -> dreg */
	{
		REG(pm_dreg) = (INT32)pm_read32(PM_REG_I(pmi));
		PM_REG_I(pmi) += PM_REG_M(pmm);
		UPDATE_CIRCULAR_BUFFER_PM(pmi);
	}

	if (dmd)        /* dreg -> DM */
	{
		dm_write32(DM_REG_I(dmi), parallel_dm_dreg);
		DM_REG_I(dmi) += DM_REG_M(dmm);
		UPDATE_CIRCULAR_BUFFER_DM(dmi);
	}
	else            /* DM -> dreg */
	{
		REG(dm_dreg) = (INT32)dm_read32(DM_REG_I(dmi));
		DM_REG_I(dmi) += DM_REG_M(dmm);
		UPDATE_CIRCULAR_BUFFER_DM(dmi);
	}
}

/*****************************************************************************/
/* | 00000001x | */

/* compute */
static void sharcop_compute(void)
{
	int cond = op_get_cond(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (IF_CONDITION_CODE(cond) && compute)
		COMPUTE(compute);
}

/*****************************************************************************/
/* | 010xxxxxx | */

/* compute / ureg <-> DM|PM, pre-modify */
static void sharcop_compute_ureg_dmpm_premod(void)
{
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int m = (int)((m_core->opcode >> 38) & 0x7);
	int cond = op_get_cond(m_core->opcode);
	int g = (int)((m_core->opcode >> 32) & 0x1);
	int d = (int)((m_core->opcode >> 31) & 0x1);
	int ureg = (int)((m_core->opcode >> 23) & 0xff);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (IF_CONDITION_CODE(cond))
	{
		/* due to parallelity issues, source UREG must be saved */
		/* because the compute operation may change it */
		UINT32 parallel_ureg = GET_UREG(ureg);

		if (!g)
		{
			if (d)
				DM_PRE_W(DM_REG_I(i)+DM_REG_M(m), parallel_ureg);
			else
				DM_PRE_R(DM_REG_I(i)+DM_REG_M(m));
		}

		if (compute)
			COMPUTE(compute);

		if (g)      /* PM */
		{
			if (d)      /* ureg -> PM */
			{
				if (ureg == 0xdb)       /* PX register access is always 48-bit */
					pm_write48(PM_REG_I(i)+PM_REG_M(m), m_core->px);
				else
					pm_write32(PM_REG_I(i)+PM_REG_M(m), parallel_ureg);
			}
			else        /* PM <- ureg */
			{
				if (ureg == 0xdb)       /* PX register access is always 48-bit */
					m_core->px = pm_read48(PM_REG_I(i)+PM_REG_M(m));
				else
					SET_UREG(ureg, pm_read32(PM_REG_I(i)+PM_REG_M(m)));
			}
		}
		else    /* DM */
		{
			if (d)      /* ureg -> DM */
				dm_write32(DM_REG_I(i)+DM_REG_M(m), parallel_ureg);
			else        /* DM <- ureg */
				SET_UREG(ureg, dm_read32(DM_REG_I(i)+DM_REG_M(m)));
		}
	}
}

/* compute / ureg <-> DM|PM, post-modify */
static void sharcop_compute_ureg_dmpm_postmod(void)
{
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int m = (int)((m_core->opcode >> 38) & 0x7);
	int cond = op_get_cond(m_core->opcode);
	int g = (int)((m_core->opcode >> 32) & 0x1);
	int d = (int)((m_core->opcode >> 31) & 0x1);
	int ureg = (int)((m_core->opcode >> 23) & 0xff);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (IF_CONDITION_CODE(cond))
	{
		/* due to parallelity issues, source UREG must be saved */
		/* because the compute operation may change it */
		UINT32 parallel_ureg = GET_UREG(ureg);

		if (!g)
		{
			if (d)
				DM_PRE_W(DM_REG_I(i), parallel_ureg);
			else
				DM_PRE_R(DM_REG_I(i));
		}

		if (compute)
			COMPUTE(compute);

		if (g)      /* PM */
		{
			if (d)      /* ureg -> PM */
			{
				if (ureg == 0xdb)       /* PX register access is always 48-bit */
					pm_write48(PM_REG_I(i), m_core->px);
				else
					pm_write32(PM_REG_I(i), parallel_ureg);
				PM_REG_I(i) += PM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_PM(i);
			}
			else        /* PM <- ureg */
			{
				if (ureg == 0xdb)       /* PX register access is always 48-bit */
					m_core->px = pm_read48(PM_REG_I(i));
				else
					SET_UREG(ureg, pm_read32(PM_REG_I(i)));
				PM_REG_I(i) += PM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_PM(i);
			}
		}
		else    /* DM */
		{
			if (d)      /* ureg -> DM */
			{
				dm_write32(DM_REG_I(i), parallel_ureg);
				DM_REG_I(i) += DM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_DM(i);
			}
			else        /* DM <- ureg */
			{
				SET_UREG(ureg, dm_read32(DM_REG_I(i)));
				DM_REG_I(i) += DM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_DM(i);
			}
		}
	}
}

/*****************************************************************************/
/* | 0110xxxxx | */

/* compute / dreg <- DM, immediate modify */
static void sharcop_compute_dm_to_dreg_immmod(void)
{
	int cond = op_get_cond(m_core->opcode);
	int u = (int)((m_core->opcode >> 38) & 0x1);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int mod = op_get_reladdr(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (IF_CONDITION_CODE(cond))
	{
		if (u)
			DM_PRE_R(DM_REG_I(i));
		else
			DM_PRE_R(DM_REG_I(i) + mod);

		if (compute)
		{
			COMPUTE(compute);
		}

		if (u)      /* post-modify with update */
		{
			REG(dreg) = (INT32)dm_read32(DM_REG_I(i));
			DM_REG_I(i) += mod;
			UPDATE_CIRCULAR_BUFFER_DM(i);
		}
		else        /* pre-modify, no update */
		{
			REG(dreg) = (INT32)dm_read32(DM_REG_I(i) + mod);
		}
	}
}

/* compute / dreg -> DM, immediate modify */
static void sharcop_compute_dreg_to_dm_immmod(void)
{
	int cond = op_get_cond(m_core->opcode);
	int u = (int)((m_core->opcode >> 38) & 0x1);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int mod = op_get_reladdr(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	/* due to parallelity issues, source REG must be saved */
	/* because the shift operation may change it */
	UINT32 parallel_dreg = UIREG(dreg);

	if (IF_CONDITION_CODE(cond))
	{
		if (u)
			DM_PRE_W(DM_REG_I(i), parallel_dreg);
		else
			DM_PRE_W(DM_REG_I(i) + mod, parallel_dreg);

		if (compute)
			COMPUTE(compute);

		if (u)      /* post-modify with update */
		{
			dm_write32(DM_REG_I(i), parallel_dreg);
			DM_REG_I(i) += mod;
			UPDATE_CIRCULAR_BUFFER_DM(i);
		}
		else        /* pre-modify, no update */
		{
			dm_write32(DM_REG_I(i) + mod, parallel_dreg);
		}
	}
}

/* compute / dreg <- PM, immediate modify */
static void sharcop_compute_pm_to_dreg_immmod(void)
{
	int cond = op_get_cond(m_core->opcode);
	int u = (int)((m_core->opcode >> 38) & 0x1);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int mod = op_get_reladdr(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (IF_CONDITION_CODE(cond))
	{
		if (compute)
			COMPUTE(compute);

		if (u)      /* post-modify with update */
		{
			REG(dreg) = (INT32)pm_read32(PM_REG_I(i));
			PM_REG_I(i) += mod;
			UPDATE_CIRCULAR_BUFFER_PM(i);
		}
		else        /* pre-modify, no update */
		{
			REG(dreg) = (INT32)pm_read32(PM_REG_I(i) + mod);
		}
	}
}

/* compute / dreg -> PM, immediate modify */
static void sharcop_compute_dreg_to_pm_immmod(void)
{
	int cond = op_get_cond(m_core->opcode);
	int u = (int)((m_core->opcode >> 38) & 0x1);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int mod = op_get_reladdr(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	/* due to parallelity issues, source REG must be saved */
	/* because the compute operation may change it */
	UINT32 parallel_dreg = UIREG(dreg);

	if (IF_CONDITION_CODE(cond))
	{
		if (compute)
			COMPUTE(compute);

		if (u)      /* post-modify with update */
		{
			pm_write32(PM_REG_I(i), parallel_dreg);
			PM_REG_I(i) += mod;
			UPDATE_CIRCULAR_BUFFER_PM(i);
		}
		else        /* pre-modify, no update */
		{
			pm_write32(PM_REG_I(i) + mod, parallel_dreg);
		}
	}
}

/*****************************************************************************/
/* | 0111xxxxx | */

/* compute / ureg <-> ureg */
static void sharcop_compute_ureg_to_ureg(void)
{
	int src_ureg = op_get_ureg_src(m_core->opcode);
	int dst_ureg = op_get_ureg_dst(m_core->opcode);
	int cond = op_get_cond_ureg(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (IF_CONDITION_CODE(cond))
	{
		/* due to parallelity issues, source UREG must be saved */
		/* because the compute operation may change it */
		UINT32 parallel_ureg = GET_UREG(src_ureg);

		if (compute)
			COMPUTE(compute);

		SET_UREG(dst_ureg, parallel_ureg);
	}
}

/*****************************************************************************/
/* | 1000xxxxx | */

/* immediate shift / dreg <-> DM|PM */
static void sharcop_imm_shift_dreg_dmpm(void)
{
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int m = (int)((m_core->opcode >> 38) & 0x7);
	int g = (int)((m_core->opcode >> 32) & 0x1);
	int d = (int)((m_core->opcode >> 31) & 0x1);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);
	int cond = op_get_cond(m_core->opcode);
	int data = (int)(((m_core->opcode >> 8) & 0xff) | ((m_core->opcode >> 19) & 0xf00));
	int shiftop = (int)((m_core->opcode >> 16) & 0x3f);
	int rn = (int)((m_core->opcode >> 4) & 0xf);
	int rx = (int)(m_core->opcode & 0xf);

	if (IF_CONDITION_CODE(cond))
	{
		/* due to parallelity issues, source REG must be saved */
		/* because the shift operation may change it */
		UINT32 parallel_dreg = UIREG(dreg);

		if (!g)
		{
			if (d)
				DM_PRE_W(DM_REG_I(i), parallel_dreg);
			else
				DM_PRE_R(DM_REG_I(i));
		}

		SHIFT_OPERATION_IMM(shiftop, data, rn, rx);

		if (g)      /* PM */
		{
			if (d)      /* dreg -> PM */
			{
				pm_write32(PM_REG_I(i), parallel_dreg);
				PM_REG_I(i) += PM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_PM(i);
			}
			else        /* PM <- dreg */
			{
				REG(dreg) = (INT32)pm_read32(PM_REG_I(i));
				PM_REG_I(i) += PM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_PM(i);
			}
		}
		else        /* DM */
		{
			if (d)      /* dreg -> DM */
			{
				dm_write32(DM_REG_I(i), parallel_dreg);
				DM_REG_I(i) += DM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_DM(i);
			}
			else    /* DM <- dreg */
			{
				REG(dreg) = (INT32)dm_read32(DM_REG_I(i));
				DM_REG_I(i) += DM_REG_M(m);
				UPDATE_CIRCULAR_BUFFER_DM(i);
			}
		}
	}
}

/*****************************************************************************/
/* | 00000010x | */

/* immediate shift */
static void sharcop_imm_shift(void)
{
	int cond = op_get_cond(m_core->opcode);
	int data = (int)(((m_core->opcode >> 8) & 0xff) | ((m_core->opcode >> 19) & 0xf00));
	int shiftop = (int)((m_core->opcode >> 16) & 0x3f);
	int rn = (int)((m_core->opcode >> 4) & 0xf);
	int rx = (int)(m_core->opcode & 0xf);

	if (IF_CONDITION_CODE(cond))
		SHIFT_OPERATION_IMM(shiftop, data, rn, rx);
}

/*****************************************************************************/
/* | 00000100x | */

/* compute / modify */
static void sharcop_compute_modify(void)
{
	int cond = op_get_cond(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);
	int g = (int)((m_core->opcode >> 38) & 0x1);
	int m = (int)((m_core->opcode >> 27) & 0x7);
	int i = (int)((m_core->opcode >> 30) & 0x7);

	if (IF_CONDITION_CODE(cond))
	{
		if (compute)
			COMPUTE(compute);

		if (g)      /* Modify PM */
		{
			PM_REG_I(i) += PM_REG_M(m);
			UPDATE_CIRCULAR_BUFFER_PM(i);
		}
		else        /* Modify DM */
		{
			DM_REG_I(i) += DM_REG_M(m);
			UPDATE_CIRCULAR_BUFFER_DM(i);
		}
	}
}

/*****************************************************************************/
/* | 00000110x | */

/* direct call to absolute address */
static void sharcop_direct_call(void)
{
	int j = op_get_jump_j(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 address = (UINT32)(m_core->opcode & 0xffffff);

	if (IF_CONDITION_CODE(cond))
	{
		PUSH_PC();
		if (j)
		{
			m_core->pcstk = m_core->nfaddr;     /* 1 instruction + 2 delayed instructions */
			CHANGE_PC_DELAYED(address);
		}
		else
		{
			m_core->pcstk = m_core->daddr;
			CHANGE_PC(address);
		}
	}
}

/* clear interrupt (CI bit of jumps) */
static void sharc_clear_interrupt(void)
{
	/* TODO: timer and VIRPT interrupts also push the status stack */
	if (m_core->active_irq_num >= 6 && m_core->active_irq_num <= 8)
		POP_STATUS_STACK();

	m_core->interrupt_active = 0;
	m_core->irptl &= ~(1U << m_core->active_irq_num);
}

/* direct jump to absolute address */
static void sharcop_direct_jump(void)
{
	int la = op_get_jump_la(m_core->opcode);
	int ci = op_get_jump_ci(m_core->opcode);
	int j = op_get_jump_j(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 address = (UINT32)(m_core->opcode & 0xffffff);

	if (IF_CONDITION_CODE(cond))
	{
		/* Clear Interrupt */
		if (ci)
			sharc_clear_interrupt();

		if (la)
		{
			POP_PC();
			POP_LOOP();
		}

		if (j)
			CHANGE_PC_DELAYED(address);
		else
		{
			/* port: IF flag JUMP to itself waits on the outside */
			if (!la && !ci && address == m_core->pc && sharc_cond_is_flag(cond))
				sharc_flag_spin = 1;
			CHANGE_PC(address);
		}
	}
}

/*****************************************************************************/
/* | 00000111x | */

/* direct call to relative address */
static void sharcop_relative_call(void)
{
	int j = op_get_jump_j(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 address = (UINT32)(m_core->opcode & 0xffffff);

	if (IF_CONDITION_CODE(cond))
	{
		PUSH_PC();
		if (j)
		{
			m_core->pcstk = m_core->pc + 3;  /* 1 instruction + 2 delayed instructions */
			CHANGE_PC_DELAYED(m_core->pc + sext32(address, 24));
		}
		else
		{
			m_core->pcstk = m_core->pc + 1;
			CHANGE_PC(m_core->pc + sext32(address, 24));
		}
	}
}

/* direct jump to relative address */
static void sharcop_relative_jump(void)
{
	int la = op_get_jump_la(m_core->opcode);
	int ci = op_get_jump_ci(m_core->opcode);
	int j = op_get_jump_j(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 address = (UINT32)(m_core->opcode & 0xffffff);

	if (IF_CONDITION_CODE(cond))
	{
		/* Clear Interrupt */
		if (ci)
			sharc_clear_interrupt();

		if (la)
		{
			POP_PC();
			POP_LOOP();
		}

		if (j)
			CHANGE_PC_DELAYED(m_core->pc + sext32(address, 24));
		else
		{
			/* port: IF flag JUMP (PC, 0) waits on the outside */
			if (!la && !ci && address == 0 && sharc_cond_is_flag(cond))
				sharc_flag_spin = 1;
			CHANGE_PC(m_core->pc + sext32(address, 24));
		}
	}
}

/*****************************************************************************/
/* | 00001000x | */

/* indirect jump */
static void sharcop_indirect_jump(void)
{
	int la = op_get_jump_la(m_core->opcode);
	int ci = op_get_jump_ci(m_core->opcode);
	int j = op_get_jump_j(m_core->opcode);
	int e = op_get_jump_e(m_core->opcode);
	int pmi = op_get_pmi(m_core->opcode);
	int pmm = op_get_pmm(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	/* Clear Interrupt */
	if (ci)
		sharc_clear_interrupt();

	if (e)      /* IF...ELSE */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (la)
			{
				POP_PC();
				POP_LOOP();
			}

			if (j)
				CHANGE_PC_DELAYED(PM_REG_I(pmi) + PM_REG_M(pmm));
			else
				CHANGE_PC(PM_REG_I(pmi) + PM_REG_M(pmm));
		}
		else
		{
			if (compute)
				COMPUTE(compute);
		}
	}
	else        /* IF */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (compute)
				COMPUTE(compute);

			if (la)
			{
				POP_PC();
				POP_LOOP();
			}

			if (j)
				CHANGE_PC_DELAYED(PM_REG_I(pmi) + PM_REG_M(pmm));
			else
				CHANGE_PC(PM_REG_I(pmi) + PM_REG_M(pmm));
		}
	}
}

/* indirect call */
static void sharcop_indirect_call(void)
{
	int j = op_get_jump_j(m_core->opcode);
	int e = op_get_jump_e(m_core->opcode);
	int pmi = op_get_pmi(m_core->opcode);
	int pmm = op_get_pmm(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (e)      /* IF...ELSE */
	{
		if (IF_CONDITION_CODE(cond))
		{
			PUSH_PC();
			if (j)
			{
				m_core->pcstk = m_core->nfaddr;     /* 1 instruction + 2 delayed instructions */
				CHANGE_PC_DELAYED(PM_REG_I(pmi) + PM_REG_M(pmm));
			}
			else
			{
				m_core->pcstk = m_core->daddr;
				CHANGE_PC(PM_REG_I(pmi) + PM_REG_M(pmm));
			}
		}
		else
		{
			if (compute)
				COMPUTE(compute);
		}
	}
	else        /* IF */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (compute)
				COMPUTE(compute);

			PUSH_PC();
			if (j)
			{
				m_core->pcstk = m_core->nfaddr;     /* 1 instruction + 2 delayed instructions */
				CHANGE_PC_DELAYED(PM_REG_I(pmi) + PM_REG_M(pmm));
			}
			else
			{
				m_core->pcstk = m_core->daddr;
				CHANGE_PC(PM_REG_I(pmi) + PM_REG_M(pmm));
			}
		}
	}
}

/*****************************************************************************/
/* | 00001001x | */

/* indirect jump to relative address */
static void sharcop_relative_jump_compute(void)
{
	int la = op_get_jump_la(m_core->opcode);
	int ci = op_get_jump_ci(m_core->opcode);
	int j = op_get_jump_j(m_core->opcode);
	int e = op_get_jump_e(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	/* Clear Interrupt */
	if (ci)
		sharc_clear_interrupt();

	if (e)      /* IF...ELSE */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (la)
			{
				POP_PC();
				POP_LOOP();
			}

			if (j)
				CHANGE_PC_DELAYED(m_core->pc + op_get_reladdr(m_core->opcode));
			else
				CHANGE_PC(m_core->pc + op_get_reladdr(m_core->opcode));
		}
		else
		{
			if (compute)
				COMPUTE(compute);
		}
	}
	else        /* IF */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (compute)
				COMPUTE(compute);

			if (la)
			{
				POP_PC();
				POP_LOOP();
			}

			if (j)
				CHANGE_PC_DELAYED(m_core->pc + op_get_reladdr(m_core->opcode));
			else
				CHANGE_PC(m_core->pc + op_get_reladdr(m_core->opcode));
		}
	}
}

/* indirect call to relative address */
static void sharcop_relative_call_compute(void)
{
	int j = op_get_jump_j(m_core->opcode);
	int e = op_get_jump_e(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (e)      /* IF...ELSE */
	{
		if (IF_CONDITION_CODE(cond))
		{
			PUSH_PC();
			if (j)
			{
				m_core->pcstk = m_core->nfaddr;     /* 1 instruction + 2 delayed instructions */
				CHANGE_PC_DELAYED(m_core->pc + op_get_reladdr(m_core->opcode));
			}
			else
			{
				m_core->pcstk = m_core->daddr;
				CHANGE_PC(m_core->pc + op_get_reladdr(m_core->opcode));
			}
		}
		else
		{
			if (compute)
				COMPUTE(compute);
		}
	}
	else        /* IF */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (compute)
				COMPUTE(compute);

			PUSH_PC();
			if (j)
			{
				m_core->pcstk = m_core->nfaddr;     /* 1 instruction + 2 delayed instructions */
				CHANGE_PC_DELAYED(m_core->pc + op_get_reladdr(m_core->opcode));
			}
			else
			{
				m_core->pcstk = m_core->daddr;
				CHANGE_PC(m_core->pc + op_get_reladdr(m_core->opcode));
			}
		}
	}
}

/*****************************************************************************/
/* | 110xxxxxx | */

/* indirect jump / compute / dreg <-> DM */
static void sharcop_indirect_jump_compute_dreg_dm(void)
{
	int d = (int)((m_core->opcode >> 44) & 0x1);
	int dmi = op_get_dmi(m_core->opcode);
	int dmm = op_get_dmm(m_core->opcode);
	int pmi = op_get_pmi(m_core->opcode);
	int pmm = op_get_pmm(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);

	if (IF_CONDITION_CODE(cond))
	{
		CHANGE_PC(PM_REG_I(pmi) + PM_REG_M(pmm));
	}
	else
	{
		UINT32 compute = op_get_compute(m_core->opcode);
		/* due to parallelity issues, source REG must be saved */
		/* because the compute operation may change it */
		UINT32 parallel_dreg = UIREG(dreg);

		if (d)
			DM_PRE_W(DM_REG_I(dmi), parallel_dreg);
		else
			DM_PRE_R(DM_REG_I(dmi));

		if (compute)
			COMPUTE(compute);

		if (d)      /* dreg -> DM */
		{
			dm_write32(DM_REG_I(dmi), parallel_dreg);
			DM_REG_I(dmi) += DM_REG_M(dmm);
			UPDATE_CIRCULAR_BUFFER_DM(dmi);
		}
		else        /* DM <- dreg */
		{
			REG(dreg) = (INT32)dm_read32(DM_REG_I(dmi));
			DM_REG_I(dmi) += DM_REG_M(dmm);
			UPDATE_CIRCULAR_BUFFER_DM(dmi);
		}
	}
}

/*****************************************************************************/
/* | 111xxxxxx | */

/* relative jump / compute / dreg <-> DM */
static void sharcop_relative_jump_compute_dreg_dm(void)
{
	int d = (int)((m_core->opcode >> 44) & 0x1);
	int dmi = op_get_dmi(m_core->opcode);
	int dmm = op_get_dmm(m_core->opcode);
	int cond = op_get_cond(m_core->opcode);
	int dreg = (int)((m_core->opcode >> 23) & 0xf);

	if (IF_CONDITION_CODE(cond))
	{
		CHANGE_PC(m_core->pc + op_get_reladdr(m_core->opcode));
	}
	else
	{
		UINT32 compute = op_get_compute(m_core->opcode);
		/* due to parallelity issues, source REG must be saved */
		/* because the compute operation may change it */
		UINT32 parallel_dreg = UIREG(dreg);

		if (d)
			DM_PRE_W(DM_REG_I(dmi), parallel_dreg);
		else
			DM_PRE_R(DM_REG_I(dmi));

		if (compute)
			COMPUTE(compute);

		if (d)      /* dreg -> DM */
		{
			dm_write32(DM_REG_I(dmi), parallel_dreg);
			DM_REG_I(dmi) += DM_REG_M(dmm);
			UPDATE_CIRCULAR_BUFFER_DM(dmi);
		}
		else        /* DM <- dreg */
		{
			REG(dreg) = (INT32)dm_read32(DM_REG_I(dmi));
			DM_REG_I(dmi) += DM_REG_M(dmm);
			UPDATE_CIRCULAR_BUFFER_DM(dmi);
		}
	}
}

/*****************************************************************************/
/* | 00001010x | */

/* return from subroutine / compute */
static void sharcop_rts(void)
{
	int cond = op_get_cond(m_core->opcode);
	int j = op_get_jump_j(m_core->opcode);
	int e = op_get_jump_e(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	if (e)      /* IF...ELSE */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (j)
				CHANGE_PC_DELAYED(POP_PC());
			else
				CHANGE_PC(POP_PC());
		}
		else
		{
			if (compute)
				COMPUTE(compute);
		}
	}
	else        /* IF */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (compute)
				COMPUTE(compute);

			if (j)
				CHANGE_PC_DELAYED(POP_PC());
			else
				CHANGE_PC(POP_PC());
		}
	}
}

/*****************************************************************************/
/* | 00001011x | */

/* return from interrupt / compute */
static void sharcop_rti(void)
{
	int cond = op_get_cond(m_core->opcode);
	int j = op_get_jump_j(m_core->opcode);
	int e = op_get_jump_e(m_core->opcode);
	UINT32 compute = op_get_compute(m_core->opcode);

	m_core->irptl &= ~(1U << m_core->active_irq_num);

	if (e)       /* IF...ELSE */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (j)
				CHANGE_PC_DELAYED(POP_PC());
			else
				CHANGE_PC(POP_PC());
		}
		else
		{
			if (compute)
				COMPUTE(compute);
		}
	}
	else        /* IF */
	{
		if (IF_CONDITION_CODE(cond))
		{
			if (compute)
				COMPUTE(compute);

			if (j)
				CHANGE_PC_DELAYED(POP_PC());
			else
				CHANGE_PC(POP_PC());
		}
	}

	/* TODO: timer and VIRPT interrupts also push the status stack */
	if (m_core->active_irq_num >= 6 && m_core->active_irq_num <= 8)
		POP_STATUS_STACK();

	m_core->interrupt_active = 0;
	check_interrupts();
}

/*****************************************************************************/
/* | 00001100x | */

static int loop_type_for_offset(int offset)
{
	int distance = (offset < 0) ? -offset : offset;

	if (distance == 1)
		return 1;
	else if (distance == 2)
		return 2;
	else
		return 3;
}

/* do until counter expired, LCNTR immediate */
static void sharcop_do_until_counter_imm(void)
{
	UINT16 data = (UINT16)(m_core->opcode >> 24);
	int offset = sext32((UINT32)(m_core->opcode & 0xffffff), 24);
	UINT32 address = m_core->pc + offset;
	int cond = 0xf;     /* until LCE (loop counter expired */
	int type = loop_type_for_offset(offset);

	m_core->lcntr = data;
	PUSH_PC();
	PUSH_LOOP();
	m_core->pcstk = m_core->pc + 1;
	m_core->laddr.addr = address;
	m_core->laddr.code = cond;
	m_core->laddr.loop_type = type;
}

/*****************************************************************************/
/* | 00001101x | */

/* do until counter expired, LCNTR from UREG */
static void sharcop_do_until_counter_ureg(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	int offset = sext32((UINT32)(m_core->opcode & 0xffffff), 24);
	UINT32 address = m_core->pc + offset;
	int cond = 0xf;     /* until LCE (loop counter expired */
	int type = loop_type_for_offset(offset);

	m_core->lcntr = GET_UREG(ureg);
	PUSH_PC();
	PUSH_LOOP();
	m_core->pcstk = m_core->pc + 1;
	m_core->laddr.addr = address;
	m_core->laddr.code = cond;
	m_core->laddr.loop_type = type;
}

/*****************************************************************************/
/* | 00001110x | */

/* do until */
static void sharcop_do_until(void)
{
	int cond = op_get_cond(m_core->opcode);
	int offset = sext32((UINT32)(m_core->opcode & 0xffffff), 24);
	UINT32 address = (m_core->pc + offset);

	PUSH_PC();
	PUSH_LOOP();
	m_core->pcstk = m_core->pc + 1;
	m_core->laddr.addr = address;
	m_core->laddr.code = cond;
	m_core->laddr.loop_type = 0;
}

/*****************************************************************************/
/* | 000100 | G | D | */

/* ureg <- DM (direct addressing) */
static void sharcop_dm_to_ureg_direct(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 address = (UINT32)(m_core->opcode);

	DM_PRE_R(address);
	SET_UREG(ureg, dm_read32(address));
}

/* ureg -> DM (direct addressing) */
static void sharcop_ureg_to_dm_direct(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 address = (UINT32)(m_core->opcode);

	DM_PRE_W(address, GET_UREG(ureg));
	dm_write32(address, GET_UREG(ureg));
}

/* ureg <- PM (direct addressing) */
static void sharcop_pm_to_ureg_direct(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 address = (UINT32)(m_core->opcode);

	if (ureg == 0xdb)       /* PX is 48-bit */
		m_core->px = pm_read48(address);
	else
		SET_UREG(ureg, pm_read32(address));
}

/* ureg -> PM (direct addressing) */
static void sharcop_ureg_to_pm_direct(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 address = (UINT32)(m_core->opcode);

	if (ureg == 0xdb)       /* PX is 48-bit */
		pm_write48(address, m_core->px);
	else
		pm_write32(address, GET_UREG(ureg));
}

/*****************************************************************************/
/* | 101 | G | III | D | */

/* ureg <- DM (indirect addressing) */
static void sharcop_dm_to_ureg_indirect(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 offset = (UINT32)(m_core->opcode);
	int i = (int)((m_core->opcode >> 41) & 0x7);

	DM_PRE_R(DM_REG_I(i) + offset);
	SET_UREG(ureg, dm_read32(DM_REG_I(i) + offset));
}

/* ureg -> DM (indirect addressing) */
static void sharcop_ureg_to_dm_indirect(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 offset = (UINT32)(m_core->opcode);
	int i = (int)((m_core->opcode >> 41) & 0x7);

	DM_PRE_W(DM_REG_I(i) + offset, GET_UREG(ureg));
	dm_write32(DM_REG_I(i) + offset, GET_UREG(ureg));
}

/* ureg <- PM (indirect addressing) */
static void sharcop_pm_to_ureg_indirect(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 offset = (UINT32)(m_core->opcode & 0xffffff);
	int i = (int)((m_core->opcode >> 41) & 0x7);

	if (ureg == 0xdb)       /* PX is 48-bit */
		m_core->px = pm_read48(PM_REG_I(i) + offset);
	else
		SET_UREG(ureg, pm_read32(PM_REG_I(i) + offset));
}

/* ureg -> PM (indirect addressing) */
static void sharcop_ureg_to_pm_indirect(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 offset = (UINT32)(m_core->opcode);
	int i = (int)((m_core->opcode >> 41) & 0x7);

	if (ureg == 0xdb)       /* PX is 48-bit */
		pm_write48(PM_REG_I(i) + offset, m_core->px);
	else
		pm_write32(PM_REG_I(i) + offset, GET_UREG(ureg));
}

/*****************************************************************************/
/* | 1001xxxxx | */

/* immediate data -> DM|PM */
static void sharcop_imm_to_dmpm(void)
{
	int i = (int)((m_core->opcode >> 41) & 0x7);
	int m = (int)((m_core->opcode >> 38) & 0x7);
	int g = (int)((m_core->opcode >> 37) & 0x1);
	UINT32 data = (UINT32)(m_core->opcode);

	if (g)
	{
		/* program memory (PM) */
		pm_write32(PM_REG_I(i), data);
		PM_REG_I(i) += PM_REG_M(m);
		UPDATE_CIRCULAR_BUFFER_PM(i);
	}
	else
	{
		/* data memory (DM) */
		DM_PRE_W(DM_REG_I(i), data);
		dm_write32(DM_REG_I(i), data);
		DM_REG_I(i) += DM_REG_M(m);
		UPDATE_CIRCULAR_BUFFER_DM(i);
	}
}

/*****************************************************************************/
/* | 00001111x | */

/* immediate data -> ureg */
static void sharcop_imm_to_ureg(void)
{
	int ureg = (int)((m_core->opcode >> 32) & 0xff);
	UINT32 data = (UINT32)(m_core->opcode);

	SET_UREG(ureg, data);
}

/*****************************************************************************/
/* | 00010100x | */

/* system register bit manipulation */
static void sharcop_sysreg_bitop(void)
{
	int bop = (int)((m_core->opcode >> 37) & 0x7);
	int sreg = (int)((m_core->opcode >> 32) & 0xf);
	UINT32 data = (UINT32)(m_core->opcode);

	UINT32 src = GET_UREG(0x70 | sreg);

	switch (bop)
	{
		case 0:     /* SET */
		{
			src |= data;
			break;
		}
		case 1:     /* CLEAR */
		{
			src &= ~data;
			break;
		}
		case 2:     /* TOGGLE */
		{
			src ^= data;
			break;
		}
		case 4:     /* TEST */
		{
			if ((src & data) == data)
				m_core->astat |= BTF;
			else
				m_core->astat &= ~BTF;
			break;
		}
		case 5:     /* XOR */
		{
			if (src == data)
				m_core->astat |= BTF;
			else
				m_core->astat &= ~BTF;
			break;
		}
		default:
			sharc_error("SHARC: sysreg_bitop: invalid bitop %d\n", bop);
			return;
	}

	SET_UREG(0x70 | sreg, src);
}

/*****************************************************************************/
/* | 000101100 | */

/* I register modify */
static void sharcop_modify(void)
{
	int g = (int)((m_core->opcode >> 38) & 0x1);
	int i = (int)((m_core->opcode >> 32) & 0x7);
	INT32 data = (INT32)(UINT32)(m_core->opcode);

	if (g)      /* PM */
	{
		PM_REG_I(i) += data;
		UPDATE_CIRCULAR_BUFFER_PM(i);
	}
	else        /* DM */
	{
		DM_REG_I(i) += data;
		UPDATE_CIRCULAR_BUFFER_DM(i);
	}
}

/*****************************************************************************/
/* | 000101101 | */

/* I register bit-reverse */
static void sharcop_bit_reverse(void)
{
	sharc_error("SHARC: sharcop_bit_reverse unimplemented\n");
}

/*****************************************************************************/
/* | 00010111x | */

/* push/pop stacks / flush cache */
static void sharcop_push_pop_stacks(void)
{
	if ((m_core->opcode >> 32) & 0x80)
	{
		PUSH_LOOP();
	}
	if ((m_core->opcode >> 32) & 0x40)
	{
		POP_LOOP();
	}
	if ((m_core->opcode >> 32) & 0x20)
	{
		PUSH_STATUS_STACK();
	}
	if ((m_core->opcode >> 32) & 0x10)
	{
		POP_STATUS_STACK();
	}
	if ((m_core->opcode >> 32) & 0x08)
	{
		/* TODO: what should the top-of-stack value be after this? */
		PUSH_PC();
	}
	if ((m_core->opcode >> 32) & 0x04)
	{
		POP_PC();
	}
}

/*****************************************************************************/
/* | 000000000 | */

static void sharcop_nop(void)
{
}

/*****************************************************************************/
/* | 000000001 | */

static void sharcop_idle(void)
{
	m_core->daddr = m_core->pc;
	m_core->faddr = m_core->pc+1;
	m_core->nfaddr = m_core->pc+2;

	m_core->idle = 1;
}

/*****************************************************************************/

static void sharcop_unimplemented(void)
{
	/* MAME: fatal error */
	sharc_error("SHARC: Unimplemented opcode %04X%08X at %08X\n",
			(UINT32)(m_core->opcode >> 32), (UINT32)m_core->opcode, m_core->pc);
}

/*****************************************************************************/
/* opcode table (sharcops_table.cpp) */

typedef struct
{
	UINT32 op_mask;
	UINT32 op_bits;
	void (*handler)(void);
} SHARC_OP;

static const SHARC_OP s_sharc_opcode_table[] =
{
	/*  |0 0 1| */
	{   0xe000,     0x2000,     sharcop_compute_dreg_dm_dreg_pm                     },

	/*  |0 0 0|0 0 0 0 1| */
	{   0xff00,     0x0100,     sharcop_compute                                     },

	/*  |0 1 0|0| */
	{   0xf000,     0x4000,     sharcop_compute_ureg_dmpm_premod                    },

	/*  |0 1 0|1| */
	{   0xf000,     0x5000,     sharcop_compute_ureg_dmpm_postmod                   },

	/*  |0 1 1|0|x x x|0|0| */
	{   0xf180,     0x6000,     sharcop_compute_dm_to_dreg_immmod                   },

	/*  |0 1 1|0|x x x|0|1| */
	{   0xf180,     0x6080,     sharcop_compute_dreg_to_dm_immmod                   },

	/*  |0 1 1|0|x x x|1|0| */
	{   0xf180,     0x6100,     sharcop_compute_pm_to_dreg_immmod                   },

	/*  |0 1 1|0|x x x|1|1| */
	{   0xf180,     0x6180,     sharcop_compute_dreg_to_pm_immmod                   },

	/*  |0 1 1|1| */
	{   0xf000,     0x7000,     sharcop_compute_ureg_to_ureg                        },

	/*  |1 0 0|0| */
	{   0xf000,     0x8000,     sharcop_imm_shift_dreg_dmpm                         },

	/*  |0 0 0|0 0 0 1 0| */
	{   0xff00,     0x0200,     sharcop_imm_shift                                   },

	/*  |0 0 0|0 0 1 0 0| */
	{   0xff00,     0x0400,     sharcop_compute_modify                              },

	/*  |0 0 0|0 0 1 1 0|0| */
	{   0xff80,     0x0600,     sharcop_direct_jump                                 },

	/*  |0 0 0|0 0 1 1 0|1| */
	{   0xff80,     0x0680,     sharcop_direct_call                                 },

	/*  |0 0 0|0 0 1 1 1|0| */
	{   0xff80,     0x0700,     sharcop_relative_jump                               },

	/*  |0 0 0|0 0 1 1 1|1| */
	{   0xff80,     0x0780,     sharcop_relative_call                               },

	/*  |0 0 0|0 1 0 0 0|0| */
	{   0xff80,     0x0800,     sharcop_indirect_jump                               },

	/*  |0 0 0|0 1 0 0 0|1| */
	{   0xff80,     0x0880,     sharcop_indirect_call                               },

	/*  |0 0 0|0 1 0 0 1|0| */
	{   0xff80,     0x0900,     sharcop_relative_jump_compute                       },

	/*  |0 0 0|0 1 0 0 1|1| */
	{   0xff80,     0x0980,     sharcop_relative_call_compute                       },

	/*  |1 1 0| */
	{   0xe000,     0xc000,     sharcop_indirect_jump_compute_dreg_dm               },

	/*  |1 1 1| */
	{   0xe000,     0xe000,     sharcop_relative_jump_compute_dreg_dm               },

	/*  |0 0 0|0 1 0 1 0| */
	{   0xff00,     0x0a00,     sharcop_rts                                         },

	/*  |0 0 0|0 1 0 1 1| */
	{   0xff00,     0x0b00,     sharcop_rti                                         },

	/*  |0 0 0|0 1 1 0 0| */
	{   0xff00,     0x0c00,     sharcop_do_until_counter_imm                        },

	/*  |0 0 0|0 1 1 0 1| */
	{   0xff00,     0x0d00,     sharcop_do_until_counter_ureg                       },

	/*  |0 0 0|0 1 1 1 0| */
	{   0xff00,     0x0e00,     sharcop_do_until                                    },

	/*  |0 0 0|1 0 0|0|0| */
	{   0xff00,     0x1000,     sharcop_dm_to_ureg_direct                           },

	/*  |0 0 0|1 0 0|0|1| */
	{   0xff00,     0x1100,     sharcop_ureg_to_dm_direct                           },

	/*  |0 0 0|1 0 0|1|0| */
	{   0xff00,     0x1200,     sharcop_pm_to_ureg_direct                           },

	/*  |0 0 0|1 0 0|1|1| */
	{   0xff00,     0x1300,     sharcop_ureg_to_pm_direct                           },

	/*  |1 0 1|0|x x x|0| */
	{   0xf100,     0xa000,     sharcop_dm_to_ureg_indirect                         },

	/*  |1 0 1|0|x x x|1| */
	{   0xf100,     0xa100,     sharcop_ureg_to_dm_indirect                         },

	/*  |1 0 1|1|x x x|0| */
	{   0xf100,     0xb000,     sharcop_pm_to_ureg_indirect                         },

	/*  |1 0 1|1|x x x|1| */
	{   0xf100,     0xb100,     sharcop_ureg_to_pm_indirect                         },

	/*  |1 0 0|1| */
	{   0xf000,     0x9000,     sharcop_imm_to_dmpm                                 },

	/*  |0 0 0|0 1 1 1 1| */
	{   0xff00,     0x0f00,     sharcop_imm_to_ureg                                 },

	/*  |0 0 0|1 0 1 0 0| */
	{   0xff00,     0x1400,     sharcop_sysreg_bitop                                },

	/*  |0 0 0|1 0 1 1 0|0| */
	{   0xff80,     0x1600,     sharcop_modify                                      },

	/*  |0 0 0|1 0 1 1 0|1| */
	{   0xff80,     0x1680,     sharcop_bit_reverse                                 },

	/*  |0 0 0|1 0 1 1 1| */
	{   0xff00,     0x1700,     sharcop_push_pop_stacks                             },

	/*  |0 0 0|0 0 0 0 0|0| */
	{   0xff80,     0x0000,     sharcop_nop                                         },

	/*  |0 0 0|0 0 0 0 0|1| */
	{   0xff80,     0x0080,     sharcop_idle                                        },
};
