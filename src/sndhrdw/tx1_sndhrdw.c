/***************************************************************************

	Tatsumi TX-1 / Buggy Boy sound hardware

	From current MAME (src/mame/tatsumi/tx1_a.cpp, Philip Bennett)

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "vidhrdw/res_net.h"
#include "includes/tx1.h"


/*************************************
 *
 *  8253 Programmable Interval Timer
 *
 *************************************/

static struct
{
	UINT16	counts[3];
	INT32	idx[3];
} pit8253;

data8_t *tx1_z80_ram;

static int stream;
static int buggyboyjr;

static UINT32 freq_to_step;
static UINT32 step0;
static UINT32 step1;
static UINT32 step2;

static UINT8 ay_outputa;
static UINT8 ay_outputb;

static UINT8 ppi_latch_a;
static UINT8 ppi_latch_b;
static UINT32 ts;

static INT32 pit0;
static INT32 pit1;
static INT32 pit2;

/* the gain of the TX-1 engine outputs (MAME: the output gain of the sound
   device, set by the AY port B), in percent */
static INT32 tx1_gain;

static double weights0[4];
static double weights1[3];
static double weights2[3];
static INT32 eng0[4];
static INT32 eng1[4];
static INT32 eng2[4];

static INT32 noise_lfsra;
static INT32 noise_lfsrb;
static INT32 noise_lfsrc;
static INT32 noise_lfsrd;
static INT32 noise_counter;
static UINT8 ym1_outputa;
static UINT8 ym2_outputa;
static UINT8 ym2_outputb;
static UINT16 eng_voltages[16];

static void sound_state_register(void)
{
	state_save_register_UINT16("tx1_sound", 0, "pit_counts", pit8253.counts, 3);
	state_save_register_INT32 ("tx1_sound", 0, "pit_idx",    pit8253.idx, 3);
	state_save_register_UINT32("tx1_sound", 0, "step0",      &step0, 1);
	state_save_register_UINT32("tx1_sound", 0, "step1",      &step1, 1);
	state_save_register_UINT32("tx1_sound", 0, "step2",      &step2, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ay_outputa", &ay_outputa, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ay_outputb", &ay_outputb, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ppi_latch_a", &ppi_latch_a, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ppi_latch_b", &ppi_latch_b, 1);
	state_save_register_UINT32("tx1_sound", 0, "ts",         &ts, 1);
	state_save_register_INT32 ("tx1_sound", 0, "pit0",       &pit0, 1);
	state_save_register_INT32 ("tx1_sound", 0, "pit1",       &pit1, 1);
	state_save_register_INT32 ("tx1_sound", 0, "pit2",       &pit2, 1);
	state_save_register_INT32 ("tx1_sound", 0, "gain",       &tx1_gain, 1);
	state_save_register_INT32 ("tx1_sound", 0, "eng0",       eng0, 4);
	state_save_register_INT32 ("tx1_sound", 0, "eng1",       eng1, 4);
	state_save_register_INT32 ("tx1_sound", 0, "eng2",       eng2, 4);
	state_save_register_INT32 ("tx1_sound", 0, "noise_lfsra", &noise_lfsra, 1);
	state_save_register_INT32 ("tx1_sound", 0, "noise_lfsrb", &noise_lfsrb, 1);
	state_save_register_INT32 ("tx1_sound", 0, "noise_lfsrc", &noise_lfsrc, 1);
	state_save_register_INT32 ("tx1_sound", 0, "noise_lfsrd", &noise_lfsrd, 1);
	state_save_register_INT32 ("tx1_sound", 0, "noise_counter", &noise_counter, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ym1_outputa", &ym1_outputa, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ym2_outputa", &ym2_outputa, 1);
	state_save_register_UINT8 ("tx1_sound", 0, "ym2_outputb", &ym2_outputb, 1);
}

static void sound_reset_state(void)
{
	memset(&pit8253, 0, sizeof(pit8253));
	step0 = step1 = step2 = 0;
	ay_outputa = ay_outputb = 0;
	ppi_latch_a = ppi_latch_b = 0;
	ts = 0;
	pit0 = pit1 = pit2 = 0;
	tx1_gain = 100;
	memset(eng0, 0, sizeof(eng0));
	memset(eng1, 0, sizeof(eng1));
	memset(eng2, 0, sizeof(eng2));
	noise_lfsra = 0;
	noise_lfsrb = 1;
	noise_lfsrc = 0;
	noise_lfsrd = 0;
	noise_counter = 0;
	ym1_outputa = ym2_outputa = ym2_outputb = 0;
}

