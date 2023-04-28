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

//core_a
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

	/* Has some similarities with Moon Cresta but Board is very different */
	DRIVER( rockclim )	/* (c)1981 Taito */

	/* "Scramble hardware" (and variations) games */
	DRIVER( scramble )	/* GX387 (c) 1981 Konami */
	DRIVER( scrambls )	/* GX387 (c) 1981 Stern */
	DRIVER( scramblb )	/* bootleg */
	DRIVER( explorer )	/* bootleg */
	DRIVER( atlantis )	/* (c) 1981 Comsoft */
	DRIVER( atlants2 )	/* (c) 1981 Comsoft */
	DRIVER( theend )	/* (c) 1980 Konami */
	DRIVER( theends )	/* (c) 1980 Stern */
	DRIVER( omega )		/* bootleg */
	DRIVER( ckongs )	/* bootleg */
	DRIVER( froggers )	/* bootleg */
	DRIVER( amidars )	/* GX337 (c) 1982 Konami */
	DRIVER( triplep )	/* (c) 1982 KKI */	/* made by Sanritsu? */
	DRIVER( knockout )	/* (c) 1982 KKK */
	DRIVER( mariner )	/* (c) 1981 Amenip */
	DRIVER( 800fath )	/* (c) 1981 Amenip + U.S. Billiards license */
	DRIVER( mars )		/* (c) 1981 Artic */
	DRIVER( devilfsh )	/* (c) 1982 Artic */
	DRIVER( newsin7 )	/* (c) 1983 ATW USA, Inc. */
	DRIVER( mrkougar )	/* (c) 1984 ATW */
	DRIVER( mrkougr2 )
	DRIVER( mrkougb )	/* bootleg */
	DRIVER( hotshock )	/* (c) 1982 E.G. Felaco */
	DRIVER( hunchbks )	/* (c) 1983 Century */
	DRIVER( cavelon )	/* (c) 1983 Jetsoft */
	DRIVER( sfx )
	DRIVER( mimonscr )
	DRIVER( mimonkey )
	DRIVER( mimonsco )
	DRIVER( scobra )	/* GX316 (c) 1981 Konami */
	DRIVER( scobras )	/* GX316 (c) 1981 Stern */
	DRIVER( scobrase )	/* GX316 (c) 1981 Stern */
	DRIVER( scobrab )	/* GX316 (c) 1981 Karateco (bootleg?) */
	DRIVER( stratgyx )	/* GX306 (c) 1981 Konami */
	DRIVER( stratgys )	/* GX306 (c) 1981 Stern */
	DRIVER( armorcar )	/* (c) 1981 Stern */
	DRIVER( armorca2 )	/* (c) 1981 Stern */
	DRIVER( moonwar )	/* (c) 1981 Stern */
	DRIVER( moonwara )	/* (c) 1981 Stern */
	DRIVER( spdcoin )	/* (c) 1984 Stern */
	DRIVER( darkplnt )	/* (c) 1982 Stern */
	DRIVER( tazmania )	/* (c) 1982 Stern */
	DRIVER( tazmani2 )	/* (c) 1982 Stern */
	DRIVER( calipso )	/* (c) 1982 Tago */
	DRIVER( anteater )	/* (c) 1982 Tago */
	DRIVER( rescue )	/* (c) 1982 Stern */
	DRIVER( aponow )	/* bootleg */
	DRIVER( minefld )	/* (c) 1983 Stern */
	DRIVER( losttomb )	/* (c) 1982 Stern */
	DRIVER( losttmbh )	/* (c) 1982 Stern */
	DRIVER( superbon )	/* bootleg */
	DRIVER( hustler )	/* GX343 (c) 1981 Konami */
	DRIVER( billiard )	/* bootleg */
	DRIVER( hustlerb )	/* bootleg */
	DRIVER( frogger )	/* GX392 (c) 1981 Konami */
	DRIVER( frogseg1 )	/* (c) 1981 Sega */
	DRIVER( frogseg2 )	/* 834-0068 (c) 1981 Sega */
	DRIVER( froggrmc )	/* 800-3110 (c) 1981 Sega */
	DRIVER( amidar )	/* GX337 (c) 1981 Konami */
	DRIVER( amidaru )	/* GX337 (c) 1982 Konami + Stern license */
	DRIVER( amidaro )	/* GX337 (c) 1982 Konami + Olympia license */
	DRIVER( amigo )		/* bootleg */
	DRIVER( turtles )	/* (c) 1981 Stern */
	DRIVER( turpin )	/* (c) 1981 Sega */
	DRIVER( 600 )		/* GX353 (c) 1981 Konami */
	DRIVER( flyboy )	/* (c) 1982 Kaneko */
	DRIVER( flyboyb )	/* bootleg */
	DRIVER( fastfred )	/* (c) 1982 Atari */
	DRIVER( jumpcoas )	/* (c) 1983 Kaneko */
	DRIVER( boggy84 )	/* bootleg, original is (c)1983 Taito/Kaneko */
	DRIVER( redrobin )	/* (c) 1986 Elettronolo */
	DRIVER( imago ) 	/* cocktail set */
	DRIVER( imagoa )	/* no cocktail set */

	/* "Crazy Climber hardware" games */
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
	DRIVER( yamato )	/* (c) 1983 Sega */
	DRIVER( yamato2 )	/* (c) 1983 Sega */
	DRIVER( swimmer )	/* (c) 1982 Tehkan */
	DRIVER( swimmera )	/* (c) 1982 Tehkan */
	DRIVER( swimmerb )	/* (c) 1982 Tehkan */
	DRIVER( guzzler )	/* (c) 1983 Tehkan */
	DRIVER( cannonb )	/* (c) 1985 Soft */

	/* Nichibutsu games */
	DRIVER( gomoku )	/* (c) 1981 */
	DRIVER( wiping )	/* (c) 1982 */
	DRIVER( rugrats )	/* (c) 1983 */
	DRIVER( friskyt )	/* (c) 1981 */
	DRIVER( friskyta )	/* (c) 1981 */
	DRIVER( radrad )	/* (c) 1982 Nichibutsu USA */
	DRIVER( seicross )	/* (c) 1984 + Alice */
	DRIVER( sectrzon )	/* (c) 1984 + Alice */
	DRIVER( firebatl )	/* (c) 1984 Taito */
	DRIVER( clshroad )	/* (c) 1986 Woodplace Inc. */
	DRIVER( tubep )		/* (c) 1984 + Fujitek */
	DRIVER( rjammer )	/* (c) 1984 + Alice */
	DRIVER( magmax )	/* (c) 1985 */
	DRIVER( cop01 )		/* (c) 1985 */
	DRIVER( cop01a )	/* (c) 1985 */
	DRIVER( mightguy )	/* (c) 1986 */
	DRIVER( terracre )	/* (c) 1985 */
	DRIVER( terracrb )	/* (c) 1985 */
	DRIVER( terracra )	/* (c) 1985 */
	DRIVER( amazon )	/* (c) 1986 */
	DRIVER( amatelas )	/* (c) 1986 */
	DRIVER( horekid )	/* (c) 1987 */
	DRIVER( horekidb )	/* bootleg */
	DRIVER( galivan )	/* (c) 1985 */
	DRIVER( galivan2 )	/* (c) 1985 */
	DRIVER( dangar )	/* (c) 1986 */
	DRIVER( dangar2 )	/* (c) 1986 */
	DRIVER( dangarb )	/* bootleg */
	DRIVER( ninjemak )	/* (c) 1986 (US?) */
	DRIVER( youma )		/* (c) 1986 (Japan) */
	DRIVER( legion )	/* (c) 1986 */
	DRIVER( legiono )	/* (c) 1986 */
	DRIVER( terraf )	/* (c) 1987 */
	DRIVER( terrafu )	/* (c) 1987 Nichibutsu USA */
	DRIVER( kodure )	/* (c) 1987 (Japan) */
	DRIVER( armedf )	/* (c) 1988 */
	DRIVER( cclimbr2 )	/* (c) 1988 (Japan) */

	/* Nichibutsu Mahjong games */
	DRIVER( hyhoo )		/* (c) 1987 */
	DRIVER( hyhoo2 )	/* (c) 1987 */

	DRIVER( pastelgl )	/* (c) 1985 */

	DRIVER( crystalg )	/* (c) 1986 */
	DRIVER( crystal2 )	/* (c) 1986 */
	DRIVER( citylove )	/* (c) 1986 */
	DRIVER( apparel )	/* (c) 1986 Central Denshi */
	DRIVER( secolove )	/* (c) 1986 */
	DRIVER( housemnq )	/* (c) 1987 */
	DRIVER( housemn2 )	/* (c) 1987 */
	DRIVER( seiha )		/* (c) 1987 */
	DRIVER( seiham )	/* (c) 1987 */
	DRIVER( bijokkoy )	/* (c) 1987 */
	DRIVER( iemoto )	/* (c) 1987 */
	DRIVER( ojousan )	/* (c) 1987 */
	DRIVER( bijokkog )	/* (c) 1988 */
	DRIVER( orangec )	/* (c) 1988 Daiichi Denshi */
	DRIVER( vipclub )	/* (c) 1988 Daiichi Denshi */
	DRIVER( korinai )	/* (c) 1988 */
	DRIVER( kaguya )	/* (c) 1988 MIKI SYOUJI */
	DRIVER( otonano )	/* (c) 1988 Apple */
	DRIVER( kanatuen )	/* (c) 1988 Panac */
	DRIVER( mjsikaku )	/* (c) 1988 */
	DRIVER( mjsikakb )	/* (c) 1988 */
	DRIVER( mjcamera )	/* (c) 1988 MIKI SYOUJI */
	DRIVER( mmcamera )	/* (c) 1988 MIKI SYOUJI */
	DRIVER( idhimitu )	/* (c) 1989 Digital Soft */

	DRIVER( msjiken )	/* (c) 1988 */
	DRIVER( hanamomo )	/* (c) 1988 */
	DRIVER( telmahjn )	/* (c) 1988 */
	DRIVER( gionbana )	/* (c) 1989 */
	DRIVER( mjfocus )	/* (c) 1989 */
	DRIVER( mjfocusm )	/* (c) 1989 */
	DRIVER( peepshow )	/* (c) 1989 AC */
	DRIVER( scandal )	/* (c) 1989 */
	DRIVER( scandalm )	/* (c) 1989 */
	DRIVER( mgmen89 )	/* (c) 1989 */
	DRIVER( mjnanpas )	/* (c) 1989 BROOKS */
	DRIVER( mjnanpaa )	/* (c) 1989 BROOKS */
	DRIVER( mjnanpau )	/* (c) 1989 BROOKS */
	DRIVER( pairsten )	/* (c) 1989 System Ten */
	DRIVER( bananadr )	/* (c) 1989 DIGITAL SOFT */
	DRIVER( mladyhtr )	/* (c) 1990 */
	DRIVER( chinmoku )	/* (c) 1990 */
	DRIVER( maiko )		/* (c) 1990 */
	DRIVER( club90s )	/* (c) 1990 */
	DRIVER( club90sa )	/* (c) 1990 */
	DRIVER( hanaoji )	/* (c) 1991 */

	DRIVER( pstadium )	/* (c) 1990 */
	DRIVER( triplew1 )	/* (c) 1989 */
	DRIVER( triplew2 )	/* (c) 1990 */
	DRIVER( ntopstar )	/* (c) 1990 */
	DRIVER( mjlstory )	/* (c) 1991 */
	DRIVER( vanilla )	/* (c) 1991 */
	DRIVER( finalbny )	/* (c) 1991 */
	DRIVER( qmhayaku )	/* (c) 1991 */
	DRIVER( galkoku )	/* (c) 1989 Nichibutsu/T.R.TEC */
	DRIVER( hyouban )	/* (c) 1989 Nichibutsu/T.R.TEC */
	DRIVER( galkaika )	/* (c) 1989 Nichibutsu/T.R.TEC */
	DRIVER( tokyogal )	/* (c) 1989 */
	DRIVER( tokimbsj )	/* (c) 1989 */
	DRIVER( mcontest )	/* (c) 1989 */
	DRIVER( uchuuai )	/* (c) 1989 */
	DRIVER( av2mj1bb )	/* (c) 1991 MIKI SYOUJI/AV JAPAN */
	DRIVER( av2mj2rg )	/* (c) 1991 MIKI SYOUJI/AV JAPAN */

	DRIVER( mjuraden )	/* (c) 1992 Nichibutsu/Yubis */
	DRIVER( koinomp )	/* (c) 1992 */
	DRIVER( patimono )	/* (c) 1992 */
	DRIVER( mjanbari )	/* (c) 1992 Nichibutsu/Yubis/AV JAPAN */
	DRIVER( ultramhm )	/* (c) 1993 Apple */
	DRIVER( gal10ren )	/* (c) 1993 FUJIC */
	DRIVER( renaiclb )	/* (c) 1993 FUJIC */
	DRIVER( mjlaman )	/* (c) 1993 Nichibutsu/AV JAPAN */
	DRIVER( mkeibaou )	/* (c) 1993 */
	DRIVER( pachiten )	/* (c) 1993 Nichibutsu/MIKI SYOUJI/AV JAPAN */
	DRIVER( sailorws )	/* (c) 1993 */
	DRIVER( sailorwr )	/* (c) 1993 */
	DRIVER( psailor1 )	/* (c) 1994 SPHINX */
	DRIVER( psailor2 )	/* (c) 1994 SPHINX */
	DRIVER( otatidai )	/* (c) 1995 SPHINX */
	DRIVER( ngpgal )	/* (c) 1991 */
	DRIVER( mjgottsu )	/* (c) 1991 */
	DRIVER( bakuhatu )	/* (c) 1991 */
	DRIVER( cmehyou )	/* (c) 1992 Nichibutsu/Kawakusu */
	DRIVER( mmehyou )	/* (c) 1992 Nichibutsu/Kawakusu */
	DRIVER( mjkoiura )	/* (c) 1992 */
	DRIVER( imekura )	/* (c) 1994 SPHINX/AV JAPAN */
	DRIVER( mscoutm )	/* (c) 1994 SPHINX/AV JAPAN */
	DRIVER( mjegolf )	/* (c) 1994 FUJIC/AV JAPAN */

	DRIVER( niyanpai )	/* (c) 1996 */

	/* "Phoenix hardware" (and variations) games */
	DRIVER( safarir )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( safarirj )	/* Shin Nihon Kikaku (SNK) Taito License */
	DRIVER( phoenix )	/* (c) 1980 Amstar */
	DRIVER( phoenixa )	/* (c) 1980 Amstar + Centuri license */
	DRIVER( phoenixt )	/* (c) 1980 Taito */
	DRIVER( phoenix3 )	/* bootleg */
	DRIVER( phoenixc )	/* bootleg */
	DRIVER( condor )	/* bootleg */
	DRIVER( falcon )	/* bootleg */
	DRIVER( vautour )	/* bootleg (Jeutel) */
	DRIVER( pleiads )	/* (c) 1981 Tehkan */
	DRIVER( pleiadbl )	/* bootleg */
	DRIVER( pleiadce )	/* (c) 1981 Centuri + Tehkan */
	DRIVER( capitol )	/* bootleg? */
	DRIVER( survival )	/* (c) 1982 Rock-ola */
	DRIVER( naughtyb )	/* (c) 1982 Jaleco */
	DRIVER( naughtya )	/* bootleg */
	DRIVER( naughtyc )	/* (c) 1982 Jaleco + Cinematronics */
	DRIVER( popflame )	/* (c) 1982 Jaleco */
	DRIVER( popflama )	/* (c) 1982 Jaleco */
	DRIVER( popflamb )	/* (c) 1982 Jaleco */

	/* Namco games (plus some intruders on similar hardware) */
	DRIVER( geebee )	/* [1978] Namco */
	DRIVER( geebeeg )	/* [1978] Gremlin */
	DRIVER( bombbee )	/* [1979] Namco */
	DRIVER( cutieq )	/* (c) 1979 Namco */
	DRIVER( navalone )	/* (c) 1980 Namco */
	DRIVER( kaitei )	/* [1980] K.K. Tokki */
	DRIVER( kaitein )	/* [1980] Namco */
	DRIVER( sos )		/* [1980] Namco */
	DRIVER( tankbatt )	/* (c) 1980 Namco */
	DRIVER( warpwarp )	/* (c) 1981 Namco */
	DRIVER( warpwarr )	/* (c) 1981 Rock-ola - the high score table says "NAMCO" */
	DRIVER( warpwar2 )	/* (c) 1981 Rock-ola - the high score table says "NAMCO" */
	DRIVER( rallyx )	/* (c) 1980 Namco */
	DRIVER( rallyxm )	/* (c) 1980 Midway */
	DRIVER( nrallyx )	/* (c) 1981 Namco */
	DRIVER( jungler )	/* GX327 (c) 1981 Konami */
	DRIVER( junglers )	/* GX327 (c) 1981 Stern */
	DRIVER( tactcian )	/* GX335 (c) 1982 Sega */
	DRIVER( tactcan2 )	/* GX335 (c) 1981 Sega */
	DRIVER( locomotn )	/* GX359 (c) 1982 Konami + Centuri license */
	DRIVER( gutangtn )	/* GX359 (c) 1982 Konami + Sega license */
	DRIVER( cottong )	/* bootleg */
	DRIVER( commsega )	/* (c) 1983 Sega */
	/* the following ones all have a custom I/O chip */
	DRIVER( bosco )		/* (c) 1981 */
	DRIVER( boscoo )	/* (c) 1981 */
	DRIVER( boscoo2 )	/* (c) 1981 */
	DRIVER( boscomd )	/* (c) 1981 Midway */
	DRIVER( boscomdo )	/* (c) 1981 Midway */
	DRIVER( galaga )	/* (c) 1981 */
	DRIVER( galagamw )	/* (c) 1981 Midway */
	DRIVER( galagamk )
	DRIVER( galagamf )
	DRIVER( galagao )
	DRIVER( gallag )	/* bootleg */
	DRIVER( nebulbee )	/* bootleg */
	DRIVER( gatsbee )	/* (c) 1984 Uchida / hack */
	DRIVER( digdug )	/* (c) 1982 */
	DRIVER( digdugb )	/* (c) 1982 */
	DRIVER( digdugat )	/* (c) 1982 Atari */
	DRIVER( digduga1 )	/* (c) 1982 Atari */
	DRIVER( dzigzag )	/* bootleg */
	DRIVER( xevious )	/* (c) 1982 */
	DRIVER( xeviousa )	/* (c) 1982 + Atari license */
	DRIVER( xeviousb )	/* (c) 1982 + Atari license */
	DRIVER( xeviousc )
	DRIVER( xevios )	/* bootleg */
	DRIVER( battles )	/* bootleg */
	DRIVER( sxevious )	/* (c) 1984 */
	DRIVER( superpac )	/* (c) 1982 */
	DRIVER( superpcm )	/* (c) 1982 Midway */
	DRIVER( pacnpal )	/* (c) 1983 */
	DRIVER( pacnpal2 )	/* (c) 1983 */
	DRIVER( pacnchmp )	/* (c) 1983 */
	DRIVER( phozon )	/* (c) 1983 */
	DRIVER( mappy )		/* (c) 1983 */
	DRIVER( mappyj )	/* (c) 1983 */
	DRIVER( digdug2 )	/* (c) 1985 */
	DRIVER( digdug2o )	/* (c) 1985 */
	DRIVER( todruaga )	/* (c) 1984 */
	DRIVER( todruago )	/* (c) 1984 */
	DRIVER( motos )		/* (c) 1985 */
	DRIVER( grobda )	/* (c) 1984 */
	DRIVER( grobda2 )	/* (c) 1984 */
	DRIVER( grobda3 )	/* (c) 1984 */
	DRIVER( gaplus )	/* (c) 1984 */
	DRIVER( gaplusa )	/* (c) 1984 */
	DRIVER( gapluso )	/* (c) 1984 */
	DRIVER( galaga3 )	/* (c) 1984 */
	DRIVER( galaga3m )	/* (c) 1984 */
	DRIVER( galaga3a )	/* (c) 1984 */
	/* Libble Rabble board (first Japanese game using a 68000) */
	DRIVER( liblrabl )	/* (c) 1983 */
	DRIVER( toypop )	/* (c) 1986 */
	/* Z8000 games */
	DRIVER( polepos )	/* (c) 1982  */
	DRIVER( poleposa )	/* (c) 1982 + Atari license */
	DRIVER( polepos1 )	/* (c) 1982 Atari */
	DRIVER( topracer )	/* bootleg */
	DRIVER( polepos2 )	/* (c) 1983 */
	DRIVER( poleps2a )	/* (c) 1983 + Atari license */
	DRIVER( poleps2b )	/* bootleg */
