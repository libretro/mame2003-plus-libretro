/*********************************************************************
*
*       mame2003.c
*
*   an updated port of Xmame 0.78 to the libretro API
*
*********************************************************************/

#include <stdint.h>
#include <string/stdstring.h>
#include <libretro.h>
#include <file/file_path.h>
#include <math.h>
#include "mame.h"
#include "driver.h"
#include "state.h"
#include "log.h"
#include "fileio.h"
#include "controls.h"
#include "usrintrf.h"

extern int16_t mouse_x[4];
extern int16_t mouse_y[4];
extern struct JoystickInfo mame_joy_map[];
extern int retroJsState[156];
extern int16_t analogjoy[4][4];
extern const struct KeyboardInfo retroKeys[];
extern int retroKeyState[512];


int16_t prev_pointer_x;
int16_t prev_pointer_y;
int gotFrame;
int running = 0;
int control_flag = -1;
int pressure_check = 1.28 * 20;
struct ipd *default_inputs;          /* pointer the array of structs with default MAME input mappings and labels */
const struct GameDriver *game_driver;
unsigned retroColorMode;
unsigned long lastled = 0;
retro_log_printf_t log_cb;
static struct retro_message frontend_message;

struct                             retro_perf_callback perf_cb;
retro_environment_t environ_cb = NULL;
retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_set_led_state_t led_state_cb = NULL;
int convert_analog_scale(int input);
int16_t get_pointer_delta(int16_t coord, int16_t *prev_coord);

/******************************************************************************
*
*  private function prototypes
*
******************************************************************************/


extern struct retro_controller_description controllers[];
extern struct retro_controller_info retropad_subdevice_ports[];


static void   check_system_specs(void);

int    get_mame_ctrl_id(int display_idx, int retro_ID);
void   change_control_type(int port);
void   set_content_flags(void);
void   init_core_options(void);
void   update_variables(bool first_time);
/******************************************************************************
*
*       frontend message interface
*
******************************************************************************/
void frontend_message_cb(const char *message_string, unsigned frames_to_display)
{
	frontend_message.msg = message_string;
	frontend_message.frames = frames_to_display;
	environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &frontend_message);
}

/******************************************************************************
*
*  implementation of key libretro functions
*
******************************************************************************/
void retro_init(void)
{
	struct retro_log_callback log;

	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
		log_cb = log.log;
	else
		log_cb = NULL;

#ifdef LOG_PERFORMANCE
	environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);
#endif

	check_system_specs();
}

static void check_system_specs(void)
{
	/* Should we set level variably like the API asks? Are there any frontends that implement this? */
	unsigned level = 10; /* For stub purposes, set to the highest level */

	environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;
}





void retro_get_system_av_info(struct retro_system_av_info *info)
{
	mame2003_video_get_geometry(&info->geometry);
	if (options.machine_timing) {
		if (Machine->drv->frames_per_second < 60.0)
			info->timing.fps = 60.0;
		else
			info->timing.fps = Machine->drv->frames_per_second; /* qbert is 61 fps */

		if ((Machine->drv->frames_per_second * 1000 < options.samplerate) || (Machine->drv->frames_per_second < 60)) {
			info->timing.sample_rate = Machine->drv->frames_per_second * 1000;
			log_cb(RETRO_LOG_INFO, LOGPRE "Sample timing rate too high for framerate required dropping to %f", Machine->drv->frames_per_second * 1000);
		} else {
			info->timing.sample_rate = options.samplerate;
			log_cb(RETRO_LOG_INFO, LOGPRE "Sample rate set to %d\n", options.samplerate);
		}
	} else {
		info->timing.fps = Machine->drv->frames_per_second;

		if (Machine->drv->frames_per_second * 1000 < options.samplerate)
			info->timing.sample_rate = 22050;

		else
			info->timing.sample_rate = options.samplerate;
	}
}

unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}


void retro_get_system_info(struct retro_system_info *info)
{
	/* this must match the 'corename' field in mame2003_plus_libretro.info
	 * in order for netplay to work. */
	info->library_name = "Blaster-Barcade";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
	info->library_version = GIT_VERSION;
	info->valid_extensions = "zip";
	info->need_fullpath = true;
	info->block_extract = true;
}


