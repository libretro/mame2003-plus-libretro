/*
    Yamaha YMF271-F "OPX" emulator

    C port of current MAME's core (superctr), written from the datasheet and
    the application manual.  Where the documents leave the behaviour open, the
    model follows recordings of real hardware.

    Architecture (from the datasheet / application manual):
      - 48 slots in 12 groups of 4; slot n = 12*bank + group, where bank is the
        register bank (S1..S4) and the slot number is also the order in which the
        chip evaluates operators inside one 44.1 kHz sample period.
      - Every slot runs the same operator pipeline (PG -> oscillator -> EG -> OP).
        The oscillator reads one of 7 internal waveforms (log-sin derived) or, for
        the 12 slots of groups 0/4/8 only (n % 4 == 0), external PCM data.
      - The per-group sync register selects how the 4 slots are connected
        (4op / 2x2op / 3op+PCM / 4xPCM), which slots receive broadcast writes,
        and which slot(s) act as key-on slot.

    Model:
      - Register model with sync broadcast, F-number latch, status / End flags,
        timers, external memory window.
      - OPM/OPZ-style log-sin/exp operator
      - Per-slot LFO: clock-divider rate table 2-6-2, saw / square / triangle,
        PMS depth fnum*k/1024 (table 2-6-3), AMS 63/126/252 units, phase reset
        at key-on; AM starts at full attenuation for every waveform (OPM convention).
      - AccOn = the slot output is accumulated in a saturating 14-bit sum.
      - External PCM: FM PG with the implicit fnum bit 11, Fs divider, 8-bit and
        packed 12-bit words, linear interpolation, looping, End flags, external
        key code (manual 2-9), envelope multiply, same pan as FM.

    Open points (need to be verified on real HW):
      - Waveforms 1-6 unverified on HW recordings and based on guesswork.
      - Key code: octave = Block for blocks 0..7, assumed to clamp at 0 for negative blocks.
      - wave 7 on slots other than 0,4,..,44 assumed to be silent.

    Not implemented: PFM, PCM alternate loop.

    Only the CH0 / CH1 outputs are mixed (left / right), as the MAME drivers route them.
*/

#include <math.h>
#include "driver.h"
#include "cpuintrf.h"
#include "state.h"

#define NUM_SLOTS	48
#define NUM_GROUPS	12
#define YMF271_CLOCK	16934400

static const INT8 fm_tab[16]  = { 0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1 };
static const INT8 pcm_tab[16] = { 0, 4, 8, -1, 12, 16, 20, -1, 24, 28, 32, -1, 36, 40, 44, -1 };

/* sync modes (utility reg 0x0n bits 1:0) */
enum
{
	SYNC_4OP = 0,		/* S1-S2-S3-S4, key-on slot S1 */
	SYNC_2X2OP = 1,		/* S1-S3 and S2-S4, key-on slots S1, S2 */
	SYNC_3OP_PCM = 2,	/* S1-S2-S3 FM + S4 PCM, key-on slots S1,S4 */
	SYNC_PCM = 3		/* 4 independent slots */
};

enum
{
	EG_ATTACK = 0,
	EG_DECAY1,
	EG_DECAY2,
	EG_RELEASE,
	EG_OFF
};

/* EG increment patterns (ymfm/OPM): nibble k of entry r = increment for EG sub-step k at rate r. */
static const UINT32 eg_inc[64] =
{
	0x00000000, 0x00000000, 0x10101010, 0x10101010,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x10101010, 0x10111010, 0x11101110, 0x11111110,
	0x11111111, 0x21112111, 0x21212121, 0x22212221,
	0x22222222, 0x42224222, 0x42424242, 0x44424442,
	0x44444444, 0x84448444, 0x84848484, 0x88848884,
	0x88888888, 0x88888888, 0x88888888, 0x88888888
};

/* Rate key scaling (manual table 2-6-7), [keycode][KS] */
static const UINT8 rks_tab[32][8] =
{
	{  0,  0,  0,  0,  0,  2,  4,  8 }, {  0,  0,  0,  0,  1,  3,  5,  9 },
	{  0,  0,  0,  1,  2,  4,  6, 10 }, {  0,  0,  0,  1,  3,  5,  7, 11 },
	{  0,  0,  1,  2,  4,  6,  8, 12 }, {  0,  0,  1,  2,  5,  7,  9, 13 },
	{  0,  0,  1,  3,  6,  8, 10, 14 }, {  0,  0,  1,  3,  7,  9, 11, 15 },
	{  0,  1,  2,  4,  8, 10, 12, 16 }, {  0,  1,  2,  4,  9, 11, 13, 17 },
	{  0,  1,  2,  5, 10, 12, 14, 18 }, {  0,  1,  2,  5, 11, 13, 15, 19 },
	{  0,  1,  3,  6, 12, 14, 16, 20 }, {  0,  1,  3,  6, 13, 15, 17, 21 },
	{  0,  1,  3,  7, 14, 16, 18, 22 }, {  0,  1,  3,  7, 15, 17, 19, 23 },
	{  0,  2,  4,  8, 16, 18, 20, 24 }, {  0,  2,  4,  8, 17, 19, 21, 25 },
	{  0,  2,  4,  9, 18, 20, 22, 26 }, {  0,  2,  4,  9, 19, 21, 23, 27 },
	{  0,  2,  5, 10, 20, 22, 24, 28 }, {  0,  2,  5, 10, 21, 23, 25, 29 },
	{  0,  2,  5, 11, 22, 24, 26, 30 }, {  0,  2,  5, 11, 23, 25, 27, 31 },
	{  0,  3,  6, 12, 24, 26, 28, 31 }, {  0,  3,  6, 12, 25, 27, 29, 31 },
	{  0,  3,  6, 13, 26, 28, 30, 31 }, {  0,  3,  6, 13, 27, 29, 31, 31 },
	{  0,  3,  7, 14, 28, 30, 31, 31 }, {  0,  3,  7, 14, 29, 31, 31, 31 },
	{  0,  3,  7, 15, 30, 31, 31, 31 }, {  0,  3,  7, 15, 31, 31, 31, 31 }
};

/* Detune, [keycode][DT&3], in units of fs/2^20 Hz (OPM DT1 table) */
static const UINT8 detune_tab[32][4] =
{
	{ 0, 0, 1, 2 }, { 0, 0, 1, 2 }, { 0, 0, 1, 2 }, { 0, 0, 1, 2 },
	{ 0, 1, 2, 2 }, { 0, 1, 2, 3 }, { 0, 1, 2, 3 }, { 0, 1, 2, 3 },
	{ 0, 1, 2, 4 }, { 0, 1, 3, 4 }, { 0, 1, 3, 4 }, { 0, 1, 3, 5 },
	{ 0, 2, 4, 5 }, { 0, 2, 4, 6 }, { 0, 2, 4, 6 }, { 0, 2, 5, 7 },
	{ 0, 2, 5, 8 }, { 0, 3, 6, 8 }, { 0, 3, 6, 9 }, { 0, 3, 7, 10 },
	{ 0, 4, 8, 11 }, { 0, 4, 8, 12 }, { 0, 4, 9, 13 }, { 0, 5, 10, 14 },
	{ 0, 5, 11, 16 }, { 0, 6, 12, 17 }, { 0, 6, 13, 19 }, { 0, 7, 14, 20 },
	{ 0, 8, 16, 22 }, { 0, 8, 16, 22 }, { 0, 8, 16, 22 }, { 0, 8, 16, 22 }
};

