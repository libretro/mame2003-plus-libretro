/* Tecmo System
 Driver by Farfetch & David Haywood

can't do anything with this, its protected and expects to read back 68k code :-(

*/

/*

Deroon DeroDero
(c)1996 Tecmo
Tecmo System Board

CPU  : TMP68HC000P-16
Sound: TMPZ84C00AP-8 YMF262 YMZ280B M6295
OSC  : 14.3181MHz (X1) 28.0000MHz (X2) 16.0000MHz (X3) 16.9MHz (X4)

Custom chips:
TECMO AA02-1927 (160pin PQFP) (x4)
TECMO AA03-8431 (208pin PQFP) (x4)

Others:
93C46 EEPROM (settings are stored to this)

EPROMs:
t001upau.bin - Main program (even) (27c4001)
t002upal.bin - Main program (odd)  (27c4001)

t003uz1.bin - Sound program (27c2001)

Mask ROMs:
t101uah1.j66 - Graphics (23c16000 SOP)
t102ual1.j67 |
t103ubl1.j08 |
t104ucl1.j68 /

t201ubb1.w61 - Graphics (23c8000)
t202ubc1.w62 /

t301ubd1.w63 - Graphics (23c8000)

t401uya1.w16 - YMZ280B Samples (23c16000)

t501uad1.w01 - M6295 Samples (23c4001)

*/

/*

Touki Denshou -Angel Eyes-
(c)1996 Tecmo
Tecmo System Board

CPU  : TMP68HC000P-16
Sound: TMPZ84C00AP-8 YMF262 YMZ280B M6295
OSC  : 14.3181MHz (X1) 28.0000MHz (X2) 16.0000MHz (X3) 16.9MHz (X4)

Custom chips:
TECMO AA02-1927 (160pin PQFP) (x4)
TECMO AA03-8431 (208pin PQFP) (x4)

Others:
93C46 EEPROM (settings are stored to this)

EPROMs:
aeprge-2.pal - Main program (even) (27c4001)
aeprgo-2.pau - Main program (odd)  (27c4001)

aesprg-2.z1 - Sound program (27c1001)

Mask ROMs:
ae100h.ah1 - Graphics (23c32000/16000 SOP)
ae100.al1  |
ae101h.bh1 |
ae101.bl1  |
ae102h.ch1 |
ae102.cl1  |
ae104.el1  |
ae105.fl1  |
ae106.gl1  /

ae200w74.ba1 - Graphics (23c16000)
ae201w75.bb1 |
ae202w76.bc1 /

ae300w36.bd1 - Graphics (23c4000)

ae400t23.ya1 - YMZ280B Samples (23c16000)
ae401t24.yb1 /

ae500w07.ad1 - M6295 Samples (23c4001)

*/

#include "driver.h"


static data16_t* protram;

static UINT8 device[0x10000];
static UINT32 device_read_ptr = 0;
static UINT32 device_write_ptr = 0;

enum DEV_STATUS
{
	DS_CMD,
	DS_WRITE,
	DS_WRITE_ACK,
	DS_READ,
	DS_READ_ACK
};

static UINT8 device_status = DS_CMD;

static READ16_HANDLER(reg_f80000_r)
{
	UINT16 dt;

	/* 0 means ok, no errors. -1 means error*/
	if (device_status == DS_CMD)
		return 0;

	if (device_status == DS_WRITE_ACK)
	{
		/* Notice, this is the maximum. I think the device lets 68k just writes 4/5 bytes,*/
		/* they contain "LUNA". Then, it starts sending to the 68k a bunch of stuff, including*/
		/* 68k code.*/
		if (device_write_ptr == 0x10000)
		{
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "DEVICE write finished\n");*/
			device_status = DS_READ_ACK;
			device_write_ptr = 0;
			device_read_ptr = 0;
		}
		else
			device_status = DS_WRITE;

		return 0;
	}

	if (device_status == DS_WRITE)
	{
		log_cb(RETRO_LOG_DEBUG, LOGPRE "UNEXPECTED read DS_WRITE (write ptr %x)\n", device_write_ptr);
		return 0;
	}


	if (device_status == DS_READ_ACK)
	{
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "Read ACK\n");*/
		device_status = DS_READ;
		return 0;
	}

	dt = device[device_read_ptr];

