#ifdef CDEC1
case 0x01: /* abs */
  *op = 4 + sfao(st1);
  break;

case 0x02: /* neg */
  *op = 6 + sfao(st1);
  break;

case 0x03: /* add */
  *op = 8 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1);
  break;

case 0x04: /* add */
  *op = 16 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x05: /* add */
  *op = 20 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1) + 8*sfmo(st1) + 32*rnd(st1) + 160*movm(st1);
  break;

case 0x06: /* add */
  *op = 340 + xmode(opcode, 'c', cs) + 2*sfmo(st1) + 8*rnd(st1) + 40*movm(st1);
  break;

case 0x07: /* add */
  *op = 420 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x09: /* sub */
  *op = 428 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1);
  break;

case 0x0a: /* sub */
  *op = 436 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x0b: /* sub */
  *op = 440 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1) + 8*sfmo(st1) + 32*rnd(st1) + 160*movm(st1);
  break;

case 0x0c: /* sub */
  *op = 760 + xmode(opcode, 'c', cs) + 2*sfmo(st1) + 8*rnd(st1) + 40*movm(st1);
  break;

case 0x0d: /* sub */
  *op = 840 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x11: /* lacd */
  *op = 848 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1);
  break;

case 0x12: /* lacc */
  *op = 856 + xmode(opcode, 'c', cs);
  break;

case 0x14: /* and */
  *op = 858 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1);
  break;

case 0x15: /* and */
  *op = 866 + xmode(opcode, 'c', cs);
  break;

case 0x16: /* and */
  *op = 868 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*sfai(st1) + 8*dbp(st1);
  break;

case 0x17: /* or */
  *op = 884 + xmode(opcode, 'd', cs) + 2*sfai(st1) + 4*dbp(st1);
  break;

case 0x18: /* or */
  *op = 892 + xmode(opcode, 'c', cs);
  break;

case 0x19: /* or */
  *op = 894 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*sfai(st1) + 8*dbp(st1);
  break;

case 0x21: /* mpy */
  *op = 910 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x22: /* mpy */
  *op = 918 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x24: /* mac */
  *op = 922 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1) + 8*sfma(st1);
  break;

case 0x25: /* mac */
  *op = 954 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1) + 8*sfma(st1);
  break;

case 0x26: /* mac */
  *op = 986 + xmode(opcode, 'c', cs) + 2*sfao(st1) + 4*sfma(st1);
  break;

case 0x28: /* mpyu */
  *op = 1002 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x29: /* macu */
  *op = 1010 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1) + 8*sfma(st1);
  break;

case 0x2a: /* macu */
  *op = 1042 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1) + 8*sfma(st1);
  break;

case 0x2e: /* macs */
  *op = 1074 + xmode(opcode, 'c', cs) + 2*sfao(st1) + 4*sfma(st1);
  break;

case 0x31: /* lmhd */
  *op = 1090 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x32: /* lmld */
  *op = 1094 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x33: /* lmhc */
  *op = 1098 + xmode(opcode, 'c', cs);
  break;

case 0x34: /* sfml */
  *op = 1100;
  break;

case 0x35: /* sfmr */
  *op = 1101;
  break;

case 0x38: /* wre */
  *op = 1102 + xmode(opcode, 'c', cs) + 2*xmode(opcode, 'd', cs) + 4*dbp(st1);
  break;

case 0x39: /* rde */
  *op = 1110 + xmode(opcode, 'c', cs);
  break;

#endif

#ifdef CDEC2A
case 0x01: /* sacc */
  *op = 1112 + xmode(opcode, 'c', cs) + 2*sfao(st1);
  break;

case 0x02: /* sacd */
  *op = 1116 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfao(st1);
  break;

case 0x03: /* smhd */
  *op = 1124 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfmo(st1) + 16*rnd(st1) + 80*movm(st1);
  break;

case 0x05: /* smhc */
  *op = 1284 + xmode(opcode, 'c', cs) + 2*sfmo(st1) + 8*rnd(st1) + 40*movm(st1);
  break;

case 0x06: /* slmh */
  *op = 1364 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfmo(st1) + 16*movm(st1);
  break;

case 0x07: /* slml */
  *op = 1396 + xmode(opcode, 'd', cs) + 2*dbp(st1) + 4*sfmo(st1) + 16*movm(st1);
  break;

case 0x08: /* lcaa */
  *op = 1428 + sfao(st1);
  break;

case 0x09: /* lira */
  *op = 1430 + sfao(st1);
  break;

case 0x0e: /* ref */
  *op = 1432;
  break;

case 0x0f: /* srbd */
  *op = 1433 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x10: /* dis */
  *op = 1437 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x11: /* dis */
  *op = 1441 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x12: /* dis */
  *op = 1445 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x13: /* dis */
  *op = 1449 + xmode(opcode, 'd', cs) + 2*dbp(st1);
  break;

case 0x20: /* domh */
  *op = 1453 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x21: /* domh */
  *op = 1493 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x22: /* domh */
  *op = 1533 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x23: /* domh */
  *op = 1573 + sfmo(st1) + 4*rnd(st1) + 20*movm(st1);
  break;

case 0x31: /* lpc */
  *op = 1613 + xmode(opcode, 'c', cs);
  break;

case 0x3a: /* rmov */
  break;

case 0x3c: /* raom */
  break;

case 0x3d: /* saom */
  break;

case 0x40: /* rmom */
  break;

case 0x41: /* smom */
  break;

case 0x44: /* ldpk */
  break;

case 0x45: /* ldpk */
  break;

case 0x48: /* scrm */
  break;

case 0x49: /* scrm */
  break;

case 0x4a: /* scrm */
  break;

case 0x4b: /* scrm */
  break;

case 0x50: /* sfao */
  break;

case 0x51: /* sfao */
  break;

case 0x54: /* sfai */
  break;

case 0x55: /* sfai */
  break;

case 0x58: /* sfma */
  break;

case 0x59: /* sfma */
  break;

case 0x5a: /* sfma */
  break;

case 0x5b: /* sfma */
  break;

case 0x60: /* sfmo */
  break;

case 0x61: /* sfmo */
  break;

case 0x62: /* sfmo */
  break;

case 0x63: /* sfmo */
  break;

case 0x68: /* rnd */
  break;

case 0x69: /* rnd */
  break;

case 0x6a: /* rnd */
  break;

case 0x6b: /* rnd */
  break;

case 0x6c: /* rnd */
  break;

case 0x6d: /* rnd */
  break;

case 0x6e: /* rnd */
  break;

case 0x6f: /* rnd */
  break;

#endif

#ifdef CDEC2B
case 0x01: /* sacc */
  break;

case 0x02: /* sacd */
  break;

case 0x03: /* smhd */
  break;

case 0x05: /* smhc */
  break;

case 0x06: /* slmh */
  break;

case 0x07: /* slml */
  break;

case 0x08: /* lcaa */
  break;

case 0x09: /* lira */
  break;

case 0x0e: /* ref */
  break;

case 0x0f: /* srbd */
  break;

case 0x10: /* dis */
  break;

case 0x11: /* dis */
  break;

case 0x12: /* dis */
  break;

case 0x13: /* dis */
  break;

case 0x20: /* domh */
  break;

case 0x21: /* domh */
  break;

case 0x22: /* domh */
  break;

case 0x23: /* domh */
  break;

case 0x31: /* lpc */
  break;

case 0x3a: /* rmov */
  *op = 1615;
  break;

case 0x3c: /* raom */
  *op = 1616;
	/* Undocumented instruction, reset ALU saturation flag */
	st1 &= ~ST1_AOVM;
  break;

case 0x3d: /* saom */
  *op = 1617;
	/* Undocumented instruction, sets ALU saturation flag */
	st1 |= ST1_AOVM;
  break;

case 0x40: /* rmom */
  *op = 1618;
	st1 &= ~ST1_MOVM;
  break;

case 0x41: /* smom */
  *op = 1619;
	st1 |= ST1_MOVM;
  break;

case 0x44: /* ldpk */
  *op = 1620;
	st1 &= ~ST1_DBP;
  break;

case 0x45: /* ldpk */
  *op = 1621;
	st1 |= ST1_DBP;
  break;

case 0x48: /* scrm */
  *op = 1622;
	st1 = (st1 & ~ST1_CRM) | (0 << ST1_CRM_SHIFT);
  break;

case 0x49: /* scrm */
  *op = 1623;
	st1 = (st1 & ~ST1_CRM) | (1 << ST1_CRM_SHIFT);
  break;

case 0x4a: /* scrm */
  *op = 1624;
	st1 = (st1 & ~ST1_CRM) | (2 << ST1_CRM_SHIFT);
  break;

case 0x4b: /* scrm */
  *op = 1625;
	st1 = (st1 & ~ST1_CRM) | (3 << ST1_CRM_SHIFT);
  break;

case 0x50: /* sfao */
  *op = 1626;
	st1 &= ~ST1_SFAO;
  break;

case 0x51: /* sfao */
  *op = 1627;
	st1 |= ST1_SFAO;
  break;

case 0x54: /* sfai */
  *op = 1628;
	st1 &= ~ST1_SFAI;
  break;

case 0x55: /* sfai */
  *op = 1629;
	st1 |= ST1_SFAI;
  break;

case 0x58: /* sfma */
  *op = 1630;
	st1 = (st1 & ~ST1_SFMA) | (0 << ST1_SFMA_SHIFT);
  break;

case 0x59: /* sfma */
  *op = 1631;
	st1 = (st1 & ~ST1_SFMA) | (1 << ST1_SFMA_SHIFT);
  break;

case 0x5a: /* sfma */
  *op = 1632;
	st1 = (st1 & ~ST1_SFMA) | (2 << ST1_SFMA_SHIFT);
  break;

case 0x5b: /* sfma */
  *op = 1633;
	st1 = (st1 & ~ST1_SFMA) | (3 << ST1_SFMA_SHIFT);
  break;

case 0x60: /* sfmo */
  *op = 1634;
	st1 = (st1 & ~ST1_SFMO) | (0 << ST1_SFMO_SHIFT);
  break;

case 0x61: /* sfmo */
  *op = 1635;
	st1 = (st1 & ~ST1_SFMO) | (1 << ST1_SFMO_SHIFT);
  break;

case 0x62: /* sfmo */
  *op = 1636;
	st1 = (st1 & ~ST1_SFMO) | (2 << ST1_SFMO_SHIFT);
  break;

case 0x63: /* sfmo */
  *op = 1637;
	st1 = (st1 & ~ST1_SFMO) | (3 << ST1_SFMO_SHIFT);
  break;

case 0x68: /* rnd */
  *op = 1638;
	st1 = (st1 & ~ST1_RND) | (0 << ST1_RND_SHIFT);
  break;

case 0x69: /* rnd */
  *op = 1639;
	st1 = (st1 & ~ST1_RND) | (1 << ST1_RND_SHIFT);
  break;

case 0x6a: /* rnd */
  *op = 1640;
	st1 = (st1 & ~ST1_RND) | (2 << ST1_RND_SHIFT);
  break;

case 0x6b: /* rnd */
  *op = 1641;
	st1 = (st1 & ~ST1_RND) | (3 << ST1_RND_SHIFT);
  break;

case 0x6c: /* rnd */
  *op = 1642;
	st1 = (st1 & ~ST1_RND) | (4 << ST1_RND_SHIFT);
  break;

case 0x6d: /* rnd */
  *op = 1643;
	st1 = (st1 & ~ST1_RND) | (5 << ST1_RND_SHIFT);
  break;

case 0x6e: /* rnd */
  *op = 1644;
	st1 = (st1 & ~ST1_RND) | (6 << ST1_RND_SHIFT);
  break;

case 0x6f: /* rnd */
  *op = 1645;
	st1 = (st1 & ~ST1_RND) | (7 << ST1_RND_SHIFT);
  break;

#endif

#ifdef CDEC3
case 0x08: /* idle */
  *op = 1646;
  cs->branch = BR_IDLE;
  break;

case 0x10: /* rptk */
  *op = 1647;
  break;

case 0x18: /* lcak */
  *op = 1648;
  break;

case 0x20: /* lirk */
  *op = 1649;
  break;

case 0x40: /* lcac */
  *op = 1650;
  break;

case 0x48: /* b */
  *op = 1651;
  cs->branch = BR_UB;
  break;

case 0x50: /* bgz */
  *op = 1652;
  cs->branch = BR_CB;
  break;

case 0x58: /* blz */
  *op = 1653;
  cs->branch = BR_CB;
  break;

case 0x60: /* bnz */
  *op = 1654;
  cs->branch = BR_CB;
  break;

case 0x78: /* bv */
  *op = 1655;
  cs->branch = BR_CB;
  break;

#endif

