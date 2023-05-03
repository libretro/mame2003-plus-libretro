/******************************************************************************

  driver_split.c

  The list of all available drivers. Drivers have to be included here to be
  recognized by the executable.

  To save some typing, we use a hack here. This file is recursively #included
  twice, with different definitions of the DRIVER() macro. The first one
  declares external references to the drivers; the second one builds an array
  storing all the drivers.

******************************************************************************/

#include "driver.h"


#ifndef DRIVER_RECURSIVE

/* The "root" driver, defined so we can have &driver_##NAME in macros. */
struct GameDriver driver_0 =
{
	__FILE__,
	0,
	"",
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	NOT_A_DRIVER
};

#endif

#ifndef DRIVER_RECURSIVE

#define DRIVER_RECURSIVE

/* step 1: declare all external references */
#define DRIVER(NAME) extern struct GameDriver driver_##NAME;
#define TESTDRIVER(NAME) extern struct GameDriver driver_##NAME;
#include "driver_split.c"

/* step 2: define the drivers[] array */
#undef DRIVER
#undef TESTDRIVER
#define DRIVER(NAME) &driver_##NAME,
#define TESTDRIVER(NAME)
const struct GameDriver *drivers[] =
{
#include "driver_split.c"
	0	/* end of array */
};

const int total_drivers = sizeof(drivers)/sizeof(drivers[0]);

/* step 3: define the test_drivers[] array */
#undef DRIVER
#undef TESTDRIVER
#define DRIVER(NAME)
#define TESTDRIVER(NAME) &driver_##NAME,

const struct GameDriver *test_drivers[] =
{
#include "driver_split.c"
	0	/* end of array */
};

#else	/* DRIVER_RECURSIVE */

#if defined(drv_1942) || defined(core_none)
	/* 1942.c */
	DRIVER( 1942 )		/* 12/1984 (c) 1984 */
	DRIVER( 1942a )		/* 12/1984 (c) 1984 */
	DRIVER( 1942b )		/* 12/1984 (c) 1984 */
#endif

#if defined(drv_1943) || defined(core_none)
	/* 1943.c */
	DRIVER( 1943 )		/*  6/1987 (c) 1987 (US) */
	DRIVER( 1943j )		/*  6/1987 (c) 1987 (Japan) */
	DRIVER( 1943mii )       /*  6/1987 (c) 1987 (US) */
	DRIVER( 1943kai )	/*  6/1988 (c) 1987 (Japan) */
#endif

#if defined(drv_1945kii) || defined(core_none)
	/* 1945kiii.c */
	DRIVER( 1945kiii )  /* (c) 2000 Oriental */
	DRIVER( flagrall )  /* (c) 1996 Promat? */
#endif

#if defined (drv_40love) || defined(core_none)
	/* 40love.c */
	DRIVER( fieldday )	/* A23 (c) 1984 Taito */
	DRIVER( undoukai )	/* A17 (c) 1984 Taito */
	DRIVER( 40love )	/* A30 (c) 1984 Taito */
#endif

#if defined(drv_4enraya) || defined(core_none)
	/* 4enraya.c */
	DRIVER( 4enraya )	/* (c) 1990 IDSA */
#endif

