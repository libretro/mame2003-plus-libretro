#include "libretro.h"
#include "osdepend.h"
#include "libco/libco.h"
#include "palette.h"
#include "fileio.h"
#include "common.h"
#include "mame.h"
#include "usrintrf.h"

extern cothread_t mainThread;
extern cothread_t emuThread;
extern uint16_t videoBuffer[1024*1024];
struct osd_create_params videoConfig;

// TODO: This seems to work so far, but could be better

int osd_create_display(const struct osd_create_params *params, UINT32 *rgb_components)
{
    extern unsigned retroColorMode;
    static const UINT32 rValues[3] = {0x7C00, 0xFF0000, (0x1F << 11)};
    static const UINT32 gValues[3] = {0x03E0, 0x00FF00, (0x3F << 6)};
    static const UINT32 bValues[3] = {0x001F, 0x0000FF, (0x1F)};

    if(retroColorMode >= 3)
    {
        retroColorMode %= 3; //< It's an error anyway
    }
    
    memcpy(&videoConfig, params, sizeof(videoConfig));    
    
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

}

int osd_skip_this_frame(void)
{
    return 0;
}

// But mummy I wan't to use templates
#define PALETTE_COPY(OTYPE, RLOSS, RMASK, RPOS, GLOSS, GMASK, GPOS, BLOSS, BMASK, BPOS) \
{ \
    OTYPE* output = (OTYPE*)videoBuffer; \
    const uint32_t x = display->game_visible_area.min_x; \
    const uint32_t y = display->game_visible_area.min_y; \
    const uint32_t width = display->game_bitmap->width; \
    const uint32_t height = display->game_bitmap->height; \
    const uint16_t* const input = display->game_bitmap->base; \
    const uint32_t pitch = display->game_bitmap->rowpixels; \
 \
    for(int i = 0; i != height; i ++) \
    { \
        const uint16_t* inputLine = &input[(i + y) * pitch + x]; \
 \
        for(int j = 0; j != width; j ++) \
        { \
            const uint32_t color = display->game_palette[*inputLine ++]; \
            const uint32_t r = ((color >> (16 + RLOSS)) & RMASK) << RPOS; \
            const uint32_t g = ((color >>  (8 + GLOSS)) & GMASK) << GPOS; \
            const uint32_t b = ((color >>  (0 + BLOSS)) & BMASK) << BPOS; \
            output[i * videoConfig.width + j] = r | g | b; \
        } \
    } \
}

#define DIRECT_COPY(OTYPE, ITYPE, ALLOWSIMPLE, RDOWN, RMASK, RUP, GDOWN, GMASK, GUP, BDOWN, BMASK, BUP) \
{ \
    OTYPE* output = (OTYPE*)videoBuffer; \
    const uint32_t x = display->game_visible_area.min_x; \
    const uint32_t y = display->game_visible_area.min_y; \
    const uint32_t width = display->game_bitmap->width; \
    const uint32_t height = display->game_bitmap->height; \
    const ITYPE* const input = (const ITYPE*)display->game_bitmap->base; \
    const uint32_t pitch = display->game_bitmap->rowpixels; \
 \
    for(int i = 0; i != height; i ++) \
    { \
        const ITYPE* inputLine = &input[(i + y) * pitch + x]; \
 \
        if(ALLOWSIMPLE && sizeof(OTYPE) == sizeof(ITYPE)) \
        { \
            memcpy(&output[i * videoConfig.width], inputLine, width * sizeof(OTYPE)); \
        } \
        else \
        { \
            for(int j = 0; j != width; j ++) \
            { \
                const uint32_t color = *inputLine ++; \
                const uint32_t r = ((color >> (RDOWN)) & RMASK) << RUP; \
                const uint32_t g = ((color >> (GDOWN)) & GMASK) << GUP; \
                const uint32_t b = ((color >> (BDOWN)) & BMASK) << BUP; \
                output[i * videoConfig.width + j] = r | g | b; \
            } \
        } \
    } \
}

#define SIMPLE_DIRECT_COPY(OTYPE) DIRECT_COPY(OTYPE, OTYPE, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0)

void osd_update_video_and_audio(struct mame_display *display)
{
    // Update UI area
	if (display->changed_flags & GAME_VISIBLE_AREA_CHANGED)
	{
    	set_ui_visarea(display->game_visible_area.min_x, display->game_visible_area.min_y, display->game_visible_area.max_x, display->game_visible_area.max_y);
    }

    extern unsigned retroColorMode;

    if(display->game_bitmap->depth == 16)
    {
        if(RETRO_PIXEL_FORMAT_0RGB1555 == retroColorMode)
        {
            PALETTE_COPY(uint16_t, 3, 0x1F, 10, 3, 0x1F, 5, 3, 0x1F, 0);
        }
        else if(RETRO_PIXEL_FORMAT_XRGB8888 == retroColorMode)
        {
            PALETTE_COPY(uint32_t, 0, 0xFF, 16, 0, 0xFF, 8, 0, 0xFF, 0);        
        }
        else
        {
            PALETTE_COPY(uint16_t, 3, 0x1F, 11, 2, 0x3F, 5, 3, 0x1F, 0);        
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
    
    co_switch(mainThread);
}

struct mame_bitmap *osd_override_snapshot(struct mame_bitmap *bitmap, struct rectangle *bounds){return NULL;}
const char *osd_get_fps_text(const struct performance_info *performance){return "";}