static void update_stream(void)
{
	if (Machine->sample_rate)
		stream_update(stream, 0);
}


/*************************************
 *
 *  TX-1
 *
 *************************************/

/* RC oscillator: 1785Hz */
#define TX1_NOISE_CLOCK     (1/(1000.0e-12 * 560000.0))
#define TX1_PIT_CLOCK       (TX1_PIXEL_CLOCK / 16)
#define TX1_FRAC            30

#define TX1_SHUNT           (250.0)
#define TX1_R0              (180000.0 + TX1_SHUNT)
#define TX1_R1              (56000.0  + TX1_SHUNT)
#define TX1_R2              (22000.0  + TX1_SHUNT)
#define TX1_R               (100000.0 + TX1_SHUNT)
#define TX1_RI              (180000.0)

static const double tx1_engine_gains[16] =
{
	-( TX1_R )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2 + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_R2 + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT ) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2 + 1.0/TX1_R0) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2 + 1.0/TX1_R1) )/TX1_RI,
	-( 1.0/(1.0/TX1_R + 1.0/TX1_SHUNT + 1.0/TX1_R2 + 1.0/TX1_R1 + 1.0/TX1_R0) )/TX1_RI
};

/* Main CPU and Z80 synchronisation */
WRITE16_HANDLER( tx1_z80_busreq_w )
{
	cpu_set_halt_line(2, (data & 1) ? CLEAR_LINE : ASSERT_LINE);
}

/* Z80 can trigger its own interrupts */
WRITE_HANDLER( tx1_z80_intreq_w )
{
	cpu_set_irq_line(2, 0, HOLD_LINE);
}

READ16_HANDLER( tx1_z80_shared_r )
{
	return cpunum_read_byte(2, offset);
}

WRITE16_HANDLER( tx1_z80_shared_w )
{
	cpunum_write_byte(2, offset, data & 0xff);
}

/*
	(TODO) TS: Connected in place of dipswitch A bit 0
	Accessed on startup as some sort of acknowledgement
*/
WRITE_HANDLER( tx1_ts_w )
{
	/*  TS = 1; */
	tx1_z80_ram[offset & 0x7ff] = data;
}

READ_HANDLER( tx1_ts_r )
{
	/*  TS = 1; */
	return tx1_z80_ram[offset & 0x7ff];
}

static UINT8 bit_reverse8(UINT8 val)
{
	val = ((val & 0xF0) >>  4) | ((val & 0x0F) <<  4);
	val = ((val & 0xCC) >>  2) | ((val & 0x33) <<  2);
	val = ((val & 0xAA) >>  1) | ((val & 0x55) <<  1);

	return val;
}

READ16_HANDLER( tx1_dipswitches_r )
{
	return (readinputport(TX1_PORT_DSW) & 0xfffe) | ts;
}

/* Tazmi TZ2103 custom 4-channel A/D converter @ 7.5 MHz */
READ_HANDLER( bb_analog_r )
{
	if (getenv("TX1DBG") && offset == 0 && cpu_getcurrentframe() % 100 == 0) fprintf(stderr, "frame %d accel %02x steer %02x brake %02x\n", cpu_getcurrentframe(), readinputport(TX1_PORT_ACCEL), readinputport(TX1_PORT_STEERING), readinputport(TX1_PORT_BRAKE));
	if (offset == 0)
		return bit_reverse8(((readinputport(TX1_PORT_ACCEL) & 0xf) << 4) | (readinputport(TX1_PORT_STEERING) & 0xf));
	else
		return bit_reverse8((readinputport(TX1_PORT_BRAKE) & 0xf) << 4);
}

READ_HANDLER( bbjr_analog_r )
{
	if (offset == 0)
		return ((readinputport(TX1_PORT_ACCEL) & 0xf) << 4) | (readinputport(TX1_PORT_STEERING) & 0xf);
	else
		return (readinputport(TX1_PORT_BRAKE) & 0xf) << 4;
}

WRITE_HANDLER( tx1_coin_cnt_w )
{
	coin_counter_w(0, data & 0x80);
	coin_counter_w(1, data & 0x40);
}

WRITE_HANDLER( bb_coin_cnt_w )
{
	coin_counter_w(0, data & 0x01);
	coin_counter_w(1, data & 0x02);
	/*  coin_counter_w(2, data & 0x04); */
}

