#include "driver.h"
#include "system16.h"
#include "cpu/i8039/i8039.h"
#include "vidhrdw/segaic16.h"

WRITE_HANDLER( sys16_7751_audio_8255_w );
 READ_HANDLER( sys16_7751_audio_8255_r );
 READ_HANDLER( sys16_7751_sh_rom_r );
 READ_HANDLER( sys16_7751_sh_t1_r );
 READ_HANDLER( sys16_7751_sh_command_r );
WRITE_HANDLER( sys16_7751_sh_dac_w );
WRITE_HANDLER( sys16_7751_sh_busy_w );
WRITE_HANDLER( sys16_7751_sh_offset_a0_a3_w );
WRITE_HANDLER( sys16_7751_sh_offset_a4_a7_w );
WRITE_HANDLER( sys16_7751_sh_offset_a8_a11_w );
WRITE_HANDLER( sys16_7751_sh_rom_select_w );


static UINT8 disable_screen_blanking;
static int sys16_soundbanktype=0;

static WRITE_HANDLER( UPD7759_bank_w );

static MEMORY_READ_START( sound_readmem )
  { 0x0000, 0x7fff, MRA_ROM },
  { 0xe800, 0xe800, soundlatch_r },
  { 0xf800, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
  { 0x0000, 0x7fff, MWA_ROM },
  { 0xf800, 0xffff, MWA_RAM },
MEMORY_END

static PORT_READ_START( sound_readport )
  { 0x01, 0x01, YM2151_status_port_0_r },
  { 0xc0, 0xc0, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_writeport )
  { 0x00, 0x00, YM2151_register_port_0_w },
  { 0x01, 0x01, YM2151_data_port_0_w },
PORT_END

// 7751 Sound
static MEMORY_READ_START( sound_readmem_7751 )
  { 0x0000, 0x7fff, MRA_ROM },
  { 0xe800, 0xe800, soundlatch_r },
  { 0xf800, 0xffff, MRA_RAM },
MEMORY_END

static PORT_READ_START( sound_readport_7751 )
  { 0x01, 0x01, YM2151_status_port_0_r },
  //{ 0x0e, 0x0e, sys16_7751_audio_8255_r },
  { 0xc0, 0xc0, soundlatch_r },
PORT_END

static PORT_WRITE_START( sound_writeport_7751 )
  { 0x00, 0x00, YM2151_register_port_0_w },
  { 0x01, 0x01, YM2151_data_port_0_w },
  { 0x80, 0x80, sys16_7751_audio_8255_w },
PORT_END

static MEMORY_READ_START( readmem_7751 )
  { 0x0000, 0x03ff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( writemem_7751 )
  { 0x0000, 0x03ff, MWA_ROM },
MEMORY_END

static PORT_READ_START( readport_7751 )
  { I8039_t1, I8039_t1, sys16_7751_sh_t1_r },
  { I8039_p2, I8039_p2, sys16_7751_sh_command_r },
  { I8039_bus, I8039_bus, sys16_7751_sh_rom_r },
PORT_END

static PORT_WRITE_START( writeport_7751 )
  { I8039_p1, I8039_p1, sys16_7751_sh_dac_w },
  { I8039_p2, I8039_p2, sys16_7751_sh_busy_w },
  { I8039_p4, I8039_p4, sys16_7751_sh_offset_a0_a3_w },
  { I8039_p5, I8039_p5, sys16_7751_sh_offset_a4_a7_w },
  { I8039_p6, I8039_p6, sys16_7751_sh_offset_a8_a11_w },
  { I8039_p7, I8039_p7, sys16_7751_sh_rom_select_w },
PORT_END

// 7759
static MEMORY_READ_START( sound_readmem_7759 )
  { 0x0000, 0x7fff, MRA_ROM },
  { 0x8000, 0xdfff, MRA_BANK1 },
  { 0xe800, 0xe800, soundlatch_r },
  { 0xf800, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem_7759 )
  { 0x0000, 0x7fff, MWA_ROM },
  { 0x8000, 0xdfff, MWA_BANK1 },
  { 0xf800, 0xffff, MWA_RAM },
MEMORY_END

static PORT_WRITE_START( sound_writeport_7759 )
  { 0x00, 0x00, YM2151_register_port_0_w },
  { 0x01, 0x01, YM2151_data_port_0_w },
  { 0x40, 0x40, UPD7759_bank_w },
  { 0x80, 0x80, UPD7759_0_port_w },
PORT_END

static WRITE_HANDLER( UPD7759_bank_w )
{
  int bankoffs=0;

  int size = memory_region_length(REGION_CPU2) - 0x10000;
  if (size > 0)
  /* banking depends on the ROM board */
  {
    UPD7759_start_w(0, data & 0x80);
    UPD7759_reset_w(0, data & 0x40);

    switch (sys16_soundbanktype)
    {
      case 0:

        bankoffs =  (data * 0x4000);
        break;

      case 1:

        /*
        D5 : /CS for ROM at A11
        D4 : /CS for ROM at A10
        D3 : /CS for ROM at A9
        D2 : /CS for ROM at A8
        D1 : A15 for all ROMs (Or ignored for 27256's)
        D0 : A14 for all ROMs
        */
        if (!(data & 0x04)) bankoffs = 0x00000;
        if (!(data & 0x08)) bankoffs = 0x10000;
        if (!(data & 0x10)) bankoffs = 0x20000;
        if (!(data & 0x20)) bankoffs = 0x30000;
        bankoffs += (data & 0x03) * 0x4000;
        break;


      case 2:  // Cotton Fantasy Zone II etc etc
        /*
        D5 : Unused
        D4 : Unused
        D3 : ROM select 0=A11, 1=A12
        D2 : A16 for all ROMs
        D1 : A15 for all ROMs
        D0 : A14 for all ROMs
        */
        bankoffs = ((data & 0x08) >> 3) * 0x20000;
        bankoffs += (data & 0x07) * 0x4000;
        break;

      case 3:
        /*
        D5 : Unused
        D4 : A17 for all ROMs
        D3 : ROM select 0=A11, 1=A12
        D2 : A16 for all ROMs
        D1 : A15 for all ROMs
        D0 : A14 for all ROMs
        */
        bankoffs = ((data & 0x08) >> 3) * 0x40000;
        bankoffs += ((data & 0x10) >> 4) * 0x20000;
        bankoffs += (data & 0x07) * 0x04000;
        break;
    }
    cpu_setbank(1, memory_region(REGION_CPU2) + 0x10000 + (bankoffs % size));
  }
}

static void sound_cause_a_nmi( int state )
{
  if (state) /* upd7759 callback */
    cpu_set_nmi_line(1, PULSE_LINE);
}

static WRITE16_HANDLER( sound_command_w )
{
  soundlatch_w( 0,data&0xff );
  cpu_set_irq_line( 1, 0, HOLD_LINE );
}

struct UPD7759_interface sys16b_upd7759_interface =
{
  1,      /* 1 chip */
  { 60 },   /* volumes */
  { REGION_CPU2 },      /* memory region 3 contains the sample data */
    UPD7759_SLAVE_MODE,
  { sound_cause_a_nmi },
};

static READ16_HANDLER( standard_io_r )
{
  offset &= 0x1fff;
  switch (offset & (0x3000/2))
  {
    case 0x1000/2:
      return readinputport(offset & 3);
    
    case 0x2000/2:
      return  ((offset & 1) ? readinputport(4)  : readinputport(5));
  }
  logerror("CPU #0 PC %06x: standard_io_r - unknown read access to address %06x\n", activecpu_get_pc(), offset);
  return 0;
}

static WRITE16_HANDLER( standard_io_w )
{
  offset &= 0x1fff;
  switch (offset & (0x3000/2))
  {
    case 0x0000/2:
      /*
                D7 : 1 for most games, 0 for ddux, sdi, wb3
                D6 : 1= Screen flip, 0= Normal screen display
                D5 : 1= Display on, 0= Display off
                D4 : 0 for most games, 1 for eswat
                D3 : Output to lamp 2 (1= On, 0= Off)
                D2 : Output to lamp 1 (1= On, 0= Off)
                D1 : (Output to coin counter 2?)
                D0 : Output to coin counter 1
       */
    system16b_set_screen_flip(data & 0x40);
    if (!disable_screen_blanking)
      system16b_set_draw_enable(data & 0x20);
    set_led_status(1, data & 0x08);
    set_led_status(0, data & 0x04);
    coin_counter_w(1, data & 0x02);
    coin_counter_w(0, data & 0x01);
    return;
  }
  logerror("%06X:standard_io_w - unknown write access to address %04X = %04X & %04X\n", activecpu_get_pc(), offset * 2, data, mem_mask ^ 0xffff);
}

static WRITE16_HANDLER( rom_5704_bank_w )
{
  if (ACCESSING_LSB)
    system16b_set_tile_bank(offset & 1, data & 7);
}

INPUT_PORTS_START( bullet )

  PORT_START
  PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
  PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
  PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )
  PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
  PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
  PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
  PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START3 )
  PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

  PORT_START
  PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER1 )
  PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_8WAY | IPF_PLAYER1 )

  PORT_START
  PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER3 )
  PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_8WAY | IPF_PLAYER3 )

  PORT_START
  PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_PLAYER2 )
  PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_8WAY | IPF_PLAYER2 )


  SYS16_COINAGE //DSW1

  PORT_START//DSW2
  PORT_DIPNAME( 0x01, 0x01, "Players" )
  PORT_DIPSETTING(    0x01, "2" )
  PORT_DIPSETTING(    0x00, "3" )
  PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
  PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
  PORT_DIPSETTING(    0x00, DEF_STR( On ) )
  PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
  PORT_DIPSETTING(    0x00, "2" )
  PORT_DIPSETTING(    0x0c, "3" )
  PORT_DIPSETTING(    0x08, "4" )
  PORT_DIPSETTING(    0x04, "5" )