/* Modulation level (FB register of a modulated slot) */
static const UINT16 modlevel[8] = { 128, 64, 32, 16, 8, 256, 512, 1024 };

/* LFO PM depth (manual table 2-6-3): max deviation = fnum * k / 1024 */
static const UINT8 pms_k[8] = { 0, 2, 3, 4, 6, 12, 24, 48 };

typedef struct
{
	UINT8 mods[4];
	UINT8 car;
	UINT8 fbsrc;
} opx_alg;

/* Algorithm connection tables: mods = positions modulating each position,
   car = carrier positions, fbsrc = position fed back into position 0 */
static const opx_alg alg4[16] =
{
	{ { 0, 0x4, 0x1, 0x2 }, 0x8, 0 },	/* 0: S1>S3>S2>S4 */
	{ { 0, 0x4, 0x1, 0x2 }, 0x8, 2 },	/* 1: same, fb loop S1>S3 */
	{ { 0, 0x5, 0x0, 0x2 }, 0x8, 0 },	/* 2: (S1+S3)>S2>S4 */
	{ { 0, 0x4, 0x0, 0x3 }, 0x8, 0 },	/* 3: (S1 + S3>S2)>S4 */
	{ { 0, 0x0, 0x1, 0x6 }, 0x8, 0 },	/* 4: (S1>S3 + S2)>S4 */
	{ { 0, 0x0, 0x1, 0x6 }, 0x8, 2 },	/* 5: same, fb loop S1>S3 */
	{ { 0, 0x0, 0x1, 0x2 }, 0xC, 0 },	/* 6: S1>S3, S2>S4 */
	{ { 0, 0x0, 0x1, 0x2 }, 0xC, 2 },	/* 7: same, fb loop S1>S3 */
	{ { 0, 0x4, 0x0, 0x2 }, 0x9, 0 },	/* 8: S1, S3>S2>S4 */
	{ { 0, 0x0, 0x0, 0x6 }, 0x9, 0 },	/* 9: S1, (S3+S2)>S4 */
	{ { 0, 0x0, 0x1, 0x0 }, 0xE, 0 },	/* 10: S1>S3, S2, S4 */
	{ { 0, 0x0, 0x1, 0x0 }, 0xE, 2 },	/* 11: same, fb loop S1>S3 */
	{ { 0, 0x1, 0x1, 0x1 }, 0xE, 0 },	/* 12: S1>(S2,S3,S4) */
	{ { 0, 0x4, 0x0, 0x0 }, 0xB, 0 },	/* 13: S1, S3>S2, S4 */
	{ { 0, 0x0, 0x1, 0x2 }, 0xD, 0 },	/* 14: S1, S1>S3, S2>S4 */
	{ { 0, 0x0, 0x0, 0x0 }, 0xF, 0 }	/* 15: all carriers */
};

static const opx_alg alg2[4] =
{
	{ { 0, 0x1, 0, 0 }, 0x2, 0 },	/* 0: A>B */
	{ { 0, 0x1, 0, 0 }, 0x2, 1 },	/* 1: A>B, fb loop */
	{ { 0, 0x0, 0, 0 }, 0x3, 0 },	/* 2: A, B */
	{ { 0, 0x1, 0, 0 }, 0x3, 0 }	/* 3: A, A>B */
};

static const opx_alg alg3[8] =
{
	{ { 0, 0x4, 0x1, 0 }, 0x2, 0 },	/* 0: S1>S3>S2 */
	{ { 0, 0x4, 0x1, 0 }, 0x2, 2 },	/* 1: same, fb loop */
	{ { 0, 0x5, 0x0, 0 }, 0x2, 0 },	/* 2: (S1+S3)>S2 */
	{ { 0, 0x4, 0x0, 0 }, 0x3, 0 },	/* 3: S1, S3>S2 */
	{ { 0, 0x0, 0x1, 0 }, 0x6, 0 },	/* 4: S1>S3, S2 */
	{ { 0, 0x0, 0x1, 0 }, 0x6, 2 },	/* 5: same, fb loop */
	{ { 0, 0x0, 0x0, 0 }, 0x7, 0 },	/* 6: S1, S2, S3 */
	{ { 0, 0x0, 0x1, 0 }, 0x7, 0 }	/* 7: S1, S1>S3, S2 */
};

static const opx_alg alg_single = { { 0, 0, 0, 0 }, 0x1, 0 };

typedef struct
{
	/* function registers (decoded) */
	UINT8 kon;
	UINT8 ext_en;
	UINT8 ext_out;
	UINT8 lfo_freq;
	UINT8 ams, pms, lfo_wave;
	UINT8 dt, mul;
	UINT8 tl;
	UINT8 ks, ar;
	UINT8 d1r;
	UINT8 d2r;
	UINT8 d1l, rr;
	UINT16 fnum;
	UINT8 block;
	UINT8 fnum_latch;
	UINT8 accon, fb, wave;
	UINT8 alg;
	UINT8 ch_level[4];

	/* PCM attribute registers (only meaningful if slot % 4 == 0) */
	UINT32 pcm_start;
	UINT32 pcm_end;
	UINT32 pcm_loop;
	UINT8 pcm_altloop;
	UINT8 pcm_fs;
	UINT8 pcm_12bit;
	UINT8 pcm_srcnote, pcm_srcb;

	/* runtime state */
	INT8 block_s;
	UINT8 keycode;
	UINT8 eg_state;
	INT32 eg_att;
	UINT32 phase;
	INT32 out;
	INT32 acc;
	INT32 fb_hist[2];
	UINT32 lfo_cnt;
	UINT8 lfo_pos;
	UINT32 pcm_pos;
	UINT32 pcm_frac;
	UINT8 pcm_ended;	/* End flag already raised since the last key-on */

	/* connection cache (rebuilt when sync/algorithm changes) */
	UINT8 c_nmod;
	UINT8 c_mod[3];
	UINT8 c_fbhead;
	INT8 c_fbtarget;
	UINT8 c_carrier;

	/* phase / PCM step increments without LFO PM, cached until a register they use changes */
	UINT8 inc_dirty;
	UINT32 inc_cache;
	UINT32 pcm_step_cache;

	/* phase increment with LFO PM for the last PM value (pm_valid 0 = none) */
	UINT8 pm_valid;
	INT32 pm_last;
	UINT32 pm_inc_cache;

	/* EG rate of the state eg_rate_state - 1 (0 = none) */
	UINT8 eg_rate_state;
	UINT8 eg_rate_cache;
} opx_slot;

typedef struct
{
	UINT8 sync;
	UINT8 pfm;
	UINT8 dirty;
} opx_group;

typedef struct
{
	opx_slot slots[NUM_SLOTS];
	opx_group groups[NUM_GROUPS];
	UINT8 regs_main[0x10];
	UINT16 timerA;
	UINT8 timerB;
	UINT8 timer_ctrl;
	UINT8 status;
	UINT16 end_status;
	UINT8 irqstate;
	UINT32 ext_address;
	UINT8 ext_rw;
	UINT8 ext_readlatch;
	UINT32 eg_cnt;
	UINT8 eg_phase;

	void *timA, *timB;
	int stream;
	const UINT8 *rom;
	UINT32 rom_size, rom_mask;
	void (*irq_callback)(int state);
	read8_handler ext_mem_read;
	write8_handler ext_mem_write;
} YMF271Chip;

