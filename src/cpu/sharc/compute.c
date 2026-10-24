/***************************************************************************

    ADSP-21062 SHARC compute operations, from current MAME
    (src/devices/cpu/sharc/compute.hxx, copyright Ville Linde,
    BSD-3-Clause), translated to C for MAME 2003.
    Included from sharcops.c, not compiled on its own.

***************************************************************************/

#define CLEAR_ALU_FLAGS()           do { m_core->astat &= ~(AZ | AN | AV | AC | AS | AI); } while (0)

#define SET_FLAG_AZ(r)              do { m_core->astat |= ((r) == 0) ? AZ : 0; } while (0)
#define SET_FLAG_AN(r)              do { m_core->astat |= ((r) & 0x80000000) ? AN : 0; } while (0)
#define SET_FLAG_AC_ADD(r,a,b)      do { m_core->astat |= ((UINT32)(r) < (UINT32)(a)) ? AC : 0; } while (0)
#define SET_FLAG_AC_SUB(r,a,b)      do { m_core->astat |= ((UINT32)(r) <= (UINT32)(a)) ? AC : 0; } while (0)
#define SET_FLAG_AV_ADD(r,a,b)      do { if (~((UINT32)(a) ^ (UINT32)(b)) & ((UINT32)(a) ^ (UINT32)(r)) & 0x80000000) { m_core->astat |= AV; m_core->stky |= AOS; } } while (0)
#define SET_FLAG_AV_SUB(r,a,b)      do { if (((UINT32)(a) ^ (UINT32)(b)) & ((UINT32)(a) ^ (UINT32)(r)) & 0x80000000) { m_core->astat |= AV; m_core->stky |= AOS; } } while (0)

#define CLEAR_MULTIPLIER_FLAGS()    do { m_core->astat &= ~(MN | MV | MU | MI); } while (0)

#define SET_FLAG_MN(r)              do { m_core->astat |= (((r) & 0x80000000) ? MN : 0); } while (0)
#define SET_FLAG_MV(r)              do { m_core->astat |= ((((UINT32)((UINT64)(r) >> 32)) != 0) && (((UINT32)((UINT64)(r) >> 32)) != 0xffffffff)) ? MV : 0; } while (0)

/* TODO: MU needs 80-bit result */
#define SET_FLAG_MU(r)              do { m_core->astat |= ((((UINT32)((UINT64)(r) >> 32)) == 0) && ((UINT32)(r)) != 0) ? MU : 0; } while (0)

/* saturate overflowed result */
#define SATURATE(r)                 do { (r) = (UINT32)((((INT32)(r)) >> 31) ^ 0x80000000); } while (0)

#define IS_FLOAT_ZERO(r)                ((((UINT32)(r)) & (FLOAT_EXPONENT_MASK | FLOAT_MANTISSA_MASK)) == 0)
#define IS_FLOAT_DENORMAL(r)            (((((UINT32)(r)) & FLOAT_EXPONENT_MASK) == 0) && ((((UINT32)(r)) & FLOAT_MANTISSA_MASK) != 0))
#define IS_FLOAT_DENORMAL_OR_ZERO(r)    ((((UINT32)(r)) & FLOAT_EXPONENT_MASK) == 0)
#define IS_FLOAT_NAN(r)                 (((((UINT32)(r)) & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK) && ((((UINT32)(r)) & FLOAT_MANTISSA_MASK) != 0))
#define IS_FLOAT_INFINITY(r)            ((((UINT32)(r)) & (FLOAT_EXPONENT_MASK | FLOAT_MANTISSA_MASK)) == FLOAT_INFINITY)

