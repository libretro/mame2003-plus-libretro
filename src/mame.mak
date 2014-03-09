# uncomment the following lines to include a CPU core
CPUS+=Z80@
CPUS+=Z180@
CPUS+=8080@
CPUS+=8085A@
CPUS+=M6502@
CPUS+=M65C02@
#CPUS+=M65SC02@
#CPUS+=M65CE02@
#CPUS+=M6509@
CPUS+=M6510@
#CPUS+=M6510T@
#CPUS+=M7501@
#CPUS+=M8502@
CPUS+=N2A03@
CPUS+=DECO16@
#CPUS+=M4510@
CPUS+=H6280@
CPUS+=I86@
#CPUS+=I88@
CPUS+=I186@
#CPUS+=I188@
#CPUS+=I286@
CPUS+=V20@
CPUS+=V30@
CPUS+=V33@
CPUS+=V60@
CPUS+=V70@
CPUS+=I8035@
CPUS+=I8039@
CPUS+=I8048@
CPUS+=N7751@
CPUS+=I8X41@
CPUS+=M6800@
CPUS+=M6801@
CPUS+=M6802@
CPUS+=M6803@
CPUS+=M6808@
CPUS+=HD63701@
CPUS+=NSC8105@
CPUS+=M6805@
CPUS+=M68705@
CPUS+=HD63705@
CPUS+=HD6309@
CPUS+=M6809@
CPUS+=KONAMI@
CPUS+=M68000@
CPUS+=M68010@
CPUS+=M68EC020@
CPUS+=M68020@
CPUS+=T11@
CPUS+=S2650@
CPUS+=TMS34010@
CPUS+=TMS34020@
#CPUS+=TMS9900@
#CPUS+=TMS9940@
CPUS+=TMS9980@
#CPUS+=TMS9985@
#CPUS+=TMS9989@
CPUS+=TMS9995@
#CPUS+=TMS99105A@
#CPUS+=TMS99110A@
CPUS+=Z8000@
CPUS+=TMS32010@
CPUS+=TMS32025@
CPUS+=TMS32031@
CPUS+=CCPU@
CPUS+=ADSP2100@
CPUS+=ADSP2101@
CPUS+=ADSP2104@
CPUS+=ADSP2105@
CPUS+=ADSP2115@
CPUS+=PSXCPU@
CPUS+=ASAP@
CPUS+=UPD7810@
CPUS+=UPD7807@
CPUS+=ARM@
CPUS+=JAGUAR@
CPUS+=R3000@
CPUS+=R4600@
CPUS+=R5000@
CPUS+=SH2@
CPUS+=DSP32C@
#CPUS+=PIC16C54@
CPUS+=PIC16C55@
#CPUS+=PIC16C56@
CPUS+=PIC16C57@
#CPUS+=PIC16C58@
CPUS+=G65816@
CPUS+=SPC700@
CPUS+=E132XS@

# uncomment the following lines to include a sound core
SOUNDS+=CUSTOM@
SOUNDS+=SAMPLES@
SOUNDS+=DAC@
SOUNDS+=DISCRETE@
SOUNDS+=AY8910@
SOUNDS+=YM2203@
# enable only one of the following two
#SOUNDS+=YM2151@
SOUNDS+=YM2151_ALT@
SOUNDS+=YM2608@
SOUNDS+=YM2610@
SOUNDS+=YM2610B@
SOUNDS+=YM2612@
SOUNDS+=YM3438@
SOUNDS+=YM2413@
SOUNDS+=YM3812@
SOUNDS+=YMZ280B@
SOUNDS+=YM3526@
SOUNDS+=Y8950@
SOUNDS+=SN76477@
SOUNDS+=SN76496@
SOUNDS+=POKEY@
SOUNDS+=TIA@
SOUNDS+=NES@
SOUNDS+=ASTROCADE@
SOUNDS+=NAMCO@
SOUNDS+=NAMCONA@
SOUNDS+=TMS36XX@
SOUNDS+=TMS5110@
SOUNDS+=TMS5220@
SOUNDS+=VLM5030@
SOUNDS+=ADPCM@
SOUNDS+=OKIM6295@
SOUNDS+=MSM5205@
SOUNDS+=MSM5232@
SOUNDS+=UPD7759@
SOUNDS+=HC55516@
SOUNDS+=K005289@
SOUNDS+=K007232@
SOUNDS+=K051649@
SOUNDS+=K053260@
SOUNDS+=K054539@
SOUNDS+=SEGAPCM@
SOUNDS+=RF5C68@
SOUNDS+=CEM3394@
SOUNDS+=C140@
SOUNDS+=QSOUND@
SOUNDS+=SAA1099@
SOUNDS+=IREMGA20@
SOUNDS+=ES5505@
SOUNDS+=ES5506@
SOUNDS+=BSMT2000@
SOUNDS+=YMF262@
SOUNDS+=YMF278B@
SOUNDS+=GAELCO_CG1V@
SOUNDS+=GAELCO_GAE1@
SOUNDS+=X1_010@
SOUNDS+=MULTIPCM@
SOUNDS+=C6280@
SOUNDS+=SP0250@
SOUNDS+=SCSP@
SOUNDS+=YMF271@
SOUNDS+=PSXSPU@


#DRVLIBS = \
	$(OBJ)/pacman.a $(OBJ)/epos.a $(OBJ)/nichibut.a \
	$(OBJ)/phoenix.a $(OBJ)/namco.a $(OBJ)/univers.a $(OBJ)/nintendo.a \
	$(OBJ)/midw8080.a $(OBJ)/meadows.a $(OBJ)/cvs.a $(OBJ)/midway.a \
	$(OBJ)/irem.a $(OBJ)/gottlieb.a $(OBJ)/taito.a $(OBJ)/toaplan.a $(OBJ)/cave.a \
	$(OBJ)/kyugo.a $(OBJ)/williams.a $(OBJ)/gremlin.a $(OBJ)/vicdual.a \
	$(OBJ)/capcom.a $(OBJ)/itech.a $(OBJ)/leland.a $(OBJ)/sega.a $(OBJ)/deniam.a \
	$(OBJ)/dataeast.a $(OBJ)/tehkan.a $(OBJ)/konami.a \
	$(OBJ)/exidy.a $(OBJ)/atari.a $(OBJ)/snk.a $(OBJ)/alpha.a $(OBJ)/technos.a \
	$(OBJ)/stern.a $(OBJ)/gameplan.a $(OBJ)/zaccaria.a \
	$(OBJ)/upl.a $(OBJ)/nmk.a $(OBJ)/cinemar.a $(OBJ)/cinemav.a \
	$(OBJ)/thepit.a $(OBJ)/valadon.a $(OBJ)/seibu.a $(OBJ)/tad.a $(OBJ)/jaleco.a \
	$(OBJ)/vsystem.a $(OBJ)/psikyo.a $(OBJ)/orca.a $(OBJ)/gaelco.a \
	$(OBJ)/kaneko.a $(OBJ)/seta.a $(OBJ)/atlus.a \
	$(OBJ)/sun.a $(OBJ)/suna.a $(OBJ)/dooyong.a $(OBJ)/tong.a \
	$(OBJ)/comad.a $(OBJ)/playmark.a $(OBJ)/pacific.a $(OBJ)/tecfri.a \
	$(OBJ)/metro.a $(OBJ)/venture.a $(OBJ)/yunsung.a $(OBJ)/zilec.a \
	$(OBJ)/fuuki.a $(OBJ)/unico.a $(OBJ)/afega.a $(OBJ)/esd.a $(OBJ)/dynax.a \
	$(OBJ)/sigma.a $(OBJ)/igs.a $(OBJ)/ramtek.a $(OBJ)/omori.a $(OBJ)/tch.a \
	$(OBJ)/usgames.a $(OBJ)/sanritsu.a $(OBJ)/rare.a \
	$(OBJ)/nihonsys.a $(OBJ)/alba.a $(OBJ)/homedata.a $(OBJ)/artmagic.a \
	$(OBJ)/taiyo.a $(OBJ)/other.a $(OBJ)/neogeo.a \

COREOBJS += \
	$(OBJ)/drivers/pacman.o $(OBJ)/machine/mspacman.o \
	$(OBJ)/machine/pacplus.o $(OBJ)/machine/jumpshot.o \
	$(OBJ)/machine/theglobp.o \
	$(OBJ)/machine/acitya.o \
	$(OBJ)/drivers/jrpacman.o $(OBJ)/vidhrdw/jrpacman_vidhrdw.o \
	$(OBJ)/vidhrdw/pengo_vidhrdw.o $(OBJ)/drivers/pengo.o \

COREOBJS += \
	$(OBJ)/drivers/epos.o $(OBJ)/vidhrdw/epos_vidhrdw.o \

COREOBJS += \
	$(OBJ)/vidhrdw/cclimber_vidhrdw.o $(OBJ)/sndhrdw/cclimber_sndhrdw.o $(OBJ)/drivers/cclimber.o \
	$(OBJ)/drivers/yamato.o \
	$(OBJ)/vidhrdw/gomoku_vidhrdw.o $(OBJ)/sndhrdw/gomoku_sndhrdw.o $(OBJ)/drivers/gomoku.o \
	$(OBJ)/vidhrdw/wiping_vidhrdw.o $(OBJ)/sndhrdw/wiping_sndhrdw.o $(OBJ)/drivers/wiping.o \
	$(OBJ)/vidhrdw/seicross_vidhrdw.o $(OBJ)/drivers/seicross.o \
	$(OBJ)/vidhrdw/clshroad_vidhrdw.o $(OBJ)/drivers/clshroad.o \
	$(OBJ)/vidhrdw/tubep_vidhrdw.o $(OBJ)/drivers/tubep.o \
	$(OBJ)/vidhrdw/magmax_vidhrdw.o $(OBJ)/drivers/magmax.o \
	$(OBJ)/vidhrdw/cop01_vidhrdw.o $(OBJ)/drivers/cop01.o \
	$(OBJ)/vidhrdw/terracre_vidhrdw.o $(OBJ)/drivers/terracre.o \
	$(OBJ)/vidhrdw/galivan_vidhrdw.o $(OBJ)/drivers/galivan.o \
	$(OBJ)/vidhrdw/armedf_vidhrdw.o $(OBJ)/drivers/armedf.o \
	$(OBJ)/machine/nb1413m3.o \
	$(OBJ)/vidhrdw/hyhoo_vidhrdw.o $(OBJ)/drivers/hyhoo.o \
	$(OBJ)/vidhrdw/pastelgl_vidhrdw.o $(OBJ)/drivers/pastelgl.o \
	$(OBJ)/vidhrdw/nbmj8688_vidhrdw.o $(OBJ)/drivers/nbmj8688.o \
	$(OBJ)/vidhrdw/nbmj8891_vidhrdw.o $(OBJ)/drivers/nbmj8891.o \
	$(OBJ)/vidhrdw/nbmj8991_vidhrdw.o $(OBJ)/drivers/nbmj8991.o \
	$(OBJ)/vidhrdw/nbmj9195_vidhrdw.o $(OBJ)/drivers/nbmj9195.o \
	$(OBJ)/vidhrdw/niyanpai_vidhrdw.o $(OBJ)/drivers/niyanpai.o $(OBJ)/machine/m68kfmly.o \

COREOBJS += \
	$(OBJ)/drivers/safarir.o \
	$(OBJ)/vidhrdw/phoenix_vidhrdw.o $(OBJ)/sndhrdw/phoenix_sndhrdw.o $(OBJ)/drivers/phoenix.o \
	$(OBJ)/sndhrdw/pleiads.o \
	$(OBJ)/vidhrdw/naughtyb_vidhrdw.o $(OBJ)/drivers/naughtyb.o \

