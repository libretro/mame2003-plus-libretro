/*
DJ Boy (c)1989 Kanako

Hardware has many similarities to Airbusters.

You must manually reset (F3) after booting up to proceed.
There's likely an unemulated hardware watchdog.

Video hardware and sound should both be mostly correct.
There's still some missing protection and/or exotic CPU communication.

Self Test:
press button#3 to advance past color pattern
buttons 1,2,3 are used to select and play sound/music


- CPU1 manages sprites, which are also used to display text
		irq (0x10) - timing/watchdog
		irq (0x30) - processes sprites
		nmi: wakes up this cpu

- CPU2 manages the protection device, palette, and tilemap(s)
		nmi: resets this cpu
		irq: game update
		additional protection at d8xx?

- CPU3 manages sound chips
		irq: update music
		nmi: handle sound command

	The protection device provides an API to poll dipswitches and inputs.
	It is probably involved with the memory range 0xd800..0xd8ff, which CPU2 reads.
	It handles coin input and coinage internally.
	The real game shouts "DJ Boy!" every time a credit is inserted.


Genre: Scrolling Fighter
Orientation: Horizontal
Type: Raster: Standard Resolution
CRT: Color
Conversion Class: JAMMA
Number of Simultaneous Players: 2
Maximum number of Players: 2
Gameplay: Joint
Control Panel Layout: Multiple Player
Joystick: 8-way
Buttons: 3 - Punch, Kick, Jump
Sound: Amplified Mono (one channel) - Stereo sound is available
through a 4-pin header (voice of Wolfman Jack!!)


                     BS-65  6116
                     BS-101 6116
6264                 780C-2
BS-005
BS-004               6264
16mhz
12mhz                                   beast
        41101
BS-003                BS-203    6295    2203
BS-000  pandora       780C-2    6295
BS-001
BS-002  4464 4464     BS-100          6264
BS07    4464 4464     BS-64           BS-200
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

/* public functions from vidhrdw/djboy.h */
extern void djboy_set_videoreg( data8_t data );
extern WRITE_HANDLER( djboy_scrollx_w );
extern WRITE_HANDLER( djboy_scrolly_w );
extern WRITE_HANDLER( djboy_videoram_w );
extern WRITE_HANDLER( djboy_paletteram_w );
extern VIDEO_START( djboy );
extern VIDEO_UPDATE( djboy );

static data8_t *sharedram;
static READ_HANDLER( sharedram_r )	{ return sharedram[offset]; }
static WRITE_HANDLER( sharedram_w )	{ sharedram[offset] = data; }

static int prot_offs;
static data8_t prot_ram[0x80];

/******************************************************************************/

static WRITE_HANDLER( cpu1_cause_nmi_w )
{
	cpu_set_irq_line(0, IRQ_LINE_NMI, PULSE_LINE);
}

static WRITE_HANDLER( cpu1_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	logerror( "cpu1_bankswitch( 0x%02x )\n", data );

	if( data < 4 )
	{
		RAM = &RAM[0x2000 * data];
	}
	else
	{
		RAM = &RAM[0x10000 + 0x2000 * (data-4)];
	}
	cpu_setbank(1,RAM);
}

/******************************************************************************/

static WRITE_HANDLER( cpu2_bankswitch_w )
{
	data8_t *RAM = memory_region(REGION_CPU2);
	
	djboy_set_videoreg( data );

	switch( data&0xf )
	{
	/* bs65.5y */
	case 0x00: cpu_setbank(2,&RAM[0x00000]); break;
	case 0x01: cpu_setbank(2,&RAM[0x04000]); break;
	case 0x02: cpu_setbank(2,&RAM[0x10000]); break;
	case 0x03: cpu_setbank(2,&RAM[0x14000]); break;

	/* bs101.6w */
	case 0x08: cpu_setbank(2,&RAM[0x18000]); break;
	case 0x09: cpu_setbank(2,&RAM[0x1c000]); break;
	case 0x0a: cpu_setbank(2,&RAM[0x20000]); break;
	case 0x0b: cpu_setbank(2,&RAM[0x24000]); break;
	case 0x0c: cpu_setbank(2,&RAM[0x28000]); break;
	case 0x0d: cpu_setbank(2,&RAM[0x2c000]); break;
	case 0x0e: cpu_setbank(2,&RAM[0x30000]); break;
	case 0x0f: cpu_setbank(2,&RAM[0x34000]); break;

	default:
		break;
	}
}

