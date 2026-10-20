/*
	SCSP effect DSP

	Port of the SCSP DSP of current MAME (scspdsp.cpp, by ElSemi and
	R. Belmont) to C89. Included by scsp.c, like scsplfo.c.

	The DSP runs its microprogram (up to 128 steps) once per sample. Its
	inputs are the slot sends (MIXS, selected by ISEL and scaled by IMXL),
	its outputs the 16 effect registers (EFREG), mixed by the EFSDL and
	EFPAN of slots 0-15. It keeps its delay lines (reverb, echo) in the
	sound RAM, in a ring buffer set by RBP and RBL.
*/

struct _SCSPDSP
{
	/* config */
	UINT16 *RAM;		/* sound RAM, as 16-bit words */
	UINT32 RAMmask;		/* in words */
	UINT32 RBP;		/* ring buffer pointer (in 4 kword units) */
	UINT32 RBL;		/* ring buffer length in words */

	/* context */
	INT16 COEF[64];		/* 13 bit signed, in the top bits */
	UINT16 MADRS[32];	/* memory offsets (in words) */
	UINT16 MPRO[128*4];	/* 128 steps of 64 bits */
	INT32 TEMP[128];	/* 24 bit signed */
	INT32 MEMS[32];		/* 24 bit signed */
	UINT32 DEC;

	/* input */
	INT32 MIXS[16];		/* 20 bit signed */
	INT16 EXTS[2];		/* external inputs, 16 bit signed (not connected) */

	/* output */
	INT16 EFREG[16];	/* 16 bit signed */

	int Stopped;
	int LastStep;
};

/* sign-extend the low "bits" bits of v */
static INLINE INT32 SCSPDSP_sext( INT32 v, int bits )
{
	UINT32 sign = (UINT32)1 << ( bits - 1 );
	UINT32 mask = ( sign << 1 ) - 1;
	return (INT32)( ( ( (UINT32)v & mask ) ^ sign ) - sign );
}

static INLINE INT32 SCSPDSP_clamp24( INT32 v )
{
	if( v < -0x00800000 ) return -0x00800000;
	if( v > 0x007fffff ) return 0x007fffff;
	return v;
}

/* 24 bit value to the 16 bit floating point format of the delay RAM */
static UINT16 SCSPDSP_PACK( INT32 val )
{
	UINT32 uval = (UINT32)val;
	UINT32 sign = ( uval >> 23 ) & 1;
	UINT32 temp = ( uval ^ ( uval << 1 ) ) & 0xffffff;
	UINT32 exponent = 0;
	int k;

	for( k = 0; k < 12; k++ )
	{
		if( temp & 0x800000 )
			break;
		temp <<= 1;
		exponent += 1;
	}
	if( exponent < 12 )
		uval = ( uval << exponent ) & 0x3fffff;
	else
		uval <<= 11;
	uval >>= 11;
	uval &= 0x7ff;
	uval |= sign << 15;
	uval |= exponent << 11;

	return (UINT16)uval;
}

static INT32 SCSPDSP_UNPACK( UINT16 val )
{
	UINT32 sign = ( val >> 15 ) & 1;
	int exponent = ( val >> 11 ) & 0xf;
	UINT32 mantissa = val & 0x7ff;
	UINT32 uval = mantissa << 11;
	INT32 ret;

	if( exponent > 11 )
	{
		exponent = 11;
		uval |= sign << 22;
	}
	else
		uval |= ( sign ^ 1 ) << 22;
	uval |= sign << 23;
	ret = SCSPDSP_sext( (INT32)uval, 24 );
	return ret >> exponent;
}

static void SCSPDSP_Init( struct _SCSPDSP *DSP, UINT16 *ram, UINT32 ramsize )
{
	memset( DSP, 0, sizeof( *DSP ) );
	DSP->RAM = ram;
	DSP->RAMmask = ( ramsize / 2 ) - 1;
	DSP->RBL = 8 * 1024;
	DSP->Stopped = 1;
}