static YMF271Chip YMF271[MAX_YMF271];
static UINT16 ymf_logsin[256];
static UINT16 ymf_exp[256];


/* external memory (sample ROM) */
static UINT8 read_byte(YMF271Chip *chip, UINT32 address)
{
	if (chip->ext_mem_read)
		return chip->ext_mem_read(address);
	/* the ROM is mirrored over the high bits (unmapped past a non power-of-two size) */
	address &= chip->rom_mask;
	if (address < chip->rom_size)
		return chip->rom[address];
	return 0;
}


/* Is slot (bank, group) a key-on slot under the group's sync mode? */
static int is_keyon_slot(YMF271Chip *chip, int bank, int group)
{
	switch (chip->groups[group].sync)
	{
	case SYNC_4OP:		return bank == 0;
	case SYNC_2X2OP:	return bank == 0 || bank == 1;
	case SYNC_3OP_PCM:	return bank == 0 || bank == 3;
	default:		return 1;
	}
}

/* Fill 'slots' with the slot numbers that form the voice keyed by (bank, group). */
static int voice_slots(YMF271Chip *chip, int bank, int group, int *slots)
{
	switch (chip->groups[group].sync)
	{
	case SYNC_4OP:
		if (bank != 0) return 0;
		slots[0] = group; slots[1] = group + 12; slots[2] = group + 24; slots[3] = group + 36;
		return 4;
	case SYNC_2X2OP:
		if (bank == 0) { slots[0] = group; slots[1] = group + 24; return 2; }
		if (bank == 1) { slots[0] = group + 12; slots[1] = group + 36; return 2; }
		return 0;
	case SYNC_3OP_PCM:
		if (bank == 0) { slots[0] = group; slots[1] = group + 12; slots[2] = group + 24; return 3; }
		if (bank == 3) { slots[0] = group + 36; return 1; }
		return 0;
	default:
		slots[0] = 12 * bank + group;
		return 1;
	}
}


/* key code (manual 2-9) */
static void update_keycode(opx_slot *s, int slotnum)
{
	int n43, kc;

	if (s->wave == 7 && (slotnum & 3) == 0)
	{
		UINT16 fn = s->fnum & 0x7FF;
		if (fn < 0x100)      n43 = 0;
		else if (fn < 0x300) n43 = 1;
		else if (fn < 0x500) n43 = 2;
		else                 n43 = 3;
		kc = s->pcm_srcb * 4 + s->pcm_srcnote + (s->block & 7) * 4 + n43;
	}
	else
	{
		if (s->fnum < 0x780)      n43 = 0;
		else if (s->fnum < 0x900) n43 = 1;
		else if (s->fnum < 0xA80) n43 = 2;
		else                      n43 = 3;
		/* negative blocks: the octave clamps at 0 */
		kc = ((s->block_s < 0) ? 0 : (s->block & 7) * 4) + n43;
	}
	s->keycode = (UINT8)(kc & 31);
	s->inc_dirty = 1;
	s->pm_valid = 0;
	s->eg_rate_state = 0;
}

static int eg_rate(int rate2, int rks)
{
	/* rate2 = 2*AR/D1R/D2R or 4*RR; rate 0 stays 0 (infinite) */
	if (rate2 == 0)
		return 0;
	rate2 += rks;
	return (rate2 > 63) ? 63 : rate2;
}

static void slot_keyon(YMF271Chip *chip, int slotnum)
{
	opx_slot *s = &chip->slots[slotnum];

	s->eg_state = EG_ATTACK;
	s->phase = 0;
	/* The attenuation continues from its current value (OPM); a maximum
	   attack rate jumps straight to 0 dB */
	if (eg_rate(s->ar * 2, rks_tab[s->keycode][s->ks]) >= 63)
		s->eg_att = 0;
	s->lfo_cnt = 0;
	s->lfo_pos = 0;
	s->pcm_pos = 0;
	s->pcm_frac = 0;
	s->pcm_ended = 0;
	s->acc = 0;
	if ((slotnum & 3) == 0)
		chip->end_status &= ~(1 << (slotnum >> 2));
}

static void slot_keyoff(YMF271Chip *chip, int slotnum)
{
	opx_slot *s = &chip->slots[slotnum];

	if (s->eg_state != EG_OFF)
		s->eg_state = EG_RELEASE;
}


/* write one decoded function register to one slot */
static void write_slot_reg(YMF271Chip *chip, int slotnum, int reg, UINT8 data)
{
	opx_slot *s = &chip->slots[slotnum];

	/* the cached increments and EG rate depend on these registers */
	s->pm_valid = 0;
	s->eg_rate_state = 0;

	switch (reg)
	{
	case 0x0:
		s->ext_en = (data >> 7) & 1;
		s->ext_out = (data >> 3) & 0xF;
		s->kon = data & 1;
		/* Every KON=1 write (re)triggers the slot: P-47 Aces writes KON=1 onto
		   already keyed slots for note repeats */
		if (data & 1)
			slot_keyon(chip, slotnum);
		else
			slot_keyoff(chip, slotnum);
		break;
	case 0x1:
		s->lfo_freq = data;
		break;
	case 0x2:
		s->lfo_wave = data & 3;
		s->pms = (data >> 3) & 7;
		s->ams = (data >> 6) & 3;
		break;
	case 0x3:
		s->mul = data & 0xF;
		s->dt = (data >> 4) & 7;
		s->inc_dirty = 1;
		break;
	case 0x4:
		s->tl = data & 0x7F;
		break;
	case 0x5:
		s->ar = data & 0x1F;
		s->ks = (data >> 5) & 7;
		break;
	case 0x6:
		s->d1r = data & 0x1F;
		break;
	case 0x7:
		s->d2r = data & 0x1F;
		break;
	case 0x8:
		s->rr = data & 0xF;
		s->d1l = (data >> 4) & 0xF;
		break;
	case 0x9:
		/* F-number low: commits the latched Block / F-number high nibble */
		s->fnum = ((s->fnum_latch & 0x0F) << 8) | data;
		s->block = s->fnum_latch >> 4;
		s->block_s = (INT8)((s->block ^ 8) - 8);
		update_keycode(s, slotnum);
		break;
	case 0xA:
		s->fnum_latch = data;
		break;
	case 0xB:
		s->wave = data & 7;
		s->fb = (data >> 4) & 7;
		s->accon = (data >> 7) & 1;
		update_keycode(s, slotnum);
		break;
	case 0xC:
		s->alg = data & 0xF;
		chip->groups[slotnum % 12].dirty = 1;
		break;
	case 0xD:
		s->ch_level[0] = data >> 4;
		s->ch_level[1] = data & 0xF;
		break;
	case 0xE:
		s->ch_level[2] = data >> 4;
		s->ch_level[3] = data & 0xF;
		break;
	default:
		break;
	}
}