/**
 * HACK: fake out port behavior using the addresses at which reads/writes occur
 *
 * Once the behavior is fully understood, we can emulate this at the chip level,
 * rather than tying the behavior to specific addresses.
 */

static WRITE_HANDLER( cpu2_data_w )
{
	switch( activecpu_get_pc() )
	{
	case 0x7987: /* 0x03 memtest write */
		prot_offs = 0;
		break;
	
	case 0x73a5: /* 1 is written; preceeds protection read */
		return;

	case 0x79ba: /* 0x00 preceeds each byte-write */
		return;

	case 0x79cd: /* 0x00,0xff,0x55,0xaa,0x92,0xd9,0x3d,... */
		prot_ram[(prot_offs++)&0x7f] = data;
		return;

	case 0x79ee: /* 0xfe memtest read */
		prot_offs = 0;
		break;

	case 0x7a06: /* 0x00 preceeds each byte-read */
		return;

	case 0x7a3f: /* 0xfe memtest write */
		prot_offs = 0;
		return;

	case 0x7a4d: /* 0xff read-DSW */
		break;

	case 0x7a78: /* write back DSW1&0xfe */
		break;

	case 0x7a95: /* write back DSW2 */
		break;

	case 0x7aad: /* store lives per credit */
		break;

	case 0x7ad4: /* store coinage */
		break;

	case 0x7aeb: /* 0x02 (?) reset I/O? */
		break;

	case 0x726a: /* 0x08 (?) protection */
		break;
	
	case 0x7146: break; /* prot(0x01) */
	case 0x71f4: break; /* prot(0x02) */

	default:
		logerror( "?" );
	}
	logerror( "pc == %04x; data_w(%02x)\n", activecpu_get_pc(), data );
} /* cpu2_data_w */

static READ_HANDLER( cpu2_data_r )
{
	data8_t result = 0x00;
	static int which;

	switch( activecpu_get_pc() )
	{
	case 0x7a16:
		result = prot_ram[(prot_offs++)&0x7f];
		return result;
		break;

	case 0x7a5e:
	case 0x72aa:
		result = readinputport(3); /* DSW1 (ix+$44) */
		break;

	case 0x7a88:
	case 0x72bd:
		result = readinputport(4); /* DSW2 (ix+$45) */
		break;

	case 0x731a:
	case 0x727a: /* (ix+$41) */
		which = 0;
		result = readinputport(0);
		break;

	case 0x7296: /* (ix+$42),(ix+$43) */
		result = readinputport(1+which);
		which = 1-which;
		break;

	case 0x7307: /* (ix+$40) credits */
		result = 1;
		break;

	case 0x73b5:
		/**
		 * used to poll for "events"
		 * possible values include:
		 * 0x00, 0x01, 0x80,0x81,...0x8e
		 *
		 * Each value dispatches to a different routine.  Most of them do very little.
		 */
		result = 0x82; // 'normal' - polls inputs
		if( keyboard_pressed( KEYCODE_Q ) ) result = 0;
		if( keyboard_pressed( KEYCODE_5 ) ) result = 1; /* "PUSH 1P START" */
		if( keyboard_pressed( KEYCODE_6 ) ) result = 0x8b; /* "COIN ERROR" */
//		if( keyboard_pressed( KEYCODE_B ) ) result = 0x8e;
		return result;

	case 0x7204: result = readinputport(1); break; /* (ix+$42) */
	case 0x7217: result = 0; break;
	case 0x7227: result = readinputport(2); break; /* (ix+$43) */
	case 0x723a: result = 0; break;
	case 0x724a: result = 0; break;

	default:
		break;
	}
	logerror( "pc == %04x; data_r() == 0x%02x\n", activecpu_get_pc(), result );
	return result;
} /* cpu2_data_r */

