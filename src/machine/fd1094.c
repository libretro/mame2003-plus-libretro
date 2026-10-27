/***************************************************************************

    Hitachi FD1094 encryption emulation

    The decryption (fd1094_decrypt_word and the masked opcode table) is
    the one of MAME's src/mame/sega/fd1094.cpp:

        license:BSD-3-Clause
        copyright-holders:Nicola Salmoria, Andreas Naive, Charles MacDonald

        Copyright Nicola Salmoria, Andreas Naive, and Charles MacDonald.
        All rights reserved.

    The FD1094 is a 68000 that runs encrypted code. Every word fetched from
    program space (opcodes and immediate operands) is decrypted with an 8KB
    key and a "state" (0-255); data reads are not affected.
    - On reset the state is 0.
    - CMPI.L #$00xxFFFF,D0 selects state xx; #$01xxFFFF also leaves irq mode,
      #$0200FFFF enters irq mode, #$0300FFFF leaves it.
    - Taking an interrupt enters irq mode, whose state is key byte 0; RTE
      leaves it.

    Here the decrypted opcodes are cached per state (as in MAME) and the 68000
    fetches them through its opcode base (OP_ROM), so data reads still see the
    encrypted ROM. Only the ROM at address 0 is decrypted.

    Glue for MAME 0.78: hooks in the C 68000 core (Musashi) for CMPI.L and
    RTE, and the irq callback of the CPU. The asm 68000 cores (Cyclone) have
    no such hooks, so the FD1094 sets run on the C core.

***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68k.h"
#include "state.h"
#include "fd1094.h"

#define FD1094_STATE_RESET	0x100
#define FD1094_STATE_IRQ	0x200
#define FD1094_STATE_RTE	0x300

static int     fd1094_cpunum;
static UINT16 *fd1094_rom;			/* encrypted ROM (native 16-bit words) */
static UINT32  fd1094_rom_size;		/* in bytes */
static UINT8  *fd1094_key;			/* 8KB key */
static UINT16 *fd1094_cache[256];	/* decrypted ROM per state, allocated on use */
static UINT16 *fd1094_current;		/* the one for the current state */

static UINT8   fd1094_state;
static UINT8   fd1094_irqmode;

/* CPU address windows that fetch their opcodes from the ROM, through the
   decryption: by default the whole ROM at its own address */
#define FD1094_MAX_WINDOWS	4
static int     fd1094_numwindows;
static offs_t  fd1094_win_start[FD1094_MAX_WINDOWS];
static offs_t  fd1094_win_end[FD1094_MAX_WINDOWS];
static offs_t  fd1094_win_rgnoffs[FD1094_MAX_WINDOWS];
static int     fd1094_curwin;		/* window of the current opcode base, or -1 */