COREOBJS += \
	$(OBJ)/machine/geebee_machine.o $(OBJ)/vidhrdw/geebee_vidhrdw.o $(OBJ)/sndhrdw/geebee_sndhrdw.o $(OBJ)/drivers/geebee.o \
	$(OBJ)/vidhrdw/warpwarp_vidhrdw.o $(OBJ)/sndhrdw/warpwarp_sndhrdw.o $(OBJ)/drivers/warpwarp.o \
	$(OBJ)/vidhrdw/tankbatt_vidhrdw.o $(OBJ)/drivers/tankbatt.o \
	$(OBJ)/vidhrdw/galaxian_vidhrdw.o $(OBJ)/sndhrdw/galaxian_sndhrdw.o $(OBJ)/drivers/galaxian.o \
	$(OBJ)/vidhrdw/rallyx_vidhrdw.o $(OBJ)/drivers/rallyx.o \
	$(OBJ)/drivers/locomotn.o \
	$(OBJ)/machine/bosco_machine.o $(OBJ)/sndhrdw/bosco_sndhrdw.o $(OBJ)/vidhrdw/bosco_vidhrdw.o $(OBJ)/drivers/bosco.o \
	$(OBJ)/machine/galaga_machine.o $(OBJ)/vidhrdw/galaga_vidhrdw.o $(OBJ)/drivers/galaga.o \
	$(OBJ)/machine/digdug_machine.o $(OBJ)/vidhrdw/digdug_vidhrdw.o $(OBJ)/drivers/digdug.o \
	$(OBJ)/vidhrdw/xevious_vidhrdw.o $(OBJ)/machine/xevious_machine.o $(OBJ)/drivers/xevious.o \
	$(OBJ)/machine/superpac_machine.o $(OBJ)/vidhrdw/superpac_vidhrdw.o $(OBJ)/drivers/superpac.o \
	$(OBJ)/machine/phozon_machine.o $(OBJ)/vidhrdw/phozon_vidhrdw.o $(OBJ)/drivers/phozon.o \
	$(OBJ)/machine/mappy_machine.o $(OBJ)/vidhrdw/mappy_vidhrdw.o $(OBJ)/drivers/mappy.o \
	$(OBJ)/machine/grobda_machine.o $(OBJ)/vidhrdw/grobda_vidhrdw.o $(OBJ)/drivers/grobda.o \
	$(OBJ)/machine/gaplus_machine.o $(OBJ)/vidhrdw/gaplus_vidhrdw.o $(OBJ)/drivers/gaplus.o \
	$(OBJ)/machine/toypop_machine.o $(OBJ)/vidhrdw/toypop_vidhrdw.o $(OBJ)/drivers/toypop.o \
	$(OBJ)/machine/polepos_machine.o $(OBJ)/vidhrdw/polepos_vidhrdw.o $(OBJ)/sndhrdw/polepos_sndhrdw.o $(OBJ)/drivers/polepos.o \
	$(OBJ)/vidhrdw/pacland_vidhrdw.o $(OBJ)/drivers/pacland.o \
	$(OBJ)/vidhrdw/skykid_vidhrdw.o $(OBJ)/drivers/skykid.o \
	$(OBJ)/vidhrdw/baraduke_vidhrdw.o $(OBJ)/drivers/baraduke.o \
	$(OBJ)/vidhrdw/namcos86_vidhrdw.o $(OBJ)/drivers/namcos86.o \
	$(OBJ)/vidhrdw/tceptor_vidhrdw.o $(OBJ)/drivers/tceptor.o \
	$(OBJ)/machine/namcos1_machine.o $(OBJ)/vidhrdw/namcos1_vidhrdw.o $(OBJ)/drivers/namcos1.o \
	$(OBJ)/machine/namcos2_machine.o $(OBJ)/vidhrdw/namcos2_vidhrdw.o $(OBJ)/drivers/namcos2.o \
	$(OBJ)/drivers/namcoic.o \
	$(OBJ)/vidhrdw/namcona1_vidhrdw.o $(OBJ)/drivers/namcona1.o \
	$(OBJ)/vidhrdw/namconb1_vidhrdw.o $(OBJ)/drivers/namconb1.o \
	$(OBJ)/machine/namcond1_machine.o $(OBJ)/vidhrdw/ygv608.o $(OBJ)/drivers/namcond1.o \
	$(OBJ)/vidhrdw/psx_vidhrdw.o $(OBJ)/machine/psx_machine.o \
	$(OBJ)/drivers/namcos10.o \
	$(OBJ)/drivers/namcos11.o \
	$(OBJ)/drivers/namcos12.o \
	$(OBJ)/vidhrdw/namcos3d.o \
	$(OBJ)/vidhrdw/namcos21_vidhrdw.o $(OBJ)/drivers/namcos21.o \
	$(OBJ)/vidhrdw/namcos22_vidhrdw.o $(OBJ)/drivers/namcos22.o \

COREOBJS += \
	$(OBJ)/vidhrdw/cosmic_vidhrdw.o $(OBJ)/drivers/cosmic.o \
	$(OBJ)/vidhrdw/redclash_vidhrdw.o $(OBJ)/drivers/redclash.o \
	$(OBJ)/vidhrdw/ladybug_vidhrdw.o $(OBJ)/drivers/ladybug.o \
	$(OBJ)/vidhrdw/cheekyms_vidhrdw.o $(OBJ)/drivers/cheekyms.o \
	$(OBJ)/vidhrdw/mrdo_vidhrdw.o $(OBJ)/drivers/mrdo.o \
	$(OBJ)/machine/docastle_machine.o $(OBJ)/vidhrdw/docastle_vidhrdw.o $(OBJ)/drivers/docastle.o \

COREOBJS += \
	$(OBJ)/vidhrdw/dkong_vidhrdw.o $(OBJ)/sndhrdw/dkong_sndhrdw.o $(OBJ)/drivers/dkong.o \
	$(OBJ)/machine/strtheat.o \
	$(OBJ)/vidhrdw/mario_vidhrdw.o $(OBJ)/sndhrdw/mario_sndhrdw.o $(OBJ)/drivers/mario.o \
	$(OBJ)/vidhrdw/popeye_vidhrdw.o $(OBJ)/drivers/popeye.o \
	$(OBJ)/vidhrdw/punchout_vidhrdw.o $(OBJ)/drivers/punchout.o \
	$(OBJ)/machine/rp5h01.o $(OBJ)/vidhrdw/ppu2c03b.o \
	$(OBJ)/machine/playch10_machine.o $(OBJ)/vidhrdw/playch10_vidhrdw.o $(OBJ)/drivers/playch10.o \
	$(OBJ)/machine/vsnes_machine.o $(OBJ)/vidhrdw/vsnes_vidhrdw.o $(OBJ)/drivers/vsnes.o \
	$(OBJ)/machine/snes_machine.o $(OBJ)/vidhrdw/snes_vidhrdw.o \
	$(OBJ)/sndhrdw/snes_sndhrdw.o \
	$(OBJ)/drivers/nss.o \

COREOBJS += \
	$(OBJ)/machine/8080bw.o \
	$(OBJ)/vidhrdw/8080bw_vidhrdw.o $(OBJ)/sndhrdw/8080bw_sndhrdw.o $(OBJ)/drivers/8080bw_drivers.o \
	$(OBJ)/vidhrdw/sspeedr_vidhrdw.o $(OBJ)/drivers/sspeedr.o \
	$(OBJ)/vidhrdw/m79amb_vidhrdw.o $(OBJ)/drivers/m79amb.o $(OBJ)/drivers/rotaryf.o \

COREOBJS += \
	$(OBJ)/drivers/lazercmd.o $(OBJ)/vidhrdw/lazercmd_vidhrdw.o \
	$(OBJ)/drivers/meadows.o $(OBJ)/sndhrdw/meadows_sndhrdw.o $(OBJ)/vidhrdw/meadows_vidhrdw.o \

COREOBJS += \
	$(OBJ)/drivers/cvs.o $(OBJ)/vidhrdw/cvs_vidhrdw.o \
	$(OBJ)/vidhrdw/s2636_vidhrdw.o \

COREOBJS += \
	$(OBJ)/machine/astrocde_machine.o $(OBJ)/vidhrdw/astrocde_vidhrdw.o $(OBJ)/drivers/astrocde.o $(OBJ)/sndhrdw/astrocde_sndhrdw.o \
	$(OBJ)/sndhrdw/gorf.o \
	$(OBJ)/machine/mcr.o $(OBJ)/sndhrdw/mcr_sndhrdw.o \
	$(OBJ)/vidhrdw/mcr12_vidhrdw.o $(OBJ)/vidhrdw/mcr3_vidhrdw.o \
	$(OBJ)/drivers/mcr1.o $(OBJ)/drivers/mcr2.o $(OBJ)/drivers/mcr3.o \
	$(OBJ)/vidhrdw/mcr68_vidhrdw.o $(OBJ)/drivers/mcr68.o \
	$(OBJ)/vidhrdw/balsente_vidhrdw.o $(OBJ)/machine/balsente_machine.o $(OBJ)/drivers/balsente.o \
	$(OBJ)/vidhrdw/gridlee_vidhrdw.o $(OBJ)/sndhrdw/gridlee_sndhrdw.o $(OBJ)/drivers/gridlee.o \
	$(OBJ)/drivers/seattle.o $(OBJ)/vidhrdw/voodoo_vidhrdw.o \
	$(OBJ)/vidhrdw/exterm_vidhrdw.o $(OBJ)/drivers/exterm.o \
	$(OBJ)/machine/midwayic.o $(OBJ)/sndhrdw/dcs.o \
	$(OBJ)/machine/midyunit_machine.o $(OBJ)/vidhrdw/midyunit_vidhrdw.o $(OBJ)/drivers/midyunit.o \
	$(OBJ)/drivers/midxunit.o \
	$(OBJ)/machine/midwunit_machine.o $(OBJ)/drivers/midwunit.o \
	$(OBJ)/vidhrdw/midvunit_vidhrdw.o $(OBJ)/drivers/midvunit.o \
	$(OBJ)/machine/midtunit_machine.o $(OBJ)/vidhrdw/midtunit_vidhrdw.o $(OBJ)/drivers/midtunit.o \

COREOBJS += \
	$(OBJ)/vidhrdw/skychut_vidhrdw.o $(OBJ)/drivers/skychut.o \
	$(OBJ)/drivers/olibochu.o \
	$(OBJ)/sndhrdw/irem.o \
	$(OBJ)/vidhrdw/mpatrol_vidhrdw.o $(OBJ)/drivers/mpatrol.o \
	$(OBJ)/vidhrdw/troangel_vidhrdw.o $(OBJ)/drivers/troangel.o \
	$(OBJ)/vidhrdw/yard_vidhrdw.o $(OBJ)/drivers/yard.o \
	$(OBJ)/vidhrdw/travrusa_vidhrdw.o $(OBJ)/drivers/travrusa.o \
	$(OBJ)/drivers/wilytowr.o \
	$(OBJ)/vidhrdw/m62_vidhrdw.o $(OBJ)/drivers/m62.o \
	$(OBJ)/vidhrdw/vigilant_vidhrdw.o $(OBJ)/drivers/vigilant.o \
	$(OBJ)/vidhrdw/m72_vidhrdw.o $(OBJ)/sndhrdw/m72_sndhrdw.o $(OBJ)/drivers/m72.o \
	$(OBJ)/vidhrdw/shisen_vidhrdw.o $(OBJ)/drivers/shisen.o \
	$(OBJ)/machine/irem_cpu.o \
	$(OBJ)/vidhrdw/m90_vidhrdw.o $(OBJ)/drivers/m90.o \
	$(OBJ)/vidhrdw/m92_vidhrdw.o $(OBJ)/drivers/m92.o \
	$(OBJ)/vidhrdw/m107_vidhrdw.o $(OBJ)/drivers/m107.o \

COREOBJS += \
	$(OBJ)/vidhrdw/gottlieb_vidhrdw.o $(OBJ)/sndhrdw/gottlieb_sndhrdw.o $(OBJ)/drivers/gottlieb.o \