/* FM function register write: bank 0..3 (S1..S4), address = reg<<4 | group nibble */
static void write_fm(YMF271Chip *chip, int bank, UINT8 address, UINT8 data)
{
	int group = fm_tab[address & 0xF];
	int reg = address >> 4;
	int broadcast;

	if (group < 0)
		return;

	/* Registers managed by the key-on sync mode, copied to all slots of the voice */
	switch (reg)
	{
	case 0x0: case 0x9: case 0xA: case 0xC: case 0xD: case 0xE:
		broadcast = 1;
		break;
	default:
		broadcast = 0;
		break;
	}

	if (broadcast && is_keyon_slot(chip, bank, group) && chip->groups[group].sync != SYNC_PCM)
	{
		int slots[4];
		int n = voice_slots(chip, bank, group, slots);
		int i;
		for (i = 0; i < n; i++)
			write_slot_reg(chip, slots[i], reg, data);
	}
	else
		write_slot_reg(chip, 12 * bank + group, reg, data);
}

/* PCM attribute register write (bank 5) */
static void write_pcm(YMF271Chip *chip, UINT8 address, UINT8 data)
{
	int slotnum = pcm_tab[address & 0xF];
	opx_slot *s;

	if (slotnum < 0)
		return;
	s = &chip->slots[slotnum];

	switch (address >> 4)
	{
	case 0x0: s->pcm_start = (s->pcm_start & 0xFFFF00) | data; break;
	case 0x1: s->pcm_start = (s->pcm_start & 0xFF00FF) | (data << 8); break;
	case 0x2:
		s->pcm_start = (s->pcm_start & 0x00FFFF) | ((data & 0x7F) << 16);
		s->pcm_altloop = data >> 7;
		break;
	case 0x3: s->pcm_end = (s->pcm_end & 0xFFFF00) | data; break;
	case 0x4: s->pcm_end = (s->pcm_end & 0xFF00FF) | (data << 8); break;
	case 0x5: s->pcm_end = (s->pcm_end & 0x00FFFF) | ((data & 0x7F) << 16); break;
	case 0x6: s->pcm_loop = (s->pcm_loop & 0xFFFF00) | data; break;
	case 0x7: s->pcm_loop = (s->pcm_loop & 0xFF00FF) | (data << 8); break;
	case 0x8: s->pcm_loop = (s->pcm_loop & 0x00FFFF) | ((data & 0x7F) << 16); break;
	case 0x9:
		s->pcm_fs = data & 3;
		s->pcm_12bit = (data >> 2) & 1;
		s->pcm_srcnote = (data >> 3) & 3;
		s->pcm_srcb = (data >> 5) & 7;
		update_keycode(s, slotnum);
		break;
	default:
		break;
	}
}

static void update_irq(YMF271Chip *chip)
{
	if (chip->irq_callback)
		chip->irq_callback(chip->irqstate ? 1 : 0);
}

static int ymf271_clock[MAX_YMF271];

static double clocks_to_time(int chipnum, UINT32 clocks)
{
	return (double)clocks / (double)ymf271_clock[chipnum];
}

static void timer_a_expired(int chipnum)
{
	YMF271Chip *chip = &YMF271[chipnum];
	chip->status |= 1;
	if (chip->timer_ctrl & 4)
	{
		chip->irqstate |= 1;
		update_irq(chip);
	}
	timer_adjust(chip->timA, clocks_to_time(chipnum, 384 * (1024 - chip->timerA)), chipnum, 0);
}

static void timer_b_expired(int chipnum)
{
	YMF271Chip *chip = &YMF271[chipnum];

	chip->status |= 2;
	if (chip->timer_ctrl & 8)
	{
		chip->irqstate |= 2;
		update_irq(chip);
	}
	timer_adjust(chip->timB, clocks_to_time(chipnum, 384 * 16 * (256 - chip->timerB)), chipnum, 0);
}

/* utility register write (bank 6): sync, timers, external memory access, test */
static void write_util(int chipnum, UINT8 address, UINT8 data)
{
	YMF271Chip *chip = &YMF271[chipnum];

	if ((address & 0xF0) == 0x00)
	{
		int group = fm_tab[address & 0xF];
		if (group < 0)
			return;
		chip->groups[group].sync = data & 3;
		chip->groups[group].pfm = data >> 7;
		chip->groups[group].dirty = 1;
		return;
	}

	switch (address)
	{
	case 0x10:	/* Timer A: 0x10 = high 8 bits, 0x11 = low 2 bits */
		chip->timerA = (chip->timerA & 0x003) | (data << 2);
		break;
	case 0x11:
		chip->timerA = (chip->timerA & 0x3FC) | (data & 0x03);
		break;
	case 0x12:
		chip->timerB = data;
		break;
	case 0x13:
		/* bit0/1 load A/B, bit2/3 enable IRQ A/B, bit4/5 reset flag A/B.
		   A timer is (re)loaded on the rising edge of its load bit */
		if (~chip->timer_ctrl & data & 1)
			timer_adjust(chip->timA, clocks_to_time(chipnum, 384 * (1024 - chip->timerA)), chipnum, 0);
		if (~chip->timer_ctrl & data & 2)
			timer_adjust(chip->timB, clocks_to_time(chipnum, 384 * 16 * (256 - chip->timerB)), chipnum, 0);
		if (data & 0x10)
		{
			chip->status &= ~0x01;
			chip->irqstate &= ~0x01;
		}
		if (data & 0x20)
		{
			chip->status &= ~0x02;
			chip->irqstate &= ~0x02;
		}
		chip->timer_ctrl = data;
		update_irq(chip);
		break;
	case 0x14:
		chip->ext_address = (chip->ext_address & 0xFFFF00) | data;
		break;
	case 0x15:
		chip->ext_address = (chip->ext_address & 0xFF00FF) | (data << 8);
		break;
	case 0x16:
		chip->ext_address = (chip->ext_address & 0x00FFFF) | ((data & 0x7F) << 16);
		chip->ext_rw = data >> 7;
		/* prime the read latch for the read-direction window */
		if (chip->ext_rw)
			chip->ext_readlatch = read_byte(chip, chip->ext_address);
		break;
	case 0x17:
		/* write to external memory (SRAM); the address is incremented before the write */
		chip->ext_address = (chip->ext_address + 1) & 0x7FFFFF;
		if (!chip->ext_rw && chip->ext_mem_write)
			chip->ext_mem_write(chip->ext_address, data);
		break;
	default:
		break;
	}
}

static void ymf271_w(int chipnum, int offset, int data)
{
	YMF271Chip *chip = &YMF271[chipnum];

	stream_update(chip->stream, 0);

	offset &= 0xF;
	chip->regs_main[offset] = data;

	switch (offset)
	{
	case 0x1: write_fm(chip, 0, chip->regs_main[0x0], data); break;
	case 0x3: write_fm(chip, 1, chip->regs_main[0x2], data); break;
	case 0x5: write_fm(chip, 2, chip->regs_main[0x4], data); break;
	case 0x7: write_fm(chip, 3, chip->regs_main[0x6], data); break;
	case 0x9: write_pcm(chip, chip->regs_main[0x8], data); break;
	case 0xD: write_util(chipnum, chip->regs_main[0xC], data); break;
	default:
		/* even offsets: address latches; 0xB/0xF: unused banks */
		break;
	}
}

