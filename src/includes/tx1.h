/***************************************************************************

	Tatsumi TX-1 / Buggy Boy hardware

	From current MAME (src/mame/tatsumi/tx1*, Philip Bennett)

***************************************************************************/

#ifndef TX1_H
#define TX1_H

#include "cpu/i86/i86intf.h"

#define TX1_PIXEL_CLOCK		(18000000 / 3)
#define TX1_HBSTART			256
#define TX1_HTOTAL			384
#define TX1_VBSTART			240
#define TX1_VTOTAL			264

/* as MAME: VTOTAL increased to fix a missing start countdown tone */
#define BB_PIXEL_CLOCK		(18000000 / 3)
#define BB_HBSTART			256
#define BB_HTOTAL			384
#define BB_VBSTART			240
#define BB_VTOTAL			(288 + 1)

#define CPU_MASTER_CLOCK	15000000
#define BUGGYBOY_ZCLK		(CPU_MASTER_CLOCK / 2)

/* the TEST pin of the 8086 (WAIT instruction), see cpu/i86 */
#define TX1_TEST_LINE		I86_TEST_LINE

/* machine (tx1_machine.c): the arithmetic unit */
struct tx1_math_t
{
	UINT16	cpulatch;
	UINT16	promaddr;
	UINT16	inslatch;
	UINT32	mux;
	UINT16	ppshift;
	UINT32	i0ff;
	UINT16	retval;
	UINT16	muxlatch;	/* TX-1 */
};

/* SN74S516 16x16 multiplier/divider */
struct tx1_sn74s516_t
{
	INT16	X;
	INT16	Y;
	INT32	ZW32;		/* Z: high 16 bits, W: low 16 bits */
	INT32	code;
	INT32	state;
	INT32	ZWfl;
};

extern struct tx1_math_t tx1_math;
extern struct tx1_sn74s516_t tx1_sn74s516;

READ16_HANDLER( tx1_math_r );
WRITE16_HANDLER( tx1_math_w );
READ16_HANDLER( tx1_spcs_rom_r );
READ16_HANDLER( tx1_spcs_ram_r );
WRITE16_HANDLER( tx1_spcs_ram_w );
READ16_HANDLER( buggyboy_math_r );
WRITE16_HANDLER( buggyboy_math_w );
READ16_HANDLER( buggyboy_spcs_rom_r );
READ16_HANDLER( buggyboy_spcs_ram_r );
WRITE16_HANDLER( buggyboy_spcs_ram_w );
MACHINE_INIT( tx1 );
void tx1_machine_state_register(void);

/* video (tx1_vidhrdw.c) */
extern data16_t *tx1_math_ram;
extern data16_t *tx1_vram;
extern data16_t *tx1_objram;
extern data16_t *tx1_rcram;
extern size_t tx1_vram_size;

READ16_HANDLER( tx1_crtc_r );
WRITE16_HANDLER( tx1_crtc_w );
WRITE16_HANDLER( tx1_bankcs_w );
WRITE16_HANDLER( tx1_slincs_w );
WRITE16_HANDLER( tx1_slock_w );
WRITE16_HANDLER( tx1_scolst_w );
WRITE16_HANDLER( tx1_flgcs_w );
WRITE16_HANDLER( buggyboy_gas_w );
WRITE16_HANDLER( buggyboy_sky_w );
WRITE16_HANDLER( buggyboy_scolst_w );
PALETTE_INIT( tx1 );
PALETTE_INIT( buggyboy );
VIDEO_START( tx1 );
VIDEO_START( buggyboy );
VIDEO_START( buggybjr );
VIDEO_UPDATE( tx1 );
VIDEO_UPDATE( buggyboy );
VIDEO_UPDATE( buggybjr );
VIDEO_EOF( tx1 );
VIDEO_EOF( buggyboy );

/* sound (tx1_sndhrdw.c) */
READ16_HANDLER( tx1_dipswitches_r );
WRITE16_HANDLER( tx1_z80_busreq_w );
READ16_HANDLER( tx1_z80_shared_r );
WRITE16_HANDLER( tx1_z80_shared_w );
WRITE_HANDLER( tx1_z80_intreq_w );
READ_HANDLER( tx1_ts_r );
WRITE_HANDLER( tx1_ts_w );
READ_HANDLER( tx1_pit8253_r );
WRITE_HANDLER( tx1_pit8253_w );
WRITE_HANDLER( tx1_ppi_latch_w );
READ_HANDLER( tx1_ppi_porta_r );
READ_HANDLER( tx1_ppi_portb_r );
WRITE_HANDLER( tx1_coin_cnt_w );
WRITE_HANDLER( tx1_ay8910_a_w );
WRITE_HANDLER( tx1_ay8910_b_w );
READ_HANDLER( bb_analog_r );
READ_HANDLER( bbjr_analog_r );
WRITE_HANDLER( bb_coin_cnt_w );
WRITE_HANDLER( bb_ym1_a_w );
WRITE_HANDLER( bb_ym2_a_w );
WRITE_HANDLER( bb_ym2_b_w );
WRITE_HANDLER( bbjr_ym2_b_w );
INTERRUPT_GEN( tx1_z80_irq );
int tx1_sh_start(const struct MachineSound *msound);
int buggyboy_sh_start(const struct MachineSound *msound);
void tx1_sh_reset(void);
void buggyboy_sh_reset(void);
extern data8_t *tx1_z80_ram;

/* the input ports, numbered as in the driver */
#define TX1_PORT_DSW		0
#define TX1_PORT_STEERING	1
#define TX1_PORT_ACCEL		2
#define TX1_PORT_BRAKE		3
#define TX1_PORT_PPI_A		4	/* Buggy Boy: PPI port A / BB Jr: YM2149 IC19 port A */
#define TX1_PORT_PPI_C		5	/* TX-1: PPI port C; Buggy Boy: PPI port C / BB Jr: YM2149 IC19 port B */

#endif
