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

extern uint16_t videoBuffer[1024*1024];
extern unsigned videoBufferWidth;
extern unsigned videoBufferHeight;
struct osd_create_params videoConfig;
int gotFrame;

// TODO: This seems to work so far, but could be better

static unsigned totalColors;
static uint32_t* convertedPalette;

int osd_create_display(const struct osd_create_params *params, UINT32 *rgb_components)
{
   extern unsigned retroColorMode;
   static const UINT32 rValues[3] = {0x7C00, 0xFF0000, (0x1F << 11)};
   static const UINT32 gValues[3] = {0x03E0, 0x00FF00, (0x3F << 5)};
   static const UINT32 bValues[3] = {0x001F, 0x0000FF, (0x1F)};

   memcpy(&videoConfig, params, sizeof(videoConfig));    

   if(Machine->color_depth == 16)
   {
      retroColorMode = RETRO_PIXEL_FORMAT_RGB565;
      if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode))
      {
         fprintf(stderr, "game bpp: [16], system bpp: [16], color format [RGB565] : NOT AVAILABLE, defaulting to color format [0RGB1555] (SLOW PATH).\n");
         retroColorMode = RETRO_PIXEL_FORMAT_0RGB1555;
      }
      else
         fprintf(stderr, "game bpp: [16], system bpp: [16], color format [RGB565] : SUPPORTED, enabling it.\n");
   }
   else
   {
      fprintf(stderr, "game bpp: [%d], defaulting to bpp: [32], color format [XRGB8888].\n", Machine->color_depth);
      // Assume 32bit color by default
      retroColorMode = RETRO_PIXEL_FORMAT_XRGB8888;
      if(environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode))
      {
         fprintf(stderr, "game bpp: [%d], system bpp: [32], color format [XRGB8888] : SUPPORTED, enabling it.\n", Machine->color_depth);
      }
      else
      {
         fprintf(stderr, "game bpp: [%d], system bpp: [32], color format [XRGB8888] : NOT AVAILLE, defaulting to color format [0RGB1555] (SLOW PATH).\n", Machine->color_depth);
         retroColorMode = RETRO_PIXEL_FORMAT_0RGB1555;
      }
   }

   if(Machine->color_depth == 15)
   {
      const int val = (RETRO_PIXEL_FORMAT_RGB565 == retroColorMode) ? 2 : 0;
      rgb_components[0] = rValues[val];
      rgb_components[1] = gValues[val];
      rgb_components[2] = bValues[val];
   }
   else if(Machine->color_depth == 32)
   {
      rgb_components[0] = rValues[1];
      rgb_components[1] = gValues[1];
      rgb_components[2] = bValues[1];
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

static uint32_t rgb32toNeeded(uint32_t aColor)
{
    extern unsigned retroColorMode;
    
    if(RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode)
        return aColor;
    else
    {
        const uint32_t r = (aColor >> 19) & 0x1F;
        const uint32_t g = (aColor >> 11) & 0x1F;
        const uint32_t b = (aColor >>  3) & 0x1F;
        const int rgExtra = (RETRO_PIXEL_FORMAT_RGB565 == retroColorMode) ? 1 : 0;
        
        return (r << (10 + rgExtra)) | (g << (5 + rgExtra)) | b;
    }
}

void osd_update_video_and_audio(struct mame_display *display)
{
   int i, j;
   RARCH_PERFORMANCE_INIT(update_video_and_audio);
   RARCH_PERFORMANCE_START(update_video_and_audio);

   if(display->changed_flags & 0xF)
   {    
      // Update UI area
      if(display->changed_flags & GAME_VISIBLE_AREA_CHANGED)
      {
         set_ui_visarea(display->game_visible_area.min_x, display->game_visible_area.min_y, display->game_visible_area.max_x, display->game_visible_area.max_y);
      }

      // Update palette
      if(display->changed_flags & GAME_PALETTE_CHANGED)
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
               if(dirtyField & 1)
                  convertedPalette[i + j] = rgb32toNeeded(display->game_palette[i + j]);
            }
         }
      }

      extern unsigned retroColorMode;

      // Cache some values used in the below macros
      const uint32_t x = display->game_visible_area.min_x;
      const uint32_t y = display->game_visible_area.min_y;
      const uint32_t width = videoConfig.width;
      const uint32_t height = videoConfig.height;
      const uint32_t pitch = display->game_bitmap->rowpixels;

      // Copy image size
      videoBufferWidth = width;
      videoBufferHeight = height;

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
      }
      else if(display->game_bitmap->depth == 32)
      {
         uint32_t* output = (uint32_t*)videoBuffer;
         const uint32_t* input = &((const uint32_t*)display->game_bitmap->base)[y * pitch + x];

         for(i = 0; i < height; i ++)
         {
            memcpy(&output[i * videoConfig.width], input, width * sizeof(uint32_t));
            input += pitch;
         }
      }
      else if(display->game_bitmap->depth == 15)
      {
         if(retroColorMode == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            DIRECT_COPY(uint32_t, uint16_t, 1, 10, 0x1F, 19, 5, 0x1F, 11, 0, 0x1F, 3);
         }
         else 
         {
            uint16_t* output = (uint16_t*)videoBuffer;
            const uint16_t* input = &((const uint16_t*)display->game_bitmap->base)[y * pitch + x];

            for(i = 0; i < height; i ++)
            {
               memcpy(&output[i * videoConfig.width], input, width * sizeof(uint16_t));
               input += pitch;
            }
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