#if defined(drv_8080bw_drivers) || defined(core_none)
	/* 8080bw_drivers.c */
	/* Midway 8080 b/w games */
	DRIVER( seawolf )	/* 596 [1976] */
	DRIVER( gunfight )	/* 597 [1975] */
	/* 603 - Top Gun [1976] */
	DRIVER( tornbase )	/* 605 [1976] */
	DRIVER( 280zzzap )	/* 610 [1976] */
	DRIVER( maze )		/* 611 [1976] */
	DRIVER( boothill )	/* 612 [1977] */
	DRIVER( checkmat )	/* 615 [1977] */
	DRIVER( desertgu )	/* 618 [1977] */
	DRIVER( dplay )		/* 619 [1977] */
	DRIVER( lagunar )	/* 622 [1977] */
	DRIVER( gmissile )	/* 623 [1977] */
	DRIVER( m4 )		/* 626 [1977] */
	DRIVER( clowns )	/* 630 [1978] */
	DRIVER( clowns1 )	/* 630 [1978] */
	/* 640 - Space Walk [1978] */
	DRIVER( einnings )	/* 642 [1978] Midway */
	DRIVER( shuffle )	/* 643 [1978] */
	DRIVER( dogpatch )	/* 644 [1977] */
	DRIVER( spcenctr )	/* 645 (c) 1980 Midway */
	DRIVER( phantom2 )	/* 652 [1979] */
	DRIVER( bowler )	/* 730 [1978] Midway */
	DRIVER( invaders )	/* 739 [1979] */
	DRIVER( blueshrk )	/* 742 [1978] */
	DRIVER( invad2ct )	/* 851 (c) 1980 Midway */
	DRIVER( invaddlx )	/* 852 [1980] Midway */
	DRIVER( sitv )
	DRIVER( sicv )
	DRIVER( sisv )
	DRIVER( sisv2 )
	DRIVER( galxwars )
	DRIVER( galxwar2 )
	DRIVER( galxwart )
	DRIVER( starw )
	DRIVER( lrescue )	/* LR  (c) 1979 Taito */
	DRIVER( grescue )	/* bootleg? */
	DRIVER( desterth )	/* bootleg */
	DRIVER( invadpt2 )	/* 852 [1980] Taito */
	DRIVER( cosmo )		/* TDS+Mints */
	DRIVER( schaser )	/* RT  Taito */
	DRIVER( schasrcv )	/* RT  Taito */
	DRIVER( sflush ) /* (c)1979 Taito */
	DRIVER( lupin3 )	/* LP  (c) 1980 Taito */
	DRIVER( polaris )	/* PS  (c) 1980 Taito */
	DRIVER( polarisa )	/* PS  (c) 1980 Taito */
	DRIVER( ballbomb )	/* TN  (c) 1980 Taito */
	DRIVER( indianbt )	/* Taito - 1980 */
	DRIVER( steelwkr )	/* (c) 1980 Taito */
	DRIVER( earthinv )
	DRIVER( spaceatt )
	DRIVER( spaceat2 )
	DRIVER( sinvzen )
	DRIVER( sinvemag )
	DRIVER( tst_invd )
	DRIVER( alieninv )
	DRIVER( spceking )
	DRIVER( spcewars )
	DRIVER( spacewr3 )
	DRIVER( invaderl )
	DRIVER( jspecter )
	DRIVER( jspectr2 )
	DRIVER( cosmicmo )	/* Universal */
	DRIVER( cosmicm2 )	/* Universal */
	DRIVER( superinv )
	DRIVER( sstrangr )
	DRIVER( sstrngr2 )
	DRIVER( moonbase )	/* Zeta - Nichibutsu */
	DRIVER( invrvnge )
	DRIVER( invrvnga )
	DRIVER( spclaser )
	DRIVER( laser )
	DRIVER( spcewarl )
	DRIVER( rollingc )	/* Nichibutsu */
	DRIVER( ozmawars )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( ozmawar2 )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( solfight )	/* bootleg */
	DRIVER( spaceph )	/* Zilec Games */
	DRIVER( yosakdon )
	DRIVER( yosakdoa )
	DRIVER( invasion )
	DRIVER( invasioa )
	DRIVER( invasiob )
	DRIVER( astropal )
	DRIVER( galactic )
	DRIVER( spacmiss )
#endif

#if defined(drv_88games) || defined(core_none)
	/* 88games.c */
	DRIVER( 88games )	/* GX861 (c) 1988 */
	DRIVER( konami88 )	/* GX861 (c) 1988 */
	DRIVER( hypsptsp )	/* GX861 (c) 1988 (Japan) */
#endif

#if defined(drv_aburner) || defined(core_none)
	/* aburner.c */
	DRIVER( aburner )	/* (c) 1987 */
	DRIVER( aburner2 )  /* (c) 1987 */
	DRIVER( loffire )	/* (protected) */
	DRIVER( thndrbld )	/* (protected) */
	DRIVER( thndrbdj )  /* (protected?) */
#endif

#if defined(drv_ace) || defined(core_none)
	/* ace.c */
	DRIVER( ace )		/* [1976 Allied Leisure] */
#endif

#if defined(drv_actfancr) || defined(core_none)
	/* actfancr.c */
	DRIVER( actfancr )	/* (c) 1989 Data East Corporation (World) */
	DRIVER( actfanc1 )	/* (c) 1989 Data East Corporation (World) */
	DRIVER( actfancj )	/* (c) 1989 Data East Corporation (Japan) */
	DRIVER( triothep )	/* (c) 1989 Data East Corporation (Japan) */
