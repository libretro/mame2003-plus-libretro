#include <stdio.h>

#include "libretro.h"
#include "osdepend.h"
#include "input.h"

/******************************************************************************

	Joystick & Mouse/Trackball

******************************************************************************/

extern const struct JoystickInfo jsItems[];
int retroJsState[64];

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

// These calibration functions should never actually be used (as long as needs_calibration returns 0 anyway).
int osd_joystick_needs_calibration(void) { return 0; }
void osd_joystick_start_calibration(void){ }
const char *osd_joystick_calibrate_next(void) { return 0; }
void osd_joystick_calibrate(void) { }
void osd_joystick_end_calibration(void) { }


/******************************************************************************

	Keymapping

******************************************************************************/

#define EMIT_RETRO_PAD(INDEX) \
    {"P" #INDEX " Left", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_LEFT, JOYCODE_##INDEX##_LEFT}, \
    {"P" #INDEX " Right", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_RIGHT, JOYCODE_##INDEX##_RIGHT}, \
    {"P" #INDEX " Up", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_UP, JOYCODE_##INDEX##_UP}, \
    {"P" #INDEX " Down", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_DOWN, JOYCODE_##INDEX##_DOWN}, \
    {"P" #INDEX " B", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_B, JOYCODE_##INDEX##_BUTTON1}, \
    {"P" #INDEX " Y", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_Y, JOYCODE_##INDEX##_BUTTON2}, \
    {"P" #INDEX " X", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_X, JOYCODE_##INDEX##_BUTTON3}, \
    {"P" #INDEX " A", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_A, JOYCODE_##INDEX##_BUTTON4}, \
    {"P" #INDEX " L", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_L, JOYCODE_##INDEX##_BUTTON5}, \
    {"P" #INDEX " R", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_R, JOYCODE_##INDEX##_BUTTON6}, \
    {"P" #INDEX " L2", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_L2, JOYCODE_##INDEX##_BUTTON7}, \
    {"P" #INDEX " R2", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_R2, JOYCODE_##INDEX##_BUTTON8}, \
    {"P" #INDEX " L3", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_L3, JOYCODE_##INDEX##_BUTTON9}, \
    {"P" #INDEX " R3", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_R3, JOYCODE_##INDEX##_BUTTON10}, \
    {"P" #INDEX " Start", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_START, JOYCODE_##INDEX##_START}, \
    {"P" #INDEX " Select", ((INDEX - 1) * 16) + RETRO_DEVICE_ID_JOYPAD_SELECT, JOYCODE_##INDEX##_SELECT}


const struct JoystickInfo jsItems[] =
{
    EMIT_RETRO_PAD(1),
    EMIT_RETRO_PAD(2),
    EMIT_RETRO_PAD(3),
    EMIT_RETRO_PAD(4),
    {0, 0, 0}
};
