/* ST-V SpeedUp Hacks */

/*
to be honest i think some of these cause more problems than they're worth ...
*/

#include "driver.h"

extern data32_t *stv_workram_h;
DRIVER_INIT ( stv );

/* the IC13 carts: their program ROM is now loaded as MAME loads it (odd
   bytes, mirrored), so they need no boot vector patch */
DRIVER_INIT( ic13 )
{
	init_stv();
}

/*
Super Major League / Final Arch: each frame the master queues a job for the slave
(the depth sort of the polygons), waits for the slave to take it (the read pointer
at 060ffc44) and then clears the "sort done" flag that the slave sets when the job
is over. The slave is much slower than the master on the real hardware, but in
coarse time slices it can take the job and finish it before the master looks at the
pointer again, so the flag is lost and the game hangs. A tight interleave while the
master polls keeps the order of the real hardware.
*/
static READ32_HANDLER( smleague_sync_r )
{
	/* a slice of 16 SH-2 cycles is enough (a perfect one costs half the speed);
	   with 64 cycles Super Major League and Final Arch hang again */
	cpu_boost_interleave(TIME_IN_HZ(57272720/32), TIME_IN_USEC(20));
	return stv_workram_h[0x0ffc44/4];
}

DRIVER_INIT( smleague )
{
	install_mem_read32_handler(0, 0x060ffc44, 0x060ffc47, smleague_sync_r);
	init_stv();
}

static READ32_HANDLER( shienryu_slave_speedup_r )
{
 if (activecpu_get_pc()==0x06004410)
  cpu_spinuntil_time(TIME_IN_USEC(20)); /* is this safe... we can't skip till vbl because its not a vbl wait loop*/

 return stv_workram_h[0x0ae8e4/4];
}


static READ32_HANDLER( shienryu_speedup_r )
{
	if (activecpu_get_pc()==0x060041C8) cpu_spinuntil_int(); /* after you enable the sound cpu ...*/
	return stv_workram_h[0x0ae8e0/4];
}


DRIVER_INIT(shienryu)
{
	install_mem_read32_handler(0, 0x60ae8e0, 0x60ae8e3, shienryu_speedup_r ); /* after you enable sound cpu*/
	install_mem_read32_handler(1, 0x60ae8e4, 0x60ae8e7, shienryu_slave_speedup_r ); /* after you enable sound cpu*/

	init_stv();
}

static READ32_HANDLER( prikura_speedup_r )
{
	if (activecpu_get_pc()==0x6018642) cpu_spinuntil_int(); /* after you enable the sound cpu ...*/
	return stv_workram_h[0x0b9228/4];
}


DRIVER_INIT(prikura)
{
/*
 06018640: MOV.B   @R14,R0  // 60b9228
 06018642: TST     R0,R0
 06018644: BF      $06018640

    (loops for 263473 instructions)
*/
	install_mem_read32_handler(0, 0x60b9228, 0x60b922b, prikura_speedup_r );

	init_stv();
}


static READ32_HANDLER( hanagumi_speedup_r )
{
	if (activecpu_get_pc()==0x06010162) cpu_spinuntil_int(); /* title logos*/

	return stv_workram_h[0x94188/4];
}

static READ32_HANDLER( hanagumi_slave_off )
{
	/* just turn the slave off, i don't think the game needs it */
	cpu_set_halt_line(1,ASSERT_LINE);

	return stv_workram_h[0x015438/4];
}

DRIVER_INIT(hanagumi)
{
/*
	06013E1E: NOP
	0601015E: MOV.L   @($6C,PC),R3
	06010160: MOV.L   @R3,R0  (6094188)
	06010162: TST     R0,R0
	06010164: BT      $0601015A
	0601015A: JSR     R14
	0601015C: NOP
	06013E20: MOV.L   @($34,PC),R3
	06013E22: MOV.B   @($00,R3),R0
	06013E24: TST     R0,R0
	06013E26: BT      $06013E1C
	06013E1C: RTS
	06013E1E: NOP

   (loops for 288688 instructions)
*/
   	install_mem_read32_handler(0, 0x6094188, 0x609418b, hanagumi_speedup_r );
   	install_mem_read32_handler(1, 0x6015438, 0x601543b, hanagumi_slave_off );

  	init_stv();
}