static int ymf271_r(int chipnum, int offset)
{
	YMF271Chip *chip = &YMF271[chipnum];

	offset &= 0xF;

	/* update stream before reading status registers */
	if (offset <= 1)
		stream_update(chip->stream, 0);

	switch (offset)
	{
	case 0x0:
		/* d0 TiA, d1 TiB, d2 0, d3 End0, d4 End12, d5 End24, d6 End36, d7 Busy;
		   the End flags are cleared by reading them */
		{
			UINT8 ret = (chip->status & 0x83) | (((chip->end_status >> 0) & 1) << 3) | (((chip->end_status >> 3) & 1) << 4)
				| (((chip->end_status >> 6) & 1) << 5) | (((chip->end_status >> 9) & 1) << 6);
			chip->end_status &= ~0x0249;
			return ret;
		}
	case 0x1:
		/* d0 End4, d1 End16, d2 End28, d3 End40, d4 End8, d5 End20, d6 End32, d7 End44 */
		{
			UINT16 e = chip->end_status;
			UINT8 ret = ((e >> 1) & 1) | (((e >> 4) & 1) << 1) | (((e >> 7) & 1) << 2) | (((e >> 10) & 1) << 3)
				| (((e >> 2) & 1) << 4) | (((e >> 5) & 1) << 5) | (((e >> 8) & 1) << 6) | (((e >> 11) & 1) << 7);
			chip->end_status &= ~0x0DB6;
			return ret;
		}
	case 0x2:
		{
			UINT8 ret;
			if (!chip->ext_rw)
				return 0xFF;
			ret = chip->ext_readlatch;
			chip->ext_address = (chip->ext_address + 1) & 0x7FFFFF;
			chip->ext_readlatch = read_byte(chip, chip->ext_address);
			return ret;
		}
	default:
		break;
	}
	return 0xFF;
}


/* ------------------------------------------------------------------------
   synthesis
   ------------------------------------------------------------------------ */

static void init_tables(void)
{
	int i;
	for (i = 0; i < 256; i++)
	{
		double l = floor(-log(sin((i + 0.5) * 3.14159265358979323846 / 512.0)) / log(2.0) * 256.0 + 0.5);
		double e = floor(pow(2.0, -(i + 1) / 256.0) * 2048.0 + 0.5);
		ymf_logsin[i] = (UINT16)l;
		ymf_exp[i] = (UINT16)e;
	}
}

/* apply one algorithm table to a list of slots */
static void connect(YMF271Chip *chip, const opx_alg *alg, const int *slots, int n)
{
	int p, q;
	for (p = 0; p < n; p++)
	{
		opx_slot *s = &chip->slots[slots[p]];
		s->c_nmod = 0;
		for (q = 0; q < n; q++)
			if (alg->mods[p] & (1 << q))
				s->c_mod[s->c_nmod++] = (UINT8)slots[q];
		s->c_carrier = (alg->car >> p) & 1;
		/* the head slot takes the feedback input; its history is written by position fbsrc */
		s->c_fbhead = (p == 0);
		s->c_fbtarget = (p == alg->fbsrc) ? (INT8)slots[0] : -1;
	}
}

static void rebuild_group(YMF271Chip *chip, int g)
{
	int slots[4];

	chip->groups[g].dirty = 0;
	switch (chip->groups[g].sync)
	{
	case SYNC_4OP:
		slots[0] = g; slots[1] = g + 12; slots[2] = g + 24; slots[3] = g + 36;
		connect(chip, &alg4[chip->slots[g].alg & 15], slots, 4);
		break;
	case SYNC_2X2OP:
		slots[0] = g; slots[1] = g + 24;
		connect(chip, &alg2[chip->slots[g].alg & 3], slots, 2);
		slots[0] = g + 12; slots[1] = g + 36;
		connect(chip, &alg2[chip->slots[g + 12].alg & 3], slots, 2);
		break;
	case SYNC_3OP_PCM:
		slots[0] = g; slots[1] = g + 12; slots[2] = g + 24;
		connect(chip, &alg3[chip->slots[g].alg & 7], slots, 3);
		slots[0] = g + 36;
		connect(chip, &alg_single, slots, 1);
		break;
	default:
		slots[0] = g;      connect(chip, &alg_single, slots, 1);
		slots[0] = g + 12; connect(chip, &alg_single, slots, 1);
		slots[0] = g + 24; connect(chip, &alg_single, slots, 1);
		slots[0] = g + 36; connect(chip, &alg_single, slots, 1);
		break;
	}
}

/* one EG clock (fs/2) for one slot */
static void eg_tick(YMF271Chip *chip, opx_slot *s)
{
	int rks, rate, shift, idx, inc;

	if (s->eg_state == EG_OFF)
		return;

	/* state transitions checked first (OPM) */
	if (s->eg_state == EG_ATTACK && s->eg_att <= 0)
	{
		s->eg_att = 0;
		s->eg_state = EG_DECAY1;
	}
	if (s->eg_state == EG_DECAY1)
	{
		int d1l = (s->d1l == 15) ? (31 << 5) : (s->d1l << 5);
		if (s->eg_att >= d1l)
			s->eg_state = EG_DECAY2;
	}

	if (s->eg_rate_state != s->eg_state + 1)
	{
		rks = rks_tab[s->keycode][s->ks];
		switch (s->eg_state)
		{
		case EG_ATTACK:  rate = eg_rate(s->ar * 2, rks); break;
		case EG_DECAY1:  rate = eg_rate(s->d1r * 2, rks); break;
		case EG_DECAY2:  rate = eg_rate(s->d2r * 2, rks); break;
		default:         rate = eg_rate(s->rr * 4, rks); break;
		}
		s->eg_rate_cache = (UINT8)rate;
		s->eg_rate_state = s->eg_state + 1;
	}
	rate = s->eg_rate_cache;

	if (rate < 48)
	{
		shift = 11 - (rate >> 2);
		if (chip->eg_cnt & ((1 << shift) - 1))
			return;
		idx = (chip->eg_cnt >> shift) & 7;
	}
	else
		idx = chip->eg_cnt & 7;

	inc = (eg_inc[rate] >> (idx * 4)) & 15;
	if (inc == 0)
		return;

	if (s->eg_state == EG_ATTACK)
	{
		s->eg_att += ((~s->eg_att) * inc) >> 4;
		if (s->eg_att <= 0)
		{
			s->eg_att = 0;
			s->eg_state = EG_DECAY1;
		}
	}
	else
	{
		s->eg_att += inc;
		if (s->eg_att >= 0x3FF)
		{
			s->eg_att = 0x3FF;
			if (s->eg_state == EG_RELEASE)
				s->eg_state = EG_OFF;
		}
	}
}

/* LFO clock divider (manual table 2-6-2) */
static UINT32 lfo_period(UINT8 n)
{
	if (n >= 240)
		return 16 - (n & 15);
	return (32 - (n & 15)) << (14 - (n >> 4));
}

/* advance the LFO of one slot by one sample */
static void lfo_tick(opx_slot *s)
{
	if (s->lfo_wave == 0)
		return;
	if (++s->lfo_cnt >= lfo_period(s->lfo_freq))
	{
		s->lfo_cnt = 0;
		s->lfo_pos = (s->lfo_pos + 1) & 127;
	}
}

/* bipolar LFO value for PM, -128..127 (wave 1 saw, 2 square, 3 triangle) */
static INT32 lfo_pm(const opx_slot *s)
{
	INT32 p = s->lfo_pos;
	switch (s->lfo_wave)
	{
	case 1: return ((p + 64) & 127) * 2 - 128;	/* saw */
	case 2: return (p < 64) ? 127 : -128;		/* square */
	case 3:						/* triangle */
		if (p < 32) return p * 4;
		if (p < 96) return 128 - (p - 32) * 4;
		return (p - 96) * 4 - 128;
	default: return 0;
	}
}

