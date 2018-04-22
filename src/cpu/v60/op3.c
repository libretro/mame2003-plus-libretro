UINT32 opINCB(void) /* TRUSTED */
{
	UINT8 appb;
	modAdd=PC+1;
	modDim=0;

	amLength1=ReadAMAddress();

	if (amFlag)
		appb=(UINT8)v60.reg[amOut];
	else
		appb=MemRead8(amOut);

	ADDB(appb, 1);

	if (amFlag)
		SETREG8(v60.reg[amOut], appb);
	else
		MemWrite8(amOut, appb);

	return amLength1+1;
}

UINT32 opINCH(void) /* TRUSTED */
{
	UINT16 apph;
	modAdd=PC+1;
	modDim=1;

	amLength1=ReadAMAddress();

	if (amFlag)
		apph=(UINT16)v60.reg[amOut];
	else
		apph=MemRead16(amOut);

	ADDW(apph, 1);

	if (amFlag)
		SETREG16(v60.reg[amOut], apph);
	else
		MemWrite16(amOut, apph);

	return amLength1+1;
}

UINT32 opINCW(void) /* TRUSTED */
{
	UINT32 appw;
	modAdd=PC+1;
	modDim=2;

	amLength1=ReadAMAddress();

	if (amFlag)
		appw=v60.reg[amOut];
	else
		appw=MemRead32(amOut);

	ADDL(appw, 1);

	if (amFlag)
		v60.reg[amOut]=appw;
	else
		MemWrite32(amOut,appw);

	return amLength1+1;
}

UINT32 opDECB(void) /* TRUSTED */
{
	UINT8 appb;
	modAdd=PC+1;
	modDim=0;

	amLength1=ReadAMAddress();

	if (amFlag)
		appb=(UINT8)v60.reg[amOut];
	else
		appb=MemRead8(amOut);

	SUBB(appb, 1);

	if (amFlag)
		SETREG8(v60.reg[amOut], appb);
	else
		MemWrite8(amOut, appb);

	return amLength1+1;
}

UINT32 opDECH(void) /* TRUSTED */
{
	UINT16 apph;
	modAdd=PC+1;
	modDim=1;

	amLength1=ReadAMAddress();

	if (amFlag)
		apph=(UINT16)v60.reg[amOut];
	else
		apph=MemRead16(amOut);

	SUBW(apph, 1);

	if (amFlag)
		SETREG16(v60.reg[amOut], apph);
	else
		MemWrite16(amOut, apph);

	return amLength1+1;
}

UINT32 opDECW(void) /* TRUSTED */
{
	UINT32 appw;
	modAdd=PC+1;
	modDim=2;

	amLength1=ReadAMAddress();

	if (amFlag)
		appw=v60.reg[amOut];
	else
		appw=MemRead32(amOut);

	SUBL(appw, 1);

	if (amFlag)
		v60.reg[amOut]=appw;
	else
		MemWrite32(amOut,appw);

	return amLength1+1;
}

UINT32 opJMP(void) /* TRUSTED */
{
	modAdd=PC+1;
	modDim=0;

	/* Read the address of the operand*/
	ReadAMAddress();

	/* It cannot be a register!!*/
	assert(amFlag==0);

	/* Jump there*/
	PC=amOut;
	ChangePC(PC);

	return 0;
}

UINT32 opJSR(void) /* TRUSTED */
{
	modAdd=PC + 1;
	modDim=0;

	/* Read the address of the operand*/
	amLength1=ReadAMAddress();

	/* It cannot be a register!!*/
	assert(amFlag==0);

	/* Save NextPC into the stack*/
	SP -= 4;
	MemWrite32(SP, PC + amLength1 + 1);

	/* Jump there*/
	PC=amOut;
	ChangePC(PC);

	return 0;
}

UINT32 opPREPARE(void)	/* somewhat TRUSTED */
{
	modAdd=PC+1;
	modDim=2;

	/* Read the operand*/
	amLength1=ReadAM();

	/* step 1: save frame pointer on the stack*/
	SP -= 4;
	MemWrite32(SP, FP);

	/* step 2: FP = new SP*/
	FP = SP;

	/* step 3: SP -= operand*/
	SP -= amOut;

	return amLength1 + 1;
}

