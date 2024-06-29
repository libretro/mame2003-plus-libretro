/***************************************************************************

Various Data East 8 bit games:

	Cobra Command (World)       (c) 1988 Data East Corporation (6809)
	Cobra Command (Japan)       (c) 1988 Data East Corporation (6809)
	The Real Ghostbusters (2p)  (c) 1987 Data East USA (6809 + I8751)
	The Real Ghostbusters (3p)  (c) 1987 Data East USA (6809 + I8751)
	Meikyuu Hunter G            (c) 1987 Data East Corporation (6809 + I8751)
	Super Real Darwin           (c) 1987 Data East Corporation (6809 + I8751)
	Psycho-Nics Oscar           (c) 1988 Data East USA (2*6809 + I8751)
	Psycho-Nics Oscar (Japan)   (c) 1987 Data East Corporation (2*6809 + I8751)
	Gondomania                  (c) 1987 Data East USA (6809 + I8751)
	Makyou Senshi               (c) 1987 Data East Corporation (6809 + I8751)
	Last Mission (rev 6)        (c) 1986 Data East USA (2*6809 + I8751)
	Last Mission (rev 5)        (c) 1986 Data East USA (2*6809 + I8751)
	Shackled                    (c) 1986 Data East USA (2*6809 + I8751)
	Breywood                    (c) 1986 Data East Corporation (2*6809 + I8751)
	Captain Silver (Japan)      (c) 1987 Data East Corporation (2*6809 + I8751)
	Garyo Retsuden (Japan)      (c) 1987 Data East Corporation (6809 + I8751)

	All games use a 6502 for sound (some are encrypted), all games except Cobracom
	use an Intel 8751 for protection & coinage.  For these games the coinage dip
	switch is not currently supported, they are fixed at 1 coin 1 credit.

	Meikyuu Hunter G was formerly known as Mazehunter.

	Emulation by Bryan McPhail, mish@tendril.co.uk

To do:
	Support coinage options for all i8751 emulations.
	Super Real Darwin 'Double' sprites appearing from the top of the screen are clipped
	Strangely coloured butterfly on Garyo Retsuden water levels!

  Thanks to José Miguel Morales Farreras for Super Real Darwin information!

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m6502/m6502.h"

PALETTE_INIT( ghostb );
VIDEO_UPDATE( cobracom );
VIDEO_UPDATE( ghostb );
VIDEO_UPDATE( srdarwin );
VIDEO_UPDATE( gondo );
VIDEO_UPDATE( garyoret );
VIDEO_UPDATE( lastmiss );
VIDEO_UPDATE( shackled );
VIDEO_UPDATE( oscar );
VIDEO_START( cobracom );
VIDEO_START( oscar );
VIDEO_START( ghostb );
VIDEO_START( lastmiss );
VIDEO_START( shackled );
VIDEO_START( srdarwin );
VIDEO_START( gondo );
VIDEO_START( garyoret );

WRITE_HANDLER( dec8_bac06_0_w );
WRITE_HANDLER( dec8_bac06_1_w );
WRITE_HANDLER( dec8_pf0_data_w );
WRITE_HANDLER( dec8_pf1_data_w );
READ_HANDLER( dec8_pf0_data_r );
READ_HANDLER( dec8_pf1_data_r );
WRITE_HANDLER( srdarwin_videoram_w );
WRITE_HANDLER( dec8_scroll1_w );
WRITE_HANDLER( dec8_scroll2_w );
WRITE_HANDLER( srdarwin_control_w );
WRITE_HANDLER( gondo_scroll_w );
WRITE_HANDLER( shackled_control_w );
WRITE_HANDLER( lastmiss_control_w );
WRITE_HANDLER( lastmiss_scrollx_w );
WRITE_HANDLER( lastmiss_scrolly_w );
WRITE_HANDLER( dec8_bac06_0_w );
WRITE_HANDLER( dec8_bac06_1_w );
WRITE_HANDLER( dec8_videoram_w );


/******************************************************************************/

static unsigned char *dec8_shared_ram,*dec8_shared2_ram;
extern unsigned char *dec8_pf0_data,*dec8_pf1_data,*dec8_row;

static int nmi_enable,int_enable;
static int i8751_return, i8751_value;
static int msm5205next;

/******************************************************************************/

/* Only used by ghostb, gondo, garyoret, other games can control buffering */
static VIDEO_EOF( dec8 )
{
	buffer_spriteram_w(0,0);
}

static READ_HANDLER( i8751_h_r )
{
	return i8751_return>>8; /* MSB */
}

static READ_HANDLER( i8751_l_r )
{
	return i8751_return&0xff; /* LSB */
}

static WRITE_HANDLER( i8751_reset_w )
{
	i8751_return=0;
}

/******************************************************************************/

static READ_HANDLER( gondo_player_1_r )
{
	switch (offset) {
		case 0: /* Rotary low byte */
			return ~((1 << (readinputport(5) * 12 / 256))&0xff);
		case 1: /* Joystick = bottom 4 bits, rotary = top 4 */
			return ((~((1 << (readinputport(5) * 12 / 256))>>4))&0xf0) | (readinputport(0)&0xf);
	}
	return 0xff;
}

static READ_HANDLER( gondo_player_2_r )
{
	switch (offset) {
		case 0: /* Rotary low byte */
			return ~((1 << (readinputport(6) * 12 / 256))&0xff);
		case 1: /* Joystick = bottom 4 bits, rotary = top 4 */
			return ((~((1 << (readinputport(6) * 12 / 256))>>4))&0xf0) | (readinputport(1)&0xf);
	}
	return 0xff;
}

/******************************************************************************/

static WRITE_HANDLER( ghostb_i8751_w )
{
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	if (i8751_value==0x00aa) i8751_return=0x655;
	if (i8751_value==0x021a) i8751_return=0x6e5; /* Ghostbusters ID */
	if (i8751_value==0x021b) i8751_return=0x6e4; /* Meikyuu Hunter G ID */
}

static WRITE_HANDLER( srdarwin_i8751_w )
{
	static int coins,latch;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	if (i8751_value==0x0000) {i8751_return=0;coins=0;}
	if (i8751_value==0x3063) i8751_return=0x9c; /* Protection - Japanese version */
	if (i8751_value==0x306b) i8751_return=0x94; /* Protection - World version */
	if ((i8751_value&0xff00)==0x4000) i8751_return=i8751_value; /* Coinage settings */
 	if (i8751_value==0x5000) i8751_return=((coins / 10) << 4) | (coins % 10); /* Coin request */
 	if (i8751_value==0x6000) {i8751_value=-1; coins--; } /* Coin clear */
	/* Nb:  Command 0x4000 for setting coinage options is not supported */
 	if ((readinputport(4)&1)==1) latch=1;
 	if ((readinputport(4)&1)!=1 && latch) {coins++; latch=0;}

	/* This next value is the index to a series of tables,
	each table controls the end of level bad guy, wrong values crash the
	cpu right away via a bogus jump.

	Level number requested is in low byte

	Addresses on left hand side are from the protection vector table which is
	stored at location 0xf580 in rom dy_01.rom

ba5e (lda #00) = Level 0?
ba82 (lda #01) = Pyramid boss, Level 1?
baaa           = No boss appears, game hangs
bacc (lda #04) = Killer Bee boss, Level 4?
bae0 (lda #03) = Snake type boss, Level 3?
baf9           = Double grey thing boss...!
bb0a      	   = Single grey thing boss!
bb18 (lda #00) = Hailstorm from top of screen.
bb31 (lda #28) = Small hailstorm
bb47 (ldb #05) = Small hailstorm
bb5a (lda #08) = Weird square things..
bb63           = Square things again
(24)           = Another square things boss..
(26)           = Clock boss! (level 3)
(28)           = Big dragon boss, perhaps an end-of-game baddy
(30)           = 4 things appear at corners, seems to fit with attract mode (level 1)
(32)           = Grey things teleport onto screen..
(34)           = Grey thing appears in middle of screen
(36)           = As above
(38)           = Circle thing with two pincers
(40)           = Grey bird
(42)           = Crash (end of table)

	The table below is hopefully correct thanks to José Miguel Morales Farreras,
	but Boss #6 is uncomfirmed as correct.

*/
	if (i8751_value==0x8000) i8751_return=0xf580 +  0; /* Boss #1: Snake + Bees */
	if (i8751_value==0x8001) i8751_return=0xf580 + 30; /* Boss #2: 4 Corners */
	if (i8751_value==0x8002) i8751_return=0xf580 + 26; /* Boss #3: Clock */
	if (i8751_value==0x8003) i8751_return=0xf580 +  2; /* Boss #4: Pyramid */
	if (i8751_value==0x8004) i8751_return=0xf580 +  6; /* Boss #5: Snake + Head Combo */
	if (i8751_value==0x8005) i8751_return=0xf580 + 24; /* Boss #6: LED Panels */
	if (i8751_value==0x8006) i8751_return=0xf580 + 28; /* Boss #7: Dragon */
	if (i8751_value==0x8007) i8751_return=0xf580 + 32; /* Boss #8: Teleport */
	if (i8751_value==0x8008) i8751_return=0xf580 + 38; /* Boss #9: Octopus (Pincer) */
	if (i8751_value==0x8009) i8751_return=0xf580 + 40; /* Boss #10: Bird */
	if (i8751_value==0x800a) i8751_return=0xf580 + 42; /* End Game(bad address?) */
}

static WRITE_HANDLER( gondo_i8751_w )
{
	static int coin1,coin2,latch,snd;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		if (int_enable) cpu_set_irq_line (0, M6809_IRQ_LINE, HOLD_LINE); /* IRQ on *high* byte only */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(4)&3)==3) latch=1;
 	if ((readinputport(4)&1)!=1 && latch) {coin1++; snd=1; latch=0;}
 	if ((readinputport(4)&2)!=2 && latch) {coin2++; snd=1; latch=0;}

	/* Work out return values */
	if (i8751_value==0x0000) {i8751_return=0; coin1=coin2=snd=0;}
	if (i8751_value==0x038a)  i8751_return=0x375; /* Makyou Senshi ID */
	if (i8751_value==0x038b)  i8751_return=0x374; /* Gondomania ID */
	if ((i8751_value>>8)==0x04)  i8751_return=0x40f; /* Coinage settings (Not supported) */
	if ((i8751_value>>8)==0x05) {i8751_return=0x500 | ((coin1 / 10) << 4) | (coin1 % 10);  } /* Coin 1 */
	if ((i8751_value>>8)==0x06 && coin1 && !offset) {i8751_return=0x600; coin1--; } /* Coin 1 clear */
	if ((i8751_value>>8)==0x07) {i8751_return=0x700 | ((coin2 / 10) << 4) | (coin2 % 10);  } /* Coin 2 */
	if ((i8751_value>>8)==0x08 && coin2 && !offset) {i8751_return=0x800; coin2--; } /* Coin 2 clear */
	/* Commands 0x9xx do nothing */
	if ((i8751_value>>8)==0x0a) {i8751_return=0xa00 | snd; if (snd) snd=0; }
}

static WRITE_HANDLER( shackled_i8751_w )
{
	static int coin1,coin2,latch=0;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		cpu_set_irq_line (1, M6809_FIRQ_LINE, HOLD_LINE); /* Signal main cpu */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if (/*(readinputport(2)&3)==3*/!latch) {latch=1;coin1=coin2=0;}
 	if ((readinputport(2)&1)!=1 && latch) {coin1=1; latch=0;}
 	if ((readinputport(2)&2)!=2 && latch) {coin2=1; latch=0;}

	if (i8751_value==0x0050) i8751_return=0; /* Breywood ID */
	if (i8751_value==0x0051) i8751_return=0; /* Shackled ID */
	if (i8751_value==0x0102) i8751_return=0; /* ?? */
	if (i8751_value==0x0101) i8751_return=0; /* ?? */
	if (i8751_value==0x8101) i8751_return=((coin2 / 10) << 4) | (coin2 % 10) |
			((((coin1 / 10) << 4) | (coin1 % 10))<<8); /* Coins */
}

static WRITE_HANDLER( lastmiss_i8751_w )
{
	static int coin,latch=0,snd;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		cpu_set_irq_line (0, M6809_FIRQ_LINE, HOLD_LINE); /* Signal main cpu */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(2)&3)==3 && !latch) latch=1;
 	if ((readinputport(2)&3)!=3 && latch) {coin++; latch=0;snd=0x400;i8751_return=0x400;return;}
	if (i8751_value==0x007a) i8751_return=0x0185; /* Japan ID code */
	if (i8751_value==0x007b) i8751_return=0x0184; /* USA ID code */
	if (i8751_value==0x0001) {coin=snd=0;}/*???*/
	if (i8751_value==0x0000) {i8751_return=0x0184;}/*???*/
	if (i8751_value==0x0401) i8751_return=0x0184; /*???*/
	if ((i8751_value>>8)==0x01) i8751_return=0x0184; /* Coinage setup */
	if ((i8751_value>>8)==0x02) {i8751_return=snd | ((coin / 10) << 4) | (coin % 10); snd=0;} /* Coin return */
	if ((i8751_value>>8)==0x03) {i8751_return=0; coin--; } /* Coin clear */
}

static WRITE_HANDLER( csilver_i8751_w )
{
	static int coin,latch=0,snd;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		cpu_set_irq_line (0, M6809_FIRQ_LINE, HOLD_LINE); /* Signal main cpu */
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(2)&3)==3 && !latch) latch=1;
 	if ((readinputport(2)&3)!=3 && latch) {coin++; latch=0;snd=0x1200; i8751_return=0x1200;return;}

	if (i8751_value==0x054a) {i8751_return=~(0x4a); coin=0; snd=0;} /* Captain Silver ID */
	if ((i8751_value>>8)==0x01) i8751_return=0; /* Coinage - Not Supported */
	if ((i8751_value>>8)==0x02) {i8751_return=snd | coin; snd=0; } /* Coin Return */
	if (i8751_value==0x0003 && coin) {i8751_return=0; coin--;} /* Coin Clear */
}

static WRITE_HANDLER( garyoret_i8751_w )
{
	static int coin1,coin2,latch;
	i8751_return=0;

	switch (offset) {
	case 0: /* High byte */
		i8751_value=(i8751_value&0xff) | (data<<8);
		break;
	case 1: /* Low byte */
		i8751_value=(i8751_value&0xff00) | data;
		break;
	}

	/* Coins are controlled by the i8751 */
 	if ((readinputport(2)&3)==3) latch=1;
 	if ((readinputport(2)&1)!=1 && latch) {coin1++; latch=0;}
 	if ((readinputport(2)&2)!=2 && latch) {coin2++; latch=0;}

	/* Work out return values */
	if ((i8751_value>>8)==0x00) {i8751_return=0; coin1=coin2=0;}
	if ((i8751_value>>8)==0x01)  i8751_return=0x59a; /* ID */
	if ((i8751_value>>8)==0x04)  i8751_return=i8751_value; /* Coinage settings (Not supported) */
	if ((i8751_value>>8)==0x05) {i8751_return=0x00 | ((coin1 / 10) << 4) | (coin1 % 10);  } /* Coin 1 */
	if ((i8751_value>>8)==0x06 && coin1 && !offset) {i8751_return=0x600; coin1--; } /* Coin 1 clear */
}

/******************************************************************************/

static WRITE_HANDLER( dec8_bank_w )
{
 	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	bankaddress = 0x10000 + (data & 0x0f) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);
}

