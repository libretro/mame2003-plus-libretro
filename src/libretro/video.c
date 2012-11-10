#include "libretro.h"
#include "osdepend.h"
#include "palette.h"
#include "fileio.h"
#include "common.h"
#include "mame.h"
#include "usrintrf.h"
#include "driver.h"

#ifdef DEBUG_LOG
# define COLORLOG(...) fprintf(stderr, __VA_ARGS__)
#else
# define COLORLOG(...)
#endif


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
    static const UINT32 rValues[3] = {0x7C00, 0xFF0000, (0x1F << 11)};
    static const UINT32 gValues[3] = {0x03E0, 0x00FF00, (0x3F << 6)};
    static const UINT32 bValues[3] = {0x001F, 0x0000FF, (0x1F)};
    
    memcpy(&videoConfig, params, sizeof(videoConfig));    
    
    /* Setup Color Mode; Unless I missed something osd_create_display is only called once:
       run_game()->run_machine()->vh_open()->artwork_create_display()->osd_create_display() */
    extern retro_environment_t environ_cb;
    extern unsigned retroColorMode;       
       
    static const unsigned bpp16modes[3] = {RETRO_PIXEL_FORMAT_RGB565, RETRO_PIXEL_FORMAT_0RGB1555, RETRO_PIXEL_FORMAT_XRGB8888};
    static const unsigned bpp32modes[3] = {RETRO_PIXEL_FORMAT_XRGB8888, RETRO_PIXEL_FORMAT_RGB565, RETRO_PIXEL_FORMAT_0RGB1555};
    const unsigned *const useModes = (Machine->color_depth == 32) ? bpp32modes : bpp16modes;

    for(int i = 0; i != 3; i ++)
    {
        retroColorMode = useModes[i];
        if(environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &retroColorMode))
        {
            COLORLOG("Colors: Using Mode %d\n", retroColorMode);
            break;
        }
    }
    
    /* Tell MAME about color mapping */
    if(Machine->color_depth == 15)
    {
        const unsigned fixedColorMode = (retroColorMode == RETRO_PIXEL_FORMAT_XRGB8888) ? RETRO_PIXEL_FORMAT_0RGB1555 : retroColorMode;
        rgb_components[0] = rValues[fixedColorMode];
        rgb_components[1] = gValues[fixedColorMode];
        rgb_components[2] = bValues[fixedColorMode];
        
        COLORLOG("Colors: Machine want's direct 16 bits per pixel.\n");
    }
    else if(Machine->color_depth == 32)
    {
        rgb_components[0] = rValues[1];
        rgb_components[1] = gValues[1];
        rgb_components[2] = bValues[1];
        
        COLORLOG("Colors: Machine want's direct 32 bits per pixel.\n");
    }
    else
    {
        COLORLOG("Colors: Machine uses palette.\n");
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

// But mummy I wan't to use templates
#define PALETTE_COPY(OTYPE) \
{ \
    OTYPE* output = (OTYPE*)videoBuffer; \
    const uint16_t* input = &((uint16_t*)display->game_bitmap->base)[y * pitch + x]; \
 \
    for(int i = 0; i != height; i ++) \
    { \
        for(int j = 0; j != width; j ++) \
        { \
            *output++ = convertedPalette[*input++]; \
        } \
        input += pitch - width; \
    } \
}

#define DIRECT_COPY(OTYPE, ITYPE, ALLOWSIMPLE, RDOWN, RMASK, RUP, GDOWN, GMASK, GUP, BDOWN, BMASK, BUP) \
{ \
    OTYPE* output = (OTYPE*)videoBuffer; \
    const ITYPE* input = &((const ITYPE*)display->game_bitmap->base)[y * pitch + x]; \
 \
    for(int i = 0; i != height; i ++) \
    { \
        if(ALLOWSIMPLE && sizeof(OTYPE) == sizeof(ITYPE)) \
        { \
            memcpy(&output[i * videoConfig.width], input, width * sizeof(OTYPE)); \
            input += pitch; \
        } \
        else \
        { \
            for(int j = 0; j != width; j ++) \
            { \
                const uint32_t color = *input ++; \
                const uint32_t r = ((color >> (RDOWN)) & RMASK) << RUP; \
                const uint32_t g = ((color >> (GDOWN)) & GMASK) << GUP; \
                const uint32_t b = ((color >> (BDOWN)) & BMASK) << BUP; \
                *output++ = r | g | b; \
            } \
            input += pitch - width; \
        } \
    } \
}

#define SIMPLE_DIRECT_COPY(OTYPE) DIRECT_COPY(OTYPE, OTYPE, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0)

static uint32_t rgb32toNeeded(uint32_t aColor)
{
    extern unsigned retroColorMode;
    
    if(RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode)
    {
        return aColor;
    }
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
        
            for(int i = 0; i < display->game_palette_entries; i += 32)
            {
                UINT32 dirtyField = display->game_palette_dirty[i / 32];
                
                for(int j = 0; dirtyField; j ++, dirtyField >>= 1)
                {
                    if(dirtyField & 1)
                    {
                        convertedPalette[i + j] = rgb32toNeeded(display->game_palette[i + j]);
                    }
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
            if(RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode)
            {
                PALETTE_COPY(uint32_t);
            }
            else
            {
                PALETTE_COPY(uint16_t);
            }
        }
        else if(display->game_bitmap->depth == 32)
        {
            if(RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode)
            {
                SIMPLE_DIRECT_COPY(uint32_t);
            }
            else if(RETRO_PIXEL_FORMAT_0RGB1555 == retroColorMode)
            {
                DIRECT_COPY(uint16_t, uint32_t, 1, 19, 0x1F, 10, 11, 0x1F, 5, 3, 0x1F, 0);
            }
            else
            {
                DIRECT_COPY(uint16_t, uint32_t, 1, 19, 0x1F, 11, 10, 0x3F, 5, 3, 0x1F, 0);        
            }
        }
        else if(display->game_bitmap->depth == 15)
        {
            if(RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode)
            {
                DIRECT_COPY(uint32_t, uint16_t, 1, 10, 0x1F, 19, 5, 0x1F, 11, 0, 0x1F, 3);
            }
            else 
            {
                SIMPLE_DIRECT_COPY(uint16_t);
            }
        }
    }
    
    gotFrame = 1;
}

struct mame_bitmap *osd_override_snapshot(struct mame_bitmap *bitmap, struct rectangle *bounds){return NULL;}

