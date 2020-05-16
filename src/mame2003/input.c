#include "mame.h"
#include "driver.h"
#include <math.h>
//#include "inptport.h"
/* todo
  fix analog up
  do lots of testing a lots changed
*/

struct JoystickInfo mame_joy_map[] =
{
	{ "J1 Left",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT,   JOYCODE_1_LEFT	      },
	{ "J1 Right",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT,  JOYCODE_1_RIGHT	      },
	{ "J1 Up",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_UP,     JOYCODE_1_UP	      },
	{ "J1 Down",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN,   JOYCODE_1_DOWN	      },
	{ "J1 B",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_B,      JOYCODE_OTHER	      },
	{ "J1 A",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_A,      JOYCODE_OTHER	      },
	{ "J1 Y",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_Y,      JOYCODE_OTHER	      },
	{ "J1 X",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_X,      JOYCODE_OTHER	      },
	{ "J1 L",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_L,      JOYCODE_OTHER	      },
	{ "J1 R",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_R,      JOYCODE_OTHER	      },
	{ "J1 L2",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_L2,     JOYCODE_OTHER	      },
	{ "J1 R2",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_R2,     JOYCODE_OTHER	      },
	{ "J1 L3",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_L3,     JOYCODE_OTHER	      },
	{ "J1 R3",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_R3,     JOYCODE_OTHER      },
	{ "J1 Start",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_START,  JOYCODE_OTHER	      },
	{ "J1 Sel",	   ((0) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT, JOYCODE_OTHER	      },
	{ "M1 B1",	   ((0) * 26) + 16,			      JOYCODE_MOUSE_1_BUTTON1 },
	{ "M1 B2",	   ((0) * 26) + 17,			      JOYCODE_MOUSE_1_BUTTON2 },
	{ "J1 AX0 X-",	   (((0) * 26) + 18) | 0x10000,			      JOYCODE_1_LEFT_LEFT     },
	{ "J1 AX0 X+",	   (((0) * 26) + 19) | 0x10000,			      JOYCODE_1_LEFT_RIGHT    },
	{ "J1 AX1 Y-",	   (((0) * 26) + 20) | 0x10000,			      JOYCODE_1_LEFT_UP	      },
	{ "J1 AX1 Y+",	   (((0) * 26) + 21) | 0x10000,			      JOYCODE_1_LEFT_DOWN     },
	{ "J1 AX2 X-",	   (((0) * 26) + 22) | 0x10000,			      JOYCODE_1_RIGHT_LEFT    },
	{ "J1 AX2 X+",	   (((0) * 26) + 23) | 0x10000,			      JOYCODE_1_RIGHT_RIGHT   },
	{ "J1 AX3 Y-",	   (((0) * 26) + 24) | 0x10000,			      JOYCODE_1_RIGHT_UP      },
	{ "J1 AX3 Y+",	   (((0) * 26) + 25) | 0x10000,			      JOYCODE_1_RIGHT_DOWN    },

	{ "J2 Left ",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT,   JOYCODE_2_LEFT	      },
	{ "J2 Right",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT,  JOYCODE_2_RIGHT	      },
	{ "J2 Up",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_UP,     JOYCODE_2_UP	      },
	{ "J2 Down",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN,   JOYCODE_2_DOWN	      },
	{ "J2 B",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_B,      JOYCODE_OTHER	      },
	{ "J2 A",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_A,      JOYCODE_OTHER	      },
	{ "J2 Y",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_Y,      JOYCODE_OTHER	      },
	{ "J2 X",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_X,      JOYCODE_OTHER	      },
	{ "J2 L",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_L,      JOYCODE_OTHER	      },
	{ "J2 R",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_R,      JOYCODE_OTHER	      },
	{ "J2 L2",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_L2,     JOYCODE_OTHER	      },
	{ "J2 R2",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_R2,     JOYCODE_OTHER	      },
	{ "J2 L3",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_L3,     JOYCODE_OTHER	      },
	{ "J2 R3",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_R3,     JOYCODE_OTHER      },
	{ "J2 Start",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_START,  JOYCODE_OTHER	      },
	{ "J2 Select",	   ((1) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT, JOYCODE_OTHER	      },
	{ "M2 B1", ((1) * 26) + 16,			      JOYCODE_MOUSE_2_BUTTON1 },
	{ "M2 B2", ((1) * 26) + 17,			      JOYCODE_MOUSE_2_BUTTON2 },
	{ "J2 AXIS 0 X-",  (((1) * 26) + 18) | 0x10000,			      JOYCODE_2_LEFT_LEFT     },
	{ "J2 AXIS 0 X+",  (((1) * 26) + 19) | 0x10000,			      JOYCODE_2_LEFT_RIGHT    },
	{ "J2 AXIS 1 Y-",  (((1) * 26) + 20) | 0x10000,			      JOYCODE_2_LEFT_UP	      },
	{ "J2 AXIS 1 Y+",  (((1) * 26) + 21) | 0x10000,			      JOYCODE_2_LEFT_DOWN     },
	{ "J2 AXIS 2 X-",  (((1) * 26) + 22) | 0x10000,			      JOYCODE_2_RIGHT_LEFT    },
	{ "J2 AXIS 2 X+",  (((1) * 26) + 23) | 0x10000,			      JOYCODE_2_RIGHT_RIGHT   },
	{ "J2 AXIS 3 Y-",  (((1) * 26) + 24) | 0x10000,			      JOYCODE_2_RIGHT_UP      },
	{ "J2 AXIS 3 Y+",  (((1) * 26) + 25) | 0x10000,			      JOYCODE_2_RIGHT_DOWN    },

	{ "J3 Left ",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT,   JOYCODE_3_LEFT	      },
	{ "J3 Right",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT,  JOYCODE_3_RIGHT	      },
	{ "J3 Up",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_UP,     JOYCODE_3_UP	      },
	{ "J3 Down",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN,   JOYCODE_3_DOWN	      },
	{ "J3 B",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_B,      JOYCODE_OTHER	      },
	{ "J3 A",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_A,      JOYCODE_OTHER	      },
	{ "J3 Y",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_Y,      JOYCODE_OTHER	      },
	{ "J3 X",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_X,      JOYCODE_OTHER	      },
	{ "J3 L",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_L,      JOYCODE_OTHER	      },
	{ "J3 R",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_R,      JOYCODE_OTHER	      },
	{ "J3 L2",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_L2,     JOYCODE_OTHER	      },
	{ "J3 R2",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_R2,     JOYCODE_OTHER	      },
	{ "J3 L3",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_L3,     JOYCODE_OTHER	      },
	{ "J3 R3",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_R3,     JOYCODE_OTHER      },
	{ "J3 Start",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_START,  JOYCODE_OTHER	      },
	{ "J3 Select",	   ((2) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT, JOYCODE_OTHER	      },
	{ "M3 B1", ((2) * 26) + 16 ,			      JOYCODE_MOUSE_3_BUTTON1 },
	{ "M3 B2", ((2) * 26) + 17 ,			      JOYCODE_MOUSE_3_BUTTON2 },
	{ "J3 AXIS 0 X-",  (((2) * 26) + 18) | 0x10000,			      JOYCODE_3_LEFT_LEFT     },
	{ "J3 AXIS 0 X+",  (((2) * 26) + 19) | 0x10000,			      JOYCODE_3_LEFT_RIGHT    },
	{ "J3 AXIS 1 Y-",  (((2) * 26) + 20) | 0x10000,			      JOYCODE_3_LEFT_UP	      },
	{ "J3 AXIS 1 Y+",  (((2) * 26) + 21) | 0x10000,			      JOYCODE_3_LEFT_DOWN     },
	{ "J3 AXIS 2 X-",  (((2) * 26) + 22) | 0x10000,			      JOYCODE_3_RIGHT_LEFT    },
	{ "J3 AXIS 2 X+",  (((2) * 26) + 23) | 0x10000,			      JOYCODE_3_RIGHT_RIGHT   },
	{ "J3 AXIS 3 Y-",  (((2) * 26) + 24) | 0x10000,			      JOYCODE_3_RIGHT_UP      },
	{ "J3 AXIS 3 Y+",  (((2) * 26) + 25) | 0x10000,			      JOYCODE_3_RIGHT_DOWN    },

	{ "J4 Left ",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_LEFT,   JOYCODE_4_LEFT	      },
	{ "J4 Right",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_RIGHT,  JOYCODE_4_RIGHT	      },
	{ "J4 Up",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_UP,     JOYCODE_4_UP	      },
	{ "J4 Down",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_DOWN,   JOYCODE_4_DOWN	      },
	{ "J4 B",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_B,      JOYCODE_OTHER	      },
	{ "J4 A",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_A,      JOYCODE_OTHER	      },
	{ "J4 Y",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_Y,      JOYCODE_OTHER	      },
	{ "J4 X",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_X,      JOYCODE_OTHER	      },
	{ "J4 L",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_L,      JOYCODE_OTHER	      },
	{ "J4 R",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_R,      JOYCODE_OTHER	      },
	{ "J4 L2",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_L2,     JOYCODE_OTHER	      },
	{ "J4 R2",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_R2,     JOYCODE_OTHER	      },
	{ "J4 L3",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_L3,     JOYCODE_OTHER	      },
	{ "J4 R3",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_R3,     JOYCODE_OTHER      },
	{ "J4 Start",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_START,  JOYCODE_OTHER	      },
	{ "J4 Select",	   ((3) * 26) + RETRO_DEVICE_ID_JOYPAD_SELECT, JOYCODE_OTHER	      },
	{ "M4 B1", ((3) * 26) + 16,			      JOYCODE_MOUSE_4_BUTTON1 },
	{ "M4 B1", ((3) * 26) + 17,			      JOYCODE_MOUSE_4_BUTTON2 },
	{ "J4 AXIS 0 X-",  (((3) * 26) + 18) | 0x10000,			      JOYCODE_4_LEFT_LEFT     },
	{ "J4 AXIS 0 X+",  (((3) * 26) + 19) | 0x10000,			      JOYCODE_4_LEFT_RIGHT    },
	{ "J4 AXIS 1 Y-",  (((3) * 26) + 20) | 0x10000,			      JOYCODE_4_LEFT_UP	      },
	{ "J4 AXIS 1 Y+",  (((3) * 26) + 21) | 0x10000,			      JOYCODE_4_LEFT_DOWN     },
	{ "J4 AXIS 2 X-",  (((3) * 26) + 22) | 0x10000,			      JOYCODE_4_RIGHT_LEFT    },
	{ "J4 AXIS 2 X+",  (((3) * 26) + 23) | 0x10000,			      JOYCODE_4_RIGHT_RIGHT   },
	{ "J4 AXIS 3 Y-",  (((3) * 26) + 24) | 0x10000,			      JOYCODE_4_RIGHT_UP      },
	{ "J4 AXIS 3 Y+",  (((3) * 26) + 25) | 0x10000,			      JOYCODE_4_RIGHT_DOWN    },
	{ 0,		   0,						      0			      }
};


int16_t mouse_x[4] = { 0 };
int16_t mouse_y[4] = { 0 };
int retroJsState[156] = { 0 };              // initialise to zero - we are only reading 4 players atm map for 6 (6*26)
int16_t analogjoy[4][4] = { 0 };

int convert_analog_scale(int input);
void controller_remap(unsigned port, unsigned device);
int16_t retropad_layout[4];
static struct retro_input_descriptor empty[] = { { 0 } };
/******************************************************************************
*
*       Joystick & Mouse/Trackball
*
******************************************************************************/

/* These calibration functions should never actually be used (as long as needs_calibration returns 0 anyway).*/
int osd_joystick_needs_calibration(void)
{
	return 0;
}
void osd_joystick_start_calibration(void)
{
}
const char *osd_joystick_calibrate_next(void)
{
	return 0;
}
void osd_joystick_calibrate(void)
{
}
void osd_joystick_end_calibration(void)
{
}

const struct JoystickInfo *osd_get_joy_list(void)
{
	return mame_joy_map;
}

int osd_is_joy_pressed(int joycode)
{
	if (options.input_interface == RETRO_DEVICE_KEYBOARD) return 0;

		return retroJsState[joycode  &~0x10000];

	return 0;
}

int osd_is_joystick_axis_code(int joycode)
{
  if  ( (joycode & 0x10000) == 0x10000 )
    return 1;

return 0;
}

void osd_lightgun_read(int player, int *deltax, int *deltay)
{
}

void osd_analogjoy_read(int player, int analog_axis[MAX_ANALOG_AXES], InputCode analogjoy_input[MAX_ANALOG_AXES])
{
/* fixed the reversing of axis ie changing left and right. only use one +/- axis per controller Z Y or Y*/

	int i;
	int value;

	for (i = 0; i < MAX_ANALOG_AXES; i++) {
		int code;
		value = 0;
		if (analogjoy_input[i] != CODE_NONE) {
			code = analogjoy_input[i] &~0x10000;

			if (code == (player * 26) + 18  || code == (player * 26) + 19)
				value = convert_analog_scale(analogjoy[player][0]);

			else if (code == (player * 26) + 20 || code == (player * 26) + 21)
				value = convert_analog_scale(analogjoy[player][1]);

			else if (code == (player * 26) + 22 || code == (player * 23) + 23)
				value = convert_analog_scale(analogjoy[player][2]);

			else if (code == (player * 26) + 24 || code == (player * 25) + 25)
				value = convert_analog_scale(analogjoy[player][3]);

    if(code%2) value = -value;

			analog_axis[i] = value;
		}
	}
}


void osd_trak_read(int player, int *deltax, int *deltay)
{
	*deltax = mouse_x[player];
	*deltay = mouse_y[player];
}

/******************************************************************************
*
*       Keyboard
*
******************************************************************************/

extern const struct KeyboardInfo retroKeys[];
int retroKeyState[512];
struct KeyboardInfo temp[] = { { NULL, 0, 0 } };
const struct KeyboardInfo *osd_get_key_list(void)
{
	if (options.input_interface != RETRO_DEVICE_JOYPAD)
		return retroKeys;
	else
		return temp;
}

int osd_is_key_pressed(int keycode)
{
	if (options.input_interface == RETRO_DEVICE_JOYPAD)
		return 0;

	if (keycode < 512 && keycode >= 0)
		return retroKeyState[keycode];

	return 0;
}


int osd_readkey_unicode(int flush)
{
	/* TODO*/
	return 0;
}

void osd_customize_inputport_defaults(struct ipd *defaults)
{

//	controller_remap(0); // just call this for now will need more test
//  controller_remap(1);

}


/******************************************************************************
*
*       Keymapping
*
******************************************************************************/

/* Unassigned keycodes*/
/*	KEYCODE_OPENBRACE, KEYCODE_CLOSEBRACE, KEYCODE_BACKSLASH2, KEYCODE_STOP, KEYCODE_LWIN, KEYCODE_RWIN, KEYCODE_DEL_PAD, KEYCODE_PAUSE,*/

/* The format for each systems key constants is RETROK_$(TAG) and KEYCODE_$(TAG) */
/* EMIT1(TAG): The tag value is the same between libretro and the core           */
/* EMIT2(RTAG, MTAG): The tag value is different between the two                 */
/* EXITX(TAG): The core has no equivalent key.*/

#define EMIT2(RETRO, KEY) { (char *)#RETRO, RETROK_ ## RETRO, KEYCODE_ ## KEY }
#define EMIT1(KEY) { (char *)#KEY, RETROK_ ## KEY, KEYCODE_ ## KEY }
#define EMITX(KEY) { (char *)#KEY, RETROK_ ## KEY, KEYCODE_OTHER }

const struct KeyboardInfo retroKeys[] =
{
	EMIT1(BACKSPACE),
	EMIT1(TAB),
	EMITX(CLEAR),

	EMIT1(BACKSPACE),
	EMIT1(TAB),
	EMITX(CLEAR),
	EMIT2(RETURN,	    ENTER),
	EMITX(PAUSE),
	EMIT2(ESCAPE,	    ESC),
	EMIT1(SPACE),
	EMITX(EXCLAIM),
	EMIT2(QUOTEDBL,	    TILDE),
	EMITX(HASH),
	EMITX(DOLLAR),
	EMITX(AMPERSAND),
	EMIT1(QUOTE),
	EMITX(LEFTPAREN),
	EMITX(RIGHTPAREN),
	EMIT1(ASTERISK),
	EMIT2(PLUS,	    EQUALS),
	EMIT1(COMMA),
	EMIT1(MINUS),
	EMITX(PERIOD),
	EMIT1(SLASH),

	EMIT1(0),	    EMIT1(1),	   EMIT1(2),	   EMIT1(3),	  EMIT1(4),    EMIT1(5),       EMIT1(6),    EMIT1(7),	 EMIT1(8), EMIT1(9),

	EMIT1(COLON),
	EMITX(SEMICOLON),
	EMITX(LESS),
	EMITX(EQUALS),
	EMITX(GREATER),
	EMITX(QUESTION),
	EMITX(AT),
	EMITX(LEFTBRACKET),
	EMIT1(BACKSLASH),
	EMITX(RIGHTBRACKET),
	EMITX(CARET),
	EMITX(UNDERSCORE),
	EMITX(BACKQUOTE),

	EMIT2(a,	    A),		   EMIT2(b,	   B),		  EMIT2(c,     C),	       EMIT2(d,	    D),		 EMIT2(e,  E),	    EMIT2(f,  F),
	EMIT2(g,	    G),		   EMIT2(h,	   H),		  EMIT2(i,     I),	       EMIT2(j,	    J),		 EMIT2(k,  K),	    EMIT2(l,  L),
	EMIT2(m,	    M),		   EMIT2(n,	   N),		  EMIT2(o,     O),	       EMIT2(p,	    P),		 EMIT2(q,  Q),	    EMIT2(r,  R),
	EMIT2(s,	    S),		   EMIT2(t,	   T),		  EMIT2(u,     U),	       EMIT2(v,	    V),		 EMIT2(w,  W),	    EMIT2(x,  X),
	EMIT2(y,	    Y),		   EMIT2(z,	   Z),

	EMIT2(DELETE,	    DEL),

	EMIT2(KP0,	    0_PAD),	   EMIT2(KP1,	   1_PAD),	  EMIT2(KP2,   2_PAD),	       EMIT2(KP3,   3_PAD),
	EMIT2(KP4,	    4_PAD),	   EMIT2(KP5,	   5_PAD),	  EMIT2(KP6,   6_PAD),	       EMIT2(KP7,   7_PAD),
	EMIT2(KP8,	    8_PAD),	   EMIT2(KP9,	   9_PAD),

	EMITX(KP_PERIOD),
	EMIT2(KP_DIVIDE,    SLASH_PAD),
	EMITX(KP_MULTIPLY),
	EMIT2(KP_MINUS,	    MINUS_PAD),
	EMIT2(KP_PLUS,	    PLUS_PAD),
	EMIT2(KP_ENTER,	    ENTER_PAD),
	EMITX(KP_EQUALS),

	EMIT1(UP),	    EMIT1(DOWN),   EMIT1(RIGHT),   EMIT1(LEFT),
	EMIT1(INSERT),	    EMIT1(HOME),   EMIT1(END),	   EMIT2(PAGEUP,  PGUP),       EMIT2(PAGEDOWN, PGDN),

	EMIT1(F1),	    EMIT1(F2),	   EMIT1(F3),	   EMIT1(F4),	  EMIT1(F5),   EMIT1(F6),
	EMIT1(F7),	    EMIT1(F8),	   EMIT1(F9),	   EMIT1(F10),	  EMIT1(F11),  EMIT1(F12),
	EMITX(F13),	    EMITX(F14),	   EMITX(F15),

	EMIT1(NUMLOCK),
	EMIT1(CAPSLOCK),
	EMIT2(SCROLLOCK,    SCRLOCK),
	EMIT1(RSHIFT),	    EMIT1(LSHIFT), EMIT2(RCTRL,	   RCONTROL),	  EMIT2(LCTRL, LCONTROL),      EMIT1(RALT), EMIT1(LALT),
	EMITX(RMETA),	    EMITX(LMETA),  EMITX(LSUPER),  EMITX(RSUPER),

	EMITX(MODE),
	EMITX(COMPOSE),

	EMITX(HELP),
	EMIT2(PRINT,	    PRTSCR),
	EMITX(SYSREQ),
	EMITX(BREAK),
	EMIT1(MENU),
	EMITX(POWER),
	EMITX(EURO),
	EMITX(UNDO),

	{ 0,		    0,		   0 }
};

#ifdef _MSC_VER
#if _MSC_VER < 1800
double round(double number)
{
	return (number >= 0) ? (int)(number + 0.5) : (int)(number - 0.5);
}
#endif
#endif

float map(float value, float start1, float stop1, float start2, float stop2)
{
  float outgoing =   start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));

    return outgoing;
}
long map2(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int convert_analog_scale(int input)
{

// log_cb(RETRO_LOG_INFO, LOGPRE "map(%d) map2(%d) \n,",(int)map(input,0,32767,0,128),(int)map2(input,0,32767,0,128));

return (int)map(input,0,32767,0,128);
}

/* get pointer axis vector from coord */
int16_t get_pointer_delta(int16_t coord, int16_t *prev_coord)
{
	int16_t delta = 0;

	if (*prev_coord == 0 || coord == 0) {
		*prev_coord = coord;
	} else {
		if (coord != *prev_coord) {
			delta = coord - *prev_coord;
			*prev_coord = coord;
		}
	}

	return delta;
}

struct retro_controller_description  controllers[] = {
	{ "Gamepad",          1 },
	{ "Arcade 6 Button",  2 },
	{ "Arcade 8 Button",  3 }
};

struct retro_controller_info retropad_subdevice_ports[] = {
	{ controllers, 3 },
	{ controllers, 3 },
	{ controllers, 3 },
	{ controllers, 3 },
	{ 0 },
};


void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, empty); // is this necessary? it was in the sample code
	printf("port:%u device:%u\n",in_port , device);
	retropad_layout[in_port] = device;
	controller_remap(in_port, device);
}