/* these idle loops might change if the interrupts change / are fixed because i don't really think these are vblank waits... */

/* puyosun

CPU0: Aids Screen

06021CF0: MOV.B   @($13,GBR),R0 (60ffc13)
06021CF2: CMP/PZ  R0
06021CF4: BT      $06021CF0
   (loops for xxx instructions)

   this is still very slow .. but i don't think it can be sped up further


*/

static READ32_HANDLER( puyosun_speedup_r )
{
	if (activecpu_get_pc()==0x6021CF2) cpu_spinuntil_time(TIME_IN_USEC(400)); /* spinuntilint breaks controls again .. urgh*/


	return stv_workram_h[0x0ffc10/4];
}

static READ32_HANDLER( puyosun_speedup2_r )
{
	/* this is read when the opcode is read .. we can't do much else because the only
	 thing the loop checks is an internal sh2 register */

	if (activecpu_get_pc()==0x6023702) cpu_spinuntil_time(TIME_IN_USEC(2000));

/*	logerror ("Ugly %08x\n", activecpu_get_pc());*/

	return stv_workram_h[0x023700/4];
}

DRIVER_INIT(puyosun)
{
   	install_mem_read32_handler(0, 0x60ffc10, 0x60ffc13, puyosun_speedup_r ); /* idle loop of main cpu*/
   	install_mem_read32_handler(1, 0x6023700, 0x6023703, puyosun_speedup2_r ); /* UGLY hack for second cpu*/

	init_ic13();
}

/* mausuke

CPU0 Data East Logo:
060461A0: MOV.B   @($13,GBR),R0  (60ffc13)
060461A2: CMP/PZ  R0
060461A4: BT      $060461A0
   (loops for 232602 instructions)

*/

static READ32_HANDLER( mausuke_speedup_r )
{
	if (activecpu_get_pc()==0x060461A2) cpu_spinuntil_time(TIME_IN_USEC(20)); /* spinuntilint breaks controls again .. urgh*/

	return stv_workram_h[0x0ffc10/4];
}

DRIVER_INIT(mausuke)
{
   	install_mem_read32_handler(0, 0x60ffc10, 0x60ffc13, mausuke_speedup_r ); /* idle loop of main cpu*/

	init_ic13();
}

static READ32_HANDLER( cottonbm_speedup_r )
{
	if (activecpu_get_pc()==0x06030EE4) cpu_spinuntil_time(TIME_IN_USEC(20)); /* spinuntilint breaks lots of things*/

	return stv_workram_h[0x0ffc10/4];
}

static READ32_HANDLER( cottonbm_speedup2_r )
{
	/* this is read when the opcode is read .. we can't do much else because the only
	 thing the loop checks is an internal sh2 register

	 it will fill the log with cpu #1 (PC=06032B52): warning - op-code execute on mapped I/O :/
	 */

	if (activecpu_get_pc()==0x6032b52)
	{
		if (
		   (stv_workram_h[0x0ffc44/4] != 0x260fbe34) &&
		   (stv_workram_h[0x0ffc48/4] != 0x260fbe34) &&
		   (stv_workram_h[0x0ffc44/4] != 0x260fbe2c) &&
		   (stv_workram_h[0x0ffc48/4] != 0x260fbe2c)
		   )
		{
			log_cb(RETRO_LOG_DEBUG, LOGPRE "cpu1 skip %08x %08x\n",stv_workram_h[0x0ffc44/4],stv_workram_h[0x0ffc48/4]);

			cpu_spinuntil_time(TIME_IN_USEC(200));
		}

	}

/*	logerror ("Ugly %08x\n", activecpu_get_pc());*/

	return stv_workram_h[0x032b50/4];
}

DRIVER_INIT(cottonbm)
{
   	install_mem_read32_handler(0, 0x60ffc10, 0x60ffc13, cottonbm_speedup_r ); /* idle loop of main cpu*/
   	install_mem_read32_handler(1, 0x6032b50, 0x6032b53, cottonbm_speedup2_r ); /* UGLY hack for second cpu*/

	init_stv();
}

static READ32_HANDLER( cotton2_speedup_r )
{
	if (activecpu_get_pc()==0x06031c7c) cpu_spinuntil_time(TIME_IN_USEC(20)); /* spinuntilint breaks lots of things*/

	return stv_workram_h[0x0ffc10/4];
}