/*	DRIVER( poleps2c )	 bootleg */ 
	/* no custom I/O in the following, HD63701 (or compatible) microcontroller instead */
	DRIVER( pacland )	/* (c) 1984 */
	DRIVER( pacland2 )	/* (c) 1984 */
	DRIVER( pacland3 )	/* (c) 1984 */
	DRIVER( paclandm )	/* (c) 1984 Midway */
	DRIVER( drgnbstr )	/* (c) 1984 */
	DRIVER( skykid )	/* (c) 1985 */
	DRIVER( skykido )	/* (c) 1985 */
	DRIVER( skykidd )	/* (c) 1985 */
	DRIVER( baraduke )	/* (c) 1985 */
	DRIVER( baraduka )	/* (c) 1985 */
	DRIVER( metrocrs )	/* (c) 1985 */
	DRIVER( metrocra )	/* (c) 1985 */

	/* Namco System 86 games */
	DRIVER( hopmappy )	/* (c) 1986 */
	DRIVER( skykiddx )	/* (c) 1986 */
	DRIVER( skykiddo )	/* (c) 1986 */
	DRIVER( roishtar )	/* (c) 1986 */
	DRIVER( genpeitd )	/* (c) 1986 */
	DRIVER( rthunder )	/* (c) 1986 new version */
	DRIVER( rthundro )	/* (c) 1986 old version */
	DRIVER( wndrmomo )	/* (c) 1987 */

	/* Thunder Ceptor HW */
	DRIVER( tceptor )
	DRIVER( tceptor2 )

	/* Namco System 1 games */
	DRIVER( shadowld )	/* (c) 1987 */
	DRIVER( youkaidk )	/* (c) 1987 (Japan new version) */
	DRIVER( yokaidko )	/* (c) 1987 (Japan old version) */
	DRIVER( dspirit )	/* (c) 1987 new version */
	DRIVER( dspirito )	/* (c) 1987 old version */
	DRIVER( blazer )	/* (c) 1987 (Japan) */
	DRIVER( quester )	/* (c) 1987 (Japan) */
	DRIVER( pacmania )	/* (c) 1987 */
	DRIVER( pacmanij )	/* (c) 1987 (Japan) */
	DRIVER( galaga88 )	/* (c) 1987 */
	DRIVER( galag88b )	/* (c) 1987 */
	DRIVER( galag88j )	/* (c) 1987 (Japan) */
	DRIVER( ws )		/* (c) 1988 (Japan) */
	DRIVER( berabohm )	/* (c) 1988 (Japan) */
	DRIVER( beraboho )	/* (c) 1988 (Japan) */
	DRIVER( mmaze )		/* (c) 1988 (Japan) */
	DRIVER( bakutotu )	/* (c) 1988 (Japan) */
	DRIVER( wldcourt )	/* (c) 1988 (Japan) */
	DRIVER( splatter )	/* (c) 1988 (Japan) */
	DRIVER( faceoff )	/* (c) 1988 (Japan) */
	DRIVER( rompers )	/* (c) 1989 (Japan) */
	DRIVER( romperso )	/* (c) 1989 (Japan) */
	DRIVER( blastoff )	/* (c) 1989 (Japan) */
	DRIVER( ws89 )		/* (c) 1989 (Japan) */
	DRIVER( dangseed )	/* (c) 1989 (Japan) */
	DRIVER( ws90 )		/* (c) 1990 (Japan) */
	DRIVER( pistoldm )	/* (c) 1990 (Japan) */
	DRIVER( boxyboy )	/* (c) 1990 (US) */
	DRIVER( soukobdx )	/* (c) 1990 (Japan) */
	DRIVER( puzlclub )	/* (c) 1990 (Japan) */
	DRIVER( tankfrce )	/* (c) 1991 (US) */
	DRIVER( tankfrcj )	/* (c) 1991 (Japan) */

	/* Namco System 2 games */
	DRIVER( finallap )	/* 87.12 Final Lap */
	DRIVER( finalapd )	/* 87.12 Final Lap */
	DRIVER( finalapc )	/* 87.12 Final Lap */
	DRIVER( finlapjc )	/* 87.12 Final Lap */
	DRIVER( finlapjb )	/* 87.12 Final Lap */
	DRIVER( assault )	/* (c) 1988 */
	DRIVER( assaultj )	/* (c) 1988 (Japan) */
	DRIVER( assaultp )	/* (c) 1988 (Japan) */
	DRIVER( metlhawk )	/* (c) 1988 */
	DRIVER( ordyne )	/* (c) 1988 */
	DRIVER( mirninja )	/* (c) 1988 (Japan) */
	DRIVER( phelios )	/* (c) 1988 (Japan) */
	DRIVER( dirtfoxj )	/* (c) 1989 (Japan) */
	DRIVER( fourtrax )	/* 89.11 */
	DRIVER( valkyrie )	/* (c) 1989 (Japan) */
	DRIVER( finehour )	/* (c) 1989 (Japan) */
	DRIVER( burnforc )	/* (c) 1989 (Japan) */
	DRIVER( marvland )	/* (c) 1989 (US) */
	DRIVER( marvlanj )	/* (c) 1989 (Japan) */
	DRIVER( kyukaidk )	/* (c) 1990 (Japan) */
	DRIVER( kyukaido )	/* (c) 1990 (Japan) */
	DRIVER( dsaber )	/* (c) 1990 */
	DRIVER( dsaberj )	/* (c) 1990 (Japan) */
	DRIVER( finalap2 )	/* 90.8  Final Lap 2 */
	DRIVER( finalp2j )	/* 90.8  Final Lap 2 (Japan) */
	DRIVER( gollygho )	/* 91.7  Golly Ghost */
	DRIVER( rthun2 )	/* (c) 1990 */
	DRIVER( rthun2j )	/* (c) 1990 (Japan) */
	DRIVER( sgunner )	/* (c) 1990 */
	/* 91.9  Super World Stadium */
	DRIVER( sgunner2 )	/* (c) 1991 (US) */
	DRIVER( sgunnr2j )	/* (c) 1991 (Japan) */
	DRIVER( cosmogng )	/* (c) 1991 (US) */
	DRIVER( cosmognj )	/* (c) 1991 (Japan) */
	DRIVER( finalap3 )	/* 92.9  Final Lap 3 */
	DRIVER( finalp3a )	/* 92.9  Final Lap 3 */
	DRIVER( luckywld )	/* (c) 1992 */
	DRIVER( suzuka8h )
	DRIVER( suzuk8hj )
	/* 92.8  Bubble Trouble */
	DRIVER( sws92 )		/* (c) 1992 (Japan) */
	DRIVER( sws92g )	/* (c) 1992 (Japan) */
	DRIVER( suzuk8h2 )
	DRIVER( sws93 )		/* (c) 1993 (Japan) */
	/* 93.6  Super World Stadium '93 */

	/* Namco NA-1 / NA-2 System games */
	DRIVER( bkrtmaq )	/* (c) 1992 (Japan) */
	DRIVER( cgangpzl )	/* (c) 1992 (US) */
	DRIVER( cgangpzj )	/* (c) 1992 (Japan) */
	DRIVER( exvania )	/* (c) 1992 (Japan) */
	DRIVER( fghtatck )	/* (c) 1992 (US) */
	DRIVER( fa )		/* (c) 1992 (Japan) */
	DRIVER( knckhead )	/* (c) 1992 (World) */
	DRIVER( knckhedj )	/* (c) 1992 (Japan) */
	DRIVER( swcourt )	/* (c) 1992 (Japan) */
	DRIVER( emeralda )	/* (c) 1993 (Japan) */
	DRIVER( emerldaa )	/* (c) 1993 (Japan) */
	DRIVER( numanath )	/* (c) 1993 (World) */
	DRIVER( numanatj )	/* (c) 1993 (Japan) */
	DRIVER( quiztou )	/* (c) 1993 (Japan) */
	DRIVER( tinklpit )	/* (c) 1993 (Japan) */
	DRIVER( xday2 )		/* (c) 1995 (Japan) */

	/* Namco NB-1 / NB-2 System games */
	DRIVER( nebulray )	/* (c) 1994 (World) */
	DRIVER( nebulryj )	/* (c) 1994 (Japan) */
	DRIVER( ptblank )	/* (c) 1994 */
	DRIVER( gunbulet )	/* (c) 1994 (Japan) */
	DRIVER( gslgr94u )	/* (c) 1994 */
	DRIVER( sws95 )		/* (c) 1995 (Japan) */
	DRIVER( sws96 )		/* (c) 1996 (Japan) */
	DRIVER( sws97 )		/* (c) 1997 (Japan) */
	DRIVER( vshoot )	/* (c) 1994 */
	DRIVER( outfxies )	/* (c) 1994 */
	DRIVER( outfxesj )	/* (c) 1994 (Japan) */
	DRIVER( machbrkr )	/* (c) 1995 (Japan) */

	/* Namco ND-1 games */
	DRIVER( ncv1 )		/* (c) 1995 */
	DRIVER( ncv1j )		/* (c) 1995 (Japan) */
	DRIVER( ncv1j2 )	/* (c) 1995 (Japan) */
	DRIVER( ncv2 )		/* (c) 1996 */
	DRIVER( ncv2j )		/* (c) 1996 (Japan) */

	/* Namco System 21 games */
	/* 1988, Winning Run */
	/* 1989, Winning Run Suzuka Grand Prix */
	DRIVER( winrun91 )
	DRIVER( solvalou )	/* (c) 1991 (Japan) */
	DRIVER( starblad )	/* (c) 1991 */
