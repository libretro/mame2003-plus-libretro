/*************************************************************************

	Sega System C/C2 Driver

**************************************************************************/

#include "vidhrdw/segavdp.h"

/*----------- defined in drivers/segac2.c -----------*/

void segac2_vdp_irq_update(void);
DRIVER_INIT( segac2 );

/* the Mega Drive based boards, shared with drivers/megaplay.c */
extern data16_t *genesis_68k_ram;
extern unsigned char *genesis_z80_ram;
extern data16_t *genesis_io_ram;

MACHINE_DRIVER_EXTERN( genesis_base );
MACHINE_INIT( genesis );
INTERRUPT_GEN( genesis_vblank_interrupt );
INTERRUPT_GEN( megatech_irq );
void genesis_enable_iack(void);
READ16_HANDLER( genesis_68k_to_z80_r );

READ16_HANDLER( genesis_ctrl_r );
WRITE16_HANDLER( genesis_ctrl_w );
WRITE16_HANDLER( genesis_io_w );
WRITE16_HANDLER( genesis_68k_to_z80_w );
WRITE16_HANDLER( sn76489_w );
READ_HANDLER( genesis_z80_r );
WRITE_HANDLER( genesis_z80_w );
READ_HANDLER( genesis_z80_bank_r );
READ_HANDLER( megatech_bios_port_be_bf_r );
WRITE_HANDLER( megatech_bios_port_be_bf_w );

extern struct YM2612interface gen_ym3438_intf;
extern struct SN76496interface megatech_sn76489_intf;

/* the two Mega Drive pads */
#define GENESIS_PORTS \
	PORT_START \
 \
	PORT_START	/* Player 1 Controls - part 1 */ \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) \
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) \
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) \
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) \
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_BUTTON3 ) \
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNUSED ) \
 \
	PORT_START	/* Player 1 Controls - part 2 */ \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x04, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT(  0x08, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNUSED ) \
 \
	PORT_START	/* Player 2 Controls - part 1 */ \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 ) \
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 ) \
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 ) \
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2  ) \
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) \
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 ) \
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNUSED ) \
 \
	PORT_START	/* Player 2 Controls - part 2 */ \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x04, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT(  0x08, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) \
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNUSED ) \
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_UNUSED ) \


/*----------- defined in vidhrdw/segac2_vidhrdw.c -----------*/

VIDEO_START( puckpkmn );
VIDEO_START( megatech );
VIDEO_START( megaplay );

VIDEO_START( segac2 );

VIDEO_EOF( segac2 );

VIDEO_UPDATE( segac2 );
VIDEO_UPDATE( megaplay );