static void SCSPDSP_Step( struct _SCSPDSP *DSP )
{
	INT32 ACC = 0;		/* 26 bit */
	INT32 SHIFTED = 0;	/* 24 bit */
	INT32 X;		/* 24 bit */
	INT32 Y = 0;		/* 13 bit */
	INT32 B;		/* 26 bit */
	INT32 INPUTS = 0;	/* 24 bit */
	INT32 MEMVAL = 0;
	INT32 FRC_REG = 0;	/* 13 bit */
	INT32 Y_REG = 0;	/* 24 bit */
	UINT32 ADDR;
	UINT32 ADRS_REG = 0;	/* 13 bit */
	int step;

	if( DSP->Stopped )
		return;

	memset( DSP->EFREG, 0, sizeof( DSP->EFREG ) );

	for( step = 0; step < DSP->LastStep; ++step )
	{
		UINT16 *IPtr = DSP->MPRO + ( step * 4 );

		UINT32 TRA   = ( IPtr[0] >>  8 ) & 0x7f;
		UINT32 TWT   = ( IPtr[0] >>  7 ) & 0x01;
		UINT32 TWA   = ( IPtr[0] >>  0 ) & 0x7f;

		UINT32 XSEL  = ( IPtr[1] >> 15 ) & 0x01;
		UINT32 YSEL  = ( IPtr[1] >> 13 ) & 0x03;
		UINT32 IRA   = ( IPtr[1] >>  6 ) & 0x3f;
		UINT32 IWT   = ( IPtr[1] >>  5 ) & 0x01;
		UINT32 IWA   = ( IPtr[1] >>  0 ) & 0x1f;

		UINT32 TABLE = ( IPtr[2] >> 15 ) & 0x01;
		UINT32 MWT   = ( IPtr[2] >> 14 ) & 0x01;
		UINT32 MRD   = ( IPtr[2] >> 13 ) & 0x01;
		UINT32 EWT   = ( IPtr[2] >> 12 ) & 0x01;
		UINT32 EWA   = ( IPtr[2] >>  8 ) & 0x0f;
		UINT32 ADRL  = ( IPtr[2] >>  7 ) & 0x01;
		UINT32 FRCL  = ( IPtr[2] >>  6 ) & 0x01;
		UINT32 SHFT  = ( IPtr[2] >>  4 ) & 0x03;
		UINT32 YRL   = ( IPtr[2] >>  3 ) & 0x01;
		UINT32 NEGB  = ( IPtr[2] >>  2 ) & 0x01;
		UINT32 ZERO  = ( IPtr[2] >>  1 ) & 0x01;
		UINT32 BSEL  = ( IPtr[2] >>  0 ) & 0x01;

		UINT32 NOFL  = ( IPtr[3] >> 15 ) & 0x01;
		UINT32 COEF  = ( IPtr[3] >>  9 ) & 0x3f;

		UINT32 MASA  = ( IPtr[3] >>  2 ) & 0x1f;
		UINT32 ADREB = ( IPtr[3] >>  1 ) & 0x01;
		UINT32 NXADR = ( IPtr[3] >>  0 ) & 0x01;

		INT64 v;

		/* inputs */
		if( IRA <= 0x1f )
			INPUTS = DSP->MEMS[IRA];
		else if( IRA <= 0x2f )
			INPUTS = DSP->MIXS[IRA - 0x20] << 4;	/* MIXS is 20 bit */
		else if( IRA <= 0x31 )
			INPUTS = DSP->EXTS[IRA - 0x30] << 8;	/* EXTS is 16 bit */
		else
			return;

		INPUTS = SCSPDSP_sext( INPUTS, 24 );

		if( IWT )
		{
			DSP->MEMS[IWA] = MEMVAL;	/* MEMVAL was read by the previous MRD */
			if( IRA == IWA )
				INPUTS = MEMVAL;
		}

		/* operands */
		if( !ZERO )
		{
			if( BSEL )
				B = ACC;
			else
				B = SCSPDSP_sext( DSP->TEMP[( TRA + DSP->DEC ) & 0x7f], 24 );
			if( NEGB )
				B = 0 - B;
		}
		else
			B = 0;

		if( XSEL )
			X = INPUTS;
		else
			X = SCSPDSP_sext( DSP->TEMP[( TRA + DSP->DEC ) & 0x7f], 24 );

		if( YSEL == 0 )
			Y = FRC_REG;
		else if( YSEL == 1 )
			Y = DSP->COEF[COEF] >> 3;	/* COEF is 16 bits */
		else if( YSEL == 2 )
			Y = ( Y_REG >> 11 ) & 0x1fff;
		else
			Y = ( Y_REG >> 4 ) & 0x0fff;

		if( YRL )
			Y_REG = INPUTS;

		/* shifter */
		if( SHFT == 0 )
			SHIFTED = SCSPDSP_clamp24( ACC );
		else if( SHFT == 1 )
			SHIFTED = SCSPDSP_clamp24( ACC * 2 );
		else if( SHFT == 2 )
			SHIFTED = SCSPDSP_sext( ACC * 2, 24 );
		else
			SHIFTED = SCSPDSP_sext( ACC, 24 );

		/* accumulator */
		Y = SCSPDSP_sext( Y, 13 );

		v = ( (INT64)X * (INT64)Y ) >> 12;
		ACC = (INT32)( v + B );

		if( TWT )
			DSP->TEMP[( TWA + DSP->DEC ) & 0x7f] = SHIFTED;

		if( FRCL )
		{
			if( SHFT == 3 )
				FRC_REG = SHIFTED & 0x0fff;
			else
				FRC_REG = ( SHIFTED >> 11 ) & 0x1fff;
		}

		/* delay RAM: odd steps only */
		if( ( MRD || MWT ) && ( step & 1 ) )
		{
			ADDR = DSP->MADRS[MASA];
			if( !TABLE )
				ADDR += DSP->DEC;
			if( ADREB )
				ADDR += ADRS_REG & 0x0fff;
			if( NXADR )
				ADDR++;
			if( !TABLE )
				ADDR &= DSP->RBL - 1;
			else
				ADDR &= 0xffff;
			ADDR += DSP->RBP << 12;
			ADDR &= DSP->RAMmask;
			if( MRD )
			{
				if( NOFL )
					MEMVAL = (INT32)( (UINT32)DSP->RAM[ADDR] << 8 );
				else
					MEMVAL = SCSPDSP_UNPACK( DSP->RAM[ADDR] );
			}
			if( MWT )
			{
				if( NOFL )
					DSP->RAM[ADDR] = (UINT16)( SHIFTED >> 8 );
				else
					DSP->RAM[ADDR] = SCSPDSP_PACK( SHIFTED );
			}
		}

		if( ADRL )
		{
			if( SHFT == 3 )
				ADRS_REG = ( SHIFTED >> 12 ) & 0xfff;
			else
				ADRS_REG = INPUTS >> 16;
		}

		if( EWT )
			DSP->EFREG[EWA] += SHIFTED >> 8;
	}
	--DSP->DEC;
	memset( DSP->MIXS, 0, sizeof( DSP->MIXS ) );
}

static void SCSPDSP_Start( struct _SCSPDSP *DSP )
{
	int i;

	DSP->Stopped = 0;
	for( i = 127; i >= 0; --i )
	{
		UINT16 *IPtr = DSP->MPRO + ( i * 4 );
		if( IPtr[0] || IPtr[1] || IPtr[2] || IPtr[3] )
			break;
	}
	DSP->LastStep = i + 1;
}