COREOBJS += \
	$(OBJ)/drivers/sbowling.o \
	$(OBJ)/machine/chaknpop_machine.o $(OBJ)/vidhrdw/chaknpop_vidhrdw.o $(OBJ)/drivers/chaknpop.o \
	$(OBJ)/machine/qix_machine.o $(OBJ)/vidhrdw/qix_vidhrdw.o $(OBJ)/drivers/qix.o \
	$(OBJ)/machine/taitosj_machine.o $(OBJ)/vidhrdw/taitosj_vidhrdw.o $(OBJ)/drivers/taitosj.o \
	$(OBJ)/machine/grchamp_machine.o $(OBJ)/vidhrdw/grchamp_vidhrdw.o $(OBJ)/drivers/grchamp.o \
	$(OBJ)/machine/pitnrun_machine.o $(OBJ)/vidhrdw/pitnrun_vidhrdw.o $(OBJ)/drivers/pitnrun.o \
	$(OBJ)/drivers/marinedt.o \
	$(OBJ)/vidhrdw/crbaloon_vidhrdw.o $(OBJ)/drivers/crbaloon.o \
	$(OBJ)/vidhrdw/bking2_vidhrdw.o $(OBJ)/drivers/bking2.o \
	$(OBJ)/vidhrdw/gsword_vidhrdw.o $(OBJ)/drivers/gsword.o $(OBJ)/machine/tait8741.o \
	$(OBJ)/vidhrdw/msisaac_vidhrdw.o $(OBJ)/drivers/msisaac.o \
	$(OBJ)/machine/retofinv_machine.o $(OBJ)/vidhrdw/retofinv_vidhrdw.o $(OBJ)/drivers/retofinv.o \
	$(OBJ)/vidhrdw/rollrace_vidhrdw.o $(OBJ)/drivers/rollrace.o \
	$(OBJ)/vidhrdw/40love_vidhrdw.o $(OBJ)/drivers/40love.o \
	$(OBJ)/vidhrdw/ssrj_vidhrdw.o $(OBJ)/drivers/ssrj.o \
	$(OBJ)/machine/bigevglf_machine.o $(OBJ)/vidhrdw/bigevglf_vidhrdw.o $(OBJ)/drivers/bigevglf.o \
	$(OBJ)/vidhrdw/tsamurai_vidhrdw.o $(OBJ)/drivers/tsamurai.o \
	$(OBJ)/machine/flstory_machine.o $(OBJ)/vidhrdw/flstory_vidhrdw.o $(OBJ)/drivers/flstory.o \
	$(OBJ)/vidhrdw/gladiatr_vidhrdw.o $(OBJ)/drivers/gladiatr.o \
	$(OBJ)/machine/nycaptor_machine.o $(OBJ)/vidhrdw/nycaptor_vidhrdw.o $(OBJ)/drivers/nycaptor.o \
	$(OBJ)/drivers/halleys.o \
	$(OBJ)/machine/lsasquad_machine.o $(OBJ)/vidhrdw/lsasquad_vidhrdw.o $(OBJ)/drivers/lsasquad.o \
	$(OBJ)/machine/bublbobl_machine.o $(OBJ)/vidhrdw/bublbobl_vidhrdw.o $(OBJ)/drivers/bublbobl.o \
	$(OBJ)/drivers/missb2.o \
	$(OBJ)/machine/mexico86_machine.o $(OBJ)/vidhrdw/mexico86_vidhrdw.o $(OBJ)/drivers/mexico86.o \
	$(OBJ)/vidhrdw/darius_vidhrdw.o $(OBJ)/drivers/darius.o \
	$(OBJ)/vidhrdw/rastan_vidhrdw.o $(OBJ)/sndhrdw/rastan_sndhrdw.o $(OBJ)/drivers/rastan.o \
	$(OBJ)/machine/rainbow_machine.o $(OBJ)/drivers/rainbow.o \
	$(OBJ)/drivers/opwolf.o \
	$(OBJ)/vidhrdw/othunder_vidhrdw.o $(OBJ)/drivers/othunder.o \
	$(OBJ)/vidhrdw/topspeed_vidhrdw.o $(OBJ)/drivers/topspeed.o \
	$(OBJ)/machine/arkanoid_machine.o $(OBJ)/vidhrdw/arkanoid_vidhrdw.o $(OBJ)/drivers/arkanoid.o \
	$(OBJ)/vidhrdw/superqix_vidhrdw.o $(OBJ)/drivers/superqix.o \
	$(OBJ)/vidhrdw/pbillian_vidhrdw.o $(OBJ)/drivers/pbillian.o $(OBJ)/sndhrdw/pbillian_sndhrdw.o \
	$(OBJ)/vidhrdw/exzisus_vidhrdw.o $(OBJ)/drivers/exzisus.o \
	$(OBJ)/drivers/taito_x.o $(OBJ)/machine/cchip.o \
	$(OBJ)/vidhrdw/minivadr_vidhrdw.o $(OBJ)/drivers/minivadr.o \
	$(OBJ)/drivers/ashnojoe.o $(OBJ)/vidhrdw/ashnojoe_vidhrdw.o \
	$(OBJ)/machine/volfied_machine.o $(OBJ)/vidhrdw/volfied_vidhrdw.o $(OBJ)/drivers/volfied.o \
	$(OBJ)/machine/bonzeadv.o $(OBJ)/vidhrdw/asuka_vidhrdw.o $(OBJ)/drivers/asuka.o \
	$(OBJ)/vidhrdw/wgp_vidhrdw.o $(OBJ)/drivers/wgp.o \
	$(OBJ)/vidhrdw/slapshot_vidhrdw.o $(OBJ)/drivers/slapshot.o \
	$(OBJ)/vidhrdw/ninjaw_vidhrdw.o $(OBJ)/drivers/ninjaw.o \
	$(OBJ)/vidhrdw/warriorb_vidhrdw.o $(OBJ)/drivers/warriorb.o \
	$(OBJ)/machine/tnzs_machine.o $(OBJ)/vidhrdw/tnzs_vidhrdw.o $(OBJ)/drivers/tnzs.o \
	$(OBJ)/machine/buggychl_machine.o $(OBJ)/vidhrdw/buggychl_vidhrdw.o $(OBJ)/drivers/buggychl.o \
	$(OBJ)/machine/lkage_machine.o $(OBJ)/vidhrdw/lkage_vidhrdw.o $(OBJ)/drivers/lkage.o \
	$(OBJ)/vidhrdw/taitoic.o $(OBJ)/sndhrdw/taitosnd.o \
	$(OBJ)/vidhrdw/taito_l_vidhrdw.o $(OBJ)/drivers/taito_l.o \
	$(OBJ)/vidhrdw/taito_h_vidhrdw.o $(OBJ)/drivers/taito_h.o \
	$(OBJ)/vidhrdw/taito_b_vidhrdw.o $(OBJ)/drivers/taito_b.o \
	$(OBJ)/vidhrdw/taito_z_vidhrdw.o $(OBJ)/drivers/taito_z.o \
	$(OBJ)/vidhrdw/gunbustr_vidhrdw.o $(OBJ)/drivers/gunbustr.o \
	$(OBJ)/vidhrdw/superchs_vidhrdw.o $(OBJ)/drivers/superchs.o \
	$(OBJ)/vidhrdw/undrfire_vidhrdw.o $(OBJ)/drivers/undrfire.o \
	$(OBJ)/vidhrdw/groundfx_vidhrdw.o $(OBJ)/drivers/groundfx.o \
	$(OBJ)/vidhrdw/taito_f2_vidhrdw.o $(OBJ)/drivers/taito_f2.o \
	$(OBJ)/vidhrdw/taito_f3_vidhrdw.o $(OBJ)/sndhrdw/taito_f3_sndhrdw.o $(OBJ)/drivers/taito_f3.o \
	$(OBJ)/vidhrdw/taitoair_vidhrdw.o $(OBJ)/drivers/taitoair.o \

COREOBJS += \
	$(OBJ)/machine/slapfght_machine.o $(OBJ)/vidhrdw/slapfght_vidhrdw.o $(OBJ)/drivers/slapfght.o \
	$(OBJ)/machine/twincobr_machine.o $(OBJ)/vidhrdw/twincobr_vidhrdw.o $(OBJ)/drivers/twincobr.o \
	$(OBJ)/drivers/wardner.o \
	$(OBJ)/drivers/mjsister.o $(OBJ)/vidhrdw/mjsister_vidhrdw.o \
	$(OBJ)/machine/toaplan1_machine.o $(OBJ)/vidhrdw/toaplan1_vidhrdw.o $(OBJ)/drivers/toaplan1.o \
	$(OBJ)/vidhrdw/snowbros_vidhrdw.o $(OBJ)/drivers/snowbros.o \
	$(OBJ)/vidhrdw/toaplan2_vidhrdw.o $(OBJ)/drivers/toaplan2.o \

COREOBJS += \
	$(OBJ)/vidhrdw/cave_vidhrdw.o $(OBJ)/drivers/cave.o \

COREOBJS += \
	$(OBJ)/vidhrdw/kyugo_vidhrdw.o $(OBJ)/machine/kyugo_machine.o $(OBJ)/drivers/kyugo.o \

COREOBJS += \
	$(OBJ)/machine/williams_machine.o $(OBJ)/vidhrdw/williams_vidhrdw.o $(OBJ)/sndhrdw/williams_sndhrdw.o $(OBJ)/drivers/williams.o \

COREOBJS += \
	$(OBJ)/vidhrdw/vulgus_vidhrdw.o $(OBJ)/drivers/vulgus.o \
	$(OBJ)/vidhrdw/sonson_vidhrdw.o $(OBJ)/drivers/sonson.o \
	$(OBJ)/vidhrdw/higemaru_vidhrdw.o $(OBJ)/drivers/higemaru.o \
	$(OBJ)/vidhrdw/1942_vidhrdw.o $(OBJ)/drivers/1942.o \
	$(OBJ)/vidhrdw/exedexes_vidhrdw.o $(OBJ)/drivers/exedexes.o \
	$(OBJ)/vidhrdw/commando_vidhrdw.o $(OBJ)/drivers/commando.o \
	$(OBJ)/vidhrdw/gng_vidhrdw.o $(OBJ)/drivers/gng.o \
	$(OBJ)/vidhrdw/gunsmoke_vidhrdw.o $(OBJ)/drivers/gunsmoke.o \
	$(OBJ)/vidhrdw/srumbler_vidhrdw.o $(OBJ)/drivers/srumbler.o \
	$(OBJ)/vidhrdw/lwings_vidhrdw.o $(OBJ)/drivers/lwings.o \
	$(OBJ)/vidhrdw/sidearms_vidhrdw.o $(OBJ)/drivers/sidearms.o \
	$(OBJ)/vidhrdw/bionicc_vidhrdw.o $(OBJ)/drivers/bionicc.o \
	$(OBJ)/vidhrdw/1943_vidhrdw.o $(OBJ)/drivers/1943.o \
	$(OBJ)/vidhrdw/blktiger_vidhrdw.o $(OBJ)/drivers/blktiger.o \
	$(OBJ)/vidhrdw/tigeroad_vidhrdw.o $(OBJ)/drivers/tigeroad.o \
	$(OBJ)/vidhrdw/lastduel_vidhrdw.o $(OBJ)/drivers/lastduel.o \
	$(OBJ)/vidhrdw/sf1_vidhrdw.o $(OBJ)/drivers/sf1.o \
	$(OBJ)/machine/kabuki.o \
	$(OBJ)/vidhrdw/mitchell_vidhrdw.o $(OBJ)/drivers/mitchell.o \
	$(OBJ)/vidhrdw/cbasebal_vidhrdw.o $(OBJ)/drivers/cbasebal.o \
	$(OBJ)/vidhrdw/cps1_vidhrdw.o $(OBJ)/drivers/cps1.o $(OBJ)/drivers/cps2.o \
	$(OBJ)/drivers/zn.o \

COREOBJS += \
	$(OBJ)/vidhrdw/tms34061_vidhrdw.o \
	$(OBJ)/machine/capbowl_machine.o $(OBJ)/vidhrdw/capbowl_vidhrdw.o $(OBJ)/drivers/capbowl.o \
	$(OBJ)/vidhrdw/itech8_vidhrdw.o $(OBJ)/drivers/itech8.o \
	$(OBJ)/vidhrdw/itech32_vidhrdw.o $(OBJ)/drivers/itech32.o \
	$(OBJ)/machine/slikshot.o \

COREOBJS += \
	$(OBJ)/vidhrdw/blockade_vidhrdw.o $(OBJ)/drivers/blockade.o \

COREOBJS += \
	$(OBJ)/vidhrdw/vicdual_vidhrdw.o $(OBJ)/drivers/vicdual.o \
	$(OBJ)/sndhrdw/carnival.o $(OBJ)/sndhrdw/depthch.o $(OBJ)/sndhrdw/invinco.o $(OBJ)/sndhrdw/pulsar.o \