/* opcodes using PC-relative addressing, replaced by FFFF */
static const UINT16 masked_opcodes[] =
{
	0x013a,0x033a,0x053a,0x073a,0x083a,0x093a,0x0b3a,0x0d3a,0x0f3a,

	0x103a,       0x10ba,0x10fa,    0x113a,0x117a,0x11ba,0x11fa,
	0x123a,       0x12ba,0x12fa,    0x133a,0x137a,0x13ba,0x13fa,
	0x143a,       0x14ba,0x14fa,    0x153a,0x157a,0x15ba,
	0x163a,       0x16ba,0x16fa,    0x173a,0x177a,0x17ba,
	0x183a,       0x18ba,0x18fa,    0x193a,0x197a,0x19ba,
	0x1a3a,       0x1aba,0x1afa,    0x1b3a,0x1b7a,0x1bba,
	0x1c3a,       0x1cba,0x1cfa,    0x1d3a,0x1d7a,0x1dba,
	0x1e3a,       0x1eba,0x1efa,    0x1f3a,0x1f7a,0x1fba,

	0x203a,0x207a,0x20ba,0x20fa,    0x213a,0x217a,0x21ba,0x21fa,
	0x223a,0x227a,0x22ba,0x22fa,    0x233a,0x237a,0x23ba,0x23fa,
	0x243a,0x247a,0x24ba,0x24fa,    0x253a,0x257a,0x25ba,
	0x263a,0x267a,0x26ba,0x26fa,    0x273a,0x277a,0x27ba,
	0x283a,0x287a,0x28ba,0x28fa,    0x293a,0x297a,0x29ba,
	0x2a3a,0x2a7a,0x2aba,0x2afa,    0x2b3a,0x2b7a,0x2bba,
	0x2c3a,0x2c7a,0x2cba,0x2cfa,    0x2d3a,0x2d7a,0x2dba,
	0x2e3a,0x2e7a,0x2eba,0x2efa,    0x2f3a,0x2f7a,0x2fba,

	0x303a,0x307a,0x30ba,0x30fa,    0x313a,0x317a,0x31ba,0x31fa,
	0x323a,0x327a,0x32ba,0x32fa,    0x333a,0x337a,0x33ba,0x33fa,
	0x343a,0x347a,0x34ba,0x34fa,    0x353a,0x357a,0x35ba,
	0x363a,0x367a,0x36ba,0x36fa,    0x373a,0x377a,0x37ba,
	0x383a,0x387a,0x38ba,0x38fa,    0x393a,0x397a,0x39ba,
	0x3a3a,0x3a7a,0x3aba,0x3afa,    0x3b3a,0x3b7a,0x3bba,
	0x3c3a,0x3c7a,0x3cba,0x3cfa,    0x3d3a,0x3d7a,0x3dba,
	0x3e3a,0x3e7a,0x3eba,0x3efa,    0x3f3a,0x3f7a,0x3fba,

	0x41ba,0x43ba,0x44fa,0x45ba,0x46fa,0x47ba,0x49ba,0x4bba,0x4cba,0x4cfa,0x4dba,0x4fba,

	0x803a,0x807a,0x80ba,0x80fa,    0x81fa,
	0x823a,0x827a,0x82ba,0x82fa,    0x83fa,
	0x843a,0x847a,0x84ba,0x84fa,    0x85fa,
	0x863a,0x867a,0x86ba,0x86fa,    0x87fa,
	0x883a,0x887a,0x88ba,0x88fa,    0x89fa,
	0x8a3a,0x8a7a,0x8aba,0x8afa,    0x8bfa,
	0x8c3a,0x8c7a,0x8cba,0x8cfa,    0x8dfa,
	0x8e3a,0x8e7a,0x8eba,0x8efa,    0x8ffa,

	0x903a,0x907a,0x90ba,0x90fa,    0x91fa,
	0x923a,0x927a,0x92ba,0x92fa,    0x93fa,
	0x943a,0x947a,0x94ba,0x94fa,    0x95fa,
	0x963a,0x967a,0x96ba,0x96fa,    0x97fa,
	0x983a,0x987a,0x98ba,0x98fa,    0x99fa,
	0x9a3a,0x9a7a,0x9aba,0x9afa,    0x9bfa,
	0x9c3a,0x9c7a,0x9cba,0x9cfa,    0x9dfa,
	0x9e3a,0x9e7a,0x9eba,0x9efa,    0x9ffa,

	0xb03a,0xb07a,0xb0ba,0xb0fa,    0xb1fa,
	0xb23a,0xb27a,0xb2ba,0xb2fa,    0xb3fa,
	0xb43a,0xb47a,0xb4ba,0xb4fa,    0xb5fa,
	0xb63a,0xb67a,0xb6ba,0xb6fa,    0xb7fa,
	0xb83a,0xb87a,0xb8ba,0xb8fa,    0xb9fa,
	0xba3a,0xba7a,0xbaba,0xbafa,    0xbbfa,
	0xbc3a,0xbc7a,0xbcba,0xbcfa,    0xbdfa,
	0xbe3a,0xbe7a,0xbeba,0xbefa,    0xbffa,

	0xc03a,0xc07a,0xc0ba,0xc0fa,    0xc1fa,
	0xc23a,0xc27a,0xc2ba,0xc2fa,    0xc3fa,
	0xc43a,0xc47a,0xc4ba,0xc4fa,    0xc5fa,
	0xc63a,0xc67a,0xc6ba,0xc6fa,    0xc7fa,
	0xc83a,0xc87a,0xc8ba,0xc8fa,    0xc9fa,
	0xca3a,0xca7a,0xcaba,0xcafa,    0xcbfa,
	0xcc3a,0xcc7a,0xccba,0xccfa,    0xcdfa,
	0xce3a,0xce7a,0xceba,0xcefa,    0xcffa,

	0xd03a,0xd07a,0xd0ba,0xd0fa,    0xd1fa,
	0xd23a,0xd27a,0xd2ba,0xd2fa,    0xd3fa,
	0xd43a,0xd47a,0xd4ba,0xd4fa,    0xd5fa,
	0xd63a,0xd67a,0xd6ba,0xd6fa,    0xd7fa,
	0xd83a,0xd87a,0xd8ba,0xd8fa,    0xd9fa,
	0xda3a,0xda7a,0xdaba,0xdafa,    0xdbfa,
	0xdc3a,0xdc7a,0xdcba,0xdcfa,    0xddfa,
	0xde3a,0xde7a,0xdeba,0xdefa,    0xdffa
};