/* unipolar LFO value for AM (attenuation), 0..127 */
static INT32 lfo_am(const opx_slot *s)
{
	INT32 p = s->lfo_pos;
	switch (s->lfo_wave)
	{
	case 1: return 127 - p;						/* saw: 127 -> 0 */
	case 2: return (p < 64) ? 127 : 0;				/* square */
	case 3: return (p < 64) ? (127 - p * 2) : (p - 64) * 2 + 1;	/* triangle */
	default: return 0;
	}
}

/* phase increment per sample, 2^32 = one cycle */
static UINT32 phase_inc(const opx_slot *s, INT32 pm)
{
	INT64 inc;
	int sh = s->block_s + 11;	/* 3..18 */
	int dt;
	INT64 fnum = (INT64)s->fnum << 7;	/* fnum with 7 fraction bits for the LFO */

	/* LFO PM: fnum * (1 + k * lfo / (1024 * 128)) */
	if (pm != 0)
		fnum += ((INT64)s->fnum * pms_k[s->pms] * pm) >> 10;
	inc = (fnum << sh) >> 7;
	dt = detune_tab[s->keycode][s->dt & 3] << 12;
	if (s->dt & 4)
		inc -= dt;
	else
		inc += dt;
	if (inc < 0)
		inc = 0;
	if (s->mul == 0)
		inc >>= 1;
	else
		inc *= s->mul;
	return (UINT32)inc;
}

/* phase increment with LFO PM, cached for the last PM value */
static UINT32 phase_inc_pm(opx_slot *s, INT32 pm)
{
	if (!s->pm_valid || s->pm_last != pm)
	{
		s->pm_inc_cache = phase_inc(s, pm);
		s->pm_last = pm;
		s->pm_valid = 1;
	}
	return s->pm_inc_cache;
}

/* external PCM: step in source samples per frame (16-bit fraction) */
static UINT32 pcm_step(const opx_slot *s, INT32 pm)
{
	INT64 inc;
	INT64 fnum = (INT64)((s->fnum & 0x7FF) | 0x800) << 7;	/* 7 fraction bits for the LFO */
	int dt;

	if (pm != 0)
		fnum += ((INT64)((s->fnum & 0x7FF) | 0x800) * pms_k[s->pms] * pm) >> 10;
	inc = (fnum << 16) >> (18 - s->block_s);
	dt = detune_tab[s->keycode][s->dt & 3] << 6;
	if (s->dt & 4)
		inc -= dt;
	else
		inc += dt;
	if (inc < 0)
		inc = 0;
	if (s->mul == 0)
		inc >>= 1;
	else
		inc *= s->mul;
	inc >>= s->pcm_fs;
	return (UINT32)inc;
}

/* 12-bit sample word at index pos of slot s (8-bit data = upper byte) */
static INT32 pcm_word(YMF271Chip *chip, const opx_slot *s, UINT32 pos)
{
	INT32 v;

	if (!s->pcm_12bit)
		v = (INT32)(INT8)read_byte(chip, (s->pcm_start + pos) & 0x7FFFFF) << 4;
	else
	{
		/* 3 bytes per 2 words */
		UINT32 addr = (s->pcm_start + (pos >> 1) * 3) & 0x7FFFFF;
		if (pos & 1)
			v = (read_byte(chip, addr + 2) << 4) | (read_byte(chip, addr + 1) >> 4);
		else
			v = (read_byte(chip, addr) << 4) | (read_byte(chip, addr + 1) & 0x0F);
		v = (INT32)((v & 0xFFF) ^ 0x800) - 0x800;
	}
	return v;
}

/* interpolated 14-bit sample at the current position, then advance */
static void update_inc_cache(opx_slot *s)
{
	if (s->inc_dirty)
	{
		s->inc_cache = phase_inc(s, 0);
		s->pcm_step_cache = pcm_step(s, 0);
		s->inc_dirty = 0;
	}
}

static INT32 pcm_sample(YMF271Chip *chip, opx_slot *s, int slotnum, INT32 pm)
{
	INT32 a = pcm_word(chip, s, s->pcm_pos);
	INT32 b = pcm_word(chip, s, s->pcm_pos + 1);
	INT32 frac = (INT32)(s->pcm_frac >> 8);	/* 8-bit interpolation weight */
	INT32 out = (a * (256 - frac) + b * frac) >> 6;	/* 12 bit -> 14 bit */
	UINT32 step;
	if (pm)
		step = pcm_step(s, pm);
	else
	{
		update_inc_cache(s);
		step = s->pcm_step_cache;
	}

	s->pcm_frac += step;
	s->pcm_pos += s->pcm_frac >> 16;
	s->pcm_frac &= 0xFFFF;
	if (s->pcm_pos >= s->pcm_end)
	{
		if (s->pcm_end > s->pcm_loop)
		{
			UINT32 len = s->pcm_end - s->pcm_loop;
			s->pcm_pos = s->pcm_loop + (s->pcm_pos - s->pcm_end) % len;
		}
		else
			s->pcm_pos = s->pcm_loop;	/* degenerate loop: hold */
		/* The End flag is raised once per key-on */
		if (!s->pcm_ended)
		{
			s->pcm_ended = 1;
			chip->end_status |= 1 << (slotnum >> 2);
		}
	}
	return out;
}

/* multiply a 14-bit sample by the envelope (10-bit attenuation, 64 = 6 dB) */
static INT32 env_mul(INT32 v, UINT32 env)
{
	return (v * (INT32)ymf_exp[(env & 63) << 2]) >> (11 + (env >> 6));
}

/* operator: 10-bit phase, waveform, 10-bit total attenuation -> 14-bit output.
   From this attenuation on the output is 0 for every phase: att >= env * 4 =
   3328, and ymf_exp (at most 2048) * 4 >> 13 is 0 */
#define OP_SILENT_ENV	0x340

static INT32 op(UINT32 phase, int wave, UINT32 env)
{
	UINT32 p = phase & 1023;
	UINT32 idx, att, neg;
	INT32 out;

	idx = p & 255;
	if (p & 256)
		idx ^= 255;
	neg = 0;
	switch (wave)
	{
	case 0:	/* sine */
		att = ymf_logsin[idx];
		neg = p & 512;
		break;
	case 1:	/* +/-sin^2 */
		att = ymf_logsin[idx] << 1;
		neg = p & 512;
		break;
	case 2:	/* |sin| */
		att = ymf_logsin[idx];
		break;
	case 3:	/* half sine */
		if (p & 512)
			return 0;
		att = ymf_logsin[idx];
		break;
	case 4:	/* sin(2wt) on the first half */
	case 5:	/* |sin(2wt)| on the first half */
		if (p & 512)
			return 0;
		idx = (p << 1) & 255;
		if (p & 128)
			idx ^= 255;
		att = ymf_logsin[idx];
		if (wave == 4)
			neg = p & 256;
		break;
	default:	/* 6: linear waveform, 7: external PCM -- both handled in the update */
		return 0;
	}

	att += env << 2;
	if (att >= 4096)
		return 0;
	out = (ymf_exp[att & 255] << 2) >> (att >> 8);
	return neg ? -out : out;
}

/* channel level table: x(1 or 0.75) >> (L>>1), L >= 13 mute */
static INT32 pan(INT32 v, UINT8 level)
{
	if (level >= 13)
		return 0;
	if (level & 1)
		v = (v * 3) >> 2;
	return v >> (level >> 1);
}