static READ_HANDLER( cpu2_status_r )
{
	switch( activecpu_get_pc() )
	{
	case 0x27b3: return 0;//!0x02
	case 0x27c5: return 0;//!0x02

	case 0x28e9: return 0; /* unknown */

	case 0x31cc: return 0;//!0x02

	case 0x703f: return 1<<2;
	case 0x70d0: return 1<<2;
	case 0x70f0: return 1<<2; /* 0x0c */

	case 0x7110: return 1<<2;
	case 0x7130: return 1<<2; /* prot(0x01) */
	case 0x7150: return 1<<2;
	case 0x7170: return 1<<2;
	case 0x718f: return 0; /* !0x04 */
	case 0x71a4: return 1<<2;
	case 0x71c3: return 0; /* !0x04 */
	case 0x71de: return 1<<2; /* prot(0x02) */
	case 0x71fb: return 0; /* prot(0x02) !0x04 */

	case 0x720e: return 0; /* prot(0x02) !0x04 */
	case 0x721e: return 0; /* prot(0x02) !0x04 */
	case 0x7231: return 0; /* prot(0x02) !0x04 */
	case 0x7241: return 0; /* prot(0x02) !0x04 */
	case 0x7254: return 1<<2;
	case 0x7271: return 0;//!0x04
	case 0x728d: return 0;//!0x04
	case 0x72a1: return 0;//!0x04
	case 0x72b4: return 0;//!0x04
	case 0x72db: return 1<<2;
	case 0x72fe: return 0;//!0x04
	
	case 0x7311: return 0;//!0x04
	case 0x738f: return 1<<2;
	case 0x73ac: return 0;//!0x04

	case 0x7971: return 1<<2;
	case 0x798e: return 0;//!0x08
	case 0x79af: return 0;//!0x08
	case 0x79c1: return 0;//!0x08
	case 0x79e1: return 0;//!0x08
	case 0x79fb: return 0;//!0x08

	case 0x7a0d: return 0;//!0x04
	case 0x7a2e: return 0;//!0x08
	case 0x7a55: return 0;//!0x04
	case 0x7a68: return 0;//!0x08
	case 0x7a7f: return 0;//!0x04
	case 0x7aa1: return 0;//!0x08
	case 0x7ac8: return 0;//!0x08
	case 0x7ade: return 0;//!0x08

	default: break;
	}
	logerror( "pc == %04x; status_r\n", activecpu_get_pc() );
	return 0x02|(rand()&0x0c);
}

/******************************************************************************/

static WRITE_HANDLER( cpu3_nmi_soundcommand_w )
{
	soundlatch_w(0,data);
	cpu_set_irq_line(2, IRQ_LINE_NMI, PULSE_LINE);
}

static WRITE_HANDLER( cpu3_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU3);

	if( data<3 )
	{
		RAM = &RAM[0x04000 * data];
	}
	else
	{
		RAM = &RAM[0x10000 + 0x4000*(data-3)];
	}
	cpu_setbank(3,RAM);
}

/******************************************************************************/

static MEMORY_READ_START( cpu1_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xb000, 0xbfff, MRA_RAM }, /* spriteram */
	{ 0xc000, 0xdfff, MRA_BANK1 },
	{ 0xe000, 0xefff, MRA_RAM }, /* shareram */
	{ 0xf000, 0xffff, MRA_RAM },
MEMORY_END


static MEMORY_WRITE_START( cpu1_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xb000, 0xbfff, MWA_RAM, &spriteram },
	{ 0xc000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xffff, MWA_RAM, &sharedram },
MEMORY_END

static PORT_WRITE_START( cpu1_writeport )
	{ 0x00, 0x00, cpu1_bankswitch_w },
PORT_END

/******************************************************************************/

static MEMORY_READ_START( cpu2_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK2 },
	{ 0xc000, 0xcfff, MRA_RAM }, /* videoram */
	{ 0xd000, 0xd3ff, MRA_RAM }, /* paletteram */
	{ 0xd400, 0xd7ff, MRA_RAM }, /* workram */
	/* { 0xd800, 0xd8ff, MRA_RAM }, */ /* protection? */
	{ 0xe000, 0xffff, sharedram_r },
MEMORY_END

static MEMORY_WRITE_START( cpu2_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, djboy_videoram_w, &videoram },
	{ 0xd000, 0xd3ff, djboy_paletteram_w, &paletteram },
	{ 0xd400, 0xd7ff, MWA_RAM }, /* workram */
	/* { 0xd800, 0xd8ff, MWA_RAM }, */ /* protection? */
	{ 0xe000, 0xffff, sharedram_w },
MEMORY_END

static PORT_READ_START( readport2 )
	{ 0x04, 0x04, cpu2_data_r },
	{ 0x0c, 0x0c, cpu2_status_r },