COREOBJS += \
	$(OBJ)/machine/segacrpt.o $(OBJ)/sndhrdw/segasnd.o \
	$(OBJ)/vidhrdw/sega_vidhrdw.o $(OBJ)/sndhrdw/sega_sndhrdw.o $(OBJ)/machine/sega_machine.o $(OBJ)/drivers/sega.o \
	$(OBJ)/vidhrdw/segar_vidhrdw.o $(OBJ)/sndhrdw/segar_sndhrdw.o $(OBJ)/machine/segar_machine.o $(OBJ)/drivers/segar.o \
	$(OBJ)/vidhrdw/tms9928a.o $(OBJ)/drivers/sg1000a.o \
	$(OBJ)/vidhrdw/zaxxon_vidhrdw.o $(OBJ)/sndhrdw/zaxxon_sndhrdw.o $(OBJ)/drivers/zaxxon.o \
	$(OBJ)/machine/turbo_machine.o $(OBJ)/vidhrdw/turbo_vidhrdw.o $(OBJ)/drivers/turbo.o \
	$(OBJ)/drivers/kopunch.o $(OBJ)/vidhrdw/kopunch_vidhrdw.o \
	$(OBJ)/vidhrdw/suprloco_vidhrdw.o $(OBJ)/drivers/suprloco.o \
	$(OBJ)/vidhrdw/dotrikun_vidhrdw.o $(OBJ)/drivers/dotrikun.o \
	$(OBJ)/vidhrdw/angelkds_vidhrdw.o $(OBJ)/drivers/angelkds.o \
	$(OBJ)/vidhrdw/system1_vidhrdw.o $(OBJ)/drivers/system1.o \
	$(OBJ)/vidhrdw/segasyse_vidhrdw.o $(OBJ)/drivers/segasyse.o \
	$(OBJ)/machine/system16_machine.o $(OBJ)/vidhrdw/system16_vidhrdw.o $(OBJ)/vidhrdw/sys16spr.o \
	$(OBJ)/sndhrdw/system16_sndhrdw.o \
	$(OBJ)/drivers/system16.o $(OBJ)/drivers/aburner.o $(OBJ)/drivers/outrun.o \
	$(OBJ)/drivers/sharrier.o $(OBJ)/drivers/system18.o \
	$(OBJ)/drivers/system24.o $(OBJ)/machine/system24_machine.o $(OBJ)/vidhrdw/system24_vidhrdw.o \
	$(OBJ)/vidhrdw/segaic24.o \
	$(OBJ)/drivers/system32.o $(OBJ)/drivers/multi32.o $(OBJ)/vidhrdw/system32_vidhrdw.o \
	$(OBJ)/vidhrdw/segac2_vidhrdw.o $(OBJ)/drivers/segac2.o \
	$(OBJ)/drivers/stv.o $(OBJ)/drivers/stvhacks.o $(OBJ)/machine/stvcd.o \
	$(OBJ)/machine/scudsp.o \
	$(OBJ)/vidhrdw/stvvdp1_vidhrdw.o $(OBJ)/vidhrdw/stvvdp2_vidhrdw.o \

COREOBJS += \
	$(OBJ)/vidhrdw/deniam_vidhrdw.o $(OBJ)/drivers/deniam.o \

COREOBJS += \
	$(OBJ)/machine/btime_machine.o $(OBJ)/vidhrdw/btime_vidhrdw.o $(OBJ)/drivers/btime.o \
	$(OBJ)/machine/decocass_machine.o $(OBJ)/vidhrdw/decocass_vidhrdw.o $(OBJ)/drivers/decocass.o \
	$(OBJ)/vidhrdw/astrof_vidhrdw.o $(OBJ)/sndhrdw/astrof_sndhrdw.o $(OBJ)/drivers/astrof.o \
	$(OBJ)/vidhrdw/liberate_vidhrdw.o $(OBJ)/drivers/liberate.o \
	$(OBJ)/vidhrdw/bwing_vidhrdw.o $(OBJ)/drivers/bwing.o \
	$(OBJ)/vidhrdw/kchamp_vidhrdw.o $(OBJ)/drivers/kchamp.o \
	$(OBJ)/vidhrdw/firetrap_vidhrdw.o $(OBJ)/drivers/firetrap.o \
	$(OBJ)/vidhrdw/brkthru_vidhrdw.o $(OBJ)/drivers/brkthru.o \
	$(OBJ)/vidhrdw/metlclsh_vidhrdw.o $(OBJ)/drivers/metlclsh.o \
	$(OBJ)/drivers/compgolf.o \
	$(OBJ)/drivers/tryout.o \
	$(OBJ)/vidhrdw/shootout_vidhrdw.o $(OBJ)/drivers/shootout.o \
	$(OBJ)/vidhrdw/sidepckt_vidhrdw.o $(OBJ)/drivers/sidepckt.o \
	$(OBJ)/vidhrdw/exprraid_vidhrdw.o $(OBJ)/drivers/exprraid.o \
	$(OBJ)/vidhrdw/pcktgal_vidhrdw.o $(OBJ)/drivers/pcktgal.o \
	$(OBJ)/vidhrdw/battlera_vidhrdw.o $(OBJ)/drivers/battlera.o \
	$(OBJ)/vidhrdw/actfancr_vidhrdw.o $(OBJ)/drivers/actfancr.o \
	$(OBJ)/vidhrdw/dec8_vidhrdw.o $(OBJ)/drivers/dec8.o \
	$(OBJ)/vidhrdw/karnov_vidhrdw.o $(OBJ)/drivers/karnov.o \
	$(OBJ)/machine/decocrpt.o $(OBJ)/machine/decoprot.o \
	$(OBJ)/vidhrdw/deco16ic.o \
	$(OBJ)/machine/dec0_machine.o $(OBJ)/vidhrdw/dec0_vidhrdw.o $(OBJ)/drivers/dec0.o \
	$(OBJ)/vidhrdw/stadhero_vidhrdw.o $(OBJ)/drivers/stadhero.o \
	$(OBJ)/vidhrdw/madmotor_vidhrdw.o $(OBJ)/drivers/madmotor.o \
	$(OBJ)/vidhrdw/vaportra_vidhrdw.o $(OBJ)/drivers/vaportra.o \
	$(OBJ)/vidhrdw/cbuster_vidhrdw.o $(OBJ)/drivers/cbuster.o \
	$(OBJ)/vidhrdw/darkseal_vidhrdw.o $(OBJ)/drivers/darkseal.o \
	$(OBJ)/vidhrdw/supbtime_vidhrdw.o $(OBJ)/drivers/supbtime.o \
	$(OBJ)/vidhrdw/cninja_vidhrdw.o $(OBJ)/drivers/cninja.o \
	$(OBJ)/vidhrdw/dassault_vidhrdw.o $(OBJ)/drivers/dassault.o \
	$(OBJ)/vidhrdw/rohga_vidhrdw.o $(OBJ)/drivers/rohga.o \
	$(OBJ)/vidhrdw/tumblep_vidhrdw.o $(OBJ)/drivers/tumblep.o \
	$(OBJ)/vidhrdw/lemmings_vidhrdw.o $(OBJ)/drivers/lemmings.o \
	$(OBJ)/vidhrdw/funkyjet_vidhrdw.o $(OBJ)/drivers/funkyjet.o \
	$(OBJ)/vidhrdw/deco32_vidhrdw.o $(OBJ)/drivers/deco32.o \
	$(OBJ)/vidhrdw/avengrgs_vidhrdw.o $(OBJ)/drivers/avengrgs.o \
	$(OBJ)/vidhrdw/sshangha_vidhrdw.o $(OBJ)/drivers/sshangha.o \

COREOBJS += \
	$(OBJ)/sndhrdw/senjyo_sndhrdw.o $(OBJ)/vidhrdw/senjyo_vidhrdw.o $(OBJ)/drivers/senjyo.o \
	$(OBJ)/vidhrdw/bombjack_vidhrdw.o $(OBJ)/drivers/bombjack.o \
	$(OBJ)/vidhrdw/pbaction_vidhrdw.o $(OBJ)/drivers/pbaction.o \
	$(OBJ)/vidhrdw/tehkanwc_vidhrdw.o $(OBJ)/drivers/tehkanwc.o \
	$(OBJ)/vidhrdw/solomon_vidhrdw.o $(OBJ)/drivers/solomon.o \
	$(OBJ)/vidhrdw/tecmo_vidhrdw.o $(OBJ)/drivers/tecmo.o \
	$(OBJ)/vidhrdw/tbowl_vidhrdw.o $(OBJ)/drivers/tbowl.o \
	$(OBJ)/vidhrdw/gaiden_vidhrdw.o $(OBJ)/drivers/gaiden.o \
	$(OBJ)/vidhrdw/wc90_vidhrdw.o $(OBJ)/drivers/wc90.o \
	$(OBJ)/vidhrdw/wc90b_vidhrdw.o $(OBJ)/drivers/wc90b.o \
	$(OBJ)/vidhrdw/spbactn_vidhrdw.o $(OBJ)/drivers/spbactn.o \
	$(OBJ)/vidhrdw/tecmo16_vidhrdw.o $(OBJ)/drivers/tecmo16.o \
	$(OBJ)/drivers/tecmosys.o \

