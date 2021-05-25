/***************************************************************************

  inptport.c

  Input ports handling

TODO:	remove the 1 analog device per port limitation
		support for inputports producing interrupts
		support for extra "real" hardware (PC throttle's, spinners etc)

                 Controller-Specific Input Port Mappings
                 ---------------------------------------

The main purpose of the controller-specific configuration is to remap
inputs.  This is handled via two mechanisms: key re-mapping and
sequence re-mapping.

Key re-mapping occurs at the most basic level.  The specified keycode is
replaced where ever it occurs (in all sequences) with the specified
replacement.  Only single keycodes can be used as a replacement (that is, a
single key cannot be replaced with a key sequence).

Sequence mapping occurs at a higher level.  In this case, the entire
key sequence for the given input is replaced with the specified key
sequence.

Keycodes are specified as a single keyword.  Key sequences are specified
as a series of keycodes separated by spaces (the full sequence must be
enclosed in quotes if spaces exist).  Two special keywords are available
for defining key sequences, CODE_OR and CODE_NOT (which can abbreviated |
and ! respectively).

When two keycodes are specified together (separated by only whitespace),
the default action is a logical AND, such that both keys must be pressed
at the same time for the action to occur.  Often it desired that either
key can be pressed for the action to occur (for example LEFT CTRL and
Joystick Button 0), in which case the two keycodes need to be separated
by a CODE_OR (|) keyword.  Finally, certain combinations may be
undesirable and warrant no action by MAME.  For these, the keywords should
be specified by a CODE_NOT (!) (for example, ALT-TAB on a windows machine).

An example file containing all key sequences and the standard definitions
can be found in ctrlr\std.ini.  This file is NOT parsed by MAME and is
only provided for reference.  Lines can be copied from this file to create
a controller customization file.  When creating a file, keep the number of
redefinitions to a minimum.  Any input not listed will default to the
standard sequence.  Note that several files may be parsed for any given
controller/game combination, so an input may be re-defined in multiple
places.

For support of controller input customization, two new command-line
options were added:

-ctrlr_directory "name"

Specifies the directory that contains the controller customization .ini files.

-ctrlr "name"

Specifies a controller name for customization.  The .ini files for the
controller are stored in either a directory or a .zip file with the same
name as the specified controller.

The first file in the directory/zip to be scanned is the file default.ini.
From there, game-specific files are scanned starting with the top-most
parent file and continuing down to the specified game name.

Once the default.ini and all game-specific files are scanned, additional
files may be scanned depending on the game type.  The files to be scanned
are specified by the following custom keywords:

dial_ini                "name"
paddle_ini              "name"
pedal_ini               "name"
trackball_ini           "name"
ad_stick_ini            "name"

Each of these keywords specify the name of an .ini file that will be
parsed if the game has one of these types of inputs.  For example,
Tempest, which  has a dial input, will automatically parse the file
specified by the dial_ini keyword.  Typically, all of the x_ini keywords
will point to the same file (e.g. mouse.ini) which enables mouse and/or
joystick support.  In this way, mouse and joystick support will only be
enabled on those games that require it.  If all games for this controller
require joystick and/or mouse support, those keywords can be placed in
the main default.ini file.

All of the options that can be specified on the command-line or in the
main mame.ini file can be specified in the controller-specific ini files,
though because of when these files are parsed, the specified options may
have no effect.  The only two general options guaranteed to be supported
are mouse and joystick.  Note that command-line specification of these
options takes precedence.  Since most front-ends, including MAME32,
specify these options through the command-line, the mouse and/or joystick
options specified in the .ini files will be ignored.

Another custom keyword is:

ctrlrname               "name"

This keyword defines a detailed name for the selected controller.  Names
that include spaces must be enclosed in quotes.


Keywords:
---------

Keywords are separated into two categories, keycodes and input port
definitions.  To specify a key re-mapping, specify the keycode as the
keyword to re-define (that is, on the left-hand side) followed by the
replacement code.  To specify a sequence re-map, specify the input port
code to be re-defined on the left followed by the sequence.

In addition to the standard codes, additional OSD specific codes may be
generated.  The keycodes added are typically generated based on connected
hardware.  For example, in the windows port, direct input is polled to
determine what inputs are available and assign names.

With respect to the windows port, the generated keycodes are identical
to those displayed in the configuration menu with all spaces replaced by
underscores.  For example, the keycodes automatically generated for a
Wingman Warrior joystick are:

J1_X-axis_+        J1_X-axis_-        J1_Y-axis_+        J1_Y-axis_-
J1_Z-axis_+        J1_Z-axis_-        J1_Rz-axis_+       J1_Rz-axis_-
J1_Button_0        J1_Button_1        J1_Button_2        J1_Button_3
J1_POV_0_U         J1_POV_0_D         J1_POV_0_L         J1_POV_0_R

The names generated will vary with the port to which the joystick is
connected.  In the codes listed above, the Wingman Warrior was connected as
the first joystick (J1).  If it had been configured as a different input,
the generated codes would have a different Jx number.

All keyword matching including the standard keywords is case sensitive.
Also, some of the automatically generated OSD codes may be redundant.
For example, J1_Button_0 is the same as JOYCODE_1_BUTTON1.  Standard codes
are preferred over OSD codes.


The standard keycodes are:
--------------------------

KEYCODE_A                KEYCODE_B                KEYCODE_C
KEYCODE_D                KEYCODE_E                KEYCODE_F
KEYCODE_G                KEYCODE_H                KEYCODE_I
KEYCODE_J                KEYCODE_K                KEYCODE_L
KEYCODE_M                KEYCODE_N                KEYCODE_O
KEYCODE_P                KEYCODE_Q                KEYCODE_R
KEYCODE_S                KEYCODE_T                KEYCODE_U
KEYCODE_V                KEYCODE_W                KEYCODE_X
KEYCODE_Y                KEYCODE_Z                KEYCODE_0
KEYCODE_1                KEYCODE_2                KEYCODE_3
KEYCODE_4                KEYCODE_5                KEYCODE_6
KEYCODE_7                KEYCODE_8                KEYCODE_9
KEYCODE_0_PAD            KEYCODE_1_PAD            KEYCODE_2_PAD
KEYCODE_3_PAD            KEYCODE_4_PAD            KEYCODE_5_PAD
KEYCODE_6_PAD            KEYCODE_7_PAD            KEYCODE_8_PAD
KEYCODE_9_PAD            KEYCODE_F1               KEYCODE_F2
KEYCODE_F3               KEYCODE_F4               KEYCODE_F5
KEYCODE_F6               KEYCODE_F7               KEYCODE_F8
KEYCODE_F9               KEYCODE_F10              KEYCODE_F11
KEYCODE_F12              KEYCODE_ESC              KEYCODE_TILDE
KEYCODE_MINUS            KEYCODE_EQUALS           KEYCODE_BACKSPACE
KEYCODE_TAB              KEYCODE_OPENBRACE        KEYCODE_CLOSEBRACE
KEYCODE_ENTER            KEYCODE_COLON            KEYCODE_QUOTE
KEYCODE_BACKSLASH        KEYCODE_BACKSLASH2       KEYCODE_COMMA
KEYCODE_STOP             KEYCODE_SLASH            KEYCODE_SPACE
KEYCODE_INSERT           KEYCODE_DEL              KEYCODE_HOME
KEYCODE_END              KEYCODE_PGUP             KEYCODE_PGDN
KEYCODE_LEFT             KEYCODE_RIGHT            KEYCODE_UP
KEYCODE_DOWN             KEYCODE_SLASH_PAD        KEYCODE_ASTERISK
KEYCODE_MINUS_PAD        KEYCODE_PLUS_PAD         KEYCODE_DEL_PAD
KEYCODE_ENTER_PAD        KEYCODE_PRTSCR           KEYCODE_PAUSE
KEYCODE_LSHIFT           KEYCODE_RSHIFT           KEYCODE_LCONTROL
KEYCODE_RCONTROL         KEYCODE_LALT             KEYCODE_RALT
KEYCODE_SCRLOCK          KEYCODE_NUMLOCK          KEYCODE_CAPSLOCK
KEYCODE_LWIN             KEYCODE_RWIN             KEYCODE_MENU

JOYCODE_1_LEFT           JOYCODE_1_RIGHT          JOYCODE_1_UP
JOYCODE_1_DOWN           JOYCODE_1_BUTTON1        JOYCODE_1_BUTTON2
JOYCODE_1_BUTTON3        JOYCODE_1_BUTTON4        JOYCODE_1_BUTTON5
JOYCODE_1_BUTTON6        JOYCODE_1_START          JOYCODE_1_SELECT
JOYCODE_2_LEFT           JOYCODE_2_RIGHT          JOYCODE_2_UP
JOYCODE_2_DOWN           JOYCODE_2_BUTTON1        JOYCODE_2_BUTTON2
JOYCODE_2_BUTTON3        JOYCODE_2_BUTTON4        JOYCODE_2_BUTTON5
JOYCODE_2_BUTTON6        JOYCODE_2_START          JOYCODE_2_SELECT
JOYCODE_3_LEFT           JOYCODE_3_RIGHT          JOYCODE_3_UP
JOYCODE_3_DOWN           JOYCODE_3_BUTTON1        JOYCODE_3_BUTTON2
JOYCODE_3_BUTTON3        JOYCODE_3_BUTTON4        JOYCODE_3_BUTTON5
JOYCODE_3_BUTTON6        JOYCODE_3_START          JOYCODE_3_SELECT
JOYCODE_4_LEFT           JOYCODE_4_RIGHT          JOYCODE_4_UP
JOYCODE_4_DOWN           JOYCODE_4_BUTTON1        JOYCODE_4_BUTTON2
JOYCODE_4_BUTTON3        JOYCODE_4_BUTTON4        JOYCODE_4_BUTTON5
JOYCODE_4_BUTTON6        JOYCODE_4_START          JOYCODE_4_SELECT

MOUSECODE_1_BUTTON1      MOUSECODE_1_BUTTON2      MOUSECODE_1_BUTTON3

KEYCODE_NONE             CODE_NONE                CODE_OTHER
CODE_DEFAULT             CODE_PREVIOUS            CODE_NOT
CODE_OR                  !                        |



The input port codes are:
-------------------------

UI_CONFIGURE             UI_ON_SCREEN_DISPLAY     UI_PAUSE
UI_RESET_MACHINE         UI_SHOW_GFX              UI_FRAMESKIP_DEC
UI_FRAMESKIP_INC         UI_THROTTLE              UI_SHOW_FPS
UI_SHOW_PROFILER         UI_SNAPSHOT              UI_TOGGLE_CHEAT
UI_UP                    UI_DOWN                  UI_LEFT
UI_RIGHT                 UI_SELECT                UI_CANCEL
UI_PAN_UP                UI_PAN_DOWN              UI_PAN_LEFT
UI_PAN_RIGHT             UI_TOGGLE_DEBUG          UI_SAVE_STATE
UI_LOAD_STATE            UI_ADD_CHEAT             UI_DELETE_CHEAT
UI_SAVE_CHEAT            UI_WATCH_VALUE           UI_EDIT_CHEAT
START1                   START2                   START3
START4                   COIN1                    COIN2
COIN3                    COIN4                    SERVICE1
SERVICE2                 SERVICE3                 SERVICE4
TILT

P1_JOYSTICK_UP           P1_JOYSTICK_DOWN         P1_JOYSTICK_LEFT
P1_JOYSTICK_RIGHT        P1_BUTTON1               P1_BUTTON2
P1_BUTTON3               P1_BUTTON4               P1_BUTTON5
P1_BUTTON6               P1_BUTTON7               P1_BUTTON8
P1_BUTTON9               P1_BUTTON10              P1_JOYSTICKRIGHT_UP
P1_JOYSTICKRIGHT_DOWN    P1_JOYSTICKRIGHT_LEFT    P1_JOYSTICKRIGHT_RIGHT
P1_JOYSTICKLEFT_UP       P1_JOYSTICKLEFT_DOWN     P1_JOYSTICKLEFT_LEFT
P1_JOYSTICKLEFT_RIGHT

P2_JOYSTICK_UP           P2_JOYSTICK_DOWN         P2_JOYSTICK_LEFT
P2_JOYSTICK_RIGHT        P2_BUTTON1               P2_BUTTON2
P2_BUTTON3               P2_BUTTON4               P2_BUTTON5
P2_BUTTON6               P2_BUTTON7               P2_BUTTON8
P2_BUTTON9               P2_BUTTON10              P2_JOYSTICKRIGHT_UP
P2_JOYSTICKRIGHT_DOWN    P2_JOYSTICKRIGHT_LEFT    P2_JOYSTICKRIGHT_RIGHT
P2_JOYSTICKLEFT_UP       P2_JOYSTICKLEFT_DOWN     P2_JOYSTICKLEFT_LEFT
P2_JOYSTICKLEFT_RIGHT

P3_JOYSTICK_UP           P3_JOYSTICK_DOWN         P3_JOYSTICK_LEFT
P3_JOYSTICK_RIGHT        P3_BUTTON1               P3_BUTTON2
P3_BUTTON3               P3_BUTTON4

P4_JOYSTICK_UP           P4_JOYSTICK_DOWN         P4_JOYSTICK_LEFT
P4_JOYSTICK_RIGHT        P4_BUTTON1               P4_BUTTON2
P4_BUTTON3               P4_BUTTON4

P1_PEDAL                 P1_PEDAL_EXT             P2_PEDAL
P2_PEDAL_EXT             P3_PEDAL                 P3_PEDAL_EXT
P4_PEDAL                 P4_PEDAL_EXT

P1_PADDLE                P1_PADDLE_EXT            P2_PADDLE
P2_PADDLE_EXT            P3_PADDLE                P3_PADDLE_EXT
P4_PADDLE                P4_PADDLE_EXT            P1_PADDLE_V
P1_PADDLE_V_EXT          P2_PADDLE_V              P2_PADDLE_V_EXT
P3_PADDLE_V              P3_PADDLE_V_EXT          P4_PADDLE_V
P4_PADDLE_V_EXT

P1_DIAL                  P1_DIAL_EXT              P2_DIAL
P2_DIAL_EXT              P3_DIAL                  P3_DIAL_EXT
P4_DIAL                  P4_DIAL_EXT              P1_DIAL_V
P1_DIAL_V_EXT            P2_DIAL_V                P2_DIAL_V_EXT
P3_DIAL_V                P3_DIAL_V_EXT            P4_DIAL_V
P4_DIAL_V_EXT

P1_TRACKBALL_X           P1_TRACKBALL_X_EXT       P2_TRACKBALL_X
P2_TRACKBALL_X_EXT       P3_TRACKBALL_X           P3_TRACKBALL_X_EXT
P4_TRACKBALL_X           P4_TRACKBALL_X_EXT

P1_TRACKBALL_Y           P1_TRACKBALL_Y_EXT       P2_TRACKBALL_Y
P2_TRACKBALL_Y_EXT       P3_TRACKBALL_Y           P3_TRACKBALL_Y_EXT
P4_TRACKBALL_Y           P4_TRACKBALL_Y_EXT

P1_AD_STICK_X            P1_AD_STICK_X_EXT        P2_AD_STICK_X
P2_AD_STICK_X_EXT        P3_AD_STICK_X            P3_AD_STICK_X_EXT
P4_AD_STICK_X            P4_AD_STICK_X_EXT

P1_AD_STICK_Y            P1_AD_STICK_Y_EXT        P2_AD_STICK_Y
P2_AD_STICK_Y_EXT        P3_AD_STICK_Y            P3_AD_STICK_Y_EXT
P4_AD_STICK_Y            P4_AD_STICK_Y_EXT

OSD_1                    OSD_2                    OSD_3
OSD_4



***************************************************************************/

#include <math.h>
#include "driver.h"
#include "config.h"


/***************************************************************************

	Extern declarations

***************************************************************************/

extern void *record;
extern void *playback;

extern unsigned int dispensed_tickets;
extern unsigned int coins[COIN_COUNTERS];
extern unsigned int lastcoin[COIN_COUNTERS];
extern unsigned int coinlockedout[COIN_COUNTERS];

extern int legacy_flag;

static unsigned short input_port_value[MAX_INPUT_PORTS];
static unsigned short input_vblank[MAX_INPUT_PORTS];


/***************************************************************************

	Local variables

***************************************************************************/

/* Assuming a maxium of one analog input device per port BW 101297 */
static struct InputPort *input_analog[MAX_INPUT_PORTS];
static int input_analog_current_value[MAX_INPUT_PORTS], input_analog_previous_value[MAX_INPUT_PORTS];
static int          input_analog_init[MAX_INPUT_PORTS];
static int         input_analog_scale[MAX_INPUT_PORTS];

/* [player#][mame axis#] array */
static InputCode      analogjoy_input[MAX_PLAYER_COUNT][MAX_ANALOG_AXES];
	
static int           mouse_delta_axis[MAX_PLAYER_COUNT][MAX_ANALOG_AXES];
static int        lightgun_delta_axis[MAX_PLAYER_COUNT][MAX_ANALOG_AXES];
static int        analog_current_axis[MAX_PLAYER_COUNT][MAX_ANALOG_AXES];
static int       analog_previous_axis[MAX_PLAYER_COUNT][MAX_ANALOG_AXES];


/***************************************************************************

  Configuration load/save

***************************************************************************/

/* this must match the enum in inptport.h */
const char ipdn_defaultstrings[][MAX_DEFSTR_LEN] =
{
	"Off",
	"On",
	"No",
	"Yes",
	"Lives",
	"Bonus Life",
	"Difficulty",
	"Demo Sounds",
	"Coinage",
	"Coin A",
	"Coin B",
	"9 Coins/1 Credit",
	"8 Coins/1 Credit",
	"7 Coins/1 Credit",
	"6 Coins/1 Credit",
	"5 Coins/1 Credit",
	"4 Coins/1 Credit",
	"3 Coins/1 Credit",
	"8 Coins/3 Credits",
	"4 Coins/2 Credits",
	"2 Coins/1 Credit",
	"5 Coins/3 Credits",
	"3 Coins/2 Credits",
	"4 Coins/3 Credits",
	"4 Coins/4 Credits",
	"3 Coins/3 Credits",
	"2 Coins/2 Credits",
	"1 Coin/1 Credit",
	"4 Coins/5 Credits",
	"3 Coins/4 Credits",
	"2 Coins/3 Credits",
	"4 Coins/7 Credits",
	"2 Coins/4 Credits",
	"1 Coin/2 Credits",
	"2 Coins/5 Credits",
	"2 Coins/6 Credits",
	"1 Coin/3 Credits",
	"2 Coins/7 Credits",
	"2 Coins/8 Credits",
	"1 Coin/4 Credits",
	"1 Coin/5 Credits",
	"1 Coin/6 Credits",
	"1 Coin/7 Credits",
	"1 Coin/8 Credits",
	"1 Coin/9 Credits",
	"Free Play",
	"Cabinet",
	"Upright",
	"Cocktail",
	"Flip Screen",
	"Service Mode",
	"Unused",
	"Unknown"
};


struct ipd inputport_defaults[] =
{
  { IPT_UI_CONFIGURE,         "Config Menu",    SEQ_DEF_3(KEYCODE_TAB,   CODE_OR, JOYCODE_1_BUTTON9) },
  { IPT_UI_SHOW_GFX,          "Show Gfx",       SEQ_DEF_1(KEYCODE_NONE) },
  { IPT_UI_TOGGLE_CHEAT,      "Toggle Cheat",   SEQ_DEF_1(KEYCODE_NONE) },
  { IPT_UI_UP,                "UI Up",          SEQ_DEF_7(KEYCODE_UP,    CODE_OR, JOYCODE_1_UP,       CODE_OR,   JOYCODE_1_LEFT_UP,    CODE_OR,   JOYCODE_GUN_1_DPAD_UP )     },
  { IPT_UI_DOWN,              "UI Down",        SEQ_DEF_7(KEYCODE_DOWN,  CODE_OR, JOYCODE_1_DOWN,     CODE_OR,   JOYCODE_1_LEFT_DOWN,  CODE_OR,   JOYCODE_GUN_1_DPAD_DOWN )     },
  { IPT_UI_LEFT,              "UI Left",        SEQ_DEF_7(KEYCODE_LEFT,  CODE_OR, JOYCODE_1_LEFT,     CODE_OR,   JOYCODE_1_LEFT_LEFT,  CODE_OR,   JOYCODE_GUN_1_DPAD_LEFT )   },
  { IPT_UI_RIGHT,             "UI Right",       SEQ_DEF_7(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT,    CODE_OR,   JOYCODE_1_LEFT_RIGHT, CODE_OR,   JOYCODE_GUN_1_DPAD_RIGHT )  },
  { IPT_UI_SELECT,            "UI Select",      SEQ_DEF_5(KEYCODE_ENTER, CODE_OR, JOYCODE_1_BUTTON2,  CODE_OR,   JOYCODE_GUN_1_BUTTON2 ) },
  { IPT_UI_CANCEL,            "UI Cancel",      SEQ_DEF_5(KEYCODE_ESC,   CODE_OR, JOYCODE_1_BUTTON1,  CODE_OR,   JOYCODE_GUN_1_BUTTON1 ) },
  { IPT_UI_ADD_CHEAT,         "Add Cheat",      SEQ_DEF_1(KEYCODE_NONE) },
  { IPT_UI_DELETE_CHEAT,      "Delete Cheat",   SEQ_DEF_1(KEYCODE_NONE) },
  { IPT_UI_SAVE_CHEAT,        "Save Cheat",     SEQ_DEF_1(KEYCODE_NONE) },
  { IPT_UI_WATCH_VALUE,       "Watch Value",    SEQ_DEF_1(KEYCODE_NONE) },
  { IPT_UI_EDIT_CHEAT,        "Edit Cheat",     SEQ_DEF_1(KEYCODE_NONE) },

  { IPT_START1,   "P1 Start",      SEQ_DEF_5(KEYCODE_1, CODE_OR, JOYCODE_1_START, CODE_OR, JOYCODE_GUN_1_START) },
  { IPT_START2,   "P2 Start",      SEQ_DEF_5(KEYCODE_2, CODE_OR, JOYCODE_2_START, CODE_OR, JOYCODE_GUN_2_START) },
  { IPT_START3,   "P3 Start",      SEQ_DEF_5(KEYCODE_3, CODE_OR, JOYCODE_3_START, CODE_OR, JOYCODE_GUN_3_START) },
  { IPT_START4,   "P4 Start",      SEQ_DEF_5(KEYCODE_4, CODE_OR, JOYCODE_4_START, CODE_OR, JOYCODE_GUN_4_START) },
  { IPT_START5,   "P5 Start",      SEQ_DEF_3(JOYCODE_5_START, CODE_OR, JOYCODE_GUN_5_START) },
  { IPT_START6,   "P6 Start",      SEQ_DEF_3(JOYCODE_6_START, CODE_OR, JOYCODE_GUN_6_START) },
  { IPT_START7,   "P7 Start",      SEQ_DEF_3(JOYCODE_7_START, CODE_OR, JOYCODE_GUN_7_START) },
  { IPT_START8,   "P8 Start",      SEQ_DEF_3(JOYCODE_8_START, CODE_OR, JOYCODE_GUN_8_START) },
  { IPT_COIN1,    "P1 Coin",       SEQ_DEF_5(KEYCODE_5, CODE_OR, JOYCODE_1_SELECT, CODE_OR, JOYCODE_GUN_1_SELECT) },
  { IPT_COIN2,    "P2 Coin",       SEQ_DEF_5(KEYCODE_6, CODE_OR, JOYCODE_2_SELECT, CODE_OR, JOYCODE_GUN_2_SELECT) },
  { IPT_COIN3,    "P3 Coin",       SEQ_DEF_5(KEYCODE_7, CODE_OR, JOYCODE_3_SELECT, CODE_OR, JOYCODE_GUN_3_SELECT) },
  { IPT_COIN4,    "P4 Coin",       SEQ_DEF_5(KEYCODE_8, CODE_OR, JOYCODE_4_SELECT, CODE_OR, JOYCODE_GUN_4_SELECT) },
  { IPT_COIN5,    "P5 Coin",       SEQ_DEF_3(JOYCODE_5_SELECT, CODE_OR, JOYCODE_GUN_5_SELECT) },
  { IPT_COIN6,    "P6 Coin",       SEQ_DEF_3(JOYCODE_6_SELECT, CODE_OR, JOYCODE_GUN_6_SELECT) },
  { IPT_COIN7,    "P7 Coin",       SEQ_DEF_3(JOYCODE_7_SELECT, CODE_OR, JOYCODE_GUN_7_SELECT) },
  { IPT_COIN8,    "P8 Coin",       SEQ_DEF_3(JOYCODE_8_SELECT, CODE_OR, JOYCODE_GUN_8_SELECT) },