/*	log_cb(RETRO_LOG_DEBUG, LOGPRE "DEVICE read %x: %x (at %x)\n", device_read_ptr, dt, cpunum_get_pc(0));*/

	device_read_ptr++;
	device_read_ptr &= 0xFFFF;

	device_status = DS_READ_ACK;

	return dt<<8;
}

/* Write 0x13*/
/* Read something (acknowledge? If -1, write -1 and restart)*/
/* Write data*/
/* Read value (!=1 is ok)*/

static READ16_HANDLER(reg_b80000_r)
{
	if (ACCESSING_MSB)
	{
		/* Bit 7: 0 = ready to write*/
		/* Bit 6: 0 = ready to read*/
		return 0;
	}

	return 0;
}

static WRITE16_HANDLER(reg_e80000_w)
{
	/* Only LSB*/
	data >>= 8;

	if (device_status == DS_CMD)
	{
		switch (data)
		{
		case 0x13:
/*			log_cb(RETRO_LOG_DEBUG, LOGPRE "DEVICE mode WRITE (cmd 0x13)\n");*/
			device_status = DS_WRITE;
			device_write_ptr = 0;
			break;
		}

		return;
	}

	/* @@@ Should skip the writes while in read mode?*/
	if (device_status == DS_READ || device_status == DS_READ_ACK)
	{
/*		log_cb(RETRO_LOG_DEBUG, LOGPRE "EEPROM write %x: %x\n", device_write_ptr, data);*/
		return;
	}

	device[device_write_ptr] = (UINT8)data;
	device_write_ptr++;
	device_status = DS_WRITE_ACK;

}

static MEMORY_READ16_START( readmem )
	{ 0x000000, 0x0fffff, MRA16_ROM },
	{ 0x200000, 0x20ffff, MRA16_RAM },
	{ 0x300000, 0x3013ff, MRA16_RAM },
	{ 0x400000, 0x4013ff, MRA16_RAM },
	{ 0x500000, 0x5013ff, MRA16_RAM },
	{ 0x700000, 0x703fff, MRA16_RAM },
	{ 0x800000, 0x80ffff, MRA16_RAM },
	{ 0x900000, 0x907fff, MRA16_RAM },
	{ 0x980000, 0x980fff, MRA16_RAM },

	{ 0xb80000, 0xb80001, reg_b80000_r },
	{ 0xf80000, 0xf80001, reg_f80000_r },

MEMORY_END

static MEMORY_WRITE16_START( writemem )
	{ 0x000000, 0x0fffff, MWA16_ROM },
	{ 0x200000, 0x20ffff, MWA16_RAM, &protram  },
	{ 0x300000, 0x3013ff, MWA16_RAM },
	{ 0x400000, 0x4013ff, MWA16_RAM },
	{ 0x500000, 0x5013ff, MWA16_RAM },
	{ 0x700000, 0x703fff, MWA16_RAM },
	{ 0x800000, 0x80ffff, MWA16_RAM },
	{ 0x900000, 0x907fff, MWA16_RAM },
	{ 0x980000, 0x980fff, MWA16_RAM },

{0x880022, 0x880023, MWA16_NOP },

	{ 0xe80000, 0xe80001, reg_e80000_w },
MEMORY_END

INPUT_PORTS_START( deroon )
INPUT_PORTS_END

/*
static struct GfxLayout tecmosys_charlayout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0,1,2,3 },
	{ 0, 4, 8, 12, 16, 20, 24, 28 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	8*32
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tecmosys_charlayout,   0, 1  },
	{ -1 }
};
*/




static WRITE_HANDLER( deroon_bankswitch_w )
{
	cpu_setbank( 1, memory_region(REGION_CPU2) + ((data-2) & 0x0f) * 0x4000 + 0x10000 );
}

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xe000, 0xf7ff, MRA_RAM },

MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xe000, 0xf7ff, MWA_RAM },
MEMORY_END



static PORT_READ_START( readport )
	{ 0x00, 0x00, YMF262_status_0_r },
	{ 0x40, 0x40, soundlatch_r },
	/*{ 0x60, 0x60, YMZ280B_status_0_r },*/
PORT_END