COREOBJS += \
	$(OBJ)/machine/scramble_machine.o $(OBJ)/sndhrdw/scramble_sndhrdw.o $(OBJ)/drivers/scramble.o \
	$(OBJ)/drivers/frogger.o \
	$(OBJ)/drivers/scobra.o \
	$(OBJ)/drivers/amidar.o \
	$(OBJ)/vidhrdw/fastfred_vidhrdw.o $(OBJ)/drivers/fastfred.o \
	$(OBJ)/vidhrdw/tutankhm_vidhrdw.o $(OBJ)/drivers/tutankhm.o \
	$(OBJ)/drivers/junofrst.o \
	$(OBJ)/vidhrdw/pooyan_vidhrdw.o $(OBJ)/drivers/pooyan.o \
	$(OBJ)/vidhrdw/timeplt_vidhrdw.o $(OBJ)/drivers/timeplt.o $(OBJ)/sndhrdw/timeplt_sndhrdw.o \
	$(OBJ)/vidhrdw/megazone_vidhrdw.o $(OBJ)/drivers/megazone.o \
	$(OBJ)/vidhrdw/pandoras_vidhrdw.o $(OBJ)/drivers/pandoras.o \
	$(OBJ)/sndhrdw/gyruss_sndhrdw.o $(OBJ)/vidhrdw/gyruss_vidhrdw.o $(OBJ)/drivers/gyruss.o \
	$(OBJ)/machine/konami_machine.o \
	$(OBJ)/vidhrdw/trackfld_vidhrdw.o $(OBJ)/sndhrdw/trackfld_sndhrdw.o $(OBJ)/drivers/trackfld.o \
	$(OBJ)/vidhrdw/rocnrope_vidhrdw.o $(OBJ)/drivers/rocnrope.o \
	$(OBJ)/vidhrdw/circusc_vidhrdw.o $(OBJ)/drivers/circusc.o \
	$(OBJ)/vidhrdw/tp84_vidhrdw.o $(OBJ)/drivers/tp84.o \
	$(OBJ)/vidhrdw/hyperspt_vidhrdw.o $(OBJ)/drivers/hyperspt.o \
	$(OBJ)/vidhrdw/sbasketb_vidhrdw.o $(OBJ)/drivers/sbasketb.o \
	$(OBJ)/vidhrdw/mikie_vidhrdw.o $(OBJ)/drivers/mikie.o \
	$(OBJ)/vidhrdw/yiear_vidhrdw.o $(OBJ)/drivers/yiear.o \
	$(OBJ)/vidhrdw/shaolins_vidhrdw.o $(OBJ)/drivers/shaolins.o \
	$(OBJ)/vidhrdw/pingpong_vidhrdw.o $(OBJ)/drivers/pingpong.o \
	$(OBJ)/vidhrdw/gberet_vidhrdw.o $(OBJ)/drivers/gberet.o \
	$(OBJ)/vidhrdw/jailbrek_vidhrdw.o $(OBJ)/drivers/jailbrek.o \
	$(OBJ)/vidhrdw/finalizr_vidhrdw.o $(OBJ)/drivers/finalizr.o \
	$(OBJ)/vidhrdw/ironhors_vidhrdw.o $(OBJ)/drivers/ironhors.o \
	$(OBJ)/machine/jackal_machine.o $(OBJ)/vidhrdw/jackal_vidhrdw.o $(OBJ)/drivers/jackal.o \
	$(OBJ)/vidhrdw/ddrible_vidhrdw.o $(OBJ)/drivers/ddrible.o \
	$(OBJ)/vidhrdw/contra_vidhrdw.o $(OBJ)/drivers/contra.o \
	$(OBJ)/vidhrdw/combatsc_vidhrdw.o $(OBJ)/drivers/combatsc.o \
	$(OBJ)/vidhrdw/hcastle_vidhrdw.o $(OBJ)/drivers/hcastle.o \
	$(OBJ)/vidhrdw/nemesis_vidhrdw.o $(OBJ)/drivers/nemesis.o \
	$(OBJ)/vidhrdw/konamiic.o \
	$(OBJ)/vidhrdw/rockrage_vidhrdw.o $(OBJ)/drivers/rockrage.o \
	$(OBJ)/vidhrdw/flkatck_vidhrdw.o $(OBJ)/drivers/flkatck.o \
	$(OBJ)/vidhrdw/fastlane_vidhrdw.o $(OBJ)/drivers/fastlane.o \
	$(OBJ)/vidhrdw/labyrunr_vidhrdw.o $(OBJ)/drivers/labyrunr.o \
	$(OBJ)/vidhrdw/battlnts_vidhrdw.o $(OBJ)/drivers/battlnts.o \
	$(OBJ)/vidhrdw/bladestl_vidhrdw.o $(OBJ)/drivers/bladestl.o \
	$(OBJ)/machine/ajax_machine.o $(OBJ)/vidhrdw/ajax_vidhrdw.o $(OBJ)/drivers/ajax.o \
	$(OBJ)/vidhrdw/thunderx_vidhrdw.o $(OBJ)/drivers/thunderx.o \
	$(OBJ)/vidhrdw/mainevt_vidhrdw.o $(OBJ)/drivers/mainevt.o \
	$(OBJ)/vidhrdw/88games_vidhrdw.o $(OBJ)/drivers/88games.o \
	$(OBJ)/vidhrdw/gbusters_vidhrdw.o $(OBJ)/drivers/gbusters.o \
	$(OBJ)/vidhrdw/crimfght_vidhrdw.o $(OBJ)/drivers/crimfght.o \
	$(OBJ)/vidhrdw/spy_vidhrdw.o $(OBJ)/drivers/spy.o \
	$(OBJ)/vidhrdw/bottom9_vidhrdw.o $(OBJ)/drivers/bottom9.o \
	$(OBJ)/vidhrdw/blockhl_vidhrdw.o $(OBJ)/drivers/blockhl.o \
	$(OBJ)/vidhrdw/aliens_vidhrdw.o $(OBJ)/drivers/aliens.o \
	$(OBJ)/vidhrdw/surpratk_vidhrdw.o $(OBJ)/drivers/surpratk.o \
	$(OBJ)/vidhrdw/parodius_vidhrdw.o $(OBJ)/drivers/parodius.o \
	$(OBJ)/vidhrdw/rollerg_vidhrdw.o $(OBJ)/drivers/rollerg.o \
	$(OBJ)/vidhrdw/xexex_vidhrdw.o $(OBJ)/drivers/xexex.o \
	$(OBJ)/vidhrdw/asterix_vidhrdw.o $(OBJ)/drivers/asterix.o \
	$(OBJ)/vidhrdw/gijoe_vidhrdw.o $(OBJ)/drivers/gijoe.o \
	$(OBJ)/machine/simpsons_machine.o $(OBJ)/vidhrdw/simpsons_vidhrdw.o $(OBJ)/drivers/simpsons.o \
	$(OBJ)/vidhrdw/vendetta_vidhrdw.o $(OBJ)/drivers/vendetta.o \
	$(OBJ)/vidhrdw/wecleman_vidhrdw.o $(OBJ)/drivers/wecleman.o \
	$(OBJ)/vidhrdw/chqflag_vidhrdw.o $(OBJ)/drivers/chqflag.o \
	$(OBJ)/vidhrdw/ultraman_vidhrdw.o $(OBJ)/drivers/ultraman.o \
	$(OBJ)/vidhrdw/hexion_vidhrdw.o $(OBJ)/drivers/hexion.o \
	$(OBJ)/vidhrdw/twin16_vidhrdw.o $(OBJ)/drivers/twin16.o \
	$(OBJ)/vidhrdw/tmnt_vidhrdw.o $(OBJ)/drivers/tmnt.o \
	$(OBJ)/vidhrdw/xmen_vidhrdw.o $(OBJ)/drivers/xmen.o \
	$(OBJ)/vidhrdw/overdriv_vidhrdw.o $(OBJ)/drivers/overdriv.o \
	$(OBJ)/vidhrdw/gradius3_vidhrdw.o $(OBJ)/drivers/gradius3.o \
	$(OBJ)/vidhrdw/moo_vidhrdw.o $(OBJ)/drivers/moo.o \
	$(OBJ)/vidhrdw/mystwarr_vidhrdw.o $(OBJ)/drivers/mystwarr.o \
	$(OBJ)/vidhrdw/rungun_vidhrdw.o $(OBJ)/drivers/rungun.o \
	$(OBJ)/vidhrdw/dbz2_vidhrdw.o $(OBJ)/drivers/dbz2.o \
	$(OBJ)/vidhrdw/bishi_vidhrdw.o $(OBJ)/drivers/bishi.o \
	$(OBJ)/machine/konamigx_machine.o $(OBJ)/vidhrdw/konamigx_vidhrdw.o $(OBJ)/drivers/konamigx.o \
	$(OBJ)/vidhrdw/djmain_vidhrdw.o $(OBJ)/drivers/djmain.o \
	$(OBJ)/vidhrdw/plygonet_vidhrdw.o $(OBJ)/drivers/plygonet.o \
	$(OBJ)/drivers/mogura.o \
	$(OBJ)/machine/am53cf96.o $(OBJ)/drivers/konamigq.o \

COREOBJS += \
	$(OBJ)/machine/carpolo_machine.o $(OBJ)/vidhrdw/carpolo_vidhrdw.o $(OBJ)/drivers/carpolo.o \
	$(OBJ)/vidhrdw/exidy_vidhrdw.o $(OBJ)/sndhrdw/exidy_sndhrdw.o $(OBJ)/drivers/exidy.o \
	$(OBJ)/sndhrdw/targ.o \
	$(OBJ)/vidhrdw/circus_vidhrdw.o $(OBJ)/drivers/circus.o \
	$(OBJ)/vidhrdw/starfire_vidhrdw.o $(OBJ)/drivers/starfire.o \
	$(OBJ)/vidhrdw/victory_vidhrdw.o $(OBJ)/drivers/victory.o \
	$(OBJ)/sndhrdw/exidy440_sndhrdw.o $(OBJ)/vidhrdw/exidy440_vidhrdw.o $(OBJ)/drivers/exidy440.o \

COREOBJS += \
	$(OBJ)/machine/atari_vg.o \
	$(OBJ)/vidhrdw/tia.o $(OBJ)/drivers/tourtabl.o \
	$(OBJ)/machine/asteroid_machine.o $(OBJ)/sndhrdw/asteroid_sndhrdw.o $(OBJ)/drivers/asteroid.o \
	$(OBJ)/sndhrdw/llander.o \
	$(OBJ)/drivers/bwidow.o \
	$(OBJ)/sndhrdw/bzone_sndhrdw.o	$(OBJ)/drivers/bzone.o \
	$(OBJ)/sndhrdw/redbaron.o \
	$(OBJ)/drivers/tempest.o \
	$(OBJ)/machine/starwars_machine.o $(OBJ)/drivers/starwars.o $(OBJ)/sndhrdw/starwars_sndhrdw.o \
	$(OBJ)/machine/mhavoc_machine.o $(OBJ)/drivers/mhavoc.o \
	$(OBJ)/drivers/quantum.o \
	$(OBJ)/vidhrdw/copsnrob_vidhrdw.o $(OBJ)/machine/copsnrob_machine.o $(OBJ)/drivers/copsnrob.o \
	$(OBJ)/vidhrdw/flyball_vidhrdw.o $(OBJ)/drivers/flyball.o \
	$(OBJ)/vidhrdw/sprint2_vidhrdw.o $(OBJ)/drivers/sprint2.o \
	$(OBJ)/vidhrdw/sprint4_vidhrdw.o $(OBJ)/drivers/sprint4.o \
	$(OBJ)/vidhrdw/sprint8_vidhrdw.o $(OBJ)/drivers/sprint8.o \
	$(OBJ)/vidhrdw/nitedrvr_vidhrdw.o $(OBJ)/machine/nitedrvr_machine.o $(OBJ)/drivers/nitedrvr.o \
	$(OBJ)/machine/dominos.o \
	$(OBJ)/vidhrdw/triplhnt_vidhrdw.o $(OBJ)/drivers/triplhnt.o \
	$(OBJ)/vidhrdw/dragrace_vidhrdw.o $(OBJ)/drivers/dragrace.o \
	$(OBJ)/vidhrdw/poolshrk_vidhrdw.o $(OBJ)/drivers/poolshrk.o \
	$(OBJ)/vidhrdw/starshp1_vidhrdw.o $(OBJ)/drivers/starshp1.o \
	$(OBJ)/vidhrdw/canyon_vidhrdw.o $(OBJ)/drivers/canyon.o \
	$(OBJ)/vidhrdw/destroyr_vidhrdw.o $(OBJ)/drivers/destroyr.o \
	$(OBJ)/drivers/ultratnk.o \
	$(OBJ)/vidhrdw/wolfpack_vidhrdw.o $(OBJ)/drivers/wolfpack.o \
	$(OBJ)/vidhrdw/boxer_vidhrdw.o $(OBJ)/drivers/boxer.o \
	$(OBJ)/vidhrdw/skyraid_vidhrdw.o $(OBJ)/drivers/skyraid.o \
	$(OBJ)/machine/avalnche_machine.o $(OBJ)/vidhrdw/avalnche_vidhrdw.o $(OBJ)/drivers/avalnche.o \
	$(OBJ)/drivers/firetrk.o $(OBJ)/vidhrdw/firetrk_vidhrdw.o \
	$(OBJ)/vidhrdw/skydiver_vidhrdw.o $(OBJ)/drivers/skydiver.o \
	$(OBJ)/machine/sbrkout_machine.o $(OBJ)/vidhrdw/sbrkout_vidhrdw.o $(OBJ)/drivers/sbrkout.o \
	$(OBJ)/machine/atarifb_machine.o $(OBJ)/vidhrdw/atarifb_vidhrdw.o $(OBJ)/drivers/atarifb.o \
	$(OBJ)/vidhrdw/orbit_vidhrdw.o $(OBJ)/drivers/orbit.o \
	$(OBJ)/vidhrdw/videopin_vidhrdw.o $(OBJ)/drivers/videopin.o \
	$(OBJ)/machine/subs_machine.o $(OBJ)/vidhrdw/subs_vidhrdw.o $(OBJ)/drivers/subs.o \
	$(OBJ)/vidhrdw/bsktball_vidhrdw.o $(OBJ)/machine/bsktball_machine.o $(OBJ)/drivers/bsktball.o \
	$(OBJ)/vidhrdw/centiped_vidhrdw.o $(OBJ)/drivers/centiped.o \
	$(OBJ)/vidhrdw/runaway_vidhrdw.o $(OBJ)/drivers/runaway.o \
	$(OBJ)/machine/missile_machine.o $(OBJ)/vidhrdw/missile_vidhrdw.o $(OBJ)/drivers/missile.o \
	$(OBJ)/vidhrdw/foodf_vidhrdw.o $(OBJ)/drivers/foodf.o \
	$(OBJ)/drivers/tunhunt.o $(OBJ)/vidhrdw/tunhunt_vidhrdw.o \
	$(OBJ)/vidhrdw/liberatr_vidhrdw.o $(OBJ)/drivers/liberatr.o \
	$(OBJ)/vidhrdw/ccastles_vidhrdw.o $(OBJ)/drivers/ccastles.o \
	$(OBJ)/vidhrdw/cloak_vidhrdw.o $(OBJ)/drivers/cloak.o \
	$(OBJ)/vidhrdw/cloud9_vidhrdw.o $(OBJ)/drivers/cloud9.o \
	$(OBJ)/vidhrdw/jedi_vidhrdw.o $(OBJ)/drivers/jedi.o \
	$(OBJ)/machine/atarigen.o $(OBJ)/sndhrdw/atarijsa.o \
	$(OBJ)/vidhrdw/atarimo_vidhrdw.o $(OBJ)/vidhrdw/atarirle_vidhrdw.o \
	$(OBJ)/machine/slapstic.o \
	$(OBJ)/vidhrdw/atarisy1_vidhrdw.o $(OBJ)/drivers/atarisy1.o \
	$(OBJ)/vidhrdw/atarisy2_vidhrdw.o $(OBJ)/drivers/atarisy2.o \
	$(OBJ)/machine/irobot_machine.o $(OBJ)/vidhrdw/irobot_vidhrdw.o $(OBJ)/drivers/irobot.o \
	$(OBJ)/machine/harddriv_machine.o $(OBJ)/vidhrdw/harddriv_vidhrdw.o $(OBJ)/sndhrdw/harddriv_sndhrdw.o $(OBJ)/drivers/harddriv.o \
	$(OBJ)/vidhrdw/gauntlet_vidhrdw.o $(OBJ)/drivers/gauntlet.o \
	$(OBJ)/vidhrdw/atetris_vidhrdw.o $(OBJ)/drivers/atetris.o \
	$(OBJ)/vidhrdw/toobin_vidhrdw.o $(OBJ)/drivers/toobin.o \
	$(OBJ)/vidhrdw/vindictr_vidhrdw.o $(OBJ)/drivers/vindictr.o \
	$(OBJ)/vidhrdw/klax_vidhrdw.o $(OBJ)/drivers/klax.o \
	$(OBJ)/vidhrdw/blstroid_vidhrdw.o $(OBJ)/drivers/blstroid.o \
	$(OBJ)/vidhrdw/xybots_vidhrdw.o $(OBJ)/drivers/xybots.o \
	$(OBJ)/vidhrdw/eprom_vidhrdw.o $(OBJ)/drivers/eprom.o \
	$(OBJ)/vidhrdw/skullxbo_vidhrdw.o $(OBJ)/drivers/skullxbo.o \
	$(OBJ)/vidhrdw/badlands_vidhrdw.o $(OBJ)/drivers/badlands.o \
	$(OBJ)/vidhrdw/cyberbal_vidhrdw.o $(OBJ)/sndhrdw/cyberbal_sndhrdw.o $(OBJ)/drivers/cyberbal.o \
	$(OBJ)/vidhrdw/rampart_vidhrdw.o $(OBJ)/drivers/rampart.o \
	$(OBJ)/vidhrdw/shuuz_vidhrdw.o $(OBJ)/drivers/shuuz.o \
	$(OBJ)/vidhrdw/atarig1_vidhrdw.o $(OBJ)/drivers/atarig1.o \
	$(OBJ)/vidhrdw/thunderj_vidhrdw.o $(OBJ)/drivers/thunderj.o \
	$(OBJ)/vidhrdw/batman_vidhrdw.o $(OBJ)/drivers/batman.o \
	$(OBJ)/vidhrdw/relief_vidhrdw.o $(OBJ)/drivers/relief.o \
	$(OBJ)/vidhrdw/offtwall_vidhrdw.o $(OBJ)/drivers/offtwall.o \
	$(OBJ)/vidhrdw/arcadecl_vidhrdw.o $(OBJ)/drivers/arcadecl.o \
	$(OBJ)/vidhrdw/beathead_vidhrdw.o $(OBJ)/drivers/beathead.o \
	$(OBJ)/vidhrdw/atarig42_vidhrdw.o $(OBJ)/drivers/atarig42.o \
	$(OBJ)/machine/asic65.o \
 	$(OBJ)/vidhrdw/atarigx2_vidhrdw.o $(OBJ)/drivers/atarigx2.o \
	$(OBJ)/vidhrdw/atarigt_vidhrdw.o $(OBJ)/drivers/atarigt.o \
	$(OBJ)/vidhrdw/jaguar_vidhrdw.o $(OBJ)/sndhrdw/jaguar_sndhrdw.o $(OBJ)/drivers/cojag.o \
	$(OBJ)/sndhrdw/cage_sndhrdw.o \

