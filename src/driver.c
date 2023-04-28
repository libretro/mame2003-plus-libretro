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

#endif	/* DRIVER_RECURSIVE */