/* Used by Ghostbusters, Meikyuu Hunter G & Gondomania */
static WRITE_HANDLER( ghostb_bank_w )
{
 	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* Bit 0: Interrupt enable/disable (I think..)
	   Bit 1: NMI enable/disable
	   Bit 2: ??
	   Bit 3: Screen flip
	   Bits 4-7: Bank switch
	*/

	bankaddress = 0x10000 + (data >> 4) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);

	if (data&1) int_enable=1; else int_enable=0;
	if (data&2) nmi_enable=1; else nmi_enable=0;
	flip_screen_set(data & 0x08);
}

WRITE_HANDLER( csilver_control_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	/*
		Bit 0x0f - ROM bank switch.
		Bit 0x10 - Always set(?)
		Bit 0x20 - Unused.
		Bit 0x40 - Unused.
		Bit 0x80 - Hold subcpu reset line high if clear, else low?  (Not needed anyway)
	*/
	cpu_setbank(1,&RAM[0x10000 + (data & 0x0f) * 0x4000]);
}

static WRITE_HANDLER( dec8_sound_w )
{
 	soundlatch_w(0,data);
	cpu_set_irq_line(1,IRQ_LINE_NMI,PULSE_LINE);
}

static WRITE_HANDLER( oscar_sound_w )
{
 	soundlatch_w(0,data);
	cpu_set_irq_line(2,IRQ_LINE_NMI,PULSE_LINE);
}

static void csilver_adpcm_int(int data)
{
	static int toggle =0;

	toggle ^= 1;
	if (toggle)
		cpu_set_irq_line(2,M6502_IRQ_LINE,HOLD_LINE);

	MSM5205_data_w (0,msm5205next>>4);
	msm5205next<<=4;
}

static READ_HANDLER( csilver_adpcm_reset_r )
{
	MSM5205_reset_w(0,0);
	return 0;
}

static WRITE_HANDLER( csilver_adpcm_data_w )
{
	msm5205next = data;
}

static WRITE_HANDLER( csilver_sound_bank_w )
{
	unsigned char *RAM = memory_region(REGION_CPU3);

	if (data&8) { cpu_setbank(3,&RAM[0x14000]); }
	else { cpu_setbank(3,&RAM[0x10000]); }
}

/******************************************************************************/

static WRITE_HANDLER( oscar_int_w )
{
	/* Deal with interrupts, coins also generate NMI to CPU 0 */
	switch (offset) {
		case 0: /* IRQ2 */
			cpu_set_irq_line(1,M6809_IRQ_LINE,ASSERT_LINE);
			return;
		case 1: /* IRC 1 */
			cpu_set_irq_line(0,M6809_IRQ_LINE,CLEAR_LINE);
			return;
		case 2: /* IRQ 1 */
			cpu_set_irq_line(0,M6809_IRQ_LINE,ASSERT_LINE);
			return;
		case 3: /* IRC 2 */
			cpu_set_irq_line(1,M6809_IRQ_LINE,CLEAR_LINE);
			return;
	}
}

/* Used by Shackled, Last Mission, Captain Silver */
static WRITE_HANDLER( shackled_int_w )
{
#if 0
/* This is correct, but the cpus in Shackled need an interleave of about 5000!
	With lower interleave CPU 0 misses an interrupt at the start of the game
	(The last interrupt has not finished and been ack'd when the new one occurs */
	switch (offset) {
		case 0: /* CPU 2 - IRQ acknowledge */
			cpu_set_irq_line(1,M6809_IRQ_LINE,CLEAR_LINE);
            return;
        case 1: /* CPU 1 - IRQ acknowledge */
			cpu_set_irq_line(0,M6809_IRQ_LINE,CLEAR_LINE);
        	return;
        case 2: /* i8751 - FIRQ acknowledge */
            return;
        case 3: /* IRQ 1 */
			cpu_set_irq_line(0,M6809_IRQ_LINE,ASSERT_LINE);
			return;
        case 4: /* IRQ 2 */
            cpu_set_irq_line(1,M6809_IRQ_LINE,ASSERT_LINE);
            return;
	}
#endif

	switch (offset) {
		case 0: /* CPU 2 - IRQ acknowledge */
            return;
        case 1: /* CPU 1 - IRQ acknowledge */
        	return;
        case 2: /* i8751 - FIRQ acknowledge */
            return;
        case 3: /* IRQ 1 */
			cpu_set_irq_line (0, M6809_IRQ_LINE, HOLD_LINE);
			return;
        case 4: /* IRQ 2 */
            cpu_set_irq_line (1, M6809_IRQ_LINE, HOLD_LINE);
            return;
	}
}

/******************************************************************************/

static READ_HANDLER( dec8_share_r ) { return dec8_shared_ram[offset]; }
static READ_HANDLER( dec8_share2_r ) { return dec8_shared2_ram[offset]; }
static WRITE_HANDLER( dec8_share_w ) { dec8_shared_ram[offset]=data; }
static WRITE_HANDLER( dec8_share2_w ) { dec8_shared2_ram[offset]=data; }
static READ_HANDLER( shackled_sprite_r ) { return spriteram[offset]; }
static WRITE_HANDLER( shackled_sprite_w ) { spriteram[offset]=data; }
static WRITE_HANDLER( flip_screen_w ) {	flip_screen_set(data); }

/******************************************************************************/