static PORT_WRITE_START( writeport )
	{ 0x00, 0x00, YMF262_register_A_0_w },
	{ 0x01, 0x01, YMF262_data_A_0_w },
	{ 0x02, 0x02, YMF262_register_B_0_w },
	{ 0x03, 0x03, YMF262_data_B_0_w },

	{ 0x10, 0x10, OKIM6295_data_0_w },

	{ 0x30, 0x30, deroon_bankswitch_w },

	/*{ 0x50, 0x50, to_main_cpu_latch_w },*/
	{ 0x50, 0x50, IOWP_NOP },

	{ 0x60, 0x60, YMZ280B_register_0_w },
	{ 0x61, 0x61, YMZ280B_data_0_w },
PORT_END



VIDEO_START(deroon)
{
return 0;
}


static int command_data=0;

VIDEO_UPDATE(deroon)
{
/* nothing  - just simulate sound commands writes here ... to test OPL3 emulator */

	int j,trueorientation;
	char buf[64];

	if (keyboard_pressed_memory(KEYCODE_Q))
	{
		command_data++;
	}
	if (keyboard_pressed_memory(KEYCODE_A))
	{
		command_data--;
	}
	command_data &= 0xff;



	trueorientation = Machine->orientation;
	Machine->orientation = ROT0;

	sprintf(buf,"keys: Q,A and C");
	for (j = 0;j < 16;j++)
		drawgfx(bitmap,Machine->uifont,buf[j],0,0,0,10+6*j,20,0,TRANSPARENCY_NONE,0);
	sprintf(buf,"command code: %2x", command_data);
	for (j = 0;j < 16;j++)
		drawgfx(bitmap,Machine->uifont,buf[j],0,0,0,10+6*j,40,0,TRANSPARENCY_NONE,0);

	Machine->orientation = trueorientation;


	if (keyboard_pressed_memory(KEYCODE_C))
	{
		soundlatch_w(0,command_data);
		cpu_set_irq_line(1, IRQ_LINE_NMI, PULSE_LINE);
		usrintf_showmessage("command write=%2x",command_data);
	}
}

/*
>>> Richard wrote:
> Here's the sound info (I got it playing in M1, I
> didn't bother "porting" it since the main game doesn't
> even boot).
>
> memory map:
> 0000-7fff: fixed program ROM
> 8000-bfff: banked ROM
> e000-f7ff: work RAM
>
> I/O ports:

> 0-3: YMF262 OPL3
> 0x10: OKIM6295
> 0x30: bank select, in 0x4000 byte units based at the
> start of the ROM (so 2 = 0x8000).
> 0x40: latch from 68000
> 0x50: latch to 68000
> 0x60/0x61: YMZ280B
>
> IRQ from YMF262 goes to Z80 IRQ.
>
> NMI is asserted when the 68000 writes a command.
>
> Z80 clock appears to be 8 MHz (music slows down in
> "intense" sections if it's 4 MHz, and the crystals are
> all in the area of 16 MHz).
>
> The YMZ280B samples for both games may be misdumped,
> deroon has lots of "bad" noises but tkdensho only has
> a few.
*/