#ifdef CINTRP
static void ex_4(const icd *i) /* abs sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(((INT32)aacc) < 0) {
	aacc = - aacc;
	if(((INT32)aacc) < 0)
		st1 |= ST1_AOV;
	}
}

static void ex_5(const icd *i) /* abs sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc = (aacc << 7);
	if(((INT32)aacc) < 0) {
	aacc = - aacc;
	if(((INT32)aacc) < 0)
		st1 |= ST1_AOV;
	}
}

static void ex_6(const icd *i) /* neg sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = -(INT64)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_7(const icd *i) /* neg sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = -(INT64)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_8(const icd *i) /* add dmode=0 dbp=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) + (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_9(const icd *i) /* add dmode=1 dbp=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) + (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_10(const icd *i) /* add dmode=0 dbp=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) + (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_11(const icd *i) /* add dmode=1 dbp=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) + (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_12(const icd *i) /* add dmode=0 dbp=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) + (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_13(const icd *i) /* add dmode=1 dbp=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) + (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_14(const icd *i) /* add dmode=0 dbp=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) + (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_15(const icd *i) /* add dmode=1 dbp=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) + (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_16(const icd *i) /* add cmode=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_17(const icd *i) /* add cmode=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_18(const icd *i) /* add cmode=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_19(const icd *i) /* add cmode=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_20(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_21(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_22(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_23(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_24(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_25(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_26(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_27(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_28(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_29(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_30(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_31(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_32(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_33(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_34(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_35(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_36(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_37(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_38(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_39(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_40(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_41(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_42(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_43(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_44(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_45(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_46(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_47(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_48(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_49(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_50(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_51(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_52(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_53(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_54(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_55(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_56(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_57(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_58(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_59(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_60(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_61(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_62(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_63(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_64(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_65(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_66(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_67(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_68(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_69(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_70(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_71(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_72(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_73(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_74(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_75(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_76(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_77(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_78(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_79(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_80(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_81(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_82(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_83(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_84(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_85(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_86(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_87(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_88(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_89(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_90(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_91(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_92(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_93(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_94(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_95(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_96(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_97(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_98(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_99(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_100(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_101(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_102(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_103(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_104(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_105(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_106(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_107(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_108(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_109(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_110(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_111(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_112(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_113(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_114(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_115(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_116(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_117(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_118(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_119(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_120(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_121(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_122(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_123(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_124(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_125(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_126(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_127(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_128(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_129(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_130(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_131(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_132(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_133(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_134(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_135(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_136(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_137(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_138(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_139(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_140(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_141(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_142(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_143(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_144(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_145(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_146(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_147(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_148(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_149(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_150(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_151(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_152(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_153(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_154(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_155(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_156(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_157(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_158(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_159(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_160(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_161(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_162(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_163(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_164(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_165(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_166(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_167(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_168(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_169(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_170(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_171(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_172(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_173(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_174(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_175(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_176(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_177(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_178(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_179(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_180(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_181(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_182(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_183(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_184(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_185(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_186(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_187(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_188(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_189(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_190(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_191(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_192(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_193(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_194(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_195(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_196(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_197(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_198(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_199(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_200(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_201(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_202(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_203(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_204(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_205(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_206(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_207(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_208(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_209(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_210(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_211(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_212(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_213(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_214(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_215(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_216(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_217(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_218(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_219(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_220(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_221(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_222(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_223(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_224(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_225(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_226(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_227(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_228(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_229(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_230(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_231(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_232(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_233(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_234(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_235(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_236(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_237(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_238(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_239(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_240(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_241(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_242(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_243(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_244(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_245(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_246(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_247(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_248(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_249(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_250(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_251(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_252(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_253(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_254(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_255(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_256(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_257(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_258(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_259(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_260(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_261(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_262(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_263(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_264(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_265(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_266(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_267(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_268(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_269(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_270(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_271(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_272(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_273(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_274(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_275(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_276(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_277(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_278(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_279(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_280(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_281(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_282(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_283(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_284(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_285(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_286(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_287(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_288(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_289(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_290(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_291(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_292(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_293(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_294(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_295(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_296(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_297(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_298(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_299(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_300(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_301(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_302(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_303(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_304(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_305(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_306(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_307(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_308(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_309(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_310(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_311(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_312(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_313(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_314(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_315(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_316(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_317(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_318(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_319(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_320(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_321(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_322(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_323(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_324(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_325(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_326(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_327(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_328(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_329(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_330(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_331(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_332(const icd *i) /* add dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_333(const icd *i) /* add dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_334(const icd *i) /* add dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_335(const icd *i) /* add dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_336(const icd *i) /* add dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_337(const icd *i) /* add dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_338(const icd *i) /* add dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_339(const icd *i) /* add dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_340(const icd *i) /* add cmode=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_341(const icd *i) /* add cmode=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_342(const icd *i) /* add cmode=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_343(const icd *i) /* add cmode=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_344(const icd *i) /* add cmode=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_345(const icd *i) /* add cmode=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_346(const icd *i) /* add cmode=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_347(const icd *i) /* add cmode=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_348(const icd *i) /* add cmode=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_349(const icd *i) /* add cmode=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_350(const icd *i) /* add cmode=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_351(const icd *i) /* add cmode=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_352(const icd *i) /* add cmode=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_353(const icd *i) /* add cmode=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_354(const icd *i) /* add cmode=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_355(const icd *i) /* add cmode=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_356(const icd *i) /* add cmode=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_357(const icd *i) /* add cmode=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_358(const icd *i) /* add cmode=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_359(const icd *i) /* add cmode=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_360(const icd *i) /* add cmode=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_361(const icd *i) /* add cmode=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_362(const icd *i) /* add cmode=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_363(const icd *i) /* add cmode=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_364(const icd *i) /* add cmode=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_365(const icd *i) /* add cmode=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_366(const icd *i) /* add cmode=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_367(const icd *i) /* add cmode=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_368(const icd *i) /* add cmode=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_369(const icd *i) /* add cmode=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_370(const icd *i) /* add cmode=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_371(const icd *i) /* add cmode=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_372(const icd *i) /* add cmode=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_373(const icd *i) /* add cmode=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_374(const icd *i) /* add cmode=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_375(const icd *i) /* add cmode=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_376(const icd *i) /* add cmode=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_377(const icd *i) /* add cmode=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_378(const icd *i) /* add cmode=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_379(const icd *i) /* add cmode=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_380(const icd *i) /* add cmode=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_381(const icd *i) /* add cmode=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_382(const icd *i) /* add cmode=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_383(const icd *i) /* add cmode=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_384(const icd *i) /* add cmode=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_385(const icd *i) /* add cmode=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_386(const icd *i) /* add cmode=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_387(const icd *i) /* add cmode=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_388(const icd *i) /* add cmode=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_389(const icd *i) /* add cmode=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_390(const icd *i) /* add cmode=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_391(const icd *i) /* add cmode=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_392(const icd *i) /* add cmode=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_393(const icd *i) /* add cmode=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_394(const icd *i) /* add cmode=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_395(const icd *i) /* add cmode=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_396(const icd *i) /* add cmode=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_397(const icd *i) /* add cmode=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_398(const icd *i) /* add cmode=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_399(const icd *i) /* add cmode=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_400(const icd *i) /* add cmode=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_401(const icd *i) /* add cmode=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_402(const icd *i) /* add cmode=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_403(const icd *i) /* add cmode=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_404(const icd *i) /* add cmode=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_405(const icd *i) /* add cmode=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_406(const icd *i) /* add cmode=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_407(const icd *i) /* add cmode=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_408(const icd *i) /* add cmode=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_409(const icd *i) /* add cmode=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_410(const icd *i) /* add cmode=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_411(const icd *i) /* add cmode=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_412(const icd *i) /* add cmode=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_413(const icd *i) /* add cmode=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_414(const icd *i) /* add cmode=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_415(const icd *i) /* add cmode=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_416(const icd *i) /* add cmode=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_417(const icd *i) /* add cmode=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_418(const icd *i) /* add cmode=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_419(const icd *i) /* add cmode=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) + (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_420(const icd *i) /* add cmode=0 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) + (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_421(const icd *i) /* add cmode=1 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) + (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_422(const icd *i) /* add cmode=0 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) + (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_423(const icd *i) /* add cmode=1 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) + (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_424(const icd *i) /* add cmode=0 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) + (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_425(const icd *i) /* add cmode=1 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) + (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_426(const icd *i) /* add cmode=0 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) + (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_427(const icd *i) /* add cmode=1 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) + (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_428(const icd *i) /* sub dmode=0 dbp=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) - (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_429(const icd *i) /* sub dmode=1 dbp=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) - (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_430(const icd *i) /* sub dmode=0 dbp=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) - (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_431(const icd *i) /* sub dmode=1 dbp=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) - (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_432(const icd *i) /* sub dmode=0 dbp=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) - (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_433(const icd *i) /* sub dmode=1 dbp=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) - (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_434(const icd *i) /* sub dmode=0 dbp=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) - (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_435(const icd *i) /* sub dmode=1 dbp=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) - (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_436(const icd *i) /* sub cmode=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_437(const icd *i) /* sub cmode=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (INT64)(INT32)aacc;
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_438(const icd *i) /* sub cmode=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_439(const icd *i) /* sub cmode=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (INT64)(INT32)(aacc << 7);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_440(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_441(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_442(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_443(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_444(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_445(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_446(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_447(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_448(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_449(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_450(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_451(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_452(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_453(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_454(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_455(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_456(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_457(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_458(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_459(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_460(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_461(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_462(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_463(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_464(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_465(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_466(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_467(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_468(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_469(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_470(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_471(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_472(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_473(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_474(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_475(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_476(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_477(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_478(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_479(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_480(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_481(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_482(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_483(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_484(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_485(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_486(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_487(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_488(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_489(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_490(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_491(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_492(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_493(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_494(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_495(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_496(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_497(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_498(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_499(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_500(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_501(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_502(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_503(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_504(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_505(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_506(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_507(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_508(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_509(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_510(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_511(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_512(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_513(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_514(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_515(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_516(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_517(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_518(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_519(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_520(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_521(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_522(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_523(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_524(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_525(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_526(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_527(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_528(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_529(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_530(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_531(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_532(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_533(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_534(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_535(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_536(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_537(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_538(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_539(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_540(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_541(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_542(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_543(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_544(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_545(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_546(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_547(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_548(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_549(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_550(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_551(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_552(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_553(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_554(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_555(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_556(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_557(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_558(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_559(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_560(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_561(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_562(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_563(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_564(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_565(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_566(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_567(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_568(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_569(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_570(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_571(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_572(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_573(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_574(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_575(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_576(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_577(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_578(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_579(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_580(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_581(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_582(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_583(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_584(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_585(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_586(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_587(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_588(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_589(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_590(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_591(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_592(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_593(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_594(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_595(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_596(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_597(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_598(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_599(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_600(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_601(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_602(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_603(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_604(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_605(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_606(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_607(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_608(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_609(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_610(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_611(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_612(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_613(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_614(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_615(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_616(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_617(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_618(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_619(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_620(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_621(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_622(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_623(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_624(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_625(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_626(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_627(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_628(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_629(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_630(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_631(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_632(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_633(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_634(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_635(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_636(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_637(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_638(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_639(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_640(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_641(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_642(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_643(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_644(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_645(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_646(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_647(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_648(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_649(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_650(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_651(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_652(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_653(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_654(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_655(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_656(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_657(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_658(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_659(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_660(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_661(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_662(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_663(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_664(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_665(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_666(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_667(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_668(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_669(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_670(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_671(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_672(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_673(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_674(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_675(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_676(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_677(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_678(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_679(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_680(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_681(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_682(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_683(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_684(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_685(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_686(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_687(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_688(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_689(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_690(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_691(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_692(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_693(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_694(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_695(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_696(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_697(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_698(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_699(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_700(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_701(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_702(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_703(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_704(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_705(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_706(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_707(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_708(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_709(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_710(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_711(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_712(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_713(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_714(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_715(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_716(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_717(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_718(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_719(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_720(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_721(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_722(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_723(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_724(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_725(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_726(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_727(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_728(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_729(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_730(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_731(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_732(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_733(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_734(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_735(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_736(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_737(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_738(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_739(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_740(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_741(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_742(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_743(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_744(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_745(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_746(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_747(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_748(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_749(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_750(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_751(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_752(const icd *i) /* sub dmode=0 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_753(const icd *i) /* sub dmode=1 sfai=0 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_754(const icd *i) /* sub dmode=0 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_755(const icd *i) /* sub dmode=1 sfai=1 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_756(const icd *i) /* sub dmode=0 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_757(const icd *i) /* sub dmode=1 sfai=0 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_758(const icd *i) /* sub dmode=0 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_759(const icd *i) /* sub dmode=1 sfai=1 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	r = (INT64)(INT32)d - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_760(const icd *i) /* sub cmode=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_761(const icd *i) /* sub cmode=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_762(const icd *i) /* sub cmode=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_763(const icd *i) /* sub cmode=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_764(const icd *i) /* sub cmode=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_765(const icd *i) /* sub cmode=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_766(const icd *i) /* sub cmode=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_767(const icd *i) /* sub cmode=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_768(const icd *i) /* sub cmode=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_769(const icd *i) /* sub cmode=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_770(const icd *i) /* sub cmode=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_771(const icd *i) /* sub cmode=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_772(const icd *i) /* sub cmode=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_773(const icd *i) /* sub cmode=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_774(const icd *i) /* sub cmode=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_775(const icd *i) /* sub cmode=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_776(const icd *i) /* sub cmode=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_777(const icd *i) /* sub cmode=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_778(const icd *i) /* sub cmode=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_779(const icd *i) /* sub cmode=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_780(const icd *i) /* sub cmode=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_781(const icd *i) /* sub cmode=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_782(const icd *i) /* sub cmode=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_783(const icd *i) /* sub cmode=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_784(const icd *i) /* sub cmode=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_785(const icd *i) /* sub cmode=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_786(const icd *i) /* sub cmode=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_787(const icd *i) /* sub cmode=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_788(const icd *i) /* sub cmode=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_789(const icd *i) /* sub cmode=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_790(const icd *i) /* sub cmode=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_791(const icd *i) /* sub cmode=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_792(const icd *i) /* sub cmode=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_793(const icd *i) /* sub cmode=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_794(const icd *i) /* sub cmode=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_795(const icd *i) /* sub cmode=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_796(const icd *i) /* sub cmode=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_797(const icd *i) /* sub cmode=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_798(const icd *i) /* sub cmode=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_799(const icd *i) /* sub cmode=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_800(const icd *i) /* sub cmode=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_801(const icd *i) /* sub cmode=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_802(const icd *i) /* sub cmode=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_803(const icd *i) /* sub cmode=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_804(const icd *i) /* sub cmode=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_805(const icd *i) /* sub cmode=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_806(const icd *i) /* sub cmode=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_807(const icd *i) /* sub cmode=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_808(const icd *i) /* sub cmode=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_809(const icd *i) /* sub cmode=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_810(const icd *i) /* sub cmode=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_811(const icd *i) /* sub cmode=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_812(const icd *i) /* sub cmode=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_813(const icd *i) /* sub cmode=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_814(const icd *i) /* sub cmode=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_815(const icd *i) /* sub cmode=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_816(const icd *i) /* sub cmode=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_817(const icd *i) /* sub cmode=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_818(const icd *i) /* sub cmode=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_819(const icd *i) /* sub cmode=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_820(const icd *i) /* sub cmode=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_821(const icd *i) /* sub cmode=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_822(const icd *i) /* sub cmode=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_823(const icd *i) /* sub cmode=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_824(const icd *i) /* sub cmode=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_825(const icd *i) /* sub cmode=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_826(const icd *i) /* sub cmode=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_827(const icd *i) /* sub cmode=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_828(const icd *i) /* sub cmode=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_829(const icd *i) /* sub cmode=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_830(const icd *i) /* sub cmode=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_831(const icd *i) /* sub cmode=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_832(const icd *i) /* sub cmode=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_833(const icd *i) /* sub cmode=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_834(const icd *i) /* sub cmode=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_835(const icd *i) /* sub cmode=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_836(const icd *i) /* sub cmode=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_837(const icd *i) /* sub cmode=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_838(const icd *i) /* sub cmode=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(i->param) - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_839(const icd *i) /* sub cmode=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)get_cmem(ca) - (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_840(const icd *i) /* sub cmode=0 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) - (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_841(const icd *i) /* sub cmode=1 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8) - (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_842(const icd *i) /* sub cmode=0 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) - (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_843(const icd *i) /* sub cmode=1 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8) - (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_844(const icd *i) /* sub cmode=0 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) - (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_845(const icd *i) /* sub cmode=1 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8) - (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_846(const icd *i) /* sub cmode=0 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) - (INT64)(INT32)get_cmem(i->param);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_847(const icd *i) /* sub cmode=1 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	r = (INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8) - (INT64)(INT32)get_cmem(ca);
  if(r < TMS_MIN32 || r > 2147483647) {
    st1 |= ST1_AOV;
    if(st1 & ST1_AOVM) r = TMS_CLAMP32(r);
  }  aacc = r;
}

static void ex_848(const icd *i) /* lacd dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = d;
}

static void ex_849(const icd *i) /* lacd dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = d;
}

static void ex_850(const icd *i) /* lacd dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = d;
}

static void ex_851(const icd *i) /* lacd dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = d;
}

static void ex_852(const icd *i) /* lacd dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = d;
}

static void ex_853(const icd *i) /* lacd dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = d;
}

static void ex_854(const icd *i) /* lacd dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = d;
}

static void ex_855(const icd *i) /* lacd dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = d;
}

static void ex_856(const icd *i) /* lacc cmode=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc = get_cmem(i->param);
}

static void ex_857(const icd *i) /* lacc cmode=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc = get_cmem(ca);
}

static void ex_858(const icd *i) /* and dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc &= d;
}

static void ex_859(const icd *i) /* and dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc &= d;
}

static void ex_860(const icd *i) /* and dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc &= d;
}

static void ex_861(const icd *i) /* and dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc &= d;
}

static void ex_862(const icd *i) /* and dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc &= d;
}

static void ex_863(const icd *i) /* and dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc &= d;
}

static void ex_864(const icd *i) /* and dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc &= d;
}

static void ex_865(const icd *i) /* and dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc &= d;
}

static void ex_866(const icd *i) /* and cmode=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc &= get_cmem(i->param);
}

static void ex_867(const icd *i) /* and cmode=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc &= get_cmem(ca);
}

static void ex_868(const icd *i) /* and cmode=0 dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) & d;
}

static void ex_869(const icd *i) /* and cmode=1 dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) & d;
}

static void ex_870(const icd *i) /* and cmode=0 dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) & d;
}

static void ex_871(const icd *i) /* and cmode=1 dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) & d;
}

static void ex_872(const icd *i) /* and cmode=0 dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
}

static void ex_873(const icd *i) /* and cmode=1 dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
}

static void ex_874(const icd *i) /* and cmode=0 dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
}

static void ex_875(const icd *i) /* and cmode=1 dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
}

static void ex_876(const icd *i) /* and cmode=0 dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) & d;
}

static void ex_877(const icd *i) /* and cmode=1 dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) & d;
}

static void ex_878(const icd *i) /* and cmode=0 dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) & d;
}

static void ex_879(const icd *i) /* and cmode=1 dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) & d;
}

static void ex_880(const icd *i) /* and cmode=0 dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
}

static void ex_881(const icd *i) /* and cmode=1 dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
}

static void ex_882(const icd *i) /* and cmode=0 dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) & d;
}

static void ex_883(const icd *i) /* and cmode=1 dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) & d;
}

static void ex_884(const icd *i) /* or dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc |= d;
}

static void ex_885(const icd *i) /* or dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc |= d;
}

static void ex_886(const icd *i) /* or dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc |= d;
}

static void ex_887(const icd *i) /* or dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc |= d;
}

static void ex_888(const icd *i) /* or dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc |= d;
}

static void ex_889(const icd *i) /* or dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc |= d;
}

static void ex_890(const icd *i) /* or dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc |= d;
}

static void ex_891(const icd *i) /* or dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc |= d;
}

static void ex_892(const icd *i) /* or cmode=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc |= get_cmem(i->param);
}

static void ex_893(const icd *i) /* or cmode=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	aacc |= get_cmem(ca);
}

static void ex_894(const icd *i) /* or cmode=0 dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) | d;
}

static void ex_895(const icd *i) /* or cmode=1 dmode=0 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(i->param + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) | d;
}

static void ex_896(const icd *i) /* or cmode=0 dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(i->param) | d;
}

static void ex_897(const icd *i) /* or cmode=1 dmode=1 sfai=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem0[(id + ba0) & 0xff] << 8);
	aacc = get_cmem(ca) | d;
}

static void ex_898(const icd *i) /* or cmode=0 dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
}

static void ex_899(const icd *i) /* or cmode=1 dmode=0 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(i->param + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
}

static void ex_900(const icd *i) /* or cmode=0 dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
}

static void ex_901(const icd *i) /* or cmode=1 dmode=1 sfai=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem0[(id + ba0) & 0xff] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
}

static void ex_902(const icd *i) /* or cmode=0 dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) | d;
}

static void ex_903(const icd *i) /* or cmode=1 dmode=0 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(i->param + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) | d;
}

static void ex_904(const icd *i) /* or cmode=0 dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(i->param) | d;
}

static void ex_905(const icd *i) /* or cmode=1 dmode=1 sfai=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d =  (dmem1[(id + ba1) & 0x1f] << 8);
	aacc = get_cmem(ca) | d;
}

static void ex_906(const icd *i) /* or cmode=0 dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
}

static void ex_907(const icd *i) /* or cmode=1 dmode=0 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(i->param + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
}

static void ex_908(const icd *i) /* or cmode=0 dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(i->param) | d;
}

static void ex_909(const icd *i) /* or cmode=1 dmode=1 sfai=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = ((INT32)( (dmem1[(id + ba1) & 0x1f] << 8))) >> 1;
	aacc = get_cmem(ca) | d;
}

static void ex_910(const icd *i) /* mpy cmode=0 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_911(const icd *i) /* mpy cmode=1 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_912(const icd *i) /* mpy cmode=0 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_913(const icd *i) /* mpy cmode=1 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_914(const icd *i) /* mpy cmode=0 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_915(const icd *i) /* mpy cmode=1 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_916(const icd *i) /* mpy cmode=0 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_917(const icd *i) /* mpy cmode=1 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = r >> 7;
}

static void ex_918(const icd *i) /* mpy cmode=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = r >> 15;
}

static void ex_919(const icd *i) /* mpy cmode=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = r >> 15;
}

static void ex_920(const icd *i) /* mpy cmode=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = r >> 15;
}

static void ex_921(const icd *i) /* mpy cmode=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = r >> 15;
}

static void ex_922(const icd *i) /* mac cmode=0 dmode=0 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_923(const icd *i) /* mac cmode=1 dmode=0 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_924(const icd *i) /* mac cmode=0 dmode=1 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_925(const icd *i) /* mac cmode=1 dmode=1 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_926(const icd *i) /* mac cmode=0 dmode=0 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_927(const icd *i) /* mac cmode=1 dmode=0 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_928(const icd *i) /* mac cmode=0 dmode=1 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_929(const icd *i) /* mac cmode=1 dmode=1 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_930(const icd *i) /* mac cmode=0 dmode=0 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_931(const icd *i) /* mac cmode=1 dmode=0 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_932(const icd *i) /* mac cmode=0 dmode=1 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_933(const icd *i) /* mac cmode=1 dmode=1 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_934(const icd *i) /* mac cmode=0 dmode=0 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_935(const icd *i) /* mac cmode=1 dmode=0 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_936(const icd *i) /* mac cmode=0 dmode=1 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_937(const icd *i) /* mac cmode=1 dmode=1 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_938(const icd *i) /* mac cmode=0 dmode=0 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_939(const icd *i) /* mac cmode=1 dmode=0 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_940(const icd *i) /* mac cmode=0 dmode=1 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_941(const icd *i) /* mac cmode=1 dmode=1 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_942(const icd *i) /* mac cmode=0 dmode=0 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_943(const icd *i) /* mac cmode=1 dmode=0 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_944(const icd *i) /* mac cmode=0 dmode=1 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_945(const icd *i) /* mac cmode=1 dmode=1 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_946(const icd *i) /* mac cmode=0 dmode=0 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_947(const icd *i) /* mac cmode=1 dmode=0 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_948(const icd *i) /* mac cmode=0 dmode=1 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_949(const icd *i) /* mac cmode=1 dmode=1 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_950(const icd *i) /* mac cmode=0 dmode=0 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_951(const icd *i) /* mac cmode=1 dmode=0 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_952(const icd *i) /* mac cmode=0 dmode=1 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_953(const icd *i) /* mac cmode=1 dmode=1 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_954(const icd *i) /* mac dmode=0 dbp=0 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_955(const icd *i) /* mac dmode=1 dbp=0 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_956(const icd *i) /* mac dmode=0 dbp=1 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_957(const icd *i) /* mac dmode=1 dbp=1 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_958(const icd *i) /* mac dmode=0 dbp=0 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_959(const icd *i) /* mac dmode=1 dbp=0 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_960(const icd *i) /* mac dmode=0 dbp=1 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_961(const icd *i) /* mac dmode=1 dbp=1 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = macc + (r >> 7);
}

static void ex_962(const icd *i) /* mac dmode=0 dbp=0 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_963(const icd *i) /* mac dmode=1 dbp=0 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_964(const icd *i) /* mac dmode=0 dbp=1 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_965(const icd *i) /* mac dmode=1 dbp=1 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_966(const icd *i) /* mac dmode=0 dbp=0 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_967(const icd *i) /* mac dmode=1 dbp=0 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_968(const icd *i) /* mac dmode=0 dbp=1 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_969(const icd *i) /* mac dmode=1 dbp=1 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_970(const icd *i) /* mac dmode=0 dbp=0 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_971(const icd *i) /* mac dmode=1 dbp=0 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_972(const icd *i) /* mac dmode=0 dbp=1 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_973(const icd *i) /* mac dmode=1 dbp=1 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_974(const icd *i) /* mac dmode=0 dbp=0 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_975(const icd *i) /* mac dmode=1 dbp=0 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_976(const icd *i) /* mac dmode=0 dbp=1 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_977(const icd *i) /* mac dmode=1 dbp=1 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_978(const icd *i) /* mac dmode=0 dbp=0 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_979(const icd *i) /* mac dmode=1 dbp=0 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_980(const icd *i) /* mac dmode=0 dbp=1 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_981(const icd *i) /* mac dmode=1 dbp=1 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_982(const icd *i) /* mac dmode=0 dbp=0 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_983(const icd *i) /* mac dmode=1 dbp=0 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_984(const icd *i) /* mac dmode=0 dbp=1 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_985(const icd *i) /* mac dmode=1 dbp=1 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f];
	if(d & 0x00800000)
	d |= 0xff000000;
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)(INT32)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_986(const icd *i) /* mac cmode=0 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = macc + (r >> 15);
}

static void ex_987(const icd *i) /* mac cmode=1 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = macc + (r >> 15);
}

static void ex_988(const icd *i) /* mac cmode=0 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = macc + (r >> 15);
}

static void ex_989(const icd *i) /* mac cmode=1 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = macc + (r >> 15);
}

static void ex_990(const icd *i) /* mac cmode=0 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 2) + (r >> 15);
}

static void ex_991(const icd *i) /* mac cmode=1 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 2) + (r >> 15);
}

static void ex_992(const icd *i) /* mac cmode=0 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 2) + (r >> 15);
}

static void ex_993(const icd *i) /* mac cmode=1 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 2) + (r >> 15);
}

static void ex_994(const icd *i) /* mac cmode=0 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 4) + (r >> 15);
}

static void ex_995(const icd *i) /* mac cmode=1 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 4) + (r >> 15);
}

static void ex_996(const icd *i) /* mac cmode=0 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 4) + (r >> 15);
}

static void ex_997(const icd *i) /* mac cmode=1 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 4) + (r >> 15);
}

static void ex_998(const icd *i) /* mac cmode=0 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc >> 16) + (r >> 15);
}

static void ex_999(const icd *i) /* mac cmode=1 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc >> 16) + (r >> 15);
}

static void ex_1000(const icd *i) /* mac cmode=0 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc >> 16) + (r >> 15);
}

static void ex_1001(const icd *i) /* mac cmode=1 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc >> 16) + (r >> 15);
}

static void ex_1002(const icd *i) /* mpyu cmode=0 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1003(const icd *i) /* mpyu cmode=1 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1004(const icd *i) /* mpyu cmode=0 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1005(const icd *i) /* mpyu cmode=1 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1006(const icd *i) /* mpyu cmode=0 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1007(const icd *i) /* mpyu cmode=1 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1008(const icd *i) /* mpyu cmode=0 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1009(const icd *i) /* mpyu cmode=1 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	r = (INT64)(INT32)c * (INT64)d;
	macc = r >> 7;
}

static void ex_1010(const icd *i) /* macu cmode=0 dmode=0 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1011(const icd *i) /* macu cmode=1 dmode=0 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1012(const icd *i) /* macu cmode=0 dmode=1 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1013(const icd *i) /* macu cmode=1 dmode=1 dbp=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1014(const icd *i) /* macu cmode=0 dmode=0 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1015(const icd *i) /* macu cmode=1 dmode=0 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1016(const icd *i) /* macu cmode=0 dmode=1 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1017(const icd *i) /* macu cmode=1 dmode=1 dbp=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1018(const icd *i) /* macu cmode=0 dmode=0 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1019(const icd *i) /* macu cmode=1 dmode=0 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1020(const icd *i) /* macu cmode=0 dmode=1 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1021(const icd *i) /* macu cmode=1 dmode=1 dbp=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1022(const icd *i) /* macu cmode=0 dmode=0 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1023(const icd *i) /* macu cmode=1 dmode=0 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1024(const icd *i) /* macu cmode=0 dmode=1 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1025(const icd *i) /* macu cmode=1 dmode=1 dbp=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1026(const icd *i) /* macu cmode=0 dmode=0 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1027(const icd *i) /* macu cmode=1 dmode=0 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1028(const icd *i) /* macu cmode=0 dmode=1 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1029(const icd *i) /* macu cmode=1 dmode=1 dbp=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1030(const icd *i) /* macu cmode=0 dmode=0 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1031(const icd *i) /* macu cmode=1 dmode=0 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1032(const icd *i) /* macu cmode=0 dmode=1 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1033(const icd *i) /* macu cmode=1 dmode=1 dbp=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1034(const icd *i) /* macu cmode=0 dmode=0 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1035(const icd *i) /* macu cmode=1 dmode=0 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1036(const icd *i) /* macu cmode=0 dmode=1 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1037(const icd *i) /* macu cmode=1 dmode=1 dbp=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1038(const icd *i) /* macu cmode=0 dmode=0 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1039(const icd *i) /* macu cmode=1 dmode=0 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1040(const icd *i) /* macu cmode=0 dmode=1 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1041(const icd *i) /* macu cmode=1 dmode=1 dbp=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1042(const icd *i) /* macu dmode=0 dbp=0 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1043(const icd *i) /* macu dmode=1 dbp=0 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1044(const icd *i) /* macu dmode=0 dbp=1 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1045(const icd *i) /* macu dmode=1 dbp=1 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1046(const icd *i) /* macu dmode=0 dbp=0 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1047(const icd *i) /* macu dmode=1 dbp=0 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1048(const icd *i) /* macu dmode=0 dbp=1 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1049(const icd *i) /* macu dmode=1 dbp=1 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = macc + (r >> 7);
}

static void ex_1050(const icd *i) /* macu dmode=0 dbp=0 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1051(const icd *i) /* macu dmode=1 dbp=0 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1052(const icd *i) /* macu dmode=0 dbp=1 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1053(const icd *i) /* macu dmode=1 dbp=1 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1054(const icd *i) /* macu dmode=0 dbp=0 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1055(const icd *i) /* macu dmode=1 dbp=0 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1056(const icd *i) /* macu dmode=0 dbp=1 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1057(const icd *i) /* macu dmode=1 dbp=1 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 2) + (r >> 7);
}

static void ex_1058(const icd *i) /* macu dmode=0 dbp=0 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1059(const icd *i) /* macu dmode=1 dbp=0 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1060(const icd *i) /* macu dmode=0 dbp=1 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1061(const icd *i) /* macu dmode=1 dbp=1 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1062(const icd *i) /* macu dmode=0 dbp=0 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1063(const icd *i) /* macu dmode=1 dbp=0 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1064(const icd *i) /* macu dmode=0 dbp=1 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1065(const icd *i) /* macu dmode=1 dbp=1 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc << 4) + (r >> 7);
}

static void ex_1066(const icd *i) /* macu dmode=0 dbp=0 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1067(const icd *i) /* macu dmode=1 dbp=0 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1068(const icd *i) /* macu dmode=0 dbp=1 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1069(const icd *i) /* macu dmode=1 dbp=1 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = aacc;
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1070(const icd *i) /* macu dmode=0 dbp=0 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(i->param + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1071(const icd *i) /* macu dmode=1 dbp=0 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem0[(id + ba0) & 0xff]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1072(const icd *i) /* macu dmode=0 dbp=1 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(i->param + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1073(const icd *i) /* macu dmode=1 dbp=1 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	d = dmem1[(id + ba1) & 0x1f]; /* d is 24bits unsigned */
	creg = c = (aacc << 7);
	r = (INT64)(INT32)c * (INT64)d;
	macc = (macc >> 16) + (r >> 7);
}

