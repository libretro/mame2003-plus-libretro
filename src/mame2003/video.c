#include "libretro.h"
#include "libretro_perf.h"
#include "mame2003.h"
#include "palette.h"
#include "fileio.h"
#include "common.h"
#include "mame.h"
#include "usrintrf.h"
#include "driver.h"

extern retro_log_printf_t log_cb;
extern retro_environment_t environ_cb;
extern retro_video_refresh_t video_cb;
extern retro_set_led_state_t led_state_cb;
static unsigned long prev_led_state = 0;

#define MAX_LED 16

uint16_t videoBuffer[1024*1024];
struct osd_create_params videoConfig;
int gotFrame;

// TODO: This seems to work so far, but could be better

extern unsigned retroColorMode;

int osd_create_display(const struct osd_create_params *params, UINT32 *rgb_components)
{
   memcpy(&videoConfig, params, sizeof(videoConfig));    

   if(Machine->color_depth == 16)
   {
      retroColorMode = RETRO_PIXEL_FORMAT_RGB565;
      environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "game bpp: [%d], system bpp: [16], color format [RGB565] : SUPPORTED, enabling it.\n", Machine->color_depth);
   }
   else
   {
      // Assume 32bit color by default
      retroColorMode = RETRO_PIXEL_FORMAT_XRGB8888;
      environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "game bpp: [%d], system bpp: [32], color format [XRGB8888] : SUPPORTED, enabling it.\n", Machine->color_depth);
   }

   if(Machine->color_depth == 15)
   {
      /* 32bpp only */
      rgb_components[0] = 0x7C00;
      rgb_components[1] = 0x03E0;
      rgb_components[2] = 0x001F;

      /* 16bpp - just in case we ever do anything with this -
       * r - (0x1F << 11) 
       * g - (0x3F << 5)
       * b - Ox1F
       */
   }
   else if(Machine->color_depth == 32)
   {
      rgb_components[0] = 0xFF0000;
      rgb_components[1] = 0x00FF00;
      rgb_components[2] = 0x0000FF;
   }

   return 0;
}

void osd_close_display(void)
{
}

static const int frameskip_table[12][12] = { { 0,0,0,0,0,0,0,0,0,0,0,0 },
	                                                                    { 0,0,0,0,0,0,0,0,0,0,0,1 },
	                                                                    { 0,0,0,0,0,1,0,0,0,0,0,1 },
	                                                                    { 0,0,0,1,0,0,0,1,0,0,0,1 },
	                                                                    { 0,0,1,0,0,1,0,0,1,0,0,1 },
	                                                                    { 0,1,0,0,1,0,1,0,0,1,0,1 },
	                                                                    { 0,1,0,1,0,1,0,1,0,1,0,1 },
	                                                                    { 0,1,0,1,1,0,1,0,1,1,0,1 },
	                                                                    { 0,1,1,0,1,1,0,1,1,0,1,1 },
	                                                                    { 0,1,1,1,0,1,1,1,0,1,1,1 },
	                                                                    { 0,1,1,1,1,1,0,1,1,1,1,1 },
	                                                                    { 0,1,1,1,1,1,1,1,1,1,1,1 } };
static unsigned frameskip_counter = 0;

int osd_skip_this_frame(void)
{
   int ret;
   if (frameskip_counter >= 11)
      frameskip_counter = 0;

   ret = frameskip_table[options.frameskip][frameskip_counter];

   frameskip_counter++;

   return ret;
}

void osd_update_video_and_audio(struct mame_display *display)
{
   uint32_t width, height;
   RETRO_PERFORMANCE_INIT(perf_cb, update_video_and_audio);
   RETRO_PERFORMANCE_START(perf_cb, update_video_and_audio);

   width = videoConfig.width;
   height = videoConfig.height;

   if(display->changed_flags & 0xF)
   {
      int i, j;

      // Update UI area
      if (display->changed_flags & GAME_VISIBLE_AREA_CHANGED)
      {
         set_ui_visarea(display->game_visible_area.min_x, display->game_visible_area.min_y, display->game_visible_area.max_x, display->game_visible_area.max_y);
      }

      if (video_cb && display->changed_flags & GAME_BITMAP_CHANGED && (osd_skip_this_frame() == 0))
      {
         // Cache some values used in the below macros
         const uint32_t x = display->game_visible_area.min_x;
         const uint32_t y = display->game_visible_area.min_y;
         const uint32_t pitch = display->game_bitmap->rowpixels;

         // Copy pixels
         if(display->game_bitmap->depth == 16)
         {
            uint16_t* output = (uint16_t*)videoBuffer;
            const uint16_t* input = &((uint16_t*)display->game_bitmap->base)[y * pitch + x];

            for(i = 0; i < height; i ++)
            {
               for (j = 0; j < width; j ++)
               {
                  const uint32_t color = display->game_palette[*input++];
                  *output++ = (((color >> 19) & 0x1f) << 11) | (((color >> 11) & 0x1f) << 6) |
                     ((color >> 3) & 0x1f);
               }
               input += pitch - width;
            }

            video_cb(videoBuffer, width, height, width * 2);
         }
         else if(display->game_bitmap->depth == 32)
         {
            const uint32_t* const input = &((const uint32_t*)display->game_bitmap->base)[y * pitch + x];
            video_cb(input, width, height, pitch * 4);
         }
         else if(display->game_bitmap->depth == 15)
         {
            uint32_t* output = (uint32_t*)videoBuffer;
            const uint16_t* input = &((const uint16_t*)display->game_bitmap->base)[y * pitch + x];

            for(i = 0; i < height; i ++)
            {
               for(j = 0; j < width; j ++)
               {
                  const uint32_t color = *input ++;
                  *output++ = (((color >> 10) & 0x1f) << 19) | (((color >> 5) & 0x1f) << 11) |
                     (((color) & 0x1f) << 3);
               }
               input += pitch - width;
            }

            video_cb(videoBuffer, width, height, width * 4);
         }
      }
      else
         video_cb(NULL, width, height, width * 2);
   }

   if (display->changed_flags & LED_STATE_CHANGED)
   {
       if(led_state_cb != NULL)
       {
           // Set each changed LED
           unsigned long o = prev_led_state;
           unsigned long n = display->led_state;
           int led;
           for(led=0;led<MAX_LED;led++)
           {
               if((o & 0x01) != (n & 0x01))
               {
                 led_state_cb(led,n&0x01);
               }
               o >>= 1;
               n >>= 1;
           }
           prev_led_state = display->led_state;
       }
   }
   
   gotFrame = 1;

   RETRO_PERFORMANCE_STOP(perf_cb, update_video_and_audio);
}

struct mame_bitmap *osd_override_snapshot(struct mame_bitmap *bitmap, struct rectangle *bounds)
{
   return NULL;
}