bool retro_load_game(const struct retro_game_info *game)
{
	int i;
	int driverIndex = 0;
	int port_index;
	char *driver_lookup = NULL;

	if (string_is_empty(game->path)) {
		log_cb(RETRO_LOG_ERROR, LOGPRE "Content path is not set. Exiting!\n");
		return false;
	}

	log_cb(RETRO_LOG_INFO, LOGPRE "Full content path %s\n", game->path);
	if (!path_is_valid(game->path)) {
		log_cb(RETRO_LOG_ERROR, LOGPRE "Content path is not valid. Exiting!");
		return false;
	}
	log_cb(RETRO_LOG_INFO, LOGPRE "Git Version %s\n", GIT_VERSION);
	driver_lookup = strdup(path_basename(game->path));
	path_remove_extension(driver_lookup);

	log_cb(RETRO_LOG_INFO, LOGPRE "Content lookup name: %s\n", driver_lookup);

	for (driverIndex = 0; driverIndex < total_drivers; driverIndex++) {
		const struct GameDriver *needle = drivers[driverIndex];

		if ((strcasecmp(driver_lookup, needle->description) == 0)
		    || (strcasecmp(driver_lookup, needle->name) == 0)) {
			log_cb(RETRO_LOG_INFO, LOGPRE "Driver index counter: %d. Matched game driver: %s\n", driverIndex, needle->name);
			game_driver = needle;
			options.romset_filename_noext = driver_lookup;
			break;
		}
		if (driverIndex == total_drivers - 2) { // we could fix the total drives in drivers c but the it pointless its taken into account here
			log_cb(RETRO_LOG_ERROR, LOGPRE "Driver index counter: %d. Game driver not found for %s!\n", driverIndex, driver_lookup);
			return false;
		}
	}

	if (!init_game(driverIndex))
		return false;

	set_content_flags();

	options.libretro_content_path = strdup(game->path);
	path_basedir(options.libretro_content_path);
	/*fix trailing slash in path*/
	for (i = 0; options.libretro_content_path[i] != '\0'; ++i);
	if (options.libretro_content_path[i - 1] == '/' || options.libretro_content_path[i - 1] == '\\')
		options.libretro_content_path[i - 1] = 0;

	/* Get system directory from frontend */
	options.libretro_system_path = NULL;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &options.libretro_system_path);
	if (options.libretro_system_path == NULL || options.libretro_system_path[0] == '\0') {
		log_cb(RETRO_LOG_INFO, LOGPRE "libretro system path not set by frontend, using content path\n");
		options.libretro_system_path = options.libretro_content_path;
	}

	/* Get save directory from frontend */
	options.libretro_save_path = NULL;
	environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &options.libretro_save_path);
	if (options.libretro_save_path == NULL || options.libretro_save_path[0] == '\0') {
		log_cb(RETRO_LOG_INFO, LOGPRE "libretro save path not set by frontent, using content path\n");
		options.libretro_save_path = options.libretro_content_path;
	}

	log_cb(RETRO_LOG_INFO, LOGPRE "content path: %s\n", options.libretro_content_path);
	log_cb(RETRO_LOG_INFO, LOGPRE " system path: %s\n", options.libretro_system_path);
	log_cb(RETRO_LOG_INFO, LOGPRE "   save path: %s\n", options.libretro_save_path);


	init_core_options();
	update_variables(true);

//todo add some extar code for desciptors if i feel like add ra mapping all i need is the port info for now
	environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)retropad_subdevice_ports);


	if (!run_game(driverIndex))
		return true;

	return false;
}



void retro_reset(void)
{
	machine_reset(); /* use internal core function */
}