static int IS_FLOAT_NAN_ADD(UINT32 a, UINT32 b)
{
	int aemax = (a & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	int bemax = (b & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	return (aemax && ((a & FLOAT_MANTISSA_MASK) != 0)) || (bemax && ((b & FLOAT_MANTISSA_MASK) != 0)) || (aemax && bemax && ((a ^ b) & FLOAT_SIGN_MASK));
}

static int IS_FLOAT_NAN_SUB(UINT32 a, UINT32 b)
{
	int aemax = (a & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	int bemax = (b & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	return (aemax && ((a & FLOAT_MANTISSA_MASK) != 0)) || (bemax && ((b & FLOAT_MANTISSA_MASK) != 0)) || (aemax && bemax && !((a ^ b) & FLOAT_SIGN_MASK));
}

static int IS_FLOAT_NAN_MUL(UINT32 a, UINT32 b)
{
	int aemax = (a & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	int bemax = (b & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	return (aemax && (((a & FLOAT_MANTISSA_MASK) != 0) || IS_FLOAT_ZERO(b))) || (bemax && (((b & FLOAT_MANTISSA_MASK) != 0) || IS_FLOAT_ZERO(a)));
}

#define FLOAT_FLUSH_DENORMAL(r)     (IS_FLOAT_DENORMAL_OR_ZERO(r) ? (((UINT32)(r)) & FLOAT_SIGN_MASK) : ((UINT32)(r)))

/* nearbyintf() with the default FE_TONEAREST rounding (ties to even),
   portable to C89 compilers */
static float sharc_nearbyintf(float x)
{
	double d = (double)x;
	double fl = floor(d);
	double diff = d - fl;
	if (diff > 0.5)
		fl += 1.0;
	else if (diff == 0.5)
	{
		if (fmod(fl, 2.0) != 0.0)
			fl += 1.0;
	}
	return (float)fl;
}


/*****************************************************************************/

/* Mantissa lookup-table for RECIPS opcode */
static const UINT32 recips_mantissa_lookup[128] =
{
	0x007f8000, 0x007e0000, 0x007c0000, 0x007a0000,
	0x00780000, 0x00760000, 0x00740000, 0x00720000,
	0x00700000, 0x006f0000, 0x006d0000, 0x006b0000,
	0x006a0000, 0x00680000, 0x00660000, 0x00650000,
	0x00630000, 0x00610000, 0x00600000, 0x005e0000,
	0x005d0000, 0x005b0000, 0x005a0000, 0x00590000,
	0x00570000, 0x00560000, 0x00540000, 0x00530000,
	0x00520000, 0x00500000, 0x004f0000, 0x004e0000,
	0x004c0000, 0x004b0000, 0x004a0000, 0x00490000,
	0x00470000, 0x00460000, 0x00450000, 0x00440000,
	0x00430000, 0x00410000, 0x00400000, 0x003f0000,
	0x003e0000, 0x003d0000, 0x003c0000, 0x003b0000,
	0x003a0000, 0x00390000, 0x00380000, 0x00370000,
	0x00360000, 0x00350000, 0x00340000, 0x00330000,
	0x00320000, 0x00310000, 0x00300000, 0x002f0000,
	0x002e0000, 0x002d0000, 0x002c0000, 0x002b0000,
	0x002a0000, 0x00290000, 0x00280000, 0x00280000,
	0x00270000, 0x00260000, 0x00250000, 0x00240000,
	0x00230000, 0x00230000, 0x00220000, 0x00210000,
	0x00200000, 0x001f0000, 0x001f0000, 0x001e0000,
	0x001d0000, 0x001c0000, 0x001c0000, 0x001b0000,
	0x001a0000, 0x00190000, 0x00190000, 0x00180000,
	0x00170000, 0x00170000, 0x00160000, 0x00150000,
	0x00140000, 0x00140000, 0x00130000, 0x00120000,
	0x00120000, 0x00110000, 0x00100000, 0x00100000,
	0x000f0000, 0x000f0000, 0x000e0000, 0x000d0000,
	0x000d0000, 0x000c0000, 0x000c0000, 0x000b0000,
	0x000a0000, 0x000a0000, 0x00090000, 0x00090000,
	0x00080000, 0x00070000, 0x00070000, 0x00060000,
	0x00060000, 0x00050000, 0x00050000, 0x00040000,
	0x00040000, 0x00030000, 0x00030000, 0x00020000,
	0x00020000, 0x00010000, 0x00010000, 0x00000000,
};

/* Mantissa lookup-table for RSQRTS opcode */
static const UINT32 rsqrts_mantissa_lookup[128] =
{
	0x00350000, 0x00330000, 0x00320000, 0x00300000,
	0x002f0000, 0x002e0000, 0x002d0000, 0x002b0000,
	0x002a0000, 0x00290000, 0x00280000, 0x00270000,
	0x00260000, 0x00250000, 0x00230000, 0x00220000,
	0x00210000, 0x00200000, 0x001f0000, 0x001e0000,
	0x001e0000, 0x001d0000, 0x001c0000, 0x001b0000,
	0x001a0000, 0x00190000, 0x00180000, 0x00170000,
	0x00160000, 0x00160000, 0x00150000, 0x00140000,
	0x00130000, 0x00130000, 0x00120000, 0x00110000,
	0x00100000, 0x00100000, 0x000f0000, 0x000e0000,
	0x000e0000, 0x000d0000, 0x000c0000, 0x000b0000,
	0x000b0000, 0x000a0000, 0x000a0000, 0x00090000,
	0x00080000, 0x00080000, 0x00070000, 0x00070000,
	0x00060000, 0x00050000, 0x00050000, 0x00040000,
	0x00040000, 0x00030000, 0x00030000, 0x00020000,
	0x00020000, 0x00010000, 0x00010000, 0x00000000,
	0x007f8000, 0x007e0000, 0x007c0000, 0x007a0000,
	0x00780000, 0x00760000, 0x00740000, 0x00730000,
	0x00710000, 0x006f0000, 0x006e0000, 0x006c0000,
	0x006a0000, 0x00690000, 0x00670000, 0x00660000,
	0x00640000, 0x00630000, 0x00620000, 0x00600000,
	0x005f0000, 0x005e0000, 0x005c0000, 0x005b0000,
	0x005a0000, 0x00590000, 0x00570000, 0x00560000,
	0x00550000, 0x00540000, 0x00530000, 0x00520000,
	0x00510000, 0x004f0000, 0x004e0000, 0x004d0000,
	0x004c0000, 0x004b0000, 0x004a0000, 0x00490000,
	0x00480000, 0x00470000, 0x00460000, 0x00450000,
	0x00450000, 0x00440000, 0x00430000, 0x00420000,
	0x00410000, 0x00400000, 0x003f0000, 0x003e0000,
	0x003e0000, 0x003d0000, 0x003c0000, 0x003b0000,
	0x003a0000, 0x003a0000, 0x00390000, 0x00380000,
	0x00370000, 0x00370000, 0x00360000, 0x00350000,
};


/*****************************************************************************/
/* ALU helpers */

/* Fx + Fy */
static UINT32 FADD(int fx, int fy)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN_ADD(REG(fx), REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		SHARC_REG x, y;
		x.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fx));
		y.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fy));
		r.f = x.f + y.f;

		if (IS_FLOAT_INFINITY(r.r))
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				r.r = (INT32)((r.r & FLOAT_SIGN_MASK) | 0x7f7fffff);
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(r.r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((r.r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			r.r &= FLOAT_SIGN_MASK;
		}
		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? AN : 0;
	}

	m_core->astat |= AF;
	return (UINT32)r.r;
}

/* Fx - Fy */
static UINT32 FSUB(int fx, int fy)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN_SUB(REG(fx), REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		SHARC_REG x, y;
		x.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fx));
		y.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fy));
		r.f = x.f - y.f;

		if (IS_FLOAT_INFINITY(r.r))
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				r.r = (INT32)((r.r & FLOAT_SIGN_MASK) | 0x7f7fffff);
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(r.r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((r.r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			r.r &= FLOAT_SIGN_MASK;
		}
		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? AN : 0;
	}

	m_core->astat |= AF;
	return (UINT32)r.r;
}

/* (Fx + Fy) / 2 */
static UINT32 FAVG(int fx, int fy)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN_ADD(REG(fx), REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		/* TODO: this loses one bit of range due to the intermediate sub being constrained to single precision */
		SHARC_REG x, y;
		x.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fx));
		y.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fy));
		r.f = (x.f + y.f) * 0.5f;

		if (IS_FLOAT_INFINITY(r.r))
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				r.r = (INT32)((r.r & FLOAT_SIGN_MASK) | 0x7f7fffff);
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(r.r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((r.r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			r.r &= FLOAT_SIGN_MASK;
		}
		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? AN : 0;
	}

	m_core->astat |= AF;
	return (UINT32)r.r;
}

/* ABS Fx */
static UINT32 FABS(int fx)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN(REG(fx)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		if (IS_FLOAT_DENORMAL_OR_ZERO(REG(fx)))
		{
			r.r = 0;
			m_core->astat |= AZ;
		}
		else
		{
			r.r = (INT32)(REG(fx) & ~FLOAT_SIGN_MASK);
		}
		m_core->astat |= (REG(fx) & FLOAT_SIGN_MASK) ? AS : 0;
	}

	m_core->astat |= AF;
	return (UINT32)r.r;
}