COREOBJS += \
	$(OBJ)/vidhrdw/rockola_vidhrdw.o $(OBJ)/sndhrdw/rockola_sndhrdw.o $(OBJ)/drivers/rockola.o \
	$(OBJ)/vidhrdw/lasso_vidhrdw.o $(OBJ)/drivers/lasso.o \
	$(OBJ)/drivers/munchmo.o $(OBJ)/vidhrdw/munchmo_vidhrdw.o \
	$(OBJ)/vidhrdw/marvins_vidhrdw.o $(OBJ)/drivers/marvins.o \
	$(OBJ)/vidhrdw/jcross_vidhrdw.o $(OBJ)/drivers/jcross.o \
	$(OBJ)/vidhrdw/mainsnk_vidhrdw.o $(OBJ)/drivers/mainsnk.o \
	$(OBJ)/drivers/hal21.o \
	$(OBJ)/vidhrdw/snk_vidhrdw.o $(OBJ)/drivers/snk.o \
	$(OBJ)/drivers/sgladiat.o \
	$(OBJ)/vidhrdw/snk68_vidhrdw.o $(OBJ)/drivers/snk68.o \
	$(OBJ)/vidhrdw/prehisle_vidhrdw.o $(OBJ)/drivers/prehisle.o \
	$(OBJ)/vidhrdw/bbusters_vidhrdw.o $(OBJ)/drivers/bbusters.o \

COREOBJS += \
	$(OBJ)/drivers/shougi.o \
	$(OBJ)/machine/equites_machine.o $(OBJ)/vidhrdw/equites_vidhrdw.o $(OBJ)/drivers/equites.o \
	$(OBJ)/vidhrdw/alpha68k_vidhrdw.o $(OBJ)/drivers/alpha68k.o \
	$(OBJ)/vidhrdw/champbas_vidhrdw.o $(OBJ)/drivers/champbas.o \
	$(OBJ)/machine/exctsccr_machine.o $(OBJ)/vidhrdw/exctsccr_vidhrdw.o $(OBJ)/drivers/exctsccr.o \

COREOBJS += \
	$(OBJ)/drivers/scregg.o \
	$(OBJ)/vidhrdw/tagteam_vidhrdw.o $(OBJ)/drivers/tagteam.o \
	$(OBJ)/vidhrdw/ssozumo_vidhrdw.o $(OBJ)/drivers/ssozumo.o \
	$(OBJ)/vidhrdw/mystston_vidhrdw.o $(OBJ)/drivers/mystston.o \
	$(OBJ)/vidhrdw/dogfgt_vidhrdw.o $(OBJ)/drivers/dogfgt.o \
	$(OBJ)/vidhrdw/bogeyman_vidhrdw.o $(OBJ)/drivers/bogeyman.o \
	$(OBJ)/vidhrdw/matmania_vidhrdw.o $(OBJ)/drivers/matmania.o $(OBJ)/machine/maniach.o \
	$(OBJ)/vidhrdw/renegade_vidhrdw.o $(OBJ)/drivers/renegade.o \
	$(OBJ)/vidhrdw/xain_vidhrdw.o $(OBJ)/drivers/xain.o \
	$(OBJ)/vidhrdw/battlane_vidhrdw.o $(OBJ)/drivers/battlane.o \
	$(OBJ)/vidhrdw/ddragon_vidhrdw.o $(OBJ)/drivers/ddragon.o \
	$(OBJ)/drivers/chinagat.o \
	$(OBJ)/vidhrdw/spdodgeb_vidhrdw.o $(OBJ)/drivers/spdodgeb.o \
	$(OBJ)/vidhrdw/wwfsstar_vidhrdw.o $(OBJ)/drivers/wwfsstar.o \
	$(OBJ)/vidhrdw/vball_vidhrdw.o $(OBJ)/drivers/vball.o \
	$(OBJ)/vidhrdw/blockout_vidhrdw.o $(OBJ)/drivers/blockout.o \
	$(OBJ)/vidhrdw/ddragon3_vidhrdw.o $(OBJ)/drivers/ddragon3.o \
	$(OBJ)/vidhrdw/wwfwfest_vidhrdw.o $(OBJ)/drivers/wwfwfest.o \
	$(OBJ)/vidhrdw/shadfrce_vidhrdw.o $(OBJ)/drivers/shadfrce.o \

COREOBJS += \
	$(OBJ)/machine/berzerk_machine.o $(OBJ)/vidhrdw/berzerk_vidhrdw.o $(OBJ)/sndhrdw/berzerk_sndhrdw.o $(OBJ)/drivers/berzerk.o \
	$(OBJ)/drivers/mazerbla.o \
	$(OBJ)/drivers/supdrapo.o \

COREOBJS += \
	$(OBJ)/drivers/toratora.o \
	$(OBJ)/vidhrdw/gameplan_vidhrdw.o $(OBJ)/drivers/gameplan.o \

COREOBJS += \
	$(OBJ)/vidhrdw/zac2650_vidhrdw.o $(OBJ)/drivers/zac2650.o \
	$(OBJ)/vidhrdw/zaccaria_vidhrdw.o $(OBJ)/drivers/zaccaria.o \

COREOBJS += \
	$(OBJ)/vidhrdw/mouser_vidhrdw.o $(OBJ)/drivers/mouser.o \
	$(OBJ)/vidhrdw/nova2001_vidhrdw.o $(OBJ)/drivers/nova2001.o \
	$(OBJ)/vidhrdw/ninjakid_vidhrdw.o $(OBJ)/drivers/ninjakid.o \
	$(OBJ)/vidhrdw/raiders5_vidhrdw.o $(OBJ)/drivers/raiders5.o \
	$(OBJ)/vidhrdw/pkunwar_vidhrdw.o $(OBJ)/drivers/pkunwar.o \
	$(OBJ)/vidhrdw/xxmissio_vidhrdw.o $(OBJ)/drivers/xxmissio.o \
	$(OBJ)/vidhrdw/ninjakd2_vidhrdw.o $(OBJ)/drivers/ninjakd2.o \
	$(OBJ)/vidhrdw/mnight_vidhrdw.o $(OBJ)/drivers/mnight.o \
	$(OBJ)/vidhrdw/omegaf_vidhrdw.o $(OBJ)/drivers/omegaf.o \

COREOBJS += \
	$(OBJ)/vidhrdw/nmk16_vidhrdw.o $(OBJ)/drivers/nmk16.o \
	$(OBJ)/drivers/jalmah.o \
	$(OBJ)/drivers/quizpani.o $(OBJ)/vidhrdw/quizpani_vidhrdw.o \
	$(OBJ)/vidhrdw/macrossp_vidhrdw.o $(OBJ)/drivers/macrossp.o \
	$(OBJ)/vidhrdw/quizdna_vidhrdw.o $(OBJ)/drivers/quizdna.o \

COREOBJS += \
	$(OBJ)/vidhrdw/jack_vidhrdw.o $(OBJ)/drivers/jack.o \
	$(OBJ)/drivers/embargo.o \

COREOBJS += \
	$(OBJ)/sndhrdw/cinemat_sndhrdw.o $(OBJ)/drivers/cinemat.o \
	$(OBJ)/machine/cchasm_machine.o $(OBJ)/vidhrdw/cchasm_vidhrdw.o $(OBJ)/sndhrdw/cchasm_sndhrdw.o $(OBJ)/drivers/cchasm.o \

COREOBJS += \
	$(OBJ)/vidhrdw/thepit_vidhrdw.o $(OBJ)/drivers/thepit.o \
	$(OBJ)/vidhrdw/timelimt_vidhrdw.o $(OBJ)/drivers/timelimt.o \

COREOBJS += \
	$(OBJ)/machine/bagman_machine.o $(OBJ)/vidhrdw/bagman_vidhrdw.o $(OBJ)/drivers/bagman.o \
	$(OBJ)/vidhrdw/tankbust_vidhrdw.o $(OBJ)/drivers/tankbust.o \

COREOBJS += \
	$(OBJ)/vidhrdw/wiz_vidhrdw.o $(OBJ)/drivers/wiz.o \
	$(OBJ)/vidhrdw/kncljoe_vidhrdw.o $(OBJ)/drivers/kncljoe.o \
	$(OBJ)/machine/stfight_machine.o $(OBJ)/vidhrdw/stfight_vidhrdw.o $(OBJ)/drivers/stfight.o \
	$(OBJ)/drivers/cshooter.o \
	$(OBJ)/sndhrdw/seibu.o \
	$(OBJ)/vidhrdw/deadang_vidhrdw.o $(OBJ)/drivers/deadang.o \
	$(OBJ)/vidhrdw/dynduke_vidhrdw.o $(OBJ)/drivers/dynduke.o \
	$(OBJ)/vidhrdw/raiden_vidhrdw.o $(OBJ)/drivers/raiden.o $(OBJ)/drivers/raiden2.o \
	$(OBJ)/vidhrdw/dcon_vidhrdw.o $(OBJ)/drivers/dcon.o \
	$(OBJ)/vidhrdw/sengokmj_vidhrdw.o $(OBJ)/drivers/sengokmj.o \
	$(OBJ)/vidhrdw/mustache_vidhrdw.o $(OBJ)/drivers/mustache.o \

COREOBJS += \
	$(OBJ)/vidhrdw/cabal_vidhrdw.o $(OBJ)/drivers/cabal.o \
	$(OBJ)/vidhrdw/toki_vidhrdw.o $(OBJ)/drivers/toki.o \
	$(OBJ)/vidhrdw/bloodbro_vidhrdw.o $(OBJ)/drivers/bloodbro.o \
	$(OBJ)/vidhrdw/legionna_vidhrdw.o $(OBJ)/drivers/legionna.o \
	$(OBJ)/vidhrdw/goal92_vidhrdw.o $(OBJ)/drivers/goal92.o \