WRITE_HANDLER( tx1_ppi_latch_w )
{
	ppi_latch_a = ((readinputport(TX1_PORT_BRAKE) & 0xf) << 4) | (readinputport(TX1_PORT_ACCEL) & 0xf);
	ppi_latch_b = readinputport(TX1_PORT_STEERING) & 0xf;
}

READ_HANDLER( tx1_ppi_porta_r )
{
	return ppi_latch_a;
}

READ_HANDLER( tx1_ppi_portb_r )
{
	/* upper nibble goes to unpopulated DS.3 */
	return ppi_latch_b & 0xf;
}

WRITE_HANDLER( tx1_pit8253_w )
{
	update_stream();

	if (offset < 3)
	{
		if (pit8253.idx[offset] == 0)
		{
			pit8253.counts[offset] = (pit8253.counts[offset] & 0xff00) | data;
			pit8253.idx[offset] = 1;
		}
		else
		{
			pit8253.counts[offset] = (pit8253.counts[offset] & 0x00ff) | (data << 8);
			pit8253.idx[offset] = 0;
		}
	}
	else
	{
		int mode = (data >> 1) & 7;

		if (mode == 3)
		{
			int cntsel = (data >> 6) & 3;
			/* as MAME: a select of 3 is the read-back command of the
			   8254, it would index out of the counters */
			if (cntsel < 3)
			{
				pit8253.idx[cntsel] = 0;
				pit8253.counts[cntsel] = 0;
			}
		}
	}
}

READ_HANDLER( tx1_pit8253_r )
{
	return 0;
}

/* Periodic Z80 interrupt */
INTERRUPT_GEN( tx1_z80_irq )
{
	cpu_set_irq_line(2, 0, HOLD_LINE);
}

/***************************************************************************

	AY-8910 port mappings:

	Port A                      Port B
	=======                     ======

	0: Engine 0 gain #0         0: Engine 0 gain #8
	1: Engine 0 gain #1         1: Engine 0 gain #9
	2: Engine 0 gain #2         2: Engine 0 gain #10
	3: Engine 0 gain #3         3: Engine 0 gain #11
	4: Engine 0 gain #4         4: /Enable AY on speaker CR
	5: Engine 0 gain #5         5: /Enable Engines 1/2 on speakers LR/RR/CF
	6: Engine 0 gain #6         6: /Skid 0 enable
	7: Engine 0 gain #7         7: /Skid 1 enable

***************************************************************************/

WRITE_HANDLER( tx1_ay8910_a_w )
{
	update_stream();

	/* All outputs inverted */
	ay_outputa = ~data;
}

WRITE_HANDLER( tx1_ay8910_b_w )
{
	update_stream();

	/* Only B3-0 are inverted */
	ay_outputb = data ^ 0xf;

	/* It'll do until we get quadrophonic speaker support! */
	tx1_gain = BIT(ay_outputb, 4) ? 150 : 200;
}

/***************************************************************************

	Engine sounds are produced by three of these 4013B chains:

	            +------------------
	            |  +---------+
	    +-----+ |  | +-----+ |
	  +-|D 1 Q|-+  +-|D 2 Q|-|-----
	  | |C  /Q|-o----|C  /Q|-+
	  | +-----+ |    +-----+
	  +---------+

	 +----------+
	 |  +-----+ |    +-----+
	 |  |C 3 Q|-+    |C 4 Q|-------
	!&--|D  /Q|------|D  /Q|-+
	 |  +-----+      +-----+ |
	 +-----------------------+

	 Common clocks omitted for clarity (all driven from an 8253 output pin).

	     Player: ES0, ES1, ES2
	 Opponent 2: ES2

 ***************************************************************************/

INLINE void update_engine(INT32 eng[4])
{
	INT32 p0 = eng[0];
	INT32 p1 = eng[1];
	INT32 p2 = eng[2];
	INT32 p3 = eng[3];

	eng[0] = !p0;
	if (p0 && !eng[0]) eng[1] = !p1;
	eng[2] = !(p2 && !p3);
	eng[3] = !p2;
}

INLINE INT16 clip16(double v)
{
	if (v > 32767.0) return 32767;
	if (v < -32768.0) return -32768;
	return (INT16)v;
}

/* MAME outputs value / 32768 routed at 0.2: the stream outputs value * 0.2
   at full level, with the AY at MAME's 0.1 */