void controller_remap(unsigned port, unsigned device)
{
/* note: testing will need done as a rule of thumb never save mames input changes if your diverted from defaults
 *
 * note: I was testing out the cutstom conroller ports(retroarch) is seriously bugged it works for the core your using
 * when you set another on another core setting changes to known. Its not reliable atm will have to be set in the options
 * menu  for reliability until its fixed.
 *
 *
 * mapping viusal aid for gamepads
 * Assuming the standard RetroPad layout:
 *
 *   [L2]                                 [R2]
 *   [L]                                   [R]
 *
 *     [^]                               [X]
 *
 * [<]     [>]    [start] [selct]    [Y]     [A]
 *
 *     [v]                               [B]
 */

/* initial work done to allow mapping need to think of the layout i want for 8 panel should make sense
    for other RA cores that use an l/r button so two choices are.
    Y X L  R
    B A L2 R2
    or
    L  Y X R
    L2 B A R2
    ill test both with a few dial games and some snes9x games see which one feels better. The gampad creep
    will sink in i get a feeling the first one will be a more natural fit test both over time for a real result.

*/
int offset;

enum {
 RP_LEFT =0,
 RP_RIGHT,
 RP_UP,
 RP_DOWN,
 RP_B,
 RP_A,
 RP_Y,
 RP_X,
 RP_L,
 RP_R,
 RP_L2,
 RP_R2,
 RP_L3,
 RP_R3,
 RP_ST,
 RP_SL
};


enum {
 LEFT=0,
 RIGHT,
 UP,
 DOWN,
 BUTTON1,
 BUTTON2,
 BUTTON3,
 BUTTON4,
 BUTTON5,
 BUTTON6,
 BUTTON7,
 BUTTON8,
 BUTTON9,
 BUTTON10,
 START,
 SELECT,
};



if( port== 0) offset = JOYCODE_1_LEFT;
if( port== 1) offset = JOYCODE_2_LEFT;
if( port== 2) offset = JOYCODE_3_LEFT;
if( port== 3) offset = JOYCODE_4_LEFT;

//only need to assign 6 buttons for mame itself as well as start and select

if (device == 1) // gampad mapping
{
		mame_joy_map[(port * 26) + RP_B].standardcode = offset + BUTTON1;
		mame_joy_map[(port * 26) + RP_A].standardcode = offset + BUTTON2;
		mame_joy_map[(port * 26) + RP_Y].standardcode = offset + BUTTON3;
		mame_joy_map[(port * 26) + RP_X].standardcode = offset + BUTTON4;
		mame_joy_map[(port * 26) + RP_L].standardcode = offset + BUTTON5;
		mame_joy_map[(port * 26) + RP_R].standardcode = offset + BUTTON6;
		mame_joy_map[(port * 26) + RP_ST].standardcode = offset + START;
		mame_joy_map[(port * 26) + RP_SL].standardcode = offset + SELECT;
}

if (device == 2) // 6 button
{
		mame_joy_map[(port * 26) + RP_B].standardcode = offset + BUTTON1;
		mame_joy_map[(port * 26) + RP_A].standardcode = offset + BUTTON2;
		mame_joy_map[(port * 26) + RP_Y].standardcode = offset + BUTTON4;
		mame_joy_map[(port * 26) + RP_X].standardcode = offset + BUTTON5;
		mame_joy_map[(port * 26) + RP_L].standardcode = offset + BUTTON6;
		mame_joy_map[(port * 26) + RP_R].standardcode = offset + BUTTON3;
		mame_joy_map[(port * 26) + RP_ST].standardcode = offset + START;
		mame_joy_map[(port * 26) + RP_SL].standardcode = offset + SELECT;
}


	if (strcmp(Machine->gamedrv->name, "sf2") == 0) {
		mame_joy_map[(port * 26) + RP_B].standardcode = offset + BUTTON4;
		mame_joy_map[(port * 26) + RP_A].standardcode = offset + BUTTON5;
		mame_joy_map[(port * 26) + RP_Y].standardcode = offset + BUTTON1;
		mame_joy_map[(port * 26) + RP_X].standardcode = offset + BUTTON2;
		mame_joy_map[(port * 26) + RP_L].standardcode = offset + BUTTON3;
		mame_joy_map[(port * 26) + RP_R].standardcode = offset + BUTTON6;
	}

//this applies to gampads will mess up arcade panels

	if ( (strcmp(Machine->gamedrv->name, "renegade") == 0 ||
  strcmp(Machine->gamedrv->name, "ddragon") == 0 ||
  strcmp(Machine->gamedrv->name, "pow") == 0 ||
  strcmp(Machine->gamedrv->name, "ddragon2") == 0) && (device == 1) ) {
		mame_joy_map[(port * 26) + RP_B].standardcode = offset + BUTTON2;
		mame_joy_map[(port * 26) + RP_A].standardcode = offset + BUTTON3;
		mame_joy_map[(port * 26) + RP_Y].standardcode = offset + BUTTON1;
	}
}