/* MIN(Fx, Fy) */
static UINT32 FMIN(int fx, int fy)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN(REG(fx)) || IS_FLOAT_NAN(REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		/* TODO: flush denormals to zero, handle negative versus positive zero */
		/* std::min(a, b) */
		r = (FREG(fy) < FREG(fx)) ? m_core->r[fy] : m_core->r[fx];

		m_core->astat |= (r.f < 0.0f) ? AN : 0;
		m_core->astat |= (IS_FLOAT_ZERO(r.r)) ? AZ : 0;
	}

	m_core->astat |= AF;
	return (UINT32)r.r;
}

/* MAX(Fx, Fy) */
static UINT32 FMAX(int fx, int fy)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN(REG(fx)) || IS_FLOAT_NAN(REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		/* TODO: flush denormals to zero, handle negative versus positive zero */
		/* std::max(a, b) */
		r = (FREG(fx) < FREG(fy)) ? m_core->r[fy] : m_core->r[fx];

		m_core->astat |= (r.f < 0.0f) ? AN : 0;
		m_core->astat |= (IS_FLOAT_ZERO(r.r)) ? AZ : 0;
	}

	m_core->astat |= AF;
	return (UINT32)r.r;
}

/* Fx + Fy,   Fx - Fy */
static void FADD_FSUB(int fx, int fy, SHARC_REG *first, SHARC_REG *second)
{
	int xemax = (REG(fx) & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;
	int yemax = (REG(fy) & FLOAT_EXPONENT_MASK) == FLOAT_EXPONENT_MASK;

	if ((xemax && ((REG(fx) & FLOAT_MANTISSA_MASK) != 0)) || (yemax && (REG(fy) & FLOAT_MANTISSA_MASK) != 0))
	{
		/* at least one NaN */
		first->r = (INT32)FLOAT_CANONICAL_NAN;
		second->r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else if (!xemax || !yemax)
	{
		/* neither NaN, at least one not infinity */
		SHARC_REG x, y;
		x.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fx));
		y.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fy));
		first->f = x.f + y.f;
		second->f = x.f - y.f;

		if (IS_FLOAT_INFINITY(first->r))
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				first->r = (INT32)((first->r & FLOAT_SIGN_MASK) | 0x7f7fffff);
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(first->r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((first->r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			first->r &= FLOAT_SIGN_MASK;
		}

		if (IS_FLOAT_INFINITY(second->r))
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				second->r = (INT32)((second->r & FLOAT_SIGN_MASK) | 0x7f7fffff);
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(second->r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((second->r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			second->r &= FLOAT_SIGN_MASK;
		}

		m_core->astat |= ((first->r | second->r) & FLOAT_SIGN_MASK) ? AN : 0;
	}
	else
	{
		/* both infinity */
		/* note: as in MAME, the integer bit pattern is converted to float here */
		if ((REG(fx) ^ REG(fy)) & FLOAT_SIGN_MASK)
		{
			/* opposite signs - sum is NaN */
			first->r = (INT32)FLOAT_CANONICAL_NAN;
			second->f = (float)(UINT32)((REG(fx) & FLOAT_SIGN_MASK) | ((m_core->mode1 & MODE1_TRUNCATE) ? 0x7f7fffff : FLOAT_INFINITY));
		}
		else
		{
			/* same sign - difference is NaN */
			first->f = (float)(UINT32)((REG(fx) & FLOAT_SIGN_MASK) | ((m_core->mode1 & MODE1_TRUNCATE) ? 0x7f7fffff : FLOAT_INFINITY));
			second->r = (INT32)FLOAT_CANONICAL_NAN;
		}

		m_core->astat |= (REG(fx) & FLOAT_SIGN_MASK) ? AN : 0;
		m_core->astat |= AV | AI;
		m_core->stky |= AVS | AIS;
	}

	m_core->astat |= AF;
}

static UINT32 SCALB(SHARC_REG fx, int ry)
{
	UINT32 mantissa = fx.r & FLOAT_MANTISSA_MASK;
	UINT32 sign = fx.r & FLOAT_SIGN_MASK;

	int exponent = float_get_unbiased_exponent(fx.r);
	exponent += (INT32)REG(ry);

	if (exponent > 127)
	{
		/* overflow */
		m_core->astat |= AV;
		return sign | FLOAT_INFINITY;
	}
	else if (exponent < -126)
	{
		/* denormal */
		m_core->astat |= AZ;
		return sign;
	}
	else
	{
		return sign | float_make_biased_exponent(exponent) | mantissa;
	}
}


/*****************************************************************************/
/* Multiplier helpers */

/* Fn = Fx * Fy */
static UINT32 FMUL(int fx, int fy)
{
	SHARC_REG r;
	if (IS_FLOAT_NAN_MUL(REG(fx), REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= MI;
		m_core->stky |= MIS;
	}
	else
	{
		r.f = FREG(fx) * FREG(fy);

		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? MN : 0;
		m_core->astat |= IS_FLOAT_INFINITY(r.r) ? MV : 0;
		m_core->astat |= IS_FLOAT_DENORMAL(r.r) ? MU : 0;
		m_core->stky |= IS_FLOAT_INFINITY(r.r) ? MVS : 0;
		m_core->stky |= IS_FLOAT_DENORMAL(r.r) ? MUS : 0;
	}
	return (UINT32)r.r;
}


/*****************************************************************************/
/* Integer ALU operations */

/* Rn = Rx + Ry */
static void compute_add(int rn, int rx, int ry)
{
	UINT32 r = UIREG(rx) + UIREG(ry);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_ADD(r, REG(rx), REG(ry));
	SET_FLAG_AC_ADD(r, REG(rx), REG(ry));

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx - Ry */
static void compute_sub(int rn, int rx, int ry)
{
	UINT32 r = UIREG(rx) - UIREG(ry);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_SUB(r, REG(rx), REG(ry));
	SET_FLAG_AC_SUB(r, REG(rx), REG(ry));

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx + Ry + CI */
static void compute_add_ci(int rn, int rx, int ry)
{
	int c = (m_core->astat & AC) ? 1 : 0;
	UINT32 r = UIREG(rx) + UIREG(ry) + c;

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_ADD(r, REG(rx), REG(ry));
	SET_FLAG_AC_ADD(r, REG(rx), REG(ry));
	if ((c == 1) && (UIREG(ry) == 0xffffffff))
		m_core->astat |= AC;

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx - Ry + CI - 1 */
static void compute_sub_ci(int rn, int rx, int ry)
{
	int c = (m_core->astat & AC) ? 1 : 0;
	UINT32 r = UIREG(rx) - UIREG(ry) + c - 1;

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_SUB(r, REG(rx), REG(ry));
	if ((c != 0) || (UIREG(ry) != 0xffffffff))
		SET_FLAG_AC_SUB(r, REG(rx), REG(ry));

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* COMP(Rx, Ry) */
static void compute_comp(int rx, int ry)
{
	UINT32 comp_accum;

	CLEAR_ALU_FLAGS();
	if (REG(rx) == REG(ry))
		m_core->astat |= AZ;
	else if ((INT32)REG(rx) < (INT32)REG(ry))
		m_core->astat |= AN;

	/* Update ASTAT compare accumulation register */
	comp_accum = (m_core->astat >> 1) & 0x7f000000;
	if ((m_core->astat & (AZ | AN)) == 0)
		comp_accum |= 0x80000000;

	m_core->astat &= 0x00ffffff;
	m_core->astat |= comp_accum;

	m_core->astat &= ~AF;
}

/* Rn = Rx + CI */
static void compute_add_ci1(int rn, int rx)
{
	int c = (m_core->astat & AC) ? 1 : 0;
	UINT32 r = UIREG(rx) + c;

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_ADD(r, REG(rx), 0);
	SET_FLAG_AC_ADD(r, REG(rx), 0);

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx + CI - 1 */
static void compute_sub_ci1(int rn, int rx)
{
	int c = (m_core->astat & AC) ? 1 : 0;
	UINT32 r = UIREG(rx) + c - 1;

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_SUB(r, REG(rx), 0);
	SET_FLAG_AC_SUB(r, REG(rx), 0);

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx + 1 */
static void compute_inc(int rn, int rx)
{
	UINT32 r = UIREG(rx) + 1;

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_ADD(r, REG(rx), 1);
	SET_FLAG_AC_ADD(r, REG(rx), 1);

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx - 1 */
static void compute_dec(int rn, int rx)
{
	UINT32 r = UIREG(rx) - 1;

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_SUB(r, REG(rx), 1);
	SET_FLAG_AC_SUB(r, REG(rx), 1);

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = -Rx */
static void compute_neg(int rn, int rx)
{
	UINT32 r = 0 - UIREG(rx);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AV_SUB(r, 0, REG(rx));
	SET_FLAG_AC_SUB(r, 0, REG(rx));

	if ((m_core->mode1 & MODE1_ALUSAT) && (m_core->astat & AV))
		SATURATE(r);

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = ABS Rx */
static void compute_abs(int rn, int rx)
{
	UINT32 r = (REG(rx) < 0) ? (0 - UIREG(rx)) : UIREG(rx);

	CLEAR_ALU_FLAGS();
	if ((INT32)r < 0)
	{
		m_core->astat |= AV;
		m_core->stky |= AOS;
		if (m_core->mode1 & MODE1_ALUSAT)
			r = 0x7fffffff;
	}

	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);
	m_core->astat |= (REG(rx) < 0) ? AS : 0;

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = PASS Rx */
static void compute_pass(int rn, int rx)
{
	CLEAR_ALU_FLAGS();
	/* TODO: floating-point extension field is set to 0 */

	REG(rn) = REG(rx);
	if (REG(rn) == 0)
		m_core->astat |= AZ;
	if (UIREG(rn) & 0x80000000)
		m_core->astat |= AN;

	m_core->astat &= ~AF;
}

/* Rn = Rx AND Ry */
static void compute_and(int rn, int rx, int ry)
{
	UINT32 r = UIREG(rx) & UIREG(ry);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx OR Ry */
static void compute_or(int rn, int rx, int ry)
{
	UINT32 r = UIREG(rx) | UIREG(ry);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = Rx XOR Ry */
static void compute_xor(int rn, int rx, int ry)
{
	UINT32 r = UIREG(rx) ^ UIREG(ry);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = NOT Rx */
static void compute_not(int rn, int rx)
{
	UINT32 r = ~UIREG(rx);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = MIN(Rx, Ry) */
static void compute_min(int rn, int rx, int ry)
{
	UINT32 r = (UINT32)((REG(ry) < REG(rx)) ? REG(ry) : REG(rx));

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = MAX(Rx, Ry) */
static void compute_max(int rn, int rx, int ry)
{
	UINT32 r = (UINT32)((REG(rx) < REG(ry)) ? REG(ry) : REG(rx));

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}

/* Rn = CLIP Rx BY Ry */
static void compute_clip(int rn, int rx, int ry)
{
	/* std::clamp(Rx, -abs(Ry), abs(Ry)), with wrapping as on the host */
	INT32 absry = (INT32)((REG(ry) < 0) ? (0 - UIREG(ry)) : UIREG(ry));
	INT32 negabsry = (INT32)(0 - (UINT32)absry);
	UINT32 r;

	if (REG(rx) < negabsry)
		r = (UINT32)negabsry;
	else if (absry < REG(rx))
		r = (UINT32)absry;
	else
		r = UIREG(rx);

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r);
	SET_FLAG_AZ(r);

	REG(rn) = (INT32)r;
	m_core->astat &= ~AF;
}


/*****************************************************************************/
/* Floating-point ALU operations */

/* Fn = Fx + Fy */
static void compute_fadd(int fn, int fx, int fy)
{
	CLEAR_ALU_FLAGS();
	REG(fn) = (INT32)FADD(fx, fy);
}

/* Fn = Fx - Fy */
static void compute_fsub(int fn, int fx, int fy)
{
	CLEAR_ALU_FLAGS();
	REG(fn) = (INT32)FSUB(fx, fy);
}

/* Fn = ABS(Fx + Fy) */
static void compute_fadd_abs(int fn, int fx, int fy)
{
	SHARC_REG r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN_ADD(REG(fx), REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		SHARC_REG x, y;
		x.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fx));
		y.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fy));
		r.f = x.f + y.f;
		r.r &= ~FLOAT_SIGN_MASK;

		if ((UINT32)r.r == FLOAT_EXPONENT_MASK) /* sign bit won't be set, avoid redundant masking */
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				r.r = 0x7f7fffff;
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(r.r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((r.r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			r.r = 0;
		}
	}

	m_core->r[fn] = r;
	m_core->astat |= AF;
}

/* Fn = ABS(Fx - Fy) */
static void compute_fsub_abs(int fn, int fx, int fy)
{
	SHARC_REG r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN_SUB(REG(fx), REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		SHARC_REG x, y;
		x.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fx));
		y.r = (INT32)FLOAT_FLUSH_DENORMAL(REG(fy));
		r.f = x.f - y.f;
		r.r &= ~FLOAT_SIGN_MASK;

		if ((UINT32)r.r == FLOAT_EXPONENT_MASK) /* sign bit won't be set, avoid redundant masking */
		{
			m_core->astat |= AV;
			m_core->stky |= AVS;
			if (m_core->mode1 & MODE1_TRUNCATE)
				r.r = 0x7f7fffff;
		}
		else if (IS_FLOAT_DENORMAL_OR_ZERO(r.r))
		{
			m_core->astat |= AZ;
			m_core->stky |= ((r.r & FLOAT_MANTISSA_MASK) != 0) ? AUS : 0;
			r.r = 0;
		}
	}

	m_core->r[fn] = r;
	m_core->astat |= AF;
}

/* Fn = (Fx + Fy) / 2 */
static void compute_favg(int fn, int fx, int fy)
{
	CLEAR_ALU_FLAGS();
	REG(fn) = (INT32)FAVG(fx, fy);
}

/* COMP(Fx, Fy) */
static void compute_fcomp(int fx, int fy)
{
	UINT32 comp_accum;

	CLEAR_ALU_FLAGS();
	if (IS_FLOAT_NAN(REG(fx)) || IS_FLOAT_NAN(REG(fy)))
	{
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		if (FREG(fx) == FREG(fy))
			m_core->astat |= AZ;
		else if (FREG(fx) < FREG(fy))
			m_core->astat |= AN;
	}

	/* Update ASTAT compare accumulation register */
	/* TODO: confirm whether this is set for unordered operands */
	comp_accum = (m_core->astat >> 1) & 0x7f000000;
	if ((m_core->astat & (AZ | AN)) == 0)
		comp_accum |= 0x80000000;

	m_core->astat &= 0x00ffffff;
	m_core->astat |= comp_accum;

	m_core->astat |= AF;
}

/* Fn = -Fx */
static void compute_fneg(int fn, int fx)
{
	SHARC_REG r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN(REG(fx)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		if (IS_FLOAT_DENORMAL_OR_ZERO(REG(fx)))
		{
			r.r = (INT32)(~UIREG(fx) & FLOAT_SIGN_MASK);
			m_core->astat |= AZ;
		}
		else
		{
			r.r = (INT32)(UIREG(fx) ^ FLOAT_SIGN_MASK);
		}
		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? AN : 0;
	}

	m_core->r[fn] = r;
	m_core->astat |= AF;
}

/* Fn = ABS Fx */
static void compute_fabs(int fn, int fx)
{
	CLEAR_ALU_FLAGS();
	REG(fn) = (INT32)FABS(fx);
}

/* Fn = PASS Fx */
static void compute_fpass(int fn, int fx)
{
	SHARC_REG r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN(REG(fx)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		if (IS_FLOAT_DENORMAL_OR_ZERO(REG(fx)))
		{
			r.r = (INT32)(UIREG(fx) & FLOAT_SIGN_MASK);
			m_core->astat |= AZ;
		}
		else
		{
			r.r = REG(fx);
		}
		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? AN : 0;
	}

	m_core->r[fn] = r;
	m_core->astat |= AF;
}

/* Fn = SCALB Fx BY Fy */
static void compute_scalb(int fn, int fx, int ry)
{
	/* verified */
	SHARC_REG r;
	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN(REG(fx)))
	{
		m_core->astat |= AI;
		m_core->stky |= AIS;

		REG(fn) = (INT32)FLOAT_CANONICAL_NAN;
	}
	else
	{
		r.r = (INT32)SCALB(m_core->r[fx], ry);

		/* AN */
		SET_FLAG_AN((UINT32)r.r);
		/* AZ */
		m_core->astat |= IS_FLOAT_ZERO(r.r) ? AZ : 0;
		/* AUS */
		m_core->stky |= (IS_FLOAT_DENORMAL(r.r)) ? AUS : 0;

		m_core->r[fn] = r;
	}
	m_core->astat |= AF;
}

/* Rn = LOGB Fx */
static void compute_logb(int rn, int fx)
{
	/* verified */
	UINT32 r = UIREG(fx);

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_INFINITY(REG(fx)))
	{
		REG(rn) = (INT32)FLOAT_INFINITY;

		m_core->astat |= AV;
	}
	else if (IS_FLOAT_ZERO(REG(fx)))
	{
		REG(rn) = (INT32)(FLOAT_SIGN_MASK | FLOAT_INFINITY);

		m_core->astat |= AV;
	}
	else if (IS_FLOAT_NAN(REG(fx)))
	{
		REG(rn) = (INT32)FLOAT_CANONICAL_NAN;

		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		int exponent = float_get_unbiased_exponent(r);

		/* AN */
		SET_FLAG_AN((UINT32)exponent);
		/* AZ */
		SET_FLAG_AZ(exponent);

		REG(rn) = exponent;
	}
	m_core->astat |= AF;
}

/* Rn = FIX Fx BY Ry */
static void compute_fix_scaled(int rn, int fx, int ry)
{
	INT32 alu_i;
	SHARC_REG r_alu;

	r_alu.r = (INT32)SCALB(m_core->r[fx], ry);
	if (m_core->mode1 & MODE1_TRUNCATE)
	{
		{ double t = floor(r_alu.f); alu_i = (INT32)t; }
	}
	else
	{
		{ float t = sharc_nearbyintf(r_alu.f); alu_i = (INT32)t; } /* assume rounding mode is set to FE_TONEAREST */
	}

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN((UINT32)alu_i);
	/* AZ */
	SET_FLAG_AZ(alu_i);
	/* AU */
	m_core->stky |= (IS_FLOAT_DENORMAL(r_alu.r)) ? AUS : 0;
	/* AI */
	m_core->astat |= (IS_FLOAT_NAN(REG(fx))) ? AI : 0;
	/* TODO: AV flag */

	REG(rn) = alu_i;
	m_core->astat |= AF;
}

/* Rn = FIX Fx */
static void compute_fix(int rn, int fx)
{
	INT32 alu_i;
	SHARC_REG r_alu;

	r_alu = m_core->r[fx];
	if (m_core->mode1 & MODE1_TRUNCATE)
	{
		{ double t = floor(r_alu.f); alu_i = (INT32)t; }
	}
	else
	{
		{ float t = sharc_nearbyintf(r_alu.f); alu_i = (INT32)t; } /* assume rounding mode is set to FE_TONEAREST */
	}

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN((UINT32)alu_i);
	/* AZ */
	SET_FLAG_AZ(alu_i);
	/* AU */
	m_core->stky |= (IS_FLOAT_DENORMAL(r_alu.r)) ? AUS : 0;
	/* AI */
	m_core->astat |= (IS_FLOAT_NAN(REG(fx))) ? AI : 0;
	/* TODO: AV flag */

	REG(rn) = alu_i;
	m_core->astat |= AF;
}

/* Fn = FLOAT Rx BY Ry */
static void compute_float_scaled(int fn, int rx, int ry)
{
	SHARC_REG x;
	x.f = (float)(INT32)REG(rx);

	/* verified */
	CLEAR_ALU_FLAGS();

	REG(fn) = (INT32)SCALB(x, ry);

	/* AN */
	SET_FLAG_AN(UIREG(fn));
	/* AZ */
	m_core->astat |= IS_FLOAT_DENORMAL_OR_ZERO(REG(fn)) ? AZ : 0;
	/* AU */
	m_core->stky |= IS_FLOAT_DENORMAL(REG(fn)) ? AUS : 0;

	m_core->astat |= AF;
}

/* Fn = FLOAT Rx */
static void compute_float(int fn, int rx)
{
	/* verified */
	FREG(fn) = (float)(INT32)REG(rx);

	CLEAR_ALU_FLAGS();
	/* AN */
	SET_FLAG_AN(UIREG(fn));
	/* AZ */
	m_core->astat |= IS_FLOAT_DENORMAL_OR_ZERO(REG(fn)) ? AZ : 0;
	/* AUS */
	m_core->stky |= IS_FLOAT_DENORMAL(REG(fn)) ? AUS : 0;
	/* TODO: AV flag */

	m_core->astat |= AF;
}

/* Fn = RECIPS Fx */
static void compute_recips(int fn, int fx)
{
	/* verified */
	UINT32 r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN(REG(fx)))
	{
		/* NaN */
		r = FLOAT_CANONICAL_NAN;

		/* AI */
		m_core->astat |= AI;

		/* AIS */
		m_core->stky |= AIS;
	}
	else if (IS_FLOAT_ZERO(REG(fx)))
	{
		/* +- Zero */
		r = (UIREG(fx) & FLOAT_SIGN_MASK) | FLOAT_INFINITY;

		m_core->astat |= AV;
	}
	else
	{
		UINT32 mantissa = UIREG(fx) & FLOAT_MANTISSA_MASK;
		UINT32 sign = UIREG(fx) & FLOAT_SIGN_MASK;

		int res_exponent = -float_get_unbiased_exponent(UIREG(fx)) - 1;

		UINT32 res_mantissa = recips_mantissa_lookup[mantissa >> 16];

		if (res_exponent > 125 || res_exponent < -126)
		{
			res_exponent = 0;
			res_mantissa = 0;
		}
		else
		{
			res_exponent = (res_exponent + FLOAT_EXPONENT_BIAS) & 0xff;
		}

		r = sign | ((UINT32)res_exponent << FLOAT_EXPONENT_SHIFT) | res_mantissa;

		SET_FLAG_AN(UIREG(fx));
		/* AZ */
		m_core->astat |= (IS_FLOAT_ZERO(r)) ? AZ : 0;
	}

	REG(fn) = (INT32)r;
	m_core->astat |= AF;
}

/* Fn = RSQRTS Fx */
static void compute_rsqrts(int fn, int fx)
{
	/* verified */
	UINT32 r;

	if (UIREG(fx) > 0x80000000)
	{
		/* non-zero negative */
		r = FLOAT_CANONICAL_NAN;
	}
	else if (IS_FLOAT_NAN(REG(fx)))
	{
		/* NaN */
		r = FLOAT_CANONICAL_NAN;
	}
	else
	{
		UINT32 mantissa = UIREG(fx) & 0xffffff;   /* mantissa + LSB of biased exponent */
		UINT32 sign = UIREG(fx) & FLOAT_SIGN_MASK;

		INT32 res_exponent = -(float_get_unbiased_exponent(UIREG(fx)) >> 1) - 1;

		UINT32 res_mantissa = rsqrts_mantissa_lookup[mantissa >> 17];

		r = sign | float_make_biased_exponent(res_exponent) | res_mantissa;
	}

	CLEAR_ALU_FLAGS();
	/* AN */
	m_core->astat |= (UIREG(fx) == 0x80000000) ? AN : 0;
	/* AZ & AV */
	m_core->astat |= (IS_FLOAT_ZERO(r)) ? AZ : 0;
	m_core->astat |= (IS_FLOAT_ZERO(REG(fx))) ? AV : 0;
	/* AI */
	m_core->astat |= (IS_FLOAT_NAN(REG(fx)) || (UIREG(fx) & FLOAT_SIGN_MASK)) ? AI : 0;
	/* AIS */
	if (m_core->astat & AI)   m_core->stky |= AIS;

	REG(fn) = (INT32)r;
	m_core->astat |= AF;
}

/* Fn = COPYSIGN(Fx, Fy) */
static void compute_fcopysign(int fn, int fx, int fy)
{
	SHARC_REG r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN(REG(fx)) || IS_FLOAT_NAN(REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		r.r = (INT32)(UIREG(fy) & FLOAT_SIGN_MASK);
		m_core->astat |= r.r ? AN : 0; /* only the sign bit is set here so it avoids need to mask */
		if (IS_FLOAT_DENORMAL_OR_ZERO(REG(fx)))
			m_core->astat |= AZ;
		else
			r.r |= (INT32)(UIREG(fx) & ~FLOAT_SIGN_MASK);
	}

	m_core->r[fn] = r;
	m_core->astat |= AF;
}

/* Fn = MIN(Fx, Fy) */
static void compute_fmin(int fn, int fx, int fy)
{
	CLEAR_ALU_FLAGS();
	REG(fn) = (INT32)FMIN(fx, fy);
}

/* Fn = MAX(Fx, Fy) */
static void compute_fmax(int fn, int fx, int fy)
{
	CLEAR_ALU_FLAGS();
	REG(fn) = (INT32)FMAX(fx, fy);
}

/* Fn = CLIP Fx BY Fy */
static void compute_fclip(int fn, int fx, int fy)
{
	SHARC_REG r;

	CLEAR_ALU_FLAGS();

	if (IS_FLOAT_NAN(REG(fx)) || IS_FLOAT_NAN(REG(fy)))
	{
		r.r = (INT32)FLOAT_CANONICAL_NAN;
		m_core->astat |= AI;
		m_core->stky |= AIS;
	}
	else
	{
		if (IS_FLOAT_DENORMAL_OR_ZERO(REG(fx)) || IS_FLOAT_DENORMAL_OR_ZERO(REG(fy)))
		{
			r.r = (INT32)(UIREG(fx) & FLOAT_SIGN_MASK);
			m_core->astat |= AZ;
		}
		else
		{
			SHARC_REG absry, negabsry;
			absry.r = (INT32)(UIREG(fy) & ~FLOAT_SIGN_MASK);
			negabsry.r = (INT32)(UIREG(fy) | FLOAT_SIGN_MASK);
			/* std::clamp(Fx, -|Fy|, |Fy|) */
			if (FREG(fx) < negabsry.f)
				r = negabsry;
			else if (absry.f < FREG(fx))
				r = absry;
			else
				r = m_core->r[fx];
			m_core->astat |= IS_FLOAT_ZERO(r.r) ? AZ : 0;
		}
		m_core->astat |= (r.r & FLOAT_SIGN_MASK) ? AN : 0;
	}

	m_core->r[fn] = r;
	m_core->astat |= AF;
}


/*****************************************************************************/
/* Multiplier opcodes */

/* Rn = (unsigned)Rx * (unsigned)Ry, integer, no rounding */
static void compute_mul_uuin(int rn, int rx, int ry)
{
	UINT64 r = (UINT64)UIREG(rx) * (UINT64)UIREG(ry);

	CLEAR_MULTIPLIER_FLAGS();
	SET_FLAG_MN((UINT32)r);
	SET_FLAG_MV(r);
	SET_FLAG_MU(r);

	REG(rn) = (INT32)(UINT32)r;
}

/* Rn = (signed)Rx * (signed)Ry, integer, no rounding */
static void compute_mul_ssin(int rn, int rx, int ry)
{
	UINT64 r = (UINT64)((INT64)REG(rx) * (INT64)REG(ry));

	CLEAR_MULTIPLIER_FLAGS();
	SET_FLAG_MN((UINT32)r);
	SET_FLAG_MV(r);
	SET_FLAG_MU(r);

	REG(rn) = (INT32)(UINT32)r;
}

/* MRF + (signed)Rx * (signed)Ry, integer, no rounding */
static UINT32 compute_mrf_plus_mul_ssin(int rx, int ry)
{
	UINT64 r = m_core->mrf + (UINT64)((INT64)REG(rx) * (INT64)REG(ry));

	CLEAR_MULTIPLIER_FLAGS();
	SET_FLAG_MN((UINT32)r);
	SET_FLAG_MV(r);
	SET_FLAG_MU(r);

	return (UINT32)r;
}

/* MRB + (signed)Rx * (signed)Ry, integer, no rounding */
static UINT32 compute_mrb_plus_mul_ssin(int rx, int ry)
{
	UINT64 r = m_core->mrb + (UINT64)((INT64)REG(rx) * (INT64)REG(ry));

	CLEAR_MULTIPLIER_FLAGS();
	SET_FLAG_MN((UINT32)r);
	SET_FLAG_MV(r);
	SET_FLAG_MU(r);

	return (UINT32)r;
}

/* Fn = Fx * Fy */
static void compute_fmul(int fn, int fx, int fy)
{
	CLEAR_MULTIPLIER_FLAGS();
	REG(fn) = (INT32)FMUL(fx, fy);
}

/*****************************************************************************/

/* multi function opcodes */

/* integer*/
static void compute_multi_mr_to_reg(int ai, int rk)
{
	switch (ai)
	{
		case 0:     SET_UREG(rk, (UINT32)m_core->mrf); break;
		case 1:     SET_UREG(rk, (UINT32)(m_core->mrf >> 32)); break;
		case 2:     sharc_error("SHARC: tried to load MR2F\n"); break;
		case 4:     SET_UREG(rk, (UINT32)m_core->mrb); break;
		case 5:     SET_UREG(rk, (UINT32)(m_core->mrb >> 32)); break;
		case 6:     sharc_error("SHARC: tried to load MR2B\n"); break;
		default:    sharc_error("SHARC: unknown ai %d in mr_to_reg\n", ai); break;
	}

	CLEAR_MULTIPLIER_FLAGS();
}

static void compute_multi_reg_to_mr(int ai, int rk)
{
	/* note: as in MAME, ~0xffffffff is a 32-bit constant here, so the
	   whole register is replaced by the low word */
	switch (ai)
	{
		case 0:     m_core->mrf &= (UINT32)~0xffffffff; m_core->mrf |= GET_UREG(rk); break;
		case 1:     m_core->mrf &= 0xffffffff; m_core->mrf |= (UINT64)GET_UREG(rk) << 32; break;
		case 2:     sharc_error("SHARC: tried to write MR2F\n"); break;
		case 4:     m_core->mrb &= (UINT32)~0xffffffff; m_core->mrb |= GET_UREG(rk); break;
		case 5:     m_core->mrb &= 0xffffffff; m_core->mrb |= (UINT64)GET_UREG(rk) << 32; break;
		case 6:     sharc_error("SHARC: tried to write MR2B\n"); break;
		default:    sharc_error("SHARC: unknown ai %d in reg_to_mr\n", ai); break;
	}

	CLEAR_MULTIPLIER_FLAGS();
}

/* Ra = Rx + Ry,   Rs = Rx - Ry */
static void compute_dual_add_sub(int ra, int rs, int rx, int ry)
{
	UINT32 r_add = UIREG(rx) + UIREG(ry);
	UINT32 r_sub = UIREG(rx) - UIREG(ry);
	int av_add = (~(UIREG(rx) ^ UIREG(ry)) & (UIREG(rx) ^ r_add) & 0x80000000) != 0;
	int av_sub = ((UIREG(rx) ^ UIREG(ry)) & (UIREG(rx) ^ r_sub) & 0x80000000) != 0;

	CLEAR_ALU_FLAGS();
	if (av_add || av_sub)
	{
		m_core->astat |= AV;
		m_core->stky |= AOS;
	}
	m_core->astat |= ((r_add < UIREG(rx)) || (r_sub <= UIREG(rx))) ? AC : 0;

	if (m_core->mode1 & MODE1_ALUSAT)
	{
		if (av_add) SATURATE(r_add);
		if (av_sub) SATURATE(r_sub);
	}

	m_core->astat |= ((r_add == 0) || (r_sub == 0)) ? AZ : 0;
	m_core->astat |= ((r_add | r_sub) & 0x80000000) ? AN : 0;

	REG(ra) = (INT32)r_add;
	REG(rs) = (INT32)r_sub;

	m_core->astat &= ~AF;
}

/* Rm = (signed)Rxm * (signed)Rym, fractional, rounding,   Ra = Rxa + Rya */
static void compute_mul_ssfr_add(int rm, int rxm, int rym, int ra, int rxa, int rya)
{
	UINT32 r_mul = (UINT32)(INT32)(((INT64)REG(rxm) * (INT64)REG(rym)) >> 31);
	UINT32 r_add = UIREG(rxa) + UIREG(rya);

	CLEAR_MULTIPLIER_FLAGS();
	SET_FLAG_MN(r_mul);
	/* TODO: MV flag */
	/* TODO: MU flag */
	/* TODO: MI flag */

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r_add);
	SET_FLAG_AZ(r_add);
	SET_FLAG_AV_ADD(r_add, REG(rxa), REG(rya));
	SET_FLAG_AC_ADD(r_add, REG(rxa), REG(rya));


	REG(rm) = (INT32)r_mul;
	REG(ra) = (INT32)r_add;

	m_core->astat &= ~AF;
}

/* Rm = (signed)Rxm * (signed)Rym, fractional, rounding,   Ra = Rxa - Rya */
static void compute_mul_ssfr_sub(int rm, int rxm, int rym, int ra, int rxa, int rya)
{
	UINT32 r_mul = (UINT32)(INT32)(((INT64)REG(rxm) * (INT64)REG(rym)) >> 31);
	UINT32 r_sub = UIREG(rxa) - UIREG(rya);

	CLEAR_MULTIPLIER_FLAGS();
	SET_FLAG_MN(r_mul);
	/* TODO: MV flag */
	/* TODO: MU flag */
	/* TODO: MI flag */

	CLEAR_ALU_FLAGS();
	SET_FLAG_AN(r_sub);
	SET_FLAG_AZ(r_sub);
	SET_FLAG_AV_SUB(r_sub, REG(rxa), REG(rya));
	SET_FLAG_AC_SUB(r_sub, REG(rxa), REG(rya));


	REG(rm) = (INT32)r_mul;
	REG(ra) = (INT32)r_sub;

	m_core->astat &= ~AF;
}


/*****************************************************************************/
/* Dual add/subtract */

/* Fa = Fx + Fy,   Fs = Fx - Fy */
static void compute_dual_fadd_fsub(int fa, int fs, int fx, int fy)
{
	SHARC_REG r_add, r_sub;
	CLEAR_ALU_FLAGS();
	FADD_FSUB(fx, fy, &r_add, &r_sub);
	m_core->r[fa] = r_add;
	m_core->r[fs] = r_sub;
}


/*****************************************************************************/
/* Floating-point multiplication and ALU operation */

/* Fm = Fxm * Fym,   Fa = Fxa + Fya */
static void compute_fmul_fadd(int fm, int fxm, int fym, int fa, int fxa, int fya)
{
	SHARC_REG r_mul, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	r_alu.r = (INT32)FADD(fxa, fya);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
}

/* Fm = Fxm * Fym,   Fa = Fxa - Fya */
static void compute_fmul_fsub(int fm, int fxm, int fym, int fa, int fxa, int fya)
{
	SHARC_REG r_mul, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	r_alu.r = (INT32)FSUB(fxa, fya);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
}

/* Fm = Fxm * Fym,   Fa = FLOAT Rxa BY Rya */
static void compute_fmul_float_scaled(int fm, int fxm, int fym, int fa, int rxa, int rya)
{
	SHARC_REG r_mul, x, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);

	x.f = (float)(INT32)REG(rxa);

	r_alu.r = (INT32)SCALB(x, rya);

	m_core->astat |= (r_alu.f < 0.0f) ? AN : 0;
	/* AZ */
	m_core->astat |= IS_FLOAT_DENORMAL_OR_ZERO(r_alu.r) ? AZ : 0;
	/* AU */
	m_core->stky |= IS_FLOAT_DENORMAL(r_alu.r) ? AUS : 0;
	/* TODO: set AV if overflowed */

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
	m_core->astat |= AF;
}

/* Fm = Fxm * Fym,   Ra = FIX Fxa BY Rya */
static void compute_fmul_fix_scaled(int fm, int fxm, int fym, int ra, int fxa, int rya)
{
	SHARC_REG r_mul, r_alu;
	INT32 alu_i;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);

	r_alu.r = (INT32)SCALB(m_core->r[fxa], rya);

	if (m_core->mode1 & MODE1_TRUNCATE)
	{
		alu_i = (INT32)(r_alu.f);
	}
	else
	{
		{ float t = sharc_nearbyintf(r_alu.f); alu_i = (INT32)t; } /* assume rounding mode is set to FE_TONEAREST */
	}

	SET_FLAG_AN((UINT32)alu_i);
	/* AZ */
	SET_FLAG_AZ(alu_i);
	/* AU */
	m_core->stky |= (IS_FLOAT_DENORMAL(r_alu.r)) ? AUS : 0;
	/* AI */
	m_core->astat |= (IS_FLOAT_NAN(REG(fxa))) ? AI : 0;
	/* TODO: AV flag */

	m_core->r[fm] = r_mul;
	REG(ra) = alu_i;
	m_core->astat |= AF;
}

static void compute_fmul_favg(int fm, int fxm, int fym, int fa, int fxa, int fya)
{
	SHARC_REG r_mul, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	r_alu.r = (INT32)FAVG(fxa, fya);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
}

static void compute_fmul_fabs(int fm, int fxm, int fym, int fa, int fxa)
{
	SHARC_REG r_mul, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	r_alu.r = (INT32)FABS(fxa);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
}

/* Fm = Fxm * Fym,   Fa = MAX(Fxa, Fya) */
static void compute_fmul_fmax(int fm, int fxm, int fym, int fa, int fxa, int fya)
{
	SHARC_REG r_mul, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	r_alu.r = (INT32)FMAX(fxa, fya);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
}


/* Fm = Fxm * Fym,   Fa = MIN(Fxa, Fya) */
static void compute_fmul_fmin(int fm, int fxm, int fym, int fa, int fxa, int fya)
{
	SHARC_REG r_mul, r_alu;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	r_alu.r = (INT32)FMIN(fxa, fya);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_alu;
}


/*****************************************************************************/
/* Multiplication and dual add/subtract */

/* Fm = Fxm * Fym,   Fa = Fxa + Fya,   Fs = Fxa - Fya */
static void compute_fmul_dual_fadd_fsub(int fm, int fxm, int fym, int fa, int fs, int fxa, int fya)
{
	SHARC_REG r_mul, r_add, r_sub;

	CLEAR_MULTIPLIER_FLAGS();
	CLEAR_ALU_FLAGS();

	r_mul.r = (INT32)FMUL(fxm, fym);
	FADD_FSUB(fxa, fya, &r_add, &r_sub);

	m_core->r[fm] = r_mul;
	m_core->r[fa] = r_add;
	m_core->r[fs] = r_sub;
}