INPUT_PORTS_END

INPUT_PORTS_START( cotton )

  SYS16_SERVICE
  SYS16_JOY1
  SYS16_UNUSED
  SYS16_JOY2

    SYS16_COINAGE //DSW1
  PORT_START //DSW2
  PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
  PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
  PORT_DIPSETTING(    0x00, DEF_STR( On ) )
  PORT_DIPNAME( 0x06, 0x06, DEF_STR( Lives ) )
  PORT_DIPSETTING(    0x04, "2" )
  PORT_DIPSETTING(    0x06, "3" )
  PORT_DIPSETTING(    0x02, "4" )
  PORT_DIPSETTING(    0x00, "5" )
  PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
  PORT_DIPSETTING(    0x10, "Easy" )
  PORT_DIPSETTING(    0x18, "Normal" )
  PORT_DIPSETTING(    0x08, "Hard" )
  PORT_DIPSETTING(    0x00, "Hardest" )
  //"SW2:6" unused
  //"SW2:7" unused
  //"SW2:8" unused
INPUT_PORTS_END

INPUT_PORTS_START( fantzn2x )
  SYS16_SERVICE
  SYS16_JOY1
  SYS16_UNUSED
  SYS16_JOY2

  SYS16_COINAGE //DSW1
  PORT_START////DSW2
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
  PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
  PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
  PORT_DIPSETTING(    0x08, "2" )
  PORT_DIPSETTING(    0x0c, "3" )
  PORT_DIPSETTING(    0x04, "4" )
  PORT_DIPSETTING(    0x00, "240 (Cheat)")
  PORT_BIT( 0x010, IP_ACTIVE_LOW, IPT_UNKNOWN )
  PORT_BIT( 0x020, IP_ACTIVE_LOW, IPT_UNKNOWN )
  PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
  PORT_DIPSETTING(    0x80, "Easy" )
  PORT_DIPSETTING(    0xc0, "Normal" )
  PORT_DIPSETTING(    0x40, "Hard" )
  PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END