/* 1 bit per even opcode; [0] the normal set, [1] the more aggressive one */
static UINT8 masked_opcodes_lookup[2][65536/8/2];

static void build_masked_opcodes_lookup(void)
{
	int i, opcode;

	memset(masked_opcodes_lookup, 0, sizeof(masked_opcodes_lookup));
	for (i = 0; i < sizeof(masked_opcodes) / sizeof(masked_opcodes[0]); i++)
	{
		opcode = masked_opcodes[i];
		masked_opcodes_lookup[0][opcode >> 4] |= 1 << ((opcode >> 1) & 7);
		masked_opcodes_lookup[1][opcode >> 4] |= 1 << ((opcode >> 1) & 7);
	}

	/* more opcodes for the more aggressive set */
	for (opcode = 0; opcode < 65536; opcode += 2)
		if ((opcode & 0xff80) == 0x4e80 || (opcode & 0xf0f8) == 0x50c8 || (opcode & 0xf000) == 0x6000)
			masked_opcodes_lookup[1][opcode >> 4] |= 1 << ((opcode >> 1) & 7);
}


/*-------------------------------------------------
    fd1094_decrypt_word - decrypt one word;
    address is the word address (byte address / 2)
-------------------------------------------------*/

static UINT16 fd1094_decrypt_word(offs_t address, UINT16 val, const UINT8 *main_key, UINT8 state, int vector_fetch)
{
	UINT8 gkey1 = main_key[1];
	UINT8 gkey2 = main_key[2];
	UINT8 gkey3 = main_key[3];
	UINT8 mainkey, key_F;
	UINT8 global_xor0, global_xor1, global_swap2, global_swap0a, global_swap0b;
	UINT8 global_swap3, global_swap1, global_swap4;
	UINT8 key_0a, key_0b, key_0c, key_1a, key_1b, key_2a, key_2b, key_3a, key_3b;
	UINT8 key_4a, key_4b, key_5a, key_5b, key_6a, key_6b, key_7a;

	/* adjust the global key with the state */
	if (state & 0x0001)
	{
		gkey1 ^= 0x04;  /* global_xor1 */
		gkey2 ^= 0x80;  /* key_1a invert */
		gkey3 ^= 0x80;  /* key_2a invert */
	}
	if (state & 0x0002)
	{
		gkey1 ^= 0x01;  /* global_swap2 */
		gkey2 ^= 0x10;  /* key_7a invert */
		gkey3 ^= 0x01;  /* key_4b invert */
	}
	if (state & 0x0004)
	{
		gkey1 ^= 0x80;  /* key_0b invert */
		gkey2 ^= 0x40;  /* key_6b invert */
		gkey3 ^= 0x04;  /* global_swap4 */
	}
	if (state & 0x0008)
	{
		gkey1 ^= 0x20;  /* global_xor0 */
		gkey2 ^= 0x02;  /* key_6a invert */
		gkey3 ^= 0x20;  /* key_5a invert */
	}
	if (state & 0x0010)
	{
		gkey1 ^= 0x02;  /* key_0c invert */
		gkey1 ^= 0x40;  /* key_5b invert */
		gkey2 ^= 0x08;  /* key_4a invert */
	}
	if (state & 0x0020)
	{
		gkey1 ^= 0x08;  /* key_1b invert */
		gkey3 ^= 0x08;  /* key_3b invert */
		gkey3 ^= 0x10;  /* global_swap1 */
	}
	if (state & 0x0040)
	{
		gkey1 ^= 0x10;  /* key_2b invert */
		gkey2 ^= 0x20;  /* global_swap0a */
		gkey2 ^= 0x04;  /* global_swap0b */
	}
	if (state & 0x0080)
	{
		gkey2 ^= 0x01;  /* key_3a invert */
		gkey3 ^= 0x02;  /* key_0a invert */
		gkey3 ^= 0x40;  /* global_swap3 */
	}

	/* for address xx0000-xx0006 (but only if >= 000008), use key xx2000-xx2006 */
	if ((address & 0x0ffc) == 0 && address >= 4)
		mainkey = main_key[(address & 0x1fff) | 0x1000];
	else
		mainkey = main_key[address & 0x1fff];

	if (address & 0x1000)
		key_F = BIT(mainkey,7);
	else
		key_F = BIT(mainkey,6);

	/* the initial SP and PC are decrypted differently than opcodes at 0-6 */
	if (vector_fetch)
	{
		if (address <= 3) gkey3 = 0x00;
		if (address <= 2) gkey2 = 0x00;
		if (address <= 1) gkey1 = 0x00;
		if (address <= 1) key_F = 0;
	}

	global_xor0   = 1^BIT(gkey1,5);
	global_xor1   = 1^BIT(gkey1,2);
	global_swap2  = 1^BIT(gkey1,0);

	global_swap0a = 1^BIT(gkey2,5);
	global_swap0b = 1^BIT(gkey2,2);

	global_swap3  = 1^BIT(gkey3,6);
	global_swap1  = 1^BIT(gkey3,4);
	global_swap4  = 1^BIT(gkey3,2);

	key_0a = BIT(mainkey,0) ^ BIT(gkey3,1);
	key_0b = BIT(mainkey,0) ^ BIT(gkey1,7);
	key_0c = BIT(mainkey,0) ^ BIT(gkey1,1);

	key_1a = BIT(mainkey,1) ^ BIT(gkey2,7);
	key_1b = BIT(mainkey,1) ^ BIT(gkey1,3);

	key_2a = BIT(mainkey,2) ^ BIT(gkey3,7);
	key_2b = BIT(mainkey,2) ^ BIT(gkey1,4);

	key_3a = BIT(mainkey,3) ^ BIT(gkey2,0);
	key_3b = BIT(mainkey,3) ^ BIT(gkey3,3);

	key_4a = BIT(mainkey,4) ^ BIT(gkey2,3);
	key_4b = BIT(mainkey,4) ^ BIT(gkey3,0);

	key_5a = BIT(mainkey,5) ^ BIT(gkey3,5);
	key_5b = BIT(mainkey,5) ^ BIT(gkey1,6);

	key_6a = BIT(mainkey,6) ^ BIT(gkey2,1);
	key_6b = BIT(mainkey,6) ^ BIT(gkey2,6);

	key_7a = BIT(mainkey,7) ^ BIT(gkey2,4);

	if (val & 0x8000)           /* block invariant: val & 0x8000 != 0 */
	{
		val = BITSWAP16(val, 15, 9,10,13, 3,12, 0,14, 6, 5, 2,11, 8, 1, 4, 7);

		if (!global_xor1)   if (~val & 0x0800)  val ^= 0x3002;                                      /* 1,12,13 */
		if (~val & 0x0020)  val ^= 0x0044;                                      /* 2,6 */
		if (!key_1b)        if (~val & 0x0400)  val ^= 0x0890;                                      /* 4,7,11 */
		if (!global_swap2)  if (!key_0c)        val ^= 0x0308;                                      /* 3,8,9 */
												val ^= 0x6561;

		if (!key_2b)        val = BITSWAP16(val,15,10,13,12,11,14,9,8,7,6,0,4,3,2,1,5);             /* 0-5, 10-14 */
	}

	if (val & 0x4000)           /* block invariant: val & 0x4000 != 0 */
	{
		val = BITSWAP16(val, 13,14, 7, 0, 8, 6, 4, 2, 1,15, 3,11,12,10, 5, 9);

		if (!global_xor0)   if (val & 0x0010)   val ^= 0x0468;                                      /* 3,5,6,10 */
		if (!key_3a)        if (val & 0x0100)   val ^= 0x0081;                                      /* 0,7 */
		if (!key_6a)        if (val & 0x0004)   val ^= 0x0100;                                      /* 8 */
		if (!key_5b)        if (!key_0b)        val ^= 0x3012;                                      /* 1,4,12,13 */
												val ^= 0x3523;

		if (!global_swap0b) val = BITSWAP16(val, 2,14,13,12, 9,10,11, 8, 7, 6, 5, 4, 3,15, 1, 0);   /* 2-15, 9-11 */
	}

	if (val & 0x2000)           /* block invariant: val & 0x2000 != 0 */
	{
		val = BITSWAP16(val, 10, 2,13, 7, 8, 0, 3,14, 6,15, 1,11, 9, 4, 5,12);

		if (!key_4a)        if (val & 0x0800)   val ^= 0x010c;                                      /* 2,3,8 */
		if (!key_1a)        if (val & 0x0080)   val ^= 0x1000;                                      /* 12 */
		if (!key_7a)        if (val & 0x0400)   val ^= 0x0a21;                                      /* 0,5,9,11 */
		if (!key_4b)        if (!key_0a)        val ^= 0x0080;                                      /* 7 */
		if (!global_swap0a) if (!key_6b)        val ^= 0xc000;                                      /* 14,15 */
												val ^= 0x99a5;

		if (!key_5b)        val = BITSWAP16(val,15,14,13,12,11, 1, 9, 8, 7,10, 5, 6, 3, 2, 4, 0);   /* 1,4,6,10 */
	}

	if (val & 0xe000)           /* block invariant: val & 0xe000 != 0 */
	{
		val = BITSWAP16(val,15,13,14, 5, 6, 0, 9,10, 4,11, 1, 2,12, 3, 7, 8);

		val ^= 0x17ff;

		if (!global_swap4)  val = BITSWAP16(val, 15,14,13, 6,11,10, 9, 5, 7,12, 8, 4, 3, 2, 1, 0);  /* 5-8, 6-12 */
		if (!global_swap3)  val = BITSWAP16(val, 13,15,14,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);  /* 15-14-13 */
		if (!global_swap2)  val = BITSWAP16(val, 15,14,13,12,11, 2, 9, 8,10, 6, 5, 4, 3, 0, 1, 7);  /* 10-2-0-7 */
		if (!key_3b)        val = BITSWAP16(val, 15,14,13,12,11,10, 4, 8, 7, 6, 5, 9, 1, 2, 3, 0);  /* 9-4, 3-1 */
		if (!key_2a)        val = BITSWAP16(val, 13,14,15,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);  /* 13-15 */

		if (!global_swap1)  val = BITSWAP16(val, 15,14,13,12, 9, 8,11,10, 7, 6, 5, 4, 3, 2, 1, 0);  /* 11...8 */
		if (!key_5a)        val = BITSWAP16(val, 15,14,13,12,11,10, 9, 8, 4, 5, 7, 6, 3, 2, 1, 0);  /* 7...4 */
		if (!global_swap0a) val = BITSWAP16(val, 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 0, 3, 2, 1);  /* 3...0 */
	}

	val = BITSWAP16(val, 12,15,14,13,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

	if ((val & 0xb080) == 0x8000) val ^= 0x4000;
	if ((val & 0xf000) == 0xc000) val ^= 0x0080;
	if ((val & 0xb100) == 0x0000) val ^= 0x4000;

	/* mask out opcodes doing PC-relative addressing, replace them with FFFF */
	if ((masked_opcodes_lookup[key_F][val >> 4] >> ((val >> 1) & 7)) & 1)
		val = 0xffff;

	return val;
}


/*-------------------------------------------------
    decrypted opcodes cache
-------------------------------------------------*/

static UINT16 *fd1094_get_decrypted(UINT8 state)
{
	offs_t offset;
	UINT16 *buf = fd1094_cache[state];

	if (buf != NULL)
		return buf;

	buf = auto_malloc(fd1094_rom_size);
	if (buf == NULL)
		return fd1094_rom;
	for (offset = 0; offset < fd1094_rom_size; offset += 2)
		buf[offset / 2] = fd1094_decrypt_word(offset / 2, fd1094_rom[offset / 2], fd1094_key, state, offset < 8);
	fd1094_cache[state] = buf;
	return buf;
}


/*-------------------------------------------------
    opcode base: decrypted opcodes in the ROM,
    plain ones elsewhere
-------------------------------------------------*/

static OPBASE_HANDLER( fd1094_setopbase )
{
	int w;

	for (w = 0; w < fd1094_numwindows; w++)
		if (address >= fd1094_win_start[w] && address <= fd1094_win_end[w])
		{
			OP_RAM = (UINT8 *)fd1094_rom + fd1094_win_rgnoffs[w] - fd1094_win_start[w];
			OP_ROM = (UINT8 *)fd1094_current + fd1094_win_rgnoffs[w] - fd1094_win_start[w];
			OP_MEM_MIN = fd1094_win_start[w];
			OP_MEM_MAX = fd1094_win_end[w];
			opcode_entry = STATIC_ROM;
			fd1094_curwin = w;
			return ~0;
		}

	/* let the memory system find the base, with no offset for the opcodes */
	fd1094_curwin = -1;
	OP_ROM = OP_RAM;
	return address;
}


/*-------------------------------------------------
    fd1094_refresh - point the CPU to the opcodes
    of the current state when it runs in the ROM
-------------------------------------------------*/

static void fd1094_refresh(void)
{
	fd1094_current = fd1094_get_decrypted(fd1094_irqmode ? fd1094_key[0] : fd1094_state);

	cpuintrf_push_context(fd1094_cpunum);
	if (fd1094_curwin >= 0)
	{
		offs_t delta = fd1094_win_rgnoffs[fd1094_curwin] - fd1094_win_start[fd1094_curwin];
		if (OP_RAM == (UINT8 *)fd1094_rom + delta)
			OP_ROM = (UINT8 *)fd1094_current + delta;
	}
	cpuintrf_pop_context();
}


/*-------------------------------------------------
    fd1094_change_state - switch the state
-------------------------------------------------*/

static void fd1094_change_state(int newstate)
{
	switch (newstate & 0x300)
	{
		case 0x000:				/* 0x00xx: select state xx */
			fd1094_state = newstate & 0xff;
			break;

		case FD1094_STATE_RESET:	/* 0x01xx: select state xx and exit irq mode */
			fd1094_state = newstate & 0xff;
			fd1094_irqmode = 0;
			break;

		case FD1094_STATE_IRQ:		/* 0x02xx: enter irq mode */
			fd1094_irqmode = 1;
			break;

		case FD1094_STATE_RTE:		/* 0x03xx: exit irq mode */
			fd1094_irqmode = 0;
			break;
	}

	fd1094_refresh();
}


/* CMPI.L #imm,D0 */
static void fd1094_cmpild_callback(unsigned int val, int reg)
{
	if (cpu_getactivecpu() == fd1094_cpunum && reg == 0 && (val & 0x0000ffff) == 0x0000ffff)
		fd1094_change_state(val >> 16);
}

static void fd1094_rte_callback(void)
{
	if (cpu_getactivecpu() == fd1094_cpunum)
		fd1094_change_state(FD1094_STATE_RTE);
}

static int fd1094_irq_callback(int irqline)
{
	fd1094_change_state(FD1094_STATE_IRQ);
	return M68K_INT_ACK_AUTOVECTOR;
}

static void fd1094_postload(void)
{
	fd1094_refresh();
}


/*-------------------------------------------------
    public interface
-------------------------------------------------*/

void fd1094_driver_init(int cpunum, int keyregion)
{
	int i;

	fd1094_cpunum = cpunum;
	fd1094_rom = (UINT16 *)memory_region(REGION_CPU1 + cpunum);
	fd1094_rom_size = memory_region_length(REGION_CPU1 + cpunum);
	fd1094_key = memory_region(keyregion);

	for (i = 0; i < 256; i++)
		fd1094_cache[i] = NULL;
	build_masked_opcodes_lookup();

	fd1094_numwindows = 1;
	fd1094_win_start[0] = 0;
	fd1094_win_end[0] = fd1094_rom_size - 1;
	fd1094_win_rgnoffs[0] = 0;
	fd1094_curwin = -1;

	fd1094_state = 0;
	fd1094_irqmode = 0;
	fd1094_current = fd1094_get_decrypted(0);

	state_save_register_UINT8("fd1094", 0, "state", &fd1094_state, 1);
	state_save_register_UINT8("fd1094", 0, "irqmode", &fd1094_irqmode, 1);
	state_save_register_func_postload(fd1094_postload);
}

void fd1094_set_window(int which, offs_t start, offs_t end, offs_t rgnoffs)
{
	fd1094_win_start[which] = start;
	fd1094_win_end[which] = end;
	fd1094_win_rgnoffs[which] = rgnoffs;
	if (fd1094_numwindows < which + 1)
		fd1094_numwindows = which + 1;
}

void fd1094_machine_init(void)
{
	/* the hooks are cleared when the CPUs are initialised */
	m68k_cmpild_callback = fd1094_cmpild_callback;
	m68k_rte_callback = fd1094_rte_callback;
	cpu_set_irq_callback(fd1094_cpunum, fd1094_irq_callback);
	memory_set_opbase_handler(fd1094_cpunum, fd1094_setopbase);

	/* the CPU is reset after this and fetches its vectors from the ROM */
	cpuintrf_push_context(fd1094_cpunum);
	OP_RAM = (UINT8 *)fd1094_rom;
	OP_MEM_MIN = 0;
	OP_MEM_MAX = fd1094_win_end[0];
	opcode_entry = STATIC_ROM;
	fd1094_curwin = 0;
	cpuintrf_pop_context();

	fd1094_cpu_reset();
}

void fd1094_cpu_reset(void)
{
	fd1094_change_state(FD1094_STATE_RESET);
}