#endif

#if defined(drv_aeroboto) || defined(core_none)
	/* aeroboto.c */
	DRIVER( formatz )	/* (c) 1984 Jaleco */
	DRIVER( aeroboto )	/* (c) 1984 Williams */
#endif

#if defined(drv_aerofgt) || defined(core_none)
	/* aerofgt.c */
	DRIVER( spinlbrk )	/* (c) 1990 V-System Co. (World) */
	DRIVER( spinlbru )	/* (c) 1990 V-System Co. (US) */
	DRIVER( spinlbrj )	/* (c) 1990 V-System Co. (Japan) */
	DRIVER( pspikes )	/* (c) 1991 Video System Co. (World) */
	DRIVER( pspikesk )	/* (c) 1991 Video System Co. (Korea) */
	DRIVER( svolly91 )	/* (c) 1991 Video System Co. (Japan) */
	DRIVER( karatblz )	/* (c) 1991 Video System Co. */
	DRIVER( karatblu )	/* (c) 1991 Video System Co. (US) */
	DRIVER( turbofrc )	/* (c) 1991 Video System Co. */
	DRIVER( aerofgt )	/* (c) 1992 Video System Co. */
	DRIVER( aerofgtb )	/* (c) 1992 Video System Co. */
	DRIVER( aerofgtc )	/* (c) 1992 Video System Co. */
	DRIVER( sonicwi )	/* (c) 1992 Video System Co. (Japan) */
#endif

#if defined(drv_afega) || defined(core_none)
	/* afega.c */
	DRIVER( stagger1 )	/* (c) 1998 */
	DRIVER( redhawk )	  /* (c) 1997 */
	DRIVER( grdnstrm )	/* (c) 1998 */
	DRIVER( bubl2000 )	/* (c) 1998 Tuning */
	DRIVER( spec2k )    /* (c) 2000 Yonatech */
	DRIVER( firehawk )  /* (c) 2001 ESD */
#endif

#if defined(drv_airbustr) || defined(core_none)
	/* airbustr.c */
	DRIVER( airbustr )	/* (c) 1990 Kaneko + Namco */
	DRIVER( airbustj )	/* (c) 1990 Kaneko + Namco (Japan) */
#endif

#if defined(drv_ajax) || defined(core_none)
	/* ajax.c */
	DRIVER( ajax )		/* GX770 (c) 1987 */
	DRIVER( typhoon )	/* GX770 (c) 1987 */
	DRIVER( ajaxj )		/* GX770 (c) 1987 (Japan) */
#endif

#if defined(drv_aliens) || defined(core_none)
	/* aliens.c */
	DRIVER( aliens )	/* GX875 (c) 1990 (World) */
	DRIVER( aliens2 )	/* GX875 (c) 1990 (World) */
	DRIVER( aliensu )	/* GX875 (c) 1990 (US) */
	DRIVER( aliensj )	/* GX875 (c) 1990 (Japan) */
#endif

#if defined(drv_alpha68k) || defined(core_none)
	/* alpha68k.c */
	DRIVER( sstingry )	/* (c) 1986 Alpha Denshi Co. */
	DRIVER( kyros )		/* (c) 1987 World Games */
	DRIVER( kyrosj )	/* (c) 1986 Alpha Denshi Co. */
	DRIVER( paddlema )	/* Alpha-68K96I  'PM' (c) 1988 SNK */
	DRIVER( timesold )	/* Alpha-68K96II 'BT' (c) 1987 SNK / Romstar */
	DRIVER( timesol1 )  /* Alpha-68K96II 'BT' (c) 1987 */
	DRIVER( btlfield )  /* Alpha-68K96II 'BT' (c) 1987 */
	DRIVER( skysoldr )	/* Alpha-68K96II 'SS' (c) 1988 SNK (Romstar with dip switch) */
	DRIVER( goldmedl )	/* Alpha-68K96II 'GM' (c) 1988 SNK */
	DRIVER( goldmedb )	/* Alpha-68K96II bootleg */
	DRIVER( skyadvnt )	/* Alpha-68K96V  'SA' (c) 1989 Alpha Denshi Co. */
	DRIVER( skyadvnu )	/* Alpha-68K96V  'SA' (c) 1989 SNK of America licensed from Alpha */
	DRIVER( skyadvnj )	/* Alpha-68K96V  'SA' (c) 1989 Alpha Denshi Co. */
	DRIVER( gangwars )	/* Alpha-68K96V       (c) 1989 Alpha Denshi Co. */
	DRIVER( gangwarb )	/* Alpha-68K96V bootleg */
	DRIVER( sbasebal )	/* Alpha-68K96V       (c) 1989 SNK of America licensed from Alpha */
	DRIVER( tnexspce )	/* A8003 'NS' (c) 1989 */