PORT_END

static PORT_WRITE_START( writeport2 )
	{ 0x00, 0x00, cpu2_bankswitch_w },
	{ 0x02, 0x02, cpu3_nmi_soundcommand_w },
	{ 0x04, 0x04, cpu2_data_w },
	{ 0x0a, 0x0a, cpu1_cause_nmi_w },
	{ 0x06, 0x06, djboy_scrolly_w },
	{ 0x08, 0x08, djboy_scrollx_w },
PORT_END

/******************************************************************************/

static MEMORY_READ_START( cpu3_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK3 },
	{ 0xc000, 0xdfff, MRA_RAM },
MEMORY_END


static MEMORY_WRITE_START( cpu3_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
MEMORY_END

static PORT_READ_START( cpu3_readport )
	{ 0x02, 0x02, YM2203_status_port_0_r },
	{ 0x03, 0x03, YM2203_read_port_0_r },
	{ 0x04, 0x04, soundlatch_r },
	{ 0x06, 0x06, OKIM6295_status_0_r },
	{ 0x07, 0x07, OKIM6295_status_1_r },
PORT_END

static PORT_WRITE_START( cpu3_writeport )
	{ 0x00, 0x00, cpu3_bankswitch_w },
	{ 0x02, 0x02, YM2203_control_port_0_w },
	{ 0x03, 0x03, YM2203_write_port_0_w },
	{ 0x06, 0x06, OKIM6295_data_0_w },
	{ 0x07, 0x07, OKIM6295_data_1_w },
PORT_END

/******************************************************************************/

static struct GfxLayout tile_layout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{
		0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4,
		8*32+0*4,8*32+1*4,8*32+2*4,8*32+3*4,8*32+4*4,8*32+5*4,8*32+6*4,8*32+7*4
	},
	{
		0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32,
		16*32+0*32,16*32+1*32,16*32+2*32,16*32+3*32,16*32+4*32,16*32+5*32,16*32+6*32,16*32+7*32
	},
	4*8*32
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tile_layout, 0x000, 16 }, /* foreground tiles? */
	{ REGION_GFX2, 0, &tile_layout, 0x100, 16 }, /* sprite */
	{ REGION_GFX3, 0, &tile_layout, 0x000, 16 }, /* background tiles */
	{ -1 }
};

/******************************************************************************/

static struct YM2203interface ym2203_interface =
{
	1,
	3000000, /* ? */
	{ YM2203_VOL(0xff,0xff) },	/* gain,volume */
	{ 0 },	/* port A read */
	{ 0 }, /* port B read */
	{ 0 }, /* port A write */
	{ 0 }, /* port B write */
	{ 0 } /* IRQ handler for YM2203 */
};

static struct OKIM6295interface okim6295_interface =
{
	2,
	{ 12000000/4/165,12000000/4/165 }, /* ? */
	{ REGION_SOUND1,REGION_SOUND1 },
	{ 50,50 }
};

/******************************************************************************/

static INTERRUPT_GEN( djboy_interrupt )
{
	/* CPU1 uses interrupt mode 2.
	 * For now, just alternate the two interrupts.  It isn't known what triggers them
	 */
	static int addr = 0xff;
	addr ^= 0x02;
	cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, addr);
}

