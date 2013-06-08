#include "libretro.h"
#include "performance.h"
#include "osdepend.h"
#include "palette.h"
#include "fileio.h"
#include "common.h"
#include "mame.h"
#include "usrintrf.h"
#include "driver.h"

extern retro_environment_t environ_cb;
extern retro_video_refresh_t video_cb;

uint16_t videoBuffer[1024*1024];
struct osd_create_params videoConfig;
int gotFrame;

// TODO: This seems to work so far, but could be better

static unsigned totalColors;
uint32_t* convertedPalette;
extern unsigned retroColorMode;

int osd_create_display(const struct osd_create_params *params, UINT32 *rgb_components)
{
   memcpy(&videoConfig, params, sizeof(videoConfig));    

   if(Machine->color_depth == 16)
   {
      retroColorMode = RETRO_PIXEL_FORMAT_RGB565;
      environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode);
      fprintf(stderr, "game bpp: [%d], system bpp: [16], color format [RGB565] : SUPPORTED, enabling it.\n", Machine->color_depth);
   }
   else
   {
      // Assume 32bit color by default
      retroColorMode = RETRO_PIXEL_FORMAT_XRGB8888;
      environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode);
      fprintf(stderr, "game bpp: [%d], system bpp: [32], color format [XRGB8888] : SUPPORTED, enabling it.\n", Machine->color_depth);
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
    free(convertedPalette);
    convertedPalette = 0;
    totalColors = 0;
}

int osd_skip_this_frame(void)
{
    return 0;
}

#define DIRECT_COPY(OTYPE, ITYPE, ALLOWSIMPLE, RDOWN, RMASK, RUP, GDOWN, GMASK, GUP, BDOWN, BMASK, BUP) \
{ \
   OTYPE* output = (OTYPE*)videoBuffer; \
   const ITYPE* input = &((const ITYPE*)display->game_bitmap->base)[y * pitch + x]; \
   \
   for(i = 0; i < height; i ++) \
   { \
      for(j = 0; j < width; j ++) \
      { \
         const uint32_t color = *input ++; \
         const uint32_t r = ((color >> (RDOWN)) & RMASK) << RUP; \
         const uint32_t g = ((color >> (GDOWN)) & GMASK) << GUP; \
         const uint32_t b = ((color >> (BDOWN)) & BMASK) << BUP; \
         *output++ = r | g | b; \
      } \
      input += pitch - width; \
   } \
}

void osd_update_video_and_audio(struct mame_display *display)
{
   RARCH_PERFORMANCE_INIT(update_video_and_audio);
   RARCH_PERFORMANCE_START(update_video_and_audio);

   if(display->changed_flags & 0xF)
   {
      int i, j;

      // Update UI area
      if(display->changed_flags & GAME_VISIBLE_AREA_CHANGED)
      {
         set_ui_visarea(display->game_visible_area.min_x, display->game_visible_area.min_y, display->game_visible_area.max_x, display->game_visible_area.max_y);
      }

      // Update palette
      if(retroColorMode != RETRO_PIXEL_FORMAT_XRGB8888 && display->changed_flags & GAME_PALETTE_CHANGED)
      {
         if(display->game_palette_entries > totalColors)
         {
            totalColors = display->game_palette_entries;
            free(convertedPalette);
            convertedPalette = malloc(4 * display->game_palette_entries + (32 * 4));
         }

         for(i = 0; i < display->game_palette_entries; i += 32)
         {
            UINT32 dirtyField = display->game_palette_dirty[i / 32];

            for(j = 0; dirtyField; j ++, dirtyField >>= 1)
            {
               if (dirtyField & 1)
               {
                  uint32_t aColor = display->game_palette[i + j];
                  const int rgExtra = (retroColorMode == RETRO_PIXEL_FORMAT_RGB565) ? 1 : 0;

                  convertedPalette[i + j] = (((aColor >> 19) & 0x1F) << (10 + rgExtra)) | (((aColor >> 11) & 0x1F) << (5 + rgExtra)) | ((aColor >> 3) & 0x1F);
               }
            }
         }
      }

      extern unsigned retroColorMode;

      if (video_cb && display->changed_flags & GAME_BITMAP_CHANGED)
      {
         // Cache some values used in the below macros
         const uint32_t x = display->game_visible_area.min_x;
         const uint32_t y = display->game_visible_area.min_y;
         const uint32_t width = videoConfig.width;
         const uint32_t height = videoConfig.height;
         const uint32_t pitch = display->game_bitmap->rowpixels;

         // Copy pixels
         if(display->game_bitmap->depth == 16)
         {
            uint16_t* output = (uint16_t*)videoBuffer;
            const uint16_t* input = &((uint16_t*)display->game_bitmap->base)[y * pitch + x];

            for(i = 0; i < height; i ++)
            {
               for (j = 0; j < width; j ++)
                  *output++ = convertedPalette[*input++];
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
            DIRECT_COPY(uint32_t, uint16_t, 1, 10, 0x1F, 19, 5, 0x1F, 11, 0, 0x1F, 3);
            video_cb(videoBuffer, width, height, width * 4);
         }
      }
   }

   gotFrame = 1;

   RARCH_PERFORMANCE_STOP(update_video_and_audio);
}

struct mame_bitmap *osd_override_snapshot(struct mame_bitmap *bitmap, struct rectangle *bounds)
{
   return NULL;
}