UINT32 opRET(void) /* TRUSTED */
{
	modAdd=PC + 1;
	modDim=2;

	/* Read the operand*/
	ReadAM();

	/* Read return address from stack*/
	PC=MemRead32(SP);
	SP+=4;
	ChangePC(PC);

	/* Restore AP from stack*/
	AP=MemRead32(SP);
	SP+=4;

	/* Skip stack frame*/
	SP += amOut;

	return 0;
}

UINT32 opTRAP(void)
{
	modAdd=PC + 1;
	modDim=0;

	/* Read the operand*/
	amLength1=ReadAM();

	/* Normalize the flags*/
	NORMALIZEFLAGS();

	switch ((amOut >> 4) & 0xF)
	{
	case 0:
		if (!_OV) return amLength1+1;
		else break;
	case 1:
		if (_OV) return amLength1+1;
		else break;
	case 2:
		if (!_CY) return amLength1+1;
		else break;
	case 3:
		if (_CY) return amLength1+1;
		else break;
	case 4:
		if (!_Z) return amLength1+1;
		else break;
	case 5:
		if (_Z) return amLength1+1;
		else break;
	case 6:
		if (!(_CY | _Z)) return amLength1+1;
		else break;
	case 7:
		if ((_CY | _Z)) return amLength1+1;
		else break;
	case 8:
		if (!_S) return amLength1+1;
		else break;
	case 9:
		if (_S) return amLength1+1;
		else break;
	case 10:
		break;
	case 11:
		return amLength1+1;
	case 12:
		if (!(_S^_OV)) return amLength1+1;
		else break;
	case 13:
		if ((_S^_OV)) return amLength1+1;
		else break;
	case 14:
		if (!((_S^_OV)|_Z)) return amLength1+1;
		else break;
	case 15:
		if (((_S^_OV)|_Z)) return amLength1+1;
		else break;
	}

	UPDATEPSW();

	/* Issue the software trap with interrupts*/
	SP -= 4;
	MemWrite32(SP, 0x3000 + 0x100 * (amOut&0xF));

	SP -= 4;
	MemWrite32(SP, PSW);

	SP -= 4;
	MemWrite32(SP, PC + amLength1 + 1);

	PC = GETINTVECT(48 + (amOut&0xF));
	ChangePC(PC);

	return 0;
}

UINT32 opRETIU(void) /* TRUSTED */
{
	UINT32 tempPSW;
	modAdd=PC + 1;
	modDim=1;

	/* Read the operand*/
	ReadAM();

	/* Restore PC and PSW from stack*/
	PC = MemRead32(SP);
	SP += 4;
	ChangePC(PC);

	tempPSW=MemRead32(SP);
	SP += 4;

	/* Destroy stack frame*/
	SP += amOut;
	
	v60WritePSW(tempPSW);

	/* Update all the flags from PSW*/
	UPDATECPUFLAGS();
	UPDATEFPUFLAGS();

	return 0;
}

UINT32 opRETIS(void)
{
	UINT32 appw;

	modAdd=PC + 1;
	modDim=1;

	/* Read the operand*/
	ReadAM();

	/* Restore PC and PSW from stack*/
	PC = MemRead32(SP);
	SP += 4;
	ChangePC(PC);

	appw = MemRead32(SP);
	SP += 4;

	v60WritePSW(appw);

	/* Destroy stack frame*/
	SP += amOut;

	/* Update only CPU flags from PSW @@@*/
/*	UPDATECPUFLAGS();*/

	return 0;
}