void retro_run(void)
{
	int i;
	bool pointer_pressed;
	const struct KeyboardInfo *thisInput;
	bool updated = false;

	if (running == 0) running = 1;
	poll_cb();

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		update_variables(false);

	/* Keyboard */
	thisInput = retroKeys;
	while (thisInput->name) {
		retroKeyState[thisInput->code] = input_cb(0, RETRO_DEVICE_KEYBOARD, 0, thisInput->code);
		thisInput++;
	}

	for (i = 0; i < 4; i++) {
		unsigned int offset = (i * 26);

		/* Analog joystick */

		analogjoy[i][0] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
		analogjoy[i][1] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
		analogjoy[i][2] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
		analogjoy[i][3] = input_cb(i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

		retroJsState[0 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
		retroJsState[1 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
		retroJsState[2 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
		retroJsState[3 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
		retroJsState[4 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
		retroJsState[5 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
		retroJsState[6 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
		retroJsState[7 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
		retroJsState[8 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
		retroJsState[9 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
		retroJsState[10 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L);
		retroJsState[11 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R);
		retroJsState[12 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
		retroJsState[13 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);
		retroJsState[14 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3);
		retroJsState[15 + offset] = input_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3);

		if (options.mouse_device) {
			if (options.mouse_device == RETRO_DEVICE_MOUSE) {
				retroJsState[16 + offset] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
				retroJsState[17 + offset] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
				mouse_x[i] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
				mouse_y[i] = input_cb(i, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
			} else { /* RETRO_DEVICE_POINTER */
				pointer_pressed = input_cb(i, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
				retroJsState[16 + offset] = pointer_pressed;
				retroJsState[17 + offset] = 0; /* padding */
				mouse_x[i] = pointer_pressed ? get_pointer_delta(input_cb(i, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X), &prev_pointer_x) : 0;
				mouse_y[i] = pointer_pressed ? get_pointer_delta(input_cb(i, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y), &prev_pointer_y) : 0;
			}
		} else {
			retroJsState[16 + offset] = 0;
			retroJsState[17 + offset] = 0;
		}


		retroJsState[18 + offset] = 0;
		retroJsState[19 + offset] = 0;
		retroJsState[20 + offset] = 0;
		retroJsState[21 + offset] = 0;
		retroJsState[22 + offset] = 0;
		retroJsState[23 + offset] = 0;
		retroJsState[24 + offset] = 0;
		retroJsState[25 + offset] = 0;

		if (convert_analog_scale(analogjoy[i][0]) < -pressure_check)
			retroJsState[18 + offset] = convert_analog_scale(analogjoy[i][0]);

		if (convert_analog_scale(analogjoy[i][0]) > pressure_check)
			retroJsState[19 + offset] = convert_analog_scale(analogjoy[i][0]);

		if (convert_analog_scale(analogjoy[i][1]) < -pressure_check)
			retroJsState[20 + offset] = convert_analog_scale(analogjoy[i][1]);

		if (convert_analog_scale(analogjoy[i][1]) > pressure_check)
			retroJsState[21 + offset] = convert_analog_scale(analogjoy[i][1]);

		if (convert_analog_scale(analogjoy[i][2]) < -pressure_check)
			retroJsState[22 + offset] = convert_analog_scale(analogjoy[i][2]);

		if (convert_analog_scale(analogjoy[i][2]) > pressure_check)
			retroJsState[23 + offset] = convert_analog_scale(analogjoy[i][2]);

		if (convert_analog_scale(analogjoy[i][3]) < -pressure_check)
			retroJsState[24 + offset] = convert_analog_scale(analogjoy[i][3]);
		if (convert_analog_scale(analogjoy[i][3]) > pressure_check)
			retroJsState[25 + offset] = convert_analog_scale(analogjoy[i][3]);
	}
	mame_frame();
}

void retro_unload_game(void)
{
	mame_done();
	/* do we need to be freeing things here? */

	free(options.romset_filename_noext);
}

void retro_deinit(void)
{
#ifdef LOG_PERFORMANCE
	perf_cb.perf_log();
#endif
}

extern size_t state_get_dump_size(void);

size_t retro_serialize_size(void)
{
	return state_get_dump_size();
}

bool retro_serialize(void *data, size_t size)
{
	int cpunum;

	if (retro_serialize_size() == size && size) {
		/* write the save state */
		state_save_save_begin(data);

		/* write tag 0 */
		state_save_set_current_tag(0);
		if (state_save_save_continue())
			return false;

		/* loop over CPUs */
		for (cpunum = 0; cpunum < cpu_gettotalcpu(); cpunum++) {
			cpuintrf_push_context(cpunum);

			/* make sure banking is set */
			activecpu_reset_banking();

			/* save the CPU data */
			state_save_set_current_tag(cpunum + 1);
			if (state_save_save_continue())
				return false;

			cpuintrf_pop_context();
		}

		/* finish and close */
		state_save_save_finish();

		return true;
	}

	return false;
}

bool retro_unserialize(const void *data, size_t size)
{
	int cpunum;

	/* if successful, load it */
	if ((retro_serialize_size()) && (data) && (size) && (!state_save_load_begin((void *)data, size))) {
		/* read tag 0 */
		state_save_set_current_tag(0);
		if (state_save_load_continue())
			return false;

		/* loop over CPUs */
		for (cpunum = 0; cpunum < cpu_gettotalcpu(); cpunum++) {
			cpuintrf_push_context(cpunum);

			/* make sure banking is set */
			activecpu_reset_banking();

			/* load the CPU data */
			state_save_set_current_tag(cpunum + 1);
			if (state_save_load_continue())
				return false;

			cpuintrf_pop_context();
		}

		/* finish and close */
		state_save_load_finish();


		return true;
	}

	return false;
}





/******************************************************************************
*
*  Miscellaneous
*
******************************************************************************/
unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}
void *retro_get_memory_data(unsigned type)
{
	return 0;
}
size_t retro_get_memory_size(unsigned type)
{
	return 0;
}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
	return false;
}
void retro_cheat_reset(void)
{
}
void retro_cheat_set(unsigned unused, bool unused1, const char *unused2)
{
}
void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}
void retro_set_audio_sample(retro_audio_sample_t cb)
{
}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}
void retro_set_input_poll(retro_input_poll_t cb)
{
	poll_cb = cb;
}
void retro_set_input_state(retro_input_state_t cb)
{
	input_cb = cb;
}


/******************************************************************************
*
*       RetroPad mapping
*
******************************************************************************/

/* libretro presents "Player 1", "Player 2", "Player 3", etc while internally using indexed data starting at 0, 1, 2 */
/* The core presents "Player 1", "Player 2," "Player 3", and indexes them via enum values like JOYCODE_1_BUTTON1,        */
/* JOYCODE_2_BUTTON1, JOYCODE_3_BUTTON1 or with #define-d masks IPF_PLAYER1, IPF_PLAYER2, IPF_PLAYER3.               */
/*                                                                                                                   */
/* We are by convention passing "display" value used for mapping to core enums and player # masks to these macros.   */
/* (Display Index - 1) can be used for indexed data structures.                                                      */