#endif

#if defined(drv_ambush) || defined(core_none)
	/* ambush.c */
	DRIVER( ambush )	/* (c) 1983 Nippon Amuse Co-Ltd */
	DRIVER( ambusht )	/* (c) 1983 Tecfri */
#endif

#if defined(drv_amidar) || defined(core_none)
	/* amidar.c */
	DRIVER( amidar )	/* GX337 (c) 1981 Konami */
	DRIVER( amidaru )	/* GX337 (c) 1982 Konami + Stern license */
	DRIVER( amidaro )	/* GX337 (c) 1982 Konami + Olympia license */
	DRIVER( amigo )		/* bootleg */
	DRIVER( turtles )	/* (c) 1981 Stern */
	DRIVER( turpin )	/* (c) 1981 Sega */
	DRIVER( 600 )		/* GX353 (c) 1981 Konami */
#endif

#if defined(drv_climber) || defined(core_none)
	/* cclimber.c */
	DRIVER( cclimber )	/* (c) 1980 Nichibutsu */
	DRIVER( cclimbrj )	/* (c) 1980 Nichibutsu */
	DRIVER( ccboot )	/* bootleg */
	DRIVER( ccboot2 )	/* bootleg */
	DRIVER( ckong )		/* (c) 1981 Falcon */
	DRIVER( ckonga )	/* (c) 1981 Falcon */
	DRIVER( ckongjeu )	/* bootleg */
	DRIVER( ckongo )	/* bootleg */
	DRIVER( ckongalc )	/* bootleg */
	DRIVER( monkeyd )	/* bootleg */
	DRIVER( rpatrolb )	/* bootleg */
	DRIVER( silvland )	/* Falcon */
	DRIVER( swimmer )	/* (c) 1982 Tehkan */
	DRIVER( swimmera )	/* (c) 1982 Tehkan */
	DRIVER( swimmerb )	/* (c) 1982 Tehkan */
	DRIVER( guzzler )	/* (c) 1983 Tehkan */
	DRIVER( cannonb )	/* (c) 1985 Soft */
#endif

#if defined(drv_system32) || defined(core_none)
	/* system32.c */
	DRIVER( holo )		/* (c) 1992 (US) */
	DRIVER( arescue )	/* (c) 1992 */
	DRIVER( radm )
	DRIVER( radr )		/* (c) 1991 */
	DRIVER( spidey )	/* (c) 1991 */
	DRIVER( spideyj )	/* (c) 1991 (Japan) */
	DRIVER( f1en )
	DRIVER( arabfgt )	/* (c) 1991 */
	DRIVER( ga2 )		/* (c) 1992 */
	DRIVER( ga2j )		/* (c) 1992 */
	DRIVER( brival )	/* (c) 1992 (Japan) */
	DRIVER( sonic )		/* (c) 1992 (Japan) */
	DRIVER( sonicp )	/* (c) 1992 (Japan) */
	DRIVER( alien3 )	/* (c) 1993 */
	DRIVER( jpark )		/* (c) 1994 */
	DRIVER( svf )		/* (c) 1994 */
	DRIVER( svs )		/* (c) 1994 */
	DRIVER( jleague )	/* (c) 1994 (Japan) */
	DRIVER( f1lap )     /* (c) 1993 (World) */
	DRIVER( f1lapj )    /* (c) 1993 (Japan) */
	DRIVER( darkedge )
	DRIVER( dbzvrvs )
	DRIVER( slipstrm )	/* Capcom */
#endif

#if defined(drv_yamato) || defined(core_none)
	/* yamato.c */
	DRIVER( yamato )	/* (c) 1983 Sega */
	DRIVER( yamato2 )	/* (c) 1983 Sega */
#endif

#endif	/* DRIVER_RECURSIVE */