static void ex_1074(const icd *i) /* macs cmode=0 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = macc + (r >> 14);
}

static void ex_1075(const icd *i) /* macs cmode=1 sfao=0 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = macc + (r >> 14);
}

static void ex_1076(const icd *i) /* macs cmode=0 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = macc + (r >> 14);
}

static void ex_1077(const icd *i) /* macs cmode=1 sfao=1 sfma=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = macc + (r >> 14);
}

static void ex_1078(const icd *i) /* macs cmode=0 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 2) + (r >> 14);
}

static void ex_1079(const icd *i) /* macs cmode=1 sfao=0 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 2) + (r >> 14);
}

static void ex_1080(const icd *i) /* macs cmode=0 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 2) + (r >> 14);
}

static void ex_1081(const icd *i) /* macs cmode=1 sfao=1 sfma=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 2) + (r >> 14);
}

static void ex_1082(const icd *i) /* macs cmode=0 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 4) + (r >> 14);
}

static void ex_1083(const icd *i) /* macs cmode=1 sfao=0 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc << 4) + (r >> 14);
}

static void ex_1084(const icd *i) /* macs cmode=0 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 4) + (r >> 14);
}

static void ex_1085(const icd *i) /* macs cmode=1 sfao=1 sfma=2 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc << 4) + (r >> 14);
}

static void ex_1086(const icd *i) /* macs cmode=0 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc >> 16) + (r >> 14);
}

static void ex_1087(const icd *i) /* macs cmode=1 sfao=0 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)aacc;
	macc = (macc >> 16) + (r >> 14);
}

static void ex_1088(const icd *i) /* macs cmode=0 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(i->param);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc >> 16) + (r >> 14);
}

static void ex_1089(const icd *i) /* macs cmode=1 sfao=1 sfma=3 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	creg = c = get_cmem(ca);
	r = (INT64)(INT32)c * (INT64)(INT32)(aacc << 7);
	macc = (macc >> 16) + (r >> 14);
}

static void ex_1090(const icd *i) /* lmhd dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = ((INT64)(INT32)(dmem0[(i->param + ba0) & 0xff] << 8)) << 16;
}

static void ex_1091(const icd *i) /* lmhd dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = ((INT64)(INT32)(dmem0[(id + ba0) & 0xff] << 8)) << 16;
}

static void ex_1092(const icd *i) /* lmhd dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = ((INT64)(INT32)(dmem1[(i->param + ba1) & 0x1f] << 8)) << 16;
}

static void ex_1093(const icd *i) /* lmhd dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = ((INT64)(INT32)(dmem1[(id + ba1) & 0x1f] << 8)) << 16;
}

static void ex_1094(const icd *i) /* lmld dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = (macc & ~TMS_U64(0x00000000, 0x00ffffff)) | dmem0[(i->param + ba0) & 0xff];
}

static void ex_1095(const icd *i) /* lmld dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = (macc & ~TMS_U64(0x00000000, 0x00ffffff)) | dmem0[(id + ba0) & 0xff];
}

static void ex_1096(const icd *i) /* lmld dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = (macc & ~TMS_U64(0x00000000, 0x00ffffff)) | dmem1[(i->param + ba1) & 0x1f];
}

static void ex_1097(const icd *i) /* lmld dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = (macc & ~TMS_U64(0x00000000, 0x00ffffff)) | dmem1[(id + ba1) & 0x1f];
}

static void ex_1098(const icd *i) /* lmhc cmode=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = ((INT64)(INT32)get_cmem(i->param)) << 16;
}

static void ex_1099(const icd *i) /* lmhc cmode=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc_write = macc = ((INT64)(INT32)get_cmem(ca)) << 16;
}

static void ex_1100(const icd *i) /* sfml  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc = (macc & TMS_U64(0x00080000, 0x00000000)) | ((macc << 1) & TMS_U64(0x0007ffff, 0xffffffff));
}

static void ex_1101(const icd *i) /* sfmr  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	macc = (macc & TMS_U64(0x00080000, 0x00000000)) | ((macc >> 1) & TMS_U64(0x0007ffff, 0xffffffff));
}

static void ex_1102(const icd *i) /* wre cmode=0 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem0[(i->param + ba0) & 0xff];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1103(const icd *i) /* wre cmode=1 dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem0[(i->param + ba0) & 0xff];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1104(const icd *i) /* wre cmode=0 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem0[(id + ba0) & 0xff];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1105(const icd *i) /* wre cmode=1 dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem0[(id + ba0) & 0xff];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1106(const icd *i) /* wre cmode=0 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem1[(i->param + ba1) & 0x1f];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1107(const icd *i) /* wre cmode=1 dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem1[(i->param + ba1) & 0x1f];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1108(const icd *i) /* wre cmode=0 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem1[(id + ba1) & 0x1f];
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1109(const icd *i) /* wre cmode=1 dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xwr = dmem1[(id + ba1) & 0x1f];
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_WRITE;
}

static void ex_1110(const icd *i) /* rde cmode=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xoa = get_cmem(i->param);
	xm_init();
	sti |= S_READ;
}

static void ex_1111(const icd *i) /* rde cmode=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & (S_READ|S_WRITE))
	return;
	xoa = get_cmem(ca);
	xm_init();
	sti |= S_READ;
}

static void ex_1112(const icd *i) /* sacc cmode=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = aacc;
}

static void ex_1113(const icd *i) /* sacc cmode=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = aacc;
}

static void ex_1114(const icd *i) /* sacc cmode=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = (aacc << 7);
}

static void ex_1115(const icd *i) /* sacc cmode=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = (aacc << 7);
}

static void ex_1116(const icd *i) /* sacd dmode=0 dbp=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = aacc >> 8;
}

static void ex_1117(const icd *i) /* sacd dmode=1 dbp=0 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = aacc >> 8;
}

static void ex_1118(const icd *i) /* sacd dmode=0 dbp=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = aacc >> 8;
}

static void ex_1119(const icd *i) /* sacd dmode=1 dbp=1 sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = aacc >> 8;
}

static void ex_1120(const icd *i) /* sacd dmode=0 dbp=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (aacc << 7) >> 8;
}

static void ex_1121(const icd *i) /* sacd dmode=1 dbp=0 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (aacc << 7) >> 8;
}

static void ex_1122(const icd *i) /* sacd dmode=0 dbp=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (aacc << 7) >> 8;
}

static void ex_1123(const icd *i) /* sacd dmode=1 dbp=1 sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (aacc << 7) >> 8;
}

static void ex_1124(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1125(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1126(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1127(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1128(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1129(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1130(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1131(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1132(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1133(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1134(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1135(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1136(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1137(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1138(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1139(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1140(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1141(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1142(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1143(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1144(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1145(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1146(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1147(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1148(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1149(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1150(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1151(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1152(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1153(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1154(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1155(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1156(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1157(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1158(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1159(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1160(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1161(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1162(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1163(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1164(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1165(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1166(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1167(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1168(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1169(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1170(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1171(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1172(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1173(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1174(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1175(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1176(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1177(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1178(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1179(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1180(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1181(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1182(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1183(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1184(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1185(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1186(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1187(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1188(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1189(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1190(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1191(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1192(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1193(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1194(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1195(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1196(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1197(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1198(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1199(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1200(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1201(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1202(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1203(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1204(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1205(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1206(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1207(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1208(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1209(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1210(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1211(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1212(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1213(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1214(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1215(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1216(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1217(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1218(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1219(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1220(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1221(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1222(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1223(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1224(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1225(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1226(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1227(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1228(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1229(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1230(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1231(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1232(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1233(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1234(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1235(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1236(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1237(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1238(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1239(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1240(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1241(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1242(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1243(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1244(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1245(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1246(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1247(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1248(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1249(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1250(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1251(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1252(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1253(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1254(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1255(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1256(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1257(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1258(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1259(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1260(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1261(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1262(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1263(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1264(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1265(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1266(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1267(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1268(const icd *i) /* smhd dmode=0 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1269(const icd *i) /* smhd dmode=1 dbp=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1270(const icd *i) /* smhd dmode=0 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1271(const icd *i) /* smhd dmode=1 dbp=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1272(const icd *i) /* smhd dmode=0 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1273(const icd *i) /* smhd dmode=1 dbp=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1274(const icd *i) /* smhd dmode=0 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1275(const icd *i) /* smhd dmode=1 dbp=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1276(const icd *i) /* smhd dmode=0 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1277(const icd *i) /* smhd dmode=1 dbp=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1278(const icd *i) /* smhd dmode=0 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1279(const icd *i) /* smhd dmode=1 dbp=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1280(const icd *i) /* smhd dmode=0 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1281(const icd *i) /* smhd dmode=1 dbp=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1282(const icd *i) /* smhd dmode=0 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1283(const icd *i) /* smhd dmode=1 dbp=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1284(const icd *i) /* smhc cmode=0 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1285(const icd *i) /* smhc cmode=1 sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1286(const icd *i) /* smhc cmode=0 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1287(const icd *i) /* smhc cmode=1 sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1288(const icd *i) /* smhc cmode=0 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1289(const icd *i) /* smhc cmode=1 sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1290(const icd *i) /* smhc cmode=0 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1291(const icd *i) /* smhc cmode=1 sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1292(const icd *i) /* smhc cmode=0 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1293(const icd *i) /* smhc cmode=1 sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1294(const icd *i) /* smhc cmode=0 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1295(const icd *i) /* smhc cmode=1 sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1296(const icd *i) /* smhc cmode=0 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1297(const icd *i) /* smhc cmode=1 sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1298(const icd *i) /* smhc cmode=0 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1299(const icd *i) /* smhc cmode=1 sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1300(const icd *i) /* smhc cmode=0 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1301(const icd *i) /* smhc cmode=1 sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1302(const icd *i) /* smhc cmode=0 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1303(const icd *i) /* smhc cmode=1 sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1304(const icd *i) /* smhc cmode=0 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1305(const icd *i) /* smhc cmode=1 sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1306(const icd *i) /* smhc cmode=0 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1307(const icd *i) /* smhc cmode=1 sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1308(const icd *i) /* smhc cmode=0 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1309(const icd *i) /* smhc cmode=1 sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1310(const icd *i) /* smhc cmode=0 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1311(const icd *i) /* smhc cmode=1 sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1312(const icd *i) /* smhc cmode=0 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1313(const icd *i) /* smhc cmode=1 sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1314(const icd *i) /* smhc cmode=0 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1315(const icd *i) /* smhc cmode=1 sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1316(const icd *i) /* smhc cmode=0 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1317(const icd *i) /* smhc cmode=1 sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1318(const icd *i) /* smhc cmode=0 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1319(const icd *i) /* smhc cmode=1 sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1320(const icd *i) /* smhc cmode=0 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1321(const icd *i) /* smhc cmode=1 sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1322(const icd *i) /* smhc cmode=0 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1323(const icd *i) /* smhc cmode=1 sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1324(const icd *i) /* smhc cmode=0 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1325(const icd *i) /* smhc cmode=1 sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1326(const icd *i) /* smhc cmode=0 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1327(const icd *i) /* smhc cmode=1 sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1328(const icd *i) /* smhc cmode=0 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1329(const icd *i) /* smhc cmode=1 sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1330(const icd *i) /* smhc cmode=0 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1331(const icd *i) /* smhc cmode=1 sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 16;
}

static void ex_1332(const icd *i) /* smhc cmode=0 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1333(const icd *i) /* smhc cmode=1 sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1334(const icd *i) /* smhc cmode=0 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1335(const icd *i) /* smhc cmode=1 sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1336(const icd *i) /* smhc cmode=0 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1337(const icd *i) /* smhc cmode=1 sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1338(const icd *i) /* smhc cmode=0 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1339(const icd *i) /* smhc cmode=1 sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 16;
}

static void ex_1340(const icd *i) /* smhc cmode=0 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1341(const icd *i) /* smhc cmode=1 sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1342(const icd *i) /* smhc cmode=0 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1343(const icd *i) /* smhc cmode=1 sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1344(const icd *i) /* smhc cmode=0 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1345(const icd *i) /* smhc cmode=1 sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1346(const icd *i) /* smhc cmode=0 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1347(const icd *i) /* smhc cmode=1 sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 16;
}

static void ex_1348(const icd *i) /* smhc cmode=0 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1349(const icd *i) /* smhc cmode=1 sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1350(const icd *i) /* smhc cmode=0 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1351(const icd *i) /* smhc cmode=1 sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1352(const icd *i) /* smhc cmode=0 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1353(const icd *i) /* smhc cmode=1 sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1354(const icd *i) /* smhc cmode=0 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1355(const icd *i) /* smhc cmode=1 sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 16;
}

static void ex_1356(const icd *i) /* smhc cmode=0 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1357(const icd *i) /* smhc cmode=1 sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1358(const icd *i) /* smhc cmode=0 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1359(const icd *i) /* smhc cmode=1 sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1360(const icd *i) /* smhc cmode=0 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1361(const icd *i) /* smhc cmode=1 sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1362(const icd *i) /* smhc cmode=0 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[i->param] = macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1363(const icd *i) /* smhc cmode=1 sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	cmem[ca] = macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 16;
}

static void ex_1364(const icd *i) /* slmh dmode=0 dbp=0 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0() >> 24) & 0xffff00;
}

static void ex_1365(const icd *i) /* slmh dmode=1 dbp=0 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0() >> 24) & 0xffff00;
}

static void ex_1366(const icd *i) /* slmh dmode=0 dbp=1 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0() >> 24) & 0xffff00;
}

static void ex_1367(const icd *i) /* slmh dmode=1 dbp=1 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0() >> 24) & 0xffff00;
}

static void ex_1368(const icd *i) /* slmh dmode=0 dbp=0 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1() >> 24) & 0xffff00;
}

static void ex_1369(const icd *i) /* slmh dmode=1 dbp=0 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1() >> 24) & 0xffff00;
}

static void ex_1370(const icd *i) /* slmh dmode=0 dbp=1 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1() >> 24) & 0xffff00;
}

static void ex_1371(const icd *i) /* slmh dmode=1 dbp=1 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1() >> 24) & 0xffff00;
}

static void ex_1372(const icd *i) /* slmh dmode=0 dbp=0 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2() >> 24) & 0xffff00;
}

static void ex_1373(const icd *i) /* slmh dmode=1 dbp=0 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2() >> 24) & 0xffff00;
}

static void ex_1374(const icd *i) /* slmh dmode=0 dbp=1 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2() >> 24) & 0xffff00;
}

static void ex_1375(const icd *i) /* slmh dmode=1 dbp=1 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2() >> 24) & 0xffff00;
}

static void ex_1376(const icd *i) /* slmh dmode=0 dbp=0 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3() >> 24) & 0xffff00;
}

static void ex_1377(const icd *i) /* slmh dmode=1 dbp=0 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3() >> 24) & 0xffff00;
}

static void ex_1378(const icd *i) /* slmh dmode=0 dbp=1 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3() >> 24) & 0xffff00;
}

static void ex_1379(const icd *i) /* slmh dmode=1 dbp=1 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3() >> 24) & 0xffff00;
}

static void ex_1380(const icd *i) /* slmh dmode=0 dbp=0 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0s() >> 24) & 0xffff00;
}

static void ex_1381(const icd *i) /* slmh dmode=1 dbp=0 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0s() >> 24) & 0xffff00;
}

static void ex_1382(const icd *i) /* slmh dmode=0 dbp=1 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0s() >> 24) & 0xffff00;
}

static void ex_1383(const icd *i) /* slmh dmode=1 dbp=1 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0s() >> 24) & 0xffff00;
}

static void ex_1384(const icd *i) /* slmh dmode=0 dbp=0 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1s() >> 24) & 0xffff00;
}

static void ex_1385(const icd *i) /* slmh dmode=1 dbp=0 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1s() >> 24) & 0xffff00;
}

static void ex_1386(const icd *i) /* slmh dmode=0 dbp=1 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1s() >> 24) & 0xffff00;
}

static void ex_1387(const icd *i) /* slmh dmode=1 dbp=1 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1s() >> 24) & 0xffff00;
}

static void ex_1388(const icd *i) /* slmh dmode=0 dbp=0 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2s() >> 24) & 0xffff00;
}

static void ex_1389(const icd *i) /* slmh dmode=1 dbp=0 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2s() >> 24) & 0xffff00;
}

static void ex_1390(const icd *i) /* slmh dmode=0 dbp=1 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2s() >> 24) & 0xffff00;
}

static void ex_1391(const icd *i) /* slmh dmode=1 dbp=1 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2s() >> 24) & 0xffff00;
}

static void ex_1392(const icd *i) /* slmh dmode=0 dbp=0 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3s() >> 24) & 0xffff00;
}

static void ex_1393(const icd *i) /* slmh dmode=1 dbp=0 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3s() >> 24) & 0xffff00;
}

static void ex_1394(const icd *i) /* slmh dmode=0 dbp=1 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3s() >> 24) & 0xffff00;
}

static void ex_1395(const icd *i) /* slmh dmode=1 dbp=1 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3s() >> 24) & 0xffff00;
}

static void ex_1396(const icd *i) /* slml dmode=0 dbp=0 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0() >> 8) & 0xffffff;
}

static void ex_1397(const icd *i) /* slml dmode=1 dbp=0 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0() >> 8) & 0xffffff;
}

static void ex_1398(const icd *i) /* slml dmode=0 dbp=1 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0() >> 8) & 0xffffff;
}

static void ex_1399(const icd *i) /* slml dmode=1 dbp=1 sfmo=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0() >> 8) & 0xffffff;
}

static void ex_1400(const icd *i) /* slml dmode=0 dbp=0 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1() >> 8) & 0xffffff;
}

static void ex_1401(const icd *i) /* slml dmode=1 dbp=0 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1() >> 8) & 0xffffff;
}

static void ex_1402(const icd *i) /* slml dmode=0 dbp=1 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1() >> 8) & 0xffffff;
}

static void ex_1403(const icd *i) /* slml dmode=1 dbp=1 sfmo=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1() >> 8) & 0xffffff;
}

static void ex_1404(const icd *i) /* slml dmode=0 dbp=0 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2() >> 8) & 0xffffff;
}

static void ex_1405(const icd *i) /* slml dmode=1 dbp=0 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2() >> 8) & 0xffffff;
}

static void ex_1406(const icd *i) /* slml dmode=0 dbp=1 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2() >> 8) & 0xffffff;
}

static void ex_1407(const icd *i) /* slml dmode=1 dbp=1 sfmo=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2() >> 8) & 0xffffff;
}

static void ex_1408(const icd *i) /* slml dmode=0 dbp=0 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3() >> 8) & 0xffffff;
}

static void ex_1409(const icd *i) /* slml dmode=1 dbp=0 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3() >> 8) & 0xffffff;
}

static void ex_1410(const icd *i) /* slml dmode=0 dbp=1 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3() >> 8) & 0xffffff;
}

static void ex_1411(const icd *i) /* slml dmode=1 dbp=1 sfmo=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3() >> 8) & 0xffffff;
}

static void ex_1412(const icd *i) /* slml dmode=0 dbp=0 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_0s() >> 8) & 0xffffff;
}

static void ex_1413(const icd *i) /* slml dmode=1 dbp=0 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_0s() >> 8) & 0xffffff;
}

static void ex_1414(const icd *i) /* slml dmode=0 dbp=1 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_0s() >> 8) & 0xffffff;
}

static void ex_1415(const icd *i) /* slml dmode=1 dbp=1 sfmo=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_0s() >> 8) & 0xffffff;
}

static void ex_1416(const icd *i) /* slml dmode=0 dbp=0 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_1s() >> 8) & 0xffffff;
}

static void ex_1417(const icd *i) /* slml dmode=1 dbp=0 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_1s() >> 8) & 0xffffff;
}

static void ex_1418(const icd *i) /* slml dmode=0 dbp=1 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_1s() >> 8) & 0xffffff;
}

static void ex_1419(const icd *i) /* slml dmode=1 dbp=1 sfmo=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_1s() >> 8) & 0xffffff;
}

static void ex_1420(const icd *i) /* slml dmode=0 dbp=0 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_2s() >> 8) & 0xffffff;
}

static void ex_1421(const icd *i) /* slml dmode=1 dbp=0 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_2s() >> 8) & 0xffffff;
}

static void ex_1422(const icd *i) /* slml dmode=0 dbp=1 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_2s() >> 8) & 0xffffff;
}

static void ex_1423(const icd *i) /* slml dmode=1 dbp=1 sfmo=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_2s() >> 8) & 0xffffff;
}

static void ex_1424(const icd *i) /* slml dmode=0 dbp=0 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = (check_macc_overflow_3s() >> 8) & 0xffffff;
}

static void ex_1425(const icd *i) /* slml dmode=1 dbp=0 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = (check_macc_overflow_3s() >> 8) & 0xffffff;
}

static void ex_1426(const icd *i) /* slml dmode=0 dbp=1 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = (check_macc_overflow_3s() >> 8) & 0xffffff;
}

static void ex_1427(const icd *i) /* slml dmode=1 dbp=1 sfmo=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = (check_macc_overflow_3s() >> 8) & 0xffffff;
}

static void ex_1428(const icd *i) /* lcaa sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	ca = aacc >> 24;
}

static void ex_1429(const icd *i) /* lcaa sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	ca = (aacc << 7) >> 24;
}

static void ex_1430(const icd *i) /* lira sfao=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	id = aacc >> 24;
}

static void ex_1431(const icd *i) /* lira sfao=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	id = (aacc << 7) >> 24;
}

static void ex_1432(const icd *i) /* ref  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	/* nothing to do */
}

static void ex_1433(const icd *i) /* srbd dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = xrd;
}

static void ex_1434(const icd *i) /* srbd dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = xrd;
}

static void ex_1435(const icd *i) /* srbd dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = xrd;
}

static void ex_1436(const icd *i) /* srbd dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = xrd;
}

static void ex_1437(const icd *i) /* dis dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = si[0];
}

static void ex_1438(const icd *i) /* dis dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = si[0];
}

static void ex_1439(const icd *i) /* dis dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = si[0];
}

static void ex_1440(const icd *i) /* dis dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = si[0];
}

static void ex_1441(const icd *i) /* dis dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = si[1];
}

static void ex_1442(const icd *i) /* dis dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = si[1];
}

static void ex_1443(const icd *i) /* dis dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = si[1];
}

static void ex_1444(const icd *i) /* dis dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = si[1];
}

static void ex_1445(const icd *i) /* dis dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = si[2];
}

static void ex_1446(const icd *i) /* dis dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = si[2];
}

static void ex_1447(const icd *i) /* dis dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = si[2];
}

static void ex_1448(const icd *i) /* dis dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = si[2];
}

static void ex_1449(const icd *i) /* dis dmode=0 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(i->param + ba0) & 0xff] = si[3];
}

static void ex_1450(const icd *i) /* dis dmode=1 dbp=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem0[(id + ba0) & 0xff] = si[3];
}

static void ex_1451(const icd *i) /* dis dmode=0 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(i->param + ba1) & 0x1f] = si[3];
}

static void ex_1452(const icd *i) /* dis dmode=1 dbp=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	dmem1[(id + ba1) & 0x1f] = si[3];
}

static void ex_1453(const icd *i) /* domh sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1454(const icd *i) /* domh sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1455(const icd *i) /* domh sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1456(const icd *i) /* domh sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1457(const icd *i) /* domh sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1458(const icd *i) /* domh sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1459(const icd *i) /* domh sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1460(const icd *i) /* domh sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1461(const icd *i) /* domh sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1462(const icd *i) /* domh sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1463(const icd *i) /* domh sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1464(const icd *i) /* domh sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1465(const icd *i) /* domh sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1466(const icd *i) /* domh sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1467(const icd *i) /* domh sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1468(const icd *i) /* domh sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1469(const icd *i) /* domh sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1470(const icd *i) /* domh sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1471(const icd *i) /* domh sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1472(const icd *i) /* domh sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1473(const icd *i) /* domh sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1474(const icd *i) /* domh sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1475(const icd *i) /* domh sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1476(const icd *i) /* domh sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1477(const icd *i) /* domh sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1478(const icd *i) /* domh sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1479(const icd *i) /* domh sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1480(const icd *i) /* domh sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1481(const icd *i) /* domh sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1482(const icd *i) /* domh sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1483(const icd *i) /* domh sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1484(const icd *i) /* domh sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1485(const icd *i) /* domh sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1486(const icd *i) /* domh sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1487(const icd *i) /* domh sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1488(const icd *i) /* domh sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1489(const icd *i) /* domh sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1490(const icd *i) /* domh sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1491(const icd *i) /* domh sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1492(const icd *i) /* domh sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[0] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1493(const icd *i) /* domh sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1494(const icd *i) /* domh sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1495(const icd *i) /* domh sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1496(const icd *i) /* domh sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1497(const icd *i) /* domh sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1498(const icd *i) /* domh sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1499(const icd *i) /* domh sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1500(const icd *i) /* domh sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1501(const icd *i) /* domh sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1502(const icd *i) /* domh sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1503(const icd *i) /* domh sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1504(const icd *i) /* domh sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1505(const icd *i) /* domh sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1506(const icd *i) /* domh sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1507(const icd *i) /* domh sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1508(const icd *i) /* domh sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1509(const icd *i) /* domh sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1510(const icd *i) /* domh sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1511(const icd *i) /* domh sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1512(const icd *i) /* domh sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1513(const icd *i) /* domh sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1514(const icd *i) /* domh sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1515(const icd *i) /* domh sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1516(const icd *i) /* domh sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1517(const icd *i) /* domh sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1518(const icd *i) /* domh sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1519(const icd *i) /* domh sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1520(const icd *i) /* domh sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1521(const icd *i) /* domh sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1522(const icd *i) /* domh sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1523(const icd *i) /* domh sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1524(const icd *i) /* domh sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1525(const icd *i) /* domh sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1526(const icd *i) /* domh sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1527(const icd *i) /* domh sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1528(const icd *i) /* domh sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1529(const icd *i) /* domh sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1530(const icd *i) /* domh sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1531(const icd *i) /* domh sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1532(const icd *i) /* domh sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[1] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1533(const icd *i) /* domh sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1534(const icd *i) /* domh sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1535(const icd *i) /* domh sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1536(const icd *i) /* domh sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1537(const icd *i) /* domh sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1538(const icd *i) /* domh sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1539(const icd *i) /* domh sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1540(const icd *i) /* domh sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1541(const icd *i) /* domh sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1542(const icd *i) /* domh sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1543(const icd *i) /* domh sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1544(const icd *i) /* domh sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1545(const icd *i) /* domh sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1546(const icd *i) /* domh sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1547(const icd *i) /* domh sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1548(const icd *i) /* domh sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1549(const icd *i) /* domh sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1550(const icd *i) /* domh sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1551(const icd *i) /* domh sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1552(const icd *i) /* domh sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1553(const icd *i) /* domh sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1554(const icd *i) /* domh sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1555(const icd *i) /* domh sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1556(const icd *i) /* domh sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1557(const icd *i) /* domh sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1558(const icd *i) /* domh sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1559(const icd *i) /* domh sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1560(const icd *i) /* domh sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1561(const icd *i) /* domh sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1562(const icd *i) /* domh sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1563(const icd *i) /* domh sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1564(const icd *i) /* domh sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1565(const icd *i) /* domh sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1566(const icd *i) /* domh sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1567(const icd *i) /* domh sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1568(const icd *i) /* domh sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1569(const icd *i) /* domh sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1570(const icd *i) /* domh sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1571(const icd *i) /* domh sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1572(const icd *i) /* domh sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[2] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1573(const icd *i) /* domh sfmo=0 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1574(const icd *i) /* domh sfmo=1 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1575(const icd *i) /* domh sfmo=2 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1576(const icd *i) /* domh sfmo=3 rnd=0 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1577(const icd *i) /* domh sfmo=0 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1578(const icd *i) /* domh sfmo=1 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1579(const icd *i) /* domh sfmo=2 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1580(const icd *i) /* domh sfmo=3 rnd=1 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1581(const icd *i) /* domh sfmo=0 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1582(const icd *i) /* domh sfmo=1 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1583(const icd *i) /* domh sfmo=2 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1584(const icd *i) /* domh sfmo=3 rnd=2 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1585(const icd *i) /* domh sfmo=0 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1586(const icd *i) /* domh sfmo=1 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1587(const icd *i) /* domh sfmo=2 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1588(const icd *i) /* domh sfmo=3 rnd=3 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1589(const icd *i) /* domh sfmo=0 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1590(const icd *i) /* domh sfmo=1 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1591(const icd *i) /* domh sfmo=2 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1592(const icd *i) /* domh sfmo=3 rnd=4 movm=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1593(const icd *i) /* domh sfmo=0 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1594(const icd *i) /* domh sfmo=1 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1595(const icd *i) /* domh sfmo=2 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1596(const icd *i) /* domh sfmo=3 rnd=0 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00000000), TMS_U64(0xffffffff, 0xffffffff)) >> 24) & 0xffffff;
}

static void ex_1597(const icd *i) /* domh sfmo=0 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1598(const icd *i) /* domh sfmo=1 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1599(const icd *i) /* domh sfmo=2 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1600(const icd *i) /* domh sfmo=3 rnd=1 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00008000), TMS_U64(0xffffffff, 0xffff0000)) >> 24) & 0xffffff;
}

static void ex_1601(const icd *i) /* domh sfmo=0 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1602(const icd *i) /* domh sfmo=1 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1603(const icd *i) /* domh sfmo=2 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1604(const icd *i) /* domh sfmo=3 rnd=2 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00800000), TMS_U64(0xffffffff, 0xff000000)) >> 24) & 0xffffff;
}

static void ex_1605(const icd *i) /* domh sfmo=0 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1606(const icd *i) /* domh sfmo=1 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1607(const icd *i) /* domh sfmo=2 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1608(const icd *i) /* domh sfmo=3 rnd=3 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3s(TMS_U64(0x00000000, 0x00020000), TMS_U64(0xffffffff, 0xfffc0000)) >> 24) & 0xffffff;
}

static void ex_1609(const icd *i) /* domh sfmo=0 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_0s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1610(const icd *i) /* domh sfmo=1 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_1s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1611(const icd *i) /* domh sfmo=2 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_2s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1612(const icd *i) /* domh sfmo=3 rnd=4 movm=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	so[3] = (macc_to_output_3s(TMS_U64(0x00000000, 0x80000000), TMS_U64(0xffffffff, 0x00000000)) >> 24) & 0xffffff;
}

static void ex_1613(const icd *i) /* lpc cmode=0 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & S_HOST)
	return;
	c = get_cmem(i->param);
	host[0] = c >> 24;
	host[1] = c >> 16;
	host[2] = c >> 8;
	host[3] = c;
	hidx = 0;
	sti |= S_HOST;
	update_dready();
}

static void ex_1614(const icd *i) /* lpc cmode=1 */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(sti & S_HOST)
	return;
	c = get_cmem(ca);
	host[0] = c >> 24;
	host[1] = c >> 16;
	host[2] = c >> 8;
	host[3] = c;
	hidx = 0;
	sti |= S_HOST;
	update_dready();
}

