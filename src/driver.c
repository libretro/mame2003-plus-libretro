/******************************************************************************

  driver.c

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
#include "driver.c"

/* step 2: define the drivers[] array */
#undef DRIVER
#undef TESTDRIVER
#define DRIVER(NAME) &driver_##NAME,
#define TESTDRIVER(NAME)
const struct GameDriver *drivers[] =
{
#include "driver.c"
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
#include "driver.c"
	0	/* end of array */
};

#else	/* DRIVER_RECURSIVE */

	/* "Pacman hardware" games */
	DRIVER( puckman )	/* (c) 1980 Namco */
	DRIVER( puckmana )	/* (c) 1980 Namco */
	DRIVER( pacman )	/* (c) 1980 Midway */
	DRIVER( pacmanf )	/* hack (speedup) */
	DRIVER( puckmod )	/* (c) 1981 Namco */
	DRIVER( pacmod )	/* (c) 1981 Midway */
	DRIVER( hangly )	/* hack */
	DRIVER( hangly2 )	/* hack */
	DRIVER( hangly3 )	/* hack */
	DRIVER( newpuckx )	/* hack */
	DRIVER( pacheart )	/* hack */
	DRIVER( joyman  )	/* hack */
	DRIVER( newpuc2 )	/* hack */
	DRIVER( newpuc2b )	/* hack */
	DRIVER( piranha )	/* GL */
	DRIVER( piranhao )	/* GL */
	DRIVER( piranhah )	/* hack */
	DRIVER( nmouse )	/* (c) 1981 Amenip (Palcom Queen River) */
	DRIVER( nmouseb )	/* (c) 1981 Amenip Nova Games Ltd. */
	DRIVER( woodpeck )	/* (c) 1981 Amenip (Palcom Queen River) set 1 */
	DRIVER( woodpeca )	/* (c) 1981 Amenip (Palcom Queen River) set 2 */
	DRIVER( pacplus )
	DRIVER( mspacman )	/* (c) 1981 Midway */	/* made by Gencomp */
	DRIVER( mspacmnf )	/* hack (speedup) */
	DRIVER( mspacmab )	/* bootleg */
	DRIVER( mspacmat )	/* hack */
	DRIVER( mspacpls )	/* hack */
	DRIVER( pacgal )	/* hack */
	DRIVER( mschamp )	/* hack */
	DRIVER( mspactwin )	/* hack */
	DRIVER( maketrax )	/* (c) 1981 Williams, high score table says KRL (fur Kural) */
	DRIVER( maketrxb )	/* (c) 1981 [Kural] (Williams license) */
	DRIVER( korosuke )	/* (c) 1981 Kural Electric */
	DRIVER( crush )		/* (c) 1981 Kural Samno Electric Ltd */
	DRIVER( crush2 )	/* (c) 1981 Kural Esco Electric Ltd - bootleg? */
	DRIVER( crush3 )	/* Kural Electric Ltd - bootleg? */
	DRIVER( mbrush )	/* 1981 bootleg */
	DRIVER( paintrlr )	/* 1981 bootleg */
	DRIVER( eyes )		/* (c) 1982 Digitrex Techstar + "Rockola presents" */
	DRIVER( eyes2 )		/* (c) 1982 Techstar + "Rockola presents" */
	DRIVER( mrtnt )		/* (c) 1983 Telko */
	DRIVER( gorkans )	/* (c) 1984 Techstar */
	DRIVER( eggor )		/* (c) 1983 Telko */
	DRIVER( ponpoko )	/* (c) 1982 Sigma Ent. Inc. */
	DRIVER( ponpokov )	/* (c) 1982 Sigma Ent. Inc. + Venture Line license */
	DRIVER( lizwiz )	/* (c) 1985 Techstar + "Sunn presents" */
	DRIVER( theglobp )	/* (c) 1983 Epos Corporation */
	DRIVER( beastf )	/* (c) 1984 Epos Corporation */
	DRIVER( acitya )	/* (c) 1983 Epos Corporation */
	DRIVER( bwcasino )	/* (c) 1983 Epos Coropration */
	DRIVER( dremshpr )	/* (c) 1982 Sanritsu */
	DRIVER( vanvan )	/* (c) 1983 Sanritsu */
	DRIVER( vanvank )	/* (c) 1983 Karateco (bootleg?) */
	DRIVER( alibaba )	/* (c) 1982 Sega */
	DRIVER( alibabab )	/* (c) 1982 Sega (bootleg) */
	DRIVER( pengo )		/* 834-0386 (c) 1982 Sega */
	DRIVER( pengo2 )	/* 834-0386 (c) 1982 Sega */
	DRIVER( pengo2u )	/* 834-0386 (c) 1982 Sega */
	DRIVER( pengo3u )	/* 834-0386 (c) 1982 Sega */
	DRIVER( pengob )	/* bootleg */
	DRIVER( penta )		/* bootleg */
	DRIVER( jrpacman )	/* (c) 1983 Midway */
	DRIVER( jrpacmnf )	/* hack */
	DRIVER( jumpshot )	/* (c) 1985 Bally Midway */
	DRIVER( shootbul )	/* (c) 1985 Bally Midway */
	DRIVER( bigbucks )	/* (c) 1986 Dynasoft Inc. */

	/* S2650 Pacman Kits */
	DRIVER( drivfrcp )	/* (c) 1984 Shinkai Inc. (Magic Eletronics Inc. licence) */
	DRIVER( 8bpm )		/* (c) 1985 Seatongrove Ltd (Magic Eletronics USA licence) */
	DRIVER( porky )		/* (c) 1985 Shinkai Inc. (Magic Eletronics Inc. licence) */

	/* Epos games */
	DRIVER( megadon )	/* (c) 1982 */
	DRIVER( catapult )	/* (c) 1982 */
	DRIVER( eeekk )		/* (c) 1983 */
	DRIVER( suprglob )	/* (c) 1983 */
	DRIVER( theglob )	/* (c) 1983 */
	DRIVER( theglob2 )	/* (c) 1983 */
	DRIVER( theglob3 )	/* (c) 1983 */
	DRIVER( igmo )		/* (c) 1984 */
	DRIVER( dealer )	/* (c) 198? */

	/* "Galaxian hardware" games */
	DRIVER( galaxian )	/* (c) Namco */
	DRIVER( galaxiaj )	/* (c) Namco */
	DRIVER( galmidw )	/* (c) Midway */
	DRIVER( galmidwo )	/* (c) Midway */
	DRIVER( superg )	/* hack */
	DRIVER( galapx )	/* hack */
	DRIVER( moonaln )	/* [Nichibutsu] (Karateco license) or hack */
	DRIVER( galap1 )	/* hack */
	DRIVER( galap4 )	/* hack */
	DRIVER( galturbo )	/* hack */
	DRIVER( swarm )		/* hack */
	DRIVER( zerotime )	/* hack */
	DRIVER( tst_galx )
	DRIVER( gmgalax )	/* bootleg */
	DRIVER( pisces )	/* Subelectro */
	DRIVER( piscesb )	/* bootleg */
	DRIVER( uniwars )	/* (c) Irem */
	DRIVER( gteikoku )	/* (c) Irem */
	DRIVER( gteikokb )	/* bootleg */
	DRIVER( gteikob2 )	/* bootleg */
	DRIVER( spacbatt )	/* bootleg */
	DRIVER( skyraidr )	/* bootleg */
	DRIVER( batman2 )	/* bootleg */
	DRIVER( warofbug )	/* (c) 1981 Armenia */
	DRIVER( redufo )	/* bootleg - original should be (c) Artic */
	DRIVER( exodus )	/* Subelectro - bootleg? */
	DRIVER( streakng )	/* [1980] Shoei */
	DRIVER( pacmanbl )	/* bootleg */
	DRIVER( devilfsg )	/* (c) 1984 Vision / Artic (bootleg?) */
	DRIVER( zigzag )	/* (c) 1982 LAX */
	DRIVER( zigzag2 )	/* (c) 1982 LAX */
	DRIVER( jumpbug )	/* (c) 1981 Rock-ola */
	DRIVER( jumpbugb )	/* (c) 1981 Sega */
	DRIVER( levers )	/* (c) 1983 Rock-ola */
	DRIVER( azurian )	/* (c) 1982 Rait Electronics Ltd */
	DRIVER( orbitron )	/* Signatron USA */
	DRIVER( mooncrgx )	/* bootleg */
	DRIVER( mooncrst )	/* (c) 1980 Nichibutsu */
	DRIVER( mooncrsu )	/* (c) 1980 Nichibutsu USA */
	DRIVER( mooncrsa )	/* (c) 1980 Nichibutsu */
	DRIVER( mooncrsg )	/* (c) 1980 Gremlin */
	DRIVER( smooncrs )	/* Gremlin */
	DRIVER( mooncrsb )	/* bootleg */
	DRIVER( mooncrs2 )	/* bootleg */
	DRIVER( fantazia )	/* bootleg */
	DRIVER( moonqsr )	/* (c) 1980 Nichibutsu */
	DRIVER( mshuttle )	/* (c) 1980 Nichibutsu */
	DRIVER( mshuttlj )	/* (c) 1980 Nichibutsu */
	DRIVER( moonal2 )	/* Nichibutsu */
	DRIVER( moonal2b )	/* Nichibutsu */
	DRIVER( eagle )		/* (c) Centuri */
	DRIVER( eagle2 )	/* (c) Centuri */
	DRIVER( skybase )	/* (c) 1982 Omori Electric Co., Ltd. */
	DRIVER( checkman )	/* (c) 1982 Zilec-Zenitone */
	DRIVER( checkmaj )	/* (c) 1982 Jaleco (Zenitone/Zilec in ROM CM4, and the programmer names) */
	DRIVER( dingo )		/* (c) 1983 Ashby Computers and Graphics LTD. + Jaleco license */
	DRIVER( blkhole )	/* TDS (Tokyo Denshi Sekkei) */
	DRIVER( kingball )	/* (c) 1980 Namco */
	DRIVER( kingbalj )	/* (c) 1980 Namco */
	DRIVER( scorpnmc )	/* bootleg */
	DRIVER( frogg )		/* bootleg */
	DRIVER( 4in1 )		/* (c) 1981 Armenia / Food and Fun */
	DRIVER( bagmanmc )	/* bootleg */
	DRIVER( dkongjrm )	/* bootleg */
	DRIVER( ozon1 )		/* (c) 1983 Proma */
	DRIVER( ladybugg )	/* bootleg */
	DRIVER( vpool )		/* bootleg */
	DRIVER( drivfrcg )	/* Shinkai */

#endif	/* DRIVER_RECURSIVE */