static void ymf271_update(int num, INT16 **outputs, int length)
{
	YMF271Chip *chip = &YMF271[num];
	int i, g, n;

	/* the registers do not change inside one update */
	for (g = 0; g < NUM_GROUPS; g++)
		if (chip->groups[g].dirty)
			rebuild_group(chip, g);

	for (i = 0; i < length; i++)
	{
		INT32 acc0 = 0, acc1 = 0;
		int eg_clock;

		/* EG runs at fs/2 */
		eg_clock = chip->eg_phase;
		chip->eg_phase ^= 1;
		if (eg_clock)
			chip->eg_cnt++;

		/* slots in hardware order: modulators with a lower slot number are
		   taken from the current frame, higher-numbered ones from the previous */
		for (n = 0; n < NUM_SLOTS; n++)
		{
			opx_slot *s = &chip->slots[n];
			INT32 mod, out, pm;
			UINT32 env;

			if (eg_clock)
				eg_tick(chip, s);
			lfo_tick(s);

			if (s->eg_state == EG_OFF)
			{
				s->out = 0;
				s->acc = 0;
				continue;
			}

			env = (UINT32)s->eg_att + ((UINT32)s->tl << 3);
			if (s->ams && s->lfo_wave)
			{
				/* AMS max 63 / 126 / 252 units of 0.09375 dB */
				INT32 am = lfo_am(s);
				env += (s->ams == 1) ? (am >> 1) : (s->ams == 2) ? am : (am << 1);
			}
			if (env > 0x3FF)
				env = 0x3FF;

			pm = (s->pms && s->lfo_wave) ? lfo_pm(s) : 0;

			if (s->wave < 6 && env >= OP_SILENT_ENV)
			{
				/* the operator output is 0 whatever the modulation (see op) */
				out = 0;
				if (pm) s->phase += phase_inc_pm(s, pm);
				else { update_inc_cache(s); s->phase += s->inc_cache; }
			}
			else
			{
			/* modulation input, in 10-bit phase units */
			if (s->c_fbhead)
			{
				int fb = s->fb & 7;
				mod = fb ? ((s->fb_hist[0] + s->fb_hist[1]) >> (10 - fb)) : 0;
			}
			else
			{
				INT32 sum = 0;
				int k;
				for (k = 0; k < s->c_nmod; k++)
					sum += chip->slots[s->c_mod[k]].out;
				mod = (sum * modlevel[s->fb]) >> 8;
			}

			if (s->wave == 7)
			{
				/* external PCM data: only the slots of groups 0/4/8 can fetch it */
				out = ((n & 3) == 0) ? env_mul(pcm_sample(chip, s, n, pm), env) : 0;
			}
			else if (s->wave == 6)
			{
				/* "linear waveform table": a DC level of half scale plus the
				   modulation input passed through as a ramp */
				INT32 mm = s->mul ? mod * (INT32)s->mul : (mod >> 1);
				INT32 lin = 8192 + ((mm << 6) & 32767);
				out = env_mul(lin, env);
				if (pm) s->phase += phase_inc_pm(s, pm);	/* the PG keeps running */
				else { update_inc_cache(s); s->phase += s->inc_cache; }
			}
			else
			{
				out = op((s->phase >> 22) + mod, s->wave, env);
				if (pm) s->phase += phase_inc_pm(s, pm);
				else { update_inc_cache(s); s->phase += s->inc_cache; }
			}
			}
			if (s->accon)
			{
				/* "Acc On": the slot output is accumulated, saturating at the 14-bit range */
				s->acc += out;
				if (s->acc > 8191) s->acc = 8191;
				else if (s->acc < -8192) s->acc = -8192;
				out = s->acc;
			}
			s->out = out;

			if (s->c_fbtarget >= 0)
			{
				opx_slot *t = &chip->slots[s->c_fbtarget];
				t->fb_hist[1] = t->fb_hist[0];
				t->fb_hist[0] = out;
			}

			if (s->c_carrier)
			{
				acc0 += pan(out, s->ch_level[0]);
				acc1 += pan(out, s->ch_level[1]);
			}
		}

		/* one carrier at full level = 14-bit +/-8192 = -12 dB FS */
		if (acc0 > 32767) acc0 = 32767; else if (acc0 < -32768) acc0 = -32768;
		if (acc1 > 32767) acc1 = 32767; else if (acc1 < -32768) acc1 = -32768;
		outputs[0][i] = (INT16)acc0;
		outputs[1][i] = (INT16)acc1;
	}
}


/* ------------------------------------------------------------------------
   interface
   ------------------------------------------------------------------------ */

static void ymf271_reset(YMF271Chip *chip)
{
	int i;
	for (i = 0; i < NUM_SLOTS; i++)
	{
		opx_slot *s = &chip->slots[i];
		memset(s, 0, sizeof(*s));
		s->eg_state = EG_OFF;
		s->eg_att = 0x3FF;
		s->c_fbtarget = -1;
		s->inc_dirty = 1;
	}
	for (i = 0; i < NUM_GROUPS; i++)
	{
		chip->groups[i].sync = 0;
		chip->groups[i].pfm = 0;
		chip->groups[i].dirty = 1;
	}
	chip->eg_cnt = 0;
	chip->eg_phase = 0;
	memset(chip->regs_main, 0, sizeof(chip->regs_main));

	timer_adjust(chip->timA, TIME_NEVER, 0, 0);
	timer_adjust(chip->timB, TIME_NEVER, 0, 0);

	chip->timerA = 0;
	chip->timerB = 0;
	chip->timer_ctrl = 0;
	chip->status = 0;
	chip->end_status = 0;
	chip->irqstate = 0;
	chip->ext_address = 0;
	chip->ext_rw = 0;
	chip->ext_readlatch = 0;

	if (chip->irq_callback)
		chip->irq_callback(0);
}

/* time left on the timers, saved so that a loaded state goes on exactly */
static double timer_left[MAX_YMF271][2];

static void ymf271_presave(void)
{
	int i;
	for (i = 0; i < MAX_YMF271; i++)
	{
		YMF271Chip *chip = &YMF271[i];
		if (!chip->timA)
			continue;
		timer_left[i][0] = timer_timeleft(chip->timA);
		timer_left[i][1] = timer_timeleft(chip->timB);
	}
}