COREOBJS += \
	$(OBJ)/vidhrdw/exerion_vidhrdw.o $(OBJ)/drivers/exerion.o \
	$(OBJ)/drivers/fcombat.o \
	$(OBJ)/vidhrdw/aeroboto_vidhrdw.o $(OBJ)/drivers/aeroboto.o \
	$(OBJ)/vidhrdw/citycon_vidhrdw.o $(OBJ)/drivers/citycon.o \
	$(OBJ)/vidhrdw/momoko_vidhrdw.o $(OBJ)/drivers/momoko.o \
	$(OBJ)/vidhrdw/argus_vidhrdw.o $(OBJ)/drivers/argus.o \
	$(OBJ)/vidhrdw/psychic5_vidhrdw.o $(OBJ)/drivers/psychic5.o \
	$(OBJ)/vidhrdw/ginganin_vidhrdw.o $(OBJ)/drivers/ginganin.o \
	$(OBJ)/vidhrdw/skyfox_vidhrdw.o $(OBJ)/drivers/skyfox.o \
	$(OBJ)/vidhrdw/homerun_vidhrdw.o $(OBJ)/drivers/homerun.o \
	$(OBJ)/vidhrdw/cischeat_vidhrdw.o $(OBJ)/drivers/cischeat.o \
	$(OBJ)/vidhrdw/tetrisp2_vidhrdw.o $(OBJ)/drivers/tetrisp2.o \
	$(OBJ)/vidhrdw/megasys1_vidhrdw.o $(OBJ)/drivers/megasys1.o \
	$(OBJ)/vidhrdw/ms32_vidhrdw.o $(OBJ)/drivers/ms32.o \
	$(OBJ)/vidhrdw/bigstrkb_vidhrdw.o $(OBJ)/drivers/bigstrkb.o \

COREOBJS += \
	$(OBJ)/vidhrdw/rpunch_vidhrdw.o $(OBJ)/drivers/rpunch.o \
	$(OBJ)/vidhrdw/tail2nos_vidhrdw.o $(OBJ)/drivers/tail2nos.o \
	$(OBJ)/vidhrdw/ojankohs_vidhrdw.o $(OBJ)/drivers/ojankohs.o \
	$(OBJ)/vidhrdw/fromance_vidhrdw.o $(OBJ)/drivers/fromance.o $(OBJ)/drivers/pipedrm.o \
	$(OBJ)/vidhrdw/aerofgt_vidhrdw.o $(OBJ)/drivers/aerofgt.o \
	$(OBJ)/vidhrdw/welltris_vidhrdw.o $(OBJ)/drivers/welltris.o \
	$(OBJ)/vidhrdw/f1gp_vidhrdw.o $(OBJ)/drivers/f1gp.o \
	$(OBJ)/vidhrdw/taotaido_vidhrdw.o $(OBJ)/drivers/taotaido.o \
	$(OBJ)/vidhrdw/crshrace_vidhrdw.o $(OBJ)/drivers/crshrace.o \
	$(OBJ)/vidhrdw/gstriker_vidhrdw.o $(OBJ)/drivers/gstriker.o \
	$(OBJ)/vidhrdw/suprslam_vidhrdw.o $(OBJ)/drivers/suprslam.o \
	$(OBJ)/vidhrdw/fromanc2_vidhrdw.o $(OBJ)/drivers/fromanc2.o \
	$(OBJ)/vidhrdw/inufuku_vidhrdw.o $(OBJ)/drivers/inufuku.o \

COREOBJS += \
	$(OBJ)/vidhrdw/psikyo_vidhrdw.o $(OBJ)/drivers/psikyo.o \
	$(OBJ)/vidhrdw/psikyosh_vidhrdw.o $(OBJ)/drivers/psikyosh.o \
	$(OBJ)/vidhrdw/psikyo4_vidhrdw.o $(OBJ)/drivers/psikyo4.o \

COREOBJS += \
	$(OBJ)/machine/8254pit.o $(OBJ)/drivers/leland.o $(OBJ)/vidhrdw/leland_vidhrdw.o $(OBJ)/machine/leland_machine.o $(OBJ)/sndhrdw/leland_sndhrdw.o \
	$(OBJ)/drivers/ataxx.o \

COREOBJS += \
	$(OBJ)/vidhrdw/marineb_vidhrdw.o $(OBJ)/drivers/marineb.o \
	$(OBJ)/vidhrdw/funkybee_vidhrdw.o $(OBJ)/drivers/funkybee.o \
	$(OBJ)/vidhrdw/zodiack_vidhrdw.o $(OBJ)/drivers/zodiack.o \
	$(OBJ)/vidhrdw/espial_vidhrdw.o $(OBJ)/drivers/espial.o \
	$(OBJ)/vidhrdw/vastar_vidhrdw.o $(OBJ)/drivers/vastar.o \

COREOBJS += \
	$(OBJ)/vidhrdw/xorworld_vidhrdw.o $(OBJ)/drivers/xorworld.o \
	$(OBJ)/vidhrdw/splash_vidhrdw.o $(OBJ)/drivers/splash.o \
	$(OBJ)/vidhrdw/thoop2_vidhrdw.o $(OBJ)/drivers/thoop2.o \
	$(OBJ)/vidhrdw/gaelco_vidhrdw.o $(OBJ)/drivers/gaelco.o \
	$(OBJ)/machine/wrally_machine.o $(OBJ)/vidhrdw/wrally_vidhrdw.o $(OBJ)/drivers/wrally.o \
	$(OBJ)/vidhrdw/targeth_vidhrdw.o $(OBJ)/drivers/targeth.o \
	$(OBJ)/machine/gaelco2_machine.o $(OBJ)/vidhrdw/gaelco2_vidhrdw.o $(OBJ)/drivers/gaelco2.o \
	$(OBJ)/vidhrdw/glass_vidhrdw.o $(OBJ)/drivers/glass.o \

COREOBJS += \
	$(OBJ)/vidhrdw/airbustr_vidhrdw.o $(OBJ)/drivers/airbustr.o \
	$(OBJ)/vidhrdw/djboy_vidhrdw.o $(OBJ)/drivers/djboy.o \
	$(OBJ)/vidhrdw/galpanic_vidhrdw.o $(OBJ)/drivers/galpanic.o \
	$(OBJ)/vidhrdw/galpani2_vidhrdw.o $(OBJ)/drivers/galpani2.o \
	$(OBJ)/drivers/jchan.o \
	$(OBJ)/vidhrdw/kaneko16_vidhrdw.o $(OBJ)/drivers/kaneko16.o \
	$(OBJ)/vidhrdw/suprnova_vidhrdw.o $(OBJ)/drivers/suprnova.o \

COREOBJS += \
	$(OBJ)/machine/pd4990a.o $(OBJ)/machine/neocrypt.o \
	$(OBJ)/machine/neogeo_machine.o $(OBJ)/vidhrdw/neogeo_vidhrdw.o $(OBJ)/drivers/neogeo.o \

COREOBJS += \
	$(OBJ)/vidhrdw/hanaawas_vidhrdw.o $(OBJ)/drivers/hanaawas.o \
	$(OBJ)/vidhrdw/speedatk_vidhrdw.o $(OBJ)/drivers/speedatk.o \
	$(OBJ)/vidhrdw/srmp2_vidhrdw.o $(OBJ)/drivers/srmp2.o \
	$(OBJ)/vidhrdw/seta_vidhrdw.o $(OBJ)/drivers/seta.o \
	$(OBJ)/vidhrdw/seta2_vidhrdw.o $(OBJ)/drivers/seta2.o \
	$(OBJ)/vidhrdw/ssv_vidhrdw.o $(OBJ)/drivers/ssv.o \

COREOBJS += \
	$(OBJ)/vidhrdw/powerins_vidhrdw.o $(OBJ)/drivers/powerins.o \
	$(OBJ)/vidhrdw/ohmygod_vidhrdw.o $(OBJ)/drivers/ohmygod.o \
	$(OBJ)/vidhrdw/blmbycar_vidhrdw.o $(OBJ)/drivers/blmbycar.o \

COREOBJS += \
	$(OBJ)/vidhrdw/route16_vidhrdw.o $(OBJ)/drivers/route16.o \
	$(OBJ)/vidhrdw/ttmahjng_vidhrdw.o $(OBJ)/drivers/ttmahjng.o \
	$(OBJ)/vidhrdw/kangaroo_vidhrdw.o $(OBJ)/drivers/kangaroo.o \
	$(OBJ)/vidhrdw/arabian_vidhrdw.o $(OBJ)/drivers/arabian.o \
	$(OBJ)/vidhrdw/markham_vidhrdw.o $(OBJ)/drivers/markham.o \
	$(OBJ)/vidhrdw/strnskil_vidhrdw.o $(OBJ)/drivers/strnskil.o \
	$(OBJ)/vidhrdw/ikki_vidhrdw.o $(OBJ)/drivers/ikki.o \
	$(OBJ)/drivers/shanghai.o \
	$(OBJ)/vidhrdw/shangha3_vidhrdw.o $(OBJ)/drivers/shangha3.o \

COREOBJS += \
	$(OBJ)/vidhrdw/goindol_vidhrdw.o $(OBJ)/drivers/goindol.o \
	$(OBJ)/vidhrdw/suna8_vidhrdw.o $(OBJ)/drivers/suna8.o \
	$(OBJ)/vidhrdw/suna16_vidhrdw.o $(OBJ)/drivers/suna16.o \

COREOBJS += \
	$(OBJ)/vidhrdw/gundealr_vidhrdw.o $(OBJ)/drivers/gundealr.o \
	$(OBJ)/vidhrdw/dooyong_vidhrdw.o $(OBJ)/drivers/dooyong.o \

COREOBJS += \
	$(OBJ)/machine/leprechn_machine.o $(OBJ)/vidhrdw/leprechn_vidhrdw.o $(OBJ)/drivers/leprechn.o \
	$(OBJ)/machine/beezer_machine.o $(OBJ)/vidhrdw/beezer_vidhrdw.o $(OBJ)/drivers/beezer.o \

COREOBJS += \
	$(OBJ)/vidhrdw/pushman_vidhrdw.o $(OBJ)/drivers/pushman.o \
	$(OBJ)/vidhrdw/zerozone_vidhrdw.o $(OBJ)/drivers/zerozone.o \
	$(OBJ)/vidhrdw/galspnbl_vidhrdw.o $(OBJ)/drivers/galspnbl.o \

COREOBJS += \
	$(OBJ)/vidhrdw/sslam_vidhrdw.o $(OBJ)/drivers/sslam.o \
	$(OBJ)/vidhrdw/playmark_vidhrdw.o $(OBJ)/drivers/playmark.o \

COREOBJS += \
	$(OBJ)/vidhrdw/thief_vidhrdw.o $(OBJ)/drivers/thief.o \
	$(OBJ)/vidhrdw/mrflea_vidhrdw.o $(OBJ)/drivers/mrflea.o \

COREOBJS += \
	$(OBJ)/vidhrdw/holeland_vidhrdw.o $(OBJ)/drivers/holeland.o \
	$(OBJ)/vidhrdw/speedbal_vidhrdw.o $(OBJ)/drivers/speedbal.o \
	$(OBJ)/vidhrdw/sauro_vidhrdw.o $(OBJ)/drivers/sauro.o \

COREOBJS += \
	$(OBJ)/vidhrdw/metro_vidhrdw.o $(OBJ)/drivers/metro.o \
	$(OBJ)/vidhrdw/hyprduel_vidhrdw.o $(OBJ)/drivers/hyprduel.o \

COREOBJS += \
	$(OBJ)/vidhrdw/spcforce_vidhrdw.o $(OBJ)/drivers/spcforce.o \
	$(OBJ)/drivers/looping.o \

COREOBJS += \
	$(OBJ)/vidhrdw/paradise_vidhrdw.o $(OBJ)/drivers/paradise.o \
	$(OBJ)/vidhrdw/yunsung8_vidhrdw.o $(OBJ)/drivers/yunsung8.o \
	$(OBJ)/vidhrdw/yunsun16_vidhrdw.o $(OBJ)/drivers/yunsun16.o \

COREOBJS += \
	$(OBJ)/vidhrdw/blueprnt_vidhrdw.o $(OBJ)/drivers/blueprnt.o \