#define TX1_OUT_SCALE	0.2

static void tx1_stream_update(int param, INT16 **buffer, int length)
{
	INT16 *bufl = buffer[0];
	INT16 *bufr = buffer[1];
	UINT32 step_0, step_1, step_2;
	double /*gain_0, gain_1,*/ gain_2, gain_3, gain;
	int sampindex;

	/* 8253 outputs for the player/opponent engine sounds. */
	step_0 = pit8253.counts[0] ? ((UINT32)(TX1_PIT_CLOCK / pit8253.counts[0]) * freq_to_step) : 0;
	step_1 = pit8253.counts[1] ? ((UINT32)(TX1_PIT_CLOCK / pit8253.counts[1]) * freq_to_step) : 0;
	step_2 = pit8253.counts[2] ? ((UINT32)(TX1_PIT_CLOCK / pit8253.counts[2]) * freq_to_step) : 0;

	/*gain_0 = tx1_engine_gains[ay_outputa & 0xf];*/
	/*gain_1 = tx1_engine_gains[ay_outputa >> 4];*/
	gain_2 = tx1_engine_gains[ay_outputb & 0xf];
	gain_3 = BIT(ay_outputb, 5) ? 1.0f : 1.5f;
	gain = tx1_gain / 100.0 * TX1_OUT_SCALE;

	for (sampindex = 0; sampindex < length; sampindex++)
	{
		INT16 out;

		if (step0 & ((1 << TX1_FRAC)))
		{
			update_engine(eng0);
			pit0 = combine_4_weights(weights0, eng0[0], eng0[1], eng0[2], eng0[3]);
			step0 &= ((1 << TX1_FRAC) - 1);
		}

		if (step1 & ((1 << TX1_FRAC)))
		{
			update_engine(eng1);
			pit1 = combine_3_weights(weights1, eng1[0], eng1[1], eng1[3]);
			step1 &= ((1 << TX1_FRAC) - 1);
		}

		if (step2 & ((1 << TX1_FRAC)))
		{
			update_engine(eng2);
			pit2 = combine_3_weights(weights2, eng2[0], eng2[1], eng2[3]);
			step2 &= ((1 << TX1_FRAC) - 1);
		}

		out = clip16(((pit0 + pit1) * gain_3 + 2 * pit2 * gain_2) * gain);
		bufl[sampindex] = out;
		bufr[sampindex] = out;

		step0 += step_0;
		step1 += step_1;
		step2 += step_2;
	}
}

int tx1_sh_start(const struct MachineSound *msound)
{
	static const int r0[4] = { 390000, 180000, 180000, 180000 };
	static const int r1[3] = { 180000, 390000, 56000 };
	static const int r2[3] = { 390000, 390000, 180000 };
	static const char *names[2] = { "TX-1 Engine L", "TX-1 Engine R" };
	static const int vols[2] = { MIXER(100, MIXER_PAN_LEFT), MIXER(100, MIXER_PAN_RIGHT) };

	buggyboyjr = 0;

	/* Allocate the stream */
	stream = stream_init_multi(2, names, vols, Machine->sample_rate, 0, tx1_stream_update);
	freq_to_step = Machine->sample_rate ? (UINT32)((double)(1 << TX1_FRAC) / (double)Machine->sample_rate) : 0;

	/* Compute the engine resistor weights */
	compute_resistor_weights(0, 10000, -1.0,
			4, &r0[0], weights0, 0, 0,
			3, &r1[0], weights1, 0, 0,
			3, &r2[0], weights2, 0, 0);

	sound_reset_state();
	sound_state_register();
	return 0;
}

void tx1_sh_reset(void)
{
	update_stream();
	step0 = step1 = step2 = 0;
}


/*************************************
 *
 *  Buggy Boy
 *
 *************************************/

#define BUGGYBOY_PIT_CLOCK      (BUGGYBOY_ZCLK / 8)
#define BUGGYBOY_NOISE_CLOCK    (BUGGYBOY_PIT_CLOCK / 4)

#define BUGGYBOY_R1     47000.0
#define BUGGYBOY_R2     22000.0
#define BUGGYBOY_R3     10000.0
#define BUGGYBOY_R4     5600.0
#define BUGGYBOY_SHUNT  250.0