/* 199?, Driver's Eyes */
/* 1992, ShimDrive */
	DRIVER( aircombj )	/* (c) 1992 (Japan) */
	DRIVER( aircombu )	/* (c) 1992 (US) */
	DRIVER( cybsled )	/* (c) 1993 */

	/* Namco System 22 games */
	DRIVER( alpinerd )	/* (c) 1994 */
	DRIVER( alpinerc )	/* (c) 1994 */
	DRIVER( raveracw )	/* (c) 1995 */
	DRIVER( ridgeraj )
	DRIVER( ridger2j )
	DRIVER( acedrvrw )
	DRIVER( victlapw )
	DRIVER( cybrcomm )
	DRIVER( airco22b )
	DRIVER( cybrcycc )
	DRIVER( timecrsa )
	DRIVER( propcycl )	/* (c) 1996 */


	/* Universal games */
	DRIVER( cosmicg )	/* 7907 (c) 1979 */
	DRIVER( cosmica )	/* 7910 (c) [1979] */
	DRIVER( cosmica2 )	/* 7910 (c) 1979 */
	DRIVER( panic )		/* (c) 1980 */
	DRIVER( panic2 )	/* (c) 1980 */
	DRIVER( panic3 )	/* (c) 1980 */
	DRIVER( panich )	/* (c) 1980 */
	DRIVER( panicger )	/* (c) 1980 */
	DRIVER( zerohour )	/* 8011 (c) Universal */
	DRIVER( redclash )	/* (c) 1981 Tehkan */
	DRIVER( redclask )	/* (c) Kaneko (bootleg?) */
	DRIVER( magspot )	/* ???? (c) [1980] */
	DRIVER( magspot2 )	/* 8013 (c) [1980] */
	DRIVER( devzone )	/* 8022 (c) [1980] */
	DRIVER( devzone2 )	/* 8022 (c) [1980] */
	DRIVER( nomnlnd )	/* (c) [1980?] */
	DRIVER( nomnlndg )	/* (c) [1980?] + Gottlieb */
	DRIVER( cheekyms )	/* (c) [1980?] */
	DRIVER( ladybug )	/* (c) 1981 */
	DRIVER( ladybugb )	/* bootleg */
	DRIVER( snapjack )	/* (c) */
	DRIVER( cavenger )	/* (c) 1981 */
	DRIVER( dorodon )	/* Falcon */
	DRIVER( dorodon2 )	/* Falcon */
	DRIVER( sraider )	/* (c) 1982 */
	DRIVER( mrsdyna )	/* (c) 1982 */
	DRIVER( mrdo )		/* (c) 1982 */
	DRIVER( mrdoy )		/* (c) 1982 */
	DRIVER( mrdot )		/* (c) 1982 + Taito license */
	DRIVER( mrdofix )	/* (c) 1982 + Taito license */
	DRIVER( mrlo )		/* bootleg */
	DRIVER( mrdu )		/* bootleg */
	DRIVER( yankeedo )	/* bootleg */
	DRIVER( docastle )	/* (c) 1983 */
	DRIVER( docastl2 )	/* (c) 1983 */
	DRIVER( docastlo )	/* (c) 1983 */
	DRIVER( douni )		/* (c) 1983 */
	DRIVER( dorunrun )	/* (c) 1984 */
	DRIVER( dorunru2 )	/* (c) 1984 */
	DRIVER( dorunruc )	/* (c) 1984 */
	DRIVER( spiero )	/* (c) 1987 */
	DRIVER( dowild )	/* (c) 1984 */
	DRIVER( jjack )		/* (c) 1984 */
	DRIVER( kickridr )	/* (c) 1984 */
	DRIVER( idsoccer )	/* (c) 1985 */

	/* Nintendo games */
	DRIVER( spacefev )  /* (c) 1979 Nintendo */
	DRIVER( spacefevo ) /* (c) 1979 Nintendo */
	DRIVER( highsplt )  /* (c) 1979 Nintendo */
	DRIVER( highspla )  /* (c) 1979 Nintendo */
	DRIVER( spacelnc )  /* (c) 1979 Nintendo */
	DRIVER( sheriff )   /* (c) 1979 Nintendo */
	DRIVER( bandido )   /* (c) 1980 Exidy */
	DRIVER( helifire )  /* (c) 1980 Nintendo */
	DRIVER( helifira )  /* (c) 1980 Nintendo */
	DRIVER( radarscp )  /* (c) 1980 Nintendo */
	DRIVER( radarscpc ) /* (c) 1980 Nintendo */
	DRIVER( radarscp1 ) /* (c) 1980 Nintendo */
	DRIVER( dkong )	    /* (c) 1981 Nintendo of America */
	DRIVER( dkongpe )
	DRIVER( dkongo )    /* (c) 1981 Nintendo */
	DRIVER( dkongjp )	/* (c) 1981 Nintendo */
	DRIVER( dkongjo )	/* (c) 1981 Nintendo */
	DRIVER( dkongjo1 )	/* (c) 1981 Nintendo */
	DRIVER( dkongx )	/* (c) 2008 Bootleg  */
	DRIVER( dkrdemo )	/* 2015 Bootleg  */
	DRIVER( dkongjr )	/* (c) 1982 Nintendo of America */
	DRIVER( dkongjrj )	/* (c) 1982 Nintendo */
	DRIVER( dkngjnrj )	/* (c) 1982 Nintendo */
	DRIVER( dkongjrb )	/* bootleg */
	DRIVER( dkngjnrb )	/* (c) 1982 Nintendo of America */
	DRIVER( dkong3 )	/* (c) 1983 Nintendo of America */
	DRIVER( dkong3j )	/* (c) 1983 Nintendo */
	DRIVER( dkong3b )	/* bootleg */
	DRIVER( mario )		/* (c) 1983 Nintendo of America */
	DRIVER( marioe )	/* (c) 1983 Nintendo of America */
	DRIVER( mariof )	/* (c) 1983 Nintendo of America */
	DRIVER( marioj )	/* (c) 1983 Nintendo */
	DRIVER( masao )		/* bootleg */
	DRIVER( pestplce )	/* bootleg on donkey kong hw */
	DRIVER( spclforc )
	DRIVER( spcfrcii )
	DRIVER( 8ballact )
	DRIVER( 8ballat2 )
	DRIVER( shootgal )
	DRIVER( drakton )
	DRIVER( strtheat )
	DRIVER( hunchbkd )	/* (c) 1983 Century */
	DRIVER( sbdk )		/* (c) 1984 Century */
	DRIVER( herbiedk )	/* (c) 1984 CVS */
	DRIVER( herodk )	/* (c) 1984 Seatongrove + Crown license */
	DRIVER( herodku )	/* (c) 1984 Seatongrove + Crown license */
	DRIVER( skyskipr )	/* (c) 1981 */
	DRIVER( popeye )	/* (c) 1982 */
	DRIVER( popeyeu )	/* (c) 1982 */
	DRIVER( popeyef )	/* (c) 1982 */
	DRIVER( popeyebl )	/* bootleg */
	DRIVER( punchout )	/* (c) 1984 */
	DRIVER( spnchout )	/* (c) 1984 */
	DRIVER( spnchotj )	/* (c) 1984 (Japan) */
	DRIVER( armwrest )	/* (c) 1985 */

	/* Nintendo Playchoice 10 games */
	DRIVER( pc_tenis )	/* (c) 1983 Nintendo */
	DRIVER( pc_mario )	/* (c) 1983 Nintendo */
	DRIVER( pc_bball )	/* (c) 1984 Nintendo of America */
	DRIVER( pc_bfght )	/* (c) 1984 Nintendo */
	DRIVER( pc_ebike )	/* (c) 1984 Nintendo */
	DRIVER( pc_golf	)	/* (c) 1984 Nintendo */
	DRIVER( pc_kngfu )	/* (c) 1984 Irem (Nintendo license) */
	DRIVER( pc_1942 )	/* (c) 1985 Capcom */
	DRIVER( pc_smb )	/* (c) 1985 Nintendo */
	DRIVER( pc_vball )	/* (c) 1986 Nintendo */
	DRIVER( pc_duckh )	/* (c) 1984 Nintendo */
	DRIVER( pc_hgaly )	/* (c) 1984 Nintendo */
	DRIVER( pc_wgnmn )	/* (c) 1984 Nintendo */
	DRIVER( pc_grdus )	/* (c) 1986 Konami */
	DRIVER( pc_grdue )	/* (c) 1986 Konami */
	DRIVER( pc_tkfld )	/* (c) 1987 Konami (Nintendo of America license) */
	DRIVER( pc_pwrst )	/* (c) 1986 Nintendo */
	DRIVER( pc_trjan )	/* (c) 1986 Capcom USA (Nintendo of America license) */
	DRIVER( pc_cvnia )	/* (c) 1987 Konami (Nintendo of America license) */
	DRIVER( pc_dbldr )	/* (c) 1987 Konami (Nintendo of America license) */
	DRIVER( pc_rnatk )	/* (c) 1987 Konami (Nintendo of America license) */
	DRIVER( pc_rygar )	/* (c) 1987 Tecmo (Nintendo of America license) */
	DRIVER( pc_cntra )	/* (c) 1988 Konami (Nintendo of America license) */
	DRIVER( pc_goons )	/* (c) 1986 Konami */
	DRIVER( pc_mtoid )	/* (c) 1986 Nintendo */
	DRIVER( pc_radrc )	/* (c) 1987 Square */
	DRIVER( pc_miket )	/* (c) 1987 Nintendo */
	DRIVER( pc_rcpam )	/* (c) 1987 Rare */
	DRIVER( pc_ngaid )	/* (c) 1989 Tecmo (Nintendo of America license) */
	DRIVER( pc_tmnt )	/* (c) 1989 Konami (Nintendo of America license) */
	DRIVER( pc_ftqst )	/* (c) 1989 Sunsoft (Nintendo of America license) */
	DRIVER( pc_bstar )	/* (c) 1989 SNK (Nintendo of America license) */
	DRIVER( pc_tbowl )	/* (c) 1989 Tecmo (Nintendo of America license) */
	DRIVER( pc_drmro )	/* (c) 1990 Nintendo */
	DRIVER( pc_ynoid )	/* (c) 1990 Capcom USA (Nintendo of America license) */
	DRIVER( pc_rrngr )	/* (c) Capcom USA (Nintendo of America license) */
	DRIVER( pc_ddrgn )
	DRIVER( pc_gntlt )	/* (c) 1985 Atari/Tengen (Nintendo of America license) */
	DRIVER( pc_smb2 )	/* (c) 1988 Nintendo */
	DRIVER( pc_smb3 )	/* (c) 1988 Nintendo */
	DRIVER( pc_mman3 )	/* (c) 1990 Capcom USA (Nintendo of America license) */
	DRIVER( pc_radr2 )	/* (c) 1990 Square (Nintendo of America license) */
	DRIVER( pc_suprc )	/* (c) 1990 Konami (Nintendo of America license) */
	DRIVER( pc_tmnt2 )	/* (c) 1990 Konami (Nintendo of America license) */
	DRIVER( pc_wcup )	/* (c) 1990 Technos (Nintendo license) */
	DRIVER( pc_ngai2 )	/* (c) 1990 Tecmo (Nintendo of America license) */
	DRIVER( pc_ngai3 )	/* (c) 1991 Tecmo (Nintendo of America license) */
	DRIVER( pc_pwbld )	/* (c) 1991 Taito (Nintendo of America license) */
	DRIVER( pc_rkats )	/* (c) 1991 Atlus (Nintendo of America license) */
	DRIVER( pc_pinbt )	/* (c) 1988 Rare (Nintendo of America license) */
	DRIVER( pc_cshwk )	/* (c) 1989 Rare (Nintendo of America license) */
	DRIVER( pc_sjetm )	/* (c) 1990 Rare */
	DRIVER( pc_moglf )	/* (c) 1991 Nintendo */

	/* Nintendo VS games */
	DRIVER( btlecity )	/* (c) 1985 Namco */
	DRIVER( starlstr )	/* (c) 1985 Namco */
	DRIVER( cstlevna )	/* (c) 1987 Konami */
	DRIVER( cluclu )	/* (c) 1984 Nintendo */
	DRIVER( drmario )	/* (c) 1990 Nintendo */
	DRIVER( duckhunt )	/* (c) 1985 Nintendo */
	DRIVER( excitebk )	/* (c) 1984 Nintendo */
	DRIVER( excitbkj )	/* (c) 1984 Nintendo */
	DRIVER( goonies )	/* (c) 1986 Konami */
	DRIVER( hogalley )	/* (c) 1985 Nintendo */
	DRIVER( iceclimb )	/* (c) 1984 Nintendo */
	DRIVER( iceclmbj )	/* (c) 1984 Nintendo */
	DRIVER( ladygolf )	/* (c) 1984 Nintendo */
	DRIVER( machridr )	/* (c) 1985 Nintendo */
	DRIVER( machridj )	/* (c) 1985 Nintendo */
	DRIVER( rbibb )		/* (c) 1987 Namco */
	DRIVER( rbibba )	/* (c) 1987 Namco */
	DRIVER( suprmrio )	/* (c) 1986 Nintendo */
	DRIVER( vsskykid )	/* (c) 1986 Namco */
	DRIVER( tkoboxng )	/* (c) 1987 Data East */
	DRIVER( smgolf )	/* (c) 1984 Nintendo */
	DRIVER( smgolfj )	/* (c) 1984 Nintendo */
	DRIVER( vspinbal )	/* (c) 1984 Nintendo */
	DRIVER( vspinblj )	/* (c) 1984 Nintendo */
	DRIVER( vsslalom )	/* (c) 1986 Nintendo */
	DRIVER( vssoccer )	/* (c) 1985 Nintendo */
	DRIVER( vsgradus )	/* (c) 1986 Konami */
	DRIVER( platoon )	/* (c) 1987 Ocean */
	DRIVER( vstetris )	/* (c) 1988 Atari */
	DRIVER( mightybj )	/* (c) 1986 Tecmo */
	DRIVER( jajamaru )	/* (c) 1985 Jaleco */
	DRIVER( topgun )	/* (c) 1987 Konami */
	DRIVER( bnglngby )	/* (c) 1985 Nintendo / Broderbund Software Inc. */
	DRIVER( vstennis )	/* (c) 1984 Nintendo */
	DRIVER( wrecking )	/* (c) 1984 Nintendo */
	DRIVER( balonfgt )	/* (c) 1984 Nintendo */
	DRIVER( vsmahjng )	/* (c) 1984 Nintendo */
	DRIVER( vsbball )	/* (c) 1984 Nintendo */
	DRIVER( vsbballj )	/* (c) 1984 Nintendo */
	DRIVER( vsbbalja )	/* (c) 1984 Nintendo */
	DRIVER( iceclmrj )	/* (c) 1984 Nintendo */
	DRIVER( vsgshoe )	/* (c) 1986 Nintendo */
	DRIVER( supxevs )
	DRIVER( vsfdf )
	DRIVER( smgolfb )
	DRIVER( vsbbaljb )

	/* Nintendo Super System Games */
	DRIVER( nss_ssoc )
	DRIVER( nss_actr )
	DRIVER( nss_con3 )
	DRIVER( nss_adam )
	DRIVER( nss_aten )
	DRIVER( nss_rob3 )
	DRIVER( nss_ncaa )
	DRIVER( nss_skin )
	DRIVER( nss_lwep )
	DRIVER( nss_smw )
	DRIVER( nss_fzer )
	DRIVER( nss_sten )

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
	DRIVER( invadpt2 )	/* 852 [1980] Taito */
	DRIVER( invaddlx )	/* 852 [1980] Midway */
	DRIVER( moonbase )	/* Zeta - Nichibutsu */
    	DRIVER( indianbt )	/* Taito - 1980 */
	/* 870 - Space Invaders Deluxe cocktail */
	DRIVER( earthinv )
	DRIVER( spaceatt )
	DRIVER( spaceat2 )
	DRIVER( sinvzen )
	DRIVER( superinv )
	DRIVER( sstrangr )
	DRIVER( sstrngr2 )
	DRIVER( sinvemag )
	DRIVER( jspecter )
	DRIVER( jspectr2 )
	DRIVER( invrvnge )
	DRIVER( invrvnga )
	DRIVER( galxwars )
	DRIVER( galxwar2 )
	DRIVER( galxwart )
	DRIVER( starw )
	DRIVER( lrescue )	/* LR  (c) 1979 Taito */
	DRIVER( grescue )	/* bootleg? */
	DRIVER( desterth )	/* bootleg */
	DRIVER( cosmicmo )	/* Universal */
    	DRIVER( cosmicm2 )	/* Universal */
	DRIVER( rollingc )	/* Nichibutsu */
	DRIVER( ozmawars )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( ozmawar2 )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( solfight )	/* bootleg */
	DRIVER( spaceph )	/* Zilec Games */
	DRIVER( schaser )	/* RT  Taito */
	DRIVER( schasrcv )	/* RT  Taito */
	DRIVER( lupin3 )	/* LP  (c) 1980 Taito */
	DRIVER( spclaser )
	DRIVER( laser )
	DRIVER( spcewarl )
	DRIVER( polaris )	/* PS  (c) 1980 Taito */
	DRIVER( polarisa )	/* PS  (c) 1980 Taito */
	DRIVER( ballbomb )	/* TN  (c) 1980 Taito */
	DRIVER( steelwkr )	/* (c) 1980 Taito */
	DRIVER( m79amb )
	DRIVER( alieninv )
	DRIVER( tst_invd )
	DRIVER( sitv )
	DRIVER( sicv )
	DRIVER( sisv )
	DRIVER( sisv2 )
	DRIVER( spacewr3 )
	DRIVER( invaderl )
    	DRIVER( yosakdoa )
	DRIVER( yosakdon )
	DRIVER( spceking )
	DRIVER( spcewars )
	DRIVER( cosmo )		/* TDS+Mints */
	DRIVER( astropal )
	DRIVER( invasion )
	DRIVER( invasioa )
	DRIVER( invasiob )
	DRIVER( galactic )
	DRIVER( spacmiss )


	DRIVER( sspeedr )	/* 1979 Midway */

	/* Similar but with a M6800 instead of an 8080 */
	DRIVER( sflush ) /* (c)1979 Taito */

	/* Meadows S2650 games */
	DRIVER( lazercmd )	/* [1976?] */
	DRIVER( bbonk )		/* [1976?] */
	DRIVER( deadeye )	/* [1978?] */
	DRIVER( gypsyjug )	/* [1978?] */
	DRIVER( minferno )	/* [1978?] */
	DRIVER( medlanes )	/* [1977?] */

	/* CVS games */
	DRIVER( cosmos )	/* (c) 1981 Century */
	DRIVER( darkwar )	/* (c) 1981 Century */
	DRIVER( spacefrt )	/* (c) 1981 Century */
	DRIVER( 8ball )		/* (c) 1982 Century */
	DRIVER( 8ball1 )
	DRIVER( logger )	/* (c) 1982 Century */
	DRIVER( dazzler )	/* (c) 1982 Century */
	DRIVER( wallst )	/* (c) 1982 Century */
	DRIVER( radarzon )	/* (c) 1982 Century */
	DRIVER( radarzn1 )	/* (c) 1982 Century */
	DRIVER( radarznt )	/* (c) 1982 Tuni Electro Service */
	DRIVER( outline )	/* (c) 1982 Century */
	DRIVER( goldbug )	/* (c) 1982 Century */
	DRIVER( diggerc )	/* (c) 1982 Century */
	DRIVER( heartatk )	/* (c) 1983 Century Electronics */
	DRIVER( hunchbak )	/* (c) 1983 Century */
	DRIVER( superbik )	/* (c) 1983 Century */
	DRIVER( hero )		/* (c) 1983 Seatongrove (c) 1984 CVS */
	DRIVER( huncholy )	/* (c) 1984 Seatongrove (c) CVS */

	/* Midway "Astrocade" games */
	DRIVER( seawolf2 )
	DRIVER( spacezap )	/* (c) 1980 */
	DRIVER( ebases )
	DRIVER( wow )		/* (c) 1980 */
	DRIVER( gorf )		/* (c) 1981 */
	DRIVER( gorfpgm1 )	/* (c) 1981 */
	DRIVER( robby )		/* (c) 1981 Bally Midway */
	DRIVER( profpac )	/* (c) 1983 Bally Midway */

	/* Bally Midway MCR games */
	/* MCR1 */
	DRIVER( solarfox )	/* (c) 1981 */
	DRIVER( kick )		/* (c) 1981 */
	DRIVER( kicka )		/* bootleg? */
	/* MCR2 */
	DRIVER( shollow )	/* (c) 1981 */
	DRIVER( shollow2 )	/* (c) 1981 */
	DRIVER( tron )		/* (c) 1982 */
	DRIVER( tron2 )		/* (c) 1982 */
	DRIVER( tronfp )	/* (c) custom free rom */
	DRIVER( kroozr )	/* (c) 1982 */
	DRIVER( domino )	/* (c) 1982 */
	DRIVER( wacko )		/* (c) 1982 */
	DRIVER( twotiger )	/* (c) 1984 */
	DRIVER( twotigra )	/* (c) 1984 */
	/* MCR2 + MCR3 sprites */
	DRIVER( journey )	/* (c) 1983 */
	/* MCR3 */
	DRIVER( tapper )	/* (c) 1983 */
	DRIVER( tappera )	/* (c) 1983 */
	DRIVER( sutapper )	/* (c) 1983 */
	DRIVER( rbtapper )	/* (c) 1984 */
	DRIVER( timber )	/* (c) 1984 */
	DRIVER( dotron )	/* (c) 1983 */
	DRIVER( dotrona )	/* (c) 1983 */
	DRIVER( dotrone )	/* (c) 1983 */
	DRIVER( demoderb )	/* (c) 1984 */
	DRIVER( demoderm )	/* (c) 1984 */
	DRIVER( sarge )		/* (c) 1985 */
	DRIVER( rampage )	/* (c) 1986 */
	DRIVER( rampage2 )	/* (c) 1986 */
	DRIVER( powerdrv )	/* (c) 1986 */
	DRIVER( stargrds )	/* (c) 1987 */
	DRIVER( maxrpm )	/* (c) 1986 */
	DRIVER( spyhunt )	/* (c) 1983 */
	DRIVER( turbotag )	/* (c) 1985 */
	DRIVER( crater )	/* (c) 1984 */
	/* MCR 68000 */
	DRIVER( zwackery )	/* (c) 1984 */
	DRIVER( xenophob )	/* (c) 1987 */
	DRIVER( spyhunt2 )	/* (c) 1987 */
	DRIVER( spyhnt2a )	/* (c) 1987 */
	DRIVER( blasted )	/* (c) 1988 */
	DRIVER( archrivl )	/* (c) 1989 */
	DRIVER( archriv2 )	/* (c) 1989 */
	DRIVER( trisport )	/* (c) 1989 */
	DRIVER( pigskin )	/* (c) 1990 */

	/* Bally / Sente games */
	DRIVER( sentetst )
	DRIVER( cshift )	/* (c) 1984 */
	DRIVER( gghost )	/* (c) 1984 */
	DRIVER( hattrick )	/* (c) 1984 */
	DRIVER( otwalls )	/* (c) 1984 */
	DRIVER( snakepit )	/* (c) 1984 */
	DRIVER( snakjack )	/* (c) 1984 */
	DRIVER( stocker )	/* (c) 1984 */
	DRIVER( triviag1 )	/* (c) 1984 */
	DRIVER( triviag2 )	/* (c) 1984 */
	DRIVER( triviasp )	/* (c) 1984 */
	DRIVER( triviayp )	/* (c) 1984 */
	DRIVER( triviabb )	/* (c) 1984 */
	DRIVER( gimeabrk )	/* (c) 1985 */
	DRIVER( minigolf )	/* (c) 1985 */
	DRIVER( minigol2 )	/* (c) 1985 */
	DRIVER( toggle )	/* (c) 1985 */
	DRIVER( nametune )	/* (c) 1986 */
	DRIVER( nstocker )	/* (c) 1986 */
	DRIVER( sfootbal )	/* (c) 1986 */
	DRIVER( spiker )	/* (c) 1986 */
	DRIVER( stompin )	/* (c) 1986 */
	DRIVER( rescraid )	/* (c) 1987 */
	DRIVER( rescrdsa )	/* (c) 1987 */
	DRIVER( grudge )
	DRIVER( shrike )	/* (c) 1987 */
	DRIVER( gridlee )	/* [1983 Videa] prototype - no copyright notice */

	/* Irem games */
	/* trivia: IREM means "International Rental Electronics Machines" */
	DRIVER( andromed )
	DRIVER( ipminvad )	/* M10 no copyright notice */
	DRIVER( skychut )	/* Irem [1980] */
	DRIVER( spacbeam )	/* M15 no copyright notice */
	DRIVER( greenber )	/* Irem */

	DRIVER( redalert )	/* (c) 1981 + "GDI presents" */
	DRIVER( demoneye )	/* (c) 1981 */
	DRIVER( olibochu )	/* M47 (c) 1981 + "GDI presents" */
	DRIVER( mpatrol )	/* M52 (c) 1982 */
	DRIVER( mpatrolw )	/* M52 (c) 1982 + Williams license */
	DRIVER( troangel )	/* (c) 1983 */
	DRIVER( yard )		/* (c) 1983 */
	DRIVER( vsyard )	/* (c) 1983/1984 */
	DRIVER( vsyard2 )	/* (c) 1983/1984 */
	DRIVER( travrusa )	/* (c) 1983 */
	DRIVER( motorace )	/* (c) 1983 Williams license */
	DRIVER( shtrider )	/* (c) 1984 Seibu Kaihatsu */
	DRIVER( wilytowr )	/* M63 (c) 1984 */
	DRIVER( atomboy )	/* M63 (c) 1985 Irem + Memetron license */
	/* M62 */
	DRIVER( kungfum )	/* (c) 1984 */
	DRIVER( kungfud )	/* (c) 1984 + Data East license */
	DRIVER( spartanx )	/* (c) 1984 (Japan) */
	DRIVER( kungfub )	/* bootleg */
	DRIVER( kungfub2 )	/* bootleg */
	DRIVER( battroad )	/* (c) 1984 */
	DRIVER( ldrun )		/* (c) 1984 licensed from Broderbund */
	DRIVER( ldruna )	/* (c) 1984 licensed from Broderbund */
	DRIVER( ldrun2 )	/* (c) 1984 licensed from Broderbund */
	DRIVER( ldrun3 )	/* (c) 1985 licensed from Broderbund */
	DRIVER( ldrun3jp )	/* (c) 1985 licensed from Broderbund */
	DRIVER( ldrun4 )	/* (c) 1986 licensed from Broderbund */
	DRIVER( lotlot )	/* (c) 1985 licensed from Tokuma Shoten */
	DRIVER( kidniki )	/* (c) 1986 + Data East USA license */
	DRIVER( yanchamr )	/* (c) 1986 (Japan) */
	DRIVER( spelunkr )	/* (c) 1985 licensed from Broderbund */
	DRIVER( spelnkrj )	/* (c) 1985 licensed from Broderbund */
	DRIVER( spelunk2 )	/* (c) 1986 licensed from Broderbund */
	DRIVER( horizon )	/* (c) 1985 */
	DRIVER( youjyudn )	/* (c) 1986 (Japan) */
	DRIVER( vigilant )	/* (c) 1988 (World) */
	DRIVER( vigilntu )	/* (c) 1988 (US) */
	DRIVER( vigilntj )	/* (c) 1988 (Japan) */
	DRIVER( kikcubic )	/* (c) 1988 (Japan) */
	/* M72 (and derivatives) */
	DRIVER( rtype )		/* (c) 1987 (Japan) */
	DRIVER( rtypepj )	/* (c) 1987 (Japan) */
	DRIVER( rtypeu )	/* (c) 1987 + Nintendo USA license (US) */
	DRIVER( bchopper )	/* (c) 1987 */
	DRIVER( mrheli )	/* (c) 1987 (Japan) */
	DRIVER( nspirit )	/* (c) 1988 */
	DRIVER( nspiritj )	/* (c) 1988 (Japan) */
	DRIVER( imgfight )	/* (c) 1988 (Japan) */
	DRIVER( loht )		/* (c) 1989 */
	DRIVER( xmultipl )	/* (c) 1989 (Japan) */
	DRIVER( dbreed )	/* (c) 1989 */
	DRIVER( rtype2 )	/* (c) 1989 */
	DRIVER( rtype2j )	/* (c) 1989 (Japan) */
	DRIVER( majtitle )	/* (c) 1990 (Japan) */
	DRIVER( hharry )	/* (c) 1990 (World) */
	DRIVER( hharryu )	/* (c) 1990 Irem America (US) */
	DRIVER( dkgensan )	/* (c) 1990 (Japan) */
	DRIVER( dkgenm72 )	/* (c) 1990 (Japan) */
	DRIVER( poundfor )	/* (c) 1990 (World) */
	DRIVER( poundfou )	/* (c) 1990 Irem America (US) */
	DRIVER( airduel )	/* (c) 1990 (Japan) */
	DRIVER( cosmccop )	/* (c) 1991 (World) */
	DRIVER( gallop )	/* (c) 1991 (Japan) */
	DRIVER( kengo )		/* (c) 1991 */
	/* not M72, but same sound hardware */
	DRIVER( sichuan2 )	/* (c) 1989 Tamtex */
	DRIVER( sichuana )	/* (c) 1989 Tamtex */
	DRIVER( shisen )	/* (c) 1989 Tamtex */
	DRIVER( matchit )	/* (c) 1989 Tamtex */
	/* M90 */
	DRIVER( hasamu )	/* (c) 1991 Irem (Japan) */
	DRIVER( dynablst )	/* (c) 1991 Irem (World) */
	DRIVER( dynablsb )	/* bootleg */
	DRIVER( dicegame )  /* bootleg */
	DRIVER( bombrman )	/* (c) 1991 Irem (Japan) */
	/* M97 */
	DRIVER( bbmanw )	/* (c) 1992 Irem (World) */
	DRIVER( bbmanwj )	/* (c) 1992 Irem (Japan) */
	DRIVER( atompunk )	/* (c) 1992 Irem America (US) */
	DRIVER( quizf1 )	/* (c) 1992 Irem (Japan) */
	DRIVER( riskchal )  /* (c) 1993 Irem (World) */
	DRIVER( gussun )    /* (c) 1993 Irem (Japan) */
	DRIVER( matchit2 )  /* (c) 1993 Irem (World) */
	DRIVER( shisen2 )   /* (c) 1992 Irem (Japan) */
	/* M92 */
	DRIVER( gunforce )	/* (c) 1991 Irem (World) */
	DRIVER( gunforcu )	/* (c) 1991 Irem America (US) */
	DRIVER( gunforcj )	/* (c) 1991 Irem (Japan) */
	DRIVER( bmaster )	/* (c) 1991 Irem */
	DRIVER( lethalth )	/* (c) 1991 Irem (World) */
	DRIVER( thndblst )	/* (c) 1991 Irem (Japan) */
	DRIVER( uccops )	/* (c) 1992 Irem (World) */
	DRIVER( uccopsar )      /* (c) 1992 Irem America (US) */
	DRIVER( uccopsj )	/* (c) 1992 Irem (Japan) */
	DRIVER( mysticri )	/* (c) 1992 Irem (World) */
	DRIVER( gunhohki )	/* (c) 1992 Irem (Japan) */
	DRIVER( majtitl2 )	/* (c) 1992 Irem (World) */
	DRIVER( skingame )	/* (c) 1992 Irem America (US) */
	DRIVER( skingam2 )	/* (c) 1992 Irem America (US) */
	DRIVER( hook )		/* (c) 1992 Irem (World) */
	DRIVER( hooku )		/* (c) 1992 Irem America (US) */
	DRIVER( hookj )		/* (c) 1992 Irem (Japan) */
	DRIVER( rtypeleo )	/* (c) 1992 Irem (World) */
	DRIVER( rtypelej )	/* (c) 1992 Irem (Japan) */
	DRIVER( inthunt )	/* (c) 1993 Irem (World) */
	DRIVER( inthuntu )	/* (c) 1993 Irem (US) */
	DRIVER( kaiteids )	/* (c) 1993 Irem (Japan) */
	DRIVER( nbbatman )	/* (c) 1993 Irem America (US) */
	DRIVER( leaguemn )	/* (c) 1993 Irem (Japan) */
	DRIVER( ssoldier )	/* (c) 1993 Irem America (US) */
	DRIVER( psoldier )	/* (c) 1993 Irem (Japan) */
	DRIVER( dsccr94j )	/* (c) 1994 Irem (Japan) */
	DRIVER( gunforc2 )	/* (c) 1994 Irem */
	DRIVER( geostorm )	/* (c) 1994 Irem (Japan) */
	/* M107 */
	DRIVER( firebarr )	/* (c) 1993 Irem (Japan) */
	DRIVER( dsoccr94 )	/* (c) 1994 Irem (Data East Corporation license) */
	DRIVER( wpksoc )


#endif	/* DRIVER_RECURSIVE */
