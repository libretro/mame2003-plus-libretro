/* Copyright  (C) 2023 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (filtered_poll.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <features/features_cpu.h>
#include <filtered_poll.h>

static struct fp_codes_for_filter *fp_codes_filters   = NULL;
static int fp_actions_array_size                      = 0;
static struct fp_retro_code *fp_code                  = NULL;
static struct fp_filter_state *fp_filter_states       = NULL;
static retro_input_state_t default_state_cb           = NULL;
static int run_filter                                 = 0;
static int initialized                                = 0;

int16_t core_input_state_filtered_poll_return_cb_override(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
  int16_t pressed;

  printf("Before poll data in filtered_poll_return_cb.\n");
  pressed = default_state_cb(port, device, idx, id);
  printf("After poll data in filtered_poll_return_cb.\n");

  printf("run_filter = %i\n", run_filter);
  printf("device = %u\n", device);
  printf("id = %u\n", id);
  if (    run_filter
      &&  (device == RETRO_DEVICE_KEYBOARD || device == RETRO_DEVICE_JOYPAD))
  {
    if (fp_codes_filters != NULL)
    {
      if ((!initialized) && (fp_filter_states != NULL))
      {
        printf("After initialized.\n");
        for (int i = 0; i < fp_actions_array_size; i++)
        {
          fp_filter_states[i].player = fp_codes_filters[i].player;
          printf("fp_filter_states[%i].player = %i\n", i, fp_filter_states[i].player);
          fp_filter_states[i].type = fp_codes_filters[i].type;
          printf("fp_filter_states[%i].type = %i\n", i, fp_filter_states[i].type);
          for (int j = 0; j < MAX_OPERATIONS; j++)
          {
            fp_filter_states[i].state[j] = 0;
            printf("fp_filter_states[%i].state[%i] = %li\n", i, j, fp_filter_states[i].state[j]);
          }
        }
        initialized = 1;

      }

      int action = 0;
      int run_loop = 1;

      if ((!fp_codes_filters[action].player) && (fp_codes_filters[action].next_active_position))
        action = fp_codes_filters[action].next_active_position;
      else run_loop = 0;

      if (run_loop)
      {
        do
        {
          if (fp_codes_filters[action].operations & FP_TIMELOCKOUT)
          {
            int time_lockout_updated = 0;
            for (int i = 0; i < fp_codes_filters[action].codes_array_length; i++)
            {
              if (fp_codes_filters[action].codes[i].id == NO_CODE)
                break;
              if (      port   == fp_codes_filters[action].codes[i].port
                    &&  device == fp_codes_filters[action].codes[i].device
                    &&  idx    == fp_codes_filters[action].codes[i].idx
                    &&  id     == fp_codes_filters[action].codes[i].id)
              {
                if (( fp_codes_filters[action].player != fp_filter_states[action].player) ||
                     (fp_codes_filters[action].type   != fp_filter_states[action].type))
                {
                  int a = 0;
                  int row_moved = 0;
                  while ((a < fp_actions_array_size) && !row_moved)
                  {
                    if (( fp_codes_filters[action].player == fp_filter_states[a].player) &&
                         (fp_codes_filters[action].type   == fp_filter_states[a].type))
                    {
                      fp_filter_states[action].player = fp_filter_states[a].player;
                      fp_filter_states[a].player = 0;
                      fp_filter_states[action].type   = fp_filter_states[a].type;
                      fp_filter_states[a].type = 0;
                      for (int s = 0; s < MAX_OPERATIONS; s++)
                      {
                        fp_filter_states[action].state[s] = fp_filter_states[a].state[s];
                        fp_filter_states[a].state[s] = 0;
                      }
                      row_moved = 1;
                    }
                    a++;
                  }
                }
                if (pressed)
                {
                  if (!fp_filter_states[action].state[0])
                    fp_filter_states[action].state[0] = cpu_features_get_time_usec();
                  else if (cpu_features_get_time_usec() - fp_filter_states[action].state[0]
                                < fp_codes_filters[action].modifier[0] * 1000)
                    return 0;
                  else
                    fp_filter_states[action].state[0] = 0;
                }
              }
            }
          }
          /* example of 'if statement' to define a second operation if needed in the future */
/*        if (fp_codes_filters[action].operations & FP_NAME_OF_OPERATION2)
          {
          } */

          if (fp_codes_filters[action].next_active_position)
            action = fp_codes_filters[action].next_active_position;
          else
            run_loop = 0;
        } while (run_loop);
      }
    }
  }

  return pressed;
}

static retro_input_state_t retro_core_input_state_filtered_poll_return_cb(void)
{
  printf("In retro_core_input_state_filtered_poll_return_cb\n");
  // int16_t pressed = 0;
  // pressed = core_input_state_filtered_poll_return_cb_override;
  return core_input_state_filtered_poll_return_cb_override;
}

retro_input_state_t retro_get_core_input_state_filtered_poll_return_cb(void)
{
  return retro_core_input_state_filtered_poll_return_cb();
}

void retro_set_filtered_poll_run_filter(int runFilter)
{
  run_filter = runFilter;
}

void retro_set_filtered_poll_original_cb(retro_input_state_t defaultStateCb)
{
  default_state_cb = defaultStateCb;
}

void retro_set_filtered_poll_variables(   int fpActionsArraySize,
                                          struct fp_codes_for_filter *codesFilter,
                                          struct fp_filter_state *filterState,
                                          void *fpCode,
                                          int fpCodesArrayLength)
{
  fp_actions_array_size = fpActionsArraySize;
  fp_codes_filters = codesFilter;
  fp_filter_states = filterState;
  fp_code = (struct fp_retro_code *) fpCode;
  for (int i=0; i < fp_actions_array_size; i++)
  {
    fp_codes_filters[i].codes = (struct fp_retro_code *) fp_code + (i * fpCodesArrayLength);
    fp_codes_filters[i].codes_array_length = fpCodesArrayLength;
  }

  printf("At end of retro_set_filtered_poll_variables.\n");
  for (int i=0; i < fp_actions_array_size; i++)
  {
    printf("fp_codes_filters[%i].player: %i\n", i, fp_codes_filters[i].player);
    printf("fp_codes_filters[%i].type: %i\n", i, fp_codes_filters[i].type);
    printf("fp_codes_filters[%i].operations: %X\n", i, fp_codes_filters[i].operations);
    printf("fp_codes_filters[%i].modifier[0]: %i\n", i, fp_codes_filters[i].modifier[0]);
    for (int j=0; j < fp_codes_filters[i].codes_array_length; j++)
    {
      printf("fp_codes_filters[%i].codes[%i].port: %u\n", i, j, fp_codes_filters[i].codes[j].port);
      printf("fp_codes_filters[%i].codes[%i].device: %u\n", i, j, fp_codes_filters[i].codes[j].device);
      printf("fp_codes_filters[%i].codes[%i].idx: %u\n", i, j, fp_codes_filters[i].codes[j].idx);
      printf("fp_codes_filters[%i].codes[%i].id: %u\n", i, j, fp_codes_filters[i].codes[j].id);
    }
    printf("fp_codes_filters[%i].codes_array_length: %i\n", i, fp_codes_filters[i].codes_array_length);
    printf("fp_codes_filters[%i].next_active_position: %i\n", i, fp_codes_filters[i].next_active_position);
  }
}