static MEMORY_READ_START( cobra_readmem )
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0fff, dec8_pf0_data_r },
	{ 0x1000, 0x17ff, dec8_pf1_data_r },
	{ 0x1800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x31ff, paletteram_r },
	{ 0x3800, 0x3800, input_port_0_r }, /* Player 1 */
	{ 0x3801, 0x3801, input_port_1_r }, /* Player 2 */
	{ 0x3802, 0x3802, input_port_3_r }, /* Dip 1 */
	{ 0x3803, 0x3803, input_port_4_r }, /* Dip 2 */
	{ 0x3a00, 0x3a00, input_port_2_r }, /* VBL & coins */
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( cobra_writemem )
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x1000, 0x17ff, dec8_pf1_data_w, &dec8_pf1_data },
	{ 0x1800, 0x1fff, MWA_RAM },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x31ff, paletteram_xxxxBBBBGGGGRRRR_swap_w, &paletteram },
	{ 0x3200, 0x37ff, MWA_RAM }, /* Unused */
	{ 0x3800, 0x381f, dec8_bac06_0_w },
	{ 0x3a00, 0x3a1f, dec8_bac06_1_w },
	{ 0x3c00, 0x3c00, dec8_bank_w },
	{ 0x3c02, 0x3c02, buffer_spriteram_w }, /* DMA */
	{ 0x3e00, 0x3e00, dec8_sound_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( ghostb_readmem )
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x1800, 0x1fff, videoram_r },
	{ 0x2000, 0x27ff, dec8_pf0_data_r },
	{ 0x2800, 0x2dff, MRA_RAM },
	{ 0x3000, 0x37ff, MRA_RAM },
	{ 0x3800, 0x3800, input_port_0_r }, /* Player 1 */
	{ 0x3801, 0x3801, input_port_1_r }, /* Player 2 */
	{ 0x3802, 0x3802, input_port_2_r }, /* Player 3 */
	{ 0x3803, 0x3803, input_port_3_r }, /* Start buttons + VBL */
	{ 0x3820, 0x3820, input_port_5_r }, /* Dip */
	{ 0x3840, 0x3840, i8751_h_r },
	{ 0x3860, 0x3860, i8751_l_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( ghostb_writemem )
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x27ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x2800, 0x2bff, MWA_RAM }, /* Scratch ram for rowscroll? */
	{ 0x2c00, 0x2dff, MWA_RAM, &dec8_row },
	{ 0x2e00, 0x2fff, MWA_RAM }, /* Unused */
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3800, 0x3800, dec8_sound_w },
	{ 0x3820, 0x383f, dec8_bac06_0_w },
	{ 0x3840, 0x3840, ghostb_bank_w },
	{ 0x3860, 0x3861, ghostb_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( srdarwin_readmem )
	{ 0x0000, 0x13ff, MRA_RAM },
	{ 0x1400, 0x17ff, dec8_pf0_data_r },
	{ 0x2000, 0x2000, i8751_h_r },
	{ 0x2001, 0x2001, i8751_l_r },
	{ 0x3800, 0x3800, input_port_2_r }, /* Dip 1 */
	{ 0x3801, 0x3801, input_port_0_r }, /* Player 1 */
	{ 0x3802, 0x3802, input_port_1_r }, /* Player 2 (cocktail) + VBL */
	{ 0x3803, 0x3803, input_port_3_r }, /* Dip 2 */
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( srdarwin_writemem )
	{ 0x0000, 0x05ff, MWA_RAM },
	{ 0x0600, 0x07ff, MWA_RAM, &spriteram },
	{ 0x0800, 0x0fff, srdarwin_videoram_w, &videoram, &spriteram_size },
	{ 0x1000, 0x13ff, MWA_RAM },
	{ 0x1400, 0x17ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x1800, 0x1801, srdarwin_i8751_w },
	{ 0x1802, 0x1802, i8751_reset_w },		/* Maybe.. */
	{ 0x1803, 0x1803, MWA_NOP },            /* NMI ack */
	{ 0x1804, 0x1804, buffer_spriteram_w }, /* DMA */
	{ 0x1805, 0x1806, srdarwin_control_w }, /* Scroll & Bank */
	{ 0x2000, 0x2000, dec8_sound_w },       /* Sound */
	{ 0x2001, 0x2001, flip_screen_w },  /* Flipscreen */
	{ 0x2800, 0x288f, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x3000, 0x308f, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( gondo_readmem )
	{ 0x0000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1fff, videoram_r },
	{ 0x2000, 0x27ff, dec8_pf0_data_r },
	{ 0x2800, 0x2bff, paletteram_r },
	{ 0x2c00, 0x2fff, paletteram_2_r },
	{ 0x3000, 0x37ff, MRA_RAM },          /* Sprites */
	{ 0x3800, 0x3800, input_port_7_r },   /* Dip 1 */
	{ 0x3801, 0x3801, input_port_8_r },   /* Dip 2 */
	{ 0x380a, 0x380b, gondo_player_1_r }, /* Player 1 rotary */
	{ 0x380c, 0x380d, gondo_player_2_r }, /* Player 2 rotary */
	{ 0x380e, 0x380e, input_port_3_r },   /* VBL */
	{ 0x380f, 0x380f, input_port_2_r },   /* Fire buttons */
	{ 0x3838, 0x3838, i8751_h_r },
	{ 0x3839, 0x3839, i8751_l_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( gondo_writemem )
	{ 0x0000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x27ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x2800, 0x2bff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x2c00, 0x2fff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3810, 0x3810, dec8_sound_w },
	{ 0x3818, 0x382f, gondo_scroll_w },
	{ 0x3830, 0x3830, ghostb_bank_w }, /* Bank + NMI enable */
	{ 0x383a, 0x383b, gondo_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( oscar_readmem )
	{ 0x0000, 0x0eff, dec8_share_r },
	{ 0x0f00, 0x0fff, MRA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, dec8_pf0_data_r },
	{ 0x3000, 0x37ff, MRA_RAM }, /* Sprites */
	{ 0x3800, 0x3bff, paletteram_r },
	{ 0x3c00, 0x3c00, input_port_0_r },
	{ 0x3c01, 0x3c01, input_port_1_r },
	{ 0x3c02, 0x3c02, input_port_2_r }, /* VBL & coins */
	{ 0x3c03, 0x3c03, input_port_3_r }, /* Dip 1 */
	{ 0x3c04, 0x3c04, input_port_4_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( oscar_writemem )
	{ 0x0000, 0x0eff, dec8_share_w, &dec8_shared_ram },
	{ 0x0f00, 0x0fff, MWA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3800, 0x3bff, paletteram_xxxxBBBBGGGGRRRR_swap_w, &paletteram },
	{ 0x3c00, 0x3c1f, dec8_bac06_0_w },
	{ 0x3c80, 0x3c80, buffer_spriteram_w },	/* DMA */
	{ 0x3d00, 0x3d00, dec8_bank_w },   		/* BNKS */
	{ 0x3d80, 0x3d80, oscar_sound_w }, 		/* SOUN */
	{ 0x3e00, 0x3e00, MWA_NOP },       		/* COINCL */
	{ 0x3e80, 0x3e83, oscar_int_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( oscar_sub_readmem )
	{ 0x0000, 0x0eff, dec8_share_r },
	{ 0x0f00, 0x0fff, MRA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_r },
	{ 0x4000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( oscar_sub_writemem )
	{ 0x0000, 0x0eff, dec8_share_w },
	{ 0x0f00, 0x0fff, MWA_RAM },
	{ 0x1000, 0x1fff, dec8_share2_w },
	{ 0x3e80, 0x3e83, oscar_int_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( lastmiss_readmem )
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r }, /* Dip 1 */
	{ 0x1804, 0x1804, input_port_4_r }, /* Dip 2 */
	{ 0x1806, 0x1806, i8751_h_r },
	{ 0x1807, 0x1807, i8751_l_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( lastmiss_writemem )
	{ 0x0000, 0x0fff, dec8_share_w, &dec8_shared_ram },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1809, 0x1809, lastmiss_scrollx_w }, /* Scroll LSB */
	{ 0x180b, 0x180b, lastmiss_scrolly_w }, /* Scroll LSB */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, lastmiss_control_w }, /* Bank switch + Scroll MSB */
	{ 0x180e, 0x180f, lastmiss_i8751_w },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x37ff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x3800, 0x3fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( lastmiss_sub_readmem )
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r }, /* Dip 1 */
	{ 0x1804, 0x1804, input_port_4_r }, /* Dip 2 */
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( lastmiss_sub_writemem )
	{ 0x0000, 0x0fff, dec8_share_w },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x2000, 0x27ff, dec8_videoram_w },
	{ 0x2800, 0x2fff, shackled_sprite_w },
	{ 0x3000, 0x37ff, dec8_share2_w },
	{ 0x3800, 0x3fff, dec8_pf0_data_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( shackled_readmem )
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r },
	{ 0x1804, 0x1804, input_port_4_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, shackled_sprite_r },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( shackled_writemem )
	{ 0x0000, 0x0fff, dec8_share_w, &dec8_shared_ram },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1809, 0x1809, lastmiss_scrollx_w }, /* Scroll LSB */
	{ 0x180b, 0x180b, lastmiss_scrolly_w }, /* Scroll LSB */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, shackled_control_w }, /* Bank switch + Scroll MSB */
	{ 0x2000, 0x27ff, dec8_videoram_w },
	{ 0x2800, 0x2fff, shackled_sprite_w },
	{ 0x3000, 0x37ff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x3800, 0x3fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( shackled_sub_readmem )
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1801, 0x1801, input_port_1_r },
	{ 0x1802, 0x1802, input_port_2_r },
	{ 0x1803, 0x1803, input_port_3_r },
	{ 0x1804, 0x1804, input_port_4_r },
	{ 0x1806, 0x1806, i8751_h_r },
	{ 0x1807, 0x1807, i8751_l_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( shackled_sub_writemem )
	{ 0x0000, 0x0fff, dec8_share_w },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1809, 0x1809, lastmiss_scrollx_w }, /* Scroll LSB */
	{ 0x180b, 0x180b, lastmiss_scrolly_w }, /* Scroll LSB */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, shackled_control_w }, /* Bank switch + Scroll MSB */
	{ 0x180e, 0x180f, shackled_i8751_w },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x37ff, dec8_share2_w },
	{ 0x3800, 0x3fff, dec8_pf0_data_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( csilver_readmem )
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1800, 0x1800, input_port_1_r },
	{ 0x1801, 0x1801, input_port_0_r },
	{ 0x1803, 0x1803, input_port_2_r },
	{ 0x1804, 0x1804, input_port_4_r }, /* Dip 2 */
	{ 0x1805, 0x1805, input_port_3_r }, /* Dip 1 */
	{ 0x1c00, 0x1c00, i8751_h_r },
	{ 0x1e00, 0x1e00, i8751_l_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, shackled_sprite_r },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( csilver_writemem )
	{ 0x0000, 0x0fff, dec8_share_w, &dec8_shared_ram },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x1807, 0x1807, flip_screen_w },
	{ 0x1808, 0x180b, dec8_scroll2_w },
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x180d, 0x180d, csilver_control_w },
	{ 0x180e, 0x180f, csilver_i8751_w },
	{ 0x2000, 0x27ff, dec8_videoram_w },
	{ 0x2800, 0x2fff, shackled_sprite_w },
	{ 0x3000, 0x37ff, dec8_share2_w, &dec8_shared2_ram },
	{ 0x3800, 0x3fff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( csilver_sub_readmem )
	{ 0x0000, 0x0fff, dec8_share_r },
	{ 0x1000, 0x13ff, paletteram_r },
	{ 0x1400, 0x17ff, paletteram_2_r },
	{ 0x1803, 0x1803, input_port_2_r },
	{ 0x1804, 0x1804, input_port_4_r },
	{ 0x1805, 0x1805, input_port_3_r },
	{ 0x2000, 0x27ff, videoram_r },
	{ 0x2800, 0x2fff, MRA_RAM },
	{ 0x3000, 0x37ff, dec8_share2_r },
	{ 0x3800, 0x3fff, dec8_pf0_data_r },
	{ 0x4000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( csilver_sub_writemem )
	{ 0x0000, 0x0fff, dec8_share_w },
	{ 0x1000, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split1_w },
	{ 0x1400, 0x17ff, paletteram_xxxxBBBBGGGGRRRR_split2_w },
	{ 0x1800, 0x1804, shackled_int_w },
	{ 0x1805, 0x1805, buffer_spriteram_w }, /* DMA */
	{ 0x180c, 0x180c, oscar_sound_w },
	{ 0x2000, 0x27ff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2800, 0x2fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x37ff, dec8_share2_w },
	{ 0x3800, 0x3fff, dec8_pf0_data_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( garyoret_readmem )
	{ 0x0000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1fff, videoram_r },
	{ 0x2000, 0x27ff, dec8_pf0_data_r },
	{ 0x2800, 0x2bff, paletteram_r },
	{ 0x2c00, 0x2fff, paletteram_2_r },
	{ 0x3000, 0x37ff, MRA_RAM },          /* Sprites */
	{ 0x3800, 0x3800, input_port_3_r },   /* Dip 1 */
	{ 0x3801, 0x3801, input_port_4_r },   /* Dip 2 */
	{ 0x3808, 0x3808, MRA_NOP },          /* ? */
	{ 0x380a, 0x380a, input_port_1_r },   /* Player 2 + VBL */
	{ 0x380b, 0x380b, input_port_0_r },   /* Player 1 */
	{ 0x383a, 0x383a, i8751_h_r },
	{ 0x383b, 0x383b, i8751_l_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( garyoret_writemem )
	{ 0x0000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, dec8_videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x27ff, dec8_pf0_data_w, &dec8_pf0_data },
	{ 0x2800, 0x2bff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x2c00, 0x2fff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3810, 0x3810, dec8_sound_w },
	{ 0x3818, 0x382f, gondo_scroll_w },
	{ 0x3830, 0x3830, ghostb_bank_w }, /* Bank + NMI enable */
	{ 0x3838, 0x3839, garyoret_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

/******************************************************************************/

/* Used for Cobra Command, Maze Hunter, Super Real Darwin etc */
static MEMORY_READ_START( dec8_s_readmem )
	{ 0x0000, 0x05ff, MRA_RAM},
	{ 0x6000, 0x6000, soundlatch_r },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( dec8_s_writemem )
	{ 0x0000, 0x05ff, MWA_RAM},
	{ 0x2000, 0x2000, YM2203_control_port_0_w }, /* OPN */
	{ 0x2001, 0x2001, YM2203_write_port_0_w },
	{ 0x4000, 0x4000, YM3812_control_port_0_w }, /* OPL */
	{ 0x4001, 0x4001, YM3812_write_port_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

/* Used by Gondomania, Psycho-Nics Oscar & Garyo Retsuden */
static MEMORY_WRITE_START( oscar_s_writemem )
	{ 0x0000, 0x05ff, MWA_RAM},
	{ 0x2000, 0x2000, YM2203_control_port_0_w }, /* OPN */
	{ 0x2001, 0x2001, YM2203_write_port_0_w },
	{ 0x4000, 0x4000, YM3526_control_port_0_w }, /* OPL */
	{ 0x4001, 0x4001, YM3526_write_port_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

/* Used by Last Mission, Shackled & Breywood */
static MEMORY_READ_START( ym3526_s_readmem )
	{ 0x0000, 0x05ff, MRA_RAM},
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( ym3526_s_writemem )
	{ 0x0000, 0x05ff, MWA_RAM},
	{ 0x0800, 0x0800, YM2203_control_port_0_w }, /* OPN */
	{ 0x0801, 0x0801, YM2203_write_port_0_w },
	{ 0x1000, 0x1000, YM3526_control_port_0_w }, /* OPL? */
	{ 0x1001, 0x1001, YM3526_write_port_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

/* Captain Silver - same sound system as Pocket Gal */
static MEMORY_READ_START( csilver_s_readmem )
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x3400, 0x3400, csilver_adpcm_reset_r },	/* ? not sure */
	{ 0x4000, 0x7fff, MRA_BANK3 },
	{ 0x8000, 0xffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( csilver_s_writemem )
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0800, YM2203_control_port_0_w },
	{ 0x0801, 0x0801, YM2203_write_port_0_w },
	{ 0x1000, 0x1000, YM3526_control_port_0_w },
	{ 0x1001, 0x1001, YM3526_write_port_0_w },
	{ 0x1800, 0x1800, csilver_adpcm_data_w },	/* ADPCM data for the MSM5205 chip */
	{ 0x2000, 0x2000, csilver_sound_bank_w },
	{ 0x4000, 0xffff, MWA_ROM },
MEMORY_END

/******************************************************************************/

#define PLAYER1_JOYSTICK /* Player 1 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )

#define PLAYER2_JOYSTICK /* Player 2 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )

INPUT_PORTS_START( cobracom )
	PORT_START /* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "50k, 150k" )
	PORT_DIPSETTING(    0x00, "100k, 200k" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ghostb )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 3 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dummy input for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* Dip switch */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, "Scene Time" )
	PORT_DIPSETTING(    0x00, "4.00" )
	PORT_DIPSETTING(    0x10, "4.30" )
	PORT_DIPSETTING(    0x30, "5.00" )
	PORT_DIPSETTING(    0x20, "6.00" )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Beam Energy Pickup" ) /* Ghostb only */
	PORT_DIPSETTING(    0x00, "Up 1.5%" )
	PORT_DIPSETTING(    0x80, "Normal" )
INPUT_PORTS_END

INPUT_PORTS_START( meikyuh )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Player 3 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* Dummy input for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* Dip switch */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( srdarwin )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "28", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Continues" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 ) /* Fake */
INPUT_PORTS_END

INPUT_PORTS_START( gondo )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	/* Top 4 bits are rotary controller */

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	/* Top 4 bits are rotary controller */

 	PORT_START	/* Player 1 & 2 fire buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* Fake port for the i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* player 1 12-way rotary control */
	PORT_ANALOGX( 0xff, 0x0a, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, KEYCODE_Z, KEYCODE_X, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START	/* player 2 12-way rotary control */
	PORT_ANALOGX( 0xff, 0x0a, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 10, 0, 0, KEYCODE_N, KEYCODE_M, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x10, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( oscar )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x20, 0x20, "Demo Freeze Mode" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "Every 40000" )
	PORT_DIPSETTING(    0x20, "Every 60000" )
	PORT_DIPSETTING(    0x10, "Every 90000" )
	PORT_DIPSETTING(    0x00, "50000 only" )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( lastmisn )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x80,    0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "30k, 70k then every 70k" )
	PORT_DIPSETTING(    0x04, "40k, 90k then every 90k" )
	PORT_DIPSETTING(    0x02, "40k and 80k" )
	PORT_DIPSETTING(    0x00, "50k only" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) ) /* Unused according to the manual */
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( lastmsnj )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_BITX( 0x40,    0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x80,    0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x06, "30k, 50k then every 50k" )
	PORT_DIPSETTING(    0x04, "30k, 70k then every 70k" )
	PORT_DIPSETTING(    0x02, "50k, 100k then every 100k" )
	PORT_DIPSETTING(    0x00, "50k only" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) ) /* Unused according to the manual */
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( shackled )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* Dip switch bank 1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) ) /* All marked as unused in the manual */
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )	/* game doesn't boot when this is On */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* Dip switch bank 2 */
	PORT_DIPNAME( 0x0f, 0x0f, "Power" )
	PORT_DIPSETTING( 0x07, "200" )
	PORT_DIPSETTING( 0x0b, "300" )
	PORT_DIPSETTING( 0x03, "400" )
	PORT_DIPSETTING( 0x0d, "500" )
	PORT_DIPSETTING( 0x05, "600" )
	PORT_DIPSETTING( 0x09, "700" )
	PORT_DIPSETTING( 0x01, "800" )
	PORT_DIPSETTING( 0x0e, "900" )
	PORT_DIPSETTING( 0x0f, "1000" )
	PORT_DIPSETTING( 0x06, "2000" )
	PORT_DIPSETTING( 0x0a, "3000" )
	PORT_DIPSETTING( 0x02, "4000" )
	PORT_DIPSETTING( 0x0c, "5000" )
	PORT_DIPSETTING( 0x04, "6000" )
	PORT_DIPSETTING( 0x08, "7000" )
	PORT_DIPSETTING( 0x00, "8000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x10, "Very Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( csilver )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( garyoret )
	PORT_START	/* Player 1 controls */
	PLAYER1_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

 	PORT_START	/* Player 2 controls */
	PLAYER2_JOYSTICK
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* Fake port for i8751 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout_32k =
{
	8,8,
	1024,
	2,
	{ 0x4000*8,0x0000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout chars_3bpp =
{
	8,8,
	1024,
	3,
	{ 0x6000*8,0x4000*8,0x2000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

/* SRDarwin characters - very unusual layout for Data East */
static struct GfxLayout charlayout_16k =
{
	8,8,	/* 8*8 characters */
	1024,
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0x2000*8+0, 0x2000*8+1, 0x2000*8+2, 0x2000*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout oscar_charlayout =
{
	8,8,
	1024,
	3,
	{ 0x3000*8,0x2000*8,0x1000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

/* Darwin sprites - only 3bpp */
static struct GfxLayout sr_sprites =
{
	16,16,
	2048,
	3,
 	{ 0x10000*8,0x20000*8,0x00000*8 },
	{ 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8),
		0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 },
	16*16
};

static struct GfxLayout srdarwin_tiles =
{
	16,16,
	256,
	4,
	{ 0x8000*8, 0x8000*8+4, 0, 4 },
	{ 0, 1, 2, 3, 1024*8*8+0, 1024*8*8+1, 1024*8*8+2, 1024*8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+1024*8*8+0, 16*8+1024*8*8+1, 16*8+1024*8*8+2, 16*8+1024*8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every tile takes 32 consecutive bytes */
};

static struct GfxLayout tiles =
{
	16,16,
	4096,
	4,
 	{ 0x60000*8,0x40000*8,0x20000*8,0x00000*8 },
	{ 16*8, 1+(16*8), 2+(16*8), 3+(16*8), 4+(16*8), 5+(16*8), 6+(16*8), 7+(16*8),
		0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	16*16
};

/* X flipped on Ghostbusters tiles */
static struct GfxLayout tiles_r =
{
	16,16,
	2048,
	4,
 	{ 0x20000*8,0x00000*8,0x30000*8,0x10000*8 },
	{ 7,6,5,4,3,2,1,0,
		7+(16*8), 6+(16*8), 5+(16*8), 4+(16*8), 3+(16*8), 2+(16*8), 1+(16*8), 0+(16*8) },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 ,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8},
	16*16
};

static struct GfxDecodeInfo cobracom_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_32k, 0, 8 },
	{ REGION_GFX2, 0, &tiles,         64, 4 },
	{ REGION_GFX4, 0, &tiles,        128, 4 },
	{ REGION_GFX3, 0, &tiles,        192, 4 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ghostb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars_3bpp,	0,  4 },
	{ REGION_GFX2, 0, &tiles,     256, 16 },
	{ REGION_GFX3, 0, &tiles_r,   512, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo srdarwin_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout_16k,128, 4 }, /* Only 1 used so far :/ */
	{ REGION_GFX2, 0x00000, &sr_sprites,     64, 8 },
	{ REGION_GFX3, 0x00000, &srdarwin_tiles,  0, 8 },
  	{ REGION_GFX3, 0x10000, &srdarwin_tiles,  0, 8 },
    { REGION_GFX3, 0x20000, &srdarwin_tiles,  0, 8 },
    { REGION_GFX3, 0x30000, &srdarwin_tiles,  0, 8 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gondo_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars_3bpp,  0, 16 }, /* Chars */
	{ REGION_GFX2, 0, &tiles,     256, 32 }, /* Sprites */
	{ REGION_GFX3, 0, &tiles,     768, 16 }, /* Tiles */
 	{ -1 } /* end of array */
};

static struct GfxDecodeInfo oscar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &oscar_charlayout, 256,  8 }, /* Chars */
	{ REGION_GFX2, 0, &tiles,              0, 16 }, /* Sprites */
	{ REGION_GFX3, 0, &tiles,            384,  8 }, /* Tiles */
 	{ -1 } /* end of array */
};

static struct GfxDecodeInfo shackled_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars_3bpp,   0,  4 },
	{ REGION_GFX2, 0, &tiles,      256, 16 },
	{ REGION_GFX3, 0, &tiles,      768, 16 },
 	{ -1 } /* end of array */
};

/******************************************************************************/

static struct YM2203interface ym2203_interface =
{
	1,
	1500000,	/* Should be accurate for all games, derived from 12MHz crystal */
	{ YM2203_VOL(20,23) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

/* handler called by the 3812 emulator when the internal timers cause an IRQ */
static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,0,linestate); /* M6502_IRQ_LINE */
}

static void oscar_irqhandler(int linestate)
{
	cpu_set_irq_line(2,0,linestate); /* M6502_IRQ_LINE */
}

static struct YM3526interface ym3526_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 70 },
	{ irqhandler },
};

static struct YM3526interface oscar_ym3526_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 70 },
	{ oscar_irqhandler },
};

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 70 },
	{ irqhandler },
};