static void ex_1615(const icd *i) /* rmov  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 &= ~ST1_MOV;
}

static void ex_1616(const icd *i) /* raom  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	/* Undocumented instruction, reset ALU saturation flag */
	st1 &= ~ST1_AOVM;
}

static void ex_1617(const icd *i) /* saom  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	/* Undocumented instruction, sets ALU saturation flag */
	st1 |= ST1_AOVM;
}

static void ex_1618(const icd *i) /* rmom  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 &= ~ST1_MOVM;
}

static void ex_1619(const icd *i) /* smom  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 |= ST1_MOVM;
}

static void ex_1620(const icd *i) /* ldpk  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 &= ~ST1_DBP;
}

static void ex_1621(const icd *i) /* ldpk  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 |= ST1_DBP;
}

static void ex_1622(const icd *i) /* scrm  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_CRM) | (0 << ST1_CRM_SHIFT);
}

static void ex_1623(const icd *i) /* scrm  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_CRM) | (1 << ST1_CRM_SHIFT);
}

static void ex_1624(const icd *i) /* scrm  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_CRM) | (2 << ST1_CRM_SHIFT);
}

static void ex_1625(const icd *i) /* scrm  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_CRM) | (3 << ST1_CRM_SHIFT);
}

static void ex_1626(const icd *i) /* sfao  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 &= ~ST1_SFAO;
}

static void ex_1627(const icd *i) /* sfao  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 |= ST1_SFAO;
}

static void ex_1628(const icd *i) /* sfai  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 &= ~ST1_SFAI;
}

static void ex_1629(const icd *i) /* sfai  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 |= ST1_SFAI;
}

static void ex_1630(const icd *i) /* sfma  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMA) | (0 << ST1_SFMA_SHIFT);
}

static void ex_1631(const icd *i) /* sfma  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMA) | (1 << ST1_SFMA_SHIFT);
}

static void ex_1632(const icd *i) /* sfma  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMA) | (2 << ST1_SFMA_SHIFT);
}

static void ex_1633(const icd *i) /* sfma  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMA) | (3 << ST1_SFMA_SHIFT);
}

static void ex_1634(const icd *i) /* sfmo  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMO) | (0 << ST1_SFMO_SHIFT);
}

static void ex_1635(const icd *i) /* sfmo  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMO) | (1 << ST1_SFMO_SHIFT);
}

static void ex_1636(const icd *i) /* sfmo  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMO) | (2 << ST1_SFMO_SHIFT);
}

static void ex_1637(const icd *i) /* sfmo  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_SFMO) | (3 << ST1_SFMO_SHIFT);
}

static void ex_1638(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (0 << ST1_RND_SHIFT);
}

static void ex_1639(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (1 << ST1_RND_SHIFT);
}

static void ex_1640(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (2 << ST1_RND_SHIFT);
}

static void ex_1641(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (3 << ST1_RND_SHIFT);
}

static void ex_1642(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (4 << ST1_RND_SHIFT);
}

static void ex_1643(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (5 << ST1_RND_SHIFT);
}

static void ex_1644(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (6 << ST1_RND_SHIFT);
}

static void ex_1645(const icd *i) /* rnd  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	st1 = (st1 & ~ST1_RND) | (7 << ST1_RND_SHIFT);
}

static void ex_1646(const icd *i) /* idle  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	sti |= S_IDLE;
}

static void ex_1647(const icd *i) /* rptk  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	rptc_next = i->param;
}

static void ex_1648(const icd *i) /* lcak  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	ca = i->param;
}

static void ex_1649(const icd *i) /* lirk  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	id = i->param;
}

static void ex_1650(const icd *i) /* lcac  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(((INT32)aacc) >= 0)
	ca = i->param;
}

static void ex_1651(const icd *i) /* b  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	pc = i->param;
  sti |= S_BRANCH;
}

static void ex_1652(const icd *i) /* bgz  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(((INT32)aacc) > 0) {
	pc = i->param;
  sti |= S_BRANCH;
	}
}

static void ex_1653(const icd *i) /* blz  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(((INT32)aacc) < 0) {
	pc = i->param;
  sti |= S_BRANCH;
	}
}

static void ex_1654(const icd *i) /* bnz  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(aacc) {
	pc = i->param;
  sti |= S_BRANCH;
	}
}

static void ex_1655(const icd *i) /* bv  */
{
	UINT32 c;
	UINT32 d;
	INT64 r;
	(void)c; (void)d; (void)r;
	if(st1 & ST1_AOV) {
	st1 &= ~ST1_AOV;
	pc = i->param;
  sti |= S_BRANCH;
	}
}