static MEMORY_READ16_START( bullet_readmem )
  { 0x000000, 0x02ffff, MRA16_ROM },
  { 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
  { 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
  { 0x440000, 0x4407ff, SYS16_MRA16_SPRITERAM },
  { 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
  { 0xc40000, 0xc43fff, standard_io_r },
  { 0xffc000, 0xffffff, SYS16_MRA16_WORKINGRAM },
MEMORY_END

static MEMORY_WRITE16_START( bullet_writemem )
  { 0x000000, 0x02ffff, MWA16_ROM },
  { 0x400000, 0x40ffff, segaic16_tileram_w, &sys16_tileram },
  { 0x410000, 0x410fff, system16b_textram_w, &sys16_textram },
  { 0x440000, 0x4407ff, SYS16_MWA16_SPRITERAM, &sys16_spriteram },
  { 0x840000, 0x840fff, segaic16_paletteram_w, &paletteram16 },
  { 0xc00006, 0xc00007, sound_command_w },
  { 0xc40000, 0xc43fff, standard_io_w },
  { 0xffc000, 0xffffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram },
MEMORY_END

static MEMORY_READ16_START( cotton_readmem )
  { 0x000000, 0x0bffff, MRA16_ROM },
  { 0x100000, 0x10FFFF, MRA16_RAM }, //rom_5704_bank
  { 0x200000, 0x203FFF, SYS16_MRA16_WORKINGRAM }, //workingram
  { 0x300000, 0x3007FF, SYS16_MRA16_SPRITERAM },
  { 0x400000, 0x40FFFF, SYS16_MRA16_TILERAM },
  { 0x410000, 0x410FFF, SYS16_MRA16_TEXTRAM },
  { 0x500000, 0x500FFF, SYS16_MRA16_PALETTERAM },
  { 0x600000, 0x603FFF, standard_io_r },
MEMORY_END

static MEMORY_WRITE16_START( cotton_writemem )
    { 0x000000, 0x0bffff, MWA16_ROM },
  { 0x100000, 0x10FFFF, rom_5704_bank_w }, //rom_5704_bank
  { 0x200000, 0x203FFF, SYS16_MWA16_WORKINGRAM, &sys16_workingram }, //workingram
  { 0x300000, 0x3007FF, SYS16_MWA16_SPRITERAM, &sys16_spriteram },
  { 0x400000, 0x40FFFF, segaic16_tileram_w, &sys16_tileram },
  { 0x410000, 0x410FFF, system16b_textram_w, &sys16_textram },
  { 0x500000, 0x500FFF, segaic16_paletteram_w, &paletteram16 },
  { 0x600000, 0x603FFF, standard_io_w  },
  { 0xFF0006, 0xFF0007, sound_command_w },
MEMORY_END


static MEMORY_READ16_START( fantzn2x_readmem )
  { 0x000000, 0x0bffff, MRA16_ROM },
  { 0x200000, 0x23ffff, SYS16_MRA16_WORKINGRAM },
  { 0x400000, 0x40ffff, SYS16_MRA16_TILERAM },
  { 0x410000, 0x410fff, SYS16_MRA16_TEXTRAM },
  { 0x440000, 0x440fff, SYS16_MRA16_SPRITERAM },
  { 0x840000, 0x840fff, SYS16_MRA16_PALETTERAM },
  { 0xc40000, 0xc43FFF, standard_io_r }, MEMORY_END

static MEMORY_WRITE16_START( fantzn2x_writemem )
  { 0x000000, 0x0bffff, MWA16_ROM },
  { 0x200000, 0x23ffff, SYS16_MWA16_WORKINGRAM, &sys16_workingram},
  { 0x3f0000, 0x3fffff, rom_5704_bank_w }, //rom_5704_bank
  { 0x400000, 0x40ffff, segaic16_tileram_w, &sys16_tileram },
  { 0x410000, 0x410fff, system16b_textram_w, &sys16_textram },
  { 0x440000, 0x440fff, SYS16_MWA16_SPRITERAM, &sys16_spriteram },
  { 0x840000, 0x840fff, segaic16_paletteram_w, &paletteram16 },
  { 0xc40000, 0xc43FFF, standard_io_w },
  { 0xfe0006, 0xfe0007, sound_command_w },
MEMORY_END

ROM_START( bulletd )
  ROM_REGION( 0x040000, REGION_CPU1, 0 ) /* 68000 code */
  ROM_LOAD16_BYTE( "bootleg_epr-11010.a4",  0x000000, 0x08000, CRC(c4b7cb63) SHA1(c35fceab2a03f23d9690432a582064d12de950f6) )
  ROM_LOAD16_BYTE( "bootleg_epr-11007.a1",  0x000001, 0x08000, CRC(2afa84c5) SHA1(97f74ba4b9f83314c9e1f61afe7db3d7fa7a9935) )
  ROM_LOAD16_BYTE( "epr11011.a5",  0x010000, 0x08000, CRC(7f446b9f) SHA1(0b92ab100c13bdcdd0f770da5da5e19cb79afde1) )
  ROM_LOAD16_BYTE( "epr11008.a2",  0x010001, 0x08000, CRC(34824d3b) SHA1(7a3134a71ad176b8a08a919c0acb75ae1e05743b) )
  ROM_LOAD16_BYTE( "epr11012.a6",  0x020000, 0x08000, CRC(3992f159) SHA1(50686b394693ab01cbd159ae661f326c8eee50b8) )
  ROM_LOAD16_BYTE( "epr11009.a3",  0x020001, 0x08000, CRC(df199999) SHA1(2669e923aa4f1bedc788401f44ad19c318658f00) )

  ROM_REGION( 0x30000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
  ROM_LOAD( "epr10994.b9",  0x00000, 0x10000, CRC(3035468a) SHA1(778366815a2a74188d72d64c5e1e95215bc4ca81) )
  ROM_LOAD( "epr10995.b10", 0x10000, 0x10000, CRC(6b97aff1) SHA1(323bafe43a703476f6f4e68b46ec86bb9152f88e) )
  ROM_LOAD( "epr10996.b11", 0x20000, 0x10000, CRC(501bddd6) SHA1(545273b1b874b3e68d23b0dcae81c8531bd98756) )

  ROM_REGION16_BE( 0x80000, REGION_GFX2, ROMREGION_ERASE00 ) /* sprites */
  ROM_LOAD16_BYTE( "epr10999.b1", 0x00001, 0x010000, CRC(119f0008) SHA1(6a39b537bb58ea19ed3b0322ebca37e6574289fd) )
  ROM_LOAD16_BYTE( "epr11003.b5", 0x00000, 0x010000, CRC(2f429089) SHA1(08bf9d9c15fafbcb26604ff30be367ecf25404b2) )
  ROM_LOAD16_BYTE( "epr11000.b2", 0x20001, 0x010000, CRC(f5482bbe) SHA1(d8482ba73622798b15e78ab2c123d0fd4c33480a) )
  ROM_LOAD16_BYTE( "epr11004.b6", 0x20000, 0x010000, CRC(8c886df0) SHA1(348f9111fe45fc94cb32b101d0a1a6a39ef1ec50) )
  ROM_LOAD16_BYTE( "epr11001.b3", 0x40001, 0x010000, CRC(65ea71e0) SHA1(79224c445ceaa1d13a3616e58e9d4eb595e920cb) )
  ROM_LOAD16_BYTE( "epr11005.b7", 0x40000, 0x010000, CRC(ea2f9d50) SHA1(db62584591d62780f81de651869bc74a61363793) )
  ROM_LOAD16_BYTE( "epr11002.b4", 0x60001, 0x010000, CRC(9e25042b) SHA1(cb0e20ca8ca1c42ad2a95b83ea8711b7ad8e42f5) )
  ROM_LOAD16_BYTE( "epr11006.b8", 0x60000, 0x010000, CRC(6b7384f2) SHA1(5201e3b5e4aeb4bc8f5b3ba3d8a9ffb3705eccf4) )

  ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
  ROM_LOAD( "epr10997.a7", 0x00000, 0x8000, CRC(5dd9cab5) SHA1(b9b27bbdc19feacb83cc5b33a74d910d86ac8f33) )
  ROM_LOAD( "epr10998.a8", 0x10000, 0x8000, CRC(f971a817) SHA1(502c95638e4fd5f87e5fc837cb44b39a5d62f4e4) )
ROM_END

ROM_START( cottond )
  ROM_REGION( 0x100000, REGION_CPU1, 0 ) // 68000 code
  ROM_LOAD16_BYTE( "bootleg_epr-13921a.a7", 0x000000, 0x20000, CRC(92947867) SHA1(6d5f1be45690bacac7093b442ed99c4de56d73a4) )
  ROM_LOAD16_BYTE( "bootleg_epr-13919a.a5", 0x000001, 0x20000, CRC(30f131fb) SHA1(5b35b4691d2436e82be3079634d8f7f259e46450) )
  ROM_LOAD16_BYTE( "bootleg_epr-13922a.a8", 0x080000, 0x20000, CRC(f0f75329) SHA1(e223b3b6e15ead11f93e353ddce5227f8b362d2e) )
  ROM_LOAD16_BYTE( "bootleg_epr-13920a.a6", 0x080001, 0x20000, CRC(a3721aab) SHA1(bfcd8e564f06520e51c61418246ef06e4a0036d7) )

  ROM_REGION( 0xc0000, REGION_GFX1, 0 ) // tiles
  ROM_LOAD( "opr-13862.a14", 0x00000, 0x20000, CRC(a47354b6) SHA1(ce52813b245f1d491a134d4bd5ab074e71d20129) )
  ROM_LOAD( "opr-13877.b14", 0x20000, 0x20000, CRC(d38424b5) SHA1(884ca190936aee2d2cac86491d4d0cdf4a45efe5) )
  ROM_LOAD( "opr-13863.a15", 0x40000, 0x20000, CRC(8c990026) SHA1(07b4510936376c171f3b31d87ac6154361eb0cbc) )
  ROM_LOAD( "opr-13878.b15", 0x60000, 0x20000, CRC(21c15b8a) SHA1(690d92420ec5465885e0f4870419992961420e33) )
  ROM_LOAD( "opr-13864.a16", 0x80000, 0x20000, CRC(d2b175bf) SHA1(897b7c794d0e7229ea5e9a682f64266a947a818f) )
  ROM_LOAD( "opr-13879.b16", 0xa0000, 0x20000, CRC(b9d62531) SHA1(e8c5e7b93339c00f75a3b66ce18f7838255577be) )

  ROM_REGION16_BE( 0x200000, REGION_GFX2, 0 ) // sprites
  ROM_LOAD16_BYTE( "opr-13869.b5", 0x000000, 0x20000, CRC(ab4b3468) SHA1(3071654a295152d609d2c2c1d4153b5ba3f174d5) )
  ROM_LOAD16_BYTE( "opr-13865.b1", 0x000001, 0x20000, CRC(7024f404) SHA1(4b2f9cdfdd97218797a3e386106e53f713b8650d) )
  ROM_LOAD16_BYTE( "opr-13870.b6", 0x040000, 0x20000, CRC(69b41ac3) SHA1(4c5a85e5a5ca9f8260557d4e97eb091dd857d63a) )
  ROM_LOAD16_BYTE( "opr-13866.b2", 0x040001, 0x20000, CRC(6169bba4) SHA1(a24a418ee7cd0c1109870a2e7a91e430671897ed) )
  ROM_LOAD16_BYTE( "opr-13871.b7", 0x080000, 0x20000, CRC(0801cf02) SHA1(3007bbbce2f327f4700e78e2b8672f4482189cd7) )
  ROM_LOAD16_BYTE( "opr-13867.b3", 0x080001, 0x20000, CRC(b014f02d) SHA1(46f5ed0b44cee03a6aec9ec57b506bb15bf35e47) )
  ROM_LOAD16_BYTE( "opr-13872.b8", 0x0c0000, 0x20000, CRC(f066f315) SHA1(bbeb24daaded994240d0cdb5cec2e662b677cb75) )
  ROM_LOAD16_BYTE( "opr-13868.b4", 0x0c0001, 0x20000, CRC(e62a7cd6) SHA1(1e6d06345f7b6cef2e887d9b9cd45e0155140c5e) )
  ROM_LOAD16_BYTE( "opr-13873.b10",0x100000, 0x20000, CRC(1bd145f3) SHA1(4744ffe9fbda453785345b46eb61b56730048f42) )
  ROM_LOAD16_BYTE( "opr-13852.a1", 0x100001, 0x20000, CRC(943aba8b) SHA1(d0dd1665a8d9495a92ae4e35d6b15b966e8d43cd) )
  ROM_LOAD16_BYTE( "opr-13874.b11",0x140000, 0x20000, CRC(4fd59bff) SHA1(2b4630e49b60593d668fe34d8faf712ac6928c14) )
  ROM_LOAD16_BYTE( "opr-13853.a2", 0x140001, 0x20000, CRC(7ea93200) SHA1(8e2d8cd48a12306772653f25bddc99ad0597a698) )
  ROM_LOAD16_BYTE( "opr-13894.b12",0x180000, 0x20000, CRC(e3d0bee2) SHA1(503a78123ca9d6f3405972bca281dcdaba929c99) )
  ROM_LOAD16_BYTE( "opr-13891.a3", 0x180001, 0x20000, CRC(c6b3c414) SHA1(0f0d936e77eb483be8865e8d968d78260e88ca99) )
  ROM_LOAD16_BYTE( "opr-13876.b13",0x1c0000, 0x20000, CRC(1c5ffad8) SHA1(13e5886ceece564cc71ba7f43a26d2b1782ccfc8) )
  ROM_LOAD16_BYTE( "opr-13855.a4", 0x1c0001, 0x20000, CRC(856f3ee2) SHA1(72346d887ff9738ebe93acb2e3f8cd80d494621e) )

  ROM_REGION( 0x50000, REGION_CPU2, 0 ) // sound CPU
  ROM_LOAD( "epr-13892.a10", 0x00000, 0x08000, CRC(fdfbe6ad) SHA1(9ebb94889c0e96e6af9cdced084804ca98612d61) )
  ROM_LOAD( "opr-13893.a11", 0x10000, 0x20000, CRC(384233df) SHA1(dfdf94697587a5ee45e97700f3741be54b90742b) )

//  ROM_REGION( 0x0100, "plds", 0 )
//  ROM_LOAD( "315-5298.b9",  0x0000, 0x00eb, CRC(39b47212) SHA1(432b47aee5ecbf08a8a6dc2f8379c816feb86328) ) // PLS153

ROM_END

ROM_START( fantzn2x )
  ROM_REGION( 0xc0000, REGION_CPU1, 0 ) /* 68000 code */
  ROM_LOAD16_BYTE( "fz2.a7", 0x00000, 0x20000, CRC(94c05f0b) SHA1(53da68a919776a46ae96dbc094ff941308d13613) )
  ROM_LOAD16_BYTE( "fz2.a5", 0x00001, 0x20000, CRC(f3526895) SHA1(3197956608138601192f111d3bcc26662a7d6ec1) )
  /* empty 0x40000 - 0x80000 */
  ROM_LOAD16_BYTE( "fz2.a8", 0x80000, 0x20000, CRC(b2ebb209) SHA1(bd40c90a372ab92a869bdd28d12cf52b45ecc80e) )
  ROM_LOAD16_BYTE( "fz2.a6", 0x80001, 0x20000, CRC(6833f546) SHA1(b4503cdb5bdb1322c34b9da3ff4227c740dad707) )

  ROM_REGION( 0x60000, REGION_GFX1, ROMREGION_DISPOSE ) /* tiles */
  ROM_LOAD( "fz2.a14", 0x00000, 0x20000, CRC(1c0a4537) SHA1(3abdf51ea81780309bcfaf12c04efdf7cb15a649) )
  ROM_LOAD( "fz2.a15", 0x20000, 0x20000, CRC(2b933344) SHA1(5b53ea8d58cc3d157aec6926db048359984e4276) )
  ROM_LOAD( "fz2.a16", 0x40000, 0x20000, CRC(e63281a1) SHA1(72379c579484c1ef7784a9598d373446ef0a472b) )

  ROM_REGION16_BE( 0x180000, REGION_GFX2, 0 ) /* sprites */
  ROM_LOAD16_BYTE( "fz2.b1",  0x000001, 0x20000, CRC(46bba615) SHA1(b291df4a83d7155eb7606f86ed733c24362a4db3) )
  ROM_LOAD16_BYTE( "fz2.b5",  0x000000, 0x20000, CRC(bebeee5d) SHA1(9e57e62c6b9136667aa90d7d423fc33ac6df4352) )
  ROM_LOAD16_BYTE( "fz2.b2",  0x040001, 0x20000, CRC(6681a7b6) SHA1(228df38601ba3895e9449921a64850941715b421) )
  ROM_LOAD16_BYTE( "fz2.b6",  0x040000, 0x20000, CRC(42d3241f) SHA1(c3240e3e1d7d398e74e76ba65adca6b06f0f67a9) )
  ROM_LOAD16_BYTE( "fz2.b3",  0x080001, 0x20000, CRC(5863926f) SHA1(0e591c4b85e5d572b3311bec2c1f6d2484204db6) )
  ROM_LOAD16_BYTE( "fz2.b7",  0x080000, 0x20000, CRC(cd830510) SHA1(8a32a1aa43f8af5e86f552f05da40b6e4ba12495) )
  ROM_LOAD16_BYTE( "fz2.b4",  0x0c0001, 0x20000, CRC(b98fa5b6) SHA1(c3f8891f81e80321e2ee5cc1f4d93b1867ed1868) )
  ROM_LOAD16_BYTE( "fz2.b8",  0x0c0000, 0x20000, CRC(e8248f68) SHA1(7876945d2baf1d7bdb9cc3a23be9f1a1681cede9) )
  ROM_LOAD16_BYTE( "fz2.a1",  0x100001, 0x20000, CRC(9d2f41f3) SHA1(54f5dc47d854cd26b108695f55263d8b8c29ce0e) )
  ROM_LOAD16_BYTE( "fz2.b10", 0x100000, 0x20000, CRC(7686ea33) SHA1(812a638f42500b30f80f9a3956c5eb4553cc35d0) )
  ROM_LOAD16_BYTE( "fz2.a2",  0x140001, 0x20000, CRC(3b4050b7) SHA1(8c7c8051c577a4b2ca54d7e60c100fbd5391551f) )
  ROM_LOAD16_BYTE( "fz2.b11", 0x140000, 0x20000, CRC(da8a95dc) SHA1(d44e1515008d4ee302f940ce7799fa9a790799e9) )

  ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
  ROM_LOAD( "fz2.a10", 0x00000, 0x08000, CRC(92c92924) SHA1(3c98cea8f42c316405b28ae03469c6876de5e806) )
  ROM_LOAD( "fz2.a11", 0x10000, 0x20000, CRC(8c641bb9) SHA1(920da63961d2f3457c80d4c5f6d4f405374bb23a) )
ROM_END


static MACHINE_INIT( bank1 )
{
  machine_init_sys16_onetime();
  system16b_configure_sprite_banks(1);
}

static MACHINE_INIT( bank0 )
{
  machine_init_sys16_onetime();
  system16b_configure_sprite_banks(0);
}

/***************************************************************************/

static INTERRUPT_GEN( sys16_interrupt )
{
  if(sys16_custom_irq) sys16_custom_irq();
  cpu_set_irq_line(0, 4, HOLD_LINE); /* Interrupt vector 4, used by VBlank */
}

static MACHINE_DRIVER_START( system16b )

  /* basic machine hardware */
  MDRV_CPU_ADD_TAG("main", M68000, 10000000)
  MDRV_CPU_VBLANK_INT(sys16_interrupt,1)

  MDRV_CPU_ADD_TAG("sound", Z80, 5000000)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
    MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
  MDRV_CPU_PORTS(sound_readport,sound_writeport)

  MDRV_FRAMES_PER_SECOND(60)
  MDRV_VBLANK_DURATION(1000000 * (262 - 224) / (262 * 60))

  /* video hardware */
  MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
  MDRV_SCREEN_SIZE(40*8, 28*8)
  MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 28*8-1)
  MDRV_GFXDECODE(sys16_gfxdecodeinfo)
  MDRV_PALETTE_LENGTH(2048*3)

  MDRV_VIDEO_START(system16b)
  MDRV_VIDEO_UPDATE(system16b)

  /* sound hardware */
  MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
  MDRV_SOUND_ADD_TAG("2151", YM2151, sys16_ym2151_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( system16_7759 )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(system16b)

  MDRV_CPU_MODIFY("sound")
    MDRV_CPU_MEMORY(sound_readmem_7759,sound_writemem_7759)
  MDRV_CPU_PORTS(sound_readport,sound_writeport_7759)

  /* sound hardware */
  MDRV_SOUND_ADD_TAG("7759", UPD7759, sys16b_upd7759_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( system16_7751 )

  /* basic machine hardware */
  MDRV_IMPORT_FROM(system16b)

  MDRV_CPU_MODIFY("sound")
  MDRV_CPU_MEMORY(sound_readmem_7751,sound_writemem)
  MDRV_CPU_PORTS(sound_readport_7751,sound_writeport_7751)

  MDRV_CPU_ADD(N7751, 6000000/15)
  MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
  MDRV_CPU_MEMORY(readmem_7751,writemem_7751)
  MDRV_CPU_PORTS(readport_7751,writeport_7751)

  /* sound hardware */
  MDRV_SOUND_ADD(DAC, sys16_7751_dac_interface)
MACHINE_DRIVER_END

/***************************************************************************/

static MACHINE_INIT( cotton )
{
  machine_init_sys16_onetime();
  machine_init_bank1();
  sys16_soundbanktype=2;
}
static MACHINE_INIT( bullet )
{
  machine_init_sys16_onetime();
  machine_init_bank0();
  sys16_soundbanktype=1;
}
static MACHINE_DRIVER_START( bullet )

  /* basic machine hardware */
  MDRV_IMPORT_FROM(system16_7759)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_MEMORY(bullet_readmem,bullet_writemem)
  MDRV_MACHINE_INIT(bullet)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( cotton )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(system16_7759)
  MDRV_CPU_MODIFY("main")
    MDRV_CPU_MEMORY(cotton_readmem,cotton_writemem)
  MDRV_MACHINE_INIT(cotton)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( fantzn2x )

  /* basic machine hardware */
  MDRV_IMPORT_FROM(system16_7759)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_MEMORY(fantzn2x_readmem,fantzn2x_writemem)

  MDRV_MACHINE_INIT(bank1)
MACHINE_DRIVER_END

/*          rom       parent    machine   inp       init */
GAME( 1991, cottond,  cotton,   cotton,   cotton,   0,       ROT0,   "Sega",    "Cotton (set 4, World) (unprotected of FD1094 317-0181a set)" )
GAME( 1987, bulletd,  0,        bullet,   bullet,   0,       ROT0,   "Sega",    "Bullet (unprotected of FD1094 317-0041 set)" )
GAME( 2008, fantzn2x, 0,        fantzn2x, fantzn2x, 0,       ROT0,   "Sega",    "Fantasy Zone II - The Tears of Opa-Opa (System 16C version)" )