static void sound_irq(int irq)
{
	/* IRQ */
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YMF262interface ymf262_interface =
{
	1,					/* 1 chip */
	14318180,			/* X1 ? */
	{ YAC512_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },	/* channels A and B */
	{ YAC512_VOL(100,MIXER_PAN_LEFT,100,MIXER_PAN_RIGHT) },	/* channels C and D */
	{ sound_irq },		/* irq */
};


static struct OKIM6295interface okim6295_interface =
{
	1,					/* 1 chip */
	{ 14318180/2048 },	/* = 6991 Hz ? */
	{ REGION_SOUND2 },
	{ 50 }
};

static struct YMZ280Binterface ymz280b_interface =
{
	1,					/* 1 chip */
	{ 16900000 },		/* X4 ? */
	{ REGION_SOUND1 },
	{ YM3012_VOL(30,MIXER_PAN_LEFT,30,MIXER_PAN_RIGHT) },
	{ 0 }	/* irq */
};

static MACHINE_DRIVER_START( deroon )
	MDRV_CPU_ADD(M68000, 16000000/8) /* the /8 divider is here only for OPL3 testing */
	MDRV_CPU_MEMORY(readmem,writemem)
	/*MDRV_CPU_VBLANK_INT(irq1_line_hold,1)*/

	MDRV_CPU_ADD(Z80, 16000000/2 )	/* 8 MHz ??? */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(readport,writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

/*	MDRV_GFXDECODE(gfxdecodeinfo)*/

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(32*8, 32*8) 	/*was:64*8, 64*8*/
	MDRV_VISIBLE_AREA(0*8, 32*8-1, 0*8, 32*8-1)
	MDRV_PALETTE_LENGTH(0x800)

	MDRV_VIDEO_START(deroon)
	MDRV_VIDEO_UPDATE(deroon)

	/* sound hardware */
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(YMF262,   ymf262_interface)
	MDRV_SOUND_ADD(OKIM6295, okim6295_interface)
	MDRV_SOUND_ADD(YMZ280B,  ymz280b_interface)
MACHINE_DRIVER_END


ROM_START( deroon )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* Main Program*/
	ROM_LOAD16_BYTE( "t001upau.bin", 0x00000, 0x80000, CRC(14b92c18) SHA1(b47b8c828222a3f7c0fe9271899bd38171d972fb) )
	ROM_LOAD16_BYTE( "t002upal.bin", 0x00001, 0x80000, CRC(0fb05c68) SHA1(5140592e15414770fb46d5ac9ba8f76e3d4ab323) )

	ROM_REGION( 0x048000, REGION_CPU2, 0 ) /* Sound Porgram*/
	ROM_LOAD( "t003uz1.bin", 0x000000, 0x008000, CRC(8bdfafa0) SHA1(c0cf3eb7a65d967958fe2aace171859b0faf7753) )
	ROM_CONTINUE(            0x010000, 0x038000 ) /* banked part */

	ROM_REGION( 0xb00000, REGION_GFX1, 0 ) /* Graphics - mostly (maybe all?) not tile based*/
	ROM_LOAD( "t101uah1.j66", 0x000000, 0x200000, CRC(74baf845) SHA1(935d2954ba227a894542be492654a2750198e1bc) )
	ROM_LOAD( "t102ual1.j67", 0x200000, 0x200000, CRC(1a02c4a3) SHA1(5155eeaef009fc9a9f258e3e54ca2a7f78242df5) )
	ROM_LOAD( "t103ubl1.j08", 0x400000, 0x200000, CRC(75431ec5) SHA1(c03e724c15e1fe7a0a385332f849e9ac9d149887) )
	ROM_LOAD( "t104ucl1.j68", 0x600000, 0x200000, CRC(66eb611a) SHA1(64435d35677fea3c06fdb03c670f3f63ee481c02) )
	ROM_LOAD( "t201ubb1.w61", 0x800000, 0x100000, CRC(d5a087ac) SHA1(5098160ce7719d93e3edae05f6edd317d4c61f0d) )
	ROM_LOAD( "t202ubc1.w62", 0x900000, 0x100000, CRC(f051dae1) SHA1(f5677c07fe644b3838657370f0309fb09244c619) )
	ROM_LOAD( "t301ubd1.w63", 0xa00000, 0x100000, CRC(8b026177) SHA1(3887856bdaec4d9d3669fe3bc958ef186fbe9adb) )

	ROM_REGION( 0x200000, REGION_SOUND1, 0 ) /* YMZ280B Samples*/
	ROM_LOAD( "t401uya1.w16", 0x000000, 0x200000, CRC(92111992) SHA1(ae27e11ae76dec0b9892ad32e1a8bf6ab11f2e6c) )

	ROM_REGION( 0x080000, REGION_SOUND2, 0 ) /* M6295 Samples*/
	ROM_LOAD( "t501uad1.w01", 0x000000, 0x080000, CRC(2fbcfe27) SHA1(f25c830322423f0959a36955edb563a6150f2142) )
ROM_END

ROM_START( tkdensho )
	ROM_REGION( 0x600000, REGION_CPU1, 0 )
	ROM_LOAD16_BYTE( "aeprge-2.pal", 0x00000, 0x80000, CRC(25e453d6) SHA1(9c84e2af42eff5cc9b14c1759d5bab42fa7bb663) )
	ROM_LOAD16_BYTE( "aeprgo-2.pau", 0x00001, 0x80000, CRC(22d59510) SHA1(5ade482d6ab9a22df2ee8337458c22cfa9045c73) )

	ROM_REGION( 0x038000, REGION_CPU2, 0 ) /* Sound Porgram*/
	ROM_LOAD( "aesprg-2.z1", 0x000000, 0x008000, CRC(43550ab6) SHA1(2580129ef8ebd9295249175de4ba985c752e06fe) )
	ROM_CONTINUE(            0x010000, 0x018000 ) /* banked part */

	ROM_REGION( 0x2900000, REGION_GFX1, 0 ) /* Graphics - mostly (maybe all?) not tile based*/
	ROM_LOAD( "ae100h.ah1",    0x0000000, 0x0400000, CRC(06be252b) SHA1(08d1bb569fd2e66e2c2f47da7780b31945232e62) )
	ROM_LOAD( "ae100.al1",     0x0400000, 0x0400000, CRC(009cdff4) SHA1(fd88f07313d14fd4429b09a1e8d6b595df3b98e5) )
	ROM_LOAD( "ae101h.bh1",    0x0800000, 0x0400000, CRC(f2469eff) SHA1(ba49d15cc7949437ba9f56d9b425a5f0e62137df) )
	ROM_LOAD( "ae101.bl1",     0x0c00000, 0x0400000, CRC(db7791bb) SHA1(1fe40b747b7cee7a9200683192b1d60a735a0446) )
	ROM_LOAD( "ae102h.ch1",    0x1000000, 0x0200000, CRC(f9d2a343) SHA1(d141ac0b20be587e77a576ef78f15d269d9c84e5) )
	ROM_LOAD( "ae102.cl1",     0x1200000, 0x0200000, CRC(681be889) SHA1(8044ca7cbb325e6dcadb409f91e0c01b88a1bca7) )
	ROM_LOAD( "ae104.el1",     0x1400000, 0x0400000, CRC(e431b798) SHA1(c2c24d4f395bba8c78a45ecf44009a830551e856) )
	ROM_LOAD( "ae105.fl1",     0x1800000, 0x0400000, CRC(b7f9ebc1) SHA1(987f664072b43a578b39fa6132aaaccc5fe5bfc2) )
	ROM_LOAD( "ae106.gl1",     0x1c00000, 0x0200000, CRC(7c50374b) SHA1(40865913125230122072bb13f46fb5fb60c088ea) )
	ROM_LOAD( "ae200w74.ba1",  0x1e00000, 0x0100000, CRC(c1645041) SHA1(323670a6aa2a4524eb968cc0b4d688098ffeeb12) )
	ROM_LOAD( "ae201w75.bb1",  0x1f00000, 0x0100000, CRC(3f63bdff) SHA1(0d3d57fdc0ec4bceef27c11403b3631d23abadbf) )
	ROM_LOAD( "ae202w76.bc1",  0x2000000, 0x0100000, CRC(5cc857ca) SHA1(2553fb5220433acc15dfb726dc064fe333e51d88) )
	ROM_LOAD( "ae300w36.bd1",  0x2100000, 0x0080000, CRC(e829f29e) SHA1(e56bfe2669ed1d1ae394c644def426db129d97e3) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* YMZ280B Samples*/
	ROM_LOAD( "ae400t23.ya1", 0x000000, 0x200000, CRC(c6ffb043) SHA1(e0c6c5f6b840f63c9a685a2c3be66efa4935cbeb) )
	ROM_LOAD( "ae401t24.yb1", 0x200000, 0x200000, CRC(d83f1a73) SHA1(412b7ac9ff09a984c28b7d195330d78c4aac3dc5) )

	ROM_REGION( 0x080000, REGION_SOUND2, 0 ) /* M6295 Samples*/
	ROM_LOAD( "ae500w07.ad1", 0x000000, 0x080000, CRC(3734f92c) SHA1(048555b5aa89eaf983305c439ba08d32b4a1bb80) )
ROM_END


static DRIVER_INIT( deroon )
{
	data16_t *ROM = (data16_t *)memory_region(REGION_CPU1);

	memcpy(protram, ROM+0xC46/2, 0x10);

	/* Patch the long eeprom write delay to speedup bootstrapping*/
	ROM[0x39C2/2] = 0x1;

/*	ROM[0x448/2] = 0x4E71;*/
/*	ROM[0x44A/2] = 0x4E71;*/
}

GAMEX( 1996, deroon,      0, deroon, deroon, deroon,     ROT0, "Tecmo", "Deroon DeroDero", GAME_NOT_WORKING | GAME_NO_SOUND )
GAMEX( 1996, tkdensho,    0, deroon, deroon, 0,          ROT0, "Tecmo", "Touki Denshou -Angel Eyes-", GAME_NOT_WORKING | GAME_NO_SOUND )