COREOBJS += \
	$(OBJ)/vidhrdw/fuukifg2_vidhrdw.o $(OBJ)/drivers/fuukifg2.o \
	$(OBJ)/vidhrdw/fuukifg3_vidhrdw.o $(OBJ)/drivers/fuukifg3.o \

COREOBJS += \
	$(OBJ)/vidhrdw/drgnmst_vidhrdw.o $(OBJ)/drivers/drgnmst.o \
	$(OBJ)/vidhrdw/unico_vidhrdw.o $(OBJ)/drivers/unico.o \
	$(OBJ)/vidhrdw/silkroad_vidhrdw.o $(OBJ)/drivers/silkroad.o \

COREOBJS += \
	$(OBJ)/vidhrdw/afega_vidhrdw.o $(OBJ)/drivers/afega.o \

COREOBJS += \
	$(OBJ)/vidhrdw/esd16_vidhrdw.o $(OBJ)/drivers/esd16.o \

COREOBJS += \
	$(OBJ)/drivers/royalmah.o \
	$(OBJ)/vidhrdw/hnayayoi_vidhrdw.o $(OBJ)/drivers/hnayayoi.o \
	$(OBJ)/vidhrdw/dynax_vidhrdw.o $(OBJ)/drivers/dynax.o \
	$(OBJ)/drivers/ddenlovr.o \
	$(OBJ)/drivers/realbrk.o $(OBJ)/vidhrdw/realbrk_vidhrdw.o \

COREOBJS += \
	$(OBJ)/vidhrdw/crtc6845.o \
	$(OBJ)/vidhrdw/nyny_vidhrdw.o $(OBJ)/drivers/nyny.o \
	$(OBJ)/drivers/r2dtank.o \
	$(OBJ)/machine/spiders_machine.o $(OBJ)/vidhrdw/spiders_vidhrdw.o $(OBJ)/drivers/spiders.o \

COREOBJS += \
	$(OBJ)/vidhrdw/iqblock_vidhrdw.o $(OBJ)/drivers/iqblock.o \
	$(OBJ)/drivers/chindrag.o \
	$(OBJ)/drivers/grtwall.o \
	$(OBJ)/vidhrdw/pgm_vidhrdw.o $(OBJ)/drivers/pgm.o \
	$(OBJ)/machine/pgmprot_machine.o $(OBJ)/machine/pgmcrypt.o \

COREOBJS += \
	$(OBJ)/vidhrdw/hitme_vidhrdw.o $(OBJ)/drivers/hitme.o \
	$(OBJ)/vidhrdw/starcrus_vidhrdw.o $(OBJ)/drivers/starcrus.o \

COREOBJS += \
	$(OBJ)/vidhrdw/battlex_vidhrdw.o $(OBJ)/drivers/battlex.o \
	$(OBJ)/vidhrdw/carjmbre_vidhrdw.o $(OBJ)/drivers/carjmbre.o \
	$(OBJ)/vidhrdw/popper_vidhrdw.o $(OBJ)/drivers/popper.o \

COREOBJS += \
	$(OBJ)/vidhrdw/speedspn_vidhrdw.o $(OBJ)/drivers/speedspn.o \
	$(OBJ)/vidhrdw/kickgoal_vidhrdw.o $(OBJ)/drivers/kickgoal.o \

COREOBJS += \
	$(OBJ)/vidhrdw/usgames_vidhrdw.o $(OBJ)/drivers/usgames.o \

COREOBJS += \
	$(OBJ)/vidhrdw/mermaid_vidhrdw.o $(OBJ)/drivers/mermaid.o \
	$(OBJ)/vidhrdw/drmicro_vidhrdw.o $(OBJ)/drivers/drmicro.o \
	$(OBJ)/vidhrdw/appoooh_vidhrdw.o $(OBJ)/drivers/appoooh.o \
	$(OBJ)/vidhrdw/bankp_vidhrdw.o $(OBJ)/drivers/bankp.o \
	$(OBJ)/vidhrdw/mjkjidai_vidhrdw.o $(OBJ)/drivers/mjkjidai.o \
	$(OBJ)/vidhrdw/mayumi_vidhrdw.o $(OBJ)/drivers/mayumi.o \

COREOBJS += \
	$(OBJ)/vidhrdw/btoads_vidhrdw.o $(OBJ)/drivers/btoads.o \
	$(OBJ)/vidhrdw/kinst_vidhrdw.o $(OBJ)/drivers/kinst.o \

COREOBJS += \
	$(OBJ)/vidhrdw/freekick_vidhrdw.o $(OBJ)/drivers/freekick.o \

COREOBJS += \
	$(OBJ)/drivers/rmhaihai.o \
	$(OBJ)/drivers/hanaroku.o \
	$(OBJ)/drivers/yumefuda.o \

COREOBJS += \
	$(OBJ)/vidhrdw/homedata_vidhrdw.o $(OBJ)/drivers/homedata.o \

COREOBJS += \
	$(OBJ)/vidhrdw/artmagic_vidhrdw.o $(OBJ)/drivers/artmagic.o \

COREOBJS += \
	$(OBJ)/vidhrdw/shangkid_vidhrdw.o $(OBJ)/drivers/shangkid.o \

COREOBJS += \
	$(OBJ)/vidhrdw/astinvad_vidhrdw.o $(OBJ)/sndhrdw/astinvad_sndhrdw.o $(OBJ)/drivers/astinvad.o \
	$(OBJ)/vidhrdw/spacefb_vidhrdw.o $(OBJ)/drivers/spacefb.o \
	$(OBJ)/drivers/omegrace.o \
	$(OBJ)/vidhrdw/dday_vidhrdw.o $(OBJ)/drivers/dday.o \
	$(OBJ)/vidhrdw/hexa_vidhrdw.o $(OBJ)/drivers/hexa.o \
	$(OBJ)/vidhrdw/redalert_vidhrdw.o $(OBJ)/sndhrdw/redalert_sndhrdw.o $(OBJ)/drivers/redalert.o \
	$(OBJ)/machine/stactics_machine.o $(OBJ)/vidhrdw/stactics_vidhrdw.o $(OBJ)/drivers/stactics.o \
	$(OBJ)/vidhrdw/kingobox_vidhrdw.o $(OBJ)/drivers/kingobox.o \
	$(OBJ)/vidhrdw/ambush_vidhrdw.o $(OBJ)/drivers/ambush.o \
	$(OBJ)/drivers/dlair.o \
	$(OBJ)/vidhrdw/aztarac_vidhrdw.o $(OBJ)/sndhrdw/aztarac_sndhrdw.o $(OBJ)/drivers/aztarac.o \
	$(OBJ)/vidhrdw/mole_vidhrdw.o $(OBJ)/drivers/mole.o \
	$(OBJ)/vidhrdw/gotya_vidhrdw.o $(OBJ)/sndhrdw/gotya_sndhrdw.o $(OBJ)/drivers/gotya.o \
	$(OBJ)/vidhrdw/mrjong_vidhrdw.o $(OBJ)/drivers/mrjong.o \
	$(OBJ)/vidhrdw/polyplay_vidhrdw.o $(OBJ)/sndhrdw/polyplay_sndhrdw.o $(OBJ)/drivers/polyplay.o \
	$(OBJ)/vidhrdw/amspdwy_vidhrdw.o $(OBJ)/drivers/amspdwy.o \
	$(OBJ)/vidhrdw/othldrby_vidhrdw.o $(OBJ)/drivers/othldrby.o \
	$(OBJ)/vidhrdw/mosaic_vidhrdw.o $(OBJ)/drivers/mosaic.o \
	$(OBJ)/drivers/spdbuggy.o \
	$(OBJ)/vidhrdw/sprcros2_vidhrdw.o $(OBJ)/drivers/sprcros2.o \
	$(OBJ)/vidhrdw/mugsmash_vidhrdw.o $(OBJ)/drivers/mugsmash.o \
	$(OBJ)/vidhrdw/stlforce_vidhrdw.o $(OBJ)/drivers/stlforce.o \
	$(OBJ)/vidhrdw/gcpinbal_vidhrdw.o $(OBJ)/drivers/gcpinbal.o \
	$(OBJ)/vidhrdw/aquarium_vidhrdw.o $(OBJ)/drivers/aquarium.o \
	$(OBJ)/vidhrdw/policetr_vidhrdw.o $(OBJ)/drivers/policetr.o \
	$(OBJ)/vidhrdw/pass_vidhrdw.o $(OBJ)/drivers/pass.o \
	$(OBJ)/vidhrdw/news_vidhrdw.o $(OBJ)/drivers/news.o \
	$(OBJ)/vidhrdw/taxidrvr_vidhrdw.o $(OBJ)/drivers/taxidrvr.o \
	$(OBJ)/vidhrdw/xyonix_vidhrdw.o $(OBJ)/drivers/xyonix.o \
	$(OBJ)/drivers/findout.o \
	$(OBJ)/vidhrdw/dribling_vidhrdw.o $(OBJ)/drivers/dribling.o \
	$(OBJ)/drivers/ace.o \
	$(OBJ)/vidhrdw/clayshoo_vidhrdw.o $(OBJ)/machine/clayshoo_machine.o $(OBJ)/drivers/clayshoo.o \
	$(OBJ)/vidhrdw/pirates_vidhrdw.o $(OBJ)/drivers/pirates.o \
	$(OBJ)/vidhrdw/fitfight_vidhrdw.o $(OBJ)/drivers/fitfight.o \
	$(OBJ)/vidhrdw/flower_vidhrdw.o $(OBJ)/sndhrdw/flower_sndhrdw.o $(OBJ)/drivers/flower.o \
	$(OBJ)/vidhrdw/diverboy_vidhrdw.o $(OBJ)/drivers/diverboy.o \
	$(OBJ)/vidhrdw/beaminv_vidhrdw.o $(OBJ)/drivers/beaminv.o \
	$(OBJ)/vidhrdw/mcatadv_vidhrdw.o $(OBJ)/drivers/mcatadv.o \
	$(OBJ)/vidhrdw/4enraya_vidhrdw.o $(OBJ)/drivers/4enraya.o \
	$(OBJ)/vidhrdw/oneshot_vidhrdw.o $(OBJ)/drivers/oneshot.o \
	$(OBJ)/drivers/tugboat.o \
	$(OBJ)/vidhrdw/gotcha_vidhrdw.o $(OBJ)/drivers/gotcha.o \
	$(OBJ)/drivers/coolpool.o \
	$(OBJ)/vidhrdw/gumbo_vidhrdw.o $(OBJ)/drivers/gumbo.o \
	$(OBJ)/drivers/statriv2.o \
	$(OBJ)/vidhrdw/tickee_vidhrdw.o $(OBJ)/drivers/tickee.o \
	$(OBJ)/vidhrdw/crgolf_vidhrdw.o $(OBJ)/drivers/crgolf.o \
	$(OBJ)/vidhrdw/truco_vidhrdw.o $(OBJ)/drivers/truco.o \
	$(OBJ)/vidhrdw/thedeep_vidhrdw.o $(OBJ)/drivers/thedeep.o \
	$(OBJ)/vidhrdw/fantland_vidhrdw.o $(OBJ)/drivers/fantland.o \
	$(OBJ)/drivers/wallc.o \
	$(OBJ)/drivers/skyarmy.o \
	$(OBJ)/vidhrdw/lethalj_vidhrdw.o $(OBJ)/drivers/lethalj.o \
	$(OBJ)/vidhrdw/sbugger_vidhrdw.o $(OBJ)/drivers/sbugger.o \
	$(OBJ)/vidhrdw/portrait_vidhrdw.o $(OBJ)/drivers/portrait.o \
	$(OBJ)/drivers/enigma2.o \
	$(OBJ)/drivers/ltcasino.o \
	$(OBJ)/drivers/vamphalf.o \
	$(OBJ)/drivers/strvmstr.o \
	$(OBJ)/vidhrdw/dorachan_vidhrdw.o $(OBJ)/drivers/dorachan.o \
	$(OBJ)/vidhrdw/ladyfrog_vidhrdw.o $(OBJ)/drivers/ladyfrog.o \
	$(OBJ)/drivers/rabbit.o \
	$(OBJ)/drivers/malzak.o $(OBJ)/vidhrdw/malzak_vidhrdw.o \
	$(OBJ)/drivers/supertnk.o \
	$(OBJ)/drivers/crospang.o \
	$(OBJ)/drivers/funybubl.o \


COREOBJS += $(OBJ)/driver.o $(OBJ)/cheat.o