UINT32 opSTTASK(void)
{
	int i;
	UINT32 adr;

	modAdd=PC + 1;
	modDim=2;

	amLength1 = ReadAM();

	adr = TCB;

	UPDATEPSW();
	v60WritePSW(PSW | 0x10000000);

	MemWrite32(adr, TKCW);
	adr += 4;
	if(SYCW & 0x100) {
		MemWrite32(adr, L0SP);
		adr += 4;
	}
	if(SYCW & 0x200) {
		MemWrite32(adr, L1SP);
		adr += 4;
	}
	if(SYCW & 0x400) {
		MemWrite32(adr, L2SP);
		adr += 4;
	}
	if(SYCW & 0x800) {
		MemWrite32(adr, L3SP);
		adr += 4;
	}

	/* 31 registers supported, _not_ 32*/
	for(i=0; i<31; i++)
		if(amOut & (1<<i)) {
			MemWrite32(adr, v60.reg[i]);
			adr += 4;
		}

	/* #### Ignore the virtual addressing crap.*/

	return amLength1 + 1;
}

UINT32 opGETPSW(void)
{
	UPDATEPSW();

	modAdd=PC + 1;
	modDim=2;
	modWriteValW=PSW;

	/* Write PSW to the operand*/
	amLength1=WriteAM();

	return amLength1 + 1;
}

UINT32 opTASI(void)
{
	UINT8 appb;
	modAdd=PC + 1;
	modDim=0;

	/* Load the address of the operand*/
	amLength1=ReadAMAddress();

	/* Load UINT8 from the address*/
	if (amFlag)
		appb=(UINT8)v60.reg[amOut&0x1F];
	else
		appb=MemRead8(amOut);

	/* Set the flags for SUB appb,FF*/
	SUBB(appb, 0xff);

	/* Write FF in the operand*/
	if (amFlag)
		SETREG8(v60.reg[amOut&0x1F], 0xFF);
	else
		MemWrite8(amOut,0xFF);

	return amLength1 + 1;
}

UINT32 opCLRTLB(void)
{
	modAdd=PC+1;
	modDim=2;

	/* Read the operand*/
	amLength1=ReadAM();

	/* @@@ TLB not yet emulated*/

	return amLength1 + 1;
}

UINT32 opPOPM(void)
{
	int i;

	modAdd=PC+1;
	modDim=2;

	/* Read the bit register list*/
	amLength1=ReadAM();

	for (i=0;i<31;i++)
		if (amOut & (1<<i))
		{
			v60.reg[i] = MemRead32(SP);
			SP += 4;
		}

	if (amOut & (1<<31))
	{
		PSW = (PSW & 0xFFFF0000) | MemRead16(SP);
		SP += 4;
		UPDATECPUFLAGS();
	}

	return amLength1 + 1;
}

UINT32 opPUSHM(void)
{
	int i;

	modAdd=PC+1;
	modDim=2;

	/* Read the bit register list*/
	amLength1=ReadAM();

	if (amOut & (1<<31))
	{
		UPDATEPSW();
		SP -= 4;
		MemWrite32(SP,PSW);
	}

	for (i=0;i<31;i++)
		if (amOut & (1<<(30-i)))
		{
			SP -= 4;
			MemWrite32(SP,v60.reg[(30-i)]);
		}


	return amLength1 + 1;
}

UINT32 opTESTB(void) /* TRUSTED */
{
	modAdd=PC+1;
	modDim=0;

	/* Read the operand*/
	amLength1=ReadAM();

	_Z = (amOut == 0);
	_S = ((amOut & 0x80) != 0);
	_CY = 0;
	_OV = 0;

	return amLength1 + 1;
}

UINT32 opTESTH(void) /* TRUSTED */
{
	modAdd=PC+1;
	modDim=1;

	/* Read the operand*/
	amLength1=ReadAM();

	_Z = (amOut == 0);
	_S = ((amOut & 0x8000) != 0);
	_CY = 0;
	_OV = 0;

	return amLength1 + 1;
}

UINT32 opTESTW(void) /* TRUSTED */
{
	modAdd=PC+1;
	modDim=2;

	/* Read the operand*/
	amLength1=ReadAM();

	_Z = (amOut == 0);
	_S = ((amOut & 0x80000000) != 0);
	_CY = 0;
	_OV = 0;

	return amLength1 + 1;
}