static MACHINE_DRIVER_START( djboy )
	MDRV_CPU_ADD(Z80,6000000) /* ? */
	MDRV_CPU_MEMORY(cpu1_readmem,cpu1_writemem)
	MDRV_CPU_PORTS(0,cpu1_writeport)
	MDRV_CPU_VBLANK_INT(djboy_interrupt,2)

	MDRV_CPU_ADD(Z80,6000000) /* ? */
	MDRV_CPU_MEMORY(cpu2_readmem,cpu2_writemem)
	MDRV_CPU_PORTS(readport2,writeport2)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(Z80, 6000000) /* ? */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(cpu3_readmem,cpu3_writemem)
	MDRV_CPU_PORTS(cpu3_readport,cpu3_writeport)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(100)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER )
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 256-1, 16, 256-16-1)
	MDRV_GFXDECODE(gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(0x200)

	MDRV_VIDEO_START(djboy)
	MDRV_VIDEO_UPDATE(djboy)

	MDRV_SOUND_ADD(YM2203, ym2203_interface)
	MDRV_SOUND_ADD(OKIM6295, okim6295_interface)
MACHINE_DRIVER_END

ROM_START( djboy )
	ROM_REGION( 0x48000, REGION_CPU1, 0 )
	ROM_LOAD( "bs64.4b",  0x00000, 0x08000, CRC(b77aacc7) SHA1(78100d4695738a702f13807526eb1bcac759cce3) )
	ROM_CONTINUE( 0x10000, 0x18000 )
	ROM_LOAD( "bs100.4d", 0x28000, 0x20000, CRC(081e8af8) SHA1(3589dab1cf31b109a40370b4db1f31785023e2ed) )

	ROM_REGION( 0x38000, REGION_CPU2, 0 )
	ROM_LOAD( "bs65.5y",  0x00000, 0x08000, CRC(0f1456eb) SHA1(62ed48c0d71c1fabbb3f6ada60381f57f692cef8) )
	ROM_CONTINUE( 0x10000, 0x08000 )
	ROM_LOAD( "bs101.6w", 0x18000, 0x20000, CRC(a7c85577) SHA1(8296b96d5f69f6c730b7ed77fa8c93496b33529c) )

	ROM_REGION( 0x24000, REGION_CPU3, 0 ) /* sound */
	ROM_LOAD( "bs200.8c", 0x00000, 0x0c000, CRC(f6c19e51) SHA1(82193f71122df07cce0a7f057a87b89eb2d587a1) )
	ROM_CONTINUE( 0x10000, 0x14000 )

	ROM_REGION( 0x10000, REGION_GFX1, 0 ) /* foreground tiles?  alt sprite bank? */
	ROM_LOAD( "bs07.1b", 0x000000, 0x10000, CRC(d9b7a220) SHA1(ba3b528d50650c209c986268bb29b42ff1276eb2) )

	ROM_REGION( 0x200000, REGION_GFX2, 0 ) /* sprites */
	ROM_LOAD( "bs000.1h", 0x000000, 0x80000, CRC(be4bf805) SHA1(a73c564575fe89d26225ca8ec2d98b6ac319ac18) )
	ROM_LOAD( "bs001.1f", 0x080000, 0x80000, CRC(fdf36e6b) SHA1(a8762458dfd5201304247c113ceb85e96e33d423) )
	ROM_LOAD( "bs002.1d", 0x100000, 0x80000, CRC(c52fee7f) SHA1(bd33117f7a57899fd4ec0a77413107edd9c44629) )
	ROM_LOAD( "bs003.1k", 0x180000, 0x80000, CRC(ed89acb4) SHA1(611af362606b73cd2cf501678b463db52dcf69c4) )

	ROM_REGION( 0x100000, REGION_GFX3, 0 ) /* background */
	ROM_LOAD( "bs004.1s", 0x000000, 0x80000, CRC(2f1392c3) SHA1(1bc3030b3612766a02133eef0b4d20013c0495a4) )
	ROM_LOAD( "bs005.1u", 0x080000, 0x80000, CRC(46b400c4) SHA1(35f4823364bbff1fc935994498d462bbd3bc6044) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 ) /* OKI-M6295 samples */
	ROM_LOAD( "bs203.5j", 0x000000, 0x40000, CRC(805341fb) SHA1(fb94e400e2283aaa806814d5a39d6196457dc822) )
ROM_END

INPUT_PORTS_START( djboy )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* labeled "TEST" in self test */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) /* punch */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) /* kick */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 ) /* jump */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Service_Mode ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) ) /* coin mode? */
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, "Bonus" )
	PORT_DIPSETTING(    0x0c, "10k,30k,50k,70k,90k" )
	PORT_DIPSETTING(    0x08, "10k,20k,30k,40k,50k,60k,70k,80k,90k" )
	PORT_DIPSETTING(    0x04, "20k,50k" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x30, "5" )
	PORT_DIPSETTING(    0x10, "7" )
	PORT_DIPSETTING(    0x00, "9" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Stero Sound" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/*     YEAR, NAME,  PARENT, MACHINE, INPUT, INIT, MNTR,  COMPANY, FULLNAME, FLAGS */
GAMEX( 1989, djboy, 0,      djboy,   djboy, 0,    ROT0, "Kaneko", "DJ Boy", GAME_NOT_WORKING )