  { IPT_SERVICE1, "Service 1",     SEQ_DEF_1(KEYCODE_9)      },
  { IPT_SERVICE2, "Service 2",     SEQ_DEF_1(KEYCODE_0)      },
  { IPT_SERVICE3, "Service 3",     SEQ_DEF_1(KEYCODE_MINUS)  },
  { IPT_SERVICE4, "Service 4",     SEQ_DEF_1(KEYCODE_EQUALS) },
  { IPT_TILT,     "Tilt",          SEQ_DEF_1(KEYCODE_T)      },

  { IPT_JOYSTICK_UP         | IPF_PLAYER1, "P1 Up",          SEQ_DEF_7(KEYCODE_UP,       CODE_OR, JOYCODE_1_UP,          CODE_OR, JOYCODE_1_LEFT_UP,        CODE_OR, JOYCODE_GUN_1_DPAD_UP )       },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER1, "P1 Down",        SEQ_DEF_7(KEYCODE_DOWN,     CODE_OR, JOYCODE_1_DOWN,        CODE_OR, JOYCODE_1_LEFT_DOWN,      CODE_OR, JOYCODE_GUN_1_DPAD_DOWN )     },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER1, "P1 Left",        SEQ_DEF_7(KEYCODE_LEFT,     CODE_OR, JOYCODE_1_LEFT,        CODE_OR, JOYCODE_1_LEFT_LEFT,      CODE_OR, JOYCODE_GUN_1_DPAD_LEFT )     },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER1, "P1 Right",       SEQ_DEF_7(KEYCODE_RIGHT,    CODE_OR, JOYCODE_1_RIGHT,       CODE_OR, JOYCODE_1_LEFT_RIGHT,     CODE_OR, JOYCODE_GUN_1_DPAD_RIGHT )    },
  { IPT_BUTTON1             | IPF_PLAYER1, "P1 Button 1",    SEQ_DEF_7(KEYCODE_LCONTROL, CODE_OR, JOYCODE_1_BUTTON1,     CODE_OR, JOYCODE_MOUSE_1_BUTTON1,  CODE_OR, JOYCODE_GUN_1_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER1, "P1 Button 2",    SEQ_DEF_7(KEYCODE_LALT,     CODE_OR, JOYCODE_1_BUTTON2,     CODE_OR, JOYCODE_MOUSE_1_BUTTON2,  CODE_OR, JOYCODE_GUN_1_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER1, "P1 Button 3",    SEQ_DEF_7(KEYCODE_SPACE,    CODE_OR, JOYCODE_1_BUTTON3,     CODE_OR, JOYCODE_MOUSE_1_BUTTON3,  CODE_OR, JOYCODE_GUN_1_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER1, "P1 Button 4",    SEQ_DEF_7(KEYCODE_LSHIFT,   CODE_OR, JOYCODE_1_BUTTON4,     CODE_OR, JOYCODE_MOUSE_1_BUTTON4,  CODE_OR, JOYCODE_1_GUN_BUTTON4 )  },
  { IPT_BUTTON5             | IPF_PLAYER1, "P1 Button 5",    SEQ_DEF_3(KEYCODE_Z,        CODE_OR, JOYCODE_1_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER1, "P1 Button 6",    SEQ_DEF_3(KEYCODE_X,        CODE_OR, JOYCODE_1_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER1, "P1 Button 7",    SEQ_DEF_3(KEYCODE_C,        CODE_OR, JOYCODE_1_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER1, "P1 Button 8",    SEQ_DEF_3(KEYCODE_V,        CODE_OR, JOYCODE_1_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER1, "P1 Button 9",    SEQ_DEF_3(KEYCODE_B,        CODE_OR, JOYCODE_1_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER1, "P1 Button 10",   SEQ_DEF_3(KEYCODE_N,        CODE_OR, JOYCODE_1_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER1, "P1 Right/Up",    SEQ_DEF_5(KEYCODE_I,        CODE_OR, JOYCODE_1_RIGHT_UP,    CODE_OR, JOYCODE_2_UP)    },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER1, "P1 Right/Down",  SEQ_DEF_5(KEYCODE_K,        CODE_OR, JOYCODE_1_RIGHT_DOWN,  CODE_OR, JOYCODE_2_DOWN)  },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER1, "P1 Right/Left",  SEQ_DEF_5(KEYCODE_J,        CODE_OR, JOYCODE_1_RIGHT_LEFT,  CODE_OR, JOYCODE_2_LEFT)  },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER1, "P1 Right/Right", SEQ_DEF_5(KEYCODE_L,        CODE_OR, JOYCODE_1_RIGHT_RIGHT, CODE_OR, JOYCODE_2_RIGHT) },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER1, "P1 Left/Up",     SEQ_DEF_5(KEYCODE_E,        CODE_OR, JOYCODE_1_LEFT_UP,     CODE_OR, JOYCODE_1_UP)    },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER1, "P1 Left/Down",   SEQ_DEF_5(KEYCODE_D,        CODE_OR, JOYCODE_1_LEFT_DOWN,   CODE_OR, JOYCODE_1_DOWN)  },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER1, "P1 Left/Left",   SEQ_DEF_5(KEYCODE_S,        CODE_OR, JOYCODE_1_LEFT_LEFT,   CODE_OR, JOYCODE_1_LEFT)  },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER1, "P1 Left/Right",  SEQ_DEF_5(KEYCODE_F,        CODE_OR, JOYCODE_1_LEFT_RIGHT,  CODE_OR, JOYCODE_1_RIGHT) },

  { IPT_JOYSTICK_UP         | IPF_PLAYER2, "P2 Up",          SEQ_DEF_7(KEYCODE_R,             CODE_OR, JOYCODE_2_UP,         CODE_OR, JOYCODE_2_LEFT_UP,         CODE_OR, JOYCODE_GUN_2_DPAD_UP )       },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER2, "P2 Down",        SEQ_DEF_7(KEYCODE_F,             CODE_OR, JOYCODE_2_DOWN,       CODE_OR, JOYCODE_2_LEFT_DOWN,       CODE_OR, JOYCODE_GUN_2_DPAD_DOWN )     },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER2, "P2 Left",        SEQ_DEF_7(KEYCODE_D,             CODE_OR, JOYCODE_2_LEFT,       CODE_OR, JOYCODE_2_LEFT_LEFT,       CODE_OR, JOYCODE_GUN_2_DPAD_LEFT )     },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER2, "P2 Right",       SEQ_DEF_7(KEYCODE_G,             CODE_OR, JOYCODE_2_RIGHT,      CODE_OR, JOYCODE_2_LEFT_RIGHT,      CODE_OR, JOYCODE_GUN_2_DPAD_RIGHT )    },
  { IPT_BUTTON1             | IPF_PLAYER2, "P2 Button 1",    SEQ_DEF_7(KEYCODE_A,             CODE_OR, JOYCODE_2_BUTTON1,    CODE_OR, JOYCODE_MOUSE_2_BUTTON1,   CODE_OR, JOYCODE_GUN_2_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER2, "P2 Button 2",    SEQ_DEF_7(KEYCODE_S,             CODE_OR, JOYCODE_2_BUTTON2,    CODE_OR, JOYCODE_MOUSE_2_BUTTON2,   CODE_OR, JOYCODE_GUN_2_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER2, "P2 Button 3",    SEQ_DEF_7(KEYCODE_Q,             CODE_OR, JOYCODE_2_BUTTON3,    CODE_OR, JOYCODE_MOUSE_2_BUTTON3,   CODE_OR, JOYCODE_GUN_2_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER2, "P2 Button 4",    SEQ_DEF_7(KEYCODE_W,             CODE_OR, JOYCODE_2_BUTTON4,    CODE_OR, JOYCODE_MOUSE_2_BUTTON4,   CODE_OR, JOYCODE_GUN_2_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER2, "P2 Button 5",    SEQ_DEF_1(JOYCODE_2_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER2, "P2 Button 6",    SEQ_DEF_1(JOYCODE_2_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER2, "P2 Button 7",    SEQ_DEF_1(JOYCODE_2_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER2, "P2 Button 8",    SEQ_DEF_1(JOYCODE_2_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER2, "P2 Button 9",    SEQ_DEF_1(JOYCODE_2_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER2, "P2 Button 10",   SEQ_DEF_1(JOYCODE_2_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER2, "P2 Right/Up",    SEQ_DEF_3(JOYCODE_2_RIGHT_UP,    CODE_OR, JOYCODE_4_UP)    },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER2, "P2 Right/Down",  SEQ_DEF_3(JOYCODE_2_RIGHT_DOWN,  CODE_OR, JOYCODE_4_DOWN)  },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER2, "P2 Right/Left",  SEQ_DEF_3(JOYCODE_2_RIGHT_LEFT,  CODE_OR, JOYCODE_4_LEFT)  },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER2, "P2 Right/Right", SEQ_DEF_3(JOYCODE_2_RIGHT_RIGHT, CODE_OR, JOYCODE_4_RIGHT) },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER2, "P2 Left/Up",     SEQ_DEF_3(JOYCODE_2_LEFT_UP,     CODE_OR, JOYCODE_3_UP    ) },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER2, "P2 Left/Down",   SEQ_DEF_3(JOYCODE_2_LEFT_DOWN,   CODE_OR, JOYCODE_3_DOWN  ) },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER2, "P2 Left/Left",   SEQ_DEF_3(JOYCODE_2_LEFT_LEFT,   CODE_OR, JOYCODE_3_LEFT  ) },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER2, "P2 Left/Right",  SEQ_DEF_3(JOYCODE_2_LEFT_RIGHT,  CODE_OR, JOYCODE_3_RIGHT ) },

  { IPT_JOYSTICK_UP         | IPF_PLAYER3, "P3 Up",          SEQ_DEF_7(KEYCODE_I,             CODE_OR, JOYCODE_3_UP,      CODE_OR, JOYCODE_3_LEFT_UP,       CODE_OR, JOYCODE_GUN_3_DPAD_UP )    },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER3, "P3 Down",        SEQ_DEF_7(KEYCODE_K,             CODE_OR, JOYCODE_3_DOWN,    CODE_OR, JOYCODE_3_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_3_DPAD_DOWN )  },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER3, "P3 Left",        SEQ_DEF_7(KEYCODE_J,             CODE_OR, JOYCODE_3_LEFT,    CODE_OR, JOYCODE_3_LEFT_LEFT,     CODE_OR, JOYCODE_GUN_3_DPAD_LEFT )  },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER3, "P3 Right",       SEQ_DEF_7(KEYCODE_L,             CODE_OR, JOYCODE_3_RIGHT,   CODE_OR, JOYCODE_3_LEFT_RIGHT,    CODE_OR, JOYCODE_GUN_3_DPAD_RIGHT ) },
  { IPT_BUTTON1             | IPF_PLAYER3, "P3 Button 1",    SEQ_DEF_7(KEYCODE_RCONTROL,      CODE_OR, JOYCODE_3_BUTTON1, CODE_OR, JOYCODE_MOUSE_3_BUTTON1, CODE_OR, JOYCODE_GUN_3_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER3, "P3 Button 2",    SEQ_DEF_7(KEYCODE_RSHIFT,        CODE_OR, JOYCODE_3_BUTTON2, CODE_OR, JOYCODE_MOUSE_3_BUTTON2, CODE_OR, JOYCODE_GUN_3_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER3, "P3 Button 3",    SEQ_DEF_7(KEYCODE_ENTER,         CODE_OR, JOYCODE_3_BUTTON3, CODE_OR, JOYCODE_MOUSE_3_BUTTON3, CODE_OR, JOYCODE_GUN_3_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER3, "P3 Button 4",    SEQ_DEF_5(JOYCODE_3_BUTTON4, CODE_OR, JOYCODE_MOUSE_3_BUTTON4, CODE_OR, JOYCODE_GUN_3_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER3, "P3 Button 5",    SEQ_DEF_1(JOYCODE_3_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER3, "P3 Button 6",    SEQ_DEF_1(JOYCODE_3_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER3, "P3 Button 7",    SEQ_DEF_1(JOYCODE_3_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER3, "P3 Button 8",    SEQ_DEF_1(JOYCODE_3_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER3, "P3 Button 9",    SEQ_DEF_1(JOYCODE_3_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER3, "P3 Button 10",   SEQ_DEF_1(JOYCODE_3_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER3, "P3 Right/Up",    SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER3, "P3 Right/Down",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER3, "P3 Right/Left",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER3, "P3 Right/Right", SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER3, "P3 Left/Up",     SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER3, "P3 Left/Down",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER3, "P3 Left/Left",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER3, "P3 Left/Right",  SEQ_DEF_0 },

  { IPT_JOYSTICK_UP         | IPF_PLAYER4, "P4 Up",          SEQ_DEF_5(JOYCODE_4_UP,          CODE_OR, JOYCODE_4_LEFT_UP,       CODE_OR, JOYCODE_GUN_4_DPAD_UP )    },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER4, "P4 Down",        SEQ_DEF_5(JOYCODE_4_DOWN,        CODE_OR, JOYCODE_4_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_4_DPAD_DOWN )  },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER4, "P4 Left",        SEQ_DEF_5(JOYCODE_4_LEFT,        CODE_OR, JOYCODE_4_LEFT_LEFT,     CODE_OR, JOYCODE_GUN_4_DPAD_LEFT )  },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER4, "P4 Right",       SEQ_DEF_5(JOYCODE_4_RIGHT,       CODE_OR, JOYCODE_4_LEFT_RIGHT,    CODE_OR, JOYCODE_GUN_4_DPAD_RIGHT ) },
  { IPT_BUTTON1             | IPF_PLAYER4, "P4 Button 1",    SEQ_DEF_5(JOYCODE_4_BUTTON1,     CODE_OR, JOYCODE_MOUSE_4_BUTTON1, CODE_OR, JOYCODE_GUN_4_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER4, "P4 Button 2",    SEQ_DEF_5(JOYCODE_4_BUTTON2,     CODE_OR, JOYCODE_MOUSE_4_BUTTON2, CODE_OR, JOYCODE_GUN_4_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER4, "P4 Button 3",    SEQ_DEF_5(JOYCODE_4_BUTTON3,     CODE_OR, JOYCODE_MOUSE_4_BUTTON3, CODE_OR, JOYCODE_GUN_4_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER4, "P4 Button 4",    SEQ_DEF_5(JOYCODE_4_BUTTON4,     CODE_OR, JOYCODE_MOUSE_4_BUTTON4, CODE_OR, JOYCODE_GUN_4_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER4, "P4 Button 5",    SEQ_DEF_1(JOYCODE_4_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER4, "P4 Button 6",    SEQ_DEF_1(JOYCODE_4_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER4, "P4 Button 7",    SEQ_DEF_1(JOYCODE_4_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER4, "P4 Button 8",    SEQ_DEF_1(JOYCODE_4_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER4, "P4 Button 9",    SEQ_DEF_1(JOYCODE_4_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER4, "P4 Button 10",   SEQ_DEF_1(JOYCODE_4_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER4, "P4 Right/Up",    SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER4, "P4 Right/Down",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER4, "P4 Right/Left",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER4, "P4 Right/Right", SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER4, "P4 Left/Up",     SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER4, "P4 Left/Down",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER4, "P4 Left/Left",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER4, "P4 Left/Right",  SEQ_DEF_0 },

  { IPT_JOYSTICK_UP         | IPF_PLAYER5, "P5 Up",          SEQ_DEF_5(JOYCODE_5_UP,          CODE_OR, JOYCODE_5_LEFT_UP,       CODE_OR, JOYCODE_GUN_5_DPAD_UP )       },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER5, "P5 Down",        SEQ_DEF_5(JOYCODE_5_DOWN,        CODE_OR, JOYCODE_5_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_5_DPAD_DOWN )     },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER5, "P5 Left",        SEQ_DEF_5(JOYCODE_5_LEFT,        CODE_OR, JOYCODE_5_LEFT_LEFT,     CODE_OR, JOYCODE_GUN_5_DPAD_LEFT )     },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER5, "P5 Right",       SEQ_DEF_5(JOYCODE_5_RIGHT,       CODE_OR, JOYCODE_5_LEFT_RIGHT,    CODE_OR, JOYCODE_GUN_5_DPAD_RIGHT )    },
  { IPT_BUTTON1             | IPF_PLAYER5, "P5 Button 1",    SEQ_DEF_5(JOYCODE_5_BUTTON1,     CODE_OR, JOYCODE_MOUSE_5_BUTTON1, CODE_OR, JOYCODE_GUN_5_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER5, "P5 Button 2",    SEQ_DEF_5(JOYCODE_5_BUTTON2,     CODE_OR, JOYCODE_MOUSE_5_BUTTON2, CODE_OR, JOYCODE_GUN_5_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER5, "P5 Button 3",    SEQ_DEF_5(JOYCODE_5_BUTTON3,     CODE_OR, JOYCODE_MOUSE_5_BUTTON3, CODE_OR, JOYCODE_GUN_5_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER5, "P5 Button 4",    SEQ_DEF_5(JOYCODE_5_BUTTON4,     CODE_OR, JOYCODE_MOUSE_5_BUTTON4, CODE_OR, JOYCODE_GUN_5_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER5, "P5 Button 5",    SEQ_DEF_1(JOYCODE_5_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER5, "P5 Button 6",    SEQ_DEF_1(JOYCODE_5_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER5, "P5 Button 7",    SEQ_DEF_1(JOYCODE_5_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER5, "P5 Button 8",    SEQ_DEF_1(JOYCODE_5_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER5, "P5 Button 9",    SEQ_DEF_1(JOYCODE_5_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER5, "P5 Button 10",   SEQ_DEF_1(JOYCODE_5_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER5, "P5 Right/Up",    SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER5, "P5 Right/Down",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER5, "P5 Right/Left",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER5, "P5 Right/Right", SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER5, "P5 Left/Up",     SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER5, "P5 Left/Down",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER5, "P5 Left/Left",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER5, "P5 Left/Right",  SEQ_DEF_0 },

  { IPT_JOYSTICK_UP         | IPF_PLAYER6, "P6 Up",          SEQ_DEF_5(JOYCODE_6_UP,          CODE_OR, JOYCODE_6_LEFT_UP,       CODE_OR, JOYCODE_GUN_6_DPAD_UP )      },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER6, "P6 Down",        SEQ_DEF_5(JOYCODE_6_DOWN,        CODE_OR, JOYCODE_6_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_6_DPAD_DOWN )    },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER6, "P6 Left",        SEQ_DEF_5(JOYCODE_6_LEFT,        CODE_OR, JOYCODE_6_LEFT_LEFT,     CODE_OR, JOYCODE_GUN_6_DPAD_LEFT )    },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER6, "P6 Right",       SEQ_DEF_5(JOYCODE_6_RIGHT,       CODE_OR, JOYCODE_6_LEFT_RIGHT,    CODE_OR, JOYCODE_GUN_6_DPAD_RIGHT )   },
  { IPT_BUTTON1             | IPF_PLAYER6, "P6 Button 1",    SEQ_DEF_5(JOYCODE_6_BUTTON1,     CODE_OR, JOYCODE_MOUSE_6_BUTTON1, CODE_OR, JOYCODE_GUN_6_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER6, "P6 Button 2",    SEQ_DEF_5(JOYCODE_6_BUTTON2,     CODE_OR, JOYCODE_MOUSE_6_BUTTON2, CODE_OR, JOYCODE_GUN_6_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER6, "P6 Button 3",    SEQ_DEF_5(JOYCODE_6_BUTTON3,     CODE_OR, JOYCODE_MOUSE_6_BUTTON3, CODE_OR, JOYCODE_GUN_6_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER6, "P6 Button 4",    SEQ_DEF_5(JOYCODE_6_BUTTON4,     CODE_OR, JOYCODE_MOUSE_6_BUTTON4, CODE_OR, JOYCODE_GUN_6_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER6, "P6 Button 5",    SEQ_DEF_1(JOYCODE_6_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER6, "P6 Button 6",    SEQ_DEF_1(JOYCODE_6_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER6, "P6 Button 7",    SEQ_DEF_1(JOYCODE_6_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER6, "P6 Button 8",    SEQ_DEF_1(JOYCODE_6_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER6, "P6 Button 9",    SEQ_DEF_1(JOYCODE_6_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER6, "P6 Button 10",   SEQ_DEF_1(JOYCODE_6_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER6, "P6 Right/Up",    SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER6, "P6 Right/Down",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER6, "P6 Right/Left",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER6, "P6 Right/Right", SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER6, "P6 Left/Up",     SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER6, "P6 Left/Down",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER6, "P6 Left/Left",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER6, "P6 Left/Right",  SEQ_DEF_0 },

  { IPT_JOYSTICK_UP         | IPF_PLAYER7, "P7 Up",          SEQ_DEF_5(JOYCODE_7_UP,          CODE_OR, JOYCODE_7_LEFT_UP,       CODE_OR, JOYCODE_GUN_7_DPAD_UP )       },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER7, "P7 Down",        SEQ_DEF_5(JOYCODE_7_DOWN,        CODE_OR, JOYCODE_7_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_7_DPAD_DOWN )     },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER7, "P7 Left",        SEQ_DEF_5(JOYCODE_7_LEFT,        CODE_OR, JOYCODE_7_LEFT_LEFT,     CODE_OR, JOYCODE_GUN_7_DPAD_LEFT )     },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER7, "P7 Right",       SEQ_DEF_5(JOYCODE_7_RIGHT,       CODE_OR, JOYCODE_7_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_7_DPAD_RIGHT )    },
  { IPT_BUTTON1             | IPF_PLAYER7, "P7 Button 1",    SEQ_DEF_5(JOYCODE_7_BUTTON1,     CODE_OR, JOYCODE_MOUSE_7_BUTTON1, CODE_OR, JOYCODE_GUN_7_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER7, "P7 Button 2",    SEQ_DEF_5(JOYCODE_7_BUTTON2,     CODE_OR, JOYCODE_MOUSE_7_BUTTON2, CODE_OR, JOYCODE_GUN_7_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER7, "P7 Button 3",    SEQ_DEF_5(JOYCODE_7_BUTTON3,     CODE_OR, JOYCODE_MOUSE_7_BUTTON3, CODE_OR, JOYCODE_GUN_7_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER7, "P7 Button 4",    SEQ_DEF_5(JOYCODE_7_BUTTON4,     CODE_OR, JOYCODE_MOUSE_7_BUTTON4, CODE_OR, JOYCODE_GUN_7_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER7, "P7 Button 5",    SEQ_DEF_1(JOYCODE_7_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER7, "P7 Button 6",    SEQ_DEF_1(JOYCODE_7_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER7, "P7 Button 7",    SEQ_DEF_1(JOYCODE_7_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER7, "P7 Button 8",    SEQ_DEF_1(JOYCODE_7_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER7, "P7 Button 9",    SEQ_DEF_1(JOYCODE_7_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER7, "P7 Button 10",   SEQ_DEF_1(JOYCODE_7_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER7, "P7 Right/Up",    SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER7, "P7 Right/Down",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER7, "P7 Right/Left",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER7, "P7 Right/Right", SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER7, "P7 Left/Up",     SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER7, "P7 Left/Down",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER7, "P7 Left/Left",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER7, "P7 Left/Right",  SEQ_DEF_0 },

  { IPT_JOYSTICK_UP         | IPF_PLAYER8, "P8 Up",          SEQ_DEF_5(JOYCODE_8_UP,          CODE_OR, JOYCODE_8_LEFT_UP,       CODE_OR, JOYCODE_GUN_8_DPAD_UP )      },
  { IPT_JOYSTICK_DOWN       | IPF_PLAYER8, "P8 Down",        SEQ_DEF_5(JOYCODE_8_DOWN,        CODE_OR, JOYCODE_8_LEFT_DOWN,     CODE_OR, JOYCODE_GUN_8_DPAD_DOWN )    },
  { IPT_JOYSTICK_LEFT       | IPF_PLAYER8, "P8 Left",        SEQ_DEF_5(JOYCODE_8_LEFT,        CODE_OR, JOYCODE_8_LEFT_LEFT,     CODE_OR, JOYCODE_GUN_8_DPAD_LEFT )    },
  { IPT_JOYSTICK_RIGHT      | IPF_PLAYER8, "P8 Right",       SEQ_DEF_5(JOYCODE_8_RIGHT,       CODE_OR, JOYCODE_8_LEFT_RIGHT,    CODE_OR, JOYCODE_GUN_8_DPAD_RIGHT )   },
  { IPT_BUTTON1             | IPF_PLAYER8, "P8 Button 1",    SEQ_DEF_5(JOYCODE_8_BUTTON1,     CODE_OR, JOYCODE_MOUSE_8_BUTTON1, CODE_OR, JOYCODE_GUN_8_BUTTON1 ) },
  { IPT_BUTTON2             | IPF_PLAYER8, "P8 Button 2",    SEQ_DEF_5(JOYCODE_8_BUTTON2,     CODE_OR, JOYCODE_MOUSE_8_BUTTON2, CODE_OR, JOYCODE_GUN_8_BUTTON2 ) },
  { IPT_BUTTON3             | IPF_PLAYER8, "P8 Button 3",    SEQ_DEF_5(JOYCODE_8_BUTTON3,     CODE_OR, JOYCODE_MOUSE_8_BUTTON3, CODE_OR, JOYCODE_GUN_8_BUTTON3 ) },
  { IPT_BUTTON4             | IPF_PLAYER8, "P8 Button 4",    SEQ_DEF_5(JOYCODE_8_BUTTON4,     CODE_OR, JOYCODE_MOUSE_8_BUTTON4, CODE_OR, JOYCODE_GUN_8_BUTTON4 ) },
  { IPT_BUTTON5             | IPF_PLAYER8, "P8 Button 5",    SEQ_DEF_1(JOYCODE_8_BUTTON5)  },
  { IPT_BUTTON6             | IPF_PLAYER8, "P8 Button 6",    SEQ_DEF_1(JOYCODE_8_BUTTON6)  },
  { IPT_BUTTON7             | IPF_PLAYER8, "P8 Button 7",    SEQ_DEF_1(JOYCODE_8_BUTTON7)  },
  { IPT_BUTTON8             | IPF_PLAYER8, "P8 Button 8",    SEQ_DEF_1(JOYCODE_8_BUTTON8)  },
  { IPT_BUTTON9             | IPF_PLAYER8, "P8 Button 9",    SEQ_DEF_1(JOYCODE_8_BUTTON9)  },
  { IPT_BUTTON10            | IPF_PLAYER8, "P8 Button 10",   SEQ_DEF_1(JOYCODE_8_BUTTON10) },
  { IPT_JOYSTICKRIGHT_UP    | IPF_PLAYER8, "P8 Right/Up",    SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_DOWN  | IPF_PLAYER8, "P8 Right/Down",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_LEFT  | IPF_PLAYER8, "P8 Right/Left",  SEQ_DEF_0 },
  { IPT_JOYSTICKRIGHT_RIGHT | IPF_PLAYER8, "P8 Right/Right", SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_UP     | IPF_PLAYER8, "P8 Left/Up",     SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_DOWN   | IPF_PLAYER8, "P8 Left/Down",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_LEFT   | IPF_PLAYER8, "P8 Left/Left",   SEQ_DEF_0 },
  { IPT_JOYSTICKLEFT_RIGHT  | IPF_PLAYER8, "P8 Left/Right",  SEQ_DEF_0 },

  { IPT_PEDAL                 | IPF_PLAYER1, "P1 Pedal 1",     SEQ_DEF_3(KEYCODE_LCONTROL, CODE_OR, JOYCODE_1_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER1, "P1 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER2, "P2 Pedal 1",     SEQ_DEF_3(KEYCODE_A, CODE_OR, JOYCODE_2_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER2, "P2 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER3, "P3 Pedal 1",     SEQ_DEF_3(KEYCODE_RCONTROL, CODE_OR, JOYCODE_3_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER3, "P3 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER4, "P4 Pedal 1",     SEQ_DEF_1(JOYCODE_4_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER4, "P4 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER5, "P5 Pedal 1",     SEQ_DEF_1(JOYCODE_5_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER5, "P5 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER6, "P6 Pedal 1",     SEQ_DEF_1(JOYCODE_6_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER6, "P6 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER7, "P7 Pedal 1",     SEQ_DEF_1(JOYCODE_7_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER7, "P7 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL                 | IPF_PLAYER8, "P8 Pedal 1",     SEQ_DEF_1(JOYCODE_8_BUTTON6) },
  { (IPT_PEDAL+IPT_EXTENSION) | IPF_PLAYER8, "P8 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },

  { IPT_PEDAL2                 | IPF_PLAYER1, "P1 Pedal 2",     SEQ_DEF_1(JOYCODE_1_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER1, "P1 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER2, "P2 Pedal 2",     SEQ_DEF_1(JOYCODE_2_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER2, "P2 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER3, "P3 Pedal 2",     SEQ_DEF_1(JOYCODE_3_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER3, "P3 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER4, "P4 Pedal 2",     SEQ_DEF_1(JOYCODE_4_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER4, "P4 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER5, "P5 Pedal 2",     SEQ_DEF_1(JOYCODE_5_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER5, "P5 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER6, "P6 Pedal 2",     SEQ_DEF_1(JOYCODE_6_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER6, "P6 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER7, "P7 Pedal 2",     SEQ_DEF_1(JOYCODE_7_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER7, "P7 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },
  { IPT_PEDAL2                 | IPF_PLAYER8, "P8 Pedal 2",     SEQ_DEF_1(JOYCODE_8_BUTTON5) },
  { (IPT_PEDAL2+IPT_EXTENSION) | IPF_PLAYER8, "P8 Auto Release <Y/N>", SEQ_DEF_1(KEYCODE_Y) },

  { IPT_PADDLE | IPF_PLAYER1,  "Paddle",        SEQ_DEF_5(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT, CODE_OR, JOYCODE_1_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER1)+IPT_EXTENSION,             "Paddle",        SEQ_DEF_5(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT, CODE_OR, JOYCODE_1_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER2,  "Paddle 2",      SEQ_DEF_5(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT, CODE_OR, JOYCODE_2_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER2)+IPT_EXTENSION,             "Paddle 2",      SEQ_DEF_5(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT, CODE_OR, JOYCODE_2_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER3,  "Paddle 3",      SEQ_DEF_5(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT, CODE_OR, JOYCODE_3_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER3)+IPT_EXTENSION,             "Paddle 3",      SEQ_DEF_5(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT, CODE_OR, JOYCODE_3_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER4,  "Paddle 4",      SEQ_DEF_3(JOYCODE_4_LEFT, CODE_OR, JOYCODE_4_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER4)+IPT_EXTENSION,             "Paddle 4",      SEQ_DEF_3(JOYCODE_4_RIGHT, CODE_OR, JOYCODE_4_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER5,  "Paddle 5",      SEQ_DEF_3(JOYCODE_5_LEFT, CODE_OR, JOYCODE_5_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER5)+IPT_EXTENSION,             "Paddle 5",      SEQ_DEF_3(JOYCODE_5_RIGHT, CODE_OR, JOYCODE_5_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER6,  "Paddle 6",      SEQ_DEF_3(JOYCODE_6_LEFT, CODE_OR, JOYCODE_6_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER6)+IPT_EXTENSION,             "Paddle 6",      SEQ_DEF_3(JOYCODE_6_RIGHT, CODE_OR, JOYCODE_6_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER7,  "Paddle 7",      SEQ_DEF_3(JOYCODE_7_LEFT, CODE_OR, JOYCODE_7_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER7)+IPT_EXTENSION,             "Paddle 7",      SEQ_DEF_3(JOYCODE_7_RIGHT, CODE_OR, JOYCODE_7_LEFT_RIGHT) },
  { IPT_PADDLE | IPF_PLAYER8,  "Paddle 8",      SEQ_DEF_3(JOYCODE_8_LEFT, CODE_OR, JOYCODE_8_LEFT_LEFT) },
  { (IPT_PADDLE | IPF_PLAYER8)+IPT_EXTENSION,             "Paddle 8",      SEQ_DEF_3(JOYCODE_8_RIGHT, CODE_OR, JOYCODE_8_LEFT_RIGHT) },

  { IPT_PADDLE_V | IPF_PLAYER1,  "Paddle V",          SEQ_DEF_5(KEYCODE_UP, CODE_OR, JOYCODE_1_UP, CODE_OR, JOYCODE_1_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER1)+IPT_EXTENSION,             "Paddle V",        SEQ_DEF_5(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN, CODE_OR, JOYCODE_1_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER2,  "Paddle V 2",        SEQ_DEF_5(KEYCODE_R, CODE_OR, JOYCODE_2_UP, CODE_OR, JOYCODE_2_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER2)+IPT_EXTENSION,             "Paddle V 2",      SEQ_DEF_5(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN, CODE_OR, JOYCODE_2_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER3,  "Paddle V 3",        SEQ_DEF_5(KEYCODE_I, CODE_OR, JOYCODE_3_UP, CODE_OR, JOYCODE_3_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER3)+IPT_EXTENSION,             "Paddle V 3",      SEQ_DEF_5(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN, CODE_OR, JOYCODE_3_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER4,  "Paddle V 4",        SEQ_DEF_3(JOYCODE_4_UP, CODE_OR, JOYCODE_4_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER4)+IPT_EXTENSION,             "Paddle V 4",      SEQ_DEF_3(JOYCODE_4_DOWN, CODE_OR, JOYCODE_4_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER5,  "Paddle V 5",        SEQ_DEF_3(JOYCODE_5_UP, CODE_OR, JOYCODE_5_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER5)+IPT_EXTENSION,             "Paddle V 5",      SEQ_DEF_3(JOYCODE_5_DOWN, CODE_OR, JOYCODE_5_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER6,  "Paddle V 6",        SEQ_DEF_3(JOYCODE_6_UP, CODE_OR, JOYCODE_6_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER6)+IPT_EXTENSION,             "Paddle V 6",      SEQ_DEF_3(JOYCODE_6_DOWN, CODE_OR, JOYCODE_6_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER7,  "Paddle V 7",        SEQ_DEF_3(JOYCODE_7_UP, CODE_OR, JOYCODE_7_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER7)+IPT_EXTENSION,             "Paddle V 7",      SEQ_DEF_3(JOYCODE_7_DOWN, CODE_OR, JOYCODE_7_LEFT_DOWN) },
  { IPT_PADDLE_V | IPF_PLAYER8,  "Paddle V 8",        SEQ_DEF_3(JOYCODE_8_UP, CODE_OR, JOYCODE_8_LEFT_UP) },
  { (IPT_PADDLE_V | IPF_PLAYER8)+IPT_EXTENSION,             "Paddle V 8",      SEQ_DEF_3(JOYCODE_8_DOWN, CODE_OR, JOYCODE_8_LEFT_DOWN) },

  { IPT_DIAL | IPF_PLAYER1,    "Dial",          SEQ_DEF_5(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT, CODE_OR, JOYCODE_1_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER1)+IPT_EXTENSION,               "Dial",        SEQ_DEF_5(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT, CODE_OR, JOYCODE_1_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER2,    "Dial 2",        SEQ_DEF_5(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT, CODE_OR, JOYCODE_2_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER2)+IPT_EXTENSION,               "Dial 2",      SEQ_DEF_5(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT, CODE_OR, JOYCODE_2_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER3,    "Dial 3",        SEQ_DEF_5(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT, CODE_OR, JOYCODE_3_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER3)+IPT_EXTENSION,               "Dial 3",      SEQ_DEF_5(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT, CODE_OR, JOYCODE_3_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER4,    "Dial 4",        SEQ_DEF_3(JOYCODE_4_LEFT, CODE_OR, JOYCODE_4_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER4)+IPT_EXTENSION,               "Dial 4",      SEQ_DEF_3(JOYCODE_4_RIGHT, CODE_OR, JOYCODE_4_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER5,    "Dial 5",        SEQ_DEF_3(JOYCODE_5_LEFT, CODE_OR, JOYCODE_5_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER5)+IPT_EXTENSION,               "Dial 5",      SEQ_DEF_3(JOYCODE_5_RIGHT, CODE_OR, JOYCODE_5_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER6,    "Dial 6",        SEQ_DEF_3(JOYCODE_6_LEFT, CODE_OR, JOYCODE_6_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER6)+IPT_EXTENSION,               "Dial 6",      SEQ_DEF_3(JOYCODE_6_RIGHT, CODE_OR, JOYCODE_6_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER7,    "Dial 7",        SEQ_DEF_3(JOYCODE_7_LEFT, CODE_OR, JOYCODE_7_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER7)+IPT_EXTENSION,               "Dial 7",      SEQ_DEF_3(JOYCODE_7_RIGHT, CODE_OR, JOYCODE_7_LEFT_RIGHT) },
  { IPT_DIAL | IPF_PLAYER8,    "Dial 8",        SEQ_DEF_3(JOYCODE_8_LEFT, CODE_OR, JOYCODE_8_LEFT_LEFT) },
  { (IPT_DIAL | IPF_PLAYER8)+IPT_EXTENSION,               "Dial 8",      SEQ_DEF_3(JOYCODE_8_RIGHT, CODE_OR, JOYCODE_8_LEFT_RIGHT) },

  { IPT_DIAL_V | IPF_PLAYER1,  "Dial V",          SEQ_DEF_5(KEYCODE_UP, CODE_OR, JOYCODE_1_UP, CODE_OR, JOYCODE_1_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER1)+IPT_EXTENSION,             "Dial V",        SEQ_DEF_5(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN, CODE_OR, JOYCODE_1_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER2,  "Dial V 2",        SEQ_DEF_5(KEYCODE_R, CODE_OR, JOYCODE_2_UP, CODE_OR, JOYCODE_2_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER2)+IPT_EXTENSION,             "Dial V 2",      SEQ_DEF_5(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN, CODE_OR, JOYCODE_2_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER3,  "Dial V 3",        SEQ_DEF_5(KEYCODE_I, CODE_OR, JOYCODE_3_UP, CODE_OR, JOYCODE_3_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER3)+IPT_EXTENSION,             "Dial V 3",      SEQ_DEF_5(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN, CODE_OR, JOYCODE_3_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER4,  "Dial V 4",        SEQ_DEF_3(JOYCODE_4_UP, CODE_OR, JOYCODE_4_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER4)+IPT_EXTENSION,             "Dial V 4",      SEQ_DEF_3(JOYCODE_4_DOWN, CODE_OR, JOYCODE_4_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER5,  "Dial V 5",        SEQ_DEF_3(JOYCODE_5_UP, CODE_OR, JOYCODE_5_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER5)+IPT_EXTENSION,             "Dial V 5",      SEQ_DEF_3(JOYCODE_5_DOWN, CODE_OR, JOYCODE_5_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER6,  "Dial V 6",        SEQ_DEF_3(JOYCODE_6_UP, CODE_OR, JOYCODE_6_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER6)+IPT_EXTENSION,             "Dial V 6",      SEQ_DEF_3(JOYCODE_6_DOWN, CODE_OR, JOYCODE_6_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER7,  "Dial V 7",        SEQ_DEF_3(JOYCODE_7_UP, CODE_OR, JOYCODE_7_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER7)+IPT_EXTENSION,             "Dial V 7",      SEQ_DEF_3(JOYCODE_7_DOWN, CODE_OR, JOYCODE_7_LEFT_DOWN) },
  { IPT_DIAL_V | IPF_PLAYER8,  "Dial V 8",        SEQ_DEF_3(JOYCODE_8_UP, CODE_OR, JOYCODE_8_LEFT_UP) },
  { (IPT_DIAL_V | IPF_PLAYER8)+IPT_EXTENSION,             "Dial V 8",      SEQ_DEF_3(JOYCODE_8_DOWN, CODE_OR, JOYCODE_8_LEFT_DOWN) },

  { IPT_TRACKBALL_X | IPF_PLAYER1, "Track X",   SEQ_DEF_5(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT, CODE_OR, JOYCODE_1_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER1)+IPT_EXTENSION,                 "Track X",   SEQ_DEF_5(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT, CODE_OR, JOYCODE_1_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER2, "Track X 2", SEQ_DEF_5(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT, CODE_OR, JOYCODE_2_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER2)+IPT_EXTENSION,                 "Track X 2", SEQ_DEF_5(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT, CODE_OR, JOYCODE_2_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER3, "Track X 3", SEQ_DEF_5(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT, CODE_OR, JOYCODE_3_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER3)+IPT_EXTENSION,                 "Track X 3", SEQ_DEF_5(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT, CODE_OR, JOYCODE_3_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER4, "Track X 4", SEQ_DEF_3(JOYCODE_4_LEFT, CODE_OR, JOYCODE_4_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER4)+IPT_EXTENSION,                 "Track X 4", SEQ_DEF_3(JOYCODE_4_RIGHT, CODE_OR, JOYCODE_4_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER5, "Track X 5", SEQ_DEF_3(JOYCODE_5_LEFT, CODE_OR, JOYCODE_5_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER5)+IPT_EXTENSION,                 "Track X 5", SEQ_DEF_3(JOYCODE_5_RIGHT, CODE_OR, JOYCODE_5_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER6, "Track X 6", SEQ_DEF_3(JOYCODE_6_LEFT, CODE_OR, JOYCODE_6_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER6)+IPT_EXTENSION,                 "Track X 6", SEQ_DEF_3(JOYCODE_6_RIGHT, CODE_OR, JOYCODE_6_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER7, "Track X 7", SEQ_DEF_3(JOYCODE_7_LEFT, CODE_OR, JOYCODE_7_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER7)+IPT_EXTENSION,                 "Track X 7", SEQ_DEF_3(JOYCODE_7_RIGHT, CODE_OR, JOYCODE_7_LEFT_RIGHT) },
  { IPT_TRACKBALL_X | IPF_PLAYER8, "Track X 8", SEQ_DEF_3(JOYCODE_8_LEFT, CODE_OR, JOYCODE_8_LEFT_LEFT) },
  { (IPT_TRACKBALL_X | IPF_PLAYER8)+IPT_EXTENSION,                 "Track X 8", SEQ_DEF_3(JOYCODE_8_RIGHT, CODE_OR, JOYCODE_8_LEFT_RIGHT) },

  { IPT_TRACKBALL_Y | IPF_PLAYER1, "Track Y",   SEQ_DEF_5(KEYCODE_UP, CODE_OR, JOYCODE_1_UP, CODE_OR, JOYCODE_1_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER1)+IPT_EXTENSION,                 "Track Y",   SEQ_DEF_5(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN, CODE_OR, JOYCODE_1_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER2, "Track Y 2", SEQ_DEF_5(KEYCODE_R, CODE_OR, JOYCODE_2_UP, CODE_OR, JOYCODE_2_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER2)+IPT_EXTENSION,                 "Track Y 2", SEQ_DEF_5(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN, CODE_OR, JOYCODE_2_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER3, "Track Y 3", SEQ_DEF_5(KEYCODE_I, CODE_OR, JOYCODE_3_UP, CODE_OR, JOYCODE_3_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER3)+IPT_EXTENSION,                 "Track Y 3", SEQ_DEF_5(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN, CODE_OR, JOYCODE_3_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER4, "Track Y 4", SEQ_DEF_3(JOYCODE_4_UP, CODE_OR, JOYCODE_4_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER4)+IPT_EXTENSION,                 "Track Y 4", SEQ_DEF_3(JOYCODE_4_DOWN, CODE_OR, JOYCODE_4_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER5, "Track Y 5", SEQ_DEF_3(JOYCODE_5_UP, CODE_OR, JOYCODE_5_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER5)+IPT_EXTENSION,                 "Track Y 5", SEQ_DEF_3(JOYCODE_5_DOWN, CODE_OR, JOYCODE_5_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER6, "Track Y 6", SEQ_DEF_3(JOYCODE_6_UP, CODE_OR, JOYCODE_6_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER6)+IPT_EXTENSION,                 "Track Y 6", SEQ_DEF_3(JOYCODE_6_DOWN, CODE_OR, JOYCODE_6_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER7, "Track Y 7", SEQ_DEF_3(JOYCODE_7_UP, CODE_OR, JOYCODE_7_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER7)+IPT_EXTENSION,                 "Track Y 7", SEQ_DEF_3(JOYCODE_7_DOWN, CODE_OR, JOYCODE_7_LEFT_DOWN) },
  { IPT_TRACKBALL_Y | IPF_PLAYER8, "Track Y 8", SEQ_DEF_3(JOYCODE_8_UP, CODE_OR, JOYCODE_8_LEFT_UP) },
  { (IPT_TRACKBALL_Y | IPF_PLAYER8)+IPT_EXTENSION,                 "Track Y 8", SEQ_DEF_3(JOYCODE_8_DOWN, CODE_OR, JOYCODE_8_LEFT_DOWN) },

  { IPT_AD_STICK_X | IPF_PLAYER1, "AD Stick X",   SEQ_DEF_5(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT, CODE_OR, JOYCODE_1_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER1)+IPT_EXTENSION,                "AD Stick X",   SEQ_DEF_5(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT, CODE_OR, JOYCODE_1_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER2, "AD Stick X 2", SEQ_DEF_5(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT, CODE_OR, JOYCODE_2_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER2)+IPT_EXTENSION,                "AD Stick X 2", SEQ_DEF_5(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT, CODE_OR, JOYCODE_2_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER3, "AD Stick X 3", SEQ_DEF_5(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT, CODE_OR, JOYCODE_3_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER3)+IPT_EXTENSION,                "AD Stick X 3", SEQ_DEF_5(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT, CODE_OR, JOYCODE_3_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER4, "AD Stick X 4", SEQ_DEF_3(JOYCODE_4_LEFT, CODE_OR, JOYCODE_4_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER4)+IPT_EXTENSION,                "AD Stick X 4", SEQ_DEF_3(JOYCODE_4_RIGHT, CODE_OR, JOYCODE_4_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER5, "AD Stick X 5", SEQ_DEF_3(JOYCODE_5_LEFT, CODE_OR, JOYCODE_5_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER5)+IPT_EXTENSION,                "AD Stick X 5", SEQ_DEF_3(JOYCODE_5_RIGHT, CODE_OR, JOYCODE_5_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER6, "AD Stick X 6", SEQ_DEF_3(JOYCODE_6_LEFT, CODE_OR, JOYCODE_6_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER6)+IPT_EXTENSION,                "AD Stick X 6", SEQ_DEF_3(JOYCODE_6_RIGHT, CODE_OR, JOYCODE_6_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER7, "AD Stick X 7", SEQ_DEF_3(JOYCODE_7_LEFT, CODE_OR, JOYCODE_7_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER7)+IPT_EXTENSION,                "AD Stick X 7", SEQ_DEF_3(JOYCODE_7_RIGHT, CODE_OR, JOYCODE_7_LEFT_RIGHT) },
  { IPT_AD_STICK_X | IPF_PLAYER8, "AD Stick X 8", SEQ_DEF_3(JOYCODE_8_LEFT, CODE_OR, JOYCODE_8_LEFT_LEFT) },
  { (IPT_AD_STICK_X | IPF_PLAYER8)+IPT_EXTENSION,                "AD Stick X 8", SEQ_DEF_3(JOYCODE_8_RIGHT, CODE_OR, JOYCODE_8_LEFT_RIGHT) },

  { IPT_AD_STICK_Y | IPF_PLAYER1, "AD Stick Y",   SEQ_DEF_5(KEYCODE_UP, CODE_OR, JOYCODE_1_UP, CODE_OR, JOYCODE_1_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER1)+IPT_EXTENSION,                "AD Stick Y",   SEQ_DEF_5(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN, CODE_OR, JOYCODE_1_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER2, "AD Stick Y 2", SEQ_DEF_5(KEYCODE_R, CODE_OR, JOYCODE_2_UP, CODE_OR, JOYCODE_2_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER2)+IPT_EXTENSION,                "AD Stick Y 2", SEQ_DEF_5(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN, CODE_OR, JOYCODE_2_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER3, "AD Stick Y 3", SEQ_DEF_5(KEYCODE_I, CODE_OR, JOYCODE_3_UP, CODE_OR, JOYCODE_3_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER3)+IPT_EXTENSION,                "AD Stick Y 3", SEQ_DEF_5(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN, CODE_OR, JOYCODE_3_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER4, "AD Stick Y 4", SEQ_DEF_3(JOYCODE_4_UP, CODE_OR, JOYCODE_4_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER4)+IPT_EXTENSION,                "AD Stick Y 4", SEQ_DEF_3(JOYCODE_4_DOWN, CODE_OR, JOYCODE_4_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER5, "AD Stick Y 5", SEQ_DEF_3(JOYCODE_5_UP, CODE_OR, JOYCODE_5_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER5)+IPT_EXTENSION,                "AD Stick Y 5", SEQ_DEF_3(JOYCODE_5_DOWN, CODE_OR, JOYCODE_5_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER6, "AD Stick Y 6", SEQ_DEF_3(JOYCODE_6_UP, CODE_OR, JOYCODE_6_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER6)+IPT_EXTENSION,                "AD Stick Y 6", SEQ_DEF_3(JOYCODE_6_DOWN, CODE_OR, JOYCODE_6_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER7, "AD Stick Y 7", SEQ_DEF_3(JOYCODE_7_UP, CODE_OR, JOYCODE_7_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER7)+IPT_EXTENSION,                "AD Stick Y 7", SEQ_DEF_3(JOYCODE_7_DOWN, CODE_OR, JOYCODE_7_LEFT_DOWN) },
  { IPT_AD_STICK_Y | IPF_PLAYER8, "AD Stick Y 8", SEQ_DEF_3(JOYCODE_8_UP, CODE_OR, JOYCODE_8_LEFT_UP) },
  { (IPT_AD_STICK_Y | IPF_PLAYER8)+IPT_EXTENSION,                "AD Stick Y 8", SEQ_DEF_3(JOYCODE_8_DOWN, CODE_OR, JOYCODE_8_LEFT_DOWN) },

  { IPT_AD_STICK_Z | IPF_PLAYER1, "AD Stick Z",   SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER1)+IPT_EXTENSION,                "AD Stick Z",   SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER2, "AD Stick Z 2", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER2)+IPT_EXTENSION,                "AD Stick Z 2", SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER3, "AD Stick Z 3", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER3)+IPT_EXTENSION,                "AD Stick Z 3", SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER4, "AD Stick Z 4", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER4)+IPT_EXTENSION,                "AD Stick Z 4", SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER5, "AD Stick Z 5", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER5)+IPT_EXTENSION,                "AD Stick Z 5", SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER6, "AD Stick Z 6", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER6)+IPT_EXTENSION,                "AD Stick Z 6", SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER7, "AD Stick Z 7", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER7)+IPT_EXTENSION,                "AD Stick Z 7", SEQ_DEF_0 },
  { IPT_AD_STICK_Z | IPF_PLAYER8, "AD Stick Z 8", SEQ_DEF_0 },
  { (IPT_AD_STICK_Z | IPF_PLAYER8)+IPT_EXTENSION,                "AD Stick Z 8", SEQ_DEF_0 },

  { IPT_LIGHTGUN_X | IPF_PLAYER1, "Lightgun X",   SEQ_DEF_5(KEYCODE_LEFT, CODE_OR, JOYCODE_1_LEFT, CODE_OR, JOYCODE_1_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER1)+IPT_EXTENSION,                "Lightgun X",   SEQ_DEF_5(KEYCODE_RIGHT, CODE_OR, JOYCODE_1_RIGHT, CODE_OR, JOYCODE_1_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER2, "Lightgun X 2", SEQ_DEF_5(KEYCODE_D, CODE_OR, JOYCODE_2_LEFT, CODE_OR, JOYCODE_2_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER2)+IPT_EXTENSION,                "Lightgun X 2", SEQ_DEF_5(KEYCODE_G, CODE_OR, JOYCODE_2_RIGHT, CODE_OR, JOYCODE_2_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER3, "Lightgun X 3", SEQ_DEF_5(KEYCODE_J, CODE_OR, JOYCODE_3_LEFT, CODE_OR, JOYCODE_3_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER3)+IPT_EXTENSION,                "Lightgun X 3", SEQ_DEF_5(KEYCODE_L, CODE_OR, JOYCODE_3_RIGHT, CODE_OR, JOYCODE_3_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER4, "Lightgun X 4", SEQ_DEF_3(JOYCODE_4_LEFT, CODE_OR, JOYCODE_4_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER4)+IPT_EXTENSION,                "Lightgun X 4", SEQ_DEF_3(JOYCODE_4_RIGHT, CODE_OR, JOYCODE_4_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER5, "Lightgun X 5", SEQ_DEF_3(JOYCODE_5_LEFT, CODE_OR, JOYCODE_5_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER5)+IPT_EXTENSION,                "Lightgun X 5", SEQ_DEF_3(JOYCODE_5_RIGHT, CODE_OR, JOYCODE_5_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER6, "Lightgun X 6", SEQ_DEF_3(JOYCODE_6_LEFT, CODE_OR, JOYCODE_6_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER6)+IPT_EXTENSION,                "Lightgun X 6", SEQ_DEF_3(JOYCODE_6_RIGHT, CODE_OR, JOYCODE_6_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER7, "Lightgun X 7", SEQ_DEF_3(JOYCODE_7_LEFT, CODE_OR, JOYCODE_7_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER7)+IPT_EXTENSION,                "Lightgun X 7", SEQ_DEF_3(JOYCODE_7_RIGHT, CODE_OR, JOYCODE_7_LEFT_RIGHT) },
  { IPT_LIGHTGUN_X | IPF_PLAYER8, "Lightgun X 8", SEQ_DEF_3(JOYCODE_8_LEFT, CODE_OR, JOYCODE_8_LEFT_LEFT) },
  { (IPT_LIGHTGUN_X | IPF_PLAYER8)+IPT_EXTENSION,                "Lightgun X 8", SEQ_DEF_3(JOYCODE_8_RIGHT, CODE_OR, JOYCODE_8_LEFT_RIGHT) },

  { IPT_LIGHTGUN_Y | IPF_PLAYER1, "Lightgun Y",   SEQ_DEF_5(KEYCODE_UP, CODE_OR, JOYCODE_1_UP, CODE_OR, JOYCODE_1_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER1)+IPT_EXTENSION,                "Lightgun Y",   SEQ_DEF_5(KEYCODE_DOWN, CODE_OR, JOYCODE_1_DOWN, CODE_OR, JOYCODE_1_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER2, "Lightgun Y 2", SEQ_DEF_5(KEYCODE_R, CODE_OR, JOYCODE_2_UP, CODE_OR, JOYCODE_2_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER2)+IPT_EXTENSION,                "Lightgun Y 2", SEQ_DEF_5(KEYCODE_F, CODE_OR, JOYCODE_2_DOWN, CODE_OR, JOYCODE_2_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER3, "Lightgun Y 3", SEQ_DEF_5(KEYCODE_I, CODE_OR, JOYCODE_3_UP, CODE_OR, JOYCODE_3_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER3)+IPT_EXTENSION,                "Lightgun Y 3", SEQ_DEF_5(KEYCODE_K, CODE_OR, JOYCODE_3_DOWN, CODE_OR, JOYCODE_3_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER4, "Lightgun Y 4", SEQ_DEF_3(JOYCODE_4_UP, CODE_OR, JOYCODE_4_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER4)+IPT_EXTENSION,                "Lightgun Y 4", SEQ_DEF_3(JOYCODE_4_DOWN, CODE_OR, JOYCODE_4_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER5, "Lightgun Y 5", SEQ_DEF_3(JOYCODE_5_UP, CODE_OR, JOYCODE_5_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER5)+IPT_EXTENSION,                "Lightgun Y 5", SEQ_DEF_3(JOYCODE_5_DOWN, CODE_OR, JOYCODE_5_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER6, "Lightgun Y 6", SEQ_DEF_3(JOYCODE_6_UP, CODE_OR, JOYCODE_6_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER6)+IPT_EXTENSION,                "Lightgun Y 6", SEQ_DEF_3(JOYCODE_6_DOWN, CODE_OR, JOYCODE_6_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER7, "Lightgun Y 7", SEQ_DEF_3(JOYCODE_7_UP, CODE_OR, JOYCODE_7_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER7)+IPT_EXTENSION,                "Lightgun Y 7", SEQ_DEF_3(JOYCODE_7_DOWN, CODE_OR, JOYCODE_7_LEFT_DOWN) },
  { IPT_LIGHTGUN_Y | IPF_PLAYER8, "Lightgun Y 8", SEQ_DEF_3(JOYCODE_8_UP, CODE_OR, JOYCODE_8_LEFT_UP) },
  { (IPT_LIGHTGUN_Y | IPF_PLAYER8)+IPT_EXTENSION,                "Lightgun Y 8", SEQ_DEF_3(JOYCODE_8_DOWN, CODE_OR, JOYCODE_8_LEFT_DOWN) },

  { IPT_UNKNOWN,             "UNKNOWN",         SEQ_DEF_0 },
  { IPT_OSD_DESCRIPTION,     "",                SEQ_DEF_0 },
  { IPT_OSD_DESCRIPTION,     "",                SEQ_DEF_0 },
  { IPT_OSD_DESCRIPTION,     "",                SEQ_DEF_0 },
  { IPT_OSD_DESCRIPTION,     "",                SEQ_DEF_0 },
  { IPT_END,                 0,                 SEQ_DEF_0 }  /* returned when there is no match */
};

struct ipd inputport_defaults_backup[sizeof(inputport_defaults)/sizeof(struct ipd)];

struct ik *osd_input_keywords = NULL;

struct ik input_keywords[] =
{
	{ "KEYCODE_A",		   		IKT_STD,		KEYCODE_A },
	{ "KEYCODE_B",		   		IKT_STD,		KEYCODE_B },
	{ "KEYCODE_C",		   		IKT_STD,		KEYCODE_C },
	{ "KEYCODE_D",		   		IKT_STD,		KEYCODE_D },
	{ "KEYCODE_E",		   		IKT_STD,		KEYCODE_E },
	{ "KEYCODE_F",		   		IKT_STD,		KEYCODE_F },
	{ "KEYCODE_G",		   		IKT_STD,		KEYCODE_G },
	{ "KEYCODE_H",		   		IKT_STD,		KEYCODE_H },
	{ "KEYCODE_I",		   		IKT_STD,		KEYCODE_I },
	{ "KEYCODE_J",		   		IKT_STD,		KEYCODE_J },
	{ "KEYCODE_K",		   		IKT_STD,		KEYCODE_K },
	{ "KEYCODE_L",		   		IKT_STD,		KEYCODE_L },
	{ "KEYCODE_M",		   		IKT_STD,		KEYCODE_M },
	{ "KEYCODE_N",		   		IKT_STD,		KEYCODE_N },
	{ "KEYCODE_O",		   		IKT_STD,		KEYCODE_O },
	{ "KEYCODE_P",		   		IKT_STD,		KEYCODE_P },
	{ "KEYCODE_Q",		   		IKT_STD,		KEYCODE_Q },
	{ "KEYCODE_R",		   		IKT_STD,		KEYCODE_R },
	{ "KEYCODE_S",		   		IKT_STD,		KEYCODE_S },
	{ "KEYCODE_T",		   		IKT_STD,		KEYCODE_T },
	{ "KEYCODE_U",		   		IKT_STD,		KEYCODE_U },
	{ "KEYCODE_V",		   		IKT_STD,		KEYCODE_V },
	{ "KEYCODE_W",		   		IKT_STD,		KEYCODE_W },
	{ "KEYCODE_X",		   		IKT_STD,		KEYCODE_X },
	{ "KEYCODE_Y",		   		IKT_STD,		KEYCODE_Y },
	{ "KEYCODE_Z",		   		IKT_STD,		KEYCODE_Z },
	{ "KEYCODE_0",		   		IKT_STD,		KEYCODE_0 },
	{ "KEYCODE_1",		   		IKT_STD,		KEYCODE_1 },
	{ "KEYCODE_2",		   		IKT_STD,		KEYCODE_2 },
	{ "KEYCODE_3",		   		IKT_STD,		KEYCODE_3 },
	{ "KEYCODE_4",		   		IKT_STD,		KEYCODE_4 },
	{ "KEYCODE_5",		   		IKT_STD,		KEYCODE_5 },
	{ "KEYCODE_6",		   		IKT_STD,		KEYCODE_6 },
	{ "KEYCODE_7",		   		IKT_STD,		KEYCODE_7 },
	{ "KEYCODE_8",		   		IKT_STD,		KEYCODE_8 },
	{ "KEYCODE_9",		   		IKT_STD,		KEYCODE_9 },

	{ "KEYCODE_0_PAD",	   		IKT_STD,		KEYCODE_0_PAD },
	{ "KEYCODE_1_PAD",	   		IKT_STD,		KEYCODE_1_PAD },
	{ "KEYCODE_2_PAD",	   		IKT_STD,		KEYCODE_2_PAD },
	{ "KEYCODE_3_PAD",	   		IKT_STD,		KEYCODE_3_PAD },
	{ "KEYCODE_4_PAD",	   		IKT_STD,		KEYCODE_4_PAD },
	{ "KEYCODE_5_PAD",	   		IKT_STD,		KEYCODE_5_PAD },
	{ "KEYCODE_6_PAD",	   		IKT_STD,		KEYCODE_6_PAD },
	{ "KEYCODE_7_PAD",	   		IKT_STD,		KEYCODE_7_PAD },
	{ "KEYCODE_8_PAD",	      	IKT_STD,		KEYCODE_8_PAD },
	{ "KEYCODE_9_PAD",	      	IKT_STD,		KEYCODE_9_PAD },

	{ "KEYCODE_F1",		   		IKT_STD,		KEYCODE_F1 },
	{ "KEYCODE_F2",			  	IKT_STD,		KEYCODE_F2 },
	{ "KEYCODE_F3",			  	IKT_STD,		KEYCODE_F3 },
	{ "KEYCODE_F4",			  	IKT_STD,		KEYCODE_F4 },
	{ "KEYCODE_F5",			  	IKT_STD,		KEYCODE_F5 },
	{ "KEYCODE_F6",			  	IKT_STD,		KEYCODE_F6 },
	{ "KEYCODE_F7",			  	IKT_STD,		KEYCODE_F7 },
	{ "KEYCODE_F8",			  	IKT_STD,		KEYCODE_F8 },
	{ "KEYCODE_F9",			  	IKT_STD,		KEYCODE_F9 },
	{ "KEYCODE_F10",		  	IKT_STD,		KEYCODE_F10 },
	{ "KEYCODE_F11",		  	IKT_STD,		KEYCODE_F11 },
	{ "KEYCODE_F12",		  	IKT_STD,		KEYCODE_F12 },
	{ "KEYCODE_ESC",		  	IKT_STD,		KEYCODE_ESC },

	{ "KEYCODE_TILDE",		  	IKT_STD,		KEYCODE_TILDE },
	{ "KEYCODE_MINUS",		  	IKT_STD,		KEYCODE_MINUS },
	{ "KEYCODE_EQUALS",		  	IKT_STD,		KEYCODE_EQUALS },
	{ "KEYCODE_BACKSPACE",	  	IKT_STD,		KEYCODE_BACKSPACE },
	{ "KEYCODE_TAB",		  	IKT_STD,		KEYCODE_TAB },
	{ "KEYCODE_OPENBRACE",	  	IKT_STD,		KEYCODE_OPENBRACE },
	{ "KEYCODE_CLOSEBRACE",	  	IKT_STD,		KEYCODE_CLOSEBRACE },
	{ "KEYCODE_ENTER",		  	IKT_STD,		KEYCODE_ENTER },
	{ "KEYCODE_COLON",		  	IKT_STD,		KEYCODE_COLON },
	{ "KEYCODE_QUOTE",		  	IKT_STD,		KEYCODE_QUOTE },
	{ "KEYCODE_BACKSLASH",	  	IKT_STD,		KEYCODE_BACKSLASH },
	{ "KEYCODE_BACKSLASH2",	  	IKT_STD,		KEYCODE_BACKSLASH2 },
	{ "KEYCODE_COMMA",		  	IKT_STD,		KEYCODE_COMMA },
	{ "KEYCODE_STOP",		  	IKT_STD,		KEYCODE_STOP },
	{ "KEYCODE_SLASH",		  	IKT_STD,		KEYCODE_SLASH },
	{ "KEYCODE_SPACE",		  	IKT_STD,		KEYCODE_SPACE },
	{ "KEYCODE_INSERT",		  	IKT_STD,		KEYCODE_INSERT },
	{ "KEYCODE_DEL",		  	IKT_STD,		KEYCODE_DEL },
	{ "KEYCODE_HOME",		  	IKT_STD,		KEYCODE_HOME },
	{ "KEYCODE_END",		  	IKT_STD,		KEYCODE_END },
	{ "KEYCODE_PGUP",		  	IKT_STD,		KEYCODE_PGUP },
	{ "KEYCODE_PGDN",		  	IKT_STD,		KEYCODE_PGDN },

	{ "KEYCODE_LEFT",		  	IKT_STD,		KEYCODE_LEFT },
	{ "KEYCODE_RIGHT",		  	IKT_STD,		KEYCODE_RIGHT },
	{ "KEYCODE_UP",			  	IKT_STD,		KEYCODE_UP },
	{ "KEYCODE_DOWN",		  	IKT_STD,		KEYCODE_DOWN },

	{ "KEYCODE_SLASH_PAD",	  	IKT_STD,		KEYCODE_SLASH_PAD },
	{ "KEYCODE_ASTERISK",	  	IKT_STD,		KEYCODE_ASTERISK },
	{ "KEYCODE_MINUS_PAD",	  	IKT_STD,		KEYCODE_MINUS_PAD },
	{ "KEYCODE_PLUS_PAD",	  	IKT_STD,		KEYCODE_PLUS_PAD },
	{ "KEYCODE_DEL_PAD",	  	IKT_STD,		KEYCODE_DEL_PAD },
	{ "KEYCODE_ENTER_PAD",	  	IKT_STD,		KEYCODE_ENTER_PAD },
	{ "KEYCODE_PRTSCR",		  	IKT_STD,		KEYCODE_PRTSCR },
	{ "KEYCODE_LSHIFT",		  	IKT_STD,		KEYCODE_LSHIFT },
	{ "KEYCODE_RSHIFT",		  	IKT_STD,		KEYCODE_RSHIFT },
	{ "KEYCODE_LCONTROL",	  	IKT_STD,		KEYCODE_LCONTROL },
	{ "KEYCODE_RCONTROL",	  	IKT_STD,		KEYCODE_RCONTROL },
	{ "KEYCODE_LALT",		  	IKT_STD,		KEYCODE_LALT },
	{ "KEYCODE_RALT",		  	IKT_STD,		KEYCODE_RALT },
	{ "KEYCODE_SCRLOCK",	  	IKT_STD,		KEYCODE_SCRLOCK },
	{ "KEYCODE_NUMLOCK",	  	IKT_STD,		KEYCODE_NUMLOCK },
	{ "KEYCODE_CAPSLOCK",	  	IKT_STD,		KEYCODE_CAPSLOCK },
	{ "KEYCODE_LWIN",		  	IKT_STD,		KEYCODE_LWIN },
	{ "KEYCODE_RWIN",		  	IKT_STD,		KEYCODE_RWIN },
	{ "KEYCODE_MENU",		  	IKT_STD,		KEYCODE_MENU },

	{ "JOYCODE_1_LEFT",		  	IKT_STD,		JOYCODE_1_LEFT },
	{ "JOYCODE_1_RIGHT",	  	IKT_STD,		JOYCODE_1_RIGHT },
	{ "JOYCODE_1_UP",		  	IKT_STD,		JOYCODE_1_UP },
	{ "JOYCODE_1_DOWN",		  	IKT_STD,		JOYCODE_1_DOWN },
	{ "JOYCODE_1_BUTTON1",	  	IKT_STD,		JOYCODE_1_BUTTON1 },
	{ "JOYCODE_1_BUTTON2",	  	IKT_STD,		JOYCODE_1_BUTTON2 },
	{ "JOYCODE_1_BUTTON3",	  	IKT_STD,		JOYCODE_1_BUTTON3 },
	{ "JOYCODE_1_BUTTON4",	  	IKT_STD,		JOYCODE_1_BUTTON4 },
	{ "JOYCODE_1_BUTTON5",	  	IKT_STD,		JOYCODE_1_BUTTON5 },
	{ "JOYCODE_1_BUTTON6",	  	IKT_STD,		JOYCODE_1_BUTTON6 },
	{ "JOYCODE_1_BUTTON7",	  	IKT_STD,		JOYCODE_1_BUTTON7 },
	{ "JOYCODE_1_BUTTON8",	  	IKT_STD,		JOYCODE_1_BUTTON8 },
	{ "JOYCODE_1_BUTTON9",	  	IKT_STD,		JOYCODE_1_BUTTON9 },
	{ "JOYCODE_1_BUTTON10",	  	IKT_STD,		JOYCODE_1_BUTTON10 },
	{ "JOYCODE_1_START",	  	IKT_STD,		JOYCODE_1_START },
	{ "JOYCODE_1_SELECT",	  	IKT_STD,		JOYCODE_1_SELECT },
	{ "JOYCODE_2_LEFT",		  	IKT_STD,		JOYCODE_2_LEFT },
	{ "JOYCODE_2_RIGHT",	  	IKT_STD,		JOYCODE_2_RIGHT },
	{ "JOYCODE_2_UP",		  	IKT_STD,		JOYCODE_2_UP },
	{ "JOYCODE_2_DOWN",		  	IKT_STD,		JOYCODE_2_DOWN },
	{ "JOYCODE_2_BUTTON1",	  	IKT_STD,		JOYCODE_2_BUTTON1 },
	{ "JOYCODE_2_BUTTON2",	  	IKT_STD,		JOYCODE_2_BUTTON2 },
	{ "JOYCODE_2_BUTTON3",	  	IKT_STD,		JOYCODE_2_BUTTON3 },
	{ "JOYCODE_2_BUTTON4",	  	IKT_STD,		JOYCODE_2_BUTTON4 },
	{ "JOYCODE_2_BUTTON5",	  	IKT_STD,		JOYCODE_2_BUTTON5 },
	{ "JOYCODE_2_BUTTON6",	  	IKT_STD,		JOYCODE_2_BUTTON6 },
	{ "JOYCODE_2_BUTTON7",	  	IKT_STD,		JOYCODE_2_BUTTON7 },
	{ "JOYCODE_2_BUTTON8",	  	IKT_STD,		JOYCODE_2_BUTTON8 },
	{ "JOYCODE_2_BUTTON9",	  	IKT_STD,		JOYCODE_2_BUTTON9 },
	{ "JOYCODE_2_BUTTON10",	  	IKT_STD,		JOYCODE_2_BUTTON10 },
	{ "JOYCODE_2_START",	  	IKT_STD,		JOYCODE_2_START },
	{ "JOYCODE_2_SELECT",	  	IKT_STD,		JOYCODE_2_SELECT },
	{ "JOYCODE_3_LEFT",		  	IKT_STD,		JOYCODE_3_LEFT },
	{ "JOYCODE_3_RIGHT",	  	IKT_STD,		JOYCODE_3_RIGHT },
	{ "JOYCODE_3_UP",		  	IKT_STD,		JOYCODE_3_UP },
	{ "JOYCODE_3_DOWN",		  	IKT_STD,		JOYCODE_3_DOWN },
	{ "JOYCODE_3_BUTTON1",	  	IKT_STD,		JOYCODE_3_BUTTON1 },
	{ "JOYCODE_3_BUTTON2",	  	IKT_STD,		JOYCODE_3_BUTTON2 },
	{ "JOYCODE_3_BUTTON3",	  	IKT_STD,		JOYCODE_3_BUTTON3 },
	{ "JOYCODE_3_BUTTON4",	  	IKT_STD,		JOYCODE_3_BUTTON4 },
	{ "JOYCODE_3_BUTTON5",	  	IKT_STD,		JOYCODE_3_BUTTON5 },
	{ "JOYCODE_3_BUTTON6",	  	IKT_STD,		JOYCODE_3_BUTTON6 },
	{ "JOYCODE_3_BUTTON7",	  	IKT_STD,		JOYCODE_3_BUTTON7 },
	{ "JOYCODE_3_BUTTON8",	  	IKT_STD,		JOYCODE_3_BUTTON8 },
	{ "JOYCODE_3_BUTTON9",	  	IKT_STD,		JOYCODE_3_BUTTON9 },
	{ "JOYCODE_3_BUTTON10",	  	IKT_STD,		JOYCODE_3_BUTTON10 },
	{ "JOYCODE_3_START",	  	IKT_STD,		JOYCODE_3_START },
	{ "JOYCODE_3_SELECT",	  	IKT_STD,		JOYCODE_3_SELECT },
	{ "JOYCODE_4_LEFT",		  	IKT_STD,		JOYCODE_4_LEFT },
	{ "JOYCODE_4_RIGHT",	  	IKT_STD,		JOYCODE_4_RIGHT },
	{ "JOYCODE_4_UP",		  	IKT_STD,		JOYCODE_4_UP },
	{ "JOYCODE_4_DOWN",		  	IKT_STD,		JOYCODE_4_DOWN },
	{ "JOYCODE_4_BUTTON1",	  	IKT_STD,		JOYCODE_4_BUTTON1 },
	{ "JOYCODE_4_BUTTON2",	  	IKT_STD,		JOYCODE_4_BUTTON2 },
	{ "JOYCODE_4_BUTTON3",	  	IKT_STD,		JOYCODE_4_BUTTON3 },
	{ "JOYCODE_4_BUTTON4",	  	IKT_STD,		JOYCODE_4_BUTTON4 },
	{ "JOYCODE_4_BUTTON5",	  	IKT_STD,		JOYCODE_4_BUTTON5 },
	{ "JOYCODE_4_BUTTON6",	  	IKT_STD,		JOYCODE_4_BUTTON6 },
	{ "JOYCODE_4_BUTTON7",	  	IKT_STD,		JOYCODE_4_BUTTON7 },
	{ "JOYCODE_4_BUTTON8",	  	IKT_STD,		JOYCODE_4_BUTTON8 },
	{ "JOYCODE_4_BUTTON9",	  	IKT_STD,		JOYCODE_4_BUTTON9 },
	{ "JOYCODE_4_BUTTON10",	  	IKT_STD,		JOYCODE_4_BUTTON10 },
	{ "JOYCODE_4_START",	  	IKT_STD,		JOYCODE_4_START },
	{ "JOYCODE_4_SELECT",	  	IKT_STD,		JOYCODE_4_SELECT },
	{ "JOYCODE_5_LEFT",		  	IKT_STD,		JOYCODE_5_LEFT },
	{ "JOYCODE_5_RIGHT",	  	IKT_STD,		JOYCODE_5_RIGHT },
	{ "JOYCODE_5_UP",		  	IKT_STD,		JOYCODE_5_UP },
	{ "JOYCODE_5_DOWN",		  	IKT_STD,		JOYCODE_5_DOWN },
	{ "JOYCODE_5_BUTTON1",	  	IKT_STD,		JOYCODE_5_BUTTON1 },
	{ "JOYCODE_5_BUTTON2",	  	IKT_STD,		JOYCODE_5_BUTTON2 },
	{ "JOYCODE_5_BUTTON3",	  	IKT_STD,		JOYCODE_5_BUTTON3 },
	{ "JOYCODE_5_BUTTON4",	  	IKT_STD,		JOYCODE_5_BUTTON4 },
	{ "JOYCODE_5_BUTTON5",	  	IKT_STD,		JOYCODE_5_BUTTON5 },
	{ "JOYCODE_5_BUTTON6",	  	IKT_STD,		JOYCODE_5_BUTTON6 },
	{ "JOYCODE_5_BUTTON7",	  	IKT_STD,		JOYCODE_5_BUTTON7 },
	{ "JOYCODE_5_BUTTON8",	  	IKT_STD,		JOYCODE_5_BUTTON8 },
	{ "JOYCODE_5_BUTTON9",	  	IKT_STD,		JOYCODE_5_BUTTON9 },
	{ "JOYCODE_5_BUTTON10",	  	IKT_STD,		JOYCODE_5_BUTTON10 },
	{ "JOYCODE_5_START",	  	IKT_STD,		JOYCODE_5_START },
	{ "JOYCODE_5_SELECT",	  	IKT_STD,		JOYCODE_5_SELECT },
	{ "JOYCODE_6_LEFT",		  	IKT_STD,		JOYCODE_6_LEFT },
	{ "JOYCODE_6_RIGHT",	  	IKT_STD,		JOYCODE_6_RIGHT },
	{ "JOYCODE_6_UP",		  	IKT_STD,		JOYCODE_6_UP },
	{ "JOYCODE_6_DOWN",		  	IKT_STD,		JOYCODE_6_DOWN },
	{ "JOYCODE_6_BUTTON1",	  	IKT_STD,		JOYCODE_6_BUTTON1 },
	{ "JOYCODE_6_BUTTON2",	  	IKT_STD,		JOYCODE_6_BUTTON2 },
	{ "JOYCODE_6_BUTTON3",	  	IKT_STD,		JOYCODE_6_BUTTON3 },
	{ "JOYCODE_6_BUTTON4",	  	IKT_STD,		JOYCODE_6_BUTTON4 },
	{ "JOYCODE_6_BUTTON5",	  	IKT_STD,		JOYCODE_6_BUTTON5 },
	{ "JOYCODE_6_BUTTON6",	  	IKT_STD,		JOYCODE_6_BUTTON6 },
	{ "JOYCODE_6_BUTTON7",	  	IKT_STD,		JOYCODE_6_BUTTON7 },
	{ "JOYCODE_6_BUTTON8",	  	IKT_STD,		JOYCODE_6_BUTTON8 },
	{ "JOYCODE_6_BUTTON9",	  	IKT_STD,		JOYCODE_6_BUTTON9 },
	{ "JOYCODE_6_BUTTON10",	  	IKT_STD,		JOYCODE_6_BUTTON10 },
	{ "JOYCODE_6_START",	  	IKT_STD,		JOYCODE_6_START },
	{ "JOYCODE_6_SELECT",	  	IKT_STD,		JOYCODE_6_SELECT },
	{ "JOYCODE_7_LEFT",		  	IKT_STD,		JOYCODE_7_LEFT },
	{ "JOYCODE_7_RIGHT",	  	IKT_STD,		JOYCODE_7_RIGHT },
	{ "JOYCODE_7_UP",		  	IKT_STD,		JOYCODE_7_UP },
	{ "JOYCODE_7_DOWN",		  	IKT_STD,		JOYCODE_7_DOWN },
	{ "JOYCODE_7_BUTTON1",	  	IKT_STD,		JOYCODE_7_BUTTON1 },
	{ "JOYCODE_7_BUTTON2",	  	IKT_STD,		JOYCODE_7_BUTTON2 },
	{ "JOYCODE_7_BUTTON3",	  	IKT_STD,		JOYCODE_7_BUTTON3 },
	{ "JOYCODE_7_BUTTON4",	  	IKT_STD,		JOYCODE_7_BUTTON4 },
	{ "JOYCODE_7_BUTTON5",	  	IKT_STD,		JOYCODE_7_BUTTON5 },
	{ "JOYCODE_7_BUTTON6",	  	IKT_STD,		JOYCODE_7_BUTTON6 },
	{ "JOYCODE_7_BUTTON7",	  	IKT_STD,		JOYCODE_7_BUTTON7 },
	{ "JOYCODE_7_BUTTON8",	  	IKT_STD,		JOYCODE_7_BUTTON8 },
	{ "JOYCODE_7_BUTTON9",	  	IKT_STD,		JOYCODE_7_BUTTON9 },
	{ "JOYCODE_7_BUTTON10",	  	IKT_STD,		JOYCODE_7_BUTTON10 },
	{ "JOYCODE_7_START",	  	IKT_STD,		JOYCODE_7_START },
	{ "JOYCODE_7_SELECT",	  	IKT_STD,		JOYCODE_7_SELECT },
	{ "JOYCODE_8_LEFT",		  	IKT_STD,		JOYCODE_8_LEFT },
	{ "JOYCODE_8_RIGHT",	  	IKT_STD,		JOYCODE_8_RIGHT },
	{ "JOYCODE_8_UP",		  	IKT_STD,		JOYCODE_8_UP },
	{ "JOYCODE_8_DOWN",		  	IKT_STD,		JOYCODE_8_DOWN },
	{ "JOYCODE_8_BUTTON1",	  	IKT_STD,		JOYCODE_8_BUTTON1 },
	{ "JOYCODE_8_BUTTON2",	  	IKT_STD,		JOYCODE_8_BUTTON2 },
	{ "JOYCODE_8_BUTTON3",	  	IKT_STD,		JOYCODE_8_BUTTON3 },
	{ "JOYCODE_8_BUTTON4",	  	IKT_STD,		JOYCODE_8_BUTTON4 },
	{ "JOYCODE_8_BUTTON5",	  	IKT_STD,		JOYCODE_8_BUTTON5 },
	{ "JOYCODE_8_BUTTON6",	  	IKT_STD,		JOYCODE_8_BUTTON6 },
	{ "JOYCODE_8_BUTTON7",	  	IKT_STD,		JOYCODE_8_BUTTON7 },
	{ "JOYCODE_8_BUTTON8",	  	IKT_STD,		JOYCODE_8_BUTTON8 },
	{ "JOYCODE_8_BUTTON9",	  	IKT_STD,		JOYCODE_8_BUTTON9 },
	{ "JOYCODE_8_BUTTON10",	  	IKT_STD,		JOYCODE_8_BUTTON10 },
	{ "JOYCODE_8_START",	  	IKT_STD,		JOYCODE_8_START },
	{ "JOYCODE_8_SELECT",	  	IKT_STD,		JOYCODE_8_SELECT },

	{ "MOUSECODE_1_BUTTON1", 	IKT_STD,		JOYCODE_MOUSE_1_BUTTON1 },
	{ "MOUSECODE_1_BUTTON2", 	IKT_STD,		JOYCODE_MOUSE_1_BUTTON2 },
	{ "MOUSECODE_1_BUTTON3", 	IKT_STD,		JOYCODE_MOUSE_1_BUTTON3 },
	{ "MOUSECODE_2_BUTTON1", 	IKT_STD,		JOYCODE_MOUSE_2_BUTTON1 },
	{ "MOUSECODE_2_BUTTON2", 	IKT_STD,		JOYCODE_MOUSE_2_BUTTON2 },
	{ "MOUSECODE_2_BUTTON3", 	IKT_STD,		JOYCODE_MOUSE_2_BUTTON3 },
	{ "MOUSECODE_3_BUTTON1", 	IKT_STD,		JOYCODE_MOUSE_3_BUTTON1 },
	{ "MOUSECODE_3_BUTTON2", 	IKT_STD,		JOYCODE_MOUSE_3_BUTTON2 },
	{ "MOUSECODE_3_BUTTON3", 	IKT_STD,		JOYCODE_MOUSE_3_BUTTON3 },
	{ "MOUSECODE_4_BUTTON1", 	IKT_STD,		JOYCODE_MOUSE_4_BUTTON1 },
	{ "MOUSECODE_4_BUTTON2", 	IKT_STD,		JOYCODE_MOUSE_4_BUTTON2 },
	{ "MOUSECODE_4_BUTTON3", 	IKT_STD,		JOYCODE_MOUSE_4_BUTTON3 },
	{ "MOUSECODE_5_BUTTON1", 	IKT_STD,		JOYCODE_MOUSE_5_BUTTON1 },
	{ "MOUSECODE_5_BUTTON2", 	IKT_STD,		JOYCODE_MOUSE_5_BUTTON2 },
	{ "MOUSECODE_5_BUTTON3", 	IKT_STD,		JOYCODE_MOUSE_5_BUTTON3 },
	{ "MOUSECODE_6_BUTTON1", 	IKT_STD,		JOYCODE_MOUSE_6_BUTTON1 },
	{ "MOUSECODE_6_BUTTON2", 	IKT_STD,		JOYCODE_MOUSE_6_BUTTON2 },
	{ "MOUSECODE_6_BUTTON3", 	IKT_STD,		JOYCODE_MOUSE_6_BUTTON3 },

	{ "JOYCODE_1_LEFT_LEFT",	IKT_STD,		JOYCODE_1_LEFT_LEFT },
	{ "JOYCODE_1_LEFT_RIGHT",	IKT_STD,		JOYCODE_1_LEFT_RIGHT },
	{ "JOYCODE_1_LEFT_UP",  	IKT_STD,		JOYCODE_1_LEFT_UP },
	{ "JOYCODE_1_LEFT_DOWN",	IKT_STD,		JOYCODE_1_LEFT_DOWN },
	{ "JOYCODE_1_RIGHT_LEFT",	IKT_STD,		JOYCODE_1_RIGHT_LEFT },
	{ "JOYCODE_1_RIGHT_RIGHT",	IKT_STD,		JOYCODE_1_RIGHT_RIGHT },
	{ "JOYCODE_1_RIGHT_UP",  	IKT_STD,		JOYCODE_1_RIGHT_UP },
	{ "JOYCODE_1_RIGHT_DOWN",	IKT_STD,		JOYCODE_1_RIGHT_DOWN },

	{ "JOYCODE_2_LEFT_LEFT",	IKT_STD,		JOYCODE_2_LEFT_LEFT },
	{ "JOYCODE_2_LEFT_RIGHT",	IKT_STD,		JOYCODE_2_LEFT_RIGHT },
	{ "JOYCODE_2_LEFT_UP",  	IKT_STD,		JOYCODE_2_LEFT_UP },
	{ "JOYCODE_2_LEFT_DOWN",	IKT_STD,		JOYCODE_2_LEFT_DOWN },
	{ "JOYCODE_2_RIGHT_LEFT",	IKT_STD,		JOYCODE_2_RIGHT_LEFT },
	{ "JOYCODE_2_RIGHT_RIGHT",	IKT_STD,		JOYCODE_2_RIGHT_RIGHT },
	{ "JOYCODE_2_RIGHT_UP",  	IKT_STD,		JOYCODE_2_RIGHT_UP },
	{ "JOYCODE_2_RIGHT_DOWN",	IKT_STD,		JOYCODE_2_RIGHT_DOWN },

	{ "JOYCODE_3_LEFT_LEFT",	IKT_STD,		JOYCODE_3_LEFT_LEFT },
	{ "JOYCODE_3_LEFT_RIGHT",	IKT_STD,		JOYCODE_3_LEFT_RIGHT },
	{ "JOYCODE_3_LEFT_UP",  	IKT_STD,		JOYCODE_3_LEFT_UP },
	{ "JOYCODE_3_LEFT_DOWN",	IKT_STD,		JOYCODE_3_LEFT_DOWN },
	{ "JOYCODE_3_RIGHT_LEFT",	IKT_STD,		JOYCODE_3_RIGHT_LEFT },
	{ "JOYCODE_3_RIGHT_RIGHT",	IKT_STD,		JOYCODE_3_RIGHT_RIGHT },
	{ "JOYCODE_3_RIGHT_UP",  	IKT_STD,		JOYCODE_3_RIGHT_UP },
	{ "JOYCODE_3_RIGHT_DOWN",	IKT_STD,		JOYCODE_3_RIGHT_DOWN },

	{ "JOYCODE_4_LEFT_LEFT",	IKT_STD,		JOYCODE_4_LEFT_LEFT },
	{ "JOYCODE_4_LEFT_RIGHT",	IKT_STD,		JOYCODE_4_LEFT_RIGHT },
	{ "JOYCODE_4_LEFT_UP",  	IKT_STD,		JOYCODE_4_LEFT_UP },
	{ "JOYCODE_4_LEFT_DOWN",	IKT_STD,		JOYCODE_4_LEFT_DOWN },
	{ "JOYCODE_4_RIGHT_LEFT",	IKT_STD,		JOYCODE_4_RIGHT_LEFT },
	{ "JOYCODE_4_RIGHT_RIGHT",	IKT_STD,		JOYCODE_4_RIGHT_RIGHT },
	{ "JOYCODE_4_RIGHT_UP",  	IKT_STD,		JOYCODE_4_RIGHT_UP },
	{ "JOYCODE_4_RIGHT_DOWN",	IKT_STD,		JOYCODE_4_RIGHT_DOWN },

	{ "JOYCODE_5_LEFT_LEFT",	IKT_STD,		JOYCODE_5_LEFT_LEFT },
	{ "JOYCODE_5_LEFT_RIGHT",	IKT_STD,		JOYCODE_5_LEFT_RIGHT },
	{ "JOYCODE_5_LEFT_UP",  	IKT_STD,		JOYCODE_5_LEFT_UP },
	{ "JOYCODE_5_LEFT_DOWN",	IKT_STD,		JOYCODE_5_LEFT_DOWN },
	{ "JOYCODE_5_RIGHT_LEFT",	IKT_STD,		JOYCODE_5_RIGHT_LEFT },
	{ "JOYCODE_5_RIGHT_RIGHT",	IKT_STD,		JOYCODE_5_RIGHT_RIGHT },
	{ "JOYCODE_5_RIGHT_UP",  	IKT_STD,		JOYCODE_5_RIGHT_UP },
	{ "JOYCODE_5_RIGHT_DOWN",	IKT_STD,		JOYCODE_5_RIGHT_DOWN },

	{ "JOYCODE_6_LEFT_LEFT",	IKT_STD,		JOYCODE_6_LEFT_LEFT },
	{ "JOYCODE_6_LEFT_RIGHT",	IKT_STD,		JOYCODE_6_LEFT_RIGHT },
	{ "JOYCODE_6_LEFT_UP",  	IKT_STD,		JOYCODE_6_LEFT_UP },
	{ "JOYCODE_6_LEFT_DOWN",	IKT_STD,		JOYCODE_6_LEFT_DOWN },
	{ "JOYCODE_6_RIGHT_LEFT",	IKT_STD,		JOYCODE_6_RIGHT_LEFT },
	{ "JOYCODE_6_RIGHT_RIGHT",	IKT_STD,		JOYCODE_6_RIGHT_RIGHT },
	{ "JOYCODE_6_RIGHT_UP",  	IKT_STD,		JOYCODE_6_RIGHT_UP },
	{ "JOYCODE_6_RIGHT_DOWN",	IKT_STD,		JOYCODE_6_RIGHT_DOWN },

	{ "JOYCODE_7_LEFT_LEFT",	IKT_STD,		JOYCODE_7_LEFT_LEFT },
	{ "JOYCODE_7_LEFT_RIGHT",	IKT_STD,		JOYCODE_7_LEFT_RIGHT },
	{ "JOYCODE_7_LEFT_UP",  	IKT_STD,		JOYCODE_7_LEFT_UP },
	{ "JOYCODE_7_LEFT_DOWN",	IKT_STD,		JOYCODE_7_LEFT_DOWN },
	{ "JOYCODE_7_RIGHT_LEFT",	IKT_STD,		JOYCODE_7_RIGHT_LEFT },
	{ "JOYCODE_7_RIGHT_RIGHT",	IKT_STD,		JOYCODE_7_RIGHT_RIGHT },
	{ "JOYCODE_7_RIGHT_UP",  	IKT_STD,		JOYCODE_7_RIGHT_UP },
	{ "JOYCODE_7_RIGHT_DOWN",	IKT_STD,		JOYCODE_7_RIGHT_DOWN },

	{ "JOYCODE_8_LEFT_LEFT",	IKT_STD,		JOYCODE_8_LEFT_LEFT },
	{ "JOYCODE_8_LEFT_RIGHT",	IKT_STD,		JOYCODE_8_LEFT_RIGHT },
	{ "JOYCODE_8_LEFT_UP",  	IKT_STD,		JOYCODE_8_LEFT_UP },
	{ "JOYCODE_8_LEFT_DOWN",	IKT_STD,		JOYCODE_8_LEFT_DOWN },
	{ "JOYCODE_8_RIGHT_LEFT",	IKT_STD,		JOYCODE_8_RIGHT_LEFT },
	{ "JOYCODE_8_RIGHT_RIGHT",	IKT_STD,		JOYCODE_8_RIGHT_RIGHT },
	{ "JOYCODE_8_RIGHT_UP",  	IKT_STD,		JOYCODE_8_RIGHT_UP },
	{ "JOYCODE_8_RIGHT_DOWN",	IKT_STD,		JOYCODE_8_RIGHT_DOWN },



	{ "KEYCODE_NONE",			IKT_STD,		CODE_NONE },
	{ "CODE_NONE",			  	IKT_STD,		CODE_NONE },
	{ "CODE_OTHER",				IKT_STD,		CODE_OTHER },
	{ "CODE_DEFAULT",			IKT_STD,		CODE_DEFAULT },
	{ "CODE_PREVIOUS",			IKT_STD,		CODE_PREVIOUS },
	{ "CODE_NOT",				IKT_STD,		CODE_NOT },
	{ "CODE_OR",			   	IKT_STD,		CODE_OR },
	{ "!",						IKT_STD,		CODE_NOT },
	{ "|",					   	IKT_STD,		CODE_OR },

	{ "UI_CONFIGURE", 			IKT_IPT,	 	IPT_UI_CONFIGURE },
	{ "UI_RESET_MACHINE",		IKT_IPT,		IPT_UI_RESET_MACHINE },
	{ "UI_TOGGLE_CHEAT",		IKT_IPT,		IPT_UI_TOGGLE_CHEAT },
	{ "UI_UP",					IKT_IPT,		IPT_UI_UP },
	{ "UI_DOWN",				IKT_IPT,		IPT_UI_DOWN },
	{ "UI_LEFT",				IKT_IPT,		IPT_UI_LEFT },
	{ "UI_RIGHT",				IKT_IPT,		IPT_UI_RIGHT },
	{ "UI_SELECT",				IKT_IPT,		IPT_UI_SELECT },
	{ "UI_CANCEL",				IKT_IPT,		IPT_UI_CANCEL },
	{ "UI_ADD_CHEAT",			IKT_IPT,		IPT_UI_ADD_CHEAT },
	{ "UI_DELETE_CHEAT",		IKT_IPT,		IPT_UI_DELETE_CHEAT },
	{ "UI_SAVE_CHEAT",			IKT_IPT,		IPT_UI_SAVE_CHEAT },
	{ "UI_WATCH_VALUE",			IKT_IPT,		IPT_UI_WATCH_VALUE },
	{ "UI_EDIT_CHEAT",			IKT_IPT,		IPT_UI_EDIT_CHEAT },
	{ "START1",					IKT_IPT,		IPT_START1 },
	{ "START2",					IKT_IPT,		IPT_START2 },
	{ "START3",					IKT_IPT,		IPT_START3 },
	{ "START4",					IKT_IPT,		IPT_START4 },
	{ "START5",					IKT_IPT,		IPT_START5 },
	{ "START6",					IKT_IPT,		IPT_START6 },
	{ "START7",					IKT_IPT,		IPT_START7 },
	{ "START8",					IKT_IPT,		IPT_START8 },
	{ "COIN1",					IKT_IPT,		IPT_COIN1 },
	{ "COIN2",					IKT_IPT,		IPT_COIN2 },
	{ "COIN3",					IKT_IPT,		IPT_COIN3 },
	{ "COIN4",					IKT_IPT,		IPT_COIN4 },
	{ "COIN5",					IKT_IPT,		IPT_COIN5 },
	{ "COIN6",					IKT_IPT,		IPT_COIN6 },
	{ "COIN7",					IKT_IPT,		IPT_COIN7 },
	{ "COIN8",					IKT_IPT,		IPT_COIN8 },
	{ "SERVICE1",				IKT_IPT,		IPT_SERVICE1 },
	{ "SERVICE2",				IKT_IPT,		IPT_SERVICE2 },
	{ "SERVICE3",				IKT_IPT,		IPT_SERVICE3 },
	{ "SERVICE4",				IKT_IPT,		IPT_SERVICE4 },
	{ "TILT",					IKT_IPT,		IPT_TILT },

	{ "P1_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICK_UP },
	{ "P1_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICK_DOWN },
	{ "P1_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICK_LEFT },
	{ "P1_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICK_RIGHT },
	{ "P1_BUTTON1",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON1 },
	{ "P1_BUTTON2",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON2 },
	{ "P1_BUTTON3",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON3 },
	{ "P1_BUTTON4",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON4 },
	{ "P1_BUTTON5",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON5 },
	{ "P1_BUTTON6",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON6 },
	{ "P1_BUTTON7",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON7 },
	{ "P1_BUTTON8",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON8 },
	{ "P1_BUTTON9",				IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON9 },
	{ "P1_BUTTON10",			IKT_IPT,		IPF_PLAYER1 | IPT_BUTTON10 },
	{ "P1_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKRIGHT_UP },
	{ "P1_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P1_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P1_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P1_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKLEFT_UP },
	{ "P1_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKLEFT_DOWN },
	{ "P1_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKLEFT_LEFT },
	{ "P1_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER1 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P2_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICK_UP },
	{ "P2_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICK_DOWN },
	{ "P2_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICK_LEFT },
	{ "P2_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICK_RIGHT },
	{ "P2_BUTTON1",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON1 },
	{ "P2_BUTTON2",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON2 },
	{ "P2_BUTTON3",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON3 },
	{ "P2_BUTTON4",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON4 },
	{ "P2_BUTTON5",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON5 },
	{ "P2_BUTTON6",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON6 },
	{ "P2_BUTTON7",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON7 },
	{ "P2_BUTTON8",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON8 },
	{ "P2_BUTTON9",				IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON9 },
	{ "P2_BUTTON10",			IKT_IPT,		IPF_PLAYER2 | IPT_BUTTON10 },
	{ "P2_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKRIGHT_UP },
	{ "P2_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P2_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P2_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P2_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKLEFT_UP },
	{ "P2_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKLEFT_DOWN },
	{ "P2_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKLEFT_LEFT },
	{ "P2_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER2 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P3_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICK_UP },
	{ "P3_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICK_DOWN },
	{ "P3_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICK_LEFT },
	{ "P3_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICK_RIGHT },
	{ "P3_BUTTON1",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON1 },
	{ "P3_BUTTON2",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON2 },
	{ "P3_BUTTON3",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON3 },
	{ "P3_BUTTON4",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON4 },
	{ "P3_BUTTON5",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON5 },
	{ "P3_BUTTON6",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON6 },
	{ "P3_BUTTON7",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON7 },
	{ "P3_BUTTON8",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON8 },
	{ "P3_BUTTON9",				IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON9 },
	{ "P3_BUTTON10",			IKT_IPT,		IPF_PLAYER3 | IPT_BUTTON10 },
	{ "P3_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKRIGHT_UP },
	{ "P3_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P3_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P3_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P3_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKLEFT_UP },
	{ "P3_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKLEFT_DOWN },
	{ "P3_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKLEFT_LEFT },
	{ "P3_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER3 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P4_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICK_UP },
	{ "P4_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICK_DOWN },
	{ "P4_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICK_LEFT },
	{ "P4_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICK_RIGHT },
	{ "P4_BUTTON1",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON1 },
	{ "P4_BUTTON2",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON2 },
	{ "P4_BUTTON3",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON3 },
	{ "P4_BUTTON4",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON4 },
	{ "P4_BUTTON5",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON5 },
	{ "P4_BUTTON6",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON6 },
	{ "P4_BUTTON7",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON7 },
	{ "P4_BUTTON8",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON8 },
	{ "P4_BUTTON9",				IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON9 },
	{ "P4_BUTTON10",			IKT_IPT,		IPF_PLAYER4 | IPT_BUTTON10 },
	{ "P4_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKRIGHT_UP },
	{ "P4_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P4_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P4_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P4_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKLEFT_UP },
	{ "P4_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKLEFT_DOWN },
	{ "P4_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKLEFT_LEFT },
	{ "P4_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER4 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P5_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICK_UP },
	{ "P5_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICK_DOWN },
	{ "P5_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICK_LEFT },
	{ "P5_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICK_RIGHT },
	{ "P5_BUTTON1",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON1 },
	{ "P5_BUTTON2",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON2 },
	{ "P5_BUTTON3",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON3 },
	{ "P5_BUTTON4",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON4 },
	{ "P5_BUTTON5",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON5 },
	{ "P5_BUTTON6",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON6 },
	{ "P5_BUTTON7",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON7 },
	{ "P5_BUTTON8",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON8 },
	{ "P5_BUTTON9",				IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON9 },
	{ "P5_BUTTON10",			IKT_IPT,		IPF_PLAYER5 | IPT_BUTTON10 },
	{ "P5_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKRIGHT_UP },
	{ "P5_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P5_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P5_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P5_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKLEFT_UP },
	{ "P5_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKLEFT_DOWN },
	{ "P5_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKLEFT_LEFT },
	{ "P5_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER5 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P6_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICK_UP },
	{ "P6_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICK_DOWN },
	{ "P6_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICK_LEFT },
	{ "P6_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICK_RIGHT },
	{ "P6_BUTTON1",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON1 },
	{ "P6_BUTTON2",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON2 },
	{ "P6_BUTTON3",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON3 },
	{ "P6_BUTTON4",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON4 },
	{ "P6_BUTTON5",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON5 },
	{ "P6_BUTTON6",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON6 },
	{ "P6_BUTTON7",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON7 },
	{ "P6_BUTTON8",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON8 },
	{ "P6_BUTTON9",				IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON9 },
	{ "P6_BUTTON10",			IKT_IPT,		IPF_PLAYER6 | IPT_BUTTON10 },
	{ "P6_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKRIGHT_UP },
	{ "P6_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P6_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P6_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P6_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKLEFT_UP },
	{ "P6_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKLEFT_DOWN },
	{ "P6_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKLEFT_LEFT },
	{ "P6_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER6 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P7_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICK_UP },
	{ "P7_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICK_DOWN },
	{ "P7_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICK_LEFT },
	{ "P7_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICK_RIGHT },
	{ "P7_BUTTON1",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON1 },
	{ "P7_BUTTON2",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON2 },
	{ "P7_BUTTON3",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON3 },
	{ "P7_BUTTON4",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON4 },
	{ "P7_BUTTON5",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON5 },
	{ "P7_BUTTON6",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON6 },
	{ "P7_BUTTON7",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON7 },
	{ "P7_BUTTON8",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON8 },
	{ "P7_BUTTON9",				IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON9 },
	{ "P7_BUTTON10",			IKT_IPT,		IPF_PLAYER7 | IPT_BUTTON10 },
	{ "P7_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKRIGHT_UP },
	{ "P7_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P7_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P7_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P7_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKLEFT_UP },
	{ "P7_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKLEFT_DOWN },
	{ "P7_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKLEFT_LEFT },
	{ "P7_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER7 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P8_JOYSTICK_UP",			IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICK_UP },
	{ "P8_JOYSTICK_DOWN",		IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICK_DOWN },
	{ "P8_JOYSTICK_LEFT",		IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICK_LEFT },
	{ "P8_JOYSTICK_RIGHT",		IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICK_RIGHT },
	{ "P8_BUTTON1",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON1 },
	{ "P8_BUTTON2",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON2 },
	{ "P8_BUTTON3",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON3 },
	{ "P8_BUTTON4",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON4 },
	{ "P8_BUTTON5",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON5 },
	{ "P8_BUTTON6",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON6 },
	{ "P8_BUTTON7",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON7 },
	{ "P8_BUTTON8",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON8 },
	{ "P8_BUTTON9",				IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON9 },
	{ "P8_BUTTON10",			IKT_IPT,		IPF_PLAYER8 | IPT_BUTTON10 },
	{ "P8_JOYSTICKRIGHT_UP",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKRIGHT_UP },
	{ "P8_JOYSTICKRIGHT_DOWN",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKRIGHT_DOWN },
	{ "P8_JOYSTICKRIGHT_LEFT",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKRIGHT_LEFT },
	{ "P8_JOYSTICKRIGHT_RIGHT",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKRIGHT_RIGHT },
	{ "P8_JOYSTICKLEFT_UP",		IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKLEFT_UP },
	{ "P8_JOYSTICKLEFT_DOWN",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKLEFT_DOWN },
	{ "P8_JOYSTICKLEFT_LEFT",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKLEFT_LEFT },
	{ "P8_JOYSTICKLEFT_RIGHT",	IKT_IPT,		IPF_PLAYER8 | IPT_JOYSTICKLEFT_RIGHT },

	{ "P1_PEDAL",				IKT_IPT,		IPF_PLAYER1 | IPT_PEDAL },
	{ "P1_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER1 | IPT_PEDAL },
	{ "P2_PEDAL",				IKT_IPT,		IPF_PLAYER2 | IPT_PEDAL },
	{ "P2_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER2 | IPT_PEDAL },
	{ "P3_PEDAL",				IKT_IPT,		IPF_PLAYER3 | IPT_PEDAL },
	{ "P3_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER3 | IPT_PEDAL },
	{ "P4_PEDAL",				IKT_IPT,		IPF_PLAYER4 | IPT_PEDAL },
	{ "P4_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER4 | IPT_PEDAL },
	{ "P5_PEDAL",				IKT_IPT,		IPF_PLAYER5 | IPT_PEDAL },
	{ "P5_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER5 | IPT_PEDAL },
	{ "P6_PEDAL",				IKT_IPT,		IPF_PLAYER6 | IPT_PEDAL },
	{ "P6_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER6 | IPT_PEDAL },
	{ "P7_PEDAL",				IKT_IPT,		IPF_PLAYER7 | IPT_PEDAL },
	{ "P7_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER7 | IPT_PEDAL },
	{ "P8_PEDAL",				IKT_IPT,		IPF_PLAYER8 | IPT_PEDAL },
	{ "P8_PEDAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER8 | IPT_PEDAL },

	{ "P1_PEDAL2",				IKT_IPT,		IPF_PLAYER1 | IPT_PEDAL2 },
	{ "P1_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER1 | IPT_PEDAL2 },
	{ "P2_PEDAL2",				IKT_IPT,		IPF_PLAYER2 | IPT_PEDAL2 },
	{ "P2_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER2 | IPT_PEDAL2 },
	{ "P3_PEDAL2",				IKT_IPT,		IPF_PLAYER3 | IPT_PEDAL2 },
	{ "P3_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER3 | IPT_PEDAL2 },
	{ "P4_PEDAL2",				IKT_IPT,		IPF_PLAYER4 | IPT_PEDAL2 },
	{ "P4_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER4 | IPT_PEDAL2 },
	{ "P5_PEDAL2",				IKT_IPT,		IPF_PLAYER5 | IPT_PEDAL2 },
	{ "P5_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER5 | IPT_PEDAL2 },
	{ "P6_PEDAL2",				IKT_IPT,		IPF_PLAYER6 | IPT_PEDAL2 },
	{ "P6_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER6 | IPT_PEDAL2 },
	{ "P7_PEDAL2",				IKT_IPT,		IPF_PLAYER7 | IPT_PEDAL2 },
	{ "P7_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER7 | IPT_PEDAL2 },
	{ "P8_PEDAL2",				IKT_IPT,		IPF_PLAYER8 | IPT_PEDAL2 },
	{ "P8_PEDAL2_EXT",			IKT_IPT_EXT,	IPF_PLAYER8 | IPT_PEDAL2 },

	{ "P1_PADDLE",				IKT_IPT,		IPF_PLAYER1 | IPT_PADDLE },
	{ "P1_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER1 | IPT_PADDLE },
	{ "P2_PADDLE",				IKT_IPT,		IPF_PLAYER2 | IPT_PADDLE },
	{ "P2_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER2 | IPT_PADDLE },
	{ "P3_PADDLE",				IKT_IPT,		IPF_PLAYER3 | IPT_PADDLE },
	{ "P3_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER3 | IPT_PADDLE },
	{ "P4_PADDLE",				IKT_IPT,		IPF_PLAYER4 | IPT_PADDLE },
	{ "P4_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER4 | IPT_PADDLE },
	{ "P5_PADDLE",				IKT_IPT,		IPF_PLAYER5 | IPT_PADDLE },
	{ "P5_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER5 | IPT_PADDLE },
	{ "P6_PADDLE",				IKT_IPT,		IPF_PLAYER6 | IPT_PADDLE },
	{ "P6_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER6 | IPT_PADDLE },
	{ "P7_PADDLE",				IKT_IPT,		IPF_PLAYER7 | IPT_PADDLE },
	{ "P7_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER7 | IPT_PADDLE },
	{ "P8_PADDLE",				IKT_IPT,		IPF_PLAYER8 | IPT_PADDLE },
	{ "P8_PADDLE_EXT",			IKT_IPT_EXT,	IPF_PLAYER8 | IPT_PADDLE },

	{ "P1_PADDLE_V",			IKT_IPT,		IPF_PLAYER1 | IPT_PADDLE_V },
	{ "P1_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_PADDLE_V },
	{ "P2_PADDLE_V",			IKT_IPT,		IPF_PLAYER2 | IPT_PADDLE_V },
	{ "P2_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_PADDLE_V },
	{ "P3_PADDLE_V",			IKT_IPT,		IPF_PLAYER3 | IPT_PADDLE_V },
	{ "P3_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_PADDLE_V },
	{ "P4_PADDLE_V",			IKT_IPT,		IPF_PLAYER4 | IPT_PADDLE_V },
	{ "P4_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_PADDLE_V },
	{ "P5_PADDLE_V",			IKT_IPT,		IPF_PLAYER5 | IPT_PADDLE_V },
	{ "P5_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_PADDLE_V },
	{ "P6_PADDLE_V",			IKT_IPT,		IPF_PLAYER6 | IPT_PADDLE_V },
	{ "P6_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_PADDLE_V },
	{ "P7_PADDLE_V",			IKT_IPT,		IPF_PLAYER7 | IPT_PADDLE_V },
	{ "P7_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_PADDLE_V },
	{ "P8_PADDLE_V",			IKT_IPT,		IPF_PLAYER8 | IPT_PADDLE_V },
	{ "P8_PADDLE_V_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_PADDLE_V },

	{ "P1_DIAL",				IKT_IPT,		IPF_PLAYER1 | IPT_DIAL },
	{ "P1_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER1 | IPT_DIAL },
	{ "P2_DIAL",				IKT_IPT,		IPF_PLAYER2 | IPT_DIAL },
	{ "P2_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER2 | IPT_DIAL },
	{ "P3_DIAL",				IKT_IPT,		IPF_PLAYER3 | IPT_DIAL },
	{ "P3_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER3 | IPT_DIAL },
	{ "P4_DIAL",				IKT_IPT,		IPF_PLAYER4 | IPT_DIAL },
	{ "P4_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER4 | IPT_DIAL },
	{ "P5_DIAL",				IKT_IPT,		IPF_PLAYER5 | IPT_DIAL },
	{ "P5_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER5 | IPT_DIAL },
	{ "P6_DIAL",				IKT_IPT,		IPF_PLAYER6 | IPT_DIAL },
	{ "P6_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER6 | IPT_DIAL },
	{ "P7_DIAL",				IKT_IPT,		IPF_PLAYER7 | IPT_DIAL },
	{ "P7_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER7 | IPT_DIAL },
	{ "P8_DIAL",				IKT_IPT,		IPF_PLAYER8 | IPT_DIAL },
	{ "P8_DIAL_EXT",			IKT_IPT_EXT,	IPF_PLAYER8 | IPT_DIAL },

	{ "P1_DIAL_V",				IKT_IPT,		IPF_PLAYER1 | IPT_DIAL_V },
	{ "P1_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER1 | IPT_DIAL_V },
	{ "P2_DIAL_V",				IKT_IPT,		IPF_PLAYER2 | IPT_DIAL_V },
	{ "P2_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER2 | IPT_DIAL_V },
	{ "P3_DIAL_V",				IKT_IPT,		IPF_PLAYER3 | IPT_DIAL_V },
	{ "P3_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER3 | IPT_DIAL_V },
	{ "P4_DIAL_V",				IKT_IPT,		IPF_PLAYER4 | IPT_DIAL_V },
	{ "P4_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER4 | IPT_DIAL_V },
	{ "P5_DIAL_V",				IKT_IPT,		IPF_PLAYER5 | IPT_DIAL_V },
	{ "P5_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER5 | IPT_DIAL_V },
	{ "P6_DIAL_V",				IKT_IPT,		IPF_PLAYER6 | IPT_DIAL_V },
	{ "P6_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER6 | IPT_DIAL_V },
	{ "P7_DIAL_V",				IKT_IPT,		IPF_PLAYER7 | IPT_DIAL_V },
	{ "P7_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER7 | IPT_DIAL_V },
	{ "P8_DIAL_V",				IKT_IPT,		IPF_PLAYER8 | IPT_DIAL_V },
	{ "P8_DIAL_V_EXT",			IKT_IPT_EXT,	IPF_PLAYER8 | IPT_DIAL_V },

	{ "P1_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER1 | IPT_TRACKBALL_X },
	{ "P1_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_TRACKBALL_X },
	{ "P2_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER2 | IPT_TRACKBALL_X },
	{ "P2_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_TRACKBALL_X },
	{ "P3_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER3 | IPT_TRACKBALL_X },
	{ "P3_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_TRACKBALL_X },
	{ "P4_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER4 | IPT_TRACKBALL_X },
	{ "P4_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_TRACKBALL_X },
	{ "P5_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER5 | IPT_TRACKBALL_X },
	{ "P5_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_TRACKBALL_X },
	{ "P6_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER6 | IPT_TRACKBALL_X },
	{ "P6_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_TRACKBALL_X },
	{ "P7_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER7 | IPT_TRACKBALL_X },
	{ "P7_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_TRACKBALL_X },
	{ "P8_TRACKBALL_X",			IKT_IPT,		IPF_PLAYER8 | IPT_TRACKBALL_X },
	{ "P8_TRACKBALL_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_TRACKBALL_X },

	{ "P1_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER1 | IPT_TRACKBALL_Y },
	{ "P1_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_TRACKBALL_Y },
	{ "P2_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER2 | IPT_TRACKBALL_Y },
	{ "P2_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_TRACKBALL_Y },
	{ "P3_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER3 | IPT_TRACKBALL_Y },
	{ "P3_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_TRACKBALL_Y },
	{ "P4_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER4 | IPT_TRACKBALL_Y },
	{ "P4_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_TRACKBALL_Y },
	{ "P5_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER5 | IPT_TRACKBALL_Y },
	{ "P5_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_TRACKBALL_Y },
	{ "P6_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER6 | IPT_TRACKBALL_Y },
	{ "P6_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_TRACKBALL_Y },
	{ "P7_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER7 | IPT_TRACKBALL_Y },
	{ "P7_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_TRACKBALL_Y },
	{ "P8_TRACKBALL_Y",			IKT_IPT,		IPF_PLAYER8 | IPT_TRACKBALL_Y },
	{ "P8_TRACKBALL_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_TRACKBALL_Y },

	{ "P1_AD_STICK_X",			IKT_IPT,		IPF_PLAYER1 | IPT_AD_STICK_X },
	{ "P1_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_AD_STICK_X },
	{ "P2_AD_STICK_X",			IKT_IPT,		IPF_PLAYER2 | IPT_AD_STICK_X },
	{ "P2_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_AD_STICK_X },
	{ "P3_AD_STICK_X",			IKT_IPT,		IPF_PLAYER3 | IPT_AD_STICK_X },
	{ "P3_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_AD_STICK_X },
	{ "P4_AD_STICK_X",			IKT_IPT,		IPF_PLAYER4 | IPT_AD_STICK_X },
	{ "P4_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_AD_STICK_X },
	{ "P5_AD_STICK_X",			IKT_IPT,		IPF_PLAYER5 | IPT_AD_STICK_X },
	{ "P5_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_AD_STICK_X },
	{ "P6_AD_STICK_X",			IKT_IPT,		IPF_PLAYER6 | IPT_AD_STICK_X },
	{ "P6_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_AD_STICK_X },
	{ "P7_AD_STICK_X",			IKT_IPT,		IPF_PLAYER7 | IPT_AD_STICK_X },
	{ "P7_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_AD_STICK_X },
	{ "P8_AD_STICK_X",			IKT_IPT,		IPF_PLAYER8 | IPT_AD_STICK_X },
	{ "P8_AD_STICK_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_AD_STICK_X },

	{ "P1_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER1 | IPT_AD_STICK_Y },
	{ "P1_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_AD_STICK_Y },
	{ "P2_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER2 | IPT_AD_STICK_Y },
	{ "P2_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_AD_STICK_Y },
	{ "P3_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER3 | IPT_AD_STICK_Y },
	{ "P3_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_AD_STICK_Y },
	{ "P4_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER4 | IPT_AD_STICK_Y },
	{ "P4_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_AD_STICK_Y },
	{ "P5_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER5 | IPT_AD_STICK_Y },
	{ "P5_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_AD_STICK_Y },
	{ "P6_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER6 | IPT_AD_STICK_Y },
	{ "P6_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_AD_STICK_Y },
	{ "P7_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER7 | IPT_AD_STICK_Y },
	{ "P7_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_AD_STICK_Y },
	{ "P8_AD_STICK_Y",			IKT_IPT,		IPF_PLAYER8 | IPT_AD_STICK_Y },
	{ "P8_AD_STICK_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_AD_STICK_Y },

	{ "P1_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER1 | IPT_LIGHTGUN_X },
	{ "P1_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_LIGHTGUN_X },
	{ "P2_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER2 | IPT_LIGHTGUN_X },
	{ "P2_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_LIGHTGUN_X },
	{ "P3_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER3 | IPT_LIGHTGUN_X },
	{ "P3_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_LIGHTGUN_X },
	{ "P4_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER4 | IPT_LIGHTGUN_X },
	{ "P4_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_LIGHTGUN_X },
	{ "P5_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER5 | IPT_LIGHTGUN_X },
	{ "P5_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_LIGHTGUN_X },
	{ "P6_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER6 | IPT_LIGHTGUN_X },
	{ "P6_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_LIGHTGUN_X },
	{ "P7_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER7 | IPT_LIGHTGUN_X },
	{ "P7_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_LIGHTGUN_X },
	{ "P8_LIGHTGUN_X",			IKT_IPT,		IPF_PLAYER8 | IPT_LIGHTGUN_X },
	{ "P8_LIGHTGUN_X_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_LIGHTGUN_X },

	{ "P1_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER1 | IPT_LIGHTGUN_Y },
	{ "P1_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_LIGHTGUN_Y },
	{ "P2_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER2 | IPT_LIGHTGUN_Y },
	{ "P2_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_LIGHTGUN_Y },
	{ "P3_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER3 | IPT_LIGHTGUN_Y },
	{ "P3_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_LIGHTGUN_Y },
	{ "P4_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER4 | IPT_LIGHTGUN_Y },
	{ "P4_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_LIGHTGUN_Y },
	{ "P5_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER5 | IPT_LIGHTGUN_Y },
	{ "P5_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_LIGHTGUN_Y },
	{ "P6_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER6 | IPT_LIGHTGUN_Y },
	{ "P6_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_LIGHTGUN_Y },
	{ "P7_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER7 | IPT_LIGHTGUN_Y },
	{ "P7_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_LIGHTGUN_Y },
	{ "P8_LIGHTGUN_Y",			IKT_IPT,		IPF_PLAYER8 | IPT_LIGHTGUN_Y },
	{ "P8_LIGHTGUN_Y_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_LIGHTGUN_Y },

	{ "P1_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER1 | IPT_AD_STICK_Z },
	{ "P1_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER1 | IPT_AD_STICK_Z },
	{ "P2_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER2 | IPT_AD_STICK_Z },
	{ "P2_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER2 | IPT_AD_STICK_Z },
	{ "P3_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER3 | IPT_AD_STICK_Z },
	{ "P3_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER3 | IPT_AD_STICK_Z },
	{ "P4_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER4 | IPT_AD_STICK_Z },
	{ "P4_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER4 | IPT_AD_STICK_Z },
	{ "P5_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER5 | IPT_AD_STICK_Z },
	{ "P5_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER5 | IPT_AD_STICK_Z },
	{ "P6_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER6 | IPT_AD_STICK_Z },
	{ "P6_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER6 | IPT_AD_STICK_Z },
	{ "P7_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER7 | IPT_AD_STICK_Z },
	{ "P7_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER7 | IPT_AD_STICK_Z },
	{ "P8_AD_STICK_Z",			IKT_IPT,		IPF_PLAYER8 | IPT_AD_STICK_Z },
	{ "P8_AD_STICK_Z_EXT",		IKT_IPT_EXT,	IPF_PLAYER8 | IPT_AD_STICK_Z },
	{ "OSD_1",					IKT_IPT,		IPT_OSD_1 },
	{ "OSD_2",					IKT_IPT,		IPT_OSD_2 },
	{ "OSD_3",					IKT_IPT,		IPT_OSD_3 },
	{ "OSD_4",					IKT_IPT,		IPT_OSD_4 },

	{ "UNKNOWN",				IKT_IPT,		IPT_UNKNOWN },
	{ "END",					IKT_IPT,		IPT_END },

	{ "",						0,	0 }
};

int num_ik = sizeof(input_keywords)/sizeof(struct ik);

/***************************************************************************/


const char *generic_ctrl_label(int input)
{
  unsigned int i = 0;

  for( ; inputport_defaults[i].type != IPT_END; ++i)
  {
    struct ipd *entry = &inputport_defaults[i];
    if(entry->type == input)
    {
      if(input >= IPT_SERVICE1 && input <= IPT_UI_EDIT_CHEAT)
        return entry->name; /* these strings are not player-specific */
      else /* start with the third character, trimming the initial 'P1', 'P2', etc which we don't need for libretro */
        return &entry->name[3];
    }
  }
  return NULL;
}


/***************************************************************************/
/* Generic IO */

static int readint(mame_file *f,UINT32 *num)
{
	unsigned i;

	*num = 0;
	for (i = 0;i < sizeof(UINT32);i++)
	{
		unsigned char c;


		*num <<= 8;
		if (mame_fread(f,&c,1) != 1)
			return -1;
		*num |= c;
	}

	return 0;
}

static void writeint(mame_file *f,UINT32 num)
{
	unsigned i;

	for (i = 0;i < sizeof(UINT32);i++)
	{
		unsigned char c;


		c = (num >> 8 * (sizeof(UINT32)-1)) & 0xff;
		mame_fwrite(f,&c,1);
		num <<= 8;
	}
}

static int readword(mame_file *f,UINT16 *num)
{
	unsigned i;
	int res;

	res = 0;
	for (i = 0;i < sizeof(UINT16);i++)
	{
		unsigned char c;


		res <<= 8;
		if (mame_fread(f,&c,1) != 1)
			return -1;
		res |= c;
	}

	*num = res;
	return 0;
}

static void writeword(mame_file *f,UINT16 num)
{
	unsigned i;

	for (i = 0;i < sizeof(UINT16);i++)
	{
		unsigned char c;


		c = (num >> 8 * (sizeof(UINT16)-1)) & 0xff;
		mame_fwrite(f,&c,1);
		num <<= 8;
	}
}



/***************************************************************************/
/* Load */

static void load_default_keys(void)
{
	config_file *cfg;

	memcpy(inputport_defaults_backup,inputport_defaults,sizeof(inputport_defaults));

	cfg = config_open(NULL);
	if (cfg)
	{
		config_read_default_ports(cfg, inputport_defaults);
		config_close(cfg);
	}

	osd_customize_inputport_defaults(inputport_defaults);
}

static void save_default_keys(void)
{
	config_file *cfg;

	cfg = config_create(NULL);
	if (cfg)
	{
		config_write_default_ports(cfg, inputport_defaults_backup, inputport_defaults);
		config_close(cfg);
	}

	memcpy(inputport_defaults,inputport_defaults_backup,sizeof(inputport_defaults_backup));
}


int load_input_port_settings(void)
{
	config_file *cfg;
	int err;
	struct mixer_config mixercfg;
	char buf[20];

	if(options.mame_remapping)
		sprintf(buf,"%s",Machine->gamedrv->name);
	else
		sprintf(buf,"ra_%s",Machine->gamedrv->name);

	load_default_keys();

	cfg = config_open(buf);
	if (cfg)
		{
		err = config_read_ports(cfg, Machine->input_ports_default, Machine->input_ports);
		if (err)
				goto getout;

		err = config_read_coin_and_ticket_counters(cfg, coins, lastcoin, coinlockedout, &dispensed_tickets);
		if (err)
				goto getout;

		err = config_read_mixer_config(cfg, &mixercfg);
		if (err)
			goto getout;

		mixer_load_config(&mixercfg);

getout:
		config_close(cfg);
	}

	/* All analog ports need initialization */
	{
		int i;
		for (i = 0; i < MAX_INPUT_PORTS; i++)
			input_analog_init[i] = 1;
	}

	init_analog_seq();

	update_input_ports();

	/* if we didn't find a saved config, return 0 so the main core knows that it */
	/* is the first time the game is run and it should diplay the disclaimer. */
	return cfg ? 1 : 0;
}

/***************************************************************************/
/* Save */

void save_input_port_settings(void)
{
	if (legacy_flag)
	{
		config_file *cfg;
		struct mixer_config mixercfg;
		char buf[20];
		if(options.mame_remapping)
			sprintf(buf,"%s",Machine->gamedrv->name);
		else
			sprintf(buf,"ra_%s",Machine->gamedrv->name);

		save_default_keys();

		cfg = config_create(buf);
		if (cfg)
		{
			mixer_save_config(&mixercfg);

			config_write_ports(cfg, Machine->input_ports_default, Machine->input_ports);
			config_write_coin_and_ticket_counters(cfg, coins, lastcoin, coinlockedout, dispensed_tickets);
			config_write_mixer_config(cfg, &mixercfg);
			config_close(cfg);
		}
	}
}


/* Note that the following 3 routines have slightly different meanings with analog ports */
const char *input_port_name(const struct InputPort *in)
{
	int i;
	unsigned type;

	if (in->name != IP_NAME_DEFAULT) return in->name;

	i = 0;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
		type = (in-1)->type & (~IPF_MASK | IPF_PLAYERMASK);
	else
		type = in->type & (~IPF_MASK | IPF_PLAYERMASK);

	while (inputport_defaults[i].type != IPT_END &&
			inputport_defaults[i].type != type)
		i++;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
		return inputport_defaults[i+1].name;
	else
		return inputport_defaults[i].name;
}

InputSeq* input_port_type_seq(int type)
{
	unsigned i;

	i = 0;

	while (inputport_defaults[i].type != IPT_END &&
			inputport_defaults[i].type != type)
		i++;

	return &inputport_defaults[i].seq;
}

InputSeq* input_port_seq(const struct InputPort *in)
{
	int i,type;

	static InputSeq ip_none = SEQ_DEF_1(CODE_NONE);

	while (seq_get_1((InputSeq*)&in->seq) == CODE_PREVIOUS) in--;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
	{
		type = (in-1)->type & (~IPF_MASK | IPF_PLAYERMASK);
		/* if port is disabled, or cheat with cheats disabled, return no key */
		if (((in-1)->type & IPF_UNUSED) || (!options.cheat_input_ports && ((in-1)->type & IPF_CHEAT)))
			return &ip_none;
	}
	else
	{
		type = in->type & (~IPF_MASK | IPF_PLAYERMASK);
		/* if port is disabled, or cheat with cheats disabled, return no key */
		if ((in->type & IPF_UNUSED) || (!options.cheat_input_ports && (in->type & IPF_CHEAT)))
			return &ip_none;
	}

	if (seq_get_1((InputSeq*)&in->seq) != CODE_DEFAULT)
		return (InputSeq*)&in->seq;

	i = 0;

	while (inputport_defaults[i].type != IPT_END &&
			inputport_defaults[i].type != type)
		i++;

	if ((in->type & ~IPF_MASK) == IPT_EXTENSION)
		return &inputport_defaults[i+1].seq;
	else
		return &inputport_defaults[i].seq;
}

void update_analog_port(int port)
{
	struct InputPort *in;
	int current, delta, type, sensitivity, min, max, default_value;
	int axis, is_stick, is_gun, check_bounds;
	InputSeq* incseq;
	InputSeq* decseq;
	int keydelta;
	int player;

	/* get input definition */
	in = input_analog[port];

	/* if we're not cheating and this is a cheat-only port, bail */
	if (!options.cheat_input_ports && (in->type & IPF_CHEAT)) return;
	type=(in->type & ~IPF_MASK);

	decseq = input_port_seq(in);
	incseq = input_port_seq(in+1);

	keydelta = IP_GET_DELTA(in);

	switch (type)
	{
		case IPT_PADDLE:
			axis = X_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		case IPT_PADDLE_V:
			axis = Y_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		case IPT_DIAL:
			axis = X_AXIS; is_stick = 0; is_gun=0; check_bounds = 0; break;
		case IPT_DIAL_V:
			axis = Y_AXIS; is_stick = 0; is_gun=0; check_bounds = 0; break;
		case IPT_TRACKBALL_X:
			axis = X_AXIS; is_stick = 0; is_gun=0; check_bounds = 0; break;
		case IPT_TRACKBALL_Y:
			axis = Y_AXIS; is_stick = 0; is_gun=0; check_bounds = 0; break;
		case IPT_AD_STICK_X:
			axis = X_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		case IPT_AD_STICK_Y:
			axis = Y_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		case IPT_AD_STICK_Z:
			axis = Z_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		case IPT_LIGHTGUN_X:
			axis = X_AXIS; is_stick = 1; is_gun=1; check_bounds = 1; break;
		case IPT_LIGHTGUN_Y:
			axis = Y_AXIS; is_stick = 1; is_gun=1; check_bounds = 1; break;
		case IPT_PEDAL:
			axis = PEDAL_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		case IPT_PEDAL2:
			axis = Z_AXIS; is_stick = 1; is_gun=0; check_bounds = 1; break;
		default:
			/* Use some defaults to prevent crash */
			axis = X_AXIS; is_stick = 0; is_gun=0; check_bounds = 0;
			log_cb(RETRO_LOG_ERROR, LOGPRE "Oops, polling non analog device in update_analog_port()????\n");
	}


	sensitivity = IP_GET_SENSITIVITY(in);
	min = IP_GET_MIN(in);
	max = IP_GET_MAX(in);
	default_value = in->default_value * 100 / sensitivity;
	/* extremes can be either signed or unsigned */
	if (min > max)
	{
		if (in->mask > 0xff) min = min - 0x10000;
		else min = min - 0x100;
	}

	input_analog_previous_value[port] = input_analog_current_value[port];

	/* if IPF_CENTER go back to the default position */
	/* sticks are handled later... */
	if ((in->type & IPF_CENTER) && (!is_stick))
		input_analog_current_value[port] = in->default_value * 100 / sensitivity;

	current = input_analog_current_value[port];

	delta = 0;

	player = IP_GET_PLAYER(in);

    /* if second player on a dial, and dial sharing turned on, use Y axis from player 1 */
    if (options.dial_share_xy && type == IPT_DIAL && player == 1)
    {
        axis = Y_AXIS;
        player = 0;
    }

	delta = mouse_delta_axis[player][axis];

	if (seq_pressed(decseq)) delta -= keydelta;

	if (type != IPT_PEDAL && type != IPT_PEDAL2)
	{
		if (seq_pressed(incseq)) delta += keydelta;
	}
	else
	{
		/* is this cheesy or what? */
		if (!delta && seq_get_1(incseq) == KEYCODE_Y) delta += keydelta;
		delta = -delta;
	}

	if (in->type & IPF_REVERSE) delta = -delta;

	if (is_gun)
	{
		/* The OSD lightgun call should return the delta from the middle of the screen
		when the gun is fired (not the absolute pixel value), and 0 when the gun is
		inactive.  We take advantage of this to provide support for other controllers
		in place of a physical lightgun.  When the OSD lightgun returns 0, then control
		passes through to the analog joystick, and mouse, in that order.  When the OSD
		lightgun returns a value it overrides both mouse & analog joystick.

		The value returned by the OSD layer should be -128 to 128, same as analog
		joysticks.

		There is an ugly hack to stop scaling of lightgun returned values.  It really
		needs rewritten...
		*/
		if (axis == X_AXIS) {
			if (lightgun_delta_axis[player][X_AXIS] || lightgun_delta_axis[player][Y_AXIS]) {
				analog_previous_axis[player][X_AXIS]=0;
				analog_current_axis[player][X_AXIS]=lightgun_delta_axis[player][X_AXIS];
				input_analog_scale[port]=0;
				sensitivity=100;
			}
		}
		else
		{
			if (lightgun_delta_axis[player][X_AXIS] || lightgun_delta_axis[player][Y_AXIS]) {
				analog_previous_axis[player][Y_AXIS]=0;
				analog_current_axis[player][Y_AXIS]=lightgun_delta_axis[player][Y_AXIS];
				input_analog_scale[port]=0;
				sensitivity=100;
			}
		}
	}

	if (is_stick)
	{
		int new, prev;

		/* center stick - core option to center digital joysticks */
		if (delta == 0 && options.digital_joy_centering && (type == IPT_AD_STICK_X || type == IPT_AD_STICK_Y))
			current = default_value;

		else if ((delta == 0) && (in->type & IPF_CENTER))
		{
			if (current > default_value)
			delta = -100 / sensitivity;
			if (current < default_value)
			delta = 100 / sensitivity;
		}

		/* An analog joystick which is not at zero position (or has just */
		/* moved there) takes precedence over all other computations */
		/* analog_x/y holds values from -128 to 128 (yes, 128, not 127) */

		new  = analog_current_axis[player][axis];
		prev = analog_previous_axis[player][axis];

		if ((new != 0) || (new-prev != 0))
		{
			delta=0;

			/* for pedals, need to change to possitive number */
			/* and, if needed, reverse pedal input */
			if (type == IPT_PEDAL || type == IPT_PEDAL2)
			{
				new  = -new;
				prev = -prev;
				if (in->type & IPF_REVERSE)		/* a reversed pedal is diff than normal reverse */
				{								/* 128 = no gas, 0 = all gas */
					new  = 128-new;				/* the default "new=-new" doesn't handle this */
					prev = 128-prev;
				}
			}
			else if (in->type & IPF_REVERSE)
			{
				new  = -new;
				prev = -prev;
			}

			/* apply sensitivity using a logarithmic scale */
			if (in->mask > 0xff)
			{
				if (new > 0)
				{
					current = (pow(new / 32768.0, 100.0 / sensitivity) * (max-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
				else
				{
					current = (pow(-new / 32768.0, 100.0 / sensitivity) * (min-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
			}
			else
			{
				if (new > 0)
				{
					current = (pow(new / 128.0, 100.0 / sensitivity) * (max-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
				else
				{
					current = (pow(-new / 128.0, 100.0 / sensitivity) * (min-in->default_value)
							+ in->default_value) * 100 / sensitivity;
				}
			}
		}
	}

	current += delta;

	if (check_bounds)
	{
		int temp;

		if (current >= 0)
			temp = (current * sensitivity + 50) / 100;
		else
			temp = (-current * sensitivity + 50) / -100;

		if (temp < min)
		{
			if (min >= 0)
				current = (min * 100 + sensitivity/2) / sensitivity;
			else
				current = (-min * 100 + sensitivity/2) / -sensitivity;
		}
		if (temp > max)
		{
			if (max >= 0)
				current = (max * 100 + sensitivity/2) / sensitivity;
			else
				current = (-max * 100 + sensitivity/2) / -sensitivity;
		}
	}

	input_analog_current_value[port] = current;
}

static void scale_analog_port(int port)
{
	struct InputPort *in;
	int delta,current,sensitivity;

profiler_mark(PROFILER_INPUT);
	in = input_analog[port];
	sensitivity = IP_GET_SENSITIVITY(in);

	/* apply scaling fairly in both positive and negative directions */
	delta = input_analog_current_value[port] - input_analog_previous_value[port];
	if (delta >= 0)
		delta = cpu_scalebyfcount(delta);
	else
		delta = -cpu_scalebyfcount(-delta);

	current = input_analog_previous_value[port] + delta;

	/* An ugly hack to remove scaling on lightgun ports */
	if (input_analog_scale[port]) {
		/* apply scaling fairly in both positive and negative directions */
		if (current >= 0)
			current = (current * sensitivity + 50) / 100;
		else
			current = (-current * sensitivity + 50) / -100;
	}

	input_port_value[port] &= ~in->mask;
	input_port_value[port] |= current & in->mask;

	if (playback)
		readword(playback,&input_port_value[port]);
	if (record)
		writeword(record,input_port_value[port]);
	profiler_mark(PROFILER_END);
}

#define MAX_JOYSTICKS 3
#define MAX_PLAYERS 8
static int mJoyCurrent[MAX_JOYSTICKS*MAX_PLAYERS];
static int mJoyPrevious[MAX_JOYSTICKS*MAX_PLAYERS];
static int mJoy4Way[MAX_JOYSTICKS*MAX_PLAYERS];
/*
The above "Joy" states contain packed bits:
	0001	up
	0010	down
	0100	left
	1000	right
*/

static void
ScanJoysticks( struct InputPort *in )
{
	int i;
	int port = 0;

	/* Save old Joystick state. */
	memcpy( mJoyPrevious, mJoyCurrent, sizeof(mJoyPrevious) );

	/* Initialize bits of mJoyCurrent to zero. */
	memset( mJoyCurrent, 0, sizeof(mJoyCurrent) );

	/* Now iterate over the input port structure to populate mJoyCurrent. */
	while( in->type != IPT_END && port < MAX_INPUT_PORTS )
	{
		while (in->type != IPT_END && in->type != IPT_PORT)
		{
			if ((in->type & ~IPF_MASK) >= IPT_JOYSTICK_UP &&
				(in->type & ~IPF_MASK) <= IPT_JOYSTICKLEFT_RIGHT)
			{
				InputSeq* seq;
				seq = input_port_seq(in);
				if( seq_pressed(seq) )
				{
					int joynum,joydir,player;
					player = IP_GET_PLAYER(in);

					joynum = player * MAX_JOYSTICKS +
							 ((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) / 4;
					joydir = ((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) % 4;

					mJoyCurrent[joynum] |= 1<<joydir;
				}
			}
			in++;
		}
		port++;
		if (in->type == IPT_PORT) in++;
	}

	/* Process the joystick states, to filter out illegal combinations of switches. */
	for( i=0; i<MAX_JOYSTICKS*MAX_PLAYERS; i++ )
	{
		if( (mJoyCurrent[i]&0x3)==0x3 ) /* both up and down are pressed */
		{
			mJoyCurrent[i]&=0xc; /* clear up and down */
		}
		if( (mJoyCurrent[i]&0xc)==0xc ) /* both left and right are pressed */
		{
			mJoyCurrent[i]&=0x3; /* clear left and right */
		}

		/* Only update mJoy4Way if the joystick has moved. */
		if( mJoyCurrent[i]!=mJoyPrevious[i] && !options.restrict_4_way)
		{
			mJoy4Way[i] = mJoyCurrent[i];

			  if( (mJoy4Way[i] & 0x3) && (mJoy4Way[i] & 0xc) )
			  {
			  	  /* If joystick is pointing at a diagonal, acknowledge that the player moved
				   * the joystick by favoring a direction change.  This minimizes frustration
				   * when using a keyboard for input, and maximizes responsiveness.
				   *
				   * For example, if you are holding "left" then switch to "up" (where both left
				   * and up are briefly pressed at the same time), we'll transition immediately
				   * to "up."
				   *
				   * Under the old "sticky" key implentation, "up" wouldn't be triggered until
				   * left was released.
				   *
				   * Zero any switches that didn't change from the previous to current state.
				   */
				  mJoy4Way[i] ^= (mJoy4Way[i] & mJoyPrevious[i]);
			  }

			  if( (mJoy4Way[i] & 0x3) && (mJoy4Way[i] & 0xc) )
			  {
			  	  /* If we are still pointing at a diagonal, we are in an indeterminant state.
				   *
				   * This could happen if the player moved the joystick from the idle position directly
				   * to a diagonal, or from one diagonal directly to an extreme diagonal.
				   *
				   * The chances of this happening with a keyboard are slim, but we still need to
				   * constrain this case.
				   *
				   * For now, just resolve randomly.
				   */
				  if( rand()&1 )
				  {
				  	  mJoy4Way[i] &= 0x3; /* eliminate horizontal component */
				  }
				  else
				  {
				 	  mJoy4Way[i] &= 0xc; /* eliminate vertical component */
				  }
			  }

		}
    else if (options.restrict_4_way) //start use alternative code
    {
      if(options.content_flags[CONTENT_ROTATE_JOY_45])
      {
        if  ( (mJoyCurrent[i]) && (mJoyCurrent[i] !=1) &&
              (mJoyCurrent[i] !=2) && (mJoyCurrent[i] !=4) &&
              (mJoyCurrent[i] !=8) )
        {
          if      (mJoyCurrent[i] == 9)  mJoy4Way[i]=1;
          else if (mJoyCurrent[i] == 6)  mJoy4Way[i]=2;
          else if (mJoyCurrent[i] == 5)  mJoy4Way[i]=4;
          else if (mJoyCurrent[i] == 10) mJoy4Way[i]=8;
        }
        else if (mJoy4Way[i])
          mJoy4Way[i]=0;
      }
      else // just a regular 4-way - last press no code needed just ignore diagonals and no movement
      {
        if  ( (mJoyCurrent[i]) && (mJoyCurrent[i] !=5) && (mJoyCurrent[i] !=6)
          &&  (mJoyCurrent[i] !=9) && (mJoyCurrent[i] !=10) )
        {
          mJoy4Way[i] = mJoyCurrent[i];
        }
      }
		}
	}
} /* ScanJoysticks */

void update_input_ports(void)
{
	int port,ib;
	struct InputPort *in;

#define MAX_INPUT_BITS 1024
	static int impulsecount[MAX_INPUT_BITS];
	static int waspressed[MAX_INPUT_BITS];
	static int pbwaspressed[MAX_INPUT_BITS];

profiler_mark(PROFILER_INPUT);

	/* clear all the values before proceeding */
	for (port = 0;port < MAX_INPUT_PORTS;port++)
	{
		input_port_value[port] = 0;
		input_vblank[port] = 0;
		input_analog[port] = 0;
	}

	in = Machine->input_ports;
	if (in->type == IPT_END) return; 	/* nothing to do */

	/* make sure the InputPort definition is correct */
	if (in->type != IPT_PORT)
	{
		log_cb(RETRO_LOG_ERROR, LOGPRE "Error in InputPort definition: expecting PORT_START\n");
		return;
	}
	else
	{
		in++;
	}

	ScanJoysticks( in ); /* populates mJoyCurrent[] */

	/* scan all the input ports */
	port = 0;
	ib = 0;
	while (in->type != IPT_END && port < MAX_INPUT_PORTS)
	{
		struct InputPort *start;
		/* first of all, scan the whole input port definition and build the */
		/* default value. I must do it before checking for input because otherwise */
		/* multiple keys associated with the same input bit wouldn't work (the bit */
		/* would be reset to its default value by the second entry, regardless if */
		/* the key associated with the first entry was pressed) */
		start = in;
		while (in->type != IPT_END && in->type != IPT_PORT)
		{
			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING &&	/* skip dipswitch definitions */
				(in->type & ~IPF_MASK) != IPT_EXTENSION)			/* skip analog extension fields */
			{
				input_port_value[port] =
						(input_port_value[port] & ~in->mask) | (in->default_value & in->mask);
			}

			in++;
		}

		/* now get back to the beginning of the input port and check the input bits. */
		for (in = start;
			 in->type != IPT_END && in->type != IPT_PORT;
			 in++, ib++)
		{
			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING &&	/* skip dipswitch definitions */
					(in->type & ~IPF_MASK) != IPT_EXTENSION)		/* skip analog extension fields */
			{
				if ((in->type & ~IPF_MASK) == IPT_VBLANK)
				{
					input_vblank[port] ^= in->mask;
					input_port_value[port] ^= in->mask;
					if (Machine->drv->vblank_duration == 0)
						log_cb(RETRO_LOG_ERROR, LOGPRE "Warning: you are using IPT_VBLANK with vblank_duration = 0. You need to increase vblank_duration for IPT_VBLANK to work.\n");
				}
				/* If it's an analog control, handle it appropriately */
				else if (((in->type & ~IPF_MASK) > IPT_ANALOG_START)
					  && ((in->type & ~IPF_MASK) < IPT_ANALOG_END  )) /* LBO 120897 */
				{
					input_analog[port]=in;
					/* reset the analog port on first access */
					if (input_analog_init[port])
					{
						input_analog_init[port] = 0;
						input_analog_scale[port] = 1;
						input_analog_current_value[port] = input_analog_previous_value[port]
							= in->default_value * 100 / IP_GET_SENSITIVITY(in);
					}
				}
				else
				{
					InputSeq* seq;
					seq = input_port_seq(in);
					if (seq_pressed(seq))
					{
						/* skip if coin input and it's locked out */
						if ((in->type & ~IPF_MASK) >= IPT_COIN1 &&
							(in->type & ~IPF_MASK) <= IPT_COIN4 &&
							coinlockedout[(in->type & ~IPF_MASK) - IPT_COIN1])
						{
							continue;
						}
						if ((in->type & ~IPF_MASK) >= IPT_COIN5 &&
							(in->type & ~IPF_MASK) <= IPT_COIN8 &&
							coinlockedout[(in->type & ~IPF_MASK) - IPT_COIN5 + 4])
						{
							continue;
						}

						/* if IPF_RESET set, reset the first CPU */
						if ((in->type & IPF_RESETCPU) && waspressed[ib] == 0 && !playback)
						{
							cpu_set_reset_line(0,PULSE_LINE);
						}

						if (in->type & IPF_IMPULSE)
						{
							if (IP_GET_IMPULSE(in) == 0)
								log_cb(RETRO_LOG_ERROR, LOGPRE "error in input port definition: IPF_IMPULSE with length = 0\n");
							if (waspressed[ib] == 0)
								impulsecount[ib] = IP_GET_IMPULSE(in);
								/* the input bit will be toggled later */
						}
						else if (in->type & IPF_TOGGLE)
						{
							if (waspressed[ib] == 0)
							{
								in->default_value ^= in->mask;
								input_port_value[port] ^= in->mask;
							}
						}
						else if ((in->type & ~IPF_MASK) >= IPT_JOYSTICK_UP &&
								(in->type & ~IPF_MASK) <= IPT_JOYSTICKLEFT_RIGHT)
						{
							int joynum,joydir,mask,player;

							player = IP_GET_PLAYER(in);
							joynum = player * MAX_JOYSTICKS +
									((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) / 4;

							joydir = ((in->type & ~IPF_MASK) - IPT_JOYSTICK_UP) % 4;

							mask = in->mask;

							if( in->type & IPF_4WAY )
							{
								/* apply 4-way joystick constraint */
								if( ((mJoy4Way[joynum]>>joydir)&1) == 0 )
								{
									mask = 0;
								}
							}
							else
							{
								/* filter up+down and left+right */
								if( ((mJoyCurrent[joynum]>>joydir)&1) == 0 )
								{
									mask = 0;
								}
							}

							input_port_value[port] ^= mask;
						} /* joystick */
						else
						{
							input_port_value[port] ^= in->mask;
						}
						waspressed[ib] = 1;
					}
					else
						waspressed[ib] = 0;

					if ((in->type & IPF_IMPULSE) && impulsecount[ib] > 0)
					{
						impulsecount[ib]--;
						waspressed[ib] = 1;
						input_port_value[port] ^= in->mask;
					}
				}
			}
		}

		port++;
		if (in->type == IPT_PORT) in++;
	}

	if (playback)
	{
		int i;

		ib=0;
		in = Machine->input_ports;
		in++;
		for (i = 0; i < MAX_INPUT_PORTS; i ++)
		{
			readword(playback,&input_port_value[i]);

			/* check if the input port includes an IPF_RESETCPU bit
			   and reset the CPU on first "press", no need to check
			   the impulse count as this was done during recording */
			for (; in->type != IPT_END && in->type != IPT_PORT; in++, ib++)
			{
				if (in->type & IPF_RESETCPU)
				{
					if((input_port_value[i] ^ in->default_value) & in->mask)
					{
						if (pbwaspressed[ib] == 0)
							cpu_set_reset_line(0,PULSE_LINE);
						pbwaspressed[ib] = 1;
					}
					else
						pbwaspressed[ib] = 0;
				}
			}
			if (in->type == IPT_PORT) in++;
		}
	}

	if (record)
	{
		int i;

		for (i = 0; i < MAX_INPUT_PORTS; i ++)
			writeword(record,input_port_value[i]);
	}

	profiler_mark(PROFILER_END);
}



/* used the the CPU interface to notify that VBlank has ended, so we can update */
/* IPT_VBLANK input ports. */
void inputport_vblank_end(void)
{
	int port;
	int i;


profiler_mark(PROFILER_INPUT);
	for (port = 0;port < MAX_INPUT_PORTS;port++)
	{
		if (input_vblank[port])
		{
			input_port_value[port] ^= input_vblank[port];
			input_vblank[port] = 0;
		}
	}

	/* update the analog devices */
	for (i = 0;i < MAX_PLAYER_COUNT;i++)
	{
		/* update the analog joystick position */
		int a;
		for (a=0; a<MAX_ANALOG_AXES ; a++)
		{
			analog_previous_axis[i][a] = analog_current_axis[i][a];
		}
		osd_analogjoy_read (i, analog_current_axis[i], analogjoy_input[i]);

		/* update mouse/trackball position */
		osd_trak_read (i, &(mouse_delta_axis[i])[X_AXIS], &(mouse_delta_axis[i])[Y_AXIS]);

		/* update lightgun position, if any */
 		osd_lightgun_read (i, &(lightgun_delta_axis[i])[X_AXIS], &(lightgun_delta_axis[i])[Y_AXIS]);
	}

	for (i = 0;i < MAX_INPUT_PORTS;i++)
	{
		struct InputPort *in;

		in=input_analog[i];
		if (in)
		{
			update_analog_port(i);
		}
	}
profiler_mark(PROFILER_END);
}



int readinputport(int port)
{
	struct InputPort *in;

	/* Update analog ports on demand */
	in=input_analog[port];
	if (in)
	{
		scale_analog_port(port);
	}

	return input_port_value[port];
}

READ_HANDLER( input_port_0_r ) { return readinputport(0); }
READ_HANDLER( input_port_1_r ) { return readinputport(1); }
READ_HANDLER( input_port_2_r ) { return readinputport(2); }
READ_HANDLER( input_port_3_r ) { return readinputport(3); }
READ_HANDLER( input_port_4_r ) { return readinputport(4); }
READ_HANDLER( input_port_5_r ) { return readinputport(5); }
READ_HANDLER( input_port_6_r ) { return readinputport(6); }
READ_HANDLER( input_port_7_r ) { return readinputport(7); }
READ_HANDLER( input_port_8_r ) { return readinputport(8); }
READ_HANDLER( input_port_9_r ) { return readinputport(9); }
READ_HANDLER( input_port_10_r ) { return readinputport(10); }
READ_HANDLER( input_port_11_r ) { return readinputport(11); }
READ_HANDLER( input_port_12_r ) { return readinputport(12); }
READ_HANDLER( input_port_13_r ) { return readinputport(13); }
READ_HANDLER( input_port_14_r ) { return readinputport(14); }
READ_HANDLER( input_port_15_r ) { return readinputport(15); }
READ_HANDLER( input_port_16_r ) { return readinputport(16); }
READ_HANDLER( input_port_17_r ) { return readinputport(17); }
READ_HANDLER( input_port_18_r ) { return readinputport(18); }
READ_HANDLER( input_port_19_r ) { return readinputport(19); }
READ_HANDLER( input_port_20_r ) { return readinputport(20); }
READ_HANDLER( input_port_21_r ) { return readinputport(21); }
READ_HANDLER( input_port_22_r ) { return readinputport(22); }
READ_HANDLER( input_port_23_r ) { return readinputport(23); }
READ_HANDLER( input_port_24_r ) { return readinputport(24); }
READ_HANDLER( input_port_25_r ) { return readinputport(25); }
READ_HANDLER( input_port_26_r ) { return readinputport(26); }
READ_HANDLER( input_port_27_r ) { return readinputport(27); }
READ_HANDLER( input_port_28_r ) { return readinputport(28); }
READ_HANDLER( input_port_29_r ) { return readinputport(29); }

READ16_HANDLER( input_port_0_word_r ) { return readinputport(0); }
READ16_HANDLER( input_port_1_word_r ) { return readinputport(1); }
READ16_HANDLER( input_port_2_word_r ) { return readinputport(2); }
READ16_HANDLER( input_port_3_word_r ) { return readinputport(3); }
READ16_HANDLER( input_port_4_word_r ) { return readinputport(4); }
READ16_HANDLER( input_port_5_word_r ) { return readinputport(5); }
READ16_HANDLER( input_port_6_word_r ) { return readinputport(6); }
READ16_HANDLER( input_port_7_word_r ) { return readinputport(7); }
READ16_HANDLER( input_port_8_word_r ) { return readinputport(8); }
READ16_HANDLER( input_port_9_word_r ) { return readinputport(9); }
READ16_HANDLER( input_port_10_word_r ) { return readinputport(10); }
READ16_HANDLER( input_port_11_word_r ) { return readinputport(11); }
READ16_HANDLER( input_port_12_word_r ) { return readinputport(12); }
READ16_HANDLER( input_port_13_word_r ) { return readinputport(13); }
READ16_HANDLER( input_port_14_word_r ) { return readinputport(14); }
READ16_HANDLER( input_port_15_word_r ) { return readinputport(15); }
READ16_HANDLER( input_port_16_word_r ) { return readinputport(16); }
READ16_HANDLER( input_port_17_word_r ) { return readinputport(17); }
READ16_HANDLER( input_port_18_word_r ) { return readinputport(18); }
READ16_HANDLER( input_port_19_word_r ) { return readinputport(19); }
READ16_HANDLER( input_port_20_word_r ) { return readinputport(20); }
READ16_HANDLER( input_port_21_word_r ) { return readinputport(21); }
READ16_HANDLER( input_port_22_word_r ) { return readinputport(22); }
READ16_HANDLER( input_port_23_word_r ) { return readinputport(23); }
READ16_HANDLER( input_port_24_word_r ) { return readinputport(24); }
READ16_HANDLER( input_port_25_word_r ) { return readinputport(25); }
READ16_HANDLER( input_port_26_word_r ) { return readinputport(26); }
READ16_HANDLER( input_port_27_word_r ) { return readinputport(27); }
READ16_HANDLER( input_port_28_word_r ) { return readinputport(28); }
READ16_HANDLER( input_port_29_word_r ) { return readinputport(29); }


/***************************************************************************/
/* InputPort conversion */

static unsigned input_port_count(const struct InputPortTiny *src)
{
	unsigned total;

	total = 0;
	while (src->type != IPT_END)
	{
		int type = src->type & ~IPF_MASK;
		if (type > IPT_ANALOG_START && type < IPT_ANALOG_END)
			total += 2;
		else if (type != IPT_EXTENSION)
			++total;
		++src;
	}

	++total; /* for IPT_END */

	return total;
}

struct InputPort* input_port_allocate(const struct InputPortTiny *src)
{
	struct InputPort* dst;
	struct InputPort* base;
	unsigned total;

	total = input_port_count(src);

	base = (struct InputPort*)malloc(total * sizeof(struct InputPort));
	dst = base;

	while (src->type != IPT_END)
	{
		int type = src->type & ~IPF_MASK;
		const struct InputPortTiny *ext;
		const struct InputPortTiny *src_end;
		InputCode seq_default;

		if (type > IPT_ANALOG_START && type < IPT_ANALOG_END)
			src_end = src + 2;
		else
			src_end = src + 1;

		switch (type)
		{
			case IPT_END :
			case IPT_PORT :
			case IPT_DIPSWITCH_NAME :
			case IPT_DIPSWITCH_SETTING :
				seq_default = CODE_NONE;
			break;
			default:
				seq_default = CODE_DEFAULT;
				break;
		}

		ext = src_end;
		while (src != src_end)
		{
			dst->type = src->type;
			dst->mask = src->mask;
			dst->default_value = src->default_value;
			dst->name = src->name;

  			if (ext->type == IPT_EXTENSION)
  			{
				InputCode or1 =	IP_GET_CODE_OR1(ext);
				InputCode or2 =	IP_GET_CODE_OR2(ext);
				InputCode or3 = CODE_NONE;
				InputCode or4 = CODE_NONE;

				#define MATCH_ANALOG_JOYCODE(PLAYER_NUM) \
				case JOYCODE_##PLAYER_NUM##_BUTTON1:    or3 = JOYCODE_MOUSE_##PLAYER_NUM##_BUTTON1;      or4 = JOYCODE_GUN_##PLAYER_NUM##_BUTTON1;      break; \
				case JOYCODE_##PLAYER_NUM##_BUTTON2:    or3 = JOYCODE_MOUSE_##PLAYER_NUM##_BUTTON2;      or4 = JOYCODE_GUN_##PLAYER_NUM##_BUTTON2;      break; \
				case JOYCODE_##PLAYER_NUM##_BUTTON3:    or3 = JOYCODE_MOUSE_##PLAYER_NUM##_BUTTON3;      or4 = JOYCODE_GUN_##PLAYER_NUM##_BUTTON3;      break;

				switch(or2)
				{
					MATCH_ANALOG_JOYCODE(1)
					MATCH_ANALOG_JOYCODE(2)
					MATCH_ANALOG_JOYCODE(3)
					MATCH_ANALOG_JOYCODE(4)
					MATCH_ANALOG_JOYCODE(5)
					MATCH_ANALOG_JOYCODE(6)
					MATCH_ANALOG_JOYCODE(7)
					MATCH_ANALOG_JOYCODE(8)
					default:
						or3 = CODE_NONE;
						or4 = CODE_NONE;
						break;
				}

				if (or1 < __code_max)
				{
					if (or3 < __code_max)
						seq_set_7(&dst->seq, or1, CODE_OR, or2, CODE_OR, or3, CODE_OR, or4);
					else if (or2 < __code_max)
						seq_set_3(&dst->seq, or1, CODE_OR, or2);
					else
						seq_set_1(&dst->seq, or1);
				} else {
					if (or1 == CODE_NONE)
						seq_set_1(&dst->seq, or2);
					else
						seq_set_1(&dst->seq, or1);
				}

  				++ext;
  			} else {
				seq_set_1(&dst->seq,seq_default);
  			}

			++src;
			++dst;
		}

		src = ext;
	}

	dst->type = IPT_END;

	return base;
}

void input_port_free(struct InputPort* dst)
{
	free(dst);
}


void seq_set_string(InputSeq* a, const char *buf)
{
	char *lbuf;
	char *arg = NULL;
	int j;
	struct ik *pik;
	int found;

	/* create a locale buffer to be parsed by strtok */
	lbuf = malloc (strlen(buf)+1);

	/* copy the input string */
	strcpy (lbuf, buf);

	for(j=0;j<SEQ_MAX;++j)
		(*a)[j] = CODE_NONE;

	arg = strtok(lbuf, " \t\r\n");
	j = 0;
	while( arg != NULL )
	{
		found = 0;

		pik = input_keywords;

		while (!found && pik->name && pik->name[0] != 0)
		{
			if (strcmp(pik->name,arg) == 0)
			{
				/* this entry is only valid if it is a KEYCODE */
				if (pik->type == IKT_STD)
				{
					(*a)[j] = pik->val;
					j++;
					found = 1;
				}
			}
			pik++;
		}

		pik = osd_input_keywords;

		if (pik)
		{
			while (!found && pik->name && pik->name[0] != 0)
			{
				if (strcmp(pik->name,arg) == 0)
				{
					switch (pik->type)
					{
						case IKT_STD:
							(*a)[j] = pik->val;
							j++;
							found = 1;
						break;

						case IKT_OSD_KEY:
							(*a)[j] = keyoscode_to_code(pik->val);
							j++;
							found = 1;
						break;

						case IKT_OSD_JOY:
							(*a)[j] = joyoscode_to_code(pik->val);
							j++;
							found = 1;
						break;
					}
				}
				pik++;
			}
		}

		arg = strtok(NULL, " \t\r\n");
	}
	free (lbuf);
}

void init_analog_seq()
{
	struct InputPort *in;
	int player, axis;

/* init analogjoy_input array */
	for (player=0; player<MAX_PLAYER_COUNT; player++)
	{
		for (axis=0; axis<MAX_ANALOG_AXES; axis++)
		{
			analogjoy_input[player][axis] = CODE_NONE;
		}
	}

	in = Machine->input_ports;
	if (in->type == IPT_END) return; 	/* nothing to do */

	/* make sure the InputPort definition is correct */
	if (in->type != IPT_PORT)
	{
		log_cb(RETRO_LOG_ERROR, LOGPRE "Error in InputPort definition: expecting PORT_START\n");
		return;
	}
	else
	{
		in++;
	}

	while (in->type != IPT_END)
	{
		if (in->type != IPT_PORT && ((in->type & ~IPF_MASK) > IPT_ANALOG_START)
			&& ((in->type & ~IPF_MASK) < IPT_ANALOG_END))
		{
			int j, invert;
			InputSeq *seq;
			InputCode analog_seq;

			seq = input_port_seq(in);
			invert = 0;
			analog_seq = CODE_NONE;

			for(j=0; j<SEQ_MAX && analog_seq == CODE_NONE; ++j)
			{
				switch ((*seq)[j])
				{
					case CODE_NONE :
						continue;
					case CODE_NOT :
						invert = !invert;
						break;
					case CODE_OR :
						invert = 0;
						break;
					default:
						if (!invert && is_joystick_axis_code((*seq)[j]) )
						{
							analog_seq = return_os_joycode((*seq)[j]);
						}
						invert = 0;
						break;
				}
			}
			if (analog_seq != CODE_NONE)
			{
				player = IP_GET_PLAYER(in);

				switch (in->type & ~IPF_MASK)
				{
					case IPT_DIAL:
					case IPT_PADDLE:
					case IPT_TRACKBALL_X:
					case IPT_LIGHTGUN_X:
					case IPT_AD_STICK_X:
						axis = X_AXIS;
						break;
					case IPT_DIAL_V:
					case IPT_PADDLE_V:
					case IPT_TRACKBALL_Y:
					case IPT_LIGHTGUN_Y:
					case IPT_AD_STICK_Y:
						axis = Y_AXIS;
						break;
					case IPT_AD_STICK_Z:
					case IPT_PEDAL2:
						axis = Z_AXIS;
						break;
					case IPT_PEDAL:
						axis = PEDAL_AXIS;
						break;
					default:
						axis = 0;
						break;
				}

				analogjoy_input[player][axis] = analog_seq;
			}
		}

		in++;
	}

	return;
}