static READ32_HANDLER( cotton2_speedup2_r )
{
	/* this is read when the opcode is read .. we can't do much else because the only
	 thing the loop checks is an internal sh2 register

	 it will fill the log with cpu #1 (PC=xxx): warning - op-code execute on mapped I/O :/
	 */

	if (activecpu_get_pc()==0x60338ec)
	{
		if (
		   (stv_workram_h[0x0ffc44/4] != 0x260fd264) &&
		   (stv_workram_h[0x0ffc48/4] != 0x260fd264) &&
		   (stv_workram_h[0x0ffc44/4] != 0x260fd25c) &&
		   (stv_workram_h[0x0ffc48/4] != 0x260fd25c)
		   )
		{
			log_cb(RETRO_LOG_DEBUG, LOGPRE "cpu1 skip %08x %08x\n",stv_workram_h[0x0ffc44/4],stv_workram_h[0x0ffc48/4]);

			cpu_spinuntil_time(TIME_IN_USEC(200));
		}

	}

/*	logerror ("Ugly %08x\n", activecpu_get_pc());*/

	return stv_workram_h[0x0338ec/4];
}

DRIVER_INIT(cotton2)
{
   	install_mem_read32_handler(0, 0x60ffc10, 0x60ffc13, cotton2_speedup_r ); /* idle loop of main cpu*/
   	install_mem_read32_handler(1, 0x60338ec, 0x60338ef, cotton2_speedup2_r ); /* UGLY hack for second cpu*/

	init_stv();
}

static READ32_HANDLER( dnmtdeka_speedup_r )
{
	if (activecpu_get_pc()==0x6027c93) cpu_spinuntil_time(TIME_IN_USEC(20));

	return stv_workram_h[0x0985a0/4];
}


DRIVER_INIT(dnmtdeka)
{
/*   	install_mem_read32_handler(0, 0x60985a0, 0x60985a3, dnmtdeka_speedup_r ); */ /* idle loop of main cpu*/

	init_ic13();
}


static READ32_HANDLER( fhboxers_speedup_r )
{
	if (activecpu_get_pc()==0x060041c4) cpu_spinuntil_time(TIME_IN_USEC(20));

	return stv_workram_h[0x00420c/4];
}

static READ32_HANDLER( fhboxers_speedup2_r )
{
	if (activecpu_get_pc()==0x0600bb0c) cpu_spinuntil_time(TIME_IN_USEC(20));


	return stv_workram_h[0x090740/4];
}

/* fhboxers ... doesn't seem to work properly anyway .. even without the speedups, timing issues / interrupt order .. who knows */

DRIVER_INIT(fhboxers)
{
   	install_mem_read32_handler(0, 0x600420c, 0x600420f, fhboxers_speedup_r ); /* idle loop of main cpu*/
   	install_mem_read32_handler(1, 0x6090740, 0x6090743, fhboxers_speedup2_r ); /* idle loop of second cpu*/

	init_ic13();
}


static READ32_HANDLER( bakubaku_speedup_r )
{
	if (activecpu_get_pc()==0x06036dc8) cpu_spinuntil_int(); /* title logos*/

	return stv_workram_h[0x0833f0/4];
}

static READ32_HANDLER( bakubaku_speedup2_r )
{
	if (activecpu_get_pc()==0x06033762) 	cpu_set_halt_line(1,ASSERT_LINE);

	return stv_workram_h[0x0033762/4];
}

static READ32_HANDLER( bakubaku_hangskip_r )
{
	if (activecpu_get_pc()==0x060335e4) stv_workram_h[0x0335e6/4] = 0x32300909;

	return stv_workram_h[0x033660/4];
}

DRIVER_INIT(bakubaku)
{
   	install_mem_read32_handler(0, 0x60833f0, 0x60833f3, bakubaku_speedup_r ); /* idle loop of main cpu*/
   	install_mem_read32_handler(1, 0x60fdfe8, 0x60fdfeb, bakubaku_speedup2_r ); /* turn off slave sh2, is it needed after boot ??*/
   	install_mem_read32_handler(0, 0x6033660, 0x6033663, bakubaku_hangskip_r ); /* it waits for a ram address to chamge what should change it?*/

	init_ic13();
}