#define BUGGYBOY_R1S    (1.0/(1.0/BUGGYBOY_R1 + 1.0/BUGGYBOY_SHUNT))
#define BUGGYBOY_R2S    (1.0/(1.0/BUGGYBOY_R2 + 1.0/BUGGYBOY_SHUNT))
#define BUGGYBOY_R3S    (1.0/(1.0/BUGGYBOY_R3 + 1.0/BUGGYBOY_SHUNT))
#define BUGGYBOY_R4S    (1.0/(1.0/BUGGYBOY_R4 + 1.0/BUGGYBOY_SHUNT))

static const double bb_engine_gains[16] =
{
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1  + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2  + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3  + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4 ) + 1.0/100e3)/100e3,
	-1.0/(1.0/(BUGGYBOY_R1S + BUGGYBOY_R2S + BUGGYBOY_R3S + BUGGYBOY_R4S) + 1.0/100e3)/100e3,
};

/***************************************************************************

	YM-2149, IC24 port mappings:

	Port A                      Port B
	=======                     ======

	0: Engine 1 gain (FR) #0    0: *Coin Counter 1
	1: Engine 1 gain (FR) #1    1: *Coin Counter 2
	2: Engine 1 gain (FR) #2    2: *Coin Counter 3 (Unused)
	3: Engine 1 gain (FR) #3    3: *Engine 0 gain
	4: Engine 1 gain (FL) #0    4: Skid 0 enable
	5: Engine 1 gain (FL) #1    5: Skid 1 enable
	6: Engine 1 gain (FL) #2    6: Enable YM IC24 output on RR
	7: Engine 1 gain (FL) #3    7: Enable YM IC19 output on RL

	(* Buggy Boy Junior only)

	The engine sounds are generated by an 8253. There are two channels.

	#0 is the player's buggy
	#1 is the opponents' buggies

	          +------------> GAIN[1] +--> FL
	          |                      +--> FR
	8255 #0 --+--> BL
	          +--> BR

	8255 #1 --+--> GAIN[2] ---> FL
	          +--> GAIN[3] ---> FR


	[1] is used to amplify sound during tunnel.
	[2] and [3] are stereo fades

***************************************************************************/

WRITE_HANDLER( bb_ym1_a_w )
{
	update_stream();
	ym1_outputa = data ^ 0xff;
}

WRITE_HANDLER( bb_ym2_a_w )
{
	update_stream();
	ym2_outputa = data ^ 0xff;
}

/* Until we support > 2 speakers, double the gain of the front speakers */
/* (the YM2149 mixing level is doubled, so 100 here is the doubled gain) */
static void bb_set_ym_gains(void)
{
	/* Rear left speaker */
	AY8910_set_volume(0, ALL_8910_CHANNELS, (ym2_outputb & 0x80) ? 100 : 50);

	/* Rear right speaker */
	AY8910_set_volume(1, ALL_8910_CHANNELS, (ym2_outputb & 0x40) ? 100 : 50);
}

WRITE_HANDLER( bb_ym2_b_w )
{
	update_stream();

	ym2_outputb = data ^ 0xff;
	bb_set_ym_gains();
}

WRITE_HANDLER( bbjr_ym2_b_w )
{
	bb_ym2_b_w(offset, data);

	coin_counter_w(0, ~data & 0x01);
	coin_counter_w(1, ~data & 0x02);
}

/* MAME outputs value / 32768 routed at 0.2: the stream outputs value * 0.2
   at full level, with the YMs at MAME's 0.15 */
#define BB_OUT_SCALE	0.2

