#include <stdio.h>

#include "libretro.h"
#include "osdepend.h"
#include "input.h"

/******************************************************************************

	Joystick & Mouse/Trackball

******************************************************************************/

extern const struct JoystickInfo jsItems[];
int retroJsState[16];

const struct JoystickInfo *osd_get_joy_list(void)
{
    return jsItems;
}

int osd_is_joy_pressed(int joycode)
{
    return (joycode >= 0 && joycode < 16) ? retroJsState[joycode] : 0;
}

int osd_is_joystick_axis_code(int joycode)
{
    return 0;
}

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

void osd_lightgun_read(int player, int *deltax, int *deltay)
{

}

void osd_trak_read(int player, int *deltax, int *deltay)
{

}

void osd_analogjoy_read(int player,int analog_axis[MAX_ANALOG_AXES], InputCode analogjoy_input[MAX_ANALOG_AXES])
{

}

void osd_customize_inputport_defaults(struct ipd *defaults)
{

}

/******************************************************************************

	Keymapping

******************************************************************************/

const struct JoystickInfo jsItems[] =
{
    // Player 1
    {"P1 Left", RETRO_DEVICE_ID_JOYPAD_LEFT, JOYCODE_1_LEFT},
    {"P1 Right", RETRO_DEVICE_ID_JOYPAD_RIGHT, JOYCODE_1_RIGHT},
    {"P1 Up", RETRO_DEVICE_ID_JOYPAD_UP, JOYCODE_1_UP},
    {"P1 Down", RETRO_DEVICE_ID_JOYPAD_DOWN, JOYCODE_1_DOWN},
    {"P1 B", RETRO_DEVICE_ID_JOYPAD_B, JOYCODE_1_BUTTON1},
    {"P1 Y", RETRO_DEVICE_ID_JOYPAD_Y, JOYCODE_1_BUTTON2},
    {"P1 X", RETRO_DEVICE_ID_JOYPAD_X, JOYCODE_1_BUTTON3},
    {"P1 A", RETRO_DEVICE_ID_JOYPAD_A, JOYCODE_1_BUTTON4},
    {"P1 L", RETRO_DEVICE_ID_JOYPAD_L, JOYCODE_1_BUTTON5},
    {"P1 R", RETRO_DEVICE_ID_JOYPAD_R, JOYCODE_1_BUTTON6},
    {"P1 L2", RETRO_DEVICE_ID_JOYPAD_L2, JOYCODE_1_BUTTON7},
    {"P1 R2", RETRO_DEVICE_ID_JOYPAD_R2, JOYCODE_1_BUTTON8},
    {"P1 L3", RETRO_DEVICE_ID_JOYPAD_L3, JOYCODE_1_BUTTON9},
    {"P1 R3", RETRO_DEVICE_ID_JOYPAD_R3, JOYCODE_1_BUTTON10},
    {"P1 Start", RETRO_DEVICE_ID_JOYPAD_START, JOYCODE_1_START},
    {"P1 Select", RETRO_DEVICE_ID_JOYPAD_SELECT, JOYCODE_1_SELECT},
    {0, 0, 0}
};