#endif
#ifdef CINTRPDECL
static void ex_4(const icd *i);
static void ex_5(const icd *i);
static void ex_6(const icd *i);
static void ex_7(const icd *i);
static void ex_8(const icd *i);
static void ex_9(const icd *i);
static void ex_10(const icd *i);
static void ex_11(const icd *i);
static void ex_12(const icd *i);
static void ex_13(const icd *i);
static void ex_14(const icd *i);
static void ex_15(const icd *i);
static void ex_16(const icd *i);
static void ex_17(const icd *i);
static void ex_18(const icd *i);
static void ex_19(const icd *i);
static void ex_20(const icd *i);
static void ex_21(const icd *i);
static void ex_22(const icd *i);
static void ex_23(const icd *i);
static void ex_24(const icd *i);
static void ex_25(const icd *i);
static void ex_26(const icd *i);
static void ex_27(const icd *i);
static void ex_28(const icd *i);
static void ex_29(const icd *i);
static void ex_30(const icd *i);
static void ex_31(const icd *i);
static void ex_32(const icd *i);
static void ex_33(const icd *i);
static void ex_34(const icd *i);
static void ex_35(const icd *i);
static void ex_36(const icd *i);
static void ex_37(const icd *i);
static void ex_38(const icd *i);
static void ex_39(const icd *i);
static void ex_40(const icd *i);
static void ex_41(const icd *i);
static void ex_42(const icd *i);
static void ex_43(const icd *i);
static void ex_44(const icd *i);
static void ex_45(const icd *i);
static void ex_46(const icd *i);
static void ex_47(const icd *i);
static void ex_48(const icd *i);
static void ex_49(const icd *i);
static void ex_50(const icd *i);
static void ex_51(const icd *i);
static void ex_52(const icd *i);
static void ex_53(const icd *i);
static void ex_54(const icd *i);
static void ex_55(const icd *i);
static void ex_56(const icd *i);
static void ex_57(const icd *i);
static void ex_58(const icd *i);
static void ex_59(const icd *i);
static void ex_60(const icd *i);
static void ex_61(const icd *i);
static void ex_62(const icd *i);
static void ex_63(const icd *i);
static void ex_64(const icd *i);
static void ex_65(const icd *i);
static void ex_66(const icd *i);
static void ex_67(const icd *i);
static void ex_68(const icd *i);
static void ex_69(const icd *i);
static void ex_70(const icd *i);
static void ex_71(const icd *i);
static void ex_72(const icd *i);
static void ex_73(const icd *i);
static void ex_74(const icd *i);
static void ex_75(const icd *i);
static void ex_76(const icd *i);
static void ex_77(const icd *i);
static void ex_78(const icd *i);
static void ex_79(const icd *i);
static void ex_80(const icd *i);
static void ex_81(const icd *i);
static void ex_82(const icd *i);
static void ex_83(const icd *i);
static void ex_84(const icd *i);
static void ex_85(const icd *i);
static void ex_86(const icd *i);
static void ex_87(const icd *i);
static void ex_88(const icd *i);
static void ex_89(const icd *i);
static void ex_90(const icd *i);
static void ex_91(const icd *i);
static void ex_92(const icd *i);
static void ex_93(const icd *i);
static void ex_94(const icd *i);
static void ex_95(const icd *i);
static void ex_96(const icd *i);
static void ex_97(const icd *i);
static void ex_98(const icd *i);
static void ex_99(const icd *i);
static void ex_100(const icd *i);
static void ex_101(const icd *i);
static void ex_102(const icd *i);
static void ex_103(const icd *i);
static void ex_104(const icd *i);
static void ex_105(const icd *i);
static void ex_106(const icd *i);
static void ex_107(const icd *i);
static void ex_108(const icd *i);
static void ex_109(const icd *i);
static void ex_110(const icd *i);
static void ex_111(const icd *i);
static void ex_112(const icd *i);
static void ex_113(const icd *i);
static void ex_114(const icd *i);
static void ex_115(const icd *i);
static void ex_116(const icd *i);
static void ex_117(const icd *i);
static void ex_118(const icd *i);
static void ex_119(const icd *i);
static void ex_120(const icd *i);
static void ex_121(const icd *i);
static void ex_122(const icd *i);
static void ex_123(const icd *i);
static void ex_124(const icd *i);
static void ex_125(const icd *i);
static void ex_126(const icd *i);
static void ex_127(const icd *i);
static void ex_128(const icd *i);
static void ex_129(const icd *i);
static void ex_130(const icd *i);
static void ex_131(const icd *i);
static void ex_132(const icd *i);
static void ex_133(const icd *i);
static void ex_134(const icd *i);
static void ex_135(const icd *i);
static void ex_136(const icd *i);
static void ex_137(const icd *i);
static void ex_138(const icd *i);
static void ex_139(const icd *i);
static void ex_140(const icd *i);
static void ex_141(const icd *i);
static void ex_142(const icd *i);
static void ex_143(const icd *i);
static void ex_144(const icd *i);
static void ex_145(const icd *i);
static void ex_146(const icd *i);
static void ex_147(const icd *i);
static void ex_148(const icd *i);
static void ex_149(const icd *i);
static void ex_150(const icd *i);
static void ex_151(const icd *i);
static void ex_152(const icd *i);
static void ex_153(const icd *i);
static void ex_154(const icd *i);
static void ex_155(const icd *i);
static void ex_156(const icd *i);
static void ex_157(const icd *i);
static void ex_158(const icd *i);
static void ex_159(const icd *i);
static void ex_160(const icd *i);
static void ex_161(const icd *i);
static void ex_162(const icd *i);
static void ex_163(const icd *i);
static void ex_164(const icd *i);
static void ex_165(const icd *i);
static void ex_166(const icd *i);
static void ex_167(const icd *i);
static void ex_168(const icd *i);
static void ex_169(const icd *i);
static void ex_170(const icd *i);
static void ex_171(const icd *i);
static void ex_172(const icd *i);
static void ex_173(const icd *i);
static void ex_174(const icd *i);
static void ex_175(const icd *i);
static void ex_176(const icd *i);
static void ex_177(const icd *i);
static void ex_178(const icd *i);
static void ex_179(const icd *i);
static void ex_180(const icd *i);
static void ex_181(const icd *i);
static void ex_182(const icd *i);
static void ex_183(const icd *i);
static void ex_184(const icd *i);
static void ex_185(const icd *i);
static void ex_186(const icd *i);
static void ex_187(const icd *i);
static void ex_188(const icd *i);
static void ex_189(const icd *i);
static void ex_190(const icd *i);
static void ex_191(const icd *i);
static void ex_192(const icd *i);
static void ex_193(const icd *i);
static void ex_194(const icd *i);
static void ex_195(const icd *i);
static void ex_196(const icd *i);
static void ex_197(const icd *i);
static void ex_198(const icd *i);
static void ex_199(const icd *i);
static void ex_200(const icd *i);
static void ex_201(const icd *i);
static void ex_202(const icd *i);
static void ex_203(const icd *i);
static void ex_204(const icd *i);
static void ex_205(const icd *i);
static void ex_206(const icd *i);
static void ex_207(const icd *i);
static void ex_208(const icd *i);
static void ex_209(const icd *i);
static void ex_210(const icd *i);
static void ex_211(const icd *i);
static void ex_212(const icd *i);
static void ex_213(const icd *i);
static void ex_214(const icd *i);
static void ex_215(const icd *i);
static void ex_216(const icd *i);
static void ex_217(const icd *i);
static void ex_218(const icd *i);
static void ex_219(const icd *i);
static void ex_220(const icd *i);
static void ex_221(const icd *i);
static void ex_222(const icd *i);
static void ex_223(const icd *i);
static void ex_224(const icd *i);
static void ex_225(const icd *i);
static void ex_226(const icd *i);
static void ex_227(const icd *i);
static void ex_228(const icd *i);
static void ex_229(const icd *i);
static void ex_230(const icd *i);
static void ex_231(const icd *i);
static void ex_232(const icd *i);
static void ex_233(const icd *i);
static void ex_234(const icd *i);
static void ex_235(const icd *i);
static void ex_236(const icd *i);
static void ex_237(const icd *i);
static void ex_238(const icd *i);
static void ex_239(const icd *i);
static void ex_240(const icd *i);
static void ex_241(const icd *i);
static void ex_242(const icd *i);
static void ex_243(const icd *i);
static void ex_244(const icd *i);
static void ex_245(const icd *i);
static void ex_246(const icd *i);
static void ex_247(const icd *i);
static void ex_248(const icd *i);
static void ex_249(const icd *i);
static void ex_250(const icd *i);
static void ex_251(const icd *i);
static void ex_252(const icd *i);
static void ex_253(const icd *i);
static void ex_254(const icd *i);
static void ex_255(const icd *i);
static void ex_256(const icd *i);
static void ex_257(const icd *i);
static void ex_258(const icd *i);
static void ex_259(const icd *i);
static void ex_260(const icd *i);
static void ex_261(const icd *i);
static void ex_262(const icd *i);
static void ex_263(const icd *i);
static void ex_264(const icd *i);
static void ex_265(const icd *i);
static void ex_266(const icd *i);
static void ex_267(const icd *i);
static void ex_268(const icd *i);
static void ex_269(const icd *i);
static void ex_270(const icd *i);
static void ex_271(const icd *i);
static void ex_272(const icd *i);
static void ex_273(const icd *i);
static void ex_274(const icd *i);
static void ex_275(const icd *i);
static void ex_276(const icd *i);
static void ex_277(const icd *i);
static void ex_278(const icd *i);
static void ex_279(const icd *i);
static void ex_280(const icd *i);
static void ex_281(const icd *i);
static void ex_282(const icd *i);
static void ex_283(const icd *i);
static void ex_284(const icd *i);
static void ex_285(const icd *i);
static void ex_286(const icd *i);
static void ex_287(const icd *i);
static void ex_288(const icd *i);
static void ex_289(const icd *i);
static void ex_290(const icd *i);
static void ex_291(const icd *i);
static void ex_292(const icd *i);
static void ex_293(const icd *i);
static void ex_294(const icd *i);
static void ex_295(const icd *i);
static void ex_296(const icd *i);
static void ex_297(const icd *i);
static void ex_298(const icd *i);
static void ex_299(const icd *i);
static void ex_300(const icd *i);
static void ex_301(const icd *i);
static void ex_302(const icd *i);
static void ex_303(const icd *i);
static void ex_304(const icd *i);
static void ex_305(const icd *i);
static void ex_306(const icd *i);
static void ex_307(const icd *i);
static void ex_308(const icd *i);
static void ex_309(const icd *i);
static void ex_310(const icd *i);
static void ex_311(const icd *i);
static void ex_312(const icd *i);
static void ex_313(const icd *i);
static void ex_314(const icd *i);
static void ex_315(const icd *i);
static void ex_316(const icd *i);
static void ex_317(const icd *i);
static void ex_318(const icd *i);
static void ex_319(const icd *i);
static void ex_320(const icd *i);
static void ex_321(const icd *i);
static void ex_322(const icd *i);
static void ex_323(const icd *i);
static void ex_324(const icd *i);
static void ex_325(const icd *i);
static void ex_326(const icd *i);
static void ex_327(const icd *i);
static void ex_328(const icd *i);
static void ex_329(const icd *i);
static void ex_330(const icd *i);
static void ex_331(const icd *i);
static void ex_332(const icd *i);
static void ex_333(const icd *i);
static void ex_334(const icd *i);
static void ex_335(const icd *i);
static void ex_336(const icd *i);
static void ex_337(const icd *i);
static void ex_338(const icd *i);
static void ex_339(const icd *i);
static void ex_340(const icd *i);
static void ex_341(const icd *i);
static void ex_342(const icd *i);
static void ex_343(const icd *i);
static void ex_344(const icd *i);
static void ex_345(const icd *i);
static void ex_346(const icd *i);
static void ex_347(const icd *i);
static void ex_348(const icd *i);
static void ex_349(const icd *i);
static void ex_350(const icd *i);
static void ex_351(const icd *i);
static void ex_352(const icd *i);
static void ex_353(const icd *i);
static void ex_354(const icd *i);
static void ex_355(const icd *i);
static void ex_356(const icd *i);
static void ex_357(const icd *i);
static void ex_358(const icd *i);
static void ex_359(const icd *i);
static void ex_360(const icd *i);
static void ex_361(const icd *i);
static void ex_362(const icd *i);
static void ex_363(const icd *i);
static void ex_364(const icd *i);
static void ex_365(const icd *i);
static void ex_366(const icd *i);
static void ex_367(const icd *i);
static void ex_368(const icd *i);
static void ex_369(const icd *i);
static void ex_370(const icd *i);
static void ex_371(const icd *i);
static void ex_372(const icd *i);
static void ex_373(const icd *i);
static void ex_374(const icd *i);
static void ex_375(const icd *i);
static void ex_376(const icd *i);
static void ex_377(const icd *i);
static void ex_378(const icd *i);
static void ex_379(const icd *i);
static void ex_380(const icd *i);
static void ex_381(const icd *i);
static void ex_382(const icd *i);
static void ex_383(const icd *i);
static void ex_384(const icd *i);
static void ex_385(const icd *i);
static void ex_386(const icd *i);
static void ex_387(const icd *i);
static void ex_388(const icd *i);
static void ex_389(const icd *i);
static void ex_390(const icd *i);
static void ex_391(const icd *i);
static void ex_392(const icd *i);
static void ex_393(const icd *i);
static void ex_394(const icd *i);
static void ex_395(const icd *i);
static void ex_396(const icd *i);
static void ex_397(const icd *i);
static void ex_398(const icd *i);
static void ex_399(const icd *i);
static void ex_400(const icd *i);
static void ex_401(const icd *i);
static void ex_402(const icd *i);
static void ex_403(const icd *i);
static void ex_404(const icd *i);
static void ex_405(const icd *i);
static void ex_406(const icd *i);
static void ex_407(const icd *i);
static void ex_408(const icd *i);
static void ex_409(const icd *i);
static void ex_410(const icd *i);
static void ex_411(const icd *i);
static void ex_412(const icd *i);
static void ex_413(const icd *i);
static void ex_414(const icd *i);
static void ex_415(const icd *i);
static void ex_416(const icd *i);
static void ex_417(const icd *i);
static void ex_418(const icd *i);
static void ex_419(const icd *i);
static void ex_420(const icd *i);
static void ex_421(const icd *i);
static void ex_422(const icd *i);
static void ex_423(const icd *i);
static void ex_424(const icd *i);
static void ex_425(const icd *i);
static void ex_426(const icd *i);
static void ex_427(const icd *i);
static void ex_428(const icd *i);
static void ex_429(const icd *i);
static void ex_430(const icd *i);
static void ex_431(const icd *i);
static void ex_432(const icd *i);
static void ex_433(const icd *i);
static void ex_434(const icd *i);
static void ex_435(const icd *i);
static void ex_436(const icd *i);
static void ex_437(const icd *i);
static void ex_438(const icd *i);
static void ex_439(const icd *i);
static void ex_440(const icd *i);
static void ex_441(const icd *i);
static void ex_442(const icd *i);
static void ex_443(const icd *i);
static void ex_444(const icd *i);
static void ex_445(const icd *i);
static void ex_446(const icd *i);
static void ex_447(const icd *i);
static void ex_448(const icd *i);
static void ex_449(const icd *i);
static void ex_450(const icd *i);
static void ex_451(const icd *i);
static void ex_452(const icd *i);
static void ex_453(const icd *i);
static void ex_454(const icd *i);
static void ex_455(const icd *i);
static void ex_456(const icd *i);
static void ex_457(const icd *i);
static void ex_458(const icd *i);
static void ex_459(const icd *i);
static void ex_460(const icd *i);
static void ex_461(const icd *i);
static void ex_462(const icd *i);
static void ex_463(const icd *i);
static void ex_464(const icd *i);
static void ex_465(const icd *i);
static void ex_466(const icd *i);
static void ex_467(const icd *i);
static void ex_468(const icd *i);
static void ex_469(const icd *i);
static void ex_470(const icd *i);
static void ex_471(const icd *i);
static void ex_472(const icd *i);
static void ex_473(const icd *i);
static void ex_474(const icd *i);
static void ex_475(const icd *i);
static void ex_476(const icd *i);
static void ex_477(const icd *i);
static void ex_478(const icd *i);
static void ex_479(const icd *i);
static void ex_480(const icd *i);
static void ex_481(const icd *i);
static void ex_482(const icd *i);
static void ex_483(const icd *i);
static void ex_484(const icd *i);
static void ex_485(const icd *i);
static void ex_486(const icd *i);
static void ex_487(const icd *i);
static void ex_488(const icd *i);
static void ex_489(const icd *i);
static void ex_490(const icd *i);
static void ex_491(const icd *i);
static void ex_492(const icd *i);
static void ex_493(const icd *i);
static void ex_494(const icd *i);
static void ex_495(const icd *i);
static void ex_496(const icd *i);
static void ex_497(const icd *i);
static void ex_498(const icd *i);
static void ex_499(const icd *i);
static void ex_500(const icd *i);
static void ex_501(const icd *i);
static void ex_502(const icd *i);
static void ex_503(const icd *i);
static void ex_504(const icd *i);
static void ex_505(const icd *i);
static void ex_506(const icd *i);
static void ex_507(const icd *i);
static void ex_508(const icd *i);
static void ex_509(const icd *i);
static void ex_510(const icd *i);
static void ex_511(const icd *i);
static void ex_512(const icd *i);
static void ex_513(const icd *i);
static void ex_514(const icd *i);
static void ex_515(const icd *i);
static void ex_516(const icd *i);
static void ex_517(const icd *i);
static void ex_518(const icd *i);
static void ex_519(const icd *i);
static void ex_520(const icd *i);
static void ex_521(const icd *i);
static void ex_522(const icd *i);
static void ex_523(const icd *i);
static void ex_524(const icd *i);
static void ex_525(const icd *i);
static void ex_526(const icd *i);
static void ex_527(const icd *i);
static void ex_528(const icd *i);
static void ex_529(const icd *i);
static void ex_530(const icd *i);
static void ex_531(const icd *i);
static void ex_532(const icd *i);
static void ex_533(const icd *i);
static void ex_534(const icd *i);
static void ex_535(const icd *i);
static void ex_536(const icd *i);
static void ex_537(const icd *i);
static void ex_538(const icd *i);
static void ex_539(const icd *i);
static void ex_540(const icd *i);
static void ex_541(const icd *i);
static void ex_542(const icd *i);
static void ex_543(const icd *i);
static void ex_544(const icd *i);
static void ex_545(const icd *i);
static void ex_546(const icd *i);
static void ex_547(const icd *i);
static void ex_548(const icd *i);
static void ex_549(const icd *i);
static void ex_550(const icd *i);
static void ex_551(const icd *i);
static void ex_552(const icd *i);
static void ex_553(const icd *i);
static void ex_554(const icd *i);
static void ex_555(const icd *i);
static void ex_556(const icd *i);
static void ex_557(const icd *i);
static void ex_558(const icd *i);
static void ex_559(const icd *i);
static void ex_560(const icd *i);
static void ex_561(const icd *i);
static void ex_562(const icd *i);
static void ex_563(const icd *i);
static void ex_564(const icd *i);
static void ex_565(const icd *i);
static void ex_566(const icd *i);
static void ex_567(const icd *i);
static void ex_568(const icd *i);
static void ex_569(const icd *i);
static void ex_570(const icd *i);
static void ex_571(const icd *i);
static void ex_572(const icd *i);
static void ex_573(const icd *i);
static void ex_574(const icd *i);
static void ex_575(const icd *i);
static void ex_576(const icd *i);
static void ex_577(const icd *i);
static void ex_578(const icd *i);
static void ex_579(const icd *i);
static void ex_580(const icd *i);
static void ex_581(const icd *i);
static void ex_582(const icd *i);
static void ex_583(const icd *i);
static void ex_584(const icd *i);
static void ex_585(const icd *i);
static void ex_586(const icd *i);
static void ex_587(const icd *i);
static void ex_588(const icd *i);
static void ex_589(const icd *i);
static void ex_590(const icd *i);
static void ex_591(const icd *i);
static void ex_592(const icd *i);
static void ex_593(const icd *i);
static void ex_594(const icd *i);
static void ex_595(const icd *i);
static void ex_596(const icd *i);
static void ex_597(const icd *i);
static void ex_598(const icd *i);
static void ex_599(const icd *i);
static void ex_600(const icd *i);
static void ex_601(const icd *i);
static void ex_602(const icd *i);
static void ex_603(const icd *i);
static void ex_604(const icd *i);
static void ex_605(const icd *i);
static void ex_606(const icd *i);
static void ex_607(const icd *i);
static void ex_608(const icd *i);
static void ex_609(const icd *i);
static void ex_610(const icd *i);
static void ex_611(const icd *i);
static void ex_612(const icd *i);
static void ex_613(const icd *i);
static void ex_614(const icd *i);
static void ex_615(const icd *i);
static void ex_616(const icd *i);
static void ex_617(const icd *i);
static void ex_618(const icd *i);
static void ex_619(const icd *i);
static void ex_620(const icd *i);
static void ex_621(const icd *i);
static void ex_622(const icd *i);
static void ex_623(const icd *i);
static void ex_624(const icd *i);
static void ex_625(const icd *i);
static void ex_626(const icd *i);
static void ex_627(const icd *i);
static void ex_628(const icd *i);
static void ex_629(const icd *i);
static void ex_630(const icd *i);
static void ex_631(const icd *i);
static void ex_632(const icd *i);
static void ex_633(const icd *i);
static void ex_634(const icd *i);
static void ex_635(const icd *i);
static void ex_636(const icd *i);
static void ex_637(const icd *i);
static void ex_638(const icd *i);
static void ex_639(const icd *i);
static void ex_640(const icd *i);
static void ex_641(const icd *i);
static void ex_642(const icd *i);
static void ex_643(const icd *i);
static void ex_644(const icd *i);
static void ex_645(const icd *i);
static void ex_646(const icd *i);
static void ex_647(const icd *i);
static void ex_648(const icd *i);
static void ex_649(const icd *i);
static void ex_650(const icd *i);
static void ex_651(const icd *i);
static void ex_652(const icd *i);
static void ex_653(const icd *i);
static void ex_654(const icd *i);
static void ex_655(const icd *i);
static void ex_656(const icd *i);
static void ex_657(const icd *i);
static void ex_658(const icd *i);
static void ex_659(const icd *i);
static void ex_660(const icd *i);
static void ex_661(const icd *i);
static void ex_662(const icd *i);
static void ex_663(const icd *i);
static void ex_664(const icd *i);
static void ex_665(const icd *i);
static void ex_666(const icd *i);
static void ex_667(const icd *i);
static void ex_668(const icd *i);
static void ex_669(const icd *i);
static void ex_670(const icd *i);
static void ex_671(const icd *i);
static void ex_672(const icd *i);
static void ex_673(const icd *i);
static void ex_674(const icd *i);
static void ex_675(const icd *i);
static void ex_676(const icd *i);
static void ex_677(const icd *i);
static void ex_678(const icd *i);
static void ex_679(const icd *i);
static void ex_680(const icd *i);
static void ex_681(const icd *i);
static void ex_682(const icd *i);
static void ex_683(const icd *i);
static void ex_684(const icd *i);
static void ex_685(const icd *i);
static void ex_686(const icd *i);
static void ex_687(const icd *i);
static void ex_688(const icd *i);
static void ex_689(const icd *i);
static void ex_690(const icd *i);
static void ex_691(const icd *i);
static void ex_692(const icd *i);
static void ex_693(const icd *i);
static void ex_694(const icd *i);
static void ex_695(const icd *i);
static void ex_696(const icd *i);
static void ex_697(const icd *i);
static void ex_698(const icd *i);
static void ex_699(const icd *i);
static void ex_700(const icd *i);
static void ex_701(const icd *i);
static void ex_702(const icd *i);
static void ex_703(const icd *i);
static void ex_704(const icd *i);
static void ex_705(const icd *i);
static void ex_706(const icd *i);
static void ex_707(const icd *i);
static void ex_708(const icd *i);
static void ex_709(const icd *i);
static void ex_710(const icd *i);
static void ex_711(const icd *i);
static void ex_712(const icd *i);
static void ex_713(const icd *i);
static void ex_714(const icd *i);
static void ex_715(const icd *i);
static void ex_716(const icd *i);
static void ex_717(const icd *i);
static void ex_718(const icd *i);
static void ex_719(const icd *i);
static void ex_720(const icd *i);
static void ex_721(const icd *i);
static void ex_722(const icd *i);
static void ex_723(const icd *i);
static void ex_724(const icd *i);
static void ex_725(const icd *i);
static void ex_726(const icd *i);
static void ex_727(const icd *i);
static void ex_728(const icd *i);
static void ex_729(const icd *i);
static void ex_730(const icd *i);
static void ex_731(const icd *i);
static void ex_732(const icd *i);
static void ex_733(const icd *i);
static void ex_734(const icd *i);
static void ex_735(const icd *i);
static void ex_736(const icd *i);
static void ex_737(const icd *i);
static void ex_738(const icd *i);
static void ex_739(const icd *i);
static void ex_740(const icd *i);
static void ex_741(const icd *i);
static void ex_742(const icd *i);
static void ex_743(const icd *i);
static void ex_744(const icd *i);
static void ex_745(const icd *i);
static void ex_746(const icd *i);
static void ex_747(const icd *i);
static void ex_748(const icd *i);
static void ex_749(const icd *i);
static void ex_750(const icd *i);
static void ex_751(const icd *i);
static void ex_752(const icd *i);
static void ex_753(const icd *i);
static void ex_754(const icd *i);
static void ex_755(const icd *i);
static void ex_756(const icd *i);
static void ex_757(const icd *i);
static void ex_758(const icd *i);
static void ex_759(const icd *i);
static void ex_760(const icd *i);
static void ex_761(const icd *i);
static void ex_762(const icd *i);
static void ex_763(const icd *i);
static void ex_764(const icd *i);
static void ex_765(const icd *i);
static void ex_766(const icd *i);
static void ex_767(const icd *i);
static void ex_768(const icd *i);
static void ex_769(const icd *i);
static void ex_770(const icd *i);
static void ex_771(const icd *i);
static void ex_772(const icd *i);
static void ex_773(const icd *i);
static void ex_774(const icd *i);
static void ex_775(const icd *i);
static void ex_776(const icd *i);
static void ex_777(const icd *i);
static void ex_778(const icd *i);
static void ex_779(const icd *i);
static void ex_780(const icd *i);
static void ex_781(const icd *i);
static void ex_782(const icd *i);
static void ex_783(const icd *i);
static void ex_784(const icd *i);
static void ex_785(const icd *i);
static void ex_786(const icd *i);
static void ex_787(const icd *i);
static void ex_788(const icd *i);
static void ex_789(const icd *i);
static void ex_790(const icd *i);
static void ex_791(const icd *i);
static void ex_792(const icd *i);
static void ex_793(const icd *i);
static void ex_794(const icd *i);
static void ex_795(const icd *i);
static void ex_796(const icd *i);
static void ex_797(const icd *i);
static void ex_798(const icd *i);
static void ex_799(const icd *i);
static void ex_800(const icd *i);
static void ex_801(const icd *i);
static void ex_802(const icd *i);
static void ex_803(const icd *i);
static void ex_804(const icd *i);
static void ex_805(const icd *i);
static void ex_806(const icd *i);
static void ex_807(const icd *i);
static void ex_808(const icd *i);
static void ex_809(const icd *i);
static void ex_810(const icd *i);
static void ex_811(const icd *i);
static void ex_812(const icd *i);
static void ex_813(const icd *i);
static void ex_814(const icd *i);
static void ex_815(const icd *i);
static void ex_816(const icd *i);
static void ex_817(const icd *i);
static void ex_818(const icd *i);
static void ex_819(const icd *i);
static void ex_820(const icd *i);
static void ex_821(const icd *i);
static void ex_822(const icd *i);
static void ex_823(const icd *i);
static void ex_824(const icd *i);
static void ex_825(const icd *i);
static void ex_826(const icd *i);
static void ex_827(const icd *i);
static void ex_828(const icd *i);
static void ex_829(const icd *i);
static void ex_830(const icd *i);
static void ex_831(const icd *i);
static void ex_832(const icd *i);
static void ex_833(const icd *i);
static void ex_834(const icd *i);
static void ex_835(const icd *i);
static void ex_836(const icd *i);
static void ex_837(const icd *i);
static void ex_838(const icd *i);
static void ex_839(const icd *i);
static void ex_840(const icd *i);
static void ex_841(const icd *i);
static void ex_842(const icd *i);
static void ex_843(const icd *i);
static void ex_844(const icd *i);
static void ex_845(const icd *i);
static void ex_846(const icd *i);
static void ex_847(const icd *i);
static void ex_848(const icd *i);
static void ex_849(const icd *i);
static void ex_850(const icd *i);
static void ex_851(const icd *i);
static void ex_852(const icd *i);
static void ex_853(const icd *i);
static void ex_854(const icd *i);
static void ex_855(const icd *i);
static void ex_856(const icd *i);
static void ex_857(const icd *i);
static void ex_858(const icd *i);
static void ex_859(const icd *i);
static void ex_860(const icd *i);
static void ex_861(const icd *i);
static void ex_862(const icd *i);
static void ex_863(const icd *i);
static void ex_864(const icd *i);
static void ex_865(const icd *i);
static void ex_866(const icd *i);
static void ex_867(const icd *i);
static void ex_868(const icd *i);
static void ex_869(const icd *i);
static void ex_870(const icd *i);
static void ex_871(const icd *i);
static void ex_872(const icd *i);
static void ex_873(const icd *i);
static void ex_874(const icd *i);
static void ex_875(const icd *i);
static void ex_876(const icd *i);
static void ex_877(const icd *i);
static void ex_878(const icd *i);
static void ex_879(const icd *i);
static void ex_880(const icd *i);
static void ex_881(const icd *i);
static void ex_882(const icd *i);
static void ex_883(const icd *i);
static void ex_884(const icd *i);
static void ex_885(const icd *i);
static void ex_886(const icd *i);
static void ex_887(const icd *i);
static void ex_888(const icd *i);
static void ex_889(const icd *i);
static void ex_890(const icd *i);
static void ex_891(const icd *i);
static void ex_892(const icd *i);
static void ex_893(const icd *i);
static void ex_894(const icd *i);
static void ex_895(const icd *i);
static void ex_896(const icd *i);
static void ex_897(const icd *i);
static void ex_898(const icd *i);
static void ex_899(const icd *i);
static void ex_900(const icd *i);
static void ex_901(const icd *i);
static void ex_902(const icd *i);
static void ex_903(const icd *i);
static void ex_904(const icd *i);
static void ex_905(const icd *i);
static void ex_906(const icd *i);
static void ex_907(const icd *i);
static void ex_908(const icd *i);
static void ex_909(const icd *i);
static void ex_910(const icd *i);
static void ex_911(const icd *i);
static void ex_912(const icd *i);
static void ex_913(const icd *i);
static void ex_914(const icd *i);
static void ex_915(const icd *i);
static void ex_916(const icd *i);
static void ex_917(const icd *i);
static void ex_918(const icd *i);
static void ex_919(const icd *i);
static void ex_920(const icd *i);
static void ex_921(const icd *i);
static void ex_922(const icd *i);
static void ex_923(const icd *i);
static void ex_924(const icd *i);
static void ex_925(const icd *i);
static void ex_926(const icd *i);
static void ex_927(const icd *i);
static void ex_928(const icd *i);
static void ex_929(const icd *i);
static void ex_930(const icd *i);
static void ex_931(const icd *i);
static void ex_932(const icd *i);
static void ex_933(const icd *i);
static void ex_934(const icd *i);
static void ex_935(const icd *i);
static void ex_936(const icd *i);
static void ex_937(const icd *i);
static void ex_938(const icd *i);
static void ex_939(const icd *i);
static void ex_940(const icd *i);
static void ex_941(const icd *i);
static void ex_942(const icd *i);
static void ex_943(const icd *i);
static void ex_944(const icd *i);
static void ex_945(const icd *i);
static void ex_946(const icd *i);
static void ex_947(const icd *i);
static void ex_948(const icd *i);
static void ex_949(const icd *i);
static void ex_950(const icd *i);
static void ex_951(const icd *i);
static void ex_952(const icd *i);
static void ex_953(const icd *i);
static void ex_954(const icd *i);
static void ex_955(const icd *i);
static void ex_956(const icd *i);
static void ex_957(const icd *i);
static void ex_958(const icd *i);
static void ex_959(const icd *i);
static void ex_960(const icd *i);
static void ex_961(const icd *i);
static void ex_962(const icd *i);
static void ex_963(const icd *i);
static void ex_964(const icd *i);
static void ex_965(const icd *i);
static void ex_966(const icd *i);
static void ex_967(const icd *i);
static void ex_968(const icd *i);
static void ex_969(const icd *i);
static void ex_970(const icd *i);
static void ex_971(const icd *i);
static void ex_972(const icd *i);
static void ex_973(const icd *i);
static void ex_974(const icd *i);
static void ex_975(const icd *i);
static void ex_976(const icd *i);
static void ex_977(const icd *i);
static void ex_978(const icd *i);
static void ex_979(const icd *i);
static void ex_980(const icd *i);
static void ex_981(const icd *i);
static void ex_982(const icd *i);
static void ex_983(const icd *i);
static void ex_984(const icd *i);
static void ex_985(const icd *i);
static void ex_986(const icd *i);
static void ex_987(const icd *i);
static void ex_988(const icd *i);
static void ex_989(const icd *i);
static void ex_990(const icd *i);
static void ex_991(const icd *i);
static void ex_992(const icd *i);
static void ex_993(const icd *i);
static void ex_994(const icd *i);
static void ex_995(const icd *i);
static void ex_996(const icd *i);
static void ex_997(const icd *i);
static void ex_998(const icd *i);
static void ex_999(const icd *i);
static void ex_1000(const icd *i);
static void ex_1001(const icd *i);
static void ex_1002(const icd *i);
static void ex_1003(const icd *i);
static void ex_1004(const icd *i);
static void ex_1005(const icd *i);
static void ex_1006(const icd *i);
static void ex_1007(const icd *i);
static void ex_1008(const icd *i);
static void ex_1009(const icd *i);
static void ex_1010(const icd *i);
static void ex_1011(const icd *i);
static void ex_1012(const icd *i);
static void ex_1013(const icd *i);
static void ex_1014(const icd *i);
static void ex_1015(const icd *i);
static void ex_1016(const icd *i);
static void ex_1017(const icd *i);
static void ex_1018(const icd *i);
static void ex_1019(const icd *i);
static void ex_1020(const icd *i);
static void ex_1021(const icd *i);
static void ex_1022(const icd *i);
static void ex_1023(const icd *i);
static void ex_1024(const icd *i);
static void ex_1025(const icd *i);
static void ex_1026(const icd *i);
static void ex_1027(const icd *i);
static void ex_1028(const icd *i);
static void ex_1029(const icd *i);
static void ex_1030(const icd *i);
static void ex_1031(const icd *i);
static void ex_1032(const icd *i);
static void ex_1033(const icd *i);
static void ex_1034(const icd *i);
static void ex_1035(const icd *i);
static void ex_1036(const icd *i);
static void ex_1037(const icd *i);
static void ex_1038(const icd *i);
static void ex_1039(const icd *i);
static void ex_1040(const icd *i);
static void ex_1041(const icd *i);
static void ex_1042(const icd *i);
static void ex_1043(const icd *i);
static void ex_1044(const icd *i);
static void ex_1045(const icd *i);
static void ex_1046(const icd *i);
static void ex_1047(const icd *i);
static void ex_1048(const icd *i);
static void ex_1049(const icd *i);
static void ex_1050(const icd *i);
static void ex_1051(const icd *i);
static void ex_1052(const icd *i);
static void ex_1053(const icd *i);
static void ex_1054(const icd *i);
static void ex_1055(const icd *i);
static void ex_1056(const icd *i);
static void ex_1057(const icd *i);
static void ex_1058(const icd *i);
static void ex_1059(const icd *i);
static void ex_1060(const icd *i);
static void ex_1061(const icd *i);
static void ex_1062(const icd *i);
static void ex_1063(const icd *i);
static void ex_1064(const icd *i);
static void ex_1065(const icd *i);
static void ex_1066(const icd *i);
static void ex_1067(const icd *i);
static void ex_1068(const icd *i);
static void ex_1069(const icd *i);
static void ex_1070(const icd *i);
static void ex_1071(const icd *i);
static void ex_1072(const icd *i);
static void ex_1073(const icd *i);
static void ex_1074(const icd *i);
static void ex_1075(const icd *i);
static void ex_1076(const icd *i);
static void ex_1077(const icd *i);
static void ex_1078(const icd *i);
static void ex_1079(const icd *i);
static void ex_1080(const icd *i);
static void ex_1081(const icd *i);
static void ex_1082(const icd *i);
static void ex_1083(const icd *i);
static void ex_1084(const icd *i);
static void ex_1085(const icd *i);
static void ex_1086(const icd *i);
static void ex_1087(const icd *i);
static void ex_1088(const icd *i);
static void ex_1089(const icd *i);
static void ex_1090(const icd *i);
static void ex_1091(const icd *i);
static void ex_1092(const icd *i);
static void ex_1093(const icd *i);
static void ex_1094(const icd *i);
static void ex_1095(const icd *i);
static void ex_1096(const icd *i);
static void ex_1097(const icd *i);
static void ex_1098(const icd *i);
static void ex_1099(const icd *i);
static void ex_1100(const icd *i);
static void ex_1101(const icd *i);
static void ex_1102(const icd *i);
static void ex_1103(const icd *i);
static void ex_1104(const icd *i);
static void ex_1105(const icd *i);
static void ex_1106(const icd *i);
static void ex_1107(const icd *i);
static void ex_1108(const icd *i);
static void ex_1109(const icd *i);
static void ex_1110(const icd *i);
static void ex_1111(const icd *i);
static void ex_1112(const icd *i);
static void ex_1113(const icd *i);
static void ex_1114(const icd *i);
static void ex_1115(const icd *i);
static void ex_1116(const icd *i);
static void ex_1117(const icd *i);
static void ex_1118(const icd *i);
static void ex_1119(const icd *i);
static void ex_1120(const icd *i);
static void ex_1121(const icd *i);
static void ex_1122(const icd *i);
static void ex_1123(const icd *i);
static void ex_1124(const icd *i);
static void ex_1125(const icd *i);
static void ex_1126(const icd *i);
static void ex_1127(const icd *i);
static void ex_1128(const icd *i);
static void ex_1129(const icd *i);
static void ex_1130(const icd *i);
static void ex_1131(const icd *i);
static void ex_1132(const icd *i);
static void ex_1133(const icd *i);
static void ex_1134(const icd *i);
static void ex_1135(const icd *i);
static void ex_1136(const icd *i);
static void ex_1137(const icd *i);
static void ex_1138(const icd *i);
static void ex_1139(const icd *i);
static void ex_1140(const icd *i);
static void ex_1141(const icd *i);
static void ex_1142(const icd *i);
static void ex_1143(const icd *i);
static void ex_1144(const icd *i);
static void ex_1145(const icd *i);
static void ex_1146(const icd *i);
static void ex_1147(const icd *i);
static void ex_1148(const icd *i);
static void ex_1149(const icd *i);
static void ex_1150(const icd *i);
static void ex_1151(const icd *i);
static void ex_1152(const icd *i);
static void ex_1153(const icd *i);
static void ex_1154(const icd *i);
static void ex_1155(const icd *i);
static void ex_1156(const icd *i);
static void ex_1157(const icd *i);
static void ex_1158(const icd *i);
static void ex_1159(const icd *i);
static void ex_1160(const icd *i);
static void ex_1161(const icd *i);
static void ex_1162(const icd *i);
static void ex_1163(const icd *i);
static void ex_1164(const icd *i);
static void ex_1165(const icd *i);
static void ex_1166(const icd *i);
static void ex_1167(const icd *i);
static void ex_1168(const icd *i);
static void ex_1169(const icd *i);
static void ex_1170(const icd *i);
static void ex_1171(const icd *i);
static void ex_1172(const icd *i);
static void ex_1173(const icd *i);
static void ex_1174(const icd *i);
static void ex_1175(const icd *i);
static void ex_1176(const icd *i);
static void ex_1177(const icd *i);
static void ex_1178(const icd *i);
static void ex_1179(const icd *i);
static void ex_1180(const icd *i);
static void ex_1181(const icd *i);
static void ex_1182(const icd *i);
static void ex_1183(const icd *i);
static void ex_1184(const icd *i);
static void ex_1185(const icd *i);
static void ex_1186(const icd *i);
static void ex_1187(const icd *i);
static void ex_1188(const icd *i);
static void ex_1189(const icd *i);
static void ex_1190(const icd *i);
static void ex_1191(const icd *i);
static void ex_1192(const icd *i);
static void ex_1193(const icd *i);
static void ex_1194(const icd *i);
static void ex_1195(const icd *i);
static void ex_1196(const icd *i);
static void ex_1197(const icd *i);
static void ex_1198(const icd *i);
static void ex_1199(const icd *i);
static void ex_1200(const icd *i);
static void ex_1201(const icd *i);
static void ex_1202(const icd *i);
static void ex_1203(const icd *i);
static void ex_1204(const icd *i);
static void ex_1205(const icd *i);
static void ex_1206(const icd *i);
static void ex_1207(const icd *i);
static void ex_1208(const icd *i);
static void ex_1209(const icd *i);
static void ex_1210(const icd *i);
static void ex_1211(const icd *i);
static void ex_1212(const icd *i);
static void ex_1213(const icd *i);
static void ex_1214(const icd *i);
static void ex_1215(const icd *i);
static void ex_1216(const icd *i);
static void ex_1217(const icd *i);
static void ex_1218(const icd *i);
static void ex_1219(const icd *i);
static void ex_1220(const icd *i);
static void ex_1221(const icd *i);
static void ex_1222(const icd *i);
static void ex_1223(const icd *i);
static void ex_1224(const icd *i);
static void ex_1225(const icd *i);
static void ex_1226(const icd *i);
static void ex_1227(const icd *i);
static void ex_1228(const icd *i);
static void ex_1229(const icd *i);
static void ex_1230(const icd *i);
static void ex_1231(const icd *i);
static void ex_1232(const icd *i);
static void ex_1233(const icd *i);
static void ex_1234(const icd *i);
static void ex_1235(const icd *i);
static void ex_1236(const icd *i);
static void ex_1237(const icd *i);
static void ex_1238(const icd *i);
static void ex_1239(const icd *i);
static void ex_1240(const icd *i);
static void ex_1241(const icd *i);
static void ex_1242(const icd *i);
static void ex_1243(const icd *i);
static void ex_1244(const icd *i);
static void ex_1245(const icd *i);
static void ex_1246(const icd *i);
static void ex_1247(const icd *i);
static void ex_1248(const icd *i);
static void ex_1249(const icd *i);
static void ex_1250(const icd *i);
static void ex_1251(const icd *i);
static void ex_1252(const icd *i);
static void ex_1253(const icd *i);
static void ex_1254(const icd *i);
static void ex_1255(const icd *i);
static void ex_1256(const icd *i);
static void ex_1257(const icd *i);
static void ex_1258(const icd *i);
static void ex_1259(const icd *i);
static void ex_1260(const icd *i);
static void ex_1261(const icd *i);
static void ex_1262(const icd *i);
static void ex_1263(const icd *i);
static void ex_1264(const icd *i);
static void ex_1265(const icd *i);
static void ex_1266(const icd *i);
static void ex_1267(const icd *i);
static void ex_1268(const icd *i);
static void ex_1269(const icd *i);
static void ex_1270(const icd *i);
static void ex_1271(const icd *i);
static void ex_1272(const icd *i);
static void ex_1273(const icd *i);
static void ex_1274(const icd *i);
static void ex_1275(const icd *i);
static void ex_1276(const icd *i);
static void ex_1277(const icd *i);
static void ex_1278(const icd *i);
static void ex_1279(const icd *i);
static void ex_1280(const icd *i);
static void ex_1281(const icd *i);
static void ex_1282(const icd *i);
static void ex_1283(const icd *i);
static void ex_1284(const icd *i);
static void ex_1285(const icd *i);
static void ex_1286(const icd *i);
static void ex_1287(const icd *i);
static void ex_1288(const icd *i);
static void ex_1289(const icd *i);
static void ex_1290(const icd *i);
static void ex_1291(const icd *i);
static void ex_1292(const icd *i);
static void ex_1293(const icd *i);
static void ex_1294(const icd *i);
static void ex_1295(const icd *i);
static void ex_1296(const icd *i);
static void ex_1297(const icd *i);
static void ex_1298(const icd *i);
static void ex_1299(const icd *i);
static void ex_1300(const icd *i);
static void ex_1301(const icd *i);
static void ex_1302(const icd *i);
static void ex_1303(const icd *i);
static void ex_1304(const icd *i);
static void ex_1305(const icd *i);
static void ex_1306(const icd *i);
static void ex_1307(const icd *i);
static void ex_1308(const icd *i);
static void ex_1309(const icd *i);
static void ex_1310(const icd *i);
static void ex_1311(const icd *i);
static void ex_1312(const icd *i);
static void ex_1313(const icd *i);
static void ex_1314(const icd *i);
static void ex_1315(const icd *i);
static void ex_1316(const icd *i);
static void ex_1317(const icd *i);
static void ex_1318(const icd *i);
static void ex_1319(const icd *i);
static void ex_1320(const icd *i);
static void ex_1321(const icd *i);
static void ex_1322(const icd *i);
static void ex_1323(const icd *i);
static void ex_1324(const icd *i);
static void ex_1325(const icd *i);
static void ex_1326(const icd *i);
static void ex_1327(const icd *i);
static void ex_1328(const icd *i);
static void ex_1329(const icd *i);
static void ex_1330(const icd *i);
static void ex_1331(const icd *i);
static void ex_1332(const icd *i);
static void ex_1333(const icd *i);
static void ex_1334(const icd *i);
static void ex_1335(const icd *i);
static void ex_1336(const icd *i);
static void ex_1337(const icd *i);
static void ex_1338(const icd *i);
static void ex_1339(const icd *i);
static void ex_1340(const icd *i);
static void ex_1341(const icd *i);
static void ex_1342(const icd *i);
static void ex_1343(const icd *i);
static void ex_1344(const icd *i);
static void ex_1345(const icd *i);
static void ex_1346(const icd *i);
static void ex_1347(const icd *i);
static void ex_1348(const icd *i);
static void ex_1349(const icd *i);
static void ex_1350(const icd *i);
static void ex_1351(const icd *i);
static void ex_1352(const icd *i);
static void ex_1353(const icd *i);
static void ex_1354(const icd *i);
static void ex_1355(const icd *i);
static void ex_1356(const icd *i);
static void ex_1357(const icd *i);
static void ex_1358(const icd *i);
static void ex_1359(const icd *i);
static void ex_1360(const icd *i);
static void ex_1361(const icd *i);
static void ex_1362(const icd *i);
static void ex_1363(const icd *i);
static void ex_1364(const icd *i);
static void ex_1365(const icd *i);
static void ex_1366(const icd *i);
static void ex_1367(const icd *i);
static void ex_1368(const icd *i);
static void ex_1369(const icd *i);
static void ex_1370(const icd *i);
static void ex_1371(const icd *i);
static void ex_1372(const icd *i);
static void ex_1373(const icd *i);
static void ex_1374(const icd *i);
static void ex_1375(const icd *i);
static void ex_1376(const icd *i);
static void ex_1377(const icd *i);
static void ex_1378(const icd *i);
static void ex_1379(const icd *i);
static void ex_1380(const icd *i);
static void ex_1381(const icd *i);
static void ex_1382(const icd *i);
static void ex_1383(const icd *i);
static void ex_1384(const icd *i);
static void ex_1385(const icd *i);
static void ex_1386(const icd *i);
static void ex_1387(const icd *i);
static void ex_1388(const icd *i);
static void ex_1389(const icd *i);
static void ex_1390(const icd *i);
static void ex_1391(const icd *i);
static void ex_1392(const icd *i);
static void ex_1393(const icd *i);
static void ex_1394(const icd *i);
static void ex_1395(const icd *i);
static void ex_1396(const icd *i);
static void ex_1397(const icd *i);
static void ex_1398(const icd *i);
static void ex_1399(const icd *i);
static void ex_1400(const icd *i);
static void ex_1401(const icd *i);
static void ex_1402(const icd *i);
static void ex_1403(const icd *i);
static void ex_1404(const icd *i);
static void ex_1405(const icd *i);
static void ex_1406(const icd *i);
static void ex_1407(const icd *i);
static void ex_1408(const icd *i);
static void ex_1409(const icd *i);
static void ex_1410(const icd *i);
static void ex_1411(const icd *i);
static void ex_1412(const icd *i);
static void ex_1413(const icd *i);
static void ex_1414(const icd *i);
static void ex_1415(const icd *i);
static void ex_1416(const icd *i);
static void ex_1417(const icd *i);
static void ex_1418(const icd *i);
static void ex_1419(const icd *i);
static void ex_1420(const icd *i);
static void ex_1421(const icd *i);
static void ex_1422(const icd *i);
static void ex_1423(const icd *i);
static void ex_1424(const icd *i);
static void ex_1425(const icd *i);
static void ex_1426(const icd *i);
static void ex_1427(const icd *i);
static void ex_1428(const icd *i);
static void ex_1429(const icd *i);
static void ex_1430(const icd *i);
static void ex_1431(const icd *i);
static void ex_1432(const icd *i);
static void ex_1433(const icd *i);
static void ex_1434(const icd *i);
static void ex_1435(const icd *i);
static void ex_1436(const icd *i);
static void ex_1437(const icd *i);
static void ex_1438(const icd *i);
static void ex_1439(const icd *i);
static void ex_1440(const icd *i);
static void ex_1441(const icd *i);
static void ex_1442(const icd *i);
static void ex_1443(const icd *i);
static void ex_1444(const icd *i);
static void ex_1445(const icd *i);
static void ex_1446(const icd *i);
static void ex_1447(const icd *i);
static void ex_1448(const icd *i);
static void ex_1449(const icd *i);
static void ex_1450(const icd *i);
static void ex_1451(const icd *i);
static void ex_1452(const icd *i);
static void ex_1453(const icd *i);
static void ex_1454(const icd *i);
static void ex_1455(const icd *i);
static void ex_1456(const icd *i);
static void ex_1457(const icd *i);
static void ex_1458(const icd *i);
static void ex_1459(const icd *i);
static void ex_1460(const icd *i);
static void ex_1461(const icd *i);
static void ex_1462(const icd *i);
static void ex_1463(const icd *i);
static void ex_1464(const icd *i);
static void ex_1465(const icd *i);
static void ex_1466(const icd *i);
static void ex_1467(const icd *i);
static void ex_1468(const icd *i);
static void ex_1469(const icd *i);
static void ex_1470(const icd *i);
static void ex_1471(const icd *i);
static void ex_1472(const icd *i);
static void ex_1473(const icd *i);
static void ex_1474(const icd *i);
static void ex_1475(const icd *i);
static void ex_1476(const icd *i);
static void ex_1477(const icd *i);
static void ex_1478(const icd *i);
static void ex_1479(const icd *i);
static void ex_1480(const icd *i);
static void ex_1481(const icd *i);
static void ex_1482(const icd *i);
static void ex_1483(const icd *i);
static void ex_1484(const icd *i);
static void ex_1485(const icd *i);
static void ex_1486(const icd *i);
static void ex_1487(const icd *i);
static void ex_1488(const icd *i);
static void ex_1489(const icd *i);
static void ex_1490(const icd *i);
static void ex_1491(const icd *i);
static void ex_1492(const icd *i);
static void ex_1493(const icd *i);
static void ex_1494(const icd *i);
static void ex_1495(const icd *i);
static void ex_1496(const icd *i);
static void ex_1497(const icd *i);
static void ex_1498(const icd *i);
static void ex_1499(const icd *i);
static void ex_1500(const icd *i);
static void ex_1501(const icd *i);
static void ex_1502(const icd *i);
static void ex_1503(const icd *i);
static void ex_1504(const icd *i);
static void ex_1505(const icd *i);
static void ex_1506(const icd *i);
static void ex_1507(const icd *i);
static void ex_1508(const icd *i);
static void ex_1509(const icd *i);
static void ex_1510(const icd *i);
static void ex_1511(const icd *i);
static void ex_1512(const icd *i);
static void ex_1513(const icd *i);
static void ex_1514(const icd *i);
static void ex_1515(const icd *i);
static void ex_1516(const icd *i);
static void ex_1517(const icd *i);
static void ex_1518(const icd *i);
static void ex_1519(const icd *i);
static void ex_1520(const icd *i);
static void ex_1521(const icd *i);
static void ex_1522(const icd *i);
static void ex_1523(const icd *i);
static void ex_1524(const icd *i);
static void ex_1525(const icd *i);
static void ex_1526(const icd *i);
static void ex_1527(const icd *i);
static void ex_1528(const icd *i);
static void ex_1529(const icd *i);
static void ex_1530(const icd *i);
static void ex_1531(const icd *i);
static void ex_1532(const icd *i);
static void ex_1533(const icd *i);
static void ex_1534(const icd *i);
static void ex_1535(const icd *i);
static void ex_1536(const icd *i);
static void ex_1537(const icd *i);
static void ex_1538(const icd *i);
static void ex_1539(const icd *i);
static void ex_1540(const icd *i);
static void ex_1541(const icd *i);
static void ex_1542(const icd *i);
static void ex_1543(const icd *i);
static void ex_1544(const icd *i);
static void ex_1545(const icd *i);
static void ex_1546(const icd *i);
static void ex_1547(const icd *i);
static void ex_1548(const icd *i);
static void ex_1549(const icd *i);
static void ex_1550(const icd *i);
static void ex_1551(const icd *i);
static void ex_1552(const icd *i);
static void ex_1553(const icd *i);
static void ex_1554(const icd *i);
static void ex_1555(const icd *i);
static void ex_1556(const icd *i);
static void ex_1557(const icd *i);
static void ex_1558(const icd *i);
static void ex_1559(const icd *i);
static void ex_1560(const icd *i);
static void ex_1561(const icd *i);
static void ex_1562(const icd *i);
static void ex_1563(const icd *i);
static void ex_1564(const icd *i);
static void ex_1565(const icd *i);
static void ex_1566(const icd *i);
static void ex_1567(const icd *i);
static void ex_1568(const icd *i);
static void ex_1569(const icd *i);
static void ex_1570(const icd *i);
static void ex_1571(const icd *i);
static void ex_1572(const icd *i);
static void ex_1573(const icd *i);
static void ex_1574(const icd *i);
static void ex_1575(const icd *i);
static void ex_1576(const icd *i);
static void ex_1577(const icd *i);
static void ex_1578(const icd *i);
static void ex_1579(const icd *i);
static void ex_1580(const icd *i);
static void ex_1581(const icd *i);
static void ex_1582(const icd *i);
static void ex_1583(const icd *i);
static void ex_1584(const icd *i);
static void ex_1585(const icd *i);
static void ex_1586(const icd *i);
static void ex_1587(const icd *i);
static void ex_1588(const icd *i);
static void ex_1589(const icd *i);
static void ex_1590(const icd *i);
static void ex_1591(const icd *i);
static void ex_1592(const icd *i);
static void ex_1593(const icd *i);
static void ex_1594(const icd *i);
static void ex_1595(const icd *i);
static void ex_1596(const icd *i);
static void ex_1597(const icd *i);
static void ex_1598(const icd *i);
static void ex_1599(const icd *i);
static void ex_1600(const icd *i);
static void ex_1601(const icd *i);
static void ex_1602(const icd *i);
static void ex_1603(const icd *i);
static void ex_1604(const icd *i);
static void ex_1605(const icd *i);
static void ex_1606(const icd *i);
static void ex_1607(const icd *i);
static void ex_1608(const icd *i);
static void ex_1609(const icd *i);
static void ex_1610(const icd *i);
static void ex_1611(const icd *i);
static void ex_1612(const icd *i);
static void ex_1613(const icd *i);
static void ex_1614(const icd *i);
static void ex_1615(const icd *i);
static void ex_1616(const icd *i);
static void ex_1617(const icd *i);
static void ex_1618(const icd *i);
static void ex_1619(const icd *i);
static void ex_1620(const icd *i);
static void ex_1621(const icd *i);
static void ex_1622(const icd *i);
static void ex_1623(const icd *i);
static void ex_1624(const icd *i);
static void ex_1625(const icd *i);
static void ex_1626(const icd *i);
static void ex_1627(const icd *i);
static void ex_1628(const icd *i);
static void ex_1629(const icd *i);
static void ex_1630(const icd *i);
static void ex_1631(const icd *i);
static void ex_1632(const icd *i);
static void ex_1633(const icd *i);
static void ex_1634(const icd *i);
static void ex_1635(const icd *i);
static void ex_1636(const icd *i);
static void ex_1637(const icd *i);
static void ex_1638(const icd *i);
static void ex_1639(const icd *i);
static void ex_1640(const icd *i);
static void ex_1641(const icd *i);
static void ex_1642(const icd *i);
static void ex_1643(const icd *i);
static void ex_1644(const icd *i);
static void ex_1645(const icd *i);
static void ex_1646(const icd *i);
static void ex_1647(const icd *i);
static void ex_1648(const icd *i);
static void ex_1649(const icd *i);
static void ex_1650(const icd *i);
static void ex_1651(const icd *i);
static void ex_1652(const icd *i);
static void ex_1653(const icd *i);
static void ex_1654(const icd *i);
static void ex_1655(const icd *i);
#endif
#ifdef CINTRPSWITCH
case 4: ex_4(i); break;
case 5: ex_5(i); break;
case 6: ex_6(i); break;
case 7: ex_7(i); break;
case 8: ex_8(i); break;
case 9: ex_9(i); break;
case 10: ex_10(i); break;
case 11: ex_11(i); break;
case 12: ex_12(i); break;
case 13: ex_13(i); break;
case 14: ex_14(i); break;
case 15: ex_15(i); break;
case 16: ex_16(i); break;
case 17: ex_17(i); break;
case 18: ex_18(i); break;
case 19: ex_19(i); break;
case 20: ex_20(i); break;
case 21: ex_21(i); break;
case 22: ex_22(i); break;
case 23: ex_23(i); break;
case 24: ex_24(i); break;
case 25: ex_25(i); break;
case 26: ex_26(i); break;
case 27: ex_27(i); break;
case 28: ex_28(i); break;
case 29: ex_29(i); break;
case 30: ex_30(i); break;
case 31: ex_31(i); break;
case 32: ex_32(i); break;
case 33: ex_33(i); break;
case 34: ex_34(i); break;
case 35: ex_35(i); break;
case 36: ex_36(i); break;
case 37: ex_37(i); break;
case 38: ex_38(i); break;
case 39: ex_39(i); break;
case 40: ex_40(i); break;
case 41: ex_41(i); break;
case 42: ex_42(i); break;
case 43: ex_43(i); break;
case 44: ex_44(i); break;
case 45: ex_45(i); break;
case 46: ex_46(i); break;
case 47: ex_47(i); break;
case 48: ex_48(i); break;
case 49: ex_49(i); break;
case 50: ex_50(i); break;
case 51: ex_51(i); break;
case 52: ex_52(i); break;
case 53: ex_53(i); break;
case 54: ex_54(i); break;
case 55: ex_55(i); break;
case 56: ex_56(i); break;
case 57: ex_57(i); break;
case 58: ex_58(i); break;
case 59: ex_59(i); break;
case 60: ex_60(i); break;
case 61: ex_61(i); break;
case 62: ex_62(i); break;
case 63: ex_63(i); break;
case 64: ex_64(i); break;
case 65: ex_65(i); break;
case 66: ex_66(i); break;
case 67: ex_67(i); break;
case 68: ex_68(i); break;
case 69: ex_69(i); break;
case 70: ex_70(i); break;
case 71: ex_71(i); break;
case 72: ex_72(i); break;
case 73: ex_73(i); break;
case 74: ex_74(i); break;
case 75: ex_75(i); break;
case 76: ex_76(i); break;
case 77: ex_77(i); break;
case 78: ex_78(i); break;
case 79: ex_79(i); break;
case 80: ex_80(i); break;
case 81: ex_81(i); break;
case 82: ex_82(i); break;
case 83: ex_83(i); break;
case 84: ex_84(i); break;
case 85: ex_85(i); break;
case 86: ex_86(i); break;
case 87: ex_87(i); break;
case 88: ex_88(i); break;
case 89: ex_89(i); break;
case 90: ex_90(i); break;
case 91: ex_91(i); break;
case 92: ex_92(i); break;
case 93: ex_93(i); break;
case 94: ex_94(i); break;
case 95: ex_95(i); break;
case 96: ex_96(i); break;
case 97: ex_97(i); break;
case 98: ex_98(i); break;
case 99: ex_99(i); break;
case 100: ex_100(i); break;
case 101: ex_101(i); break;
case 102: ex_102(i); break;
case 103: ex_103(i); break;
case 104: ex_104(i); break;
case 105: ex_105(i); break;
case 106: ex_106(i); break;
case 107: ex_107(i); break;
case 108: ex_108(i); break;
case 109: ex_109(i); break;
case 110: ex_110(i); break;
case 111: ex_111(i); break;
case 112: ex_112(i); break;
case 113: ex_113(i); break;
case 114: ex_114(i); break;
case 115: ex_115(i); break;
case 116: ex_116(i); break;
case 117: ex_117(i); break;
case 118: ex_118(i); break;
case 119: ex_119(i); break;
case 120: ex_120(i); break;
case 121: ex_121(i); break;
case 122: ex_122(i); break;
case 123: ex_123(i); break;
case 124: ex_124(i); break;
case 125: ex_125(i); break;
case 126: ex_126(i); break;
case 127: ex_127(i); break;
case 128: ex_128(i); break;
case 129: ex_129(i); break;
case 130: ex_130(i); break;
case 131: ex_131(i); break;
case 132: ex_132(i); break;
case 133: ex_133(i); break;
case 134: ex_134(i); break;
case 135: ex_135(i); break;
case 136: ex_136(i); break;
case 137: ex_137(i); break;
case 138: ex_138(i); break;
case 139: ex_139(i); break;
case 140: ex_140(i); break;
case 141: ex_141(i); break;
case 142: ex_142(i); break;
case 143: ex_143(i); break;
case 144: ex_144(i); break;
case 145: ex_145(i); break;
case 146: ex_146(i); break;
case 147: ex_147(i); break;
case 148: ex_148(i); break;
case 149: ex_149(i); break;
case 150: ex_150(i); break;
case 151: ex_151(i); break;
case 152: ex_152(i); break;
case 153: ex_153(i); break;
case 154: ex_154(i); break;
case 155: ex_155(i); break;
case 156: ex_156(i); break;
case 157: ex_157(i); break;
case 158: ex_158(i); break;
case 159: ex_159(i); break;
case 160: ex_160(i); break;
case 161: ex_161(i); break;
case 162: ex_162(i); break;
case 163: ex_163(i); break;
case 164: ex_164(i); break;
case 165: ex_165(i); break;
case 166: ex_166(i); break;
case 167: ex_167(i); break;
case 168: ex_168(i); break;
case 169: ex_169(i); break;
case 170: ex_170(i); break;
case 171: ex_171(i); break;
case 172: ex_172(i); break;
case 173: ex_173(i); break;
case 174: ex_174(i); break;
case 175: ex_175(i); break;
case 176: ex_176(i); break;
case 177: ex_177(i); break;
case 178: ex_178(i); break;
case 179: ex_179(i); break;
case 180: ex_180(i); break;
case 181: ex_181(i); break;
case 182: ex_182(i); break;
case 183: ex_183(i); break;
case 184: ex_184(i); break;
case 185: ex_185(i); break;
case 186: ex_186(i); break;
case 187: ex_187(i); break;
case 188: ex_188(i); break;
case 189: ex_189(i); break;
case 190: ex_190(i); break;
case 191: ex_191(i); break;
case 192: ex_192(i); break;
case 193: ex_193(i); break;
case 194: ex_194(i); break;
case 195: ex_195(i); break;
case 196: ex_196(i); break;
case 197: ex_197(i); break;
case 198: ex_198(i); break;
case 199: ex_199(i); break;
case 200: ex_200(i); break;
case 201: ex_201(i); break;
case 202: ex_202(i); break;
case 203: ex_203(i); break;
case 204: ex_204(i); break;
case 205: ex_205(i); break;
case 206: ex_206(i); break;
case 207: ex_207(i); break;
case 208: ex_208(i); break;
case 209: ex_209(i); break;
case 210: ex_210(i); break;
case 211: ex_211(i); break;
case 212: ex_212(i); break;
case 213: ex_213(i); break;
case 214: ex_214(i); break;
case 215: ex_215(i); break;
case 216: ex_216(i); break;
case 217: ex_217(i); break;
case 218: ex_218(i); break;
case 219: ex_219(i); break;
case 220: ex_220(i); break;
case 221: ex_221(i); break;
case 222: ex_222(i); break;
case 223: ex_223(i); break;
case 224: ex_224(i); break;
case 225: ex_225(i); break;
case 226: ex_226(i); break;
case 227: ex_227(i); break;
case 228: ex_228(i); break;
case 229: ex_229(i); break;
case 230: ex_230(i); break;
case 231: ex_231(i); break;
case 232: ex_232(i); break;
case 233: ex_233(i); break;
case 234: ex_234(i); break;
case 235: ex_235(i); break;
case 236: ex_236(i); break;
case 237: ex_237(i); break;
case 238: ex_238(i); break;
case 239: ex_239(i); break;
case 240: ex_240(i); break;
case 241: ex_241(i); break;
case 242: ex_242(i); break;
case 243: ex_243(i); break;
case 244: ex_244(i); break;
case 245: ex_245(i); break;
case 246: ex_246(i); break;
case 247: ex_247(i); break;
case 248: ex_248(i); break;
case 249: ex_249(i); break;
case 250: ex_250(i); break;
case 251: ex_251(i); break;
case 252: ex_252(i); break;
case 253: ex_253(i); break;
case 254: ex_254(i); break;
case 255: ex_255(i); break;
case 256: ex_256(i); break;
case 257: ex_257(i); break;
case 258: ex_258(i); break;
case 259: ex_259(i); break;
case 260: ex_260(i); break;
case 261: ex_261(i); break;
case 262: ex_262(i); break;
case 263: ex_263(i); break;
case 264: ex_264(i); break;
case 265: ex_265(i); break;
case 266: ex_266(i); break;
case 267: ex_267(i); break;
case 268: ex_268(i); break;
case 269: ex_269(i); break;
case 270: ex_270(i); break;
case 271: ex_271(i); break;
case 272: ex_272(i); break;
case 273: ex_273(i); break;
case 274: ex_274(i); break;
case 275: ex_275(i); break;
case 276: ex_276(i); break;
case 277: ex_277(i); break;
case 278: ex_278(i); break;
case 279: ex_279(i); break;
case 280: ex_280(i); break;
case 281: ex_281(i); break;
case 282: ex_282(i); break;
case 283: ex_283(i); break;
case 284: ex_284(i); break;
case 285: ex_285(i); break;
case 286: ex_286(i); break;
case 287: ex_287(i); break;
case 288: ex_288(i); break;
case 289: ex_289(i); break;
case 290: ex_290(i); break;
case 291: ex_291(i); break;
case 292: ex_292(i); break;
case 293: ex_293(i); break;
case 294: ex_294(i); break;
case 295: ex_295(i); break;
case 296: ex_296(i); break;
case 297: ex_297(i); break;
case 298: ex_298(i); break;
case 299: ex_299(i); break;
case 300: ex_300(i); break;
case 301: ex_301(i); break;
case 302: ex_302(i); break;
case 303: ex_303(i); break;
case 304: ex_304(i); break;
case 305: ex_305(i); break;
case 306: ex_306(i); break;
case 307: ex_307(i); break;
case 308: ex_308(i); break;
case 309: ex_309(i); break;
case 310: ex_310(i); break;
case 311: ex_311(i); break;
case 312: ex_312(i); break;
case 313: ex_313(i); break;
case 314: ex_314(i); break;
case 315: ex_315(i); break;
case 316: ex_316(i); break;
case 317: ex_317(i); break;
case 318: ex_318(i); break;
case 319: ex_319(i); break;
case 320: ex_320(i); break;
case 321: ex_321(i); break;
case 322: ex_322(i); break;
case 323: ex_323(i); break;
case 324: ex_324(i); break;
case 325: ex_325(i); break;
case 326: ex_326(i); break;
case 327: ex_327(i); break;
case 328: ex_328(i); break;
case 329: ex_329(i); break;
case 330: ex_330(i); break;
case 331: ex_331(i); break;
case 332: ex_332(i); break;
case 333: ex_333(i); break;
case 334: ex_334(i); break;
case 335: ex_335(i); break;
case 336: ex_336(i); break;
case 337: ex_337(i); break;
case 338: ex_338(i); break;
case 339: ex_339(i); break;
case 340: ex_340(i); break;
case 341: ex_341(i); break;
case 342: ex_342(i); break;
case 343: ex_343(i); break;
case 344: ex_344(i); break;
case 345: ex_345(i); break;
case 346: ex_346(i); break;
case 347: ex_347(i); break;
case 348: ex_348(i); break;
case 349: ex_349(i); break;
case 350: ex_350(i); break;
case 351: ex_351(i); break;
case 352: ex_352(i); break;
case 353: ex_353(i); break;
case 354: ex_354(i); break;
case 355: ex_355(i); break;
case 356: ex_356(i); break;
case 357: ex_357(i); break;
case 358: ex_358(i); break;
case 359: ex_359(i); break;
case 360: ex_360(i); break;
case 361: ex_361(i); break;
case 362: ex_362(i); break;
case 363: ex_363(i); break;
case 364: ex_364(i); break;
case 365: ex_365(i); break;
case 366: ex_366(i); break;
case 367: ex_367(i); break;
case 368: ex_368(i); break;
case 369: ex_369(i); break;
case 370: ex_370(i); break;
case 371: ex_371(i); break;
case 372: ex_372(i); break;
case 373: ex_373(i); break;
case 374: ex_374(i); break;
case 375: ex_375(i); break;
case 376: ex_376(i); break;
case 377: ex_377(i); break;
case 378: ex_378(i); break;
case 379: ex_379(i); break;
case 380: ex_380(i); break;
case 381: ex_381(i); break;
case 382: ex_382(i); break;
case 383: ex_383(i); break;
case 384: ex_384(i); break;
case 385: ex_385(i); break;
case 386: ex_386(i); break;
case 387: ex_387(i); break;
case 388: ex_388(i); break;
case 389: ex_389(i); break;
case 390: ex_390(i); break;
case 391: ex_391(i); break;
case 392: ex_392(i); break;
case 393: ex_393(i); break;
case 394: ex_394(i); break;
case 395: ex_395(i); break;
case 396: ex_396(i); break;
case 397: ex_397(i); break;
case 398: ex_398(i); break;
case 399: ex_399(i); break;
case 400: ex_400(i); break;
case 401: ex_401(i); break;
case 402: ex_402(i); break;
case 403: ex_403(i); break;
case 404: ex_404(i); break;
case 405: ex_405(i); break;
case 406: ex_406(i); break;
case 407: ex_407(i); break;
case 408: ex_408(i); break;
case 409: ex_409(i); break;
case 410: ex_410(i); break;
case 411: ex_411(i); break;
case 412: ex_412(i); break;
case 413: ex_413(i); break;
case 414: ex_414(i); break;
case 415: ex_415(i); break;
case 416: ex_416(i); break;
case 417: ex_417(i); break;
case 418: ex_418(i); break;
case 419: ex_419(i); break;
case 420: ex_420(i); break;
case 421: ex_421(i); break;
case 422: ex_422(i); break;
case 423: ex_423(i); break;
case 424: ex_424(i); break;
case 425: ex_425(i); break;
case 426: ex_426(i); break;
case 427: ex_427(i); break;
case 428: ex_428(i); break;
case 429: ex_429(i); break;
case 430: ex_430(i); break;
case 431: ex_431(i); break;
case 432: ex_432(i); break;
case 433: ex_433(i); break;
case 434: ex_434(i); break;
case 435: ex_435(i); break;
case 436: ex_436(i); break;
case 437: ex_437(i); break;
case 438: ex_438(i); break;
case 439: ex_439(i); break;
case 440: ex_440(i); break;
case 441: ex_441(i); break;
case 442: ex_442(i); break;
case 443: ex_443(i); break;
case 444: ex_444(i); break;
case 445: ex_445(i); break;
case 446: ex_446(i); break;
case 447: ex_447(i); break;
case 448: ex_448(i); break;
case 449: ex_449(i); break;
case 450: ex_450(i); break;
case 451: ex_451(i); break;
case 452: ex_452(i); break;
case 453: ex_453(i); break;
case 454: ex_454(i); break;
case 455: ex_455(i); break;
case 456: ex_456(i); break;
case 457: ex_457(i); break;
case 458: ex_458(i); break;
case 459: ex_459(i); break;
case 460: ex_460(i); break;
case 461: ex_461(i); break;
case 462: ex_462(i); break;
case 463: ex_463(i); break;
case 464: ex_464(i); break;
case 465: ex_465(i); break;
case 466: ex_466(i); break;
case 467: ex_467(i); break;
case 468: ex_468(i); break;
case 469: ex_469(i); break;
case 470: ex_470(i); break;
case 471: ex_471(i); break;
case 472: ex_472(i); break;
case 473: ex_473(i); break;
case 474: ex_474(i); break;
case 475: ex_475(i); break;
case 476: ex_476(i); break;
case 477: ex_477(i); break;
case 478: ex_478(i); break;
case 479: ex_479(i); break;
case 480: ex_480(i); break;
case 481: ex_481(i); break;
case 482: ex_482(i); break;
case 483: ex_483(i); break;
case 484: ex_484(i); break;
case 485: ex_485(i); break;
case 486: ex_486(i); break;
case 487: ex_487(i); break;
case 488: ex_488(i); break;
case 489: ex_489(i); break;
case 490: ex_490(i); break;
case 491: ex_491(i); break;
case 492: ex_492(i); break;
case 493: ex_493(i); break;
case 494: ex_494(i); break;
case 495: ex_495(i); break;
case 496: ex_496(i); break;
case 497: ex_497(i); break;
case 498: ex_498(i); break;
case 499: ex_499(i); break;
case 500: ex_500(i); break;
case 501: ex_501(i); break;
case 502: ex_502(i); break;
case 503: ex_503(i); break;
case 504: ex_504(i); break;
case 505: ex_505(i); break;
case 506: ex_506(i); break;
case 507: ex_507(i); break;
case 508: ex_508(i); break;
case 509: ex_509(i); break;
case 510: ex_510(i); break;
case 511: ex_511(i); break;
case 512: ex_512(i); break;
case 513: ex_513(i); break;
case 514: ex_514(i); break;
case 515: ex_515(i); break;
case 516: ex_516(i); break;
case 517: ex_517(i); break;
case 518: ex_518(i); break;
case 519: ex_519(i); break;
case 520: ex_520(i); break;
case 521: ex_521(i); break;
case 522: ex_522(i); break;
case 523: ex_523(i); break;
case 524: ex_524(i); break;
case 525: ex_525(i); break;
case 526: ex_526(i); break;
case 527: ex_527(i); break;
case 528: ex_528(i); break;
case 529: ex_529(i); break;
case 530: ex_530(i); break;
case 531: ex_531(i); break;
case 532: ex_532(i); break;
case 533: ex_533(i); break;
case 534: ex_534(i); break;
case 535: ex_535(i); break;
case 536: ex_536(i); break;
case 537: ex_537(i); break;
case 538: ex_538(i); break;
case 539: ex_539(i); break;
case 540: ex_540(i); break;
case 541: ex_541(i); break;
case 542: ex_542(i); break;
case 543: ex_543(i); break;
case 544: ex_544(i); break;
case 545: ex_545(i); break;
case 546: ex_546(i); break;
case 547: ex_547(i); break;
case 548: ex_548(i); break;
case 549: ex_549(i); break;
case 550: ex_550(i); break;
case 551: ex_551(i); break;
case 552: ex_552(i); break;
case 553: ex_553(i); break;
case 554: ex_554(i); break;
case 555: ex_555(i); break;
case 556: ex_556(i); break;
case 557: ex_557(i); break;
case 558: ex_558(i); break;
case 559: ex_559(i); break;
case 560: ex_560(i); break;
case 561: ex_561(i); break;
case 562: ex_562(i); break;
case 563: ex_563(i); break;
case 564: ex_564(i); break;
case 565: ex_565(i); break;
case 566: ex_566(i); break;
case 567: ex_567(i); break;
case 568: ex_568(i); break;
case 569: ex_569(i); break;
case 570: ex_570(i); break;
case 571: ex_571(i); break;
case 572: ex_572(i); break;
case 573: ex_573(i); break;
case 574: ex_574(i); break;
case 575: ex_575(i); break;
case 576: ex_576(i); break;
case 577: ex_577(i); break;
case 578: ex_578(i); break;
case 579: ex_579(i); break;
case 580: ex_580(i); break;
case 581: ex_581(i); break;
case 582: ex_582(i); break;
case 583: ex_583(i); break;
case 584: ex_584(i); break;
case 585: ex_585(i); break;
case 586: ex_586(i); break;
case 587: ex_587(i); break;
case 588: ex_588(i); break;
case 589: ex_589(i); break;
case 590: ex_590(i); break;
case 591: ex_591(i); break;
case 592: ex_592(i); break;
case 593: ex_593(i); break;
case 594: ex_594(i); break;
case 595: ex_595(i); break;
case 596: ex_596(i); break;
case 597: ex_597(i); break;
case 598: ex_598(i); break;
case 599: ex_599(i); break;
case 600: ex_600(i); break;
case 601: ex_601(i); break;
case 602: ex_602(i); break;
case 603: ex_603(i); break;
case 604: ex_604(i); break;
case 605: ex_605(i); break;
case 606: ex_606(i); break;
case 607: ex_607(i); break;
case 608: ex_608(i); break;
case 609: ex_609(i); break;
case 610: ex_610(i); break;
case 611: ex_611(i); break;
case 612: ex_612(i); break;
case 613: ex_613(i); break;
case 614: ex_614(i); break;
case 615: ex_615(i); break;
case 616: ex_616(i); break;
case 617: ex_617(i); break;
case 618: ex_618(i); break;
case 619: ex_619(i); break;
case 620: ex_620(i); break;
case 621: ex_621(i); break;
case 622: ex_622(i); break;
case 623: ex_623(i); break;
case 624: ex_624(i); break;
case 625: ex_625(i); break;
case 626: ex_626(i); break;
case 627: ex_627(i); break;
case 628: ex_628(i); break;
case 629: ex_629(i); break;
case 630: ex_630(i); break;
case 631: ex_631(i); break;
case 632: ex_632(i); break;
case 633: ex_633(i); break;
case 634: ex_634(i); break;
case 635: ex_635(i); break;
case 636: ex_636(i); break;
case 637: ex_637(i); break;
case 638: ex_638(i); break;
case 639: ex_639(i); break;
case 640: ex_640(i); break;
case 641: ex_641(i); break;
case 642: ex_642(i); break;
case 643: ex_643(i); break;
case 644: ex_644(i); break;
case 645: ex_645(i); break;
case 646: ex_646(i); break;
case 647: ex_647(i); break;
case 648: ex_648(i); break;
case 649: ex_649(i); break;
case 650: ex_650(i); break;
case 651: ex_651(i); break;
case 652: ex_652(i); break;
case 653: ex_653(i); break;
case 654: ex_654(i); break;
case 655: ex_655(i); break;
case 656: ex_656(i); break;
case 657: ex_657(i); break;
case 658: ex_658(i); break;
case 659: ex_659(i); break;
case 660: ex_660(i); break;
case 661: ex_661(i); break;
case 662: ex_662(i); break;
case 663: ex_663(i); break;
case 664: ex_664(i); break;
case 665: ex_665(i); break;
case 666: ex_666(i); break;
case 667: ex_667(i); break;
case 668: ex_668(i); break;
case 669: ex_669(i); break;
case 670: ex_670(i); break;
case 671: ex_671(i); break;
case 672: ex_672(i); break;
case 673: ex_673(i); break;
case 674: ex_674(i); break;
case 675: ex_675(i); break;
case 676: ex_676(i); break;
case 677: ex_677(i); break;
case 678: ex_678(i); break;
case 679: ex_679(i); break;
case 680: ex_680(i); break;
case 681: ex_681(i); break;
case 682: ex_682(i); break;
case 683: ex_683(i); break;
case 684: ex_684(i); break;
case 685: ex_685(i); break;
case 686: ex_686(i); break;
case 687: ex_687(i); break;
case 688: ex_688(i); break;
case 689: ex_689(i); break;
case 690: ex_690(i); break;
case 691: ex_691(i); break;
case 692: ex_692(i); break;
case 693: ex_693(i); break;
case 694: ex_694(i); break;
case 695: ex_695(i); break;
case 696: ex_696(i); break;
case 697: ex_697(i); break;
case 698: ex_698(i); break;
case 699: ex_699(i); break;
case 700: ex_700(i); break;
case 701: ex_701(i); break;
case 702: ex_702(i); break;
case 703: ex_703(i); break;
case 704: ex_704(i); break;
case 705: ex_705(i); break;
case 706: ex_706(i); break;
case 707: ex_707(i); break;
case 708: ex_708(i); break;
case 709: ex_709(i); break;
case 710: ex_710(i); break;
case 711: ex_711(i); break;
case 712: ex_712(i); break;
case 713: ex_713(i); break;
case 714: ex_714(i); break;
case 715: ex_715(i); break;
case 716: ex_716(i); break;
case 717: ex_717(i); break;
case 718: ex_718(i); break;
case 719: ex_719(i); break;
case 720: ex_720(i); break;
case 721: ex_721(i); break;
case 722: ex_722(i); break;
case 723: ex_723(i); break;
case 724: ex_724(i); break;
case 725: ex_725(i); break;
case 726: ex_726(i); break;
case 727: ex_727(i); break;
case 728: ex_728(i); break;
case 729: ex_729(i); break;
case 730: ex_730(i); break;
case 731: ex_731(i); break;
case 732: ex_732(i); break;
case 733: ex_733(i); break;
case 734: ex_734(i); break;
case 735: ex_735(i); break;
case 736: ex_736(i); break;
case 737: ex_737(i); break;
case 738: ex_738(i); break;
case 739: ex_739(i); break;
case 740: ex_740(i); break;
case 741: ex_741(i); break;
case 742: ex_742(i); break;
case 743: ex_743(i); break;
case 744: ex_744(i); break;
case 745: ex_745(i); break;
case 746: ex_746(i); break;
case 747: ex_747(i); break;
case 748: ex_748(i); break;
case 749: ex_749(i); break;
case 750: ex_750(i); break;
case 751: ex_751(i); break;
case 752: ex_752(i); break;
case 753: ex_753(i); break;
case 754: ex_754(i); break;
case 755: ex_755(i); break;
case 756: ex_756(i); break;
case 757: ex_757(i); break;
case 758: ex_758(i); break;
case 759: ex_759(i); break;
case 760: ex_760(i); break;
case 761: ex_761(i); break;
case 762: ex_762(i); break;
case 763: ex_763(i); break;
case 764: ex_764(i); break;
case 765: ex_765(i); break;
case 766: ex_766(i); break;
case 767: ex_767(i); break;
case 768: ex_768(i); break;
case 769: ex_769(i); break;
case 770: ex_770(i); break;
case 771: ex_771(i); break;
case 772: ex_772(i); break;
case 773: ex_773(i); break;
case 774: ex_774(i); break;
case 775: ex_775(i); break;
case 776: ex_776(i); break;
case 777: ex_777(i); break;
case 778: ex_778(i); break;
case 779: ex_779(i); break;
case 780: ex_780(i); break;
case 781: ex_781(i); break;
case 782: ex_782(i); break;
case 783: ex_783(i); break;
case 784: ex_784(i); break;
case 785: ex_785(i); break;
case 786: ex_786(i); break;
case 787: ex_787(i); break;
case 788: ex_788(i); break;
case 789: ex_789(i); break;
case 790: ex_790(i); break;
case 791: ex_791(i); break;
case 792: ex_792(i); break;
case 793: ex_793(i); break;
case 794: ex_794(i); break;
case 795: ex_795(i); break;
case 796: ex_796(i); break;
case 797: ex_797(i); break;
case 798: ex_798(i); break;
case 799: ex_799(i); break;
case 800: ex_800(i); break;
case 801: ex_801(i); break;
case 802: ex_802(i); break;
case 803: ex_803(i); break;
case 804: ex_804(i); break;
case 805: ex_805(i); break;
case 806: ex_806(i); break;
case 807: ex_807(i); break;
case 808: ex_808(i); break;
case 809: ex_809(i); break;
case 810: ex_810(i); break;
case 811: ex_811(i); break;
case 812: ex_812(i); break;
case 813: ex_813(i); break;
case 814: ex_814(i); break;
case 815: ex_815(i); break;
case 816: ex_816(i); break;
case 817: ex_817(i); break;
case 818: ex_818(i); break;
case 819: ex_819(i); break;
case 820: ex_820(i); break;
case 821: ex_821(i); break;
case 822: ex_822(i); break;
case 823: ex_823(i); break;
case 824: ex_824(i); break;
case 825: ex_825(i); break;
case 826: ex_826(i); break;
case 827: ex_827(i); break;
case 828: ex_828(i); break;
case 829: ex_829(i); break;
case 830: ex_830(i); break;
case 831: ex_831(i); break;
case 832: ex_832(i); break;
case 833: ex_833(i); break;
case 834: ex_834(i); break;
case 835: ex_835(i); break;
case 836: ex_836(i); break;
case 837: ex_837(i); break;
case 838: ex_838(i); break;
case 839: ex_839(i); break;
case 840: ex_840(i); break;
case 841: ex_841(i); break;
case 842: ex_842(i); break;
case 843: ex_843(i); break;
case 844: ex_844(i); break;
case 845: ex_845(i); break;
case 846: ex_846(i); break;
case 847: ex_847(i); break;
case 848: ex_848(i); break;
case 849: ex_849(i); break;
case 850: ex_850(i); break;
case 851: ex_851(i); break;
case 852: ex_852(i); break;
case 853: ex_853(i); break;
case 854: ex_854(i); break;
case 855: ex_855(i); break;
case 856: ex_856(i); break;
case 857: ex_857(i); break;
case 858: ex_858(i); break;
case 859: ex_859(i); break;
case 860: ex_860(i); break;
case 861: ex_861(i); break;
case 862: ex_862(i); break;
case 863: ex_863(i); break;
case 864: ex_864(i); break;
case 865: ex_865(i); break;
case 866: ex_866(i); break;
case 867: ex_867(i); break;
case 868: ex_868(i); break;
case 869: ex_869(i); break;
case 870: ex_870(i); break;
case 871: ex_871(i); break;
case 872: ex_872(i); break;
case 873: ex_873(i); break;
case 874: ex_874(i); break;
case 875: ex_875(i); break;
case 876: ex_876(i); break;
case 877: ex_877(i); break;
case 878: ex_878(i); break;
case 879: ex_879(i); break;
case 880: ex_880(i); break;
case 881: ex_881(i); break;
case 882: ex_882(i); break;
case 883: ex_883(i); break;
case 884: ex_884(i); break;
case 885: ex_885(i); break;
case 886: ex_886(i); break;
case 887: ex_887(i); break;
case 888: ex_888(i); break;
case 889: ex_889(i); break;
case 890: ex_890(i); break;
case 891: ex_891(i); break;
case 892: ex_892(i); break;
case 893: ex_893(i); break;
case 894: ex_894(i); break;
case 895: ex_895(i); break;
case 896: ex_896(i); break;
case 897: ex_897(i); break;
case 898: ex_898(i); break;
case 899: ex_899(i); break;
case 900: ex_900(i); break;
case 901: ex_901(i); break;
case 902: ex_902(i); break;
case 903: ex_903(i); break;
case 904: ex_904(i); break;
case 905: ex_905(i); break;
case 906: ex_906(i); break;
case 907: ex_907(i); break;
case 908: ex_908(i); break;
case 909: ex_909(i); break;
case 910: ex_910(i); break;
case 911: ex_911(i); break;
case 912: ex_912(i); break;
case 913: ex_913(i); break;
case 914: ex_914(i); break;
case 915: ex_915(i); break;
case 916: ex_916(i); break;
case 917: ex_917(i); break;
case 918: ex_918(i); break;
case 919: ex_919(i); break;
case 920: ex_920(i); break;
case 921: ex_921(i); break;
case 922: ex_922(i); break;
case 923: ex_923(i); break;
case 924: ex_924(i); break;
case 925: ex_925(i); break;
case 926: ex_926(i); break;
case 927: ex_927(i); break;
case 928: ex_928(i); break;
case 929: ex_929(i); break;
case 930: ex_930(i); break;
case 931: ex_931(i); break;
case 932: ex_932(i); break;
case 933: ex_933(i); break;
case 934: ex_934(i); break;
case 935: ex_935(i); break;
case 936: ex_936(i); break;
case 937: ex_937(i); break;
case 938: ex_938(i); break;
case 939: ex_939(i); break;
case 940: ex_940(i); break;
case 941: ex_941(i); break;
case 942: ex_942(i); break;
case 943: ex_943(i); break;
case 944: ex_944(i); break;
case 945: ex_945(i); break;
case 946: ex_946(i); break;
case 947: ex_947(i); break;
case 948: ex_948(i); break;
case 949: ex_949(i); break;
case 950: ex_950(i); break;
case 951: ex_951(i); break;
case 952: ex_952(i); break;
case 953: ex_953(i); break;
case 954: ex_954(i); break;
case 955: ex_955(i); break;
case 956: ex_956(i); break;
case 957: ex_957(i); break;
case 958: ex_958(i); break;
case 959: ex_959(i); break;
case 960: ex_960(i); break;
case 961: ex_961(i); break;
case 962: ex_962(i); break;
case 963: ex_963(i); break;
case 964: ex_964(i); break;
case 965: ex_965(i); break;
case 966: ex_966(i); break;
case 967: ex_967(i); break;
case 968: ex_968(i); break;
case 969: ex_969(i); break;
case 970: ex_970(i); break;
case 971: ex_971(i); break;
case 972: ex_972(i); break;
case 973: ex_973(i); break;
case 974: ex_974(i); break;
case 975: ex_975(i); break;
case 976: ex_976(i); break;
case 977: ex_977(i); break;
case 978: ex_978(i); break;
case 979: ex_979(i); break;
case 980: ex_980(i); break;
case 981: ex_981(i); break;
case 982: ex_982(i); break;
case 983: ex_983(i); break;
case 984: ex_984(i); break;
case 985: ex_985(i); break;
case 986: ex_986(i); break;
case 987: ex_987(i); break;
case 988: ex_988(i); break;
case 989: ex_989(i); break;
case 990: ex_990(i); break;
case 991: ex_991(i); break;
case 992: ex_992(i); break;
case 993: ex_993(i); break;
case 994: ex_994(i); break;
case 995: ex_995(i); break;
case 996: ex_996(i); break;
case 997: ex_997(i); break;
case 998: ex_998(i); break;
case 999: ex_999(i); break;
case 1000: ex_1000(i); break;
case 1001: ex_1001(i); break;
case 1002: ex_1002(i); break;
case 1003: ex_1003(i); break;
case 1004: ex_1004(i); break;
case 1005: ex_1005(i); break;
case 1006: ex_1006(i); break;
case 1007: ex_1007(i); break;
case 1008: ex_1008(i); break;
case 1009: ex_1009(i); break;
case 1010: ex_1010(i); break;
case 1011: ex_1011(i); break;
case 1012: ex_1012(i); break;
case 1013: ex_1013(i); break;
case 1014: ex_1014(i); break;
case 1015: ex_1015(i); break;
case 1016: ex_1016(i); break;
case 1017: ex_1017(i); break;
case 1018: ex_1018(i); break;
case 1019: ex_1019(i); break;
case 1020: ex_1020(i); break;
case 1021: ex_1021(i); break;
case 1022: ex_1022(i); break;
case 1023: ex_1023(i); break;
case 1024: ex_1024(i); break;
case 1025: ex_1025(i); break;
case 1026: ex_1026(i); break;
case 1027: ex_1027(i); break;
case 1028: ex_1028(i); break;
case 1029: ex_1029(i); break;
case 1030: ex_1030(i); break;
case 1031: ex_1031(i); break;
case 1032: ex_1032(i); break;
case 1033: ex_1033(i); break;
case 1034: ex_1034(i); break;
case 1035: ex_1035(i); break;
case 1036: ex_1036(i); break;
case 1037: ex_1037(i); break;
case 1038: ex_1038(i); break;
case 1039: ex_1039(i); break;
case 1040: ex_1040(i); break;
case 1041: ex_1041(i); break;
case 1042: ex_1042(i); break;
case 1043: ex_1043(i); break;
case 1044: ex_1044(i); break;
case 1045: ex_1045(i); break;
case 1046: ex_1046(i); break;
case 1047: ex_1047(i); break;
case 1048: ex_1048(i); break;
case 1049: ex_1049(i); break;
case 1050: ex_1050(i); break;
case 1051: ex_1051(i); break;
case 1052: ex_1052(i); break;
case 1053: ex_1053(i); break;
case 1054: ex_1054(i); break;
case 1055: ex_1055(i); break;
case 1056: ex_1056(i); break;
case 1057: ex_1057(i); break;
case 1058: ex_1058(i); break;
case 1059: ex_1059(i); break;
case 1060: ex_1060(i); break;
case 1061: ex_1061(i); break;
case 1062: ex_1062(i); break;
case 1063: ex_1063(i); break;
case 1064: ex_1064(i); break;
case 1065: ex_1065(i); break;
case 1066: ex_1066(i); break;
case 1067: ex_1067(i); break;
case 1068: ex_1068(i); break;
case 1069: ex_1069(i); break;
case 1070: ex_1070(i); break;
case 1071: ex_1071(i); break;
case 1072: ex_1072(i); break;
case 1073: ex_1073(i); break;
case 1074: ex_1074(i); break;
case 1075: ex_1075(i); break;
case 1076: ex_1076(i); break;
case 1077: ex_1077(i); break;
case 1078: ex_1078(i); break;
case 1079: ex_1079(i); break;
case 1080: ex_1080(i); break;
case 1081: ex_1081(i); break;
case 1082: ex_1082(i); break;
case 1083: ex_1083(i); break;
case 1084: ex_1084(i); break;
case 1085: ex_1085(i); break;
case 1086: ex_1086(i); break;
case 1087: ex_1087(i); break;
case 1088: ex_1088(i); break;
case 1089: ex_1089(i); break;
case 1090: ex_1090(i); break;
case 1091: ex_1091(i); break;
case 1092: ex_1092(i); break;
case 1093: ex_1093(i); break;
case 1094: ex_1094(i); break;
case 1095: ex_1095(i); break;
case 1096: ex_1096(i); break;
case 1097: ex_1097(i); break;
case 1098: ex_1098(i); break;
case 1099: ex_1099(i); break;
case 1100: ex_1100(i); break;
case 1101: ex_1101(i); break;
case 1102: ex_1102(i); break;
case 1103: ex_1103(i); break;
case 1104: ex_1104(i); break;
case 1105: ex_1105(i); break;
case 1106: ex_1106(i); break;
case 1107: ex_1107(i); break;
case 1108: ex_1108(i); break;
case 1109: ex_1109(i); break;
case 1110: ex_1110(i); break;
case 1111: ex_1111(i); break;
case 1112: ex_1112(i); break;
case 1113: ex_1113(i); break;
case 1114: ex_1114(i); break;
case 1115: ex_1115(i); break;
case 1116: ex_1116(i); break;
case 1117: ex_1117(i); break;
case 1118: ex_1118(i); break;
case 1119: ex_1119(i); break;
case 1120: ex_1120(i); break;
case 1121: ex_1121(i); break;
case 1122: ex_1122(i); break;
case 1123: ex_1123(i); break;
case 1124: ex_1124(i); break;
case 1125: ex_1125(i); break;
case 1126: ex_1126(i); break;
case 1127: ex_1127(i); break;
case 1128: ex_1128(i); break;
case 1129: ex_1129(i); break;
case 1130: ex_1130(i); break;
case 1131: ex_1131(i); break;
case 1132: ex_1132(i); break;
case 1133: ex_1133(i); break;
case 1134: ex_1134(i); break;
case 1135: ex_1135(i); break;
case 1136: ex_1136(i); break;
case 1137: ex_1137(i); break;
case 1138: ex_1138(i); break;
case 1139: ex_1139(i); break;
case 1140: ex_1140(i); break;
case 1141: ex_1141(i); break;
case 1142: ex_1142(i); break;
case 1143: ex_1143(i); break;
case 1144: ex_1144(i); break;
case 1145: ex_1145(i); break;
case 1146: ex_1146(i); break;
case 1147: ex_1147(i); break;
case 1148: ex_1148(i); break;
case 1149: ex_1149(i); break;
case 1150: ex_1150(i); break;
case 1151: ex_1151(i); break;
case 1152: ex_1152(i); break;
case 1153: ex_1153(i); break;
case 1154: ex_1154(i); break;
case 1155: ex_1155(i); break;
case 1156: ex_1156(i); break;
case 1157: ex_1157(i); break;
case 1158: ex_1158(i); break;
case 1159: ex_1159(i); break;
case 1160: ex_1160(i); break;
case 1161: ex_1161(i); break;
case 1162: ex_1162(i); break;
case 1163: ex_1163(i); break;
case 1164: ex_1164(i); break;
case 1165: ex_1165(i); break;
case 1166: ex_1166(i); break;
case 1167: ex_1167(i); break;
case 1168: ex_1168(i); break;
case 1169: ex_1169(i); break;
case 1170: ex_1170(i); break;
case 1171: ex_1171(i); break;
case 1172: ex_1172(i); break;
case 1173: ex_1173(i); break;
case 1174: ex_1174(i); break;
case 1175: ex_1175(i); break;
case 1176: ex_1176(i); break;
case 1177: ex_1177(i); break;
case 1178: ex_1178(i); break;
case 1179: ex_1179(i); break;
case 1180: ex_1180(i); break;
case 1181: ex_1181(i); break;
case 1182: ex_1182(i); break;
case 1183: ex_1183(i); break;
case 1184: ex_1184(i); break;
case 1185: ex_1185(i); break;
case 1186: ex_1186(i); break;
case 1187: ex_1187(i); break;
case 1188: ex_1188(i); break;
case 1189: ex_1189(i); break;
case 1190: ex_1190(i); break;
case 1191: ex_1191(i); break;
case 1192: ex_1192(i); break;
case 1193: ex_1193(i); break;
case 1194: ex_1194(i); break;
case 1195: ex_1195(i); break;
case 1196: ex_1196(i); break;
case 1197: ex_1197(i); break;
case 1198: ex_1198(i); break;
case 1199: ex_1199(i); break;
case 1200: ex_1200(i); break;
case 1201: ex_1201(i); break;
case 1202: ex_1202(i); break;
case 1203: ex_1203(i); break;
case 1204: ex_1204(i); break;
case 1205: ex_1205(i); break;
case 1206: ex_1206(i); break;
case 1207: ex_1207(i); break;
case 1208: ex_1208(i); break;
case 1209: ex_1209(i); break;
case 1210: ex_1210(i); break;
case 1211: ex_1211(i); break;
case 1212: ex_1212(i); break;
case 1213: ex_1213(i); break;
case 1214: ex_1214(i); break;
case 1215: ex_1215(i); break;
case 1216: ex_1216(i); break;
case 1217: ex_1217(i); break;
case 1218: ex_1218(i); break;
case 1219: ex_1219(i); break;
case 1220: ex_1220(i); break;
case 1221: ex_1221(i); break;
case 1222: ex_1222(i); break;
case 1223: ex_1223(i); break;
case 1224: ex_1224(i); break;
case 1225: ex_1225(i); break;
case 1226: ex_1226(i); break;
case 1227: ex_1227(i); break;
case 1228: ex_1228(i); break;
case 1229: ex_1229(i); break;
case 1230: ex_1230(i); break;
case 1231: ex_1231(i); break;
case 1232: ex_1232(i); break;
case 1233: ex_1233(i); break;
case 1234: ex_1234(i); break;
case 1235: ex_1235(i); break;
case 1236: ex_1236(i); break;
case 1237: ex_1237(i); break;
case 1238: ex_1238(i); break;
case 1239: ex_1239(i); break;
case 1240: ex_1240(i); break;
case 1241: ex_1241(i); break;
case 1242: ex_1242(i); break;
case 1243: ex_1243(i); break;
case 1244: ex_1244(i); break;
case 1245: ex_1245(i); break;
case 1246: ex_1246(i); break;
case 1247: ex_1247(i); break;
case 1248: ex_1248(i); break;
case 1249: ex_1249(i); break;
case 1250: ex_1250(i); break;
case 1251: ex_1251(i); break;
case 1252: ex_1252(i); break;
case 1253: ex_1253(i); break;
case 1254: ex_1254(i); break;
case 1255: ex_1255(i); break;
case 1256: ex_1256(i); break;
case 1257: ex_1257(i); break;
case 1258: ex_1258(i); break;
case 1259: ex_1259(i); break;
case 1260: ex_1260(i); break;
case 1261: ex_1261(i); break;
case 1262: ex_1262(i); break;
case 1263: ex_1263(i); break;
case 1264: ex_1264(i); break;
case 1265: ex_1265(i); break;
case 1266: ex_1266(i); break;
case 1267: ex_1267(i); break;
case 1268: ex_1268(i); break;
case 1269: ex_1269(i); break;
case 1270: ex_1270(i); break;
case 1271: ex_1271(i); break;
case 1272: ex_1272(i); break;
case 1273: ex_1273(i); break;
case 1274: ex_1274(i); break;
case 1275: ex_1275(i); break;
case 1276: ex_1276(i); break;
case 1277: ex_1277(i); break;
case 1278: ex_1278(i); break;
case 1279: ex_1279(i); break;
case 1280: ex_1280(i); break;
case 1281: ex_1281(i); break;
case 1282: ex_1282(i); break;
case 1283: ex_1283(i); break;
case 1284: ex_1284(i); break;
case 1285: ex_1285(i); break;
case 1286: ex_1286(i); break;
case 1287: ex_1287(i); break;
case 1288: ex_1288(i); break;
case 1289: ex_1289(i); break;
case 1290: ex_1290(i); break;
case 1291: ex_1291(i); break;
case 1292: ex_1292(i); break;
case 1293: ex_1293(i); break;
case 1294: ex_1294(i); break;
case 1295: ex_1295(i); break;
case 1296: ex_1296(i); break;
case 1297: ex_1297(i); break;
case 1298: ex_1298(i); break;
case 1299: ex_1299(i); break;
case 1300: ex_1300(i); break;
case 1301: ex_1301(i); break;
case 1302: ex_1302(i); break;
case 1303: ex_1303(i); break;
case 1304: ex_1304(i); break;
case 1305: ex_1305(i); break;
case 1306: ex_1306(i); break;
case 1307: ex_1307(i); break;
case 1308: ex_1308(i); break;
case 1309: ex_1309(i); break;
case 1310: ex_1310(i); break;
case 1311: ex_1311(i); break;
case 1312: ex_1312(i); break;
case 1313: ex_1313(i); break;
case 1314: ex_1314(i); break;
case 1315: ex_1315(i); break;
case 1316: ex_1316(i); break;
case 1317: ex_1317(i); break;
case 1318: ex_1318(i); break;
case 1319: ex_1319(i); break;
case 1320: ex_1320(i); break;
case 1321: ex_1321(i); break;
case 1322: ex_1322(i); break;
case 1323: ex_1323(i); break;
case 1324: ex_1324(i); break;
case 1325: ex_1325(i); break;
case 1326: ex_1326(i); break;
case 1327: ex_1327(i); break;
case 1328: ex_1328(i); break;
case 1329: ex_1329(i); break;
case 1330: ex_1330(i); break;
case 1331: ex_1331(i); break;
case 1332: ex_1332(i); break;
case 1333: ex_1333(i); break;
case 1334: ex_1334(i); break;
case 1335: ex_1335(i); break;
case 1336: ex_1336(i); break;
case 1337: ex_1337(i); break;
case 1338: ex_1338(i); break;
case 1339: ex_1339(i); break;
case 1340: ex_1340(i); break;
case 1341: ex_1341(i); break;
case 1342: ex_1342(i); break;
case 1343: ex_1343(i); break;
case 1344: ex_1344(i); break;
case 1345: ex_1345(i); break;
case 1346: ex_1346(i); break;
case 1347: ex_1347(i); break;
case 1348: ex_1348(i); break;
case 1349: ex_1349(i); break;
case 1350: ex_1350(i); break;
case 1351: ex_1351(i); break;
case 1352: ex_1352(i); break;
case 1353: ex_1353(i); break;
case 1354: ex_1354(i); break;
case 1355: ex_1355(i); break;
case 1356: ex_1356(i); break;
case 1357: ex_1357(i); break;
case 1358: ex_1358(i); break;
case 1359: ex_1359(i); break;
case 1360: ex_1360(i); break;
case 1361: ex_1361(i); break;
case 1362: ex_1362(i); break;
case 1363: ex_1363(i); break;
case 1364: ex_1364(i); break;
case 1365: ex_1365(i); break;
case 1366: ex_1366(i); break;
case 1367: ex_1367(i); break;
case 1368: ex_1368(i); break;
case 1369: ex_1369(i); break;
case 1370: ex_1370(i); break;
case 1371: ex_1371(i); break;
case 1372: ex_1372(i); break;
case 1373: ex_1373(i); break;
case 1374: ex_1374(i); break;
case 1375: ex_1375(i); break;
case 1376: ex_1376(i); break;
case 1377: ex_1377(i); break;
case 1378: ex_1378(i); break;
case 1379: ex_1379(i); break;
case 1380: ex_1380(i); break;
case 1381: ex_1381(i); break;
case 1382: ex_1382(i); break;
case 1383: ex_1383(i); break;
case 1384: ex_1384(i); break;
case 1385: ex_1385(i); break;
case 1386: ex_1386(i); break;
case 1387: ex_1387(i); break;
case 1388: ex_1388(i); break;
case 1389: ex_1389(i); break;
case 1390: ex_1390(i); break;
case 1391: ex_1391(i); break;
case 1392: ex_1392(i); break;
case 1393: ex_1393(i); break;
case 1394: ex_1394(i); break;
case 1395: ex_1395(i); break;
case 1396: ex_1396(i); break;
case 1397: ex_1397(i); break;
case 1398: ex_1398(i); break;
case 1399: ex_1399(i); break;
case 1400: ex_1400(i); break;
case 1401: ex_1401(i); break;
case 1402: ex_1402(i); break;
case 1403: ex_1403(i); break;
case 1404: ex_1404(i); break;
case 1405: ex_1405(i); break;
case 1406: ex_1406(i); break;
case 1407: ex_1407(i); break;
case 1408: ex_1408(i); break;
case 1409: ex_1409(i); break;
case 1410: ex_1410(i); break;
case 1411: ex_1411(i); break;
case 1412: ex_1412(i); break;
case 1413: ex_1413(i); break;
case 1414: ex_1414(i); break;
case 1415: ex_1415(i); break;
case 1416: ex_1416(i); break;
case 1417: ex_1417(i); break;
case 1418: ex_1418(i); break;
case 1419: ex_1419(i); break;
case 1420: ex_1420(i); break;
case 1421: ex_1421(i); break;
case 1422: ex_1422(i); break;
case 1423: ex_1423(i); break;
case 1424: ex_1424(i); break;
case 1425: ex_1425(i); break;
case 1426: ex_1426(i); break;
case 1427: ex_1427(i); break;
case 1428: ex_1428(i); break;
case 1429: ex_1429(i); break;
case 1430: ex_1430(i); break;
case 1431: ex_1431(i); break;
case 1432: ex_1432(i); break;
case 1433: ex_1433(i); break;
case 1434: ex_1434(i); break;
case 1435: ex_1435(i); break;
case 1436: ex_1436(i); break;
case 1437: ex_1437(i); break;
case 1438: ex_1438(i); break;
case 1439: ex_1439(i); break;
case 1440: ex_1440(i); break;
case 1441: ex_1441(i); break;
case 1442: ex_1442(i); break;
case 1443: ex_1443(i); break;
case 1444: ex_1444(i); break;
case 1445: ex_1445(i); break;
case 1446: ex_1446(i); break;
case 1447: ex_1447(i); break;
case 1448: ex_1448(i); break;
case 1449: ex_1449(i); break;
case 1450: ex_1450(i); break;
case 1451: ex_1451(i); break;
case 1452: ex_1452(i); break;
case 1453: ex_1453(i); break;
case 1454: ex_1454(i); break;
case 1455: ex_1455(i); break;
case 1456: ex_1456(i); break;
case 1457: ex_1457(i); break;
case 1458: ex_1458(i); break;
case 1459: ex_1459(i); break;
case 1460: ex_1460(i); break;
case 1461: ex_1461(i); break;
case 1462: ex_1462(i); break;
case 1463: ex_1463(i); break;
case 1464: ex_1464(i); break;
case 1465: ex_1465(i); break;
case 1466: ex_1466(i); break;
case 1467: ex_1467(i); break;
case 1468: ex_1468(i); break;
case 1469: ex_1469(i); break;
case 1470: ex_1470(i); break;
case 1471: ex_1471(i); break;
case 1472: ex_1472(i); break;
case 1473: ex_1473(i); break;
case 1474: ex_1474(i); break;
case 1475: ex_1475(i); break;
case 1476: ex_1476(i); break;
case 1477: ex_1477(i); break;
case 1478: ex_1478(i); break;
case 1479: ex_1479(i); break;
case 1480: ex_1480(i); break;
case 1481: ex_1481(i); break;
case 1482: ex_1482(i); break;
case 1483: ex_1483(i); break;
case 1484: ex_1484(i); break;
case 1485: ex_1485(i); break;
case 1486: ex_1486(i); break;
case 1487: ex_1487(i); break;
case 1488: ex_1488(i); break;
case 1489: ex_1489(i); break;
case 1490: ex_1490(i); break;
case 1491: ex_1491(i); break;
case 1492: ex_1492(i); break;
case 1493: ex_1493(i); break;
case 1494: ex_1494(i); break;
case 1495: ex_1495(i); break;
case 1496: ex_1496(i); break;
case 1497: ex_1497(i); break;
case 1498: ex_1498(i); break;
case 1499: ex_1499(i); break;
case 1500: ex_1500(i); break;
case 1501: ex_1501(i); break;
case 1502: ex_1502(i); break;
case 1503: ex_1503(i); break;
case 1504: ex_1504(i); break;
case 1505: ex_1505(i); break;
case 1506: ex_1506(i); break;
case 1507: ex_1507(i); break;
case 1508: ex_1508(i); break;
case 1509: ex_1509(i); break;
case 1510: ex_1510(i); break;
case 1511: ex_1511(i); break;
case 1512: ex_1512(i); break;
case 1513: ex_1513(i); break;
case 1514: ex_1514(i); break;
case 1515: ex_1515(i); break;
case 1516: ex_1516(i); break;
case 1517: ex_1517(i); break;
case 1518: ex_1518(i); break;
case 1519: ex_1519(i); break;
case 1520: ex_1520(i); break;
case 1521: ex_1521(i); break;
case 1522: ex_1522(i); break;
case 1523: ex_1523(i); break;
case 1524: ex_1524(i); break;
case 1525: ex_1525(i); break;
case 1526: ex_1526(i); break;
case 1527: ex_1527(i); break;
case 1528: ex_1528(i); break;
case 1529: ex_1529(i); break;
case 1530: ex_1530(i); break;
case 1531: ex_1531(i); break;
case 1532: ex_1532(i); break;
case 1533: ex_1533(i); break;
case 1534: ex_1534(i); break;
case 1535: ex_1535(i); break;
case 1536: ex_1536(i); break;
case 1537: ex_1537(i); break;
case 1538: ex_1538(i); break;
case 1539: ex_1539(i); break;
case 1540: ex_1540(i); break;
case 1541: ex_1541(i); break;
case 1542: ex_1542(i); break;
case 1543: ex_1543(i); break;
case 1544: ex_1544(i); break;
case 1545: ex_1545(i); break;
case 1546: ex_1546(i); break;
case 1547: ex_1547(i); break;
case 1548: ex_1548(i); break;
case 1549: ex_1549(i); break;
case 1550: ex_1550(i); break;
case 1551: ex_1551(i); break;
case 1552: ex_1552(i); break;
case 1553: ex_1553(i); break;
case 1554: ex_1554(i); break;
case 1555: ex_1555(i); break;
case 1556: ex_1556(i); break;
case 1557: ex_1557(i); break;
case 1558: ex_1558(i); break;
case 1559: ex_1559(i); break;
case 1560: ex_1560(i); break;
case 1561: ex_1561(i); break;
case 1562: ex_1562(i); break;
case 1563: ex_1563(i); break;
case 1564: ex_1564(i); break;
case 1565: ex_1565(i); break;
case 1566: ex_1566(i); break;
case 1567: ex_1567(i); break;
case 1568: ex_1568(i); break;
case 1569: ex_1569(i); break;
case 1570: ex_1570(i); break;
case 1571: ex_1571(i); break;
case 1572: ex_1572(i); break;
case 1573: ex_1573(i); break;
case 1574: ex_1574(i); break;
case 1575: ex_1575(i); break;
case 1576: ex_1576(i); break;
case 1577: ex_1577(i); break;
case 1578: ex_1578(i); break;
case 1579: ex_1579(i); break;
case 1580: ex_1580(i); break;
case 1581: ex_1581(i); break;
case 1582: ex_1582(i); break;
case 1583: ex_1583(i); break;
case 1584: ex_1584(i); break;
case 1585: ex_1585(i); break;
case 1586: ex_1586(i); break;
case 1587: ex_1587(i); break;
case 1588: ex_1588(i); break;
case 1589: ex_1589(i); break;
case 1590: ex_1590(i); break;
case 1591: ex_1591(i); break;
case 1592: ex_1592(i); break;
case 1593: ex_1593(i); break;
case 1594: ex_1594(i); break;
case 1595: ex_1595(i); break;
case 1596: ex_1596(i); break;
case 1597: ex_1597(i); break;
case 1598: ex_1598(i); break;
case 1599: ex_1599(i); break;
case 1600: ex_1600(i); break;
case 1601: ex_1601(i); break;
case 1602: ex_1602(i); break;
case 1603: ex_1603(i); break;
case 1604: ex_1604(i); break;
case 1605: ex_1605(i); break;
case 1606: ex_1606(i); break;
case 1607: ex_1607(i); break;
case 1608: ex_1608(i); break;
case 1609: ex_1609(i); break;
case 1610: ex_1610(i); break;
case 1611: ex_1611(i); break;
case 1612: ex_1612(i); break;
case 1613: ex_1613(i); break;
case 1614: ex_1614(i); break;
case 1615: ex_1615(i); break;
case 1616: ex_1616(i); break;
case 1617: ex_1617(i); break;
case 1618: ex_1618(i); break;
case 1619: ex_1619(i); break;
case 1620: ex_1620(i); break;
case 1621: ex_1621(i); break;
case 1622: ex_1622(i); break;
case 1623: ex_1623(i); break;
case 1624: ex_1624(i); break;
case 1625: ex_1625(i); break;
case 1626: ex_1626(i); break;
case 1627: ex_1627(i); break;
case 1628: ex_1628(i); break;
case 1629: ex_1629(i); break;
case 1630: ex_1630(i); break;
case 1631: ex_1631(i); break;
case 1632: ex_1632(i); break;
case 1633: ex_1633(i); break;
case 1634: ex_1634(i); break;
case 1635: ex_1635(i); break;
case 1636: ex_1636(i); break;
case 1637: ex_1637(i); break;
case 1638: ex_1638(i); break;
case 1639: ex_1639(i); break;
case 1640: ex_1640(i); break;
case 1641: ex_1641(i); break;
case 1642: ex_1642(i); break;
case 1643: ex_1643(i); break;
case 1644: ex_1644(i); break;
case 1645: ex_1645(i); break;
case 1646: ex_1646(i); break;
case 1647: ex_1647(i); break;
case 1648: ex_1648(i); break;
case 1649: ex_1649(i); break;
case 1650: ex_1650(i); break;
case 1651: ex_1651(i); break;
case 1652: ex_1652(i); break;
case 1653: ex_1653(i); break;
case 1654: ex_1654(i); break;
case 1655: ex_1655(i); break;
#endif