static void ymf271_state_register(int i)
{
	YMF271Chip *chip = &YMF271[i];
	char name[32];
	int n;

#define SLOT_U8(field, cnt)  sprintf(name, "s%02d_" #field, n); state_save_register_UINT8("YMF271", i, name, (UINT8 *)&s->field, cnt)
#define SLOT_I8(field, cnt)  sprintf(name, "s%02d_" #field, n); state_save_register_INT8("YMF271", i, name, (INT8 *)&s->field, cnt)
#define SLOT_U16(field, cnt) sprintf(name, "s%02d_" #field, n); state_save_register_UINT16("YMF271", i, name, (UINT16 *)&s->field, cnt)
#define SLOT_U32(field, cnt) sprintf(name, "s%02d_" #field, n); state_save_register_UINT32("YMF271", i, name, (UINT32 *)&s->field, cnt)
#define SLOT_I32(field, cnt) sprintf(name, "s%02d_" #field, n); state_save_register_INT32("YMF271", i, name, (INT32 *)&s->field, cnt)

	for (n = 0; n < NUM_SLOTS; n++)
	{
		opx_slot *s = &chip->slots[n];
		SLOT_U8(kon, 1); SLOT_U8(ext_en, 1); SLOT_U8(ext_out, 1); SLOT_U8(lfo_freq, 1);
		SLOT_U8(ams, 1); SLOT_U8(pms, 1); SLOT_U8(lfo_wave, 1); SLOT_U8(dt, 1); SLOT_U8(mul, 1);
		SLOT_U8(tl, 1); SLOT_U8(ks, 1); SLOT_U8(ar, 1); SLOT_U8(d1r, 1); SLOT_U8(d2r, 1);
		SLOT_U8(d1l, 1); SLOT_U8(rr, 1); SLOT_U16(fnum, 1); SLOT_U8(block, 1); SLOT_U8(fnum_latch, 1);
		SLOT_U8(accon, 1); SLOT_U8(fb, 1); SLOT_U8(wave, 1); SLOT_U8(alg, 1); SLOT_U8(ch_level[0], 4);
		SLOT_U32(pcm_start, 1); SLOT_U32(pcm_end, 1); SLOT_U32(pcm_loop, 1); SLOT_U8(pcm_altloop, 1);
		SLOT_U8(pcm_fs, 1); SLOT_U8(pcm_12bit, 1); SLOT_U8(pcm_srcnote, 1); SLOT_U8(pcm_srcb, 1);
		SLOT_I8(block_s, 1); SLOT_U8(keycode, 1); SLOT_U8(eg_state, 1); SLOT_I32(eg_att, 1);
		SLOT_U32(phase, 1); SLOT_I32(out, 1); SLOT_I32(acc, 1); SLOT_I32(fb_hist[0], 2);
		SLOT_U32(lfo_cnt, 1); SLOT_U8(lfo_pos, 1); SLOT_U32(pcm_pos, 1); SLOT_U32(pcm_frac, 1);
		SLOT_U8(pcm_ended, 1); SLOT_U8(c_nmod, 1); SLOT_U8(c_mod[0], 3); SLOT_U8(c_fbhead, 1);
		SLOT_I8(c_fbtarget, 1); SLOT_U8(c_carrier, 1);
	}
	for (n = 0; n < NUM_GROUPS; n++)
	{
		sprintf(name, "g%02d_sync", n); state_save_register_UINT8("YMF271", i, name, &chip->groups[n].sync, 1);
		sprintf(name, "g%02d_pfm", n); state_save_register_UINT8("YMF271", i, name, &chip->groups[n].pfm, 1);
		sprintf(name, "g%02d_dirty", n); state_save_register_UINT8("YMF271", i, name, &chip->groups[n].dirty, 1);
	}
	state_save_register_UINT8("YMF271", i, "regs_main", chip->regs_main, 0x10);
	state_save_register_UINT16("YMF271", i, "timerA", &chip->timerA, 1);
	state_save_register_UINT8("YMF271", i, "timerB", &chip->timerB, 1);
	state_save_register_UINT8("YMF271", i, "timer_ctrl", &chip->timer_ctrl, 1);
	state_save_register_UINT8("YMF271", i, "status", &chip->status, 1);
	state_save_register_UINT16("YMF271", i, "end_status", &chip->end_status, 1);
	state_save_register_UINT8("YMF271", i, "irqstate", &chip->irqstate, 1);
	state_save_register_UINT32("YMF271", i, "ext_address", &chip->ext_address, 1);
	state_save_register_UINT8("YMF271", i, "ext_rw", &chip->ext_rw, 1);
	state_save_register_UINT8("YMF271", i, "ext_readlatch", &chip->ext_readlatch, 1);
	state_save_register_UINT32("YMF271", i, "eg_cnt", &chip->eg_cnt, 1);
	state_save_register_UINT8("YMF271", i, "eg_phase", &chip->eg_phase, 1);
	state_save_register_double("YMF271", i, "timA_left", &timer_left[i][0], 1);
	state_save_register_double("YMF271", i, "timB_left", &timer_left[i][1], 1);
}

static void ymf271_postload(void)
{
	int i, n;
	for (i = 0; i < MAX_YMF271; i++)
	{
		YMF271Chip *chip = &YMF271[i];
		if (!chip->timA)
			continue;
		for (n = 0; n < NUM_SLOTS; n++)
		{
			chip->slots[n].inc_dirty = 1;
			chip->slots[n].pm_valid = 0;
			chip->slots[n].eg_rate_state = 0;
		}
		/* the timers go on from the time they had left */
		timer_adjust(chip->timA, (chip->timer_ctrl & 1) ? timer_left[i][0] : TIME_NEVER, i, 0);
		timer_adjust(chip->timB, (chip->timer_ctrl & 2) ? timer_left[i][1] : TIME_NEVER, i, 0);
	}
}

int YMF271_sh_start( const struct MachineSound *msound )
{
	char buf[2][40];
	const char *name[2];
	int vol[2];
	const struct YMF271interface *intf = msound->sound_interface;
	int i;

	init_tables();

	for (i = 0; i < intf->num; i++)
	{
		YMF271Chip *chip = &YMF271[i];
		UINT32 rmask;

		memset(chip, 0, sizeof(*chip));
		chip->timA = timer_alloc(timer_a_expired);
		chip->timB = timer_alloc(timer_b_expired);
		chip->rom = memory_region(intf->region[i]);
		chip->rom_size = chip->rom ? memory_region_length(intf->region[i]) : 0;
		rmask = chip->rom_size ? chip->rom_size - 1 : 0;
		rmask |= rmask >> 1; rmask |= rmask >> 2; rmask |= rmask >> 4; rmask |= rmask >> 8; rmask |= rmask >> 16;
		chip->rom_mask = rmask;
		chip->irq_callback = intf->irq_callback[i];
		chip->ext_mem_read = intf->ext_read[i];
		chip->ext_mem_write = intf->ext_write[i];

		sprintf(buf[0], "YMF271 %d L", i);
		sprintf(buf[1], "YMF271 %d R", i);
		name[0] = buf[0];
		name[1] = buf[1];
		/* as YM3012_VOL: the left output (CH0) in the low 16 bits */
		vol[0] = intf->mixing_level[i] & 0xffff;
		vol[1] = intf->mixing_level[i] >> 16;
		/* outputs CH0 / CH1 at the chip's sample rate, clock / 384 = 44.1 kHz */
		ymf271_clock[i] = intf->clock[i] ? intf->clock[i] : YMF271_CLOCK;
		chip->stream = stream_init_multi(2, name, vol, ymf271_clock[i] / 384, i, ymf271_update);

		ymf271_reset(chip);
		ymf271_state_register(i);
	}
	state_save_register_func_presave(ymf271_presave);
	state_save_register_func_postload(ymf271_postload);

	return 0;
}

void YMF271_sh_stop( void )
{
}

void YMF271_sh_reset( void )
{
	int i;
	for (i = 0; i < MAX_YMF271; i++)
		if (YMF271[i].timA)
			ymf271_reset(&YMF271[i]);
}

READ_HANDLER( YMF271_0_r )
{
	return ymf271_r(0, offset);
}

WRITE_HANDLER( YMF271_0_w )
{
	ymf271_w(0, offset, data);
}

READ_HANDLER( YMF271_1_r )
{
	return ymf271_r(1, offset);
}

WRITE_HANDLER( YMF271_1_w )
{
	ymf271_w(1, offset, data);
}