static struct MSM5205interface msm5205_interface =
{
	1,					/* 1 chip             */
	384000,				/* 384KHz             */
	{ csilver_adpcm_int },/* interrupt function */
	{ MSM5205_S48_4B },	/* 8KHz               */
	{ 88 }
};

/******************************************************************************/

static INTERRUPT_GEN( ghostb_interrupt )
{
	static int latch[4];
	int i8751_out=readinputport(4);

	/* Ghostbusters coins are controlled by the i8751 */
	if ((i8751_out & 0x8) == 0x8) latch[0]=1;
	if ((i8751_out & 0x4) == 0x4) latch[1]=1;
	if ((i8751_out & 0x2) == 0x2) latch[2]=1;
	if ((i8751_out & 0x1) == 0x1) latch[3]=1;

	if (((i8751_out & 0x8) != 0x8) && latch[0]) {latch[0]=0; cpu_set_irq_line(0,M6809_IRQ_LINE,HOLD_LINE); i8751_return=0x8001; } /* Player 1 coin */
	if (((i8751_out & 0x4) != 0x4) && latch[1]) {latch[1]=0; cpu_set_irq_line(0,M6809_IRQ_LINE,HOLD_LINE); i8751_return=0x4001; } /* Player 2 coin */
	if (((i8751_out & 0x2) != 0x2) && latch[2]) {latch[2]=0; cpu_set_irq_line(0,M6809_IRQ_LINE,HOLD_LINE); i8751_return=0x2001; } /* Player 3 coin */
	if (((i8751_out & 0x1) != 0x1) && latch[3]) {latch[3]=0; cpu_set_irq_line(0,M6809_IRQ_LINE,HOLD_LINE); i8751_return=0x1001; } /* Service */

	if (nmi_enable) cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE); /* VBL */
}

static INTERRUPT_GEN( gondo_interrupt )
{
	if (nmi_enable)
		cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE); /* VBL */
}

/* Coins generate NMI's */
static INTERRUPT_GEN( oscar_interrupt )
{
	static int latch=1;

	if ((readinputport(2) & 0x7) == 0x7) latch=1;
	if (latch && (readinputport(2) & 0x7) != 0x7) {
		latch=0;
    	cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
    }
}

/******************************************************************************/