static void buggyboy_stream_update(int param, INT16 **buffer, int length)
{
	/* This is admittedly a bit of a hack job... */

	INT16 *bufl = buffer[0];
	INT16 *bufr = buffer[1];
	UINT32 step_0, step_1;
	int n1_en, n2_en;
	double gain0, gain1_l, gain1_r;
	int sampindex, i, noise_steps;

	/* 8253 outputs for the player/opponent buggy engine sounds. */
	step_0 = pit8253.counts[0] ? ((UINT32)(BUGGYBOY_PIT_CLOCK / pit8253.counts[0]) * freq_to_step) : 0;
	step_1 = pit8253.counts[1] ? ((UINT32)(BUGGYBOY_PIT_CLOCK / pit8253.counts[1]) * freq_to_step) : 0;

	if (buggyboyjr)
		gain0 = BIT(ym2_outputb, 3) ? 1.0 : 2.0;
	else
		gain0 = BIT(ym1_outputa, 3) ? 1.0 : 2.0;

	n1_en = BIT(ym2_outputb, 4);
	n2_en = BIT(ym2_outputb, 5);

	gain1_l = bb_engine_gains[ym2_outputa >> 4] * 5;
	gain1_r = bb_engine_gains[ym2_outputa & 0xf] * 5;

	noise_steps = Machine->sample_rate ? BUGGYBOY_NOISE_CLOCK / Machine->sample_rate : 0;

	for (sampindex = 0; sampindex < length; sampindex++)
	{
		INT32 pit0_v, pit1_v, n1, n2;
		pit0_v = eng_voltages[(step0 >> 24) & 0xf];
		pit1_v = eng_voltages[(step1 >> 24) & 0xf];

		/* Calculate the tyre screech noise source */
		for (i = 0; i < noise_steps; ++i)
		{
			/* CD4006 is a 4-4-1-4-4-1 shift register */
			int p13 = BIT(noise_lfsra, 3);
			int p12 = BIT(noise_lfsrb, 4);
			int p10 = BIT(noise_lfsrc, 3);
			int p8 = BIT(noise_lfsrd, 3);

			/* Update the register */
			noise_lfsra = p12 | ((noise_lfsra << 1) & 0xf);
			noise_lfsrb = (p8 ^ p12) | ((noise_lfsrb << 1) & 0x1f);
			noise_lfsrc = p13 | ((noise_lfsrc << 1) & 0xf);
			noise_lfsrd = p10 | ((noise_lfsrd << 1) & 0x1f);

			/* 4040 12-bit counter is clocked on the falling edge of Q13 */
			if (!BIT(noise_lfsrc, 3) && p10)
				noise_counter = (noise_counter + 1) & 0x0fff;
		}

		if (n1_en)
		{
			n1 = !BIT(noise_counter, 7-1) * 16000;
			if (BIT(noise_counter, 11-1)) n1 /=2;
		}
		else
			n1 = 8192;

		if (n2_en)
		{
			n2 = !BIT(noise_counter, 6-1) * 16000;
			if (BIT(noise_counter, 11-1)) n2 /=2;
		}
		else
			n2 = 8192;

		bufl[sampindex] = clip16((n1 + n2 + (pit0_v * gain0) + (pit1_v * gain1_l)) * BB_OUT_SCALE);
		bufr[sampindex] = clip16((n1 + n2 + (pit0_v * gain0) + (pit1_v * gain1_r)) * BB_OUT_SCALE);

		step0 += step_0;
		step1 += step_1;
	}
}

static void buggyboy_postload(void)
{
	bb_set_ym_gains();
}

int buggyboy_sh_start(const struct MachineSound *msound)
{
	static const int resistors[4] = { 330000, 220000, 330000, 220000 };
	static const int tmp[16] =
	{
		0x0, 0x1, 0xe, 0xf, 0x8, 0x9, 0x6, 0x7, 0xc, 0xd, 0xe, 0xf, 0x4, 0x5, 0x6, 0x7
	};
	static const char *names[2] = { "Buggy Boy Engine L", "Buggy Boy Engine R" };
	static const int vols[2] = { MIXER(100, MIXER_PAN_LEFT), MIXER(100, MIXER_PAN_RIGHT) };
	double aweights[4];
	int i;

	buggyboyjr = !strcmp(Machine->gamedrv->name, "buggyboyjr");

	compute_resistor_weights(0, 16384,  -1.0,
							4,  &resistors[0], aweights, 0, 0,
							0, 0, 0, 0, 0,
							0, 0, 0, 0, 0 );

	for (i = 0; i < 16; i++)
		eng_voltages[i] = combine_4_weights(aweights, BIT(tmp[i], 0), BIT(tmp[i], 1), BIT(tmp[i], 2), BIT(tmp[i], 3));

	/* Allocate the stream */
	stream = stream_init_multi(2, names, vols, Machine->sample_rate, 0, buggyboy_stream_update);
	freq_to_step = Machine->sample_rate ? (UINT32)((double)(1 << 24) / (double)Machine->sample_rate) : 0;

	sound_reset_state();
	sound_state_register();
	state_save_register_func_postload(buggyboy_postload);
	return 0;
}

void buggyboy_sh_reset(void)
{
	update_stream();
	step0 = step1 = 0;

	/* Reset noise LFSR */
	noise_lfsra = 0;
	noise_lfsrb = 1;
	noise_lfsrc = 0;
	noise_lfsrd = 0;

	bb_set_ym_gains();
}