UINT32 opPUSH(void)
{
	modAdd=PC+1;
	modDim=2;

	amLength1=ReadAM();

	SP-=4;
	MemWrite32(SP,amOut);

	return amLength1 + 1;
}

UINT32 opPOP(void)
{
	modAdd=PC+1;
	modDim=2;
	modWriteValW=MemRead32(SP);
	SP+=4;
	amLength1=WriteAM();

	return amLength1 + 1;
}


UINT32 opINCB_0(void) { modM=0; return opINCB(); }
UINT32 opINCB_1(void) { modM=1; return opINCB(); }
UINT32 opINCH_0(void) { modM=0; return opINCH(); }
UINT32 opINCH_1(void) { modM=1; return opINCH(); }
UINT32 opINCW_0(void) { modM=0; return opINCW(); }
UINT32 opINCW_1(void) { modM=1; return opINCW(); }

UINT32 opDECB_0(void) { modM=0; return opDECB(); }
UINT32 opDECB_1(void) { modM=1; return opDECB(); }
UINT32 opDECH_0(void) { modM=0; return opDECH(); }
UINT32 opDECH_1(void) { modM=1; return opDECH(); }
UINT32 opDECW_0(void) { modM=0; return opDECW(); }
UINT32 opDECW_1(void) { modM=1; return opDECW(); }

UINT32 opJMP_0(void) { modM=0; return opJMP(); }
UINT32 opJMP_1(void) { modM=1; return opJMP(); }

UINT32 opJSR_0(void) { modM=0; return opJSR(); }
UINT32 opJSR_1(void) { modM=1; return opJSR(); }

UINT32 opPREPARE_0(void) { modM=0; return opPREPARE(); }
UINT32 opPREPARE_1(void) { modM=1; return opPREPARE(); }

UINT32 opRET_0(void) { modM=0; return opRET(); }
UINT32 opRET_1(void) { modM=1; return opRET(); }

UINT32 opTRAP_0(void) { modM=0; return opTRAP(); }
UINT32 opTRAP_1(void) { modM=1; return opTRAP(); }

UINT32 opRETIU_0(void) { modM=0; return opRETIU(); }
UINT32 opRETIU_1(void) { modM=1; return opRETIU(); }

UINT32 opRETIS_0(void) { modM=0; return opRETIS(); }
UINT32 opRETIS_1(void) { modM=1; return opRETIS(); }

UINT32 opGETPSW_0(void) { modM=0; return opGETPSW(); }
UINT32 opGETPSW_1(void) { modM=1; return opGETPSW(); }

UINT32 opTASI_0(void) { modM=0; return opTASI(); }
UINT32 opTASI_1(void) { modM=1; return opTASI(); }

UINT32 opCLRTLB_0(void) { modM=0; return opCLRTLB(); }
UINT32 opCLRTLB_1(void) { modM=1; return opCLRTLB(); }

UINT32 opPOPM_0(void) { modM=0; return opPOPM(); }
UINT32 opPOPM_1(void) { modM=1; return opPOPM(); }

UINT32 opPUSHM_0(void) { modM=0; return opPUSHM(); }
UINT32 opPUSHM_1(void) { modM=1; return opPUSHM(); }

UINT32 opTESTB_0(void) { modM=0; return opTESTB(); }
UINT32 opTESTB_1(void) { modM=1; return opTESTB(); }

UINT32 opTESTH_0(void) { modM=0; return opTESTH(); }
UINT32 opTESTH_1(void) { modM=1; return opTESTH(); }

UINT32 opTESTW_0(void) { modM=0; return opTESTW(); }
UINT32 opTESTW_1(void) { modM=1; return opTESTW(); }

UINT32 opPUSH_0(void) { modM=0; return opPUSH(); }
UINT32 opPUSH_1(void) { modM=1; return opPUSH(); }

UINT32 opPOP_0(void) { modM=0; return opPOP(); }
UINT32 opPOP_1(void) { modM=1; return opPOP(); }

UINT32 opSTTASK_0(void) { modM=0; return opSTTASK(); }
UINT32 opSTTASK_1(void) { modM=1; return opSTTASK(); }