static MACHINE_DRIVER_START( cobracom )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(cobra_readmem,cobra_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(dec8_s_readmem,dec8_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(529) /* 58Hz, 529ms Vblank duration */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(cobracom_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(256)

	MDRV_VIDEO_START(cobracom)
	MDRV_VIDEO_UPDATE(cobracom)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ghostb )

	/* basic machine hardware */
	MDRV_CPU_ADD(HD6309, 3000000)
	MDRV_CPU_MEMORY(ghostb_readmem,ghostb_writemem)
	MDRV_CPU_VBLANK_INT(ghostb_interrupt,1)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(dec8_s_readmem,dec8_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(2500) /* 58Hz, 529ms Vblank duration */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(ghostb_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_PALETTE_INIT(ghostb)
	MDRV_VIDEO_START(ghostb)
	MDRV_VIDEO_EOF(dec8)
	MDRV_VIDEO_UPDATE(ghostb)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( srdarwin )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809,2000000)  /* MC68A09EP */
	MDRV_CPU_MEMORY(srdarwin_readmem,srdarwin_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(dec8_s_readmem,dec8_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(529) /* 58Hz, 529ms Vblank duration */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(srdarwin_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(144)

	MDRV_VIDEO_START(srdarwin)
	MDRV_VIDEO_UPDATE(srdarwin)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( gondo )

	/* basic machine hardware */
	MDRV_CPU_ADD(HD6309,3000000) /* HD63C09EP */
	MDRV_CPU_MEMORY(gondo_readmem,gondo_writemem)
	MDRV_CPU_VBLANK_INT(gondo_interrupt,1)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(dec8_s_readmem,oscar_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(529) /* 58Hz, 529ms Vblank duration */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(gondo_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(gondo)
	MDRV_VIDEO_EOF(dec8)
	MDRV_VIDEO_UPDATE(gondo)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3526, ym3526_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( oscar )

	/* basic machine hardware */
	MDRV_CPU_ADD(HD6309, 2000000)
	MDRV_CPU_MEMORY(oscar_readmem,oscar_writemem)
	MDRV_CPU_VBLANK_INT(oscar_interrupt,1)

	MDRV_CPU_ADD(HD6309, 2000000)
	MDRV_CPU_MEMORY(oscar_sub_readmem,oscar_sub_writemem)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(dec8_s_readmem,oscar_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(2500) /* 58Hz, 529ms Vblank duration */
	MDRV_INTERLEAVE(40) /* 40 CPU slices per frame */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(oscar_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(512)

	MDRV_VIDEO_START(oscar)
	MDRV_VIDEO_UPDATE(oscar)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3526, oscar_ym3526_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( lastmiss )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(lastmiss_readmem,lastmiss_writemem)

	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(lastmiss_sub_readmem,lastmiss_sub_writemem)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(ym3526_s_readmem,ym3526_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(2500) /* 58Hz, 529ms Vblank duration */
	MDRV_INTERLEAVE(200)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(shackled_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(lastmiss)
	MDRV_VIDEO_UPDATE(lastmiss)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3526, oscar_ym3526_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( shackled )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(shackled_readmem,shackled_writemem)

	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(shackled_sub_readmem,shackled_sub_writemem)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(ym3526_s_readmem,ym3526_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(2500) /* 58Hz, 529ms Vblank duration */
	MDRV_INTERLEAVE(80)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(shackled_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(shackled)
	MDRV_VIDEO_UPDATE(shackled)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3526, oscar_ym3526_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( csilver )

	/* basic machine hardware */
	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(csilver_readmem,csilver_writemem)

	MDRV_CPU_ADD(M6809, 2000000)
	MDRV_CPU_MEMORY(csilver_sub_readmem,csilver_sub_writemem)
	MDRV_CPU_VBLANK_INT(nmi_line_pulse,1)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(csilver_s_readmem,csilver_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(529) /* 58Hz, 529ms Vblank duration */
	MDRV_INTERLEAVE(100)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(shackled_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(lastmiss)
	MDRV_VIDEO_UPDATE(lastmiss)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3526, oscar_ym3526_interface)
	MDRV_SOUND_ADD(MSM5205, msm5205_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( garyoret )

	/* basic machine hardware */
	MDRV_CPU_ADD(HD6309,3000000) /* HD63C09EP */
	MDRV_CPU_MEMORY(garyoret_readmem,garyoret_writemem)
	MDRV_CPU_VBLANK_INT(gondo_interrupt,1)

	MDRV_CPU_ADD(M6502, 1500000)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(dec8_s_readmem,oscar_s_writemem)
								/* NMIs are caused by the main CPU */
	MDRV_FRAMES_PER_SECOND(58)
	MDRV_VBLANK_DURATION(529) /* 58Hz, 529ms Vblank duration */

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 1*8, 31*8-1)
	MDRV_GFXDECODE(gondo_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(1024)

	MDRV_VIDEO_START(garyoret)
	MDRV_VIDEO_EOF(dec8)
	MDRV_VIDEO_UPDATE(garyoret)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(YM3526, ym3526_interface)
MACHINE_DRIVER_END

/******************************************************************************/

ROM_START( cobracom )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "el11-5.bin",  0x08000, 0x08000, CRC(af0a8b05) SHA1(096e4e7f2785a20bfaec14277413ce4e20e90214) )
	ROM_LOAD( "el12-4.bin",  0x10000, 0x10000, CRC(7a44ef38) SHA1(d7dc277dce08f9d073290e100be4a7ca2e2b82cb) )
	ROM_LOAD( "el13.bin",    0x20000, 0x10000, CRC(04505acb) SHA1(2220efb277884588859375dab9960f04f07273a7) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "el10-4.bin",  0x8000,  0x8000,  CRC(edfad118) SHA1(10de8805472346fead62460a3fdc09ae26a4e0d5) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "el14.bin",    0x00000, 0x08000, CRC(47246177) SHA1(51b025740dc03b04009ac97d8d110ab521894386) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "el00-4.bin",  0x00000, 0x10000, CRC(122da2a8) SHA1(ce72f16abf7e5449c7d044d4b827e8735c3be0ff) )
	ROM_LOAD( "el01-4.bin",  0x20000, 0x10000, CRC(27bf705b) SHA1(196c35aaf3816d3eef4c2af6d146a90a48365d33) )
	ROM_LOAD( "el02-4.bin",  0x40000, 0x10000, CRC(c86fede6) SHA1(97584fa19591651fcfb39d1b2b6306165e93554c) )
	ROM_LOAD( "el03-4.bin",  0x60000, 0x10000, CRC(1d8a855b) SHA1(429261c200dddc62a330be8aea150b2037133188) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles 1 */
	ROM_LOAD( "el05.bin",    0x00000, 0x10000, CRC(1c4f6033) SHA1(4a7dece911166d1ff5f41df6ec5140596206d8d4) )
	ROM_LOAD( "el06.bin",    0x20000, 0x10000, CRC(d24ba794) SHA1(b34b7bbaab4ebdd81c87d363f087cc92e27e8d1c) )
	ROM_LOAD( "el04.bin",    0x40000, 0x10000, CRC(d80a49ce) SHA1(1a92413b5ab53f80e44a954433e69ec5fe2c0aa6) )
	ROM_LOAD( "el07.bin",    0x60000, 0x10000, CRC(6d771fc3) SHA1(f29979f3aa07bdb544fb0c1d773c5558b4533390) )

	ROM_REGION( 0x80000, REGION_GFX4, ROMREGION_DISPOSE )	/* tiles 2 */
	ROM_LOAD( "el08.bin",    0x00000, 0x08000, CRC(cb0dcf4c) SHA1(e14853f83ee9ba5cbf2eb1e085fee4e65af3cc25) )
	ROM_CONTINUE(            0x40000, 0x08000 )
	ROM_LOAD( "el09.bin",    0x20000, 0x08000, CRC(1fae5be7) SHA1(be6e090b0b82648b385d9b6d11775f3ff40f0af3) )
	ROM_CONTINUE(            0x60000, 0x08000 )
ROM_END

ROM_START( cobracmj )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "eh-11.rom",    0x08000, 0x08000, CRC(868637e1) SHA1(8b1e3e045e341bb94b1f6c7d89198b22e6c19de7) )
	ROM_LOAD( "eh-12.rom",    0x10000, 0x10000, CRC(7c878a83) SHA1(9b2a3083c6dae69626fdab16d97517d30eaa1859) )
	ROM_LOAD( "el13.bin",     0x20000, 0x10000, CRC(04505acb) SHA1(2220efb277884588859375dab9960f04f07273a7) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "eh-10.rom",    0x8000,  0x8000,  CRC(62ca5e89) SHA1(b04acaccc58846e0d277868a873a440b7f8071b0) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "el14.bin",    0x00000, 0x08000, CRC(47246177) SHA1(51b025740dc03b04009ac97d8d110ab521894386) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "eh-00.rom",    0x00000, 0x10000, CRC(d96b6797) SHA1(01c4a9f2bebb13cba14636690cd5356db73f045e) )
	ROM_LOAD( "eh-01.rom",    0x20000, 0x10000, CRC(3fef9c02) SHA1(e4b731faf6a2f4e5fed8ba9bd07e0f203981ffec) )
	ROM_LOAD( "eh-02.rom",    0x40000, 0x10000, CRC(bfae6c34) SHA1(9503a120e11e9466cd9a2931fd44a631d72ca5f0) )
	ROM_LOAD( "eh-03.rom",    0x60000, 0x10000, CRC(d56790f8) SHA1(1cc7cb9f7102158de14a737e9317a54f01790ba8) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles 1 */
	ROM_LOAD( "el05.bin",    0x00000, 0x10000, CRC(1c4f6033) SHA1(4a7dece911166d1ff5f41df6ec5140596206d8d4) )
	ROM_LOAD( "el06.bin",    0x20000, 0x10000, CRC(d24ba794) SHA1(b34b7bbaab4ebdd81c87d363f087cc92e27e8d1c) )
	ROM_LOAD( "el04.bin",    0x40000, 0x10000, CRC(d80a49ce) SHA1(1a92413b5ab53f80e44a954433e69ec5fe2c0aa6) )
	ROM_LOAD( "el07.bin",    0x60000, 0x10000, CRC(6d771fc3) SHA1(f29979f3aa07bdb544fb0c1d773c5558b4533390) )

	ROM_REGION( 0x80000, REGION_GFX4, ROMREGION_DISPOSE )	/* tiles 2 */
	ROM_LOAD( "el08.bin",    0x00000, 0x08000, CRC(cb0dcf4c) SHA1(e14853f83ee9ba5cbf2eb1e085fee4e65af3cc25) )
	ROM_CONTINUE(            0x40000, 0x08000 )
	ROM_LOAD( "el09.bin",    0x20000, 0x08000, CRC(1fae5be7) SHA1(be6e090b0b82648b385d9b6d11775f3ff40f0af3) )
	ROM_CONTINUE(            0x60000, 0x08000 )
ROM_END

ROM_START( ghostb )
	ROM_REGION( 0x50000, REGION_CPU1, 0 )
	ROM_LOAD( "dz-01.rom", 0x08000, 0x08000, CRC(7c5bb4b1) SHA1(75865102c9bfbf9bd341b8ea54f49904c727f474) )
	ROM_LOAD( "dz-02.rom", 0x10000, 0x10000, CRC(8e117541) SHA1(7dfa6eabb29f39a615f3e5123bddcc7197ab82d0) )
	ROM_LOAD( "dz-03.rom", 0x20000, 0x10000, CRC(5606a8f4) SHA1(e46e887f13f648fe2162cb853b3c20fa60e3d215) )
	ROM_LOAD( "dz-04.rom", 0x30000, 0x10000, CRC(d09bad99) SHA1(bde8e4316cedf1d292f0aed8627b0dc6794c6e07) )
	ROM_LOAD( "dz-05.rom", 0x40000, 0x10000, CRC(0315f691) SHA1(3bfad18b9f230e64c608a22af20c3b00dca3e6da) )

	ROM_REGION( 2*0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dz-06.rom", 0x8000, 0x8000, CRC(798f56df) SHA1(aee33cd0c102015114e17f6cb98945e7cc806f55) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dz-00.rom", 0x00000, 0x08000, CRC(992b4f31) SHA1(a9f255286193ccc261a9b6983aabf3c76ebe5ce5) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dz-15.rom", 0x00000, 0x10000, CRC(a01a5fd9) SHA1(c15e11fbc0ede9e4a232abe37e6d221d5789ce8e) )
	ROM_LOAD( "dz-16.rom", 0x10000, 0x10000, CRC(5a9a344a) SHA1(f4e8c2bae023ce996e99383873eba23ab6f972a8) )
	ROM_LOAD( "dz-12.rom", 0x20000, 0x10000, CRC(817fae99) SHA1(4179501aedbdf5bb0824bf1c13e033685e57a207) )
	ROM_LOAD( "dz-14.rom", 0x30000, 0x10000, CRC(0abbf76d) SHA1(fefb0cb7b866452b890bcf8c47b1ed95df35095e) )
	ROM_LOAD( "dz-11.rom", 0x40000, 0x10000, CRC(a5e19c24) SHA1(a4aae81a116577ee3cdd9e1a46cae413ae252b76) )
	ROM_LOAD( "dz-13.rom", 0x50000, 0x10000, CRC(3e7c0405) SHA1(2cdcb9a902acecec0729a906b7edb44baf130d32) )
	ROM_LOAD( "dz-17.rom", 0x60000, 0x10000, CRC(40361b8b) SHA1(6ee59129e236ead3d9b828fb9726311b7a4f2ff6) )
	ROM_LOAD( "dz-18.rom", 0x70000, 0x10000, CRC(8d219489) SHA1(0490ad84085d1a60ece1b8ab45f0c551d2ac219d) )

	ROM_REGION( 0x40000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dz-07.rom", 0x00000, 0x10000, CRC(e7455167) SHA1(a4582ced57862563ef626a25ced4072bc2c95750) )
	ROM_LOAD( "dz-08.rom", 0x10000, 0x10000, CRC(32f9ddfe) SHA1(2b8c228b0ca938ab7495d53e1a39275a8b872828) )
	ROM_LOAD( "dz-09.rom", 0x20000, 0x10000, CRC(bb6efc02) SHA1(ec501cd4a624d9c36a545dd100bc4f2f8b1e5cc0) )
	ROM_LOAD( "dz-10.rom", 0x30000, 0x10000, CRC(6ef9963b) SHA1(f12a2e2b0451a118234b2995185bb14d4998d430) )

	ROM_REGION( 0x0800, REGION_PROMS, 0 )
	ROM_LOAD( "dz19a.10d", 0x0000, 0x0400, CRC(47e1f83b) SHA1(f073eea1f33ed7a4862e4efd143debf1e0ee64b4) )
	ROM_LOAD( "dz20a.11d", 0x0400, 0x0400, CRC(d8fe2d99) SHA1(56f8fcf2f871c7d52d4299a5b9988401ada4319d) )
ROM_END

ROM_START( ghostb3 )
	ROM_REGION( 0x50000, REGION_CPU1, 0 )
	ROM_LOAD( "dz01-3b",   0x08000, 0x08000, CRC(c8cc862a) SHA1(e736107beb11a12cdf413655c6874df28d5a9c70) )
	ROM_LOAD( "dz-02.rom", 0x10000, 0x10000, CRC(8e117541) SHA1(7dfa6eabb29f39a615f3e5123bddcc7197ab82d0) )
	ROM_LOAD( "dz-03.rom", 0x20000, 0x10000, CRC(5606a8f4) SHA1(e46e887f13f648fe2162cb853b3c20fa60e3d215) )
	ROM_LOAD( "dz04-1",    0x30000, 0x10000, CRC(3c3eb09f) SHA1(ae4975992698fa97c68a857a25b470a05539160a) )
	ROM_LOAD( "dz05",      0x40000, 0x10000, CRC(b4971d33) SHA1(25e052c4b414c7bd7b6e3ae9c211873902afb5f7) )

	ROM_REGION( 2*0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dz-06.rom", 0x8000, 0x8000, CRC(798f56df) SHA1(aee33cd0c102015114e17f6cb98945e7cc806f55) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dz-00.rom", 0x00000, 0x08000, CRC(992b4f31) SHA1(a9f255286193ccc261a9b6983aabf3c76ebe5ce5) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dz-15.rom", 0x00000, 0x10000, CRC(a01a5fd9) SHA1(c15e11fbc0ede9e4a232abe37e6d221d5789ce8e) )
	ROM_LOAD( "dz-16.rom", 0x10000, 0x10000, CRC(5a9a344a) SHA1(f4e8c2bae023ce996e99383873eba23ab6f972a8) )
	ROM_LOAD( "dz-12.rom", 0x20000, 0x10000, CRC(817fae99) SHA1(4179501aedbdf5bb0824bf1c13e033685e57a207) )
	ROM_LOAD( "dz-14.rom", 0x30000, 0x10000, CRC(0abbf76d) SHA1(fefb0cb7b866452b890bcf8c47b1ed95df35095e) )
	ROM_LOAD( "dz-11.rom", 0x40000, 0x10000, CRC(a5e19c24) SHA1(a4aae81a116577ee3cdd9e1a46cae413ae252b76) )
	ROM_LOAD( "dz-13.rom", 0x50000, 0x10000, CRC(3e7c0405) SHA1(2cdcb9a902acecec0729a906b7edb44baf130d32) )
	ROM_LOAD( "dz-17.rom", 0x60000, 0x10000, CRC(40361b8b) SHA1(6ee59129e236ead3d9b828fb9726311b7a4f2ff6) )
	ROM_LOAD( "dz-18.rom", 0x70000, 0x10000, CRC(8d219489) SHA1(0490ad84085d1a60ece1b8ab45f0c551d2ac219d) )

	ROM_REGION( 0x40000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dz-07.rom", 0x00000, 0x10000, CRC(e7455167) SHA1(a4582ced57862563ef626a25ced4072bc2c95750) )
	ROM_LOAD( "dz-08.rom", 0x10000, 0x10000, CRC(32f9ddfe) SHA1(2b8c228b0ca938ab7495d53e1a39275a8b872828) )
	ROM_LOAD( "dz-09.rom", 0x20000, 0x10000, CRC(bb6efc02) SHA1(ec501cd4a624d9c36a545dd100bc4f2f8b1e5cc0) )
	ROM_LOAD( "dz-10.rom", 0x30000, 0x10000, CRC(6ef9963b) SHA1(f12a2e2b0451a118234b2995185bb14d4998d430) )

	ROM_REGION( 0x0800, REGION_PROMS, 0 )
	ROM_LOAD( "dz19a.10d", 0x0000, 0x0400, CRC(47e1f83b) SHA1(f073eea1f33ed7a4862e4efd143debf1e0ee64b4) )
	ROM_LOAD( "dz20a.11d", 0x0400, 0x0400, CRC(d8fe2d99) SHA1(56f8fcf2f871c7d52d4299a5b9988401ada4319d) )
ROM_END

ROM_START( meikyuh )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD( "dw-01.rom", 0x08000, 0x08000, CRC(87610c39) SHA1(47b83e7decd18f117d00a9f55c42da93b337c04a) )
	ROM_LOAD( "dw-02.rom", 0x10000, 0x10000, CRC(40c9b0b8) SHA1(81deb25e00eb4d4c5133ea42cda279c318ee771c) )
	ROM_LOAD( "dz-03.rom", 0x20000, 0x10000, CRC(5606a8f4) SHA1(e46e887f13f648fe2162cb853b3c20fa60e3d215) )
	ROM_LOAD( "dw-04.rom", 0x30000, 0x10000, CRC(235c0c36) SHA1(f0635f8348459cb8a56eb6184f1bc31c3a82de6a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dw-05.rom", 0x8000, 0x8000, CRC(c28c4d82) SHA1(ad88506bcbc9763e39d6e6bb25ef2bd6aa929f30) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dw-00.rom", 0x00000, 0x8000, CRC(3d25f15c) SHA1(590518460d069bc235b5efebec81731d7a2375de) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dw-14.rom", 0x00000, 0x10000, CRC(9b0dbfa9) SHA1(c9db6e70b217a34fbc2bf17da3f5ec6f0130514a) )
	ROM_LOAD( "dw-15.rom", 0x10000, 0x10000, CRC(95683fda) SHA1(aa91ad1cd685790e29e16d64bd75a5b4367cf87b) )
	ROM_LOAD( "dw-11.rom", 0x20000, 0x10000, CRC(1b1fcca7) SHA1(17e510c1b3efa0f6da49461c286b89295db6b9a6) )
	ROM_LOAD( "dw-13.rom", 0x30000, 0x10000, CRC(e7413056) SHA1(62048a9648cbb6b651e3409f77cee268822fd2e1) )
	ROM_LOAD( "dw-10.rom", 0x40000, 0x10000, CRC(57667546) SHA1(e7756997ea04204e62404ce8069f8cdb33cb4565) )
	ROM_LOAD( "dw-12.rom", 0x50000, 0x10000, CRC(4c548db8) SHA1(988411ab41884c926ca971e7b58f406f85be3b54) )
	ROM_LOAD( "dw-16.rom", 0x60000, 0x10000, CRC(e5bcf927) SHA1(b96bd4c124c9745fae1c1f35bdbbdec9f97ab4a5) )
	ROM_LOAD( "dw-17.rom", 0x70000, 0x10000, CRC(9e10f723) SHA1(159c5e3d821a10b64cd6d538d19063d0f5b057c0) )

	ROM_REGION( 0x40000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dw-06.rom", 0x00000, 0x10000, CRC(b65e029d) SHA1(f8791d57f688f16e0f076361603510e7133f4e36) )
	ROM_LOAD( "dw-07.rom", 0x10000, 0x10000, CRC(668d995d) SHA1(dc6221de6103168c8e19f2c6eb159b8989ca2208) )
	ROM_LOAD( "dw-08.rom", 0x20000, 0x10000, CRC(bb2cf4a0) SHA1(78806adb6a9ad9fc0707ead567a3220eb2bdb32f) )
	ROM_LOAD( "dw-09.rom", 0x30000, 0x10000, CRC(6a528d13) SHA1(f1ef592f1efea637abde26bb8e3d02d552582a43) )

	ROM_REGION( 0x0800, REGION_PROMS, 0 )
	ROM_LOAD( "dz19a.10d", 0x0000, 0x0400, BAD_DUMP CRC(47e1f83b) SHA1(f073eea1f33ed7a4862e4efd143debf1e0ee64b4)  )
	ROM_LOAD( "dz20a.11d", 0x0400, 0x0400, BAD_DUMP CRC(d8fe2d99) SHA1(56f8fcf2f871c7d52d4299a5b9988401ada4319d)  )
ROM_END

ROM_START( srdarwin )
	ROM_REGION( 0x28000, REGION_CPU1, 0 )
	ROM_LOAD( "dy01-e.b14", 0x20000, 0x08000, CRC(176e9299) SHA1(20cd44ab610e384ab4f0172054c9adc432b12e9c) )
	ROM_CONTINUE(           0x08000, 0x08000 )
	ROM_LOAD( "dy00.b16", 0x10000, 0x10000, CRC(2bf6b461) SHA1(435d922c7b9df7f2b2f774346caed81d330be8a0) )

	ROM_REGION( 2*0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dy04.d7", 0x8000, 0x8000, CRC(2ae3591c) SHA1(f21b06d84e2c3d3895be0812024641fd006e45cf) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dy05.b6", 0x00000, 0x4000, CRC(8780e8a3) SHA1(03ea91fdc5aba8e139201604fb3bf9b69f71f056) )

	ROM_REGION( 0x30000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dy07.h16", 0x00000, 0x8000, CRC(97eaba60) SHA1(e3252b67bad7babcf4ece39f46ae4aeb950eb92b) )
	ROM_LOAD( "dy06.h14", 0x08000, 0x8000, CRC(c279541b) SHA1(eb3737413499d07b6c2af99a95b27b2590e670c5) )
	ROM_LOAD( "dy09.k13", 0x10000, 0x8000, CRC(d30d1745) SHA1(647b6121ab6fa812368da45e1295cc41f73be89d) )
	ROM_LOAD( "dy08.k11", 0x18000, 0x8000, CRC(71d645fd) SHA1(a74a9b9697fc39b4e675e732a9d7d82976cc95dd) )
	ROM_LOAD( "dy11.k16", 0x20000, 0x8000, CRC(fd9ccc5b) SHA1(b38c44c01acdc455d4192e4c8be1d68d9eb0c7b6) )
	ROM_LOAD( "dy10.k14", 0x28000, 0x8000, CRC(88770ab8) SHA1(0a4a807a8d3b0653864bd984872d5567836f8cf8) )

	ROM_REGION( 0x40000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dy03.b4",  0x00000, 0x4000, CRC(44f2a4f9) SHA1(97368dd112451cd630f2fa5ba54679e84e7d4d97) )
	ROM_CONTINUE(         0x10000, 0x4000 )
	ROM_CONTINUE(         0x20000, 0x4000 )
	ROM_CONTINUE(         0x30000, 0x4000 )
	ROM_LOAD( "dy02.b5",  0x08000, 0x4000, CRC(522d9a9e) SHA1(248274ed6df604357cad386fcf0521b26810aa0e) )
	ROM_CONTINUE(         0x18000, 0x4000 )
	ROM_CONTINUE(         0x28000, 0x4000 )
	ROM_CONTINUE(         0x38000, 0x4000 )

	ROM_REGION( 256, REGION_PROMS, 0 )
	ROM_LOAD( "dy12.f4",  0x00000,  0x100,  CRC(ebfaaed9) SHA1(5723dbfa3eb3fc4df8c8975b320a5c49848309d8) )	/* Priority (Not yet used) */
ROM_END

ROM_START( srdarwnj )
	ROM_REGION( 0x28000, REGION_CPU1, 0 )
	ROM_LOAD( "dy_01.rom", 0x20000, 0x08000, CRC(1eeee4ff) SHA1(89a70de8bd61c671582b11773ce69b2edcd9c2f8) )
	ROM_CONTINUE(          0x08000, 0x08000 )
	ROM_LOAD( "dy00.b16", 0x10000, 0x10000, CRC(2bf6b461) SHA1(435d922c7b9df7f2b2f774346caed81d330be8a0) )

	ROM_REGION( 2*0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "dy04.d7", 0x8000, 0x8000, CRC(2ae3591c) SHA1(f21b06d84e2c3d3895be0812024641fd006e45cf) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dy05.b6", 0x00000, 0x4000, CRC(8780e8a3) SHA1(03ea91fdc5aba8e139201604fb3bf9b69f71f056) )

	ROM_REGION( 0x30000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dy07.h16", 0x00000, 0x8000, CRC(97eaba60) SHA1(e3252b67bad7babcf4ece39f46ae4aeb950eb92b) )
	ROM_LOAD( "dy06.h14", 0x08000, 0x8000, CRC(c279541b) SHA1(eb3737413499d07b6c2af99a95b27b2590e670c5) )
	ROM_LOAD( "dy09.k13", 0x10000, 0x8000, CRC(d30d1745) SHA1(647b6121ab6fa812368da45e1295cc41f73be89d) )
	ROM_LOAD( "dy08.k11", 0x18000, 0x8000, CRC(71d645fd) SHA1(a74a9b9697fc39b4e675e732a9d7d82976cc95dd) )
	ROM_LOAD( "dy11.k16", 0x20000, 0x8000, CRC(fd9ccc5b) SHA1(b38c44c01acdc455d4192e4c8be1d68d9eb0c7b6) )
	ROM_LOAD( "dy10.k14", 0x28000, 0x8000, CRC(88770ab8) SHA1(0a4a807a8d3b0653864bd984872d5567836f8cf8) )

	ROM_REGION( 0x40000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dy03.b4",  0x00000, 0x4000, CRC(44f2a4f9) SHA1(97368dd112451cd630f2fa5ba54679e84e7d4d97) )
	ROM_CONTINUE(         0x10000, 0x4000 )
	ROM_CONTINUE(         0x20000, 0x4000 )
	ROM_CONTINUE(         0x30000, 0x4000 )
	ROM_LOAD( "dy02.b5",  0x08000, 0x4000, CRC(522d9a9e) SHA1(248274ed6df604357cad386fcf0521b26810aa0e) )
	ROM_CONTINUE(         0x18000, 0x4000 )
	ROM_CONTINUE(         0x28000, 0x4000 )
	ROM_CONTINUE(         0x38000, 0x4000 )

	ROM_REGION( 256, REGION_PROMS, 0 )
	ROM_LOAD( "dy12.f4",  0x00000,  0x100,  CRC(ebfaaed9) SHA1(5723dbfa3eb3fc4df8c8975b320a5c49848309d8) )	/* Priority (Not yet used) */
ROM_END

ROM_START( gondo )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD( "dt-00.256", 0x08000, 0x08000, CRC(a8cf9118) SHA1(865744c9866957d686a31608d356e279fe58934e) )
	ROM_LOAD( "dt-01.512", 0x10000, 0x10000, CRC(c39bb877) SHA1(9beb59ba19f38417c5d4d36e8f3c41f2b017d2d6) )
	ROM_LOAD( "dt-02.512", 0x20000, 0x10000, CRC(bb5e674b) SHA1(8057dc7464a8b6987536f248d607957923b223cf) )
	ROM_LOAD( "dt-03.512", 0x30000, 0x10000, CRC(99c32b13) SHA1(3d79f48e7d198cb2e519d592a89eda505044bce5) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dt-05.256", 0x8000, 0x8000, CRC(ec08aa29) SHA1(ce83974ae095d9518d1ebf9f7e712f0cbc2c1b42) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dt-14.256", 0x00000, 0x08000, CRC(4bef16e1) SHA1(b8157a7a1b8f36cea1fd353267a4e03d920cb4aa) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dt-19.512", 0x00000, 0x10000, CRC(da2abe4b) SHA1(d53e4769671f3fd437edcff7e7ea05156bbcb45d) )
	ROM_LOAD( "dt-20.256", 0x10000, 0x08000, CRC(42d01002) SHA1(5a289ffdc83c05f21908a5d0b6247da5b51c1ddd) )
	ROM_LOAD( "dt-16.512", 0x20000, 0x10000, CRC(e9955d8f) SHA1(aeef5e18f9d36c1bab3000e95205ce1b18cfbf0b) )
	ROM_LOAD( "dt-18.256", 0x30000, 0x08000, CRC(c0c5df1c) SHA1(5b0f71f590434cdd0545ce098666798927727469) )
	ROM_LOAD( "dt-15.512", 0x40000, 0x10000, CRC(a54b2eb6) SHA1(25cb61f67135672154f1ad8e0c49ec04655e91de) )
	ROM_LOAD( "dt-17.256", 0x50000, 0x08000, CRC(3bbcff0d) SHA1(a8f7aa56ff49ed6b29240c3504d6c9945944953b) )
	ROM_LOAD( "dt-21.512", 0x60000, 0x10000, CRC(1c5f682d) SHA1(4b7022cce930a9e9a0087c91e8344269fe7ed889) )
	ROM_LOAD( "dt-22.256", 0x70000, 0x08000, CRC(c1876a5f) SHA1(66122ce765723765e20036bd4d461a210c8b94d3) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dt-08.512", 0x00000, 0x08000, CRC(aec483f5) SHA1(1d6de823ab0eeb9c89e9c227428ff278663627f3) )
	ROM_CONTINUE(          0x10000, 0x08000 )
	ROM_LOAD( "dt-09.256", 0x08000, 0x08000, CRC(446f0ce0) SHA1(072b88d6de5aa0ed6b1d60c266bcf170dea927d5) )
	ROM_LOAD( "dt-06.512", 0x20000, 0x08000, CRC(3fe1527f) SHA1(b8df4bef2b1a879b65214025fc3b5998ef5c8886) )
	ROM_CONTINUE(          0x30000, 0x08000 )
	ROM_LOAD( "dt-07.256", 0x28000, 0x08000, CRC(61f9bce5) SHA1(ef8a5f5e4c66a143304bcab50ca87579f1507864) )
	ROM_LOAD( "dt-12.512", 0x40000, 0x08000, CRC(1a72ca8d) SHA1(f412758452cb3417e85c355ccb8794fde7edf1cc) )
	ROM_CONTINUE(          0x50000, 0x08000 )
	ROM_LOAD( "dt-13.256", 0x48000, 0x08000, CRC(ccb81aec) SHA1(56e524ed4373b7bd1074a0d22ff75ede379f1696) )
	ROM_LOAD( "dt-10.512", 0x60000, 0x08000, CRC(cfcfc9ed) SHA1(57f43d638cf864d68420f0203740be7bda9da5ca) )
	ROM_CONTINUE(          0x70000, 0x08000 )
	ROM_LOAD( "dt-11.256", 0x68000, 0x08000, CRC(53e9cf17) SHA1(8cbb45154a60f42f1b1e7299b12d2e92fc194df8) )

	ROM_REGION( 1024, REGION_PROMS, 0 )
	ROM_LOAD( "mb7122e.10b", 0x00000,  0x400,  CRC(dcbfec4e) SHA1(a375caef4575746870e285d90ba991ea7daefad6) )	/* Priority (Not yet used) */
ROM_END

ROM_START( makyosen )
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	ROM_LOAD( "ds00",      0x08000, 0x08000, CRC(33bb16fe) SHA1(5d3873b66e0d08b35d56a8b508c774b27368a100) )
	ROM_LOAD( "dt-01.512", 0x10000, 0x10000, CRC(c39bb877) SHA1(9beb59ba19f38417c5d4d36e8f3c41f2b017d2d6) )
	ROM_LOAD( "ds02",      0x20000, 0x10000, CRC(925307a4) SHA1(1e8b8eb21df1a11b14c981b343b34c6cc3676517) )
	ROM_LOAD( "ds03",      0x30000, 0x10000, CRC(9c0fcbf6) SHA1(bfe42b5277fea111840a9f59b2cb8dfe44444029) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "ds05",      0x8000, 0x8000, CRC(e6e28ca9) SHA1(3b1f8219331db1910bfb428f8964f8fc1063df6f) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ds14",      0x00000, 0x08000, CRC(00cbe9c8) SHA1(de7b640de8fd54ee79194945c96d5768d09f483b) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dt-19.512", 0x00000, 0x10000, CRC(da2abe4b) SHA1(d53e4769671f3fd437edcff7e7ea05156bbcb45d) )
	ROM_LOAD( "ds20",      0x10000, 0x08000, CRC(0eef7f56) SHA1(05c23aa6a598478cd4822634cff96055c585e9d2) )
	ROM_LOAD( "dt-16.512", 0x20000, 0x10000, CRC(e9955d8f) SHA1(aeef5e18f9d36c1bab3000e95205ce1b18cfbf0b) )
	ROM_LOAD( "ds18",      0x30000, 0x08000, CRC(2b2d1468) SHA1(a144ac1b367e1fec876156230e9ab1c99416962e) )
	ROM_LOAD( "dt-15.512", 0x40000, 0x10000, CRC(a54b2eb6) SHA1(25cb61f67135672154f1ad8e0c49ec04655e91de) )
	ROM_LOAD( "ds17",      0x50000, 0x08000, CRC(75ae349a) SHA1(15755a28925d5ed37fab4bd988716fcc5d20c290) )
	ROM_LOAD( "dt-21.512", 0x60000, 0x10000, CRC(1c5f682d) SHA1(4b7022cce930a9e9a0087c91e8344269fe7ed889) )
	ROM_LOAD( "ds22",      0x70000, 0x08000, CRC(c8ffb148) SHA1(ae1a8b3cd1f5e423dc1a3c7d05f9fe7c689432e3) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dt-08.512", 0x00000, 0x08000, CRC(aec483f5) SHA1(1d6de823ab0eeb9c89e9c227428ff278663627f3) )
	ROM_CONTINUE(          0x10000, 0x08000 )
	ROM_LOAD( "dt-09.256", 0x08000, 0x08000, CRC(446f0ce0) SHA1(072b88d6de5aa0ed6b1d60c266bcf170dea927d5) )
	ROM_LOAD( "dt-06.512", 0x20000, 0x08000, CRC(3fe1527f) SHA1(b8df4bef2b1a879b65214025fc3b5998ef5c8886) )
	ROM_CONTINUE(          0x30000, 0x08000 )
	ROM_LOAD( "dt-07.256", 0x28000, 0x08000, CRC(61f9bce5) SHA1(ef8a5f5e4c66a143304bcab50ca87579f1507864) )
	ROM_LOAD( "dt-12.512", 0x40000, 0x08000, CRC(1a72ca8d) SHA1(f412758452cb3417e85c355ccb8794fde7edf1cc) )
	ROM_CONTINUE(          0x50000, 0x08000 )
	ROM_LOAD( "dt-13.256", 0x48000, 0x08000, CRC(ccb81aec) SHA1(56e524ed4373b7bd1074a0d22ff75ede379f1696) )
	ROM_LOAD( "dt-10.512", 0x60000, 0x08000, CRC(cfcfc9ed) SHA1(57f43d638cf864d68420f0203740be7bda9da5ca) )
	ROM_CONTINUE(          0x70000, 0x08000 )
	ROM_LOAD( "dt-11.256", 0x68000, 0x08000, CRC(53e9cf17) SHA1(8cbb45154a60f42f1b1e7299b12d2e92fc194df8) )

	ROM_REGION( 1024, REGION_PROMS, 0 )
	ROM_LOAD( "mb7122e.10b", 0x00000,  0x400,  CRC(dcbfec4e) SHA1(a375caef4575746870e285d90ba991ea7daefad6) )	/* Priority (Not yet used) */
ROM_END

ROM_START( oscar )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "ed10", 0x08000, 0x08000, CRC(f9b0d4d4) SHA1(dc2aba978ba96f365027c7be5714728d5d7fb802) )
	ROM_LOAD( "ed09", 0x10000, 0x10000, CRC(e2d4bba9) SHA1(99f0310debe51f4bcd00b5fdaedc1caf2eeccdeb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "ed11", 0x0000, 0x10000,  CRC(10e5d919) SHA1(13bc3497cb4aaa6dd272853819212ad63656f8a7) )

	ROM_REGION( 2*0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000,  CRC(432031c5) SHA1(af2deea48b98eb0f9e85a4fb1486021f999f9abd) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, CRC(308ac264) SHA1(fd1c4ec4e4f99c33e93cd15e178c4714251a9b7e) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, CRC(416a791b) SHA1(e6541b713225289a43962363029eb0e22a1ecb4a) )
	ROM_LOAD( "ed05", 0x20000, 0x10000, CRC(fcdba431) SHA1(0be2194519c36ddf136610f60890506eda1faf0b) )
	ROM_LOAD( "ed06", 0x40000, 0x10000, CRC(7d50bebc) SHA1(06375f3273c48c7c7d81f1c15cbc5d3f3e05b8ed) )
	ROM_LOAD( "ed07", 0x60000, 0x10000, CRC(8fdf0fa5) SHA1(2b4d1ca1436864e89b13b3fa151a4a3708592e0a) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, CRC(d3a58e9e) SHA1(35eda2aa630fc2c11a1aff2b00bcfabe2f3d4249) )
	ROM_LOAD( "ed03", 0x20000, 0x10000, CRC(4fc4fb0f) SHA1(0906762a3adbffe765e072255262fedaa78bdb2a) )
	ROM_LOAD( "ed00", 0x40000, 0x10000, CRC(ac201f2d) SHA1(77f13eb6a1a44444ca9b25363031451b0d68c988) )
	ROM_LOAD( "ed02", 0x60000, 0x10000, CRC(7ddc5651) SHA1(f5ec5245cf3d9d4d9c1df6a8128c24565e331317) )
ROM_END

ROM_START( oscarj0 )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "du10", 0x08000, 0x08000, CRC(120040d8) SHA1(22d5f84f3ca724cbf39dfc4790f2175ba4945aaf) )
	ROM_LOAD( "ed09", 0x10000, 0x10000, CRC(e2d4bba9) SHA1(99f0310debe51f4bcd00b5fdaedc1caf2eeccdeb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "du11", 0x0000, 0x10000, CRC(ff45c440) SHA1(4769944bcfebcdcba7ed7d5133d4d9f98890d75c) )

	ROM_REGION( 2*0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000, CRC(432031c5) SHA1(af2deea48b98eb0f9e85a4fb1486021f999f9abd) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, CRC(308ac264) SHA1(fd1c4ec4e4f99c33e93cd15e178c4714251a9b7e) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, CRC(416a791b) SHA1(e6541b713225289a43962363029eb0e22a1ecb4a) )
	ROM_LOAD( "ed05", 0x20000, 0x10000, CRC(fcdba431) SHA1(0be2194519c36ddf136610f60890506eda1faf0b) )
	ROM_LOAD( "ed06", 0x40000, 0x10000, CRC(7d50bebc) SHA1(06375f3273c48c7c7d81f1c15cbc5d3f3e05b8ed) )
	ROM_LOAD( "ed07", 0x60000, 0x10000, CRC(8fdf0fa5) SHA1(2b4d1ca1436864e89b13b3fa151a4a3708592e0a) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, CRC(d3a58e9e) SHA1(35eda2aa630fc2c11a1aff2b00bcfabe2f3d4249) )
	ROM_LOAD( "ed03", 0x20000, 0x10000, CRC(4fc4fb0f) SHA1(0906762a3adbffe765e072255262fedaa78bdb2a) )
	ROM_LOAD( "ed00", 0x40000, 0x10000, CRC(ac201f2d) SHA1(77f13eb6a1a44444ca9b25363031451b0d68c988) )
	ROM_LOAD( "ed02", 0x60000, 0x10000, CRC(7ddc5651) SHA1(f5ec5245cf3d9d4d9c1df6a8128c24565e331317) )
ROM_END

ROM_START( oscarj1 )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "oscr10-1.bin", 0x08000, 0x08000, CRC(4ebc9f7a) SHA1(df8fdc4b4b203dc1bdd048f069fb6b723bdea0d2) ) /* DU10-1 */
	ROM_LOAD( "ed09", 0x10000, 0x10000, CRC(e2d4bba9) SHA1(99f0310debe51f4bcd00b5fdaedc1caf2eeccdeb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "du11", 0x0000, 0x10000, CRC(ff45c440) SHA1(4769944bcfebcdcba7ed7d5133d4d9f98890d75c) )

	ROM_REGION( 2*0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000, CRC(432031c5) SHA1(af2deea48b98eb0f9e85a4fb1486021f999f9abd) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, CRC(308ac264) SHA1(fd1c4ec4e4f99c33e93cd15e178c4714251a9b7e) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, CRC(416a791b) SHA1(e6541b713225289a43962363029eb0e22a1ecb4a) )
	ROM_LOAD( "ed05", 0x20000, 0x10000, CRC(fcdba431) SHA1(0be2194519c36ddf136610f60890506eda1faf0b) )
	ROM_LOAD( "ed06", 0x40000, 0x10000, CRC(7d50bebc) SHA1(06375f3273c48c7c7d81f1c15cbc5d3f3e05b8ed) )
	ROM_LOAD( "ed07", 0x60000, 0x10000, CRC(8fdf0fa5) SHA1(2b4d1ca1436864e89b13b3fa151a4a3708592e0a) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, CRC(d3a58e9e) SHA1(35eda2aa630fc2c11a1aff2b00bcfabe2f3d4249) )
	ROM_LOAD( "ed03", 0x20000, 0x10000, CRC(4fc4fb0f) SHA1(0906762a3adbffe765e072255262fedaa78bdb2a) )
	ROM_LOAD( "ed00", 0x40000, 0x10000, CRC(ac201f2d) SHA1(77f13eb6a1a44444ca9b25363031451b0d68c988) )
	ROM_LOAD( "ed02", 0x60000, 0x10000, CRC(7ddc5651) SHA1(f5ec5245cf3d9d4d9c1df6a8128c24565e331317) )
ROM_END

ROM_START( oscarj )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "oscr10-2.bin", 0x08000, 0x08000, CRC(114e898d) SHA1(1072ccabe6d53c50cdfa1e27d5d848ecdd6559cc) ) /* DU10-2 */
	ROM_LOAD( "ed09", 0x10000, 0x10000, CRC(e2d4bba9) SHA1(99f0310debe51f4bcd00b5fdaedc1caf2eeccdeb) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "du11", 0x0000, 0x10000, CRC(ff45c440) SHA1(4769944bcfebcdcba7ed7d5133d4d9f98890d75c) )

	ROM_REGION( 2*0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU + 64k for decrypted opcodes */
	ROM_LOAD( "ed12", 0x8000, 0x8000, CRC(432031c5) SHA1(af2deea48b98eb0f9e85a4fb1486021f999f9abd) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "ed08", 0x00000, 0x04000, CRC(308ac264) SHA1(fd1c4ec4e4f99c33e93cd15e178c4714251a9b7e) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "ed04", 0x00000, 0x10000, CRC(416a791b) SHA1(e6541b713225289a43962363029eb0e22a1ecb4a) )
	ROM_LOAD( "ed05", 0x20000, 0x10000, CRC(fcdba431) SHA1(0be2194519c36ddf136610f60890506eda1faf0b) )
	ROM_LOAD( "ed06", 0x40000, 0x10000, CRC(7d50bebc) SHA1(06375f3273c48c7c7d81f1c15cbc5d3f3e05b8ed) )
	ROM_LOAD( "ed07", 0x60000, 0x10000, CRC(8fdf0fa5) SHA1(2b4d1ca1436864e89b13b3fa151a4a3708592e0a) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "ed01", 0x00000, 0x10000, CRC(d3a58e9e) SHA1(35eda2aa630fc2c11a1aff2b00bcfabe2f3d4249) )
	ROM_LOAD( "ed03", 0x20000, 0x10000, CRC(4fc4fb0f) SHA1(0906762a3adbffe765e072255262fedaa78bdb2a) )
	ROM_LOAD( "ed00", 0x40000, 0x10000, CRC(ac201f2d) SHA1(77f13eb6a1a44444ca9b25363031451b0d68c988) )
	ROM_LOAD( "ed02", 0x60000, 0x10000, CRC(7ddc5651) SHA1(f5ec5245cf3d9d4d9c1df6a8128c24565e331317) )
ROM_END

ROM_START( lastmisn )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "dl03-6.13h",  0x08000, 0x08000, CRC(47751a5e) SHA1(190970a6eb849781e8853f2bed7b34ac44e569ca) ) /* Rev 6 roms */
	ROM_LOAD( "lm_dl04.7h",  0x10000, 0x10000, CRC(7dea1552) SHA1(920684413e2ba4313111e79821c5714977b26b1a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "lm_dl02.18h", 0x0000, 0x10000, CRC(ec9b5daf) SHA1(86d47bad123676abc82dd7c92943878c54c33075) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dl05-.5h",    0x8000, 0x8000, CRC(1a5df8c0) SHA1(83d36b1d5fb87f50c44f3110804d6bbdbbc0da99) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dl01-.2a",    0x00000, 0x2000, CRC(f3787a5d) SHA1(3701df42cb2aca951963703e72c6c7b272eed82b) )
	ROM_CONTINUE(		     0x06000, 0x2000 )
	ROM_CONTINUE(		 	 0x04000, 0x2000 )
	ROM_CONTINUE(		 	 0x02000, 0x2000 )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dl11-.13f",   0x00000, 0x08000, CRC(36579d3b) SHA1(8edf952dafcd5bc66e08074687f0bec809fd4c2f) )
	ROM_LOAD( "dl12-.9f",    0x20000, 0x08000, CRC(2ba6737e) SHA1(c5e4c27726bf14e9cd60d62e2f17ea5be8093c37) )
	ROM_LOAD( "dl13-.8f",    0x40000, 0x08000, CRC(39a7dc93) SHA1(3b7968fd06ac0379525c1d3e73f8bbe18ea36439) )
	ROM_LOAD( "dl10-.16f",   0x60000, 0x08000, CRC(fe275ea8) SHA1(2f089f96583235f1f5226ef2a64b430d84efbeee) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dl09-.12k",   0x00000, 0x10000, CRC(6a5a0c5d) SHA1(0106cf693c284be5faf96e56b651fab92a410915) )
	ROM_LOAD( "dl08-.14k",   0x20000, 0x10000, CRC(3b38cfce) SHA1(d6829bed6916fb301c08031bd466ee4dcc05b275) )
	ROM_LOAD( "dl07-.15k",   0x40000, 0x10000, CRC(1b60604d) SHA1(1ee15cfdac87f7eeb92050766293b894cfad1466) )
	ROM_LOAD( "dl06-.17k",   0x60000, 0x10000, CRC(c43c26a7) SHA1(896e278935b100edc12cd970469f2e8293eb96cc) )

	ROM_REGION( 256, REGION_PROMS, 0 )
	ROM_LOAD( "dl-14.9c",    0x00000,  0x100,  CRC(2e55aa12) SHA1(c0f2b9649467eb9d2c1e47589b5990f5c5e8cc93) )	/* Priority (Not yet used) */
ROM_END

ROM_START( lastmsno )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "lm_dl03.13h", 0x08000, 0x08000, CRC(357f5f6b) SHA1(a114aac50db62a6bcb943681e517ad7c88ec47f4) ) /* Rev 5 roms */
	ROM_LOAD( "lm_dl04.7h",  0x10000, 0x10000, CRC(7dea1552) SHA1(920684413e2ba4313111e79821c5714977b26b1a) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "lm_dl02.18h", 0x0000, 0x10000, CRC(ec9b5daf) SHA1(86d47bad123676abc82dd7c92943878c54c33075) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dl05-.5h",    0x8000, 0x8000, CRC(1a5df8c0) SHA1(83d36b1d5fb87f50c44f3110804d6bbdbbc0da99) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dl01-.2a",    0x00000, 0x2000, CRC(f3787a5d) SHA1(3701df42cb2aca951963703e72c6c7b272eed82b) )
	ROM_CONTINUE(		     0x06000, 0x2000 )
	ROM_CONTINUE(		 	 0x04000, 0x2000 )
	ROM_CONTINUE(		 	 0x02000, 0x2000 )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dl11-.13f",   0x00000, 0x08000, CRC(36579d3b) SHA1(8edf952dafcd5bc66e08074687f0bec809fd4c2f) )
	ROM_LOAD( "dl12-.9f",    0x20000, 0x08000, CRC(2ba6737e) SHA1(c5e4c27726bf14e9cd60d62e2f17ea5be8093c37) )
	ROM_LOAD( "dl13-.8f",    0x40000, 0x08000, CRC(39a7dc93) SHA1(3b7968fd06ac0379525c1d3e73f8bbe18ea36439) )
	ROM_LOAD( "dl10-.16f",   0x60000, 0x08000, CRC(fe275ea8) SHA1(2f089f96583235f1f5226ef2a64b430d84efbeee) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dl09-.12k",   0x00000, 0x10000, CRC(6a5a0c5d) SHA1(0106cf693c284be5faf96e56b651fab92a410915) )
	ROM_LOAD( "dl08-.14k",   0x20000, 0x10000, CRC(3b38cfce) SHA1(d6829bed6916fb301c08031bd466ee4dcc05b275) )
	ROM_LOAD( "dl07-.15k",   0x40000, 0x10000, CRC(1b60604d) SHA1(1ee15cfdac87f7eeb92050766293b894cfad1466) )
	ROM_LOAD( "dl06-.17k",   0x60000, 0x10000, CRC(c43c26a7) SHA1(896e278935b100edc12cd970469f2e8293eb96cc) )

	ROM_REGION( 256, REGION_PROMS, 0 )
	ROM_LOAD( "dl-14.9c",    0x00000,  0x100,  CRC(2e55aa12) SHA1(c0f2b9649467eb9d2c1e47589b5990f5c5e8cc93) )	/* Priority (Not yet used) */
ROM_END

ROM_START( lastmsnj )
	ROM_REGION( 0x20000, REGION_CPU1, 0 )
	ROM_LOAD( "dl03-.13h",   0x08000, 0x08000, CRC(4be5e7e1) SHA1(9f943658663da31947cebdcbcb5f4e2be0714c06) )
	ROM_LOAD( "dl04-.7h",    0x10000, 0x10000, CRC(f026adf9) SHA1(4ccd0e714a6eb7cee388c93beee2d5510407c961) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "dl02-.18h",   0x0000, 0x10000, CRC(d0de2b5d) SHA1(e0bb34c2a2ef6fc6f05ab9a98bd23a39004c0c05) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dl05-.5h",    0x8000, 0x8000, CRC(1a5df8c0) SHA1(83d36b1d5fb87f50c44f3110804d6bbdbbc0da99) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dl01-.2a",    0x00000, 0x2000, CRC(f3787a5d) SHA1(3701df42cb2aca951963703e72c6c7b272eed82b) )
	ROM_CONTINUE(		     0x06000, 0x2000 )
	ROM_CONTINUE(		 	 0x04000, 0x2000 )
	ROM_CONTINUE(		 	 0x02000, 0x2000 )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dl11-.13f",   0x00000, 0x08000, CRC(36579d3b) SHA1(8edf952dafcd5bc66e08074687f0bec809fd4c2f) )
	ROM_LOAD( "dl12-.9f",    0x20000, 0x08000, CRC(2ba6737e) SHA1(c5e4c27726bf14e9cd60d62e2f17ea5be8093c37) )
	ROM_LOAD( "dl13-.8f",    0x40000, 0x08000, CRC(39a7dc93) SHA1(3b7968fd06ac0379525c1d3e73f8bbe18ea36439) )
	ROM_LOAD( "dl10-.16f",   0x60000, 0x08000, CRC(fe275ea8) SHA1(2f089f96583235f1f5226ef2a64b430d84efbeee) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dl09-.12k",   0x00000, 0x10000, CRC(6a5a0c5d) SHA1(0106cf693c284be5faf96e56b651fab92a410915) )
	ROM_LOAD( "dl08-.14k",   0x20000, 0x10000, CRC(3b38cfce) SHA1(d6829bed6916fb301c08031bd466ee4dcc05b275) )
	ROM_LOAD( "dl07-.15k",   0x40000, 0x10000, CRC(1b60604d) SHA1(1ee15cfdac87f7eeb92050766293b894cfad1466) )
	ROM_LOAD( "dl06-.17k",   0x60000, 0x10000, CRC(c43c26a7) SHA1(896e278935b100edc12cd970469f2e8293eb96cc) )

	ROM_REGION( 256, REGION_PROMS, 0 )
	ROM_LOAD( "dl-14.9c",    0x00000,  0x100,  CRC(2e55aa12) SHA1(c0f2b9649467eb9d2c1e47589b5990f5c5e8cc93) )	/* Priority (Not yet used) */
ROM_END

ROM_START( shackled )
	ROM_REGION( 0x48000, REGION_CPU1, 0 )
	ROM_LOAD( "dk-02.rom", 0x08000, 0x08000, CRC(87f8fa85) SHA1(1cb93a60eefdb453a3cc6ec9c5cc2e367fb8aeb0) )
	ROM_LOAD( "dk-06.rom", 0x10000, 0x10000, CRC(69ad62d1) SHA1(1aa23b12ab4f1908cddd25f091e1f9bd70a5113c) )
	ROM_LOAD( "dk-05.rom", 0x20000, 0x10000, CRC(598dd128) SHA1(10843c5352eef03c8675df6abaf23c9c9c795aa3) )
	ROM_LOAD( "dk-04.rom", 0x30000, 0x10000, CRC(36d305d4) SHA1(17586c316aff405cf20c1467d69c98fa2a3c2630) )
	ROM_LOAD( "dk-03.rom", 0x40000, 0x08000, CRC(6fd90fd1) SHA1(2f8db17e5545c82d243a7e23e7bda2c2a9101360) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "dk-01.rom", 0x00000, 0x10000, CRC(71fe3bda) SHA1(959cce01362b2c670c2e15b03a78a1ff9cea4ee9) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dk-07.rom", 0x08000, 0x08000, CRC(887e4bcc) SHA1(6427396080e9cd8647adff47c8ed04593a14268c) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dk-00.rom", 0x00000, 0x08000, CRC(69b975aa) SHA1(38cb96768c79ff1aa1b4b190e08ec9155baf698a) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dk-12.rom", 0x00000, 0x10000, CRC(615c2371) SHA1(30b25dc27d34646d886a465c77622eaa894d83c3) )
	ROM_LOAD( "dk-13.rom", 0x10000, 0x10000, CRC(479aa503) SHA1(1167f0d15439c95a1094f81855203e863ce0488d) )
	ROM_LOAD( "dk-14.rom", 0x20000, 0x10000, CRC(cdc24246) SHA1(1a4189bc2b1fa99740dd7921608159936ba3bd07) )
	ROM_LOAD( "dk-15.rom", 0x30000, 0x10000, CRC(88db811b) SHA1(7d3c4a80925f323efb589798b4a341d1a2ca95f9) )
	ROM_LOAD( "dk-16.rom", 0x40000, 0x10000, CRC(061a76bd) SHA1(5bcb513e48bed9b7c4207d94531be691a85e295d) )
	ROM_LOAD( "dk-17.rom", 0x50000, 0x10000, CRC(a6c5d8af) SHA1(58f3fece9a5ef8b39090a2f39610381b8e7cdbf7) )
	ROM_LOAD( "dk-18.rom", 0x60000, 0x10000, CRC(4d466757) SHA1(701d79bebbba4f266e19080d16ff2f93ffa94287) )
	ROM_LOAD( "dk-19.rom", 0x70000, 0x10000, CRC(1911e83e) SHA1(174e9db3f2211ecbbb93c6bda8f6185dbfdbc818) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dk-11.rom", 0x00000, 0x10000, CRC(5cf5719f) SHA1(8c7582ac19010421ec748391a193aa18e51b981f) )
	ROM_LOAD( "dk-10.rom", 0x20000, 0x10000, CRC(408e6d08) SHA1(28cb76792e5f84bd101a91cb82597a5939804f84) )
	ROM_LOAD( "dk-09.rom", 0x40000, 0x10000, CRC(c1557fac) SHA1(7d39ec793113a48baf45c2ea07abb07e2e48985a) )
	ROM_LOAD( "dk-08.rom", 0x60000, 0x10000, CRC(5e54e9f5) SHA1(1ab41a3bde1f2c2be670e89cf402be28001c17d1) )
ROM_END

ROM_START( breywood )
	ROM_REGION( 0x48000, REGION_CPU1, 0 )
	ROM_LOAD( "7.bin", 0x08000, 0x08000, CRC(c19856b9) SHA1(766994703bb59879c311675353d7231ad27c7c16) )
	ROM_LOAD( "3.bin", 0x10000, 0x10000, CRC(2860ea02) SHA1(7ac090c3ae9d71baa6227ec9555f1c9f2d25ea0d) )
	ROM_LOAD( "4.bin", 0x20000, 0x10000, CRC(0fdd915e) SHA1(262df956dfc727c710ade28af7f33fddaafd7ee2) )
	ROM_LOAD( "5.bin", 0x30000, 0x10000, CRC(71036579) SHA1(c58ff3222b5bcd75d58c5f282554e92103e80916) )
	ROM_LOAD( "6.bin", 0x40000, 0x08000, CRC(308f4893) SHA1(539c138ff01c5718cc8a982482b989468d532699) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "8.bin", 0x0000, 0x10000,  CRC(3d9fb623) SHA1(6e5eaad9bb0a432e2da5da5b18a2ed36617bdde2) )

	ROM_REGION( 0x10000, REGION_CPU3, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "2.bin", 0x8000, 0x8000,  CRC(4a471c38) SHA1(963ed7b6afeefdfc2cf0d65b0998f973330e6495) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "1.bin",  0x00000, 0x08000, CRC(815a891a) SHA1(e557d6a35821a8589d9e3df0f42131b58b08c8ca) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "20.bin", 0x00000, 0x10000, CRC(2b7634f2) SHA1(56d963d4960d9b3e888c8107340763e176adfa9b) )
	ROM_LOAD( "19.bin", 0x10000, 0x10000, CRC(4530a952) SHA1(99251a21347815cba465669e18df31262bcdaba1) )
	ROM_LOAD( "18.bin", 0x20000, 0x10000, CRC(87c28833) SHA1(3f1a294065326389d304e540bc880844c6c7cb06) )
	ROM_LOAD( "17.bin", 0x30000, 0x10000, CRC(bfb43a4d) SHA1(56092935147a3b643a9b39eb7cfc067a764644c5) )
	ROM_LOAD( "16.bin", 0x40000, 0x10000, CRC(f9848cc4) SHA1(6d8e77b67ce4d418defba6f6979632f31d2307c6) )
	ROM_LOAD( "15.bin", 0x50000, 0x10000, CRC(baa3d218) SHA1(3c31df23cc871cffd9a4dafae106e4a98f5af848) )
	ROM_LOAD( "14.bin", 0x60000, 0x10000, CRC(12afe533) SHA1(6df3471c16a714d118717da549a7523aa388ddd3) )
	ROM_LOAD( "13.bin", 0x70000, 0x10000, CRC(03373755) SHA1(d2541dd957803168f246d96b7cd74eae7fd43188) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "9.bin",  0x00000, 0x10000, CRC(067e2a43) SHA1(f1da7455aab21f94ed25a93b0ebfde69baa475d1) )
	ROM_LOAD( "10.bin", 0x20000, 0x10000, CRC(c19733aa) SHA1(3dfcfd33c5c4f792bb941ac933301c03ddd72b03) )
	ROM_LOAD( "11.bin", 0x40000, 0x10000, CRC(e37d5dbe) SHA1(ff79b4f6d8b0a3061e78d15480df0155650f347f) )
	ROM_LOAD( "12.bin", 0x60000, 0x10000, CRC(beee880f) SHA1(9a818a75cbec425a13f629bda6d50aa341aa1896) )
ROM_END

ROM_START( csilver )
	ROM_REGION( 0x48000, REGION_CPU1, 0 )
	ROM_LOAD( "a4", 0x08000, 0x08000, CRC(02dd8cfc) SHA1(f29c0d9dd03e8c52672c0f3dbee44a93c5b4261d) )
	ROM_LOAD( "a2", 0x10000, 0x10000, CRC(570fb50c) SHA1(3002f53182834a060fc282be1bc5767906e19ba2) )
	ROM_LOAD( "a3", 0x20000, 0x10000, CRC(58625890) SHA1(503a969085f6dcb16687217c48136ea22d07c89f) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* CPU 2, 1st 16k is empty */
	ROM_LOAD( "a5", 0x0000, 0x10000,  CRC(29432691) SHA1(a76ecd27d217c66a0e43f93e29efe83c657925c3) )

	ROM_REGION( 0x18000, REGION_CPU3, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "a6", 0x10000, 0x08000,  CRC(eb32cf25) SHA1(9390c88033259c65eb15320e31f5d696970987cc) )
	ROM_CONTINUE(   0x08000, 0x08000 )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "a1",  0x00000, 0x08000, CRC(f01ef985) SHA1(d5b823bd7c0efcf3137f8643c5d99a260bed5675) )

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites (3bpp) */
	ROM_LOAD( "b5",  0x00000, 0x10000, CRC(80f07915) SHA1(ea100f12ef3a68110af911fa9beeb73b388f069d) )
	/* 0x10000-0x1ffff empy */
	ROM_LOAD( "b4",  0x20000, 0x10000, CRC(d32c02e7) SHA1(d0518ec31e9e3f7b4e76fba5d7c05c33c61a9c72) )
	/* 0x30000-0x3ffff empy */
	ROM_LOAD( "b3",  0x40000, 0x10000, CRC(ac78b76b) SHA1(c2be347fd950894401123ada8b27bfcfce53e66b) )
	/* 0x50000-0x5ffff empy */
	/* 0x60000-0x7ffff empy (no 4th plane) */

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles (3bpp) */
	ROM_LOAD( "a7",  0x00000, 0x10000, CRC(b6fb208c) SHA1(027d33f0b5feb6f0433134213cfcef96790eaace) )
	ROM_LOAD( "a8",  0x10000, 0x10000, CRC(ee3e1817) SHA1(013496976a9ffacf1587b3a6fc0f548becb1ab0e) )
	ROM_LOAD( "a9",  0x20000, 0x10000, CRC(705900fe) SHA1(53b9d09f9780a3bf3545bc27a2855ebee3884124) )
	ROM_LOAD( "a10", 0x30000, 0x10000, CRC(3192571d) SHA1(240c6c099f1e6edbf0be7d5a4ec396b056c9f70f) )
	ROM_LOAD( "b1",  0x40000, 0x10000, CRC(3ef77a32) SHA1(97b97c35a6ca994d2e7a6e7a63101eda9709bcb1) )
	ROM_LOAD( "b2",  0x50000, 0x10000, CRC(9cf3d5b8) SHA1(df4974f8412ab1cf65871b8e4e3dbee478bf4d21) )
ROM_END

ROM_START( garyoret )
	ROM_REGION( 0x58000, REGION_CPU1, 0 )
	ROM_LOAD( "dv00", 0x08000, 0x08000, CRC(cceaaf05) SHA1(b8f54638feab77d023e01ced947e1269f0d19fd8) )
	ROM_LOAD( "dv01", 0x10000, 0x10000, CRC(c33fc18a) SHA1(0d9594b0e6c39aea5b9f15f6aa364b31604f1066) )
	ROM_LOAD( "dv02", 0x20000, 0x10000, CRC(f9e26ce7) SHA1(8589594ebc7ae16942739382273a222dfa30b3b7) )
	ROM_LOAD( "dv03", 0x30000, 0x10000, CRC(55d8d699) SHA1(da1519cd54d27cc406420ce0845e43f7228cfd4e) )
	ROM_LOAD( "dv04", 0x40000, 0x10000, CRC(ed3d00ee) SHA1(6daa2ee509235ad03d3012a00382820279add620) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 64K for sound CPU */
	ROM_LOAD( "dv05", 0x08000, 0x08000, CRC(c97c347f) SHA1(a1b22733dc15d524af97db3e608a82503a49b182) )

	ROM_REGION( 0x08000, REGION_GFX1, ROMREGION_DISPOSE )	/* characters */
	ROM_LOAD( "dv14", 0x00000, 0x08000, CRC(fb2bc581) SHA1(d597976c5ae586166c49051cc3de8cf0c2e5a5e1) )	/* Characters */

	ROM_REGION( 0x80000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */
	ROM_LOAD( "dv22", 0x00000, 0x10000, CRC(cef0367e) SHA1(8beb3a6b91ec0a6ec052243c8f626a581d349f65) )
	ROM_LOAD( "dv21", 0x10000, 0x08000, CRC(90042fb7) SHA1(f19bbf158c92030e8fddb5087b5b69b71956baf8) )
	ROM_LOAD( "dv20", 0x20000, 0x10000, CRC(451a2d8c) SHA1(f4eea444b797d394edeb514ddc1c494fd7ccc2f2) )
	ROM_LOAD( "dv19", 0x30000, 0x08000, CRC(14e1475b) SHA1(f0aec5b7b4be0da06a73ed382e7e851654e47e47) )
	ROM_LOAD( "dv18", 0x40000, 0x10000, CRC(7043bead) SHA1(5d1be8b9cd56ae43d60406b05258d20de980096d) )
	ROM_LOAD( "dv17", 0x50000, 0x08000, CRC(28f449d7) SHA1(cf1bc690b67910c42ad09531ab1d88461d00b944) )
	ROM_LOAD( "dv16", 0x60000, 0x10000, CRC(37e4971e) SHA1(9442c315b7cdbcc046d9e6838cb793c1857174ed) )
	ROM_LOAD( "dv15", 0x70000, 0x08000, CRC(ca41b6ac) SHA1(d7a9ef6c89741c1e8e17a668a9d39ea089f5c73f) )

	ROM_REGION( 0x80000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles */
	ROM_LOAD( "dv08", 0x00000, 0x08000, CRC(89c13e15) SHA1(6507e60de5cd78a5b46090e4825a44c2a23631d7) )
	ROM_CONTINUE(     0x10000, 0x08000 )
	ROM_LOAD( "dv09", 0x08000, 0x08000, CRC(6a345a23) SHA1(b86f81b9fe25acd833ca3e2cff6cfa853c02280a) )
	ROM_CONTINUE(     0x18000, 0x08000 )

	ROM_LOAD( "dv06", 0x20000, 0x08000, CRC(1eb52a20) SHA1(46670ed973f794be9c2c7e6bf5d97db51211e9a9) )
	ROM_CONTINUE(     0x30000, 0x08000 )
	ROM_LOAD( "dv07", 0x28000, 0x08000, CRC(e7346ef8) SHA1(8083a7a182e8ed904daf2f692115d01b3d0830eb) )
	ROM_CONTINUE(     0x38000, 0x08000 )

	ROM_LOAD( "dv12", 0x40000, 0x08000, CRC(46ba5af4) SHA1(a1c13e7e3c85060202120b64e3cee32c1f733270) )
	ROM_CONTINUE(     0x50000, 0x08000 )
	ROM_LOAD( "dv13", 0x48000, 0x08000, CRC(a7af6dfd) SHA1(fa41bdafb64c79bd9769903fd37d4d5172b66a52) )
	ROM_CONTINUE(     0x58000, 0x08000 )

	ROM_LOAD( "dv10", 0x60000, 0x08000, CRC(68b6d75c) SHA1(ac719ef6b30ac9e63fab13cb359e6114493f274d) )
	ROM_CONTINUE(     0x70000, 0x08000 )
	ROM_LOAD( "dv11", 0x68000, 0x08000, CRC(b5948aee) SHA1(587afbfeda985bede9e4b5f57dad6763f4294962) )
	ROM_CONTINUE(     0x78000, 0x08000 )
ROM_END

/******************************************************************************/

/* Ghostbusters, Darwin, Oscar use a "Deco 222" custom 6502 for sound. */
static DRIVER_INIT( deco222 )
{
	int A,sound_cpu,diff;
	unsigned char *rom;

	sound_cpu = 1;
	/* Oscar has three CPUs */
	if (Machine->drv->cpu[2].cpu_type != 0) sound_cpu = 2;

	/* bits 5 and 6 of the opcodes are swapped */
	rom = memory_region(REGION_CPU1+sound_cpu);
	diff = memory_region_length(REGION_CPU1+sound_cpu) / 2;

	memory_set_opcode_base(sound_cpu,rom+diff);

	for (A = 0;A < 0x10000;A++)
		rom[A + diff] = (rom[A] & 0x9f) | ((rom[A] & 0x20) << 1) | ((rom[A] & 0x40) >> 1);
}

static DRIVER_INIT( meikyuh )
{
	/* Blank out unused garbage in colour prom to avoid colour overflow */
	unsigned char *RAM = memory_region(REGION_PROMS);
	memset(RAM+0x20,0,0xe0);
}

static DRIVER_INIT( ghostb )
{
	init_deco222();
	init_meikyuh();
}

/******************************************************************************/

GAME(1988, cobracom, 0,        cobracom, cobracom, 0,       ROT0,   "Data East Corporation", "Cobra-Command (World revision 5)" )
GAME(1988, cobracmj, cobracom, cobracom, cobracom, 0,       ROT0,   "Data East Corporation", "Cobra-Command (Japan)" )
GAME(1987, ghostb,   0,        ghostb,   ghostb,   ghostb,  ROT0,   "Data East USA", "The Real Ghostbusters (US 2 Players)" )
GAME(1987, ghostb3,  ghostb,   ghostb,   ghostb,   ghostb,  ROT0,   "Data East USA", "The Real Ghostbusters (US 3 Players)" )
GAME(1987, meikyuh,  ghostb,   ghostb,   meikyuh,  meikyuh, ROT0,   "Data East Corporation", "Meikyuu Hunter G (Japan)" )
GAME(1987, srdarwin, 0,        srdarwin, srdarwin, deco222, ROT270, "Data East Corporation", "Super Real Darwin (World)" )
GAME(1987, srdarwnj, srdarwin, srdarwin, srdarwin, deco222, ROT270, "Data East Corporation", "Super Real Darwin (Japan)" )
GAME(1987, gondo,    0,        gondo,    gondo,    0,       ROT270, "Data East USA", "Gondomania (US)" )
GAME(1987, makyosen, gondo,    gondo,    gondo,    0,       ROT270, "Data East Corporation", "Makyou Senshi (Japan)" )
GAME(1988, oscar,    0,        oscar,    oscar,    deco222, ROT0,   "Data East USA", "Psycho-Nics Oscar (US)" )
GAME(1987, oscarj,   oscar,    oscar,    oscar,    deco222, ROT0,   "Data East Corporation", "Psycho-Nics Oscar (Japan revision 2)" )
GAME(1987, oscarj1,  oscar,    oscar,    oscar,    deco222, ROT0,   "Data East Corporation", "Psycho-Nics Oscar (Japan revision 1)" )
GAME(1987, oscarj0,  oscar,    oscar,    oscar,    deco222, ROT0,   "Data East Corporation", "Psycho-Nics Oscar (Japan revision 0)" )
GAME(1986, lastmisn, 0,        lastmiss, lastmisn, 0,       ROT270, "Data East USA", "Last Mission (US revision 6)" )
GAME(1986, lastmsno, lastmisn, lastmiss, lastmisn, 0,       ROT270, "Data East USA", "Last Mission (US revision 5)" )
GAME(1986, lastmsnj, lastmisn, lastmiss, lastmsnj, 0,       ROT270, "Data East Corporation", "Last Mission (Japan)" )
GAME(1986, shackled, 0,        shackled, shackled, 0,       ROT0,   "Data East USA", "Shackled (US)" )
GAME(1986, breywood, shackled, shackled, shackled, 0,       ROT0,   "Data East Corporation", "Breywood (Japan revision 2)" )
GAME(1987, csilver,  0,        csilver,  csilver,  0,       ROT0,   "Data East Corporation", "Captain Silver (Japan)" )
GAME(1987, garyoret, 0,        garyoret, garyoret, 0,       ROT0,   "Data East Corporation", "Garyo Retsuden (Japan)" )
